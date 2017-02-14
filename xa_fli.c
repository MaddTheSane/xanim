
/*
 * xa_fli.c
 *
 * Copyright (C) 1990,1991,1992,1993,1994,1995 by Mark Podlipec. 
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
#include "xa_fli.h" 

extern xaULONG fli_pad_kludge;

xaLONG Is_FLI_File();
xaULONG Fli_Read_File();
static void FLI_Read_Header();
xaULONG FLI_Read_Frame_Header();
FLI_FRAME *FLI_Add_Frame();
void FLI_Set_Time();
xaULONG FLI_Decode_BRUN();
xaULONG FLI_Decode_LC();
xaULONG FLI_Decode_COPY();
xaULONG FLI_Decode_BLACK();
xaULONG FLI_Decode_LC7();
static void FLI_Read_COLOR();

extern XA_ACTION *ACT_Get_Action();
extern XA_CHDR *ACT_Get_CMAP();
extern void ACT_Add_CHDR_To_Action();
extern void ACT_Setup_Mapped();
extern void ACT_Get_CCMAP();
extern XA_ANIM_SETUP *XA_Get_Anim_Setup();
extern void XA_Free_Anim_Setup();
extern void ACT_Setup_Delta();

extern void UTIL_Sub_Image();
extern xaULONG UTIL_Get_LSB_Long();
extern xaULONG UTIL_Get_LSB_Short();

static Fli_Header fli_hdr;
static Fli_Frame_Header frame_hdr;

static FLI_FRAME *fli_frame_start,*fli_frame_cur;
static xaULONG fli_frame_cnt;

FLI_FRAME *FLI_Add_Frame(time,timelo,act)
xaULONG time,timelo;
XA_ACTION *act;
{
  FLI_FRAME *fframe;

  fframe = (FLI_FRAME *) malloc(sizeof(FLI_FRAME));
  if (fframe == 0) TheEnd1("FLI_Add_Frame: malloc err");

  fframe->time   = time;
  fframe->timelo = timelo;
  fframe->act = act;
  fframe->next = 0;

  if (fli_frame_start == 0) fli_frame_start = fframe;
  else fli_frame_cur->next = fframe;

  fli_frame_cur = fframe;
  fli_frame_cnt++;
  return(fframe);
}

static void FLI_Free_Frame_List(fframes)
FLI_FRAME *fframes;
{
  FLI_FRAME *ftmp;
  while(fframes != 0)
  {
    ftmp = fframes;
    fframes = fframes->next;
    FREE(ftmp,0x2000);
  }
}

/*
 *
 */
xaLONG Is_FLI_File(filename)
char *filename;
{
  FILE *fin;
  xaULONG data;

  if ( (fin=fopen(filename,XA_OPEN_MODE)) == 0) return(xaNOFILE);

  data = UTIL_Get_LSB_Long(fin);  /* read past size */
  data = UTIL_Get_LSB_Short(fin); /* read magic */
  fclose(fin);
  if ( (data == 0xaf11) || (data == 0xaf12) ) return(xaTRUE);
  return(xaFALSE);
}


static void FLI_Read_Header(fli,fin,fli_hdr)
XA_ANIM_SETUP *fli;
FILE *fin;
Fli_Header *fli_hdr;
{
  xaLONG i;

  fli_hdr->size   = UTIL_Get_LSB_Long(fin);
  fli_hdr->magic  = UTIL_Get_LSB_Short(fin);
  fli_hdr->frames = UTIL_Get_LSB_Short(fin);
  fli_hdr->width  = UTIL_Get_LSB_Short(fin);
  fli_hdr->height = UTIL_Get_LSB_Short(fin);
  fli_hdr->res1   = UTIL_Get_LSB_Short(fin);
  fli_hdr->flags  = UTIL_Get_LSB_Short(fin);
  fli_hdr->speed  = UTIL_Get_LSB_Short(fin);
  if (fli_hdr->speed <= 0) fli_hdr->speed = 1;
  fli_hdr->next   = UTIL_Get_LSB_Long(fin);
  fli_hdr->frit   = UTIL_Get_LSB_Long(fin);
  for(i=0;i<102;i++) fgetc(fin);   /* ignore unused part of Fli_Header */

  fli->imagex=fli_hdr->width;
  fli->imagey=fli_hdr->height;
  if ( (fli_hdr->magic != 0xaf11) && (fli_hdr->magic != 0xaf12) )
  {
   fprintf(stderr,"imagex=%lx imagey=%lx\n",fli->imagex,fli->imagey);
   fprintf(stderr,"Fli Header Error magic %lx not = 0xaf11\n",fli_hdr->magic);
   TheEnd();
  }
}

xaULONG FLI_Read_Frame_Header(fp,frame_hdr)
FILE *fp;
Fli_Frame_Header *frame_hdr;
{
  xaULONG i;
  xaUBYTE tmp[6];

  for(i=0;i<6;i++) tmp[i] = (xaUBYTE)fgetc(fp);

  DEBUG_LEVEL1 fprintf(stderr,"  magic = %02lx%02lx\n",tmp[5],tmp[4]);
  while( !( (tmp[5]==0xf1) && ((tmp[4]==0xfa) || (tmp[4] == 0x00)) ) )
  {
    for(i=0;i<6;i++) tmp[i] = tmp[i+1];
    tmp[5] = (xaUBYTE)fgetc(fp);
    if (feof(fp)) return(0);
  }

  frame_hdr->size = (tmp[0])|(tmp[1] << 8)|(tmp[2] << 16)|(tmp[3] << 24);
  frame_hdr->magic = (tmp[4])|(tmp[5] << 8);
  frame_hdr->chunks = UTIL_Get_LSB_Short(fp);
  for(i=0;i<8;i++) fgetc(fp);	/* ignore unused part of Fli_Frame_Header */

  DEBUG_LEVEL1 
	fprintf(stderr,"Frame Header size %lx  magic %lx  chunks %ld\n",
		frame_hdr->size,frame_hdr->magic,frame_hdr->chunks);

  return(1);
}

xaULONG Fli_Read_File(fname,anim_hdr)
char *fname;
XA_ANIM_HDR *anim_hdr;
{
  FILE *fin;
  xaLONG j,ret;
  XA_ACTION *act;
  xaULONG tmp_frame_cnt,file_pos,frame_i;
  xaULONG t_time,t_timelo;
 XA_ANIM_SETUP *fli;

  if ( (fin=fopen(fname,XA_OPEN_MODE)) == 0)
  { 
    fprintf(stderr,"can't open Fli File %s for reading\n",fname); 
    return(xaFALSE);
  }
 
  fli = XA_Get_Anim_Setup();
  fli->depth = 8;

  fli_frame_start = 0;
  fli_frame_cur = 0;
  fli_frame_cnt = 0;

  FLI_Read_Header(fli,fin,&fli_hdr);

  if (xa_verbose) fprintf(stderr,"   Size %ldx%ld Frames= %ld Magic= %lx\n",
	fli_hdr.width, fli_hdr.height,fli_hdr.frames,fli_hdr.magic);

    /* Allocate image buffer so deltas may be applied if buffering */
  fli->pic_size = fli->imagex * fli->imagey;
  if (xa_buffer_flag == xaTRUE)
  {
    fli->pic = (xaUBYTE *) malloc( XA_PIC_SIZE(fli->pic_size) );
    if (fli->pic == 0) TheEnd1("BufferAction: malloc failed");
  }


  tmp_frame_cnt = 0;
  file_pos= 0x80;  /* need to do this because chunk sizes don't always
		    * add up to the frame size. */
  while( FLI_Read_Frame_Header(fin, &frame_hdr) != 0)
  {
    xaULONG frame_read_size;

    file_pos += frame_hdr.size;
    tmp_frame_cnt++;
    FLI_Set_Time(fli,fli_hdr.speed);
    
    if (frame_hdr.magic == 0xf100)
    {
      xaLONG i;
      DEBUG_LEVEL1 
	 fprintf(stderr,"FLI 0xf100 Frame: size = %lx\n",frame_hdr.size);
      for(i=0;i<(frame_hdr.size - 16);i++) fgetc(fin);
      if (frame_hdr.size & 0x01) fgetc(fin);
    }
    else if (frame_hdr.chunks==0)  /* this frame is for timing purposes */
    {
      DEBUG_LEVEL1 fprintf(stderr," FLI DELAY %ld\n",fli_hdr.speed);
      act = ACT_Get_Action(anim_hdr,ACT_DELAY);
      act->data = 0;
      FLI_Add_Frame(fli->vid_time,fli->vid_timelo,act);
    }
    else /* this frame has real data in it */
    {
      /* Loop through chunks in the frame
       */
      frame_read_size = 10;
      for(j=0;j<frame_hdr.chunks;j++)
      {
        xaLONG chunk_size;
	xaULONG chunk_type;
   
	/* only last chunk in frame should have full time(j must stay signed)*/
        if (j < (frame_hdr.chunks - 1) ) { fli->vid_time = fli->vid_timelo = 0; }
	else FLI_Set_Time(fli,fli_hdr.speed);
        chunk_size = UTIL_Get_LSB_Long(fin) & 0xffffff;
        chunk_type = UTIL_Get_LSB_Short(fin);
	frame_read_size += 6 + chunk_size;
	if ( (chunk_size & 0x01)&&(fli_pad_kludge==xaFALSE)) frame_read_size++;
        DEBUG_LEVEL1 fprintf(stderr,"  chunk %ld) size %lx  type %lx tot %lx\n",
		j,chunk_size,chunk_type,frame_read_size);
        switch(chunk_type)
        {
          case CHUNK_4:     /* FLI Color with 8 bits RGB */
		DEBUG_LEVEL1 fprintf(stderr," FLI COLOR(4)\n");
		FLI_Read_COLOR(fli,anim_hdr,fin,8,frame_hdr.chunks);
		break;
          case FLI_COLOR:     /* FLI Color with 6 bits RGB */
		DEBUG_LEVEL1 fprintf(stderr," FLI COLOR(11)\n");
		FLI_Read_COLOR(fli,anim_hdr,fin,6,frame_hdr.chunks);
		break;
          case FLI_LC:
          case FLI_LC7:
          case FLI_BRUN:
          case FLI_COPY:
          case FLI_BLACK:
		{
		  ACT_DLTA_HDR *dlta_hdr;
		 
		  if (chunk_type==FLI_COPY) chunk_size = fli->pic_size;
		  else	chunk_size -= 6;

		  act = ACT_Get_Action(anim_hdr,ACT_DELTA);
		  FLI_Add_Frame(fli->vid_time,fli->vid_timelo,act);

		  if (chunk_type == FLI_BLACK)
		  {
		    if (chunk_size != 0) TheEnd1("FLI BLACK: size err\n");
		    else
		    {
 		      dlta_hdr = (ACT_DLTA_HDR *)malloc(sizeof(ACT_DLTA_HDR));
 		      if (dlta_hdr == 0) TheEnd1("FLI BK CHUNK: malloc failed");
		      act->data = (xaUBYTE *)dlta_hdr;
		      dlta_hdr->flags = ACT_SNGL_BUF | DLTA_DATA;
		      dlta_hdr->fpos = 0; dlta_hdr->fsize = 0;
		    }
		  }
		  else if (chunk_size > 0)
		  {
		    if (xa_file_flag == xaTRUE)
		    {
 		      dlta_hdr = (ACT_DLTA_HDR *) malloc(sizeof(ACT_DLTA_HDR));
 		      if (dlta_hdr == 0) TheEnd1("FLI CHUNK: malloc failed");
		      act->data = (xaUBYTE *)dlta_hdr;
		      dlta_hdr->flags = ACT_SNGL_BUF;
		      dlta_hdr->fpos  = ftell(fin);
		      dlta_hdr->fsize = chunk_size;
 		      fseek(fin,chunk_size,1); /* move past this chunk */
		      if (chunk_size > fli->max_fvid_size) 
					fli->max_fvid_size = chunk_size;
		    }
		    else
		    {
 		      dlta_hdr = (ACT_DLTA_HDR *)
				malloc(sizeof(ACT_DLTA_HDR) + chunk_size);
 		      if (dlta_hdr == 0) TheEnd1("FLI CHUNK: malloc failed");
		      act->data = (xaUBYTE *)dlta_hdr;
		      dlta_hdr->flags = 
				ACT_SNGL_BUF | DLTA_DATA;
		      dlta_hdr->fpos = 0; dlta_hdr->fsize = chunk_size;
 		      ret = fread( dlta_hdr->data, chunk_size, 1, fin);
 		      if (ret != 1) TheEnd1("FLI DLTA: read failed");
		    }
		  }
		  else TheEnd1("FLI DLTA: invalid size");

		  dlta_hdr->xpos = dlta_hdr->ypos = 0;
		  dlta_hdr->xsize = fli->imagex;
		  dlta_hdr->ysize = fli->imagey;
		  dlta_hdr->special = 0;
		  dlta_hdr->extra = (void *)(0);
		  dlta_hdr->xapi_rev = 0x0001;
		  switch(chunk_type)
		  {
		   case FLI_LC7:
			DEBUG_LEVEL1 fprintf(stderr," FLI LC(7)\n");
			dlta_hdr->delta = FLI_Decode_LC7;
			break;
		  case FLI_LC:
			DEBUG_LEVEL1 fprintf(stderr," FLI LC(12)\n");
			dlta_hdr->delta = FLI_Decode_LC;
			break;
		  case FLI_BLACK:
			DEBUG_LEVEL1 fprintf(stderr," FLI BLACK(13)\n");
			dlta_hdr->delta = FLI_Decode_BLACK;
			break;
		  case FLI_BRUN:
			DEBUG_LEVEL1 fprintf(stderr," FLI BRUN(15)\n");
			dlta_hdr->delta = FLI_Decode_BRUN;
			break;
		  case FLI_COPY:
			DEBUG_LEVEL1 fprintf(stderr," FLI COPY(16)\n");
			dlta_hdr->delta = FLI_Decode_COPY;
			break;
		 }
		 ACT_Setup_Delta(fli,act,dlta_hdr,fin);
		}
		break;
 
          case FLI_MINI:
		{
		  xaLONG i;
		  DEBUG_LEVEL1 fprintf(stderr," FLI MINI(18) ignored.\n");
		  for(i=0;i<(chunk_size-6);i++) fgetc(fin);
		  if ((fli_pad_kludge==xaFALSE)&&(chunk_size & 0x01)) fgetc(fin);
		}
		break;

           default: 
             {
               xaLONG i;
	       fprintf(stderr,"FLI Unsupported Chunk: type = %lx size=%lx\n",
					chunk_type,chunk_size);
               for(i=0;i<(chunk_size-6);i++) fgetc(fin);
	       if ((fli_pad_kludge==xaFALSE)&&(chunk_size & 0x01)) fgetc(fin);
             }
	         
		 break;
        } /* end of switch */
      } /* end of chunks is frame */
    } /* end of not Timing Frame */
    /* More robust way of reading FLI's. */
    fseek(fin,file_pos,0);
  } /* end of frames in file */
  if (fli->pic != 0) { FREE(fli->pic,0x2000); fli->pic=0;}
  fclose(fin);

  /* extra for end and JMP2END */
  anim_hdr->frame_lst = (XA_FRAME *)
		malloc(sizeof(XA_FRAME) * (fli_frame_cnt + 2));
  if (anim_hdr->frame_lst == NULL) TheEnd1("FLI_Read_Anim: malloc err");

  fli_frame_cur = fli_frame_start;
  frame_i = 0;
  t_time = t_timelo = 0;
  while(fli_frame_cur != 0)
  {
    if (frame_i > fli_frame_cnt)
    {
      fprintf(stderr,"FLI_Read_Anim: frame inconsistency %ld %ld\n",
                frame_i,fli_frame_cnt);
      break;
    }
    anim_hdr->frame_lst[frame_i].time_dur = fli_frame_cur->time;
    anim_hdr->frame_lst[frame_i].zztime = t_time;
    anim_hdr->frame_lst[frame_i].act = fli_frame_cur->act;
    t_time += fli_frame_cur->time;
    t_timelo += fli_frame_cur->timelo;
    while(t_timelo > (1<<24)) {t_time++; t_timelo -= (1<<24);}
    fli_frame_cur = fli_frame_cur->next;
    frame_i++;
  }
  /* ADD JMP2END for FLI/FLC animations with a loop frame */
  if ( (tmp_frame_cnt > fli_hdr.frames) && (frame_i > 1)
      && (!(anim_hdr->anim_flags & ANIM_NOLOOP)) )
  { 
    anim_hdr->total_time = anim_hdr->frame_lst[frame_i-2].zztime 
				+ anim_hdr->frame_lst[frame_i-2].time_dur;

    /* This last frame takes place of 1st frame and so has time of 0 */
    anim_hdr->frame_lst[frame_i].zztime = 0;
    anim_hdr->frame_lst[frame_i].time_dur 
				= anim_hdr->frame_lst[frame_i-1].time_dur;
    anim_hdr->frame_lst[frame_i].act  = anim_hdr->frame_lst[frame_i-1].act;
    anim_hdr->frame_lst[frame_i-1].zztime = -1;
    anim_hdr->frame_lst[frame_i-1].time_dur = 0;
    anim_hdr->frame_lst[frame_i-1].act = ACT_Get_Action(anim_hdr,ACT_JMP2END);
    frame_i++;
    anim_hdr->loop_frame = 1;
  }
  else 
  {
    anim_hdr->loop_frame = 0;
    if (frame_i > 0)
      anim_hdr->total_time = anim_hdr->frame_lst[frame_i-1].zztime 
				+ anim_hdr->frame_lst[frame_i-1].time_dur;
    else anim_hdr->total_time = 0;
  }
  anim_hdr->last_frame = (frame_i>0)?(frame_i-1):(0);
  anim_hdr->frame_lst[frame_i].zztime = -1;
  anim_hdr->frame_lst[frame_i].time_dur = 0;
  anim_hdr->frame_lst[frame_i].act  = 0;
  frame_i++;
  anim_hdr->imagex = fli->imagex;
  anim_hdr->imagey = fli->imagey;
  anim_hdr->imagec = fli->imagec;
  anim_hdr->imaged = fli->depth;
  anim_hdr->max_fvid_size = fli->max_fvid_size;
  FLI_Free_Frame_List(fli_frame_start);
  if (xa_buffer_flag == xaFALSE) anim_hdr->anim_flags |= ANIM_SNG_BUF;
  if (xa_file_flag == xaTRUE) anim_hdr->anim_flags |= ANIM_USE_FILE;
  anim_hdr->fname = anim_hdr->name; /* data file is same as name */
  XA_Free_Anim_Setup(fli);
  return(xaTRUE);
}


/*
 * Routine to Decode a Fli BRUN chunk
 */
xaULONG
FLI_Decode_BRUN(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG special = dec_info->special;          void *extra = dec_info->extra;
  XA_CHDR *chdr = dec_info->chdr;
  xaULONG i,k,packets,size,x,offset,pix_size;

  dec_info->xs = dec_info->ys = 0; dec_info->xe = imagex; dec_info->ye = imagey;
  pix_size = (map_flag==xaTRUE)?x11_bytes_pixel:1;
  for(i=0; i < imagey; i++)
  {
    offset = i * imagex;
    packets = *delta++;

    x=0;
/* Apparently, some flc file don't properly set packets. POD NOTE:
    for(j= 0; j < packets; j++)
*/
    while(x < imagex)
    {
      size = *delta++;
      if (size & 0x80)         /* size < 0 so there is -size unique bytes */
      {
        size = 256-size; 
        if (map_flag == xaTRUE)
        {
          if (pix_size == 4)
            for(k=x;k<(x+size);k++)
                ((xaULONG *)(image))[k+offset] = (xaULONG)(map[*delta++]);
          else if (pix_size == 2)
            for(k=x;k<(x+size);k++)
                ((xaUSHORT *)(image))[k+offset] = (xaUSHORT)(map[*delta++]);
          else
            for(k=x;k<(x+size);k++)
                ((xaUBYTE *)(image))[k+offset] = (xaUBYTE)(map[*delta++]);
        }
        else
        {
          for(k=x;k<(x+size);k++)
                ((xaUBYTE *)(image))[k+offset] = (xaUBYTE)(*delta++);
        }

        x += size;   
      }
      else                     /* size is pos repeat next byte size times */
      {
        xaULONG d;
        d = *delta++;
        if (map_flag == xaTRUE)
        {
          if (pix_size == 4)
            for(k=x;k<(x+size);k++)
                ((xaULONG *)(image))[k+offset] = (xaULONG)(map[d]);
          else if (pix_size == 2)
             for(k=x;k<(x+size);k++)
                ((xaUSHORT *)(image))[k+offset] = (xaUSHORT)(map[d]);
          else
             for(k=x;k<(x+size);k++)
                ((xaUBYTE *)(image))[k+offset] = (xaUBYTE)(map[d]);
        }
        else
        {
          for(k=x;k<(x+size);k++)
                ((xaUBYTE *)(image))[k+offset] = (xaUBYTE)(d);
        }
        x+=size;   
      }
    } /* end of packets per line */
  } /* end of line */
  if (map_flag) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}

/*
 * Routine to Decode an Fli LC chunk
 */

xaULONG
FLI_Decode_LC(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG special = dec_info->special;          void *extra = dec_info->extra;
  XA_CHDR *chdr = dec_info->chdr;
  xaULONG i,j,k,packets,size,x,offset;
  xaULONG start,lines,skip,minx,maxx,pix_size;

  pix_size = (map_flag==xaTRUE)?x11_bytes_pixel:1;
  start = *delta++; start |= *delta++ << 8;  /* lines to skip */
  lines = *delta++; lines |= *delta++ << 8;  /* number of lines */

  minx = imagex; 
  maxx = 0;
 
  for(i=start;i<(start+lines);i++)
  {
    offset = i * imagex;
    packets = *delta++;

    x=0;
    for(j=0;j<packets;j++)
    {
      skip = *delta++;   /* this is the skip count */
      size = *delta++;

      if (j==0) { if (skip < minx) minx = skip; }
      x+=skip;
      if (size & 0x80) /* next byte repeated -size times */
      {
        xaULONG d;
        size = 256-size; 
        d = *delta++;
	if (map_flag == xaTRUE)
        {
	  if (pix_size == 4)
            for(k=x;k<(x+size);k++) 
		((xaULONG *)(image))[k+offset] = (xaULONG)(map[d]);
	  else if (pix_size == 2)
             for(k=x;k<(x+size);k++) 
		((xaUSHORT *)(image))[k+offset] = (xaUSHORT)(map[d]);
	  else
             for(k=x;k<(x+size);k++) 
		((xaUBYTE *)(image))[k+offset] = (xaUBYTE)(map[d]);
        }
        else
        {
          for(k=x;k<(x+size);k++) 
		((xaUBYTE *)(image))[k+offset] = (xaUBYTE)(d);
        }
        x+=size;   
      }
      else if (size) /* size is pos */
      {
        if (map_flag == xaTRUE)
        {
          if (pix_size == 4)
            for(k=x;k<(x+size);k++) 
		((xaULONG *)(image))[k+offset] = (xaULONG)(map[*delta++]);
          else if (pix_size == 2)
            for(k=x;k<(x+size);k++) 
		((xaUSHORT *)(image))[k+offset] = (xaUSHORT)(map[*delta++]);
          else
            for(k=x;k<(x+size);k++) 
		((xaUBYTE *)(image))[k+offset] = (xaUBYTE)(map[*delta++]);
        }
        else
        {
          for(k=x;k<(x+size);k++) 
		((xaUBYTE *)(image))[k+offset] = (xaUBYTE)(*delta++);
        }

        x+=size;   
      }
    } /* end of packets per line */
    if (x > maxx) maxx = x;
  } /* end of line */

  if ( (lines==0) || (maxx <= minx) )
  	{ dec_info->ys = dec_info->ye = dec_info->xs = dec_info->xe = 0;
						return(ACT_DLTA_NOP); }
  if (xa_optimize_flag == xaTRUE)
  {
    dec_info->ys = start;
    dec_info->ye = start + lines;
    dec_info->xs = minx;
    if (maxx > imagex) maxx=imagex;
    if (maxx > minx) dec_info->xe = maxx;
    else dec_info->xe = imagex;
  }
  else { dec_info->ys = 0; dec_info->ye = imagey;
	 dec_info->xs = 0; dec_info->xe = imagex; }

  if (map_flag) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}

xaULONG
FLI_Decode_COPY(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG special = dec_info->special;          void *extra = dec_info->extra;
  XA_CHDR *chdr = dec_info->chdr;
  xaULONG image_size = imagex * imagey;
  dec_info->xs = dec_info->ys = 0;
  dec_info->xe = imagex; dec_info->ye = imagey;
  if (map_flag)
  {
    if (x11_bytes_pixel == 4)
    { xaULONG *i_ptr = (xaULONG *)image;
      while(image_size--) *i_ptr++ = (xaULONG)map[ *delta++ ];
    }
    else if (x11_bytes_pixel == 2)
    { xaUSHORT *i_ptr = (xaUSHORT *)image;
      while(image_size--) *i_ptr++ = (xaUSHORT)map[ *delta++ ];
    }
    else
    { xaUBYTE *i_ptr = (xaUBYTE *)image;
      while(image_size--) *i_ptr++ = (xaUBYTE)map[ *delta++ ];
    }
  } 
  else memcpy( (char *)image,(char *)delta,image_size);
  if (map_flag) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}

xaULONG
FLI_Decode_BLACK(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG special = dec_info->special;          void *extra = dec_info->extra;
  XA_CHDR *chdr = dec_info->chdr;
  xaULONG image_size = imagex * imagey;
  dec_info->xs = dec_info->ys = 0;
  dec_info->xe = imagex; dec_info->ye = imagey;
  if (map_flag)
  {
    xaULONG black = map[0];
    if (x11_bytes_pixel == 4)
    { xaULONG *i_ptr = (xaULONG *)image;
      while(image_size--) *i_ptr++ = (xaULONG)black;
    }
    else if (x11_bytes_pixel == 2)
    { xaUSHORT *i_ptr = (xaUSHORT *)image;
      while(image_size--) *i_ptr++ = (xaUSHORT)black;
    }
    else
    { xaUBYTE *i_ptr = (xaUBYTE *)image;
      while(image_size--) *i_ptr++ = (xaUBYTE)black;
    }
  } 
  else memset( (char *)image,0,image_size);
  if (map_flag) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}


/*
 * Routine to read an Fli COLOR chunk
 */
static void FLI_Read_COLOR(fli,anim_hdr,fin,cbits,num_of_chunks)
XA_ANIM_SETUP *fli;
XA_ANIM_HDR *anim_hdr;
FILE *fin;
xaULONG cbits;
xaULONG num_of_chunks;
{
 XA_ACTION *act;
 xaULONG k,l,c_index,packets,skip,colors,cnt;
 xaULONG mask;

 mask = (0x01 << cbits) - 1;
 packets = UTIL_Get_LSB_Short(fin);
 c_index = 0;

 DEBUG_LEVEL1 fprintf(stderr,"   packets = %ld\n",packets);
 cnt=2;
 for(k=0;k<packets;k++)
 {
  skip = fgetc(fin);
  colors=fgetc(fin);
  DEBUG_LEVEL1 fprintf(stderr,"      skip %ld colors %ld\n",skip,colors);
  cnt+=2;
  if (colors==0) colors=FLI_MAX_COLORS;
  c_index += skip;
  for(l = 0; l < colors; l++)
  {
   fli->cmap[c_index].red   = fgetc(fin) & mask;
   fli->cmap[c_index].green = fgetc(fin) & mask;
   fli->cmap[c_index].blue  = fgetc(fin) & mask;
   DEBUG_LEVEL5 fprintf(stderr,"         %ld)  <%lx %lx %lx>\n", 
		  l,fli->cmap[l].red, fli->cmap[l].green,fli->cmap[l].blue);
   c_index++;
  } /* end of colors */
  cnt+= 3 * colors;
 } /* end of packets */
			/* read pad byte if needed */
 if ((fli_pad_kludge==xaFALSE)&&(cnt&0x01)) fgetc(fin);

 /* if only one chunk in frame this is a cmap change only */
 if ( (num_of_chunks==1) && (fli->chdr != 0) )
 {
   act = ACT_Get_Action(anim_hdr,0);
   ACT_Get_CCMAP(act,fli->cmap,FLI_MAX_COLORS,0,cbits,cbits,cbits);
   FLI_Add_Frame(fli->vid_time,fli->vid_timelo,act);
   ACT_Add_CHDR_To_Action(act,fli->chdr);
 }
 else
   fli->chdr = ACT_Get_CMAP(fli->cmap,FLI_MAX_COLORS,0,FLI_MAX_COLORS,0,
							cbits,cbits,cbits);
}

/*
 * Routine to Decode an Fli LC chunk
 */
xaULONG
FLI_Decode_LC7(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG special = dec_info->special;          void *extra = dec_info->extra;
  XA_CHDR *chdr = dec_info->chdr;
  xaULONG i,j,x,y;
  xaULONG lines,blocks,xoff,cnt,tmp_data0,tmp_data1;
  xaULONG minx,maxx,miny,pix_size,last_pixel,last_pix_flag;
  xaUBYTE *ptr;
  
  minx = imagex;
  maxx = 0;
  miny = imagey;

  pix_size = (map_flag==xaTRUE)?x11_bytes_pixel:1;
  ptr = image;
  y = 0;
  lines = *delta++; lines |= *delta++ << 8;  /* # of lines encoded */

  DEBUG_LEVEL5 fprintf(stderr,"lines=%ld\n",lines);

  last_pix_flag = last_pixel = 0;
  for(i=0; i < lines; i++)
  {
   
    blocks = *delta++; blocks |= *delta++ << 8; 

    DEBUG_LEVEL5 fprintf(stderr,"     %ld) ",i);

    while(blocks & 0x8000)
    {
      /* Upper bits 11 - SKIP lines */
      if (blocks & 0x4000)
      {
        blocks = 0x10000 - blocks;
        y += blocks;
        DEBUG_LEVEL5 fprintf(stderr,"     yskip %ld",blocks);
      }
      else /* Upper bits 10 - Last Pixel Encoding */
      {
        DEBUG_LEVEL5 fprintf(stderr,"     lastpixel %ld",blocks);
        last_pix_flag = 1;
        last_pixel = blocks & 0xff;
      }
      blocks = *delta++; blocks |= *delta++ << 8;
    }

    DEBUG_LEVEL5 fprintf(stderr,"     blocks = %ld\n",blocks);

    if (xa_optimize_flag == xaTRUE) if (y < miny) miny = y;

    ptr = (xaUBYTE *)(image + (y*imagex*pix_size) );
    x = 0;

    for(j=0; j < blocks; j++)
    {
      xoff = (xaULONG) *delta++;  /* x offset */
      cnt  = (xaULONG) *delta++;  /* halfword count */
      ptr += (xoff * pix_size);
      x += xoff;

      DEBUG_LEVEL5 
        fprintf(stderr,"          %ld) xoff %lx  cnt = %lx",j,xoff,cnt);

      if (cnt & 0x80)
      {
        cnt = 256 - cnt;
        DEBUG_LEVEL5 fprintf(stderr,"               NEG %ld\n",cnt);
        if (xa_optimize_flag == xaTRUE)
        {  
          if (x < minx) minx = x;
          x += (cnt << 1);
          if (x > maxx) maxx = x;
        }

	if (map_flag == xaTRUE)
        {
          tmp_data0 = map[*delta++];
          tmp_data1 = map[*delta++];
          if (pix_size == 4)
	  { 
	    xaULONG *ulp = (xaULONG *)ptr;
            while(cnt--) { *ulp++ = (xaULONG)tmp_data0;
			   *ulp++ = (xaULONG)tmp_data1; }
	    ptr = (xaUBYTE *)ulp;
          }
          else if (pix_size == 2)
	  { 
	    xaUSHORT *usp = (xaUSHORT *)ptr;
	    while(cnt--) { *usp++ = (xaUSHORT)tmp_data0;
			   *usp++ = (xaUSHORT)tmp_data1; }
	    ptr = (xaUBYTE *)usp;
          }
          else
            while(cnt--) { *ptr++ = (xaUBYTE)tmp_data0;
                           *ptr++ = (xaUBYTE)tmp_data1; }
	}
	else
	{
          tmp_data0 = *delta++;
          tmp_data1 = *delta++;
          while(cnt--) { *ptr++ = (xaUBYTE)tmp_data0;
                         *ptr++ = (xaUBYTE)tmp_data1; }
	}

      }
      else
      {   /* cnt is number of unique pairs of bytes */
        DEBUG_LEVEL5 fprintf(stderr,"               POS %ld\n",cnt);
        if (xa_optimize_flag == xaTRUE)
        {
          if (cnt == 1)  /* potential NOPs just to move x */
          {
            if ( (*ptr != *delta) || (ptr[1] != delta[1]) )  
            {
              if (x < minx) minx = x; 
              x += (cnt << 1);
              if (x > maxx) maxx = x;
            }
          }
	  else
          {
            if (x < minx) minx = x; 
            x += (cnt << 1);
            if (x > maxx) maxx = x;
          }
        }
        if (map_flag == xaTRUE)
        {
          if (pix_size == 4)
	  { 
	    xaULONG *ulp = (xaULONG *)ptr;
	    while(cnt--) { *ulp++ = (xaULONG)map[*delta++];
                           *ulp++ = (xaULONG)map[*delta++]; }
	    ptr = (xaUBYTE *)ulp;
          }
          else if (pix_size == 2)
	  { 
	    xaUSHORT *usp = (xaUSHORT *)ptr;
	    while(cnt--) { *usp++ = (xaUSHORT)map[*delta++];
			   *usp++ = (xaUSHORT)map[*delta++]; }
	    ptr = (xaUBYTE *)usp;
          }
          else
             while(cnt--) { *ptr++ = (xaUBYTE)map[*delta++];
			    *ptr++ = (xaUBYTE)map[*delta++]; }
        }
        else
             while(cnt--) { *ptr++ = (xaUBYTE) *delta++;
			    *ptr++ = (xaUBYTE) *delta++; }

      } 
    } /* (j) end of blocks */
    if (last_pix_flag) /* last pixel */
    {
        if (map_flag == xaTRUE)
        {
          if (pix_size == 4)
		{ xaULONG *ulp = (xaULONG *)ptr; *ulp = (xaULONG)map[last_pixel];}
          else if (pix_size == 2)
		{ xaUSHORT *slp = (xaUSHORT *)ptr; *slp = (xaUSHORT)map[last_pixel];}
          else { *ptr = (xaUBYTE)map[last_pixel]; }
        }
        else *ptr = (xaUBYTE)(last_pixel);
        last_pix_flag = 0;
    }
    y++;
  } /* (i) end of lines */

  if (xa_optimize_flag == xaTRUE)
  {
    if ( (maxx <= minx) || (y <= miny) )
  	{ dec_info->ys = dec_info->ye = dec_info->xs = dec_info->xe = 0;
					return(ACT_DLTA_NOP); }
    if (minx >= imagex) minx = 0;     dec_info->xs = minx;
    if (miny >= imagey) miny = 0;     dec_info->ys = miny;
/* POD NOTE: look into the +2 and +1 stuff */
    maxx += 2;  if (maxx > imagex) maxx = imagex;
    y += 1; if (y > imagey) y = imagey;
    dec_info->xe = maxx;
    dec_info->ye = y;
  }
  else { dec_info->xs = 0; dec_info->ys = 0;
	 dec_info->xe = imagex; dec_info->ye = imagey; }
  DEBUG_LEVEL1 fprintf(stderr,"      LC7: xypos=<%ld %ld> xysize=<%ld %ld>\n",
		 dec_info->xs,dec_info->ys,dec_info->xe,dec_info->ye);
  if (map_flag) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}

/* 
 * FLI has time period of 1/70th sec where  FLC has time period of 1 ms.
 * affects global variables fli->vid_time, fli->vid_timelo.
 * uses global variable fli_hdr.magic.
 */
void FLI_Set_Time(fli,speed)
XA_ANIM_SETUP *fli;
xaULONG speed;
{
  fli->vid_timelo = 0;
  if (xa_jiffy_flag) fli->vid_time = xa_jiffy_flag;
  else if (fli_hdr.magic == 0xaf11)	/* 1/70th sec units */
  { double ftime = (((double)(speed) * 1000.0)/70.0); /* convert to ms */
    fli->vid_time =  (xaULONG)(ftime);
    ftime -= (double)(fli->vid_time);
    fli->vid_timelo = (ftime * (double)(1<<24));
  }
  else fli->vid_time = speed; /* 1 ms units */
}


