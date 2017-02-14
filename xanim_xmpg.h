
/*
 * xanim_xmpg.c
 *
 * Copyright (C) 1995 by Mark Podlipec.
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



#define MPG_TYPE_I 1
#define MPG_TYPE_P 2
#define MPG_TYPE_B 3
 
#define MPG_PIC_START 0x00
#define MPG_MIN_SLICE 0x01
#define MPG_MAX_SLICE 0xaf
#define MPG_USR_START 0xb2
#define MPG_SEQ_START 0xb3
#define MPG_EXT_START 0xb5
#define MPG_SEQ_END   0xb7
#define MPG_GOP_START 0xb8
#define MPG_UNK_BA    0xba
#define MPG_UNK_BB    0xbb
#define MPG_UNK_C0    0xc0
#define MPG_UNK_E0    0xe0

 
typedef struct STRUCT_SEQ_HDR
{
  ULONG width;
  ULONG height;
  ULONG aspect;
  ULONG pic_rate;
  ULONG bit_rate;
  ULONG vbv_buff_size;
  ULONG constrained;
  ULONG intra_flag;
  ULONG non_intra_flag;
  UBYTE intra_qtab[64];
  UBYTE non_intra_qtab[64];
} MPG_SEQ_HDR;

typedef struct STRUCT_SLICE_HDR
{
  ULONG vert_pos;
  ULONG fpos;
  ULONG fsize;
  XA_ACTION *act;
  struct STRUCT_SLICE_HDR *next;
} MPG_SLICE_HDR;
 
typedef struct STRUCT_PIC_HDR
{
  ULONG type;
  ULONG time;
  ULONG vbv_delay;  /* not used */
  ULONG full_forw_flag;
  ULONG forw_r_size;
  ULONG forw_f;
  ULONG full_back_flag;
  ULONG back_r_size;
  ULONG back_f;
  ULONG vert_pos;
  ULONG q_scale;
  MPG_SEQ_HDR *seq_hdr;
  ULONG slice_cnt;
  MPG_SLICE_HDR *slice_1st;
  MPG_SLICE_HDR *slice_last;
  MPG_SLICE_HDR slices[2];  /* must be last */
} MPG_PIC_HDR;

