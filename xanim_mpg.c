
/*
 * xanim_mpg.c
 *
 * Copyright (C) 1995 by Mark Podlipec.
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
 *
 */

#include "xanim_mpg.h"
#include "xanim_xmpg.h"

/* internal FUNCTIONS */
LONG Is_MPG_File();
ULONG MPG_Read_File();
ULONG mpg_get_start_code();
ULONG mpg_read_SEQ_HDR();
ULONG mpg_read_GOP_HDR();
ULONG mpg_read_PIC_HDR();
ULONG MPG_Setup_Delta();
ULONG MPG_Decode_I();
ULONG MPG_Decode_P();
ULONG MPG_Decode_B();
void mpg_init_stuff();
void mpg_init_motion_vectors();
void mpg_init_mb_type_B();
void mpg_init_mb_type_P();
void mpg_init_mb_addr_inc();
void mpg_free_stuff();
void  decodeDCTDCSizeLum();
void  decodeDCTDCSizeChrom();
static void decodeDCTCoeff();
void  decodeDCTCoeffFirst();
void  decodeDCTCoeffNext();
void mpg_huffparseI();
void mpg_skip_extra_bitsB();
void j_orig_rev_dct();
void j_rev_dct();


/* external FUNCTIONS */
extern ULONG QT_Get_Color24();
extern void QT_Gen_YUV_Tabs();
XA_ACTION *ACT_Get_Action();
extern XA_ANIM_SETUP *XA_Get_Anim_Setup();
void XA_Free_Anim_Setup();
XA_CHDR   *ACT_Get_CMAP();
XA_CHDR *CMAP_Create_332();
XA_CHDR *CMAP_Create_Gray();
XA_CHDR *CMAP_Create_CHDR_From_True();
UBYTE *UTIL_RGB_To_Map();
UBYTE *UTIL_RGB_To_FS_Map();
void ACT_Setup_Mapped();
void ACT_Add_CHDR_To_Action();
extern void jpg_alloc_MCU_bufs();
extern void jpg_setup_samp_limit_table();
extern void jpg_411_spec_color();
extern void jpg_411_byte_color();
extern void jpg_411_short_color();
extern void jpg_411_long_color();
extern void jpg_411_byte_CF4();
extern ULONG UTIL_Get_MSB_Long();

/* external Buffers */
extern UBYTE *jpg_samp_limit;
extern UBYTE *jpg_Ybuf,*jpg_Ubuf,*jpg_Vbuf;
extern int xa_vid_fd;

#define MPG_RDCT  j_rev_dct
/* DON'T USE - NO LONGER THE SAME AS j_rev_dct
#define MPG_RDCT j_orig_rev_dct
*/

SHORT mpg_dct_buf[64];

UBYTE  *mpg_buff = 0;
LONG   mpg_bsize = 0;

/* BUFFER reading global vars */
LONG   mpg_b_bnum;
ULONG  mpg_b_bbuf;
FILE *mpg_f_file;

/* FILE reading global vars */
LONG   mpg_f_bnum;  /* this must be signed */
ULONG  mpg_f_bbuf;

ULONG mpg_init_flag = FALSE;

ULONG mpg_start_code,mpg_count;

/*
static UBYTE mpg_def_intra_qtab[64] = {
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
static UBYTE mpg_def_intra_qtab[64] = {
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

/* YUV cache tables for CVID */
extern LONG *QT_UB_tab;
extern LONG *QT_VR_tab;
extern LONG *QT_UG_tab;
extern LONG *QT_VG_tab;

/* used in xanim_avi.c */
UBYTE *mpg_dlta_buf = 0;
ULONG mpg_dlta_size = 0;


typedef struct MPG_FRAME_STRUCT
{ ULONG time;
  ULONG timelo;
  XA_ACTION *act;
  struct MPG_FRAME_STRUCT *next;
} MPG_FRAME;

MPG_FRAME *MPG_Add_Frame();
void MPG_Free_Frame_List();
MPG_FRAME *mpg_frame_start,*mpg_frame_cur;
ULONG mpg_frame_cnt;
ULONG mpg_Icnt,mpg_Pcnt,mpg_Bcnt;

/*******
 *
 */
#define MPG_INIT_FBUF(fin) \
  { mpg_f_file = fin; mpg_f_bnum = 0; mpg_f_bbuf = 0; }

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
{ result = (mpg_bsize > (num))?(TRUE):(FALSE); }

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
		mpg_f_bbuf = (fgetc(mpg_f_file)) | (mpg_f_bbuf << 8); } \
  mpg_f_bnum -= (num); \
  result = (mpg_f_bbuf >> mpg_f_bnum) & MPG_BIT_MASK(num); \
}

#define MPG_ALIGN_FBUF() { mpg_f_bnum -= (mpg_f_bnum % 8); }

/*******
 *
 */
MPG_FRAME *MPG_Add_Frame(time,timelo,act)
ULONG time,timelo;
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


/* TEMPORARY 
 */
LONG Is_MPG_File(filename)
char *filename;
{
  FILE *fin;
  ULONG data0;
  if ( (fin=fopen(filename,XA_OPEN_MODE)) == 0) return(XA_NOFILE);
  data0 = UTIL_Get_MSB_Long(fin);
  fclose(fin);
  if ( (data0 == 0x000001b3) || (data0 == 0x000001ba)) return(TRUE);
  return(FALSE);
}


ULONG MPG_Read_File(fname,anim_hdr)
char *fname;
XA_ANIM_HDR *anim_hdr;
{
  FILE *fin;
  ULONG i,t_timelo;
  ULONG t_time;
  XA_ANIM_SETUP *mpg;

  if ( (fin=fopen(fname,XA_OPEN_MODE)) == 0)
  {
    fprintf(stderr,"can't open MPEG File %s for reading\n",fname);
    return(FALSE);
  }

  mpg = XA_Get_Anim_Setup();
  mpg->vid_time = XA_GET_TIME( 200 ); /* default */
  mpg->cmap_frame_num = 20;  /* ?? how many frames in file?? */

/*
  mpg->imagex = mpg->imagey = 0;
  mpg->depth = mpg->imagec = 0;
  mpg->chdr = 0;
  mpg->cmap_flag = 0;
  mpg->cmap_cnt = 0;
  mpg->cmap_frame_num = 20;
  mpg->pic = 0;
  mpg->pic_size = 0;
  mpg->vid_timelo = 0;
  mpg->max_fvid_size = 0;
  mpg->compression = 0;
*/

  mpg_frame_cnt = 0;
  mpg_frame_cur = mpg_frame_start = 0;
  mpg_Icnt = mpg_Pcnt = mpg_Bcnt = 0;
  mpg_Gpic_hdr = mpg_pic_hdr = 0;
  mpg_pic_start = mpg_pic_cur = 0;

  mpg_seq_hdr = 0;

  mpg_init_stuff();
   /* find file size */

  MPG_INIT_FBUF(fin); /* init file bit buffer */
  if (mpg_get_start_code() == FALSE) return(FALSE);
  while( !feof(mpg_f_file) )
  { ULONG ret,need_nxt_flag = TRUE;

    if (mpg_start_code == MPG_SEQ_START)
    { XA_ACTION *act = 0;
      act = ACT_Get_Action(anim_hdr,ACT_NOP);
      mpg_seq_hdr = (MPG_SEQ_HDR *)malloc(sizeof(MPG_SEQ_HDR));
      act->data = (UBYTE *)mpg_seq_hdr;
      mpg_read_SEQ_HDR(mpg,mpg_seq_hdr);
    }
    else if (mpg_start_code == MPG_PIC_START)
    {
      if (mpg_Gpic_hdr==0)
      { XA_ACTION *act = 0;
        act = ACT_Get_Action(anim_hdr,ACT_NOP);
        mpg_Gpic_hdr = (MPG_PIC_HDR *)malloc(sizeof(MPG_PIC_HDR));
        act->data = (UBYTE *)mpg_Gpic_hdr;
        mpg_Gpic_hdr->slice_1st = mpg_Gpic_hdr->slice_last = 0;
	mpg_Gpic_hdr->slice_cnt = 0;
	mpg_Gpic_hdr->slices[0].fsize = 0;
	mpg_Gpic_hdr->slices[0].act   = 0;
      }
      mpg_read_PIC_HDR(mpg_Gpic_hdr);
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
	    act->data = (UBYTE *)mpg_pic_hdr;
	    mpg_pic_hdr->slice_1st = mpg_pic_hdr->slice_last = 0;
	    mpg_pic_hdr->slice_cnt = 0;
	    mpg_pic_hdr->slices[0].fsize = 0;
	    mpg_pic_hdr->slices[0].act   = 0;
            MPG_Add_Frame(mpg->vid_time,mpg->vid_timelo, act);
	  }
	  mpg_Icnt++; 
	  break;
	case MPG_TYPE_P: mpg_Pcnt++; break;
	case MPG_TYPE_B: mpg_Bcnt++; break;
	default: 
	  fprintf(stderr,"MPG Unk type %lx\n",mpg->compression);
	  break;
      }
    }
    else if (mpg_start_code == MPG_UNK_BA)
		{ DEBUG_LEVEL1 fprintf(stderr,"MPG: BA code  - ignored\n"); }
    else if (mpg_start_code == MPG_UNK_BB)
		{ DEBUG_LEVEL1 fprintf(stderr,"MPG: BB code  - ignored\n"); }
    else if (mpg_start_code == MPG_UNK_C0)
		{ DEBUG_LEVEL1 fprintf(stderr,"MPG: C0 code  - ignored\n"); }
    else if (mpg_start_code == MPG_UNK_E0)
		{ DEBUG_LEVEL1 fprintf(stderr,"MPG: E0 code  - ignored\n"); }
    else if (mpg_start_code == MPG_GOP_START)	mpg_read_GOP_HDR();
    else if (mpg_start_code == MPG_SEQ_END)	break; /* END??? */
    else if (mpg_start_code == MPG_USR_START)	
	{ DEBUG_LEVEL1 fprintf(stderr,"USR START:\n"); }
    else if (mpg_start_code == MPG_EXT_START)	
	{ DEBUG_LEVEL1 fprintf(stderr,"EXT START:\n"); }
    else if (   (mpg_start_code >= MPG_MIN_SLICE)
	     && (mpg_start_code <= MPG_MAX_SLICE) )
    { ULONG except_flag = 0;
      DEBUG_LEVEL1 fprintf(stderr,"SLICE %lx\n",mpg_start_code);
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
	tmp_act->data = (UBYTE *)mpg_pic_hdr;
	mpg_pic_hdr->slice_1st = mpg_pic_hdr->slice_last = 0;
	mpg_pic_hdr->slice_cnt = 0;
	mpg_pic_hdr->slices[0].fsize = 0;
	mpg_pic_hdr->slices[0].act   = 0;
	MPG_Add_Frame(mpg->vid_time,mpg->vid_timelo, tmp_act);
      }
      if ( (mpg->compression == MPG_TYPE_I) && (mpg_pic_hdr))
      { MPG_SLICE_HDR *slice_hdr;
        XA_ACTION *act = 0;

	/* alloc SLICE_HDR and ADD to current PIC_HDR */
	act = ACT_Get_Action(anim_hdr,ACT_NOP);
	slice_hdr = (MPG_SLICE_HDR *)malloc(sizeof(MPG_SLICE_HDR));
	act->data = (UBYTE *)slice_hdr;
	slice_hdr->next = 0;
	slice_hdr->act  = act;
	if (mpg_pic_hdr->slice_1st) mpg_pic_hdr->slice_last->next = slice_hdr;
	else			    mpg_pic_hdr->slice_1st = slice_hdr;
        mpg_pic_hdr->slice_last = slice_hdr;
	mpg_pic_hdr->slice_cnt++;

	slice_hdr->vert_pos = mpg_start_code & 0xff;
	slice_hdr->fpos = ftell(mpg_f_file);  /* get file position */
	mpg_count = 0;
	ret = mpg_get_start_code(); need_nxt_flag = FALSE;
	slice_hdr->fsize = mpg_count; /* get slice size */
	if (mpg_count > mpg->max_fvid_size) mpg->max_fvid_size = mpg_count;
      }
      if (except_flag) mpg_pic_hdr = 0; /* one slice only per pic_hdr*/
    }
    else fprintf(stderr,"MPG_UNK CODE: %02lx\n",mpg_start_code);

    /* get next if necessary */
    if (need_nxt_flag == TRUE) ret = mpg_get_start_code(); 
    if (ret == FALSE) break;
  } /* end of while forever */

  if (mpg_frame_cnt == 0)
  { fprintf(stderr,"MPG: No supported video frames exist in this file.\n");
    return(FALSE);
  }
  if (xa_verbose)
	fprintf(stdout,"MPG %ldx%ld frames %ld  I's %ld  P's %ld  B's %ld\n",
	  mpg->imagex,mpg->imagey,mpg_frame_cnt,mpg_Icnt,mpg_Pcnt,mpg_Bcnt);

  mpg_pic_hdr = 0; /* force err if used */

  /* Adjust Temporary Data Structures into Final Organization */
  mpg_frame_cur = mpg_frame_start;
  while(mpg_frame_cur != 0)
  { XA_ACTION *phdr_act = mpg_frame_cur->act;
    MPG_PIC_HDR *phdr = (MPG_PIC_HDR *)(phdr_act->data);
    ULONG slice_cnt = phdr->slice_cnt;

    if (slice_cnt == 0) 
    {
      fprintf(stderr,"MPG scnt 0 err - deal mit later\n");
    }
    else
    { MPG_PIC_HDR  *new_phdr;
      ACT_DLTA_HDR *dlta_hdr;
      MPG_SLICE_HDR *shdr;
      ULONG s_cnt;

      DEBUG_LEVEL1 fprintf(stderr,"MPG: Rearranging Structures\n");

       /* alloc new pic_hdr with slice array at end */
      s_cnt = (sizeof(ACT_DLTA_HDR)) + (sizeof(MPG_PIC_HDR));
      s_cnt += ( (slice_cnt + 1) * (sizeof(MPG_SLICE_HDR)) );
      dlta_hdr = (ACT_DLTA_HDR *)malloc( s_cnt );

      if (dlta_hdr == 0) TheEnd1("MPG dlta malloc err");
      phdr_act->data = (UBYTE *)dlta_hdr; /* old saved in phdr */

      dlta_hdr->flags = DLTA_DATA;
      dlta_hdr->fpos = dlta_hdr->fsize = 0;

      new_phdr = (MPG_PIC_HDR *)(dlta_hdr->data);
        /* copy old phdr to new phdr */
      memcpy((char *)new_phdr,(char *)phdr,sizeof(MPG_PIC_HDR));

      s_cnt = 0;
      shdr = phdr->slice_1st;
      while(shdr)
      { XA_ACTION *s_act = shdr->act;
	ULONG fpos,fsize;
        new_phdr->slices[s_cnt].vert_pos	= shdr->vert_pos;
        new_phdr->slices[s_cnt].fpos =   fpos  	= shdr->fpos;
        new_phdr->slices[s_cnt].fsize =  fsize	= shdr->fsize;
        new_phdr->slices[s_cnt].act		= shdr->act;
        shdr = shdr->next; /* move on */
	s_cnt++;

        free(s_act->data); s_act->data = 0;
        if ( (xa_buffer_flag == FALSE) && (xa_file_flag == FALSE) )
        {  /* need to read in the data */
          UBYTE *tmp = 0;
          fseek(mpg_f_file,fpos,0);
          tmp = (UBYTE *)malloc( fsize );
          if (tmp==0) TheEnd1("MPG: alloc err 3");
          fread((char *)tmp,fsize,1,mpg_f_file);
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
  fclose(fin);
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
    { fprintf(stderr,"MPG_Read_Anim: frame inconsistency %ld %ld\n",
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
  if (xa_buffer_flag == FALSE) anim_hdr->anim_flags |= ANIM_SNG_BUF;
  if (xa_file_flag == TRUE) anim_hdr->anim_flags |= ANIM_USE_FILE;
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
  return(TRUE);
}

/******************
 *  Look for 0x00 0x00 0x01 MPEG Start Code
 *****/
ULONG mpg_get_start_code()
{ LONG d; ULONG state = 0;
  MPG_ALIGN_FBUF(); /* make mpg_f_bnum multiple of 8 */
  while( !feof(mpg_f_file) )
  { 

/*
    MPG_GET_FBITS(8,d);
*/
    if (mpg_f_bnum > 0)
    { 
fprintf(stderr,"FBITS: %ld %lx\n",mpg_f_bnum,mpg_f_bbuf);

       mpg_f_bnum -= 8; d = (mpg_f_bbuf >> mpg_f_bnum) & 0xff;
    }
    else d =  fgetc(mpg_f_file);
    if (d >= 0) mpg_count++;

    if (state == 3) 
    { 
      mpg_start_code = d;
      mpg_f_bnum = 0; mpg_f_bbuf = 0;
      return(TRUE);
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
  return(FALSE);
}

/*****
 *
 */
void mpg_skip_extra_bitsB()
{ ULONG i,f;
  DEBUG_LEVEL1 fprintf(stderr,"MPG Extra:\n");
  i = 0;
  f = 1; 
  while(f)
  { ULONG d;
    MPG_GET_BBITS(1,f);
    if (f) 
    { MPG_GET_BBITS(8,d);
      DEBUG_LEVEL1
      { fprintf(stderr,"%02lx ",d);
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
 *   1 bit  constrained param flag(1 = TRUE, 0 = FALSE)
 *   1 bit  intra quant flag(1 = TRUE, 0 = FALSE)
 *      if (1) get 64 * 8 bits for intra quant table
 *   1 bit  non-intra quant flag(1 = TRUE, 0 = FALSE)
 *      if (1) get 64 * 8 bits for non-intra quant table
 */

/*Xing Added 9; */
ULONG mpg_pic_rate_idx[] = {30,24,24,25,30,30,50,60,60,15,30,30,30,30,30,30};

ULONG mpg_const_param,mpg_intra_q_flag,mpg_nonintra_q_flag;

ULONG mpg_read_SEQ_HDR(mpg,shdr)
XA_ANIM_SETUP *mpg;
MPG_SEQ_HDR *shdr;
{ ULONG i,d,marker;
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
if (xa_verbose) fprintf(stderr,"MPEG Pic rate: %lx\n",shdr->pic_rate);
  if (xa_jiffy_flag) { mpg->vid_time = xa_jiffy_flag; mpg->vid_timelo = 0; }
  { ULONG prate = mpg_pic_rate_idx[shdr->pic_rate];
    double ptime = (double)(1000.0) / (double)(prate);
    mpg->vid_time = (ULONG)(ptime);
    ptime -= (double)(mpg->vid_time);
    mpg->vid_timelo = (ULONG)(ptime * (double)(1<<24));
  }

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

  jpg_alloc_MCU_bufs(mpg->imagex);
  mpg->depth = 24; 
  QT_Gen_YUV_Tabs();
  jpg_setup_samp_limit_table();

  if (   (cmap_true_map_flag == FALSE) /* depth 16 and not true_map */
      || (xa_buffer_flag == FALSE) )
  {
     if (cmap_true_to_332 == TRUE)
             mpg->chdr = CMAP_Create_332(mpg->cmap,&mpg->imagec);
     else    mpg->chdr = CMAP_Create_Gray(mpg->cmap,&mpg->imagec);
  }
  if ( (mpg->pic==0) && (xa_buffer_flag == TRUE))
  {
    mpg->pic_size = mpg->imagex * mpg->imagey;
    if ( (cmap_true_map_flag == TRUE) && (mpg->depth > 8) )
                mpg->pic = (UBYTE *) malloc( 3 * mpg->pic_size );
    else mpg->pic = (UBYTE *) malloc( XA_PIC_SIZE(mpg->pic_size) );
    if (mpg->pic == 0) TheEnd1("MPG_Buffer_Action: malloc failed");
  }
  return(TRUE);
}


ULONG mpg_num_pics;

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
ULONG mpg_read_GOP_HDR()
{ ULONG drop_flag,hours,minutes,marker,seconds;
  ULONG mpg_num_pics,closed_gop,broken_link;
DEBUG_LEVEL1 fprintf(stderr,"MPG_GOP_START: \n");
  MPG_GET_FBITS(1,drop_flag);
  MPG_GET_FBITS(5,hours);
  MPG_GET_FBITS(6,minutes);
  MPG_GET_FBITS(1,marker);
  MPG_GET_FBITS(6,seconds);
  MPG_GET_FBITS(6,mpg_num_pics);
  MPG_GET_FBITS(1,closed_gop);
  MPG_GET_FBITS(1,broken_link);
  DEBUG_LEVEL1 fprintf(stderr,"  dflag %ld time %02ld:%02ld:%02ld mrkr %ld\n",
	drop_flag,hours,minutes,seconds,marker);
  DEBUG_LEVEL1 fprintf(stderr,"   numpics %ld %ld %ld\n",
	mpg_num_pics,closed_gop,broken_link);
 /* USR EXT data?? */
  return(TRUE);
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


ULONG mpg_read_PIC_HDR(phdr)
MPG_PIC_HDR *phdr;
{ ULONG d;
  MPG_GET_FBITS(10,phdr->time);
  MPG_GET_FBITS(3,phdr->type);
  MPG_GET_FBITS(16,phdr->vbv_delay);
DEBUG_LEVEL1 fprintf(stderr,"MPG_PIC_START: TYPE %ld\n",phdr->type);
  phdr->full_forw_flag = 0;
  phdr->forw_r_size = phdr->forw_f = 0;
  phdr->full_back_flag = 0;
  phdr->back_r_size = phdr->back_f = 0;

  if ( (phdr->type == MPG_TYPE_P) || (phdr->type == MPG_TYPE_B))
  {
    MPG_GET_FBITS(1,phdr->full_forw_flag);
    MPG_GET_FBITS(3,d);
    phdr->forw_r_size = d - 1;	phdr->forw_f = (1 << phdr->forw_r_size);
  }
  else if (phdr->type == MPG_TYPE_B)
  {
    MPG_GET_FBITS(1,phdr->full_back_flag);
    MPG_GET_FBITS(3,d);
    phdr->back_r_size = d - 1;	phdr->back_f = (1 << phdr->back_r_size);
  }
  return(TRUE);
}


/*******
 * assume mpg_pic_hdr is valid
 *
 * USES global variable xa_vid_fd
 */
ULONG MPG_Setup_Delta(mpg,fname,act)
XA_ANIM_SETUP *mpg;
char *fname;
XA_ACTION *act;			/* action to use  */
{
  ACT_DLTA_HDR *dlta_hdr = (ACT_DLTA_HDR *)act->data;
  MPG_PIC_HDR *the_pic_hdr = (MPG_PIC_HDR *)dlta_hdr->data;
  MPG_SEQ_HDR *the_seq_hdr = the_pic_hdr->seq_hdr;

  if ( (xa_buffer_flag==TRUE) || (xa_file_flag==TRUE) )
  { /* POD CAUTION   xa_vid_fd is global variable */
    if ( (xa_vid_fd=open(fname,O_RDONLY,NULL)) == 0)
    { fprintf(stderr,"MPG: Open file %s for video err\n",fname);
      return(FALSE);
    }
    if (mpg_dlta_buf==0)
    { mpg_dlta_size = mpg->max_fvid_size;
      mpg_dlta_buf = (UBYTE *)malloc(mpg_dlta_size);
      if (mpg_dlta_buf == 0) TheEnd1("MPG: dlta_buf alloc err");
    }
    else if (mpg->max_fvid_size > mpg_dlta_size)
    { UBYTE *tmp;
      mpg_dlta_size = mpg->max_fvid_size;
      tmp = (UBYTE *)realloc(mpg_dlta_buf,mpg_dlta_size);
      if (tmp) mpg_dlta_buf = tmp;
      else TheEnd1("MPG: dlta_buf realloc err");
    }
  }

  dlta_hdr->xpos = dlta_hdr->ypos = 0;
  dlta_hdr->xsize = the_seq_hdr->width;
  dlta_hdr->ysize = the_seq_hdr->height;
  dlta_hdr->special = 0;
  dlta_hdr->extra = (void *)(&mpg_dlta_buf); /* scratch buffer */
  switch(the_pic_hdr->type)
  {
    case MPG_TYPE_I:
	dlta_hdr->delta = MPG_Decode_I;
	break;
    case MPG_TYPE_B:
	dlta_hdr->delta = MPG_Decode_B;
	act->type = ACT_NOP; /* POD TEMP */
	return(TRUE);
	break;
    case MPG_TYPE_P:
	dlta_hdr->delta = MPG_Decode_P;
	act->type = ACT_NOP; /* POD TEMP */
	return(TRUE);
	break;
    default:
	fprintf(stderr,"MPG frame type %ld unknown\n",the_pic_hdr->type);
	act->type = ACT_NOP;
	return(FALSE);
	break;
  }

  ACT_Setup_Delta(mpg,act,dlta_hdr,0);

  if (xa_vid_fd >= 0) { close(xa_vid_fd); xa_vid_fd = -1; }
  return(TRUE);
}



/*******
 *
 */
ULONG
MPG_Decode_B(image,delta,dsize,tchdr,map,map_flag,imagex,imagey,imaged,
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

/* POD NOTE: Do better initializing these??? maybe not */

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
{ ULONG idx,f;
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
{ ULONG idx,f;
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
USHORT *dct_coeff_tbl;                                       
ULONG *run;
LONG *level;
{ ULONG temp, idx, value, f;

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
      { ULONG t;
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
	  if ( (*level) < 128 ) fprintf(stderr,"DCT err A-%ld\n",*level);
*/
	} else if (temp != 128) 
	{ 
	  if (temp & 0x80) *level = temp - 256;
          else *level = temp;
/* was
	  *level = ((LONG) (temp << 24)) >> 24; 
*/
	}
	else
	{ MPG_GET_BBITS(8,*level);
	  *level = *level - 256;
	/* POD Is Ness?? */
/*
	  if ( ((*level) > -128) || ((*level) < -255) )
			fprintf(stderr,"DCT err B-%ld\n",*level);
*/
	}
      }
    }
  }
  else 
  { ULONG t;
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
ULONG *run;
LONG *level;
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
ULONG *run;
LONG *level;
{
  decodeDCTCoeff(dct_coeff_next, run, level);
}

/********************END HUFFMAN*******************************************/

/*******
 *      Initialize all the tables for VLC decoding; this must be
 *      called when the system is set up before any decoding can
 *      take place.
 */


void mpg_init_stuff()
{
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


void mpg_free_stuff()
{
  if (mb_addr_inc) { free(mb_addr_inc); mb_addr_inc = 0; }
  if (mb_type_P) { free(mb_type_P); mb_type_P = 0; }
  if (mb_type_B) { free(mb_type_B); mb_type_B = 0; }
  if (motion_vectors) { free(motion_vectors); motion_vectors = 0; }
  if (mpg_dlta_buf) { free(mpg_dlta_buf); mpg_dlta_buf = 0; }
}

#define MPG_GET_MBAddrInc(ainc) \
{ register ULONG i,j;		\
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


LONG mpg_dc_y, mpg_dc_cr, mpg_dc_cb;

/*******
 *
 */
ULONG
MPG_Decode_I(image,delta,dsize,tchdr,map,map_flag,imagex,imagey,imaged,
                                                xs,ys,xe,ye,special,extra)
UBYTE *image;           /* Image Buffer. */
UBYTE *delta;           /* delta data. */
ULONG dsize;            /* delta size */
XA_CHDR *tchdr;         /* color map info */
ULONG *map;             /* used if it's going to be remapped. */
ULONG map_flag;         /* whether or not to use remap_map info. */
LONG imagex,imagey;	/* Size of image buffer. */
ULONG imaged;           /* Depth of Image. (IFF specific) */
ULONG *xs,*ys;          /* pos of changed area. */
ULONG *xe,*ye;          /* size of changed area. */
ULONG special;          /* Special Info. */
void *extra;            /* extra info needed to decode delta */
{
  ULONG mb_addr,vert_pos,q_scale;
  LONG x,y,mcu_cols,mcu_rows,mcu_row_size;
  UBYTE *qtab;
  UBYTE *Ybuf0,*Ybuf1,*Ubuf,*Vbuf;
  ULONG orow_size;
  MPG_PIC_HDR *phdr = (MPG_PIC_HDR *)(delta);
  MPG_SEQ_HDR *shdr =  phdr->seq_hdr;
  MPG_SLICE_HDR *slice;
  UBYTE *data_buf;
  XA_CHDR *chdr;
  ULONG special_flag;
  UBYTE *iptr = image;
  void (*color_func)();
  ULONG mb_size;
  LONG sidx,y_count;


  *xs = *ys = 0; *xe = imagex; *ye = imagey; 
  if (tchdr) {chdr=(tchdr->new_chdr)?(tchdr->new_chdr):(tchdr);} else chdr=0;

  special_flag = special & 0x0001;
  if (cmap_color_func == 4) { MPG_CMAP_CACHE_CHECK(chdr); }

  orow_size = imagex;
  if (special_flag) { orow_size *= 3;	color_func = jpg_411_spec_color; }
  else { orow_size *= x11_bytes_pixel;
    if (x11_bytes_pixel==1)		
    { 
      if ((cmap_color_func == 4) && (chdr)
          && !(x11_display_type & XA_X11_TRUE) ) color_func = jpg_411_byte_CF4;
      else color_func = jpg_411_byte_color;
    }
    else if (x11_bytes_pixel==2)	color_func = jpg_411_short_color;
    else				color_func = jpg_411_long_color;
  }
  imagex++; imagex >>= 1;
    
  mcu_cols  = ((shdr->width  + 15) / 16);
  mcu_rows = ((shdr->height + 15) / 16);
  mcu_row_size = 2 * mcu_cols * DCTSIZE2;
  mb_size = mcu_cols * mcu_rows;

  qtab = shdr->intra_qtab;

  /* get current slice */
  sidx = 0;
  slice = &(phdr->slices[sidx]);

  while(slice->fsize)
  { 
    if (slice->act) data_buf = (UBYTE *)(slice->act->data);
    else data_buf = 0;

    if (data_buf == 0)
    { LONG ret;
      UBYTE **t = (UBYTE **)(extra);
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

DEBUG_LEVEL1 { if (vert_pos != 0) fprintf(stderr,"PODTMP VERT_POS = %ld mcu_rows = %ld\n",vert_pos,mcu_rows); }

    /* adjust pointers for slice */
    y = mcu_rows - vert_pos;
    mb_addr = (vert_pos * mcu_cols) - 1;
    y_count = imagey - (vert_pos << 4);
    iptr   = image;
    iptr  += (orow_size * (vert_pos << 4));

    mpg_dc_y = mpg_dc_cr = mpg_dc_cb = 1024;
    while(y > 0)
    {
      if (y_count <= 0) break;
      Ybuf0 = jpg_Ybuf;   Ubuf = jpg_Ubuf;   Vbuf = jpg_Vbuf;
      Ybuf1 = Ybuf0 + mcu_row_size;
      x = mcu_cols; while(x--)
      { ULONG tmp,i;

	/* parse MB addr increment and update MB address */
	do
	{ MPG_GET_MBAddrInc(tmp);  /* huff decode MB addr inc */
	  if (tmp == MB_ESCAPE) { mb_addr += 33; tmp = MB_STUFFING; }
	} while(tmp == MB_STUFFING);

	mb_addr += tmp;
	if (tmp > 1) mpg_dc_y = mpg_dc_cr = mpg_dc_cb = 1024;

	if (mb_addr >= mb_size) { y = 0; break; }
	MPG_CHECK_BBITS(4,tmp); if (tmp == FALSE) { y = 0; break; }

	/* Decode I Type MacroBlock Type  1 or 01 */
	MPG_NXT_BBITS(2,i);
	if (i & 0x02) { MPG_FLUSH_BBITS(1); }
	/* else if (i == 0x00) ERROR */
	else /* new quant scale */
	{ MPG_FLUSH_BBITS(2);
	  MPG_GET_BBITS(5,q_scale);  /* quant scale */
	  DEBUG_LEVEL1 fprintf(stderr,"New Quant Scale %lx\n",q_scale);
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
      { (void)(color_func)(iptr,imagex,y_count,mcu_row_size,
                                                orow_size,map_flag,map,chdr);
	y_count -= 16;  iptr += (orow_size << 4);
	y--;
      }
    } /* end of y */
  } /* end of while slices */
  if (map_flag) return(ACT_DLTA_MAPD); else return(ACT_DLTA_NORM);
}


/*******
 *
 */
ULONG
MPG_Decode_P(image,delta,dsize,tchdr,map,map_flag,imagex,imagey,imaged,
                                                xs,ys,xe,ye,special,extra)
UBYTE *image;           /* Image Buffer. */
UBYTE *delta;           /* delta data. */
ULONG dsize;            /* delta size */
XA_CHDR *tchdr;         /* color map info */
ULONG *map;             /* used if it's going to be remapped. */
ULONG map_flag;         /* whether or not to use remap_map info. */
LONG imagex,imagey;	/* Size of image buffer. */
ULONG imaged;           /* Depth of Image. (IFF specific) */
ULONG *xs,*ys;          /* pos of changed area. */
ULONG *xe,*ye;          /* size of changed area. */
ULONG special;          /* Special Info. */
void *extra;            /* extra info needed to decode delta */
{
  *xs = *ys = 0; *xe = imagex; *ye = imagey; 
  if (map_flag) return(ACT_DLTA_MAPD); else return(ACT_DLTA_NORM);
}

#ifdef NOTYET_FINISHED
  ULONG mb_addr,vert_pos,q_scale;
  LONG x,y,mcu_cols,mcu_rows,mcu_row_size;
  UBYTE *qtab;
  UBYTE *Ybuf0,*Ybuf1,*Ubuf,*Vbuf;
  ULONG orow_size;
  MPG_PIC_HDR *phdr = (MPG_PIC_HDR *)(delta);
  MPG_SEQ_HDR *shdr =  phdr->seq_hdr;
  MPG_SLICE_HDR *slice;
  UBYTE *data_buf;
  XA_CHDR *chdr;
  ULONG special_flag;
  UBYTE *iptr = image;
  void (*color_func)();
  ULONG mb_size;
  LONG sidx,y_count;

ULONG Pquant,Pmotion_fwd,Ppat,Pintra;
ULONG P_cbp;
LONG mot_h_forw_code,mot_v_forw_code;
ULONG mot_h_forw_r,mot_v_forw_r;
LONG recon_right_for,recon_down_for;
LONG recon_right_for_prev,recon_down_for_prev;



  *xs = *ys = 0; *xe = imagex; *ye = imagey; 
  if (tchdr) {chdr=(tchdr->new_chdr)?(tchdr->new_chdr):(tchdr);} else chdr=0;

  special_flag = special & 0x0001;
  if (cmap_color_func == 4) { MPG_CMAP_CACHE_CHECK(chdr); }

  orow_size = imagex;
  if (special_flag) { orow_size *= 3;	color_func = jpg_411_spec_color; }
  else { orow_size *= x11_bytes_pixel;
    if (x11_bytes_pixel==1)		
    { 
      if ((cmap_color_func == 4) && (chdr)
          && !(x11_display_type & XA_X11_TRUE) ) color_func = jpg_411_byte_CF4;
      else color_func = jpg_411_byte_color;
    }
    else if (x11_bytes_pixel==2)	color_func = jpg_411_short_color;
    else				color_func = jpg_411_long_color;
  }
  imagex++; imagex >>= 1;

  mcu_cols  = ((shdr->width  + 15) / 16);
  mcu_rows = ((shdr->height + 15) / 16);
  mcu_row_size = 2 * mcu_cols * DCTSIZE2;
  mb_size = mcu_cols * mcu_rows;

  qtab = shdr->intra_qtab;

  /* get current slice */
  sidx = 0;
  slice = &(phdr->slices[sidx]);

  while(slice->fsize)
  { 
    data_buf = (UBYTE *)(slice->act->data);
    if (data_buf == 0)
    { LONG ret;
      UBYTE **t = (UBYTE **)(extra);
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

DEBUG_LEVEL1 { if (vert_pos != 0) fprintf(stderr,"PODTMP VERT_POS = %ld\n",vert_pos); }

    /* adjust pointers for slice */
    y = mcu_rows - vert_pos;
    mb_addr = (vert_pos * mcu_rows) - 1;
    y_count = imagey - (vert_pos << 4);
    iptr   = image;
    iptr  += (orow_size * (vert_pos << 4));

    mpg_dc_y = mpg_dc_cr = mpg_dc_cb = 1024;
    while(y > 0)
    {
      if (y_count <= 0) break;
      Ybuf0 = jpg_Ybuf;   Ubuf = jpg_Ubuf;   Vbuf = jpg_Vbuf;
      Ybuf1 = Ybuf0 + mcu_row_size;
      x = mcu_cols; while(x--)
      { ULONG tmp,i;

	/* parse MB addr increment and update MB address */
	do
	{ MPG_GET_MBAddrInc(tmp);  /* huff decode MB addr inc */
	  if (tmp == MB_ESCAPE) { mb_addr += 33; tmp = MB_STUFFING; }
	} while(tmp == MB_STUFFING);

	mb_addr += tmp;
	if (tmp > 1) mpg_dc_y = mpg_dc_cr = mpg_dc_cb = 1024;

	if (mb_addr >= mb_size) { y = 0; break; }
	MPG_CHECK_BBITS(4,tmp); if (tmp == FALSE) { y = 0; break; }

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
	  DEBUG_LEVEL1 fprintf(stderr,"New Quant Scale %lx\n",q_scale);
	}
		/* forward motion vectors exist */
	if (mb_motion_forw)
	{ ULONG i;

		/* decode horiz forward motion vector */
	  MPG_NXT_BBITS(11,i);
	  mot_h_forw_code = motion_vectors[i].code;
/* POD NOTE: signed?? dcval = mpg_huff_EXTEND(tmp,size); */
	  MPG_FLUSH_BBITS(motion_vectors[i].num_bits);
		/* if horiz forward r data exists, parse off */
	  if (mot_h_forw_code != 0) /* && (pic.forw_f != 1) */
	  {
	    MPG_GET_BBITS(forw_r_size,i);
	    mot_h_forw_r = i;  /* ULONG */
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
	    mot_v_forw_r = i;  /* ULONG */
	  }
	}
		/* Code Block Pattern exists */
	if (Ppat)
	{ ULONG index,f;
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
      { (void)(color_func)(iptr,imagex,y_count,mcu_row_size,
                                                orow_size,map_flag,map,chdr);
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
ULONG hsz;
LONG *dc_val_in;
LONG qscale;
UBYTE *qtab;
SHORT *dct_buf;
UBYTE *OBuf;
{ ULONG size,idx,flush;
  LONG tmp;
  LONG i,dcval,c_cnt;
  UBYTE *rnglimit = jpg_samp_limit + (MAXJSAMPLE + 1);

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
 
 memset( (char *)(dct_buf), 0, (DCTSIZE2 * sizeof(SHORT)) );
 dct_buf[0] = dcval;
 c_cnt = 0;

 i = 0;
 while( i < DCTSIZE2 )
 { LONG run,level;
   DECODE_DCT_COEFF_NEXT(&run,&level);
   if (run == END_OF_BLOCK) break;
   i += (run + 1);
   if (i < 64)
   { register ULONG pos = zigzag_direct[ i ];
     register LONG coeff = (level * qscale * (LONG)(qtab[pos]) ) >> 3;
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
    fprintf(stderr,"  %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
	dct_buf[ii], dct_buf[ii+1], dct_buf[ii+2], dct_buf[ii+3],
	dct_buf[ii+4], dct_buf[ii+5], dct_buf[ii+6], dct_buf[ii+7] );
   }
 }
*/


  if (c_cnt) j_rev_dct(dct_buf,OBuf,rnglimit);
  else
  { register UBYTE *op = OBuf;
    SHORT v = dcval;
    register UBYTE dc;
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
#define SHIFT_TEMPS	LONG shift_temp;
#define RIGHT_SHIFT(x,shft)  \
	((shift_temp = (x)) < 0 ? \
	 (shift_temp >> (shft)) | ((~((LONG) 0)) << (32-(shft))) : \
	 (shift_temp >> (shft)))
#else
#define SHIFT_TEMPS
#define RIGHT_SHIFT(x,shft)	((x) >> (shft))
#endif

/*POD TEST (could be 2) */
#define PASS1_BITS  2

#define ONE	((LONG) 1)

#define CONST_BITS 13

#define CONST_SCALE (ONE << CONST_BITS)

#define FIX(x)	((LONG) ((x) * CONST_SCALE + 0.5))

#define DESCALE(x,n)  RIGHT_SHIFT((x) + (ONE << ((n)-1)), n)

#define MULTIPLY(var,const)  ((var) * (const))

void j_rev_dct (data,outptr,rnglimit)
SHORT *data;
UBYTE *outptr;
UBYTE *rnglimit;
{
  LONG tmp0, tmp1, tmp2, tmp3;
  LONG tmp10, tmp11, tmp12, tmp13;
  LONG z1, z2, z3, z4, z5;
  LONG d0, d1, d2, d3, d4, d5, d6, d7;
  register SHORT *dataptr;
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
	  SHORT dcval = (SHORT) (d0 << PASS1_BITS);
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

    dataptr[0] = (SHORT) DESCALE(tmp10 + tmp3, CONST_BITS-PASS1_BITS);
    dataptr[7] = (SHORT) DESCALE(tmp10 - tmp3, CONST_BITS-PASS1_BITS);
    dataptr[1] = (SHORT) DESCALE(tmp11 + tmp2, CONST_BITS-PASS1_BITS);
    dataptr[6] = (SHORT) DESCALE(tmp11 - tmp2, CONST_BITS-PASS1_BITS);
    dataptr[2] = (SHORT) DESCALE(tmp12 + tmp1, CONST_BITS-PASS1_BITS);
    dataptr[5] = (SHORT) DESCALE(tmp12 - tmp1, CONST_BITS-PASS1_BITS);
    dataptr[3] = (SHORT) DESCALE(tmp13 + tmp0, CONST_BITS-PASS1_BITS);
    dataptr[4] = (SHORT) DESCALE(tmp13 - tmp0, CONST_BITS-PASS1_BITS);

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

