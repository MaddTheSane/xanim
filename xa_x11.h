
/*
 * xa_x11.h
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

/*********************************** X11 stuff */
extern Display		*theDisp;
extern int		theScreen;
extern Colormap		theCmap;
extern Window		mainW;
#ifdef XMBUF
extern Window		mainWbuffers[2];
extern int		mainWbufIndex;
#endif
extern GC		theGC;
extern XImage		*theImage;
extern XColor		defs[256];
/******************************** Xt new stuff */
extern XtAppContext	theContext;

extern void xanim_expose();
extern void xanim_key();
extern void xanim_quit();
extern void xanim_events();
extern void XAnim_Looped();
extern void Cycle_It();

extern void X11Setup();
extern void X11_Show_Visuals();

