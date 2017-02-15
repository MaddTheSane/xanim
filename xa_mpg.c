
/*
 * xa_mpg.c
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

/* The following copyright notice applies to some portions of the
 * following code.
 */
/*
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose, without fee, and without written agreement is
 * hereby granted, provided that the above copyright notice and the following
 * two paragraphs appear in all copies of this software.
 *
 * IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT
 * OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF
 * CALIFORNIA HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 */

/* Revision History
 *
 * 03apr95 - fixed bug with New quant scale. Wasn't flushing bits.
 *		(typically caused HUFF ERRs)
 * 03apr95 - fixed bug with height > width.(was showing bottom part of screen)
 * 03apr95 - fixed incorrect warning message(DCT-A and DCT-B errs).
 * 29jun95 - modified timing to account for skipped P and B frames.
 * 22Aug95 - +F dithering to MPEG.
 * 29Jan99 - Added MPG_Decode_FULLI (no slice parsing stuff)
 *
 */

#include "xanim.h"
#include "xa_mpg.h"
#include "xa_xmpg.h"
#include "xa_color.h"
#include "xa_cmap.h"
#include "xa_formats.h"

extern YUVBufs jpg_YUVBufs;
extern YUVTabs def_yuv_tabs;

/* internal FUNCTIONS */
xaULONG mpg_get_start_code();
static xaULONG mpg_read_SEQ_HDR();
static xaULONG mpg_read_GOP_HDR();
static xaULONG mpg_read_PIC_HDR();
static xaULONG MPG_Setup_Delta();
static xaULONG MPG_Decode_I();
#ifdef NOTYET
xaULONG MPG_Decode_P();
xaULONG MPG_Decode_B();
#endif
static void mpg_init_motion_vectors();
static void mpg_init_mb_type_B();
static void mpg_init_mb_type_P();
static void mpg_init_mb_addr_inc();
static void MPG_Free_Stuff();
static void  decodeDCTDCSizeLum();
static void  decodeDCTDCSizeChrom();
static void decodeDCTCoeff();
static void  decodeDCTCoeffFirst();
static void  decodeDCTCoeffNext();
static void mpg_huffparseI();
static void mpg_skip_extra_bitsB();
static void j_orig_rev_dct();
void j_rev_dct();

extern xaLONG xa_dither_flag;

/* external FUNCTIONS */
extern void XA_Gen_YUV_Tabs();
extern void XA_MCU221111_To_RGB();
extern void XA_MCU221111_To_CLR8();
extern void XA_MCU221111_To_CLR16();
extern void XA_MCU221111_To_CLR32();
extern void XA_MCU221111_To_CF4();
extern void XA_MCU221111_To_332_Dither();
extern void XA_MCU221111_To_332();

extern XA_ACTION *ACT_Get_Action();
extern XA_ANIM_SETUP *XA_Get_Anim_Setup();
extern void XA_Add_Func_To_Free_Chain();
extern void XA_Free_Anim_Setup();
extern xaUBYTE *UTIL_RGB_To_Map();
extern xaUBYTE *UTIL_RGB_To_FS_Map();
extern void UTIL_FPS_2_Time();
extern void ACT_Setup_Mapped();
extern void ACT_Add_CHDR_To_Action();
extern void JPG_Alloc_MCU_Bufs();
extern void JPG_Setup_Samp_Limit_Table();
extern xaULONG UTIL_Get_MSB_Long();
extern void ACT_Setup_Delta();

/* external Buffers */
extern xaUBYTE *xa_byte_limit;
extern int xa_vid_fd;

#define MPG_RDCT  j_rev_dct
/* DON'T USE - NO xaLONGER THE SAME AS j_rev_dct
#define MPG_RDCT j_orig_rev_dct
*/

#define MPG_FROM_BUFF	0
#define MPG_FROM_FILE	1

xaSHORT mpg_dct_buf[64];

xaUBYTE  *mpg_buff = 0;
xaLONG   mpg_bsize = 0;

/* BUFFER reading global vars */
xaLONG   mpg_b_bnum;
xaULONG  mpg_b_bbuf;
XA_INPUT *mpg_f_file;

/* FILE reading global vars */
xaLONG   mpg_f_bnum;  /* this must be signed */
xaULONG  mpg_f_bbuf;

xaULONG mpg_init_flag = xaFALSE;

xaULONG mpg_start_code,mpg_count;

/*
static xaUBYTE mpg_def_intra_qtab[64] = {
	8,  16, 19, 22, 26, 27, 29, 34,
	16, 16, 22, 24, 27, 29, 34, 37,
	19, 22, 26, 27, 29, 34, 34, 38,
	22, 22, 26, 27, 29, 34, 37, 40,
	22, 26, 27, 29, 32, 35, 40, 48,
	26, 27, 29, 32, 35, 40, 48, 58,
	26, 27, 29, 34, 38, 46, 56, 69,
	27, 29, 35, 38, 46, 56, 69, 83};
*/

/* my shifted version */
static xaUBYTE mpg_def_intra_qtab[64] = {
	 8,16,19,22,22,26,26,27,
	16,16,22,22,26,27,27,29,
	19,22,26,26,27,29,29,35,
	22,24,27,27,29,32,34,38,
	26,27,29,29,32,35,38,46,
	27,29,34,34,35,40,46,56,
	29,34,34,37,40,48,56,69,
	34,37,38,40,48,58,69,83 };



MPG_SEQ_HDR *mpg_seq_hdr;

MPG_PIC_HDR *mpg_pic_hdr,*mpg_Gpic_hdr;
MPG_PIC_HDR *mpg_pic_start,*mpg_pic_cur;

/* used in xa_avi.c */
xaUBYTE *mpg_dlta_buf = 0;
xaULONG mpg_dlta_size = 0;


typedef struct MPG_FRAME_STRUCT
{ xaULONG time;
  xaULONG timelo;
  XA_ACTION *act;
  struct MPG_FRAME_STRUCT *next;
} MPG_FRAME;

MPG_FRAME *MPG_Add_Frame();
void MPG_Free_Frame_List();
MPG_FRAME *mpg_frame_start,*mpg_frame_cur;
xaULONG mpg_frame_cnt;
xaULONG mpg_Icnt,mpg_Pcnt,mpg_Bcnt;

/*******
 *
 */
#define MPG_INIT_FBUF(xin) \
  { mpg_f_file = xin; mpg_f_bnum = 0; mpg_f_bbuf = 0; }

/*******
 *
 */
#define MPG_INIT_BBUF(buff,bsize) \
{ mpg_buff = buff; mpg_bsize = bsize; \
  mpg_b_bnum = 0; mpg_b_bbuf = 0; \
}

/*******
 *
 */
#define MPG_BIT_MASK(s) ((1 << (s)) - 1)

/*******
 *
 */
#define MPG_GET_BBITS(num,result) \
{ while(mpg_b_bnum < (num)) { mpg_b_bnum += 8; mpg_bsize--; \
		mpg_b_bbuf = (*mpg_buff++) | (mpg_b_bbuf << 8); } \
  mpg_b_bnum -= (num); \
  (result) = ((mpg_b_bbuf >> mpg_b_bnum) & MPG_BIT_MASK(num)); \
}

/* ROUGH CHECK */
#define MPG_CHECK_BBITS(num,result) \
{ result = (mpg_bsize > (num))?(xaTRUE):(xaFALSE); }

#define MPG_NXT_BBITS(num,result) \
{ while(mpg_b_bnum < (num)) { mpg_b_bnum += 8; mpg_bsize--; \
		mpg_b_bbuf = (*mpg_buff++) | (mpg_b_bbuf << 8); } \
  result = (mpg_b_bbuf >> (mpg_b_bnum - num)) & MPG_BIT_MASK(num); \
}

#define MPG_FLUSH_BBITS(num) \
{ while(mpg_b_bnum < (num)) { mpg_b_bnum += 8; mpg_bsize--; \
		mpg_b_bbuf = (*mpg_buff++) | (mpg_b_bbuf << 8); } \
  mpg_b_bnum -= (num); \
}


/*******
 *
 */
#define MPG_GET_FBITS(num,result) \
{ while(mpg_f_bnum < (num)) { mpg_f_bnum += 8; mpg_count++; \
	mpg_f_bbuf = (mpg_f_file->Read_U8(mpg_f_file)) | (mpg_f_bbuf << 8); } \
  mpg_f_bnum -= (num); \
  result = (mpg_f_bbuf >> mpg_f_bnum) & MPG_BIT_MASK(num); \
}

#define MPG_ALIGN_FBUF() { mpg_f_bnum -= (mpg_f_bnum % 8); }
#define MPG_ALIGN_BBUF() { mpg_b_bnum -= (mpg_b_bnum % 8); }

void MPG_Time_Adjust(mpg,ret_time,ret_timelo,skipped_frames)
XA_ANIM_SETUP *mpg;
xaULONG *ret_time;
xaULONG *ret_timelo;
xaULONG skipped_frames;
{ xaULONG t_time   = mpg->vid_time;
  xaULONG t_timelo = mpg->vid_timelo;
  if (skipped_frames)
  {
    while(skipped_frames--)
    {
      t_time   += mpg->vid_time;
      t_timelo += mpg->vid_timelo;
      while(t_timelo > (1<<24)) {t_time++; t_timelo -= (1<<24);}
    }
  }
  *ret_time   = t_time;
  *ret_timelo = t_timelo;
}

/*******
 *
 */
MPG_FRAME *MPG_Add_Frame(time,timelo,act)
xaULONG time,timelo;
XA_ACTION *act;
{ MPG_FRAME *fframe;

  fframe = (MPG_FRAME *) malloc(sizeof(MPG_FRAME));
  if (fframe == 0) TheEnd1("MPG_Add_Frame: malloc err");
  fframe->time   = time;
  fframe->timelo = timelo;
  fframe->act = act;
  fframe->next = 0;

  if (mpg_frame_start == 0) mpg_frame_start = fframe;
  else mpg_frame_cur->next = fframe;

  mpg_frame_cur = fframe;
  mpg_frame_cnt++;
  return(fframe);
}

/*******
 *
 */
void MPG_Free_Frame_List(fframes)
MPG_FRAME *fframes;
{ MPG_FRAME *ftmp;
  while(fframes != 0)
  { ftmp = fframes;
    fframes = fframes->next;
    FREE(ftmp,0xA000);
  }
}


xaULONG MPG_Read_File(const char *fname,XA_ANIM_HDR *anim_hdr)
{ XA_INPUT *xin = anim_hdr->xin;
  xaULONG i,t_timelo;
  xaULONG t_time;
  xaULONG skipped_frames = 0;
  XA_ANIM_SETUP *mpg;

  mpg = XA_Get_Anim_Setup();
  mpg->vid_time = XA_GET_TIME( 200 ); /* default */
  mpg->cmap_frame_num = 20;  /* ?? how many frames in file?? */

  mpg_frame_cnt = 0;
  mpg_frame_cur = mpg_frame_start = 0;
  mpg_Icnt = mpg_Pcnt = mpg_Bcnt = 0;
  mpg_Gpic_hdr = mpg_pic_hdr = 0;
  mpg_pic_start = mpg_pic_cur = 0;

  mpg_seq_hdr = 0;

  /* Initialize tables and setup for Dying Time */
  MPG_Init_Stuff(anim_hdr);

   /* find file size */
  MPG_INIT_FBUF(xin); /* init file bit buffer */
  if (mpg_get_start_code() == xaFALSE) return(xaFALSE);
  while( !mpg_f_file->At_EOF(mpg_f_file,-1) )
  { xaULONG ret,need_nxt_flag = xaTRUE;

    DEBUG_LEVEL1 fprintf(stderr,"MPG CODE %x cnt %x\n",
						mpg_start_code,mpg_count);
    if (mpg_start_code == MPG_SEQ_START)
    { XA_ACTION *act = 0;
      act = ACT_Get_Action(anim_hdr,ACT_NOP);
      mpg_seq_hdr = (MPG_SEQ_HDR *)malloc(sizeof(MPG_SEQ_HDR));
      act->data = (xaUBYTE *)mpg_seq_hdr;
      mpg_read_SEQ_HDR(anim_hdr,mpg,mpg_seq_hdr);
    }
    else if (mpg_start_code == MPG_PIC_START)
    {
      if (mpg_Gpic_hdr==0)
      { XA_ACTION *act = 0;
        act = ACT_Get_Action(anim_hdr,ACT_NOP);
        mpg_Gpic_hdr = (MPG_PIC_HDR *)malloc(sizeof(MPG_PIC_HDR));
        act->data = (xaUBYTE *)mpg_Gpic_hdr;
        mpg_Gpic_hdr->slice_1st = mpg_Gpic_hdr->slice_last = 0;
	mpg_Gpic_hdr->slice_cnt = 0;
	mpg_Gpic_hdr->slices[0].fsize = 0;
	mpg_Gpic_hdr->slices[0].act   = 0;
      }
      mpg_read_PIC_HDR(mpg_Gpic_hdr,MPG_FROM_FILE);
      mpg_Gpic_hdr->seq_hdr = mpg_seq_hdr;

      mpg->compression = mpg_Gpic_hdr->type;
      mpg_pic_hdr = 0;
      switch(mpg->compression)
      {
	case MPG_TYPE_I: 
	  { XA_ACTION *act = 0;
	    act = ACT_Get_Action(anim_hdr,ACT_NOP);
	    mpg_pic_hdr = (MPG_PIC_HDR *)malloc(sizeof(MPG_PIC_HDR));
	    if (mpg_pic_hdr==0) TheEnd1("MPG: pic_hdr malloc err");
	    memcpy((char *)mpg_pic_hdr,(char *)mpg_Gpic_hdr,
						sizeof(MPG_PIC_HDR));
	    act->data = (xaUBYTE *)mpg_pic_hdr;
	    mpg_pic_hdr->slice_1st = mpg_pic_hdr->slice_last = 0;
	    mpg_pic_hdr->slice_cnt = 0;
	    mpg_pic_hdr->slices[0].fsize = 0;
	    mpg_pic_hdr->slices[0].act   = 0;
	    MPG_Time_Adjust(mpg,&t_time,&t_timelo,skipped_frames);
	    skipped_frames = 0;
            MPG_Add_Frame(t_time,t_timelo, act);

	  }
	  mpg_Icnt++; 
	  break;
	case MPG_TYPE_P: mpg_Pcnt++; skipped_frames++; break;
	case MPG_TYPE_B: mpg_Bcnt++; skipped_frames++; break;
	default: 
	  fprintf(stderr,"MPG Unk type %x\n",mpg->compression);
	  break;
      }
    }
	/* For now skip system and most packet start codes */
    else if ( (mpg_start_code >= 0xb9)
		&& (mpg_start_code <= 0xdf) )
    { 
      DEBUG_LEVEL1 fprintf(stderr,
		"MPG: unsupported code %02x - ignored\n", mpg_start_code);
    }
    else if ( (mpg_start_code >= 0xe1)
		&& (mpg_start_code <= 0xff) )
    { 
      DEBUG_LEVEL1 fprintf(stderr,
		"MPG: unsupported code %02x - ignored\n", mpg_start_code);
    }
    else if (mpg_start_code == MPG_GOP_START)	mpg_read_GOP_HDR(MPG_FROM_FILE);
    else if (mpg_start_code == MPG_SEQ_END)	break; /* END??? */
    else if (mpg_start_code == MPG_USR_START)	
	{ DEBUG_LEVEL1 fprintf(stderr,"USR START:\n"); }
    else if (mpg_start_code == MPG_EXT_START)	
	{ DEBUG_LEVEL1 fprintf(stderr,"EXT START:\n"); }
    else if (   (mpg_start_code >= MPG_MIN_SLICE)
	     && (mpg_start_code <= MPG_MAX_SLICE) )
    { xaULONG except_flag = 0;
      DEBUG_LEVEL1 fprintf(stderr,"SLICE %x\n",mpg_start_code);
      if (mpg_seq_hdr == 0)
	   { fprintf(stderr,"Slice without Sequence Header Err\n"); break; }
      if (   (mpg->compression == MPG_TYPE_I)
	  && (mpg_pic_hdr == 0) )  /* assume old Xing and create one */
      { XA_ACTION *tmp_act;
	fprintf(stderr,"slice without pic hdr err\n");
        except_flag = 1;
	tmp_act = ACT_Get_Action(anim_hdr,ACT_NOP);
	mpg_pic_hdr = (MPG_PIC_HDR *)malloc(sizeof(MPG_PIC_HDR));
	if (mpg_pic_hdr==0) TheEnd1("MPG: pic_hdr malloc err");
	mpg_pic_hdr->seq_hdr = mpg_seq_hdr;
	mpg->compression = mpg_pic_hdr->type = MPG_TYPE_I;
	tmp_act->data = (xaUBYTE *)mpg_pic_hdr;
	mpg_pic_hdr->slice_1st = mpg_pic_hdr->slice_last = 0;
	mpg_pic_hdr->slice_cnt = 0;
	mpg_pic_hdr->slices[0].fsize = 0;
	mpg_pic_hdr->slices[0].act   = 0;
	MPG_Time_Adjust(mpg,&t_time,&t_timelo,skipped_frames);
	skipped_frames = 0;
	MPG_Add_Frame(t_time,t_timelo, tmp_act);
      }
      if ( (mpg->compression == MPG_TYPE_I) && (mpg_pic_hdr))
      { MPG_SLICE_HDR *slice_hdr;
        XA_ACTION *act = 0;

	/* alloc SLICE_HDR and ADD to current PIC_HDR */
	act = ACT_Get_Action(anim_hdr,ACT_NOP);
	slice_hdr = (MPG_SLICE_HDR *)malloc(sizeof(MPG_SLICE_HDR));
	act->data = (xaUBYTE *)slice_hdr;
	slice_hdr->next = 0;
	slice_hdr->act  = act;
	if (mpg_pic_hdr->slice_1st) mpg_pic_hdr->slice_last->next = slice_hdr;
	else			    mpg_pic_hdr->slice_1st = slice_hdr;
        mpg_pic_hdr->slice_last = slice_hdr;
	mpg_pic_hdr->slice_cnt++;

	slice_hdr->vert_pos = mpg_start_code & 0xff;
	slice_hdr->fpos = mpg_f_file->Get_FPos(mpg_f_file);
	mpg_count = 0;
	ret = mpg_get_start_code(); need_nxt_flag = xaFALSE;
	slice_hdr->fsize = mpg_count; /* get slice size */
	if (mpg_count > mpg->max_fvid_size) mpg->max_fvid_size = mpg_count;
      }
      if (except_flag) mpg_pic_hdr = 0; /* one slice only per pic_hdr*/
    }
#ifdef NOTHING
    else if (mpg_start_code == MPG_UNK_E0)  /* Slice Continuation */
    { xaULONG sys_clk_ref, mux_rate;
      { MPG_SLICE_HDR *slice_hdr;
        XA_ACTION *act = 0;

        /* alloc SLICE_HDR and ADD to current PIC_HDR */
        act = ACT_Get_Action(anim_hdr,ACT_NOP);
        slice_hdr = (MPG_SLICE_HDR *)malloc(sizeof(MPG_SLICE_HDR));
        act->data = (xaUBYTE *)slice_hdr;
        slice_hdr->next = 0;
        slice_hdr->act  = act;
        if (mpg_pic_hdr->slice_1st) mpg_pic_hdr->slice_last->next = slice_hdr;
        else                        mpg_pic_hdr->slice_1st = slice_hdr;
        mpg_pic_hdr->slice_last = slice_hdr;
        mpg_pic_hdr->slice_cnt++;

        slice_hdr->vert_pos = mpg_start_code & 0xff;
        slice_hdr->fpos = mpg_f_file->Get_FPos(mpg_f_file);
        mpg_count = 0;
        ret = mpg_get_start_code(); need_nxt_flag = xaFALSE;
        slice_hdr->fsize = mpg_count; /* get slice size */
        if (mpg_count > mpg->max_fvid_size) mpg->max_fvid_size = mpg_count;
      }
    }
#endif
    else
    {
      fprintf(stderr,"MPG_UNK CODE: %02x\n",mpg_start_code);
    }

    /* get next if necessary */
    if (need_nxt_flag == xaTRUE) ret = mpg_get_start_code(); 
    if (ret == xaFALSE) break;
  } /* end of while forever */

  if (mpg_frame_cnt == 0)
  { fprintf(stderr,"MPG: No supported video frames exist in this file.\n");
    return(xaFALSE);
  }
  if (xa_verbose)
	fprintf(stdout,"MPG %dx%d frames %d  I's %d  P's %d  B's %d\n",
	  mpg->imagex,mpg->imagey,mpg_frame_cnt,mpg_Icnt,mpg_Pcnt,mpg_Bcnt);

  mpg_pic_hdr = 0; /* force err if used */

  /* Adjust Temporary Data Structures into Final Organization */
  mpg_frame_cur = mpg_frame_start;
  while(mpg_frame_cur != 0)
  { XA_ACTION *phdr_act = mpg_frame_cur->act;
    MPG_PIC_HDR *phdr = (MPG_PIC_HDR *)(phdr_act->data);
    xaULONG slice_cnt = phdr->slice_cnt;

    if (slice_cnt == 0) 
    {
      fprintf(stderr,"MPG scnt 0 err - deal mit later\n");
    }
    else
    { MPG_PIC_HDR  *new_phdr;
      ACT_DLTA_HDR *dlta_hdr;
      MPG_SLICE_HDR *shdr;
      xaULONG s_cnt;

      DEBUG_LEVEL1 fprintf(stderr,"MPG: Rearranging Structures\n");

       /* alloc new pic_hdr with slice array at end */
      s_cnt = (sizeof(ACT_DLTA_HDR)) + (sizeof(MPG_PIC_HDR));
      s_cnt += ( (slice_cnt + 1) * (sizeof(MPG_SLICE_HDR)) );
      dlta_hdr = (ACT_DLTA_HDR *)malloc( s_cnt );

      if (dlta_hdr == 0) TheEnd1("MPG dlta malloc err");
      phdr_act->data = (xaUBYTE *)dlta_hdr; /* old saved in phdr */

      dlta_hdr->flags = DLTA_DATA;
      dlta_hdr->fpos = dlta_hdr->fsize = 0;

      new_phdr = (MPG_PIC_HDR *)(dlta_hdr->data);
        /* copy old phdr to new phdr */
      memcpy((char *)new_phdr,(char *)phdr,sizeof(MPG_PIC_HDR));

      s_cnt = 0;
      shdr = phdr->slice_1st;
      while(shdr)
      { XA_ACTION *s_act = shdr->act;
	xaULONG fpos,fsize;
        new_phdr->slices[s_cnt].vert_pos	= shdr->vert_pos;
        new_phdr->slices[s_cnt].fpos =   fpos  	= shdr->fpos;
        new_phdr->slices[s_cnt].fsize =  fsize	= shdr->fsize;
        new_phdr->slices[s_cnt].act		= shdr->act;
        shdr = shdr->next; /* move on */
	s_cnt++;

        free(s_act->data); s_act->data = 0;
        if ( (xa_buffer_flag == xaFALSE) && (xa_file_flag == xaFALSE) )
        {  /* need to read in the data */
          xaUBYTE *tmp = 0;
          mpg_f_file->Seek_FPos(mpg_f_file,fpos,0);
          tmp = (xaUBYTE *)malloc( fsize );
          if (tmp==0) TheEnd1("MPG: alloc err 3");
          mpg_f_file->Read_Block(mpg_f_file,(char *)tmp,fsize);
          s_act->data = tmp;
        }
      }
      new_phdr->slices[s_cnt].fpos  = 0;
      new_phdr->slices[s_cnt].fsize = 0;
      new_phdr->slices[s_cnt].act   = 0;
      free(phdr); phdr = 0; /* not phdr_act->data already reassigned */
      phdr_act->type = ACT_DELTA;
      MPG_Setup_Delta(mpg,fname,phdr_act);
    }
    mpg_frame_cur = mpg_frame_cur->next; /* move on */
  }
  xin->Close_File(xin);
  if (mpg->pic) { free(mpg->pic); mpg->pic = 0; }

  anim_hdr->frame_lst = (XA_FRAME *)
                                malloc( sizeof(XA_FRAME) * (mpg_frame_cnt+1));
  if (anim_hdr->frame_lst == NULL) TheEnd1("MPG_Read_File: frame malloc err");


  mpg_frame_cur = mpg_frame_start;
  i = 0;
  t_time = 0;
  t_timelo = 0;
  while(mpg_frame_cur != 0)
  { if (i > mpg_frame_cnt)
    { fprintf(stderr,"MPG_Read_Anim: frame inconsistency %d %d\n",
                i,mpg_frame_cnt);
      break;
    }
    anim_hdr->frame_lst[i].time_dur = mpg_frame_cur->time;
    anim_hdr->frame_lst[i].zztime = t_time;
    t_time += mpg_frame_cur->time;
    t_timelo += mpg_frame_cur->timelo;
    while(t_timelo > (1<<24)) {t_time++; t_timelo -= (1<<24);}
    anim_hdr->frame_lst[i].act = mpg_frame_cur->act;
    mpg_frame_cur = mpg_frame_cur->next;
    i++;
  }
  anim_hdr->imagex = mpg->imagex;
  anim_hdr->imagey = mpg->imagey;
  anim_hdr->imagec = mpg->imagec;
  anim_hdr->imaged = 8; /* nop */
  anim_hdr->frame_lst[i].time_dur = 0;
  anim_hdr->frame_lst[i].zztime = -1;
  anim_hdr->frame_lst[i].act  = 0;
  anim_hdr->loop_frame = 0;
  if (xa_buffer_flag == xaFALSE) anim_hdr->anim_flags |= ANIM_SNG_BUF;
  if (xa_file_flag == xaTRUE) anim_hdr->anim_flags |= ANIM_USE_FILE;
  anim_hdr->anim_flags |= ANIM_FULL_IM;
  anim_hdr->max_fvid_size = 0; /* all buffering internal */
  anim_hdr->max_faud_size = 0; /* mpg_max_faud_size; */
  anim_hdr->fname = anim_hdr->name;
  if (i > 0)
  {
    anim_hdr->last_frame = i - 1;
    anim_hdr->total_time = anim_hdr->frame_lst[i-1].zztime
                                + anim_hdr->frame_lst[i-1].time_dur;
  }
  else
  {
    anim_hdr->last_frame = 0;
    anim_hdr->total_time = 0;
  }
  MPG_Free_Frame_List(mpg_frame_start);
  XA_Free_Anim_Setup(mpg);
  return(xaTRUE);
}

/******************
 *  Look for 0x00 0x00 0x01 MPEG Start Code
 *****/
xaULONG mpg_get_start_code()
{ xaLONG d; xaULONG state = 0;
  MPG_ALIGN_FBUF(); /* make mpg_f_bnum multiple of 8 */
  while( !mpg_f_file->At_EOF(mpg_f_file) )
  { 

/*
    MPG_GET_FBITS(8,d);
*/
    if (mpg_f_bnum > 0)
    { 
fprintf(stderr,"FBITS: %d %x\n",mpg_f_bnum,mpg_f_bbuf);

       mpg_f_bnum -= 8; d = (mpg_f_bbuf >> mpg_f_bnum) & 0xff;
    }
    else d =  mpg_f_file->Read_U8(mpg_f_file);
    if (d >= 0) mpg_count++;

/* DEBUG_LEVEL1 fprintf(stderr,"GET_CODE: st(%d) d=%x\n",state,d); */
    if (state == 3) 
    { 
      mpg_start_code = d;
      mpg_f_bnum = 0; mpg_f_bbuf = 0;
      return(xaTRUE);
    }
    else if (state == 2)
    { if (d==0x01) state = 3;
      else if (d==0x00) state = 2; 
      else state = 0;
    }
    else
    { if (d==0x00) state++; 
      else state = 0; 
    }
  }
  return(xaFALSE);
}

/*****
 *
 */
void mpg_skip_extra_bitsB()
{ xaULONG i,f;
  DEBUG_LEVEL1 fprintf(stderr,"MPG Extra:\n");
  i = 0;
  f = 1; 
  while(f)
  { xaULONG d;
    MPG_GET_BBITS(1,f);
    if (f) 
    { MPG_GET_BBITS(8,d);
      DEBUG_LEVEL1
      { fprintf(stderr,"%02x ",d);
	i++; if (i >=8) {i=0; fprintf(stderr,"\n"); }
      }
    }
  }
  DEBUG_LEVEL1 if (i) fprintf(stderr,"\n");
}


/*******
 *  12 bits width
 *  12 bits height
 *   4 bits aspect ratio
 *   4 bits picture rate
 *
 *  18 bits bit rate
 *   1 bit  marker bit
 *  10 bits vbv buffer size
 *   1 bit  constrained param flag(1 = xaTRUE, 0 = xaFALSE)
 *   1 bit  intra quant flag(1 = xaTRUE, 0 = xaFALSE)
 *      if (1) get 64 * 8 bits for intra quant table
 *   1 bit  non-intra quant flag(1 = xaTRUE, 0 = xaFALSE)
 *      if (1) get 64 * 8 bits for non-intra quant table
 */

/*Xing Added 9; */
/* Is 9 12 fps or 15 fps ?? */
xaULONG mpg_pic_rate_idx[] = {30,24,24,25,30,30,50,60,60,12,30,30,30,30,30,30};

xaULONG mpg_const_param,mpg_intra_q_flag,mpg_nonintra_q_flag;

xaULONG mpg_read_SEQ_HDR(anim_hdr,mpg,shdr)
XA_ANIM_HDR *anim_hdr;
XA_ANIM_SETUP *mpg;
MPG_SEQ_HDR *shdr;
{ xaULONG i,d,marker;
  d = 0;
DEBUG_LEVEL1 fprintf(stderr,"MPG_SEQ_START: \n");
  MPG_GET_FBITS(12,shdr->width);
  MPG_GET_FBITS(12,shdr->height);
  MPG_GET_FBITS(4,shdr->aspect);
  MPG_GET_FBITS(4,shdr->pic_rate);
  MPG_GET_FBITS(18,shdr->bit_rate);
  MPG_GET_FBITS(1,marker);
  MPG_GET_FBITS(10,shdr->vbv_buff_size);
  MPG_GET_FBITS(1,shdr->constrained);
  MPG_GET_FBITS(1,shdr->intra_flag);

/*POD TEMP */
if (xa_verbose) fprintf(stderr,"MPEG Pic rate: %x\n",shdr->pic_rate);

  UTIL_FPS_2_Time(mpg, ((double)(mpg_pic_rate_idx[shdr->pic_rate])) );

  if (shdr->intra_flag)
  { for(i=0; i < 64; i++)  
    { MPG_GET_FBITS(8,d); shdr->intra_qtab[i] = d;  /* ZIG */
    }
  }
  else
  { for(i=0; i < 64;i++)  
    { shdr->intra_qtab[i] = mpg_def_intra_qtab[i];  /* NO-ZIG */
    }
  }
  MPG_GET_FBITS(1,shdr->non_intra_flag);
  if (mpg_nonintra_q_flag)
  { for(i=0; i < 64; i++)  
    { MPG_GET_FBITS(8,d); shdr->non_intra_qtab[i] = d; 
    } 
  }
/*POD TEMP until real fix */
  mpg->imagex = shdr->width;
  if (mpg->imagex & 1) { mpg->imagex++; shdr->width++; }
  mpg->imagey = shdr->height;
  if (mpg->imagey & 1) { mpg->imagey++; shdr->height++; }

  JPG_Alloc_MCU_Bufs(anim_hdr,mpg->imagex,mpg->imagey,xaTRUE);
  mpg->depth = 24; 
  XA_Gen_YUV_Tabs(anim_hdr);
  JPG_Setup_Samp_Limit_Table(anim_hdr);

  if (   (cmap_true_map_flag == xaFALSE) /* depth 16 and not true_map */
      || (xa_buffer_flag == xaFALSE) )
  {
     if (cmap_true_to_332 == xaTRUE)
             mpg->chdr = CMAP_Create_332(mpg->cmap,&mpg->imagec);
     else    mpg->chdr = CMAP_Create_Gray(mpg->cmap,&mpg->imagec);
  }
  if ( (mpg->pic==0) && (xa_buffer_flag == xaTRUE))
  {
    mpg->pic_size = mpg->imagex * mpg->imagey;
    if ( (cmap_true_map_flag == xaTRUE) && (mpg->depth > 8) )
                mpg->pic = (xaUBYTE *) malloc( 3 * mpg->pic_size );
    else mpg->pic = (xaUBYTE *) malloc( XA_PIC_SIZE(mpg->pic_size) );
    if (mpg->pic == 0) TheEnd1("MPG_Buffer_Action: malloc failed");
  }
  return(xaTRUE);
}


xaULONG mpg_num_pics;

/*******
 * 1 drop_frame_flag
 * 5 hour
 * 6 minutes
 * 1 marker
 * 6 seconds
 * 6 num_pics
 * 1 closed gop flag
 * 1 broken link flag
 *
 */
xaULONG mpg_read_GOP_HDR(bflag)
xaULONG bflag;
{ xaULONG drop_flag,hours,minutes,marker,seconds;
  xaULONG mpg_num_pics,closed_gop,broken_link;

DEBUG_LEVEL1 fprintf(stderr,"MPG_GOP_START: \n");
  if (bflag == MPG_FROM_BUFF)
  {
    MPG_GET_BBITS(1,drop_flag);
    MPG_GET_BBITS(5,hours);
    MPG_GET_BBITS(6,minutes);
    MPG_GET_BBITS(1,marker);
    MPG_GET_BBITS(6,seconds);
    MPG_GET_BBITS(6,mpg_num_pics);
    MPG_GET_BBITS(1,closed_gop);
    MPG_GET_BBITS(1,broken_link);
  }
  else
  {
    MPG_GET_FBITS(1,drop_flag);
    MPG_GET_FBITS(5,hours);
    MPG_GET_FBITS(6,minutes);
    MPG_GET_FBITS(1,marker);
    MPG_GET_FBITS(6,seconds);
    MPG_GET_FBITS(6,mpg_num_pics);
    MPG_GET_FBITS(1,closed_gop);
    MPG_GET_FBITS(1,broken_link);
  }
  DEBUG_LEVEL1 fprintf(stderr,"  dflag %d time %02d:%02d:%02d mrkr %d\n",
	drop_flag,hours,minutes,seconds,marker);
  DEBUG_LEVEL1 fprintf(stderr,"   numpics %d %d %d\n",
	mpg_num_pics,closed_gop,broken_link);
 /* USR EXT data?? */
  return(xaTRUE);
}

/*
00000000: 000001B3 0A007819 FFFFE084 000001B2   ......x.........
00000010: 000000E8 000001B8 00000000 00000100   ................
00000020: 004FFFFF FFFC0000 01B2FFFF 00000101   .O..............
00000030: 43FBA498 FCE3E785 73BF8D80 3C48D0CC   C.......s...<H..
00000040: 33E6E2FB FC440FBC 280E80BA 502CFFDC   3....D..(...P,..
00000050: 9FEFBA00 3A02A929 3D1BA47E 6E177C58   ....:..)=...n..X
...
*/

/*******
 *
 * 10 timestamp
 *  3 frame type (I=1   P=2   B=3 )
 * 16 vbv buffer delay
 * if P or B frame
 *   1 full_pel_forw_vect flag
 *   3 forw_r_code  forw_r_size = code-1; forw_f = (1 << forw_r_size);
 * if P or B frame
 *   1 full_pel_back_vect flag (back vector full pixel flag)
 *   3 back_r_code  back_r_size = code-1; back_f = (1 << back_r_size);
 *
 */


xaULONG mpg_read_PIC_HDR(phdr,bflag)
MPG_PIC_HDR *phdr;
xaULONG bflag;
{ xaULONG d;
  if (bflag == MPG_FROM_BUFF)
  {
    MPG_GET_BBITS(10,phdr->time);
    MPG_GET_BBITS(3,phdr->type);
    MPG_GET_BBITS(16,phdr->vbv_delay);
    phdr->full_forw_flag = 0;
    phdr->forw_r_size = phdr->forw_f = 0;
    phdr->full_back_flag = 0;
    phdr->back_r_size = phdr->back_f = 0;

    if ( (phdr->type == MPG_TYPE_P) || (phdr->type == MPG_TYPE_B))
    {
      MPG_GET_BBITS(1,phdr->full_forw_flag);
      MPG_GET_BBITS(3,d);
      phdr->forw_r_size = d - 1;
      phdr->forw_f = (1 << phdr->forw_r_size);
    }
    else if (phdr->type == MPG_TYPE_B)
    {
      MPG_GET_BBITS(1,phdr->full_back_flag);
      MPG_GET_BBITS(3,d);
      phdr->back_r_size = d - 1;
      phdr->back_f = (1 << phdr->back_r_size);
    }
  }
  else /* From File */
  {
    MPG_GET_FBITS(10,phdr->time);
    MPG_GET_FBITS(3,phdr->type);
    MPG_GET_FBITS(16,phdr->vbv_delay);
    phdr->full_forw_flag = 0;
    phdr->forw_r_size = phdr->forw_f = 0;
    phdr->full_back_flag = 0;
    phdr->back_r_size = phdr->back_f = 0;

    if ( (phdr->type == MPG_TYPE_P) || (phdr->type == MPG_TYPE_B))
    {
      MPG_GET_FBITS(1,phdr->full_forw_flag);
      MPG_GET_FBITS(3,d);
      phdr->forw_r_size = d - 1;
      phdr->forw_f = (1 << phdr->forw_r_size);
    }
    else if (phdr->type == MPG_TYPE_B)
    {
      MPG_GET_FBITS(1,phdr->full_back_flag);
      MPG_GET_FBITS(3,d);
      phdr->back_r_size = d - 1;
      phdr->back_f = (1 << phdr->back_r_size);
    }
  }
  DEBUG_LEVEL1 fprintf(stderr,"MPG_PIC_START: TYPE %d\n",phdr->type);
  return(xaTRUE);
}


/*******
 * assume mpg_pic_hdr is valid
 *
 * USES global variable xa_vid_fd
 */
xaULONG MPG_Setup_Delta(mpg,fname,act)
XA_ANIM_SETUP *mpg;
char *fname;
XA_ACTION *act;			/* action to use  */
{
  ACT_DLTA_HDR *dlta_hdr = (ACT_DLTA_HDR *)act->data;
  MPG_PIC_HDR *the_pic_hdr = (MPG_PIC_HDR *)dlta_hdr->data;
  MPG_SEQ_HDR *the_seq_hdr = the_pic_hdr->seq_hdr;

  if ( (xa_buffer_flag==xaTRUE) || (xa_file_flag==xaTRUE) )
  { /* POD CAUTION   xa_vid_fd is global variable */
    if ( (xa_vid_fd=open(fname,O_RDONLY,NULL)) == 0)
    { fprintf(stderr,"MPG: Open file %s for video err\n",fname);
      return(xaFALSE);
    }
    if (mpg_dlta_buf==0)
    { mpg_dlta_size = mpg->max_fvid_size;
      mpg_dlta_buf = (xaUBYTE *)malloc(mpg_dlta_size);
      if (mpg_dlta_buf == 0) TheEnd1("MPG: dlta_buf alloc err");
    }
    else if (mpg->max_fvid_size > mpg_dlta_size)
    { xaUBYTE *tmp;
      mpg_dlta_size = mpg->max_fvid_size;
      tmp = (xaUBYTE *)realloc(mpg_dlta_buf,mpg_dlta_size);
      if (tmp) mpg_dlta_buf = tmp;
      else TheEnd1("MPG: dlta_buf realloc err");
    }
  }

  dlta_hdr->xpos = dlta_hdr->ypos = 0;
  dlta_hdr->xsize = the_seq_hdr->width;
  dlta_hdr->ysize = the_seq_hdr->height;
  dlta_hdr->special = 0;
  dlta_hdr->extra = (void *)(&mpg_dlta_buf); /* scratch buffer */
  dlta_hdr->xapi_rev = 0x0001;
  switch(the_pic_hdr->type)
  {
    case MPG_TYPE_I:
	dlta_hdr->delta = MPG_Decode_I;
	break;
    case MPG_TYPE_B:
	dlta_hdr->delta = MPG_Decode_I; /* MPG_Decode_B; */
	act->type = ACT_NOP; /* POD TEMP */
	return(xaTRUE);
	break;
    case MPG_TYPE_P:
	dlta_hdr->delta = MPG_Decode_I; /* MPG_Decode_P; */
	act->type = ACT_NOP; /* POD TEMP */
	return(xaTRUE);
	break;
    default:
	fprintf(stderr,"MPG frame type %d unknown\n",the_pic_hdr->type);
	act->type = ACT_NOP;
	return(xaFALSE);
	break;
  }

  ACT_Setup_Delta(mpg,act,dlta_hdr,0);

  if (xa_vid_fd >= 0) { close(xa_vid_fd); xa_vid_fd = -1; }
  return(xaTRUE);
}



/*******
 *
 */
xaULONG
MPG_Decode_B(image,delta,dsize,tchdr,map,map_flag,imagex,imagey,imaged,
                                                xs,ys,xe,ye,special,extra)
xaUBYTE *image;           /* Image Buffer. */
xaUBYTE *delta;           /* delta data. */
xaULONG dsize;            /* delta size */
XA_CHDR *tchdr;         /* color map info */
xaULONG *map;             /* used if it's going to be remapped. */
xaULONG map_flag;         /* whether or not to use remap_map info. */
xaULONG imagex,imagey;    /* Size of image buffer. */
xaULONG imaged;           /* Depth of Image. (IFF specific) */
xaULONG *xs,*ys;          /* pos of changed area. */
xaULONG *xe,*ye;          /* size of changed area. */
xaULONG special;          /* Special Info. */
void *extra;            /* extra info needed to decode delta */
{
  XA_CHDR *chdr;
  if (tchdr) {chdr=(tchdr->new_chdr)?(tchdr->new_chdr):(tchdr);} else chdr=0;
  *xs = *ys = *xe = *ye = 0; return(ACT_DLTA_NOP);


/*
  if (map_flag) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
*/
}



/**************** HUFFMAN TABLES AND INIT ******************************/

#define MB_ESCAPE 35
#define MB_STUFFING 34

/* Structure for an entry in the decoding table of
 * macroblock_address_increment */

mb_addr_inc_entry    *mb_addr_inc = 0;		/* MacroBlk Addr Inc huff tab */
mb_type_entry        *mb_type_P = 0;		/* MacroType P huff table */
mb_type_entry        *mb_type_B = 0;		/* MacroType B huff table */
motion_vectors_entry *motion_vectors = 0;	/* Motion Vectors huff table */



/* Code for unbound values in decoding tables */
#define ERROR -1
#define DCT_ERROR 63

#define MACRO_BLOCK_STUFFING 34
#define MACRO_BLOCK_ESCAPE 35

/*******
 * Macro for filling up the decoding table for mb_addr_inc
 */
#define ASSIGN1(start, end, step, val, num)	\
  for (i = start; i < end; i+= step)	\
  { for (j = 0; j < step; j++)		\
    { mb_addr_inc[i+j].value = val;	\
      mb_addr_inc[i+j].num_bits = num;	\
    }		\
    val--;	\
  }

/*******
 * Init mb_addr_inc huffman table 
 */
void mpg_init_mb_addr_inc()
{ int i, j, val;

  for (i = 0; i < 8; i++) 
  { mb_addr_inc[i].value = ERROR;
    mb_addr_inc[i].num_bits = 0;
  }

  mb_addr_inc[8].value = MACRO_BLOCK_ESCAPE;
  mb_addr_inc[8].num_bits = 11;

  for (i = 9; i < 15; i++) 
  { mb_addr_inc[i].value = ERROR;
    mb_addr_inc[i].num_bits = 0;
  }

  mb_addr_inc[15].value = MACRO_BLOCK_STUFFING;
  mb_addr_inc[15].num_bits = 11;

  for (i = 16; i < 24; i++) 
  { mb_addr_inc[i].value = ERROR;
    mb_addr_inc[i].num_bits = 0;
  }

  val = 33;

  ASSIGN1(24, 36, 1, val, 11);
  ASSIGN1(36, 48, 2, val, 10);
  ASSIGN1(48, 96, 8, val, 8);
  ASSIGN1(96, 128, 16, val, 7);
  ASSIGN1(128, 256, 64, val, 5);
  ASSIGN1(256, 512, 128, val, 4);
  ASSIGN1(512, 1024, 256, val, 3);
  ASSIGN1(1024, 2048, 1024, val, 1);
}

/* Macro for filling up the decoding table for mb_type */
#define ASSIGN2(start, end, quant, motion_forward, motion_backward, pattern, intra, num, mb_type) \
  for (i = start; i < end; i ++)			\
  { mb_type[i].mb_quant = quant;			\
    mb_type[i].mb_motion_forward = motion_forward;	\
    mb_type[i].mb_motion_backward = motion_backward;	\
    mb_type[i].mb_pattern = pattern;			\
    mb_type[i].mb_intra = intra;			\
    mb_type[i].num_bits = num;				\
  }


/*******
 *      Initialize the VLC decoding table for macro_block_type in
 *      predictive-coded pictures.
 */
void mpg_init_mb_type_P()
{ int i;

  mb_type_P[0].mb_quant = mb_type_P[0].mb_motion_forward
	= mb_type_P[0].mb_motion_backward = mb_type_P[0].mb_pattern
	= mb_type_P[0].mb_intra = ERROR;
  mb_type_P[0].num_bits = 0;

  ASSIGN2( 1,  2, 1, 0, 0, 0, 1, 6, mb_type_P)
  ASSIGN2( 2,  4, 1, 0, 0, 1, 0, 5, mb_type_P)
  ASSIGN2( 4,  6, 1, 1, 0, 1, 0, 5, mb_type_P);
  ASSIGN2( 6,  8, 0, 0, 0, 0, 1, 5, mb_type_P);
  ASSIGN2( 8, 16, 0, 1, 0, 0, 0, 3, mb_type_P);
  ASSIGN2(16, 32, 0, 0, 0, 1, 0, 2, mb_type_P);
  ASSIGN2(32, 64, 0, 1, 0, 1, 0, 1, mb_type_P);
}

/******
 *      Initialize the VLC decoding table for macro_block_type in
 *      bidirectionally-coded pictures.
 */
void mpg_init_mb_type_B()
{ int i;

  mb_type_B[0].mb_quant = mb_type_B[0].mb_motion_forward
	= mb_type_B[0].mb_motion_backward = mb_type_B[0].mb_pattern
	= mb_type_B[0].mb_intra = ERROR;
  mb_type_B[0].num_bits = 0;

  ASSIGN2( 1,  2, 1, 0, 0, 0, 1, 6, mb_type_B);
  ASSIGN2( 2,  3, 1, 0, 1, 1, 0, 6, mb_type_B);
  ASSIGN2( 3,  4, 1, 1, 0, 1, 0, 6, mb_type_B);
  ASSIGN2( 4,  6, 1, 1, 1, 1, 0, 5, mb_type_B);
  ASSIGN2( 6,  8, 0, 0, 0, 0, 1, 5, mb_type_B);
  ASSIGN2( 8, 12, 0, 1, 0, 0, 0, 4, mb_type_B);
  ASSIGN2(12, 16, 0, 1, 0, 1, 0, 4, mb_type_B);
  ASSIGN2(16, 24, 0, 0, 1, 0, 0, 3, mb_type_B);
  ASSIGN2(24, 32, 0, 0, 1, 1, 0, 3, mb_type_B);
  ASSIGN2(32, 48, 0, 1, 1, 0, 0, 2, mb_type_B);
  ASSIGN2(48, 64, 0, 1, 1, 1, 0, 2, mb_type_B);
}

/* Macro for filling up the decoding tables for motion_vectors */
#define ASSIGN3(start, end, step, val, num)	\
  for (i = start; i < end; i+= step) 		\
  { for (j = 0; j < step / 2; j++)		\
    { motion_vectors[i+j].code = val;		\
      motion_vectors[i+j].num_bits = num;	\
    }						\
    for (j = step / 2; j < step; j++) 		\
    { motion_vectors[i+j].code = -val;		\
      motion_vectors[i+j].num_bits = num;	\
    }		\
    val--;	\
  }

/*******
 *      Initialize the VLC decoding table for the various motion
 *      vectors, including motion_horizontal_forward_code,
 *      motion_vertical_forward_code, motion_horizontal_backward_code,
 *      and motion_vertical_backward_code.
 */
void mpg_init_motion_vectors()
{ int i, j, val = 16;

  for (i = 0; i < 24; i++) 
  { motion_vectors[i].code = ERROR;
    motion_vectors[i].num_bits = 0;
  }

  ASSIGN3(24, 36, 2, val, 11);
  ASSIGN3(36, 48, 4, val, 10);
  ASSIGN3(48, 96, 16, val, 8);
  ASSIGN3(96, 128, 32, val, 7);
  ASSIGN3(128, 256, 128, val, 5);
  ASSIGN3(256, 512, 256, val, 4);
  ASSIGN3(512, 1024, 512, val, 3);
  ASSIGN3(1024, 2048, 1024, val, 1);
}

/*
 *--------------------------------------------------------------
 *
 * DecodeDCTDCSizeLum --
 *
 *	Huffman Decoder for dct_dc_size_luminance; location where
 *      the result of decoding will be placed is passed as argument.
 *      The decoded values are obtained by doing a table lookup on
 *      dct_dc_size_luminance.
 *
 * Results:
 *	The decoded value for dct_dc_size_luminance or ERROR for 
 *      unbound values will be placed in the location specified.
 *
 * Side effects:
 *	Bit stream is irreversibly parsed.
 *
 *--------------------------------------------------------------
 */        
void decodeDCTDCSizeLum(value)
unsigned int *value;
{ xaULONG idx,f;
  MPG_NXT_BBITS(7,idx);
  *value = dct_dc_size_luminance[idx].value;
  f = dct_dc_size_luminance[idx].num_bits;
  MPG_FLUSH_BBITS(f);
}


/*
 *--------------------------------------------------------------
 *
 * DecodeDCTDCSizeChrom --
 *
 *	Huffman Decoder for dct_dc_size_chrominance; location where
 *      the result of decoding will be placed is passed as argument.
 *      The decoded values are obtained by doing a table lookup on
 *      dct_dc_size_chrominance.
 *
 * Results:
 *	The decoded value for dct_dc_size_chrominance or ERROR for
 *      unbound values will be placed in the location specified.
 *
 * Side effects:
 *	Bit stream is irreversibly parsed.
 *
 *--------------------------------------------------------------
 */    
void    decodeDCTDCSizeChrom(value)
unsigned int *value;
{ xaULONG idx,f;
  MPG_NXT_BBITS(8,idx);
  *value = dct_dc_size_chrominance[idx].value;
  f = dct_dc_size_chrominance[idx].num_bits; 
  MPG_FLUSH_BBITS(f);
}

#define RUN_MASK	0xfc00
#define LEVEL_MASK	0x03f0
#define NUM_MASK	0x000f
#define RUN_SHIFT	  10
#define LEVEL_SHIFT	   4
#define END_OF_BLOCK	0x3e
#define ESCAPE		0x3d


/*
 *--------------------------------------------------------------
 *
 * decodeDCTCoeff --
 *
 *	Huffman Decoder for dct_coeff_first and dct_coeff_next;
 *      locations where the results of decoding: run and level, are to
 *      be placed and also the type of DCT coefficients, either
 *      dct_coeff_first or dct_coeff_next, are being passed as argument.
 *      
 *      The decoder first examines the next 8 bits in the input stream,
 *      and perform according to the following cases:
 *      
 *      '0000 0000' - examine 8 more bits (i.e. 16 bits total) and
 *                    perform a table lookup on dct_coeff_tbl_0.
 *                    One more bit is then examined to determine the sign
 *                    of level.
 *
 *      '0000 0001' - examine 4 more bits (i.e. 12 bits total) and 
 *                    perform a table lookup on dct_coeff_tbl_1.
 *                    One more bit is then examined to determine the sign
 *                    of level.
 *      
 *      '0000 0010' - examine 2 more bits (i.e. 10 bits total) and
 *                    perform a table lookup on dct_coeff_tbl_2.
 *                    One more bit is then examined to determine the sign
 *                    of level.
 *
 *      '0000 0011' - examine 2 more bits (i.e. 10 bits total) and 
 *                    perform a table lookup on dct_coeff_tbl_3.
 *                    One more bit is then examined to determine the sign
 *                    of level.
 *
 *      otherwise   - perform a table lookup on dct_coeff_tbl. If the
 *                    value of run is not ESCAPE, extract one more bit
 *                    to determine the sign of level; otherwise 6 more
 *                    bits will be extracted to obtain the actual value 
 *                    of run , and then 8 or 16 bits to get the value of level.
 *                    
 *      
 *
 * Results:
 *	The decoded values of run and level or ERROR for unbound values
 *      are placed in the locations specified.
 *
 * Side effects:
 *	Bit stream is irreversibly parsed.
 *
 *--------------------------------------------------------------
 */
static void decodeDCTCoeff(dct_coeff_tbl, run, level)
xaUSHORT *dct_coeff_tbl;                                       
xaULONG *run;
xaLONG *level;
{ xaULONG temp, idx, value, f;

  MPG_NXT_BBITS(8,idx);

  if (idx > 3) 
  {
    value = dct_coeff_tbl[idx];
    *run = (value & RUN_MASK) >> RUN_SHIFT;
    if (*run == END_OF_BLOCK) { *level = END_OF_BLOCK; }
    else 
    { f = (value & NUM_MASK) + 1;
      MPG_FLUSH_BBITS(f);

      if (*run != ESCAPE) 
      { xaULONG t;
         *level = (value & LEVEL_MASK) >> LEVEL_SHIFT;
	 MPG_GET_BBITS(1,t); 
	 if (t) *level = - *level;
      }
      else	/* *run == ESCAPE */
      { /* get_bits14(temp); */
	MPG_GET_BBITS(14,temp);  /* 6 are run, 8 are part of level */
	*run = temp >> 8;  /*  1st 6 bits are run */

	temp &= 0xff;  /* next 8 are for value */
	if (temp == 0) 
	{ MPG_GET_BBITS(8,*level);
	/* POD Is Ness?? */
/*
	  if ( (*level) < 128 ) fprintf(stderr,"DCT err A-%d\n",*level);
*/
	} else if (temp != 128) 
	{ 
	  if (temp & 0x80) *level = temp - 256;
          else *level = temp;
/* was
	  *level = ((xaLONG) (temp << 24)) >> 24; 
*/
	}
	else
	{ MPG_GET_BBITS(8,*level);
	  *level = *level - 256;
	/* POD Is Ness?? */
/*
	  if ( ((*level) > -128) || ((*level) < -255) )
			fprintf(stderr,"DCT err B-%d\n",*level);
*/
	}
      }
    }
  }
  else 
  { xaULONG t;
    if (idx == 2) 
    { MPG_NXT_BBITS(10,idx); 
      value = dct_coeff_tbl_2[idx & 3];
    }
    else if (idx == 3) 
    { MPG_NXT_BBITS(10,idx); 
      value = dct_coeff_tbl_3[idx & 3];
    }
    else if (idx)	/* idx == 1 */
    { MPG_NXT_BBITS(12,idx); 
      value = dct_coeff_tbl_1[idx & 15];
    }
    else /* idx == 0 */
    { MPG_NXT_BBITS(16,idx); 
      value = dct_coeff_tbl_0[idx & 255];
    }
    *run = (value & RUN_MASK) >> RUN_SHIFT;
    *level = (value & LEVEL_MASK) >> LEVEL_SHIFT;

    f = (value & NUM_MASK) + 1;
    MPG_FLUSH_BBITS(f);
    MPG_GET_BBITS(1,t);
    if (t) *level = -*level;
  }
}

/*
 *--------------------------------------------------------------
 *
 * decodeDCTCoeffFirst --
 *
 *	Huffman Decoder for dct_coeff_first. Locations for the
 *      decoded results: run and level, are being passed as
 *      arguments. Actual work is being done by calling DecodeDCTCoeff,
 *      with the table dct_coeff_first.
 *
 * Results:
 *	The decoded values of run and level for dct_coeff_first or
 *      ERROR for unbound values are placed in the locations given.
 *
 * Side effects:
 *	Bit stream is irreversibly parsed.
 *
 *--------------------------------------------------------------
 */        
void decodeDCTCoeffFirst(run, level)
xaULONG *run;
xaLONG *level;
{
  decodeDCTCoeff(dct_coeff_first, run, level);
}



/*
 *--------------------------------------------------------------
 *
 * decodeDCTCoeffNext --
 *
 *	Huffman Decoder for dct_coeff_first. Locations for the
 *      decoded results: run and level, are being passed as
 *      arguments. Actual work is being done by calling DecodeDCTCoeff,
 *      with the table dct_coeff_next.
 *
 * Results:
 *	The decoded values of run and level for dct_coeff_next or
 *      ERROR for unbound values are placed in the locations given.
 *
 * Side effects:
 *	Bit stream is irreversibly parsed.
 *
 *--------------------------------------------------------------
 */ 
void       decodeDCTCoeffNext(run, level)
xaULONG *run;
xaLONG *level;
{
  decodeDCTCoeff(dct_coeff_next, run, level);
}

/********************END HUFFMAN*******************************************/

/***********************************************
 * Function to
 *      Initialize all the tables for VLC decoding; this must be
 *      called when the system is set up before any decoding can
 *      take place.
 *
 *********************/

void MPG_Init_Stuff(XA_ANIM_HDR *anim_hdr)
{
  XA_Add_Func_To_Free_Chain(anim_hdr,MPG_Free_Stuff);
  if (mb_addr_inc == 0)  
  { mb_addr_inc 
	= (mb_addr_inc_entry *)malloc(2048 * sizeof(mb_addr_inc_entry));
    mpg_init_mb_addr_inc();
  }
  if (mb_type_P == 0)
  { mb_type_P = (mb_type_entry *)malloc(64 * sizeof(mb_type_entry));
    mpg_init_mb_type_P();
  }
  if (mb_type_B == 0)
  { mb_type_B = (mb_type_entry *)malloc(64 * sizeof(mb_type_entry));
    mpg_init_mb_type_B();
  }
  if (motion_vectors==0)
  { motion_vectors 
	= (motion_vectors_entry *)malloc(2048 * sizeof(motion_vectors_entry));
    mpg_init_motion_vectors();
  }
}


/***********************************************
 * Function to Free MPG tables and buffers, etc.
 *
 *********************/
void MPG_Free_Stuff()
{
  if (mb_addr_inc) { free(mb_addr_inc); mb_addr_inc = 0; }
  if (mb_type_P) { free(mb_type_P); mb_type_P = 0; }
  if (mb_type_B) { free(mb_type_B); mb_type_B = 0; }
  if (motion_vectors) { free(motion_vectors); motion_vectors = 0; }
  if (mpg_dlta_buf) { free(mpg_dlta_buf); mpg_dlta_buf = 0; }
}

#define MPG_GET_MBAddrInc(ainc) \
{ register xaULONG i,j;		\
  MPG_NXT_BBITS(11,i);		\
  ainc = mb_addr_inc[i].value;	\
  j = mb_addr_inc[i].num_bits;	\
  MPG_FLUSH_BBITS(j);		\
}

#define DCTSIZE   8
#define DCTSIZE2 64

/*
MBACoeff
MVDCoeff
CBPCoeff
TCoeff1
TCoeff2
TCoeff3
TCoeff4
IntraTypeCoeff
PredictedTypeCoeff
InterpolatedTypeCoeff
DCLumCoeff
DCChromCoeff
*/

/**** HUFFMAN TABLES ****************
 *Bits Vals 
 * 11   36   MBA Coeff    
 * 11   33   MVD Coeff
 *  9   63   CBP Coef
 *           T Coeff 1
 *           T Coeff 2
 *           T Coeff 3
 *           T Coeff 4
 *  2    2   Intra Type Coeff
 *  6    7   Predicted Type Coeff
 *  6   11   Interpolated Type Coeff
 *  7    9   DC Lum Coeff
 *  8    9   DC Chrom Coeff
 *  1    1   DC Type Coeff ???
 *******************************/

/*****
 *
 */
#define MPG_CMAP_CACHE_CHECK(chdr); { if (cmap_cache == 0) CMAP_Cache_Init(0); \
  if (chdr != cmap_cache_chdr) { CMAP_Cache_Clear(); cmap_cache_chdr = chdr; \
} } \


xaLONG mpg_dc_y, mpg_dc_cr, mpg_dc_cb;

/*******
 *
 */
xaULONG
MPG_Decode_I(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG special = dec_info->special;		void *extra = dec_info->extra;
  XA_CHDR *chdr = dec_info->chdr;
  xaULONG mb_addr,vert_pos,q_scale;
  xaLONG mcu_cols,mcu_rows;
  xaUBYTE *qtab;
  xaUBYTE *Ybuf,*Ubuf,*Vbuf;
  xaULONG orow_size;
  MPG_PIC_HDR *phdr = (MPG_PIC_HDR *)(delta);
  MPG_SEQ_HDR *shdr =  phdr->seq_hdr;
  MPG_SLICE_HDR *slice;
  xaUBYTE *data_buf;
  xaULONG special_flag;
  xaUBYTE *iptr = image;
  void (*color_func)();
  xaULONG mb_size;
  xaLONG sidx;

   /* always full image for now */
  dec_info->xs = dec_info->ys = 0;
  dec_info->xe = imagex; dec_info->ye = imagey; 

   /* Indicate we can drop these frames */
  if (dec_info->skip_flag > 0) return(ACT_DLTA_DROP);
   
  if (chdr) { if (chdr->new_chdr) chdr = chdr->new_chdr; }

  special_flag = special & 0x0001;
  if (cmap_color_func == 4) { MPG_CMAP_CACHE_CHECK(chdr); }

  orow_size = imagex;
	/*** Setup Color Decode Functions */
  if (special_flag) { orow_size *= 3;	color_func = XA_MCU221111_To_RGB; }
  else
  { orow_size *= x11_bytes_pixel;
    if (x11_bytes_pixel==1)		
    { 
      color_func = XA_MCU221111_To_CLR8;
      if ( (chdr) && (x11_display_type == XA_PSEUDOCOLOR))
      {
	if (cmap_color_func == 4)        color_func = XA_MCU221111_To_CF4;
	else if ( (cmap_true_to_332 == xaTRUE) && (x11_cmap_size == 256) ) {
        if (xa_dither_flag==xaTRUE)    { color_func = XA_MCU221111_To_332_Dither; }
        else                           { color_func = XA_MCU221111_To_332; }
        }
      }
    }
    else if (x11_bytes_pixel==2)	color_func = XA_MCU221111_To_CLR16;
    else				color_func = XA_MCU221111_To_CLR32;
  }
  imagex++; imagex >>= 1;
    
  mcu_cols = ((shdr->width  + 15) / 16);
  mcu_rows = ((shdr->height + 15) / 16);
  mb_size = mcu_cols * mcu_rows;

DEBUG_LEVEL1 fprintf(stderr,"mcu xy %d %d  size %d\n",
					mcu_cols,mcu_rows,mb_size);

  qtab = shdr->intra_qtab;

  /* get current slice */
  sidx = 0;
  slice = &(phdr->slices[sidx]);

	/*** Loop through slices in order to construct each Image */
  while(slice->fsize)
  { 
    if (slice->act) data_buf = (xaUBYTE *)(slice->act->data);
    else data_buf = 0;

    if (data_buf == 0)
    { xaLONG ret;
      xaUBYTE **t = (xaUBYTE **)(extra);
      data_buf = *t;
      ret = lseek(xa_vid_fd,slice->fpos,SEEK_SET);
      ret = read(xa_vid_fd,data_buf,slice->fsize);
      /* read data into slice */
    }

    MPG_INIT_BBUF(data_buf,slice->fsize);
    vert_pos = slice->vert_pos - 1;
    MPG_GET_BBITS(5,q_scale);  /* quant scale */
    mpg_skip_extra_bitsB();

    /* move on */
    sidx++; 
    slice = &(phdr->slices[sidx]);

DEBUG_LEVEL1 { if (vert_pos != 0) fprintf(stderr,"VERT_POS = %d mcu rows = %d cols = %d\n",vert_pos,mcu_rows,mcu_cols); }

    /* adjust pointers for slice */

    mb_addr = (vert_pos * mcu_cols) - 1;

    mpg_dc_y = mpg_dc_cr = mpg_dc_cb = 1024;

    do  /* while(mb_addr < mb_size) */
    { xaULONG tmp,i;

	/* parse MB addr increment and update MB address */
DEBUG_LEVEL1 fprintf(stderr,"PARSE MB ADDR(%d): ",mb_addr);

	do
	{ MPG_GET_MBAddrInc(tmp);  /* huff decode MB addr inc */

DEBUG_LEVEL1 fprintf(stderr," %d",tmp);

	  if (tmp == MB_ESCAPE) 
	  { mb_addr += 33; tmp = MB_STUFFING;
	  }
	} while(tmp == MB_STUFFING);

        if (tmp < 0)
        {
         /* END OF SLICE?? */
          break;
        }

	if (tmp > 1) mpg_dc_y = mpg_dc_cr = mpg_dc_cb = 1024;
	mb_addr += tmp;
DEBUG_LEVEL1 fprintf(stderr,"  :mb_addr %d\n",mb_addr);

	if (mb_addr >= mb_size) break;

	/* check for end of slice */
	MPG_CHECK_BBITS(4,tmp); 
        if (tmp == xaFALSE) 
        { DEBUG_LEVEL1 fprintf(stderr,"end of slice\n");
          break;
        }

/* Calculate Y,U,V buffers */
{ xaULONG offset;
  Ybuf = jpg_YUVBufs.Ybuf; Ubuf = jpg_YUVBufs.Ubuf; Vbuf = jpg_YUVBufs.Vbuf;

  offset = DCTSIZE2 * mb_addr;
  Ubuf += offset;
  Vbuf += offset;
  Ybuf += offset << 2;
}

	/* Decode I Type MacroBlock Type  1 or 01 */
	MPG_NXT_BBITS(2,i);

DEBUG_LEVEL1 fprintf(stderr,"MB type %x\n",i);

	if (i & 0x02) { MPG_FLUSH_BBITS(1); }
	/* else if (i == 0x00) ERROR */
	else /* new quant scale */
	{ MPG_FLUSH_BBITS(2);
	  MPG_GET_BBITS(5,q_scale);  /* quant scale */
	  DEBUG_LEVEL1 fprintf(stderr,"New Quant Scale %x\n",q_scale);
	}
	mpg_huffparseI(dct_dc_size_luminance, 7, &mpg_dc_y,
			q_scale, qtab, mpg_dct_buf, Ybuf); Ybuf += DCTSIZE2;
	mpg_huffparseI(dct_dc_size_luminance, 7, &mpg_dc_y,
			q_scale, qtab, mpg_dct_buf, Ybuf); Ybuf += DCTSIZE2;
	mpg_huffparseI(dct_dc_size_luminance, 7, &mpg_dc_y,
			q_scale, qtab, mpg_dct_buf, Ybuf); Ybuf += DCTSIZE2;
	mpg_huffparseI(dct_dc_size_luminance, 7, &mpg_dc_y,
			q_scale, qtab, mpg_dct_buf, Ybuf); Ybuf += DCTSIZE2;
	mpg_huffparseI(dct_dc_size_chrominance, 8, &mpg_dc_cr,
			q_scale, qtab, mpg_dct_buf, Ubuf); Ubuf += DCTSIZE2;
	mpg_huffparseI(dct_dc_size_chrominance, 8, &mpg_dc_cb,
			q_scale, qtab, mpg_dct_buf, Vbuf); Vbuf += DCTSIZE2;
    } while(mb_addr < (mb_size - 1) );
  } /* end of while slices */

  (void)(color_func)(iptr,imagex,imagey,(mcu_cols * DCTSIZE2),orow_size,
			&jpg_YUVBufs,&def_yuv_tabs,map_flag,map,chdr);

  if (map_flag) return(ACT_DLTA_MAPD); else return(ACT_DLTA_NORM);
}


xaULONG MPG_Decode_B();
#ifdef NOTYET_FINISHED
/*******
 *
 */
xaULONG
MPG_Decode_P(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG special = dec_info->special;          void *extra = dec_info->extra;
  XA_CHDR *chdr = dec_info->chdr;

  dec_info->xs = dec_info->ys = 0;
  dec_info->xe = imagex; dec_info->ye = imagey; 
  if (chdr) { if (chdr->new_chdr) chdr = chdr->new_chdr; }

  if (map_flag) return(ACT_DLTA_MAPD); else return(ACT_DLTA_NORM);
}

  xaULONG mb_addr,vert_pos,q_scale;
  xaLONG x,y,mcu_cols,mcu_rows,mcu_row_size;
  xaUBYTE *qtab;
  xaUBYTE *Ybuf0,*Ybuf1,*Ubuf,*Vbuf;
  xaULONG orow_size;
  MPG_PIC_HDR *phdr = (MPG_PIC_HDR *)(delta);
  MPG_SEQ_HDR *shdr =  phdr->seq_hdr;
  MPG_SLICE_HDR *slice;
  xaUBYTE *data_buf;
  XA_CHDR *chdr;
  xaULONG special_flag;
  xaUBYTE *iptr = image;
  void (*color_func)();
  xaULONG mb_size,mb_cnt;
  xaLONG sidx,y_count;

xaULONG Pquant,Pmotion_fwd,Ppat,Pintra;
xaULONG P_cbp;
xaLONG mot_h_forw_code,mot_v_forw_code;
xaULONG mot_h_forw_r,mot_v_forw_r;
xaLONG recon_right_for,recon_down_for;
xaLONG recon_right_for_prev,recon_down_for_prev;



  *xs = *ys = 0; *xe = imagex; *ye = imagey; 
  if (tchdr) {chdr=(tchdr->new_chdr)?(tchdr->new_chdr):(tchdr);} else chdr=0;

  special_flag = special & 0x0001;
  if (cmap_color_func == 4) { MPG_CMAP_CACHE_CHECK(chdr); }

  orow_size = imagex;
  if (special_flag) { orow_size *= 3;	color_func = XA_MCU221111_To_RGB; }
  else { orow_size *= x11_bytes_pixel;
    if (x11_bytes_pixel==1)		
    { 
      if ((cmap_color_func == 4) && (chdr)
          && !(x11_display_type & XA_X11_TRUE) ) color_func = XA_MCU221111_To_CF4;
      else color_func = XA_MCU221111_To_CLR8;
    }
    else if (x11_bytes_pixel==2)	color_func = XA_MCU221111_To_CLR16;
    else				color_func = XA_MCU221111_To_CLR32;
  }
  imagex++; imagex >>= 1;

  mcu_cols  = ((shdr->width  + 15) / 16);
  mcu_rows = ((shdr->height + 15) / 16);
  mcu_row_size = mcu_cols * DCTSIZE2;
  mb_size = mcu_cols * mcu_rows;

  qtab = shdr->intra_qtab;

  /* get current slice */
  sidx = 0;
  slice = &(phdr->slices[sidx]);

  while(slice->fsize)
  { 
    data_buf = (xaUBYTE *)(slice->act->data);
    if (data_buf == 0)
    { xaLONG ret;
      xaUBYTE **t = (xaUBYTE **)(extra);
      data_buf = *t;
      ret = lseek(xa_vid_fd,slice->fpos,SEEK_SET);
      ret = read(xa_vid_fd,data_buf,slice->fsize);
      /* read data into slice */
    }

    MPG_INIT_BBUF(data_buf,slice->fsize);
    vert_pos = slice->vert_pos - 1;
    MPG_GET_BBITS(5,q_scale);  /* quant scale */
    mpg_skip_extra_bitsB();

    /* move on */
    sidx++; 
    slice = &(phdr->slices[sidx]);

DEBUG_LEVEL1 { if (vert_pos != 0) fprintf(stderr,"VERT_POS = %d\n",vert_pos); }

    /* adjust pointers for slice */
    y = mcu_rows - vert_pos;
    mb_addr = (vert_pos * mcu_rows) - 1;
    y_count = imagey - (vert_pos << 4);
    iptr   = image;
    iptr  += (orow_size * (vert_pos << 4));

    mpg_dc_y = mpg_dc_cr = mpg_dc_cb = 1024;
    mb_cnt = 0;

    x = 0;
    Ybuf0 = jpg_YUVBufs.Ybuf;		Ybuf1 = Ybuf0 + (mcu_row_size << 1);
    Ubuf  = jpg_YUVBufs.Ubuf;		Vbuf = jpg_YUVBufs.Vbuf;
    while(mb_cnt < mb_size)
    { xaULONG tmp,i;

	/* parse MB addr increment and update MB address */
	do
	{ MPG_GET_MBAddrInc(tmp);  /* huff decode MB addr inc */
	  if (tmp == MB_ESCAPE) { mb_addr += 33; tmp = MB_STUFFING; }
	} while(tmp == MB_STUFFING);

	mb_addr += tmp;
	if (tmp > 1) mpg_dc_y = mpg_dc_cr = mpg_dc_cb = 1024;

	if (mb_addr >= mb_size) {  break; }
	MPG_CHECK_BBITS(4,tmp); if (tmp == xaFALSE) { y = 0; break; }

	/* Decode P Type MacroBlock Type */

        MPG_NXT_BBITS(6,i);
        Pquant = mb_type_P[index].mb_quant;
        Pmotion_fwd = mb_type_P[index].mb_motion_forward;
	Ppat = mb_type_P[index].mb_pattern;
	Pintra = mb_type_P[index].mb_intra;
	MPG_FLUSH_BBITS(mb_type_P[index].num_bits);
		/* new quant scale */
        if (Pquant) 
	{
	  MPG_GET_BBITS(5,q_scale);
	  DEBUG_LEVEL1 fprintf(stderr,"New Quant Scale %x\n",q_scale);
	}
		/* forward motion vectors exist */
	if (mb_motion_forw)
	{ xaULONG i;

		/* decode horiz forward motion vector */
	  MPG_NXT_BBITS(11,i);
	  mot_h_forw_code = motion_vectors[i].code;
/* POD NOTE: signed?? dcval = mpg_huff_EXTEND(tmp,size); */
	  MPG_FLUSH_BBITS(motion_vectors[i].num_bits);
		/* if horiz forward r data exists, parse off */
	  if (mot_h_forw_code != 0) /* && (pic.forw_f != 1) */
	  {
	    MPG_GET_BBITS(forw_r_size,i);
	    mot_h_forw_r = i;  /* xaULONG */
	  }
		/* decode vert forward motion vector */
	  MPG_NXT_BBITS(11,i);
	  mot_v_forw_code = motion_vectors[i].code;
/* POD NOTE: signed?? dcval = mpg_huff_EXTEND(tmp,size); */
	  MPG_FLUSH_BBITS(motion_vectors[i].num_bits);
		/* if horiz forward r data exists, parse off */
	  if (mot_v_forw_code != 0) /* && (pic.forw_f != 1) */
	  {
	    MPG_GET_BBITS(forw_r_size,i);
	    mot_v_forw_r = i;  /* xaULONG */
	  }
	}
		/* Code Block Pattern exists */
	if (Ppat)
	{ xaULONG index,f;
	  MPG_NXT_BBITS(9,index);
	  P_cbp = coded_block_pattern[index].cbp;
	  f = coded_block_pattern[index].num_bits;
	  MPG_FLUSH_BBITS(f);
	} else P_cbp = 0;

	if (!mb_motion_forw)
	{
	  recon_right_for = recon_down_for = 0;
	  recon_right_for_prev = recon_down_for_prev = 0;
	}
	else
	{
/*
ComputeForwVector(recon_right_for_ptr, recon_down_for_ptr)
     int *recon_right_for_ptr;
     int *recon_down_for_ptr;
{ Pict *picture; Macroblock *mblock;
  picture = &(curVidStream->picture);
  mblock = &(curVidStream->mblock);
  ComputeVector(recon_right_for_ptr, recon_down_for_ptr,
                mblock->recon_right_for_prev,
                mblock->recon_down_for_prev,
                picture->forw_f, picture->full_pel_forw_vector,
                mblock->motion_h_forw_code, mblock->motion_v_forw_code,
                mblock->motion_h_forw_r, mblock->motion_v_forw_r);
*/

	}

	if ( (Pintra) || (P_cpb & 0x20) )
	{
	}
	if ( (Pintra) || (P_cpb & 0x10) )
	{
	}
	if ( (Pintra) || (P_cpb & 0x08) )
	{
	}
	if ( (Pintra) || (P_cpb & 0x04) )
	{
	}
	if ( (Pintra) || (P_cpb & 0x02) )
	{
	}
	if ( (Pintra) || (P_cpb & 0x01) )
	{
	}
	mpg_huffparseI(dct_dc_size_luminance, 7, &mpg_dc_y,
			q_scale, qtab, mpg_dct_buf, Ybuf0 ); Ybuf0 += DCTSIZE2;
	mpg_huffparseI(dct_dc_size_luminance, 7, &mpg_dc_y,
			q_scale, qtab, mpg_dct_buf, Ybuf0 ); Ybuf0 += DCTSIZE2;
	mpg_huffparseI(dct_dc_size_luminance, 7, &mpg_dc_y,
			q_scale, qtab, mpg_dct_buf, Ybuf1);  Ybuf1 += DCTSIZE2;
	mpg_huffparseI(dct_dc_size_luminance, 7, &mpg_dc_y,
			q_scale, qtab, mpg_dct_buf, Ybuf1);  Ybuf1 += DCTSIZE2;

	mpg_huffparseI(dct_dc_size_chrominance, 8, &mpg_dc_cr,
			q_scale, qtab, mpg_dct_buf, Ubuf);   Ubuf += DCTSIZE2;
	mpg_huffparseI(dct_dc_size_chrominance, 8, &mpg_dc_cb,
			q_scale, qtab, mpg_dct_buf, Vbuf);   Vbuf += DCTSIZE2;
      } /* end of mcu_cols */
      if (y) 
      { (void)(color_func)(iptr,imagex,y_count,mcu_row_size,orow_size,
			&jpg_YUVBufs, &def_yuv_tabs, map_flag,map,chdr);
	y_count -= 16;  iptr += (orow_size << 4);
	y--;
      }
    } /* end of y */
  } /* end of while slices */
  if (map_flag) return(ACT_DLTA_MAPD); else return(ACT_DLTA_NORM);
}
#endif


/******
 *
 */
#define mpg_huff_EXTEND(val,sz) \
 ((val) < (1<<((sz)-1)) ? (val) + (((-1)<<(sz)) + 1) : (val))

int zigzag_direct[64] = {
   0,  1,  8, 16,  9,  2,  3, 10, 17, 24, 32, 25, 18, 11, 4, 5, 12,
  19, 26, 33, 40, 48, 41, 34, 27, 20, 13,  6,  7, 14, 21, 28, 35,
  42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
  58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};


/*
#define DECODE_DCT_COEFF_FIRST DecodeDCTCoeffFirst
#define DECODE_DCT_COEFF_NEXT DecodeDCTCoeffNext
*/

#define DECODE_DCT_COEFF_FIRST decodeDCTCoeffFirst
#define DECODE_DCT_COEFF_NEXT  decodeDCTCoeffNext


/*
#define DecodeDCTCoeffFirst(runval, levelval) \
  { DecodeDCTCoeff(dct_coeff_first, runval, levelval); }

#define DecodeDCTCoeffNext(runval, levelval) \
  { DecodeDCTCoeff(dct_coeff_next, runval, levelval); }
*/


#define CENTERJSAMPLE 128
#define MAXJSAMPLE 255
#define RANGE_MASK  (MAXJSAMPLE * 4 + 3)


/**********
 *  dct_dc_htab is either dct_dc_size_luminance or dct_dc_size_chrominance
 *  dc_val_in is either mpg_dc_y, mpg_dc_cr or mpg_dc_cb
 *  qscale
 *  qtab is usually intra_quant table 
 *  hsz is 7 for luminance and 8 for chroma
 */
void mpg_huffparseI(dct_dc_htab,hsz,dc_val_in,qscale,qtab,dct_buf,OBuf)
dct_dc_size_entry *dct_dc_htab;
xaULONG hsz;
xaLONG *dc_val_in;
xaLONG qscale;
xaUBYTE *qtab;
xaSHORT *dct_buf;
xaUBYTE *OBuf;
{ xaULONG size,idx,flush;
  xaLONG tmp;
  xaLONG i,dcval,c_cnt;
  xaUBYTE *rnglimit = xa_byte_limit;

  MPG_NXT_BBITS(hsz,idx); /* look at next hsz bits */
  size =  dct_dc_htab[idx].value;
  flush = dct_dc_htab[idx].num_bits;
  MPG_FLUSH_BBITS(flush); /* flush used bits */

if (size > 10) { fprintf(stderr,"HUFF ERR \n"); return; }

  if (size)
  {
    MPG_GET_BBITS(size,tmp);  /* get bits */
    dcval = mpg_huff_EXTEND(tmp,size); /* sign extend them */
    dcval = (*dc_val_in) + (dcval << 3);
    *dc_val_in = dcval;
  } else dcval = *dc_val_in;
 
 memset( (char *)(dct_buf), 0, (DCTSIZE2 * sizeof(xaSHORT)) );
 dct_buf[0] = dcval;
 c_cnt = 0;

 i = 0;
 while( i < DCTSIZE2 )
 { xaLONG run,level;
   DECODE_DCT_COEFF_NEXT(&run,&level);
   if (run == END_OF_BLOCK) break;
   i += (run + 1);
   if (i < 64)
   { register xaULONG pos = zigzag_direct[ i ];
     register xaLONG coeff = (level * qscale * (xaLONG)(qtab[pos]) ) >> 3;
     if (level < 0) coeff += (coeff & 1);
     else coeff -= (coeff & 1);
     if (coeff) { dct_buf[pos] = coeff; c_cnt++; }
   }
 }
 MPG_FLUSH_BBITS(2); /* marker bits ?? */


/*
 DEBUG_LEVEL1
 { int ii;
   fprintf(stderr,"BLOCK \n");
   for(ii = 0; ii < 64; ii += 8)
   {
    fprintf(stderr,"  %08x %08x %08x %08x %08x %08x %08x %08x\n",
	dct_buf[ii], dct_buf[ii+1], dct_buf[ii+2], dct_buf[ii+3],
	dct_buf[ii+4], dct_buf[ii+5], dct_buf[ii+6], dct_buf[ii+7] );
   }
 }
*/


  if (c_cnt) j_rev_dct(dct_buf,OBuf,rnglimit);
  else
  { register xaUBYTE *op = OBuf;
    xaSHORT v = dcval;
    register xaUBYTE dc;
    v = (v < 0)?( (v-3)>>3 ):( (v+4)>>3 );
    dc = rnglimit[ (int) (v & RANGE_MASK) ];
    op[0]  = op[1]  = op[2]  = op[3]  = op[4]  = op[5]  = op[6]  =op[7] =dc;
    op[8]  = op[9]  = op[10] = op[11] = op[12] = op[13] = op[14] =op[15] =dc;
    op[16] = op[17] = op[18] = op[19] = op[20] = op[21] = op[22] =op[23] =dc;
    op[24] = op[25] = op[26] = op[27] = op[28] = op[29] = op[30] =op[31] =dc;
    op[32] = op[33] = op[34] = op[35] = op[36] = op[37] = op[38] =op[39] =dc;
    op[40] = op[41] = op[42] = op[43] = op[44] = op[45] = op[46] =op[47] =dc;
    op[48] = op[49] = op[50] = op[51] = op[52] = op[53] = op[54] =op[55] =dc;
    op[56] = op[57] = op[58] = op[59] = op[60] = op[61] = op[62] =op[63] =dc;
  }
  return;
}



#ifdef RIGHT_SHIFT_IS_UNSIGNED
#define SHIFT_TEMPS	xaLONG shift_temp;
#define RIGHT_SHIFT(x,shft)  \
	((shift_temp = (x)) < 0 ? \
	 (shift_temp >> (shft)) | ((~((xaLONG) 0)) << (32-(shft))) : \
	 (shift_temp >> (shft)))
#else
#define SHIFT_TEMPS
#define RIGHT_SHIFT(x,shft)	((x) >> (shft))
#endif

#define PASS1_BITS  2

#define ONE	((xaLONG) 1)

#define CONST_BITS 13

#define CONST_SCALE (ONE << CONST_BITS)

#define FIX(x)	((xaLONG) ((x) * CONST_SCALE + 0.5))

#define DESCALE(x,n)  RIGHT_SHIFT((x) + (ONE << ((n)-1)), n)

#define MULTIPLY(var,const)  ((var) * (const))

void j_rev_dct (xaSHORT *data,xaUBYTE *outptr,xaUBYTE *rnglimit)
{
  xaLONG tmp0, tmp1, tmp2, tmp3;
  xaLONG tmp10, tmp11, tmp12, tmp13;
  xaLONG z1, z2, z3, z4, z5;
  xaLONG d0, d1, d2, d3, d4, d5, d6, d7;
  register xaSHORT *dataptr;
  int rowctr;
  SHIFT_TEMPS
   
  /* Pass 1: process rows. */
  /* Note results are scaled up by sqrt(8) compared to a true IDCT; */
  /* furthermore, we scale the results by 2**PASS1_BITS. */

  dataptr = data;

  for (rowctr = DCTSIZE-1; rowctr >= 0; rowctr--) 
  {
    /* Due to quantization, we will usually find that many of the input
     * coefficients are zero, especially the AC terms.  We can exploit this
     * by short-circuiting the IDCT calculation for any row in which all
     * the AC terms are zero.  In that case each output is equal to the
     * DC coefficient (with scale factor as needed).
     * With typical images and quantization tables, half or more of the
     * row DCT calculations can be simplified this way.
     */

    register int *idataptr = (int*)dataptr;
    d0 = dataptr[0];
    d1 = dataptr[1];
    if ((d1 == 0) && (idataptr[1] | idataptr[2] | idataptr[3]) == 0) {
      /* AC terms all zero */
      if (d0) {
	  /* Compute a 32 bit value to assign. */
	  xaSHORT dcval = (xaSHORT) (d0 << PASS1_BITS);
	  register int v = (dcval & 0xffff) | ((dcval << 16) & 0xffff0000);
	  
	  idataptr[0] = v;
	  idataptr[1] = v;
	  idataptr[2] = v;
	  idataptr[3] = v;
      }
      
      dataptr += DCTSIZE;	/* advance pointer to next row */
      continue;
    }
    d2 = dataptr[2];
    d3 = dataptr[3];
    d4 = dataptr[4];
    d5 = dataptr[5];
    d6 = dataptr[6];
    d7 = dataptr[7];

    /* Even part: reverse the even part of the forward DCT. */
    /* The rotator is sqrt(2)*c(-6). */
    if (d6) {
	if (d4) {
	    if (d2) {
		if (d0) {
		    /* d0 != 0, d2 != 0, d4 != 0, d6 != 0 */
		    z1 = MULTIPLY(d2 + d6, FIX(0.541196100));
		    tmp2 = z1 + MULTIPLY(d6, - FIX(1.847759065));
		    tmp3 = z1 + MULTIPLY(d2, FIX(0.765366865));

		    tmp0 = (d0 + d4) << CONST_BITS;
		    tmp1 = (d0 - d4) << CONST_BITS;

		    tmp10 = tmp0 + tmp3;
		    tmp13 = tmp0 - tmp3;
		    tmp11 = tmp1 + tmp2;
		    tmp12 = tmp1 - tmp2;
		} else {
		    /* d0 == 0, d2 != 0, d4 != 0, d6 != 0 */
		    z1 = MULTIPLY(d2 + d6, FIX(0.541196100));
		    tmp2 = z1 + MULTIPLY(d6, - FIX(1.847759065));
		    tmp3 = z1 + MULTIPLY(d2, FIX(0.765366865));

		    tmp0 = d4 << CONST_BITS;

		    tmp10 = tmp0 + tmp3;
		    tmp13 = tmp0 - tmp3;
		    tmp11 = tmp2 - tmp0;
		    tmp12 = -(tmp0 + tmp2);
		}
	    } else {
		if (d0) {
		    /* d0 != 0, d2 == 0, d4 != 0, d6 != 0 */
		    tmp2 = MULTIPLY(d6, - FIX(1.306562965));
		    tmp3 = MULTIPLY(d6, FIX(0.541196100));

		    tmp0 = (d0 + d4) << CONST_BITS;
		    tmp1 = (d0 - d4) << CONST_BITS;

		    tmp10 = tmp0 + tmp3;
		    tmp13 = tmp0 - tmp3;
		    tmp11 = tmp1 + tmp2;
		    tmp12 = tmp1 - tmp2;
		} else {
		    /* d0 == 0, d2 == 0, d4 != 0, d6 != 0 */
		    tmp2 = MULTIPLY(d6, -FIX(1.306562965));
		    tmp3 = MULTIPLY(d6, FIX(0.541196100));

		    tmp0 = d4 << CONST_BITS;

		    tmp10 = tmp0 + tmp3;
		    tmp13 = tmp0 - tmp3;
		    tmp11 = tmp2 - tmp0;
		    tmp12 = -(tmp0 + tmp2);
		}
	    }
	} else {
	    if (d2) {
		if (d0) {
		    /* d0 != 0, d2 != 0, d4 == 0, d6 != 0 */
		    z1 = MULTIPLY(d2 + d6, FIX(0.541196100));
		    tmp2 = z1 + MULTIPLY(d6, - FIX(1.847759065));
		    tmp3 = z1 + MULTIPLY(d2, FIX(0.765366865));

		    tmp0 = d0 << CONST_BITS;

		    tmp10 = tmp0 + tmp3;
		    tmp13 = tmp0 - tmp3;
		    tmp11 = tmp0 + tmp2;
		    tmp12 = tmp0 - tmp2;
		} else {
		    /* d0 == 0, d2 != 0, d4 == 0, d6 != 0 */
		    z1 = MULTIPLY(d2 + d6, FIX(0.541196100));
		    tmp2 = z1 + MULTIPLY(d6, - FIX(1.847759065));
		    tmp3 = z1 + MULTIPLY(d2, FIX(0.765366865));

		    tmp10 = tmp3;
		    tmp13 = -tmp3;
		    tmp11 = tmp2;
		    tmp12 = -tmp2;
		}
	    } else {
		if (d0) {
		    /* d0 != 0, d2 == 0, d4 == 0, d6 != 0 */
		    tmp2 = MULTIPLY(d6, - FIX(1.306562965));
		    tmp3 = MULTIPLY(d6, FIX(0.541196100));

		    tmp0 = d0 << CONST_BITS;

		    tmp10 = tmp0 + tmp3;
		    tmp13 = tmp0 - tmp3;
		    tmp11 = tmp0 + tmp2;
		    tmp12 = tmp0 - tmp2;
		} else {
		    /* d0 == 0, d2 == 0, d4 == 0, d6 != 0 */
		    tmp2 = MULTIPLY(d6, - FIX(1.306562965));
		    tmp3 = MULTIPLY(d6, FIX(0.541196100));

		    tmp10 = tmp3;
		    tmp13 = -tmp3;
		    tmp11 = tmp2;
		    tmp12 = -tmp2;
		}
	    }
	}
    } else {
	if (d4) {
	    if (d2) {
		if (d0) {
		    /* d0 != 0, d2 != 0, d4 != 0, d6 == 0 */
		    tmp2 = MULTIPLY(d2, FIX(0.541196100));
		    tmp3 = MULTIPLY(d2, FIX(1.306562965));

		    tmp0 = (d0 + d4) << CONST_BITS;
		    tmp1 = (d0 - d4) << CONST_BITS;

		    tmp10 = tmp0 + tmp3;
		    tmp13 = tmp0 - tmp3;
		    tmp11 = tmp1 + tmp2;
		    tmp12 = tmp1 - tmp2;
		} else {
		    /* d0 == 0, d2 != 0, d4 != 0, d6 == 0 */
		    tmp2 = MULTIPLY(d2, FIX(0.541196100));
		    tmp3 = MULTIPLY(d2, FIX(1.306562965));

		    tmp0 = d4 << CONST_BITS;

		    tmp10 = tmp0 + tmp3;
		    tmp13 = tmp0 - tmp3;
		    tmp11 = tmp2 - tmp0;
		    tmp12 = -(tmp0 + tmp2);
		}
	    } else {
		if (d0) {
		    /* d0 != 0, d2 == 0, d4 != 0, d6 == 0 */
		    tmp10 = tmp13 = (d0 + d4) << CONST_BITS;
		    tmp11 = tmp12 = (d0 - d4) << CONST_BITS;
		} else {
		    /* d0 == 0, d2 == 0, d4 != 0, d6 == 0 */
		    tmp10 = tmp13 = d4 << CONST_BITS;
		    tmp11 = tmp12 = -tmp10;
		}
	    }
	} else {
	    if (d2) {
		if (d0) {
		    /* d0 != 0, d2 != 0, d4 == 0, d6 == 0 */
		    tmp2 = MULTIPLY(d2, FIX(0.541196100));
		    tmp3 = MULTIPLY(d2, FIX(1.306562965));

		    tmp0 = d0 << CONST_BITS;

		    tmp10 = tmp0 + tmp3;
		    tmp13 = tmp0 - tmp3;
		    tmp11 = tmp0 + tmp2;
		    tmp12 = tmp0 - tmp2;
		} else {
		    /* d0 == 0, d2 != 0, d4 == 0, d6 == 0 */
		    tmp2 = MULTIPLY(d2, FIX(0.541196100));
		    tmp3 = MULTIPLY(d2, FIX(1.306562965));

		    tmp10 = tmp3;
		    tmp13 = -tmp3;
		    tmp11 = tmp2;
		    tmp12 = -tmp2;
		}
	    } else {
		if (d0) {
		    /* d0 != 0, d2 == 0, d4 == 0, d6 == 0 */
		    tmp10 = tmp13 = tmp11 = tmp12 = d0 << CONST_BITS;
		} else {
		    /* d0 == 0, d2 == 0, d4 == 0, d6 == 0 */
		    tmp10 = tmp13 = tmp11 = tmp12 = 0;
		}
	    }
	}
    }


    /* Odd part per figure 8; the matrix is unitary and hence its
     * transpose is its inverse.  i0..i3 are y7,y5,y3,y1 respectively.
     */

    if (d7) {
	if (d5) {
	    if (d3) {
		if (d1) {
		    /* d1 != 0, d3 != 0, d5 != 0, d7 != 0 */
		    z1 = d7 + d1;
		    z2 = d5 + d3;
		    z3 = d7 + d3;
		    z4 = d5 + d1;
		    z5 = MULTIPLY(z3 + z4, FIX(1.175875602));
		    
		    tmp0 = MULTIPLY(d7, FIX(0.298631336)); 
		    tmp1 = MULTIPLY(d5, FIX(2.053119869));
		    tmp2 = MULTIPLY(d3, FIX(3.072711026));
		    tmp3 = MULTIPLY(d1, FIX(1.501321110));
		    z1 = MULTIPLY(z1, - FIX(0.899976223));
		    z2 = MULTIPLY(z2, - FIX(2.562915447));
		    z3 = MULTIPLY(z3, - FIX(1.961570560));
		    z4 = MULTIPLY(z4, - FIX(0.390180644));
		    
		    z3 += z5;
		    z4 += z5;
		    
		    tmp0 += z1 + z3;
		    tmp1 += z2 + z4;
		    tmp2 += z2 + z3;
		    tmp3 += z1 + z4;
		} else {
		    /* d1 == 0, d3 != 0, d5 != 0, d7 != 0 */
		    z1 = d7;
		    z2 = d5 + d3;
		    z3 = d7 + d3;
		    z5 = MULTIPLY(z3 + d5, FIX(1.175875602));
		    
		    tmp0 = MULTIPLY(d7, FIX(0.298631336)); 
		    tmp1 = MULTIPLY(d5, FIX(2.053119869));
		    tmp2 = MULTIPLY(d3, FIX(3.072711026));
		    z1 = MULTIPLY(d7, - FIX(0.899976223));
		    z2 = MULTIPLY(z2, - FIX(2.562915447));
		    z3 = MULTIPLY(z3, - FIX(1.961570560));
		    z4 = MULTIPLY(d5, - FIX(0.390180644));
		    
		    z3 += z5;
		    z4 += z5;
		    
		    tmp0 += z1 + z3;
		    tmp1 += z2 + z4;
		    tmp2 += z2 + z3;
		    tmp3 = z1 + z4;
		}
	    } else {
		if (d1) {
		    /* d1 != 0, d3 == 0, d5 != 0, d7 != 0 */
		    z1 = d7 + d1;
		    z2 = d5;
		    z3 = d7;
		    z4 = d5 + d1;
		    z5 = MULTIPLY(z3 + z4, FIX(1.175875602));
		    
		    tmp0 = MULTIPLY(d7, FIX(0.298631336)); 
		    tmp1 = MULTIPLY(d5, FIX(2.053119869));
		    tmp3 = MULTIPLY(d1, FIX(1.501321110));
		    z1 = MULTIPLY(z1, - FIX(0.899976223));
		    z2 = MULTIPLY(d5, - FIX(2.562915447));
		    z3 = MULTIPLY(d7, - FIX(1.961570560));
		    z4 = MULTIPLY(z4, - FIX(0.390180644));
		    
		    z3 += z5;
		    z4 += z5;
		    
		    tmp0 += z1 + z3;
		    tmp1 += z2 + z4;
		    tmp2 = z2 + z3;
		    tmp3 += z1 + z4;
		} else {
		    /* d1 == 0, d3 == 0, d5 != 0, d7 != 0 */
		    tmp0 = MULTIPLY(d7, - FIX(0.601344887)); 
		    z1 = MULTIPLY(d7, - FIX(0.899976223));
		    z3 = MULTIPLY(d7, - FIX(1.961570560));
		    tmp1 = MULTIPLY(d5, - FIX(0.509795578));
		    z2 = MULTIPLY(d5, - FIX(2.562915447));
		    z4 = MULTIPLY(d5, - FIX(0.390180644));
		    z5 = MULTIPLY(d5 + d7, FIX(1.175875602));
		    
		    z3 += z5;
		    z4 += z5;
		    
		    tmp0 += z3;
		    tmp1 += z4;
		    tmp2 = z2 + z3;
		    tmp3 = z1 + z4;
		}
	    }
	} else {
	    if (d3) {
		if (d1) {
		    /* d1 != 0, d3 != 0, d5 == 0, d7 != 0 */
		    z1 = d7 + d1;
		    z3 = d7 + d3;
		    z5 = MULTIPLY(z3 + d1, FIX(1.175875602));
		    
		    tmp0 = MULTIPLY(d7, FIX(0.298631336)); 
		    tmp2 = MULTIPLY(d3, FIX(3.072711026));
		    tmp3 = MULTIPLY(d1, FIX(1.501321110));
		    z1 = MULTIPLY(z1, - FIX(0.899976223));
		    z2 = MULTIPLY(d3, - FIX(2.562915447));
		    z3 = MULTIPLY(z3, - FIX(1.961570560));
		    z4 = MULTIPLY(d1, - FIX(0.390180644));
		    
		    z3 += z5;
		    z4 += z5;
		    
		    tmp0 += z1 + z3;
		    tmp1 = z2 + z4;
		    tmp2 += z2 + z3;
		    tmp3 += z1 + z4;
		} else {
		    /* d1 == 0, d3 != 0, d5 == 0, d7 != 0 */
		    z3 = d7 + d3;
		    
		    tmp0 = MULTIPLY(d7, - FIX(0.601344887)); 
		    z1 = MULTIPLY(d7, - FIX(0.899976223));
		    tmp2 = MULTIPLY(d3, FIX(0.509795579));
		    z2 = MULTIPLY(d3, - FIX(2.562915447));
		    z5 = MULTIPLY(z3, FIX(1.175875602));
		    z3 = MULTIPLY(z3, - FIX(0.785694958));
		    
		    tmp0 += z3;
		    tmp1 = z2 + z5;
		    tmp2 += z3;
		    tmp3 = z1 + z5;
		}
	    } else {
		if (d1) {
		    /* d1 != 0, d3 == 0, d5 == 0, d7 != 0 */
		    z1 = d7 + d1;
		    z5 = MULTIPLY(z1, FIX(1.175875602));

		    z1 = MULTIPLY(z1, FIX(0.275899379));
		    z3 = MULTIPLY(d7, - FIX(1.961570560));
		    tmp0 = MULTIPLY(d7, - FIX(1.662939224)); 
		    z4 = MULTIPLY(d1, - FIX(0.390180644));
		    tmp3 = MULTIPLY(d1, FIX(1.111140466));

		    tmp0 += z1;
		    tmp1 = z4 + z5;
		    tmp2 = z3 + z5;
		    tmp3 += z1;
		} else {
		    /* d1 == 0, d3 == 0, d5 == 0, d7 != 0 */
		    tmp0 = MULTIPLY(d7, - FIX(1.387039845));
		    tmp1 = MULTIPLY(d7, FIX(1.175875602));
		    tmp2 = MULTIPLY(d7, - FIX(0.785694958));
		    tmp3 = MULTIPLY(d7, FIX(0.275899379));
		}
	    }
	}
    } else {
	if (d5) {
	    if (d3) {
		if (d1) {
		    /* d1 != 0, d3 != 0, d5 != 0, d7 == 0 */
		    z2 = d5 + d3;
		    z4 = d5 + d1;
		    z5 = MULTIPLY(d3 + z4, FIX(1.175875602));
		    
		    tmp1 = MULTIPLY(d5, FIX(2.053119869));
		    tmp2 = MULTIPLY(d3, FIX(3.072711026));
		    tmp3 = MULTIPLY(d1, FIX(1.501321110));
		    z1 = MULTIPLY(d1, - FIX(0.899976223));
		    z2 = MULTIPLY(z2, - FIX(2.562915447));
		    z3 = MULTIPLY(d3, - FIX(1.961570560));
		    z4 = MULTIPLY(z4, - FIX(0.390180644));
		    
		    z3 += z5;
		    z4 += z5;
		    
		    tmp0 = z1 + z3;
		    tmp1 += z2 + z4;
		    tmp2 += z2 + z3;
		    tmp3 += z1 + z4;
		} else {
		    /* d1 == 0, d3 != 0, d5 != 0, d7 == 0 */
		    z2 = d5 + d3;
		    
		    z5 = MULTIPLY(z2, FIX(1.175875602));
		    tmp1 = MULTIPLY(d5, FIX(1.662939225));
		    z4 = MULTIPLY(d5, - FIX(0.390180644));
		    z2 = MULTIPLY(z2, - FIX(1.387039845));
		    tmp2 = MULTIPLY(d3, FIX(1.111140466));
		    z3 = MULTIPLY(d3, - FIX(1.961570560));
		    
		    tmp0 = z3 + z5;
		    tmp1 += z2;
		    tmp2 += z2;
		    tmp3 = z4 + z5;
		}
	    } else {
		if (d1) {
		    /* d1 != 0, d3 == 0, d5 != 0, d7 == 0 */
		    z4 = d5 + d1;
		    
		    z5 = MULTIPLY(z4, FIX(1.175875602));
		    z1 = MULTIPLY(d1, - FIX(0.899976223));
		    tmp3 = MULTIPLY(d1, FIX(0.601344887));
		    tmp1 = MULTIPLY(d5, - FIX(0.509795578));
		    z2 = MULTIPLY(d5, - FIX(2.562915447));
		    z4 = MULTIPLY(z4, FIX(0.785694958));
		    
		    tmp0 = z1 + z5;
		    tmp1 += z4;
		    tmp2 = z2 + z5;
		    tmp3 += z4;
		} else {
		    /* d1 == 0, d3 == 0, d5 != 0, d7 == 0 */
		    tmp0 = MULTIPLY(d5, FIX(1.175875602));
		    tmp1 = MULTIPLY(d5, FIX(0.275899380));
		    tmp2 = MULTIPLY(d5, - FIX(1.387039845));
		    tmp3 = MULTIPLY(d5, FIX(0.785694958));
		}
	    }
	} else {
	    if (d3) {
		if (d1) {
		    /* d1 != 0, d3 != 0, d5 == 0, d7 == 0 */
		    z5 = d1 + d3;
		    tmp3 = MULTIPLY(d1, FIX(0.211164243));
		    tmp2 = MULTIPLY(d3, - FIX(1.451774981));
		    z1 = MULTIPLY(d1, FIX(1.061594337));
		    z2 = MULTIPLY(d3, - FIX(2.172734803));
		    z4 = MULTIPLY(z5, FIX(0.785694958));
		    z5 = MULTIPLY(z5, FIX(1.175875602));
		    
		    tmp0 = z1 - z4;
		    tmp1 = z2 + z4;
		    tmp2 += z5;
		    tmp3 += z5;
		} else {
		    /* d1 == 0, d3 != 0, d5 == 0, d7 == 0 */
		    tmp0 = MULTIPLY(d3, - FIX(0.785694958));
		    tmp1 = MULTIPLY(d3, - FIX(1.387039845));
		    tmp2 = MULTIPLY(d3, - FIX(0.275899379));
		    tmp3 = MULTIPLY(d3, FIX(1.175875602));
		}
	    } else {
		if (d1) {
		    /* d1 != 0, d3 == 0, d5 == 0, d7 == 0 */
		    tmp0 = MULTIPLY(d1, FIX(0.275899379));
		    tmp1 = MULTIPLY(d1, FIX(0.785694958));
		    tmp2 = MULTIPLY(d1, FIX(1.175875602));
		    tmp3 = MULTIPLY(d1, FIX(1.387039845));
		} else {
		    /* d1 == 0, d3 == 0, d5 == 0, d7 == 0 */
		    tmp0 = tmp1 = tmp2 = tmp3 = 0;
		}
	    }
	}
    }

    /* Final output stage: inputs are tmp10..tmp13, tmp0..tmp3 */

    dataptr[0] = (xaSHORT) DESCALE(tmp10 + tmp3, CONST_BITS-PASS1_BITS);
    dataptr[7] = (xaSHORT) DESCALE(tmp10 - tmp3, CONST_BITS-PASS1_BITS);
    dataptr[1] = (xaSHORT) DESCALE(tmp11 + tmp2, CONST_BITS-PASS1_BITS);
    dataptr[6] = (xaSHORT) DESCALE(tmp11 - tmp2, CONST_BITS-PASS1_BITS);
    dataptr[2] = (xaSHORT) DESCALE(tmp12 + tmp1, CONST_BITS-PASS1_BITS);
    dataptr[5] = (xaSHORT) DESCALE(tmp12 - tmp1, CONST_BITS-PASS1_BITS);
    dataptr[3] = (xaSHORT) DESCALE(tmp13 + tmp0, CONST_BITS-PASS1_BITS);
    dataptr[4] = (xaSHORT) DESCALE(tmp13 - tmp0, CONST_BITS-PASS1_BITS);

    dataptr += DCTSIZE;		/* advance pointer to next row */
  }

  /* Pass 2: process columns. */
  /* Note that we must descale the results by a factor of 8 == 2**3, */
  /* and also undo the PASS1_BITS scaling. */

  dataptr = data;
  for (rowctr = DCTSIZE-1; rowctr >= 0; rowctr--) 
  {
    /* Columns of zeroes can be exploited in the same way as we did with rows.
     * However, the row calculation has created many nonzero AC terms, so the
     * simplification applies less often (typically 5% to 10% of the time).
     * On machines with very fast multiplication, it's possible that the
     * test takes more time than it's worth.  In that case this section
     * may be commented out.
     */

    d0 = dataptr[DCTSIZE*0];
    d1 = dataptr[DCTSIZE*1];
    d2 = dataptr[DCTSIZE*2];
    d3 = dataptr[DCTSIZE*3];
    d4 = dataptr[DCTSIZE*4];
    d5 = dataptr[DCTSIZE*5];
    d6 = dataptr[DCTSIZE*6];
    d7 = dataptr[DCTSIZE*7];

    /* Even part: reverse the even part of the forward DCT. */
    /* The rotator is sqrt(2)*c(-6). */
    if (d6) {
	if (d4) {
	    if (d2) {
		if (d0) {
		    /* d0 != 0, d2 != 0, d4 != 0, d6 != 0 */
		    z1 = MULTIPLY(d2 + d6, FIX(0.541196100));
		    tmp2 = z1 + MULTIPLY(d6, - FIX(1.847759065));
		    tmp3 = z1 + MULTIPLY(d2, FIX(0.765366865));

		    tmp0 = (d0 + d4) << CONST_BITS;
		    tmp1 = (d0 - d4) << CONST_BITS;

		    tmp10 = tmp0 + tmp3;
		    tmp13 = tmp0 - tmp3;
		    tmp11 = tmp1 + tmp2;
		    tmp12 = tmp1 - tmp2;
		} else {
		    /* d0 == 0, d2 != 0, d4 != 0, d6 != 0 */
		    z1 = MULTIPLY(d2 + d6, FIX(0.541196100));
		    tmp2 = z1 + MULTIPLY(d6, - FIX(1.847759065));
		    tmp3 = z1 + MULTIPLY(d2, FIX(0.765366865));

		    tmp0 = d4 << CONST_BITS;

		    tmp10 = tmp0 + tmp3;
		    tmp13 = tmp0 - tmp3;
		    tmp11 = tmp2 - tmp0;
		    tmp12 = -(tmp0 + tmp2);
		}
	    } else {
		if (d0) {
		    /* d0 != 0, d2 == 0, d4 != 0, d6 != 0 */
		    tmp2 = MULTIPLY(d6, - FIX(1.306562965));
		    tmp3 = MULTIPLY(d6, FIX(0.541196100));

		    tmp0 = (d0 + d4) << CONST_BITS;
		    tmp1 = (d0 - d4) << CONST_BITS;

		    tmp10 = tmp0 + tmp3;
		    tmp13 = tmp0 - tmp3;
		    tmp11 = tmp1 + tmp2;
		    tmp12 = tmp1 - tmp2;
		} else {
		    /* d0 == 0, d2 == 0, d4 != 0, d6 != 0 */
		    tmp2 = MULTIPLY(d6, -FIX(1.306562965));
		    tmp3 = MULTIPLY(d6, FIX(0.541196100));

		    tmp0 = d4 << CONST_BITS;

		    tmp10 = tmp0 + tmp3;
		    tmp13 = tmp0 - tmp3;
		    tmp11 = tmp2 - tmp0;
		    tmp12 = -(tmp0 + tmp2);
		}
	    }
	} else {
	    if (d2) {
		if (d0) {
		    /* d0 != 0, d2 != 0, d4 == 0, d6 != 0 */
		    z1 = MULTIPLY(d2 + d6, FIX(0.541196100));
		    tmp2 = z1 + MULTIPLY(d6, - FIX(1.847759065));
		    tmp3 = z1 + MULTIPLY(d2, FIX(0.765366865));

		    tmp0 = d0 << CONST_BITS;

		    tmp10 = tmp0 + tmp3;
		    tmp13 = tmp0 - tmp3;
		    tmp11 = tmp0 + tmp2;
		    tmp12 = tmp0 - tmp2;
		} else {
		    /* d0 == 0, d2 != 0, d4 == 0, d6 != 0 */
		    z1 = MULTIPLY(d2 + d6, FIX(0.541196100));
		    tmp2 = z1 + MULTIPLY(d6, - FIX(1.847759065));
		    tmp3 = z1 + MULTIPLY(d2, FIX(0.765366865));

		    tmp10 = tmp3;
		    tmp13 = -tmp3;
		    tmp11 = tmp2;
		    tmp12 = -tmp2;
		}
	    } else {
		if (d0) {
		    /* d0 != 0, d2 == 0, d4 == 0, d6 != 0 */
		    tmp2 = MULTIPLY(d6, - FIX(1.306562965));
		    tmp3 = MULTIPLY(d6, FIX(0.541196100));

		    tmp0 = d0 << CONST_BITS;

		    tmp10 = tmp0 + tmp3;
		    tmp13 = tmp0 - tmp3;
		    tmp11 = tmp0 + tmp2;
		    tmp12 = tmp0 - tmp2;
		} else {
		    /* d0 == 0, d2 == 0, d4 == 0, d6 != 0 */
		    tmp2 = MULTIPLY(d6, - FIX(1.306562965));
		    tmp3 = MULTIPLY(d6, FIX(0.541196100));

		    tmp10 = tmp3;
		    tmp13 = -tmp3;
		    tmp11 = tmp2;
		    tmp12 = -tmp2;
		}
	    }
	}
    } else {
	if (d4) {
	    if (d2) {
		if (d0) {
		    /* d0 != 0, d2 != 0, d4 != 0, d6 == 0 */
		    tmp2 = MULTIPLY(d2, FIX(0.541196100));
		    tmp3 = MULTIPLY(d2, FIX(1.306562965));

		    tmp0 = (d0 + d4) << CONST_BITS;
		    tmp1 = (d0 - d4) << CONST_BITS;

		    tmp10 = tmp0 + tmp3;
		    tmp13 = tmp0 - tmp3;
		    tmp11 = tmp1 + tmp2;
		    tmp12 = tmp1 - tmp2;
		} else {
		    /* d0 == 0, d2 != 0, d4 != 0, d6 == 0 */
		    tmp2 = MULTIPLY(d2, FIX(0.541196100));
		    tmp3 = MULTIPLY(d2, FIX(1.306562965));

		    tmp0 = d4 << CONST_BITS;

		    tmp10 = tmp0 + tmp3;
		    tmp13 = tmp0 - tmp3;
		    tmp11 = tmp2 - tmp0;
		    tmp12 = -(tmp0 + tmp2);
		}
	    } else {
		if (d0) {
		    /* d0 != 0, d2 == 0, d4 != 0, d6 == 0 */
		    tmp10 = tmp13 = (d0 + d4) << CONST_BITS;
		    tmp11 = tmp12 = (d0 - d4) << CONST_BITS;
		} else {
		    /* d0 == 0, d2 == 0, d4 != 0, d6 == 0 */
		    tmp10 = tmp13 = d4 << CONST_BITS;
		    tmp11 = tmp12 = -tmp10;
		}
	    }
	} else {
	    if (d2) {
		if (d0) {
		    /* d0 != 0, d2 != 0, d4 == 0, d6 == 0 */
		    tmp2 = MULTIPLY(d2, FIX(0.541196100));
		    tmp3 = MULTIPLY(d2, FIX(1.306562965));

		    tmp0 = d0 << CONST_BITS;

		    tmp10 = tmp0 + tmp3;
		    tmp13 = tmp0 - tmp3;
		    tmp11 = tmp0 + tmp2;
		    tmp12 = tmp0 - tmp2;
		} else {
		    /* d0 == 0, d2 != 0, d4 == 0, d6 == 0 */
		    tmp2 = MULTIPLY(d2, FIX(0.541196100));
		    tmp3 = MULTIPLY(d2, FIX(1.306562965));

		    tmp10 = tmp3;
		    tmp13 = -tmp3;
		    tmp11 = tmp2;
		    tmp12 = -tmp2;
		}
	    } else {
		if (d0) {
		    /* d0 != 0, d2 == 0, d4 == 0, d6 == 0 */
		    tmp10 = tmp13 = tmp11 = tmp12 = d0 << CONST_BITS;
		} else {
		    /* d0 == 0, d2 == 0, d4 == 0, d6 == 0 */
		    tmp10 = tmp13 = tmp11 = tmp12 = 0;
		}
	    }
	}
    }

    /* Odd part per figure 8; the matrix is unitary and hence its
     * transpose is its inverse.  i0..i3 are y7,y5,y3,y1 respectively.
     */
    if (d7) {
	if (d5) {
	    if (d3) {
		if (d1) {
		    /* d1 != 0, d3 != 0, d5 != 0, d7 != 0 */
		    z1 = d7 + d1;
		    z2 = d5 + d3;
		    z3 = d7 + d3;
		    z4 = d5 + d1;
		    z5 = MULTIPLY(z3 + z4, FIX(1.175875602));
		    
		    tmp0 = MULTIPLY(d7, FIX(0.298631336)); 
		    tmp1 = MULTIPLY(d5, FIX(2.053119869));
		    tmp2 = MULTIPLY(d3, FIX(3.072711026));
		    tmp3 = MULTIPLY(d1, FIX(1.501321110));
		    z1 = MULTIPLY(z1, - FIX(0.899976223));
		    z2 = MULTIPLY(z2, - FIX(2.562915447));
		    z3 = MULTIPLY(z3, - FIX(1.961570560));
		    z4 = MULTIPLY(z4, - FIX(0.390180644));
		    
		    z3 += z5;
		    z4 += z5;
		    
		    tmp0 += z1 + z3;
		    tmp1 += z2 + z4;
		    tmp2 += z2 + z3;
		    tmp3 += z1 + z4;
		} else {
		    /* d1 == 0, d3 != 0, d5 != 0, d7 != 0 */
		    z1 = d7;
		    z2 = d5 + d3;
		    z3 = d7 + d3;
		    z5 = MULTIPLY(z3 + d5, FIX(1.175875602));
		    
		    tmp0 = MULTIPLY(d7, FIX(0.298631336)); 
		    tmp1 = MULTIPLY(d5, FIX(2.053119869));
		    tmp2 = MULTIPLY(d3, FIX(3.072711026));
		    z1 = MULTIPLY(d7, - FIX(0.899976223));
		    z2 = MULTIPLY(z2, - FIX(2.562915447));
		    z3 = MULTIPLY(z3, - FIX(1.961570560));
		    z4 = MULTIPLY(d5, - FIX(0.390180644));
		    
		    z3 += z5;
		    z4 += z5;
		    
		    tmp0 += z1 + z3;
		    tmp1 += z2 + z4;
		    tmp2 += z2 + z3;
		    tmp3 = z1 + z4;
		}
	    } else {
		if (d1) {
		    /* d1 != 0, d3 == 0, d5 != 0, d7 != 0 */
		    z1 = d7 + d1;
		    z2 = d5;
		    z3 = d7;
		    z4 = d5 + d1;
		    z5 = MULTIPLY(z3 + z4, FIX(1.175875602));
		    
		    tmp0 = MULTIPLY(d7, FIX(0.298631336)); 
		    tmp1 = MULTIPLY(d5, FIX(2.053119869));
		    tmp3 = MULTIPLY(d1, FIX(1.501321110));
		    z1 = MULTIPLY(z1, - FIX(0.899976223));
		    z2 = MULTIPLY(d5, - FIX(2.562915447));
		    z3 = MULTIPLY(d7, - FIX(1.961570560));
		    z4 = MULTIPLY(z4, - FIX(0.390180644));
		    
		    z3 += z5;
		    z4 += z5;
		    
		    tmp0 += z1 + z3;
		    tmp1 += z2 + z4;
		    tmp2 = z2 + z3;
		    tmp3 += z1 + z4;
		} else {
		    /* d1 == 0, d3 == 0, d5 != 0, d7 != 0 */
		    tmp0 = MULTIPLY(d7, - FIX(0.601344887)); 
		    z1 = MULTIPLY(d7, - FIX(0.899976223));
		    z3 = MULTIPLY(d7, - FIX(1.961570560));
		    tmp1 = MULTIPLY(d5, - FIX(0.509795578));
		    z2 = MULTIPLY(d5, - FIX(2.562915447));
		    z4 = MULTIPLY(d5, - FIX(0.390180644));
		    z5 = MULTIPLY(d5 + d7, FIX(1.175875602));
		    
		    z3 += z5;
		    z4 += z5;
		    
		    tmp0 += z3;
		    tmp1 += z4;
		    tmp2 = z2 + z3;
		    tmp3 = z1 + z4;
		}
	    }
	} else {
	    if (d3) {
		if (d1) {
		    /* d1 != 0, d3 != 0, d5 == 0, d7 != 0 */
		    z1 = d7 + d1;
		    z3 = d7 + d3;
		    z5 = MULTIPLY(z3 + d1, FIX(1.175875602));
		    
		    tmp0 = MULTIPLY(d7, FIX(0.298631336)); 
		    tmp2 = MULTIPLY(d3, FIX(3.072711026));
		    tmp3 = MULTIPLY(d1, FIX(1.501321110));
		    z1 = MULTIPLY(z1, - FIX(0.899976223));
		    z2 = MULTIPLY(d3, - FIX(2.562915447));
		    z3 = MULTIPLY(z3, - FIX(1.961570560));
		    z4 = MULTIPLY(d1, - FIX(0.390180644));
		    
		    z3 += z5;
		    z4 += z5;
		    
		    tmp0 += z1 + z3;
		    tmp1 = z2 + z4;
		    tmp2 += z2 + z3;
		    tmp3 += z1 + z4;
		} else {
		    /* d1 == 0, d3 != 0, d5 == 0, d7 != 0 */
		    z3 = d7 + d3;
		    
		    tmp0 = MULTIPLY(d7, - FIX(0.601344887)); 
		    z1 = MULTIPLY(d7, - FIX(0.899976223));
		    tmp2 = MULTIPLY(d3, FIX(0.509795579));
		    z2 = MULTIPLY(d3, - FIX(2.562915447));
		    z5 = MULTIPLY(z3, FIX(1.175875602));
		    z3 = MULTIPLY(z3, - FIX(0.785694958));
		    
		    tmp0 += z3;
		    tmp1 = z2 + z5;
		    tmp2 += z3;
		    tmp3 = z1 + z5;
		}
	    } else {
		if (d1) {
		    /* d1 != 0, d3 == 0, d5 == 0, d7 != 0 */
		    z1 = d7 + d1;
		    z5 = MULTIPLY(z1, FIX(1.175875602));

		    z1 = MULTIPLY(z1, FIX(0.275899379));
		    z3 = MULTIPLY(d7, - FIX(1.961570560));
		    tmp0 = MULTIPLY(d7, - FIX(1.662939224)); 
		    z4 = MULTIPLY(d1, - FIX(0.390180644));
		    tmp3 = MULTIPLY(d1, FIX(1.111140466));

		    tmp0 += z1;
		    tmp1 = z4 + z5;
		    tmp2 = z3 + z5;
		    tmp3 += z1;
		} else {
		    /* d1 == 0, d3 == 0, d5 == 0, d7 != 0 */
		    tmp0 = MULTIPLY(d7, - FIX(1.387039845));
		    tmp1 = MULTIPLY(d7, FIX(1.175875602));
		    tmp2 = MULTIPLY(d7, - FIX(0.785694958));
		    tmp3 = MULTIPLY(d7, FIX(0.275899379));
		}
	    }
	}
    } else {
	if (d5) {
	    if (d3) {
		if (d1) {
		    /* d1 != 0, d3 != 0, d5 != 0, d7 == 0 */
		    z2 = d5 + d3;
		    z4 = d5 + d1;
		    z5 = MULTIPLY(d3 + z4, FIX(1.175875602));
		    
		    tmp1 = MULTIPLY(d5, FIX(2.053119869));
		    tmp2 = MULTIPLY(d3, FIX(3.072711026));
		    tmp3 = MULTIPLY(d1, FIX(1.501321110));
		    z1 = MULTIPLY(d1, - FIX(0.899976223));
		    z2 = MULTIPLY(z2, - FIX(2.562915447));
		    z3 = MULTIPLY(d3, - FIX(1.961570560));
		    z4 = MULTIPLY(z4, - FIX(0.390180644));
		    
		    z3 += z5;
		    z4 += z5;
		    
		    tmp0 = z1 + z3;
		    tmp1 += z2 + z4;
		    tmp2 += z2 + z3;
		    tmp3 += z1 + z4;
		} else {
		    /* d1 == 0, d3 != 0, d5 != 0, d7 == 0 */
		    z2 = d5 + d3;
		    
		    z5 = MULTIPLY(z2, FIX(1.175875602));
		    tmp1 = MULTIPLY(d5, FIX(1.662939225));
		    z4 = MULTIPLY(d5, - FIX(0.390180644));
		    z2 = MULTIPLY(z2, - FIX(1.387039845));
		    tmp2 = MULTIPLY(d3, FIX(1.111140466));
		    z3 = MULTIPLY(d3, - FIX(1.961570560));
		    
		    tmp0 = z3 + z5;
		    tmp1 += z2;
		    tmp2 += z2;
		    tmp3 = z4 + z5;
		}
	    } else {
		if (d1) {
		    /* d1 != 0, d3 == 0, d5 != 0, d7 == 0 */
		    z4 = d5 + d1;
		    
		    z5 = MULTIPLY(z4, FIX(1.175875602));
		    z1 = MULTIPLY(d1, - FIX(0.899976223));
		    tmp3 = MULTIPLY(d1, FIX(0.601344887));
		    tmp1 = MULTIPLY(d5, - FIX(0.509795578));
		    z2 = MULTIPLY(d5, - FIX(2.562915447));
		    z4 = MULTIPLY(z4, FIX(0.785694958));
		    
		    tmp0 = z1 + z5;
		    tmp1 += z4;
		    tmp2 = z2 + z5;
		    tmp3 += z4;
		} else {
		    /* d1 == 0, d3 == 0, d5 != 0, d7 == 0 */
		    tmp0 = MULTIPLY(d5, FIX(1.175875602));
		    tmp1 = MULTIPLY(d5, FIX(0.275899380));
		    tmp2 = MULTIPLY(d5, - FIX(1.387039845));
		    tmp3 = MULTIPLY(d5, FIX(0.785694958));
		}
	    }
	} else {
	    if (d3) {
		if (d1) {
		    /* d1 != 0, d3 != 0, d5 == 0, d7 == 0 */
		    z5 = d1 + d3;
		    tmp3 = MULTIPLY(d1, FIX(0.211164243));
		    tmp2 = MULTIPLY(d3, - FIX(1.451774981));
		    z1 = MULTIPLY(d1, FIX(1.061594337));
		    z2 = MULTIPLY(d3, - FIX(2.172734803));
		    z4 = MULTIPLY(z5, FIX(0.785694958));
		    z5 = MULTIPLY(z5, FIX(1.175875602));
		    
		    tmp0 = z1 - z4;
		    tmp1 = z2 + z4;
		    tmp2 += z5;
		    tmp3 += z5;
		} else {
		    /* d1 == 0, d3 != 0, d5 == 0, d7 == 0 */
		    tmp0 = MULTIPLY(d3, - FIX(0.785694958));
		    tmp1 = MULTIPLY(d3, - FIX(1.387039845));
		    tmp2 = MULTIPLY(d3, - FIX(0.275899379));
		    tmp3 = MULTIPLY(d3, FIX(1.175875602));
		}
	    } else {
		if (d1) {
		    /* d1 != 0, d3 == 0, d5 == 0, d7 == 0 */
		    tmp0 = MULTIPLY(d1, FIX(0.275899379));
		    tmp1 = MULTIPLY(d1, FIX(0.785694958));
		    tmp2 = MULTIPLY(d1, FIX(1.175875602));
		    tmp3 = MULTIPLY(d1, FIX(1.387039845));
		} else {
		    /* d1 == 0, d3 == 0, d5 == 0, d7 == 0 */
		    tmp0 = tmp1 = tmp2 = tmp3 = 0;
		}
	    }
	}
    }

    /* Final output stage: inputs are tmp10..tmp13, tmp0..tmp3 */


    outptr[DCTSIZE*0] = rnglimit[ (int) DESCALE(tmp10 + tmp3,
				   CONST_BITS+PASS1_BITS+3) & RANGE_MASK];
    outptr[DCTSIZE*7] = rnglimit[ (int) DESCALE(tmp10 - tmp3,
				   CONST_BITS+PASS1_BITS+3) & RANGE_MASK];
    outptr[DCTSIZE*1] = rnglimit[ (int) DESCALE(tmp11 + tmp2,
				   CONST_BITS+PASS1_BITS+3) & RANGE_MASK];
    outptr[DCTSIZE*6] = rnglimit[ (int) DESCALE(tmp11 - tmp2,
				   CONST_BITS+PASS1_BITS+3) & RANGE_MASK];
    outptr[DCTSIZE*2] = rnglimit[ (int) DESCALE(tmp12 + tmp1,
				   CONST_BITS+PASS1_BITS+3) & RANGE_MASK];
    outptr[DCTSIZE*5] = rnglimit[ (int) DESCALE(tmp12 - tmp1,
				   CONST_BITS+PASS1_BITS+3) & RANGE_MASK];
    outptr[DCTSIZE*3] = rnglimit[ (int) DESCALE(tmp13 + tmp0,
				   CONST_BITS+PASS1_BITS+3) & RANGE_MASK];
    outptr[DCTSIZE*4] = rnglimit[ (int) DESCALE(tmp13 - tmp0,
				   CONST_BITS+PASS1_BITS+3) & RANGE_MASK];
    
    dataptr++;			/* advance pointer to next column */
    outptr++;
  }
}


/************************
 *  Find next MPEG start code. If buf is non-zero, get data from
 *  buf. Else use the file pointer, xin.
 *
 ****/
static xaLONG mpg_buf_get_start_code()
{ xaLONG not_beof = xaTRUE;
  xaULONG state = 0;

  MPG_ALIGN_BBUF();
  while(not_beof == xaTRUE)
  { xaLONG d;
    MPG_CHECK_BBITS(8,not_beof);
    if (not_beof == xaFALSE) break;
    MPG_GET_BBITS(8,d); /* Get 8 bits */
    if (state == 3)
    { 
      return(d);
    }
    else if (state == 2)
    { if (d == 0x01) state = 3;
      else if (d == 0x00) state = 2;
      else state = 0;
    }
    else
    { if (d == 0x00) state++;
      else state = 0;
    }
  }
  return((xaLONG)(-1));
}



/* my shifted version for AVI XMPG */
static xaUBYTE xmpg_def_intra_qtab[64] = {
         8,16,19,22,22,26,26,27,
        16,16,22,22,26,27,27,29,
        19,22,26,26,27,29,29,35,
        22,24,27,27,29,32,34,38,
        26,27,29,29,32,35,38,46,
        27,29,34,34,35,40,46,56,
        29,34,34,37,40,48,56,69,
        34,37,38,40,48,58,69,83 };

/*******
 *
 */
xaULONG
MPG_Decode_FULLI(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG special = dec_info->special;
  xaULONG extra = (xaULONG)dec_info->extra;
  XA_CHDR *chdr = dec_info->chdr;
  xaULONG mb_addr,vert_pos,q_scale;
  xaLONG mcu_cols,mcu_rows;
  xaUBYTE *qtab;
  xaUBYTE *Ybuf,*Ubuf,*Vbuf;
  xaULONG orow_size;
  xaULONG special_flag;
  xaUBYTE *iptr = image;
  void (*color_func)();
  xaULONG mb_size;

/* TODO
  MPG_PIC_HDR *phdr = (MPG_PIC_HDR *)(delta);
  MPG_SEQ_HDR *shdr =  phdr->seq_hdr;
  MPG_SLICE_HDR *slice;
*/
  MPG_PIC_HDR phdr;

   /* always full image for now */
  dec_info->xs = dec_info->ys = 0;
  dec_info->xe = imagex; dec_info->ye = imagey; 

   /* Indicate we can drop these frames */
  if ((delta == 0) || (dec_info->skip_flag > 0)) return(ACT_DLTA_DROP);
   
  if (chdr) { if (chdr->new_chdr) chdr = chdr->new_chdr; }

  special_flag = special & 0x0001;
  if (cmap_color_func == 4) { MPG_CMAP_CACHE_CHECK(chdr); }

  orow_size = imagex;
	/*** Setup Color Decode Functions */
  if (special_flag) { orow_size *= 3;	color_func = XA_MCU221111_To_RGB; }
  else
  { orow_size *= x11_bytes_pixel;
    if (x11_bytes_pixel==1)		
    { 
      color_func = XA_MCU221111_To_CLR8;
      if ( (chdr) && (x11_display_type == XA_PSEUDOCOLOR))
      {
	if (cmap_color_func == 4)        color_func = XA_MCU221111_To_CF4;
	else if ( (cmap_true_to_332 == xaTRUE) && (x11_cmap_size == 256) )
        if (xa_dither_flag==xaTRUE)    color_func = XA_MCU221111_To_332_Dither;
        else                           color_func = XA_MCU221111_To_332;
      }
    }
    else if (x11_bytes_pixel==2)	color_func = XA_MCU221111_To_CLR16;
    else				color_func = XA_MCU221111_To_CLR32;
  }

  mcu_cols = ((imagex + 15) / 16);
  mcu_rows = ((imagey + 15) / 16);
  mb_size = mcu_cols * mcu_rows;
  DEBUG_LEVEL1 fprintf(stderr,"mcu xy %d %d  size %d\n",
					mcu_cols,mcu_rows,mb_size);

	/* This is done because of 221111 */
  imagex++; imagex >>= 1;


  qtab = xmpg_def_intra_qtab;

  MPG_INIT_BBUF(delta,dsize);

  /* Check for Broadway MPEG encoding(BW10)
   * Skip 12 bytes and then expect GOP
   */
  if (extra & 0x01) 
  { MPG_FLUSH_BBITS(32);
    MPG_FLUSH_BBITS(32);
    MPG_FLUSH_BBITS(32);
    qtab = xmpg_def_intra_qtab;
  }
  else if (extra & 0x02)  /* XING Editable MPEG */
  {
    qtab = xmpg_def_intra_qtab;
  }

  vert_pos = -1;

  while(1)
  { xaLONG ret;
    xaLONG start_code = mpg_buf_get_start_code();

    if (start_code == MPG_GOP_START)
    {
      DEBUG_LEVEL1 fprintf(stderr,"MPG_FULLI: GOP Header found\n");
      mpg_read_GOP_HDR(MPG_FROM_BUFF);
      continue;
    }
    else if (start_code == MPG_PIC_START)
    {
      DEBUG_LEVEL1 fprintf(stderr,"MPG_FULLI: PIC START found\n");
      ret = mpg_read_PIC_HDR(&phdr,MPG_FROM_BUFF);
      if (ret == xaFALSE)
      {
        DEBUG_LEVEL1 fprintf(stderr,"MPG_FULLI: PIC START error\n");
        return(ACT_DLTA_DROP);
      }
      continue;
    }
    else if (start_code == MPG_USR_START)
    {
      DEBUG_LEVEL1 fprintf(stderr,"MPG_FULLI: USR Start found\n");
      continue;
    }
    else if (start_code < 0)
    {
      DEBUG_LEVEL1 fprintf(stderr,"MPG_FULLI: Reached EOF\n");
      break;
    }
    else if (   (start_code >= MPG_MIN_SLICE) 
             && (start_code <= MPG_MAX_SLICE) )
    {
      DEBUG_LEVEL1 fprintf(stderr,"MPG_FULLI: Found Slice %d\n", (start_code & 0xff) );
      vert_pos = (start_code & 0xff) - 1;

      MPG_GET_BBITS(5,q_scale);  /* quant scale */
      mpg_skip_extra_bitsB();

      DEBUG_LEVEL1
      { if (vert_pos >= 0)
		fprintf(stderr,"VERT_POS = %d mcu rows = %d cols = %d\n",
					vert_pos,mcu_rows,mcu_cols);
      }

      /* adjust pointers for slice */
      mb_addr = (vert_pos * mcu_cols) - 1;
      mpg_dc_y = mpg_dc_cr = mpg_dc_cb = 1024;

      do  /* while(mb_addr < mb_size) */
      { xaULONG tmp,i;

	/* parse MB addr increment and update MB address */
        DEBUG_LEVEL1 fprintf(stderr,"PARSE MB ADDR(%d): ",mb_addr);

        { xaULONG next;
	  MPG_NXT_BBITS(23,next);
          if (next == 0) break; /* out of mb_addr */
	}

	do
        { MPG_GET_MBAddrInc(tmp);  /* huff decode MB addr inc */

          DEBUG_LEVEL1 fprintf(stderr," %d",tmp);

          if (tmp == MB_ESCAPE) 
          { mb_addr += 33; tmp = MB_STUFFING;
          }
        } while(tmp == MB_STUFFING);

/* POD EXPERIMENTING TODO CLEAN POSSIBLY NEED TO CHANGE MGAddrInc*/
        if (tmp < 0) break;

        if (tmp > 1) mpg_dc_y = mpg_dc_cr = mpg_dc_cb = 1024;
        mb_addr += tmp;
        DEBUG_LEVEL1 fprintf(stderr,"  :mb_addr %d\n",mb_addr);

        if (mb_addr >= mb_size) break;

#ifdef REMOVE_OR_CHANGE_THIS
        /* check for end of slice */
        MPG_CHECK_BBITS(4,tmp); 
        if (tmp == xaFALSE) 
        { DEBUG_LEVEL1 fprintf(stderr,"end of slice\n");
          break;
        }
#endif

        /* Calculate Y,U,V buffers */
        { xaULONG offset;
          Ybuf = jpg_YUVBufs.Ybuf; 
	  Ubuf = jpg_YUVBufs.Ubuf;
	  Vbuf = jpg_YUVBufs.Vbuf;
    
          offset = DCTSIZE2 * mb_addr;
          Ubuf += offset;
          Vbuf += offset;
          Ybuf += offset << 2;
        }

        /* Decode I Type MacroBlock Type  1 or 01 */
        MPG_NXT_BBITS(2,i);

        DEBUG_LEVEL1 fprintf(stderr,"MB type %x\n",i);

        if (i & 0x02) { MPG_FLUSH_BBITS(1); }
        /* else if (i == 0x00) ERROR */
        else /* new quant scale */
        { MPG_FLUSH_BBITS(2);
          MPG_GET_BBITS(5,q_scale);  /* quant scale */
          DEBUG_LEVEL1 fprintf(stderr,"New Quant Scale %x\n",q_scale);
        }
	mpg_huffparseI(dct_dc_size_luminance, 7, &mpg_dc_y,
			q_scale, qtab, mpg_dct_buf, Ybuf); Ybuf += DCTSIZE2;
	mpg_huffparseI(dct_dc_size_luminance, 7, &mpg_dc_y,
			q_scale, qtab, mpg_dct_buf, Ybuf); Ybuf += DCTSIZE2;
	mpg_huffparseI(dct_dc_size_luminance, 7, &mpg_dc_y,
			q_scale, qtab, mpg_dct_buf, Ybuf); Ybuf += DCTSIZE2;
	mpg_huffparseI(dct_dc_size_luminance, 7, &mpg_dc_y,
			q_scale, qtab, mpg_dct_buf, Ybuf); Ybuf += DCTSIZE2;
	mpg_huffparseI(dct_dc_size_chrominance, 8, &mpg_dc_cr,
			q_scale, qtab, mpg_dct_buf, Ubuf); Ubuf += DCTSIZE2;
	mpg_huffparseI(dct_dc_size_chrominance, 8, &mpg_dc_cb,
			q_scale, qtab, mpg_dct_buf, Vbuf); Vbuf += DCTSIZE2;
      } while(mb_addr < (mb_size - 1) );
    } /* end of SLICE if */
    else
    {
      fprintf(stderr,"MPG_FULLI: UNK Code %08x\n",start_code);
    }
  } /* end of 1 */

  if (vert_pos >= 0) /* Found at least one slice */
  {
    (void)(color_func)(iptr,imagex,imagey,(mcu_cols * DCTSIZE2),orow_size,
			&jpg_YUVBufs,&def_yuv_tabs,map_flag,map,chdr);
  }

  if (map_flag) return(ACT_DLTA_MAPD); else return(ACT_DLTA_NORM);
}


