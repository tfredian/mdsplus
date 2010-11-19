#ifndef HAVE_VXWORKS_H
#include <config.h>
#endif
#include <string.h>
#include <ncidef.h>
#include "treeshrp.h"

unsigned char OldClass(unsigned char newClass) {
  switch (newClass) {
  case CLASS_S: return CLASS_S_SHORT;
  case CLASS_D: return CLASS_D_SHORT;
  case CLASS_XD: return CLASS_XD_SHORT;
  case CLASS_XS: return CLASS_XS_SHORT;
  case CLASS_R: return CLASS_R_SHORT;
  case CLASS_CA: return CLASS_CA_SHORT;
  case CLASS_APD: return CLASS_APD_SHORT;
  case CLASS_A: return CLASS_A_SHORT;
  default: return newClass;
  }
}

unsigned char NewClass(unsigned char oldClass) {
  switch (oldClass) {
  case CLASS_S_SHORT: return CLASS_S;
  case CLASS_D_SHORT: return CLASS_D;
  case CLASS_XD_SHORT: return CLASS_XD;
  case CLASS_XS_SHORT: return CLASS_XS;
  case CLASS_R_SHORT: return CLASS_R;
  case CLASS_CA_SHORT: return CLASS_CA;
  case CLASS_APD_SHORT: return CLASS_APD;
  case CLASS_A_SHORT: return CLASS_A;
  default: return oldClass;
  }
}

void TreeSerializeNciOut(NCI *in, char *out)
{
  char *ptr = out;
  char class;
  memset(out,0,42);
  LoadInt(in->flags,ptr);                           ptr += 4;
  *ptr = in->flags2;                                ptr += 1;
  *ptr = in->length_hi;                             ptr += 1;
  LoadQuad(in->time_inserted,ptr);                  ptr += 8;
  LoadInt(in->owner_identifier,ptr);                ptr += 4;
  
  *ptr = OldClass(in->class);                       ptr += 1;
  *ptr = in->dtype;                                 ptr += 1;
  LoadInt(in->length,ptr);                          ptr += 4;
                                                    ptr += 1;
  LoadInt(in->status,ptr);                          ptr += 4;
  if (in->flags2 & NciM_DATA_IN_ATT_BLOCK)
  {
    *ptr = in->DATA_INFO.DATA_IN_RECORD.element_length;     ptr += 1;
    memcpy(ptr, in->DATA_INFO.DATA_IN_RECORD.data,11);
  }
  else if (in->flags2 & NciM_ERROR_ON_PUT)
  {
    LoadInt(in->DATA_INFO.ERROR_INFO.error_status,ptr);  ptr += 4;
    LoadInt(in->DATA_INFO.ERROR_INFO.stv,ptr);
  }
  else
  {
    *ptr = in->DATA_INFO.DATA_LOCATION.record_length_hi;  ptr += 1;
    *ptr = in->DATA_INFO.DATA_LOCATION.file_version;      ptr += 1;
    memcpy(ptr, in->DATA_INFO.DATA_LOCATION.rfa, 6);      ptr += 6;
    LoadInt(in->DATA_INFO.DATA_LOCATION.record_length,ptr);
  }
}


void TreeSerializeNciIn(char *in, NCI *out)
{
  unsigned char *ptr = in;
  out->flags = swapint(ptr);                                              ptr += 4;
  out->flags2 = *ptr;                                                     ptr += 1;
  out->length_hi = *ptr;                                                  ptr += 1;
  out->time_inserted = swapquad(ptr);                                     ptr += 8;
  out->owner_identifier = swapint(ptr);                                   ptr += 4;
  out->class = NewClass(*ptr);                                            ptr += 1;
  out->dtype = *ptr;                                                      ptr += 1;
  out->length = swapint(ptr);                                             ptr += 4;
                                                                          ptr += 1;
  out->status = swapint(ptr);                                             ptr += 4;
  if (out->flags2 & NciM_DATA_IN_ATT_BLOCK)
  {
    out->DATA_INFO.DATA_IN_RECORD.element_length = *ptr;                  ptr += 1;
    memcpy(out->DATA_INFO.DATA_IN_RECORD.data,ptr,11);
  }
  else if (out->flags2 & NciM_ERROR_ON_PUT)
  {
    out->DATA_INFO.ERROR_INFO.error_status = swapint(ptr);                ptr += 4;
    out->DATA_INFO.ERROR_INFO.stv = swapint(ptr);                         ptr += 4;
  }
  else
  {
    out->DATA_INFO.DATA_LOCATION.record_length_hi = *ptr;                       ptr += 1;
    out->DATA_INFO.DATA_LOCATION.file_version = *ptr;                     ptr += 1;
    memcpy(out->DATA_INFO.DATA_LOCATION.rfa,ptr,6);                       ptr += 6;
    out->DATA_INFO.DATA_LOCATION.record_length = swapint(ptr);
  }
}
    
