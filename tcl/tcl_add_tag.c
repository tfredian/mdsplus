#include        "tclsysdef.h"
#include        <ncidef.h>
#include        <usagedef.h>

/**********************************************************************
* TCL_ADD_TAG.C --
*
* TclAddTag:  Add a tag name to a node
*
* History:
*  30-Jan-1998  TRG  Create.  Ported from original mds code.
*
************************************************************************/



	/*****************************************************************
	 * TclAddTag:
	 * Add a tag name to a node
	 *****************************************************************/
int   TclAddTag()		/* Return: status			*/
   {
    int nid;
    int sts;
    static DYNAMIC_DESCRIPTOR(dsc_nodnam);
    static DYNAMIC_DESCRIPTOR(dsc_tagnam);

    cli_get_value("NODE",&dsc_nodnam);
    cli_get_value("TAGNAME",&dsc_tagnam);
    l2u(dsc_nodnam.pointer,0);
    l2u(dsc_tagnam.pointer,0);
    sts = TreeFindNode(dsc_nodnam.pointer,&nid);
    if (sts & 1)
        sts = TreeAddTag(nid,dsc_tagnam.pointer);
    if (!(sts & 1))
       {
        MdsMsg(sts,"Error adding tag %s",dsc_tagnam.pointer);
#ifdef vms
        lib$signal(sts,0);
#endif
       }
    return sts;
   }
