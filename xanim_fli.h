
/*
 * xanim_fli.h
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
  LONG size;      /*  0 size 4  */
  LONG magic;     /*  4 size 2  */
  LONG frames;    /*  6 size 2  */
  LONG width;     /*  8 size 2  */
  LONG height;    /* 10 size 2  */
  LONG flags;     /* 12 size 2  */
  LONG res1;      /* 14 size 2  */
  LONG speed;     /* 16 size 2  */
  LONG next;      /* 18 size 4  */
  LONG frit;      /* 22 size 4  */
                 /* 26 size 102 future enhancement */
} Fli_Header;

typedef struct
{
  LONG size;      /*  0 size 4 size of chunk */
  LONG magic;     /*  4 size 2 */
  LONG chunks;    /*  4 size 2 number of chunks in frame */
                  /*  4 size 8 future*/
} Fli_Frame_Header;

typedef struct FLI_FRAME_STRUCT
{
  ULONG time;
  ULONG timelo;
  XA_ACTION *act;
  struct FLI_FRAME_STRUCT *next;
} FLI_FRAME;

#define CHUNK_4          4
#define FLI_LC7          7
#define FLI_COLOR       11
#define FLI_LC          12
#define FLI_BLACK       13
#define FLI_BRUN        15
#define FLI_COPY        16
#define FLI_MINI	18

#define FLI_MAX_COLORS  256

extern void Decode_Fli_BRUN();
extern void Decode_Fli_LC();
extern void Fli_Buffer_Action();
extern ULONG Fli_Read_File();
extern LONG Is_FLI_File();


