
/*
 * xanim_gif.c
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

/* REVISIONS ***********
 * 30Mar95  Code made a little more robust to handle GIF images that
 *          violate the spec by not including an EOI and the end
 *          of the image.
 */
#include "xanim_gif.h"

ULONG GIF_Read_Anim();
GIF_FRAME *GIF_Read_File();
GIF_FRAME *GIF_Add_Frame();

LONG Is_GIF_File();
void GIF_Decompress();
void GIF_Get_Next_Entry();
void GIF_Add_To_Table();
void GIF_Send_Data();
void GIF_Init_Table();
void GIF_Clear_Table();
ULONG GIF_Get_Code();
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
LONG UTIL_Get_LSB_Short();


#define MAXVAL  4100            /* maxval of lzw coding size */
#define MAXVALP 4200


#define GIF_CMAP_SIZE 256
static GIF_Screen_Hdr gifscrn;
static GIF_Image_Hdr gifimage;
static ColorReg gif_cmap[GIF_CMAP_SIZE];
static XA_CHDR *gif_chdr_now;
static GIF_Table table[MAXVALP];

static ULONG root_code_size,code_size,CLEAR,EOI,INCSIZE;
static ULONG nextab;
static ULONG gif_mask[16] = {1,1,3,7,15,31,63,127,255,511,1023,2047,4095,8191,0,0};
static ULONG gif_ptwo[16] = {1,2,4,8,16,32,64,128,256,512,1024,2048,4096,8192,0,0};
static UBYTE gif_buff[MAXVALP];
static ULONG gif_block_size;
static LONG gif_bits,num_bits;
static gif_lace_flag;

static LONG gif_max_imagec,gif_max_imagex,gif_max_imagey,gif_max_imaged;
static ULONG gif_imagex,gif_imagey,gif_imagec,gif_imaged;
static ULONG gif_screenx,gif_screeny;

static LONG pic_i,pic_size;

/* GIF89a variables */
static LONG gif_anim_type;  /* from GIF89 f9 extension */
static LONG gif_anim_time;  /* from GIF89 f9 extension */
static LONG gif_anim_mask;  /* from GIF89 f9 extension */

static GIF_FRAME *gif_frame_start,*gif_frame_cur;
static ULONG gif_frame_cnt;

GIF_FRAME *GIF_Add_Frame(time,act)
ULONG time;
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

ULONG
GIF_Read_Anim(fname,anim_hdr)
char *fname;
XA_ANIM_HDR *anim_hdr;
{
  GIF_FRAME *glist,*gtmp;
  ULONG frame_cnt,i;
  LONG t_time = 0;

  glist = GIF_Read_File(fname,anim_hdr,&frame_cnt);
  if (glist==0) return(FALSE);

  anim_hdr->frame_lst = (XA_FRAME *)malloc(sizeof(XA_FRAME) * (frame_cnt+1));
  if (anim_hdr->frame_lst == NULL)
       TheEnd1("GIF_Read_Anim: malloc err");

  gtmp = glist;
  i = 0;
  while(gtmp != 0)
  {
    if (i >= frame_cnt)
    {
      fprintf(stderr,"GIF_Read_Anim: frame inconsistency %ld %ld\n",
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
  return(TRUE);
}

/*
 *
 */
GIF_FRAME *GIF_Read_File(fname,anim_hdr,frame_cnt)
char *fname;
XA_ANIM_HDR *anim_hdr;
ULONG *frame_cnt;
{
  FILE *fp;
  LONG i,exit_flag;
  XA_ACTION *act;

  gif_frame_cnt = 0;
  gif_frame_start = gif_frame_cur = 0;
  gif_imagex = gif_max_imagex = 0;
  gif_imagey = gif_max_imagey = 0;
  gif_imagec = gif_max_imagec = 0;
  gif_imaged = gif_max_imaged = 0;
  gif_screenx = 0;
  gif_screeny = 0;

  if ( (fp=fopen(fname,XA_OPEN_MODE))==0)
  { 
    fprintf(stderr,"Can't open %s for reading.\n",fname); 
    return(0);
  }

  gif_anim_type = 0;
  gif_anim_time = XA_GET_TIME(100); /* 100ms is 10 fps */
  gif_anim_mask = 0;

  GIF_Screen_Header(fp,anim_hdr);

  /*** read until  ,  separator */
  do
  {
    i=fgetc(fp);
    if ( (i<0) && feof(fp))
    {
      fclose(fp);
      fprintf(stderr,"GIF_Read_Header: Unexpected EOF\n");
      return(0);
    }
    if (i == '!') GIF_Read_Extension(fp);  /* GIF extension */
  } while(i != ',');

  exit_flag = 0;
  while(!exit_flag)
  {
    UBYTE *pic_ptr;
  
    /* Read Image Header
     */
    GIF_Image_Header(fp,anim_hdr);

    /*** Setup ACTION for IMAGE or CLIP */
    act = ACT_Get_Action(anim_hdr,ACT_MAPPED);

    pic_size = gif_imagex * gif_imagey;

    pic_ptr = (UBYTE *) malloc( XA_PIC_SIZE(pic_size) );
    if (pic_ptr==0) TheEnd1("GIF_Read_File: could malloc of image\n");

    pic_i=0;
    GIF_Decompress(fp,pic_ptr);

    if (gif_anim_type == 0x01)
    {
      ACT_Setup_Mapped(act, pic_ptr, gif_chdr_now,
			gifimage.left,gifimage.top,gif_imagex,gif_imagey,
			gif_screenx,gif_screeny,TRUE,gif_anim_mask,
			TRUE,FALSE,FALSE);
    }
    else
    {
       ACT_Setup_Mapped(act, pic_ptr, gif_chdr_now,
			gifimage.left,gifimage.top,gif_imagex,gif_imagey,
			gif_screenx,gif_screeny,FALSE,0,TRUE,FALSE,FALSE);
    }
    ACT_Add_CHDR_To_Action(act,gif_chdr_now);
    GIF_Add_Frame(gif_anim_time,act);

    /*** read until "," ";" or feof */ 
    do
    {
      i=fgetc(fp);
      if ( (i<0) || (i == ';')) exit_flag = 1;
      else if (i == '!') GIF_Read_Extension(fp);  /* extension */
    } while( (i != ',') && (!exit_flag) );

  } /*** end of while images */
  fclose(fp);

  anim_hdr->imagex = gif_max_imagex;
  anim_hdr->imagey = gif_max_imagey;
  anim_hdr->imagec = gif_max_imagec;
  anim_hdr->imaged = gif_max_imaged;
  *frame_cnt = gif_frame_cnt;
  return(gif_frame_start);
}

void GIF_Decompress(fp,pic)
FILE *fp;
char *pic;
{
  register ULONG code,old;

  gif_bits = 0;
  num_bits=0;
  gif_block_size=0;
     /* starting code size of LZW */
  root_code_size=(fgetc(fp) & 0xff); 
  DEBUG_LEVEL3 fprintf(stderr,"  root code size = %ld\n",root_code_size);
  GIF_Clear_Table();  /* clear decoding symbol table */

  code=GIF_Get_Code(fp);

  if (code==CLEAR) 
  {
    GIF_Clear_Table(); 
    code=GIF_Get_Code(fp);
  }
  /* write code(or what it currently stands for) to file */
  GIF_Send_Data(code,pic);   
  old=code;
  code=GIF_Get_Code(fp);
  do
  {
    if (table[code].valid == 1)    /* if known code */
    {
       /* send it's associated string to file */
      GIF_Send_Data(code,pic);
      GIF_Get_Next_Entry(fp);       /* get next table entry (nextab) */
      GIF_Add_To_Table(old,code,nextab);  /* add old+code to table */
      old=code;
    }
    else      /* code doesn't exist */
    {
      GIF_Add_To_Table(old,old,code);   /* add old+old to table */
      GIF_Send_Data(code,pic);
      old=code;
    }
    code=GIF_Get_Code(fp);
    if (code==CLEAR)
    { 
      GIF_Clear_Table();
      code=GIF_Get_Code(fp);
      GIF_Send_Data(code,pic);
      old=code;
      code=GIF_Get_Code(fp);
    }
  } while(code!=EOI);
}

void GIF_Get_Next_Entry(fp)
FILE *fp;
{
    /* table walk to empty spot */
  while( (table[nextab].valid==1) && (nextab<MAXVAL) ) nextab++;

  /* Ran out of space??  Something's roached */
  if (nextab>=MAXVAL)    
  { 
    fprintf(stderr,"Error: GetNext nextab=%ld\n",nextab);
    fclose(fp);
    TheEnd();
  }
  if (nextab == INCSIZE)   /* go to next table size (and LZW code size ) */
  {
    DEBUG_LEVEL4 fprintf(stderr,"GetNext INCSIZE was %ld ",nextab);
    code_size++; INCSIZE=(INCSIZE*2)+1;
    if (code_size>=12) code_size=12;
    DEBUG_LEVEL4 fprintf(stderr,"<%ld>",INCSIZE);
  }
}


/*  body is associated string
 *  next is code to add to that string to form associated string for
 *  index
 */     
void GIF_Add_To_Table(body,next,index)
register ULONG body,next,index;
{
  if (index>MAXVAL)
  { 
    fprintf(stderr,"Error index=%ld\n",index);
  }
  else
  {
    table[index].valid=1;
    table[index].data=table[next].first;
    table[index].first=table[body].first;
    table[index].last=body;
  }
}

void GIF_Send_Data(index,pic)
register LONG index;
char *pic;
{
  register LONG i,j;
  i=0;
  do         /* table walk to retrieve string associated with index */
  { 
    gif_buff[i] = table[index].data; 
    i++;
    index = table[index].last;
    if (i>MAXVAL)
    { 
      fprintf(stderr,"Error: Sending i=%ld index=%ld\n",i,index);
      TheEnd();
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
}


/* 
 * initialize string table 
 */
void GIF_Init_Table()       
{
  register LONG maxi,i;

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
 register LONG i;
DEBUG_LEVEL3 fprintf(stderr,"  Clearing Table...\n");
 for(i=0;i<MAXVAL;i++) table[i].valid=0;
 GIF_Init_Table();
}


/*CODE*/
ULONG GIF_Get_Code(fp) /* get code depending of current LZW code size */
FILE *fp;
{
  ULONG code;
  register LONG tmp;

  while(num_bits < code_size)
  {
    /**** if at end of a block, start new block */
    if (gif_block_size==0)
    {
      tmp = fgetc(fp);
      if (tmp >= 0 ) gif_block_size=(ULONG)(tmp);
      else {fprintf(stderr,"GIF: No EOF before EOI\n"); return(EOI); }
      DEBUG_LEVEL3 fprintf(stderr,"New Block size=%ld\n",gif_block_size);
    }

    tmp = fgetc(fp);   gif_block_size--;
    if (tmp >= 0)
    {
      gif_bits |= ( ((ULONG)(tmp) & (ULONG)(0xff)) << num_bits );
      DEBUG_LEVEL3 
       fprintf(stderr,"tmp=%lx bits=%lx num_bits=%ld\n",tmp,gif_bits,num_bits);

      num_bits+=8;
    }
    else {fprintf(stderr,"GIF: No EOF before EOI\n"); return(EOI); }
  }

  code = gif_bits & gif_mask[code_size];
  gif_bits >>= code_size;  
  num_bits -= code_size;
  DEBUG_LEVEL3 fprintf(stderr,"code=%lx \n",code);

  if (code>MAXVAL)
  {
    fprintf(stderr,"\nError! in stream=%lx \n",code);
    fprintf(stderr,"CLEAR=%lx INCSIZE=%lx EOI=%lx code_size=%lx \n",
                                           CLEAR,INCSIZE,EOI,code_size);
    code=EOI;
  }

  if (code==INCSIZE)
  {
    DEBUG_LEVEL3 fprintf(stderr,"  code=INCSIZE(%ld)\n",INCSIZE);
    if (code_size<12)
    {
      code_size++; INCSIZE=(INCSIZE*2)+1;
    }
    else DEBUG_LEVEL3 fprintf(stderr,"  <13?>\n");
    DEBUG_LEVEL3 fprintf(stderr,"  new size = %ld\n",code_size);
  }
  return(code);
}


/* 
 * read GIF header 
 */
void GIF_Screen_Header(fp,anim_hdr)
FILE *fp;
XA_ANIM_HDR *anim_hdr;
{
 LONG temp,i;

 for(i=0;i<6;i++) fgetc(fp);	/* read and toss GIF87a or GIF89? */

 gif_screenx = gif_imagex = gifscrn.width  = UTIL_Get_LSB_Short(fp);
 gif_screeny = gif_imagey = gifscrn.height = UTIL_Get_LSB_Short(fp);

 temp=fgetc(fp);
 gifscrn.m       =  temp & 0x80;
 gifscrn.cres    = (temp & 0x70) >> 4;
 gifscrn.pixbits =  temp & 0x07;
 gifscrn.bc  = fgetc(fp);
 temp=fgetc(fp);
 gif_imaged = gifscrn.pixbits + 1;
 gif_imagec = gif_ptwo[gif_imaged];

 if (gif_imagex > gif_max_imagex) gif_max_imagex = gif_imagex;
 if (gif_imagey > gif_max_imagey) gif_max_imagey = gif_imagey;
 if (gif_imagec > gif_max_imagec) gif_max_imagec = gif_imagec;
 if (gif_imaged > gif_max_imaged) gif_max_imaged = gif_imaged;

 if (xa_verbose == TRUE)
  fprintf(stderr,"  Screen: %ldx%ldx%ld m=%ld cres=%ld bkgnd=%ld pix=%ld\n",
    gifscrn.width,gifscrn.height,gif_imagec,gifscrn.m,gifscrn.cres,
    gifscrn.bc,gifscrn.pixbits);

 if (   (gif_imagec > x11_cmap_size) && (x11_cmap_flag == TRUE)
     && (x11_display_type & XA_X11_CMAP) )
 {
  fprintf(stderr,"ERROR: Image has %ld colors, display can handle %ld\n",
					gif_imagec,x11_cmap_size);
  TheEnd();
 }

  if (gifscrn.m)
  {
     for(i=0; i < gif_imagec; i++)
     {
       gif_cmap[i].red   = fgetc(fp);
       gif_cmap[i].green = fgetc(fp);
       gif_cmap[i].blue  = fgetc(fp);
     }

     gif_chdr_now = ACT_Get_CMAP(gif_cmap,gif_imagec,0,gif_imagec,0,8,8,8);
  }
} /* end of function */

/*
 *
 */
void GIF_Image_Header(fp,anim_hdr)
FILE *fp;
XA_ANIM_HDR *anim_hdr;
{
 LONG temp,i;

 gifimage.left   = UTIL_Get_LSB_Short(fp);
 gifimage.top    = UTIL_Get_LSB_Short(fp);
 gifimage.width  = UTIL_Get_LSB_Short(fp);
 gifimage.height = UTIL_Get_LSB_Short(fp);
 temp=fgetc(fp);
 gifimage.m        = temp & 0x80;
 gifimage.i        = temp & 0x40;
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
  fprintf(stderr,"  Image: %ldx%ldx%ld m=%ld i=%ld pix=%ld resvd=%lx",
    gif_imagex,gif_imagey,gif_imagec,gifimage.m,gifimage.i,gifimage.pixbits,
    gifimage.reserved );
  fprintf(stderr,"Pos: %ldx%ld \n",gifimage.left,gifimage.top);
 }
 
   /* Read in Image CMAP if Image has one */
  if (gifimage.m)
  {
    for(i=0; i < gif_imagec; i++)
    {
      gif_cmap[i].red   = fgetc(fp);
      gif_cmap[i].green = fgetc(fp);
      gif_cmap[i].blue  = fgetc(fp);
    }
    gif_chdr_now = ACT_Get_CMAP(gif_cmap,gif_imagec,0,gif_imagec,0,8,8,8);
  }
}

/*
 *
 */
LONG Is_GIF_File(filename)
char *filename;
{
 FILE *fp;
 ULONG firstword;

 if ( (fp=fopen(filename,XA_OPEN_MODE)) == 0) return(XA_NOFILE);
 /* by reading bytes we can ignore big/little endian problems */
 firstword  = (ULONG)(fgetc(fp) & 0xff) << 24;
 firstword |= (ULONG)(fgetc(fp) & 0xff) << 16;
 firstword |= (ULONG)(fgetc(fp) & 0xff) <<  8;
 firstword |= (ULONG)(fgetc(fp) & 0xff);

 fclose(fp);

 if (firstword == 0x47494638) return((LONG)TRUE);
 return((LONG)FALSE);
}

void
GIF_Read_Extension(fp)
FILE *fp;
{
  LONG block_size,code,tmp,i;

  code = fgetc(fp);
  DEBUG_LEVEL2 fprintf(stderr,"  GIF Extension: Code = %lx\n",code);
  if ( (code == 0xfe) && (xa_verbose==TRUE) )
	fprintf(stderr,"  GIF COMMENT EXTENSION BLOCK\n");
  block_size = fgetc(fp);

  if ( (code == 0xf9) && (block_size == 4))
  {
    gif_anim_type = fgetc(fp);
    i = UTIL_Get_LSB_Short(fp); if (i < 0) i = 1;
    gif_anim_time = XA_GET_TIME(i * 10);
    gif_anim_mask = fgetc(fp);
    block_size = fgetc(fp);
  }

  while(block_size > 0)
  {
    for(i=0; i<block_size; i++) 
    {
      tmp=fgetc(fp);
      if ( (code == 0xfe) && (xa_verbose==TRUE) ) fprintf(stderr,"%c",tmp);
      if (code == 0xf9) DEBUG_LEVEL1 fprintf(stderr,"%lx ",tmp);
    }
    block_size = fgetc(fp);
    if (code == 0xf9) DEBUG_LEVEL1 fprintf(stderr,"\n");
  }
}

