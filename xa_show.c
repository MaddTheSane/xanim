
/*
 * xa_show.c
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
 */

/* Revisions:
 * 13Nov94:  wasn't properly skipping video frames in certain instances.
 * 09Sep97:  Eric Shaw sent mods to tile video to root display. Very Cool.
 * 14Dec97:  Adam Moss: XA_SHOW_IMAGE routine wasn't freeing PixMap when
 *           scaling images with clip masks.
 */

#include "xanim.h"
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>

#ifdef XSHM
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
extern Visual        *theVisual;  /* POD NOTE: needed for XMBUF?? */
#endif /*XSHM*/

#ifdef XMBUF
#include <X11/extensions/multibuf.h>
#endif

#include "xa_x11.h"

#define AUD_SYNC_CHECK() { XSync(theDisp,False); }

void XA_SHOW_IMAGE();
void XA_SHOW_PIXMAP();
void XA_SHOW_IMAGES();
void XA_SHOW_PIXMAPS();
void XA_SHOW_DELTA();

xaUBYTE *UTIL_Scale_Bitmap();
xaUBYTE *UTIL_Scale_Mapped();
void UTIL_Pack_Image();
void UTIL_Mapped_To_Mapped();
void UTIL_Mapped_To_Bitmap();
void UTIL_Mapped_To_Floyd();
xaULONG XA_Image_To_Pixmap();
xaULONG XA_Read_Delta();
void X11_Init_Image_Struct();
void XA_Install_CMAP();
void IFF_Init_DLTA_HDR();
void IFF_Update_DLTA_HDR();
void IFF_Buffer_HAM6();
void IFF_Buffer_HAM8();
extern xaULONG XA_Get_Image_Type();


extern xaLONG  xa_frames_skipd, xa_frames_Sskipd;
extern xaULONG xa_need_to_scale_b;
extern xaULONG xa_need_to_scale_u;
extern xaULONG xa_buff_x, xa_buff_y;
extern xaULONG xa_disp_x, xa_disp_y;
extern char *im_buff0,*im_buff1,*im_buff2,*im_buff3;
extern char *xa_pic,*xa_disp_buff,*xa_scale_buff;
extern xaULONG xa_disp_buff_size,xa_scale_buff_size;
extern xaLONG xa_skip_flag;
extern xaLONG xa_skip_video;
extern xaLONG xa_skip_cnt;
#define XA_SKIP_MAX 10  /* don't skip more than 5 frames in a row */
extern xaULONG xa_image_size;
extern xaULONG xa_imagex;
extern xaULONG xa_imagey;
extern xaULONG xa_imaged;
extern xaLONG xa_pixmap_flag;
extern xaLONG xa_dither_flag;
extern int xa_vid_fd;
extern xaUBYTE *xa_vidcodec_buf;
extern xaULONG x11_expose_flag;
extern xaLONG xa_anim_flags;
extern xaLONG xa_no_disp;

extern xaLONG xa_cycle_cnt;
extern xaLONG xa_now_cycling;
extern void XA_Cycle_It();

extern xaULONG shm;
#ifdef XSHM
extern XShmSegmentInfo im0_shminfo;
extern XShmSegmentInfo im1_shminfo;
extern XShmSegmentInfo im2_shminfo;
extern XImage *im0_Image;
extern XImage *im1_Image;
extern XImage *im2_Image;
extern XImage *sh_Image;
#endif

extern xaULONG mbuf;

/* SMR 13 */
extern xaLONG xa_window_x;
extern xaLONG xa_window_y;
extern xaLONG xa_window_center_flag;
extern xaLONG xa_max_imagex;
extern xaLONG xa_max_imagey;
extern float xa_scalex;
extern float xa_scaley;

/* override XPutImage call to support playing at a particular
   position in a window.  Uses #define kludge to avoid changing
   lots of code */
void xa_putimage(dpy, d, gc, im, sx, sy, dx, dy, w, h)
Display *dpy;
Drawable d;
GC gc;
XImage *im;
int sx, sy, dx, dy;
unsigned int w;
unsigned int h;
{ 
  /* variables used for tiling */
  XWindowAttributes	rootwindowattributes;
  Window	root_return;
  int		x_return,y_return;
  unsigned int	width_return,height_return, border_width_return,depth_return;
  int rowcount;
  int colcount;
  int rowindex;
  int colindex;

  if (d == mainW) 
  { dx += xa_window_x;
    dy += xa_window_y;
    if (xa_window_center_flag == xaTRUE)
    { dx -= (int)((double)xa_max_imagex * xa_scalex / 2.0);
      dy -= (int)((double)xa_max_imagey * xa_scaley / 2.0);
    }
  }
  XPutImage(dpy, d, gc, im, sx, sy, dx, dy, w, h);

  /* stuff for tiling starts here */
  if(xa_root == xaTRUE)
  {
    XGetGeometry(dpy,d,&root_return,&x_return,&y_return,&width_return,
                     &height_return,&border_width_return,&depth_return);
    XGetWindowAttributes(theDisp,root_return,&rootwindowattributes);
    w += 2*dx;
    h += 2*dy;
    colcount=rootwindowattributes.width/w + 1;
    rowcount=rootwindowattributes.height/h + 1;
    for(rowindex=0;rowindex<rowcount;rowindex++)
      for(colindex=0;colindex<colcount;colindex++)
        XPutImage(theDisp,root_return,theGC,im,sx,sy,
           w*colindex+dx,h*rowindex+dy,w-2*dx,h-2*dy);
  }
}
#define XPutImage xa_putimage

#ifdef XSHM
void xa_shmputimage(dpy, d, gc, im, sx, sy, dx, dy, w, h, event)
Display *dpy;
Drawable d;
GC gc;
XImage *im;
int sx, sy, dx, dy;
unsigned int w, h;
int event;
{ 
  /* variables used for tiling */
  XWindowAttributes rootwindowattributes;
  Window root_return;
  int x_return,y_return;
  unsigned int width_return,height_return,
     border_width_return,depth_return;
  int rowcount;
  int colcount;
  int rowindex;
  int colindex;

  if (d == mainW) 
  { dx += xa_window_x;
    dy += xa_window_y;
    if (xa_window_center_flag == xaTRUE)
    { dx -= (int)((double)xa_max_imagex * xa_scalex / 2.0);
      dy -= (int)((double)xa_max_imagey * xa_scaley / 2.0);
    }
  }
  XShmPutImage(dpy, d, gc, im, sx, sy, dx, dy, w, h, False);

  /* stuff for tiling starts here */
  if (xa_root == xaTRUE)
  {
    XGetGeometry(dpy,d,&root_return,&x_return,&y_return,&width_return,
                     &height_return,&border_width_return,&depth_return);
    XGetWindowAttributes(theDisp,root_return,&rootwindowattributes);
    w += 2*dx; h+= 2*dy;
    colcount=rootwindowattributes.width/w + 1;
    rowcount=rootwindowattributes.height/h + 1;
    for(rowindex=0;rowindex<rowcount;rowindex++)
      for(colindex=0;colindex<colcount;colindex++)
        XShmPutImage(theDisp,root_return,theGC,im,sx,sy,
           w*colindex+dx,h*rowindex+dy,w-2*dx,h-2*dy,False);
  }
}
#define XShmPutImage xa_shmputimage
#endif
/* end SMR 13 */


XA_DEC_INFO  xa_dec_info;
XA_DEC2_INFO xa_dec2_info;


/*POD clean this up */
void XA_Show_Action(act)
XA_ACTION *act;
{
 switch(act->type)
 {
       /* 
        * NOP and DELAY don't change anything but can have timing info
        * that might prove useful. ie dramatic pauses :^)
        */
    case ACT_NOP:      
	DEBUG_LEVEL2 fprintf(stderr,"ACT_NOP:\n");
	break;
    case ACT_DELAY:    
	DEBUG_LEVEL2 fprintf(stderr,"ACT_DELAY:\n");
	break;
       /* 
        * Change Color Map.
        */
     case ACT_CMAP:     
	if (   (cmap_play_nice == xaFALSE) 
	    && (!(x11_display_type & XA_X11_STATIC)) )
	{ ColorReg *tcmap;
	  xaLONG j;
	  ACT_CMAP_HDR *cmap_hdr;

	  cmap_hdr	= (ACT_CMAP_HDR *)act->data;
	  xa_cmap_size	= cmap_hdr->csize;
	  if (xa_cmap_size > x11_cmap_size) xa_cmap_size = x11_cmap_size;
	  xa_cmap_off	= cmap_hdr->coff;
	  tcmap		= (ColorReg *)cmap_hdr->data;
          for(j=0; j<xa_cmap_size;j++)
	  {
		xa_cmap[j].red   = tcmap[j].red;
		xa_cmap[j].green = tcmap[j].green;
		xa_cmap[j].blue  = tcmap[j].blue;
		xa_cmap[j].gray  = tcmap[j].gray;
	  }
		/* POD TEMP share routine whith CHDR install */
	  if (x11_cmap_flag == xaTRUE)
	  {
	    DEBUG_LEVEL2 fprintf(stderr,"CMAP: size=%d off=%d\n",
                	xa_cmap_size,xa_cmap_off);
	    if (x11_display_type & XA_X11_GRAY)
	    {
	      for(j=0;j<xa_cmap_size;j++)
	      {
	        defs[j].pixel = xa_cmap_off + j;
	        defs[j].red   = xa_cmap[j].gray;
	        defs[j].green = xa_cmap[j].gray;
	        defs[j].blue  = xa_cmap[j].gray;
	        defs[j].flags = DoRed | DoGreen | DoBlue;
	      }
	    }
	    else
	    {
	      for(j=0; j<xa_cmap_size;j++)
	      {
	        defs[j].pixel = xa_cmap_off + j;
	        defs[j].red   = xa_cmap[j].red;
	        defs[j].green = xa_cmap[j].green;
	        defs[j].blue  = xa_cmap[j].blue;
	        defs[j].flags = DoRed | DoGreen | DoBlue;
	        DEBUG_LEVEL3 fprintf(stderr," %d) %x %x %x <%d>\n",
				j,xa_cmap[j].red,xa_cmap[j].green,
				xa_cmap[j].blue,xa_cmap[j].gray);
	      }
	    }
	    XStoreColors(theDisp,theCmap,defs,xa_cmap_size);
	  }
	}
	break;
       /* 
        * Lower Color Intensity by 1/16.
        */
     case ACT_FADE:     
	{ xaLONG j;
	  DEBUG_LEVEL2 fprintf(stderr,"ACT_FADE:\n");
	  if ( (x11_display_type & XA_X11_CMAP) && (x11_cmap_flag == xaTRUE) )
	  {
	    for(j=0;j<xa_cmap_size;j++)
	    {
	      if (xa_cmap[j].red   <= 16) xa_cmap[j].red   = 0;
	      else xa_cmap[j].red   -= 16;
	      if (xa_cmap[j].green <= 16) xa_cmap[j].green = 0;
	      else xa_cmap[j].green -= 16;
	      if (xa_cmap[j].blue  <= 16) xa_cmap[j].blue  = 0;
	      else xa_cmap[j].blue  -= 16;

	      defs[j].pixel = j;
	      defs[j].red   = xa_cmap[j].red   << 8;
	      defs[j].green = xa_cmap[j].green << 8;
	      defs[j].blue  = xa_cmap[j].blue  << 8;
	      defs[j].flags = DoRed | DoGreen | DoBlue;
	    }
	    XStoreColors(theDisp,theCmap,defs,xa_cmap_size);
	    XFlush(theDisp);
	  }
	}
	break;

     case ACT_APTR:   
	{ ACT_APTR_HDR *aptr_hdr = (ACT_APTR_HDR *)act->data;
    	  if (aptr_hdr->act->type == ACT_IMAGE)
		XA_SHOW_IMAGE(aptr_hdr->act,aptr_hdr->xpos,aptr_hdr->ypos,
				aptr_hdr->xsize,aptr_hdr->ysize,1);
    	  else if (aptr_hdr->act->type == ACT_PIXMAP)
		XA_SHOW_PIXMAP(aptr_hdr->act,aptr_hdr->xpos,aptr_hdr->ypos,
				aptr_hdr->xsize,aptr_hdr->ysize,1);
	  else
	  {
  	    fprintf(stderr,"ACT_APTR: error invalid action %x\n",act->type);
	  }
	}
	break;

     case ACT_IMAGE:   
	XA_SHOW_IMAGE(act,0,0,0,0,0);
	break;

     case ACT_PIXMAP:   
	XA_SHOW_PIXMAP(act,0,0,0,0,0);
	break;

     case ACT_IMAGES:
	XA_SHOW_IMAGES(act);
	break;

     case ACT_PIXMAPS:
	XA_SHOW_PIXMAPS(act);
	break;

       /* Act upon IFF Color Cycling chunk.  */
     case ACT_CYCLE:
	{
	  /* if there is a new_chdr, install it, not the old one */
	  /*
 	   * XA_CHDR *the_chdr;
	   * if (act->chdr->new_chdr) the_chdr = act->chdr->new_chdr;
	   * else the_chdr = act->chdr;
	   */

	  if (   (cmap_play_nice == xaFALSE) 
	     && (x11_display_type & XA_X11_CMAP)
	     && (xa_anim_flags & ANIM_CYCLE) )
	  {
	    ACT_CYCLE_HDR *act_cycle;
	    act_cycle = (ACT_CYCLE_HDR *)act->data;

	    DEBUG_LEVEL2 fprintf(stderr,"ACT_CYCLE:\n");
	    if ( !(act_cycle->flags & ACT_CYCLE_STARTED) )
	    {
	      if ( (act->chdr != 0) && (act->chdr != xa_chdr_now) )
			XA_Install_CMAP(act->chdr);
	      xa_cycle_cnt++;
	      act_cycle->flags |= ACT_CYCLE_STARTED;
	      xa_now_cycling = xaTRUE;
	      XtAppAddTimeOut(theContext,(int)(act_cycle->rate), 
		(XtTimerCallbackProc)XA_Cycle_It, (XtPointer)(act_cycle));
	    }
	  }
	} /*POD*/
	break;

     case ACT_DELTA:        
	XA_SHOW_DELTA(act);
	break;

     default:	fprintf(stderr,"Unknown not supported %x\n",act->type);
  } /* end of switch of action type */
}


/***************************************************** 
 * XA_SHOW_IMAGE
 *
 *****************************************************/
void XA_SHOW_IMAGE(act,im_xpos,im_ypos,im_xsize,im_ysize,flag)
XA_ACTION *act;
xaULONG im_xpos,im_ypos;
xaULONG im_xsize,im_ysize;
xaULONG flag;		/* override flag 0 normal 1 use pos/size */
{
  Pixmap pix_map = 0;
  ACT_IMAGE_HDR *act_im_hdr;

  DEBUG_LEVEL2 fprintf(stderr,"ACT_IMAGE:\n");

  act_im_hdr = (ACT_IMAGE_HDR *)(act->data);
  if (flag==0)
  { im_xpos = act_im_hdr->xpos;
    im_ypos = act_im_hdr->ypos;
    im_xsize = act_im_hdr->xsize;
    im_ysize = act_im_hdr->ysize;
  }

  if (xa_need_to_scale_b)
  { xaUBYTE *tmp_pic;
    xaULONG xp,yp,xsize,ysize;

    if(act_im_hdr->clip)
    {
      xsize = im_xsize;	ysize = im_ysize;
      xp = im_xpos;	yp = im_ypos;
      tmp_pic = UTIL_Scale_Bitmap(0,act_im_hdr->clip,im_xsize,im_ysize,
		(X11_Get_Bitmap_Width(xsize)/8),xa_buff_x,xa_buff_y,
		xa_disp_x,xa_disp_y,&xp,&yp,&xsize,&ysize,
		XA_LSBIT_1ST,XA_LSBIT_1ST);
      if (tmp_pic)
      {
        pix_map = XCreatePixmapFromBitmapData(theDisp,mainW,(char *)tmp_pic,
		 	 X11_Get_Bitmap_Width(xsize),ysize,0x01,0x00,1);
        XSetClipMask(theDisp,theGC,pix_map);
        XSetClipOrigin(theDisp,theGC,xp,yp);
      }
      else pix_map = 0;
    }

    xp = im_xpos;	yp = im_ypos;
    if (x11_display_type == XA_MONOCHROME)
      tmp_pic = UTIL_Scale_Bitmap(0,act_im_hdr->image->data,
			im_xsize,im_ysize, act_im_hdr->image->bytes_per_line,
			xa_buff_x,xa_buff_y, xa_disp_x,xa_disp_y,
			&xp,&yp,&xsize,&ysize,x11_bit_order,x11_bit_order);
    else	tmp_pic = UTIL_Scale_Mapped(0,act_im_hdr->image->data,
		   0,0, im_xsize, im_ysize, act_im_hdr->image->bytes_per_line,
		   xa_buff_x,xa_buff_y,xa_disp_x,xa_disp_y,
		   x11_bytes_pixel,&xp,&yp,&xsize,&ysize,0);
    if (tmp_pic)
    {
      theImage->data = (char *)tmp_pic;
		
      X11_Init_Image_Struct(theImage,xsize,ysize);
      if ( (act->chdr != 0) && (act->chdr != xa_chdr_now) )
		XA_Install_CMAP(act->chdr);
      XPutImage(theDisp,mainW,theGC,theImage,0,0,xp,yp,xsize,ysize);
    }
    if(act_im_hdr->clip)
    { XSetClipMask(theDisp,theGC,None);
      if (pix_map) XFreePixmap(theDisp,pix_map);
    }
  }
  else /* Not scaling Image */
  {
    if(act_im_hdr->clip)
    {
      pix_map = XCreatePixmapFromBitmapData(theDisp,mainW,
                (char *)act_im_hdr->clip,
                X11_Get_Bitmap_Width(im_xsize),im_ysize,0x01,0x00,1);
      XSetClipMask(theDisp,theGC,pix_map);
      XSetClipOrigin(theDisp,theGC,im_xpos,im_ypos);
    }
    if ( (act->chdr != 0) && (act->chdr != xa_chdr_now) )
		XA_Install_CMAP(act->chdr);
    XPutImage(theDisp,mainW,theGC,act_im_hdr->image, 0, 0,
		im_xpos,  im_ypos, im_xsize, im_ysize  );
    if (act_im_hdr->clip)
    {
      XSetClipMask(theDisp,theGC,None);
      XFreePixmap(theDisp,pix_map);
    }
  }
  AUD_SYNC_CHECK();
}


/***************************************************** 
 * XA_SHOW_PIXMAP
 *
 *****************************************************/
void XA_SHOW_PIXMAP(act,pm_xpos,pm_ypos,pm_xsize,pm_ysize,flag)
XA_ACTION *act;
xaULONG pm_xpos,pm_ypos;
xaULONG pm_xsize,pm_ysize;
xaULONG flag;		/* 0 normal 1 use pos/size */
{
  ACT_PIXMAP_HDR *act_pm_hdr;

  DEBUG_LEVEL2 fprintf(stderr,"ACT_PIXMAP:\n");
  act_pm_hdr = (ACT_PIXMAP_HDR *)(act->data);
  if (flag==0)
  { pm_xpos = act_pm_hdr->xpos;
    pm_ypos = act_pm_hdr->ypos;
    pm_xsize = act_pm_hdr->xsize;
    pm_ysize = act_pm_hdr->ysize;
  }
  if (xa_need_to_scale_b)
  {
    Pixmap pix_map;
    XImage *t_image,*p_image;
    xaUBYTE *tmp_pic;
    xaULONG xp,yp,xsize,ysize;

    if (act_pm_hdr->clip)
    {
      xp = pm_xpos;      yp = pm_ypos;
      xsize = pm_xsize;  ysize = pm_ysize;
      p_image = XGetImage(theDisp,act_pm_hdr->clip,0,0,xsize,ysize,1,XYPixmap);
      tmp_pic = UTIL_Scale_Bitmap(0,p_image->data, xsize,ysize,
		p_image->bytes_per_line,xa_buff_x,xa_buff_y,
                xa_disp_x,xa_disp_y,&xp,&yp,&xsize,&ysize,
					x11_bit_order,XA_LSBIT_1ST);
      if (tmp_pic)
      {
        pix_map = XCreatePixmapFromBitmapData(theDisp,mainW,
				(char *)tmp_pic, X11_Get_Bitmap_Width(xsize),
				ysize,0x01,0x00,1);
        XSetClipMask(theDisp,theGC,pix_map);
        XSetClipOrigin(theDisp,theGC,xp,yp);
      } else pix_map = 0;
    }

    xp = pm_xpos;	yp = pm_ypos;
    xsize = pm_xsize;	ysize = pm_ysize;
    t_image = XGetImage(theDisp,act_pm_hdr->pixmap,0,0,xsize,ysize,
				AllPlanes,ZPixmap);
    if (x11_display_type == XA_MONOCHROME)
    {
      t_image = XGetImage(theDisp,act_pm_hdr->pixmap,0,0,xsize,ysize,
				1,XYPixmap);
      tmp_pic = UTIL_Scale_Bitmap(0,t_image->data,
                xsize,ysize,t_image->bytes_per_line,xa_buff_x,xa_buff_y,
                xa_disp_x,xa_disp_y,&xp,&yp,&xsize,&ysize,
					x11_bit_order,x11_bit_order);
    }
    else
    {
      t_image = XGetImage(theDisp,act_pm_hdr->pixmap,0,0,xsize,ysize,
				AllPlanes,ZPixmap);
      tmp_pic = UTIL_Scale_Mapped(0,t_image->data,
                   0,0, xsize, ysize, t_image->bytes_per_line,
                   xa_buff_x,xa_buff_y,xa_disp_x,xa_disp_y,
                   x11_bytes_pixel,&xp,&yp,&xsize,&ysize,0);
    }
    if (tmp_pic) /* if image to draw */
    {
      theImage->data = (char *)tmp_pic;
      X11_Init_Image_Struct(theImage,xsize,ysize);
      if ( (act->chdr != 0) && (act->chdr != xa_chdr_now) )
		  XA_Install_CMAP(act->chdr);
      XPutImage(theDisp,mainW,theGC,theImage,0,0,xp,yp,xsize,ysize);
    }
    if (act_pm_hdr->clip) 
    {
      XSetClipMask(theDisp,theGC,None);
      XDestroyImage(p_image);
      if (pix_map) XFreePixmap(theDisp,pix_map);
    }
    XDestroyImage(t_image);
    AUD_SYNC_CHECK();
  }
  else /* no rescale */
  {
    if (act_pm_hdr->clip)
    {
      XSetClipMask(theDisp,theGC,act_pm_hdr->clip);
      XSetClipOrigin(theDisp,theGC,pm_xpos,pm_ypos);
      XSetPlaneMask(theDisp,theGC,AllPlanes);
    }
    if ( (act->chdr != 0) && (act->chdr != xa_chdr_now) )
		XA_Install_CMAP(act->chdr);
    XCopyArea(theDisp,act_pm_hdr->pixmap,mainW,theGC, 0, 0, 
		pm_xsize, pm_ysize, pm_xpos,  pm_ypos   );
    AUD_SYNC_CHECK();
    if (act_pm_hdr->clip) XSetClipMask(theDisp,theGC,None);
  }
}

/***************************************************** 
 * XA_SHOW_IMAGES
 *
 *****************************************************/
void XA_SHOW_IMAGES(act)
XA_ACTION *act;
{
  ACT_SETTER_HDR *act_ims_hdr;
  ACT_PIXMAP_HDR *work_pm_hdr;
  Pixmap work;
  ACT_IMAGE_HDR *back_im_hdr;
  XImage *back;
  xaLONG xpback,ypback;

  DEBUG_LEVEL2 fprintf(stderr,"ACT_SETTERS: im\n");
  act_ims_hdr = (ACT_SETTER_HDR *)(act->data);

  /* work still needs to be PIXMAP */
  if (act_ims_hdr->work->type != ACT_PIXMAP)
		XA_Image_To_Pixmap(act_ims_hdr->work);
  work_pm_hdr = (ACT_PIXMAP_HDR *)act_ims_hdr->work->data;
  work = work_pm_hdr->pixmap;

  back_im_hdr = (ACT_IMAGE_HDR *)act_ims_hdr->back->data;
  back = back_im_hdr->image;

  /* copy backgrnd into work area */
  xpback = act_ims_hdr->xpback;
  ypback = act_ims_hdr->ypback;
  {
    xaLONG xback,yback;
    xaLONG xlen,ylen,xlen1,ylen1;

    xback = act_ims_hdr->xback;
    yback = act_ims_hdr->yback;
    xlen = xback - xpback;
    ylen = yback - ypback;
    if (xlen >= xa_buff_x) xlen1 = 0;
    else xlen1 = xa_buff_x - xlen;
    if (ylen >= xa_buff_y) ylen1 = 0;
    else ylen1 = xa_buff_y - ylen;

    if (xlen1 == 0)
    { 
      if (ylen1 == 0)
      {
        XPutImage(theDisp, work, theGC, back,
		      xpback,ypback,0,0,xa_buff_x,xa_buff_y );
      }
      else
      {
        XPutImage(theDisp, work, theGC, back,
		        xpback,ypback,0,   0, xa_buff_x,ylen);
        XPutImage(theDisp, work, theGC, back,
		      xpback,    0, 0,ylen, xa_buff_x,ylen1);
      }
    }
    else /* xlen1 != 0 */
    { 
      if (ylen1 == 0)
      {
        XPutImage(theDisp, work, theGC, back,
		      xpback,ypback,0,   0,xlen ,xa_buff_y);
        XPutImage(theDisp, work, theGC, back,
		          0,ypback,xlen, 0,xlen1,xa_buff_y);
      }
      else
      {
        XPutImage(theDisp, work, theGC, back,
		      xpback,ypback,    0,    0,  xlen,  ylen);
        XPutImage(theDisp, work, theGC, back,
		          0,    0, xlen, ylen, xlen1, ylen1);
        XPutImage(theDisp, work, theGC, back,
		          0,ypback, xlen,    0, xlen1,  ylen);
        XPutImage(theDisp, work, theGC, back,
		      xpback,    0,    0, ylen,  xlen, ylen1);
      }
    }
  }

  /* loop through face pixmaps */
  while(act_ims_hdr != 0)
  {
    ACT_IMAGE_HDR *face_im_hdr;
    Pixmap pix_map = 0;

    face_im_hdr = (ACT_IMAGE_HDR *)act_ims_hdr->face->data;
    if (face_im_hdr->clip)
    {
      pix_map = XCreatePixmapFromBitmapData(theDisp,mainW,
                (char *)face_im_hdr->clip,
                X11_Get_Bitmap_Width(face_im_hdr->xsize),face_im_hdr->ysize,
                0x01,0x00,1);
      XSetClipMask(theDisp,theGC,pix_map);
      XSetClipOrigin(theDisp,theGC,
		act_ims_hdr->xpface,act_ims_hdr->ypface);
    }
    XPutImage(theDisp, work, theGC,face_im_hdr->image,
		  0, 0, act_ims_hdr->xpface,  act_ims_hdr->ypface,
		        act_ims_hdr->xface, act_ims_hdr->yface    );

    if(face_im_hdr->clip)
    {
      XSetClipMask(theDisp,theGC,None);
      XFreePixmap(theDisp,pix_map);
    }
    act_ims_hdr = act_ims_hdr->next;
  }

  if (xa_need_to_scale_b)
  {
    XImage *t_image;
    xaULONG xp,yp,xsize,ysize;
    xaUBYTE *tmp_pic;
    xp = yp = 0; xsize = xa_buff_x; ysize = xa_buff_y;
    if (x11_display_type == XA_MONOCHROME)
    {
      t_image=XGetImage(theDisp,work,0,0,xsize,ysize,1,XYPixmap);
      tmp_pic = UTIL_Scale_Bitmap(0,t_image->data,
                xsize,ysize,t_image->bytes_per_line,xa_buff_x,xa_buff_y,
                xa_disp_x,xa_disp_y,&xp,&yp,&xsize,&ysize,
					x11_bit_order,x11_bit_order);
    }
    else
    {
      t_image=XGetImage(theDisp,work,0,0,xsize,ysize,AllPlanes,ZPixmap);
      tmp_pic = UTIL_Scale_Mapped(0,t_image->data,
                   0,0, xsize, ysize, t_image->bytes_per_line,
                   xa_buff_x,xa_buff_y,xa_disp_x,xa_disp_y,
                   x11_bytes_pixel,&xp,&yp,&xsize,&ysize,0);
    }
    if (tmp_pic)
    {
      theImage->data = (char *)tmp_pic;
      X11_Init_Image_Struct(theImage,xsize,ysize);
      if ( (act->chdr != 0) && (act->chdr != xa_chdr_now) )
                  XA_Install_CMAP(act->chdr);
      XPutImage(theDisp,mainW,theGC,theImage,0,0,xp,yp,xsize,ysize);
    }
    XDestroyImage(t_image);
  }
  else /* not scaling */
  {
    if ( (act->chdr != 0) && (act->chdr != xa_chdr_now) )
		XA_Install_CMAP(act->chdr);
    XCopyArea(theDisp,work,mainW,theGC,0,0,xa_disp_x,xa_disp_y,0, 0);
  }
  AUD_SYNC_CHECK();
}

/***************************************************** 
 * XA_SHOW_PIXMAPS
 *
 *****************************************************/
void XA_SHOW_PIXMAPS(act)
XA_ACTION *act;
{
  ACT_SETTER_HDR *act_pms_hdr;
  ACT_PIXMAP_HDR *back_pm_hdr,*work_pm_hdr;
  Pixmap work,back;
  xaLONG xpback,ypback;

  DEBUG_LEVEL2 fprintf(stderr,"ACT_SETTERS:\n");
  act_pms_hdr = (ACT_SETTER_HDR *)(act->data);

  if (act_pms_hdr->back->type != ACT_PIXMAP)
		XA_Image_To_Pixmap(act_pms_hdr->back);
  if (act_pms_hdr->work->type != ACT_PIXMAP)
		XA_Image_To_Pixmap(act_pms_hdr->work);
  back_pm_hdr = (ACT_PIXMAP_HDR *)act_pms_hdr->back->data;
  work_pm_hdr = (ACT_PIXMAP_HDR *)act_pms_hdr->work->data;
  back = back_pm_hdr->pixmap;
  work = work_pm_hdr->pixmap;

  /* copy backgrnd into work area */
  xpback = act_pms_hdr->xpback;
  ypback = act_pms_hdr->ypback;
  {
    xaLONG xback,yback;
    xaLONG xlen,ylen,xlen1,ylen1;

    xback = act_pms_hdr->xback;
    yback = act_pms_hdr->yback;
    xlen = xback - xpback;
    ylen = yback - ypback;
    if (xlen >= xa_buff_x) xlen1 = 0;
    else xlen1 = xa_buff_x - xlen;
    if (ylen >= xa_buff_y) ylen1 = 0;
    else ylen1 = xa_buff_y - ylen;

    if (xlen1 == 0)
    { 
      if (ylen1 == 0)
      {
        XCopyArea(theDisp, back, work, theGC,
		      xpback,ypback,xa_buff_x,xa_buff_y,0,0);
      }
      else
      {
        XCopyArea(theDisp, back, work, theGC,
		      xpback,ypback,xa_buff_x,ylen ,0,   0);
        XCopyArea(theDisp, back, work, theGC,
		      xpback,    0,xa_buff_x,ylen1,0,ylen);
      }
    }
    else /* xlen1 != 0 */
    { 
      if (ylen1 == 0)
      {
        XCopyArea(theDisp, back, work, theGC,
		      xpback,ypback,xlen ,xa_buff_y,0,   0);
        XCopyArea(theDisp, back, work, theGC,
		          0,ypback,xlen1,xa_buff_y,xlen,0);
      }
      else
      {
        XCopyArea(theDisp, back, work, theGC,
		      xpback,ypback,  xlen,  ylen,    0,    0);
        XCopyArea(theDisp, back, work, theGC,
		          0,    0, xlen1, ylen1, xlen, ylen);
        XCopyArea(theDisp, back, work, theGC,
		          0,ypback, xlen1,  ylen, xlen,    0);
        XCopyArea(theDisp, back, work, theGC,
		      xpback,    0,  xlen, ylen1,    0, ylen);
      }
    }
  }

    /* loop through face pixmaps */
  while(act_pms_hdr != 0)
  {
    xaULONG ret;
    ACT_PIXMAP_HDR *face_pm_hdr;

    if (act_pms_hdr->face->type != ACT_PIXMAP)
                ret = XA_Image_To_Pixmap(act_pms_hdr->face);
    face_pm_hdr = (ACT_PIXMAP_HDR *)act_pms_hdr->face->data;
    if (face_pm_hdr->clip)
    {
      XSetClipMask(theDisp,theGC,face_pm_hdr->clip);
      XSetClipOrigin(theDisp,theGC,
		act_pms_hdr->xpface,act_pms_hdr->ypface);
    }

    XCopyArea(theDisp,
		  face_pm_hdr->pixmap, work, theGC,
		  0, 0, 
		  act_pms_hdr->xface, act_pms_hdr->yface,
		  act_pms_hdr->xpface,  act_pms_hdr->ypface   );
    if (face_pm_hdr->clip) XSetClipMask(theDisp,theGC,None);
    act_pms_hdr = act_pms_hdr->next;
  }

  if (xa_need_to_scale_b)
  {
    XImage *t_image;
    xaULONG xp,yp,xsize,ysize;
    xaUBYTE *tmp_pic;
    xp = yp = 0; xsize = xa_buff_x; ysize = xa_buff_y;
    if (x11_display_type == XA_MONOCHROME)
    {
      t_image=XGetImage(theDisp,work,0,0,xsize,ysize,1,XYPixmap);
      tmp_pic = UTIL_Scale_Bitmap(0,t_image->data,
                xsize,ysize,t_image->bytes_per_line,xa_buff_x,xa_buff_y,
                xa_disp_x,xa_disp_y,&xp,&yp,&xsize,&ysize,
					x11_bit_order,x11_bit_order);
    }
    else
    {
      t_image=XGetImage(theDisp,work,0,0,xsize,ysize,AllPlanes,ZPixmap);
      tmp_pic = UTIL_Scale_Mapped(0,t_image->data,
                   0,0, xsize, ysize, t_image->bytes_per_line,
                   xa_buff_x,xa_buff_y,xa_disp_x,xa_disp_y,
                   x11_bytes_pixel,&xp,&yp,&xsize,&ysize,0);
    }
    if (tmp_pic)
    {
      theImage->data = (char *)tmp_pic;
      X11_Init_Image_Struct(theImage,xsize,ysize);
      if ( (act->chdr != 0) && (act->chdr != xa_chdr_now) )
                  XA_Install_CMAP(act->chdr);
      XPutImage(theDisp,mainW,theGC,theImage,0,0,xp,yp,xsize,ysize);
    }
    XDestroyImage(t_image);
  }
  else
  {
    if ( (act->chdr != 0) && (act->chdr != xa_chdr_now) )
		XA_Install_CMAP(act->chdr);
    XCopyArea(theDisp,work,mainW,theGC,0,0,xa_disp_x,xa_disp_y,0, 0);
  }
  AUD_SYNC_CHECK();
}


/***************************************************** 
 * XA_SHOW_DELTA
 *
 *****************************************************/
void XA_SHOW_DELTA(act)
XA_ACTION *act;
{ 
  ACT_DLTA_HDR *dlta_hdr = (ACT_DLTA_HDR *)act->data;
  xaULONG xsrc,ysrc,xdst,ydst,xsize,ysize,*remap_map;
  xaULONG xbuff,ybuff,map_flag,dith_flag,dlta_flag;
  XA_CHDR *the_chdr;
  char *the_pic;
  xaULONG no_shared;
  XA_DEC_INFO  *dec_info  = &xa_dec_info;
  XA_DEC2_INFO *dec2_info = &xa_dec2_info;

  no_shared = 0;
  if (cmap_dither_type == CMAP_DITHER_FLOYD) dith_flag = xaTRUE;
  else dith_flag = xaFALSE; 

  if (act->chdr == 0)	/* NOP */ return;
  /* if there is a new_chdr, install it, not the old one */
  else if (act->chdr->new_chdr) 
  {
    if (   (x11_display_type & XA_X11_TRUE)
	|| (x11_kludge_1 == xaTRUE)
	|| (x11_bytes_pixel != 1)	  ) map_flag = xaTRUE;
    /* if dithering and new_chdr then don't remap while decoding */
    else if (dith_flag)  map_flag = xaFALSE;
    else map_flag = xaTRUE;
    the_chdr = act->chdr->new_chdr;
  }
  else 
  {
    /* remap to larger pixel size NOTE: all anim are 1 byte */
    if (   (x11_display_type & XA_X11_TRUE)
	|| (x11_kludge_1 == xaTRUE)
	|| (x11_bytes_pixel != 1)	  ) map_flag = xaTRUE;
    else map_flag = xaFALSE;
    dith_flag = xaFALSE;
    the_chdr = act->chdr;
  }
  remap_map = act->chdr->map;

  if (dlta_hdr->xapi_rev == 0x0001)
  { dec_info->imagex	= xa_imagex;
    dec_info->imagey	= xa_imagey;
    dec_info->imaged	= xa_imaged;
    dec_info->chdr	= act->chdr;
    dec_info->map_flag	= map_flag;
    dec_info->map	= remap_map;
    dec_info->special	= dlta_hdr->special;
    dec_info->extra	= dlta_hdr->extra;

    if (xa_skip_video == 0) { xa_skip_cnt = 0; dec_info->skip_flag = 0; }
    else { if (xa_skip_cnt < XA_SKIP_MAX) dec_info->skip_flag= xa_skip_video;
	   else { xa_skip_cnt = 0; dec_info->skip_flag = 0; } }

    the_pic = xa_pic; xbuff = xa_imagex; ybuff = xa_imagey;
    if ((xa_vid_fd >= 0) && (!(dlta_hdr->flags & DLTA_DATA)) )
    { int ret = XA_Read_Delta(xa_vidcodec_buf, xa_vid_fd,
					dlta_hdr->fpos, dlta_hdr->fsize);
      /* if read fails */
      if (ret == xaFALSE) dlta_flag = ACT_DLTA_NOP;
      else dlta_flag = dlta_hdr->delta(the_pic,xa_vidcodec_buf,
					dlta_hdr->fsize,dec_info);
    }
    else
    { dlta_flag = dlta_hdr->delta(the_pic,dlta_hdr->data,
					dlta_hdr->fsize,dec_info);
    }
    if ( (dlta_flag == ACT_DLTA_DROP) || (dec_info->skip_flag > 0) )
	{ xa_skip_cnt++; xa_frames_skipd++; return; }
  } /* End of XAPI_REV 0x0001 */
  else if (dlta_hdr->xapi_rev == 0x0002)
  { dec2_info->imagex	= xa_imagex;
    dec2_info->imagey	= xa_imagey;
    dec2_info->imaged	= xa_imaged;
    dec2_info->chdr	= act->chdr;
    dec2_info->map_flag	= map_flag;
    dec2_info->map	= remap_map;
    dec2_info->special	= dlta_hdr->special;
    dec2_info->extra	= dlta_hdr->extra;
    dec2_info->tmp1 = dec2_info->tmp2 = dec2_info->tmp3 = dec2_info->tmp4 = 0;
    dec2_info->bytes_pixel	= x11_bytes_pixel;
    dec2_info->image_type = XA_Get_Image_Type( dlta_hdr->special,
							act->chdr,map_flag);
/* POD or in dither and cf4 into special */
    if (xa_dither_flag)	dec2_info->special |= XA_DEC_SPEC_DITH;
/* XA_DEC_SPEC_CF4 */

    if (xa_skip_video == 0) { xa_skip_cnt = 0; dec2_info->skip_flag = 0; }
    else { if (xa_skip_cnt < XA_SKIP_MAX) dec2_info->skip_flag= xa_skip_video;
	   else { xa_skip_cnt = 0; dec2_info->skip_flag = 0; } }

    the_pic = xa_pic; xbuff = xa_imagex; ybuff = xa_imagey;
    if ((xa_vid_fd >= 0) && (!(dlta_hdr->flags & DLTA_DATA)) )
    { int ret = XA_Read_Delta(xa_vidcodec_buf, xa_vid_fd,
					dlta_hdr->fpos, dlta_hdr->fsize);
      /* if read fails */
      if (ret == xaFALSE) dlta_flag = ACT_DLTA_NOP;
      else dlta_flag = dlta_hdr->delta(the_pic,xa_vidcodec_buf,
					dlta_hdr->fsize,dec2_info);
    }
    else
    { dlta_flag = dlta_hdr->delta(the_pic,dlta_hdr->data,
					dlta_hdr->fsize,dec2_info);
    }
    if ( (dlta_flag == ACT_DLTA_DROP) || (dec2_info->skip_flag > 0) )
	{ xa_skip_cnt++; xa_frames_skipd++; return; }
  } /* End of XAPI_REV 0x0001 */
  else /* Start of XAPI_REV 0x0000 */
  {
fprintf(stderr,"WHO USES THIS???\n");
     dlta_flag = ACT_DLTA_NOP;
  } /* End of XAPI_REV 0x0000 */


  if (x11_expose_flag == xaTRUE) { x11_expose_flag = xaFALSE;
		xsrc = ysrc = 0; xsize = xbuff; ysize = ybuff; }
  else if (dlta_flag & ACT_DLTA_NOP) { act->type = ACT_NOP; return; }
  else if (dlta_hdr->xapi_rev == 0x0001)
  { xsrc = dec_info->xs;	ysrc = dec_info->ys; 
    xsize = dec_info->xe;	ysize = dec_info->ye;
  }
  else if (dlta_hdr->xapi_rev == 0x0002)
  { xsrc = dec2_info->xs;	ysrc = dec2_info->ys; 
    xsize = dec2_info->xe;	ysize = dec2_info->ye;
  }

  if (dlta_flag & ACT_DLTA_BODY)
  {
    if (im_buff0 && (im_buff0 != the_pic) )
		memcpy((char *)im_buff0, (char *)the_pic, xa_image_size);
    if (im_buff1 && (im_buff1 != the_pic) )
		memcpy((char *)im_buff1, (char *)the_pic, xa_image_size);
    xsize = dlta_hdr->xsize; ysize = dlta_hdr->ysize;
    xa_image_size = xa_imagex * xa_imagey;
    IFF_Init_DLTA_HDR(xsize,ysize);
  }
  if (xa_anim_flags & ANIM_DBL_BUF)
  {
    IFF_Update_DLTA_HDR(&xsrc,&ysrc,&xsize,&ysize);
    xa_pic = (xa_pic==im_buff0)?im_buff1:im_buff0;
    /* With Double Buffering we don't want any NOP's generated */
    if (xsize == xsrc) { xsrc = ysrc = 0; xsize = 8; ysize = 1; }
  } 

  /* convert min/max to pos/size */
  xsize -= xsrc;	ysize -= ysrc;
  xdst = xsrc; ydst = ysrc;

  if (xa_anim_flags & ANIM_HAM)
  {
    xsize =  4*((xsize+3)/4); /* PODTEST */
    if (xa_anim_flags & ANIM_HAM6) IFF_Buffer_HAM6(im_buff2,the_pic,
		act->chdr,act->h_cmap,xsize,ysize,xsrc,ysrc,xbuff,xaTRUE);
    else			 IFF_Buffer_HAM8(im_buff2,the_pic,
		act->chdr,act->h_cmap,xsize,ysize,xsrc,ysrc,xbuff,xaTRUE);
    the_pic = im_buff2;
#ifdef XSHM
    if (shm) { sh_Image = im2_Image; }
#endif
    dlta_flag |= ACT_DLTA_MAPD;
    xsrc = ysrc = 0; xbuff = xsize; ybuff = ysize;
  }

  if (x11_display_type == XA_MONOCHROME)
  { no_shared = 1;
    if (xa_need_to_scale_u)
    {
      xaUBYTE *tmp_pic;
      xaULONG line_size;
      tmp_pic = UTIL_Scale_Mapped(0,the_pic,0,0,
			xbuff,ybuff, xbuff,
			xa_imagex,xa_imagey, xa_disp_x,xa_disp_y,
			x11_bytes_pixel,&xdst,&ydst,&xsize,&ysize,0);
      if (tmp_pic == 0) return; /* NOP */
      line_size = X11_Get_Line_Size(xsize);
      UTIL_Mapped_To_Bitmap(tmp_pic,tmp_pic,act->chdr,0,0,
		    xsize,ysize,xsize,ysize,line_size);
      xsrc = ysrc = 0;
      theImage->data = (char *)tmp_pic;
      X11_Init_Image_Struct(theImage,xsize,ysize);
    } /* end of scale */
    else
    {
      xaULONG line_size,tsize;
      tsize = ysize * X11_Get_Line_Size(xsize);
      XA_REALLOC(xa_disp_buff,xa_disp_buff_size,tsize);
      line_size = X11_Get_Line_Size(xbuff);
      UTIL_Mapped_To_Bitmap(xa_disp_buff,the_pic,act->chdr,
			xsrc,ysrc,xsize,ysize,xbuff,ybuff,line_size);
      xsrc = ysrc = 0;
      theImage->data = xa_disp_buff;
      X11_Init_Image_Struct(theImage,xsize,ysize);
    }
  } /* end of mono */
  else
  {
    if (dith_flag == xaTRUE) /* map here if dithering is on */
    {
      if (cmap_dither_type == CMAP_DITHER_FLOYD)
      {
        UTIL_Mapped_To_Floyd(im_buff2,the_pic,
	      		act->chdr->new_chdr,act->chdr,xsrc,ysrc,
			xsize,ysize,xbuff,ybuff);
        xsrc = ysrc = 0; xbuff = xsize; ybuff = ysize;
        the_pic = im_buff2;	dlta_flag |= ACT_DLTA_MAPD;
#ifdef XSHM
        if (shm) 
        { sh_Image = im2_Image; 
          X11_Init_Image_Struct(im2_Image,xbuff,ybuff);
        }
#endif
      }
    } /* end of dither */

    if (xa_need_to_scale_u)
    {
      XA_CHDR *tmp_chdr;
      xaUBYTE *tmp_pic;
      no_shared = 1;
      if ((map_flag==xaTRUE) && (!(dlta_flag & ACT_DLTA_MAPD)) )
				tmp_chdr = act->chdr;
      else tmp_chdr = 0;
      tmp_pic = UTIL_Scale_Mapped(0,the_pic,xsrc,ysrc,
			  xsize,ysize,X11_Get_Line_Size(xbuff),
			  xa_imagex,xa_imagey,xa_disp_x,xa_disp_y,
			  x11_bytes_pixel,&xdst,&ydst,&xsize,&ysize,tmp_chdr);
      if (tmp_pic==0) return; /*NOP*/
      xsrc = ysrc = 0;
      theImage->data = (char *)tmp_pic;
      xbuff = xsize; ybuff = ysize;
    } /* end of scaling */
    else /* no scaling */
    {
      if (   ((x11_bytes_pixel > 1) && (!(dlta_flag & ACT_DLTA_MAPD)))
	        || ((map_flag==xaTRUE) && (!(dlta_flag & ACT_DLTA_MAPD)))     )
      {
        xaULONG tsize = (xsize) * (ysize) * x11_bytes_pixel;
        XA_REALLOC(xa_disp_buff,xa_disp_buff_size,tsize);
        UTIL_Mapped_To_Mapped(xa_disp_buff,the_pic,act->chdr,
			xsrc,ysrc,xsize,ysize,xbuff,ybuff);
        xsrc = ysrc = 0;
        theImage->data = (char *)xa_disp_buff;
        xbuff = xsize; ybuff = ysize;
        no_shared = 1;
      } 
      else 
      { 
#ifdef XSHM
        if (shm) sh_Image->data = the_pic;
#endif
        theImage->data = the_pic;
      }
    } /* end of no scaling */

    if (x11_pack_flag == xaTRUE)
    { no_shared = 1;
      UTIL_Pack_Image(im_buff3,theImage->data,xbuff,ybuff);
      theImage->data = im_buff3;
    }
#ifdef XSHM
    if (sh_Image) X11_Init_Image_Struct(sh_Image,xbuff,ybuff);
#endif
    X11_Init_Image_Struct(theImage,xbuff,ybuff);
  } /* end of not mono */

  if (xa_no_disp == xaFALSE)
  {
    if ( (the_chdr != 0) && (the_chdr != xa_chdr_now) )
						XA_Install_CMAP(the_chdr);
#ifdef XSHM
    if (shm && (no_shared==0) )
    {
      AUD_SYNC_CHECK();
      XShmPutImage(theDisp,mainW,theGC,sh_Image,
				xsrc,ysrc,xdst,ydst,xsize,ysize,True );
    } else
#endif
    {
      XPutImage(theDisp,mainW,theGC,theImage,xsrc,ysrc,xdst,ydst,xsize,ysize);
    }
#ifdef XMBUF
    if (mbuf) {
      DEBUG_LEVEL1 fprintf(stderr, "Swapping buffers\n");
      XmbufDisplayBuffers(theDisp, 1, &mainW, 0, 0);
      mainW = mainWbuffers[mainWbufIndex = !mainWbufIndex];
    }
#endif
    AUD_SYNC_CHECK();
  }
#ifdef XSHM
  if ( (shm) && (xa_anim_flags & ANIM_DBL_BUF) ) 
		{sh_Image = (xa_pic==im_buff0)?im0_Image:im1_Image;}
	/* note: xa_pic already swapped */
#endif
} /* end of DELTA case */


/***************************************************** 
 * XA_Image_To_Pixmap
 *
 * Convert IMAGE action into PIXMAP action
 *****************************************************/
xaULONG XA_Image_To_Pixmap(act)
XA_ACTION *act;
{
  ACT_IMAGE_HDR *act_im_hdr;
  ACT_PIXMAP_HDR *act_pm_hdr;
  xaULONG line_size;

  if (act->type == ACT_NOP) return(0);
  if (act->type != ACT_IMAGE) 
  { 
    fprintf(stderr,"XA_Image_To_Pixmap: not Image err %x\n",act->type);
    TheEnd();
  }
  act_im_hdr = (ACT_IMAGE_HDR *)(act->data);
  if (!(act_im_hdr->flag & ACT_BUFF_VALID)) return(0);
  act_pm_hdr = (ACT_PIXMAP_HDR *) malloc( sizeof(ACT_PIXMAP_HDR) );
  if (act_pm_hdr == 0) TheEnd1("Image_to_Pixmap: malloc err\n");

  if (x11_display_type == XA_MONOCHROME)
	line_size = X11_Get_Bitmap_Width(act_im_hdr->xsize);
  else line_size = act_im_hdr->xsize;

  if(act_im_hdr->clip)
  {
    act_pm_hdr->clip = 
	XCreatePixmapFromBitmapData(theDisp,mainW,
                (char *)act_im_hdr->clip,
                X11_Get_Bitmap_Width(act_im_hdr->xsize),act_im_hdr->ysize,
                0x01,0x00,1);
    XSync(theDisp,False);
  }
  else act_pm_hdr->clip = 0;

  act->type = ACT_PIXMAP;
  act->data = (xaUBYTE *) act_pm_hdr;
  act_pm_hdr->flag  = ACT_BUFF_VALID;
  act_pm_hdr->xpos  = act_im_hdr->xpos;
  act_pm_hdr->ypos  = act_im_hdr->ypos;
  act_pm_hdr->xsize = act_im_hdr->xsize;
  act_pm_hdr->ysize = act_im_hdr->ysize;
  act_pm_hdr->pixmap = XCreatePixmap(theDisp,mainW,
                line_size, act_pm_hdr->ysize, x11_depth);
  XSync(theDisp,False);
  DEBUG_LEVEL2 fprintf(stderr,
	"XA_Image_To_Pixmap: pixmap = %x\n", (xaULONG)act_pm_hdr->pixmap);
  XSetClipMask(theDisp,theGC,None);
  XPutImage(theDisp, act_pm_hdr->pixmap, theGC, act_im_hdr->image,
      0,0,0,0,act_pm_hdr->xsize,act_pm_hdr->ysize);
  XSync(theDisp,False);
  if (act_im_hdr->clip) { FREE(act_im_hdr->clip,0x03); act_im_hdr->clip=0;}
  XDestroyImage(act_im_hdr->image);
  FREE(act_im_hdr,0x04);
/*POD NOTE: NEED TO SEARCH ACTIONS FOR IMAGES WITH non valid buffers to
 * to see if they just got converted to a pixmap by this as well. */
  return(1);
}

/***************************************************** 
 * XA_Read_Delta
 *
 * Read delta fromf ile for decompression.
 *****************************************************/
xaULONG XA_Read_Delta(dptr,fd,fpos,fsize)
char *dptr;	/* buffer to put delta into */
int fd;		/* file descript to read from */
xaULONG fpos;	/* pos within file */
xaULONG fsize;	/* size of delta */
{
  xaLONG ret;

  if (fsize == 0) return(xaTRUE); /* nothing to read - NOP frame */

  ret = lseek(fd,fpos,SEEK_SET);
  if (ret != fpos) 
  { fprintf(stderr,"XA_Read_Delta: Can't seek vid fpos");
    return(xaFALSE);
  }
  ret = read(fd,dptr,fsize);
  if (ret != fsize) 
  { fprintf(stderr,"XA_Read_Delta: Can't read vid data ret %08x fsz %08x\n",
								ret,fsize);
    return(xaFALSE);
  }
  return(xaTRUE);
}


