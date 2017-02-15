
/*
 * xa_replay.c
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


#include "xa_replay.h" 
#include "xa_formats.h"

ARM_FRAME *ARM_Add_Frame();
void ARM_Free_Frame_List();
void ACT_Setup_Delta();
xaULONG ARM_Read_Header();
xaULONG ARM_Read_Index();
void ARM_Read_Frame();
xaULONG ARM_Get_Length();

void ARM_Free_Stuff();
void ARM_Init_Tables();


xaULONG ARM_Decode_MLINES();


/* CODEC ROUTINES */
extern void yuv_to_rgb();
extern void XA_Gen_YUV_Tabs();
extern xaULONG XA_RGB24_To_CLR32();

void CMAP_Cache_Clear();
void CMAP_Cache_Init();

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
extern void XA_Add_Func_To_Free_Chain();
extern void XA_Free_Anim_Setup();
extern xaULONG XA_find_str();

xaUBYTE *ARM_prev_buff = 0;
xaULONG ARM_prev_buff_size = 0;
xaLONG *ARM_MOVL_X = 0;
xaLONG *ARM_MOVL_Y = 0;

xaULONG arm_audio_attempt;
xaULONG arm_audio_type;
xaULONG arm_audio_freq;
xaULONG arm_audio_chans;
xaULONG arm_audio_bps;
xaULONG XA_Add_Sound();


xaULONG arm_frame_cnt;
ARM_FRAME *arm_frame_start,*arm_frame_cur;

ARM_FRAME *ARM_Add_Frame(time,timelo,act)
xaULONG time,timelo;
XA_ACTION *act;
{
  ARM_FRAME *fframe;
 
  fframe = (ARM_FRAME *) malloc(sizeof(ARM_FRAME));
  if (fframe == 0) TheEnd1("ARM_Add_Frame: malloc err");
 
  fframe->time   = time;
  fframe->timelo = timelo;
  fframe->act = act;
  fframe->next = 0;
 
  if (arm_frame_start == 0) arm_frame_start = fframe;
  else arm_frame_cur->next = fframe;
 
  arm_frame_cur = fframe;
  arm_frame_cnt++;
  return(fframe);
}

void ARM_Free_Frame_List(fframes)
ARM_FRAME *fframes;
{
  ARM_FRAME *ftmp;
  while(fframes != 0)
  {
    ftmp = fframes;
    fframes = fframes->next;
    FREE(ftmp,0xA000);
  }
}


xaULONG ARM_Read_File(const char *fname,XA_ANIM_HDR *anim_hdr,xaULONG audio_attempt)
{ XA_INPUT *xin = anim_hdr->xin;
  xaLONG i,t_time;
  xaULONG t_timelo;
  XA_ANIM_SETUP *arm;
  ARM_HDR   arm_hdr;
 

  arm = XA_Get_Anim_Setup();
  arm->vid_time = XA_GET_TIME( 100 ); /* default */

  arm_frame_cnt		= 0;
  arm_frame_start	= 0;
  arm_frame_cur		= 0;
  arm_audio_attempt	= audio_attempt;


  if (ARM_Read_Header(anim_hdr,xin,arm,&arm_hdr) == xaFALSE)
  {
    fprintf(stderr,"ARM: read header error\n");
    xin->Close_File(xin);
    return(xaFALSE);
  }

  if (ARM_Read_Index(xin,anim_hdr,arm,&arm_hdr)==xaFALSE) return(xaFALSE);

  if (xa_verbose) 
  {
    fprintf(stderr,"ARM %dx%dx%d frames %d\n",
		arm->imagex,arm->imagey,arm->imagec,arm_frame_cnt);
  }
  if (arm_frame_cnt == 0)
  { 
    fprintf(stderr,"ARM: No supported video frames exist in this file.\n");
    return(xaFALSE);
  }

  anim_hdr->frame_lst = (XA_FRAME *)
				malloc( sizeof(XA_FRAME) * (arm_frame_cnt+1));
  if (anim_hdr->frame_lst == NULL) TheEnd1("ARM_Read_File: frame malloc err");

  arm_frame_cur = arm_frame_start;
  i = 0;
  t_time = 0;
  t_timelo = 0;
  while(arm_frame_cur != 0)
  {
    if (i > arm_frame_cnt)
    {
      fprintf(stderr,"ARM_Read_Anim: frame inconsistency %d %d\n",
                i,arm_frame_cnt);
      break;
    }
    anim_hdr->frame_lst[i].time_dur = arm_frame_cur->time;
    anim_hdr->frame_lst[i].zztime = t_time;
    t_time += arm_frame_cur->time;
    t_timelo += arm_frame_cur->timelo;
    while(t_timelo > (1<<24)) {t_time++; t_timelo -= (1<<24);}
    anim_hdr->frame_lst[i].act = arm_frame_cur->act;
    arm_frame_cur = arm_frame_cur->next;
    i++;
  }
  anim_hdr->imagex = arm->imagex;
  anim_hdr->imagey = arm->imagey;
  anim_hdr->imagec = arm->imagec;
  anim_hdr->imaged = 8; /* nop */
  anim_hdr->frame_lst[i].time_dur = 0;
  anim_hdr->frame_lst[i].zztime = -1;
  anim_hdr->frame_lst[i].act  = 0;
  anim_hdr->loop_frame = 0;
  if (!(xin->load_flag & XA_IN_LOAD_BUF)) anim_hdr->anim_flags |= ANIM_SNG_BUF;
  if (xin->load_flag & XA_IN_LOAD_FILE) anim_hdr->anim_flags |= ANIM_USE_FILE;
  anim_hdr->anim_flags |= ANIM_FULL_IM;
  anim_hdr->max_fvid_size = arm->max_fvid_size;
  anim_hdr->max_faud_size = arm->max_faud_size;
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
  ARM_Free_Frame_List(arm_frame_start);
  XA_Free_Anim_Setup(arm);
  return(xaTRUE);
} /* end of read file */


#define ARM_MAX_LINE 256
char arm_line[ARM_MAX_LINE];

xaULONG ARM_Read_Line(xin,ptr)
XA_INPUT *xin;
char *ptr;
{ int i = 0;
  while( !xin->At_EOF(xin,-1) )
  { int d = xin->Read_U8(xin);
    if (d >= 0)
    { ptr[i] = d; i++;
      if (i  >= ARM_MAX_LINE) return(xaFALSE);
      if ( (d == 0x0a) || (d == 0x0d) ) 
	{ ptr[i] = 0; 
	  DEBUG_LEVEL1 fprintf(stderr,"LINE: %s",ptr);
          return(xaTRUE); 
	}
    }
    else return(xaFALSE); 
  }
  return(xaFALSE);
}

xaULONG ARM_Read_Header(anim_hdr,xin,arm,arm_hdr)
XA_ANIM_HDR *anim_hdr;
XA_INPUT *xin;
XA_ANIM_SETUP *arm;
ARM_HDR *arm_hdr;
{ double tmpf;

  if (ARM_Read_Line(xin,arm_line) == xaFALSE) return(xaFALSE);  /* ARMovie */
  if (ARM_Read_Line(xin,arm_line) == xaFALSE) return(xaFALSE);  /* name */
  if (ARM_Read_Line(xin,arm_line) == xaFALSE) return(xaFALSE);  /* date/copyright */
  if (ARM_Read_Line(xin,arm_line) == xaFALSE) return(xaFALSE);  /* author/etc */
  if (ARM_Read_Line(xin,arm_line) == xaFALSE) return(xaFALSE);  /* video format */
  if (arm_line[0] == '1') arm_hdr->vid_codec = ARM_VIDEO_MOVL_RGB;
  else
  { fprintf(stderr,"ARM: Unsupported Video Type: %s,",arm_line);
    arm_hdr->vid_codec = ARM_VIDEO_UNK;
  }

  if (ARM_Read_Line(xin,arm_line) == xaFALSE) return(xaFALSE);	/* width */
    tmpf = atof(arm_line);  arm_hdr->width	 = (xaLONG)tmpf;
  if (ARM_Read_Line(xin,arm_line) == xaFALSE) return(xaFALSE);	/* height */
    tmpf = atof(arm_line);  arm_hdr->height	= (xaLONG)tmpf;
  if (ARM_Read_Line(xin,arm_line) == xaFALSE) return(xaFALSE);	/* depth */
    tmpf = atof(arm_line);  arm_hdr->depth	= (xaLONG)tmpf;

/* Check Video Precision Line for modifiers */
    if (arm_hdr->vid_codec == ARM_VIDEO_MOVL_RGB)
    {
       if (   (XA_find_str(arm_line,"yuv") == xaTRUE) 
           || (XA_find_str(arm_line,"YUV") == xaTRUE) )
		arm_hdr->vid_codec = ARM_VIDEO_MOVL_YUV;
    }

  if (ARM_Read_Line(xin,arm_line) == xaFALSE) return(xaFALSE);	/* fps */
    arm_hdr->fps = atof(arm_line);
  if (ARM_Read_Line(xin,arm_line) == xaFALSE) return(xaFALSE);  /*audio format */
    if (arm_audio_attempt == xaTRUE)
    {
      if (arm_line[0] == '0') 
      {
	arm_hdr->aud_codec = ARM_AUDIO_NONE;
	arm_audio_attempt = xaFALSE;
      }
      else if (arm_line[0] == '1') 
      { 
	arm_hdr->aud_codec = ARM_AUDIO_FORM1;
      }
      else
      { 
	fprintf(stderr,"ARM: Unsupported Audio Type: %s\n",arm_line);
	arm_hdr->aud_codec = ARM_AUDIO_UNK;
	arm_audio_attempt = xaFALSE;
      }
    }
  if (ARM_Read_Line(xin,arm_line) == xaFALSE) return(xaFALSE);  /* audio freq */
     /* NOTE: this can be fraction HZ - eventually support this or us */
    tmpf = atof(arm_line);
    { static unsigned char us[3] = { 0xB5, 0x73, 0x00 };
      if (XA_find_str(arm_line,us)==xaTRUE)
      { double freq = (1000000.0 / tmpf) + 0.5;
        arm_hdr->aud_freq = (xaLONG)freq;
      }
      else arm_hdr->aud_freq = (xaLONG)tmpf;
    }
     
  if (ARM_Read_Line(xin,arm_line) == xaFALSE) return(xaFALSE);  /* audio chans */
    tmpf = atof(arm_line);  arm_hdr->aud_chans	= (xaLONG)tmpf;
  if (ARM_Read_Line(xin,arm_line) == xaFALSE) return(xaFALSE);  /* audio prec */
    tmpf = atof(arm_line);  arm_hdr->aud_prec	= (xaLONG)tmpf;

    /* Check Audio Precision Line for modifiers */
    if (arm_audio_attempt == xaTRUE)
    {
      arm_audio_freq  = arm_hdr->aud_freq;
      arm_audio_chans = arm_hdr->aud_chans;
      if (arm_hdr->aud_prec == 8) arm_audio_bps = 1;
      else if (arm_hdr->aud_prec == 16) arm_audio_bps = 2;
      else arm_audio_bps = 0;
      if ( XA_find_str(arm_line,"linear") == xaTRUE) 
      {
        if (xa_verbose==xaTRUE) 
		fprintf(stderr,"AUDIO: Signed %dHz chans: %d  bits: %d\n",
			arm_audio_freq,arm_audio_chans,arm_hdr->aud_prec);
        arm_audio_type = XA_AUDIO_SIGNED;  /* NOTE: little endian */
      } 
      else if ( XA_find_str(arm_line,"logarithmic") == xaTRUE) 
      {
        if (xa_verbose==xaTRUE) 
	    fprintf(stderr,"AUDIO: LOGARITHMIC %dHz chans: %d bits: %d\n",
			arm_audio_freq,arm_audio_chans,arm_hdr->aud_prec);
        arm_audio_type = XA_AUDIO_ARMLAW;  /* NOTE: little endian */
      } 
      else arm_audio_type = XA_AUDIO_INVALID;
      /* paranoia */
      if ( (arm_audio_chans > 2) || (arm_audio_chans == 0) )
					arm_audio_type = XA_AUDIO_INVALID;
      if ( (arm_audio_bps > 2) || (arm_audio_bps == 0) )
					arm_audio_type = XA_AUDIO_INVALID;

      /* warning and final modifications */
      if (arm_audio_type == XA_AUDIO_INVALID)
      {
        if (arm_audio_attempt == xaTRUE) 
	  fprintf(stderr,"ARM: Audio Type unsupported %d \n",
						arm_hdr->aud_codec);
        arm_audio_attempt = xaFALSE;
      }
      else
      {
        if (arm_audio_chans == 2) arm_audio_type |= XA_AUDIO_STEREO_MSK;
        if (arm_audio_bps == 2) arm_audio_type |= XA_AUDIO_BPS_2_MSK;
      }
    }

  if (ARM_Read_Line(xin,arm_line) == xaFALSE) return(xaFALSE);/*frames perchnk*/
    tmpf = atof(arm_line);  arm_hdr->fp_chunk	= (xaLONG)tmpf;
  if (ARM_Read_Line(xin,arm_line) == xaFALSE) return(xaFALSE); /* chunk cnt */
    tmpf = atof(arm_line);  arm_hdr->chunk_cnt	= (xaLONG)tmpf;

    /* This is necessary so +CF4 will function correctly */
    arm->cmap_frame_num  = (arm_hdr->chunk_cnt * arm_hdr->fp_chunk) 
							/ cmap_sample_cnt;

  if (ARM_Read_Line(xin,arm_line) == xaFALSE) return(xaFALSE); /*ev chunk sz */
    tmpf = atof(arm_line);  arm_hdr->ev_chk_size	= (xaLONG)tmpf;
  if (ARM_Read_Line(xin,arm_line) == xaFALSE) return(xaFALSE); /*od chunk sz */
    tmpf = atof(arm_line);  arm_hdr->od_chk_size	= (xaLONG)tmpf;
  if (ARM_Read_Line(xin,arm_line) == xaFALSE) return(xaFALSE); /* idx offset */
    tmpf = atof(arm_line);  arm_hdr->idx_offset	= (xaLONG)tmpf;
  if (ARM_Read_Line(xin,arm_line) == xaFALSE) return(xaFALSE); /*sprite offset*/
    tmpf = atof(arm_line);  arm_hdr->sprite_off	= (xaLONG)tmpf;
  if (ARM_Read_Line(xin,arm_line) == xaFALSE) return(xaFALSE); /*sprite size*/
    tmpf = atof(arm_line);  arm_hdr->sprite_size = (xaLONG)tmpf;
  if (ARM_Read_Line(xin,arm_line) == xaFALSE) return(xaFALSE); /*key  offset */
    tmpf = atof(arm_line);  arm_hdr->key_offset	= (xaLONG)tmpf;
  

  UTIL_FPS_2_Time(arm, arm_hdr->fps );

/*
  switch(arm_hdr->aud_codec)
  {
  }
*/

  arm->depth  = arm_hdr->depth;
  arm->imagex = arm_hdr->width;
  arm->imagey = arm_hdr->height;
  XA_Gen_YUV_Tabs(anim_hdr);
  ARM_Init_Tables(anim_hdr,arm->imagex,arm->imagey);

if (xa_verbose)
{
  fprintf(stderr,"ARM: %dx%dx%x\n",arm->imagex,arm->imagey,arm->depth);
}

  if (   (cmap_true_map_flag == xaFALSE) /* depth 16 and not true_map */
      || (!(xin->load_flag & XA_IN_LOAD_BUF)) )
  {
     if (cmap_true_to_332 == xaTRUE)
             arm->chdr = CMAP_Create_332(arm->cmap,&arm->imagec);
     else    arm->chdr = CMAP_Create_Gray(arm->cmap,&arm->imagec);
  }
  if ( (arm->pic==0) && (xin->load_flag & XA_IN_LOAD_BUF))
  {
    arm->pic_size = arm->imagex * arm->imagey;
    if ( (cmap_true_map_flag == xaTRUE) && (arm->depth > 8) )
                arm->pic = (xaUBYTE *) malloc( 3 * arm->pic_size );
    else arm->pic = (xaUBYTE *) malloc( XA_PIC_SIZE(arm->pic_size) );
    if (arm->pic == 0) TheEnd1("ARM_Buffer_Action: malloc failed");
  }
  return(xaTRUE);
}


xaULONG ARM_Read_Index(xin,anim_hdr,arm,arm_hdr)
XA_INPUT *xin;
XA_ANIM_HDR *anim_hdr;
XA_ANIM_SETUP *arm;
ARM_HDR *arm_hdr;
{ xaULONG idx_cnt = arm_hdr->chunk_cnt;
  xaULONG i,*idx_off,*idx_vsize,*idx_asize;

  idx_off = (xaULONG *)malloc( (idx_cnt + 2) * sizeof(xaULONG) );
  idx_vsize = (xaULONG *)malloc( (idx_cnt + 2) * sizeof(xaULONG) );
  idx_asize = (xaULONG *)malloc( (idx_cnt + 2) * sizeof(xaULONG) );

  DEBUG_LEVEL2 fprintf(stderr,"    idx offset  %08x\n",arm_hdr->idx_offset);
  xin->Seek_FPos(xin,arm_hdr->idx_offset,0);
  /* Go Through Chunks */
  for(i=0; i<=idx_cnt; i++)
  { xaLONG off, vid_size, aud_size, ret;
    if (ARM_Read_Line(xin,arm_line) == xaFALSE) return(xaFALSE); /*chunk count*/
    ret = sscanf(arm_line,"%d,%d;%d",&off,&vid_size,&aud_size);
    DEBUG_LEVEL1
    {
     fprintf(stderr,"  (%d) %d) off %08x  vid %08x  aud %08x\n",ret,i,
						off,vid_size,aud_size);
    }
    idx_off[i] = off;
    idx_vsize[i] = vid_size;
    idx_asize[i] = aud_size;

  } /* end of for */


  for(i=0; i<=idx_cnt; i++)
  { xaULONG j; xaULONG tot_dsize = 0;

DEBUG_LEVEL1 fprintf(stderr," idx %d) chunks %d\n",i, arm_hdr->fp_chunk);
    /* VIDEO */
    xin->Seek_FPos(xin,idx_off[i],0);
    for(j=0; j < arm_hdr->fp_chunk; j++)
    { xaULONG fpos,dsize;
      fpos = xin->Get_FPos(xin);		/* save xin position */
      dsize = ARM_Get_Length(xin,arm);	/* find length of delta */
      xin->Seek_FPos(xin,fpos,0);		/* return to xin position */
      ARM_Read_Frame(xin,anim_hdr,arm,arm_hdr,dsize);
      tot_dsize += dsize;
    }

    /* AUDIO */
    if (arm_audio_attempt == xaTRUE) 
    { xaLONG ret, snd_size = (xaLONG)idx_asize[i];
      xaULONG snd_off = idx_off[i] + idx_vsize[i];
      if (xin->load_flag & XA_IN_LOAD_FILE)
      { xaLONG rets;
	rets = XA_Add_Sound(anim_hdr,0,arm_audio_type, snd_off,
				arm_audio_freq, snd_size, 
				&arm->aud_time,&arm->aud_timelo, 0, 0);
	if (rets==xaFALSE) arm_audio_attempt = xaFALSE;
	if (snd_size > arm->max_faud_size) arm->max_faud_size = snd_size;
      }
      else
      { xaUBYTE *snd = (xaUBYTE *)malloc(snd_size);
	if (snd==0) TheEnd1("ARM: snd malloc err");
        xin->Seek_FPos(xin,snd_off,0); /* seek to audio info */
	ret = xin->Read_Block(xin, snd, snd_size);
	if (ret < snd_size) fprintf(stderr,"ARM: snd rd err(size %x\n",snd_size);
	else
	{ int rets;
          /*NOTE: don't free snd */
	  rets = XA_Add_Sound(anim_hdr,snd,arm_audio_type, -1,
				arm_audio_freq, snd_size,
				&arm->aud_time, &arm->aud_timelo, 0, 0);
	  if (rets==xaFALSE) arm_audio_attempt = xaFALSE;
	}
      }
    } /* end of audio attempt */
  }
  free(idx_off);
  free(idx_vsize);
  free(idx_asize);
  return(xaTRUE);
}

/* Assuming xin is pointing to start of delta.
 * leaves xin just after end of delta.
 */
void ARM_Read_Frame(xin,anim_hdr,arm,arm_hdr,vsize)
XA_INPUT *xin;
XA_ANIM_HDR *anim_hdr;
XA_ANIM_SETUP *arm;
ARM_HDR *arm_hdr;
xaLONG vsize;
{ XA_ACTION *act;
  xaULONG dlta_len = vsize;
  ACT_DLTA_HDR *dlta_hdr;

  act = ACT_Get_Action(anim_hdr,ACT_DELTA);
  if (xin->load_flag & XA_IN_LOAD_FILE)
  {
    dlta_hdr = (ACT_DLTA_HDR *) malloc(sizeof(ACT_DLTA_HDR));
    if (dlta_hdr == 0) TheEnd1("ARM: dlta malloc err");
    act->data = (xaUBYTE *)dlta_hdr;
    dlta_hdr->flags = ACT_SNGL_BUF;
    dlta_hdr->fsize = dlta_len;
    dlta_hdr->fpos  = xin->Get_FPos(xin);
    if (dlta_len > arm->max_fvid_size) arm->max_fvid_size = dlta_len;
    xin->Seek_FPos(xin,dlta_len,1);
  }
  else
  { xaULONG d; xaLONG ret;
    d = dlta_len + (sizeof(ACT_DLTA_HDR));
    dlta_hdr = (ACT_DLTA_HDR *) malloc( d );
    if (dlta_hdr == 0) TheEnd1("QT rle: malloc failed");
    act->data = (xaUBYTE *)dlta_hdr;
    dlta_hdr->flags = ACT_SNGL_BUF | DLTA_DATA;
    dlta_hdr->fpos = 0; dlta_hdr->fsize = dlta_len;
    ret = xin->Read_Block(xin, dlta_hdr->data, dlta_len);
    if (ret < dlta_len) { fprintf(stderr,"ARM: read err\n"); return; }
  }
  ARM_Add_Frame(arm->vid_time,arm->vid_timelo,act);
  dlta_hdr->xpos = dlta_hdr->ypos = 0;
  dlta_hdr->xsize = arm->imagex;
  dlta_hdr->ysize = arm->imagey;
  dlta_hdr->special = 0;
  if (arm_hdr->vid_codec == ARM_VIDEO_MOVL_YUV)
		dlta_hdr->extra = (void *)(ARM_VIDEO_YUV);
  else		dlta_hdr->extra = (void *)(ARM_VIDEO_RGB);
  dlta_hdr->xapi_rev = 0x0001;
  dlta_hdr->delta = ARM_Decode_MLINES;
  ACT_Setup_Delta(arm,act,dlta_hdr,xin);
}



#define ARM_15YUV(pix,y,u,v) { \
  v = (pix >> 10) & 0x1f; u = (pix >> 5) & 0x1f; y = pix & 0x1f; \
  u ^= 0x10; v ^= 0x10; \
  y = (y << 3) | (y >> 2); u = (u << 3) | (u >> 2); v = (v << 3) | (v >> 2); \
} 

#define ARM_16YUV(pix,y,u,v) { \
  v = (pix >> 11) & 0x1f; u = (pix >> 6) & 0x1f; y = (pix >> 1) & 0x1f; \
  u ^= 0x10; v ^= 0x10; \
  y = (y << 3) | (y >> 2); u = (u << 3) | (u >> 2); v = (v << 3) | (v >> 2); \
}

#define ARM_GET_CODE(dp,code) {code = (*dp++); code |= (*dp++) << 8; }

xaULONG ARM_Get_RGB_Pixel(pix,map_flag,map,chdr)
xaULONG pix,map_flag,*map;
XA_CHDR *chdr;
{ xaULONG r,g,b;
  r = (pix >> 10) & 0x1f; g = (pix >> 5) & 0x1f; b = pix & 0x1f;
  r = (r << 3) | (r >> 2); g = (g << 3) | (g >> 2); b = (b << 3) | (b >> 2);
  return( XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr) );
} 

xaULONG ARM_Get_YUV_Pixel(pix,map_flag,map,chdr)
xaULONG pix,map_flag,*map;
XA_CHDR *chdr;
{ xaULONG y,u,v,r,g,b;
  v = (pix >> 10) & 0x1f; u = (pix >> 5) & 0x1f; y = pix & 0x1f;
  u ^= 0x10; v ^= 0x10;
  y = (y << 3) | (y >> 2); u = (u << 3) | (u >> 2); v = (v << 3) | (v >> 2);
  yuv_to_rgb(y,u,v,&r,&g,&b); 
  return( XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr) );
} 

xaULONG ARM_Get_RGB_RGB(pix,ir,ig,ib)
xaULONG pix;
xaULONG *ir,*ig,*ib;
{ xaULONG r,g,b;
  r = (pix >> 10) & 0x1f; g = (pix >> 5) & 0x1f; b = pix & 0x1f;
  r = (r << 3) | (r >> 2); g = (g << 3) | (g >> 2); b = (b << 3) | (b >> 2);
  *ir = r; *ig = g; *ib = b;
  return(1);
} 

xaULONG ARM_Get_YUV_RGB(pix,ir,ig,ib)
xaULONG pix;
xaULONG *ir,*ig,*ib;
{ xaULONG y,u,v;
  v = (pix >> 10) & 0x1f; u = (pix >> 5) & 0x1f; y = pix & 0x1f;
  u ^= 0x10; v ^= 0x10;
  y = (y << 3) | (y >> 2); u = (u << 3) | (u >> 2); v = (v << 3) | (v >> 2);
  yuv_to_rgb(y,u,v,ir,ig,ib); 
  return(1);
} 

/* TEST */
xaULONG
ARM_Decode_MLINES(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG special = dec_info->special;          void *extra = dec_info->extra;
  XA_CHDR *chdr = dec_info->chdr;
  xaUBYTE *dp = delta;
  xaULONG special_flag = special & 0x0001;
  xaLONG i_cnt,im_size = imagex * imagey;
  xaULONG code;
  xaUBYTE *pptr = ARM_prev_buff;
  xaULONG vid_type = (xaULONG)(extra);
  xaULONG (*arm_get_pixel)();
  
  if (chdr) { if (chdr->new_chdr) chdr = chdr->new_chdr; }

  /* Need to copy previous image, since Temporal references it */
  { i_cnt  = im_size * ((special_flag)?(3):(x11_bytes_pixel));
    memcpy((char *)(pptr),(char *)(image),(i_cnt));
  }

/* */
  if (special)
  {
    if (vid_type == ARM_VIDEO_YUV) arm_get_pixel = ARM_Get_YUV_RGB;
    else arm_get_pixel = ARM_Get_RGB_RGB;
  }
  else
  {
    if (vid_type == ARM_VIDEO_YUV) arm_get_pixel = ARM_Get_YUV_Pixel;
    else arm_get_pixel = ARM_Get_RGB_Pixel;
  }

  i_cnt = 0;

 if (special)
 { xaUBYTE *iptr = image;
  while(i_cnt < im_size)
  { ARM_GET_CODE(dp,code); 
    if (code & 0x01)
    { xaULONG test,cnt;
      if (code == 0xe601) { i_cnt = im_size; break; }	/* End of Frame */
      test = code >> 7;		cnt = ((code >> 1) & 0x3f) + 2;
      if (test < 0x1cc)			/* Temporal/Spatial */
      { xaUBYTE *isp;
        if (test < 0x120) { isp = pptr; isp += (3 * i_cnt); } /* temporal */
	else isp = iptr; /* spatial */
	isp += 3 * (ARM_MOVL_X[test] + (ARM_MOVL_Y[test] * imagex));
	i_cnt += cnt;	if (i_cnt >= im_size) cnt -= (i_cnt - im_size);
	while(cnt--) { *iptr++ = *isp++; *iptr++ = *isp++; *iptr++ = *isp++; }
      }
      else if (test >= 0x1e0)
      { cnt = ((code >> 1) & 0x3ff) + 1;
	if (test >=0x1f0)				/* New N Pixels */
	{ xaLONG  arm_bcnt   = ((((cnt * 15) + 15) >> 4) << 1); /* dp bytes */
	  xaLONG  arm_b_bnum = 0;	xaULONG arm_b_bbuf = 0;
	  DEBUG_LEVEL1  fprintf(stderr,"New N Pixels %d\n",cnt);
	  i_cnt += cnt; if (i_cnt >= im_size) cnt -= (i_cnt - im_size);
	  while(cnt--)  /* pixels */
	  { xaULONG r,g,b; /* Little Endian */
	    while(arm_b_bnum < 15) { arm_b_bbuf |=  (*dp++) << arm_b_bnum;
						arm_b_bnum += 8; arm_bcnt--; }
	    arm_get_pixel(arm_b_bbuf,&r,&g,&b);
	    *iptr++ = (xaUBYTE)r; *iptr++ = (xaUBYTE)g; *iptr++ = (xaUBYTE)b;
	    arm_b_bbuf >>= 15;		arm_b_bnum -= 15;
	  }
	  while(arm_bcnt--) dp++; /* left overs */
	} else { iptr += (3 * cnt);  i_cnt += cnt; }		/* Copy/Skip */
      }
      else if (test == 0x1cc)
      { xaULONG r,g,b,pixel;
	ARM_GET_CODE(dp,pixel);
	arm_get_pixel(pixel,&r,&g,&b);
	i_cnt += cnt;	if (i_cnt >= im_size) cnt -= (i_cnt - im_size);
	while(cnt--) { *iptr++ = (xaUBYTE)r; 
				*iptr++ = (xaUBYTE)g; *iptr++ = (xaUBYTE)b; }
      } else fprintf(stderr,"ARM decode ERROR: %04x\n",code);
    }
    else			/* New Pixel */
    { xaULONG r,g,b; arm_get_pixel((code>>1),&r,&g,&b);  i_cnt++;
      *iptr++ = (xaUBYTE)r; *iptr++ = (xaUBYTE)g; *iptr++ = (xaUBYTE)b;
    }
  } 
 }
 else if ( (x11_bytes_pixel==1) || (map_flag == xaFALSE) )
 { xaUBYTE *iptr = image;
  while(i_cnt < im_size)
  { ARM_GET_CODE(dp,code); 
    if (code & 0x01)
    { xaULONG test,cnt;
      if (code == 0xe601) { i_cnt = im_size; break; }	/* End of Frame */
      test = code >> 7;		cnt = ((code >> 1) & 0x3f) + 2;
      if (test < 0x1cc)			/* Temporal/Spatial */
      { xaUBYTE *isp;
        if (test < 0x120) { isp = pptr; isp += i_cnt; } /* temporal */
	else isp = iptr; /* spatial */
	isp += (ARM_MOVL_X[test] + (ARM_MOVL_Y[test] * imagex));
	i_cnt += cnt;	if (i_cnt >= im_size) cnt -= (i_cnt - im_size);
	while(cnt--) *iptr++ = *isp++;
      }
      else if (test >= 0x1e0)
      { cnt = ((code >> 1) & 0x3ff) + 1;
	if (test >=0x1f0)				/* New N Pixels */
	{ xaLONG  arm_bcnt   = ((((cnt * 15) + 15) >> 4) << 1); /* dp bytes */
	  xaLONG  arm_b_bnum = 0;	xaULONG arm_b_bbuf = 0;
	  DEBUG_LEVEL1  fprintf(stderr,"New N Pixels %d\n",cnt);
	  i_cnt += cnt; if (i_cnt >= im_size) cnt -= (i_cnt - im_size);
	  while(cnt--)  /* pixels */
	  { /* Little Endian */
	    while(arm_b_bnum < 15) { arm_b_bbuf |=  (*dp++) << arm_b_bnum;
						arm_b_bnum += 8; arm_bcnt--; }
	    *iptr++ = (xaUBYTE)arm_get_pixel(arm_b_bbuf,map_flag,map,chdr);
	    arm_b_bbuf >>= 15;		arm_b_bnum -= 15;
	  }
	  while(arm_bcnt--) dp++; /* left overs */
	} else { iptr += cnt;  i_cnt += cnt; }		/* Copy/Skip */
      }
      else if (test == 0x1cc)
      { xaULONG pixel; xaUBYTE d;
	ARM_GET_CODE(dp,pixel);
	d = (xaUBYTE)arm_get_pixel(pixel,map_flag,map,chdr);
	i_cnt += cnt;	if (i_cnt >= im_size) cnt -= (i_cnt - im_size);
	while(cnt--) *iptr++ = d;
      } else fprintf(stderr,"ARM decode ERROR: %04x\n",code);
    }
    else			/* New Pixel */
    { *iptr++ = (xaUBYTE)arm_get_pixel((code>>1),map_flag,map,chdr);  i_cnt++; }
  } 
 } /* end of byte 1 */
 else if (x11_bytes_pixel==4)
 { xaULONG *iptr = (xaULONG *)image;
  while(i_cnt < im_size)
  { ARM_GET_CODE(dp,code); 
    if (code & 0x01)
    { xaULONG test,cnt;
      if (code == 0xe601) { i_cnt = im_size; break; }	/* End of Frame */
      test = code >> 7;		cnt = ((code >> 1) & 0x3f) + 2;
      if (test < 0x1cc)			/* Temporal/Spatial */
      { xaULONG *isp;
        if (test < 0x120) { isp = (xaULONG *)pptr; isp += i_cnt; }/*temporal */
	else isp = (xaULONG *)iptr; /* spatial */
	isp += (ARM_MOVL_X[test] + (ARM_MOVL_Y[test] * imagex));
	i_cnt += cnt;	if (i_cnt >= im_size) cnt -= (i_cnt - im_size);
	while(cnt--) *iptr++ = *isp++;
      }
      else if (test >= 0x1e0)
      { cnt = ((code >> 1) & 0x3ff) + 1;
	if (test >=0x1f0)				/* New N Pixels */
	{ xaLONG  arm_bcnt   = ((((cnt * 15) + 15) >> 4) << 1); /* dp bytes */
	  xaLONG  arm_b_bnum = 0;	xaULONG arm_b_bbuf = 0;
	  DEBUG_LEVEL1  fprintf(stderr,"New N Pixels %d\n",cnt);
	  i_cnt += cnt; if (i_cnt >= im_size) cnt -= (i_cnt - im_size);
	  while(cnt--)  /* pixels */
	  { /* Little Endian */
	    while(arm_b_bnum < 15) { arm_b_bbuf |=  (*dp++) << arm_b_bnum;
						arm_b_bnum += 8; arm_bcnt--; }
	    *iptr++ = (xaULONG)arm_get_pixel(arm_b_bbuf,map_flag,map,chdr);
	    arm_b_bbuf >>= 15;		arm_b_bnum -= 15;
	  }
	  while(arm_bcnt--) dp++; /* left overs */
	} else { iptr += cnt;  i_cnt += cnt; }		/* Copy/Skip */
      }
      else if (test == 0x1cc)
      { xaULONG pixel; xaULONG d;
	ARM_GET_CODE(dp,pixel);
	d = (xaULONG)arm_get_pixel(pixel,map_flag,map,chdr);
	i_cnt += cnt;	if (i_cnt >= im_size) cnt -= (i_cnt - im_size);
	while(cnt--) *iptr++ = d;
      } else fprintf(stderr,"ARM decode ERROR: %04x\n",code);
    }
    else			/* New Pixel */
    { *iptr++ = (xaULONG)arm_get_pixel((code>>1),map_flag,map,chdr);  i_cnt++; }
  } 
 } /* end of byte 4 */
 else /* if (x11_bytes_pixel==2) */
 { xaUSHORT *iptr = (xaUSHORT *)image;
  while(i_cnt < im_size)
  { ARM_GET_CODE(dp,code); 
    if (code & 0x01)
    { xaULONG test,cnt;
      if (code == 0xe601) { i_cnt = im_size; break; }	/* End of Frame */
      test = code >> 7;		cnt = ((code >> 1) & 0x3f) + 2;
      if (test < 0x1cc)			/* Temporal/Spatial */
      { xaUSHORT *isp;
        if (test < 0x120) { isp = (xaUSHORT *)pptr; isp += i_cnt; }/*temporal */
	else isp = (xaUSHORT *)iptr; /* spatial */
	isp += (ARM_MOVL_X[test] + (ARM_MOVL_Y[test] * imagex));
	i_cnt += cnt;	if (i_cnt >= im_size) cnt -= (i_cnt - im_size);
	while(cnt--) *iptr++ = *isp++;
      }
      else if (test >= 0x1e0)
      { cnt = ((code >> 1) & 0x3ff) + 1;
	if (test >=0x1f0)				/* New N Pixels */
	{ xaLONG  arm_bcnt   = ((((cnt * 15) + 15) >> 4) << 1); /* dp bytes */
	  xaLONG  arm_b_bnum = 0;	xaULONG arm_b_bbuf = 0;
	  DEBUG_LEVEL1  fprintf(stderr,"New N Pixels %d\n",cnt);
	  i_cnt += cnt; if (i_cnt >= im_size) cnt -= (i_cnt - im_size);
	  while(cnt--)  /* pixels */
	  { /* Little Endian */
	    while(arm_b_bnum < 15) { arm_b_bbuf |=  (*dp++) << arm_b_bnum;
						arm_b_bnum += 8; arm_bcnt--; }
	    *iptr++ = (xaUSHORT)arm_get_pixel(arm_b_bbuf,map_flag,map,chdr);
	    arm_b_bbuf >>= 15;		arm_b_bnum -= 15;
	  }
	  while(arm_bcnt--) dp++; /* left overs */
	} else { iptr += cnt;  i_cnt += cnt; }		/* Copy/Skip */
      }
      else if (test == 0x1cc)
      { xaULONG pixel; xaUSHORT d;
	ARM_GET_CODE(dp,pixel);
	d = (xaUSHORT)arm_get_pixel(pixel,map_flag,map,chdr);
	i_cnt += cnt;	if (i_cnt >= im_size) cnt -= (i_cnt - im_size);
	while(cnt--) *iptr++ = d;
      } else fprintf(stderr,"ARM decode ERROR: %04x\n",code);
    }
    else			/* New Pixel */
    { *iptr++ = (xaUSHORT)arm_get_pixel((code>>1),map_flag,map,chdr); i_cnt++; }
  } 
 } /* end of byte 2 */

  dec_info->xs = dec_info->ys = 0; dec_info->xe = imagex; dec_info->ye = imagey;
  if (map_flag==xaTRUE) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}


xaULONG ARM_Get_Length(xin,arm)
XA_INPUT *xin;
XA_ANIM_SETUP *arm;
{ xaULONG d_cnt = 0;
  xaLONG i = arm->imagex * arm->imagey;
  xaULONG code;

  while(i > 0)
  {
    if ( xin->At_EOF(xin,-1) ) return(0);
    code = xin->Read_LSB_U16(xin); d_cnt += 2; 

    if (code & 0x01)
    { xaULONG test,cnt;

      if (code == 0xe601) return(d_cnt);	/* End of Frame */
      test = code >> 7;		
      cnt = ((code >> 1) & 0x3f) + 2;
      if (test < 0x120)		{ i -= cnt; }	/* Temporal */
      else if (test < 0x1cc)	{ i -= cnt; }	/* Spatial */ 
      else if (test >= 0x1e0)			/* New N Pix */
      { cnt = ((code >> 1) & 0x3ff) + 1;
	if (test >=0x1f0)			/* New N Pixels */
	{ xaULONG bs = ((cnt * 15) + 15) >> 4;
	  i -= cnt;
	  while(bs--) { xin->Read_LSB_U16(xin); d_cnt += 2; } 
	}
	else	{ i -= cnt; }			/* Copy/Skip */
      }
      else if (test == 0x1cc)				/* Duplicate */
      {
        if (cnt <= 2) {fprintf(stderr,"DUP CNT ERR\n"); continue; }
        xin->Read_LSB_U16(xin); d_cnt += 2;
	i -= cnt;
      }
      else 
      {
	fprintf(stderr,"ARM Get len error: %04x\n",code);
      }
    }
    else { i--; }				/* New Pixel */
  }
  code = xin->Read_LSB_U16(xin); 
  if (code == 0xe601) d_cnt += 2;
  return(d_cnt);
}

void arm_yuv_to_rgb(iy,iu,iv,ir,ig,ib)
xaULONG iy,iu,iv;
xaULONG *ir,*ig,*ib;
{ float yy,uu,vv;
  float u2,u3,v2,v3;
  float DD;
  float rr,gg,bb;
  

  DD = 255.0;
  yy =  ((float)(iy)/31.0);
  vv =  ((float)(iv)/15.0);
  uu =  ((float)(iu)/15.0);
  if (vv > 1.0) vv = -((31.0 - (float)(iv) ) / 15.0);
  if (uu > 1.0) uu = -((31.0 - (float)(iu) ) / 15.0);
  v2 = vv * 0.701;
  u2 = uu * 0.886;
  v3 = vv * (0.299 * 0.701 / 0.587);
  u3 = uu * (0.114 * 0.886 / 0.587);

  bb = (yy + u2);
  rr = (yy + v2);
  gg = (yy - u3 - v3);
  DEBUG_LEVEL1 
	fprintf(stderr,"rr = %f gg = %f bb = %f\n",rr,gg,bb);
  bb *= 255.0;
  rr *= 255.0;
  gg *= 255.0;

  if (bb > 255.0) bb = 255.0; if (bb < 0.0) bb = 0.0;
  if (rr > 255.0) rr = 255.0; if (rr < 0.0) rr = 0.0;
  if (gg > 255.0) gg = 255.0; if (gg < 0.0) gg = 0.0;
  *ir = (xaULONG)(rr);
  *ig = (xaULONG)(gg);
  *ib = (xaULONG)(bb);

  DEBUG_LEVEL1
    fprintf(stderr,"YUV %d %d %d  RGB %d %d %d\n",iy,iu,iv,*ir,*ig,*ib);
}


/*****************************
 *  Function To allocate and initialized the following tables and buffers:
 *
 *********/
void ARM_Init_Tables(anim_hdr,imagex,imagey)
XA_ANIM_HDR *anim_hdr;
xaULONG imagex,imagey;
{ xaULONG i; xaLONG x,y;

 XA_Add_Func_To_Free_Chain(anim_hdr,ARM_Free_Stuff);
 /* Prev Buffer Allocation */
 if (cmap_color_func == 4)  i = xaMAX(3,x11_bytes_pixel);
 else i = x11_bytes_pixel;
 i *= imagex * imagey;

 if (ARM_prev_buff == 0)
 { ARM_prev_buff_size = i;
   ARM_prev_buff = (xaUBYTE *)malloc( ARM_prev_buff_size );
 }
 else if (i > ARM_prev_buff_size)
 { ARM_prev_buff_size = i;
   ARM_prev_buff = (xaUBYTE *)realloc( ARM_prev_buff, ARM_prev_buff_size );
 }
 if (ARM_prev_buff == 0) TheEnd1("ARM: prev_buff malloc err");

 /* Spatial/Temporal Tables Allocation */
 if (ARM_MOVL_X == 0)
 { ARM_MOVL_X = (xaLONG *)malloc( 459 * sizeof(xaLONG) );
   if (ARM_MOVL_X==0) TheEnd1("ARM: MOVL_X malloc err");
 }
 if (ARM_MOVL_Y == 0)
 { ARM_MOVL_Y = (xaLONG *)malloc( 459 * sizeof(xaLONG) );
   if (ARM_MOVL_Y==0) TheEnd1("ARM: MOVL_Y malloc err");
 }
 /* Temporal Encoding */
 x = y = -8;
 for(i = 0; i < 288; i++)
 { ARM_MOVL_X[i] = x;
   ARM_MOVL_Y[i] = y;
   x++; if (x > 8) { x = -8; y++; }
   if ((x==0) && (y==0))  x++; /* skip x = y = 0 case */
 }
 /* Spatial Encoding */
 x = y = -9;
 for(i = 288; i < 459; i++)
 { ARM_MOVL_X[i] = x;
   ARM_MOVL_Y[i] = y;
   x++; if (x > 9) { x = -9; y++; }
 }
}

/*****************************
 *  Function To Free  the following tables and buffers.
 *
 *********/
void ARM_Free_Stuff()
{
  if (ARM_MOVL_X) { free(ARM_MOVL_X); ARM_MOVL_X = 0; }
  if (ARM_MOVL_Y) { free(ARM_MOVL_Y); ARM_MOVL_Y = 0; }
  if (ARM_prev_buff) { free(ARM_prev_buff); ARM_prev_buff = 0; }
}


