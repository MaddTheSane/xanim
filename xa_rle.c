
/*
 * xa_rle.c
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
#include "xa_rle.h"

xaLONG Is_RLE_File();
xaULONG RLE_Read_File();
xaULONG RLE_Read_Header();
xaULONG RLE_Get_Image();
void RLE_Read_Row();

XA_ACTION *ACT_Get_Action();
XA_CHDR   *ACT_Get_CMAP();
XA_CHDR *CMAP_Create_332();
XA_CHDR *CMAP_Create_Gray();
XA_CHDR *CMAP_Create_CHDR_From_True();
xaUBYTE *UTIL_RGB_To_Map();
xaUBYTE *UTIL_RGB_To_FS_Map();
xaULONG UTIL_Get_LSB_Short();
void ACT_Setup_Mapped();
void ACT_Add_CHDR_To_Action();

xaULONG rle_imagex, rle_imagey, rle_imagec;
xaULONG rle_max_imagex, rle_max_imagey;
xaULONG rle_xpos,rle_ypos;
xaULONG rle_image_size;
ColorReg rle_cmap[256];
xaUBYTE bg_color[3];
xaUBYTE rle_chan_map[3][256];
XA_CHDR *rle_chdr;
xaUBYTE *rle_pic;

static RLE_FRAME *rle_frame_start,*rle_frame_cur;
static xaULONG rle_frame_cnt;
static RLE_HDR rle_hdr;
static xaLONG priv_vert_skip;
static xaLONG priv_scan_y;
static xaLONG priv_is_eof;
static xaUBYTE *rle_row_buf,*rle_rows[3];

/*
 *
 */
xaLONG Is_RLE_File(filename)
char *filename;
{
  FILE *fin;
  xaULONG data;

  if ( (fin=fopen(filename,XA_OPEN_MODE)) == 0) return(xaNOFILE);
  data = UTIL_Get_LSB_Short(fin);  /* read past size */
  fclose(fin);
  if (data == RLE_MAGIC) return(xaTRUE);
  return(xaFALSE);
}


RLE_FRAME *RLE_Add_Frame(time,act)
xaULONG time;
XA_ACTION *act;
{
  RLE_FRAME *rframe;

  rframe = (RLE_FRAME *) malloc(sizeof(RLE_FRAME));
  if (rframe == 0) TheEnd1("RLE_Add_Frame: malloc err");

  rframe->time = time;
  rframe->act = act;
  rframe->next = 0;

  if (rle_frame_start == 0) rle_frame_start = rframe;
  else rle_frame_cur->next = rframe;

  rle_frame_cur = rframe;
  rle_frame_cnt++;
  return(rframe);
}

void
RLE_Free_Frame_List(rframes)
RLE_FRAME *rframes;
{
  RLE_FRAME *rtmp;
  while(rframes != 0)
  {
    rtmp = rframes;
    rframes = rframes->next;
    FREE(rtmp,0x8000);
  }
}


xaULONG RLE_Read_File(fname,anim_hdr)
char *fname;
XA_ANIM_HDR *anim_hdr;
{
  int i;
  RLE_FRAME *rtmp;
  xaLONG t_time;


  rle_frame_start = 0;
  rle_frame_cur = 0;
  rle_frame_cnt = 0;
  rle_imagex = rle_imagey = 0;
  rle_max_imagex = rle_max_imagey = 0;
  rle_imagec = 0;

  rle_chdr = 0;

  if (RLE_Get_Image(fname,anim_hdr)==xaFALSE) return(xaFALSE);

    anim_hdr->frame_lst = (XA_FRAME *)
		malloc(sizeof(XA_FRAME) * (rle_frame_cnt + 1));
  if (anim_hdr->frame_lst == NULL)
       TheEnd1("RLE_Read_Anim: malloc err");

  rtmp = rle_frame_start;
  i = 0;
  t_time = 0;
  while(rtmp != 0)
  {
    if (i >= rle_frame_cnt)
    {
      fprintf(stderr,"RLE_Read_Anim: frame inconsistency %ld %ld\n",
                i,rle_frame_cnt);
      break;
    }
    anim_hdr->frame_lst[i].time_dur = rtmp->time;
    anim_hdr->frame_lst[i].zztime = t_time;
    t_time += rtmp->time;
    anim_hdr->frame_lst[i].act = rtmp->act;
    rtmp = rtmp->next;
    i++;
  }
  anim_hdr->frame_lst[i].time_dur = 0;
  anim_hdr->frame_lst[i].zztime = -1;
  anim_hdr->frame_lst[i].act  = 0;
  anim_hdr->loop_frame = 0;
  if (i > 0) anim_hdr->last_frame = i - 1;
  else i = 0;
  anim_hdr->imagex = rle_max_imagex;
  anim_hdr->imagey = rle_max_imagey;
  anim_hdr->imagec = rle_imagec;
  anim_hdr->imaged = 8;

  RLE_Free_Frame_List(rle_frame_start);
  return(xaTRUE);
} 

xaULONG 
RLE_Read_Header(fp,rle_hdr)
FILE *fp;
RLE_HDR *rle_hdr;
{
  rle_hdr->magic  = UTIL_Get_LSB_Short(fp);
  if (rle_hdr->magic != RLE_MAGIC) return(xaFALSE);

  rle_hdr->xpos  = UTIL_Get_LSB_Short(fp);
  rle_hdr->ypos  = UTIL_Get_LSB_Short(fp);
  rle_hdr->xsize = UTIL_Get_LSB_Short(fp);
  rle_hdr->ysize = UTIL_Get_LSB_Short(fp);

  rle_hdr->flags     = (xaULONG)getc(fp) & 0xff;
  rle_hdr->chan_num  = (xaULONG)getc(fp) & 0xff;
  rle_hdr->pbits     = (xaULONG)getc(fp) & 0xff;
  rle_hdr->cmap_num  = (xaULONG)getc(fp) & 0xff;
  rle_hdr->cbits     = getc(fp) & 0xff;
  if (rle_hdr->cbits > 8) TheEnd1("RLE_Read_Header: csize > 256\n");
  if (rle_hdr->cbits == 0) rle_hdr->cbits = rle_hdr->pbits; /*PODGUESS*/
  rle_hdr->csize     =  0x01 << rle_hdr->cbits;

  DEBUG_LEVEL1
  {
    fprintf(stderr,"RLE_Read_Header: pos %ld %ld size %ld %ld csize %ld\n",
		rle_hdr->xpos,rle_hdr->ypos,rle_hdr->xsize,rle_hdr->ysize,
		rle_hdr->csize);
    fprintf(stderr,"                 flags %lx chans %ld pbits %ld maps %ld\n",
	rle_hdr->flags,rle_hdr->chan_num,rle_hdr->pbits,rle_hdr->cmap_num);
  }

  /* read background colors - only save 1st three */
  if ( !(rle_hdr->flags & RLEH_NO_BACKGROUND) )
  {
    register xaULONG i,j;
    j = 1 + (rle_hdr->chan_num / 2) * 2;
    for(i=0; i < j; i++)
    { 
      if (i < 3) bg_color[i] = getc(fp) & 0xff;
      else (void)getc(fp);
    }
  }
  else 
  {
    bg_color[0] = bg_color[1] = bg_color[2] = 0; 
    (void)getc(fp);
  }

  /* read all of the color maps - only store 1st three */
  if (rle_hdr->cmap_num)
  {
    register xaULONG i,j,d;
    for(i=0; i < rle_hdr->cmap_num; i++)
    {
      for(j=0; j < rle_hdr->csize; j++)
      {
        d = UTIL_Get_LSB_Short(fp);
        if (i < 3) rle_chan_map[i][j] = d >> 8;
      }
    }
    if (rle_hdr->cmap_num == 1) /* singlemap? copy to others */
    {
      for(j=0; j < rle_hdr->csize; j++)
        rle_chan_map[1][j] = rle_chan_map[2][j] = rle_chan_map[0][j];
    } 
  }
  else  /* no maps? then create linear maps */
  { register xaULONG i;
    for(i=0;i<rle_hdr->csize;i++) 
	rle_chan_map[0][i] = rle_chan_map[1][i] = rle_chan_map[2][i] = i;
  }
  /* GAMMA adjust map */
/* PODPOD NOTE  may need to add OR (x11_display_type & XA_X11_TRUE) here*/
  if (   (rle_hdr->chan_num == 3) && (cmap_true_map_flag == xaTRUE)
      && (xa_gamma_flag==xaTRUE) )  
  { register xaULONG i,j;
    for(j=0;j<3;j++)
      for(i=0;i<rle_hdr->csize;i++)
	rle_chan_map[j][i] = xa_gamma_adj[ rle_chan_map[j][i] ] >> 8;
  }

  /* skip comments */
  if (rle_hdr->flags & RLEH_COMMENT)  /* see rle_retrow.c for more details */
  {
    register xaULONG d,i,len;
    len = UTIL_Get_LSB_Short(fp);
    for(i=0; i<len; i++)
    {
      d = getc(fp) & 0xff;
	/* if (xa_verbose) fprintf(stderr,"%c",d); */
    }
    if (len & 0x01) getc(fp); /* pad */
  }

  /*  if not end of file return ok */
  if ( !feof(fp) )
  {
    rle_imagec = rle_hdr->csize;
    rle_imagex = rle_hdr->xsize;
    rle_imagey = rle_hdr->ysize;
    if (rle_imagex > rle_max_imagex) rle_max_imagex = rle_imagex;
    if (rle_imagey > rle_max_imagey) rle_max_imagey = rle_imagey;
    rle_xpos   = rle_hdr->xpos;
    rle_ypos   = rle_max_imagey - (rle_hdr->ypos + rle_hdr->ysize);
    rle_image_size = rle_imagex * rle_imagey;
     /* POD need to check for initial image having non zero pos and need to
      * check for subsequent images fitting in initial images
      * maybe keep initial positions around to shift subsequent.
      */
    priv_scan_y = rle_hdr->ypos;
    priv_vert_skip = 0;
    priv_is_eof = xaFALSE;

    return(xaTRUE);
  } else return(False);
}


xaULONG RLE_Get_Image(filename,anim_hdr)
char *filename;
XA_ANIM_HDR *anim_hdr;
{
  XA_ACTION *act;
  FILE *fin;
  int x,y;
  int rle_cnt;
  xaUBYTE *im_ptr;

  if ( (fin=fopen(filename,XA_OPEN_MODE)) == 0)
  {
    fprintf(stderr,"can't open RLE File %s for reading\n",filename);
    return(xaFALSE);
  }

  rle_cnt = 0;
  while ( RLE_Read_Header(fin,&rle_hdr) == xaTRUE )
  {
    rle_cnt++;

    if ( (rle_hdr.chan_num != 1) && (rle_hdr.chan_num != 3) )
    		TheEnd1("RLE_Get_Image: only 1 and 3 channels supported");
    /* currently must do this here to init xa_r_mask, etc TBD*/
    if (  (rle_hdr.chan_num == 3) && (cmap_true_map_flag != xaTRUE)
	&& (rle_chdr == 0) )
    {
	if (cmap_true_to_332 == xaTRUE)
		rle_chdr = CMAP_Create_332(rle_cmap,&rle_imagec);
	else	rle_chdr = CMAP_Create_Gray(rle_cmap,&rle_imagec);
    }

    if (xa_verbose) fprintf(stderr,"%ld) pos %ld %ld  size %ld %ld chans %ld\n",
	  rle_cnt,rle_xpos,rle_ypos,rle_imagex,rle_imagey,rle_hdr.chan_num);

    /* Allocate memory for the input image. */
    if (rle_cnt == 1) 
    {
      xaULONG i,num_to_alloc;
      xaUBYTE *p;
      if (x11_display_type & XA_X11_TRUE) num_to_alloc = 3;
      else num_to_alloc = rle_hdr.chan_num;
      p = rle_row_buf = (xaUBYTE *)malloc( num_to_alloc * rle_imagex );
      if (rle_row_buf == 0) TheEnd1("RLE rowbuf malloc err");
      for(i=0; i<num_to_alloc; i++) { rle_rows[i] = p; p += rle_imagex; };
    }
    if (    (cmap_true_map_flag == xaTRUE) && (rle_hdr.chan_num == 3)
        && !(x11_display_type & XA_X11_TRUE) )
		rle_pic = (xaUBYTE *)malloc( 3 * rle_image_size );
    else	rle_pic = (xaUBYTE *)malloc( XA_PIC_SIZE(rle_image_size) );
    if (rle_pic == 0) TheEnd1("RLE_Get_Image: malloc err\n");

    im_ptr = rle_pic;
    y = rle_imagey; while(y--)
    {
      RLE_Read_Row(fin,&rle_hdr);

      /* if three channel image then remap data with rle_chan_map */
      if (rle_hdr.chan_num == 3)
      {
	register int i; register xaUBYTE *p0,*p1,*p2;
	i = rle_imagex; p0 = rle_rows[0]; p1 = rle_rows[1]; p2 = rle_rows[2];
	while(i--) 
	{ 
	  *p0++ = rle_chan_map[RLE_RED][(xaULONG)(*p0)];
	  *p1++ = rle_chan_map[RLE_GREEN][(xaULONG)(*p1)]; 
	  *p2++ = rle_chan_map[RLE_BLUE][(xaULONG)(*p2)]; 
	}
      }

      if (x11_display_type & XA_X11_TRUE)
      {
	register int i; register xaUBYTE *p0,*p1,*p2;

	/* expand to rgb if color mapped image */
	if (rle_hdr.chan_num == 1)
	{
	  i = rle_imagex; p0 = rle_rows[0]; p1 = rle_rows[1]; p2 = rle_rows[2];
	  while(i--)
	  { register xaULONG d = (xaULONG)(*p0);
	    *p0++ = rle_chan_map[RLE_RED][d];
	    *p1++ = rle_chan_map[RLE_GREEN][d];
	    *p2++ = rle_chan_map[RLE_BLUE][d];
	  }
	}

	i = rle_imagex; p0 = rle_rows[0]; p1 = rle_rows[1]; p2 = rle_rows[2];
        if (x11_bytes_pixel == 1)
        {
          xaUBYTE *iptr = (xaUBYTE *)(rle_pic + (y * rle_imagex));
	  while(i--) *iptr++ = (xaUBYTE)
	    X11_Get_True_Color((xaULONG)(*p0++),(xaULONG)(*p1++),(xaULONG)(*p2++),8);
	}
        else if (x11_bytes_pixel == 2)
        {
          xaUSHORT *iptr = (xaUSHORT *)(rle_pic + (2 * y * rle_imagex));
	  while(i--) *iptr++ = (xaUSHORT)
	    X11_Get_True_Color((xaULONG)(*p0++),(xaULONG)(*p1++),(xaULONG)(*p2++),8);
	}
        else
        {
          xaULONG *iptr = (xaULONG *)(rle_pic + (4 * y * rle_imagex));
	  while(i--) *iptr++ = (xaULONG)
	    X11_Get_True_Color((xaULONG)(*p0++),(xaULONG)(*p1++),(xaULONG)(*p2++),8);
	}
      }
      else if (rle_hdr.chan_num == 1) /* color mapped image */
      {
        im_ptr = (xaUBYTE *)(rle_pic + y * rle_imagex);
        for(x=0; x < rle_imagex; x++) *im_ptr++ = (xaUBYTE)rle_rows[0][x];
      }
      else if (cmap_true_map_flag == xaTRUE) /* 3 channels */
      {
        im_ptr = (xaUBYTE *)(rle_pic + (3 * y * rle_imagex));
        for(x=0; x < rle_imagex; x++)
	{
	  *im_ptr++ = (xaUBYTE)rle_rows[0][x];
	  *im_ptr++ = (xaUBYTE)rle_rows[1][x];
	  *im_ptr++ = (xaUBYTE)rle_rows[2][x];
	}
      }
      else
      {
        im_ptr =  (xaUBYTE *)( rle_pic + (y * rle_imagex));
        if (cmap_true_to_332 == xaTRUE)
	{
          for(x=0; x < rle_imagex; x++)
	  {
	    *im_ptr++ = CMAP_GET_332( (xaULONG)(rle_rows[0][x]),
		(xaULONG)(rle_rows[1][x]), (xaULONG)(rle_rows[2][x]), CMAP_SCALE8 );
	  }
	}
        else 
	{
          for(x=0; x < rle_imagex; x++)
	  {
	    *im_ptr++ = CMAP_GET_GRAY( (xaULONG)(rle_rows[0][x]),
		(xaULONG)(rle_rows[1][x]), (xaULONG)(rle_rows[2][x]), CMAP_SCALE13 );
	  }
	}
      }
    }

    act = ACT_Get_Action(anim_hdr,ACT_MAPPED);

    /* Create Color Maps */
    if (rle_hdr.chan_num == 1) /* images with color maps */
    { register xaULONG i;
      for(i=0; i<rle_imagec; i++)
      {
	rle_cmap[i].red   = rle_chan_map[RLE_RED][i];
	rle_cmap[i].green = rle_chan_map[RLE_GREEN][i];
	rle_cmap[i].blue  = rle_chan_map[RLE_BLUE][i];
      }
      rle_chdr = ACT_Get_CMAP(rle_cmap,rle_imagec,0,rle_imagec,0,8,8,8);
    }
    else if (cmap_true_map_flag == xaTRUE)  /* 3 channels */
    {
      xaUBYTE *tpic;

      if (    (cmap_true_to_all == xaTRUE)
          || ((cmap_true_to_1st == xaTRUE) && (rle_chdr == 0) )
         )		rle_chdr = CMAP_Create_CHDR_From_True(rle_pic,8,8,8,
				rle_imagex,rle_imagey,rle_cmap,&rle_imagec);
      else if ( (cmap_true_to_332 == xaTRUE) && (rle_chdr == 0) )
			rle_chdr = CMAP_Create_332(rle_cmap,&rle_imagec);
      else if ( (cmap_true_to_gray == xaTRUE) && (rle_chdr == 0) )
			rle_chdr = CMAP_Create_Gray(rle_cmap,&rle_imagec);

      if (cmap_dither_type == CMAP_DITHER_FLOYD)
        tpic = UTIL_RGB_To_FS_Map(0,rle_pic,rle_chdr,
				rle_imagex,rle_imagey,xaTRUE);
      else
        tpic = UTIL_RGB_To_Map(0,rle_pic,rle_chdr,rle_imagex,rle_imagey,xaTRUE);
      FREE(rle_pic,0x8001);
      rle_pic = tpic;
    } /* NOTE: 332 and Gray Cases are done much earlier */
    else  /* 3 channels to either gray or 332 */
    {
      if (rle_chdr == 0)  /* error, this should have happened earlier */
	TheEnd1("RLE_Read_File: 332/gray error");
    }

    if (x11_display_type & XA_X11_TRUE)
    { 
      ACT_Setup_Mapped(act,rle_pic, 0,
                     rle_xpos,rle_ypos,rle_imagex,rle_imagey,
                     rle_imagex,rle_imagey,xaFALSE,0,xaTRUE,xaTRUE,xaTRUE);
    }
    else
    { 
      ACT_Setup_Mapped(act,rle_pic, rle_chdr,
                     rle_xpos,rle_ypos,rle_imagex,rle_imagey,
                     rle_imagex,rle_imagey,xaFALSE,0,xaTRUE,xaTRUE,xaFALSE);
      ACT_Add_CHDR_To_Action(act,rle_chdr);
    }
    RLE_Add_Frame(100,act);

  }
  if (rle_row_buf) { FREE(rle_row_buf,0x8002); rle_row_buf=0; }
  fclose(fin);
  return(xaTRUE);
}


void RLE_Read_Row(fp,rle_hdr)
FILE *fp;
RLE_HDR *rle_hdr;
{
  xaULONG posx,opcode,opdata,data;
  xaULONG ymax,channel,exit_flag;
 
  ymax = rle_ypos + rle_imagey;
  posx = 0;
  exit_flag = 0;

  /* clear to background if necessary */
  if ( !(rle_hdr->flags & RLEH_NO_BACKGROUND) ) 
  { register xaULONG i;
    for(i=0; i < rle_hdr->chan_num; i++)
		memset( (char *)rle_rows[i], bg_color[i], rle_hdr->xsize);
  }

  if (priv_vert_skip) 
  {
    priv_vert_skip--; 
    DEBUG_LEVEL3 fprintf(stderr,"  Prev Skip %ld\n",priv_vert_skip);
    return;
  }
  if ( priv_is_eof ) return;  /* EOF already encountered */

  while(!exit_flag)
  {
    opcode = getc(fp) & 0xff;
    opdata = getc(fp) & 0xff;
    switch( RLE_OPCODE(opcode) )
    {
      case RLE_SkipLinesOp:
	if (RLE_LONGP(opcode))  priv_vert_skip = UTIL_Get_LSB_Short(fp); 
        else priv_vert_skip = opdata;
	priv_vert_skip--;  /* count this line */
	posx = 0;
        exit_flag = 1; 
	break;
      case RLE_SetColorOp:
	channel = opdata; posx = 0;
	break;
      case RLE_SkipPixelsOp:
	if (RLE_LONGP(opcode))  posx += UTIL_Get_LSB_Short(fp); 
        else posx += opdata;
	break;
      case RLE_ByteDataOp:
	{ register xaULONG i;
	if (RLE_LONGP(opcode))  opdata = UTIL_Get_LSB_Short(fp); 
	opdata++; i = opdata;
        if (channel < 3)
		  while(i--) {rle_rows[channel][posx] = getc(fp); posx++;}
	else	{ posx += opdata; while(i--) (void)getc(fp); }
	if (opdata & 0x01) (void)getc(fp);  /* pad to xaSHORT */
	}
	break;
      case RLE_RunDataOp:
	if (RLE_LONGP(opcode))  opdata = UTIL_Get_LSB_Short(fp); 
	opdata++;
	data = getc(fp) & 0xff;  (void)getc(fp);
        if (channel < 3)
		while(opdata--) {rle_rows[channel][posx] = data; posx++;}
	else	posx += opdata;
	break;
      case RLE_EOFOp:
	priv_is_eof = xaTRUE;
	exit_flag = 1;
	break;
      default:
	fprintf(stderr,"RLE unknown opcode %lx\n",RLE_OPCODE(opcode));
	exit_flag = 1;
    } /* end of opcode switch */
  } /* end of while */
}


