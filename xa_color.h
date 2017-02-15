/*
 * xa_color.h
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

#ifndef __XA_COLOR_H__
#define __XA_COLOR_H__

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

typedef struct
{
  xaUBYTE r0,g0,b0;
  xaUBYTE r1,g1,b1;
  xaUBYTE r2,g2,b2;
  xaUBYTE r3,g3,b3;
  xaULONG clr0_0,clr0_1,clr0_2,clr0_3;
  xaULONG clr1_0,clr1_1,clr1_2,clr1_3;
  xaULONG clr2_0,clr2_1,clr2_2,clr2_3;
  xaULONG clr3_0,clr3_1,clr3_2,clr3_3;
} XA_2x2_Color;


#endif
