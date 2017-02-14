 
/*
 * xanim_qt.h
 *
 * Copyright (C) 1993,1994,1995 by Mark Podlipec.
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
 
#define QT_moov 0x6D6F6F76
#define QT_trak 0x7472616B
#define QT_mdia 0x6D646961
#define QT_minf 0x6D696E66
#define QT_stbl 0x7374626C
/*-------------- LISTS ---------------------*/
#define QT_edts 0x65647473
/*-------------- STUFF ---------------------*/
#define QT_hdlr 0x68646C72
#define QT_mvhd 0x6D766864
#define QT_tkhd 0x746B6864
#define QT_elst 0x656C7374
#define QT_mdhd 0x6D646864
#define QT_stsd 0x73747364
#define QT_stts 0x73747473
#define QT_stss 0x73747373
#define QT_stsc 0x73747363
#define QT_stsz 0x7374737a
#define QT_stco 0x7374636f
/*-------------- VIDEO CODECS ---------------*/
#define QT_rle   0x726c6520
#define QT_smc   0x736D6320
#define QT_rpza  0x72707A61
#define QT_cvid  0x63766964
#define QT_jpeg  0x6a706567
#define QT_SPIG  0x53504947
#define QT_yuv2  0x79757632
#define QT_PGVV  0x50475656
#define QT_YUV9  0x59565539
#define QT_YVU9  0x59555639
#define QT_RT21  0x52543231
#define QT_rt21  0x72743231
#define QT_IV31  0x49563331
#define QT_iv31  0x69763331
#define QT_IV32  0x49563332
#define QT_iv32  0x69763332

/*-------------- VIDEO/AUDIO CODECS ---------------*/
#define QT_raw   0x72617720
/*-------------- AUDIO CODECS ---------------*/
#define QT_raw00 0x00000000
#define QT_twos  0x74776f73
#define QT_MAC6  0x4d414336
/*-------------- misc ----------------------*/
#define QT_vmhd 0x766D6864
#define QT_dinf 0x64696e66
#define QT_appl 0x6170706C
#define QT_mdat 0x6D646174
#define QT_smhd 0x736d6864
#define QT_stgs 0x73746773
#define QT_udta 0x75647461
#define QT_skip 0x736B6970
#define QT_gmhd 0x676d6864
#define QT_text 0x74657874
#define QT_clip 0x636C6970   /* clip ??? contains crgn atom  */
#define QT_crgn 0x6372676E   /* crgn ??? contain coordinates?? */

typedef struct
{
  ULONG compression;
  ULONG dref_id;
  ULONG version;
  ULONG codec_rev;
  ULONG vendor;
  USHORT chan_num;
  USHORT bits_samp;
  USHORT comp_id;
  USHORT pack_size;
  USHORT samp_rate;
  USHORT pad;
  ULONG  bps;		/* convenience for me */
} QTS_CODEC_HDR;

typedef struct
{
  ULONG version;                /* version/flags */
  ULONG creation;               /* creation time */
  ULONG modtime;                /* modification time */
  ULONG timescale;
  ULONG duration;
  ULONG rate;
  USHORT volume;
  ULONG  r1;
  ULONG  r2;
  ULONG matrix[3][3];
  USHORT r3;
  ULONG  r4;
  ULONG pv_time;
  ULONG pv_durat;
  ULONG post_time;
  ULONG sel_time;
  ULONG sel_durat;
  ULONG cur_time;
  ULONG nxt_tk_id;
} QT_MVHDR;

typedef struct
{
  ULONG version;                /* version/flags */
  ULONG creation;               /* creation time */
  ULONG modtime;                /* modification time */
  ULONG trackid;
  ULONG timescale;
  ULONG duration;
  ULONG time_off;
  ULONG priority;
  USHORT layer;
  USHORT alt_group;
  USHORT volume;
  ULONG matrix[3][3];
  ULONG tk_width;
  ULONG tk_height;
  USHORT pad;
} QT_TKHDR;

typedef struct
{
  ULONG version;                /* version/flags */
  ULONG creation;               /* creation time */
  ULONG modtime;                /* modification time */
  ULONG timescale;
  ULONG duration;
  USHORT language;
  USHORT quality;
} QT_MDHDR;

typedef struct
{
  ULONG version;                /* version/flags */
  ULONG type;
  ULONG subtype;
  ULONG vendor;
  ULONG flags;
  ULONG mask;
} QT_HDLR_HDR;


typedef struct QT_FRAME_STRUCT
{
  ULONG time;
  ULONG timelo;
  XA_ACTION *act;
  struct QT_FRAME_STRUCT *next;
} QT_FRAME;

typedef struct
{
  ULONG width;
  ULONG height;
  ULONG depth;
  ULONG compression;
  XA_CHDR *chdr;
} QTV_CODEC_HDR;

typedef struct
{
  ULONG first;
  ULONG num;
  ULONG tag;
} QT_S2CHUNK_HDR;

typedef struct
{
  ULONG cnt;
  ULONG time;
  ULONG timelo;
} QT_T2SAMP_HDR;

