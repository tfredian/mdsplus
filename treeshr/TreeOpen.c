#include <stdlib.h>
#include <string.h>
#include <mdsdescrip.h>
#include <mdsshr.h>
#include <treeshr.h>
#include "treeshrp.h"
#include <ctype.h>
#include <mds_stdarg.h>
#include <libroutines.h>
#include <strroutines.h>
#ifdef _WIN32
#include <io.h>
extern char *index(char *str,char c);
#else
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#endif
#include <usagedef.h>
#include <ncidef.h>
#include <errno.h>
#include <fcntl.h>

#ifdef HAVE_VXWORKS_H
#include <ioLib.h>
#endif

#ifdef HAVE_SYS_RESOURCE_H
#ifdef __APPLE__
#include <sys/time.h>
#endif
#include <sys/resource.h>
#endif
#include "treeshrp.h"


#ifdef __toupper
#undef __toupper
#endif
#define __toupper(c) (((c) >= 'a' && (c) <= 'z') ? (c) & 0xDF : (c))
#ifdef __tolower
#undef __tolower
#endif
#define __tolower(c) (((c) >= 'A' && (c) <= 'Z') ? (c) | 0x20 : (c))


static char *cvsrev = "@(#)$RCSfile$ $Revision$ $Date$";

extern char *TranslateLogical(char *);
extern void TranslateLogicalFree(char *);

int treeshr_errno = 0;
#ifndef HAVE_VXWORKS_H
extern int MDSEventCan();
#endif
static void RemoveBlanksAndUpcase(char *out, char *in);
static int CloseTopTree(PINO_DATABASE *dblist, int call_hook);
static int ConnectTree(PINO_DATABASE *dblist, char *tree, NODE *parent, char *subtree_list);
static int CreateDbSlot(PINO_DATABASE **dblist, char *tree, int shot, int editting);
static int OpenTreefile(char *tree, int shot, TREE_INFO *info, int edit_flag, int *nomap, int *fd, int report);
static int MapFile(int fd, TREE_INFO *info, int edit_flag, int nomap);
static int ReadTree(TREE_INFO *info, int edit_flag);
static int GetVmForTree(TREE_INFO *info,int nomap);
static int MapTree(char *tree, int shot, TREE_INFO *info, int edit_flag, int remote);
static void SubtreeNodeConnect(NODE *parent, NODE *subtreetop);

void *DBID = NULL;

int TreeClose(char *tree, int shot) { return _TreeClose(&DBID,tree,shot); }
int TreeEditing() { return _TreeEditing(DBID);}
int TreeGetStackSize() { return _TreeGetStackSize(DBID);}
int TreeIsOpen() { return _TreeIsOpen(DBID);}
int TreeOpen(char *tree, int shot, int read_only) { return _TreeOpen(&DBID,tree,shot,read_only);}
int TreeSetStackSize(int size) { return _TreeSetStackSize(&DBID, size);}
void TreeRestoreContext(void *ctx) { _TreeRestoreContext(&DBID,ctx); }
void *TreeSaveContext() { return _TreeSaveContext(DBID);}
int TreeOpenEdit(char *tree, int shot) { return _TreeOpenEdit(&DBID,tree,shot);}
int TreeOpenNew(char *tree, int shot) { return _TreeOpenNew(&DBID, tree, shot);}

static char *TreePath( char *tree, char *tree_lower_out )
{
  int len = strlen(tree);
  int i;
  char tree_lower[13];
  char pathname[32];
  char *path;
  for (i=0;i<len && i < 12;i++) tree_lower[i] = __tolower(tree[i]);
  tree_lower[i]=0;
  strcpy(pathname,tree_lower);
  strcat(pathname,TREE_PATH_SUFFIX);
  if (tree_lower_out)
    strcpy(tree_lower_out,tree_lower);
  path = TranslateLogical(pathname);
  if (path)
  {
    for (i=strlen(path)-1;i>=0 && (path[i]==32 || path[i]==9);i--) 
      path[i]=0;
  }
  return path;
}

int _TreeOpen(void **dbid, char *tree_in, int shot_in, int read_only_flag)
{
  int       status = TreeFAILURE;
  int       shot;
  char     *tree = malloc(strlen(tree_in)+1);
  char     *subtree_list = 0;
  char     *comma_ptr;

  RemoveBlanksAndUpcase(tree,tree_in);
  if (comma_ptr = strchr(tree, ','))
  {
    subtree_list = strcpy(malloc(strlen(tree)+1),tree);
    *comma_ptr='\0';
  }

/**************************************************
 To open a tree we need only to connect up the MAIN
 tree which will in turn link up any subtrees.
**************************************************/

  shot = shot_in ? shot_in : TreeGetCurrentShotId(tree);
  if (shot)
  {
    char *path = TreePath(tree,0);
    if (path)
    {
      PINO_DATABASE **dblist = (PINO_DATABASE **)dbid;
      int db_slot_status = CreateDbSlot(dblist, tree, shot, 0);
      if (db_slot_status == TreeNORMAL || db_slot_status == TreeALREADY_OPEN)
      {
        if (strlen(path) > 2 && path[strlen(path)-2] == ':' && path[strlen(path)-1] == ':')
          status = ConnectTreeRemote(*dblist, tree, subtree_list, path, status);
        else
          status = ConnectTree(*dblist, tree, 0, subtree_list);
        if (status == TreeNORMAL || status == TreeNOTALLSUBS)
        {
          if (db_slot_status == TreeNORMAL)
            (*dblist)->default_node = (*dblist)->tree_info->root;
          (*dblist)->open = 1;
          (*dblist)->open_readonly = read_only_flag;
        }
        else
        {
          PINO_DATABASE *db;
          for (db = *dblist; db->next; db=db->next);
          if (db)
          {
            db->next = *dblist;
            *dblist = (*dblist)->next;
            db->next->next = 0;
          }
        }
      }
      TranslateLogicalFree(path);
    }
  }
  if (subtree_list)
  free(subtree_list);
  free(tree);
  return status;
}

static void RemoveBlanksAndUpcase(char *out, char *in)
{
  while(*in)
  {                          
  char c = *in++;
  if (c != (char)32 && c != (char)9)
   *out++=__toupper(c);
 }
 *out=0;
}

int _TreeClose(void **dbid, char *tree, int shot)
{
  PINO_DATABASE **dblist = (PINO_DATABASE **)dbid;
  int       status;
  PINO_DATABASE *db;
  PINO_DATABASE *prev_db;
  status = TreeNOT_OPEN;
  if (dblist && *dblist)
  {
    if (tree)
    {
      char uptree[13];
      int i;
      int len = strlen(tree);
      for (i=0;i<12 && i<len;i++)
      uptree[i] = __toupper(tree[i]);
      uptree[i]='\0';
      status = TreeNOT_OPEN;
      if (!shot)
        shot = TreeGetCurrentShotId(tree);
      for (prev_db = 0, db = *dblist; db ? db->open : 0; prev_db = db, db = db->next)
      {
        if ((shot == db->shotid) && (strcmp(db->main_treenam, uptree) == 0))
        {
          if (prev_db)
          {
            prev_db->next = db->next;
            db->next = *dblist;
            *dblist = db;
          }
          status = TreeNORMAL;
          break;
        }
      }
    }
    else
      status = TreeNORMAL;
    if (status & 1)
    {
      if ((*dblist)->modified)
      {
        status = TreeWRITEFIRST;
      }
      else if ((*dblist)->open)
      {
        status = CloseTopTree(*dblist,1);
        if (status & 1)
        {
          for (prev_db = 0, db = *dblist; db; prev_db = db, db = db->next);
          if (prev_db && (*dblist)->next)
          {
            db = (*dblist)->next;
            (*dblist)->next = 0;
            prev_db->next = *dblist;
            *dblist = db;
          }
        }
      }
      else
        status = TreeNOT_OPEN;
    }
  }
  return status;
}

static int CloseTopTree(PINO_DATABASE *dblist, int call_hook)
{
  TREE_INFO *local_info = dblist ? dblist->tree_info : 0;
  TREE_INFO *previous_info;
  int       status;

  status = TreeNORMAL;
  if (dblist)
  {
    if (dblist->remote)
    {
      status = CloseTreeRemote(dblist, call_hook);
      if (status == TreeNOT_OPEN)  /**** Remote server might have already opened the tree ****/
	status = TreeNORMAL;
    }
    else if (local_info)
    {

 /************************************************
     We check the BLOCKID just to make sure that what
     we were passed in indeed was a tree info block.
     This is a safety check because if we just assumed
     the block was correct, mapping and freeing random
     pages of memory would cause serious problems that
     may not show up immediately.
    *************************************************/

    if (local_info->blockid == TreeBLOCKID)
    {

      /******************************************************
       If the edit pointer field of this tree is non-zero,
       the tree has been opened for editting. The edit pointer
       points to a structure containing the number of virtual
       memory pages that were dynamically allocated for each
       section of the tree. If any of the tree sections had
       virtual memory allocated, this memory must be freed.
      *****************************************************/

      if (local_info->edit)
      {
        static int tree_edit_size = sizeof(TREE_EDIT);
        if (local_info->edit->header_pages)
        free(local_info->header);
        if (local_info->edit->node_vm_size)
          free(local_info->node);
        if (local_info->edit->tags_pages)
          free(local_info->tags);
        if (local_info->edit->tag_info_pages)
          free(local_info->tag_info);
        if (local_info->edit->external_pages)
          free(local_info->external);
        free(local_info->edit);
      }

       /********************************************************
       For each tree in the linked list, first the pages must be
       remapped to the pagefile using sys$cretva before they can
       be freed for general use. This automatically unmaps the
       tree section file pages. Once the section is unmapped the
       I/O channel can be deassigned. Then the virtual memory
       can be deallocated using lib$free_vm_page.
       Finally the tree information block memory can be released.
       *********************************************************/

      while (local_info)
      {
        if (local_info->blockid == TreeBLOCKID)
        {
#ifndef HAVE_VXWORKS_H
          if (local_info->rundown_id)
          MDSEventCan(local_info->rundown_id);
#endif
          if (local_info->section_addr[0])
          {
#if (!defined(HAVE_WINDOWS_H) && !defined(HAVE_VXWORKS_H))
            if (local_info->channel)
            { int status;
              MDS_IO_CLOSE(local_info->channel);
              status = munmap(local_info->section_addr[0],local_info->alq * 512);
            }
#endif
            if (local_info->vm_addr)
              free(local_info->vm_addr);
            }
            TreeWait(local_info);
            if (local_info->data_file)
            {
              MdsFree1Dx(local_info->data_file->data,NULL);
              if (local_info->data_file->get)
                MDS_IO_CLOSE(local_info->data_file->get);
              if (local_info->data_file->put)
                MDS_IO_CLOSE(local_info->data_file->put);
              free(local_info->data_file);
              local_info->data_file = NULL;
            }
            if (local_info->nci_file)
            {
              if (local_info->nci_file->get)
                MDS_IO_CLOSE(local_info->nci_file->get);
              if (local_info->nci_file->put)
                MDS_IO_CLOSE(local_info->nci_file->put);
              free(local_info->nci_file);
              local_info->nci_file = NULL;
            }
            if (call_hook) 
              TreeCallHook(CloseTree, local_info,0);
            if (local_info->filespec)
              free(local_info->filespec);
            if (local_info->treenam)
              free(local_info->treenam);
            previous_info = local_info;
            local_info = local_info->next_info;
            free(previous_info);
          }
        }
        dblist->tree_info = 0;
      }
      else
        status = TreeINVTREE;
    }
    if (status & 1)
    {
      dblist->open = 0;
      dblist->open_for_edit = 0;
      dblist->modified = 0;
      dblist->remote = 0;
      free(dblist->experiment);
      dblist->experiment = 0;
      free(dblist->main_treenam);
      dblist->main_treenam = 0;
    }
  }
  return status;
}

int _TreeIsOpen(void *dbid)
{
 PINO_DATABASE *dblist = (PINO_DATABASE *)dbid;
 return IS_OPEN_FOR_EDIT(dblist) ? TreeOPEN_EDIT : (IS_OPEN(dblist) ? TreeOPEN : TreeNOT_OPEN);
}

int _TreeEditing(void *dbid)
{
 PINO_DATABASE *dblist = (PINO_DATABASE *)dbid;
 return IS_OPEN_FOR_EDIT(dblist) ? TreeNORMAL : TreeNOEDIT;
}

static int ConnectTree(PINO_DATABASE *dblist, char *tree, NODE *parent, char *subtree_list)
{
  int       status = TreeNORMAL;
  int       ext_status;
  int       i;
  TREE_INFO *info;
  TREE_INFO *iptr;


/***********************************************
  If the parent's usage is not subtree then
  just return success.
************************************************/

  if (parent && parent->usage!=TreeUSAGE_SUBTREE)
    return TreeNORMAL;

/***********************************************
  If there is a treelist (canditates) then if 
  this tree is not in it then just return 
  success.
************************************************/

  if (subtree_list)
    {
      char *found;
      char *tmp_list = malloc(strlen(subtree_list)+3);
      char *tmp_tree = malloc(strlen(tree)+3);
      int llen = strlen(subtree_list);
      int slen = strlen(tree);
      strcpy(tmp_list,",");
      strcat(tmp_list,subtree_list);
      strcat(tmp_list,",");
      strcpy(tmp_tree,",");
      strcat(tmp_tree,tree);
      strcat(tmp_tree,",");
      found = strstr(tmp_list,tmp_tree);
      free(tmp_list);
      free(tmp_tree);
      if (!found)
 return TreeNOT_IN_LIST;
    }

/***********************************************
  Get virtual memory for the tree
  information structure and zero the structure.
***********************************************/

  for (info = dblist->tree_info; info && strcmp(tree,info->treenam); info = info->next_info);
  if (!info)
    {
      info = malloc(sizeof(TREE_INFO));
      if (info)
 {
   memset(info,0,sizeof(*info));
   
   /***********************************************
   Next we map the file and if successful copy
   the tree name (blank filled) into the info block.
   ***********************************************/
   
   info->flush = (dblist->shotid == -1);
   info->treenam = strcpy(malloc(strlen(tree)+1),tree);
   info->shot = dblist->shotid;
   status = MapTree(tree, dblist->shotid, info, 0, parent == 0);
   if (status == TreeFAILURE && treeshr_errno == TreeFILE_NOT_FOUND)
   {
     status = TreeCallHook(RetrieveTree, info,0);
     if (status == TreeNORMAL)
       status = MapTree(tree, dblist->shotid, info, 0, parent == 0);
   }
   if (status == TreeNORMAL)
   {
     TreeCallHook(OpenTree, info,0);

      /**********************************************
       If this is the main tree the root node is the
       first node in the tree. If it is a subtree we
       must make the parent/subtree node connections
       and the referencing node is replaced by
       the root node of the subtree.
      **********************************************/

     info->root = info->node;
     if (parent == 0)
     {
       dblist->tree_info = info;
              dblist->remote = 0;
            }
     else
       {
  SubtreeNodeConnect(parent, info->node);
  for (iptr = dblist->tree_info; iptr->next_info; iptr = iptr->next_info);
  iptr->next_info = info;
       }

      /***********************************************
       For each of the external references (subtrees),
       We make a string descriptor for the subtree
       name using the node name of the referencing node
       and then recursively call this routine to connect
       the subtree(s).
      *************************************************/
   }
   if (!(status & 1) && info)
   {
     if (info->treenam)
       free(info->treenam);
     free(info);
     info = 0;
   }
      }
  }
  if (info)
  {
    for (i = 0; i < info->header->externals; i++)
    {
      NODE     *external_node = info->node + swapint((char *)&info->external[i]);
      char *subtree = strncpy(malloc(sizeof(NODE_NAME)+1),external_node->name,sizeof(NODE_NAME));
      char *blank = strchr(subtree,32);
      subtree[sizeof(NODE_NAME)] = '\0';
      if (blank) *blank = 0;
      ext_status = ConnectTree(dblist, subtree, external_node, subtree_list);
      free(subtree);
      if (!(ext_status & 1))
      {
        status = TreeNOTALLSUBS;
        if (treeshr_errno == TreeCANCEL) 
          break;
      }
    }
  }
  return status;
}

#define move_to_top(prev_db,db)\
  if (prev_db) \
            {prev_db->next = db->next;\
            db->next = *dblist;\
            *dblist = db;}

static int CreateDbSlot(PINO_DATABASE **dblist, char *tree, int shot, int editting)
{
  int       status;
  PINO_DATABASE *db;
  PINO_DATABASE *prev_db;
  PINO_DATABASE *saved_prev_db;
  PINO_DATABASE *useable_db = 0;
  int       count;
  int     stack_size = 8;
  enum options
  {
    MOVE_TO_TOP, CLOSE, ERROR_DIRTY, OPEN_NEW
  };
  enum options option = OPEN_NEW;

  if (*dblist) 
    stack_size = (*dblist)->stack_size;
  for (prev_db = 0, db = *dblist; db ? db->open : 0; prev_db = db, db = db->next)
  {
    if (db->shotid == shot)
    {
      if (strcmp(db->main_treenam, tree) == 0)
      {
        if (editting)
        {
          if (db->open_for_edit)
          {
            option = MOVE_TO_TOP;
            break;
          }
          else
          {
            option = CLOSE;
            break;
          }
        }
        else
        {
          if (db->open_for_edit)
          {
            if (db->modified)
            {
              option = ERROR_DIRTY;
              break;
            }
            else
            {
              option = CLOSE;
              break;
            }
          }
          else
          {
            option = MOVE_TO_TOP;
            break;
          }
        }
      }
    }
  }
  switch (option)
  {
  case MOVE_TO_TOP:
    move_to_top(prev_db, db);
    status = TreeALREADY_OPEN;
    break;

  case CLOSE:
    move_to_top(prev_db, db);
    _TreeClose((void **)dblist,0,0);
    status = TreeNORMAL;
    break;
  case ERROR_DIRTY:
    status = TreeFAILURE;
    treeshr_errno = TreeWRITEFIRST;
    break;
  default:

    for (count = 0, prev_db = 0, db = *dblist; db; count++, prev_db = db, db = db->next)
    {
      if (!db->open)
      {
        move_to_top(prev_db, db);
        status = TreeNORMAL;
        break;
      }
      else if (!(db->open_for_edit && db->modified))
      {
        saved_prev_db = prev_db,
          useable_db = db;
      }
    }
    if (!db)
    {
      if (count >= stack_size)
      {
        if (useable_db)
        {
          move_to_top(saved_prev_db, useable_db);
          _TreeClose((void **)dblist,0,0);
          move_to_top(saved_prev_db, useable_db);
          status = TreeNORMAL;
        }
        else
        {
          status = TreeFAILURE;
          treeshr_errno = TreeMAXOPENEDIT;
        }
      }
      else
      {
        db = *dblist;
        *dblist=malloc(sizeof(PINO_DATABASE));
        if (*dblist)
        {
          memset(*dblist,0,sizeof(PINO_DATABASE));
          (*dblist)->next = db;
          status = TreeNORMAL;
        }
      }
    }
  }
  if (status == TreeNORMAL)
  {
    (*dblist)->shotid = shot;
    (*dblist)->setup_info = (shot == -1);
    if ((*dblist)->experiment)
      free((*dblist)->experiment);
    (*dblist)->experiment = strcpy(malloc(strlen(tree)+1),tree);
    if ((*dblist)->main_treenam)
      free((*dblist)->main_treenam);
    (*dblist)->main_treenam = strcpy(malloc(strlen(tree)+1),tree);
    (*dblist)->stack_size = stack_size;
  }
  return status;
}

int _TreeGetStackSize(void *dbid)
{
  PINO_DATABASE *dblist = (PINO_DATABASE *)dbid;
  return dblist->stack_size;
}

int _TreeSetStackSize(void **dbid, int size)
{
  PINO_DATABASE *dblist = *(PINO_DATABASE **)dbid;
  int new_size = size > 0 ? (size < 11 ? size : 10) : 1;
  int old_size = dblist ? dblist->stack_size : 8;
  if (!dblist)
    CreateDbSlot((PINO_DATABASE **)dbid, "", 987654321, 0);
  for (dblist = *(PINO_DATABASE **)dbid; dblist; dblist = dblist->next) dblist->stack_size = new_size;
  if (dblist && dblist->remote) SetStackSizeRemote(dblist,new_size);
  return old_size;
}
static char TreeMask[13] = "TREE";
static char ShotMask[11] = "JIHGFEDCBA";

static char *GetFname(char *tree, int shot)
{
  int status = 1;
  static char *ans = 0;
  struct descriptor fname = {0, DTYPE_T, CLASS_D, 0};
  void *arglist[4];
  char expression[128];
  static void *TdiExecute = 0;
  struct descriptor expression_d = {0, DTYPE_T, CLASS_S, 0};
  if (ans)
  {
    free(ans);
    ans = 0;
  }
  expression_d.length = sprintf(expression,"%s_tree_filename(%d)",tree,shot);
  expression_d.pointer = expression;
  arglist[0] = (void *)3;
  arglist[1] = &expression_d;
  arglist[2] = &fname;
  arglist[3] = MdsEND_ARG;
  if (TdiExecute == 0)
  {
    static DESCRIPTOR(image,"TdiShr");
    static DESCRIPTOR(routine,"TdiExecute");
    status = LibFindImageSymbol(&image,&routine,&TdiExecute);
  }
  if (status & 1)
    status = LibCallg(arglist,TdiExecute);
  if (status & 1)
  {
    ans = strncpy(malloc(fname.length+2),fname.pointer,fname.length);
    ans[fname.length] = '+';
    ans[fname.length+1] = 0;
  }
  else
  {
    ans = strcpy(malloc(6),"ErRoR");
  }
  StrFree1Dx(&fname);
  return(ans);
}

char *MaskReplace(char *path_in,char *tree,int shot)
{
  char *path = strcpy(malloc(strlen(path_in)+1),path_in);
  char ShotMask[13];
  char *tilde;
  char *fname;
  int replace_tilde = 0;
  unsigned int i; 
  if (shot > 0)
    sprintf(ShotMask,"%012u",shot);
  else
    memset(ShotMask,'X',12);
  for(tilde=(char *)index(path, '~'); tilde != 0; tilde=(char *)index(path, '~'))
  {
    char *tmp,*tmp2;
    switch (tilde[1])
    {
    case 'a':  tilde[0]=ShotMask[11]; strcpy(&tilde[1],&tilde[2]); break;
    case 'b':  tilde[0]=ShotMask[10]; strcpy(&tilde[1],&tilde[2]); break;
    case 'c':  tilde[0]=ShotMask[9]; strcpy(&tilde[1],&tilde[2]); break;
    case 'd':  tilde[0]=ShotMask[8]; strcpy(&tilde[1],&tilde[2]); break;
    case 'e':  tilde[0]=ShotMask[7]; strcpy(&tilde[1],&tilde[2]); break;
    case 'f':  tilde[0]=ShotMask[6]; strcpy(&tilde[1],&tilde[2]); break;
    case 'g':  tilde[0]=ShotMask[5]; strcpy(&tilde[1],&tilde[2]); break;
    case 'h':  tilde[0]=ShotMask[4]; strcpy(&tilde[1],&tilde[2]); break;
    case 'i':  tilde[0]=ShotMask[3]; strcpy(&tilde[1],&tilde[2]); break;
    case 'j':  tilde[0]=ShotMask[2]; strcpy(&tilde[1],&tilde[2]); break;
    case 'n':  fname = GetFname(tree,shot);
      if ((strlen(tilde+2) > 1) || ((strlen(tilde+2) == 1) && (tilde[2] != '\\')))
      {
        tmp = strcpy(malloc(strlen(tilde+2)+1),tilde+2);
        tmp2 = strcpy(malloc(strlen(path)+1+strlen(fname)),path);
        strcpy(tmp2 +(tilde-path)+strlen(fname), tmp);
        free(tmp);
        strncpy(tmp2+(tilde-path),fname,strlen(fname));
      }
      else
      {
        tmp2 = strcpy(malloc(strlen(path)+1+strlen(fname)),path);
        strcpy(tmp2+(tilde-path),fname);
      }
      free(path);
      path=tmp2;
      break;
    case 't':  tmp = strcpy(malloc(strlen(tilde+2)+1),tilde+2);
      tmp2 = strcpy(malloc(strlen(path)+1+strlen(tree)),path);
      strcpy(tmp2 +(tilde-path)+strlen(tree), tmp);
      free(tmp);
      strncpy(tmp2+(tilde-path),tree,strlen(tree)); 
      free(path);
      path=tmp2;
      break;
    default: tilde[0]=1;
      replace_tilde = (char)1;
      break;
    }
  }
  if (replace_tilde)
  {
    for (i=0;i<strlen(path);i++) if (path[i] == (char)1) path[i] = '~';
  }
  return path;
}  


static int  OpenOne(TREE_INFO *info, char *tree, int shot, char *type,int new,char **resnam_out, int report, int edit_flag)
{
  int fd = -1;
  char *path;
  char name[32];
  int i;
  char tree_lower[13];
  char *resnam = 0;
#ifdef HAVE_SYS_RESOURCE_H
  static int initialized = 0;
  if (!initialized)
  {
    struct rlimit rlp;
    getrlimit(RLIMIT_NOFILE,&rlp);
    rlp.rlim_cur = rlp.rlim_max;
    setrlimit(RLIMIT_NOFILE,&rlp);
    initialized = 1;
  }
#endif
  path = TreePath(tree,tree_lower);
  if (path)
  {
    char *part;
    int pathlen = strlen(path);
    char *npath;
    npath = MaskReplace(path,tree_lower,shot);
    TranslateLogicalFree(path);
    path = npath;
    pathlen = strlen(path);
    if (shot < 0)
      sprintf(name,"%s_model",tree_lower);
    else if (shot < 1000)
      sprintf(name,"%s_%03d",tree_lower,shot);
    else
      sprintf(name,"%s_%d",tree_lower,shot);
    for (i=0,part=path;i<pathlen+1 && fd==-1;i++)
    {
      if (*part == ' ') 
        part++;
      else if ((path[i] == ';' || path[i] == 0) && strlen(part))
      {
        path[i] = 0;
        resnam = strcpy(malloc(strlen(part)+strlen(name)+strlen(type)+2),part);
        if (resnam[strlen(resnam)-1] == '+')
          resnam[strlen(resnam)-1] = '\0';
        else
        {
          if (strcmp(resnam+strlen(resnam)-1,TREE_PATH_DELIM))
            strcat(resnam,TREE_PATH_DELIM);
          strcat(resnam,name);
        }
        strcat(resnam,type);
        info->channel = 0;
        if (new)
        {
          fd = MDS_IO_OPEN(resnam,O_RDWR | O_CREAT, 0777);
        }
        else
        {
          fd = MDS_IO_OPEN(resnam,edit_flag ? O_RDWR : O_RDONLY,0);
#if (defined(__osf__) || defined(__linux) || defined(__hpux) || defined(__sun) || defined(__sgi) || defined(_AIX) || defined(__APPLE__)) && !defined(HAVE_VXWORKS_H)
          info->channel = (MDS_IO_SOCKET(fd) == -1) ? fd : 0;
#endif
        }
        if (fd == -1)
        {
          free(resnam);
          resnam = NULL;
        }
        part = &path[i+1];
      }
    }
    /*
    if (file == NULL && report)
    {
    perror("Error opening tree file");
    }
    */
    free(path);
  }
  if (fd != -1 && strcmp(type,TREE_TREEFILE_TYPE) == 0 && edit_flag)
  {
    if (!(MDS_IO_LOCK(fd,1,1,10) & 1))
    {
      MDS_IO_CLOSE(fd);
      fd = -2;
    }
  }   
  if (resnam_out)
    *resnam_out = fd >= 0 ? resnam : 0;
  else if (resnam)
    free(resnam);
  return fd;
}

static int MapTree(char *tree, int shot, TREE_INFO *info, int edit_flag, int report)
{
  int       status;
  int       nomap;
  int     fd;


  /******************************************
  First we need to open the tree file.
  If successful, we create and map a global
  section on the tree file.
  *******************************************/

  status = OpenTreefile(tree, shot, info, edit_flag, &nomap, &fd, report);
  if (status == TreeNORMAL)
    status = MapFile(fd, info, edit_flag, nomap);
  return status;
}

static int OpenTreefile(char *tree, int shot, TREE_INFO *info, int edit_flag, int *nomap, int *fd, int report)
{
  int       status;

  char *resnam;
  *fd = OpenOne(info, tree, shot, TREE_TREEFILE_TYPE, 0, &resnam, report, edit_flag);
  switch (*fd)
  {
  case -1: status = TreeFILE_NOT_FOUND; break;
  case -2: status = TreeOPEN_EDIT & 0xfffffffa; break;
  default:
    info->alq = (int)MDS_IO_LSEEK(*fd, 0, SEEK_END) / 512;
    MDS_IO_LSEEK(*fd, 0, SEEK_SET);
    status = TreeNORMAL;
    info->filespec=resnam;
    *nomap = info->channel == 0;
  }
  return status;
}

static void SwapBytesInt(char *in)
{
  char tmp = in[0];
  in[0] = in[3];
  in[3] = tmp;
  tmp = in[1];
  in[1] = in[2];
  in[2] = tmp;
}
  
#ifdef WORDS_BIGENDIAN
void FixupHeader(TREE_HEADER *hdr)
{
  char flags = ((char *)hdr)[1];
  hdr->sort_children = (flags & 1) != 0;
  hdr->sort_members = (flags & 2) != 0;
  SwapBytesInt((char *)&hdr->free);
  SwapBytesInt((char *)&hdr->tags);
  SwapBytesInt((char *)&hdr->externals);
  SwapBytesInt((char *)&hdr->nodes);
}
#else
#define FixupHeader(arg)
#endif

static int MapFile(int fd, TREE_INFO *info, int edit_flag, int nomap)
{
  int       status;

  /********************************************
  First we get virtual memory for the tree to
  be mapped into.
  ********************************************/

  status = GetVmForTree(info,nomap);
  if (status == TreeNORMAL)
  {
    if (nomap)
    {
      MDS_IO_READ(fd,(void *)info->section_addr[0], 512 * info->alq);
      MDS_IO_CLOSE(fd);
      status = 1;
    }
#if (!defined (HAVE_WINDOWS_H) && !defined(HAVE_VXWORKS_H))
    else
    {
#ifndef MAP_FILE
#define MAP_FILE 0
#endif
      info->section_addr[0] = mmap(0,info->alq * 512,PROT_READ | PROT_WRITE, MAP_FILE | MAP_PRIVATE, MDS_IO_FD(info->channel), 0);
      status = info->section_addr[0] != (void *)-1;
      if (!status)
        perror("Error mapping file");

    }
#endif
    /***********************************************
    If we successfully mapped the file, see if
    we got all of the file and the return addresses
    match what we asked for.
    If ok, fill in the addresses of the various
    parts of the tree based on the information in
    the tree header.
    ************************************************/

    if (status & 1)
    {
      info->blockid = TreeBLOCKID;
      info->header = (TREE_HEADER *) info->section_addr[0];
      FixupHeader(info->header);
      info->node = (NODE *) (info->section_addr[0] + ((sizeof(TREE_HEADER) + 511) / 512) * 512);
      info->tags = (int *) (((char *)info->node) + ((info->header->nodes * sizeof(NODE) + 511) / 512) * 512);
      info->tag_info = (TAG_INFO *) (((char *)info->tags) + ((info->header->tags * 4 + 511) / 512) * 512);
      info->external = (int *) (((char *)info->tag_info) + ((info->header->tags * sizeof(TAG_INFO) + 511) / 512) * 512);
      TreeEstablishRundownEvent(info);
      status = TreeNORMAL;
    }
    else if (info->vm_addr)
      free(info->vm_addr);
  }
  return status;
}

static int GetVmForTree(TREE_INFO *info,int nomap)
{
  int status = TreeNORMAL;
  info->vm_pages = info->alq;
  if (nomap)
  {
    info->vm_addr = malloc(info->vm_pages*512);
    if (info->vm_addr)
      info->section_addr[0] = info->vm_addr;
    else
      status = TreeFAILURE;
  }
  else
    info->vm_addr = 0;
  return status;
}


static void SubtreeNodeConnect(NODE *parent, NODE *subtreetop)
{
  NODE     *grandparent = parent_of(parent);

  /*************************************************
  We must connect the parent node to its child
  node by modifying the child index of the parent.
  Before this field can be modified we must make
  the page of memory modifiable. After modification
  the page will be set back to readonly.
  *************************************************/

  if (child_of(grandparent) == parent)
  {
    link_it(grandparent->child,subtreetop, grandparent);
  }
  else
  {
    NODE     *bro;
    for (bro = child_of(grandparent); brother_of(bro) && (brother_of(bro) != parent); bro = brother_of(bro));
    if (brother_of(bro))
    {
      link_it(bro->brother, subtreetop, bro);
    }
  }
  memcpy(subtreetop->name, parent->name, sizeof(subtreetop->name));
  link_it(subtreetop->parent, grandparent, subtreetop);
  if (parent->brother)
  {
    link_it(subtreetop->brother, brother_of(parent), subtreetop);
  }
  return;
}

struct context
{
  char *expt;
  int  shot;
  int  edit;
  int  readonly;
  int  defnid;
};

void *_TreeSaveContext(void *dbid)
{
  PINO_DATABASE *dblist = (PINO_DATABASE *)dbid;
  struct context *ctx = NULL;
  if (IS_OPEN(dblist))
  {
    ctx = malloc(sizeof(struct context));
    ctx->expt = strcpy(malloc(strlen(dblist->experiment)+1),dblist->experiment);
    ctx->shot = dblist->shotid;
    ctx->edit = dblist->open_for_edit;
    ctx->readonly = dblist->open_readonly;
    _TreeGetDefaultNid(dbid,&ctx->defnid);
  }
  return (void *)ctx;
}

void _TreeRestoreContext(void **dbid, void *ctxin)
{
  if (ctxin)
  {
    struct context *ctx = (struct context *)ctxin;
    if (ctx->edit)
      _TreeOpenEdit(dbid, ctx->expt, ctx->shot);
    else
      _TreeOpen(dbid, ctx->expt, ctx->shot, ctx->readonly);
    _TreeSetDefaultNid(*dbid, ctx->defnid);
    free(ctx->expt);
    free(ctx);
  }
}

int TreeCloseFiles(TREE_INFO *info)
{
  int       status;
  status = TreeNORMAL;
  if (info)
  {
    if (info->blockid == TreeBLOCKID)
    {
      info->reopen = 0;
      TreeWait(info);
      if (info->data_file)
      {
        MdsFree1Dx(info->data_file->data, NULL);
        if (info->data_file->get)    MDS_IO_CLOSE(info->data_file->get);
        if (info->data_file->put)    MDS_IO_CLOSE(info->data_file->put);
        free(info->data_file);
        info->data_file = NULL;
      }
      if (info->nci_file)
      {
        if (info->nci_file->get)    MDS_IO_CLOSE(info->nci_file->get);
        if (info->nci_file->put)    MDS_IO_CLOSE(info->nci_file->put);
        free(info->nci_file);
        info->nci_file = NULL;
      }
    }
    else
      status = TreeINVTREE;
  }
  return status;
}

int       _TreeOpenEdit(void **dbid, char *tree_in, int shot_in)
{
  PINO_DATABASE **dblist = (PINO_DATABASE **)dbid;
  TREE_INFO *info;
  char     *tree = malloc(strlen(tree_in)+1);
  int       shot;
  int       status = TreeFAILURE;

  RemoveBlanksAndUpcase(tree,tree_in);
  shot = shot_in ? shot_in : TreeGetCurrentShotId(tree);
  if (shot)
  {
    PINO_DATABASE **dblist = (PINO_DATABASE **)dbid;
    status = CreateDbSlot(dblist, tree, shot, 1);
    if (status == TreeNORMAL)
    {
      info = (TREE_INFO *)malloc(sizeof(TREE_INFO));
      if (info)
      {
        memset(info,0,sizeof(*info));
        info->flush = ((*dblist)->shotid == -1);
        info->treenam = strcpy(malloc(strlen(tree)+1),tree);
	info->shot = (*dblist)->shotid;
        status = MapTree(tree, (*dblist)->shotid, info, 1, 1);
        if (status == TreeFAILURE && treeshr_errno == TreeFILE_NOT_FOUND)
        {
          status = TreeCallHook(RetrieveTree, info,0);
          if (status == TreeNORMAL)
            status = MapTree(tree, (*dblist)->shotid, info, 1, 1);
        }
        if (status & 1)
        {
          TreeCallHook(OpenTreeEdit, info,0);
          info->edit = (TREE_EDIT *)malloc(sizeof(TREE_EDIT));
          if (info->edit)
          {
            memset(info->edit, 0, sizeof(TREE_EDIT));
            info->root = info->node;
            status = TreeOpenNciW(info, 0);
            if (status & 1)
            {
              (*dblist)->tree_info = info;
              (*dblist)->open = 1;
              (*dblist)->open_for_edit = 1;
              (*dblist)->open_readonly = 0;
              (*dblist)->default_node = info->root;
            }
            else
              free(info->edit);
          }
        }
        if (!(status & 1))
        {
          free(info->treenam);
          free(info);
        }
      }
    }
  }
  return status;
}

int       _TreeOpenNew(void **dbid, char *tree_in, int shot_in)
{
  PINO_DATABASE **dblist = (PINO_DATABASE **)dbid;
  TREE_INFO *info;
  char     *tree = malloc(strlen(tree_in)+1);
  int       shot;
  int       status = TreeFAILURE;

  RemoveBlanksAndUpcase(tree,tree_in);
  shot = shot_in ? shot_in : TreeGetCurrentShotId(tree);
  if (shot)
  {
    PINO_DATABASE **dblist = (PINO_DATABASE **)dbid;
    status = CreateDbSlot(dblist, tree, shot, 1);
    if (status == TreeNORMAL)
    {
      info = (TREE_INFO *)malloc(sizeof(TREE_INFO));
      if (info)
      {
        int fd;
        memset(info,0,sizeof(*info));
        info->flush = ((*dblist)->shotid == -1);
        info->treenam = strcpy(malloc(strlen(tree)+1),tree);
        info->shot = (*dblist)->shotid;
        fd = OpenOne(info, tree, (*dblist)->shotid, TREE_TREEFILE_TYPE, 1, &info->filespec, 0, 0);
        if (fd != -1)
        {
          char *resnam = 0;
          MDS_IO_CLOSE(fd);
          fd = OpenOne(info, tree, (*dblist)->shotid, TREE_NCIFILE_TYPE, 1, &resnam, 0, 0);
          if (resnam)
            free(resnam);
          if (fd != -1)
          {
            MDS_IO_CLOSE(fd);
            fd = OpenOne(info, tree, (*dblist)->shotid, TREE_DATAFILE_TYPE, 1, &resnam, 0, 0);
            if (resnam)
              free(resnam);
            if (fd != -1)
              MDS_IO_CLOSE(fd);
          }
        }
        if (fd != -1)
        {
          TreeCallHook(OpenTreeEdit, info,0);
          info->edit = (TREE_EDIT *)malloc(sizeof(TREE_EDIT));
          if (info->edit)
          {
            memset(info->edit, 0, sizeof(TREE_EDIT));
            info->blockid = TreeBLOCKID;
            info->header = malloc(512);
            memset(info->header, 0, sizeof(TREE_HEADER));
            info->header->free = -1;
            info->edit->header_pages = 1;
            (*dblist)->tree_info = info;
            (*dblist)->open = 1;
            (*dblist)->open_for_edit = 1;
            (*dblist)->open_readonly = 0;
            info->root = info->node;
            (*dblist)->default_node = info->root;
            TreeOpenNciW(info, 0);
            status = TreeExpandNodes(*dblist, 0, 0);
            strncpy(info->node->name,"TOP         ",sizeof(info->node->name));
            info->node->parent = 0;
            info->node->child = 0;
            info->node->member = 0;
            info->node->usage = TreeUSAGE_SUBTREE;
            (info->node + 1)->child = 0;
            bitassign(1,info->edit->nci->flags,NciM_INCLUDE_IN_PULSE);
            info->tags = malloc(512);
            info->edit->tags_pages = 1;
            info->tag_info = malloc(sizeof(TAG_INFO)*512);
            info->edit->tag_info_pages = sizeof(TAG_INFO);
            info->external = malloc(512);
            info->edit->external_pages = 1;
	    info->edit->first_in_mem = 0;
            info->header->version = 1;
            info->header->sort_children = 1;
            info->header->sort_members = 1;
            info->header->free = sizeof(NODE);
          }
        }
        else
        {
          free(info->treenam);
          free(info);
        }
      }
    }
  }
  if (status & 1)
    _TreeWriteTree(dbid, 0, 0);
  return status;
}

void *TreeSwitchDbid(void *dbid)
{
  void *old_dbid = DBID;
  DBID = dbid;
  return old_dbid;
}

struct descriptor *TreeFileName(char *tree, int shot)
{
  static struct descriptor ans_dsc={0, DTYPE_T, CLASS_D, 0};
  int fd;
  char *ans;
  TREE_INFO dummy_info;

  fd = OpenOne(&dummy_info, tree, shot, TREE_TREEFILE_TYPE, 0, &ans, 0, 0);
  if (fd != -1) {
    MDS_IO_CLOSE(fd);
    ans_dsc.pointer = ans;
    ans_dsc.length = strlen(ans);
  }
  else {
    ans_dsc.pointer = NULL;
    ans_dsc.length = 0;
  }
  return &ans_dsc;
}
