
/*
 * xa_qt_decs.c
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
/* REVISIONS ***********
 * 31Aug94  RPZA was using *iptr+=row_inc instead of iptr+=row_inc.
 * 15Sep94  Added support for RAW32 format. straight RGB with 0x00 up front.
 * 20Sep94  Added RAW4,RAW16,RAW24,RAW32,Gray CVID and Gray Other codecs.
 * 07Nov94  Fixed bug in RLE,RLE16,RLE24,RLE32 and RLE1 code, where I
 *          had improperly guessed what a header meant. Now it's a different
 *	    hopeful more correct guess.
 * 29Dec94  Above bug wasn't fixed in RLE(8bit), now it is.
 * 11Feb95  Fixed Bug with RLE depth 1 codec.
 * 04Mar95  Added Dithering(+F option) to Cinepak Video Codec.
 * 17Mar95  Fixed bug that was causing quicktime to erroneously send
 *          back a FULL_IM flag allowing serious skipping to occur. This
 *          causes on screen corruption if the Video Codec doesn't 
 *          really support serious skipping.
 * 11Apr95  Fixed bug in QT_Create_Gray_Cmap that caused last color of
 *	    colormap not to be generated correctly. Only affected
 *	    the Gray Quicktime codecs.
 * 16Jun95  Removed Cinepak Codec per Radius request.
 *  1Mar96  Separated Video Codecs from mainline qt code.
 * 17Mar96  Added dithering to RPZA Codec(Apple Video).
 * 08Jul98  Added support for 8BPS depth 8 and depth 24 video codecs.
 * 18Nov98  konstantin Priblouda added support for RAW3(SGI grayscale) 
 * 18Nov98  Support for MJPA motion jpeg
 * 18Nov98  Added support for 8BPS depth 32 video codec.
 */
#include "xa_qt.h"
#include "xa_codecs.h"
#include "xa_color.h"

xaLONG QT_Codec_Query();
xaLONG QT_UNK_Codec_Query();

static xaULONG QT_Decode_RAW1();
static xaULONG QT_Decode_RAW4();
static xaULONG QT_Decode_RAW8();
static xaULONG QT_Decode_RAW16();
static xaULONG QT_Decode_RAW24();
static xaULONG QT_Decode_RAW32();
static xaULONG QT_Decode_RLE1();
static xaULONG QT_Decode_RLE2();
static xaULONG QT_Decode_RLE4();
static xaULONG QT_Decode_RLE8();
static xaULONG QT_Decode_RLE16();
static xaULONG QT_Decode_RLE24();
static xaULONG QT_Decode_RLE32();
xaULONG QT_Decode_RPZA();
static xaULONG QT_Decode_SMC();
static xaULONG QT_Decode_YUV2();
static xaULONG QT_Decode_8BPS8();
static xaULONG QT_Decode_8BPS24();
extern xaULONG JFIF_Decode_JPEG();
extern xaULONG AVI_Decode_CRAM();
extern xaULONG AVI_Decode_CRAM16();

static xaULONG QT_RPZA_Dither();

extern void XA_Gen_YUV_Tabs();
/* JPEG and other assist routines */
extern void JPG_Alloc_MCU_Bufs();
extern void JPG_Setup_Samp_Limit_Table();

extern void yuv_to_rgb();
void	QT_Get_Dith1_Color24();
void	QT_Get_Dith4_Color24();
xaULONG	QT_Get_Color();
void	QT_Get_RGBColor();
void	QT_Get_RGBColorL();
void	QT_Get_AV_Colors();
void	QT_Get_AV_RGBColors();
void	QT_Get_AV_DITH_RGB();
xaULONG	QT_Get_DithColor24();
extern	xaULONG XA_RGB24_To_CLR32();
extern	xaUSHORT qt_gamma_adj[32];

void *XA_YUV211111_Func();

extern void CMAP_Cache_Clear();
extern void CMAP_Cache_Init();
extern xaULONG CMAP_Find_Closest();

extern xaLONG xa_dither_flag;
extern xaUBYTE  *xa_byte_limit;
extern YUVBufs jpg_YUVBufs;
extern YUVTabs def_yuv_tabs;


#define	SMC_MAX_CNT 256
/* POD NOTE: eventually make these conditionally dynamic */
static xaULONG smc_8cnt,smc_Acnt,smc_Ccnt;
static xaULONG smc_8[ (2 * SMC_MAX_CNT) ];
static xaULONG smc_A[ (4 * SMC_MAX_CNT) ];
static xaULONG smc_C[ (8 * SMC_MAX_CNT) ];


/*****************************************************************************
 *
 ****************/
xaLONG QT_Codec_Query(codec)
XA_CODEC_HDR *codec;
{ xaLONG ret = CODEC_UNKNOWN;   /* default */
  codec->extra = 0;
  codec->xapi_rev = 0x0001;
  codec->decoder = 0;
  codec->description = 0;
  codec->avi_read_ext = 0;

  switch(codec->compression)
  {
    case QT_rle:
        codec->compression      = QT_rle;
        codec->description      = "Apple Animation(RLE)";
        ret = CODEC_SUPPORTED;
	switch(codec->depth)
	{
	  case 40:
	  case 8:	codec->decoder = QT_Decode_RLE8;
			codec->x = 4 * ((codec->x + 3)/4);
			codec->depth = 8;
			break;
	  case 16:	codec->decoder = QT_Decode_RLE16;
			break;
	  case 24:	codec->decoder = QT_Decode_RLE24;
			break;
	  case 32:	codec->decoder = QT_Decode_RLE32;
			break;
	  case 33:
	  case  1:	codec->decoder = QT_Decode_RLE1;
			codec->x = 16 * ((codec->x + 15)/16);
			codec->depth = 1;
			break;
	  case 34:
	  case  2:	codec->decoder = QT_Decode_RLE2;
			codec->x = 16 * ((codec->x + 15)/16);
			codec->depth = 4;
			break;
	  case 36:
	  case  4:	codec->decoder = QT_Decode_RLE4;
			codec->x = 8 * ((codec->x + 7)/8);
			codec->depth = 4;
			break;
	  default:	ret = CODEC_UNSUPPORTED;	break;
	}
	break;

    case QT_smc:
        codec->compression      = QT_smc;
        codec->description      = "Apple Graphics(SMC)";
        ret = CODEC_SUPPORTED;
	if ((codec->depth==8) || (codec->depth==40))
	{ codec->decoder = QT_Decode_SMC;
	  codec->x = 4 * ((codec->x + 3)/4);
	  codec->y = 4 * ((codec->y + 3)/4);
	  codec->depth = 8;
	}
	else ret = CODEC_UNSUPPORTED;
	break;

    case QT_raw3: /* added by konstantin Priblouda to support 
		   * sgi grayscale 8 bit QT files */
    case QT_raw:
        codec->compression      = QT_raw;
        codec->description      = "Apple Uncompressed";
        ret = CODEC_SUPPORTED;
	switch(codec->depth)
	{
	  case 33:
	  case 1:	codec->decoder = QT_Decode_RAW1;
			codec->depth = 1;
			break;
	  case 36:
	  case 4:	codec->decoder = QT_Decode_RAW4;
			codec->depth = 4;
			break;
	  case 40:
	  case 8:	codec->decoder = QT_Decode_RAW8;
			codec->depth = 8;
			break;
	  case 16:	codec->decoder = QT_Decode_RAW16;
			break;
	  case 24:	codec->decoder = QT_Decode_RAW24;
			break;
	  case 32:	codec->decoder = QT_Decode_RAW32;
			break;
	  default:	ret = CODEC_UNSUPPORTED;	break;
	}
	break;

    case QT_8BPS:
        codec->compression      = QT_8BPS;
        codec->description      = "Photoshop";
        ret = CODEC_SUPPORTED;
	switch(codec->depth)
	{
/* NOT YET SUPPORTED
	  case 33:
	  case 1:	codec->decoder = QT_Decode_RAW1;
			codec->depth = 1;
			break;
	  case 36:
	  case 4:	codec->decoder = QT_Decode_RAW4;
			codec->depth = 4;
			break;
	  case 16:	codec->decoder = QT_Decode_RAW16;
			break;
	  case 32:	codec->decoder = QT_Decode_RAW32;
			break;
*/
	  case 40:
	  case 8:	codec->decoder = QT_Decode_8BPS8;
			codec->depth = 8;
			break;
	  case 32:	/* TEST */
		codec->extra = (void *)(0x01);
	  case 24:	codec->decoder = QT_Decode_8BPS24;
			break;
	  default:	ret = CODEC_UNSUPPORTED;	break;
	}
	if (ret == CODEC_SUPPORTED)
		JPG_Alloc_MCU_Bufs(codec->anim_hdr,codec->x,0,xaFALSE);
	break;



    case QT_azpr:
    case QT_rpza:
        codec->compression      = QT_rpza;
        codec->description      = "Apple Video(RPZA)";
        ret = CODEC_SUPPORTED;
	if (codec->depth == 16)
	{ codec->decoder = QT_Decode_RPZA;
	  codec->x = 4 * ((codec->x + 3)/4);
	  codec->y = 4 * ((codec->y + 3)/4);
	  JPG_Setup_Samp_Limit_Table(codec->anim_hdr);
	}
	else ret = CODEC_UNSUPPORTED;
	break;

    case QT_jpeg:
        codec->compression      = QT_jpeg;
        codec->description      = "JPEG";
	codec->xapi_rev		= 0x0002;
        ret = CODEC_SUPPORTED;
	JPG_Alloc_MCU_Bufs(codec->anim_hdr,codec->x,0,xaFALSE);
	JPG_Setup_Samp_Limit_Table(codec->anim_hdr);
	codec->x = 4 * ((codec->x + 3)/4);
	codec->y = 2 * ((codec->y + 1)/2);
	codec->decoder = JFIF_Decode_JPEG;
	if (codec->depth == 40) codec->depth = 8;
        if (codec->depth > 8)   XA_Gen_YUV_Tabs(codec->anim_hdr);
	break;

/* NOTE: image appears to shifted 25% to the right  qts/mjpg/08*.qt */
    case QT_MJPG:
    case QT_mjpg:
        codec->compression      = QT_MJPG;
        codec->description      = "Motion JPEG";
        codec->xapi_rev = 0x0002;
        ret = CODEC_SUPPORTED;
        codec->extra    = (void *)(0x00);
        codec->decoder = JFIF_Decode_JPEG;
        codec->x = 4 * ((codec->x + 3)/4);
        codec->y = 2 * ((codec->y + 1)/2);
	if (codec->depth == 40) codec->depth = 8;
        if (codec->depth > 8)   XA_Gen_YUV_Tabs(codec->anim_hdr);
        else    codec->avi_ctab_flag = xaFALSE;
        JPG_Alloc_MCU_Bufs(codec->anim_hdr,codec->x,0,xaFALSE);
        break;

    case QT_mjpa:
        codec->compression      = QT_MJPG;
        codec->description      = "Motion JPEG Type A";
        codec->xapi_rev = 0x0002;
        ret = CODEC_SUPPORTED;
	/* interleave is optional and detected automatically, but with mjpga
	 * first field is always top field
	 */
        codec->extra    = (void *)(0x02);
        codec->decoder = JFIF_Decode_JPEG;
        codec->x = 4 * ((codec->x + 3)/4);
        codec->y = 4 * ((codec->y + 3)/4);
	if (codec->depth == 40) codec->depth = 8;
        if (codec->depth > 8)   XA_Gen_YUV_Tabs(codec->anim_hdr);
        else    codec->avi_ctab_flag = xaFALSE;
        JPG_Alloc_MCU_Bufs(codec->anim_hdr,codec->x,0,xaFALSE);
        break;


    case QT_yuv2:
	codec->xapi_rev = 0x0002;
        codec->compression      = QT_yuv2;
        codec->description      = "Component Video(YUV2)";
        ret = CODEC_SUPPORTED;
	codec->decoder = QT_Decode_YUV2;
	codec->x = 2 * ((codec->x + 1)/2);
	codec->y = 2 * ((codec->y + 1)/2); /* POD ness? */
	JPG_Alloc_MCU_Bufs(codec->anim_hdr,codec->x,0,xaFALSE);
	JPG_Setup_Samp_Limit_Table(codec->anim_hdr);
	XA_Gen_YUV_Tabs(codec->anim_hdr);
	break;

    case QT_cram:
    case QT_CRAM:
    case QT_wham:
    case QT_WHAM:
    case QT_msvc:
    case QT_MSVC:
        codec->compression      = QT_cram;
        codec->description      = "Microsoft Video 1";
        ret = CODEC_SUPPORTED;
	if (codec->depth == 8)		codec->decoder = AVI_Decode_CRAM;
	else if (codec->depth == 16)	codec->decoder = AVI_Decode_CRAM16;
	else ret = CODEC_UNSUPPORTED;
	codec->x = 4 * ((codec->x + 3)/4);
	codec->y = 4 * ((codec->y + 3)/4);
	JPG_Setup_Samp_Limit_Table(codec->anim_hdr);
	break;


	
    default:
        ret = CODEC_UNKNOWN;
        break;
  }
  return(ret);
}

/*****************************************************************************
 *
 ****************/
xaLONG QT_UNK_Codec_Query(codec)
XA_CODEC_HDR *codec;
{ xaLONG ret = CODEC_UNKNOWN;   /* default */
  codec->extra = 0;
  codec->xapi_rev = 0x0001;
  codec->decoder = 0;
  codec->description = 0;
  codec->avi_read_ext = 0;

  switch(codec->compression)
  {
    case QT_PGVV:
        codec->compression      = QT_PGVV;
        codec->description      = "Radius (PGVV)";
        ret = CODEC_UNSUPPORTED;
	break;

    case QT_cvid:
    case QT_CVID:
        codec->compression      = QT_CVID;
        codec->description      = "Radius Cinepak";
        ret = CODEC_UNSUPPORTED;
	break;

    case QT_SPIG:
        codec->compression      = QT_SPIG;
        codec->description      = "Radius Spigot";
        ret = CODEC_UNSUPPORTED;
	break;

    case QT_rt21:
    case QT_RT21:
        codec->compression      = QT_RT21;
        codec->description      = "Intel Indeo R2.1";
        ret = CODEC_UNSUPPORTED;
	break;

    case QT_iv31:
    case QT_IV31:
        codec->compression      = QT_IV31;
        codec->description      = "Intel Indeo R3.1";
        ret = CODEC_UNSUPPORTED;
	break;

    case QT_iv32:
    case QT_IV32:
        codec->compression      = QT_IV32;
        codec->description      = "Intel Indeo R3.2";
        ret = CODEC_UNSUPPORTED;
	break;

    case QT_iv41:
    case QT_IV41:
        codec->compression      = QT_IV41;
        codec->description      = "Intel Indeo R4.1";
        ret = CODEC_UNSUPPORTED;
	break;

    case QT_YVU9:
    case QT_YUV9:
        codec->compression      = QT_YUV9;
        codec->description      = "Intel Raw(YUV9)";
        ret = CODEC_UNSUPPORTED;
	break;

    case QT_kpcd:
    case QT_KPCD:
        codec->compression      = QT_KPCD;
        codec->description      = "Kodak Photo CD";
        ret = CODEC_UNSUPPORTED;
	break;

    case QT_SVQ1:
        codec->compression      = QT_SVQ1;
        codec->description      = "Sorenson Video";
        ret = CODEC_UNSUPPORTED;
	break;

    default:
        ret = CODEC_UNKNOWN;
        break;
  }
  return(ret);
}



/********************* * * * *******************************************/
xaULONG QT_Decode_RAW4(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaUBYTE *dp = delta;
  xaLONG i = (imagex * imagey) >> 1;

  dec_info->xs = dec_info->ys = 0;
  dec_info->xe = imagex; dec_info->ye = imagey;

    /* Indicate we can drop these frames */
  if (dec_info->skip_flag > 0) return(ACT_DLTA_DROP);

/* POD QUESTION: is imagex a multiple of 2??? */
  if (map_flag==xaFALSE) { xaUBYTE *iptr = (xaUBYTE *)image; 
    while(i--) { xaUBYTE d = *dp++; *iptr++ = (d>>4); *iptr++ = d & 0xf; }
  }
  else if (x11_bytes_pixel==1) { xaUBYTE *iptr = (xaUBYTE *)image; 
    while(i--) { xaULONG d = (xaULONG)*dp++;
                 *iptr++ = (xaUBYTE)map[d>>4]; *iptr++ = (xaUBYTE)map[d&15]; }
  }
  else if (x11_bytes_pixel==4) { xaULONG *iptr = (xaULONG *)image; 
    while(i--) { xaULONG d = (xaULONG)*dp++;
                 *iptr++ = (xaULONG)map[d>>4]; *iptr++ = (xaULONG)map[d&15]; }
  }
  else /*(x11_bytes_pixel==2)*/ { xaUSHORT *iptr = (xaUSHORT *)image; 
    while(i--) { xaULONG d = (xaULONG)*dp++;
                 *iptr++ = (xaUSHORT)map[d>>4]; *iptr++ = (xaUSHORT)map[d&15]; }
  }
  if (map_flag==xaTRUE) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}

/********************* * * * *******************************************/
xaULONG QT_Decode_RAW1(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaUBYTE *dp = delta;
  xaLONG i = (imagex * imagey) >> 1;
  xaULONG white,black;

  dec_info->xs = dec_info->ys = 0;
  dec_info->xe = imagex; dec_info->ye = imagey;

  black = 0x00;
  white = 0x01;

    /* Indicate we can drop these frames */
  if (dec_info->skip_flag > 0) return(ACT_DLTA_DROP);

/* POD QUESTION: is imagex a multiple of 8??? */
  if (map_flag==xaFALSE) { xaUBYTE *iptr = (xaUBYTE *)image; 
    while(i--) { xaUBYTE d = *dp++; 
       *iptr++ = (xaUBYTE)( (d & 0x80)?(white):(black) );
       *iptr++ = (xaUBYTE)( (d & 0x40)?(white):(black) );
       *iptr++ = (xaUBYTE)( (d & 0x20)?(white):(black) );
       *iptr++ = (xaUBYTE)( (d & 0x10)?(white):(black) );
       *iptr++ = (xaUBYTE)( (d & 0x08)?(white):(black) );
       *iptr++ = (xaUBYTE)( (d & 0x04)?(white):(black) );
       *iptr++ = (xaUBYTE)( (d & 0x02)?(white):(black) );
       *iptr++ = (xaUBYTE)( (d & 0x01)?(white):(black) ); }
  }
  else if (x11_bytes_pixel==1) { xaUBYTE *iptr = (xaUBYTE *)image; 
    while(i--) { xaUBYTE d = *dp++;
       *iptr++ = (xaUBYTE)map[ (d & 0x80)?(white):(black) ];
       *iptr++ = (xaUBYTE)map[ (d & 0x40)?(white):(black) ];
       *iptr++ = (xaUBYTE)map[ (d & 0x20)?(white):(black) ];
       *iptr++ = (xaUBYTE)map[ (d & 0x10)?(white):(black) ];
       *iptr++ = (xaUBYTE)map[ (d & 0x08)?(white):(black) ];
       *iptr++ = (xaUBYTE)map[ (d & 0x04)?(white):(black) ];
       *iptr++ = (xaUBYTE)map[ (d & 0x02)?(white):(black) ];
       *iptr++ = (xaUBYTE)map[ (d & 0x01)?(white):(black) ]; }
  }
  else if (x11_bytes_pixel==4) { xaULONG *iptr = (xaULONG *)image; 
    while(i--) { xaUBYTE d = *dp++;
       *iptr++ = (xaULONG)map[ (d & 0x80)?(white):(black) ];
       *iptr++ = (xaULONG)map[ (d & 0x40)?(white):(black) ];
       *iptr++ = (xaULONG)map[ (d & 0x20)?(white):(black) ];
       *iptr++ = (xaULONG)map[ (d & 0x10)?(white):(black) ];
       *iptr++ = (xaULONG)map[ (d & 0x08)?(white):(black) ];
       *iptr++ = (xaULONG)map[ (d & 0x04)?(white):(black) ];
       *iptr++ = (xaULONG)map[ (d & 0x02)?(white):(black) ];
       *iptr++ = (xaULONG)map[ (d & 0x01)?(white):(black) ]; }
  }
  else /*(x11_bytes_pixel==2)*/ { xaUSHORT *iptr = (xaUSHORT *)image; 
    while(i--) { xaUBYTE d = *dp++;
       *iptr++ = (xaUSHORT)map[ (d & 0x80)?(white):(black) ];
       *iptr++ = (xaUSHORT)map[ (d & 0x40)?(white):(black) ];
       *iptr++ = (xaUSHORT)map[ (d & 0x20)?(white):(black) ];
       *iptr++ = (xaUSHORT)map[ (d & 0x10)?(white):(black) ];
       *iptr++ = (xaUSHORT)map[ (d & 0x08)?(white):(black) ];
       *iptr++ = (xaUSHORT)map[ (d & 0x04)?(white):(black) ];
       *iptr++ = (xaUSHORT)map[ (d & 0x02)?(white):(black) ];
       *iptr++ = (xaUSHORT)map[ (d & 0x01)?(white):(black) ]; }
  }
  if (map_flag==xaTRUE) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}

/********************* * * * *******************************************/
xaULONG QT_Decode_RAW8(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaUBYTE *dptr = delta;
  xaLONG i = imagex * imagey;

  dec_info->xs = dec_info->ys = 0;
  dec_info->xe = imagex; dec_info->ye = imagey;
    /* Indicate we can drop these frames */
  if (dec_info->skip_flag > 0) return(ACT_DLTA_DROP);
  
  if (map_flag==xaFALSE) { xaUBYTE *iptr = (xaUBYTE *)image; 
				while(i--) *iptr++ = (xaUBYTE)*dptr++; }
  else if (x11_bytes_pixel==1) { xaUBYTE *iptr = (xaUBYTE *)image; 
				while(i--) *iptr++ = (xaUBYTE)map[*dptr++]; }
  else if (x11_bytes_pixel==2) { xaUSHORT *iptr = (xaUSHORT *)image; 
				while(i--) *iptr++ = (xaUSHORT)map[*dptr++]; }
  else { xaULONG *iptr = (xaULONG *)image; 
				while(i--) *iptr++ = (xaULONG)map[*dptr++]; }

  if (map_flag==xaTRUE) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}

#define QT_RAW_READ16RGB(dp,r,g,b) { xaULONG _d = *dp++ << 8; _d |= *dp++; \
  r = (_d >> 10) & 0x1f; g = (_d >> 5) & 0x1f; b = _d & 0x1f;	\
  r = (r << 3) | (r >> 2); g = (g << 3) | (g >> 2); b = (b << 3) | (b >> 2); }

/****************** RAW CODEC DEPTH 16 *********************************
 *  1 unused bit. 5 bits R, G, B
 *
 *********************************************************************/
xaULONG QT_Decode_RAW16(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG special = dec_info->special;
  XA_CHDR *chdr = dec_info->chdr;
  xaUBYTE *dp = delta;
  xaULONG special_flag = special & 0x0001;
  xaLONG i = imagex * imagey;
  
  dec_info->xs = dec_info->ys = 0;
  dec_info->xe = imagex; dec_info->ye = imagey;
    /* Indicate we can drop these frames */
  if (dec_info->skip_flag > 0) return(ACT_DLTA_DROP);

  if (chdr) { if (chdr->new_chdr) chdr = chdr->new_chdr; }

  if (special_flag)
  { xaUBYTE *iptr = (xaUBYTE *)image; 
    while(i--) 
    { xaULONG r,g,b;  QT_RAW_READ16RGB(dp,r,g,b);
      *iptr++ = r; *iptr++ = g; *iptr++ = b;}
  }
  else if ( (map_flag==xaFALSE) || (x11_bytes_pixel==1) )
  { xaUBYTE *iptr = (xaUBYTE *)image; 
    while(i--) 
    { xaULONG r,g,b; QT_RAW_READ16RGB(dp,r,g,b);
      *iptr++ = (xaUBYTE)XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
    }
  }
  else if (x11_bytes_pixel==4)
  { xaULONG *iptr = (xaULONG *)image; 
    while(i--) 
    { xaULONG r,g,b;  QT_RAW_READ16RGB(dp,r,g,b);
      *iptr++ = (xaULONG)XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
    }
  }
  else /* (x11_bytes_pixel==2) */
  { xaUSHORT *iptr = (xaUSHORT *)image; 
    while(i--) 
    { xaULONG r,g,b;  QT_RAW_READ16RGB(dp,r,g,b);
      *iptr++ = (xaUSHORT)XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
    }
  }
  if (map_flag==xaTRUE) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}


/****************** RAW CODEC DEPTH 24 *********************************
 *  R G B
 *********************************************************************/
xaULONG QT_Decode_RAW24(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG special = dec_info->special;
  XA_CHDR *chdr = dec_info->chdr;
  xaUBYTE *dp = delta;
  xaULONG special_flag = special & 0x0001;
  xaLONG i = imagex * imagey;
  
  dec_info->xs = dec_info->ys = 0;
  dec_info->xe = imagex; dec_info->ye = imagey;
    /* Indicate we can drop these frames */
  if (dec_info->skip_flag > 0) return(ACT_DLTA_DROP);

  if (chdr) { if (chdr->new_chdr) chdr = chdr->new_chdr; }

  if (special_flag)
  { xaUBYTE *iptr = (xaUBYTE *)image; 
    while(i--) { *iptr++ = *dp++; *iptr++ = *dp++; *iptr++ = *dp++;}
  }
  else if ( (map_flag==xaFALSE) || (x11_bytes_pixel==1) )
  { xaUBYTE *iptr = (xaUBYTE *)image; 
    while(i--) 
    { xaULONG r,g,b;
      r = (xaULONG)*dp++; g = (xaULONG)*dp++; b = (xaULONG)*dp++;
      *iptr++ = (xaUBYTE)XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
    }
  }
  else if (x11_bytes_pixel==4)
  { xaULONG *iptr = (xaULONG *)image; 
    while(i--) 
    { xaULONG r,g,b; 
      r = (xaULONG)*dp++; g = (xaULONG)*dp++; b = (xaULONG)*dp++;
      *iptr++ = (xaULONG)XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
    }
  }
  else /* (x11_bytes_pixel==2) */
  { xaUSHORT *iptr = (xaUSHORT *)image; 
    while(i--) 
    { xaULONG r,g,b; 
      r = (xaULONG)*dp++; g = (xaULONG)*dp++; b = (xaULONG)*dp++;
      *iptr++ = (xaUSHORT)XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
    }
  }
  if (map_flag==xaTRUE) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}


/****************** RAW CODEC DEPTH 32 *********************************
 *  0 R G B
 *
 *********************************************************************/
xaULONG QT_Decode_RAW32(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG special = dec_info->special;
  XA_CHDR *chdr = dec_info->chdr;
  xaUBYTE *dp = delta;
  xaULONG special_flag = special & 0x0001;
  xaLONG i = imagex * imagey;
  
  dec_info->xs = dec_info->ys = 0;
  dec_info->xe = imagex; dec_info->ye = imagey;
    /* Indicate we can drop these frames */
  if (dec_info->skip_flag > 0) return(ACT_DLTA_DROP);

  if (chdr) { if (chdr->new_chdr) chdr = chdr->new_chdr; }
  if (special_flag)
  { xaUBYTE *iptr = (xaUBYTE *)image; 
    while(i--) {dp++; *iptr++ = *dp++; *iptr++ = *dp++; *iptr++ = *dp++;}
  }
  else if ( (map_flag==xaFALSE) || (x11_bytes_pixel==1) )
  { xaUBYTE *iptr = (xaUBYTE *)image; 
    while(i--) 
    { xaULONG r,g,b;
      dp++; r = (xaULONG)*dp++; g = (xaULONG)*dp++; b = (xaULONG)*dp++;
      *iptr++ = (xaUBYTE)XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
    }
  }
  else if (x11_bytes_pixel==4)
  { xaULONG *iptr = (xaULONG *)image; 
    while(i--) 
    { xaULONG r,g,b; 
      dp++; r = (xaULONG)*dp++; g = (xaULONG)*dp++; b = (xaULONG)*dp++;
      *iptr++ = (xaULONG)XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
    }
  }
  else /* (x11_bytes_pixel==2) */
  { xaUSHORT *iptr = (xaUSHORT *)image; 
    while(i--) 
    { xaULONG r,g,b; 
      dp++; r = (xaULONG)*dp++; g = (xaULONG)*dp++; b = (xaULONG)*dp++;
      *iptr++ = (xaUSHORT)XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
    }
  }
  if (map_flag==xaTRUE) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}


/********************* * * * *******************************************/
xaULONG QT_Decode_RLE1(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaLONG x,y,d,lines; /* xaLONG min_x,max_x,min_y,max_y; */
  xaUBYTE *dptr;

  dptr = delta;
  dptr += 4;    /* skip codec size */
  d = (*dptr++) << 8;  d |= *dptr++;   /* read code either 0008 or 0000 */
  if (dsize < 8) /* NOP */
  { 
    dec_info->xs = dec_info->ys = dec_info->xe = dec_info->ye = 0;
    return(ACT_DLTA_NOP);
  }
  if (d & 0x0008) /* Header present */
  {
    y = (*dptr++) << 8; y |= *dptr++;           /* start line */
    dptr += 2;                                  /* unknown */
    lines = (*dptr++) << 8; lines |= *dptr++;   /* number of lines */
    dptr += 2;                                  /* unknown */
  }
  else { y = 0; lines = imagey; }
  x = 0; y--; lines++;
  while(lines)				/* loop thru lines */
  {
    xaULONG d,xskip,cnt;

    xskip = *dptr++;				/* skip x pixels */
    cnt = *dptr++;				/* RLE code */
    if (cnt == 0) break; 			/* exit */
    
/* this can be removed */
    if ((xskip == 0x80) && (cnt == 0x00))  /* end of codec */
    {
	  lines = 0; y++; x = 0; 
    }
    else if ((xskip == 0x80) && (cnt == 0xff)) /* skip line */
	{lines--; y++; x = 0; }
    else
    {
      if (xskip & 0x80) {lines--; y++; x = xskip & 0x7f;}
      else x += xskip;

      if (cnt < 0x80)				/* run of data */
      { 
        xaUBYTE *bptr; xaUSHORT *sptr; xaULONG *lptr;
	if ((x11_bytes_pixel==1) || (map_flag==xaFALSE) )
		bptr = (xaUBYTE *)(image + (y * imagex) + (x << 4) );
	else if (x11_bytes_pixel==2)
		sptr = (xaUSHORT *)(image + 2*(y * imagex) + (x << 5) );
        else lptr = (xaULONG *)(image + 4*(y * imagex) + (x << 6) );
        x += cnt;
        while(cnt--) 
        { xaULONG i,mask;
          d = (*dptr++ << 8); d |= *dptr++;
          mask = 0x8000;
          for(i=0;i<16;i++)
          {
            if (map_flag==xaFALSE) 
		{ if (d & mask) *bptr++ = 0;  else *bptr++ = 1; }
            else if (x11_bytes_pixel==1) {if (d & mask) *bptr++=(xaUBYTE)map[0];
					else *bptr++=(xaUBYTE)map[1];}
            else if (x11_bytes_pixel==2) {if (d & mask) *sptr++ =(xaUSHORT)map[0];
					else *sptr++ =(xaUSHORT)map[1]; }
            else { if (d & mask) *lptr++ = (xaULONG)map[0]; 
					else *lptr++ = (xaULONG)map[1]; }
            mask >>= 1;
          }
        }
      } /* end run */ 
      else				/* repeat data */
      { 
        xaUBYTE *bptr; xaUSHORT *sptr; xaULONG *lptr;
	if ((x11_bytes_pixel==1) || (map_flag==xaFALSE) )
		bptr = (xaUBYTE *)(image + (y * imagex) + (x << 4) );
	else if (x11_bytes_pixel==2)
		sptr = (xaUSHORT *)(image + 2*(y * imagex) + (x << 5) );
        else lptr = (xaULONG *)(image + 4*(y * imagex) + (x << 6) );
        cnt = 0x100 - cnt;
        x += cnt;
        d = (*dptr++ << 8); d |= *dptr++;
        while(cnt--) 
        { xaULONG i,mask;
          mask = 0x8000;
          for(i=0;i<16;i++)
          {
            if (map_flag==xaFALSE) 
		{ if (d & mask) *bptr++ = 0;  else *bptr++ = 1; }
            else if (x11_bytes_pixel==1) {if (d & mask) *bptr++=(xaUBYTE)map[0];
					else *bptr++=(xaUBYTE)map[1];}
            else if (x11_bytes_pixel==2) {if (d & mask) *sptr++ =(xaUSHORT)map[0];
					else *sptr++ =(xaUSHORT)map[1]; }
            else { if (d & mask) *lptr++ = (xaULONG)map[0]; 
					else *lptr++ = (xaULONG)map[1]; }
            mask >>= 1;
          }
        }
      } /* end repeat */
    } /* end of code */
  } /* end of lines */
 dec_info->xs = dec_info->ys = 0; dec_info->xe = imagex; dec_info->ye = imagey;
 if (map_flag==xaTRUE) return(ACT_DLTA_MAPD);
 else return(ACT_DLTA_NORM);
}

/********************* * * * *******************************************/
xaULONG QT_Decode_RLE2(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaLONG x,y,lines; /* xaLONG min_x,max_x,min_y,max_y; */
  xaULONG d;
  xaUBYTE *dptr;

  dptr = delta;
  dptr += 4;    /* skip codec size */
  d = (*dptr++) << 8;  d |= *dptr++;   /* read code either 0008 or 0000 */
  if (dsize < 8) /* NOP */
  { 
    dec_info->xs = dec_info->ys = dec_info->xe = dec_info->ye = 0;
DEBUG_LEVEL1 fprintf(stderr,"nop\n"); /* POD */
    return(ACT_DLTA_NOP);
  }
  if (d & 0x0008) /* Header present */
  {
    y = (*dptr++) << 8; y |= *dptr++;           /* start line */
    dptr += 2;                                  /* unknown */
    lines = (*dptr++) << 8; lines |= *dptr++;   /* number of lines */
    dptr += 2;                                  /* unknown */
  }
  else { y = 0; lines = imagey; }

DEBUG_LEVEL1 fprintf(stderr,"d=%d y=%d lines %d\n",d,y,lines); /* POD */

  x = -1; lines++;
  while(lines)				/* loop thru lines */
  {
    xaULONG xskip,cnt;

    if (x == -1) 
    {  xskip = *dptr++;  /* skip x pixels */
       if (xskip == 0) break;  /* end of codec */
    }
    else xskip = 0;


    cnt = *dptr++;				/* RLE code */

DEBUG_LEVEL1 fprintf(stderr," xy <%d,%d> xskip %x cnt %x\n",x,y,xskip,cnt);

    if (cnt == 0) break; 			/* exit */
    
/* this can be removed */
    if (cnt == 0x00)  /* end of codec */
    {
	  lines = 0; y++; x = -1; 
    }
    else if (cnt == 0xff) /* skip line */
    { lines--; y++; x = -1; 

      DEBUG_LEVEL1 fprintf(stderr,"    skip line xy <%d,%d>\n",x,y);
    }
    else
    {
      if (xskip & 0x80) {lines--; y++; x = xskip & 0x7f;}
      else x += xskip;

      DEBUG_LEVEL1 fprintf(stderr,"    cnt %x <%d,%d>\n",cnt,x,y);

      if (cnt < 0x80)				/* run of data */
      { 
        xaUBYTE *bptr; xaUSHORT *sptr; xaULONG *lptr;
	if ((x11_bytes_pixel==1) || (map_flag==xaFALSE) )
		bptr = (xaUBYTE *)(image + (y * imagex) + (x << 4) );
	else if (x11_bytes_pixel==2)
		sptr = (xaUSHORT *)(image + 2*(y * imagex) + (x << 5) );
        else lptr = (xaULONG *)(image + 4*(y * imagex) + (x << 6) );
        x += cnt;
        while(cnt--) 
        { xaULONG i,shift,d0;
          d0 = (*dptr++ << 24);  d0 |= (*dptr++ << 16); 
				d0 |= (*dptr++ << 8);  d0 |= *dptr++;
          shift = 32;
          for(i=0;i<16;i++)
          { shift -= 2;
            d = (d0 >> shift) & 0x03;
            if (map_flag==xaFALSE)	 *bptr++ = (xaUBYTE)d;
            else if (x11_bytes_pixel==1) *bptr++ = (xaUBYTE)map[d];
            else if (x11_bytes_pixel==2) *sptr++ = (xaUSHORT)map[d];
            else			 *lptr++ = (xaULONG)map[d]; 
          }
        }
      } /* end run */ 
      else				/* repeat data */
      { xaULONG d0;
        xaUBYTE *bptr; xaUSHORT *sptr; xaULONG *lptr;
	if ((x11_bytes_pixel==1) || (map_flag==xaFALSE) )
		bptr = (xaUBYTE *)(image + (y * imagex) + (x << 4) );
	else if (x11_bytes_pixel==2)
		sptr = (xaUSHORT *)(image + 2*(y * imagex) + (x << 5) );
        else lptr = (xaULONG *)(image + 4*(y * imagex) + (x << 6) );
        cnt = 0x100 - cnt;
        x += cnt;
        d0 = (*dptr++ << 24);  d0 |= (*dptr++ << 16); 
				d0 |= (*dptr++ << 8);  d0 |= *dptr++;
        while(cnt--) 
        { xaULONG i,shift;
          shift = 32;
          for(i=0;i<16;i++)
          { shift -= 2;
            d = (d0 >> shift) & 0x03;
            if (map_flag==xaFALSE) 	 *bptr++ = (xaUBYTE)d;
            else if (x11_bytes_pixel==1) *bptr++ = (xaUBYTE)map[d];
            else if (x11_bytes_pixel==2) *sptr++ = (xaUSHORT)map[d];
            else			 *lptr++ = (xaULONG)map[d]; 
          }
        }
      } /* end repeat */
    } /* end of code */
  } /* end of lines */
 dec_info->xs = dec_info->ys = 0; dec_info->xe = imagex; dec_info->ye = imagey;
 if (map_flag==xaTRUE) return(ACT_DLTA_MAPD);
 else return(ACT_DLTA_NORM);
}

/********************* * * * *******************************************/
xaULONG QT_Decode_RLE4(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaLONG x,y,lines; /* xaLONG min_x,max_x,min_y,max_y; */
  xaULONG d;
  xaUBYTE *dptr;

  dptr = delta;
  dptr += 4;    /* skip codec size */
  d = (*dptr++) << 8;  d |= *dptr++;   /* read code either 0008 or 0000 */
  if (dsize < 8) /* NOP */
  { 
    dec_info->xs = dec_info->ys = dec_info->xe = dec_info->ye = 0;
DEBUG_LEVEL1 fprintf(stderr,"nop\n"); /* POD */
    return(ACT_DLTA_NOP);
  }
  if (d & 0x0008) /* Header present */
  {
    y = (*dptr++) << 8; y |= *dptr++;           /* start line */
    dptr += 2;                                  /* unknown */
    lines = (*dptr++) << 8; lines |= *dptr++;   /* number of lines */
    dptr += 2;                                  /* unknown */
  }
  else { y = 0; lines = imagey; }

DEBUG_LEVEL1 fprintf(stderr,"d=%d y=%d lines %d\n",d,y,lines); /* POD */

  x = -1; lines++;
  while(lines)				/* loop thru lines */
  {
    xaULONG xskip,cnt;

    if (x == -1) 
    {  xskip = *dptr++;  /* skip x pixels */
       if (xskip == 0) break;  /* end of codec */
    }
    else xskip = 0;


    cnt = *dptr++;				/* RLE code */

DEBUG_LEVEL1 fprintf(stderr," xy <%d,%d> xskip %x cnt %x\n",x,y,xskip,cnt);

    if (cnt == 0) break; 			/* exit */
    
/* this can be removed */
    if (cnt == 0x00)  /* end of codec */
    {
	  lines = 0; y++; x = -1; 
    }
    else if (cnt == 0xff) /* skip line */
    { lines--; y++; x = -1; 

      DEBUG_LEVEL1 fprintf(stderr,"    skip line xy <%d,%d>\n",x,y);
    }
    else
    {
      if (xskip & 0x80) {lines--; y++; x = xskip & 0x7f;}
      else x += xskip;

      DEBUG_LEVEL1 fprintf(stderr,"    cnt %x <%d,%d>\n",cnt,x,y);

      if (cnt < 0x80)				/* run of data */
      { 
        xaUBYTE *bptr; xaUSHORT *sptr; xaULONG *lptr;
	if ((x11_bytes_pixel==1) || (map_flag==xaFALSE) )
		bptr = (xaUBYTE *)(image + (y * imagex) + (x << 3) );
	else if (x11_bytes_pixel==2)
		sptr = (xaUSHORT *)(image + 2*(y * imagex) + (x << 4) );
        else lptr = (xaULONG *)(image + 4*(y * imagex) + (x << 5) );
        x += cnt;
        while(cnt--) 
        { xaULONG i,shift,d0;
          d0 = (*dptr++ << 24);  d0 |= (*dptr++ << 16); 
				d0 |= (*dptr++ << 8);  d0 |= *dptr++;
          shift = 32;
          for(i=0;i<8;i++)
          { shift -= 4;
            d = (d0 >> shift) & 0x0f;
            if (map_flag==xaFALSE)	 *bptr++ = (xaUBYTE)d;
            else if (x11_bytes_pixel==1) *bptr++ = (xaUBYTE)map[d];
            else if (x11_bytes_pixel==2) *sptr++ = (xaUSHORT)map[d];
            else			 *lptr++ = (xaULONG)map[d]; 
          }
        }
      } /* end run */ 
      else				/* repeat data */
      { xaULONG d0;
        xaUBYTE *bptr; xaUSHORT *sptr; xaULONG *lptr;
	if ((x11_bytes_pixel==1) || (map_flag==xaFALSE) )
		bptr = (xaUBYTE *)(image + (y * imagex) + (x << 3) );
	else if (x11_bytes_pixel==2)
		sptr = (xaUSHORT *)(image + 2*(y * imagex) + (x << 4) );
        else lptr = (xaULONG *)(image + 4*(y * imagex) + (x << 5) );
        cnt = 0x100 - cnt;
        x += cnt;
        d0 = (*dptr++ << 24);  d0 |= (*dptr++ << 16); 
				d0 |= (*dptr++ << 8);  d0 |= *dptr++;
        while(cnt--) 
        { xaULONG i,shift;
          shift = 32;
          for(i=0;i<8;i++)
          { shift -= 4;
            d = (d0 >> shift) & 0x0f;
            if (map_flag==xaFALSE) 	 *bptr++ = (xaUBYTE)d;
            else if (x11_bytes_pixel==1) *bptr++ = (xaUBYTE)map[d];
            else if (x11_bytes_pixel==2) *sptr++ = (xaUSHORT)map[d];
            else			 *lptr++ = (xaULONG)map[d]; 
          }
        }
      } /* end repeat */
    } /* end of code */
  } /* end of lines */
 dec_info->xs = dec_info->ys = 0; dec_info->xe = imagex; dec_info->ye = imagey;
 if (map_flag==xaTRUE) return(ACT_DLTA_MAPD);
 else return(ACT_DLTA_NORM);
}


/********************* * * * *******************************************/
xaULONG QT_Decode_RLE8(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaLONG y,lines,d; /* xaLONG min_x,max_x,min_y,max_y;  */
  xaUBYTE *dptr;

  dptr = delta;
  dptr += 4;    /* skip codec size */
  d = (*dptr++) << 8;  d |= *dptr++;   /* read code either 0008 or 0000 */
  if (dsize < 8) /* NOP */
  { dec_info->xs = dec_info->ys = dec_info->xe = dec_info->ye = 0;
    return(ACT_DLTA_NOP);
  }
  if (d & 0x0008) /* Header present */
  {
    y = (*dptr++) << 8; y |= *dptr++;           /* start line */
    dptr += 2;                                  /* unknown */
    lines = (*dptr++) << 8; lines |= *dptr++;   /* number of lines */
    dptr += 2;                                  /* unknown */
  }
  else { y = 0; lines = imagey; }
  while(lines--)
  {
    xaULONG xskip,cnt;
    xskip = *dptr++;				/* skip x pixels */
    if (xskip==0) break;			/* exit */
    cnt = *dptr++;				/* RLE code */
    if ( (x11_bytes_pixel==1) || (map_flag==xaFALSE) )
    { xaUBYTE *iptr = (xaUBYTE *)(image + (y * imagex) + (4 * (xskip-1)) );
      while(cnt != 0xff)
      {
        if (cnt == 0x00) { xskip = *dptr++; iptr += 4 * (xskip-1); }
        else if (cnt < 0x80)			/* run of data */
        {
          cnt *= 4; if (map_flag==xaFALSE) while(cnt--) *iptr++ = (xaUBYTE)*dptr++;
          else while(cnt--) *iptr++ = (xaUBYTE)map[*dptr++];
        } else					/* repeat data */
        { xaUBYTE d1,d2,d3,d4;	cnt = 0x100 - cnt;
          if (map_flag==xaTRUE) { d1=(xaUBYTE)map[*dptr++]; d2=(xaUBYTE)map[*dptr++];
			      d3=(xaUBYTE)map[*dptr++]; d4=(xaUBYTE)map[*dptr++]; }
	  else	{ d1 = (xaUBYTE)*dptr++; d2 = (xaUBYTE)*dptr++;
		  d3 = (xaUBYTE)*dptr++; d4 = (xaUBYTE)*dptr++; }
          while(cnt--) { *iptr++ =d1; *iptr++ =d2; *iptr++ =d3; *iptr++ =d4; }
        } /* end of  >= 0x80 */
        cnt = *dptr++;
      } /* end while cnt */
    } else if (x11_bytes_pixel==2)
    { xaUSHORT *iptr = (xaUSHORT *)(image + 2 *((y * imagex) + (4 * (xskip-1))) );
      while(cnt != 0xff)
      {
        if (cnt == 0x00) { xskip = *dptr++; iptr += 4 * (xskip-1); }
        else if (cnt < 0x80)			/* run of data */
        {
          cnt *= 4; while(cnt--) *iptr++ = (xaUSHORT)map[*dptr++];
        } else					/* repeat data */
        { xaUSHORT d1,d2,d3,d4;	cnt = 0x100 - cnt;
	  { d1 = (xaUSHORT)map[*dptr++]; d2 = (xaUSHORT)map[*dptr++];
	    d3 = (xaUSHORT)map[*dptr++]; d4 = (xaUSHORT)map[*dptr++]; }
          while(cnt--) { *iptr++ =d1; *iptr++ =d2; *iptr++ =d3; *iptr++ =d4; }
        } /* end of  >= 0x80 */
        cnt = *dptr++;
      } /* end while cnt */
    } else /* bytes == 4 */
    { xaULONG *iptr = (xaULONG *)(image + 4 * ((y * imagex) + (4 * (xskip-1))) );
      while(cnt != 0xff)
      {
        if (cnt == 0x00) { xskip = *dptr++; iptr += 4 * (xskip-1); }
        else if (cnt < 0x80)			/* run of data */
        {
          cnt *= 4; while(cnt--) *iptr++ = (xaULONG)map[*dptr++];
        } else					/* repeat data */
        { xaULONG d1,d2,d3,d4; cnt = 0x100 - cnt;
	  { d1 = (xaULONG)map[*dptr++]; d2 = (xaULONG)map[*dptr++]; 
	    d3 = (xaULONG)map[*dptr++]; d4 = (xaULONG)map[*dptr++]; }
          while(cnt--) { *iptr++ =d1; *iptr++ =d2; *iptr++ =d3; *iptr++ =d4; }
        } /* end of  >= 0x80 */
        cnt = *dptr++;
      } /* end while cnt */
    }
    y++;
  } /* end of lines */
 /* one more zero byte */
 dec_info->xs = dec_info->ys = 0; dec_info->xe = imagex; dec_info->ye = imagey;
 if (map_flag==xaTRUE) return(ACT_DLTA_MAPD);
 else return(ACT_DLTA_NORM);
}

/********************* * * * *******************************************/
xaULONG QT_Decode_RLE16(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG special = dec_info->special;
  XA_CHDR *chdr = dec_info->chdr;
  xaLONG y,d,lines; /* xaLONG min_x,max_x,min_y,max_y; */
  xaULONG special_flag;
  xaUBYTE r,g,b,*dptr;

  if (chdr) { if (chdr->new_chdr) chdr = chdr->new_chdr; }

  special_flag = special & 0x0001;

  dptr = delta;
  dptr += 4;    /* skip codec size */
  d = (*dptr++) << 8;  d |= *dptr++;   /* read code either 0008 or 0000 */
  if (dsize < 8) /* NOP */
  {
    dec_info->xs = dec_info->ys = dec_info->xe = dec_info->ye = 0;
    return(ACT_DLTA_NOP);
  }
  if (d & 0x0008) /* Header present */
  {
    y = (*dptr++) << 8; y |= *dptr++;           /* start line */
    dptr += 2;                                  /* unknown */
    lines = (*dptr++) << 8; lines |= *dptr++;   /* number of lines */
    dptr += 2;                                  /* unknown */
  }
  else { y = 0; lines = imagey; }
  while(lines--)				/* loop thru lines */
  {
    xaULONG d,xskip,cnt;
    xskip = *dptr++;				/* skip x pixels */
    if (xskip==0) break;			/* exit */
    cnt = *dptr++;				/* RLE code */

    if (special_flag)
    { xaUBYTE *iptr = (xaUBYTE *)(image + 3*((y * imagex) + (xskip-1)) );
      while(cnt != 0xff)				/* while not EOL */
      {
        if (cnt == 0x00) { xskip = *dptr++; iptr += 3*(xskip-1); }
        else if (cnt < 0x80)				/* run of data */
          while(cnt--) { d = (*dptr++ << 8); d |= *dptr++;
			 QT_Get_RGBColor(&r,&g,&b,d);
			*iptr++ = r; *iptr++ = g; *iptr++ = b; }
        else						/* repeat data */
        { cnt = 0x100 - cnt; d = (*dptr++ << 8); d |= *dptr++;
          QT_Get_RGBColor(&r,&g,&b,d);
          while(cnt--) { *iptr++ = r; *iptr++ = g; *iptr++ = b; }
        }
        cnt = *dptr++;				/* get new RLE code */
      } /* end of line */
    }
    else if ( (x11_bytes_pixel==1) || (map_flag==xaFALSE) )
    { xaUBYTE *iptr = (xaUBYTE *)(image + (y * imagex) + (xskip-1) );
      while(cnt != 0xff)				/* while not EOL */
      {
        if (cnt == 0x00) { xskip = *dptr++; iptr += (xskip-1); }
        else if (cnt < 0x80)				/* run of data */
          while(cnt--) { d = (*dptr++ << 8); d |= *dptr++;
		*iptr++ = (xaUBYTE)QT_Get_Color(d,map_flag,map,chdr); }
        else						/* repeat data */
        { cnt = 0x100 - cnt; d = (*dptr++ << 8); d |= *dptr++;
          d = QT_Get_Color(d,map_flag,map,chdr);
          while(cnt--) { *iptr++ = (xaUBYTE)d; }
        }
        cnt = *dptr++;				/* get new RLE code */
      } /* end of line */
    }
    else if (x11_bytes_pixel==4)
    { xaULONG *iptr = (xaULONG *)(image + 4*((y * imagex)+(xskip-1)) );
      while(cnt != 0xff)				/* while not EOL */
      {
        if (cnt == 0x00) { xskip = *dptr++; iptr += (xskip-1); }
        else if (cnt < 0x80)				/* run of data */
          while(cnt--) { d = (*dptr++ << 8); d |= *dptr++;
		*iptr++ = (xaULONG)QT_Get_Color(d,map_flag,map,chdr); }
        else						/* repeat data */
        { cnt = 0x100 - cnt; d = (*dptr++ << 8); d |= *dptr++;
          d = QT_Get_Color(d,map_flag,map,chdr);
          while(cnt--) { *iptr++ = (xaULONG)d; }
        }
        cnt = *dptr++;				/* get new RLE code */
      } /* end of line */
    }
    else /* if (x11_bytes_pixel==2) */
    { xaUSHORT *iptr = (xaUSHORT *)(image + 2*((y * imagex)+(xskip-1)) );
      while(cnt != 0xff)				/* while not EOL */
      {
        if (cnt == 0x00) { xskip = *dptr++; iptr += (xskip-1); }
        else if (cnt < 0x80)				/* run of data */
          while(cnt--) { d = (*dptr++ << 8); d |= *dptr++;
		*iptr++ = (xaUSHORT)QT_Get_Color(d,map_flag,map,chdr); }
        else						/* repeat data */
        { cnt = 0x100 - cnt; d = (*dptr++ << 8); d |= *dptr++;
          d = QT_Get_Color(d,map_flag,map,chdr);
          while(cnt--) { *iptr++ = (xaUSHORT)d; }
        }
        cnt = *dptr++;				/* get new RLE code */
      } /* end of line */
    }
    y++;
  } /* end of lines */
 /* one more zero byte */
 dec_info->xs = dec_info->ys = 0; dec_info->xe = imagex; dec_info->ye = imagey;
 if (map_flag==xaTRUE) return(ACT_DLTA_MAPD);
 else return(ACT_DLTA_NORM);
}

/********************* * * * *******************************************/
xaULONG QT_Decode_RLE24(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG special = dec_info->special;
  XA_CHDR *chdr = dec_info->chdr;
  xaLONG y,d,lines; /* xaULONG min_x,max_x,min_y,max_y; */
  xaULONG special_flag,r,g,b;
  xaUBYTE *dptr;

  if (chdr) { if (chdr->new_chdr) chdr = chdr->new_chdr; }

  special_flag = special & 0x0001;

  dptr = delta;
  dptr += 4;    /* skip codec size */
  d = (*dptr++) << 8;  d |= *dptr++;   /* read code either 0008 or 0000 */
  if (dsize < 8) /* NOP */
  {  dec_info->xs = dec_info->ys = dec_info->xe = dec_info->ye = 0;
    return(ACT_DLTA_NOP);
  }
  if (d & 0x0008) /* Header present */
  { y = (*dptr++) << 8; y |= *dptr++;		/* start line */
    dptr += 2;					/* unknown */
    lines = (*dptr++) << 8; lines |= *dptr++;	/* number of lines */
    dptr += 2;					/* unknown */
  }
  else { y = 0; lines = imagey; }

  while(lines--)				/* loop thru lines */
  { xaULONG d,xskip,cnt;
    xskip = *dptr++;				/* skip x pixels */
    if (xskip == 0) break; 			/* exit */
    cnt = *dptr++;				/* RLE code */

    if (special_flag)
    { xaUBYTE *iptr = (xaUBYTE *)(image + 3*((y * imagex) + (xskip-1)) );
      while(cnt != 0xff)				/* while not EOL */
      { if (cnt == 0x00) { xskip = *dptr++; iptr += 3*(xskip-1); }
        else if (cnt < 0x80)				/* run of data */
          while(cnt--) { r = *dptr++; g = *dptr++; b = *dptr++;
			*iptr++ = (xaUBYTE)r; *iptr++ = (xaUBYTE)g; 
					    *iptr++ = (xaUBYTE)b; }
        else						/* repeat data */
        { cnt = 0x100 - cnt; r = *dptr++; g = *dptr++; b = *dptr++;
          while(cnt--) { *iptr++ = (xaUBYTE)r; *iptr++ = (xaUBYTE)g; 
					     *iptr++ = (xaUBYTE)b; }
        }
        cnt = *dptr++;				/* get new RLE code */
      } /* end of line */
    }
    else if ( (x11_bytes_pixel==1) || (map_flag==xaFALSE) )
    { xaUBYTE *iptr = (xaUBYTE *)(image + (y * imagex) + (xskip-1) );
      while(cnt != 0xff)				/* while not EOL */
      { if (cnt == 0x00) { xskip = *dptr++; iptr += (xskip-1); }
        else if (cnt < 0x80)				/* run of data */
          while(cnt--) { r = *dptr++; g = *dptr++; b = *dptr++;
		*iptr++ = (xaUBYTE)XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr); }
        else						/* repeat data */
        { cnt = 0x100 - cnt; r = *dptr++; g = *dptr++; b = *dptr++;
          d = XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
          while(cnt--) { *iptr++ = (xaUBYTE)d; }
        }
        cnt = *dptr++;				/* get new RLE code */
      } /* end of line */
    }
    else if (x11_bytes_pixel==4)
    { xaULONG *iptr = (xaULONG *)(image + 4*((y * imagex)+(xskip-1)) );
      while(cnt != 0xff)				/* while not EOL */
      { if (cnt == 0x00) { xskip = *dptr++; iptr += (xskip-1); }
        else if (cnt < 0x80)				/* run of data */
          while(cnt--) { r = *dptr++; g = *dptr++; b = *dptr++;
		*iptr++ = (xaULONG)XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr); }
        else						/* repeat data */
        { cnt = 0x100 - cnt; r = *dptr++; g = *dptr++; b = *dptr++;
          d = XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
          while(cnt--) { *iptr++ = (xaULONG)d; }
        }
        cnt = *dptr++;				/* get new RLE code */
      } /* end of line */
    }
    else /* if (x11_bytes_pixel==2) */
    { xaUSHORT *iptr = (xaUSHORT *)(image + 2*((y * imagex)+(xskip-1)) );
      while(cnt != 0xff)				/* while not EOL */
      {
        if (cnt == 0x00) { xskip = *dptr++; iptr += (xskip-1); }
        else if (cnt < 0x80)				/* run of data */
          while(cnt--) { r = *dptr++; g = *dptr++; b = *dptr++;
	       *iptr++ = (xaUSHORT)XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr); }
        else						/* repeat data */
        { cnt = 0x100 - cnt; r = *dptr++; g = *dptr++; b = *dptr++;
          d = XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
          while(cnt--) { *iptr++ = (xaUSHORT)d; }
        }
        cnt = *dptr++;				/* get new RLE code */
      } /* end of line */
    }
    y++;
  } /* end of lines */
 /* one more zero byte */
  dec_info->xs = dec_info->ys = 0; dec_info->xe = imagex; dec_info->ye = imagey;
  if (map_flag==xaTRUE) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}

/********************* * * * *******************************************/
xaULONG QT_Decode_RLE32(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG special = dec_info->special;
  XA_CHDR *chdr = dec_info->chdr;
  xaLONG y,d,lines; /* xaULONG min_x,max_x,min_y,max_y; */
  xaULONG special_flag,r,g,b;
  xaUBYTE *dp;

  if (chdr) { if (chdr->new_chdr) chdr = chdr->new_chdr; }

  special_flag = special & 0x0001;

  dp = delta;
  dp += 4;    /* skip codec size */
  d = (*dp++) << 8;  d |= *dp++;   /* read code either 0008 or 0000 */
  if (dsize < 8) /* NOP */
  {
    dec_info->xs = dec_info->ys = dec_info->xe = dec_info->ye = 0;
    return(ACT_DLTA_NOP);
  }
  if (d & 0x0008) /* Header present */
  {
    y = (*dp++) << 8; y |= *dp++;           /* start line */
    dp += 2;                                  /* unknown */
    lines = (*dp++) << 8; lines |= *dp++;   /* number of lines */
    dp += 2;                                  /* unknown */
  }
  else { y = 0; lines = imagey; }
  while(lines--)				/* loop thru lines */
  {
    xaULONG d,xskip,cnt;
    xskip = *dp++;				/* skip x pixels */
    if (xskip == 0) break;			/* exit */
    cnt = *dp++;				/* RLE code */

    if (special_flag)
    { xaUBYTE *iptr = (xaUBYTE *)(image + 3*((y * imagex) + (xskip-1)) );
      while(cnt != 0xff)				/* while not EOL */
      {
        if (cnt == 0x00) { xskip = *dp++; iptr += 3*(xskip-1); }
        else if (cnt < 0x80)				/* run of data */
          while(cnt--) { dp++; r = *dp++; g = *dp++; b = *dp++;
			*iptr++ = (xaUBYTE)r; *iptr++ = (xaUBYTE)g; 
					    *iptr++ = (xaUBYTE)b; }
        else						/* repeat data */
        { cnt = 0x100 - cnt; dp++; r = *dp++; g = *dp++; b = *dp++;
          while(cnt--) { *iptr++ = (xaUBYTE)r; *iptr++ = (xaUBYTE)g; 
					     *iptr++ = (xaUBYTE)b; }
        }
        cnt = *dp++;				/* get new RLE code */
      } /* end of line */
    }
    else if ( (x11_bytes_pixel==1) || (map_flag==xaFALSE) )
    { xaUBYTE *iptr = (xaUBYTE *)(image + (y * imagex) + (xskip-1) );
      while(cnt != 0xff)				/* while not EOL */
      {
        if (cnt == 0x00) { xskip = *dp++; iptr += (xskip-1); }
        else if (cnt < 0x80)				/* run of data */
          while(cnt--) { dp++; r = *dp++; g = *dp++; b = *dp++;
		*iptr++ = (xaUBYTE)XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr); }
        else						/* repeat data */
        { cnt = 0x100 - cnt; dp++; r = *dp++; g = *dp++; b = *dp++;
          d = XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
          while(cnt--) { *iptr++ = (xaUBYTE)d; }
        }
        cnt = *dp++;				/* get new RLE code */
      } /* end of line */
    }
    else if (x11_bytes_pixel==4)
    { xaULONG *iptr = (xaULONG *)(image + 4*((y * imagex)+(xskip-1)) );
      while(cnt != 0xff)				/* while not EOL */
      {
        if (cnt == 0x00) { xskip = *dp++; iptr += (xskip-1); }
        else if (cnt < 0x80)				/* run of data */
          while(cnt--) { dp++; r = *dp++; g = *dp++; b = *dp++;
		*iptr++ = (xaULONG)XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr); }
        else						/* repeat data */
        { cnt = 0x100 - cnt; dp++; r = *dp++; g = *dp++; b = *dp++;
          d = XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
          while(cnt--) { *iptr++ = (xaULONG)d; }
        }
        cnt = *dp++;				/* get new RLE code */
      } /* end of line */
    }
    else /* if (x11_bytes_pixel==2) */
    { xaUSHORT *iptr = (xaUSHORT *)(image + 2*((y * imagex)+(xskip-1)) );
      while(cnt != 0xff)				/* while not EOL */
      {
        if (cnt == 0x00) { xskip = *dp++; iptr += (xskip-1); }
        else if (cnt < 0x80)				/* run of data */
          while(cnt--) { dp++; r = *dp++; g = *dp++; b = *dp++;
		*iptr++ = (xaUSHORT)XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr); }
        else						/* repeat data */
        { cnt = 0x100 - cnt; dp++; r = *dp++; g = *dp++; b = *dp++;
          d = XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
          while(cnt--) { *iptr++ = (xaUSHORT)d; }
        }
        cnt = *dp++;				/* get new RLE code */
      } /* end of line */
    }
    y++;
  } /* end of lines */
 /* one more zero byte */
  dec_info->xs = dec_info->ys = 0; dec_info->xe = imagex; dec_info->ye = imagey;
 if (map_flag==xaTRUE) return(ACT_DLTA_MAPD);
 else return(ACT_DLTA_NORM);
}

/********************* * * * *******************************************/
#define QT_BLOCK_INC(x,y,imagex) { x += 4; if (x>=imagex) { x=0; y += 4; }}

#define QT_RPZA_BLOCK_INC(x,y,imagex,im0,im1,im2,im3,binc,rinc) \
{ x += 4; im0 += binc; im1 += binc; im2 += binc; im3 += binc;	\
  if (x>=imagex)					\
  { x=0; y += 4; im0 += rinc; im1 += rinc; im2 += rinc; im3 += rinc; } }

#define QT_MIN_MAX_CHECK(x,y,min_x,min_y,max_x,max_y) {	\
  if (x > max_x) max_x=x; if (y > max_y) max_y=y;	\
  if (x < min_x) min_x=x; if (y < min_y) min_y=y;  }

#define QT_RPZA_C1(ip0,ip1,ip2,ip3,c,CAST) { \
 ip0[0] = ip0[1] = ip0[2] = ip0[3] = (CAST)c; \
 ip1[0] = ip1[1] = ip1[2] = ip1[3] = (CAST)c; \
 ip2[0] = ip2[1] = ip2[2] = ip2[3] = (CAST)c; \
 ip3[0] = ip3[1] = ip3[2] = ip3[3] = (CAST)c; }

#define QT_RPZA_C4(ip,c,mask,CAST); { \
 ip[0] =(CAST)(c[((mask>>6)&0x03)]); ip[1] =(CAST)(c[((mask>>4)&0x03)]); \
 ip[2] =(CAST)(c[((mask>>2)&0x03)]); ip[3] =(CAST)(c[ (mask & 0x03)]); }

#define QT_RPZA_C16(ip0,ip1,ip2,ip3,c,CAST) { \
 ip0[0] = (CAST)(*c++); ip0[1] = (CAST)(*c++); \
 ip0[2] = (CAST)(*c++); ip0[3] = (CAST)(*c++); \
 ip1[0] = (CAST)(*c++); ip1[1] = (CAST)(*c++); \
 ip1[2] = (CAST)(*c++); ip1[3] = (CAST)(*c++); \
 ip2[0] = (CAST)(*c++); ip2[1] = (CAST)(*c++); \
 ip2[2] = (CAST)(*c++); ip2[3] = (CAST)(*c++); \
 ip3[0] = (CAST)(*c++); ip3[1] = (CAST)(*c++); \
 ip3[2] = (CAST)(*c++); ip3[3] = (CAST)(*c  ); }

#define QT_RPZA_rgbC1(ip,r,g,b) { \
 ip[0] = ip[3] = ip[6] = ip[9]  = r; \
 ip[1] = ip[4] = ip[7] = ip[10] = g; \
 ip[2] = ip[5] = ip[8] = ip[11] = b; }

#define QT_RPZA_rgbC4(ip,r,g,b,mask); { xaULONG _idx; \
 _idx = (mask>>6)&0x03; ip[0] = r[_idx]; ip[1] = g[_idx]; ip[2] = b[_idx]; \
 _idx = (mask>>4)&0x03; ip[3] = r[_idx]; ip[4] = g[_idx]; ip[5] = b[_idx]; \
 _idx = (mask>>2)&0x03; ip[6] = r[_idx]; ip[7] = g[_idx]; ip[8] = b[_idx]; \
 _idx =  mask    &0x03; ip[9] = r[_idx]; ip[10] = g[_idx]; ip[11] = b[_idx]; }

#define QT_RPZA_rgbC16(ip,r,g,b) { \
 ip[0]= *r++; ip[1]= *g++; ip[2]= *b++; \
 ip[3]= *r++; ip[4]= *g++; ip[5]= *b++; \
 ip[6]= *r++; ip[7]= *g++; ip[8]= *b++; \
 ip[9]= *r++; ip[10]= *g++; ip[11]= *b++; }


#define RPZA_DITH_COL2RGB(_r,_g,_b,_col) { \
_r = (_col & 0x7c00); _g = (_col & 0x03e0); _b = (_col & 0x001f);	\
_r = _r | (_r >> 5);  _g = (_g << 5) | _g; _b = (_b << 10) | (_b << 5); }

#define RPZA_DITH_GET_RGB(r,g,b,re,ge,be,col) { xaLONG r1,g1,b1; \
  r1 = (xaLONG)rnglimit[(r + re) >> 7];	\
  g1 = (xaLONG)rnglimit[(g + ge) >> 7];	\
  b1 = (xaLONG)rnglimit[(b + be) >> 7];	\
  col = (r1 & 0xe0) | ((g1 & 0xe0) >> 3) | ((b1 & 0xc0) >> 6); }

#define RPZA_DITH_GET_ERR(r,g,b,re,ge,be,col,cmap) { \
  re =  ((xaLONG)(r) - (xaLONG)(cmap[col].red   >> 1)) >> 1; \
  ge =  ((xaLONG)(g) - (xaLONG)(cmap[col].green >> 1)) >> 1; \
  be =  ((xaLONG)(b) - (xaLONG)(cmap[col].blue  >> 1)) >> 1; }


/********************* * * * *******************************************/
xaULONG QT_Decode_RPZA(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG special = dec_info->special;
  XA_CHDR *chdr = dec_info->chdr;
  xaLONG x,y,len,row_inc,blk_inc; 
  xaLONG min_x,max_x,min_y,max_y;
  xaUBYTE *dptr = delta;
  xaULONG code,changed;
  xaUBYTE *im0,*im1,*im2,*im3;

  dec_info->xs = dec_info->ys = 0; dec_info->xe = imagex; dec_info->ye = imagey;

  dptr++;			/* skip  0xe1 */
  len =(*dptr++)<<16; len |= (*dptr++)<< 8; len |= (*dptr++); /* Read Len */
  /* CHECK FOR CORRUPTION - FAIRLY COMMON */
  if (len != dsize)
  { if (xa_verbose==xaTRUE) 
	fprintf(stderr,"QT corruption-skipping this frame %x %x\n",dsize,len);
    return(ACT_DLTA_NOP);
  }
  len -= 4;				/* read 4 already */

  if (chdr) { if (chdr->new_chdr) chdr = chdr->new_chdr; }

   /* special case dither routine - fill out a bit */
  if (   (!special) && (x11_bytes_pixel==1) 
      && (chdr) && (x11_display_type == XA_PSEUDOCOLOR)
      && (cmap_color_func != 4)
      && (cmap_true_to_332 == xaTRUE) && (x11_cmap_size == 256)
      && (xa_dither_flag==xaTRUE)
     )
     return( QT_RPZA_Dither(image,dptr,len,dec_info) );


  max_x = max_y = 0; min_x = imagex; min_y = imagey; changed = 0;
  x = y = 0;
  if (special) blk_inc = 3;
  else if ( (x11_bytes_pixel==1) || (map_flag == xaFALSE) ) blk_inc = 1;
  else if (x11_bytes_pixel==2) blk_inc = 2;
  else blk_inc = 4;
  row_inc = blk_inc * imagex;
  blk_inc *= 4;
  im1 = im0 = image;	im1 += row_inc; 
  im2 = im1;		im2 += row_inc;
  im3 = im2;		im3 += row_inc;
  row_inc *= 3; /* skip 3 rows at a time */

  
  while(len > 0)
  { code = *dptr++; len--;

    if ( (code >= 0xa0) && (code <= 0xbf) )			/* SINGLE */
    {
      xaULONG color,skip;
      changed = 1;
      color = (*dptr++) << 8; color |= *dptr++; len -= 2;
      skip = (code-0x9f);
      if (special)
      { xaUBYTE r,g,b;
        QT_Get_RGBColor(&r,&g,&b,color); 
        while(skip--)
        { xaUBYTE *ip0=im0; xaUBYTE *ip1=im1; xaUBYTE *ip2=im2; xaUBYTE *ip3=im3;
	  QT_RPZA_rgbC1(ip0,r,g,b);
	  QT_RPZA_rgbC1(ip1,r,g,b);
	  QT_RPZA_rgbC1(ip2,r,g,b);
	  QT_RPZA_rgbC1(ip3,r,g,b);
	  QT_MIN_MAX_CHECK(x,y,min_x,min_y,max_x,max_y);
	  QT_RPZA_BLOCK_INC(x,y,imagex,im0,im1,im2,im3,blk_inc,row_inc);
	}
      }
      else /* not special */
      {
        color = QT_Get_Color(color,map_flag,map,chdr); 
        while(skip--)
        {
          if ( (x11_bytes_pixel==1) || (map_flag == xaFALSE) )
          { xaUBYTE *ip0=im0; xaUBYTE *ip1=im1; xaUBYTE *ip2=im2; xaUBYTE *ip3=im3;
	    QT_RPZA_C1(ip0,ip1,ip2,ip3,color,xaUBYTE);
	  }
          else if (x11_bytes_pixel==4)
          { xaULONG *ip0= (xaULONG *)im0; xaULONG *ip1= (xaULONG *)im1; 
	    xaULONG *ip2= (xaULONG *)im2; xaULONG *ip3= (xaULONG *)im3;
	    QT_RPZA_C1(ip0,ip1,ip2,ip3,color,xaULONG);
	  }
          else /* if (x11_bytes_pixel==2) */
          { xaUSHORT *ip0= (xaUSHORT *)im0; xaUSHORT *ip1= (xaUSHORT *)im1; 
	    xaUSHORT *ip2= (xaUSHORT *)im2; xaUSHORT *ip3= (xaUSHORT *)im3;
	    QT_RPZA_C1(ip0,ip1,ip2,ip3,color,xaUSHORT);
	  }
	  QT_MIN_MAX_CHECK(x,y,min_x,min_y,max_x,max_y);
	  QT_RPZA_BLOCK_INC(x,y,imagex,im0,im1,im2,im3,blk_inc,row_inc);
        } /* end of skip-- */
      } /* end not special */
    }
    else if ( (code >= 0x80) && (code <= 0x9f) )		/* SKIP */
    { xaULONG skip = (code-0x7f);
      while(skip--) 
		QT_RPZA_BLOCK_INC(x,y,imagex,im0,im1,im2,im3,blk_inc,row_inc);
    }
    else if ( (code < 0x80) 				/* FOUR/SIXTEEN */ 
	     || ((code >= 0xc0) && (code <= 0xdf)) )
    { xaULONG cA,cB;
      changed = 1;
      /* Get 1st two colors */
      if (code >= 0xc0) { cA = (*dptr++) << 8; cA |= *dptr++; len -= 2; }
      else {cA = (code << 8) | *dptr++; len -= 1;}
      cB = (*dptr++) << 8; cB |= *dptr++; len -= 2;

      /****** SIXTEEN COLOR *******/
      if ( (code < 0x80) && ((cB & 0x8000)==0) ) /* 16 color */
      {
        xaULONG i,d,*clr,c[16];
        xaUBYTE r[16],g[16],b[16];
	if (special)
	{ QT_Get_RGBColor(&r[0],&g[0],&b[0],cA);
          QT_Get_RGBColor(&r[1],&g[1],&b[1],cB);
          for(i=2; i<16; i++)
          {
            d = (*dptr++) << 8; d |= *dptr++; len -= 2;
            QT_Get_RGBColor(&r[i],&g[i],&b[i],d);
          }
	}
	else
	{
	  clr = c;
          *clr++ = QT_Get_Color(cA,map_flag,map,chdr);
          *clr++ = QT_Get_Color(cB,map_flag,map,chdr);
          for(i=2; i<16; i++)
          {
            d = (*dptr++) << 8; d |= *dptr++; len -= 2;
            *clr++ = QT_Get_Color(d,map_flag,map,chdr);
          }
	}
	clr = c;
	if (special)
        { xaUBYTE *ip0=im0; xaUBYTE *ip1=im1; xaUBYTE *ip2=im2; xaUBYTE *ip3=im3;
	  xaUBYTE *tr,*tg,*tb; tr=r; tg=g; tb=b;
	  QT_RPZA_rgbC16(ip0,tr,tg,tb);
	  QT_RPZA_rgbC16(ip1,tr,tg,tb);
	  QT_RPZA_rgbC16(ip2,tr,tg,tb);
	  QT_RPZA_rgbC16(ip3,tr,tg,tb);
	}
	else if ( (x11_bytes_pixel==1) || (map_flag==xaFALSE) )
        { xaUBYTE *ip0=im0; xaUBYTE *ip1=im1; xaUBYTE *ip2=im2; xaUBYTE *ip3=im3;
	  QT_RPZA_C16(ip0,ip1,ip2,ip3,clr,xaUBYTE);
	}
	else if (x11_bytes_pixel==4)
        { xaULONG *ip0= (xaULONG *)im0; xaULONG *ip1= (xaULONG *)im1; 
	  xaULONG *ip2= (xaULONG *)im2; xaULONG *ip3= (xaULONG *)im3;
	  QT_RPZA_C16(ip0,ip1,ip2,ip3,clr,xaULONG);
	}
	else /* if (x11_bytes_pixel==2) */
        { xaUSHORT *ip0= (xaUSHORT *)im0; xaUSHORT *ip1= (xaUSHORT *)im1; 
	  xaUSHORT *ip2= (xaUSHORT *)im2; xaUSHORT *ip3= (xaUSHORT *)im3;
	  QT_RPZA_C16(ip0,ip1,ip2,ip3,clr,xaUSHORT);
	}
	QT_MIN_MAX_CHECK(x,y,min_x,min_y,max_x,max_y);
	QT_RPZA_BLOCK_INC(x,y,imagex,im0,im1,im2,im3,blk_inc,row_inc);
      } /*** END of SIXTEEN COLOR */
      else					/****** FOUR COLOR *******/
      { xaULONG m_cnt,msk0,msk1,msk2,msk3,c[4];
	xaUBYTE r[4],g[4],b[4];

        if (code < 0x80) m_cnt = 1; 
	else m_cnt = code - 0xbf; 

	if (special)		QT_Get_AV_RGBColors(c,r,g,b,cA,cB);
	else			QT_Get_AV_Colors(c,cA,cB,map_flag,map,chdr);

        while(m_cnt--)
        { msk0 = *dptr++; msk1 = *dptr++;
	  msk2 = *dptr++; msk3 = *dptr++; len -= 4;
	  if (special)
          { xaUBYTE *ip0=im0; xaUBYTE *ip1=im1; xaUBYTE *ip2=im2; xaUBYTE *ip3=im3;
	    QT_RPZA_rgbC4(ip0,r,g,b,msk0);
	    QT_RPZA_rgbC4(ip1,r,g,b,msk1);
	    QT_RPZA_rgbC4(ip2,r,g,b,msk2);
	    QT_RPZA_rgbC4(ip3,r,g,b,msk3);
	  }
	  else if ( (x11_bytes_pixel==1) || (map_flag==xaFALSE) )
          { xaUBYTE *ip0=im0; xaUBYTE *ip1=im1; xaUBYTE *ip2=im2; xaUBYTE *ip3=im3;
	    QT_RPZA_C4(ip0,c,msk0,xaUBYTE);
	    QT_RPZA_C4(ip1,c,msk1,xaUBYTE);
	    QT_RPZA_C4(ip2,c,msk2,xaUBYTE);
	    QT_RPZA_C4(ip3,c,msk3,xaUBYTE);
	  }
	  else if (x11_bytes_pixel==4)
          { xaULONG *ip0= (xaULONG *)im0; xaULONG *ip1= (xaULONG *)im1; 
	    xaULONG *ip2= (xaULONG *)im2; xaULONG *ip3= (xaULONG *)im3;
	    QT_RPZA_C4(ip0,c,msk0,xaULONG);
	    QT_RPZA_C4(ip1,c,msk1,xaULONG);
	    QT_RPZA_C4(ip2,c,msk2,xaULONG);
	    QT_RPZA_C4(ip3,c,msk3,xaULONG);
	  }
	  else /* if (x11_bytes_pixel==2) */
          { xaUSHORT *ip0= (xaUSHORT *)im0; xaUSHORT *ip1= (xaUSHORT *)im1; 
	    xaUSHORT *ip2= (xaUSHORT *)im2; xaUSHORT *ip3= (xaUSHORT *)im3;
	    QT_RPZA_C4(ip0,c,msk0,xaUSHORT);
	    QT_RPZA_C4(ip1,c,msk1,xaUSHORT);
	    QT_RPZA_C4(ip2,c,msk2,xaUSHORT);
	    QT_RPZA_C4(ip3,c,msk3,xaUSHORT);
	  }
	  QT_MIN_MAX_CHECK(x,y,min_x,min_y,max_x,max_y);
	  QT_RPZA_BLOCK_INC(x,y,imagex,im0,im1,im2,im3,blk_inc,row_inc);
        }  
      } /*** END of FOUR COLOR *******/
    } /*** END of 4/16 COLOR BLOCKS ****/
    else /* UNKNOWN */
    {
      fprintf(stderr,"QT RPZA: Unknown %x\n",code);
      return(ACT_DLTA_NOP);
    }
  }
  if (xa_optimize_flag == xaTRUE)
  {
    if (changed) { dec_info->xs=min_x; dec_info->ys=min_y;
		   dec_info->xe=max_x + 4; dec_info->ye=max_y + 4; }
    else  { dec_info->xs = dec_info->ys = dec_info->xe = dec_info->ye = 0;
	    return(ACT_DLTA_NOP); }
  }
  else { dec_info->xs = dec_info->ys = 0; 
	 dec_info->xe = imagex; dec_info->ye = imagey; }
  if (map_flag) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}

void QT_Get_RGBColor(r,g,b,color)
xaUBYTE *r,*g,*b;
xaULONG color;
{ xaULONG ra,ga,ba;
  ra = (color >> 10) & 0x1f;	ra = (ra << 3) | (ra >> 2);
  ga = (color >>  5) & 0x1f;	ga = (ga << 3) | (ga >> 2);
  ba =  color & 0x1f;		ba = (ba << 3) | (ba >> 2);
  *r = ra; *g = ga; *b = ba;
}

void QT_Get_RGBColorL(r,g,b,color)
xaLONG *r,*g,*b;
xaULONG color;
{ xaULONG ra,ga,ba;
  ra = (color >> 10) & 0x1f;	ra = (ra << 3) | (ra >> 2);
  ga = (color >>  5) & 0x1f;	ga = (ga << 3) | (ga >> 2);
  ba =  color & 0x1f;		ba = (ba << 3) | (ba >> 2);
  *r = ra; *g = ga; *b = ba;
}

xaULONG QT_Get_Color(color,map_flag,map,chdr)
xaULONG color,map_flag,*map;
XA_CHDR *chdr;
{
  register xaULONG clr,ra,ga,ba,ra5,ga5,ba5;

  ra5 = (color >> 10) & 0x1f;
  ga5 = (color >>  5) & 0x1f;
  ba5 =  color & 0x1f;
  ra = qt_gamma_adj[ra5]; ga = qt_gamma_adj[ga5]; ba = qt_gamma_adj[ba5];

  if (x11_display_type & XA_X11_TRUE) clr = X11_Get_True_Color(ra,ga,ba,16);
  else 
  { 
    if ((cmap_color_func == 4) && (chdr))
    { register xaULONG cache_i = color & 0x7fff;
      if (cmap_cache == 0) CMAP_Cache_Init(0);
      if (chdr != cmap_cache_chdr)
      {
        CMAP_Cache_Clear();
        cmap_cache_chdr = chdr;
      }
      if (cmap_cache[cache_i] == 0xffff)
      {
        clr = chdr->coff +
           CMAP_Find_Closest(chdr->cmap,chdr->csize,ra,ga,ba,16,16,16,xaTRUE);
        cmap_cache[cache_i] = (xaUSHORT)clr;
      }
      else clr = (xaULONG)cmap_cache[cache_i];
    }
    else
    {
      if (cmap_true_to_332 == xaTRUE) clr = CMAP_GET_332(ra5,ga5,ba5,CMAP_SCALE5);
      else			  clr = CMAP_GET_GRAY(ra5,ga5,ba5,CMAP_SCALE10);
      if (map_flag) clr = map[clr];
    }
  }
  return(clr);
}


void QT_Get_AV_RGBColors(c,r,g,b,cA,cB)
xaULONG *c;
xaUBYTE *r,*g,*b;
xaULONG cA,cB;
{ xaULONG rA,gA,bA,rB,gB,bB,ra,ga,ba;
/**color 3 ***/
  rA = (cA >> 10) & 0x1f;	r[3] = (rA << 3) | (rA >> 2);
  gA = (cA >>  5) & 0x1f;	g[3] = (gA << 3) | (gA >> 2);
  bA =  cA & 0x1f;		b[3] = (bA << 3) | (bA >> 2);
/**color 0 ***/
  rB = (cB >> 10) & 0x1f;	r[0] = (rB << 3) | (rB >> 2);
  gB = (cB >>  5) & 0x1f;	g[0] = (gB << 3) | (gB >> 2);
  bB =  cB & 0x1f;		b[0] = (bB << 3) | (bB >> 2);
/**color 2 ***/
  ra = (21*rA + 11*rB) >> 5;	r[2] = (ra << 3) | (ra >> 2);
  ga = (21*gA + 11*gB) >> 5;	g[2] = (ga << 3) | (ga >> 2);
  ba = (21*bA + 11*bB) >> 5;	b[2] = (ba << 3) | (ba >> 2);
/**color 1 ***/
  ra = (11*rA + 21*rB) >> 5;	r[1] = (ra << 3) | (ra >> 2);
  ga = (11*gA + 21*gB) >> 5;	g[1] = (ga << 3) | (ga >> 2);
  ba = (11*bA + 21*bB) >> 5;	b[1] = (ba << 3) | (ba >> 2);
}

void QT_Get_AV_DITH_RGB(r,g,b,cA,cB)
xaLONG *r,*g,*b;
xaULONG cA,cB;
{ xaULONG rA,gA,bA,rB,gB,bB,ra,ga,ba;
/**color 3 ***/
  rA = (cA >> 10) & 0x1f;	r[3] = (rA << 9) | (rA << 4);
  gA = (cA >>  5) & 0x1f;	g[3] = (gA << 9) | (gA << 4);
  bA =  cA & 0x1f;		b[3] = (bA << 9) | (bA << 4);
/**color 0 ***/
  rB = (cB >> 10) & 0x1f;	r[0] = (rB << 9) | (rB << 4);
  gB = (cB >>  5) & 0x1f;	g[0] = (gB << 9) | (gB << 4);
  bB =  cB & 0x1f;		b[0] = (bB << 9) | (bB << 4);
/**color 2 ***/
  ra = (21*rA + 11*rB) >> 5;	r[2] = (ra << 9) | (ra << 4);
  ga = (21*gA + 11*gB) >> 5;	g[2] = (ga << 9) | (ga << 4);
  ba = (21*bA + 11*bB) >> 5;	b[2] = (ba << 9) | (ba << 4);
/**color 1 ***/
  ra = (11*rA + 21*rB) >> 5;	r[1] = (ra << 9) | (ra << 4);
  ga = (11*gA + 21*gB) >> 5;	g[1] = (ga << 9) | (ga << 4);
  ba = (11*bA + 21*bB) >> 5;	b[1] = (ba << 9) | (ba << 4);
}

void QT_Get_AV_Colors(c,cA,cB,map_flag,map,chdr)
xaULONG *c;
xaULONG cA,cB,map_flag,*map;
XA_CHDR *chdr;
{
  xaULONG clr,rA,gA,bA,rB,gB,bB,r0,g0,b0,r1,g1,b1;
  xaULONG rA5,gA5,bA5,rB5,gB5,bB5;
  xaULONG r05,g05,b05,r15,g15,b15;

/*color 3*/
  rA5 = (cA >> 10) & 0x1f;
  gA5 = (cA >>  5) & 0x1f;
  bA5 =  cA & 0x1f;
/*color 0*/
  rB5 = (cB >> 10) & 0x1f;
  gB5 = (cB >>  5) & 0x1f;
  bB5 =  cB & 0x1f;
/*color 2*/
  r05 = (21*rA5 + 11*rB5) >> 5;
  g05 = (21*gA5 + 11*gB5) >> 5;
  b05 = (21*bA5 + 11*bB5) >> 5;
/*color 1*/
  r15 = (11*rA5 + 21*rB5) >> 5;
  g15 = (11*gA5 + 21*gB5) >> 5;
  b15 = (11*bA5 + 21*bB5) >> 5;
/*adj and scale to 16 bits */
  rA=qt_gamma_adj[rA5]; gA=qt_gamma_adj[gA5]; bA=qt_gamma_adj[bA5];
  rB=qt_gamma_adj[rB5]; gB=qt_gamma_adj[gB5]; bB=qt_gamma_adj[bB5];
  r0=qt_gamma_adj[r05]; g0=qt_gamma_adj[g05]; b0=qt_gamma_adj[b05];
  r1=qt_gamma_adj[r15]; g1=qt_gamma_adj[g15]; b1=qt_gamma_adj[b15];

  /*** 1st Color **/
  if (x11_display_type & XA_X11_TRUE) clr = X11_Get_True_Color(rA,gA,bA,16);
  else 
  { 
    if ((cmap_color_func == 4) && (chdr))
    { register xaULONG cache_i = cA & 0x7fff;
      if (cmap_cache == 0) CMAP_Cache_Init(0);
      if (chdr != cmap_cache_chdr)
      {
        CMAP_Cache_Clear();
        cmap_cache_chdr = chdr;
      }
      if (cmap_cache[cache_i] == 0xffff)
      {
        clr = chdr->coff +
           CMAP_Find_Closest(chdr->cmap,chdr->csize,rA,gA,bA,16,16,16,xaTRUE);
        cmap_cache[cache_i] = (xaUSHORT)clr;
      }
      else clr = (xaULONG)cmap_cache[cache_i];
    }
    else
    { if (cmap_true_to_332 == xaTRUE) clr = CMAP_GET_332(rA5,gA5,bA5,CMAP_SCALE5);
      else	clr = CMAP_GET_GRAY(rA5,gA5,bA5,CMAP_SCALE10);
      if (map_flag) clr = map[clr];
    }
  }
  c[3] = clr;

  /*** 2nd Color **/
  if (x11_display_type & XA_X11_TRUE) clr = X11_Get_True_Color(rB,gB,bB,16);
  else 
  { 
    if ((cmap_color_func == 4) && (chdr))
    { register xaULONG cache_i = cB & 0x7fff;
      if (cmap_cache[cache_i] == 0xffff)
      {
        clr = chdr->coff +
           CMAP_Find_Closest(chdr->cmap,chdr->csize,rB,gB,bB,16,16,16,xaTRUE);
        cmap_cache[cache_i] = (xaUSHORT)clr;
      }
      else clr = (xaULONG)cmap_cache[cache_i];
    }
    else
    { if (cmap_true_to_332 == xaTRUE) clr = CMAP_GET_332(rB5,gB5,bB5,CMAP_SCALE5);
      else	clr = CMAP_GET_GRAY(rB5,gB5,bB5,CMAP_SCALE10);
      if (map_flag) clr = map[clr];
    }
  }
  c[0] = clr;

  /*** 1st Av ****/
  if (x11_display_type & XA_X11_TRUE) clr = X11_Get_True_Color(r0,g0,b0,16);
  else 
  { 
    if ((cmap_color_func == 4) && (chdr))
    { register xaULONG cache_i;
      cache_i = (xaULONG)(r05 << 10) | (g05 << 5) | b05;
      if (cmap_cache[cache_i] == 0xffff)
      {
        clr = chdr->coff +
           CMAP_Find_Closest(chdr->cmap,chdr->csize,r0,g0,b0,16,16,16,xaTRUE);
        cmap_cache[cache_i] = (xaUSHORT)clr;
      }
      else clr = (xaULONG)cmap_cache[cache_i];
    }
    else
    { if (cmap_true_to_332 == xaTRUE) clr = CMAP_GET_332(r05,g05,b05,CMAP_SCALE5);
      else	clr = CMAP_GET_GRAY(r05,g05,b05,CMAP_SCALE10);
      if (map_flag) clr = map[clr];
    }
  }
  c[2] = clr;

  /*** 2nd Av ****/
  if (x11_display_type & XA_X11_TRUE) clr = X11_Get_True_Color(r1,g1,b1,16);
  else 
  { 
    if ((cmap_color_func == 4) && (chdr))
    { register xaULONG cache_i;
      cache_i = (xaULONG)(r15 << 10) | (g15 << 5) | b15;
      if (cmap_cache[cache_i] == 0xffff)
      {
        clr = chdr->coff +
           CMAP_Find_Closest(chdr->cmap,chdr->csize,r1,g1,b1,16,16,16,xaTRUE);
        cmap_cache[cache_i] = (xaUSHORT)clr;
      }
      else clr = (xaULONG)cmap_cache[cache_i];
    }
    else
    { if (cmap_true_to_332 == xaTRUE) clr = CMAP_GET_332(r15,g15,b15,CMAP_SCALE5);
      else	clr = CMAP_GET_GRAY(r15,g15,b15,CMAP_SCALE10);
      if (map_flag) clr = map[clr];
    }
  }
  c[1] = clr;
}



#define QT_SMC_O2I(i,o,rinc) { \
*i++ = *o++; *i++ = *o++; *i++ = *o++; *i++ = *o++; i += rinc; o += rinc; \
*i++ = *o++; *i++ = *o++; *i++ = *o++; *i++ = *o++; i += rinc; o += rinc; \
*i++ = *o++; *i++ = *o++; *i++ = *o++; *i++ = *o++; i += rinc; o += rinc; \
*i++ = *o++; *i++ = *o++; *i++ = *o++; *i++ = *o++;  } 

#define QT_SMC_C1(i,c,rinc) { \
*i++ = c; *i++ = c; *i++ = c; *i++ = c;  i += rinc; \
*i++ = c; *i++ = c; *i++ = c; *i++ = c;  i += rinc; \
*i++ = c; *i++ = c; *i++ = c; *i++ = c;  i += rinc; \
*i++ = c; *i++ = c; *i++ = c; *i++ = c;  i += rinc; }

#define QT_SMC_C2(i,c0,c1,msk,rinc) { \
*i++ =(msk&0x80)?c1:c0; *i++ =(msk&0x40)?c1:c0; \
*i++ =(msk&0x20)?c1:c0; *i++ =(msk&0x10)?c1:c0; i += rinc; \
*i++ =(msk&0x08)?c1:c0; *i++ =(msk&0x04)?c1:c0; \
*i++ =(msk&0x02)?c1:c0; *i++ =(msk&0x01)?c1:c0; }

#define QT_SMC_C4(i,CST,c,mska,mskb,rinc) { \
*i++ = (CST)(c[(mska>>6) & 0x03]); *i++ = (CST)(c[(mska>>4) & 0x03]); \
*i++ = (CST)(c[(mska>>2) & 0x03]); *i++ = (CST)(c[mska & 0x03]); i+=rinc; \
*i++ = (CST)(c[(mskb>>6) & 0x03]); *i++ = (CST)(c[(mskb>>4) & 0x03]); \
*i++ = (CST)(c[(mskb>>2) & 0x03]); *i++ = (CST)(c[mskb & 0x03]); }

#define QT_SMC_C8(i,CST,c,msk,rinc) { \
*i++ = (CST)(c[(msk>>21) & 0x07]); *i++ = (CST)(c[(msk>>18) & 0x07]); \
*i++ = (CST)(c[(msk>>15) & 0x07]); *i++ = (CST)(c[(msk>>12) & 0x07]); i+=rinc; \
*i++ = (CST)(c[(msk>> 9) & 0x07]); *i++ = (CST)(c[(msk>> 6) & 0x07]); \
*i++ = (CST)(c[(msk>> 3) & 0x07]); *i++ = (CST)(c[msk & 0x07]); }

#define QT_SMC_C16m(i,dp,CST,map,rinc) { \
*i++ =(CST)map[*dp++]; *i++ =(CST)map[*dp++]; \
*i++ =(CST)map[*dp++]; *i++ =(CST)map[*dp++]; i += rinc; \
*i++ =(CST)map[*dp++]; *i++ =(CST)map[*dp++]; \
*i++ =(CST)map[*dp++]; *i++ =(CST)map[*dp++]; }

#define QT_SMC_C16(i,dp,CST) { \
*i++ =(CST)(*dp++); *i++ =(CST)(*dp++); \
*i++ =(CST)(*dp++); *i++ =(CST)(*dp++); }

xaULONG
QT_Decode_SMC(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaLONG x,y,len,row_inc; /* xaLONG min_x,max_x,min_y,max_y; */
  xaUBYTE *dptr;
  xaULONG i,cnt,hicode,code;
  xaULONG *c;

  smc_8cnt = smc_Acnt = smc_Ccnt = 0;

  dec_info->xs = dec_info->ys = 0; dec_info->xe = imagex; dec_info->ye = imagey;
  dptr = delta;
  x = y = 0;
  row_inc = imagex - 4;

  dptr++;				/* skip over 0xe1 */
  len =(*dptr++)<<16; len |= (*dptr++)<< 8; len |= (*dptr++); /* Read Len */
  len -= 4;				/* read 4 already */
  while(len > 0)
  {
    code = *dptr++; len--; hicode = code & 0xf0;
    switch(hicode)
    {
      case 0x00: /* SKIPs */
      case 0x10:
	if (hicode == 0x10) {cnt = 1 + *dptr++; len -= 1;}
	else cnt = 1 + (code & 0x0f);
        while(cnt--) {x += 4; if (x >= imagex) { x = 0; y += 4; } }
	break;
      case 0x20: /* Repeat Last Block */
      case 0x30:
	{ xaLONG tx,ty;
	  if (hicode == 0x30) {cnt = 1 + *dptr++; len--;}
	  else cnt = 1 + (code & 0x0f);
	  if (x==0) {ty = y-4; tx = imagex-4;} else {ty=y; tx = x-4;}

	  while(cnt--)
	  { 
	    if ( (x11_bytes_pixel==1) || (map_flag==xaFALSE) )
	    { xaUBYTE *i_ptr = (xaUBYTE *)(image + y * imagex + x);
	      xaUBYTE *o_ptr = (xaUBYTE *)(image + ty * imagex + tx);
	      QT_SMC_O2I(i_ptr,o_ptr,row_inc);
	    } else if (x11_bytes_pixel==2)
	    { xaUSHORT *i_ptr = (xaUSHORT *)(image + 2*(y * imagex + x) );
	      xaUSHORT *o_ptr = (xaUSHORT *)(image + 2*(ty * imagex + tx) );
	      QT_SMC_O2I(i_ptr,o_ptr,row_inc);
	    } else /* if (x11_bytes_pixel==4) */
	    { xaULONG *i_ptr = (xaULONG *)(image + 4*(y * imagex + x) );
	      xaULONG *o_ptr = (xaULONG *)(image + 4*(ty * imagex + tx) );
	      QT_SMC_O2I(i_ptr,o_ptr,row_inc);
	    }
	    x += 4; if (x >= imagex) { x = 0; y += 4; }
	  }
	}
	break;
      case 0x40: /* */
      case 0x50:
	{ xaULONG cnt,cnt1;
	  xaULONG m_cnt,m_cnt1;
	  xaLONG m_tx,m_ty;
          xaLONG tx,ty;
	  if (hicode == 0x50) 
	  {  
	     m_cnt1 = 1 + *dptr++; len--; 
	     m_cnt = 2;
	  }
          else 
	  {
	    m_cnt1 = (1 + (code & 0x0f));
	    m_cnt = 2;
	  }
          m_tx = x-(xaLONG)(4 * m_cnt); m_ty = y; 
	  if (m_tx < 0) {m_tx += imagex; m_ty -= 4;}
	  cnt1 = m_cnt1;
	  while(cnt1--)
	  {
	    cnt = m_cnt; tx = m_tx; ty = m_ty;
	    while(cnt--)
	    { 
	      if ( (x11_bytes_pixel==1) || (map_flag==xaFALSE) )
	      { xaUBYTE *i_ptr = (xaUBYTE *)(image + y * imagex + x);
		xaUBYTE *o_ptr = (xaUBYTE *)(image + ty * imagex + tx);
		QT_SMC_O2I(i_ptr,o_ptr,row_inc);
	      } else if (x11_bytes_pixel==2)
	      { xaUSHORT *i_ptr = (xaUSHORT *)(image + 2*(y * imagex + x));
		xaUSHORT *o_ptr = (xaUSHORT *)(image + 2*(ty * imagex + tx));
		QT_SMC_O2I(i_ptr,o_ptr,row_inc);
	      } else 
	      { xaULONG *i_ptr = (xaULONG *)(image + 4*(y * imagex + x));
		xaULONG *o_ptr = (xaULONG *)(image + 4*(ty * imagex + tx));
		QT_SMC_O2I(i_ptr,o_ptr,row_inc);
	      }
	      x += 4; if (x >= imagex) { x = 0; y += 4; }
	      tx += 4; if (tx >= imagex) { tx = 0; ty += 4; }
	    } /* end of cnt */
	  } /* end of cnt1 */
	}
	break;

      case 0x60: /* Repeat Data */
      case 0x70:
	{ xaULONG ct,cnt;
	  if (hicode == 0x70) {cnt = 1 + *dptr++; len--;}
	  else cnt = 1 + (code & 0x0f);
	  ct = (map_flag)?(map[*dptr++]):(xaULONG)(*dptr++); len--;
	  while(cnt--)
	  {
	    if ( (x11_bytes_pixel==1) || (map_flag==xaFALSE) )
	    { xaUBYTE *i_ptr = (xaUBYTE *)(image + y * imagex + x);
	      xaUBYTE d = (xaUBYTE)(ct);
	      QT_SMC_C1(i_ptr,d,row_inc);
	    } else if (x11_bytes_pixel==2)
	    { xaUSHORT *i_ptr = (xaUSHORT *)(image + 2*(y * imagex + x));
	      xaUSHORT d = (xaUBYTE)(ct);
	      QT_SMC_C1(i_ptr,d,row_inc);
	    } else
	    { xaULONG *i_ptr = (xaULONG *)(image + 4*(y * imagex + x));
	      QT_SMC_C1(i_ptr,ct,row_inc);
	    }
	    x += 4; if (x >= imagex) { x = 0; y += 4; }
	  }
	}
	break;

      case 0x80: /* 2 colors plus 16 mbits per */
      case 0x90:
        { xaULONG cnt = 1 + (code & 0x0f);
	  if (hicode == 0x80)
	  {
            c = (xaULONG *)&smc_8[ (smc_8cnt * 2) ];  len -= 2;
	    smc_8cnt++; if (smc_8cnt >= SMC_MAX_CNT) smc_8cnt -= SMC_MAX_CNT;
	    for(i=0;i<2;i++) {c[i]=(map_flag)?map[*dptr++]:(xaULONG)(*dptr++);}
	  }
          else { c = (xaULONG *)&smc_8[ ((xaULONG)(*dptr++) << 1) ]; len--; }
	  while(cnt--)
	  { xaULONG msk1,msk0;
	    msk0 = *dptr++; msk1 = *dptr++;  len-= 2;
	    if ( (x11_bytes_pixel==1) || (map_flag==xaFALSE) )
	    { xaUBYTE *i_ptr = (xaUBYTE *)(image + y * imagex + x);
	      xaUBYTE c0=(xaUBYTE)c[0];	xaUBYTE c1=(xaUBYTE)c[1];
	      QT_SMC_C2(i_ptr,c0,c1,msk0,row_inc); i_ptr += row_inc;
	      QT_SMC_C2(i_ptr,c0,c1,msk1,row_inc);
	    } else if (x11_bytes_pixel==2)
	    { xaUSHORT *i_ptr = (xaUSHORT *)(image + 2*(y * imagex + x));
	      xaUSHORT c0=(xaUSHORT)c[0];	xaUSHORT c1=(xaUSHORT)c[1];
	      QT_SMC_C2(i_ptr,c0,c1,msk0,row_inc); i_ptr += row_inc;
	      QT_SMC_C2(i_ptr,c0,c1,msk1,row_inc);
	    } else
	    { xaULONG *i_ptr = (xaULONG *)(image + 4*(y * imagex + x));
	      xaULONG c0=(xaULONG)c[0];	xaULONG c1=(xaULONG)c[1];
	      QT_SMC_C2(i_ptr,c0,c1,msk0,row_inc); i_ptr += row_inc;
	      QT_SMC_C2(i_ptr,c0,c1,msk1,row_inc);
	    }
	    x += 4; if (x >= imagex) { x = 0; y += 4; }
          } 
        } 
	break;

      case 0xA0: /* 4 color + 32 mbits */
      case 0xB0:
        { xaULONG cnt = 1 + (code & 0xf);
          if (hicode == 0xA0)
          {
            c = (xaULONG *)&smc_A[ (smc_Acnt << 2) ]; len -= 4;
            smc_Acnt++; if (smc_Acnt >= SMC_MAX_CNT) smc_Acnt -= SMC_MAX_CNT;
            for(i=0;i<4;i++) {c[i]=(map_flag)?map[*dptr++]:(xaULONG)(*dptr++);}
          }
          else { c = (xaULONG *)&smc_A[ ((xaULONG)(*dptr++) << 2) ]; len--; }
	  while(cnt--)
	  { xaUBYTE msk0,msk1,msk2,msk3; 
	    msk0 = *dptr++;	msk1 = *dptr++; 
	    msk2 = *dptr++;	msk3 = *dptr++;		len -= 4;
	    if ( (x11_bytes_pixel==1) || (map_flag==xaFALSE) )
	    { xaUBYTE *i_ptr = (xaUBYTE *)(image + y * imagex + x);
	      QT_SMC_C4(i_ptr,xaUBYTE,c,msk0,msk1,row_inc); i_ptr += row_inc;
	      QT_SMC_C4(i_ptr,xaUBYTE,c,msk2,msk3,row_inc);
	    } else if (x11_bytes_pixel==2)
	    { xaUSHORT *i_ptr = (xaUSHORT *)(image + 2*(y * imagex + x));
	      QT_SMC_C4(i_ptr,xaUSHORT,c,msk0,msk1,row_inc); i_ptr += row_inc;
	      QT_SMC_C4(i_ptr,xaUSHORT,c,msk2,msk3,row_inc);
	    } else
	    { xaULONG *i_ptr = (xaULONG *)(image + 4*(y * imagex + x));
	      QT_SMC_C4(i_ptr,xaULONG,c,msk0,msk1,row_inc); i_ptr += row_inc;
	      QT_SMC_C4(i_ptr,xaULONG,c,msk2,msk3,row_inc);
	    }
	    x += 4; if (x >= imagex) { x = 0; y += 4; }
          } 
        } 
	break;

      case 0xC0: /* 8 colors + 48 mbits */
      case 0xD0:
        { xaULONG cnt = 1 + (code & 0xf);
          if (hicode == 0xC0)
          {
            c = (xaULONG *)&smc_C[ (smc_Ccnt << 3) ];   len -= 8;
            smc_Ccnt++; if (smc_Ccnt >= SMC_MAX_CNT) smc_Ccnt -= SMC_MAX_CNT;
            for(i=0;i<8;i++) {c[i]=(map_flag)?map[*dptr++]:(xaULONG)(*dptr++);}
          }
          else { c = (xaULONG *)&smc_C[ ((xaULONG)(*dptr++) << 3) ]; len--; }

	  while(cnt--)
	  { xaULONG t,mbits0,mbits1;
	    t = (*dptr++) << 8; t |= *dptr++;
	    mbits0  = (t & 0xfff0) << 8;  mbits1  = (t & 0x000f) << 8;
	    t = (*dptr++) << 8; t |= *dptr++;
	    mbits0 |= (t & 0xfff0) >> 4;  mbits1 |= (t & 0x000f) << 4;
	    t = (*dptr++) << 8; t |= *dptr++;
	    mbits1 |= (t & 0xfff0) << 8;  mbits1 |= (t & 0x000f);
	    len -= 6;
	    if ( (x11_bytes_pixel==1) || (map_flag==xaFALSE) )
	    { xaUBYTE *i_ptr = (xaUBYTE *)(image + y * imagex + x);
	      QT_SMC_C8(i_ptr,xaUBYTE,c,mbits0,row_inc); i_ptr += row_inc;
	      QT_SMC_C8(i_ptr,xaUBYTE,c,mbits1,row_inc);
	    } else if (x11_bytes_pixel==2)
	    { xaUSHORT *i_ptr = (xaUSHORT *)(image + 2*(y * imagex + x));
	      QT_SMC_C8(i_ptr,xaUSHORT,c,mbits0,row_inc); i_ptr += row_inc;
	      QT_SMC_C8(i_ptr,xaUSHORT,c,mbits1,row_inc);
	    } else
	    { xaULONG *i_ptr = (xaULONG *)(image + 4*(y * imagex + x));
	      QT_SMC_C8(i_ptr,xaULONG,c,mbits0,row_inc); i_ptr += row_inc;
	      QT_SMC_C8(i_ptr,xaULONG,c,mbits1,row_inc);
	    }
	    x += 4; if (x >= imagex) { x = 0; y += 4; }
          } 
        } 
	break;

      case 0xE0: /* 16 colors */
        { xaULONG cnt = 1 + (code & 0x0f);
	  while(cnt--)
	  { 
	    if (map_flag==xaFALSE)
	    { xaUBYTE *i_ptr = (xaUBYTE *)(image + y * imagex + x);
	      QT_SMC_C16(i_ptr,dptr,xaUBYTE); i_ptr += row_inc;
	      QT_SMC_C16(i_ptr,dptr,xaUBYTE); i_ptr += row_inc;
	      QT_SMC_C16(i_ptr,dptr,xaUBYTE); i_ptr += row_inc;
	      QT_SMC_C16(i_ptr,dptr,xaUBYTE);
	    } else if (x11_bytes_pixel==1)
	    { xaUBYTE *i_ptr = (xaUBYTE *)(image + y * imagex + x);
	      QT_SMC_C16m(i_ptr,dptr,xaUBYTE,map,row_inc); i_ptr += row_inc;
	      QT_SMC_C16m(i_ptr,dptr,xaUBYTE,map,row_inc);
	    } else if (x11_bytes_pixel==2)
	    { xaUSHORT *i_ptr = (xaUSHORT *)(image + 2*(y * imagex + x));
	      QT_SMC_C16m(i_ptr,dptr,xaUSHORT,map,row_inc); i_ptr += row_inc;
	      QT_SMC_C16m(i_ptr,dptr,xaUSHORT,map,row_inc);
	    } else
	    { xaULONG *i_ptr = (xaULONG *)(image + 4*(y * imagex + x));
	      QT_SMC_C16m(i_ptr,dptr,xaULONG,map,row_inc); i_ptr += row_inc;
	      QT_SMC_C16m(i_ptr,dptr,xaULONG,map,row_inc);
	    }
	    len -= 16; x += 4; if (x >= imagex) { x = 0; y += 4; }
	  }
	}
	break;

      default:   /* 0xF0 */
	fprintf(stderr,"SMC opcode %x is unknown\n",code);
	break;
    }
  }
  if (map_flag) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}

xaULONG QT_Decode_YUV2(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC2_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG special = dec_info->special & 0x01;
  XA_CHDR *chdr = dec_info->chdr;
  xaUBYTE *dptr = delta;
  xaULONG ycnt, row_inc = imagex << 1;
  xaULONG mx = imagex >> 1;
  void (*color_func)() = (void (*)())XA_YUV211111_Func(dec_info->image_type);


  dec_info->xs = dec_info->ys = 0;
  dec_info->xe = imagex; dec_info->ye = imagey;
  if (dec_info->skip_flag > 0) return(ACT_DLTA_DROP);
  if (chdr) { if (chdr->new_chdr) chdr = chdr->new_chdr; }

  row_inc *= (special)?(3):(dec_info->bytes_pixel);

  ycnt = 0;
  while( (imagey > 0) && (dsize > 0) )
  { xaUBYTE *yp,*up,*vp;
    xaULONG x = mx;
    if (ycnt == 0)
      { yp = jpg_YUVBufs.Ybuf; up = jpg_YUVBufs.Ubuf; vp = jpg_YUVBufs.Vbuf; }
    while(x--)
    { *yp++ = *dptr++; *up++ = (*dptr++) ^ 0x80;
      *yp++ = *dptr++; *vp++ = (*dptr++) ^ 0x80;  dsize -= 4;
    }
    ycnt++;
    imagey--;
    if ((ycnt >= 2) || (imagey == 0))
    { color_func(image,imagex,ycnt,imagex,ycnt,
                &jpg_YUVBufs, &def_yuv_tabs, map_flag, map, chdr);
      ycnt = 0;
      image += row_inc;
    }
  } /*end of y */

  if (map_flag) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}

/*********
 *
 *******/

xaULONG QT_RPZA_Dither(image,delta,len,dec_info)
xaUBYTE *image;		/* Image Buffer. */
xaUBYTE *delta;		/* delta data. */
xaULONG len;		/* delta size */
XA_DEC_INFO *dec_info;	/* Decoder Info Header */
{ 
  xaULONG imagex = dec_info->imagex;
  xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  XA_CHDR *chdr = dec_info->chdr;
  xaLONG x,y,row_inc,blk_inc; 
  xaLONG min_x,max_x,min_y,max_y;
  xaUBYTE *dptr;
  xaULONG code,changed;
  xaUBYTE *im0,*im1,*im2,*im3;
  xaUBYTE *rnglimit = xa_byte_limit;
  ColorReg *cmap;

  if (chdr) { if (chdr->new_chdr) chdr = chdr->new_chdr; }
  cmap = chdr->cmap;
  dec_info->xs = dec_info->ys = 0; dec_info->xe = imagex; dec_info->ye = imagey;
  max_x = max_y = 0; min_x = imagex; min_y = imagey; changed = 0;
  dptr = delta;
  x = y = 0;

  blk_inc = 1; /* possible optimization */
  row_inc = blk_inc * imagex;
  blk_inc *= 4;
  im1 = im0 = image;    im1 += row_inc;
  im2 = im1;            im2 += row_inc;
  im3 = im2;            im3 += row_inc;
  row_inc *= 3; /* skip 3 rows at a time */

  while(len > 0) 
  { code = *dptr++; len--; 
    if ( (code >= 0xa0) && (code <= 0xbf) )		/* SINGLE */
    { xaULONG color,skip;
      xaLONG re,ge,be,r,g,b,col;	

      changed = 1;
      color = (*dptr++) << 8; color |= *dptr++; len -= 2;
      skip = (code - 0x9f);

      while(skip--)
      { xaUBYTE *ip0=im0; xaUBYTE *ip1=im1;
	xaUBYTE *ip2=im2; xaUBYTE *ip3=im3;
	  /* get original r,g,b  and scale to 8 bits */
	RPZA_DITH_COL2RGB(r,g,b,color);

      /**---0 0*/
	  RPZA_DITH_GET_RGB(r,g,b,0,0,0,col);
	  if (map_flag) col = map[col];			ip0[0] = (xaUBYTE)col;
	  RPZA_DITH_GET_ERR(r,g,b,re,ge,be,col,cmap);
      /*----0 1*/
	  RPZA_DITH_GET_RGB(r,g,b,re,ge,be,col);
	  if (map_flag) col = map[col];			ip0[1] = (xaUBYTE)col;
	  RPZA_DITH_GET_ERR(r,g,b,re,ge,be,col,cmap);
      /*----1 1*/
	  RPZA_DITH_GET_RGB(r,g,b,re,ge,be,col);
	  if (map_flag) col = map[col];			ip1[1] = (xaUBYTE)col;
	  RPZA_DITH_GET_ERR(r,g,b,re,ge,be,col,cmap);
      /*----1 0*/
	  RPZA_DITH_GET_RGB(r,g,b,re,ge,be,col);
	  if (map_flag) col = map[col];			ip1[0] = (xaUBYTE)col;
	  RPZA_DITH_GET_ERR(r,g,b,re,ge,be,col,cmap);
      /**---2 0*/
	  RPZA_DITH_GET_RGB(r,g,b,re,ge,be,col);
	  if (map_flag) col = map[col];			ip2[0] = (xaUBYTE)col;
	  RPZA_DITH_GET_ERR(r,g,b,re,ge,be,col,cmap);
      /*----3 0*/
	  RPZA_DITH_GET_RGB(r,g,b,re,ge,be,col);
	  if (map_flag) col = map[col];			ip3[0] = (xaUBYTE)col;
	  RPZA_DITH_GET_ERR(r,g,b,re,ge,be,col,cmap);
      /*----3 1*/
	  RPZA_DITH_GET_RGB(r,g,b,re,ge,be,col);
	  if (map_flag) col = map[col];			ip3[1] = (xaUBYTE)col;
	  RPZA_DITH_GET_ERR(r,g,b,re,ge,be,col,cmap);
      /*----2 1*/
	  RPZA_DITH_GET_RGB(r,g,b,re,ge,be,col);
	  if (map_flag) col = map[col];			ip2[1] = (xaUBYTE)col;
	  RPZA_DITH_GET_ERR(r,g,b,re,ge,be,col,cmap);
      /**---2 2*/
	  RPZA_DITH_GET_RGB(r,g,b,re,ge,be,col);
	  if (map_flag) col = map[col];			ip2[2] = (xaUBYTE)col;
	  RPZA_DITH_GET_ERR(r,g,b,re,ge,be,col,cmap);
      /*----3 2*/
	  RPZA_DITH_GET_RGB(r,g,b,re,ge,be,col);
	  if (map_flag) col = map[col];			ip3[2] = (xaUBYTE)col;
	  RPZA_DITH_GET_ERR(r,g,b,re,ge,be,col,cmap);
      /*----3 3*/
	  RPZA_DITH_GET_RGB(r,g,b,re,ge,be,col);
	  if (map_flag) col = map[col];			ip3[3] = (xaUBYTE)col;
	  RPZA_DITH_GET_ERR(r,g,b,re,ge,be,col,cmap);
      /*----2 3*/
	  RPZA_DITH_GET_RGB(r,g,b,re,ge,be,col);
	  if (map_flag) col = map[col];			ip2[3] = (xaUBYTE)col;
	  RPZA_DITH_GET_ERR(r,g,b,re,ge,be,col,cmap);
      /**---1 3*/
	  RPZA_DITH_GET_RGB(r,g,b,re,ge,be,col);
	  if (map_flag) col = map[col];			ip1[3] = (xaUBYTE)col;
	  RPZA_DITH_GET_ERR(r,g,b,re,ge,be,col,cmap);
      /*----1 2*/
	  RPZA_DITH_GET_RGB(r,g,b,re,ge,be,col);
	  if (map_flag) col = map[col];			ip1[2] = (xaUBYTE)col;
	  RPZA_DITH_GET_ERR(r,g,b,re,ge,be,col,cmap);
      /*----0 2*/
	  RPZA_DITH_GET_RGB(r,g,b,re,ge,be,col);
	  if (map_flag) col = map[col];			ip0[2] = (xaUBYTE)col;
	  RPZA_DITH_GET_ERR(r,g,b,re,ge,be,col,cmap);
      /*----0 3*/
	  RPZA_DITH_GET_RGB(r,g,b,re,ge,be,col);
	  if (map_flag) col = map[col];			ip0[3] = (xaUBYTE)col;
	  
	  QT_MIN_MAX_CHECK(x,y,min_x,min_y,max_x,max_y);
	  QT_RPZA_BLOCK_INC(x,y,imagex,im0,im1,im2,im3,blk_inc,row_inc);
        } /* end while */
    }
    else if ( (code >= 0x80) && (code <= 0x9f) )		/* SKIP */
    { xaULONG skip = (code - 0x7f);
      while(skip--) 
		QT_RPZA_BLOCK_INC(x,y,imagex,im0,im1,im2,im3,blk_inc,row_inc);
    }

 				/* FOUR/SIXTEEN */ 
    else if ( (code < 0x80) || ((code >= 0xc0) && (code <= 0xdf)) )
    { xaULONG cA,cB;
      changed = 1;
      /* Get 1st two colors */
      if (code >= 0xc0) { cA = (*dptr++) << 8; cA |= *dptr++; len -= 2; }
      else {cA = (code << 8) | *dptr++; len -= 1;}
      cB = (*dptr++) << 8; cB |= *dptr++; len -= 2;

      /****** SIXTEEN COLOR *******/
      if ( (code < 0x80) && ((cB & 0x8000)==0) ) /* 16 color */
      { xaUBYTE *cptr = dptr;
        xaUBYTE *ip0=im0; xaUBYTE *ip1=im1;
	xaUBYTE *ip2=im2; xaUBYTE *ip3=im3;
        xaLONG re,ge,be,ra,ga,ba,col;	
	dptr += 28; len -= 28;  /* skip over colors */

	/**---0 0*/
	 RPZA_DITH_COL2RGB(ra,ga,ba,cA);
	 RPZA_DITH_GET_RGB(ra,ga,ba,0,0,0,col);
	 if (map_flag) col = map[col];		ip0[0] = (xaUBYTE)col;
	 RPZA_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
	/*----0 1*/
	 RPZA_DITH_COL2RGB(ra,ga,ba,cB);
	 RPZA_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	 if (map_flag) col = map[col];		ip0[1] = (xaUBYTE)col;
	 RPZA_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
	/*----1 1*/
	 cA = (cptr[ 6]) << 8; cA |= cptr[ 7]; RPZA_DITH_COL2RGB(ra,ga,ba,cA);
	 RPZA_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	 if (map_flag) col = map[col];		ip1[1] = (xaUBYTE)col;
	 RPZA_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
	/*----1 0*/
	 cA = (cptr[ 4]) << 8; cA |= cptr[ 5]; RPZA_DITH_COL2RGB(ra,ga,ba,cA);
	 RPZA_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	 if (map_flag) col = map[col];		ip1[0] = (xaUBYTE)col;
	 RPZA_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
	/**---2 0*/
	 cA = (cptr[12]) << 8; cA |= cptr[13]; RPZA_DITH_COL2RGB(ra,ga,ba,cA);
	 RPZA_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	 if (map_flag) col = map[col];		ip2[0] = (xaUBYTE)col;
	 RPZA_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
	/*----3 0*/
	 cA = (cptr[20]) << 8; cA |= cptr[21]; RPZA_DITH_COL2RGB(ra,ga,ba,cA);
	 RPZA_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	 if (map_flag) col = map[col];		ip3[0] = (xaUBYTE)col;
	 RPZA_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
	/*----3 1*/
	 cA = (cptr[22]) << 8; cA |= cptr[23]; RPZA_DITH_COL2RGB(ra,ga,ba,cA);
	 RPZA_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	 if (map_flag) col = map[col];		ip3[1] = (xaUBYTE)col;
	 RPZA_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
	/*----2 1*/
	 cA = (cptr[14]) << 8; cA |= cptr[15]; RPZA_DITH_COL2RGB(ra,ga,ba,cA);
	 RPZA_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	 if (map_flag) col = map[col];		ip2[1] = (xaUBYTE)col;
	 RPZA_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
	/**---2 2*/
	 cA = (cptr[16]) << 8; cA |= cptr[17]; RPZA_DITH_COL2RGB(ra,ga,ba,cA);
	 RPZA_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	 if (map_flag) col = map[col];		ip2[2] = (xaUBYTE)col;
	 RPZA_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
	/*----3 2*/
	 cA = (cptr[24]) << 8; cA |= cptr[25]; RPZA_DITH_COL2RGB(ra,ga,ba,cA);
	 RPZA_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	 if (map_flag) col = map[col];		ip3[2] = (xaUBYTE)col;
	 RPZA_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
	/*----3 3*/
	 cA = (cptr[26]) << 8; cA |= cptr[27]; RPZA_DITH_COL2RGB(ra,ga,ba,cA);
	 RPZA_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	 if (map_flag) col = map[col];		ip3[3] = (xaUBYTE)col;
	 RPZA_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
	/*----2 3*/
	 cA = (cptr[18]) << 8; cA |= cptr[19]; RPZA_DITH_COL2RGB(ra,ga,ba,cA);
	 RPZA_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	 if (map_flag) col = map[col];		ip2[3] = (xaUBYTE)col;
	 RPZA_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
	/**---1 3*/
	 cA = (cptr[10]) << 8; cA |= cptr[11]; RPZA_DITH_COL2RGB(ra,ga,ba,cA);
	 RPZA_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	 if (map_flag) col = map[col];		ip1[3] = (xaUBYTE)col;
	 RPZA_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
	/*----1 2*/
	 cA = (cptr[ 8]) << 8; cA |= cptr[ 9]; RPZA_DITH_COL2RGB(ra,ga,ba,cA);
	 RPZA_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	 if (map_flag) col = map[col];		ip1[2] = (xaUBYTE)col;
	 RPZA_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
	/*----0 2*/
	 cA = (cptr[ 0]) << 8; cA |= cptr[ 1]; RPZA_DITH_COL2RGB(ra,ga,ba,cA);
	 RPZA_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	 if (map_flag) col = map[col];		ip0[2] = (xaUBYTE)col;
	 RPZA_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
	/*----0 3*/
	 cA = (cptr[ 2]) << 8; cA |= cptr[ 3]; RPZA_DITH_COL2RGB(ra,ga,ba,cA);
	 RPZA_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	 if (map_flag) col = map[col];		ip0[3] = (xaUBYTE)col;

	QT_MIN_MAX_CHECK(x,y,min_x,min_y,max_x,max_y);
	QT_RPZA_BLOCK_INC(x,y,imagex,im0,im1,im2,im3,blk_inc,row_inc);
      } /*** END of SIXTEEN COLOR */
      else					/****** FOUR COLOR *******/
      { xaULONG m_cnt,msk0,msk1,msk2,msk3;
	xaLONG r[4],g[4],b[4];

        if (code < 0x80) m_cnt = 1; 
	else m_cnt = code - 0xbf; 

	{ xaULONG rA,gA,bA, rB,gB,bB;
	  RPZA_DITH_COL2RGB(rA,gA,bA,cA);  r[3] = rA; g[3] = gA; b[3] = bA;
	  RPZA_DITH_COL2RGB(rB,gB,bB,cB);  r[0] = rB; g[0] = gB; b[0] = bB;
          r[2] = (21*rA + 11*rB) >> 5;	   r[1] = (11*rA + 21*rB) >> 5; 
	  g[2] = (21*gA + 11*gB) >> 5;	   g[1] = (11*gA + 21*gB) >> 5;
	  b[2] = (21*bA + 11*bB) >> 5;	   b[1] = (11*bA + 21*bB) >> 5;
	}


        while(m_cnt--)
        { msk0 = *dptr++; msk1 = *dptr++;
	  msk2 = *dptr++; msk3 = *dptr++; len -= 4;

          { xaUBYTE *ip0=im0; xaUBYTE *ip1=im1;
	    xaUBYTE *ip2=im2; xaUBYTE *ip3=im3;
            xaLONG idx,re,ge,be,ra,ga,ba,col;	
      /**---0 0*/
	    idx = (msk0>>6)&0x03; ra = r[idx]; ga = g[idx]; ba = b[idx];
	    RPZA_DITH_GET_RGB(ra,ga,ba,0,0,0,col);
	    if (map_flag) col = map[col];		ip0[0] = (xaUBYTE)col;
	    RPZA_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
      /*----0 1*/
	    idx = (msk0>>4)&0x03; ra = r[idx]; ga = g[idx]; ba = b[idx];
	    RPZA_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	    if (map_flag) col = map[col];		ip0[1] = (xaUBYTE)col;
	    RPZA_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
      /*----1 1*/
	    idx = (msk1>>4)&0x03; ra = r[idx]; ga = g[idx]; ba = b[idx];
	    RPZA_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	    if (map_flag) col = map[col];		ip1[1] = (xaUBYTE)col;
	    RPZA_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
      /*----1 0*/
	    idx = (msk1>>6)&0x03; ra = r[idx]; ga = g[idx]; ba = b[idx];
	    RPZA_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	    if (map_flag) col = map[col];		ip1[0] = (xaUBYTE)col;
	    RPZA_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
      /**---2 0*/
	    idx = (msk2>>6)&0x03; ra = r[idx]; ga = g[idx]; ba = b[idx];
	    RPZA_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	    if (map_flag) col = map[col];		ip2[0] = (xaUBYTE)col;
	    RPZA_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
      /*----3 0*/
	    idx = (msk3>>6)&0x03; ra = r[idx]; ga = g[idx]; ba = b[idx];
	    RPZA_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	    if (map_flag) col = map[col];		ip3[0] = (xaUBYTE)col;
	    RPZA_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
      /*----3 1*/
	    idx = (msk3>>4)&0x03; ra = r[idx]; ga = g[idx]; ba = b[idx];
	    RPZA_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	    if (map_flag) col = map[col];		ip3[1] = (xaUBYTE)col;
	    RPZA_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
      /*----2 1*/
	    idx = (msk2>>4)&0x03; ra = r[idx]; ga = g[idx]; ba = b[idx];
	    RPZA_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	    if (map_flag) col = map[col];		ip2[1] = (xaUBYTE)col;
	    RPZA_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
      /**---2 2*/
	    idx = (msk2>>2)&0x03; ra = r[idx]; ga = g[idx]; ba = b[idx];
	    RPZA_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	    if (map_flag) col = map[col];		ip2[2] = (xaUBYTE)col;
	    RPZA_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
      /*----3 2*/
	    idx = (msk3>>2)&0x03; ra = r[idx]; ga = g[idx]; ba = b[idx];
	    RPZA_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	    if (map_flag) col = map[col];		ip3[2] = (xaUBYTE)col;
	    RPZA_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
      /*----3 3*/
	    idx = msk3 & 0x03; ra = r[idx]; ga = g[idx]; ba = b[idx];
	    RPZA_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	    if (map_flag) col = map[col];		ip3[3] = (xaUBYTE)col;
	    RPZA_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
      /*----2 3*/
	    idx = msk2 & 0x03; ra = r[idx]; ga = g[idx]; ba = b[idx];
	    RPZA_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	    if (map_flag) col = map[col];		ip2[3] = (xaUBYTE)col;
	    RPZA_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
      /**---1 3*/
	    idx = msk1 & 0x03; ra = r[idx]; ga = g[idx]; ba = b[idx];
	    RPZA_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	    if (map_flag) col = map[col];		ip1[3] = (xaUBYTE)col;
	    RPZA_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
      /*----1 2*/
	    idx = (msk1>>2)&0x03; ra = r[idx]; ga = g[idx]; ba = b[idx];
	    RPZA_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	    if (map_flag) col = map[col];		ip1[2] = (xaUBYTE)col;
	    RPZA_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
      /*----0 2*/
	    idx = (msk0>>2)&0x03; ra = r[idx]; ga = g[idx]; ba = b[idx];
	    RPZA_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	    if (map_flag) col = map[col];		ip0[2] = (xaUBYTE)col;
	    RPZA_DITH_GET_ERR(ra,ga,ba,re,ge,be,col,cmap);
      /*----0 3*/
	    idx = msk0 & 0x03; ra = r[idx]; ga = g[idx]; ba = b[idx];
	    RPZA_DITH_GET_RGB(ra,ga,ba,re,ge,be,col);
	    if (map_flag) col = map[col];		ip0[3] = (xaUBYTE)col;
	  }
	  QT_MIN_MAX_CHECK(x,y,min_x,min_y,max_x,max_y);
	  QT_RPZA_BLOCK_INC(x,y,imagex,im0,im1,im2,im3,blk_inc,row_inc);
        }  
      } /*** END of FOUR COLOR *******/
    } /*** END of 4/16 COLOR BLOCKS ****/
    else /* UNKNOWN */
    {
      fprintf(stderr,"QT RPZA: Unknown %x\n",code);
      return(ACT_DLTA_NOP);
    }
  }
  if (xa_optimize_flag == xaTRUE)
  { if (changed) { dec_info->xs=min_x; dec_info->ys=min_y;
		   dec_info->xe=max_x + 4; dec_info->ye=max_y + 4; }
    else  { dec_info->xs = dec_info->ys = dec_info->xe = dec_info->ye = 0;
	    return(ACT_DLTA_NOP); }
  }
  else { dec_info->xs = dec_info->ys = 0; 
	 dec_info->xe = imagex; dec_info->ye = imagey; }
  if (map_flag) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}


/****************** 8BPS CODEC DEPTH 24 ********************************
 *  R G B
 *********************************************************************/
xaULONG QT_Decode_8BPS24(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG special = dec_info->special;
  xaULONG extra = (xaULONG)(dec_info->extra);
  XA_CHDR *chdr = dec_info->chdr;
  xaULONG special_flag = special & 0x0001;
  xaLONG len, y;
  xaUBYTE *rp, *gp, *bp;
  xaUBYTE *r_lp, *g_lp, *b_lp, *p_lp;
  
  dec_info->xs = dec_info->ys = 0;
  dec_info->xe = imagex; dec_info->ye = imagey;
    /* Indicate we can drop these frames */
  if (dec_info->skip_flag > 0) return(ACT_DLTA_DROP);

  if (chdr) { if (chdr->new_chdr) chdr = chdr->new_chdr; }

	/* Line Pointers for each Plane */
  r_lp = delta;
  g_lp = r_lp + (imagey << 1);
  b_lp = g_lp + (imagey << 1);
  p_lp = b_lp + (imagey << 1); /* pad/alpha?? */

	/* Start of Red Rows */
  if (extra == 0x01)	rp = p_lp + (imagey << 1);
  else			rp = b_lp + (imagey << 1);
	/* Calculate Size of Compressed Red Plane */
  gp = r_lp; len = 0;
  for(y = 0; y < imagey; y++) { len += ((gp[0] << 8) | gp[1]); gp += 2; }
	/* Start of Green Rows */
  gp = rp + len;
	/* Calculate Size of Compressed Green Plane */
  bp = g_lp; len = 0;
  for(y = 0; y < imagey; y++) { len += ((bp[0] << 8) | bp[1]); bp += 2; }
	/* Start of Blue Rows */
  bp = gp + len;

  for(y=0; y<imagey; y++)
  { xaUBYTE *yp, *vp, *up;
    xaLONG x;

	/* RED PLANE */
    yp = jpg_YUVBufs.Ybuf; 
    len = r_lp[ (y*2) ] << 8;  len |= r_lp[ (y*2) + 1 ];
    x = 0;
    while((x < imagex) && (len > 0))
    { xaUBYTE code = *rp++; len--; /* code*/
      if (code <= 127) { code++; len -= code; x += code;
				while(code--) { *yp++ = *rp++; } }
      else if (code > 128) { code = (xaULONG)(257) - code; x += code; 
				while(code--) { *yp++ = *rp; } rp++; len--; }
    }
	/* Green PLANE */
    up = jpg_YUVBufs.Ubuf; 
    len = g_lp[ (y*2) ] << 8;  len |= g_lp[ (y*2) + 1 ];
    x = 0;
    while((x < imagex) && (len > 0))
    { xaUBYTE code = *gp++; len--; /* code*/
      if (code <= 127) { code++; len -= code; x += code;
				while(code--) { *up++ = *gp++; } }
      else if (code > 128) { code = (xaULONG)(257) - code; x += code; 
				while(code--) { *up++ = *gp; } gp++; len--; }
    }
	/* Blue PLANE */
    vp = jpg_YUVBufs.Vbuf; 
    len = b_lp[ (y*2) ] << 8;  len |= b_lp[ (y*2) + 1 ];
    x = 0;
    while((x < imagex) && (len > 0))
    { xaUBYTE code = *bp++; len--; /* code*/
      if (code <= 127) { code++; len -= code; x += code;
				while(code--) { *vp++ = *bp++; } }
      else if (code > 128) { code = (xaULONG)(257) - code; x += code; 
				while(code--) { *vp++ = *bp; } bp++; len--; }
    }

    yp = jpg_YUVBufs.Ybuf; up = jpg_YUVBufs.Ubuf; vp = jpg_YUVBufs.Vbuf;
    x = imagex;
    if (special_flag)
    { xaUBYTE *ip = (xaUBYTE *)(image + (y * imagex * 3)); 
      while(x--) { *ip++ = *yp++; *ip++ = *up++; *ip++ = *vp++; }
    }
    else if ( (map_flag==xaFALSE) || (x11_bytes_pixel==1) )
    { xaUBYTE *ip = (xaUBYTE *)(image + y * imagex); 
      while(x--) *ip++ = 
	(xaUBYTE)XA_RGB24_To_CLR32(*yp++,*up++,*vp++,map_flag,map,chdr);
    }
    else if (x11_bytes_pixel==4)
    { xaULONG *ip = (xaULONG *)(image + ((y * imagex) << 2));
      while(x--) *ip++ = 
	(xaULONG)XA_RGB24_To_CLR32(*yp++,*up++,*vp++,map_flag,map,chdr);
    }
    else /* (x11_bytes_pixel==2) */
    { xaUSHORT *ip = (xaUSHORT *)(image + ((y * imagex) << 1));
      while(x--) *ip++ = 
	(xaUSHORT)XA_RGB24_To_CLR32(*yp++,*up++,*vp++,map_flag,map,chdr);
    }
  } /* end of y */

  if (map_flag==xaTRUE) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}


/********************* * * * *******************************************/
xaULONG QT_Decode_8BPS8(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaLONG len, y;
  xaUBYTE *rp, *lp;

  dec_info->xs = dec_info->ys = 0;
  dec_info->xe = imagex; dec_info->ye = imagey;
    /* Indicate we can drop these frames */
  if (dec_info->skip_flag > 0) return(ACT_DLTA_DROP);

	/* Line Pointers for each Plane */
  lp = delta;

	/* Start of Rows */
  rp = lp + (imagey << 1);

	/* Loop Through Each Line */
  for(y=0; y<imagey; y++)
  { xaUBYTE *yp;
    xaLONG x;

	/* Decode Row */
    yp = jpg_YUVBufs.Ybuf; 
    len = lp[ (y*2) ] << 8;  len |= lp[ (y*2) + 1 ];
    x = 0;
    while((x < imagex) && (len > 0))
    { xaUBYTE code = *rp++; len--; /* code*/
      if (code <= 127) { code++; len -= code; x += code;
				while(code--) { *yp++ = *rp++; } }
      else if (code > 128) { code = (xaULONG)(257) - code; x += code; 
				while(code--) { *yp++ = *rp; } rp++; len--; }
    }

    yp = jpg_YUVBufs.Ybuf;
    x = imagex;
    if (map_flag==xaFALSE)
    { xaUBYTE *ip = (xaUBYTE *)(image + y * imagex); 
	while(x--) { *ip++ = *yp++; }
    }
    else if (x11_bytes_pixel==1)
    { xaUBYTE *ip = (xaUBYTE *)(image + y * imagex); 
	while(x--) *ip++ = (xaUBYTE)map[*yp++];
    }
    else if (x11_bytes_pixel==4)
    { xaULONG *ip = (xaULONG *)(image + ((y * imagex) << 2));
	while(x--) *ip++ = (xaULONG)map[*yp++];
    }
    else /* (x11_bytes_pixel==2) */
    { xaUSHORT *ip = (xaUSHORT *)(image + ((y * imagex) << 1));
	while(x--) *ip++ = (xaUSHORT)map[*yp++];
    }
  } /* end of y */

  if (map_flag==xaTRUE) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}

