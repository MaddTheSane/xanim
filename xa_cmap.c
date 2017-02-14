
/*
 * xa_cmap.c
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

extern XA_CHDR *xa_chdr_first;
extern XColor  defs[256];
extern Display       *theDisp;
extern Colormap      theCmap;

extern xaULONG cmap_use_combsort;

static XA_CHDR *cmap_332_chdr  = 0;
static XA_CHDR *cmap_gray_chdr = 0;
static double  cmap_332_gamma  = 0.0;
static double  cmap_gray_gamma = 0.0;
static double  cmap_cur_gamma = 0.0;

/* TBD move to xanim.h */
#define GAMMA_MIN 0.0001

#define CMAP_ABS(x)  (((x)<0)?(-(x)):(x))
#define CMAP_SQR(x)  ((x) * (x))

xaULONG cmap_scale[17] = { 65535, 65535, 21845, 9362,
			  4369,  2114,  1040,  516,
			   257,   128,    64,   32,
			    16,     8,     4,    2,   1};

ColorReg find_cmap[256];
ColorReg *cur_find_cmap = 0;
xaULONG find_red[256],find_green[256],find_blue[256];

/* Box structure for Median Cut algorithm */
typedef struct CMAP_Box_Struct
{
 struct CMAP_Box_Struct *next;
 xaLONG rmin,rmax,gmin,gmax,bmin,bmax;
 xaULONG last_sort;
 xaULONG *clist;
 xaULONG clist_len;
} CMAP_Box;

#define CMAP_MEDIAN_NONE  0
#define CMAP_MEDIAN_RED   1
#define CMAP_MEDIAN_GREEN 2
#define CMAP_MEDIAN_BLUE  3

int ColorComp();
int CMAP_CList_Compare();
xaULONG CMAP_Find_Closest();
xaLONG CMAP_Find_Exact();
xaLONG CMAP_CHDR_Match();
void CMAP_Remap_CHDRs();
void CMAP_Remap_CHDR();
void CMAP_Compact_Box();
xaLONG CMAP_Split_Box();
void CMAP_Histogram_CHDR();
void CMAP_CMAP_From_Clist();
void CMAP_CList_CombSort();
void CMAP_CList_CombSort_Red();
void CMAP_CList_CombSort_Green();
void CMAP_CList_CombSort_Blue();
xaULONG CMAP_Gamma_Adjust();
void CMAP_Shrink_CHDR();
void CMAP_Map_To_One();


xaLONG CMAP_Median_Cut();
CMAP_Box *CMAP_Get_Box();
void CMAP_Find_Box_Color();
int CMAP_Median_Compare_Red();
int CMAP_Median_Compare_Green();
int CMAP_Median_Compare_Blue();
xaLONG CMAP_Split_Box();

XA_CHDR *ACT_Get_CHDR();
XA_CHDR *ACT_Get_CMAP();
XA_CHDR *CMAP_Create_CHDR_From_True();

void X11_Get_Colormap();
void X11_Make_Nice_CHDR();

/*
 *
 */
int ColorComp(c1,c2)
ColorReg *c1,*c2;
{
 long val1,val2;

  val1 = (3 * c1->red) + (4 * c1->green) + (2 * c1->blue);
  val2 = (3 * c2->red) + (4 * c2->green) + (2 * c2->blue);
  if (val1 != val2) return( val2 - val1 );
  else if (c1->green != c2->green) return( c2->green - c1->green );
  else if (c1->red   != c2->red  ) return( c2->red   - c1->red   );
  else return( c2->blue - c1->blue );
}

/*
 *
 */
xaULONG CMAP_Find_Closest(t_cmap,csize,r,g,b,rbits,gbits,bbits,color_flag)
ColorReg *t_cmap;
xaULONG csize;
xaLONG r,g,b;
xaULONG rbits,gbits,bbits;
xaULONG color_flag;
{ register xaULONG i,min_diff;
  register xaULONG cmap_entry;

  if (color_flag == xaFALSE)
  {
    register xaLONG gray;

    gray  = 11 * (r * cmap_scale[rbits]);
    gray += 16 * (g * cmap_scale[gbits]);
    gray +=  5 * (b * cmap_scale[bbits]);
    gray >>= 5;
    cmap_entry = 0;
    for(i=0; i<csize; i++)
    {
      register xaULONG diff;
      diff = CMAP_ABS(gray - (xaLONG)(t_cmap[i].gray));
      if (i == 0) min_diff = diff;
  
      if (diff == 0) return(i);
      if (diff < min_diff) {min_diff = diff; cmap_entry = i;}
    }
    return(cmap_entry);
  }
  else
  {
    if (cur_find_cmap != t_cmap)
    {
      if (cur_find_cmap == 0)
      {
        for(i=0;i<256;i++)
        {
	  find_red[i]   = 11 * i * i; 
	  find_green[i] = 16 * i * i;
	  find_blue[i]  =  5 * i * i;
        }
      }
      for(i=0;i<csize;i++)
      {  
	find_cmap[i].red   = t_cmap[i].red   >> 8;
	find_cmap[i].green = t_cmap[i].green >> 8;
	find_cmap[i].blue  = t_cmap[i].blue  >> 8;
      }  
      cur_find_cmap = t_cmap;
    }
    if (rbits < 16) r *= cmap_scale[rbits];
    if (gbits < 16) g *= cmap_scale[gbits];
    if (bbits < 16) b *= cmap_scale[bbits];
    r >>= 8; g >>= 8; b >>= 8;
    cmap_entry = 0;
    for(i=0; i<csize; i++)
    {
      register xaULONG diff;
      diff  = find_red[   CMAP_ABS(r - (xaLONG)(find_cmap[i].red))  ];
      diff += find_green[ CMAP_ABS(g - (xaLONG)(find_cmap[i].green))];
      diff += find_blue[  CMAP_ABS(b - (xaLONG)(find_cmap[i].blue)) ];
      if (i == 0) min_diff = diff;
  
      if (diff == 0) return(i);
      if (diff < min_diff) {min_diff = diff; cmap_entry = i;}
    }
    return(cmap_entry);
  }
}

/*
 * return index of exact match.
 * return -1 if no match found.
 */
xaLONG CMAP_Find_Exact(cmap,coff,csize,r,g,b,gray)
ColorReg *cmap;
xaULONG coff,csize;
xaUSHORT r,g,b,gray;
{
  register xaLONG i,match;

  match = -1;
  i = csize;
  if (x11_display_type & XA_X11_GRAY)
  {
    while( (i > coff) && (match < 0) )
      { i--; if (gray == cmap[i].gray) match = i; }
  }
  else
  {
    while( (i > coff) && (match < 0) )
    { i--;
      if (   (r == cmap[i].red)
          && (g == cmap[i].green)
          && (b == cmap[i].blue)  ) match = i;
    }
  }
  return(match);
}

/*
 *
 */
xaLONG CMAP_CHDR_Match(chdr1,chdr2)
XA_CHDR *chdr1,*chdr2;
{
  ColorReg *cmap1,*cmap2;
  xaULONG i;

  if (chdr1 == chdr2) return(xaTRUE);
  if (    (chdr1->csize != chdr2->csize)
       || (chdr1->coff  != chdr2->coff ) ) return(xaFALSE);
 
  cmap1 = chdr1->cmap;
  cmap2 = chdr2->cmap;
  for(i=0; i < chdr1->csize; i++)
  {
    if (   (cmap1[i].red   != cmap2[i].red)
        || (cmap1[i].green != cmap2[i].green)
        || (cmap1[i].blue  != cmap2[i].blue)  ) return(xaFALSE);
  }
  return(xaTRUE);
}

/*
 *
 */
int CMAP_CList_Compare(pc1,pc2)
void *pc1,*pc2;
{ int *c1 = (int *)pc1;
  int *c2 = (int *)pc2;
  return( (*c1) - (*c2) );
}

void
CMAP_BitMask_CList(clist,cnum,bits)
xaULONG *clist,cnum,bits;
{
  xaULONG i,r_mask;
  if ( (bits==0) || (bits>=9) ) TheEnd1("CMAP_BitMask_CList: bad bits");
  r_mask = ((0x01 << bits) - 1);
  r_mask <<= 8 - bits;
  r_mask = (r_mask << 16) | (r_mask << 8) | r_mask;
  for(i=0;i<cnum;i++) *clist++ &= r_mask;
}

xaULONG CMAP_Compress_CList(clist,cnum)
xaULONG *clist,cnum;
{
  register xaULONG i,j,data;

  DEBUG_LEVEL2 fprintf(stderr,"CMAP_Compress_CList: start %d c %d\n",
		cnum,cmap_use_combsort);
  if (cnum == 1) return(cnum);
  /* sort color list */
  if (cmap_use_combsort == xaTRUE) CMAP_CList_CombSort(clist,cnum);
  else		qsort(clist,cnum,sizeof(xaULONG),CMAP_CList_Compare);
  /* eliminate identical entries */
  DEBUG_LEVEL2 fprintf(stderr,"CMAP_Compress_CList: sort done %d\n",cnum);
  data = clist[0];   j = 1; 
  for(i=1; i<cnum; i++)
  { if (data != clist[i])
    { data = clist[i];
      clist[j] = data;  j++; 
    }
  }
  DEBUG_LEVEL2 fprintf(stderr,"CMAP_Compress_CList: done %d\n",j);
  return(j); 
}

/*
 * return the total number of colors in all of the current chdr's
 * also return the largest csize in max_csize if not NULL.
 */
xaULONG CMAP_Color_Count(chdr,max_csize)
XA_CHDR *chdr;
xaULONG *max_csize;
{
  xaULONG cnt,max_size;
  cnt = max_size = 0;
  while(chdr) 
  {
    if (chdr->csize > max_size) max_size = chdr->csize;
    cnt += chdr->csize;
    chdr = chdr->next; }
  if (max_csize != 0) *max_csize = max_size;
  return(cnt);
}


xaULONG CMAP_Make_Clist(chdr,clist)
XA_CHDR *chdr;
xaULONG **clist;
{
  xaULONG *clst,clist_len,c_i;

	/* count colors in chdr's */
  clist_len = CMAP_Color_Count(chdr,(xaULONG *)(0));
	/* allocate room for list of all colors */
  clst = (xaULONG *)malloc(clist_len * sizeof(xaULONG));
  if (clst == 0) TheEnd1("CMAP_Make_Clist: malloc err\n");

  c_i = 0;
  while(chdr)
  {
    ColorReg *cmap;
    xaULONG i,csize,*hist,stat_size;
    
    csize = chdr->csize;
    cmap  = chdr->cmap;
    if (cmap_hist_flag == xaTRUE)
    {
      hist = (xaULONG *)malloc(csize * sizeof(xaULONG));
      if (hist == 0) TheEnd1("CMAP_Manipulate_CHDRS: hist malloc err\n");
      for(i=0;i<csize;i++) hist[i] = 0;
      CMAP_Histogram_CHDR(chdr,hist,csize,chdr->moff);
      stat_size = c_i;
    } else hist = 0;
    for(i=0; i<chdr->csize; i++)
    {
      if (    (cmap_hist_flag == xaFALSE)
          || ((cmap_hist_flag == xaTRUE) && hist[i])  )
      {
        /* note: cmap's are full 16 bits and clist is 0 r g b, 8 bits each*/
        clst[c_i]  = (xaULONG)(cmap[i].red   & 0xff00) << 8; 
        clst[c_i] |= (xaULONG)(cmap[i].green & 0xff00);
        clst[c_i] |= (xaULONG)(cmap[i].blue  & 0xff00) >> 8; 
        c_i++;
      }
    }
    if (hist) { FREE(hist,0x201); hist=0;}
    if (cmap_hist_flag == xaTRUE)
    {
      DEBUG_LEVEL2 
        fprintf(stderr,"   csize %x afta hist %d\n",
		chdr->csize,(c_i - stat_size) );
    }
    chdr = chdr->next;
  }
  *clist = clst;
  return(c_i);
}

void
CMAP_Map_To_One()
{
  XA_CHDR *new_chdr;
  xaULONG *clist,clist_len,i,wanted_csize;
  xaULONG actual_csize;

   /* NOTE: clist is malloc'd in CMAP_Make_Clist */
  clist_len = CMAP_Make_Clist(xa_chdr_start,&clist);
  DEBUG_LEVEL2 
	fprintf(stderr,"CMAP_Map_To_One: start csize = %d\n",clist_len);

  wanted_csize = x11_cmap_size;

  {
    xaULONG bits;

    bits = 7;
    while ( (clist_len >= wanted_csize) && (bits >= cmap_median_bits) )
    {
      CMAP_BitMask_CList(clist,clist_len,bits);
      clist_len = CMAP_Compress_CList(clist,clist_len);
DEBUG_LEVEL2 
  fprintf(stderr,"CMAP_Map_To_One: bit %d  csize = %d\n",bits,clist_len);
      bits--;
    }
    if (clist_len < wanted_csize) wanted_csize = clist_len;
  }

  new_chdr = ACT_Get_CHDR(1,wanted_csize,0,wanted_csize,0,xaTRUE,xaTRUE);
  if (clist_len > wanted_csize)
  {
    actual_csize = CMAP_Median_Cut(new_chdr->cmap,
				clist,clist_len,wanted_csize);
    DEBUG_LEVEL2 fprintf(stderr,"CMAP_Median_Cut: csize %d\n",actual_csize);
  }
  else
  {
    CMAP_CMAP_From_Clist(new_chdr->cmap,clist,clist_len);
    actual_csize = clist_len;
    DEBUG_LEVEL2 fprintf(stderr,"CMAP_CList: csize %d\n",actual_csize);
  }
  FREE(clist,0x202); clist=0;
  new_chdr->csize = actual_csize;
  new_chdr->coff  = x11_cmap_size - actual_csize;
  new_chdr->msize = actual_csize;
  new_chdr->moff  = x11_cmap_size - actual_csize;
  for(i=0; i<new_chdr->msize; i++) new_chdr->map[i] = i + new_chdr->moff;
  if (cmap_play_nice == xaTRUE) X11_Make_Nice_CHDR(new_chdr);
  CMAP_Remap_CHDRs(new_chdr);
  xa_chdr_first = new_chdr;
}

void
CMAP_Shrink_CHDR(old_chdr,new_csize)
XA_CHDR *old_chdr;
xaULONG new_csize;
{
  XA_CHDR *new_chdr;
  xaULONG *clist,clist_len,i,wanted_csize;
  xaULONG actual_csize;

   /* NOTE: clist is malloc'd in CMAP_Make_Clist */
  clist_len = CMAP_Make_Clist(xa_chdr_start,&clist);
  DEBUG_LEVEL2 
	fprintf(stderr,"CMAP_Shrink_CHDR: start csize = %d\n",clist_len);
  wanted_csize = new_csize;

  {
    xaULONG bits;

    bits = 7;
    while ( (clist_len >= wanted_csize) && (bits >= cmap_median_bits) )
    {
      CMAP_BitMask_CList(clist,clist_len,bits);
      clist_len = CMAP_Compress_CList(clist,clist_len);
DEBUG_LEVEL2 
  fprintf(stderr,"CMAP_Map_To_One: bit %d  csize = %d\n",bits,clist_len);
      bits--;
    }
    if (clist_len < wanted_csize) wanted_csize = clist_len;
  }

  new_chdr = ACT_Get_CHDR(1,wanted_csize,0,wanted_csize,0,xaTRUE,xaTRUE);
  if (clist_len > wanted_csize)
  {
    actual_csize = CMAP_Median_Cut(new_chdr->cmap,
				clist,clist_len,wanted_csize);
    DEBUG_LEVEL2 fprintf(stderr,"CMAP_Median_Cut: csize %d\n",actual_csize);
  }
  else
  {
    CMAP_CMAP_From_Clist(new_chdr->cmap,clist,clist_len);
    actual_csize = clist_len;
    DEBUG_LEVEL2 fprintf(stderr,"CMAP_CList: csize %d\n",actual_csize);
  }
  FREE(clist,0x202); clist=0;
  new_chdr->csize = actual_csize;
  new_chdr->coff  = x11_cmap_size - actual_csize;
  new_chdr->msize = actual_csize;
  new_chdr->moff  = x11_cmap_size - actual_csize;
  for(i=0; i<new_chdr->msize; i++) new_chdr->map[i] = i + new_chdr->moff;
  CMAP_Remap_CHDRs(new_chdr);
}

void
CMAP_Manipulate_CHDRS()
{
  xa_chdr_first = xa_chdr_start;

  if (x11_display_type == XA_MONOCHROME) return;
  if (   (x11_display_type & XA_X11_STATIC)
      || (x11_display_type & XA_X11_TRUE)   )
  {
    XA_CHDR *fixed_chdr;

    fixed_chdr = ACT_Get_CHDR(1,x11_cmap_size,0,x11_cmap_size,0,xaTRUE,xaTRUE);
    X11_Get_Colormap(fixed_chdr);
    xa_chdr_first = fixed_chdr;
    CMAP_Remap_CHDRs(fixed_chdr);
    return;
  }
  else if (   (cmap_play_nice == xaTRUE) || (cmap_map_to_one_flag == xaTRUE) 
	   || (cmap_color_func == 4) )
  { 
    CMAP_Map_To_One();
    return;
  }
  else if (cmap_map_to_1st_flag == xaTRUE)
  {
    CMAP_Remap_CHDRs(xa_chdr_start);
    return;
  }
  else if (x11_display_type != XA_MONOCHROME)
  {		/* Check for CMAPs > x11_cmap_size */
    xaULONG flag;
    XA_CHDR *tmp_chdr;

    flag = 0;
    tmp_chdr = xa_chdr_start;
    while(tmp_chdr != 0)
    {
      if (tmp_chdr->csize > x11_cmap_size)
      {
	CMAP_Shrink_CHDR(tmp_chdr,x11_cmap_size);
	if (flag == 0) xa_chdr_first = tmp_chdr->new_chdr;
      }
      tmp_chdr = tmp_chdr->next;
      flag = 1;
    }
  }
}

void
CMAP_Remap_CHDR(new_chdr,old_chdr)
XA_CHDR *new_chdr,*old_chdr;
{
  ColorReg *new_cmap,*old_cmap;
  xaULONG *tmp_map,*old_map;
  xaULONG new_csize,new_coff,old_csize,old_coff;
  xaULONG new_moff,old_msize,old_moff;
  xaULONG i,cflag;

  old_cmap  = old_chdr->cmap;
  old_csize = old_chdr->csize;
  old_coff  = old_chdr->coff;
  old_map   = old_chdr->map;
  old_msize = old_chdr->msize;
  old_moff  = old_chdr->moff;

  new_csize = new_chdr->csize;
  new_coff  = new_chdr->coff;
  new_cmap  = new_chdr->cmap;
  new_moff  = new_chdr->moff;

  tmp_map = (xaULONG *)malloc(old_msize * sizeof(xaULONG));
  if (tmp_map == 0) TheEnd1("CMAP_Remap_CHDR: map malloc err\n");

  cflag = (x11_display_type & XA_X11_COLOR)?xaTRUE:xaFALSE;
  for(i=0; i < old_msize; i++)
  {
    xaULONG j,match;
    j = i + old_moff - old_coff; /* get cmap entry for this pixel */
    if (x11_display_type & XA_X11_TRUE)
    {
      tmp_map[i] = X11_Get_True_Color(
		old_cmap[j].red,old_cmap[j].green,old_cmap[j].blue,16);
    }
    else
    {
      match = CMAP_Find_Closest( new_cmap,new_csize,
	old_cmap[j].red, old_cmap[j].green, old_cmap[j].blue,16,16,16,cflag);
      tmp_map[i] = match + new_coff; /* new pixel value */
    }
  }
  old_chdr->new_chdr = new_chdr;
  FREE(old_chdr->map,0x203);
  old_chdr->map = tmp_map;
}

void
CMAP_Remap_CHDRs(the_chdr)
XA_CHDR *the_chdr;
{
  XA_CHDR *tmp_chdr;

  DEBUG_LEVEL2 fprintf(stderr,"CMAP_Remap_CHDRs to %x\n",(xaULONG)the_chdr);

  tmp_chdr = xa_chdr_start;
  while(tmp_chdr)
  {
    if (CMAP_CHDR_Match(the_chdr,tmp_chdr) == xaFALSE)
			CMAP_Remap_CHDR(the_chdr,tmp_chdr);
    tmp_chdr = tmp_chdr->next;
  }
}

void
CMAP_CMAP_From_Clist(cmap_out,clist,clist_len)
ColorReg *cmap_out;
xaULONG *clist,clist_len;
{
  register xaULONG i,r,g,b;

  for(i=0;i<clist_len;i++)
  {
    r = (clist[i] >> 16) & 0xff;
    g = (clist[i] >> 8) & 0xff;
    b = clist[i] & 0xff;
    cmap_out[i].red   = (xaUSHORT)( r | (r << 8) );
    cmap_out[i].green = (xaUSHORT)( g | (g << 8) );
    cmap_out[i].blue  = (xaUSHORT)( b | (b << 8) );
    cmap_out[i].gray  = 
	(xaUSHORT)( (((r * 11) + (g * 16) + (b * 5) ) >> 5) * 257 );
  }
}

CMAP_Box *CMAP_Get_Box()
{
  CMAP_Box *tmp;
  tmp = (CMAP_Box *)malloc(sizeof(CMAP_Box));
  if (tmp == 0) TheEnd1("CMAP_Get_Box: malloc err\n");
  return(tmp);
}

xaLONG CMAP_Median_Cut(cmap_out,clist,clist_len,wanted_clen)
ColorReg *cmap_out;
xaULONG *clist,clist_len,wanted_clen;
{
  CMAP_Box *start_box,*box;
  xaULONG i,cur_box_num;

  DEBUG_LEVEL2 fprintf(stderr,"CMAP_Median_Cut: starting\n"); 
  /* make first box */
  start_box = CMAP_Get_Box();
  start_box->clist     = clist;
  start_box->clist_len = clist_len;
  start_box->next      = 0;
  CMAP_Compact_Box(start_box);

  cur_box_num = 1;
  while(cur_box_num < wanted_clen)
  {
    xaULONG box_i,box_num;

    /* loop through current boxes and split in half */
    box_i = 0;
    box_num = cur_box_num;
    box = start_box;
    while( box && (cur_box_num < wanted_clen) && (box_i < box_num) )
    {
      if ( CMAP_Split_Box(box) == xaTRUE ) 
      {
        cur_box_num++;
        box = box->next; /* move past new box */
      }
      box_i++;
      box = box->next;
    }
    if (box_num == cur_box_num) break; /* no boxes split */
  }

  box = start_box;
  for(i=0; i<cur_box_num; i++)
  {
    if (box)
    {
      CMAP_Find_Box_Color(&cmap_out[i],box);
      start_box = box;
      box = box->next;
      FREE(start_box,0x205);
    }
    else fprintf(stderr,"CMAP_Median_Cut: box/box_num mismatch\n");
  }
  DEBUG_LEVEL2 fprintf(stderr,"CMAP_Median_Cut: done\n");
  return(cur_box_num);
}


void
CMAP_Compact_Box(box)
CMAP_Box *box;
{
  xaLONG i;

  DEBUG_LEVEL3 fprintf(stderr,"Compacting Box %x\n",(xaULONG)box); 
  /* 256 is max+1 in 8 bit r,g,b */
  box->rmin = box->gmin = box->bmin = 256;
  box->rmax = box->gmax = box->bmax = -1;
  for(i=0; i<box->clist_len; i++)
  {
    register xaLONG r,g,b;

    b = box->clist[i];
    r = (b >> 16) & 0xff;
    g = (b >>  8) & 0xff;
    b &= 0xff;

    if (r < box->rmin) box->rmin = r;
    if (g < box->gmin) box->gmin = g;
    if (b < box->bmin) box->bmin = b;
    if (r > box->rmax) box->rmax = r;
    if (g > box->gmax) box->gmax = g;
    if (b > box->bmax) box->bmax = b;
  }
}

int CMAP_Median_Compare_Red(pc1,pc2)
void *pc1,*pc2;
{ int *c1 = (int *)pc1;
  int *c2 = (int *)pc2;
  return( ((*c1) & 0xff0000) - ((*c2) & 0xff0000) );
}

int CMAP_Median_Compare_Green(pc1,pc2)
void *pc1,*pc2;
{ int *c1 = (int *)pc1;
  int *c2 = (int *)pc2;
  return( ((*c1) & 0xff00) - ((*c2) & 0xff00) );
}

int CMAP_Median_Compare_Blue(pc1,pc2)
void *pc1,*pc2;
{ int *c1 = (int *)pc1;
  int *c2 = (int *)pc2;
  return( ((*c1) & 0xff) - ((*c2) & 0xff) );
}



xaLONG CMAP_Split_Box(box)
CMAP_Box *box;
{
  CMAP_Box *newbox;
  register xaLONG sort_type,rdif,gdif,bdif;

  rdif = box->rmax - box->rmin;
  gdif = box->gmax - box->gmin;
  bdif = box->bmax - box->bmin;
  if (box->clist_len <= 1) return(xaFALSE);

  if ((rdif >= gdif) && (rdif >= bdif)) sort_type = CMAP_MEDIAN_RED;
  else if ((gdif >= rdif) && (gdif >= bdif)) sort_type = CMAP_MEDIAN_GREEN;
  else sort_type = CMAP_MEDIAN_BLUE;

  if (box->last_sort != sort_type)
  {
    if (cmap_use_combsort == xaTRUE)
      switch(sort_type)
      {
        case CMAP_MEDIAN_RED: 
		CMAP_CList_CombSort_Red(box->clist,box->clist_len); break;
        case CMAP_MEDIAN_GREEN: 
		CMAP_CList_CombSort_Green(box->clist,box->clist_len); break;
        default: 
		CMAP_CList_CombSort_Blue(box->clist,box->clist_len); break;
      }
    else
      switch(sort_type)
      {
        case CMAP_MEDIAN_RED: qsort(box->clist,box->clist_len,
				sizeof(xaLONG),CMAP_Median_Compare_Red); break;
        case CMAP_MEDIAN_GREEN: qsort(box->clist,box->clist_len,
				sizeof(xaLONG),CMAP_Median_Compare_Green); break;
        default: qsort(box->clist,box->clist_len,
				sizeof(xaLONG),CMAP_Median_Compare_Blue); break;
      }
    box->last_sort = sort_type;
  }
  newbox = CMAP_Get_Box();
  newbox->next = box->next;
  box->next = newbox;
  /* split color list */
  newbox->clist_len = box->clist_len / 2;
  box->clist_len -= newbox->clist_len;
  newbox->clist = &box->clist[box->clist_len];
  newbox->last_sort = sort_type;
  CMAP_Compact_Box(box);
  CMAP_Compact_Box(newbox);
  return(xaTRUE);
}

/*
 * Assumes 8 bits per color component
 */
void
CMAP_Find_Box_Color(creg,box)
ColorReg *creg;
CMAP_Box *box;
{
  register xaLONG i;
  register xaULONG r,g,b,sum;

  DEBUG_LEVEL3 fprintf(stderr,"    box has %d\n",box->clist_len);
  if (cmap_median_type == CMAP_MEDIAN_SUM)
  {
    r=0; g=0; b=0; sum=0;
    for(i=0; i<box->clist_len; i++)
    {
      register xaULONG tcol;
      tcol = box->clist[i];
      r += (tcol >> 16) & 0xff;
      g += (tcol >>  8) & 0xff;
      b +=  tcol        & 0xff;
    }
    sum = box->clist_len;
    r /= sum;  g /= sum;  b /= sum;
  }
  else
  {
    r = (box->rmin + box->rmax) >> 1;
    g = (box->gmin + box->gmax) >> 1;
    b = (box->bmin + box->bmax) >> 1;
  }
  creg->red   = r * 257;
  creg->green = g * 257;
  creg->blue  = b * 257;
  creg->gray  = (((r * 11) + (g * 16) + (b * 5) ) >> 5) * 257;
}


void
CMAP_Histogram_CHDR(chdr,hist,csize,moff)
XA_CHDR *chdr;
xaULONG *hist,csize,moff;
{
  XA_ACTION *act;

  DEBUG_LEVEL2 fprintf(stderr,"Histogram for %x\n",(xaULONG)chdr);
  act = chdr->acts;
  while(act)
  {
    switch(act->type)
    {
      case ACT_DISP:
      case ACT_MAPPED:
	{
	  register xaUBYTE *p;
	  register xaULONG psize;
	  ACT_MAPPED_HDR *map_hdr;

          DEBUG_LEVEL2 fprintf(stderr,"  hist'ing act %x\n",(xaULONG)act);
	  map_hdr = (ACT_MAPPED_HDR *)act->data;
	  psize = map_hdr->xsize * map_hdr->ysize;
	  p = map_hdr->data;
	  if (p)
	  {
	    if (moff) while(psize--) hist[ (*p++)-moff ]++;
	    else while(psize--) hist[ *p++ ]++;
	  }
	}
	break;
/*
      case ACT_SETTER:
	{
	  register xaUBYTE *p;
	  register xaULONG psize;
	  ACT_MAPPED_HDR *map_hdr;
	  ACT_SETTER_HDR *setter_hdr;
	  XA_ACTION *back_act;

          DEBUG_LEVEL2 fprintf(stderr,"  hist'ing setter act %x\n",act);

	  setter_hdr = (ACT_SETTER_HDR *)act->data;
	  back_act = setter_hdr->back;
	  if (back_act)
	  {
	    map_hdr = (ACT_MAPPED_HDR *)back_act->data;
	    psize = map_hdr->xsize * map_hdr->ysize;
	    DEBUG_LEVEL2 fprintf(stderr,"psize = %d\n",psize);
	    p = map_hdr->data;
	    if (p)
	    {
	      if (moff) while(psize--) hist[ (*p++)-moff ]++;
	      else while(psize--) hist[ *p++ ]++;
	    }
	  }
	}
	break;
*/
    } /* end of switch */
    act = act->next_same_chdr;
  } /* end of while */

}

void CMAP_Expand_Maps()
{
  XA_CHDR *tmp_chdr;

  DEBUG_LEVEL2 fprintf(stderr,"CMAP_Expand_Maps\n");

  tmp_chdr = xa_chdr_start;
  while(tmp_chdr)
  {
    register xaULONG i,msize,d,*map;

    msize = tmp_chdr->msize;
    map   = tmp_chdr->map;
    if (map)
      for(i=0;i<msize;i++)
      {
        d = map[i] & 0xff;
	map[i] =  (d<<24) | (d<<16) | (d<<8) | d;
      }
    tmp_chdr = tmp_chdr->next;
  }
}

void
CMAP_Cache_Init(flag)
xaULONG flag;
{
  cmap_cache_bits = cmap_median_bits;
  if (cmap_cache_bits > CMAP_CACHE_MAX_BITS)
                cmap_cache_bits = CMAP_CACHE_MAX_BITS;
  cmap_cache_size = 0x01 << (cmap_cache_bits * 3);
  cmap_cache = (xaUSHORT *)malloc(cmap_cache_size * sizeof(xaUSHORT));
  cmap_cache_bmask = ( (0x01 << cmap_cache_bits) - 1) << 8;
  cmap_cache_gmask =  cmap_cache_bmask << cmap_cache_bits;
  cmap_cache_rmask =  cmap_cache_gmask << cmap_cache_bits;
  if (cmap_cache == 0) TheEnd1("CMAP_CACHE: malloc err");
  cmap_cache_chdr = 0;
  if (flag == 1)
  {
    cmap_cache2 = (xaUSHORT *)malloc(cmap_cache_size * sizeof(xaUSHORT));
    if (cmap_cache2 == 0) TheEnd1("CMAP_CACHE: malloc err2");
  }
  DEBUG_LEVEL3 fprintf(stderr,"cache mask's %x %x %x\n",
			cmap_cache_rmask,cmap_cache_gmask,cmap_cache_bmask);
}
/*
 * Set CMAP cache to all 0xffff's. Since CMAP's are currently limited
 * to 256 in size, this is a non-valid value.
 */
void
CMAP_Cache_Clear()
{
  register xaUSHORT *tp;
  register xaULONG i;
  tp = cmap_cache; i = cmap_cache_size;
  while(i--) *tp++ = 0xffff;
}


xaULONG CMAP_Get_Or_Mask(cmap_size)
xaULONG cmap_size;   /* number of colors in anim cmap */
{
  xaULONG or_mask,imagec;

  /* if image is getting remapped then this doesn't matter */
  if (   (cmap_play_nice == xaTRUE)
      || (x11_display_type & XA_X11_TRUE)
      || (x11_display_type & XA_X11_STATIC)
      || (x11_display_type == XA_MONOCHROME) ) return(0);
 
  
  if (cmap_size >= x11_cmap_size) return(0);  /* needs to be quant'd */

  /* find largest power of 2 <= display's cmap size */
  or_mask = 0x01;
  while(or_mask <= x11_cmap_size) or_mask <<= 1;
  or_mask >>=1;

  /* round cmap_size up to nearest power of two */
  imagec = 0x01;
  while(imagec <= cmap_size) imagec <<= 1;
  imagec >>=1;

  if (imagec >= or_mask) return(0);
  return(or_mask - imagec);
}


/*
 *
 * affects global variables                            
 *   iff_r_shift
 *   iff_g_shift 
 *   iff_b_shift
 *   iff_r_mask 
 *   iff_g_mask 
 *   iff_b_mask
 *
 *   Creates 332 CHDR if doesn't exist.
 *   copies cmap of 332 CHDR into given cmap.
 *   sets csize to 332 CHDR size.
 *
 */ 
XA_CHDR *CMAP_Create_332(cmap,csize)
ColorReg *cmap;
xaULONG *csize;
{
  xaULONG i,size;

  if ( (cmap_332_chdr == 0) || (cmap_cur_gamma != cmap_332_gamma) )
  {
    xaULONG r_bits,g_bits,b_bits,last,disp_bits;
    xaULONG r_scale,g_scale,b_scale;

    if (x11_display_type == XA_MONOCHROME) { size = 256; disp_bits = 8; }
    else
    { int xsize = (x11_cmap_size > 256)?(256):(x11_cmap_size);
      size = 0x01; disp_bits = 0;
      while(size <= xsize) { size <<= 1; disp_bits++; }
      size >>=1; disp_bits--;
    }
    r_bits = 3; g_bits = 3; b_bits = 3; last = 2;
    while( (r_bits + g_bits + b_bits) > disp_bits)
    {
      switch(last)
      {
        case 0:  g_bits--; last++; break;
        case 1:  r_bits--; last++; break;
        default: b_bits--; last=0; break;
      }
    }
    xa_b_shift = 16 - b_bits;
    xa_b_mask = ((0x01 << b_bits) - 1) << (16 - b_bits);

    xa_g_shift = xa_b_shift - g_bits;
    xa_g_mask = ((0x01 << g_bits) - 1) << (16 - g_bits);

    xa_r_shift = xa_g_shift - r_bits;
    xa_r_mask = ((0x01 << r_bits) - 1) << (16 - r_bits);

    DEBUG_LEVEL3
    {
      fprintf(stderr,"CMAP_Create_332: %d: %d %d %d\n",
	disp_bits,r_bits,g_bits,b_bits);
      fprintf(stderr,"CMAP_Create_332: masks: %x %x %x\n",
	xa_r_mask,xa_g_mask,xa_b_mask);
      fprintf(stderr,"CMAP_Create_332: shifts: %x %x %x\n",
	xa_r_shift,xa_g_shift,xa_b_shift);
    }
    i = r_bits + g_bits + b_bits;
    *csize = size = 0x01 << i;

    r_scale = cmap_scale[r_bits];
    g_scale = cmap_scale[g_bits];
    b_scale = cmap_scale[b_bits];

    {
      register xaULONG rmsk,gmsk,bmsk,rshft,gshft;

      gshft = b_bits;     rshft = b_bits + g_bits;
      bmsk =  (0x01 << b_bits) - 1;
      gmsk = ((0x01 << g_bits) - 1) << gshft;
      rmsk = ((0x01 << r_bits) - 1) << rshft;

      for(i=0;i<size;i++)
      {
        xaULONG r,g,b;
        cmap[i].red   = r = ( (i & rmsk) >> rshft ) * r_scale;
        cmap[i].green = g = ( (i & gmsk) >> gshft ) * g_scale;
        cmap[i].blue  = b = (  i & bmsk           ) * b_scale;
        cmap[i].gray = ((r * 11) + (g * 16) + (b * 5)) >> 5;
      }
    }
    cmap_332_chdr = ACT_Get_CMAP(cmap,size,0,size,0,16,16,16);
    cmap_332_gamma = cmap_cur_gamma;
  } /* end of create cmap_332_chdr */
  else
  {
    ColorReg *cmap332;
    *csize = size = cmap_332_chdr->csize; 
    cmap332 = cmap_332_chdr->cmap;
    for(i=0;i<size;i++)
    {
      cmap[i].red   = cmap332[i].red;
      cmap[i].green = cmap332[i].green;
      cmap[i].blue  = cmap332[i].blue;
      cmap[i].gray  = cmap332[i].gray; 
    }
  }
  return(cmap_332_chdr);
}

XA_CHDR *CMAP_Create_Gray(cmap,csize)
ColorReg *cmap;
xaULONG *csize;
{
  xaULONG i,size;


  if ( (cmap_gray_chdr == 0) || (cmap_cur_gamma != cmap_gray_gamma) )
  {
    xaULONG disp_bits;
    xaULONG g_scale;
    /* find number of bits in display or use 256 if monochrome */
    if (x11_display_type == XA_MONOCHROME) { disp_bits = 8; size = 256; }
    else
    { int xsize = (x11_cmap_size > 256)?(256):(x11_cmap_size);
      size = 0x01; disp_bits = 0;
      while(size <= xsize) { size <<= 1; disp_bits++; }
      size >>=1; disp_bits--;
    }
    xa_gray_bits = disp_bits;
    xa_gray_shift = 16 - disp_bits;

    DEBUG_LEVEL3 fprintf(stderr,"Gray: bits %d shift %d\n",
					disp_bits,xa_gray_shift);
    g_scale = cmap_scale[disp_bits];

    *csize = size;
    for(i=0;i<size;i++) cmap[i].red = cmap[i].green = cmap[i].blue
	  = cmap[i].gray = i * g_scale;
    cmap_gray_chdr = ACT_Get_CMAP(cmap,size,0,size,0,16,16,16);
    cmap_gray_gamma = cmap_cur_gamma;
  }
  else
  {
    *csize = size = cmap_gray_chdr->csize;
    for(i=0;i<size;i++) cmap[i].red = cmap[i].green = cmap[i].blue
	  = cmap_gray_chdr->cmap[i].gray;
  }
  return(cmap_gray_chdr);
}


/*
 *
 *
 */
XA_CHDR *CMAP_Create_CHDR_From_True(ipic,rbits,gbits,bbits,
						width,height,cmap,csize)
xaUBYTE *ipic;
xaULONG rbits,gbits,bbits;
xaULONG width,height;
ColorReg *cmap;
xaULONG *csize;
{
  XA_CHDR *new_chdr;
  xaULONG *clist,clist_len,wanted_csize;
  register xaULONG i,rshift,gshift,bshift;

/* NOTE: should make sure 2^(rbits+gbits+bbits) is < (width*height) */
/* optimize for space if so */

  if ( (rbits > 8) | (gbits > 8) | (bbits > 8) )
		TheEnd1("CMAP_Create_CHDR_From_True: bits > 24 err");

  clist_len = width * height;
  clist = (xaULONG *) malloc( clist_len * sizeof(xaULONG) );
  if (clist == 0) TheEnd1("CMAP_Create_CHDR_From_True: malloc err");

  rshift = 24 - rbits;
  gshift = 16 - gbits;
  bshift = 8  - bbits;

  for(i=0; i < clist_len; i++)
  {
    register xaULONG r,g,b;
    r = (xaULONG)(*ipic++);
    g = (xaULONG)(*ipic++);
    b = (xaULONG)(*ipic++);
    clist[i] = (r << rshift) | (g << gshift) | (b << bshift); 
  }

  wanted_csize = x11_cmap_size;

  DEBUG_LEVEL2 fprintf(stderr,"xaTRUE_CHDR: cnum = %d wanted =%d\n",
				clist_len,wanted_csize);
  CMAP_BitMask_CList(clist,clist_len,cmap_median_bits);
  clist_len = CMAP_Compress_CList(clist,clist_len);
  DEBUG_LEVEL2 fprintf(stderr,"xaTRUE_CHDR: compress cnum = %d\n",clist_len);

  if (clist_len < wanted_csize) wanted_csize = clist_len;

  new_chdr = ACT_Get_CHDR(1,wanted_csize,0,wanted_csize,0,xaTRUE,xaTRUE);
  if (clist_len > wanted_csize)
  {
    wanted_csize = 
	CMAP_Median_Cut(new_chdr->cmap,clist,clist_len,wanted_csize);
  DEBUG_LEVEL2 fprintf(stderr,"xaTRUE_CHDR: median cnum = %d\n",wanted_csize);
  }
  else
  {
    wanted_csize = clist_len;
    CMAP_CMAP_From_Clist(new_chdr->cmap,clist,clist_len);
  }
  FREE(clist,0x204); clist=0;
  *csize = wanted_csize;
  for(i=0;i<wanted_csize;i++) cmap[i] = new_chdr->cmap[i]; 
  new_chdr->csize = wanted_csize;
  new_chdr->coff  = x11_cmap_size - wanted_csize;
  new_chdr->msize = wanted_csize;
  new_chdr->moff  = x11_cmap_size - wanted_csize;
  for(i=0; i<new_chdr->msize; i++) new_chdr->map[i] = i + new_chdr->moff;
  return(new_chdr);
}

void CMAP_CList_CombSort(clist,cnum)
xaULONG *clist,cnum;
{ register xaULONG ShrinkFactor,gap,i,temp,finished;
  ShrinkFactor = 13; gap = cnum;
  do
  { finished = 1; gap = (gap * 10) / ShrinkFactor;
    if (gap < 1) gap = 1;
    if ( (gap==9) | (gap == 10) ) gap = 11;
    for(i=0; i < (cnum - gap); i++)
    { if (clist[i] < clist[i+gap])
      { temp = clist[i]; clist[i] = clist[i+gap];
        clist[i+gap] = temp;
        finished = 0;
      }
    }
  } while( !finished ||  (gap > 1) );
}

void CMAP_CList_CombSort_Red(clist,cnum)
xaULONG *clist,cnum;
{
  register xaULONG ShrinkFactor,gap,i,temp,finished;
  ShrinkFactor = 13; gap = cnum;
  do
  { finished = 1; gap = (gap * 10) / ShrinkFactor;
    if (gap < 1) gap = 1;
    if ( (gap==9) | (gap == 10) ) gap = 11;
    for(i=0; i < (cnum - gap); i++)
    { if ( (clist[i] & 0xff0000) < (clist[i+gap] & 0xff0000) )
      { temp = clist[i]; clist[i] = clist[i+gap];
        clist[i+gap] = temp;
        finished = 0;
      }
    }
  } while( !finished ||  (gap > 1) );
}

void CMAP_CList_CombSort_Green(clist,cnum)
xaULONG *clist,cnum;
{
  register xaULONG ShrinkFactor,gap,i,temp,finished;
  ShrinkFactor = 13; gap = cnum;
  do
  { finished = 1; gap = (gap * 10) / ShrinkFactor;
    if (gap < 1) gap = 1;
    if ( (gap==9) | (gap == 10) ) gap = 11;
    for(i=0; i < (cnum - gap); i++)
    { if ( (clist[i] & 0xff00) < (clist[i+gap] & 0xff00) )
      { temp = clist[i]; clist[i] = clist[i+gap];
        clist[i+gap] = temp;
        finished = 0;
      }
    }
  } while( !finished ||  (gap > 1) );
}

void CMAP_CList_CombSort_Blue(clist,cnum)
xaULONG *clist,cnum;
{
  register xaULONG ShrinkFactor,gap,i,temp,finished;
  ShrinkFactor = 13; gap = cnum;
  do
  { finished = 1; gap = (gap * 10) / ShrinkFactor;
    if (gap < 1) gap = 1;
    if ( (gap==9) | (gap == 10) ) gap = 11;
    for(i=0; i < (cnum - gap); i++)
    { if ( (clist[i] & 0xff) < (clist[i+gap] & 0xff) )
      { temp = clist[i]; clist[i] = clist[i+gap];
        clist[i+gap] = temp;
        finished = 0;
      }
    }
  } while( !finished ||  (gap > 1) );
}

xaULONG CMAP_Gamma_Adjust(gamma_adj,disp_gamma,anim_gamma)
xaUSHORT *gamma_adj;
double disp_gamma,anim_gamma;
{
  register xaULONG i;
  double pow(),t64k,d;
 
  DEBUG_LEVEL2 fprintf(stderr,"CMAP_Gamma_Adjust\n");
  if (disp_gamma < GAMMA_MIN) disp_gamma = 1.0;
  if (anim_gamma < GAMMA_MIN) anim_gamma = 1.0;

  cmap_cur_gamma = anim_gamma/disp_gamma;
  t64k = (double)(65535.0);
  for(i=0;i<256;i++)
  {
    d = (double)(i * 257)/t64k;
    gamma_adj[i] = (xaUSHORT)(0.5 + t64k * pow(d, cmap_cur_gamma));
  }
  if (disp_gamma == anim_gamma) return(xaFALSE);
  else return(xaTRUE);
}

/*
 *
 * affects global variables                            
 *   iff_r_shift
 *   iff_g_shift 
 *   iff_b_shift
 *   iff_r_mask 
 *   iff_g_mask 
 *   iff_b_mask
 *
 *   Creates 422 CHDR if doesn't exist.
 *   copies cmap of 422 CHDR into given cmap.
 *   sets csize to 422 CHDR size.
 *
 */ 
XA_CHDR *CMAP_Create_422(cmap,csize)
ColorReg *cmap;
xaULONG *csize;
{
  xaULONG i,size;
  xaULONG r_bits,g_bits,b_bits,last,disp_bits;
  xaULONG r_scale,g_scale,b_scale;
  XA_CHDR *chdr_422; 

    if (x11_display_type == XA_MONOCHROME) { size = 256; disp_bits = 8; }
    else
    {
      size = 0x01; disp_bits = 0;
      while(size <= x11_cmap_size) { size <<= 1; disp_bits++; }
      size >>=1; disp_bits--;
    }
    r_bits = 4; g_bits = 2; b_bits = 2; last = 0;
    while( (r_bits + g_bits + b_bits) > disp_bits)
    {
      switch(last)
      {
        case 0:  g_bits--; last++; break;
        case 1:  r_bits--; last++; break;
        default: b_bits--; last=0; break;
      }
    }
    xa_b_shift = 16 - b_bits;
    xa_b_mask = ((0x01 << b_bits) - 1) << (16 - b_bits);

    xa_g_shift = xa_b_shift - g_bits;
    xa_g_mask = ((0x01 << g_bits) - 1) << (16 - g_bits);

    xa_r_shift = xa_g_shift - r_bits;
    xa_r_mask = ((0x01 << r_bits) - 1) << (16 - r_bits);

    i = r_bits + g_bits + b_bits;
    *csize = size = 0x01 << i;

    r_scale = cmap_scale[r_bits];
    g_scale = cmap_scale[g_bits];
    b_scale = cmap_scale[b_bits];

    {
      register xaULONG rmsk,gmsk,bmsk,rshft,gshft;

      gshft = b_bits;     rshft = b_bits + g_bits;
      bmsk =  (0x01 << b_bits) - 1;
      gmsk = ((0x01 << g_bits) - 1) << gshft;
      rmsk = ((0x01 << r_bits) - 1) << rshft;

      for(i=0;i<size;i++)
      {
        xaULONG r,g,b;
        cmap[i].red   = r = ( (i & rmsk) >> rshft ) * r_scale;
        cmap[i].green = g = ( (i & gmsk) >> gshft ) * g_scale;
        cmap[i].blue  = b = (  i & bmsk           ) * b_scale;
        cmap[i].gray = ((r * 11) + (g * 16) + (b * 5)) >> 5;
      }
    }
    chdr_422 = ACT_Get_CMAP(cmap,size,0,size,0,16,16,16);
  return(chdr_422);
}


