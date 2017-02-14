
/*
 * xa_pfx.c
 *
 * Copyright (C) 1992,1993,1994,1995 by Mark Podlipec.
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
#include "xa_pfx.h"
#include "xa_iff.h"

xaLONG Is_PFX_File();
xaULONG PFX_Read_File();
void PFX_Setup_CMAP();
void PFX_Extract_Images();
xaULONG UTIL_Get_MSB_Long();
xaULONG UTIL_Get_MSB_UShort();
XA_CHDR *ACT_Get_CMAP();
XA_ACTION *ACT_Get_Action();
void ACT_Setup_Mapped();
void ACT_Add_CHDR_To_Action();


static PFX_PLAY_HDR *pfx_play_stack,*pfx_play_start,*pfx_play_lcur;

static XA_CHDR *pfx_chdr;
static ColorReg pfx_cmap[256];

#define PFX_DEFAULT_TIME 17
static xaLONG pfx_time;
static PFX_RCHG_HDR *pfx_images;

static xaULONG pfx_imagex,pfx_imagey,pfx_imagec,pfx_imaged;

/*
 *
 */
xaLONG Is_PFX_File(filename)
char *filename;
{
  FILE *fin;
  xaULONG data;

  if ( (fin=fopen(filename,XA_OPEN_MODE)) == 0) return(xaNOFILE);

  data = UTIL_Get_MSB_Long(fin);  /* read past size */
  data = UTIL_Get_MSB_Long(fin); /* read magic */
  fclose(fin);
  if (data == pfx_RCHG) return(xaTRUE);
  return(xaFALSE);
}

void PFX_PUSH_PLAY_HDR(p)
PFX_PLAY_HDR *p;
{
  PFX_PLAY_HDR *temp;

  temp = (PFX_PLAY_HDR *) malloc(sizeof(PFX_PLAY_HDR));
  if (temp == 0) TheEnd1("PFX_PUSH_PLAY_HDR: malloc failed\n");
  temp->type = p->type;
  temp->data = p->data;
  temp->count = 0;
  temp->next = 0;
  temp->loop = 0;
  if (pfx_play_start == 0) pfx_play_start = temp;
  else pfx_play_lcur->next = temp;
  pfx_play_lcur = temp;

 DEBUG_LEVEL2 fprintf(stderr,"PUSHING %c%c%c%c", ((p->type >> 24) & 0xff),
    ((p->type >> 16) & 0xff), ((p->type >> 8) & 0xff), ((p->type) & 0xff) );
 DEBUG_LEVEL2 fprintf(stderr," data=%lx ptr=%lx\n",
					p->data,(xaULONG)pfx_play_lcur);

	/* stack knows where it's duplicate is on play list */
  p->loop = pfx_play_lcur;  
  p->next = pfx_play_stack;
  pfx_play_stack = p;
}

PFX_PLAY_HDR *PFX_POP_PLAY_HDR()
{
  PFX_PLAY_HDR *p;
  p = pfx_play_stack;
  if (p != 0) pfx_play_stack = pfx_play_stack->next;
  return(p);
}

PFX_RCHG_HDR *PFX_Alloc_RCHG()
{
  PFX_RCHG_HDR *temp;

  temp = (PFX_RCHG_HDR *)malloc(sizeof(PFX_RCHG_HDR));
  if (temp == 0) TheEnd1("PFX_Alloc_RCHG malloc error\n");
  temp->orig = 0;
  temp->dest = 0;
  temp->len  = 0;
  temp->data = 0;
  temp->next = 0;
  return(temp);
}

PFX_FRAM_HDR *PFX_Alloc_FRAM()
{
  PFX_FRAM_HDR *temp;
  temp = (PFX_FRAM_HDR *)malloc(sizeof(PFX_FRAM_HDR));
  if (temp == 0) TheEnd1("PFX_Alloc_FRAM malloc error\n");
  temp->frame = 0;
  temp->time  = 0;
  temp->next = 0;
  return(temp);
}


xaULONG PFX_Read_File(fname,anim_hdr)
char *fname;
XA_ANIM_HDR *anim_hdr;
{
  FILE *fin;
  PFX_RCHG_HDR  *first_rchg,*cur_rchg;
  PFX_RCHG_HDR  *first_parm,*cur_parm;
  PFX_FRAM_HDR *first_fram,*cur_fram;
  xaULONG pfx_max_rchg;
  xaULONG pfx_xflag,pfx_yflag,pfx_cflag,pfx_rchg_flag;
  xaULONG d,chid,chlen;
  xaULONG frame_count;
  PFX_PLAY_HDR *pfx_play_temp;


  pfx_play_stack = 0;
  pfx_play_start = 0;
  pfx_play_lcur  = 0;

  pfx_chdr = 0;
  pfx_time = PFX_DEFAULT_TIME;
  pfx_images = 0;
  pfx_imagex = 0;
  pfx_imagey = 0;
  pfx_imagec = 0;
  pfx_imaged = 0;

  first_rchg = cur_rchg = 0;
  first_parm = cur_parm = 0;
  pfx_max_rchg = 0;
  pfx_xflag = xaFALSE;
  pfx_yflag = xaFALSE;
  pfx_cflag = xaFALSE;

  if ( (fin=fopen(fname,XA_OPEN_MODE)) == 0)
  {
    fprintf(stderr,"Can't open PFX File %s for reading\n",fname);
    return(xaFALSE);
  }

  d = UTIL_Get_MSB_Long(fin);   /* probably version number */
  chid = UTIL_Get_MSB_Long(fin); /* get chunk id */
  chlen = UTIL_Get_MSB_Long(fin); /* get chunk length */
  while(chid == pfx_RCHG)
  {
    xaLONG ret;
    xaULONG orig,dest,garb;
    PFX_RCHG_HDR  *tmp_rchg;
    xaUBYTE *data;

    orig = UTIL_Get_MSB_UShort(fin);
    dest = UTIL_Get_MSB_UShort(fin);
    garb = UTIL_Get_MSB_Long(fin);  /* orig ptr or id or ?? */
    garb = UTIL_Get_MSB_Long(fin);  /* dest ptr or id or ?? */
    garb = UTIL_Get_MSB_UShort(fin); /* always 0xffff */
    DEBUG_LEVEL1 fprintf(stderr,"RCHG %lx  <%ld %ld>\n",chlen,orig,dest);
    chlen -= 14;

    data = (xaUBYTE *)malloc(chlen);
    if (data == 0) TheEnd1("PFX: rchg data malloc failed\n");
    ret = fread((char *)(data),1,chlen,fin);
    if (ret != chlen) TheEnd1("PFX: rchg data read failed\n");

    tmp_rchg = PFX_Alloc_RCHG();
    tmp_rchg->orig = orig;    
    tmp_rchg->dest = dest;    
    tmp_rchg->len = chlen;    
    tmp_rchg->data = data;

    if (first_rchg == 0) first_rchg = cur_rchg = tmp_rchg;
    else
    {
      cur_rchg->next = tmp_rchg;
      cur_rchg = tmp_rchg;
    }
    if (orig > pfx_max_rchg) pfx_max_rchg = orig;
    if (dest > pfx_max_rchg) pfx_max_rchg = dest;

    chid = UTIL_Get_MSB_Long(fin); /* get chunk id */
    chlen = UTIL_Get_MSB_Long(fin); /* get chunk length */
  }

  while(chid == pfx_PARM)
  {
    xaLONG ret;
    xaULONG pnum,res1,res2;
    PFX_RCHG_HDR  *tmp_parm;
    xaUBYTE *data;

    pnum = UTIL_Get_MSB_UShort(fin);  /* parm number */
    res1 = UTIL_Get_MSB_UShort(fin);  /* ??? */
    res2 = UTIL_Get_MSB_UShort(fin);   /* ??? */
    chlen -= 6;

    data = (xaUBYTE *)malloc(chlen);
    if (data == 0) TheEnd1("PFX: parm data malloc failed\n");
    ret = fread((char *)(data),1,chlen,fin);
    if (ret != chlen) TheEnd1("PFX: parm data read failed\n");

    DEBUG_LEVEL1 fprintf(stderr,"PARM %lx\n",chlen);
    DEBUG_LEVEL2 fprintf(stderr,"     %s\n",data);
     
    tmp_parm = PFX_Alloc_RCHG();
    tmp_parm->orig = pnum;
    tmp_parm->dest = res1;
    tmp_parm->len = chlen;
    tmp_parm->data = data;

    if (first_parm == 0) first_parm = cur_parm = tmp_parm;
    else
    {
      cur_parm->next = tmp_parm;
      cur_parm = tmp_parm;
    }

    chid = UTIL_Get_MSB_Long(fin); /* get chunk id */
    chlen = UTIL_Get_MSB_Long(fin); /* get chunk length */
  }

  if (chid != pfx_STMT)
  { 
    fprintf(stderr,"PFX: confused\n");
    return(xaFALSE);
  }


  first_fram = cur_fram = 0;
  frame_count = 0;

  pfx_rchg_flag = xaFALSE;
  while(chlen && !feof(fin) )
  {
    chid = UTIL_Get_MSB_Long(fin);  chlen-=4; /* get script id */

    if (chid == pfx_A_B)
    {
      PFX_PLAY_HDR *p;
      p = PFX_POP_PLAY_HDR();
      if (p == 0) TheEnd1("PFX Play List: stack empty err1\n");
      if (pfx_play_temp == 0) TheEnd1("PFX Play List: stack empty err2\n");
      DEBUG_LEVEL1 fprintf(stderr,"POP %c%c%c%c\n", 
	((p->type >> 24) & 0xff), ((p->type >> 16) & 0xff), 
	((p->type >> 8) & 0xff), ((p->type) & 0xff) );
      if (p->type == pfx_REPE) /* point back to just after REPE */
      {
        if (p->loop->next != 0) pfx_play_temp->loop = p->loop->next;
        else TheEnd1("PFX_Loop: hosed\n");
        DEBUG_LEVEL1 fprintf(stderr,"   loop to %c%c%c%c", 
	 ((p->loop->next->type >> 24) & 0xff),
	 ((p->loop->next->type >> 16) & 0xff), 
	 ((p->loop->next->type >> 8) & 0xff),
	 ((p->loop->next->type) & 0xff) );
        DEBUG_LEVEL1 fprintf(stderr,"   count=%lx\n",p->loop->next->data);
      }
      FREE(p,0x6000); p=0;
    }
    else
    {
      pfx_play_temp = (PFX_PLAY_HDR *)malloc(sizeof(PFX_PLAY_HDR));
      if (pfx_play_temp == 0) TheEnd1("PFX: malloc PFX_PLAY_HDR error\n");
      pfx_play_temp->type = chid;
      pfx_play_temp->data = 0;
      pfx_play_temp->loop = 0;
      pfx_play_temp->next = 0;
      switch(chid)
      {
	case pfx_INIT:
	case pfx_GLOC:
	case pfx_GLOV:
	case pfx_REPE:
		PFX_PUSH_PLAY_HDR(pfx_play_temp);
		break;
	case pfx_BITP:
	case pfx_XDIM:
	case pfx_YDIM:
	case pfx_DURA:
	case pfx_HIGH:
	case pfx_FLAC:
	case pfx_COUN:
	case pfx_INTE:
	case pfx_AUST:
	case pfx_SPDU:
	case pfx_SLWD:
		pfx_play_temp->data = UTIL_Get_MSB_Long(fin); chlen-=4;
		PFX_PUSH_PLAY_HDR(pfx_play_temp);
		break;
	case pfx_PALL:
	case pfx_FRAM:
	case pfx_FILE:
		pfx_play_temp->data = UTIL_Get_MSB_UShort(fin); chlen-=2;
		PFX_PUSH_PLAY_HDR(pfx_play_temp);
		break;
	default:
		fprintf(stdout,"PFX read: Unknown STMT chid %lx\n",chid);
		return(xaFALSE);
		break;
      }
    }
  } /* end of while for reading play list */

  fclose(fin);
  /* free up Play List stack just in case it's not already freed up */
  {
    PFX_PLAY_HDR *p;
    do
    {
      p = PFX_POP_PLAY_HDR();
      if (p != 0) 
      {
        FREE(p,0x6000); p=0;
        fprintf(stderr,"PFX Warning: play list different than expected\n");
      }
    } while(p != 0);
  }


/* loop through play list to create frame list */
  pfx_play_lcur = pfx_play_start;
  while(pfx_play_lcur != 0)
  {
   xaULONG garb;

    chid = pfx_play_lcur->type;
    switch(chid)
    {
      case pfx_INIT:
      case pfx_GLOC:
      case pfx_GLOV:
      case pfx_REPE: /* REPEAT: for now must be followed by COUN */
      case pfx_FILE: /* NOTE: this is comment only */
		break;
      case pfx_BITP: /* Num of Bitplanes */
		pfx_imaged = pfx_play_lcur->data;
		break;
      case pfx_XDIM: /* X size */
		pfx_imagex = pfx_play_lcur->data;
		pfx_xflag = xaTRUE;
		break;
      case pfx_YDIM: /* Y size */
		pfx_imagey = pfx_play_lcur->data;
		pfx_yflag = xaTRUE;
		break;
      case pfx_HIGH: /* ??? */
      case pfx_FLAC: /* ??? */
      case pfx_INTE: /* Interlace flag */
      case pfx_AUST: /* don't know */
		break;
      case pfx_DURA: /* ?Duration between frames */
		garb = pfx_play_lcur->data; if (garb == 0) garb = 1;
		pfx_time = garb * PFX_DEFAULT_TIME;
		break;
      case pfx_SPDU: /* speed up by this much */
		garb = pfx_play_lcur->data;
		pfx_time += garb * PFX_DEFAULT_TIME;
		break;
      case pfx_SLWD: /* slow down by this much */
		garb = pfx_play_lcur->data;
		pfx_time -= garb * PFX_DEFAULT_TIME;
		if (pfx_time <= 0) pfx_time = 1;
		break;
      case pfx_PALL: /* CMAP parm id */
		garb = pfx_play_lcur->data;
		PFX_Setup_CMAP(garb,first_parm);
		pfx_cflag = xaTRUE;
		break;
      case pfx_COUN:
		garb = pfx_play_lcur->data;
		    /* 0x3b9ac9ff for example is mouse button */
		if (garb >= 0x100) garb = 1;
		pfx_play_lcur->count = garb;
		break;
      case pfx_FRAM:
		garb = pfx_play_lcur->data;
		if (first_fram == 0)
		  first_fram = cur_fram = PFX_Alloc_FRAM();
		else
		{
		  cur_fram->next = PFX_Alloc_FRAM();
		  cur_fram = cur_fram->next;
		}
		cur_fram->frame = garb;
		cur_fram->time  = XA_GET_TIME(pfx_time);
		frame_count++;
		break;
      default: 
		fprintf(stdout,"PFX frame lst: Unknown STMT chid %lx\n",chid);
		return(xaFALSE);
		break;
     } /* end switch */
     if (   (pfx_xflag==xaTRUE) && (pfx_yflag==xaTRUE) 
         && (pfx_cflag==xaTRUE) && (pfx_rchg_flag==xaFALSE))
     {
       pfx_rchg_flag = xaTRUE;
       PFX_Extract_Images(anim_hdr,first_rchg,pfx_max_rchg);
     }
     if (pfx_play_lcur->loop != 0) 
     { 
       PFX_PLAY_HDR *p;
       p = pfx_play_lcur->loop;
       DEBUG_LEVEL2 fprintf(stderr,"PFX frame loop cnt=%lx\n",p->count);
       p->count--;
       if (p->count > 0) pfx_play_lcur = p; /* goto COUN */
     } 

     pfx_play_lcur = pfx_play_lcur->next;
   } /* end of while STMT */

   /* Free up play list */
   { 
     PFX_PLAY_HDR *p;
     while(pfx_play_start) 
     {
       p = pfx_play_start;
       pfx_play_start = pfx_play_start->next;
       FREE(p,0x6000); p=0;
     }
   }

 {
   PFX_FRAM_HDR *ptr_fram,*tmp_fram;
   xaULONG i;

   anim_hdr->frame_lst = 
	(XA_FRAME *)malloc(sizeof(XA_FRAME) * (frame_count + 1));
   if (anim_hdr->frame_lst == NULL)
        TheEnd1("PFX_ANIM: couldn't malloc for frame_lst\0");

   DEBUG_LEVEL3 fprintf(stderr,"PFX frame List(%ld): ",frame_count);
   ptr_fram = first_fram;
   for(i=0; i<frame_count; i++)
   {  /* frames start at 1, nothing is at 0 */
     anim_hdr->frame_lst[i].act  = pfx_images[ptr_fram->frame].act;
     anim_hdr->frame_lst[i].time = ptr_fram->time;
     anim_hdr->frame_lst[i].timelo = 0;

     DEBUG_LEVEL3 fprintf(stderr,"<%ld>",ptr_fram->frame);
     tmp_fram = ptr_fram;
     ptr_fram = ptr_fram->next;
     FREE(tmp_fram,0x6000);
   }
   if (ptr_fram != 0) 
     fprintf(stderr," FRAME stuff: ptr_fram %lx non_zero\n",(xaULONG)ptr_fram);
   DEBUG_LEVEL3 fprintf(stderr,"\n");
   anim_hdr->frame_lst[frame_count].time = 0;
   anim_hdr->frame_lst[frame_count].timelo = 0;
   anim_hdr->frame_lst[frame_count].act  = 0;
   anim_hdr->loop_frame = 0;
   anim_hdr->last_frame = frame_count - 1;
   anim_hdr->imagex = pfx_imagex;
   anim_hdr->imagey = pfx_imagey;
   anim_hdr->imagec = pfx_imagec;
   anim_hdr->imaged = pfx_imaged;
 }

 { /* FREE PARM STRUCTURES */
   PFX_RCHG_HDR *tmp,*tmp1;

   if (pfx_images != 0) FREE(pfx_images,0x6000); /* free up image structures */
   pfx_images=0;
   tmp = first_parm;
   while(tmp) 
   { 
     tmp1 = tmp; 
     tmp = tmp->next;
     FREE(tmp1->data,0x6000);
     FREE(tmp1,0x6000);
   }
   first_parm=0;
 }
 return(xaTRUE);
}

 
void PFX_Extract_Images(anim_hdr,first_rchg,pfx_max_rchg)
XA_ANIM_HDR *anim_hdr;
PFX_RCHG_HDR *first_rchg;
xaULONG pfx_max_rchg;
{
  xaULONG psize,i;
  xaUBYTE *data,*pic;
  PFX_RCHG_HDR *tmp_rchg;

  psize = pfx_imagex * pfx_imagey;

  pfx_max_rchg++;  /* add 0 up front */
  pfx_images = (PFX_RCHG_HDR *)malloc(pfx_max_rchg * sizeof(PFX_RCHG_HDR));
  if (pfx_images == 0) TheEnd1("PFX: malloc failed 1\n");
  for(i=0; i<pfx_max_rchg;i++) 
  {
    pfx_images[i].orig = 0; 
    pfx_images[i].data = 0;
  }
  pfx_images[0].orig = 1; /* indicate it is used */ 

  tmp_rchg = first_rchg;
  while(tmp_rchg)
  {
    xaULONG orig,dest,len;
    xaULONG b_cnt,a_cnt,offset,d,dmask;
    PFX_RCHG_HDR *tmp1_rchg;

    orig = tmp_rchg->orig;
    dest = tmp_rchg->dest;
    len  = tmp_rchg->len;
    data = tmp_rchg->data;
    DEBUG_LEVEL2 fprintf(stderr,"PFX Extracting %ld %ld\n",orig,dest);


	/* new image */
    if ( (pfx_images[orig].orig == 0) || (pfx_images[dest].orig == 0 ) )
    {
      pic = (xaUBYTE *)malloc( XA_PIC_SIZE(psize) );
      if (pic == 0) TheEnd1("PFX: malloc failed 2\n");

      if (orig == 0) memset((char *)(pic),0x00,psize);
      else
      {
        if (pfx_images[orig].orig == 0) 
        { 
          i = orig; orig = dest; dest = i;
        }
	if (pfx_images[orig].orig == 0)
        { 
	  fprintf(stderr,"PFX orig doesn't exist yet. faking it.\n");
	  memset((char *)(pic),0x00,psize);
        }
	else memcpy((char *)(pic),(char *)(pfx_images[orig].data),psize);
      } 

      dmask = 0x01;
      while(len)
      {
        xaUBYTE *ptr;
        b_cnt = (xaULONG)(*data++) << 8;   b_cnt |= (xaULONG)(*data++);  len-=2;
        DEBUG_LEVEL3 fprintf(stderr,"b_cnt = %ld\n",b_cnt);
        while(len && b_cnt)
        {
          offset = (xaULONG)(*data++) << 8; offset |= (xaULONG)(*data++);  len-=2;
          a_cnt = (xaULONG)(*data++) << 8;  a_cnt |= (xaULONG)(*data++);  len-=2;
          DEBUG_LEVEL4 fprintf(stderr,"<%lx %lx> ",offset,a_cnt);
          ptr = (xaUBYTE *)(pic + (offset * 8));
	  for(i = 0; i <= a_cnt; i++)
	  {
            d = (xaULONG)(*data++) << 8;  d |= (xaULONG)(*data++);  len-=2;
	    IFF_Short_Mod(ptr,d,dmask,1);
            ptr += pfx_imagex;
          }
          b_cnt--;
        }
        DEBUG_LEVEL4 fprintf(stderr,"\n\n");
        dmask <<= 1;  /* move to next bitplane */
      }
      if (b_cnt != 0) fprintf(stderr,"b_cnt nonzero\n");

      pfx_images[dest].orig = dest;
      pfx_images[dest].data = pic;
      DEBUG_LEVEL2 fprintf(stderr,"New Image pfx_images[deset] = %ld\n",dest);
    } /* new image */

    tmp1_rchg = tmp_rchg;
    tmp_rchg = tmp_rchg->next; /* move to next image */
    FREE(tmp1_rchg->data,0x6000); tmp1_rchg->data=0;
    FREE(tmp1_rchg,0x6000); tmp1_rchg=0;

  } /* end of while rchg_hdr's */

  for(i=1; i<pfx_max_rchg; i++)
  {
    XA_ACTION *act;
    
    if (pfx_images[i].data == 0) fprintf(stderr,"ERROR: im not completed\n");
    act = ACT_Get_Action(anim_hdr,ACT_MAPPED);
    pfx_images[i].act = act;
    ACT_Setup_Mapped(act, pfx_images[i].data,
	pfx_chdr,0,0,pfx_imagex,pfx_imagey,pfx_imagex,pfx_imagey,xaFALSE,0,
	xaTRUE,xaTRUE,xaFALSE);
    ACT_Add_CHDR_To_Action(act,pfx_chdr);
  }
} /* end of function */

       
void PFX_Setup_CMAP(cmap_id,first_parm)
xaULONG cmap_id;
PFX_RCHG_HDR *first_parm;
{
  xaULONG found_flag,i;
  xaUBYTE *ptr;
  PFX_RCHG_HDR *tmp_parm;

  found_flag = xaFALSE;
  tmp_parm = first_parm;

  while(tmp_parm && (found_flag==xaFALSE))
  {
    if (tmp_parm->orig == cmap_id) found_flag = xaTRUE;
    else tmp_parm = tmp_parm->next;
  }

  if (found_flag == xaFALSE) TheEnd1(stderr,"PFX CMAP: not found\n");

  pfx_imagec = tmp_parm->len--;
  pfx_imagec /= 3;
  ptr = tmp_parm->data;
  for(i=0;i<pfx_imagec;i++)
  {
    xaULONG d;
    d = (*ptr++) - 48; if (d > 9) d -= 7;   /* ascii to integer */
    pfx_cmap[i].red =  d & 0x0f;
    d = (*ptr++) - 48; if (d > 9) d -= 7;   /* ascii to integer */
    pfx_cmap[i].green =  d & 0x0f;
    d = (*ptr++) - 48; if (d > 9) d -= 7;   /* ascii to integer */
    pfx_cmap[i].blue =  d & 0x0f;
    DEBUG_LEVEL2 fprintf(stderr," %ld) %lx %lx %lx\n",
	i,pfx_cmap[i].red,pfx_cmap[i].green,pfx_cmap[i].blue);
  }
  pfx_chdr = ACT_Get_CMAP(pfx_cmap,pfx_imagec,0,pfx_imagec,0,4,4,4);
}
   

