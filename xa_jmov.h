
/*
 * xa_jmov.h
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

#include "xanim.h"

typedef struct JMOV_FRAME_STRUCT
{
  xaULONG time;
  xaULONG timelo;
  XA_ACTION *act;
  struct JMOV_FRAME_STRUCT *next;
} JMOV_FRAME;
 
 
typedef struct
{
  xaULONG version;
  xaULONG fps;            /* frames per second */
  xaULONG frames;         /* total video frames */
  xaULONG width;
  xaULONG height;
  xaULONG bandwidth;      /* 1kbytes/sec need to playback */
  xaULONG qfactor;        /* quantization scaling factor */
  xaULONG mapsize;        /* colors in colormap */
  xaULONG indexbuf;       /* offset in file of frame indexes */
  xaULONG tracks;         /* num of audio tracks */
  xaULONG volbase;        /* base volume */
  xaULONG audioslice;     /* audio bytes per frame */
/*Audio_hdr */          /* Audio_hdr?!?  for what machine?? */
  xaULONG freq;
  xaULONG chans;
  xaULONG prec;
  xaULONG codec;
/*filler(48) */
} JMOV_HDR;
 
#define JMOV_AUDIO_ENC_NONE (0)   /* no encoding assigned */
#define JMOV_AUDIO_ENC_ULAW (1)   /* u-law encoding */
#define JMOV_AUDIO_ENC_ALAW (2)   /* A-law encoding */
#define JMOV_AUDIO_ENC_PCM  (3)   /* Linear PCM encoding */

