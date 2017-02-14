
/*
 * xanim_iff.h
 *
 * Copyright (C) 1990,1991,1992,1993,1994,1995 by Mark Podlipec. 
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
 LONG id;
 LONG size;
} Chunk_Header;

/* Graphic Stuff */
#define ANHD 0x414e4844
#define ANIM 0x414e494d
#define ANSQ 0x414e5351
#define BMHD 0x424d4844
#define BODY 0x424f4459
#define TINY 0x54494E59
#define CAMG 0x43414d47
#define CMAP 0x434d4150
#define CRNG 0x43524e47
#define DLTA 0x444c5441
#define DPAN 0x4450414e
#define DPPS 0x44505053
#define DPPV 0x44505056
#define DRNG 0x44524e47
#define FORM 0x464f524d
#define GRAB 0x47524142
#define ILBM 0x494c424d
#define IMRT 0x494d5254
#define DPI  0x44504920
#define ANFI 0x414e4649

/* Grouping Stuff */
#define LIST 0x4c495354
#define PROP 0x50524f50
#define FACE 0x46414345

/* Sound stuff */
#define VHDR 0x56484452
#define ANNO 0x414e4e4f
#define CHAN 0x4348414e


typedef struct
{
 UWORD width, height;
 WORD x, y;
 UBYTE depth;
 UBYTE masking;
 UBYTE compression;
 UBYTE pad1;
 UWORD transparentColor;
 UBYTE xAspect, yAspect;
 WORD pageWidth, pageHeight;
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
 UWORD width,height;
 WORD x, y;
 WORD xoff, yoff;
} Face_Header;

typedef struct 
{
 LONG id;
 LONG size;
 LONG subid;
} Group_Header;

typedef struct 
{
 LONG id;
 LONG size;
 LONG subid;
 UBYTE grpData[ 1 ];
} Group_Chunk;

typedef struct
{
 UBYTE op;
 UBYTE mask;
 UWORD w,h;
 UWORD x,y;
 ULONG abstime;
 ULONG reltime;
 UBYTE interleave;
 UBYTE pad0;
 ULONG bits;
 UBYTE pad[16];
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
 LONG minx;
 LONG miny;
 LONG maxx;
 LONG maxy;
} IFF_DLTA_HDR;

typedef struct
{
  ULONG dnum;
  ULONG time;
  ULONG frame;
} IFF_ANSQ;

typedef struct IFF_ACT_LST_STRUCT
{
  ULONG type;
  XA_ACTION *act;
  struct IFF_ACT_LST_STRUCT *next;
} IFF_ACT_LST;

typedef struct
{
  ULONG cnt;
  ULONG frame;
  IFF_ACT_LST *start;
  IFF_ACT_LST *end;
} IFF_DLTA_TABLE;

extern ULONG IFF_Read_File();
extern ULONG IFF_Delta3();
extern ULONG IFF_Delta5();
extern ULONG IFF_Delta7();
extern ULONG IFF_Delta8();
extern ULONG IFF_Deltal();
extern ULONG IFF_DeltaJ();
extern LONG Is_IFF_File();
extern LONG UnPackRow();

/* POD NOTE: further optimization would be to have
 * IFF_Byte_Mod and IFF_Byte_Mod_with_XOR_flag
 */
#define IFF_Byte_Mod(ptr,data,dmask,xorflag) { register UBYTE *_iptr = ptr; \
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
  register UBYTE dmaskoff = ~dmask; \
  if (0x80 & data) *_iptr++ |= dmask; else *_iptr++  &= dmaskoff; \
  if (0x40 & data) *_iptr++ |= dmask; else *_iptr++  &= dmaskoff; \
  if (0x20 & data) *_iptr++ |= dmask; else *_iptr++  &= dmaskoff; \
  if (0x10 & data) *_iptr++ |= dmask; else *_iptr++  &= dmaskoff; \
  if (0x08 & data) *_iptr++ |= dmask; else *_iptr++  &= dmaskoff; \
  if (0x04 & data) *_iptr++ |= dmask; else *_iptr++  &= dmaskoff; \
  if (0x02 & data) *_iptr++ |= dmask; else *_iptr++  &= dmaskoff; \
  if (0x01 & data) *_iptr   |= dmask; else *_iptr    &= dmaskoff; \
} }


#define IFF_Short_Mod(ptr,data,dmask,xorflag) { register UBYTE *_iptr = ptr; \
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
  register UBYTE dmaskoff = ~dmask; \
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
