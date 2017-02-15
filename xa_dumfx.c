
/*
 * xa_dumfx.c
 *
 * Copyright (C) 1995-1998,1999 by Mark Podlipec. 
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

#include "xanim.h"
#include "xa_formats.h"

extern XA_ANIM_SETUP *XA_Get_Anim_Setup();
extern xaULONG FLI_Decode_BLACK();
extern void XA_Free_Anim_Setup();
extern XA_ACTION *ACT_Get_Action();
extern XA_CHDR *ACT_Get_CMAP();
extern void ACT_Setup_Delta();
extern XA_CHDR *CMAP_Create_332();
extern XA_CHDR *CMAP_Create_Gray();

xaULONG DUM_Read_File(const char *fname,XA_ANIM_HDR *anim_hdr)
{ xaULONG i,num_frames;
  XA_ACTION *act;
  ACT_DLTA_HDR *dlta_hdr;
  XA_ANIM_SETUP *dum;

  dum = XA_Get_Anim_Setup();
  dum->depth = 8;
  dum->imagex = 320;
  dum->imagey = 20;
  dum->pic_size = dum->imagex * dum->imagey;
  dum->pic = 0;
  dum->vid_time = 100;
  dum->vid_timelo = 0;

/*
  dum->chdr = CMAP_Create_Gray(dum->cmap,&dum->imagec);
*/
  dum->chdr = CMAP_Create_332(dum->cmap,&dum->imagec);
    
  act = ACT_Get_Action(anim_hdr,ACT_DELTA);
  dlta_hdr = (ACT_DLTA_HDR *)malloc(sizeof(ACT_DLTA_HDR));
  if (dlta_hdr == 0) TheEnd1("DUM delta: malloc failed");
  act->data = (xaUBYTE *)dlta_hdr;
  dlta_hdr->flags = ACT_SNGL_BUF | DLTA_DATA;
  dlta_hdr->fpos = 0; dlta_hdr->fsize = 0;
  dlta_hdr->xpos = dlta_hdr->ypos = 0;
  dlta_hdr->xsize = dum->imagex;
  dlta_hdr->ysize = dum->imagey;
  dlta_hdr->special = 0;
  dlta_hdr->extra = (void *)(0);
  dlta_hdr->xapi_rev = 0x0001;
  dlta_hdr->delta = FLI_Decode_BLACK;
  ACT_Setup_Delta(dum,act,dlta_hdr,0);

  if (anim_hdr->total_time)
       num_frames = (anim_hdr->total_time + dum->vid_time - 1) / dum->vid_time;
  else num_frames = 4;

  anim_hdr->frame_lst = (XA_FRAME *) malloc( (num_frames+1) * sizeof(XA_FRAME));
  if (anim_hdr->frame_lst == NULL) TheEnd1("DUM_Read_Anim: malloc err");

  for(i=0; i < num_frames; i++)
  {
    anim_hdr->frame_lst[i].time_dur	= dum->vid_time;
    anim_hdr->frame_lst[i].zztime	= i * dum->vid_time;
    anim_hdr->frame_lst[i].act		= act;
  }
  anim_hdr->frame_lst[i].zztime		= -1;
  anim_hdr->frame_lst[i].time_dur	= 0;
  anim_hdr->frame_lst[i].act		= 0;
  anim_hdr->loop_frame			= 0;
  anim_hdr->last_frame			= (i - 1);
  anim_hdr->total_time			= i * dum->vid_time;

  anim_hdr->imagex = dum->imagex;
  anim_hdr->imagey = dum->imagey;
  anim_hdr->imagec = dum->imagec;
  anim_hdr->imaged = dum->depth;
  anim_hdr->max_fvid_size = 0;
  if (xa_buffer_flag == xaFALSE) anim_hdr->anim_flags |= ANIM_SNG_BUF;
  anim_hdr->fname = anim_hdr->name; /* data file is same as name */
  XA_Free_Anim_Setup(dum);
  return(xaTRUE);
}

