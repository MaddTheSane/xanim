
/*
 * xanim_wav.c
 *
 * Copyright (C) 1994,1995 by Mark Podlipec.
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

/*******************************
 * Revision
 *
 ********************************/


#include "xanim_avi.h"

LONG Is_WAV_File();
ULONG WAV_Read_File();
ULONG wav_max_faud_size;
extern void  AVI_Print_ID();

ULONG wav_format,wav_chans;
ULONG wav_freq,wav_bits,wav_bps;
ULONG wav_snd_time,wav_snd_timelo;
ULONG wav_audio_type;
ULONG UTIL_Get_MSB_Long();
ULONG UTIL_Get_LSB_Long();
ULONG UTIL_Get_LSB_Short();
ULONG XA_Add_Sound();

/*
 *
 */
LONG Is_WAV_File(filename)
char *filename;
{
  FILE *fin;
  ULONG data1,len,data3;

  if ( (fin=fopen(filename,XA_OPEN_MODE)) == 0) return(XA_NOFILE);
  data1 = UTIL_Get_MSB_Long(fin);  /* read past size */
  len   = UTIL_Get_MSB_Long(fin);  /* read past size */
  data3 = UTIL_Get_MSB_Long(fin);  /* read past size */
  fclose(fin);
  if ( (data1 == RIFF_RIFF) && (data3 == RIFF_WAVE)) return(TRUE);
  return(FALSE);
}

ULONG WAV_Read_File(fname,anim_hdr,audio_attempt)
char *fname;
XA_ANIM_HDR *anim_hdr;
ULONG audio_attempt;    /* TRUE if audio is to be attempted */
{
  FILE *fin;
  LONG wav_riff_size;

  if ( (fin=fopen(fname,XA_OPEN_MODE)) == 0)
  {
    fprintf(stderr,"can't open AVI File %s for reading\n",fname);
    return(FALSE);
  }

  wav_max_faud_size = anim_hdr->max_faud_size;
  wav_format = 0;
  wav_chans  = 0;
  wav_freq   = 0;
  wav_bits   = 0;
  wav_bps    = 0;
  wav_audio_type = 0;
  wav_riff_size = 0;

  wav_snd_time = 0;
  wav_snd_timelo = 0;

  while( !feof(fin) )
  { LONG ret;
    ULONG d,ck_id,ck_size;

    ck_id = UTIL_Get_MSB_Long(fin);
    ck_size = UTIL_Get_LSB_Long(fin);
    wav_riff_size -= 8;

DEBUG_LEVEL1
{
  fprintf(stderr,"WAV cid ");
  AVI_Print_ID(stderr,ck_id);
  fprintf(stderr,"  cksize %08lx\n",ck_size);
}
    switch(ck_id)
    {
      case RIFF_RIFF:
	d = UTIL_Get_MSB_Long(fin);
	wav_riff_size = (2*ck_size) - 4;
	DEBUG_LEVEL2
	{
	  fprintf(stderr,"  RIFF form type ");
	  AVI_Print_ID(stderr,d);
	  fprintf(stderr,"\n");
	}
	break;

        case RIFF_fmt:
          { ULONG garb,len;
	    if (ck_size & 1) ck_size++;
	    wav_format = UTIL_Get_LSB_Short(fin);
	    wav_chans  = UTIL_Get_LSB_Short(fin);
	    wav_freq   = UTIL_Get_LSB_Long(fin);
	    garb       = UTIL_Get_LSB_Long(fin);   /* av bytes/sec */
	    garb       = UTIL_Get_LSB_Short(fin);  /* blk align */
	    wav_bits   = UTIL_Get_LSB_Short(fin);  /* bits/sample */
	    len = ck_size - 16;
	    while(len--) fgetc(fin);
	  }
          break;

        case RIFF_data:
	  { ULONG supported = FALSE;
	    ULONG snd_size = ck_size;
	    if (ck_size & 1) ck_size++;
	    switch(wav_format)
	    { /* POD NOTE: eventually use auds_hdr */
	      case WAVE_FORMAT_PCM:
		if (wav_bits == 8) wav_audio_type = XA_AUDIO_LINEAR;
		else if (wav_bits == 16) wav_audio_type = XA_AUDIO_SIGNED;
		else wav_audio_type = XA_AUDIO_INVALID;
		supported = TRUE;
		break;
	      default:
		break;
	    }
	    if (supported == TRUE)
	    { UBYTE *snd = (UBYTE *)malloc(ck_size);
              if (snd==0) TheEnd1("WAV: snd malloc err");
              ret = fread( snd, ck_size, 1, fin);
              if (ret != 1) fprintf(stderr,"WAV: snd rd err\n");
              else
              { int rets;
                rets = XA_Add_Sound(anim_hdr,snd,wav_audio_type, -1,
                   wav_freq, snd_size, &wav_snd_time, &wav_snd_timelo);
              }
	    }
	    else
	    {
	      fprintf(stderr,"WAV: Audio Codec not supported\n");
	      fseek(fin,ck_size,1);
	    }
	  }
	  break;
        default:
	  if ( !feof(fin) )
	  {
            if (wav_riff_size > 0)
            {
                if (ck_size & 0x01) ck_size++; /* pad */
                fseek(fin,ck_size,1); /* move past this chunk */
                DEBUG_LEVEL1
                {
                  fprintf(stderr,"AVI: unknown  chunk ");
                  AVI_Print_ID(stderr,ck_id);
                  fprintf(stderr,"\n");
                }
            }
            else
            {
                AVI_Print_ID(stderr,ck_id);
                fprintf(stderr,"  chunk unknown\n");
                fseek(fin,0,2); /* goto end of file */
            }
	  }
	  break;
    } /* end of ck_id switch */
    wav_riff_size -= ck_size;
    if (ck_size & 0x01) wav_riff_size--; /* odd byte pad */
  } /* while not exitflag */

  fclose(fin);
  if (xa_verbose)
  {
 /* POD NOTE: put switch here */
    fprintf(stderr,"   freq=%ld chans=%ld size=%ld\n",wav_freq,wav_chans,wav_bits);
  }

  anim_hdr->max_faud_size = wav_max_faud_size;
  return(TRUE);
} /* end of read file */



