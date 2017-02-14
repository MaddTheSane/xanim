
/*
 * xa_color.c
 *
 * Copyright (C) 1995,1995 by Mark Podlipec.
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

/* Revision History
 *
 * 22Aug95 dithering routines for MPEG/JPG
 */

#include "xanim.h"
#include "xa_color.h"

YUVTabs def_yuv_tabs;
void XA_Gen_YUV_Tabs();
void XA_Free_YUV_Tabs();
extern void XA_Add_Func_To_Free_Chain();

xaLONG *YUV_Y_tab;
xaLONG *YUV_UB_tab;
xaLONG *YUV_VR_tab;
xaLONG *YUV_UG_tab;
xaLONG *YUV_VG_tab;

void XA_YUV111_To_Gray();
void XA_YUV111_To_RGB();
void XA_YUV111_To_CLR8();
void XA_YUV111_To_CLR16();
void XA_YUV111_To_CLR32();
void XA_YUV111_To_332_Dither();
void XA_YUV111_To_332();
void XA_YUV211_To_RGB();
void XA_YUV211_To_CLR8();
void XA_YUV211_To_CLR16();
void XA_YUV211_To_CLR32();
void XA_YUV211_To_332_Dither();
void XA_YUV211_To_332();
void XA_YUV411_To_RGB();
void XA_YUV411_To_CLR8();
void XA_YUV411_To_CLR16();
void XA_YUV411_To_CLR32();
void XA_YUV411_To_CF4();
void XA_YUV411_To_332_Dither();
void XA_YUV411_To_332();

void XA_2x2_OUT_1BLK_rgb();
void XA_2x2_OUT_1BLK_clr8();
void XA_2x2_OUT_1BLK_clr16();
void XA_2x2_OUT_1BLK_clr32();
void XA_2x2_OUT_1BLK_dith8();
void XA_2x2_OUT_1BLK_dith16();
void XA_2x2_OUT_1BLK_dith32();
void XA_2x2_OUT_4BLKS_rgb();
void XA_2x2_OUT_4BLKS_clr8();
void XA_2x2_OUT_4BLKS_clr16();
void XA_2x2_OUT_4BLKS_clr32();
void XA_2x2_OUT_4BLKS_dith8();
void XA_2x2_OUT_4BLKS_dith16();
void XA_2x2_OUT_4BLKS_dith32();


extern xaUBYTE  *xa_byte_limit;

/*
 *      R = Y               + 1.40200 * V
 *      G = Y - 0.34414 * U - 0.71414 * V
 *      B = Y + 1.77200 * U
 */
void XA_Gen_YUV_Tabs(anim_hdr)
XA_ANIM_HDR *anim_hdr;
{ xaLONG i;
  float t_ub,t_vr,t_ug,t_vg;

  XA_Add_Func_To_Free_Chain(anim_hdr,XA_Free_YUV_Tabs);
  if (YUV_Y_tab==0)
  {
    YUV_Y_tab =  (xaLONG *)malloc( 256 * sizeof(xaLONG) );
    YUV_UB_tab = (xaLONG *)malloc( 256 * sizeof(xaLONG) );
    YUV_VR_tab = (xaLONG *)malloc( 256 * sizeof(xaLONG) );
    YUV_UG_tab = (xaLONG *)malloc( 256 * sizeof(xaLONG) );
    YUV_VG_tab = (xaLONG *)malloc( 256 * sizeof(xaLONG) );
    if (  (YUV_UB_tab==0) || (YUV_VR_tab==0)
        ||(YUV_UG_tab==0)||(YUV_VG_tab==0) ) 
			TheEnd1("YUV_GEN: yuv tab malloc err");
  }

  t_ub = (1.77200/2.0) * (float)(1<<6) + 0.5;
  t_vr = (1.40200/2.0) * (float)(1<<6) + 0.5;
  t_ug = (0.34414/2.0) * (float)(1<<6) + 0.5;
  t_vg = (0.71414/2.0) * (float)(1<<6) + 0.5;
  for(i=0;i<256;i++)
  {
    float x = (float)(2 * i - 255);
    
    YUV_UB_tab[i] = (xaLONG)( ( t_ub * x) + (1<<5));
    YUV_VR_tab[i] = (xaLONG)( ( t_vr * x) + (1<<5));
    YUV_UG_tab[i] = (xaLONG)( (-t_ug * x)         );
    YUV_VG_tab[i] = (xaLONG)( (-t_vg * x) + (1<<5));
    YUV_Y_tab[i]  = (xaLONG)( (i << 6) | (i >> 2) );
  }
  def_yuv_tabs.Uskip_mask = 0;
  def_yuv_tabs.YUV_Y_tab  = YUV_Y_tab;
  def_yuv_tabs.YUV_UB_tab = YUV_UB_tab;
  def_yuv_tabs.YUV_VR_tab = YUV_VR_tab;
  def_yuv_tabs.YUV_UG_tab = YUV_UG_tab;
  def_yuv_tabs.YUV_VG_tab = YUV_VG_tab;

}

void XA_Free_YUV_Tabs()
{
  if (YUV_Y_tab) { free(YUV_Y_tab); YUV_Y_tab = 0; }
  if (YUV_UB_tab) { free(YUV_UB_tab); YUV_UB_tab = 0; }
  if (YUV_VR_tab) { free(YUV_VR_tab); YUV_VR_tab = 0; }
  if (YUV_UG_tab) { free(YUV_UG_tab); YUV_UG_tab = 0; }
  if (YUV_VG_tab) { free(YUV_VG_tab); YUV_VG_tab = 0; }
  def_yuv_tabs.YUV_Y_tab = 0;
  def_yuv_tabs.YUV_UB_tab = def_yuv_tabs.YUV_VR_tab = 0;
  def_yuv_tabs.YUV_UG_tab = def_yuv_tabs.YUV_VG_tab = 0;
}


/****************************
 *
 ***************/
void yuv_to_rgb(y,u,v,ir,ig,ib)
xaULONG y,u,v,*ir,*ig,*ib;
{ register xaLONG r,g,b,YY = (xaLONG)(YUV_Y_tab[(y)]);
  r = (YY + YUV_VR_tab[v]) >> 6;
  g = (YY + YUV_UG_tab[u] + YUV_VG_tab[v]) >> 6;
  b = (YY + YUV_UB_tab[u]) >> 6;
/*POD replace with range limit table */
  if (r<0) r = 0; if (g<0) g = 0; if (b<0) b = 0;
  if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
  *ir = (xaULONG)r; *ig = (xaULONG)g; *ib = (xaULONG)b;
}


/****************************
 *
 ***************/
xaULONG XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr)
register xaULONG r,g,b;
xaULONG map_flag,*map;
XA_CHDR *chdr;
{ xaULONG clr;

  if (x11_display_type & XA_X11_TRUE) 
	clr = X11_Get_True_Color( xa_gamma_adj[r], 
				  xa_gamma_adj[g], xa_gamma_adj[b], 16);
  else
  {
    if ((cmap_color_func == 4) && (chdr))
    { register xaULONG cache_i;
      if (cmap_cache == 0) CMAP_Cache_Init(0);
      if (chdr != cmap_cache_chdr)
      {
        CMAP_Cache_Clear();
        cmap_cache_chdr = chdr;
      }
      cache_i  = ((r>>3)<<10) | ((g>>3)<<5) | (b>>3);
      if ( (clr = (xaULONG)cmap_cache[cache_i]) == 0xffff)
      { r = xa_gamma_adj[r]; g = xa_gamma_adj[g]; b = xa_gamma_adj[b];
        clr = chdr->coff +
           CMAP_Find_Closest(chdr->cmap,chdr->csize,r,g,b,16,16,16,xaTRUE);
        cmap_cache[cache_i] = (xaUSHORT)clr;
      }
    }
    else
    { if (cmap_true_to_332 == xaTRUE) clr = CMAP_GET_332(r,g,b,CMAP_SCALE8);
      else                        clr = CMAP_GET_GRAY(r,g,b,CMAP_SCALE13);
      if (map_flag) clr = map[clr];
    }
  }
  return(clr);
}

#define RGB888_TO_COLOR(r,g,b,map_flag,map,chdr,ip,CAST) \
{ register xaULONG clr; \
  if (x11_display_type & XA_X11_TRUE) clr = X11_Get_True_Color( \
		xa_gamma_adj[r],xa_gamma_adj[g],xa_gamma_adj[b], 16); \
  else if ((cmap_color_func == 4) && (chdr)) \
  { register xaULONG cache_i = ((r>>3)<<10) | ((g>>3)<<5) | (b>>3); \
    if (cmap_cache[cache_i] == 0xffff) \
    { r = xa_gamma_adj[r]; g = xa_gamma_adj[g]; b = xa_gamma_adj[b]; \
      clr = chdr->coff + CMAP_Find_Closest(chdr->cmap,chdr->csize, \
						r,g,b,16,16,16,xaTRUE); \
      cmap_cache[cache_i] = (xaUSHORT)clr; \
    } else clr = (xaULONG)cmap_cache[cache_i]; \
  } else \
  { if (cmap_true_to_332 == xaTRUE) clr = CMAP_GET_332(r,g,b,CMAP_SCALE8); \
      else                        clr = CMAP_GET_GRAY(r,g,b,CMAP_SCALE13); \
      if (map_flag) clr = map[clr]; \
  } \
  *ip++ = (CAST)(clr); \
}

#define YUV_TO_COLOR(y,cr,cg,cb,map_flag,map,chdr,ip,CAST) \
{ xaLONG r,g,b,YY = (xaLONG)(y);		\
  r = (xaULONG)rnglimit[ (YY + cr) >> 6 ];	\
  g = (xaULONG)rnglimit[ (YY + cg) >> 6 ];	\
  b = (xaULONG)rnglimit[ (YY + cb) >> 6 ];	\
  RGB888_TO_COLOR(r,g,b,map_flag,map,chdr,ip,CAST);	\
}

#define YUV_TO_CF4(y,cr,cg,cb,chdr,ip) \
{ xaLONG r,g,b,YY = (xaLONG)(y);                \
  r = (xaULONG)rnglimit[ (YY + cr) >> 6 ];      \
  g = (xaULONG)rnglimit[ (YY + cg) >> 6 ];      \
  b = (xaULONG)rnglimit[ (YY + cb) >> 6 ];      \
  { register xaULONG clr,cache_i = ((r>>3)<<10) | ((g>>3)<<5) | (b>>3);	\
    if ( (clr=cmap_cache[cache_i]) == 0xffff)	\
    {  clr = coff + CMAP_Find_Closest(the_cmap,csize,xa_gamma_adj[r],	\
	 	xa_gamma_adj[g], xa_gamma_adj[b],16,16,16,xaTRUE);	\
       cmap_cache[cache_i] = (xaUSHORT)clr;				\
    }						\
    *ip++ = (xaUBYTE)clr;			\
} }

#define iYUV_TO_RGB(y,cr,cg,cb,ip)	\
{ xaLONG YY = (xaLONG)(y);		\
  *ip++ = (xaUBYTE)rnglimit[ (YY + cr) >> 6 ];	\
  *ip++ = (xaUBYTE)rnglimit[ (YY + cg) >> 6 ];	\
  *ip++ = (xaUBYTE)rnglimit[ (YY + cb) >> 6 ];	\
}

#define YUV_TO_332_DITH(y,cr,cg,cb,m_flag,map,chdr,ip)	\
{ xaLONG r,g,b,YY = (xaLONG)(y);	\
  r = (xaLONG)rnglimit[(YY + cr + re) >> 6]; 		\
  g = (xaLONG)rnglimit[(YY + cg + ge) >> 6]; 		\
  b = (xaLONG)rnglimit[(YY + cb + be) >> 6]; 		\
  YY = (r & 0xe0) | ((g & 0xe0) >> 3) | ((b & 0xc0) >> 6);	\
  if (m_flag) YY = map[YY];     *ip++ = (xaUBYTE)(YY);       \
  re =  (xaLONG)(r << 6) - (xaLONG)(chdr->cmap[YY].red   >> 2);  \
  ge =  (xaLONG)(g << 6) - (xaLONG)(chdr->cmap[YY].green >> 2);  \
  be =  (xaLONG)(b << 6) - (xaLONG)(chdr->cmap[YY].blue  >> 2);  \
} 

#define YUV_TO_332(y,cr,cg,cb,m_flag,map,ip)	\
{ xaULONG r,g,b; xaLONG YY = (xaLONG)(y);	\
  r = (xaULONG)rnglimit[(YY + cr) >> 6]; 		\
  g = (xaULONG)rnglimit[(YY + cg) >> 6]; 		\
  b = (xaULONG)rnglimit[(YY + cb) >> 6]; 		\
  YY = (r & 0xe0) | ((g & 0xe0) >> 3) | ((b & 0xc0) >> 6);  \
  *ip++ = (xaUBYTE)( (m_flag)?(map[YY]):(YY) );		\
} 



/*******
 *
 ***/
void
XA_YUV111_To_Gray(iptr,imagex,imagey,uv_size,ip_size,yuv_bufs,yuv_tabs,map_flag,map,chdr)
xaUBYTE *iptr; xaLONG imagex,imagey; xaULONG uv_size,ip_size;
YUVBufs *yuv_bufs;
YUVTabs *yuv_tabs;
xaULONG map_flag,*map; XA_CHDR *chdr;
{ xaULONG yi = 8;	xaUBYTE *yptr = yuv_bufs->Ybuf;
  xaLONG *YTab,*UBTab,*VRTab,*UGTab,*VGTab;
  YTab=yuv_tabs->YUV_Y_tab;
  UBTab=yuv_tabs->YUV_UB_tab; VRTab=yuv_tabs->YUV_VR_tab;
  UGTab=yuv_tabs->YUV_UG_tab; VGTab=yuv_tabs->YUV_VG_tab;

  if (map_flag==xaFALSE)
  { while(yi--)
    { xaUBYTE *yp=yptr; xaLONG skip=0; xaLONG xi=imagex; xaUBYTE *ip=(xaUBYTE *)iptr;
      if (imagey <= 0) return;
      while(xi--) {*ip++ = *yp++; skip++; if (skip >= 8) {skip=0; yp += 56;} }
      yptr += 8; imagey--;  iptr += ip_size;
  } }
  else if (x11_bytes_pixel==1)
  { while(yi--)
    { xaUBYTE *yp=yptr; xaLONG skip=0; xaLONG xi=imagex; xaUBYTE *ip=(xaUBYTE *)iptr;
      if (imagey <= 0) return;
      while(xi--) { *ip++ = (xaUBYTE)map[ *yp++ ]; 
			skip++; if (skip >= 8) {skip = 0; yp += 56;} }
      yptr += 8; imagey--;  iptr += ip_size;
  } }
  else if (x11_bytes_pixel==4)
  { while(yi--)
    { xaUBYTE *yp=yptr; xaLONG skip=0; xaLONG xi=imagex; xaULONG *ip=(xaULONG *)iptr;
      if (imagey <= 0) return;
      while(xi--) { *ip++ = (xaULONG)map[ *yp++ ]; 
			skip++; if (skip >= 8) {skip = 0; yp += 56;} }
      yptr += 8; imagey--;  iptr += ip_size;
  } }
  else /* if (x11_bytes_pixel==2) */
  { while(yi--)
    { xaUBYTE *yp=yptr; xaLONG skip=0; xaLONG xi=imagex; xaUSHORT *ip=(xaUSHORT *)iptr;
      if (imagey <= 0) return;
      while(xi--) { *ip++ = (xaUSHORT)map[ *yp++ ]; 
			skip++; if (skip >= 8) {skip = 0; yp += 56;} }
      yptr += 8; imagey--;  iptr += ip_size;
  } }

}

/*
#define YUV_TO_RGB(y,cr,cg,cb,ip) { xaLONG YY = (xaLONG)(y);	\
  *ip++ = rnglimit[ (YY+cr) >> 6 ];		\
  *ip++ = rnglimit[ (YY+cg) >> 6 ];  		\
  *ip++ = rnglimit[ (YY+cb) >> 6 ];	}
*/


/*******
 *
 ***/
void
XA_YUV111_To_RGB(iptr,imagex,imagey,uv_size,ip_size,yuv_bufs,yuv_tabs,map_flag,map,chdr)
xaUBYTE *iptr;
xaLONG imagex,imagey;
xaULONG uv_size,ip_size;
YUVBufs *yuv_bufs;
YUVTabs *yuv_tabs;
xaULONG map_flag,*map;
XA_CHDR *chdr;
{ xaULONG yi;	xaUBYTE *yptr,*uptr,*vptr;
  xaUBYTE *rnglimit = xa_byte_limit;
  xaLONG *YTab,*UBTab,*VRTab,*UGTab,*VGTab;
  YTab=yuv_tabs->YUV_Y_tab;
  UBTab=yuv_tabs->YUV_UB_tab; VRTab=yuv_tabs->YUV_VR_tab;
  UGTab=yuv_tabs->YUV_UG_tab; VGTab=yuv_tabs->YUV_VG_tab;
  
  yptr = yuv_bufs->Ybuf; uptr = yuv_bufs->Ubuf; vptr = yuv_bufs->Vbuf;
  for(yi = 0; yi < 8; yi++)
  { xaUBYTE *yp,*up,*vp,*ip;  xaLONG xi,skip;
    if (imagey <= 0) return;
    yp = yptr; up = uptr; vp = vptr; ip = iptr; xi = imagex;  skip = 0;
    while(xi--)
    { xaLONG cr,cg,cb;	xaULONG u0,v0;
      u0 = (xaULONG)*up++;	v0 = (xaULONG)*vp++;
      cr = YUV_VR_tab[v0];	cb = YUV_UB_tab[u0];
      cg = YUV_UG_tab[u0] + YUV_VG_tab[v0];
      iYUV_TO_RGB(YUV_Y_tab[*yp++],cr,cg,cb,ip);
      skip++; if (skip >= 8) { skip = 0; yp += 56; up += 56; vp += 56; }
    } /* end xi */
    yptr += 8; uptr += 8; vptr += 8;
    imagey--;  iptr += ip_size;
  }
}

/*******
 *
 ***/
void
XA_YUV111_To_332_Dither(iptr,imagex,imagey,uv_size,ip_size,yuv_bufs,yuv_tabs,map_flag,map,chdr)
xaUBYTE *iptr; xaLONG imagex,imagey; xaULONG uv_size,ip_size;
YUVBufs *yuv_bufs;
YUVTabs *yuv_tabs;
xaULONG map_flag,*map; XA_CHDR *chdr;
{ xaULONG yi;	xaUBYTE *yptr,*uptr,*vptr;
  xaUBYTE *rnglimit = xa_byte_limit;
  xaLONG *YTab,*UBTab,*VRTab,*UGTab,*VGTab;
  YTab=yuv_tabs->YUV_Y_tab;
  UBTab=yuv_tabs->YUV_UB_tab; VRTab=yuv_tabs->YUV_VR_tab;
  UGTab=yuv_tabs->YUV_UG_tab; VGTab=yuv_tabs->YUV_VG_tab;

  yptr = yuv_bufs->Ybuf; uptr = yuv_bufs->Ubuf; vptr = yuv_bufs->Vbuf;
  for(yi = 0; yi < 8; yi++)
  { xaUBYTE *ip; xaUBYTE *yp,*up,*vp;  xaLONG xi,skip,re,ge,be;
    if (imagey <= 0) return;
    yp = yptr; up = uptr; vp = vptr; ip = iptr; xi = imagex;  skip = 0;
    re = ge = be = 0;
    while(xi--)
    { xaLONG cr,cg,cb;		xaULONG u0,v0;
      u0 = (xaULONG)*up++;	v0 = (xaULONG)*vp++; 
      cr = YUV_VR_tab[v0];	cb = YUV_UB_tab[u0];
      cg = YUV_UG_tab[u0] + YUV_VG_tab[v0];
      YUV_TO_332_DITH(YUV_Y_tab[*yp++],cr,cg,cb,map_flag,map,chdr,ip);
      skip++; if (skip >= 8)	{ skip = 0; yp += 56; up += 56; vp += 56; }
    }
    yptr += 8; uptr += 8; vptr += 8; imagey--;  iptr += ip_size;
  }
}

/*******
 *
 ***/
void
XA_YUV111_To_332(iptr,imagex,imagey,uv_size,ip_size,yuv_bufs,yuv_tabs,map_flag,map,chdr)
xaUBYTE *iptr; xaLONG imagex,imagey; xaULONG uv_size,ip_size;
YUVBufs *yuv_bufs;
YUVTabs *yuv_tabs;
xaULONG map_flag,*map; XA_CHDR *chdr;
{ xaULONG yi;	xaUBYTE *yptr,*uptr,*vptr;
  xaUBYTE *rnglimit = xa_byte_limit;
  xaLONG *YTab,*UBTab,*VRTab,*UGTab,*VGTab;
  YTab=yuv_tabs->YUV_Y_tab;
  UBTab=yuv_tabs->YUV_UB_tab; VRTab=yuv_tabs->YUV_VR_tab;
  UGTab=yuv_tabs->YUV_UG_tab; VGTab=yuv_tabs->YUV_VG_tab;

  yptr = yuv_bufs->Ybuf; uptr = yuv_bufs->Ubuf; vptr = yuv_bufs->Vbuf;
  for(yi = 0; yi < 8; yi++)
  { xaUBYTE *ip; xaUBYTE *yp,*up,*vp;  xaLONG xi,skip;
    if (imagey <= 0) return;
    yp = yptr; up = uptr; vp = vptr; ip = iptr; xi = imagex;  skip = 0;
    while(xi--)
    { xaLONG cr,cg,cb;		xaULONG u0,v0;
      u0 = (xaULONG)*up++;	v0 = (xaULONG)*vp++; 
      cr = YUV_VR_tab[v0];	cb = YUV_UB_tab[u0];
      cg = YUV_UG_tab[u0] + YUV_VG_tab[v0];
      YUV_TO_332(YUV_Y_tab[*yp++],cr,cg,cb,map_flag,map,ip);
      skip++; if (skip >= 8)	{ skip = 0; yp += 56; up += 56; vp += 56; }
    }
    yptr += 8; uptr += 8; vptr += 8; imagey--;  iptr += ip_size;
  }
}

/*******
 *
 ***/
void
XA_YUV111_To_CLR8(iptr,imagex,imagey,uv_size,ip_size,yuv_bufs,yuv_tabs,map_flag,map,chdr)
xaUBYTE *iptr; xaLONG imagex,imagey; xaULONG uv_size,ip_size;
YUVBufs *yuv_bufs;
YUVTabs *yuv_tabs;
xaULONG map_flag,*map; XA_CHDR *chdr;
{ xaULONG yi;	xaUBYTE *yptr,*uptr,*vptr;
  xaUBYTE *rnglimit = xa_byte_limit;
  xaLONG *YTab,*UBTab,*VRTab,*UGTab,*VGTab;
  YTab=yuv_tabs->YUV_Y_tab;
  UBTab=yuv_tabs->YUV_UB_tab; VRTab=yuv_tabs->YUV_VR_tab;
  UGTab=yuv_tabs->YUV_UG_tab; VGTab=yuv_tabs->YUV_VG_tab;

  yptr = yuv_bufs->Ybuf; uptr = yuv_bufs->Ubuf; vptr = yuv_bufs->Vbuf;
  for(yi = 0; yi < 8; yi++)
  { xaUBYTE *ip; xaUBYTE *yp,*up,*vp;  xaLONG xi,skip;
    if (imagey <= 0) return;
    yp = yptr; up = uptr; vp = vptr; ip = iptr; xi = imagex;  skip = 0;
    while(xi--)
    { xaLONG cr,cg,cb;		xaULONG u0,v0;
      u0 = (xaULONG)*up++;	v0 = (xaULONG)*vp++; 
      cr = YUV_VR_tab[v0];	cb = YUV_UB_tab[u0];
      cg = YUV_UG_tab[u0] + YUV_VG_tab[v0];
      YUV_TO_COLOR(YUV_Y_tab[*yp++],cr,cg,cb,map_flag,map,chdr,ip,xaUBYTE);
      skip++; if (skip >= 8)	{ skip = 0; yp += 56; up += 56; vp += 56; }
    }
    yptr += 8; uptr += 8; vptr += 8; imagey--;  iptr += ip_size;
  }
}

/*******
 *
 ***/
void
XA_YUV111_To_CLR16(iptr,imagex,imagey,uv_size,ip_size,yuv_bufs,yuv_tabs,map_flag,map,chdr)
xaUBYTE *iptr; xaLONG imagex,imagey; xaULONG uv_size,ip_size;
YUVBufs *yuv_bufs;
YUVTabs *yuv_tabs;
xaULONG map_flag,*map; XA_CHDR *chdr;
{ xaULONG yi;	xaUBYTE *yptr,*uptr,*vptr;
  xaUBYTE *rnglimit = xa_byte_limit;
  xaLONG *YTab,*UBTab,*VRTab,*UGTab,*VGTab;
  YTab=yuv_tabs->YUV_Y_tab;
  UBTab=yuv_tabs->YUV_UB_tab; VRTab=yuv_tabs->YUV_VR_tab;
  UGTab=yuv_tabs->YUV_UG_tab; VGTab=yuv_tabs->YUV_VG_tab;

  yptr = yuv_bufs->Ybuf; uptr = yuv_bufs->Ubuf; vptr = yuv_bufs->Vbuf;
  for(yi = 0; yi < 8; yi++)
  { xaUSHORT *ip; xaUBYTE *yp,*up,*vp;  xaLONG xi,skip;
    if (imagey <= 0) return;
    yp = yptr; up = uptr; vp = vptr; ip = (xaUSHORT *)iptr; 
    xi = imagex;  skip = 0;
    while(xi--)
    { xaLONG cr,cg,cb;		xaULONG u0,v0;
      u0 = (xaULONG)*up++;	v0 = (xaULONG)*vp++; 
      cr = YUV_VR_tab[v0];	cb = YUV_UB_tab[u0];
      cg = YUV_UG_tab[u0] + YUV_VG_tab[v0];
      YUV_TO_COLOR(YUV_Y_tab[*yp++],cr,cg,cb,map_flag,map,chdr,ip,xaUSHORT);
      skip++; if (skip >= 8)	{ skip = 0; yp += 56; up += 56; vp += 56; }
    }
    yptr += 8; uptr += 8; vptr += 8; imagey--;  iptr += ip_size;
  }
}

/*******
 *
 ***/
void
XA_YUV111_To_CLR32(iptr,imagex,imagey,uv_size,ip_size,yuv_bufs,yuv_tabs,map_flag,map,chdr)
xaUBYTE *iptr; xaLONG imagex,imagey; xaULONG uv_size,ip_size;
YUVBufs *yuv_bufs;
YUVTabs *yuv_tabs;
xaULONG map_flag,*map; XA_CHDR *chdr;
{ xaULONG yi;	xaUBYTE *yptr,*uptr,*vptr;
  xaUBYTE *rnglimit = xa_byte_limit;
  xaLONG *YTab,*UBTab,*VRTab,*UGTab,*VGTab;
  YTab=yuv_tabs->YUV_Y_tab;
  UBTab=yuv_tabs->YUV_UB_tab; VRTab=yuv_tabs->YUV_VR_tab;
  UGTab=yuv_tabs->YUV_UG_tab; VGTab=yuv_tabs->YUV_VG_tab;

  yptr = yuv_bufs->Ybuf; uptr = yuv_bufs->Ubuf; vptr = yuv_bufs->Vbuf;
  for(yi = 0; yi < 8; yi++)
  { xaULONG *ip; xaUBYTE *yp,*up,*vp;  xaLONG xi,skip;
    if (imagey <= 0) return;
    yp = yptr; up = uptr; vp = vptr; ip = (xaULONG *)iptr; xi = imagex;  skip = 0;
    while(xi--)
    { xaLONG cr,cg,cb;		xaULONG u0,v0;
      u0 = (xaULONG)*up++;	v0 = (xaULONG)*vp++; 
      cr = YUV_VR_tab[v0];	cb = YUV_UB_tab[u0];
      cg = YUV_UG_tab[u0] + YUV_VG_tab[v0];
      YUV_TO_COLOR(YUV_Y_tab[*yp++],cr,cg,cb,map_flag,map,chdr,ip,xaULONG);
      skip++; if (skip >= 8)	{ skip = 0; yp += 56; up += 56; vp += 56; }
    }
    yptr += 8; uptr += 8; vptr += 8; imagey--;  iptr += ip_size;
  }
}

/*******
 *
 ***/
void
XA_YUV211_To_332_Dither(iptr,imagex,imagey,uv_size,ip_size,yuv_bufs,yuv_tabs,map_flag,map,chdr)
xaUBYTE *iptr;
xaLONG imagex,imagey;
xaULONG uv_size,ip_size;
YUVBufs *yuv_bufs;
YUVTabs *yuv_tabs;
xaULONG map_flag,*map;
XA_CHDR *chdr;
{ xaULONG yi;	xaUBYTE *yptr,*uptr,*vptr;
  xaUBYTE *rnglimit = xa_byte_limit;
  xaLONG *YTab,*UBTab,*VRTab,*UGTab,*VGTab;
  YTab=yuv_tabs->YUV_Y_tab;
  UBTab=yuv_tabs->YUV_UB_tab; VRTab=yuv_tabs->YUV_VR_tab;
  UGTab=yuv_tabs->YUV_UG_tab; VGTab=yuv_tabs->YUV_VG_tab;

  yptr = yuv_bufs->Ybuf; uptr = yuv_bufs->Ubuf; vptr = yuv_bufs->Vbuf;
  for(yi = 0; yi < 8; yi++)
  { xaUBYTE *yp,*up,*vp,*ip;  xaLONG xi,skip,re,ge,be;
    if (imagey <= 0) return;
    yp = yptr; up = uptr; vp = vptr; ip = iptr; xi = imagex;  skip = 0;
    re = ge = be = 0;
    while(xi--)
    { xaLONG cr,cg,cb;	xaULONG u0,v0;
      u0 = (xaULONG)*up++;	v0 = (xaULONG)*vp++;
      cr = YUV_VR_tab[v0];	cb = YUV_UB_tab[u0];
      cg = YUV_UG_tab[u0] + YUV_VG_tab[v0];
      YUV_TO_332_DITH(YUV_Y_tab[*yp++],cr,cg,cb,map_flag,map,chdr,ip);
      YUV_TO_332_DITH(YUV_Y_tab[*yp++],cr,cg,cb,map_flag,map,chdr,ip);
      skip++;	if (skip == 4)	yp += 56;
      else if (skip >= 8) { skip = 0; yp += 56; up += 56; vp += 56; }
    } /* end xi */
    yptr += 8; uptr += 8; vptr += 8;
    imagey--;  iptr += ip_size;
  }
}


/*******
 *
 ***/
void
XA_YUV211_To_332(iptr,imagex,imagey,uv_size,ip_size,yuv_bufs,yuv_tabs,map_flag,map,chdr)
xaUBYTE *iptr;
xaLONG imagex,imagey;
xaULONG uv_size,ip_size;
YUVBufs *yuv_bufs;
YUVTabs *yuv_tabs;
xaULONG map_flag,*map;
XA_CHDR *chdr;
{ xaULONG yi;	xaUBYTE *yptr,*uptr,*vptr;
  xaUBYTE *rnglimit = xa_byte_limit;
  xaLONG *YTab,*UBTab,*VRTab,*UGTab,*VGTab;
  YTab=yuv_tabs->YUV_Y_tab;
  UBTab=yuv_tabs->YUV_UB_tab; VRTab=yuv_tabs->YUV_VR_tab;
  UGTab=yuv_tabs->YUV_UG_tab; VGTab=yuv_tabs->YUV_VG_tab;

  yptr = yuv_bufs->Ybuf; uptr = yuv_bufs->Ubuf; vptr = yuv_bufs->Vbuf;
  for(yi = 0; yi < 8; yi++)
  { xaUBYTE *yp,*up,*vp,*ip;  xaLONG xi,skip;
    if (imagey <= 0) return;
    yp = yptr; up = uptr; vp = vptr; ip = iptr; xi = imagex;  skip = 0;
    while(xi--)
    { xaLONG cr,cg,cb;	xaULONG u0,v0;
      u0 = (xaULONG)*up++;	v0 = (xaULONG)*vp++;
      cr = YUV_VR_tab[v0];	cb = YUV_UB_tab[u0];
      cg = YUV_UG_tab[u0] + YUV_VG_tab[v0];
      YUV_TO_332(YUV_Y_tab[*yp++],cr,cg,cb,map_flag,map,ip);
      YUV_TO_332(YUV_Y_tab[*yp++],cr,cg,cb,map_flag,map,ip);
      skip++;	if (skip == 4)	yp += 56;
      else if (skip >= 8) { skip = 0; yp += 56; up += 56; vp += 56; }
    } /* end xi */
    yptr += 8; uptr += 8; vptr += 8;
    imagey--;  iptr += ip_size;
  }
}

/*******
 *
 ***/
void
XA_YUV211_To_RGB(iptr,imagex,imagey,uv_size,ip_size,yuv_bufs,yuv_tabs,map_flag,map,chdr)
xaUBYTE *iptr;
xaLONG imagex,imagey;
xaULONG uv_size,ip_size;
YUVBufs *yuv_bufs;
YUVTabs *yuv_tabs;
xaULONG map_flag,*map;
XA_CHDR *chdr;
{ xaULONG yi;	xaUBYTE *yptr,*uptr,*vptr;
  xaUBYTE *rnglimit = xa_byte_limit;
  xaLONG *YTab,*UBTab,*VRTab,*UGTab,*VGTab;
  YTab=yuv_tabs->YUV_Y_tab;
  UBTab=yuv_tabs->YUV_UB_tab; VRTab=yuv_tabs->YUV_VR_tab;
  UGTab=yuv_tabs->YUV_UG_tab; VGTab=yuv_tabs->YUV_VG_tab;

  yptr = yuv_bufs->Ybuf; uptr = yuv_bufs->Ubuf; vptr = yuv_bufs->Vbuf;
  for(yi = 0; yi < 8; yi++)
  { xaUBYTE *yp,*up,*vp,*ip;  xaLONG xi,skip;
    if (imagey <= 0) return;
    yp = yptr; up = uptr; vp = vptr; ip = iptr; xi = imagex;  skip = 0;
    while(xi--)
    { xaLONG cr,cg,cb;	xaULONG u0,v0;
      u0 = (xaULONG)*up++;	v0 = (xaULONG)*vp++;
      cr = YUV_VR_tab[v0];	cb = YUV_UB_tab[u0];
      cg = YUV_UG_tab[u0] + YUV_VG_tab[v0];

      iYUV_TO_RGB(YUV_Y_tab[*yp++],cr,cg,cb,ip);
      iYUV_TO_RGB(YUV_Y_tab[*yp++],cr,cg,cb,ip);

      skip++;	if (skip == 4)	yp += 56;
      else if (skip >= 8) { skip = 0; yp += 56; up += 56; vp += 56; }
    } /* end xi */
    yptr += 8; uptr += 8; vptr += 8;
    imagey--;  iptr += ip_size;
  }
}

/*******
 *
 ***/
void
XA_YUV211_To_CLR8(iptr,imagex,imagey,uv_size,ip_size,yuv_bufs,yuv_tabs,map_flag,map,chdr)
xaUBYTE *iptr; xaLONG imagex,imagey; xaULONG uv_size,ip_size;
YUVBufs *yuv_bufs;
YUVTabs *yuv_tabs;
xaULONG map_flag,*map; XA_CHDR *chdr;
{ xaULONG yi;	xaUBYTE *yptr,*uptr,*vptr;
  xaUBYTE *rnglimit = xa_byte_limit;
  xaLONG *YTab,*UBTab,*VRTab,*UGTab,*VGTab;
  YTab=yuv_tabs->YUV_Y_tab;
  UBTab=yuv_tabs->YUV_UB_tab; VRTab=yuv_tabs->YUV_VR_tab;
  UGTab=yuv_tabs->YUV_UG_tab; VGTab=yuv_tabs->YUV_VG_tab;

  yptr = yuv_bufs->Ybuf; uptr = yuv_bufs->Ubuf; vptr = yuv_bufs->Vbuf;
  for(yi = 0; yi < 8; yi++)
  { xaUBYTE *yp,*up,*vp,*ip;  xaLONG xi,skip;
    if (imagey <= 0) return;
    yp = yptr; up = uptr; vp = vptr; ip = iptr; xi = imagex;  skip = 0;
    while(xi--)
    { xaLONG cr,cg,cb;		xaULONG u0,v0;
      u0 = (xaULONG)*up++;	v0 = (xaULONG)*vp++; 
      cr = YUV_VR_tab[v0];	cb = YUV_UB_tab[u0];
      cg = YUV_UG_tab[u0] + YUV_VG_tab[v0];

      YUV_TO_COLOR(YUV_Y_tab[*yp++],cr,cg,cb,map_flag,map,chdr,ip,xaUBYTE);
      YUV_TO_COLOR(YUV_Y_tab[*yp++],cr,cg,cb,map_flag,map,chdr,ip,xaUBYTE);

      skip++; if (skip == 4)	yp += 56;
      else if (skip >= 8)	{ skip = 0; yp += 56; up += 56; vp += 56; }
    }
    yptr += 8; uptr += 8; vptr += 8; imagey--;  iptr += ip_size;
  }
}


/*******
 *
 ***/
void
XA_YUV211_To_CLR16(iptr,imagex,imagey,uv_size,ip_size,yuv_bufs,yuv_tabs,map_flag,map,chdr)
xaUSHORT *iptr; xaLONG imagex,imagey; xaULONG uv_size,ip_size;
YUVBufs *yuv_bufs;
YUVTabs *yuv_tabs;
xaULONG map_flag,*map; XA_CHDR *chdr;
{ xaULONG yi;	xaUBYTE *yptr,*uptr,*vptr;
  xaUBYTE *rnglimit = xa_byte_limit;
  xaLONG *YTab,*UBTab,*VRTab,*UGTab,*VGTab;
  YTab=yuv_tabs->YUV_Y_tab;
  UBTab=yuv_tabs->YUV_UB_tab; VRTab=yuv_tabs->YUV_VR_tab;
  UGTab=yuv_tabs->YUV_UG_tab; VGTab=yuv_tabs->YUV_VG_tab;

  yptr = yuv_bufs->Ybuf; uptr = yuv_bufs->Ubuf; vptr = yuv_bufs->Vbuf;
  for(yi = 0; yi < 8; yi++)
  { xaUSHORT *ip;  xaUBYTE *yp,*up,*vp;  xaLONG xi,skip;
    if (imagey <= 0) return;
    yp = yptr; up = uptr; vp = vptr; ip = iptr; xi = imagex;  skip = 0;
    while(xi--)
    { xaLONG cr,cg,cb;		xaULONG u0,v0;
      u0 = (xaULONG)*up++;	v0 = (xaULONG)*vp++; 
      cr = YUV_VR_tab[v0];	cb = YUV_UB_tab[u0];
      cg = YUV_UG_tab[u0] + YUV_VG_tab[v0];

      YUV_TO_COLOR(YUV_Y_tab[*yp++],cr,cg,cb,map_flag,map,chdr,ip,xaUSHORT);
      YUV_TO_COLOR(YUV_Y_tab[*yp++],cr,cg,cb,map_flag,map,chdr,ip,xaUSHORT);

      skip++; if (skip == 4)	yp += 56;
      else if (skip >= 8)	{ skip = 0; yp += 56; up += 56; vp += 56; }
    }
    yptr += 8; uptr += 8; vptr += 8; imagey--;  iptr += ip_size;
  }
}

/*******
 *
 ***/
void
XA_YUV211_To_CLR32(iptr,imagex,imagey,uv_size,ip_size,yuv_bufs,yuv_tabs,map_flag,map,chdr)
xaULONG *iptr; xaLONG imagex,imagey; xaULONG uv_size,ip_size;
YUVBufs *yuv_bufs;
YUVTabs *yuv_tabs;
xaULONG map_flag,*map; XA_CHDR *chdr;
{ xaULONG yi;	xaUBYTE *yptr,*uptr,*vptr;
  xaUBYTE *rnglimit = xa_byte_limit;
  xaLONG *YTab,*UBTab,*VRTab,*UGTab,*VGTab;
  YTab=yuv_tabs->YUV_Y_tab;
  UBTab=yuv_tabs->YUV_UB_tab; VRTab=yuv_tabs->YUV_VR_tab;
  UGTab=yuv_tabs->YUV_UG_tab; VGTab=yuv_tabs->YUV_VG_tab;

  yptr = yuv_bufs->Ybuf; uptr = yuv_bufs->Ubuf; vptr = yuv_bufs->Vbuf;
  for(yi = 0; yi < 8; yi++)
  { xaULONG *ip; xaUBYTE *yp,*up,*vp;  xaLONG xi,skip;
    if (imagey <= 0) return;
    yp = yptr; up = uptr; vp = vptr; ip = iptr; xi = imagex;  skip = 0;
    while(xi--)
    { xaLONG cr,cg,cb;		xaULONG u0,v0;
      u0 = (xaULONG)*up++;	v0 = (xaULONG)*vp++; 
      cr = YUV_VR_tab[v0];	cb = YUV_UB_tab[u0];
      cg = YUV_UG_tab[u0] + YUV_VG_tab[v0];

      YUV_TO_COLOR(YUV_Y_tab[*yp++],cr,cg,cb,map_flag,map,chdr,ip,xaULONG);
      YUV_TO_COLOR(YUV_Y_tab[*yp++],cr,cg,cb,map_flag,map,chdr,ip,xaULONG);

      skip++; if (skip == 4)	yp += 56;
      else if (skip >= 8)	{ skip = 0; yp += 56; up += 56; vp += 56; }
    }
    yptr += 8; uptr += 8; vptr += 8; imagey--;  iptr += ip_size;
  }
}

/*******
 *
 ***/
void
XA_YUV411_To_RGB(iptr,imagex,imagey,uv_size,ip_size,yuv_bufs,yuv_tabs,map_flag,map,chdr)
xaUBYTE *iptr;
xaLONG imagex,imagey;
xaULONG uv_size,ip_size;
YUVBufs *yuv_bufs;
YUVTabs *yuv_tabs;
xaULONG map_flag,*map;
XA_CHDR *chdr;
{ xaULONG yi;	xaUBYTE *yptr0,*yptr1,*uptr,*vptr;
  xaUBYTE *rnglimit = xa_byte_limit;
  xaUBYTE *ybuf;
  xaLONG *YTab,*UBTab,*VRTab,*UGTab,*VGTab;
  YTab=yuv_tabs->YUV_Y_tab;
  UBTab=yuv_tabs->YUV_UB_tab; VRTab=yuv_tabs->YUV_VR_tab;
  UGTab=yuv_tabs->YUV_UG_tab; VGTab=yuv_tabs->YUV_VG_tab;

  ybuf = yptr0 = yptr1 = yuv_bufs->Ybuf; yptr1 += 8;
  uptr = yuv_bufs->Ubuf; vptr = yuv_bufs->Vbuf;
  for(yi = 0; yi < 8; yi++)
  { xaUBYTE *ip0,*ip1,*yp0,*yp1,*up,*vp;  xaLONG xi,skip;
    if (imagey <= 0) return;
    if (yi == 4) { yptr0 = ybuf + uv_size; yptr1 = yptr0 + 8; }
    yp0 = yptr0; yp1 = yptr1; up = uptr; vp = vptr;
    ip0 = iptr;  ip1 = iptr + ip_size; xi = imagex;  skip = 0;
    while(xi--)
    { xaLONG cr,cg,cb;	xaULONG u0,v0;
      u0 = (xaULONG)*up++;	v0 = (xaULONG)*vp++;
      cr = YUV_VR_tab[v0];	cb = YUV_UB_tab[u0];
      cg = YUV_UG_tab[u0] + YUV_VG_tab[v0];

      iYUV_TO_RGB(YUV_Y_tab[*yp0++],cr,cg,cb,ip0);
      iYUV_TO_RGB(YUV_Y_tab[*yp0++],cr,cg,cb,ip0);
      iYUV_TO_RGB(YUV_Y_tab[*yp1++],cr,cg,cb,ip1);
      iYUV_TO_RGB(YUV_Y_tab[*yp1++],cr,cg,cb,ip1);

      skip++; if (skip == 4)      {yp0 += 56; yp1 += 56; }
      else if (skip >= 8) {skip = 0; yp0 += 56; yp1 += 56; up += 56; vp += 56;}
    }
    yptr0 += 16; yptr1 += 16; uptr += 8; vptr += 8;
    imagey -= 2;  iptr += (ip_size << 1);
  }
}

/*******
 *
 ***/
void
XA_YUV411_To_CF4(iptr,imagex,imagey,uv_size,ip_size,yuv_bufs,yuv_tabs,map_flag,map,chdr)
xaUBYTE *iptr;
xaLONG imagex,imagey;
xaULONG uv_size,ip_size;
YUVBufs *yuv_bufs;
YUVTabs *yuv_tabs;
xaULONG map_flag,*map;
XA_CHDR *chdr;
{ xaULONG yi;     xaUBYTE *yptr0,*yptr1,*uptr,*vptr;
  xaUBYTE *rnglimit = xa_byte_limit;
  xaULONG coff,csize; ColorReg *the_cmap = chdr->cmap;
  xaUSHORT *gadj = xa_gamma_adj;
  xaUBYTE *ybuf;
  xaLONG *YTab,*UBTab,*VRTab,*UGTab,*VGTab;
  YTab=yuv_tabs->YUV_Y_tab;
  UBTab=yuv_tabs->YUV_UB_tab; VRTab=yuv_tabs->YUV_VR_tab;
  UGTab=yuv_tabs->YUV_UG_tab; VGTab=yuv_tabs->YUV_VG_tab;

  ybuf = yptr0 = yptr1 = yuv_bufs->Ybuf; yptr1 += 8;
  uptr = yuv_bufs->Ubuf; vptr = yuv_bufs->Vbuf;
  coff = chdr->coff;
  csize = chdr->csize;
  for(yi = 0; yi < 8; yi++)
  { xaUBYTE *ip0,*ip1,*yp0,*yp1,*up,*vp;  xaLONG xi,skip;
    if (imagey <= 0) return;
    if (yi == 4) { yptr0 = ybuf + uv_size; yptr1 = yptr0 + 8; }
    yp0 = yptr0; yp1 = yptr1; up = uptr; vp = vptr; xi = imagex;  skip = 0;
    ip0 = iptr;  iptr += ip_size; ip1 = iptr;  iptr += ip_size;
    while(xi--)
    { xaLONG cr,cg,cb; xaULONG u0,v0;
      u0 = (xaULONG)*up++;	v0 = (xaULONG)*vp++;
      cr = YUV_VR_tab[v0];	cb = YUV_UB_tab[u0];
      cg = YUV_UG_tab[u0] + YUV_VG_tab[v0];

      YUV_TO_CF4(YUV_Y_tab[*yp0++],cr,cg,cb,chdr,ip0);
      YUV_TO_CF4(YUV_Y_tab[*yp0++],cr,cg,cb,chdr,ip0);
      YUV_TO_CF4(YUV_Y_tab[*yp1++],cr,cg,cb,chdr,ip1);
      YUV_TO_CF4(YUV_Y_tab[*yp1++],cr,cg,cb,chdr,ip1);

      skip++; if (skip == 4)      {yp0 += 56; yp1 += 56; }
      else if (skip >= 8) {skip = 0; yp0 += 56; yp1 += 56; up += 56; vp += 56;}
    }
    yptr0 += 16; yptr1 += 16; uptr += 8; vptr += 8; imagey -= 2;
  }
}


/*******
 *
 ***/
void
XA_YUV411_To_CLR8(iptr,imagex,imagey,uv_size,ip_size,yuv_bufs,yuv_tabs,map_flag,map,chdr)
xaUBYTE *iptr;
xaLONG imagex,imagey;
xaULONG uv_size,ip_size;
YUVBufs *yuv_bufs;
YUVTabs *yuv_tabs;
xaULONG map_flag,*map;
XA_CHDR *chdr;
{ xaULONG yi;     xaUBYTE *yptr0,*yptr1,*uptr,*vptr;
  xaUBYTE *rnglimit = xa_byte_limit;
  xaUBYTE *ybuf;
  xaLONG *YTab,*UBTab,*VRTab,*UGTab,*VGTab;
  YTab=yuv_tabs->YUV_Y_tab;
  UBTab=yuv_tabs->YUV_UB_tab; VRTab=yuv_tabs->YUV_VR_tab;
  UGTab=yuv_tabs->YUV_UG_tab; VGTab=yuv_tabs->YUV_VG_tab;

  ybuf = yptr0 = yptr1 = yuv_bufs->Ybuf; yptr1 += 8;
  uptr = yuv_bufs->Ubuf; vptr = yuv_bufs->Vbuf;
  for(yi = 0; yi < 8; yi++)
  { xaUBYTE *ip0,*ip1,*yp0,*yp1,*up,*vp;  xaLONG xi,skip;
    if (imagey <= 0) return;
    if (yi == 4) { yptr0 = ybuf + uv_size; yptr1 = yptr0 + 8; }
    yp0 = yptr0; yp1 = yptr1; up = uptr; vp = vptr; xi = imagex;  skip = 0;
    ip0 = iptr;  iptr += ip_size; ip1 = iptr;  iptr += ip_size;
    while(xi--)
    { xaLONG cr,cg,cb; xaULONG u0,v0;
      u0 = (xaULONG)*up++;	v0 = (xaULONG)*vp++;
      cr = YUV_VR_tab[v0];	cb = YUV_UB_tab[u0];
      cg = YUV_UG_tab[u0] + YUV_VG_tab[v0];
      YUV_TO_COLOR(YUV_Y_tab[*yp0++],cr,cg,cb,map_flag,map,chdr,ip0,xaUBYTE);
      YUV_TO_COLOR(YUV_Y_tab[*yp0++],cr,cg,cb,map_flag,map,chdr,ip0,xaUBYTE);
      YUV_TO_COLOR(YUV_Y_tab[*yp1++],cr,cg,cb,map_flag,map,chdr,ip1,xaUBYTE);
      YUV_TO_COLOR(YUV_Y_tab[*yp1++],cr,cg,cb,map_flag,map,chdr,ip1,xaUBYTE);
      skip++; if (skip == 4)      {yp0 += 56; yp1 += 56; }
      else if (skip >= 8) {skip = 0; yp0 += 56; yp1 += 56; up += 56; vp += 56;}
    }
    yptr0 += 16; yptr1 += 16; uptr += 8; vptr += 8; imagey -= 2;
  }
}


void
XA_YUV411_To_332_Dither(iptr,imagex,imagey,uv_size,ip_size,yuv_bufs,yuv_tabs,map_flag,map,chdr)
xaUBYTE *iptr;
xaLONG imagex,imagey;
xaULONG uv_size,ip_size;
YUVBufs *yuv_bufs;
YUVTabs *yuv_tabs;
xaULONG map_flag,*map;
XA_CHDR *chdr;
{ xaULONG yi;     xaUBYTE *yptr0,*yptr1,*uptr,*vptr;
  xaUBYTE *rnglimit = xa_byte_limit;
  xaUBYTE *ybuf;
  xaLONG *YTab,*UBTab,*VRTab,*UGTab,*VGTab;
  YTab=yuv_tabs->YUV_Y_tab;
  UBTab=yuv_tabs->YUV_UB_tab; VRTab=yuv_tabs->YUV_VR_tab;
  UGTab=yuv_tabs->YUV_UG_tab; VGTab=yuv_tabs->YUV_VG_tab;

  ybuf = yptr0 = yptr1 = yuv_bufs->Ybuf; yptr1 += 8;
  uptr = yuv_bufs->Ubuf; vptr = yuv_bufs->Vbuf;
  for(yi = 0; yi < 8; yi++)
  { xaUBYTE *ip0,*ip1,*yp0,*yp1,*up,*vp;  xaLONG xi,skip,re,ge,be;
    if (imagey <= 0) return;
    if (yi == 4) { yptr0 = ybuf + uv_size; yptr1 = yptr0 + 8; }
    yp0 = yptr0; yp1 = yptr1; up = uptr; vp = vptr; xi = imagex;  skip = 0;
    ip0 = iptr;  iptr += ip_size; ip1 = iptr;  iptr += ip_size;
    re = ge = be = 0;
    while(xi--)
    { xaLONG cr,cg,cb; xaULONG u0,v0;
      u0 = (xaULONG)*up++;	v0 = (xaULONG)*vp++;
      cr = YUV_VR_tab[v0];	cb = YUV_UB_tab[u0];
      cg = YUV_UG_tab[u0] + YUV_VG_tab[v0];

      YUV_TO_332_DITH(YUV_Y_tab[*yp0++],cr,cg,cb,map_flag,map,chdr,ip0);
      YUV_TO_332_DITH(YUV_Y_tab[*yp0++],cr,cg,cb,map_flag,map,chdr,ip0);
      YUV_TO_332_DITH(YUV_Y_tab[*yp1++],cr,cg,cb,map_flag,map,chdr,ip1);
      YUV_TO_332_DITH(YUV_Y_tab[*yp1++],cr,cg,cb,map_flag,map,chdr,ip1);

      skip++; if (skip == 4)      {yp0 += 56; yp1 += 56; }
      else if (skip >= 8) {skip = 0; yp0 += 56; yp1 += 56; up += 56; vp += 56;}
    }
    yptr0 += 16; yptr1 += 16; uptr += 8; vptr += 8; imagey -= 2;
  }
}


void
XA_YUV411_To_332(iptr,imagex,imagey,uv_size,ip_size,yuv_bufs,yuv_tabs,map_flag,map,chdr)
xaUBYTE *iptr;
xaLONG imagex,imagey;
xaULONG uv_size,ip_size;
YUVBufs *yuv_bufs;
YUVTabs *yuv_tabs;
xaULONG map_flag,*map;
XA_CHDR *chdr;
{ xaULONG yi;     xaUBYTE *yptr0,*yptr1,*uptr,*vptr;
  xaUBYTE *rnglimit = xa_byte_limit;
  xaUBYTE *ybuf;
  xaLONG *YTab,*UBTab,*VRTab,*UGTab,*VGTab;
  YTab=yuv_tabs->YUV_Y_tab;
  UBTab=yuv_tabs->YUV_UB_tab; VRTab=yuv_tabs->YUV_VR_tab;
  UGTab=yuv_tabs->YUV_UG_tab; VGTab=yuv_tabs->YUV_VG_tab;

  ybuf = yptr0 = yptr1 = yuv_bufs->Ybuf; yptr1 += 8;
  uptr = yuv_bufs->Ubuf; vptr = yuv_bufs->Vbuf;
  for(yi = 0; yi < 8; yi++)
  { xaUBYTE *ip0,*ip1,*yp0,*yp1,*up,*vp;  xaLONG xi,skip;
    if (imagey <= 0) return;
    if (yi == 4) { yptr0 = ybuf + uv_size; yptr1 = yptr0 + 8; }
    yp0 = yptr0; yp1 = yptr1; up = uptr; vp = vptr; xi = imagex;  skip = 0;
    ip0 = iptr;  iptr += ip_size; ip1 = iptr;  iptr += ip_size;
    while(xi--)
    { xaLONG cr,cg,cb; xaULONG u0,v0;
      u0 = (xaULONG)*up++;	v0 = (xaULONG)*vp++;
      cr = YUV_VR_tab[v0];	cb = YUV_UB_tab[u0];
      cg = YUV_UG_tab[u0] + YUV_VG_tab[v0];

      YUV_TO_332(YUV_Y_tab[*yp0++],cr,cg,cb,map_flag,map,ip0);
      YUV_TO_332(YUV_Y_tab[*yp0++],cr,cg,cb,map_flag,map,ip0);
      YUV_TO_332(YUV_Y_tab[*yp1++],cr,cg,cb,map_flag,map,ip1);
      YUV_TO_332(YUV_Y_tab[*yp1++],cr,cg,cb,map_flag,map,ip1);

      skip++; if (skip == 4)      {yp0 += 56; yp1 += 56; }
      else if (skip >= 8) {skip = 0; yp0 += 56; yp1 += 56; up += 56; vp += 56;}
    }
    yptr0 += 16; yptr1 += 16; uptr += 8; vptr += 8; imagey -= 2;
  }
}

/*******
 *
 ***/
void
XA_YUV411_To_CLR16(iptr,imagex,imagey,uv_size,ip_size,yuv_bufs,yuv_tabs,map_flag,map,chdr)
xaUBYTE *iptr;
xaLONG imagex,imagey;
xaULONG uv_size,ip_size;
YUVBufs *yuv_bufs;
YUVTabs *yuv_tabs;
xaULONG map_flag,*map;
XA_CHDR *chdr;
{ xaULONG yi;     xaUBYTE *yptr0,*yptr1,*uptr,*vptr;
  xaUBYTE *rnglimit = xa_byte_limit;
  xaUBYTE *ybuf;
  xaLONG *YTab,*UBTab,*VRTab,*UGTab,*VGTab;
  YTab=yuv_tabs->YUV_Y_tab;
  UBTab=yuv_tabs->YUV_UB_tab; VRTab=yuv_tabs->YUV_VR_tab;
  UGTab=yuv_tabs->YUV_UG_tab; VGTab=yuv_tabs->YUV_VG_tab;

  ybuf = yptr0 = yptr1 = yuv_bufs->Ybuf; yptr1 += 8;
  uptr = yuv_bufs->Ubuf; vptr = yuv_bufs->Vbuf;
  for(yi = 0; yi < 8; yi++)
  { xaUSHORT *ip0,*ip1; xaUBYTE *yp0,*yp1,*up,*vp;  xaLONG xi,skip;
    if (imagey <= 0) return;
    if (yi == 4) { yptr0 = ybuf + uv_size; yptr1 = yptr0 + 8; }
    yp0 = yptr0; yp1 = yptr1; up = uptr; vp = vptr; xi = imagex;  skip = 0;
    ip0 = (xaUSHORT *)(iptr);  iptr += ip_size;
    ip1 = (xaUSHORT *)(iptr);  iptr += ip_size;
    while(xi--)
    { xaLONG cr,cg,cb; xaULONG u0,v0;
      u0 = (xaULONG)*up++;	v0 = (xaULONG)*vp++;
      cr = YUV_VR_tab[v0];	cb = YUV_UB_tab[u0];
      cg = YUV_UG_tab[u0] + YUV_VG_tab[v0];

      YUV_TO_COLOR(YUV_Y_tab[*yp0++],cr,cg,cb,map_flag,map,chdr,ip0,xaUSHORT);
      YUV_TO_COLOR(YUV_Y_tab[*yp0++],cr,cg,cb,map_flag,map,chdr,ip0,xaUSHORT);
      YUV_TO_COLOR(YUV_Y_tab[*yp1++],cr,cg,cb,map_flag,map,chdr,ip1,xaUSHORT);
      YUV_TO_COLOR(YUV_Y_tab[*yp1++],cr,cg,cb,map_flag,map,chdr,ip1,xaUSHORT);
      skip++; if (skip == 4)      {yp0 += 56; yp1 += 56; }
      else if (skip >= 8) {skip = 0; yp0 += 56; yp1 += 56; up += 56; vp += 56;}
    }
    yptr0 += 16; yptr1 += 16; uptr += 8; vptr += 8; imagey -= 2;
  }
}

/*******
 *
 ***/
void
XA_YUV411_To_CLR32(iptr,imagex,imagey,uv_size,ip_size,yuv_bufs,yuv_tabs,map_flag,map,chdr)
xaUBYTE *iptr;
xaLONG imagex,imagey;
xaULONG uv_size,ip_size;
YUVBufs *yuv_bufs;
YUVTabs *yuv_tabs;
xaULONG map_flag,*map;
XA_CHDR *chdr;
{ xaULONG yi;     xaUBYTE *yptr0,*yptr1,*uptr,*vptr;
  xaUBYTE *rnglimit = xa_byte_limit;
  xaUBYTE *ybuf;
  xaLONG *YTab,*UBTab,*VRTab,*UGTab,*VGTab;
  YTab=yuv_tabs->YUV_Y_tab;
  UBTab=yuv_tabs->YUV_UB_tab; VRTab=yuv_tabs->YUV_VR_tab;
  UGTab=yuv_tabs->YUV_UG_tab; VGTab=yuv_tabs->YUV_VG_tab;

  ybuf = yptr0 = yptr1 = yuv_bufs->Ybuf; yptr1 += 8;
  uptr = yuv_bufs->Ubuf; vptr = yuv_bufs->Vbuf;
  for(yi = 0; yi < 8; yi++)
  { xaULONG *ip0,*ip1; xaUBYTE *yp0,*yp1,*up,*vp;  xaLONG xi,skip;
    if (imagey <= 0) return;
    if (yi == 4) { yptr0 = ybuf + uv_size; yptr1 = yptr0 + 8; }
    yp0 = yptr0; yp1 = yptr1; up = uptr; vp = vptr; xi = imagex;  skip = 0;
    ip0 = (xaULONG *)(iptr);  iptr += ip_size;
    ip1 = (xaULONG *)(iptr);  iptr += ip_size;
    while(xi--)
    { xaLONG cr,cg,cb; xaULONG u0,v0;
      u0 = (xaULONG)*up++;	v0 = (xaULONG)*vp++;
      cr = YUV_VR_tab[v0];	cb = YUV_UB_tab[u0];
      cg = YUV_UG_tab[u0] + YUV_VG_tab[v0];
      YUV_TO_COLOR(YUV_Y_tab[*yp0++],cr,cg,cb,map_flag,map,chdr,ip0,xaULONG);
      YUV_TO_COLOR(YUV_Y_tab[*yp0++],cr,cg,cb,map_flag,map,chdr,ip0,xaULONG);
      YUV_TO_COLOR(YUV_Y_tab[*yp1++],cr,cg,cb,map_flag,map,chdr,ip1,xaULONG);
      YUV_TO_COLOR(YUV_Y_tab[*yp1++],cr,cg,cb,map_flag,map,chdr,ip1,xaULONG);
      skip++; if (skip == 4)      {yp0 += 56; yp1 += 56; }
      else if (skip >= 8) {skip = 0; yp0 += 56; yp1 += 56; up += 56; vp += 56;}
    }
    yptr0 += 16; yptr1 += 16; uptr += 8; vptr += 8; imagey -= 2;
  }
}


/************** IND STUF WORK IN **************************************/


#define IND_YUV_CALC(u,v,V2R,U2B,UV2G) { \
  V2R = VRTab[v];	\
  U2B = UBTab[u];	\
  UV2G = VGTab[v] + UGTab[u];	\
}

#define IND_YUV_RGB(y,V2R,U2B,V2G,ip)	\
{ register xaLONG _r,_g,_b,YY;		\
  YY = YTab[y];				\
  _r = (xaLONG)rnglimit[(YY + V2R) >> 6];	\
  _g = (xaLONG)rnglimit[(YY + UV2G) >> 6];	\
  _b = (xaLONG)rnglimit[(YY + U2B) >> 6];	\
  *ip++ = (xaUBYTE)_r; *ip++ = (xaUBYTE)_g; *ip++ = (xaUBYTE)_b;	\
}

/*****************
 *
 ******************/
void XA_YUV1611_To_RGB(image,imagex,imagey,i_x,i_y,yuv_bufs,yuv_tabs,map_flag,map,chdr)
xaUBYTE *image;
xaULONG imagex,imagey,i_x,i_y;
YUVBufs *yuv_bufs;
YUVTabs *yuv_tabs;
xaULONG map_flag,*map;
XA_CHDR *chdr;
{ xaULONG x,y,mx = xaMIN(i_x,imagex);
  xaULONG i_rowinc = 3 * imagex;
  xaULONG y_rowinc = i_x - 3;
  xaULONG i_rowinc12 = 3 * (imagex << 2);
  xaUBYTE *rnglimit = xa_byte_limit;
  xaUBYTE *ybuf,*ubuf,*vbuf;
  xaLONG *YTab,*UBTab,*VRTab,*UGTab,*VGTab;
  register xaULONG USkip_Mask = yuv_tabs->Uskip_mask;
  YTab=yuv_tabs->YUV_Y_tab;
  UBTab=yuv_tabs->YUV_UB_tab; VRTab=yuv_tabs->YUV_VR_tab;
  UGTab=yuv_tabs->YUV_UG_tab; VGTab=yuv_tabs->YUV_VG_tab;

  ybuf = yuv_bufs->Ybuf; ubuf = yuv_bufs->Ubuf; vbuf = yuv_bufs->Vbuf;
  for(y=0; y<imagey; y += 4)
  { xaUBYTE *iptr = image;	xaUBYTE *yptr = ybuf;
    xaUBYTE *up = ubuf;		xaUBYTE *vp = vbuf;

    if (y < i_y)
    { for(x=0; x<mx; x += 4)
      { xaULONG iU,iV;
	xaUBYTE *ip = iptr;		xaUBYTE *yp = yptr;
	xaLONG V2R,U2B,UV2G;

	iptr += 4;	yptr += 4;
	iU = (*up++);		iV = (*vp++);  
	if (iU & USkip_Mask) continue;  /* skip */
	IND_YUV_CALC(iU,iV,V2R,U2B,UV2G);

	/* Row 0 */
	IND_YUV_RGB(*yp++,V2R,U2B,UV2G,ip);
	IND_YUV_RGB(*yp++,V2R,U2B,UV2G,ip);
	IND_YUV_RGB(*yp++,V2R,U2B,UV2G,ip);
	IND_YUV_RGB(*yp  ,V2R,U2B,UV2G,ip);
	/* Row 1 */ ip += i_rowinc;  yp += y_rowinc;
	IND_YUV_RGB(*yp++,V2R,U2B,UV2G,ip);
	IND_YUV_RGB(*yp++,V2R,U2B,UV2G,ip);
	IND_YUV_RGB(*yp++,V2R,U2B,UV2G,ip);
	IND_YUV_RGB(*yp  ,V2R,U2B,UV2G,ip);
	/* Row 2 */ ip += i_rowinc;  yp += y_rowinc;
	IND_YUV_RGB(*yp++,V2R,U2B,UV2G,ip);
	IND_YUV_RGB(*yp++,V2R,U2B,UV2G,ip);
	IND_YUV_RGB(*yp++,V2R,U2B,UV2G,ip);
	IND_YUV_RGB(*yp  ,V2R,U2B,UV2G,ip);
	/* Row 3 */ ip += i_rowinc;  yp += y_rowinc;
	IND_YUV_RGB(*yp++,V2R,U2B,UV2G,ip);
	IND_YUV_RGB(*yp++,V2R,U2B,UV2G,ip);
	IND_YUV_RGB(*yp++,V2R,U2B,UV2G,ip);
	IND_YUV_RGB(*yp  ,V2R,U2B,UV2G,ip);
      } /* end x */
    } /* end y < i_y */
    image += i_rowinc12;
    ybuf += (i_x << 2);
    ubuf += (i_x >> 2);
    vbuf += (i_x >> 2);
  } /* end y */
} /* end function */




#define CMAP_GET_GRAY(r,g,b,scale) \
( ((scale)*((r)*11+(g)*16+(b)*5) ) >> xa_gray_shift)

#define CMAP_GET_332(r,g,b,scale) ( \
( (((r)*(scale)) & xa_r_mask) >> xa_r_shift) | \
( (((g)*(scale)) & xa_g_mask) >> xa_g_shift) | \
( (((b)*(scale)) & xa_b_mask) >> xa_b_shift) )


/*****************************************************************************
 * Convert to DEFAULT CASE
 *
 ******************/
#define IND_YUV_Def(y,V2R,U2B,UV2G,map_flag,map,chdr,ip,CAST)    \
{ register xaLONG _r,_g,_b,YY;              \
  register xaULONG clr,ra,ga,ba;		\
  YY = YTab[y];                            \
  _r = (xaLONG)rnglimit[(YY + V2R) >> 6];	\
  _g = (xaLONG)rnglimit[(YY + UV2G) >> 6];	\
  _b = (xaLONG)rnglimit[(YY + U2B) >> 6];	\
  ra = xa_gamma_adj[_r]; ga = xa_gamma_adj[_g]; ba = xa_gamma_adj[_b]; \
  if (x11_display_type & XA_X11_TRUE) clr = X11_Get_True_Color(ra,ga,ba,16); \
  else if ((cmap_color_func == 4) && (chdr)) \
  { register xaULONG cache_i = ((_r>>3)<<10) | ((_g>>3)<<5) | (_b>>3); \
    if (cmap_cache[cache_i] == 0xffff) \
    { clr = chdr->coff + CMAP_Find_Closest(chdr->cmap,chdr->csize, \
                                                ra,ga,ba,16,16,16,xaTRUE); \
      cmap_cache[cache_i] = (xaUSHORT)clr; \
    } else clr = (xaULONG)cmap_cache[cache_i]; \
  } else  \
  { if (cmap_true_to_332 == xaTRUE) clr = CMAP_GET_332(_r,_g,_b,CMAP_SCALE8); \
      else                        clr = CMAP_GET_GRAY(_r,_g,_b,CMAP_SCALE13); \
      if (map_flag) clr = map[clr]; \
  } \
  ip = (CAST)(clr); \
}


void XA_YUV1611_To_CLR8(image,imagex,imagey,i_x,i_y,yuv_bufs,yuv_tabs,map_flag,map,chdr)
xaUBYTE *image;
xaULONG imagex,imagey,i_x,i_y;
YUVBufs *yuv_bufs;
YUVTabs *yuv_tabs;
xaULONG map_flag,*map;
XA_CHDR *chdr;
{ xaULONG x,y,mx = xaMIN(i_x,imagex);
  xaULONG i_rowinc = imagex - 3;
  xaULONG y_rowinc = i_x - 3;
  xaUBYTE *rnglimit = xa_byte_limit;
  xaUBYTE *ybuf,*ubuf,*vbuf;
  xaLONG *YTab,*UBTab,*VRTab,*UGTab,*VGTab;
  register xaULONG USkip_Mask = yuv_tabs->Uskip_mask;
  YTab=yuv_tabs->YUV_Y_tab;
  UBTab=yuv_tabs->YUV_UB_tab; VRTab=yuv_tabs->YUV_VR_tab;
  UGTab=yuv_tabs->YUV_UG_tab; VGTab=yuv_tabs->YUV_VG_tab;

  ybuf = yuv_bufs->Ybuf; ubuf = yuv_bufs->Ubuf; vbuf = yuv_bufs->Vbuf;
  for(y=0; y<imagey; y += 4)
  { xaUBYTE *iptr = image;	xaUBYTE *yptr = ybuf;
    xaUBYTE *up = ubuf;		xaUBYTE *vp = vbuf;

    if (y < i_y)
    { for(x=0; x<mx; x += 4)
      { xaULONG iU,iV;
	xaUBYTE *ip = iptr;		xaUBYTE *yp = yptr;
	xaLONG V2R,U2B,UV2G;

	iptr += 4;	yptr += 4;
	iU = (*up++);		iV = (*vp++);  
	if (iU & USkip_Mask) continue;  /* skip */
	IND_YUV_CALC(iU,iV,V2R,U2B,UV2G);

	/* Row 0 */
	IND_YUV_Def(*yp++,V2R,U2B,UV2G,map_flag,map,chdr,*ip++,xaUBYTE);
	IND_YUV_Def(*yp++,V2R,U2B,UV2G,map_flag,map,chdr,*ip++,xaUBYTE);
	IND_YUV_Def(*yp++,V2R,U2B,UV2G,map_flag,map,chdr,*ip++,xaUBYTE);
	IND_YUV_Def(*yp  ,V2R,U2B,UV2G,map_flag,map,chdr,*ip  ,xaUBYTE);
	/* Row 1 */ ip += i_rowinc;  yp += y_rowinc;
	IND_YUV_Def(*yp++,V2R,U2B,UV2G,map_flag,map,chdr,*ip++,xaUBYTE);
	IND_YUV_Def(*yp++,V2R,U2B,UV2G,map_flag,map,chdr,*ip++,xaUBYTE);
	IND_YUV_Def(*yp++,V2R,U2B,UV2G,map_flag,map,chdr,*ip++,xaUBYTE);
	IND_YUV_Def(*yp  ,V2R,U2B,UV2G,map_flag,map,chdr,*ip  ,xaUBYTE);
	/* Row 2 */ ip += i_rowinc;  yp += y_rowinc;
	IND_YUV_Def(*yp++,V2R,U2B,UV2G,map_flag,map,chdr,*ip++,xaUBYTE);
	IND_YUV_Def(*yp++,V2R,U2B,UV2G,map_flag,map,chdr,*ip++,xaUBYTE);
	IND_YUV_Def(*yp++,V2R,U2B,UV2G,map_flag,map,chdr,*ip++,xaUBYTE);
	IND_YUV_Def(*yp  ,V2R,U2B,UV2G,map_flag,map,chdr,*ip  ,xaUBYTE);
	/* Row 3 */ ip += i_rowinc;  yp += y_rowinc;
	IND_YUV_Def(*yp++,V2R,U2B,UV2G,map_flag,map,chdr,*ip++,xaUBYTE);
	IND_YUV_Def(*yp++,V2R,U2B,UV2G,map_flag,map,chdr,*ip++,xaUBYTE);
	IND_YUV_Def(*yp++,V2R,U2B,UV2G,map_flag,map,chdr,*ip++,xaUBYTE);
	IND_YUV_Def(*yp  ,V2R,U2B,UV2G,map_flag,map,chdr,*ip  ,xaUBYTE);
      } /* end x */
    } /* end y < i_y */
    image += (imagex << 2);
    ybuf += (i_x << 2);
    ubuf += (i_x >> 2);
    vbuf += (i_x >> 2);
  } /* end y */
} /* end function */


/*****************************************************************************
 * Convert to 332
 *
 ******************/
#define IND_YUV_332(y,V2R,U2B,UV2G,map_flag,map,ip,CAST)    \
{ register xaLONG _r,_g,_b,YY;              \
  YY = YTab[y];                            \
  _r = (xaLONG)rnglimit[(YY + V2R) >> 6];                         \
  _g = (xaLONG)rnglimit[(YY + UV2G) >> 6];                   \
  _b = (xaLONG)rnglimit[(YY + U2B) >> 6];                         \
  YY = CMAP_GET_332(_r,_g,_b,CMAP_SCALE8);		\
  if (map_flag) ip = (CAST)map[YY];		\
  else 		ip = (CAST)YY;			\
}

void XA_YUV1611_To_332(image,imagex,imagey,i_x,i_y,yuv_bufs,yuv_tabs,map_flag,map,chdr)
xaUBYTE *image;
xaULONG imagex,imagey,i_x,i_y;
YUVBufs *yuv_bufs;
YUVTabs *yuv_tabs;
xaULONG map_flag,*map;
XA_CHDR *chdr;
{ xaULONG x,y,mx = xaMIN(i_x,imagex);
  xaULONG i_rowinc = imagex - 3;
  xaULONG y_rowinc = i_x - 3;
  xaUBYTE *rnglimit = xa_byte_limit;
  xaUBYTE *ybuf,*ubuf,*vbuf;
  xaLONG *YTab,*UBTab,*VRTab,*UGTab,*VGTab;
  register xaULONG USkip_Mask = yuv_tabs->Uskip_mask;
  YTab=yuv_tabs->YUV_Y_tab;
  UBTab=yuv_tabs->YUV_UB_tab; VRTab=yuv_tabs->YUV_VR_tab;
  UGTab=yuv_tabs->YUV_UG_tab; VGTab=yuv_tabs->YUV_VG_tab;

  ybuf = yuv_bufs->Ybuf; ubuf = yuv_bufs->Ubuf; vbuf = yuv_bufs->Vbuf;
  for(y=0; y<imagey; y += 4)
  { xaUBYTE *iptr = image;	xaUBYTE *yptr = ybuf;
    xaUBYTE *up = ubuf;		xaUBYTE *vp = vbuf;

    if (y < i_y)
    { for(x=0; x<mx; x += 4)
      { xaULONG iU,iV;
	xaUBYTE *ip = iptr;		xaUBYTE *yp = yptr;
	xaLONG V2R,U2B,UV2G;

	iptr += 4;	yptr += 4;
	iU = (*up++);		iV = (*vp++);  
	if (iU & USkip_Mask) continue;  /* skip */
	IND_YUV_CALC(iU,iV,V2R,U2B,UV2G);

	/* Row 0 */
	IND_YUV_332(*yp++,V2R,U2B,UV2G,map_flag,map,*ip++,xaUBYTE);
	IND_YUV_332(*yp++,V2R,U2B,UV2G,map_flag,map,*ip++,xaUBYTE);
	IND_YUV_332(*yp++,V2R,U2B,UV2G,map_flag,map,*ip++,xaUBYTE);
	IND_YUV_332(*yp  ,V2R,U2B,UV2G,map_flag,map,*ip  ,xaUBYTE);
	/* Row 1 */ ip += i_rowinc;  yp += y_rowinc;
	IND_YUV_332(*yp++,V2R,U2B,UV2G,map_flag,map,*ip++,xaUBYTE);
	IND_YUV_332(*yp++,V2R,U2B,UV2G,map_flag,map,*ip++,xaUBYTE);
	IND_YUV_332(*yp++,V2R,U2B,UV2G,map_flag,map,*ip++,xaUBYTE);
	IND_YUV_332(*yp  ,V2R,U2B,UV2G,map_flag,map,*ip  ,xaUBYTE);
	/* Row 2 */ ip += i_rowinc;  yp += y_rowinc;
	IND_YUV_332(*yp++,V2R,U2B,UV2G,map_flag,map,*ip++,xaUBYTE);
	IND_YUV_332(*yp++,V2R,U2B,UV2G,map_flag,map,*ip++,xaUBYTE);
	IND_YUV_332(*yp++,V2R,U2B,UV2G,map_flag,map,*ip++,xaUBYTE);
	IND_YUV_332(*yp  ,V2R,U2B,UV2G,map_flag,map,*ip  ,xaUBYTE);
	/* Row 3 */ ip += i_rowinc;  yp += y_rowinc;
	IND_YUV_332(*yp++,V2R,U2B,UV2G,map_flag,map,*ip++,xaUBYTE);
	IND_YUV_332(*yp++,V2R,U2B,UV2G,map_flag,map,*ip++,xaUBYTE);
	IND_YUV_332(*yp++,V2R,U2B,UV2G,map_flag,map,*ip++,xaUBYTE);
	IND_YUV_332(*yp  ,V2R,U2B,UV2G,map_flag,map,*ip  ,xaUBYTE);
      } /* end x */
    } /* end y < i_y */
    image += (imagex << 2);
    ybuf += (i_x << 2);
    ubuf += (i_x >> 2);
    vbuf += (i_x >> 2);
  } /* end y */
} /* end function */


/*****************************************************************************
 * Convert to Dith
 *
 ******************/
#define IND_YUV_DITH(y,V2R,U2B,UV2G,m_flag,map,chdr,pix)    \
{ register xaLONG _r,_g,_b,YY;              \
  YY = YTab[y];				\
  _r = (xaLONG)rnglimit[(YY + V2R + re) >> 6];	\
  _g = (xaLONG)rnglimit[(YY + UV2G + ge) >> 6];	\
  _b = (xaLONG)rnglimit[(YY + U2B + be) >> 6];	\
  /* YY = CMAP_GET_332(_r,_g,_b,CMAP_SCALE8);  */ \
  YY = (_r & 0xe0) | ((_g & 0xe0) >> 3) | ((_b & 0xc0) >> 6);	\
  if (m_flag) YY = map[YY];	pix = YY;	\
  re =  (xaLONG)(_r << 6) - (xaLONG)(chdr->cmap[YY].red   >> 2);  \
  ge =  (xaLONG)(_g << 6) - (xaLONG)(chdr->cmap[YY].green >> 2);  \
  be =  (xaLONG)(_b << 6) - (xaLONG)(chdr->cmap[YY].blue  >> 2);  \
}

void XA_YUV1611_To_332_Dither(image,imagex,imagey,i_x,i_y,yuv_bufs,yuv_tabs,map_flag,map,chdr)
xaUBYTE *image;
xaULONG imagex,imagey,i_x,i_y;
YUVBufs *yuv_bufs;
YUVTabs *yuv_tabs;
xaULONG map_flag,*map;
XA_CHDR *chdr;
{ xaULONG x,y,mx = xaMIN(i_x,imagex);
  xaULONG i_rowinc = imagex - 3;
  xaULONG y_rowinc = i_x - 3;
  xaUBYTE *rnglimit = xa_byte_limit;
  xaUBYTE *ybuf,*ubuf,*vbuf;
  xaLONG *YTab,*UBTab,*VRTab,*UGTab,*VGTab;
  register xaULONG USkip_Mask = yuv_tabs->Uskip_mask;
  YTab=yuv_tabs->YUV_Y_tab;
  UBTab=yuv_tabs->YUV_UB_tab; VRTab=yuv_tabs->YUV_VR_tab;
  UGTab=yuv_tabs->YUV_UG_tab; VGTab=yuv_tabs->YUV_VG_tab;

  ybuf = yuv_bufs->Ybuf; ubuf = yuv_bufs->Ubuf; vbuf = yuv_bufs->Vbuf;
  for(y=0; y<imagey; y += 4)
  { xaUBYTE *iptr = image;	xaUBYTE *yptr = ybuf;
    xaUBYTE *up = ubuf;		xaUBYTE *vp = vbuf;

    if (y < i_y)
    { xaLONG re,ge,be;
      re = ge = be = 0;
      for(x=0; x<mx; x += 4)
      { xaULONG iU,iV;
	xaUBYTE *ip = iptr;		
	xaUBYTE *yp0,*yp1,*yp2,*yp3;
	xaLONG V2R,U2B,UV2G;
	xaULONG r00,r01,r02,r03;
	xaULONG r10,r11,r12,r13;
	xaULONG r20,r21,r22,r23;
	xaULONG r30,r31,r32,r33;

        yp0 = yp1 = yp2 = yp3 = yptr; 
	yp1 += i_x;  yp2 += i_x << 1;  yp3 += (i_x << 1) + i_x;

	iptr += 4;	yptr += 4;
	iU = (*up++);		iV = (*vp++);  
	if (iU & USkip_Mask) continue;  /* skip */
	IND_YUV_CALC(iU,iV,V2R,U2B,UV2G);

	/* Col 0 */
	IND_YUV_DITH(*yp0++,V2R,U2B,UV2G,map_flag,map,chdr,r00);
	IND_YUV_DITH(*yp1++,V2R,U2B,UV2G,map_flag,map,chdr,r10);
	IND_YUV_DITH(*yp2++,V2R,U2B,UV2G,map_flag,map,chdr,r20);
	IND_YUV_DITH(*yp3++,V2R,U2B,UV2G,map_flag,map,chdr,r30);
	/* Col 1 */
	IND_YUV_DITH(*yp3++,V2R,U2B,UV2G,map_flag,map,chdr,r31);
	IND_YUV_DITH(*yp2++,V2R,U2B,UV2G,map_flag,map,chdr,r21);
	IND_YUV_DITH(*yp1++,V2R,U2B,UV2G,map_flag,map,chdr,r11);
	IND_YUV_DITH(*yp0++,V2R,U2B,UV2G,map_flag,map,chdr,r01);
	/* Col 2 */
	IND_YUV_DITH(*yp0++,V2R,U2B,UV2G,map_flag,map,chdr,r02);
	IND_YUV_DITH(*yp1++,V2R,U2B,UV2G,map_flag,map,chdr,r12);
	IND_YUV_DITH(*yp2++,V2R,U2B,UV2G,map_flag,map,chdr,r22);
	IND_YUV_DITH(*yp3++,V2R,U2B,UV2G,map_flag,map,chdr,r32);
	/* Col 3 */
	IND_YUV_DITH(*yp3,V2R,U2B,UV2G,map_flag,map,chdr,r33);
	IND_YUV_DITH(*yp2,V2R,U2B,UV2G,map_flag,map,chdr,r23);
	IND_YUV_DITH(*yp1,V2R,U2B,UV2G,map_flag,map,chdr,r13);
	IND_YUV_DITH(*yp0,V2R,U2B,UV2G,map_flag,map,chdr,r03);

	/* Row 0 */
	*ip++ = (xaUBYTE)(r00); *ip++ = (xaUBYTE)(r01);
	*ip++ = (xaUBYTE)(r02); *ip   = (xaUBYTE)(r03);
	/* Row 1 */ ip += i_rowinc;
	*ip++ = (xaUBYTE)(r10); *ip++ = (xaUBYTE)(r11);
	*ip++ = (xaUBYTE)(r12); *ip   = (xaUBYTE)(r13);
	/* Row 2 */ ip += i_rowinc;
	*ip++ = (xaUBYTE)(r20); *ip++ = (xaUBYTE)(r21);
	*ip++ = (xaUBYTE)(r22); *ip   = (xaUBYTE)(r23);
	/* Row 3 */ ip += i_rowinc;
	*ip++ = (xaUBYTE)(r30); *ip++ = (xaUBYTE)(r31);
	*ip++ = (xaUBYTE)(r32); *ip   = (xaUBYTE)(r33);
      } /* end x */
    } /* end y < i_y */
    image += (imagex << 2);
    ybuf += (i_x << 2);
    ubuf += (i_x >> 2);
    vbuf += (i_x >> 2);
  } /* end y */
} /* end function */

/*****************************************************************************
 * Convert to CF4
 *
 ******************/
#define IND_YUV_CF4(y,V2R,U2B,UV2G,chdr,ip,CAST)    \
{ register xaLONG _r,_g,_b,YY; 			\
  YY = YTab[y];                            \
  _r = (xaLONG)rnglimit[(YY + V2R) >> 6];	\
  _g = (xaLONG)rnglimit[(YY + UV2G) >> 6];	\
  _b = (xaLONG)rnglimit[(YY + U2B) >> 6];	\
  _r = xa_gamma_adj[_r]; _g = xa_gamma_adj[_g]; _b = xa_gamma_adj[_b];	\
  { register xaULONG cache_i = ((_r>>11)<<10) | ((_g>>11)<<5) | (_b>>11); \
    if (cmap_cache[cache_i] == 0xffff)					\
    { YY = chdr->coff + CMAP_Find_Closest(chdr->cmap,chdr->csize,	\
					_r,_g,_b,16,16,16,xaTRUE);	\
      cmap_cache[cache_i] = (xaUSHORT)YY;				\
    } else YY = (xaULONG)cmap_cache[cache_i];				\
  } 			\
  ip = (CAST)YY;	\
}
 

void XA_YUV1611_To_CF4(image,imagex,imagey,i_x,i_y,yuv_bufs,yuv_tabs,map_flag,map,chdr)
xaUBYTE *image;
xaULONG imagex,imagey,i_x,i_y;
YUVBufs *yuv_bufs;
YUVTabs *yuv_tabs;
xaULONG map_flag,*map;
XA_CHDR *chdr;
{ xaULONG x,y,mx = xaMIN(i_x,imagex);
  xaULONG i_rowinc = imagex - 3;
  xaULONG y_rowinc = i_x - 3;
  xaUBYTE *rnglimit = xa_byte_limit;
  xaUBYTE *ybuf,*ubuf,*vbuf;
  xaLONG *YTab,*UBTab,*VRTab,*UGTab,*VGTab;
  register xaULONG USkip_Mask = yuv_tabs->Uskip_mask;
  YTab=yuv_tabs->YUV_Y_tab;
  UBTab=yuv_tabs->YUV_UB_tab; VRTab=yuv_tabs->YUV_VR_tab;
  UGTab=yuv_tabs->YUV_UG_tab; VGTab=yuv_tabs->YUV_VG_tab;

  ybuf = yuv_bufs->Ybuf; ubuf = yuv_bufs->Ubuf; vbuf = yuv_bufs->Vbuf;
  for(y=0; y<imagey; y += 4)
  { xaUBYTE *iptr = image;	xaUBYTE *yptr = ybuf;
    xaUBYTE *up = ubuf;		xaUBYTE *vp = vbuf;

    if (y < i_y)
    { for(x=0; x<mx; x += 4)
      { xaULONG iU,iV;
	xaUBYTE *ip = iptr;		xaUBYTE *yp = yptr;
	xaLONG V2R,U2B,UV2G;

	iptr += 4;	yptr += 4;
	iU = (*up++);		iV = (*vp++);  
	if (iU & USkip_Mask) continue;  /* skip */
	IND_YUV_CALC(iU,iV,V2R,U2B,UV2G);

	/* Row 0 */
	IND_YUV_CF4(*yp++,V2R,U2B,UV2G,chdr,*ip++,xaUBYTE);
	IND_YUV_CF4(*yp++,V2R,U2B,UV2G,chdr,*ip++,xaUBYTE);
	IND_YUV_CF4(*yp++,V2R,U2B,UV2G,chdr,*ip++,xaUBYTE);
	IND_YUV_CF4(*yp  ,V2R,U2B,UV2G,chdr,*ip  ,xaUBYTE);
	/* Row 1 */ ip += i_rowinc;  yp += y_rowinc;
	IND_YUV_CF4(*yp++,V2R,U2B,UV2G,chdr,*ip++,xaUBYTE);
	IND_YUV_CF4(*yp++,V2R,U2B,UV2G,chdr,*ip++,xaUBYTE);
	IND_YUV_CF4(*yp++,V2R,U2B,UV2G,chdr,*ip++,xaUBYTE);
	IND_YUV_CF4(*yp  ,V2R,U2B,UV2G,chdr,*ip  ,xaUBYTE);
	/* Row 2 */ ip += i_rowinc;  yp += y_rowinc;
	IND_YUV_CF4(*yp++,V2R,U2B,UV2G,chdr,*ip++,xaUBYTE);
	IND_YUV_CF4(*yp++,V2R,U2B,UV2G,chdr,*ip++,xaUBYTE);
	IND_YUV_CF4(*yp++,V2R,U2B,UV2G,chdr,*ip++,xaUBYTE);
	IND_YUV_CF4(*yp  ,V2R,U2B,UV2G,chdr,*ip  ,xaUBYTE);
	/* Row 3 */ ip += i_rowinc;  yp += y_rowinc;
	IND_YUV_CF4(*yp++,V2R,U2B,UV2G,chdr,*ip++,xaUBYTE);
	IND_YUV_CF4(*yp++,V2R,U2B,UV2G,chdr,*ip++,xaUBYTE);
	IND_YUV_CF4(*yp++,V2R,U2B,UV2G,chdr,*ip++,xaUBYTE);
	IND_YUV_CF4(*yp  ,V2R,U2B,UV2G,chdr,*ip  ,xaUBYTE);
      } /* end x */
    } /* end y < i_y */
    image += (imagex << 2);
    ybuf += (i_x << 2);
    ubuf += (i_x >> 2);
    vbuf += (i_x >> 2);
  } /* end y */
} /* end function */

/*****************************************************************************
 * Convert to Dith
 *
 ******************/
#define IND_YUV_DITH_CF4(y,V2R,U2B,V2G,chdr,pix,CAST)    \
{ register xaLONG _r,_g,_b,YY;              \
  YY = YTab[y];				\
  _r = (xaLONG)rnglimit[(YY + V2R + re) >> 6];	\
  _g = (xaLONG)rnglimit[(YY + UV2G + ge) >> 6];	\
  _b = (xaLONG)rnglimit[(YY + U2B + be) >> 6];	\
  _r = xa_gamma_adj[_r]; _g = xa_gamma_adj[_g]; _b = xa_gamma_adj[_b];	\
  { register xaULONG cache_i = ((_r>>11)<<10) | ((_g>>11)<<5) | (_b>>11); \
    if (cmap_cache[cache_i] == 0xffff)					\
    { YY = chdr->coff + CMAP_Find_Closest(chdr->cmap,chdr->csize,	\
				_r,_g,_b,16,16,16,xaTRUE);		\
      cmap_cache[cache_i] = (xaUSHORT)YY;				\
    } else YY = (xaULONG)cmap_cache[cache_i];				\
  }						 			\
  pix = (CAST)YY;							\
  re =  ((xaLONG)(_r) - (xaLONG)(chdr->cmap[YY].red  )) >> 2;  \
  ge =  ((xaLONG)(_g) - (xaLONG)(chdr->cmap[YY].green)) >> 2;  \
  be =  ((xaLONG)(_b) - (xaLONG)(chdr->cmap[YY].blue )) >> 2;  \
}


void XA_YUV1611_To_CF4_Dither(image,imagex,imagey,i_x,i_y,yuv_bufs,yuv_tabs,map_flag,map,chdr)
xaUBYTE *image;
xaULONG imagex,imagey,i_x,i_y;
YUVBufs *yuv_bufs;
YUVTabs *yuv_tabs;
xaULONG map_flag,*map;
XA_CHDR *chdr;
{ xaULONG x,y,mx = xaMIN(i_x,imagex);
  xaULONG i_rowinc = imagex - 3;
  xaULONG y_rowinc = i_x - 3;
  xaUBYTE *rnglimit = xa_byte_limit;
  xaUBYTE *ybuf,*ubuf,*vbuf;
  xaLONG *YTab,*UBTab,*VRTab,*UGTab,*VGTab;
  register xaULONG USkip_Mask = yuv_tabs->Uskip_mask;
  YTab=yuv_tabs->YUV_Y_tab;
  UBTab=yuv_tabs->YUV_UB_tab; VRTab=yuv_tabs->YUV_VR_tab;
  UGTab=yuv_tabs->YUV_UG_tab; VGTab=yuv_tabs->YUV_VG_tab;

  ybuf = yuv_bufs->Ybuf; ubuf = yuv_bufs->Ubuf; vbuf = yuv_bufs->Vbuf;
  for(y=0; y<imagey; y += 4)
  { xaUBYTE *iptr = image;	xaUBYTE *yptr = ybuf;
    xaUBYTE *up = ubuf;		xaUBYTE *vp = vbuf;

    if (y < i_y)
    { xaLONG re,ge,be;
      re = ge = be = 0;
      for(x=0; x<mx; x += 4)
      { xaULONG iU,iV;
	xaUBYTE *ip = iptr;		
	xaUBYTE *yp0,*yp1,*yp2,*yp3;
	xaLONG V2R,U2B,UV2G;
	xaUBYTE r00,r01,r02,r03;
	xaUBYTE r10,r11,r12,r13;
	xaUBYTE r20,r21,r22,r23;
	xaUBYTE r30,r31,r32,r33;

        yp0 = yp1 = yp2 = yp3 = yptr; 
	yp1 += i_x;  yp2 += 2 * i_x;  yp3 += 3 * i_x;

	iptr += 4;	yptr += 4;
	iU = (*up++);		iV = (*vp++);  
	if (iU & USkip_Mask) continue;  /* skip */
	IND_YUV_CALC(iU,iV,V2R,U2B,UV2G);

	/* Col 0 */
	IND_YUV_DITH_CF4(*yp0++,V2R,U2B,UV2G,chdr,r00,xaUBYTE);
	IND_YUV_DITH_CF4(*yp1++,V2R,U2B,UV2G,chdr,r10,xaUBYTE);
	IND_YUV_DITH_CF4(*yp2++,V2R,U2B,UV2G,chdr,r20,xaUBYTE);
	IND_YUV_DITH_CF4(*yp3++,V2R,U2B,UV2G,chdr,r30,xaUBYTE);
	/* Col 1 */
	IND_YUV_DITH_CF4(*yp3++,V2R,U2B,UV2G,chdr,r31,xaUBYTE);
	IND_YUV_DITH_CF4(*yp2++,V2R,U2B,UV2G,chdr,r21,xaUBYTE);
	IND_YUV_DITH_CF4(*yp1++,V2R,U2B,UV2G,chdr,r11,xaUBYTE);
	IND_YUV_DITH_CF4(*yp0++,V2R,U2B,UV2G,chdr,r01,xaUBYTE);
	/* Col 2 */
	IND_YUV_DITH_CF4(*yp0++,V2R,U2B,UV2G,chdr,r02,xaUBYTE);
	IND_YUV_DITH_CF4(*yp1++,V2R,U2B,UV2G,chdr,r12,xaUBYTE);
	IND_YUV_DITH_CF4(*yp2++,V2R,U2B,UV2G,chdr,r22,xaUBYTE);
	IND_YUV_DITH_CF4(*yp3++,V2R,U2B,UV2G,chdr,r32,xaUBYTE);
	/* Col 3 */
	IND_YUV_DITH_CF4(*yp3++,V2R,U2B,UV2G,chdr,r33,xaUBYTE);
	IND_YUV_DITH_CF4(*yp2++,V2R,U2B,UV2G,chdr,r23,xaUBYTE);
	IND_YUV_DITH_CF4(*yp1++,V2R,U2B,UV2G,chdr,r13,xaUBYTE);
	IND_YUV_DITH_CF4(*yp0++,V2R,U2B,UV2G,chdr,r03,xaUBYTE);

	/* Row 0 */
	*ip++ = (xaUBYTE)(r00); *ip++ = (xaUBYTE)(r01);
	*ip++ = (xaUBYTE)(r02); *ip   = (xaUBYTE)(r03);
	/* Row 1 */ ip += i_rowinc;
	*ip++ = (xaUBYTE)(r10); *ip++ = (xaUBYTE)(r11);
	*ip++ = (xaUBYTE)(r12); *ip   = (xaUBYTE)(r13);
	/* Row 2 */ ip += i_rowinc;
	*ip++ = (xaUBYTE)(r20); *ip++ = (xaUBYTE)(r21);
	*ip++ = (xaUBYTE)(r22); *ip   = (xaUBYTE)(r23);
	/* Row 3 */ ip += i_rowinc;
	*ip++ = (xaUBYTE)(r30); *ip++ = (xaUBYTE)(r31);
	*ip++ = (xaUBYTE)(r32); *ip   = (xaUBYTE)(r33);
      } /* end x */
    } /* end y < i_y */
    image += (imagex << 2);
    ybuf += (i_x << 2);
    ubuf += (i_x >> 2);
    vbuf += (i_x >> 2);
  } /* end y */
} /* end function */


#define YUV_TO_RGB(y,cr,cg,cb,rnglimit,r,g,b)      \
{ xaLONG YY = (xaLONG)(YUV_Y_tab[(y)]);             \
  r = (xaUBYTE)rnglimit[ (YY + cr) >> 6 ];  \
  g = (xaUBYTE)rnglimit[ (YY + cg) >> 6 ];  \
  b = (xaUBYTE)rnglimit[ (YY + cb) >> 6 ];  \
}

/*****************************************************************************
 * Take Four Y's and UV and put them into a 2x2 Color structure.
 * Fill in just the r,g,b's.
 ******************/

void XA_YUV_2x2_rgb(cmap2x2,Y0,Y1,Y2,Y3,U,V)
XA_2x2_Color *cmap2x2;
xaULONG Y0,Y1,Y2,Y3,U,V;
{ xaLONG cr,cg,cb;
  xaUBYTE *rl = xa_byte_limit;

  cr = YUV_VR_tab[V];
  cb = YUV_UB_tab[U];
  cg = YUV_UG_tab[U] + YUV_VG_tab[V];

  YUV_TO_RGB(Y0,cr,cg,cb,rl,cmap2x2->r0,cmap2x2->g0,cmap2x2->b0);
  YUV_TO_RGB(Y1,cr,cg,cb,rl,cmap2x2->r1,cmap2x2->g1,cmap2x2->b1);
  YUV_TO_RGB(Y2,cr,cg,cb,rl,cmap2x2->r2,cmap2x2->g2,cmap2x2->b2);
  YUV_TO_RGB(Y3,cr,cg,cb,rl,cmap2x2->r3,cmap2x2->g3,cmap2x2->b3);
}

/*****************************************************************************
 * Take Four Y's and UV and put them into a 2x2 Color structure.
 * Convert to display clr.
 ******************/

void XA_YUV_2x2_clr(cmap2x2,Y0,Y1,Y2,Y3,U,V,map_flag,map,chdr)
XA_2x2_Color *cmap2x2;
xaULONG Y0,Y1,Y2,Y3,U,V;
xaULONG map_flag,*map;
XA_CHDR *chdr;
{ xaLONG cr,cg,cb; xaULONG r,g,b;
  xaUBYTE *rl = xa_byte_limit;

  cr = YUV_VR_tab[V];
  cb = YUV_UB_tab[U];
  cg = YUV_UG_tab[U] + YUV_VG_tab[V];

  YUV_TO_RGB(Y0,cr,cg,cb,rl,r,g,b);
  cmap2x2->clr0_0 = XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
  YUV_TO_RGB(Y1,cr,cg,cb,rl,r,g,b);
  cmap2x2->clr1_0 = XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
  YUV_TO_RGB(Y2,cr,cg,cb,rl,r,g,b);
  cmap2x2->clr2_0 = XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
  YUV_TO_RGB(Y3,cr,cg,cb,rl,r,g,b);
  cmap2x2->clr3_0 = XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
}


#define DITH_2x2_CGEN( _cc, _Y) {			\
r = rl[((_Y)+cr+re) >> 6];	\
g = rl[((_Y)+cg+ge) >> 6];	\
b = rl[((_Y)+cb+be) >> 6];	\
(_cc) = clr = XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);	}

#define DITH_2x2_EGEN(clr,r,g,b,re,ge,be) {	\
re =  (xaLONG)(r << 6) - (xaLONG)(chdr->cmap[clr].red   >> 2);	\
ge =  (xaLONG)(g << 6) - (xaLONG)(chdr->cmap[clr].green >> 2);	\
be =  (xaLONG)(b << 6) - (xaLONG)(chdr->cmap[clr].blue  >> 2);	\
} 

/*****************************************************************************
 * Take Four Y's and UV and put them into a 2x2 Color structure.
 * Produce one dither table for each YUV triplet.
 ******************/
void
XA_YUV_2x2_Dither14(cmap2x2,Y0,Y1,Y2,Y3,U,V,map_flag,map,chdr)
XA_2x2_Color *cmap2x2;
xaULONG Y0,Y1,Y2,Y3,U,V;
xaULONG map_flag,*map;
XA_CHDR *chdr;
{ xaLONG cr,cg,cb;
  xaUBYTE *rl = xa_byte_limit;
  xaULONG clr;
  xaLONG r,g,b;
  xaLONG re,ge,be;
  xaLONG YA,YB,YC,YD;

  cr = YUV_VR_tab[V];
  cb = YUV_UB_tab[U];
  cg = YUV_UG_tab[U] + YUV_VG_tab[V];
  Y0 = (xaLONG)(YUV_Y_tab[ Y0 ]);	Y1 = (xaLONG)(YUV_Y_tab[ Y1 ]); 
  Y2 = (xaLONG)(YUV_Y_tab[ Y2 ]);	Y3 = (xaLONG)(YUV_Y_tab[ Y3 ]);
  re = ge = be = 0;
	/* COLOR 0 */
  YA = Y0;
  YB = (Y0 + Y1) >> 1;
  YC = (Y0 + Y2) >> 1;
  YD = (Y0 + Y1 + Y2 + Y3) >> 2;
  DITH_2x2_CGEN(cmap2x2->clr0_3,YD);
  DITH_2x2_EGEN(clr,r,g,b,re,ge,be);
  DITH_2x2_CGEN(cmap2x2->clr0_2,YC);
  DITH_2x2_EGEN(clr,r,g,b,re,ge,be);
  DITH_2x2_CGEN(cmap2x2->clr0_0 ,YA);
  DITH_2x2_EGEN(clr,r,g,b,re,ge,be);
  DITH_2x2_CGEN(cmap2x2->clr0_1,YB);
  /* DITH_2x2_EGEN(clr,r,g,b,re,ge,be); */

	/* COLOR 1 */
  YA = (Y1 + Y0) >> 1;
  YB = Y1;
  YC = (Y0 + Y1 + Y2 + Y3) >> 2;
  YD = (Y1 + Y3) >> 1;
  re = ge = be = 0;
  DITH_2x2_CGEN(cmap2x2->clr1_0,YA);
  DITH_2x2_EGEN(clr,r,g,b,re,ge,be);
  DITH_2x2_CGEN(cmap2x2->clr1_1,YB);
  DITH_2x2_EGEN(clr,r,g,b,re,ge,be);
  DITH_2x2_CGEN(cmap2x2->clr1_3,YD);
  DITH_2x2_EGEN(clr,r,g,b,re,ge,be);
  DITH_2x2_CGEN(cmap2x2->clr1_2,YC);
  /* DITH_2x2_EGEN(clr,r,g,b,re,ge,be); */

	/* COLOR 2 */
  YA = (Y2 + Y0) >> 1;
  YB = (Y0 + Y1 + Y2 + Y3) >> 2;
  YC = Y2;
  YD = (Y2 + Y3) >> 1;
  re = ge = be = 0;
  DITH_2x2_CGEN(cmap2x2->clr2_0,YA);
  DITH_2x2_EGEN(clr,r,g,b,re,ge,be);
  DITH_2x2_CGEN(cmap2x2->clr2_1,YB);
  DITH_2x2_EGEN(clr,r,g,b,re,ge,be);
  DITH_2x2_CGEN(cmap2x2->clr2_3,YD);
  DITH_2x2_EGEN(clr,r,g,b,re,ge,be);
  DITH_2x2_CGEN(cmap2x2->clr2_2,YC);
  /* DITH_2x2_EGEN(clr,r,g,b,re,ge,be); */

	/* COLOR 3 */
  YA = (Y0 + Y1 + Y2 + Y3) >> 2;
  YB = (Y3 + Y1) >> 1;
  YC = (Y3 + Y2) >> 1;
  YD = Y3;
  re = ge = be = 0;
  DITH_2x2_CGEN(cmap2x2->clr3_3,YD);
  DITH_2x2_EGEN(clr,r,g,b,re,ge,be);
  DITH_2x2_CGEN(cmap2x2->clr3_2,YC);
  DITH_2x2_EGEN(clr,r,g,b,re,ge,be);
  DITH_2x2_CGEN(cmap2x2->clr3_0,YA);
  DITH_2x2_EGEN(clr,r,g,b,re,ge,be);
  DITH_2x2_CGEN(cmap2x2->clr3_1,YB);
}

/*****************
 * Take Four Y's and UV and put them into a 2x2 Color structure.
 * Produce two dither tables. One in one direction, the other in the opposite
 */
void
XA_YUV_2x2_Dither42(cmap2x2,Y0,Y1,Y2,Y3,U,V,map_flag,map,chdr)
XA_2x2_Color *cmap2x2;
xaULONG Y0,Y1,Y2,Y3,U,V;
xaULONG map_flag,*map;
XA_CHDR *chdr;
{ xaLONG cr,cg,cb;
  xaUBYTE *rl = xa_byte_limit;
  xaULONG clr;
  xaLONG r,g,b;
  xaLONG Y,re,ge,be;

  cr = YUV_VR_tab[V];
  cb = YUV_UB_tab[U];
  cg = YUV_UG_tab[U] + YUV_VG_tab[V];
  YUV_TO_RGB(Y0,cr,cg,cb,rl,r,g,b);
  cmap2x2->clr0_0 = clr = XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
  DITH_2x2_EGEN(clr,r,g,b,re,ge,be);
  Y = (xaLONG)(YUV_Y_tab[Y2]);
  DITH_2x2_CGEN(cmap2x2->clr2_0,Y);
  DITH_2x2_EGEN(clr,r,g,b,re,ge,be);
  Y = (xaLONG)(YUV_Y_tab[Y3]);
  DITH_2x2_CGEN(cmap2x2->clr3_0,Y);
  DITH_2x2_EGEN(clr,r,g,b,re,ge,be);
  Y = (xaLONG)(YUV_Y_tab[Y1]);
  DITH_2x2_CGEN(cmap2x2->clr1_0,Y);

  YUV_TO_RGB(Y3,cr,cg,cb,rl,r,g,b);
  cmap2x2->clr3_1 = clr = XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
  DITH_2x2_EGEN(clr,r,g,b,re,ge,be);
  Y = (xaLONG)(YUV_Y_tab[Y1]);
  DITH_2x2_CGEN(cmap2x2->clr1_1,Y);
  DITH_2x2_EGEN(clr,r,g,b,re,ge,be);
  Y = (xaLONG)(YUV_Y_tab[Y0]);
  DITH_2x2_CGEN(cmap2x2->clr0_1,Y);
  DITH_2x2_EGEN(clr,r,g,b,re,ge,be);
  Y = (xaLONG)(YUV_Y_tab[Y2]);
  DITH_2x2_CGEN(cmap2x2->clr2_1,Y);
}



#define ip_OUT_2x2_1BLK_DITH(ip,CAST,cmap2x2,rinc) { \
 *ip++ = (CAST)(cmap2x2->clr0_0);	\
 *ip++ = (CAST)(cmap2x2->clr0_1);	\
 *ip++ = (CAST)(cmap2x2->clr1_0);	\
 *ip   = (CAST)(cmap2x2->clr1_1);	\
  ip += rinc;				\
 *ip++ = (CAST)(cmap2x2->clr0_2);	\
 *ip++ = (CAST)(cmap2x2->clr0_3);	\
 *ip++ = (CAST)(cmap2x2->clr1_2);	\
 *ip   = (CAST)(cmap2x2->clr1_3);	\
  ip += rinc;				\
 *ip++ = (CAST)(cmap2x2->clr2_0);	\
 *ip++ = (CAST)(cmap2x2->clr2_1);	\
 *ip++ = (CAST)(cmap2x2->clr3_0);	\
 *ip   = (CAST)(cmap2x2->clr3_1);	\
  ip += rinc;				\
 *ip++ = (CAST)(cmap2x2->clr2_2);	\
 *ip++ = (CAST)(cmap2x2->clr2_3);	\
 *ip++ = (CAST)(cmap2x2->clr3_2);	\
 *ip   = (CAST)(cmap2x2->clr3_3);	\
}

#define ip_OUT_2x2_1BLK(ip,CAST,cmap2x2,rinc) { register CAST d0,d1; \
 *ip++ = d0 = (CAST)(cmap2x2->clr0_0); *ip++ = d0; 	\
 *ip++ = d1 = (CAST)(cmap2x2->clr1_0); *ip   = d1; 	\
  ip += rinc; \
 *ip++ = d0; *ip++ = d0; *ip++ = d1; *ip = d1; ip += rinc;	\
 *ip++ = d0 = (CAST)(cmap2x2->clr2_0); *ip++ = d0; 	\
 *ip++ = d1 = (CAST)(cmap2x2->clr3_0); *ip   = d1;		\
  ip += rinc; *ip++ = d0; *ip++ = d0; *ip++ = d1; *ip = d1; }

#define ip_OUT_2x2_2BLKS(ip,CAST,c2x2map0,c2x2map1,rinc) { \
 *ip++ = (CAST)(c2x2map0->clr0_0);	\
 *ip++ = (CAST)(c2x2map0->clr1_0);	\
 *ip++ = (CAST)(c2x2map1->clr0_0);	\
 *ip   = (CAST)(c2x2map1->clr1_0); ip += rinc; \
 *ip++ = (CAST)(c2x2map0->clr2_0);	\
 *ip++ = (CAST)(c2x2map0->clr3_0);	\
 *ip++ = (CAST)(c2x2map1->clr2_0);	\
 *ip   = (CAST)(c2x2map1->clr3_0); }

#define ip_OUT_2x2_2BLKS_DITH1(ip,CAST,c2x2map0,c2x2map1,rinc) { \
 *ip++ = (CAST)(c2x2map0->clr0_1);	\
 *ip++ = (CAST)(c2x2map0->clr1_1);	\
 *ip++ = (CAST)(c2x2map1->clr0_0);	\
 *ip   = (CAST)(c2x2map1->clr1_0); ip += rinc; \
 *ip++ = (CAST)(c2x2map0->clr2_1);	\
 *ip++ = (CAST)(c2x2map0->clr3_1);	\
 *ip++ = (CAST)(c2x2map1->clr2_0);	\
 *ip   = (CAST)(c2x2map1->clr3_0); }

#define ip_OUT_2x2_2BLKS_DITH0(ip,CAST,c2x2map0,c2x2map1,rinc) { \
 *ip++ = (CAST)(c2x2map0->clr0_0);	\
 *ip++ = (CAST)(c2x2map0->clr1_0);	\
 *ip++ = (CAST)(c2x2map1->clr0_1);	\
 *ip   = (CAST)(c2x2map1->clr1_1); ip += rinc; \
 *ip++ = (CAST)(c2x2map0->clr2_0);	\
 *ip++ = (CAST)(c2x2map0->clr3_0);	\
 *ip++ = (CAST)(c2x2map1->clr2_1);	\
 *ip   = (CAST)(c2x2map1->clr3_1); }


void XA_2x2_OUT_1BLK_rgb(image,x,y,imagex,cmap2x2)
xaUBYTE *image;
xaULONG x,y,imagex;
XA_2x2_Color *cmap2x2;
{ xaULONG row_inc = imagex - 3;
  xaUBYTE *ip = (xaUBYTE *)(image + 3*(y * imagex + x) );
  register xaUBYTE r0,r1,b0,b1,g0,g1;
  row_inc *= 3; row_inc -= 2;
  *ip++ = r0 = (xaUBYTE)(cmap2x2->r0);  
  *ip++ = g0 = (xaUBYTE)(cmap2x2->g0);  
  *ip++ = b0 = (xaUBYTE)(cmap2x2->b0);  
  *ip++ = r0; *ip++ = g0; *ip++ = b0;
  *ip++ = r1 = (xaUBYTE)(cmap2x2->r1);  
  *ip++ = g1 = (xaUBYTE)(cmap2x2->g1);  
  *ip++ = b1 = (xaUBYTE)(cmap2x2->b1);  
  *ip++ = r1; *ip++ = g1; *ip = b1;
   ip += row_inc;
  *ip++ = r0; *ip++ = g0; *ip++ = b0;
  *ip++ = r0; *ip++ = g0; *ip++ = b0;
  *ip++ = r1; *ip++ = g1; *ip++ = b1;
  *ip++ = r1; *ip++ = g1; *ip   = b1;
   ip += row_inc;
  *ip++ = r0 = (xaUBYTE)(cmap2x2->r2);  
  *ip++ = g0 = (xaUBYTE)(cmap2x2->g2);  
  *ip++ = b0 = (xaUBYTE)(cmap2x2->b2);  
  *ip++ = r0; *ip++ = g0; *ip++ = b0;
  *ip++ = r1 = (xaUBYTE)(cmap2x2->r3);  
  *ip++ = g1 = (xaUBYTE)(cmap2x2->g3);  
  *ip++ = b1 = (xaUBYTE)(cmap2x2->b3);  
  *ip++ = r1; *ip++ = g1; *ip = b1;
   ip += row_inc;
  *ip++ = r0; *ip++ = g0; *ip++ = b0;
  *ip++ = r0; *ip++ = g0; *ip++ = b0;
  *ip++ = r1; *ip++ = g1; *ip++ = b1;
  *ip++ = r1; *ip++ = g1; *ip   = b1;
}

void XA_2x2_OUT_1BLK_dith8(image,x,y,imagex,cmap2x2)
xaUBYTE *image;
xaULONG x,y,imagex;
XA_2x2_Color *cmap2x2;
{ xaULONG row_inc = imagex - 3;
  xaUBYTE *ip = (xaUBYTE *)(image + y * imagex + x);
  ip_OUT_2x2_1BLK_DITH(ip,xaUBYTE,cmap2x2,row_inc);
}

void XA_2x2_OUT_1BLK_dith16(image,x,y,imagex,cmap2x2)
xaUBYTE *image;
xaULONG x,y,imagex;
XA_2x2_Color *cmap2x2;
{ xaULONG row_inc = imagex - 3;
  xaUSHORT *ip = (xaUSHORT *)(image + ((y*imagex+x)<<1) );
  ip_OUT_2x2_1BLK_DITH(ip,xaUSHORT,cmap2x2,row_inc);
}

void XA_2x2_OUT_1BLK_dith32(image,x,y,imagex,cmap2x2)
xaUBYTE *image;
xaULONG x,y,imagex;
XA_2x2_Color *cmap2x2;
{ xaULONG row_inc = imagex - 3;
  xaULONG *ip = (xaULONG *)(image + ((y*imagex+x)<<2) );
  ip_OUT_2x2_1BLK_DITH(ip,xaULONG,cmap2x2,row_inc);
}


void XA_2x2_OUT_1BLK_clr8(image,x,y,imagex,cmap2x2)
xaUBYTE *image;
xaULONG x,y,imagex;
XA_2x2_Color *cmap2x2;
{ xaULONG row_inc = imagex - 3;
  xaUBYTE *ip = (xaUBYTE *)(image + y * imagex + x);
  ip_OUT_2x2_1BLK(ip,xaUBYTE,cmap2x2,row_inc);
}

void XA_2x2_OUT_1BLK_clr16(image,x,y,imagex,cmap2x2)
xaUBYTE *image;
xaULONG x,y,imagex;
XA_2x2_Color *cmap2x2;
{ xaULONG row_inc = imagex - 3;
  xaUSHORT *ip = (xaUSHORT *)(image + ((y*imagex+x)<<1) );
  ip_OUT_2x2_1BLK(ip,xaUSHORT,cmap2x2,row_inc);
}

void XA_2x2_OUT_1BLK_clr32(image,x,y,imagex,cmap2x2)
xaUBYTE *image;
xaULONG x,y,imagex;
XA_2x2_Color *cmap2x2;
{ xaULONG row_inc = imagex - 3;
  xaULONG *ip = (xaULONG *)(image + ((y*imagex+x)<<2) );
  ip_OUT_2x2_1BLK(ip,xaULONG,cmap2x2,row_inc);
}

/**************************** 
 *
 *****/
void XA_2x2_OUT_4BLKS_rgb(image,x,y,imagex,cm0,cm1,cm2,cm3)
xaUBYTE *image;
xaULONG x,y,imagex;
XA_2x2_Color *cm0,*cm1,*cm2,*cm3;
{ xaULONG row_inc = imagex - 3;
  xaUBYTE *ip = (xaUBYTE *)(image + 3*(y * imagex + x) );
  row_inc *= 3; row_inc -= 2;
  *ip++ = (xaUBYTE)(cm0->r0);  *ip++ = (xaUBYTE)(cm0->g0);  
  *ip++ = (xaUBYTE)(cm0->b0);
  *ip++ = (xaUBYTE)(cm0->r1);  *ip++ = (xaUBYTE)(cm0->g1);  
  *ip++ = (xaUBYTE)(cm0->b1);
  *ip++ = (xaUBYTE)(cm1->r0);  *ip++ = (xaUBYTE)(cm1->g0);  
  *ip++ = (xaUBYTE)(cm1->b0);
  *ip++ = (xaUBYTE)(cm1->r1);  *ip++ = (xaUBYTE)(cm1->g1);  
  *ip   = (xaUBYTE)(cm1->b1);   ip += row_inc;
  *ip++ = (xaUBYTE)(cm0->r2);  *ip++ = (xaUBYTE)(cm0->g2);  
  *ip++ = (xaUBYTE)(cm0->b2);
  *ip++ = (xaUBYTE)(cm0->r3);  *ip++ = (xaUBYTE)(cm0->g3);  
  *ip++ = (xaUBYTE)(cm0->b3);
  *ip++ = (xaUBYTE)(cm1->r2);  *ip++ = (xaUBYTE)(cm1->g2);  
  *ip++ = (xaUBYTE)(cm1->b2);
  *ip++ = (xaUBYTE)(cm1->r3);  *ip++ = (xaUBYTE)(cm1->g3);  
  *ip   = (xaUBYTE)(cm1->b3);   ip += row_inc;
  *ip++ = (xaUBYTE)(cm2->r0);  *ip++ = (xaUBYTE)(cm2->g0);  
  *ip++ = (xaUBYTE)(cm2->b0);
  *ip++ = (xaUBYTE)(cm2->r1);  *ip++ = (xaUBYTE)(cm2->g1);  
  *ip++ = (xaUBYTE)(cm2->b1);
  *ip++ = (xaUBYTE)(cm3->r0);  *ip++ = (xaUBYTE)(cm3->g0);  
  *ip++ = (xaUBYTE)(cm3->b0);
  *ip++ = (xaUBYTE)(cm3->r1);  *ip++ = (xaUBYTE)(cm3->g1);  
  *ip   = (xaUBYTE)(cm3->b1);   ip += row_inc;
  *ip++ = (xaUBYTE)(cm2->r2);  *ip++ = (xaUBYTE)(cm2->g2);  
  *ip++ = (xaUBYTE)(cm2->b2);
  *ip++ = (xaUBYTE)(cm2->r3);  *ip++ = (xaUBYTE)(cm2->g3);  
  *ip++ = (xaUBYTE)(cm2->b3);
  *ip++ = (xaUBYTE)(cm3->r2);  *ip++ = (xaUBYTE)(cm3->g2);  
  *ip++ = (xaUBYTE)(cm3->b2);
  *ip++ = (xaUBYTE)(cm3->r3);  *ip++ = (xaUBYTE)(cm3->g3);  
  *ip   = (xaUBYTE)(cm3->b3);
}

void XA_2x2_OUT_4BLKS_dith8(image,x,y,imagex,cm0,cm1,cm2,cm3)
xaUBYTE *image;
xaULONG x,y,imagex;
XA_2x2_Color *cm0,*cm1,*cm2,*cm3;
{ xaULONG row_inc = imagex - 3;
  xaUBYTE *ip = (xaUBYTE *)(image + y * imagex + x);
  ip_OUT_2x2_2BLKS_DITH0(ip,xaUBYTE,cm0,cm1,row_inc); ip += row_inc;
  ip_OUT_2x2_2BLKS_DITH1(ip,xaUBYTE,cm2,cm3,row_inc);
}

void XA_2x2_OUT_4BLKS_dith16(image,x,y,imagex,cm0,cm1,cm2,cm3)
xaUBYTE *image;
xaULONG x,y,imagex;
XA_2x2_Color *cm0,*cm1,*cm2,*cm3;
{ xaULONG row_inc = imagex - 3;
  xaUSHORT *ip = (xaUSHORT *)(image + ((y*imagex+x)<<1) );
  ip_OUT_2x2_2BLKS_DITH0(ip,xaUSHORT,cm0,cm1,row_inc); ip += row_inc;
  ip_OUT_2x2_2BLKS_DITH1(ip,xaUSHORT,cm2,cm3,row_inc);
}

void XA_2x2_OUT_4BLKS_dith32(image,x,y,imagex,cm0,cm1,cm2,cm3)
xaUBYTE *image;
xaULONG x,y,imagex;
XA_2x2_Color *cm0,*cm1,*cm2,*cm3;
{ xaULONG row_inc = imagex - 3;
  xaULONG *ip = (xaULONG *)(image + ((y*imagex+x)<<2) );
  ip_OUT_2x2_2BLKS_DITH0(ip,xaULONG,cm0,cm1,row_inc); ip += row_inc;
  ip_OUT_2x2_2BLKS_DITH1(ip,xaULONG,cm2,cm3,row_inc);
}

void XA_2x2_OUT_4BLKS_clr8(image,x,y,imagex,cm0,cm1,cm2,cm3)
xaUBYTE *image;
xaULONG x,y,imagex;
XA_2x2_Color *cm0,*cm1,*cm2,*cm3;
{ xaULONG row_inc = imagex - 3;
  xaUBYTE *ip = (xaUBYTE *)(image + y * imagex + x);
  ip_OUT_2x2_2BLKS(ip,xaUBYTE,cm0,cm1,row_inc); ip += row_inc;
  ip_OUT_2x2_2BLKS(ip,xaUBYTE,cm2,cm3,row_inc);
}

void XA_2x2_OUT_4BLKS_clr16(image,x,y,imagex,cm0,cm1,cm2,cm3)
xaUBYTE *image;
xaULONG x,y,imagex;
XA_2x2_Color *cm0,*cm1,*cm2,*cm3;
{ xaULONG row_inc = imagex - 3;
  xaUSHORT *ip = (xaUSHORT *)(image + ((y*imagex+x)<<1) );
  ip_OUT_2x2_2BLKS(ip,xaUSHORT,cm0,cm1,row_inc); ip += row_inc;
  ip_OUT_2x2_2BLKS(ip,xaUSHORT,cm2,cm3,row_inc);
}

void XA_2x2_OUT_4BLKS_clr32(image,x,y,imagex,cm0,cm1,cm2,cm3)
xaUBYTE *image;
xaULONG x,y,imagex;
XA_2x2_Color *cm0,*cm1,*cm2,*cm3;
{ xaULONG row_inc = imagex - 3;
  xaULONG *ip = (xaULONG *)(image + ((y*imagex+x)<<2) );
  ip_OUT_2x2_2BLKS(ip,xaULONG,cm0,cm1,row_inc); ip += row_inc;
  ip_OUT_2x2_2BLKS(ip,xaULONG,cm2,cm3,row_inc);
}

