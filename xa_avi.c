
/*
 * xa_avi.c
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
 * Revision History
 *
 * 16Aug94  video chunks of 0 size now properly used as NOP's with timing info.
 * 12Apr95  added RGB depth 24 support
 * 15Apr95  added XMPG support - what a KLUDGE format.
 * 16Jun95  Removed Cinepak Codec per Radius request.
 *
 ********************************/


#include "xa_avi.h" 
#include "xa_xmpg.h" 
#include "xa_codecs.h"

XA_CODEC_HDR avi_codec_hdr;

#ifdef XA_CINEPAK
extern xaULONG	Cinepak_What_Rev_API();
extern xaLONG   Cinepak_Codec_Query();
#define XA_CINEPAK_QUERY_CNT 1
#else
#define XA_CINEPAK_QUERY_CNT 0
#endif

#ifdef XA_INDEO
extern xaULONG	Indeo_What_Rev_API();
extern xaLONG	Indeo_Codec_Query();
#define XA_INDEO_QUERY_CNT 1
#else
#define XA_INDEO_QUERY_CNT 0
#endif

static xaLONG  AVI_Codec_Query();
static xaLONG  AVI_UNK_Codec_Query();

#define AVI_QUERY_CNT 2 + XA_INDEO_QUERY_CNT + XA_CINEPAK_QUERY_CNT

xaLONG (*avi_query_func[])() = {
#ifdef XA_INDEO
		Indeo_Codec_Query,
#endif
#ifdef XA_CINEPAK
		Cinepak_Codec_Query,
#endif
		AVI_Codec_Query,
		AVI_UNK_Codec_Query};



static xaULONG AVI_IJPG_Read_Ext();
static xaULONG AVI_JPEG_Read_Ext();
xaLONG Is_AVI_File();
xaULONG AVI_Read_File();
void AVI_Print_ID();
AVI_FRAME *AVI_Add_Frame();
void AVI_Free_Frame_List();
xaULONG RIFF_Read_AVIH();
xaULONG RIFF_Read_STRD();
xaULONG RIFF_Read_STRH();
xaULONG RIFF_Read_VIDS();
xaULONG RIFF_Read_AUDS();
void AVI_Audio_Type();
xaULONG AVI_Get_Color();
void AVI_Get_RGBColor();
ACT_DLTA_HDR *AVI_Read_00DC();
void ACT_Setup_Delta();
xaULONG AVI_XMPG_00XM();

/* CODEC ROUTINES */
xaULONG JFIF_Decode_JPEG();
void JFIF_Read_IJPG_Tables();
xaULONG AVI_Decode_RLE8();
xaULONG AVI_Decode_CRAM();
xaULONG AVI_Decode_CRAM16();
xaULONG AVI_Decode_RGB();
xaULONG AVI_Decode_RGB24();
extern xaULONG XA_RGB24_To_CLR32();
xaULONG AVI_Decode_ULTI();
extern xaULONG MPG_Decode_I();
extern void MPG_Init_Stuff();
extern void XA_Gen_YUV_Tabs();
extern void JPG_Alloc_MCU_Bufs();
extern void JPG_Setup_Samp_Limit_Table();
extern jpg_search_marker();
xaULONG AVI_Get_Ulti_Color();
void AVI_Get_Ulti_rgbColor();
void AVI_ULTI_Gen_YUV();
void AVI_ULTI_LTC();
void AVI_Ulti_Gen_LTC();
xaULONG AVI_Ulti_Check();

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
xaULONG CMAP_Find_Closest();
xaUBYTE *UTIL_RGB_To_FS_Map();
xaUBYTE *UTIL_RGB_To_Map();

xaULONG UTIL_Get_MSB_Long();
xaULONG UTIL_Get_LSB_Long();
xaULONG UTIL_Get_LSB_Short();
extern XA_ANIM_SETUP *XA_Get_Anim_Setup();
void XA_Free_Anim_Setup();


xaULONG avi_video_attempt;
/** AVI SOUND STUFF ****/
xaULONG avi_audio_attempt;
xaULONG avi_audio_type;
xaULONG avi_audio_freq;
xaULONG avi_audio_chans;
xaULONG avi_audio_bps;
xaULONG avi_audio_end;
xaULONG XA_Add_Sound();
AUDS_HDR auds_hdr;
char avi_unk_desc[256];


/* Currently used to check 1st frame for Microsoft MJPG screwup */
xaULONG avi_first_delta;

static xaLONG ulti_Cr[16],ulti_Cb[16],ulti_CrCb[256];
xaUBYTE *avi_ulti_tab = 0;



AVI_HDR avi_hdr;
AVI_STREAM_HDR strh_hdr;
VIDS_HDR vids_hdr;
xaULONG avi_use_index_flag;
xaUBYTE *avi_strd;
xaULONG avi_strd_size,avi_strd_cursz;

#define AVI_MAX_STREAMS 2
xaULONG avi_stream_type[4];
xaULONG avi_stream_ok[4];
xaULONG avi_stream_cnt;

xaULONG avi_frame_cnt;
AVI_FRAME *avi_frame_start,*avi_frame_cur;

AVI_FRAME *AVI_Add_Frame(time,timelo,act)
xaULONG time,timelo;
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
xaLONG Is_AVI_File(filename)
char *filename;
{
  FILE *fin;
  xaULONG data1,len,data3;

  if ( (fin=fopen(filename,XA_OPEN_MODE)) == 0) return(xaNOFILE);
  data1 = UTIL_Get_MSB_Long(fin);  /* read past size */
  len   = UTIL_Get_MSB_Long(fin);  /* read past size */
  data3 = UTIL_Get_MSB_Long(fin);  /* read past size */
  fclose(fin);
  if ( (data1 == RIFF_RIFF) && (data3 == RIFF_AVI)) return(xaTRUE);
  return(xaFALSE);
}


xaULONG AVI_Read_File(fname,anim_hdr,audio_attempt)
char *fname;
XA_ANIM_HDR *anim_hdr;
xaULONG audio_attempt;	/* xaTRUE if audio is to be attempted */
{
  FILE *fin;
  xaLONG i,t_time;
  xaULONG t_timelo;
  XA_ACTION *act;
  xaLONG avi_riff_size;
  XA_ANIM_SETUP *avi;
 
  if ( (fin=fopen(fname,XA_OPEN_MODE)) == 0)
  {
    fprintf(stderr,"can't open AVI File %s for reading\n",fname);
    return(xaFALSE);
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
  avi_video_attempt	= xaTRUE;
  avi_riff_size		= 0;
  avi_first_delta	= 0;
  avi_stream_cnt	= 0;
  for(i=0; i < AVI_MAX_STREAMS; i++)
	{ avi_stream_type[i] = 0; avi_stream_ok[i] = xaFALSE; }

  while( !feof(fin) )
  {
    xaULONG d,ck_id,ck_size;

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
                if (RIFF_Read_AVIH(avi,fin,ck_size,&avi_hdr)==xaFALSE) return(xaFALSE);
                break;
 
	case RIFF_strh:
		DEBUG_LEVEL2 fprintf(stderr,"  STRH HDR:\n");
                if (RIFF_Read_STRH(fin,ck_size,&strh_hdr)==xaFALSE) return(xaFALSE);
                break;

	case RIFF_strd:
		DEBUG_LEVEL2 fprintf(stderr,"  STRD HDR:\n");
		avi_strd_cursz = ck_size;
		if (ck_size & 1) ck_size++;
		if (avi_strd_size==0)
		{ avi_strd_size = ck_size;
		  avi_strd = (xaUBYTE *)malloc(ck_size);
		  if (avi_strd==0) TheEnd1("AVI: strd malloc err");
		}
	  	else if (ck_size > avi_strd_size)
		{ xaUBYTE *tmp;
		  avi_strd_size = ck_size;
		  tmp = (xaUBYTE *)realloc(avi_strd,ck_size);
		  if (tmp==0) TheEnd1("AVI: strd malloc err");
		  else avi_strd = tmp;
		}
		fread((char *)(avi_strd),ck_size,1,fin);
		break;
 
	case RIFF_strf:
	  DEBUG_LEVEL2 fprintf(stderr,"  STRF HDR:\n");
	  if  (    (avi_stream_cnt < AVI_MAX_STREAMS)
		&& (avi_stream_cnt < avi_hdr.streams) )
	  { xaULONG str_type,break_flag = xaFALSE;
	    str_type = avi_stream_type[avi_stream_cnt] = strh_hdr.fcc_type;
	    switch(str_type)
	    {
	      case RIFF_vids:
		avi_stream_ok[avi_stream_cnt] = 
			RIFF_Read_VIDS(anim_hdr,avi,fin,ck_size,&vids_hdr);
		break_flag = xaTRUE;
		break;
	      case RIFF_auds:
		{ xaULONG ret = RIFF_Read_AUDS(fin,ck_size,&auds_hdr);
		  if ( (avi_audio_attempt==xaTRUE) && (ret==xaFALSE))
		  {
		    fprintf(stderr,"  AVI Audio Type Unsupported\n");
		    avi_audio_attempt = xaFALSE;
		  }
		  avi_stream_ok[avi_stream_cnt] = avi_audio_attempt;
		}
		break_flag = xaTRUE;
		break;
	      case RIFF_pads:
		if (xa_verbose) fprintf(stderr,"AVI: STRH(pads) ignored\n");
		avi_stream_ok[avi_stream_cnt] = xaFALSE;
		break;
	      default:
		fprintf(stderr,"unknown fcc_type at strf %08lx ",str_type);
		fprintf(stderr,"- ignoring this stream.\n");
		avi_stream_ok[avi_stream_cnt] = xaFALSE;
	    }
	    avi_stream_cnt++;
	    if (break_flag) break;
	  }
	  /* ignore STRF chunk on higher streams or unsupport fcc_types */
	  if (ck_size & 0x01) ck_size++;
	  fseek(fin,ck_size,1); /* move past this chunk */
	  break;
 

	case RIFF_01pc:
	case RIFF_00pc:
		{ xaULONG pc_firstcol,pc_numcols;
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
	  /* Check for EOF */
	  if (feof(fin))  break;	/* End of File */
	
	  /* Check if past end of RIFF Chunk */
	  if (avi_riff_size <= 0)
	  { fprintf(stderr,"  Past End of AVI File\n");
	    fseek(fin,0,2); /* goto end of file */
	    break;
	  }
	  /*** Check if Stream chunk or handle other unknown chunks */
	  { xaULONG stream_id = ck_id & RIFF_FF00;
	    xaULONG stream_type,stream_ok;
		/*** Potentially get stream_type */
	      switch(stream_id)
	      {
		case RIFF_00:	stream_type = avi_stream_type[0];
				stream_ok   = avi_stream_ok[0]; break;
		case RIFF_01:	stream_type = avi_stream_type[1];
				stream_ok   = avi_stream_ok[1]; break;
		case RIFF_02:	stream_type = avi_stream_type[2];
				stream_ok   = avi_stream_ok[2]; break;
		case RIFF_03:	stream_type = avi_stream_type[3];
				stream_ok   = avi_stream_ok[3]; break;
		default: stream_type = 0; stream_ok = xaFALSE;	break;
	      }
	      switch(stream_type)
	      {
		case RIFF_vids:
		  if (stream_ok == xaFALSE)  /* ignore chunk */
		  { if (ck_size & 0x01) ck_size++;
		    fseek(fin,ck_size,1);
		  }
		  else
		  { act = ACT_Get_Action(anim_hdr,ACT_DELTA);
		    if (avi->compression == RIFF_XMPG)
		    { xaULONG ret;
		      ret = AVI_XMPG_00XM(avi,fin,ck_size,act,anim_hdr);
		      if (ret==xaFALSE) return(xaFALSE);
		      MPG_Setup_Delta(avi,fname,act);
		    }
		    else
		    { ACT_DLTA_HDR *dlta_hdr = 
					AVI_Read_00DC(avi,fin,ck_size,act);
		      if (act->type == ACT_NOP) break;
		      if (dlta_hdr == 0)        return(xaFALSE);
		      ACT_Setup_Delta(avi,act,dlta_hdr,fin);
		    }
		  }
		  break;
		case RIFF_auds:
		  { xaULONG snd_size = ck_size;
		    if (ck_size & 0x01) ck_size++;
		    if (ck_size == 0) break;
		    if ((avi_audio_attempt==xaTRUE) && (stream_ok==xaTRUE))
		    { xaLONG ret;
		      if (xa_file_flag==xaTRUE)
		      { xaLONG rets, cur_fpos = ftell(fin);
			rets = XA_Add_Sound(anim_hdr,0,avi_audio_type, 
				cur_fpos, avi_audio_freq, snd_size, 
				&avi->aud_time,&avi->aud_timelo);
			if (rets==xaFALSE) avi_audio_attempt = xaFALSE;
			fseek(fin,ck_size,1); /* move past this chunk */
			if (snd_size > avi->max_faud_size) 
					avi->max_faud_size = snd_size;
		      }
		      else
		      { xaUBYTE *snd = (xaUBYTE *)malloc(ck_size);
			if (snd==0) TheEnd1("AVI: snd malloc err");
			ret = fread( snd, ck_size, 1, fin);
			if (ret != 1) fprintf(stderr,"AVI: snd rd err\n");
			else
			{ int rets; /*NOTE: don't free snd */
			  rets = XA_Add_Sound(anim_hdr,snd,avi_audio_type, -1,
					avi_audio_freq, snd_size, 
					&avi->aud_time, &avi->aud_timelo);
			  if (rets==xaFALSE) avi_audio_attempt = xaFALSE;
			}
		      }
		    } /* valid audio */
		    else fseek(fin,ck_size,1);
		  }
		  break;/* break out - spotlights, sirens, rifles - but he...*/

		case 0x00:   /* NOT A STREAM */
		  DEBUG_LEVEL1 
		  {
		    fprintf(stderr,"AVI: unknown chunk ");
		    AVI_Print_ID(stderr,ck_id);
		    fprintf(stderr,"\n");
		  }
		  if (ck_size & 0x01) ck_size++;
		  fseek(fin,ck_size,1); /* move past this chunk */
		  break;

			/* Unsupported or Unknown stream */
		case RIFF_pads:
		  default:
		  if (ck_size & 0x01) ck_size++;
		  fseek(fin,ck_size,1); /* move past this chunk */
		  break;

	      } /* end of switch */
	  } /* end of default case */
	  break;

      } /* end of ck_id switch */
      /* reduce pessimism */
      avi_riff_size -= ck_size;
      if (ck_size & 0x01) avi_riff_size--; /* odd byte pad */
    } /* while not exitflag */

  if (avi->pic != 0) { FREE(avi->pic,0xA003); avi->pic=0; }
  fclose(fin);

  if (xa_verbose) 
  { float fps =  1000000.0 / (float)(avi_hdr.us_frame);
    fprintf(stdout, "  Frame Stats: Size=%ldx%ld  Frames=%ld  fps=%2.1f\n",
		avi->imagex,avi->imagey,avi_frame_cnt,fps);
  }
/* POD IMPROVE THIS to detect whether support or just plain none */
  if (avi_frame_cnt == 0)
  { 
    if (xa_verbose) 
	fprintf(stdout,"  Warning: No supported Video frames found.\n");
    return(xaFALSE);
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
  if (xa_buffer_flag == xaFALSE) anim_hdr->anim_flags |= ANIM_SNG_BUF;
  if (xa_file_flag == xaTRUE) anim_hdr->anim_flags |= ANIM_USE_FILE;
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
  return(xaTRUE);
} /* end of read file */

xaULONG RIFF_Read_AVIH(avi,fin,size,avi_hdr)
XA_ANIM_SETUP *avi;
FILE *fin;
xaULONG size;
AVI_HDR *avi_hdr;
{
  if (size != 0x38)
  {
    fprintf(stderr,"avih: size not 56 size=%ld\n",size);
    return(xaFALSE);
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
    avi->vid_time =  (xaULONG)(ftime);
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
  return(xaTRUE);
}

xaULONG RIFF_Read_STRH(fin,size,strh_hdr)
FILE *fin;
xaULONG size;
AVI_STREAM_HDR *strh_hdr;
{
  xaULONG d,tsize;
 
  if (size < 0x24) 
	{fprintf(stderr,"strh: size < 36 size = %ld\n",size); return(xaFALSE);}
 
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
  return(xaTRUE);
}

/***********************************************
 *
 * Return:   xaFALSE on error.  xaTRUE on ok(supported or not)
 **************/
xaULONG RIFF_Read_VIDS(anim_hdr,avi,fin,size,vids_hdr)
XA_ANIM_HDR *anim_hdr;
XA_ANIM_SETUP *avi;
FILE *fin;
xaLONG size;
VIDS_HDR *vids_hdr;
{ xaULONG d,i,ctable_flag;
  xaLONG  codec_ret;
 
  if (size & 0x01) size++;
  ctable_flag		= xaTRUE;
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

  avi_codec_hdr.compression = avi->compression;
  avi_codec_hdr.x = avi->imagex;
  avi_codec_hdr.y = avi->imagey;
  avi_codec_hdr.depth = avi->depth;
  avi_codec_hdr.anim_hdr = (void *)anim_hdr;
  avi_codec_hdr.avi_ctab_flag = ctable_flag;

  /* Query to see if Video Compression is supported or not */
  { xaULONG q = 0;
    while(q < (AVI_QUERY_CNT) )
    {
      codec_ret = avi_query_func[q](&avi_codec_hdr);
      if (codec_ret == CODEC_SUPPORTED)
      { avi->imagex = avi_codec_hdr.x;
	avi->imagey = avi_codec_hdr.y;
	avi->compression = avi_codec_hdr.compression;
	ctable_flag = avi_codec_hdr.avi_ctab_flag;
        break;
      }
      else if (codec_ret == CODEC_UNSUPPORTED) break;
      q++;
    }
  }

  /*** Return False if Codec is Unknown or Not Supported */
  if (codec_ret != CODEC_SUPPORTED)
  { char tmpbuf[256];
    if (codec_ret == CODEC_UNKNOWN)
    { xaULONG ii,a[4],dd = avi->compression;
      for(ii=0; ii<4; ii++)
      { a[ii] = dd & 0xff;  dd >>= 8;
	if ((a[ii] < ' ') || (a[ii] > 'z')) a[ii] = '.';
      }
      sprintf(tmpbuf,"Unknown %c%c%c%c(%08lx)",
                                a[3],a[2],a[1],a[0],avi->compression);
      avi_codec_hdr.description = tmpbuf;
    }
    if (xa_verbose)
                fprintf(stderr,"  Video Codec: %s",avi_codec_hdr.description);
    else        fprintf(stderr,"AVI Video Codec: %s",avi_codec_hdr.description);
    fprintf(stderr," depth=%ld is unsupported by this version.\n",
								avi->depth);
    while(size > 0) { d = getc(fin); size--; }
    return(xaFALSE);
  }
  
  if (xa_verbose) fprintf(stdout,"  Video Codec: %s depth=%ld\n",
				avi_codec_hdr.description,avi->depth);

  /*** Read AVI Color Table if it's present */
  if ( (avi->depth <= 8) && (ctable_flag==xaTRUE) )
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
  else if (   (cmap_true_map_flag == xaFALSE) /* depth 16 and not true_map */
           || (xa_buffer_flag == xaFALSE) )
  {
     if (cmap_true_to_332 == xaTRUE)
             avi->chdr = CMAP_Create_332(avi->cmap,&avi->imagec);
     else    avi->chdr = CMAP_Create_Gray(avi->cmap,&avi->imagec);
  }
  if ( (avi->pic==0) && (xa_buffer_flag == xaTRUE))
  {
    avi->pic_size = avi->imagex * avi->imagey;
    if ( (cmap_true_map_flag == xaTRUE) && (avi->depth > 8) )
		avi->pic = (xaUBYTE *) malloc( 3 * avi->pic_size );
    else avi->pic = (xaUBYTE *) malloc( XA_PIC_SIZE(avi->pic_size) );
    if (avi->pic == 0) TheEnd1("AVI_Buffer_Action: malloc failed");
  }

  /************* Some Video Codecs have Header Extensions ***************/
  if ( (size > 0) && (avi_codec_hdr.avi_read_ext))
  { xaULONG ret = avi_codec_hdr.avi_read_ext(fin,anim_hdr);
    size -= ret;
  }

  while(size > 0) { d = getc(fin); size--; }
  return(xaTRUE);
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
xaLONG ck_size;
XA_ACTION *act;
{
  ACT_DLTA_HDR *dlta_hdr = 0;
  xaULONG d;

  if (ck_size & 0x01) ck_size++;
  if (ck_size == 0)  /* NOP wait frame */
  {
    act->type = ACT_NOP;
    act->data = 0;
    act->chdr = 0;
    AVI_Add_Frame( avi->vid_time, avi->vid_timelo, act);
    return(0);
  }

  if (xa_file_flag==xaTRUE)
  {
    dlta_hdr = (ACT_DLTA_HDR *) malloc(sizeof(ACT_DLTA_HDR));
    if (dlta_hdr == 0) TheEnd1("AVI vid dlta0: malloc failed");
    act->data = (xaUBYTE *)dlta_hdr;
    dlta_hdr->flags = ACT_SNGL_BUF;
    dlta_hdr->fpos  = ftell(fin);
    dlta_hdr->fsize = ck_size;
    fseek(fin,ck_size,1); /* move past this chunk */
    if (ck_size > avi->max_fvid_size) avi->max_fvid_size = ck_size;
  }
  else
  { xaLONG ret;
    d = ck_size + (sizeof(ACT_DLTA_HDR));
    dlta_hdr = (ACT_DLTA_HDR *) malloc( d );
    if (dlta_hdr == 0) TheEnd1("AVI vid dlta1: malloc failed");
    act->data = (xaUBYTE *)dlta_hdr;
    dlta_hdr->flags = ACT_SNGL_BUF | DLTA_DATA;
    dlta_hdr->fpos = 0; dlta_hdr->fsize = ck_size;
    ret = fread( dlta_hdr->data, ck_size, 1, fin);
    if (ret != 1) 
    {  fprintf(stderr,"AVI vid dlta: read failed\n"); 
       act->type = ACT_NOP;	act->data = 0;	act->chdr = 0;
       free(dlta_hdr);
       return(0);
    }
  }

  AVI_Add_Frame( avi->vid_time, avi->vid_timelo, act);
  dlta_hdr->xpos = dlta_hdr->ypos = 0;
  dlta_hdr->xsize = avi->imagex;
  dlta_hdr->ysize = avi->imagey;
  dlta_hdr->special = 0;
  dlta_hdr->xapi_rev = avi_codec_hdr.xapi_rev;
  dlta_hdr->delta = avi_codec_hdr.decoder;
  dlta_hdr->extra = avi_codec_hdr.extra;

  if (avi->compression == RIFF_MJPG)  /* Special Case */
  { if (avi_first_delta==0)
    { xaUBYTE *mjpg_tmp,*mjpg_buff = dlta_hdr->data;
      xaLONG  mjpg_size  = ck_size;
      avi_first_delta = 1;
      if (xa_file_flag==xaTRUE) /* read from file if necessary */
      { xaULONG ret;
	mjpg_tmp = mjpg_buff = (xaUBYTE *)malloc(ck_size);
	fseek(fin,dlta_hdr->fpos,0);
	ret = fread(mjpg_tmp, mjpg_size, 1, fin);
      }
      while(jpg_search_marker(0xdb,&mjpg_buff,&mjpg_size) == xaTRUE)
      { xaULONG len = (*mjpg_buff++) << 8;  len |= (*mjpg_buff++);
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
/* MOVE Elsewhere  PODPODNOTE
    default:
	AVI_UNSUPPORTED:
	fprintf(stderr,"AVI: unsupported comp ");
	AVI_Print_ID(stderr,avi->compression);
	fprintf(stderr," with depth %ld\n",avi->depth);
	return(0);
	break;
*/
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
{ register xaULONG _r,_g,_b; \
  _r = (color >> 10) & 0x1f; r = (_r << 3) | (_r >> 2); \
  _g = (color >>  5) & 0x1f; g = (_g << 3) | (_g >> 2); \
  _b =  color & 0x1f;        b = (_b << 3) | (_b >> 2); \
  if (xa_gamma_flag==xaTRUE) { r = xa_gamma_adj[r]>>8;    \
     g = xa_gamma_adj[g]>>8; b = xa_gamma_adj[b]>>8; } }


xaULONG
AVI_Decode_CRAM(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG special = dec_info->special;          void *extra = dec_info->extra;
  XA_CHDR *chdr = dec_info->chdr;
  xaULONG row_dec,exitflag,changed,block_cnt;
  xaULONG code0,code1;
  xaLONG x,y,min_x,max_x,min_y,max_y;
  xaUBYTE *dptr;

  changed = 0;
  max_x = max_y = 0;	min_x = imagex;	min_y = imagey;
  dptr = delta;
  row_dec = imagex + 3;
  x = 0;
  y = imagey - 1;
  exitflag = 0;
  block_cnt = ((imagex * imagey) >> 4) + 1;

  if (map_flag == xaTRUE)
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
	  { xaULONG skip = ((code1 - 0x84) << 8) + code0;
	    block_cnt -= (skip-1); while(skip--) AVI_BLOCK_INC(x,y,imagex);
	  }
	  else /* single block encoded */
	  {
	    if (code1 >= 0x90) /* 8 color quadrant encoding */
	    { xaULONG cA0,cA1,cB0,cB1;
	      xaULONG *i_ptr = (xaULONG *)(image + ((y * imagex + x) << 2) );
	      cB0 = (xaULONG)map[*dptr++];  cA0 = (xaULONG)map[*dptr++];
	      cB1 = (xaULONG)map[*dptr++];  cA1 = (xaULONG)map[*dptr++];
	      AVI_CRAM_C4(i_ptr,code0,cA0,cA1,cB0,cB1,row_dec); i_ptr -=row_dec;
	      cB0 = (xaULONG)map[*dptr++];  cA0 = (xaULONG)map[*dptr++];
	      cB1 = (xaULONG)map[*dptr++];  cA1 = (xaULONG)map[*dptr++];
	      AVI_CRAM_C4(i_ptr,code1,cA0,cA1,cB0,cB1,row_dec);
	    } else if (code1 < 0x80) /* 2 color encoding */
	    { register xaULONG clr_A,clr_B;
	      xaULONG *i_ptr = (xaULONG *)(image + ((y * imagex + x) << 2) );
	      clr_B = (xaULONG)map[*dptr++];   clr_A = (xaULONG)map[*dptr++];
	      AVI_CRAM_C2(i_ptr,code0,clr_A,clr_B,row_dec); i_ptr -= row_dec;
	      AVI_CRAM_C2(i_ptr,code1,clr_A,clr_B,row_dec);
	    }
	    else /* 1 color encoding */
	    { xaULONG clr = (xaULONG)map[code0]; 
	      xaULONG *i_ptr = (xaULONG *)(image + ((y * imagex + x) << 2) );
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
	  { xaULONG skip = ((code1 - 0x84) << 8) + code0;
	    block_cnt -= (skip-1); while(skip--) AVI_BLOCK_INC(x,y,imagex);
	  } else /* single block encoded */
	  {
	    if (code1 >= 0x90) /* 8 color quadrant encoding */
	    {
	      xaUSHORT cA0,cA1,cB0,cB1;
	      xaUSHORT *i_ptr = (xaUSHORT *)(image + ((y * imagex + x) << 1) );
	      cB0 = map[*dptr++];  cA0 = map[*dptr++];
	      cB1 = map[*dptr++];  cA1 = map[*dptr++];
	      AVI_CRAM_C4(i_ptr,code0,cA0,cA1,cB0,cB1,row_dec); i_ptr -=row_dec;
	      cB0 = map[*dptr++];  cA0 = map[*dptr++];
	      cB1 = map[*dptr++];  cA1 = map[*dptr++];
	      AVI_CRAM_C4(i_ptr,code1,cA0,cA1,cB0,cB1,row_dec);
	    } /* end of 8 color quadrant encoding */
	    else if (code1 < 0x80) /* 2 color encoding */
	    { xaUSHORT clr_A,clr_B;
	      xaUSHORT *i_ptr = (xaUSHORT *)(image + ((y * imagex + x) << 1) );
	      clr_B = (xaUSHORT)map[*dptr++];   clr_A = (xaUSHORT)map[*dptr++];
	      AVI_CRAM_C2(i_ptr,code0,clr_A,clr_B,row_dec); i_ptr -= row_dec;
	      AVI_CRAM_C2(i_ptr,code1,clr_A,clr_B,row_dec);
	    } /* end of 2 color */
	    else /* 1 color encoding */
	    { xaUSHORT clr = (xaUSHORT)map[code0];
	      xaUSHORT *i_ptr = (xaUSHORT *)(image + ((y * imagex + x) << 1) );
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
	  { xaULONG skip = ((code1 - 0x84) << 8) + code0;
	    block_cnt -= (skip-1); while(skip--) AVI_BLOCK_INC(x,y,imagex);
	  } else /* single block encoded */
	  { 
	    if (code1 >= 0x90) /* 8 color quadrant encoding */
	    { xaUBYTE cA0,cA1,cB0,cB1;
	      xaUBYTE *i_ptr = (xaUBYTE *)(image + y * imagex + x);
	      cB0 = (xaUBYTE)map[*dptr++];  cA0 = (xaUBYTE)map[*dptr++];
	      cB1 = (xaUBYTE)map[*dptr++];  cA1 = (xaUBYTE)map[*dptr++];
	      AVI_CRAM_C4(i_ptr,code0,cA0,cA1,cB0,cB1,row_dec); i_ptr -=row_dec;
	      cB0 = (xaUBYTE)map[*dptr++];  cA0 = (xaUBYTE)map[*dptr++];
	      cB1 = (xaUBYTE)map[*dptr++];  cA1 = (xaUBYTE)map[*dptr++];
	      AVI_CRAM_C4(i_ptr,code1,cA0,cA1,cB0,cB1,row_dec);
	    } 
	    else if (code1 < 0x80) /* 2 color encoding */
	    { xaUBYTE clr_A,clr_B;
	      xaUBYTE *i_ptr = (xaUBYTE *)(image + y * imagex + x);
	      clr_B = (xaUBYTE)map[*dptr++];   clr_A = (xaUBYTE)map[*dptr++];
	      AVI_CRAM_C2(i_ptr,code0,clr_A,clr_B,row_dec); i_ptr -= row_dec;
	      AVI_CRAM_C2(i_ptr,code1,clr_A,clr_B,row_dec);
	    }
	    else /* 1 color encoding */
	    { xaUBYTE clr = (xaUBYTE)map[code0];
	      xaUBYTE *i_ptr = (xaUBYTE *)(image + y * imagex + x);
	      AVI_CRAM_C1(i_ptr,clr,row_dec);
	    }
	    AVI_MIN_MAX_CHECK(x,y,min_x,max_x,min_y,max_y);
	    changed = 1; AVI_BLOCK_INC(x,y,imagex);
	  } /* end of single block */
	} /* end of not term code */
      } /* end of not while exit */
    } /* end of 1 bytes pixel */
  } /* end of map is xaTRUE */
  else
  {
      while(!exitflag)
      {
	code0 =  *dptr++;	code1 =  *dptr++;	block_cnt--;
	if ( ((code1==0) && (code0==0) && !block_cnt) || (y<0)) exitflag = 1;
	else
	{
	  if ((code1 >= 0x84) && (code1 <= 0x87)) /* skip */
	  { xaULONG skip = ((code1 - 0x84) << 8) + code0;
	    block_cnt -= (skip-1); while(skip--) AVI_BLOCK_INC(x,y,imagex);
	  } else /* single block encoded */
	  {
	    if (code1 >= 0x90) /* 8 color quadrant encoding */
	    {
	      xaUBYTE cA0,cA1,cB0,cB1;
	      xaUBYTE *i_ptr = (xaUBYTE *)(image + y * imagex + x);
	      cB0 = (xaUBYTE)*dptr++;  cA0 = (xaUBYTE)*dptr++;
	      cB1 = (xaUBYTE)*dptr++;  cA1 = (xaUBYTE)*dptr++;
	      AVI_CRAM_C4(i_ptr,code0,cA0,cA1,cB0,cB1,row_dec); i_ptr -=row_dec;
	      cB0 = (xaUBYTE)*dptr++;  cA0 = (xaUBYTE)*dptr++;
	      cB1 = (xaUBYTE)*dptr++;  cA1 = (xaUBYTE)*dptr++;
	      AVI_CRAM_C4(i_ptr,code1,cA0,cA1,cB0,cB1,row_dec);
	    } 
	    else if (code1 < 0x80) /* 2 color encoding */
	    { xaUBYTE clr_A,clr_B;
	      xaUBYTE *i_ptr = (xaUBYTE *)(image + y * imagex + x);
	      clr_B = (xaUBYTE)*dptr++;   clr_A = (xaUBYTE)*dptr++;
	      AVI_CRAM_C2(i_ptr,code0,clr_A,clr_B,row_dec); i_ptr -= row_dec;
	      AVI_CRAM_C2(i_ptr,code1,clr_A,clr_B,row_dec);
	    } /* end of 2 color */
	    else /* 1 color encoding */
	    {
	      xaUBYTE clr = (xaUBYTE)code0;
	      xaUBYTE *i_ptr = (xaUBYTE *)(image + y * imagex + x);
	      AVI_CRAM_C1(i_ptr,clr,row_dec);
	    }
	    AVI_MIN_MAX_CHECK(x,y,min_x,max_x,min_y,max_y);
	    changed = 1; AVI_BLOCK_INC(x,y,imagex);
	  } /* end of single block */
	} /* end of not term code */
      } /* end of not while exit */
  }
  if (xa_optimize_flag == xaTRUE)
  {
    if (changed) { dec_info->xs=min_x; dec_info->ys=min_y - 3;
			dec_info->xe=max_x + 4; dec_info->ye=max_y + 1; }
    else  { dec_info->xs = dec_info->ys = dec_info->xe = dec_info->ye = 0;
			return(ACT_DLTA_NOP); }
  }
  else { dec_info->xs = dec_info->ys = 0;
	 dec_info->xe = imagex; dec_info->ye = imagey; }
  if (map_flag) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}


/*
 * Routine to Decode an AVI RGB chunk
 * (i.e. just copy it into the image buffer)
 * courtesy of Julian Bradfield.
 */

xaULONG
AVI_Decode_RGB(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG special = dec_info->special;          void *extra = dec_info->extra;
  XA_CHDR *chdr = dec_info->chdr;
  xaULONG oddflag;
  xaUBYTE *dptr = delta;
  
  oddflag = imagex & 0x01;
  if (map_flag == xaTRUE)
  {
    if (x11_bytes_pixel == 4)
    { xaLONG x,y = imagey - 1;
      while ( y >= 0 )
      { xaULONG *i_ptr = (xaULONG *)(image + ((y * imagex)<<2) ); y--; 
        x = imagex; while(x--) *i_ptr++ = (xaULONG)map[*dptr++];
	if (oddflag) dptr++;
      }
    }
    else if (x11_bytes_pixel == 2)
    { xaLONG x,y = imagey - 1;
      while ( y >= 0 )
      { xaUSHORT *i_ptr = (xaUSHORT *)(image + ((y * imagex)<<1) ); y--; 
        x = imagex; while(x--) *i_ptr++ = (xaUSHORT)map[*dptr++];
	if (oddflag) dptr++;
      }
    }
    else /* (x11_bytes_pixel == 1) */
    { xaLONG x,y = imagey - 1;
      while ( y >= 0 )
      { xaUBYTE *i_ptr = (xaUBYTE *)(image + y * imagex); y--; 
        x = imagex; while(x--) *i_ptr++ = (xaUBYTE)map[*dptr++];
	if (oddflag) dptr++;
      }
    }
  } /* end of map is xaTRUE */
  else
  { xaLONG x,y = imagey - 1;
    while ( y >= 0 )
    { xaUBYTE *i_ptr = (xaUBYTE *)(image + y * imagex); y--; 
      x = imagex; while(x--) *i_ptr++ = (xaUBYTE)*dptr++;
	if (oddflag) dptr++;
    }
  }
  dec_info->xs = dec_info->ys = 0; dec_info->xe = imagex; dec_info->ye = imagey;
  if (map_flag) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}

/*******************
 *  RGB Decode Depth 24
 ******/
xaULONG
AVI_Decode_RGB24(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG special = dec_info->special;          void *extra = dec_info->extra;
  XA_CHDR *chdr = dec_info->chdr;
  xaUBYTE *dp = delta;
  xaULONG oddflag;
  xaULONG special_flag = special & 0x0001;

  if (chdr) { if (chdr->new_chdr) chdr = chdr->new_chdr; }
  
  oddflag = imagex & 0x01;

  if (special_flag)
  { xaLONG x,y = imagey - 1;
    while ( y >= 0 )
    { xaULONG *iptr = (xaULONG *)(image + (y * imagex * 3)); y--; 
      x = imagex;
      while(x--) { xaUBYTE r,g,b; r = *dp++; g = *dp++; b = *dp++;
       *iptr++ = r; *iptr++ = g; *iptr++ = b; }
      if (oddflag) dp++; /* pad to short words ness in RGB 8 but here? */
    }
  }
  else if ( (map_flag==xaFALSE) || (x11_bytes_pixel==1) )
  { xaLONG x,y = imagey - 1;
    while ( y >= 0 )
    { xaUBYTE *iptr = (xaUBYTE *)(image + y * imagex); y--; 
      x = imagex; 
      while(x--) 
      { xaULONG r,g,b; r = (xaULONG)*dp++; g = (xaULONG)*dp++; b = (xaULONG)*dp++;
        *iptr++ = (xaUBYTE)XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
      }
      if (oddflag) dp++;
    }
  }
  else if (x11_bytes_pixel==4)
  { xaLONG x,y = imagey - 1;
    while ( y >= 0 )
    { xaULONG *iptr = (xaULONG *)(image + ((y * imagex)<<2) ); y--; 
      x = imagex; 
      while(x--) 
      { xaULONG r,g,b; r = (xaULONG)*dp++; g = (xaULONG)*dp++; b = (xaULONG)*dp++;
        *iptr++ = (xaULONG)XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
      }
      if (oddflag) dp++;
    }
  }
  else if (x11_bytes_pixel==2)
  { xaLONG x,y = imagey - 1;
    while ( y >= 0 )
    { xaUSHORT *iptr = (xaUSHORT *)(image + ((y * imagex)<<2) ); y--; 
      x = imagex; 
      while(x--) 
      { xaULONG r,g,b; r = (xaULONG)*dp++; g = (xaULONG)*dp++; b = (xaULONG)*dp++;
        *iptr++ = (xaUSHORT)XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
      }
      if (oddflag) dp++;
    }
  }
  dec_info->xs = dec_info->ys = 0; dec_info->xe = imagex; dec_info->ye = imagey;
  if (map_flag) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}


void AVI_Print_ID(fout,id)
FILE *fout;
xaLONG id;
{
 fprintf(fout,"%c",     ((id >> 24) & 0xff)   );
 fprintf(fout,"%c",     ((id >> 16) & 0xff)   );
 fprintf(fout,"%c",     ((id >>  8) & 0xff)   );
 fprintf(fout,"%c(%lx)", (id        & 0xff),id);
}


xaULONG AVI_Get_Color(color,map_flag,map,chdr)
xaULONG color,map_flag,*map;
XA_CHDR *chdr;
{
  register xaULONG clr,ra,ga,ba,tr,tg,tb;
 
  ra = (color >> 10) & 0x1f;
  ga = (color >>  5) & 0x1f;
  ba =  color & 0x1f;
  tr = (ra << 3) | (ra >> 2);
  tg = (ga << 3) | (ga >> 2);
  tb = (ba << 3) | (ba >> 2);
  if (xa_gamma_flag==xaTRUE) { tr = xa_gamma_adj[tr]>>8;  
     tg = xa_gamma_adj[tg]>>8; tb = xa_gamma_adj[tb]>>8; }

 
  if (x11_display_type & XA_X11_TRUE) clr = X11_Get_True_Color(ra,ga,ba,5);
  else
  { 
    if ((cmap_color_func == 4) && (chdr))
    { register xaULONG cache_i = color & 0x7fff;
      if (cmap_cache == 0) CMAP_Cache_Init(0);
      if (chdr != cmap_cache_chdr)
      {
	CMAP_Cache_Clear();
	cmap_cache_chdr = chdr;
      }
      if (cmap_cache[cache_i] == 0xffff)
      {
        clr = chdr->coff + 
	   CMAP_Find_Closest(chdr->cmap,chdr->csize,ra,ga,ba,5,5,5,xaTRUE);
        cmap_cache[cache_i] = (xaUSHORT)clr;
      }
      else clr = (xaULONG)cmap_cache[cache_i];
    }
    else
    {
      if (cmap_true_to_332 == xaTRUE) 
	  clr=CMAP_GET_332(ra,ga,ba,CMAP_SCALE5);
      else   clr = CMAP_GET_GRAY(ra,ga,ba,CMAP_SCALE10);
      if (map_flag) clr = map[clr];
    }
  }
  return(clr);
}


xaULONG
AVI_Decode_RLE8(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG special = dec_info->special;          void *extra = dec_info->extra;
  XA_CHDR *chdr = dec_info->chdr;
  xaULONG opcode,mod;
  xaLONG x,y,min_x,max_x,min_y,max_y;
  xaUBYTE *dptr;

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
        xaULONG yskip,xskip;
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
        if (map_flag==xaTRUE)
	{
	  if (x11_bytes_pixel==1)
          { xaUBYTE *iptr = (xaUBYTE *)(image + (y * imagex + x) );
            while(cnt--) 
	    { if (x >= imagex) { max_x = imagex; min_x = 0;
		 x -= imagex; y--; iptr = (xaUBYTE *)(image+y*imagex+x); }
              *iptr++ = (xaUBYTE)map[*dptr++];  x++;
	    }
	  }
	  else if (x11_bytes_pixel==2)
          { xaUSHORT *iptr = (xaUSHORT *)(image + ((y * imagex + x)<<1) );
            while(cnt--) 
	    { if (x >= imagex)  { max_x = imagex; min_x = 0;
		x -= imagex; y--; iptr = (xaUSHORT *)(image+y*imagex+x); }
              *iptr++ = (xaUSHORT)map[*dptr++];  x++;
	    }
	  }
	  else /* if (x11_bytes_pixel==4) */
          { xaULONG *iptr = (xaULONG *)(image + ((y * imagex + x)<<2) );
            while(cnt--) 
	    { if (x >= imagex)  { max_x = imagex; min_x = 0;
		x -= imagex; y--; iptr = (xaULONG *)(image+y*imagex+x); }
              *iptr++ = (xaULONG)map[*dptr++];  x++;
	    }
	  }
        }
        else
        { xaUBYTE *iptr = (xaUBYTE *)(image + (y * imagex + x) );
          while(cnt--) 
	  { if (x >= imagex)  { max_x = imagex; min_x = 0;
		x -=imagex; y--; iptr = (xaUBYTE *)(image+y*imagex+x); }
	    *iptr++ = (xaUBYTE)(*dptr++); x++;
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
      color = (map_flag==xaTRUE)?(map[opcode]):(opcode);
      if ( (map_flag==xaFALSE) || (x11_bytes_pixel==1) )
      { xaUBYTE *iptr = (xaUBYTE *)(image + (y * imagex + x) );
	xaUBYTE clr = (xaUBYTE)color;
	while(cnt--) 
	{ if (x >= imagex) { max_x = imagex; min_x = 0;
		x -=imagex; y--; iptr = (xaUBYTE *)(image+y*imagex+x); }
	  *iptr++ = clr; x++;
	}
      }
      else if (x11_bytes_pixel==2)
      { xaUSHORT *iptr = (xaUSHORT *)(image + ((y * imagex + x)<<1) );
	xaUSHORT clr = (xaUSHORT)color;
	while(cnt--) 
	{ if (x >= imagex)  { max_x = imagex; min_x = 0;
		x -=imagex; y--; iptr = (xaUSHORT *)(image+y*imagex+x); }
	  *iptr++ = clr; x++;
	}
      }
      else /* if (x11_bytes_pixel==4) */
      { xaULONG *iptr = (xaULONG *)(image + ((y * imagex + x)<<2) );
	xaULONG clr = (xaULONG)color;
	while(cnt--) 
	{ if (x >= imagex)  { max_x = imagex; min_x = 0;
		x -=imagex; y--; iptr = (xaULONG *)(image+y*imagex+x); }
	  *iptr++ = clr; x++;
	}
      }
      if (y < min_y) min_y = y; if (x > max_x) x = max_x;
    }
  } /* end of while */

  if (xa_optimize_flag == xaTRUE)
  {
    max_x++; if (max_x>imagex) max_x=imagex;
    max_y++; if (max_y>imagey) max_y=imagey;
    if ((min_x >= max_x) || (min_y >= max_y)) /* no change */
	{ dec_info->xs = dec_info->ys = dec_info->xe = dec_info->ye = 0; 
			return(ACT_DLTA_NOP); }
    else { dec_info->xs=min_x; dec_info->ys=min_y; 
			dec_info->xe=max_x; dec_info->ye=max_y; }
  }
  else { dec_info->xs = dec_info->ys = 0;
	 dec_info->xe = imagex; dec_info->ye = imagey; }
  if (map_flag) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}

/**************************************************
 * Decode Microsoft Video 1 Depth 16 Video Codec
 * XAPI Rev 0x0001
 *
 *************/
xaULONG AVI_Decode_CRAM16(image,delta,dsize,dec_info)
xaUBYTE *image;		/* Image Buffer. */
xaUBYTE *delta;		/* delta data. */
xaULONG dsize;		/* delta size */
XA_DEC_INFO *dec_info;	/* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;	xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;	xaULONG *map = dec_info->map;
  xaULONG special = dec_info->special;		void *extra = dec_info->extra;
  XA_CHDR *chdr = dec_info->chdr;
  xaULONG row_dec,exitflag,changed,block_cnt;
  xaULONG code0,code1;
  xaLONG x,y,min_x,max_x,min_y,max_y;
  xaUBYTE *dptr;

  if (chdr) { if (chdr->new_chdr) chdr = chdr->new_chdr; }

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
      { xaULONG skip = ((code1 - 0x84) << 8) + code0;
	block_cnt -= (skip-1); while(skip--) AVI_BLOCK_INC(x,y,imagex);
      }
      else /* not skip */
      { xaUBYTE *i_ptr = (xaUBYTE *)(image + 3 * (y * imagex + x) );
	if (code1 < 0x80) /* 2 or 8 color encoding */
	{ xaULONG cA,cB; xaUBYTE rA0,gA0,bA0,rB0,gB0,bB0;
	  AVI_GET_16(cB,dptr); AVI_Get_RGBColor(rB0,gB0,bB0,cB);
	  AVI_GET_16(cA,dptr); AVI_Get_RGBColor(rA0,gA0,bA0,cA); 
	  if (cB & 0x8000)   /* Eight Color Encoding */
	  { xaUBYTE rA1,gA1,bA1,rB1,gB1,bB1;
	    register xaULONG flag = code0;
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
	  { register xaULONG flag = code0;
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
	{ xaULONG cA = (code1<<8) | code0;
	  xaUBYTE r,g,b;
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
    if ( (x11_bytes_pixel == 1) || (map_flag == xaFALSE) )
    {
      while(!exitflag)
      {
	code0 =  *dptr++;	code1 =  *dptr++;	block_cnt--;
	if ( (code1==0) && (code0==0) && !block_cnt) { exitflag = 1; continue; }
	if (y < 0) {exitflag = 1; continue; }
	if ((code1 >= 0x84) && (code1 <= 0x87)) /* skip */
	{ xaULONG skip = ((code1 - 0x84) << 8) + code0;
	  block_cnt -= (skip-1); while(skip--) AVI_BLOCK_INC(x,y,imagex);
	}
	else /* not skip */
	{ xaUBYTE *i_ptr = (xaUBYTE *)(image + (y * imagex + x) );
	  if (code1 < 0x80) /* 2 or 8 color encoding */
	  { xaULONG cA,cB; xaUBYTE cA0,cB0;
	    AVI_GET_16(cB,dptr); AVI_GET_16(cA,dptr);
	    cB0 = (xaUBYTE)AVI_Get_Color(cB,map_flag,map,chdr);
	    cA0 = (xaUBYTE)AVI_Get_Color(cA,map_flag,map,chdr);
	    if (cB & 0x8000)   /* Eight Color Encoding */
	    { xaUBYTE cA1,cB1;
	      AVI_GET_16(cB,dptr); AVI_GET_16(cA,dptr);
	      cB1 = (xaUBYTE)AVI_Get_Color(cB,map_flag,map,chdr);
	      cA1 = (xaUBYTE)AVI_Get_Color(cA,map_flag,map,chdr);
	      AVI_CRAM_C4(i_ptr,code0,cA0,cA1,cB0,cB1,row_dec); i_ptr -=row_dec;
	      AVI_GET_16(cB,dptr); AVI_GET_16(cA,dptr);
	      cB0 = (xaUBYTE)AVI_Get_Color(cB,map_flag,map,chdr);
	      cA0 = (xaUBYTE)AVI_Get_Color(cA,map_flag,map,chdr);
	      AVI_GET_16(cB,dptr); AVI_GET_16(cA,dptr);
	      cB1 = (xaUBYTE)AVI_Get_Color(cB,map_flag,map,chdr);
	      cA1 = (xaUBYTE)AVI_Get_Color(cA,map_flag,map,chdr);
	      AVI_CRAM_C4(i_ptr,code1,cA0,cA1,cB0,cB1,row_dec);
	    } else /* Two Color Encoding */
	    { 
	      AVI_CRAM_C2(i_ptr,code0,cA0,cB0,row_dec); i_ptr -= row_dec;
	      AVI_CRAM_C2(i_ptr,code1,cA0,cB0,row_dec);
	    }
	  } /* end of 2 or 8 */
	  else /* 1 color encoding (80-83) && (>=88)*/
	  { xaULONG cA = (code1<<8) | code0;
	    xaUBYTE clr = (xaUBYTE)AVI_Get_Color(cA,map_flag,map,chdr);
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
	{ xaULONG skip = ((code1 - 0x84) << 8) + code0;
	  block_cnt -= (skip-1); while(skip--) AVI_BLOCK_INC(x,y,imagex);
	}
	else /* not skip */
	{ xaUSHORT *i_ptr = (xaUSHORT *)(image + ((y * imagex + x) << 1) );
	  if (code1 < 0x80) /* 2 or 8 color encoding */
	  { xaULONG cA,cB; xaUSHORT cA0,cB0;
	    AVI_GET_16(cB,dptr); AVI_GET_16(cA,dptr);
	    cB0 = (xaUSHORT)AVI_Get_Color(cB,map_flag,map,chdr);
	    cA0 = (xaUSHORT)AVI_Get_Color(cA,map_flag,map,chdr);
	    if (cB & 0x8000)   /* Eight Color Encoding */
	    { xaUSHORT cA1,cB1;
	      AVI_GET_16(cB,dptr); AVI_GET_16(cA,dptr);
	      cB1 = (xaUSHORT)AVI_Get_Color(cB,map_flag,map,chdr);
	      cA1 = (xaUSHORT)AVI_Get_Color(cA,map_flag,map,chdr);
	      AVI_CRAM_C4(i_ptr,code0,cA0,cA1,cB0,cB1,row_dec); i_ptr -=row_dec;
	      AVI_GET_16(cB,dptr); AVI_GET_16(cA,dptr);
	      cB0 = (xaUSHORT)AVI_Get_Color(cB,map_flag,map,chdr);
	      cA0 = (xaUSHORT)AVI_Get_Color(cA,map_flag,map,chdr);
	      AVI_GET_16(cB,dptr); AVI_GET_16(cA,dptr);
	      cB1 = (xaUSHORT)AVI_Get_Color(cB,map_flag,map,chdr);
	      cA1 = (xaUSHORT)AVI_Get_Color(cA,map_flag,map,chdr);
	      AVI_CRAM_C4(i_ptr,code1,cA0,cA1,cB0,cB1,row_dec);
	    } else /* Two Color Encoding */
	    { 
	      AVI_CRAM_C2(i_ptr,code0,cA0,cB0,row_dec); i_ptr -= row_dec;
	      AVI_CRAM_C2(i_ptr,code1,cA0,cB0,row_dec);
	    }
	  } /* end of 2 or 8 */
	  else /* 1 color encoding (80-83) && (>=88)*/
	  { xaULONG cA = (code1<<8) | code0;
	    xaUSHORT clr = (xaUSHORT)AVI_Get_Color(cA,map_flag,map,chdr);
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
	{ xaULONG skip = ((code1 - 0x84) << 8) + code0;
	  block_cnt -= (skip-1); while(skip--) AVI_BLOCK_INC(x,y,imagex);
	}
	else /* not skip */
	{ xaULONG *i_ptr = (xaULONG *)(image + ((y * imagex + x) << 2) );
	  if (code1 < 0x80) /* 2 or 8 color encoding */
	  { xaULONG cA,cB,cA0,cB0;
	    AVI_GET_16(cB,dptr); AVI_GET_16(cA,dptr);
	    cB0 = AVI_Get_Color(cB,map_flag,map,chdr);
	    cA0 = AVI_Get_Color(cA,map_flag,map,chdr);
	    if (cB & 0x8000)   /* Eight Color Encoding */
	    { xaULONG cA1,cB1;
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
	  { xaULONG cA = (code1<<8) | code0;
	    xaULONG clr = AVI_Get_Color(cA,map_flag,map,chdr);
	    AVI_CRAM_C1(i_ptr,clr,row_dec);
	  }
	  changed = 1; AVI_MIN_MAX_CHECK(x,y,min_x,max_x,min_y,max_y);
	  AVI_BLOCK_INC(x,y,imagex);
	} /* end of not skip */
      } /* end of not while exit */
    } /* end of 4 bytes pixel */
  } /* end of not special */
  if (xa_optimize_flag == xaTRUE)
  {
    if (changed) { 	dec_info->xs=min_x;	dec_info->ys=min_y - 3;
			dec_info->xe=max_x + 4;	dec_info->ye=max_y + 1; }
    else  { dec_info->xs = dec_info->ys = dec_info->xe = dec_info->ye = 0;
						return(ACT_DLTA_NOP); }
  }
  else { dec_info->xs = dec_info->ys = 0; 
	 dec_info->xe = imagex; dec_info->ye = imagey; }
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

xaULONG
AVI_Decode_ULTI(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG special = dec_info->special;          void *extra = dec_info->extra;
  XA_CHDR *chdr = dec_info->chdr;
  xaULONG r_inc,exitflag,changed;
  xaLONG block_cnt;
  xaLONG x,y,min_x,max_x,min_y,max_y;
  xaUBYTE *dptr;
  xaULONG stream_mode,chrom_mode,chrom_next_uniq;
  xaULONG bhedr,opcode,chrom;

  stream_mode = ULTI_STREAM0;
  chrom_mode = ULTI_CHROM_NORM;
  chrom_next_uniq = xaFALSE;
  if (chdr) { if (chdr->new_chdr) chdr = chdr->new_chdr; }
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
	  { xaULONG d;
	    d = *dptr++;
	    if (d==0) stream_mode = ULTI_STREAM0;
	    else if (d==1) stream_mode = ULTI_STREAM1;
	    else { fprintf(stderr,"ULTI: stream err %ld\n",stream_mode);
	           TheEnd(); }
	  }
	  break;
	case 0x71: /* next blk has uniq chrom */
	  chrom_next_uniq = xaTRUE;
	  break;
	case 0x72: /* chrom mode toggle */
	  chrom_mode = (chrom_mode==ULTI_CHROM_NORM)?(ULTI_CHROM_UNIQ)
						    :(ULTI_CHROM_NORM); 
	  break;
	case 0x73: /* Frame Guard */
	  exitflag = 1; 
	  break;
	case 0x74: /* Skip */
	  { xaULONG cnt = (xaULONG)(*dptr++);
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
    { xaULONG chrom_flag;
      xaULONG quadrant,msh;

      block_cnt--;
      if ( (chrom_mode==ULTI_CHROM_UNIQ) || (chrom_next_uniq == xaTRUE) )
      { 
	chrom_next_uniq = xaFALSE;
	chrom_flag = xaTRUE;
	chrom = 0; 
      }
      else 
      { 
	chrom_flag = xaFALSE; 
	if (bhedr != 0x00) chrom = *dptr++; /* read chrom */
      }
      msh = 8;
      for(quadrant=0;quadrant<4;quadrant++)
      { xaULONG tx,ty;
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
	    xaULONG angle,y0,y1;
	    if (chrom_flag==xaTRUE) { chrom = *dptr++; }
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
	  { xaULONG angle,ltc_idx,y0,y1,y2,y3;
	    xaUBYTE *tmp;
	    if (chrom_flag==xaTRUE) { chrom = *dptr++; }
	    ltc_idx = (*dptr++) << 8; ltc_idx |= (*dptr++);
	    angle = (ltc_idx >> 12) & 0x0f; /* 4 bits angle */
	    ltc_idx &= 0x0fff; /* index to 4 byte lum table */
	    tmp = &avi_ulti_tab[ ( ltc_idx << 2 ) ];
	    y0 = (xaULONG)(*tmp++); y1 = (xaULONG)(*tmp++); 
	    y2 = (xaULONG)(*tmp++); y3 = (xaULONG)(*tmp++);
	    AVI_ULTI_LTC(image,tx,ty,imagex,special,map_flag,map,chdr,
					y0,y1,y2,y3,chrom,angle);
	  }
	  break;

	  case 0x03:  /* Statistical/extended LTC */
	  { xaULONG d;
	    if (chrom_flag==xaTRUE) { chrom = *dptr++; }
	    d = *dptr++;
	    if (d & 0x80) /* extend LTC */
	    { xaULONG angle,y0,y1,y2,y3;
	      angle = (d >> 4) & 0x07; /* 3 bits angle */
	      d =  (d << 8) | (*dptr++);
	      y0 = (d >> 6) & 0x3f;    y1 = d & 0x3f;
	      y2 = (*dptr++) & 0x3f;   y3 = (*dptr++) & 0x3f;
	      AVI_ULTI_LTC(image,tx,ty,imagex,special,map_flag,map,chdr,
	 					y0,y1,y2,y3,chrom,angle);
	    }
	    else /* Statistical pattern */
	    { xaULONG y0,y1; xaUBYTE flag0,flag1;
	      flag0 = *dptr++;  flag1 = (xaUBYTE)d;
	      y0 = (*dptr++) & 0x3f;   y1 = (*dptr++) & 0x3f;
	      /* bit 0 => y0, bit 1 = > y1. */
	      /* raster scan order MSB = 0 */
	      if (special)
	      { xaUBYTE r0,r1,g0,g1,b0,b1;
		xaUBYTE *ip = (xaUBYTE *)(image + 3 * (ty * imagex + tx) );
	        AVI_Get_Ulti_rgbColor(y0,chrom,&r0,&g0,&b0);
	        AVI_Get_Ulti_rgbColor(y1,chrom,&r1,&g1,&b1);
	        AVI_ULTI_rgbC2(ip,flag1,r0,r1,g0,g1,b0,b1,r_inc); 
		ip += r_inc;
	        AVI_ULTI_rgbC2(ip,flag0,r0,r1,g0,g1,b0,b1,r_inc); 
	      }
	      else 
	      { xaULONG c0,c1;
	        c0 = AVI_Get_Ulti_Color(y0,chrom,map_flag,map,chdr);
	        c1 = AVI_Get_Ulti_Color(y1,chrom,map_flag,map,chdr);
	        if ( (x11_bytes_pixel==1) || (map_flag==xaFALSE) )
	        { xaUBYTE *ip = (xaUBYTE *)(image + (ty * imagex + tx) );
	          AVI_ULTI_C2(ip,flag1,xaUBYTE,c0,c1,r_inc); ip += r_inc;
	          AVI_ULTI_C2(ip,flag0,xaUBYTE,c0,c1,r_inc);
		}
	        else if (x11_bytes_pixel==4)
	        { xaULONG *ip = (xaULONG *)(image + ((ty * imagex + tx)<<2) );
	          AVI_ULTI_C2(ip,flag1,xaULONG,c0,c1,r_inc); ip += r_inc;
	          AVI_ULTI_C2(ip,flag0,xaULONG,c0,c1,r_inc);
		}
	        else /* (x11_bytes_pixel==2) */
	        { xaUSHORT *ip = (xaUSHORT *)(image + ((ty * imagex + tx)<<1) );
	          AVI_ULTI_C2(ip,flag1,xaUSHORT,c0,c1,r_inc); ip += r_inc;
	          AVI_ULTI_C2(ip,flag0,xaUSHORT,c0,c1,r_inc);
		}
	      }
	    }
	  }
	  break;

	  case 0x06:  /* Subsampled 4-luminance quadrant */
	  { xaULONG y0,y1,y2,y3;
	    if (chrom_flag==xaTRUE) { chrom = *dptr++; }
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
	  { xaULONG i,d,y[16];
	    if (chrom_flag==xaTRUE) { chrom = *dptr++; }
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
	    { xaUBYTE r,g,b,*ip = (xaUBYTE *)(image + 3 * (ty * imagex + tx) );
	      for(i=0;i<16;i++)
	      { AVI_Get_Ulti_rgbColor(y[i],chrom,&r,&g,&b);
		*ip++ = r; *ip++ = g; *ip++ = b; if ( (i%4)==3) ip += r_inc;
	      }
	    }
	    else if ( (x11_bytes_pixel==1) || (map_flag==xaFALSE) )
	    { xaUBYTE c,*ip = (xaUBYTE *)(image + (ty * imagex + tx) );
	      for(i=0;i<16;i++)
	      { c = (xaUBYTE)AVI_Get_Ulti_Color(y[i],chrom,map_flag,map,chdr);
		*ip++ = c; if ( (i%4)==3) ip += r_inc; }
	    }
	    else if (x11_bytes_pixel==4)
	    { xaULONG c,*ip = (xaULONG *)(image + ((ty * imagex + tx)<<2) );
	      for(i=0;i<16;i++)
	      { c = (xaULONG)AVI_Get_Ulti_Color(y[i],chrom,map_flag,map,chdr);
		*ip++ = c; if ( (i%4)==3) ip += r_inc; }
	    }
	    else /* if (x11_bytes_pixel==2) */
	    { xaUSHORT c,*ip = (xaUSHORT *)(image + ((ty * imagex + tx)<<1) );
	      for(i=0;i<16;i++)
	      { c = (xaUSHORT)AVI_Get_Ulti_Color(y[i],chrom,map_flag,map,chdr);
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
  { dec_info->xs = dec_info->ys = 0;
    dec_info->xe = imagex; dec_info->ye = imagey; }
  if (map_flag) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}


void AVI_ULTI_Gen_YUV()
{
  float r0,r1,b0,b1;
  xaLONG i;

  r0 = 16384.0 *  1.40200;
  b0 = 16384.0 *  1.77200;
  r1 = 16384.0 * -0.71414;
  b1 = 16384.0 * -0.34414;

  for(i=0;i<16;i++)
  { xaLONG tmp; float cr,cb;
    tmp = (i & 0x0f); 
    cr = 63.0 * ( ((float)(tmp) - 5.0) / 40.0);  
    cb = 63.0 * ( ((float)(tmp) - 6.0) / 34.0);
    ulti_Cr[i] = (xaLONG)(r0 * (float)(cr) );
    ulti_Cb[i] = (xaLONG)(b0 * (float)(cb) );
  }
  for(i=0;i<256;i++)
  { xaLONG tmp; float cr,cb;
    tmp = (i & 0x0f); 
    cr = 63.0 * ( ((float)(tmp) - 5.0) / 40.0);  
    tmp = ( (i>>4) & 0x0f); 
    cb = 63.0 * ( ((float)(tmp) - 6.0) / 34.0);
    ulti_CrCb[i] = (xaLONG)(b1 * cb + r1 * cr); 
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
xaLONG lum;
xaULONG chrom;
xaUBYTE *r,*g,*b;
{ xaULONG cr,cb,ra,ga,ba;
  xaLONG tmp;
  if (cmap_true_to_gray == xaTRUE) { ra = ba = ga = lum; }
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
  *r = (xaUBYTE)( (ra << 2) | (ra >> 4) );
  *g = (xaUBYTE)( (ga << 2) | (ga >> 4) );
  *b = (xaUBYTE)( (ba << 2) | (ba >> 4) );
}

xaULONG AVI_Get_Ulti_Color(lum,chrom,map_flag,map,chdr)
xaLONG lum;		/* 6 bits of lum */
xaULONG chrom;		/* 8 bits of chrom */
xaULONG map_flag,*map;
XA_CHDR *chdr;
{
  register xaULONG cr,cb,clr,ra,ga,ba,tr,tg,tb;
  xaLONG tmp;

  if (cmap_true_to_gray == xaTRUE) { ra = ba = ga = lum; }
  else
  { xaLONG lum1 = lum << 14;
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
  if (xa_gamma_flag==xaTRUE) { tr = xa_gamma_adj[tr]>>8;  
     tg = xa_gamma_adj[tg]>>8; tb = xa_gamma_adj[tb]>>8; }
 
  if (x11_display_type & XA_X11_TRUE) clr = X11_Get_True_Color(ra,ga,ba,5);
  else
  { 
    if ((cmap_color_func == 4) && (chdr))
    { register xaULONG cache_i = ((lum << 8) | chrom) & 0x3fff;
      if (cmap_cache == 0) CMAP_Cache_Init(0);
      if (chdr != cmap_cache_chdr)
      {
	CMAP_Cache_Clear();
	cmap_cache_chdr = chdr;
      }
      if (cmap_cache[cache_i] == 0xffff)
      {
        clr = chdr->coff + 
	   CMAP_Find_Closest(chdr->cmap,chdr->csize,ra,ga,ba,6,6,6,xaTRUE);
        cmap_cache[cache_i] = (xaUSHORT)clr;
      }
      else clr = (xaULONG)cmap_cache[cache_i];
    }
    else
    {
      if (cmap_true_to_332 == xaTRUE) 
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
xaUBYTE *image;
xaULONG x,y,imagex,special,map_flag,*map;
XA_CHDR *chdr;
xaULONG y0,y1,y2,y3,chrom,angle;
{ xaULONG r_inc;

  if (special)
  { xaULONG i;
    xaUBYTE *ip = (xaUBYTE *)(image + 3 * (y * imagex + x) );
    xaUBYTE r[4],g[4],b[4],*ix,idx[16];
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
    {   case 0: AVI_ULTI_0000(ix,xaUBYTE,0,1,2,3,1); break;
	case 1: AVI_ULTI_0225(ix,xaUBYTE,0,1,2,3,1); break;
	case 2: AVI_ULTI_0450(ix,xaUBYTE,0,1,2,3,1); break;
	case 3: AVI_ULTI_0675(ix,xaUBYTE,0,1,2,3,1); break;
	case 4: AVI_ULTI_0900(ix,xaUBYTE,0,1,2,3,1); break;
	case 5: AVI_ULTI_1125(ix,xaUBYTE,0,1,2,3,1); break;
	case 6: AVI_ULTI_1350(ix,xaUBYTE,0,1,2,3,1); break;
	case 7: AVI_ULTI_1575(ix,xaUBYTE,0,1,2,3,1); break;
	default: AVI_ULTI_C4(ix,xaUBYTE,0,1,2,3,1); break;
    } /* end switch */
    for(i=0;i<16;i++)
    { register xaULONG j = idx[i];
      *ip++ = r[j]; *ip++ = g[j]; *ip++ = b[j];
      if ( (i & 3) == 3 ) ip += r_inc;
    }
  }
  else 
  { xaULONG c0,c1,c2,c3;
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

    if ( (x11_bytes_pixel == 1) || (map_flag == xaFALSE) )
    { xaUBYTE *ip = (xaUBYTE *)(image + (y * imagex + x) );
      switch(angle)
      { case 0: AVI_ULTI_0000(ip,xaUBYTE,c0,c1,c2,c3,r_inc); break;
	case 1: AVI_ULTI_0225(ip,xaUBYTE,c0,c1,c2,c3,r_inc); break;
	case 2: AVI_ULTI_0450(ip,xaUBYTE,c0,c1,c2,c3,r_inc); break;
	case 3: AVI_ULTI_0675(ip,xaUBYTE,c0,c1,c2,c3,r_inc); break;
	case 4: AVI_ULTI_0900(ip,xaUBYTE,c0,c1,c2,c3,r_inc); break;
	case 5: AVI_ULTI_1125(ip,xaUBYTE,c0,c1,c2,c3,r_inc); break;
	case 6: AVI_ULTI_1350(ip,xaUBYTE,c0,c1,c2,c3,r_inc); break;
	case 7: AVI_ULTI_1575(ip,xaUBYTE,c0,c1,c2,c3,r_inc); break;
	default: AVI_ULTI_C4(ip,xaUBYTE,c0,c1,c2,c3,r_inc); break;
      }
    }
    else if (x11_bytes_pixel == 4)
    { xaULONG *ip = (xaULONG *)(image + ((y * imagex + x) << 2) );
      switch(angle)
      { case 0: AVI_ULTI_0000(ip,xaULONG,c0,c1,c2,c3,r_inc); break;
	case 1: AVI_ULTI_0225(ip,xaULONG,c0,c1,c2,c3,r_inc); break;
	case 2: AVI_ULTI_0450(ip,xaULONG,c0,c1,c2,c3,r_inc); break;
	case 3: AVI_ULTI_0675(ip,xaULONG,c0,c1,c2,c3,r_inc); break;
	case 4: AVI_ULTI_0900(ip,xaULONG,c0,c1,c2,c3,r_inc); break;
	case 5: AVI_ULTI_1125(ip,xaULONG,c0,c1,c2,c3,r_inc); break;
	case 6: AVI_ULTI_1350(ip,xaULONG,c0,c1,c2,c3,r_inc); break;
	case 7: AVI_ULTI_1575(ip,xaULONG,c0,c1,c2,c3,r_inc); break;
	default: AVI_ULTI_C4(ip,xaULONG,c0,c1,c2,c3,r_inc); break;
      }
    }
    else /* if (x11_bytes_pixel == 2) */
    { xaUSHORT *ip = (xaUSHORT *)(image + ( (y * imagex + x) << 1) );
      switch(angle)
      { case 0: AVI_ULTI_0000(ip,xaUSHORT,c0,c1,c2,c3,r_inc); break;
	case 1: AVI_ULTI_0225(ip,xaUSHORT,c0,c1,c2,c3,r_inc); break;
	case 2: AVI_ULTI_0450(ip,xaUSHORT,c0,c1,c2,c3,r_inc); break;
	case 3: AVI_ULTI_0675(ip,xaUSHORT,c0,c1,c2,c3,r_inc); break;
	case 4: AVI_ULTI_0900(ip,xaUSHORT,c0,c1,c2,c3,r_inc); break;
	case 5: AVI_ULTI_1125(ip,xaUSHORT,c0,c1,c2,c3,r_inc); break;
	case 6: AVI_ULTI_1350(ip,xaUSHORT,c0,c1,c2,c3,r_inc); break;
	case 7: AVI_ULTI_1575(ip,xaUSHORT,c0,c1,c2,c3,r_inc); break;
	default: AVI_ULTI_C4(ip,xaUSHORT,c0,c1,c2,c3,r_inc); break;
      }
    } /* end of shorts */
  } /* end of not special */
} /* end */


xaULONG RIFF_Read_AUDS(fin,size,auds_hdr)
FILE *fin;
xaLONG size;
AUDS_HDR *auds_hdr;
{ xaULONG ret = xaTRUE;
  auds_hdr->format	= UTIL_Get_LSB_Short(fin);
  auds_hdr->channels	= UTIL_Get_LSB_Short(fin);
  auds_hdr->rate	= UTIL_Get_LSB_Long(fin);
  auds_hdr->av_bps	= UTIL_Get_LSB_Long(fin);
  auds_hdr->align	= UTIL_Get_LSB_Short(fin);

  if (size & 0x01) size++;
  if (size >= 0x10) { auds_hdr->size = UTIL_Get_LSB_Short(fin); size -= 0x10; }
  else  { auds_hdr->size  = 8; size -= 0x0e; }


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
    ret = xaFALSE;
  }
  avi_audio_freq  = auds_hdr->rate;
  avi_audio_chans = auds_hdr->channels;
  if (auds_hdr->size == 8) avi_audio_bps = 1;
  else if (auds_hdr->size == 16) avi_audio_bps = 2;
  else if (auds_hdr->size == 32) avi_audio_bps = 4;
  else if (auds_hdr->size == 4) avi_audio_bps = 1;
  else avi_audio_bps = 1000 + auds_hdr->size;
  avi_audio_end   = 0;
  if (avi_audio_chans > 2) ret = xaFALSE;
  if (avi_audio_bps > 2) ret = xaFALSE;

  if (xa_verbose)
  {
    fprintf(stderr,"  Audio Codec: "); AVI_Audio_Type(auds_hdr->format);
    fprintf(stderr," Rate=%ld Chans=%ld bps=%ld\n",
			avi_audio_freq,avi_audio_chans,auds_hdr->size);
     
  }
/* modify type */
  if (avi_audio_chans==2)	avi_audio_type |= XA_AUDIO_STEREO_MSK;
  if (avi_audio_bps==2)		avi_audio_type |= XA_AUDIO_BPS_2_MSK;
  DEBUG_LEVEL2 fprintf(stderr,"size = %ld ret = %lx\n",size,ret);
  while(size > 0) { fgetc(fin); size--; }
  return(ret);
}

void AVI_Audio_Type(type)
xaULONG type;
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

static xaUBYTE avi_ulti_lin[11] = {2,3,5,6,7,8,11,14,17,20,255};
static xaUBYTE avi_ulti_lo[15]  = {4,5,6,7,8,11,14,17,20,23,26,29,32,36,255};
static xaUBYTE avi_ulti_hi[14]  = {6,8,11,14,17,20,23,26,29,32,35,40,46,255};

xaULONG AVI_Ulti_Check(val,ptr)
xaULONG val;
xaUBYTE *ptr;
{ 
  while(*ptr != 255) if ((xaULONG)(*ptr++) == val) return(1);
  return(0);
}

void AVI_Ulti_Gen_LTC()
{ xaLONG ys,ye,ydelta;
  xaUBYTE *utab;
  if (avi_ulti_tab==0) avi_ulti_tab = (xaUBYTE *)malloc(16384 * sizeof(xaUBYTE));
  else return;
  if (avi_ulti_tab==0) TheEnd1("AVI_Ulti_Gen_LTC: malloc err");
  utab = avi_ulti_tab;
  for(ys=0; ys <= 63; ys++)
  {
    for(ye=ys; ye <= 63; ye++)
    {
      ydelta = ye - ys;
      if (AVI_Ulti_Check(ydelta,avi_ulti_lin))
	{ xaULONG yinc = (ydelta + 1) / 3;
	  *utab++ = ys; *utab++ = ys + yinc; *utab++ = ye - yinc; *utab++ = ye;
	}
      if (AVI_Ulti_Check(ydelta,avi_ulti_lo)==1)
	{ xaLONG y1,y2;
          float yd = (float)(ydelta);
	  /* 1/4 */
	  y1 = ye - (xaLONG)(  (2 * yd - 5.0) / 10.0 );
	  y2 = ye - (xaLONG)(  (    yd - 5.0) / 10.0 );
	  *utab++ = ys; *utab++ = y1; *utab++ = y2; *utab++ = ye;
	  /* 1/2 */
	  y2 = ys + (xaLONG)(  (2 * yd + 5.0) / 10.0 );
	  *utab++ = ys; *utab++ = y2; *utab++ = y1; *utab++ = ye;
	  /* 3/4 */
	  y1 = ys + (xaLONG)(  (    yd + 5.0) / 10.0 );
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
{ xaULONG i;
  xaUBYTE *tmp = avi_ulti_tab;
  for(i=0;i<4096;i++)
  {
    fprintf(stderr,"%02lx %02lx %02lx %02lx\n",tmp[0],tmp[1],tmp[2],tmp[3]);
    tmp += 4;
  }
}
}


/* my shifted version */
static xaUBYTE xmpg_def_intra_qtab[64] = {
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
xaLONG xmpg_get_start_code(bufp,fin,buf_size)
xaUBYTE **bufp;
FILE *fin;
xaLONG *buf_size;
{ xaLONG d,size = *buf_size;
  xaULONG state = 0;
  xaULONG flag = 0;
  xaUBYTE *buf;

  if (bufp) {buf = *bufp; flag = 1;}
  while(size > 0)
  { if (flag) d = (xaLONG)((*buf++) & 0xff);
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
  return((xaLONG)(-1));
}


/* FROM xa_mpg.c */
extern xaUBYTE mpg_dlta_buf;
extern xaULONG mpg_dlta_size;

xaULONG xmpg_avi_once = 0;

xaULONG AVI_XMPG_00XM(avi,fin,ck_size,act,anim_hdr)
XA_ANIM_SETUP *avi;
FILE *fin;
xaLONG ck_size;
XA_ACTION *act;
XA_ANIM_HDR *anim_hdr;
{ MPG_SEQ_HDR *seq_hdr = 0;
  MPG_PIC_HDR *pic_hdr = 0;
  MPG_SLICE_HDR *slice = 0;
  ACT_DLTA_HDR *dlta_hdr = 0;
  XA_ACTION *tmp_act;
  xaLONG i,code;

  act->type = ACT_NOP; /* in case of failure */

  /* Allocate and fake out SEQ Header */
  tmp_act = ACT_Get_Action(anim_hdr,ACT_NOP);
  seq_hdr = (MPG_SEQ_HDR *)malloc(sizeof(MPG_SEQ_HDR));
  tmp_act->data = (xaUBYTE *)seq_hdr;
  tmp_act = 0;
  seq_hdr->width  = avi->imagex;
  seq_hdr->height = avi->imagey;
  for(i=0; i < 64;i++)
  { seq_hdr->intra_qtab[i] = xmpg_def_intra_qtab[i];  /* NO-ZIG */
  }

  { xaUBYTE *strd_buf = avi_strd;
  i = avi_strd_cursz; 
  if (strd_buf == 0) i = 0; /* redundant checking */
  while(i > 0) 
  {
    code = xmpg_get_start_code(&strd_buf,0,&i);
    if (code < 0) break;
    if (code == MPG_SEQ_START)
    { /* 12 w, 12 h, 4 aspect, 4 picrate, 18 bitrate etc */
      xaLONG d;
      xaULONG width,height,pic_rate,pic_size;
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
        { xaUBYTE *tmp_pic;
          if ( (cmap_true_map_flag == xaTRUE) && (avi->depth > 8) )
               tmp_pic = (xaUBYTE *)realloc( avi->pic, (3 * pic_size));
          else tmp_pic = (xaUBYTE *)realloc( avi->pic, (XA_PIC_SIZE(pic_size)) );
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
    code = xmpg_get_start_code( (xaUBYTE **)(0),fin,&ck_size);
    if (code < 0) 
	{fprintf(stderr,"XMPG: start err\n");  return(xaFALSE); }

    if (code == MPG_USR_START)
        { DEBUG_LEVEL1 fprintf(stderr,"USR START:\n"); }
    else if (code == MPG_EXT_START)
        { DEBUG_LEVEL1 fprintf(stderr,"EXT START:\n"); }
    else if (code == MPG_SEQ_END)
        { DEBUG_LEVEL1 fprintf(stderr,"SEQ END:\n"); }
    else if (code == MPG_PIC_START)
    { xaLONG d;
      DEBUG_LEVEL1 fprintf(stderr,"PIC START:\n");
      /* Allocate and fake out PIC Header */
      /* 10bits time  3bits type  16bits vbv_delay */

      /* NOTE: MPG_PIC_HDR includes 1 SLICE_HDR free of charge */
      dlta_hdr = (ACT_DLTA_HDR *)
		malloc( sizeof(ACT_DLTA_HDR) + sizeof(MPG_PIC_HDR) );
      if (dlta_hdr == 0) TheEnd1("AVI XMPG dlta malloc err");
      act->data = (xaUBYTE *)dlta_hdr;
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
	{ fprintf(stderr,"XMPG: no pic hdr\n"); return(xaFALSE); }

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


      if ( (xa_buffer_flag == xaFALSE) && (xa_file_flag == xaFALSE) )
      { xaUBYTE *tmp;
        tmp_act = ACT_Get_Action(anim_hdr,ACT_NOP);
        tmp = (xaUBYTE *)malloc( ck_size );
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
	{fprintf(stderr,"XMPG: failed before slice\n");  return(xaFALSE); }

  act->type = ACT_DELTA; /* in case of failure */
  return(xaTRUE);
}

/****************************
 *
 *
 ****************/
static xaLONG	AVI_Codec_Query(codec)
XA_CODEC_HDR *codec;
{ xaLONG ret = CODEC_UNKNOWN;	/* default */
  codec->extra = 0;
  codec->xapi_rev = 0x0001;
  codec->decoder = 0;
  codec->description = 0;
  codec->avi_read_ext = 0;

  switch(codec->compression)
  {
    case RIFF_RGB:  
    case RIFF_rgb:  
	codec->compression	= RIFF_RGB;
	codec->description	= "Microsoft RGB";
	ret = CODEC_SUPPORTED;
	if (codec->depth == 8)		codec->decoder = AVI_Decode_RGB;
	else if (codec->depth == 24)	codec->decoder = AVI_Decode_RGB24;
	else ret = CODEC_UNSUPPORTED;
	break;

    case RIFF_RLE8: 
    case RIFF_rle8: 
	codec->compression	= RIFF_RLE8;
	codec->description	= "Microsoft RLE8";
	ret = CODEC_SUPPORTED;
	if (codec->depth == 8)		codec->decoder = AVI_Decode_RLE8;
	else ret = CODEC_UNSUPPORTED;
	break;

    case RIFF_MSVC:
    case RIFF_CRAM:
	codec->compression	= RIFF_CRAM;
	codec->description	= "Microsoft Video 1";
	ret = CODEC_SUPPORTED;
        if (codec->depth == 8)		codec->decoder = AVI_Decode_CRAM;
	else if (codec->depth == 16)	codec->decoder = AVI_Decode_CRAM16;
	else ret = CODEC_UNSUPPORTED;
	codec->x = 4 * ((codec->x + 3)/4);
	codec->y = 4 * ((codec->y + 3)/4);
	break;

	/* POD NOTE: this needs special case later on. Work In?? */
    case RIFF_xmpg:
    case RIFF_XMPG:
	codec->compression	= RIFF_XMPG;
	codec->description	= "Editable MPEG";
	ret = CODEC_SUPPORTED;
	codec->decoder		= MPG_Decode_I;
	codec->x = 4 * ((codec->x + 3)/4);
	codec->y = 4 * ((codec->y + 3)/4);
	XA_Gen_YUV_Tabs(codec->anim_hdr);
	JPG_Alloc_MCU_Bufs(codec->anim_hdr,codec->x,codec->y,xaTRUE);
	JPG_Setup_Samp_Limit_Table(codec->anim_hdr);
	MPG_Init_Stuff(codec->anim_hdr);
	break;

    case RIFF_ulti:
    case RIFF_ULTI: 
	codec->compression	= RIFF_ULTI;
	codec->description	= "IBM Ultimotion";
	ret = CODEC_SUPPORTED;
	if (codec->depth == 16) 
	{
	  codec->decoder = AVI_Decode_ULTI;
	  codec->x = 8 * ((codec->x + 7)/8);
	  codec->y = 8 * ((codec->y + 7)/8);
	  AVI_ULTI_Gen_YUV();  /* generate tables POD NOTE: free chain 'em */
	  AVI_Ulti_Gen_LTC();
	}
	else ret = CODEC_UNSUPPORTED;
	break;


    case RIFF_jpeg: 
    case RIFF_JPEG:
	codec->compression	= RIFF_JPEG;
	codec->description	= "JFIF JPEG";
	ret = CODEC_SUPPORTED;
	if ( (codec->depth == 8) || (codec->depth == 24) )
	{
	  codec->avi_read_ext = AVI_JPEG_Read_Ext;
	  codec->decoder = JFIF_Decode_JPEG;
	  codec->x = 4 * ((codec->x + 3)/4);
	  codec->y = 2 * ((codec->y + 1)/2);
	  if (codec->depth > 8)	XA_Gen_YUV_Tabs(codec->anim_hdr);
	  else	codec->avi_ctab_flag = xaFALSE;
	  XA_Gen_YUV_Tabs(codec->anim_hdr);
	  JPG_Alloc_MCU_Bufs(codec->anim_hdr,codec->x,0,xaFALSE);
	  JPG_Setup_Samp_Limit_Table(codec->anim_hdr);
	}
	else ret = CODEC_UNSUPPORTED;
	break;

    case RIFF_IJPG: 
	codec->compression	= RIFF_IJPG;
	codec->description	= "Intergraph JPEG";
	ret = CODEC_SUPPORTED;
	if (codec->depth == 24)
	{
	  codec->avi_read_ext = AVI_IJPG_Read_Ext;
          codec->extra = (void *)(0x11);
          codec->decoder = JFIF_Decode_JPEG;
	  codec->x = 4 * ((codec->x + 3)/4);
	  codec->y = 2 * ((codec->y + 1)/2);
	  XA_Gen_YUV_Tabs(codec->anim_hdr);
	  JPG_Alloc_MCU_Bufs(codec->anim_hdr,codec->x,0,xaFALSE);
	}
	else ret = CODEC_UNSUPPORTED;
	break;

    case RIFF_MJPG:
	codec->compression	= RIFF_MJPG;
	codec->description	= "Motion JPEG";
	ret = CODEC_SUPPORTED;
	codec->avi_read_ext = AVI_JPEG_Read_Ext;
	codec->extra	= (void *)(0x00);  /* Changed Later */
        codec->decoder = JFIF_Decode_JPEG;
	codec->x = 4 * ((codec->x + 3)/4);
	codec->y = 2 * ((codec->y + 1)/2);
	if (codec->depth > 8)	XA_Gen_YUV_Tabs(codec->anim_hdr);
	else	codec->avi_ctab_flag = xaFALSE;
	JPG_Alloc_MCU_Bufs(codec->anim_hdr,codec->x,0,xaFALSE);
	break;
    default:
	ret = CODEC_UNKNOWN;
	break;
  }
  return(ret);
}

/****************************
 *
 ****************/
static xaLONG AVI_UNK_Codec_Query(codec)
XA_CODEC_HDR *codec;
{ xaLONG ret = CODEC_UNKNOWN;	/* default */
  codec->extra = 0;
  codec->xapi_rev = 0x0001;
  codec->decoder = 0;
  codec->description = 0;
  codec->avi_read_ext = 0;

  switch(codec->compression)
  {
    case RIFF_RLE4: 
    case RIFF_rle4: 
	codec->compression	= RIFF_RLE4;
	codec->description	= "Microsoft RLE4";
	ret = CODEC_UNSUPPORTED;
	break;
    case RIFF_cvid: 
    case RIFF_CVID: 
	codec->compression	= RIFF_CVID;
	codec->description	= "Radius Cinepak";
	ret = CODEC_UNSUPPORTED;
	break;
    case RIFF_rt21: 
    case RIFF_RT21: 
	codec->compression	= RIFF_RT21;
	codec->description	= "Intel Indeo R2.1";
	ret = CODEC_UNSUPPORTED;
	break;
    case RIFF_iv31: 
    case RIFF_IV31: 
	codec->compression	= RIFF_IV32;
	codec->description	= "Intel Indeo R3.1";
	ret = CODEC_UNSUPPORTED;
	break;
    case RIFF_iv32: 
    case RIFF_IV32: 
	codec->compression	= RIFF_IV32;
	codec->description	= "Intel Indeo R3.2";
	ret = CODEC_UNSUPPORTED;
	break;
    case RIFF_YVU9: 
    case RIFF_YUV9: 
	codec->compression	= RIFF_YUV9;
	codec->description	= "Intel Raw(YUV9)";
	ret = CODEC_UNSUPPORTED;
	break;

    default:
	break;
  }
  return(ret);
}

/****************************
 * Function for reading the JPEG and MJPG AVI header extension.
 *
 ****************/
static xaULONG AVI_JPEG_Read_Ext(fin,anim_hdr)
FILE *fin;
void *anim_hdr;
{ xaULONG offset, jsize, format, cspace, bits, hsubsamp,vsubsamp;
  offset   = UTIL_Get_LSB_Long(fin);
  jsize    = UTIL_Get_LSB_Long(fin);
  format   = UTIL_Get_LSB_Long(fin);
  cspace   = UTIL_Get_LSB_Long(fin);
  bits     = UTIL_Get_LSB_Long(fin);
  hsubsamp = UTIL_Get_LSB_Long(fin);
  vsubsamp = UTIL_Get_LSB_Long(fin);

  DEBUG_LEVEL1 
  {
    fprintf(stderr,"M/JPG: offset %lx jsize %lx format %lx ",
						offset,jsize,format);
    fprintf(stderr,"cspace %lx bits %lx hsub %lx vsub %lx\n", 
					cspace,bits,hsubsamp,vsubsamp);
  }
  return(28);  /* return number of bytes read */
}

/****************************
 * Function for reading the JPEG and MJPG AVI header extension.
 *
 ****************/
static xaULONG AVI_IJPG_Read_Ext(fin,anim_hdr)
FILE *fin;
void *anim_hdr;
{ xaULONG offset, jsize, format, cspace, ret_size;
  offset = UTIL_Get_LSB_Long(fin);
  jsize  = UTIL_Get_LSB_Long(fin);
  format = UTIL_Get_LSB_Long(fin);
  cspace = UTIL_Get_LSB_Long(fin);
  DEBUG_LEVEL1
  {
    fprintf(stderr,"IJPG: offset %lx jsize %lx format %lx cspace %lx\n",
						offset,jsize,format,cspace);
  }
  ret_size = 16;
  JFIF_Read_IJPG_Tables(fin);
  ret_size += 128;
  return(ret_size);
}

