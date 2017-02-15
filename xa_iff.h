/*
 * xa_iff.h
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

#ifndef __XA_IFF_H__
#define __XA_IFF_H__

#include "xanim.h"

typedef struct 
{
 xaLONG id;
 xaLONG size;
} Chunk_Header;

/* Graphic Stuff */
#define IFF_ANHD 0x414e4844
#define IFF_ANIM 0x414e494d
#define IFF_ANSQ 0x414e5351
#define IFF_BMHD 0x424d4844
#define IFF_BODY 0x424f4459
#define IFF_TINY 0x54494E59
#define IFF_CAMG 0x43414d47
#define IFF_CMAP 0x434d4150
#define IFF_CRNG 0x43524e47
#define IFF_DLTA 0x444c5441
#define IFF_DPAN 0x4450414e
#define IFF_DPPS 0x44505053
#define IFF_DPPV 0x44505056
#define IFF_DRNG 0x44524e47
#define IFF_FORM 0x464f524d
#define IFF_GRAB 0x47524142
#define IFF_ILBM 0x494c424d
#define IFF_IMRT 0x494d5254
#define IFF_DPI  0x44504920
#define IFF_ANFI 0x414e4649
#define IFF_NAME 0x4e414d45

/* Grouping Stuff */
#define IFF_LIST 0x4c495354
#define IFF_PROP 0x50524f50
#define IFF_FACE 0x46414345

/* Sound stuff */
#define IFF_VHDR 0x56484452
#define IFF_ANNO 0x414e4e4f
#define IFF_CHAN 0x4348414e
#define IFF_8SVX 0x38535658


typedef struct
{
 xaUSHORT width, height;
 xaSHORT x, y;
 xaUBYTE depth;
 xaUBYTE masking;
 xaUBYTE compression;
 xaUBYTE pad1;
 xaUSHORT transparentColor;
 xaUBYTE xAspect, yAspect;
 xaSHORT pageWidth, pageHeight;
} Bit_Map_Header;

#define BMHD_COMP_NONE 0L
#define BMHD_COMP_BYTERUN 1L

#define BMHD_MSK_NONE 0L
#define BMHD_MSK_HAS 1L
#define BMHD_MSK_TRANS 2L
#define BMHD_MSK_LASSO 3L

#define mskNone                 0
#define mskHasMask              1
#define mskHasTransparentColor  2
#define mskLasso                3

#define cmpNone      0
#define cmpByteRun1  1

/* Aspect ratios: The proper fraction xAspect/yAspect represents the pixel
 * aspect ratio pixel_width/pixel_height.
 *
 * For the 4 Amiga display modes:
 *   320 x 200: 10/11  (these pixels are taller than they are wide)
 *   320 x 400: 20/11
 *   640 x 200:  5/11
 *   640 x 400: 10/11      */
#define x320x200Aspect 10
#define y320x200Aspect 11
#define x320x400Aspect 20
#define y320x400Aspect 11
#define x640x200Aspect  5
#define y640x200Aspect 11
#define x640x400Aspect 10
#define y640x400Aspect 11

/* CRNG Stuff */
#define IFF_CRNG_ACTIVE   1
#define IFF_CRNG_REVERSE  2
/* 16384 * 16.6667 ms */
#define IFF_CRNG_INTERVAL (273065)
#define IFF_CRNG_HDR_SIZE 8
#define IFF_CRNG_DPII_KLUDGE 36

/* CAMG Stuff */
#define IFF_CAMG_NOP   0x00000001
#define IFF_CAMG_EHB   0x00000080
#define IFF_CAMG_HAM   0x00000800
#define IFF_CAMG_LACE  0x00004000

typedef struct
{
 xaUSHORT width,height;
 xaSHORT x, y;
 xaSHORT xoff, yoff;
} Face_Header;

typedef struct 
{
 xaLONG id;
 xaLONG size;
 xaLONG subid;
} Group_Header;

typedef struct 
{
 xaLONG id;
 xaLONG size;
 xaLONG subid;
 xaUBYTE grpData[ 1 ];
} Group_Chunk;

typedef struct
{
 xaUBYTE op;
 xaUBYTE mask;
 xaUSHORT w,h;
 xaUSHORT x,y;
 xaULONG abstime;
 xaULONG reltime;
 xaUBYTE interleave;
 xaUBYTE pad0;
 xaULONG bits;
 xaUBYTE pad[16];
} Anim_Header;
#define Anim_Header_SIZE 40
#define IFF_ANHD_LDATA  0x0001
#define IFF_ANHD_XOR    0x0002
#define IFF_ANHD_1LIST  0x0002
#define IFF_ANHD_RLC    0x0008
#define IFF_ANHD_VERT   0x0010
#define IFF_ANHD_LIOFF  0x0020

typedef struct
{
 xaLONG minx;
 xaLONG miny;
 xaLONG maxx;
 xaLONG maxy;
} IFF_DLTA_HDR;

typedef struct
{
  xaULONG dnum;
  xaULONG time;
  xaULONG frame;
} IFF_ANSQ_HDR;

typedef struct IFF_ACT_LST_STRUCT
{
  xaULONG type;
  XA_ACTION *act;
  struct IFF_ACT_LST_STRUCT *next;
} IFF_ACT_LST;

typedef struct
{
  xaULONG cnt;
  xaULONG frame;
  IFF_ACT_LST *start;
  IFF_ACT_LST *end;
} IFF_DLTA_TABLE;

extern xaULONG IFF_Delta3();
extern xaULONG IFF_Delta5();
extern xaULONG IFF_Delta7();
extern xaULONG IFF_Delta8();
extern xaULONG IFF_Deltal();
extern xaULONG IFF_DeltaJ();
extern xaLONG Is_IFF_File();
extern xaLONG UnPackRow();


extern void IFF_Print_ID(FILE *fout,xaLONG id);
extern void IFF_Read_BODY(XA_INPUT *xin,xaUBYTE *image_out,xaLONG bodysize,xaULONG xsize,xaULONG ysize,xaULONG depth,
			xaLONG compression,xaLONG masking,xaULONG or_mask);
extern void IFF_Buffer_HAM6(xaUBYTE *out, xaUBYTE *in, XA_CHDR *chdr, ColorReg *h_cmap, xaULONG xosize, xaULONG yosize, xaULONG xip,xaULONG yip, xaULONG xisize, xaULONG d_flag);
extern void IFF_Buffer_HAM8(xaUBYTE *out, xaUBYTE *in, XA_CHDR *chdr, ColorReg *h_cmap, xaULONG xosize, xaULONG yosize, xaULONG xip,xaULONG yip, xaULONG xisize, xaULONG d_flag);
extern void IFF_Init_DLTA_HDR(xaULONG max_x,xaULONG max_y);
extern void IFF_Update_DLTA_HDR(xaLONG *min_x,xaLONG *min_y,xaLONG *max_x,xaLONG *max_y);
extern void IFF_Read_BMHD(XA_INPUT *xin,Bit_Map_Header *bmhd);
extern void IFF_Read_CMAP_0(ColorReg *cmap,xaULONG size,XA_INPUT *xin);
extern void IFF_Read_CMAP_1(ColorReg *cmap,xaULONG size,XA_INPUT *xin);
extern void IFF_Shift_CMAP(ColorReg *cmap,xaULONG csize);

/* POD NOTE: further optimization would be to have
 * IFF_Byte_Mod and IFF_Byte_Mod_with_XOR_flag
 */
#define IFF_Byte_Mod(ptr,data,dmask,xorflag) { register xaUBYTE *_iptr = ptr; \
if (xorflag) { \
  if (0x80 & data) *_iptr++ ^= dmask; else _iptr++; \
  if (0x40 & data) *_iptr++ ^= dmask; else _iptr++; \
  if (0x20 & data) *_iptr++ ^= dmask; else _iptr++; \
  if (0x10 & data) *_iptr++ ^= dmask; else _iptr++; \
  if (0x08 & data) *_iptr++ ^= dmask; else _iptr++; \
  if (0x04 & data) *_iptr++ ^= dmask; else _iptr++; \
  if (0x02 & data) *_iptr++ ^= dmask; else _iptr++; \
  if (0x01 & data) *_iptr   ^= dmask; \
} else { \
  register xaUBYTE dmaskoff = ~dmask; \
  if (0x80 & data) *_iptr++ |= dmask; else *_iptr++  &= dmaskoff; \
  if (0x40 & data) *_iptr++ |= dmask; else *_iptr++  &= dmaskoff; \
  if (0x20 & data) *_iptr++ |= dmask; else *_iptr++  &= dmaskoff; \
  if (0x10 & data) *_iptr++ |= dmask; else *_iptr++  &= dmaskoff; \
  if (0x08 & data) *_iptr++ |= dmask; else *_iptr++  &= dmaskoff; \
  if (0x04 & data) *_iptr++ |= dmask; else *_iptr++  &= dmaskoff; \
  if (0x02 & data) *_iptr++ |= dmask; else *_iptr++  &= dmaskoff; \
  if (0x01 & data) *_iptr   |= dmask; else *_iptr    &= dmaskoff; \
} }


#define IFF_Short_Mod(ptr,data,dmask,xorflag) { register xaUBYTE *_iptr = ptr; \
if (xorflag) { \
  if (0x8000 & data) *_iptr++  ^= dmask; else _iptr++; \
  if (0x4000 & data) *_iptr++  ^= dmask; else _iptr++; \
  if (0x2000 & data) *_iptr++  ^= dmask; else _iptr++; \
  if (0x1000 & data) *_iptr++  ^= dmask; else _iptr++; \
  if (0x0800 & data) *_iptr++  ^= dmask; else _iptr++; \
  if (0x0400 & data) *_iptr++  ^= dmask; else _iptr++; \
  if (0x0200 & data) *_iptr++  ^= dmask; else _iptr++; \
  if (0x0100 & data) *_iptr++  ^= dmask; else _iptr++; \
  if (0x0080 & data) *_iptr++  ^= dmask; else _iptr++; \
  if (0x0040 & data) *_iptr++  ^= dmask; else _iptr++; \
  if (0x0020 & data) *_iptr++   ^= dmask; else _iptr++; \
  if (0x0010 & data) *_iptr++   ^= dmask; else _iptr++; \
  if (0x0008 & data) *_iptr++   ^= dmask; else _iptr++; \
  if (0x0004 & data) *_iptr++   ^= dmask; else _iptr++; \
  if (0x0002 & data) *_iptr++   ^= dmask; else _iptr++; \
  if (0x0001 & data) *_iptr     ^= dmask; \
} else { \
  register xaUBYTE dmaskoff = ~dmask; \
  if (0x8000 & data) *_iptr++  |= dmask; else *_iptr++  &= dmaskoff; \
  if (0x4000 & data) *_iptr++  |= dmask; else *_iptr++  &= dmaskoff; \
  if (0x2000 & data) *_iptr++  |= dmask; else *_iptr++  &= dmaskoff; \
  if (0x1000 & data) *_iptr++  |= dmask; else *_iptr++  &= dmaskoff; \
  if (0x0800 & data) *_iptr++  |= dmask; else *_iptr++  &= dmaskoff; \
  if (0x0400 & data) *_iptr++  |= dmask; else *_iptr++  &= dmaskoff; \
  if (0x0200 & data) *_iptr++  |= dmask; else *_iptr++  &= dmaskoff; \
  if (0x0100 & data) *_iptr++  |= dmask; else *_iptr++  &= dmaskoff; \
  if (0x0080 & data) *_iptr++  |= dmask; else *_iptr++  &= dmaskoff; \
  if (0x0040 & data) *_iptr++  |= dmask; else *_iptr++  &= dmaskoff; \
  if (0x0020 & data) *_iptr++   |= dmask; else *_iptr++   &= dmaskoff; \
  if (0x0010 & data) *_iptr++   |= dmask; else *_iptr++   &= dmaskoff; \
  if (0x0008 & data) *_iptr++   |= dmask; else *_iptr++   &= dmaskoff; \
  if (0x0004 & data) *_iptr++   |= dmask; else *_iptr++   &= dmaskoff; \
  if (0x0002 & data) *_iptr++   |= dmask; else *_iptr++   &= dmaskoff; \
  if (0x0001 & data) *_iptr     |= dmask; else *_iptr     &= dmaskoff; \
} }

#endif
