
/*
 * xanim_avi.c
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

/*******************************
 * Revision
 *
 * 16Aug94 +video chunks of 0 size now properly used as NOP's with timing info.
 * 12Apr95 +added RGB depth 24 support
 * 15Apr95 +added XMPG support - what a KLUDGE format.
 *
 ********************************/


#include "xanim_avi.h" 
#include "xanim_xmpg.h" 

LONG Is_AVI_File();
ULONG AVI_Read_File();
void AVI_Print_ID();
AVI_FRAME *AVI_Add_Frame();
void AVI_Free_Frame_List();
ULONG RIFF_Read_AVIH();
ULONG RIFF_Read_STRD();
ULONG RIFF_Read_STRH();
ULONG RIFF_Read_VIDS();
ULONG RIFF_Read_AUDS();
void AVI_Audio_Type();
ULONG AVI_Get_Color();
void AVI_Get_RGBColor();
ACT_DLTA_HDR *AVI_Read_00DC();
void ACT_Setup_Delta();
ULONG AVI_XMPG_00XM();

/* CODEC ROUTINES */
ULONG JFIF_Decode_JPEG();
void JFIF_Read_IJPG_Tables();
ULONG AVI_Decode_RLE8();
ULONG AVI_Decode_CRAM();
ULONG AVI_Decode_CRAM16();
ULONG AVI_Decode_RGB();
ULONG AVI_Decode_RGB24();
extern ULONG QT_Get_Color24();
ULONG AVI_Decode_ULTI();
extern ULONG MPG_Decode_I();
extern void mpg_init_stuff();
extern ULONG QT_Decode_YUV9();
extern void QT_Gen_YUV_Tabs();
extern jpg_alloc_MCU_bufs();
extern void jpg_setup_samp_limit_table();
extern jpg_search_marker();
ULONG AVI_Get_Ulti_Color();
void AVI_Get_Ulti_rgbColor();
void AVI_ULTI_Gen_YUV();
void AVI_ULTI_LTC();
void AVI_Ulti_Gen_LTC();
ULONG AVI_Ulti_Check();

void CMAP_Cache_Clear();
void CMAP_Cache_Init();

XA_ACTION *ACT_Get_Action();
XA_CHDR *ACT_Get_CMAP();
XA_CHDR *CMAP_Create_332();
XA_CHDR *CMAP_Create_422();
XA_CHDR *CMAP_Create_Gray();
void ACT_Add_CHDR_To_Action();
void ACT_Setup_Mapped();
void ACT_Get_CCMAP();
XA_CHDR *CMAP_Create_CHDR_From_True();
ULONG CMAP_Find_Closest();
UBYTE *UTIL_RGB_To_FS_Map();
UBYTE *UTIL_RGB_To_Map();

ULONG UTIL_Get_MSB_Long();
ULONG UTIL_Get_LSB_Long();
ULONG UTIL_Get_LSB_Short();
extern XA_ANIM_SETUP *XA_Get_Anim_Setup();
void XA_Free_Anim_Setup();


/** AVI SOUND STUFF ****/
ULONG avi_audio_attempt;
ULONG avi_audio_type;
ULONG avi_audio_freq;
ULONG avi_audio_chans;
ULONG avi_audio_bps;
ULONG avi_audio_end;
ULONG XA_Add_Sound();
AUDS_HDR auds_hdr;


/* Currently used to check 1st frame for Microsoft MJPG screwup */
ULONG avi_first_delta;

static LONG ulti_Cr[16],ulti_Cb[16],ulti_CrCb[256];
UBYTE *avi_ulti_tab = 0;



AVI_HDR avi_hdr;
AVI_STREAM_HDR strh_hdr;
VIDS_HDR vids_hdr;
ULONG avi_use_index_flag;
UBYTE *avi_strd;
ULONG avi_strd_size,avi_strd_cursz;

#define AVI_MAX_STREAMS 2
ULONG avi_stream[4];
ULONG avi_stream_cnt;

ULONG avi_frame_cnt;
AVI_FRAME *avi_frame_start,*avi_frame_cur;

AVI_FRAME *AVI_Add_Frame(time,timelo,act)
ULONG time,timelo;
XA_ACTION *act;
{
  AVI_FRAME *fframe;
 
  fframe = (AVI_FRAME *) malloc(sizeof(AVI_FRAME));
  if (fframe == 0) TheEnd1("AVI_Add_Frame: malloc err");
 
  fframe->time   = time;
  fframe->timelo = timelo;
  fframe->act = act;
  fframe->next = 0;
 
  if (avi_frame_start == 0) avi_frame_start = fframe;
  else avi_frame_cur->next = fframe;
 
  avi_frame_cur = fframe;
  avi_frame_cnt++;
  return(fframe);
}

void AVI_Free_Frame_List(fframes)
AVI_FRAME *fframes;
{
  AVI_FRAME *ftmp;
  while(fframes != 0)
  {
    ftmp = fframes;
    fframes = fframes->next;
    FREE(ftmp,0xA000);
  }
}

/*
 *
 */
LONG Is_AVI_File(filename)
char *filename;
{
  FILE *fin;
  ULONG data1,len,data3;

  if ( (fin=fopen(filename,XA_OPEN_MODE)) == 0) return(XA_NOFILE);
  data1 = UTIL_Get_MSB_Long(fin);  /* read past size */
  len   = UTIL_Get_MSB_Long(fin);  /* read past size */
  data3 = UTIL_Get_MSB_Long(fin);  /* read past size */
  fclose(fin);
  if ( (data1 == RIFF_RIFF) && (data3 == RIFF_AVI)) return(TRUE);
  return(FALSE);
}


ULONG AVI_Read_File(fname,anim_hdr,audio_attempt)
char *fname;
XA_ANIM_HDR *anim_hdr;
ULONG audio_attempt;	/* TRUE if audio is to be attempted */
{
  FILE *fin;
  LONG i,t_time;
  ULONG t_timelo;
  XA_ACTION *act;
  LONG avi_riff_size;
  XA_ANIM_SETUP *avi;
 
  if ( (fin=fopen(fname,XA_OPEN_MODE)) == 0)
  {
    fprintf(stderr,"can't open AVI File %s for reading\n",fname);
    return(FALSE);
  }

  avi = XA_Get_Anim_Setup();
  avi->vid_time = XA_GET_TIME( 100 ); /* default */

  avi_strd		= 0;
  avi_strd_size		= avi_strd_cursz = 0;
  avi_frame_cnt		= 0;
  avi_frame_start	= 0;
  avi_frame_cur		= 0;
  avi_use_index_flag	= 0;
  avi_audio_attempt	= audio_attempt;
  avi_riff_size		= 0;
  avi_first_delta	= 0;
  avi_stream_cnt	= 0;
  for(i=0; i < AVI_MAX_STREAMS; i++) avi_stream[i] = 0;

  while( !feof(fin) )
  {
    ULONG d,ck_id,ck_size;

    ck_id = UTIL_Get_MSB_Long(fin);
    ck_size = UTIL_Get_LSB_Long(fin);
    avi_riff_size -= 8;
DEBUG_LEVEL2 
{
  fprintf(stderr,"AVI cid ");
  AVI_Print_ID(stderr,ck_id);
  fprintf(stderr,"  cksize %08lx\n",ck_size);
}
    switch(ck_id)
    {
	case RIFF_RIFF:
		d = UTIL_Get_MSB_Long(fin);
		avi_riff_size = (2*ck_size) - 4;
		DEBUG_LEVEL2 
		{
			fprintf(stderr,"  RIFF form type ");
			AVI_Print_ID(stderr,d);
			fprintf(stderr,"\n");
		}
                break;
	case RIFF_LIST:
		d = UTIL_Get_MSB_Long(fin);
		avi_riff_size += (ck_size - 4); /* don't count LISTs */ 
		DEBUG_LEVEL2 
		{
			fprintf(stderr,"  List type ");
			AVI_Print_ID(stderr,d);
			fprintf(stderr,"\n");
		}
		break;
 
	case RIFF_avih:
		DEBUG_LEVEL2 fprintf(stderr,"  AVI_HDR:\n");
                if (RIFF_Read_AVIH(avi,fin,ck_size,&avi_hdr)==FALSE) return(FALSE);
                break;
 
	case RIFF_strh:
		DEBUG_LEVEL2 fprintf(stderr,"  STRH HDR:\n");
                if (RIFF_Read_STRH(fin,ck_size,&strh_hdr)==FALSE) return(FALSE);
                break;

	case RIFF_strd:
		DEBUG_LEVEL2 fprintf(stderr,"  STRD HDR:\n");
		avi_strd_cursz = ck_size;
		if (ck_size & 1) ck_size++;
		if (avi_strd_size==0)
		{ avi_strd_size = ck_size;
		  avi_strd = (UBYTE *)malloc(ck_size);
		  if (avi_strd==0) TheEnd1("AVI: strd malloc err");
		}
	  	else if (ck_size > avi_strd_size)
		{ UBYTE *tmp;
		  avi_strd_size = ck_size;
		  tmp = (UBYTE *)realloc(avi_strd,ck_size);
		  if (tmp==0) TheEnd1("AVI: strd malloc err");
		  else avi_strd = tmp;
		}
		fread((char *)(avi_strd),ck_size,1,fin);
		break;
 
	case RIFF_strf:
	  DEBUG_LEVEL2 fprintf(stderr,"  STRF HDR:\n");
	  if  (    (avi_stream_cnt < AVI_MAX_STREAMS)
		&& (avi_stream_cnt < avi_hdr.streams) )
	  { ULONG str_type;
	    str_type = avi_stream[avi_stream_cnt] = strh_hdr.fcc_type;
	    avi_stream_cnt++;
	    if (str_type == RIFF_vids)
	    {  if (RIFF_Read_VIDS(avi,fin,ck_size,&vids_hdr)==FALSE)
								return(FALSE);
		break;
	    }
	    else if (str_type == RIFF_auds)
	    { ULONG ret = RIFF_Read_AUDS(fin,ck_size,&auds_hdr);
	      if (avi_audio_attempt==TRUE)
	      {
		if (ret==FALSE)
		{
		  fprintf(stderr,"  AVI Audio Type Unsupported\n");
		  avi_audio_attempt = FALSE;
		}
		else avi_audio_attempt = TRUE;
	      }
	      break;
	    }
	    else if (str_type == RIFF_auds)
	    {
	      if (xa_verbose) fprintf(stderr,"AVI: STRH(pads) ignored\n");
	      /* NOTE: fall through */
	    }
	    else
	    {
	      fprintf(stderr,"unknown fcc_type at strf %08lx - ignoring this stream.\n",strh_hdr.fcc_type);
	      /* NOTE: fall through */
	    }
	  }
	  /* ignore STRF chunk on higher streams or unsupport fcc_types */
	  if (ck_size & 0x01) ck_size++;
	  fseek(fin,ck_size,1); /* move past this chunk */
	  break;
 

	case RIFF_01pc:
	case RIFF_00pc:
		{ ULONG pc_firstcol,pc_numcols;
		  pc_firstcol = getc(fin);
		  pc_numcols  = getc(fin);
DEBUG_LEVEL2 fprintf(stderr,"00PC: 1st %ld num %ld\n",pc_firstcol,pc_numcols);
		  d = getc(fin);
		  d = getc(fin);
		  for(i = 0; i < pc_numcols; i++) 
		  {
		    avi->cmap[i + pc_firstcol].red   = (getc(fin) & 0xff);
		    avi->cmap[i + pc_firstcol].green = (getc(fin) & 0xff);
		    avi->cmap[i + pc_firstcol].blue  = (getc(fin) & 0xff);
		    d = getc(fin);
		  }
		  act = ACT_Get_Action(anim_hdr,0);
		  AVI_Add_Frame(avi->vid_time,avi->vid_timelo,act);
		  avi->chdr = ACT_Get_CMAP(avi->cmap,avi->imagec,0,
							avi->imagec,0,8,8,8);
		  ACT_Add_CHDR_To_Action(act,avi->chdr);
		}
		break;


 
        case RIFF_idx1:
        case RIFF_vedt:
        case RIFF_strl:
        case RIFF_hdrl:
        case RIFF_vids:
        case RIFF_JUNK:
	case RIFF_DISP:
	case RIFF_ISBJ:
/*
	case RIFF_ISFT:
	case RIFF_IDIT:
*/
	case RIFF_00AM:
                if (ck_size & 0x01) ck_size++;
		fseek(fin,ck_size,1); /* move past this chunk */
                break;
 
/* NOW Look for RIFF_00 and RIFF_01 in the default case for Video/Audio 
case RIFF_00dc:
case RIFF_00id:
case RIFF_00rt:
case RIFF_0021:
case RIFF_00iv:
case RIFF_0031:
case RIFF_0032:
case RIFF_00db:
case RIFF_00vc:
case RIFF_00dx:
case RIFF_00xx:
case RIFF_00xm:
case RIFF_01wb:
*/
        default:
	  if ( !feof(fin) )
	  {
	    if (avi_riff_size > 0)
	    { ULONG stream_id = ck_id & RIFF_FF00;
	      ULONG stream_type = 0;
	      if (stream_id == RIFF_00) stream_type = avi_stream[0];
	      else if (stream_id == RIFF_01) stream_type = avi_stream[1];
	      if (stream_type == RIFF_vids)  /* VIDEO Delta CHUNK */
	      { ACT_DLTA_HDR *dlta_hdr;
		DEBUG_LEVEL2  /* POD NOTE: 00dc not true */
		   fprintf(stderr,"00dc video attemp %lx\n",avi_audio_attempt);
		act = ACT_Get_Action(anim_hdr,ACT_DELTA);
		if (avi->compression == RIFF_XMPG)
		{ ULONG ret;
		  ret = AVI_XMPG_00XM(avi,fin,ck_size,act,anim_hdr);
		  if (ret==FALSE) return(FALSE);
		  MPG_Setup_Delta(avi,fname,act);
		}
		else
		{
		  dlta_hdr = AVI_Read_00DC(avi,fin,ck_size,act);
		  if (act->type == ACT_NOP) break;
		  if (dlta_hdr == 0)        return(FALSE);
		  ACT_Setup_Delta(avi,act,dlta_hdr,fin);
		}
		break;
	      } /* end vids */
	      else if (stream_type == RIFF_auds) /* AUDIO Delta CHUNK */
	      { ULONG snd_size = ck_size;
		if (ck_size & 0x01) ck_size++;
		if (ck_size == 0) break;
		DEBUG_LEVEL2  /* POD NOTE: 01wb not true */
		   fprintf(stderr,"01wb audio attemp %lx\n",avi_audio_attempt);
		if (avi_audio_attempt==TRUE)
		{ LONG ret;
		  if (xa_file_flag==TRUE)
		  { LONG rets, cur_fpos = ftell(fin);
		    rets = XA_Add_Sound(anim_hdr,0,avi_audio_type, cur_fpos,
		    avi_audio_freq, snd_size, &avi->aud_time,&avi->aud_timelo);
		    if (rets==FALSE) avi_audio_attempt = FALSE;
		    fseek(fin,ck_size,1); /* move past this chunk */
		    if (snd_size > avi->max_faud_size) 
					avi->max_faud_size = snd_size;

		  }
		  else
		  { UBYTE *snd = (UBYTE *)malloc(ck_size);
		    if (snd==0) TheEnd1("AVI: snd malloc err");
		    ret = fread( snd, ck_size, 1, fin);
		    if (ret != 1) fprintf(stderr,"AVI: snd rd err\n");
		    else
		    { int rets;
		      rets = XA_Add_Sound(anim_hdr,snd,avi_audio_type, -1,
					avi_audio_freq, snd_size, 
					&avi->aud_time, &avi->aud_timelo);
		      if (rets==FALSE) avi_audio_attempt = FALSE;
		    }
		  }
		}
		else fseek(fin,ck_size,1);
		break; /* break out - spotlights, sirens, rifles - but he... */
	      } /* end auds */
	      else /* Ignore */
	      {
		if (ck_size & 0x01) ck_size++;
		fseek(fin,ck_size,1); /* move past this chunk */
		DEBUG_LEVEL1 
		{
		  fprintf(stderr,"AVI: unknown chunk ");
		  AVI_Print_ID(stderr,ck_id);
		  fprintf(stderr,"\n");
		}
	      }
	    }
	    else /* past end of RIFF Chunk */
	    {
		fprintf(stderr,"  Past End of AVI File\n");
		fseek(fin,0,2); /* goto end of file */
	    }
	  }
	  break;

      } /* end of ck_id switch */
      /* reduce pessimism */
      avi_riff_size -= ck_size;
      if (ck_size & 0x01) avi_riff_size--; /* odd byte pad */
    } /* while not exitflag */

  if (avi->pic != 0) { FREE(avi->pic,0xA003); avi->pic=0; }
  fclose(fin);

  if (xa_verbose) 
  {
    fprintf(stderr,"AVI %ldx%ldx%ld frames %ld codec ",
			avi->imagex,avi->imagey,avi->imagec,avi_frame_cnt);
    AVI_Print_ID(stderr,avi->compression);
    fprintf(stderr," depth=%ld\n",avi->depth);
  }
  if (avi_frame_cnt == 0)
  { 
    fprintf(stderr,"AVI: No supported video frames exist in this file.\n");
    return(FALSE);
  }

  anim_hdr->frame_lst = (XA_FRAME *)
				malloc( sizeof(XA_FRAME) * (avi_frame_cnt+1));
  if (anim_hdr->frame_lst == NULL) TheEnd1("AVI_Read_File: frame malloc err");

  avi_frame_cur = avi_frame_start;
  i = 0;
  t_time = 0;
  t_timelo = 0;
  while(avi_frame_cur != 0)
  {
    if (i > avi_frame_cnt)
    {
      fprintf(stderr,"AVI_Read_Anim: frame inconsistency %ld %ld\n",
                i,avi_frame_cnt);
      break;
    }
    anim_hdr->frame_lst[i].time_dur = avi_frame_cur->time;
    anim_hdr->frame_lst[i].zztime = t_time;
    t_time += avi_frame_cur->time;
    t_timelo += avi_frame_cur->timelo;
    while(t_timelo > (1<<24)) {t_time++; t_timelo -= (1<<24);}
    anim_hdr->frame_lst[i].act = avi_frame_cur->act;
    avi_frame_cur = avi_frame_cur->next;
    i++;
  }
  anim_hdr->imagex = avi->imagex;
  anim_hdr->imagey = avi->imagey;
  anim_hdr->imagec = avi->imagec;
  anim_hdr->imaged = 8; /* nop */
  anim_hdr->frame_lst[i].time_dur = 0;
  anim_hdr->frame_lst[i].zztime = -1;
  anim_hdr->frame_lst[i].act  = 0;
  anim_hdr->loop_frame = 0;
  if (xa_buffer_flag == FALSE) anim_hdr->anim_flags |= ANIM_SNG_BUF;
  if (xa_file_flag == TRUE) anim_hdr->anim_flags |= ANIM_USE_FILE;
  if (   (avi->compression == RIFF_JPEG) 
      || (avi->compression == RIFF_MJPG) 
      || (avi->compression == RIFF_IJPG) 
      || (avi->compression == RIFF_RGB) 
     ) anim_hdr->anim_flags |= ANIM_FULL_IM;
  else anim_hdr->anim_flags &=  ~ANIM_FULL_IM;
  anim_hdr->max_fvid_size = avi->max_fvid_size;
  anim_hdr->max_faud_size = avi->max_faud_size;
  anim_hdr->fname = anim_hdr->name;
  if (i > 0) 
  {
    anim_hdr->last_frame = i - 1;
    anim_hdr->total_time = anim_hdr->frame_lst[i-1].zztime
				+ anim_hdr->frame_lst[i-1].time_dur;
  }
  else
  {
    anim_hdr->last_frame = 0;
    anim_hdr->total_time = 0;
  }
  AVI_Free_Frame_List(avi_frame_start);
  XA_Free_Anim_Setup(avi);
  return(TRUE);
} /* end of read file */

ULONG RIFF_Read_AVIH(avi,fin,size,avi_hdr)
XA_ANIM_SETUP *avi;
FILE *fin;
ULONG size;
AVI_HDR *avi_hdr;
{
  if (size != 0x38)
  {
    fprintf(stderr,"avih: size not 56 size=%ld\n",size);
    return(FALSE);
  }
 
  avi_hdr->us_frame     = UTIL_Get_LSB_Long(fin);
  avi_hdr->max_bps      = UTIL_Get_LSB_Long(fin);
  avi_hdr->pad_gran     = UTIL_Get_LSB_Long(fin);
  avi_hdr->flags        = UTIL_Get_LSB_Long(fin);
  avi_hdr->tot_frames   = UTIL_Get_LSB_Long(fin);
  avi_hdr->init_frames  = UTIL_Get_LSB_Long(fin);
  avi_hdr->streams      = UTIL_Get_LSB_Long(fin);
  avi_hdr->sug_bsize    = UTIL_Get_LSB_Long(fin);
  avi_hdr->width        = UTIL_Get_LSB_Long(fin);
  avi_hdr->height       = UTIL_Get_LSB_Long(fin);
  avi_hdr->scale        = UTIL_Get_LSB_Long(fin);
  avi_hdr->rate         = UTIL_Get_LSB_Long(fin);
  avi_hdr->start        = UTIL_Get_LSB_Long(fin);
  avi_hdr->length       = UTIL_Get_LSB_Long(fin);

  if (avi_hdr->streams > 2)
  {
    fprintf(stderr,"AVI: This file has %ld streams. XAnim currently only supports the\n",avi_hdr->streams);
    fprintf(stderr,"     first two of them. Contact Author.\n");
  }

  avi->cmap_frame_num = avi_hdr->tot_frames / cmap_sample_cnt;
  if (xa_jiffy_flag) { avi->vid_time = xa_jiffy_flag; avi->vid_timelo = 0; }
  else
  { double ftime = (double)((avi_hdr->us_frame)/1000.0); /* convert to ms */
    avi->vid_time =  (ULONG)(ftime);
    ftime -= (double)(avi->vid_time);
    avi->vid_timelo = (ftime * (double)(1<<24));
  }
  if (avi_hdr->flags & AVIF_MUSTUSEINDEX) avi_use_index_flag = 1;
  else avi_use_index_flag = 0;

  /* if (xa_verbose) */
  DEBUG_LEVEL1
  {
    fprintf(stderr,"  AVI flags: ");
    if (avi_hdr->flags & AVIF_HASINDEX) fprintf(stderr,"Has_Index ");
    if (avi_hdr->flags & AVIF_MUSTUSEINDEX) fprintf(stderr,"Use_Index ");
    if (avi_hdr->flags & AVIF_ISINTERLEAVED) fprintf(stderr,"Interleaved ");
    if (avi_hdr->flags & AVIF_WASCAPTUREFILE) fprintf(stderr,"Captured ");
    if (avi_hdr->flags & AVIF_COPYRIGHTED) fprintf(stderr,"Copyrighted ");
    fprintf(stderr,"\n");
  }
  return(TRUE);
}

ULONG RIFF_Read_STRH(fin,size,strh_hdr)
FILE *fin;
ULONG size;
AVI_STREAM_HDR *strh_hdr;
{
  ULONG d,tsize;
 
  if (size < 0x24) 
	{fprintf(stderr,"strh: size < 36 size = %ld\n",size); return(FALSE);}
 
  strh_hdr->fcc_type    = UTIL_Get_MSB_Long(fin);
  strh_hdr->fcc_handler = UTIL_Get_MSB_Long(fin);
  strh_hdr->flags       = UTIL_Get_LSB_Long(fin);
  strh_hdr->priority    = UTIL_Get_LSB_Long(fin);
  strh_hdr->init_frames = UTIL_Get_LSB_Long(fin);
  strh_hdr->scale       = UTIL_Get_LSB_Long(fin);
  strh_hdr->rate        = UTIL_Get_LSB_Long(fin);
  strh_hdr->start       = UTIL_Get_LSB_Long(fin);
  strh_hdr->length      = UTIL_Get_LSB_Long(fin);
  strh_hdr->sug_bsize   = UTIL_Get_LSB_Long(fin);
  strh_hdr->quality     = UTIL_Get_LSB_Long(fin);
  strh_hdr->samp_size   = UTIL_Get_LSB_Long(fin);
 
  tsize = 48; if (size & 0x01) size++;
  while(tsize < size) { d = getc(fin); tsize++; }
 
  DEBUG_LEVEL2 fprintf(stderr,"AVI TEST handler = 0x%08lx\n",
						strh_hdr->fcc_handler);
  return(TRUE);
}

ULONG RIFF_Read_VIDS(avi,fin,size,vids_hdr)
XA_ANIM_SETUP *avi;
FILE *fin;
LONG size;
VIDS_HDR *vids_hdr;
{
  ULONG d,i,ctable_flag;
 
  if (size & 0x01) size++;
  ctable_flag		= TRUE;
  vids_hdr->size        = UTIL_Get_LSB_Long(fin);
  vids_hdr->width       = UTIL_Get_LSB_Long(fin);
  vids_hdr->height      = UTIL_Get_LSB_Long(fin);
  vids_hdr->planes      = UTIL_Get_LSB_Short(fin);
  vids_hdr->bit_cnt     = UTIL_Get_LSB_Short(fin);
  vids_hdr->compression = UTIL_Get_MSB_Long(fin);
  vids_hdr->image_size  = UTIL_Get_LSB_Long(fin);
  vids_hdr->xpels_meter = UTIL_Get_LSB_Long(fin);
  vids_hdr->ypels_meter = UTIL_Get_LSB_Long(fin);
  vids_hdr->num_colors  = UTIL_Get_LSB_Long(fin);
  vids_hdr->imp_colors  = UTIL_Get_LSB_Long(fin);
  size -= 40;

  avi->compression = vids_hdr->compression;
DEBUG_LEVEL2 fprintf(stderr,"VIDS compression = %08lx\n",avi->compression);
  avi->depth = vids_hdr->bit_cnt;
  avi->imagex = vids_hdr->width;
  avi->imagey = vids_hdr->height;
  avi->imagec = vids_hdr->num_colors;
  if ( (avi->imagec==0) && (avi->depth <= 8) ) avi->imagec = (1 << avi->depth);
  vids_hdr->num_colors = avi->imagec; /* re-update struct */

  switch(avi->compression)
  {
    case RIFF_rgb:  avi->compression = RIFF_RGB;  break;
    case RIFF_rle8: avi->compression = RIFF_RLE8; break;
    case RIFF_rle4: avi->compression = RIFF_RLE4; break;
    case RIFF_none: avi->compression = RIFF_NONE; break;
    case RIFF_pack: avi->compression = RIFF_PACK; break;
    case RIFF_tran: avi->compression = RIFF_TRAN; break;
    case RIFF_ccc : avi->compression = RIFF_CCC;  break;
    case RIFF_IJPG: 
	avi->imagex = 4 * ((avi->imagex + 3)/4);
	avi->imagey = 2 * ((avi->imagey + 1)/2);
	if (avi->depth > 8)	QT_Gen_YUV_Tabs();
	else	ctable_flag = FALSE;
	jpg_alloc_MCU_bufs(avi->imagex);
	break;
    case RIFF_MJPG:
	avi->imagex = 4 * ((avi->imagex + 3)/4);
	avi->imagey = 2 * ((avi->imagey + 1)/2);
	if (avi->depth > 8)	QT_Gen_YUV_Tabs();
	else	ctable_flag = FALSE;
	jpg_alloc_MCU_bufs(avi->imagex);
	break;
    case RIFF_XMPG:
    case RIFF_xmpg:
	avi->imagex = 4 * ((avi->imagex + 3)/4);
	avi->imagey = 4 * ((avi->imagey + 3)/4);
	avi->compression = RIFF_XMPG; 
	QT_Gen_YUV_Tabs();
	jpg_alloc_MCU_bufs(avi->imagex);
	jpg_setup_samp_limit_table();
	mpg_init_stuff();
	break;
    case RIFF_JPEG: 
    case RIFF_jpeg: 
	avi->imagex = 4 * ((avi->imagex + 3)/4);
	avi->imagey = 2 * ((avi->imagey + 1)/2);
	avi->compression = RIFF_JPEG; 
	if (avi->depth > 8)	QT_Gen_YUV_Tabs();
	else	ctable_flag = FALSE;
	jpg_alloc_MCU_bufs(avi->imagex);
	break;
    case RIFF_YUV9:
    case RIFF_YVU9:
	QT_Gen_YUV_Tabs();
	jpg_setup_samp_limit_table();
	avi->imagex = 4 * ((avi->imagex + 3)/4);
	avi->imagey = 4 * ((avi->imagey + 3)/4);
	break;
    case RIFF_rt21: avi->compression = RIFF_RT21;  break;
    case RIFF_iv31: avi->compression = RIFF_IV31;  break;
    case RIFF_iv32: avi->compression = RIFF_IV32;  break;
    case RIFF_CVID:
	break;
    case RIFF_MSVC: /* need to be multiple of 4 */
    case RIFF_CRAM: /* need to be multiple of 4 */
	avi->imagex = 4 * ((avi->imagex + 3)/4);
	avi->imagey = 4 * ((avi->imagey + 3)/4);
	break;
    case RIFF_ULTI: /* need to be multiple of 8 */
	avi->imagex = 8 * ((avi->imagex + 7)/8);
	avi->imagey = 8 * ((avi->imagey + 7)/8);
	AVI_ULTI_Gen_YUV();/* generate tables */
	AVI_Ulti_Gen_LTC();
	break;

  }

  if ( (avi->depth <= 8) && (ctable_flag==TRUE) )
  {
    DEBUG_LEVEL1 fprintf(stderr,"AVI reading cmap %ld\n",avi->imagec);
    for(i=0; i < avi->imagec; i++)
    {
      avi->cmap[i].blue  =  ( getc(fin) ) & 0xff;
      avi->cmap[i].green =  ( getc(fin) ) & 0xff;
      avi->cmap[i].red   =  ( getc(fin) ) & 0xff;
      d = getc(fin); /* pad */
      size -= 4; if (size <= 0) break;
    }
    avi->chdr = ACT_Get_CMAP(avi->cmap,avi->imagec,0,avi->imagec,0,8,8,8);
  }
  else if (   (cmap_true_map_flag == FALSE) /* depth 16 and not true_map */
           || (xa_buffer_flag == FALSE) )
  {
     if (cmap_true_to_332 == TRUE)
             avi->chdr = CMAP_Create_332(avi->cmap,&avi->imagec);
     else    avi->chdr = CMAP_Create_Gray(avi->cmap,&avi->imagec);
  }
  if ( (avi->pic==0) && (xa_buffer_flag == TRUE))
  {
    avi->pic_size = avi->imagex * avi->imagey;
    if ( (cmap_true_map_flag == TRUE) && (avi->depth > 8) )
		avi->pic = (UBYTE *) malloc( 3 * avi->pic_size );
    else avi->pic = (UBYTE *) malloc( XA_PIC_SIZE(avi->pic_size) );
    if (avi->pic == 0) TheEnd1("AVI_Buffer_Action: malloc failed");
  }

  /* Read rest of header */
  if (avi->compression == RIFF_IJPG)
  { ULONG offset, jsize, format, cspace;
    fprintf(stderr,"IJPG: size = %lx\n",size); 
    offset = UTIL_Get_LSB_Long(fin);
    jsize  = UTIL_Get_LSB_Long(fin);
    format = UTIL_Get_LSB_Long(fin);
    cspace = UTIL_Get_LSB_Long(fin);
    DEBUG_LEVEL1 fprintf(stderr,"IJPG: offset %lx jsize %lx format %lx cspace %lx\n", offset,jsize,format,cspace);
    size -= 16;
    JFIF_Read_IJPG_Tables(fin);
    size -= 128;
  }
  else if ((avi->compression == RIFF_JPEG) || (avi->compression == RIFF_MJPG))
  { ULONG offset, jsize, format, cspace, bits, hsubsamp,vsubsamp;
    offset = UTIL_Get_LSB_Long(fin);
    jsize  = UTIL_Get_LSB_Long(fin);
    format = UTIL_Get_LSB_Long(fin);
    cspace = UTIL_Get_LSB_Long(fin);
    bits   = UTIL_Get_LSB_Long(fin);
    hsubsamp = UTIL_Get_LSB_Long(fin);
    vsubsamp = UTIL_Get_LSB_Long(fin);
DEBUG_LEVEL1 fprintf(stderr,"M/JPG: offset %lx jsize %lx format %lx cspace %lx bits %lx hsub %lx vsub %lx\n", offset,jsize,format,cspace,bits,hsubsamp,vsubsamp);
    size -= 28;
  }
  while(size > 0) { d = getc(fin); size--; }
  return(TRUE);
}

	/* by not setting act->type to NOP, yet returning 0, indicates
         * that compression is not supported and that this AVI
	 * file should be no read in. Currently this is fine since
	 * only one compression is supported per file.
	 */

/****************************
 * This function return in three ways.
 *
 *  act->type = NOP        : NOP frame.
 *  dlta_hdr = 0           : comprssion not supported.
 *  dlta_hdr != 0	   : valid frame
 *
 *  NOTE: act->type comes in as ACT_DELTA
 *
 */
ACT_DLTA_HDR *AVI_Read_00DC(avi,fin,ck_size,act)
XA_ANIM_SETUP *avi;
FILE *fin;
LONG ck_size;
XA_ACTION *act;
{
  ACT_DLTA_HDR *dlta_hdr = 0;
  ULONG d;

  if (ck_size & 0x01) ck_size++;
  if (ck_size == 0)  /* NOP wait frame */
  {
    act->type = ACT_NOP;
    act->data = 0;
    act->chdr = 0;
    AVI_Add_Frame( avi->vid_time, avi->vid_timelo, act);
    return(0);
  }

  if (xa_file_flag==TRUE)
  {
    dlta_hdr = (ACT_DLTA_HDR *) malloc(sizeof(ACT_DLTA_HDR));
    if (dlta_hdr == 0) TheEnd1("AVI vid dlta0: malloc failed");
    act->data = (UBYTE *)dlta_hdr;
    dlta_hdr->flags = ACT_SNGL_BUF;
    dlta_hdr->fpos  = ftell(fin);
    dlta_hdr->fsize = ck_size;
    fseek(fin,ck_size,1); /* move past this chunk */
    if (ck_size > avi->max_fvid_size) avi->max_fvid_size = ck_size;
  }
  else
  { LONG ret;
    d = ck_size + (sizeof(ACT_DLTA_HDR));
    dlta_hdr = (ACT_DLTA_HDR *) malloc( d );
    if (dlta_hdr == 0) TheEnd1("AVI vid dlta1: malloc failed");
    act->data = (UBYTE *)dlta_hdr;
    dlta_hdr->flags = ACT_SNGL_BUF | DLTA_DATA;
    dlta_hdr->fpos = 0; dlta_hdr->fsize = ck_size;
    ret = fread( dlta_hdr->data, ck_size, 1, fin);
    if (ret != 1) {fprintf(stderr,"AVI vid dlta: read failed\n"); return(0);}
  }

  AVI_Add_Frame( avi->vid_time, avi->vid_timelo, act);
  dlta_hdr->xpos = dlta_hdr->ypos = 0;
  dlta_hdr->xsize = avi->imagex;
  dlta_hdr->ysize = avi->imagey;
  dlta_hdr->special = 0;
  dlta_hdr->extra = (void *)(0);
  switch(avi->compression)
  {
    case RIFF_RLE8:
	if (avi->depth == 8) dlta_hdr->delta = AVI_Decode_RLE8;
	else goto AVI_UNSUPPORTED;
	break;
    case RIFF_MSVC:
    case RIFF_CRAM:
	if (avi->depth == 8) dlta_hdr->delta = AVI_Decode_CRAM;
	else if (avi->depth ==16) dlta_hdr->delta = AVI_Decode_CRAM16;
	else goto AVI_UNSUPPORTED;
	break;
    case RIFF_ULTI:
	if (avi->depth == 16) dlta_hdr->delta = AVI_Decode_ULTI;
	else goto AVI_UNSUPPORTED;
	break;
    case RIFF_RGB:
	if (avi->depth == 8) dlta_hdr->delta = AVI_Decode_RGB;
	else if (avi->depth == 24) dlta_hdr->delta = AVI_Decode_RGB24;
	else goto AVI_UNSUPPORTED;
	break;
    case RIFF_YVU9:
    case RIFF_YUV9:
	fprintf(stderr,"NOT FULLY SUPPORTED YET depth %ld\n",avi->depth);
	dlta_hdr->delta = QT_Decode_YUV9;
/*
	if (avi->depth == 24) dlta_hdr->delta = QT_Decode_YUV9;
	else goto AVI_UNSUPPORTED;
*/
	break;
    case RIFF_XMPG:
	dlta_hdr->delta = MPG_Decode_I;
	break;
    case RIFF_CVID:
        fprintf(stderr,"AVI: Radius Cinepak support temporarily removed - sorry.\n");
	goto AVI_UNSUPPORTED;
	break;
    case RIFF_MJPG:
	if ( (avi->depth == 24) || (avi->depth == 8) )
        {
	  dlta_hdr->delta = JFIF_Decode_JPEG;
	  if (avi_first_delta==0)
	  { UBYTE *mjpg_tmp,*mjpg_buff = dlta_hdr->data;
	    LONG  mjpg_size  = ck_size;
	    avi_first_delta = 1;
	    if (xa_file_flag==TRUE) /* read from file if necessary */
	    { ULONG ret;
	      mjpg_tmp = mjpg_buff = (UBYTE *)malloc(ck_size);
	      fseek(fin,dlta_hdr->fpos,0);
	      ret = fread(mjpg_tmp, mjpg_size, 1, fin);
	    }
	    while(jpg_search_marker(0xdb,&mjpg_buff,&mjpg_size) == TRUE)
	    { ULONG len = (*mjpg_buff++) << 8;  len |= (*mjpg_buff++);
	      if ((len == 0x41) || (len == 0x82)) /* Microsoft MJPG screw up */
	      { 
		dlta_hdr->extra = (void *)(0x40); break;
	      }
	      else if ((len == 0x43) || (len == 0x84)) /* valid lengths */
	      { 
		dlta_hdr->extra = (void *)(0x00); break;
	      }
	      else mjpg_buff -= 2;  /* else back up */
	    }
	  } /* end of 1st delta */
	}
	else goto AVI_UNSUPPORTED;
	break;
    case RIFF_JPEG:
	if ((avi->depth == 24) || (avi->depth == 8)) 
				dlta_hdr->delta = JFIF_Decode_JPEG;
	else goto AVI_UNSUPPORTED;
	break;
    case RIFF_IJPG:
	if (avi->depth == 24) 
	{ /* POD NOTE: create JPEG structure for extra */
	  dlta_hdr->extra = (void *)(0x10); /* to indicate Q tables present */
	  dlta_hdr->delta = JFIF_Decode_JPEG;
	}
	else goto AVI_UNSUPPORTED;
	break;
    case RIFF_RT21:
    case RIFF_IV31:
    case RIFF_IV32:
	fprintf(stderr,"AVI: Intel Indeo Video Codec Not Supported\n");
	goto AVI_UNSUPPORTED;
	break;
    default:
	AVI_UNSUPPORTED:
	fprintf(stderr,"AVI: unsupported comp ");
	AVI_Print_ID(stderr,avi->compression);
	fprintf(stderr," with depth %ld\n",avi->depth);
	return(0);
	break;
  }
  return(dlta_hdr);
}


/*
 * Routine to Decode an AVI CRAM chunk
 */

#define AVI_CRAM_C1(ip,clr,rdec) { \
 *ip++ = clr; *ip++ = clr; *ip++ = clr; *ip = clr; ip -= rdec; \
 *ip++ = clr; *ip++ = clr; *ip++ = clr; *ip = clr; ip -= rdec; \
 *ip++ = clr; *ip++ = clr; *ip++ = clr; *ip = clr; ip -= rdec; \
 *ip++ = clr; *ip++ = clr; *ip++ = clr; *ip = clr; }

#define AVI_CRAM_C2(ip,flag,cA,cB,rdec) { \
  *ip++ =(flag&0x01)?(cB):(cA); *ip++ =(flag&0x02)?(cB):(cA); \
  *ip++ =(flag&0x04)?(cB):(cA); *ip   =(flag&0x08)?(cB):(cA); ip-=rdec; \
  *ip++ =(flag&0x10)?(cB):(cA); *ip++ =(flag&0x20)?(cB):(cA); \
  *ip++ =(flag&0x40)?(cB):(cA); *ip   =(flag&0x80)?(cB):(cA); }

#define AVI_CRAM_C4(ip,flag,cA0,cA1,cB0,cB1,rdec) { \
  *ip++ =(flag&0x01)?(cB0):(cA0); *ip++ =(flag&0x02)?(cB0):(cA0); \
  *ip++ =(flag&0x04)?(cB1):(cA1); *ip   =(flag&0x08)?(cB1):(cA1); ip-=rdec; \
  *ip++ =(flag&0x10)?(cB0):(cA0); *ip++ =(flag&0x20)?(cB0):(cA0); \
  *ip++ =(flag&0x40)?(cB1):(cA1); *ip   =(flag&0x80)?(cB1):(cA1); }

#define AVI_MIN_MAX_CHECK(x,y,min_x,max_x,min_y,max_y) { \
    if (x < min_x) min_x = x; if (y > max_y) max_y = y; \
    if (x > max_x) max_x = x; if (y < min_y) min_y = y; } 

#define AVI_BLOCK_INC(x,y,imagex) { x += 4; if (x>=imagex) { x=0; y -= 4; } }

#define AVI_GET_16(data,dptr) { data = *dptr++; data |= (*dptr++) << 8; }

#define AVI_CRAM_rgbC1(ip,r,g,b) { \
 *ip++=r; *ip++=g; *ip++=b; *ip++=r; *ip++=g; *ip++=b; \
 *ip++=r; *ip++=g; *ip++=b; *ip++=r; *ip++=g; *ip  =b; }

#define AVI_CRAM_rgbC2(ip,flag,rA,gA,bA,rB,gB,bB) { \
  if (flag&0x01) {*ip++=rB; *ip++=gB; *ip++=bB;} \
  else		 {*ip++=rA; *ip++=gA; *ip++=bA;} \
  if (flag&0x02) {*ip++=rB; *ip++=gB; *ip++=bB;} \
  else		 {*ip++=rA; *ip++=gA; *ip++=bA;} \
  if (flag&0x04) {*ip++=rB; *ip++=gB; *ip++=bB;} \
  else		 {*ip++=rA; *ip++=gA; *ip++=bA;} \
  if (flag&0x08) {*ip++=rB; *ip++=gB; *ip  =bB;} \
  else		 {*ip++=rA; *ip++=gA; *ip  =bA;}  }

#define AVI_CRAM_rgbC4(ip,flag,rA,gA,bA,rB,gB,bB) { \
  if (flag&0x01) {*ip++=rB; *ip++=gB; *ip++=bB;} \
  else		 {*ip++=rA; *ip++=gA; *ip++=bA;} \
  if (flag&0x02) {*ip++=rB; *ip++=gB; *ip  =bB;} \
  else		 {*ip++=rA; *ip++=gA; *ip  =bA;} }

#define AVI_Get_RGBColor(r,g,b,color) \
{ register ULONG _r,_g,_b; \
  _r = (color >> 10) & 0x1f; r = (_r << 3) | (_r >> 2); \
  _g = (color >>  5) & 0x1f; g = (_g << 3) | (_g >> 2); \
  _b =  color & 0x1f;        b = (_b << 3) | (_b >> 2); \
  if (xa_gamma_flag==TRUE) { r = xa_gamma_adj[r]>>8;    \
     g = xa_gamma_adj[g]>>8; b = xa_gamma_adj[b]>>8; } }


ULONG
AVI_Decode_CRAM(image,delta,dsize,chdr,map,map_flag,imagex,imagey,imaged,
						xs,ys,xe,ye,special,extra)
UBYTE *image;		/* Image Buffer. */
UBYTE *delta;		/* delta data. */
ULONG dsize;		/* delta size */
XA_CHDR *chdr;		/* color map info */
ULONG *map;		/* used if it's going to be remapped. */
ULONG map_flag;		/* whether or not to use remap_map info. */
ULONG imagex,imagey;	/* Size of image buffer. */
ULONG imaged;		/* Depth of Image. (IFF specific) */
ULONG *xs,*ys;		/* pos of changed area. */
ULONG *xe,*ye;		/* size of changed area. */
ULONG special;		/* Special Info. */
void *extra;		/* extra info needed to decode delta */
{
  ULONG row_dec,exitflag,changed,block_cnt;
  ULONG code0,code1;
  LONG x,y,min_x,max_x,min_y,max_y;
  UBYTE *dptr;

  changed = 0;
  max_x = max_y = 0;	min_x = imagex;	min_y = imagey;
  dptr = delta;
  row_dec = imagex + 3;
  x = 0;
  y = imagey - 1;
  exitflag = 0;
  block_cnt = ((imagex * imagey) >> 4) + 1;

  if (map_flag == TRUE)
  {
    if (x11_bytes_pixel == 4)
    {
      while(!exitflag)
      {
	code0 =  *dptr++;	code1 =  *dptr++;	block_cnt--;
	if ( ((code1==0) && (code0==0) && !block_cnt) || (y<0)) exitflag = 1;
	else
	{
	  if ((code1 >= 0x84) && (code1 <= 0x87)) /* skip */
	  { ULONG skip = ((code1 - 0x84) << 8) + code0;
	    block_cnt -= (skip-1); while(skip--) AVI_BLOCK_INC(x,y,imagex);
	  }
	  else /* single block encoded */
	  {
	    if (code1 >= 0x90) /* 8 color quadrant encoding */
	    { ULONG cA0,cA1,cB0,cB1;
	      ULONG *i_ptr = (ULONG *)(image + ((y * imagex + x) << 2) );
	      cB0 = (ULONG)map[*dptr++];  cA0 = (ULONG)map[*dptr++];
	      cB1 = (ULONG)map[*dptr++];  cA1 = (ULONG)map[*dptr++];
	      AVI_CRAM_C4(i_ptr,code0,cA0,cA1,cB0,cB1,row_dec); i_ptr -=row_dec;
	      cB0 = (ULONG)map[*dptr++];  cA0 = (ULONG)map[*dptr++];
	      cB1 = (ULONG)map[*dptr++];  cA1 = (ULONG)map[*dptr++];
	      AVI_CRAM_C4(i_ptr,code1,cA0,cA1,cB0,cB1,row_dec);
	    } else if (code1 < 0x80) /* 2 color encoding */
	    { register ULONG clr_A,clr_B;
	      ULONG *i_ptr = (ULONG *)(image + ((y * imagex + x) << 2) );
	      clr_B = (ULONG)map[*dptr++];   clr_A = (ULONG)map[*dptr++];
	      AVI_CRAM_C2(i_ptr,code0,clr_A,clr_B,row_dec); i_ptr -= row_dec;
	      AVI_CRAM_C2(i_ptr,code1,clr_A,clr_B,row_dec);
	    }
	    else /* 1 color encoding */
	    { ULONG clr = (ULONG)map[code0]; 
	      ULONG *i_ptr = (ULONG *)(image + ((y * imagex + x) << 2) );
	      AVI_CRAM_C1(i_ptr,clr,row_dec);
	    }
	    AVI_MIN_MAX_CHECK(x,y,min_x,max_x,min_y,max_y);
	    changed = 1; AVI_BLOCK_INC(x,y,imagex);
	  } /* end of single block */
	} /* end of not term code */
      } /* end of not while exit */
    } /* end of 4 bytes pixel */
    else if (x11_bytes_pixel == 2)
    {
      while(!exitflag)
      {
	code0 =  *dptr++;	code1 =  *dptr++;	block_cnt--;
	if ( ((code1==0) && (code0==0) && !block_cnt) || (y<0)) exitflag = 1;
	else
	{
	  if ((code1 >= 0x84) && (code1 <= 0x87)) /* skip */
	  { ULONG skip = ((code1 - 0x84) << 8) + code0;
	    block_cnt -= (skip-1); while(skip--) AVI_BLOCK_INC(x,y,imagex);
	  } else /* single block encoded */
	  {
	    if (code1 >= 0x90) /* 8 color quadrant encoding */
	    {
	      USHORT cA0,cA1,cB0,cB1;
	      USHORT *i_ptr = (USHORT *)(image + ((y * imagex + x) << 1) );
	      cB0 = map[*dptr++];  cA0 = map[*dptr++];
	      cB1 = map[*dptr++];  cA1 = map[*dptr++];
	      AVI_CRAM_C4(i_ptr,code0,cA0,cA1,cB0,cB1,row_dec); i_ptr -=row_dec;
	      cB0 = map[*dptr++];  cA0 = map[*dptr++];
	      cB1 = map[*dptr++];  cA1 = map[*dptr++];
	      AVI_CRAM_C4(i_ptr,code1,cA0,cA1,cB0,cB1,row_dec);
	    } /* end of 8 color quadrant encoding */
	    else if (code1 < 0x80) /* 2 color encoding */
	    { USHORT clr_A,clr_B;
	      USHORT *i_ptr = (USHORT *)(image + ((y * imagex + x) << 1) );
	      clr_B = (USHORT)map[*dptr++];   clr_A = (USHORT)map[*dptr++];
	      AVI_CRAM_C2(i_ptr,code0,clr_A,clr_B,row_dec); i_ptr -= row_dec;
	      AVI_CRAM_C2(i_ptr,code1,clr_A,clr_B,row_dec);
	    } /* end of 2 color */
	    else /* 1 color encoding */
	    { USHORT clr = (USHORT)map[code0];
	      USHORT *i_ptr = (USHORT *)(image + ((y * imagex + x) << 1) );
	      AVI_CRAM_C1(i_ptr,clr,row_dec);
	    }
	    AVI_MIN_MAX_CHECK(x,y,min_x,max_x,min_y,max_y);
	    changed = 1; AVI_BLOCK_INC(x,y,imagex);
	  } /* end of single block */
	} /* end of not term code */
      } /* end of not while exit */
    } /* end of 2 bytes pixel */
    else /* (x11_bytes_pixel == 1) */
    {
      while(!exitflag)
      {
	code0 =  *dptr++;	code1 =  *dptr++;	block_cnt--;
	if ( ((code1==0) && (code0==0) && !block_cnt) || (y<0)) exitflag = 1;
	else
	{
	  if ((code1 >= 0x84) && (code1 <= 0x87)) /* skip */
	  { ULONG skip = ((code1 - 0x84) << 8) + code0;
	    block_cnt -= (skip-1); while(skip--) AVI_BLOCK_INC(x,y,imagex);
	  } else /* single block encoded */
	  { 
	    if (code1 >= 0x90) /* 8 color quadrant encoding */
	    { UBYTE cA0,cA1,cB0,cB1;
	      UBYTE *i_ptr = (UBYTE *)(image + y * imagex + x);
	      cB0 = (UBYTE)map[*dptr++];  cA0 = (UBYTE)map[*dptr++];
	      cB1 = (UBYTE)map[*dptr++];  cA1 = (UBYTE)map[*dptr++];
	      AVI_CRAM_C4(i_ptr,code0,cA0,cA1,cB0,cB1,row_dec); i_ptr -=row_dec;
	      cB0 = (UBYTE)map[*dptr++];  cA0 = (UBYTE)map[*dptr++];
	      cB1 = (UBYTE)map[*dptr++];  cA1 = (UBYTE)map[*dptr++];
	      AVI_CRAM_C4(i_ptr,code1,cA0,cA1,cB0,cB1,row_dec);
	    } 
	    else if (code1 < 0x80) /* 2 color encoding */
	    { UBYTE clr_A,clr_B;
	      UBYTE *i_ptr = (UBYTE *)(image + y * imagex + x);
	      clr_B = (UBYTE)map[*dptr++];   clr_A = (UBYTE)map[*dptr++];
	      AVI_CRAM_C2(i_ptr,code0,clr_A,clr_B,row_dec); i_ptr -= row_dec;
	      AVI_CRAM_C2(i_ptr,code1,clr_A,clr_B,row_dec);
	    }
	    else /* 1 color encoding */
	    { UBYTE clr = (UBYTE)map[code0];
	      UBYTE *i_ptr = (UBYTE *)(image + y * imagex + x);
	      AVI_CRAM_C1(i_ptr,clr,row_dec);
	    }
	    AVI_MIN_MAX_CHECK(x,y,min_x,max_x,min_y,max_y);
	    changed = 1; AVI_BLOCK_INC(x,y,imagex);
	  } /* end of single block */
	} /* end of not term code */
      } /* end of not while exit */
    } /* end of 1 bytes pixel */
  } /* end of map is TRUE */
  else
  {
      while(!exitflag)
      {
	code0 =  *dptr++;	code1 =  *dptr++;	block_cnt--;
	if ( ((code1==0) && (code0==0) && !block_cnt) || (y<0)) exitflag = 1;
	else
	{
	  if ((code1 >= 0x84) && (code1 <= 0x87)) /* skip */
	  { ULONG skip = ((code1 - 0x84) << 8) + code0;
	    block_cnt -= (skip-1); while(skip--) AVI_BLOCK_INC(x,y,imagex);
	  } else /* single block encoded */
	  {
	    if (code1 >= 0x90) /* 8 color quadrant encoding */
	    {
	      UBYTE cA0,cA1,cB0,cB1;
	      UBYTE *i_ptr = (UBYTE *)(image + y * imagex + x);
	      cB0 = (UBYTE)*dptr++;  cA0 = (UBYTE)*dptr++;
	      cB1 = (UBYTE)*dptr++;  cA1 = (UBYTE)*dptr++;
	      AVI_CRAM_C4(i_ptr,code0,cA0,cA1,cB0,cB1,row_dec); i_ptr -=row_dec;
	      cB0 = (UBYTE)*dptr++;  cA0 = (UBYTE)*dptr++;
	      cB1 = (UBYTE)*dptr++;  cA1 = (UBYTE)*dptr++;
	      AVI_CRAM_C4(i_ptr,code1,cA0,cA1,cB0,cB1,row_dec);
	    } 
	    else if (code1 < 0x80) /* 2 color encoding */
	    { UBYTE clr_A,clr_B;
	      UBYTE *i_ptr = (UBYTE *)(image + y * imagex + x);
	      clr_B = (UBYTE)*dptr++;   clr_A = (UBYTE)*dptr++;
	      AVI_CRAM_C2(i_ptr,code0,clr_A,clr_B,row_dec); i_ptr -= row_dec;
	      AVI_CRAM_C2(i_ptr,code1,clr_A,clr_B,row_dec);
	    } /* end of 2 color */
	    else /* 1 color encoding */
	    {
	      UBYTE clr = (UBYTE)code0;
	      UBYTE *i_ptr = (UBYTE *)(image + y * imagex + x);
	      AVI_CRAM_C1(i_ptr,clr,row_dec);
	    }
	    AVI_MIN_MAX_CHECK(x,y,min_x,max_x,min_y,max_y);
	    changed = 1; AVI_BLOCK_INC(x,y,imagex);
	  } /* end of single block */
	} /* end of not term code */
      } /* end of not while exit */
  }
  if (xa_optimize_flag == TRUE)
  {
    if (changed) { *xs=min_x; *ys=min_y - 3; *xe=max_x + 4; *ye=max_y + 1; }
    else  { *xs = *ys = *xe = *ye = 0; return(ACT_DLTA_NOP); }
  }
  else { *xs = *ys = 0; *xe = imagex; *ye = imagey; }
  if (map_flag) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}


/*
 * Routine to Decode an AVI RGB chunk
 * (i.e. just copy it into the image buffer)
 * courtesy of Julian Bradfield.
 */

ULONG
AVI_Decode_RGB(image,delta,dsize,chdr,map,map_flag,imagex,imagey,imaged,
						xs,ys,xe,ye,special,extra)
UBYTE *image;		/* Image Buffer. */
UBYTE *delta;		/* delta data. */
ULONG dsize;		/* delta size */
XA_CHDR *chdr;		/* color map info */
ULONG *map;		/* used if it's going to be remapped. */
ULONG map_flag;		/* whether or not to use remap_map info. */
ULONG imagex,imagey;	/* Size of image buffer. */
ULONG imaged;		/* Depth of Image. (IFF specific) */
ULONG *xs,*ys;		/* pos of changed area. */
ULONG *xe,*ye;		/* size of changed area. */
ULONG special;		/* Special Info. */
void *extra;		/* extra info needed to decode delta */
{ ULONG oddflag;
  UBYTE *dptr = delta;
  
  oddflag = imagex & 0x01;
  if (map_flag == TRUE)
  {
    if (x11_bytes_pixel == 4)
    { LONG x,y = imagey - 1;
      while ( y >= 0 )
      { ULONG *i_ptr = (ULONG *)(image + ((y * imagex)<<2) ); y--; 
        x = imagex; while(x--) *i_ptr++ = (ULONG)map[*dptr++];
	if (oddflag) dptr++;
      }
    }
    else if (x11_bytes_pixel == 2)
    { LONG x,y = imagey - 1;
      while ( y >= 0 )
      { USHORT *i_ptr = (USHORT *)(image + ((y * imagex)<<1) ); y--; 
        x = imagex; while(x--) *i_ptr++ = (USHORT)map[*dptr++];
	if (oddflag) dptr++;
      }
    }
    else /* (x11_bytes_pixel == 1) */
    { LONG x,y = imagey - 1;
      while ( y >= 0 )
      { UBYTE *i_ptr = (UBYTE *)(image + y * imagex); y--; 
        x = imagex; while(x--) *i_ptr++ = (UBYTE)map[*dptr++];
	if (oddflag) dptr++;
      }
    }
  } /* end of map is TRUE */
  else
  { LONG x,y = imagey - 1;
    while ( y >= 0 )
    { UBYTE *i_ptr = (UBYTE *)(image + y * imagex); y--; 
      x = imagex; while(x--) *i_ptr++ = (UBYTE)*dptr++;
	if (oddflag) dptr++;
    }
  }
 *xs = *ys = 0; *xe = imagex; *ye = imagey;
  if (map_flag) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}

/*******************
 *  RGB Decode Depth 24
 ******/
ULONG
AVI_Decode_RGB24(image,delta,dsize,tchdr,map,map_flag,imagex,imagey,imaged,
						xs,ys,xe,ye,special,extra)
UBYTE *image;		/* Image Buffer. */
UBYTE *delta;		/* delta data. */
ULONG dsize;		/* delta size */
XA_CHDR *tchdr;		/* color map info */
ULONG *map;		/* used if it's going to be remapped. */
ULONG map_flag;		/* whether or not to use remap_map info. */
ULONG imagex,imagey;	/* Size of image buffer. */
ULONG imaged;		/* Depth of Image. (IFF specific) */
ULONG *xs,*ys;		/* pos of changed area. */
ULONG *xe,*ye;		/* size of changed area. */
ULONG special;		/* Special Info. */
void *extra;		/* extra info needed to decode delta */
{ UBYTE *dp = delta;
  ULONG oddflag;
  ULONG special_flag = special & 0x0001;
  XA_CHDR *chdr;

  if (tchdr) {chdr=(tchdr->new_chdr)?(tchdr->new_chdr):(tchdr);} else chdr=0;
  
  oddflag = imagex & 0x01;

  if (special_flag)
  { LONG x,y = imagey - 1;
    while ( y >= 0 )
    { ULONG *iptr = (ULONG *)(image + (y * imagex * 3)); y--; 
      x = imagex;
      while(x--) { UBYTE r,g,b; r = *dp++; g = *dp++; b = *dp++;
       *iptr++ = r; *iptr++ = g; *iptr++ = b; }
      if (oddflag) dp++; /* pad to short words ness in RGB 8 but here? */
    }
  }
  else if ( (map_flag==FALSE) || (x11_bytes_pixel==1) )
  { LONG x,y = imagey - 1;
    while ( y >= 0 )
    { UBYTE *iptr = (UBYTE *)(image + y * imagex); y--; 
      x = imagex; 
      while(x--) 
      { ULONG r,g,b; r = (ULONG)*dp++; g = (ULONG)*dp++; b = (ULONG)*dp++;
        *iptr++ = (UBYTE)QT_Get_Color24(r,g,b,map_flag,map,chdr);
      }
      if (oddflag) dp++;
    }
  }
  else if (x11_bytes_pixel==4)
  { LONG x,y = imagey - 1;
    while ( y >= 0 )
    { ULONG *iptr = (ULONG *)(image + ((y * imagex)<<2) ); y--; 
      x = imagex; 
      while(x--) 
      { ULONG r,g,b; r = (ULONG)*dp++; g = (ULONG)*dp++; b = (ULONG)*dp++;
        *iptr++ = (ULONG)QT_Get_Color24(r,g,b,map_flag,map,chdr);
      }
      if (oddflag) dp++;
    }
  }
  else if (x11_bytes_pixel==2)
  { LONG x,y = imagey - 1;
    while ( y >= 0 )
    { USHORT *iptr = (USHORT *)(image + ((y * imagex)<<2) ); y--; 
      x = imagex; 
      while(x--) 
      { ULONG r,g,b; r = (ULONG)*dp++; g = (ULONG)*dp++; b = (ULONG)*dp++;
        *iptr++ = (USHORT)QT_Get_Color24(r,g,b,map_flag,map,chdr);
      }
      if (oddflag) dp++;
    }
  }
  *xs = *ys = 0; *xe = imagex; *ye = imagey;
  if (map_flag) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}


void AVI_Print_ID(fout,id)
FILE *fout;
LONG id;
{
 fprintf(fout,"%c",     ((id >> 24) & 0xff)   );
 fprintf(fout,"%c",     ((id >> 16) & 0xff)   );
 fprintf(fout,"%c",     ((id >>  8) & 0xff)   );
 fprintf(fout,"%c(%lx)", (id        & 0xff),id);
}


ULONG AVI_Get_Color(color,map_flag,map,chdr)
ULONG color,map_flag,*map;
XA_CHDR *chdr;
{
  register ULONG clr,ra,ga,ba,tr,tg,tb;
 
  ra = (color >> 10) & 0x1f;
  ga = (color >>  5) & 0x1f;
  ba =  color & 0x1f;
  tr = (ra << 3) | (ra >> 2);
  tg = (ga << 3) | (ga >> 2);
  tb = (ba << 3) | (ba >> 2);
  if (xa_gamma_flag==TRUE) { tr = xa_gamma_adj[tr]>>8;  
     tg = xa_gamma_adj[tg]>>8; tb = xa_gamma_adj[tb]>>8; }

 
  if (x11_display_type & XA_X11_TRUE) clr = X11_Get_True_Color(ra,ga,ba,5);
  else
  { 
    if ((cmap_color_func == 4) && (chdr))
    { register ULONG cache_i = color & 0x7fff;
      if (cmap_cache == 0) CMAP_Cache_Init(0);
      if (chdr != cmap_cache_chdr)
      {
	CMAP_Cache_Clear();
	cmap_cache_chdr = chdr;
      }
      if (cmap_cache[cache_i] == 0xffff)
      {
        clr = chdr->coff + 
	   CMAP_Find_Closest(chdr->cmap,chdr->csize,ra,ga,ba,5,5,5,TRUE);
        cmap_cache[cache_i] = (USHORT)clr;
      }
      else clr = (ULONG)cmap_cache[cache_i];
    }
    else
    {
      if (cmap_true_to_332 == TRUE) 
	  clr=CMAP_GET_332(ra,ga,ba,CMAP_SCALE5);
      else   clr = CMAP_GET_GRAY(ra,ga,ba,CMAP_SCALE10);
      if (map_flag) clr = map[clr];
    }
  }
  return(clr);
}


ULONG
AVI_Decode_RLE8(image,delta,dsize,chdr,map,map_flag,imagex,imagey,imaged,
                                                xs,ys,xe,ye,special,extra)
UBYTE *image;           /* Image Buffer. */
UBYTE *delta;           /* delta data. */
LONG dsize;             /* delta size */
XA_CHDR *chdr;          /* color map info */
ULONG *map;             /* used if it's going to be remapped. */
ULONG map_flag;         /* whether or not to use remap_map info. */
ULONG imagex,imagey;    /* Size of image buffer. */
ULONG imaged;           /* Depth of Image. (IFF specific) */
ULONG *xs,*ys;          /* pos of changed area. */
ULONG *xe,*ye;          /* size of changed area. */
ULONG special;          /* Special Info. */
void *extra;            /* extra info needed to decode delta */
{
  ULONG opcode,mod;
  LONG x,y,min_x,max_x,min_y,max_y;
  UBYTE *dptr;

  max_x = max_y = 0; min_x = imagex; min_y = imagey;
  x = 0;  y = imagey - 1;
  dptr = delta;

  while( (y >= 0) && (dsize > 0) )
  {
    mod = *dptr++;
    opcode = *dptr++;  dsize-=2;

    if (mod == 0x00)				/* END-OF-LINE */
    {
      if (opcode==0x00)
      {
        while(x > imagex) { x -=imagex; y--; }
        x = 0; y--;
      }
      else if (opcode==0x01)			/* END Of Image */
      {
        y = -1;
      }
      else if (opcode==0x02)			/* SKIP */
      {
        ULONG yskip,xskip;
        xskip = *dptr++; 
        yskip = *dptr++;  dsize-=2;
        x += xskip;
        y -= yskip;
      }
      else					/* ABSOLUTE MODE */
      {
        int cnt = opcode;
        
	dsize-=cnt;
        while(x >= imagex) { x -= imagex; y--; }
	if (y > max_y) max_y = y; if (x < min_x) x = min_x;
        if (map_flag==TRUE)
	{
	  if (x11_bytes_pixel==1)
          { UBYTE *iptr = (UBYTE *)(image + (y * imagex + x) );
            while(cnt--) 
	    { if (x >= imagex) { max_x = imagex; min_x = 0;
		 x -= imagex; y--; iptr = (UBYTE *)(image+y*imagex+x); }
              *iptr++ = (UBYTE)map[*dptr++];  x++;
	    }
	  }
	  else if (x11_bytes_pixel==2)
          { USHORT *iptr = (USHORT *)(image + ((y * imagex + x)<<1) );
            while(cnt--) 
	    { if (x >= imagex)  { max_x = imagex; min_x = 0;
		x -= imagex; y--; iptr = (USHORT *)(image+y*imagex+x); }
              *iptr++ = (USHORT)map[*dptr++];  x++;
	    }
	  }
	  else /* if (x11_bytes_pixel==4) */
          { ULONG *iptr = (ULONG *)(image + ((y * imagex + x)<<2) );
            while(cnt--) 
	    { if (x >= imagex)  { max_x = imagex; min_x = 0;
		x -= imagex; y--; iptr = (ULONG *)(image+y*imagex+x); }
              *iptr++ = (ULONG)map[*dptr++];  x++;
	    }
	  }
        }
        else
        { UBYTE *iptr = (UBYTE *)(image + (y * imagex + x) );
          while(cnt--) 
	  { if (x >= imagex)  { max_x = imagex; min_x = 0;
		x -=imagex; y--; iptr = (UBYTE *)(image+y*imagex+x); }
	    *iptr++ = (UBYTE)(*dptr++); x++;
	  }
        }
        if (opcode & 0x01) { dptr++; dsize--; }
        if (y < min_y) min_y = y; if (x > max_x) x = max_x;
      }
    }
    else					/* ENCODED MODE */
    {
      int color,cnt;
      while(x >= imagex) { x -=imagex; y--; }
      if (y > max_y) max_y = y; if (x < min_x) x = min_x;
      cnt = mod;
      color = (map_flag==TRUE)?(map[opcode]):(opcode);
      if ( (map_flag==FALSE) || (x11_bytes_pixel==1) )
      { UBYTE *iptr = (UBYTE *)(image + (y * imagex + x) );
	UBYTE clr = (UBYTE)color;
	while(cnt--) 
	{ if (x >= imagex) { max_x = imagex; min_x = 0;
		x -=imagex; y--; iptr = (UBYTE *)(image+y*imagex+x); }
	  *iptr++ = clr; x++;
	}
      }
      else if (x11_bytes_pixel==2)
      { USHORT *iptr = (USHORT *)(image + ((y * imagex + x)<<1) );
	USHORT clr = (USHORT)color;
	while(cnt--) 
	{ if (x >= imagex)  { max_x = imagex; min_x = 0;
		x -=imagex; y--; iptr = (USHORT *)(image+y*imagex+x); }
	  *iptr++ = clr; x++;
	}
      }
      else /* if (x11_bytes_pixel==4) */
      { ULONG *iptr = (ULONG *)(image + ((y * imagex + x)<<2) );
	ULONG clr = (ULONG)color;
	while(cnt--) 
	{ if (x >= imagex)  { max_x = imagex; min_x = 0;
		x -=imagex; y--; iptr = (ULONG *)(image+y*imagex+x); }
	  *iptr++ = clr; x++;
	}
      }
      if (y < min_y) min_y = y; if (x > max_x) x = max_x;
    }
  } /* end of while */

  if (xa_optimize_flag == TRUE)
  {
    max_x++; if (max_x>imagex) max_x=imagex;
    max_y++; if (max_y>imagey) max_y=imagey;
    if ((min_x >= max_x) || (min_y >= max_y)) /* no change */
		{ *xs = *ys = *xe = *ye = 0; return(ACT_DLTA_NOP); }
    else	{ *xs=min_x; *ys=min_y; *xe=max_x; *ye=max_y; }
  }
  else { *xs = *ys = 0; *xe = imagex; *ye = imagey; }
  if (map_flag) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}

ULONG
AVI_Decode_CRAM16(image,delta,dsize,tchdr,map,map_flag,imagex,imagey,imaged,
						xs,ys,xe,ye,special,extra)
UBYTE *image;		/* Image Buffer. */
UBYTE *delta;		/* delta data. */
ULONG dsize;		/* delta size */
XA_CHDR *tchdr;		/* color map info */
ULONG *map;		/* used if it's going to be remapped. */
ULONG map_flag;		/* whether or not to use remap_map info. */
ULONG imagex,imagey;	/* Size of image buffer. */
ULONG imaged;		/* Depth of Image. (IFF specific) */
ULONG *xs,*ys;		/* pos of changed area. */
ULONG *xe,*ye;		/* size of changed area. */
ULONG special;		/* Special Info. */
void *extra;		/* extra info needed to decode delta */
{
  ULONG row_dec,exitflag,changed,block_cnt;
  ULONG code0,code1;
  LONG x,y,min_x,max_x,min_y,max_y;
  UBYTE *dptr;
  XA_CHDR *chdr;

  if (tchdr) {chdr=(tchdr->new_chdr)?(tchdr->new_chdr):(tchdr);} else chdr=0;
  changed = 0;
  max_x = max_y = 0;	min_x = imagex;	min_y = imagey;
  dptr = delta;
  if (special) row_dec = (3*(imagex+4))-1; else row_dec = imagex + 3; 
  x = 0;
  y = imagey - 1;
  exitflag = 0;
  block_cnt = ((imagex * imagey) >> 4) + 1;

  if (special == 1)
  {
    while(!exitflag)
    {
      code0 =  *dptr++;	code1 =  *dptr++;	block_cnt--;
      if ( (code1==0) && (code0==0) && !block_cnt) { exitflag = 1; continue; }
      if (y < 0) {exitflag = 1; continue; }
      if ((code1 >= 0x84) && (code1 <= 0x87)) /* skip */
      { ULONG skip = ((code1 - 0x84) << 8) + code0;
	block_cnt -= (skip-1); while(skip--) AVI_BLOCK_INC(x,y,imagex);
      }
      else /* not skip */
      { UBYTE *i_ptr = (UBYTE *)(image + 3 * (y * imagex + x) );
	if (code1 < 0x80) /* 2 or 8 color encoding */
	{ ULONG cA,cB; UBYTE rA0,gA0,bA0,rB0,gB0,bB0;
	  AVI_GET_16(cB,dptr); AVI_Get_RGBColor(rB0,gB0,bB0,cB);
	  AVI_GET_16(cA,dptr); AVI_Get_RGBColor(rA0,gA0,bA0,cA); 
	  if (cB & 0x8000)   /* Eight Color Encoding */
	  { UBYTE rA1,gA1,bA1,rB1,gB1,bB1;
	    register flag = code0;
	    AVI_GET_16(cB,dptr); AVI_Get_RGBColor(rB1,gB1,bB1,cB);
	    AVI_GET_16(cA,dptr); AVI_Get_RGBColor(rA1,gA1,bA1,cA); 
	    AVI_CRAM_rgbC4(i_ptr,flag,rA0,gA0,bA0,rB0,gB0,bB0); 
	    i_ptr++; flag >>= 2;
	    AVI_CRAM_rgbC4(i_ptr,flag,rA1,gA1,bA1,rB1,gB1,bB1); 
	    i_ptr -= row_dec; flag >>= 2;
	    AVI_CRAM_rgbC4(i_ptr,flag,rA0,gA0,bA0,rB0,gB0,bB0); 
	    i_ptr++; flag >>= 2;
	    AVI_CRAM_rgbC4(i_ptr,flag,rA1,gA1,bA1,rB1,gB1,bB1); 
	    i_ptr -= row_dec; flag = code1;
	    AVI_GET_16(cB,dptr); AVI_Get_RGBColor(rB0,gB0,bB0,cB);
	    AVI_GET_16(cA,dptr); AVI_Get_RGBColor(rA0,gA0,bA0,cA); 
	    AVI_GET_16(cB,dptr); AVI_Get_RGBColor(rB1,gB1,bB1,cB);
	    AVI_GET_16(cA,dptr); AVI_Get_RGBColor(rA1,gA1,bA1,cA); 
	    AVI_CRAM_rgbC4(i_ptr,flag,rA0,gA0,bA0,rB0,gB0,bB0); 
	    i_ptr++; flag >>= 2;
	    AVI_CRAM_rgbC4(i_ptr,flag,rA1,gA1,bA1,rB1,gB1,bB1); 
	    i_ptr -= row_dec; flag >>= 2;
	    AVI_CRAM_rgbC4(i_ptr,flag,rA0,gA0,bA0,rB0,gB0,bB0); 
	    i_ptr++; flag >>= 2;
	    AVI_CRAM_rgbC4(i_ptr,flag,rA1,gA1,bA1,rB1,gB1,bB1); 
	  } else /* Two Color Encoding */
	  { register ULONG flag = code0;
	    AVI_CRAM_rgbC2(i_ptr,flag,rA0,gA0,bA0,rB0,gB0,bB0); 
	    i_ptr -= row_dec; flag >>= 4;
	    AVI_CRAM_rgbC2(i_ptr,flag,rA0,gA0,bA0,rB0,gB0,bB0); 
	    i_ptr -= row_dec; flag = code1;
	    AVI_CRAM_rgbC2(i_ptr,flag,rA0,gA0,bA0,rB0,gB0,bB0); 
	    i_ptr -= row_dec; flag >>= 4;
	    AVI_CRAM_rgbC2(i_ptr,flag,rA0,gA0,bA0,rB0,gB0,bB0); 
	  }
	} /* end of 2 or 8 */
	else /* 1 color encoding (80-83) && (>=88)*/
	{ ULONG cA = (code1<<8) | code0;
	  UBYTE r,g,b;
	  AVI_Get_RGBColor(r,g,b,cA);
	  AVI_CRAM_rgbC1(i_ptr,r,g,b);  i_ptr -= row_dec;
	  AVI_CRAM_rgbC1(i_ptr,r,g,b);  i_ptr -= row_dec;
	  AVI_CRAM_rgbC1(i_ptr,r,g,b);  i_ptr -= row_dec;
	  AVI_CRAM_rgbC1(i_ptr,r,g,b);
	}
	changed = 1; AVI_MIN_MAX_CHECK(x,y,min_x,max_x,min_y,max_y);
	AVI_BLOCK_INC(x,y,imagex);
      } /* end of not skip */
    } /* end of not while exit */
  } /* end of special */
  else
  {
    if ( (x11_bytes_pixel == 1) || (map_flag == FALSE) )
    {
      while(!exitflag)
      {
	code0 =  *dptr++;	code1 =  *dptr++;	block_cnt--;
	if ( (code1==0) && (code0==0) && !block_cnt) { exitflag = 1; continue; }
	if (y < 0) {exitflag = 1; continue; }
	if ((code1 >= 0x84) && (code1 <= 0x87)) /* skip */
	{ ULONG skip = ((code1 - 0x84) << 8) + code0;
	  block_cnt -= (skip-1); while(skip--) AVI_BLOCK_INC(x,y,imagex);
	}
	else /* not skip */
	{ UBYTE *i_ptr = (UBYTE *)(image + (y * imagex + x) );
	  if (code1 < 0x80) /* 2 or 8 color encoding */
	  { ULONG cA,cB; UBYTE cA0,cB0;
	    AVI_GET_16(cB,dptr); AVI_GET_16(cA,dptr);
	    cB0 = (UBYTE)AVI_Get_Color(cB,map_flag,map,chdr);
	    cA0 = (UBYTE)AVI_Get_Color(cA,map_flag,map,chdr);
	    if (cB & 0x8000)   /* Eight Color Encoding */
	    { UBYTE cA1,cB1;
	      AVI_GET_16(cB,dptr); AVI_GET_16(cA,dptr);
	      cB1 = (UBYTE)AVI_Get_Color(cB,map_flag,map,chdr);
	      cA1 = (UBYTE)AVI_Get_Color(cA,map_flag,map,chdr);
	      AVI_CRAM_C4(i_ptr,code0,cA0,cA1,cB0,cB1,row_dec); i_ptr -=row_dec;
	      AVI_GET_16(cB,dptr); AVI_GET_16(cA,dptr);
	      cB0 = (UBYTE)AVI_Get_Color(cB,map_flag,map,chdr);
	      cA0 = (UBYTE)AVI_Get_Color(cA,map_flag,map,chdr);
	      AVI_GET_16(cB,dptr); AVI_GET_16(cA,dptr);
	      cB1 = (UBYTE)AVI_Get_Color(cB,map_flag,map,chdr);
	      cA1 = (UBYTE)AVI_Get_Color(cA,map_flag,map,chdr);
	      AVI_CRAM_C4(i_ptr,code1,cA0,cA1,cB0,cB1,row_dec);
	    } else /* Two Color Encoding */
	    { 
	      AVI_CRAM_C2(i_ptr,code0,cA0,cB0,row_dec); i_ptr -= row_dec;
	      AVI_CRAM_C2(i_ptr,code1,cA0,cB0,row_dec);
	    }
	  } /* end of 2 or 8 */
	  else /* 1 color encoding (80-83) && (>=88)*/
	  { ULONG cA = (code1<<8) | code0;
	    UBYTE clr = (UBYTE)AVI_Get_Color(cA,map_flag,map,chdr);
	    AVI_CRAM_C1(i_ptr,clr,row_dec);
	  }
	  changed = 1; AVI_MIN_MAX_CHECK(x,y,min_x,max_x,min_y,max_y);
	  AVI_BLOCK_INC(x,y,imagex);
	} /* end of not skip */
      } /* end of not while exit */
    } /* end of 1 bytes pixel */
    else if (x11_bytes_pixel == 2)
    {
      while(!exitflag)
      {
	code0 =  *dptr++;	code1 =  *dptr++;	block_cnt--;
	if ( (code1==0) && (code0==0) && !block_cnt) { exitflag = 1; continue; }
	if (y < 0) {exitflag = 1; continue; }
	if ((code1 >= 0x84) && (code1 <= 0x87)) /* skip */
	{ ULONG skip = ((code1 - 0x84) << 8) + code0;
	  block_cnt -= (skip-1); while(skip--) AVI_BLOCK_INC(x,y,imagex);
	}
	else /* not skip */
	{ USHORT *i_ptr = (USHORT *)(image + ((y * imagex + x) << 1) );
	  if (code1 < 0x80) /* 2 or 8 color encoding */
	  { ULONG cA,cB; USHORT cA0,cB0;
	    AVI_GET_16(cB,dptr); AVI_GET_16(cA,dptr);
	    cB0 = (USHORT)AVI_Get_Color(cB,map_flag,map,chdr);
	    cA0 = (USHORT)AVI_Get_Color(cA,map_flag,map,chdr);
	    if (cB & 0x8000)   /* Eight Color Encoding */
	    { USHORT cA1,cB1;
	      AVI_GET_16(cB,dptr); AVI_GET_16(cA,dptr);
	      cB1 = (USHORT)AVI_Get_Color(cB,map_flag,map,chdr);
	      cA1 = (USHORT)AVI_Get_Color(cA,map_flag,map,chdr);
	      AVI_CRAM_C4(i_ptr,code0,cA0,cA1,cB0,cB1,row_dec); i_ptr -=row_dec;
	      AVI_GET_16(cB,dptr); AVI_GET_16(cA,dptr);
	      cB0 = (USHORT)AVI_Get_Color(cB,map_flag,map,chdr);
	      cA0 = (USHORT)AVI_Get_Color(cA,map_flag,map,chdr);
	      AVI_GET_16(cB,dptr); AVI_GET_16(cA,dptr);
	      cB1 = (USHORT)AVI_Get_Color(cB,map_flag,map,chdr);
	      cA1 = (USHORT)AVI_Get_Color(cA,map_flag,map,chdr);
	      AVI_CRAM_C4(i_ptr,code1,cA0,cA1,cB0,cB1,row_dec);
	    } else /* Two Color Encoding */
	    { 
	      AVI_CRAM_C2(i_ptr,code0,cA0,cB0,row_dec); i_ptr -= row_dec;
	      AVI_CRAM_C2(i_ptr,code1,cA0,cB0,row_dec);
	    }
	  } /* end of 2 or 8 */
	  else /* 1 color encoding (80-83) && (>=88)*/
	  { ULONG cA = (code1<<8) | code0;
	    USHORT clr = (USHORT)AVI_Get_Color(cA,map_flag,map,chdr);
	    AVI_CRAM_C1(i_ptr,clr,row_dec);
	  }
	  changed = 1; AVI_MIN_MAX_CHECK(x,y,min_x,max_x,min_y,max_y);
	  AVI_BLOCK_INC(x,y,imagex);
	} /* end of not skip */
      } /* end of not while exit */
    } /* end of 2 bytes pixel */
    else if (x11_bytes_pixel == 4)
    {
      while(!exitflag)
      {
	code0 =  *dptr++;	code1 =  *dptr++;	block_cnt--;
	if ( (code1==0) && (code0==0) && !block_cnt) { exitflag = 1; continue; }
	if (y < 0) {exitflag = 1; continue; }
	if ((code1 >= 0x84) && (code1 <= 0x87)) /* skip */
	{ ULONG skip = ((code1 - 0x84) << 8) + code0;
	  block_cnt -= (skip-1); while(skip--) AVI_BLOCK_INC(x,y,imagex);
	}
	else /* not skip */
	{ ULONG *i_ptr = (ULONG *)(image + ((y * imagex + x) << 2) );
	  if (code1 < 0x80) /* 2 or 8 color encoding */
	  { ULONG cA,cB,cA0,cB0;
	    AVI_GET_16(cB,dptr); AVI_GET_16(cA,dptr);
	    cB0 = AVI_Get_Color(cB,map_flag,map,chdr);
	    cA0 = AVI_Get_Color(cA,map_flag,map,chdr);
	    if (cB & 0x8000)   /* Eight Color Encoding */
	    { ULONG cA1,cB1;
	      AVI_GET_16(cB,dptr); AVI_GET_16(cA,dptr);
	      cB1 = AVI_Get_Color(cB,map_flag,map,chdr);
	      cA1 = AVI_Get_Color(cA,map_flag,map,chdr);
	      AVI_CRAM_C4(i_ptr,code0,cA0,cA1,cB0,cB1,row_dec); i_ptr -=row_dec;
	      AVI_GET_16(cB,dptr); AVI_GET_16(cA,dptr);
	      cB0 = AVI_Get_Color(cB,map_flag,map,chdr);
	      cA0 = AVI_Get_Color(cA,map_flag,map,chdr);
	      AVI_GET_16(cB,dptr); AVI_GET_16(cA,dptr);
	      cB1 = AVI_Get_Color(cB,map_flag,map,chdr);
	      cA1 = AVI_Get_Color(cA,map_flag,map,chdr);
	      AVI_CRAM_C4(i_ptr,code1,cA0,cA1,cB0,cB1,row_dec);
	    } else /* Two Color Encoding */
	    {
	      AVI_CRAM_C2(i_ptr,code0,cA0,cB0,row_dec); i_ptr -= row_dec;
	      AVI_CRAM_C2(i_ptr,code1,cA0,cB0,row_dec);
	    }
	  } /* end of 2 or 8 */
	  else /* 1 color encoding (80-83) && (>=88)*/
	  { ULONG cA = (code1<<8) | code0;
	    ULONG clr = AVI_Get_Color(cA,map_flag,map,chdr);
	    AVI_CRAM_C1(i_ptr,clr,row_dec);
	  }
	  changed = 1; AVI_MIN_MAX_CHECK(x,y,min_x,max_x,min_y,max_y);
	  AVI_BLOCK_INC(x,y,imagex);
	} /* end of not skip */
      } /* end of not while exit */
    } /* end of 4 bytes pixel */
  } /* end of not special */
  if (xa_optimize_flag == TRUE)
  {
    if (changed) { *xs=min_x; *ys=min_y - 3; *xe=max_x + 4; *ye=max_y + 1; }
    else  { *xs = *ys = *xe = *ye = 0; return(ACT_DLTA_NOP); }
  }
  else { *xs = *ys = 0; *xe = imagex; *ye = imagey; }
  if (map_flag) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}

#define ULTI_CHROM_NORM 0
#define ULTI_CHROM_UNIQ 1
#define ULTI_STREAM0 0x0
#define ULTI_STREAM1 0x4

#define AVI_ULTI_C2(ip,flag,CST,c0,c1,rinc) { \
  *ip++ =(CST)((flag&0x80)?(c1):(c0)); *ip++ =(CST)(flag&0x40)?(c1):(c0); \
  *ip++ =(CST)((flag&0x20)?(c1):(c0)); *ip++ =(CST)(flag&0x10)?(c1):(c0);  \
  ip += rinc; \
  *ip++ =(CST)((flag&0x08)?(c1):(c0)); *ip++ =(CST)(flag&0x04)?(c1):(c0); \
  *ip++ =(CST)((flag&0x02)?(c1):(c0)); *ip++ =(CST)(flag&0x01)?(c1):(c0); }

#define AVI_ULTI_rgbC2(p,msk,r0,r1,g0,g1,b0,b1,rinc) { \
 if (msk&0x80) {*p++=r0; *p++=g0; *p++=b0;} else {*p++=r1; *p++=g1; *p++=b1;} \
 if (msk&0x40) {*p++=r0; *p++=g0; *p++=b0;} else {*p++=r1; *p++=g1; *p++=b1;} \
 if (msk&0x20) {*p++=r0; *p++=g0; *p++=b0;} else {*p++=r1; *p++=g1; *p++=b1;} \
 if (msk&0x10) {*p++=r0; *p++=g0; *p++=b0;} else {*p++=r1; *p++=g1; *p++=b1;} \
 p += rinc; \
 if (msk&0x08) {*p++=r0; *p++=g0; *p++=b0;} else {*p++=r1; *p++=g1; *p++=b1;} \
 if (msk&0x04) {*p++=r0; *p++=g0; *p++=b0;} else {*p++=r1; *p++=g1; *p++=b1;} \
 if (msk&0x02) {*p++=r0; *p++=g0; *p++=b0;} else {*p++=r1; *p++=g1; *p++=b1;} \
 if (msk&0x01) {*p++=r0; *p++=g0; *p++=b0;} else {*p++=r1; *p++=g1; *p++=b1;} }

ULONG
AVI_Decode_ULTI(image,delta,dsize,tchdr,map,map_flag,imagex,imagey,imaged,
						xs,ys,xe,ye,special,extra)
UBYTE *image;		/* Image Buffer. */
UBYTE *delta;		/* delta data. */
ULONG dsize;		/* delta size */
XA_CHDR *tchdr;		/* color map info */
ULONG *map;		/* used if it's going to be remapped. */
ULONG map_flag;		/* whether or not to use remap_map info. */
ULONG imagex,imagey;	/* Size of image buffer. */
ULONG imaged;		/* Depth of Image. (IFF specific) */
ULONG *xs,*ys;		/* pos of changed area. */
ULONG *xe,*ye;		/* size of changed area. */
ULONG special;		/* Special Info. */
void *extra;		/* extra info needed to decode delta */
{
  ULONG r_inc,exitflag,changed;
  LONG block_cnt;
  LONG x,y,min_x,max_x,min_y,max_y;
  UBYTE *dptr;
  XA_CHDR *chdr;
  ULONG stream_mode,chrom_mode,chrom_next_uniq;
  ULONG bhedr,opcode,chrom;

  stream_mode = ULTI_STREAM0;
  chrom_mode = ULTI_CHROM_NORM;
  chrom_next_uniq = FALSE;
  if (tchdr) {chdr=(tchdr->new_chdr)?(tchdr->new_chdr):(tchdr);} else chdr=0;
  changed = 0;
  max_x = max_y = 0;	min_x = imagex;	min_y = imagey;
  dptr = delta;
  r_inc = imagex - 4; if (special) r_inc *= 3;
  x = 0;
  y = 0;
  exitflag = 0;
  block_cnt = ((imagex * imagey) >> 6) + 1;
  while(!exitflag)
  { 
    bhedr = *dptr++;

    if ( (y > imagey) || (block_cnt < 0) )
    {
      exitflag = 1;
      fprintf(stderr,"y = %ld block_cnt = %ld bhedr = %lx\n",y,block_cnt,bhedr);
      continue;
    }
    else if ( (bhedr & 0xf8) == 0x70) /* ESCAPE */
    {
      switch(bhedr)
      {
	case 0x70: /* stream mode toggle */ 
	  { ULONG d;
	    d = *dptr++;
	    if (d==0) stream_mode = ULTI_STREAM0;
	    else if (d==1) stream_mode = ULTI_STREAM1;
	    else { fprintf(stderr,"ULTI: stream err %ld\n",stream_mode);
	           TheEnd(); }
	  }
	  break;
	case 0x71: /* next blk has uniq chrom */
	  chrom_next_uniq = TRUE;
	  break;
	case 0x72: /* chrom mode toggle */
	  chrom_mode = (chrom_mode==ULTI_CHROM_NORM)?(ULTI_CHROM_UNIQ)
						    :(ULTI_CHROM_NORM); 
	  break;
	case 0x73: /* Frame Guard */
	  exitflag = 1; 
	  break;
	case 0x74: /* Skip */
	  { ULONG cnt = (ULONG)(*dptr++);
	    block_cnt -= cnt;
	    while(cnt--) { x += 8; if (x>=imagex) { x=0; y += 8; } }
	  }
	  break;
	default: /* reserved escapes */
	  fprintf(stderr,"Reserved Escapes %lx\n",bhedr);
	  exitflag = 1;
	  break;
      }
    } /* end of escape */
    else /* not escape */
    { ULONG chrom_flag;
      ULONG quadrant,msh;

      block_cnt--;
      if ( (chrom_mode==ULTI_CHROM_UNIQ) || (chrom_next_uniq == TRUE) )
      { 
	chrom_next_uniq = FALSE;
	chrom_flag = TRUE;
	chrom = 0; 
      }
      else 
      { 
	chrom_flag = FALSE; 
	if (bhedr != 0x00) chrom = *dptr++; /* read chrom */
      }
      msh = 8;
      for(quadrant=0;quadrant<4;quadrant++)
      { ULONG tx,ty;
	/* move to quadrant */
	if (quadrant==0) {tx=x; ty=y;}
	else if (quadrant==1) ty+=4;
	else if (quadrant==2) tx+=4;
	else ty-=4;
	msh -= 2;
	opcode = ((bhedr >> msh) & 0x03) | stream_mode;

        /* POSSIBLY TEST FOR 0x04 or 0x00 1st */
	switch(opcode)
	{
	  case 0x04:
	  case 0x00:  /* Unchanged quadrant */
		/* ??? in unique chrom mode is there a chrom for this quad?? */
	  break;

	  case 0x05:
	  case 0x01:  /* Homogenous/shallow LTC quadrant */
	  {
	    ULONG angle,y0,y1;
	    if (chrom_flag==TRUE) { chrom = *dptr++; }
	    y0 = *dptr++;  angle = (y0 >> 6) & 0x03;  y0 &= 0x3f;
	    if (angle == 0)
	    {
	       AVI_ULTI_LTC(image,tx,ty,imagex,special,map_flag,map,chdr,
	 					   y0,y0,y0,y0,chrom,angle);
	    }
	    else
	    {  y1 = y0 + 1; if (y1 > 63) y1 = 63;
	       if (angle==3) angle = 12;
	       else if (angle==2) angle = 6;
	       else angle = 2; 
	       AVI_ULTI_LTC(image,tx,ty,imagex,special,map_flag,map,chdr,
	 					y0,y0,y1,y1,chrom,angle);
	    }
	  }
	  break;

	  case 0x02:  /* LTC quadrant */
	  { ULONG angle,ltc_idx,y0,y1,y2,y3;
	    UBYTE *tmp;
	    if (chrom_flag==TRUE) { chrom = *dptr++; }
	    ltc_idx = (*dptr++) << 8; ltc_idx |= (*dptr++);
	    angle = (ltc_idx >> 12) & 0x0f; /* 4 bits angle */
	    ltc_idx &= 0x0fff; /* index to 4 byte lum table */
	    tmp = &avi_ulti_tab[ ( ltc_idx << 2 ) ];
	    y0 = (ULONG)(*tmp++); y1 = (ULONG)(*tmp++); 
	    y2 = (ULONG)(*tmp++); y3 = (ULONG)(*tmp++);
	    AVI_ULTI_LTC(image,tx,ty,imagex,special,map_flag,map,chdr,
					y0,y1,y2,y3,chrom,angle);
	  }
	  break;

	  case 0x03:  /* Statistical/extended LTC */
	  { ULONG d;
	    if (chrom_flag==TRUE) { chrom = *dptr++; }
	    d = *dptr++;
	    if (d & 0x80) /* extend LTC */
	    { ULONG angle,y0,y1,y2,y3;
	      angle = (d >> 4) & 0x07; /* 3 bits angle */
	      d =  (d << 8) | (*dptr++);
	      y0 = (d >> 6) & 0x3f;    y1 = d & 0x3f;
	      y2 = (*dptr++) & 0x3f;   y3 = (*dptr++) & 0x3f;
	      AVI_ULTI_LTC(image,tx,ty,imagex,special,map_flag,map,chdr,
	 					y0,y1,y2,y3,chrom,angle);
	    }
	    else /* Statistical pattern */
	    { ULONG y0,y1; UBYTE flag0,flag1;
	      flag0 = *dptr++;  flag1 = (UBYTE)d;
	      y0 = (*dptr++) & 0x3f;   y1 = (*dptr++) & 0x3f;
	      /* bit 0 => y0, bit 1 = > y1. */
	      /* raster scan order MSB = 0 */
	      if (special)
	      { UBYTE r0,r1,g0,g1,b0,b1;
		UBYTE *ip = (UBYTE *)(image + 3 * (ty * imagex + tx) );
	        AVI_Get_Ulti_rgbColor(y0,chrom,&r0,&g0,&b0);
	        AVI_Get_Ulti_rgbColor(y1,chrom,&r1,&g1,&b1);
	        AVI_ULTI_rgbC2(ip,flag1,r0,r1,g0,g1,b0,b1,r_inc); 
		ip += r_inc;
	        AVI_ULTI_rgbC2(ip,flag0,r0,r1,g0,g1,b0,b1,r_inc); 
	      }
	      else 
	      { ULONG c0,c1;
	        c0 = AVI_Get_Ulti_Color(y0,chrom,map_flag,map,chdr);
	        c1 = AVI_Get_Ulti_Color(y1,chrom,map_flag,map,chdr);
	        if ( (x11_bytes_pixel==1) || (map_flag==FALSE) )
	        { UBYTE *ip = (UBYTE *)(image + (ty * imagex + tx) );
	          AVI_ULTI_C2(ip,flag1,UBYTE,c0,c1,r_inc); ip += r_inc;
	          AVI_ULTI_C2(ip,flag0,UBYTE,c0,c1,r_inc);
		}
	        else if (x11_bytes_pixel==4)
	        { ULONG *ip = (ULONG *)(image + ((ty * imagex + tx)<<2) );
	          AVI_ULTI_C2(ip,flag1,ULONG,c0,c1,r_inc); ip += r_inc;
	          AVI_ULTI_C2(ip,flag0,ULONG,c0,c1,r_inc);
		}
	        else /* (x11_bytes_pixel==2) */
	        { USHORT *ip = (USHORT *)(image + ((ty * imagex + tx)<<1) );
	          AVI_ULTI_C2(ip,flag1,USHORT,c0,c1,r_inc); ip += r_inc;
	          AVI_ULTI_C2(ip,flag0,USHORT,c0,c1,r_inc);
		}
	      }
	    }
	  }
	  break;

	  case 0x06:  /* Subsampled 4-luminance quadrant */
	  { ULONG y0,y1,y2,y3;
	    if (chrom_flag==TRUE) { chrom = *dptr++; }
	    y3 = (*dptr++) << 16; y3 |= (*dptr++) << 8; y3 |= (*dptr++);
	    y0 = (y3 >> 18) & 0x3f;  /* NW */
	    y1 = (y3 >> 12) & 0x3f;  /* NE */
	    y2 = (y3 >>  6) & 0x3f;  /* SW */
	    y3 &= 0x3f;   /* SE */
	    AVI_ULTI_LTC(image,tx,ty,imagex,special,map_flag,map,chdr,
					y0,y1,y2,y3,chrom,0x10);
	  }
	  break;

	  case 0x07:  /* 16-luminance quadrant */
	  { ULONG i,d,y[16];
	    if (chrom_flag==TRUE) { chrom = *dptr++; }
	    d = (*dptr++) << 16; d |= (*dptr++) << 8; d |= (*dptr++);
	    y[0] = (d >> 18) & 0x3f;   y[1] = (d >> 12) & 0x3f;
	    y[2] = (d >>  6) & 0x3f;   y[3] = d & 0x3f;
	    d = (*dptr++) << 16; d |= (*dptr++) << 8; d |= (*dptr++);
	    y[4] = (d >> 18) & 0x3f;   y[5] = (d >> 12) & 0x3f;
	    y[6] = (d >>  6) & 0x3f;   y[7] = d & 0x3f;
	    d = (*dptr++) << 16; d |= (*dptr++) << 8; d |= (*dptr++);
	    y[8] = (d >> 18) & 0x3f;   y[9] = (d >> 12) & 0x3f;
	    y[10] = (d >>  6) & 0x3f;  y[11] = d & 0x3f;
	    d = (*dptr++) << 16; d |= (*dptr++) << 8; d |= (*dptr++);
	    y[12] = (d >> 18) & 0x3f;  y[13] = (d >> 12) & 0x3f;
	    y[14] = (d >>  6) & 0x3f;  y[15] = d & 0x3f;

	    if (special)
	    { UBYTE r,g,b,*ip = (UBYTE *)(image + 3 * (ty * imagex + tx) );
	      for(i=0;i<16;i++)
	      { AVI_Get_Ulti_rgbColor(y[i],chrom,&r,&g,&b);
		*ip++ = r; *ip++ = g; *ip++ = b; if ( (i%4)==3) ip += r_inc;
	      }
	    }
	    else if ( (x11_bytes_pixel==1) || (map_flag==FALSE) )
	    { UBYTE c,*ip = (UBYTE *)(image + (ty * imagex + tx) );
	      for(i=0;i<16;i++)
	      { c = (UBYTE)AVI_Get_Ulti_Color(y[i],chrom,map_flag,map,chdr);
		*ip++ = c; if ( (i%4)==3) ip += r_inc; }
	    }
	    else if (x11_bytes_pixel==4)
	    { ULONG c,*ip = (ULONG *)(image + ((ty * imagex + tx)<<2) );
	      for(i=0;i<16;i++)
	      { c = (ULONG)AVI_Get_Ulti_Color(y[i],chrom,map_flag,map,chdr);
		*ip++ = c; if ( (i%4)==3) ip += r_inc; }
	    }
	    else /* if (x11_bytes_pixel==2) */
	    { USHORT c,*ip = (USHORT *)(image + ((ty * imagex + tx)<<1) );
	      for(i=0;i<16;i++)
	      { c = (USHORT)AVI_Get_Ulti_Color(y[i],chrom,map_flag,map,chdr);
		*ip++ = c; if ( (i%4)==3) ip += r_inc; }
	    }
	  }
	  break;
	  default:
		fprintf(stderr,"Error opcode=%lx\n",opcode);
		break;
	} /* end of switch opcode */
      } /* end of 4 quadrant */
      { x += 8; if (x>=imagex) { x=0; y += 8; } }
    } /* end of not escape */
  } /* end of while */
changed = 1;
  { *xs = *ys = 0; *xe = imagex; *ye = imagey; }
  if (map_flag) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}


void AVI_ULTI_Gen_YUV()
{
  float r0,r1,b0,b1;
  LONG i;

  r0 = 16384.0 *  1.40200;
  b0 = 16384.0 *  1.77200;
  r1 = 16384.0 * -0.71414;
  b1 = 16384.0 * -0.34414;

  for(i=0;i<16;i++)
  { LONG tmp; float cr,cb;
    tmp = (i & 0x0f); 
    cr = 63.0 * ( ((float)(tmp) - 5.0) / 40.0);  
    cb = 63.0 * ( ((float)(tmp) - 6.0) / 34.0);
    ulti_Cr[i] = (LONG)(r0 * (float)(cr) );
    ulti_Cb[i] = (LONG)(b0 * (float)(cb) );
  }
  for(i=0;i<256;i++)
  { LONG tmp; float cr,cb;
    tmp = (i & 0x0f); 
    cr = 63.0 * ( ((float)(tmp) - 5.0) / 40.0);  
    tmp = ( (i>>4) & 0x0f); 
    cb = 63.0 * ( ((float)(tmp) - 6.0) / 34.0);
    ulti_CrCb[i] = (LONG)(b1 * cb + r1 * cr); 
  }
}

/*
> >     y = y6 / 63
> >     u = (u4 - 5) / 40
> >     v = (v4 - 6) / 34

I should have mentioned that these numbers come from inspecting the
weird decimals used in the specification:

    2.037 should be 2.032 = 63/31

    8.226 = 255/31

    6.375 = 255/40

    7.5   = 255/34
*/

void AVI_Get_Ulti_rgbColor(lum,chrom,r,g,b)
LONG lum;
ULONG chrom;
UBYTE *r,*g,*b;
{ ULONG cr,cb,ra,ga,ba;
  LONG tmp;
  if (cmap_true_to_gray == TRUE) { ra = ba = ga = lum; }
  else
  {
  lum <<= 14; 
  cb = (chrom >> 4) & 0x0f;   cr = chrom & 0x0f;
  tmp = (lum + ulti_Cr[cr]) >> 14; 
  if (tmp < 0) tmp = 0; else if (tmp > 63) tmp = 63;  ra = tmp;
  tmp = (lum + ulti_Cb[cb]) >> 14;
  if (tmp < 0) tmp = 0; else if (tmp > 63) tmp = 63;  ba = tmp;
  tmp = (lum + ulti_CrCb[chrom]) >> 14;
  if (tmp < 0) tmp = 0; else if (tmp > 63) tmp = 63;  ga = tmp;
  }
  *r = (UBYTE)( (ra << 2) | (ra >> 4) );
  *g = (UBYTE)( (ga << 2) | (ga >> 4) );
  *b = (UBYTE)( (ba << 2) | (ba >> 4) );
}

ULONG AVI_Get_Ulti_Color(lum,chrom,map_flag,map,chdr)
LONG lum;		/* 6 bits of lum */
ULONG chrom;		/* 8 bits of chrom */
ULONG map_flag,*map;
XA_CHDR *chdr;
{
  register ULONG cr,cb,clr,ra,ga,ba,tr,tg,tb;
  LONG tmp;

  if (cmap_true_to_gray == TRUE) { ra = ba = ga = lum; }
  else
  { LONG lum1 = lum << 14;
  cb = (chrom >> 4) & 0x0f;   cr = chrom & 0x0f;
  tmp = (lum1 + ulti_Cr[cr]) >> 14; 
  if (tmp < 0) tmp = 0; else if (tmp > 63) tmp = 63;  ra = tmp;
  tmp = (lum1 + ulti_Cb[cb]) >> 14;
  if (tmp < 0) tmp = 0; else if (tmp > 63) tmp = 63;  ba = tmp;
  tmp = (lum1 + ulti_CrCb[chrom]) >> 14;
  if (tmp < 0) tmp = 0; else if (tmp > 63) tmp = 63;  ga = tmp;
  }

  tr = (ra << 2) | (ra >> 4);
  tg = (ga << 2) | (ga >> 4);
  tb = (ba << 2) | (ba >> 4);
  if (xa_gamma_flag==TRUE) { tr = xa_gamma_adj[tr]>>8;  
     tg = xa_gamma_adj[tg]>>8; tb = xa_gamma_adj[tb]>>8; }
 
  if (x11_display_type & XA_X11_TRUE) clr = X11_Get_True_Color(ra,ga,ba,5);
  else
  { 
    if ((cmap_color_func == 4) && (chdr))
    { register ULONG cache_i = ((lum << 8) | chrom) & 0x3fff;
      if (cmap_cache == 0) CMAP_Cache_Init(0);
      if (chdr != cmap_cache_chdr)
      {
	CMAP_Cache_Clear();
	cmap_cache_chdr = chdr;
      }
      if (cmap_cache[cache_i] == 0xffff)
      {
        clr = chdr->coff + 
	   CMAP_Find_Closest(chdr->cmap,chdr->csize,ra,ga,ba,6,6,6,TRUE);
        cmap_cache[cache_i] = (USHORT)clr;
      }
      else clr = (ULONG)cmap_cache[cache_i];
    }
    else
    {
      if (cmap_true_to_332 == TRUE) 
	  clr=CMAP_GET_332(tr,tg,tb,CMAP_SCALE8);
      else   clr = CMAP_GET_GRAY(tr,tg,tb,CMAP_SCALE13);
      if (map_flag) clr = map[clr];
    }
  }
  return(clr);
}

#define AVI_ULTI_0000(ip,CST,c0,c1,c2,c3,r_inc) { \
  *ip++ =(CST)c0; *ip++ =(CST)c1; *ip++ =(CST)c2; *ip =(CST)c3; ip += r_inc; \
  *ip++ =(CST)c0; *ip++ =(CST)c1; *ip++ =(CST)c2; *ip =(CST)c3; ip += r_inc; \
  *ip++ =(CST)c0; *ip++ =(CST)c1; *ip++ =(CST)c2; *ip =(CST)c3; ip += r_inc; \
  *ip++ =(CST)c0; *ip++ =(CST)c1; *ip++ =(CST)c2; *ip =(CST)c3; }

#define AVI_ULTI_0225(ip,CST,c0,c1,c2,c3,r_inc) { \
  *ip++ =(CST)c1; *ip++ =(CST)c2; *ip++ =(CST)c3; *ip =(CST)c3; ip += r_inc; \
  *ip++ =(CST)c0; *ip++ =(CST)c1; *ip++ =(CST)c2; *ip =(CST)c3; ip += r_inc; \
  *ip++ =(CST)c0; *ip++ =(CST)c1; *ip++ =(CST)c2; *ip =(CST)c3; ip += r_inc; \
  *ip++ =(CST)c0; *ip++ =(CST)c0; *ip++ =(CST)c1; *ip =(CST)c2; }

#define AVI_ULTI_0450(ip,CST,c0,c1,c2,c3,r_inc) { \
  *ip++ =(CST)c1; *ip++ =(CST)c2; *ip++ =(CST)c3; *ip =(CST)c3; ip += r_inc; \
  *ip++ =(CST)c1; *ip++ =(CST)c2; *ip++ =(CST)c2; *ip =(CST)c3; ip += r_inc; \
  *ip++ =(CST)c0; *ip++ =(CST)c1; *ip++ =(CST)c1; *ip =(CST)c2; ip += r_inc; \
  *ip++ =(CST)c0; *ip++ =(CST)c0; *ip++ =(CST)c1; *ip =(CST)c2; }

#define AVI_ULTI_0675(ip,CST,c0,c1,c2,c3,r_inc) { \
  *ip++ =(CST)c2; *ip++ =(CST)c3; *ip++ =(CST)c3; *ip =(CST)c3; ip += r_inc; \
  *ip++ =(CST)c1; *ip++ =(CST)c2; *ip++ =(CST)c2; *ip =(CST)c3; ip += r_inc; \
  *ip++ =(CST)c0; *ip++ =(CST)c1; *ip++ =(CST)c1; *ip =(CST)c2; ip += r_inc; \
  *ip++ =(CST)c0; *ip++ =(CST)c0; *ip++ =(CST)c0; *ip =(CST)c1; }

#define AVI_ULTI_0900(ip,CST,c0,c1,c2,c3,r_inc) { \
  *ip++ =(CST)c3; *ip++ =(CST)c3; *ip++ =(CST)c3; *ip =(CST)c3; ip += r_inc; \
  *ip++ =(CST)c2; *ip++ =(CST)c2; *ip++ =(CST)c2; *ip =(CST)c2; ip += r_inc; \
  *ip++ =(CST)c1; *ip++ =(CST)c1; *ip++ =(CST)c1; *ip =(CST)c1; ip += r_inc; \
  *ip++ =(CST)c0; *ip++ =(CST)c0; *ip++ =(CST)c0; *ip =(CST)c0; }

#define AVI_ULTI_1125(ip,CST,c0,c1,c2,c3,r_inc) { \
  *ip++ =(CST)c3; *ip++ =(CST)c3; *ip++ =(CST)c3; *ip =(CST)c2; ip += r_inc; \
  *ip++ =(CST)c3; *ip++ =(CST)c2; *ip++ =(CST)c2; *ip =(CST)c1; ip += r_inc; \
  *ip++ =(CST)c2; *ip++ =(CST)c1; *ip++ =(CST)c1; *ip =(CST)c0; ip += r_inc; \
  *ip++ =(CST)c1; *ip++ =(CST)c0; *ip++ =(CST)c0; *ip =(CST)c0; }

#define AVI_ULTI_1350(ip,CST,c0,c1,c2,c3,r_inc) { \
  *ip++ =(CST)c3; *ip++ =(CST)c3; *ip++ =(CST)c2; *ip =(CST)c2; ip += r_inc; \
  *ip++ =(CST)c3; *ip++ =(CST)c2; *ip++ =(CST)c1; *ip =(CST)c1; ip += r_inc; \
  *ip++ =(CST)c2; *ip++ =(CST)c2; *ip++ =(CST)c1; *ip =(CST)c0; ip += r_inc; \
  *ip++ =(CST)c1; *ip++ =(CST)c1; *ip++ =(CST)c0; *ip =(CST)c0; }

#define AVI_ULTI_1575(ip,CST,c0,c1,c2,c3,r_inc) { \
  *ip++ =(CST)c3; *ip++ =(CST)c3; *ip++ =(CST)c2; *ip =(CST)c1; ip += r_inc; \
  *ip++ =(CST)c3; *ip++ =(CST)c2; *ip++ =(CST)c1; *ip =(CST)c0; ip += r_inc; \
  *ip++ =(CST)c3; *ip++ =(CST)c2; *ip++ =(CST)c1; *ip =(CST)c0; ip += r_inc; \
  *ip++ =(CST)c2; *ip++ =(CST)c1; *ip++ =(CST)c0; *ip =(CST)c0; }

#define AVI_ULTI_C4(ip,CST,c0,c1,c2,c3,r_inc) { \
  *ip++ =(CST)c0; *ip++ =(CST)c0; *ip++ =(CST)c1; *ip =(CST)c1; ip += r_inc; \
  *ip++ =(CST)c0; *ip++ =(CST)c0; *ip++ =(CST)c1; *ip =(CST)c1; ip += r_inc; \
  *ip++ =(CST)c2; *ip++ =(CST)c2; *ip++ =(CST)c3; *ip =(CST)c3; ip += r_inc; \
  *ip++ =(CST)c2; *ip++ =(CST)c2; *ip++ =(CST)c3; *ip =(CST)c3; }

void AVI_ULTI_LTC(image,x,y,imagex,special,map_flag,map,chdr,
					y0,y1,y2,y3,chrom,angle)
UBYTE *image;
ULONG x,y,imagex,special,map_flag,*map;
XA_CHDR *chdr;
ULONG y0,y1,y2,y3,chrom,angle;
{ ULONG r_inc;

  if (special)
  { ULONG i;
    UBYTE *ip = (UBYTE *)(image + 3 * (y * imagex + x) );
    UBYTE r[4],g[4],b[4],*ix,idx[16];
    r_inc = 3 * (imagex - 4);
    if (angle & 0x08) /* reverse */
    { angle &= 0x07;
      AVI_Get_Ulti_rgbColor(y3,chrom,&r[0],&g[0],&b[0]);
      AVI_Get_Ulti_rgbColor(y2,chrom,&r[1],&g[1],&b[1]);
      AVI_Get_Ulti_rgbColor(y1,chrom,&r[2],&g[2],&b[2]);
      AVI_Get_Ulti_rgbColor(y0,chrom,&r[3],&g[3],&b[3]);
    }
    else
    {
      AVI_Get_Ulti_rgbColor(y0,chrom,&r[0],&g[0],&b[0]);
      if (y1==y0) {r[1]=r[0]; g[1]=g[0]; b[1]=b[0]; }
      else AVI_Get_Ulti_rgbColor(y1,chrom,&r[1],&g[1],&b[1]);
      if (y2==y1) {r[2]=r[1]; g[2]=g[1]; b[2]=b[1]; }
      else AVI_Get_Ulti_rgbColor(y2,chrom,&r[2],&g[2],&b[2]);
      if (y3==y2) {r[3]=r[2]; g[3]=g[2]; b[3]=b[2]; }
      else AVI_Get_Ulti_rgbColor(y3,chrom,&r[3],&g[3],&b[3]);
    }
    ix = idx;
    switch(angle)
    {   case 0: AVI_ULTI_0000(ix,UBYTE,0,1,2,3,1); break;
	case 1: AVI_ULTI_0225(ix,UBYTE,0,1,2,3,1); break;
	case 2: AVI_ULTI_0450(ix,UBYTE,0,1,2,3,1); break;
	case 3: AVI_ULTI_0675(ix,UBYTE,0,1,2,3,1); break;
	case 4: AVI_ULTI_0900(ix,UBYTE,0,1,2,3,1); break;
	case 5: AVI_ULTI_1125(ix,UBYTE,0,1,2,3,1); break;
	case 6: AVI_ULTI_1350(ix,UBYTE,0,1,2,3,1); break;
	case 7: AVI_ULTI_1575(ix,UBYTE,0,1,2,3,1); break;
	default: AVI_ULTI_C4(ix,UBYTE,0,1,2,3,1); break;
    } /* end switch */
    for(i=0;i<16;i++)
    { register ULONG j = idx[i];
      *ip++ = r[j]; *ip++ = g[j]; *ip++ = b[j];
      if ( (i & 3) == 3 ) ip += r_inc;
    }
  }
  else 
  { ULONG c0,c1,c2,c3;
    r_inc = imagex - 3;
    if (angle & 0x08) /* reverse */
    { angle &= 0x07;
      c0 = AVI_Get_Ulti_Color(y3,chrom,map_flag,map,chdr);
      c1 = AVI_Get_Ulti_Color(y2,chrom,map_flag,map,chdr);
      c2 = AVI_Get_Ulti_Color(y1,chrom,map_flag,map,chdr);
      c3 = AVI_Get_Ulti_Color(y0,chrom,map_flag,map,chdr);
    }
    else
    {
      c0 = AVI_Get_Ulti_Color(y0,chrom,map_flag,map,chdr);
      if (y1==y0) c1 = c0;
      else c1 = AVI_Get_Ulti_Color(y1,chrom,map_flag,map,chdr);
      if (y2==y1) c2 = c1;
      else c2 = AVI_Get_Ulti_Color(y2,chrom,map_flag,map,chdr);
      if (y3==y2) c3 = c2;
      else c3 = AVI_Get_Ulti_Color(y3,chrom,map_flag,map,chdr);
    }

    if ( (x11_bytes_pixel == 1) || (map_flag == FALSE) )
    { UBYTE *ip = (UBYTE *)(image + (y * imagex + x) );
      switch(angle)
      { case 0: AVI_ULTI_0000(ip,UBYTE,c0,c1,c2,c3,r_inc); break;
	case 1: AVI_ULTI_0225(ip,UBYTE,c0,c1,c2,c3,r_inc); break;
	case 2: AVI_ULTI_0450(ip,UBYTE,c0,c1,c2,c3,r_inc); break;
	case 3: AVI_ULTI_0675(ip,UBYTE,c0,c1,c2,c3,r_inc); break;
	case 4: AVI_ULTI_0900(ip,UBYTE,c0,c1,c2,c3,r_inc); break;
	case 5: AVI_ULTI_1125(ip,UBYTE,c0,c1,c2,c3,r_inc); break;
	case 6: AVI_ULTI_1350(ip,UBYTE,c0,c1,c2,c3,r_inc); break;
	case 7: AVI_ULTI_1575(ip,UBYTE,c0,c1,c2,c3,r_inc); break;
	default: AVI_ULTI_C4(ip,UBYTE,c0,c1,c2,c3,r_inc); break;
      }
    }
    else if (x11_bytes_pixel == 4)
    { ULONG *ip = (ULONG *)(image + ((y * imagex + x) << 2) );
      switch(angle)
      { case 0: AVI_ULTI_0000(ip,ULONG,c0,c1,c2,c3,r_inc); break;
	case 1: AVI_ULTI_0225(ip,ULONG,c0,c1,c2,c3,r_inc); break;
	case 2: AVI_ULTI_0450(ip,ULONG,c0,c1,c2,c3,r_inc); break;
	case 3: AVI_ULTI_0675(ip,ULONG,c0,c1,c2,c3,r_inc); break;
	case 4: AVI_ULTI_0900(ip,ULONG,c0,c1,c2,c3,r_inc); break;
	case 5: AVI_ULTI_1125(ip,ULONG,c0,c1,c2,c3,r_inc); break;
	case 6: AVI_ULTI_1350(ip,ULONG,c0,c1,c2,c3,r_inc); break;
	case 7: AVI_ULTI_1575(ip,ULONG,c0,c1,c2,c3,r_inc); break;
	default: AVI_ULTI_C4(ip,ULONG,c0,c1,c2,c3,r_inc); break;
      }
    }
    else /* if (x11_bytes_pixel == 2) */
    { USHORT *ip = (USHORT *)(image + ( (y * imagex + x) << 1) );
      switch(angle)
      { case 0: AVI_ULTI_0000(ip,USHORT,c0,c1,c2,c3,r_inc); break;
	case 1: AVI_ULTI_0225(ip,USHORT,c0,c1,c2,c3,r_inc); break;
	case 2: AVI_ULTI_0450(ip,USHORT,c0,c1,c2,c3,r_inc); break;
	case 3: AVI_ULTI_0675(ip,USHORT,c0,c1,c2,c3,r_inc); break;
	case 4: AVI_ULTI_0900(ip,USHORT,c0,c1,c2,c3,r_inc); break;
	case 5: AVI_ULTI_1125(ip,USHORT,c0,c1,c2,c3,r_inc); break;
	case 6: AVI_ULTI_1350(ip,USHORT,c0,c1,c2,c3,r_inc); break;
	case 7: AVI_ULTI_1575(ip,USHORT,c0,c1,c2,c3,r_inc); break;
	default: AVI_ULTI_C4(ip,USHORT,c0,c1,c2,c3,r_inc); break;
      }
    } /* end of shorts */
  } /* end of not special */
} /* end */


ULONG RIFF_Read_AUDS(fin,size,auds_hdr)
FILE *fin;
LONG size;
AUDS_HDR *auds_hdr;
{ ULONG ret = TRUE;
  auds_hdr->format	= UTIL_Get_LSB_Short(fin);
  auds_hdr->channels	= UTIL_Get_LSB_Short(fin);
  auds_hdr->rate	= UTIL_Get_LSB_Long(fin);
  auds_hdr->av_bps	= UTIL_Get_LSB_Long(fin);
  auds_hdr->align	= UTIL_Get_LSB_Short(fin);
  auds_hdr->size	= UTIL_Get_LSB_Short(fin);

  DEBUG_LEVEL2 fprintf(stderr,"ret = %lx\n",ret);
  if (auds_hdr->format == WAVE_FORMAT_PCM)
  {
    if (auds_hdr->size == 8) avi_audio_type = XA_AUDIO_LINEAR;
    else if (auds_hdr->size == 16) avi_audio_type = XA_AUDIO_SIGNED;
    else avi_audio_type = XA_AUDIO_INVALID;
  }
  else if (auds_hdr->format == WAVE_FORMAT_ADPCM)
  {
    if (auds_hdr->size == 4) avi_audio_type = XA_AUDIO_ADPCM_M;
    else avi_audio_type = XA_AUDIO_INVALID;
  }
  else
  {
    avi_audio_type = XA_AUDIO_INVALID;
    ret = FALSE;
  }
  avi_audio_freq  = auds_hdr->rate;
  avi_audio_chans = auds_hdr->channels;
  if (auds_hdr->size == 8) avi_audio_bps = 1;
  else if (auds_hdr->size == 16) avi_audio_bps = 2;
  else if (auds_hdr->size == 32) avi_audio_bps = 4;
  else if (auds_hdr->size == 4) avi_audio_bps = 1;
  else avi_audio_bps = 1000 + auds_hdr->size;
  avi_audio_end   = 0;
  if (avi_audio_chans > 2) ret = FALSE;
  if (avi_audio_bps > 2) ret = FALSE;

  if (xa_verbose)
  {
    fprintf(stderr,"  Audio: "); AVI_Audio_Type(auds_hdr->format);
    fprintf(stderr," Rate %ld Chans %ld bps %ld\n",
			avi_audio_freq,avi_audio_chans,auds_hdr->size);
     
  }
/* modify type */
  if (avi_audio_chans==2)	avi_audio_type |= XA_AUDIO_STEREO_MSK;
  if (avi_audio_bps==2)		avi_audio_type |= XA_AUDIO_BPS_2_MSK;
  DEBUG_LEVEL2 fprintf(stderr,"ret = %lx\n",ret);
  if (size & 01) size++; size -= 16;
  while(size--) fgetc(fin);
  return(ret);
}

void AVI_Audio_Type(type)
ULONG type;
{
  switch(type)
  {
    case WAVE_FORMAT_PCM: fprintf(stderr,"PCM"); break;
    case WAVE_FORMAT_ADPCM: fprintf(stderr,"ADPCM"); break;
    case WAVE_FORMAT_ALAW: fprintf(stderr,"ALAW"); break;
    case WAVE_FORMAT_MULAW: fprintf(stderr,"ULAW"); break;
    case WAVE_FORMAT_OKI_ADPCM: fprintf(stderr,"OKI_ADPCM"); break;
    case IBM_FORMAT_MULAW: fprintf(stderr,"IBM_ULAW"); break;
    case IBM_FORMAT_ALAW: fprintf(stderr,"IBM_ALAW"); break;
    case IBM_FORMAT_ADPCM: fprintf(stderr,"IBM_ADPCM"); break;
    default: fprintf(stderr,"UNK%lx",type); break;
  }
}

static UBYTE avi_ulti_lin[11] = {2,3,5,6,7,8,11,14,17,20,255};
static UBYTE avi_ulti_lo[15]  = {4,5,6,7,8,11,14,17,20,23,26,29,32,36,255};
static UBYTE avi_ulti_hi[14]  = {6,8,11,14,17,20,23,26,29,32,35,40,46,255};

ULONG AVI_Ulti_Check(val,ptr)
ULONG val;
UBYTE *ptr;
{ 
  while(*ptr != 255) if ((ULONG)(*ptr++) == val) return(1);
  return(0);
}

void AVI_Ulti_Gen_LTC()
{ LONG ys,ye,ydelta;
  UBYTE *utab;
  if (avi_ulti_tab==0) avi_ulti_tab = (UBYTE *)malloc(16384 * sizeof(UBYTE));
  else return;
  if (avi_ulti_tab==0) TheEnd1("AVI_Ulti_Gen_LTC: malloc err");
  utab = avi_ulti_tab;
  for(ys=0; ys <= 63; ys++)
  {
    for(ye=ys; ye <= 63; ye++)
    {
      ydelta = ye - ys;
      if (AVI_Ulti_Check(ydelta,avi_ulti_lin))
	{ ULONG yinc = (ydelta + 1) / 3;
	  *utab++ = ys; *utab++ = ys + yinc; *utab++ = ye - yinc; *utab++ = ye;
	}
      if (AVI_Ulti_Check(ydelta,avi_ulti_lo)==1)
	{ LONG y1,y2;
          float yd = (float)(ydelta);
	  /* 1/4 */
	  y1 = ye - (LONG)(  (2 * yd - 5.0) / 10.0 );
	  y2 = ye - (LONG)(  (    yd - 5.0) / 10.0 );
	  *utab++ = ys; *utab++ = y1; *utab++ = y2; *utab++ = ye;
	  /* 1/2 */
	  y2 = ys + (LONG)(  (2 * yd + 5.0) / 10.0 );
	  *utab++ = ys; *utab++ = y2; *utab++ = y1; *utab++ = ye;
	  /* 3/4 */
	  y1 = ys + (LONG)(  (    yd + 5.0) / 10.0 );
	  *utab++ = ys; *utab++ = y1; *utab++ = y2; *utab++ = ye;
	}
      if (AVI_Ulti_Check(ydelta,avi_ulti_hi)==1)
	{
	  *utab++ = ys; *utab++ = ye; *utab++ = ye; *utab++ = ye;
	  *utab++ = ys; *utab++ = ys; *utab++ = ye; *utab++ = ye;
	  *utab++ = ys; *utab++ = ys; *utab++ = ys; *utab++ = ye;
	}
    }
  }
DEBUG_LEVEL1
{ ULONG i;
  UBYTE *tmp = avi_ulti_tab;
  for(i=0;i<4096;i++)
  {
    fprintf(stderr,"%02lx %02lx %02lx %02lx\n",tmp[0],tmp[1],tmp[2],tmp[3]);
    tmp += 4;
  }
}
}


/* my shifted version */
static UBYTE xmpg_def_intra_qtab[64] = {
         8,16,19,22,22,26,26,27,
        16,16,22,22,26,27,27,29,
        19,22,26,26,27,29,29,35,
        22,24,27,27,29,32,34,38,
        26,27,29,29,32,35,38,46,
        27,29,34,34,35,40,46,56,
        29,34,34,37,40,48,56,69,
        34,37,38,40,48,58,69,83 };


/************************
 *  Find next MPEG start code. If buf is non-zero, get data from
 *  buf. Else use the file pointer, fin.
 *
 ****/
LONG xmpg_get_start_code(bufp,fin,buf_size)
UBYTE **bufp;
FILE *fin;
LONG *buf_size;
{ LONG d,size = *buf_size;
  ULONG state = 0;
  ULONG flag = 0;
  UBYTE *buf;

  if (bufp) {buf = *bufp; flag = 1;}
  while(size > 0)
  { if (flag) d = (LONG)((*buf++) & 0xff);
    else d = fgetc(fin);
    size--;
    if (state == 3) 
    { *buf_size = size; 
      if (flag) *bufp = buf; 
      return(d); 
    }
    else if (state == 2)
    { if (d==0x01) state = 3;
      else if (d==0x00) state = 2;
      else state = 0;
    }
    else
    { if (d==0x00) state++;
      else state = 0;
    }
  }
  if (flag) *bufp = buf;
  *buf_size = 0;
  return((LONG)(-1));
}


/* FROM xanim_mpg.c */
extern UBYTE mpg_dlta_buf;
extern ULONG mpg_dlta_size;

ULONG xmpg_avi_once = 0;

ULONG AVI_XMPG_00XM(avi,fin,ck_size,act,anim_hdr)
XA_ANIM_SETUP *avi;
FILE *fin;
LONG ck_size;
XA_ACTION *act;
XA_ANIM_HDR *anim_hdr;
{ MPG_SEQ_HDR *seq_hdr = 0;
  MPG_PIC_HDR *pic_hdr = 0;
  MPG_SLICE_HDR *slice = 0;
  ACT_DLTA_HDR *dlta_hdr = 0;
  XA_ACTION *tmp_act;
  LONG i,code;

  act->type = ACT_NOP; /* in case of failure */

  /* Allocate and fake out SEQ Header */
  tmp_act = ACT_Get_Action(anim_hdr,ACT_NOP);
  seq_hdr = (MPG_SEQ_HDR *)malloc(sizeof(MPG_SEQ_HDR));
  tmp_act->data = (UBYTE *)seq_hdr;
  tmp_act = 0;
  seq_hdr->width  = avi->imagex;
  seq_hdr->height = avi->imagey;
  for(i=0; i < 64;i++)
  { seq_hdr->intra_qtab[i] = xmpg_def_intra_qtab[i];  /* NO-ZIG */
  }

  { UBYTE *strd_buf = avi_strd;
  i = avi_strd_cursz; 
  if (strd_buf == 0) i = 0; /* redundant checking */
  while(i > 0) 
  {
    code = xmpg_get_start_code(&strd_buf,0,&i);
    if (code < 0) break;
    if (code == MPG_SEQ_START)
    { /* 12 w, 12 h, 4 aspect, 4 picrate, 18 bitrate etc */
      LONG d;
      ULONG width,height,pic_rate,pic_size;
      width = height = pic_rate = 0;
      d = *strd_buf++;  i--;
      if (d >= 0) { width = (d & 0xff) << 4; }
      else break;

      d = *strd_buf++;  i--;
      if (d >= 0) { width |= (d >> 4) & 0x0f; height = (d & 0x0f) << 8; }
      else { width = 0; break; }

      d = *strd_buf++;  i--;
      if (d >= 0) { height |= (d & 0xff); }
      else break;

      if ( (width != avi->imagex) || (height != avi->imagey) )
      {
	if (xmpg_avi_once==0)  
	{ xmpg_avi_once = 1; 
	  fprintf(stderr,"AVI XMPG: Mismatched image size\n");
	}
        seq_hdr->width  = width;
        seq_hdr->height = height;
        avi->imagex = width;
        avi->imagey = height;
        if (width > avi->max_imagex) avi->max_imagex = width;
        if (height > avi->max_imagey) avi->max_imagey = height;
        /* readjust pic_size if necessary(ie it gets larger) */
        pic_size = width * height;
        if ( (avi->pic) && (pic_size > avi->pic_size))
        { UBYTE *tmp_pic;
          if ( (cmap_true_map_flag == TRUE) && (avi->depth > 8) )
               tmp_pic = (UBYTE *)realloc( avi->pic, (3 * pic_size));
          else tmp_pic = (UBYTE *)realloc( avi->pic, (XA_PIC_SIZE(pic_size)) );
          if (tmp_pic == 0) TheEnd1("AVI_XMPG: pic malloc err");
          avi->pic = tmp_pic;
	  avi->pic_size = pic_size;  /* only size it grew */
        }
      }
      break;
    }
  } /* end of while */
  } /* end of test strd */
 
  if (ck_size & 0x01) ck_size++; /* PAD */

  DEBUG_LEVEL1 fprintf(stderr,"AVI_XMPG_00XM:\n");
  /* Loop on MPEG start codes */
  while(ck_size > 0) 
  {
    code = xmpg_get_start_code(0,fin,&ck_size);
    if (code < 0) 
	{fprintf(stderr,"XMPG: start err\n");  return(FALSE); }

    if (code == MPG_USR_START)
        { DEBUG_LEVEL1 fprintf(stderr,"USR START:\n"); }
    else if (code == MPG_EXT_START)
        { DEBUG_LEVEL1 fprintf(stderr,"EXT START:\n"); }
    else if (code == MPG_SEQ_END)
        { DEBUG_LEVEL1 fprintf(stderr,"SEQ END:\n"); }
    else if (code == MPG_PIC_START)
    { LONG d;
      DEBUG_LEVEL1 fprintf(stderr,"PIC START:\n");
      /* Allocate and fake out PIC Header */
      /* 10bits time  3bits type  16bits vbv_delay */

      /* NOTE: MPG_PIC_HDR includes 1 SLICE_HDR free of charge */
      dlta_hdr = (ACT_DLTA_HDR *)
		malloc( sizeof(ACT_DLTA_HDR) + sizeof(MPG_PIC_HDR) );
      if (dlta_hdr == 0) TheEnd1("AVI XMPG dlta malloc err");
      act->data = (UBYTE *)dlta_hdr;
      dlta_hdr->flags = ACT_SNGL_BUF | DLTA_DATA;
      dlta_hdr->fpos = dlta_hdr->fsize = 0;

      pic_hdr = (MPG_PIC_HDR *)(dlta_hdr->data);

      pic_hdr->seq_hdr = seq_hdr;
      pic_hdr->slice_1st = pic_hdr->slice_last = 0;
      pic_hdr->slice_cnt = 0;
      pic_hdr->slices[0].fsize = 0;
      pic_hdr->slices[0].act   = 0;
      pic_hdr->full_forw_flag = 0;
      pic_hdr->forw_r_size = pic_hdr->forw_f = 0;
      pic_hdr->full_back_flag = 0;
      pic_hdr->back_r_size = pic_hdr->back_f = 0;
      /* Assume ONLY I Frames */
      d = fgetc(fin); ck_size--;  /* 8 bits time */
      d = fgetc(fin); ck_size--;  /* 2b time, 3b type, 3b vbv */
      d >>= 3;  pic_hdr->type = d & 0x07;
      if (pic_hdr->type != 0x01) fprintf(stderr,"XMPG: Not I Frame err\n");
      d = fgetc(fin); ck_size--;  /* 8b vbv */
      d = fgetc(fin); ck_size--;  /* 5b vbv */
      AVI_Add_Frame( avi->vid_time, avi->vid_timelo, act);
    }
    else if ((code >= MPG_MIN_SLICE) && (code <= MPG_MAX_SLICE))
    { MPG_SLICE_HDR *last_slice;
      if (pic_hdr == 0) 
	{ fprintf(stderr,"XMPG: no pic hdr\n"); return(FALSE); }

      DEBUG_LEVEL1 fprintf(stderr,"SLICE START:\n");
      last_slice = slice = pic_hdr->slices;
      last_slice++;
      last_slice->fpos = 0;
      last_slice->fsize = 0;
      last_slice->act = 0;

      slice->next = 0;
      slice->act  = 0;
      if (pic_hdr->slice_1st) pic_hdr->slice_last->next = slice;
      else                    pic_hdr->slice_1st = slice;
      pic_hdr->slice_last = slice;
      pic_hdr->slice_cnt++;
      slice->vert_pos = code & 0xff;
      slice->fpos = ftell(fin);  /* get file position */
      if (ck_size > avi->max_fvid_size) avi->max_fvid_size = ck_size;
      slice->fsize = ck_size;


      if ( (xa_buffer_flag == FALSE) && (xa_file_flag == FALSE) )
      { UBYTE *tmp;
        tmp_act = ACT_Get_Action(anim_hdr,ACT_NOP);
        tmp = (UBYTE *)malloc( ck_size );
	if (tmp==0) TheEnd1("MPG: alloc err 3");
        fread((char *)tmp,ck_size,1,fin);
        tmp_act->data = tmp;
        slice->act = tmp_act;
	tmp_act = 0;
      }
      else fseek(fin,ck_size,1); /* else skip over bytes */
      AVI_Add_Frame( avi->vid_time, avi->vid_timelo, act);
      ck_size = 0;
    } /* end of slice */
  } /* end of while */
  if (slice == 0) 
	{fprintf(stderr,"XMPG: failed before slice\n");  return(FALSE); }

  act->type = ACT_DELTA; /* in case of failure */
  return(TRUE);
}


