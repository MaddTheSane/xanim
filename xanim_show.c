
/*
 * xanim_show.c
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

/* Revisions:
 * 13Nov94:  wasn't properly skipping video frames in certain instances.
 */

#include "xanim.h"
#include <Intrinsic.h>
#include <StringDefs.h>
#include <Shell.h>
#ifdef XSHM
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
extern Visual        *theVisual;
#endif /*XSHM*/
#include "xanim_x11.h"

void XA_SHOW_IMAGE();
void XA_SHOW_PIXMAP();
void XA_SHOW_IMAGES();
void XA_SHOW_PIXMAPS();

UBYTE *UTIL_Scale_Bitmap();
UBYTE *UTIL_Scale_Mapped();
void UTIL_Pack_Image();
void UTIL_Mapped_To_Mapped();
void UTIL_Mapped_To_Bitmap();
void UTIL_Mapped_To_Floyd();
ULONG XA_Image_To_Pixmap();
void XA_Read_Delta();
void X11_Init_Image_Struct();
void XA_Install_CMAP();
void IFF_Init_DLTA_HDR();
void IFF_Update_DLTA_HDR();
void IFF_Buffer_HAM6();
void IFF_Buffer_HAM8();


extern ULONG xa_need_to_scale_b;
extern ULONG xa_need_to_scale_u;
extern ULONG xa_buff_x, xa_buff_y;
extern ULONG xa_disp_x, xa_disp_y;
extern char *im_buff0,*im_buff1,*im_buff2,*im_buff3;
extern char *xa_pic,*xa_disp_buff,*xa_scale_buff;
extern ULONG xa_disp_buff_size,xa_scale_buff_size;
extern LONG xa_skip_flag;
extern LONG xa_serious_skip_flag;
extern LONG xa_skip_video;
extern LONG xa_skip_cnt;
#define XA_SKIP_MAX 25  /* don't skip more than 5 frames in a row */
extern ULONG xa_image_size;
extern ULONG xa_imagex;
extern ULONG xa_imagey;
extern ULONG xa_imaged;
extern LONG xa_pixmap_flag;
extern LONG xa_dither_flag;
extern int xa_vid_fd;
extern UBYTE *xa_vidcodec_buf;
extern ULONG x11_expose_flag;
extern LONG xa_anim_flags;
extern LONG xa_no_disp;


extern ULONG shm;
#ifdef XSHM
extern XShmSegmentInfo im0_shminfo;
extern XShmSegmentInfo im1_shminfo;
extern XShmSegmentInfo im2_shminfo;
extern XImage *im0_Image;
extern XImage *im1_Image;
extern XImage *im2_Image;
extern XImage *sh_Image;
#endif







/***************************************************** 
 * XA_SHOW_IMAGE
 *
 *****************************************************/
void XA_SHOW_IMAGE(act,im_xpos,im_ypos,im_xsize,im_ysize,flag)
XA_ACTION *act;
ULONG im_xpos,im_ypos;
ULONG im_xsize,im_ysize;
ULONG flag;		/* override flag 0 normal 1 use pos/size */
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
  { UBYTE *tmp_pic;
    ULONG xp,yp,xsize,ysize;

    if(act_im_hdr->clip)
    {
      xsize = im_xsize;	ysize = im_ysize;
      xp = im_xpos;	yp = im_ypos;
      tmp_pic = UTIL_Scale_Bitmap(0,act_im_hdr->clip,im_xsize,im_ysize,
		(X11_Get_Bitmap_Width(xsize)/8),xa_buff_x,xa_buff_y,
		xa_disp_x,xa_disp_y,&xp,&yp,&xsize,&ysize,X11_LSB,X11_LSB);
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
    if(act_im_hdr->clip) XSetClipMask(theDisp,theGC,None);
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
  if (xa_audio_enable != TRUE) XSync(theDisp,False);
}


/***************************************************** 
 * XA_SHOW_PIXMAP
 *
 *****************************************************/
void XA_SHOW_PIXMAP(act,pm_xpos,pm_ypos,pm_xsize,pm_ysize,flag)
XA_ACTION *act;
ULONG pm_xpos,pm_ypos;
ULONG pm_xsize,pm_ysize;
ULONG flag;		/* 0 normal 1 use pos/size */
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
    UBYTE *tmp_pic;
    ULONG xp,yp,xsize,ysize;

    if (act_pm_hdr->clip)
    {
      xp = pm_xpos;      yp = pm_ypos;
      xsize = pm_xsize;  ysize = pm_ysize;
      p_image = XGetImage(theDisp,act_pm_hdr->clip,0,0,xsize,ysize,1,XYPixmap);
      tmp_pic = UTIL_Scale_Bitmap(0,p_image->data, xsize,ysize,
		p_image->bytes_per_line,xa_buff_x,xa_buff_y,
                xa_disp_x,xa_disp_y,&xp,&yp,&xsize,&ysize,
					x11_bit_order,X11_LSB);
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
    if (xa_audio_enable != TRUE) XSync(theDisp,False);
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
    if (xa_audio_enable != TRUE) XSync(theDisp,False);
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
  LONG xpback,ypback;

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
    LONG xback,yback;
    LONG xlen,ylen,xlen1,ylen1;

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
    ULONG xp,yp,xsize,ysize;
    UBYTE *tmp_pic;
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
  if (xa_audio_enable != TRUE) XSync(theDisp,False);
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
  LONG xpback,ypback;

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
    LONG xback,yback;
    LONG xlen,ylen,xlen1,ylen1;

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
    ULONG ret;
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
    ULONG xp,yp,xsize,ysize;
    UBYTE *tmp_pic;
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
  if (xa_audio_enable != TRUE) XSync(theDisp,False);
}


/***************************************************** 
 * XA_SHOW_DELTA
 *
 *****************************************************/
void XA_SHOW_DELTA(act)
XA_ACTION *act;
{
  ACT_DLTA_HDR *dlta_hdr = (ACT_DLTA_HDR *)act->data;
  ULONG xsrc,ysrc,xdst,ydst,xsize,ysize,*remap_map;
  ULONG xbuff,ybuff,map_flag,dith_flag,dlta_flag;
  XA_CHDR *the_chdr;
  char *the_pic;
  ULONG no_shared;

  no_shared = 0;
  if (cmap_dither_type == CMAP_DITHER_FLOYD) dith_flag = TRUE;
  else dith_flag = FALSE; 
  /* if there is a new_chdr, install it, not the old one */
  if (act->chdr->new_chdr) 
  {
    /* if dithering and new_chdr then don't remap while decoding */
    if (dith_flag)  map_flag = FALSE;
    else map_flag = TRUE;
    the_chdr = act->chdr->new_chdr;
  }
  else 
  {
    /* remap to larger pixel size NOTE: all anim are 1 byte */
    if (   (x11_display_type & XA_X11_TRUE)
	|| (x11_kludge_1 == TRUE)
	|| (x11_bytes_pixel != 1)	  ) map_flag = TRUE;
    else map_flag = FALSE;
    dith_flag = FALSE;
    the_chdr = act->chdr;
  }
  remap_map = act->chdr->map;

  /* EXTREME skip video frame */
  if ((xa_serious_skip_flag==TRUE) && (xa_skip_video > 2))
  {
    if (xa_skip_cnt < XA_SKIP_MAX)
    {
      xa_skip_cnt++;
      xa_skip_video--;
      return;
    }
  }

  the_pic = xa_pic; xbuff = xa_imagex; ybuff = xa_imagey;
  if ((xa_vid_fd >= 0) && (!(dlta_hdr->flags & DLTA_DATA)) )
  {
    XA_Read_Delta(xa_vidcodec_buf,xa_vid_fd,dlta_hdr->fpos,dlta_hdr->fsize);
    dlta_flag = dlta_hdr->delta(the_pic,xa_vidcodec_buf,dlta_hdr->fsize,
			act->chdr,remap_map,map_flag,xbuff,ybuff,xa_imaged,
			&xsrc,&ysrc,&xsize,&ysize,dlta_hdr->special,
			dlta_hdr->extra);
  }
  else
  {
    dlta_flag = dlta_hdr->delta(the_pic,dlta_hdr->data,dlta_hdr->fsize,
			act->chdr,remap_map,map_flag,xbuff,ybuff,xa_imaged,
			&xsrc,&ysrc,&xsize,&ysize,dlta_hdr->special,
			dlta_hdr->extra);
  }
  /* skip video frame */
  if ((xa_skip_flag==TRUE) && (xa_skip_video > 2)) 
  {
    if (xa_skip_cnt < XA_SKIP_MAX) 
    { 
      xa_skip_cnt++;
      xa_skip_video--;
      return;
    }
    else xa_skip_cnt = 0;
  }
  else xa_skip_cnt = 0;
  if (x11_expose_flag == TRUE) { x11_expose_flag = FALSE;
		xsrc = ysrc = 0; xsize = xbuff; ysize = ybuff; }
  else if (dlta_flag & ACT_DLTA_NOP) { act->type = ACT_NOP; return; }

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
  } 

  /* convert min/max to pos/size */
  xsize -= xsrc;	ysize -= ysrc;
  xdst = xsrc; ydst = ysrc;
  if (xsize == 0) {act->type = ACT_NOP; fprintf(stderr,"QQ\n"); return;}

  if (xa_anim_flags & ANIM_HAM)
  {
    xsize =  4*((xsize+3)/4); /* PODTEST */
    if (xa_anim_flags & ANIM_HAM6) IFF_Buffer_HAM6(im_buff2,the_pic,
		act->chdr,act->h_cmap,xsize,ysize,xsrc,ysrc,xbuff,TRUE);
    else			 IFF_Buffer_HAM8(im_buff2,the_pic,
		act->chdr,act->h_cmap,xsize,ysize,xsrc,ysrc,xbuff,TRUE);
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
      UBYTE *tmp_pic;
      ULONG line_size;
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
      ULONG line_size,tsize;
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
    if (dith_flag == TRUE) /* map here if dithering is on */
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
      UBYTE *tmp_pic;
      no_shared = 1;
      if ((map_flag==TRUE) && (!(dlta_flag & ACT_DLTA_MAPD)) )
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
	        || ((map_flag==TRUE) && (!(dlta_flag & ACT_DLTA_MAPD)))     )
      {
        ULONG tsize = (xsize) * (ysize) * x11_bytes_pixel;
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

    if (x11_pack_flag == TRUE)
    { no_shared = 1;
      UTIL_Pack_Image(im_buff3,theImage->data,xbuff,ybuff);
      theImage->data = im_buff3;
    }
#ifdef XSHM
    if (sh_Image) X11_Init_Image_Struct(sh_Image,xbuff,ybuff);
#endif
    X11_Init_Image_Struct(theImage,xbuff,ybuff);
  } /* end of not mono */

  if (xa_no_disp == FALSE)
  {
    if ( (the_chdr != 0) && (the_chdr != xa_chdr_now) )
						XA_Install_CMAP(the_chdr);
#ifdef XSHM
    if (shm && (no_shared==0) )
    {
      if (xa_audio_enable != TRUE) XSync(theDisp,False);
      XShmPutImage(theDisp,mainW,theGC,sh_Image,
				xsrc,ysrc,xdst,ydst,xsize,ysize,True );
    } else
#endif
    {
      XPutImage(theDisp,mainW,theGC,theImage,xsrc,ysrc,xdst,ydst,xsize,ysize);
    }
    if (xa_audio_enable != TRUE) XSync(theDisp,False);
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
ULONG XA_Image_To_Pixmap(act)
XA_ACTION *act;
{
  ACT_IMAGE_HDR *act_im_hdr;
  ACT_PIXMAP_HDR *act_pm_hdr;
  ULONG line_size;

  if (act->type == ACT_NOP) return(0);
  if (act->type != ACT_IMAGE) 
  { 
    fprintf(stderr,"XA_Image_To_Pixmap: not Image err %lx\n",act->type);
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
  act->data = (UBYTE *) act_pm_hdr;
  act_pm_hdr->flag  = ACT_BUFF_VALID;
  act_pm_hdr->xpos  = act_im_hdr->xpos;
  act_pm_hdr->ypos  = act_im_hdr->ypos;
  act_pm_hdr->xsize = act_im_hdr->xsize;
  act_pm_hdr->ysize = act_im_hdr->ysize;
  act_pm_hdr->pixmap = XCreatePixmap(theDisp,mainW,
                line_size, act_pm_hdr->ysize, x11_depth);
  XSync(theDisp,False);
  DEBUG_LEVEL2 fprintf(stderr,
	"XA_Image_To_Pixmap: pixmap = %lx\n", act_pm_hdr->pixmap);
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
void XA_Read_Delta(dptr,fd,fpos,fsize)
char *dptr;	/* buffer to put delta into */
int fd;		/* file descript to read from */
ULONG fpos;	/* pos within file */
ULONG fsize;	/* size of delta */
{
  LONG ret;
  ret = lseek(fd,fpos,SEEK_SET);
  if (ret != fpos) TheEnd1("XA_Read_Delta: Can't seek vid fpos");
  ret = read(fd,dptr,fsize);
  if (ret != fsize) 
  {
    fprintf(stderr,"XA_Read_Delta: Can't read vid data ret %08lx fsz %08lx",
								ret,fsize);
    TheEnd();
  }
}

