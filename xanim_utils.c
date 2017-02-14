
/*
 * xanim_utils.c
 *
 * Copyright (C) 1991,1992,1993,1994,1995 by Mark Podlipec. 
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
/* revhist
 * 12dec94  +bCdm caused core on Alpha OSF/1 v3.0. 
 *           found and fixed by David Mosberger-Tang.
 *
 */ 
#include "xanim.h"

void UTIL_Sub_Image();
void UTIL_Create_Clip();
void UTIL_Mapped_To_Mapped();
void UTIL_Mapped_To_Bitmap();
ULONG UTIL_Get_Buffer_Scale();
void UTIL_Scale_Buffer_Pos();
void UTIL_Scale_Pos_Size();
UBYTE *UTIL_Alloc_Scaled();
UBYTE *UTIL_Scale_Mapped();
UBYTE *UTIL_Scale_Bitmap();
void UTIL_Pack_Image();

ULONG UTIL_Get_LSB_Long();
ULONG UTIL_Get_LSB_Short();
ULONG UTIL_Get_MSB_Long();
ULONG UTIL_Get_MSB_UShort();
LONG UTIL_Get_MSB_Short();
void UTIL_Mapped_To_Floyd();
LONG CMAP_Find_Closest();
void CMAP_Cache_Init();
void CMAP_Cache_Clear();

extern ULONG *xa_scale_row_buff;
extern ULONG xa_scale_row_size;
extern char *xa_scale_buff;
extern ULONG xa_scale_buff_size;
extern LONG xa_dither_flag;
extern ULONG xa_buff_x,xa_buff_y;
extern float xa_bscalex,xa_bscaley;


void
UTIL_Sub_Image(out,in,xosize,yosize,xipos,yipos,xisize,pix_size)
UBYTE *out,*in;
ULONG xosize,yosize,xipos,yipos,xisize,pix_size;
{
  ULONG y,x;
  UBYTE *in_ptr;
  xosize *= pix_size;
  xisize *= pix_size;
  xipos  *= pix_size;
  for(y=yipos; y < (yipos + yosize); y++)
  {
    in_ptr = (UBYTE *)(in + (y * xisize + xipos));
    x = xosize;
    while(x--) *out++ = *in_ptr++;
  } 
}

void
UTIL_Mapped_To_Mapped(out,in,chdr,xipos,yipos,xosize,yosize,xisize,yisize)
UBYTE *out;		/* output image - maybe the same as input image */
UBYTE *in;		/* input image */
XA_CHDR *chdr;		/* color header to remap to */
ULONG xipos,yipos;	/* pos of section in input buffer */
ULONG xosize,yosize;	/* size of section in input buffer */
ULONG xisize,yisize;	/* size of input buffer */
{
  ULONG *map,moff;
  ULONG y,x;
  UBYTE *in_ptr;

  map = chdr->map;
  moff = chdr->moff;
  switch(x11_bytes_pixel)
  {
   case 4: /* start from backside to allow for when out = in */
	{
	  ULONG *ulp;
	  for(y=yosize; y > 0; y--)
	  {
	    in_ptr = (UBYTE *)(in + (y + yipos - 1) * xisize 
						+ xipos + xosize - 1 );
	    ulp =    (ULONG *)(out + ((4 * y * xosize) - 4));
	    x = xosize;
	    if (moff) while(x--) *ulp-- = map[(*in_ptr--)-moff];
	    else while(x--) *ulp-- = map[*in_ptr--];
	  }
	}
	break;
   case 2: /* start from backside to allow for when out = in */
	{
	  USHORT *usp;
	  for(y=yosize; y > 0; y--)
	  {
	    in_ptr = (UBYTE *)(in + (y + yipos - 1) * xisize 
						+ xipos + xosize - 1 );
	    usp =    (USHORT *)(out + ((2 * y * xosize) - 2));
	    x = xosize;
	    if (moff) while(x--) *usp-- = (USHORT)map[(*in_ptr--)-moff];
	    else while(x--) *usp-- = (USHORT)map[*in_ptr--];
	  }
	}
	break;
   case 1:
	{
	  for(y=yipos; y < (yipos + yosize); y++)
	  {
	    in_ptr = (UBYTE *)(in + y * xisize + xipos);
	    x = xosize;
	    if (moff) while(x--) *out++ = (UBYTE)(map[(*in_ptr++)-moff]);
	    else while(x--) *out++ = (UBYTE)(map[*in_ptr++]);
	  }
	}
	break;
  } /* end switch */ 
}

void
UTIL_Create_Clip(out,in,pix_mask,xsize,ysize,pix_size,xosize,bit_order)
UBYTE *out,*in;
ULONG pix_mask,xsize,ysize,pix_size,xosize,bit_order;
{
  register ULONG data_in,bit_mask,x,y,mask_start,mask_end;
  register UBYTE data_out;

  if (bit_order == X11_MSB) { mask_start = 0x80; mask_end = 0x01; }
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
	  { ULONG *ulp = (ULONG *)in;	data_in = *ulp; }
        else if (pix_size == 2)
	  { USHORT *usp = (USHORT *)in;	data_in = (ULONG)(*usp); }
        else				data_in = (ULONG)(*in);
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
UBYTE *out;		/* output buffer */
UBYTE *in;		/* input buffer */
XA_CHDR *chdr;		/* Color Hdr to use */
ULONG xpos,ypos;	/* pos of section in input buffer */
ULONG xsize, ysize;	/* size of section in input buffer */
ULONG width, height;	/* size of buffer */
ULONG line_size;	/* size if bytes of input buffer */
{
  ULONG y,flag,csize;
  ColorReg *cmap;
  register ULONG mask,invert,x,mask_start,mask_end;
  SHORT *imap,*c_ptr,*n_ptr,*err_buff,err,threshold;
  UBYTE data,*o_ptr;
  
  csize = chdr->csize;
  cmap = chdr->cmap;
 
  err_buff = (SHORT *)malloc(xsize * 2 * sizeof(SHORT));
  imap     = (SHORT *)malloc(csize * sizeof(SHORT));

   /* shift gray down to 8 bits */
  for(x=0; x<csize; x++) imap[x] = cmap[x].gray >> 8;

  for(x = 0; x < xsize; x++) err_buff[x] = (USHORT)imap[ *in++ ];
  flag = 0;

  /* used to invert image */
  if (x11_white & 0x01) invert = 0x00;
  else invert = ~0x00;
  threshold = 128;

  if (x11_bit_order == X11_MSB) { mask_start = 0x80; mask_end = 0x01; }
  else { mask_start = 0x01; mask_end = 0x80; }

  for(y = 0; y < ysize; y++)
  {
    o_ptr = (UBYTE *)(out + (line_size * y));
    /* double buffer error arrays */
    n_ptr = c_ptr = err_buff;
    if (flag == 0) { n_ptr += xsize; flag = 1; }
    else { c_ptr += xsize; flag = 0;
    }
    data = 0x00;
    mask = mask_start;
    if (y < (ysize - 1) )  n_ptr[0] = (USHORT)imap[ *in++ ];

    for(x=0; x<xsize; x++)
    {
      if (*c_ptr >= threshold) { err = *c_ptr - 255; data |= mask; } 
      else  err = *c_ptr;

      if (mask == mask_end) 
	{ *o_ptr++ = data^invert; data = 0x00; mask = mask_start; }
      else if (mask_start == 0x80) mask >>= 1;
           else mask <<= 1;
      c_ptr++;

      if (xa_dither_flag == FALSE)
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
          { *n_ptr = (USHORT)(imap[*in++]) + (SHORT)(err/16); n_ptr--; }
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
ULONG UTIL_Get_LSB_Long(fp)
FILE *fp;
{
 ULONG ret;

 ret =  fgetc(fp);
 ret |= fgetc(fp) << 8;
 ret |= fgetc(fp) << 16;
 ret |= fgetc(fp) << 24;
 return ret;
}

/* Routine to read a little endian half word.
 */
ULONG UTIL_Get_LSB_Short(fp)
FILE *fp;
{
 ULONG ret;

 ret =  fgetc(fp);
 ret |= fgetc(fp) << 8;
 return ret;
}

/* Routine to read a big endian long word.
 */
ULONG UTIL_Get_MSB_Long(fp)
FILE *fp;
{
 ULONG ret;

 ret  = fgetc(fp) << 24;
 ret |= fgetc(fp) << 16;
 ret |= fgetc(fp) << 8;
 ret |=  fgetc(fp);
 return ret;
}

/* Routine to read a big endian half word.
 */
LONG UTIL_Get_MSB_Short(fp)
FILE *fp;
{
 LONG ret;
 SHORT tmp;

 tmp  =  fgetc(fp) << 8;
 tmp |=  fgetc(fp);
 ret = tmp;
 return ret;
}

/* 
 * Routine to read a big endian half word.
 */
ULONG UTIL_Get_MSB_UShort(fp)
FILE *fp;
{
 ULONG ret;

 ret  =  fgetc(fp) << 8;
 ret |=  fgetc(fp);
 return ret;
}

/*
 *
 */
void UTIL_Mapped_To_Floyd(out,in,chdr_out,chdr_in,
		xipos,yipos,xosize,yosize,xisize,yisize)
UBYTE *out;		/* output (size of section) */
UBYTE *in;		/* input image */
XA_CHDR *chdr_out;	/* chdr to map to */
XA_CHDR *chdr_in;	/* input images chdr */
ULONG xipos,yipos;	/* section position within input buffer */
ULONG xosize, yosize;	/* section size */
ULONG xisize, yisize;	/* input buffer size */
{
  ULONG flag,csize_out,cflag;
  ColorReg *cmap_in,*cmap_out;
  register ULONG x,y;
  ULONG shift_r,shift_g,shift_b,coff_out;
  SHORT *err_buff0, *err_buff1, *e_ptr, *ne_ptr;
  SHORT r_err,g_err,b_err;
  UBYTE *o_ptr;

  cflag = (x11_display_type & XA_X11_COLOR)?TRUE:FALSE;

  if (!(x11_display_type & XA_X11_TRUE))
  {
    if (cmap_cache == 0) CMAP_Cache_Init(0);
    if (chdr_out != cmap_cache_chdr)
    {
      DEBUG_LEVEL2 fprintf(stderr,"CACHE: clear new_chdr %lx\n",(ULONG)chdr_out);
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
  e_ptr = err_buff1 = err_buff0 = (SHORT *)malloc(6 * xosize * sizeof(SHORT));
  if (err_buff0 == 0) TheEnd1("UTIL_Mapped_To_Floyd: malloc err");
  err_buff1 = &err_buff0[(3 * xosize)]; /* POD verify */
  flag = 0;

  { /* POD NOTE: only do this with different cmaps */
    register ULONG i,moff,msize,mcoff;
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
    register UBYTE *i_ptr = (UBYTE *)(in + (xisize * (yosize-1+yipos) + xipos));
    x = xosize; while(x--)
    {
      register ULONG p = *i_ptr++;
      *e_ptr++ = (USHORT)cmap_in[ p ].red   << 4;
      *e_ptr++ = (USHORT)cmap_in[ p ].green << 4;
      *e_ptr++ = (USHORT)cmap_in[ p ].blue  << 4;
    }
  }

  y = yosize; while(y--)
  {
    o_ptr = (UBYTE *)(out + (xosize * y * x11_bytes_pixel));
    if (flag) { e_ptr = err_buff1; ne_ptr = err_buff0; flag = 0; }
    else      { e_ptr = err_buff0; ne_ptr = err_buff1; flag = 1; }
    if (y > 0)
    {
      register SHORT *tptr = ne_ptr;
      register UBYTE *i_ptr = (UBYTE *)(in + (xisize * (y-1+yipos) + xipos));
      x = xosize; while(x--)
      {
	register ULONG p = *i_ptr++;
	*tptr++ = (USHORT)cmap_in[ p ].red   << 4;
	*tptr++ = (USHORT)cmap_in[ p ].green << 4;
	*tptr++ = (USHORT)cmap_in[ p ].blue  << 4;
      }
    }

    x = xosize; while(x--)
    {
      ULONG color_out,pix_out;
      register SHORT r,g,b;

      r = (*e_ptr++)/16; if (r<0) r = 0; else if (r>255) r = 255;
      g = (*e_ptr++)/16; if (g<0) g = 0; else if (g>255) g = 255;
      b = (*e_ptr++)/16; if (b<0) b = 0; else if (b>255) b = 255;
     
      if (x11_display_type & XA_X11_TRUE)
      {
	ULONG tr,tg,tb;
	pix_out = X11_Get_True_Color(r,g,b,8);
	tr = ((pix_out & x11_red_mask)   >> x11_red_shift);
	tg = ((pix_out & x11_green_mask) >> x11_green_shift);
	tb = ((pix_out & x11_blue_mask)  >> x11_blue_shift);
	r_err = r - (SHORT)(cmap_out[tr].red >> 8);
	g_err = g - (SHORT)(cmap_out[tg].green >> 8);
	b_err = b - (SHORT)(cmap_out[tb].blue >> 8);
      }
      else
      {
        register ULONG cache_i;
        cache_i = (  ( (r << shift_r) & cmap_cache_rmask)
                   | ( (g << shift_g) & cmap_cache_gmask)
                   | ( (b << shift_b) & cmap_cache_bmask) ) >> 8;
	if (cmap_cache[cache_i] == 0xffff)
	{
	  color_out = CMAP_Find_Closest(cmap_out,csize_out,r,g,b,8,8,8,cflag);
	  cmap_cache[cache_i] = (SHORT)color_out;
	}
	else color_out = (ULONG)cmap_cache[cache_i];
	pix_out = color_out + coff_out;
        r_err = r - (SHORT)(cmap_out[color_out].red >> 8);
        g_err = g - (SHORT)(cmap_out[color_out].green >> 8);
        b_err = b - (SHORT)(cmap_out[color_out].blue >> 8);
      }
 
      if (x11_bytes_pixel == 1) *o_ptr++ = (UBYTE)pix_out;
      else if (x11_bytes_pixel == 2)
      { USHORT *so_ptr = (USHORT *)(o_ptr);
        *so_ptr = (USHORT)pix_out; o_ptr +=2; }
      else { ULONG *lo_ptr = (ULONG *)(o_ptr);
             *lo_ptr = (ULONG)pix_out; o_ptr +=4; }

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
LONG *xpos,*ypos;	 /* input/output positions */
LONG *xsize,*ysize;	 /* input/output sizes */
LONG xi_scale,yi_scale; /* input scale/image size */
LONG xo_scale,yo_scale; /* output scale/image size */
{
  LONG ixp,iyp,oxp,oyp;
  LONG ixsize,iysize,oxsize,oysize;

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
LONG *xpos,*ypos;	 /* input/output positions */
LONG xi_scale,yi_scale; /* input scale/image size */
LONG xo_scale,yo_scale; /* output scale/image size */
{
  LONG ixp,iyp,oxp,oyp;

  ixp = *xpos; iyp = *ypos;

  oxp = (ixp * xo_scale) / xi_scale;
  if ( ((oxp * xi_scale) / xo_scale) < ixp ) oxp++;

  oyp = (iyp * yo_scale) / yi_scale;
  if ( ((oyp * yi_scale) / yo_scale) < iyp ) oyp++;

  *xpos = oxp;		*ypos = oyp;
}


/*
 * Function to get Scaling Buffer Coordinates.
 * 	returns TRUE if need to scale while buffering
 *	returns FALSE if you don't 
 *
 * NOTE: This should only be called within XXX_Read_File or else
 *       scale isn't guaranteed to be valid.
 * ACT_Setup_Mapped is okay since it has the same limitations.
 */
ULONG UTIL_Get_Buffer_Scale(width,height,buffx_o,buffy_o)
ULONG width,height;	 /* width, height of anim */
ULONG *buffx_o,*buffy_o; /* scaled width, height of anim */
{
 ULONG need_to_scale;
 ULONG buff_x,buff_y;

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
    buff_x = (ULONG)((float)(width) * xa_bscalex);
    if (buff_x == 0) buff_x = width;
    buff_y = (ULONG)((float)(height) * xa_bscaley);
    if (buff_y == 0) buff_y = height;
  }
  if ((buff_x != width) || (buff_y != height)) need_to_scale = TRUE;
  else need_to_scale = FALSE;
  *buffx_o = buff_x; *buffy_o = buff_y;
  return(need_to_scale);
}

/*
 * Function to allocate buffer for Scaling
 *
 */
UBYTE *UTIL_Alloc_Scaled(xi_size,yi_size,xi_scale,yi_scale,
	xo_scale,yo_scale,pix_size,xo_pos,yo_pos,bitmap_flag)
ULONG xi_size,yi_size;   /* size of section */
ULONG xi_scale,yi_scale; /* size of full input image */
ULONG xo_scale,yo_scale; /* size of full output image */
ULONG pix_size;          /* pixel size */
ULONG xo_pos,yo_pos;     /* pos of section in input buffer */
ULONG bitmap_flag;	 /* if true, then alloc for a bitmap */

{
  ULONG sx,sy,xsize,ysize;
  UBYTE *out;

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
    DEBUG_LEVEL1 fprintf(stderr,"SCALE: zero size %ld %ld\n",xsize,ysize);
    return(0);
  }

DEBUG_LEVEL1 fprintf(stderr,"AllocScaled: xy %ld %ld  t %ld\n",
  xsize,ysize,(xsize * ysize * pix_size));

  if (bitmap_flag==TRUE)
  {
    ULONG line_size;
    line_size = X11_Get_Bitmap_Width(xsize) / 8;
    out = (UBYTE *)malloc(line_size * ysize);
  }
  else out = (UBYTE *)malloc(xsize * ysize * pix_size);
  if (out==0) TheEnd1("UTIL_Alloc_Scaled: malloc err");
  return(out);
}

/*
 * Function to Scale a Mapped Image 
 *
 */
UBYTE *UTIL_Scale_Mapped(out,in,xi_pos,yi_pos,xi_size,yi_size,
	xi_buff_size,xi_scale,yi_scale,xo_scale,yo_scale,pix_size,
	xo_pos,yo_pos,xo_size,yo_size,chdr)
UBYTE *out;		 /* output buffer, if 0 then one is created */
UBYTE *in;		 /* input image buffer */
ULONG xi_pos,yi_pos;     /* pos of section in input buffer */
ULONG xi_size,yi_size;   /* size of section */
ULONG xi_buff_size;      /* row size of input buffer in bytes */
ULONG xi_scale,yi_scale; /* size of full input image */
ULONG xo_scale,yo_scale; /* size of full output image */
ULONG pix_size;		 /* pixel size */
ULONG *xo_pos,*yo_pos;   /* I screen pos/O pos of section in output buffer */
ULONG *xo_size,*yo_size; /* O size of outgoing section */
XA_CHDR *chdr;
/* note: pos of section in output buffer is always 0,0 */
{
  ULONG i,y,sx,sy,xvp,yvp,xsize,ysize,xoff;


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
    DEBUG_LEVEL1 fprintf(stderr,"SCALE: zero size %ld %ld\n",xsize,ysize);
    *xo_pos = *yo_pos = *xo_size = *yo_size = 0;
    return(0);
  }

  /* use scale buffer if necessary */
  if (out == 0)
  {
    ULONG tsize = xsize * ysize * pix_size;
    XA_REALLOC(xa_scale_buff,xa_scale_buff_size,tsize);
    out = (UBYTE *)xa_scale_buff;
  }

  /* readjust row cache buffer if necessary */
  if (xsize > xa_scale_row_size)
  {
    ULONG *tmp;
    if (xa_scale_row_buff == 0) tmp = (ULONG *) malloc(xsize * sizeof(ULONG) );
    else tmp = (ULONG *) realloc(xa_scale_row_buff, xsize * sizeof(ULONG) );
    if (tmp == 0) TheEnd1("UTIL_Scale_Mapped: row malloc err\n");
    xa_scale_row_buff = tmp;	xa_scale_row_size = xsize;
  }

  y = sx; for(i = 0; i < xsize; i++,y++)
	xa_scale_row_buff[i] = ((y * xi_scale) / xo_scale ) - xoff;

  if (chdr==0) /* no mapping */
  {
    register ULONG x,yoff;
    yoff = yvp - yi_pos;
    if (pix_size == 4)
    {
      register ULONG *optr = (ULONG *)out;
      for(y=sy; y< (sy + ysize); y++)
      { register ULONG *in_r_ptr = 
		(ULONG *)(in+(((y*yi_scale)/yo_scale)-yoff)*xi_buff_size);
        for(x = 0; x < xsize; x++) *optr++ = in_r_ptr[ xa_scale_row_buff[x] ];
      }
    }
    else if (pix_size == 2)
    {
      register USHORT *optr = (USHORT *)out;
      for(y=sy; y< (sy + ysize); y++)
      { register USHORT *in_r_ptr = 
		(USHORT *)(in+(((y*yi_scale)/yo_scale)-yoff)*xi_buff_size);
        for(x = 0; x < xsize; x++) *optr++ = in_r_ptr[ xa_scale_row_buff[x] ];
      }
    }
    else
    {
      register UBYTE *optr = (UBYTE *)out;
      for(y=sy; y< (sy + ysize); y++)
      { register UBYTE *in_r_ptr = 
		(UBYTE *)(in+(((y*yi_scale)/yo_scale)-yoff)*xi_buff_size);
        for(x = 0; x < xsize; x++) *optr++ = in_r_ptr[ xa_scale_row_buff[x] ];
      }
    }
  }
  else /* remap as we scale */
  {
    register ULONG x,yoff,moff,*map;
    map  = chdr->map; moff = chdr->moff; yoff = yvp - yi_pos;
    if (pix_size == 4)
    {
      register ULONG *optr = (ULONG *)out;
      for(y=sy; y< (sy + ysize); y++)
      { register UBYTE *in_r_ptr = 
		(UBYTE *)(in+(((y*yi_scale)/yo_scale)-yoff)*xi_buff_size);
	if (moff) for(x = 0; x < xsize; x++) 
		*optr++ = (ULONG)map[in_r_ptr[ xa_scale_row_buff[x]]-moff];
	else for(x = 0; x < xsize; x++) 
		*optr++ = (ULONG)map[in_r_ptr[ xa_scale_row_buff[x]]];
      }
    }
    else if (pix_size == 2)
    {
      register USHORT *optr = (USHORT *)out;
      for(y=sy; y< (sy + ysize); y++)
      { register UBYTE *in_r_ptr = 
		(UBYTE *)(in+(((y*yi_scale)/yo_scale)-yoff)*xi_buff_size);
	if (moff) for(x = 0; x < xsize; x++) 
		*optr++ = (USHORT)map[in_r_ptr[ xa_scale_row_buff[x]]-moff];
	else for(x = 0; x < xsize; x++) 
		*optr++ = (USHORT)map[in_r_ptr[ xa_scale_row_buff[x]]];
      }
    }
    else
    {
      register UBYTE *optr = (UBYTE *)out;
      for(y=sy; y< (sy + ysize); y++)
      { register UBYTE *in_r_ptr = 
		(UBYTE *)(in+(((y*yi_scale)/yo_scale)-yoff)*xi_buff_size);
	if (moff) for(x = 0; x < xsize; x++) 
		*optr++ = (UBYTE)map[in_r_ptr[ xa_scale_row_buff[x]]-moff];
	else for(x = 0; x < xsize; x++) 
		*optr++ = (UBYTE)map[in_r_ptr[ xa_scale_row_buff[x]]];
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
UBYTE *UTIL_Scale_Bitmap(out,in,xi_size,yi_size,
	xi_line_size,xi_scale,yi_scale,xo_scale,yo_scale,
	xo_pos,yo_pos,xo_size,yo_size,ibit_order,obit_order)
UBYTE *out,*in;
ULONG xi_size,yi_size;   /* size of section */
ULONG xi_line_size;      /* row size of input buffer in bytes*/
ULONG xi_scale,yi_scale; /* size of full input image */
ULONG xo_scale,yo_scale; /* size of full output image */
ULONG *xo_pos,*yo_pos;   /* pos of section in input/output buffer */
ULONG *xo_size,*yo_size; /* size of outgoing section */
ULONG ibit_order;	 /* input bit_order */
ULONG obit_order;	 /* output bit_order */
{
  ULONG xo_line_size;
  ULONG i,y,sx,sy,xvp,yvp,xsize,ysize;
  ULONG *row_b_ptr;
  UBYTE *row_msk_ptr,mask_start,mask_end;

  xvp = *xo_pos; yvp = *yo_pos;
  sx = (xvp * xo_scale) / xi_scale;
  if ( ((sx * xi_scale) / xo_scale) < xvp ) sx++;
  xsize = (((xvp + xi_size) * xo_scale) / xi_scale) + 1 - sx;
  if ( (((sx+xsize-1) * xi_scale) / xo_scale) >= (xvp+xi_size) ) xsize--;

  sy = (yvp * yo_scale) / yi_scale;
  if ( ((sy * yi_scale) / yo_scale) < yvp ) sy++;
  ysize = (((yvp + yi_size) * yo_scale) / yi_scale) + 1 - sy;
  if ( (((sy+ysize-1) * yi_scale) / yo_scale) >= (yvp+yi_size) ) ysize--;

DEBUG_LEVEL1 fprintf(stderr,"SCALE BITMAP: siz %ldx%ld -> %ldx%ld\n",
	xi_size,yi_size,xsize,ysize);
DEBUG_LEVEL1 fprintf(stderr,"SCALE BITMAP: pos %ldx%ld -> %ldx%ld\n",
	xvp,yvp,sx,sy);

  if ( (xsize==0) || (ysize==0) ) 
  {
    DEBUG_LEVEL1 fprintf(stderr,"SCALE: zero size %ld %ld\n",xsize,ysize);
    *xo_size = *yo_size = 0;
    return(0);
  }
  xo_line_size = X11_Get_Bitmap_Width(xsize) / 8;
  /* use scale buffer if necessary */
  if (out == 0)
  {
    ULONG tsize = ysize * xo_line_size;
    XA_REALLOC(xa_scale_buff,xa_scale_buff_size,tsize);
    out = (UBYTE *)xa_scale_buff;
  }

  /* readjust row cache buffer if necessary */
  if ( (xsize<<1) > xa_scale_row_size)
  {
    ULONG *tmp;
    if (xa_scale_row_buff == 0) tmp = (ULONG *) malloc(xsize * sizeof(ULONG) );
    else tmp = (ULONG *) realloc(xa_scale_row_buff,(xsize<<1)*sizeof(ULONG));
    if (tmp == 0) TheEnd1("UTIL_Scale_Mapped: row malloc err\n");
    xa_scale_row_buff = tmp;    xa_scale_row_size = (xsize<<1);
  }
  row_b_ptr = xa_scale_row_buff;
  row_msk_ptr = (UBYTE *)&xa_scale_row_buff[xsize];

  y = sx;
  for(i = 0; i < xsize; i++,y++)
  {
    register ULONG x = ((y * xi_scale) / xo_scale) - xvp;
    row_b_ptr[i] = x/8;
    if (ibit_order == X11_LSB) row_msk_ptr[i] = 0x01 << (x%8);
    else row_msk_ptr[i] = 0x01 << (7 - (x%8));
  }
  if (obit_order == X11_MSB) { mask_start = 0x80; mask_end = 0x01; }
  else { mask_start = 0x01; mask_end = 0x80; }
  for(y=sy; y < (sy+ysize);y++)
  {
    register ULONG x;
    register UBYTE omsk,odat;
    register UBYTE *iptr,*optr;
    iptr = (UBYTE *)(in + (((y * yi_scale) / yo_scale)-yvp) * xi_line_size);
    optr = (UBYTE *)(out + ((y - sy) * xo_line_size));
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
UBYTE *UTIL_RGB_To_Map(out,in,chdr,xsize,ysize,free_in_flag)
UBYTE *out,*in;
XA_CHDR *chdr;
ULONG xsize,ysize;
ULONG free_in_flag;
{
  ULONG i,shift_r,shift_g,shift_b,pic_size;
  UBYTE *t_pic,*iptr,*optr;
  ULONG csize,coff;
 
  DEBUG_LEVEL1 fprintf(stderr,"UTIL_RGB_To_Map\n");
  pic_size = xsize * ysize;
  if (out == 0)
  {
    t_pic = (UBYTE *)malloc( XA_PIC_SIZE(pic_size) );
    if (t_pic == 0) TheEnd1("RGB: t_pic malloc err");
  } else t_pic = out;

  if (cmap_cache == 0) CMAP_Cache_Init(0);
  if (chdr != cmap_cache_chdr)
  {
    DEBUG_LEVEL2 fprintf(stderr,"CACHE: clear new_chdr %lx\n",
                                                        (ULONG)chdr);
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
    register ULONG color_out,r,g,b;
    register ULONG cache_i;

    r = (ULONG)(*iptr++); g = (ULONG)(*iptr++); b = (ULONG)(*iptr++);
    cache_i  = (ULONG)(  (  ( (r << shift_r) & cmap_cache_rmask)
                          | ( (g << shift_g) & cmap_cache_gmask)
                          | ( (b << shift_b) & cmap_cache_bmask)
                         ) >> 8);
    if (cmap_cache[cache_i] == 0xffff)
    {
      color_out = coff +
                CMAP_Find_Closest(chdr->cmap,csize,r,g,b,8,8,8,TRUE);
      cmap_cache[cache_i] = (USHORT)color_out;
    }
    else color_out = (ULONG)cmap_cache[cache_i];

    *optr++ = (UBYTE)(color_out);

  }
  if (free_in_flag == TRUE) FREE(in,0x304); in=0;
  return(t_pic);
}

/**********************
 *
 **********************/
UBYTE *UTIL_RGB_To_FS_Map(out,in,chdr,xsize,ysize,free_in_flag)
UBYTE *out,*in;
XA_CHDR *chdr;
ULONG xsize,ysize;
ULONG free_in_flag;
{
  ULONG flag,csize,xsize3;
  ColorReg *cmap;
  register ULONG x,y;
  ULONG shift_r,shift_g,shift_b,coff;
  SHORT *err_buff0, *err_buff1, *e_ptr, *ne_ptr;
  SHORT r_err,g_err,b_err;
  UBYTE *o_ptr,*tpic;

  DEBUG_LEVEL1 fprintf(stderr,"UTIL_RGB_To_FS_Map\n");

  if (out == 0)
  {
    tpic = (UBYTE *)malloc( XA_PIC_SIZE(xsize * ysize) );
    if (tpic == 0) TheEnd1("UTIL_RGB_To_FS_Map: malloc err");
  } else tpic = out;

  if (cmap_cache == 0) CMAP_Cache_Init(0);

  if (chdr != cmap_cache_chdr)
  {
    DEBUG_LEVEL2 fprintf(stderr,"CACHE: clear new_chdr %lx\n",(ULONG)chdr);
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
  e_ptr = err_buff0 = (SHORT *)malloc(6 * xsize * sizeof(SHORT) );
  if (err_buff0 == 0) TheEnd1("UTIL_Mapped_To_Floyd: malloc err");
  err_buff1 = &err_buff0[xsize3];
  flag = 0;

  {
    register UBYTE *i_ptr = (UBYTE *)( in + (xsize3 * (ysize-1)));
    x = xsize; while(x--)
    {
      *e_ptr++ = (USHORT)(*i_ptr++) << 4;
      *e_ptr++ = (USHORT)(*i_ptr++) << 4;
      *e_ptr++ = (USHORT)(*i_ptr++) << 4;
    }
  }

  y = ysize; while(y--)
  {
    o_ptr = (UBYTE *)(tpic + (xsize * y));
    if (flag) { e_ptr = err_buff1; ne_ptr = err_buff0; flag = 0; }
    else      { e_ptr = err_buff0; ne_ptr = err_buff1; flag = 1; }
    if (y > 0)
    {
      register SHORT *tptr = ne_ptr;
      register UBYTE *i_ptr = (UBYTE *)(in + (xsize3 * (y-1)));
      x = xsize; while(x--)
      {
	*tptr++ = (USHORT)(*i_ptr++) << 4;
	*tptr++ = (USHORT)(*i_ptr++) << 4;
	*tptr++ = (USHORT)(*i_ptr++) << 4;
      }
    }

    x = xsize; while(x--)
    {
      ULONG color_out;
      register SHORT r,g,b;
      register ULONG cache_i;

      r = (*e_ptr++)/16; if (r<0) r = 0; else if (r>255) r = 255;
      g = (*e_ptr++)/16; if (g<0) g = 0; else if (g>255) g = 255;
      b = (*e_ptr++)/16; if (b<0) b = 0; else if (b>255) b = 255;
     
      cache_i = (  ( (r << shift_r) & cmap_cache_rmask)
                 | ( (g << shift_g) & cmap_cache_gmask)
                 | ( (b << shift_b) & cmap_cache_bmask) ) >> 8;

      if (cmap_cache[cache_i] == 0xffff)
      {
        color_out = CMAP_Find_Closest(cmap,csize,r,g,b,8,8,8,TRUE);
        cmap_cache[cache_i] = (SHORT)color_out;
      }
      else color_out = (ULONG)cmap_cache[cache_i];
      
      *o_ptr++ = (UBYTE)(color_out + coff);

      r_err = r - (SHORT)(cmap[color_out].red >> 8);
      g_err = g - (SHORT)(cmap[color_out].green >> 8);
      b_err = b - (SHORT)(cmap[color_out].blue >> 8);

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
  if (free_in_flag == TRUE) FREE(in,0x306);
  return(tpic);
}

void
UTIL_Pack_Image(out,in,xsize,ysize)
UBYTE *out;
UBYTE *in;
ULONG xsize,ysize;
{
  UBYTE *o_ptr;
  ULONG x,y,shft_msk;
  LONG shft_start,shft_end,shft_inc;

  if (x11_bits_per_pixel == 4)
  { shft_msk = 0x0f;
    if (x11_byte_order == X11_MSB) {shft_start=4; shft_end=0; shft_inc= -4;}
    else			   {shft_start=0; shft_end=4; shft_inc=  4;}
  }
  else if (x11_bits_per_pixel == 2)
  { shft_msk = 0x03;
    if (x11_byte_order == X11_MSB) {shft_start=6; shft_end=0; shft_inc= -2;}
    else			   {shft_start=0; shft_end=6; shft_inc=  2;}
  }
  else /* unsupported depth */
  {
    fprintf(stderr,"X11 Packing of bits %ld to depth %ld not yet supported\n",
	x11_bits_per_pixel,x11_depth);
    return;
  }

  o_ptr = out;
  for(y=0;y<ysize;y++)
  {
    register ULONG data;
    register LONG shft;
    shft = shft_start;
    for(x=0;x<xsize;x++)
    {
	if (shft != shft_end) /* still creating byte */
	{
	  data = ((*in++) & shft_msk) << shft; 
	  shft += shft_inc;
	}
	else /* at the end */
	{
	  *o_ptr++ = data | (((*in++) & shft_msk) << shft);
	  shft = shft_start;
	} 
    }
    if (shft != shft_start) *o_ptr++ = data; /* partial byte */
  } /* end of y loop */
}
