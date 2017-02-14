
/*
 * xanim_dl.c
 *
 * Copyright (C) 1990,1991,1992,1993,1994,1995 by Mark Podlipec. 
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

#include "xanim_dl.h"

XA_CHDR   *ACT_Get_CMAP();
XA_ACTION *ACT_Get_Action();
void  ACT_Setup_Mapped();
void  ACT_Add_CHDR_To_Action();
void  ACT_Setup_Loop();
LONG  UTIL_Get_LSB_Short();
ULONG UTIL_Get_LSB_Long();
void  UTIL_Sub_Image();
ULONG DL_Read_File();
extern XA_ANIM_SETUP *XA_Get_Anim_Setup();
void XA_Free_Anim_Setup();
ULONG DL_Decode_Image();
ACT_DLTA_HDR *DL_Get_Dlta_HDR();

static ULONG dl_version;
static ULONG dl_format;
static UBYTE dl_title[41];
static UBYTE dl_author[41];
static ULONG dl_num_of_screens;
static ULONG dl_num_of_images;
static ULONG dl_ims_per_screen;
static ULONG dl_num_of_frames;
static ULONG dl_frame_cnt;
static ULONG dl_loop_frame;
static ULONG dl_txt_size;


static XA_ACTION **dl_acts;
static ULONG dl_image_cnt;

typedef struct DL_FRAME_STRUCT
{
  ULONG type;
  ULONG cnt;
  ULONG dat0,dat1,dat2,dat3;
  ULONG time;
  ULONG timelo;
  XA_ACTION *act;
  struct DL_FRAME_STRUCT *loop;
  struct DL_FRAME_STRUCT *prev_loop;
  struct DL_FRAME_STRUCT *next;
} DL_FRAME;

DL_FRAME *DL_Add_Cmd();
void DL_Free_Cmd_List();
ULONG DL_Cnt_Cmds();
ULONG DL_Gen_Frame_List();
static DL_FRAME *dl_cmd_start,*dl_cmd_cur;
static ULONG dl_cmd_cnt;

/**********
 *
 ****/
DL_FRAME *DL_Add_Cmd(type,act,time,timelo)
ULONG type;
XA_ACTION *act;
ULONG time,timelo;
{ DL_FRAME *fframe;

  fframe = (DL_FRAME *) malloc(sizeof(DL_FRAME));
  if (fframe == 0) TheEnd1("DL_Add_Cmd: malloc err");

  fframe->type = type;
  fframe->act = act;
  fframe->time   = time;
  fframe->timelo = timelo;
  fframe->cnt = 0;
  fframe->dat0 = fframe->dat1 = fframe->dat2 = fframe->dat3 = 0;
  fframe->next = 0;
  fframe->loop = 0;

  if (dl_cmd_start == 0) dl_cmd_start = fframe;
  else dl_cmd_cur->next = fframe;

  dl_cmd_cur = fframe;
  dl_cmd_cnt++;
  return(fframe);
}

/**********
 *
 ****/
void DL_Free_Cmd_List(fframes)
DL_FRAME *fframes;
{ DL_FRAME *ftmp;
  while(fframes != 0)
  { ftmp = fframes;
    fframes = fframes->next;
    FREE(ftmp,0x2000);
  }
}

/**********
 *
 *****/
LONG Is_DL_File(filename)
char *filename;
{
  FILE *fin;
  ULONG data0,data1;

  if ( (fin=fopen(filename,XA_OPEN_MODE)) == 0) return(XA_NOFILE);
  data0 = fgetc(fin);
  data1 = fgetc(fin);
  fclose(fin);
  if (data0 == 0x01) return(TRUE);
  if (data0 == 0x02)
  {
    if ( (data1 >= 0x00) && (data1 <= 0x03) ) return(TRUE);
  }
  if (data0 == 0x03) return(TRUE);
  return(FALSE);
}

/**********
 *
 ****/
ULONG DL_Read_File(fname,anim_hdr)
XA_ANIM_HDR *anim_hdr;
char *fname;
{ FILE *fin;
  LONG i,j,tmp;
  XA_ACTION *prev_end_act;
  XA_ANIM_SETUP *dl;

  dl = XA_Get_Anim_Setup();
  dl->vid_time = XA_GET_TIME(100);

  dl_acts = 0; 

  dl_cmd_start = 0;
  dl_cmd_cur   = 0;
  dl_cmd_cnt   = 0;

  dl_ims_per_screen = 1;

  if ( (fin = fopen(fname,XA_OPEN_MODE)) == 0)
  { 
    fprintf(stderr,"DL_Read_File: Can't open %s for reading.\n",fname); 
    return(FALSE);
  }

  dl_version = fgetc(fin);  /* either 1 or 2 */

/* EVENTUALLY MERGE below two switches into one */
  switch(dl_version)
  {
    case DL_VERSION_1:
		dl_format = DL_FORM_MEDIUM;
		dl_txt_size = 20;
		break;
    case DL_VERSION_2:
		dl_format = fgetc(fin);
		dl_txt_size = 20;
		break;
    case DL_VERSION_3:
		dl_format = fgetc(fin);
		dl_format = DL_FORM_LARGE; 
		dl_txt_size = 40;
		/* not really. size defined later.kludge */
		break;
    default:
		break;
  }

  switch(dl_format)
  {
    case DL_FORM_LARGE:
	dl->imagex = DL_LARGE_XSIZE;
	dl->imagey = DL_LARGE_YSIZE;
	dl_ims_per_screen = 1;
	break;
    case DL_FORM_MEDIUM:
	dl->imagex = DL_MEDIUM_XSIZE;
	dl->imagey = DL_MEDIUM_YSIZE;
	dl_ims_per_screen = 4;
	break;
    case DL_FORM_SMALL:
	dl->imagex = DL_SMALL_XSIZE;
	dl->imagey = DL_SMALL_YSIZE;
	dl_ims_per_screen = 16;
	break;
    default:
	fprintf(stderr,"DL_Read_File: unknown format %lx\n",dl_format);
	return(FALSE);
	break;
  }

/*POD TEMP UNKNOWN STUFF */
  if (dl_version==DL_VERSION_3)
  { int cnt;
    cnt = 50;
    while(cnt--) {getc(fin); }
  }

  /********* GET TITLE ***********************/
  dl_title[dl_txt_size] = 0;
  for(i=0; i<dl_txt_size; i++) 
  {
    tmp = fgetc(fin);
    dl_title[i] = (tmp)?(tmp ^ 0xff):tmp;
  }

  /********* GET AUTHOR ***********************/
  dl_author[0] = dl_author[dl_txt_size] = 0;
  if ( (dl_version == DL_VERSION_2) || (dl_version == DL_VERSION_3) )
  {
    for(i=0; i< dl_txt_size; i++)
    {
      tmp = fgetc(fin);
      dl_author[i] = (tmp)?(tmp ^ 0xff):tmp;
    }
  }

  if (dl_version == DL_VERSION_3)
    dl_num_of_screens = UTIL_Get_LSB_Short(fin);
  else
    dl_num_of_screens = fgetc(fin);

  dl_num_of_images = dl_num_of_screens * dl_ims_per_screen;

  if (dl_version == DL_VERSION_1)
     dl_num_of_frames = UTIL_Get_LSB_Short(fin);
  else if (dl_version == DL_VERSION_2)
     dl_num_of_frames = UTIL_Get_LSB_Long(fin);
  else
  { ULONG dl_num_of_audio;
    dl_num_of_frames = UTIL_Get_LSB_Short(fin);
    dl_num_of_audio = UTIL_Get_LSB_Short(fin);
  }
     

  dl->imagec = DL_MAX_COLORS;
  for(i=0; i < DL_MAX_COLORS; i++)
  {
    dl->cmap[i].red   = fgetc(fin) & 0x3f;
    dl->cmap[i].green = fgetc(fin) & 0x3f;
    dl->cmap[i].blue  = fgetc(fin) & 0x3f;
  }
  dl->chdr = ACT_Get_CMAP(dl->cmap,DL_MAX_COLORS,0,DL_MAX_COLORS,0,6,6,6);
  
 DEBUG_LEVEL1
 {
   fprintf(stderr,"   Version %ld  Format %ld",dl_version,dl_format);
   fprintf(stderr," Images %ld  Frames %ld\n",
			dl_num_of_images, dl_num_of_frames );
   fprintf(stderr,"   Title = %s  Author = %s\n",dl_title,dl_author);
 }

  if (dl_version != DL_VERSION_3)
  {
    if (dl->imagex > dl->max_imagex) dl->max_imagex = dl->imagex;
    if (dl->imagey > dl->max_imagey) dl->max_imagey = dl->imagey;
  }

  dl->pic_size = dl->imagex * dl->imagey;

  dl_acts = (XA_ACTION **)malloc(dl_num_of_images * sizeof(XA_ACTION *));
  dl_image_cnt = 0;

/* Since dl_format is fixed per file, move this loop down and replicate*/
  for(j = 0; j < dl_num_of_screens; j++)
  {
    switch(dl_format)
    {
      case DL_FORM_LARGE: /* large */
	{ ACT_DLTA_HDR *dlta_hdr;
	  XA_ACTION *act;

          if (dl_version == DL_VERSION_3)
	  {
	    dl->imagex = UTIL_Get_LSB_Short(fin);
	    dl->imagey = UTIL_Get_LSB_Short(fin);
	    if (dl->imagex > dl->max_imagex) dl->max_imagex = dl->imagex;
	    if (dl->imagey > dl->max_imagey) dl->max_imagey = dl->imagey;
	    dl->pic_size = dl->imagex * dl->imagey;
	    DEBUG_LEVEL1 fprintf(stderr,"DL %ld) size %ld %ld\n",
						j,dl->imagex,dl->imagey);
	  }
	  DEBUG_LEVEL2 fprintf(stderr,"Read large format image\n");

          act = ACT_Get_Action(anim_hdr,ACT_DELTA);
	  dl_acts[dl_image_cnt] = act;  dl_image_cnt++;

	  dlta_hdr = DL_Get_Dlta_HDR(fin,dl->pic_size,act,
					dl->imagex,dl->imagey,0,TRUE);
	  if (dlta_hdr==0) return(FALSE);
          fseek(fin,dl->pic_size,1);  /* skip over image */
	  if (dl->pic_size > dl->max_fvid_size)
					dl->max_fvid_size = dl->pic_size;
	  dl->pic = 0;
	  ACT_Setup_Delta(dl,act,dlta_hdr,fin);
	}
	break;

      case DL_FORM_MEDIUM: /* medium */
      case DL_FORM_SMALL:  /* small */
	{ ULONG r,rows,off,small_flag = (dl_format == DL_FORM_SMALL)?(1):(0);

	  rows = (small_flag)?(4):(2);
	  off = rows - 1;

	  DEBUG_LEVEL2 fprintf(stderr,"Read small/medium image\n");

	  r = rows;
          while(r--)
	  { ACT_DLTA_HDR *d_hdr0,*d_hdr1,*d_hdr2,*d_hdr3;
	    XA_ACTION    *act0,*act1,*act2,*act3;
	    ULONG s_size = rows * dl->imagex * dl->imagey;

	    /*** dlta_hdr for image0 */
	    act0 = ACT_Get_Action(anim_hdr,ACT_DELTA);
	    dl_acts[dl_image_cnt] = act0;  dl_image_cnt++;
	    act1 = ACT_Get_Action(anim_hdr,ACT_DELTA);
	    dl_acts[dl_image_cnt] = act1;  dl_image_cnt++;
	    if (small_flag)
	    {
	      act2 = ACT_Get_Action(anim_hdr,ACT_DELTA);
	      dl_acts[dl_image_cnt] = act2;  dl_image_cnt++;
	      act3 = ACT_Get_Action(anim_hdr,ACT_DELTA);
	      dl_acts[dl_image_cnt] = act3;  dl_image_cnt++;
	    }

	    if (xa_file_flag == TRUE)
	    { if (s_size > dl->max_fvid_size) dl->max_fvid_size = s_size;
	      d_hdr0 = DL_Get_Dlta_HDR(fin,s_size,act0,
					dl->imagex,dl->imagey,off,FALSE);
	      if (d_hdr0 == 0) return(FALSE);
	      fseek(fin,dl->imagex,1); s_size -= dl->imagex;
	      d_hdr1 = DL_Get_Dlta_HDR(fin,s_size,act1,
					dl->imagex,dl->imagey,off,FALSE);
	      if (d_hdr1 == 0) return(FALSE);
	      if (small_flag)
	      {
	        fseek(fin,dl->imagex,1); s_size -= dl->imagex;
	        d_hdr2 = DL_Get_Dlta_HDR(fin,s_size,act2,
					dl->imagex,dl->imagey,off,FALSE);
	        if (d_hdr2 == 0) return(FALSE);
	        fseek(fin,dl->imagex,1); s_size -= dl->imagex;
	        d_hdr3 = DL_Get_Dlta_HDR(fin,s_size,act3,
					dl->imagex,dl->imagey,off,FALSE);
	        if (d_hdr3 == 0) return(FALSE);
	      }
	      fseek(fin,s_size,1);
	    }
	    else
	    { ULONG y = dl->imagey;
	      UBYTE *dp0,*dp1,*dp2,*dp3;

	      d_hdr0 = DL_Get_Dlta_HDR(fin,dl->pic_size,act0,
					dl->imagex,dl->imagey,0,FALSE);
	      if (d_hdr0 == 0) return(FALSE);
	      dp0 = d_hdr0->data;	

	      d_hdr1 = DL_Get_Dlta_HDR(fin,dl->pic_size,act1,
					dl->imagex,dl->imagey,0,FALSE);
	      if (d_hdr1 == 0) return(FALSE);
	      dp1 = d_hdr1->data;

	      if (small_flag)
	      { 
		d_hdr2 = DL_Get_Dlta_HDR(fin,dl->pic_size,act2,
					dl->imagex,dl->imagey,0,FALSE);
		if (d_hdr2 == 0) return(FALSE);
		dp2 = d_hdr2->data;

		d_hdr3 = DL_Get_Dlta_HDR(fin,dl->pic_size,act3,
					dl->imagex,dl->imagey,0,FALSE);
		if (d_hdr3 == 0) return(FALSE);
		dp3 = d_hdr3->data; 
	      }
	      while(y--)
	      { fread((char *)(dp0),dl->imagex,1,fin); dp0 += dl->imagex;
		fread((char *)(dp1),dl->imagex,1,fin); dp1 += dl->imagex;
		if (small_flag)
	        { fread((char *)(dp2),dl->imagex,1,fin); dp2 += dl->imagex;
		  fread((char *)(dp3),dl->imagex,1,fin); dp3 += dl->imagex;
	        }
	      }
	    }

	    dl->pic = 0;	ACT_Setup_Delta(dl,act0,d_hdr0,fin);
	    dl->pic = 0;	ACT_Setup_Delta(dl,act1,d_hdr1,fin);
	    if (small_flag)
	    {
	      dl->pic = 0;	ACT_Setup_Delta(dl,act2,d_hdr2,fin);
	      dl->pic = 0;	ACT_Setup_Delta(dl,act3,d_hdr3,fin);
	    }
	  } /* end of rows */
	} /* end of case */
	break;
   }
 }

 if (xa_verbose == TRUE)
 {
   fprintf(stderr,"   Version %ld  Format %ld",dl_version,dl_format);
   fprintf(stderr," Images %ld  Frames %ld\n",
			dl_num_of_images, dl_num_of_frames );
   fprintf(stderr,"   Title = %s  Author = %s\n",dl_title,dl_author);
 }

 prev_end_act = 0;
 dl_loop_frame = 0;
 dl_frame_cnt = 0;
 switch(dl_version)
 {
   case DL_VERSION_1:
	for(i=0; i < dl_num_of_frames; i++)
	{ DL_FRAME *dl_fm;
	  register ULONG tmp;
	  tmp = fgetc(fin);
	  tmp = (tmp % 10) - 1 + ((tmp / 10) - 1) * 4;

	  if (tmp < dl_image_cnt)
	  {
	    dl_fm = DL_Add_Cmd(tmp,0,dl->vid_time,dl->vid_timelo);
	    dl_fm->act = dl_acts[tmp];
	    dl_frame_cnt++;
	  }
	  else fprintf(stderr,"   unknown code - ignored. %lx\n",tmp);
	}
	break;
   case DL_VERSION_3: /* POD TEMP FOR NOW */
   case DL_VERSION_2:
	{ DL_FRAME *dl_fm,*dl_endloop;
          ULONG dl_ffea_xpos,dl_ffea_ypos;
          dl_ffea_xpos = dl_ffea_ypos = 0;
	  DEBUG_LEVEL2 fprintf(stderr," DL reading frame lst: ");
	  dl_endloop = 0;
	  i = 0;
	  while(i < dl_num_of_frames)
	  { register ULONG tmp;
	    tmp  =  fgetc(fin); tmp |=  ( fgetc(fin) << 8); i++;
	    DEBUG_LEVEL2 fprintf(stderr,"\t<%ld %lx>",i,tmp);

	    if (tmp & 0x8000)
	    {
	      switch(tmp)
	      {
		case 0xffff:
		  { ULONG cnt;
		    cnt  =  UTIL_Get_LSB_Short(fin); i++;
		    DEBUG_LEVEL1 fprintf(stderr,"DL: begin loop %ld\n",cnt);
		    if (cnt > 20) cnt = 20; /* POD NOTE: ARBITRARY */
		    dl_fm = DL_Add_Cmd(0xffff,0,0,0);
		    dl_fm->cnt = cnt;
			/* add to prev loop chain */
		    dl_fm->prev_loop = dl_endloop;
		    dl_endloop = dl_fm;
		  }
		  break;
		case 0xfffe:
		  { DL_FRAME *loop_start = dl_endloop;
		    if (loop_start)
		    { dl_fm = DL_Add_Cmd(0xfffe,0,0,0);
		      loop_start->loop = dl_fm;
		      dl_fm->loop = loop_start;
		      dl_endloop = loop_start->prev_loop;
		      DEBUG_LEVEL1 fprintf(stderr,"DL: end loop\n");
		    }
		    else fprintf(stderr,"DL: invalid end loop\n");
		  }
		  break;
		case 0xFFFD:
		  tmp  =  UTIL_Get_LSB_Short(fin); i++;
		  DEBUG_LEVEL1 fprintf(stderr,"DL: key press %lx\n",tmp);
		  break;
		default: 
		  fprintf(stderr,"DL: unk code %lx\n",tmp);
		  break;
	      }
	    }
	    else /* code is below 8000 */
	    {
	      if (tmp < dl_image_cnt)
	      {
/* NOTE YET DL 3.0 only 
	        if ( (dl_ffea_xpos) || (dl_ffea_ypos) )
		else
*/
		DEBUG_LEVEL1 fprintf(stderr,"DL: image %ld\n",tmp);
		dl_fm = DL_Add_Cmd(tmp,0,dl->vid_time,dl->vid_timelo);
		dl_fm->act = dl_acts[tmp];
	        dl_frame_cnt++;
	      }
	      else fprintf(stderr,"   unknown code - ignored. %lx\n",tmp);
	    }
	  }
	  DEBUG_LEVEL2 fprintf(stderr,"\n");
	  if (dl_loop_frame >= dl_frame_cnt) dl_loop_frame = 0;
	}
	break;
  }
  fclose(fin);

DEBUG_LEVEL1 fprintf(stderr,"OLD: dl_frame_cnt %ld\n",dl_frame_cnt);
dl_frame_cnt = DL_Cnt_Cmds();
DEBUG_LEVEL1 fprintf(stderr,"NEW: dl_frame_cnt %ld\n",dl_frame_cnt);

  anim_hdr->frame_lst = 
	(XA_FRAME *)malloc(sizeof(XA_FRAME) * (dl_frame_cnt + 1) ); 
  if (anim_hdr->frame_lst == NULL)
      TheEnd1("DL_ANIM: couldn't malloc for frame_lst\0");

  dl_frame_cnt = DL_Gen_Frame_List(anim_hdr->frame_lst,dl_frame_cnt);

  anim_hdr->frame_lst[dl_frame_cnt].time_dur = 0;
  anim_hdr->frame_lst[dl_frame_cnt].zztime = 0;
  anim_hdr->frame_lst[dl_frame_cnt].act  = 0;
  if (dl_frame_cnt > 0)
  {
    anim_hdr->last_frame = dl_frame_cnt-1;
    anim_hdr->total_time =  anim_hdr->frame_lst[dl_frame_cnt-1].zztime
			  + anim_hdr->frame_lst[dl_frame_cnt-1].time_dur;
  }
  else
  {
    anim_hdr->last_frame = 0;
    anim_hdr->total_time = 0;
  }
  if (xa_buffer_flag == FALSE) anim_hdr->anim_flags |= ANIM_SNG_BUF;
  if (xa_file_flag == TRUE)    anim_hdr->anim_flags |= ANIM_USE_FILE;
  anim_hdr->anim_flags |= ANIM_FULL_IM; /* always full image - until V3 */
  anim_hdr->loop_frame = dl_loop_frame;
  anim_hdr->imagex = dl->max_imagex;
  anim_hdr->imagey = dl->max_imagey;
  anim_hdr->imagec = dl->imagec;
  anim_hdr->imaged = 8; /* nop */
  anim_hdr->max_fvid_size = dl->max_fvid_size;
  anim_hdr->max_faud_size = 0;
  anim_hdr->fname = anim_hdr->name;
  
  DL_Free_Cmd_List(dl_cmd_start);
  FREE(dl_acts,0x5001); dl_acts=0;
  XA_Free_Anim_Setup(dl);
  return(TRUE);
}

ULONG DL_Cnt_Cmds()
{ ULONG cnt = 0;
  DL_FRAME *fm_next,*fm = dl_cmd_start;

  while(fm)
  {
    fm_next = fm->next;
    if (fm->act) cnt++;
    else
    {
       switch(fm->type)
       {
         case 0xffff:
		fm->dat0 = fm->cnt;
DEBUG_LEVEL1 { fprintf(stderr,"BEGIN: dat0 <= %ld\n",fm->cnt); }
		break;
         case 0xfffe:
		{ DL_FRAME *lp = fm->loop;
		  lp->dat0--;
DEBUG_LEVEL1 { fprintf(stderr,"END: dat0 = %ld\n",lp->dat0); }
		  if (lp->dat0 > 0) fm_next = lp->next;
		}
		break;
         default: 
		fprintf(stderr,"DL_Cnt_Cmds: def err %lx\n",fm->type);
		break;
	  
       }
    } 
    fm = fm_next;
  }
  return(cnt);
}

ULONG DL_Gen_Frame_List(frame_lst,frame_cnt)
XA_FRAME *frame_lst;
LONG frame_cnt;
{ DL_FRAME *fm_next,*fm = dl_cmd_start;
  ULONG i,t_time,t_timelo;
  t_time = t_timelo = 0;

  i = 0;
  while(fm && (i < frame_cnt) )
  {
    fm_next = fm->next;
    if (fm->act)
    {
      frame_lst[i].act        = fm->act;
      frame_lst[i].time_dur   = fm->time;
      frame_lst[i].zztime     = t_time;
      t_time += fm->time;
      t_timelo += fm->timelo;
      while(t_timelo > (1<<24)) {t_time++; t_timelo -= (1<<24);}

DEBUG_LEVEL3 fprintf(stderr,"frame list %ld) type = %lx act = %lx\n",
		i,fm->type,frame_lst[i].act);
      i++;
    }
    else
    {
       switch(fm->type)
       {
         case 0xffff:
		fm->dat0 = fm->cnt;
		break;
         case 0xfffe:
		{ DL_FRAME *lp = fm->loop;
		  lp->dat0--;
		  if (lp->dat0 > 0) fm_next = lp->next;
		}
		break;
         default: fprintf(stderr,"DL_Cnt_Cmds: def err %lx\n",fm->type);
	  
       }
    }
    fm = fm_next;
  }
  return(i);
}

ACT_DLTA_HDR *DL_Get_Dlta_HDR(fin,fsize,act,imx,imy,extra,rd_flag)
FILE *fin;
ULONG fsize;
XA_ACTION *act;
ULONG imx,imy,extra;
ULONG rd_flag;
{ ACT_DLTA_HDR *dlta_hdr;
  if (xa_file_flag==TRUE)
  {
    dlta_hdr = (ACT_DLTA_HDR *) malloc(sizeof(ACT_DLTA_HDR));
    if (dlta_hdr == 0) TheEnd1("DL dlta: malloc err0");
    act->data = (UBYTE *)dlta_hdr;
    dlta_hdr->flags = ACT_SNGL_BUF;
    dlta_hdr->fpos  = ftell(fin);
    dlta_hdr->fsize = fsize;
  }
  else
  { ULONG d = fsize + (sizeof(ACT_DLTA_HDR));
    dlta_hdr = (ACT_DLTA_HDR *) malloc( d );
    if (dlta_hdr == 0) TheEnd1("DL dlta: malloc err1");
    act->data = (UBYTE *)dlta_hdr;
    dlta_hdr->flags = ACT_SNGL_BUF | DLTA_DATA;
    dlta_hdr->fpos  = 0;
    dlta_hdr->fsize = fsize;
    if (rd_flag == TRUE)
    { LONG ret,fpos = ftell(fin);
      ret = fread( dlta_hdr->data, fsize, 1, fin);
      if (ret != 1) {fprintf(stderr,"DL dlta: read err\n"); return(0);}
      fseek(fin,fpos,0);
    }
  }
  dlta_hdr->xpos = dlta_hdr->ypos = 0;
  dlta_hdr->xsize = imx;
  dlta_hdr->ysize = imy;
  dlta_hdr->extra = (void *)(extra);
  dlta_hdr->special = 0;
  dlta_hdr->delta = DL_Decode_Image;
  return(dlta_hdr);
}


ULONG
DL_Decode_Image(image,delta,dsize,chdr,map,map_flag,imagex,imagey,imaged,
					xs,ys,xe,ye,special,extra)
UBYTE *image;           /* Image Buffer. */
UBYTE *delta;           /* delta data. */
ULONG dsize;            /* delta size */
XA_CHDR *chdr;		/* color map info */
ULONG *map;             /* used if it's going to be remapped. */
ULONG map_flag;         /* whether or not to use remap_map info. */
ULONG imagex,imagey;    /* Size of image buffer. */
ULONG imaged;           /* Depth of Image. (IFF specific) */
ULONG *xs,*ys;          /* pos of changed area. */
ULONG *xe,*ye;          /* size of changed area. */
ULONG special;          /* Special Info. */
void *extra;		/* extra info needed to decode delta */
{ UBYTE *dp = delta;
  ULONG skip_lines = imagex * ((ULONG)(extra));
  ULONG x, y = imagey;

  if (map_flag==FALSE) 
  { UBYTE *ip = (UBYTE *)image; 
    while(y--) { x = imagex; while(x--) { *ip++ = (UBYTE)(*dp++); }
    dp += skip_lines; }
  }
  else if (x11_bytes_pixel==1) 
  { UBYTE *ip = (UBYTE *)image; 
    while(y--) 
	{ x = imagex; while(x--) { *ip++ = (UBYTE)map[*dp++]; } 
    dp += skip_lines; }
  }
  else if (x11_bytes_pixel==2) 
  { USHORT *ip = (USHORT *)image; 
    while(y--) 
	{ x = imagex; while(x--) { *ip++ = (USHORT)map[*dp++]; }
    dp += skip_lines; }
  }
  else /* if (x11_bytes_pixel==4) */
  { ULONG *ip = (ULONG *)image; 
    while(y--) 
	{ x = imagex; while(x--) { *ip++ = (ULONG)map[*dp++]; }
    dp += skip_lines; }
  }
  *xs = *ys = 0; *xe = imagex; *ye = imagey;
  if (map_flag==TRUE) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}
