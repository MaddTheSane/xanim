
/*
 * xa_avi.h
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

/* The following copyright applies to all Ultimotion Segments of the Code:
 *
 * "Copyright International Business Machines Corporation 1994, All rights
 *  reserved. This product uses Ultimotion(tm) IBM video technology."
 *
 */


#include "xanim.h"

#define RIFF_RIFF 0x52494646
#define RIFF_LIST 0x4C495354
#define RIFF_avih 0x61766968
#define RIFF_strd 0x73747264
#define RIFF_strh 0x73747268
#define RIFF_strf 0x73747266
#define RIFF_vedt 0x76656474
#define RIFF_JUNK 0x4A554E4B
#define RIFF_idx1 0x69647831
/*********Chunk Names***************/
#define RIFF_FF00 0xFFFF0000
#define RIFF_00   0x30300000
#define RIFF_01   0x30310000
#define RIFF_02   0x30320000
#define RIFF_03   0x30330000
#define RIFF_00pc 0x30307063
#define RIFF_01pc 0x30317063
#define RIFF_00dc 0x30306463
#define RIFF_00dx 0x30306478
#define RIFF_00db 0x30306462
#define RIFF_00xx 0x30307878
#define RIFF_00id 0x30306964
#define RIFF_00rt 0x30307274
#define RIFF_0021 0x30303231
#define RIFF_00iv 0x30306976
#define RIFF_0031 0x30303331
#define RIFF_0032 0x30303332
#define RIFF_00vc 0x30305643
#define RIFF_00xm 0x3030786D
#define RIFF_01wb 0x30317762
#define RIFF_01dc 0x30306463
/*********VIDEO CODECS**************/
#define RIFF_CRAM 0x4352414D
#define RIFF_rgb  0x00000000
#define RIFF_RGB  0x52474220
#define RIFF_rle8 0x01000000
#define RIFF_RLE8 0x524c4538
#define RIFF_rle4 0x02000000
#define RIFF_RLE4 0x524c4534
#define RIFF_none 0x0000FFFF
#define RIFF_NONE 0x4e4f4e45
#define RIFF_pack 0x0100FFFF
#define RIFF_PACK 0x5041434b
#define RIFF_tran 0x0200FFFF
#define RIFF_TRAN 0x5452414e
#define RIFF_ccc  0x0300FFFF
#define RIFF_CCC  0x43434320
#define RIFF_jpeg 0x0400FFFF
#define RIFF_JPEG 0x4A504547
#define RIFF_MJPG 0x4d4a5047
#define RIFF_IJPG 0x494a5047
#define RIFF_RT21 0x52543231
#define RIFF_rt21 0x72743231
#define RIFF_IV31 0x49563331
#define RIFF_iv31 0x69763331
#define RIFF_IV32 0x49563332
#define RIFF_iv32 0x69763332
#define RIFF_CVID 0x43564944
#define RIFF_cvid 0x63766964
#define RIFF_ULTI 0x756c7469
#define RIFF_ulti 0x554c5449
#define RIFF_YUV9 0x59565539
#define RIFF_YVU9 0x59555639
#define RIFF_XMPG 0x584D5047
#define RIFF_xmpg 0x786D7067
/*********** WAV file stuff *************/
#define RIFF_fmt  0x666D7420
#define RIFF_data 0x64617461
/*********** FND in MJPG **********/
#define RIFF_ISFT 0x49534654
#define RIFF_IDIT 0x49444954

#define RIFF_00AM 0x3030414d
#define RIFF_hdrl 0x6864726C
#define RIFF_strl 0x7374726C
#define RIFF_DISP 0x44495350
#define RIFF_ISBJ 0x4953424a

/* RIFF Types */
#define RIFF_AVI  0x41564920
#define RIFF_WAVE 0x57415645

/* fcc Types */
#define RIFF_vids 0x76696473
#define RIFF_auds 0x61756473
#define RIFF_pads 0x70616473
 
/* fcc handlers */
#define RIFF_RLE  0x524c4520
#define RIFF_msvc 0x6D737663
#define RIFF_MSVC 0x4d535643

typedef struct
{
  xaULONG ckid;
  xaULONG flags;
  xaULONG chunk_offset;   /* position of chunk rel to movi list include 8b hdr*/
  xaULONG chunk_size;     /* length of chunk excluding 8 bytes for RIFF hdr */
} AVI_INDEX_ENTRY;

/* Flags for AVI_INDEX_ENTRY */
#define AVIIF_LIST          0x00000001L
#define AVIIF_TWOCC         0x00000002L
        /* keyframe doesn't need previous info to be decompressed */
#define AVIIF_KEYFRAME      0x00000010L
        /* this chunk needs the frames following it to be used */
#define AVIIF_FIRSTPART     0x00000020L
        /* this chunk needs the frames before it to be used */
#define AVIIF_LASTPART      0x00000040L
#define AVIIF_MIDPART       (AVIIF_LASTPART|AVIIF_FIRSTPART)
        /* this chunk doesn't affect timing ie palette change */
#define AVIIF_NOTIME        0x00000100L
#define AVIIF_COMPUSE       0x0FFF0000L
 
typedef struct
{
  xaULONG us_frame;       /* MicroSecPerFrame - timing between frames */
  xaULONG max_bps;        /* MaxBytesPerSec - approx bps system must handle */
  xaULONG pad_gran;       /* */
  xaULONG flags;          /* Flags */
  xaULONG tot_frames;     /* TotalFrames */
  xaULONG init_frames;    /* InitialFrames - initial frame before interleaving */
  xaULONG streams;        /* Streams */
  xaULONG sug_bsize;      /* SuggestedBufferSize */
  xaULONG width;          /* Width */
  xaULONG height;         /* Height */
  xaULONG scale;          /* Scale */
  xaULONG rate;           /* Rate */
  xaULONG start;          /* Start */
  xaULONG length;         /* Length */
} AVI_HDR;
 
/* AVI_HDR Flags */
        /* had idx1 chunk */
#define AVIF_HASINDEX           0x00000010
        /* must use idx1 chunk to determine order */
#define AVIF_MUSTUSEINDEX       0x00000020
        /* AVI file is interleaved */
#define AVIF_ISINTERLEAVED      0x00000100
        /* specially allocated used for capturing real time video */
#define AVIF_WASCAPTUREFILE     0x00010000
        /* contains copyrighted data */
#define AVIF_COPYRIGHTED        0x00020000
 
 
typedef struct
{
  xaULONG fcc_type;       /* fccType  {vids} */
  xaULONG fcc_handler;    /* fccHandler {msvc,RLE} */
  xaULONG flags;          /* Flags */
  xaULONG priority;       /* Priority*/
  xaULONG init_frames;    /* InitialFrames */
  xaULONG scale;          /* Scale */
  xaULONG rate;           /* Rate */
  xaULONG start;          /* Start */
  xaULONG length;         /* Length In units above...*/
  xaULONG sug_bsize;      /* SuggestedBufferSize */
  xaULONG quality;        /* Quality */
  xaULONG samp_size;      /* SampleSize */
} AVI_STREAM_HDR;
/* AVI_STREAM_HDR Flags */
#define AVISF_DISABLED                  0x00000001
#define AVISF_VIDEO_PALCHANGES          0x00010000
 
 
typedef struct /* BitMapInfoHeader */
{
  xaULONG size;           /* Size */
  xaULONG width;          /* Width */
  xaULONG height;         /* Height */
  xaULONG planes;         /* short Planes */
  xaULONG bit_cnt;        /* short BitCount */
  xaULONG compression;    /* Compression {1} */
  xaULONG image_size;     /* SizeImage */
  xaULONG xpels_meter;    /* XPelsPerMeter */
  xaULONG ypels_meter;    /* XPelsPerMeter */
  xaULONG num_colors;     /* ClrUsed */
  xaULONG imp_colors;     /* ClrImportant */
} VIDS_HDR;
 

typedef struct AVI_FRAME_STRUCT
{
  xaULONG time;
  xaULONG timelo;
  XA_ACTION *act;
  struct AVI_FRAME_STRUCT *next;
} AVI_FRAME;

typedef struct /* BitMapInfoHeader */
{
  xaULONG format;         /* S format */
  xaULONG channels;       /* S channels */
  xaULONG rate;           /* L rate */
  xaULONG av_bps;         /* L average bytes/sec */
  xaULONG align;          /* S alignment */
  xaULONG size;           /* S size */
  xaULONG style;          /* - SIGN2 or unsigned */
} AUDS_HDR;

/**** from public Microsoft RIFF docs ******/
#define WAVE_FORMAT_UNKNOWN             (0x0000)
#define WAVE_FORMAT_PCM                 (0x0001)
#define WAVE_FORMAT_ADPCM               (0x0002)
#define WAVE_FORMAT_ALAW                (0x0006)
#define WAVE_FORMAT_MULAW               (0x0007)
#define WAVE_FORMAT_OKI_ADPCM           (0x0010)
#define WAVE_FORMAT_DIGISTD             (0x0015)
#define WAVE_FORMAT_DIGIFIX             (0x0016)
#define IBM_FORMAT_MULAW                (0x0101)
#define IBM_FORMAT_ALAW                 (0x0102)
#define IBM_FORMAT_ADPCM                (0x0103)
/*********************/

