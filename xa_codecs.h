/*
 * xa_codecs.h
 *
 * Copyright (C) 1995-1998,1999 by Mark Podlipec.
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

#ifndef __XA_CODECS_H__
#define __XA_CODECS_H__

typedef struct
{
  void *anim_hdr;		/* used to add stuff to Free Chain */
  xaULONG compression;		/* input/output compression */
  xaULONG x,y;			/* input/output x,y */
  xaULONG depth;		/* input depth  and cfunc */
  void  *extra;			/* extra for delta */
  xaULONG xapi_rev;		/* XAnim API rev */
  xaULONG (*decoder)();		/* decoder routine */
  char *description;		/* text string */
  xaULONG avi_ctab_flag;	/* AVI ctable to be read */
  xaULONG (*avi_read_ext)();	/* routine to read extended data */

} XA_CODEC_HDR;

#define CODEC_SUPPORTED    1
#define CODEC_UNKNOWN      0
#define CODEC_UNSUPPORTED -1

#endif
