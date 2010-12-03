#include <string.h>
#include <mdsdescrip.h>
#include <mdstypes.h>
#include <mdsshr.h>
#include <librtl_messages.h>
#include <STATICdef.h>

extern void MdsFixDscLength(struct descriptor *in);

#define align(bytes,size) ((((bytes) + (size) - 1)/(size)) * (size))
#define FixLength(a) if (a.length == 0) MdsFixDscLength((struct descriptor *)&a)

#define LoadChar(in,outp)  (outp)[0] = ((char *)&in)[0]
#ifdef WORDS_BIGENDIAN
#define LoadShort(in,outp) (outp)[0] = ((char *)&in)[1]; (outp)[1] = ((char *)&in)[0]
#define LoadInt(in,outp)   (outp)[0] = ((char *)&in)[3]; (outp)[1] = ((char *)&in)[2]; \
                           (outp)[2] = ((char *)&in)[1]; (outp)[3] = ((char *)&in)[0]
#define LoadQuad(in,outp)  (outp)[0] = ((char *)&in)[7]; (outp)[1] = ((char *)&in)[6]; \
                           (outp)[2] = ((char *)&in)[5]; (outp)[3] = ((char *)&in)[4]; \
                           (outp)[4] = ((char *)&in)[3]; (outp)[5] = ((char *)&in)[2]; \
                           (outp)[6] = ((char *)&in)[1]; (outp)[7] = ((char *)&in)[0]
#else
#define LoadShort(in,outp) (outp)[0] = ((char *)&in)[0]; (outp)[1] = ((char *)&in)[1]
#define LoadInt(in,outp)   (outp)[0] = ((char *)&in)[0]; (outp)[1] = ((char *)&in)[1]; \
                           (outp)[2] = ((char *)&in)[2]; (outp)[3] = ((char *)&in)[3]
#define LoadQuad(in,outp)  (outp)[0] = ((char *)&in)[0]; (outp)[1] = ((char *)&in)[1]; \
                           (outp)[2] = ((char *)&in)[2]; (outp)[3] = ((char *)&in)[3]; \
                           (outp)[4] = ((char *)&in)[4]; (outp)[5] = ((char *)&in)[5]; \
                           (outp)[6] = ((char *)&in)[6]; (outp)[7] = ((char *)&in)[7]
#endif
#define set_aflags(flags)  *(flags) = (inp->aflags.binscale << 3)  | (inp->aflags.redim << 4) | (inp->aflags.column << 5) \
                                     | (inp->aflags.coeff << 6) | (inp->aflags.bounds << 7)
#define offset(ptr)       *(unsigned int *)&ptr

#ifdef HAVE_VXWORKS_H
#define _int64 long long
#endif


#if defined(WORDS_BIGENDIAN)

#define swap(dtype,ptr,ans) \
{ char *p = ptr;\
  char __tmp[sizeof(dtype)]; \
  unsigned int __i;							\
  for (__i=0;__i<sizeof(dtype);__i++) __tmp[sizeof(dtype)-__i-1] = p[__i]; \
  ans = *(dtype *)__tmp; \
}

#else

#define swap(dtype,ptr,ans) ans = *(dtype *)(ptr)

#endif

STATIC_ROUTINE int copy_rec_dx( unsigned char *in_ptr, struct descriptor *out_dsc_ptr, 
                        descriptor_llength *b_out, descriptor_llength *b_in)
{
  unsigned int status = 1,i,j;
  descriptor_llength
              bytes_out = 0,
              bytes_in = 0,
              size_out,
              size_in;
  if (in_ptr && (in_ptr[0] || in_ptr[1] || in_ptr[2] || in_ptr[3]))
    switch (in_ptr[3])
    {
    case CLASS_S_SHORT:
    case CLASS_D_SHORT:
     {
       /* This class is stored with 2-byte length, 1-byte dtype, 1-byte class, 4-bytes pointer(unused), data... */
	struct descriptor in;
	struct descriptor *po = out_dsc_ptr;
	unsigned short len;
	swap(unsigned short,in_ptr,len);
        in.length=(descriptor_length)len;
        in.dtype = in_ptr[2];
        FixLength(in);
	in.class = CLASS_S;
	if (po)
	{
	  *po = in;
	  po->pointer = (char *) (po + 1);
          memcpy(po->pointer,in_ptr+8,in.length);
#if defined(WORDS_BIGENDIAN)
          if (po->length > 1 && po->dtype != DTYPE_T && po->dtype != DTYPE_IDENT && po->dtype != DTYPE_PATH)
	  {
	    switch (po->length)
	    {
	    case 2: swap(short,po->pointer,*(short *)po->pointer) break;
            case 4: swap(int,po->pointer,*(int *)po->pointer) break;
            case 8: swap(_int64,po->pointer,*(_int64 *)po->pointer); break;
	    }
	  }
#endif
	}
	bytes_out = align(sizeof(struct descriptor) + in.length,sizeof(void *));
        bytes_in = 8 + in.length;
     }
     break;

#ifdef BIG_DESC
    case CLASS_S:
    case CLASS_D: {
      /* This class stored as 2-bytes(unused), 1-byte dtype, 1-byte class, 4-byte l_length, data... */
	struct descriptor in;
	struct descriptor *po = out_dsc_ptr;
	swap(descriptor_length,in_ptr+4,in.length);
        in.dtype = in_ptr[2];
        FixLength(in);
	in.class = CLASS_S;
	if (po)
	{
	  *po = in;
	  po->pointer = (char *) (po + 1);
          memcpy(po->pointer,in_ptr+8,in.length);
#if defined(WORDS_BIGENDIAN)
          if (po->length > 1 && po->dtype != DTYPE_T && po->dtype != DTYPE_IDENT && po->dtype != DTYPE_PATH)
	  {
	    switch (po->length)
	    {
	    case 2: swap(short,po->pointer,*(short *)po->pointer) break;
            case 4: swap(int,po->pointer,*(int *)po->pointer) break;
            case 8: swap(_int64,po->pointer,*(_int64 *)po->pointer); break;
	    }
	  }
#endif
	}
	bytes_out = align(sizeof(struct descriptor) + in.length,sizeof(void *));
        bytes_in = 8 + in.length;
     }
     break;
#endif

     case CLASS_XS_SHORT:
     case CLASS_XD_SHORT:
     {
       /*This class is stored as 2-byte(unused), 1-byte dtype, 1-byte class, 4-byte pointer(unused),4-byte l_length,data...*/
       struct descriptor_xs in = {DESCRIPTOR_HEAD_INI(0,0,0,0),0};
       struct descriptor_xs *po = (struct descriptor_xs *) out_dsc_ptr;
       unsigned int len;
       in.dtype = in_ptr[2];
       in.class = CLASS_XS;
       swap(unsigned int,in_ptr+8,len);
       in.l_length=(descriptor_llength)len;
       if (po) {
	 *po = in;
	 po->pointer = (struct descriptor *) (po + 1);
	 memcpy(po->pointer,in_ptr+12,in.l_length);
       }
       bytes_out = align(sizeof(struct descriptor_xs) + in.l_length,sizeof(void *));
       bytes_in = 12 + in.l_length;
     }
     break;

#ifdef BIG_DESC
     case CLASS_XS:
     case CLASS_XD:
     {
       /*This class is stored as 2-byte(unused), 1-byte dtype, 1-byte class, 8-byte l_length,data...*/
       struct descriptor_xs in = {DESCRIPTOR_HEAD_INI(0,0,0,0),0};
       struct descriptor_xs *po = (struct descriptor_xs *) out_dsc_ptr;
       in.dtype = in_ptr[2];
       in.class = CLASS_XS;
       swap(descriptor_llength,in_ptr+4,in.l_length);
       if (po) {
	 *po = in;
	 po->pointer = (struct descriptor *) (po + 1);
	 memcpy(po->pointer,in_ptr+12,in.l_length);
       }
       bytes_out = align(sizeof(struct descriptor_xs) + in.l_length,sizeof(void *));
       bytes_in = 12 + in.l_length;
     }
     break;
#endif

     case CLASS_R_SHORT:
     {
       /*This class is stored as 2-byte length, 1-byte dtype, 1-byte class, 4-byte pointer (unused), 1-byte ndesc, 3-bytes(unused),
	 4-byte offset to descriptor repeated ndesc times,data...,descriptor data repeated ndesc times*/
        struct descriptor_r pi_tmp;
	struct descriptor_r *pi = &pi_tmp;
	struct descriptor_r *po = (struct descriptor_r *) out_dsc_ptr;
	unsigned short len;
	swap(unsigned short,in_ptr,len);
	pi_tmp.length=(descriptor_length)len;
        pi_tmp.dtype = in_ptr[2];
        FixLength(pi_tmp);
        pi_tmp.class = CLASS_R;
        pi_tmp.ndesc = in_ptr[8];
	pi_tmp.dscptrs[0] = 0;
	bytes_out = sizeof(struct descriptor_r) + (int)(pi->ndesc - 1) * sizeof(struct descriptor *);
        bytes_in = 12 + pi->ndesc * 4;
	if (po)
	{

	  memset(po,0,bytes_out);
          *po = *pi;
	  if (pi->length > 0)
	  {
	    po->pointer = (unsigned char *) po + bytes_out;
            memcpy(po->pointer,&in_ptr[12+pi->ndesc*4],pi->length);
#if defined(WORDS_BIGENDIAN)
            if (po->dtype == DTYPE_FUNCTION && po->length == 2)
	      {
                swap(short,(char *)po->pointer,*(short *)po->pointer);
              }
#endif
	  }
	}
	bytes_out = align(bytes_out + pi->length,sizeof(void *));
	bytes_in += pi->length;
      /******************************
      Each descriptor must be copied.
      ******************************/
	for (j = 0; j < pi->ndesc && status & 1; ++j)
	{
          if (in_ptr[12+j*4] | in_ptr[13+j*4] | in_ptr[14+j*4] | in_ptr[15+j*4])
	  {
	    status = copy_rec_dx(in_ptr + bytes_in, po ? (struct descriptor *) ((char *) po + bytes_out) : 0, &size_out, &size_in);
	    if (po)
	      po->dscptrs[j] = size_out ? (struct descriptor *) ((char *) po + bytes_out) : 0;
	    bytes_out = align(bytes_out + size_out, sizeof(void *));
            bytes_in += size_in;
	  }
	}
     }
     break;

#ifdef BIG_DESC
     case CLASS_R:
     {
       /*This class is stored as 2-byte length(unused), 1-byte dtype, 1-byte class, 4-byte length, 4-byte ndesc,
	 1-byte dscr flag repeated ndesc times,data...,descriptor data repeated ndesc times...*/
        struct descriptor_r pi_tmp;
	struct descriptor_r *pi = &pi_tmp;
	struct descriptor_r *po = (struct descriptor_r *) out_dsc_ptr;
	swap(descriptor_length,in_ptr+4,pi->length);
        pi->dtype = in_ptr[2];
        FixLength(pi_tmp);
        pi->class = CLASS_R;
        swap(descriptor_ndesc,in_ptr+8,pi->ndesc);
	pi->dscptrs[0] = 0;
	bytes_out = sizeof(struct descriptor_r) + (pi->ndesc - 1) * sizeof(struct descriptor *);
        bytes_in = 12 + pi->ndesc;
	if (po) {

	  memset(po,0,bytes_out);
          *po = *pi;
	  if (pi->length > 0) {
	    po->pointer = (unsigned char *) po + bytes_out;
            memcpy(po->pointer,&in_ptr[12+pi->ndesc],pi->length);
#if defined(WORDS_BIGENDIAN)
            if (po->dtype == DTYPE_FUNCTION && po->length == 2) {
	      unsigned short opcode;
	      swap(unsigned short,(char *)po->pointer,opcode);
	      *(unsigned short *)po->pointer=opcode;
	    }
#endif
	  }
	}
	bytes_out = align(bytes_out + pi->length,sizeof(void *));
	bytes_in += pi->length;

      /******************************
      Each descriptor must be copied.
      ******************************/
	for (j = 0; j < pi->ndesc && status & 1; ++j) {
          if (in_ptr[12+j]) {
	    status = copy_rec_dx(in_ptr + bytes_in, po ? (struct descriptor *) ((char *) po + bytes_out) : 0, &size_out, &size_in);
	    if (po)
	      po->dscptrs[j] = size_out ? (struct descriptor *) ((char *) po + bytes_out) : 0;
	    bytes_out = align(bytes_out + size_out, sizeof(void *));
            bytes_in += size_in;
	  }
	}
     }
     break;
#endif

     case CLASS_A_SHORT:
     {
       /* This class is stored as 2-byte length, 1-byte dtype, 1-byte class, 4-byte pointer(unused), 1-byte scale, 1-byte digits, 1-byte aflags,
	  1-byte dimct, 4-byte arsize, (optional 4-byte a0, one per dimct 4-byte multiplier (,optional one per dimct (4-byte lower bound, 4-byte upper bound))),data....
       */
        int dsc_size;
        int align_size;
        array_coeff a_tmp;
	array_coeff *pi = &a_tmp;
	array_coeff *po = (array_coeff *) out_dsc_ptr;
	unsigned short len;
	unsigned int arsize;
	swap(unsigned short,in_ptr,len);
	pi->length = len;
        pi->dtype = in_ptr[2];
        FixLength(a_tmp);
        pi->class = CLASS_A;
        pi->scale = in_ptr[8];
        pi->digits = in_ptr[9];
	pi->aflags.binscale = (*(in_ptr+10) & 0x08) != 0;
	pi->aflags.redim = (in_ptr[10] & 0x10) != 0;
	pi->aflags.column = (in_ptr[10] & 0x20) != 0;
	pi->aflags.coeff = (in_ptr[10] & 0x40) != 0;
	pi->aflags.bounds = (in_ptr[10] & 0x80) != 0;
        pi->dimct = in_ptr[11];
	swap(unsigned int,in_ptr+12,arsize);
	pi->arsize=(descriptor_a_arsize)arsize;
	bytes_in = 16
		+ (pi->aflags.coeff ? 4 + 4 * pi->dimct : 0)
		+ (pi->aflags.bounds ? 4 * (pi->dimct * 2) : 0);
	dsc_size = sizeof(struct descriptor_a)
		+ (pi->aflags.coeff ? sizeof(char *) + sizeof(descriptor_a_mult) * pi->dimct : 0)
		+ (pi->aflags.bounds ? sizeof(descriptor_a_bounds) * (pi->dimct * 2) : 0);
        align_size = (pi->dtype == DTYPE_T) ? 1 : pi->length;
	bytes_out = dsc_size + pi->arsize + align_size;
	if (po)
	{
          memcpy(po,pi,sizeof(struct descriptor_a));
	  if (pi->aflags.coeff) {
	    for (i=0;i<pi->dimct;i++) {
	      unsigned int mult;
              swap(unsigned int,in_ptr + 20 + 4 * i,mult);
	      po->m[i] = (descriptor_a_mult)mult;
              if (pi->aflags.bounds) {
		int bound;
                swap(int,in_ptr + 20 + (pi->dimct + 2 * i) * 4,bound);
		po->m[pi->dimct + 2 * i] = (descriptor_a_bounds)bound;
                swap(int,in_ptr + 20 + (pi->dimct + 2 * i + 1) * 4,bound);
                po->m[pi->dimct + 2 * i + 1] = (descriptor_a_bounds)bound;
              }
            }
          }
	  po->pointer = (char *)po + align((char *)po -(char *)0 + dsc_size,align_size) - ((char *)po - (char *)0);
          memcpy(po->pointer,in_ptr+bytes_in,pi->arsize);
	  if (pi->aflags.coeff) {
	    int offset;
	    int a0;
	    swap(int,in_ptr+4,offset);
	    swap(int,in_ptr+16,a0);
	    po->a0 = po->pointer + (a0 - offset);
	  }
#if defined(WORDS_BIGENDIAN)
          if (po->dtype != DTYPE_T && po->dtype != DTYPE_IDENT && po->dtype != DTYPE_PATH)
	  {
            int i;
	    switch (po->length)
	    {
	    case 2: 
	      { short *ptr;
		for (i=0,ptr=(short *)po->pointer;i< (int)po->arsize;i += sizeof(*ptr),ptr++) swap(short,(char *)ptr,*ptr);
	      }
	      break;
	    case 4:
	      { int *ptr;
		for (i=0,ptr=(int *)po->pointer;i<(int)po->arsize;i += sizeof(*ptr),ptr++) swap(int,(char *)ptr,*ptr);
	      }
	      break;
	    case 8:
	      { _int64 *ptr;
		for (i=0,ptr=(_int64 *)po->pointer;i<(int)po->arsize;i += sizeof(*ptr),ptr++) swap(_int64,(char *)ptr,*ptr);
	      }
	      break;
	    }
	  }
#endif
	}
	bytes_out += pi->arsize;
        bytes_in += pi->arsize;
     }
     break;
#ifdef BIG_DESC
     case CLASS_A:
     {
       /* This class is stored as 2-byte unused, 1-byte dtype, 1-byte class, 4-byte length, 1-byte scale, 1-byte digits, 1-byte aflags,
	  1-byte dimct, 8-byte arsize, (optional 8-byte a0, one per dimct 8-byte multiplier (,optional one per dimct (8-byte lower bound, 8-byte upper bound))),data....
       */
        int dsc_size;
        int align_size;
        array_coeff a_tmp;
	array_coeff *pi = &a_tmp;
	array_coeff *po = (array_coeff *) out_dsc_ptr;
	swap(descriptor_length,in_ptr+4,pi->length);
        pi->dtype = in_ptr[2];
        FixLength(a_tmp);
        pi->class = CLASS_A;
        pi->scale = in_ptr[8];
        pi->digits = in_ptr[9];
	pi->aflags.binscale = (*(in_ptr+10) & 0x08) != 0;
	pi->aflags.redim = (in_ptr[10] & 0x10) != 0;
	pi->aflags.column = (in_ptr[10] & 0x20) != 0;
	pi->aflags.coeff = (in_ptr[10] & 0x40) != 0;
	pi->aflags.bounds = (in_ptr[10] & 0x80) != 0;
        pi->dimct = in_ptr[11];
	swap(descriptor_a_arsize,in_ptr+12,pi->arsize);
	bytes_in = 20
		+ (pi->aflags.coeff ? 8 + 8 * pi->dimct : 0)
		+ (pi->aflags.bounds ? 8 * (pi->dimct * 2) : 0);
	dsc_size = sizeof(struct descriptor_a)
		+ (pi->aflags.coeff ? sizeof(char *) + sizeof(descriptor_a_mult) * pi->dimct : 0)
		+ (pi->aflags.bounds ? sizeof(descriptor_a_bounds) * (pi->dimct * 2) : 0);
        align_size = (pi->dtype == DTYPE_T) ? 1 : pi->length;
	bytes_out = dsc_size + pi->arsize + align_size;
	if (po)
	{
          memcpy(po,pi,sizeof(struct descriptor_a));
	  if (pi->aflags.coeff) {
	    for (i=0;i<pi->dimct;i++) {
              swap(descriptor_a_mult,in_ptr + 28 + 8 * i,po->m[i]);
              if (pi->aflags.bounds) {
                swap(descriptor_a_bounds,in_ptr + 28 + (pi->dimct + 2 * i) * 8,po->m[pi->dimct + 2 * i]);
                swap(descriptor_a_bounds,in_ptr + 28 + (pi->dimct + 2 * i + 1) * 8,po->m[pi->dimct + 2 * i + 1]);
              }
            }
          }
	  po->pointer = (char *)po + align((char *)po -(char *)0 + dsc_size,align_size) - ((char *)po - (char *)0);
          memcpy(po->pointer,in_ptr+bytes_in,pi->arsize);
	  if (pi->aflags.coeff) {
	    long long offset;
	    swap(long long,in_ptr+20,offset);
	    po->a0 = po->pointer + offset;
	  }
#if defined(WORDS_BIGENDIAN)
          if (po->dtype != DTYPE_T && po->dtype != DTYPE_IDENT && po->dtype != DTYPE_PATH)
	  {
            int i;
	    switch (po->length)
	    {
	    case 2: 
	      { short *ptr;
		for (i=0,ptr=(short *)po->pointer;i< (int)po->arsize;i += sizeof(*ptr),ptr++) swap(short,(char *)ptr,*ptr);
	      }
	      break;
	    case 4:
	      { int *ptr;
		for (i=0,ptr=(int *)po->pointer;i<(int)po->arsize;i += sizeof(*ptr),ptr++) swap(int,(char *)ptr,*ptr);
	      }
	      break;
	    case 8:
	      { _int64 *ptr;
		for (i=0,ptr=(_int64 *)po->pointer;i<(int)po->arsize;i += sizeof(*ptr),ptr++) swap(_int64,(char *)ptr,*ptr);
	      }
	      break;
	    }
	  }
#endif
	}
	bytes_out += pi->arsize;
        bytes_in += pi->arsize;
     }
     break;
#endif

    /**************************************
    For CA and APD, a0 is the offset.
    **************************************/
     case CLASS_APD_SHORT:
       {
       /* This class is stored as 2-byte length(unused), 1-byte dtype, 1-byte class, 4-byte pointer, 1-byte scale, 1-byte digits, 1-byte aflags,
	  1-byte dimct, 4-byte arsize, (optional 4-byte a0, one per dimct 4-byte multiplier (,optional one per dimct (4-byte lower bound, 4-byte upper bound))),
	  arsize/4 descriptors and data....
       */
        array_coeff a_tmp;
	array_coeff *pi = &a_tmp;
	array_coeff *po = (array_coeff *) out_dsc_ptr;
	struct descriptor **pdi = (struct descriptor **) pi->pointer;
	struct descriptor **pdo = 0;
	unsigned int       num_dsc;
	unsigned short len;
	unsigned int arsize;
        pi->dtype = in_ptr[2];
        pi->class = CLASS_APD;
        pi->scale = in_ptr[8];
        pi->digits = in_ptr[9];
	pi->aflags.binscale = (*(in_ptr+10) & 0x08) != 0;
	pi->aflags.redim = (in_ptr[10] & 0x10) != 0;
	pi->aflags.column = (in_ptr[10] & 0x20) != 0;
	pi->aflags.coeff = (in_ptr[10] & 0x40) != 0;
	pi->aflags.bounds = (in_ptr[10] & 0x80) != 0;
        pi->dimct = in_ptr[11];
	swap(unsigned int,in_ptr+12,arsize);
	pi->arsize=(descriptor_a_arsize)arsize;
	num_dsc = pi->arsize/4;
        pi->arsize=num_dsc*sizeof(void *);
	pi->length=sizeof(void *);
	bytes_in = 16
		+ (pi->aflags.coeff ? 4 + 4 * pi->dimct : 0)
		+ (pi->aflags.bounds ? 4 * (pi->dimct * 2) : 0);
	bytes_out = sizeof(struct descriptor_a)
		+ (pi->aflags.coeff ? sizeof(char *) + sizeof(descriptor_a_mult) * pi->dimct : 0)
		+ (pi->aflags.bounds ? sizeof(descriptor_a_bounds) * (pi->dimct * 2) : 0);
	if (po) {
          memcpy(po,pi,sizeof(struct descriptor_a));
	  if (pi->aflags.coeff) {
	    for (i=0;i<pi->dimct;i++) {
	      unsigned int mult;
              swap(unsigned int,in_ptr + 20 + 4 * i,mult);
	      po->m[i] = (descriptor_a_mult)mult;
              if (pi->aflags.bounds) {
		int bound;
                swap(int,in_ptr + 20 + (pi->dimct + 2 * i) * 4,bound);
		po->m[pi->dimct + 2 * i] = (descriptor_a_bounds)bound;
                swap(int,in_ptr + 20 + (pi->dimct + 2 * i + 1) * 4,bound);
                po->m[pi->dimct + 2 * i + 1] = (descriptor_a_bounds)bound;
              }
            }
          }
	  po->pointer = (char *) po + bytes_out;
	  if (pi->aflags.coeff) {
	    int offset;
	    int a0;
	    swap(int,in_ptr+4,offset);
	    swap(int,in_ptr+16,a0);
	    po->a0 = po->pointer + (a0 - offset);
	  }
	  pdo = (struct descriptor **) po->pointer;
	}
	bytes_out += pi->arsize;
        bytes_in += num_dsc*4;
	/******************************
      Each descriptor must be copied.
	******************************/
	for (j = 0; j < num_dsc && status & 1; ++j) {
          status = copy_rec_dx(in_ptr + bytes_in, po ? (struct descriptor *) ((char *) po + bytes_out) : 0, &size_out, &size_in);
	  if (po)
	    *pdo++ = size_out ? (struct descriptor *) ((char *) po + bytes_out) : 0;
	  bytes_out += size_out;
          bytes_in += size_in;
	}
       }
       break;

#ifdef BIG_DESC

     case CLASS_APD:
       {
       /* This class is stored as 2-byte length(unused), 1-byte dtype, 1-byte class, 1-byte scale, 1-byte digits, 1-byte aflags,
	  1-byte dimct, 8-byte arsize, (optional 8-byte a0, one per dimct 8-byte multiplier (,optional one per dimct (8-byte lower bound, 8-byte upper bound))),
	  arsize/8 descriptors and data....
       */
        array_coeff a_tmp;
	array_coeff *pi = &a_tmp;
	array_coeff *po = (array_coeff *) out_dsc_ptr;
	struct descriptor **pdi = (struct descriptor **) pi->pointer;
	struct descriptor **pdo = 0;
	unsigned long long       num_dsc;
        pi->dtype = in_ptr[2];
        pi->class = CLASS_APD;
        pi->scale = in_ptr[8];
        pi->digits = in_ptr[9];
	pi->aflags.binscale = (*(in_ptr+10) & 0x08) != 0;
	pi->aflags.redim = (in_ptr[10] & 0x10) != 0;
	pi->aflags.column = (in_ptr[10] & 0x20) != 0;
	pi->aflags.coeff = (in_ptr[10] & 0x40) != 0;
	pi->aflags.bounds = (in_ptr[10] & 0x80) != 0;
        pi->dimct = in_ptr[11];
	swap(descriptor_a_arsize,in_ptr+12,pi->arsize);
	num_dsc = pi->arsize/8;
	pi->length=sizeof(void *);
	bytes_in = 16
		+ (pi->aflags.coeff ? 8 + 8 * pi->dimct : 0)
		+ (pi->aflags.bounds ? 8 * (pi->dimct * 2) : 0);
	bytes_out = sizeof(struct descriptor_a)
		+ (pi->aflags.coeff ? sizeof(char *) + sizeof(descriptor_a_mult) * pi->dimct : 0)
		+ (pi->aflags.bounds ? sizeof(descriptor_a_bounds) * (pi->dimct * 2) : 0);
	if (po) {
          memcpy(po,pi,sizeof(struct descriptor_a));
	  if (pi->aflags.coeff) {
	    for (i=0;i<pi->dimct;i++) {
              swap(descriptor_a_mult, in_ptr + 24 + 8 * i,po->m[i]);
              if (pi->aflags.bounds) {
                swap(int,in_ptr + 24 + (pi->dimct + 2 * i) * 8,po->m[pi->dimct + 2 * i]);
                swap(int,in_ptr + 24 + (pi->dimct + 2 * i + 1) * 8,po->m[pi->dimct + 2 * i + 1]);
              }
            }
          }
	  po->pointer = (char *) po + bytes_out;
	  if (pi->aflags.coeff) {
	    long long offset;
	    swap(int,in_ptr+16,offset);
	    po->a0 = po->pointer + offset;
	  }
	  pdo = (struct descriptor **) po->pointer;
	}
	bytes_out += pi->arsize;
	/******************************
      Each descriptor must be copied.
	******************************/
	for (j = 0; j < num_dsc && status & 1; ++j) {
          status = copy_rec_dx(in_ptr + bytes_in, po ? (struct descriptor *) ((char *) po + bytes_out) : 0, &size_out, &size_in);
	  if (po)
	    *pdo++ = size_out ? (struct descriptor *) ((char *) po + bytes_out) : 0;
	  bytes_out += size_out;
          bytes_in += size_in;
	}
       }
       break;
#endif

     case CLASS_CA_SHORT:
     {
       /* This class is stored as 2-byte length, 1-byte dtype, 1-byte class, 4-byte pointer, 1-byte scale, 1-byte digits, 1-byte aflags,
	  1-byte dimct, 4-byte arsize, (optional 4-byte a0, one per dimct 4-byte multiplier (,optional one per dimct (4-byte lower bound, 4-byte upper bound))),
	  compressed descriptor and data....
       */
        array_coeff a_tmp;
	array_coeff *pi = &a_tmp;
	array_coeff *po = (array_coeff *) out_dsc_ptr;
	struct descriptor **pdi = (struct descriptor **) pi->pointer;
	struct descriptor **pdo = 0;
	unsigned short len;
	unsigned int arsize;
	unsigned int offset;
	swap(unsigned short,in_ptr,len);
	pi->length = len;
        pi->dtype = in_ptr[2];
        FixLength(a_tmp);
        pi->class = CLASS_CA;
        pi->scale = in_ptr[8];
        pi->digits = in_ptr[9];
	pi->aflags.binscale = (*(in_ptr+10) & 0x08) != 0;
	pi->aflags.redim = (in_ptr[10] & 0x10) != 0;
	pi->aflags.column = (in_ptr[10] & 0x20) != 0;
	pi->aflags.coeff = (in_ptr[10] & 0x40) != 0;
	pi->aflags.bounds = (in_ptr[10] & 0x80) != 0;
        pi->dimct = in_ptr[11];
	swap(unsigned int,in_ptr+12,arsize);
	pi->arsize=(descriptor_a_arsize)arsize;
	bytes_in = 16
		+ (pi->aflags.coeff ? 4 + 4 * pi->dimct : 0)
		+ (pi->aflags.bounds ? 4 * (pi->dimct * 2) : 0);
	bytes_out = align(sizeof(struct descriptor_a)
		+ (pi->aflags.coeff ? sizeof(char *) + sizeof(descriptor_a_mult) * pi->dimct : 0)
		+ (pi->aflags.bounds ? sizeof(descriptor_a_bounds) * (pi->dimct * 2) : 0),sizeof(void *));
	swap(unsigned int,in_ptr+4,offset);
	if (po) {
          memcpy(po,pi,sizeof(struct descriptor_a));
	  if (pi->aflags.coeff) {
	    int a0_offset;
	    swap(int,in_ptr+16,a0_offset);
	    po->a0 = (char *)0+a0_offset;
	    for (i=0;i<pi->dimct;i++) {
	      unsigned int mult;
              swap(unsigned int,in_ptr + 20 + 4 * i,mult);
	      po->m[i] = (descriptor_a_mult)mult;
              if (pi->aflags.bounds) {
		int bound;
                swap(int,in_ptr + 20 + (pi->dimct + 2 * i) * 4,bound);
		po->m[pi->dimct + 2 * i] = (descriptor_a_bounds)bound;
                swap(int,in_ptr + 20 + (pi->dimct + 2 * i + 1) * 4,bound);
                po->m[pi->dimct + 2 * i + 1] = (descriptor_a_bounds)bound;
              }
            }
          }
          po->pointer = offset ? 
              (char *)po + align( ((char *)po - (char *)0) + bytes_out, sizeof(void *)) - ((char *)po - (char *)0) : 0;
	}

      /***************************
      Null pointer for shape only.
      ***************************/
	if (offset)
	{
          status = copy_rec_dx(in_ptr + bytes_in, po ? (struct descriptor *) ((char *) po + bytes_out) : 0, &size_out, &size_in);
	  bytes_out += size_out;
          bytes_in += size_in;
	}
      }
      break;

#ifdef BIG_DESC
     case CLASS_CA:
     {
       /* This class is stored as 2-byte unused, 1-byte dtype, 1-byte class, 4-byte length, 1-byte scale, 1-byte digits, 1-byte aflags,
	  1-byte dimct, 8-byte arsize, 1-byte pointer-flag, (optional 8-byte a0, one per dimct 8-byte multiplier (,optional one per dimct (8-byte lower bound, 8-byte upper bound))),data....
       */
        int dsc_size;
        int align_size;
        array_coeff a_tmp;
	array_coeff *pi = &a_tmp;
	array_coeff *po = (array_coeff *) out_dsc_ptr;
	struct descriptor **pdi = (struct descriptor **) pi->pointer;
	struct descriptor **pdo = 0;
        char pointer_flag;
	swap(descriptor_length,in_ptr+4,pi->length);
        pi->dtype = in_ptr[2];
        FixLength(a_tmp);
        pi->class = CLASS_CA;
        pi->scale = in_ptr[8];
        pi->digits = in_ptr[9];
	pi->aflags.binscale = (*(in_ptr+10) & 0x08) != 0;
	pi->aflags.redim = (in_ptr[10] & 0x10) != 0;
	pi->aflags.column = (in_ptr[10] & 0x20) != 0;
	pi->aflags.coeff = (in_ptr[10] & 0x40) != 0;
	pi->aflags.bounds = (in_ptr[10] & 0x80) != 0;
        pi->dimct = in_ptr[11];
	swap(descriptor_a_arsize,in_ptr+12,pi->arsize);
        pointer_flag=in_ptr[20];
	bytes_in = 21
		+ (pi->aflags.coeff ? 8 + 8 * pi->dimct : 0)
		+ (pi->aflags.bounds ? 8 * (pi->dimct * 2) : 0);
	dsc_size = sizeof(struct descriptor_a)
		+ (pi->aflags.coeff ? sizeof(char *) + sizeof(descriptor_a_mult) * pi->dimct : 0)
		+ (pi->aflags.bounds ? sizeof(descriptor_a_bounds) * (pi->dimct * 2) : 0);
	bytes_out = align(sizeof(struct descriptor_a)
		+ (pi->aflags.coeff ? sizeof(char *) + sizeof(descriptor_a_mult) * pi->dimct : 0)
		+ (pi->aflags.bounds ? sizeof(descriptor_a_bounds) * (pi->dimct * 2) : 0),sizeof(void *));
	if (po) {
          memcpy(po,pi,sizeof(struct descriptor_a));
	  if (pi->aflags.coeff) {
	    long long a0_offset;
	    swap(long long,in_ptr+21,a0_offset);
	    po->a0 = (char *)0 + a0_offset;
	    for (i=0;i<pi->dimct;i++) {
              swap(descriptor_a_mult,in_ptr + 29 + 8 * i,po->m[i]);
              if (pi->aflags.bounds) {
                swap(descriptor_a_bounds,in_ptr + 29 + (pi->dimct + 2 * i) * 8, po->m[pi->dimct + 2 * i]);
                swap(descriptor_a_bounds,in_ptr + 29 + (pi->dimct + 2 * i + 1) * 8, po->m[pi->dimct + 2 * i + 1]);
              }
            }
          }
          po->pointer = (po->arsize && pointer_flag) ? 
	    (char *)po + align( ((char *)po - (char *)0) + bytes_out, sizeof(void *)) - ((char *)po - (char *)0) : 0;
	}

      /***************************
      Null pointer for shape only.
      ***************************/
	if (pointer_flag)
	{
          //status = copy_rec_dx(in_ptr + bytes_in, po ? (struct descriptor *) ((char *) po + bytes_out) : 0, &size_out, &size_in);
	  status = copy_rec_dx(in_ptr + bytes_in, po ? (struct descriptor *) po->pointer : 0, &size_out, &size_in);
	  bytes_out += size_out;
          bytes_in += size_in;
	}
      }
      break;
#endif

     default:
      status = LibINVSTRDES;
      break;
    }
  *b_out = bytes_out;
  *b_in  = bytes_in;
  return status;
}

int MdsSerializeDscIn(char *in, struct descriptor_xd *out_ptr)
{
  descriptor_llength size_out;
  descriptor_llength size_in;
  int       status;
  STATIC_CONSTANT const unsigned char dsc_dtype = DTYPE_DSC;
  EMPTYXD(out);
  MdsFree1Dx(out_ptr,0);
  status = copy_rec_dx(in, 0, &size_out, &size_in);
  if (status & 1 && size_out) {
    status = MdsGet1Dx(&size_out, (unsigned char *) &dsc_dtype, &out, 0);
    if (status & 1) {
      status = copy_rec_dx(in, (struct descriptor *)out.pointer, &size_out, &size_in);
#ifdef BIG_DESC
      if (status & 1) {
	if (out_ptr->class == CLASS_XD_SHORT)
	  status = MdsCopyDxXd(out.pointer,out_ptr);
	else
	  *out_ptr = out;
      } else
	MdsFree1Dx(&out,0);
#else
      if (status & 1)
	*out_ptr = out;
      else
	MdsFree1Dx(&out,0);
#endif 
    }
  }
  return status;
}

STATIC_ROUTINE int copy_dx_rec( struct descriptor *in_ptr,char *out_ptr,descriptor_llength *b_out, descriptor_llength *b_in, descriptor_llength *length) {
  unsigned int status = 1,j,num_dsc;
  descriptor_llength
              bytes_out = 0,
              bytes_in = 0,
              size_out,
              size_in;
  if (in_ptr)
    switch (in_ptr->class)
    {
     case CLASS_S:
     case CLASS_D:
      {
	unsigned char class = CLASS_S_SHORT;
	*length = *length + in_ptr->length;
#ifdef BIG_DESC
	  if (in_ptr->length >= (descriptor_length)0x10000)
	    class = CLASS_S;
#endif
	if (out_ptr) {
	  if (class == CLASS_S_SHORT) {
	    unsigned short len = (unsigned short)in_ptr->length;
	    LoadShort(len, out_ptr);
	  } else {
	    LoadInt(in_ptr->length,out_ptr+4);
	  }
	  LoadChar(in_ptr->dtype, out_ptr+2);
	  LoadChar(class,out_ptr+3);
	  out_ptr += 8;
#ifdef WORDS_BIGENDIAN
	  if (in_ptr->dtype != DTYPE_T && in_ptr->dtype != DTYPE_IDENT && in_ptr->dtype != DTYPE_PATH) {
	    switch (in_ptr->length) {
	    case 2: LoadShort(in_ptr->pointer[0],out_ptr); break;
	    case 4: LoadInt(in_ptr->pointer[0],out_ptr); break;
	    case 8: LoadQuad(in_ptr->pointer[0],out_ptr); break;
	    default: memcpy(out_ptr,in_ptr->pointer, in_ptr->length); break;
	    }
	  } else
#endif
	    memcpy(out_ptr,in_ptr->pointer, in_ptr->length);
	  out_ptr += in_ptr->length;
	}
	bytes_in = sizeof(struct descriptor) + in_ptr->length;
	bytes_out = 8 + in_ptr->length;
	break;
      }

     case CLASS_XS:
     case CLASS_XD:
      {
	struct descriptor_xs *inp = (struct descriptor_xs *)in_ptr;
	unsigned char class = CLASS_XS_SHORT;
	*length = *length + inp->l_length;
#ifdef BIG_DESC
	if (inp->l_length >= (descriptor_llength)0x100000000LL)
	  class = CLASS_XS;
#endif
	if (out_ptr) {
	  if (class == CLASS_XS_SHORT) {
	    unsigned int len = (unsigned int)inp->l_length;
	    LoadInt(len,out_ptr+8);
	  } else {
	    LoadQuad(inp->l_length,out_ptr+4);
	  }
	  LoadChar(in_ptr->dtype, out_ptr+2);
	  LoadChar(class,out_ptr+3);
          out_ptr += 12; 
          memcpy(out_ptr, inp->pointer, inp->l_length);
          out_ptr += inp->l_length;
	}
	bytes_in = sizeof(struct descriptor_xs) + inp->l_length;
	bytes_out = 12 + inp->l_length;
	break;
      }
      break;

     case CLASS_R:
      {
        struct descriptor_r *inp = (struct descriptor_r *)in_ptr;
	unsigned char class = CLASS_R_SHORT;
        char *begin = out_ptr;
        char *dscptrs = NULL;
        *length = *length + inp->length;
#ifdef BIG_DESC
	if (inp->ndesc >= (descriptor_ndesc)0x100 || inp->length >= (descriptor_length)0x10000)
	  class = CLASS_R;
#endif
        if (out_ptr) {
	  if (class == CLASS_R_SHORT) {
	    unsigned short len;
	    unsigned char ndesc;
            int offset=12+inp->ndesc*4;
	    len=(unsigned short)inp->length;
	    LoadShort(len,out_ptr);
	    ndesc = (unsigned char)inp->ndesc;
	    LoadInt(offset,out_ptr+4);
	    LoadChar(ndesc,out_ptr+8);
	  } else {
            LoadInt(inp->length, out_ptr+4);
	    LoadInt(inp->ndesc, out_ptr+8);
	  }
          LoadChar(inp->dtype, out_ptr+2);
          LoadChar(class,out_ptr+3);
          dscptrs = out_ptr + 12;
	}
	if (class == CLASS_R_SHORT) {
	  if (out_ptr) {
	    memset(dscptrs,0,inp->ndesc * 4);
	    out_ptr += 12 + inp->ndesc * 4;
	  }
	  bytes_out = 12 + (int)(inp->ndesc) * 4 + inp->length;
	} else {
	  if (out_ptr) {
	    memset(dscptrs,0,inp->ndesc);
	    out_ptr += 12 + inp->ndesc;
	  }
	  bytes_out = 12 + inp->ndesc + inp->length;
	}
	if (out_ptr) {
          if (inp->length) {
            memcpy(out_ptr,inp->pointer,inp->length);
#if defined(WORDS_BIGENDIAN)
            if (inp->dtype == DTYPE_FUNCTION && inp->length == 2) {
	      short value;
	      swap(short,(char *)out_ptr,value);
	      memcpy(out_ptr,&value,2);
	    }
#endif
          }
          out_ptr += inp->length;
        }
        bytes_in = sizeof(struct descriptor_r) + (int)(inp->ndesc - 1) * sizeof(struct descriptor *) + inp->length;
        for (j = 0; j < inp->ndesc; j++) {
	  if (inp->dscptrs[j]) {
	    status = copy_dx_rec(inp->dscptrs[j], out_ptr, &size_out, &size_in, length);
            if (out_ptr) {
              int offset=(int)(out_ptr-begin);
	      if (class == CLASS_R_SHORT) {
		LoadInt(offset, dscptrs + (j * 4));
	      }
	      else
		dscptrs[j]=1;
              out_ptr += size_out;
            }
            bytes_out += size_out;
            bytes_in += size_in;
          }
	}
      }
      break;

     case CLASS_A:
      {
        array_coeff *inp = (array_coeff *)in_ptr;
	unsigned char class = CLASS_A_SHORT;
	*length = *length + inp->arsize;
#ifdef BIG_DESC
	if (inp->length >= 0x10000 || inp->arsize >= 0x100000000LL)
	  class = CLASS_A;
#endif
	if (out_ptr) {
          char *inp2 = inp->pointer;
	  int dscsize;
	  unsigned short len;
          LoadChar(inp->dtype, out_ptr+2);
          LoadChar(class,out_ptr+3);
	  if (class == CLASS_A_SHORT) {
	    len = (unsigned short)inp->length;
	    dscsize = 16 + (inp->aflags.coeff ? 4 + 4 * inp->dimct : 0)
                    + (inp->aflags.bounds ? 4 * (inp->dimct * 2) : 0);
	    LoadShort(inp->length, out_ptr);
            LoadInt(dscsize,out_ptr+4);
	    LoadChar(inp->scale,out_ptr+8);
	    LoadChar(inp->digits,out_ptr+9);
	    set_aflags(out_ptr+10);
	    LoadChar(inp->dimct,out_ptr+11);
	    LoadInt(inp->arsize,out_ptr+12);
	    out_ptr += 16;
	  } else {
	    LoadInt(inp->length, out_ptr+4);
	    LoadChar(inp->scale,out_ptr+8);
	    LoadChar(inp->digits,out_ptr+9);
	    set_aflags(out_ptr+10);
	    LoadChar(inp->dimct,out_ptr+11);
	    LoadQuad(inp->arsize,out_ptr+12);
	    out_ptr += 20;
	  }
          if (inp->aflags.coeff) {
	    if (class == CLASS_A_SHORT) {
	      int a0 = dscsize + inp->a0 - inp->pointer;
	      LoadInt(a0,out_ptr);
	      out_ptr += 4;
	      for (j=0;j<inp->dimct;j++) {
		LoadInt(inp->m[j],out_ptr);
		out_ptr += 4;
	      }
	      if (inp->aflags.bounds) {
		for (j=0;j<inp->dimct;j++) {
		  LoadInt(inp->m[inp->dimct + 2 * j],out_ptr);
		  out_ptr += 4;
		  LoadInt(inp->m[inp->dimct + 2 * j + 1],out_ptr);
		  out_ptr += 4;
		}
	      }
	    } else {
	      long long a0 = inp->a0 - inp->pointer;
	      LoadQuad(a0,out_ptr);
	      out_ptr += 8;
	      for (j=0;j<inp->dimct;j++) {
		LoadQuad(inp->m[j],out_ptr);
		out_ptr += 8;
	      }
	      if (inp->aflags.bounds) {
		for (j=0;j<inp->dimct;j++) {
		  LoadQuad(inp->m[inp->dimct + 2 * j],out_ptr);
		  out_ptr += 8;
		  LoadQuad(inp->m[inp->dimct + 2 * j + 1],out_ptr);
		  out_ptr += 8;
		}
	      }
	    }
          }
#ifdef WORDS_BIGENDIAN
          if (in_ptr->dtype != DTYPE_T && in_ptr->dtype != DTYPE_IDENT && in_ptr->dtype != DTYPE_PATH) {
            descriptor_a_arsize i;
            switch (in_ptr->length) {
  	      case 2:  for (i=0; i < inp->arsize; i += in_ptr->length) {LoadShort(inp2[i],out_ptr+i);} break;
              case 4:  for (i=0; i < inp->arsize; i += in_ptr->length) {LoadInt(inp2[i],out_ptr+i);} break;
	      case 8:  for (i=0; i < inp->arsize; i += in_ptr->length) {LoadQuad(inp2[i],out_ptr+i);} break;
              default: memcpy(out_ptr,inp2,inp->arsize);
            }
          }
          else
#endif
            memcpy(out_ptr,inp2,inp->arsize);
          out_ptr += inp->arsize;
        }
	if (class == CLASS_A_SHORT) {
	  bytes_out = 16
	    + (inp->aflags.coeff ? 4 + 4 * inp->dimct : 0)
	    + (inp->aflags.bounds ? 4 * (inp->dimct * 2) : 0) + inp->arsize;
	} else {
	  bytes_out = 20
	    + (inp->aflags.coeff ? 8 + 8 * inp->dimct : 0)
	    + (inp->aflags.bounds ? 8 * (inp->dimct * 2) : 0) + inp->arsize;
	}
	bytes_in = sizeof(struct descriptor_a)
	  + (inp->aflags.coeff ? sizeof(char *) + sizeof(descriptor_a_mult) * inp->dimct : 0)
	  + (inp->aflags.bounds ? sizeof(descriptor_a_bounds) * (inp->dimct*2) : 0) + inp->arsize;
      }
      break;

    /**************************************
    For CA and APD, a0 is the offset.
    **************************************/
     case CLASS_APD:
      {
        array_coeff *inp = (array_coeff *)in_ptr;
	unsigned char class = CLASS_APD_SHORT;
        struct descriptor **dsc = (struct descriptor **)  (((char *)in_ptr) + offset(inp->pointer));
        char *begin = out_ptr;
        char *dscptr;
	int dscsize;
#ifdef BIG_DESC
	if (inp->length >= 0x10000 || inp->arsize >= 0x100000000LL)
	  class = CLASS_APD;
#endif
	num_dsc = inp->arsize / inp->length;
        if (out_ptr) {
	  LoadChar(inp->dtype, out_ptr+2);
          LoadChar(class,out_ptr+3);
	  if (class == CLASS_APD_SHORT) {
	    dscsize = 16 + (inp->aflags.coeff ? 4 + 4 * inp->dimct : 0)
                    + (inp->aflags.bounds ? 4 * (inp->dimct * 2) : 0);
	    short length=sizeof(int);
	    int arsize=sizeof(int)*num_dsc;
	    LoadShort(length, out_ptr);
	    LoadInt(dscsize,out_ptr+4);
	    LoadChar(inp->scale,out_ptr+8);
	    LoadChar(inp->digits,out_ptr+9);
	    set_aflags(out_ptr+10);
	    LoadChar(inp->dimct,out_ptr+11);
	    LoadInt(arsize,out_ptr+12);
	    out_ptr += 16;
	  } else {
	    descriptor_length length=8;
	    descriptor_a_arsize arsize=8*num_dsc;
	    LoadInt(length, out_ptr+4);
	    LoadChar(inp->scale,out_ptr+8);
	    LoadChar(inp->digits,out_ptr+9);
	    set_aflags(out_ptr+10);
	    LoadChar(inp->dimct,out_ptr+11);
	    LoadQuad(arsize,out_ptr+12);
	    out_ptr += 20;
	  }
          if (inp->aflags.coeff) {
	    if (class == CLASS_APD_SHORT) {
	      int a0 = dscsize + inp->a0 - inp->pointer;
	      LoadInt(a0,out_ptr);
	      out_ptr += 4;
	      for (j=0;j<inp->dimct;j++) {
		LoadInt(inp->m[j],out_ptr);
		out_ptr += 4;
	      }
	      if (inp->aflags.bounds) {
		for (j=0;j<inp->dimct;j++) {
		  LoadInt(inp->m[inp->dimct + 2 * j],out_ptr);
		  out_ptr += 4;
		  LoadInt(inp->m[inp->dimct + 2 * j + 1],out_ptr);
		  out_ptr += 4;
		}
	      }
	    } else {
	      long long a0 = inp->a0 - inp->pointer;
	      LoadQuad(a0,out_ptr);
	      out_ptr += 8;
	      for (j=0;j<inp->dimct;j++) {
		LoadQuad(inp->m[j],out_ptr);
		out_ptr += 8;
	      }
	      if (inp->aflags.bounds) {
		for (j=0;j<inp->dimct;j++) {
		  LoadQuad(inp->m[inp->dimct + 2 * j],out_ptr);
		  out_ptr += 8;
		  LoadQuad(inp->m[inp->dimct + 2 * j + 1],out_ptr);
		  out_ptr += 8;
		}
	      }
	    }
          }
          dscptr = out_ptr;
          out_ptr += num_dsc * 4;
          memset(dscptr, 0, num_dsc * 4);
        }
	if (class == CLASS_APD_SHORT) {
	  bytes_out = 16
	    + (inp->aflags.coeff ? 4 + 4 * inp->dimct : 0)
	    + (inp->aflags.bounds ? 4 * (inp->dimct * 2) : 0) + inp->arsize;
	} else {
	  bytes_out = 20
	    + (inp->aflags.coeff ? 8 + 8 * inp->dimct : 0)
	    + (inp->aflags.bounds ? 8 * (inp->dimct * 2) : 0) + inp->arsize;
	}
	bytes_in = sizeof(struct descriptor_a)
	  + (inp->aflags.coeff ? sizeof(char *) + sizeof(descriptor_a_mult) * inp->dimct : 0)
	  + (inp->aflags.bounds ? sizeof(descriptor_a_bounds) * (inp->dimct*2) : 0) + inp->arsize;
      /******************************
      Each descriptor must be copied.
      ******************************/
	for (j = 0; j < num_dsc; j++, bytes_in += sizeof(void *)) {
	  struct descriptor *dptr=((struct descriptor **)in_ptr->pointer)[j];
          if (dptr) {
            status = copy_dx_rec(dptr, out_ptr, &size_out, &size_in, length);
  	    if (out_ptr) {
	      if (class == CLASS_APD_SHORT) {
		int poffset = out_ptr - begin;
		LoadInt(poffset, dscptr + (j * 4));
	      } else {
		long long poffset = out_ptr - begin;
		LoadQuad(poffset,dscptr + (j*8));
	      }
	      out_ptr += size_out;
            }
	    bytes_out += size_out;
            bytes_in += size_in;
          }
	}
      }
      break;

     case CLASS_CA:
      {
        array_coeff *inp = (array_coeff *)in_ptr;
	unsigned char class = CLASS_CA_SHORT;
	descriptor_llength dumlen;
	*length = *length + inp->arsize;
#ifdef BIG_DESC
	if (inp->length >= 0x10000 || inp->arsize >= 0x100000000LL)
	  class = CLASS_CA;
#endif
	if (out_ptr) {
          char *inp2 = inp->pointer;
	  int dscsize;
	  unsigned short len;
          LoadChar(inp->dtype, out_ptr+2);
          LoadChar(class,out_ptr+3);
	  if (class == CLASS_CA_SHORT) {
            int zero=0;
	    len = (unsigned short)inp->length;
	    dscsize = 16 + (inp->aflags.coeff ? 4 + 4 * inp->dimct : 0)
                    + (inp->aflags.bounds ? 4 * (inp->dimct * 2) : 0);
	    LoadShort(inp->length, out_ptr);
            if (inp->pointer) {
	      LoadInt(dscsize,out_ptr+4);
            } else {
              LoadInt(zero,out_ptr+4);
            }
	    LoadChar(inp->scale,out_ptr+8);
	    LoadChar(inp->digits,out_ptr+9);
	    set_aflags(out_ptr+10);
	    LoadChar(inp->dimct,out_ptr+11);
	    LoadInt(inp->arsize,out_ptr+12);
	    out_ptr += 16;
	  } else {
            char pointer_flag=inp->pointer != 0;
	    LoadInt(inp->length, out_ptr+4);
	    LoadChar(inp->scale,out_ptr+8);
	    LoadChar(inp->digits,out_ptr+9);
	    set_aflags(out_ptr+10);
	    LoadChar(inp->dimct,out_ptr+11);
	    LoadQuad(inp->arsize,out_ptr+12);
            LoadChar(pointer_flag,out_ptr+20);
	    out_ptr += 21;
	  }
          if (inp->aflags.coeff) {
	    if (class == CLASS_CA_SHORT) {
	      int a0 = (char *)inp->a0 - (char *)0;
	      LoadInt(a0,out_ptr);
	      out_ptr += 4;
	      for (j=0;j<inp->dimct;j++) {
		LoadInt(inp->m[j],out_ptr);
		out_ptr += 4;
	      }
	      if (inp->aflags.bounds) {
		for (j=0;j<inp->dimct;j++) {
		  LoadInt(inp->m[inp->dimct + 2 * j],out_ptr);
		  out_ptr += 4;
		  LoadInt(inp->m[inp->dimct + 2 * j + 1],out_ptr);
		  out_ptr += 4;
		}
	      }
	    } else {
	      long long a0 = (char *)inp->a0-(char *)0;
	      LoadQuad(a0,out_ptr);
	      out_ptr += 8;
	      for (j=0;j<inp->dimct;j++) {
		LoadQuad(inp->m[j],out_ptr);
		out_ptr += 8;
	      }
	      if (inp->aflags.bounds) {
		for (j=0;j<inp->dimct;j++) {
		  LoadQuad(inp->m[inp->dimct + 2 * j],out_ptr);
		  out_ptr += 8;
		  LoadQuad(inp->m[inp->dimct + 2 * j + 1],out_ptr);
		  out_ptr += 8;
		}
	      }
	    }
          }
	}
	if (class == CLASS_CA_SHORT) {
	  bytes_out = 16
	    + (inp->aflags.coeff ? 4 + 4 * inp->dimct : 0)
	    + (inp->aflags.bounds ? 4 * (inp->dimct * 2) : 0);
	} else {
	  bytes_out = 21
	    + (inp->aflags.coeff ? 8 + 8 * inp->dimct : 0)
	    + (inp->aflags.bounds ? 8 * (inp->dimct * 2) : 0);
	}
	bytes_in = sizeof(struct descriptor_a)
	  + (inp->aflags.coeff ? sizeof(char *) + sizeof(descriptor_a_mult) * inp->dimct : 0)
	  + (inp->aflags.bounds ? sizeof(descriptor_a_bounds) * (inp->dimct*2) : 0);
      /***************************
      Null pointer for shape only.
      ***************************/
	if (inp->pointer) {
	  status = copy_dx_rec((struct descriptor *)inp->pointer, out_ptr, &size_out, &size_in, &dumlen);
	  bytes_out += size_out;
          bytes_in += size_in;
	}
      }
      break;

     default:
      status = LibINVSTRDES;
      break;
    }
  *b_out = bytes_out;
  *b_in  = bytes_in;
  return status;
}

STATIC_ROUTINE int Dsc2Rec(struct descriptor *inp, struct descriptor_xd *out_dsc_ptr, descriptor_llength *reclen, descriptor_llength *length)
{
  descriptor_llength size_out;
  descriptor_llength size_in;
  int       status;
  STATIC_CONSTANT const unsigned char dsc_dtype = DTYPE_B;
  status = copy_dx_rec((struct descriptor *)inp, 0, &size_out, &size_in, length);
  if (status & 1 && size_out)
  {
    descriptor_length nlen = 1;
    array out_template = {DESCRIPTOR_HEAD_INI(1,DTYPE_B,CLASS_A,0),0,0,{0,1,1,0,0},1,0};
    out_template.arsize = *reclen = size_out;
    status = MdsGet1DxA((struct descriptor_a *)&out_template, &nlen, (unsigned char *) &dsc_dtype, out_dsc_ptr);
    if (status & 1)
    {
	  memset(out_dsc_ptr->pointer->pointer,0,size_out);
	  *length=0;
	  status = copy_dx_rec((struct descriptor *)inp, out_dsc_ptr->pointer->pointer, &size_out, &size_in, length);
	}
  }
  else
    MdsFree1Dx(out_dsc_ptr, NULL);
  return status;
}
/*
STATIC_CONSTANT int PointerToOffset(struct descriptor *dsc_ptr, descriptor_llength *length)
{
  int       status = 1;
  if ((dsc_ptr->dtype == DTYPE_DSC) && (dsc_ptr->class != CLASS_A) && (dsc_ptr->class != CLASS_APD))
    status = PointerToOffset((struct descriptor *) dsc_ptr->pointer, length);
  if (status & 1)
  {
    switch (dsc_ptr->class)
    {
     case CLASS_S:
     case CLASS_D:
      *length += sizeof(struct descriptor) + dsc_ptr->length;
      dsc_ptr->pointer = dsc_ptr->pointer - ((char *)dsc_ptr - (char *)0);
      break;
     case CLASS_XD:
     case CLASS_XS:
      *length += sizeof(struct descriptor_xd) + ((struct descriptor_xd *) dsc_ptr)->l_length;
      dsc_ptr->pointer = dsc_ptr->pointer - ((char *)dsc_ptr - (char *)0);
      break;
     case CLASS_R:
      {
	struct descriptor_r *r_ptr = (struct descriptor_r *) dsc_ptr;
	int       i;
	*length += sizeof(*r_ptr) + (r_ptr->ndesc - 1) * sizeof(struct descriptor *)
		   + r_ptr->length;
	if (r_ptr->length != 0)
	  r_ptr->pointer = r_ptr->pointer - ((char *)r_ptr - (char *)0);
	for (i = 0; (status & 1) && (i < r_ptr->ndesc); i++)
	  if (r_ptr->dscptrs[i] != 0)
	  {
	    status = PointerToOffset(r_ptr->dscptrs[i], length);
	    r_ptr->dscptrs[i] = (struct descriptor *) ((char *) r_ptr->dscptrs[i] - ((char *)r_ptr - (char *)0));
	  }
      }
      break;
     case CLASS_A:
      {
	struct descriptor_a *a_ptr = (struct descriptor_a *) dsc_ptr;
	*length += sizeof(struct descriptor_a)
		+ (a_ptr->aflags.coeff ? sizeof(int) * (a_ptr->dimct + 1) : 0)
		+ (a_ptr->aflags.bounds ? sizeof(int) * (a_ptr->dimct * 2) : 0)
		+ a_ptr->arsize;
	a_ptr->pointer = a_ptr->pointer - ((char *)a_ptr - (char *)0);
	if (a_ptr->aflags.coeff)
	{
	  int     *a0_ptr = (int *)((char *) a_ptr + sizeof(struct descriptor_a));
	  *a0_ptr = *a0_ptr - ((char *)a_ptr - (char *)0);
	}
      }
      break;
     case CLASS_APD:
      {
	int       i;
	struct descriptor_a *a_ptr = (struct descriptor_a *) dsc_ptr;
	int       elts = a_ptr->arsize / a_ptr->length;
	*length += sizeof(struct descriptor_a)
		+ (a_ptr->aflags.coeff ? sizeof(int) * (a_ptr->dimct + 1) : 0)
		+ (a_ptr->aflags.bounds ? sizeof(int) * (a_ptr->dimct * 2) : 0)
		+ a_ptr->arsize;
	for (i = 0; (i < elts) && (status & 1); i++)
	{
	  struct descriptor **dsc_ptr = (struct descriptor **) a_ptr->pointer + i;
	  if (dsc_ptr && *dsc_ptr)
	  {
	    status = PointerToOffset(*dsc_ptr, length);
	    *dsc_ptr = (struct descriptor *) ((char *) *dsc_ptr - ((char *)a_ptr - (char *)0));
	  }
	  else
	    status = 1;
	}
	if (status & 1)
	{
	  a_ptr->pointer = a_ptr->pointer - ((char *)a_ptr - (char *)0);
	  if (a_ptr->aflags.coeff)
	  {
	    char     *a0_ptr = (char *) a_ptr + sizeof(struct descriptor_a);
	    *a0_ptr = *a0_ptr - ((char *)a_ptr - (char *)0);
	  }
	}
      }
      break;
     case CLASS_CA:
      if (dsc_ptr->pointer)
      {
	descriptor_llength dummy_length;
	struct descriptor_a *a_ptr = (struct descriptor_a *) dsc_ptr;
	*length += sizeof(struct descriptor_a)
		+ (a_ptr->aflags.coeff ? sizeof(int) * (a_ptr->dimct + 1) : 0)
		+ (a_ptr->aflags.bounds ? sizeof(int) * (a_ptr->dimct * 2) : 0)
		+ a_ptr->arsize;
	status = PointerToOffset((struct descriptor *) dsc_ptr->pointer, &dummy_length);
	a_ptr->pointer = a_ptr->pointer - ((char *)a_ptr - (char *)0);
      }
      break;
     default:
      status = LibINVSTRDES;
      break;
    }
  }
  return status;
}
*/
int MdsSerializeDscOutZ(struct descriptor *in,
	     struct descriptor_xd *out,
             int (*fixupNid)(),
	     void *fixupNidArg,
             int (*fixupPath)(),
             void *fixupPathArg,
             int compress,
             int *compressible_out,
             descriptor_llength *length_out,
             descriptor_llength *reclen_out,
             unsigned char *dtype_out,
             unsigned char *class_out,
             int  altbuflen,
             void *altbuf,
             int *data_in_altbuf_out)
{
  int       status;
  struct descriptor *out_ptr;
  struct descriptor_xd tempxd;
  int compressible = 0;
  descriptor_llength length = 0;
  descriptor_llength reclen = 0;
  unsigned char dtype = 0;
  unsigned char class = 0;
  int data_in_altbuf = 0;
  status = MdsCopyDxXdZ(in, out, 0, fixupNid, fixupNidArg, fixupPath, fixupPathArg);
  if (status == MdsCOMPRESSIBLE)
  {
    if (compress)
    {
      tempxd = *out;
      out->l_length = 0;
      out->pointer = 0;
      status = MdsCompress(0, 0, tempxd.pointer, out);
      MdsFree1Dx(&tempxd, NULL);
      compressible = 0;
    }
    else
      compressible = 1;
  }
  else
    compressible = 0;
  if (status & 1)
  {
    if (out->pointer && out->dtype == DTYPE_DSC)
    {
      out_ptr = out->pointer;
      dtype = out_ptr->dtype;
      if ( (out_ptr->class == CLASS_S || out_ptr->class == CLASS_D) &&
           out_ptr->length < altbuflen)
      {
        data_in_altbuf = 1;
	class = CLASS_S;
	length = out_ptr->length + 8;
#ifdef WORDS_BIGENDIAN
        if (dtype != DTYPE_T && dtype != DTYPE_IDENT && dtype != DTYPE_PATH)
        {
          char *outp = (char *)altbuf;
          char *inp = out_ptr->pointer;
	  switch (out_ptr->length)
	  {
	    case 2:  LoadShort(inp[0],outp); break;
            case 4:  LoadInt(inp[0],outp); break;
            case 8:  LoadQuad(inp[0],outp); break;
	    default: memcpy(outp, inp, out_ptr->length);
	  }
        }
        else
#endif
          memcpy(altbuf, out_ptr->pointer, out_ptr->length);
      }
      else
      {
	data_in_altbuf = 0;
	class = (out_ptr->class == CLASS_XD) ? CLASS_XS : out_ptr->class;
	length = 0;
	/*status = PointerToOffset(out->pointer, &length);*/
        status = 1;
        if (status & 1)
	{ 
	  tempxd = *out;
	  out->l_length = 0;
	  out->pointer = 0;
	  Dsc2Rec(tempxd.pointer,out,&reclen,&length);
	  MdsFree1Dx(&tempxd, NULL);
	}
      }
    }
    else
    {
      length = 0;
      dtype = 0;
      class = 0;
      reclen = 0;
      data_in_altbuf = 1;
    }
  }
  if (compressible_out) *compressible_out = compressible;
  if (length_out) *length_out = length;
  if (reclen_out && (data_in_altbuf != 1)) *reclen_out = reclen;
  if (dtype_out) *dtype_out = dtype;
  if (class_out) *class_out = class;
  if (data_in_altbuf_out) *data_in_altbuf_out = data_in_altbuf;
  return status;
}

int MdsSerializeDscOut(struct descriptor *in,struct descriptor_xd *out)
{
  return MdsSerializeDscOutZ(in,out,0,0,0,0,0,0,0,0,0,0,0,0,0);
}
