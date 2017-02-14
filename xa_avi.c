
/*
 * xa_avi.c
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
 * 19Mar96  Modified for support of audio only files.
 * 28Mar96  Added RGB depth  4 support
 * 28Mar96  Added RGB depth 16 support
 * 31Mar96  ULTI: fixed problem with 16/24 bit displays(wrong colors)
 * 11Apr96  CRAM16: added some dithering support.
 *	97  RGB24: padding was wrong - now fixed.
 *	97  RLE8:  width not multiple of 4 broken - fixed.
 * 29Jan99  Added Broadway MPEG(BW10) Codec
 * 29Jan99  Rewrote Xings XMPG format.
 *
 ********************************/


#include "xa_avi.h" 
#include "xa_xmpg.h" 
#include "xa_codecs.h"
#include "xa_color.h"
#include "xa_cmap.h"


#ifdef XA_GSM
extern void GSM_Init();
#endif

xaLONG  AVI_Codec_Query();
xaLONG  AVI_UNK_Codec_Query();

extern xaLONG AVI_Video_Codec_Query();


static xaULONG AVI_IJPG_Read_Ext();
static xaULONG AVI_JPEG_Read_Ext();
xaULONG AVI_Read_File();
void AVI_Print_ID();
AVI_FRAME *AVI_Add_Frame();
void AVI_Free_Frame_List();
xaULONG RIFF_Read_AVIH();
xaULONG RIFF_Read_STRD();
xaULONG RIFF_Read_STRH();
xaULONG RIFF_Read_VIDS();
xaULONG RIFF_Read_AUDS();
void AVI_Print_Audio_Type();
xaULONG AVI_Get_Color();
void AVI_Get_RGBColor();
ACT_DLTA_HDR *AVI_Read_00DC();
void ACT_Setup_Delta();
xaULONG AVI_Stream_Chunk();
xaULONG AVI_Read_IDX1();

/* CODEC ROUTINES */
extern xaULONG JFIF_Decode_JPEG();
extern void JFIF_Read_IJPG_Tables();
static xaULONG AVI_Decode_RLE8();
extern xaULONG AVI_Decode_CRAM();
extern xaULONG AVI_Decode_CRAM16();
static xaULONG AVI_Decode_RGB4();
static xaULONG AVI_Decode_RGB8();
static xaULONG AVI_Decode_RGB16();
static xaULONG AVI_Decode_RGB24();
static xaULONG AVI_Decode_V422();
static xaULONG AVI_Decode_VYUY();
static xaULONG AVI_Decode_YUY2();
static xaULONG AVI_Decode_YV12();
static xaULONG AVI_Decode_I420();
static xaULONG AVI_Decode_Y41P();
#ifdef NO_CLUE_JUST_PUTZIN_AROUND
xaULONG AVI_Decode_VIXL();
#endif
/* xaULONG AVI_Decode_AURA16(); */
extern void QT_Create_Gray_Cmap();
extern xaULONG QT_Decode_RPZA();
extern xaULONG XA_RGB24_To_CLR32();
extern xaULONG XA_RGB16_To_CLR32();
xaULONG AVI_Decode_ULTI();
extern xaULONG MPG_Decode_FULLI();
extern void MPG_Init_Stuff();
extern xaULONG MPG_Setup_Delta();
extern void XA_Gen_YUV_Tabs();
extern void JPG_Alloc_MCU_Bufs();
extern void JPG_Setup_Samp_Limit_Table();
extern xaULONG jpg_search_marker();
static xaULONG AVI_Get_Ulti_Color();
static void AVI_Get_Ulti_rgbColor();
static void AVI_ULTI_Gen_YUV();
static void AVI_ULTI_LTC();
static void AVI_Ulti_Gen_LTC();
static xaULONG AVI_Ulti_Check();
static void AVI_XMPG_Kludge();

extern XA_ACTION *ACT_Get_Action();
extern XA_CHDR *ACT_Get_CMAP();
extern XA_CHDR *CMAP_Create_332();
extern XA_CHDR *CMAP_Create_422();
extern XA_CHDR *CMAP_Create_Gray();
extern void ACT_Add_CHDR_To_Action();
extern void ACT_Setup_Mapped();
extern void ACT_Get_CCMAP();
extern XA_CHDR *CMAP_Create_CHDR_From_True();
extern xaULONG CMAP_Find_Closest();
extern xaUBYTE *UTIL_RGB_To_FS_Map();
extern xaUBYTE *UTIL_RGB_To_Map();

extern XA_ANIM_SETUP *XA_Get_Anim_Setup();
extern void XA_Free_Anim_Setup();

extern void *XA_YUV211111_Func();
extern void *XA_YUV221111_Func();

static xaULONG avi_video_attempt;
/** AVI SOUND STUFF ****/
static xaULONG avi_audio_attempt;
static xaULONG avi_has_audio;
static xaULONG avi_has_video;
static xaULONG avi_audio_type;
static xaULONG avi_audio_freq;
static xaULONG avi_audio_chans;
static xaULONG avi_audio_bps;
static xaULONG avi_audio_end;
static AUDS_HDR auds_hdr;
extern xaULONG XA_Add_Sound();


extern xaLONG xa_dither_flag;
extern xaUBYTE  *xa_byte_limit;
extern YUVBufs jpg_YUVBufs;
extern YUVTabs def_yuv_tabs;


/* Currently used to check 1st frame for Microsoft MJPG screwup */
static xaULONG avi_first_delta;

static xaLONG ulti_Cr[16],ulti_Cb[16],ulti_CrCb[256];
static xaUBYTE *avi_ulti_tab = 0;


static AVI_HDR avi_hdr;
static AVI_STREAM_HDR strh_hdr;
static VIDS_HDR vids_hdr;
static xaULONG avi_use_index_flag;
static xaLONG avi_movi_offset;
static xaUBYTE *avi_strd;
static xaULONG avi_strd_size,avi_strd_cursz;

#define AVI_MAX_STREAMS 8
XA_CODEC_HDR avi_codec_hdr[AVI_MAX_STREAMS];
static xaULONG avi_stream_type[AVI_MAX_STREAMS];
static xaULONG avi_stream_ok[AVI_MAX_STREAMS];
static xaULONG avi_stream_cnt;

static xaULONG avi_frame_cnt;
static AVI_FRAME *avi_frame_start,*avi_frame_cur;

/****-----------------------------------------------------------------****
 *
 ****-----------------------------------------------------------------****/
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

/****-----------------------------------------------------------------****
 *
 ****-----------------------------------------------------------------****/
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

/****-----------------------------------------------------------------****
 *
 ****-----------------------------------------------------------------****/
xaULONG AVI_Read_File(fname,anim_hdr,audio_attempt)
char *fname;
XA_ANIM_HDR *anim_hdr;
xaULONG audio_attempt;	/* xaTRUE if audio is to be attempted */
{ XA_INPUT *xin;
  xaLONG i,t_time;
  xaULONG t_timelo;
  XA_ACTION *act;
  xaLONG avi_riff_size;
  XA_ANIM_SETUP *avi;
 
  xin = anim_hdr->xin;
  avi = XA_Get_Anim_Setup();
  avi->vid_time = XA_GET_TIME( 100 ); /* default */

  avi_strd		= 0;
  avi_strd_size		= avi_strd_cursz = 0;
  avi_frame_cnt		= 0;
  avi_frame_start	= 0;
  avi_frame_cur		= 0;
  avi_use_index_flag	= 0;
  avi_movi_offset	= 0;
  avi_audio_attempt	= audio_attempt;
  avi_video_attempt	= xaTRUE;
  avi_has_audio		= xaFALSE;
  avi_has_video		= xaFALSE;
  avi_riff_size		= 1;
  avi_first_delta	= 0;
  avi_stream_cnt	= 0;
  for(i=0; i < AVI_MAX_STREAMS; i++)
	{ avi_stream_type[i] = 0; avi_stream_ok[i] = xaFALSE; }

  xin->Set_EOF(xin,9);
  while( !xin->At_EOF(xin,8) && (avi_riff_size > 0) )
  {
    xaULONG d,ck_id,ck_size;

    ck_id = xin->Read_MSB_U32(xin);
    ck_size = xin->Read_LSB_U32(xin);
    avi_riff_size -= 8;
DEBUG_LEVEL2 
{
  fprintf(stdout,"AVI cid ");
  AVI_Print_ID(stdout,ck_id);
  fprintf(stdout,"  cksize %08x\n",ck_size);
}
    switch(ck_id)
    {
	case RIFF_RIFF:
		xin->Set_EOF(xin, (ck_size + 8));
		d = xin->Read_MSB_U32(xin);
		avi_riff_size = (2*ck_size) - 4;
		DEBUG_LEVEL2 
		{
			fprintf(stdout,"  RIFF form type ");
			AVI_Print_ID(stdout,d);
			fprintf(stdout,"\n");
		}
                break;
	case RIFF_LIST:
		d = xin->Read_MSB_U32(xin);
			/* Skip over movie list if using Index chunk */
		if ((d == RIFF_movi) && (avi_use_index_flag))
	        { xaLONG eof_pos;
		  avi_movi_offset = xin->Get_FPos(xin);
		  avi_movi_offset -= 4;  /* back over type */
			/* use to test for truncation */
		  xin->Seek_FPos(xin,0,2);
		  eof_pos = xin->Get_FPos(xin);

DEBUG_LEVEL2 fprintf(stdout,
		"movi LIST: eof %d movi_offset %d ck_size %d\n",
			eof_pos, avi_movi_offset, ck_size);

		  if (eof_pos < (avi_movi_offset + ck_size)) /* truncated */
		  { avi_use_index_flag = 0;
		    xin->Seek_FPos(xin,(avi_movi_offset + 4),0);
		    avi_riff_size += (ck_size - 4);
DEBUG_LEVEL2 fprintf(stdout,"TRUNCATED\n");
		  }
		  else
		  { if (ck_size & 1) ck_size++;
			/* skip over movi List */
		    xin->Seek_FPos(xin,(avi_movi_offset + ck_size),0);
		  }
		}
		else
		{ /* re-add list size minus size of type */
		  avi_riff_size += (ck_size - 4); /* don't count LISTs */ 
		}

		DEBUG_LEVEL2 
		{
			fprintf(stdout,"  List type ");
			AVI_Print_ID(stdout,d);
			fprintf(stdout,"\n");
		}
		break;
 
	case RIFF_avih:
		DEBUG_LEVEL2 fprintf(stdout,"  AVI_HDR:\n");
                if (RIFF_Read_AVIH(xin,avi,ck_size,&avi_hdr)==xaFALSE) return(xaFALSE);
                break;
 
	case RIFF_strh:
		DEBUG_LEVEL2 fprintf(stdout,"  STRH HDR:\n");
                if (RIFF_Read_STRH(xin,avi,ck_size,&strh_hdr)==xaFALSE) return(xaFALSE);
                break;

	case RIFF_strd:
		DEBUG_LEVEL2 fprintf(stdout,"  STRD HDR:\n");
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
		xin->Read_Block(xin,avi_strd,ck_size);

	 	if (avi_stream_cnt > 0)
		{ xaULONG stream_cnt = avi_stream_cnt - 1;
		  if (avi_codec_hdr[stream_cnt].compression == RIFF_XMPG)
		  {
		    AVI_XMPG_Kludge(avi,&vids_hdr,avi_strd,ck_size,stream_cnt);
		  }
		}
		break;
 
	case RIFF_strf:
	  DEBUG_LEVEL2 fprintf(stdout,"  STRF HDR:\n");
	  if  (    (avi_stream_cnt < AVI_MAX_STREAMS)
		&& (avi_stream_cnt < avi_hdr.streams) )
	  { xaULONG str_type,break_flag = xaFALSE;
	    str_type = avi_stream_type[avi_stream_cnt] = strh_hdr.fcc_type;
	    switch(str_type)
	    {
	      case RIFF_vids:
		avi_stream_ok[avi_stream_cnt] = 
			RIFF_Read_VIDS(xin,anim_hdr,avi,ck_size,&vids_hdr,
					avi_stream_cnt);
		break_flag = xaTRUE;
		break;
	      case RIFF_auds:
		{ xaULONG ret = RIFF_Read_AUDS(xin,ck_size,&auds_hdr);
		  if ( (avi_audio_attempt==xaTRUE) && (ret==xaFALSE))
		  {
		    fprintf(stdout,"  AVI Audio Type Unsupported\n");
		    avi_audio_attempt = xaFALSE;
		  }
		  avi_stream_ok[avi_stream_cnt] = avi_audio_attempt;
		}
		break_flag = xaTRUE;
		break;
	      case RIFF_pads:
		DEBUG_LEVEL1 fprintf(stdout,"AVI: STRH(pads) ignored\n");
		avi_stream_ok[avi_stream_cnt] = xaFALSE;
		break;
	      case RIFF_txts:
		DEBUG_LEVEL1 fprintf(stdout,"AVI: STRH(txts) ignored\n");
		avi_stream_ok[avi_stream_cnt] = xaFALSE;
		break;
	      default:
		fprintf(stdout,"unknown fcc_type at strf %08x ",str_type);
		fprintf(stdout,"- ignoring this stream.\n");
		avi_stream_ok[avi_stream_cnt] = xaFALSE;
	    }
	    avi_stream_cnt++;
	    if (break_flag) break;
	  }
	  /* ignore STRF chunk on higher streams or unsupport fcc_types */
	  if (ck_size & 0x01) ck_size++;
	  xin->Seek_FPos(xin,ck_size,1); /* move past this chunk */
	  break;
 

	case RIFF_01pc:
	case RIFF_00pc:
		{ xaULONG pc_firstcol,pc_numcols;
		  pc_firstcol = xin->Read_U8(xin);
		  pc_numcols  = xin->Read_U8(xin);
DEBUG_LEVEL2 fprintf(stdout,"00PC: 1st %d num %d\n",pc_firstcol,pc_numcols);
		  d = xin->Read_U8(xin);
		  d = xin->Read_U8(xin);
		  for(i = 0; i < pc_numcols; i++) 
		  {
		    avi->cmap[i + pc_firstcol].red   = xin->Read_U8(xin);
		    avi->cmap[i + pc_firstcol].green = xin->Read_U8(xin);
		    avi->cmap[i + pc_firstcol].blue  = xin->Read_U8(xin);
		    d = xin->Read_U8(xin);
		  }
		  act = ACT_Get_Action(anim_hdr,0);
		  AVI_Add_Frame(avi->vid_time,avi->vid_timelo,act);
		  avi->chdr = ACT_Get_CMAP(avi->cmap,avi->imagec,0,
							avi->imagec,0,8,8,8);
		  ACT_Add_CHDR_To_Action(act,avi->chdr);
		}
		break;


 
        case RIFF_idx1:
		if (avi_use_index_flag)
		{ 
                  if (ck_size & 0x01) ck_size++;
		  AVI_Read_IDX1(xin,anim_hdr,avi,ck_size,fname);
		}
		else
		{
                  if (ck_size & 0x01) ck_size++;
		  xin->Seek_FPos(xin,ck_size,1); /* move past this chunk */
		}
		break;

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
		xin->Seek_FPos(xin,ck_size,1); /* move past this chunk */
                break;
 
        default:
	  /* Check for EOF */
	  if (xin->At_EOF(xin,0))  break;	/* End of File */
	
	  /* Check if past end of RIFF Chunk */
	  if (avi_riff_size <= 0)
	  { fprintf(stdout,"  Past End of AVI File\n");
	    xin->Seek_FPos(xin,0,2); /* goto end of file */
	    break;
	  }

	  /*** Check if Stream chunk or handle other unknown chunks */
          if ( AVI_Stream_Chunk(xin,anim_hdr,avi,
				ck_id,ck_size,fname) == xaFALSE)
							return(xaFALSE);

	  break;

      } /* end of ck_id switch */
      /* reduce pessimism */
      avi_riff_size -= ck_size;
      if (ck_size & 0x01) avi_riff_size--; /* odd byte pad */
    } /* while not exitflag */

  if (avi->pic != 0) { FREE(avi->pic,0xA003); avi->pic=0; }
  xin->Close_File(xin);

  if (xa_verbose) 
  { float fps =  1000000.0 / (float)(avi_hdr.us_frame);
    fprintf(stdout, "  Frame Stats: Size=%dx%d  Frames=%d  fps=%2.1f\n",
		avi->imagex,avi->imagey,avi_frame_cnt,fps);
  }

  if (avi_frame_cnt == 0)
  {
    if (avi_has_video == xaTRUE)
    { if (avi_has_audio == xaFALSE)
      { fprintf(stdout,"  AVI Notice: No supported Video frames found.\n");
	return(xaFALSE); }
      else fprintf(stdout,"  AVI Notice: No supported Video frames - treating as audio only file\n");          
    }


    if (auds_hdr.blockalign == 0) auds_hdr.blockalign = 1;
	/* can easily blow out of 32 bits - hence the float */
    if (auds_hdr.rate)
        anim_hdr->total_time = (xaULONG)
            ((((float)auds_hdr.byte_cnt * (float)auds_hdr.samps_block * 1000.0)
                                        / (float)auds_hdr.blockalign)
                                        / (float)auds_hdr.rate);
    else  anim_hdr->total_time = 0;
/*
fprintf(stdout,"byte_cnt %d  sampblk %d  blkalign %d rate %d\n",
	auds_hdr.byte_cnt,auds_hdr.samps_block,
	auds_hdr.blockalign,auds_hdr.rate);
fprintf(stdout,"total time %d\n", anim_hdr->total_time);
*/
  }
  else
  {
    anim_hdr->frame_lst = (XA_FRAME *)
				malloc( sizeof(XA_FRAME) * (avi_frame_cnt+1));
    if (anim_hdr->frame_lst == NULL) TheEnd1("AVI_Read_File: frame malloc err");

    avi_frame_cur = avi_frame_start;
    i = 0;
    t_time = 0;
    t_timelo = 0;
    while(avi_frame_cur != 0)
    { if (i > avi_frame_cnt)
      { fprintf(stdout,"AVI_Read_Anim: frame inconsistency %d %d\n",
						i,avi_frame_cnt); break; }
      anim_hdr->frame_lst[i].time_dur = avi_frame_cur->time;
      anim_hdr->frame_lst[i].zztime = t_time;
      t_time += avi_frame_cur->time;
      t_timelo += avi_frame_cur->timelo;
      while(t_timelo > (1<<24)) {t_time++; t_timelo -= (1<<24);}
      anim_hdr->frame_lst[i].act = avi_frame_cur->act;
      avi_frame_cur = avi_frame_cur->next;
      i++;
    }
    if (i > 0) 
    { anim_hdr->last_frame = i - 1;
      anim_hdr->total_time = anim_hdr->frame_lst[i-1].zztime
				+ anim_hdr->frame_lst[i-1].time_dur;
    }
    else
    { anim_hdr->last_frame = 0;
      anim_hdr->total_time = 0;
    }
    AVI_Free_Frame_List(avi_frame_start);
    anim_hdr->imagex = avi->imagex;
    anim_hdr->imagey = avi->imagey;
    anim_hdr->imagec = avi->imagec;
    anim_hdr->imaged = 8; /* nop */
    anim_hdr->frame_lst[i].time_dur = 0;
    anim_hdr->frame_lst[i].zztime = -1;
    anim_hdr->frame_lst[i].act  = 0;
    anim_hdr->loop_frame = 0;
  } /* end of video present */

  if (xa_buffer_flag == xaFALSE) anim_hdr->anim_flags |= ANIM_SNG_BUF;
  if (xa_file_flag == xaTRUE) anim_hdr->anim_flags |= ANIM_USE_FILE;
  anim_hdr->max_fvid_size = avi->max_fvid_size;
  anim_hdr->max_faud_size = avi->max_faud_size;
  anim_hdr->fname = anim_hdr->name;
  XA_Free_Anim_Setup(avi);
  return(xaTRUE);
} /* end of read file */

xaULONG RIFF_Read_AVIH(xin,avi,size,avi_hdr)
XA_INPUT *xin;
XA_ANIM_SETUP *avi;
xaULONG size;
AVI_HDR *avi_hdr;
{
  if (size != 0x38)
  {
    fprintf(stdout,"avih: size not 56 size=%d\n",size);
    return(xaFALSE);
  }
 
  avi_hdr->us_frame     = xin->Read_LSB_U32(xin);
  avi_hdr->max_bps      = xin->Read_LSB_U32(xin);
  avi_hdr->pad_gran     = xin->Read_LSB_U32(xin);
  avi_hdr->flags        = xin->Read_LSB_U32(xin);
  avi_hdr->tot_frames   = xin->Read_LSB_U32(xin);
  avi_hdr->init_frames  = xin->Read_LSB_U32(xin);
  avi_hdr->streams      = xin->Read_LSB_U32(xin);
  avi_hdr->sug_bsize    = xin->Read_LSB_U32(xin);
  avi_hdr->width        = xin->Read_LSB_U32(xin);
  avi_hdr->height       = xin->Read_LSB_U32(xin);
  avi_hdr->scale        = xin->Read_LSB_U32(xin);
  avi_hdr->rate         = xin->Read_LSB_U32(xin);
  avi_hdr->start        = xin->Read_LSB_U32(xin);
  avi_hdr->length       = xin->Read_LSB_U32(xin);

  avi->cmap_frame_num = avi_hdr->tot_frames / cmap_sample_cnt;

  if (!(xin->type_flag & XA_IN_TYPE_RANDOM)) /* sequential */
  {
    if (avi_hdr->flags & AVIF_MUSTUSEINDEX)
    {
	fprintf(stdout,"AVI file must use index, but not possible with this input stream.\n");
	fprintf(stdout,"   Will do the best it can, but expect strange results.\n");
    }
    avi_use_index_flag = 0;
  }
  else if (   (avi_hdr->flags & AVIF_MUSTUSEINDEX) 
	   || (avi_hdr->flags & AVIF_HASINDEX) )	avi_use_index_flag = 1;
  else avi_use_index_flag = 0;

#ifdef POD_USE
  if (xa_verbose)
#else
  DEBUG_LEVEL1
#endif
  {
    fprintf(stdout,"  AVI flags: ");
    if (avi_hdr->flags & AVIF_HASINDEX) fprintf(stdout,"Has_Index ");
    if (avi_hdr->flags & AVIF_MUSTUSEINDEX) fprintf(stdout,"Use_Index ");
    if (avi_hdr->flags & AVIF_ISINTERLEAVED) fprintf(stdout,"Interleaved ");
    if (avi_hdr->flags & AVIF_WASCAPTUREFILE) fprintf(stdout,"Captured ");
    if (avi_hdr->flags & AVIF_COPYRIGHTED) fprintf(stdout,"Copyrighted ");
    fprintf(stdout,"\n");
  }
  return(xaTRUE);
}

/****-----------------------------------------------------------------****
 *
 ****-----------------------------------------------------------------****/
xaULONG RIFF_Read_STRH(xin,avi,size,strh_hdr)
XA_INPUT 	*xin;
XA_ANIM_SETUP	*avi;
xaULONG 	size;
AVI_STREAM_HDR	*strh_hdr;
{
  xaULONG d,tsize;
 
  if (size < 0x24) 
	{fprintf(stdout,"strh: size < 36 size = %d\n",size); return(xaFALSE);}
 
  strh_hdr->fcc_type    = xin->Read_MSB_U32(xin);
  strh_hdr->fcc_handler = xin->Read_MSB_U32(xin);
  strh_hdr->flags       = xin->Read_LSB_U32(xin);
  strh_hdr->priority    = xin->Read_LSB_U32(xin);
  strh_hdr->init_frames = xin->Read_LSB_U32(xin);
  strh_hdr->scale       = xin->Read_LSB_U32(xin);
  strh_hdr->rate        = xin->Read_LSB_U32(xin);
  strh_hdr->start       = xin->Read_LSB_U32(xin);
  strh_hdr->length      = xin->Read_LSB_U32(xin);
  strh_hdr->sug_bsize   = xin->Read_LSB_U32(xin);
  strh_hdr->quality     = xin->Read_LSB_U32(xin);
  strh_hdr->samp_size   = xin->Read_LSB_U32(xin);

  if (strh_hdr->fcc_type == RIFF_vids)
  {
    if (xa_jiffy_flag) { avi->vid_time = xa_jiffy_flag; avi->vid_timelo = 0; }
    else
    { double ftime =  1.0/15.0;  /* default on failure */

      /* TODO: what about start != 0??????? */
	/* TODO: on failure default to us in avih header */
      if (strh_hdr->rate > 0)
              ftime = (double)strh_hdr->scale / (double)(strh_hdr->rate);
      /* convert to milliseconds */
      ftime *= 1000.0;
      avi->vid_time =  (xaULONG)(ftime);
      ftime -= (double)(avi->vid_time);
      avi->vid_timelo = (ftime * (double)(1<<24));
/*
fprintf(stderr,"STRH: rate us_frame %lf\n",
	1000.0 * (double)strh_hdr->scale / (double)(strh_hdr->rate) );
*/
    }
  }

 
  tsize = 48; if (size & 0x01) size++;
  while(tsize < size) { d = xin->Read_U8(xin); tsize++; }
 
  DEBUG_LEVEL2 fprintf(stdout,"AVI TEST handler = 0x%08x\n",
						strh_hdr->fcc_handler);
  return(xaTRUE);
}

/****-----------------------------------------------------------------****
 *
 * Return:   xaFALSE on error.  xaTRUE on ok(supported or not)
 ****-----------------------------------------------------------------****/
xaULONG RIFF_Read_VIDS(xin,anim_hdr,avi,size,vids_hdr,stream_cnt)
XA_INPUT *xin;
XA_ANIM_HDR *anim_hdr;
XA_ANIM_SETUP *avi;
xaLONG size;
VIDS_HDR *vids_hdr;
xaULONG stream_cnt;
{ xaULONG d,i,ctable_flag;
  xaLONG  codec_ret;
 
  avi_has_video = xaTRUE;

  if (size & 0x01) size++;
  ctable_flag		= xaTRUE;
  vids_hdr->size        = xin->Read_LSB_U32(xin);
  vids_hdr->width       = xin->Read_LSB_U32(xin);
  vids_hdr->height      = xin->Read_LSB_U32(xin);
  vids_hdr->planes      = xin->Read_LSB_U16(xin);
  vids_hdr->bit_cnt     = xin->Read_LSB_U16(xin);
  vids_hdr->compression = xin->Read_MSB_U32(xin);
  vids_hdr->image_size  = xin->Read_LSB_U32(xin);
  vids_hdr->xpels_meter = xin->Read_LSB_U32(xin);
  vids_hdr->ypels_meter = xin->Read_LSB_U32(xin);
  vids_hdr->num_colors  = xin->Read_LSB_U32(xin);
  vids_hdr->imp_colors  = xin->Read_LSB_U32(xin);
  size -= 40;

  avi->compression = vids_hdr->compression;
DEBUG_LEVEL2 fprintf(stdout,"VIDS compression = %08x\n",avi->compression);
  avi->depth = vids_hdr->bit_cnt;
  avi->imagex = vids_hdr->width;
  avi->imagey = vids_hdr->height;



  avi->imagec = vids_hdr->num_colors;
  if ( (avi->imagec==0) && (avi->depth <= 8) ) avi->imagec = (1 << avi->depth);
  vids_hdr->num_colors = avi->imagec; /* re-update struct */

  avi_codec_hdr[stream_cnt].compression = avi->compression;
  avi_codec_hdr[stream_cnt].x = avi->imagex;
  avi_codec_hdr[stream_cnt].y = avi->imagey;
  avi_codec_hdr[stream_cnt].depth = avi->depth;
  avi_codec_hdr[stream_cnt].anim_hdr = (void *)anim_hdr;
  avi_codec_hdr[stream_cnt].avi_ctab_flag = ctable_flag;

  /* Query to see if Video Compression is supported or not */
  codec_ret = AVI_Video_Codec_Query( &avi_codec_hdr[stream_cnt] );
  if (codec_ret == CODEC_SUPPORTED)
  {
    avi->imagex = avi_codec_hdr[stream_cnt].x;
    avi->imagey = avi_codec_hdr[stream_cnt].y;
    avi->compression = avi_codec_hdr[stream_cnt].compression;
    ctable_flag = avi_codec_hdr[stream_cnt].avi_ctab_flag;
  }
  else /*** Return False if Codec is Unknown or Not Supported */
  /* if (codec_ret != CODEC_SUPPORTED) */
  { char tmpbuf[256];
    if (codec_ret == CODEC_UNKNOWN)
    { xaULONG ii,a[4],dd = avi->compression;

fprintf(stdout,"comp %08x %08x %08x\n",
		avi->compression,
		avi_codec_hdr[stream_cnt].compression,
		dd );

      for(ii=0; ii<4; ii++)
      { a[ii] = dd & 0xff;  dd >>= 8;
	if ((a[ii] < ' ') || (a[ii] > 'z')) a[ii] = '.';
      }
      sprintf(tmpbuf,"Unknown %c%c%c%c(%08x)",
           (char)a[3],(char)a[2],(char)a[1],(char)a[0],avi->compression);
      avi_codec_hdr[stream_cnt].description = tmpbuf;
    }
    if (xa_verbose)
                fprintf(stdout,"  Video Codec: %s",
			avi_codec_hdr[stream_cnt].description);
    else        fprintf(stdout,"AVI Video Codec: %s",
			avi_codec_hdr[stream_cnt].description);
    fprintf(stdout," is unsupported by this executable.(E%x)\n",
								avi->depth);

/* point 'em to the readme's */
    switch(avi->compression)
    { 
      case RIFF_iv41: 
      case RIFF_IV41: 
	fprintf(stdout,"      Please see the file \"indeo4x.readme\".\n");
	break;
      case RIFF_iv31: 
      case RIFF_IV31: 
      case RIFF_iv32: 
      case RIFF_IV32: 
      case RIFF_YVU9: 
      case RIFF_YUV9: 
	fprintf(stdout,"      Please see the file \"indeo.readme\".\n");
	break;
      case RIFF_cvid: 
      case RIFF_CVID: 
	fprintf(stdout,"      Please see the file \"cinepak.readme\".\n");
	break;
      case RIFF_cyuv: 
	fprintf(stdout,"      Please see the file \"creative.readme\".\n");
	break;
    }
    while(size > 0) { d = xin->Read_U8(xin); size--; }
    return(xaFALSE);
  }

  
  if (xa_verbose) fprintf(stdout,"  Video Codec: %s depth=%d\n",
			avi_codec_hdr[stream_cnt].description,avi->depth);

  /*** Read AVI Color Table if it's present */
  if (avi->depth == 40)  /* someone screwed up when converting QT to AVI */
  {
    avi->imagec = 256;
    vids_hdr->num_colors = avi->imagec; /* ness? */
    QT_Create_Gray_Cmap(avi->cmap,0,avi->imagec);
    avi->chdr = ACT_Get_CMAP(avi->cmap,avi->imagec,0,avi->imagec,0,16,16,16);
  }
  else if ( (avi->depth <= 8) && (ctable_flag==xaTRUE) )
  {
    DEBUG_LEVEL1 fprintf(stdout,"AVI reading cmap %d\n",avi->imagec);
    for(i=0; i < avi->imagec; i++)
    {
      avi->cmap[i].blue  =  xin->Read_U8(xin);
      avi->cmap[i].green =  xin->Read_U8(xin);
      avi->cmap[i].red   =  xin->Read_U8(xin);
      d = xin->Read_U8(xin); /* pad */
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
  if ( (size > 0) && (avi_codec_hdr[stream_cnt].avi_read_ext))
  { xaULONG ret = avi_codec_hdr[stream_cnt].avi_read_ext(xin,anim_hdr);
    size -= ret;
  }

  while(size > 0) { d = xin->Read_U8(xin); size--; }
  return(xaTRUE);
}

	/* by not setting act->type to NOP, yet returning 0, indicates
         * that compression is not supported and that this AVI
	 * file should be no read in. Currently this is fine since
	 * only one compression is supported per file.
	 */

/****-----------------------------------------------------------------****
 * This function return in three ways.
 *
 *  act->type = NOP        : NOP frame.
 *  dlta_hdr = 0           : comprssion not supported.
 *  dlta_hdr != 0	   : valid frame
 *
 *  NOTE: act->type comes in as ACT_DELTA
 *
 ****-----------------------------------------------------------------****/
ACT_DLTA_HDR *AVI_Read_00DC(xin,avi,ck_size,act,ck_offset,stream_num)
XA_INPUT *xin;
XA_ANIM_SETUP *avi;
xaLONG ck_size;
XA_ACTION *act;
xaLONG ck_offset;
xaLONG stream_num;
{
  ACT_DLTA_HDR *dlta_hdr = 0;
  xaULONG d;

  if (ck_size & 0x01) ck_size++;
	/* NOP wait frame */
  if (ck_size == 0)
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
    dlta_hdr->fsize = ck_size;
    if (ck_offset >= 0)		dlta_hdr->fpos  = ck_offset;
    else   /* fin marks chunk offset and need to seek past */
    { dlta_hdr->fpos  = xin->Get_FPos(xin);
      xin->Seek_FPos(xin,ck_size,1); /* move past this chunk */
    }
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
    if (ck_offset >= 0)		xin->Seek_FPos( xin, ck_offset, 0);
    if (ck_size)
    { ret = xin->Read_Block(xin,dlta_hdr->data,ck_size);
      if (ret != ck_size) 
      {  fprintf(stdout,"AVI vid dlta: read failed\n"); 
         act->type = ACT_NOP;	act->data = 0;	act->chdr = 0;
         free(dlta_hdr);
         return(0);
      }
    }
  }

  AVI_Add_Frame( avi->vid_time, avi->vid_timelo, act);
  dlta_hdr->xpos = dlta_hdr->ypos = 0;
  dlta_hdr->xsize = avi->imagex;
  dlta_hdr->ysize = avi->imagey;
  dlta_hdr->special = 0;
  dlta_hdr->xapi_rev = avi_codec_hdr[stream_num].xapi_rev;
  dlta_hdr->delta = avi_codec_hdr[stream_num].decoder;
  dlta_hdr->extra = avi_codec_hdr[stream_num].extra;

  if (avi->compression == RIFF_MJPG)  /* Special Case */
  { if (avi_first_delta==0)
    { xaUBYTE *mjpg_tmp,*mjpg_buff = dlta_hdr->data;
      xaLONG  mjpg_size  = ck_size;
      avi_first_delta = 1;
      if (xa_file_flag==xaTRUE) /* read from file if necessary */
      { xaULONG ret;
	mjpg_tmp = mjpg_buff = (xaUBYTE *)malloc(ck_size);
	xin->Seek_FPos(xin,dlta_hdr->fpos,0);
	ret = xin->Read_Block(xin, mjpg_tmp, mjpg_size);
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
  return(dlta_hdr);
}


/*
 * Routine to Decode an AVI CRAM chunk
 */

#define CRAM_DITH_COL2RGB(_r,_g,_b,_col) { \
_r = (_col & 0x7c00); _g = (_col & 0x03e0); _b = (_col & 0x001f);       \
_r = _r | (_r >> 5);  _g = (_g << 5) | _g; _b = (_b << 10) | (_b << 5); }

#define CRAM_DITH_GET_RGB(_r,_g,_b,_re,_ge,_be,_col) { xaLONG r1,g1,b1; \
  r1 = (xaLONG)rnglimit[(_r + _re) >> 7]; \
  g1 = (xaLONG)rnglimit[(_g + _ge) >> 7]; \
  b1 = (xaLONG)rnglimit[(_b + _be) >> 7]; \
  _col = (r1 & 0xe0) | ((g1 & 0xe0) >> 3) | ((b1 & 0xc0) >> 6); }

#define CRAM_DITH_GET_ERR(_r,_g,_b,_re,_ge,_be,_col,cmap) { \
  _re =  ((xaLONG)(_r) - (xaLONG)(cmap[_col].red   >> 1)) >> 1; \
  _ge =  ((xaLONG)(_g) - (xaLONG)(cmap[_col].green >> 1)) >> 1; \
  _be =  ((xaLONG)(_b) - (xaLONG)(cmap[_col].blue  >> 1)) >> 1; }


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
AVI_Decode_RGB4(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG oddcnt;
  xaUBYTE *dptr = delta;
  
  dec_info->xs = dec_info->ys = 0;
  dec_info->xe = imagex; dec_info->ye = imagey;
    /* Indicate we can drop these frames */
  if (dec_info->skip_flag > 0) return(ACT_DLTA_DROP);

  oddcnt = ((imagex & 0x03)==1)?(1):(0);
  if (map_flag == xaTRUE)
  { if (x11_bytes_pixel == 4)
    { xaLONG x,y = imagey - 1;
      while ( y >= 0 )
      { xaULONG *i_ptr = (xaULONG *)(image + ((y * imagex)<<2) ); y--; 
        x = imagex; while(x) { xaUBYTE d = *dptr++; x -= 2;
	   *i_ptr++ = (xaULONG)map[d >> 4]; *i_ptr++ = (xaULONG)map[d & 0xf]; }
        x = oddcnt; while(x--) dptr++; 
      }
    }
    else if (x11_bytes_pixel == 2)
    { xaLONG x,y = imagey - 1;
      while ( y >= 0 )
      { xaUSHORT *i_ptr = (xaUSHORT *)(image + ((y * imagex)<<1) ); y--; 
        x = imagex; while(x) { xaUBYTE d = *dptr++; x -= 2;
	  *i_ptr++ = (xaUSHORT)map[d >> 4]; *i_ptr++ = (xaUSHORT)map[d & 0xf]; }
        x = oddcnt; while(x--) dptr++; 
      }
    }
    else /* (x11_bytes_pixel == 1) */
    { xaLONG x,y = imagey - 1;
      while ( y >= 0 )
      { xaUBYTE *i_ptr = (xaUBYTE *)(image + y * imagex); y--; 
        x = imagex; while(x) { xaUBYTE d = *dptr++; x -= 2;
	   *i_ptr++ = (xaUBYTE)map[d >> 4]; *i_ptr++ = (xaUBYTE)map[d & 0xf]; }
        x = oddcnt; while(x--) dptr++; 
      }
    }
  } /* end of map is xaTRUE */
  else
  { xaLONG x,y = imagey - 1;
    while ( y >= 0 )
    { xaUBYTE *i_ptr = (xaUBYTE *)(image + y * imagex); y--; 
        x = imagex; while(x) { xaUBYTE d = *dptr++; x -= 2;
	   *i_ptr++ = (xaUBYTE)(d >> 4); *i_ptr++ = (xaUBYTE)(d & 0xf); }
      x = oddcnt; while(x--) dptr++; 
    }
  }
  if (map_flag) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}


xaULONG
AVI_Decode_RGB8(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG oddcnt;
  xaUBYTE *dptr = delta;
  
  dec_info->xs = dec_info->ys = 0;
  dec_info->xe = imagex; dec_info->ye = imagey;
    /* Indicate we can drop these frames */
  if (dec_info->skip_flag > 0) return(ACT_DLTA_DROP);

  oddcnt = 4 - (imagex & 0x03);  if (oddcnt == 4) oddcnt = 0;
  if (map_flag == xaTRUE)
  {
    if (x11_bytes_pixel == 4)
    { xaLONG x,y = imagey - 1;
      while ( y >= 0 )
      { xaULONG *i_ptr = (xaULONG *)(image + ((y * imagex)<<2) ); y--; 
        x = imagex; while(x--) *i_ptr++ = (xaULONG)map[*dptr++];
        x = oddcnt; while(x--) dptr++; 
      }
    }
    else if (x11_bytes_pixel == 2)
    { xaLONG x,y = imagey - 1;
      while ( y >= 0 )
      { xaUSHORT *i_ptr = (xaUSHORT *)(image + ((y * imagex)<<1) ); y--; 
        x = imagex; while(x--) *i_ptr++ = (xaUSHORT)map[*dptr++];
        x = oddcnt; while(x--) dptr++; 
      }
    }
    else /* (x11_bytes_pixel == 1) */
    { xaLONG x,y = imagey - 1;
      while ( y >= 0 )
      { xaUBYTE *i_ptr = (xaUBYTE *)(image + y * imagex); y--; 
        x = imagex; while(x--) *i_ptr++ = (xaUBYTE)map[*dptr++];
        x = oddcnt; while(x--) dptr++; 
      }
    }
  } /* end of map is xaTRUE */
  else
  { xaLONG x,y = imagey - 1;
    while ( y >= 0 )
    { xaUBYTE *i_ptr = (xaUBYTE *)(image + y * imagex); y--; 
      x = imagex; while(x--) *i_ptr++ = (xaUBYTE)*dptr++;
      x = oddcnt; while(x--) dptr++; 
    }
  }
  if (map_flag) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}

/*******************
 *  RGB Decode Depth 16
 ******/
xaULONG
AVI_Decode_RGB16(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG special = dec_info->special;
  XA_CHDR *chdr = dec_info->chdr;
  xaUBYTE *dp = delta;
  xaULONG special_flag = special & 0x0001;

  dec_info->xs = dec_info->ys = 0;
  dec_info->xe = imagex; dec_info->ye = imagey;
    /* Indicate we can drop these frames */
  if (dec_info->skip_flag > 0) return(ACT_DLTA_DROP);

  if (chdr) { if (chdr->new_chdr) chdr = chdr->new_chdr; }
  if (special_flag)
  { xaLONG x,y = imagey - 1;
    while ( y >= 0 )
    { xaUBYTE *iptr = (xaUBYTE *)(image + (y * imagex * 3)); y--; 
      x = imagex;
      while(x--) { xaULONG r,g,b, d = *dp++; d |= (*dp++)<<8;
			r = (d >> 10) & 0x1f; r = (r << 3) | (r >> 2);
			g = (d >>  5) & 0x1f; g = (g << 3) | (g >> 2);
			b =  d & 0x1f;        b = (b << 3) | (b >> 2);
	*iptr++ = (xaUBYTE)r; *iptr++ = (xaUBYTE)g; *iptr++ = (xaUBYTE)b; }
    }
  }
  else if ( (map_flag==xaFALSE) || (x11_bytes_pixel==1) )
  { xaLONG x,y = imagey - 1;
    while ( y >= 0 )
    { xaUBYTE *iptr = (xaUBYTE *)(image + y * imagex); y--; 
      x = imagex; 
      while(x--) 
      { xaULONG d = *dp++; d |= (*dp++)<<8;
        *iptr++ = (xaUBYTE)XA_RGB16_To_CLR32(d,map_flag,map,chdr);
      }
    }
  }
  else if (x11_bytes_pixel==4)
  { xaLONG x,y = imagey - 1;
    while ( y >= 0 )
    { xaULONG *iptr = (xaULONG *)(image + ((y * imagex)<<2) ); y--; 
      x = imagex; 
      while(x--) 
      { xaULONG d = *dp++; d |= (*dp++)<<8;
        *iptr++ = (xaULONG)XA_RGB16_To_CLR32(d,map_flag,map,chdr);
      }
    }
  }
  else if (x11_bytes_pixel==2)
  { xaLONG x,y = imagey - 1;
    while ( y >= 0 )
    { xaUSHORT *iptr = (xaUSHORT *)(image + ((y * imagex)<<1) ); y--; 
      x = imagex; 
      while(x--) 
      { xaULONG d = *dp++; d |= (*dp++)<<8;
        *iptr++ = (xaUSHORT)XA_RGB16_To_CLR32(d,map_flag,map,chdr);
      }
    }
  }
  if (map_flag) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}


/*******************
 *  RGB Decode Depth 24
 *  Actually BGR
 ******/
xaULONG
AVI_Decode_RGB24(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG special = dec_info->special;
  XA_CHDR *chdr = dec_info->chdr;
  xaUBYTE *dp = delta;
  xaULONG pad;
  xaULONG special_flag = special & 0x0001;

  dec_info->xs = dec_info->ys = 0;
  dec_info->xe = imagex; dec_info->ye = imagey;
    /* Indicate we can drop these frames */
  if (dec_info->skip_flag > 0) return(ACT_DLTA_DROP);

  if (chdr) { if (chdr->new_chdr) chdr = chdr->new_chdr; }
  pad = (4 - (3 * imagex % 4)) & 0x03;
  if (special_flag)
  { xaLONG x,y = imagey - 1;
    while ( y >= 0 )
    { xaUBYTE *iptr = (xaUBYTE *)(image + (y * imagex * 3)); y--; 
      x = imagex;
      while(x--) { xaUBYTE r,g,b; b = *dp++; g = *dp++; r = *dp++;
       *iptr++ = r; *iptr++ = g; *iptr++ = b; }
      if (pad) dp += pad;
    }
  }
  else if ( (map_flag==xaFALSE) || (x11_bytes_pixel==1) )
  { xaLONG x,y = imagey - 1;
    while ( y >= 0 )
    { xaUBYTE *iptr = (xaUBYTE *)(image + y * imagex); y--; 
      x = imagex; 
      while(x--) 
      { xaULONG r,g,b; b = (xaULONG)*dp++; g = (xaULONG)*dp++; r = (xaULONG)*dp++;
        *iptr++ = (xaUBYTE)XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
      }
      if (pad) dp += pad;
    }
  }
  else if (x11_bytes_pixel==4)
  { xaLONG x,y = imagey - 1;
    while ( y >= 0 )
    { xaULONG *iptr = (xaULONG *)(image + ((y * imagex)<<2) ); y--; 
      x = imagex; 
      while(x--) 
      { xaULONG r,g,b; b = (xaULONG)*dp++; g = (xaULONG)*dp++; r = (xaULONG)*dp++;
        *iptr++ = (xaULONG)XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
      }
      if (pad) dp += pad;
    }
  }
  else if (x11_bytes_pixel==2)
  { xaLONG x,y = imagey - 1;
    while ( y >= 0 )
    { xaUSHORT *iptr = (xaUSHORT *)(image + ((y * imagex)<<1) ); y--; 
      x = imagex; 
      while(x--) 
      { xaULONG r,g,b; b = (xaULONG)*dp++; g = (xaULONG)*dp++; r = (xaULONG)*dp++;
        *iptr++ = (xaUSHORT)XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
      }
      if (pad) dp += pad;
    }
  }
  if (map_flag) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}


void AVI_Print_ID(fout,id)
FILE *fout;
xaLONG id;
{ fprintf(fout,"%c",     (char)((id >> 24) & 0xff)   );
  fprintf(fout,"%c",     (char)((id >> 16) & 0xff)   );
  fprintf(fout,"%c",     (char)((id >>  8) & 0xff)   );
  fprintf(fout,"%c(%x)",(char) (id        & 0xff),id);
}


xaULONG AVI_Get_Color(color,map_flag,map,chdr)
xaULONG color,map_flag,*map;
XA_CHDR *chdr;
{ register xaULONG clr,ra,ga,ba,tr,tg,tb;
 
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
AVI_Decode_RLE8(image,delta,tdsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG tdsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG opcode,mod;
  xaLONG x,y,min_x,max_x,min_y,max_y;
  xaUBYTE *dptr;
  xaLONG dsize = tdsize;

  max_x = max_y = 0; min_x = imagex; min_y = imagey;
  x = 0;  y = imagey - 1;
  dptr = delta;

  while( (y >= 0) && (dsize > 0) )
  {
    mod = *dptr++;
    opcode = *dptr++;  dsize-=2;

DEBUG_LEVEL2 fprintf(stdout,"MOD %x OPCODE %x <%d,%d>\n",mod,opcode,x,y);
    if (mod == 0x00)				/* END-OF-LINE */
    {
      if (opcode==0x00)
      {
        while(x > imagex) { x -=imagex; y--; }
        x = 0; y--;
DEBUG_LEVEL2 fprintf(stdout,"EOL <%d,%d>\n",x,y);
      }
      else if (opcode==0x01)			/* END Of Image */
      {
        y = -1;
DEBUG_LEVEL2 fprintf(stdout,"EOI <%d,%d>\n",x,y);
      }
      else if (opcode==0x02)			/* SKIP */
      {
        xaULONG yskip,xskip;
        xskip = *dptr++; 
        yskip = *dptr++;  dsize-=2;
        x += xskip;
        y -= yskip;
DEBUG_LEVEL2 fprintf(stdout,"SKIP <%d,%d>\n",x,y);
      }
      else					/* ABSOLUTE MODE */
      {
        int cnt = opcode;
        
	dsize-=cnt;
        while(x >= imagex) { x -= imagex; y--; }
	if (y > max_y) max_y = y; if (x < min_x) min_x = x;
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
DEBUG_LEVEL2 fprintf(stdout,"ABSOLUTE <%d,%d>\n",x,y);
		/* PAD to Short */
        if (opcode & 0x01) { dptr++; dsize--; }
        if (y < min_y) min_y = y; if (x > max_x) max_x = x;
      }
    }
    else					/* ENCODED MODE */
    {
      int color,cnt;
      while(x >= imagex) { x -=imagex; y--; }
      if (y > max_y) max_y = y; if (x < min_x) min_x = x;  /* OPT */
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
      if (y < min_y) min_y = y; if (x > max_x) max_x = x;
DEBUG_LEVEL2 fprintf(stdout,"ENCODED <%d,%d>\n",x,y);
    }
  } /* end of while */

DEBUG_LEVEL2 
{
  fprintf(stdout,"dsize %d\n  ",dsize);
  while(dsize)  { int d = *dptr++;  fprintf(stdout,"<%02x> ",d); dsize--; }
  fprintf(stdout,"\n");
}

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
  xaULONG special = dec_info->special;
  XA_CHDR *chdr = dec_info->chdr;
  xaULONG row_dec,exitflag,changed,block_cnt;
  xaULONG code0,code1,dither = 0;
  xaLONG x,y,min_x,max_x,min_y,max_y;
  xaUBYTE *dptr;

  if (chdr) { if (chdr->new_chdr) chdr = chdr->new_chdr; }

   /* special case dither routine - fill out a bit */
  if (   (!special) && (x11_bytes_pixel==1)
      && (chdr) && (x11_display_type == XA_PSEUDOCOLOR)
      && (cmap_color_func != 4)
      && (cmap_true_to_332 == xaTRUE) && (x11_cmap_size == 256)
      && (xa_dither_flag==xaTRUE)
     )
     dither = 1;


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
    if (dither)
    { ColorReg *cmap = chdr->cmap;
      xaUBYTE *rnglimit = xa_byte_limit;

      while(!exitflag)
      { code0 =  *dptr++;	code1 =  *dptr++;	block_cnt--;
	if ( (code1==0) && (code0==0) && !block_cnt) { exitflag = 1; continue; }
	if (y < 0) {exitflag = 1; continue; }
	if ((code1 >= 0x84) && (code1 <= 0x87)) /* skip */
	{ xaULONG skip = ((code1 - 0x84) << 8) + code0;
	  block_cnt -= (skip-1); while(skip--) AVI_BLOCK_INC(x,y,imagex);
	}
	else /* not skip */
	{ xaUBYTE *ip = (xaUBYTE *)(image + (y * imagex + x) );
	  if (code1 < 0x80) /* 2 or 8 color encoding */
	  { xaULONG cA,cB;
	    AVI_GET_16(cB,dptr); AVI_GET_16(cA,dptr);
	    if (cB & 0x8000)   /* Eight Color Encoding */
	    { xaUBYTE cA0,cB0, cA1,cB1;
	      cB0 = (xaUBYTE)AVI_Get_Color(cB,map_flag,map,chdr);
	      cA0 = (xaUBYTE)AVI_Get_Color(cA,map_flag,map,chdr);
	      AVI_GET_16(cB,dptr); AVI_GET_16(cA,dptr);
	      cB1 = (xaUBYTE)AVI_Get_Color(cB,map_flag,map,chdr);
	      cA1 = (xaUBYTE)AVI_Get_Color(cA,map_flag,map,chdr);
	      AVI_CRAM_C4(ip,code0,cA0,cA1,cB0,cB1,row_dec);
	      ip -=row_dec;
	      AVI_GET_16(cB,dptr); AVI_GET_16(cA,dptr);
	      cB0 = (xaUBYTE)AVI_Get_Color(cB,map_flag,map,chdr);
	      cA0 = (xaUBYTE)AVI_Get_Color(cA,map_flag,map,chdr);
	      AVI_GET_16(cB,dptr); AVI_GET_16(cA,dptr);
	      cB1 = (xaUBYTE)AVI_Get_Color(cB,map_flag,map,chdr);
	      cA1 = (xaUBYTE)AVI_Get_Color(cA,map_flag,map,chdr);
	      AVI_CRAM_C4(ip,code1,cA0,cA1,cB0,cB1,row_dec);
	    } else /* Two Color Encoding */
	    { xaUBYTE clr0a,clr0b,clr1a,clr1b;
	      xaLONG re,ge,be,r,g,b;

	      CRAM_DITH_COL2RGB(r,g,b,cA);
	      CRAM_DITH_GET_RGB(r,g,b, 0, 0, 0,clr0a);
	      if (map_flag) clr0a = map[clr0a];
	      CRAM_DITH_GET_ERR(r,g,b,re,ge,be,clr0a,cmap);
	      CRAM_DITH_GET_RGB(r,g,b,re,ge,be,clr1a);
	      if (map_flag) clr1a = map[clr1a];

	      CRAM_DITH_COL2RGB(r,g,b,cB);
	      CRAM_DITH_GET_RGB(r,g,b, 0, 0, 0,clr0b);
	      if (map_flag) clr0b = map[clr0b];
	      CRAM_DITH_GET_ERR(r,g,b,re,ge,be,clr0b,cmap);
	      CRAM_DITH_GET_RGB(r,g,b,re,ge,be,clr1b);
	      if (map_flag) clr1b = map[clr1b];

  *ip++ =(code0 & 0x01)?(clr0b):(clr0a); *ip++ =(code0 & 0x02)?(clr1b):(clr1a); 
  *ip++ =(code0 & 0x04)?(clr0b):(clr0a); *ip   =(code0 & 0x08)?(clr1b):(clr1a);
  ip -= row_dec; 
  *ip++ =(code0 & 0x10)?(clr1b):(clr1a); *ip++ =(code0 & 0x20)?(clr0b):(clr0a); 
  *ip++ =(code0 & 0x40)?(clr1b):(clr1a); *ip   =(code0 & 0x80)?(clr0b):(clr0a);
  ip -= row_dec;
  *ip++ =(code1 & 0x01)?(clr0b):(clr0a); *ip++ =(code1 & 0x02)?(clr1b):(clr1a); 
  *ip++ =(code1 & 0x04)?(clr0b):(clr0a); *ip   =(code1 & 0x08)?(clr1b):(clr1a);
  ip -= row_dec; 
  *ip++ =(code1 & 0x10)?(clr1b):(clr1a); *ip++ =(code1 & 0x20)?(clr0b):(clr0a); 
  *ip++ =(code1 & 0x40)?(clr1b):(clr1a); *ip   =(code1 & 0x80)?(clr0b):(clr0a);

	    }
	  } /* end of 2 or 8 */
	  else /* 1 color encoding (80-83) && (>=88)*/
	  { xaULONG cA = (code1<<8) | code0;
	    xaUBYTE clr0,clr1;
	    xaLONG re,ge,be,r,g,b;

	    CRAM_DITH_COL2RGB(r,g,b,cA);
	    CRAM_DITH_GET_RGB(r,g,b, 0, 0, 0,clr0);
	    if (map_flag) clr0 = map[clr0];
	    CRAM_DITH_GET_ERR(r,g,b,re,ge,be,clr0,cmap);
	    CRAM_DITH_GET_RGB(r,g,b,re,ge,be,clr1);
	    if (map_flag) clr1 = map[clr1];

 *ip++ = clr0; *ip++ = clr1; *ip++ = clr0; *ip = clr1; ip -= row_dec; 
 *ip++ = clr1; *ip++ = clr0; *ip++ = clr1; *ip = clr0; ip -= row_dec; 
 *ip++ = clr0; *ip++ = clr1; *ip++ = clr0; *ip = clr1; ip -= row_dec; 
 *ip++ = clr1; *ip++ = clr0; *ip++ = clr1; *ip = clr0; 


	  }
	  changed = 1; AVI_MIN_MAX_CHECK(x,y,min_x,max_x,min_y,max_y);
	  AVI_BLOCK_INC(x,y,imagex);
	} /* end of not skip */
      } /* end of not while exit */
    } /* end of dither */
    else if ( (x11_bytes_pixel == 1) || (map_flag == xaFALSE) )
    {
      while(!exitflag)
      { code0 =  *dptr++;	code1 =  *dptr++;	block_cnt--;
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
  xaULONG special = dec_info->special;
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
      fprintf(stdout,"y = %d block_cnt = %d bhedr = %x\n",y,block_cnt,bhedr);
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
	    else { fprintf(stdout,"ULTI: stream err %d\n",stream_mode);
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
	  fprintf(stdout,"Reserved Escapes %x\n",bhedr);
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
		fprintf(stdout,"Error opcode=%x\n",opcode);
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
 
  if (x11_display_type & XA_X11_TRUE) clr = X11_Get_True_Color(tr,tg,tb,8);
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

xaULONG RIFF_Read_AUDS(xin,size,auds_hdr)
XA_INPUT *xin;
xaLONG size;
AUDS_HDR *auds_hdr;
{ xaULONG ret = xaTRUE;
  auds_hdr->format	= xin->Read_LSB_U16(xin);
  auds_hdr->channels	= xin->Read_LSB_U16(xin);
  auds_hdr->rate	= xin->Read_LSB_U32(xin);
  auds_hdr->av_bps	= xin->Read_LSB_U32(xin);
  auds_hdr->blockalign	= xin->Read_LSB_U16(xin);
  auds_hdr->samps_block	= 1;
  auds_hdr->byte_cnt	= 0;

  if (size & 0x01) size++;
  if (size >= 0x10) { auds_hdr->size = xin->Read_LSB_U16(xin); size -= 0x10; }
  else  { auds_hdr->size  = 8; size -= 0x0e; }


  DEBUG_LEVEL2 fprintf(stdout,"ret = %x\n",ret);
  if (auds_hdr->format == WAVE_FORMAT_PCM)
  {
    if (auds_hdr->size == 8) avi_audio_type = XA_AUDIO_LINEAR;
    else if (auds_hdr->size == 16) avi_audio_type = XA_AUDIO_SIGNED;
    else avi_audio_type = XA_AUDIO_INVALID;
  }
  else if (auds_hdr->format == WAVE_FORMAT_ADPCM)
  { int i;
    if (auds_hdr->size == 4) avi_audio_type = XA_AUDIO_ADPCM;
    else avi_audio_type = XA_AUDIO_INVALID;
    auds_hdr->ext_size    = xin->Read_LSB_U16(xin);
    auds_hdr->samps_block = xin->Read_LSB_U16(xin);
    auds_hdr->num_coefs   = xin->Read_LSB_U16(xin);	size -= 6;
#ifdef POD_USE
    if (xa_verbose) fprintf(stdout," MSADPM_EXT: sampblk %d numcoefs %d\n",
				auds_hdr->samps_block, auds_hdr->num_coefs);
#endif
    for(i=0; i < auds_hdr->num_coefs; i++)
    { xaSHORT coef1, coef2;
      coef1 = xin->Read_LSB_U16(xin);
      coef2 = xin->Read_LSB_U16(xin);			size -= 4;
#ifdef POD_USE
      if (xa_verbose) fprintf(stdout,"%d) coef1 %d coef2 %d\n",
							i, coef1, coef2);
#endif
    }
  }
  else if (auds_hdr->format == WAVE_FORMAT_DVI_ADPCM)
  {
    avi_audio_type = XA_AUDIO_DVI;
    auds_hdr->ext_size    = xin->Read_LSB_U16(xin);
    auds_hdr->samps_block = xin->Read_LSB_U16(xin);	size -= 4;
#ifdef POD_USE
    if (xa_verbose) fprintf(stdout," DVI: samps per block %d\n",
						auds_hdr->samps_block);
#endif
  }
  else if (auds_hdr->format == WAVE_FORMAT_ALAW)
  {
    avi_audio_type = XA_AUDIO_ALAW;
  }
  else if (auds_hdr->format == WAVE_FORMAT_MULAW)
  {
    avi_audio_type = XA_AUDIO_ULAW;
  }
#ifdef XA_GSM
  else if (auds_hdr->format == WAVE_FORMAT_GSM610)
  {
    avi_audio_type = XA_AUDIO_MSGSM;
    auds_hdr->ext_size    = xin->Read_LSB_U16(xin);
    auds_hdr->samps_block = xin->Read_LSB_U16(xin);     size -= 4;
#ifdef POD_USE
    if (xa_verbose) fprintf(stdout," GSM: samps per block %d\n",
                                                auds_hdr->samps_block);
#endif
    GSM_Init();
  }
#endif
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

  if (xa_verbose)
  {
    fprintf(stdout,"  Audio Codec: "); AVI_Print_Audio_Type(auds_hdr->format);
    fprintf(stdout," Rate=%d Chans=%d bps=%d\n",
			avi_audio_freq,avi_audio_chans,auds_hdr->size);
#ifdef POD_USE
  fprintf(stdout,"        block_align %d\n",auds_hdr->blockalign);
#endif
  }
/* modify type */
  if (avi_audio_chans==2)	avi_audio_type |= XA_AUDIO_STEREO_MSK;
  if (avi_audio_bps==2)		avi_audio_type |= XA_AUDIO_BPS_2_MSK;
  DEBUG_LEVEL2 fprintf(stdout,"size = %d ret = %x\n",size,ret);
  while(size > 0) { (void)xin->Read_U8(xin); size--; }
  return(ret);
}

void AVI_Print_Audio_Type(type)
xaULONG type;
{
  switch(type)
  {
    case WAVE_FORMAT_PCM: fprintf(stdout,"PCM"); break;
    case WAVE_FORMAT_ADPCM: fprintf(stdout,"MS ADPCM"); break;
    case WAVE_FORMAT_DVI_ADPCM: fprintf(stdout,"DVI ADPCM"); break;
    case WAVE_FORMAT_ALAW: fprintf(stdout,"ALAW"); break;
    case WAVE_FORMAT_MULAW: fprintf(stdout,"ULAW"); break;
    case WAVE_FORMAT_OKI_ADPCM: fprintf(stdout,"OKI_ADPCM"); break;
    case IBM_FORMAT_MULAW: fprintf(stdout,"IBM_ULAW"); break;
    case IBM_FORMAT_ALAW: fprintf(stdout,"IBM_ALAW"); break;
    case IBM_FORMAT_ADPCM: fprintf(stdout,"IBM_ADPCM"); break;
    case WAVE_FORMAT_GSM610: fprintf(stdout,"GSM 6.10"); break;
    case WAVE_FORMAT_DSP_TRUESPEECH: fprintf(stdout,"DSP TrueSpeech"); break;
    default: fprintf(stdout,"Unknown(%x)",type); break;
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
DEBUG_LEVEL2
{ xaULONG i;
  xaUBYTE *tmp = avi_ulti_tab;
  for(i=0;i<4096;i++)
  {
    fprintf(stdout,"%02x %02x %02x %02x\n",tmp[0],tmp[1],tmp[2],tmp[3]);
    tmp += 4;
  }
}
}


#ifdef NO_LONGER_USED
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
#endif


/************************
 *  Find next MPEG start code. If buf is non-zero, get data from
 *  buf. Else use the file pointer, xin.
 *
 ****/
static xaLONG xmpg_get_start_code(bufp,buf_size)
xaUBYTE **bufp;
xaLONG *buf_size;
{ xaLONG d,size = *buf_size;
  xaULONG state = 0;
  xaUBYTE *buf = *bufp;

  while(size > 0)
  { d = (xaLONG)((*buf++) & 0xff);
    size--;
    if (state == 3) 
    { *buf_size = size; 
      *bufp = buf; 
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
  *bufp = buf;
  *buf_size = 0;
  return((xaLONG)(-1));
}

/****----------------------------------------------------------------****
 * This parses STRD chunk for a MPEG Sequence header in case the
 * XMPG width/height is different than the AVI width/height.
 ****----------------------------------------------------------------****/
static void AVI_XMPG_Kludge(avi,vids_hdr,avi_strd,ck_size,stream_cnt)
XA_ANIM_SETUP *avi;
VIDS_HDR *vids_hdr;
xaUBYTE *avi_strd;
xaLONG ck_size;
xaULONG stream_cnt;
{
  xaUBYTE *buf = avi_strd;
  xaLONG len = ck_size;
  xaULONG width = 0;
  xaULONG height = 0;
  xaULONG /* pic_rate,*/ pic_size; /* TODO:what about pic_rate? */

  while(1)
  { xaLONG code = xmpg_get_start_code(&buf,&len);

    if (code < 0) /* EOF of STRING */
    {
      break;
    }

    switch( code )
    {
      case MPG_SEQ_START:
      { /* 12 w, 12 h, 4 aspect, 4 picrate, 18 bitrate etc */
        xaLONG d;

	if (len < 3) break;
        d  = *buf++;
        width = (d & 0xff) << 4;
	d = *buf++;
	width |= (d >> 4) & 0x0f;
	height = (d & 0x0f) << 8;
	d = *buf++;
	height |= (d & 0xff);
	len -= 3;

	/* Eventually read rest of Sequence header for kicks */
	buf += 5;
	len -= 5;
	
	if ( (width != avi->imagex) || (height != avi->imagey) )
	{
          vids_hdr->width  = avi->imagex = width;
          vids_hdr->height = avi->imagey = height;
	  avi_codec_hdr[stream_cnt].x = width;
	  avi_codec_hdr[stream_cnt].y = height;

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
      }
      break;

      case MPG_GOP_START:
        {
	  buf += 4;
	  len -= 4;
        }
      break;

      default:
	DEBUG_LEVEL1 fprintf(stderr,"XMPG: STRD CODE: %02x\n",code);
        break;
    }
  }
}

/****************************
 *
 *
 ****************/
xaLONG	AVI_Codec_Query(codec)
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
	if (codec->depth == 4)		codec->decoder = AVI_Decode_RGB4;
	else if (codec->depth == 8)	codec->decoder = AVI_Decode_RGB8;
	else if (codec->depth == 16)	codec->decoder = AVI_Decode_RGB16;
	else if (codec->depth == 24)	codec->decoder = AVI_Decode_RGB24;
	else ret = CODEC_UNSUPPORTED;
	break;

    case RIFF_RLE8: 
    case RIFF_rle8: 
	codec->compression	= RIFF_RLE8;
	codec->description	= "Microsoft RLE8";
	ret = CODEC_SUPPORTED;
	if (codec->depth == 8)
	{ codec->decoder = AVI_Decode_RLE8;
	  codec->x = 4 * ((codec->x + 3)/4);
	}
	else ret = CODEC_UNSUPPORTED;
	break;

    case RIFF_wham:
    case RIFF_WHAM:
    case RIFF_msvc:
    case RIFF_MSVC:
    case RIFF_cram:
    case RIFF_CRAM:
	codec->compression	= RIFF_CRAM;
	codec->description	= "Microsoft Video 1";
	ret = CODEC_SUPPORTED;
        if (codec->depth == 8)		codec->decoder = AVI_Decode_CRAM;
	else if (codec->depth == 16)	codec->decoder = AVI_Decode_CRAM16;
	else ret = CODEC_UNSUPPORTED;
	codec->x = 4 * ((codec->x + 3)/4);
	codec->y = 4 * ((codec->y + 3)/4);
	JPG_Setup_Samp_Limit_Table(codec->anim_hdr);
	break;

	/* POD NOTE: this needs special case later on. Work In?? */
    case RIFF_xmpg:
    case RIFF_XMPG:
	codec->compression	= RIFF_XMPG;
	codec->description	= "Editable MPEG";
	ret = CODEC_SUPPORTED;
	codec->decoder		= MPG_Decode_FULLI;
        codec->extra = (void *)(0x02);
	codec->x = 4 * ((codec->x + 3)/4);
	codec->y = 4 * ((codec->y + 3)/4);
	XA_Gen_YUV_Tabs(codec->anim_hdr);
	JPG_Alloc_MCU_Bufs(codec->anim_hdr,codec->x,codec->y,xaTRUE);
	JPG_Setup_Samp_Limit_Table(codec->anim_hdr);
	MPG_Init_Stuff(codec->anim_hdr);
	break;

    case RIFF_bw10:
    case RIFF_BW10:
	codec->compression	= RIFF_BW10;
	codec->description	= "Broadway MPEG";
	ret = CODEC_SUPPORTED;
	codec->decoder		= MPG_Decode_FULLI;
        codec->extra = (void *)(0x01);
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
	codec->xapi_rev = 0x0002;
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

    case RIFF_dmb1: 
    case RIFF_DMB1:
	codec->compression	= RIFF_DMB1;
	codec->description	= "Rainbow Runner JPEG";
	codec->xapi_rev = 0x0002;
	ret = CODEC_SUPPORTED;
	if ( (codec->depth == 8) || (codec->depth == 24) )
	{
	  /* codec->avi_read_ext = AVI_JPEG_Read_Ext; ??? */
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
	codec->xapi_rev = 0x0002;
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
	  JPG_Setup_Samp_Limit_Table(codec->anim_hdr);
	}
	else ret = CODEC_UNSUPPORTED;
	break;

    case RIFF_v422:
    case RIFF_V422:
	codec->compression	= RIFF_V422;
	codec->description	= "Vitec Multimedia(V422) ";
	codec->xapi_rev = 0x0002;
	ret = CODEC_SUPPORTED;
        codec->decoder = AVI_Decode_V422;
	codec->x = 2 * ((codec->x + 1)/2);
        JPG_Alloc_MCU_Bufs(codec->anim_hdr,codec->x,codec->y,xaTRUE);
	JPG_Setup_Samp_Limit_Table(codec->anim_hdr);
        XA_Gen_YUV_Tabs(codec->anim_hdr);
	break;

    case RIFF_vyuy:
    case RIFF_VYUY:
	codec->compression	= RIFF_VYUY;
	codec->description	= "VYUY";
	codec->xapi_rev = 0x0002;
	ret = CODEC_SUPPORTED;
        codec->decoder = AVI_Decode_VYUY;
	codec->x = 2 * ((codec->x + 1)/2);
        JPG_Alloc_MCU_Bufs(codec->anim_hdr,codec->x,codec->y,xaTRUE);
	JPG_Setup_Samp_Limit_Table(codec->anim_hdr);
        XA_Gen_YUV_Tabs(codec->anim_hdr);
	break;

    case RIFF_YUY2:
	codec->compression	= RIFF_YUY2;
	codec->description	= "YUY2 4:2:2";
	codec->xapi_rev = 0x0002;
	ret = CODEC_SUPPORTED;
        codec->decoder = AVI_Decode_YUY2;
	codec->x = 2 * ((codec->x + 1)/2);
        JPG_Alloc_MCU_Bufs(codec->anim_hdr,codec->x,codec->y,xaTRUE);
	JPG_Setup_Samp_Limit_Table(codec->anim_hdr);
        XA_Gen_YUV_Tabs(codec->anim_hdr);
	break;

    case RIFF_y41p:
    case RIFF_Y41P:
	codec->compression	= RIFF_Y41P;
	codec->description	= "Y41P";
	codec->xapi_rev = 0x0002;
	ret = CODEC_SUPPORTED;
        codec->decoder = AVI_Decode_Y41P;
	codec->x = 2 * ((codec->x + 1)/2);
	codec->y = 2 * ((codec->y + 1)/2);
        JPG_Alloc_MCU_Bufs(codec->anim_hdr,codec->x,codec->y,xaTRUE);
	JPG_Setup_Samp_Limit_Table(codec->anim_hdr);
        XA_Gen_YUV_Tabs(codec->anim_hdr);
	break;

    case RIFF_yv12:
    case RIFF_YV12:
	codec->compression	= RIFF_YV12;
	codec->description	= "YV12";
	codec->xapi_rev = 0x0002;
	ret = CODEC_SUPPORTED;
        codec->decoder = AVI_Decode_YV12;
	codec->x = 2 * ((codec->x + 1)/2);
	codec->y = 2 * ((codec->y + 1)/2);
        JPG_Alloc_MCU_Bufs(codec->anim_hdr,codec->x,codec->y,xaTRUE);
	JPG_Setup_Samp_Limit_Table(codec->anim_hdr);
        XA_Gen_YUV_Tabs(codec->anim_hdr);
	break;

#ifdef NO_CLUE_JUST_PUTZIN_AROUND
    case RIFF_vixl:
    case RIFF_VIXL:
	codec->compression	= RIFF_VIXL;
	codec->description	= "VIXL";
	codec->xapi_rev = 0x0002;
	ret = CODEC_SUPPORTED;
        codec->decoder = AVI_Decode_VIXL;
	codec->x = 2 * ((codec->x + 1)/2);
        JPG_Alloc_MCU_Bufs(codec->anim_hdr,codec->x,codec->y,xaTRUE);
	JPG_Setup_Samp_Limit_Table(codec->anim_hdr);
        XA_Gen_YUV_Tabs(codec->anim_hdr);
	break;
#endif

    case RIFF_iyuv:    /* rumor has it that IYUV is same as I420  */
    case RIFF_IYUV:
    case RIFF_i420:
    case RIFF_I420:
	codec->compression	= RIFF_I420;
	codec->description	= "IYUV/I420";
	codec->xapi_rev = 0x0002;
	ret = CODEC_SUPPORTED;
        codec->decoder = AVI_Decode_I420;
	codec->x = 2 * ((codec->x + 1)/2);
	codec->y = 2 * ((codec->y + 1)/2);
        JPG_Alloc_MCU_Bufs(codec->anim_hdr,codec->x,codec->y,xaTRUE);
	JPG_Setup_Samp_Limit_Table(codec->anim_hdr);
        XA_Gen_YUV_Tabs(codec->anim_hdr);
	break;


#ifdef NOT_FULLY_FIGURED_OUT_YET
    case RIFF_AURA:
	codec->compression	= RIFF_AURA;
	codec->description	= "Aura";
	codec->xapi_rev = 0x0002;
	if (codec->depth == 16)
	{
	  ret = CODEC_SUPPORTED;
          codec->decoder = AVI_Decode_ATEST;
	  codec->x = 4 * ((codec->x + 3)/4);
          JPG_Alloc_MCU_Bufs(codec->anim_hdr,codec->x,codec->y,xaTRUE);
	  JPG_Setup_Samp_Limit_Table(codec->anim_hdr);
          XA_Gen_YUV_Tabs(codec->anim_hdr);
	}
	else ret = CODEC_UNSUPPORTED;
	break;
#endif

    case RIFF_azpr:
    case RIFF_rpza:
	codec->compression	= RIFF_rpza;
	codec->description	= "Apple Video(RPZA)";
	ret = CODEC_SUPPORTED;
	if ( (codec->depth == 16) || (codec->depth == 24))
	{ codec->decoder = QT_Decode_RPZA;
	  codec->x = 4 * ((codec->x + 3)/4);
	  codec->y = 4 * ((codec->y + 3)/4);
	  JPG_Setup_Samp_Limit_Table(codec->anim_hdr);
	}
	else ret = CODEC_UNSUPPORTED;
	break;

    case RIFF_mJPG:
    case RIFF_MJPG:
	codec->compression	= RIFF_MJPG;
	codec->description	= "Motion JPEG";
	codec->xapi_rev = 0x0002;
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
xaLONG AVI_UNK_Codec_Query(codec)
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
    case RIFF_m263:
    case RIFF_M263:
	codec->compression	= RIFF_M263;
	codec->description	= "CCITT H.263";
	ret = CODEC_UNSUPPORTED;
	break;
    case RIFF_rt21: 
    case RIFF_RT21: 
	codec->compression	= RIFF_RT21;
	codec->description	= "Intel Indeo R2.1";
	ret = CODEC_UNSUPPORTED;
	break;
    case RIFF_iv41: 
    case RIFF_IV41: 
	codec->compression	= RIFF_IV41;
	codec->description	= "Intel Indeo R4.1";
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
    case RIFF_cyuv: 
	codec->compression	= RIFF_cyuv;
	codec->description	= "Creative Tech CYUV";
	ret = CODEC_UNSUPPORTED;
	break;
    case RIFF_VDOW: 
	codec->compression	= RIFF_VDOW;
	codec->description	= "VDONet Video";
	ret = CODEC_UNSUPPORTED;
	break;
    case RIFF_vixl: 
    case RIFF_VIXL: 
	codec->compression	= RIFF_VIXL;
	codec->description	= "VIXL";
	ret = CODEC_UNSUPPORTED;
	break;
    case RIFF_MVI1:
    case RIFF_mvi1:
    case RIFF_MPIX:
	codec->compression	= RIFF_MVI1;
	codec->description	= "Motion Pixels";
	ret = CODEC_UNSUPPORTED;
	break;

    case RIFF_Q1_0:
	codec->description	= "Q-Team QPEG";
	ret = CODEC_UNSUPPORTED;
	break;

    case RIFF_YUV8:
	codec->description	= "Winnov YUV8";
	ret = CODEC_UNSUPPORTED;
	break;
    case RIFF_WINX:
	codec->description	= "Winnov WINX";
	ret = CODEC_UNSUPPORTED;
	break;
    case RIFF_WPY2:
	codec->description	= "Winnov WPY2 pyramid 2";
	ret = CODEC_UNSUPPORTED;
	break;
    case RIFF_SFMC:
	codec->description	= "Crystal Net SFM";
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
static xaULONG AVI_JPEG_Read_Ext(xin,anim_hdr)
XA_INPUT *xin;
void *anim_hdr;
{ xaULONG offset, jsize, format, cspace, bits, hsubsamp,vsubsamp;
  offset   = xin->Read_LSB_U32(xin);
  jsize    = xin->Read_LSB_U32(xin);
  format   = xin->Read_LSB_U32(xin);
  cspace   = xin->Read_LSB_U32(xin);
  bits     = xin->Read_LSB_U32(xin);
  hsubsamp = xin->Read_LSB_U32(xin);
  vsubsamp = xin->Read_LSB_U32(xin);

  DEBUG_LEVEL1 
  {
    fprintf(stdout,"M/JPG: offset %x jsize %x format %x ",
						offset,jsize,format);
    fprintf(stdout,"cspace %x bits %x hsub %x vsub %x\n", 
					cspace,bits,hsubsamp,vsubsamp);
  }
  return(28);  /* return number of bytes read */
}

/****************************
 * Function for reading the JPEG and MJPG AVI header extension.
 *
 ****************/
static xaULONG AVI_IJPG_Read_Ext(xin,anim_hdr)
XA_INPUT *xin;
void *anim_hdr;
{ xaULONG offset, jsize, format, cspace, ret_size;
  offset = xin->Read_LSB_U32(xin);
  jsize  = xin->Read_LSB_U32(xin);
  format = xin->Read_LSB_U32(xin);
  cspace = xin->Read_LSB_U32(xin);
  DEBUG_LEVEL1
  {
    fprintf(stdout,"IJPG: offset %x jsize %x format %x cspace %x\n",
						offset,jsize,format,cspace);
  }
  ret_size = 16;
  JFIF_Read_IJPG_Tables(xin);
  ret_size += 128;
  return(ret_size);
}

	  /*** Check if Stream chunk or handle other unknown chunks */
xaULONG AVI_Stream_Chunk(xin,anim_hdr,avi,ck_id,ck_size,fname)
XA_INPUT *xin;
XA_ANIM_HDR *anim_hdr;
XA_ANIM_SETUP *avi;
xaLONG ck_id;
xaLONG ck_size;
char *fname;
{ xaULONG stream_id = ck_id & RIFF_FF00;
  xaULONG stream_type,stream_ok;
  xaLONG  stream_num = -1;
  XA_ACTION *act;

  /*** Potentially get stream_type */
  switch(stream_id)
  {
    case RIFF_00:	stream_num = 0; break;
    case RIFF_01:	stream_num = 1; break;
    case RIFF_02:	stream_num = 2; break;
    case RIFF_03:	stream_num = 3; break;
    case RIFF_04:	stream_num = 4; break;
    case RIFF_05:	stream_num = 5; break;
    case RIFF_06:	stream_num = 6; break;
    case RIFF_07:	stream_num = 7; break;
    default: 		stream_num = -1; break;
  }
  if (stream_num >= 0)
  { stream_type = avi_stream_type[stream_num];
    stream_ok   = avi_stream_ok[stream_num]; 
  }
  else { stream_type = 0; stream_ok   = xaFALSE; }

/* POD exit early if stream not OK - re do this mess */

  switch(stream_type)
  {
    case RIFF_vids:
    if (stream_ok == xaFALSE)  /* ignore chunk */
    { if (ck_size & 0x01) ck_size++;
      xin->Seek_FPos(xin,ck_size,1);
    }
    else
    { act = ACT_Get_Action(anim_hdr,ACT_DELTA);
      { ACT_DLTA_HDR *dlta_hdr = 
	AVI_Read_00DC(xin,avi,ck_size,act,-1,stream_num);
	if (act->type == ACT_NOP) break;
	if (dlta_hdr == 0)        return(xaFALSE);
	ACT_Setup_Delta(avi,act,dlta_hdr,xin);
      }
    }
    break;

    case RIFF_auds:
    { xaULONG snd_size = ck_size;
      auds_hdr.byte_cnt += ck_size;
      if (ck_size & 0x01) ck_size++;
      if (ck_size == 0) break;
      if ((avi_audio_attempt==xaTRUE) && (stream_ok==xaTRUE))
      { xaLONG ret;
	if (xa_file_flag==xaTRUE)
	{ xaLONG rets, cur_fpos = xin->Get_FPos(xin);
	  rets = XA_Add_Sound(anim_hdr,0,avi_audio_type, 
				cur_fpos, avi_audio_freq, snd_size, 
				&avi->aud_time,&avi->aud_timelo,
				auds_hdr.blockalign, auds_hdr.samps_block);
	  if (rets==xaFALSE) avi_audio_attempt = xaFALSE;
	  xin->Seek_FPos(xin,ck_size,1); /* move past this chunk */
	  if (snd_size > avi->max_faud_size) avi->max_faud_size = snd_size;
	}
	else
	{ xaUBYTE *snd = (xaUBYTE *)malloc(ck_size);
	  if (snd==0) TheEnd1("AVI: snd malloc err");
	  ret = xin->Read_Block(xin, snd, ck_size);
	  if (ret != ck_size) fprintf(stdout,"AVI: snd rd err\n");
	  else
	  { int rets; /*NOTE: don't free snd */
	    rets = XA_Add_Sound(anim_hdr,snd,avi_audio_type, -1,
				avi_audio_freq, snd_size, 
				&avi->aud_time, &avi->aud_timelo,
				auds_hdr.blockalign, auds_hdr.samps_block);
	    if (rets==xaFALSE) avi_audio_attempt = xaFALSE;
	  }
	}
	if (avi_audio_attempt == xaTRUE) avi_has_audio = xaTRUE;
      } /* valid audio */
      else xin->Seek_FPos(xin,ck_size,1);
    }
    break;/* break out - spotlights, sirens, rifles - but he...*/

    case 0x00:   /* NOT A STREAM */
      DEBUG_LEVEL2 
      {
        fprintf(stdout,"AVI: unknown chunk ");
        AVI_Print_ID(stdout,ck_id);
        fprintf(stdout,"\n");
      }
      if (ck_size & 0x01) ck_size++;
      xin->Seek_FPos(xin,ck_size,1); /* move past this chunk */
      break;

    /* Unsupported or Unknown stream */
    case RIFF_pads:
    case RIFF_txts:
    default:
      if (ck_size & 0x01) ck_size++;
      xin->Seek_FPos(xin,ck_size,1); /* move past this chunk */
      break;
  } /* end of switch */
  return(xaTRUE);
} 


/* POD todo */
	  /*** Check if Stream chunk or handle other unknown chunks */

xaULONG AVI_Read_IDX1(xin,anim_hdr,avi,ck_size,fname)
XA_INPUT *xin;
XA_ANIM_HDR *anim_hdr;
XA_ANIM_SETUP *avi;
xaLONG ck_size;
char *fname;
{ xaULONG i,cnt, minoff;
  xaULONG stream_id, stream_type, stream_ok;
  XA_ACTION *act;
  AVI_INDEX_ENTRY *idx;
  xaLONG tmp_fpos;
  xaLONG stream_num = -1;

  cnt = ck_size >> 4;
  idx = (AVI_INDEX_ENTRY *)malloc(cnt * sizeof(AVI_INDEX_ENTRY));
  if (idx == 0) TheEnd1("AVI: malloc err");

DEBUG_LEVEL1 fprintf(stdout,"IDX1 cnt = %d\n",cnt);

  minoff = 0xffffffff;
  for(i=0; i<cnt; i++)
  { idx[i].ckid		= xin->Read_MSB_U32(xin);
    idx[i].flags	= xin->Read_LSB_U32(xin);
    idx[i].offset	= xin->Read_LSB_U32(xin);
    idx[i].size		= xin->Read_LSB_U32(xin);
    if (idx[i].offset < minoff) minoff = idx[i].offset;

    DEBUG_LEVEL2 fprintf(stdout,"CKID %x off %x size %x\n",
			idx[i].ckid, idx[i].offset, idx[i].size);
  }

  /* The IDX1 chunk either references from the start of the file
   * OR from the start of the MOVI LIST. How do you tell which 
   * to do, you ask? You can't. You just have to take an educated
   * guess.
   */
  if (minoff >= avi_movi_offset) avi_movi_offset = 0;
  avi_movi_offset += 8;  /* skip over chunk's id and size */

 /* just in case extra bytes */
  ck_size -= (cnt << 4);
  while(ck_size--) (void)(xin->Read_U8(xin));
  tmp_fpos = xin->Get_FPos(xin);  /* save for later */

DEBUG_LEVEL2 fprintf(stdout,"movi_offset = %x\n",avi_movi_offset);

  for(i=0; i<cnt; i++)
  {
    idx[i].offset += avi_movi_offset;
    
    stream_id 		= idx[i].ckid & RIFF_FF00;
    /*** Potentially get stream_type */
   switch(stream_id)
   {
     case RIFF_00:       stream_num = 0; break;
     case RIFF_01:       stream_num = 1; break;
     case RIFF_02:       stream_num = 2; break;
     case RIFF_03:       stream_num = 3; break;
     case RIFF_04:       stream_num = 4; break;
     case RIFF_05:       stream_num = 5; break;
     case RIFF_06:       stream_num = 6; break;
     case RIFF_07:       stream_num = 7; break;
     default:            stream_num = -1; break;
   }
   if (stream_num >= 0)
   { stream_type = avi_stream_type[stream_num];
     stream_ok   = avi_stream_ok[stream_num];
   }
   else { stream_type = 0; stream_ok   = xaFALSE; }

    if (stream_ok == xaFALSE) continue; /* POD NEW: back to for( */

    switch(stream_type)
    {
      case RIFF_vids:
      { act = ACT_Get_Action(anim_hdr,ACT_DELTA);
        { ACT_DLTA_HDR *dlta_hdr = 
	  	AVI_Read_00DC(xin,avi, idx[i].size, act, idx[i].offset,
				stream_num);
	  if (act->type == ACT_NOP) break;
	  if (dlta_hdr == 0)        return(xaFALSE);
	  ACT_Setup_Delta(avi,act,dlta_hdr,xin);
        }
      }
      break;

      case RIFF_auds:
      { xaULONG snd_size = idx[i].size;
        auds_hdr.byte_cnt += idx[i].size;
        if (avi_audio_attempt==xaTRUE)
        { xaLONG ret;
	  if (xa_file_flag==xaTRUE)
	  { xaLONG rets, cur_fpos = idx[i].offset;
	    rets = XA_Add_Sound(anim_hdr,0,avi_audio_type, 
				cur_fpos, avi_audio_freq, snd_size, 
				&avi->aud_time,&avi->aud_timelo,
				auds_hdr.blockalign, auds_hdr.samps_block);
	    if (rets==xaFALSE) avi_audio_attempt = xaFALSE;
	    if (snd_size > avi->max_faud_size) avi->max_faud_size = snd_size;
	  }
	  else
	  { xaUBYTE *snd = (xaUBYTE *)malloc(snd_size);
	    if (snd==0) TheEnd1("AVI: snd malloc err");

	    ret = xin->Seek_FPos(xin, idx[i].offset, 0);

	    DEBUG_LEVEL2 fprintf(stdout,"AUDS offset %x size %x ret %x\n",
			idx[i].offset,snd_size,ret);

	    ret = xin->Read_Block(xin, snd, snd_size);
	    if (ret != snd_size) fprintf(stdout,"AVI: snd rd err\n");
	    else
	    { int rets; /*NOTE: don't free snd */
	      rets = XA_Add_Sound(anim_hdr,snd,avi_audio_type, -1,
				avi_audio_freq, snd_size, 
				&avi->aud_time, &avi->aud_timelo,
				auds_hdr.blockalign, auds_hdr.samps_block);
	      if (rets==xaFALSE) avi_audio_attempt = xaFALSE;
	    }
	  }
        } /* valid audio */
	if (avi_audio_attempt == xaTRUE) avi_has_audio = xaTRUE;
      }
      break;/* break out - spotlights, sirens, rifles - but he...*/
    } /* end of switch */
  }
  free(idx);
  xin->Seek_FPos(xin, tmp_fpos, 0);
  return(xaTRUE);
}

xaULONG AVI_Decode_YUY2(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC2_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;
  xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  XA_CHDR *chdr = dec_info->chdr;
  xaUBYTE *dptr = delta;
  xaLONG ycnt;
  void (*color_func)() = (void (*)())XA_YUV211111_Func(dec_info->image_type);
  xaUBYTE *yp,*up,*vp;

  dec_info->xs = dec_info->ys = 0;
  dec_info->xe = imagex; dec_info->ye = imagey;
  if (dec_info->skip_flag > 0) return(ACT_DLTA_DROP);
  if (chdr) { if (chdr->new_chdr) chdr = chdr->new_chdr; }

  yp = jpg_YUVBufs.Ybuf; yp += (imagey - 1) * imagex;
  up = jpg_YUVBufs.Ubuf; up += (imagey - 1) * (imagex >> 1);
  vp = jpg_YUVBufs.Vbuf; vp += (imagey - 1) * (imagex >> 1);

  ycnt = imagey;
  while( (ycnt > 0) && (dsize > 0) )
  {
    xaLONG x = imagex >> 1;

    while(x--)
    { *yp++ = *dptr++; *up++ = (*dptr++);
      *yp++ = *dptr++; *vp++ = (*dptr++);  dsize -= 4;
    }
    ycnt--;
    yp -= (imagex << 1);
    up -=  imagex;
    vp -=  imagex;
  }
  color_func(image,imagex,imagey,imagex,imagey,
                &jpg_YUVBufs, &def_yuv_tabs, map_flag, map, chdr);

  if (map_flag) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}


xaULONG AVI_Decode_YV12(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC2_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;
  xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  XA_CHDR *chdr = dec_info->chdr;
  xaUBYTE *dptr = delta;
  void (*color_func)() = (void (*)())XA_YUV221111_Func(dec_info->image_type);
  xaLONG  i,plane_sz;
  xaUBYTE *yp,*up,*vp;

  dec_info->xs = dec_info->ys = 0;
  dec_info->xe = imagex; dec_info->ye = imagey;
  if (dec_info->skip_flag > 0) return(ACT_DLTA_DROP);
  if (chdr) { if (chdr->new_chdr) chdr = chdr->new_chdr; }


	/* Read Y Plane */
  yp = jpg_YUVBufs.Ybuf;
  plane_sz = imagex * imagey;
  for(i = 0; i < plane_sz; i++)  *yp++ = *dptr++;
  
	/* Read V Plane */
  vp = jpg_YUVBufs.Vbuf;
  plane_sz = (imagex * imagey) >> 2;
  for(i = 0; i < plane_sz; i++)  *vp++ = *dptr++;

	/* Read U Plane */
  up = jpg_YUVBufs.Ubuf;
  for(i = 0; i < plane_sz; i++)  *up++ = *dptr++;

  color_func(image,imagex,imagey,imagex,imagey,
                &jpg_YUVBufs, &def_yuv_tabs, map_flag, map, chdr);

  if (map_flag) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}


xaULONG AVI_Decode_I420(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC2_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;
  xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  XA_CHDR *chdr = dec_info->chdr;
  xaUBYTE *dptr = delta;
  void (*color_func)() = (void (*)())XA_YUV221111_Func(dec_info->image_type);
  xaLONG  i,plane_sz;
  xaUBYTE *yp,*up,*vp;

  dec_info->xs = dec_info->ys = 0;
  dec_info->xe = imagex; dec_info->ye = imagey;
  if (dec_info->skip_flag > 0) return(ACT_DLTA_DROP);
  if (chdr) { if (chdr->new_chdr) chdr = chdr->new_chdr; }


	/* Read Y Plane */
  yp = jpg_YUVBufs.Ybuf;
  plane_sz = imagex * imagey;
  for(i = 0; i < plane_sz; i++)  *yp++ = *dptr++;
  
	/* Read U Plane */
  plane_sz = (imagex * imagey) >> 2;
  up = jpg_YUVBufs.Ubuf;
  for(i = 0; i < plane_sz; i++)  *up++ = *dptr++;

	/* Read V Plane */
  vp = jpg_YUVBufs.Vbuf;
  for(i = 0; i < plane_sz; i++)  *vp++ = *dptr++;

  color_func(image,imagex,imagey,imagex,imagey,
                &jpg_YUVBufs, &def_yuv_tabs, map_flag, map, chdr);

  if (map_flag) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}


xaULONG AVI_Decode_Y41P(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC2_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;
  xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  XA_CHDR *chdr = dec_info->chdr;
  xaUBYTE *dptr = delta;
  void (*color_func)() = (void (*)())XA_YUV221111_Func(dec_info->image_type);
  xaLONG  j,y_row_sz, uv_row_sz;
  xaUBYTE *yp,*up,*vp;

  dec_info->xs = dec_info->ys = 0;
  dec_info->xe = imagex; dec_info->ye = imagey;
  if (dec_info->skip_flag > 0) return(ACT_DLTA_DROP);
  if (chdr) { if (chdr->new_chdr) chdr = chdr->new_chdr; }


	/* Read Y Plane */
  y_row_sz	= imagex;
  uv_row_sz	= imagex >> 2;

  yp = jpg_YUVBufs.Ybuf + ((imagey - 1) * y_row_sz);
  up = jpg_YUVBufs.Ubuf + ((imagey - 1) * uv_row_sz);
  vp = jpg_YUVBufs.Vbuf + ((imagey - 1) * uv_row_sz);

  for(j = 0; j < imagey; j++)  
  { int i;

    for(i = 0; i < imagex; i+=8)  
    {
      *up++ = *dptr++;
      *yp++ = *dptr++;
      *vp++ = *dptr++;
      *yp++ = *dptr++;

      *up++ = *dptr++;
      *yp++ = *dptr++;
      *vp++ = *dptr++;
      *yp++ = *dptr++;

      *yp++ = *dptr++;
      *yp++ = *dptr++;
      *yp++ = *dptr++;
      *yp++ = *dptr++;
    }
    yp -= (y_row_sz << 1);
    up -= (uv_row_sz << 1);
    vp -= (uv_row_sz << 1);
  }

  color_func(image,imagex,imagey,imagex,imagey,
                &jpg_YUVBufs, &def_yuv_tabs, map_flag, map, chdr);

  if (map_flag) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}


xaULONG AVI_Decode_V422(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC2_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;
  xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  XA_CHDR *chdr = dec_info->chdr;
  xaUBYTE *dptr = delta;
  void (*color_func)() = (void (*)())XA_YUV211111_Func(dec_info->image_type);
  xaLONG  ycnt;
  xaUBYTE *yp,*up,*vp;

  dec_info->xs = dec_info->ys = 0;
  dec_info->xe = imagex; dec_info->ye = imagey;
  if (dec_info->skip_flag > 0) return(ACT_DLTA_DROP);
  if (chdr) { if (chdr->new_chdr) chdr = chdr->new_chdr; }

  yp = jpg_YUVBufs.Ybuf; yp += (imagey - 1) * imagex;
  up = jpg_YUVBufs.Ubuf; up += (imagey - 1) * (imagex >> 1);
  vp = jpg_YUVBufs.Vbuf; vp += (imagey - 1) * (imagex >> 1);

  ycnt = imagey;
  while((ycnt--) && (dsize > 0))
  { int x = imagex >> 1;

    while(x--)
    {  /* Y0 U Y1 V  BUT little endian 16 bit swapped */
      *up++ = *dptr++;
      *yp++ = *dptr++;
      *vp++ = *dptr++;
      *yp++ = *dptr++;
      dsize -= 4; 
    }
    yp -= (imagex << 1);
    up -= imagex;
    vp -= imagex;
  }
  
  color_func(image,imagex,imagey,imagex,imagey,
                &jpg_YUVBufs, &def_yuv_tabs, map_flag, map, chdr);

  if (map_flag) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}


xaULONG AVI_Decode_VYUY(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC2_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;
  xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  XA_CHDR *chdr = dec_info->chdr;
  xaUBYTE *dptr = delta;
  void (*color_func)() = (void (*)())XA_YUV211111_Func(dec_info->image_type);
  xaLONG  j,y_row_sz, uv_row_sz;
  xaUBYTE *yp,*up,*vp;

  dec_info->xs = dec_info->ys = 0;
  dec_info->xe = imagex; dec_info->ye = imagey;
  if (dec_info->skip_flag > 0) return(ACT_DLTA_DROP);
  if (chdr) { if (chdr->new_chdr) chdr = chdr->new_chdr; }

	/* Read Y Plane */
  y_row_sz	= imagex;
  uv_row_sz	= imagex >> 1;

  yp = jpg_YUVBufs.Ybuf;
  up = jpg_YUVBufs.Ubuf;
  vp = jpg_YUVBufs.Vbuf;

  for(j = 0; j < imagey; j++)  
  { int i;
    for(i = 0; i < imagex; i+=2)  
    {   /* V Y U Y */
      yp[0] = *dptr++;
      *up++ = *dptr++;
      yp[1]   = *dptr++;
      *vp++ = *dptr++;
      yp += 2;
    }
  }
  color_func(image,imagex,imagey,imagex,imagey,
                &jpg_YUVBufs, &def_yuv_tabs, map_flag, map, chdr);

  if (map_flag) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}

#ifdef NO_CLUE_JUST_PUTZIN_AROUND
xaULONG AVI_Decode_VIXL(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC2_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;
  xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG special = dec_info->special & 0x0001;
  XA_CHDR *chdr = dec_info->chdr;
  xaUBYTE *dptr = delta;
  void (*color_func)() = (void (*)())XA_YUV211111_Func(dec_info->image_type);
  xaLONG  j,y_row_sz, uv_row_sz;
  xaUBYTE *yp,*up,*vp;

  dec_info->xs = dec_info->ys = 0;
  dec_info->xe = imagex; dec_info->ye = imagey;
  if (dec_info->skip_flag > 0) return(ACT_DLTA_DROP);
  if (chdr) { if (chdr->new_chdr) chdr = chdr->new_chdr; }

	/* Read Y Plane */
  y_row_sz	= imagex;
  uv_row_sz	= imagex >> 1;

  yp = jpg_YUVBufs.Ybuf;
  up = jpg_YUVBufs.Ubuf;
  vp = jpg_YUVBufs.Vbuf;

  for(j = 0; j < imagey; j++)  
  { int i;
    for(i = 0; i < imagex; i+=2)  
    {   /* V Y U Y */
      xaULONG a,b;
      a = *dptr++;
      b = *dptr++;
      *up++ = 0x80;
      *vp++ = 0x80;

      y[0] = 
      y[1] = 
      yp += 2;
    }
  }
  color_func(image,imagex,imagey,imagex,imagey,
                &jpg_YUVBufs, &def_yuv_tabs, map_flag, map, chdr);

  if (map_flag) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}
#endif

