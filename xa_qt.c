
/*
 * xa_qt.c
 *
 * Copyright (C) 1993-1998,1999 by Mark Podlipec.
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
 * 31Aug94  RPZA was using *iptr+=row_inc instead of iptr+=row_inc.
 * 15Sep94  Added support for RAW32 format. straight RGB with 0x00 up front.
 * 19Sep94  Fixed stereo audio bug. Needed to double bps with stereo snd.
 * 20Sep94  I forgot to declare xin in the QT_Read_Audio_STSD(xin) and
 *	    that caused problems on the Alpha machines.
 * 20Sep94  Added RAW4,RAW16,RAW24,RAW32,Gray CVID and Gray Other codecs.
 * 07Nov94  Fixed bug in RLE,RLE16,RLE24,RLE32 and RLE1 code, where I
 *          had improperly guessed what a header meant. Now it's a different
 *	    hopeful more correct guess.
 * 29Dec94  Above bug wasn't fixe in RLE(8bit), now it is.
 * 30Jan95  Appletalk on SONY uses directory .afprsrc for resource forks
 *	    instead of .resource like other programs use. 
 *	    patch written by Kazushi Yoshida.
 * 11Feb95  Fixed Bug with RLE depth 1 codec.
 * 04Mar95  Added Dithering(+F option) to Cinepak Video Codec.
 * 08Mar95  Fixed audio Bug with SGI generated quicktimes audio depth > 8.
 * 17Mar95  Fixed bug that was causing quicktime to erroneously send
 *          back a FULL_IM flag allowing serious skipping to occur. This
 *          causes on screen corruption if the Video Codec doesn't 
 *          really support serious skipping.
 * 11Apr95  Fixed bug in QT_Create_Gray_Cmap that caused last color of
 *	    colormap not to be generated correctly. Only affected
 *	    the Gray Quicktime codecs.
 * 16Jun95  Removed Cinepak Codec per Radius request.
 * 15Sep95  Code snippet to support erroneous Quicktime files that
 *	    have the length of the "mdat" atom, but not the "mdat" ID.
 * 15Sep95  Better check for truncated Quicktime files
 * 26Feb96  Fixed prob in Read_Video_Data, where timing chunks were
 *          incremented properly resulting in video/audio sync problems.
 *  1Mar96  Moved Video Codecs into xa_qt_decs.c file
 * 19Mar96  Modified for support of audio only files.
 */
#include "xa_qt.h"
#include "xa_codecs.h"
#include "xa_cmap.h"
#include "xa_formats.h"
#include <sys/stat.h>

#ifdef XA_ZLIB
#include <zlib.h>
#endif

static XA_CODEC_HDR qt_codec_hdr;

extern xaULONG QT_Video_Codec_Query();
extern xaULONG XA_Mem_Open_Init();

static xaULONG QT_Read_Video_Codec_HDR();
static xaULONG QT_Read_Audio_Codec_HDR();
static void QT_Audio_Type();


static void QT_Create_Default_Cmap();
extern void QT_Create_Gray_Cmap();
extern char *XA_rindex();


xaUSHORT qt_gamma_adj[32];

QTV_CODEC_HDR *qtv_codecs;
QTS_CODEC_HDR *qts_codecs;
xaULONG qtv_codec_num,qts_codec_num;

extern XA_ACTION *ACT_Get_Action();
extern XA_CHDR *ACT_Get_CMAP();
extern XA_CHDR *CMAP_Create_332();
extern XA_CHDR *CMAP_Create_422();
extern XA_CHDR *CMAP_Create_Gray();
extern void ACT_Add_CHDR_To_Action();
extern void ACT_Setup_Mapped();
extern XA_CHDR *CMAP_Create_CHDR_From_True();
extern xaUBYTE *UTIL_RGB_To_FS_Map();
extern xaUBYTE *UTIL_RGB_To_Map();
extern xaULONG CMAP_Find_Closest();
extern XA_ANIM_SETUP *XA_Get_Anim_Setup();
extern void XA_Free_Anim_Setup();
extern void ACT_Setup_Delta();


#ifdef TEMPORARILY_REMOVE
FILE *QT_Open_File();
xaULONG QT_Parse_Bin();
#endif
xaULONG QT_Parse_Chunks();
xaULONG QT_Read_Video_Data();
xaULONG QT_Read_Audio_Data();

void QT_Print_ID();
void QT_Read_MVHD();
void QT_Read_TKHD();
void QT_Read_ELST();
void QT_Read_MDHD();
void QT_Read_HDLR();
xaULONG QT_Read_Video_STSD();
void QT_Read_Audio_STSD();
void QT_Read_Name();
void QT_Read_STTS();
void QT_Read_STSS();
void QT_Read_STCO();
void QT_Read_STSZ();
void QT_Read_STSC();
void QT_Read_STGS();
void QT_Free_Stuff();
void QT_Codec_List();



QT_MVHDR qt_mvhdr;
QT_TKHDR qt_tkhdr;
QT_MDHDR qt_mdhdr;
QT_HDLR_HDR qt_hdlr_hdr;

#ifdef TEMPORARILY_REMOVED
char qt_rfilename[256];
char qt_dfilename[256];
#endif
static xaULONG qt_video_flag, qt_audio_flag;
static xaULONG qt_data_flag;
static xaULONG qt_v_flag, qt_s_flag;
static xaULONG qt_moov_flag;

static xaULONG qt_frame_cnt;
static xaULONG qt_mv_timescale,qt_md_timescale;
static xaULONG qt_vid_timescale, qt_aud_timescale;
static xaULONG qt_tk_timescale,qts_tk_timescale,qtv_tk_timescale;

#define QT_CODEC_UNK   0x000

/*** SOUND SUPPORT ****/
static xaULONG qt_audio_attempt;
static xaULONG qt_audio_type;
static xaULONG qt_audio_freq;
static xaULONG qt_audio_chans;
static xaULONG qt_audio_bps;
static xaULONG qt_audio_end;
xaULONG XA_Add_Sound();


QT_FRAME *qt_frame_start,*qt_frame_cur;
 
QT_FRAME *QT_Add_Frame(time,timelo,act)
xaULONG time,timelo;
XA_ACTION *act;
{
  QT_FRAME *fframe;
 
  fframe = (QT_FRAME *) malloc(sizeof(QT_FRAME));
  if (fframe == 0) TheEnd1("QT_Add_Frame: malloc err");
 
  fframe->time = time;
  fframe->timelo = timelo;
  fframe->act = act;
  fframe->next = 0;
 
  if (qt_frame_start == 0) qt_frame_start = fframe;
  else qt_frame_cur->next = fframe;
 
  qt_frame_cur = fframe;
  qt_frame_cnt++;
  return(fframe);
}
 
void QT_Free_Frame_List(fframes)
QT_FRAME *fframes;
{
  QT_FRAME *ftmp;
  while(fframes != 0)
  {
    ftmp = fframes;
    fframes = fframes->next;
    FREE(ftmp,0x9000);
  }
}


xaLONG Is_QT_File(filename)
char *filename;
{
#ifdef TEMPORARILY_REMOVED
  ZILE *Zin;
  xaULONG ret;
  if ( (Zin=QT_Open_File(filename,qt_rfilename,qt_dfilename)) == 0)
				return(xaNOFILE);
  ret = QT_Parse_Bin(Zin);
  fclose(Zin);
  if ( ret != 0 ) return(xaTRUE);
#endif
  return(xaFALSE);
}

/* FOR PARSING Quicktime Files */
xaULONG *qtv_samp_sizes,*qts_samp_sizes;
xaULONG qtv_samp_num,qts_samp_num;
xaULONG qt_init_duration,qts_init_duration, qtv_init_duration;
xaULONG qt_start_offset,qts_start_offset, qtv_start_offset;

QT_S2CHUNK_HDR *qtv_s2chunks,*qts_s2chunks;
xaULONG qtv_s2chunk_num,qts_s2chunk_num;

QT_T2SAMP_HDR *qtv_t2samps,*qts_t2samps;
xaULONG qtv_t2samp_num,qts_t2samp_num;
 
xaULONG qtv_chunkoff_num,qts_chunkoff_num;
xaULONG *qtv_chunkoffs,*qts_chunkoffs;

xaULONG qtv_codec_lstnum,qts_codec_lstnum;
xaULONG qtv_chunkoff_lstnum,qts_chunkoff_lstnum;
xaULONG qtv_samp_lstnum,qts_samp_lstnum;
xaULONG qtv_s2chunk_lstnum,qts_s2chunk_lstnum;
xaULONG qt_stgs_num;

xaULONG qt_has_ctab;

xaULONG qt_saw_audio, qt_saw_video;

/* main() */
xaULONG QT_Read_File(const char* fname,XA_ANIM_HDR *anim_hdr,xaULONG audio_attempt)
{ XA_INPUT *xin;
  xaLONG i,t_time;
  xaULONG t_timelo;
  XA_ANIM_SETUP *qt;
  xaULONG qt_has_audio, qt_has_video;

  xin = anim_hdr->xin;
  qt = XA_Get_Anim_Setup();
  qt->vid_time 		= XA_GET_TIME( 100 ); /* default 10 fps */
  qt->compression	= QT_CODEC_UNK;


  qt_saw_audio	= xaFALSE;
  qt_saw_video	= xaFALSE;
  qt_has_audio	= xaFALSE;
  qt_has_video	= xaFALSE;
  qt_has_ctab	= xaFALSE;
  qt_stgs_num	= 0;
  qtv_codec_lstnum	= qts_codec_lstnum = 0;
  qtv_chunkoff_lstnum	= qts_chunkoff_lstnum = 0;
  qtv_samp_lstnum	= qts_samp_lstnum = 0;
  qtv_codecs = 0;
  qts_codecs = 0;
  qtv_codec_num 	= qts_codec_num = 0;
  qt_data_flag = xaFALSE;
  qt_video_flag		= 0;
  qt_audio_flag		= 0;
  qt_v_flag		= qt_s_flag = 0;
  qt_moov_flag = xaFALSE;

  qt_frame_cnt = 0;
  qt_frame_start = 0;
  qt_frame_cur = 0;

  qt_vid_timescale	= qt_aud_timescale = 1000;
  qt_mv_timescale	= qt_md_timescale = 1000;
  qts_tk_timescale 	= qtv_tk_timescale 	= 1000;
  qtv_chunkoff_num	= qts_chunkoff_num = 0;
  qtv_chunkoffs		= qts_chunkoffs = 0;
  qtv_s2chunk_num	= qts_s2chunk_lstnum = 0;
  qtv_s2chunks		= qts_s2chunks = 0;
  qtv_s2chunk_num	= qts_s2chunk_lstnum = 0;
  qtv_t2samp_num	= qts_t2samp_num = 0;
  qtv_t2samps		= qts_t2samps = 0;
  qtv_samp_sizes	= qts_samp_sizes = 0;
  qtv_samp_num		= qts_samp_num = 0;
  qt_init_duration	= qts_init_duration	= qtv_init_duration = 0;
  qt_start_offset 	= qts_start_offset	= qtv_start_offset = 0;

  qt_audio_attempt	= audio_attempt;

  for(i=0;i<32;i++) qt_gamma_adj[i] = xa_gamma_adj[ ((i<<3)|(i>>2)) ];

#ifdef TEMPORARILY_REMOVED
  if ( (xin=QT_Open_File(fname,qt_rfilename,qt_dfilename)) == 0)
  {
    fprintf(stdout,"QT_Read: can't open %s\n",qt_rfilename);
    XA_Free_Anim_Setup(qt);
    return(xaFALSE);
  }

  if ( QT_Parse_Bin(xin) == 0 )
  {
    fprintf(stdout,"Not quicktime file\n");
    XA_Free_Anim_Setup(qt);
    return(xaFALSE);
  }
#endif

  if (QT_Parse_Chunks(anim_hdr,qt,xin) == xaFALSE)
  {
    QT_Free_Stuff();
    XA_Free_Anim_Setup(qt);
    return(xaFALSE);
  }

  if (qt_data_flag == xaFALSE) 
  {
    fprintf(stderr,"QT: no data found in file\n");
    xin->Close_File(xin);
    return(xaFALSE);
  }
#ifdef TEMPORARILY_REMOVED
  if (qt_data_flag == xaFALSE) 
  { /* mdat was not in .rscr file need to open .data file */
    fclose(xin); /* close .rscr file */
    if (qt_dfilename[0] == 0)
    { fprintf(stdout,"QT_Data: No data in %s file and no *.data file.\n",
		qt_rfilename);
      fprintf(stdout,"         Some Quicktimes do not contain info playable by XAnim.\n");
      return(xaFALSE);
    }
    if ( (xin=fopen(qt_dfilename,XA_OPEN_MODE)) == 0) 
    { fprintf(stdout,"QT_Data: can't open %s file.\n",qt_dfilename);
      return(xaFALSE);
    }
  } else strcpy(qt_dfilename,qt_rfilename); /* r file is d file */
#endif

DEBUG_LEVEL1 fprintf(stdout,"reading data\n");

  if (qtv_samp_sizes && qt_video_flag)
		qt_has_video = QT_Read_Video_Data(qt,xin,anim_hdr);
  if (qts_samp_sizes && qt_audio_flag && (qt_audio_attempt==xaTRUE))
		qt_has_audio = QT_Read_Audio_Data(qt,xin,anim_hdr);
  xin->Close_File(xin);

  if ((qt_has_video == xaFALSE) || (qt_frame_cnt == 0))
  {

    if (qt_has_audio == xaFALSE)  /* no video and no audio */
    { 

      if ((qt_saw_video == xaTRUE) && (qt_saw_audio == xaTRUE))
      { fprintf(stdout,
	   "  Notice: Video and Audio are present, but not yet supported.\n");
      }
      else if ((qt_saw_video == xaTRUE) && (qt_saw_audio == xaFALSE))
      { fprintf(stdout,
	   "  Notice: Video is present, but not yet supported.\n");
      }
      else if ((qt_saw_video == xaFALSE) && (qt_saw_audio == xaTRUE))
      { fprintf(stdout,
	   "  Notice: Audio is present, but not yet supported.\n");
      }
      else
      {		 /* At least we saw the moov header */
        if (qt_moov_flag == xaTRUE)
	{
		/* No Video and we weren't trying for audio */
          if (qt_audio_attempt == xaFALSE)
          {
             fprintf(stdout,"QT: no video to play. possibly truncated.\n");
          }
          else
	  {
		/* No Video and No Audio */
	    fprintf(stdout,"QT: no video/audio to play. possibly truncated.\n");
	  }
        }
        else  /* either truncated or missing .rsrc file */
	{
	  fprintf(stdout,
		"QT: file possibly truncated or missing .rsrc info.\n");
        }
      }
      return(xaFALSE);
    }

    if (qt_saw_video == xaTRUE)
    { fprintf(stdout,
	   "  Notice: Video is present, but not yet supported.\n");
    }

/* NO LONGER???
    if (qtv_samp_sizes)
	fprintf(stdout,"QT Notice: No supported Video frames - treating as audio only file\n");
*/

    anim_hdr->total_time = (qt_mvhdr.duration * 1000) / (qt_mvhdr.timescale);
  }
  else  
  {
    anim_hdr->frame_lst = (XA_FRAME *)
                                malloc( sizeof(XA_FRAME) * (qt_frame_cnt+1));
    if (anim_hdr->frame_lst == NULL) TheEnd1("QT_Read_File: frame malloc err");
 
    qt_frame_cur = qt_frame_start;
    i = 0;
    t_time = 0;
    t_timelo = 0;
    while(qt_frame_cur != 0)
    { if (i >= qt_frame_cnt)
      { fprintf(stdout,"QT_Read_Anim: frame inconsistency %d %d\n",
                i,qt_frame_cnt); break;
      }
      anim_hdr->frame_lst[i].time_dur = qt_frame_cur->time;
      anim_hdr->frame_lst[i].zztime = t_time;
      t_time	+= qt_frame_cur->time;
      t_timelo	+= qt_frame_cur->timelo;
      while(t_timelo >= (1<<24)) {t_time++; t_timelo -= (1<<24);}
      anim_hdr->frame_lst[i].act = qt_frame_cur->act;
      qt_frame_cur = qt_frame_cur->next;
      i++;
    }
    if (i > 0)
    { anim_hdr->last_frame = i - 1;
      anim_hdr->total_time = anim_hdr->frame_lst[i-1].zztime
                                + anim_hdr->frame_lst[i-1].time_dur;
    }
    else { anim_hdr->last_frame = 0; anim_hdr->total_time = 0; }

    if (xa_verbose)
    { fprintf(stdout, "  Frame Stats: Size=%dx%d  Frames=%d",
				qt->imagex,qt->imagey,qt_frame_cnt);
      if (anim_hdr->total_time) 
      { float fps = (float)(1000 * qt_frame_cnt)/(float)(anim_hdr->total_time);
        fprintf(stdout, "  avfps=%2.1f\n",fps);
      } 
      else fprintf(stdout,"\n");
    }
    anim_hdr->imagex = qt->max_imagex;
    anim_hdr->imagey = qt->max_imagey;
    anim_hdr->imagec = qt->imagec;
    anim_hdr->imaged = 8; /* nop */
    anim_hdr->frame_lst[i].time_dur = 0;
    anim_hdr->frame_lst[i].zztime = -1;
    anim_hdr->frame_lst[i].act  = 0;
    anim_hdr->loop_frame = 0;
  }

  if (xa_buffer_flag == xaFALSE) anim_hdr->anim_flags |= ANIM_SNG_BUF;
  anim_hdr->max_fvid_size = qt->max_fvid_size;
  anim_hdr->max_faud_size = qt->max_faud_size;
	/***----------------------------------------------------------***
	 * If reading from file, then use the data fork file name
	 * as the file name.
	 ***----------------------------------------------------------***/
  if (xa_file_flag == xaTRUE) 
  { /* xaULONG len; */
    anim_hdr->anim_flags |= ANIM_USE_FILE;
#ifdef TEMPORARILY_REMOVED
    len = strlen(qt_dfilename) + 1;
    anim_hdr->fname = (char *)malloc(len);
    if (anim_hdr->fname==0) TheEnd1("QT: malloc fname err");
    strcpy(anim_hdr->fname,qt_dfilename);
#endif
  }
  anim_hdr->fname = anim_hdr->name;
  QT_Free_Stuff();
  XA_Free_Anim_Setup(qt);
  return(xaTRUE);
}

void QT_Free_Stuff()
{
  QT_Free_Frame_List(qt_frame_start);
  if (qtv_samp_sizes) FREE(qtv_samp_sizes,0x9003);
  if (qts_samp_sizes) FREE(qts_samp_sizes,0x9004);
  if (qtv_codecs) FREE(qtv_codecs,0x9005);
  if (qts_codecs) FREE(qts_codecs,0x9006);
  if (qtv_t2samps) FREE(qtv_t2samps,0x9007);
  if (qts_t2samps) FREE(qts_t2samps,0x9008);
  if (qtv_s2chunks) FREE(qtv_s2chunks,0x9009);
  if (qts_s2chunks) FREE(qts_s2chunks,0x900a);
  if (qtv_chunkoffs) FREE(qtv_chunkoffs,0x900b);
  if (qts_chunkoffs) FREE(qts_chunkoffs,0x900c);
}

/****------------------------------------------------------------------****
 * Quicktime exists natively on Macintosh as a .rsrc file(containing
 * all the headers) and a .data file(containing all the vid/aud/txt/etc data)
 *
 * Hopefully the file was flattened, made into single fork, saved for
 * export, etc, before being send into the real world.  In this case
 * all the info needed is contained in one single file.
 *
 * However, all the fun is in the other convoluted solutions developed
 * to cross-platform the Macintosh .rsrc/.data forks to unix.
 *
 * If sent across using MacBinary and unpacked with macutils.
 *
 *    filename.data		data fork
 *    filename.rsrc		rsrc fork
 *
 * Sony's NEWS-OS when file sharing over AppleTalk does the following:
 *    filename			data fork
 *    .afprsrc/filename		rsrc fork
 * 
 * Another file sharing method is:
 *    filename			data fork
 *    .resource/filename	rsrc fork
 *
 *
 * This routine determines the rsrc file name and the data file name.
 ****------------------------------------------------------------------****/
 
xaULONG QT_Parse_Chunks(anim_hdr,qt,xin)
XA_ANIM_HDR *anim_hdr;
XA_ANIM_SETUP *qt;
XA_INPUT *xin;
{
  xaLONG file_len;
  xaULONG id,len;

  file_len = 1;
  while(file_len > 0)
  {
    len = xin->Read_MSB_U32(xin);
    id  = xin->Read_MSB_U32(xin);

/* if (xa_verbose) */
DEBUG_LEVEL1 
	fprintf(stdout,"%c%c%c%c %04x len = %x file_len =  %x\n",
	(char)(id >> 24),(char)((id>>16)&0xff),
	(char)((id>>8)&0xff),(char)(id&0xff),id,len,file_len);

    if ( (len == 0) && (id == QT_mdat) )
    {
      fprintf(stdout,"QT: mdat len is 0. You also need a .rsrc file.\n");
      return(xaFALSE);
    }
    if (len < 8) { file_len = 0; continue; } /* just bad - xinish this */
    if (file_len == 1)
    {
      if (id == QT_moov) file_len = len;
      else file_len = len;
    }

/* if (xa_verbose) */
DEBUG_LEVEL1 
    fprintf(stdout,"  len = %x file_len = %x\n",len,file_len);

    switch(id)
    {
    /*--------------ATOMS------------------*/
      case QT_trak:
	qt_v_flag = qt_s_flag = 0;
	qtv_codec_lstnum = qtv_codec_num;
	qts_codec_lstnum = qts_codec_num;
	qtv_chunkoff_lstnum = qtv_chunkoff_num;
	qts_chunkoff_lstnum = qts_chunkoff_num;
	qtv_samp_lstnum = qtv_samp_num;
	qts_samp_lstnum = qts_samp_num;
	qtv_s2chunk_lstnum = qtv_s2chunk_num;
	qts_s2chunk_lstnum = qts_s2chunk_num;
	file_len -= 8;
	break;
      case QT_moov:
        qt_moov_flag = xaTRUE;
	file_len -= 8;
	break;
      case QT_mdia:
      case QT_minf:
      case QT_stbl:
      case QT_edts:
	file_len -= 8;
	break;
    /*---------------STUFF------------------*/
      case QT_mvhd:
	QT_Read_MVHD(xin,&qt_mvhdr);
	file_len -= len;
	break;
      case QT_tkhd:
	QT_Read_TKHD(xin,&qt_tkhdr);
	file_len -= len;
	break;
      case QT_elst:
	QT_Read_ELST(xin,&qt_start_offset,&qt_init_duration);
	file_len -= len;
	break;
      case QT_mdhd:
	QT_Read_MDHD(xin,&qt_mdhdr);
	file_len -= len;
	break;
      case QT_hdlr:
	QT_Read_HDLR(xin,len,&qt_hdlr_hdr);
	file_len -= len;
	break;
    /*--------------DATA CHUNKS-------------*/
      case QT_mdat:  /* data is included in .rsrc - assumed flatness */
	xin->Seek_FPos(xin,(len-8),1); /* skip over it for now */
	file_len -= len;
	qt_data_flag = xaTRUE;
	break;
      case QT_stsd:
	if (qt_v_flag)
	{ qt_saw_video = xaTRUE;
	  QT_Read_Video_STSD(anim_hdr,qt,xin,(len-8));
	}
	else if (qt_s_flag)
	{ qt_saw_audio = xaTRUE;
	  QT_Read_Audio_STSD(xin,(len-8));
	}
        else xin->Seek_FPos(xin,(len-8),1);
	file_len -= len;
	break;
      case QT_stts:
	if (qt_v_flag) 
		QT_Read_STTS(xin,(len-8),&qtv_t2samp_num,&qtv_t2samps);
/*POD NOTE: AUDIO doesn't need? probably, just haven't been bit by it yet.
	else if (qt_s_flag) 
		QT_Read_STTS(xin,(len-8),&qts_t2samp_num,&qts_t2samps);
*/
        else	xin->Seek_FPos(xin,(len-8),1);
	file_len -= len;
	break;
      case QT_stss:
	QT_Read_STSS(xin);
	file_len -= len;
	break;
      case QT_stco:
	if (qt_v_flag) QT_Read_STCO(xin,&qtv_chunkoff_num,&qtv_chunkoffs);
	else if (qt_s_flag) QT_Read_STCO(xin,&qts_chunkoff_num,&qts_chunkoffs);
	else xin->Seek_FPos(xin,(len-8),1);
	file_len -= len;
	break;
      case QT_stsz:
	if (qt_v_flag) QT_Read_STSZ(xin,len,&qtv_samp_num,&qtv_samp_sizes);
	else if (qt_s_flag) 
		QT_Read_STSZ(xin,len,&qts_samp_num,&qts_samp_sizes);
	else xin->Seek_FPos(xin,(len-8),1);
	file_len -= len;
	break;
      case QT_stsc:
	if (qt_v_flag) QT_Read_STSC(xin,len,&qtv_s2chunk_num,&qtv_s2chunks,
			qtv_chunkoff_lstnum,qtv_codec_num,qtv_codec_lstnum);
	else if (qt_s_flag) QT_Read_STSC(xin,len,&qts_s2chunk_num,&qts_s2chunks,
			qts_chunkoff_lstnum,qts_codec_num,qts_codec_lstnum);
	else xin->Seek_FPos(xin,(len-8),1);
	file_len -= len;
	break;
      case QT_stgs:
	QT_Read_STGS(xin,len);
	file_len -= len;
	break;
    /*-----------Sound Codec Headers--------------*/
      case QT_raw00:
    /*-----------Video/Sound Codec Headers--------------*/
      case QT_raw:
    /*-----------Video Codec Headers--------------*/
      case QT_smc:
      case QT_rpza:
      case QT_rle:
      case QT_cvid:
	fprintf(stdout,"QT: Warning %08x\n",id);
        xin->Seek_FPos(xin,(len-8),1);  /* skip over */
	file_len -= len;
	break;
    /*-----------TYPE OF TRAK---------------*/
      case QT_vmhd:
        xin->Seek_FPos(xin,(len-8),1);
	file_len -= len; qt_v_flag = 1;
	qt_vid_timescale = qt_md_timescale;
	qtv_tk_timescale = qt_tk_timescale;
	qtv_init_duration = qt_init_duration; qt_init_duration = 0;
	qtv_start_offset = qt_start_offset; qt_start_offset = 0;
	break;
      case QT_smhd:
        xin->Seek_FPos(xin,(len-8),1);
	file_len -= len; qt_s_flag = 1;
	qt_aud_timescale = qt_md_timescale;
	qts_tk_timescale = qt_tk_timescale;
	qts_init_duration = qt_init_duration; qt_init_duration = 0;
	qts_start_offset = qt_start_offset; qt_start_offset = 0;
	break;
    /************ CTAB ******************
     * Color Table to be used for display 16/24 bit animations on
     * 8 Bit displays.
     *************************************/
      case QT_ctab:
	{ xaULONG i,tmp,start,end;
          if (x11_display_type != XA_PSEUDOCOLOR)
	  {
	     while(len > 0) {xin->Read_U8(xin); len--; } 
	     break;
	  }
if (xa_verbose) fprintf(stdout,"QT: has ctab\n");
          tmp   = xin->Read_MSB_U32(xin);  /* ?? */
          start = xin->Read_MSB_U16(xin);  /* start */
          end   = xin->Read_MSB_U16(xin);  /* end */
	  len -= 8;
	  for(i=start; i <= end; i++)
	  { xaULONG idx,r,g,b;
	    idx = xin->Read_MSB_U16(xin);
	    r   = xin->Read_MSB_U16(xin);
	    g   = xin->Read_MSB_U16(xin);
	    b   = xin->Read_MSB_U16(xin);  len -= 8;
	    /* if (cflag & 0x8000)  idx = i; */
	    if (idx < qt->imagec)
	    {
	      qt->cmap[idx].red   = r;
	      qt->cmap[idx].green = g;
	      qt->cmap[idx].blue  = b;
	    }
	    if (len <= 0) break;
	  } /* end of for i */
	  while(len > 0) {xin->Read_U8(xin); len--; } 
	  qt->imagec = 256;
	  qt->chdr = ACT_Get_CMAP(qt->cmap,qt->imagec,0,qt->imagec,0,8,8,8);
	  qt_has_ctab = xaTRUE;
	}
	break;
    /*--------------Indicated We Don't Support this File -----------*/
      case QT_rmra:
	fprintf(stdout,"NOTE: RMRA chunk is not yet supported.\n");
        xin->Seek_FPos(xin,(len-8),1);  /* skip over */
	file_len -= len;
	break;
/*
00000000: 00000AAC 636D7664 00001A0C 789CB598   ....cmvd....x...
00000010: 7B7054D5 1DC7BF1B 486049C8 3BEC665F   {pT.....H`I.;.f_
*/
      case QT_cmov:
	{ unsigned char *cmov_buf = 0;
	  unsigned char *moov_buf = 0;
	  int cmov_ret = xaFALSE;
	  int tlen, cmov_sz,moov_sz;
	  xaULONG cmov_comp = 0;
	  XA_INPUT	mem_xin;

	  file_len -= len;

	  len -= 8;
	  while(len > 8)
	  { xaULONG t_id, t_sz;
            
	    t_sz = xin->Read_MSB_U32(xin);
	    t_id = xin->Read_MSB_U32(xin);

	    len -= 8;
	    t_sz -= 8;
	  DEBUG_LEVEL1 fprintf(stderr,"QT CMOV: id %08x len %08x\n",t_id,t_sz);

	    if (len < t_sz)
	    { fprintf(stderr,"QT err parsing cmov\n");
	      break;
	    }
	    len -= t_sz;
	    switch(t_id)
	    {
		/* Find out how cmov is compressed */
	      case QT_dcom:
		cmov_comp = xin->Read_MSB_U32(xin);	t_sz -= 4;
		if (cmov_comp != QT_zlib)
		{ fprintf(stderr,"QT cmov: unsupported compression %08x\n",
							cmov_comp);
		  fprintf(stderr,"         contact author.\n");
		  break;
		}
		else if (xa_verbose)
		{ 
		  fprintf(stdout,"  QT Compressed Hdr Codec: zlib\n");
		}
		if (t_sz > 0) { xin->Seek_FPos(xin,t_sz,1); t_sz = 0; }
		break;

	      case QT_cmvd:
			/* read how large uncompressed moov will be */
		moov_sz = xin->Read_MSB_U32(xin); t_sz -= 4;
		cmov_sz = t_sz;

			/* Allocate buffer for compressed header */
		cmov_buf = (unsigned char *)malloc( cmov_sz );
		if (cmov_buf == 0) TheEnd1("QT cmov: malloc err 0");
			/* Read in  compressed header */
		tlen = xin->Read_Block(xin, cmov_buf, cmov_sz);
		if (tlen != cmov_sz)
		{ fprintf(stderr,"QT cmov: read err\n");
		  free(cmov_buf);
		  return(xaFALSE);
		}
			/* Allocate buffer for decompressed header */
		moov_sz += 16; /* slop?? */
		moov_buf = (unsigned char *)malloc( moov_sz );
		if (moov_buf == 0) TheEnd1("QT cmov: malloc err 1");

#ifndef XA_ZLIB
	  	  fprintf(stderr,
			"QT: cmov support not compiled in. See zlib.readme\n");
		  break;
#else
		{ z_stream zstrm;
		  int zret;
		 
  		  zstrm.zalloc          = (alloc_func)0;
  		  zstrm.zfree           = (free_func)0;
  		  zstrm.opaque          = (voidpf)0;
  		  zstrm.next_in         = cmov_buf;
  		  zstrm.avail_in        = cmov_sz;
  		  zstrm.next_out        = moov_buf;
  		  zstrm.avail_out       = moov_sz;

  		  zret = inflateInit(&zstrm);
  		  if (zret != Z_OK)
	          { fprintf(stderr,"QT cmov: inflateInit err %d\n",zret);
		    break;
		  }
  		  zret = inflate(&zstrm, Z_NO_FLUSH);
		  if ((zret != Z_OK) && (zret != Z_STREAM_END))
	          { fprintf(stderr,"QT cmov inflate: ERR %d\n",zret);
		    break;
		  }
		  moov_sz = zstrm.total_out;
  		  zret = inflateEnd(&zstrm);

		  XA_Mem_Open_Init(&mem_xin,moov_buf,moov_sz);
		  cmov_ret = QT_Parse_Chunks(anim_hdr,qt,&mem_xin);
		  DEBUG_LEVEL1 fprintf(stderr,"QT cmov parse %d\n",cmov_ret);
		}
#endif
		break;

	      default:
		fprintf(stderr,"QT CMOV: Unk chunk %08x \n",t_id);
		if (t_sz > 0) { xin->Seek_FPos(xin,t_sz,1); t_sz = 0; }
		break;

	    } /* end of cmov sub chunk ID case */
	  } /* end of while len  */
	  if (cmov_buf) free(cmov_buf);
	  if (moov_buf) free(moov_buf);
          if (cmov_ret == xaFALSE) return(cmov_ret); /*failed or unsupported*/
	} /* end of cmov */
	break;
    /*--------------IGNORED FOR NOW---------*/
      case QT_gmhd:
      case QT_text:
      case QT_clip:
      case QT_skip:
      case QT_free:
      case QT_udta:
      case QT_dinf:
        xin->Seek_FPos(xin,(len-8),1);  /* skip over */
	file_len -= len;
	break;
    /*--------------UNKNOWN-----------------*/
      default:
	if ( !xin->At_EOF(xin) && (len <= file_len) )
	{
	  xaLONG i;

DEBUG_LEVEL1
{
	  QT_Print_ID(stdout,id,1);
	  fprintf(stdout," len = %x file_len %x\n",len,file_len);
}
	  i = (xaLONG)(len) - 8;
		/* read and check eof */
	  while(i > 0) { i--; xin->Read_U8(xin); if (xin->At_EOF(xin)) i = 0; }
	}
	file_len -= len;
	break;
    } /* end of switch */

    if ( xin->At_EOF(xin) ) 
    {
      file_len = 0;
      if ((qt_moov_flag == xaFALSE) && (qt_data_flag == xaTRUE))
      { fprintf(stdout,"QT: file possibly truncated or missing .rsrc info.\n");
	return(xaFALSE);
      }
    }
    else if (file_len <= 0)
    {
	/* interesting dilemma */
      if (   (qt_data_flag == xaFALSE)
	  || (qt_moov_flag == xaFALSE))
      {
	/** START OVER **/
	file_len = 1;
      }
    }
  } /* end of while */
  return(xaTRUE);
}

void QT_Print_ID(fout,id,flag)
FILE *fout;
xaLONG id,flag;
{ fprintf(fout,"%c",     (char)((id >> 24) & 0xff));
  fprintf(fout,"%c",     (char)((id >> 16) & 0xff));
  fprintf(fout,"%c",     (char)((id >>  8) & 0xff));
  fprintf(fout,"%c",     (char) (id        & 0xff));
  if (flag) fprintf(fout,"(%x)",id);
}


void QT_Read_MVHD(xin,qt_mvhdr)
XA_INPUT *xin;
QT_MVHDR *qt_mvhdr;
{
  xaULONG i,j;

  qt_mvhdr->version =	xin->Read_MSB_U32(xin);
  qt_mvhdr->creation =	xin->Read_MSB_U32(xin);
  qt_mvhdr->modtime =	xin->Read_MSB_U32(xin);
  qt_mvhdr->timescale =	xin->Read_MSB_U32(xin);
  qt_mvhdr->duration =	xin->Read_MSB_U32(xin);
  qt_mvhdr->rate =	xin->Read_MSB_U32(xin);
  qt_mvhdr->volume =	xin->Read_MSB_U16(xin);
  qt_mvhdr->r1  =	xin->Read_MSB_U32(xin);
  qt_mvhdr->r2  =	xin->Read_MSB_U32(xin);
  for(i=0;i<3;i++) for(j=0;j<3;j++) 
	qt_mvhdr->matrix[i][j]=xin->Read_MSB_U32(xin);
  qt_mvhdr->r3  =	xin->Read_MSB_U16(xin);
  qt_mvhdr->r4  =	xin->Read_MSB_U32(xin);
  qt_mvhdr->pv_time =	xin->Read_MSB_U32(xin);
  qt_mvhdr->post_time =	xin->Read_MSB_U32(xin);
  qt_mvhdr->sel_time =	xin->Read_MSB_U32(xin);
  qt_mvhdr->sel_durat =	xin->Read_MSB_U32(xin);
  qt_mvhdr->cur_time =	xin->Read_MSB_U32(xin);
  qt_mvhdr->nxt_tk_id =	xin->Read_MSB_U32(xin);

  if (qt_mvhdr->timescale) qt_mv_timescale = qt_mvhdr->timescale;
  else qt_mv_timescale = 1000;
  qt_vid_timescale = qt_mv_timescale;

  
  DEBUG_LEVEL2
  { fprintf(stdout,"mv   ver = %x  timescale = %x  duration = %x\n",
	qt_mvhdr->version,qt_mvhdr->timescale, qt_mvhdr->duration);
    fprintf(stdout,"     rate = %x volume = %x  nxt_tk = %x\n",
	qt_mvhdr->rate,qt_mvhdr->volume,qt_mvhdr->nxt_tk_id);
  }
}

void QT_Read_TKHD(xin,qt_tkhdr)
XA_INPUT *xin;
QT_TKHDR *qt_tkhdr;
{ xaULONG i,j;

  qt_tkhdr->version =	xin->Read_MSB_U32(xin);
  qt_tkhdr->creation =	xin->Read_MSB_U32(xin);
  qt_tkhdr->modtime =	xin->Read_MSB_U32(xin);
  qt_tkhdr->trackid =	xin->Read_MSB_U32(xin);
  qt_tkhdr->timescale =	xin->Read_MSB_U32(xin);
  qt_tkhdr->duration =	xin->Read_MSB_U32(xin);
  qt_tkhdr->time_off =	xin->Read_MSB_U32(xin);
  qt_tkhdr->priority  =	xin->Read_MSB_U32(xin);
  qt_tkhdr->layer  =	xin->Read_MSB_U16(xin);
  qt_tkhdr->alt_group = xin->Read_MSB_U16(xin);
  qt_tkhdr->volume  =	xin->Read_MSB_U16(xin);
  for(i=0;i<3;i++) for(j=0;j<3;j++) 
			qt_tkhdr->matrix[i][j]=xin->Read_MSB_U32(xin);
  qt_tkhdr->tk_width =	xin->Read_MSB_U32(xin);
  qt_tkhdr->tk_height =	xin->Read_MSB_U32(xin);
  qt_tkhdr->pad  =	xin->Read_MSB_U16(xin);

  if (qt_tkhdr->timescale) qt_tk_timescale = qt_tkhdr->timescale;
  else qt_tk_timescale = qt_mv_timescale;

  DEBUG_LEVEL2
  { fprintf(stdout,"tk   ver = %x  tk_id = %x  timescale = %x\n",
	qt_tkhdr->version,qt_tkhdr->trackid,qt_tkhdr->timescale);
    fprintf(stdout,"     dur= %x timoff= %x tk_width= %x  tk_height= %x\n",
	qt_tkhdr->duration,qt_tkhdr->time_off,qt_tkhdr->tk_width,qt_tkhdr->tk_height);
  }
}


/* POD TODO */
/* implement difference between start_offset and init_duration */
void QT_Read_ELST(xin,qt_start_offset,qt_init_duration)
XA_INPUT *xin;
xaULONG  *qt_start_offset;
xaULONG  *qt_init_duration;
{ xaULONG i,num,version;

 version = xin->Read_MSB_U32(xin);
 num = xin->Read_MSB_U32(xin);
 DEBUG_LEVEL2 fprintf(stdout,"    ELST ver %x num %x\n",version,num);
 for(i=0; i < num; i++)
 { xaULONG duration,time,rate,pad;
   duration = xin->Read_MSB_U32(xin); 
   time = xin->Read_MSB_U32(xin); 
   rate = xin->Read_MSB_U16(xin); 
   pad  = xin->Read_MSB_U16(xin); 

/* This is currently a kludge with limited support */
   if (i==0)
   {  if (time == 0xffffffff)	*qt_init_duration += duration;
      else if (time != 0x0)	*qt_start_offset += time;
   }
   DEBUG_LEVEL2 fprintf(stdout,"    -) dur %x tim %x rate %x\n",
		duration,time,rate);
 }
}

void QT_Read_MDHD(xin,qt_mdhdr)
XA_INPUT *xin;
QT_MDHDR *qt_mdhdr;
{ qt_mdhdr->version =	xin->Read_MSB_U32(xin);
  qt_mdhdr->creation =	xin->Read_MSB_U32(xin);
  qt_mdhdr->modtime =	xin->Read_MSB_U32(xin);
  qt_mdhdr->timescale =	xin->Read_MSB_U32(xin);
  qt_mdhdr->duration =	xin->Read_MSB_U32(xin);
  qt_mdhdr->language =	xin->Read_MSB_U16(xin);
  qt_mdhdr->quality =	xin->Read_MSB_U16(xin);

  if (qt_mdhdr->timescale) qt_md_timescale = qt_mdhdr->timescale;
  else qt_md_timescale = qt_tk_timescale;

  DEBUG_LEVEL2
  { fprintf(stdout,"md   ver = %x  timescale = %x  duration = %x\n",
	qt_mdhdr->version,qt_mdhdr->timescale,qt_mdhdr->duration);
    fprintf(stdout,"     lang= %x quality= %x\n", 
	qt_mdhdr->language,qt_mdhdr->quality);
  }
}


void QT_Read_HDLR(xin,len,qt_hdlr_hdr)
XA_INPUT *xin;
xaLONG len;
QT_HDLR_HDR *qt_hdlr_hdr;
{ qt_hdlr_hdr->version =	xin->Read_MSB_U32(xin);
  qt_hdlr_hdr->type =	xin->Read_MSB_U32(xin);
  qt_hdlr_hdr->subtype =	xin->Read_MSB_U32(xin);
  qt_hdlr_hdr->vendor =	xin->Read_MSB_U32(xin);
  qt_hdlr_hdr->flags =	xin->Read_MSB_U32(xin);
  qt_hdlr_hdr->mask =	xin->Read_MSB_U32(xin);

  DEBUG_LEVEL2
  { fprintf(stdout,"     ver = %x  ",qt_hdlr_hdr->version);
    QT_Print_ID(stdout,qt_hdlr_hdr->type,1);
    QT_Print_ID(stdout,qt_hdlr_hdr->subtype,1);
    QT_Print_ID(stdout,qt_hdlr_hdr->vendor,0);
    fprintf(stdout,"\n     flags= %x mask= %x\n",
	qt_hdlr_hdr->flags,qt_hdlr_hdr->mask);
  }
  /* Read Handler Name if Present */
  if (len > 32)
  { len -= 32;
    QT_Read_Name(xin,len);
  }
}


/*********************************************
 * Read and Parse Video Codecs
 *
 **********/
xaULONG QT_Read_Video_STSD(anim_hdr,qt,xin,clen)
XA_ANIM_HDR	*anim_hdr;
XA_ANIM_SETUP	*qt;
XA_INPUT		*xin;
xaLONG		clen;
{ xaULONG i,version,num,cur,sup;
  version = xin->Read_MSB_U32(xin);
  num = xin->Read_MSB_U32(xin);
  clen -= 8;
  DEBUG_LEVEL2 fprintf(stdout,"     ver = %x  num = %x\n", version,num);
  if (qtv_codecs == 0)
  { qtv_codec_num = num;
    qtv_codecs = (QTV_CODEC_HDR *)malloc(qtv_codec_num * sizeof(QTV_CODEC_HDR));
    if (qtv_codecs==0) TheEnd1("QT STSD: malloc err");
    cur = 0;
  }
  else
  { QTV_CODEC_HDR *tcodecs;
    tcodecs = (QTV_CODEC_HDR *)malloc((qtv_codec_num+num) * sizeof(QTV_CODEC_HDR));
    if (tcodecs==0) TheEnd1("QT STSD: malloc err");
    for(i=0;i<qtv_codec_num;i++) tcodecs[i] = qtv_codecs[i];
    cur = qtv_codec_num;
    qtv_codec_num += num;
    FREE(qtv_codecs,0x900d);
    qtv_codecs = tcodecs;
  }
  sup = 0;
  for(i=0; i < num; i++)
  {
    sup |= QT_Read_Video_Codec_HDR(anim_hdr,qt, &qtv_codecs[cur], xin, &clen ); 
DEBUG_LEVEL1 fprintf(stdout,"CODEC %d) sup=%d\n",i,sup);
    cur++;
  }
  if (clen)
  { DEBUG_LEVEL1 fprintf(stdout,"QT Video STSD len mismatch %x\n",clen); 
    xin->Seek_FPos(xin,clen,1);
  }

  if (sup == 0) return(xaFALSE);
  qt_video_flag = 1; 
  if ( (qt->pic==0) && (xa_buffer_flag == xaTRUE))
  {
    qt->pic_size = qt->max_imagex * qt->max_imagey;
    if ( (cmap_true_map_flag == xaTRUE) && (qt->depth > 8) )
		qt->pic = (xaUBYTE *) malloc(3 * qt->pic_size);
    else	qt->pic = (xaUBYTE *) malloc( XA_PIC_SIZE(qt->pic_size) );
    if (qt->pic == 0) TheEnd1("QT_Buffer_Action: malloc failed");
  }
  return(xaTRUE);
}


xaULONG QT_Read_Video_Codec_HDR(anim_hdr,qt,c_hdr,xin, clen)
XA_ANIM_HDR *anim_hdr;
XA_ANIM_SETUP *qt;
QTV_CODEC_HDR *c_hdr;
XA_INPUT *xin;
xaLONG		*clen;
{
  xaULONG id;
  xaLONG len,i,codec_ret;
  xaULONG unk_0,unk_1,unk_2,unk_3,unk_4,unk_5,unk_6,unk_7,flag;
  xaULONG vendor,temp_qual,spat_qual,h_res,v_res;
  
  len		= xin->Read_MSB_U32(xin);
  *clen		-= len;
  id 		= xin->Read_MSB_U32(xin);
  unk_0		= xin->Read_MSB_U32(xin);
  unk_1		= xin->Read_MSB_U32(xin);
  unk_2		= xin->Read_MSB_U16(xin);
  unk_3		= xin->Read_MSB_U16(xin);
  vendor	= xin->Read_MSB_U32(xin);
  temp_qual	= xin->Read_MSB_U32(xin);
  spat_qual	= xin->Read_MSB_U32(xin);
  qt->imagex	= xin->Read_MSB_U16(xin);
  qt->imagey	= xin->Read_MSB_U16(xin);
  h_res		= xin->Read_MSB_U16(xin);
  unk_4		= xin->Read_MSB_U16(xin);
  v_res		= xin->Read_MSB_U16(xin);
  unk_5		= xin->Read_MSB_U16(xin);
  unk_6		= xin->Read_MSB_U32(xin);
  unk_7		= xin->Read_MSB_U16(xin);
  QT_Read_Name(xin,32);
  qt->depth	= xin->Read_MSB_U16(xin);
  flag		= xin->Read_MSB_U16(xin);
  len -= 0x56;

  /* init now in case of early out */
  c_hdr->width	= qt->imagex;
  c_hdr->height	= qt->imagey;
  c_hdr->depth	= qt->depth;
  c_hdr->compression = id;
  c_hdr->chdr	= 0;

  if (   (qt->depth == 8) || (qt->depth == 40)
      || (qt->depth == 4) || (qt->depth == 36)
      || (qt->depth == 2) || (qt->depth == 34) ) /* generate colormap */
  {
    if      ((qt->depth & 0x0f) == 0x04) qt->imagec = 16;
    else if ((qt->depth & 0x0f) == 0x02) qt->imagec = 4;
    else				 qt->imagec = 256;

    if ((qt->depth < 32) && (id != QT_raw3))
    {
      QT_Create_Default_Cmap(qt->cmap,qt->imagec);
    }
    else /* grayscale */
    {
      if ((id == QT_jpeg) || (id == QT_cvid) || (id == QT_raw3)) 
		QT_Create_Gray_Cmap(qt->cmap,1,qt->imagec);
      else	QT_Create_Gray_Cmap(qt->cmap,0,qt->imagec);
    }

    if ( !(flag & 0x08) && (len) ) /* colormap isn't default */
    {
      xaULONG start,end,p,r,g,b,cflag;
DEBUG_LEVEL1 fprintf(stdout,"reading colormap. flag %x\n",flag);
      start = xin->Read_MSB_U32(xin); /* is this start or something else? */
      cflag = xin->Read_MSB_U16(xin); /* is this end or total number? */
      end   = xin->Read_MSB_U16(xin); /* is this end or total number? */
      len -= 8;
DEBUG_LEVEL1 fprintf(stdout," start %x end %x cflag %x\n",start,end,cflag);
      for(i = start; i <= end; i++)
      {
        p = xin->Read_MSB_U16(xin); 
        r = xin->Read_MSB_U16(xin); 
        g = xin->Read_MSB_U16(xin); 
        b = xin->Read_MSB_U16(xin);  len -= 8;
        if (cflag & 0x8000) p = i;
        if (p<qt->imagec)
        {
	  qt->cmap[p].red   = r;
	  qt->cmap[p].green = g;
	  qt->cmap[p].blue  = b;
        }
        if (len <= 0) break;
      }
    } 
  }

  if (len) 
  { xin->Seek_FPos(xin,len,1);
    len = 0;
  }

  qt->compression = qt_codec_hdr.compression = id;
  qt_codec_hdr.x = qt->imagex;
  qt_codec_hdr.y = qt->imagey;
  qt_codec_hdr.depth = qt->depth;
  qt_codec_hdr.anim_hdr = (void *)anim_hdr;
  qt_codec_hdr.avi_ctab_flag = xaFALSE;

  /* Query to see if Video Compression is supported or not */
  codec_ret = QT_Video_Codec_Query( &qt_codec_hdr );
  if (codec_ret == CODEC_SUPPORTED)
  {
    qt->imagex		= qt_codec_hdr.x;
    qt->imagey		= qt_codec_hdr.y;
    qt->depth		= qt_codec_hdr.depth;
    qt->compression	= qt_codec_hdr.compression;
    c_hdr->xapi_rev	= qt_codec_hdr.xapi_rev;
    c_hdr->decoder	= qt_codec_hdr.decoder;
    c_hdr->depth 	= qt_codec_hdr.depth;		/* depth */
    c_hdr->dlta_extra	= qt_codec_hdr.extra;
  }
  else /*** Return False if Codec is Unknown or Not Supported */
  /* if (codec_ret != CODEC_SUPPORTED) */
  { char tmpbuf[256];
    if (codec_ret == CODEC_UNKNOWN)
    { xaULONG ii,a[4],dd = qt->compression;
      for(ii=0; ii<4; ii++)
      { a[ii] = dd & 0xff;  dd >>= 8;
        if ((a[ii] < ' ') || (a[ii] > 'z')) a[ii] = '.';
      }
      sprintf(tmpbuf,"Unknown %c%c%c%c(%08x)",
		(char)a[3],(char)a[2],(char)a[1],(char)a[0],qt->compression);
      qt_codec_hdr.description = tmpbuf;
    }
    fprintf(stdout,"  Video Codec: %s",qt_codec_hdr.description);
    fprintf(stdout," not yet supported.(E%x)\n",qt->depth);
/* point 'em to the readme's */
    switch(qt->compression)
    { 
      case QT_iv31: 
      case QT_IV31: 
      case QT_iv32: 
      case QT_IV32: 
      case QT_YVU9: 
      case QT_YUV9: 
        fprintf(stdout,"      To support this Codec please read the file \"indeo.readme\".\n");
        break;
      case QT_cvid: 
      case QT_CVID: 
        fprintf(stdout,"      To support this Codec please read the file \"cinepak.readme\".\n");
        break;
    }
    return(0);
  }
  if (qt->depth == 40) qt->depth = 8; 

  /* Print out Video Codec info */
  if (xa_verbose) fprintf(stdout,"  Video Codec: %s depth=%d\n",
                                qt_codec_hdr.description,qt->depth);

 
  if (qt->depth == 1)
  {
    qt->imagec = 2;
    qt->cmap[0].red = qt->cmap[0].green = qt->cmap[0].blue = 0; 
    qt->cmap[1].red = qt->cmap[1].green = qt->cmap[1].blue = 0xff; 
    qt->chdr = ACT_Get_CMAP(qt->cmap,qt->imagec,0,qt->imagec,0,8,8,8);
  }
  else if ((qt->depth == 8) || (qt->depth == 4))
  {
    qt->chdr = ACT_Get_CMAP(qt->cmap,qt->imagec,0,qt->imagec,0,16,16,16);
  }
  else if ( (qt->depth >= 16) && (qt->depth <= 32) )
  {
    if (   (cmap_true_map_flag == xaFALSE) /* depth 16 and not true_map */
           || (xa_buffer_flag == xaFALSE) )
    {
      if (cmap_true_to_332 == xaTRUE)
		qt->chdr = CMAP_Create_332(qt->cmap,&qt->imagec);
      else	qt->chdr = CMAP_Create_Gray(qt->cmap,&qt->imagec);
    }
    else { qt->imagec = 0; qt->chdr = 0; }
  }
  c_hdr->width	= qt->imagex;
  c_hdr->height	= qt->imagey;
  c_hdr->depth	= qt->depth;
  c_hdr->compression = qt->compression;
  c_hdr->chdr	= qt->chdr;
  if (qt->imagex > qt->max_imagex) qt->max_imagex = qt->imagex;
  if (qt->imagey > qt->max_imagey) qt->max_imagey = qt->imagey;
  return(1);
}

void QT_Read_Name(xin,r_len)
XA_INPUT *xin;
xaLONG r_len;
{
  xaULONG len,d,i;

  len = xin->Read_U8(xin); r_len--;
  if (r_len == 0) r_len = len;
  if (len > r_len) fprintf(stdout,"QT_Name: len(%d) > r_len(%d)\n",len,r_len);
  DEBUG_LEVEL2 fprintf(stdout,"      (%d/%d) ",len,r_len);
  for(i=0;i<r_len;i++)
  {
    d = xin->Read_U8(xin) & 0x7f;
    if (i < len) DEBUG_LEVEL2 fputc(d,stdout);
  }
  DEBUG_LEVEL2 fputc('\n',stdout);
}

/* Time To Sample */
void QT_Read_STTS(xin,len,qt_t2samp_num,qt_t2samps)
XA_INPUT *xin;
xaLONG len;
xaULONG *qt_t2samp_num;
QT_T2SAMP_HDR **qt_t2samps;
{
  xaULONG version,num,i,samp_cnt,duration,cur; 
  xaULONG t2samp_num = *qt_t2samp_num;
  QT_T2SAMP_HDR *t2samps = *qt_t2samps;

  version	= xin->Read_MSB_U32(xin);
  num		= xin->Read_MSB_U32(xin);  len -= 8;
  DEBUG_LEVEL2 fprintf(stdout,"    ver=%x num of entries = %x\n",version,num);
/* POD TEST chunk len/num mismatch - deal with it - ignore given num??*/
  num = len >> 3;
  if (t2samps==0)
  {
    t2samp_num = num;
    t2samps = (QT_T2SAMP_HDR *)malloc(num * sizeof(QT_T2SAMP_HDR));
    if (t2samps==0) TheEnd1("QT_Read_STTS: malloc err");
    cur = 0;
  }
  else
  { QT_T2SAMP_HDR *t_t2samp;
    t_t2samp = (QT_T2SAMP_HDR *)
			malloc((t2samp_num + num) * sizeof(QT_T2SAMP_HDR));
    if (t_t2samp==0) TheEnd1("QT_Read_STTS: malloc err");
    for(i=0;i<t2samp_num;i++) t_t2samp[i] = t2samps[i];
    cur = t2samp_num;
    t2samp_num += num;
    FREE(t2samps,0x900e);
    t2samps = t_t2samp;
  }
  for(i=0;i<num;i++)
  { double ftime;
    samp_cnt	= xin->Read_MSB_U32(xin);
    duration	= xin->Read_MSB_U32(xin);  len -= 8;
    if (duration == 0) duration = 1;
    /* NOTE: convert to 1000ms per second */
    t2samps[cur].cnt = samp_cnt;
    ftime = (1000.0 * (double)(duration)) / (double)(qt_vid_timescale);
    t2samps[cur].time = (xaULONG)(ftime);
    ftime -= (double)(t2samps[cur].time);
    t2samps[cur].timelo = (xaULONG)(ftime * (double)(1<<24));
    DEBUG_LEVEL2 fprintf(stdout,"      %d) samp_cnt=%x duration = %x time %d timelo %d ftime %f\n",
	i,samp_cnt,duration,t2samps[cur].time,t2samps[cur].timelo,ftime);
    cur++;
  }
  *qt_t2samp_num = t2samp_num;
  *qt_t2samps = t2samps;
  while(len > 0) {len--; xin->Read_U8(xin); }
}


/* Sync Sample */
void QT_Read_STSS(xin)
XA_INPUT *xin;
{
  xaULONG version,num,i,j;
  version	= xin->Read_MSB_U32(xin);
  num		= xin->Read_MSB_U32(xin);
  DEBUG_LEVEL2 
  {
    fprintf(stdout,"    ver=%x num of entries = %x\n",version,num);
    j = 0;
    fprintf(stdout,"      ");
  }
  for(i=0;i<num;i++)
  {
    xaULONG samp_num;
    samp_num	= xin->Read_MSB_U32(xin);
    DEBUG_LEVEL2
    {
      fprintf(stdout,"%x ",samp_num); j++;
      if (j >= 8) {fprintf(stdout,"\n      "); j=0; }
    }
  }
  DEBUG_LEVEL2 fprintf(stdout,"\n");
}


/* Sample to Chunk */
void QT_Read_STSC(xin,len,qt_s2chunk_num,qt_s2chunks,
		chunkoff_lstnum,codec_num,codec_lstnum)
XA_INPUT *xin;
xaLONG len;
xaULONG *qt_s2chunk_num;
QT_S2CHUNK_HDR **qt_s2chunks;
xaULONG chunkoff_lstnum,codec_num,codec_lstnum;
{
  xaULONG version,num,i,cur,stsc_type,last;
  xaULONG s2chunk_num = *qt_s2chunk_num;
  QT_S2CHUNK_HDR *s2chunks = *qt_s2chunks;

  version	= xin->Read_MSB_U32(xin);
  num		= xin->Read_MSB_U32(xin);	len -= 16;
  i = (num)?(len/num):(0);
  if (i == 16) 
  { 
    DEBUG_LEVEL2 fprintf(stdout,"STSC: OLD STYLE\n");
    len -= num * 16; stsc_type = 0;
  }
  else 
  { 
    DEBUG_LEVEL2 fprintf(stdout,"STSC: NEW STYLE\n");
    len -= num * 12; stsc_type = 1;
  }

  DEBUG_LEVEL2 fprintf(stdout,"    ver=%x num of entries = %x\n",version,num);
  if (s2chunks == 0)
  {
    s2chunk_num = num;
    s2chunks = (QT_S2CHUNK_HDR *)malloc((num+1) * sizeof(QT_S2CHUNK_HDR));
    cur = 0;
  }
  else
  { QT_S2CHUNK_HDR *ts2c;
    ts2c = (QT_S2CHUNK_HDR *)
	malloc( (s2chunk_num + num + 1) * sizeof(QT_S2CHUNK_HDR));
    for(i=0;i<s2chunk_num;i++) ts2c[i] = s2chunks[i];
    cur = s2chunk_num;
    s2chunk_num += num;
    FREE(s2chunks,0x900f);
    s2chunks = ts2c;
  }
  last = 0;
  for(i=0;i<num;i++)
  {
    xaULONG first_chk,samp_per,chunk_tag;
    if (stsc_type == 0)  /* 4 entries */
    { xaULONG tmp;
      first_chk	= xin->Read_MSB_U32(xin);
      tmp	= xin->Read_MSB_U32(xin);
      samp_per	= xin->Read_MSB_U32(xin);
      chunk_tag	= xin->Read_MSB_U32(xin);
      if (i > 0) s2chunks[cur-1].num   = first_chk - last;
      last = first_chk;
      if (i==(num-1))
      {
	if (qt_stgs_num)
	{
	  s2chunks[cur].num   = (qt_stgs_num - first_chk) + 1;
	  qt_stgs_num = 0;
        }
	else 
	{
	  fprintf(stdout,"STSC: old style but not stgs chunk warning\n");
	  s2chunks[cur].num = 100000;
	}
      }
    }
    else		/* 3 entries */
    {
      first_chk	= xin->Read_MSB_U32(xin);
      samp_per	= xin->Read_MSB_U32(xin);
      chunk_tag	= xin->Read_MSB_U32(xin);
      s2chunks[cur].num   = samp_per;
    }
    DEBUG_LEVEL2 
     fprintf(stdout,"      %d-%d) first_chunk=%x samp_per_chk=%x chk_tag=%x\n",
					i,cur,first_chk,samp_per,chunk_tag);
        /* start at 0 not 1  and account for previous chunks */
    s2chunks[cur].first = first_chk - 1 + chunkoff_lstnum;
    if (chunk_tag > (codec_num - codec_lstnum)) 
	{ samp_per = chunk_tag = 1; }
    s2chunks[cur].tag   = chunk_tag - 1 + codec_lstnum;
    cur++;
  }
  s2chunks[cur].first = 0;
  s2chunks[cur].num   = 0;
  s2chunks[cur].tag   = 0;
  DEBUG_LEVEL2 fprintf(stdout,"    STSC left over %d\n",len);
  while (len > 0) { xin->Read_U8(xin); len--; }
  *qt_s2chunk_num = s2chunk_num;
  *qt_s2chunks = s2chunks;
}

/* Sample Size */
void QT_Read_STSZ(xin,len,qt_samp_num,qt_samp_sizes)
XA_INPUT *xin;
xaLONG len;
xaULONG *qt_samp_num,**qt_samp_sizes;
{
  xaULONG version,samp_size,num,i,cur;
  xaULONG samp_num   = *qt_samp_num;
  xaULONG *samp_sizes = *qt_samp_sizes;

  version	= xin->Read_MSB_U32(xin);
  samp_size	= xin->Read_MSB_U32(xin);
  num		= xin->Read_MSB_U32(xin);
  len = (len - 20) / 4;   /* number of stored samples */
  DEBUG_LEVEL2 fprintf(stdout,"    ver=%x samp_size=%x entries= %x stored entries=%x\n",version,samp_size,num,len);

  if (samp_size == 1) num = 1; /* for AUDIO PODNOTE: rethink this */
  if (samp_sizes == 0)
  {
    samp_num = num;
    samp_sizes = (xaULONG *)malloc(num * sizeof(xaULONG));
    if (samp_sizes == 0) {fprintf(stdout,"malloc err 0\n"); TheEnd();}
    cur = 0;
  }
  else /*TRAK*/
  {
    xaULONG *tsamps;
    tsamps = (xaULONG *)malloc((samp_num + num) * sizeof(xaULONG));
    if (tsamps == 0) {fprintf(stdout,"malloc err 0\n"); TheEnd();}
    for(i=0; i<samp_num; i++) tsamps[i] = samp_sizes[i];
    cur = samp_num;
    samp_num += num;
    FREE(samp_sizes,0x9010);
    samp_sizes = tsamps;
  }
  for(i=0;i<num;i++) 
  {
    if (i < len) samp_sizes[cur] = xin->Read_MSB_U32(xin);
    else if (i==0) samp_sizes[cur] = samp_size;
           else samp_sizes[cur] = samp_sizes[cur-1];
    cur++;
  }
  *qt_samp_num = samp_num;
  *qt_samp_sizes = samp_sizes;
}

/* Chunk Offset */
void QT_Read_STCO(xin,qt_chunkoff_num,qt_chunkoffs)
XA_INPUT *xin;
xaULONG *qt_chunkoff_num;
xaULONG **qt_chunkoffs;
{
  xaULONG version,num,i,cur;
  xaULONG chunkoff_num = *qt_chunkoff_num;
  xaULONG *chunkoffs = *qt_chunkoffs;

  version	= xin->Read_MSB_U32(xin);
  num		= xin->Read_MSB_U32(xin);
  DEBUG_LEVEL2 fprintf(stdout,"    ver=%x entries= %x\n",version,num);
  if (chunkoffs == 0)
  {
    chunkoff_num = num;
    chunkoffs = (xaULONG *)malloc(num * sizeof(xaULONG) );
    cur = 0;
  }
  else
  {
    xaULONG *tchunks;
    tchunks = (xaULONG *)malloc((chunkoff_num + num) * sizeof(xaULONG));
    if (tchunks == 0) {fprintf(stdout,"malloc err 0\n"); TheEnd();}
    for(i=0; i<chunkoff_num; i++) tchunks[i] = chunkoffs[i];
    cur = chunkoff_num;
    chunkoff_num += num;
    FREE(chunkoffs,0x9011);
    chunkoffs = tchunks;
  }
  for(i=0;i<num;i++) {chunkoffs[cur] = xin->Read_MSB_U32(xin); cur++; }
  *qt_chunkoff_num = chunkoff_num;
  *qt_chunkoffs = chunkoffs;
 DEBUG_LEVEL2
 { for(i=0;i<num;i++)  fprintf(stdout,"  STCO %d) %x\n",i,
		chunkoffs[ i ]); 
 }
}



xaULONG QT_Read_Video_Data(qt,xin,anim_hdr)
XA_ANIM_SETUP *qt;
XA_INPUT *xin;
XA_ANIM_HDR *anim_hdr;
{
  xaULONG d,ret,i;
  xaULONG cur_samp,cur_s2chunk,nxt_s2chunk;
  xaULONG cur_t2samp,nxt_t2samp;
  xaULONG tag;
  xaULONG cur_off;
  xaULONG unsupported_flag = xaFALSE;
  XA_ACTION *act;

  qtv_init_duration += qts_start_offset;

  qt->cmap_frame_num = qtv_chunkoff_num / cmap_sample_cnt;

  nxt_t2samp = cur_t2samp = 0;
  if (qtv_t2samps)
  {
    if (xa_jiffy_flag) { qt->vid_time = xa_jiffy_flag; qt->vid_timelo = 0; }
    else
    { 
      qt->vid_time   = qtv_t2samps[cur_t2samp].time;
      qt->vid_timelo = qtv_t2samps[cur_t2samp].timelo;
    }
    nxt_t2samp += qtv_t2samps[cur_t2samp].cnt;
  } else { qt->vid_time = XA_GET_TIME(100); qt->vid_timelo = 0; }

  cur_off=0;
  cur_samp = 0;
  cur_s2chunk = 0;
  nxt_s2chunk = qtv_s2chunks[cur_s2chunk + 1].first;
  tag =  qtv_s2chunks[cur_s2chunk].tag;
  qt->imagex = qtv_codecs[tag].width;
  qt->imagey = qtv_codecs[tag].height;
  qt->depth  = qtv_codecs[tag].depth;
  qt->compression  = qtv_codecs[tag].compression;
  qt->chdr   = qtv_codecs[tag].chdr;

  /* Loop through chunk offsets */
  for(i=0; i < qtv_chunkoff_num; i++)
  {
    xaULONG size,chunk_off,num_samps;
    ACT_DLTA_HDR *dlta_hdr;

    chunk_off =  qtv_chunkoffs[i];

/* survive RPZA despite corruption(offsets commonly corrupted).*/
/* MOVE THIS INTO RPZA DECODE
   check size of RPZA again size in codec.
*/

    if ( (i == nxt_s2chunk) && ((cur_s2chunk+1) < qtv_s2chunk_num) )
    {
      cur_s2chunk++;
      nxt_s2chunk = qtv_s2chunks[cur_s2chunk + 1].first;
    }
    num_samps = qtv_s2chunks[cur_s2chunk].num;

    /* Check tags and possibly move to new codec */
    if (qtv_s2chunks[cur_s2chunk].tag >= qtv_codec_num) 
    {
      fprintf(stdout,"QT Data: Warning stsc chunk invalid %d tag %d\n",
		cur_s2chunk,qtv_s2chunks[cur_s2chunk].tag);
    } 
    else if (qtv_s2chunks[cur_s2chunk].tag != tag)
    {
      tag =  qtv_s2chunks[cur_s2chunk].tag;
      qt->imagex = qtv_codecs[tag].width;
      qt->imagey = qtv_codecs[tag].height;
      qt->depth  = qtv_codecs[tag].depth;
      qt->compression  = qtv_codecs[tag].compression;
      qt->chdr   = qtv_codecs[tag].chdr;
    }

    /* Read number of samples in each chunk */
    cur_off = chunk_off;
    while(num_samps--)
    {

DEBUG_LEVEL2 fprintf(stdout,"T2S: cur-t2 %d cur-smp %d nxt-t2 %d tot t2 %d\n", cur_t2samp,cur_samp,nxt_t2samp,qtv_t2samp_num);
    if ( (cur_samp >= nxt_t2samp) && (cur_t2samp < qtv_t2samp_num) )
    {
      cur_t2samp++;
      if (xa_jiffy_flag) { qt->vid_time = xa_jiffy_flag; qt->vid_timelo = 0; }
      else
      { qt->vid_time   = qtv_t2samps[cur_t2samp].time;
	qt->vid_timelo = qtv_t2samps[cur_t2samp].timelo;
      }
      nxt_t2samp += qtv_t2samps[cur_t2samp].cnt;
    }

      size = qtv_samp_sizes[cur_samp];

      act = ACT_Get_Action(anim_hdr,ACT_DELTA);
      if (xa_file_flag == xaTRUE)
      {
	dlta_hdr = (ACT_DLTA_HDR *) malloc(sizeof(ACT_DLTA_HDR));
	if (dlta_hdr == 0) TheEnd1("QT rle: malloc failed");
	act->data = (xaUBYTE *)dlta_hdr;
	dlta_hdr->flags = ACT_SNGL_BUF;
	dlta_hdr->fsize = size;
	dlta_hdr->fpos  = cur_off;
	if (size > qt->max_fvid_size) qt->max_fvid_size = size;
      }
      else
      {
	d = size + (sizeof(ACT_DLTA_HDR));
	dlta_hdr = (ACT_DLTA_HDR *) malloc( d );
	if (dlta_hdr == 0) TheEnd1("QT rle: malloc failed");
	act->data = (xaUBYTE *)dlta_hdr;
	dlta_hdr->flags = ACT_SNGL_BUF | DLTA_DATA;
	dlta_hdr->fpos = 0; dlta_hdr->fsize = size;
	xin->Seek_FPos(xin,cur_off,0);
	ret = xin->Read_Block(xin, dlta_hdr->data, size);
	if (ret != size) 
        { fprintf(stdout,"QT: video read err\n"); 
          if (qt_frame_cnt)	return(xaTRUE);
	  else			return(xaFALSE); }
      }
      cur_off += size;

      if (qtv_init_duration)
      { xaLONG t_time, t_timelo; double ftime;

        ftime = (1000.0 * (double)(qtv_init_duration)) 
					/ (double)(qtv_tk_timescale);
        t_time = (xaULONG)(ftime);
        ftime -= (double)(t_time);
        t_timelo = (xaULONG)(ftime * (double)(1<<24));
	t_time   += qt->vid_time;
	t_timelo += qt->vid_timelo;
	while(t_timelo >= 1<<24) { t_timelo -= 1<<24; t_time++; }

/* Some Quicktimes have two tracks. One chock full of video and one
 * with a single image and a duration of the entire clip.
 * Unfortunately, by the time XAnim is done with that image, it's
 * too late for the others.
 *
 * TODO: Need a routine to detect overlaps and shorten as necessary.
 *
 */
/*TESTING*/
/* if (t_time > 500) t_time = 0; */

        QT_Add_Frame(t_time,t_timelo,act);
        qtv_init_duration = 0;
      }
      else 
	QT_Add_Frame(qt->vid_time,qt->vid_timelo,act);
      dlta_hdr->xpos = dlta_hdr->ypos = 0;
      dlta_hdr->xsize = qt->imagex;
      dlta_hdr->ysize = qt->imagey;
      dlta_hdr->special = 0;
      if (qtv_codecs[tag].decoder)
      { dlta_hdr->extra = qtv_codecs[tag].dlta_extra;
	dlta_hdr->delta = qtv_codecs[tag].decoder;
	dlta_hdr->xapi_rev = qtv_codecs[tag].xapi_rev;
      }
      else
      {
	if (unsupported_flag == xaFALSE) /* only output single warning */
	{
	  fprintf(stdout,
		"QT: Sections of this movie use an unsupported Video Codec\n");
	  fprintf(stdout," and are therefore NOT viewable.\n");
	  unsupported_flag = xaTRUE;
        }
        act->type = ACT_NOP;
      }
      ACT_Setup_Delta(qt,act,dlta_hdr,xin);
      cur_samp++;
      if (cur_samp >= qtv_samp_num) break;
    } /* end of sample number */
    if (cur_samp >= qtv_samp_num) break;
  } /* end of chunk_offset loop */
  return(xaTRUE);
}



/*********************************************
 * Read and Parse Audio Codecs
 *
 **********/
void QT_Read_Audio_STSD(xin,clen)
XA_INPUT	*xin;
xaLONG	clen;
{ xaULONG i,version,num,cur,sup;
  version	= xin->Read_MSB_U32(xin);
  num		= xin->Read_MSB_U32(xin);
  clen -= 8;
  DEBUG_LEVEL2 fprintf(stdout,"     ver = %x  num = %x\n", version,num);
  if (qts_codecs == 0)
  { qts_codec_num = num;
    qts_codecs = (QTS_CODEC_HDR *)malloc(qts_codec_num 
						* sizeof(QTS_CODEC_HDR));
    if (qts_codecs==0) TheEnd1("QT STSD: malloc err");
    cur = 0;
  }
  else
  { QTS_CODEC_HDR *tcodecs;
    tcodecs = (QTS_CODEC_HDR *)malloc((qts_codec_num + num)
						* sizeof(QTS_CODEC_HDR));
    if (tcodecs==0) TheEnd1("QT STSD: malloc err");
    for(i=0;i<qts_codec_num;i++) tcodecs[i] = qts_codecs[i];
    cur = qts_codec_num;
    qts_codec_num += num;
    FREE(qts_codecs,0x9014);
    qts_codecs = tcodecs;
  }
  sup = 0;
  for(i=0; i < num; i++)
  {
    sup |= QT_Read_Audio_Codec_HDR( &qts_codecs[cur], xin, &clen );
    cur++;
  }
  if (sup == 0)
  {
    if (qt_audio_attempt == xaTRUE) 
    {
      qt_audio_attempt = xaFALSE;
    }
  }
  else qt_audio_flag = 1; 

	/** If Extra bytes, skip over them **/
  if (clen)
  { DEBUG_LEVEL1 fprintf(stdout,"QT Audio STSD len mismatch %x\n",clen); 
    xin->Seek_FPos(xin,clen,1);
  }
}


xaULONG QT_Read_Audio_Codec_HDR(c_hdr,xin, clen)
QTS_CODEC_HDR	*c_hdr;
XA_INPUT		*xin;
xaLONG		*clen;
{ xaULONG len;
  xaULONG ret = 1;
  len			= xin->Read_MSB_U32(xin);
  c_hdr->format		= xin->Read_MSB_U32(xin);
  c_hdr->compression	= XA_AUDIO_INVALID;
  c_hdr->dref_id	= xin->Read_MSB_U32(xin);
  c_hdr->version	= xin->Read_MSB_U32(xin);
  c_hdr->codec_rev	= xin->Read_MSB_U32(xin);
  c_hdr->vendor		= xin->Read_MSB_U32(xin);
  c_hdr->chan_num	= xin->Read_MSB_U16(xin);
  c_hdr->bits_samp	= xin->Read_MSB_U16(xin);

/* Some Mac Shareware program screws up when converting AVI's to QT's */
  if (   (c_hdr->bits_samp == 1)
      && (   (c_hdr->format == QT_twos)
          || (c_hdr->format == QT_raw)
          || (c_hdr->format == QT_raw00)
     )  )                                       c_hdr->bits_samp = 8;

  c_hdr->comp_id	= xin->Read_MSB_U16(xin);
  c_hdr->pack_size	= xin->Read_MSB_U16(xin);
  c_hdr->samp_rate	= xin->Read_MSB_U16(xin);
  c_hdr->pad		= xin->Read_MSB_U16(xin);

  *clen -= 36;

  if (c_hdr->format == QT_twos) c_hdr->compression = XA_AUDIO_SIGNED;
  else if (c_hdr->format == QT_raw) c_hdr->compression = XA_AUDIO_LINEAR;
  else if (c_hdr->format == QT_raw00) c_hdr->compression = XA_AUDIO_LINEAR;
  else if (c_hdr->format == QT_ima4) c_hdr->compression = XA_AUDIO_IMA4;
  else if (c_hdr->format == QT_agsm) c_hdr->compression = XA_AUDIO_GSM;
  else if (c_hdr->format == QT_ulaw)
  {  c_hdr->compression = XA_AUDIO_ULAW;
     c_hdr->bits_samp = 8;  /* why they list it at 16 I have no clue */
  }
  else if (c_hdr->format == QT_MAC3)
  { fprintf(stdout,
	"  Audio Codec: Apple MAC3 not yet supported.\n");
     ret = 0;
  }
  else if (c_hdr->format == QT_MAC6)
  { fprintf(stdout,
	"  Audio Codec: Apple MAC6 not yet supported.\n");
     ret = 0;
  }
  else if (c_hdr->format == QT_QDMC)
  { fprintf(stdout,
	"  Audio Codec: QDesign Music Codec (QDMC) not yet supported.\n");
     ret = 0;
  }
  else if (c_hdr->format == QT_Qclp)
  { fprintf(stdout,
	"  Audio Codec: QualComm PureVoice (Qclp) not yet supported.\n");
     ret = 0;
  }
  else
  { xaULONG t = c_hdr->format;
    fprintf(stdout,"Unknown(and unsupported) Audio Codec: %c%c%c%c(0x%x).\n",
		((t>>24) & 0x7f), ((t>>16) & 0x7f), 
		((t>>8) & 0x7f), (t & 0x7f),		 t);
    ret = 0;
  }

/*** If not supported, then already output ***/
  if (ret & xa_verbose)
  {
    fprintf(stdout,"  Audio Codec: "); QT_Audio_Type(c_hdr->format);
    fprintf(stdout," Rate=%d Chans=%d Bps=%d\n",
	c_hdr->samp_rate,c_hdr->chan_num,c_hdr->bits_samp);
  }

  if (c_hdr->bits_samp==8) c_hdr->bps = 1;
  else if (c_hdr->bits_samp==16) c_hdr->bps = 2;
  else if (c_hdr->bits_samp==32) c_hdr->bps = 4;
  else c_hdr->bps = 100 + c_hdr->bits_samp;

  if (c_hdr->bps > 2) ret = 0;
  if (c_hdr->chan_num > 2) ret = 0;

/* This is only valid for PCM type samples */
  if ( (c_hdr->bps==2)  &&
	 (   ((c_hdr->compression & XA_AUDIO_TYPE_MASK) == XA_AUDIO_LINEAR)
	  || ((c_hdr->compression & XA_AUDIO_TYPE_MASK) == XA_AUDIO_SIGNED)
	 )
     )
  {
    c_hdr->compression |= XA_AUDIO_BPS_2_MSK;
    c_hdr->compression |= XA_AUDIO_BIGEND_MSK; /* only has meaning >= 2 bps */
  }
  if (c_hdr->chan_num==2)
  {
    c_hdr->compression |= XA_AUDIO_STEREO_MSK;
    c_hdr->bps *= 2;
  }

  return(ret);
}

void QT_Audio_Type(type)
xaULONG type;
{ switch(type)
  { case QT_raw:	fprintf(stdout,"PCM"); break;
    case QT_raw00:	fprintf(stdout,"PCM0"); break;
    case QT_twos:	fprintf(stdout,"TWOS"); break;
    case QT_MAC3:	fprintf(stdout,"MAC3"); break;
    case QT_MAC6:	fprintf(stdout,"MAC6"); break;
    case QT_ima4:	fprintf(stdout,"IMA4"); break;
    case QT_ulaw:	fprintf(stdout,"uLaw"); break;
    default:		fprintf(stdout,"(%c%c%c%c)(%08x)",
			  (char)((type>>24)&0xff), (char)((type>>16)&0xff),
			  (char)((type>>8)&0xff),(char)((type)&0xff),type); 
	break;
  }
}

/**************
 *
 * 
 *******/
xaULONG QT_Read_Audio_Data(qt,xin,anim_hdr)
XA_ANIM_SETUP *qt;
XA_INPUT *xin;
XA_ANIM_HDR *anim_hdr;
{
  xaULONG ret,i;
  xaULONG cur_s2chunk,nxt_s2chunk;
  xaULONG tag;

DEBUG_LEVEL1 fprintf(stdout,"QT_Read_Audio: attempt %x co# %d\n",
				qt_audio_attempt,qts_chunkoff_num);

  cur_s2chunk	= 0;
  nxt_s2chunk	= qts_s2chunks[cur_s2chunk + 1].first;
  tag		= qts_s2chunks[cur_s2chunk].tag;
  qt_audio_freq	 = qts_codecs[tag].samp_rate;
  qt_audio_chans = qts_codecs[tag].chan_num;
  qt_audio_bps	 = qts_codecs[tag].bps;
  qt_audio_type	 = qts_codecs[tag].compression;
  qt_audio_end	 = 1;

  /* Initial Silence if any. PODNOTE: Eventually Modify for Middle silence */
  /**** TODO: detect middle silences and add NOP audio chunks ****/
  qts_init_duration += qtv_start_offset;
  if (qts_init_duration)
  { xaULONG snd_size;

    snd_size = (qt_audio_freq * qts_init_duration) / qts_tk_timescale;
    if (XA_Add_Sound(anim_hdr,0,XA_AUDIO_NOP, -1, qt_audio_freq,
	snd_size, &qt->aud_time, &qt->aud_timelo, 0, 0) == xaFALSE) 
							return(xaFALSE);
  }


  /* Loop through chunk offsets */
  for(i=0; i < qts_chunkoff_num; i++)
  { xaULONG size,chunk_off,num_samps,snd_size;
    xaULONG blockalign, sampsblock;

    if ( (i == nxt_s2chunk) && ((cur_s2chunk+1) < qts_s2chunk_num) )
    {
      cur_s2chunk++;
      nxt_s2chunk = qts_s2chunks[cur_s2chunk+1].first;
    }
    num_samps = qts_s2chunks[cur_s2chunk].num; /* * sttz */

    /* Check tags and possibly move to new codec */
    if (qts_s2chunks[cur_s2chunk].tag >= qts_codec_num) 
    {
      fprintf(stdout,"QT Data: Warning stsc chunk invalid %d tag %d\n",
		cur_s2chunk,qts_s2chunks[cur_s2chunk].tag);
    } 
    else if (qts_s2chunks[cur_s2chunk].tag != tag)
    {
      tag =  qts_s2chunks[cur_s2chunk].tag;
      qt_audio_freq  = qts_codecs[tag].samp_rate;
      qt_audio_chans = qts_codecs[tag].chan_num;
      qt_audio_bps   = qts_codecs[tag].bps;
      qt_audio_type  = qts_codecs[tag].compression;
      qt_audio_end   = 1;
    }

/* NOTE: THE STSZ CHUNKS FOR AUDIO IS TOTALLY F****D UP AND INCONSISTENT
 * ACROSS MANY ANIMATIONS. CURRENTLY JUST IGNORE THIS. */
    if (i < qts_samp_num) size = qts_samp_sizes[i];
    else if (qts_samp_num) size = qts_samp_sizes[qts_samp_num-1];
    else size = qt_audio_bps;

    if (size > qt_audio_bps) 
    {
	fprintf(stdout,"QT UNIQE AUDIO: sz %d bps %d\n",size,qt_audio_bps);
    }

    chunk_off =  qts_chunkoffs[i];


/*
fprintf(stdout,"SND: off %08x size %08x\n",chunk_off, size);
*/
    if (qt_audio_type == XA_AUDIO_IMA4_M)
    { xaULONG numblks = num_samps / 0x40;
      snd_size = numblks * 0x22; 
      blockalign = 0x22;
      sampsblock = 0x40;
    }
    else if (qt_audio_type == XA_AUDIO_IMA4_S)
    { xaULONG numblks = num_samps / 0x40;
      snd_size = numblks * 0x44; 
      blockalign = 0x44;
      sampsblock = 0x40;
    }
    else if (qt_audio_type == XA_AUDIO_GSM_M)
    { xaULONG numblks = num_samps / 160;
      snd_size = numblks * 33; 
      blockalign = 33;
      sampsblock = 160;
    }
    else
    { snd_size = num_samps * qt_audio_bps;
      blockalign = qt_audio_bps; /* not really valid yet */
      sampsblock = 0x1;
    }

DEBUG_LEVEL1 fprintf(stdout,"snd_size %x  numsamps %x size %x bps %d off %x\n",
		snd_size,num_samps,size,qt_audio_bps,chunk_off);

    if (xa_file_flag == xaTRUE)
    {
      if (XA_Add_Sound(anim_hdr,0,qt_audio_type, chunk_off, qt_audio_freq,
		snd_size, &qt->aud_time, &qt->aud_timelo, 
		blockalign,sampsblock) == xaFALSE) return(xaFALSE);
      if (snd_size > qt->max_faud_size) qt->max_faud_size = snd_size;
    }
    else
    { xaUBYTE *snd_data = (xaUBYTE *)malloc(snd_size);
      if (snd_data==0) TheEnd1("QT aud_dat: malloc err");
      xin->Seek_FPos(xin,chunk_off,0);  /* move to start of chunk data */
      ret = xin->Read_Block(xin, snd_data, snd_size);
      if (ret != snd_size)
      { fprintf(stdout,"QT: snd rd err\n"); 
        return(xaTRUE);
      }
      if (XA_Add_Sound(anim_hdr,snd_data,qt_audio_type, -1, qt_audio_freq,
		snd_size, &qt->aud_time, &qt->aud_timelo,
		blockalign,sampsblock) == xaFALSE) return(xaFALSE);

    }
  } /* end of chunk_offset loop */
  return(xaTRUE);
}


/********
 * Have No Clue
 *
 ****/
void QT_Read_STGS(xin,len)
XA_INPUT *xin;
xaLONG len;
{
  xaULONG i,version,num;
  xaULONG samps,pad;

  version	= xin->Read_MSB_U32(xin);
  num		= xin->Read_MSB_U32(xin); len -= 16;
  qt_stgs_num = 0;
  for(i=0; i<num; i++)
  {
    samps	= xin->Read_MSB_U32(xin);
    pad		= xin->Read_MSB_U32(xin);	len -= 8;
    qt_stgs_num += samps;
  }
  while(len > 0) {len--; xin->Read_U8(xin); }
}

int qt_2map[] = {
0x93, 0x65, 0x5e,
0xff, 0xff, 0xff,
0xdf, 0xd0, 0xab,
0x00, 0x00, 0x00
};

/* MAX */
int qt_4map[] = {
0xff, 0xfb, 0xff,
0xef, 0xd9, 0xbb,
0xe8, 0xc9, 0xb1,
0x93, 0x65, 0x5e,
0xfc, 0xde, 0xe8,
0x9d, 0x88, 0x91,
0xff, 0xff, 0xff,
0xff, 0xff, 0xff,
0xff, 0xff, 0xff,
0x47, 0x48, 0x37,
0x7a, 0x5e, 0x55,
0xdf, 0xd0, 0xab,
0xff, 0xfb, 0xf9,
0xe8, 0xca, 0xc5,
0x8a, 0x7c, 0x77,
0x00, 0x00, 0x00
};


/**************************************
 * QT_Create_Default_Cmap
 *
 * This routine recreates the Default Apple colormap.
 * It is an educated quess after looking at two quicktime animations
 * and may not be totally correct, but seems to work okay.
 */
void QT_Create_Default_Cmap(cmap,cnum)
ColorReg *cmap;
xaULONG cnum;
{
  xaLONG r,g,b,i;

  if (cnum == 4)
  { 
    for(i=0;i<4;i++)
    { int d = i * 3;
      cmap[i].red   = 0x101 * qt_2map[d];
      cmap[i].green = 0x101 * qt_2map[d+1];
      cmap[i].blue  = 0x101 * qt_2map[d+2];
    }
  }
  else if (cnum == 16)
  { 
    for(i=0;i<16;i++)
    { int d = i * 3;
      cmap[i].red   = 0x101 * qt_4map[d];
      cmap[i].green = 0x101 * qt_4map[d+1];
      cmap[i].blue  = 0x101 * qt_4map[d+2];
    }
  }
  else
  { static xaUBYTE pat[10] = {0xee,0xdd,0xbb,0xaa,0x88,0x77,0x55,0x44,0x22,0x11};
    r = g = b = 0xff;
    for(i=0;i<215;i++)
    {
      cmap[i].red   = 0x101 * r;
      cmap[i].green = 0x101 * g;
      cmap[i].blue  = 0x101 * b;
      b -= 0x33;
      if (b < 0) { b = 0xff; g -= 0x33; if (g < 0) { g = 0xff; r -= 0x33; } }
    }
    for(i=0;i<10;i++)
    { xaULONG d = 0x101 * pat[i];
      xaULONG ip = 215 + i; 
      cmap[ip].red   = d; cmap[ip].green = cmap[ip].blue  = 0; ip += 10;
      cmap[ip].green = d; cmap[ip].red   = cmap[ip].blue  = 0; ip += 10;
      cmap[ip].blue  = d; cmap[ip].red   = cmap[ip].green = 0; ip += 10;
      cmap[ip].red   = cmap[ip].green = cmap[ip].blue  = d;
    }
    cmap[255].red = cmap[255].green = cmap[255].blue  = 0x00;
  }
}
   

/**************************************
 * QT_Create_Gray_Cmap
 *
 * This routine recreates the Default Gray Apple colormap.
 */
void QT_Create_Gray_Cmap(cmap,flag,num)
ColorReg *cmap;
xaULONG flag;	/* flag=0  0=>255;  flag=1 255=>0 */
xaULONG num;	/* size of color map */
{
  xaLONG g,i;

  if (num == 256)
  {
    g = (flag)?(0x00):(0xff);
    for(i=0;i<256;i++)
    {
      cmap[i].red   = 0x101 * g;
      cmap[i].green = 0x101 * g;
      cmap[i].blue  = 0x101 * g;
      cmap[i].gray  = 0x101 * g;
      if (flag) g++; else g--;
    }
  }
  else if (num == 16)
  {
    g = 0xf;
    for(i=0;i<16;i++)
    {
      cmap[i].red   = 0x1111 * g;
      cmap[i].green = 0x1111 * g;
      cmap[i].blue  = 0x1111 * g;
      cmap[i].gray  = 0x1111 * g;
      g--;
    }
  }
}



