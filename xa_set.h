
/*
 * xa_set.h
 *
 * Copyright (C) 1992-1998,1999 by Mark Podlipec. 
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
extern Display       *theDisp;
extern Colormap      theCmap;
extern Window        mainW;
extern GC            theGC;
extern XImage        *theImage;
extern XColor         defs[256];


typedef struct STRUCT_SET_FACE_HDR
{
  xaULONG xsize,ysize;
  xaLONG x,y;
  xaLONG xoff,yoff;
  XA_ACTION *face_act;
} SET_FACE_HDR;

typedef struct STRUCT_SET_BACK_HDR
{
  xaULONG num;
  xaULONG xsize,ysize;
  xaULONG xscreen,yscreen;
  xaULONG xpos,ypos;
  XA_ACTION *back_act;
  xaULONG csize;
  XA_CHDR *chdr;
  struct STRUCT_SET_BACK_HDR *next;
} SET_BACK_HDR;

typedef struct STRUCT_SET_SSET_HDR
{
  xaULONG num;
  xaULONG face_num;
  SET_FACE_HDR *faces;
  struct STRUCT_SET_SSET_HDR *next;
} SET_SSET_HDR;

typedef struct STRUCT_SET_FRAM_HDR
{
  xaULONG time;
  XA_ACTION *cm_act;
  XA_ACTION *pms_act;
  XA_ACTION *back_act;
  XA_CHDR *cm_hdr;
  ACT_SETTER_HDR *pms_hdr;
} SET_FRAM_HDR;

