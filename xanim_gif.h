
/*
 * xanim_gif.h
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

#include "xanim.h"

typedef struct
{
  LONG width;
  LONG height;
  UBYTE m;
  UBYTE cres;
  UBYTE pixbits;
  UBYTE bc;
} GIF_Screen_Hdr; 

typedef struct
{
  LONG left;
  LONG top;
  LONG width;
  LONG height;
  UBYTE m;
  UBYTE i;
  UBYTE pixbits;
  UBYTE reserved;
} GIF_Image_Hdr;

typedef struct 
{
  UBYTE valid;
  UBYTE data;
  UBYTE first;
  UBYTE res;
  LONG last;
} GIF_Table;

typedef struct GIF_FRAME_STRUCT
{
  ULONG time;
  XA_ACTION *act;
  struct GIF_FRAME_STRUCT *next;
} GIF_FRAME;

