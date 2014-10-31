/*
 * Name: KS2115shr.c
 *
 * Author:  Joshua Stillerman
 *
 * Description:  Camshr entry points that use the General Atomics Kinetic Systems 2115 driver
 *   to perform camac operations.
 *  
 * Note:  Only operations needed by mitdevices library are supported.
 * Note:  Name arguments are strings of the form: 
 *             "KSn:c:s"  
 *                 where 
 *                   n - highway number (0)
 *                   c - crate number
 *                   s - slot number (always two digits!)
 *                 e.g.
 *                   "KS0:2:15"  - highway 0, crate 2, slot 15
 */  
#include <string.h>
#include <stdio.h>

#define KS2115_QSTOP_MODE 0
#define KS2115_QIGNORE_MODE 1

typedef struct { 
                unsigned short status;
        unsigned short bytcnt;
        unsigned           fill     : 24;
        unsigned char  scsi_status;
} Iosb;

typedef struct { 
        unsigned short status;
        unsigned short bytcnt;
        unsigned       x        : 1;
        unsigned       q        : 1;
        unsigned       err      : 1;
        unsigned       lpe      : 1;
        unsigned       tpe      : 1;
        unsigned       no_sync  : 1;
        unsigned       tmo      : 1;
        unsigned       adnr     : 1;
        unsigned       list     : 1;
        unsigned       fill     : 7;
        unsigned short lbytcnt;
} TranslatedIosb;
static TranslatedIosb LastIosb;

typedef struct camac_status
{
  int error_status;
  int control_status_register;
  int error_status_register;
  int list_status_register;
  int q_and_x_sum;
  int transaction_count;
  int fortran_index_into_last_list;
  int fortran_index_into_last_data;
  int total_word_count_errors;
  int total_qxe_errors;
} CAMAC_STATUS;

static CAMAC_STATUS last_camac_status; 

extern int CamBytcnt( TranslatedIosb *iosb); 
extern int CamStopw(char *name, unsigned int a, unsigned int f,
                     unsigned int num, void *data, int width, Iosb *iosb);
extern int CamFStopw(char *name, unsigned int a, unsigned int f, 
                     unsigned int num, void *data, int width, Iosb *iosb);
extern int CamGetStat( TranslatedIosb *iosb );
extern int CamPioQrepw(char *name, unsigned int a, unsigned int f, 
                    void *data, int width, Iosb *iosb);
extern int CamPiow(char *name, unsigned int a, unsigned int f,
                    void *data, int width, Iosb *iosb);
extern int CamQ( TranslatedIosb *iosb );
extern int CamQstopw(char *name, unsigned int a, unsigned int f, 
                     unsigned int num, void *data, int width, Iosb *iosb);
extern int CamQrepw(char *name, unsigned int a, unsigned int f,
                     unsigned int num, void *data, int width, Iosb *iosb);extern int CamX( TranslatedIosb *iosb );
extern int CamXandQ( TranslatedIosb *iosb );
/*
 * Dummy entry points for unimplemented routines
 */
#define dummyRtn(name) extern int name() { printf("kscamshr %s not implimented\n","name");return(1);}

dummyRtn(CamAssign)
dummyRtn(CamFQrepw)
dummyRtn(CamFQstopw)
dummyRtn(CamGetMAXBUF)
dummyRtn(CamQscanw)
dummyRtn(CamSetMAXBUF)
dummyRtn(CamStat)
dummyRtn(CamVerbose)
/* 
 *  Interface to KS2115lib
 */
extern int caopen(
    int *chan,
    char *desc1,
    int *status);

extern unsigned int caclos(
    int *chan,
    int *status);

extern int cainterface(
        int *chan,
        short *crate,
        short *slot,
        short *a,
        short *f,
        char *data,
        int wordsize,
        int nwords,
        int tmode,
        int qmode,
        int *status);

#define MAX_DEVICE_NAME 10
static int OpenHighway(int h)
{
  static int chan = -1;
  static int last_h = -1;
  int status;
  char filename[MAX_DEVICE_NAME+1];
  if (chan != -1) {
    if (h != last_h) {
       caclos( &chan, (int *)&last_camac_status);
       chan = -1;
    }
  }
  if (chan == -1) {
    sprintf(filename, "ks%1.1da", h);
    status = caopen(&chan, filename, (int *)&last_camac_status);
  }
  return(chan);
}

#define CAMINVALID 0
static int TranslateName(char *name, int *highway, int *crate, int *slot)
{
  int status = CAMINVALID;
  int cnt;
  int hw;
  int c;
  int s;
  unsigned char hwc;
  
  cnt = sscanf(name, "ks%c%d:%d", &hwc, &c, &s);
  if (cnt == 3) {
    hw=hwc-'a';
    status = 1;
    *highway = hw;
    *crate = c;
    *slot = s;
  } 
  else {
    printf("KS2115 camac names must be of the form ksac:s, where a is {a|b} c is the crate number and s is the slot number.\n");
  }
  return(status);
}

extern int CamQ( TranslatedIosb *iosb )                        // CAM$Q_SCSI()
{
        TranslatedIosb  *iosb_use;
        iosb_use = (iosb) ? iosb : &LastIosb;
        return iosb_use->q;
}

extern int CamX( TranslatedIosb *iosb )                        // CAM$X_SCSI()
{
        TranslatedIosb  *iosb_use;
        iosb_use = (iosb) ? iosb : &LastIosb;
        return iosb_use->x;
}

extern int CamXandQ( TranslatedIosb *iosb )            // CAM$XANDQ_SCSI()
{
        TranslatedIosb  *iosb_use;
        iosb_use = (iosb) ? iosb : &LastIosb;
        return iosb_use->x && iosb_use->q;
}


extern int CamBytcnt( TranslatedIosb *iosb )           // CAM$BYTCNT_SCSI()
{
        TranslatedIosb  *iosb_use;
        iosb_use = (iosb) ? iosb : &LastIosb;
        return ((int)iosb_use->bytcnt) | (((int)iosb_use->lbytcnt) << 16);
}

extern int CamStatus( TranslatedIosb *iosb )           // CAM$STATUS_SCSI()
{
        TranslatedIosb  *iosb_use;
        iosb_use = (iosb) ? iosb : &LastIosb;
        return iosb_use->status;
}

extern int CamGetStat( TranslatedIosb *iosb )          // CAM$GET_STAT_SCSI()
{
        *iosb = LastIosb;
        return 1;
}

extern int CamError( int xexp, int qexp, TranslatedIosb *iosb )
{
        int                             xexp_use;
        int                             qexp_use;
        TranslatedIosb  *iosb_use;

        xexp_use = xexp ? xexp : 0;
        qexp_use = qexp ? qexp : 0;
        iosb_use = iosb ? iosb : &LastIosb;

        iosb_use->err = !iosb_use->x && !iosb_use->q;
        return iosb_use->err;
}

/*
typedef struct {
        unsigned short status;
        unsigned short bytcnt;
        unsigned       x        : 1;
        unsigned       q        : 1;
        unsigned       err      : 1;
        unsigned       lpe      : 1;
        unsigned       tpe      : 1;
        unsigned       no_sync  : 1;
        unsigned       tmo      : 1;
        unsigned       adnr     : 1;
        unsigned       list     : 1;
        unsigned       fill     : 7;
        unsigned short lbytcnt;
} TranslatedIosb;
static TranslatedIosb LastIosb;

typedef struct camac_status
{
  int error_status;
  int control_status_register;
  int error_status_register;
  int list_status_register;
  int q_and_x_sum;
  int transaction_count;
  int fortran_index_into_last_list;
  int fortran_index_into_last_data;
  int total_word_count_errors;
  int total_qxe_errors;
} CAMAC_STATUS;

*/
#define ERR303 0xE1F500A6L
#define NO_Q_ERROR	ERR303
#define ERR305 0xE1F500AAL
#define NO_X_ERROR	ERR305

static int TranslateIosb(int status, CAMAC_STATUS *cstatus, int width)
{
   int bytcnt = cstatus->transaction_count * ((width==16) ? 2 : 3);
   int retstat = status;
   LastIosb.status = cstatus->error_status;
   LastIosb.bytcnt = bytcnt & 0xFFFF;
   LastIosb.x = cstatus->q_and_x_sum > 0;
   LastIosb.q = cstatus->q_and_x_sum > 1;
   LastIosb.lbytcnt = (bytcnt &0xFFFF0000) >> 16;
   if ((retstat == NO_Q_ERROR) || (retstat == NO_X_ERROR))
     retstat = 1;
   return retstat;
}

extern int CamPiow(char *name, unsigned int a, unsigned int f,
                    void *data, int width, Iosb *iosb)
{
  int status;
  int chan;
  int h,c,s;
  if ((status = TranslateName(name, &h, &c, &s)) & 1) 
  {
    chan = OpenHighway(h);
    status = cainterface(
                &chan,
                (short *)&c,
                (short *)&s,
                (short *)&a,
                (short *)&f,
                (char *)data,
                width,
                1,
                1,  /* transfer mode: standard */
                0,  /* q mode 0=qstop, 1=qignore*/
                (int *)&last_camac_status);
    status = TranslateIosb(status, &last_camac_status, width); 
    if (iosb) {
      *iosb=*(Iosb *)&LastIosb;
    }
  }
  printf("CamPiow(%s, a=%d, f=%d,...) returning %d\n", name, a, f, status);
  return status;
}

extern int CamPioQrepw(char *name, unsigned int a, unsigned int f,
                    void *data, int width, Iosb *iosb)
{
  int status;
  int chan;
  int h,c,s;
  if ((status = TranslateName(name, &h, &c, &s)) & 1)
  {
    chan = OpenHighway(h);
    status = cainterface(
                &chan,
                (short *)&c,
                (short *)&s,
                (short *)&a,
                (short *)&f,
                (char *)data,
                width,
                1,
                1,  /* transfer mode: standard */
                2,  /* q mode 0=qstop, 1=qignore 2=qrep */
                (int *)&last_camac_status);
    status = TranslateIosb(status, &last_camac_status, width);
  }
  return status;
}

extern int CamQstopw(char *name, unsigned int a, unsigned int f,
                     unsigned int num, void *data, int width, Iosb *iosb)
{
  int status;
  int chan;
  int h,c,s;
  if ((status = TranslateName(name, &h, &c, &s)) & 1)
  {
    chan = OpenHighway(h);
    status = cainterface(
                &chan,
                (short *)&c,
                (short *)&s,
                (short *)&a,
                (short *)&f,
                (char *)data,
                width,
                num,
                1,  /* transfer mode: standard */
                0,  /* q mode 0=qstop, 1=qignore*/
                (int *)&last_camac_status);
    status = TranslateIosb(status, &last_camac_status, width);
  }
  if (iosb) LastIosb = *(TranslatedIosb *)iosb;
  return status;
}

extern int CamQrepw(char *name, unsigned int a, unsigned int f,
                     unsigned int num, void *data, int width, Iosb *iosb)
{
  int status;
  int chan;
  int h,c,s;
  if ((status = TranslateName(name, &h, &c, &s)) & 1)
  {
    chan = OpenHighway(h);
    status = cainterface(
                &chan,
                (short *)&c,
                (short *)&s,
                (short *)&a,
                (short *)&f,
                (char *)data,
                width,
                num,
                1,  /* transfer mode: standard */
                1,  /* q mode 0=qstop, 1=qignore, 2=qrepeat */
                (int *)&last_camac_status);
    status = TranslateIosb(status, &last_camac_status, width);
  }
  if (iosb) LastIosb = *(TranslatedIosb *)iosb;
  return status;
}


extern int CamFStopw(char *name, unsigned int a, unsigned int f,
                     unsigned int num, void *data, int width, Iosb *iosb)
{
  int status;
  int chan;
  int h,c,s;
  if ((status = TranslateName(name, &h, &c, &s)) & 1)
  {
    chan = OpenHighway(h);
    status = cainterface(
                &chan,
                (short *)&c,
                (short *)&s,
                (short *)&a,
                (short *)&f,
                (char *)data,
                width,
                num,
                2,  /* transfer mode: standard */
                0,  /* q mode 0=qstop, 1=qignore*/
                (int *)&last_camac_status);
    status = TranslateIosb(status, &last_camac_status, width);
  }
  if (iosb) LastIosb = *(TranslatedIosb *)iosb;
  return status;
}

extern int CamStopw(char *name, unsigned int a, unsigned int f,
                     unsigned int num, void *data, int width, Iosb *iosb)
{
  int status;
  int chan;
  int h,c,s;
  if ((status = TranslateName(name, &h, &c, &s)) & 1)
  {
    chan = OpenHighway(h);
    status = cainterface(
                &chan,
                (short *)&c,
                (short *)&s,
                (short *)&a,
                (short *)&f,
                (char *)data,
                width,
                num,
                1,  /* transfer mode: standard */
                0,  /* q mode 0=qstop, 1=qignore*/
                (int *)&last_camac_status);
    status = TranslateIosb(status, &last_camac_status, width);
  }
  if (iosb) LastIosb = *(TranslatedIosb *)iosb;
  return status;
}

int main()
{
  int hw, c, s, status;
  printf("about to call TranslateName\n");
  status = TranslateName("KS01:02:15", &hw, &c, &s);
  printf("got back %d %d %d %d\n", status, hw, c, s);
}
