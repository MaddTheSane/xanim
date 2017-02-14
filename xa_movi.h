
/*
 * xa_movi.h
 *
 * Copyright (C) 1996-1998,1999 by Mark Podlipec. 
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

#include "xanim.h"

typedef struct MOVI_FRAME_STRUCT
{
  xaULONG time;
  xaULONG timelo;
  XA_ACTION *act;
  struct MOVI_FRAME_STRUCT *next;
} MOVI_FRAME;

typedef struct
{
  xaULONG frames;
  xaULONG width;
  xaULONG chans;
  xaULONG format;
  float rate;
  xaULONG compression;
  xaULONG *off;
  xaULONG *size;
} MOVI_A_HDR;

typedef struct
{
  xaULONG frames;
  xaULONG width;
  xaULONG height;
  xaULONG compression;
  xaULONG interlacing;
  xaULONG packing;
  xaULONG orientation;
  float pixel_aspect;
  float fps;
  float q_spatial;
  float q_temporal;
  xaULONG *off;
  xaULONG *size;
} MOVI_I_HDR;

/* NOTES:
 *   from makemovie manpage
 *     interlace  none, even lines 1st, odd lines 1st
 *     loopmode   once, loop, swing(pingpong)
 *     compressions   mvc1   (most likely my comp1)
 *                    mvc2
 *                    jpeg
 *                    rle   8 bit
 *                    rle24 24 bit
 *    orientation  top_to_bottom  bottom_to_top?
 *    packing      32-bit RGBX ???
 */
 
typedef struct
{
 xaULONG version;
 xaULONG i_tracks;
 xaULONG a_tracks;
 xaULONG loop_mode;
 xaULONG num_loops;
 xaULONG optimized;
 MOVI_A_HDR *a_hdr;
 MOVI_I_HDR *i_hdr;
} MOVI_HDR;
 
