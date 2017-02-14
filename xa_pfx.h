
/*
 * xa_pfx.h
 *
 * Copyright (C) 1992,1993,1994,1995 by Mark Podlipec. 
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

#define pfx_INIT 0x494e4954
#define pfx_GLOC 0x474c4f43
#define pfx_BITP 0x42495450
#define pfx_XDIM 0x5844494d
#define pfx_YDIM 0x5944494d
#define pfx_GLOV 0x474c4f56
#define pfx_DURA 0x44555241
#define pfx_HIGH 0x48494748
#define pfx_FLAC 0x464c4143
#define pfx_PALL 0x50414c4c
#define pfx_REPE 0x52455045
#define pfx_COUN 0x434f554e
#define pfx_FRAM 0x4652414d
#define pfx_FILE 0x46494c45
#define pfx_A_B  0x29000a00
#define pfx_INTE 0x494e5445
#define pfx_AUST 0x41555354
#define pfx_SPDU 0x53504455
#define pfx_SLWD 0x534c5744

#define pfx_RCHG 0x52434847
#define pfx_PARM 0x5041524d
#define pfx_STMT 0x53544d54

typedef struct STRUCT_PFX_RCHG_HDR
{
 xaULONG orig,dest,len;
 xaUBYTE *data;
 XA_ACTION *act;
 struct STRUCT_PFX_RCHG_HDR *next;
} PFX_RCHG_HDR;

typedef struct STRUCT_PFX_FRAM_HDR
{
 xaULONG frame;
 xaULONG time;
 struct STRUCT_PFX_FRAM_HDR *next;
} PFX_FRAM_HDR;

typedef struct STRUCT_PFX_PLAY_HDR
{
 xaULONG type,data;
 xaLONG count;
 struct STRUCT_PFX_PLAY_HDR *loop;
 struct STRUCT_PFX_PLAY_HDR *next;
} PFX_PLAY_HDR;


