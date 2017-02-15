
/*
 * xa_set.c
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
#include "xanim.h"
#include "xa_set.h"
#include "xa_iff.h"
#include "xa_formats.h"
#include <ctype.h>

SET_FRAM_HDR *SET_Init_FRAM_HDRS();
xaUBYTE *SET_Read_BACK();
xaUBYTE *SET_Read_FACE();
SET_SSET_HDR *SET_Alloc_SSET_HDR();
SET_SSET_HDR *SET_Get_SSET();
SET_BACK_HDR *SET_Alloc_BACK_HDR();
SET_BACK_HDR *SET_Get_BACK();
SET_BACK_HDR *SET_Add_Black();
void SET_TheEnd();
void SET_TheEnd1();
void SET_Extract_Directory();
void SET_Modify_Directory();
void SET_Read_SSET();
void SET_Add_SETTER();
void SET_Add_BACK();
void SET_Add_TIME();
void SET_Read_Sound_IFF();
void SET_Add_CHDR();
void SET_Free_Stuff();

XA_ACTION *ACT_Get_Action();
xaULONG UTIL_Get_Buffer_Scale();
void UTIL_Scale_Buffer_Pos();
void ACT_Setup_Mapped();

void ACT_Add_CHDR_To_Action();
extern void XA_Setup_Input_Methods();

static ColorReg set_cmap[256];
static XA_CHDR *set_chdr;
static SET_BACK_HDR *set_back_start,*set_back_cur;
static xaULONG set_back_num,set_sset_num;
static SET_FRAM_HDR *set_frames;
static xaULONG set_fram_num;
static SET_SSET_HDR *set_sset_start,*set_sset_cur;
static XA_ACTION *work_act;
static XA_ACTION *back_act;
static xaULONG set_time;
static xaLONG set_xscroll_flag,set_xscroll_len,set_yscroll_flag,set_yscroll_len;
static xaLONG set_back_xpos,set_back_ypos;
static xaULONG set_back_scroll_fram;
static xaLONG set_sset_cur_num,set_sset_xoff,set_sset_yoff;
static xaULONG set_multi_flag;
static xaULONG set_or_mask;

static char set_buff[512];
static char set_buff1[64];
static Bit_Map_Header bmhd;

static xaULONG set_imagex,set_imagey,set_imagec,set_imaged;

static xaULONG set_frame_dur = 16;
static xaULONG set_frame_time = 0;

void SET_Print_CHID(chid)
xaULONG chid;
{
  xaULONG d;
  d = (chid >> 24) & 0xff; fputc(d,stderr);
  d = (chid >> 16) & 0xff; fputc(d,stderr);
  d = (chid >>  8) & 0xff; fputc(d,stderr);
  d = chid  & 0xff; 	   fputc(d,stderr);
  fputc(' ',stderr);
}

SET_BACK_HDR *SET_Get_BACK(back_num)
xaULONG back_num;
{
  SET_BACK_HDR *tmp;

  tmp = set_back_start;
  while(tmp)
  {
    if (tmp->num == back_num) return(tmp);
    tmp = tmp->next;
  }
  fprintf(stderr,"SET_Get_BACK: invalid back_num %x\n",back_num);
  SET_TheEnd();
  return(0);
}

SET_BACK_HDR *SET_Add_Black(anim_hdr)
XA_ANIM_HDR *anim_hdr;
{
  xaUBYTE *pic;
  SET_BACK_HDR *tmp;
  XA_ACTION *act;
  xaULONG psize;

    /* if black already exists */
  if (set_back_start != 0)
	if (set_back_start->num == 0xffff) return(set_back_start);

  psize = set_imagex * set_imagey;
  if (work_act == 0)
  {
    xaUBYTE *t_pic;
    t_pic = (xaUBYTE *)malloc( XA_PIC_SIZE(psize) );
    if (t_pic == 0) SET_TheEnd1("SET a: malloc failed\n");
    memset(t_pic, 0x00, psize );
    act = ACT_Get_Action(anim_hdr,ACT_MAPPED);
    ACT_Setup_Mapped(act,t_pic,set_chdr,0,0,set_imagex,set_imagey,
	set_imagex,set_imagey,xaFALSE,0,xaTRUE,xaTRUE,xaFALSE);
    work_act = act;
  }

  tmp = (SET_BACK_HDR *)malloc( sizeof(SET_BACK_HDR) );
  if (tmp == 0) SET_TheEnd1("SET: back malloc fail\n");

  tmp->num = 0xffff;
  tmp->xsize = tmp->xscreen = set_imagex;
  tmp->ysize = tmp->yscreen = set_imagey;
  tmp->xpos = tmp->ypos = 0;
  tmp->back_act = 0;
  tmp->csize = 0;
  tmp->chdr  = 0;
  tmp->next = set_back_start;
  set_back_start = tmp;

  pic = (xaUBYTE *)malloc( XA_PIC_SIZE(psize) );
  if (pic == 0) SET_TheEnd1("SET_Add_Black: malloc failed\n");

  memset( pic, set_or_mask, psize );
  act = ACT_Get_Action(anim_hdr,ACT_MAPPED);
  tmp->back_act = act; 
  ACT_Setup_Mapped(act,pic,set_chdr,0,0,set_imagex,set_imagey,
	set_imagex,set_imagey,xaFALSE,0,xaTRUE,xaTRUE,xaFALSE);
  return(set_back_start);
}
  

SET_BACK_HDR *SET_Alloc_BACK_HDR()
{
  SET_BACK_HDR *tmp;

  
  tmp = (SET_BACK_HDR *)malloc( sizeof(SET_BACK_HDR) );
  if (tmp == 0) SET_TheEnd1("SET: back malloc fail\n");

  tmp->num = set_back_num;
  set_back_num++;
  tmp->xsize = 0;   tmp->ysize = 0;
  tmp->xscreen = 0; tmp->yscreen = 0;
  tmp->xpos = 0;    tmp->ypos = 0;
  tmp->back_act = 0;
  tmp->csize = 0;
  tmp->chdr = 0;
  tmp->next = 0;

  if (set_back_start == 0) set_back_start = tmp;
  else set_back_cur->next = tmp;
  set_back_cur = tmp;
  return(set_back_cur);
}

SET_SSET_HDR *SET_Get_SSET(sset_num)
xaULONG sset_num;
{
  SET_SSET_HDR *tmp;

  tmp = set_sset_start;
  while(tmp)
  {
    if (tmp->num == sset_num) return(tmp);
    tmp = tmp->next;
  }
  fprintf(stderr,"SET_Get_SSET: invalid sset_num %x\n",sset_num);
  SET_TheEnd();
  return(0);
}

SET_SSET_HDR *SET_Alloc_SSET_HDR()
{
  SET_SSET_HDR *tmp;

  tmp = (SET_SSET_HDR *)malloc( sizeof(SET_SSET_HDR) );
  if (tmp == 0) SET_TheEnd1("SET: set malloc fail\n");

  tmp->num = set_sset_num;
  set_sset_num++;
  tmp->faces = 0;
  tmp->next = 0;
  tmp->face_num = 0;

  if (set_sset_start == 0) set_sset_start = tmp;
  else set_sset_cur->next = tmp;
  set_sset_cur = tmp;
  return(set_sset_cur);
}


xaULONG SET_Read_File(const char *fname,XA_ANIM_HDR *anim_hdr)
{ XA_INPUT *xin = anim_hdr->xin;
  xaULONG op; 
  xaULONG cur_fram_num;
  xaULONG cur_fram_cnt;
  xaULONG exit_flag;

  set_multi_flag = xaTRUE;
  cur_fram_num   = 0;
  cur_fram_cnt   = 0;
  work_act    = 0;
  back_act    = 0;
  set_fram_num   = 0;
  set_frames     = (SET_FRAM_HDR *)0;
  set_back_start = 0;
  set_back_cur   = 0;
  set_back_num   = 0;
  set_sset_start = 0;
  set_sset_cur   = 0;
  set_sset_num   = 0;
  set_xscroll_flag = 0;
  set_xscroll_len = 0;
  set_yscroll_flag = 0;
  set_yscroll_len = 0;
  set_back_xpos = 0;
  set_back_ypos = 0;
  set_back_scroll_fram = 1;
  /* default stuff if no background */
  set_imagex = 352;
  set_imagey = 240;
  set_imaged = 5;
  bmhd.compression = BMHD_COMP_BYTERUN;
  bmhd.masking = BMHD_MSK_NONE;

  set_or_mask = CMAP_Get_Or_Mask(1 << set_imaged);

  DEBUG_LEVEL1 fprintf(stderr,"Reading SET file %s:\n",fname);
  exit_flag = 0;
  while( !(xin->At_EOF(xin)) && !exit_flag)
  {
   XA_ACTION *act;

    op = xin->Read_U8(xin);

    switch(op)
    {
     case 'a':  /* 256 ascii 32 ascii IFF stuff  Background Image */
	{
	  SET_BACK_HDR *tmp_back_hdr;
	  xaUBYTE *pic;
	  xin->Read_Block(xin,set_buff,256);	set_buff[256] = 0;
	  DEBUG_LEVEL1 fprintf(stderr," a) %s    ",set_buff);
	  xin->Read_Block(xin,set_buff1,32);	set_buff1[32] = 0;
	  DEBUG_LEVEL1 fprintf(stderr,"   %s\n",set_buff1);
	  tmp_back_hdr = SET_Alloc_BACK_HDR();
	  if (set_multi_flag == xaFALSE)
		  pic = SET_Read_BACK(xin,tmp_back_hdr);
	  else
	  { XA_INPUT xin_new;
	    SET_Extract_Directory(set_buff,set_buff1);
	    (void)XA_Setup_Input_Methods(&xin_new,set_buff);
	    if (xin_new.Open_File(&xin_new) == xaFALSE)
	    {
	      SET_Modify_Directory(set_buff,0);
	      (void)XA_Setup_Input_Methods(&xin_new,set_buff);
	      if (xin_new.Open_File(&xin_new) == xaFALSE)
	      {
	        fprintf(stderr,"Can't open SET File %s for reading\n",set_buff);
	        SET_TheEnd();
	      }
	    }
	    pic = SET_Read_BACK(&xin_new,tmp_back_hdr);
	    xin_new.Close_File(&xin_new);
	  }
	  if (pic == 0) SET_TheEnd1("SET read a) failed");
	  if (work_act == 0)
	  {
	    xaUBYTE *t_pic;
	    t_pic = (xaUBYTE *) malloc( 
		XA_PIC_SIZE(tmp_back_hdr->xsize * tmp_back_hdr->ysize) );
	    if (t_pic == 0) SET_TheEnd1("SET a: malloc failed\n");
	    memset(t_pic, 0x00, (tmp_back_hdr->xsize * tmp_back_hdr->ysize) ); 
	    act = ACT_Get_Action(anim_hdr,ACT_MAPPED);
	    ACT_Setup_Mapped(act,t_pic,set_chdr,
		0,0,tmp_back_hdr->xsize,tmp_back_hdr->ysize,
	        set_imagex,set_imagey,xaFALSE,0,xaTRUE,xaFALSE,xaFALSE);
            ACT_Add_CHDR_To_Action(act,tmp_back_hdr->chdr);
	    work_act = act;
	  }
	  act = ACT_Get_Action(anim_hdr,ACT_MAPPED);
	  tmp_back_hdr->back_act = act;
	  ACT_Setup_Mapped(act,pic,set_chdr,
		0,0,tmp_back_hdr->xsize,tmp_back_hdr->ysize,
	        set_imagex,set_imagey,xaFALSE,0,xaTRUE,xaFALSE,xaFALSE);
          ACT_Add_CHDR_To_Action(act,set_chdr);
	}
	break;
     case 'b':  /* 256 ascii 32 ascii IFF stuff  Faces */
	{
	  SET_SSET_HDR *tmp_sset_hdr;
	  tmp_sset_hdr = SET_Alloc_SSET_HDR();
	  xin->Read_Block(xin,set_buff,256); /* disk:directory name */
	  set_buff[256] = 0;
	  DEBUG_LEVEL1 fprintf(stderr," b) %s    ",set_buff);
	  xin->Read_Block(xin,set_buff1,32); /* file name */
	  set_buff1[32] = 0;
	  DEBUG_LEVEL1 fprintf(stderr,"   %s\n",set_buff1);
          if (set_multi_flag == xaFALSE) SET_Read_SSET(anim_hdr,xin,tmp_sset_hdr);
          else
          { XA_INPUT xin_new;
            SET_Extract_Directory(set_buff,set_buff1);
	    (void)XA_Setup_Input_Methods(&xin_new,set_buff);
	    if (xin_new.Open_File(&xin_new) == xaFALSE)
            {
	      SET_Modify_Directory(set_buff,0);
	      (void)XA_Setup_Input_Methods(&xin_new,set_buff);
	      if (xin_new.Open_File(&xin_new) == xaFALSE)
	      {
		fprintf(stderr,"Can't open SET File %s for reading\n",set_buff);
		SET_TheEnd();
	      }
            }
	    SET_Read_SSET(anim_hdr,&xin_new,tmp_sset_hdr);
            xin_new.Close_File(&xin_new);
	  }
	}
	break;
     case 'c': /* 10 bytes set current frame */
	{ xaULONG i;
	  xaUBYTE unk[10];
	  for(i=0; i<8; i++) unk[i] = xin->Read_U8(xin);
	  cur_fram_num  = xin->Read_MSB_U16(xin);
	  cur_fram_cnt = cur_fram_num;
	  if (cur_fram_num == 1)
	  {
	    set_frame_time = 0;
	  }
	  else
	  {
	    set_frame_time += set_frame_dur;
	  }
	  DEBUG_LEVEL2 
	  {
	    fprintf(stderr," c) %x %06x: ",cur_fram_num,set_frame_time);
	    for(i=0; i<8; i++) fprintf(stderr," %02x",unk[i]);
	    fprintf(stderr,"\n");
	  }
	}
	break;
     case 'd': /* new SSET header 32 ascii + 10 bytes */
	{
	  xaULONG i;
	  xin->Read_Block(xin,set_buff,32);
	  set_buff[32] = 0;
	  DEBUG_LEVEL1 fprintf(stderr," d) sset hdr %s  ",set_buff);
	  set_sset_cur_num = xin->Read_MSB_U16(xin);
	  set_sset_xoff = xin->Read_MSB_U16(xin);
	  set_sset_yoff = xin->Read_MSB_U16(xin);
	  i = xin->Read_MSB_U16(xin);
	  i = xin->Read_MSB_U16(xin); /* last fram in series */
	  set_sset_cur = SET_Get_SSET(set_sset_cur_num);
	  DEBUG_LEVEL2 fprintf(stderr,"  %x %x\n",
		set_sset_cur_num,set_sset_cur->num);
	  cur_fram_cnt = cur_fram_num;
	}
	break;
     case 'e': /* position face 12 bytes */
	{
	  SET_FACE_HDR *face_hdr;
	  xaULONG face_num,depth,garb,fram_num;
	  xaLONG xpos,ypos,xoff,yoff;
	  xaLONG back_x,back_y;

	  face_num = xin->Read_MSB_U16(xin);
	  xoff     = xin->Read_MSB_U16(xin);
	  yoff     = xin->Read_MSB_U16(xin);
	  depth    = xin->Read_MSB_U16(xin);
	  garb     = xin->Read_MSB_U16(xin);
	  fram_num = xin->Read_MSB_U16(xin); /* not correct */
	  DEBUG_LEVEL2 fprintf(stderr," e) %x (%x %x) %x %x %x\n",
	    face_num,xoff,yoff,depth,garb,fram_num);
	  if (face_num > set_sset_cur->face_num)
	  {
	    fprintf(stderr,"SET_Read: e) face_num invalid %x (%x)",
		face_num, set_sset_cur->face_num);
	    SET_TheEnd();
	  }
	  face_hdr = &(set_sset_cur->faces[face_num]);

	  xpos  = face_hdr->xoff + set_sset_xoff;
	  xpos += xoff;
	  ypos  = face_hdr->yoff + set_sset_yoff;
	  ypos += yoff;
DEBUG_LEVEL2 fprintf(stderr,"      e FACE off <%d,%d> fin <%d,%d>\n",
		xoff,yoff,xpos,ypos);


	  /* Back ground Scrolling calculations - see g) */
	  if (set_xscroll_flag == 1)
	  {
	    back_x = -set_xscroll_len * (cur_fram_cnt - set_back_scroll_fram);
	    while(back_x < 0) back_x += set_back_cur->xsize;
	    if (back_x >= set_back_cur->xsize) back_x %= set_back_cur->xsize;
	  } else back_x = 0;

	  if (set_yscroll_flag == 1)
	  {
	    back_y = -set_yscroll_len * (cur_fram_cnt - set_back_scroll_fram);
	    while(back_y < 0) back_y += set_back_cur->ysize;
	    if (back_y >= set_back_cur->ysize) back_y %= set_back_cur->ysize;
	  } else back_y = 0;

          if (face_hdr->face_act->type != ACT_NOP)
	    SET_Add_SETTER(anim_hdr,cur_fram_cnt,back_x,back_y,
		  xpos,ypos,face_hdr->face_act,depth);
	  cur_fram_cnt++;
	}
	break;
     case 'f': /* backgrnd display info  4 bytes */
	{
	  xaULONG back,effect;
	  back = xin->Read_MSB_U16(xin);
	  effect = xin->Read_MSB_U16(xin);
	  DEBUG_LEVEL2 
	     fprintf(stderr," f) backgrnd info %x %x  ",back,effect);
	  if (back == 0xffff) set_back_cur = SET_Add_Black(anim_hdr);
	  else set_back_cur = SET_Get_BACK(back);
	  DEBUG_LEVEL2 fprintf(stderr,"  %x\n",set_back_cur->num);
	  SET_Add_BACK(cur_fram_num,set_back_cur->back_act,set_back_cur->chdr);
	}
	break;
     case 'g': /* backgrnd scrolling info(?)  8 bytes */
	{
	  xaLONG tmp1,tmp2,tmp3,tmp4;

	  tmp1 = xin->Read_MSB_U16(xin);  if (tmp1 & 0x8000) tmp1 -= 0x10000;
	  tmp2 = xin->Read_MSB_U16(xin);  if (tmp2 & 0x8000) tmp2 -= 0x10000;
	  tmp3 = xin->Read_MSB_U16(xin);  if (tmp3 & 0x8000) tmp3 -= 0x10000;
	  tmp4 = xin->Read_MSB_U16(xin);  if (tmp4 & 0x8000) tmp4 -= 0x10000;

	  if (tmp4 == 0)
	  { /* scroll only in x direction */
	    set_xscroll_flag = 1;
	    set_yscroll_flag = 0;
	    set_yscroll_len = 0;
	    if (tmp3 == 0) set_xscroll_len = tmp1;
	    else set_xscroll_len = tmp2;
	    if (set_xscroll_len == 0) set_xscroll_flag = 0;
	  }
	  else
	  { /* scroll only in y direction */
	    set_yscroll_flag = 1;
	    set_xscroll_flag = 0;
	    set_xscroll_len = 0;
	    if (tmp3 == 0) set_yscroll_len = tmp1;
	    else set_yscroll_len = tmp2; 
	    /* PODNOTE: I haven't seen tmp3 == tmp4 == 1 so this is a guess */
	    if (set_yscroll_len == 0) set_yscroll_flag = 0;
	  }
	  set_back_scroll_fram = cur_fram_num;
	  DEBUG_LEVEL2 fprintf(stderr," g) %d %d %d %d\n",
	     set_xscroll_flag,set_xscroll_len,set_yscroll_flag,set_yscroll_len);
	}
	break;
     case 'h': /* sound info  10 bytes */
	{
	  xaULONG a,b,c,d,e;
	  a = xin->Read_MSB_U16(xin);
	  b = xin->Read_MSB_U16(xin);
	  c = xin->Read_MSB_U16(xin);
	  d = xin->Read_MSB_U16(xin);
	  e = xin->Read_MSB_U16(xin);
	  DEBUG_LEVEL2 fprintf(stderr," h) sound info: %04x %04x %04x %04x %04x\n",a,b,c,d,e);
	}
	break;
     case 'i': /* timing info  2 bytes*/
	set_time = xin->Read_MSB_U16(xin); if (set_time == 0) set_time = 1;
	DEBUG_LEVEL2 fprintf(stderr," i) %x\n",set_time);
	set_time = set_time * 16;
        SET_Add_TIME(cur_fram_num,set_time);
	set_frame_dur = set_time;
	break;
     case 'j':  /* 256 ascii 32 ascii IFF stuff  Sounds Info */
	xin->Read_Block(xin,set_buff,256); set_buff[256] = 0;
	DEBUG_LEVEL1 fprintf(stderr," j) %s    ",set_buff);
	xin->Read_Block(xin,set_buff1,32); set_buff1[32] = 0;
	DEBUG_LEVEL1 fprintf(stderr,"   %s\n",set_buff1);
        if (set_multi_flag == xaFALSE) SET_Read_Sound_IFF(xin);
	/* else ignore */
	break;
     case 'k': /* ? header of some sort 40 bytes */
	{
	  xaULONG i;
	  DEBUG_LEVEL2 fprintf(stderr," k) ???\n");
	  i = 40; while(i--) xin->Read_U8(xin);
	}
	break;
     case 'l': /* cmap info 64 bytes */
	DEBUG_LEVEL2 fprintf(stderr," l) cmap\n");
	set_imagec = 32;
	IFF_Read_CMAP_1(set_cmap,set_imagec,xin);
	IFF_Shift_CMAP(set_cmap,set_imagec);
	set_chdr = ACT_Get_CMAP(set_cmap,set_imagec,set_or_mask,
				set_imagec,set_or_mask,4,4,4);
	SET_Add_CHDR(set_chdr,cur_fram_num);
	break;
     case 'v': /* ? header of some sort 26 bytes */
	{
	  xaULONG i;
	  DEBUG_LEVEL2 fprintf(stderr," v) ?header?\n");
	  i = 26; while(i--) xin->Read_U8(xin);
	}
	break;
     case 'w': /* frame count */
	set_fram_num = xin->Read_MSB_U16(xin)+1;
        set_frames = SET_Init_FRAM_HDRS(set_fram_num);
	DEBUG_LEVEL1 fprintf(stderr," w) %d frames\n",set_fram_num);
	break;
     case 'x': /* title and version 32 ascii*/
	xin->Read_Block(xin,set_buff1,32); set_buff1[32] = 0;
	DEBUG_LEVEL1 fprintf(stderr," x) %s\n",set_buff1);
	break;
     case 'y': /*  8 bytes  if present indicates IFF chunks included  */
	DEBUG_LEVEL2 fprintf(stderr," y)\n");
	set_multi_flag = xaFALSE;
	break;
     case 'z': /* EOF marker */
	exit_flag = 1;
	break;


    default: fprintf(stderr,"Unknown opcode = %x\n",op);
        xin->Close_File(xin);
	SET_TheEnd();
   } /* end of switch */
 } /* end of while */
 xin->Close_File(xin);

 {
   xaULONG frame_cnt,i,j;
   XA_ACTION *back_act;
   xaULONG fram_time;
   xaULONG buffx,buffy,need_to_scale;
   xaLONG t_time;

   /* Check Out For buffer Scaling */
   need_to_scale = UTIL_Get_Buffer_Scale(set_imagex,set_imagey,&buffx,&buffy);

   fram_time = 1;
   frame_cnt = 0;
   back_act = 0;
   for(i=0; i<set_fram_num; i++)
   {
     SET_FRAM_HDR *fram_hdr;
     ACT_SETTER_HDR *pms_hdr;

     fram_hdr = &(set_frames[i]);
     frame_cnt++;
     /* register chdr's */
     if (fram_hdr->cm_hdr == 0) fram_hdr->cm_hdr = set_chdr;
     else set_chdr = fram_hdr->cm_hdr;

     /* spread out timing info */
     if (fram_hdr->time == 0xffffffff)
     {
       fram_hdr->time = fram_time;
     }
     else
     {
       fram_time = fram_hdr->time;
     }

     /* spread out backgrounds */
     if (fram_hdr->back_act != 0) back_act = fram_hdr->back_act;
     else 
       if (back_act != 0) fram_hdr->back_act = back_act;
       else 
       { 
         set_back_cur = SET_Add_Black(anim_hdr);
         back_act = set_back_cur->back_act;
         fram_hdr->back_act = back_act;
       }

     if (back_act->chdr == 0) ACT_Add_CHDR_To_Action(back_act,set_chdr);

	/* add set_chdr to setter action if it doesn't have one */
     if (fram_hdr->pms_act) 
     {
       if (fram_hdr->pms_act->chdr == 0)
		ACT_Add_CHDR_To_Action(fram_hdr->pms_act,set_chdr);
     }
     pms_hdr = fram_hdr->pms_hdr;
     while(pms_hdr)
     {
       ACT_MAPPED_HDR *back_map_hdr;
       back_map_hdr = (ACT_MAPPED_HDR *)back_act->data;
       pms_hdr->back = back_act;
       pms_hdr->xback = back_map_hdr->xsize;
       pms_hdr->yback = back_map_hdr->ysize;
DEBUG_LEVEL1
{
  fprintf(stderr,"---> %d %d   ",pms_hdr->xpback,pms_hdr->ypback);
  if (pms_hdr->face) fprintf(stderr,"%d %d\n",pms_hdr->xpface,pms_hdr->ypface);
  else fprintf(stderr,"\n");
}
       if (need_to_scale)
		UTIL_Scale_Buffer_Pos(&pms_hdr->xpback,&pms_hdr->ypback,
			set_imagex,set_imagey,buffx,buffy);
	/* add set_chdr to face action if it doesn't have one */
       if (pms_hdr->face)
       {
	 if (pms_hdr->face->chdr == 0)
		ACT_Add_CHDR_To_Action(pms_hdr->face,set_chdr);
         if (need_to_scale)
		UTIL_Scale_Buffer_Pos(&pms_hdr->xpface,&pms_hdr->ypface,
			set_imagex,set_imagey,buffx,buffy);
       }
DEBUG_LEVEL1
{
  fprintf(stderr,"     %d %d   ",pms_hdr->xpback,pms_hdr->ypback);
  if (pms_hdr->face) 
	fprintf(stderr,"%d %d\n",pms_hdr->xpface,pms_hdr->ypface);
  else fprintf(stderr,"\n");
}
       pms_hdr = pms_hdr->next;
     }
   } /* end of frame list loop */
   frame_cnt++; /* last frame */ 
   anim_hdr->frame_lst = (XA_FRAME *)malloc(sizeof(XA_FRAME) * frame_cnt);
   if (anim_hdr->frame_lst == NULL)
       TheEnd1("SET_ANIM: frame_lst malloc err");
   anim_hdr->frame_lst[frame_cnt-1].time_dur = 0;
   anim_hdr->frame_lst[frame_cnt-1].zztime = -1;
   anim_hdr->frame_lst[frame_cnt-1].act  = 0;

   j = 0;
   t_time = 0;
   for(i=0; i<set_fram_num; i++)
   {
     if (set_frames[i].pms_hdr != 0)
     {
       anim_hdr->frame_lst[j].time_dur = set_frames[i].time;
       anim_hdr->frame_lst[j].zztime = t_time;
       anim_hdr->frame_lst[j].act  = set_frames[i].pms_act;
       t_time += set_frames[i].time;
       j++;
     }
     else
     {
       anim_hdr->frame_lst[j].time_dur = set_frames[i].time;
       anim_hdr->frame_lst[j].zztime = t_time;
       anim_hdr->frame_lst[j].act  = set_frames[i].back_act;
       t_time += set_frames[i].time;
       j++;
     }

   }
   anim_hdr->total_time = t_time;
   anim_hdr->loop_frame = 0;
   anim_hdr->last_frame = frame_cnt-2;
   anim_hdr->imagex = set_imagex;
   anim_hdr->imagey = set_imagey;
   anim_hdr->imagec = set_imagec;
   anim_hdr->imaged = set_imaged;

 }
 SET_Free_Stuff();
 return(xaTRUE);
}

xaUBYTE *SET_Read_BACK(xin,back_hdr)
XA_INPUT *xin;
SET_BACK_HDR *back_hdr;
{
  xaULONG chid,data;
  xaLONG size,form_size;
  xaUBYTE *pic;

  pic = 0;
  chid = xin->Read_MSB_U32(xin);
  /* NOTE: for whatever reason the Anti-Lemmin' animation as a "bad" form 
   * size with the upper 16 bits set to 0xffff. By making sure the last
   * bit is clear, things should work ok anyways. We just read to the BODY.
   */
  form_size = xin->Read_MSB_U32(xin);
  form_size &= 0x7fffffff;  /* PODNOTE: Anti-Lemmin' kludge/workaround */
  data = xin->Read_MSB_U32(xin); form_size -= 4;
  DEBUG_LEVEL1 fprintf(stderr,"SET_Read_BACK: chid %x fsize %x dat %x\n", chid,form_size,data);
  if (chid != IFF_FORM) SET_TheEnd1("SET: back is not FORM\n");
  while(form_size)
  {
    chid = xin->Read_MSB_U32(xin); form_size -= 4;
    size = xin->Read_MSB_U32(xin); form_size -= 4;
    size &= 0x0000ffff;   /* PODNOTE: Anti-Lemmin' kludge/workaround */
    form_size -= size;
    if (form_size < 0) form_size = 0;

    DEBUG_LEVEL2 IFF_Print_ID(stderr,chid);
    DEBUG_LEVEL2 fprintf(stderr," size %x ",size);
    switch(chid)
    {
      case IFF_BMHD:
	IFF_Read_BMHD(xin,&bmhd);
	set_imagex = bmhd.width;  /*pageWidth;*/
	set_imagey = bmhd.height; /*pageHeight;*/
	set_imaged = bmhd.depth;
	back_hdr->xsize = bmhd.width;
	back_hdr->ysize = bmhd.height;
	back_hdr->xscreen = bmhd.width; /*pageWidth; */
	back_hdr->yscreen = bmhd.height; /*pageHeight; */
	DEBUG_LEVEL1 fprintf(stderr,"    %dx%dx%d %dx%d cmp=%d msk=%d\n",
		back_hdr->xsize,back_hdr->ysize,
	        set_imaged,back_hdr->xscreen,back_hdr->yscreen,
		bmhd.compression, bmhd.masking);
	break;
      case IFF_CMAP:
	set_imagec = size/3;
	back_hdr->csize = set_imagec;
	IFF_Read_CMAP_0(set_cmap,set_imagec,xin);
	IFF_Shift_CMAP(set_cmap,set_imagec);
	set_chdr = ACT_Get_CMAP(set_cmap,set_imagec,set_or_mask,
				set_imagec,set_or_mask,4,4,4);
	back_hdr->chdr = set_chdr;
	break;
      case IFF_CAMG: /* ignore for now */
      case IFF_CRNG: /* ignore for now */
      case IFF_DPPS: /* ignore for now */
      case IFF_DPPV: /* ignore for now */
	{
	  xaULONG d;
          while( (size--) && !xin->At_EOF(xin) ) d = xin->Read_U8(xin);
	}
        break;
      case IFF_BODY:
	{
	  xaULONG xsize,ysize;
	  xsize = bmhd.width;
	  ysize = bmhd.height;
	  pic = (xaUBYTE *) malloc( XA_PIC_SIZE(xsize * ysize) );
	  if (pic == 0) SET_TheEnd1("SET_Read_BODY: malloc failed\n");
	  IFF_Read_BODY(xin,pic,size, xsize, ysize, (xaULONG)(bmhd.depth),
		(int)(bmhd.compression), (int)(bmhd.masking),set_or_mask);
	  form_size = 0; /* body is always last */
	  break;
	}
      default: 
	SET_Print_CHID(chid); SET_TheEnd1("\nUnknown in Back chunk\n");
	break;
    } /* end of switch */
  } /* end of while */
  return(pic);
}


void SET_Read_SSET(anim_hdr,xin,set_hdr)
XA_ANIM_HDR *anim_hdr;
XA_INPUT *xin;
SET_SSET_HDR *set_hdr;
{
  xaULONG chid,data;
  xaLONG list_num,type,i;

  chid = xin->Read_MSB_U32(xin);
  list_num = xin->Read_MSB_U32(xin) + 1;
  data = xin->Read_MSB_U32(xin);
  set_hdr->face_num = list_num;
  DEBUG_LEVEL1 
  {
    fprintf(stderr,"    ");
    SET_Print_CHID(chid);
    fprintf(stderr,"%x ",list_num);
    SET_Print_CHID(data);
    fprintf(stderr,"\n");
  }
  if (chid != IFF_LIST) SET_TheEnd1("SET: LIST not 1st in SSET\n");

  set_hdr->faces = (SET_FACE_HDR *)malloc(list_num * sizeof(SET_FACE_HDR) );
  if (set_hdr->faces == 0) SET_TheEnd1("SET: faces malloc failed\n");

  /* Read PROP */
  chid = xin->Read_MSB_U32(xin);  /* PROP */
  DEBUG_LEVEL1 fprintf(stderr,"    ");
  DEBUG_LEVEL1 SET_Print_CHID(chid);
  if (chid != IFF_PROP) SET_TheEnd1("SET: 1st not PROP in SET\n");
  data = xin->Read_MSB_U32(xin);  /* prop size */
  type = xin->Read_MSB_U32(xin);  /* prop type */
  DEBUG_LEVEL1 fprintf(stderr,"%x ",data);
  DEBUG_LEVEL1 SET_Print_CHID(type);

  /* Read CMAP */
  chid = xin->Read_MSB_U32(xin);  /* CMAP */
  DEBUG_LEVEL1 SET_Print_CHID(chid);
  if (chid != IFF_CMAP) SET_TheEnd1("SET: 2st not CMAP in SET\n");
  set_imagec = xin->Read_MSB_U32(xin);  /* CMAP size */
  set_imagec /= 2;
  DEBUG_LEVEL1 fprintf(stderr," %x\n",set_imagec);
  IFF_Read_CMAP_1(set_cmap,set_imagec,xin);
  IFF_Shift_CMAP(set_cmap,set_imagec);
  set_chdr = ACT_Get_CMAP(set_cmap,set_imagec,set_or_mask,
				set_imagec,set_or_mask,4,4,4);
  
  for(i = 1; i < list_num; i++)
  {
    SET_FACE_HDR *face;
    xaUBYTE *pic;
    XA_ACTION *act;

    face = &set_hdr->faces[i];
    pic = SET_Read_FACE(xin,face);
    if (pic==0) SET_TheEnd1("SET_Read_SSET: read face failed\n");

    act = ACT_Get_Action(anim_hdr,ACT_MAPPED);
    ACT_Setup_Mapped(act,pic,set_chdr,
	face->x,face->y,face->xsize,face->ysize,
	set_imagex,set_imagey,xaTRUE,set_or_mask,xaTRUE,xaFALSE,xaFALSE);
    ACT_Add_CHDR_To_Action(act,set_chdr);
    face->face_act = act;
  }
}


xaUBYTE *SET_Read_FACE(xin,face_hdr)
XA_INPUT *xin;
SET_FACE_HDR *face_hdr;
{
  xaULONG chid,data;
  xaLONG size,form_size;
  xaUBYTE *pic;

  pic = 0;
  face_hdr->xsize = 0;
  face_hdr->ysize = 0;
  chid = xin->Read_MSB_U32(xin);
  form_size = xin->Read_MSB_U32(xin);
  data = xin->Read_MSB_U32(xin); form_size -= 4;
  DEBUG_LEVEL2 fprintf(stderr,"    ");
  DEBUG_LEVEL2 SET_Print_CHID(chid);
  DEBUG_LEVEL2 fprintf(stderr,"%x ",form_size);
  DEBUG_LEVEL2 SET_Print_CHID(data);
  if (chid != IFF_FORM) SET_TheEnd1("SET: face is not FORM\n");
  while(form_size)
  {
    chid = xin->Read_MSB_U32(xin); form_size -= 4;
    size = xin->Read_MSB_U32(xin); form_size -= 4;
    DEBUG_LEVEL2 SET_Print_CHID(chid);
    DEBUG_LEVEL2 fprintf(stderr,"%x ",size);
    form_size -= size;
    if (form_size < 0) form_size = 0;

    switch(chid)
    {
      case IFF_FACE:
	face_hdr->xsize = xin->Read_MSB_U16(xin);
	face_hdr->ysize = xin->Read_MSB_U16(xin);
	face_hdr->x     = xin->Read_MSB_U16(xin);
	face_hdr->y     = xin->Read_MSB_U16(xin);
	face_hdr->xoff  = xin->Read_MSB_U16(xin);
	face_hdr->yoff  = xin->Read_MSB_U16(xin);
	DEBUG_LEVEL2 
	{
  fprintf(stderr,"      FACE size <%d,%d> p <%d,%d> off <%d,%d>\n",
	  (xaLONG)face_hdr->xsize,(xaLONG)face_hdr->ysize,
	  (xaLONG)face_hdr->x,    (xaLONG)face_hdr->y,
	  (xaLONG)face_hdr->xoff, (xaLONG)face_hdr->yoff );
	}
	break;
      case IFF_BODY:
	{
	  pic = (xaUBYTE *) 
		malloc( XA_PIC_SIZE(face_hdr->xsize * face_hdr->ysize) );
	  if (pic == 0) SET_TheEnd1("SET_Read_BODY: malloc failed\n");
          DEBUG_LEVEL2 fprintf(stderr,"   %dx%dx%d comp=%d msk=%d",
                face_hdr->xsize,face_hdr->ysize,set_imaged,bmhd.compression,
		bmhd.masking);

	  IFF_Read_BODY(xin,pic,size,
		face_hdr->xsize, face_hdr->ysize, 5,
		(xaULONG)(bmhd.compression), (xaULONG)(bmhd.masking),set_or_mask);
	  form_size = 0; /* body is always last */
	  break;
	}
      default: 
	SET_TheEnd1("\nUnknown in Back chunk\n");
	break;
    } /* end of switch */
  } /* end of while */
 DEBUG_LEVEL2 fprintf(stderr,"\n");
 return(pic);
}


void SET_Free_Stuff()
{
  while(set_back_cur != 0)
  {
    SET_BACK_HDR *tmp;
    tmp = set_back_cur;
    set_back_cur = set_back_cur->next;
    FREE(tmp,0x7000);
  }
  while(set_sset_cur != 0)
  {
    SET_SSET_HDR *tmp;
    xaULONG num;
    num = set_sset_cur->face_num;
    if (set_sset_cur->faces != 0) 
	{ FREE(set_sset_cur->faces,0x7000); set_sset_cur->faces=0;}
    tmp = set_sset_cur;
    set_sset_cur = set_sset_cur->next;
    FREE(tmp,0x7000);
  }
}

void SET_TheEnd1(mess)
xaUBYTE *mess;
{
  SET_Free_Stuff();
  TheEnd1(mess);
}

void SET_TheEnd()
{
  SET_Free_Stuff();
  TheEnd();
}

void SET_Read_Sound_IFF(xin)
XA_INPUT *xin;
{
  xaULONG chid,data;
  xaLONG size,form_size;

  chid = xin->Read_MSB_U32(xin);
  form_size = xin->Read_MSB_U32(xin);
  DEBUG_LEVEL2 fprintf(stderr,"    FORM %x ",form_size);
  data = xin->Read_MSB_U32(xin); form_size -= 4;
  DEBUG_LEVEL2 SET_Print_CHID(data);
  if (chid != IFF_FORM) SET_TheEnd1("SET: sound is not FORM\n");
  while(form_size)
  {
    xaULONG d;
    chid = xin->Read_MSB_U32(xin); form_size -= 4;
    size = xin->Read_MSB_U32(xin); form_size -= 4;
    DEBUG_LEVEL2 SET_Print_CHID(data);
    DEBUG_LEVEL2 fprintf(stderr," %x ",size);
    form_size -= size;
    if (form_size < 0) form_size = 0;
    switch(chid)
    {
      case IFF_BODY:
	form_size = 0; /* last chunk */
      case IFF_VHDR:
      case IFF_ANNO:
      case IFF_CHAN:
	while( (size--) && !xin->At_EOF(xin) ) d = xin->Read_U8(xin);
	break;
      default: 
	SET_Print_CHID(chid); SET_TheEnd1("\nUnknown in SOUND chunk\n");
	break;
    } /* end of switch */
  } /* end of while */
 DEBUG_LEVEL2 fprintf(stderr,"\n");
}

SET_FRAM_HDR *SET_Init_FRAM_HDRS(num)
xaULONG num;
{
  SET_FRAM_HDR *tmp_fram;
  xaULONG i;

  tmp_fram = (SET_FRAM_HDR *)malloc(num * sizeof(SET_FRAM_HDR) );
  if (tmp_fram == 0) SET_TheEnd1("SET_Init_Fram_HDR: malloc failed");

  for(i=0; i<num; i++)
  {
    tmp_fram[i].time     = 0xffffffff;
    tmp_fram[i].cm_act   = 0;
    tmp_fram[i].pms_act  = 0;
    tmp_fram[i].back_act = 0;
    tmp_fram[i].cm_hdr   = 0;
    tmp_fram[i].pms_hdr  = 0;
  }
  return(tmp_fram);
}

  
void SET_Add_CHDR(chdr,fram_num)
XA_CHDR *chdr;
xaULONG fram_num;
{
  SET_FRAM_HDR *fram_hdr;

  if (fram_num > set_fram_num)
  {
    fprintf(stderr,"SET_Add_CHDR: invalid frame %x\n", fram_num);
    return;
  }
  fram_hdr = &set_frames[fram_num];
  if (fram_hdr->cm_hdr != 0)
  {
    DEBUG_LEVEL2 fprintf(stderr,"SET_Add_CHDR: duplicate cmap warning\n");
  }
  fram_hdr->cm_hdr = chdr;
}

void SET_Add_BACK(fram_num,back_act,chdr)
xaULONG fram_num;
XA_ACTION *back_act;
XA_CHDR *chdr;
{
  SET_FRAM_HDR *fram_hdr;

  if (fram_num > set_fram_num)
  {
    fprintf(stderr,"SET_Add_BACK: invalid frame %x\n", fram_num);
    return;
  }
  fram_hdr = &set_frames[fram_num];
  if (fram_hdr->back_act != 0)
    fprintf(stderr,"SET_Add_BACK: duplicate back err\n");
  fram_hdr->back_act = back_act;

      /* if no CMAP for this frame add the back's */
  if (fram_hdr->cm_hdr == 0) SET_Add_CHDR(chdr,fram_num);

}

void SET_Add_TIME(fram_num,fram_time)
xaULONG fram_num,fram_time;
{
  SET_FRAM_HDR *fram_hdr;
  if (fram_num > set_fram_num) 
  {
    fprintf(stderr,"SET_Add_TIME: invalid fram_num %x\n",fram_num);
    return;
  }
  fram_hdr = &set_frames[fram_num]; 
  fram_hdr->time = set_time;
}


/*
 *
 */
void SET_Add_SETTER(anim_hdr,fram_num,xpback,ypback,
		xpface,ypface,face_act,depth)
XA_ANIM_HDR *anim_hdr;
xaULONG fram_num;
xaULONG xpback,ypback;
xaULONG xpface,ypface;
XA_ACTION *face_act;
xaULONG depth;
{
  ACT_MAPPED_HDR *face_map_hdr;
  SET_FRAM_HDR *fram_hdr;
  ACT_SETTER_HDR *tmp_pms,*cur_pms;
  XA_ACTION *act;

  if (fram_num > set_fram_num) 
  {
    fprintf(stderr,"SET_Add_SETTER: invalid fram_num %x\n",fram_num);
    return;
  }
  fram_hdr = &set_frames[fram_num]; 
  face_map_hdr = (ACT_MAPPED_HDR *) face_act->data;

  act = ACT_Get_Action(anim_hdr,ACT_SETTER);


  tmp_pms = (ACT_SETTER_HDR *)malloc(sizeof(ACT_SETTER_HDR));
  if (tmp_pms == 0) SET_TheEnd1("SET_Add_SETTER: malloc failed");

  act->data = (xaUBYTE *)tmp_pms;

  tmp_pms->work = work_act;
  tmp_pms->xpback = xpback;
  tmp_pms->ypback = ypback;
  tmp_pms->xface = face_map_hdr->xsize;
  tmp_pms->yface = face_map_hdr->ysize;
  tmp_pms->xpface = xpface;
  tmp_pms->ypface = ypface;
  tmp_pms->depth = depth;
  tmp_pms->face = face_act;
  tmp_pms->back = 0;
  tmp_pms->xback = 0;
  tmp_pms->yback = 0;

   /* if this is the 1st one. */
  if (fram_hdr->pms_hdr == 0)  /* 1st one */
  {
    fram_hdr->pms_hdr = tmp_pms;
    tmp_pms->next = 0;
    fram_hdr->pms_act = act;
    return;
  }
  /* if 1st one is of less or equal depth */
  if (fram_hdr->pms_hdr->depth <= depth)
  {
    tmp_pms->next = fram_hdr->pms_hdr;
    fram_hdr->pms_hdr = tmp_pms;
    fram_hdr->pms_act = act;  /* keep act current */
    return;
  }

  cur_pms = fram_hdr->pms_hdr;
  while(cur_pms != 0)
  {
    /*  there isn't a next one */
    if (cur_pms->next == 0)
    {
      tmp_pms->next = 0;
      cur_pms->next = tmp_pms;
      return;
    }
     /* if the next one's depth is <= then put here */
    if (cur_pms->next->depth <= depth)
    {
      tmp_pms->next = cur_pms->next;
      cur_pms->next = tmp_pms;
      return;
    }
    cur_pms = cur_pms->next;
  }

}

void SET_Extract_Directory(dname,cname)
xaUBYTE *dname,*cname;
{
 xaULONG i,j;

 i = 0;
 while( (dname[i] != ':') && (i < 256) && (dname[i] != 0) ) i++;
 if ((dname[i]==0) || (i >= 256)) SET_TheEnd1("SET_Extract: error 1\n");

 i++; /* skip over : */
 j = 0;
 while( (dname[i] != 0) && (i < 256) )
 {
   dname[j] = dname[i]; i++; j++;
 }
 if (i >= 256) SET_TheEnd1("SET_Extract: error 2\n");
 dname[j] = '/'; j++;

 i = 0;
 while( (cname[i] != 0) && (i<32) ) 
 {
   dname[j] = cname[i]; i++; j++;
 }
 if (i >= 32) SET_TheEnd1("SET_Extract: error 3\n");
 dname[j] = 0;
}

void SET_Modify_Directory(dname,flag)
char dname[];
xaULONG flag;
{
  int d = (int)dname[0];
  if (flag == 0)  /* flip upper/lower on 1st char */
  {
    if (isupper(d) ) dname[0] = (char)tolower(d);
    else dname[0] = (char)toupper(d);
  }
}

