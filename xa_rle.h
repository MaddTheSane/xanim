
/*
 * xa_rle.h
 *
 * Copyright (C) 1993-1998,1999 by Mark Podlipec. 
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

#define RLE_MAGIC 0xcc52

typedef struct RLE_FRAME_STRUCT
{
 xaULONG time;
 XA_ACTION *act;
 struct RLE_FRAME_STRUCT *next;
} RLE_FRAME;

typedef struct RLE_HDR_STRUCT
{
 xaULONG magic;
 xaULONG xpos;
 xaULONG ypos;
 xaULONG xsize;
 xaULONG ysize;      
 xaULONG flags;      /* misc flags */
 xaULONG chan_num;   /* number of channels   */
 xaULONG pbits;      /* pixel bits */
 xaULONG cmap_num;   /* number of channels with cmaps */
 xaULONG cbits;      /* Log2 of cmap length  */
 xaULONG csize;      /* size of cmap  */
} RLE_HDR;

/* RLE flags definitions */
/* TBD */
#define     RLEH_CLEARFIRST        0x1   /* clear framebuffer flag */
#define     RLEH_NO_BACKGROUND     0x2   /* if set, no bg color supplied */
#define     RLEH_ALPHA             0x4   /* if set, alpha channel (-1) present */
#define     RLEH_COMMENT           0x8   /* if set, comments present */


#define RLE_RED         0       /* Red channel traditionally here. */
#define RLE_GREEN       1       /* Green channel traditionally here. */
#define RLE_BLUE        2       /* Blue channel traditionally here. */
#define RLE_ALPHA      -1       /* Alpha channel here. */

#define RLE_OPCODE(x) ((x) & 0x3f)
#define RLE_LONGP(x)  ((x) & 0x40)
#define RLE_SkipLinesOp        0x01
#define RLE_SetColorOp         0x02
#define RLE_SkipPixelsOp       0x03
#define RLE_ByteDataOp         0x05
#define RLE_RunDataOp          0x06
#define RLE_EOFOp              0x07


