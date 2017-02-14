
/*
 * xa_jmov.c
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

/*******************************
 * Revision
 *
 *
 ********************************/


#include "xa_jmov.h" 

xaULONG JMOV_Read_File();
JMOV_FRAME *JMOV_Add_Frame();
void JMOV_Free_Frame_List();
void ACT_Setup_Delta();
xaULONG JMOV_Read_Header();
void JMOV_Read_Index();
void JMOV_Read_Frame();


/* CODEC ROUTINES */
extern xaULONG JFIF_Decode_JPEG();
extern void XA_Gen_YUV_Tabs();
extern void JPG_Alloc_MCU_Bufs();
extern char IJPG_Tab1[64];
extern char IJPG_Tab2[64];
extern void JFIF_Init_IJPG_Tables();

extern void CMAP_Cache_Clear();
extern void CMAP_Cache_Init();

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

extern void  UTIL_FPS_2_Time();
extern XA_ANIM_SETUP *XA_Get_Anim_Setup();
void XA_Free_Anim_Setup();


xaULONG jmov_audio_attempt;
xaULONG jmov_audio_type;
xaULONG jmov_audio_freq;
xaULONG jmov_audio_chans;
xaULONG jmov_audio_bps;
extern xaULONG XA_Add_Sound();


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


xaULONG JMOV_Read_File(fname,anim_hdr,audio_attempt)
char *fname;
XA_ANIM_HDR *anim_hdr;
xaULONG audio_attempt;	/* xaTRUE if audio is to be attempted */
{ XA_INPUT *xin = anim_hdr->xin;
  xaLONG i,t_time;
  xaULONG t_timelo;
  XA_ANIM_SETUP *jmov;
  JMOV_HDR   jmov_hdr;
 
  jmov = XA_Get_Anim_Setup();
  jmov->vid_time = XA_GET_TIME( 100 ); /* default */

  jmov_frame_cnt	= 0;
  jmov_frame_start	= 0;
  jmov_frame_cur	= 0;
  jmov_audio_attempt	= audio_attempt;

  if (JMOV_Read_Header(xin,anim_hdr,jmov,&jmov_hdr) == xaFALSE)
  {
    fprintf(stderr,"JMOV: read header error\n");
    xin->Close_File(xin);
    return(xaFALSE);
  }

  JMOV_Read_Index(xin,anim_hdr,jmov,&jmov_hdr);


  if (xa_verbose) 
  {
    fprintf(stderr,"JMOV %dx%dx%d frames %d\n",
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
      fprintf(stderr,"JMOV_Read_Anim: frame inconsistency %d %d\n",
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
  if (!(xin->load_flag & XA_IN_LOAD_BUF)) anim_hdr->anim_flags |= ANIM_SNG_BUF;
  if (xin->load_flag & XA_IN_LOAD_FILE) anim_hdr->anim_flags |= ANIM_USE_FILE;
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


xaULONG JMOV_Read_Header(xin,anim_hdr,jmov,jmov_hdr)
XA_INPUT *xin;
XA_ANIM_HDR *anim_hdr;
XA_ANIM_SETUP *jmov;
JMOV_HDR *jmov_hdr;
{ xaULONG t;
  char *audio_desc;
  t 			= xin->Read_MSB_U32(xin); /* skip 8 bytes of header */
  t 			= xin->Read_MSB_U32(xin); 
  jmov_hdr->version	= xin->Read_MSB_U32(xin);
  jmov_hdr->fps		= xin->Read_MSB_U32(xin);
  jmov_hdr->frames	= xin->Read_MSB_U32(xin);
  jmov_hdr->width	= xin->Read_MSB_U32(xin);  
  jmov_hdr->height	= xin->Read_MSB_U32(xin);
  jmov_hdr->bandwidth	= xin->Read_MSB_U32(xin);
  jmov_hdr->qfactor	= xin->Read_MSB_U32(xin);
  jmov_hdr->mapsize	= xin->Read_MSB_U32(xin);
  jmov_hdr->indexbuf	= xin->Read_MSB_U32(xin);
  jmov_hdr->tracks	= xin->Read_MSB_U32(xin);
  jmov_hdr->volbase 	= xin->Read_MSB_U32(xin); 
  jmov_hdr->audioslice	= xin->Read_MSB_U32(xin);
/* Sun's Audio_hdr */
  jmov_hdr->freq	= xin->Read_MSB_U32(xin);
  xin->Read_MSB_U32(xin); /* sja throw away samples per unit */
  jmov_hdr->prec	= xin->Read_MSB_U32(xin);
  jmov_hdr->chans	= xin->Read_MSB_U32(xin);
  jmov_hdr->codec	= xin->Read_MSB_U32(xin);

  UTIL_FPS_2_Time(jmov, ((double)(jmov_hdr->fps)) );

  jmov_audio_freq = jmov_hdr->freq;
  jmov_audio_chans = jmov_hdr->chans;
  jmov_audio_bps   = jmov_hdr->prec;

  jmov_audio_type = XA_AUDIO_INVALID;
  switch(jmov_hdr->codec)
  {
    case JMOV_AUDIO_ENC_NONE:
    case JMOV_AUDIO_ENC_PCM:
	if (jmov_audio_bps == 1) jmov_audio_type = XA_AUDIO_LINEAR;
	else			 jmov_audio_type = XA_AUDIO_SIGNED;
        audio_desc = "PCM";
	break;
    case JMOV_AUDIO_ENC_ULAW:
	jmov_audio_type = XA_AUDIO_ULAW;
        audio_desc = "ULAW";
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

  if (jmov_hdr->audioslice)
  { if (jmov_audio_type == XA_AUDIO_INVALID)
    { if (jmov_audio_attempt == xaTRUE) 
	fprintf(stderr,"JMOV: Audio Type unsupported %d \n",jmov_hdr->codec);
      jmov_audio_attempt = xaFALSE;
    }
    else
    { if (xa_verbose) 
      { fprintf(stderr,"  Audio Codec: %s Rate=%d Chans=%d bps=%d\n",
	audio_desc,jmov_audio_freq,jmov_audio_chans,
			(jmov_audio_bps==1)?(8):(16) );
      }
    }
  }

  /* JFIF_Init_IJPG_Tables( jmov_hdr->qfactor ); */
  JFIF_Init_IJPG_Tables( 1 );

DEBUG_LEVEL1
{
  fprintf(stderr,"JMOV: ver %d  fps %d frame %d  res %dx%d\n",
	jmov_hdr->version, jmov_hdr->fps, jmov_hdr->frames,
	jmov_hdr->width, jmov_hdr->height);
  fprintf(stderr,"      qf %d mapsize %d indx %x tracks %d audsz %x\n",
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
      || (!(xin->load_flag & XA_IN_LOAD_BUF)) )
  {
     if (cmap_true_to_332 == xaTRUE)
             jmov->chdr = CMAP_Create_332(jmov->cmap,&jmov->imagec);
     else    jmov->chdr = CMAP_Create_Gray(jmov->cmap,&jmov->imagec);
  }
  if ( (jmov->pic==0) && (xin->load_flag & XA_IN_LOAD_BUF))
  {
    jmov->pic_size = jmov->imagex * jmov->imagey;
    if ( (cmap_true_map_flag == xaTRUE) && (jmov->depth > 8) )
                jmov->pic = (xaUBYTE *) malloc( 3 * jmov->pic_size );
    else jmov->pic = (xaUBYTE *) malloc( XA_PIC_SIZE(jmov->pic_size) );
    if (jmov->pic == 0) TheEnd1("JMOV_Buffer_Action: malloc failed");
  }
  return(xaTRUE);
}


void JMOV_Read_Index(xin,anim_hdr,jmov,jmov_hdr)
XA_INPUT *xin;
XA_ANIM_HDR *anim_hdr;
XA_ANIM_SETUP *jmov;
JMOV_HDR *jmov_hdr;
{ xaLONG ret;
  xaULONG idx_off = jmov_hdr->indexbuf;
  xaULONG idx_cnt = jmov_hdr->frames;
  xaULONG i,*index;
 

  index = (xaULONG *)malloc( (idx_cnt + 1) * sizeof(xaULONG) );

  ret = xin->Seek_FPos(xin,idx_off,0);
  for(i=0; i<idx_cnt; i++)
  {
    if (xin->At_EOF(xin,-1))
    {
      fprintf(stderr,"JMOV: index truncated cur %d tot %d\n",i,idx_cnt);
      idx_cnt = i;
      break;
    }
    index[i] = xin->Read_MSB_U32(xin);
  }
  index[idx_cnt] = 0;

  for(i=0; i<idx_cnt; i++)
  { xaULONG offset = index[i];
    xin->Seek_FPos(xin,offset,0);
    JMOV_Read_Frame(xin,anim_hdr,jmov,jmov_hdr); 
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


void JMOV_Read_Frame(xin,anim_hdr,jmov,jmov_hdr)
XA_INPUT *xin;
XA_ANIM_HDR *anim_hdr;
XA_ANIM_SETUP *jmov;
JMOV_HDR *jmov_hdr;
{ xaULONG exit_flag;

  exit_flag = xaFALSE;
  while( (!xin->At_EOF(xin,-1)) & (exit_flag == xaFALSE))
  { xaULONG type;
    type = xin->Read_U8(xin);
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
	DEBUG_LEVEL1 fprintf(stderr,"JMOV: AUDIO %x\n",type);

	  if (jmov_audio_attempt==xaTRUE)
	  { xaLONG ret;
	    if (xin->load_flag & XA_IN_LOAD_FILE)
	    { xaLONG rets, cur_fpos = xin->Get_FPos(xin);
	      rets = XA_Add_Sound(anim_hdr,0,jmov_audio_type, cur_fpos,
			jmov_audio_freq, snd_size, 
			&jmov->aud_time,&jmov->aud_timelo, 0, 0);
	      if (rets==xaFALSE) jmov_audio_attempt = xaFALSE;
	      xin->Seek_FPos(xin,snd_size,1); /* move past this chunk */
	      if (snd_size > jmov->max_faud_size) 
				jmov->max_faud_size = snd_size;
 
	    }
	    else
	    { xaUBYTE *snd = (xaUBYTE *)malloc(snd_size);
	      if (snd==0) TheEnd1("JMOV: snd malloc err");
	      ret = xin->Read_Block(xin, snd, snd_size);
	      if (ret < snd_size) fprintf(stderr,"JMOV: snd rd err\n");
	      else
	      { int rets;
	        rets = XA_Add_Sound(anim_hdr,snd,jmov_audio_type, -1,
                                        jmov_audio_freq, snd_size,
                                        &jmov->aud_time, &jmov->aud_timelo, 0, 0);
	        if (rets==xaFALSE) jmov_audio_attempt = xaFALSE;
	      }
	    }
	  }
	  else xin->Seek_FPos(xin,snd_size,1); /* skip over */
	}
	break;
      case JMOV_CMAP:
	{ xaULONG size = jmov_hdr->mapsize * 3;
	  DEBUG_LEVEL1 fprintf(stderr,"JMOV: CMAP - ignored\n");
          if (jmov_hdr->mapsize > (16*1024) )
	  {
	    fprintf(stderr,"JMOV: CMAP just too big %d\n",jmov_hdr->mapsize);
	    return;
	  }
          xin->Seek_FPos(xin,size,1);  /* skip over */
	}
	break;
      case JMOV_MMAP:
	{ xaULONG cnt;
	  DEBUG_LEVEL1 fprintf(stderr,"JMOV: MMAP - ignored\n");

          cnt = xin->Read_U8(xin);
	  while(cnt--)
          { xaULONG slot,r,g,b;
	    slot = xin->Read_MSB_U16();
	    r = xin->Read_U8(xin); g = xin->Read_U8(xin); b = xin->Read_U8(xin);
          }
        }
	break;
      case JMOV_FPS:
        { xaULONG fps;
	  DEBUG_LEVEL1 fprintf(stderr,"JMOV: FPS\n");
	  fps = xin->Read_MSB_U32(xin);
	  UTIL_FPS_2_Time(jmov, ((double)(fps)) );
        }
	break;
      case JMOV_JPEG:
        { xaULONG dlta_len;
	  XA_ACTION *act;
          ACT_DLTA_HDR *dlta_hdr;

	  DEBUG_LEVEL1 fprintf(stderr,"JMOV: JPEG\n");
	  dlta_len = xin->Read_MSB_U32(xin);

	  act = ACT_Get_Action(anim_hdr,ACT_DELTA);
	  if (xin->load_flag & XA_IN_LOAD_FILE)
	  {
	    dlta_hdr = (ACT_DLTA_HDR *) malloc(sizeof(ACT_DLTA_HDR));
	    if (dlta_hdr == 0) TheEnd1("JMOV: dlta malloc err");
	    act->data = (xaUBYTE *)dlta_hdr;
	    dlta_hdr->flags = ACT_SNGL_BUF;
	    dlta_hdr->fsize = dlta_len;
	    dlta_hdr->fpos  = xin->Get_FPos(xin);
	    if (dlta_len > jmov->max_fvid_size) jmov->max_fvid_size = dlta_len;
	    xin->Seek_FPos(xin,dlta_len,1);
	  }
	  else
	  { xaULONG d; xaLONG ret;
	    d = dlta_len + (sizeof(ACT_DLTA_HDR));
	    dlta_hdr = (ACT_DLTA_HDR *) malloc( d );
	    if (dlta_hdr == 0) TheEnd1("JMOV: malloc failed");
	    act->data = (xaUBYTE *)dlta_hdr;
	    dlta_hdr->flags = ACT_SNGL_BUF | DLTA_DATA;
	    dlta_hdr->fpos = 0; dlta_hdr->fsize = dlta_len;
	    ret = xin->Read_Block(xin, dlta_hdr->data, dlta_len);
	    if (ret < dlta_len) { fprintf(stderr,"JMOV: read err\n"); return; }
	  }
	  JMOV_Add_Frame(jmov->vid_time,jmov->vid_timelo,act);
	  dlta_hdr->xpos = dlta_hdr->ypos = 0;
	  dlta_hdr->xsize = jmov->imagex;
	  dlta_hdr->ysize = jmov->imagey;
	  dlta_hdr->special = 0;
	  dlta_hdr->extra = (void *)(0x10);
	  dlta_hdr->xapi_rev = 0x0002;
	  dlta_hdr->delta = JFIF_Decode_JPEG;
	  ACT_Setup_Delta(jmov,act,dlta_hdr,xin);
        }
	break;
      default:
	fprintf(stderr,"JMOV: Unknown type %x - aborting\n",type);
	return;
	break;
    } /* end of switch */
  } /* end of while */
}

