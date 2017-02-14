
/*
 * xa_color.h
 *
 * Copyright (C) 1995,1995 by Mark Podlipec.
 * All rights reserved.
 *
 * This software may be freely copied, modified and redistributed without
 * fee for non-commerical purposes provided that this copyright notice is
 * preserved intact on all copies and modified copies.
 *
 * There is no warranty or other guarantee of fitness of this software.
 * It is provided solely "as is". The author(s) disclaim(s) all
 * responsibility and liability with respect to this software's usage
 * or its effect upon hardware or computer systems.
 *
 */

typedef struct
{
  xaUBYTE *Ybuf;
  xaUBYTE *Ubuf;
  xaUBYTE *Vbuf;
  xaUBYTE *the_buf;
  xaULONG  the_buf_size;
  xaUSHORT y_w,y_h;
  xaUSHORT uv_w,uv_h;
} YUVBufs;

typedef struct
{
  xaULONG Uskip_mask;
  xaLONG *YUV_Y_tab;
  xaLONG *YUV_UB_tab;
  xaLONG *YUV_VR_tab;
  xaLONG *YUV_UG_tab;
  xaLONG *YUV_VG_tab;
} YUVTabs;


