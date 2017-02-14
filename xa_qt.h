 
/*
 * xa_qt.h
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
 
#define QT_moov 0x6D6F6F76
#define QT_trak 0x7472616B
#define QT_mdia 0x6D646961
#define QT_minf 0x6D696E66
#define QT_stbl 0x7374626C
#define QT_rmra 0x726d7261
#define QT_cmov 0x636d6f76
#define QT_dcom 0x64636F6D
#define QT_zlib 0x7A6C6962
#define QT_cmvd 0x636D7664
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
#define QT_azpr  0x617A7072
#define QT_CVID  0x43564944
#define QT_cvid  0x63766964
#define QT_jpeg  0x6a706567
#define QT_MJPG  0x4d4a5047
#define QT_mjpg  0x6d6a7067
#define QT_mjpa  0x6d6a7061
#define QT_mjpb  0x6d6a7062
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
#define QT_IV41  0x49563431
#define QT_iv41  0x69763431
#define QT_kpcd  0x6b706364
#define QT_KPCD  0x4b504344
#define QT_cram 0x6372616D
#define QT_CRAM 0x4352414D
#define QT_wham 0x7768616d
#define QT_WHAM 0x5748414d
#define QT_msvc 0x6D737663
#define QT_MSVC 0x4d535643
#define QT_SVQ1 0x53565131
#define QT_UCOD 0x55434f44
#define QT_8BPS	0x38425053

/* added by konstantin priblouda to support sgi converted quicktimes */
/* this represents luminance only 8 bit images, uncompressed */
/* I suppose sgi movie library goes far beyond apple ideas :) */
/* #define QT_sgi_raw_gray8  0x72617733   POD renamed to _raw3 */
#define QT_raw3	0x72617733


/*-------------- VIDEO/AUDIO CODECS ---------------*/
#define QT_raw   0x72617720
/*-------------- AUDIO CODECS ---------------*/
#define QT_raw00 0x00000000
#define QT_twos  0x74776f73
#define QT_MAC3  0x4d414333
#define QT_MAC6  0x4d414336
#define QT_ima4  0x696d6134
#define QT_ulaw  0x756c6177
#define QT_Qclp  0x51636c70
#define QT_QDMC	 0x51444d43
#define QT_agsm  0x6167736d
/*-------------- misc ----------------------*/
#define QT_free 0x66726565
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
#define QT_ctab 0x63746162   /* ctab: color table for 16/24 anims on 8 bit */

typedef struct
{
  xaULONG format;
  xaULONG compression;
  xaULONG dref_id;
  xaULONG version;
  xaULONG codec_rev;
  xaULONG vendor;
  xaUSHORT chan_num;
  xaUSHORT bits_samp;
  xaUSHORT comp_id;
  xaUSHORT pack_size;
  xaUSHORT samp_rate;
  xaUSHORT pad;
  xaULONG  bps;		/* convenience for me */
} QTS_CODEC_HDR;

typedef struct
{
  xaULONG version;                /* version/flags */
  xaULONG creation;               /* creation time */
  xaULONG modtime;                /* modification time */
  xaULONG timescale;
  xaULONG duration;
  xaULONG rate;
  xaUSHORT volume;
  xaULONG  r1;
  xaULONG  r2;
  xaULONG matrix[3][3];
  xaUSHORT r3;
  xaULONG  r4;
  xaULONG pv_time;
  xaULONG pv_durat;
  xaULONG post_time;
  xaULONG sel_time;
  xaULONG sel_durat;
  xaULONG cur_time;
  xaULONG nxt_tk_id;
} QT_MVHDR;

typedef struct
{
  xaULONG version;                /* version/flags */
  xaULONG creation;               /* creation time */
  xaULONG modtime;                /* modification time */
  xaULONG trackid;
  xaULONG timescale;
  xaULONG duration;
  xaULONG time_off;
  xaULONG priority;
  xaUSHORT layer;
  xaUSHORT alt_group;
  xaUSHORT volume;
  xaULONG matrix[3][3];
  xaULONG tk_width;
  xaULONG tk_height;
  xaUSHORT pad;
} QT_TKHDR;

typedef struct
{
  xaULONG version;                /* version/flags */
  xaULONG creation;               /* creation time */
  xaULONG modtime;                /* modification time */
  xaULONG timescale;
  xaULONG duration;
  xaUSHORT language;
  xaUSHORT quality;
} QT_MDHDR;

typedef struct
{
  xaULONG version;                /* version/flags */
  xaULONG type;
  xaULONG subtype;
  xaULONG vendor;
  xaULONG flags;
  xaULONG mask;
} QT_HDLR_HDR;


typedef struct QT_FRAME_STRUCT
{
  xaULONG time;
  xaULONG timelo;
  XA_ACTION *act;
  struct QT_FRAME_STRUCT *next;
} QT_FRAME;

typedef struct
{
  xaULONG width;
  xaULONG height;
  xaULONG depth;
  xaULONG compression;
  xaULONG xapi_rev;
  xaULONG (*decoder)();
  void *dlta_extra;
  XA_CHDR *chdr;
} QTV_CODEC_HDR;

typedef struct
{
  xaULONG first;
  xaULONG num;
  xaULONG tag;
} QT_S2CHUNK_HDR;

typedef struct
{
  xaULONG cnt;
  xaULONG time;
  xaULONG timelo;
} QT_T2SAMP_HDR;


