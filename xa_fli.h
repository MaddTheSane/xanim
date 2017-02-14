
/*
 * xa_fli.h
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

#include "xanim.h"

typedef struct
{
  xaLONG size;      /*  0 size 4  */
  xaLONG magic;     /*  4 size 2  */
  xaLONG frames;    /*  6 size 2  */
  xaLONG width;     /*  8 size 2  */
  xaLONG height;    /* 10 size 2  */
  xaLONG flags;     /* 12 size 2  */
  xaLONG res1;      /* 14 size 2  */
  xaLONG speed;     /* 16 size 2  */
  xaLONG next;      /* 18 size 4  */
  xaLONG frit;      /* 22 size 4  */
                 /* 26 size 102 future enhancement */
} Fli_Header;

typedef struct
{
  xaLONG size;      /*  0 size 4 size of chunk */
  xaLONG magic;     /*  4 size 2 */
  xaLONG chunks;    /*  4 size 2 number of chunks in frame */
                  /*  4 size 8 future*/
} Fli_Frame_Header;

typedef struct FLI_FRAME_STRUCT
{
  xaULONG time;
  xaULONG timelo;
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
extern xaULONG Fli_Read_File();
extern xaLONG Is_FLI_File();


