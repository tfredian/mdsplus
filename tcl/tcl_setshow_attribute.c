#include        "tclsysdef.h"
#include        <mdsshr.h>
#include <strroutines.h>
extern int TdiDecompile();
extern int TdiSortVal();

/**********************************************************************
* TCL_SHOW_ATTRIBUTE.C --
*
* TclShowAttribute:  Display node attributes.
*
*
************************************************************************/


	/***************************************************************
	 * TclShowAttribute:
	 ***************************************************************/
int TclShowAttribute() {
  int status;
  int nid;
  EMPTYXD(xd);
  static DYNAMIC_DESCRIPTOR(dsc_node);
  static DYNAMIC_DESCRIPTOR(dsc_attr);
  static DYNAMIC_DESCRIPTOR(dsc_string);
  cli_get_value("NODE",&dsc_node);
  status = TreeFindNode(dsc_node.pointer,&nid);
  if (status & 1) {
    status = cli_get_value("NAME",&dsc_attr);
    if (status & 1) {
      status = TreeGetXNci(nid,dsc_attr.pointer,&xd);
      if (status & 1) {
	status = TdiDecompile(&xd,&dsc_string MDS_END_ARG);
	if (status & 1) {
	  char *p = malloc(dsc_string.length+1);
	  strncpy(p,dsc_string.pointer,dsc_string.length);
	  p[dsc_string.length]='\0';
	  TclTextOut(p);
	  free(p);
	}
	StrFree1Dx(&dsc_string);
	MdsFree1Dx(&xd,0);
      }
    } else {
      if (TreeGetXNci(nid,"attributenames",&xd)&1) {
	TdiSortVal(&xd,&xd MDS_END_ARG);
	if (xd.pointer && xd.pointer->class == CLASS_A) {
	  typedef ARRAY(char) ARRAY_DSC;
	  ARRAY_DSC *array=(ARRAY_DSC *)xd.pointer;
	  char *name=array->pointer;
	  TclTextOut("Defined attributes for this node:");
	  for (name=array->pointer;name<array->pointer+array->arsize;name+=array->length) {
	    char *out=malloc(array->length+6);
	    sprintf(out,"    %.*s",(int)array->length,name);
	    TclTextOut(out);
	    free(out);
	  }
	} else {
	  TclTextOut("No attributes defined for this node");
	}
	MdsFree1Dx(&xd,0);
      } else {
	TclTextOut("No attributes defined for this node");
      }
      status=1;
    }
  }
  if (!(status & 1)) {
    MdsMsg(status,0);
  }
  return status;
}
extern int TdiCompile();



int TclSetAttribute()
   {
    static DYNAMIC_DESCRIPTOR(dsc_nodnam);
    static DYNAMIC_DESCRIPTOR(dsc_attname);
    static DYNAMIC_DESCRIPTOR(dsc_ascValue);
    static int val;
    static struct descriptor_xd value_xd = {0,DTYPE_DSC,CLASS_XD,0,0};
    int sts;
    int nid;
    int ctx = 0;

    cli_get_value("NODE",&dsc_nodnam);
    cli_get_value("NAME",&dsc_attname);
    sts = TreeFindNode(dsc_nodnam.pointer,&nid);
    if (sts & 1)
       {
        if (cli_present("EXTENDED") & 1)
           {
            static DYNAMIC_DESCRIPTOR(val_part);
            static DYNAMIC_DESCRIPTOR(dsc_eof);
            int use_lf = cli_present("LF") & 1;
            int symbols = cli_present("SYMBOLS") & 1;

            str_free1_dx(&dsc_ascValue);
            str_free1_dx(&dsc_eof);
            cli_get_value("EOF",&dsc_eof);
            while ((mdsdcl_get_input_nosymbols("ATT> ",&val_part) & 1))
               {
                if (dsc_eof.pointer && val_part.pointer)
                   {
                    if (!strcmp(dsc_eof.pointer,val_part.pointer))
                        break;
                   }
                else if (!val_part.length)
                    break;
                if (use_lf)
                    str_concat(&dsc_ascValue,&dsc_ascValue,&val_part,"\n",0);
                else
                    str_append(&dsc_ascValue,&val_part);
               }
            str_free1_dx(&val_part);
           }
        else
            cli_get_value("VALUE",&dsc_ascValue);
        sts = TdiCompile(&dsc_ascValue,&value_xd MDS_END_ARG);
        if (sts & 1)
           {
            if (!value_xd.l_length) value_xd.dtype = DTYPE_DSC;
            sts = TreeSetXNci(nid,dsc_attname.pointer,(struct descriptor *)&value_xd);
           }
       }
    str_free1_dx(&dsc_ascValue);
    str_free1_dx(&dsc_attname);
    MdsFree1Dx(&value_xd,NULL);
    if (~sts & 1)
        MdsMsg(sts,"TclSetAttribute: error processing command");
    return sts;
   }
