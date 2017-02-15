
/*
 * xa_jpg.c
 *
 * Copyright (C) 1994-1998,1999 by Mark Podlipec.
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

/* Revision History
 *
 * 22Aug95 - +F dithering to MPEG.
 * 18Nov98 - Added support for QT MJPA format
 * 21Mar99 - Added default DQT tables for JPEGs that don't have them.
 */

#include "xa_jpg.h"

#include "xa_color.h"
#include "xa_xmpg.h"
#include "xa_formats.h"
YUVBufs jpg_YUVBufs;
extern YUVTabs def_yuv_tabs;

extern xaLONG xa_dither_flag;

/* JPEG Specific Variables */
xaUBYTE  *jpg_buff = 0;
xaLONG  jpg_bsize = 0;

xaLONG   jpg_h_bnum;  /* this must be signed */
xaULONG  jpg_h_bbuf;

xaUBYTE  *jpg_samp_limit = 0;
xaUBYTE  *xa_byte_limit = 0;

xaULONG  jpg_init_flag = xaFALSE;
xaLONG   *jpg_quant_tables[JJ_NUM_QUANT_TBLS];
xaULONG  jpg_marker = 0;
xaULONG  jpg_saw_SOI,jpg_saw_SOF,jpg_saw_SOS;
xaULONG  jpg_saw_DHT,jpg_saw_DQT,jpg_saw_EOI;
xaULONG  jpg_std_DHT_flag = 0;
xaLONG   jpg_dprec,jpg_height,jpg_width;
xaLONG   jpg_num_comps,jpg_comps_in_scan;
xaLONG   jpg_nxt_rst_num;
xaLONG   jpg_rst_interval;

xaULONG xa_mjpg_kludge;

static void JPG_Double_MCU();


JJ_HUFF_TBL jpg_ac_huff[JJ_NUM_HUFF_TBLS];
JJ_HUFF_TBL jpg_dc_huff[JJ_NUM_HUFF_TBLS];

#define JPG_MAX_COMPS 4
#define JPG_DUMMY_COMP (JPG_MAX_COMPS + 1)
COMPONENT_HDR jpg_comps[JPG_MAX_COMPS + 1];


#define JJ_INPUT_SKIP(len) \
{ int _tlen =  (len > jpg_bsize)?(jpg_bsize):(len); \
  jpg_buff += _tlen; jpg_bsize -= _tlen; }
#define JJ_INPUT_CHECK(val)  ((jpg_bsize >= (val))?(xaTRUE):(xaFALSE))
#define JJ_INPUT_xaBYTE(var) { var = *jpg_buff++; jpg_bsize--; }
#define JJ_INPUT_xaSHORT(var) \
 { var = (*jpg_buff++) << 8; var |= (*jpg_buff++); jpg_bsize -= 2; }
#define JJ_INPUT_xaLONG(var) \
 { var  = (*jpg_buff++) << 24; var |= (*jpg_buff++) << 16; \
   var |= (*jpg_buff++) << 8; var |= (*jpg_buff++); jpg_bsize -= 4; }

xaLONG JJ_ZAG[DCTSIZE2+16] = {
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

xaULONG jpg_MCUbuf_size = 0;
xaUBYTE *jpg_Ybuf = 0;
xaUBYTE *jpg_Ubuf = 0;
xaUBYTE *jpg_Vbuf = 0;
xaSHORT jpg_dct_buf[DCTSIZE2];


/* FUNCTIONS */
xaULONG JFIF_Decode_JPEG();
xaULONG jpg_read_markers();
xaULONG jpg_decode_111111();
xaULONG jpg_decode_211111();
xaULONG jpg_decode_221111();
xaULONG jpg_decode_411111();
void  jpg_huff_build();
xaULONG jpg_huffparse();
extern void j_rev_dct();
extern void j_rev_dct_sparse();
void JPG_Setup_Samp_Limit_Table();
void JPG_Free_Samp_Limit_Table();
xaULONG jpg_read_EOI_marker();
xaULONG jpg_std_DHT();
xaULONG jpg_std_DQT();
void JPG_Alloc_MCU_Bufs();
void JPG_Free_MCU_Bufs();
xaULONG jpg_search_marker();
xaULONG jpg_read_SOF();
xaULONG jpg_skip_marker();
xaULONG jpg_get_marker();
xaULONG jpg_read_appX();
void jpg_init_input();
void jpg_huff_reset();
void JFIF_Read_IJPG_Tables();
void JFIF_Init_IJPG_Tables();
char IJPG_Tab1[64];
char IJPG_Tab2[64];
xaULONG jpg_check_for_marker();
xaULONG jpg_skip_to_nxt_rst();

/* external */
extern void XA_Gen_YUV_Tabs();
extern void XA_MCU111111_To_Gray();
extern void *XA_MCU111111_Func();
extern void *XA_MCU211111_Func();
extern void *XA_MCU221111_Func();
extern void *XA_MCU411111_Func();
extern void XA_Add_Func_To_Free_Chain();

/******* JFIF extern stuff and variables ***************/
XA_ACTION *ACT_Get_Action();
XA_CHDR   *ACT_Get_CMAP();
XA_CHDR *CMAP_Create_332();
XA_CHDR *CMAP_Create_Gray();
XA_CHDR *CMAP_Create_CHDR_From_True();
xaUBYTE *UTIL_RGB_To_Map();
xaUBYTE *UTIL_RGB_To_FS_Map();
xaULONG UTIL_Get_LSB_Short();
void ACT_Setup_Mapped();
void ACT_Add_CHDR_To_Action();
xaULONG CMAP_Find_Closest();
extern XA_ANIM_SETUP *XA_Get_Anim_Setup();
extern void XA_Free_Anim_Setup();
extern void ACT_Setup_Delta();


typedef struct
{
  int		valid;
  int		field_sz;
  int		pad_field_sz;
  int		next_off;
  int		quant_off;
  int		huff_off;
  int		image_off;
  int		scan_off;
  int		data_off;
} QT_JPEG_INFO;
static QT_JPEG_INFO qt_jpeg_info;

typedef struct
{
  int		valid;
  int		ileave;
} AVI_JPEG_INFO;
static AVI_JPEG_INFO avi_jpeg_info;

/*******
 *
 */
void jpg_init_stuff()
{ xaLONG i;
  DEBUG_LEVEL1 fprintf(stderr,"JJINIT\n");
  if (jpg_init_flag==xaTRUE) return;
  for(i=0; i < JJ_NUM_QUANT_TBLS; i++) jpg_quant_tables[i] = 0;
  if (jpg_samp_limit==0) JPG_Setup_Samp_Limit_Table(0);
  jpg_init_flag = xaTRUE;
  jpg_std_DHT_flag = 0;
  qt_jpeg_info.valid = xaFALSE;
  avi_jpeg_info.valid = xaFALSE;
}


/*******
 *
 */
void jpg_free_stuff()
{ xaLONG i;
  DEBUG_LEVEL1 fprintf(stderr,"JJFREE\n");
  if (jpg_init_flag == xaFALSE) return;
  for(i=0; i < JJ_NUM_QUANT_TBLS; i++)
    if (jpg_quant_tables[i]) { free(jpg_quant_tables[i]); jpg_quant_tables[i]=0; }
  if (jpg_samp_limit)	{ free(jpg_samp_limit); jpg_samp_limit = 0; }
  jpg_init_flag = xaFALSE;
  jpg_std_DHT_flag = 0;
}

#define JFIF_APP0_LEN 14

/*****
 *
 */
xaULONG JFIF_Read_File(const char *fname,XA_ANIM_HDR *anim_hdr)
{ XA_INPUT *xin = anim_hdr->xin;
  ACT_DLTA_HDR *dlta_hdr = 0;
  XA_ACTION *act = 0;
  xaULONG jfif_flen;
  xaUBYTE *inbuff = 0;
  XA_ANIM_SETUP *jfif;

  jfif = XA_Get_Anim_Setup();
  jfif->cmap_frame_num = 1; /* always 1 image per file */
  jfif->vid_time   = XA_GET_TIME( 200 );

   /* find file size */
  xin->Seek_FPos(xin,0L,2);
  jfif_flen = xin->Get_FPos(xin); /* POD may not be portable */
  xin->Seek_FPos(xin,0L,0);

  if (xa_file_flag==xaTRUE)
  { xaLONG ret;
    dlta_hdr = (ACT_DLTA_HDR *) malloc(sizeof(ACT_DLTA_HDR));
    if (dlta_hdr == 0) TheEnd1("JFIF: malloc failed");
    dlta_hdr->flags = 0;
    dlta_hdr->fpos  = xin->Get_FPos(xin);
    dlta_hdr->fsize = jfif_flen;
    jfif->max_fvid_size = jfif_flen + 16;
    inbuff = (xaUBYTE *)malloc(jfif_flen);
    ret = xin->Read_Block(xin,inbuff,jfif_flen);
    if (ret < jfif_flen)
		{fprintf(stderr,"JFIF: inbuff read failed\n"); return(xaFALSE);}
  }
  else
  { xaLONG ret,d;
    d = jfif_flen + (sizeof(ACT_DLTA_HDR));
    dlta_hdr = (ACT_DLTA_HDR *) malloc( d );
    if (dlta_hdr == 0) TheEnd1("JFIF: malloc failed");
    dlta_hdr->flags = DLTA_DATA;
    dlta_hdr->fpos = 0; dlta_hdr->fsize = jfif_flen;
    ret = xin->Read_Block(xin, dlta_hdr->data, jfif_flen);
    if (ret < jfif_flen)
		{fprintf(stderr,"JFIF: read failed\n"); return(xaFALSE);}
    inbuff = dlta_hdr->data;
  }

  /******** EXTRACT IMAGE SIZE ****************/
  jpg_init_input(inbuff,jfif_flen);
  jpg_saw_SOF = xaFALSE;
  while(jpg_get_marker() == xaTRUE)
  { 
    if (   (jpg_marker == M_SOF0)	/* baseline */
        || (jpg_marker == M_SOF1) )	/* extended sequential */
    {
      if (jpg_read_SOF()==xaTRUE) jpg_saw_SOF = xaTRUE;
      break;
    }
    else if (jpg_marker == M_SOF2)
    {
      fprintf(stderr,"JPEG: progessive images not yet supported\n");
      return(xaFALSE);
    }
    else if ((jpg_marker == M_SOI) || (jpg_marker == M_TEM)) continue;
    else if ((jpg_marker >= M_RST0) && (jpg_marker <= M_RST7)) continue;
    else jpg_skip_marker();
  }
  if (jpg_saw_SOF == xaFALSE)
  {
    fprintf(stderr,"JPEG: not recognized as a JPEG file\n");
    return(xaFALSE);
  }
  jfif->imagex = 4 * ((jpg_width + 3)/4);
  jfif->imagey = 2 * ((jpg_height + 1)/2);
  JPG_Alloc_MCU_Bufs(anim_hdr,jfif->imagex,0,xaFALSE);
  XA_Gen_YUV_Tabs(anim_hdr);

  if (xa_verbose)
  {
    fprintf(stderr,"JFIF: size %d by %d\n",jfif->imagex,jfif->imagey);
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
    if (   (cmap_true_map_flag == xaFALSE) /* depth 24 and not true_map */
        || (xa_buffer_flag == xaFALSE) )
    {
	if (cmap_true_to_332 == xaTRUE)
		jfif->chdr = CMAP_Create_332(jfif->cmap,&jfif->imagec);
	else	jfif->chdr = CMAP_Create_Gray(jfif->cmap,&jfif->imagec);
    }
  }
  else
  {
    fprintf(stderr,"JFIF: unsupported number of components %d\n",jpg_num_comps);
    return(xaFALSE);
  }

  act = ACT_Get_Action(anim_hdr,ACT_DELTA);
  act->data = (xaUBYTE *)dlta_hdr;

  dlta_hdr->xpos = dlta_hdr->ypos = 0;
  dlta_hdr->xsize = jfif->imagex;
  dlta_hdr->ysize = jfif->imagey;
  dlta_hdr->special = 0;
  dlta_hdr->extra = (void *)(0);
  dlta_hdr->xapi_rev = 0x0002;
  dlta_hdr->delta = JFIF_Decode_JPEG;

  jfif->pic = 0; 
  ACT_Setup_Delta(jfif,act,dlta_hdr,xin);

  xin->Close_File(xin);
  /* free up buffer in necessary */
  if (xa_file_flag==xaTRUE) {free(inbuff); inbuff = 0; }
  
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
  if (xa_buffer_flag == xaFALSE) anim_hdr->anim_flags |= ANIM_SNG_BUF;
  if (xa_file_flag == xaTRUE) anim_hdr->anim_flags |= ANIM_USE_FILE;
  anim_hdr->anim_flags |= ANIM_FULL_IM;
  anim_hdr->max_fvid_size = jfif->max_fvid_size;
  anim_hdr->max_faud_size = 0;
  anim_hdr->fname = anim_hdr->name;
  XA_Free_Anim_Setup(jfif);
  return(xaTRUE);
}

/********* END OF JFIF Code *************************************************/



/******* 
 *
 */
xaULONG jpg_read_SOI()
{
  DEBUG_LEVEL1 fprintf(stderr,"SOI: \n");
  jpg_rst_interval = 0;
  return(xaTRUE);
}


/******* 
 *
 */
xaULONG jpg_read_SOF()
{
  xaLONG len,ci;
  COMPONENT_HDR *comp;
  
  JJ_INPUT_xaSHORT(len);
  if (xa_mjpg_kludge) len -= 6;
  else len -= 8;

  JJ_INPUT_xaBYTE(jpg_dprec);
  JJ_INPUT_xaSHORT(jpg_height);
  JJ_INPUT_xaSHORT(jpg_width);
  JJ_INPUT_xaBYTE(jpg_num_comps);

  DEBUG_LEVEL1 fprintf(stderr,"SOF: dprec %x res %d x %d comps %x\n",jpg_dprec,jpg_width,jpg_height,jpg_num_comps);

  for(ci = 0; ci < jpg_num_comps; ci++)
  { xaULONG c;
    if (ci > JPG_MAX_COMPS)	comp = &jpg_comps[JPG_DUMMY_COMP];
    else			comp = &jpg_comps[ci];
    JJ_INPUT_xaBYTE(comp->id);
    JJ_INPUT_xaBYTE(c);
    comp->hvsample = c;
    JJ_INPUT_xaBYTE(comp->qtbl_num);
    DEBUG_LEVEL1 fprintf(stderr,"   id %x hvsamp %x qtbl %x\n",comp->id,c,comp->qtbl_num);
  }
  return(JJ_INPUT_CHECK(0));
}

/******* 
 *
 */
xaULONG jpg_read_SOS()
{ xaLONG len,i;
  xaLONG jpg_Ss, jpg_Se, jpg_AhAl;

  JJ_INPUT_xaSHORT(len);
  /* if (xa_mjpg_kludge) len += 2; length ignored */

  JJ_INPUT_xaBYTE(jpg_comps_in_scan);

  DEBUG_LEVEL1 fprintf(stderr,"SOS: comps %d\n",jpg_comps_in_scan);

  for (i = 0; i < jpg_comps_in_scan; i++)
  { xaLONG j,comp_id,htbl_num;
    COMPONENT_HDR *comp = 0;

    JJ_INPUT_xaBYTE(comp_id);
    j = 0;
    while(j < jpg_num_comps)
    { comp = &jpg_comps[j];
      if (comp->id == comp_id) break;
      j++;
    }
    if (j > jpg_num_comps) 
	{ fprintf(stderr,"JJ: bad id %x",comp_id); return(xaFALSE); }

    JJ_INPUT_xaBYTE(htbl_num);
    comp->dc_htbl_num = (htbl_num >> 4) & 0x0f;
    comp->ac_htbl_num = (htbl_num     ) & 0x0f;
    DEBUG_LEVEL1 fprintf(stderr,"     id %x dc/ac %x\n",comp_id,htbl_num);
  }
  JJ_INPUT_xaBYTE(jpg_Ss);
  JJ_INPUT_xaBYTE(jpg_Se);
  JJ_INPUT_xaBYTE(jpg_AhAl);
  return(JJ_INPUT_CHECK(0));
}

/******* 
 *
 */
xaULONG jpg_read_DQT()
{ xaLONG len;
  JJ_INPUT_xaSHORT(len);
  if ( !xa_mjpg_kludge ) len -= 2;

  DEBUG_LEVEL1 fprintf(stderr,"DQT:\n");

  while(len > 0)
  { xaLONG i,tbl_num,prec;
    xaLONG *quant_table;

    JJ_INPUT_xaBYTE(tbl_num);  len -= 1;
    DEBUG_LEVEL1 fprintf(stderr,"     prec/tnum %02x\n",tbl_num);

    prec = (tbl_num >> 4) & 0x0f;
    prec = (prec)?(2 * DCTSIZE2):(DCTSIZE2);  /* 128 or 64 */
    tbl_num &= 0x0f;
    if (tbl_num > 4)
	{ fprintf(stderr,"JJ: bad DQT tnum %x\n",tbl_num); return(xaFALSE); }

    if (jpg_quant_tables[tbl_num] == 0)
    {
      jpg_quant_tables[tbl_num] = (xaLONG *)malloc(64 * sizeof(xaLONG));
      if (jpg_quant_tables[tbl_num] == 0)
	{ fprintf(stderr,"JJ: DQT alloc err %x \n",tbl_num); return(xaFALSE); }
    }
    len -= prec;
    if (JJ_INPUT_CHECK(prec)==xaFALSE) return(xaFALSE);
    quant_table = jpg_quant_tables[tbl_num];
    if (prec==128)
    { xaULONG tmp; 
      for (i = 0; i < DCTSIZE2; i++)
	{ JJ_INPUT_xaSHORT(tmp); quant_table[ JJ_ZAG[i] ] = (xaLONG) tmp; }
    }
    else
    { xaULONG tmp; 
      for (i = 0; i < DCTSIZE2; i++)
	{ JJ_INPUT_xaBYTE(tmp); quant_table[ JJ_ZAG[i] ] = (xaLONG) tmp; }
    }
  }
  return(xaTRUE);
}

/******* 
 * Handle DRI chunk 
 */
xaULONG jpg_read_DRI()
{ xaLONG len;
  JJ_INPUT_xaSHORT(len);
  JJ_INPUT_xaSHORT(jpg_rst_interval);
  DEBUG_LEVEL1 fprintf(stderr,"DRI: int %x\n",jpg_rst_interval);
  return(xaTRUE);
}

/******* 
 *
 */
xaULONG jpg_skip_marker()
{ xaLONG len,tmp;
  JJ_INPUT_xaSHORT(len); 
  DEBUG_LEVEL1 fprintf(stderr,"SKIP: marker %x len %x\n",jpg_marker,len);
  len -= 2;  if (len <=0) return(xaFALSE);
  if (JJ_INPUT_CHECK(len)==xaFALSE) return(xaFALSE);
  while(len--) JJ_INPUT_xaBYTE(tmp); /* POD improve this */
  return(xaTRUE);
}

/*******
 *
 */
xaULONG jpg_read_DHT()
{
  xaLONG len;
  xaULONG ret = xaFALSE;
  JJ_HUFF_TBL *htable;
  xaUBYTE  *hbits;
  xaUBYTE  *hvals;

  jpg_std_DHT_flag = 0;
  JJ_INPUT_xaSHORT(len);

  DEBUG_LEVEL1 fprintf(stderr,"DHT: len %d\n",len);

  if (xa_mjpg_kludge) len += 2;
  len -= 2;
  if (JJ_INPUT_CHECK(len)==xaFALSE) return(xaFALSE);

  while(len > 0)
  { xaLONG i,index,count;
    JJ_INPUT_xaBYTE(index);
    len--;
    /* POD index check */
    if (index & 0x10)				/* AC Table */
    {
      index &= 0x0f;
      if (index >= JJ_NUM_HUFF_TBLS) break; /* return(xaFALSE); */
      htable = &(jpg_ac_huff[index]);
      hbits  = jpg_ac_huff[index].bits;
      hvals  = jpg_ac_huff[index].vals;
    }
    else					/* DC Table */
    {
      index &= 0x0f;
      if (index >= JJ_NUM_HUFF_TBLS) break; /* return(xaFALSE); */
      htable = &(jpg_dc_huff[index]);
      hbits  = jpg_dc_huff[index].bits;
      hvals  = jpg_dc_huff[index].vals;
    }
    hbits[0] = 0;		count = 0;
    if (len < 16) break;
    for (i = 1; i <= 16; i++)
    {
      JJ_INPUT_xaBYTE(hbits[i]);
      count += hbits[i];
    }
    len -= 16;
    if (count > 256)
    { fprintf(stderr,"JJ: DHT bad count %d defaulting to STD.\n",count);
      break;
    }
    if (len < count)
    { fprintf(stderr,"JJ: DHT count(%d) > len(%d).\n",count,len);
      break;
    }

    for (i = 0; i < count; i++) JJ_INPUT_xaBYTE(hvals[i]);
    len -= count;

    jpg_huff_build(htable,hbits,hvals);
    ret = xaTRUE;
  } /* end of len */

  if (ret == xaFALSE)
  { xaULONG garb;
    /* Something is roached, but what the heck, try default DHT instead */
    DEBUG_LEVEL1 fprintf(stderr,"bad DHT trying STD instead.\n");
    while(len > 0)
    { len--;
      JJ_INPUT_xaBYTE(garb);
    }
    jpg_std_DHT();
    return(xaTRUE); 
  }
  return(xaTRUE);
}

/*******
 *
 */
xaULONG jpg_get_marker()
{
  xaLONG c;

  for(;;)
  {
    JJ_INPUT_xaBYTE(c);
    while(c != 0xFF)    /* look for FF */
    {
      if (JJ_INPUT_CHECK(1)==xaFALSE) return(xaFALSE);
      JJ_INPUT_xaBYTE(c);
    }
    /* now we've got 1 0xFF, keep reading until not 0xFF */
    do
    {
      if (JJ_INPUT_CHECK(1)==xaFALSE) return(xaFALSE);
      JJ_INPUT_xaBYTE(c);
    } while (c == 0xFF);
    if (c != 0) break; /* ignore FF/00 sequences */
  }
  jpg_marker = c;
  return(xaTRUE);
}


/******* 
 *
 */
xaULONG jpg_read_markers()
{
  for(;;)
  { 
    if (jpg_get_marker() == xaFALSE) return(xaFALSE);
DEBUG_LEVEL1 fprintf(stderr,"JJ: marker %x\n",jpg_marker);
    switch(jpg_marker)
    {
      case M_SOI: 
	if (jpg_read_SOI()==xaFALSE) return(xaFALSE);
	jpg_saw_SOI = xaTRUE;
	break;
      case M_SOF0: 
      case M_SOF1: 
      case M_SOF2: 
	if (jpg_read_SOF()==xaFALSE) return(xaFALSE);
	jpg_saw_SOF = xaTRUE;
	break;
	/* Not yet supported */
     case M_SOF3:
     case M_SOF5:
     case M_SOF6:
     case M_SOF7:
     case M_SOF9:
     case M_SOF10:
     case M_SOF11:
     case M_SOF13:
     case M_SOF14:
     case M_SOF15:
	return(xaFALSE);
	break;
      case M_SOS: 
	if (jpg_read_SOS()==xaFALSE) return(xaFALSE);
	jpg_saw_SOS = xaTRUE;
	jpg_nxt_rst_num = 0;
	return(xaTRUE);
	break;
      case M_DHT:
	if (jpg_read_DHT()==xaFALSE) return(xaFALSE);
	jpg_saw_DHT = xaTRUE;
	break;
      case M_DQT:
	if (jpg_read_DQT()==xaFALSE) return(xaFALSE);
	jpg_saw_DQT = xaTRUE;
	break;
      case M_DRI:
	if (jpg_read_DRI()==xaFALSE) return(xaFALSE);
	break;
      case M_COM: /* COMMENT */
	{ int len;
          JJ_INPUT_xaSHORT(len);
	  len -= 2;
	  if (xa_verbose) fprintf(stdout,"JPG Comment: ");
	  while(len > 0)
          { int d; 
            JJ_INPUT_xaBYTE(d); len--;
	    if ((xa_verbose) && (d >= 20))  fputc(d,stdout);
          }
	  if (xa_verbose) fprintf(stdout,"\n");
	}
	break;
      case M_APP0:
      case M_APP1:
	if (jpg_read_appX() == xaFALSE) return(xaFALSE);
	break;
      case M_EOI:
	fprintf(stderr,"JJ: reached EOI without data\n");
	return(xaFALSE);
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
	DEBUG_LEVEL1 fprintf(stderr,"JJ: unknown marker %x\n",jpg_marker);
	if (jpg_skip_marker()==xaFALSE) return(xaFALSE);
	break;
    } /* end of switch */
  } /* end of forever */
}

/* Good lord, is there no standard */
xaULONG jpg_read_appX()
{ xaLONG len;
  JJ_INPUT_xaSHORT(len);
  len -= 2;
  if (len > 4)
  { xaULONG first;
    JJ_INPUT_xaLONG(first);
    len -= 4;
    if (first == 0x41564931)  /* AVI1 */
    { int interleave;
      JJ_INPUT_xaBYTE(interleave);
      len--;
      avi_jpeg_info.valid = xaTRUE;
      avi_jpeg_info.ileave = interleave;
    }
    else if (len > (0x28 - 4)) /* Maybe APPLE MJPEG A */
    { xaULONG jid;
      JJ_INPUT_xaLONG(jid);
      len -= 4;
      if (jid == 0x6D6A7067)  /* TODO: define JPEG_APP1_mjpg */
      {  qt_jpeg_info.valid = xaTRUE;
	 JJ_INPUT_xaLONG(qt_jpeg_info.field_sz);
	 JJ_INPUT_xaLONG(qt_jpeg_info.pad_field_sz);
	 JJ_INPUT_xaLONG(qt_jpeg_info.next_off);
	 JJ_INPUT_xaLONG(qt_jpeg_info.quant_off);
	 JJ_INPUT_xaLONG(qt_jpeg_info.huff_off);
	 JJ_INPUT_xaLONG(qt_jpeg_info.image_off);
	 JJ_INPUT_xaLONG(qt_jpeg_info.scan_off);
	 JJ_INPUT_xaLONG(qt_jpeg_info.data_off);
	 len -= 32;
      }
    }
  }
  if (len) JJ_INPUT_SKIP(len);
  return(xaTRUE);
}


/*****
 *
 */
void jpg_huff_build(htbl,hbits,hvals)
JJ_HUFF_TBL *htbl; 
xaUBYTE *hbits,*hvals;
{ xaULONG clen,num_syms,p,i,si,code,lookbits;
  xaULONG l,ctr;
  xaUBYTE huffsize[257];
  xaULONG huffcode[257];

  /*** generate code lengths for each symbol */
  num_syms = 0;
  for(clen = 1; clen <= 16; clen++)
  {
    for(i = 1; i <= (xaULONG)(hbits[clen]); i++) 
				huffsize[num_syms++] = (xaUBYTE)(clen);
  }
  huffsize[num_syms] = 0;

  /*** generate codes */
  code = 0;
  si = huffsize[0];
  p = 0;
  while (huffsize[p])
  {
    while ( ((xaULONG)huffsize[p]) == si) 
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
      p += (xaULONG)(htbl->bits[l]);
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
  memset((char *)htbl->cache, 0, ((1<<HUFF_LOOKAHEAD) * sizeof(xaUSHORT)) );
  p = 0;
  for (l = 1; l <= HUFF_LOOKAHEAD; l++) 
  {
    for (i = 1; i <= (xaULONG) htbl->bits[l]; i++, p++) 
    { xaSHORT the_code = (xaUSHORT)((l << 8) | htbl->vals[p]);

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
xaUBYTE *buff;
xaLONG buff_size;
{
  jpg_buff = buff;
  jpg_bsize = buff_size;
}

/*******
 *
 */
xaULONG
JFIF_Decode_JPEG(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC2_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;
  void *extra = dec_info->extra;
  xaLONG base_y;
  xaULONG jpg_type;
  xaULONG interleave,row_offset;

  dec_info->xs = dec_info->ys = 0; 
  dec_info->xe = imagex; dec_info->ye = imagey;

    /* Indicate we can drop these frames */
  if (dec_info->skip_flag > 0) return(ACT_DLTA_DROP);

/* JPEG_TYPE passed in via extra
 *
 * 0x01		interleaved jpeg files
 * 0x40         AVI MJPG kludge(they screwed up the marker lengths)
 * 0x10		IJPG/JMOV images(no upfront markers)
 *
 */

  jpg_type = (xaULONG)(extra);
  xa_mjpg_kludge = (jpg_type & 0x40)?(0x40):(0x00);

  if (jpg_init_flag==xaFALSE) jpg_init_stuff();

/* init buffer stuff */
  jpg_init_input(delta,dsize);

  base_y = 0;
  while(base_y < (imagey - 4)) /* the 4 is just a fuzz factor  TODO do better */
  {
    jpg_saw_EOI = jpg_saw_DHT = xaFALSE;
    if (jpg_type & 0x10) /* IJPG/JMOV File */
    { xaLONG ci,*qtbl0,*qtbl1;
      jpg_saw_SOI = jpg_saw_SOF = jpg_saw_SOS = jpg_saw_DQT = xaTRUE;
      jpg_saw_EOI = xaTRUE;

      if (base_y == 0) /* 1st time through */
      {
        jpg_huff_reset(); /* only  1st time */
        jpg_width  = imagex; 
	if (jpg_type & 0x01) 
		jpg_height = (imagey + 1) >> 1;/* interleaved */
        else	jpg_height = imagey;
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
          jpg_quant_tables[0] = (xaLONG *)malloc(64 * sizeof(xaLONG));
          if (jpg_quant_tables[0] == 0)
            { fprintf(stderr,"JJ: IJPG DQT0 alloc err \n"); return(xaFALSE); }
        }
        if (jpg_quant_tables[1] == 0)
        {
          jpg_quant_tables[1] = (xaLONG *)malloc(64 * sizeof(xaLONG));
          if (jpg_quant_tables[1] == 0)
            { fprintf(stderr,"JJ: IJPG DQT1 alloc err \n"); return(xaFALSE); }
        }
        qtbl0 = jpg_quant_tables[0];
        qtbl1 = jpg_quant_tables[1];
        for(ci=0; ci < DCTSIZE2; ci++)
        {
          qtbl0[ JJ_ZAG[ci] ] = (xaLONG)IJPG_Tab1[ci];
          qtbl1[ JJ_ZAG[ci] ] = (xaLONG)IJPG_Tab2[ci];
        }
      } else jpg_saw_DHT = xaTRUE; /* 2nd time through IJPG */

      if (jpg_type & 0x01)
      {
	interleave = 2;
	if (jpg_type & 0x2)  /* first field is even/top field */
	{
	  row_offset = (base_y)?(1):(0);
	}
	else /* first field is odd/bottom field */
	{
	  row_offset = (base_y)?(0):(1);
	}
      }
      else
      {
	interleave = 1;
	row_offset = 0;
      }

    } else
    {
      /* read markers */
      jpg_saw_SOI = jpg_saw_SOF = jpg_saw_SOS = jpg_saw_DHT = jpg_saw_DQT = xaFALSE;
      if (jpg_read_markers() == xaFALSE) { jpg_free_stuff(); 
	fprintf(stderr,"JPG: rd marker err\n"); return(ACT_DLTA_NOP); }
      jpg_huff_reset();

DEBUG_LEVEL1 fprintf(stderr,"jpg_height %d imagey %d qt_jpeg.valid %d\n",
				jpg_height, imagey, qt_jpeg_info.valid);
      interleave = (jpg_height <= ((imagey>>1)+1) )?(2):(1);
      if (interleave == 2)
      {
	if (avi_jpeg_info.valid == xaTRUE)
	{
	  switch(avi_jpeg_info.ileave)
	  {
	    case 0:
	    case 2: row_offset = 0; break;
	    case 1: row_offset = 1; break;
	    default:
		DEBUG_LEVEL1 fprintf(stderr,"AVIJPEG: unknown ileave %d\n",
							avi_jpeg_info.ileave);
		row_offset = 0;
	        break;
	  }
	}
        else if (jpg_type & 0x2)  /* first field is even/top field */
        {
          row_offset = (base_y == 0)?(0):(1);
        }
        else  /* first field is odd/bottom field */
        {
          row_offset = (base_y == 0)?(1):(0);
        }
      }
      else row_offset = 0;
    }
    jpg_marker = 0x00;
    if (jpg_width > imagex) JPG_Alloc_MCU_Bufs(0,jpg_width,0,xaFALSE);


    if ((jpg_saw_DHT != xaTRUE) && (jpg_std_DHT_flag==0))
    {
      DEBUG_LEVEL1 fprintf(stderr,"standard DHT tables\n");
      (void)jpg_std_DHT();
    }

    if (jpg_saw_DQT != xaTRUE)
    { 
      DEBUG_LEVEL1 fprintf(stderr,"JPG: warning no DQT table - guessing\n");
      (void)jpg_std_DQT(100);
    }


    DEBUG_LEVEL1 fprintf(stderr,"JJ: imagexy %d %d  jjxy %d %d basey %d\n",imagex,imagey,jpg_width,jpg_height,base_y);

    if (   (jpg_num_comps == 3) && (jpg_comps_in_scan == 3) 
	&& (jpg_comps[1].hvsample == 0x11) && (jpg_comps[2].hvsample== 0x11) )
    {
      if (jpg_comps[0].hvsample == 0x41) /* 411 */
	{ jpg_decode_411111(image,jpg_width,jpg_height,interleave,row_offset,
			imagex,imagey,dec_info); }
      else if (jpg_comps[0].hvsample == 0x22) /* 411 */
	{ jpg_decode_221111(image,jpg_width,jpg_height,interleave,row_offset,
			imagex,imagey,dec_info); }
      else if (jpg_comps[0].hvsample == 0x21) /* 211 */
	{ jpg_decode_211111(image,jpg_width,jpg_height,interleave,row_offset,
			imagex,imagey,dec_info); }
      else if (jpg_comps[0].hvsample == 0x11) /* 111 */
	{ jpg_decode_111111(image,jpg_width,jpg_height,interleave,row_offset,
			imagex,imagey,dec_info,0); }
      else 
      { fprintf(stderr,"JPG: cmps %d %d mcu %04x %04x %04x unsupported\n",
		jpg_num_comps,jpg_comps_in_scan,jpg_comps[0].hvsample,
		jpg_comps[1].hvsample,jpg_comps[2].hvsample); 
        break;
      }
    } 
    else if ( (jpg_num_comps == 1) || (jpg_comps_in_scan == 1) )
    {
      jpg_decode_111111(image,jpg_width,jpg_height,interleave,row_offset,
			imagex,imagey,dec_info,1);
    }
    else
    { fprintf(stderr,"JPG: cmps %d %d mcu %04x %04x %04x unsupported.\n",
		jpg_num_comps,jpg_comps_in_scan,jpg_comps[0].hvsample,
		jpg_comps[1].hvsample,jpg_comps[2].hvsample); 
      break;
    }

    base_y += ((interleave == 1)?(imagey):(jpg_height));
    if (jpg_marker == M_EOI) { jpg_saw_EOI = xaTRUE; jpg_marker = 0x00; }
    else if (jpg_saw_EOI==xaFALSE) if (jpg_read_EOI_marker() == xaFALSE) break;
  }
  if (map_flag==xaTRUE) return(ACT_DLTA_MAPD);
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
xaULONG jpg_read_EOI_marker()
{ 
  /* POD make sure previous code restores bit buffer to input stream */ 
  while( jpg_get_marker() == xaTRUE)
  {
    if (jpg_marker == M_EOI) {jpg_saw_EOI = xaTRUE; return(xaTRUE); }
  }
  return(xaFALSE);
}

void JPG_Free_MCU_Bufs()
{
  if (jpg_Ybuf)		{ free(jpg_Ybuf); jpg_Ybuf = 0; }
  if (jpg_Ubuf)		{ free(jpg_Ubuf); jpg_Ubuf = 0; }
  if (jpg_Vbuf)		{ free(jpg_Vbuf); jpg_Vbuf = 0; }
  jpg_YUVBufs.Ybuf = 0; jpg_YUVBufs.Ubuf = 0; jpg_YUVBufs.Vbuf = 0;
}

/*******
 *
 */
void JPG_Alloc_MCU_Bufs(anim_hdr,width,height,full_flag)
XA_ANIM_HDR *anim_hdr;
xaULONG width, height, full_flag;
{ xaULONG twidth  = (width + 15) / 16;
  xaULONG theight = (height + 15) / 16;  if (theight & 1) theight++;

  if (full_flag==xaTRUE) twidth *= (theight << 2);
  else		 	 twidth <<= 2; /* four dct's deep */

DEBUG_LEVEL1 fprintf(stderr,"Alloc_MCU: wh %d %d twid %d act %d\n",
			width,height,twidth,(twidth * DCTSIZE2) );

  if (anim_hdr) XA_Add_Func_To_Free_Chain(anim_hdr,JPG_Free_MCU_Bufs);
  if (jpg_init_flag==xaFALSE) jpg_init_stuff();
  if (jpg_Ybuf == 0)
  { jpg_MCUbuf_size = twidth;
    jpg_Ybuf = (xaUBYTE *)malloc(twidth * DCTSIZE2);
    jpg_Ubuf = (xaUBYTE *)malloc(twidth * DCTSIZE2);
    jpg_Vbuf = (xaUBYTE *)malloc(twidth * DCTSIZE2);
  }
  else if (twidth > jpg_MCUbuf_size)
  { jpg_MCUbuf_size = twidth;
    jpg_Ybuf = (xaUBYTE *)realloc( jpg_Ybuf,(twidth * DCTSIZE2) );
    jpg_Ubuf = (xaUBYTE *)realloc( jpg_Ubuf,(twidth * DCTSIZE2) );
    jpg_Vbuf = (xaUBYTE *)realloc( jpg_Vbuf,(twidth * DCTSIZE2) );
  }
  jpg_YUVBufs.Ybuf = jpg_Ybuf;
  jpg_YUVBufs.Ubuf = jpg_Ubuf;
  jpg_YUVBufs.Vbuf = jpg_Vbuf;

}

#define jpg_huff_EXTEND(val,sz) \
 ((val) < (1<<((sz)-1)) ? (val) + (((-1)<<(sz)) + 1) : (val))


/*
 * IF marker is read then jpg_marker is set and nothing more is read in.
 */

#define JJ_HBBUF_FILL8(hbbuf,hbnum)	\
{ register xaULONG _tmp;		\
  hbbuf <<= 8;				\
  if (jpg_marker) return(xaFALSE); 	\
  else { _tmp = *jpg_buff++;  jpg_bsize--; } 	\
  while(_tmp == 0xff)			\
  { register xaULONG _t1; _t1 = *jpg_buff++; jpg_bsize--; \
    if (_t1 == 0x00) break;		\
    else if (_t1 == 0xff) continue;	\
    else {jpg_marker = _t1; _tmp = 0x00; break;} \
  }					\
  hbbuf |= _tmp;	hbnum += 8;	\
}

#define JJ_HBBUF_FILL8_1(hbbuf,hbnum)	\
{ register xaULONG __tmp;		\
  hbbuf <<= 8;	hbnum += 8;		\
  if (jpg_marker) __tmp = 0x00;		\
  else { __tmp = *jpg_buff++;  jpg_bsize--; } \
  while(__tmp == 0xff)			\
  { register xaULONG _t1; _t1 = *jpg_buff++; jpg_bsize--; \
    if (_t1 == 0x00) break;		\
    else if (_t1 == 0xff) continue;	\
    else {jpg_marker = _t1; __tmp = 0x00; break;} \
  }					\
  hbbuf |= __tmp;			\
}

#define JJ_HUFF_DECODE(huff_hdr,htbl, hbnum, hbbuf, result)	\
{ register xaULONG _tmp, _hcode;				\
  while(hbnum < 16) JJ_HBBUF_FILL8_1(hbbuf,hbnum);		\
  _tmp = (hbbuf >> (hbnum - 8)) & 0xff;				\
  _hcode = (htbl)[_tmp];					\
  if (_hcode) { hbnum -= (_hcode >> 8); (result) = _hcode & 0xff; } \
  else \
  { register xaULONG _hcode, _shift, _minbits = 9;		\
    _tmp = (hbbuf >> (hbnum - 16)) & 0xffff; /* get 16 bits */	\
    _shift = 16 - _minbits;	_hcode = _tmp >> _shift; \
    while(_hcode > huff_hdr->maxcode[_minbits]) \
	{ _minbits++; _shift--; _hcode = _tmp >> _shift; } \
    if (_minbits > 16) { fprintf(stderr,"JHDerr\n"); return(xaFALSE); } \
    else  \
    {  hbnum -= _minbits;  _hcode -= huff_hdr->mincode[_minbits]; \
       result = huff_hdr->vals[ (huff_hdr->valptr[_minbits] + _hcode) ]; } \
  } \
}

#define JJ_HUFF_MASK(s) ((1 << (s)) - 1)

#define JJ_GET_BITS(n, hbnum, hbbuf, result) \
{ hbnum -= n; \
  while(hbnum < 0)  { JJ_HBBUF_FILL8_1(hbbuf,hbnum); } \
  (result) = ((hbbuf >> hbnum) & JJ_HUFF_MASK(n)); \
}

/****--------------------------------------------------------------------****
 * Look ahead to see if marker is present.  This is not 100% robust
 * in that it doesn't catch markers with multiple leading 0xFF's.
 ****--------------------------------------------------------------------****/
xaULONG jpg_check_for_marker()
{
  if (jpg_marker) return(jpg_marker);
  if (jpg_bsize < 2) return(0);
  if ((jpg_buff[0] == 0xff) && (jpg_buff[1] != 0x00))
  {
    jpg_marker = jpg_buff[1];
    DEBUG_LEVEL1
    { if (jpg_h_bnum)
      { fprintf(stderr,"JPG: check marker positive - lost %d bits\n",
								jpg_h_bnum);
      }
    }
    jpg_h_bnum = 0;
    jpg_h_bbuf = 0;
    jpg_buff += 2;
    jpg_bsize -= 2;
  }
  return(jpg_marker);
}

/****--------------------------------------------------------------------****
 * Skip ahead to next marker.
 ****--------------------------------------------------------------------****/
xaULONG jpg_skip_to_nxt_rst()
{ xaULONG d, last_ff = 0;
  jpg_h_bnum = 0;
  jpg_h_bbuf = 0;
  while( jpg_bsize )
  {
    d = *jpg_buff++; jpg_bsize--;
    if (last_ff)
    {
       if ((d != 0) && (d != 0xff)) return(d);
    }
    last_ff = (d == 0xff)?(1):(0);
  }
  return(M_EOI);
}


/*  clears dctbuf to zeroes. 
 *  fills from huffman encode stream
 */
xaULONG jpg_huffparse(comp, dct_buf, qtab, OBuf)
register COMPONENT_HDR *comp;
register xaSHORT *dct_buf;
xaULONG *qtab;
xaUBYTE *OBuf;
{ xaLONG i,dcval;
  xaULONG size;
  JJ_HUFF_TBL *huff_hdr = &(jpg_dc_huff[ comp->dc_htbl_num ]);
  xaUSHORT *huff_tbl = huff_hdr->cache;
  xaUBYTE *rnglimit = jpg_samp_limit + (CENTERJSAMPLE + MAXJSAMPLE + 1);
  xaULONG c_cnt,pos = 0;

  JJ_HUFF_DECODE(huff_hdr,huff_tbl,jpg_h_bnum,jpg_h_bbuf,size);

DEBUG_LEVEL2 { fprintf(stderr," HUFF DECODE: size %d\n",size); }

  if (size)
  { xaULONG bits;
    JJ_GET_BITS(size,jpg_h_bnum,jpg_h_bbuf,bits);
    dcval = jpg_huff_EXTEND(bits, size);
    comp->dc += dcval;
DEBUG_LEVEL2 { fprintf(stderr,"   dcval %d  -dc %d\n",dcval,comp->dc); }
  }
  dcval = comp->dc;

  /* clear reset of dct buffer */
  memset((char *)(dct_buf),0,(DCTSIZE2 * sizeof(xaSHORT)));
  dcval *= (xaLONG)qtab[0];
  dct_buf[0] = (xaSHORT)dcval;
  c_cnt = 0;

  huff_hdr = &(jpg_ac_huff[ comp->ac_htbl_num ]);
  huff_tbl = huff_hdr->cache;
  i = 1;
  while(i < 64)
  { xaLONG level;	xaULONG run,tmp;
    JJ_HUFF_DECODE(huff_hdr,huff_tbl,jpg_h_bnum,jpg_h_bbuf,tmp); 
    size =  tmp & 0x0f;
    run = (tmp >> 4) & 0x0f; /* leading zeroes */
DEBUG_LEVEL2 { fprintf(stderr,"     %d) tmp %x size %x run %x\n",
				i,tmp,size,run); }
    if (size)
    { xaLONG coeff;
      i += run; /* skip zeroes */
      JJ_GET_BITS(size, jpg_h_bnum,jpg_h_bbuf,level);
      coeff = (xaLONG)jpg_huff_EXTEND(level, size);
DEBUG_LEVEL2 fprintf(stderr,"                   size %d coeff %x\n",size,coeff);
      pos = JJ_ZAG[i];
      coeff *= (xaLONG)qtab[ pos ];
      if (coeff)
      { c_cnt++;
        dct_buf[ pos ] = (xaSHORT)(coeff);
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
  { register xaUBYTE *op = OBuf;
    register int jj = 8;
    xaSHORT v = *dct_buf;
    register xaUBYTE dc;
    v = (v < 0)?( (v-3)>>3 ):( (v+4)>>3 );
    dc = rnglimit[ (int) (v & RANGE_MASK) ];
    while(jj--)
    { op[0] = op[1] = op[2] = op[3] = op[4] = op[5] = op[6] = op[7] = dc;
      op += 8;
    }
  }
  return(xaTRUE);
}

/* POD since these are fixed, possibly eliminate */
#define FIX_0_298631336  ((xaLONG)  2446)	/* FIX(0.298631336) */
#define FIX_0_390180644  ((xaLONG)  3196)	/* FIX(0.390180644) */
#define FIX_0_541196100  ((xaLONG)  4433)	/* FIX(0.541196100) */
#define FIX_0_765366865  ((xaLONG)  6270)	/* FIX(0.765366865) */
#define FIX_0_899976223  ((xaLONG)  7373)	/* FIX(0.899976223) */
#define FIX_1_175875602  ((xaLONG)  9633)	/* FIX(1.175875602) */
#define FIX_1_501321110  ((xaLONG)  12299)	/* FIX(1.501321110) */
#define FIX_1_847759065  ((xaLONG)  15137)	/* FIX(1.847759065) */
#define FIX_1_961570560  ((xaLONG)  16069)	/* FIX(1.961570560) */
#define FIX_2_053119869  ((xaLONG)  16819)	/* FIX(2.053119869) */
#define FIX_2_562915447  ((xaLONG)  20995)	/* FIX(2.562915447) */
#define FIX_3_072711026  ((xaLONG)  25172)	/* FIX(3.072711026) */

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

void JPG_Free_Samp_Limit_Table()
{
  if (jpg_samp_limit) { free(jpg_samp_limit); jpg_samp_limit = 0; }
}

void JPG_Setup_Samp_Limit_Table(anim_hdr)
XA_ANIM_HDR *anim_hdr;
{
  xaUBYTE *table;
  xaLONG i;

  if (jpg_samp_limit==0)
  {
     jpg_samp_limit = (xaUBYTE *)malloc((5 * (MAXJSAMPLE+1) + CENTERJSAMPLE));
     if (jpg_samp_limit==0) 
     { fprintf(stderr,"JJ: samp limit malloc err\n"); 
       return;
     } /*POD note improve this */
  }
  else return; /* already done */

  if (anim_hdr) XA_Add_Func_To_Free_Chain(anim_hdr,JPG_Free_Samp_Limit_Table);
  xa_byte_limit = jpg_samp_limit + (MAXJSAMPLE + 1);

  table = jpg_samp_limit;
  table += (MAXJSAMPLE+1);   /* allow negative subscripts of simple table */

  /* First segment of "simple" table: limit[x] = 0 for x < 0 */
  memset(table - (MAXJSAMPLE+1), 0, (MAXJSAMPLE+1));

  /* Main part of "simple" table: limit[x] = x */
  for (i = 0; i <= MAXJSAMPLE; i++) table[i] = (xaUBYTE) i;

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
xaULONG jpg_std_DHT()
{
  xaLONG ttt,len;
  JJ_HUFF_TBL *htable;
  xaUBYTE  *hbits,*Sbits;
  xaUBYTE  *hvals,*Svals;

  static xaUBYTE dc_luminance_bits[17] =
    { /* 0-base */ 0, 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0 };
  static xaUBYTE dc_luminance_vals[] =
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

  static xaUBYTE dc_chrominance_bits[17] =
    { /* 0-base */ 0, 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 };
  static xaUBYTE dc_chrominance_vals[] =
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

  static xaUBYTE ac_luminance_bits[17] =
    { /* 0-base */ 0, 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d };
  static xaUBYTE ac_luminance_vals[] =
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

  static xaUBYTE ac_chrominance_bits[17] =
    { /* 0-base */ 0, 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77 };
  static xaUBYTE ac_chrominance_vals[] =
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
  { xaULONG index = ttt & 1;
    xaLONG i,count;

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
	{ fprintf(stderr,"JJ: STD DHT bad count %d\n",count); return(xaFALSE); }

    for (i = 0; i < count; i++) hvals[i] = Svals[i];
    len -= count;

    jpg_huff_build(htable,hbits,hvals);

  } /* end of i */
  jpg_std_DHT_flag = 1;
  return(xaTRUE);
}

xaULONG jpg_std_DQT(scale)
xaLONG 	scale;
{ xaLONG i, tbl_num;
  xaLONG *quant_table;
  unsigned int *table;
  unsigned int std_luminance_quant_tbl[DCTSIZE2] = {
    16,  11,  10,  16,  24,  40,  51,  61,
    12,  12,  14,  19,  26,  58,  60,  55,
    14,  13,  16,  24,  40,  57,  69,  56,
    14,  17,  22,  29,  51,  87,  80,  62,
    18,  22,  37,  56,  68, 109, 103,  77,
    24,  35,  55,  64,  81, 104, 113,  92,
    49,  64,  78,  87, 103, 121, 120, 101,
    72,  92,  95,  98, 112, 100, 103,  99
  };
  unsigned int std_chrominance_quant_tbl[DCTSIZE2] = {
    17,  18,  24,  47,  99,  99,  99,  99,
    18,  21,  26,  66,  99,  99,  99,  99,
    24,  26,  56,  99,  99,  99,  99,  99,
    47,  66,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99
  };

  tbl_num = 0;
  for(tbl_num = 0; tbl_num <= 1; tbl_num++ )
  {
    if (jpg_quant_tables[tbl_num] == 0)
    { jpg_quant_tables[tbl_num] = (xaLONG *)malloc(64 * sizeof(xaLONG));
      if (jpg_quant_tables[tbl_num] == 0)
      { fprintf(stderr,"JJ: DQT alloc err %x \n",tbl_num);
        return(xaFALSE);
      }
    }
    if (tbl_num == 0) table = std_luminance_quant_tbl;
    else table = std_chrominance_quant_tbl;
    quant_table = jpg_quant_tables[tbl_num];
    for (i = 0; i < DCTSIZE2; i++)
    { xaLONG tmp;
      tmp = ((long) table[i] * scale + 50L) / 100L;
      if (tmp <= 0) tmp = 1;
      if (tmp > 255) tmp = 255;
/*
      quant_table[ JJ_ZAG[i] ] = (xaLONG) tmp;
*/
      quant_table[ i ] = (xaLONG) tmp;
    }
  }
  return(xaTRUE);
}
/*
jcparam.c:  static const unsigned int std_luminance_quant_tbl[DCTSIZE2] = {
jcparam.c:  static const unsigned int std_chrominance_quant_tbl[DCTSIZE2] = {
jcparam.c:  jpeg_add_quant_table(cinfo, 0, std_luminance_quant_tbl,
jcparam.c:  jpeg_add_quant_table(cinfo, 1, std_chrominance_quant_tbl,
*/

xaULONG jpg_search_marker(marker,data_ptr,data_size)
xaULONG marker;
xaUBYTE **data_ptr;
xaLONG *data_size;
{ xaULONG d = 0;
  xaUBYTE *dptr = *data_ptr;
  xaLONG dsize = *data_size;

  while( dsize )
  {
    if (d == 0xff) /* potential marker */
    {
      d = *dptr++; dsize--;
      if (d == marker) 
      {
	*data_size = dsize; *data_ptr = dptr; 	
	return(xaTRUE); /* found marker */
      }
    } else { d = *dptr++; dsize--; }
  }
  *data_size = dsize; *data_ptr = dptr; 	
  return(xaFALSE);
}

void JFIF_Read_IJPG_Tables(xin)
XA_INPUT *xin;
{ int i;
  for(i=0;i<64;i++) IJPG_Tab1[i] = xin->Read_U8(xin);
  for(i=0;i<64;i++) IJPG_Tab2[i] = xin->Read_U8(xin);
}

static char std_luminance_quant_tbl[64] = {
  16,  11,  12,  14,  12,  10,  16,  14,
  13,  14,  18,  17,  16,  19,  24,  40,
  26,  24,  22,  22,  24,  49,  35,  37,
  29,  40,  58,  51,  61,  60,  57,  51,
  56,  55,  64,  72,  92,  78,  64,  68,
  87,  69,  55,  56,  80, 109,  81,  87,
  95,  98, 103, 104, 103,  62,  77, 113,
 121, 112, 100, 120,  92, 101, 103,  99
};
 
static char std_chrominance_quant_tbl[64] = {
  17,  18,  18,  24,  21,  24,  47,  26,
  26,  47,  99,  66,  56,  66,  99,  99,
  99,  99,  99,  99,  99,  99,  99,  99,
  99,  99,  99,  99,  99,  99,  99,  99,
  99,  99,  99,  99,  99,  99,  99,  99,
  99,  99,  99,  99,  99,  99,  99,  99,
  99,  99,  99,  99,  99,  99,  99,  99,
  99,  99,  99,  99,  99,  99,  99,  99
};


void JFIF_Init_IJPG_Tables(qfactor)
int qfactor;
{ int i;
  for(i=0;i<64;i++) IJPG_Tab1[i] = qfactor * std_luminance_quant_tbl[i];
  for(i=0;i<64;i++) IJPG_Tab2[i] = qfactor * std_chrominance_quant_tbl[i];
}

#define JPG_HANDLE_RST(rst_int,rst_cnt)	{		\
if ( ((rst_int) && (rst_cnt==0))  ) 			\
{ jpg_h_bbuf = 0;  jpg_h_bnum = 0;			\
  DEBUG_LEVEL1 fprintf(stderr,"  jRST_INT %d rst_cnt %d\n", rst_int,rst_cnt); \
  if (jpg_marker == 0) jpg_marker = jpg_check_for_marker();	\
  if (jpg_marker)							\
  { 	\
    if (jpg_marker == M_EOI) { jpg_saw_EOI = xaTRUE; return(xaTRUE); }	\
    else if (jpg_marker == M_SOS)  (void)jpg_read_SOS();		\
    else if ( !((jpg_marker >= M_RST0) && (jpg_marker <= M_RST7)))	\
    {fprintf(stderr,"JPEG: unexp marker(%x)\n",jpg_marker); /*return(xaFALSE);*/} \
    jpg_marker = 0;						\
  }	\
  jpg_comps[0].dc = jpg_comps[1].dc = jpg_comps[2].dc = 0;	\
  rst_cnt = rst_int;						\
} else rst_cnt--;						}

#define JPG_TST_MARKER() \
while(jpg_marker)							\
{ DEBUG_LEVEL4 fprintf(stderr,"  jpg_marker(%x)\n",jpg_marker);		\
if (jpg_marker == M_EOI) { jpg_saw_EOI = xaTRUE; jpg_marker = 0; }	\
else if (jpg_marker == M_SOS) { (void)jpg_read_SOS(); jpg_marker = 0; }	\
else if ( (jpg_marker >= M_RST0) && (jpg_marker <= M_RST7) )		\
{ DEBUG_LEVEL1 fprintf(stderr,"JPEG: marker %x  rst_cnt %x rst_int %x\n", \
                        jpg_marker,rst_count,jpg_rst_interval);		\
  jpg_comps[0].dc = jpg_comps[1].dc = jpg_comps[2].dc = 0;		\
  rst_skip = rst_count;	rst_count = jpg_rst_interval;			\
  jpg_marker = 0; jpg_h_bbuf = 0;  jpg_h_bnum = 0;			\
}									\
else /* Unknown or unexpected Marker */					\
{ fprintf(stderr,"JPEG: unexp marker(%x)\n",jpg_marker);		\
  jpg_marker = jpg_skip_to_nxt_rst(); /* hopefully a RST marker */	\
}									\
}


/*******
 *
 */
xaULONG jpg_decode_111111(image,width,height,interleave,row_offset,
				imagex,imagey,dec_info,gray)
xaUBYTE *image;
xaULONG width,height,interleave,row_offset;
xaULONG imagex,imagey;
XA_DEC2_INFO *dec_info;
xaULONG gray;
{
  xaUBYTE *iptr = image;
  xaULONG map_flag = dec_info->map_flag;
  xaULONG *map = dec_info->map;
  XA_CHDR *chdr = dec_info->chdr;
  xaULONG x,mcu_cols,mcu_rows;
  xaLONG *qtab0,*qtab1,*qtab2;
  xaUBYTE *Ybuf,*Ubuf,*Vbuf;
  xaULONG rst_count;
  xaULONG rst_skip = 0;
  xaULONG orow_size = imagex * dec_info->bytes_pixel;
  void (*color_func)();

  if (chdr) { if (chdr->new_chdr) chdr = chdr->new_chdr; }

  if (gray) color_func = XA_MCU111111_To_Gray;
  else	    color_func = (void (*)())XA_MCU111111_Func(dec_info->image_type);

  if (row_offset) iptr += row_offset * orow_size;
  orow_size *= interleave;
  if (interleave == 2) imagey >>= 1;

  qtab0 = jpg_quant_tables[ jpg_comps[0].qtbl_num ];
  qtab1 = jpg_quant_tables[ jpg_comps[1].qtbl_num ];
  qtab2 = jpg_quant_tables[ jpg_comps[2].qtbl_num ];

  mcu_cols = (width  + 7) / 8;
  mcu_rows = (height + 7) / 8;
  if (gray)
  { DEBUG_LEVEL1 fprintf(stderr,"111111 gray MCUS %d,%d\n",mcu_cols,mcu_rows); }
  else { DEBUG_LEVEL1 fprintf(stderr,"111111 MCUS %d,%d\n",mcu_cols,mcu_rows); }
  jpg_marker = 0x00;

  rst_count = jpg_rst_interval;
  while(mcu_rows--)
  { 
    Ybuf = jpg_YUVBufs.Ybuf; Ubuf = jpg_YUVBufs.Ubuf; Vbuf = jpg_YUVBufs.Vbuf;
    x = mcu_cols;
    while(x--)
    { 
      DEBUG_LEVEL1 fprintf(stderr,"  MCU XY %d,%d\n",x,mcu_rows);

      if (rst_skip)
      {
	DEBUG_LEVEL1 fprintf(stderr,"  RST_SKIP: rst_skip %d\n",rst_skip);
	rst_skip--;
        if (Ybuf != jpg_YUVBufs.Ybuf)
	{ xaUBYTE *prev;
          prev = Ybuf - DCTSIZE2;
          memcpy(Ybuf,prev,DCTSIZE2);
	  Ybuf += DCTSIZE2;
          if (gray==0)
	  { prev = Ubuf - DCTSIZE2; memcpy(Ubuf,prev,DCTSIZE2);
	    prev = Vbuf - DCTSIZE2; memcpy(Vbuf,prev,DCTSIZE2);
	    Ubuf += DCTSIZE2;
	    Vbuf += DCTSIZE2;
          }
	}
	else
	{
	  memset(Ybuf,0,DCTSIZE2);
	  Ybuf += DCTSIZE2;
          if (gray==0)
	  { memset(Ubuf,0x80,DCTSIZE2);
	    memset(Vbuf,0x80,DCTSIZE2);
	    Ubuf += DCTSIZE2;
	    Vbuf += DCTSIZE2;
          }
        }
      }
      else
      {
        JPG_HANDLE_RST(jpg_rst_interval,rst_count);

        jpg_huffparse(&jpg_comps[0],jpg_dct_buf,qtab0,Ybuf); Ybuf += DCTSIZE2;
        if (gray==0)
        {
          jpg_huffparse(&jpg_comps[1],jpg_dct_buf,qtab1,Ubuf); Ubuf += DCTSIZE2;
          jpg_huffparse(&jpg_comps[2],jpg_dct_buf,qtab2,Vbuf); Vbuf += DCTSIZE2;
        } 
        if (jpg_marker == 0) jpg_marker = jpg_check_for_marker();
        JPG_TST_MARKER();

      } /* end of rst_skip */
    } /* end of mcu_cols */

    DEBUG_LEVEL1 fprintf(stderr,"imagey %d\n",imagey);

    (void)(color_func)(iptr,imagex,xaMIN(imagey,8),(mcu_cols * DCTSIZE2),
	orow_size, &jpg_YUVBufs,&def_yuv_tabs, map_flag,map,chdr);

    imagey -= 8;  iptr += (orow_size << 3);

  } /* end of mcu_rows */
  if (jpg_marker) { jpg_h_bbuf = 0; jpg_h_bnum = 0; }
  return(xaTRUE);
}


/*******
 *
 */
xaULONG jpg_decode_211111(image,width,height,interleave,row_offset,
				imagex,imagey,dec_info)
xaUBYTE *image;
xaULONG width,height,interleave,row_offset;
xaULONG imagex,imagey;
XA_DEC2_INFO *dec_info;
{
  xaUBYTE *iptr = image;
  xaULONG map_flag = dec_info->map_flag;
  xaULONG *map = dec_info->map;
  XA_CHDR *chdr = dec_info->chdr;
  xaULONG x,mcu_cols,mcu_rows;
  xaLONG *qtab0,*qtab1,*qtab2;
  xaUBYTE *Ybuf,*Ubuf,*Vbuf;
  xaULONG rst_count;
  xaULONG rst_skip = 0;
  xaULONG orow_size = imagex * dec_info->bytes_pixel;
  void (*color_func)() = (void (*)())XA_MCU211111_Func(dec_info->image_type);

  if (chdr) { if (chdr->new_chdr) chdr = chdr->new_chdr; }

  if (row_offset) iptr += row_offset * orow_size;
  orow_size *= interleave;
  if (interleave == 2) imagey >>= 1;
  imagex++; imagex >>= 1;

  qtab0 = jpg_quant_tables[ jpg_comps[0].qtbl_num ];
  qtab1 = jpg_quant_tables[ jpg_comps[1].qtbl_num ];
  qtab2 = jpg_quant_tables[ jpg_comps[2].qtbl_num ];

  mcu_cols = (width  + 15) / 16;
  mcu_rows = (height +  7) / 8;
  DEBUG_LEVEL1 fprintf(stderr,"211111 begin MCUS(%d,%d)\n",mcu_cols,mcu_rows);
  jpg_marker = 0x00;

  rst_count = jpg_rst_interval;
  while(mcu_rows--)
  { 
    Ybuf = jpg_YUVBufs.Ybuf; Ubuf = jpg_YUVBufs.Ubuf; Vbuf = jpg_YUVBufs.Vbuf;
    x = mcu_cols;
    while(x--)
    { /* DEBUG_LEVEL1 fprintf(stderr,"MCU XY(%d,%d)\n", x,mcu_rows); */

      if (rst_skip)
      {
  	DEBUG_LEVEL1 fprintf(stderr,"  RST_SKIP: rst_skip\n");
  	rst_skip--;
  	memset(Ybuf,0,(DCTSIZE2 << 1));
  	memset(Ubuf,0x80,DCTSIZE2);
  	memset(Vbuf,0x80,DCTSIZE2);
  	Ybuf += (DCTSIZE2 << 1);
  	Ubuf += DCTSIZE2;
  	Vbuf += DCTSIZE2;
      }
      else
      {
        JPG_HANDLE_RST(jpg_rst_interval,rst_count);

        jpg_huffparse(&jpg_comps[0],jpg_dct_buf,qtab0,Ybuf); Ybuf += DCTSIZE2;
        jpg_huffparse(&jpg_comps[0],jpg_dct_buf,qtab0,Ybuf); Ybuf += DCTSIZE2;
        jpg_huffparse(&jpg_comps[1],jpg_dct_buf,qtab1,Ubuf); Ubuf += DCTSIZE2;
        jpg_huffparse(&jpg_comps[2],jpg_dct_buf,qtab2,Vbuf); Vbuf += DCTSIZE2;

        if (jpg_marker == 0) jpg_marker = jpg_check_for_marker();
        JPG_TST_MARKER();
      }
    } /* end of mcu_cols */

    if (width <= imagex)  /* NOTE: imagex already >> 1 above */
    {
      JPG_Double_MCU(jpg_YUVBufs.Ybuf, (mcu_cols << 1) );
      JPG_Double_MCU(jpg_YUVBufs.Ubuf, mcu_cols);
      JPG_Double_MCU(jpg_YUVBufs.Vbuf, mcu_cols);
      (void)(color_func)(iptr,imagex,xaMIN(imagey,8),((mcu_cols<<1) * DCTSIZE2),
	  orow_size, &jpg_YUVBufs,&def_yuv_tabs, map_flag,map,chdr);
    }
    else
    {
      (void)(color_func)(iptr,imagex,xaMIN(imagey,8),(mcu_cols * DCTSIZE2),
	  orow_size, &jpg_YUVBufs,&def_yuv_tabs, map_flag,map,chdr);
    }
    imagey -= 8;  iptr += (orow_size << 3);

  } /* end of mcu_rows */
  if (jpg_marker) { jpg_h_bbuf = 0; jpg_h_bnum = 0; }
  return(xaTRUE);
}


static void JPG_Double_MCU(ptr,mcus)
xaUBYTE	*ptr;
int	mcus;
{ xaUBYTE *sblk, *dblk;
  int  blks = mcus * 8;
  int  flag = 0;

  sblk = ptr + (blks * 8) - 8;
  dblk = ptr + (blks * 16) - 8;
  while(blks--)
  { dblk[7] = dblk[6] = sblk[7];
    dblk[5] = dblk[4] = sblk[6];
    dblk[3] = dblk[2] = sblk[5];
    dblk[1] = dblk[0] = sblk[4];
    dblk -= 64;
    dblk[7] = dblk[6] = sblk[3];
    dblk[5] = dblk[4] = sblk[2];
    dblk[3] = dblk[2] = sblk[1];
    dblk[1] = dblk[0] = sblk[0];
    flag++;
    if (flag >= 8)
    {
      flag = 0;
      dblk -= 8;
    }
    else
    {
      dblk += 56;
    }
    sblk -= 8;
  }
}
  

/*******
 *
 */
xaULONG jpg_decode_221111(image,width,height,interleave,row_offset,
				imagex,imagey,dec_info)
xaUBYTE *image;
xaULONG width,height,interleave,row_offset;
xaLONG imagex,imagey;
XA_DEC2_INFO *dec_info;
{ xaUBYTE *iptr = image;
  xaULONG map_flag = dec_info->map_flag;
  xaULONG *map = dec_info->map;
  XA_CHDR *chdr = dec_info->chdr;
  xaLONG x,mcu_cols,mcu_rows;
  xaLONG *qtab0,*qtab1,*qtab2;
  xaUBYTE *Ybuf,*Ubuf,*Vbuf;
  xaULONG rst_count;
  xaULONG rst_skip = 0;
  void (*color_func)() = (void (*)())XA_MCU221111_Func(dec_info->image_type);
  xaULONG orow_size = imagex * dec_info->bytes_pixel;

  if (chdr) { if (chdr->new_chdr) chdr = chdr->new_chdr; }

  if (row_offset) iptr += row_offset * orow_size;
  orow_size *= interleave;
  if (interleave == 2) imagey >>= 1;
  imagex++; imagex >>= 1;  /* 2h */
  qtab0 = jpg_quant_tables[ jpg_comps[0].qtbl_num ];
  qtab1 = jpg_quant_tables[ jpg_comps[1].qtbl_num ];
  qtab2 = jpg_quant_tables[ jpg_comps[2].qtbl_num ];

  mcu_cols = (width  + 15) / 16;
  mcu_rows = (height + 15) / 16;
  DEBUG_LEVEL1 fprintf(stderr,"221111 begin MCUS(%d,%d)\n",mcu_cols,mcu_rows);
  jpg_marker = 0x00;

  rst_count = jpg_rst_interval;
  while(mcu_rows--)
  { Ybuf = jpg_YUVBufs.Ybuf; Ubuf = jpg_YUVBufs.Ubuf; Vbuf = jpg_YUVBufs.Vbuf;
    x = mcu_cols; while(x--)
    { DEBUG_LEVEL1 fprintf(stderr,"  MCU XY(%d,%d)\n",x,mcu_rows);

      if (rst_skip)
      {
  	DEBUG_LEVEL1 fprintf(stderr,"  RST_SKIP: rst_skip\n");
  	rst_skip--;
  	memset(Ybuf,0,(DCTSIZE2 << 2));
  	memset(Ubuf,0x80,DCTSIZE2);
  	memset(Vbuf,0x80,DCTSIZE2);
  	Ybuf += (DCTSIZE2 << 2);
  	Ubuf += DCTSIZE2;
  	Vbuf += DCTSIZE2;
      }
      else
      {
        JPG_HANDLE_RST(jpg_rst_interval,rst_count);

	  /* Y0 Y1 Y2 Y3 U V */
        jpg_huffparse(&jpg_comps[0],jpg_dct_buf,qtab0,Ybuf); Ybuf += DCTSIZE2;
        jpg_huffparse(&jpg_comps[0],jpg_dct_buf,qtab0,Ybuf); Ybuf += DCTSIZE2;
        jpg_huffparse(&jpg_comps[0],jpg_dct_buf,qtab0,Ybuf); Ybuf += DCTSIZE2;
        jpg_huffparse(&jpg_comps[0],jpg_dct_buf,qtab0,Ybuf); Ybuf += DCTSIZE2;
        jpg_huffparse(&jpg_comps[1],jpg_dct_buf,qtab1,Ubuf); Ubuf += DCTSIZE2;
        jpg_huffparse(&jpg_comps[2],jpg_dct_buf,qtab2,Vbuf); Vbuf += DCTSIZE2;

        if (jpg_marker == 0) jpg_marker = jpg_check_for_marker();
        JPG_TST_MARKER();
      }
    } /* end of mcu_cols */

    (void)(color_func)(iptr,imagex,xaMIN(imagey,16),(mcu_cols * DCTSIZE2),orow_size,
			&jpg_YUVBufs,&def_yuv_tabs,map_flag,map,chdr);
    imagey -= 16;  iptr += (orow_size << 4);

  } /* end of mcu_rows */
  if (jpg_marker) { jpg_h_bbuf = 0; jpg_h_bnum = 0; }
  DEBUG_LEVEL1 fprintf(stderr,"411: done\n");
  return(xaTRUE);
}



/*******
 *
 */
xaULONG jpg_decode_411111(image,width,height,interleave,row_offset,
				imagex,imagey,dec_info)
xaUBYTE *image;
xaULONG width,height,interleave,row_offset;
xaLONG imagex,imagey;
XA_DEC2_INFO *dec_info;
{ xaUBYTE *iptr = image;
  xaULONG map_flag = dec_info->map_flag;
  xaULONG *map = dec_info->map;
  XA_CHDR *chdr = dec_info->chdr;
  xaLONG x,mcu_cols,mcu_rows;
  xaLONG *qtab0,*qtab1,*qtab2;
  xaUBYTE *Ybuf,*Ubuf,*Vbuf;
  xaULONG rst_count;
  xaULONG rst_skip = 0;
  xaULONG orow_size = imagex * dec_info->bytes_pixel;
  void (*color_func)() = (void (*)())XA_MCU411111_Func(dec_info->image_type);

  if (chdr) { if (chdr->new_chdr) chdr = chdr->new_chdr; }

  if (row_offset) iptr += row_offset * orow_size;
  orow_size *= interleave;
  if (interleave == 2) imagey >>= 1;
  imagex +=3; imagex >>= 2; /* 4h */
  qtab0 = jpg_quant_tables[ jpg_comps[0].qtbl_num ];
  qtab1 = jpg_quant_tables[ jpg_comps[1].qtbl_num ];
  qtab2 = jpg_quant_tables[ jpg_comps[2].qtbl_num ];

  mcu_cols = (width  + 31) / 32;
  mcu_rows = (height + 7) / 8;
  DEBUG_LEVEL1 fprintf(stderr,"411111 begin MCUS(%d,%d)\n",mcu_cols,mcu_rows);
  jpg_marker = 0x00;

  rst_count = jpg_rst_interval;
  while(mcu_rows--)
  { Ybuf = jpg_YUVBufs.Ybuf; Ubuf = jpg_YUVBufs.Ubuf; Vbuf = jpg_YUVBufs.Vbuf;
    x = mcu_cols; while(x--)
    { DEBUG_LEVEL1 fprintf(stderr,"  MCU XY(%d,%d)\n",x,mcu_rows);


      if (rst_skip)
      {
  	DEBUG_LEVEL1 fprintf(stderr,"  RST_SKIP: rst_skip\n");
  	rst_skip--;
  	memset(Ybuf,0,(DCTSIZE2 << 2));
  	memset(Ubuf,0x80,DCTSIZE2);
  	memset(Vbuf,0x80,DCTSIZE2);
  	Ybuf += (DCTSIZE2 << 2);
  	Ubuf += DCTSIZE2;
  	Vbuf += DCTSIZE2;
      }
      else
      {
        JPG_HANDLE_RST(jpg_rst_interval,rst_count);

	  /* Y0 Y1 Y2 Y3 U V */
        jpg_huffparse(&jpg_comps[0],jpg_dct_buf,qtab0,Ybuf); Ybuf += DCTSIZE2;
        jpg_huffparse(&jpg_comps[0],jpg_dct_buf,qtab0,Ybuf); Ybuf += DCTSIZE2;
        jpg_huffparse(&jpg_comps[0],jpg_dct_buf,qtab0,Ybuf); Ybuf += DCTSIZE2;
        jpg_huffparse(&jpg_comps[0],jpg_dct_buf,qtab0,Ybuf); Ybuf += DCTSIZE2;
        jpg_huffparse(&jpg_comps[1],jpg_dct_buf,qtab1,Ubuf); Ubuf += DCTSIZE2;
        jpg_huffparse(&jpg_comps[2],jpg_dct_buf,qtab2,Vbuf); Vbuf += DCTSIZE2;

        if (jpg_marker == 0) jpg_marker = jpg_check_for_marker();
        JPG_TST_MARKER();
      }
    } /* end of mcu_cols */

    (void)(color_func)(iptr,imagex,xaMIN(imagey,8),(mcu_cols * DCTSIZE2),orow_size,
			&jpg_YUVBufs,&def_yuv_tabs,map_flag,map,chdr);
    imagey -= 8;  iptr += (orow_size << 3);

  } /* end of mcu_rows */
  if (jpg_marker) { jpg_h_bbuf = 0; jpg_h_bnum = 0; }
  DEBUG_LEVEL1 fprintf(stderr,"411: done\n");
  return(xaTRUE);
}


