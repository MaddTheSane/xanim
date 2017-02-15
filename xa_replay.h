/*
 * xa_replay.h
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

#ifndef __XA_REPLAY_H__
#define __XA_REPLAY_H__

#include "xanim.h"

typedef struct ARM_FRAME_STRUCT
{
  xaULONG time;
  xaULONG timelo;
  XA_ACTION *act;
  struct ARM_FRAME_STRUCT *next;
} ARM_FRAME;
 
 
typedef struct
{
  xaULONG vid_codec;	/* */
  xaULONG width;		/*I            */
  xaULONG height;		/*I            */
  xaULONG depth;		/*I  16 or 8 */
  xaULONG aud_codec;	/*               */
  xaULONG aud_freq;	/*F  audio freq HZ */
  xaULONG aud_chans;	/*I  audio chans */
  xaULONG aud_prec;	/*I  snd precision  linear/unsigned */
  xaULONG fp_chunk;	/*I  frames per chunk */	
  xaULONG chunk_cnt;	/*I  chunk count */
  xaULONG ev_chk_size;	/*I  even chunk size */
  xaULONG od_chk_size;	/*I  odd chunk size */
  xaULONG idx_offset;	/*I  offset to chunk index */
  xaULONG sprite_off;	/*I  offset to helpful sprite??? */
  xaULONG sprite_size;	/*I  size of sprite info */
  xaULONG key_offset;	/*I  offset to key frames  -1, 0 no keys */
  double fps;		/*F  frames per second */
} ARM_HDR;
 
#define ARM_VIDEO_MOVL_RGB  0
#define ARM_VIDEO_MOVL_YUV  1
#define ARM_VIDEO_MOVB_RGB  2
#define ARM_VIDEO_MOVB_YUV  3
#define ARM_VIDEO_UNK    99

#define ARM_AUDIO_NONE    0
#define ARM_AUDIO_FORM1   1
#define ARM_AUDIO_UNK    99

#define ARM_VIDEO_RGB 0
#define ARM_VIDEO_YUV 1

#endif
