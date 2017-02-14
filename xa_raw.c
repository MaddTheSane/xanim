
/*
 * xa_qcif.c
 *
 * Copyright (C) 1998,1999 by Mark Podlipec. 
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
 * Revision History
 *
 ********************************/


#include "xa_raw.h"
#include "xa_codecs.h"
#include "xa_color.h"
#include "xa_cmap.h"


/****---------- Global Routines -----------------------****/
xaULONG QCIF_Read_File();
xaULONG	CIF_Read_File();

/****---------- Local Routines --------------------****/
static xaULONG		RAW_Read_File();
static RAW_FRAME	*RAW_Add_Frame();
static void		RAW_Free_Frame_List();
static ACT_DLTA_HDR	*RAW_Read_Frame();
static xaULONG		RAW_Setup_Codec();

/****---------- Decode Routines -----------------****/
extern xaULONG RAW_Decode_YUV411();  /* QCIF, CIF */

/****---------- External Routines -----------------****/
extern XA_ANIM_SETUP *XA_Get_Anim_Setup();
extern void XA_Free_Anim_Setup();
extern XA_ACTION *ACT_Get_Action();
extern void ACT_Add_CHDR_To_Action();
extern void ACT_Setup_Mapped();
extern void ACT_Setup_Delta();
extern void ACT_Get_CCMAP();
extern XA_CHDR *CMAP_Create_CHDR_From_True();
extern xaULONG CMAP_Find_Closest();
extern xaUBYTE *UTIL_RGB_To_FS_Map();
extern xaUBYTE *UTIL_RGB_To_Map();
extern void JPG_Alloc_MCU_Bufs();
extern void JPG_Setup_Samp_Limit_Table();
extern void XA_Gen_YUV_Tabs();
extern void *XA_YUV221111_Func();

/****----------- Local Variables ------------------****/
static xaULONG raw_frame_cnt;
static RAW_FRAME *raw_frame_start,*raw_frame_cur;

/****---------- External Variables ----------------****/
extern xaLONG xa_dither_flag;
extern xaUBYTE  *xa_byte_limit;
extern YUVBufs jpg_YUVBufs;
extern YUVTabs def_yuv_tabs;



/****------------------------------------------------------------------****
 ****
 ****------------------------------------------------------------------****/
static RAW_FRAME *RAW_Add_Frame(time,timelo,act)
xaULONG time,timelo;
XA_ACTION *act;
{
  RAW_FRAME *fframe;
 
  fframe = (RAW_FRAME *) malloc(sizeof(RAW_FRAME));
  if (fframe == 0) TheEnd1("RAW_Add_Frame: malloc err");
 
  fframe->time   = time;
  fframe->timelo = timelo;
  fframe->act = act;
  fframe->next = 0;
 
  if (raw_frame_start == 0) raw_frame_start = fframe;
  else raw_frame_cur->next = fframe;
 
  raw_frame_cur = fframe;
  raw_frame_cnt++;
  return(fframe);
}

/****------------------------------------------------------------------****
 ****
 ****------------------------------------------------------------------****/
static void RAW_Free_Frame_List(fframes)
RAW_FRAME *fframes;
{
  RAW_FRAME *ftmp;
  while(fframes != 0)
  {
    ftmp = fframes;
    fframes = fframes->next;
    FREE(ftmp,0xA000);
  }
}

/****------------------------------------------------------------------****
 ****
 ****------------------------------------------------------------------****/
xaULONG QCIF_Read_File(fname,anim_hdr,audio_attempt)
char *fname;
XA_ANIM_HDR *anim_hdr;
xaULONG audio_attempt;	/* xaTRUE if audio is to be attempted */
{ xaULONG ret, delta_size;
  XA_CODEC_HDR	codec;

  codec.compression	= RAW_QCIF;
  codec.anim_hdr	= (void *)anim_hdr;
  codec.avi_ctab_flag	= xaFALSE;
  codec.depth		= 24;

  RAW_Setup_Codec(&codec);


	/* Setup Size of "compressed" Image */
  delta_size = codec.x * codec.y;
  delta_size = delta_size + (delta_size >> 1);

  ret = RAW_Read_File(fname,anim_hdr,&codec,delta_size);
  return(ret);
}

/****------------------------------------------------------------------****
 ****
 ****------------------------------------------------------------------****/
xaULONG CIF_Read_File(fname,anim_hdr,audio_attempt)
char *fname;
XA_ANIM_HDR *anim_hdr;
xaULONG audio_attempt;	/* xaTRUE if audio is to be attempted */
{ xaULONG ret, delta_size;
  XA_CODEC_HDR	codec;

  codec.compression	= RAW_CIF;
  codec.anim_hdr	= (void *)anim_hdr;
  codec.avi_ctab_flag	= xaFALSE;
  codec.depth		= 24;

  RAW_Setup_Codec(&codec);


	/* Setup Size of "compressed" Image */
  delta_size = codec.x * codec.y;
  delta_size = delta_size + (delta_size >> 1);

  ret = RAW_Read_File(fname,anim_hdr,&codec,delta_size);
  return(ret);
}


/****------------------------------------------------------------------****
 ****
 ****------------------------------------------------------------------****/
static xaULONG RAW_Read_File(fname,anim_hdr,codec,delta_size)
char		*fname;
XA_ANIM_HDR	*anim_hdr;
XA_CODEC_HDR	*codec;
xaULONG		delta_size;	/* xaTRUE if audio is to be attempted */
{ XA_INPUT *xin;
  xaLONG i,t_time;
  xaULONG t_timelo;
  XA_ACTION *act;
  XA_ANIM_SETUP *raw;
  xaULONG offset;
 
  xin = anim_hdr->xin;
  raw = XA_Get_Anim_Setup();
  raw->vid_time = XA_GET_TIME( 33 ); /* default */

  raw_frame_cnt		= 0;
  raw_frame_start	= 0;
  raw_frame_cur		= 0;

  raw->imagex = codec->x;
  raw->imagey = codec->y;
  raw->imagec = 256;
  raw->depth  = codec->depth;
  raw->compression = codec->compression;
  

  raw->max_fvid_size = delta_size;


	/*** Setup Colormaps ***/
  if (cmap_true_to_332 == xaTRUE)
	raw->chdr = CMAP_Create_332(raw->cmap,&raw->imagec);
  else	raw->chdr = CMAP_Create_Gray(raw->cmap,&raw->imagec);

	/*** If buffering(ie decompressing) Setup image */
  if ( (raw->pic==0) && (xa_buffer_flag == xaTRUE))
  {
    raw->pic_size = raw->imagex * raw->imagey;
    if ( (cmap_true_map_flag == xaTRUE) && (raw->depth > 8) )
                raw->pic = (xaUBYTE *) malloc( 3 * raw->pic_size );
    else raw->pic = (xaUBYTE *) malloc( XA_PIC_SIZE(raw->pic_size) );
    if (raw->pic == 0) TheEnd1("RAW_Buffer_Action: malloc failed");
  }


	/*** Loop Through File and read in frames ***/
  offset = 0;
  while( !xin->At_EOF(xin,-1) )
  { ACT_DLTA_HDR *dlta_hdr;

    act = ACT_Get_Action(anim_hdr,ACT_DELTA);

/*
    dlta_hdr = RAW_Read_Frame(xin, raw, delta_size, act, offset, codec);
*/
    dlta_hdr = RAW_Read_Frame(xin, raw, delta_size, act, -1, codec);

    if (dlta_hdr) ACT_Setup_Delta(raw,act,dlta_hdr,xin);

    offset += delta_size;
  }
  xin->Close_File(xin);


  if (xa_verbose) 
  { 
    fprintf(stdout, "  Frame Stats: Size=%dx%d  Frames=%d\n",
		raw->imagex,raw->imagey,raw_frame_cnt);
  }

	/* Free image buffers if used */
  if (raw->pic) { free(raw->pic); raw->pic = 0; }

  if (raw_frame_cnt == 0)
  {
    anim_hdr->total_time = 0;
    XA_Free_Anim_Setup(raw);
    return(xaFALSE);
  }
  else
  {
    anim_hdr->frame_lst = (XA_FRAME *)
				malloc( sizeof(XA_FRAME) * (raw_frame_cnt+1));
    if (anim_hdr->frame_lst == NULL) TheEnd1("RAW_Read_File: frame malloc err");

    raw_frame_cur = raw_frame_start;
    i = 0;
    t_time = 0;
    t_timelo = 0;
    while(raw_frame_cur != 0)
    { if (i > raw_frame_cnt)
      { fprintf(stdout,"RAW_Read_Anim: frame inconsistency %d %d\n",
						i,raw_frame_cnt); break; }
      anim_hdr->frame_lst[i].time_dur = raw_frame_cur->time;
      anim_hdr->frame_lst[i].zztime = t_time;
      t_time += raw_frame_cur->time;
      t_timelo += raw_frame_cur->timelo;
      while(t_timelo > (1<<24)) {t_time++; t_timelo -= (1<<24);}
      anim_hdr->frame_lst[i].act = raw_frame_cur->act;
      raw_frame_cur = raw_frame_cur->next;
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
    RAW_Free_Frame_List(raw_frame_start);
    anim_hdr->imagex = raw->imagex;
    anim_hdr->imagey = raw->imagey;
    anim_hdr->imagec = raw->imagec;
    anim_hdr->imaged = 8; /* nop */
    anim_hdr->frame_lst[i].time_dur = 0;
    anim_hdr->frame_lst[i].zztime = -1;
    anim_hdr->frame_lst[i].act  = 0;
    anim_hdr->loop_frame = 0;
  } /* end of video present */

  if (xa_buffer_flag == xaFALSE) anim_hdr->anim_flags |= ANIM_SNG_BUF;
  if (xa_file_flag == xaTRUE) anim_hdr->anim_flags |= ANIM_USE_FILE;
  anim_hdr->max_fvid_size = raw->max_fvid_size;
  anim_hdr->max_faud_size = 0;
  anim_hdr->fname = anim_hdr->name;

  XA_Free_Anim_Setup(raw);
  return(xaTRUE);
} /* end of read file */


/****------------------------------------------------------------------****
 ****
 ****------------------------------------------------------------------****/
static ACT_DLTA_HDR *RAW_Read_Frame(xin,raw,ck_size,act,ck_offset,codec)
XA_INPUT	*xin;
XA_ANIM_SETUP	*raw;
xaLONG		ck_size;
XA_ACTION	*act;
xaLONG		ck_offset;
XA_CODEC_HDR	*codec;
{
  ACT_DLTA_HDR *dlta_hdr = 0;
  xaULONG d;

  if (xa_file_flag==xaTRUE)
  {
    dlta_hdr = (ACT_DLTA_HDR *) malloc(sizeof(ACT_DLTA_HDR));
    if (dlta_hdr == 0) TheEnd1("RAW vid dlta0: malloc failed");
    act->data = (xaUBYTE *)dlta_hdr;
    dlta_hdr->flags = ACT_SNGL_BUF;
    dlta_hdr->fsize = ck_size;
    if (ck_offset >= 0)         
    { dlta_hdr->fpos  = ck_offset;
      xin->Seek_FPos(xin,(ck_offset + ck_size - 1),0);
      (void)xin->Read_U8(xin);  /* do this to trigger EOF */
    }
    else   /* fin marks chunk offset and need to seek past */
    { dlta_hdr->fpos  = xin->Get_FPos(xin);
      xin->Seek_FPos(xin,(ck_size-1),1); /* move past this chunk */
      (void)xin->Read_U8(xin);  /* do this to trigger EOF */
    }
    if (ck_size > raw->max_fvid_size) raw->max_fvid_size = ck_size;
  }
  else
  { xaLONG ret;
    d = ck_size + (sizeof(ACT_DLTA_HDR));
    dlta_hdr = (ACT_DLTA_HDR *) malloc( d );
    if (dlta_hdr == 0) TheEnd1("RAW vid dlta1: malloc failed");
    act->data = (xaUBYTE *)dlta_hdr;
    dlta_hdr->flags = ACT_SNGL_BUF | DLTA_DATA;
    dlta_hdr->fpos = 0; dlta_hdr->fsize = ck_size;
    if (ck_offset >= 0)         xin->Seek_FPos( xin, ck_offset, 0);
    ret = xin->Read_Block(xin,dlta_hdr->data,ck_size);
    if (ret != ck_size)
    {  fprintf(stdout,"RAW vid dlta: read failed\n");
       act->type = ACT_NOP;     act->data = 0;  act->chdr = 0;
       free(dlta_hdr);
       return(0);
    }
  }

  RAW_Add_Frame( raw->vid_time, raw->vid_timelo, act);
  dlta_hdr->xpos	= dlta_hdr->ypos = 0;
  dlta_hdr->xsize	= raw->imagex;
  dlta_hdr->ysize	= raw->imagey;
  dlta_hdr->special	= 0;
  dlta_hdr->xapi_rev	= codec->xapi_rev;
  dlta_hdr->delta	= codec->decoder;
  dlta_hdr->extra	= codec->extra;
  return(dlta_hdr);
}

/****------------------------------------------------------------------****
 ****
 ****------------------------------------------------------------------****/
static xaULONG RAW_Setup_Codec(codec)
XA_CODEC_HDR *codec;
{ xaLONG ret = CODEC_UNKNOWN;   /* default */
  codec->extra = 0;
  codec->xapi_rev = 0x0001;
  codec->decoder = 0;
  codec->description = 0;
  codec->avi_read_ext = 0;

  switch(codec->compression)
  {
    case RAW_QCIF: 
	codec->compression	= RAW_QCIF;
	codec->description	= "QCIF YUV411";
	codec->xapi_rev = 0x0002;
	codec->x = 176;
	codec->y = 144;
	ret = CODEC_SUPPORTED;
	codec->decoder = RAW_Decode_YUV411;
	JPG_Alloc_MCU_Bufs(codec->anim_hdr,codec->x,codec->y,xaTRUE);
	JPG_Setup_Samp_Limit_Table(codec->anim_hdr);
	XA_Gen_YUV_Tabs(codec->anim_hdr);
	break;

    case RAW_CIF: 
	codec->compression	= RAW_CIF;
	codec->description	= "CIF YUV411";
	codec->xapi_rev = 0x0002;
	codec->x = 352;
	codec->y = 288;
	ret = CODEC_SUPPORTED;
	codec->decoder = RAW_Decode_YUV411;
	JPG_Alloc_MCU_Bufs(codec->anim_hdr,codec->x,codec->y,xaTRUE);
	JPG_Setup_Samp_Limit_Table(codec->anim_hdr);
	XA_Gen_YUV_Tabs(codec->anim_hdr);
	break;
  }
 
  return(ret);
}

/****------------------------------------------------------------------****
 ****
 ****------------------------------------------------------------------****/

xaULONG RAW_Decode_YUV411(image,delta,dsize,dec_info)
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
  plane_sz = imagex * imagey;
  yp = jpg_YUVBufs.Ybuf;
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

