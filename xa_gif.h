/*
 * xa_gif.h
 *
 * Copyright (C) 1990-1998,1999 by Mark Podlipec. 
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

#ifndef __XA_GIF_H__
#define __XA_GIF_H__

#include "xanim.h"

typedef struct
{
  xaLONG width;
  xaLONG height;
  xaUBYTE m;
  xaUBYTE cres;
  xaUBYTE pixbits;
  xaUBYTE bc;
} GIF_Screen_Hdr; 

typedef struct
{
  xaLONG left;
  xaLONG top;
  xaLONG width;
  xaLONG height;
  xaUBYTE m;
  xaUBYTE i;
  xaUBYTE pixbits;
  xaUBYTE reserved;
} GIF_Image_Hdr;

typedef struct 
{
  xaUBYTE valid;
  xaUBYTE data;
  xaUBYTE first;
  xaUBYTE res;
  xaLONG last;
} GIF_Table;

typedef struct GIF_FRAME_STRUCT
{
  xaULONG time;
  XA_ACTION *act;
  struct GIF_FRAME_STRUCT *next;
} GIF_FRAME;

extern GIF_FRAME *GIF_Read_File(const char *fname,XA_ANIM_HDR *anim_hdr,xaULONG *frame_cnt);
extern void GIF_Free_Frame_List(GIF_FRAME *gframes);

#endif
