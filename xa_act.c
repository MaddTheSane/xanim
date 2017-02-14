
/*
 * xa_act.c
 *
 * Copyright (C) 1990-1998,1999 by Mark Podlipec. 
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
 * 17Mar98:  Fixed bug in Make_Images with some types of Clips and TrueColor
 *	     clip_mask would get mapped to common color. Need to generate
 *	     clip masks prior to mapping.
 */
#include "xanim.h"
#include <Intrinsic.h>
#include <StringDefs.h>
#include <Shell.h>
#include <sys/signal.h>
#ifndef VMS
#include <sys/times.h>
#endif
#include <ctype.h>

#ifdef XSHM
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#endif /*XSHM*/

#include "xa_x11.h"
extern xaULONG shm;
extern Visual     *theVisual;

extern xaLONG xa_pixmap_flag;
extern xaUSHORT xa_gamma_adj[];

XA_ACTION *ACT_Get_Action();
void ACT_Setup_Mapped();
void ACT_Setup_Packed();
void ACT_Setup_Loop();
void ACT_Make_Images();
void ACT_Add_CHDR_To_Action();
void ACT_Del_CHDR_From_Action();
void ACT_Get_CCMAP();
XA_CHDR *ACT_Get_CMAP();
void ACT_Free_Act();
void ACT_Setup_Delta();

void UTIL_Mapped_To_Bitmap();
void UTIL_Mapped_To_Mapped();
void UTIL_Mapped_To_Floyd();
void UTIL_Pack_Image();
xaUBYTE *UTIL_Alloc_Scaled();
xaUBYTE *UTIL_Scale_Mapped();
xaUBYTE *UTIL_Scale_Bitmap();
xaULONG UTIL_Get_Buffer_Scale();
void UTIL_Create_Clip();
void UTIL_Sub_Image();
xaUBYTE *UTIL_RGB_To_FS_Map();
xaUBYTE *UTIL_RGB_To_Map();

extern xaULONG XA_Get_Image_Type();

XA_CHDR *CMAP_Create_CHDR_From_True();
XA_CHDR *CMAP_Create_332();
XA_CHDR *CMAP_Create_Gray();

XA_ACTION *act_first_cmap = 0;
XA_ACTION *act_cur_cmap = 0;


/*
 * Allocate, Add to Anim_Hdr and Return an Action.
 */
XA_ACTION *ACT_Get_Action(anim_hdr,type)
XA_ANIM_HDR *anim_hdr;
xaLONG type;
{
  XA_ACTION *act;

  act = (XA_ACTION *)malloc(sizeof(XA_ACTION));
  if (act == 0) TheEnd1("ACT_Get_Action: malloc err\n");

  act->type = type;
  act->cmap_rev = 0;
  act->data = 0;
  act->chdr = 0;
  act->h_cmap = 0;
  act->map = 0;
  act->next_same_chdr = 0;

  act->next = anim_hdr->acts;  /* insert at front of list */
  anim_hdr->acts = act;
  return(act);
}

/*
 *
 */
void
ACT_Setup_Mapped(act,ipic,chdr,xpos,ypos,xsize,ysize,width,height,
		clip_flag,clip_mask,
		free_ipic_flag,full_ipic_flag,already_disp_flag)
XA_ACTION *act;
xaUBYTE *ipic;
XA_CHDR *chdr;
xaLONG xpos,ypos,xsize,ysize;	/* image/sub-image dimensions */
xaLONG width,height;		/* screen dimensions */
xaULONG clip_flag;		/* if xaTRUE then create clip with clip_mask */ 
xaULONG clip_mask;		/* transparent pixel value of clip  */
xaULONG free_ipic_flag;		/* free ipic before returning */
xaULONG full_ipic_flag;		/* full screen given, not just image */
xaULONG already_disp_flag;	/* ipic already in display form */
{
  xaULONG line_size,buff_x,buff_y,need_to_scale;
  xaULONG in_x_size,in_y_size,inline_size,inpix_size,resize_flag;
  xaLONG map_size;
  xaUBYTE *t_pic,*clip_ptr;
  ACT_MAPPED_HDR *act_map_hdr;

	/* Figure out buffer scaling */
  need_to_scale = UTIL_Get_Buffer_Scale(width,height,&buff_x,&buff_y);
  
	/* get correct input buffer size */
  inpix_size = (already_disp_flag==xaTRUE)?x11_bytes_pixel:1;
  in_x_size = (full_ipic_flag==xaTRUE)?width:xsize;
  in_y_size = (full_ipic_flag==xaTRUE)?height:ysize;
  inline_size = in_x_size * inpix_size;
  if ( (full_ipic_flag == xaTRUE) && ((xsize != width) || (ysize != height)) )
	resize_flag = xaTRUE;
  else  resize_flag = xaFALSE;

  DEBUG_LEVEL2 fprintf(stderr,"Setup_Mapped:\n");
  DEBUG_LEVEL2 fprintf(stderr,"  <%d,%d> <%d,%d>\n",
                xpos,  ypos, xsize, ysize  );

  t_pic = clip_ptr = 0;
  act_map_hdr = (ACT_MAPPED_HDR *) malloc( sizeof(ACT_MAPPED_HDR) );
  if (act_map_hdr == 0) TheEnd1("ACT_Setup_Mapped: malloc err\n");

  map_size = (chdr)?(chdr->csize): 0 ;
  line_size = X11_Get_Line_Size(xsize);

/*******
 * t_pic should hold image data
 * i_pic should be left unchanged
 * xpos,ypos,xsize,ysize can change and should be correct at break;
 */
  switch (x11_display_type)
  {
    case XA_MONOCHROME:
          /* POD NOTE: Currently assumes ipic always get converted */
	if (need_to_scale == xaTRUE)
        {
	  xaULONG xpos_o,ypos_o,xsize_o,ysize_o;

	  xpos_o = xpos; ypos_o = ypos;
	  if (full_ipic_flag == xaFALSE) xpos = ypos = 0;
	  t_pic = UTIL_Alloc_Scaled(xsize,ysize,width,height,
		  buff_x,buff_y,1,xpos_o,ypos_o,xaTRUE);
	  if (t_pic)
	  {
	    xaUBYTE *p_pic;
	    p_pic = UTIL_Alloc_Scaled(xsize,ysize,width,height,
		  buff_x,buff_y,1,xpos_o,ypos_o,xaFALSE);
	    UTIL_Scale_Mapped(p_pic,ipic,xpos,ypos,xsize,ysize,
			inline_size,width,height,buff_x,buff_y,
			inpix_size,&xpos_o,&ypos_o,&xsize_o,&ysize_o,0);
            xpos = xpos_o; ypos = ypos_o; xsize = xsize_o; ysize = ysize_o;
	    UTIL_Mapped_To_Bitmap(t_pic,p_pic,chdr, xpos,ypos,
			xsize,ysize,buff_x,buff_y, X11_Get_Line_Size(xsize));
	    act->type = ACT_DISP; act_map_hdr->psize = 0;
	    if (clip_flag == xaTRUE)
	    {
	      clip_ptr = (xaUBYTE *) 
			malloc(ysize * (X11_Get_Bitmap_Width(xsize)/8) );
	      if (clip_ptr==0) TheEnd1("Setup_Mapped: clip malloc err\n");
	      UTIL_Create_Clip(clip_ptr,p_pic,clip_mask,xsize,ysize,
			inpix_size,X11_Get_Bitmap_Width(xsize),XA_LSBIT_1ST);
	    }
	    if (p_pic) { FREE(p_pic,0x106); p_pic = 0; }
          }
	  else /* scale to zero size */
	  {
	    act->type = ACT_NOP;	clip_flag = xaFALSE;
	    FREE(act_map_hdr,0x107);	act_map_hdr = 0;
	  }
        } /* end of need to scale */
	else
	{
	  t_pic = (xaUBYTE *) malloc(ysize * line_size);
	  if (t_pic == 0) TheEnd1("X11_Get_Image: t_pic malloc failed.\n");
	  UTIL_Mapped_To_Bitmap(t_pic,ipic,chdr,
			xpos,ypos,xsize,ysize,in_x_size,in_y_size,line_size);
	  if (clip_flag == xaTRUE)
	  {
	    clip_ptr = (xaUBYTE *) 
			malloc(ysize * (X11_Get_Bitmap_Width(xsize)/8) );
	    if (clip_ptr==0) TheEnd1("Setup_Mapped: clip malloc err\n");
	    UTIL_Create_Clip(clip_ptr,ipic,clip_mask,xsize,ysize,
			inpix_size,X11_Get_Bitmap_Width(xsize),XA_LSBIT_1ST);
          }
	  act->type = ACT_DISP; act_map_hdr->psize = 0;
	}
	break;
    case XA_DIRECTCOLOR:
    case XA_TRUECOLOR:
        inpix_size = x11_bytes_pixel;


	/* Create Bitmap based on Original GIF Image */
  if ( (clip_ptr==0) && (clip_flag==xaTRUE) )
  {
    clip_ptr = (xaUBYTE *) malloc(ysize * (X11_Get_Bitmap_Width(xsize)/8) );
    if (clip_ptr==0) TheEnd1("Setup_Mapped: clip malloc err\n");
    UTIL_Create_Clip(clip_ptr,ipic,clip_mask,xsize,ysize,
			1,X11_Get_Bitmap_Width(xsize),XA_LSBIT_1ST);
  }

	if (need_to_scale == xaTRUE)
        {
	  xaULONG xpos_o,ypos_o,xsize_o,ysize_o;

	  xpos_o = xpos; ypos_o = ypos;
	  if (full_ipic_flag == xaFALSE) xpos = ypos = 0;
	  t_pic = UTIL_Alloc_Scaled(xsize,ysize,width,height,
			buff_x,buff_y,x11_bytes_pixel,xpos_o,ypos_o,xaFALSE);
	  if (t_pic)
	  { 

	    if ((clip_ptr) && (clip_flag==xaTRUE))
	    { xaULONG cxpos, cypos, cxsize, cysize;
	      xaUBYTE *new_clip = UTIL_Scale_Bitmap(0,clip_ptr,
			xsize,ysize,(X11_Get_Bitmap_Width(xsize/8)),
			width, height,
			buff_x, buff_y,
			&cxpos, &cypos, &cxsize, &cysize,
			XA_LSBIT_1ST, XA_LSBIT_1ST);
	      if (new_clip) { free(clip_ptr); clip_ptr = new_clip; }
	      else { free(clip_ptr); clip_ptr = 0; clip_flag = xaFALSE; }
	    }

	    if (already_disp_flag==xaTRUE) chdr=0;
	    UTIL_Scale_Mapped(t_pic,ipic,xpos,ypos,xsize,ysize,
		inline_size,width,height,buff_x,buff_y,x11_bytes_pixel,
		&xpos_o,&ypos_o,&xsize_o,&ysize_o,chdr);
            xpos = xpos_o; ypos = ypos_o; xsize = xsize_o; ysize = ysize_o;
	    act->type = ACT_DISP;	act_map_hdr->psize = 8;

	  }
	  else /* scale to zero size */
	  {
	    act->type = ACT_NOP;	
	    FREE(act_map_hdr,0x101);	act_map_hdr = 0;
	    clip_flag = xaFALSE;
	    if (clip_ptr) { free(clip_ptr); clip_ptr = 0; }
	  }
	}
	else
        {
	  if (   (already_disp_flag == xaFALSE) || (free_ipic_flag == xaFALSE)
	      || (resize_flag == xaTRUE) )
	  { xaULONG t_xpos,t_ypos;
	    if (resize_flag == xaFALSE) { t_xpos = t_ypos = 0; }
	    else { t_xpos = xpos; t_ypos = ypos; }
	    t_pic = (xaUBYTE *) malloc(ysize * line_size);
	    if (t_pic == 0) TheEnd1("X11_Get_Image: t_pic malloc failed.\n");
	    if (already_disp_flag == xaFALSE)
	    {
	      UTIL_Mapped_To_Mapped(t_pic,ipic,chdr,
			t_xpos,t_ypos,xsize,ysize,in_x_size,in_y_size);
	    }
	    else  /* either not_full or can't-free, either way copy */
	    {
	      UTIL_Sub_Image(t_pic,ipic,xsize,ysize,t_xpos,t_ypos,
						in_x_size,x11_bytes_pixel);
	    }
	  }
	  else t_pic = ipic;
	  act->type = ACT_DISP;	act_map_hdr->psize = 0;
        }
	break;
    case XA_STATICGRAY:
    case XA_STATICCOLOR:
    case XA_GRAYSCALE:
    case XA_PSEUDOCOLOR:

	/* Create Bitmap based on Original GIF Image */
  if ( (clip_ptr==0) && (clip_flag==xaTRUE) )
  {
    clip_ptr = (xaUBYTE *) malloc(ysize * (X11_Get_Bitmap_Width(xsize)/8) );
    if (clip_ptr==0) TheEnd1("Setup_Mapped: clip malloc err\n");
    UTIL_Create_Clip(clip_ptr,ipic,clip_mask,xsize,ysize,
			1,X11_Get_Bitmap_Width(xsize),XA_LSBIT_1ST);
  }

	if (need_to_scale == xaTRUE)
        {
	  xaULONG xpos_o,ypos_o,xsize_o,ysize_o;

	  xpos_o = xpos; ypos_o = ypos;
	  if (full_ipic_flag == xaFALSE) xpos = ypos = 0;
	  t_pic = UTIL_Alloc_Scaled(xsize,ysize,width,height,
		buff_x,buff_y,x11_bytes_pixel,xpos_o,ypos_o,xaFALSE);
	  if (t_pic)
	  {
	    if ((clip_ptr) && (clip_flag==xaTRUE))
	    { xaULONG cxpos, cypos, cxsize, cysize;
	      xaUBYTE *new_clip = UTIL_Scale_Bitmap(0,clip_ptr,
			xsize,ysize,(X11_Get_Bitmap_Width(xsize/8)),
			width, height,
			buff_x, buff_y,
			&cxpos, &cypos, &cxsize, &cysize,
			XA_LSBIT_1ST, XA_LSBIT_1ST);
	      if (new_clip) { free(clip_ptr); clip_ptr = new_clip; }
	      else { free(clip_ptr); clip_ptr = 0; clip_flag = xaFALSE; }
	    }

	    UTIL_Scale_Mapped(t_pic,ipic,xpos,ypos,xsize,ysize,
			inline_size,width,height,buff_x,buff_y,
			inpix_size,&xpos_o,&ypos_o,&xsize_o,&ysize_o,0);
            xpos = xpos_o; ypos = ypos_o; xsize = xsize_o; ysize = ysize_o;
	    act->type = ACT_MAPPED;	act_map_hdr->psize = 8;
	  }
	  else /* scale to zero size */
	  {
	    act->type = ACT_NOP;	clip_flag = xaFALSE;
	    FREE(act_map_hdr,0x102);	act_map_hdr = 0;
	  }
        } /* end of need to scale */
        else
        {
	  if ( (free_ipic_flag == xaFALSE) || (full_ipic_flag == xaFALSE)
	      || (resize_flag == xaTRUE) )
	  { xaULONG t_xpos,t_ypos;
	    if (resize_flag == xaFALSE) { t_xpos = t_ypos = 0; }
	    else { t_xpos = xpos; t_ypos = ypos; }
	    t_pic = (xaUBYTE *) malloc(ysize * line_size);
	    if (t_pic == 0) TheEnd1("Setup_Mapped: malloc err 3\n");
            UTIL_Sub_Image(t_pic,ipic,xsize,ysize,t_xpos,t_ypos,
							in_x_size,inpix_size);
	  }
	  else t_pic = ipic;
	  act->type = ACT_MAPPED;
          act_map_hdr->psize = 8;
        }
	break;
     default:
	act->type = ACT_NOP;
	fprintf(stderr,"invalid X11 display ?? %x\n",x11_display_type);
	TheEnd();
  }

  if ( (clip_ptr==0) && (clip_flag==xaTRUE) )
  {
    clip_ptr = (xaUBYTE *) malloc(ysize * (X11_Get_Bitmap_Width(xsize)/8) );
    if (clip_ptr==0) TheEnd1("Setup_Mapped: clip malloc err\n");
    UTIL_Create_Clip(clip_ptr,t_pic,clip_mask,xsize,ysize,
			inpix_size,X11_Get_Bitmap_Width(xsize),XA_LSBIT_1ST);
  }

  act->data = (xaUBYTE *) act_map_hdr;
  if (act_map_hdr)
  {
    act_map_hdr->xpos = xpos;
    act_map_hdr->ypos = ypos;
    act_map_hdr->xsize = xsize;
    act_map_hdr->ysize = ysize;
    act_map_hdr->data = t_pic;
    act_map_hdr->clip = clip_ptr;
  }
  if ((free_ipic_flag==xaTRUE) && (t_pic != ipic)) 
				{ FREE(ipic,0x103); ipic = 0; }
}

XA_CHDR *ACT_Get_CHDR(rev,csize,coff,msize,moff,cmap_flag,map_flag)
xaULONG rev,csize,coff,msize,moff;
xaULONG cmap_flag,map_flag;
{
  XA_CHDR *chdr;

  chdr = (XA_CHDR *)malloc(sizeof(XA_CHDR));
  if (chdr == 0) TheEnd1("ACT_Get_CHDR: malloc err\n");

  DEBUG_LEVEL2 fprintf(stderr,"ACT_Get_CHDR %x\n",(xaULONG)chdr);

  chdr->rev    = rev;
  chdr->csize  = csize;
  chdr->coff   = coff;
  chdr->msize  = msize;
  chdr->moff   = moff;
  chdr->next   = 0;
  chdr->acts   = 0;
  chdr->new_chdr = 0;

  if ( (csize) && (cmap_flag) )
  {
    chdr->cmap = (ColorReg *)malloc(csize * sizeof(ColorReg));
    if (chdr->cmap == 0) TheEnd1("ACT_Get_CHDR: cmap malloc err\n");
  }
  else
  {
    chdr->cmap = 0;
    if ( (csize==0) && (cmap_flag) )
	 fprintf(stderr,"ACT_Get_CHDR: csize 0 err\n");
  }
  if ( (msize) && (map_flag) )
  {
    chdr->map = (xaULONG *)malloc(msize * sizeof(xaULONG));
    if (chdr->map == 0) TheEnd1("ACT_Get_CHDR: map malloc err\n");
  }
  else
  {
    chdr->map = 0;
    if ( (msize==0) && (map_flag) )
	 fprintf(stderr,"ACT_Get_CHDR: msize 0 err\n");
  }

  if (xa_chdr_start == 0) xa_chdr_start = chdr;
  else xa_chdr_cur->next = chdr;
  xa_chdr_cur = chdr;
  return(chdr);
}

/*
 * Allocate CMAP and put at end of list
 */
XA_CHDR *ACT_Get_CMAP(new_map,csize,coff,msize,moff,rbits,gbits,bbits)
ColorReg *new_map;
xaULONG csize,coff,msize,moff;
xaULONG rbits,gbits,bbits;
{
  xaULONG i;
  XA_CHDR *chdr;
  xaULONG rscale,gscale,bscale;

  if (   (msize > csize) || (moff < coff) 
      || ( (msize+moff) > (csize+coff) )  )
	TheEnd1("ACT_Get_CMAP: map not in cmap err\n");

  chdr = ACT_Get_CHDR(0,csize,coff,msize,moff,xaTRUE,xaTRUE);

  rscale = cmap_scale[rbits];
  gscale = cmap_scale[gbits];
  bscale = cmap_scale[bbits];

  DEBUG_LEVEL2 fprintf(stderr,"c scales %x %x %x\n",rscale,gscale,bscale);
  for(i = 0; i < csize; i++)
  {
    chdr->cmap[i].red   = xa_gamma_adj[ ((rscale * new_map[i].red  ) >> 8) ];
    chdr->cmap[i].green = xa_gamma_adj[ ((gscale * new_map[i].green) >> 8) ];
    chdr->cmap[i].blue  = xa_gamma_adj[ ((bscale * new_map[i].blue ) >> 8) ];

    DEBUG_LEVEL3 fprintf(stderr,"%d) %x %x %x\n",i,
	chdr->cmap[i].red,chdr->cmap[i].green,chdr->cmap[i].blue);
  }
  rscale *= 11;
  gscale *= 16;
  bscale *=  5;
  for(i = 0; i < csize; i++)
  {
    register xaULONG d = ( (   rscale * new_map[i].red
			   + gscale * new_map[i].green
			   + bscale * new_map[i].blue) >> 13 );
    chdr->cmap[i].gray = xa_gamma_adj[ d ];
  }
  if (x11_display_type & XA_X11_TRUE)
  {
    for(i = 0; i < msize; i++)
    {
      register xaULONG j;
      j = i + moff - coff;
      chdr->map[i] = X11_Get_True_Color(chdr->cmap[j].red,
			chdr->cmap[j].green,chdr->cmap[j].blue,16);
    }
  }
  else for(i = 0; i < csize; i++) chdr->map[i] = i + moff;
  return(chdr);
}

/*
 * Allocate CCMAP and put at end of list
 */
void ACT_Get_CCMAP(act,new_map,csize,coff,rbits,gbits,bbits)
XA_ACTION *act;
ColorReg *new_map;
xaULONG csize,coff;
xaULONG rbits,gbits,bbits;
{
  xaULONG i;
  ACT_CMAP_HDR *act_cmap_hdr;
  ColorReg *act_cmap;
  xaULONG rscale,gscale,bscale;

  act_cmap_hdr = (ACT_CMAP_HDR *)malloc(sizeof(ACT_CMAP_HDR) + 
					(csize * sizeof(ColorReg)) );
  if (act_cmap_hdr == 0) TheEnd1("ACT_Get_CCMAP: malloc err");

  act->type = ACT_CMAP;
  act->data = (xaUBYTE *) act_cmap_hdr;

  act_cmap_hdr->csize = csize;
  act_cmap_hdr->coff  = coff;
  act_cmap = (ColorReg *)act_cmap_hdr->data;

  rscale = cmap_scale[rbits];
  gscale = cmap_scale[gbits];
  bscale = cmap_scale[bbits];

  DEBUG_LEVEL2 fprintf(stderr,"c scales %x %x %x\n",rscale,gscale,bscale);
  for(i = 0; i < csize; i++)
  {
    act_cmap[i].red   = xa_gamma_adj[ ((rscale * new_map[i].red  ) >> 8) ];
    act_cmap[i].green = xa_gamma_adj[ ((gscale * new_map[i].green) >> 8) ];
    act_cmap[i].blue  = xa_gamma_adj[ ((bscale * new_map[i].blue ) >> 8) ];
    DEBUG_LEVEL3 fprintf(stderr,"%d) %x %x %x\n",i,
	act_cmap[i].red,act_cmap[i].green, act_cmap[i].blue);
  }

  rscale *= 11;
  gscale *= 16;
  bscale *=  5;
  for(i = 0; i < csize; i++)
  {
    register xaULONG d = ( (   rscale * new_map[i].red
			   + gscale * new_map[i].green
			   + bscale * new_map[i].blue) >> 13 );
    act_cmap[i].gray = xa_gamma_adj[ d ];
  }
}


void ACT_Add_CHDR_To_Action(act,chdr)
XA_ACTION  *act;
XA_CHDR *chdr;
{
  if ((act) && (chdr))
  {
    /* have act use chdr and map */
    act->chdr = chdr;
    act->map  = chdr->map;
    /* add action to chdr's action list */
    act->next_same_chdr = chdr->acts;
    chdr->acts = act;
  } else fprintf(stderr,"ACT_Add_CHDR_To_Action: Err %x %x\n",
						(xaULONG)act,(xaULONG)chdr);
}

void ACT_Del_CHDR_From_Action(act,chdr)
XA_ACTION  *act;
XA_CHDR *chdr;
{
  XA_ACTION *t_act;
  t_act = chdr->acts;
  if (t_act)
  {
    if (t_act == act) chdr->acts = act->next_same_chdr; 
    else
    {
      while (t_act->next_same_chdr)
      {
        if (t_act->next_same_chdr == act) 
		t_act->next_same_chdr = act->next_same_chdr;
        else t_act = t_act->next_same_chdr;
      }  
    }
  }
  act->chdr = 0;
  act->map  = 0;
}


void ACT_Make_Images(act)
XA_ACTION *act;
{
  xaULONG line_size,pixmap_type;
  XImage *image;
  ACT_MAPPED_HDR *act_map_hdr;


  DEBUG_LEVEL1 fprintf(stderr,"ACT_Make_Images: ");
  while(act)
  {
    DEBUG_LEVEL2 fprintf(stderr,"act %x \n",act->type);
    switch(act->type)
    {
      case ACT_SETTER:
	if (xa_pixmap_flag == xaFALSE) act->type = ACT_IMAGES;
	else act->type = ACT_PIXMAPS;
	if (act->chdr)
	{
	  if (act->chdr->new_chdr)
	  {
	    act->chdr = act->chdr->new_chdr;
	    act->cmap_rev = act->chdr->rev;
	  }
	}
	break;

      case ACT_CYCLE:
	if (act->chdr)
	{
	  if (act->chdr->new_chdr)
	  {
	    ACT_CYCLE_HDR *act_cycle;
	    xaULONG i,size,*i_ptr,moff,*map;

	    act_cycle = (ACT_CYCLE_HDR *)act->data;
	    size = act_cycle->size;
	    i_ptr = (xaULONG *)(act_cycle->data);
	    map    = act->chdr->map;
	    moff   = act->chdr->moff;
	    for(i=0; i<size; i++)
	      i_ptr[i] = map[ (i_ptr[i] - moff) ];
	    act->chdr = act->chdr->new_chdr;
	    act->cmap_rev = act->chdr->rev;
	  }
	}
	break;

      case ACT_CMAP:
	if (act->chdr)
	{
	  XA_CHDR *new_chdr;
	  new_chdr = act->chdr->new_chdr;
	  if (new_chdr)
	  {
	    ACT_CMAP_HDR *old_cmap_hdr,*new_cmap_hdr;
	    ColorReg *old_cmap,*new_cmap;
	    xaULONG i,new_csize,old_csize,old_coff;

            DEBUG_LEVEL2
		fprintf(stderr,"ACT_Make_Images remapping CMAP %x %x\n",
			(xaULONG)act->chdr, (xaULONG)new_chdr);

	    old_cmap_hdr = (ACT_CMAP_HDR *) act->data;
	    old_cmap = (ColorReg *)old_cmap_hdr->data;
	    old_csize = old_cmap_hdr->csize;
	    old_coff  = old_cmap_hdr->coff;
	    new_csize = new_chdr->csize;
		/* allocate new cmap_hdr */
	    new_cmap_hdr = (ACT_CMAP_HDR *)malloc(sizeof(ACT_CMAP_HDR) +
					(new_csize * sizeof(ColorReg)) );
	    if (new_cmap_hdr == 0)
		TheEnd1("ACT_Make_Images: cmap malloc err");
	    new_cmap_hdr->csize = new_csize;
	    new_cmap_hdr->coff  = new_chdr->coff;
	    new_cmap = (ColorReg *)new_cmap_hdr->data;
		/* remap cmap change based on chdr map */
	    memcpy((char*)new_cmap,(char*)new_chdr->cmap,
					(new_csize * sizeof(ColorReg))); 
	    for(i=0; i<old_csize; i++)
	    {
	      register xaULONG d;
	      d = act->chdr->map[ i + old_coff - act->chdr->coff ];
	      new_cmap[d].red   = old_cmap[i].red;
	      new_cmap[d].green = old_cmap[i].green;
	      new_cmap[d].blue  = old_cmap[i].blue;
	      new_cmap[d].gray  = old_cmap[i].gray;
	    }
	    FREE(old_cmap_hdr,0x104); old_cmap_hdr = 0;
	    act->data = (xaUBYTE *)new_cmap_hdr;
	    act->chdr = new_chdr;
	    act->cmap_rev = act->chdr->rev;
          } /* end of new chdr */
        } /* end of chdr */
	break;
      case ACT_MAPPED:
	if (act->chdr)
	{
	  if (act->chdr->new_chdr)
	  {
            DEBUG_LEVEL2
		fprintf(stderr,"ACT_Make_Images remapping MAPPED %x %x\n",
			(xaULONG)act->chdr, (xaULONG)act->chdr->new_chdr);

	    act_map_hdr = (ACT_MAPPED_HDR *) act->data;
	    /* POD TEMP currently assumes same size mapping */
            if (cmap_dither_type == CMAP_DITHER_FLOYD)
            {
	      UTIL_Mapped_To_Floyd(act_map_hdr->data,act_map_hdr->data,
		  act->chdr->new_chdr,act->chdr,
		  0,0,act_map_hdr->xsize,act_map_hdr->ysize,
		  act_map_hdr->xsize,act_map_hdr->ysize);
            }
            else
            {
	      UTIL_Mapped_To_Mapped(act_map_hdr->data,act_map_hdr->data,
		  act->chdr,
		  0,0,act_map_hdr->xsize,act_map_hdr->ysize,
		  act_map_hdr->xsize,act_map_hdr->ysize);
	    }
	    /* update chdr ptrs and revs */
	    act->chdr = act->chdr->new_chdr;
	    act->cmap_rev = act->chdr->rev;
	  } 
	  else if (x11_bytes_pixel > 1) /* need to map anyways */
	  {
DEBUG_LEVEL2 fprintf(stderr,"ACT_Make_Images kludge remapping MAPPED\n");

	    act_map_hdr = (ACT_MAPPED_HDR *) act->data;
	    UTIL_Mapped_To_Mapped(act_map_hdr->data,act_map_hdr->data,
		  act->chdr,
		  0,0,act_map_hdr->xsize,act_map_hdr->ysize,
		  act_map_hdr->xsize,act_map_hdr->ysize);
	  }
	}
	/* fall through */
      case ACT_DISP:
      {
        ACT_IMAGE_HDR *act_im_hdr;

	act_im_hdr = (ACT_IMAGE_HDR *) malloc( sizeof(ACT_IMAGE_HDR) );
	if (act_im_hdr == 0) TheEnd1("ACT_Make_Images: malloc err 1\n");
	act_map_hdr = (ACT_MAPPED_HDR *) act->data;
	line_size = X11_Get_Line_Size(act_map_hdr->xsize);
	if (x11_display_type == XA_MONOCHROME) pixmap_type = XYPixmap;
	else pixmap_type = ZPixmap;

	if (x11_pack_flag == xaTRUE) /* PACK */
	{
          UTIL_Pack_Image(act_map_hdr->data,act_map_hdr->data,
			  act_map_hdr->xsize,act_map_hdr->ysize);
	  if (x11_bits_per_pixel==2) line_size = (line_size + 3) / 4;
          else if (x11_bits_per_pixel==4) line_size = (line_size + 1) / 2;
	}

	{
	  image = XCreateImage(theDisp, theVisual,
		x11_depth,pixmap_type,0,NULL,
		act_map_hdr->xsize, act_map_hdr->ysize,
		x11_bitmap_pad, line_size);
	  if (image == 0) TheEnd1("ACT_Make_Images: XCreateImage err\n");
	  image->data = (char *)act_map_hdr->data;
	}
	act->type = ACT_IMAGE;
	act->data = (xaUBYTE *) act_im_hdr;
	act_im_hdr->flag = ACT_BUFF_VALID;
	act_im_hdr->xpos = act_map_hdr->xpos;
	act_im_hdr->ypos = act_map_hdr->ypos;
	act_im_hdr->xsize = act_map_hdr->xsize;
	act_im_hdr->ysize = act_map_hdr->ysize;
	act_im_hdr->image = image;
	act_im_hdr->clip  = act_map_hdr->clip;
      }
      FREE(act_map_hdr,0x105); act_map_hdr = 0;
      break;
    } /* end of switch */
    act = act->next;
  } /* end of while act */
}

void ACT_Setup_Loop(begin_act,end_act,count,begin_frame,end_frame)
XA_ACTION *begin_act,*end_act;
xaULONG count,begin_frame,end_frame;
{
  ACT_BEG_LP_HDR *beg_loop;
  ACT_END_LP_HDR *end_loop;
 
  begin_act->type = ACT_BEG_LP;
  beg_loop = (ACT_BEG_LP_HDR *)malloc(sizeof(ACT_BEG_LP_HDR));
  if (beg_loop == 0) TheEnd1("ACT_Setup_Loop: malloc err0");
  begin_act->data = (xaUBYTE *)beg_loop;
  beg_loop->count = count;
  beg_loop->cnt_var = 0;
  beg_loop->end_frame = end_frame;

  end_act->type = ACT_END_LP;
  end_loop = (ACT_END_LP_HDR *)malloc(sizeof(ACT_END_LP_HDR));
  if (end_loop == 0) TheEnd1("ACT_Setup_Loop: malloc err1");
  end_act->data = (xaUBYTE *)end_loop;
  end_loop->count = &(beg_loop->count);
  end_loop->cnt_var = &(beg_loop->cnt_var);
  end_loop->begin_frame = begin_frame;
  end_loop->end_frame = &(beg_loop->end_frame);
  end_loop->prev_end_act = 0;
}

void ACT_Free_Act(act)
XA_ACTION *act;
{
  if (act)
  {
    switch(act->type)
    {
      case ACT_IMAGE:
        { ACT_IMAGE_HDR *act_im_hdr;
          act_im_hdr = (ACT_IMAGE_HDR *)(act->data);
          if (act_im_hdr->flag & ACT_BUFF_VALID) 
	  {
            if (act_im_hdr->image) 
		{ XDestroyImage(act_im_hdr->image); act_im_hdr->image = 0; }
            if (act_im_hdr->clip) 
		{ FREE(act_im_hdr->clip,0x199); act_im_hdr->clip = 0; }
            XSync(theDisp,False);
	  }
          FREE(act->data,0x108); act->data = 0;
        }
        break;

      case ACT_PIXMAP:
        { ACT_PIXMAP_HDR *act_pm_hdr;
          act_pm_hdr = (ACT_PIXMAP_HDR *)(act->data);
          if (act_pm_hdr->flag & ACT_BUFF_VALID) 
	  {
            if (act_pm_hdr->pixmap) XFreePixmap(theDisp,act_pm_hdr->pixmap);
            if (act_pm_hdr->clip)   XFreePixmap(theDisp,act_pm_hdr->clip);
            XSync(theDisp,False);
	  }
          FREE(act->data,0x109); act->data = 0;
          DEBUG_LEVEL2 fprintf(stderr,"  freed ACT_PIXMAP\n");
        }
        break;

      /*POD NOTE: merge this to default case. Used to be separate for a reason,
        but no longer. */
      case ACT_SETTER:
      case ACT_IMAGES:
      case ACT_PIXMAPS:
        { ACT_SETTER_HDR *act_pms_hdr;
          act_pms_hdr = (ACT_SETTER_HDR *)(act->data);
          FREE(act_pms_hdr,0x10a);
        }
        break;
      default:
        if (act->data != 0) { FREE(act->data,0x10b); act->data = 0; }
        break;
    } /* end of switch */
    act->data = 0;
    act->type = ACT_NOP;
  }
}

/****************************
 *
 *
 *  xin is an open XA_INPUT handle to the animation file. It MUST be
 *  returned in the same file position it was passed in as.
 ********/
void ACT_Setup_Delta(anim,act,dlta_hdr,xin)
XA_ANIM_SETUP *anim;
XA_ACTION *act;
ACT_DLTA_HDR *dlta_hdr;
XA_INPUT *xin;
{ XA_DEC_INFO dec_info;
  XA_DEC2_INFO dec2_info;
  dec2_info.imagex    = dec_info.imagex    = anim->imagex;
  dec2_info.imagey    = dec_info.imagey    = anim->imagey;
  dec2_info.imaged    = dec_info.imaged    = anim->depth;
  dec2_info.chdr      = dec_info.chdr      = 0;
  dec2_info.map_flag  = dec_info.map_flag  = xaFALSE;
  dec2_info.map       = dec_info.map       = 0;
  dec2_info.special   = dec_info.special   = 0;
  dec2_info.extra     = dec_info.extra     = dlta_hdr->extra;
  dec2_info.skip_flag = dec_info.skip_flag = 0;
  dec2_info.bytes_pixel = x11_bytes_pixel;

  if (act->type == ACT_NOP)
  {
    return;
  }
  else if (xa_buffer_flag == xaTRUE)
  { xaULONG xpos,ypos,xsize,ysize,dlta_flag,free_pic_flag;
    xaUBYTE *the_pic = anim->pic;

    if (the_pic) free_pic_flag = xaFALSE;
    else
    { xaULONG psize = anim->imagex * anim->imagey;
      if ((cmap_true_map_flag == xaTRUE) && (anim->depth > 8))
		the_pic = (xaUBYTE *) malloc( 3 * psize );
      else	the_pic = (xaUBYTE *) malloc(XA_PIC_SIZE(psize));
      if (the_pic == 0) TheEnd1("ACT_Setup_Delta: malloc err.");
      free_pic_flag = xaTRUE;
    }
    if ( (cmap_true_map_flag==xaFALSE) || (anim->depth <= 8) )
    { 

      dec2_info.map_flag = dec_info.map_flag
			= (x11_display_type & XA_X11_TRUE)?(xaTRUE):(xaFALSE); 
      dec2_info.map = dec_info.map	= anim->chdr->map;

      if (dlta_hdr->xapi_rev == 0x0002)
      {

        dec2_info.image_type = XA_Get_Image_Type( dlta_hdr->special,
				0,dec2_info.map_flag);

        dlta_flag = dlta_hdr->delta(the_pic,dlta_hdr->data,
			dlta_hdr->fsize,&dec2_info);
        if (!(dlta_flag & ACT_DLTA_MAPD)) dec2_info.map_flag = xaFALSE;
        xpos  = dec2_info.xs;	ypos  = dec2_info.ys;
        xsize = dec2_info.xe;	ysize = dec2_info.ye;
      }
      else /* if (dlta_hdr->xapi_rev == 0x0001) */
      {
        dlta_flag = dlta_hdr->delta(the_pic,dlta_hdr->data,
			dlta_hdr->fsize,&dec_info);
        if (!(dlta_flag & ACT_DLTA_MAPD)) dec_info.map_flag = xaFALSE;
        xpos  = dec_info.xs;	ypos  = dec_info.ys;
        xsize = dec_info.xe;	ysize = dec_info.ye;
      }


      xsize -= xpos; ysize -= ypos;
      FREE(dlta_hdr,0xA001); act->data = 0; dlta_hdr = 0;
      if (dlta_flag & ACT_DLTA_NOP) 
      { act->type = ACT_NOP;
        if ((free_pic_flag==xaTRUE) && the_pic) {free(the_pic); the_pic = 0;}
      }
      else ACT_Setup_Mapped(act,the_pic,anim->chdr,
			xpos,ypos,xsize,ysize,anim->imagex,anim->imagey,
			xaFALSE,0,free_pic_flag,xaTRUE,dec_info.map_flag);
      ACT_Add_CHDR_To_Action(act,anim->chdr);
    } 
    else /* decode as RGB triplets and then convert to mapped image */
    { xaUBYTE *tpic = 0;
      dec2_info.special = dec_info.special = 1;
      if (dlta_hdr->xapi_rev == 0x0002)
      { dec2_info.image_type = XA_Get_Image_Type( dlta_hdr->special,
                                           0,dec2_info.map_flag);
        dlta_flag = dlta_hdr->delta(the_pic,dlta_hdr->data,
			dlta_hdr->fsize,&dec2_info);
      }
      else /* if (dlta_hdr->xapi_rev == 0x0001) */
      { dlta_flag = dlta_hdr->delta(the_pic,dlta_hdr->data,
			dlta_hdr->fsize,&dec_info);
      }
      /*POD NOTE: need to add subimage support to RGB conversion utils */
      /* until then ignore optimization and force full image */
      /* xsize -= xpos; ysize -= ypos; */
      xpos = ypos = 0; xsize = anim->imagex; ysize = anim->imagey;
      FREE(dlta_hdr,0xA002); act->data = 0; dlta_hdr = 0;
      if (dlta_flag & ACT_DLTA_NOP)
      { act->type = ACT_NOP;
        if ((free_pic_flag==xaTRUE) && the_pic) {free(the_pic); the_pic = 0;}
      }
      else
      {
	if (    (cmap_true_to_all == xaTRUE)
	     || ((cmap_true_to_1st == xaTRUE) && (anim->chdr == 0) )
	   )  anim->chdr = CMAP_Create_CHDR_From_True(the_pic,8,8,8,
		anim->imagex,anim->imagey,anim->cmap,&(anim->imagec));
	else if ( (cmap_true_to_332 == xaTRUE) && (anim->chdr == 0) )
		anim->chdr = CMAP_Create_332(anim->cmap,&(anim->imagec));
	else if ( (cmap_true_to_gray == xaTRUE) && (anim->chdr == 0) )
		anim->chdr = CMAP_Create_Gray(anim->cmap,&(anim->imagec));
		 
	if (cmap_dither_type == CMAP_DITHER_FLOYD)
			tpic = UTIL_RGB_To_FS_Map(0,the_pic,anim->chdr,
				anim->imagex,anim->imagey,free_pic_flag);
	else tpic = UTIL_RGB_To_Map(0,the_pic,anim->chdr,
				anim->imagex,anim->imagey,free_pic_flag);
	ACT_Setup_Mapped(act,tpic,anim->chdr,xpos,ypos,xsize,ysize,
			anim->imagex,anim->imagey,xaFALSE,0,xaTRUE,xaTRUE,xaFALSE);
	if (anim->chdr) ACT_Add_CHDR_To_Action(act,anim->chdr);
      } /* end of not NOP */
    } /* end of true_map */
  } /* end of buffer */
  else /* not buffered */
  {
    /* Also make sure not xaTRUE, is 332 and special case file_flag */
    if  ( (cmap_color_func != 0)
            && (anim->depth > 8) && (anim->depth <= 32) /* QT gray depths */ 
	    && (!(x11_display_type & XA_X11_TRUE))
	)
    {
      if (anim->cmap_flag==0)
      { xaULONG dlta_flag,psize;
	xaUBYTE *cbuf,*data;

	psize = anim->imagex * anim->imagey;
	cbuf = (xaUBYTE *) malloc(3 * psize);
	if (cbuf == 0) TheEnd1("colorfunc1 malloc err0\n");
	memset((char *)(cbuf),0x00,(3 * psize) );

	if ((xa_file_flag == xaTRUE) && (dlta_hdr->fsize != 0))
	{ xaULONG pos;
	  data = (xaUBYTE *)malloc(dlta_hdr->fsize);
	  if (data==0) TheEnd1("colorfunc1 malloc err1\n");
	  pos = xin->Get_FPos(xin);
	  xin->Seek_FPos(xin,dlta_hdr->fpos,0); /* save file pos */
	  xin->Read_Block(xin,data,dlta_hdr->fsize); /* read data*/
	  xin->Seek_FPos(xin,pos,0); /* restore file pos */
	} else data = dlta_hdr->data;
	dec2_info.special = dec_info.special   = 1;
	if (dlta_hdr->xapi_rev == 0x0002)
        { dec2_info.image_type = XA_Get_Image_Type(dlta_hdr->special,
                                           0,dec2_info.map_flag);
	  dlta_flag = dlta_hdr->delta(cbuf,data,dlta_hdr->fsize,&dec2_info);
        }
        else /* if (dlta_hdr->xapi_rev == 0x0001) */
	  dlta_flag = dlta_hdr->delta(cbuf,data,dlta_hdr->fsize,&dec_info);
	if ((xa_file_flag == xaTRUE) && (dlta_hdr->fsize != 0)) 
					{ FREE(data,0xA004); data = 0; }
	switch(cmap_color_func)
	{
	  case 4:
	  {
	    anim->chdr = CMAP_Create_CHDR_From_True(cbuf,8,8,8,
			 anim->imagex,anim->imagey,anim->cmap,&(anim->imagec));
	    DEBUG_LEVEL1 fprintf(stderr,"CF4: csize = %d\n",anim->chdr->csize);
	    anim->color_cnt += anim->chdr->csize;

DEBUG_LEVEL1 fprintf(stderr,"Attempt: color_cnt %d retries %d \n",
					anim->color_cnt, anim->color_retries);

	    if (anim->color_cnt > 64)
			{ anim->cmap_flag = 1; anim->color_cnt = 0; 
			  anim->color_retries = 0; }
            else
	    {  anim->color_retries++;
	       if (anim->color_retries > 3)
			{ anim->cmap_flag = 1; anim->color_cnt = 0; 
			  anim->color_retries = 0; }
	    }
	    if (cbuf) FREE(cbuf,0xA005); cbuf = 0;
	  }
	  break;
	} /* end switch */
      } /* first time through */
      else  /* else cnt til next time */
      { 
DEBUG_LEVEL1 fprintf(stderr,"Attempt: cmap_cnt %d fm_num %d sam_cnt %d\n",
			anim->cmap_cnt, anim->cmap_frame_num, cmap_sample_cnt);
	anim->cmap_cnt++;
	if (cmap_sample_cnt && (anim->cmap_cnt >= anim->cmap_frame_num))
		{ anim->cmap_flag = 0; anim->cmap_cnt = anim->color_cnt = 0; }
      }
    } /*color func, true color anim and not TrueDisplay  */
    if (anim->chdr) ACT_Add_CHDR_To_Action(act,anim->chdr);
  } /* not bufferd */
}

