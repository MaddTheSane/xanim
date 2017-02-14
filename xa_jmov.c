
/*
 * xa_jmov.c
 *
 * Copyright (C) 1995 by Mark Podlipec. 
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
 *
 ********************************/


#include "xa_jmov.h" 

xaLONG Is_JMOV_File();
xaULONG JMOV_Read_File();
JMOV_FRAME *JMOV_Add_Frame();
void JMOV_Free_Frame_List();
void ACT_Setup_Delta();
xaULONG JMOV_Read_Header();
void JMOV_Read_Index();
void JMOV_Read_Frame();


/* CODEC ROUTINES */
xaULONG JFIF_Decode_JPEG();
void JFIF_Read_IJPG_Tables();
extern void XA_Gen_YUV_Tabs();
extern JPG_Alloc_MCU_Bufs();
extern jpg_search_marker();
extern IJPG_Tab1[64];
extern IJPG_Tab2[64];
extern JFIF_Init_IJPG_Tables();

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
xaULONG UTIL_Get_MSB_UShort();
void  UTIL_FPS_2_Time();
extern XA_ANIM_SETUP *XA_Get_Anim_Setup();
void XA_Free_Anim_Setup();


xaULONG jmov_audio_attempt;
xaULONG jmov_audio_type;
xaULONG jmov_audio_freq;
xaULONG jmov_audio_chans;
xaULONG jmov_audio_bps;
xaULONG XA_Add_Sound();


xaULONG jmov_frame_cnt;
JMOV_FRAME *jmov_frame_start,*jmov_frame_cur;

JMOV_FRAME *JMOV_Add_Frame(time,timelo,act)
xaULONG time,timelo;
XA_ACTION *act;
{
  JMOV_FRAME *fframe;
 
  fframe = (JMOV_FRAME *) malloc(sizeof(JMOV_FRAME));
  if (fframe == 0) TheEnd1("JMOV_Add_Frame: malloc err");
 
  fframe->time   = time;
  fframe->timelo = timelo;
  fframe->act = act;
  fframe->next = 0;
 
  if (jmov_frame_start == 0) jmov_frame_start = fframe;
  else jmov_frame_cur->next = fframe;
 
  jmov_frame_cur = fframe;
  jmov_frame_cnt++;
  return(fframe);
}

void JMOV_Free_Frame_List(fframes)
JMOV_FRAME *fframes;
{
  JMOV_FRAME *ftmp;
  while(fframes != 0)
  {
    ftmp = fframes;
    fframes = fframes->next;
    FREE(ftmp,0xA000);
  }
}

#define JMOV_j_mo 0x6A5F6D6F
#define JMOV_vie  0x76696500


/*
 *
 */
xaLONG Is_JMOV_File(filename)
char *filename;
{
  FILE *fin;
  xaULONG data0,data1;

  if ( (fin=fopen(filename,XA_OPEN_MODE)) == 0) return(xaNOFILE);
  data0 = UTIL_Get_MSB_Long(fin);
  data1 = (UTIL_Get_MSB_Long(fin)) & 0xffffff00;
 
  fclose(fin);
  if ( (data0 == JMOV_j_mo) && (data1 == JMOV_vie)) return(xaTRUE);
  return(xaFALSE);
}


xaULONG JMOV_Read_File(fname,anim_hdr,audio_attempt)
char *fname;
XA_ANIM_HDR *anim_hdr;
xaULONG audio_attempt;	/* xaTRUE if audio is to be attempted */
{
  FILE *fin;
  xaLONG i,t_time;
  xaULONG t_timelo;
  XA_ACTION *act;
  XA_ANIM_SETUP *jmov;
  JMOV_HDR   jmov_hdr;
 
  if ( (fin=fopen(fname,XA_OPEN_MODE)) == 0)
  {
    fprintf(stderr,"can't open JMOV File %s for reading\n",fname);
    return(xaFALSE);
  }

  jmov = XA_Get_Anim_Setup();
  jmov->vid_time = XA_GET_TIME( 100 ); /* default */

  jmov_frame_cnt	= 0;
  jmov_frame_start	= 0;
  jmov_frame_cur	= 0;
  jmov_audio_attempt	= audio_attempt;

  if (JMOV_Read_Header(fin,anim_hdr,jmov,&jmov_hdr) == xaFALSE)
  {
    fprintf(stderr,"JMOV: read header error\n");
    fclose(fin);
    return(xaFALSE);
  }

  JMOV_Read_Index(fin,anim_hdr,jmov,&jmov_hdr);


  if (xa_verbose) 
  {
    fprintf(stderr,"JMOV %ldx%ldx%ld frames %ld\n",
		jmov->imagex,jmov->imagey,jmov->imagec,jmov_frame_cnt);
  }
  if (jmov_frame_cnt == 0)
  { 
    fprintf(stderr,"JMOV: No supported video frames exist in this file.\n");
    return(xaFALSE);
  }

  anim_hdr->frame_lst = (XA_FRAME *)
				malloc( sizeof(XA_FRAME) * (jmov_frame_cnt+1));
  if (anim_hdr->frame_lst == NULL) TheEnd1("JMOV_Read_File: frame malloc err");

  jmov_frame_cur = jmov_frame_start;
  i = 0;
  t_time = 0;
  t_timelo = 0;
  while(jmov_frame_cur != 0)
  {
    if (i > jmov_frame_cnt)
    {
      fprintf(stderr,"JMOV_Read_Anim: frame inconsistency %ld %ld\n",
                i,jmov_frame_cnt);
      break;
    }
    anim_hdr->frame_lst[i].time_dur = jmov_frame_cur->time;
    anim_hdr->frame_lst[i].zztime = t_time;
    t_time += jmov_frame_cur->time;
    t_timelo += jmov_frame_cur->timelo;
    while(t_timelo > (1<<24)) {t_time++; t_timelo -= (1<<24);}
    anim_hdr->frame_lst[i].act = jmov_frame_cur->act;
    jmov_frame_cur = jmov_frame_cur->next;
    i++;
  }
  anim_hdr->imagex = jmov->imagex;
  anim_hdr->imagey = jmov->imagey;
  anim_hdr->imagec = jmov->imagec;
  anim_hdr->imaged = 8; /* nop */
  anim_hdr->frame_lst[i].time_dur = 0;
  anim_hdr->frame_lst[i].zztime = -1;
  anim_hdr->frame_lst[i].act  = 0;
  anim_hdr->loop_frame = 0;
  if (xa_buffer_flag == xaFALSE) anim_hdr->anim_flags |= ANIM_SNG_BUF;
  if (xa_file_flag == xaTRUE) anim_hdr->anim_flags |= ANIM_USE_FILE;
  anim_hdr->anim_flags |= ANIM_FULL_IM;
  anim_hdr->max_fvid_size = jmov->max_fvid_size;
  anim_hdr->max_faud_size = jmov->max_faud_size;
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
  JMOV_Free_Frame_List(jmov_frame_start);
  XA_Free_Anim_Setup(jmov);
  return(xaTRUE);
} /* end of read file */


xaULONG JMOV_Read_Header(fin,anim_hdr,jmov,jmov_hdr)
FILE *fin;
XA_ANIM_HDR *anim_hdr;
XA_ANIM_SETUP *jmov;
JMOV_HDR *jmov_hdr;
{ xaULONG t;
  t 			= UTIL_Get_MSB_Long(fin); /* skip 8 bytes of header */
  t 			= UTIL_Get_MSB_Long(fin); 
  jmov_hdr->version	= UTIL_Get_MSB_Long(fin);
  jmov_hdr->fps		= UTIL_Get_MSB_Long(fin);
  jmov_hdr->frames	= UTIL_Get_MSB_Long(fin);
  jmov_hdr->width	= UTIL_Get_MSB_Long(fin);  
  jmov_hdr->height	= UTIL_Get_MSB_Long(fin);
  jmov_hdr->bandwidth	= UTIL_Get_MSB_Long(fin);
  jmov_hdr->qfactor	= UTIL_Get_MSB_Long(fin);
  jmov_hdr->mapsize	= UTIL_Get_MSB_Long(fin);
  jmov_hdr->indexbuf	= UTIL_Get_MSB_Long(fin);
  jmov_hdr->tracks	= UTIL_Get_MSB_Long(fin);
  jmov_hdr->volbase 	= UTIL_Get_MSB_Long(fin); 
  jmov_hdr->audioslice	= UTIL_Get_MSB_Long(fin);
/* Audio_hdr ??? Nice and compatible I see */
  jmov_hdr->freq	= UTIL_Get_MSB_Long(fin);
  jmov_hdr->chans	= UTIL_Get_MSB_Long(fin);
  jmov_hdr->prec	= UTIL_Get_MSB_Long(fin);
  jmov_hdr->codec	= UTIL_Get_MSB_Long(fin);

  UTIL_FPS_2_Time(jmov, ((double)(jmov_hdr->fps)) );

  jmov_audio_freq = jmov_hdr->freq;
  jmov_audio_chans = jmov_hdr->chans;
  jmov_audio_bps   = jmov_hdr->prec;

  jmov_audio_type = XA_AUDIO_INVALID;
  switch(jmov_hdr->codec)
  {
    case JMOV_AUDIO_ENC_NONE:
    case JMOV_AUDIO_ENC_PCM:
        jmov_audio_type = XA_AUDIO_LINEAR;
	break;
    case JMOV_AUDIO_ENC_ULAW:
	jmov_audio_type = XA_AUDIO_ULAW;
	break;
  }
  if ( (jmov_audio_chans > 2) || (jmov_audio_chans == 0) )
					jmov_audio_type = XA_AUDIO_INVALID;
  if ( (jmov_audio_bps > 2) || (jmov_audio_bps == 0) )
					jmov_audio_type = XA_AUDIO_INVALID;

  if (jmov_audio_chans == 2) jmov_audio_type |= XA_AUDIO_STEREO_MSK;
  if (jmov_audio_bps == 2)   
  {
    jmov_audio_type |= XA_AUDIO_BPS_2_MSK;
    jmov_audio_type |= XA_AUDIO_BIGEND_MSK;
  }

  if (jmov_audio_type == XA_AUDIO_INVALID)
  {
    if (jmov_audio_attempt == xaTRUE) 
	fprintf(stderr,"JMOV: Audio Type unsupported %ld \n",jmov_hdr->codec);
    jmov_audio_attempt = xaFALSE;
  }

  /* JFIF_Init_IJPG_Tables( jmov_hdr->qfactor ); */
  JFIF_Init_IJPG_Tables( 1 );

DEBUG_LEVEL1
{
  fprintf(stderr,"JMOV: ver %ld  fps %ld frame %ld  res %ldx%ld\n",
	jmov_hdr->version, jmov_hdr->fps, jmov_hdr->frames,
	jmov_hdr->width, jmov_hdr->height);
  fprintf(stderr,"      qf %ld mapsize %ld indx %lx tracks %ld audsz %lx\n",
	jmov_hdr->qfactor, jmov_hdr->mapsize, jmov_hdr->indexbuf,
	jmov_hdr->tracks, jmov_hdr->audioslice);
}

  jmov->depth = 24;
  jmov->imagex = jmov_hdr->width;
  jmov->imagey = jmov_hdr->height;
  jmov->imagex = 4 * ((jmov->imagex + 3)/4);
  jmov->imagey = 4 * ((jmov->imagey + 3)/4);
  XA_Gen_YUV_Tabs(anim_hdr);
  JPG_Alloc_MCU_Bufs(anim_hdr,jmov->imagex,0,xaFALSE);

  if (   (cmap_true_map_flag == xaFALSE) /* depth 16 and not true_map */
      || (xa_buffer_flag == xaFALSE) )
  {
     if (cmap_true_to_332 == xaTRUE)
             jmov->chdr = CMAP_Create_332(jmov->cmap,&jmov->imagec);
     else    jmov->chdr = CMAP_Create_Gray(jmov->cmap,&jmov->imagec);
  }
  if ( (jmov->pic==0) && (xa_buffer_flag == xaTRUE))
  {
    jmov->pic_size = jmov->imagex * jmov->imagey;
    if ( (cmap_true_map_flag == xaTRUE) && (jmov->depth > 8) )
                jmov->pic = (xaUBYTE *) malloc( 3 * jmov->pic_size );
    else jmov->pic = (xaUBYTE *) malloc( XA_PIC_SIZE(jmov->pic_size) );
    if (jmov->pic == 0) TheEnd1("JMOV_Buffer_Action: malloc failed");
  }
  return(xaTRUE);
}


void JMOV_Read_Index(fin,anim_hdr,jmov,jmov_hdr)
FILE *fin;
XA_ANIM_HDR *anim_hdr;
XA_ANIM_SETUP *jmov;
JMOV_HDR *jmov_hdr;
{ xaLONG ret;
  xaULONG idx_off = jmov_hdr->indexbuf;
  xaULONG idx_cnt = jmov_hdr->frames;
  xaULONG i,*index;
 

  index = (xaULONG *)malloc( (idx_cnt + 1) * sizeof(xaULONG) );

  ret = fseek(fin,idx_off,0);
  for(i=0; i<idx_cnt; i++)
  {
    if (feof(fin))
    {
      fprintf(stderr,"JMOV: index truncated cur %ld tot %ld\n",i,idx_cnt);
      idx_cnt = i;
      break;
    }
    index[i] = UTIL_Get_MSB_Long(fin);
  }
  index[idx_cnt] = 0;

  for(i=0; i<idx_cnt; i++)
  { xaULONG offset = index[i];
    fseek(fin,offset,0);
    JMOV_Read_Frame(fin,anim_hdr,jmov,jmov_hdr); 
  } 
}

#define JMOV_END_FRAME  0xed
#define JMOV_CLR_FRAME  0xee
#define JMOV_AUDIO_0    0xe5
#define JMOV_AUDIO_1    0xe4
#define JMOV_AUDIO_2    0xe3
#define JMOV_AUDIO_3    0xe2
#define JMOV_CMAP       0xd9
#define JMOV_MMAP       0xf8
#define JMOV_FPS        0xdb
#define JMOV_JPEG       0xec


void JMOV_Read_Frame(fin,anim_hdr,jmov,jmov_hdr)
FILE *fin;
XA_ANIM_HDR *anim_hdr;
XA_ANIM_SETUP *jmov;
JMOV_HDR *jmov_hdr;
{ xaULONG exit_flag;

  exit_flag = xaFALSE;
  while( (!feof(fin)) & (exit_flag == xaFALSE))
  { xaULONG type;
    type = fgetc(fin);
    switch(type)
    {
      case JMOV_END_FRAME:
	DEBUG_LEVEL1 fprintf(stderr,"JMOV: END_FRAME\n");
	exit_flag = xaTRUE;
	break;
      case JMOV_CLR_FRAME:
	DEBUG_LEVEL1 fprintf(stderr,"JMOV: CLR_FRAME\n");
	break;
      case JMOV_AUDIO_0:
      case JMOV_AUDIO_1:
      case JMOV_AUDIO_2:
      case JMOV_AUDIO_3:
	{ xaULONG snd_size = jmov_hdr->audioslice;
	DEBUG_LEVEL1 fprintf(stderr,"JMOV: AUDIO %lx\n",type);

	  if (jmov_audio_attempt==xaTRUE)
	  { xaLONG ret;
	    if (xa_file_flag==xaTRUE)
	    { xaLONG rets, cur_fpos = ftell(fin);
	      rets = XA_Add_Sound(anim_hdr,0,jmov_audio_type, cur_fpos,
			jmov_audio_freq, snd_size, 
			&jmov->aud_time,&jmov->aud_timelo);
	      if (rets==xaFALSE) jmov_audio_attempt = xaFALSE;
	      fseek(fin,snd_size,1); /* move past this chunk */
	      if (snd_size > jmov->max_faud_size) 
				jmov->max_faud_size = snd_size;
 
	    }
	    else
	    { xaUBYTE *snd = (xaUBYTE *)malloc(snd_size);
	      if (snd==0) TheEnd1("JMOV: snd malloc err");
	      ret = fread( snd, snd_size, 1, fin);
	      if (ret != 1) fprintf(stderr,"JMOV: snd rd err\n");
	      else
	      { int rets;
	        rets = XA_Add_Sound(anim_hdr,snd,jmov_audio_type, -1,
                                        jmov_audio_freq, snd_size,
                                        &jmov->aud_time, &jmov->aud_timelo);
	        if (rets==xaFALSE) jmov_audio_attempt = xaFALSE;
	      }
	    }
	  }
	  else fseek(fin,snd_size,1); /* skip over */
	}
	break;
      case JMOV_CMAP:
	{ xaULONG size = jmov_hdr->mapsize * 3;
	  DEBUG_LEVEL1 fprintf(stderr,"JMOV: CMAP - ignored\n");
          if (jmov_hdr->mapsize > (16*1024) )
	  {
	    fprintf(stderr,"JMOV: CMAP just too big %ld\n",jmov_hdr->mapsize);
	    return;
	  }
          fseek(fin,size,1);  /* skip over */
	}
	break;
      case JMOV_MMAP:
	{ xaULONG cnt;
	  DEBUG_LEVEL1 fprintf(stderr,"JMOV: MMAP - ignored\n");

          cnt = fgetc(fin);
	  while(cnt--)
          { xaULONG slot,r,g,b;
	    slot = UTIL_Get_MSB_UShort();
	    r = fgetc(fin); g = fgetc(fin); b = fgetc(fin);
          }
        }
	break;
      case JMOV_FPS:
        { xaULONG fps;
	  DEBUG_LEVEL1 fprintf(stderr,"JMOV: FPS\n");
	  fps = UTIL_Get_MSB_Long(fin);
	  UTIL_FPS_2_Time(jmov, ((double)(fps)) );
        }
	break;
      case JMOV_JPEG:
        { xaULONG dlta_len;
	  XA_ACTION *act;
          ACT_DLTA_HDR *dlta_hdr;

	  DEBUG_LEVEL1 fprintf(stderr,"JMOV: JPEG\n");
	  dlta_len = UTIL_Get_MSB_Long(fin);

	  act = ACT_Get_Action(anim_hdr,ACT_DELTA);
	  if (xa_file_flag == xaTRUE)
	  {
	    dlta_hdr = (ACT_DLTA_HDR *) malloc(sizeof(ACT_DLTA_HDR));
	    if (dlta_hdr == 0) TheEnd1("JMOV: dlta malloc err");
	    act->data = (xaUBYTE *)dlta_hdr;
	    dlta_hdr->flags = ACT_SNGL_BUF;
	    dlta_hdr->fsize = dlta_len;
	    dlta_hdr->fpos  = ftell(fin);
	    if (dlta_len > jmov->max_fvid_size) jmov->max_fvid_size = dlta_len;
	    fseek(fin,dlta_len,1);
	  }
	  else
	  { xaULONG d; xaLONG ret;
	    d = dlta_len + (sizeof(ACT_DLTA_HDR));
	    dlta_hdr = (ACT_DLTA_HDR *) malloc( d );
	    if (dlta_hdr == 0) TheEnd1("QT rle: malloc failed");
	    act->data = (xaUBYTE *)dlta_hdr;
	    dlta_hdr->flags = ACT_SNGL_BUF | DLTA_DATA;
	    dlta_hdr->fpos = 0; dlta_hdr->fsize = dlta_len;
	    ret = fread( dlta_hdr->data, dlta_len, 1, fin);
	    if (ret != 1) { fprintf(stderr,"JMOV: read err\n"); return; }
	  }
	  JMOV_Add_Frame(jmov->vid_time,jmov->vid_timelo,act);
	  dlta_hdr->xpos = dlta_hdr->ypos = 0;
	  dlta_hdr->xsize = jmov->imagex;
	  dlta_hdr->ysize = jmov->imagey;
	  dlta_hdr->special = 0;
	  dlta_hdr->extra = (void *)(0x10);
	  dlta_hdr->xapi_rev = 0x0001;
	  dlta_hdr->delta = JFIF_Decode_JPEG;
	  ACT_Setup_Delta(jmov,act,dlta_hdr,fin);
        }
	break;
      default:
	fprintf(stderr,"JMOV: Unknown type %lx - aborting\n",type);
	return;
	break;
    } /* end of switch */
  } /* end of while */
}

