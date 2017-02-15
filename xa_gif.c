
/*
 * xa_gif.c
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

/* REVISIONS ***********
 * 30Mar95  Code made a little more robust to handle GIF images that
 *          violate the spec by not including an EOI and the end
 *          of the image.
 * 30Mar97  Donald Becker sent patch to fix bug in above missing EOI code.
 * 14Dec97  Adam Moss sent patches to pay attention to the gif animation
 *	    disposal methods.
 * 15Mar98  Improved GIF disposal handling. [Adam D. Moss: adam@gimp.org]
 *          
 */
#include "xa_gif.h"
#include "xa_formats.h"

GIF_FRAME *GIF_Add_Frame();

xaULONG GIF_Decompress();
xaULONG GIF_Get_Next_Entry();
void GIF_Add_To_Table();
xaULONG GIF_Send_Data();
void GIF_Init_Table();
void GIF_Clear_Table();
xaULONG GIF_Get_Code();
void GIF_Screen_Header();
void GIF_Image_Header();
void GIF_Read_Extension();

XA_ACTION *ACT_Get_Action();
XA_CHDR   *ACT_Get_CMAP();
void ACT_Setup_PIXMAP();
void ACT_Setup_IMAGE();
void ACT_Setup_CLIP();
void ACT_Setup_Mapped();
void ACT_Add_CHDR_To_Action();


#define MAXVAL  4100            /* maxval of lzw coding size */
#define MAXVALP 4200


#define GIF_CMAP_SIZE 256
static GIF_Screen_Hdr gifscrn;
static GIF_Image_Hdr gifimage;
static ColorReg gif_cmap[GIF_CMAP_SIZE];
static XA_CHDR *gif_chdr_now;
static GIF_Table table[MAXVALP];

static xaULONG root_code_size,code_size,CLEAR,EOI,INCSIZE;
static xaULONG nextab;
static xaULONG gif_mask[16] = {1,1,3,7,15,31,63,127,255,511,1023,2047,4095,8191,0,0};
static xaULONG gif_ptwo[16] = {1,2,4,8,16,32,64,128,256,512,1024,2048,4096,8192,0,0};
static xaUBYTE gif_buff[MAXVALP];
static xaULONG gif_block_size;
static xaLONG gif_bits,num_bits;
static xaULONG gif_lace_flag;

static xaLONG gif_max_imagec,gif_max_imagex,gif_max_imagey,gif_max_imaged;
static xaULONG gif_imagex,gif_imagey,gif_imagec,gif_imaged;
static xaULONG gif_screenx,gif_screeny;

static xaLONG pic_i,pic_size;

/* GIF89a variables */
static xaLONG gif_anim_type;  /* from GIF89 f9 extension */
static xaLONG gif_anim_time;  /* from GIF89 f9 extension */
static xaULONG gif_anim_mask;  /* from GIF89 f9 extension */

static GIF_FRAME *gif_frame_start,*gif_frame_cur;
static xaULONG gif_frame_cnt;

GIF_FRAME *GIF_Add_Frame(time,act)
xaULONG time;
XA_ACTION *act;
{
  GIF_FRAME *gframe;

  gframe = (GIF_FRAME *) malloc(sizeof(GIF_FRAME));
  if (gframe == 0) TheEnd1("GIF_Add_Frame: malloc err");

  gframe->time = time;
  gframe->act = act;
  gframe->next = 0;
 
  if (gif_frame_start == 0) gif_frame_start = gframe;
  else gif_frame_cur->next = gframe;
 
  gif_frame_cur = gframe;
  gif_frame_cnt++;
  return(gframe);
}

void
GIF_Free_Frame_List(gframes)
GIF_FRAME *gframes;
{
  GIF_FRAME *gtmp;
  while(gframes != 0)
  {
    gtmp = gframes;
    gframes = gframes->next;
    FREE(gtmp,0x3000);
  }
}

xaULONG
GIF_Read_Anim(const char *fname,XA_ANIM_HDR *anim_hdr)
{
  GIF_FRAME *glist,*gtmp;
  xaULONG frame_cnt,i;
  xaLONG t_time = 0;

  glist = GIF_Read_File(fname,anim_hdr,&frame_cnt);
  if (glist==0) return(xaFALSE);

  anim_hdr->frame_lst = (XA_FRAME *)malloc(sizeof(XA_FRAME) * (frame_cnt+1));
  if (anim_hdr->frame_lst == NULL)
       TheEnd1("GIF_Read_Anim: malloc err");

  gtmp = glist;
  i = 0;
  while(gtmp != 0)
  {
    if (i >= frame_cnt)
    {
      fprintf(stderr,"GIF_Read_Anim: frame inconsistency %d %d\n",
		i,frame_cnt);
      break;
    }
    anim_hdr->frame_lst[i].time_dur = gtmp->time;
    anim_hdr->frame_lst[i].zztime = t_time;
    t_time += gtmp->time;
    anim_hdr->frame_lst[i].act = gtmp->act;
    gtmp = gtmp->next;
    i++;
  }
  if (i > 0) anim_hdr->total_time = anim_hdr->frame_lst[i-1].zztime
				+ anim_hdr->frame_lst[i-1].time_dur;
  else anim_hdr->total_time = 0;
  anim_hdr->frame_lst[i].time_dur = 0;
  anim_hdr->frame_lst[i].zztime = -1;
  anim_hdr->frame_lst[i].act  = 0;
  anim_hdr->loop_frame = 0;
  if (i > 0) anim_hdr->last_frame = i - 1;
  else i = 0;

  GIF_Free_Frame_List(glist);
  return(xaTRUE);
}

/*
 *  Pad a given byte-based image buffer
 */
xaUBYTE *GIF_Pad_Byteimage(data, xoff, yoff, srcw, srch,
			   destw, desth, pad_byte)
xaUBYTE *data;
xaLONG xoff;
xaLONG yoff;
xaLONG srcw;
xaLONG srch;
xaLONG destw;
xaLONG desth;
xaUBYTE pad_byte;
{
  xaUBYTE *new_data;
  xaULONG y;

  /* Check extents. */
  if ((xoff < 0) || (yoff < 0) ||
      (srcw+xoff > destw) || (srch+yoff > desth))
      TheEnd1("GIF_Pad_Byteimage: Image limits extended beyond padded rgn\n");

  new_data = (xaUBYTE *)malloc(destw*desth);
  if (new_data == 0) TheEnd1("GIF_Pad_Byteimage: Image malloc failed\n");

  memset(new_data, pad_byte, destw*desth);

  for (y=0; y<srch; y++)
    {
      memcpy((char *)&new_data[(y+yoff)*destw +xoff],
	     (char *)&data[y*srcw], srcw);
    }

  return (new_data);
}

/*
 *
 */
GIF_FRAME *GIF_Read_File(const char *fname,XA_ANIM_HDR *anim_hdr,xaULONG *frame_cnt)
{
  XA_INPUT *xin = anim_hdr->xin;
  xaLONG i,exit_flag;
  XA_ACTION *act;
  xaUBYTE current_disposal = 2;

  gif_frame_cnt = 0;
  gif_frame_start = gif_frame_cur = 0;
  gif_imagex = gif_max_imagex = 0;
  gif_imagey = gif_max_imagey = 0;
  gif_imagec = gif_max_imagec = 0;
  gif_imaged = gif_max_imaged = 0;
  gif_screenx = 0;
  gif_screeny = 0;

  gif_anim_type = 0;
  gif_anim_time = XA_GET_TIME(100); /* 100ms is 10 fps */
  gif_anim_mask = 0;

  GIF_Screen_Header(xin,anim_hdr);

  /*** read until  ,  separator */
  do
  {
    i=xin->Read_U8(xin);
    if ( (i<0) && xin->At_EOF(xin,-1))
    {
      xin->Close_File(xin);
      fprintf(stderr,"GIF_Read_Header: Unexpected EOF\n");
      return(0);
    }
    if (i == '!') GIF_Read_Extension(xin);  /* GIF extension */
  } while(i != ',');

  exit_flag = 0;
  while(!exit_flag)
  {
    xaUBYTE *pic_ptr;
  
    /* Read Image Header
     */
    GIF_Image_Header(xin,anim_hdr);

    /*** Setup ACTION for IMAGE or CLIP */
    act = ACT_Get_Action(anim_hdr,ACT_MAPPED);

    pic_size = gif_imagex * gif_imagey;

    pic_ptr = (xaUBYTE *) malloc( XA_PIC_SIZE(pic_size) );
    if (pic_ptr==0) TheEnd1("GIF_Read_File: failed malloc of image\n");

    pic_i=0;
    GIF_Decompress(xin,pic_ptr);

    /*
    printf("%d (%d): t%d cur %d\n",gif_anim_type, ((gif_anim_type>>2) & 7),
	   gif_anim_mask,current_disposal);
    */

    if (gif_anim_type & 0x01) /* 'has transparency' bit */
      {
	switch(current_disposal) /* disposal */
	  {
	  case 3: /* 'previous' disposal - should restore previous frame,
		     but for now we just fall through... */
	  case 0: /* 'don't care' disposal. */
	  case 1: /* 'combine' disposal. */
	    ACT_Setup_Mapped(act, pic_ptr, gif_chdr_now,
			     gifimage.left,gifimage.top,gif_imagex,gif_imagey,
			     gif_screenx,gif_screeny,
			     xaTRUE,gif_anim_mask,
			     xaTRUE,xaFALSE,xaFALSE);
	    break;
	  case 2: /* 'replace' disposal (clears frame before drawing) */
	  default:
	    {
	      xaUBYTE *padded_pic_ptr;
	      
	      padded_pic_ptr = GIF_Pad_Byteimage(pic_ptr,
						 gifimage.left, gifimage.top,
						 gif_imagex, gif_imagey,
						 gif_screenx, gif_screeny,
						 gif_anim_mask);
	      free(pic_ptr);
	      pic_ptr = padded_pic_ptr;
	      ACT_Setup_Mapped(	act, pic_ptr, gif_chdr_now,
				0,0,
				gif_screenx,gif_screeny,
				gif_screenx,gif_screeny,
				xaFALSE,0,
				xaTRUE,xaFALSE,xaFALSE);
	    }
	    break;
	  }
      }
    else /* not a transparent frame */
    {
       ACT_Setup_Mapped(act, pic_ptr, gif_chdr_now,
			gifimage.left,gifimage.top,gif_imagex,gif_imagey,
			gif_screenx,gif_screeny,
			xaFALSE,0,
			xaTRUE,xaFALSE,xaFALSE);
    }
    ACT_Add_CHDR_To_Action(act,gif_chdr_now);
    GIF_Add_Frame(gif_anim_time,act);

    current_disposal = (gif_anim_type>>2) & 7;

    /*** read until "," ";" or feof */ 
    do
    {
      i=xin->Read_U8(xin);
      if (xin->At_EOF(xin,-1))	exit_flag = 1;
      else if (i == ';') exit_flag = 1;
      else if (i == '!') GIF_Read_Extension(xin);  /* extension */
    } while( (i != ',') && (!exit_flag) );

  } /*** end of while images */
  xin->Close_File(xin);

  anim_hdr->imagex = gif_max_imagex;
  anim_hdr->imagey = gif_max_imagey;
  anim_hdr->imagec = gif_max_imagec;
  anim_hdr->imaged = gif_max_imaged;
  *frame_cnt = gif_frame_cnt;
  return(gif_frame_start);
}

xaULONG GIF_Decompress(xin,pic)
XA_INPUT *xin;
char *pic;
{
  register xaULONG code,old;

  gif_bits = 0;
  num_bits=0;
  gif_block_size=0;
     /* starting code size of LZW */
  root_code_size=(xin->Read_U8(xin) & 0xff); 
  DEBUG_LEVEL3 fprintf(stderr,"  root code size = %d\n",root_code_size);
  GIF_Clear_Table();  /* clear decoding symbol table */

  code=GIF_Get_Code(xin);

  if (code==CLEAR) 
  {
    GIF_Clear_Table(); 
    code=GIF_Get_Code(xin);
  }
  /* write code(or what it currently stands for) to file */
  if (GIF_Send_Data(code,pic) == xaFALSE) return(xaFALSE);
  old=code;
  code=GIF_Get_Code(xin);
  do
  {
    if (table[code].valid == 1)    /* if known code */
    {
       /* send it's associated string to file */
      if (GIF_Send_Data(code,pic)==xaFALSE) return(xaFALSE);
	/* get next table entry (nextab) */
      if (GIF_Get_Next_Entry(xin) == xaFALSE) return(xaFALSE);
      GIF_Add_To_Table(old,code,nextab);  /* add old+code to table */
      old=code;
    }
    else      /* code doesn't exist */
    {
      GIF_Add_To_Table(old,old,code);   /* add old+old to table */
      if (GIF_Send_Data(code,pic) == xaFALSE) return(xaFALSE);
      old=code;
    }
    code=GIF_Get_Code(xin);
    if (code==CLEAR)
    { 
      GIF_Clear_Table();
      code=GIF_Get_Code(xin);
      if (GIF_Send_Data(code,pic) == xaFALSE) return(xaFALSE);
      old=code;
      code=GIF_Get_Code(xin);
    }
  } while(code!=EOI);
  return(xaTRUE);
}

xaULONG GIF_Get_Next_Entry(xin)
XA_INPUT *xin;
{
    /* table walk to empty spot */
  while( (table[nextab].valid==1) && (nextab<MAXVAL) ) nextab++;

  /* Ran out of space??  Something's roached */
  if (nextab>=MAXVAL)    
  { if ( xin->At_EOF(xin) ) 
	{ fprintf(stderr,"truncated file\n"); return(xaFALSE); }
    else 
    { /* GIF_Clear_Table(); */
      nextab = MAXVAL - 1;
    }
  }
  if (nextab == INCSIZE)   /* go to next table size (and LZW code size ) */
  {
    DEBUG_LEVEL4 fprintf(stderr,"GetNext INCSIZE was %d ",nextab);
    code_size++; INCSIZE=(INCSIZE*2)+1;
    if (code_size>=12) code_size=12;
    DEBUG_LEVEL4 fprintf(stderr,"<%d>",INCSIZE);
  }
  return(xaTRUE);
}


/*  body is associated string
 *  next is code to add to that string to form associated string for
 *  index
 */     
void GIF_Add_To_Table(body,next,index)
register xaULONG body,next,index;
{
  if (index>MAXVAL)
  { 
    fprintf(stderr,"Error index=%d\n",index);
  }
  else
  {
    table[index].valid=1;
    table[index].data=table[next].first;
    table[index].first=table[body].first;
    table[index].last=body;
  }
}

xaULONG GIF_Send_Data(index,pic)
register xaLONG index;
char *pic;
{
  register xaLONG i,j;
  i=0;
  do         /* table walk to retrieve string associated with index */
  { 
    gif_buff[i] = table[index].data; 
    i++;
    index = table[index].last;
    if (i>MAXVAL)
    { fprintf(stderr,"Error: Sending i=%d index=%d\n",i,index);
      return(xaFALSE);
    }
  } while(index >= 0);

  /* now invert that string since we retreived it backwards */
  i--;
  for(j=i;j>=0;j--) 
  {
    if (pic_i < pic_size) pic[pic_i++] = gif_buff[j];
    else 
    {
       DEBUG_LEVEL1 
	fprintf(stderr,"GIF_Send: data beyond image size - ignored\n");
    }
    if (gif_lace_flag)
    {
      if ((pic_i % gif_imagex) == 0 )
      { 
        switch(gif_lace_flag)
        {
          case 1: pic_i += gif_imagex * 7; break; /* fill every 8th row */
          case 2: pic_i += gif_imagex * 7; break; /* fill every 8th row */
          case 3: pic_i += gif_imagex * 3; break; /* fill every 4th row */
          case 4: pic_i += gif_imagex    ; break; /* fill every other row */
        }
      }
      if (pic_i >= pic_size)
      {
        gif_lace_flag++;
        switch(gif_lace_flag)
        {
          case 2: pic_i = gif_imagex << 2; break;  /* start at 4th row */
          case 3: pic_i = gif_imagex << 1; break;  /* start at 2nd row */
          case 4: pic_i = gif_imagex;      break;  /* start at 1st row */
          default: gif_lace_flag = 0; pic_i = 0; break;
        }
      }
    } /*** end of if gif_lace_flag */
  } /*** end of code expansion */
  return(xaTRUE);
}


/* 
 * initialize string table 
 */
void GIF_Init_Table()       
{
  register xaLONG maxi,i;

  DEBUG_LEVEL3 fprintf(stderr,"  Initing Table...");
  maxi=gif_ptwo[root_code_size];
  for(i=0; i<maxi; i++)
  {
    table[i].data=i;   
    table[i].first=i;
    table[i].valid=1;  
    table[i].last = -1;
  }
  CLEAR=maxi; 
  EOI=maxi+1; 
  nextab=maxi+2;
  INCSIZE = (2*maxi)-1;
  code_size=root_code_size+1;
  DEBUG_LEVEL3 fprintf(stderr,"done\n");
}


/* 
 * clear table 
 */
void GIF_Clear_Table()   
{
 register xaLONG i;
DEBUG_LEVEL3 fprintf(stderr,"  Clearing Table...\n");
 for(i=0;i<MAXVAL;i++) table[i].valid=0;
 GIF_Init_Table();
}


/*CODE*/
xaULONG GIF_Get_Code(xin) /* get code depending of current LZW code size */
XA_INPUT *xin;
{
  xaULONG code;
  register xaULONG tmp;

  while(num_bits < code_size)
  {
    /**** if at end of a block, start new block */
    if (gif_block_size==0)
    {
      tmp = xin->Read_U8(xin);
      if (tmp > 0 ) gif_block_size=(xaULONG)(tmp);
      else { fprintf(stderr,"GIF: No EOF before EOI\n"); return(EOI); }
      DEBUG_LEVEL3 fprintf(stderr,"New Block size=%d\n",gif_block_size);
    }

    tmp = xin->Read_U8(xin);   gif_block_size--;
    if (tmp >= 0)
    {
      gif_bits |= ( ((xaULONG)(tmp) & (xaULONG)(0xff)) << num_bits );
      DEBUG_LEVEL3 
       fprintf(stderr,"tmp=%x bits=%x num_bits=%d\n",tmp,gif_bits,num_bits);

      num_bits+=8;
    }
    else {fprintf(stderr,"GIF: No EOF before EOI\n"); return(EOI); }
  }

  code = gif_bits & gif_mask[code_size];
  gif_bits >>= code_size;  
  num_bits -= code_size;
  DEBUG_LEVEL3 fprintf(stderr,"code=%x \n",code);

  if (code>MAXVAL)
  {
    fprintf(stderr,"\nError! in stream=%x \n",code);
    fprintf(stderr,"CLEAR=%x INCSIZE=%x EOI=%x code_size=%x \n",
                                           CLEAR,INCSIZE,EOI,code_size);
    code=EOI;
  }

  if (code==INCSIZE)
  {
    DEBUG_LEVEL3 fprintf(stderr,"  code=INCSIZE(%d)\n",INCSIZE);
    if (code_size<12)
    {
      code_size++; INCSIZE=(INCSIZE*2)+1;
    }
    else DEBUG_LEVEL3 fprintf(stderr,"  <13?>\n");
    DEBUG_LEVEL3 fprintf(stderr,"  new size = %d\n",code_size);
  }
  return(code);
}


/* 
 * read GIF header 
 */
void GIF_Screen_Header(xin,anim_hdr)
XA_INPUT *xin;
XA_ANIM_HDR *anim_hdr;
{
 xaLONG temp,i;

 for(i=0;i<6;i++) xin->Read_U8(xin);	/* read and toss GIF87a or GIF89? */

 gif_screenx = gif_imagex = gifscrn.width  = xin->Read_LSB_U16(xin);
 gif_screeny = gif_imagey = gifscrn.height = xin->Read_LSB_U16(xin);

 temp=xin->Read_U8(xin);
 gifscrn.m       =  temp & 0x80;
 gifscrn.cres    = (temp & 0x70) >> 4;
 gifscrn.pixbits =  temp & 0x07;
 gifscrn.bc  = xin->Read_U8(xin);
 temp=xin->Read_U8(xin);
 gif_imaged = gifscrn.pixbits + 1;
 gif_imagec = gif_ptwo[gif_imaged];

 if (gif_imagex > gif_max_imagex) gif_max_imagex = gif_imagex;
 if (gif_imagey > gif_max_imagey) gif_max_imagey = gif_imagey;
 if (gif_imagec > gif_max_imagec) gif_max_imagec = gif_imagec;
 if (gif_imaged > gif_max_imaged) gif_max_imaged = gif_imaged;

 if (xa_verbose == xaTRUE)
  fprintf(stdout,"  Screen: %dx%dx%d m=%d cres=%d bkgnd=%d pix=%d\n",
    gifscrn.width,gifscrn.height,gif_imagec,gifscrn.m,gifscrn.cres,
    gifscrn.bc,gifscrn.pixbits);

 if (   (gif_imagec > x11_cmap_size) && (x11_cmap_flag == xaTRUE)
     && (x11_display_type & XA_X11_CMAP) )
 {
  fprintf(stderr,"ERROR: Image has %d colors, display can handle %d\n",
					gif_imagec,x11_cmap_size);
  TheEnd();
 }

  if (gifscrn.m)
  {
     for(i=0; i < gif_imagec; i++)
     {
       gif_cmap[i].red   = xin->Read_U8(xin);
       gif_cmap[i].green = xin->Read_U8(xin);
       gif_cmap[i].blue  = xin->Read_U8(xin);
     }

     gif_chdr_now = ACT_Get_CMAP(gif_cmap,gif_imagec,0,gif_imagec,0,8,8,8);
  }
} /* end of function */

/*
 *
 */
void GIF_Image_Header(xin,anim_hdr)
XA_INPUT *xin;
XA_ANIM_HDR *anim_hdr;
{
 xaLONG temp,i;

 gifimage.left   = xin->Read_LSB_U16(xin);
 gifimage.top    = xin->Read_LSB_U16(xin);
 gifimage.width  = xin->Read_LSB_U16(xin);
 gifimage.height = xin->Read_LSB_U16(xin);
 temp			= xin->Read_U8(xin);
 gifimage.m		= temp & 0x80;
 gifimage.i		= temp & 0x40;
 if (gifimage.i) gif_lace_flag = 1;
 else gif_lace_flag = 0;
 gifimage.pixbits  = temp & 0x07;
 gif_imaged = gifimage.pixbits + 1;
 gif_imagex = gifimage.width;
 gif_imagey = gifimage.height;
 gif_imagec = gif_ptwo[gif_imaged];

 if (gif_imagex > gif_max_imagex) gif_max_imagex = gif_imagex;
 if (gif_imagey > gif_max_imagey) gif_max_imagey = gif_imagey;
 if (gif_imagec > gif_max_imagec) gif_max_imagec = gif_imagec;
 if (gif_imaged > gif_max_imaged) gif_max_imaged = gif_imaged;

 DEBUG_LEVEL1
 {
  fprintf(stderr,"  Image: %dx%dx%d m=%d i=%d pix=%d resvd=%x",
    gif_imagex,gif_imagey,gif_imagec,gifimage.m,gifimage.i,gifimage.pixbits,
    gifimage.reserved );
  fprintf(stderr,"Pos: %dx%d \n",gifimage.left,gifimage.top);
 }
 
   /* Read in Image CMAP if Image has one */
  if (gifimage.m)
  {
    for(i=0; i < gif_imagec; i++)
    {
      gif_cmap[i].red   = xin->Read_U8(xin);
      gif_cmap[i].green = xin->Read_U8(xin);
      gif_cmap[i].blue  = xin->Read_U8(xin);
    }
    gif_chdr_now = ACT_Get_CMAP(gif_cmap,gif_imagec,0,gif_imagec,0,8,8,8);
  }
}


void
GIF_Read_Extension(xin)
XA_INPUT *xin;
{
  xaLONG block_size,code,tmp,i;

  code = xin->Read_U8(xin);
  if ( (code == 0xfe) && (xa_verbose==xaTRUE) )
	fprintf(stdout,"  GIF COMMENT EXTENSION BLOCK\n");
  block_size = xin->Read_U8(xin);

  DEBUG_LEVEL2 fprintf(stderr,"  GIF Extension: Code = %02x size = %02x\n",
							code,block_size);

  if ( (code == 0xf9) && (block_size == 4))
  {
    gif_anim_type = xin->Read_U8(xin);
    i = xin->Read_LSB_U16(xin); if (i < 0) i = 1;
    gif_anim_time = XA_GET_TIME(i * 10);
    gif_anim_mask = xin->Read_U8(xin);
    block_size = xin->Read_U8(xin);
  DEBUG_LEVEL2 fprintf(stderr,"                             size = %02x\n",
							block_size);
  }

  while(block_size > 0)
  {
    for(i=0; i<block_size; i++) 
    {
      tmp=xin->Read_U8(xin);
      if ( (code == 0xfe) && (xa_verbose==xaTRUE) ) fprintf(stdout,"%c",(char)tmp);
      if (code == 0xf9) DEBUG_LEVEL1 fprintf(stderr,"%x ",tmp);
    }
    block_size = xin->Read_U8(xin);
    if (code == 0xf9) DEBUG_LEVEL1 fprintf(stderr,"\n");
  DEBUG_LEVEL2 fprintf(stderr,"                             size = %02x\n",
							block_size);
  }
}

