
/*
 * xa_xmpg.c
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



#define MPG_TYPE_I 1
#define MPG_TYPE_P 2
#define MPG_TYPE_B 3
 
#define MPG_PIC_START	0x00
#define MPG_MIN_SLICE	0x01
#define MPG_MAX_SLICE	0xaf
#define MPG_USR_START	0xb2
#define MPG_SEQ_START	0xb3
#define MPG_EXT_START	0xb5
#define MPG_SEQ_END	0xb7
#define MPG_GOP_START	0xb8

#define MPG_ISO_END	0xb9

#define MPG_PACK_START	0xba
#define MPG_SYS_START	0xbb

#define MPG_UNK_BC    0xbc
#define MPG_UNK_BE    0xbe
#define MPG_UNK_C0    0xc0
#define MPG_UNK_E0    0xe0

 
typedef struct STRUCT_SEQ_HDR
{
  xaULONG width;
  xaULONG height;
  xaULONG aspect;
  xaULONG pic_rate;
  xaULONG bit_rate;
  xaULONG vbv_buff_size;
  xaULONG constrained;
  xaULONG intra_flag;
  xaULONG non_intra_flag;
  xaUBYTE intra_qtab[64];
  xaUBYTE non_intra_qtab[64];
} MPG_SEQ_HDR;

typedef struct STRUCT_SLICE_HDR
{
  xaULONG vert_pos;
  xaULONG fpos;
  xaULONG fsize;
  XA_ACTION *act;
  struct STRUCT_SLICE_HDR *next;
} MPG_SLICE_HDR;
 
typedef struct STRUCT_PIC_HDR
{
  xaULONG type;
  xaULONG time;
  xaULONG vbv_delay;  /* not used */
  xaULONG full_forw_flag;
  xaULONG forw_r_size;
  xaULONG forw_f;
  xaULONG full_back_flag;
  xaULONG back_r_size;
  xaULONG back_f;
  xaULONG vert_pos;
  xaULONG q_scale;
  MPG_SEQ_HDR *seq_hdr;
  xaULONG slice_cnt;
  MPG_SLICE_HDR *slice_1st;
  MPG_SLICE_HDR *slice_last;
  MPG_SLICE_HDR slices[2];  /* must be last */
} MPG_PIC_HDR;


