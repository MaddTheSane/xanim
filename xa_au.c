
/*
 * xa_au.c
 *
 * Copyright (C) 1994-1998,1999 by Mark Podlipec.
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

/* NOTE: This code is based on code from Sun */

/*
 * Copyright 1991, 1992, 1993 Guido van Rossum And Sundry Contributors.
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Guido van Rossum And Sundry Contributors are not responsible for
 * the consequences of using this software.
 */

/*******************************
 * Revision
 *
 ********************************/


#include "xanim.h"
#include "xa_formats.h"

#define AU_MAGIC 0x2e736e64
#define AU_HSIZE 0x18

#define AU_ULAW        1
#define AU_LIN_8       2
#define AU_LIN_16      3

xaULONG au_max_faud_size;
extern void  AVI_Print_ID();

static xaULONG au_format,au_chans;
static xaULONG au_freq,au_bits,au_bps;
static xaULONG au_snd_time,au_snd_timelo;
static xaULONG au_audio_type;
xaULONG UTIL_Get_MSB_Long();
xaULONG UTIL_Get_LSB_Long();
xaULONG UTIL_Get_LSB_Short();
xaULONG XA_Add_Sound();

xaULONG AU_Read_File(const char *fname,XA_ANIM_HDR *anim_hdr,xaULONG audio_attempt)
{
  FILE *fin;
  xaLONG tmp,au_hdr_size,au_data_size;

  if ( (fin=fopen(fname,XA_OPEN_MODE)) == 0)
  {
    fprintf(stderr,"can't open AVI File %s for reading\n",fname);
    return(xaFALSE);
  }

  au_max_faud_size = anim_hdr->max_faud_size;
  au_format = 0;
  au_chans  = 0;
  au_freq   = 0;
  au_bits   = 0;
  au_bps    = 0;
  au_audio_type = 0;
  au_hdr_size = 0;
  au_data_size = 0;

  au_snd_time = 0;
  au_snd_timelo = 0;

/* Read Header */
  tmp          = UTIL_Get_MSB_Long(fin);  /* magic */
  au_hdr_size  = UTIL_Get_MSB_Long(fin);  /* size of header */
  if (au_hdr_size < 0x18) return(xaFALSE); /* header too small */
  au_data_size = UTIL_Get_MSB_Long(fin);  /* size of header */
  if (au_data_size == 0xffffffff) /* unknown data size */
  { int ret,fpos = ftell(fin);
    ret = fseek(fin,0,2);
    if (ret < 0) return(xaFALSE);
    au_data_size = ftell(fin) - au_hdr_size;
    ret = fseek(fin,fpos,0);
    if (ret < 0) return(xaFALSE);
  }
  au_format = UTIL_Get_MSB_Long(fin);
  au_freq   = UTIL_Get_MSB_Long(fin);
  au_chans  = UTIL_Get_MSB_Long(fin);
  switch(au_format)
  {
    case AU_ULAW:
	au_bits = 8;
	au_bps  = 1;
	au_audio_type = XA_AUDIO_SUN_AU;
	break;
    case AU_LIN_8:
	au_bits = 8;
	au_bps  = 1;
	au_audio_type = XA_AUDIO_LINEAR;
	break;
    case AU_LIN_16:
	au_bits = 16;
	au_bps  = 2;
	au_audio_type = XA_AUDIO_LINEAR 
				| XA_AUDIO_BIGEND_MSK | XA_AUDIO_BPS_2_MSK;
	break;
    default:
	fprintf(stderr,"AU: Sound Format %x not yet supported\n",
								au_format);
	return(xaFALSE);
	break;
  }
  if ((au_chans < 1) || (au_chans > 2))
  {
    fprintf(stderr,"AU: Chans %d not supported.\n",au_chans);
    return(xaFALSE);
  }
  else if (au_chans == 2) au_audio_type |= XA_AUDIO_STEREO_MSK;
 
  anim_hdr->total_time =  (xaULONG)
	(((float)au_data_size * 1000.0)
		/ (float)(au_chans * au_bps) )
		/ (float)(au_freq);

  fseek(fin,au_hdr_size,0);
  { int ret;
    xaUBYTE *snd = (xaUBYTE *)malloc(au_data_size);
    if (snd==0) TheEnd1("AU: snd malloc err");
    ret = fread( snd, au_data_size, 1, fin);
    if (ret != 1) fprintf(stderr,"AU: snd rd err\n");
    else
    { int rets;
      rets = XA_Add_Sound(anim_hdr,snd,au_audio_type, -1,
      au_freq, au_data_size, &au_snd_time, &au_snd_timelo, 0, 0);
    }
  }

  fclose(fin);
  if (xa_verbose)
  {
 /* POD NOTE: put switch here */
    fprintf(stderr,"   freq=%d chans=%d size=%d\n",au_freq,au_chans,au_bits);
  }

  anim_hdr->max_faud_size = au_max_faud_size;
  return(xaTRUE);
} /* end of read file */


