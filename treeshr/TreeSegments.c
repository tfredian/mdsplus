/*
Copyright (c) 2017, Massachusetts Institute of Technology All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

Redistributions in binary form must reproduce the above copyright notice, this
list of conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include "treethreadsafe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <treeshr.h>
#include <usagedef.h>
#include <libroutines.h>
#include <mdsshr.h>
#include <mdsshr_messages.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <ctype.h>
#include <mds_stdarg.h>
#include <STATICdef.h>
extern void **TreeCtx();

/*** Segmented Record Support  ************************

Segmented records use a special extended attr record stored at the file offset (nci rfa field)
which describes the location of segment index and segment information metadata stored in the
datafile. There is an EXTENDED_NCI bit in the nci flags2 field which indicates that the node contains
extended nci information. The EXTENDED_NCI capability supports different types of "facilities" of which
3 are currently supported: STANDARD_RECORD, SEGMENTED_RECORD, NAMED_ATTRIBUTES. The NAMED_ATTRIBUTE
facility provides a means for adding extra node characteristics by defining name/value pairs. The use
of the STANDARD_RECORD facility is currently only used if the node also has named vars->attr.

When writing a segment in a node for the first time, an extended attribute record is stored for the node
which contains a file offset where the segment header information is stored. The segment header contains
information such as the datatype, the dimensions of rows in the segments, the length of each data element,
the current segment index, the next row to be stored in that segment and file offsets for a segment
index, and offsets for the data and dimension of the segment currently in progress (timestamped segments).

The segment index is a linked list segment indexes each containing information of up to 128 segments. The
index offset in the segment header points to the most recently stored index page. Each index page contains
information about the segments including the segment start and end, the segments dimension and the offset
of the data for the segment.

Since the structures used to index and define segments was designed prior to the addition of new features
such as timestamped segments and compress segments, the fields in the segment descriptions are used in various
ways to indicate different segment properties.

Below are the special settings that are used to indicate characteristics of a segment:

segment_info.rows:
   Normally indicates the number of rows in the segment. If rows is negative it indicates that the
   segment is stored as a serialized record located in the file a data_offset with length equal to the
   lower 31 bits of the rows field. This special indicator is used when segments are compressed or when
   segments are DTYPE_OPAQUE (i.e. image storage).

segment_info.dimension_length
   For normal segments the segment has its own dimension stored as a serialized dimension and the dimension
   offset is where the dimension data is stored and the dimension length is the length of that data.
   For timestamped segments, the dimension length is set to 0 indicating that the dimension is a list of
   64-bit timestamps, one for each row, located at the dimension offset in the file.

segment_info.xxxx_offset
   These fields are used to indicate file offsets where particular data can be found. They are set to -1 to
   indicate that there is no data for xxxx stored.

********************************************************/
#define INIT_TREESUCCESS INIT_STATUS_AS TreeSUCCESS
typedef struct common_vars {
PINO_DATABASE*		dblist;
NID*			nid_ptr;
TREE_INFO*		tinfo;
NODE*			node_ptr;
int64_t			saved_viewdate;
int			nidx;
int			stv;
NCI			local_nci;	// was pointer
int			nci_locked;
EXTENDED_ATTRIBUTES	attr;		// was pointer
int                     attr_update;    // was pointer
int64_t                 attr_offset;    // was pointer
SEGMENT_HEADER		shead;
SEGMENT_INDEX		sindex;         // was pointer
int64_t                 index_offset;
SEGMENT_INFO*           sinfo;
int                     compress;
// write
int                     idx;
int                     rows_filled;
uint32_t                add_length;
} vars_t;

#define INIT_VARS \
int status; \
vars_t _vars = {0}; \
vars_t* vars = &_vars; \
_vars.dblist = (PINO_DATABASE *) dbid; \
_vars.nid_ptr = (NID *) & nid;

#define INIT_WRITE_VARS INIT_VARS _vars.idx = idx; _vars.rows_filled = rows_filled;

static void unlock_nci(void* vars_in) {
  vars_t*vars = (vars_t*)vars_in;
  if (vars->nci_locked) {
    TreeUnLockNci(vars->tinfo, 0, vars->nidx);
    vars->nci_locked = 0;
  }
}
static void lock_nci(void* vars_in) {
  vars_t*vars = (vars_t*)vars_in;
  //  fprintf(stderr,"lock_nci-------------------------------------------------------------------------------------------------\n");
  if (!vars->nci_locked) {
    TreeLockNci(vars->tinfo, 0, vars->nidx, NULL);
    vars->nci_locked = 1;
  }
}

#ifdef WORDS_BIGENDIAN
 #define ALLOCATE_BUFFER(SIZE,BUFFER) char *BUFFER = malloc(SIZE)
 inline static void endianTransfer(const char* buffer_in, const size_t size, const int length, char* buffer_out) {
    char *bptr = (char*)buffer_out;
    int i;
    switch (length) {
    case 2:
      for (i = 0 ; i < (int)size / length ; i++)
	putint16(&bptr, &((int16_t*)buffer_in)[i]);
      break;
    case 4:
      for (i = 0 ; i < (int)size / length ; i++)
	putint32(&bptr,&((int32_t*)buffer_in)[i]);
      break;
    case 8:
      for (i = 0 ; i < (int)size / length ; i++)
	putint64(&bptr,&((int64_t*)buffer_in)[i]);
      break;
    }
 }
 #define CHECK_ENDIAN(BUFFER,SIZE,LENGTH,DTYPE) do{\
  if (LENGTH > 1 && DTYPE != DTYPE_T && DTYPE != DTYPE_IDENT && DTYPE != DTYPE_PATH) \
    endianTransfer((char*)BUFFER,SIZE,LENGTH,(char*)BUFFER); \
 }while(0)
 #define CHECK_ENDIAN_TRANSFER(BUFFER_IN,SIZE,LENGTH,DTYPE,BUFFER_OUT) do{ \
  if (LENGTH > 1 && DTYPE != DTYPE_T && DTYPE != DTYPE_IDENT && DTYPE != DTYPE_PATH) \
    endianTransfer((char*)BUFFER_IN,SIZE,LENGTH,BUFFER_OUT); \
  else memcpy(BUFFER_OUT,BUFFER_IN,SIZE); \
 }while(0)
 #define FREE_BUFFER(BUFFER) free(BUFFER)
#else
 #define ALLOCATE_BUFFER(SIZE,BUFFER) char *BUFFER
 #define CHECK_ENDIAN(BUFFER,SIZE,LENGTH,DTYPE) {/**/}
 #define CHECK_ENDIAN_TRANSFER(BUFFER_IN,SIZE,LENGTH,DTYPE,BUFFER_OUT) BUFFER_OUT = (char*)BUFFER_IN
 #define FREE_BUFFER(BUFFER) {/**/}
#endif



typedef ARRAY_COEFF(char, 8) A_COEFF_TYPE;

int _TreeMakeSegment(void *dbid, int nid, struct descriptor *start, struct descriptor *end, struct descriptor *dimension, struct descriptor_a *initialValue, int idx, int rows_filled);
/* write new segment with prefilled rows range of valid data can be defined by rows_filled
 */
int TreeMakeSegment(int nid, struct descriptor *start, struct descriptor *end, struct descriptor *dimension, struct descriptor_a *initialValue, int idx, int rows_filled){
  return _TreeMakeSegment(*TreeCtx(), nid, start, end, dimension, initialValue, idx, rows_filled);
}
int _TreeBeginSegment(void *dbid, int nid, struct descriptor *start, struct descriptor *end, struct descriptor *dimension, struct descriptor_a *initialValue, int idx){
/* write new EMPTY segment so data can be added row by row with _TreePutSegment
 * like _TreeMakeSegment but rows_filled will be initialized with 0
 */
  return _TreeMakeSegment(dbid, nid, start, end, dimension, initialValue, idx, 0);
}
int TreeBeginSegment(int nid, struct descriptor *start, struct descriptor *end, struct descriptor *dimension, struct descriptor_a *initialValue, int idx){
  return _TreeBeginSegment(*TreeCtx(), nid, start, end, dimension, initialValue, idx);
}

int _TreeSetRowsFilled(void *dbid, int nid, int rows_filled);
/* allows to correct the rows filled property of a segmented node
 * e.g. after it has been written to using beginSegment instead of makeSegment
 */
int TreeSetRowsFilled(int nid, int rows_filled){
  return _TreeSetRowsFilled(*TreeCtx(), nid, rows_filled);
}
int _TreeMakeTimestampedSegment(void *dbid, int nid, int64_t * timestamps, struct descriptor_a *initialValue, int idx, int rows_filled);
/* write new timestamped (int64_t) segment with prefilled rows range of valid data can be defined by rows_filled
 */
int TreeMakeTimestampedSegment(int nid, int64_t * timestamps, struct descriptor_a *initialValue, int idx, int rows_filled){
  return _TreeMakeTimestampedSegment(*TreeCtx(), nid, timestamps, initialValue, idx, rows_filled);
}
int _TreeBeginTimestampedSegment(void *dbid, int nid, struct descriptor_a *initialValue, int idx){
/* write new EMPTY timestamped (int64_t) segment so data can be added row by row
 * like _TreeMakeSegment but rows_filled will be initialized with 0
 */
  return _TreeMakeTimestampedSegment(dbid, nid, NULL, initialValue, idx, 0);
}
int TreeBeginTimestampedSegment(int nid, struct descriptor_a *initialValue, int idx){
  return _TreeBeginTimestampedSegment(*TreeCtx(), nid, initialValue, idx);
}

int _TreeUpdateSegment(void *dbid, int nid, struct descriptor *start, struct descriptor *end, struct descriptor *dimension, int idx);
/* allows to correct the dimension and limits of a segment without touchng the data part
 */
int TreeUpdateSegment(int nid, struct descriptor *start, struct descriptor *end, struct descriptor *dimension, int idx){
  return _TreeUpdateSegment(*TreeCtx(), nid, start, end, dimension, idx);
}

int _TreePutSegment(void *dbid, int nid, const int startIdx, struct descriptor_a *data);
int TreePutSegment(const int nid, const int startIdx, struct descriptor_a *data){
  return _TreePutSegment(*TreeCtx(), nid, startIdx, data);
}

int _TreeGetSegmentTimesXd(void *dbid, int nid, int *nsegs, struct descriptor_xd *start_list, struct descriptor_xd *end_list);
int TreeGetSegmentTimesXd(int nid, int *nsegs, struct descriptor_xd *start_list, struct descriptor_xd *end_list){
  return _TreeGetSegmentTimesXd(*TreeCtx(), nid, nsegs, start_list, end_list);
}

int _TreeGetSegmentTimes(void *dbid, int nid, int *nsegs, int64_t ** times);
int TreeGetSegmentTimes(int nid, int *nsegs, int64_t ** times){
  return _TreeGetSegmentTimes(*TreeCtx(), nid, nsegs, times);
}

int _TreeGetNumSegments(void *dbid, int nid, int *num);
/* returns the number of segments stored in the node
 */
int TreeGetNumSegments(int nid, int *num){
  return _TreeGetNumSegments(*TreeCtx(), nid, num);
}

int TreeGetSegment(int nid, int idx, struct descriptor_xd *segment, struct descriptor_xd *dim){
/* returns data and dimension of the requested segment idx
 * if segment is NULL loading the data part will be skipped for better performance
 */
  return _TreeGetSegment(*TreeCtx(), nid, idx, segment, dim);
}

int _TreeGetSegmentLimits(void *dbid, int nid, int idx, struct descriptor_xd *retStart, struct descriptor_xd *retEnd);
int TreeGetSegmentLimits(int nid, int idx, struct descriptor_xd *retStart, struct descriptor_xd *retEnd){
  return _TreeGetSegmentLimits(*TreeCtx(), nid, idx, retStart, retEnd);
}

int _TreeGetSegmentInfo(void *dbid, int nid, int idx, char *dtype, char *dimct, int *dims, int *next_row);
int TreeGetSegmentInfo(int nid, int idx, char *dtype, char *dimct, int *dims, int *next_row){
  return _TreeGetSegmentInfo(*TreeCtx(), nid, idx, dtype, dimct, dims, next_row);
}

int TreeGetSegments(int nid, struct descriptor *start, struct descriptor *end, struct descriptor_xd *out){
  return _TreeGetSegments(*TreeCtx(), nid, start, end, out);
}

int TreeGetSegmentedRecord(int nid, struct descriptor_xd *data){
  return _TreeGetSegmentedRecord(*TreeCtx(), nid, data);
}

int _TreePutRow(void *dbid, int nid, int bufsize, int64_t * timestamp, struct descriptor_a *data);
int TreePutRow(int nid, int bufsize, int64_t * timestamp, struct descriptor_a *data){
  return _TreePutRow(*TreeCtx(), nid, bufsize, timestamp, data);
}

int _TreePutTimestampedSegment(void *dbid, int nid, int64_t * timestamp, struct descriptor_a *data);
int TreePutTimestampedSegment(int nid, int64_t * timestamp, struct descriptor_a *data){
  return _TreePutTimestampedSegment(*TreeCtx(), nid, timestamp, data);
}

int _TreeGetSegmentScale(void *dbid, int nid, struct descriptor_xd *value);
int TreeGetSegmentScale(int nid, struct descriptor_xd *value) {
  return _TreeGetSegmentScale(*TreeCtx(), nid, value);
}

int _TreeSetSegmentScale(void *dbid, int nid, struct descriptor *value);
int TreeSetSegmentScale(int nid, struct descriptor *value) {
  return _TreeSetSegmentScale(*TreeCtx(), nid, value);
}

int _TreeMakeXNciSegment(void *dbid, int nid, const char *xnciname, struct descriptor *start, struct descriptor *end, struct descriptor *dimension, struct descriptor_a *initialValue, int idx, int rows_filled);
/* write new segment with prefilled rows range of valid data can be defined by rows_filled
 */

int TreeMakeXNciSegment(int nid, const char *xnciname, struct descriptor *start, struct descriptor *end, struct descriptor *dimension, struct descriptor_a *initialValue, int idx, int rows_filled){
  return _TreeMakeXNciSegment(*TreeCtx(), nid, xnciname, start, end, dimension, initialValue, idx, rows_filled);
}

//int _TreeBeginXNciSegment(void *dbid, int nid, const char *xnciname, struct descriptor *start, struct descriptor *end, struct descriptor *dimension, struct descriptor_a *initialValue, int idx){
/* write new EMPTY segment so data can be added row by row with _TreePutSegment
 * like _TreeMakeSegment but rows_filled will be initialized with 0
 */
//  return _TreeMakeXNciSegment(dbid, nid, xnciname, start, end, dimension, initialValue, idx, 0);
//}
//int TreeBeginXNciSegment(int nid, const char *xnciname, struct descriptor *start, struct descriptor *end, struct descriptor *dimension, struct descriptor_a *initialValue, int idx){
//  return _TreeBeginXNciSegment(*TreeCtx(), nid, xnciname, start, end, dimension, initialValue, idx);
//}

//int _TreeSetXNciRowsFilled(void *dbid, int nid, const char *xnciname, int rows_filled);
/* allows to correct the rows filled property of a segmented node
 * e.g. after it has been written to using beginSegment instead of makeSegment
 */
//int TreeSetXNciRowsFilled(int nid, const char *xnciname, int rows_filled){
//  return _TreeSetXNciRowsFilled(*TreeCtx(), nid, xnciname, rows_filled);
//}

//int _TreeMakeXNciTimestampedSegment(void *dbid, int nid, xnciname, int64_t * timestamps, struct descriptor_a *initialValue, int idx, int rows_filled);
/* write new timestamped (int64_t) segment with prefilled rows range of valid data can be defined by rows_filled
 */
//int TreeMakeXNciTimestampedSegment(int nid, const char *xnciname, int64_t * timestamps, struct descriptor_a *initialValue, int idx, int rows_filled){
//  return _TreeMakeXNciTimestampedSegment(*TreeCtx(), nid, xnciname, timestamps, initialValue, idx, rows_filled);
//}
//int _TreeBeginXNciTimestampedSegment(void *dbid, int nid, const char *xnciname, struct descriptor_a *initialValue, int idx){
/* write new EMPTY timestamped (int64_t) segment so data can be added row by row
 * like _TreeMakeSegment but rows_filled will be initialized with 0
 */
//  return _TreeMakeXNciTimestampedSegment(dbid, nid, xnciname, NULL, initialValue, idx, 0);
//}
//int TreeBeginXNciTimestampedSegment(int nid, const char *xnciname, struct descriptor_a *initialValue, int idx){
//  return _TreeBeginXNciTimestampedSegment(*TreeCtx(), nid, xnciname, initialValue, idx);
//}

//int _TreeUpdateXNciSegment(void *dbid, int nid, const char *xnciname, struct descriptor *start, struct descriptor *end, struct descriptor *dimension, int idx);
/* allows to correct the dimension and limits of a segment without touchng the data part
 */
//int TreeUpdateXNciSegment(int nid, const char *xnciname, struct descriptor *start, struct descriptor *end, struct descriptor *dimension, int idx){
//  return _TreeUpdateXNciSegment(*TreeCtx(), nid, xnciname, start, end, dimension, idx);
//}

//int _TreePutXNciSegment(void *dbid, int nid, const char *xnciname, const int startIdx, struct descriptor_a *data);
//int TreePutXNciSegment(const int nid, const char *xnciname, const int startIdx, struct descriptor_a *data){
//  return _TreePutXNciSegment(*TreeCtx(), nid, xnciname, startIdx, data);
//}

//int _TreeGetXNciSegmentTimesXd(void *dbid, int nid, const char *xnciname, int *nsegs, struct descriptor_xd *start_list, struct descriptor_xd *end_list);
//int TreeGetXNciSegmentTimesXd(int nid, const char *xnciname, int *nsegs, struct descriptor_xd *start_list, struct descriptor_xd *end_list){
//  return _TreeGetXNciSegmentTimesXd(*TreeCtx(), nid, xnciname, nsegs, start_list, end_list);
//}

//int _TreeGetXNciSegmentTimes(void *dbid, int nid, const char *xnciname, int *nsegs, int64_t ** times);
//int TreeGetXNciSegmentTimes(int nid, const char *xnciname, int *nsegs, int64_t ** times){
//  return _TreeGetXNciSegmentTimes(*TreeCtx(), nid, xnciname, nsegs, times);
//}

//int _TreeGetXNciNumSegments(void *dbid, int nid, const char *xnciname, int *num);
/* returns the number of segments stored in the node
 */
//int TreeGetXNciNumSegments(int nid, const char *xnciname, int *num){
//  return _TreeGetXNciNumSegments(*TreeCtx(), nid, xnciname, num);
//}

int TreeGetXNciSegment(int nid, const char *xnciname, int idx, struct descriptor_xd *segment, struct descriptor_xd *dim){
/* returns data and dimension of the requested segment idx
 * if segment is NULL loading the data part will be skipped for better performance
 */
  return _TreeGetXNciSegment(*TreeCtx(), nid, xnciname, idx, segment, dim);
}

//int _TreeGetXNciSegmentLimits(void *dbid, int nid, const char *xnciname, int idx, struct descriptor_xd *retStart, struct descriptor_xd *retEnd);
//int TreeGetXNciSegmentLimits(int nid, const char *xnciname, int idx, struct descriptor_xd *retStart, struct descriptor_xd *retEnd){
//  return _TreeGetXNciSegmentLimits(*TreeCtx(), nid, xnciname, idx, retStart, retEnd);
//}

//int _TreeGetXNciSegmentInfo(void *dbid, int nid, const char *xnciname, int idx, char *dtype, char *dimct, int *dims, int *next_row);
//int TreeGetXNciSegmentInfo(int nid, const char *xnciname, int idx, char *dtype, char *dimct, int *dims, int *next_row){
//  return _TreeGetXNciSegmentInfo(*TreeCtx(), nid, xnciname, idx, dtype, dimct, dims, next_row);
//}

//int TreeGetXNciSegments(int nid, const char *xnciname, struct descriptor *start, struct descriptor *end, struct descriptor_xd *out){
//  return _TreeGetXNciSegments(*TreeCtx(), nid, xnciname, start, end, out);
//}

//int TreeGetXNciSegmentedRecord(int nid, const char *xnciname, struct descriptor_xd *data){
//  return _TreeGetXNciSegmentedRecord(*TreeCtx(), nid, xnciname, data);
//}

//int _TreePutXNciRow(void *dbid, int nid, const char *xnciname, int bufsize, int64_t * timestamp, struct descriptor_a *data);
//int TreePutXNciRow(int nid, const char *xnciname, int bufsize, int64_t * timestamp, struct descriptor_a *data){
//  return _TreePutXNciRow(*TreeCtx(), nid, xnciname, bufsize, timestamp, data);
//}

//int _TreePutXNciTimestampedSegment(void *dbid, int nid, const char *xnciname, int64_t * timestamp, struct descriptor_a *data);
//int TreePutXNciTimestampedSegment(int nid, const char *xnciname, int64_t * timestamp, struct descriptor_a *data){
//  return _TreePutXNciTimestampedSegment(*TreeCtx(), nid, xnciname, timestamp, data);
//}

//int _TreeGetXNciSegmentScale(void *dbid, int nid, const char *xnciname, struct descriptor_xd *value);
//int TreeGetXNciSegmentScale(int nid, const char *xnciname, struct descriptor_xd *value) {
//  return _TreeGetXNciSegmentScale(*TreeCtx(), nid, xnciname, value);
//}

//int _TreeSetXNciSegmentScale(void *dbid, int nid, const char *xnciname, struct descriptor *value);
//int TreeSetXNciSegmentScale(int nid, const char *xnciname, struct descriptor *value) {
//  return _TreeSetXNciSegmentScale(*TreeCtx(), nid, xnciname, value);
//}

int _TreeSetXNci(void *dbid, int nid, const char *xnciname, struct descriptor *value);
int TreeSetXNci(int nid, const char *xnciname, struct descriptor *value){
  return _TreeSetXNci(*TreeCtx(), nid, xnciname, value);
}

int _TreeGetXNci(void *dbid, int nid, const char *xnciname, struct descriptor_xd *value);
int TreeGetXNci(int nid, const char *xnciname, struct descriptor_xd *value){
  return _TreeGetXNci(*TreeCtx(), nid, xnciname, value);
}


//////////////////////////////////////////////////////////////////////
/////////read/write properties ///////////////////////////////////////
//////////////////////////////////////////////////////////////////////
#ifdef    WORDS_BIGENDIAN
# define _WORDS_BIGENDIAN // can be activated to test WORDS_BIGENDIAN branch on little endian machines
#endif

static int read_property(TREE_INFO * tinfo, const int64_t offset, char *buffer, const int length){
  if (!tinfo->data_file) return MDSplusFATAL;
  INIT_TREESUCCESS;
  int deleted = B_TRUE;
  while STATUS_OK {
    status = (MDS_IO_READ_X(tinfo->data_file->get,offset,buffer,length, &deleted) == length) ? TreeSUCCESS : TreeFAILURE;
    if (STATUS_OK && deleted)
      status = TreeReopenDatafile(tinfo);
    else break;
  }
  return status;
}

inline static int read_property_safe(TREE_INFO * tinfo, const int64_t offset,char *buffer,const int length) {
  if (offset > -1) return read_property(tinfo,offset,buffer,length);
  return TreeFAILURE;
}

typedef struct {
 TREE_INFO *tinfo;
 int64_t   offset;
} write_cleanup_t;
static void unlock_datafile(void* c) {
  const write_cleanup_t *s = (write_cleanup_t*)c;
  if (s->offset!=-1)
    TreeUnLockDatafile(s->tinfo, 0, s->offset);
}
static int write_property(TREE_INFO * tinfo, int64_t *offset, const char *buffer, const int length) {
  if (!tinfo->data_file) return MDSplusFATAL;
  int status;
  write_cleanup_t c = {tinfo,-1};
  pthread_cleanup_push(unlock_datafile,&c);
  if (*offset == -1) {
    TreeLockDatafile(tinfo, 0, c.offset = 0      );
    *offset = MDS_IO_LSEEK(tinfo->data_file->put, 0      , SEEK_END);
  } else {
    TreeLockDatafile(tinfo, 0, c.offset = *offset);
	      MDS_IO_LSEEK(tinfo->data_file->put, *offset, SEEK_SET);
  }
  status = MDS_IO_WRITE(tinfo->data_file->put, (void*)buffer, length)
	== length ? TreeSUCCESS : TreeFAILURE;
  pthread_cleanup_pop(1);
  return status;
}

# define swap_inplace(dir,type,buf,param) dir##type(&buf,&(param))
# define swap_header(dir,buf,hdr) {\
  char *ptr = buf; int i;\
  swap_inplace(dir,int8 ,ptr,	hdr->dtype	);\
  swap_inplace(dir,int8 ,ptr,	hdr->dimct	);\
  for (i = 0; i < MAX_DIMS; i++)\
    swap_inplace(dir,int32,ptr,	hdr->dims[i]	);\
  swap_inplace(dir,int16,ptr,	hdr->length	);\
  swap_inplace(dir,int32,ptr,	hdr->idx	);\
  swap_inplace(dir,int32,ptr,	hdr->next_row	);\
  swap_inplace(dir,int64,ptr,	hdr->index_offset);\
  swap_inplace(dir,int64,ptr,	hdr->data_offset);\
  swap_inplace(dir,int64,ptr,	hdr->dim_offset	);\
}
#define SEGMENT_HEADER_SIZE 1+1 + MAX_DIMS*4 + 2+4+4+8+8+8
static int put_segment_header(TREE_INFO * tinfo, const SEGMENT_HEADER * hdr, int64_t * offset){
  /* What a hack
  char *next_row_fix = getenv("NEXT_ROW_FIX");
  if (next_row_fix != 0) {
    int fix = strtol(next_row_fix,NULL,0);
    if (fix > 0) {
      if (fix <= hdr->next_row) {
	hdr->next_row -= fix;
	printf("next row adjusted down %d, now %d\n", fix, hdr->next_row);
      } else
	printf("next row not adjusted, requested=%d, next_row=%d\n", fix, hdr->next_row);
    }
  }
  */
  char buffer[SEGMENT_HEADER_SIZE];
  swap_header(put,buffer,hdr);
  return write_property(tinfo, offset, buffer, sizeof(buffer));
}
static int get_segment_header(TREE_INFO * tinfo, const int64_t offset, SEGMENT_HEADER * hdr){
  char buffer[SEGMENT_HEADER_SIZE];
  const int status = read_property_safe(tinfo, offset, buffer, sizeof(buffer));
  if (status == TreeSUCCESS) swap_header(get,buffer,hdr);
  return status;
}
#define SEGMENT_INDEX_SIZE 8+4 + SEGMENTS_PER_INDEX * (8+8 + 8+4 + 8+4 + 8+4 + 8+4)
#define swap_sindex(dir,buf,sindex) {\
  char *ptr = buf; int i;\
  swap_inplace(dir,int64,ptr,	sindex->previous_offset		);\
  swap_inplace(dir,int32,ptr,	sindex->first_idx		);\
  for (i = 0; i < SEGMENTS_PER_INDEX; i++) {\
    swap_inplace(dir,int64,ptr,	sindex->segment[i].start	);\
    swap_inplace(dir,int64,ptr,	sindex->segment[i].end		);\
    swap_inplace(dir,int64,ptr,	sindex->segment[i].start_offset	);\
    swap_inplace(dir,int32,ptr,	sindex->segment[i].start_length	);\
    swap_inplace(dir,int64,ptr,	sindex->segment[i].end_offset	);\
    swap_inplace(dir,int32,ptr,	sindex->segment[i].end_length	);\
    swap_inplace(dir,int64,ptr,	sindex->segment[i].dimension_offset);\
    swap_inplace(dir,int32,ptr,	sindex->segment[i].dimension_length);\
    swap_inplace(dir,int64,ptr,	sindex->segment[i].data_offset	);\
    swap_inplace(dir,int32,ptr,	sindex->segment[i].rows		);\
  }\
}
static int put_segment_index(TREE_INFO * tinfo, const SEGMENT_INDEX * sindex, int64_t * offset){
  char buffer[SEGMENT_INDEX_SIZE];
  swap_sindex(put,buffer,sindex);
  return write_property(tinfo, offset, buffer, sizeof(buffer));
}
static int get_segment_index(TREE_INFO * tinfo, const int64_t offset, SEGMENT_INDEX * sindex){
  char buffer[SEGMENT_INDEX_SIZE];
  const int status = read_property_safe(tinfo, offset, buffer, sizeof(buffer));
  if (status == TreeSUCCESS) swap_sindex(get,buffer,sindex);
  return status;
}

#define swap_ext_attr(dir,buf,ext_attr) {\
  char* ptr = buf; int i;\
  swap_inplace(dir,int64,ptr,	ext_attr->next_ea_offset	);\
  for (i = 0; i < FACILITIES_PER_EA; i++) \
    swap_inplace(dir,int64,ptr,	ext_attr->facility_offset[i]	);\
  for (i = 0; i < FACILITIES_PER_EA; i++) \
    swap_inplace(dir,int32,ptr,	ext_attr->facility_length[i]	);\
}
#define EXTENDED_ATTRIBUTES_SIZE 8 + FACILITIES_PER_EA * (8+4)
int TreePutExtendedAttributes(TREE_INFO * tinfo, EXTENDED_ATTRIBUTES * ext_attr, int64_t * offset){
  char buffer[EXTENDED_ATTRIBUTES_SIZE];
  swap_ext_attr(put,buffer,ext_attr);
  return write_property(tinfo, offset, buffer, sizeof(buffer));
}
int TreeGetExtendedAttributes(TREE_INFO * tinfo, const int64_t offset, EXTENDED_ATTRIBUTES * ext_attr){
  char buffer[EXTENDED_ATTRIBUTES_SIZE];
  const int status = read_property_safe(tinfo, offset, buffer, sizeof(buffer));
  if (status == TreeSUCCESS) swap_ext_attr(get,buffer,ext_attr);
  return status;
}

#define swap_named_attr(dir,buf,index) {\
  char* ptr = buf; int i;\
  swap_inplace(dir,int64,ptr,	index->previous_offset		);\
  for (i = 0; i < NAMED_ATTRIBUTES_PER_INDEX; i++) {\
    dir##chars(&ptr,index->attribute[i].name,NAMED_ATTRIBUTE_NAME_SIZE);\
    swap_inplace(dir,int64,ptr,	index->attribute[i].offset	);\
    swap_inplace(dir,int32,ptr,	index->attribute[i].length	);\
  }\
}
#define NAMED_ATTRIBUTES_INDEX_SIZE 8 + NAMED_ATTRIBUTES_PER_INDEX * (NAMED_ATTRIBUTE_NAME_SIZE+8+4)
static int put_named_attributes_index(TREE_INFO * tinfo, const NAMED_ATTRIBUTES_INDEX * index, int64_t * offset){
  char buffer[NAMED_ATTRIBUTES_INDEX_SIZE];
  swap_named_attr(put,buffer,index);
  return write_property(tinfo, offset, buffer, sizeof(buffer));
}
static int get_named_attributes_index(TREE_INFO * tinfo, const int64_t offset, NAMED_ATTRIBUTES_INDEX * index){
  char buffer[NAMED_ATTRIBUTES_INDEX_SIZE];
  const int status = read_property_safe(tinfo, offset, buffer, sizeof(buffer));
  if (status == TreeSUCCESS) swap_named_attr(get,buffer,index);
  return status;
}

inline static int load_node_ptr(vars_t* vars) {
  vars->node_ptr = nid_to_node(vars->dblist, vars->nid_ptr);
  if (!vars->node_ptr)
    return TreeNNF;
// if (vars->node_ptr->usage != TreeUSAGE_SIGNAL) return TreeINVDTPUSG;
  return TreeSUCCESS;
}

inline static int check_segment_remote(vars_t* vars) {
  if (vars->dblist->remote) {
    printf("Segmented records are not supported using thick client mode\n");
    return TreeFAILURE;
  }
  return TreeSUCCESS;
}

inline static int load_info_ptr(vars_t* vars) {
  vars->nidx = nid_to_tree_idx(vars->dblist, vars->nid_ptr, &vars->tinfo);
  if (!vars->tinfo) return TreeNNF; //TODO verify errorcode
  return TreeSUCCESS;
}

#define shot_open (vars->dblist->shotid != -1)

inline static int open_datafile_write0(vars_t* vars) {
  INIT_TREESUCCESS;
  if (vars->dblist->open_readonly)
    return TreeREADONLY;
  RETURN_IF_NOT_OK(load_node_ptr(vars));
  RETURN_IF_NOT_OK(check_segment_remote(vars));
  RETURN_IF_NOT_OK(load_info_ptr(vars));
  TreeCallHookFun("TreeNidHook","PutData",vars->tinfo->treenam,vars->tinfo->shot,*vars->nid_ptr,NULL);
  status = TreeCallHook(PutData, vars->tinfo, *(int*)vars->nid_ptr);
  if (status && STATUS_NOT_OK)
    return status;
  TreeGetViewDate(&vars->saved_viewdate);
  status = TreeGetNciLw(vars->tinfo, vars->nidx, &vars->local_nci);
  if STATUS_NOT_OK return status;
  vars->nci_locked = 1;
  if (vars->dblist->shotid == -1) {
    if (vars->local_nci.flags & NciM_NO_WRITE_MODEL)
	return TreeNOWRITEMODEL;
  } else {
    if (vars->local_nci.flags & NciM_NO_WRITE_SHOT)
	return TreeNOWRITESHOT;
  }
  if (STATUS_OK && (vars->local_nci.flags & NciM_WRITE_ONCE)) {
    if (vars->local_nci.length)
      return TreeNOOVERWRITE;
    vars->local_nci.flags &= ~NciM_WRITE_ONCE;
  }
  return status;
}

inline static int open_datafile_write1(vars_t* vars){
  if (vars->tinfo->data_file ? (!vars->tinfo->data_file->open_for_write) : 1)
     return TreeOpenDatafileW(vars->tinfo, &vars->stv, 0);
  return TreeSUCCESS;
}

inline static int begin_finish(vars_t* vars){
  int status = put_segment_index(vars->tinfo,&vars->sindex,&vars->shead.index_offset);
  if STATUS_NOT_OK return status;
  status = put_segment_header(vars->tinfo, &vars->shead, &vars->attr.facility_offset[SEGMENTED_RECORD_FACILITY]);
  if (vars->attr_update) {
    status = TreePutExtendedAttributes(vars->tinfo, &vars->attr, &vars->attr_offset);
    SeekToRfa(vars->attr_offset, vars->local_nci.DATA_INFO.DATA_LOCATION.rfa);
    vars->local_nci.flags2 |= NciM_EXTENDED_NCI;
  }
  if STATUS_NOT_OK return status;
  if (((int64_t) vars->local_nci.length + (int64_t) vars->add_length) < (int64_t)0xffffffffU)
    vars->local_nci.length += vars->add_length;
  else
    vars->local_nci.length = 0xffffffffU;
  vars->local_nci.flags=vars->local_nci.flags | NciM_SEGMENTED;
  return TreePutNci(vars->tinfo, vars->nidx, &vars->local_nci, 0);
}


#ifndef _WIN32
static int saved_uic = 0;
static void init_saved_uic() {
  saved_uic = (getgid() << 16) | getuid();
}
#endif

inline static void begin_local_nci(vars_t* vars, const struct descriptor_a *initialValue){
  vars->local_nci.flags2 &= ~NciM_DATA_IN_ATT_BLOCK;
  vars->local_nci.dtype = initialValue->dtype;
  vars->local_nci.class = CLASS_R;
  vars->local_nci.time_inserted = TreeTimeInserted();
#ifndef _WIN32
  RUN_FUNCTION_ONCE(init_saved_uic);
#else
  const int saved_uic = 0;
#endif
  vars->local_nci.owner_identifier = saved_uic;
}

/* See if node is currently using the Extended Nci feature and if so get the current contents of the attr
 * index. If not, make an empty index and flag that a new index needs to be written.
 */
#define IF_NO_EXTENDED_NCI \
if (!(vars->local_nci.flags2 & NciM_EXTENDED_NCI) \
 || IS_NOT_OK(TreeGetExtendedAttributes(vars->tinfo, RfaToSeek(vars->local_nci.DATA_INFO.DATA_LOCATION.rfa), &vars->attr)))


inline static void begin_extended_nci(vars_t* vars){
  IF_NO_EXTENDED_NCI {
    memset(&vars->attr, -1, sizeof(vars->attr));
    vars->attr_update = 1;
    vars->attr_offset = -1;
  } else {
    vars->attr_offset = RfaToSeek(vars->local_nci.DATA_INFO.DATA_LOCATION.rfa);
    if (vars->attr.facility_offset[STANDARD_RECORD_FACILITY] != -1) {
      vars->attr.facility_offset[STANDARD_RECORD_FACILITY] = -1;
      vars->attr.facility_length[STANDARD_RECORD_FACILITY] = 0;
      vars->attr_update = 1;
    }
  }
}

/* See if the node currently has an segment header record.
 * If not, make an empty segment header and flag that a new one needs to be written.
 */
#define IF_NO_SEGMENT_HEADER \
if (vars->attr.facility_offset[SEGMENTED_RECORD_FACILITY] == -1 \
 || IS_NOT_OK(get_segment_header(vars->tinfo, vars->attr.facility_offset[SEGMENTED_RECORD_FACILITY],&vars->shead)))


inline static int begin_segment_header(vars_t* vars,struct descriptor_a *initialValue){
  IF_NO_SEGMENT_HEADER {
    memset(&vars->shead, 0, sizeof(vars->shead));
    vars->attr.facility_offset[SEGMENTED_RECORD_FACILITY] = -1;
    vars->shead.index_offset = -1;
    vars->shead.idx = -1;
    vars->attr_update = 1;
  } else if (initialValue->dtype != vars->shead.dtype ||
	    (initialValue->class == CLASS_A &&
	    (initialValue->dimct != vars->shead.dimct ||
	    (initialValue->dimct > 1
	     && memcmp(vars->shead.dims, ((A_COEFF_TYPE *)initialValue)->m, (initialValue->dimct - 1) * sizeof(int))))))
    return TreeFAILURE;
  return TreeSUCCESS;
}
static int get_xnci_info(vars_t* vars, const char *xnciname, int *length, int64_t *offset,struct descriptor_xd *value );

#define IF_NO_XNCI_SEGMENT_HEADER		\
  int xnci_length; \
  if (IS_NOT_OK(get_xnci_info(vars, xnciname, &xnci_length, &vars->attr.facility_offset[SEGMENTED_RECORD_FACILITY],NULL)) \
    || (xnci_length > 0) || IS_NOT_OK(get_segment_header(vars->tinfo, vars->attr.facility_offset[SEGMENTED_RECORD_FACILITY], &vars->shead)))    

inline static int begin_xnci_segment_header(vars_t* vars, const char *xnciname, struct descriptor_a *initialValue){
  IF_NO_XNCI_SEGMENT_HEADER {
    memset(&vars->shead, 0, sizeof(vars->shead));
    vars->attr.facility_offset[SEGMENTED_RECORD_FACILITY] = -1;
    vars->shead.index_offset = -1;
    vars->shead.idx = -1;
    vars->attr_update = 0;
  } else if (initialValue->dtype != vars->shead.dtype ||
	    (initialValue->class == CLASS_A &&
	    (initialValue->dimct != vars->shead.dimct ||
	    (initialValue->dimct > 1
	     && memcmp(vars->shead.dims, ((A_COEFF_TYPE *)initialValue)->m, (initialValue->dimct - 1) * sizeof(int))))))
    return TreeFAILURE;
  return TreeSUCCESS;
}

#define IF_NO_SEGMENT_INDEX \
if ((vars->shead.index_offset == -1) \
 || IS_NOT_OK(get_segment_index(vars->tinfo, vars->shead.index_offset, &vars->sindex)))

inline static void begin_segment_index(vars_t* vars) {
  IF_NO_SEGMENT_INDEX {
    vars->shead.index_offset = -1;
    memset(&vars->sindex, -1, sizeof(vars->sindex));
    vars->sindex.first_idx = 0;
  }
}

inline static int check_sinfo(vars_t*vars) {
  INIT_TREESUCCESS;
  if (vars->idx < 0 || vars->idx > vars->shead.idx)
    return TreeFAILURE;
  for (vars->index_offset = vars->shead.index_offset;
       STATUS_OK && vars->idx < vars->sindex.first_idx && vars->sindex.previous_offset > 0;) {
    vars->index_offset = vars->sindex.previous_offset;
    status = get_segment_index(vars->tinfo, vars->sindex.previous_offset, &vars->sindex);
  }
  if (STATUS_NOT_OK || (vars->idx < vars->sindex.first_idx))
    return TreeFAILURE;
  vars->sinfo = &vars->sindex.segment[vars->idx % SEGMENTS_PER_INDEX];
  return status;
}

inline static int begin_sinfo(vars_t*vars,struct descriptor_a *initialValue,int checkcompress()) {
  INIT_TREESUCCESS;
  vars->add_length = 0;
  if (vars->idx == -1) {
    vars->shead.idx++;
    vars->idx = vars->shead.idx;
    vars->add_length = (initialValue->class == CLASS_A) ? initialValue->arsize : 0;
  } else if (vars->idx < -1 || vars->idx > vars->shead.idx)
    return TreeBUFFEROVF;
  else {
    /* TODO: Add support for updating an existing segment. vars->add_length=new_length-old_length. */
    /* Potentially use same storage area if old array is same size. */
    printf("this is not yet supported\n");
    return TreeFAILURE;
  }
  vars->shead.data_offset = -1;
  vars->shead.dim_offset = -1;
  vars->shead.dtype = initialValue->dtype;
  vars->shead.dimct = (initialValue->class == CLASS_A) ? initialValue->dimct : 0;
  vars->shead.length = (initialValue->class == CLASS_A) ? initialValue->length : 0;
  int previous_length = -1;
  if (vars->shead.idx > 0 && vars->shead.length != 0) {
    int d;
    previous_length = vars->shead.length;
    for (d = 0; d < vars->shead.dimct; d++)
    previous_length *= vars->shead.dims[d];
  } else previous_length = -1;
  if (initialValue->class == CLASS_A) {
    if (initialValue->dimct == 1)
      vars->shead.dims[0] = initialValue->arsize / initialValue->length;
    else
      memcpy(vars->shead.dims, ((A_COEFF_TYPE *)initialValue)->m, initialValue->dimct * sizeof(int));
  }
  vars->shead.next_row = vars->rows_filled;
  /* If not the first segment, see if we can reuse the previous segment storage space
   * and compress the previous segment. */
  if (((vars->shead.idx % SEGMENTS_PER_INDEX) > 0) &&
      (previous_length == (int64_t)vars->add_length) && vars->compress) {
    EMPTYXD(xd_data);
    EMPTYXD(xd_dim);
    vars->sinfo = &vars->sindex.segment[(vars->idx % SEGMENTS_PER_INDEX) - 1];
    unlock_nci(vars);
    status = _TreeGetSegment(vars->dblist, *(int*)vars->nid_ptr, vars->idx - 1, &xd_data, &xd_dim);
    lock_nci(vars);
    if STATUS_OK
      status = checkcompress(vars,xd_data,xd_dim,initialValue);
    MdsFree1Dx(&xd_data, 0);
    MdsFree1Dx(&xd_dim, 0);
  }
  if (vars->idx >= vars->sindex.first_idx + SEGMENTS_PER_INDEX) {
    memset(&vars->sindex, -1, sizeof(vars->sindex));
    vars->sindex.previous_offset = vars->shead.index_offset;
    vars->shead.index_offset = -1;
    vars->sindex.first_idx = vars->idx;
  }
  vars->sinfo = &vars->sindex.segment[vars->idx % SEGMENTS_PER_INDEX];
  return status;
}

inline static int put_initialvalue(TREE_INFO *tinfo, int *dims, struct descriptor_a *array, int64_t *offset){
  int status;
  int length = array->length;
  int i;
  for (i = 0; i < array->dimct; i++)
    length = length * dims[i];
  ALLOCATE_BUFFER(length,buffer);
  CHECK_ENDIAN_TRANSFER(array->pointer,length,array->length,array->dtype,buffer);
  status = write_property(tinfo, offset, buffer, length);
  FREE_BUFFER(buffer);
  return status;
}

inline static int putdata_initialvalue(vars_t*vars,struct descriptor_a* initialValue) {
  int status;
  if (initialValue->dtype == DTYPE_OPAQUE) {
    int length;
    status = TreePutDsc(vars->tinfo, *(int*)vars->nid_ptr, (struct descriptor *)initialValue, &vars->sinfo->data_offset, &length, vars->compress);
    vars->shead.data_offset = vars->sinfo->data_offset;
    vars->add_length = length;
    vars->sinfo->rows = length | 0x80000000;
  } else
    status = put_initialvalue(vars->tinfo, vars->shead.dims, initialValue, &vars->shead.data_offset);
  if (initialValue->dtype != DTYPE_OPAQUE) {
    vars->sinfo->data_offset = vars->shead.data_offset;
    vars->sinfo->rows = vars->shead.dims[vars->shead.dimct - 1];
  }
  return status;
}

inline static int put_limit_by_dsc(vars_t*vars,struct descriptor *limit_in, int64_t* limit_out, int64_t* offset, int* length) {
  struct descriptor *dsc;
  for (dsc = limit_in; dsc && dsc->pointer && dsc->dtype == DTYPE_DSC;
    dsc = (struct descriptor *)dsc->pointer);
  if (dsc && dsc->pointer && (dsc->dtype == DTYPE_Q || dsc->dtype == DTYPE_QU)) {
    *limit_out = *(int64_t *) dsc->pointer;
    *offset = -1;
    *length = 0;
    return TreeSUCCESS;
  }
  if (limit_in) {
    *limit_out = -1;
    return TreePutDsc(vars->tinfo, *(int*)vars->nid_ptr, limit_in, offset, length, vars->compress);
  }
  return TreeSUCCESS;
}

inline static int putdim_dim(vars_t*vars,struct descriptor *start, struct descriptor *end, struct descriptor *dimension) {
  INIT_TREESUCCESS;
  RETURN_IF_NOT_OK(put_limit_by_dsc(vars,start,&vars->sinfo->start,&vars->sinfo->start_offset,&vars->sinfo->start_length));
  RETURN_IF_NOT_OK(put_limit_by_dsc(vars,end  ,&vars->sinfo->end  ,&vars->sinfo->end_offset  ,&vars->sinfo->end_length  ));
  if (dimension)
    status = TreePutDsc(vars->tinfo, *(int*)vars->nid_ptr, dimension, &vars->sinfo->dimension_offset, &vars->sinfo->dimension_length, vars->compress);
  return status;
}

inline static int putdim_ts(vars_t* vars, int64_t * timestamps) {
  int status;
  INIT_AND_FREE_ON_EXIT(char*,fbuffer);
  const int rows = vars->shead.dims[vars->shead.dimct-1];
  const int bufsize = sizeof(int64_t)*rows;
  char *buffer;
  if (!timestamps)
    fbuffer = buffer = calloc(bufsize,1);
  else if (vars->rows_filled<rows) {
    int off;
    fbuffer = buffer = malloc(bufsize);
    memcpy(buffer,timestamps,off = sizeof(int64_t)*vars->rows_filled);
    memset(buffer+off, 0, bufsize - off);
  } else {
    fbuffer = NULL;// nothing to free
    buffer = (char*)timestamps;
  }
  status = write_property(vars->tinfo, &vars->shead.dim_offset, buffer, bufsize);
  FREE_NOW(fbuffer);
  if STATUS_OK {
    vars->sinfo->start = vars->rows_filled > 0 ? timestamps[0] : 0;
    vars->sinfo->end   = vars->rows_filled > 0 ? timestamps[vars->rows_filled - 1] : 0;
    vars->sinfo->dimension_offset = vars->shead.dim_offset;
    vars->sinfo->dimension_length = 0;
  }
  return status;
}

inline static int check_compress_dim(vars_t* vars, struct descriptor_xd *xd_data_ptr,struct descriptor_xd *xd_dim_ptr __attribute__((unused)),struct descriptor_a *initialValue __attribute__((unused))) {
  int length = 0;
  vars->shead.data_offset = vars->sinfo->data_offset;
  int status = TreePutDsc(vars->tinfo, *(int*)vars->nid_ptr, xd_data_ptr->pointer, &vars->sinfo->data_offset, &length, vars->compress);
  /*** flag compressed segment by setting high bit in the rows field. ***/
  vars->sinfo->rows = length | 0x80000000;
  return status;
}

inline static int check_compress_ts(vars_t*vars, struct descriptor_xd *xd_data_ptr,struct descriptor_xd *xd_dim_ptr,struct descriptor_a *initialValue) {
  int length;
  A_COEFF_TYPE *data_a = (A_COEFF_TYPE *) xd_data_ptr->pointer;
  A_COEFF_TYPE *dim_a = (A_COEFF_TYPE *) xd_dim_ptr->pointer;
  int rows = (initialValue->dimct == 1) ?
	     initialValue->arsize / initialValue->length :
	     ((A_COEFF_TYPE *) initialValue)->m[initialValue->dimct - 1];
  if (data_a && data_a->class == CLASS_A && data_a->pointer
      && data_a->arsize >= initialValue->arsize && dim_a && dim_a->class == CLASS_A
      && dim_a->pointer && dim_a->arsize >= (rows * sizeof(int64_t)) && dim_a->dimct == 1
      && dim_a->length == sizeof(int64_t) && dim_a->dtype == DTYPE_Q) {
    vars->shead.data_offset = vars->sinfo->data_offset;
    vars->shead.dim_offset = vars->sinfo->dimension_offset;
    int status = TreePutDsc(vars->tinfo, *(int*)vars->nid_ptr, xd_data_ptr->pointer, &vars->sinfo->data_offset, &length, vars->compress);
    int statdim= TreePutDsc(vars->tinfo, *(int*)vars->nid_ptr, xd_dim_ptr->pointer, &vars->sinfo->dimension_offset,&vars->sinfo->dimension_length, vars->compress);
    vars->sinfo->start = ((int64_t *) dim_a->pointer)[0];
    vars->sinfo->end = ((int64_t *) dim_a->pointer)[(dim_a->arsize / dim_a->length) - 1];
     /*** flag compressed segment by setting high bit in the rows field. ***/
    vars->sinfo->rows = length | 0x80000000;
    return STATUS_OK ? statdim : status;
  } else {
    if (!data_a)
      printf("data_a null\n");
    else if (data_a->class != CLASS_A)
      printf("data_a is not CLASS_A, class=%d\n", data_a->class);
    else if (!data_a->pointer)
      printf("data_a's pointer is null\n");
    else if (data_a->arsize < initialValue->arsize)
      printf("data_a->arsize (%d) < initialValue->arsize (%d)\n", data_a->arsize,
	   initialValue->arsize);
    else if (!dim_a)
      printf("dim_a null\n");
    else if (dim_a->class != CLASS_A)
      printf("dim_a is not CLASS_A, class=%d\n", dim_a->class);
    else if (!dim_a->pointer)
      printf("dim_a's pointer is null\n");
    else if (dim_a->arsize < (rows * sizeof(int64_t)))
      printf("dim_a->arsize (%d) < (rows (%d) * sizeof(int64_t))", dim_a->arsize, rows);
    else if (dim_a->dimct != 1)
      printf("dim_a->dimct (%d) != 1\n", dim_a->dimct);
    else if (dim_a->length != sizeof(int64_t))
      printf("dim_a->length (%d) != sizeof(int64_t)\n", dim_a->length);
    else if (dim_a->dtype != DTYPE_Q)
      printf("dim_a->dtype (%d) != DTYPE_Q\n", dim_a->dtype);
    return TreeFAILURE;
  }
}

inline static void set_compress(vars_t* vars){
vars->compress =(vars->local_nci.flags & NciM_COMPRESS_ON_PUT)
	     && (vars->local_nci.flags & NciM_COMPRESS_SEGMENTS)
	     &&!(vars->local_nci.flags & NciM_DO_NOT_COMPRESS);
}

inline static int check_input(struct descriptor_a **data_p){
  while (*data_p && (*data_p)->dtype == DTYPE_DSC)
    *data_p = (struct descriptor_a *)(*data_p)->pointer;
  if (*data_p == NULL
   || !(((*data_p)->class == CLASS_R && (*data_p)->dtype == DTYPE_OPAQUE)
     || ((*data_p)->class == CLASS_A && ((*data_p)->dimct > 0 || (*data_p)->dimct <= 8))) )
    return TreeINVDTPUSG;
  return TreeSUCCESS;
}

inline static int open_datafile_read(vars_t* vars) {
  if (!(IS_OPEN(vars->dblist))) return TreeNOT_OPEN;
  int status;
  RETURN_IF_NOT_OK(load_node_ptr(vars));
  RETURN_IF_NOT_OK(check_segment_remote(vars));
  RETURN_IF_NOT_OK(load_info_ptr(vars));
  TreeGetViewDate(&vars->saved_viewdate);
  RETURN_IF_NOT_OK(TreeGetNciW(vars->tinfo, vars->nidx, &vars->local_nci, 0));
  if (!vars->tinfo->data_file)
    status = TreeOpenDatafileR(vars->tinfo);
  return status;
}

inline static int open_header_read(vars_t*vars){
  int status;
  RETURN_IF_NOT_OK(open_datafile_read(vars));
  IF_NO_EXTENDED_NCI   return TreeNOSEGMENTS;
  IF_NO_SEGMENT_HEADER return TreeNOSEGMENTS;
  return status;
}

inline static int open_xnci_header_read(vars_t*vars, const char *xnciname){
  int status;
  RETURN_IF_NOT_OK(open_datafile_read(vars));
  IF_NO_EXTENDED_NCI   return TreeNOSEGMENTS;
  IF_NO_XNCI_SEGMENT_HEADER return TreeNOSEGMENTS;
  return status;
}

inline static int open_index_read(vars_t*vars){
  int status;
  RETURN_IF_NOT_OK(open_header_read(vars));
  IF_NO_SEGMENT_INDEX return TreeFAILURE;
  return status;
}

inline static int open_xnci_index_read(vars_t*vars, const char *xnciname){
  int status;
  RETURN_IF_NOT_OK(open_xnci_header_read(vars,xnciname));
  IF_NO_SEGMENT_INDEX return TreeFAILURE;
  return status;
}

static int get_xnci_info(vars_t *vars, const char *xnciname, int *length, int64_t *offset,struct descriptor_xd *value )
{
  int status = 0;
  if (!xnciname || strlen(xnciname) < 1 || strlen(xnciname) > NAMED_ATTRIBUTE_NAME_SIZE)
    return TreeFAILURE;
  RETURN_IF_NOT_OK(open_datafile_read(vars));
    NAMED_ATTRIBUTES_INDEX index;
    char *attnames = "attributenames";
    int getnames = 0;
    struct _namelist {
      char name[NAMED_ATTRIBUTE_NAME_SIZE + 1];
      struct _namelist *next;
    } *namelist = 0;
    size_t longestattname = 0;
    int numnames = 0;
    size_t i;
    size_t len = strlen(xnciname);
    if (len == strlen(attnames)) {
      for (i = 0; i < len; i++) {
	if (tolower(xnciname[i]) != attnames[i])
	  break;
      }
      if (i == len)
	getnames = 1;
    }
    if (((vars->local_nci.flags2 & NciM_EXTENDED_NCI) == 0) ||
	((TreeGetExtendedAttributes(vars->tinfo, RfaToSeek(vars->local_nci.DATA_INFO.DATA_LOCATION.rfa), &vars->attr) & 1) == 0)) {
      status = TreeFAILURE;
    } else if (vars->attr.facility_offset[NAMED_ATTRIBUTES_FACILITY] == -1
	   ||  IS_NOT_OK(get_named_attributes_index(vars->tinfo, vars->attr.facility_offset[NAMED_ATTRIBUTES_FACILITY],&index))) {
      status = TreeFAILURE;
    } else {
      int found_index = -1;
      while (found_index == -1) {
	unsigned int i, j;
	for (i = 0; i < NAMED_ATTRIBUTES_PER_INDEX; i++) {
	  if (getnames == 1) {
	    if (index.attribute[i].name[0] != 0) {
	      size_t len = strlen(index.attribute[i].name);
	      struct _namelist *p = malloc(sizeof(struct _namelist));
	      memcpy(p->name, index.attribute[i].name, sizeof(p->name));
	      p->next = namelist;
	      namelist = p;
	      longestattname = (len > longestattname) ? len : longestattname;
	      numnames++;
	    }
	  } else {
	    for (j = 0; j < len; j++) {
	      if (tolower(xnciname[j]) != tolower(index.attribute[i].name[j]))
	        break;
	    }
	    if (j == len && index.attribute[i].name[j] == 0)
	      break;
	  }
	}
	if (i < NAMED_ATTRIBUTES_PER_INDEX)
	  found_index = i;
	else if (index.previous_offset != -1) {
	  if ((get_named_attributes_index(vars->tinfo, index.previous_offset, &index) & 1) == 0) {
	    break;
	  }
	} else
	  break;
      }
      if (found_index != -1) {
	*offset = index.attribute[found_index].offset;
	*length = index.attribute[found_index].length;
	status = TreeSUCCESS;
      } else if (getnames == 1) {
	if (namelist == 0) {
	  status = TreeFAILURE;
	} else {
	  char *names = malloc(longestattname * numnames);
	  DESCRIPTOR_A(name_array, (short)longestattname, DTYPE_T, names,
	               (unsigned int)(longestattname * numnames));
	  struct _namelist *p, *pnext;
	  char *np;
	  for (p = namelist, np = names; p; p = pnext, np += longestattname) {
	    size_t i;
	    pnext = p->next;
	    memcpy(np, p->name, longestattname);
	    for (i = 1; i < longestattname; i++)
	      if (np[i] == '\0')
	        np[i] = ' ';
	    free(p);
	  }
	  if (value != NULL)
	    MdsCopyDxXd((struct descriptor *)&name_array, value);
	  free(names);
	}
      } else
	status = TreeFAILURE;
    }
  return status;
}

static int get_compressed_segment_rows(TREE_INFO * tinfo, const int64_t offset, int *rows){
  INIT_TREESUCCESS;
  char buffer[60];
  status = read_property_safe(tinfo,offset, buffer, sizeof(buffer));
  if STATUS_OK {
    if ((class_t)buffer[3] == CLASS_CA || (class_t)buffer[3] == CLASS_A) {
      char dimct = buffer[11];
      if (dimct == 1) {
	loadint32(rows,&buffer[12]);// arsize
	*rows = *rows / swapint16(buffer);// length
      } else
	loadint32(rows,&buffer[16+dimct*4]);// last dim
      return TreeSUCCESS;
    } else
      return TreeFAILURE;
  }
  return status;
}

#define GOTO_END_ON_ERROR(statement) do{if IS_NOT_OK((status = statement)) goto end;}while(0)

#define CLEANUP_NCI_PUSH pthread_cleanup_push(unlock_nci,(void*)vars)
#define CLEANUP_NCI_POP  pthread_cleanup_pop(vars->nci_locked)
//#define CLEANUP_NCI_PUSH
//#define CLEANUP_NCI_POP  unlock_nci(vars)

int _TreeMakeSegment(void *dbid, int nid, struct descriptor *start, struct descriptor *end, struct descriptor *dimension, struct descriptor_a *initValIn, int idx, int rows_filled){
  INIT_WRITE_VARS;
  CLEANUP_NCI_PUSH;
  struct descriptor_a* initialValue = initValIn;
  GOTO_END_ON_ERROR(open_datafile_write0(vars));
  GOTO_END_ON_ERROR(check_input(&initialValue));
  GOTO_END_ON_ERROR(open_datafile_write1(vars));
  set_compress(vars);
  begin_local_nci(vars, initialValue);
  begin_extended_nci(vars);
  GOTO_END_ON_ERROR(begin_segment_header(vars,initialValue));
  begin_segment_index(vars);
  GOTO_END_ON_ERROR(begin_sinfo(vars,initialValue,check_compress_dim));
  GOTO_END_ON_ERROR(putdata_initialvalue(vars,initialValue));
  GOTO_END_ON_ERROR(putdim_dim(vars,start,end,dimension));
  TreeCallHookFun("TreeNidHook","MakeSegment",vars->tinfo->treenam,
		  vars->tinfo->shot,*vars->nid_ptr,NULL);
  SIGNAL(1) signal = {0, DTYPE_SIGNAL, CLASS_R, 0, 3, __fill_value__ \
		      (struct descriptor *)initValIn, NULL, {dimension}};
  TreeCallHookFun("TreeNidDataHook","MakeSegmentFull",vars->tinfo->treenam,
		  vars->tinfo->shot,*vars->nid_ptr,&signal,NULL);
  status = begin_finish(vars);
end: ;
  CLEANUP_NCI_POP;
  return status;
}

int _TreeMakeXNciSegment(void *dbid, int nid, const char *xnciname, struct descriptor *start, struct descriptor *end, struct descriptor *dimension, struct descriptor_a *initValIn, int idx, int rows_filled){
  INIT_WRITE_VARS;
  CLEANUP_NCI_PUSH;
  struct descriptor_a* initialValue = initValIn;
  GOTO_END_ON_ERROR(open_datafile_write0(vars));
  GOTO_END_ON_ERROR(check_input(&initialValue));
  GOTO_END_ON_ERROR(open_datafile_write1(vars));
  set_compress(vars);
  begin_local_nci(vars, initialValue);
  begin_extended_nci(vars);
  GOTO_END_ON_ERROR(begin_xnci_segment_header(vars,xnciname,initialValue));
  begin_segment_index(vars);
  GOTO_END_ON_ERROR(begin_sinfo(vars,initialValue,check_compress_dim));
  GOTO_END_ON_ERROR(putdata_initialvalue(vars,initialValue));
  GOTO_END_ON_ERROR(putdim_dim(vars,start,end,dimension));
  status = begin_finish(vars);
end: ;
  CLEANUP_NCI_POP;
  return status;
}

int _TreeMakeTimestampedSegment(void *dbid, int nid, int64_t * timestamps, struct descriptor_a *initValIn, int idx, int rows_filled){
  INIT_WRITE_VARS;
  CLEANUP_NCI_PUSH;
  struct descriptor_a* initialValue = initValIn;
  GOTO_END_ON_ERROR(open_datafile_write0(vars));
  GOTO_END_ON_ERROR(check_input(&initialValue));
  GOTO_END_ON_ERROR(open_datafile_write1(vars));
  set_compress(vars);
  begin_local_nci(vars, initialValue);
  begin_extended_nci(vars);
  GOTO_END_ON_ERROR(begin_segment_header(vars,initialValue));
  begin_segment_index(vars);
  GOTO_END_ON_ERROR(begin_sinfo(vars,initialValue,check_compress_ts));
  GOTO_END_ON_ERROR(putdata_initialvalue(vars,initialValue));
  GOTO_END_ON_ERROR(putdim_ts(vars,timestamps));
  TreeCallHookFun("TreeNidHook","MakeTimestampedSegment",vars->tinfo->treenam,
		  vars->tinfo->shot,*vars->nid_ptr,NULL);
  DESCRIPTOR_A(dimension, sizeof(int64_t), DTYPE_Q, timestamps, rows_filled * sizeof(int64_t));
  SIGNAL(1) signal = {0, DTYPE_SIGNAL, CLASS_R, 0, 3, __fill_value__ \
		      (struct descriptor *)initValIn, NULL, {(struct descriptor *)&dimension}};
  TreeCallHookFun("TreeNidDataHook","MakeTimestampedSegmentFull",vars->tinfo->treenam,
		  vars->tinfo->shot,*vars->nid_ptr,&signal,NULL);
  status = begin_finish(vars);
end: ;
  CLEANUP_NCI_POP;
  unlock_nci(vars);
  return status;
}

int _TreeSetRowsFilled(void *dbid, int nid, int rows_filled) {
  const int idx = -1;
  INIT_WRITE_VARS;
  CLEANUP_NCI_PUSH;
  GOTO_END_ON_ERROR(open_datafile_write0(vars));
  GOTO_END_ON_ERROR(open_datafile_write1(vars));
  IF_NO_EXTENDED_NCI   {status = TreeNOSEGMENTS;goto end;}
  IF_NO_SEGMENT_HEADER {status = TreeNOSEGMENTS;goto end;}
  IF_NO_SEGMENT_INDEX  {status = TreeFAILURE;goto end;}
  if (rows_filled < 0) {
    vars->sinfo = &vars->sindex.segment[vars->shead.idx % SEGMENTS_PER_INDEX];
    if (vars->sinfo->rows<0) // compressed
      status = get_compressed_segment_rows(vars->tinfo, vars->sinfo->data_offset, &vars->shead.next_row);
    else
      vars->shead.next_row = vars->sinfo->rows &0x7fffffff;
  } else
    vars->shead.next_row = rows_filled;
  if STATUS_OK
    status = put_segment_header(vars->tinfo, &vars->shead, &vars->attr.facility_offset[SEGMENTED_RECORD_FACILITY]);
end: ;
  CLEANUP_NCI_POP;
  return status;
}

int _TreeUpdateSegment(void *dbid, int nid, struct descriptor *start, struct descriptor *end, struct descriptor *dimension, int idx){
  INIT_VARS;vars->idx=idx;
  CLEANUP_NCI_PUSH;
  GOTO_END_ON_ERROR(open_datafile_write0(vars));
  GOTO_END_ON_ERROR(open_datafile_write1(vars));
  set_compress(vars);
  IF_NO_EXTENDED_NCI   {status = TreeNOSEGMENTS;goto end;}
  IF_NO_SEGMENT_HEADER {status = TreeNOSEGMENTS;goto end;}
  IF_NO_SEGMENT_INDEX  {status = TreeFAILURE;goto end;}
  GOTO_END_ON_ERROR(check_sinfo(vars));
  GOTO_END_ON_ERROR(putdim_dim(vars,start,end,dimension));
  TreeCallHookFun("TreeNidHook","UpdateSegment",vars->tinfo->treenam,
		  vars->tinfo->shot,*vars->nid_ptr,NULL);
  status = put_segment_index(vars->tinfo, &vars->sindex, &vars->index_offset);
end: ;
  CLEANUP_NCI_POP;
  return status;
}

int _TreePutSegment(void *dbid, int nid, const int startIdx, struct descriptor_a *data) {
  A_COEFF_TYPE *a_coeff = (A_COEFF_TYPE *) data;
  INIT_VARS;
  CLEANUP_NCI_PUSH;
  DESCRIPTOR_A(data_a, 0, 0, 0, 0);/*CHECK_DATA*/{
    while (data && data->dtype == DTYPE_DSC)
      data = (struct descriptor_a *)data->pointer;
    if (data->class == CLASS_S) {
      data_a.pointer = data->pointer;
      data_a.length = data->length;
      data_a.class = CLASS_A;
      data_a.dtype = data->dtype;
      data_a.arsize = data->length;
      data_a.dimct = 1;
      data = (struct descriptor_a *)&data_a;
    }
    if (data == NULL || data->class != CLASS_A || data->dimct < 1 || data->dimct > 8)
      {status = TreeINVDTPUSG; goto end;}
  }
  GOTO_END_ON_ERROR(open_datafile_write0(vars));
  GOTO_END_ON_ERROR(open_datafile_write1(vars));
  IF_NO_EXTENDED_NCI   {status = TreeNOSEGMENTS;goto end;}
  IF_NO_SEGMENT_HEADER {status = TreeNOSEGMENTS;goto end;}
  /*CHECK_DATA_DIMCT*/{
    if (data->dtype != vars->shead.dtype
    || (data->dimct != vars->shead.dimct && data->dimct != vars->shead.dimct - 1)
    || (data->dimct > 1 && memcmp(vars->shead.dims, a_coeff->m, (data->dimct - 1) * sizeof(int))))
      {status = TreeFAILURE;goto end;}
  }
  /*CHECK_STARTIDX*/
  const int start_idx = (startIdx == -1) ? vars->shead.next_row : startIdx;
  if (start_idx < 0 || start_idx >= vars->shead.dims[vars->shead.dimct - 1])
    {status = TreeBUFFEROVF;goto end;}
  /*CHECK_DATA_SIZE*/
  int bytes_per_row,i;
  for (bytes_per_row = vars->shead.length, i = 0; i < vars->shead.dimct - 1;
       bytes_per_row *= vars->shead.dims[i], i++) ;
  int rows_to_insert;
  if (data->dimct < vars->shead.dimct)
    rows_to_insert = 1;
  else if (data->dimct == 1)
    rows_to_insert = data->arsize / data->length;
  else
    rows_to_insert = a_coeff->m[a_coeff->dimct - 1];
  int remaining_rows_in_segment = vars->shead.dims[vars->shead.dimct - 1] - start_idx;
  // trunk to fit in current segment; TODO: dynamically increase number of segments
  rows_to_insert = rows_to_insert > remaining_rows_in_segment ? remaining_rows_in_segment : rows_to_insert;
  uint32_t bytes_to_insert = rows_to_insert * bytes_per_row;
  if (bytes_to_insert < data->arsize) // segment does not fit array size (shape?)
    {status = TreeBUFFEROVF;goto end;}
  int64_t offset = vars->shead.data_offset + start_idx * bytes_per_row;
  /*PUTSEG_PUTDATA*/{
    ALLOCATE_BUFFER(bytes_to_insert,buffer);
    CHECK_ENDIAN_TRANSFER(data->pointer,bytes_to_insert,vars->shead.length,data->dtype,buffer);
    status = write_property(vars->tinfo, &offset, buffer, bytes_to_insert);
    FREE_BUFFER(buffer);
  }
  if (start_idx == vars->shead.next_row)
    vars->shead.next_row += bytes_to_insert / bytes_per_row;
  if STATUS_OK {
    status = put_segment_header(vars->tinfo, &vars->shead, &vars->attr.facility_offset[SEGMENTED_RECORD_FACILITY]);
    TreeCallHookFun("TreeNidHook","PutSegment",vars->tinfo->treenam,
		    vars->tinfo->shot,*vars->nid_ptr,NULL);
    TreeCallHookFun("TreeNidDataHook","PutSegmentFull",vars->tinfo->treenam,
		    vars->tinfo->shot,*vars->nid_ptr,data,NULL);
    }
end: ;
  CLEANUP_NCI_POP;
  return status;
}

int _TreePutTimestampedSegment(void *dbid, int nid, int64_t * timestamp, struct descriptor_a *data_in){
  INIT_VARS;
  CLEANUP_NCI_PUSH;
  struct descriptor_a *data = data_in;
  GOTO_END_ON_ERROR(open_datafile_write0(vars));
  DESCRIPTOR_A(data_a, 0, 0, 0, 0);
  while (data && data->dtype == DTYPE_DSC)
    data = (struct descriptor_a *)data->pointer;
  if (data && data->class == CLASS_S) {
    data_a.pointer = data->pointer;
    data_a.length = data->length;
    data_a.class = CLASS_A;
    data_a.dtype = data->dtype;
    data_a.arsize = data->length;
    data_a.dimct = 1;
    data = (struct descriptor_a *)&data_a;
  }
  A_COEFF_TYPE *a_coeff = (A_COEFF_TYPE *) data;
  if (data == NULL || data->class != CLASS_A || data->dimct < 1 || data->dimct > 8)
    {status = TreeINVDTYPE; goto end;}
  GOTO_END_ON_ERROR(open_datafile_write1(vars));
  IF_NO_EXTENDED_NCI   {status = TreeNOSEGMENTS;goto end;};
  IF_NO_SEGMENT_HEADER {status = TreeNOSEGMENTS;goto end;};
  if (data->dtype != vars->shead.dtype)
    {status = TreeINVDTYPE; goto end;}
  /*CHECK_DATA_DIMCT*/{
    if (data->dtype != vars->shead.dtype
    || (data->dimct != vars->shead.dimct && data->dimct != vars->shead.dimct - 1)
    || (data->dimct > 1 && memcmp(vars->shead.dims, a_coeff->m, (data->dimct - 1) * sizeof(int))))
      {status = TreeFAILURE;goto end;}
  }
  int start_idx = vars->shead.next_row;
  int bytes_per_row,i;
  for (bytes_per_row = vars->shead.length, i = 0; i < vars->shead.dimct - 1;
       bytes_per_row *= vars->shead.dims[i], i++) ;
  int rows_to_insert;
  if (data->dimct < vars->shead.dimct)
    rows_to_insert = 1;
  else if (data->dimct == 1)
    rows_to_insert = data->arsize / data->length;
  else
    rows_to_insert = a_coeff->m[a_coeff->dimct - 1];
  int remaining_rows_in_segment = vars->shead.dims[vars->shead.dimct - 1] - start_idx;
  // trunk to fit in current segment; TODO: dynamically increase number of segments
  rows_to_insert = rows_to_insert > remaining_rows_in_segment ? remaining_rows_in_segment : rows_to_insert;
/*if STATUS_OK*/{
    int64_t  offset = vars->shead.data_offset + start_idx * bytes_per_row;
    uint32_t bytes_to_insert = rows_to_insert * bytes_per_row;
    if (bytes_to_insert < data->arsize) // segment does not fit array size (shape?)
      {status = TreeBUFFEROVF;goto end;}
    ALLOCATE_BUFFER(bytes_to_insert,buffer);
    CHECK_ENDIAN_TRANSFER(data->pointer,bytes_to_insert,vars->shead.length,data->dtype,buffer);
    status = write_property(vars->tinfo,&offset,buffer,bytes_to_insert);
    FREE_BUFFER(buffer);
  }
  if STATUS_OK {
    int64_t  offset = vars->shead.dim_offset + start_idx * sizeof(int64_t);
    uint32_t bytes_to_insert = rows_to_insert * sizeof(int64_t);
    ALLOCATE_BUFFER(bytes_to_insert,buffer);
    CHECK_ENDIAN_TRANSFER(timestamp,rows_to_insert,sizeof(int64_t),0,buffer);
    status = write_property(vars->tinfo,&offset,buffer,bytes_to_insert);
    FREE_BUFFER(buffer);
  }
  if STATUS_OK {
    vars->shead.next_row = start_idx + rows_to_insert;
    status = put_segment_header(vars->tinfo, &vars->shead, &vars->attr.facility_offset[SEGMENTED_RECORD_FACILITY]);
    TreeCallHookFun("TreeNidHook","PutTimestampedSegment",vars->tinfo->treenam, vars->tinfo->shot,*vars->nid_ptr,NULL);
    DESCRIPTOR_A(dimension, sizeof(int64_t), DTYPE_Q, timestamp, rows_to_insert * sizeof(int64_t));
    SIGNAL(1) signal = {0, DTYPE_SIGNAL, CLASS_R, 0, 3, __fill_value__ (struct descriptor *)data_in, NULL, {(struct descriptor *)&dimension}};
    TreeCallHookFun("TreeNidDataHook","PutTimestampedSegmentFull",vars->tinfo->treenam,vars->tinfo->shot,*vars->nid_ptr,&signal,NULL);
  }
end: ;
  CLEANUP_NCI_POP;
  return status;
}

///// GET SEGMENT TIMES /////

inline static void getlimit_array(vars_t* vars, int64_t* limitval, const int64_t sinfo_limit, const int64_t sinfo_limit_offset, const int sinfo_limit_length) {
  if (sinfo_limit != -1)
    *limitval = sinfo_limit;
  else if (sinfo_limit_offset > 0 && sinfo_limit_length > 0) {
    EMPTYXD(xd);
    if (TreeGetDsc(vars->tinfo, *(int*)vars->nid_ptr, sinfo_limit_offset, sinfo_limit_length, &xd)&1 && xd.pointer && xd.pointer->length == 8)
      *limitval = *(int64_t *) xd.pointer->pointer;
    else
      *limitval = 0;
    MdsFree1Dx(&xd, 0);
  } else
    *limitval = 0;
}

inline static void getlimit_xd(vars_t* vars, int64_t* limitval,const int64_t sinfo_limit, const int64_t sinfo_limit_offset, const int sinfo_limit_length,
	struct descriptor_xd* limit_xd,struct descriptor* limitdsc) {
  if (sinfo_limit != -1) {
    *limitval = sinfo_limit;
    MdsCopyDxXd(limitdsc,limit_xd);
  }
  if (sinfo_limit_offset > 0 && sinfo_limit_length > 0)
    TreeGetDsc(vars->tinfo, *(int*)vars->nid_ptr, sinfo_limit_offset, sinfo_limit_length, limit_xd);
}

inline static int get_filled_rows_ts(SEGMENT_HEADER* shead,SEGMENT_INFO* sinfo, const int idx, const int64_t *buffer){
  if (shead->idx==idx)
    return shead->next_row<0 ? sinfo->rows : shead->next_row;
  else {
    /* Removes trailing null timestamps from dimension and data
     * if at least more than one zero timestamps
     * or last timestamp does not continue monotonic increase of time vector
     */
    int filled_rows = sinfo->rows;
    if (filled_rows>1 && swapint64(&buffer[filled_rows-2])>=0)// if second last is not <0
      for ( ; filled_rows>0 && 0==buffer[filled_rows-1] ; filled_rows--);
    return filled_rows;
  }
}

inline static int get_segment_times_loop(vars_t* vars, int64_t* startval, int64_t* endval,
	struct descriptor_xd* start_xd,struct descriptor* startdsc,
	struct descriptor_xd* end_xd,  struct descriptor* enddsc) {
  INIT_TREESUCCESS;
  int index_idx = vars->idx % SEGMENTS_PER_INDEX;
  vars->sinfo = &vars->sindex.segment[index_idx];
  if (vars->sinfo->dimension_offset != -1 && vars->sinfo->dimension_length == 0) {
    /* It's a timestamped segment */
    if (vars->sinfo->rows < 0 || !(vars->sinfo->start == 0 && vars->sinfo->end == 0)) {
      /* Valid start and end in segment tinfo */
      *startval = vars->sinfo->start;
      *endval = vars->sinfo->end;
    } else {
      /* Current segment so use timestamps in segment */
      int length = sizeof(int64_t) * vars->sinfo->rows;
      char *buffer = NULL;
      FREE_ON_EXIT(buffer);
      buffer = malloc(length);
      status = read_property(vars->tinfo,vars->sinfo->dimension_offset, buffer, length);
      if STATUS_OK {
	loadint64(startval,buffer);
	int filled_rows = get_filled_rows_ts(&vars->shead,vars->sinfo,vars->idx,(int64_t*)buffer);
	if (filled_rows>0)
	  loadint64(endval,buffer+(filled_rows-1)*sizeof(int64_t));
	else *endval = 0;
      } else {
	*startval = 0;
	*endval = 0;
      }
      FREE_NOW(buffer);
    }
    if (start_xd) {
      MdsCopyDxXd(startdsc,start_xd);
      MdsCopyDxXd(enddsc,end_xd);
    }
  } else if (start_xd) {
    getlimit_xd(vars,startval,vars->sinfo->start,vars->sinfo->start_offset,vars->sinfo->start_length, start_xd, startdsc);
    getlimit_xd(vars,endval  ,vars->sinfo->end,  vars->sinfo->end_offset,  vars->sinfo->end_length,   end_xd  , enddsc  );
  } else {
    getlimit_array(vars,startval,vars->sinfo->start,vars->sinfo->start_offset,vars->sinfo->start_length);
    getlimit_array(vars,endval  ,vars->sinfo->end,  vars->sinfo->end_offset,  vars->sinfo->end_length);
  }
  if (index_idx == 0 && vars->idx > 0)
    return get_segment_index(vars->tinfo, vars->sindex.previous_offset, &vars->sindex);
  return TreeSUCCESS;
}

int _TreeGetSegmentTimesXd(void *dbid, int nid, int *nsegs, struct descriptor_xd *start_list, struct descriptor_xd *end_list){
  INIT_VARS;
  RETURN_IF_NOT_OK(open_index_read(vars));
  int numsegs = vars->shead.idx + 1;
  *nsegs = numsegs;
  DESCRIPTOR_APD(start_apd, DTYPE_LIST, malloc(numsegs * sizeof(void *)), numsegs);
  DESCRIPTOR_APD(end_apd,   DTYPE_LIST, malloc(numsegs * sizeof(void *)), numsegs);
  FREE_ON_EXIT(start_apd.pointer);
  FREE_ON_EXIT(end_apd.pointer);
  status = TreeSUCCESS;
  struct descriptor_xd **start_xds = (struct descriptor_xd **)start_apd.pointer;
  struct descriptor_xd **end_xds = (struct descriptor_xd **)end_apd.pointer;
  EMPTYXD(xd);
  int64_t startval;
  int64_t endval;
  struct descriptor startdsc = {sizeof(int64_t), DTYPE_Q, CLASS_S, (char *)&startval};
  struct descriptor enddsc = {sizeof(int64_t), DTYPE_Q, CLASS_S, (char *)&endval};
  for (vars->idx=numsegs ; STATUS_OK && vars->idx-->0 ;) {
    start_xds[vars->idx]= memcpy(malloc(sizeof(struct descriptor_xd)),&xd, sizeof(struct descriptor_xd));
    end_xds[vars->idx]  = memcpy(malloc(sizeof(struct descriptor_xd)),&xd, sizeof(struct descriptor_xd));
    struct descriptor_xd *start_xd = ((struct descriptor_xd **)start_apd.pointer)[vars->idx];
    struct descriptor_xd *end_xd   = ((struct descriptor_xd **)end_apd.pointer)[vars->idx];
    get_segment_times_loop(vars,&startval,&endval,start_xd,&startdsc,end_xd,&enddsc);
  }
  MdsCopyDxXd((struct descriptor *)&start_apd,start_list);
  MdsCopyDxXd((struct descriptor *)&end_apd,end_list);
  for (vars->idx=0; vars->idx<numsegs ; vars->idx++) {
    MdsFree1Dx(start_xds[vars->idx],0);
    free(start_xds[vars->idx]);
    MdsFree1Dx(end_xds[vars->idx],0);
    free(end_xds[vars->idx]);
  }
  FREE_NOW(end_apd.pointer);
  FREE_NOW(start_apd.pointer);
  return status;
}

int _TreeGetSegmentTimes(void *dbid, int nid, int *nsegs, int64_t ** times){
  *times = NULL;
  INIT_VARS;
  RETURN_IF_NOT_OK(open_index_read(vars));
  int numsegs = vars->shead.idx + 1;
  *nsegs = numsegs;
  int64_t *ans = (int64_t *) malloc(numsegs * 2 * sizeof(int64_t));
  *times = ans;
  memset(ans, 0, numsegs * 2 * sizeof(int64_t));
  for (vars->idx=numsegs; STATUS_OK && vars->idx-->0;)
    get_segment_times_loop(vars,&ans[vars->idx * 2],&ans[vars->idx * 2 + 1],0,0,0,0);
  return status;
}

int _TreeGetNumSegments(void *dbid, int nid, int *num){
  *num = 0;
  INIT_VARS;
  RETURN_IF_NOT_OK(open_datafile_read(vars));
  IF_NO_EXTENDED_NCI   return status; //return num=0
  IF_NO_SEGMENT_HEADER return status; //return num=0
  *num = _vars.shead.idx + 1;
  return status;
}

static int (*_TdiExecute) () = NULL;
static int (*_TdiCompile) () = NULL;
/* checks last segment and trims it down to last written row if necessary */
static int trim_last_segment(void* dbid, struct descriptor_xd *dim, int filled_rows){
  INIT_TREESUCCESS;
    mdsdsc_t *tmp = dim->pointer;
    while (tmp && tmp->class == CLASS_R && tmp->dtype != DTYPE_WINDOW && ((mds_function_t*)tmp)->ndesc>0)
      tmp = ((mds_function_t*)tmp)->arguments[0];
    if (!tmp) goto fallback;
    mdsdsc_s_t *begin, *end;
    if (tmp->class == CLASS_R && tmp->dtype == DTYPE_WINDOW
     && (begin = (mdsdsc_s_t*)((mds_function_t*)tmp)->arguments[0])->class == CLASS_S
     && (end   = (mdsdsc_s_t*)((mds_function_t*)tmp)->arguments[1])->class == CLASS_S) {
      int64_t begin_i;
      double  begin_d;
      switch(begin->dtype){
	case DTYPE_B:
	case DTYPE_BU:
	  begin_d = begin_i = *(int8_t *)begin->pointer;break;
	case DTYPE_W:
	case DTYPE_WU:
	  begin_d = begin_i = *(int16_t*)begin->pointer;break;
	case DTYPE_L:
	case DTYPE_LU:
	  begin_d = begin_i = *(int32_t*)begin->pointer;break;
	case DTYPE_Q:
	case DTYPE_QU:
	  begin_d = begin_i = *(int64_t*)begin->pointer;break;
	case DTYPE_FS:
	  begin_i = begin_d = *(float  *)begin->pointer;break;
	case DTYPE_FT:
	  begin_i = begin_d = *(double *)begin->pointer;break;
	default:
	  goto fallback;
      }
      switch(end->dtype){
	case DTYPE_B:
	case DTYPE_BU:
	  *(int8_t *)end->pointer = (int8_t )(begin_i+filled_rows-1);break;
	case DTYPE_W:
	case DTYPE_WU:
	  *(int16_t*)end->pointer = (int16_t)(begin_i+filled_rows-1);break;
	case DTYPE_L:
	case DTYPE_LU:
	  *(int32_t*)end->pointer = (int32_t)(begin_i+filled_rows-1);break;
	case DTYPE_Q:
	case DTYPE_QU:
	  *(int64_t*)end->pointer =          (begin_i+filled_rows-1);break;
	case DTYPE_FS:
	  *(float  *)end->pointer = (float  )(begin_i+filled_rows-1);break;
	case DTYPE_FT:
	  *(double *)end->pointer =          (begin_d+filled_rows-1);break;
	default:
	  goto fallback;
      }
      return status;
    }
    if (tmp->class == CLASS_A) {
      if (((mdsdsc_a_t*)tmp)->aflags.coeff)
	((array_coeff*)tmp)->m[0] = filled_rows;
      else
	((mdsdsc_a_t*)tmp)->arsize = tmp->length * filled_rows;
      return status;
    }
fallback: ;
    status = LibFindImageSymbol_C("TdiShr", "_TdiCompile", &_TdiCompile);
    if STATUS_OK {
      STATIC_CONSTANT DESCRIPTOR(expression, "execute('$1[$2 : $2+$3-1]',$1,lbound($1,-1),$2)");
      DESCRIPTOR_LONG(row_d, &filled_rows);
      status = _TdiCompile(&dbid,&expression,dim,&row_d,dim MDS_END_ARG);
    }
  return status;
}
/* read segmet data and dim, perform trim operation iff dbid is provided
 */
static int read_segment(void* dbid, TREE_INFO * tinfo, int nid, SEGMENT_HEADER* shead, SEGMENT_INFO* sinfo, int idx, struct descriptor_xd *segment, struct descriptor_xd *dim){
  if (sinfo->data_offset == -1) return TreeFAILURE;//copy_segment_index expects TreeFAILURE;
  INIT_TREESUCCESS;
  int filled_rows;
  int compressed_segment = sinfo->rows < 0;
  int rows;
  if (compressed_segment) {
    if IS_NOT_OK(get_compressed_segment_rows(tinfo, sinfo->data_offset, &rows))
      rows = 1;
  } else
    rows = sinfo->rows;
  if (sinfo->dimension_offset != -1 && sinfo->dimension_length == 0) {
    // this is a timestamped segment node, i.e. dim is array of int64_t
    DESCRIPTOR_A(ans, 8, DTYPE_Q, 0, 0);
    ans.arsize = rows * sizeof(int64_t);
    void *ans_ptr = ans.pointer = malloc(ans.arsize);
    status = read_property(tinfo,sinfo->dimension_offset, ans.pointer, (ssize_t)ans.arsize);
    CHECK_ENDIAN(ans.pointer,ans.arsize,sizeof(int64_t),0);
    if (dbid) {
      if (idx == shead->idx)
	filled_rows = shead->next_row;
      else
	filled_rows = get_filled_rows_ts(shead,sinfo,idx,(int64_t*)ans.pointer);
      if (dim) {
	ans.arsize = filled_rows * sizeof(int64_t);
	if (ans.arsize==0) ans.pointer = NULL;
	MdsCopyDxXd((struct descriptor *)&ans, dim);
      }
    } else filled_rows = rows;
    free(ans_ptr);
  } else if (sinfo->dimension_length != -1) {
    filled_rows = (idx == shead->idx) ? shead->next_row : rows;
    if (dim) {
      status = TreeGetDsc(tinfo, nid, sinfo->dimension_offset, sinfo->dimension_length, dim);
      if (STATUS_OK && dbid && dim->pointer && idx == shead->idx && shead->next_row != rows)
	status = trim_last_segment(dbid,dim,filled_rows);
    }
  } else { // no dim is stored, set filled_rows to next_row or full
    filled_rows = (idx == shead->idx) ? shead->next_row : rows;
    if (!segment) segment = dim; // if segment is not requested, read it as dim
  }
  if (STATUS_OK && segment){
    if (compressed_segment) {
      int data_length = sinfo->rows & 0x7fffffff;
      status = TreeGetDsc(tinfo, nid, sinfo->data_offset, data_length, segment);
    } else {
      DESCRIPTOR_A_COEFF(ans, shead->length, shead->dtype, NULL, 8, 0);
      ans.dimct = shead->dimct;
      memcpy(ans.m, shead->dims, sizeof(shead->dims));
      ans.m[shead->dimct - 1] = filled_rows;
      int i;
      ans.arsize = ans.length;
      for (i = 0; i < ans.dimct; i++)
	ans.arsize *= ans.m[i];
      void *ans_ptr = ans.pointer = ans.a0 = malloc(ans.arsize);
      status = read_property(tinfo,sinfo->data_offset, ans.pointer, (ssize_t)ans.arsize);
      if STATUS_OK {
	CHECK_ENDIAN(ans.pointer,ans.arsize,ans.length,ans.dtype);
	if (ans.arsize==0) ans.pointer = NULL;
	MdsCopyDxXd((struct descriptor *)&ans, segment);
      }
      free(ans_ptr);
    }
  }
  if (STATUS_OK && dim && dim != segment && (sinfo->dimension_offset == -1 && sinfo->dimension_length == -1)) {
    // dim is requested, but no dim is stored. segment was also requested, so we make a copy of segment.
    MdsCopyDxXd((struct descriptor *)segment, dim);
  }
  return status;
}

static int get_segment_limits(vars_t* vars, struct descriptor_xd *retStart, struct descriptor_xd *retEnd){
  INIT_TREESUCCESS;
  int64_t timestamp=0;
  struct descriptor q_d = { 8, DTYPE_Q, CLASS_S, (char *)&timestamp };
  if (vars->sinfo->dimension_offset != -1 && vars->sinfo->dimension_length == 0) {
    /*** timestamped segments ****/
    if (vars->sinfo->rows < 0 || !(vars->sinfo->start == 0 && vars->sinfo->end == 0)) {
      if (retStart) {
	timestamp = vars->sinfo->start;
	MdsCopyDxXd(&q_d, retStart);
      }
      if (retEnd) {
	timestamp = vars->sinfo->end;
	MdsCopyDxXd(&q_d, retEnd);
      }
    } else {
      char *buffer = NULL;
      FREE_ON_EXIT(buffer);
      const int length = sizeof(int64_t) * vars->sinfo->rows;
      buffer = malloc(length);
      status = read_property(vars->tinfo,vars->sinfo->dimension_offset, buffer, length);
      if STATUS_OK {
	if (retStart) {
	  loadint64(&timestamp,buffer);
	  MdsCopyDxXd(&q_d, retStart);
	}
	if (retEnd) {
	  const int filled_rows = get_filled_rows_ts(&vars->shead,vars->sinfo,vars->idx,(int64_t*)buffer);
	  if (filled_rows > 0) {
	    loadint64(&timestamp,buffer + (filled_rows-1) * sizeof(int64_t));
	    MdsCopyDxXd(&q_d, retEnd);
	  } else
	    MdsFree1Dx(retEnd, 0);
	}
      } else {
	if (retStart) MdsFree1Dx(retStart, 0);
	if (retEnd)   MdsFree1Dx(retEnd, 0);
      }
      FREE_NOW(buffer);
    }
  } else {
    if (retStart) {
      if (vars->sinfo->start != -1) {
	timestamp = vars->sinfo->start;
	MdsCopyDxXd(&q_d, retStart);
      } else if (vars->sinfo->start_length > 0 && vars->sinfo->start_offset > 0)
	status = TreeGetDsc(vars->tinfo, *(int*)vars->nid_ptr, vars->sinfo->start_offset, vars->sinfo->start_length, retStart);
      else
	status = MdsFree1Dx(retStart, 0);
    }
    if (retEnd) {
      if (vars->sinfo->end != -1) {
	timestamp = vars->sinfo->end;
	MdsCopyDxXd(&q_d, retEnd);
      } else if (vars->sinfo->end_length > 0 && vars->sinfo->end_offset > 0)
	status = TreeGetDsc(vars->tinfo, *(int*)vars->nid_ptr, vars->sinfo->end_offset, vars->sinfo->end_length, retEnd);
      else
	status = MdsFree1Dx(retEnd, 0);
    }
  }
  return status;
}

int _TreeSetXNci(void *dbid, int nid, const char *xnciname, struct descriptor *value){
  if (!xnciname || strlen(xnciname) < 1 || strlen(xnciname) > NAMED_ATTRIBUTE_NAME_SIZE)
    return TreeFAILURE;
  INIT_VARS;
  CLEANUP_NCI_PUSH;
  GOTO_END_ON_ERROR(open_datafile_write0(vars));
  GOTO_END_ON_ERROR(open_datafile_write1(vars));
  set_compress(vars);
  vars->index_offset = -1;
  int value_length = 0;
  int64_t value_offset = -1;
  NAMED_ATTRIBUTES_INDEX index, current_index;
  if (value) // NULL means delete
    GOTO_END_ON_ERROR(TreePutDsc(vars->tinfo, *(int*)vars->nid_ptr, value, &value_offset, &value_length, vars->compress));
  /*** See if node is currently using the Extended Nci feature and if so get the current contents of the attr
       index. If not, make an empty index and flag that a new index needs to be written.***/
  IF_NO_EXTENDED_NCI {
    if (!value) goto end; // has not xnci; nothing to delete
    vars->attr_offset=-1;
    memset(&vars->attr, -1, sizeof(vars->attr));
    vars->attr_update = 1;
    if (((vars->local_nci.flags2 & NciM_EXTENDED_NCI) == 0) && vars->local_nci.length > 0) {
      if (vars->local_nci.flags2 & NciM_DATA_IN_ATT_BLOCK) {
	EMPTYXD(dsc);
	struct descriptor *dptr;
	dtype_t dsc_dtype = DTYPE_DSC;
	length_t dlen = vars->local_nci.length - 8;
	l_length_t ddlen = dlen + sizeof(struct descriptor);
	status = MdsGet1Dx(&ddlen, &dsc_dtype, &dsc, 0);
	dptr = dsc.pointer;
	dptr->length = dlen;
	dptr->dtype = vars->local_nci.dtype;
	dptr->class = CLASS_S;
	dptr->pointer = (char *)dptr + sizeof(struct descriptor);
	memcpy(dptr->pointer, vars->local_nci.DATA_INFO.DATA_IN_RECORD.data, dptr->length);
	if (dptr->dtype != DTYPE_T) {
	  switch (dptr->length) {
	  case 2:
	    *(int16_t*)dptr->pointer = swapint16(dptr->pointer);
	    break;
	  case 4:
	    *(int32_t*)dptr->pointer = swapint32(dptr->pointer);
	    break;
	  case 8:
	    *(int64_t*)dptr->pointer = swapint64(dptr->pointer);
	    break;
	  }
	}
	TreePutDsc(vars->tinfo, *(int*)vars->nid_ptr, dptr, &vars->attr.facility_offset[STANDARD_RECORD_FACILITY],
	           &vars->attr.facility_length[STANDARD_RECORD_FACILITY], vars->compress);
	vars->local_nci.flags2 &= ~NciM_DATA_IN_ATT_BLOCK;
      } else {
	EMPTYXD(xd);
	int retsize;
	int nodenum;
	int length = vars->local_nci.DATA_INFO.DATA_LOCATION.record_length;
	if (length > 0) {
	  char *data = NULL;
	  FREE_ON_EXIT(data);
	  data = malloc(length);
	  status =
	      TreeGetDatafile(vars->tinfo, vars->local_nci.DATA_INFO.DATA_LOCATION.rfa, &length, data,
	                      &retsize, &nodenum, vars->local_nci.flags2);
	  if STATUS_NOT_OK
	    status = TreeBADRECORD;
	  else if (!(vars->local_nci.flags2 & NciM_NON_VMS)
	           && ((retsize != length) || (nodenum != vars->nidx)))
	    status = TreeBADRECORD;
	  else
	    status = (MdsSerializeDscIn(data, &xd) & 1) ? TreeNORMAL : TreeBADRECORD;
	  FREE_NOW(data);
	  if STATUS_OK {
	    status =
	        TreePutDsc(vars->tinfo, *(int*)vars->nid_ptr, (struct descriptor *)&xd,
	                   &vars->attr.facility_offset[STANDARD_RECORD_FACILITY],
	                   &vars->attr.facility_length[STANDARD_RECORD_FACILITY], vars->compress);
	  }
	  MdsFree1Dx(&xd, 0);
	}
	if (length <= 0 || STATUS_NOT_OK) {
	  vars->attr.facility_offset[STANDARD_RECORD_FACILITY] = 0;
	  vars->attr.facility_length[STANDARD_RECORD_FACILITY] = 0;
	  vars->local_nci.length = 0;
	  vars->local_nci.DATA_INFO.DATA_LOCATION.record_length = 0;
	}
      }
    }
  } else
    vars->attr_offset = RfaToSeek(vars->local_nci.DATA_INFO.DATA_LOCATION.rfa);
  /* See if the node currently has an named attr header record.
   * If not, make an empty named attr header and flag that a new one needs to be written.
   */
  if (vars->attr.facility_offset[NAMED_ATTRIBUTES_FACILITY] == -1
   || IS_NOT_OK(get_named_attributes_index(vars->tinfo, vars->attr.facility_offset[NAMED_ATTRIBUTES_FACILITY], &index))) {
    memset(&index, 0, sizeof(index));
    vars->attr.facility_offset[NAMED_ATTRIBUTES_FACILITY] = -1;
    vars->index_offset = -1;
    index.previous_offset = -1;
    vars->attr_update = 1;
  } else
    vars->index_offset = vars->attr.facility_offset[NAMED_ATTRIBUTES_FACILITY];
  current_index = index;
  /*** See if the node currently has a value for this attribute. ***/
  int found_index = -1;
  while (vars->index_offset != -1 && found_index == -1) {
    int i;
    size_t j;
    for (i = 0; i < NAMED_ATTRIBUTES_PER_INDEX; i++) {
      size_t len = strlen(xnciname);
      for (j = 0; j < len; j++)
	if (tolower(xnciname[j]) != tolower(index.attribute[i].name[j]))
	  break;
      if (j == len && index.attribute[i].name[j] == 0)
	break;
    }
    if (i < NAMED_ATTRIBUTES_PER_INDEX)
      found_index = i;
    else if (index.previous_offset != -1) {
      int64_t new_offset = index.previous_offset;
      if ((get_named_attributes_index(vars->tinfo, index.previous_offset, &index) & 1) == 0)
	break;
      vars->index_offset = new_offset;
    } else
      break;
  }
  /*** If name exists just replace the value else find an empty slot ***/
  if (found_index != -1) {
    index.attribute[found_index].offset = value_offset;
    index.attribute[found_index].length = value_length;
  } else {
    if (!value) goto end; // nothing to delete
    int i;
    index = current_index;
    for (i = 0; i < NAMED_ATTRIBUTES_PER_INDEX; i++) {
      if (index.attribute[i].name[0] == 0) {
	strcpy(index.attribute[i].name, xnciname);
	index.attribute[i].offset = value_offset;
	index.attribute[i].length = value_length;
	vars->index_offset = vars->attr.facility_offset[NAMED_ATTRIBUTES_FACILITY];
	break;
      }
    }
    if (i == NAMED_ATTRIBUTES_PER_INDEX) {
      memset(&index, 0, sizeof(index));
      index.previous_offset = vars->attr.facility_offset[NAMED_ATTRIBUTES_FACILITY];
      vars->attr_update = 1;
      strcpy(index.attribute[0].name, xnciname);
      index.attribute[0].offset = value_offset;
      index.attribute[0].length = value_length;
      vars->index_offset = -1;
    }
  }
  status = put_named_attributes_index(vars->tinfo, &index, &vars->index_offset);
  if (STATUS_OK && vars->attr_update) {
    vars->attr.facility_offset[NAMED_ATTRIBUTES_FACILITY] = vars->index_offset;
    status = TreePutExtendedAttributes(vars->tinfo, &vars->attr, &vars->attr_offset);
    SeekToRfa(vars->attr_offset, vars->local_nci.DATA_INFO.DATA_LOCATION.rfa);
    vars->local_nci.flags2 |= NciM_EXTENDED_NCI;
    TreePutNci(vars->tinfo, vars->nidx, &vars->local_nci, 0);
  }
end: ;
  CLEANUP_NCI_POP;
  return status;
}


int _TreeGetXNci(void *dbid, int nid, const char *xnciname, struct descriptor_xd *value) {
  INIT_VARS;
  int64_t offset;
  int length = 0;
  status = get_xnci_info(vars, xnciname, &length, &offset, value);
  if (status & 1 && length > 0) {
    status =TreeGetDsc(vars->tinfo, *(int *)vars->nid_ptr, offset, length, value);
    //  } else {
    //get signal from segments resampled by time context
  }
  return status;
}

int TreeSetRetrievalQuota(int quota __attribute__ ((unused)))
{
  printf("TreeSetRetrievalQuota is not implemented\n");
  return 0;
}

extern int TreeFixupNid();

int TreePutDsc(TREE_INFO * tinfo, int nid_in, struct descriptor *dsc, int64_t * offset, int *length, int compress)
{
  EMPTYXD(xd);
  int compressible;
  l_length_t ddlen, reclen;
  dtype_t dtype;
  class_t class;
  int data_in_altbuf;
  NID *nid = (NID *) & nid_in;
  unsigned char tree = nid->tree;
  int status = MdsSerializeDscOutZ(dsc, &xd, TreeFixupNid, &tree, 0, 0, compress,
	                           &compressible, &ddlen, &reclen, &dtype, &class, 0, 0,
	                           &data_in_altbuf);
  if (STATUS_OK && xd.pointer && xd.pointer->class == CLASS_A && xd.pointer->pointer) {
    struct descriptor_a *ap = (struct descriptor_a *)xd.pointer;
    *offset = -1;
    status  = write_property(tinfo, offset, ap->pointer, ap->arsize);
    *length = ap->arsize;
    MdsFree1Dx(&xd, 0);
  }
  return status;
}

int TreeGetDsc(TREE_INFO * tinfo, const int nid, int64_t offset, int length, struct descriptor_xd *dsc){
  INIT_TREESUCCESS;
  char *buffer = malloc(length);
  status = read_property_safe(tinfo,offset, buffer, length);
  if STATUS_OK
    status = MdsSerializeDscIn(buffer, dsc);
  if (dsc->pointer)
    status = TreeMakeNidsLocal(dsc->pointer, nid);
  free(buffer);
  return status;
}

static int data_copy(TREE_INFO * tinfo_in, TREE_INFO * tinfo_out, int64_t offset_in, int length_in, int64_t * offset_out){
  INIT_TREESUCCESS;
  *offset_out = -1;
  if (offset_in != -1 && length_in >= 0) {
    char *data = malloc(length_in);
    status = read_property(tinfo_in, offset_in, data, length_in);
    if STATUS_OK
      status = write_property(tinfo_out, offset_out, data, length_in);
    free(data);
    if STATUS_NOT_OK
      *offset_out = -1;
  }
  return status;
}

static int copy_segment(TREE_INFO *tinfo_in, TREE_INFO *tinfo_out, int nid, SEGMENT_HEADER *header, SEGMENT_INFO *sinfo, int idx, int compress) {
  INIT_TREESUCCESS;
  if (compress) {
    int length;
    EMPTYXD(xd);
    status = read_segment(NULL,tinfo_in, nid, header, sinfo, idx, &xd, NULL);
    if STATUS_OK {
      status = TreePutDsc(tinfo_out, nid, xd.pointer, &sinfo->data_offset, &length, compress);
      if STATUS_OK {
	if (sinfo->dimension_offset != -1 || sinfo->dimension_length != -1) {
	  // dim is present
	  status = read_segment(NULL,tinfo_in, nid, header, sinfo, idx, NULL, &xd);
	  if STATUS_OK
	    status = TreePutDsc(tinfo_out, nid, xd.pointer, &sinfo->dimension_offset, &sinfo->dimension_length, compress);
	}
	if (idx == header->idx) // finalize last segment
	  header->dims[header->dimct - 1] = header->next_row;
	sinfo->rows = length | 0x80000000;
      }
      MdsFree1Dx(&xd,NULL);
    }
  } else {
    int length;
    if (sinfo->data_offset != -1 && sinfo->rows < 0) {
      length = sinfo->rows & 0x7fffffff;
      status = data_copy(tinfo_in, tinfo_out, sinfo->data_offset, length, &sinfo->data_offset);
      if STATUS_OK
	status = data_copy(tinfo_in, tinfo_out, sinfo->dimension_offset, sinfo->dimension_length,
	                  &sinfo->dimension_offset);
    } else {
      int rowlen = header->length, i;
      for (i = 0; i < header->dimct - 1; i++)
	rowlen = rowlen * header->dims[i];
      if (sinfo->dimension_length == 0)
	length = sinfo->rows * sizeof(int64_t);
      else
	length = sinfo->dimension_length;
      status = data_copy(tinfo_in, tinfo_out, sinfo->dimension_offset, length, &sinfo->dimension_offset);
      if STATUS_OK {
	length = rowlen * sinfo->rows;
	status = data_copy(tinfo_in, tinfo_out, sinfo->data_offset, length, &sinfo->data_offset);
      }
    }
  }
  if (STATUS_OK && (sinfo->start_offset > 0)) {
    status = data_copy(tinfo_in, tinfo_out, sinfo->start_offset, sinfo->start_length, &sinfo->start_offset);
    if(STATUS_OK && sinfo->end_offset > 0)
	status = data_copy(tinfo_in, tinfo_out, sinfo->end_offset, sinfo->end_length, &sinfo->end_offset);
  }
  return status;
}

static inline int copy_segment_index(TREE_INFO * tinfo_in, TREE_INFO * tinfo_out, int nid, SEGMENT_HEADER * shead, int compress){
  int status;
  INIT_AND_FREE_ON_EXIT(int64_t*,offsets);
  status = TreeSUCCESS;
  int64_t offset = shead->index_offset;
  int i,ioff, noff = ((shead->idx-1) / SEGMENTS_PER_INDEX) + 1;
  // First read all offsets as they have to be read last-one-first.
  offsets = malloc(noff*sizeof(int64_t));
  for (ioff = noff ; ioff --> 0 && STATUS_OK;) {
    offsets[ioff] = offset;
    status = read_property(tinfo_in, offset, (char*)&offset, sizeof(offset));
    offset = swapint64(&offset);
    if (offsets[ioff]==offset) {
      fprintf(stderr, "segment data malformed: offset[i]==offset[i+1]\n");
      status = MDSplusFATAL;
      break;
    }
  }
  shead->index_offset = -1;
  for (ioff = 0; ioff < noff && STATUS_OK ; ioff++) {
    SEGMENT_INDEX sindex;
    status = get_segment_index(tinfo_in, offsets[ioff], &sindex);
    for (i = 0; (i < SEGMENTS_PER_INDEX) && STATUS_OK; i++)
      status = copy_segment(tinfo_in, tinfo_out, nid, shead, &sindex.segment[i], i, compress);
    // status is not ok if sindex is not full in last sindex
    sindex.previous_offset = shead->index_offset;shead->index_offset = -1; // append
    status = put_segment_index(tinfo_out, &sindex, &shead->index_offset);
  }
  FREE_NOW(offsets);
  return status;
}

static int copy_named_attributes(TREE_INFO * tinfo_in, TREE_INFO * tinfo_out, int nid, int64_t * offset, int *length, int compress){
  EMPTYXD(xd);
  NAMED_ATTRIBUTES_INDEX index;
  int status = get_named_attributes_index(tinfo_in, *offset, &index);
  if STATUS_OK {
    int i;
    *length=0;
    if (index.previous_offset != -1) //TODO: avoid recursion due to stacklimit => can be inline
      copy_named_attributes(tinfo_in, tinfo_out, nid, &index.previous_offset, 0, compress);
    for (i = 0; i < NAMED_ATTRIBUTES_PER_INDEX; i++) {
      if (index.attribute[i].name[0] != '\0' && index.attribute[i].offset != -1) {
	status = TreeGetDsc(tinfo_in, nid, index.attribute[i].offset, index.attribute[i].length, &xd);
	if STATUS_OK {
	  status = TreePutDsc(tinfo_out, nid, (struct descriptor *)&xd,
	                      &index.attribute[i].offset, &index.attribute[i].length, compress);
	  if STATUS_NOT_OK {
	    memset(index.attribute[i].name, 0, sizeof(index.attribute[i].name));
	    index.attribute[i].offset = -1;
	  }
	  MdsFree1Dx(&xd, 0);
	}
      }
    }
    *offset = -1;
    status = put_named_attributes_index(tinfo_out, &index, offset);
  } else
    *offset = -1;
  return status;
}

inline static int copy_segmented_records(TREE_INFO * tinfo_in, TREE_INFO * tinfo_out, int nid, int64_t * offset, int *length, int compress){
  SEGMENT_HEADER header;
  INIT_STATUS_AS get_segment_header(tinfo_in, *offset, &header);
  if STATUS_OK {
    *length=0;
    status = copy_segment_index(tinfo_in, tinfo_out, nid, &header, compress);
    *offset = -1;
    status = put_segment_header(tinfo_out, &header, offset);
  }
  return status;
}

inline static int copy_standard_record(TREE_INFO * tinfo_in, TREE_INFO * tinfo_out, int nid, int64_t * offset, int *length, int compress){
  int status;
  INIT_AND_FREEXD_ON_EXIT(xd);
  status = TreeGetDsc(tinfo_in, nid, *offset, *length, &xd);
  if STATUS_OK
    status = TreePutDsc(tinfo_out, nid, (struct descriptor *)&xd, offset, length, compress);
  FREEXD_NOW(xd);
  if STATUS_NOT_OK {
    *offset = -1;
    *length = 0;
  }
  return status;
}

int TreeCopyExtended(PINO_DATABASE * dbid_in, PINO_DATABASE * dbid_out, int nid, NCI * nci, int compress){
  EXTENDED_ATTRIBUTES attr;
  TREE_INFO *tinfo_in = dbid_in->tree_info, *tinfo_out = dbid_out->tree_info;
  int64_t now = -1;
  int64_t offset = -1;
  INIT_STATUS_AS TreeGetExtendedAttributes(tinfo_in, RfaToSeek(nci->DATA_INFO.DATA_LOCATION.rfa), &attr);
  if STATUS_OK {
    if (attr.facility_offset[NAMED_ATTRIBUTES_FACILITY] != -1)
      copy_named_attributes(tinfo_in, tinfo_out, nid,
	                  &attr.facility_offset[NAMED_ATTRIBUTES_FACILITY],
	                  &attr.facility_length[NAMED_ATTRIBUTES_FACILITY], compress);
    if (attr.facility_offset[SEGMENTED_RECORD_FACILITY] != -1)
      copy_segmented_records(tinfo_in, tinfo_out, nid,
	                   &attr.facility_offset[SEGMENTED_RECORD_FACILITY],
	                   &attr.facility_length[SEGMENTED_RECORD_FACILITY], compress);
    if (attr.facility_offset[STANDARD_RECORD_FACILITY] != -1)
      copy_standard_record(tinfo_in, tinfo_out, nid,
	                 &attr.facility_offset[STANDARD_RECORD_FACILITY],
	                 &attr.facility_length[STANDARD_RECORD_FACILITY], compress);
    status = TreePutExtendedAttributes(tinfo_out, &attr, &offset);
    if STATUS_OK {
      SeekToRfa(offset, nci->DATA_INFO.DATA_LOCATION.rfa);
      status = TreePutNci(tinfo_out, nid, nci, 0);
    }
    TreeSetViewDate(&now);
  }
  return status;
}


/*****************************************
 TimeContext sticks with current db (tree)
 *****************************************/
int _TreeSetTimeContext(void *dbid, struct descriptor *start, struct descriptor *end, struct descriptor *delta){
  timecontext_t* tc = &((PINO_DATABASE*)dbid)->timecontext;
  INIT_STATUS_AS MdsCopyDxXd(start, &tc->start);
  if STATUS_OK {
    status = MdsCopyDxXd(end, &tc->end);
    if STATUS_OK
      status = MdsCopyDxXd(delta, &tc->delta);
  }
  return status;
}

int TreeSetTimeContext(struct descriptor *start, struct descriptor *end, struct descriptor *delta){
  return _TreeSetTimeContext(*TreeCtx(), start, end, delta);
}

int _TreeGetTimeContext(void *dbid, struct descriptor_xd *start, struct descriptor_xd *end, struct descriptor_xd *delta){
  timecontext_t* tc = &((PINO_DATABASE*)dbid)->timecontext;
  INIT_STATUS_AS 1;
  if (start) RETURN_IF_NOT_OK(MdsCopyDxXd(tc->start.pointer,start));
  if (end  ) RETURN_IF_NOT_OK(MdsCopyDxXd(tc->end.pointer  ,end  ));
  if (delta) RETURN_IF_NOT_OK(MdsCopyDxXd(tc->delta.pointer,delta));
  return status;
}
int TreeGetTimeContext(struct descriptor_xd *start, struct descriptor_xd *end, struct descriptor_xd *delta){
  return _TreeGetTimeContext(*TreeCtx(), start, end, delta);
}

inline static int get_opaque_list(void *dbid, int nid, struct descriptor_xd *out) {
  {
    unsigned char data_type;
    NCI_ITM itmlst[] = { {1, NciDTYPE, &data_type, 0},{0, NciEND_OF_LIST, 0, 0}};
    int status = _TreeGetNci(dbid, nid, itmlst);
    if STATUS_NOT_OK return status;
    if (data_type != DTYPE_OPAQUE) return 0;
  }
  INIT_VARS;
  RETURN_IF_NOT_OK(open_header_read(vars));
  int numsegs = vars->shead.idx + 1;
  struct descriptor **dptr = calloc(sizeof(struct descriptor *) , numsegs);
  DESCRIPTOR_APD(apd, DTYPE_LIST, dptr, numsegs);
  status = get_segment_index(vars->tinfo, vars->shead.index_offset, &vars->sindex);
  int idx;
  for (idx = numsegs; STATUS_OK && idx-- > 0;) {
    while (STATUS_OK && idx < vars->sindex.first_idx && vars->sindex.previous_offset > 0)
      status = get_segment_index(vars->tinfo, vars->sindex.previous_offset, &vars->sindex);
    if STATUS_NOT_OK break;
    vars->sinfo = &vars->sindex.segment[idx % SEGMENTS_PER_INDEX];
    EMPTYXD(segment);
    status = read_segment(NULL,vars->tinfo, *(int*)vars->nid_ptr, &vars->shead, vars->sinfo, idx, &segment, NULL);
    if STATUS_OK {
      dptr[idx] = malloc(sizeof(struct descriptor_xd));
      memcpy(dptr[idx], &segment, sizeof(struct descriptor_xd));
      segment.pointer = NULL;
    } else
      MdsFree1Dx(&segment, 0);
  }
  if STATUS_OK status = MdsCopyDxXd((struct descriptor *)&apd, out);
  for (idx=0 ; idx < numsegs; idx++) {
    if (dptr[idx]) {
      MdsFree1Dx((struct descriptor_xd *)dptr[idx], 0);
      free(dptr[idx]);
    }
  }
  free(dptr);
  return status;
}

int _TreeGetSegmentedRecord(void *dbid, int nid, struct descriptor_xd *data)
{
  int status = get_opaque_list(dbid, nid, data );
  if (status) return status; // 0: data is not Opaque
  static int (*_XTreeGetTimedRecord) () = NULL;
  status = LibFindImageSymbol_C("XTreeShr", "_XTreeGetTimedRecord", &_XTreeGetTimedRecord);
  if STATUS_NOT_OK {
    fprintf(stderr, "Error activating XTreeShr library. Cannot access segmented records.\n");
    return status;
  }
  timecontext_t* tc = &((PINO_DATABASE*)dbid)->timecontext;
  return (*_XTreeGetTimedRecord) (dbid, nid, tc->start.pointer, tc->end.pointer, tc->delta.pointer, data);
}

int _TreePutRow(void *dbid, int nid, int bufsize, int64_t * timestamp, struct descriptor_a *data){
  DESCRIPTOR_A_COEFF(initValue, data->length, data->dtype, 0, 8, 0);
  INIT_STATUS_AS _TreePutTimestampedSegment(dbid, nid, timestamp, data);
  if (status == TreeNOSEGMENTS || status == TreeBUFFEROVF) {
    status = TreeSUCCESS;
    while (data && data->dtype == DTYPE_DSC)
      data = (struct descriptor_a *)data->pointer;
    if (data) {
      if (data->class == CLASS_A) {
	int i;
	initValue.arsize = data->arsize * bufsize;
	initValue.pointer = initValue.a0 = malloc(initValue.arsize);
	memset(initValue.pointer, 0, initValue.arsize);
	initValue.dimct = data->dimct + 1;
	if (data->dimct == 1) {
	  initValue.arsize = data->arsize * bufsize;
	  initValue.m[0] = data->arsize / data->length;
	  initValue.m[1] = bufsize;
	} else {
	  A_COEFF_TYPE *data_c = (A_COEFF_TYPE *) data;
	  for (i = 0; i < data->dimct; i++)
	    initValue.m[i] = data_c->m[i];
	  initValue.m[data->dimct] = bufsize;
	}
      } else if (data->class == CLASS_S || data->class == CLASS_D) {
	initValue.arsize = data->length * bufsize;
	initValue.pointer = initValue.a0 = malloc(initValue.arsize);
	memset(initValue.pointer, 0, initValue.arsize);
	initValue.dimct = 1;
	initValue.m[0] = bufsize;
      } else
	status = TreeFAILURE;
    } else
      status = TreeFAILURE;
    if STATUS_OK {
      status = _TreeBeginTimestampedSegment(dbid, nid, (struct descriptor_a *)&initValue, -1);
      free(initValue.pointer);
      if STATUS_OK
	status = _TreePutTimestampedSegment(dbid, nid, timestamp, data);
    }
  }
  return status;
}

/* RETURN_IF_NOT_OK(get_segment(vars,&idx))
 * opens tree->extended_nci->segment_handler->segemnt_index
 * and finds segment idx.
 * idx can be number in the range of 0-(number of segments-1) or -1.
 * If -1 is specified return the last segment started for the segment.
 * - no clean up required. -
 */
inline static int get_segment(vars_t* vars) {
  INIT_TREESUCCESS;
  RETURN_IF_NOT_OK(open_index_read(vars));
  if (vars->idx == -1) vars->idx = vars->shead.idx;
  while (STATUS_OK && vars->idx < vars->sindex.first_idx && vars->sindex.previous_offset > 0)
    status = get_segment_index(vars->tinfo, vars->sindex.previous_offset, &vars->sindex);
  if STATUS_NOT_OK
    return status;
  if (vars->idx < vars->sindex.first_idx || vars->idx >= vars->sindex.first_idx + SEGMENTS_PER_INDEX)
    return TreeFAILURE;
  vars->sinfo = &vars->sindex.segment[vars->idx % SEGMENTS_PER_INDEX];
  return status;
}
inline static int get_xnci_segment(vars_t* vars, const char *xnciname) {
  INIT_TREESUCCESS;
  RETURN_IF_NOT_OK(open_xnci_index_read(vars, xnciname));
  if (vars->idx == -1) vars->idx = vars->shead.idx;
  while (STATUS_OK && vars->idx < vars->sindex.first_idx && vars->sindex.previous_offset > 0)
    status = get_segment_index(vars->tinfo, vars->sindex.previous_offset, &vars->sindex);
  if STATUS_NOT_OK
    return status;
  if (vars->idx < vars->sindex.first_idx || vars->idx >= vars->sindex.first_idx + SEGMENTS_PER_INDEX)
    return TreeFAILURE;
  vars->sinfo = &vars->sindex.segment[vars->idx % SEGMENTS_PER_INDEX];
  return status;
}

int _TreeGetSegmentLimits(void *dbid, int nid, int idx, struct descriptor_xd *retStart, struct descriptor_xd *retEnd){
  INIT_VARS;vars->idx=idx;
  RETURN_IF_NOT_OK(get_segment(vars));
  return get_segment_limits(vars, retStart, retEnd);
}

int _TreeGetSegment(void *dbid, int nid, int idx, struct descriptor_xd *segment, struct descriptor_xd *dim){
  INIT_VARS;vars->idx=idx;
  RETURN_IF_NOT_OK(get_segment(vars));
  status = read_segment(dbid, vars->tinfo, nid, &vars->shead, vars->sinfo, vars->idx, segment, dim);
  return status;
}

int _TreeGetXNciSegment(void *dbid, int nid, const char *xnciname, int idx, struct descriptor_xd *segment, struct descriptor_xd *dim){
  INIT_VARS;vars->idx=idx;
  RETURN_IF_NOT_OK(get_xnci_segment(vars, xnciname));
  status = read_segment(dbid, vars->tinfo, nid, &vars->shead, vars->sinfo, vars->idx, segment, dim);
  return status;
}

int _TreeGetSegmentInfo(void *dbid, int nid, int idx, char *dtype, char *dimct, int *dims, int *next_row){
  INIT_VARS;vars->idx=idx;
  RETURN_IF_NOT_OK(get_segment(vars));
  if (vars->sinfo->data_offset == -1)
    return TreeFAILURE;
  if (dtype) *dtype = vars->shead.dtype;
  if (dimct) *dimct = vars->shead.dimct;
  if (dims)  memcpy(dims, vars->shead.dims, sizeof(vars->shead.dims));
  if (next_row) {
    if (vars->idx == vars->shead.idx)
      *next_row = vars->shead.next_row;
    else if (vars->sinfo->rows < 1)
      return get_compressed_segment_rows(vars->tinfo, vars->sinfo->data_offset, next_row);
    else
      *next_row = vars->sinfo->rows;
  }
  return status;
}

inline static int is_segment_in_range(vars_t* vars, struct descriptor *start, struct descriptor *end){
  int ans = B_FALSE;
  if ((start && start->pointer) || (end && end->pointer)) {
    INIT_TREESUCCESS;
    status = LibFindImageSymbol_C("TdiShr", "_TdiExecute", &_TdiExecute);
    if STATUS_OK {
      EMPTYXD(segstart);
      EMPTYXD(segend);
      DESCRIPTOR_LONG(ans_d, &ans);
      status = get_segment_limits(vars, &segstart, &segend);
      if STATUS_OK {
	if ((start && start->pointer) && (end && end->pointer)) {
	  STATIC_CONSTANT DESCRIPTOR(expression, "($ <= $) && ($ >= $)");
	  status = _TdiExecute(&vars->dblist,&expression,start,&segend,end,&segstart,&ans_d MDS_END_ARG);
	} else {
	  if (start && start->pointer) {
	    STATIC_CONSTANT DESCRIPTOR(expression, "($ <= $)");
	    status = _TdiExecute(&vars->dblist,&expression,start,&segend,&ans_d MDS_END_ARG);
	  } else {
	    STATIC_CONSTANT DESCRIPTOR(expression, "($ >= $)");
	    status = _TdiExecute(&vars->dblist,&expression,end,&segstart,&ans_d MDS_END_ARG);
	  }
	}
      }
      MdsFree1Dx(&segstart, 0);
      MdsFree1Dx(&segend, 0);
    }
  } else
    ans = B_TRUE;
  return ans;
}

int _TreeGetSegments(void *dbid, int nid, struct descriptor *start, struct descriptor *end, struct descriptor_xd *out){
  /* Get all the segments in an apd which contain data between the start and end times specified */
  INIT_VARS;
  RETURN_IF_NOT_OK(open_header_read(vars));
  int numsegs = vars->shead.idx + 1;
  int segfound = B_FALSE;
  int apd_off = numsegs;
  struct descriptor **dptr = malloc(sizeof(struct descriptor *) * numsegs * 2);
  DESCRIPTOR_APD(apd, DTYPE_LIST, dptr, numsegs * 2);
  memset(dptr, 0, sizeof(struct descriptor *) * numsegs * 2);
  status = get_segment_index(vars->tinfo, vars->shead.index_offset, &vars->sindex);
  for (vars->idx = numsegs; STATUS_OK && vars->idx-- > 0; ) {
    while (STATUS_OK && vars->idx < vars->sindex.first_idx && vars->sindex.previous_offset > 0)
      status = get_segment_index(vars->tinfo, vars->sindex.previous_offset, &vars->sindex);
    if STATUS_NOT_OK
      break;
    else {
      vars->sinfo = &vars->sindex.segment[vars->idx % SEGMENTS_PER_INDEX];
      if (is_segment_in_range(vars, start, end)) {
	apd_off = vars->idx;
	EMPTYXD(segment);
	EMPTYXD(dim);
	segfound = B_TRUE;
	status = read_segment(dbid, vars->tinfo, nid, &vars->shead, vars->sinfo, vars->idx, &segment, &dim);
	if STATUS_OK {
	  apd.pointer[vars->idx*2] = malloc(sizeof(struct descriptor_xd));
	  memcpy(apd.pointer[vars->idx*2], &segment, sizeof(struct descriptor_xd));
	  apd.pointer[vars->idx*2+1] = malloc(sizeof(struct descriptor_xd));
	  memcpy(apd.pointer[vars->idx*2+1], &dim, sizeof(struct descriptor_xd));
	} else {
	  MdsFree1Dx(&segment, 0);
	  MdsFree1Dx(&dim, 0);
	}
      } else if (segfound)
	  break;
    }
  }
  if STATUS_OK {
    apd.arsize = (numsegs - apd_off) * 2 * sizeof(struct descriptor *);
    apd.pointer = &apd.pointer[apd_off * 2];
    status = MdsCopyDxXd((struct descriptor *)&apd, out);
  }
  for (vars->idx = apd_off<<1; vars->idx < numsegs<<1; vars->idx++) {
    if (dptr[vars->idx] != NULL) {
      MdsFree1Dx((struct descriptor_xd *)dptr[vars->idx], 0);
      free(dptr[vars->idx]);
    }
  }
  free(dptr);
  return status;
}

#define SEGMENT_SCALE_NAME "SegmentScale"
int _TreeSetSegmentScale(void *dbid, int nid, struct descriptor *value) {
  return _TreeSetXNci(dbid, nid, SEGMENT_SCALE_NAME, value);
}
int _TreeGetSegmentScale(void *dbid, int nid, struct descriptor_xd *value) {
  return _TreeGetXNci(dbid, nid, SEGMENT_SCALE_NAME, value);
}
