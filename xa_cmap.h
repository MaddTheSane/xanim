#ifndef __XA_CMAP_H__
#define __XA_CMAP_H__

#include "xanim.h"

extern xaULONG CMAP_Find_Closest(ColorReg *t_cmap,xaULONG csize,xaLONG r,xaLONG g,xaLONG b,xaULONG rbits,xaULONG gbits,xaULONG bbits,xaULONG color_flag);
extern xaLONG CMAP_Find_Exact(ColorReg *cmap,xaULONG coff,xaULONG csize,xaUSHORT r,xaUSHORT g,xaUSHORT b,xaUSHORT gray);
extern xaLONG CMAP_CHDR_Match(XA_CHDR *chdr1, XA_CHDR *chdr2);
extern void CMAP_Remap_CHDRs(XA_CHDR *the_chdr);
extern void CMAP_Remap_CHDR(XA_CHDR *new_chdr,XA_CHDR *old_chdr);
extern void CMAP_Histogram_CHDR(XA_CHDR *chdr,xaULONG *hist,xaULONG csize,xaULONG moff);
extern void CMAP_CMAP_From_Clist(ColorReg *cmap_out,xaULONG *clist,xaULONG clist_len);
extern void CMAP_CList_CombSort(xaULONG *clist,xaULONG cnum);
extern void CMAP_CList_CombSort_Red(xaULONG *clist,xaULONG cnum);
extern void CMAP_CList_CombSort_Green(xaULONG *clist,xaULONG cnum);
extern void CMAP_CList_CombSort_Blue(xaULONG *clist,xaULONG cnum);
extern xaULONG CMAP_Gamma_Adjust(xaUSHORT *gamma_adj,double disp_gamma,double anim_gamma);

extern void CMAP_Cache_Clear(void);
extern void CMAP_Cache_Init(xaULONG flag);

extern XA_CHDR *CMAP_Create_CHDR_From_True(xaUBYTE *ipic,xaULONG rbits,xaULONG gbits,xaULONG bbits,
						xaULONG width,xaULONG height,ColorReg *cmap,xaULONG *csize);


extern XA_CHDR *CMAP_Create_332(ColorReg *cmap,xaULONG *csize);
extern XA_CHDR *CMAP_Create_422(ColorReg *cmap,xaULONG *csize);
extern XA_CHDR *CMAP_Create_Gray(ColorReg *cmap,xaULONG *csize);

extern void CMAP_Manipulate_CHDRS(void);
extern void CMAP_Expand_Maps(void);

extern xaULONG CMAP_Get_Or_Mask(xaULONG cmap_size /* number of colors in anim cmap */);

//TODO: move to xa_x11.h
extern void X11_Get_Colormap(XA_CHDR *chdr);
extern void X11_Make_Nice_CHDR(XA_CHDR *chdr);

//TODO: move to xa_act.h
extern XA_CHDR *ACT_Get_CHDR();
extern XA_CHDR *ACT_Get_CMAP();

#endif
