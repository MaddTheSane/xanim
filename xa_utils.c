
/*
 * xa_utils.c
 *
 * Copyright (C) 1991-1998,1999 by Mark Podlipec. 
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
/* revhist
 * 12dec94  +bCdm caused core on Alpha OSF/1 v3.0. 
 *           found and fixed by David Mosberger-Tang.
 * 14Dec97  Adam Moss:  Optimized scaling routines and found bug
 *          when allocating memory for scaling the clip mask of
 *	    an image(used in GIF animations).
 *
 */ 
#include "xanim.h"
#include <ctype.h>

void UTIL_Sub_Image();
void UTIL_Create_Clip();
void UTIL_Mapped_To_Mapped();
void UTIL_Mapped_To_Bitmap();
xaULONG UTIL_Get_Buffer_Scale();
void UTIL_Scale_Buffer_Pos();
void UTIL_Scale_Pos_Size();
xaUBYTE *UTIL_Alloc_Scaled();
xaUBYTE *UTIL_Scale_Mapped();
xaUBYTE *UTIL_Scale_Bitmap();
void UTIL_Pack_Image();
void UTIL_FPS_2_Time();

static xaULONG UTIL_Get_LSB_Long();
static xaULONG UTIL_Get_LSB_Short();
extern xaULONG UTIL_Get_MSB_Long();
static xaULONG UTIL_Get_MSB_UShort();
static xaLONG UTIL_Get_MSB_Short();
extern void UTIL_Mapped_To_Floyd();
extern xaULONG CMAP_Find_Closest();
extern void CMAP_Cache_Init();
extern void CMAP_Cache_Clear();

extern xaULONG *xa_scale_row_buff;
extern xaULONG xa_scale_row_size;
extern char *xa_scale_buff;
extern xaULONG xa_scale_buff_size;
extern xaLONG xa_dither_flag;
extern xaULONG xa_buff_x,xa_buff_y;
extern float xa_bscalex,xa_bscaley;


void
UTIL_Sub_Image(out,in,xosize,yosize,xipos,yipos,xisize,pix_size)
xaUBYTE *out,*in;
xaULONG xosize,yosize,xipos,yipos,xisize,pix_size;
{
  xaULONG y,x;
  xaUBYTE *in_ptr;
  xosize *= pix_size;
  xisize *= pix_size;
  xipos  *= pix_size;
  for(y=yipos; y < (yipos + yosize); y++)
  {
    in_ptr = (xaUBYTE *)(in + (y * xisize + xipos));
    x = xosize;
    while(x--) *out++ = *in_ptr++;
  } 
}

void
UTIL_Mapped_To_Mapped(out,in,chdr,xipos,yipos,xosize,yosize,xisize,yisize)
xaUBYTE *out;		/* output image - maybe the same as input image */
xaUBYTE *in;		/* input image */
XA_CHDR *chdr;		/* color header to remap to */
xaULONG xipos,yipos;	/* pos of section in input buffer */
xaULONG xosize,yosize;	/* size of section in input buffer */
xaULONG xisize,yisize;	/* size of input buffer */
{
  xaULONG *map,moff;
  xaULONG y,x;
  xaUBYTE *in_ptr;

  map = chdr->map;
  moff = chdr->moff;
  switch(x11_bytes_pixel)
  {
   case 4: /* start from backside to allow for when out = in */
	{
	  xaULONG *ulp;
	  for(y=yosize; y > 0; y--)
	  {
	    in_ptr = (xaUBYTE *)(in + (y + yipos - 1) * xisize 
						+ xipos + xosize - 1 );
	    ulp =    (xaULONG *)(out + ((4 * y * xosize) - 4));
	    x = xosize;
	    if (moff) while(x--) *ulp-- = map[(*in_ptr--)-moff];
	    else while(x--) *ulp-- = map[*in_ptr--];
	  }
	}
	break;
   case 2: /* start from backside to allow for when out = in */
	{
	  xaUSHORT *usp;
	  for(y=yosize; y > 0; y--)
	  {
	    in_ptr = (xaUBYTE *)(in + (y + yipos - 1) * xisize 
						+ xipos + xosize - 1 );
	    usp =    (xaUSHORT *)(out + ((2 * y * xosize) - 2));
	    x = xosize;
	    if (moff) while(x--) *usp-- = (xaUSHORT)map[(*in_ptr--)-moff];
	    else while(x--) *usp-- = (xaUSHORT)map[*in_ptr--];
	  }
	}
	break;
   case 1:
	{
	  for(y=yipos; y < (yipos + yosize); y++)
	  {
	    in_ptr = (xaUBYTE *)(in + y * xisize + xipos);
	    x = xosize;
	    if (moff) while(x--) *out++ = (xaUBYTE)(map[(*in_ptr++)-moff]);
	    else while(x--) *out++ = (xaUBYTE)(map[*in_ptr++]);
	  }
	}
	break;
  } /* end switch */ 
}

void
UTIL_Create_Clip(out,in,pix_mask,xsize,ysize,pix_size,xosize,bit_order)
xaUBYTE *out,*in;
xaULONG pix_mask,xsize,ysize,pix_size,xosize,bit_order;
{
  register xaULONG data_in,bit_mask,x,y,mask_start,mask_end;
  register xaUBYTE data_out;

  if (bit_order == XA_MSBIT_1ST) { mask_start = 0x80; mask_end = 0x01; }
  else { mask_start = 0x01; mask_end = 0x80; }

  for(y=0; y<ysize; y++)
  {
    bit_mask = mask_start;
    data_out = 0;
    for(x=0; x < xosize; x++)
    {
      if (x < xsize)
      {
        if (pix_size == 4) 
	  { xaULONG *ulp = (xaULONG *)in;	data_in = *ulp; }
        else if (pix_size == 2)
	  { xaUSHORT *usp = (xaUSHORT *)in;	data_in = (xaULONG)(*usp); }
        else				data_in = (xaULONG)(*in);
        in += pix_size;
        if (data_in != pix_mask) data_out |= bit_mask;
      }

      if (bit_mask == mask_end)
      {
        *out++ = data_out;
        bit_mask = mask_start;
	data_out = 0;
      }
      else if (mask_start == 0x01) bit_mask <<= 1;
           else bit_mask >>= 1;
    } /* end of for x */
    if (bit_mask != mask_start) *out++ = data_out;
  } /* end of for y */
}

void
UTIL_Mapped_To_Bitmap(out,in,chdr,xpos,ypos,xsize,ysize,width,height,line_size)
xaUBYTE *out;		/* output buffer */
xaUBYTE *in;		/* input buffer */
XA_CHDR *chdr;		/* Color Hdr to use */
xaULONG xpos,ypos;	/* pos of section in input buffer */
xaULONG xsize, ysize;	/* size of section in input buffer */
xaULONG width, height;	/* size of buffer */
xaULONG line_size;	/* size if bytes of input buffer */
{
  xaULONG y,flag,csize;
  ColorReg *cmap;
  register xaULONG mask,invert,x,mask_start,mask_end;
  xaSHORT *imap,*c_ptr,*n_ptr,*err_buff,err,threshold;
  xaUBYTE data,*o_ptr;
  
  csize = chdr->csize;
  cmap = chdr->cmap;
 
  err_buff = (xaSHORT *)malloc(xsize * 2 * sizeof(xaSHORT));
  imap     = (xaSHORT *)malloc(csize * sizeof(xaSHORT));

   /* shift gray down to 8 bits */
  for(x=0; x<csize; x++) imap[x] = cmap[x].gray >> 8;

  for(x = 0; x < xsize; x++) err_buff[x] = (xaUSHORT)imap[ *in++ ];
  flag = 0;

  /* used to invert image */
  if (x11_white & 0x01) invert = 0x00;
  else invert = ~0x00;
  threshold = 128;

  if (x11_bit_order == XA_MSBIT_1ST) { mask_start = 0x80; mask_end = 0x01; }
  else { mask_start = 0x01; mask_end = 0x80; }

  for(y = 0; y < ysize; y++)
  {
    o_ptr = (xaUBYTE *)(out + (line_size * y));
    /* double buffer error arrays */
    n_ptr = c_ptr = err_buff;
    if (flag == 0) { n_ptr += xsize; flag = 1; }
    else { c_ptr += xsize; flag = 0;
    }
    data = 0x00;
    mask = mask_start;
    if (y < (ysize - 1) )  n_ptr[0] = (xaUSHORT)imap[ *in++ ];

    for(x=0; x<xsize; x++)
    {
      if (*c_ptr >= threshold) { err = *c_ptr - 255; data |= mask; } 
      else  err = *c_ptr;

      if (mask == mask_end) 
	{ *o_ptr++ = data^invert; data = 0x00; mask = mask_start; }
      else if (mask_start == 0x80) mask >>= 1;
           else mask <<= 1;
      c_ptr++;

      if (xa_dither_flag == xaFALSE)
      {
        if (x < (xsize - 1) ) *n_ptr++ = imap[ *in++ ];
      }
      else
      {
        if (x < (xsize - 1) )  *c_ptr += (7 * err)/16;
        if (y < (ysize - 1) )
        {
          if (x > 0) *n_ptr++ += (3 * err)/16;
          *n_ptr++ += (5 * err)/16;
          if (x < (xsize - 1) ) 
          { *n_ptr = (xaUSHORT)(imap[*in++]) + (xaSHORT)(err/16); n_ptr--; }
        }
      }
    }
    if (mask != mask_start) *o_ptr++ = data^invert; /* send out partial */

  }
  FREE(err_buff,0x300); err_buff=0;
  FREE(imap,0x301); imap=0;
}

/* Routine to read a little endian long word.
 */
xaULONG UTIL_Get_LSB_Long(fp)
FILE *fp;
{
 xaULONG ret;

 ret =  fgetc(fp);
 ret |= fgetc(fp) << 8;
 ret |= fgetc(fp) << 16;
 ret |= fgetc(fp) << 24;
 return ret;
}

/* Routine to read a little endian half word.
 */
xaULONG UTIL_Get_LSB_Short(fp)
FILE *fp;
{
 xaULONG ret;

 ret =  fgetc(fp);
 ret |= fgetc(fp) << 8;
 return ret;
}

/* Routine to read a big endian long word.
 */
xaULONG UTIL_Get_MSB_Long(fp)
FILE *fp;
{
 xaULONG ret;

 ret  = fgetc(fp) << 24;
 ret |= fgetc(fp) << 16;
 ret |= fgetc(fp) << 8;
 ret |=  fgetc(fp);
 return ret;
}

/* Routine to read a big endian half word.
 */
xaLONG UTIL_Get_MSB_Short(fp)
FILE *fp;
{
 xaLONG ret;
 xaSHORT tmp;

 tmp  =  fgetc(fp) << 8;
 tmp |=  fgetc(fp);
 ret = tmp;
 return ret;
}

/* 
 * Routine to read a big endian half word.
 */
xaULONG UTIL_Get_MSB_UShort(fp)
FILE *fp;
{
 xaULONG ret;

 ret  =  fgetc(fp) << 8;
 ret |=  fgetc(fp);
 return ret;
}

/*
 *
 */
void UTIL_Mapped_To_Floyd(out,in,chdr_out,chdr_in,
		xipos,yipos,xosize,yosize,xisize,yisize)
xaUBYTE *out;		/* output (size of section) */
xaUBYTE *in;		/* input image */
XA_CHDR *chdr_out;	/* chdr to map to */
XA_CHDR *chdr_in;	/* input images chdr */
xaULONG xipos,yipos;	/* section position within input buffer */
xaULONG xosize, yosize;	/* section size */
xaULONG xisize, yisize;	/* input buffer size */
{
  xaULONG flag,csize_out,cflag;
  ColorReg *cmap_in,*cmap_out;
  register xaULONG x,y;
  xaULONG shift_r,shift_g,shift_b,coff_out;
  xaSHORT *err_buff0, *err_buff1, *e_ptr, *ne_ptr;
  xaSHORT r_err,g_err,b_err;
  xaUBYTE *o_ptr;

  cflag = (x11_display_type & XA_X11_COLOR)?xaTRUE:xaFALSE;

  if (!(x11_display_type & XA_X11_TRUE))
  {
    if (cmap_cache == 0) CMAP_Cache_Init(0);
    if (chdr_out != cmap_cache_chdr)
    {
      DEBUG_LEVEL2 fprintf(stderr,"CACHE: clear new_chdr %x\n",(xaULONG)chdr_out);
      CMAP_Cache_Clear();
      cmap_cache_chdr = chdr_out;
    }
    shift_r = 3 * cmap_cache_bits;
    shift_g = 2 * cmap_cache_bits;
    shift_b = cmap_cache_bits;
  }
  
  csize_out = chdr_out->csize;
  cmap_out = chdr_out->cmap;
  coff_out = chdr_out->coff;
 
  /* allocate error buffer and set up pointers */
  e_ptr = err_buff1 = err_buff0 = (xaSHORT *)malloc(6 * xosize * sizeof(xaSHORT));
  if (err_buff0 == 0) TheEnd1("UTIL_Mapped_To_Floyd: malloc err");
  err_buff1 = &err_buff0[(3 * xosize)]; /* POD verify */
  flag = 0;

  { /* POD NOTE: only do this with different cmaps */
    register xaULONG i,moff,msize,mcoff;
    moff = chdr_in->moff;
    mcoff = moff - chdr_in->coff;
    msize = chdr_in->msize;
    cmap_in = (ColorReg *)malloc((moff + msize) * sizeof(ColorReg));
    for(i=0; i<msize; i++)
    {
      cmap_in[i + moff].red   = chdr_in->cmap[i + mcoff].red >> 8;
      cmap_in[i + moff].green = chdr_in->cmap[i + mcoff].green >> 8;
      cmap_in[i + moff].blue  = chdr_in->cmap[i + mcoff].blue >> 8;
      cmap_in[i + moff].gray  = chdr_in->cmap[i + mcoff].gray >> 8;
    }
  }
  {
    register xaUBYTE *i_ptr = (xaUBYTE *)(in + (xisize * (yosize-1+yipos) + xipos));
    x = xosize; while(x--)
    {
      register xaULONG p = *i_ptr++;
      *e_ptr++ = (xaUSHORT)cmap_in[ p ].red   << 4;
      *e_ptr++ = (xaUSHORT)cmap_in[ p ].green << 4;
      *e_ptr++ = (xaUSHORT)cmap_in[ p ].blue  << 4;
    }
  }

  y = yosize; while(y--)
  {
    o_ptr = (xaUBYTE *)(out + (xosize * y * x11_bytes_pixel));
    if (flag) { e_ptr = err_buff1; ne_ptr = err_buff0; flag = 0; }
    else      { e_ptr = err_buff0; ne_ptr = err_buff1; flag = 1; }
    if (y > 0)
    {
      register xaSHORT *tptr = ne_ptr;
      register xaUBYTE *i_ptr = (xaUBYTE *)(in + (xisize * (y-1+yipos) + xipos));
      x = xosize; while(x--)
      {
	register xaULONG p = *i_ptr++;
	*tptr++ = (xaUSHORT)cmap_in[ p ].red   << 4;
	*tptr++ = (xaUSHORT)cmap_in[ p ].green << 4;
	*tptr++ = (xaUSHORT)cmap_in[ p ].blue  << 4;
      }
    }

    x = xosize; while(x--)
    {
      xaULONG color_out,pix_out;
      register xaSHORT r,g,b;

      r = (*e_ptr++)/16; if (r<0) r = 0; else if (r>255) r = 255;
      g = (*e_ptr++)/16; if (g<0) g = 0; else if (g>255) g = 255;
      b = (*e_ptr++)/16; if (b<0) b = 0; else if (b>255) b = 255;
     
      if (x11_display_type & XA_X11_TRUE)
      {
	xaULONG tr,tg,tb;
	pix_out = X11_Get_True_Color(r,g,b,8);
/* TODO: mismatched byte order may break this if on non depth 8 machines */
	tr = ((pix_out & x11_red_mask)   >> x11_red_shift);
	tg = ((pix_out & x11_green_mask) >> x11_green_shift);
	tb = ((pix_out & x11_blue_mask)  >> x11_blue_shift);
	r_err = r - (xaSHORT)(cmap_out[tr].red >> 8);
	g_err = g - (xaSHORT)(cmap_out[tg].green >> 8);
	b_err = b - (xaSHORT)(cmap_out[tb].blue >> 8);
      }
      else
      {
        register xaULONG cache_i;
        cache_i = (  ( (r << shift_r) & cmap_cache_rmask)
                   | ( (g << shift_g) & cmap_cache_gmask)
                   | ( (b << shift_b) & cmap_cache_bmask) ) >> 8;
	if (cmap_cache[cache_i] == 0xffff)
	{
	  color_out = CMAP_Find_Closest(cmap_out,csize_out,r,g,b,8,8,8,cflag);
	  cmap_cache[cache_i] = (xaSHORT)color_out;
	}
	else color_out = (xaULONG)cmap_cache[cache_i];
	pix_out = color_out + coff_out;
        r_err = r - (xaSHORT)(cmap_out[color_out].red >> 8);
        g_err = g - (xaSHORT)(cmap_out[color_out].green >> 8);
        b_err = b - (xaSHORT)(cmap_out[color_out].blue >> 8);
      }
 
      if (x11_bytes_pixel == 1) *o_ptr++ = (xaUBYTE)pix_out;
      else if (x11_bytes_pixel == 2)
      { xaUSHORT *so_ptr = (xaUSHORT *)(o_ptr);
        *so_ptr = (xaUSHORT)pix_out; o_ptr +=2; }
      else { xaULONG *lo_ptr = (xaULONG *)(o_ptr);
             *lo_ptr = (xaULONG)pix_out; o_ptr +=4; }

      if (x)
      {
	*e_ptr   += 7 * r_err;
	e_ptr[1] += 7 * g_err;
	e_ptr[2] += 7 * b_err;
      }
      if (y)
      {
        if (x < (xosize-1) ) /* not 1st of line */
	{
	  *ne_ptr++ += 3 * r_err;
	  *ne_ptr++ += 3 * g_err;
	  *ne_ptr++ += 3 * b_err;
	}
        *ne_ptr   += 5 * r_err;
        ne_ptr[1] += 5 * g_err;
        ne_ptr[2] += 5 * b_err;
        if (x)
        {
	  ne_ptr[3] += r_err;
	  ne_ptr[4] += g_err;
	  ne_ptr[5] += b_err;
	}
      }
    } /* end of x */
  } /* end of y */
  if (err_buff0) FREE(err_buff0,0x302);
  if (cmap_in) FREE(cmap_in,0x303);
  err_buff0 = 0; cmap_in = 0;
}

/*
 * Function to Scale <x,y> positions and sizes
 *
 */

void
UTIL_Scale_Pos_Size(xpos,ypos,xsize,ysize,xi_scale,yi_scale,
					xo_scale,yo_scale)
xaLONG *xpos,*ypos;	 /* input/output positions */
xaLONG *xsize,*ysize;	 /* input/output sizes */
xaLONG xi_scale,yi_scale; /* input scale/image size */
xaLONG xo_scale,yo_scale; /* output scale/image size */
{
  xaLONG ixp,iyp,oxp,oyp;
  xaLONG ixsize,iysize,oxsize,oysize;

  ixp = *xpos;      iyp = *ypos;
  ixsize = *xsize;  iysize = *ysize;

  oxp = (ixp * xo_scale) / xi_scale;
  if ( ((oxp * xi_scale) / xo_scale) < ixp ) oxp++;
  oxsize = (((ixp + ixsize) * xo_scale) / xi_scale) + 1 - oxp;
  if ( (((oxp+oxsize-1) * xi_scale) / xo_scale) >= (ixp+ixsize) ) oxsize--;

  oyp = (iyp * yo_scale) / yi_scale;
  if ( ((oyp * yi_scale) / yo_scale) < iyp ) oyp++;
  oysize = (((iyp + iysize) * yo_scale) / yi_scale) + 1 - oyp;
  if ( (((oyp+oysize-1) * yi_scale) / yo_scale) >= (iyp+iysize) ) oysize--;

  *xpos = oxp;		*ypos = oyp;
  *xsize = oxsize;	*ysize = oysize;
}


/*
 * Function to Scale <x,y> positions
 *
 *  new <x,y> positions returned in xopos,yopos;
 */

void
UTIL_Scale_Buffer_Pos(xpos,ypos,xi_scale,yi_scale,
					xo_scale,yo_scale)
xaLONG *xpos,*ypos;	 /* input/output positions */
xaLONG xi_scale,yi_scale; /* input scale/image size */
xaLONG xo_scale,yo_scale; /* output scale/image size */
{
  xaLONG ixp,iyp,oxp,oyp;

  ixp = *xpos; iyp = *ypos;

  oxp = (ixp * xo_scale) / xi_scale;
  if ( ((oxp * xi_scale) / xo_scale) < ixp ) oxp++;

  oyp = (iyp * yo_scale) / yi_scale;
  if ( ((oyp * yi_scale) / yo_scale) < iyp ) oyp++;

  *xpos = oxp;		*ypos = oyp;
}


/*
 * Function to get Scaling Buffer Coordinates.
 * 	returns xaTRUE if need to scale while buffering
 *	returns xaFALSE if you don't 
 *
 * NOTE: This should only be called within XXX_Read_File or else
 *       scale isn't guaranteed to be valid.
 * ACT_Setup_Mapped is okay since it has the same limitations.
 */
xaULONG UTIL_Get_Buffer_Scale(width,height,buffx_o,buffy_o)
xaULONG width,height;	 /* width, height of anim */
xaULONG *buffx_o,*buffy_o; /* scaled width, height of anim */
{
 xaULONG need_to_scale;
 xaULONG buff_x,buff_y;

  /* Figure out buffer scaling */
  if ( (xa_buff_x != 0) && (xa_buff_y != 0) )
        { buff_x = xa_buff_x; buff_y = xa_buff_y; }
  else if (xa_buff_x != 0) /* if just x, then adjust y */
        { buff_x = xa_buff_x; buff_y = (height * xa_buff_x) / width; }
  else if (xa_buff_y != 0) /* if just y, then adjust x */
        { buff_y = xa_buff_y; buff_x = (width * xa_buff_y) / height; }
  else
  {
     /* handle any scaling */
    buff_x = (xaULONG)((float)(width) * xa_bscalex);
    if (buff_x == 0) buff_x = width;
    buff_y = (xaULONG)((float)(height) * xa_bscaley);
    if (buff_y == 0) buff_y = height;
  }
  if ((buff_x != width) || (buff_y != height)) need_to_scale = xaTRUE;
  else need_to_scale = xaFALSE;
  *buffx_o = buff_x; *buffy_o = buff_y;
  return(need_to_scale);
}

/*
 * Function to allocate buffer for Scaling
 *
 */
xaUBYTE *UTIL_Alloc_Scaled(xi_size,yi_size,xi_scale,yi_scale,
	xo_scale,yo_scale,pix_size,xo_pos,yo_pos,bitmap_flag)
xaULONG xi_size,yi_size;   /* size of section */
xaULONG xi_scale,yi_scale; /* size of full input image */
xaULONG xo_scale,yo_scale; /* size of full output image */
xaULONG pix_size;          /* pixel size */
xaULONG xo_pos,yo_pos;     /* pos of section in input buffer */
xaULONG bitmap_flag;	 /* if true, then alloc for a bitmap */

{
  xaULONG sx,sy,xsize,ysize;
  xaUBYTE *out;

  sx = (xo_pos * xo_scale) / xi_scale;
  if ( ((sx * xi_scale) / xo_scale) < xo_pos ) sx++;
  xsize = (((xo_pos + xi_size) * xo_scale) / xi_scale) + 1 - sx;
  if ( (((sx+xsize-1) * xi_scale) / xo_scale) >= (xo_pos+xi_size) ) xsize--;

  sy = (yo_pos * yo_scale) / yi_scale;
  if ( ((sy * yi_scale) / yo_scale) < yo_pos ) sy++;
  ysize = (((yo_pos + yi_size) * yo_scale) / yi_scale) + 1 - sy;
  if ( (((sy+ysize-1) * yi_scale) / yo_scale) >= (yo_pos+yi_size) ) ysize--;

  if ( (xsize==0) || (ysize==0) )
  {
    DEBUG_LEVEL1 fprintf(stderr,"SCALE: zero size %d %d\n",xsize,ysize);
    return(0);
  }

DEBUG_LEVEL1 fprintf(stderr,"AllocScaled: xy %d %d  t %d\n",
  xsize,ysize,(xsize * ysize * pix_size));

  if (bitmap_flag==xaTRUE)
  {
    xaULONG line_size;
    line_size = X11_Get_Bitmap_Width(xsize) / 8;
    out = (xaUBYTE *)malloc(line_size * ysize);
  }
  else out = (xaUBYTE *)malloc(xsize * ysize * pix_size);
  if (out==0) TheEnd1("UTIL_Alloc_Scaled: malloc err");
  return(out);
}

/*
 * Function to Scale a Mapped Image 
 *
 */
xaUBYTE *UTIL_Scale_Mapped(out,in,xi_pos,yi_pos,xi_size,yi_size,
	xi_buff_size,xi_scale,yi_scale,xo_scale,yo_scale,pix_size,
	xo_pos,yo_pos,xo_size,yo_size,chdr)
xaUBYTE *out;		 /* output buffer, if 0 then one is created */
xaUBYTE *in;		 /* input image buffer */
xaULONG xi_pos,yi_pos;     /* pos of section in input buffer */
xaULONG xi_size,yi_size;   /* size of section */
xaULONG xi_buff_size;      /* row size of input buffer in bytes */
xaULONG xi_scale,yi_scale; /* size of full input image */
xaULONG xo_scale,yo_scale; /* size of full output image */
xaULONG pix_size;		 /* pixel size */
xaULONG *xo_pos,*yo_pos;   /* I screen pos/O pos of section in output buffer */
xaULONG *xo_size,*yo_size; /* O size of outgoing section */
XA_CHDR *chdr;
/* note: pos of section in output buffer is always 0,0 */
{
  xaULONG i,y,sx,sy,xvp,yvp,xsize,ysize,xoff;


  xvp = *xo_pos; yvp = *yo_pos;
  sx = (xvp * xo_scale) / xi_scale;
  if ( ((sx * xi_scale) / xo_scale) < xvp ) sx++;
  xsize = (((xvp + xi_size) * xo_scale) / xi_scale) + 1 - sx;
  if ( (((sx+xsize-1) * xi_scale) / xo_scale) >= (xvp+xi_size) ) xsize--;

  sy = (yvp * yo_scale) / yi_scale;
  if ( ((sy * yi_scale) / yo_scale) < yvp ) sy++;
  ysize = (((yvp + yi_size) * yo_scale) / yi_scale) + 1 - sy;
  if ( (((sy+ysize-1) * yi_scale) / yo_scale) >= (yvp+yi_size) ) ysize--;

  xoff = xvp - xi_pos;
  if ( (xsize==0) || (ysize==0) ) 
  {
    DEBUG_LEVEL1 fprintf(stderr,"SCALE: zero size %d %d\n",xsize,ysize);
    *xo_pos = *yo_pos = *xo_size = *yo_size = 0;
    return(0);
  }

  /* use scale buffer if necessary */
  if (out == 0)
  {
    xaULONG tsize = xsize * ysize * pix_size;
    XA_REALLOC(xa_scale_buff,xa_scale_buff_size,tsize);
    out = (xaUBYTE *)xa_scale_buff;
  }

  /* readjust row cache buffer if necessary */
  if (xsize > xa_scale_row_size)
  {
    xaULONG *tmp;
    if (xa_scale_row_buff == 0) tmp = (xaULONG *) malloc(xsize * sizeof(xaULONG) );
    else tmp = (xaULONG *) realloc(xa_scale_row_buff, xsize * sizeof(xaULONG) );
    if (tmp == 0) TheEnd1("UTIL_Scale_Mapped: row malloc err\n");
    xa_scale_row_buff = tmp;	xa_scale_row_size = xsize;
  }

  y = sx; for(i = 0; i < xsize; i++,y++)
	xa_scale_row_buff[i] = ((y * xi_scale) / xo_scale ) - xoff;

  if (chdr==0) /* no mapping */
  {
    register xaULONG x,yoff;
    yoff = yvp - yi_pos;
    if (pix_size == 4)
    {
      register xaULONG *optr = (xaULONG *)out;
      for(y=sy; y< (sy + ysize); y++)
      { register xaULONG *in_r_ptr = 
		(xaULONG *)(in+(((y*yi_scale)/yo_scale)-yoff)*xi_buff_size);
        for(x = 0; x < xsize; x++) *optr++ = in_r_ptr[ xa_scale_row_buff[x] ];
      }
    }
    else if (pix_size == 2)
    {
      register xaUSHORT *optr = (xaUSHORT *)out;
      for(y=sy; y< (sy + ysize); y++)
      { register xaUSHORT *in_r_ptr = 
		(xaUSHORT *)(in+(((y*yi_scale)/yo_scale)-yoff)*xi_buff_size);
        for(x = 0; x < xsize; x++) *optr++ = in_r_ptr[ xa_scale_row_buff[x] ];
      }
    }
    else
    {
      register xaUBYTE *optr = (xaUBYTE *)out;
      for(y=sy; y< (sy + ysize); y++)
      { register xaUBYTE *in_r_ptr = 
		(xaUBYTE *)(in+(((y*yi_scale)/yo_scale)-yoff)*xi_buff_size);
        for(x = 0; x < xsize; x++) *optr++ = in_r_ptr[ xa_scale_row_buff[x] ];
      }
    }
  }
  else /* remap as we scale */
  {
    register xaULONG x,yoff,moff,*map;
    map  = chdr->map; moff = chdr->moff; yoff = yvp - yi_pos;
    if (pix_size == 4)
    {
      register xaULONG *optr = (xaULONG *)out;
      for(y=sy; y< (sy + ysize); y++)
      { register xaUBYTE *in_r_ptr = 
		(xaUBYTE *)(in+(((y*yi_scale)/yo_scale)-yoff)*xi_buff_size);
	if (moff) for(x = 0; x < xsize; x++) 
		*optr++ = (xaULONG)map[in_r_ptr[ xa_scale_row_buff[x]]-moff];
	else for(x = 0; x < xsize; x++) 
		*optr++ = (xaULONG)map[in_r_ptr[ xa_scale_row_buff[x]]];
      }
    }
    else if (pix_size == 2)
    {
      register xaUSHORT *optr = (xaUSHORT *)out;
      for(y=sy; y< (sy + ysize); y++)
      { register xaUBYTE *in_r_ptr = 
		(xaUBYTE *)(in+(((y*yi_scale)/yo_scale)-yoff)*xi_buff_size);
	if (moff) for(x = 0; x < xsize; x++) 
		*optr++ = (xaUSHORT)map[in_r_ptr[ xa_scale_row_buff[x]]-moff];
	else for(x = 0; x < xsize; x++) 
		*optr++ = (xaUSHORT)map[in_r_ptr[ xa_scale_row_buff[x]]];
      }
    }
    else
    {
      register xaUBYTE *optr = (xaUBYTE *)out;
      for(y=sy; y< (sy + ysize); y++)
      { register xaUBYTE *in_r_ptr = 
		(xaUBYTE *)(in+(((y*yi_scale)/yo_scale)-yoff)*xi_buff_size);
	if (moff) for(x = 0; x < xsize; x++) 
		*optr++ = (xaUBYTE)map[in_r_ptr[ xa_scale_row_buff[x]]-moff];
	else for(x = 0; x < xsize; x++) 
		*optr++ = (xaUBYTE)map[in_r_ptr[ xa_scale_row_buff[x]]];
      }
    }
  }
  *yo_pos = sy; *yo_size = ysize;
  *xo_pos = sx; *xo_size = xsize;
  return(out);
}


/*
 * for now assum xi_pos=yi_pos=0 and size are multiple of 8s 
 *
 */
xaUBYTE *UTIL_Scale_Bitmap(out,in,xi_size,yi_size,
	xi_line_size,xi_scale,yi_scale,xo_scale,yo_scale,
	xo_pos,yo_pos,xo_size,yo_size,ibit_order,obit_order)
xaUBYTE *out,*in;
xaULONG xi_size,yi_size;   /* size of section */
xaULONG xi_line_size;      /* row size of input buffer in bytes*/
xaULONG xi_scale,yi_scale; /* size of full input image */
xaULONG xo_scale,yo_scale; /* size of full output image */
xaULONG *xo_pos,*yo_pos;   /* pos of section in input/output buffer */
xaULONG *xo_size,*yo_size; /* size of outgoing section */
xaULONG ibit_order;	 /* input bit_order */
xaULONG obit_order;	 /* output bit_order */
{
  xaULONG xo_line_size;
  xaULONG i,y,sx,sy,xvp,yvp,xsize,ysize;
  xaULONG *row_b_ptr;
  xaUBYTE *row_msk_ptr,mask_start,mask_end;

  xvp = *xo_pos; yvp = *yo_pos;
  sx = (xvp * xo_scale) / xi_scale;
  if ( ((sx * xi_scale) / xo_scale) < xvp ) sx++;
  xsize = (((xvp + xi_size) * xo_scale) / xi_scale) + 1 - sx;
  if ( (((sx+xsize-1) * xi_scale) / xo_scale) >= (xvp+xi_size) ) xsize--;

  sy = (yvp * yo_scale) / yi_scale;
  if ( ((sy * yi_scale) / yo_scale) < yvp ) sy++;
  ysize = (((yvp + yi_size) * yo_scale) / yi_scale) + 1 - sy;
  if ( (((sy+ysize-1) * yi_scale) / yo_scale) >= (yvp+yi_size) ) ysize--;

DEBUG_LEVEL1 fprintf(stderr,"SCALE BITMAP: siz %dx%d -> %dx%d\n",
	xi_size,yi_size,xsize,ysize);
DEBUG_LEVEL1 fprintf(stderr,"SCALE BITMAP: pos %dx%d -> %dx%d\n",
	xvp,yvp,sx,sy);

  if ( (xsize==0) || (ysize==0) ) 
  {
    DEBUG_LEVEL1 fprintf(stderr,"SCALE: zero size %d %d\n",xsize,ysize);
    *xo_size = *yo_size = 0;
    return(0);
  }
  xo_line_size = X11_Get_Bitmap_Width(xsize) / 8;
  /* use scale buffer if necessary */
  if (out == 0)
  {
    xaULONG tsize = ysize * xo_line_size;
    XA_REALLOC(xa_scale_buff,xa_scale_buff_size,tsize);
    out = (xaUBYTE *)xa_scale_buff;
  }

  /* readjust row cache buffer if necessary */
  if ( (xsize<<1) > xa_scale_row_size)
  {
    xaULONG *tmp;
    if (xa_scale_row_buff == 0)
	tmp = (xaULONG *) malloc((xsize<<1) * sizeof(xaULONG) );
    else
	tmp = (xaULONG *) realloc(xa_scale_row_buff,(xsize<<1)*sizeof(xaULONG));
    if (tmp == 0) TheEnd1("UTIL_Scale_Mapped: row malloc err\n");
    xa_scale_row_buff = tmp;    xa_scale_row_size = (xsize<<1);
  }
  row_b_ptr = xa_scale_row_buff;
  row_msk_ptr = (xaUBYTE *)&xa_scale_row_buff[xsize];

  y = sx;
  for(i = 0; i < xsize; i++,y++)
  {
    register xaULONG x = ((y * xi_scale) / xo_scale) - xvp;
    row_b_ptr[i] = x/8;
    if (ibit_order == XA_LSBIT_1ST) row_msk_ptr[i] = 0x01 << (x%8);
    else row_msk_ptr[i] = 0x01 << (7 - (x%8));
  }
  if (obit_order == XA_MSBIT_1ST) { mask_start = 0x80; mask_end = 0x01; }
  else { mask_start = 0x01; mask_end = 0x80; }
  for(y=sy; y < (sy+ysize);y++)
  {
    register xaULONG x;
    register xaUBYTE omsk,odat;
    register xaUBYTE *iptr,*optr;
    iptr = (xaUBYTE *)(in + (((y * yi_scale) / yo_scale)-yvp) * xi_line_size);
    optr = (xaUBYTE *)(out + ((y - sy) * xo_line_size));
    omsk = mask_start; odat = 0;
    for(x=0; x<xsize; x++)
    {
      if ( iptr[ row_b_ptr[x] ] & row_msk_ptr[x] ) odat |= omsk;
      if (omsk == mask_end) { *optr++ = odat; omsk = mask_start; odat = 0; }
      else if (mask_start == 0x01) omsk <<= 1; else omsk >>= 1;
    }
    if (omsk) *optr++ = odat;
  }
  *xo_pos = sx;     *yo_pos = sy;
  *xo_size = xsize; *yo_size = ysize;
  return(out);
}

/**********************
 * UTIL_RGB_To_Map
 *
 **********************/
xaUBYTE *UTIL_RGB_To_Map(out,in,chdr,xsize,ysize,free_in_flag)
xaUBYTE *out,*in;
XA_CHDR *chdr;
xaULONG xsize,ysize;
xaULONG free_in_flag;
{
  xaULONG i,shift_r,shift_g,shift_b,pic_size;
  xaUBYTE *t_pic,*iptr,*optr;
  xaULONG csize,coff;
 
  DEBUG_LEVEL1 fprintf(stderr,"UTIL_RGB_To_Map\n");
  pic_size = xsize * ysize;
  if (out == 0)
  {
    t_pic = (xaUBYTE *)malloc( XA_PIC_SIZE(pic_size) );
    if (t_pic == 0) TheEnd1("RGB: t_pic malloc err");
  } else t_pic = out;

  if (cmap_cache == 0) CMAP_Cache_Init(0);
  if (chdr != cmap_cache_chdr)
  {
    DEBUG_LEVEL2 fprintf(stderr,"CACHE: clear new_chdr %x\n",
                                                        (xaULONG)chdr);
    CMAP_Cache_Clear();
    cmap_cache_chdr = chdr;
  }
  shift_r = 3 * cmap_cache_bits;
  shift_g = 2 * cmap_cache_bits;
  shift_b = cmap_cache_bits;


  csize = chdr->csize;
  coff = chdr->coff;
  iptr = in;
  optr = t_pic;
  for(i=0; i < pic_size; i++)
  {
    register xaULONG color_out,r,g,b;
    register xaULONG cache_i;

    r = (xaULONG)(*iptr++); g = (xaULONG)(*iptr++); b = (xaULONG)(*iptr++);
    cache_i  = (xaULONG)(  (  ( (r << shift_r) & cmap_cache_rmask)
                          | ( (g << shift_g) & cmap_cache_gmask)
                          | ( (b << shift_b) & cmap_cache_bmask)
                         ) >> 8);
    if (cmap_cache[cache_i] == 0xffff)
    {
      color_out = coff +
                CMAP_Find_Closest(chdr->cmap,csize,r,g,b,8,8,8,xaTRUE);
      cmap_cache[cache_i] = (xaUSHORT)color_out;
    }
    else color_out = (xaULONG)cmap_cache[cache_i];

    *optr++ = (xaUBYTE)(color_out);

  }
  if (free_in_flag == xaTRUE) FREE(in,0x304); in=0;
  return(t_pic);
}

/**********************
 *
 **********************/
xaUBYTE *UTIL_RGB_To_FS_Map(out,in,chdr,xsize,ysize,free_in_flag)
xaUBYTE *out,*in;
XA_CHDR *chdr;
xaULONG xsize,ysize;
xaULONG free_in_flag;
{
  xaULONG flag,csize,xsize3;
  ColorReg *cmap;
  register xaULONG x,y;
  xaULONG shift_r,shift_g,shift_b,coff;
  xaSHORT *err_buff0, *err_buff1, *e_ptr, *ne_ptr;
  xaSHORT r_err,g_err,b_err;
  xaUBYTE *o_ptr,*tpic;

  DEBUG_LEVEL1 fprintf(stderr,"UTIL_RGB_To_FS_Map\n");

  if (out == 0)
  {
    tpic = (xaUBYTE *)malloc( XA_PIC_SIZE(xsize * ysize) );
    if (tpic == 0) TheEnd1("UTIL_RGB_To_FS_Map: malloc err");
  } else tpic = out;

  if (cmap_cache == 0) CMAP_Cache_Init(0);

  if (chdr != cmap_cache_chdr)
  {
    DEBUG_LEVEL2 fprintf(stderr,"CACHE: clear new_chdr %x\n",(xaULONG)chdr);
    CMAP_Cache_Clear();
    cmap_cache_chdr = chdr;
  }
  shift_r = 3 * cmap_cache_bits;
  shift_g = 2 * cmap_cache_bits;
  shift_b = cmap_cache_bits;
  
  csize = chdr->csize;
  cmap = chdr->cmap;
  coff = chdr->coff;
 
  /* allocate error buffer and set up pointers */
  xsize3 = xsize * 3;
  e_ptr = err_buff0 = (xaSHORT *)malloc(6 * xsize * sizeof(xaSHORT) );
  if (err_buff0 == 0) TheEnd1("UTIL_Mapped_To_Floyd: malloc err");
  err_buff1 = &err_buff0[xsize3];
  flag = 0;

  {
    register xaUBYTE *i_ptr = (xaUBYTE *)( in + (xsize3 * (ysize-1)));
    x = xsize; while(x--)
    {
      *e_ptr++ = (xaUSHORT)(*i_ptr++) << 4;
      *e_ptr++ = (xaUSHORT)(*i_ptr++) << 4;
      *e_ptr++ = (xaUSHORT)(*i_ptr++) << 4;
    }
  }

  y = ysize; while(y--)
  {
    o_ptr = (xaUBYTE *)(tpic + (xsize * y));
    if (flag) { e_ptr = err_buff1; ne_ptr = err_buff0; flag = 0; }
    else      { e_ptr = err_buff0; ne_ptr = err_buff1; flag = 1; }
    if (y > 0)
    {
      register xaSHORT *tptr = ne_ptr;
      register xaUBYTE *i_ptr = (xaUBYTE *)(in + (xsize3 * (y-1)));
      x = xsize; while(x--)
      {
	*tptr++ = (xaUSHORT)(*i_ptr++) << 4;
	*tptr++ = (xaUSHORT)(*i_ptr++) << 4;
	*tptr++ = (xaUSHORT)(*i_ptr++) << 4;
      }
    }

    x = xsize; while(x--)
    {
      xaULONG color_out;
      register xaSHORT r,g,b;
      register xaULONG cache_i;

      r = (*e_ptr++)/16; if (r<0) r = 0; else if (r>255) r = 255;
      g = (*e_ptr++)/16; if (g<0) g = 0; else if (g>255) g = 255;
      b = (*e_ptr++)/16; if (b<0) b = 0; else if (b>255) b = 255;
     
      cache_i = (  ( (r << shift_r) & cmap_cache_rmask)
                 | ( (g << shift_g) & cmap_cache_gmask)
                 | ( (b << shift_b) & cmap_cache_bmask) ) >> 8;

      if (cmap_cache[cache_i] == 0xffff)
      {
        color_out = CMAP_Find_Closest(cmap,csize,r,g,b,8,8,8,xaTRUE);
        cmap_cache[cache_i] = (xaSHORT)color_out;
      }
      else color_out = (xaULONG)cmap_cache[cache_i];
      
      *o_ptr++ = (xaUBYTE)(color_out + coff);

      r_err = r - (xaSHORT)(cmap[color_out].red >> 8);
      g_err = g - (xaSHORT)(cmap[color_out].green >> 8);
      b_err = b - (xaSHORT)(cmap[color_out].blue >> 8);

      if (x)
      {
	*e_ptr   += 7 * r_err;
	e_ptr[1] += 7 * g_err;
	e_ptr[2] += 7 * b_err;
      }
      if (y)
      {
        if (x < (xsize-1) ) /* not 1st of line */
	{
	  *ne_ptr++ += 3 * r_err;
	  *ne_ptr++ += 3 * g_err;
	  *ne_ptr++ += 3 * b_err;
	}
        *ne_ptr   += 5 * r_err;
        ne_ptr[1] += 5 * g_err;
        ne_ptr[2] += 5 * b_err;
        if (x)
        {
	  ne_ptr[3] += r_err;
	  ne_ptr[4] += g_err;
	  ne_ptr[5] += b_err;
	}
      }
    } /* end of x */
  } /* end of y */
  if (err_buff0) { FREE(err_buff0,0x305); err_buff0=0; }
  if (free_in_flag == xaTRUE) FREE(in,0x306);
  return(tpic);
}

void
UTIL_Pack_Image(out,in,xsize,ysize)
xaUBYTE *out;
xaUBYTE *in;
xaULONG xsize,ysize;
{
  xaUBYTE *o_ptr;
  xaULONG x,y,shft_msk;
  xaLONG shft_start,shft_end,shft_inc;

  if (x11_bits_per_pixel == 4)
  { shft_msk = 0x0f;
    if (x11_byte_order == XA_MSBIT_1ST)
			{shft_start=4; shft_end=0; shft_inc= -4;}
    else		{shft_start=0; shft_end=4; shft_inc=  4;}
  }
  else if (x11_bits_per_pixel == 2)
  { shft_msk = 0x03;
    if (x11_byte_order == XA_MSBIT_1ST)
			{shft_start=6; shft_end=0; shft_inc= -2;}
    else		{shft_start=0; shft_end=6; shft_inc=  2;}
  }
  else /* unsupported depth */
  {
    fprintf(stderr,"X11 Packing of bits %d to depth %d not yet supported\n",
	x11_bits_per_pixel,x11_depth);
    return;
  }

  o_ptr = out;
  for(y=0;y<ysize;y++)
  { register xaULONG data = 0;
    register xaLONG shft  = shft_start;
    for(x=0; x<xsize; x++)
    {
	if (shft != shft_end) /* still creating byte */
	{ data |= ((*in++) & shft_msk) << shft; 
	  shft += shft_inc;
	}
	else /* at the end */
	{
	  *o_ptr++ = data | (((*in++) & shft_msk) << shft);
	  shft = shft_start;
	  data = 0;
	} 
    }
    if (shft != shft_start) *o_ptr++ = data; /* partial byte */
  } /* end of y loop */
}

/**************************
 *
 * Convert frames per second into vid_time(ms) 
 * and vid_timelo(fractional ms).
 *
 * Global Variable: xa_jiffy_flag   non-zero then IS ms to use.
 ***********************/
void UTIL_FPS_2_Time(anim,fps)
XA_ANIM_SETUP *anim;
double fps;
{
  if (xa_jiffy_flag) { anim->vid_time = xa_jiffy_flag; anim->vid_timelo = 0; }
  { double ptime = (double)(1000.0) / fps;
    anim->vid_time = (xaULONG)(ptime);
    ptime -= (double)(anim->vid_time);
    anim->vid_timelo = (xaULONG)(ptime * (double)(1<<24));
  }
}

/**************************
 * Index
 * Searches string "s" for character "c" starting at the
 * beginning of the string.
 **************************/
char *XA_index(s,c)
char *s,c;
{ int len;
  char *p = s;

  if (s == (char *)(NULL)) return((char *)(NULL));
  len = strlen(s);
  while(len >= 0)
  { if (*p == c) return(p);
    else {p++; len--;}
  }
  return( (char *)(NULL) );
}

/**************************
 * Reverse Index
 * Searches string "s" for character "c" starting at the 
 * end of the string.
 **************************/
char *XA_rindex(s,c)
char *s,c;
{ int len;
  char *p = s;

  if (c == 0) return((char *)(NULL));
  if (s == (char *)(NULL)) return((char *)(NULL));
  len = strlen(s);
  p += len;
  while(len >= 0)
  { if (*p == c) return(p);
    else {p--; len--;}
  }
  return( (char *)(NULL) );
}

/**************************
 * Return xaTRUE is key is inside s, else return xaFALSE
 **************************/
xaULONG XA_find_str(s,key)
char *s, *key;
{ xaULONG key_len;
  char first = *key;
  char *tmp_s = s;

  if (key == 0) return(xaFALSE);
  key_len = strlen(key);
  while(s)
  { tmp_s = XA_index(s,first);
    if (tmp_s) 
    { 
      if (strncmp(tmp_s,key,key_len) == 0) return(xaTRUE); 
      s = tmp_s; s++;
    }
    else break;
  }
  return(xaFALSE);
}

/****----------------------------------------------------------------****
 *
 ****----------------------------------------------------------------****/
char *XA_white_out(s)
char *s;
{ char *tp;
  if ((s==0) || (*s==0)) return( (char *)(0) ); 

	/* strip white space off the end */
  tp = &s[ strlen(s) - 1 ];
  while((tp != s) && isspace((int)(*tp))) { *tp = 0; tp--; }

	/* strip white space from the front */
  tp = s;
  while( *tp && isspace((int)(*tp))) { tp++; }
  return(tp);
}

/****----------------------------------------------------------------****
 * Print out a string, indenting with "indent" if there is an embedded
 * newline.
 ****----------------------------------------------------------------****/
void XA_Indent_Print(fout, indent, s, first)
FILE	*fout;
char	*indent;
char	*s;
int	first;
{ char *p = s;
  int ret = 0;
  if (first) fprintf(fout,"%s",indent);
  while( p && *p)
  { fputc( (int)(*p), fout);
    ret = (*p =='\n')?(1):(0);
    p++;
    if (ret && (p[1] != 0)) fprintf(fout,"%s",indent);
  }
  if (ret == 0) fprintf(fout,"\n");
}



