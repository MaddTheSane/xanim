
/*
 * xa_movi.c
 *
 * Copyright (C) 1996,1998,1999 by Mark Podlipec. 
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
 * 01Dec97  Added support for uncompressed RGB Video Codec.
 * 18Mar98  Added support for uncompressed MVC2 Video Codec.
 ********************************/


#include "xa_movi.h" 

xaULONG MOVI_Read_File();
MOVI_FRAME *MOVI_Add_Frame();
void MOVI_Free_Frame_List();
void ACT_Setup_Delta();

float MOVI_Read_Float();
xaULONG MOVI_Read_Int();
xaULONG MOVI_Read_I_Compression();
xaULONG MOVI_Read_A_Compression();
xaULONG MOVI_Read_Header();
xaULONG MOVI_Read_Header2();
xaULONG MOVI_Read_Header3();
xaULONG MOVI_Parse_MOVI_HDR();
xaULONG MOVI_Parse_MOVI_I_HDR();
xaULONG MOVI_Parse_MOVI_A_HDR();
xaULONG MOVI_Read_Index2();
xaULONG MOVI_Read_Index3();
void MOVI_Add_Video_Frames();
void MOVI_Add_Audio_Frames();
xaULONG MOVI_Get_Color();
extern void CMAP_Cache_Init();
extern void CMAP_Cache_Clear();
extern void JPG_Setup_Samp_Limit_Table();


/* CODEC ROUTINES */
xaULONG MOVI_Decode_MVC1();
xaULONG MOVI_Decode_MVC2();
xaULONG MOVI_Decode_RGB32();
extern xaULONG JFIF_Decode_JPEG();
extern xaULONG FLI_Decode_BLACK();
extern void XA_Gen_YUV_Tabs();
extern void JPG_Alloc_MCU_Bufs();
extern xaULONG jpg_search_marker();

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
extern void ACT_Free_Act();
extern xaULONG XA_RGB24_To_CLR32();

extern void  UTIL_FPS_2_Time();
extern XA_ANIM_SETUP *XA_Get_Anim_Setup();
extern void XA_Free_Anim_Setup();

extern xaLONG xa_dither_flag;
extern xaUBYTE  *xa_byte_limit;

static xaULONG movi_audio_attempt;
static xaULONG movi_audio_type;
static xaULONG movi_audio_freq;
static xaULONG movi_audio_chans;
static xaULONG movi_audio_bps;
xaULONG XA_Add_Sound();

#define MOVI_ICOMP_UNK		0
#define MOVI_ICOMP_MVC1		1
#define MOVI_ICOMP_MVC2		2
#define MOVI_ICOMP_JPEG		10
#define MOVI_ICOMP_RGB32	55

#define MOVI_ACOMP_UNK	0
#define MOVI_ACOMP_100	100
#define MOVI_ACOMP_401 	401
#define MOVI_ACOMP_402 	402

static xaULONG movi_frame_cnt;
static MOVI_FRAME *movi_frame_start,*movi_frame_cur;

MOVI_FRAME *MOVI_Add_Frame(time,timelo,act)
xaULONG time,timelo;
XA_ACTION *act;
{
  MOVI_FRAME *fframe;
 
  fframe = (MOVI_FRAME *) malloc(sizeof(MOVI_FRAME));
  if (fframe == 0) TheEnd1("MOVI_Add_Frame: malloc err");
 
  fframe->time   = time;
  fframe->timelo = timelo;
  fframe->act = act;
  fframe->next = 0;
 
  if (movi_frame_start == 0) movi_frame_start = fframe;
  else movi_frame_cur->next = fframe;
 
  movi_frame_cur = fframe;
  movi_frame_cnt++;
  return(fframe);
}

void MOVI_Free_Frame_List(fframes)
MOVI_FRAME *fframes;
{
  MOVI_FRAME *ftmp;
  while(fframes != 0)
  {
    ftmp = fframes;
    fframes = fframes->next;
    FREE(ftmp,0xA000);
  }
}


xaULONG MOVI_Read_File(fname,anim_hdr,audio_attempt)
char *fname;
XA_ANIM_HDR *anim_hdr;
xaULONG audio_attempt;	/* xaTRUE if audio is to be attempted */
{ XA_INPUT *xin = anim_hdr->xin;
  xaLONG i,t_time;
  xaULONG t_timelo;
  XA_ANIM_SETUP *movi;
  MOVI_HDR   movi_hdr;
  
  movi = XA_Get_Anim_Setup();
  movi->vid_time = XA_GET_TIME( 100 ); /* default */
  movi->compression	= MOVI_ICOMP_UNK;

  movi_frame_cnt	= 0;
  movi_frame_start	= 0;
  movi_frame_cur	= 0;
  movi_audio_attempt	= audio_attempt;

  movi_hdr.version	= 0;
  movi_hdr.a_hdr	= 0;
  movi_hdr.i_hdr	= 0;

  if (MOVI_Read_Header(xin,anim_hdr,movi,&movi_hdr) == xaFALSE)
  { xin->Close_File(xin);
    return(xaFALSE);
  }

  if (movi_hdr.version == 3)
  {  if (MOVI_Read_Index3(xin,anim_hdr,movi,&movi_hdr)==xaFALSE) 
							return(xaFALSE);
  }
  else if (movi_hdr.version == 2)
  {  if (MOVI_Read_Index2(xin,anim_hdr,movi,&movi_hdr)==xaFALSE) 
							return(xaFALSE);
  }
  else 
  { xin->Close_File(xin);
    return(xaFALSE);
  }

  movi->cmap_frame_num = movi_hdr.i_hdr->frames / cmap_sample_cnt;

  MOVI_Add_Video_Frames(xin,anim_hdr,movi,&movi_hdr);
  MOVI_Add_Audio_Frames(xin,anim_hdr,movi,&movi_hdr);

  xin->Close_File(xin);
  if (movi_hdr.a_hdr) 
  { if (movi_hdr.a_hdr->off)
    { free(movi_hdr.a_hdr->off);
      free(movi_hdr.a_hdr->size);
    }
    free(movi_hdr.a_hdr); movi_hdr.a_hdr = 0; 
  } 
  if (movi_hdr.i_hdr) 
  { if (movi_hdr.i_hdr->off)
    { free(movi_hdr.i_hdr->off);
      free(movi_hdr.i_hdr->size);
    }
    free(movi_hdr.i_hdr); movi_hdr.i_hdr = 0;
  } 

  if (movi_frame_cnt == 0)
  { 
    fprintf(stderr,"MOVI: No supported video frames exist in this file.\n");
    return(xaFALSE);
  }

  anim_hdr->frame_lst = (XA_FRAME *)
				malloc( sizeof(XA_FRAME) * (movi_frame_cnt+1));
  if (anim_hdr->frame_lst == NULL) TheEnd1("MOVI_Read_File: frame malloc err");

  movi_frame_cur = movi_frame_start;
  i = 0;
  t_time = 0;
  t_timelo = 0;
  while(movi_frame_cur != 0)
  {
    if (i > movi_frame_cnt)
    {
      fprintf(stderr,"MOVI_Read_Anim: frame inconsistency %d %d\n",
                i,movi_frame_cnt);
      break;
    }
    anim_hdr->frame_lst[i].time_dur = movi_frame_cur->time;
    anim_hdr->frame_lst[i].zztime = t_time;
    t_time += movi_frame_cur->time;
    t_timelo += movi_frame_cur->timelo;
    while(t_timelo > (1<<24)) {t_time++; t_timelo -= (1<<24);}
    anim_hdr->frame_lst[i].act = movi_frame_cur->act;
    movi_frame_cur = movi_frame_cur->next;
    i++;
  }
  anim_hdr->imagex = movi->imagex;
  anim_hdr->imagey = movi->imagey;
  anim_hdr->imagec = movi->imagec;
  anim_hdr->imaged = 8; /* nop */
  anim_hdr->frame_lst[i].time_dur = 0;
  anim_hdr->frame_lst[i].zztime = -1;
  anim_hdr->frame_lst[i].act  = 0;
  anim_hdr->loop_frame = 0;
  if (!(xin->load_flag & XA_IN_LOAD_BUF)) anim_hdr->anim_flags |= ANIM_SNG_BUF;
  if (xin->load_flag & XA_IN_LOAD_FILE) anim_hdr->anim_flags |= ANIM_USE_FILE;
  anim_hdr->anim_flags |= ANIM_FULL_IM;
  anim_hdr->max_fvid_size = movi->max_fvid_size;
  anim_hdr->max_faud_size = movi->max_faud_size;
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
  MOVI_Free_Frame_List(movi_frame_start);
  XA_Free_Anim_Setup(movi);
/* POD TBD  Free movi_hdr */
  return(xaTRUE);
} /* end of read file */

/********************
 *
 *********************/
float MOVI_Read_Float(xin,size)
XA_INPUT *xin;
xaULONG size;
{ float fret = 0.0;
  float scale = 0.1;

 /** integer part */
 while(size--)
 { xaULONG dat = xin->Read_U8(xin);
   DEBUG_LEVEL1 fprintf(stderr,"<%x>",dat);
   if ( (dat >= 0x30) && (dat <= 0x39)) 
			fret = (10.0 * fret) + (float)(dat - 0x30);
   else if (dat <= 0) return(fret);
   else break;
 }
 /** fractional part */
 DEBUG_LEVEL1 fprintf(stderr,"<.>");
 while(size--)
 { xaULONG dat = xin->Read_U8(xin);
   DEBUG_LEVEL1 fprintf(stderr,"<%x>",dat);
   if ( (dat >= 0x30) && (dat <= 0x39)) 
   {
     fret += scale * (float)(dat - 0x30);
     scale /= 10.0;
   }
   else break;
 }
 DEBUG_LEVEL1 fprintf(stderr,"size %x\n",size);
 return(fret);
}


/********************
 *
 *********************/
xaULONG MOVI_Read_Int(xin,size)
XA_INPUT *xin;
xaULONG size;
{ xaULONG ret = 0;
 while(size--)
 { xaULONG dat = xin->Read_U8(xin);
   if ( (dat >= 0x30) && (dat <= 0x39)) ret = (10 * ret) + (dat - 0x30);
   else break;
 }
 return(ret);
}

/********************
 *
 *********************/
xaULONG MOVI_Read_I_Compression(xin,size)
XA_INPUT *xin;
xaULONG size;
{ xaULONG ret = 0;
  if (size >= 256) 
  { fprintf(stderr,"MOVI: string to long %d\n",size); 
    while(size--) (void)(xin->Read_U8(xin));
  }
  else
  { char tbuf[256]; char *p = tbuf;
    while(size--) *p++ = (char)(xin->Read_U8(xin) & 0xff);
    if (strcmp(tbuf,"1") == 0)		ret = MOVI_ICOMP_MVC1;
    else if (strcmp(tbuf,"2") == 0)	ret = MOVI_ICOMP_RGB32;
    else if (strcmp(tbuf,"10") == 0)	ret = MOVI_ICOMP_JPEG;
    else if (strcmp(tbuf,"MVC2") == 0)	ret = MOVI_ICOMP_MVC2;
    else
    { fprintf(stderr,"  MOVI: unknown video codec (%s)\n",tbuf);
      ret = MOVI_ICOMP_UNK;
    }
  }
  return(ret);
}

/********************
 *
 *********************/
xaULONG MOVI_Read_A_Compression(xin,size)
XA_INPUT *xin;
xaULONG size;
{ xaULONG ret = 0;
  if (size >= 256) 
  { fprintf(stderr,"MOVI: string to long %d\n",size); 
    while(size--) (void)(xin->Read_U8(xin));
  }
  else
  { char tbuf[256]; char *p = tbuf;
    while(size--) *p++ = (char)(xin->Read_U8(xin) & 0xff);
    if (strcmp(tbuf,"100") == 0)	ret = MOVI_ACOMP_100;
    else if (strcmp(tbuf,"401") == 0)	ret = MOVI_ACOMP_401;
    else if (strcmp(tbuf,"402") == 0)	ret = MOVI_ACOMP_402;
    else
    { fprintf(stderr,"  MOVI: unknown audio codec (%s)\n",tbuf);
      ret = MOVI_ACOMP_UNK;
    }
  }
  return(ret);
}

/********************
 *
 *********************/
xaULONG MOVI_Parse_MOVI_HDR(xin,movi_hdr,buf,size)
XA_INPUT *xin;
MOVI_HDR *movi_hdr;
char *buf;
xaULONG size;
{
  if (strcmp(buf,"__NUM_I_TRACKS") == 0)
				movi_hdr->i_tracks = MOVI_Read_Int(xin,size); 
  else if (strcmp(buf,"__NUM_A_TRACKS") == 0)
				movi_hdr->a_tracks = MOVI_Read_Int(xin,size); 
  else if (strcmp(buf,"LOOP_MODE") == 0)
				movi_hdr->loop_mode = MOVI_Read_Int(xin,size); 
  else if (strcmp(buf,"NUM_LOOPS") == 0)
				movi_hdr->num_loops = MOVI_Read_Int(xin,size); 
  else if (strcmp(buf,"OPTIMIZED") == 0)
				movi_hdr->optimized = MOVI_Read_Int(xin,size); 
  else /* unknown */
  {
    fprintf(stderr,"MOVI: unknown HDR var %s with size %d\n",buf,size);
    while(size--) (void)xin->Read_U8(xin);
  }
  return(xaTRUE);
}

/********************
 *
 *********************/
xaULONG MOVI_Parse_MOVI_A_HDR(xin,a_hdr,buf,size)
XA_INPUT *xin;
MOVI_A_HDR *a_hdr;
char *buf;
xaULONG size;
{
  if (strcmp(buf,"__DIR_COUNT") == 0)
				a_hdr->frames = MOVI_Read_Int(xin,size); 
  else if (strcmp(buf,"SAMPLE_WIDTH") == 0)
				a_hdr->width = MOVI_Read_Int(xin,size); 
  else if (strcmp(buf,"NUM_CHANNELS") == 0)
				a_hdr->chans = MOVI_Read_Int(xin,size); 
  else if (strcmp(buf,"AUDIO_FORMAT") == 0)
			a_hdr->format = MOVI_Read_A_Compression(xin,size); 
  else if (strcmp(buf,"SAMPLE_RATE") == 0)
				a_hdr->rate = MOVI_Read_Float(xin,size); 
  else if (strcmp(buf,"COMPRESSION") == 0)
			a_hdr->compression = MOVI_Read_A_Compression(xin,size); 
  else /* unknown */
  {
    fprintf(stderr,"MOVI: unknown A_HDR var %s with size %d\n",buf,size);
    while(size--) (void)xin->Read_U8(xin);
  }
  return(xaTRUE);
}

/********************
 *
 *********************/
xaULONG MOVI_Parse_MOVI_I_HDR(xin,i_hdr,buf,size)
XA_INPUT *xin;
MOVI_I_HDR *i_hdr;
char *buf;
xaULONG size;
{
  if (strcmp(buf,"__DIR_COUNT") == 0)
			i_hdr->frames = MOVI_Read_Int(xin,size); 
  else if (strcmp(buf,"WIDTH") == 0)
			i_hdr->width = MOVI_Read_Int(xin,size); 
  else if (strcmp(buf,"HEIGHT") == 0)
			i_hdr->height = MOVI_Read_Int(xin,size); 
  else if (strcmp(buf,"COMPRESSION") == 0)
			i_hdr->compression = MOVI_Read_I_Compression(xin,size); 
  else if (strcmp(buf,"INTERLACING") == 0)
			i_hdr->interlacing = MOVI_Read_Int(xin,size); 
  else if (strcmp(buf,"PACKING") == 0)
			i_hdr->packing = MOVI_Read_Int(xin,size); 
  else if (strcmp(buf,"ORIENTATION") == 0)
			i_hdr->orientation = MOVI_Read_Int(xin,size); 
  else if (strcmp(buf,"PIXEL_ASPECT") == 0)
			i_hdr->pixel_aspect = MOVI_Read_Float(xin,size); 
  else if (strcmp(buf,"FPS") == 0)
			i_hdr->fps = MOVI_Read_Float(xin,size); 
  else if (strcmp(buf,"Q_SPATIAL") == 0)
			i_hdr->q_spatial = MOVI_Read_Float(xin,size); 
  else if (strcmp(buf,"Q_TEMPORAL") == 0)
			i_hdr->q_temporal = MOVI_Read_Float(xin,size); 
  else /* unknown */
  {
    fprintf(stderr,"MOVI: unknown I HDR var %s with size %d\n",buf,size);
    while(size--) (void)xin->Read_U8(xin);
  }
  return(xaTRUE);
}


xaULONG MOVI_Read_Header(xin,anim_hdr,movi,movi_hdr)
XA_INPUT *xin;
XA_ANIM_HDR *anim_hdr;
XA_ANIM_SETUP *movi;
MOVI_HDR *movi_hdr;
{ xaULONG t,version;

  t 			= xin->Read_MSB_U32(xin); /* skip MOVI */

  /* Really have no clue about this, BUT some have
   *
   *        MOVI 0000_00003 0000_0000   => MOVI_Read_Header3
   *
   *        MOVI 0002_width hite_0000   => MOVI_Read_Header2
   */

  version 		= xin->Read_MSB_U16(xin); 
  if (version == 0)
  { version = xin->Read_MSB_U16(xin);
    if (version == 3)
    { movi_hdr->version = 3;
      t = xin->Read_MSB_U32(xin); /* unknown */
      return( MOVI_Read_Header3(xin,anim_hdr,movi,movi_hdr) );
    }
  }
  else if (version == 2)
  {  movi_hdr->version = 2;
     t = xin->Read_MSB_U16(xin); /* width */
     t = xin->Read_MSB_U16(xin); /* height */
     t = xin->Read_MSB_U16(xin); /* unknown */
     return( MOVI_Read_Header2(xin,anim_hdr,movi,movi_hdr) );
  }
  /* fall through cases */
  fprintf(stderr,"MOVI: Unknown format variation\n");
  return(xaFALSE);
}

xaULONG MOVI_Read_Header3(xin,anim_hdr,movi,movi_hdr)
XA_INPUT *xin;
XA_ANIM_HDR *anim_hdr;
XA_ANIM_SETUP *movi;
MOVI_HDR *movi_hdr;
{ xaULONG t,size;
  xaULONG vars;
  char buf[16];


  /*** MOVI Header Group ***/
  movi_hdr->i_tracks	= 0;
  movi_hdr->a_tracks	= 0;
  movi_hdr->loop_mode	= 0;
  movi_hdr->num_loops	= 0;
  movi_hdr->optimized	= 0;
  movi_hdr->a_hdr	= 0;
  movi_hdr->i_hdr	= 0;

  t 			= xin->Read_MSB_U32(xin); /* unk */
  vars 			= xin->Read_MSB_U32(xin); /* num of variables */
  t 			= xin->Read_MSB_U32(xin); /* unk */
  while(vars--)
  { int i; for(i=0;i<16;i++) buf[i] = xin->Read_U8(xin);
    size = xin->Read_MSB_U32(xin); /* size */
    DEBUG_LEVEL1 fprintf(stderr," MOVI: size %x buf %s\n",size,buf);
    if (MOVI_Parse_MOVI_HDR(xin,movi_hdr,buf,size)==xaFALSE) return(xaFALSE);
  }

  /*** MOVI Audio Header Group ***/

  if (movi_hdr->a_tracks > 1)
  { fprintf(stderr,"MOVI: multiple audio tracks not yet tolerated\n");
    return(xaFALSE);
  }
  if (movi_hdr->a_tracks == 1)  /* assume audio is optional */
  { MOVI_A_HDR *a_hdr = (MOVI_A_HDR *)malloc(sizeof(MOVI_A_HDR));
    if (a_hdr == 0) TheEnd1("MOVI: a_hdr malloc err");
/* POD TBD - free up MOVI_A_HDR and MOVI_I_HDR at end of read */
    movi_hdr->a_hdr = a_hdr;
    a_hdr->frames	= 0;
    a_hdr->width	= 0;
    a_hdr->chans	= 0;
    a_hdr->format	= 0;
    a_hdr->rate		= 0.0;
    a_hdr->compression	= 0;

    t 			= xin->Read_MSB_U32(xin); /* unk */
    vars 		= xin->Read_MSB_U32(xin); /* num of variables */
    t 			= xin->Read_MSB_U32(xin); /* unk */
    while(vars--)
    { int i; for(i=0;i<16;i++) buf[i] = xin->Read_U8(xin);
      size = xin->Read_MSB_U32(xin); /* size */
      DEBUG_LEVEL1 fprintf(stderr," MOVI: size %x buf %s\n",size,buf);
      if (MOVI_Parse_MOVI_A_HDR(xin,a_hdr,buf,size)==xaFALSE) return(xaFALSE);
    }
    if (xa_verbose)
    {
      fprintf(stdout,
		"  Audio: freq %5.1f chans %d  bps %d form %d comp %d\n",
		a_hdr->rate, a_hdr->chans, a_hdr->width, a_hdr->format, 
		a_hdr->compression);
    }
/* POD check for 0 frames?? */
    if (a_hdr->frames == 0) TheEnd1("MOVI: Audio err");
    a_hdr->off = (xaULONG *)malloc( a_hdr->frames * sizeof(xaULONG) );
    a_hdr->size = (xaULONG *)malloc( a_hdr->frames * sizeof(xaULONG) );

    movi_audio_freq =  (xaULONG)a_hdr->rate;
    movi_audio_chans = a_hdr->chans;
    movi_audio_bps   = a_hdr->width;
    movi_audio_type = XA_AUDIO_INVALID;

    if (   (a_hdr->compression == MOVI_ACOMP_100)
	&& (a_hdr->format == MOVI_ACOMP_401))
    {
      movi_audio_type = XA_AUDIO_SIGNED;
    }
    else if (   (a_hdr->compression == MOVI_ACOMP_100)
	     && (a_hdr->format == MOVI_ACOMP_402))
    {
      movi_audio_type = XA_AUDIO_LINEAR;
    }
  
    if ( (movi_audio_bps > 2) || (movi_audio_bps == 0) )
					movi_audio_type = XA_AUDIO_INVALID;
    if (movi_audio_chans == 2) movi_audio_type |= XA_AUDIO_STEREO_MSK;
    if (movi_audio_bps == 2)   
    {
      movi_audio_type |= XA_AUDIO_BPS_2_MSK;
      movi_audio_type |= XA_AUDIO_BIGEND_MSK;
    }
    if (movi_audio_type == XA_AUDIO_INVALID)
    {
      if (movi_audio_attempt == xaTRUE) 
	fprintf(stderr,"MOVI: Audio Type unsupported %d \n",
						a_hdr->compression);
      movi_audio_attempt = xaFALSE;
    }
  } /* end audio group */

  /*** MOVI Video Header Group ***/
  if (movi_hdr->i_tracks > 1)
  { fprintf(stderr,"MOVI: multiple video tracks not yet tolerated\n");
    return(xaFALSE);
  }
  if (movi_hdr->i_tracks == 1)
  { MOVI_I_HDR *i_hdr = (MOVI_I_HDR *)malloc(sizeof(MOVI_I_HDR));
    if (i_hdr == 0) TheEnd1("MOVI: i_hdr malloc err");
    movi_hdr->i_hdr 	= i_hdr;
    i_hdr->frames	= 0;
    i_hdr->width	= 0;
    i_hdr->height	= 0;
    i_hdr->compression	= 0;
    i_hdr->interlacing	= 0;
    i_hdr->packing	= 0;
    i_hdr->orientation	= 0;
    i_hdr->pixel_aspect	= 0.0;
    i_hdr->fps		= 0.0;
    i_hdr->q_spatial	= 0.0;
    i_hdr->q_temporal	= 0.0;

    t 			= xin->Read_MSB_U32(xin); /* unk */
    vars 		= xin->Read_MSB_U32(xin); /* num of variables */
    t 			= xin->Read_MSB_U32(xin); /* unk */
    while(vars--)
    { int i; for(i=0;i<16;i++) buf[i] = xin->Read_U8(xin);
      size = xin->Read_MSB_U32(xin); /* size */
      DEBUG_LEVEL1 fprintf(stderr," MOVI: size %x buf %s\n",size,buf);
      if (MOVI_Parse_MOVI_I_HDR(xin,i_hdr,buf,size)==xaFALSE) return(xaFALSE);
    }
    if (xa_verbose)
    {
      fprintf(stdout, "  Video: %dx%d  codec ", i_hdr->width, i_hdr->height);
      switch(i_hdr->compression)
      { case MOVI_ICOMP_MVC1: fprintf(stdout,"MVC1"); break;
	case MOVI_ICOMP_RGB32: fprintf(stdout,"RGB"); break;
	case MOVI_ICOMP_MVC2: fprintf(stdout,"MVC2"); break;
	case MOVI_ICOMP_JPEG: fprintf(stdout,"JPEG"); break;
	default: fprintf(stdout,"unk(%d)",i_hdr->compression);
      }
      fprintf(stdout," frames %d fps %3.2f\n",i_hdr->frames,i_hdr->fps);
    }

/* POD check for 0 frames?? */
    if (i_hdr->frames == 0) TheEnd1("MOVI: Video err");
    i_hdr->off = (xaULONG *)malloc( i_hdr->frames * sizeof(xaULONG) );
    i_hdr->size = (xaULONG *)malloc( i_hdr->frames * sizeof(xaULONG) );

    UTIL_FPS_2_Time(movi, ((double)(i_hdr->fps)) );

    switch(i_hdr->compression)
    {  
      case MOVI_ICOMP_JPEG:
        movi->compression = MOVI_ICOMP_JPEG;
        movi->depth = 24;
        movi->imagex = i_hdr->width;
        movi->imagey = i_hdr->height;
        movi->imagex = 4 * ((movi->imagex + 3)/4);
        movi->imagey = 4 * ((movi->imagey + 3)/4);
        XA_Gen_YUV_Tabs(anim_hdr);
        JPG_Alloc_MCU_Bufs(anim_hdr,movi->imagex,0,xaFALSE);
	JPG_Setup_Samp_Limit_Table(anim_hdr);
        break;

      case MOVI_ICOMP_MVC1:
        movi->compression = MOVI_ICOMP_MVC1;
        movi->depth = 16;
        movi->imagex = i_hdr->width;
        movi->imagey = i_hdr->height;
        movi->imagex = 4 * ((movi->imagex + 3)/4);
        movi->imagey = 4 * ((movi->imagey + 3)/4);
	JPG_Setup_Samp_Limit_Table(anim_hdr);
        break;

      case MOVI_ICOMP_RGB32:
        movi->compression = MOVI_ICOMP_RGB32;
        movi->depth = 24;
        movi->imagex = i_hdr->width;
        movi->imagey = i_hdr->height;
	/* same */
        break;

      case MOVI_ICOMP_MVC2:
        movi->compression = MOVI_ICOMP_MVC2;
        movi->depth = 16;
        movi->imagex = i_hdr->width;
        movi->imagey = i_hdr->height;
        movi->imagex = 4 * ((movi->imagex + 3)/4);
        movi->imagey = 4 * ((movi->imagey + 3)/4);
	JPG_Setup_Samp_Limit_Table(anim_hdr);
        break;

      default:
        movi->compression = MOVI_ICOMP_UNK;
        movi->depth = 8;
        movi->imagex = i_hdr->width;
        movi->imagey = i_hdr->height;
        fprintf(stderr," MOVI: Unknown Video Codec not supported\n"); 
        return(xaFALSE);
        break;
    }

	/* POD temp kludge for unknown movies */
/* Debug and testing only
*/
    if (movi->compression == MOVI_ICOMP_UNK)
    {
      movi->chdr = CMAP_Create_332(movi->cmap,&movi->imagec);
    }
    else 
    if ( (cmap_true_map_flag == xaFALSE) /* depth 16 and not true_map */
        || (!(xin->load_flag & XA_IN_LOAD_BUF)) )
    {
       if (cmap_true_to_332 == xaTRUE)
             movi->chdr = CMAP_Create_332(movi->cmap,&movi->imagec);
       else    movi->chdr = CMAP_Create_Gray(movi->cmap,&movi->imagec);
    }

    if ( (movi->pic==0) && (xin->load_flag & XA_IN_LOAD_BUF))
    {
      movi->pic_size = movi->imagex * movi->imagey;
      if ( (cmap_true_map_flag == xaTRUE) && (movi->depth > 8) )
                movi->pic = (xaUBYTE *) malloc( 3 * movi->pic_size );
      else movi->pic = (xaUBYTE *) malloc( XA_PIC_SIZE(movi->pic_size) );
      if (movi->pic == 0) TheEnd1("MOVI_Buffer_Action: malloc failed");
    }
  }

  return(xaTRUE);
}

xaULONG MOVI_Read_Header2(xin,anim_hdr,movi,movi_hdr)
XA_INPUT *xin;
XA_ANIM_HDR *anim_hdr;
XA_ANIM_SETUP *movi;
MOVI_HDR *movi_hdr;
{ xaULONG t;

  /*** MOVI Header Group ***/
  movi_hdr->i_tracks	= 0;
  movi_hdr->a_tracks	= 0;
  movi_hdr->loop_mode	= 0;
  movi_hdr->num_loops	= 0;
  movi_hdr->optimized	= 0;
  movi_hdr->a_hdr	= 0;
  movi_hdr->i_hdr	= 0;

  t 			= xin->Read_MSB_U32(xin); /* unk 0000_0000 */
  t 			= xin->Read_MSB_U32(xin); /* unk 402e_0000 */
  t 			= xin->Read_MSB_U32(xin); /* unk 0000_0000 */
  t 			= xin->Read_MSB_U32(xin); /* unk 0002_0001 */


  { MOVI_I_HDR *i_hdr = (MOVI_I_HDR *)malloc(sizeof(MOVI_I_HDR));
    if (i_hdr == 0) TheEnd1("MOVI: i_hdr malloc err");

    movi_hdr->i_hdr 	= i_hdr;
    i_hdr->frames	= 0;
    i_hdr->width	= 0;
    i_hdr->height	= 0;
    i_hdr->compression	= 0;
    i_hdr->interlacing	= 0;
    i_hdr->packing	= 0;
    i_hdr->orientation	= 0;
    i_hdr->pixel_aspect	= 0.0;
    i_hdr->fps		= 0.0;
    i_hdr->q_spatial	= 0.0;
    i_hdr->q_temporal	= 0.0;

    i_hdr->frames	= xin->Read_MSB_U32(xin);
    i_hdr->compression	= xin->Read_MSB_U32(xin); /* ??? */
    i_hdr->width	= xin->Read_MSB_U32(xin);
    i_hdr->height	= xin->Read_MSB_U32(xin);

    t 			= xin->Read_MSB_U32(xin); /* unk 0000_0004 */
    t 			= xin->Read_MSB_U32(xin); /* unk 0000_0002 */
    t 			= xin->Read_MSB_U32(xin); /* unk 0000_0001 */
    t 			= xin->Read_MSB_U32(xin); /* unk 0000_0000 */
    t 			= xin->Read_MSB_U32(xin); /* unk 0000_0000 */

    t 			= xin->Read_MSB_U32(xin); /* unk 0000_0000 */
    t 			= xin->Read_MSB_U32(xin); /* unk 0000_0000 */
    t 			= xin->Read_MSB_U32(xin); /* unk 0000_0000 */
    t 			= xin->Read_MSB_U32(xin); /* unk 0000_0000 */

    t = 0x80; while(t--) (void)xin->Read_U8(xin);	/* video title */
    t = 0x80; while(t--) (void)xin->Read_U8(xin);	/* audio title */
    t = 0x80; while(t--) (void)xin->Read_U8(xin);	/* unknown     */
    t = 0x80; while(t--) (void)xin->Read_U8(xin);	/* unknown     */

    i_hdr->fps = 15.0;   /* POD: TBD just a guess */
    if (xa_verbose)
    {
      fprintf(stdout,
		"  Video: %dx%d  comp %d  fps %3.2f\n",
		i_hdr->width, i_hdr->height, i_hdr->compression, i_hdr->fps);
    }

/*POD TBD free up i_hdr off and size arrays same for a_hdr */
       /* POD check for 0 frames?? */
    if (i_hdr->frames == 0) TheEnd1("MOVI: Video err");
    i_hdr->off = (xaULONG *)malloc( i_hdr->frames * sizeof(xaULONG) );
    i_hdr->size = (xaULONG *)malloc( i_hdr->frames * sizeof(xaULONG) );

    UTIL_FPS_2_Time(movi, ((double)(i_hdr->fps)) );

    switch(i_hdr->compression)
    {  
      case 1:
        movi->compression = MOVI_ICOMP_MVC1;
        movi->depth = 16;
        movi->imagex = i_hdr->width;
        movi->imagey = i_hdr->height;
        movi->imagex = 4 * ((movi->imagex + 3)/4);
        movi->imagey = 4 * ((movi->imagey + 3)/4);
	JPG_Setup_Samp_Limit_Table(anim_hdr);
        break;

      case 2:
        movi->compression = MOVI_ICOMP_RGB32;
        movi->depth = 24;
        movi->imagex = i_hdr->width;
        movi->imagey = i_hdr->height;
	/* same */
        break;

      default:
        movi->compression = MOVI_ICOMP_UNK;
        movi->depth = 8;
        movi->imagex = i_hdr->width;
        movi->imagey = i_hdr->height;
        break;
    }

/* POD this part can be shared with Header3 somehow */
	/* POD temp kludge for unknown movies */
    if (movi->compression == MOVI_ICOMP_UNK)
    {
      movi->chdr = CMAP_Create_332(movi->cmap,&movi->imagec);
    }
    else if ( (cmap_true_map_flag == xaFALSE) /* depth 16 and not true_map */
        || (!(xin->load_flag & XA_IN_LOAD_BUF)) )
    {
       if (cmap_true_to_332 == xaTRUE)
             movi->chdr = CMAP_Create_332(movi->cmap,&movi->imagec);
       else    movi->chdr = CMAP_Create_Gray(movi->cmap,&movi->imagec);
    }

    if ( (movi->pic==0) && (xin->load_flag & XA_IN_LOAD_BUF))
    {
      movi->pic_size = movi->imagex * movi->imagey;
      if ( (cmap_true_map_flag == xaTRUE) && (movi->depth > 8) )
                movi->pic = (xaUBYTE *) malloc( 3 * movi->pic_size );
      else movi->pic = (xaUBYTE *) malloc( XA_PIC_SIZE(movi->pic_size) );
      if (movi->pic == 0) TheEnd1("MOVI_Buffer_Action: malloc failed");
    }
  }

  return(xaTRUE);
}


xaULONG MOVI_Read_Index3(xin,anim_hdr,movi,movi_hdr)
XA_INPUT *xin;
XA_ANIM_HDR *anim_hdr;
XA_ANIM_SETUP *movi;
MOVI_HDR *movi_hdr;
{ xaLONG i,t; 

  if (movi_hdr->a_hdr)  /* do audio 1st */
  { MOVI_A_HDR *a_hdr = movi_hdr->a_hdr;
    for(i=0; i< a_hdr->frames; i++)
    {
       a_hdr->off[i]	= xin->Read_MSB_U32(xin);
       a_hdr->size[i]	= xin->Read_MSB_U32(xin);
       t		= xin->Read_MSB_U32(xin);  /* unk */
       t		= xin->Read_MSB_U32(xin);  /* unk */
DEBUG_LEVEL1 fprintf(stderr,"A: %x %x\n",a_hdr->off[i], a_hdr->size[i]);
    }
  }
  if (movi_hdr->i_hdr) 
  { MOVI_I_HDR *i_hdr = movi_hdr->i_hdr;
    for(i=0; i< i_hdr->frames; i++)
    {
       i_hdr->off[i]	= xin->Read_MSB_U32(xin);
       i_hdr->size[i]	= xin->Read_MSB_U32(xin);
       t		= xin->Read_MSB_U32(xin);  /* unk */
       t		= xin->Read_MSB_U32(xin);  /* unk */
DEBUG_LEVEL1 fprintf(stderr,"I: %x %x\n",i_hdr->off[i], i_hdr->size[i]);
    }

  }
  return(xaTRUE);
}

xaULONG MOVI_Read_Index2(xin,anim_hdr,movi,movi_hdr)
XA_INPUT *xin;
XA_ANIM_HDR *anim_hdr;
XA_ANIM_SETUP *movi;
MOVI_HDR *movi_hdr;
{ xaLONG i,t; 

  if (movi_hdr->a_hdr)  /* do audio 1st */
  { MOVI_A_HDR *a_hdr = movi_hdr->a_hdr;
    for(i=0; i< a_hdr->frames; i++)
    { a_hdr->off[i]	= xin->Read_MSB_U32(xin);
      t		= xin->Read_MSB_U32(xin);  /* unk */
      a_hdr->size[i]	= xin->Read_MSB_U32(xin);
      t		= xin->Read_MSB_U32(xin);  /* unk */
      t		= xin->Read_MSB_U32(xin);  /* unk */
      DEBUG_LEVEL1 fprintf(stderr,"A: %x %x\n",
				a_hdr->off[i], a_hdr->size[i]);
    }
  }
  if (movi_hdr->i_hdr) 
  { MOVI_I_HDR *i_hdr = movi_hdr->i_hdr;
    for(i=0; i< i_hdr->frames; i++)
    {  i_hdr->off[i]	= xin->Read_MSB_U32(xin);
       t		= xin->Read_MSB_U32(xin);  /* unk */
       i_hdr->size[i]	= xin->Read_MSB_U32(xin);
       t		= xin->Read_MSB_U32(xin);  /* unk */
       t		= xin->Read_MSB_U32(xin);  /* unk */
       DEBUG_LEVEL1 fprintf(stderr,"I: %x %x\n",
				i_hdr->off[i], i_hdr->size[i]);
    }
  }
  return(xaTRUE);
}


void MOVI_Add_Video_Frames(xin,anim_hdr,movi,movi_hdr)
XA_INPUT *xin;
XA_ANIM_HDR *anim_hdr;
XA_ANIM_SETUP *movi;
MOVI_HDR *movi_hdr;
{ MOVI_I_HDR *i_hdr = movi_hdr->i_hdr;
  XA_ACTION *act = 0;
  int i;

  if (i_hdr == 0) return;
  for(i = 0; i < i_hdr->frames; i++)
  { xaULONG dlta_len;
    ACT_DLTA_HDR *dlta_hdr;
  
    dlta_len = i_hdr->size[i];

    if (act == 0) act = ACT_Get_Action(anim_hdr,ACT_DELTA);
    if (xin->load_flag & XA_IN_LOAD_FILE)
    { dlta_hdr = (ACT_DLTA_HDR *) malloc(sizeof(ACT_DLTA_HDR));
      if (dlta_hdr == 0) TheEnd1("MOVI: dlta malloc err");
      act->data = (xaUBYTE *)dlta_hdr;
      dlta_hdr->flags = ACT_SNGL_BUF;
      dlta_hdr->fsize = dlta_len;
      dlta_hdr->fpos  = i_hdr->off[i];
      if (dlta_len > movi->max_fvid_size) movi->max_fvid_size = dlta_len;
    }
    else
    { xaULONG d; xaLONG ret;
      d = dlta_len + (sizeof(ACT_DLTA_HDR));
      dlta_hdr = (ACT_DLTA_HDR *) malloc( d );
      if (dlta_hdr == 0) TheEnd1("MOVI: malloc failed");
      act->data = (xaUBYTE *)dlta_hdr;
      dlta_hdr->flags = ACT_SNGL_BUF | DLTA_DATA;
      dlta_hdr->fpos = 0; dlta_hdr->fsize = dlta_len;
      ret = xin->Seek_FPos(xin, i_hdr->off[i], 0);
      ret = xin->Read_Block(xin,dlta_hdr->data,dlta_len);
      if (ret < dlta_len)
      {
        if (xa_verbose) fprintf(stderr,
		"MOVI: read err of frame %d: pos %d size %d (%d)\n",
		i,i_hdr->off[i],dlta_len,ret);
	if ((movi->compression == MOVI_ICOMP_RGB32) && (ret > 0))
	{ 
	  dlta_hdr->fsize = ret;
	}
	else 
	{ 
	  free(dlta_hdr);
	  act->data = 0;
	  continue;
	}
      }
    }
    MOVI_Add_Frame(movi->vid_time,movi->vid_timelo,act);
    dlta_hdr->xpos = dlta_hdr->ypos = 0;
    dlta_hdr->xsize = movi->imagex;
    dlta_hdr->ysize = movi->imagey;

    switch(movi->compression)
    { case MOVI_ICOMP_JPEG:
	dlta_hdr->special = 0;
	dlta_hdr->extra = (void *)(0x0);
	dlta_hdr->xapi_rev = 0x0001;
	dlta_hdr->delta = JFIF_Decode_JPEG;
	break;
      case MOVI_ICOMP_MVC1:
	dlta_hdr->special = 0;
	dlta_hdr->extra = (void *)(0x0);
	dlta_hdr->xapi_rev = 0x0001;
	dlta_hdr->delta = MOVI_Decode_MVC1;
	break;
      case MOVI_ICOMP_MVC2:
	dlta_hdr->special = 0;
	dlta_hdr->extra = (void *)(0x0);
	dlta_hdr->xapi_rev = 0x0001;
	dlta_hdr->delta = MOVI_Decode_MVC2;
	break;
      case MOVI_ICOMP_RGB32:
	dlta_hdr->special = 0;
	dlta_hdr->extra = (void *)(0x0);
	dlta_hdr->xapi_rev = 0x0001;
	dlta_hdr->delta = MOVI_Decode_RGB32;
	break;
      default:
	dlta_hdr->special = 0;
	dlta_hdr->extra = (void *)(0x0);
	dlta_hdr->xapi_rev = 0x0001;
	dlta_hdr->delta = FLI_Decode_BLACK;
	break;
    }
    ACT_Setup_Delta(movi,act,dlta_hdr,xin);
    act = 0; /* indicate we've used action pointer */
  } /* end of i thru frames */
  if (act) ACT_Free_Act(act);
}

/***********
 *
 ***********/
void MOVI_Add_Audio_Frames(xin,anim_hdr,movi,movi_hdr)
XA_INPUT *xin;
XA_ANIM_HDR *anim_hdr;
XA_ANIM_SETUP *movi;
MOVI_HDR *movi_hdr;
{ MOVI_A_HDR *a_hdr = movi_hdr->a_hdr;
  int i;

  if (a_hdr == 0) return;
  for(i = 0; i < a_hdr->frames; i++)
  { xaULONG snd_size = a_hdr->size[i];

    if (movi_audio_attempt==xaTRUE)
    { xaLONG ret;
      if (xin->load_flag & XA_IN_LOAD_FILE)
      { xaLONG rets;
	rets = XA_Add_Sound(anim_hdr,0,movi_audio_type, a_hdr->off[i],
                        movi_audio_freq, snd_size,
                        &movi->aud_time,&movi->aud_timelo, 0, 0);
	if (rets==xaFALSE)			movi_audio_attempt = xaFALSE;
	if (snd_size > movi->max_faud_size)	movi->max_faud_size = snd_size;

      }
      else
      { xaUBYTE *snd = (xaUBYTE *)malloc(snd_size);
	if (snd==0) TheEnd1("MOVI: snd malloc err");
	ret = xin->Seek_FPos( xin, a_hdr->off[i], 0);
	ret = xin->Read_Block(xin, snd, snd_size);
	if (ret < snd_size) fprintf(stderr,"MOVI: snd rd err\n");
	else
	{ int rets;
	  rets = XA_Add_Sound(anim_hdr,snd,movi_audio_type, -1,
                                        movi_audio_freq, snd_size,
                                        &movi->aud_time, &movi->aud_timelo, 0, 0);
	  if (rets==xaFALSE) movi_audio_attempt = xaFALSE;
	}
      }
    }
  } /* end of i thru chunks */
}


#define MOVI_BLOCK_DEC(x,y,imagex) { x += 4; if (x>=imagex) { x=0; y -= 4; } }
#define MOVI_BLOCK_INC(x,y,imagex) { x += 4; if (x>=imagex) { x=0; y += 4; } }
#define MOVI_GET_8(data,dptr) {data = *dptr++; }
#define MOVI_GET_16(data,dptr) {data = *dptr++ << 8; data |= *dptr++; }

#define MVC1_COLOR2(dp,p0,c0,c1,c2,c3,c4,c5,c6,c7,mflag,map,chdr,CAST) \
{ c0 = c2 = c4 = c6 = (CAST)MOVI_Get_Color(p0,mflag,map,chdr);	\
  p0 = *dp++ << 8; p0 |= *dp++;						\
  c1 = c3 = c5 = c7 = (CAST)MOVI_Get_Color(p0,mflag,map,chdr);	}

#define MVC1_COLOR8(dp,p0,c0,c1,c2,c3,c4,c5,c6,c7,mflag,map,chdr,CAST) \
{ c0 = (CAST)MOVI_Get_Color(p0,mflag,map,chdr);		\
  p0 = *dp++ << 8; p0 |= *dp++; c1 = (CAST)MOVI_Get_Color(p0,mflag,map,chdr); \
  p0 = *dp++ << 8; p0 |= *dp++; c2 = (CAST)MOVI_Get_Color(p0,mflag,map,chdr); \
  p0 = *dp++ << 8; p0 |= *dp++; c3 = (CAST)MOVI_Get_Color(p0,mflag,map,chdr); \
  p0 = *dp++ << 8; p0 |= *dp++; c4 = (CAST)MOVI_Get_Color(p0,mflag,map,chdr); \
  p0 = *dp++ << 8; p0 |= *dp++; c5 = (CAST)MOVI_Get_Color(p0,mflag,map,chdr); \
  p0 = *dp++ << 8; p0 |= *dp++; c6 = (CAST)MOVI_Get_Color(p0,mflag,map,chdr); \
  p0 = *dp++ << 8; p0 |= *dp++; c7 = (CAST)MOVI_Get_Color(p0,mflag,map,chdr); }

#define MVC1_BLK1(ip,c0) { \
  *ip++ = c0;			\
  *ip++ = c0;			\
  *ip++ = c0;			\
  *ip   = c0; ip += row_inc;	\
  *ip++ = c0;			\
  *ip++ = c0;			\
  *ip++ = c0;			\
  *ip   = c0; ip += row_inc;	\
  *ip++ = c0;			\
  *ip++ = c0;			\
  *ip++ = c0;			\
  *ip   = c0; ip += row_inc;	\
  *ip++ = c0;			\
  *ip++ = c0;			\
  *ip++ = c0;			\
  *ip   = c0;			}

#define MVC1_BLK8(ip,mask,c0,c1,c2,c3,c4,c5,c6,c7) { \
  *ip++ = (mask & 0x0001)?(c0):(c1);			\
  *ip++ = (mask & 0x0002)?(c0):(c1);			\
  *ip++ = (mask & 0x0004)?(c2):(c3);			\
  *ip   = (mask & 0x0008)?(c2):(c3); ip += row_inc;	\
  *ip++ = (mask & 0x0010)?(c0):(c1);			\
  *ip++ = (mask & 0x0020)?(c0):(c1);			\
  *ip++ = (mask & 0x0040)?(c2):(c3);			\
  *ip   = (mask & 0x0080)?(c2):(c3); ip += row_inc;	\
  *ip++ = (mask & 0x0100)?(c4):(c5);			\
  *ip++ = (mask & 0x0200)?(c4):(c5);			\
  *ip++ = (mask & 0x0400)?(c6):(c7);			\
  *ip   = (mask & 0x0800)?(c6):(c7); ip += row_inc;	\
  *ip++ = (mask & 0x1000)?(c4):(c5);			\
  *ip++ = (mask & 0x2000)?(c4):(c5);			\
  *ip++ = (mask & 0x4000)?(c6):(c7);			\
  *ip   = (mask & 0x8000)?(c6):(c7);			}


#define MVC1_PIX_2_RGB(pix,r,g,b) { xaULONG ra,ga,ba;		\
  ra = (pix>>10) & 0x1f; ga = (pix>>5) & 0x1f; ba = pix & 0x1f;	\
  r = (ra << 3) | (ra >> 2); g = (ga << 3) | (ga >> 2);		\
  b = (ba << 3) | (ba >> 2);	}


#define MVC1_RGB2(dp,p0,r,g,b) \
{ MVC1_PIX_2_RGB(p0,r[0],g[0],b[0]);	r[2] = r[4] = r[6] = r[0];	\
  g[2] = g[4] = g[6] = g[0];	b[2] = b[4] = b[6] = b[0];		\
  p0 = *dp++ << 8; p0 |= *dp++;						\
  MVC1_PIX_2_RGB(p0,r[1],g[1],b[1]);	r[3] = r[5] = r[7] = r[1];	\
  g[3] = g[5] = g[7] = g[1];	b[3] = b[5] = b[7] = b[1];	}


#define MVC1_RGB8(dp,p0,r,g,b)	\
{ MVC1_PIX_2_RGB(p0,r[0],g[0],b[0]);	\
  p0 = *dp++ << 8; p0 |= *dp++;	MVC1_PIX_2_RGB(p0,r[1],g[1],b[1]);	\
  p0 = *dp++ << 8; p0 |= *dp++;	MVC1_PIX_2_RGB(p0,r[2],g[2],b[2]);	\
  p0 = *dp++ << 8; p0 |= *dp++;	MVC1_PIX_2_RGB(p0,r[3],g[3],b[3]);	\
  p0 = *dp++ << 8; p0 |= *dp++;	MVC1_PIX_2_RGB(p0,r[4],g[4],b[4]);	\
  p0 = *dp++ << 8; p0 |= *dp++;	MVC1_PIX_2_RGB(p0,r[5],g[5],b[5]);	\
  p0 = *dp++ << 8; p0 |= *dp++;	MVC1_PIX_2_RGB(p0,r[6],g[6],b[6]);	\
  p0 = *dp++ << 8; p0 |= *dp++;	MVC1_PIX_2_RGB(p0,r[7],g[7],b[7]);	}

#define MVC1_RGB_BLK8(ip,m,r,g,b) { register xaULONG ix;    \
  ix = (m & 0x0001)?(0):(1); *ip++ = r[ix]; *ip++ = g[ix]; *ip++ = b[ix]; \
  ix = (m & 0x0002)?(0):(1); *ip++ = r[ix]; *ip++ = g[ix]; *ip++ = b[ix]; \
  ix = (m & 0x0004)?(2):(3); *ip++ = r[ix]; *ip++ = g[ix]; *ip++ = b[ix]; \
  ix = (m & 0x0008)?(2):(3); *ip++ = r[ix]; *ip++ = g[ix]; *ip   = b[ix]; \
  ip += row_inc;							  \
  ix = (m & 0x0010)?(0):(1); *ip++ = r[ix]; *ip++ = g[ix]; *ip++ = b[ix]; \
  ix = (m & 0x0020)?(0):(1); *ip++ = r[ix]; *ip++ = g[ix]; *ip++ = b[ix]; \
  ix = (m & 0x0040)?(2):(3); *ip++ = r[ix]; *ip++ = g[ix]; *ip++ = b[ix]; \
  ix = (m & 0x0080)?(2):(3); *ip++ = r[ix]; *ip++ = g[ix]; *ip   = b[ix]; \
  ip += row_inc;							  \
  ix = (m & 0x0100)?(4):(5); *ip++ = r[ix]; *ip++ = g[ix]; *ip++ = b[ix]; \
  ix = (m & 0x0200)?(4):(5); *ip++ = r[ix]; *ip++ = g[ix]; *ip++ = b[ix]; \
  ix = (m & 0x0400)?(6):(7); *ip++ = r[ix]; *ip++ = g[ix]; *ip++ = b[ix]; \
  ix = (m & 0x0800)?(6):(7); *ip++ = r[ix]; *ip++ = g[ix]; *ip   = b[ix]; \
  ip += row_inc;							  \
  ix = (m & 0x1000)?(4):(5); *ip++ = r[ix]; *ip++ = g[ix]; *ip++ = b[ix]; \
  ix = (m & 0x2000)?(4):(5); *ip++ = r[ix]; *ip++ = g[ix]; *ip++ = b[ix]; \
  ix = (m & 0x4000)?(6):(7); *ip++ = r[ix]; *ip++ = g[ix]; *ip++ = b[ix]; \
  ix = (m & 0x8000)?(6):(7); *ip++ = r[ix]; *ip++ = g[ix]; *ip   = b[ix]; }

#define MVC1_PIX_2_DITH(pix,_r,_g,_b) { \
_r = (pix & 0x7c00); _g = (pix & 0x03e0); _b = (pix & 0x001f);       \
_r = _r | (_r >> 5);  _g = (_g << 5) | _g; _b = (_b << 10) | (_b << 5); }

#define MVC1_DITH2(dp,p0,r,g,b) \
{ MVC1_PIX_2_DITH(p0,r[0],g[0],b[0]);	r[2] = r[4] = r[6] = r[0];	\
  g[2] = g[4] = g[6] = g[0];	b[2] = b[4] = b[6] = b[0];		\
  p0 = *dp++ << 8; p0 |= *dp++;						\
  MVC1_PIX_2_DITH(p0,r[1],g[1],b[1]);	r[3] = r[5] = r[7] = r[1];	\
  g[3] = g[5] = g[7] = g[1];	b[3] = b[5] = b[7] = b[1];	}


#define MVC1_DITH8(dp,p0,r,g,b)	\
{ MVC1_PIX_2_DITH(p0,r[0],g[0],b[0]);	\
  p0 = *dp++ << 8; p0 |= *dp++;	MVC1_PIX_2_DITH(p0,r[1],g[1],b[1]);	\
  p0 = *dp++ << 8; p0 |= *dp++;	MVC1_PIX_2_DITH(p0,r[2],g[2],b[2]);	\
  p0 = *dp++ << 8; p0 |= *dp++;	MVC1_PIX_2_DITH(p0,r[3],g[3],b[3]);	\
  p0 = *dp++ << 8; p0 |= *dp++;	MVC1_PIX_2_DITH(p0,r[4],g[4],b[4]);	\
  p0 = *dp++ << 8; p0 |= *dp++;	MVC1_PIX_2_DITH(p0,r[5],g[5],b[5]);	\
  p0 = *dp++ << 8; p0 |= *dp++;	MVC1_PIX_2_DITH(p0,r[6],g[6],b[6]);	\
  p0 = *dp++ << 8; p0 |= *dp++;	MVC1_PIX_2_DITH(p0,r[7],g[7],b[7]);	}


#define MVC1_DITH_GET_RGB(r,g,b,re,ge,be,col) { xaLONG r1,g1,b1; \
  r1 = (xaLONG)rnglimit[(r + re) >> 7]; \
  g1 = (xaLONG)rnglimit[(g + ge) >> 7]; \
  b1 = (xaLONG)rnglimit[(b + be) >> 7]; \
  col = (r1 & 0xe0) | ((g1 & 0xe0) >> 3) | ((b1 & 0xc0) >> 6); }

#define MVC1_DITH_GET_ERR(r,g,b,re,ge,be,col,cmap) { \
  re =  ((xaLONG)(r) - (xaLONG)(cmap[col].red   >> 1)) >> 1; \
  ge =  ((xaLONG)(g) - (xaLONG)(cmap[col].green >> 1)) >> 1; \
  be =  ((xaLONG)(b) - (xaLONG)(cmap[col].blue  >> 1)) >> 1; }


xaULONG MOVI_Decode_MVC1(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG mflag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG special = dec_info->special;
  XA_CHDR *chdr = dec_info->chdr;
  xaULONG row_inc;
  xaLONG x = 0, y = 0;
  xaUBYTE *dptr = delta;
  xaULONG dither = 0;

  dec_info->xs = dec_info->ys = 0; dec_info->xe = imagex; dec_info->ye = imagey;
  if (dec_info->skip_flag > 0) return(ACT_DLTA_DROP);

  if (chdr) { if (chdr->new_chdr) chdr = chdr->new_chdr; }
	/* POD rethink this */
  if ((cmap_color_func == 4) && (chdr))
  { if (cmap_cache == 0) CMAP_Cache_Init(0);
    if (chdr != cmap_cache_chdr) { CMAP_Cache_Clear(); cmap_cache_chdr = chdr; }
  }

  if (   (!special) && (x11_bytes_pixel==1)
      && (chdr) && (x11_display_type == XA_PSEUDOCOLOR)
      && (cmap_color_func != 4)
      && (cmap_true_to_332 == xaTRUE) && (x11_cmap_size == 256)
      && (xa_dither_flag==xaTRUE)
     ) dither = 1;

  
  row_inc = (special)?((3*(imagex-4))+1):(imagex - 3);

  if (special)
  { while( (dsize > 0) && (y < imagey))
    { xaUBYTE *ip = (xaUBYTE *)(image + 3 * (y * imagex + x) );
      xaUBYTE r[8],g[8],b[8];	xaULONG mask,p0;
      MOVI_GET_16(mask,dptr); MOVI_GET_16(p0,dptr);


      if (p0 & 0x8000)	{ MVC1_RGB8(dptr,p0,r,g,b); dsize -= 18; }
      else		{ MVC1_RGB2(dptr,p0,r,g,b); dsize -=  6; }
      MVC1_RGB_BLK8(ip,mask,r,g,b);
      MOVI_BLOCK_INC(x,y,imagex);
    }
  }
  else if (dither)
  { xaUBYTE *rnglimit = xa_byte_limit;
    ColorReg *cmap = chdr->cmap;
    while( (dsize > 0) && (y < imagey))
    { xaUBYTE *ip0 = (xaUBYTE *)(image + (y * imagex + x) );
      xaULONG r[8],g[8],b[8];	xaULONG mask,p0;
      MOVI_GET_16(mask,dptr); MOVI_GET_16(p0,dptr);
      if (p0 & 0x8000)  { MVC1_DITH8(dptr,p0,r,g,b); dsize -= 12; }
      else		{ MVC1_DITH2(dptr,p0,r,g,b); dsize -=  6; }

      { xaUBYTE *ip1 = ip0, *ip2, *ip3;
	xaLONG ix, ra, ga, ba, col, re = 0, ge = 0, be = 0;
	ip1 += imagex; ip2 = ip1; ip2 += imagex; ip3 = ip2; ip3 += imagex;
	/** 0 0 */
	    ix = (mask & 0x0001)?(0):(1); ra = r[ix]; ga = g[ix]; ba = b[ix];
	    MVC1_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	    if (mflag) col = map[col];	ip0[0] = (xaUBYTE)col;
	    MVC1_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
	/*  0 1 */
	    ix = (mask & 0x0002)?(0):(1); ra = r[ix]; ga = g[ix]; ba = b[ix];
	    MVC1_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	    if (mflag) col = map[col];	ip0[1] = (xaUBYTE)col;
	    MVC1_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
	/*  1 1 */
	    ix = (mask & 0x0020)?(0):(1); ra = r[ix]; ga = g[ix]; ba = b[ix];
	    MVC1_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	    if (mflag) col = map[col];	ip1[1] = (xaUBYTE)col;
	    MVC1_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
	/*  1 0 */
	    ix = (mask & 0x0010)?(0):(1); ra = r[ix]; ga = g[ix]; ba = b[ix];
	    MVC1_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	    if (mflag) col = map[col];	ip1[0] = (xaUBYTE)col;
	    MVC1_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
	/** 2 0 */
	    ix = (mask & 0x0100)?(4):(5); ra = r[ix]; ga = g[ix]; ba = b[ix];
	    MVC1_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	    if (mflag) col = map[col];	ip2[0] = (xaUBYTE)col;
	    MVC1_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
	/*  3 0 */
	    ix = (mask & 0x1000)?(4):(5); ra = r[ix]; ga = g[ix]; ba = b[ix];
	    MVC1_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	    if (mflag) col = map[col];	ip3[0] = (xaUBYTE)col;
	    MVC1_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
	/*  3 1 */
	    ix = (mask & 0x2000)?(4):(5); ra = r[ix]; ga = g[ix]; ba = b[ix];
	    MVC1_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	    if (mflag) col = map[col];	ip3[1] = (xaUBYTE)col;
	    MVC1_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
	/*  2 1 */
	    ix = (mask & 0x0200)?(4):(5); ra = r[ix]; ga = g[ix]; ba = b[ix];
	    MVC1_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	    if (mflag) col = map[col];	ip2[1] = (xaUBYTE)col;
	    MVC1_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
	/** 2 2 */
	    ix = (mask & 0x0400)?(6):(7); ra = r[ix]; ga = g[ix]; ba = b[ix];
	    MVC1_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	    if (mflag) col = map[col];	ip2[2] = (xaUBYTE)col;
	    MVC1_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
	/*  3 2 */
	    ix = (mask & 0x4000)?(6):(7); ra = r[ix]; ga = g[ix]; ba = b[ix];
	    MVC1_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	    if (mflag) col = map[col];	ip3[2] = (xaUBYTE)col;
	    MVC1_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
	/*  3 3 */
	    ix = (mask & 0x8000)?(6):(7); ra = r[ix]; ga = g[ix]; ba = b[ix];
	    MVC1_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	    if (mflag) col = map[col];	ip3[3] = (xaUBYTE)col;
	    MVC1_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
	/*  2 3 */
	    ix = (mask & 0x0800)?(6):(7); ra = r[ix]; ga = g[ix]; ba = b[ix];
	    MVC1_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	    if (mflag) col = map[col];	ip2[3] = (xaUBYTE)col;
	    MVC1_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
	/** 1 3 */
	    ix = (mask & 0x0080)?(2):(3); ra = r[ix]; ga = g[ix]; ba = b[ix];
	    MVC1_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	    if (mflag) col = map[col];	ip1[3] = (xaUBYTE)col;
	    MVC1_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
	/*  1 2 */
	    ix = (mask & 0x0040)?(2):(3); ra = r[ix]; ga = g[ix]; ba = b[ix];
	    MVC1_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	    if (mflag) col = map[col];	ip1[2] = (xaUBYTE)col;
	    MVC1_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
	/*  0 2 */
	    ix = (mask & 0x0004)?(2):(3); ra = r[ix]; ga = g[ix]; ba = b[ix];
	    MVC1_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	    if (mflag) col = map[col];	ip0[2] = (xaUBYTE)col;
	    MVC1_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
	/*  0 3 */
	    ix = (mask & 0x0008)?(2):(3); ra = r[ix]; ga = g[ix]; ba = b[ix];
	    MVC1_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	    if (mflag) col = map[col];	ip0[3] = (xaUBYTE)col;
      }
      MOVI_BLOCK_INC(x,y,imagex);
    }
  }
  else if ((x11_bytes_pixel == 1) || (mflag == xaFALSE))
  { while( (dsize > 0) && (y < imagey))
    { xaUBYTE *ip = (xaUBYTE *)(image + (y * imagex + x) );
      xaUBYTE c0,c1,c2,c3,c4,c5,c6,c7;
      xaULONG mask,p0;
      MOVI_GET_16(mask,dptr); MOVI_GET_16(p0,dptr);	dsize -= 6;
      if (p0 & 0x8000)  { dsize -= 12; 
	  MVC1_COLOR8(dptr,p0,c0,c1,c2,c3,c4,c5,c6,c7,mflag,map,chdr,xaUBYTE); }
      else MVC1_COLOR2(dptr,p0,c0,c1,c2,c3,c4,c5,c6,c7,mflag,map,chdr,xaUBYTE)
      MVC1_BLK8(ip,mask,c0,c1,c2,c3,c4,c5,c6,c7);
      MOVI_BLOCK_INC(x,y,imagex);
    }
  } /* end depth 8 */
  else if (x11_bytes_pixel == 4)
  { 
    while( (dsize > 0) && (y < imagey))
    { xaULONG *ip = (xaULONG *)(image + ((y * imagex + x)<<2) );
      xaULONG c0,c1,c2,c3,c4,c5,c6,c7;
      xaULONG mask,p0;
      MOVI_GET_16(mask,dptr); MOVI_GET_16(p0,dptr);	dsize -= 6;
      if (p0 & 0x8000)  { dsize -= 12; 
	  MVC1_COLOR8(dptr,p0,c0,c1,c2,c3,c4,c5,c6,c7,mflag,map,chdr,xaULONG); }
      else MVC1_COLOR2(dptr,p0,c0,c1,c2,c3,c4,c5,c6,c7,mflag,map,chdr,xaULONG)

      MVC1_BLK8(ip,mask,c0,c1,c2,c3,c4,c5,c6,c7);
      MOVI_BLOCK_INC(x,y,imagex);
    }
  } /* end depth 24/32 */
  else /* if (x11_bytes_pixel == 2) */
  { while( (dsize > 0) && (y < imagey))
    { xaUSHORT *ip = (xaUSHORT *)(image + ((y * imagex + x)<<1) );
      xaUSHORT c0,c1,c2,c3,c4,c5,c6,c7;
      xaULONG mask,p0;
      MOVI_GET_16(mask,dptr); MOVI_GET_16(p0,dptr);	dsize -= 6;
      if (p0 & 0x8000)  { dsize -= 12; 
	  MVC1_COLOR8(dptr,p0,c0,c1,c2,c3,c4,c5,c6,c7,mflag,map,chdr,xaUSHORT); }
      else MVC1_COLOR2(dptr,p0,c0,c1,c2,c3,c4,c5,c6,c7,mflag,map,chdr,xaUSHORT)
      MVC1_BLK8(ip,mask,c0,c1,c2,c3,c4,c5,c6,c7);
      MOVI_BLOCK_INC(x,y,imagex);
    }
  } /* end depth 16 */

  if (mflag) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}

xaULONG MOVI_Decode_MVC2(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG mflag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG special = dec_info->special;
  XA_CHDR *chdr = dec_info->chdr;
  xaLONG row_inc;
  xaUBYTE *dptr = delta;
  xaULONG dither = 0;
  xaLONG x = 0, y = 0;

  xaUBYTE *mbm = 0;
  xaULONG mbmsize = 0;
  xaULONG xtiles, ytiles;
  xaULONG i, nmap;
  xaULONG lmap[256];


  dec_info->xs = dec_info->ys = 0; dec_info->xe = imagex; dec_info->ye = imagey;

/*
  if (dec_info->skip_flag > 0) 
*/

 
 MOVI_GET_16(x,dptr);
 MOVI_GET_16(y,dptr);
 dsize -= 4;
 xtiles = x/4;
 ytiles = y/4;


 if  ( *dptr++ != 0)
 {
   fprintf(stderr," bitmap is present \n");
   mbmsize = xtiles * ytiles;
   mbmsize = 1+((mbmsize-1)/8);
   mbm = dptr;
   dptr += mbmsize;
   dsize -= 1 + mbmsize;
 }
 else
 {
   mbm = 0;
 }
	/* Is There a Colormap, if so, read it in */
 nmap = *dptr++;	dsize--;
 for(i=0; i<nmap; i++)
 { xaULONG r, g, b;
   r = (xaULONG)(*dptr++);
   g = (xaULONG)(*dptr++);
   b = (xaULONG)(*dptr++);
   lmap[i] = (xaULONG)XA_RGB24_To_CLR32(r,g,b,mflag,map,chdr);
 }
 dsize -= (3 * nmap);


  if (chdr) { if (chdr->new_chdr) chdr = chdr->new_chdr; }
	/* POD rethink this */
  if ((cmap_color_func == 4) && (chdr))
  { if (cmap_cache == 0) CMAP_Cache_Init(0);
    if (chdr != cmap_cache_chdr) { CMAP_Cache_Clear(); cmap_cache_chdr = chdr; }
  }

  if (   (!special) && (x11_bytes_pixel==1)
      && (chdr) && (x11_display_type == XA_PSEUDOCOLOR)
      && (cmap_color_func != 4)
      && (cmap_true_to_332 == xaTRUE) && (x11_cmap_size == 256)
      && (xa_dither_flag==xaTRUE)
     ) dither = 1;

  
/*
  row_inc = (special)?((3*(imagex-4))+1):(imagex - 3);
*/
  row_inc = -(imagex + 3);

  x = 0;
  y = imagey - 1;

  if ((x11_bytes_pixel == 1) || (mflag == xaFALSE))
  { while( (dsize > 0) && (y >= 0))
    { xaUBYTE *ip = (xaUBYTE *)(image + (y * imagex + x) );
      xaUBYTE c0,c1,c2,c3,c4,c5,c6,c7;
      xaULONG mask,p0,p1;
      MOVI_GET_8(p0,dptr);
      if (p0 & 0x80)
      { xaUBYTE r,g,b;
        if (p0 & 0x40)    /* 6 bit Luma */
	{ p0 &= 0x3f;
	  r = ((p0 << 2) | (p0 >> 4));
	  c0 = (xaUBYTE)XA_RGB24_To_CLR32(r,r,r,mflag,map,chdr);
	  MVC1_BLK1(ip,c0);
	  MOVI_BLOCK_DEC(x,y,imagex);
	  dsize -= 1;
	}
	else	/* 6 bit Blue, 8 bit Green 8 bit Red */
	{ p0 &= 0x3f;
	  b = ((p0 << 2) | (p0 >> 4));
	  MOVI_GET_8(g,dptr);
	  MOVI_GET_8(r,dptr);
	  c0 = (xaUBYTE)XA_RGB24_To_CLR32(r,g,b,mflag,map,chdr);
	  MVC1_BLK1(ip,c0);
	  MOVI_BLOCK_DEC(x,y,imagex);
	  dsize -= 3;
	}
      }
      else
      {
        MOVI_GET_8(p1,dptr);
        if (p1 & 0x80)
        {
	  if ((p0 & 0x7f) == (p1 & 0x7f))
	  {
	    c0 = lmap[ p0 & 0x7f ];
	    MVC1_BLK1(ip,c0); 
	    MOVI_BLOCK_DEC(x,y,imagex);
	    dsize -= 2;
	  }
	  else
	  { /* NOTE: p0 -> odds and p1 -> evens - opposite MVC1 */
	    c0 = c2 = c4 = c6 = lmap[ p0 & 0x7f ];
	    c1 = c3 = c5 = c7 = lmap[ p1 & 0x7f ];
	    mask = *dptr++;  mask |= *dptr++ << 8;
	    MVC1_BLK8(ip,mask,c1,c0,c3,c2,c5,c4,c7,c6);
	    MOVI_BLOCK_DEC(x,y,imagex);
	    dsize -= 4;
	  }
        }
        else
        {
	  c0 = lmap[ p0 & 0x7f ];
	  c1 = lmap[ p1 & 0x7f ];
	  c2 = lmap[ (*dptr++  & 0x7f )];
	  c3 = lmap[ (*dptr++  & 0x7f )];
	  c4 = lmap[ (*dptr++  & 0x7f )];
	  c5 = lmap[ (*dptr++  & 0x7f )];
	  c6 = lmap[ (*dptr++  & 0x7f )];
	  c7 = lmap[ (*dptr++  & 0x7f )];
	  mask = *dptr++;  mask |= *dptr++ << 8;
	  MVC1_BLK8(ip,mask,c1,c0,c3,c2,c5,c4,c7,c6);
	  MOVI_BLOCK_DEC(x,y,imagex);
	  dsize -= 10;
        }
      } /* end of mode 3/4 */
    } /* while y/dsize */
  } /* end depth 16 */
  else if (x11_bytes_pixel == 2)
  { while( (dsize > 0) && (y >= 0))
    { xaUSHORT *ip = (xaUSHORT *)(image + ((y * imagex + x)<<1) );
      xaUSHORT c0,c1,c2,c3,c4,c5,c6,c7;
      xaULONG mask,p0,p1;
      MOVI_GET_8(p0,dptr);
      if (p0 & 0x80)
      { xaUBYTE r,g,b;
        if (p0 & 0x40)    /* 6 bit Luma */
	{ p0 &= 0x3f;
	  r = ((p0 << 2) | (p0 >> 4));
	  c0 = (xaUSHORT)XA_RGB24_To_CLR32(r,r,r,mflag,map,chdr);
	  MVC1_BLK1(ip,c0);
	  MOVI_BLOCK_DEC(x,y,imagex);
	  dsize -= 1;
	}
	else	/* 6 bit Blue, 8 bit Green 8 bit Red */
	{ p0 &= 0x3f;
	  b = ((p0 << 2) | (p0 >> 4));
	  MOVI_GET_8(g,dptr);
	  MOVI_GET_8(r,dptr);
	  c0 = (xaUSHORT)XA_RGB24_To_CLR32(r,g,b,mflag,map,chdr);
	  MVC1_BLK1(ip,c0);
	  MOVI_BLOCK_DEC(x,y,imagex);
	  dsize -= 3;
	}
      }
      else
      {
        MOVI_GET_8(p1,dptr);
        if (p1 & 0x80)
        {
	  if ((p0 & 0x7f) == (p1 & 0x7f))
	  {
	    c0 = lmap[ p0 & 0x7f ];
	    MVC1_BLK1(ip,c0); 
	    MOVI_BLOCK_DEC(x,y,imagex);
	    dsize -= 2;
	  }
	  else
	  { /* NOTE: p0 -> odds and p1 -> evens - opposite MVC1 */
	    c0 = c2 = c4 = c6 = lmap[ p0 & 0x7f ];
	    c1 = c3 = c5 = c7 = lmap[ p1 & 0x7f ];
	    mask = *dptr++;  mask |= *dptr++ << 8;
	    MVC1_BLK8(ip,mask,c1,c0,c3,c2,c5,c4,c7,c6);
	    MOVI_BLOCK_DEC(x,y,imagex);
	    dsize -= 4;
	  }
        }
        else
        {
	  c0 = lmap[ p0 & 0x7f ];
	  c1 = lmap[ p1 & 0x7f ];
	  c2 = lmap[ (*dptr++  & 0x7f )];
	  c3 = lmap[ (*dptr++  & 0x7f )];
	  c4 = lmap[ (*dptr++  & 0x7f )];
	  c5 = lmap[ (*dptr++  & 0x7f )];
	  c6 = lmap[ (*dptr++  & 0x7f )];
	  c7 = lmap[ (*dptr++  & 0x7f )];
	  mask = *dptr++;  mask |= *dptr++ << 8;
	  MVC1_BLK8(ip,mask,c1,c0,c3,c2,c5,c4,c7,c6);
	  MOVI_BLOCK_DEC(x,y,imagex);
	  dsize -= 10;
        }
      } /* end of mode 3/4 */
    } /* while y/dsize */
  } /* end depth 16 */
  else if (x11_bytes_pixel == 4)
  { while( (dsize > 0) && (y >= 0))
    { xaULONG *ip = (xaULONG *)(image + ((y * imagex + x)<<2) );
      xaULONG c0,c1,c2,c3,c4,c5,c6,c7;
      xaULONG mask,p0,p1;
      MOVI_GET_8(p0,dptr);
      if (p0 & 0x80)
      { xaUBYTE r,g,b;
        if (p0 & 0x40)    /* 6 bit Luma */
	{ p0 &= 0x3f;
	  r = ((p0 << 2) | (p0 >> 4));
	  c0 = (xaULONG)XA_RGB24_To_CLR32(r,r,r,mflag,map,chdr);
	  MVC1_BLK1(ip,c0);
	  MOVI_BLOCK_DEC(x,y,imagex);
	  dsize -= 1;
	}
	else	/* 6 bit Blue, 8 bit Green 8 bit Red */
	{ p0 &= 0x3f;
	  b = ((p0 << 2) | (p0 >> 4));
	  MOVI_GET_8(g,dptr);
	  MOVI_GET_8(r,dptr);
	  c0 = (xaULONG)XA_RGB24_To_CLR32(r,g,b,mflag,map,chdr);
	  MVC1_BLK1(ip,c0);
	  MOVI_BLOCK_DEC(x,y,imagex);
	  dsize -= 3;
	}
      }
      else
      {
        MOVI_GET_8(p1,dptr);
        if (p1 & 0x80)
        {
	  if ((p0 & 0x7f) == (p1 & 0x7f))
	  {
	    c0 = lmap[ p0 & 0x7f ];
	    MVC1_BLK1(ip,c0); 
	    MOVI_BLOCK_DEC(x,y,imagex);
	    dsize -= 2;
	  }
	  else
	  { /* NOTE: p0 -> odds and p1 -> evens - opposite MVC1 */
	    c0 = c2 = c4 = c6 = lmap[ p0 & 0x7f ];
	    c1 = c3 = c5 = c7 = lmap[ p1 & 0x7f ];
	    mask = *dptr++;  mask |= *dptr++ << 8;
	    MVC1_BLK8(ip,mask,c1,c0,c3,c2,c5,c4,c7,c6);
	    MOVI_BLOCK_DEC(x,y,imagex);
	    dsize -= 4;
	  }
        }
        else
        {
	  c0 = lmap[ p0 & 0x7f ];
	  c1 = lmap[ p1 & 0x7f ];
	  c2 = lmap[ (*dptr++  & 0x7f )];
	  c3 = lmap[ (*dptr++  & 0x7f )];
	  c4 = lmap[ (*dptr++  & 0x7f )];
	  c5 = lmap[ (*dptr++  & 0x7f )];
	  c6 = lmap[ (*dptr++  & 0x7f )];
	  c7 = lmap[ (*dptr++  & 0x7f )];
	  mask = *dptr++;  mask |= *dptr++ << 8;
	  MVC1_BLK8(ip,mask,c1,c0,c3,c2,c5,c4,c7,c6);
	  MOVI_BLOCK_DEC(x,y,imagex);
	  dsize -= 10;
        }
      } /* end of mode 3/4 */
    } /* while y/dsize */
  } /* end depth 32 */
  else
  {
    return(ACT_DLTA_DROP);
  }

  if (mflag) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}

xaULONG MOVI_Get_Color(color,map_flag,map,chdr)
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

 
  if (x11_display_type & XA_X11_TRUE) clr = X11_Get_True_Color(tr,tg,tb,8);
  else
  { 
    if ((cmap_color_func == 4) && (chdr))
    { register xaULONG cache_i = color & 0x7fff;

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

xaULONG MOVI_Decode_RGB32(image,delta,tdsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG tdsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG mflag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG special = dec_info->special;
  XA_CHDR *chdr = dec_info->chdr;
  xaULONG row_inc;
  xaLONG x = 0, y = 0;
  xaUBYTE *dp = delta;
  xaLONG dsize = tdsize;

  dec_info->xs = dec_info->ys = 0;
  dec_info->xe = imagex;
  dec_info->ye = imagey;
	/* Indicate we can drop these frames */
  if (dec_info->skip_flag > 0) return(ACT_DLTA_DROP);

  if (chdr) { if (chdr->new_chdr) chdr = chdr->new_chdr; }

  row_inc = (special)?(3*imagex):imagex;

  if (special)
  { while( (dsize > 0) && (y < imagey))
    { xaUBYTE *ip = (xaUBYTE *)(image + (3 * y * imagex)); y++;
      x = imagex;
      while(x--) {xaUBYTE r,g,b; dp++; r = *dp++; g = *dp++; b = *dp++;
	*ip++ = r; *ip++ = g; *ip++ = b; }
      dsize -= (imagex << 2);
    }
  }
  else if ((x11_bytes_pixel == 1) || (mflag == xaFALSE))
  { while( (dsize > 0) && (y < imagey))
    { xaUBYTE *ip = (xaUBYTE *)(image + y * imagex);	y++;
      x = imagex;
      while(x--)
      { xaULONG r,g,b; 
	dp++; r = (xaULONG)*dp++; g = (xaULONG)*dp++; b = (xaULONG)*dp++;
	*ip++ = (xaUBYTE)XA_RGB24_To_CLR32(r,g,b,mflag,map,chdr);
      }
      dsize -= (imagex << 2);
    }
  } 
  else if (x11_bytes_pixel == 4)
  { 
    while( (dsize > 0) && (y < imagey))
    { xaULONG *ip = (xaULONG *)(image + ((y * imagex + x)<<2) );  y++;
      x = imagex;
      while(x--)
      { xaULONG r,g,b; 
	dp++; r = (xaULONG)*dp++; g = (xaULONG)*dp++; b = (xaULONG)*dp++;
	*ip++ = (xaULONG)XA_RGB24_To_CLR32(r,g,b,mflag,map,chdr);
      }
      dsize -= (imagex << 2);
    }
  }
  else /* if (x11_bytes_pixel == 2) */
  { while( (dsize > 0) && (y < imagey))
    { xaUSHORT *ip = (xaUSHORT *)(image + ((y * imagex + x)<<1) );  y++;
      x = imagex;
      while(x--)
      { xaULONG r,g,b; 
	dp++; r = (xaULONG)*dp++; g = (xaULONG)*dp++; b = (xaULONG)*dp++;
	*ip++ = (xaUSHORT)XA_RGB24_To_CLR32(r,g,b,mflag,map,chdr);
      }
      dsize -= (imagex << 2);
    }
  } 
  if (mflag) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}

