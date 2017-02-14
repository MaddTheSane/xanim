
/*
 * xa_codec.c
 *
 * Copyright (C) 1993-1998,1999 by Mark Podlipec. 
 * All rights reserved.
 *
 * This software may be freely used, copied and redistributed without
 * fee for non-commerical purposes provided that this copyright
 * notice is preserved intact on all copies.
 * 
 * There is no warranty or other guarantee of fitness of this software.
 * It is provided solely "as is". The author disclaims all
 * responsibility and liability with respect to this software's usage
 * or its effect upon hardware or computer systems.
 *
 */

/*******************************
 * Revision History
 *
 ********************************/


#include "xanim.h" 
#include <stdarg.h>
#include "xa_codecs.h"

#ifdef XA_DLL
#include <dirent.h>
#include <dlfcn.h>

#ifndef RTLD_NOW
#define RTLD_NOW 1
#endif
#endif

#include "xavid_mod.h"


/*-------- Local Functions ----------------------------------------*/
void Init_Video_Codecs();
xaLONG AVI_Video_Codec_Query();
xaLONG QT_Video_Codec_Query();
static void Video_Add_Query_To_List();
#ifdef XA_DLL
static void Video_Add_Mod_To_List();
static void Video_Add_Codec_To_List();
static void Check_For_Video_Modules();
static void Scan_Module_Directory();
static void Open_Video_Lib();
#endif

/*-------- External Functions ----------------------------------------*/
extern xaLONG	AVI_Codec_Query();
extern xaLONG	AVI_UNK_Codec_Query();
extern xaLONG	QT_Codec_Query();
extern xaLONG	QT_UNK_Codec_Query();
extern char	*XA_index();
extern char	*XA_white_out();
extern void	XA_Indent_Print();


/*-------- Optional External Functions -------------------------------*/
#ifdef XA_CVID
extern xaLONG   Cinepak_Codec_Query();
#endif

#ifdef XA_IV32
extern xaLONG	Indeo_Codec_Query();
#endif

#ifdef XA_CYUV
extern xaLONG	CYUV_Codec_Query();
#endif


/*-------- Structure Definitions -------------------------------------*/
typedef struct S_QUERY_LIST
{
  xaLONG		(*query_func)();
  struct S_QUERY_LIST	*next;
} QUERY_LIST;

typedef struct S_CODEC_LIST
{
  xaULONG		id;
  xaLONG		(*init_func)();
  xaULONG		(*dec_func)();
  struct S_CODEC_LIST	*next;
} CODEC_LIST;

typedef struct S_MOD_LIST
{
  XAVID_MOD_HDR	*mod_hdr;
  void		*handle;
  struct S_MOD_LIST	*next;
} MOD_LIST;


/*-------- Local Variables -------------------------------------------*/
static QUERY_LIST *avi_query_list = 0;
static QUERY_LIST *qt_query_list = 0;
static CODEC_LIST *misc_codec_list = 0;
static MOD_LIST *mod_list = 0;


/* SunOS doesn't seem to want to link to any symbol not already used
 * inside of xanim. So we use some math functions here just for that
 * purpose.
 */
#include <math.h>
static double what_fun = 0.0;

/****--------------------------------------------------------------------****
 *
 ****--------------------------------------------------------------------****/
void Init_Video_Codecs()
{ double sqrt();

  what_fun = (double)sqrt(6.0);

	/* UNK Must be first for AVI - And the first shall be last */
  Video_Add_Query_To_List( &avi_query_list, AVI_UNK_Codec_Query );
  Video_Add_Query_To_List( &avi_query_list, AVI_Codec_Query );

	/* UNK Must be first for QT - And the first shall be last */
  Video_Add_Query_To_List( &qt_query_list, QT_UNK_Codec_Query );
  Video_Add_Query_To_List( &qt_query_list, QT_Codec_Query );


#ifdef XA_CVID
  Video_Add_Query_To_List( &avi_query_list, Cinepak_Codec_Query );
  Video_Add_Query_To_List( &qt_query_list, Cinepak_Codec_Query );
#endif
#ifdef XA_IV32
  Video_Add_Query_To_List( &avi_query_list, Indeo_Codec_Query );
  Video_Add_Query_To_List( &qt_query_list, Indeo_Codec_Query );
#endif
#ifdef XA_CYUV
  Video_Add_Query_To_List( &avi_query_list, CYUV_Codec_Query );
  Video_Add_Query_To_List( &qt_query_list, CYUV_Codec_Query );
#endif

#ifdef XA_DLL
   Check_For_Video_Modules();
#endif
}

#ifdef XA_DLL

/****--------------------------------------------------------------------****
 *
 ****--------------------------------------------------------------------****/
static void Check_For_Video_Modules()
{ int len;
  char *dp, *dir_buf, *path_buf;
  char *mod_path = getenv("XANIM_MOD_DIR");

#ifdef XA_DLL_PATH
  if (mod_path == 0) mod_path = XA_DLL_PATH;
#else
  if (mod_path == 0)	mod_path = "/usr/local/xanim/mods";
#endif

  len = strlen( mod_path );
  what_fun = (double)cos(45.0);

	/* probably not a good idea to modify the ENV data base */
  path_buf = (char *)malloc( len + 1 );
  if (path_buf==0) return;
  strcpy(path_buf, mod_path);

	/* Also alloc a buffer so we can construct a full path later */
	/* name of module better not be more than 1024. we'll check later. */

  dir_buf = (char *)malloc( len + 1024 + 1);
  if (dir_buf==0) { free(path_buf); return; }

  dp = path_buf;
  while( dp && *dp )
  { char *t1p, *t2p;
	/* find directory separation if any */
    t1p = XA_index(dp,':');
    if (t1p) { *t1p = 0; t1p++; }  /* note: t1p may now points to next dir */
	/* strip any white space on either end of dir name */
    t2p = XA_white_out( dp );
    strcpy(dir_buf, t2p);
    Scan_Module_Directory( dir_buf );
    dp = t1p;
  }
}

/****--------------------------------------------------------------------****
 * dir_name contains a directory name plus 1024 extra bytes at end.
 ****--------------------------------------------------------------------****/
static void Scan_Module_Directory(dir_name)
char	*dir_name;
{ DIR *dir;
  struct dirent *dent;
  char *end_of_dir;
  int len;

  if (dir_name == 0) return;

  DEBUG_LEVEL1 fprintf(stderr,"dirname: %s\n", dir_name );

	/* need to add trailing backslash if one isn't there already */
  len = strlen( dir_name );
  end_of_dir = &dir_name[ len - 1 ];

  if ( *end_of_dir != '/' )
  { end_of_dir++;
    *end_of_dir = '/';
  }
  end_of_dir++;

  *end_of_dir = 0;

  dir = opendir( dir_name );
  if (dir == 0) return;
  while( (dent = readdir( dir )) )
  {
    /* suppose I could do a stat to verify it's a file, oh screw it :) */
    if ( strncmp( dent->d_name, "vid_", 4 ) == 0 )
    { char *ep = &dent->d_name[ strlen(dent->d_name) - 3];
      if ( strcmp(ep,".xa") == 0)  /* check for .so ending */
      {		/* terminate directory path */     
	*end_of_dir = 0;
		/* construct full name */
	strcat( dir_name, dent->d_name );
	Open_Video_Lib( dir_name );
      }
    }
  }
}

/****--------------------------------------------------------------------****
 * Attempt to open a Dynamically Loadable Video Library
 ****--------------------------------------------------------------------****/
static void Open_Video_Lib(fname)
char *fname;
{ void *handle;
  XAVID_MOD_HDR *(*what_the)();
  char *error;

  handle = dlopen(fname,RTLD_NOW);
  if (handle)
  { what_the = dlsym(handle, "What_The");
    if ((error = dlerror()) != NULL)
    { fprintf(stderr,"dlsym failed on %s with %s\n",fname,error);
      dlclose(handle);
    }
    else
    { XAVID_MOD_HDR *mod_hdr;
      XAVID_FUNC_HDR *func;
      int i;

      mod_hdr = what_the();
      Video_Add_Mod_To_List( &mod_list, mod_hdr, handle);

      func = mod_hdr->funcs;
      for(i=0; i < mod_hdr->num_funcs; i++)
      {
        if ( func[i].what & XAVID_AVI_QUERY )
		Video_Add_Query_To_List( &avi_query_list, func[i].iq_func );
        if ( func[i].what & XAVID_QT_QUERY )
		Video_Add_Query_To_List( &qt_query_list, func[i].iq_func );
        if ( func[i].what & XAVID_DEC_FUNC )
		Video_Add_Codec_To_List( &misc_codec_list, func[i].id,
					func[i].iq_func, func[i].dec_func);
      }
    }
  }
  else
  { error = dlerror();
    if (error) fprintf(stderr,"dlopen of %s failed: %s.\n",fname,error);
    else	fprintf(stderr,"dlopen of %s failed.\n",fname);
  }
}
#endif


/****--------------------------------------------------------------------****
 *
 ****--------------------------------------------------------------------****/
void Free_Video_Codecs()
{ 
  { QUERY_LIST *p = qt_query_list;
    while(p) { QUERY_LIST *q = p; p = p->next; q->next = 0; free(q); }
  }
  { QUERY_LIST *p = avi_query_list;
    while(p) { QUERY_LIST *q = p; p = p->next; q->next = 0; free(q); }
  }
  { CODEC_LIST *p = misc_codec_list;
    while(p) { CODEC_LIST *q = p; p = p->next; q->next = 0; free(q); }
  }
  { MOD_LIST *p = mod_list;
    while(p)
    { MOD_LIST *q = p;
      p = p->next;
      q->next = 0;
#ifdef XA_DLL
      if (q->handle) dlclose(q->handle);
#endif
      free(q);
    }
  }
}

/****--------------------------------------------------------------------****
 *
 ****--------------------------------------------------------------------****/
#ifdef XA_DLL
static void Video_Add_Mod_To_List( list, mod_hdr, handle )
MOD_LIST	**list;
XAVID_MOD_HDR	*mod_hdr;
void		*handle;
{
  MOD_LIST *new = (MOD_LIST *)malloc(sizeof(MOD_LIST));
  if (new == 0) return;
  new->mod_hdr	= mod_hdr;
  new->handle	= handle;
  new->next = *list;
  *list = new;

  if (xa_verbose)
  { 
    fprintf(stderr, "   VidModule: %s. Rev: %s\n",mod_hdr->desc,mod_hdr->rev);
    XA_Indent_Print(stderr, "              ",mod_hdr->copyright,xaTRUE);
    DEBUG_LEVEL1
    { fprintf(stderr,"   ModuleAuthor(s): %s\n",mod_hdr->mod_author);
      fprintf(stderr,"   CodecAuthor(s): %s\n",mod_hdr->authors);
    }
  }
}
#endif

/****--------------------------------------------------------------------****
 *
 ****--------------------------------------------------------------------****/
#ifdef XA_DLL
static void Video_Add_Codec_To_List( list, id, init_func, dec_func)
CODEC_LIST	**list;
xaLONG		(*init_func)();
xaULONG		(*dec_func)();
{
  CODEC_LIST *new = (CODEC_LIST *)malloc(sizeof(CODEC_LIST));
  if (new == 0) return;

  new->id		= id;
  new->init_func	= init_func;
  new->dec_func		= dec_func;
  new->next		= *list;
  *list = new;
}
#endif


/****--------------------------------------------------------------------****
 *
 ****--------------------------------------------------------------------****/
static void Video_Add_Query_To_List( list, query_func )
QUERY_LIST	**list;
xaLONG (*query_func)();
{
  QUERY_LIST *new = (QUERY_LIST *)malloc(sizeof(QUERY_LIST));
  if (new == 0) return;
  new->query_func = query_func;
  new->next	= *list;
  *list = new;
}

/****--------------------------------------------------------------------****
 *
 ****--------------------------------------------------------------------****/
xaLONG AVI_Video_Codec_Query(codec_hdr)
XA_CODEC_HDR	*codec_hdr;
{ QUERY_LIST *query_lst = avi_query_list;
  xaLONG codec_ret = CODEC_UNKNOWN;
  while(query_lst)
  { if (query_lst->query_func) codec_ret = query_lst->query_func( codec_hdr );
    if (codec_ret == CODEC_SUPPORTED) break;
    if (codec_ret == CODEC_UNSUPPORTED) break;
    query_lst = query_lst->next;
  }
  return( codec_ret );
}

/****--------------------------------------------------------------------****
 *
 ****--------------------------------------------------------------------****/
xaLONG QT_Video_Codec_Query(codec_hdr)
XA_CODEC_HDR	*codec_hdr;
{ QUERY_LIST *query_lst = qt_query_list;
  xaLONG codec_ret = CODEC_UNKNOWN;
  while(query_lst)
  { 
    if (query_lst->query_func) codec_ret = query_lst->query_func( codec_hdr );
    if (codec_ret == CODEC_SUPPORTED) break;
    if (codec_ret == CODEC_UNSUPPORTED) break;
    query_lst = query_lst->next;
  }
  return( codec_ret );
}

/****--------------------------------------------------------------------****
 * This is so the video dll modules don't need to use stdio. This may
 * allow a dll module for one platform to work on another.
 ****--------------------------------------------------------------------****/
#ifdef XA_PRINT
void XA_Print(char *fmt, ...)
{ va_list vallist;
  va_start(vallist, fmt);
  vfprintf(stderr,fmt,vallist);
  va_end(vallist);
}
#else
void XA_Print(fmt)
char *fmt;
{ 
  fprintf(stderr,"%s",fmt);
}
#endif


