
/*
 * xanim_jpg.c
 *
 * Copyright (C) 1995,1995 by Mark Podlipec.
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


#include "xanim_jpg.h"


/* JPEG Specific Variables */
UBYTE  *jpg_buff = 0;
LONG  jpg_bsize = 0;

LONG   jpg_h_bnum;  /* this must be signed */
ULONG  jpg_h_bbuf;

UBYTE  *jpg_samp_limit = 0;

ULONG  jpg_init_flag = FALSE;
LONG   *jpg_quant_tables[JJ_NUM_QUANT_TBLS];
ULONG  jpg_marker = 0;
ULONG  jpg_saw_SOI,jpg_saw_SOF,jpg_saw_SOS;
ULONG  jpg_saw_DHT,jpg_saw_DQT,jpg_saw_EOI;
ULONG  jpg_std_DHT_flag = 0;
LONG   jpg_dprec,jpg_height,jpg_width;
LONG   jpg_num_comps,jpg_comps_in_scan;
LONG   jpg_nxt_rst_num;
LONG   jpg_rst_interval;

ULONG xa_mjpg_kludge;


JJ_HUFF_TBL jpg_ac_huff[JJ_NUM_HUFF_TBLS];
JJ_HUFF_TBL jpg_dc_huff[JJ_NUM_HUFF_TBLS];

COMPONENT_HDR jpg_comps[3];


#define JJ_INPUT_CHECK(val)  ((jpg_bsize >= (val))?(TRUE):(FALSE))
#define JJ_INPUT_BYTE(var) { var = *jpg_buff++; jpg_bsize--; }
#define JJ_INPUT_SHORT(var) \
 { var = (*jpg_buff++) << 8; var |= (*jpg_buff++); jpg_bsize -= 2; }

LONG JJ_ZAG[DCTSIZE2+16] = {
  0,  1,  8, 16,  9,  2,  3, 10,
 17, 24, 32, 25, 18, 11,  4,  5,
 12, 19, 26, 33, 40, 48, 41, 34,
 27, 20, 13,  6,  7, 14, 21, 28,
 35, 42, 49, 56, 57, 50, 43, 36,
 29, 22, 15, 23, 30, 37, 44, 51,
 58, 59, 52, 45, 38, 31, 39, 46,
 53, 60, 61, 54, 47, 55, 62, 63,
  0,  0,  0,  0,  0,  0,  0,  0, /* extra entries in case k>63 below */
  0,  0,  0,  0,  0,  0,  0,  0
};

ULONG jpg_MCUbuf_size = 0;
UBYTE *jpg_Ybuf = 0;
UBYTE *jpg_Ubuf = 0;
UBYTE *jpg_Vbuf = 0;
SHORT jpg_dct_buf[DCTSIZE2];


/* FUNCTIONS */
ULONG JFIF_Decode_JPEG();
ULONG jpg_read_markers();
ULONG jpg_decode_211();
ULONG jpg_decode_411();
ULONG jpg_decode_111();
void  jpg_huff_build();
ULONG jpg_huffparse();
extern void j_rev_dct();
extern void j_rev_dct_sparse();
void jpg_setup_samp_limit_table();
ULONG jpg_read_EOI_marker();
ULONG jpg_std_DHT();
void jpg_alloc_MCU_bufs();
ULONG jpg_search_marker();
ULONG jpg_read_SOF();
ULONG jpg_skip_marker();
ULONG jpg_get_marker();
void jpg_init_input();
void jpg_huff_reset();
void JFIF_Read_IJPG_Tables();
char IJPG_Tab1[64];
char IJPG_Tab2[64];
void jpg_111_spec_color();
void jpg_111_byte_color();
void jpg_111_short_color();
void jpg_111_long_color();
void jpg_211_spec_color();
void jpg_211_byte_color();
void jpg_211_short_color();
void jpg_211_long_color();
void jpg_411_spec_color();
void jpg_411_byte_color();
void jpg_411_short_color();
void jpg_411_long_color();
void jpg_411_byte_CF4();

/* external */
extern ULONG QT_Get_Color24();
extern void QT_Gen_YUV_Tabs();
extern void CMAP_Cache_Init();
extern void CMAP_Cache_Clear();
/* YUV cache tables for CVID */
extern LONG *QT_UB_tab;
extern LONG *QT_VR_tab;
extern LONG *QT_UG_tab;
extern LONG *QT_VG_tab;

/******* JFIF extern stuff and variables ***************/
LONG Is_JFIF_File();
ULONG JFIF_Read_File();
void JFIF_Read_IJPG_Tables();
XA_ACTION *ACT_Get_Action();
XA_CHDR   *ACT_Get_CMAP();
XA_CHDR *CMAP_Create_332();
XA_CHDR *CMAP_Create_Gray();
XA_CHDR *CMAP_Create_CHDR_From_True();
UBYTE *UTIL_RGB_To_Map();
UBYTE *UTIL_RGB_To_FS_Map();
ULONG UTIL_Get_LSB_Short();
void ACT_Setup_Mapped();
void ACT_Add_CHDR_To_Action();
ULONG CMAP_Find_Closest();
extern XA_ANIM_SETUP *XA_Get_Anim_Setup();
extern void XA_Free_Anim_Setup();
extern void ACT_Setup_Delta();

/*******
 *
 */
void jpg_init_stuff()
{ LONG i;
  DEBUG_LEVEL1 fprintf(stderr,"JJINIT\n");
  if (jpg_init_flag==TRUE) return;
  for(i=0; i < JJ_NUM_QUANT_TBLS; i++) jpg_quant_tables[i] = 0;
  if (jpg_samp_limit==0) jpg_setup_samp_limit_table();
  jpg_init_flag = TRUE;
  jpg_std_DHT_flag = 0;
}


/*******
 *
 */
void jpg_free_stuff()
{ LONG i;
  DEBUG_LEVEL1 fprintf(stderr,"JJFREE\n");
  if (jpg_init_flag == FALSE) return;
  for(i=0; i < JJ_NUM_QUANT_TBLS; i++)
    if (jpg_quant_tables[i]) { free(jpg_quant_tables[i]); jpg_quant_tables[i]=0; }
  if (jpg_samp_limit)	{ free(jpg_samp_limit); jpg_samp_limit = 0; }
  if (jpg_Ybuf)		{ free(jpg_Ybuf); jpg_Ybuf = 0; }
  if (jpg_Ubuf)		{ free(jpg_Ubuf); jpg_Ubuf = 0; }
  if (jpg_Vbuf)		{ free(jpg_Vbuf); jpg_Vbuf = 0; }
  jpg_init_flag = FALSE;
  jpg_std_DHT_flag = 0;
}

/****************************************************************************/
/*
 *
 */
LONG Is_JFIF_File(filename)
char *filename;
{
  FILE *fin;
  ULONG data0,data1;
  if ( (fin=fopen(filename,XA_OPEN_MODE)) == 0) return(XA_NOFILE);
  data0 = getc(fin);
  data1 = getc(fin);
  fclose(fin);
  if ((data0 == 0xFF) && (data1 == 0xD8)) return(TRUE);
  return(FALSE);
}

#define JFIF_APP0_LEN 14

/*****
 *
 */
ULONG JFIF_Read_File(fname,anim_hdr)
char *fname;
XA_ANIM_HDR *anim_hdr;
{
  FILE *fin;
  ACT_DLTA_HDR *dlta_hdr = 0;
  XA_ACTION *act = 0;
  ULONG jfif_flen;
  UBYTE *inbuff = 0;
  XA_ANIM_SETUP *jfif;

  if ( (fin=fopen(fname,XA_OPEN_MODE)) == 0)
  {
    fprintf(stderr,"can't open JFIF File %s for reading\n",fname);
    return(FALSE);
  }
  jfif = XA_Get_Anim_Setup();
  jfif->cmap_frame_num = 1; /* always 1 image per file */
  jfif->vid_time   = XA_GET_TIME( 200 );

   /* find file size */
  fseek(fin,0L,2);
  jfif_flen = ftell(fin); /* POD may not be portable */
  fseek(fin,0L,0);

  if (xa_file_flag==TRUE)
  { LONG ret;
    dlta_hdr = (ACT_DLTA_HDR *) malloc(sizeof(ACT_DLTA_HDR));
    if (dlta_hdr == 0) TheEnd1("JFIF: malloc failed");
    dlta_hdr->flags = 0;
    dlta_hdr->fpos  = ftell(fin);
    dlta_hdr->fsize = jfif_flen;
    jfif->max_fvid_size = jfif_flen + 16;
    inbuff = (UBYTE *)malloc(jfif_flen);
    ret = fread(inbuff,jfif_flen,1,fin);
    if (ret != 1) {fprintf(stderr,"JFIF: inbuff read failed\n"); return(FALSE);}
  }
  else
  { LONG ret,d;
    d = jfif_flen + (sizeof(ACT_DLTA_HDR));
    dlta_hdr = (ACT_DLTA_HDR *) malloc( d );
    if (dlta_hdr == 0) TheEnd1("JFIF: malloc failed");
    dlta_hdr->flags = DLTA_DATA;
    dlta_hdr->fpos = 0; dlta_hdr->fsize = jfif_flen;
    ret = fread( dlta_hdr->data, jfif_flen, 1, fin);
    if (ret != 1) {fprintf(stderr,"JFIF: read failed\n"); return(FALSE);}
    inbuff = dlta_hdr->data;
  }

  /******** EXTRACT IMAGE SIZE ****************/
  jpg_init_input(inbuff,jfif_flen);
  jpg_saw_SOF = FALSE;
  while(jpg_get_marker() == TRUE)
  { 
    if ( (jpg_marker == M_SOF0) || (jpg_marker == M_SOF1) )
    {
      if (jpg_read_SOF()==TRUE) jpg_saw_SOF = TRUE;
      break;
    }
    else if ((jpg_marker == M_SOI) || (jpg_marker == M_TEM)) continue;
    else if ((jpg_marker >= M_RST0) && (jpg_marker <= M_RST7)) continue;
    else jpg_skip_marker();
  }
  if (jpg_saw_SOF == FALSE)
  {
    fprintf(stderr,"JPEG: not recognized as a JPEG file\n");
    return(FALSE);
  }
  jfif->imagex = 4 * ((jpg_width + 3)/4);
  jfif->imagey = 2 * ((jpg_height + 1)/2);
  jpg_alloc_MCU_bufs(jfif->imagex);
  QT_Gen_YUV_Tabs();

  if (xa_verbose)
  {
    fprintf(stderr,"JFIF: size %ld by %ld\n",jfif->imagex,jfif->imagey);
  }

  /* Allocate Colors */
  if (jpg_num_comps == 1) /* assume grayscale */
  { int i;
    jfif->imagec = 256;	jfif->depth = 8;
    for(i=0;i<jfif->imagec;i++)
	{ jfif->cmap[i].red = jfif->cmap[i].green = jfif->cmap[i].blue = i; }
    jfif->chdr = ACT_Get_CMAP(jfif->cmap,jfif->imagec,0,jfif->imagec,0,8,8,8);
  }
  else if (jpg_num_comps == 3)
  {
    jfif->depth = 24;
    if (   (cmap_true_map_flag == FALSE) /* depth 24 and not true_map */
        || (xa_buffer_flag == FALSE) )
    {
	if (cmap_true_to_332 == TRUE)
		jfif->chdr = CMAP_Create_332(jfif->cmap,&jfif->imagec);
	else	jfif->chdr = CMAP_Create_Gray(jfif->cmap,&jfif->imagec);
    }
  }
  else
  {
    fprintf(stderr,"JFIF: unsupported number of components %ld\n",jpg_num_comps);
    return(FALSE);
  }

  act = ACT_Get_Action(anim_hdr,ACT_DELTA);
  act->data = (UBYTE *)dlta_hdr;

  dlta_hdr->xpos = dlta_hdr->ypos = 0;
  dlta_hdr->xsize = jfif->imagex;
  dlta_hdr->ysize = jfif->imagey;
  dlta_hdr->special = 0;
  dlta_hdr->extra = (void *)(0);
  dlta_hdr->delta = JFIF_Decode_JPEG;

  jfif->pic = 0; 
  ACT_Setup_Delta(jfif,act,dlta_hdr,fin);

  fclose(fin);
  /* free up buffer in necessary */
  if (xa_file_flag==TRUE) {free(inbuff); inbuff = 0; }
  
  anim_hdr->imagex = jfif->imagex;
  anim_hdr->imagey = jfif->imagey;
  anim_hdr->imagec = jfif->imagec;
  anim_hdr->imaged = 8; /* nop */
  anim_hdr->frame_lst = (XA_FRAME *) malloc( sizeof(XA_FRAME) * 2);
  anim_hdr->frame_lst[0].time_dur = jfif->vid_time;
  anim_hdr->frame_lst[0].zztime = 0;
  anim_hdr->frame_lst[0].act  = act;
  anim_hdr->frame_lst[1].time_dur = 0;
  anim_hdr->frame_lst[1].zztime = -1;
  anim_hdr->frame_lst[1].act  = 0;
  anim_hdr->loop_frame = 0;
  anim_hdr->last_frame = 0;
  anim_hdr->total_time = anim_hdr->frame_lst[0].time_dur;
  if (xa_buffer_flag == FALSE) anim_hdr->anim_flags |= ANIM_SNG_BUF;
  if (xa_file_flag == TRUE) anim_hdr->anim_flags |= ANIM_USE_FILE;
  anim_hdr->anim_flags |= ANIM_FULL_IM;
  anim_hdr->max_fvid_size = jfif->max_fvid_size;
  anim_hdr->max_faud_size = 0;
  anim_hdr->fname = anim_hdr->name;
  XA_Free_Anim_Setup(jfif);
  return(TRUE);
}

/********* END OF JFIF Code *************************************************/



/******* 
 *
 */
ULONG jpg_read_SOI()
{
  DEBUG_LEVEL1 fprintf(stderr,"SOI: \n");
  jpg_rst_interval = 0;
  return(TRUE);
}


/******* 
 *
 */
ULONG jpg_read_SOF()
{
  LONG len,ci;
  COMPONENT_HDR *comp;
  
  JJ_INPUT_SHORT(len);
  if (xa_mjpg_kludge) len -= 6;
  else len -= 8;

  JJ_INPUT_BYTE(jpg_dprec);
  JJ_INPUT_SHORT(jpg_height);
  JJ_INPUT_SHORT(jpg_width);
  JJ_INPUT_BYTE(jpg_num_comps);

  DEBUG_LEVEL1 fprintf(stderr,"SOF: dprec %lx res %ld x %ld comps %lx\n",jpg_dprec,jpg_width,jpg_height,jpg_num_comps);

  for(ci = 0; ci < jpg_num_comps; ci++)
  { ULONG c;
    comp = &jpg_comps[ci];
    JJ_INPUT_BYTE(comp->id);
    JJ_INPUT_BYTE(c);
    comp->hvsample = c;
    JJ_INPUT_BYTE(comp->qtbl_num);
    DEBUG_LEVEL1 fprintf(stderr,"     id %lx hvsamp %lx qtbl %lx\n",comp->id,c,comp->qtbl_num);
  }
  return(JJ_INPUT_CHECK(0));
}

/******* 
 *
 */
ULONG jpg_read_SOS()
{ LONG len,i;
  LONG jpg_Ss, jpg_Se, jpg_AhAl;

  JJ_INPUT_SHORT(len);
  /* if (xa_mjpg_kludge) len += 2; length ignored */

  JJ_INPUT_BYTE(jpg_comps_in_scan);

  for (i = 0; i < jpg_comps_in_scan; i++)
  { LONG j,comp_id,htbl_num;
    COMPONENT_HDR *comp = 0;

    JJ_INPUT_BYTE(comp_id);
    j = 0;
    while(j < jpg_num_comps)
    { comp = &jpg_comps[j];
      if (comp->id == comp_id) break;
      j++;
    }
    if (j > jpg_num_comps) 
	{ fprintf(stderr,"JJ: bad id %lx",comp_id); return(FALSE); }

    JJ_INPUT_BYTE(htbl_num);
    comp->dc_htbl_num = (htbl_num >> 4) & 0x0f;
    comp->ac_htbl_num = (htbl_num     ) & 0x0f;
    DEBUG_LEVEL1 fprintf(stderr,"     id %lx dc/ac %lx\n",comp_id,htbl_num);
  }
  JJ_INPUT_BYTE(jpg_Ss);
  JJ_INPUT_BYTE(jpg_Se);
  JJ_INPUT_BYTE(jpg_AhAl);
  return(JJ_INPUT_CHECK(0));
}

/******* 
 *
 */
ULONG jpg_read_DQT()
{ LONG len;
  JJ_INPUT_SHORT(len);
  if ( !xa_mjpg_kludge ) len -= 2;

  DEBUG_LEVEL1 fprintf(stderr,"DQT:\n");

  while(len > 0)
  { LONG i,tbl_num,prec;
    LONG *quant_table;

    JJ_INPUT_BYTE(tbl_num);  len -= 1;
    DEBUG_LEVEL1 fprintf(stderr,"     prec/tnum %02lx\n",tbl_num);

    prec = (tbl_num >> 4) & 0x0f;
    prec = (prec)?(2 * DCTSIZE2):(DCTSIZE2);  /* 128 or 64 */
    tbl_num &= 0x0f;
    if (tbl_num > 4)
	{ fprintf(stderr,"JJ: bad DQT tnum %lx\n",tbl_num); return(FALSE); }

    if (jpg_quant_tables[tbl_num] == 0)
    {
      jpg_quant_tables[tbl_num] = (LONG *)malloc(64 * sizeof(LONG));
      if (jpg_quant_tables[tbl_num] == 0)
	{ fprintf(stderr,"JJ: DQT alloc err %lx \n",tbl_num); return(FALSE); }
    }
    len -= prec;
    if (JJ_INPUT_CHECK(prec)==FALSE) return(FALSE);
    quant_table = jpg_quant_tables[tbl_num];
    if (prec==128)
    { ULONG tmp; 
      for (i = 0; i < DCTSIZE2; i++)
	{ JJ_INPUT_SHORT(tmp); quant_table[ JJ_ZAG[i] ] = (LONG) tmp; }
    }
    else
    { ULONG tmp; 
      for (i = 0; i < DCTSIZE2; i++)
	{ JJ_INPUT_BYTE(tmp); quant_table[ JJ_ZAG[i] ] = (LONG) tmp; }
    }
  }
  return(TRUE);
}

/******* 
 * Handle DRI chunk 
 */
ULONG jpg_read_DRI()
{ LONG len;
  JJ_INPUT_SHORT(len);
  JJ_INPUT_SHORT(jpg_rst_interval);
  DEBUG_LEVEL1 fprintf(stderr,"DRI: int %lx\n",jpg_rst_interval);
  return(TRUE);
}

/******* 
 *
 */
ULONG jpg_skip_marker()
{ LONG len,tmp;
  JJ_INPUT_SHORT(len); 
  DEBUG_LEVEL1 fprintf(stderr,"SKIP: marker %lx len %lx\n",jpg_marker,len);
  len -= 2; if (JJ_INPUT_CHECK(len)==FALSE) return(FALSE);
  while(len--) JJ_INPUT_BYTE(tmp); /* POD improve this */
  return(TRUE);
}

/*******
 *
 */
ULONG jpg_read_DHT()
{
  LONG len;
  JJ_HUFF_TBL *htable;
  UBYTE  *hbits;
  UBYTE  *hvals;

  jpg_std_DHT_flag = 0;
  JJ_INPUT_SHORT(len);
  if (xa_mjpg_kludge) len += 2;
  len -= 2;
  if (JJ_INPUT_CHECK(len)==FALSE) return(FALSE);

  while(len > 0)
  { LONG i,index,count;
    JJ_INPUT_BYTE(index);
    /* POD index check */
    if (index & 0x10)				/* AC Table */
    {
      index &= 0x0f;
      htable = &(jpg_ac_huff[index]);
      hbits  = jpg_ac_huff[index].bits;
      hvals  = jpg_ac_huff[index].vals;
    }
    else					/* DC Table */
    {
      htable = &(jpg_dc_huff[index]);
      hbits  = jpg_dc_huff[index].bits;
      hvals  = jpg_dc_huff[index].vals;
    }
    hbits[0] = 0;		count = 0;
    for (i = 1; i <= 16; i++)
    {
      JJ_INPUT_BYTE(hbits[i]);
      count += hbits[i];
    }
    len -= 17;
    if (count > 256)
	{ fprintf(stderr,"JJ: DHT bad count %ld\n",count); return(FALSE); }

    for (i = 0; i < count; i++) JJ_INPUT_BYTE(hvals[i]);
    len -= count;

    jpg_huff_build(htable,hbits,hvals);

  } /* end of len */
  return(TRUE);
}

/*******
 *
 */
ULONG jpg_get_marker()
{
  LONG c;

  for(;;)
  {
    JJ_INPUT_BYTE(c);
    while(c != 0xFF)    /* look for FF */
    {
      if (JJ_INPUT_CHECK(1)==FALSE) return(FALSE);
      JJ_INPUT_BYTE(c);
    }
    /* now we've got 1 0xFF, keep reading until next 0xFF */
    do
    {
      if (JJ_INPUT_CHECK(1)==FALSE) return(FALSE);
      JJ_INPUT_BYTE(c);
    } while (c == 0xFF);
    if (c != 0) break; /* ignore FF/00 sequences */
  }
  jpg_marker = c;
  return(TRUE);
}


/******* 
 *
 */
ULONG jpg_read_markers()
{
  for(;;)
  { 
    if (jpg_get_marker() == FALSE) return(FALSE);
DEBUG_LEVEL1 fprintf(stderr,"JJ: marker %lx\n",jpg_marker);
    switch(jpg_marker)
    {
      case M_SOI: 
	if (jpg_read_SOI()==FALSE) return(FALSE);
	jpg_saw_SOI = TRUE;
	break;
      case M_SOF0: 
      case M_SOF1: 
	if (jpg_read_SOF()==FALSE) return(FALSE);
	jpg_saw_SOF = TRUE;
	break;
      case M_SOS: 
	if (jpg_read_SOS()==FALSE) return(FALSE);
	jpg_saw_SOS = TRUE;
	jpg_nxt_rst_num = 0;
	return(TRUE);
	break;
      case M_DHT:
	if (jpg_read_DHT()==FALSE) return(FALSE);
	jpg_saw_DHT = TRUE;
	break;
      case M_DQT:
	if (jpg_read_DQT()==FALSE) return(FALSE);
	jpg_saw_DQT = TRUE;
	break;
      case M_DRI:
	if (jpg_read_DRI()==FALSE) return(FALSE);
	break;
      case M_EOI:
	fprintf(stderr,"JJ: reached EOI without data\n");
	return(FALSE);
	break;
     case M_RST0:                /* these are all parameterless */
     case M_RST1:
     case M_RST2:
     case M_RST3:
     case M_RST4:
     case M_RST5:
     case M_RST6:
     case M_RST7:
     case M_TEM:
        break;
      default:
	if (jpg_skip_marker()==FALSE) return(FALSE);
	DEBUG_LEVEL1 fprintf(stderr,"JJ: skipped marker %lx\n",jpg_marker);
	break;
    } /* end of switch */
  } /* end of forever */
}


/*****
 *
 */
void jpg_huff_build(htbl,hbits,hvals)
JJ_HUFF_TBL *htbl; 
UBYTE *hbits,*hvals;
{ ULONG clen,num_syms,p,i,si,code,lookbits;
  ULONG l,ctr;
  UBYTE huffsize[257];
  ULONG huffcode[257];

  /*** generate code lengths for each symbol */
  num_syms = 0;
  for(clen = 1; clen <= 16; clen++)
  {
    for(i = 1; i <= (ULONG)(hbits[clen]); i++) 
				huffsize[num_syms++] = (UBYTE)(clen);
  }
  huffsize[num_syms] = 0;

  /*** generate codes */
  code = 0;
  si = huffsize[0];
  p = 0;
  while (huffsize[p])
  {
    while ( ((ULONG)huffsize[p]) == si) 
    {
      huffcode[p++] = code;
      code++;
    }
    code <<= 1;
    si++;
  }

/* Init mincode/maxcode/valptr arrays */
  p = 0;
  for (l = 1; l <= 16; l++) 
  {
    if (htbl->bits[l]) 
    {
      htbl->valptr[l] = p; /* huffval[] index of 1st symbol of code length l */
      htbl->mincode[l] = huffcode[p]; /* minimum code of length l */
      p += (ULONG)(htbl->bits[l]);
      htbl->maxcode[l] = huffcode[p-1]; /* maximum code of length l */
    } 
    else
    {
      htbl->valptr[l] = 0;  /* not needed */
      htbl->mincode[l] = 0; /* not needed */
      htbl->maxcode[l] = 0; /* WAS -1; */   /* -1 if no codes of this length */
    }
  }
  htbl->maxcode[17] = 0xFFFFFL; /* ensures huff_DECODE terminates */


/* Init huffman cache */
  memset((char *)htbl->cache, 0, ((1<<HUFF_LOOKAHEAD) * sizeof(USHORT)) );
  p = 0;
  for (l = 1; l <= HUFF_LOOKAHEAD; l++) 
  {
    for (i = 1; i <= (ULONG) htbl->bits[l]; i++, p++) 
    { SHORT the_code = (USHORT)((l << 8) | htbl->vals[p]);

      /* l = current code's length, p = its index in huffcode[] & huffval[]. */
      /* Generate left-justified code followed by all possible bit sequences */

      lookbits = huffcode[p] << (HUFF_LOOKAHEAD-l);
      for (ctr = 1 << (HUFF_LOOKAHEAD-l); ctr > 0; ctr--) 
      {
        htbl->cache[lookbits] = the_code;
        lookbits++;
      }
    }
  }
}


/*******
 *
 */
void jpg_init_input(buff,buff_size)
UBYTE *buff;
LONG buff_size;
{
  jpg_buff = buff;
  jpg_bsize = buff_size;
}

/*****
 *
 */
#define JJ_CMAP_CACHE_CHECK(chdr); { if (cmap_cache == 0) CMAP_Cache_Init(0); \
  if (chdr != cmap_cache_chdr) { CMAP_Cache_Clear(); cmap_cache_chdr = chdr; \
} }


/*******
 *
 */
ULONG
JFIF_Decode_JPEG(image,delta,dsize,tchdr,map,map_flag,imagex,imagey,imaged,
                                        xs,ys,xe,ye,special,extra)
UBYTE *image;           /* Image Buffer. */
UBYTE *delta;           /* delta data. */
ULONG dsize;            /* delta size */
XA_CHDR *tchdr;         /* color map info */
ULONG *map;             /* used if it's going to be remapped. */
ULONG map_flag;         /* whether or not to use remap_map info. */
ULONG imagex,imagey;    /* Size of image buffer. */
ULONG imaged;           /* Depth of Image. (IFF specific) */
ULONG *xs,*ys;          /* pos of changed area. */
ULONG *xe,*ye;          /* size of changed area. */
ULONG special;          /* Special Info. */
void *extra;            /* extra info needed to decode delta */
{
  LONG base_y;
  XA_CHDR *chdr;
  ULONG special_flag,jpg_type;
  ULONG interleave,row_offset;

  *xs = *ys = 0; *xe = imagex; *ye = imagey;

  jpg_type = (ULONG)(extra);
  xa_mjpg_kludge = (jpg_type & 0x40)?(0x40):(0x00);
  special_flag = special & 0x0001; /* rgb */
  if (tchdr)    { chdr = (tchdr->new_chdr)?(tchdr->new_chdr):(tchdr); }
  else          chdr=0;
  if (cmap_color_func == 4) { JJ_CMAP_CACHE_CHECK(chdr); }

  if (jpg_init_flag==FALSE) jpg_init_stuff();

/* init buffer stuff */
  jpg_init_input(delta,dsize);

  base_y = 0;
  while(base_y < imagey)
  {
    jpg_saw_EOI = jpg_saw_DHT = FALSE;
    if (jpg_type & 0x10) /* IJPG File */
    { LONG ci,*qtbl0,*qtbl1;
      jpg_saw_SOI = jpg_saw_SOF = jpg_saw_SOS = jpg_saw_DQT = TRUE;
      jpg_saw_EOI = TRUE;

      if (base_y == 0) /* 1st time through */
      {
        jpg_huff_reset(); /* only  1st time */
        jpg_width  = imagex; 
        jpg_height = (imagey + 1) >> 1;	/* interleaved */
        jpg_comps_in_scan = jpg_num_comps = 3;
        jpg_rst_interval = 0;
   
        jpg_comps[0].hvsample     = 0x21;
        jpg_comps[0].ac_htbl_num = 0; 
        jpg_comps[0].dc_htbl_num = 0;
        jpg_comps[0].qtbl_num    = 0;
        jpg_comps[1].hvsample    = jpg_comps[2].hvsample    = 0x11;
        jpg_comps[1].ac_htbl_num = jpg_comps[2].ac_htbl_num = 1; 
        jpg_comps[1].dc_htbl_num = jpg_comps[2].dc_htbl_num = 1;
        jpg_comps[1].qtbl_num    = jpg_comps[2].qtbl_num    = 1;
        jpg_comps[0].id = 0;
        jpg_comps[1].id = 1;
        jpg_comps[2].id = 2;

         /* IJPG set up quant tables */
        if (jpg_quant_tables[0] == 0)
        {
          jpg_quant_tables[0] = (LONG *)malloc(64 * sizeof(LONG));
          if (jpg_quant_tables[0] == 0)
            { fprintf(stderr,"JJ: IJPG DQT0 alloc err \n"); return(FALSE); }
        }
        if (jpg_quant_tables[1] == 0)
        {
          jpg_quant_tables[1] = (LONG *)malloc(64 * sizeof(LONG));
          if (jpg_quant_tables[1] == 0)
            { fprintf(stderr,"JJ: IJPG DQT1 alloc err \n"); return(FALSE); }
        }
        qtbl0 = jpg_quant_tables[0];
        qtbl1 = jpg_quant_tables[1];
        for(ci=0; ci < DCTSIZE2; ci++)
        {
          qtbl0[ JJ_ZAG[ci] ] = (LONG)IJPG_Tab1[ci];
          qtbl1[ JJ_ZAG[ci] ] = (LONG)IJPG_Tab2[ci];
        }
      } else jpg_saw_DHT = TRUE; /* 2nd time through IJPG */
      interleave = 2;	
      row_offset = (base_y)?(1):(0);
    } else
    {
      /* read markers */
      jpg_saw_SOI = jpg_saw_SOF = jpg_saw_SOS = jpg_saw_DHT = jpg_saw_DQT = FALSE;
      if (jpg_read_markers() == FALSE) { jpg_free_stuff(); 
	fprintf(stderr,"JPG: rd marker err\n"); return(ACT_DLTA_NOP); }
      jpg_huff_reset();

      interleave = (jpg_height <= ((imagey>>1)+1) )?(2):(1);
      row_offset = ((interleave == 2) && (base_y == 0))?(1):(0);
    }
    jpg_marker = 0x00;
    if (jpg_width > imagex) jpg_alloc_MCU_bufs(jpg_width);


    if ((jpg_saw_DHT != TRUE) && (jpg_std_DHT_flag==0))
    {
      DEBUG_LEVEL1 fprintf(stderr,"standard DHT tables\n");
      jpg_std_DHT();
    }
    DEBUG_LEVEL1 fprintf(stderr,"JJ: imagexy %ld %ld  jjxy %ld %ld basey %ld\n",imagex,imagey,jpg_width,jpg_height,base_y);

    if (   (jpg_num_comps == 3) && (jpg_comps_in_scan == 3) 
	&& (jpg_comps[1].hvsample == 0x11) && (jpg_comps[2].hvsample== 0x11) )
    {
      if (jpg_comps[0].hvsample == 0x22) /* 411 */
	{ jpg_decode_411(image,jpg_width,jpg_height,interleave,row_offset,
			imagex,imagey,map_flag,map,chdr,special_flag); }
      else if (jpg_comps[0].hvsample == 0x21) /* 211 */
	{ jpg_decode_211(image,jpg_width,jpg_height,interleave,row_offset,
			imagex,imagey,map_flag,map,chdr,special_flag); }
      else if (jpg_comps[0].hvsample == 0x11) /* 111 */
	{ jpg_decode_111(image,jpg_width,jpg_height,interleave,row_offset,
			imagex,imagey,map_flag,map,chdr,special_flag); }
      else 
      { fprintf(stderr,"JPG: cmps %ld %ld mcu %04lx %04lx %04lx unsupported\n",
		jpg_num_comps,jpg_comps_in_scan,jpg_comps[0].hvsample,
		jpg_comps[1].hvsample,jpg_comps[2].hvsample); 
        break;
      }
    } 
    else if ( (jpg_num_comps == 1) || (jpg_comps_in_scan == 1) )
    {
      special_flag |= 0x02; /* grayscale */
      jpg_decode_111(image,jpg_width,jpg_height,interleave,row_offset,
			imagex,imagey,map_flag,map,chdr,special_flag);
    }
    else
    { fprintf(stderr,"JPG: cmps %ld %ld mcu %04lx %04lx %04lx unsupported.\n",
		jpg_num_comps,jpg_comps_in_scan,jpg_comps[0].hvsample,
		jpg_comps[1].hvsample,jpg_comps[2].hvsample); 
      break;
    }

    base_y += ((interleave == 1)?(imagey):(jpg_height));
    if (jpg_marker == M_EOI) { jpg_saw_EOI = TRUE; jpg_marker = 0x00; }
    else if (jpg_saw_EOI==FALSE) if (jpg_read_EOI_marker() == FALSE) break;
  }
  if (map_flag==TRUE) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}

/*******
 *
 */
void jpg_huff_reset()
{
  jpg_comps[0].dc = 0;
  jpg_comps[1].dc = 0;
  jpg_comps[2].dc = 0;
  jpg_h_bbuf = 0;  /* clear huffman bit buffer */
  jpg_h_bnum = 0;
}

/*******
 *
 */
ULONG jpg_read_EOI_marker()
{ 
  /* POD make sure previous code restores bit buffer to input stream */ 
  while( jpg_get_marker() == TRUE)
  {
    if (jpg_marker == M_EOI) {jpg_saw_EOI = TRUE; return(TRUE); }
  }
  return(FALSE);
}

/*******
 *
 */
ULONG jpg_read_RST_marker()
{
  if ( (jpg_marker >= M_RST0) && (jpg_marker <= M_RST7) ) 
  {
    DEBUG_LEVEL1 fprintf(stderr,"JJ: RST marker %lx found\n",jpg_marker);
    return(TRUE);
  }
  else
  {
    fprintf(stderr,"JJ: NON-restart marker found %lx\n",jpg_marker);
    fprintf(stderr,"JJ: should resync-to-restart\n");
  }
  return(TRUE); /* POD NOTE just for now */
}

/*******
 *
 */
void jpg_alloc_MCU_bufs(width)
ULONG width;
{ ULONG twidth = (width + 15) / 16;
  twidth <<= 2; /* two dct's deep */

  if (jpg_init_flag==FALSE) jpg_init_stuff();
  if (jpg_Ybuf == 0)
  { jpg_MCUbuf_size = twidth;
    jpg_Ybuf = (UBYTE *)malloc(twidth * DCTSIZE2);
    jpg_Ubuf = (UBYTE *)malloc(twidth * DCTSIZE2);
    jpg_Vbuf = (UBYTE *)malloc(twidth * DCTSIZE2);
  }
  else if (twidth > jpg_MCUbuf_size)
  { jpg_MCUbuf_size = twidth;
    jpg_Ybuf = (UBYTE *)realloc( jpg_Ybuf,(twidth * DCTSIZE2) );
    jpg_Ubuf = (UBYTE *)realloc( jpg_Ubuf,(twidth * DCTSIZE2) );
    jpg_Vbuf = (UBYTE *)realloc( jpg_Vbuf,(twidth * DCTSIZE2) );
  }
}

#define jpg_huff_EXTEND(val,sz) \
 ((val) < (1<<((sz)-1)) ? (val) + (((-1)<<(sz)) + 1) : (val))


/*
 * IF marker is read then jpg_marker is set and nothing more is read in.
 */
#define JJ_HBBUF_FILL(hbbuf) \
{ register ULONG _tmp; \
  if (jpg_marker) return(FALSE); \
  hbbuf <<= 16; \
  _tmp = *jpg_buff++;  jpg_bsize--; \
  while(_tmp == 0xff) \
  { register ULONG _t1; _t1 = *jpg_buff++; jpg_bsize--; \
    if (_t1 == 0x00) break; \
    else if (_t1 == 0xff) continue; \
    else {jpg_marker = _t1; _tmp = 0x00; break;} \
  } \
  hbbuf |= _tmp << 8; \
  if (!jpg_marker) \
  { \
    _tmp = *jpg_buff++;  jpg_bsize--; \
    while(_tmp == 0xff) \
    { register ULONG _t1; _t1 = *jpg_buff++; jpg_bsize--; \
      if (_t1 == 0x00) break; \
      else if (_t1 == 0xff) continue; \
      else {jpg_marker = _t1; _tmp = 0x00; break;} \
    } \
  } \
  hbbuf |= _tmp; \
}



#define JJ_HUFF_DECODE(huff_hdr,htbl, hbnum, hbbuf, result) \
{ register ULONG _tmp, _hcode; \
  if (hbnum < 16) { JJ_HBBUF_FILL(hbbuf); hbnum += 16; } \
  _tmp = (hbbuf >> (hbnum - 8)) & 0xff; \
  _hcode = (htbl)[_tmp]; \
  if (_hcode) { hbnum -= (_hcode >> 8); (result) = _hcode & 0xff; } \
  else \
  { register ULONG _hcode, _shift, _minbits = 9; \
    _tmp = (hbbuf >> (hbnum - 16)) & 0xffff; /* get 16 bits */ \
    _shift = 16 - _minbits;	_hcode = _tmp >> _shift; \
    while(_hcode > huff_hdr->maxcode[_minbits]) \
	{ _minbits++; _shift--; _hcode = _tmp >> _shift; } \
    if (_minbits > 16) { fprintf(stderr,"JHDerr\n"); return(FALSE); } \
    else { hbnum -= _minbits;  _hcode -= huff_hdr->mincode[_minbits]; \
    result = huff_hdr->vals[ (huff_hdr->valptr[_minbits] + _hcode) ]; } \
  } \
}

#define JJ_HUFF_MASK(s) ((1 << (s)) - 1)

#define JJ_GET_BITS(n, hbnum, hbbuf, result) \
{ hbnum -= n; \
  if (hbnum < 0)  { JJ_HBBUF_FILL(hbbuf); hbnum += 16; } \
  (result) = ((hbbuf >> hbnum) & JJ_HUFF_MASK(n)); \
}



/*  clears dctbuf to zeroes. 
 *  fills from huffman encode stream
 */
ULONG jpg_huffparse(comp, dct_buf, qtab, OBuf)
register COMPONENT_HDR *comp;
register SHORT *dct_buf;
ULONG *qtab;
UBYTE *OBuf;
{ LONG i,dcval;
  ULONG size;
  JJ_HUFF_TBL *huff_hdr = &(jpg_dc_huff[ comp->dc_htbl_num ]);
  USHORT *huff_tbl = huff_hdr->cache;
  UBYTE *rnglimit = jpg_samp_limit + (CENTERJSAMPLE + MAXJSAMPLE + 1);
  ULONG c_cnt,pos = 0;

  JJ_HUFF_DECODE(huff_hdr,huff_tbl,jpg_h_bnum,jpg_h_bbuf,size);
  if (size)
  { ULONG bits;
    JJ_GET_BITS(size,jpg_h_bnum,jpg_h_bbuf,bits);
    dcval = jpg_huff_EXTEND(bits, size);
    comp->dc += dcval;
  }
  dcval = comp->dc;

  /* clear reset of dct buffer */
  memset((char *)(dct_buf),0,(DCTSIZE2 * sizeof(SHORT)));
  dcval *= (LONG)qtab[0];
  dct_buf[0] = (SHORT)dcval;
  c_cnt = 0;

  huff_hdr = &(jpg_ac_huff[ comp->ac_htbl_num ]);
  huff_tbl = huff_hdr->cache;
  i = 1;
  while(i < 64)
  { LONG level;	ULONG run,tmp;
    JJ_HUFF_DECODE(huff_hdr,huff_tbl,jpg_h_bnum,jpg_h_bbuf,tmp); 
    size =  tmp & 0x0f;
    run = (tmp >> 4) & 0x0f; /* leading zeroes */
    if (size)
    { LONG coeff;
      i += run; /* skip zeroes */
      JJ_GET_BITS(size, jpg_h_bnum,jpg_h_bbuf,level);
      coeff = (LONG)jpg_huff_EXTEND(level, size);
      pos = JJ_ZAG[i];
      coeff *= (LONG)qtab[ pos ];
      if (coeff)
      { c_cnt++;
        dct_buf[ pos ] = (SHORT)(coeff);
      }
      i++;
    }
    else
    {
      if (run != 15) break; /* EOB */
      i += 16;
    }
  }

  if (c_cnt) j_rev_dct(dct_buf, OBuf, rnglimit);
  else
  { register UBYTE *op = OBuf;
    register int jj = 8;
    SHORT v = *dct_buf;
    register UBYTE dc;
    v = (v < 0)?( (v-3)>>3 ):( (v+4)>>3 );
    dc = rnglimit[ (int) (v & RANGE_MASK) ];
    while(jj--)
    { op[0] = op[1] = op[2] = op[3] = op[4] = op[5] = op[6] = op[7] = dc;
      op += 8;
    }
  }
  return(TRUE);
}

/* POD since these are fixed, possibly eliminate */
#define FIX_0_298631336  ((LONG)  2446)	/* FIX(0.298631336) */
#define FIX_0_390180644  ((LONG)  3196)	/* FIX(0.390180644) */
#define FIX_0_541196100  ((LONG)  4433)	/* FIX(0.541196100) */
#define FIX_0_765366865  ((LONG)  6270)	/* FIX(0.765366865) */
#define FIX_0_899976223  ((LONG)  7373)	/* FIX(0.899976223) */
#define FIX_1_175875602  ((LONG)  9633)	/* FIX(1.175875602) */
#define FIX_1_501321110  ((LONG)  12299)	/* FIX(1.501321110) */
#define FIX_1_847759065  ((LONG)  15137)	/* FIX(1.847759065) */
#define FIX_1_961570560  ((LONG)  16069)	/* FIX(1.961570560) */
#define FIX_2_053119869  ((LONG)  16819)	/* FIX(2.053119869) */
#define FIX_2_562915447  ((LONG)  20995)	/* FIX(2.562915447) */
#define FIX_3_072711026  ((LONG)  25172)	/* FIX(3.072711026) */

/***********************************************************************
 * For most steps we can mathematically guarantee that the initial value
 * of x is within MAXJSAMPLE+1 of the legal range, so a table running from
 * -(MAXJSAMPLE+1) to 2*MAXJSAMPLE+1 is sufficient.  But for the initial
 * limiting step (just after the IDCT), a wildly out-of-range value is
 * possible if the input data is corrupt.  To avoid any chance of indexing
 * off the end of memory and getting a bad-pointer trap, we perform the
 * post-IDCT limiting thus:
 *              x = range_limit[x & MASK];
 * where MASK is 2 bits wider than legal sample data, ie 10 bits for 8-bit
 * samples.  Under normal circumstances this is more than enough range and
 * a correct output will be generated; with bogus input data the mask will
 * cause wraparound, and we will safely generate a bogus-but-in-range output.
 * For the post-IDCT step, we want to convert the data from signed to unsigned
 * representation by adding CENTERJSAMPLE at the same time that we limit it.
 * So the post-IDCT limiting table ends up looking like this:
 *   CENTERJSAMPLE,CENTERJSAMPLE+1,...,MAXJSAMPLE,
 *   MAXJSAMPLE (repeat 2*(MAXJSAMPLE+1)-CENTERJSAMPLE times),
 *   0          (repeat 2*(MAXJSAMPLE+1)-CENTERJSAMPLE times),
 *   0,1,...,CENTERJSAMPLE-1
 * Negative inputs select values from the upper half of the table after
 * masking.
 *
 * We can save some space by overlapping the start of the post-IDCT table
 * with the simpler range limiting table.  The post-IDCT table begins at
 * sample_range_limit + CENTERJSAMPLE.
 *
 * Note that the table is allocated in near data space on PCs; it's small
 * enough and used often enough to justify this.
 */


void jpg_setup_samp_limit_table()
{
  UBYTE *table;
  LONG i;

  if (jpg_samp_limit==0)
  {
     jpg_samp_limit = (UBYTE *)malloc((5 * (MAXJSAMPLE+1) + CENTERJSAMPLE));
     if (jpg_samp_limit==0) 
     { fprintf(stderr,"JJ: samp limit malloc err\n"); 
       return;
     } /*POD note improve this */
  }
  else return; /* already done */

  table = jpg_samp_limit;
  table += (MAXJSAMPLE+1);   /* allow negative subscripts of simple table */

  /* First segment of "simple" table: limit[x] = 0 for x < 0 */
  memset(table - (MAXJSAMPLE+1), 0, (MAXJSAMPLE+1));

  /* Main part of "simple" table: limit[x] = x */
  for (i = 0; i <= MAXJSAMPLE; i++) table[i] = (UBYTE) i;

  table += CENTERJSAMPLE;       /* Point to where post-IDCT table starts */
  /* End of simple table, rest of first half of post-IDCT table */

  for (i = CENTERJSAMPLE; i < 2*(MAXJSAMPLE+1); i++) table[i] = MAXJSAMPLE;

  /* Second half of post-IDCT table */
  memset(table + (2 * (MAXJSAMPLE+1)), 0, (2 * (MAXJSAMPLE+1) - CENTERJSAMPLE));
  memcpy(table + (4 * (MAXJSAMPLE+1) - CENTERJSAMPLE),
          (char *)(jpg_samp_limit+(MAXJSAMPLE+1)), CENTERJSAMPLE);
}

/*******
 *
 */
ULONG jpg_std_DHT()
{
  LONG ttt,len;
  JJ_HUFF_TBL *htable;
  UBYTE  *hbits,*Sbits;
  UBYTE  *hvals,*Svals;

  static UBYTE dc_luminance_bits[17] =
    { /* 0-base */ 0, 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0 };
  static UBYTE dc_luminance_vals[] =
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

  static UBYTE dc_chrominance_bits[17] =
    { /* 0-base */ 0, 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 };
  static UBYTE dc_chrominance_vals[] =
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

  static UBYTE ac_luminance_bits[17] =
    { /* 0-base */ 0, 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d };
  static UBYTE ac_luminance_vals[] =
    { 0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
      0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
      0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
      0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
      0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
      0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
      0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
      0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
      0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
      0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
      0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
      0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
      0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
      0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
      0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
      0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
      0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
      0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
      0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
      0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
      0xf9, 0xfa };

  static UBYTE ac_chrominance_bits[17] =
    { /* 0-base */ 0, 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77 };
  static UBYTE ac_chrominance_vals[] =
    { 0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
      0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
      0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
      0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
      0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,
      0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
      0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,
      0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
      0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
      0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
      0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
      0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
      0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,
      0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
      0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
      0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
      0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2,
      0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
      0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
      0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
      0xf9, 0xfa };

  for(ttt=0;ttt<4;ttt++)
  { ULONG index = ttt & 1;
    LONG i,count;

    if (ttt <= 1)  /* DC tables */ 
    {
      htable = &(jpg_dc_huff[index]);
      hbits  = jpg_dc_huff[index].bits;
      hvals  = jpg_dc_huff[index].vals;
	if (index==0) { Sbits = dc_luminance_bits; Svals = dc_luminance_vals; }
	else { Sbits = dc_chrominance_bits; Svals = dc_chrominance_vals; }
    }
    else /* AC tables */
    {
      htable = &(jpg_ac_huff[index]);
      hbits  = jpg_ac_huff[index].bits;
      hvals  = jpg_ac_huff[index].vals;
	if (index==0) { Sbits = ac_luminance_bits; Svals = ac_luminance_vals; }
	else { Sbits = ac_chrominance_bits; Svals = ac_chrominance_vals; }
    }
    hbits[0] = 0;		count = 0;
    for (i = 1; i <= 16; i++)
    {
      hbits[i] = Sbits[i];
      count += hbits[i];
    }
    len -= 17;
    if (count > 256)
	{ fprintf(stderr,"JJ: STD DHT bad count %ld\n",count); return(FALSE); }

    for (i = 0; i < count; i++) hvals[i] = Svals[i];
    len -= count;

    jpg_huff_build(htable,hbits,hvals);

  } /* end of i */
  jpg_std_DHT_flag = 1;
  return(TRUE);
}

ULONG jpg_search_marker(marker,data_ptr,data_size)
ULONG marker;
UBYTE **data_ptr;
LONG *data_size;
{ ULONG d = 0;
  UBYTE *dptr = *data_ptr;
  LONG dsize = *data_size;

  while( dsize )
  {
    if (d == 0xff) /* potential marker */
    {
      d = *dptr++; dsize--;
      if (d == marker) 
      {
	*data_size = dsize; *data_ptr = dptr; 	
	return(TRUE); /* found marker */
      }
    } else { d = *dptr++; dsize--; }
  }
  *data_size = dsize; *data_ptr = dptr; 	
  return(FALSE);
}

void JFIF_Read_IJPG_Tables(fin)
FILE *fin;
{ int i;
  for(i=0;i<64;i++) IJPG_Tab1[i] = getc(fin);
  for(i=0;i<64;i++) IJPG_Tab2[i] = getc(fin);
}

#define JJ_GET_COLOR24(r,g,b,map_flag,map,chdr,ip,CAST) \
{ register ULONG clr,ra,ga,ba; \
  ra = xa_gamma_adj[r]; ga = xa_gamma_adj[g]; ba = xa_gamma_adj[b]; \
  if (x11_display_type & XA_X11_TRUE) clr = X11_Get_True_Color(ra,ga,ba,16); \
  else if ((cmap_color_func == 4) && (chdr)) \
  { register ULONG cache_i = ((r>>3)<<10) | ((g>>3)<<5) | (b>>3); \
    if (cmap_cache[cache_i] == 0xffff) \
    { clr = chdr->coff + CMAP_Find_Closest(chdr->cmap,chdr->csize, \
						ra,ga,ba,16,16,16,TRUE); \
      cmap_cache[cache_i] = (USHORT)clr; \
    } else clr = (ULONG)cmap_cache[cache_i]; \
  } else  \
  { if (cmap_true_to_332 == TRUE) clr = CMAP_GET_332(r,g,b,CMAP_SCALE8); \
      else                        clr = CMAP_GET_GRAY(r,g,b,CMAP_SCALE13); \
      if (map_flag) clr = map[clr]; \
  } \
  *ip++ = (CAST)(clr); \
}

/*******
 *
 ***/
void
jpg_111_byte_gray(iptr,imagex,imagey,mcu_row_size,orow_size,map_flag,map,chdr)
UBYTE *iptr; LONG imagex,imagey; ULONG mcu_row_size,orow_size;
ULONG map_flag,*map; XA_CHDR *chdr;
{ ULONG yi = 8;	UBYTE *Ybuf = jpg_Ybuf;

  if (map_flag==FALSE)
  { while(yi--)
    { UBYTE *yp=Ybuf; LONG skip=0; LONG xi=imagex; UBYTE *ip=(UBYTE *)iptr;
      if (imagey <= 0) return;
      while(xi--) {*ip++ = *yp++; skip++; if (skip >= 8) {skip=0; yp += 56;} }
      Ybuf += 8; imagey--;  iptr += orow_size;
  } }
  else if (x11_bytes_pixel==1)
  { while(yi--)
    { UBYTE *yp=Ybuf; LONG skip=0; LONG xi=imagex; UBYTE *ip=(UBYTE *)iptr;
      if (imagey <= 0) return;
      while(xi--) { *ip++ = (UBYTE)map[ *yp++ ]; 
			skip++; if (skip >= 8) {skip = 0; yp += 56;} }
      Ybuf += 8; imagey--;  iptr += orow_size;
  } }
  else if (x11_bytes_pixel==4)
  { while(yi--)
    { UBYTE *yp=Ybuf; LONG skip=0; LONG xi=imagex; ULONG *ip=(ULONG *)iptr;
      if (imagey <= 0) return;
      while(xi--) { *ip++ = (ULONG)map[ *yp++ ]; 
			skip++; if (skip >= 8) {skip = 0; yp += 56;} }
      Ybuf += 8; imagey--;  iptr += orow_size;
  } }
  else /* if (x11_bytes_pixel==2) */
  { while(yi--)
    { UBYTE *yp=Ybuf; LONG skip=0; LONG xi=imagex; USHORT *ip=(USHORT *)iptr;
      if (imagey <= 0) return;
      while(xi--) { *ip++ = (USHORT)map[ *yp++ ]; 
			skip++; if (skip >= 8) {skip = 0; yp += 56;} }
      Ybuf += 8; imagey--;  iptr += orow_size;
  } }

}

/*******
 *
 ***/
void
jpg_111_spec_color(iptr,imagex,imagey,mcu_row_size,orow_size,map_flag,map,chdr)
UBYTE *iptr;
LONG imagex,imagey;
ULONG mcu_row_size,orow_size;
ULONG map_flag,*map;
XA_CHDR *chdr;
{ ULONG yi;	UBYTE *Ybuf,*Ubuf,*Vbuf;
  UBYTE *rnglimit = jpg_samp_limit + (MAXJSAMPLE + 1);
  Ybuf = jpg_Ybuf;   Ubuf = jpg_Ubuf;   Vbuf = jpg_Vbuf;
  for(yi = 0; yi < 8; yi++)
  { UBYTE *yp,*up,*vp,*ip;  LONG xi,skip;
    if (imagey <= 0) return;
    yp = Ybuf; up = Ubuf; vp = Vbuf; ip = iptr; xi = imagex;  skip = 0;
    while(xi--)
    { LONG cr,cg,cb,y0;	ULONG u0,v0;
      u0 = (ULONG)*up++;	v0 = (ULONG)*vp++;
      cr = QT_VR_tab[v0];	cb = QT_UB_tab[u0];
      cg = (QT_UG_tab[u0] + QT_VG_tab[v0]) >> 16;
      y0 = (LONG)*yp++;		*ip++ = rnglimit[y0+cr];
      *ip++ = rnglimit[y0+cg];	*ip++ = rnglimit[y0+cb];
      skip++; if (skip >= 8) { skip = 0; yp += 56; up += 56; vp += 56; }
    } /* end xi */
    Ybuf += 8; Ubuf += 8; Vbuf += 8;
    imagey--;  iptr += orow_size;
  }
}

/*******
 *
 ***/
void
jpg_111_byte_color(iptr,imagex,imagey,mcu_row_size,orow_size,map_flag,map,chdr)
UBYTE *iptr; LONG imagex,imagey; ULONG mcu_row_size,orow_size;
ULONG map_flag,*map; XA_CHDR *chdr;
{ ULONG yi;	UBYTE *Ybuf,*Ubuf,*Vbuf;
  UBYTE *rnglimit = jpg_samp_limit + (MAXJSAMPLE + 1);
  Ybuf = jpg_Ybuf;   Ubuf = jpg_Ubuf;   Vbuf = jpg_Vbuf;
  for(yi = 0; yi < 8; yi++)
  { UBYTE *ip; UBYTE *yp,*up,*vp;  LONG xi,skip;
    if (imagey <= 0) return;
    yp = Ybuf; up = Ubuf; vp = Vbuf; ip = iptr; xi = imagex;  skip = 0;
    while(xi--)
    { LONG cr,cg,cb,y0;		ULONG r,g,b,u0,v0;
      u0 = (ULONG)*up++;	v0 = (ULONG)*vp++; 
      cr = QT_VR_tab[v0];	cb = QT_UB_tab[u0];
      cg = (QT_UG_tab[u0] + QT_VG_tab[v0]) >> 16;
      y0 = (LONG)(*yp++);			r = (ULONG)rnglimit[y0+cr];
      g = (ULONG)rnglimit[y0+cg];		b = (ULONG)rnglimit[y0+cb];
      JJ_GET_COLOR24(r,g,b,map_flag,map,chdr,ip,UBYTE);
      skip++; if (skip >= 8)	{ skip = 0; yp += 56; up += 56; vp += 56; }
    }
    Ybuf += 8; Ubuf += 8; Vbuf += 8; imagey--;  iptr += orow_size;
  }
}

/*******
 *
 ***/
void
jpg_111_short_color(iptr,imagex,imagey,mcu_row_size,orow_size,map_flag,map,chdr)
UBYTE *iptr; LONG imagex,imagey; ULONG mcu_row_size,orow_size;
ULONG map_flag,*map; XA_CHDR *chdr;
{ ULONG yi;	UBYTE *Ybuf,*Ubuf,*Vbuf;
  UBYTE *rnglimit = jpg_samp_limit + (MAXJSAMPLE + 1);
  Ybuf = jpg_Ybuf;   Ubuf = jpg_Ubuf;   Vbuf = jpg_Vbuf;
  for(yi = 0; yi < 8; yi++)
  { USHORT *ip; UBYTE *yp,*up,*vp;  LONG xi,skip;
    if (imagey <= 0) return;
    yp = Ybuf; up = Ubuf; vp = Vbuf; ip = (USHORT *)iptr; 
    xi = imagex;  skip = 0;
    while(xi--)
    { LONG cr,cg,cb,y0;		ULONG r,g,b,u0,v0;
      u0 = (ULONG)*up++;	v0 = (ULONG)*vp++; 
      cr = QT_VR_tab[v0];	cb = QT_UB_tab[u0];
      cg = (QT_UG_tab[u0] + QT_VG_tab[v0]) >> 16;
      y0 = (LONG)(*yp++);			r = (ULONG)rnglimit[y0+cr];
      g = (ULONG)rnglimit[y0+cg];		b = (ULONG)rnglimit[y0+cb];
      JJ_GET_COLOR24(r,g,b,map_flag,map,chdr,ip,USHORT);
      skip++; if (skip >= 8)	{ skip = 0; yp += 56; up += 56; vp += 56; }
    }
    Ybuf += 8; Ubuf += 8; Vbuf += 8; imagey--;  iptr += orow_size;
  }
}

/*******
 *
 ***/
void
jpg_111_long_color(iptr,imagex,imagey,mcu_row_size,orow_size,map_flag,map,chdr)
UBYTE *iptr; LONG imagex,imagey; ULONG mcu_row_size,orow_size;
ULONG map_flag,*map; XA_CHDR *chdr;
{ ULONG yi;	UBYTE *Ybuf,*Ubuf,*Vbuf;
  UBYTE *rnglimit = jpg_samp_limit + (MAXJSAMPLE + 1);
  Ybuf = jpg_Ybuf;   Ubuf = jpg_Ubuf;   Vbuf = jpg_Vbuf;
  for(yi = 0; yi < 8; yi++)
  { ULONG *ip; UBYTE *yp,*up,*vp;  LONG xi,skip;
    if (imagey <= 0) return;
    yp = Ybuf; up = Ubuf; vp = Vbuf; ip = (ULONG *)iptr; xi = imagex;  skip = 0;
    while(xi--)
    { LONG cr,cg,cb,y0;		ULONG r,g,b,u0,v0;
      u0 = (ULONG)*up++;	v0 = (ULONG)*vp++; 
      cr = QT_VR_tab[v0];	cb = QT_UB_tab[u0];
      cg = (QT_UG_tab[u0] + QT_VG_tab[v0]) >> 16;
      y0 = (LONG)(*yp++);			r = (ULONG)rnglimit[y0+cr];
      g = (ULONG)rnglimit[y0+cg];		b = (ULONG)rnglimit[y0+cb];
      JJ_GET_COLOR24(r,g,b,map_flag,map,chdr,ip,ULONG);
      skip++; if (skip >= 8)	{ skip = 0; yp += 56; up += 56; vp += 56; }
    }
    Ybuf += 8; Ubuf += 8; Vbuf += 8; imagey--;  iptr += orow_size;
  }
}

/*******
 *
 */
ULONG jpg_decode_111(image,width,height,interleave,row_offset,
				imagex,imagey,map_flag,map,chdr,special)
UBYTE *image;
ULONG width,height,interleave,row_offset;
ULONG imagex,imagey;
ULONG map_flag,*map;
XA_CHDR *chdr;
ULONG special;
{
  UBYTE *iptr = image;
  ULONG x,mcu_cols,mcu_rows;
  LONG *qtab0,*qtab1,*qtab2;
  UBYTE *Ybuf,*Ubuf,*Vbuf;
  ULONG orow_size;
  ULONG gray,rst_count;
  void (*color_func)();

  gray = special & 0x02;
  orow_size = imagex;
  if (special & 0x01) 
        { color_func = jpg_411_spec_color;      orow_size *= 3; }
  else if ( (x11_bytes_pixel==1) || (map_flag == FALSE) )
        { color_func = jpg_411_byte_color; }
  else if (x11_bytes_pixel==2)
        { color_func = jpg_411_short_color;     orow_size <<= 1; }
  else  { color_func = jpg_411_long_color;      orow_size <<= 2; }
  if (special & 0x02) color_func = jpg_111_byte_gray;

  if (row_offset) iptr += row_offset * orow_size;
  orow_size *= interleave;
  if (interleave == 2) imagey >>= 1;

  qtab0 = jpg_quant_tables[ jpg_comps[0].qtbl_num ];
  qtab1 = jpg_quant_tables[ jpg_comps[1].qtbl_num ];
  qtab2 = jpg_quant_tables[ jpg_comps[2].qtbl_num ];

  mcu_cols = (width  + 7) / 8;
  mcu_rows = (height + 7) / 8;
  /* DEBUG_LEVEL1 fprintf(stderr,"MCU(%ld,%ld)\n",mcu_cols,mcu_rows); */
  jpg_marker = 0x00;

  rst_count = jpg_rst_interval;
  while(mcu_rows--)
  { 
    Ybuf = jpg_Ybuf;   Ubuf = jpg_Ubuf;   Vbuf = jpg_Vbuf;
    x = mcu_cols;
    while(x--)
    { /* DEBUG_LEVEL1 fprintf(stderr,"  MCU XY(%ld,%ld)\n",x,mcu_rows); */
/*POD macro this RST stuff? */
      if ( ((jpg_rst_interval) && (rst_count==0)) || jpg_marker )
      { jpg_h_bbuf = 0;  jpg_h_bnum = 0;
	if (jpg_marker)
	{
	  if (jpg_marker == M_EOI) { jpg_saw_EOI = TRUE; return(TRUE); }
	  else if ( !((jpg_marker >= M_RST0) && (jpg_marker <= M_RST7)))
	  { fprintf(stderr,"JPEG: unexp marker (%lx)\n",jpg_marker);
	    return(FALSE);
	  }
	  jpg_marker = 0;
	}
	else if (jpg_read_RST_marker()==FALSE)
		{ fprintf(stderr,"RST marker false\n"); return(FALSE); }
	jpg_comps[0].dc = jpg_comps[1].dc = jpg_comps[2].dc = 0;
	rst_count = jpg_rst_interval;
      } else rst_count--;

      jpg_huffparse(&jpg_comps[0],jpg_dct_buf,qtab0,Ybuf); Ybuf += DCTSIZE2;
      if (gray==0)
      {
        jpg_huffparse(&jpg_comps[1],jpg_dct_buf,qtab1,Ubuf); Ubuf += DCTSIZE2;
        jpg_huffparse(&jpg_comps[2],jpg_dct_buf,qtab2,Vbuf); Vbuf += DCTSIZE2;
      }
    } /* end of mcu_cols */

    (void)(color_func)(iptr,imagex,imagey,0,orow_size,map_flag,map,chdr);
    imagey -= 8;  iptr += (orow_size << 3);

  } /* end of mcu_rows */
  if (jpg_marker) { jpg_h_bbuf = 0; jpg_h_bnum = 0; }
  return(TRUE);
}

/*******
 *
 ***/
void
jpg_211_spec_color(iptr,imagex,imagey,mcu_row_size,orow_size,map_flag,map,chdr)
UBYTE *iptr;
LONG imagex,imagey;
ULONG mcu_row_size,orow_size;
ULONG map_flag,*map;
XA_CHDR *chdr;
{ ULONG yi;	UBYTE *Ybuf,*Ubuf,*Vbuf;
  UBYTE *rnglimit = jpg_samp_limit + (MAXJSAMPLE + 1);
  Ybuf = jpg_Ybuf;   Ubuf = jpg_Ubuf;   Vbuf = jpg_Vbuf;
  for(yi = 0; yi < 8; yi++)
  { UBYTE *yp,*up,*vp,*ip;  LONG xi,skip;
    if (imagey <= 0) return;
    yp = Ybuf; up = Ubuf; vp = Vbuf; ip = iptr; xi = imagex;  skip = 0;
    while(xi--)
    { LONG cr,cg,cb,y0;	ULONG u0,v0;
      u0 = (ULONG)*up++;	v0 = (ULONG)*vp++;
      cr = QT_VR_tab[v0];	cb = QT_UB_tab[u0];
      cg = (QT_UG_tab[u0] + QT_VG_tab[v0]) >> 16;
      y0 = (LONG)*yp++;		*ip++ = rnglimit[y0+cr];
      *ip++ = rnglimit[y0+cg];	*ip++ = rnglimit[y0+cb];
      y0 = (LONG)*yp++;		*ip++ = rnglimit[y0+cr];
      *ip++ = rnglimit[y0+cg];	*ip++ = rnglimit[y0+cb];
      skip++;	if (skip == 4)	yp += 56;
      else if (skip >= 8) { skip = 0; yp += 56; up += 56; vp += 56; }
    } /* end xi */
    Ybuf += 8; Ubuf += 8; Vbuf += 8;
    imagey--;  iptr += orow_size;
  }
}

/*******
 *
 ***/
void
jpg_211_byte_color(iptr,imagex,imagey,mcu_row_size,orow_size,map_flag,map,chdr)
UBYTE *iptr; LONG imagex,imagey; ULONG mcu_row_size,orow_size;
ULONG map_flag,*map; XA_CHDR *chdr;
{ ULONG yi;	UBYTE *Ybuf,*Ubuf,*Vbuf;
  UBYTE *rnglimit = jpg_samp_limit + (MAXJSAMPLE + 1);
  Ybuf = jpg_Ybuf;   Ubuf = jpg_Ubuf;   Vbuf = jpg_Vbuf;
  for(yi = 0; yi < 8; yi++)
  { UBYTE *yp,*up,*vp,*ip;  LONG xi,skip;
    if (imagey <= 0) return;
    yp = Ybuf; up = Ubuf; vp = Vbuf; ip = iptr; xi = imagex;  skip = 0;
    while(xi--)
    { LONG cr,cg,cb,y0;		ULONG r,g,b,u0,v0;
      u0 = (ULONG)*up++;	v0 = (ULONG)*vp++; 
      cr = QT_VR_tab[v0];	cb = QT_UB_tab[u0];
      cg = (QT_UG_tab[u0] + QT_VG_tab[v0]) >> 16;
      y0 = (LONG)(*yp++);			r = (ULONG)rnglimit[y0+cr];
      g = (ULONG)rnglimit[y0+cg];		b = (ULONG)rnglimit[y0+cb];
      JJ_GET_COLOR24(r,g,b,map_flag,map,chdr,ip,UBYTE);
      y0 = (LONG)(*yp++);			r = (ULONG)rnglimit[y0+cr];
      g = (ULONG)rnglimit[y0+cg];		b = (ULONG)rnglimit[y0+cb];
      JJ_GET_COLOR24(r,g,b,map_flag,map,chdr,ip,UBYTE);
      skip++; if (skip == 4)	yp += 56;
      else if (skip >= 8)	{ skip = 0; yp += 56; up += 56; vp += 56; }
    }
    Ybuf += 8; Ubuf += 8; Vbuf += 8; imagey--;  iptr += orow_size;
  }
}


/*******
 *
 ***/
void
jpg_211_short_color(iptr,imagex,imagey,mcu_row_size,orow_size,map_flag,map,chdr)
USHORT *iptr; LONG imagex,imagey; ULONG mcu_row_size,orow_size;
ULONG map_flag,*map; XA_CHDR *chdr;
{ ULONG yi;	UBYTE *Ybuf,*Ubuf,*Vbuf;
  UBYTE *rnglimit = jpg_samp_limit + (MAXJSAMPLE + 1);
  Ybuf = jpg_Ybuf;   Ubuf = jpg_Ubuf;   Vbuf = jpg_Vbuf;
  for(yi = 0; yi < 8; yi++)
  { USHORT *ip;  UBYTE *yp,*up,*vp;  LONG xi,skip;
    if (imagey <= 0) return;
    yp = Ybuf; up = Ubuf; vp = Vbuf; ip = iptr; xi = imagex;  skip = 0;
    while(xi--)
    { LONG cr,cg,cb,y0;		ULONG r,g,b,u0,v0;
      u0 = (ULONG)*up++;	v0 = (ULONG)*vp++; 
      cr = QT_VR_tab[v0];	cb = QT_UB_tab[u0];
      cg = (QT_UG_tab[u0] + QT_VG_tab[v0]) >> 16;
      y0 = (LONG)(*yp++);			r = (ULONG)rnglimit[y0+cr];
      g = (ULONG)rnglimit[y0+cg];		b = (ULONG)rnglimit[y0+cb];
      JJ_GET_COLOR24(r,g,b,map_flag,map,chdr,ip,USHORT);
      y0 = (LONG)(*yp++);			r = (ULONG)rnglimit[y0+cr];
      g = (ULONG)rnglimit[y0+cg];		b = (ULONG)rnglimit[y0+cb];
      JJ_GET_COLOR24(r,g,b,map_flag,map,chdr,ip,USHORT);
      skip++; if (skip == 4)	yp += 56;
      else if (skip >= 8)	{ skip = 0; yp += 56; up += 56; vp += 56; }
    }
    Ybuf += 8; Ubuf += 8; Vbuf += 8; imagey--;  iptr += orow_size;
  }
}

/*******
 *
 ***/
void
jpg_211_long_color(iptr,imagex,imagey,mcu_row_size,orow_size,map_flag,map,chdr)
ULONG *iptr; LONG imagex,imagey; ULONG mcu_row_size,orow_size;
ULONG map_flag,*map; XA_CHDR *chdr;
{ ULONG yi;	UBYTE *Ybuf,*Ubuf,*Vbuf;
  UBYTE *rnglimit = jpg_samp_limit + (MAXJSAMPLE + 1);
  Ybuf = jpg_Ybuf;   Ubuf = jpg_Ubuf;   Vbuf = jpg_Vbuf;
  for(yi = 0; yi < 8; yi++)
  { ULONG *ip; UBYTE *yp,*up,*vp;  LONG xi,skip;
    if (imagey <= 0) return;
    yp = Ybuf; up = Ubuf; vp = Vbuf; ip = iptr; xi = imagex;  skip = 0;
    while(xi--)
    { LONG cr,cg,cb,y0;		ULONG r,g,b,u0,v0;
      u0 = (ULONG)*up++;	v0 = (ULONG)*vp++; 
      cr = QT_VR_tab[v0];	cb = QT_UB_tab[u0];
      cg = (QT_UG_tab[u0] + QT_VG_tab[v0]) >> 16;
      y0 = (LONG)(*yp++);			r = (ULONG)rnglimit[y0+cr];
      g = (ULONG)rnglimit[y0+cg];		b = (ULONG)rnglimit[y0+cb];
      JJ_GET_COLOR24(r,g,b,map_flag,map,chdr,ip,ULONG);
      y0 = (LONG)(*yp++);			r = (ULONG)rnglimit[y0+cr];
      g = (ULONG)rnglimit[y0+cg];		b = (ULONG)rnglimit[y0+cb];
      JJ_GET_COLOR24(r,g,b,map_flag,map,chdr,ip,ULONG);
      skip++; if (skip == 4)	yp += 56;
      else if (skip >= 8)	{ skip = 0; yp += 56; up += 56; vp += 56; }
    }
    Ybuf += 8; Ubuf += 8; Vbuf += 8; imagey--;  iptr += orow_size;
  }
}

/*******
 *
 */
ULONG jpg_decode_211(image,width,height,interleave,row_offset,
				imagex,imagey,map_flag,map,chdr,special)
UBYTE *image;
ULONG width,height,interleave,row_offset;
ULONG imagex,imagey;
ULONG map_flag,*map;
XA_CHDR *chdr;
ULONG special;
{
  UBYTE *iptr = image;
  ULONG x,mcu_cols,mcu_rows;
  LONG *qtab0,*qtab1,*qtab2;
  UBYTE *Ybuf,*Ubuf,*Vbuf;
  ULONG orow_size;
  ULONG rst_count;
  void (*color_func)();

  orow_size = imagex;
  if (special) 
        { color_func = jpg_411_spec_color;      orow_size *= 3; }
  else if ( (x11_bytes_pixel==1) || (map_flag == FALSE) )
        { color_func = jpg_411_byte_color; }
  else if (x11_bytes_pixel==2)
        { color_func = jpg_411_short_color;     orow_size <<= 1; }
  else  { color_func = jpg_411_long_color;      orow_size <<= 2; }

  if (row_offset) iptr += row_offset * orow_size;
  orow_size *= interleave;
  if (interleave == 2) imagey >>= 1;
  imagex++; imagex >>= 1;

  qtab0 = jpg_quant_tables[ jpg_comps[0].qtbl_num ];
  qtab1 = jpg_quant_tables[ jpg_comps[1].qtbl_num ];
  qtab2 = jpg_quant_tables[ jpg_comps[2].qtbl_num ];

  mcu_cols = (width  + 15) / 16;
  mcu_rows = (height +  7) / 8;
  /* DEBUG_LEVEL1 fprintf(stderr,"MCU(%ld,%ld)\n",mcu_cols,mcu_rows); */
  jpg_marker = 0x00;

  rst_count = jpg_rst_interval;
  while(mcu_rows--)
  { 
    Ybuf = jpg_Ybuf;   Ubuf = jpg_Ubuf;   Vbuf = jpg_Vbuf;
    x = mcu_cols;
    while(x--)
    { /* DEBUG_LEVEL1 fprintf(stderr,"MCU XY(%ld,%ld)\n", x,mcu_rows); */

      if ( ((jpg_rst_interval) && (rst_count==0)) || jpg_marker )
      { jpg_h_bbuf = 0;  jpg_h_bnum = 0;
	if (jpg_marker)
	{
	  if (jpg_marker == M_EOI) { jpg_saw_EOI = TRUE; return(TRUE); }
	  else if ( !((jpg_marker >= M_RST0) && (jpg_marker <= M_RST7)))
	  { fprintf(stderr,"JPEG: unexp marker (%lx)\n",jpg_marker);
	    return(FALSE);
	  }
	  jpg_marker = 0;
	}
	else if (jpg_read_RST_marker()==FALSE)
		{ fprintf(stderr,"RST marker false\n"); return(FALSE); }
	jpg_comps[0].dc = jpg_comps[1].dc = jpg_comps[2].dc = 0;
	rst_count = jpg_rst_interval;
      } else rst_count--;

      jpg_huffparse(&jpg_comps[0],jpg_dct_buf,qtab0,Ybuf); Ybuf += DCTSIZE2;
      jpg_huffparse(&jpg_comps[0],jpg_dct_buf,qtab0,Ybuf); Ybuf += DCTSIZE2;
      jpg_huffparse(&jpg_comps[1],jpg_dct_buf,qtab1,Ubuf); Ubuf += DCTSIZE2;
      jpg_huffparse(&jpg_comps[2],jpg_dct_buf,qtab2,Vbuf); Vbuf += DCTSIZE2;
    } /* end of mcu_cols */

    (void)(color_func)(iptr,imagex,imagey,0,orow_size,map_flag,map,chdr);
    imagey -= 8;  iptr += (orow_size << 3);

  } /* end of mcu_rows */
  if (jpg_marker) { jpg_h_bbuf = 0; jpg_h_bnum = 0; }
  return(TRUE);
}


/*******
 *
 ***/
void
jpg_411_spec_color(iptr,imagex,imagey,mcu_row_size,orow_size,map_flag,map,chdr)
UBYTE *iptr;
LONG imagex,imagey;
ULONG mcu_row_size,orow_size;
ULONG map_flag,*map;
XA_CHDR *chdr;
{ ULONG yi;	UBYTE *Ybuf0,*Ybuf1,*Ubuf,*Vbuf;
  UBYTE *rnglimit = jpg_samp_limit + (MAXJSAMPLE + 1);
  Ybuf0 = jpg_Ybuf; Ybuf1 = Ybuf0 + 8; Ubuf = jpg_Ubuf; Vbuf = jpg_Vbuf;
  for(yi = 0; yi < 8; yi++)
  { UBYTE *ip0,*ip1,*yp0,*yp1,*up,*vp;  LONG xi,skip;
    if (imagey <= 0) return;
    if (yi == 4) { Ybuf0 = jpg_Ybuf + mcu_row_size; Ybuf1 = Ybuf0 + 8; }
    yp0 = Ybuf0; yp1 = Ybuf1; up = Ubuf; vp = Vbuf;
    ip0 = iptr;  ip1 = iptr + orow_size; xi = imagex;  skip = 0;
    while(xi--)
    { LONG cr,cg,cb,y0;	ULONG u0,v0;
      u0 = (ULONG)*up++;	v0 = (ULONG)*vp++;
      cr = QT_VR_tab[v0];	cb = QT_UB_tab[u0];
      cg = (QT_UG_tab[u0] + QT_VG_tab[v0]) >> 16;
      y0 = (LONG)*yp0++;		*ip0++ = rnglimit[y0+cr];
      *ip0++ = rnglimit[y0+cg];		*ip0++ = rnglimit[y0+cb];
      y0 = (LONG)*yp0++;		*ip0++ = rnglimit[y0+cr];
      *ip0++ = rnglimit[y0+cg];		*ip0++ = rnglimit[y0+cb];
      y0 = (LONG)*yp1++;		*ip1++ = rnglimit[y0+cr];
      *ip1++ = rnglimit[y0+cg];		*ip1++ = rnglimit[y0+cb];
      y0 = (LONG)*yp1++;		*ip1++ = rnglimit[y0+cr];
      *ip1++ = rnglimit[y0+cg];		*ip1++ = rnglimit[y0+cb];
      skip++; if (skip == 4)      {yp0 += 56; yp1 += 56; }
      else if (skip >= 8) {skip = 0; yp0 += 56; yp1 += 56; up += 56; vp += 56;}
    }
    Ybuf0 += 16; Ybuf1 += 16; Ubuf += 8; Vbuf += 8;
    imagey -= 2;  iptr += (orow_size << 1);
  }
}

/*******
 *
 ***/
void
jpg_411_byte_CF4(iptr,imagex,imagey,mcu_row_size,orow_size,map_flag,map,chdr)
UBYTE *iptr;
LONG imagex,imagey;
ULONG mcu_row_size,orow_size;
ULONG map_flag,*map;
XA_CHDR *chdr;
{ ULONG yi;     UBYTE *Ybuf0,*Ybuf1,*Ubuf,*Vbuf;
  UBYTE *rnglimit = jpg_samp_limit + (MAXJSAMPLE + 1);
  ULONG coff,csize; ColorReg *the_cmap = chdr->cmap;
  USHORT *gadj = xa_gamma_adj;
  Ybuf0 = jpg_Ybuf; Ybuf1 = Ybuf0 + 8; Ubuf = jpg_Ubuf; Vbuf = jpg_Vbuf;
  coff = chdr->coff;
  csize = chdr->csize;
  for(yi = 0; yi < 8; yi++)
  { UBYTE *ip0,*ip1,*yp0,*yp1,*up,*vp;  LONG xi,skip;
    if (imagey <= 0) return;
    if (yi == 4) { Ybuf0 = jpg_Ybuf + mcu_row_size; Ybuf1 = Ybuf0 + 8; }
    yp0 = Ybuf0; yp1 = Ybuf1; up = Ubuf; vp = Vbuf; xi = imagex;  skip = 0;
    ip0 = iptr;  iptr += orow_size; ip1 = iptr;  iptr += orow_size;
    while(xi--)
    { LONG cr,cg,cb,y0; ULONG r,g,b,u0,v0;
      u0 = (ULONG)*up++;	v0 = (ULONG)*vp++;
      cr = QT_VR_tab[v0];	cb = QT_UB_tab[u0];
      cg = (QT_UG_tab[u0] + QT_VG_tab[v0]) >> 16;

      y0 = (LONG)(*yp0++);		 r = (ULONG)(rnglimit[y0+cr]) >> 3;
      g = (ULONG)(rnglimit[y0+cg]) >> 3; b = (ULONG)(rnglimit[y0+cb]) >> 3;
      { register ULONG clr,cache_i = (r<<10) | (g<<5) | b;
        if ( (clr=cmap_cache[cache_i]) == 0xffff)
        {  clr = coff + CMAP_Find_Closest(the_cmap,csize,gadj[(r<<3)|(r>>2)],
		gadj[(g<<3)|(g>>2)],gadj[(b<<3)|(b>>2)],16,16,16,TRUE);
           cmap_cache[cache_i] = (USHORT)clr; }
	*ip0++ = (UBYTE)clr;
      }

      y0 = (LONG)(*yp0++);		 r = (ULONG)(rnglimit[y0+cr]) >> 3;
      g = (ULONG)(rnglimit[y0+cg]) >> 3; b = (ULONG)(rnglimit[y0+cb]) >> 3;
      { register ULONG clr,cache_i = (r<<10) | (g<<5) | b;
        if ( (clr=cmap_cache[cache_i]) == 0xffff)
        {  clr = coff + CMAP_Find_Closest(the_cmap,csize,gadj[(r<<3)|(r>>2)],
		gadj[(g<<3)|(g>>2)],gadj[(b<<3)|(b>>2)],16,16,16,TRUE);
          cmap_cache[cache_i] = (USHORT)clr; }
        *ip0++ = (UBYTE)clr;
      }

      y0 = (LONG)(*yp1++);		 r = (ULONG)(rnglimit[y0+cr]) >> 3;
      g = (ULONG)(rnglimit[y0+cg]) >> 3; b = (ULONG)(rnglimit[y0+cb]) >> 3;
      { register ULONG clr,cache_i = (r<<10) | (g<<5) | b;
        if ( (clr=cmap_cache[cache_i]) == 0xffff)
        {  clr = coff + CMAP_Find_Closest(the_cmap,csize,gadj[(r<<3)|(r>>2)],
		gadj[(g<<3)|(g>>2)],gadj[(b<<3)|(b>>2)],16,16,16,TRUE);
          cmap_cache[cache_i] = (USHORT)clr; }
        *ip1++ = (UBYTE)clr;
      }

      y0 = (LONG)(*yp1++);		 r = (ULONG)(rnglimit[y0+cr]) >> 3;
      g = (ULONG)(rnglimit[y0+cg]) >> 3; b = (ULONG)(rnglimit[y0+cb]) >> 3;
      { register ULONG clr,cache_i = (r<<10) | (g<<5) | b;
        if ( (clr=cmap_cache[cache_i]) == 0xffff)
        {  clr = coff + CMAP_Find_Closest(the_cmap,csize,gadj[(r<<3)|(r>>2)],
		gadj[(g<<3)|(g>>2)],gadj[(b<<3)|(b>>2)],16,16,16,TRUE);
          cmap_cache[cache_i] = (USHORT)clr; }
        *ip1++ = (UBYTE)clr;
      }
      skip++; if (skip == 4)      {yp0 += 56; yp1 += 56; }
      else if (skip >= 8) {skip = 0; yp0 += 56; yp1 += 56; up += 56; vp += 56;}
    }
    Ybuf0 += 16; Ybuf1 += 16; Ubuf += 8; Vbuf += 8; imagey -= 2;
  }
}
/*******
 *
 ***/
void
jpg_411_byte_color(iptr,imagex,imagey,mcu_row_size,orow_size,map_flag,map,chdr)
UBYTE *iptr;
LONG imagex,imagey;
ULONG mcu_row_size,orow_size;
ULONG map_flag,*map;
XA_CHDR *chdr;
{ ULONG yi;     UBYTE *Ybuf0,*Ybuf1,*Ubuf,*Vbuf;
  UBYTE *rnglimit = jpg_samp_limit + (MAXJSAMPLE + 1);
  Ybuf0 = jpg_Ybuf; Ybuf1 = Ybuf0 + 8; Ubuf = jpg_Ubuf; Vbuf = jpg_Vbuf;
  for(yi = 0; yi < 8; yi++)
  { UBYTE *ip0,*ip1,*yp0,*yp1,*up,*vp;  LONG xi,skip;
    if (imagey <= 0) return;
    if (yi == 4) { Ybuf0 = jpg_Ybuf + mcu_row_size; Ybuf1 = Ybuf0 + 8; }
    yp0 = Ybuf0; yp1 = Ybuf1; up = Ubuf; vp = Vbuf; xi = imagex;  skip = 0;
    ip0 = iptr;  iptr += orow_size; ip1 = iptr;  iptr += orow_size;
    while(xi--)
    { LONG cr,cg,cb,y0; ULONG r,g,b,u0,v0;
      u0 = (ULONG)*up++;	v0 = (ULONG)*vp++;
      cr = QT_VR_tab[v0];	cb = QT_UB_tab[u0];
      cg = (QT_UG_tab[u0] + QT_VG_tab[v0]) >> 16;
      y0 = (LONG)(*yp0++);        r = (ULONG)rnglimit[y0+cr];
      g = (ULONG)rnglimit[y0+cg]; b = (ULONG)rnglimit[y0+cb];
      JJ_GET_COLOR24(r,g,b,map_flag,map,chdr,ip0,UBYTE);
      y0 = (LONG)(*yp0++);        r = (ULONG)rnglimit[y0+cr];
      g = (ULONG)rnglimit[y0+cg]; b = (ULONG)rnglimit[y0+cb];
      JJ_GET_COLOR24(r,g,b,map_flag,map,chdr,ip0,UBYTE);
      y0 = (LONG)(*yp1++);        r = (ULONG)rnglimit[y0+cr];
      g = (ULONG)rnglimit[y0+cg]; b = (ULONG)rnglimit[y0+cb];
      JJ_GET_COLOR24(r,g,b,map_flag,map,chdr,ip1,UBYTE);
      y0 = (LONG)(*yp1++);        r = (ULONG)rnglimit[y0+cr];
      g = (ULONG)rnglimit[y0+cg]; b = (ULONG)rnglimit[y0+cb];
      JJ_GET_COLOR24(r,g,b,map_flag,map,chdr,ip1,UBYTE);
      skip++; if (skip == 4)      {yp0 += 56; yp1 += 56; }
      else if (skip >= 8) {skip = 0; yp0 += 56; yp1 += 56; up += 56; vp += 56;}
    }
    Ybuf0 += 16; Ybuf1 += 16; Ubuf += 8; Vbuf += 8; imagey -= 2;
  }
}

/*******
 *
 ***/
void
jpg_411_short_color(iptr,imagex,imagey,mcu_row_size,orow_size,map_flag,map,chdr)
UBYTE *iptr;
LONG imagex,imagey;
ULONG mcu_row_size,orow_size;
ULONG map_flag,*map;
XA_CHDR *chdr;
{ ULONG yi;     UBYTE *Ybuf0,*Ybuf1,*Ubuf,*Vbuf;
  UBYTE *rnglimit = jpg_samp_limit + (MAXJSAMPLE + 1);
  Ybuf0 = jpg_Ybuf; Ybuf1 = Ybuf0 + 8; Ubuf = jpg_Ubuf; Vbuf = jpg_Vbuf;
  for(yi = 0; yi < 8; yi++)
  { USHORT *ip0,*ip1; UBYTE *yp0,*yp1,*up,*vp;  LONG xi,skip;
    if (imagey <= 0) return;
    if (yi == 4) { Ybuf0 = jpg_Ybuf + mcu_row_size; Ybuf1 = Ybuf0 + 8; }
    yp0 = Ybuf0; yp1 = Ybuf1; up = Ubuf; vp = Vbuf; xi = imagex;  skip = 0;
    ip0 = (USHORT *)(iptr);  iptr += orow_size; 
    ip1 = (USHORT *)(iptr);  iptr += orow_size;
    while(xi--)
    { LONG cr,cg,cb,y0; ULONG r,g,b,u0,v0;
      u0 = (ULONG)*up++;	v0 = (ULONG)*vp++;
      cr = QT_VR_tab[v0];	cb = QT_UB_tab[u0];
      cg = (QT_UG_tab[u0] + QT_VG_tab[v0]) >> 16;
      y0 = (LONG)(*yp0++);        r = (ULONG)rnglimit[y0+cr];
      g = (ULONG)rnglimit[y0+cg]; b = (ULONG)rnglimit[y0+cb];
      JJ_GET_COLOR24(r,g,b,map_flag,map,chdr,ip0,USHORT);
      y0 = (LONG)(*yp0++);        r = (ULONG)rnglimit[y0+cr];
      g = (ULONG)rnglimit[y0+cg]; b = (ULONG)rnglimit[y0+cb];
      JJ_GET_COLOR24(r,g,b,map_flag,map,chdr,ip0,USHORT);
      y0 = (LONG)(*yp1++);        r = (ULONG)rnglimit[y0+cr];
      g = (ULONG)rnglimit[y0+cg]; b = (ULONG)rnglimit[y0+cb];
      JJ_GET_COLOR24(r,g,b,map_flag,map,chdr,ip1,USHORT);
      y0 = (LONG)(*yp1++);        r = (ULONG)rnglimit[y0+cr];
      g = (ULONG)rnglimit[y0+cg]; b = (ULONG)rnglimit[y0+cb];
      JJ_GET_COLOR24(r,g,b,map_flag,map,chdr,ip1,USHORT);
      skip++; if (skip == 4)      {yp0 += 56; yp1 += 56; }
      else if (skip >= 8) {skip = 0; yp0 += 56; yp1 += 56; up += 56; vp += 56;}
    }
    Ybuf0 += 16; Ybuf1 += 16; Ubuf += 8; Vbuf += 8; imagey -= 2;
  }
}

/*******
 *
 ***/
void
jpg_411_long_color(iptr,imagex,imagey,mcu_row_size,orow_size,map_flag,map,chdr)
UBYTE *iptr;
LONG imagex,imagey;
ULONG mcu_row_size,orow_size;
ULONG map_flag,*map;
XA_CHDR *chdr;
{ ULONG yi;     UBYTE *Ybuf0,*Ybuf1,*Ubuf,*Vbuf;
  UBYTE *rnglimit = jpg_samp_limit + (MAXJSAMPLE + 1);
  Ybuf0 = jpg_Ybuf; Ybuf1 = Ybuf0 + 8; Ubuf = jpg_Ubuf; Vbuf = jpg_Vbuf;
  for(yi = 0; yi < 8; yi++)
  { ULONG *ip0,*ip1; UBYTE *yp0,*yp1,*up,*vp;  LONG xi,skip;
    if (imagey <= 0) return;
    if (yi == 4) { Ybuf0 = jpg_Ybuf + mcu_row_size; Ybuf1 = Ybuf0 + 8; }
    yp0 = Ybuf0; yp1 = Ybuf1; up = Ubuf; vp = Vbuf; xi = imagex;  skip = 0;
    ip0 = (ULONG *)(iptr);  iptr += orow_size; 
    ip1 = (ULONG *)(iptr);  iptr += orow_size;
    while(xi--)
    { LONG cr,cg,cb,y0; ULONG r,g,b,u0,v0;
      u0 = (ULONG)*up++;	v0 = (ULONG)*vp++;
      cr = QT_VR_tab[v0];	cb = QT_UB_tab[u0];
      cg = (QT_UG_tab[u0] + QT_VG_tab[v0]) >> 16;
      y0 = (LONG)(*yp0++);        r = (ULONG)rnglimit[y0+cr];
      g = (ULONG)rnglimit[y0+cg]; b = (ULONG)rnglimit[y0+cb];
      JJ_GET_COLOR24(r,g,b,map_flag,map,chdr,ip0,ULONG);
      y0 = (LONG)(*yp0++);        r = (ULONG)rnglimit[y0+cr];
      g = (ULONG)rnglimit[y0+cg]; b = (ULONG)rnglimit[y0+cb];
      JJ_GET_COLOR24(r,g,b,map_flag,map,chdr,ip0,ULONG);
      y0 = (LONG)(*yp1++);        r = (ULONG)rnglimit[y0+cr];
      g = (ULONG)rnglimit[y0+cg]; b = (ULONG)rnglimit[y0+cb];
      JJ_GET_COLOR24(r,g,b,map_flag,map,chdr,ip1,ULONG);
      y0 = (LONG)(*yp1++);        r = (ULONG)rnglimit[y0+cr];
      g = (ULONG)rnglimit[y0+cg]; b = (ULONG)rnglimit[y0+cb];
      JJ_GET_COLOR24(r,g,b,map_flag,map,chdr,ip1,ULONG);
      skip++; if (skip == 4)      {yp0 += 56; yp1 += 56; }
      else if (skip >= 8) {skip = 0; yp0 += 56; yp1 += 56; up += 56; vp += 56;}
    }
    Ybuf0 += 16; Ybuf1 += 16; Ubuf += 8; Vbuf += 8; imagey -= 2;
  }
}


/*******
 *
 */
ULONG jpg_decode_411(image,width,height,interleave,row_offset,
				imagex,imagey,map_flag,map,chdr,special)
UBYTE *image;
ULONG width,height,interleave,row_offset;
LONG imagex,imagey;
ULONG map_flag,*map;
XA_CHDR *chdr;
ULONG special;
{
  UBYTE *iptr = image;
  LONG x,mcu_cols,mcu_rows,mcu_row_size;
  LONG *qtab0,*qtab1,*qtab2;
  UBYTE *Ybuf0,*Ybuf1,*Ubuf,*Vbuf;
  ULONG orow_size;
  ULONG rst_count;
  void (*color_func)();

  orow_size = imagex;
  if (special) 
	{ color_func = jpg_411_spec_color;	orow_size *= 3;	}
  else if ( (x11_bytes_pixel==1) || (map_flag == FALSE) )
	{ color_func = jpg_411_byte_color; }
  else if (x11_bytes_pixel==2)
	{ color_func = jpg_411_short_color;	orow_size <<= 1; }
  else	{ color_func = jpg_411_long_color;	orow_size <<= 2; }
	
  if (row_offset) iptr += row_offset * orow_size;
  orow_size *= interleave;
  if (interleave == 2) imagey >>= 1;
  imagex++; imagex >>= 1;
  qtab0 = jpg_quant_tables[ jpg_comps[0].qtbl_num ];
  qtab1 = jpg_quant_tables[ jpg_comps[1].qtbl_num ];
  qtab2 = jpg_quant_tables[ jpg_comps[2].qtbl_num ];

  mcu_cols = (width  + 15) / 16;
  mcu_rows = (height + 15) / 16;
  mcu_row_size = (2 * mcu_cols * DCTSIZE2);
  DEBUG_LEVEL1 fprintf(stderr,"411 begin MCUS(%ld,%ld)\n",mcu_cols,mcu_rows);
  jpg_marker = 0x00;

  rst_count = jpg_rst_interval;
  while(mcu_rows--)
  { 
    Ybuf0 = jpg_Ybuf;   Ubuf = jpg_Ubuf;   Vbuf = jpg_Vbuf;
    Ybuf1 = Ybuf0 + mcu_row_size;
    x = mcu_cols; while(x--)
    { DEBUG_LEVEL1 fprintf(stderr,"  MCU XY(%ld,%ld)\n",x,mcu_rows);
      if ( ((jpg_rst_interval) && (rst_count==0)) || jpg_marker )
      { jpg_h_bbuf = 0;	jpg_h_bnum = 0;
	if (jpg_marker)
	{
	  if (jpg_marker == M_EOI) { DEBUG_LEVEL1 fprintf(stderr,"411 EOI\n");
		jpg_saw_EOI = TRUE; return(TRUE); }
	  else if ( !((jpg_marker >= M_RST0) && (jpg_marker <= M_RST7)))
	  { fprintf(stderr,"JPEG 411: unexp marker (%lx)\n",jpg_marker); 
            return(FALSE);
	  }
          jpg_marker = 0;
	}
	else if (jpg_read_RST_marker()==FALSE)
		{ fprintf(stderr,"JPEG 411 RST marker err\n"); return(FALSE); }
	jpg_comps[0].dc = jpg_comps[1].dc = jpg_comps[2].dc = 0;
	rst_count = jpg_rst_interval;
      } else rst_count--;

	/* Y0 Y1 Y2 Y3 U V */
      jpg_huffparse(&jpg_comps[0],jpg_dct_buf,qtab0,Ybuf0); Ybuf0 += DCTSIZE2;
      jpg_huffparse(&jpg_comps[0],jpg_dct_buf,qtab0,Ybuf0); Ybuf0 += DCTSIZE2;
      jpg_huffparse(&jpg_comps[0],jpg_dct_buf,qtab0,Ybuf1); Ybuf1 += DCTSIZE2;
      jpg_huffparse(&jpg_comps[0],jpg_dct_buf,qtab0,Ybuf1); Ybuf1 += DCTSIZE2;
      jpg_huffparse(&jpg_comps[1],jpg_dct_buf,qtab1,Ubuf);  Ubuf += DCTSIZE2;
      jpg_huffparse(&jpg_comps[2],jpg_dct_buf,qtab2,Vbuf);  Vbuf += DCTSIZE2;
    } /* end of mcu_cols */

    (void)(color_func)(iptr,imagex,imagey,mcu_row_size,
						orow_size,map_flag,map,chdr);
    imagey -= 16;  iptr += (orow_size << 4);

  } /* end of mcu_rows */
  if (jpg_marker) { jpg_h_bbuf = 0; jpg_h_bnum = 0; }
DEBUG_LEVEL1 fprintf(stderr,"411: done\n");
  return(TRUE);
}
