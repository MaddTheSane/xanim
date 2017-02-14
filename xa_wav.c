
/*
 * xa_wav.c
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

/*******************************
 * Revision
 *
 ********************************/


#include "xa_avi.h"

xaULONG WAV_Read_File();
extern void AVI_Print_ID();
extern void AVI_Print_Audio_Type();

static AUDS_HDR auds_hdr;

static xaULONG wav_max_faud_size;
static xaULONG wav_snd_time,wav_snd_timelo;
static xaULONG wav_audio_type;
extern xaULONG XA_Add_Sound();

#ifdef XA_GSM
extern void GSM_Init();
#endif

xaULONG WAV_Read_File(fname,anim_hdr,audio_attempt)
char *fname;
XA_ANIM_HDR *anim_hdr;
xaULONG audio_attempt;    /* xaTRUE if audio is to be attempted */
{
  XA_INPUT *xin = anim_hdr->xin;
  xaLONG wav_riff_size;

	/* .wavs are audio only files */
  if (audio_attempt == xaFALSE) return(xaFALSE);

  wav_max_faud_size = anim_hdr->max_faud_size;

  auds_hdr.format	= 0;
  auds_hdr.channels	= 0;
  auds_hdr.rate		= 0;
  auds_hdr.av_bps	= 0;
  auds_hdr.blockalign	= 0;
  auds_hdr.size		= 0;
  auds_hdr.ext_size	= 0;
  auds_hdr.samps_block	= 1;
  auds_hdr.num_coefs	= 0;
  auds_hdr.coefs	= 0;
  auds_hdr.byte_cnt	= 0;


  wav_audio_type = 0;
  wav_riff_size = 0;

  wav_snd_time = 0;
  wav_snd_timelo = 0;

  xin->Set_EOF(xin,9);
  while( !xin->At_EOF(xin,8) )
  { xaLONG ret;
    xaULONG d,ck_id,ck_size;

    ck_id   = xin->Read_MSB_U32(xin);
    ck_size = xin->Read_LSB_U32(xin);
    wav_riff_size -= 8;

DEBUG_LEVEL1
{
  fprintf(stdout,"WAV cid ");
  AVI_Print_ID(stdout,ck_id);
  fprintf(stdout,"  cksize %08x\n",ck_size);
}
    switch(ck_id)
    {
      case RIFF_RIFF:
	xin->Set_EOF(xin,(ck_size + 8));
	d = xin->Read_MSB_U32(xin);
	wav_riff_size = (2*ck_size) - 4;
	DEBUG_LEVEL2
	{
	  fprintf(stdout,"  RIFF form type ");
	  AVI_Print_ID(stdout,d);
	  fprintf(stdout,"\n");
	}
	break;

        case RIFF_fmt:
          { xaULONG len;
	    if (ck_size & 1) ck_size++;
	    auds_hdr.format	= xin->Read_LSB_U16(xin);
	    auds_hdr.channels	= xin->Read_LSB_U16(xin);
	    auds_hdr.rate	= xin->Read_LSB_U32(xin);
	    auds_hdr.av_bps	= xin->Read_LSB_U32(xin);
	    auds_hdr.blockalign = xin->Read_LSB_U16(xin);
	    len = ck_size - 14;

	    if (len > 0)  
	    { auds_hdr.size	= xin->Read_LSB_U16(xin);
	      len -= 2;
	    } else auds_hdr.size	= 8;

	    if (auds_hdr.format == WAVE_FORMAT_ADPCM)
	    { int i;
	      auds_hdr.ext_size	   = xin->Read_LSB_U16(xin);
	      auds_hdr.samps_block = xin->Read_LSB_U16(xin);
	      auds_hdr.num_coefs   = xin->Read_LSB_U16(xin);	len -= 6;
	      if (xa_verbose) fprintf(stdout,
				" MSADPM_EXT: sampblk %d numcoefs %d\n",
				auds_hdr.samps_block, auds_hdr.num_coefs);
	      for(i=0; i < auds_hdr.num_coefs; i++)
	      { xaSHORT coef1, coef2;
	        coef1 = xin->Read_LSB_U16(xin);     	
	        coef2 = xin->Read_LSB_U16(xin);     	
	        len -= 4;
	        if (xa_verbose) fprintf(stdout,"%d) coef1 %d coef2 %d\n",
							i, coef1, coef2);
	      }
	    }
	    else if (auds_hdr.format == WAVE_FORMAT_DVI_ADPCM)
	    {
	      auds_hdr.ext_size	   = xin->Read_LSB_U16(xin);
	      auds_hdr.samps_block = xin->Read_LSB_U16(xin);	len -= 4;
	      if (xa_verbose) fprintf(stdout," DVI: samps per block %d\n",
						auds_hdr.samps_block);
	    }
	    else if (auds_hdr.format == WAVE_FORMAT_GSM610)
	    {
	      auds_hdr.ext_size	   = xin->Read_LSB_U16(xin);
	      auds_hdr.samps_block = xin->Read_LSB_U16(xin);	len -= 4;
	      if (xa_verbose) fprintf(stdout," GSM: samps per block %d\n",
						auds_hdr.samps_block);
	    }
	    /* xin->Seek_FPos(xin,len,1); */
	    while(len--) xin->Read_U8(xin);
	  }
          break;

        case RIFF_data:
	  { xaULONG supported = xaFALSE;
	    xaULONG the_snd_size = ck_size;
	    xaULONG pad_flag = xaFALSE;

	    if (ck_size & 1) pad_flag = xaTRUE;
	    switch(auds_hdr.format)
	    { /* POD NOTE: eventually use auds_hdr */
	      case WAVE_FORMAT_PCM:
		if (auds_hdr.size == 8) wav_audio_type = XA_AUDIO_LINEAR;
		else if (auds_hdr.size == 16) 
			wav_audio_type = XA_AUDIO_SIGNED | XA_AUDIO_BPS_2_MSK;
		else wav_audio_type = XA_AUDIO_INVALID;
		if (auds_hdr.channels == 2) 
				wav_audio_type |= XA_AUDIO_STEREO_MSK;
		supported = xaTRUE;
		break;
	      case WAVE_FORMAT_MULAW:
		wav_audio_type = XA_AUDIO_ULAW;
		if (auds_hdr.channels == 2) 
				wav_audio_type |= XA_AUDIO_STEREO_MSK;
		supported = xaTRUE;
		break;
	      case WAVE_FORMAT_ALAW:
		wav_audio_type = XA_AUDIO_ALAW;
		if (auds_hdr.channels == 2) 
				wav_audio_type |= XA_AUDIO_STEREO_MSK;
		supported = xaTRUE;
		break;
	      case WAVE_FORMAT_ADPCM:
		if (auds_hdr.size == 4) wav_audio_type = XA_AUDIO_ADPCM;
		else wav_audio_type = XA_AUDIO_INVALID;
		if (auds_hdr.channels == 2) 
				wav_audio_type |= XA_AUDIO_STEREO_MSK;
		supported = xaTRUE;
		break;
	      case WAVE_FORMAT_DVI_ADPCM:
		wav_audio_type = XA_AUDIO_DVI;
		if (auds_hdr.channels == 2) 
				wav_audio_type |= XA_AUDIO_STEREO_MSK;
		supported = xaTRUE;
		break;
#ifdef XA_GSM
	      case WAVE_FORMAT_GSM610:
		wav_audio_type = XA_AUDIO_MSGSM;
		if (auds_hdr.channels == 2) 
				wav_audio_type |= XA_AUDIO_STEREO_MSK;
		supported = xaTRUE;
		GSM_Init();
		break;
#endif
	      default:
		break;
	    }
	    if (supported == xaTRUE)
	    { xaULONG max_snd_chunk_size = XA_SND_CHUNK_SIZE;

		/* Make sure chunks are integer multiple of blockalign */
	      if (auds_hdr.blockalign)
	      { max_snd_chunk_size /= auds_hdr.blockalign;
		max_snd_chunk_size *= auds_hdr.blockalign;
	      }

		/* needed to guard against truncated files */
	      if (xa_file_flag==xaTRUE)
	      { xaLONG len, end_fpos, tmp_fpos = xin->Get_FPos(xin);
		xin->Seek_FPos(xin,0,2);
		end_fpos = xin->Get_FPos(xin);
		xin->Seek_FPos(xin,tmp_fpos,0);
		len = end_fpos - tmp_fpos;
		if (len < the_snd_size)
		{ fprintf(stdout,"WAV: truncated file err %d %d\n",
							len,the_snd_size);
		  the_snd_size = len;
		}
	      }

	      while(the_snd_size)
	      { xaLONG snd_size = the_snd_size;
	        if (snd_size > max_snd_chunk_size) 
					snd_size = max_snd_chunk_size;
		the_snd_size -= snd_size;
		
	        if (xa_file_flag==xaTRUE)
	        { xaLONG cur_fpos = xin->Get_FPos(xin);
			/* move past this chunk and try to detect truncation */
		  xin->Seek_FPos(xin,snd_size,1); 
                  XA_Add_Sound(anim_hdr,0,wav_audio_type, cur_fpos,
                     auds_hdr.rate, snd_size, &wav_snd_time, &wav_snd_timelo,
		     auds_hdr.blockalign, auds_hdr.samps_block);
		  if (snd_size > wav_max_faud_size) 
					wav_max_faud_size = snd_size;
	        }
	        else
	        { int rets;
		  xaUBYTE *snd = (xaUBYTE *)malloc(snd_size);
		  if (snd==0) TheEnd1("WAV: snd malloc err");
		  ret = xin->Read_Block(xin, snd, snd_size);
		  /* not uncommon to be truncated */
		  if (ret < snd_size)
		  { fprintf(stdout,"WAV: snd rd err %d %d\n",ret,snd_size);
		    snd_size = ret;
		    the_snd_size = 0; /* signal last loop */
		  }
                  rets = XA_Add_Sound(anim_hdr,snd,wav_audio_type, -1,
                     auds_hdr.rate, snd_size, &wav_snd_time, &wav_snd_timelo,
		     auds_hdr.blockalign, auds_hdr.samps_block);
                }
	        auds_hdr.byte_cnt += snd_size; /* for calc total_time */
	      } /* end of while */
	      if (pad_flag) xin->Read_U8(xin);
	    }
	    else
	    {
	      fprintf(stdout,"WAV: Audio Codec (%x)not supported\n",
							   auds_hdr.format);
	      xin->Seek_FPos(xin,ck_size,1);
	    }
	  }
	  break;
        default:
	  if ( !xin->At_EOF(xin,0) ) /* POD TBD better prediction of end */
	  {
            if (wav_riff_size > 0)
            {
                if (ck_size & 0x01) ck_size++; /* pad */
                xin->Seek_FPos(xin,ck_size,1); /* move past this chunk */
                DEBUG_LEVEL1
                {
                  fprintf(stdout,"WAV: unknown  chunk ");
                  AVI_Print_ID(stdout,ck_id);
                  fprintf(stdout,"\n");
                }
            }
            else
            {
                AVI_Print_ID(stdout,ck_id);
                fprintf(stdout,"  chunk unknown\n");
                xin->Seek_FPos(xin,0,2); /* goto end of file */
            }
	  }
	  break;
    } /* end of ck_id switch */
    wav_riff_size -= ck_size;
    if (ck_size & 0x01) wav_riff_size--; /* odd byte pad */
  } /* while not exitflag */

  if (auds_hdr.blockalign == 0) auds_hdr.blockalign = 1;
  if (auds_hdr.rate) 
	anim_hdr->total_time = (xaULONG)
	   ((((float)auds_hdr.byte_cnt * (float)auds_hdr.samps_block * 1000.0) 
					/ (float)auds_hdr.blockalign)
					/ (float)auds_hdr.rate);
  else	anim_hdr->total_time = 0;

  xin->Close_File(xin);
  if (xa_verbose)
  {
    fprintf(stdout,"  Audio Codec: "); AVI_Print_Audio_Type(auds_hdr.format);
    fprintf(stdout," Rate=%d Chans=%d bps=%d\n",
			auds_hdr.rate,auds_hdr.channels,auds_hdr.size);
    fprintf(stdout,"     block_align %d\n",auds_hdr.blockalign);
  }

  anim_hdr->max_faud_size = wav_max_faud_size;
  if (xa_file_flag == xaTRUE) anim_hdr->anim_flags |= ANIM_USE_FILE;
  anim_hdr->fname = anim_hdr->name;
  return(xaTRUE);
} /* end of read file */


