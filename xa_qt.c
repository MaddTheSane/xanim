
/*
 * xa_qt.c
 *
 * Copyright (C) 1993,1994,1995 by Mark Podlipec.
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
 * 31Aug94  RPZA was using *iptr+=row_inc instead of iptr+=row_inc.
 * 15Sep94  Added support for RAW32 format. straight RGB with 0x00 up front.
 * 19Sep94  Fixed stereo audio bug. Needed to double bps with stereo snd.
 * 20Sep94  I forgot to declare fin in the QT_Read_Audio_STSD(fin) and
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
 */
#include "xa_qt.h"
#include "xa_codecs.h"

XA_CODEC_HDR qt_codec_hdr;

/******************** Radius Cinepak External Library XAnim funtions ********/
#ifdef XA_CINEPAK
extern xaULONG  Cinepak_What_Rev_API();
extern xaLONG   Cinepak_Codec_Query();
#define XA_CINEPAK_QUERY_CNT 1
#else
#define XA_CINEPAK_QUERY_CNT 0
#endif


/******************** Intel Indeo External Library XAnim funtions ***********/
#ifdef XA_INDEO
extern xaULONG  Indeo_What_Rev_API();
extern xaLONG  Indeo_Codec_Query();
#define XA_INDEO_QUERY_CNT 1
#else
#define XA_INDEO_QUERY_CNT 0
#endif

xaLONG QT_Codec_Query();
xaLONG QT_UNK_Codec_Query();

#define QT_QUERY_CNT 2 + XA_INDEO_QUERY_CNT + XA_CINEPAK_QUERY_CNT

xaLONG (*qt_query_func[])() = { 
#ifdef XA_INDEO
		Indeo_Codec_Query,
#endif
#ifdef XA_CINEPAK
		Cinepak_Codec_Query,
#endif
		QT_Codec_Query,
		QT_UNK_Codec_Query};


xaULONG QT_Decode_RLE();
xaULONG QT_Decode_RLE16();
xaULONG QT_Decode_RLE24();
xaULONG QT_Decode_RLE32();
xaULONG QT_Decode_RLE1();
xaULONG QT_Decode_RAW();
xaULONG QT_Decode_RAW4();
xaULONG QT_Decode_RAW16();
xaULONG QT_Decode_RAW24();
xaULONG QT_Decode_RAW32();
xaULONG QT_Decode_RPZA();
xaULONG QT_Decode_SMC();
xaULONG QT_Decode_YUV2();
/* xaULONG QT_Decode_SPIG(); */
xaULONG JFIF_Decode_JPEG();
xaULONG QT_Read_Video_Codec_HDR();
xaULONG QT_Read_Audio_Codec_HDR();
void QT_Audio_Type();
xaULONG QT_Read_File();

extern void yuv_to_rgb();
extern void XA_Gen_YUV_Tabs();

void QT_Create_Default_Cmap();
void QT_Create_Gray_Cmap();
extern char *XA_rindex();

void CMAP_Cache_Clear();
void CMAP_Cache_Init();


/* JPEG and other assist routines */
extern void JPG_Alloc_MCU_Bufs();
extern void JPG_Setup_Samp_Limit_Table();

/* POD NOTE: testing purposes */
extern xaULONG pod;
extern xaLONG xa_dither_flag;

#define SMC_MAX_CNT 256
/* POD NOTE: eventually make these conditionally dynamic */
static xaULONG smc_8cnt,smc_Acnt,smc_Ccnt;
static xaULONG smc_8[ (2 * SMC_MAX_CNT) ];
static xaULONG smc_A[ (4 * SMC_MAX_CNT) ];
static xaULONG smc_C[ (8 * SMC_MAX_CNT) ];

QTV_CODEC_HDR *qtv_codecs;
QTS_CODEC_HDR *qts_codecs;
xaULONG qtv_codec_num,qts_codec_num;

XA_ACTION *ACT_Get_Action();
XA_CHDR *ACT_Get_CMAP();
XA_CHDR *CMAP_Create_332();
XA_CHDR *CMAP_Create_422();
XA_CHDR *CMAP_Create_Gray();
void ACT_Add_CHDR_To_Action();
void ACT_Setup_Mapped();
XA_CHDR *CMAP_Create_CHDR_From_True();
xaUBYTE *UTIL_RGB_To_FS_Map();
xaUBYTE *UTIL_RGB_To_Map();
xaULONG CMAP_Find_Closest();
xaULONG UTIL_Get_MSB_Long();
xaLONG UTIL_Get_MSB_Short();
xaULONG UTIL_Get_MSB_UShort();
extern XA_ANIM_SETUP *XA_Get_Anim_Setup();
void XA_Free_Anim_Setup();


void QT_Get_Dith1_Color24();
void QT_Get_Dith4_Color24();
 

FILE *QT_Open_File();
xaULONG QT_Parse_Chunks();
xaULONG QT_Parse_Bin();
xaULONG QT_Read_Video_Data();
void  QT_Read_Audio_Data();

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
xaULONG QT_Get_Color();
void QT_Get_RGBColor();
void QT_Get_AV_Colors();
void QT_Get_AV_RGBColors();
xaULONG QT_Get_DithColor24();

extern xaULONG XA_RGB24_To_CLR32();


QT_MVHDR qt_mvhdr;
QT_TKHDR qt_tkhdr;
QT_MDHDR qt_mdhdr;
QT_HDLR_HDR qt_hdlr_hdr;

char qt_rfilename[256];
char qt_dfilename[256];
xaULONG qt_video_flag;
xaULONG qt_data_flag;
xaULONG qt_v_flag,qt_s_flag;
xaULONG qt_moov_flag;

xaUSHORT qt_gamma_adj[32];

xaULONG qt_frame_cnt;
xaULONG qt_vid_timescale,qt_mv_timescale,qt_tk_timescale,qt_md_timescale;

#define QT_CODEC_UNK   0x000

/*** SOUND SUPPORT ****/
xaULONG qt_audio_attempt;
xaULONG qt_audio_type;
xaULONG qt_audio_freq;
xaULONG qt_audio_chans;
xaULONG qt_audio_bps;
xaULONG qt_audio_end;
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
  FILE *fin;
  xaULONG ret;

  if ( (fin=QT_Open_File(filename,qt_rfilename,qt_dfilename)) == 0)
				return(xaNOFILE);
  ret = QT_Parse_Bin(fin);
  fclose(fin);
  if ( ret != 0 ) return(xaTRUE);
  return(xaFALSE);
}

/* FOR PARSING Quicktime Files */
xaULONG *qtv_samp_sizes,*qts_samp_sizes;
xaULONG qtv_samp_num,qts_samp_num;
xaULONG qts_init_duration;

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

/* main() */
xaULONG QT_Read_File(fname,anim_hdr,audio_attempt)
char *fname;
XA_ANIM_HDR *anim_hdr;
xaULONG audio_attempt;  /* xaTRUE is audio is to be attempted */
{
  FILE *fin;
  xaLONG i,t_time;
  xaULONG t_timelo;
  XA_ANIM_SETUP *qt;


  qt = XA_Get_Anim_Setup();
  qt->vid_time 		= XA_GET_TIME( 100 ); /* default 10 fps */
  qt->compression	= QT_CODEC_UNK;


  qt_has_ctab = xaFALSE;
  qt_stgs_num = 0;
  qtv_codec_lstnum	= qts_codec_lstnum = 0;
  qtv_chunkoff_lstnum	= qts_chunkoff_lstnum = 0;
  qtv_samp_lstnum	= qts_samp_lstnum = 0;
  qtv_codecs = 0;
  qts_codecs = 0;
  qtv_codec_num 	= qts_codec_num = 0;
  qt_data_flag = xaFALSE;
  qt_video_flag		= 0;
  qt_v_flag		= qt_s_flag = 0;
  qt_moov_flag = xaFALSE;

  qt_frame_cnt = 0;
  qt_frame_start = 0;
  qt_frame_cur = 0;

  qt_vid_timescale		= 1000;
  qt_mv_timescale = qt_tk_timescale = qt_md_timescale = 1000;
  qtv_chunkoff_num	= qts_chunkoff_num = 0;
  qtv_chunkoffs		= qts_chunkoffs = 0;
  qtv_s2chunk_num	= qts_s2chunk_lstnum = 0;
  qtv_s2chunks		= qts_s2chunks = 0;
  qtv_s2chunk_num	= qts_s2chunk_lstnum = 0;
  qtv_t2samp_num	= qts_t2samp_num = 0;
  qtv_t2samps		= qts_t2samps = 0;
  qtv_samp_sizes	= qts_samp_sizes = 0;
  qtv_samp_num		= qts_samp_num = 0;
  qts_init_duration	= 0;

  qt_audio_attempt	= audio_attempt;

  for(i=0;i<32;i++) qt_gamma_adj[i] = xa_gamma_adj[ ((i<<3)|(i>>2)) ];

  if ( (fin=QT_Open_File(fname,qt_rfilename,qt_dfilename)) == 0)
  {
    fprintf(stderr,"QT_Read: can't open %s\n",qt_rfilename);
    XA_Free_Anim_Setup(qt);
    return(xaFALSE);
  }

  if ( QT_Parse_Bin(fin) == 0 )
  {
    fprintf(stderr,"Not quicktime file\n");
    XA_Free_Anim_Setup(qt);
    return(xaFALSE);
  }

  if (QT_Parse_Chunks(anim_hdr,qt,fin) == xaFALSE)
  {
    QT_Free_Stuff();
    XA_Free_Anim_Setup(qt);
    return(xaFALSE);
  }

  if (qt_data_flag == xaFALSE) 
  { /* mdat was not in .rscr file need to open .data file */
    fclose(fin); /* close .rscr file */
    if (qt_dfilename[0] == 0)
    {
       fprintf(stderr,"QT_Data: No data in %s file. Can't find .data file.\n",
		qt_rfilename);
       return(xaFALSE);
    }
    if ( (fin=fopen(qt_dfilename,XA_OPEN_MODE)) == 0) 
    {
      fprintf(stderr,"QT_Data: can't open %s file.\n",qt_dfilename);
      return(xaFALSE);
    }
  } else strcpy(qt_dfilename,qt_rfilename); /* r file is d file */
DEBUG_LEVEL1 fprintf(stderr,"reading data\n");
  if (QT_Read_Video_Data(qt,fin,anim_hdr) == xaFALSE) return(xaFALSE);
  QT_Read_Audio_Data(qt,fin,anim_hdr);
  fclose(fin);

  if (qt_frame_cnt == 0) 
  {
    if (qt_moov_flag==xaTRUE)
	 fprintf(stderr,"QT: file possibly truncated.\n");
    else fprintf(stderr,"QT: file possibly truncated or missing .rsrc info.\n");
    return(xaFALSE);
  }

  anim_hdr->frame_lst = (XA_FRAME *)
                                malloc( sizeof(XA_FRAME) * (qt_frame_cnt+1));
  if (anim_hdr->frame_lst == NULL) TheEnd1("QT_Read_File: frame malloc err");
 
  qt_frame_cur = qt_frame_start;
  i = 0;
  t_time = 0;
  t_timelo = 0;
  while(qt_frame_cur != 0)
  {
    if (i >= qt_frame_cnt)
    {
      fprintf(stderr,"QT_Read_Anim: frame inconsistency %ld %ld\n",
                i,qt_frame_cnt);
      break;
    }
    anim_hdr->frame_lst[i].time_dur = qt_frame_cur->time;
    anim_hdr->frame_lst[i].zztime = t_time;
DEBUG_LEVEL2 fprintf(stderr,"QT: Vid Time  zz %ld dur %ld\n",
	t_time, qt_frame_cur->time);
    t_time += qt_frame_cur->time;
    t_timelo += qt_frame_cur->timelo;
    while(t_timelo > (1<<24)) {t_time++; t_timelo -= (1<<24);}
    anim_hdr->frame_lst[i].act = qt_frame_cur->act;
    qt_frame_cur = qt_frame_cur->next;
    i++;
  }
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

  if (xa_verbose)
  { fprintf(stderr, "  Frame Stats: Size=%ldx%ld  Frames=%ld",
				qt->imagex,qt->imagey,qt_frame_cnt);
    if (anim_hdr->total_time) 
    { float fps = (float)(1000 * qt_frame_cnt)/(float)(anim_hdr->total_time);
      fprintf(stderr, "  avfps=%2.1f\n",fps);
    } 
    else fprintf(stderr,"\n");
  }

  anim_hdr->imagex = qt->max_imagex;
  anim_hdr->imagey = qt->max_imagey;
  anim_hdr->imagec = qt->imagec;
  anim_hdr->imaged = 8; /* nop */
  anim_hdr->frame_lst[i].time_dur = 0;
  anim_hdr->frame_lst[i].zztime = -1;
  anim_hdr->frame_lst[i].act  = 0;
  anim_hdr->loop_frame = 0;
  if (xa_buffer_flag == xaFALSE) anim_hdr->anim_flags |= ANIM_SNG_BUF;
  anim_hdr->max_fvid_size = qt->max_fvid_size;
  anim_hdr->max_faud_size = qt->max_faud_size;
  if (xa_file_flag == xaTRUE) 
  {
    xaULONG len;
    anim_hdr->anim_flags |= ANIM_USE_FILE;
    len = strlen(qt_dfilename) + 1;
    anim_hdr->fname = (char *)malloc(len);
    if (anim_hdr->fname==0) TheEnd1("QT: malloc fname err");
    strcpy(anim_hdr->fname,qt_dfilename);
  }
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

FILE *QT_Open_File(fname,r_file,d_file)
char *fname,*r_file,*d_file;
{
  FILE *fin;

  /* check to see if fname exists? */
  if ( (fin=fopen(fname,XA_OPEN_MODE)) != 0)  /* filename is as give */
  { /*three choices - with or without .rsrc ending, or using .resource subdir*/
    xaLONG len;
    FILE *ftst;
    /* path/fname exits. */

    /* check for  path/.resource/fname */
    {
      char *lastdirsep;
      strcpy(r_file,fname);			/* copy path/fname to r */
      lastdirsep = XA_rindex(r_file, '/');	/* find sep if any */
      if ( lastdirsep != (char *)(NULL) )
      {
        strcpy(d_file,lastdirsep);		/* save fname to d*/
	lastdirsep++; *lastdirsep = 0;		/* cut of fname off r*/
/*POD NOTE: eventually might want to check for both, but for now
 *          let's just wait and see. */
#ifdef sony
	/* For AppleTalk of NEWS-OS, .rsrc file is in .afprsrc directory */
	strcat(lastdirsep, ".afprsrc/");	/* add .afprsrc to r*/
#else
	strcat(lastdirsep, ".resource/");     /* add .resource to r*/
#endif
        strcat(r_file, d_file); 		/* add fname to r */
      }
      else /* no path */
      {
#ifdef sony
	strcpy(r_file,".afprsrc/");
#else
	strcpy(r_file,".resource/");
#endif
	strcat(r_file,fname);
      }
      if ( (ftst=fopen(r_file,"r")) != 0)
      {
	/* path/fname and path/.resource/fname exist - wrap it up */
	strcpy(d_file,fname);			/* setup .data name */
	fclose(fin);				/* close .data fork */
        return(ftst);		/* return .rsrc fork (in .resource) */
      }
    }
     
    /* Now check for .rsrc or .data endings */
    strcpy(r_file,fname);
    strcpy(d_file,fname);
    len = strlen(r_file) - 5;
    if (len > 0)
    { char *tmp;
      tmp = XA_rindex(d_file, '.');	/* get last "." */
      if ( tmp == (char *)(NULL) ) { *d_file = 0; return(fin); }
      else if (strcmp(tmp,".rsrc")==0)  /* fname has .rsrc ending */
      {
        strcpy(tmp,".data"); /* overwrite .rsrc with .data in d*/
	return(fin);
      }
      else if (strcmp(tmp,".data")==0)  /* fname has .data ending */
      {
        strcpy(tmp,".rsrc"); /* overwrite .rsrc with .data in d*/
        if ( (ftst=fopen(d_file,"r")) != 0) /* see if .rsrc exists */
	{
	  char t_file[256];  /* swap r and d files */
	  strcpy(t_file,r_file); strcpy(r_file,d_file); strcpy(d_file,t_file);
	  fclose(fin);		/* close .data file */
	  return(ftst);		/* return .rsrc file */
	}
	/* hopefully .data is flattened. find out later */
	else { *d_file = 0; return(fin); }
      }
      else { *d_file = 0; return(fin); }
    }
    else { *d_file = 0; return(fin); }
  }

  /* does fname.rsrc exist? */
  strcpy(r_file,fname);
  strcat(r_file,".rsrc");
  if ( (fin=fopen(r_file,XA_OPEN_MODE)) != 0)  /* fname.rsrc */
  { FILE *ftst;
    /* if so, check .data existence */
    strcpy(d_file,fname);
    strcat(d_file,".data");
    if ( (ftst=fopen(d_file,XA_OPEN_MODE)) != 0)	fclose(ftst);
    else *d_file = 0;
    return(fin);
  } else *d_file = 0;
  return(0);
}

xaULONG QT_Parse_Chunks(anim_hdr,qt,fin)
XA_ANIM_HDR *anim_hdr;
XA_ANIM_SETUP *qt;
FILE *fin;
{
  xaLONG file_len;
  xaULONG id,len;

  file_len = 1;
  while(file_len > 0)
  {
    len = UTIL_Get_MSB_Long(fin);
    id  = UTIL_Get_MSB_Long(fin);

    if ( (len == 0) && (id == QT_mdat) )
    {
      fprintf(stderr,"QT: mdat len is 0. You also need a .rsrc file.\n");
      return(xaFALSE);
    }
    if (len < 8) { file_len = 0; continue; } /* just bad - finish this */
    if (file_len == 1)
    {
      if (id == QT_moov) file_len = len;
      else file_len = len + 1;
    }
    DEBUG_LEVEL2 fprintf(stderr,"%c%c%c%c %04lx len = %lx file_len =  %lx\n",
	(id >> 24),((id>>16)&0xff),((id>>8)&0xff),(id&0xff),id,len,file_len);

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
	QT_Read_MVHD(fin,&qt_mvhdr);
	file_len -= len;
	break;
      case QT_tkhd:
	QT_Read_TKHD(fin,&qt_tkhdr);
	file_len -= len;
	break;
      case QT_elst:
	QT_Read_ELST(fin);
	file_len -= len;
	break;
      case QT_mdhd:
	QT_Read_MDHD(fin,&qt_mdhdr);
	file_len -= len;
	break;
      case QT_hdlr:
	QT_Read_HDLR(fin,len,&qt_hdlr_hdr);
	file_len -= len;
	break;
    /*--------------DATA CHUNKS-------------*/
      case QT_mdat:  /* data is included in .rsrc - assumed flatness */
	fseek(fin,(len-8),1); /* skip over it for now */
	qt_data_flag = xaTRUE;
	break;
      case QT_stsd:
	qt_video_flag = 0;
	if (qt_v_flag) 
	{
	  if (QT_Read_Video_STSD(anim_hdr,qt,fin) == xaFALSE) return(xaFALSE);
	}
	else if (qt_s_flag) QT_Read_Audio_STSD(fin);
        else fseek(fin,(len-8),1);
	file_len -= len;
	break;
      case QT_stts:
	if (qt_v_flag) 
		QT_Read_STTS(fin,(len-8),&qtv_t2samp_num,&qtv_t2samps);
/*POD NOTE: AUDIO don't need? probably, just haven't been bit by it yet.
	else if (qt_s_flag) 
		QT_Read_STTS(fin,(len-8),&qts_t2samp_num,&qts_t2samps);
*/
        else	fseek(fin,(len-8),1);
	file_len -= len;
	break;
      case QT_stss:
	QT_Read_STSS(fin);
	file_len -= len;
	break;
      case QT_stco:
	if (qt_v_flag) QT_Read_STCO(fin,&qtv_chunkoff_num,&qtv_chunkoffs);
	else if (qt_s_flag) QT_Read_STCO(fin,&qts_chunkoff_num,&qts_chunkoffs);
	else fseek(fin,(len-8),1);
	file_len -= len;
	break;
      case QT_stsz:
	if (qt_v_flag) QT_Read_STSZ(fin,len,&qtv_samp_num,&qtv_samp_sizes);
	else if (qt_s_flag) 
		QT_Read_STSZ(fin,len,&qts_samp_num,&qts_samp_sizes);
	else fseek(fin,(len-8),1);
	file_len -= len;
	break;
      case QT_stsc:
	if (qt_v_flag) QT_Read_STSC(fin,len,&qtv_s2chunk_num,&qtv_s2chunks,
			qtv_chunkoff_lstnum,qtv_codec_num,qtv_codec_lstnum);
	else if (qt_s_flag) QT_Read_STSC(fin,len,&qts_s2chunk_num,&qts_s2chunks,
			qts_chunkoff_lstnum,qts_codec_num,qts_codec_lstnum);
	else fseek(fin,(len-8),1);
	file_len -= len;
	break;
      case QT_stgs:
	QT_Read_STGS(fin,len);
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
	fprintf(stderr,"QT: Warning %08lx\n",id);
        fseek(fin,(len-8),1);  /* skip over */
	file_len -= len;
	break;
    /*-----------TYPE OF TRAK---------------*/
      case QT_vmhd:
        fseek(fin,(len-8),1);
	file_len -= len; qt_v_flag = 1;
	qt_vid_timescale = qt_md_timescale;
	break;
      case QT_smhd:
        fseek(fin,(len-8),1);
	file_len -= len; qt_s_flag = 1;
	break;
    /************ CTAB ******************
     * Color Table to be used for display 16/24 bit animations on
     * 8 Bit displays.
     *************************************/
      case QT_ctab:
	{ xaULONG i,tmp,start,end;
          if (x11_display_type != XA_PSEUDOCOLOR) break;
fprintf(stderr,"QT: has ctab\n");
          tmp   = UTIL_Get_MSB_Long(fin);  /* ?? */
          start = UTIL_Get_MSB_UShort(fin);  /* start */
          end   = UTIL_Get_MSB_UShort(fin);  /* end */
	  len -= 8;
	  for(i=start; i <= end; i++)
	  { xaULONG idx,r,g,b;
	    idx = UTIL_Get_MSB_UShort(fin);
	    r   = UTIL_Get_MSB_UShort(fin);
	    g   = UTIL_Get_MSB_UShort(fin);
	    b   = UTIL_Get_MSB_UShort(fin);  len -= 8;
	    /* if (cflag & 0x8000)  idx = i; */
	    if (idx < qt->imagec)
	    {
	      qt->cmap[idx].red   = r;
	      qt->cmap[idx].green = g;
	      qt->cmap[idx].blue  = b;
	    }
	    if (len <= 0) break;
	  } /* end of for i */
	  while(len > 0) {fgetc(fin); len--; } 
	  qt->imagec = 256;
	  qt->chdr = ACT_Get_CMAP(qt->cmap,qt->imagec,0,qt->imagec,0,8,8,8);
	  qt_has_ctab = xaTRUE;
	}
	break;
    /*--------------IGNORED FOR NOW---------*/
      case QT_gmhd:
      case QT_text:
      case QT_clip:
      case QT_skip:
      case QT_udta:
      case QT_dinf:
        fseek(fin,(len-8),1);  /* skip over */
	file_len -= len;
	break;
    /*--------------UNKNOWN-----------------*/
      default:
	if ( !feof(fin) && (len <= file_len) )
	{
	  xaLONG i;
	  QT_Print_ID(stderr,id,1);
	  fprintf(stderr," len = %lx\n",len);
	  i = (xaLONG)(len) - 8;
	  while(i > 0) { i--; getc(fin); if (feof(fin)) i = 0; }
	}
	file_len -= len;
	break;
    } /* end of switch */
    if ( feof(fin) ) file_len = 0;
  } /* end of while */
  return(xaTRUE);
}

void QT_Print_ID(fout,id,flag)
FILE *fout;
xaLONG id,flag;
{
 fprintf(fout,"%c",     ((id >> 24) & 0xff));
 fprintf(fout,"%c",     ((id >> 16) & 0xff));
 fprintf(fout,"%c",     ((id >>  8) & 0xff));
 fprintf(fout,"%c",      (id        & 0xff));
 if (flag) fprintf(fout,"(%lx)",id);
}


void QT_Read_MVHD(fin,qt_mvhdr)
FILE *fin;
QT_MVHDR *qt_mvhdr;
{
  xaULONG i,j;

  qt_mvhdr->version =	UTIL_Get_MSB_Long(fin);
  qt_mvhdr->creation =	UTIL_Get_MSB_Long(fin);
  qt_mvhdr->modtime =	UTIL_Get_MSB_Long(fin);
  qt_mvhdr->timescale =	UTIL_Get_MSB_Long(fin);
  qt_mvhdr->duration =	UTIL_Get_MSB_Long(fin);
  qt_mvhdr->rate =	UTIL_Get_MSB_Long(fin);
  qt_mvhdr->volume =	UTIL_Get_MSB_UShort(fin);
  qt_mvhdr->r1  =	UTIL_Get_MSB_Long(fin);
  qt_mvhdr->r2  =	UTIL_Get_MSB_Long(fin);
  for(i=0;i<3;i++) for(j=0;j<3;j++) 
	qt_mvhdr->matrix[i][j]=UTIL_Get_MSB_Long(fin);
  qt_mvhdr->r3  =	UTIL_Get_MSB_UShort(fin);
  qt_mvhdr->r4  =	UTIL_Get_MSB_Long(fin);
  qt_mvhdr->pv_time =	UTIL_Get_MSB_Long(fin);
  qt_mvhdr->post_time =	UTIL_Get_MSB_Long(fin);
  qt_mvhdr->sel_time =	UTIL_Get_MSB_Long(fin);
  qt_mvhdr->sel_durat =	UTIL_Get_MSB_Long(fin);
  qt_mvhdr->cur_time =	UTIL_Get_MSB_Long(fin);
  qt_mvhdr->nxt_tk_id =	UTIL_Get_MSB_Long(fin);

  if (qt_mvhdr->timescale) qt_mv_timescale = qt_mvhdr->timescale;
  else qt_mv_timescale = 1000;
  qt_vid_timescale = qt_mv_timescale;
  
  DEBUG_LEVEL2
  {
    fprintf(stderr,"mv   ver = %lx  timescale = %lx  duration = %lx\n",
	qt_mvhdr->version,qt_mvhdr->timescale, qt_mvhdr->duration);
    fprintf(stderr,"     rate = %lx volumne = %lx  nxt_tk = %lx\n",
	qt_mvhdr->rate,qt_mvhdr->volume,qt_mvhdr->nxt_tk_id);
  }
}

void QT_Read_TKHD(fin,qt_tkhdr)
FILE *fin;
QT_TKHDR *qt_tkhdr;
{
  xaULONG i,j;

  qt_tkhdr->version =	UTIL_Get_MSB_Long(fin);
  qt_tkhdr->creation =	UTIL_Get_MSB_Long(fin);
  qt_tkhdr->modtime =	UTIL_Get_MSB_Long(fin);
  qt_tkhdr->trackid =	UTIL_Get_MSB_Long(fin);
  qt_tkhdr->timescale =	UTIL_Get_MSB_Long(fin);
  qt_tkhdr->duration =	UTIL_Get_MSB_Long(fin);
  qt_tkhdr->time_off =	UTIL_Get_MSB_Long(fin);
  qt_tkhdr->priority  =	UTIL_Get_MSB_Long(fin);
  qt_tkhdr->layer  =	UTIL_Get_MSB_UShort(fin);
  qt_tkhdr->alt_group = UTIL_Get_MSB_UShort(fin);
  qt_tkhdr->volume  =	UTIL_Get_MSB_UShort(fin);
  for(i=0;i<3;i++) for(j=0;j<3;j++) 
			qt_tkhdr->matrix[i][j]=UTIL_Get_MSB_Long(fin);
  qt_tkhdr->tk_width =	UTIL_Get_MSB_Long(fin);
  qt_tkhdr->tk_height =	UTIL_Get_MSB_Long(fin);
  qt_tkhdr->pad  =	UTIL_Get_MSB_UShort(fin);

  if (qt_tkhdr->timescale) qt_tk_timescale = qt_tkhdr->timescale;
  else qt_tk_timescale = qt_mv_timescale;

  DEBUG_LEVEL2
  {
    fprintf(stderr,"tk   ver = %lx  tk_id = %lx  timescale = %lx\n",
	qt_tkhdr->version,qt_tkhdr->trackid,qt_tkhdr->timescale);
    fprintf(stderr,"     dur= %lx timoff= %lx tk_width= %lx  tk_height= %lx\n",
	qt_tkhdr->duration,qt_tkhdr->time_off,qt_tkhdr->tk_width,qt_tkhdr->tk_height);
  }
}


void QT_Read_ELST(fin)
FILE *fin;
{
 xaULONG num,version;

 if (qts_samp_num==0) qts_init_duration = 0;
 version = UTIL_Get_MSB_Long(fin);
 num = UTIL_Get_MSB_Long(fin);
 DEBUG_LEVEL2 fprintf(stderr,"    ELST ver %lx num %lx\n",version,num);
 while(num--)
 {
   xaULONG duration,time,rate,pad;
   duration = UTIL_Get_MSB_Long(fin); 
   time = UTIL_Get_MSB_Long(fin); 
   rate = UTIL_Get_MSB_UShort(fin); 
   pad  = UTIL_Get_MSB_UShort(fin); 
   /* if 1st audio doesn't start at time 0 */
   /* NOTE: this still won't work if there's a middle trak with no audio */
   if ((qts_samp_num==0) && (time == 0xffffffff)) qts_init_duration += duration;

   DEBUG_LEVEL2 fprintf(stderr,"    -) dur %lx tim %lx rate %lx\n",
		duration,time,rate);
 }
}

void QT_Read_MDHD(fin,qt_mdhdr)
FILE *fin;
QT_MDHDR *qt_mdhdr;
{
  qt_mdhdr->version =	UTIL_Get_MSB_Long(fin);
  qt_mdhdr->creation =	UTIL_Get_MSB_Long(fin);
  qt_mdhdr->modtime =	UTIL_Get_MSB_Long(fin);
  qt_mdhdr->timescale =	UTIL_Get_MSB_Long(fin);
  qt_mdhdr->duration =	UTIL_Get_MSB_Long(fin);
  qt_mdhdr->language =	UTIL_Get_MSB_UShort(fin);
  qt_mdhdr->quality =	UTIL_Get_MSB_UShort(fin);

  if (qt_mdhdr->timescale) qt_md_timescale = qt_mdhdr->timescale;
  else qt_md_timescale = qt_tk_timescale;

  DEBUG_LEVEL2
  {
    fprintf(stderr,"md   ver = %lx  timescale = %lx  duration = %lx\n",
	qt_mdhdr->version,qt_mdhdr->timescale,qt_mdhdr->duration);
    fprintf(stderr,"     lang= %lx quality= %lx\n", 
	qt_mdhdr->language,qt_mdhdr->quality);
  }
}


void QT_Read_HDLR(fin,len,qt_hdlr_hdr)
FILE *fin;
xaLONG len;
QT_HDLR_HDR *qt_hdlr_hdr;
{
  qt_hdlr_hdr->version =	UTIL_Get_MSB_Long(fin);
  qt_hdlr_hdr->type =	UTIL_Get_MSB_Long(fin);
  qt_hdlr_hdr->subtype =	UTIL_Get_MSB_Long(fin);
  qt_hdlr_hdr->vendor =	UTIL_Get_MSB_Long(fin);
  qt_hdlr_hdr->flags =	UTIL_Get_MSB_Long(fin);
  qt_hdlr_hdr->mask =	UTIL_Get_MSB_Long(fin);

  DEBUG_LEVEL2
  {
    fprintf(stderr,"     ver = %lx  ",qt_hdlr_hdr->version);
    QT_Print_ID(stderr,qt_hdlr_hdr->type,1);
    QT_Print_ID(stderr,qt_hdlr_hdr->subtype,1);
    QT_Print_ID(stderr,qt_hdlr_hdr->vendor,0);
    fprintf(stderr,"\n     flags= %lx mask= %lx\n",
	qt_hdlr_hdr->flags,qt_hdlr_hdr->mask);
  }
  /* Read Handler Name if Present */
  if (len > 32)
  {
    len -= 32;
    QT_Read_Name(fin,len);
  }
}


/*********************************************
 * Read and Parse Video Codecs
 *
 **********/
xaULONG QT_Read_Video_STSD(anim_hdr,qt,fin)
XA_ANIM_HDR *anim_hdr;
XA_ANIM_SETUP *qt;
FILE *fin;
{ xaULONG i,version,num,cur,sup;
  version = UTIL_Get_MSB_Long(fin);
  num = UTIL_Get_MSB_Long(fin);
  DEBUG_LEVEL2 fprintf(stderr,"     ver = %lx  num = %lx\n", version,num);
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
    sup |= QT_Read_Video_Codec_HDR(anim_hdr,qt, &qtv_codecs[cur], fin ); 
DEBUG_LEVEL1 fprintf(stderr,"CODEC %ld) sup=%ld\n",i,sup);
    cur++;
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


xaULONG QT_Read_Video_Codec_HDR(anim_hdr,qt,c_hdr,fin)
XA_ANIM_HDR *anim_hdr;
XA_ANIM_SETUP *qt;
QTV_CODEC_HDR *c_hdr;
FILE *fin;
{
  xaULONG id;
  xaLONG len,i,codec_ret;
  xaULONG unk_0,unk_1,unk_2,unk_3,unk_4,unk_5,unk_6,unk_7,flag;
  xaULONG vendor,temp_qual,spat_qual,h_res,v_res;
  
  len		= UTIL_Get_MSB_Long(fin);
  id 		= UTIL_Get_MSB_Long(fin);
  unk_0		= UTIL_Get_MSB_Long(fin);
  unk_1		= UTIL_Get_MSB_Long(fin);
  unk_2		= UTIL_Get_MSB_UShort(fin);
  unk_3		= UTIL_Get_MSB_UShort(fin);
  vendor	= UTIL_Get_MSB_Long(fin);
  temp_qual	= UTIL_Get_MSB_Long(fin);
  spat_qual	= UTIL_Get_MSB_Long(fin);
  qt->imagex	= UTIL_Get_MSB_UShort(fin);
  qt->imagey	= UTIL_Get_MSB_UShort(fin);
  h_res		= UTIL_Get_MSB_UShort(fin);
  unk_4		= UTIL_Get_MSB_UShort(fin);
  v_res		= UTIL_Get_MSB_UShort(fin);
  unk_5		= UTIL_Get_MSB_UShort(fin);
  unk_6		= UTIL_Get_MSB_Long(fin);
  unk_7		= UTIL_Get_MSB_UShort(fin);
  QT_Read_Name(fin,32);
  qt->depth	= UTIL_Get_MSB_UShort(fin);
  flag		= UTIL_Get_MSB_UShort(fin);
  len -= 0x56;

  /* init now in case of early out */
  c_hdr->width	= qt->imagex;
  c_hdr->height	= qt->imagey;
  c_hdr->depth	= qt->depth;
  c_hdr->compression = id;
  c_hdr->chdr	= 0;

  if (   (qt->depth == 8) || (qt->depth == 40)
      || (qt->depth == 4) || (qt->depth == 36) ) /* generate colormap */
  {
    if (qt->depth & 0x04) qt->imagec = 16;
    else qt->imagec = 256;

    if (qt->depth < 32) QT_Create_Default_Cmap(qt->cmap,qt->imagec);
    else /* grayscale */
    {
      if ((id == QT_jpeg) || (id == QT_cvid)) 
		QT_Create_Gray_Cmap(qt->cmap,1,qt->imagec);
      else	QT_Create_Gray_Cmap(qt->cmap,0,qt->imagec);
    }

    if ( !(flag & 0x08) && (len) ) /* colormap isn't default */
    {
      xaULONG start,end,p,r,g,b,cflag;
DEBUG_LEVEL1 fprintf(stderr,"reading colormap. flag %lx\n",flag);
      start = UTIL_Get_MSB_Long(fin); /* is this start or something else? */
      cflag = UTIL_Get_MSB_UShort(fin); /* is this end or total number? */
      end   = UTIL_Get_MSB_UShort(fin); /* is this end or total number? */
      len -= 8;
DEBUG_LEVEL1 fprintf(stderr," start %lx end %lx cflag %lx\n",start,end,cflag);
      for(i = start; i <= end; i++)
      {
        p = UTIL_Get_MSB_UShort(fin); 
        r = UTIL_Get_MSB_UShort(fin); 
        g = UTIL_Get_MSB_UShort(fin); 
        b = UTIL_Get_MSB_UShort(fin);  len -= 8;
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

  while(len > 0) {fgetc(fin); len--; }

  qt_codec_hdr.compression = id;
  qt_codec_hdr.x = qt->imagex;
  qt_codec_hdr.y = qt->imagey;
  qt_codec_hdr.depth = qt->depth;
  qt_codec_hdr.anim_hdr = (void *)anim_hdr;
  qt_codec_hdr.avi_ctab_flag = xaFALSE;

  /* Query to see if Video Compression is supported or not */
  { xaULONG q = 0;
    while(q < (QT_QUERY_CNT) )
    {
      codec_ret = qt_query_func[q](&qt_codec_hdr);
      if (codec_ret == CODEC_SUPPORTED)
      {
	qt->imagex = qt_codec_hdr.x;
	qt->imagey = qt_codec_hdr.y;
	qt->depth  = qt_codec_hdr.depth;
	qt->compression = qt_codec_hdr.compression;
	c_hdr->xapi_rev	= qt_codec_hdr.xapi_rev;
	c_hdr->decoder	= qt_codec_hdr.decoder;
	c_hdr->dlta_extra	= qt_codec_hdr.extra;
	break;
      }
      else if (codec_ret == CODEC_UNSUPPORTED) break;
      q++;
    }
  }

  /*** Return False if Codec is Unknown or Not Supported */
  if (codec_ret != CODEC_SUPPORTED)
  { char tmpbuf[256];
    if (codec_ret == CODEC_UNKNOWN)
    { xaULONG ii,a[4],dd = qt->compression;
      for(ii=0; ii<4; ii++)
      { a[ii] = dd & 0xff;  dd >>= 8;
        if ((a[ii] < ' ') || (a[ii] > 'z')) a[ii] = '.';
      }
      sprintf(tmpbuf,"Unknown %c%c%c%c(%08lx)",
				a[3],a[2],a[1],a[0],qt->compression);
      qt_codec_hdr.description = tmpbuf;
    }
    if (xa_verbose) 
		fprintf(stderr,"  Video Codec: %s",qt_codec_hdr.description);
    else	fprintf(stderr,"QT Video Codec: %s",qt_codec_hdr.description);
    fprintf(stderr," depth=%ld is unsupported by this executable.\n",qt->depth);
    return(0);
  }

  /* Print out Video Codec info */
  if (xa_verbose) fprintf(stderr,"  Video Codec: %s depth=%ld\n",
                                qt_codec_hdr.description,qt->depth);

 
  if (qt->depth == 1)
  {
    qt->imagec = 2;
    qt->cmap[0].red = qt->cmap[0].green = qt->cmap[0].blue = 0; 
    qt->cmap[1].red = qt->cmap[1].green = qt->cmap[1].blue = 0xff; 
    qt->chdr = ACT_Get_CMAP(qt->cmap,qt->imagec,0,qt->imagec,0,8,8,8);
  }
  else if ( (qt->depth == 8) || (qt->depth == 4) || (qt->depth >= 36) )
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

void QT_Read_Name(fin,r_len)
FILE *fin;
xaLONG r_len;
{
  xaULONG len,d,i;

  len = fgetc(fin); r_len--;
  if (r_len == 0) r_len = len;
  if (len > r_len) fprintf(stderr,"QT_Name: len(%ld) > r_len(%ld)\n",len,r_len);
  DEBUG_LEVEL2 fprintf(stderr,"      (%ld/%ld) ",len,r_len);
  for(i=0;i<r_len;i++)
  {
    d = fgetc(fin) & 0x7f;
    if (i < len) DEBUG_LEVEL2 fputc(d,stderr);
  }
  DEBUG_LEVEL2 fputc('\n',stderr);
}

/* Time To Sample */
void QT_Read_STTS(fin,len,qt_t2samp_num,qt_t2samps)
FILE *fin;
xaLONG len;
xaULONG *qt_t2samp_num;
QT_T2SAMP_HDR **qt_t2samps;
{
  xaULONG version,num,i,samp_cnt,duration,cur; 
  xaULONG t2samp_num = *qt_t2samp_num;
  QT_T2SAMP_HDR *t2samps = *qt_t2samps;

  version	= UTIL_Get_MSB_Long(fin);
  num		= UTIL_Get_MSB_Long(fin);  len -= 8;
  DEBUG_LEVEL2 fprintf(stderr,"    ver=%lx num of entries = %lx\n",version,num);
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
    samp_cnt	= UTIL_Get_MSB_Long(fin);
    duration	= UTIL_Get_MSB_Long(fin);  len -= 8;
    if (duration == 0) duration = 1;
    /* NOTE: convert to 1000ms per second */
    t2samps[cur].cnt = samp_cnt;
    ftime = (1000.0 * (double)(duration)) / (double)(qt_vid_timescale);
    t2samps[cur].time = (xaULONG)(ftime);
    ftime -= (double)(t2samps[cur].time);
    t2samps[cur].timelo = (xaULONG)(ftime * (double)(1<<24));
    DEBUG_LEVEL2 fprintf(stderr,"      %ld) samp_cnt=%lx duration = %lx time %ld timelo %ld ftime %lf\n",
	i,samp_cnt,duration,t2samps[cur].time,t2samps[cur].timelo,ftime);
    cur++;
  }
  *qt_t2samp_num = t2samp_num;
  *qt_t2samps = t2samps;
  while(len > 0) {len--; getc(fin); }
}


/* Sync Sample */
void QT_Read_STSS(fin)
FILE *fin;
{
  xaULONG version,num,i,j;
  version	= UTIL_Get_MSB_Long(fin);
  num		= UTIL_Get_MSB_Long(fin);
  DEBUG_LEVEL2 
  {
    fprintf(stderr,"    ver=%lx num of entries = %lx\n",version,num);
    j = 0;
    fprintf(stderr,"      ");
  }
  for(i=0;i<num;i++)
  {
    xaULONG samp_num;
    samp_num	= UTIL_Get_MSB_Long(fin);
    DEBUG_LEVEL2
    {
      fprintf(stderr,"%lx ",samp_num); j++;
      if (j >= 8) {fprintf(stderr,"\n      "); j=0; }
    }
  }
  DEBUG_LEVEL2 fprintf(stderr,"\n");
}


/* Sample to Chunk */
void QT_Read_STSC(fin,len,qt_s2chunk_num,qt_s2chunks,
		chunkoff_lstnum,codec_num,codec_lstnum)
FILE *fin;
xaLONG len;
xaULONG *qt_s2chunk_num;
QT_S2CHUNK_HDR **qt_s2chunks;
xaULONG chunkoff_lstnum,codec_num,codec_lstnum;
{
  xaULONG version,num,i,cur,stsc_type,last;
  xaULONG s2chunk_num = *qt_s2chunk_num;
  QT_S2CHUNK_HDR *s2chunks = *qt_s2chunks;

  version	= UTIL_Get_MSB_Long(fin);
  num		= UTIL_Get_MSB_Long(fin);	len -= 16;
  i = (num)?(len/num):(0);
  if (i == 16) 
  { 
    DEBUG_LEVEL2 fprintf(stderr,"STSC: OLD STYLE\n");
    len -= num * 16; stsc_type = 0;
  }
  else 
  { 
    DEBUG_LEVEL2 fprintf(stderr,"STSC: NEW STYLE\n");
    len -= num * 12; stsc_type = 1;
  }

  DEBUG_LEVEL2 fprintf(stderr,"    ver=%lx num of entries = %lx\n",version,num);
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
      first_chk	= UTIL_Get_MSB_Long(fin);
      tmp	= UTIL_Get_MSB_Long(fin);
      samp_per	= UTIL_Get_MSB_Long(fin);
      chunk_tag	= UTIL_Get_MSB_Long(fin);
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
	  fprintf(stderr,"STSC: old style but not stgs chunk warning\n");
	  s2chunks[cur].num = 100000;
	}
      }
    }
    else		/* 3 entries */
    {
      first_chk	= UTIL_Get_MSB_Long(fin);
      samp_per	= UTIL_Get_MSB_Long(fin);
      chunk_tag	= UTIL_Get_MSB_Long(fin);
      s2chunks[cur].num   = samp_per;
    }
    DEBUG_LEVEL2 
     fprintf(stderr,"      %ld-%ld) first_chunk=%lx samp_per_chk=%lx chk_tag=%lx\n",
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
  DEBUG_LEVEL2 fprintf(stderr,"    STSC left over %ld\n",len);
  while (len > 0) { fgetc(fin); len--; }
  *qt_s2chunk_num = s2chunk_num;
  *qt_s2chunks = s2chunks;
}

/* Sample Size */
void QT_Read_STSZ(fin,len,qt_samp_num,qt_samp_sizes)
FILE *fin;
xaLONG len;
xaULONG *qt_samp_num,**qt_samp_sizes;
{
  xaULONG version,samp_size,num,i,cur;
  xaULONG samp_num   = *qt_samp_num;
  xaULONG *samp_sizes = *qt_samp_sizes;

  version	= UTIL_Get_MSB_Long(fin);
  samp_size	= UTIL_Get_MSB_Long(fin);
  num		= UTIL_Get_MSB_Long(fin);
  len = (len - 20) / 4;   /* number of stored samples */
  DEBUG_LEVEL2 fprintf(stderr,"    ver=%lx samp_size=%lx entries= %lx stored entries=%lx\n",version,samp_size,num,len);

  if (samp_size == 1) num = 1; /* for AUDIO PODNOTE: rethink this */
  if (samp_sizes == 0)
  {
    samp_num = num;
    samp_sizes = (xaULONG *)malloc(num * sizeof(xaULONG));
    if (samp_sizes == 0) {fprintf(stderr,"malloc err 0\n"); exit(0);}
    cur = 0;
  }
  else /*TRAK*/
  {
    xaULONG *tsamps;
    tsamps = (xaULONG *)malloc((samp_num + num) * sizeof(xaULONG));
    if (tsamps == 0) {fprintf(stderr,"malloc err 0\n"); exit(0);}
    for(i=0; i<samp_num; i++) tsamps[i] = samp_sizes[i];
    cur = samp_num;
    samp_num += num;
    FREE(samp_sizes,0x9010);
    samp_sizes = tsamps;
  }
  for(i=0;i<num;i++) 
  {
    if (i < len) samp_sizes[cur] = UTIL_Get_MSB_Long(fin);
    else if (i==0) samp_sizes[cur] = samp_size;
           else samp_sizes[cur] = samp_sizes[cur-1];
    cur++;
  }
  *qt_samp_num = samp_num;
  *qt_samp_sizes = samp_sizes;
}

/* Chunk Offset */
void QT_Read_STCO(fin,qt_chunkoff_num,qt_chunkoffs)
FILE *fin;
xaULONG *qt_chunkoff_num;
xaULONG **qt_chunkoffs;
{
  xaULONG version,num,i,cur;
  xaULONG chunkoff_num = *qt_chunkoff_num;
  xaULONG *chunkoffs = *qt_chunkoffs;

  version	= UTIL_Get_MSB_Long(fin);
  num		= UTIL_Get_MSB_Long(fin);
  DEBUG_LEVEL2 fprintf(stderr,"    ver=%lx entries= %lx\n",version,num);
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
    if (tchunks == 0) {fprintf(stderr,"malloc err 0\n"); exit(0);}
    for(i=0; i<chunkoff_num; i++) tchunks[i] = chunkoffs[i];
    cur = chunkoff_num;
    chunkoff_num += num;
    FREE(chunkoffs,0x9011);
    chunkoffs = tchunks;
  }
  for(i=0;i<num;i++) {chunkoffs[cur] = UTIL_Get_MSB_Long(fin); cur++; }
  *qt_chunkoff_num = chunkoff_num;
  *qt_chunkoffs = chunkoffs;
}



xaULONG QT_Read_Video_Data(qt,fin,anim_hdr)
XA_ANIM_SETUP *qt;
FILE *fin;
XA_ANIM_HDR *anim_hdr;
{
  xaULONG d,ret,i;
  xaULONG cur_samp,cur_s2chunk,nxt_s2chunk;
  xaULONG cur_t2samp,nxt_t2samp;
  xaULONG tag;
  xaULONG cur_off;
  xaULONG unsupported_flag = xaFALSE;
  XA_ACTION *act;

  if (qtv_samp_sizes == 0) 
	{fprintf(stderr,"QT: no video samples\n"); return(xaFALSE); } 

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

DEBUG_LEVEL2 fprintf(stderr,"T2S: cur-t2 %ld cur-smp %ld nxt-t2 %ld tot t2 %ld\n", cur_t2samp,cur_samp,nxt_t2samp,qtv_t2samp_num);
    if ( (cur_samp >= nxt_t2samp) && (cur_t2samp < qtv_t2samp_num) )
    {
      cur_t2samp++;
      if (xa_jiffy_flag) { qt->vid_time = xa_jiffy_flag; qt->vid_timelo = 0; }
      else
      { 
	qt->vid_time   = qtv_t2samps[cur_t2samp].time;
	qt->vid_timelo = qtv_t2samps[cur_t2samp].timelo;
      }
      nxt_t2samp += qtv_t2samps[cur_t2samp].cnt;
    }

    if ( (i == nxt_s2chunk) && ((cur_s2chunk+1) < qtv_s2chunk_num) )
    {
      cur_s2chunk++;
      nxt_s2chunk = qtv_s2chunks[cur_s2chunk + 1].first;
    }
    num_samps = qtv_s2chunks[cur_s2chunk].num;

    /* Check tags and possibly move to new codec */
    if (qtv_s2chunks[cur_s2chunk].tag >= qtv_codec_num) 
    {
      fprintf(stderr,"QT Data: Warning stsc chunk invalid %ld tag %ld\n",
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
	fseek(fin,cur_off,0);
	ret = fread( dlta_hdr->data, size, 1, fin);
	if (ret != 1) 
		{ fprintf(stderr,"QT codec: read failed\n"); return(xaFALSE); }
      }
      cur_off += size;

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
	  fprintf(stderr,"QT: Sections of this movie use an unsupported Video Codec\n");
	  fprintf(stderr," and are therefore NOT viewable.\n");
	  unsupported_flag = xaTRUE;
        }
        act->type = ACT_NOP;
      }
      ACT_Setup_Delta(qt,act,dlta_hdr,fin);
      cur_samp++;
      if (cur_samp >= qtv_samp_num) break;
    } /* end of sample number */
    if (cur_samp >= qtv_samp_num) break;
  } /* end of chunk_offset loop */
  return(xaTRUE);
}


xaULONG
QT_Decode_RLE(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG special = dec_info->special;          void *extra = dec_info->extra;
  XA_CHDR *chdr = dec_info->chdr;
  xaLONG y,lines,d; /* xaLONG min_x,max_x,min_y,max_y;  */
  xaUBYTE *dptr;

  dptr = delta;
  dptr += 4;    /* skip codec size */
  d = (*dptr++) << 8;  d |= *dptr++;   /* read code either 0008 or 0000 */
  if (dsize < 8) /* NOP */
  { dec_info->xs = dec_info->ys = dec_info->xe = dec_info->ye = 0;
    return(ACT_DLTA_NOP);
  }
  if (d & 0x0008) /* Header present */
  {
    y = (*dptr++) << 8; y |= *dptr++;           /* start line */
    dptr += 2;                                  /* unknown */
    lines = (*dptr++) << 8; lines |= *dptr++;   /* number of lines */
    dptr += 2;                                  /* unknown */
  }
  else { y = 0; lines = imagey; }
  while(lines--)
  {
    xaULONG xskip,cnt;
    xskip = *dptr++;				/* skip x pixels */
    if (xskip==0) break;			/* exit */
    cnt = *dptr++;				/* RLE code */
    if ( (x11_bytes_pixel==1) || (map_flag==xaFALSE) )
    { xaUBYTE *iptr = (xaUBYTE *)(image + (y * imagex) + (4 * (xskip-1)) );
      while(cnt != 0xff)
      {
        if (cnt == 0x00) { xskip = *dptr++; iptr += 4 * (xskip-1); }
        else if (cnt < 0x80)			/* run of data */
        {
          cnt *= 4; if (map_flag==xaFALSE) while(cnt--) *iptr++ = (xaUBYTE)*dptr++;
          else while(cnt--) *iptr++ = (xaUBYTE)map[*dptr++];
        } else					/* repeat data */
        { xaUBYTE d1,d2,d3,d4;	cnt = 0x100 - cnt;
          if (map_flag==xaTRUE) { d1=(xaUBYTE)map[*dptr++]; d2=(xaUBYTE)map[*dptr++];
			      d3=(xaUBYTE)map[*dptr++]; d4=(xaUBYTE)map[*dptr++]; }
	  else	{ d1 = (xaUBYTE)*dptr++; d2 = (xaUBYTE)*dptr++;
		  d3 = (xaUBYTE)*dptr++; d4 = (xaUBYTE)*dptr++; }
          while(cnt--) { *iptr++ =d1; *iptr++ =d2; *iptr++ =d3; *iptr++ =d4; }
        } /* end of  >= 0x80 */
        cnt = *dptr++;
      } /* end while cnt */
    } else if (x11_bytes_pixel==2)
    { xaUSHORT *iptr = (xaUSHORT *)(image + 2 *((y * imagex) + (4 * (xskip-1))) );
      while(cnt != 0xff)
      {
        if (cnt == 0x00) { xskip = *dptr++; iptr += 4 * (xskip-1); }
        else if (cnt < 0x80)			/* run of data */
        {
          cnt *= 4; while(cnt--) *iptr++ = (xaUSHORT)map[*dptr++];
        } else					/* repeat data */
        { xaUSHORT d1,d2,d3,d4;	cnt = 0x100 - cnt;
	  { d1 = (xaUSHORT)map[*dptr++]; d2 = (xaUSHORT)map[*dptr++];
	    d3 = (xaUSHORT)map[*dptr++]; d4 = (xaUSHORT)map[*dptr++]; }
          while(cnt--) { *iptr++ =d1; *iptr++ =d2; *iptr++ =d3; *iptr++ =d4; }
        } /* end of  >= 0x80 */
        cnt = *dptr++;
      } /* end while cnt */
    } else /* bytes == 4 */
    { xaULONG *iptr = (xaULONG *)(image + 4 * ((y * imagex) + (4 * (xskip-1))) );
      while(cnt != 0xff)
      {
        if (cnt == 0x00) { xskip = *dptr++; iptr += 4 * (xskip-1); }
        else if (cnt < 0x80)			/* run of data */
        {
          cnt *= 4; while(cnt--) *iptr++ = (xaULONG)map[*dptr++];
        } else					/* repeat data */
        { xaULONG d1,d2,d3,d4; cnt = 0x100 - cnt;
	  { d1 = (xaULONG)map[*dptr++]; d2 = (xaULONG)map[*dptr++]; 
	    d3 = (xaULONG)map[*dptr++]; d4 = (xaULONG)map[*dptr++]; }
          while(cnt--) { *iptr++ =d1; *iptr++ =d2; *iptr++ =d3; *iptr++ =d4; }
        } /* end of  >= 0x80 */
        cnt = *dptr++;
      } /* end while cnt */
    }
    y++;
  } /* end of lines */
 /* one more zero byte */
 dec_info->xs = dec_info->ys = 0; dec_info->xe = imagex; dec_info->ye = imagey;
 if (map_flag==xaTRUE) return(ACT_DLTA_MAPD);
 else return(ACT_DLTA_NORM);
}

xaULONG QT_Parse_Bin(fin)
FILE *fin;
{
  xaULONG pos,len,csize,cid,total;
 
  fseek(fin,0,2);
  total = ftell(fin);
 
/* Read over Header */
  fseek(fin,0,0);
  pos = len = UTIL_Get_MSB_Long(fin);
  cid = UTIL_Get_MSB_Long(fin);
  if (cid == QT_mdat)
  {
    fseek(fin,0,0);
    if (len == 0)
    {
      fprintf(stderr,"QT: This is only .data fork. Need .rsrc fork\n");
      return(0);
    }
    else return(1);
  }
  else if (cid == QT_moov) return(1);
 
DEBUG_LEVEL1 fprintf(stderr,"QT_Parse_Bin: %lx\n",pos);
 
  while( pos < total )
  {
    fseek(fin,pos,0);
 
    len = UTIL_Get_MSB_Long(fin);
    pos += 4;        /* binary size is 4 bytes */
    csize = UTIL_Get_MSB_Long(fin);
    cid   = UTIL_Get_MSB_Long(fin);
    if (cid == QT_moov)
    {
      fseek(fin,pos,0);
      return(1);
    }
    if (len == 0) return(0);
    pos += len;
  }
  return(0);
}

#define QT_BLOCK_INC(x,y,imagex) { x += 4; if (x>=imagex) { x=0; y += 4; }}

#define QT_RPZA_BLOCK_INC(x,y,imagex,im0,im1,im2,im3,binc,rinc) \
{ x += 4; im0 += binc; im1 += binc; im2 += binc; im3 += binc;	\
  if (x>=imagex)					\
  { x=0; y += 4; im0 += rinc; im1 += rinc; im2 += rinc; im3 += rinc; } }

#define QT_MIN_MAX_CHECK(x,y,min_x,min_y,max_x,max_y) {	\
  if (x > max_x) max_x=x; if (y > max_y) max_y=y;	\
  if (x < min_x) min_x=x; if (y < min_y) min_y=y;  }

#define QT_RPZA_C1(ip0,ip1,ip2,ip3,c,CAST) { \
 ip0[0] = ip0[1] = ip0[2] = ip0[3] = (CAST)c; \
 ip1[0] = ip1[1] = ip1[2] = ip1[3] = (CAST)c; \
 ip2[0] = ip2[1] = ip2[2] = ip2[3] = (CAST)c; \
 ip3[0] = ip3[1] = ip3[2] = ip3[3] = (CAST)c; }

#define QT_RPZA_C4(ip,c,mask,CAST); { \
 ip[0] =(CAST)(c[((mask>>6)&0x03)]); ip[1] =(CAST)(c[((mask>>4)&0x03)]); \
 ip[2] =(CAST)(c[((mask>>2)&0x03)]); ip[3] =(CAST)(c[ (mask & 0x03)]); }

#define QT_RPZA_C16(ip0,ip1,ip2,ip3,c,CAST) { \
 ip0[0] = (CAST)(*c++); ip0[1] = (CAST)(*c++); \
 ip0[2] = (CAST)(*c++); ip0[3] = (CAST)(*c++); \
 ip1[0] = (CAST)(*c++); ip1[1] = (CAST)(*c++); \
 ip1[2] = (CAST)(*c++); ip1[3] = (CAST)(*c++); \
 ip2[0] = (CAST)(*c++); ip2[1] = (CAST)(*c++); \
 ip2[2] = (CAST)(*c++); ip2[3] = (CAST)(*c++); \
 ip3[0] = (CAST)(*c++); ip3[1] = (CAST)(*c++); \
 ip3[2] = (CAST)(*c++); ip3[3] = (CAST)(*c  ); }

#define QT_RPZA_rgbC1(ip,r,g,b) { \
 ip[0] = ip[3] = ip[6] = ip[9]  = r; \
 ip[1] = ip[4] = ip[7] = ip[10] = g; \
 ip[2] = ip[5] = ip[8] = ip[11] = b; }

#define QT_RPZA_rgbC4(ip,r,g,b,mask); { xaULONG _idx; \
 _idx = (mask>>6)&0x03; ip[0] = r[_idx]; ip[1] = g[_idx]; ip[2] = b[_idx]; \
 _idx = (mask>>4)&0x03; ip[3] = r[_idx]; ip[4] = g[_idx]; ip[5] = b[_idx]; \
 _idx = (mask>>2)&0x03; ip[6] = r[_idx]; ip[7] = g[_idx]; ip[8] = b[_idx]; \
 _idx =  mask    &0x03; ip[9] = r[_idx]; ip[10] = g[_idx]; ip[11] = b[_idx]; }

#define QT_RPZA_rgbC16(ip,r,g,b) { \
 ip[0]= *r++; ip[1]= *g++; ip[2]= *b++; \
 ip[3]= *r++; ip[4]= *g++; ip[5]= *b++; \
 ip[6]= *r++; ip[7]= *g++; ip[8]= *b++; \
 ip[9]= *r++; ip[10]= *g++; ip[11]= *b++; }

xaULONG
QT_Decode_RPZA(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG special = dec_info->special;          void *extra = dec_info->extra;
  XA_CHDR *chdr = dec_info->chdr;
  xaLONG x,y,len,row_inc,blk_inc; 
  xaLONG min_x,max_x,min_y,max_y;
  xaUBYTE *dptr;
  xaULONG code,changed;
  xaUBYTE *im0,*im1,*im2,*im3;

  if (chdr) { if (chdr->new_chdr) chdr = chdr->new_chdr; }
  dec_info->xs = dec_info->ys = 0; dec_info->xe = imagex; dec_info->ye = imagey;
  max_x = max_y = 0; min_x = imagex; min_y = imagey; changed = 0;
  dptr = delta;
  x = y = 0;
  if (special) blk_inc = 3;
  else if ( (x11_bytes_pixel==1) || (map_flag == xaFALSE) ) blk_inc = 1;
  else if (x11_bytes_pixel==2) blk_inc = 2;
  else blk_inc = 4;
  row_inc = blk_inc * imagex;
  blk_inc *= 4;
  im1 = im0 = image;	im1 += row_inc; 
  im2 = im1;		im2 += row_inc;
  im3 = im2;		im3 += row_inc;
  row_inc *= 3; /* skip 3 rows at a time */
  
  dptr++;			/* for 0xe1 */
  len =(*dptr++)<<16; len |= (*dptr++)<< 8; len |= (*dptr++); /* Read Len */
  if (len != dsize) /* CHECK FOR CORRUPTION - FAIRLY COMMON */
  {
    if (xa_verbose==xaTRUE) 
	fprintf(stderr,"QT corruption-skipping this frame %lx %lx\n",dsize,len);
    return(ACT_DLTA_NOP);
  }
  len -= 4;				/* read 4 already */
  while(len > 0)
  {
    code = *dptr++; len--;

    if ( (code >= 0xa0) && (code <= 0xbf) )			/* SINGLE */
    {
      xaULONG color,skip;
      changed = 1;
      color = (*dptr++) << 8; color |= *dptr++; len -= 2;
      skip = (code-0x9f);
      if (special)
      { xaUBYTE r,g,b;
        QT_Get_RGBColor(&r,&g,&b,color); 
        while(skip--)
        { xaUBYTE *ip0=im0; xaUBYTE *ip1=im1; xaUBYTE *ip2=im2; xaUBYTE *ip3=im3;
	  QT_RPZA_rgbC1(ip0,r,g,b);
	  QT_RPZA_rgbC1(ip1,r,g,b);
	  QT_RPZA_rgbC1(ip2,r,g,b);
	  QT_RPZA_rgbC1(ip3,r,g,b);
	  QT_MIN_MAX_CHECK(x,y,min_x,min_y,max_x,max_y);
	  QT_RPZA_BLOCK_INC(x,y,imagex,im0,im1,im2,im3,blk_inc,row_inc);
	}
      }
      else /* not special */
      {
        color = QT_Get_Color(color,map_flag,map,chdr); 
        while(skip--)
        {
          if ( (x11_bytes_pixel==1) || (map_flag == xaFALSE) )
          { xaUBYTE *ip0=im0; xaUBYTE *ip1=im1; xaUBYTE *ip2=im2; xaUBYTE *ip3=im3;
	    QT_RPZA_C1(ip0,ip1,ip2,ip3,color,xaUBYTE);
	  }
          else if (x11_bytes_pixel==4)
          { xaULONG *ip0= (xaULONG *)im0; xaULONG *ip1= (xaULONG *)im1; 
	    xaULONG *ip2= (xaULONG *)im2; xaULONG *ip3= (xaULONG *)im3;
	    QT_RPZA_C1(ip0,ip1,ip2,ip3,color,xaULONG);
	  }
          else /* if (x11_bytes_pixel==2) */
          { xaUSHORT *ip0= (xaUSHORT *)im0; xaUSHORT *ip1= (xaUSHORT *)im1; 
	    xaUSHORT *ip2= (xaUSHORT *)im2; xaUSHORT *ip3= (xaUSHORT *)im3;
	    QT_RPZA_C1(ip0,ip1,ip2,ip3,color,xaUSHORT);
	  }
	  QT_MIN_MAX_CHECK(x,y,min_x,min_y,max_x,max_y);
	  QT_RPZA_BLOCK_INC(x,y,imagex,im0,im1,im2,im3,blk_inc,row_inc);
        } /* end of skip-- */
      } /* end not special */
    }
    else if ( (code >= 0x80) && (code <= 0x9f) )		/* SKIP */
    { xaULONG skip = (code-0x7f);
      while(skip--) 
		QT_RPZA_BLOCK_INC(x,y,imagex,im0,im1,im2,im3,blk_inc,row_inc);
    }
    else if ( (code < 0x80) 				/* FOUR/SIXTEEN */ 
	     || ((code >= 0xc0) && (code <= 0xdf)) )
    { xaULONG cA,cB;
      changed = 1;
      /* Get 1st two colors */
      if (code >= 0xc0) { cA = (*dptr++) << 8; cA |= *dptr++; len -= 2; }
      else {cA = (code << 8) | *dptr++; len -= 1;}
      cB = (*dptr++) << 8; cB |= *dptr++; len -= 2;

      /****** SIXTEEN COLOR *******/
      if ( (code < 0x80) && ((cB & 0x8000)==0) ) /* 16 color */
      {
        xaULONG i,d,*clr,c[16];
        xaUBYTE r[16],g[16],b[16];
	if (special)
	{
          QT_Get_RGBColor(&r[0],&g[0],&b[0],cA);
          QT_Get_RGBColor(&r[1],&g[1],&b[1],cB);
          for(i=2; i<16; i++)
          {
            d = (*dptr++) << 8; d |= *dptr++; len -= 2;
            QT_Get_RGBColor(&r[i],&g[i],&b[i],d);
          }
	}
	else
	{
	  clr = c;
          *clr++ = QT_Get_Color(cA,map_flag,map,chdr);
          *clr++ = QT_Get_Color(cB,map_flag,map,chdr);
          for(i=2; i<16; i++)
          {
            d = (*dptr++) << 8; d |= *dptr++; len -= 2;
            *clr++ = QT_Get_Color(d,map_flag,map,chdr);
          }
	}
	clr = c;
	if (special)
        { xaUBYTE *ip0=im0; xaUBYTE *ip1=im1; xaUBYTE *ip2=im2; xaUBYTE *ip3=im3;
	  xaUBYTE *tr,*tg,*tb; tr=r; tg=g; tb=b;
	  QT_RPZA_rgbC16(ip0,tr,tg,tb);
	  QT_RPZA_rgbC16(ip1,tr,tg,tb);
	  QT_RPZA_rgbC16(ip2,tr,tg,tb);
	  QT_RPZA_rgbC16(ip3,tr,tg,tb);
	}
	else if ( (x11_bytes_pixel==1) || (map_flag==xaFALSE) )
        { xaUBYTE *ip0=im0; xaUBYTE *ip1=im1; xaUBYTE *ip2=im2; xaUBYTE *ip3=im3;
	  QT_RPZA_C16(ip0,ip1,ip2,ip3,clr,xaUBYTE);
	}
	else if (x11_bytes_pixel==4)
        { xaULONG *ip0= (xaULONG *)im0; xaULONG *ip1= (xaULONG *)im1; 
	  xaULONG *ip2= (xaULONG *)im2; xaULONG *ip3= (xaULONG *)im3;
	  QT_RPZA_C16(ip0,ip1,ip2,ip3,clr,xaULONG);
	}
	else /* if (x11_bytes_pixel==2) */
        { xaUSHORT *ip0= (xaUSHORT *)im0; xaUSHORT *ip1= (xaUSHORT *)im1; 
	  xaUSHORT *ip2= (xaUSHORT *)im2; xaUSHORT *ip3= (xaUSHORT *)im3;
	  QT_RPZA_C16(ip0,ip1,ip2,ip3,clr,xaUSHORT);
	}
	QT_MIN_MAX_CHECK(x,y,min_x,min_y,max_x,max_y);
	QT_RPZA_BLOCK_INC(x,y,imagex,im0,im1,im2,im3,blk_inc,row_inc);
      } /*** END of SIXTEEN COLOR */
      else					/****** FOUR COLOR *******/
      { xaULONG m_cnt,msk0,msk1,msk2,msk3,c[4];
	xaUBYTE r[4],g[4],b[4];

        if (code < 0x80) m_cnt = 1; 
	else m_cnt = code - 0xbf; 

	if (special) QT_Get_AV_RGBColors(c,r,g,b,cA,cB);
	else QT_Get_AV_Colors(c,cA,cB,map_flag,map,chdr);

        while(m_cnt--)
        { msk0 = *dptr++; msk1 = *dptr++;
	  msk2 = *dptr++; msk3 = *dptr++; len -= 4;
	  if (special)
          { xaUBYTE *ip0=im0; xaUBYTE *ip1=im1; xaUBYTE *ip2=im2; xaUBYTE *ip3=im3;
	    QT_RPZA_rgbC4(ip0,r,g,b,msk0);
	    QT_RPZA_rgbC4(ip1,r,g,b,msk1);
	    QT_RPZA_rgbC4(ip2,r,g,b,msk2);
	    QT_RPZA_rgbC4(ip3,r,g,b,msk3);
	  }
	  else if ( (x11_bytes_pixel==1) || (map_flag==xaFALSE) )
          { xaUBYTE *ip0=im0; xaUBYTE *ip1=im1; xaUBYTE *ip2=im2; xaUBYTE *ip3=im3;
	    QT_RPZA_C4(ip0,c,msk0,xaUBYTE);
	    QT_RPZA_C4(ip1,c,msk1,xaUBYTE);
	    QT_RPZA_C4(ip2,c,msk2,xaUBYTE);
	    QT_RPZA_C4(ip3,c,msk3,xaUBYTE);
	  }
	  else if (x11_bytes_pixel==4)
          { xaULONG *ip0= (xaULONG *)im0; xaULONG *ip1= (xaULONG *)im1; 
	    xaULONG *ip2= (xaULONG *)im2; xaULONG *ip3= (xaULONG *)im3;
	    QT_RPZA_C4(ip0,c,msk0,xaULONG);
	    QT_RPZA_C4(ip1,c,msk1,xaULONG);
	    QT_RPZA_C4(ip2,c,msk2,xaULONG);
	    QT_RPZA_C4(ip3,c,msk3,xaULONG);
	  }
	  else /* if (x11_bytes_pixel==2) */
          { xaUSHORT *ip0= (xaUSHORT *)im0; xaUSHORT *ip1= (xaUSHORT *)im1; 
	    xaUSHORT *ip2= (xaUSHORT *)im2; xaUSHORT *ip3= (xaUSHORT *)im3;
	    QT_RPZA_C4(ip0,c,msk0,xaUSHORT);
	    QT_RPZA_C4(ip1,c,msk1,xaUSHORT);
	    QT_RPZA_C4(ip2,c,msk2,xaUSHORT);
	    QT_RPZA_C4(ip3,c,msk3,xaUSHORT);
	  }
	  QT_MIN_MAX_CHECK(x,y,min_x,min_y,max_x,max_y);
	  QT_RPZA_BLOCK_INC(x,y,imagex,im0,im1,im2,im3,blk_inc,row_inc);
        }  
      } /*** END of FOUR COLOR *******/
    } /*** END of 4/16 COLOR BLOCKS ****/
    else /* UNKNOWN */
    {
      fprintf(stderr,"QT RPZA: Unknown %lx\n",code);
      return(ACT_DLTA_NOP);
    }
  }
  if (xa_optimize_flag == xaTRUE)
  {
    if (changed) { dec_info->xs=min_x; dec_info->ys=min_y;
		   dec_info->xe=max_x + 4; dec_info->ye=max_y + 4; }
    else  { dec_info->xs = dec_info->ys = dec_info->xe = dec_info->ye = 0;
	    return(ACT_DLTA_NOP); }
  }
  else { dec_info->xs = dec_info->ys = 0; 
	 dec_info->xe = imagex; dec_info->ye = imagey; }
  if (map_flag) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}

void QT_Get_RGBColor(r,g,b,color)
xaUBYTE *r,*g,*b;
xaULONG color;
{ xaULONG ra,ga,ba;
  ra = (color >> 10) & 0x1f;	ra = (ra << 3) | (ra >> 2);
  ga = (color >>  5) & 0x1f;	ga = (ga << 3) | (ga >> 2);
  ba =  color & 0x1f;		ba = (ba << 3) | (ba >> 2);
  *r = ra; *g = ga; *b = ba;
}

xaULONG QT_Get_Color(color,map_flag,map,chdr)
xaULONG color,map_flag,*map;
XA_CHDR *chdr;
{
  register xaULONG clr,ra,ga,ba,ra5,ga5,ba5;

  ra5 = (color >> 10) & 0x1f;
  ga5 = (color >>  5) & 0x1f;
  ba5 =  color & 0x1f;
  ra = qt_gamma_adj[ra5]; ga = qt_gamma_adj[ga5]; ba = qt_gamma_adj[ba5];

  if (x11_display_type & XA_X11_TRUE) clr = X11_Get_True_Color(ra,ga,ba,16);
  else 
  { 
    if ((cmap_color_func == 4) && (chdr))
    { register xaULONG cache_i = color & 0x7fff;
      if (cmap_cache == 0) CMAP_Cache_Init(0);
      if (chdr != cmap_cache_chdr)
      {
        CMAP_Cache_Clear();
        cmap_cache_chdr = chdr;
      }
      if (cmap_cache[cache_i] == 0xffff)
      {
        clr = chdr->coff +
           CMAP_Find_Closest(chdr->cmap,chdr->csize,ra,ga,ba,16,16,16,xaTRUE);
        cmap_cache[cache_i] = (xaUSHORT)clr;
      }
      else clr = (xaULONG)cmap_cache[cache_i];
    }
    else
    {
      if (cmap_true_to_332 == xaTRUE) clr = CMAP_GET_332(ra5,ga5,ba5,CMAP_SCALE5);
      else			  clr = CMAP_GET_GRAY(ra5,ga5,ba5,CMAP_SCALE10);
      if (map_flag) clr = map[clr];
    }
  }
  return(clr);
}


void QT_Get_AV_RGBColors(c,r,g,b,cA,cB)
xaULONG *c;
xaUBYTE *r,*g,*b;
xaULONG cA,cB;
{ xaULONG rA,gA,bA,rB,gB,bB,ra,ga,ba;
/**color 3 ***/
  rA = (cA >> 10) & 0x1f;	r[3] = (rA << 3) | (rA >> 2);
  gA = (cA >>  5) & 0x1f;	g[3] = (gA << 3) | (gA >> 2);
  bA =  cA & 0x1f;		b[3] = (bA << 3) | (bA >> 2);
/**color 0 ***/
  rB = (cB >> 10) & 0x1f;	r[0] = (rB << 3) | (rB >> 2);
  gB = (cB >>  5) & 0x1f;	g[0] = (gB << 3) | (gB >> 2);
  bB =  cB & 0x1f;		b[0] = (bB << 3) | (bB >> 2);
/**color 2 ***/
  ra = (21*rA + 11*rB) >> 5;	r[2] = (ra << 3) | (ra >> 2);
  ga = (21*gA + 11*gB) >> 5;	g[2] = (ga << 3) | (ga >> 2);
  ba = (21*bA + 11*bB) >> 5;	b[2] = (ba << 3) | (ba >> 2);
/**color 1 ***/
  ra = (11*rA + 21*rB) >> 5;	r[1] = (ra << 3) | (ra >> 2);
  ga = (11*gA + 21*gB) >> 5;	g[1] = (ga << 3) | (ga >> 2);
  ba = (11*bA + 21*bB) >> 5;	b[1] = (ba << 3) | (ba >> 2);
}

void QT_Get_AV_Colors(c,cA,cB,map_flag,map,chdr)
xaULONG *c;
xaULONG cA,cB,map_flag,*map;
XA_CHDR *chdr;
{
  xaULONG clr,rA,gA,bA,rB,gB,bB,r0,g0,b0,r1,g1,b1;
  xaULONG rA5,gA5,bA5,rB5,gB5,bB5;
  xaULONG r05,g05,b05,r15,g15,b15;

/*color 3*/
  rA5 = (cA >> 10) & 0x1f;
  gA5 = (cA >>  5) & 0x1f;
  bA5 =  cA & 0x1f;
/*color 0*/
  rB5 = (cB >> 10) & 0x1f;
  gB5 = (cB >>  5) & 0x1f;
  bB5 =  cB & 0x1f;
/*color 2*/
  r05 = (21*rA5 + 11*rB5) >> 5;
  g05 = (21*gA5 + 11*gB5) >> 5;
  b05 = (21*bA5 + 11*bB5) >> 5;
/*color 1*/
  r15 = (11*rA5 + 21*rB5) >> 5;
  g15 = (11*gA5 + 21*gB5) >> 5;
  b15 = (11*bA5 + 21*bB5) >> 5;
/*adj and scale to 16 bits */
  rA=qt_gamma_adj[rA5]; gA=qt_gamma_adj[gA5]; bA=qt_gamma_adj[bA5];
  rB=qt_gamma_adj[rB5]; gB=qt_gamma_adj[gB5]; bB=qt_gamma_adj[bB5];
  r0=qt_gamma_adj[r05]; g0=qt_gamma_adj[g05]; b0=qt_gamma_adj[b05];
  r1=qt_gamma_adj[r15]; g1=qt_gamma_adj[g15]; b1=qt_gamma_adj[b15];

  /*** 1st Color **/
  if (x11_display_type & XA_X11_TRUE) clr = X11_Get_True_Color(rA,gA,bA,16);
  else 
  { 
    if ((cmap_color_func == 4) && (chdr))
    { register xaULONG cache_i = cA & 0x7fff;
      if (cmap_cache == 0) CMAP_Cache_Init(0);
      if (chdr != cmap_cache_chdr)
      {
        CMAP_Cache_Clear();
        cmap_cache_chdr = chdr;
      }
      if (cmap_cache[cache_i] == 0xffff)
      {
        clr = chdr->coff +
           CMAP_Find_Closest(chdr->cmap,chdr->csize,rA,gA,bA,16,16,16,xaTRUE);
        cmap_cache[cache_i] = (xaUSHORT)clr;
      }
      else clr = (xaULONG)cmap_cache[cache_i];
    }
    else
    { if (cmap_true_to_332 == xaTRUE) clr = CMAP_GET_332(rA5,gA5,bA5,CMAP_SCALE5);
      else	clr = CMAP_GET_GRAY(rA5,gA5,bA5,CMAP_SCALE10);
      if (map_flag) clr = map[clr];
    }
  }
  c[3] = clr;

  /*** 2nd Color **/
  if (x11_display_type & XA_X11_TRUE) clr = X11_Get_True_Color(rB,gB,bB,16);
  else 
  { 
    if ((cmap_color_func == 4) && (chdr))
    { register xaULONG cache_i = cB & 0x7fff;
      if (cmap_cache[cache_i] == 0xffff)
      {
        clr = chdr->coff +
           CMAP_Find_Closest(chdr->cmap,chdr->csize,rB,gB,bB,16,16,16,xaTRUE);
        cmap_cache[cache_i] = (xaUSHORT)clr;
      }
      else clr = (xaULONG)cmap_cache[cache_i];
    }
    else
    { if (cmap_true_to_332 == xaTRUE) clr = CMAP_GET_332(rB5,gB5,bB5,CMAP_SCALE5);
      else	clr = CMAP_GET_GRAY(rB5,gB5,bB5,CMAP_SCALE10);
      if (map_flag) clr = map[clr];
    }
  }
  c[0] = clr;

  /*** 1st Av ****/
  if (x11_display_type & XA_X11_TRUE) clr = X11_Get_True_Color(r0,g0,b0,16);
  else 
  { 
    if ((cmap_color_func == 4) && (chdr))
    { register xaULONG cache_i;
      cache_i = (xaULONG)(r05 << 10) | (g05 << 5) | b05;
      if (cmap_cache[cache_i] == 0xffff)
      {
        clr = chdr->coff +
           CMAP_Find_Closest(chdr->cmap,chdr->csize,r0,g0,b0,16,16,16,xaTRUE);
        cmap_cache[cache_i] = (xaUSHORT)clr;
      }
      else clr = (xaULONG)cmap_cache[cache_i];
    }
    else
    { if (cmap_true_to_332 == xaTRUE) clr = CMAP_GET_332(r05,g05,b05,CMAP_SCALE5);
      else	clr = CMAP_GET_GRAY(r05,g05,b05,CMAP_SCALE10);
      if (map_flag) clr = map[clr];
    }
  }
  c[2] = clr;

  /*** 2nd Av ****/
  if (x11_display_type & XA_X11_TRUE) clr = X11_Get_True_Color(r1,g1,b1,16);
  else 
  { 
    if ((cmap_color_func == 4) && (chdr))
    { register xaULONG cache_i;
      cache_i = (xaULONG)(r15 << 10) | (g15 << 5) | b15;
      if (cmap_cache[cache_i] == 0xffff)
      {
        clr = chdr->coff +
           CMAP_Find_Closest(chdr->cmap,chdr->csize,r1,g1,b1,16,16,16,xaTRUE);
        cmap_cache[cache_i] = (xaUSHORT)clr;
      }
      else clr = (xaULONG)cmap_cache[cache_i];
    }
    else
    { if (cmap_true_to_332 == xaTRUE) clr = CMAP_GET_332(r15,g15,b15,CMAP_SCALE5);
      else	clr = CMAP_GET_GRAY(r15,g15,b15,CMAP_SCALE10);
      if (map_flag) clr = map[clr];
    }
  }
  c[1] = clr;
}


xaULONG
QT_Decode_RLE16(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG special = dec_info->special;          void *extra = dec_info->extra;
  XA_CHDR *chdr = dec_info->chdr;
  xaLONG y,d,lines; /* xaLONG min_x,max_x,min_y,max_y; */
  xaULONG special_flag;
  xaUBYTE r,g,b,*dptr;

  if (chdr) { if (chdr->new_chdr) chdr = chdr->new_chdr; }

  special_flag = special & 0x0001;

  dptr = delta;
  dptr += 4;    /* skip codec size */
  d = (*dptr++) << 8;  d |= *dptr++;   /* read code either 0008 or 0000 */
  if (dsize < 8) /* NOP */
  {
    dec_info->xs = dec_info->ys = dec_info->xe = dec_info->ye = 0;
    return(ACT_DLTA_NOP);
  }
  if (d & 0x0008) /* Header present */
  {
    y = (*dptr++) << 8; y |= *dptr++;           /* start line */
    dptr += 2;                                  /* unknown */
    lines = (*dptr++) << 8; lines |= *dptr++;   /* number of lines */
    dptr += 2;                                  /* unknown */
  }
  else { y = 0; lines = imagey; }
  while(lines--)				/* loop thru lines */
  {
    xaULONG d,xskip,cnt;
    xskip = *dptr++;				/* skip x pixels */
    if (xskip==0) break;			/* exit */
    cnt = *dptr++;				/* RLE code */

    if (special_flag)
    { xaUBYTE *iptr = (xaUBYTE *)(image + 3*((y * imagex) + (xskip-1)) );
      while(cnt != 0xff)				/* while not EOL */
      {
        if (cnt == 0x00) { xskip = *dptr++; iptr += 3*(xskip-1); }
        else if (cnt < 0x80)				/* run of data */
          while(cnt--) { d = (*dptr++ << 8); d |= *dptr++;
			 QT_Get_RGBColor(&r,&g,&b,d);
			*iptr++ = r; *iptr++ = g; *iptr++ = b; }
        else						/* repeat data */
        { cnt = 0x100 - cnt; d = (*dptr++ << 8); d |= *dptr++;
          QT_Get_RGBColor(&r,&g,&b,d);
          while(cnt--) { *iptr++ = r; *iptr++ = g; *iptr++ = b; }
        }
        cnt = *dptr++;				/* get new RLE code */
      } /* end of line */
    }
    else if ( (x11_bytes_pixel==1) || (map_flag==xaFALSE) )
    { xaUBYTE *iptr = (xaUBYTE *)(image + (y * imagex) + (xskip-1) );
      while(cnt != 0xff)				/* while not EOL */
      {
        if (cnt == 0x00) { xskip = *dptr++; iptr += (xskip-1); }
        else if (cnt < 0x80)				/* run of data */
          while(cnt--) { d = (*dptr++ << 8); d |= *dptr++;
		*iptr++ = (xaUBYTE)QT_Get_Color(d,map_flag,map,chdr); }
        else						/* repeat data */
        { cnt = 0x100 - cnt; d = (*dptr++ << 8); d |= *dptr++;
          d = QT_Get_Color(d,map_flag,map,chdr);
          while(cnt--) { *iptr++ = (xaUBYTE)d; }
        }
        cnt = *dptr++;				/* get new RLE code */
      } /* end of line */
    }
    else if (x11_bytes_pixel==4)
    { xaULONG *iptr = (xaULONG *)(image + 4*((y * imagex)+(xskip-1)) );
      while(cnt != 0xff)				/* while not EOL */
      {
        if (cnt == 0x00) { xskip = *dptr++; iptr += (xskip-1); }
        else if (cnt < 0x80)				/* run of data */
          while(cnt--) { d = (*dptr++ << 8); d |= *dptr++;
		*iptr++ = (xaULONG)QT_Get_Color(d,map_flag,map,chdr); }
        else						/* repeat data */
        { cnt = 0x100 - cnt; d = (*dptr++ << 8); d |= *dptr++;
          d = QT_Get_Color(d,map_flag,map,chdr);
          while(cnt--) { *iptr++ = (xaULONG)d; }
        }
        cnt = *dptr++;				/* get new RLE code */
      } /* end of line */
    }
    else /* if (x11_bytes_pixel==2) */
    { xaUSHORT *iptr = (xaUSHORT *)(image + 2*((y * imagex)+(xskip-1)) );
      while(cnt != 0xff)				/* while not EOL */
      {
        if (cnt == 0x00) { xskip = *dptr++; iptr += (xskip-1); }
        else if (cnt < 0x80)				/* run of data */
          while(cnt--) { d = (*dptr++ << 8); d |= *dptr++;
		*iptr++ = (xaUSHORT)QT_Get_Color(d,map_flag,map,chdr); }
        else						/* repeat data */
        { cnt = 0x100 - cnt; d = (*dptr++ << 8); d |= *dptr++;
          d = QT_Get_Color(d,map_flag,map,chdr);
          while(cnt--) { *iptr++ = (xaUSHORT)d; }
        }
        cnt = *dptr++;				/* get new RLE code */
      } /* end of line */
    }
    y++;
  } /* end of lines */
 /* one more zero byte */
 dec_info->xs = dec_info->ys = 0; dec_info->xe = imagex; dec_info->ye = imagey;
 if (map_flag==xaTRUE) return(ACT_DLTA_MAPD);
 else return(ACT_DLTA_NORM);
}


xaULONG
QT_Decode_RLE1(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG special = dec_info->special;          void *extra = dec_info->extra;
  XA_CHDR *chdr = dec_info->chdr;
  xaLONG x,y,d,lines; /* xaLONG min_x,max_x,min_y,max_y; */
  xaUBYTE *dptr;

  dptr = delta;
  dptr += 4;    /* skip codec size */
  d = (*dptr++) << 8;  d |= *dptr++;   /* read code either 0008 or 0000 */
  if (dsize < 8) /* NOP */
  { 
    dec_info->xs = dec_info->ys = dec_info->xe = dec_info->ye = 0;
    return(ACT_DLTA_NOP);
  }
  if (d & 0x0008) /* Header present */
  {
    y = (*dptr++) << 8; y |= *dptr++;           /* start line */
    dptr += 2;                                  /* unknown */
    lines = (*dptr++) << 8; lines |= *dptr++;   /* number of lines */
    dptr += 2;                                  /* unknown */
  }
  else { y = 0; lines = imagey; }
  x = 0; y--; lines++;
  while(lines)				/* loop thru lines */
  {
    xaULONG d,xskip,cnt;

    xskip = *dptr++;				/* skip x pixels */
    cnt = *dptr++;				/* RLE code */
    if (cnt == 0) break; 			/* exit */
    
/* this can be removed */
    if ((xskip == 0x80) && (cnt == 0x00))  /* end of codec */
    {
	  lines = 0; y++; x = 0; 
    }
    else if ((xskip == 0x80) && (cnt == 0xff)) /* skip line */
	{lines--; y++; x = 0; }
    else
    {
      if (xskip & 0x80) {lines--; y++; x = xskip & 0x7f;}
      else x += xskip;

      if (cnt < 0x80)				/* run of data */
      { 
        xaUBYTE *bptr; xaUSHORT *sptr; xaULONG *lptr;
	if ((x11_bytes_pixel==1) || (map_flag==xaFALSE) )
		bptr = (xaUBYTE *)(image + (y * imagex) + (x << 4) );
	else if (x11_bytes_pixel==2)
		sptr = (xaUSHORT *)(image + 2*(y * imagex) + (x << 5) );
        else lptr = (xaULONG *)(image + 4*(y * imagex) + (x << 6) );
        x += cnt;
        while(cnt--) 
        { xaULONG i,mask;
          d = (*dptr++ << 8); d |= *dptr++;
          mask = 0x8000;
          for(i=0;i<16;i++)
          {
            if (map_flag==xaFALSE) 
		{ if (d & mask) *bptr++ = 0;  else *bptr++ = 1; }
            else if (x11_bytes_pixel==1) {if (d & mask) *bptr++=(xaUBYTE)map[0];
					else *bptr++=(xaUBYTE)map[1];}
            else if (x11_bytes_pixel==2) {if (d & mask) *sptr++ =(xaUSHORT)map[0];
					else *sptr++ =(xaUSHORT)map[1]; }
            else { if (d & mask) *lptr++ = (xaULONG)map[0]; 
					else *lptr++ = (xaULONG)map[1]; }
            mask >>= 1;
          }
        }
      } /* end run */ 
      else				/* repeat data */
      { 
        xaUBYTE *bptr; xaUSHORT *sptr; xaULONG *lptr;
	if ((x11_bytes_pixel==1) || (map_flag==xaFALSE) )
		bptr = (xaUBYTE *)(image + (y * imagex) + (x << 4) );
	else if (x11_bytes_pixel==2)
		sptr = (xaUSHORT *)(image + 2*(y * imagex) + (x << 5) );
        else lptr = (xaULONG *)(image + 4*(y * imagex) + (x << 6) );
        cnt = 0x100 - cnt;
        x += cnt;
        d = (*dptr++ << 8); d |= *dptr++;
        while(cnt--) 
        { xaULONG i,mask;
          mask = 0x8000;
          for(i=0;i<16;i++)
          {
            if (map_flag==xaFALSE) 
		{ if (d & mask) *bptr++ = 0;  else *bptr++ = 1; }
            else if (x11_bytes_pixel==1) {if (d & mask) *bptr++=(xaUBYTE)map[0];
					else *bptr++=(xaUBYTE)map[1];}
            else if (x11_bytes_pixel==2) {if (d & mask) *sptr++ =(xaUSHORT)map[0];
					else *sptr++ =(xaUSHORT)map[1]; }
            else { if (d & mask) *lptr++ = (xaULONG)map[0]; 
					else *lptr++ = (xaULONG)map[1]; }
            mask >>= 1;
          }
        }
      } /* end repeat */
    } /* end of code */
  } /* end of lines */
 dec_info->xs = dec_info->ys = 0; dec_info->xe = imagex; dec_info->ye = imagey;
 if (map_flag==xaTRUE) return(ACT_DLTA_MAPD);
 else return(ACT_DLTA_NORM);
}

xaULONG
QT_Decode_RAW(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG special = dec_info->special;          void *extra = dec_info->extra;
  XA_CHDR *chdr = dec_info->chdr;
  xaUBYTE *dptr = delta;
  xaLONG i = imagex * imagey;
  
  if (map_flag==xaFALSE) { xaUBYTE *iptr = (xaUBYTE *)image; 
				while(i--) *iptr++ = (xaUBYTE)*dptr++; }
  else if (x11_bytes_pixel==1) { xaUBYTE *iptr = (xaUBYTE *)image; 
				while(i--) *iptr++ = (xaUBYTE)map[*dptr++]; }
  else if (x11_bytes_pixel==2) { xaUSHORT *iptr = (xaUSHORT *)image; 
				while(i--) *iptr++ = (xaUSHORT)map[*dptr++]; }
  else { xaULONG *iptr = (xaULONG *)image; 
				while(i--) *iptr++ = (xaULONG)map[*dptr++]; }

  dec_info->xs = dec_info->ys = 0; dec_info->xe = imagex; dec_info->ye = imagey;
  if (map_flag==xaTRUE) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}


xaULONG
QT_Decode_RAW4(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG special = dec_info->special;          void *extra = dec_info->extra;
  XA_CHDR *chdr = dec_info->chdr;
  xaUBYTE *dp = delta;
  xaLONG i = (imagex * imagey) >> 1;

{ FILE *fo;
  fo = fopen("raw4.out","a");
  while(i--)
  { xaULONG d0,d = *dp++;  d0 = (d>>4)&0x0f;
    fputc(d0,fo);
    fputc((d&0x0f),fo);
  }
  fclose(fo);
}
  dp = delta;
  i = (imagex * imagey) >> 1;

/* POD QUESTION: is imagex a multiple of 2??? */
  if (map_flag==xaFALSE) { xaUBYTE *iptr = (xaUBYTE *)image; 
    while(i--) { xaUBYTE d = *dp++; *iptr++ = (d>>4); *iptr++ = d & 0xf; }
  }
  else if (x11_bytes_pixel==1) { xaUBYTE *iptr = (xaUBYTE *)image; 
    while(i--) { xaULONG d = (xaULONG)*dp++;
                 *iptr++ = (xaUBYTE)map[d>>4]; *iptr++ = (xaUBYTE)map[d&15]; }
  }
  else if (x11_bytes_pixel==4) { xaULONG *iptr = (xaULONG *)image; 
    while(i--) { xaULONG d = (xaULONG)*dp++;
                 *iptr++ = (xaULONG)map[d>>4]; *iptr++ = (xaULONG)map[d&15]; }
  }
  else /*(x11_bytes_pixel==2)*/ { xaUSHORT *iptr = (xaUSHORT *)image; 
    while(i--) { xaULONG d = (xaULONG)*dp++;
                 *iptr++ = (xaUSHORT)map[d>>4]; *iptr++ = (xaUSHORT)map[d&15]; }
  }
  dec_info->xs = dec_info->ys = 0; dec_info->xe = imagex; dec_info->ye = imagey;
  if (map_flag==xaTRUE) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}

#define QT_RAW_READ16RGB(dp,r,g,b) { xaULONG _d = *dp++ << 8; _d |= *dp++; \
  r = (_d >> 10) & 0x1f; g = (_d >> 5) & 0x1f; b = _d & 0x1f;	\
  r = (r << 3) | (r >> 2); g = (g << 3) | (g >> 2); b = (b << 3) | (b >> 2); }

/****************** RAW CODEC DEPTH 16 *********************************
 *  1 unused bit. 5 bits R, G, B
 *
 *********************************************************************/
xaULONG
QT_Decode_RAW16(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG special = dec_info->special;          void *extra = dec_info->extra;
  XA_CHDR *chdr = dec_info->chdr;
  xaUBYTE *dp = delta;
  xaULONG special_flag = special & 0x0001;
  xaLONG i = imagex * imagey;
  
  if (chdr) { if (chdr->new_chdr) chdr = chdr->new_chdr; }

  if (special_flag)
  { xaUBYTE *iptr = (xaUBYTE *)image; 
    while(i--) 
    { xaULONG r,g,b;  QT_RAW_READ16RGB(dp,r,g,b);
      *iptr++ = r; *iptr++ = g; *iptr++ = b;}
  }
  else if ( (map_flag==xaFALSE) || (x11_bytes_pixel==1) )
  { xaUBYTE *iptr = (xaUBYTE *)image; 
    while(i--) 
    { xaULONG r,g,b; QT_RAW_READ16RGB(dp,r,g,b);
      *iptr++ = (xaUBYTE)XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
    }
  }
  else if (x11_bytes_pixel==4)
  { xaULONG *iptr = (xaULONG *)image; 
    while(i--) 
    { xaULONG r,g,b;  QT_RAW_READ16RGB(dp,r,g,b);
      *iptr++ = (xaULONG)XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
    }
  }
  else /* (x11_bytes_pixel==2) */
  { xaUSHORT *iptr = (xaUSHORT *)image; 
    while(i--) 
    { xaULONG r,g,b;  QT_RAW_READ16RGB(dp,r,g,b);
      *iptr++ = (xaUSHORT)XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
    }
  }
  dec_info->xs = dec_info->ys = 0; dec_info->xe = imagex; dec_info->ye = imagey;
  if (map_flag==xaTRUE) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}


/****************** RAW CODEC DEPTH 24 *********************************
 *  R G B
 *
 *********************************************************************/
xaULONG
QT_Decode_RAW24(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG special = dec_info->special;          void *extra = dec_info->extra;
  XA_CHDR *chdr = dec_info->chdr;
  xaUBYTE *dp = delta;
  xaULONG special_flag = special & 0x0001;
  xaLONG i = imagex * imagey;
  
  if (chdr) { if (chdr->new_chdr) chdr = chdr->new_chdr; }

  dp = delta;
  i = imagex * imagey;

  if (special_flag)
  { xaUBYTE *iptr = (xaUBYTE *)image; 
    while(i--) { *iptr++ = *dp++; *iptr++ = *dp++; *iptr++ = *dp++;}
  }
  else if ( (map_flag==xaFALSE) || (x11_bytes_pixel==1) )
  { xaUBYTE *iptr = (xaUBYTE *)image; 
    while(i--) 
    { xaULONG r,g,b;
      r = (xaULONG)*dp++; g = (xaULONG)*dp++; b = (xaULONG)*dp++;
      *iptr++ = (xaUBYTE)XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
    }
  }
  else if (x11_bytes_pixel==4)
  { xaULONG *iptr = (xaULONG *)image; 
    while(i--) 
    { xaULONG r,g,b; 
      r = (xaULONG)*dp++; g = (xaULONG)*dp++; b = (xaULONG)*dp++;
      *iptr++ = (xaULONG)XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
    }
  }
  else /* (x11_bytes_pixel==2) */
  { xaUSHORT *iptr = (xaUSHORT *)image; 
    while(i--) 
    { xaULONG r,g,b; 
      r = (xaULONG)*dp++; g = (xaULONG)*dp++; b = (xaULONG)*dp++;
      *iptr++ = (xaUSHORT)XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
    }
  }
  dec_info->xs = dec_info->ys = 0; dec_info->xe = imagex; dec_info->ye = imagey;
  if (map_flag==xaTRUE) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}

/****************** RAW CODEC DEPTH 32 *********************************
 *  0 R G B
 *
 *********************************************************************/
xaULONG
QT_Decode_RAW32(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG special = dec_info->special;          void *extra = dec_info->extra;
  XA_CHDR *chdr = dec_info->chdr;
  xaUBYTE *dp = delta;
  xaULONG special_flag = special & 0x0001;
  xaLONG i = imagex * imagey;
  
  if (chdr) { if (chdr->new_chdr) chdr = chdr->new_chdr; }

  if (special_flag)
  { xaUBYTE *iptr = (xaUBYTE *)image; 
    while(i--) {dp++; *iptr++ = *dp++; *iptr++ = *dp++; *iptr++ = *dp++;}
  }
  else if ( (map_flag==xaFALSE) || (x11_bytes_pixel==1) )
  { xaUBYTE *iptr = (xaUBYTE *)image; 
    while(i--) 
    { xaULONG r,g,b;
      dp++; r = (xaULONG)*dp++; g = (xaULONG)*dp++; b = (xaULONG)*dp++;
      *iptr++ = (xaUBYTE)XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
    }
  }
  else if (x11_bytes_pixel==4)
  { xaULONG *iptr = (xaULONG *)image; 
    while(i--) 
    { xaULONG r,g,b; 
      dp++; r = (xaULONG)*dp++; g = (xaULONG)*dp++; b = (xaULONG)*dp++;
      *iptr++ = (xaULONG)XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
    }
  }
  else /* (x11_bytes_pixel==2) */
  { xaUSHORT *iptr = (xaUSHORT *)image; 
    while(i--) 
    { xaULONG r,g,b; 
      dp++; r = (xaULONG)*dp++; g = (xaULONG)*dp++; b = (xaULONG)*dp++;
      *iptr++ = (xaUSHORT)XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
    }
  }
  dec_info->xs = dec_info->ys = 0; dec_info->xe = imagex; dec_info->ye = imagey;
  if (map_flag==xaTRUE) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}


#define QT_SMC_O2I(i,o,rinc) { \
*i++ = *o++; *i++ = *o++; *i++ = *o++; *i++ = *o++; i += rinc; o += rinc; \
*i++ = *o++; *i++ = *o++; *i++ = *o++; *i++ = *o++; i += rinc; o += rinc; \
*i++ = *o++; *i++ = *o++; *i++ = *o++; *i++ = *o++; i += rinc; o += rinc; \
*i++ = *o++; *i++ = *o++; *i++ = *o++; *i++ = *o++;  } 

#define QT_SMC_C1(i,c,rinc) { \
*i++ = c; *i++ = c; *i++ = c; *i++ = c;  i += rinc; \
*i++ = c; *i++ = c; *i++ = c; *i++ = c;  i += rinc; \
*i++ = c; *i++ = c; *i++ = c; *i++ = c;  i += rinc; \
*i++ = c; *i++ = c; *i++ = c; *i++ = c;  i += rinc; }

#define QT_SMC_C2(i,c0,c1,msk,rinc) { \
*i++ =(msk&0x80)?c1:c0; *i++ =(msk&0x40)?c1:c0; \
*i++ =(msk&0x20)?c1:c0; *i++ =(msk&0x10)?c1:c0; i += rinc; \
*i++ =(msk&0x08)?c1:c0; *i++ =(msk&0x04)?c1:c0; \
*i++ =(msk&0x02)?c1:c0; *i++ =(msk&0x01)?c1:c0; }

#define QT_SMC_C4(i,CST,c,mska,mskb,rinc) { \
*i++ = (CST)(c[(mska>>6) & 0x03]); *i++ = (CST)(c[(mska>>4) & 0x03]); \
*i++ = (CST)(c[(mska>>2) & 0x03]); *i++ = (CST)(c[mska & 0x03]); i+=rinc; \
*i++ = (CST)(c[(mskb>>6) & 0x03]); *i++ = (CST)(c[(mskb>>4) & 0x03]); \
*i++ = (CST)(c[(mskb>>2) & 0x03]); *i++ = (CST)(c[mskb & 0x03]); }

#define QT_SMC_C8(i,CST,c,msk,rinc) { \
*i++ = (CST)(c[(msk>>21) & 0x07]); *i++ = (CST)(c[(msk>>18) & 0x07]); \
*i++ = (CST)(c[(msk>>15) & 0x07]); *i++ = (CST)(c[(msk>>12) & 0x07]); i+=rinc; \
*i++ = (CST)(c[(msk>> 9) & 0x07]); *i++ = (CST)(c[(msk>> 6) & 0x07]); \
*i++ = (CST)(c[(msk>> 3) & 0x07]); *i++ = (CST)(c[msk & 0x07]); }

#define QT_SMC_C16m(i,dp,CST,map,rinc) { \
*i++ =(CST)map[*dp++]; *i++ =(CST)map[*dp++]; \
*i++ =(CST)map[*dp++]; *i++ =(CST)map[*dp++]; i += rinc; \
*i++ =(CST)map[*dp++]; *i++ =(CST)map[*dp++]; \
*i++ =(CST)map[*dp++]; *i++ =(CST)map[*dp++]; }

#define QT_SMC_C16(i,dp,CST) { \
*i++ =(CST)(*dp++); *i++ =(CST)(*dp++); \
*i++ =(CST)(*dp++); *i++ =(CST)(*dp++); }

xaULONG
QT_Decode_SMC(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG special = dec_info->special;          void *extra = dec_info->extra;
  XA_CHDR *chdr = dec_info->chdr;
  xaLONG x,y,len,row_inc; /* xaLONG min_x,max_x,min_y,max_y; */
  xaUBYTE *dptr;
  xaULONG i,cnt,hicode,code;
  xaULONG *c;

  smc_8cnt = smc_Acnt = smc_Ccnt = 0;

  dec_info->xs = dec_info->ys = 0; dec_info->xe = imagex; dec_info->ye = imagey;
  dptr = delta;
  x = y = 0;
  row_inc = imagex - 4;

  dptr++;				/* skip over 0xe1 */
  len =(*dptr++)<<16; len |= (*dptr++)<< 8; len |= (*dptr++); /* Read Len */
  len -= 4;				/* read 4 already */
  while(len > 0)
  {
    code = *dptr++; len--; hicode = code & 0xf0;
    switch(hicode)
    {
      case 0x00: /* SKIPs */
      case 0x10:
	if (hicode == 0x10) {cnt = 1 + *dptr++; len -= 1;}
	else cnt = 1 + (code & 0x0f);
        while(cnt--) {x += 4; if (x >= imagex) { x = 0; y += 4; } }
	break;
      case 0x20: /* Repeat Last Block */
      case 0x30:
	{ xaLONG tx,ty;
	  if (hicode == 0x30) {cnt = 1 + *dptr++; len--;}
	  else cnt = 1 + (code & 0x0f);
	  if (x==0) {ty = y-4; tx = imagex-4;} else {ty=y; tx = x-4;}

	  while(cnt--)
	  { 
	    if ( (x11_bytes_pixel==1) || (map_flag==xaFALSE) )
	    { xaUBYTE *i_ptr = (xaUBYTE *)(image + y * imagex + x);
	      xaUBYTE *o_ptr = (xaUBYTE *)(image + ty * imagex + tx);
	      QT_SMC_O2I(i_ptr,o_ptr,row_inc);
	    } else if (x11_bytes_pixel==2)
	    { xaUSHORT *i_ptr = (xaUSHORT *)(image + 2*(y * imagex + x) );
	      xaUSHORT *o_ptr = (xaUSHORT *)(image + 2*(ty * imagex + tx) );
	      QT_SMC_O2I(i_ptr,o_ptr,row_inc);
	    } else /* if (x11_bytes_pixel==4) */
	    { xaULONG *i_ptr = (xaULONG *)(image + 4*(y * imagex + x) );
	      xaULONG *o_ptr = (xaULONG *)(image + 4*(ty * imagex + tx) );
	      QT_SMC_O2I(i_ptr,o_ptr,row_inc);
	    }
	    x += 4; if (x >= imagex) { x = 0; y += 4; }
	  }
	}
	break;
      case 0x40: /* */
      case 0x50:
	{ xaULONG cnt,cnt1;
	  xaULONG m_cnt,m_cnt1;
	  xaLONG m_tx,m_ty;
          xaLONG tx,ty;
	  if (hicode == 0x50) 
	  {  
	     m_cnt1 = 1 + *dptr++; len--; 
	     m_cnt = 2;
	  }
          else 
	  {
	    m_cnt1 = (1 + (code & 0x0f));
	    m_cnt = 2;
	  }
          m_tx = x-(xaLONG)(4 * m_cnt); m_ty = y; 
	  if (m_tx < 0) {m_tx += imagex; m_ty -= 4;}
	  cnt1 = m_cnt1;
	  while(cnt1--)
	  {
	    cnt = m_cnt; tx = m_tx; ty = m_ty;
	    while(cnt--)
	    { 
	      if ( (x11_bytes_pixel==1) || (map_flag==xaFALSE) )
	      { xaUBYTE *i_ptr = (xaUBYTE *)(image + y * imagex + x);
		xaUBYTE *o_ptr = (xaUBYTE *)(image + ty * imagex + tx);
		QT_SMC_O2I(i_ptr,o_ptr,row_inc);
	      } else if (x11_bytes_pixel==2)
	      { xaUSHORT *i_ptr = (xaUSHORT *)(image + 2*(y * imagex + x));
		xaUSHORT *o_ptr = (xaUSHORT *)(image + 2*(ty * imagex + tx));
		QT_SMC_O2I(i_ptr,o_ptr,row_inc);
	      } else 
	      { xaULONG *i_ptr = (xaULONG *)(image + 4*(y * imagex + x));
		xaULONG *o_ptr = (xaULONG *)(image + 4*(ty * imagex + tx));
		QT_SMC_O2I(i_ptr,o_ptr,row_inc);
	      }
	      x += 4; if (x >= imagex) { x = 0; y += 4; }
	      tx += 4; if (tx >= imagex) { tx = 0; ty += 4; }
	    } /* end of cnt */
	  } /* end of cnt1 */
	}
	break;

      case 0x60: /* Repeat Data */
      case 0x70:
	{ xaULONG ct,cnt;
	  if (hicode == 0x70) {cnt = 1 + *dptr++; len--;}
	  else cnt = 1 + (code & 0x0f);
	  ct = (map_flag)?(map[*dptr++]):(xaULONG)(*dptr++); len--;
	  while(cnt--)
	  {
	    if ( (x11_bytes_pixel==1) || (map_flag==xaFALSE) )
	    { xaUBYTE *i_ptr = (xaUBYTE *)(image + y * imagex + x);
	      xaUBYTE d = (xaUBYTE)(ct);
	      QT_SMC_C1(i_ptr,d,row_inc);
	    } else if (x11_bytes_pixel==2)
	    { xaUSHORT *i_ptr = (xaUSHORT *)(image + 2*(y * imagex + x));
	      xaUSHORT d = (xaUBYTE)(ct);
	      QT_SMC_C1(i_ptr,d,row_inc);
	    } else
	    { xaULONG *i_ptr = (xaULONG *)(image + 4*(y * imagex + x));
	      QT_SMC_C1(i_ptr,ct,row_inc);
	    }
	    x += 4; if (x >= imagex) { x = 0; y += 4; }
	  }
	}
	break;

      case 0x80: /* 2 colors plus 16 mbits per */
      case 0x90:
        { xaULONG cnt = 1 + (code & 0x0f);
	  if (hicode == 0x80)
	  {
            c = (xaULONG *)&smc_8[ (smc_8cnt * 2) ];  len -= 2;
	    smc_8cnt++; if (smc_8cnt >= SMC_MAX_CNT) smc_8cnt -= SMC_MAX_CNT;
	    for(i=0;i<2;i++) {c[i]=(map_flag)?map[*dptr++]:(xaULONG)(*dptr++);}
	  }
          else { c = (xaULONG *)&smc_8[ ((xaULONG)(*dptr++) << 1) ]; len--; }
	  while(cnt--)
	  { xaULONG msk1,msk0;
	    msk0 = *dptr++; msk1 = *dptr++;  len-= 2;
	    if ( (x11_bytes_pixel==1) || (map_flag==xaFALSE) )
	    { xaUBYTE *i_ptr = (xaUBYTE *)(image + y * imagex + x);
	      xaUBYTE c0=(xaUBYTE)c[0];	xaUBYTE c1=(xaUBYTE)c[1];
	      QT_SMC_C2(i_ptr,c0,c1,msk0,row_inc); i_ptr += row_inc;
	      QT_SMC_C2(i_ptr,c0,c1,msk1,row_inc);
	    } else if (x11_bytes_pixel==2)
	    { xaUSHORT *i_ptr = (xaUSHORT *)(image + 2*(y * imagex + x));
	      xaUSHORT c0=(xaUSHORT)c[0];	xaUSHORT c1=(xaUSHORT)c[1];
	      QT_SMC_C2(i_ptr,c0,c1,msk0,row_inc); i_ptr += row_inc;
	      QT_SMC_C2(i_ptr,c0,c1,msk1,row_inc);
	    } else
	    { xaULONG *i_ptr = (xaULONG *)(image + 4*(y * imagex + x));
	      xaULONG c0=(xaULONG)c[0];	xaULONG c1=(xaULONG)c[1];
	      QT_SMC_C2(i_ptr,c0,c1,msk0,row_inc); i_ptr += row_inc;
	      QT_SMC_C2(i_ptr,c0,c1,msk1,row_inc);
	    }
	    x += 4; if (x >= imagex) { x = 0; y += 4; }
          } 
        } 
	break;

      case 0xA0: /* 4 color + 32 mbits */
      case 0xB0:
        { xaULONG cnt = 1 + (code & 0xf);
          if (hicode == 0xA0)
          {
            c = (xaULONG *)&smc_A[ (smc_Acnt << 2) ]; len -= 4;
            smc_Acnt++; if (smc_Acnt >= SMC_MAX_CNT) smc_Acnt -= SMC_MAX_CNT;
            for(i=0;i<4;i++) {c[i]=(map_flag)?map[*dptr++]:(xaULONG)(*dptr++);}
          }
          else { c = (xaULONG *)&smc_A[ ((xaULONG)(*dptr++) << 2) ]; len--; }
	  while(cnt--)
	  { xaUBYTE msk0,msk1,msk2,msk3; 
	    msk0 = *dptr++;	msk1 = *dptr++; 
	    msk2 = *dptr++;	msk3 = *dptr++;		len -= 4;
	    if ( (x11_bytes_pixel==1) || (map_flag==xaFALSE) )
	    { xaUBYTE *i_ptr = (xaUBYTE *)(image + y * imagex + x);
	      QT_SMC_C4(i_ptr,xaUBYTE,c,msk0,msk1,row_inc); i_ptr += row_inc;
	      QT_SMC_C4(i_ptr,xaUBYTE,c,msk2,msk3,row_inc);
	    } else if (x11_bytes_pixel==2)
	    { xaUSHORT *i_ptr = (xaUSHORT *)(image + 2*(y * imagex + x));
	      QT_SMC_C4(i_ptr,xaUSHORT,c,msk0,msk1,row_inc); i_ptr += row_inc;
	      QT_SMC_C4(i_ptr,xaUSHORT,c,msk2,msk3,row_inc);
	    } else
	    { xaULONG *i_ptr = (xaULONG *)(image + 4*(y * imagex + x));
	      QT_SMC_C4(i_ptr,xaULONG,c,msk0,msk1,row_inc); i_ptr += row_inc;
	      QT_SMC_C4(i_ptr,xaULONG,c,msk2,msk3,row_inc);
	    }
	    x += 4; if (x >= imagex) { x = 0; y += 4; }
          } 
        } 
	break;

      case 0xC0: /* 8 colors + 48 mbits */
      case 0xD0:
        { xaULONG cnt = 1 + (code & 0xf);
          if (hicode == 0xC0)
          {
            c = (xaULONG *)&smc_C[ (smc_Ccnt << 3) ];   len -= 8;
            smc_Ccnt++; if (smc_Ccnt >= SMC_MAX_CNT) smc_Ccnt -= SMC_MAX_CNT;
            for(i=0;i<8;i++) {c[i]=(map_flag)?map[*dptr++]:(xaULONG)(*dptr++);}
          }
          else { c = (xaULONG *)&smc_C[ ((xaULONG)(*dptr++) << 3) ]; len--; }

	  while(cnt--)
	  { xaULONG t,mbits0,mbits1;
	    t = (*dptr++) << 8; t |= *dptr++;
	    mbits0  = (t & 0xfff0) << 8;  mbits1  = (t & 0x000f) << 8;
	    t = (*dptr++) << 8; t |= *dptr++;
	    mbits0 |= (t & 0xfff0) >> 4;  mbits1 |= (t & 0x000f) << 4;
	    t = (*dptr++) << 8; t |= *dptr++;
	    mbits1 |= (t & 0xfff0) << 8;  mbits1 |= (t & 0x000f);
	    len -= 6;
	    if ( (x11_bytes_pixel==1) || (map_flag==xaFALSE) )
	    { xaUBYTE *i_ptr = (xaUBYTE *)(image + y * imagex + x);
	      QT_SMC_C8(i_ptr,xaUBYTE,c,mbits0,row_inc); i_ptr += row_inc;
	      QT_SMC_C8(i_ptr,xaUBYTE,c,mbits1,row_inc);
	    } else if (x11_bytes_pixel==2)
	    { xaUSHORT *i_ptr = (xaUSHORT *)(image + 2*(y * imagex + x));
	      QT_SMC_C8(i_ptr,xaUSHORT,c,mbits0,row_inc); i_ptr += row_inc;
	      QT_SMC_C8(i_ptr,xaUSHORT,c,mbits1,row_inc);
	    } else
	    { xaULONG *i_ptr = (xaULONG *)(image + 4*(y * imagex + x));
	      QT_SMC_C8(i_ptr,xaULONG,c,mbits0,row_inc); i_ptr += row_inc;
	      QT_SMC_C8(i_ptr,xaULONG,c,mbits1,row_inc);
	    }
	    x += 4; if (x >= imagex) { x = 0; y += 4; }
          } 
        } 
	break;

      case 0xE0: /* 16 colors */
        { xaULONG cnt = 1 + (code & 0x0f);
	  while(cnt--)
	  { 
	    if (map_flag==xaFALSE)
	    { xaUBYTE *i_ptr = (xaUBYTE *)(image + y * imagex + x);
	      QT_SMC_C16(i_ptr,dptr,xaUBYTE); i_ptr += row_inc;
	      QT_SMC_C16(i_ptr,dptr,xaUBYTE); i_ptr += row_inc;
	      QT_SMC_C16(i_ptr,dptr,xaUBYTE); i_ptr += row_inc;
	      QT_SMC_C16(i_ptr,dptr,xaUBYTE);
	    } else if (x11_bytes_pixel==1)
	    { xaUBYTE *i_ptr = (xaUBYTE *)(image + y * imagex + x);
	      QT_SMC_C16m(i_ptr,dptr,xaUBYTE,map,row_inc); i_ptr += row_inc;
	      QT_SMC_C16m(i_ptr,dptr,xaUBYTE,map,row_inc);
	    } else if (x11_bytes_pixel==2)
	    { xaUSHORT *i_ptr = (xaUSHORT *)(image + 2*(y * imagex + x));
	      QT_SMC_C16m(i_ptr,dptr,xaUSHORT,map,row_inc); i_ptr += row_inc;
	      QT_SMC_C16m(i_ptr,dptr,xaUSHORT,map,row_inc);
	    } else
	    { xaULONG *i_ptr = (xaULONG *)(image + 4*(y * imagex + x));
	      QT_SMC_C16m(i_ptr,dptr,xaULONG,map,row_inc); i_ptr += row_inc;
	      QT_SMC_C16m(i_ptr,dptr,xaULONG,map,row_inc);
	    }
	    len -= 16; x += 4; if (x >= imagex) { x = 0; y += 4; }
	  }
	}
	break;

      default:   /* 0xF0 */
	fprintf(stderr,"SMC opcode %lx is unknown\n",code);
	break;
    }
  }
  if (map_flag) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}

xaULONG
QT_Decode_RLE24(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG special = dec_info->special;          void *extra = dec_info->extra;
  XA_CHDR *chdr = dec_info->chdr;
  xaLONG y,d,lines; /* xaULONG min_x,max_x,min_y,max_y; */
  xaULONG special_flag,r,g,b;
  xaUBYTE *dptr;

  if (chdr) { if (chdr->new_chdr) chdr = chdr->new_chdr; }

  special_flag = special & 0x0001;

  dptr = delta;
  dptr += 4;    /* skip codec size */
  d = (*dptr++) << 8;  d |= *dptr++;   /* read code either 0008 or 0000 */
  if (dsize < 8) /* NOP */
  {  
    dec_info->xs = dec_info->ys = dec_info->xe = dec_info->ye = 0;
    return(ACT_DLTA_NOP);
  }
  if (d & 0x0008) /* Header present */
  {
    y = (*dptr++) << 8; y |= *dptr++;		/* start line */
    dptr += 2;					/* unknown */
    lines = (*dptr++) << 8; lines |= *dptr++;	/* number of lines */
    dptr += 2;					/* unknown */
  }
  else { y = 0; lines = imagey; }

  while(lines--)				/* loop thru lines */
  {
    xaULONG d,xskip,cnt;
    xskip = *dptr++;				/* skip x pixels */
    if (xskip == 0) break; 			/* exit */
    cnt = *dptr++;				/* RLE code */

    if (special_flag)
    { xaUBYTE *iptr = (xaUBYTE *)(image + 3*((y * imagex) + (xskip-1)) );
      while(cnt != 0xff)				/* while not EOL */
      {
        if (cnt == 0x00) { xskip = *dptr++; iptr += 3*(xskip-1); }
        else if (cnt < 0x80)				/* run of data */
          while(cnt--) { r = *dptr++; g = *dptr++; b = *dptr++;
			*iptr++ = (xaUBYTE)r; *iptr++ = (xaUBYTE)g; 
					    *iptr++ = (xaUBYTE)b; }
        else						/* repeat data */
        { cnt = 0x100 - cnt; r = *dptr++; g = *dptr++; b = *dptr++;
          while(cnt--) { *iptr++ = (xaUBYTE)r; *iptr++ = (xaUBYTE)g; 
					     *iptr++ = (xaUBYTE)b; }
        }
        cnt = *dptr++;				/* get new RLE code */
      } /* end of line */
    }
    else if ( (x11_bytes_pixel==1) || (map_flag==xaFALSE) )
    { xaUBYTE *iptr = (xaUBYTE *)(image + (y * imagex) + (xskip-1) );
      while(cnt != 0xff)				/* while not EOL */
      {
        if (cnt == 0x00) { xskip = *dptr++; iptr += (xskip-1); }
        else if (cnt < 0x80)				/* run of data */
          while(cnt--) { r = *dptr++; g = *dptr++; b = *dptr++;
		*iptr++ = (xaUBYTE)XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr); }
        else						/* repeat data */
        { cnt = 0x100 - cnt; r = *dptr++; g = *dptr++; b = *dptr++;
          d = XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
          while(cnt--) { *iptr++ = (xaUBYTE)d; }
        }
        cnt = *dptr++;				/* get new RLE code */
      } /* end of line */
    }
    else if (x11_bytes_pixel==4)
    { xaULONG *iptr = (xaULONG *)(image + 4*((y * imagex)+(xskip-1)) );
      while(cnt != 0xff)				/* while not EOL */
      {
        if (cnt == 0x00) { xskip = *dptr++; iptr += (xskip-1); }
        else if (cnt < 0x80)				/* run of data */
          while(cnt--) { r = *dptr++; g = *dptr++; b = *dptr++;
		*iptr++ = (xaULONG)XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr); }
        else						/* repeat data */
        { cnt = 0x100 - cnt; r = *dptr++; g = *dptr++; b = *dptr++;
          d = XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
          while(cnt--) { *iptr++ = (xaULONG)d; }
        }
        cnt = *dptr++;				/* get new RLE code */
      } /* end of line */
    }
    else /* if (x11_bytes_pixel==2) */
    { xaUSHORT *iptr = (xaUSHORT *)(image + 2*((y * imagex)+(xskip-1)) );
      while(cnt != 0xff)				/* while not EOL */
      {
        if (cnt == 0x00) { xskip = *dptr++; iptr += (xskip-1); }
        else if (cnt < 0x80)				/* run of data */
          while(cnt--) { r = *dptr++; g = *dptr++; b = *dptr++;
		*iptr++ = (xaUSHORT)XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr); }
        else						/* repeat data */
        { cnt = 0x100 - cnt; r = *dptr++; g = *dptr++; b = *dptr++;
          d = XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
          while(cnt--) { *iptr++ = (xaUSHORT)d; }
        }
        cnt = *dptr++;				/* get new RLE code */
      } /* end of line */
    }
    y++;
  } /* end of lines */
 /* one more zero byte */
  dec_info->xs = dec_info->ys = 0; dec_info->xe = imagex; dec_info->ye = imagey;
  if (map_flag==xaTRUE) return(ACT_DLTA_MAPD);
  else return(ACT_DLTA_NORM);
}

xaULONG
QT_Decode_RLE32(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG special = dec_info->special;          void *extra = dec_info->extra;
  XA_CHDR *chdr = dec_info->chdr;
  xaLONG y,d,lines; /* xaULONG min_x,max_x,min_y,max_y; */
  xaULONG special_flag,r,g,b;
  xaUBYTE *dp;

  if (chdr) { if (chdr->new_chdr) chdr = chdr->new_chdr; }

  special_flag = special & 0x0001;

  dp = delta;
  dp += 4;    /* skip codec size */
  d = (*dp++) << 8;  d |= *dp++;   /* read code either 0008 or 0000 */
  if (dsize < 8) /* NOP */
  {
    dec_info->xs = dec_info->ys = dec_info->xe = dec_info->ye = 0;
    return(ACT_DLTA_NOP);
  }
  if (d & 0x0008) /* Header present */
  {
    y = (*dp++) << 8; y |= *dp++;           /* start line */
    dp += 2;                                  /* unknown */
    lines = (*dp++) << 8; lines |= *dp++;   /* number of lines */
    dp += 2;                                  /* unknown */
  }
  else { y = 0; lines = imagey; }
  while(lines--)				/* loop thru lines */
  {
    xaULONG d,xskip,cnt;
    xskip = *dp++;				/* skip x pixels */
    if (xskip == 0) break;			/* exit */
    cnt = *dp++;				/* RLE code */

    if (special_flag)
    { xaUBYTE *iptr = (xaUBYTE *)(image + 3*((y * imagex) + (xskip-1)) );
      while(cnt != 0xff)				/* while not EOL */
      {
        if (cnt == 0x00) { xskip = *dp++; iptr += 3*(xskip-1); }
        else if (cnt < 0x80)				/* run of data */
          while(cnt--) { dp++; r = *dp++; g = *dp++; b = *dp++;
			*iptr++ = (xaUBYTE)r; *iptr++ = (xaUBYTE)g; 
					    *iptr++ = (xaUBYTE)b; }
        else						/* repeat data */
        { cnt = 0x100 - cnt; dp++; r = *dp++; g = *dp++; b = *dp++;
          while(cnt--) { *iptr++ = (xaUBYTE)r; *iptr++ = (xaUBYTE)g; 
					     *iptr++ = (xaUBYTE)b; }
        }
        cnt = *dp++;				/* get new RLE code */
      } /* end of line */
    }
    else if ( (x11_bytes_pixel==1) || (map_flag==xaFALSE) )
    { xaUBYTE *iptr = (xaUBYTE *)(image + (y * imagex) + (xskip-1) );
      while(cnt != 0xff)				/* while not EOL */
      {
        if (cnt == 0x00) { xskip = *dp++; iptr += (xskip-1); }
        else if (cnt < 0x80)				/* run of data */
          while(cnt--) { dp++; r = *dp++; g = *dp++; b = *dp++;
		*iptr++ = (xaUBYTE)XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr); }
        else						/* repeat data */
        { cnt = 0x100 - cnt; dp++; r = *dp++; g = *dp++; b = *dp++;
          d = XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
          while(cnt--) { *iptr++ = (xaUBYTE)d; }
        }
        cnt = *dp++;				/* get new RLE code */
      } /* end of line */
    }
    else if (x11_bytes_pixel==4)
    { xaULONG *iptr = (xaULONG *)(image + 4*((y * imagex)+(xskip-1)) );
      while(cnt != 0xff)				/* while not EOL */
      {
        if (cnt == 0x00) { xskip = *dp++; iptr += (xskip-1); }
        else if (cnt < 0x80)				/* run of data */
          while(cnt--) { dp++; r = *dp++; g = *dp++; b = *dp++;
		*iptr++ = (xaULONG)XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr); }
        else						/* repeat data */
        { cnt = 0x100 - cnt; dp++; r = *dp++; g = *dp++; b = *dp++;
          d = XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
          while(cnt--) { *iptr++ = (xaULONG)d; }
        }
        cnt = *dp++;				/* get new RLE code */
      } /* end of line */
    }
    else /* if (x11_bytes_pixel==2) */
    { xaUSHORT *iptr = (xaUSHORT *)(image + 2*((y * imagex)+(xskip-1)) );
      while(cnt != 0xff)				/* while not EOL */
      {
        if (cnt == 0x00) { xskip = *dp++; iptr += (xskip-1); }
        else if (cnt < 0x80)				/* run of data */
          while(cnt--) { dp++; r = *dp++; g = *dp++; b = *dp++;
		*iptr++ = (xaUSHORT)XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr); }
        else						/* repeat data */
        { cnt = 0x100 - cnt; dp++; r = *dp++; g = *dp++; b = *dp++;
          d = XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
          while(cnt--) { *iptr++ = (xaUSHORT)d; }
        }
        cnt = *dp++;				/* get new RLE code */
      } /* end of line */
    }
    y++;
  } /* end of lines */
 /* one more zero byte */
  dec_info->xs = dec_info->ys = 0; dec_info->xe = imagex; dec_info->ye = imagey;
 if (map_flag==xaTRUE) return(ACT_DLTA_MAPD);
 else return(ACT_DLTA_NORM);
}



/*********************************************
 * Read and Parse Audio Codecs
 *
 **********/
void QT_Read_Audio_STSD(fin)
FILE *fin;
{ xaULONG i,version,num,cur,sup;
  version = UTIL_Get_MSB_Long(fin);
  num = UTIL_Get_MSB_Long(fin);
  DEBUG_LEVEL2 fprintf(stderr,"     ver = %lx  num = %lx\n", version,num);
  if (qts_codecs == 0)
  { qts_codec_num = num;
    qts_codecs = (QTS_CODEC_HDR *)malloc(qts_codec_num * sizeof(QTS_CODEC_HDR));
    if (qts_codecs==0) TheEnd1("QT STSD: malloc err");
    cur = 0;
  }
  else
  { QTS_CODEC_HDR *tcodecs;
    tcodecs = (QTS_CODEC_HDR *)malloc((qts_codec_num+num) * sizeof(QTS_CODEC_HDR))
;
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
    sup |= QT_Read_Audio_Codec_HDR( &qts_codecs[cur], fin );
    cur++;
  }
  if (sup == 0)
  {
    if (qt_audio_attempt == xaTRUE) 
    {
      fprintf(stderr,"QT Audio Codec not supported\n");
      qt_audio_attempt = xaFALSE;
    }
  }
}


xaULONG QT_Read_Audio_Codec_HDR(c_hdr,fin)
QTS_CODEC_HDR *c_hdr;
FILE *fin;
{ xaULONG len;
  xaULONG ret = 1;
  len			= UTIL_Get_MSB_Long(fin);
  c_hdr->compression	= UTIL_Get_MSB_Long(fin);
  c_hdr->dref_id	= UTIL_Get_MSB_Long(fin);
  c_hdr->version	= UTIL_Get_MSB_Long(fin);
  c_hdr->codec_rev	= UTIL_Get_MSB_Long(fin);
  c_hdr->vendor		= UTIL_Get_MSB_Long(fin);
  c_hdr->chan_num	= UTIL_Get_MSB_UShort(fin);
  c_hdr->bits_samp	= UTIL_Get_MSB_UShort(fin);
  c_hdr->comp_id	= UTIL_Get_MSB_UShort(fin);
  c_hdr->pack_size	= UTIL_Get_MSB_UShort(fin);
  c_hdr->samp_rate	= UTIL_Get_MSB_UShort(fin);
  c_hdr->pad		= UTIL_Get_MSB_UShort(fin);

  if (xa_verbose)
  {
    fprintf(stderr,"  Audio Codec: "); QT_Audio_Type(c_hdr->compression);
    fprintf(stderr," Rate=%ld Chans=%ld Bps=%ld cmp_id=%lx\n",
	c_hdr->samp_rate,c_hdr->chan_num,c_hdr->bits_samp,c_hdr->comp_id);
  }

  if (c_hdr->compression == QT_twos) c_hdr->compression =XA_AUDIO_SIGNED;
  else if (c_hdr->compression == QT_raw) c_hdr->compression =XA_AUDIO_LINEAR;
  else if (c_hdr->compression == QT_raw00) c_hdr->compression =XA_AUDIO_LINEAR;
  else ret = 0;
  if (c_hdr->bits_samp==8) c_hdr->bps = 1;
  else if (c_hdr->bits_samp==16) c_hdr->bps = 2;
  else if (c_hdr->bits_samp==32) c_hdr->bps = 4;
  else c_hdr->bps = 100 + c_hdr->bits_samp;

  if (c_hdr->bps > 2) ret = 0;
  if (c_hdr->chan_num > 2) ret = 0;

  if (c_hdr->bps==2)		
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
{
  switch(type)
  {
    case QT_raw:	fprintf(stderr,"PCM"); break;
    case QT_raw00:	fprintf(stderr,"PCM0"); break;
    case QT_twos:	fprintf(stderr,"TWOS"); break;
    case QT_MAC6:	fprintf(stderr,"MAC6"); break;
    default:		
	fprintf(stderr,"(%c%c%c%c)(%08lx)",((type>>24)&0xff),((type>>16)&0xff),
			((type>>8)&0xff),((type)&0xff),type); 
	break;
  }
}

/**************
 *
 * 
 *******/
void QT_Read_Audio_Data(qt,fin,anim_hdr)
XA_ANIM_SETUP *qt;
FILE *fin;
XA_ANIM_HDR *anim_hdr;
{
  xaULONG ret,i;
  xaULONG cur_s2chunk,nxt_s2chunk;
  xaULONG cur_off,tag;

DEBUG_LEVEL1 fprintf(stderr,"QT_Read_Audio: attempt %lx co# %ld\n",
				qt_audio_attempt,qts_chunkoff_num);
  if (qt_audio_attempt==xaFALSE) return;
  if (qts_samp_sizes == 0)
  {
    if (xa_verbose) fprintf(stderr,"  Audio Codec: No Audio Present.\n");
    return; 
  } 

  cur_off	= 0;
  cur_s2chunk	= 0;
  nxt_s2chunk	= qts_s2chunks[cur_s2chunk + 1].first;
  tag		= qts_s2chunks[cur_s2chunk].tag;
  qt_audio_freq	 = qts_codecs[tag].samp_rate;
  qt_audio_chans = qts_codecs[tag].chan_num;
  qt_audio_bps	 = qts_codecs[tag].bps;
  qt_audio_type	 = qts_codecs[tag].compression;
  qt_audio_end	 = 1;

  /* Initial Silence if any. PODNOTE: Eventually Modify for Middle silence */
  if (qts_init_duration)
  { xaULONG num,snd_size,d;
    xaUBYTE *snd_data;
    num  = (qt_audio_freq * qts_init_duration) / qt_tk_timescale;
    snd_size = num * qt_audio_bps;
    if ((qt_audio_type & XA_AUDIO_TYPE_MASK)==XA_AUDIO_LINEAR) d = 0x80;
    else d = 0x00;
    snd_data = (xaUBYTE *)malloc(snd_size);
    if (snd_data==0) TheEnd1("QT aud_dat: malloc err0");
    memset((char *)(snd_data),d,snd_size); 
    if (XA_Add_Sound(anim_hdr,snd_data,qt_audio_type, -1, qt_audio_freq,
	snd_size, &qt->aud_time, &qt->aud_timelo) == xaFALSE) return;
  }


  /* Loop through chunk offsets */
  for(i=0; i < qts_chunkoff_num; i++)
  { xaULONG size,chunk_off,num_samps,snd_size;

    if ( (i == nxt_s2chunk) && ((cur_s2chunk+1) < qts_s2chunk_num) )
    {
      cur_s2chunk++;
      nxt_s2chunk = qts_s2chunks[cur_s2chunk+1].first;
    }
    num_samps = qts_s2chunks[cur_s2chunk].num; /* * sttz */

    /* Check tags and possibly move to new codec */
    if (qts_s2chunks[cur_s2chunk].tag >= qts_codec_num) 
    {
      fprintf(stderr,"QT Data: Warning stsc chunk invalid %ld tag %ld\n",
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
	fprintf(stderr,"QT UNIQE AUDIO: sz %ld bps %ld\n",size,qt_audio_bps);
    }

    chunk_off =  qts_chunkoffs[i];
    snd_size = num_samps * qt_audio_bps;

DEBUG_LEVEL1 fprintf(stderr,"snd_size %ld  numsamps %ld size %ld bps %ld\n",
		snd_size,num_samps,size,qt_audio_bps);

    if (xa_file_flag == xaTRUE)
    {
      if (XA_Add_Sound(anim_hdr,0,qt_audio_type, chunk_off, qt_audio_freq,
	snd_size, &qt->aud_time, &qt->aud_timelo) == xaFALSE) return;
      if (snd_size > qt->max_faud_size) qt->max_faud_size = snd_size;
    }
    else
    { xaUBYTE *snd_data = (xaUBYTE *)malloc(snd_size);
      if (snd_data==0) TheEnd1("QT aud_dat: malloc err");
      fseek(fin,chunk_off,0);  /* move to start of chunk data */
      ret = fread(snd_data, snd_size, 1, fin);
      if (ret != 1) { fprintf(stderr,"QT: snd rd err\n"); return; }
      if (XA_Add_Sound(anim_hdr,snd_data,qt_audio_type, -1, qt_audio_freq,
	snd_size, &qt->aud_time, &qt->aud_timelo) == xaFALSE) return;
    }
  } /* end of chunk_offset loop */
}


/********
 * Have No Clue
 *
 ****/
void QT_Read_STGS(fin,len)
FILE *fin;
xaLONG len;
{
  xaULONG i,version,num;
  xaULONG samps,pad;

  version	= UTIL_Get_MSB_Long(fin);
  num		= UTIL_Get_MSB_Long(fin); len -= 16;
  qt_stgs_num = 0;
  for(i=0; i<num; i++)
  {
    samps	= UTIL_Get_MSB_Long(fin);
    pad		= UTIL_Get_MSB_Long(fin);	len -= 8;
    qt_stgs_num += samps;
  }
  while(len > 0) {len--; getc(fin); }
}


xaULONG
QT_Decode_YUV2(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG map_flag = dec_info->map_flag;        xaULONG *map = dec_info->map;
  xaULONG special = dec_info->special;          void *extra = dec_info->extra;
  XA_CHDR *chdr = dec_info->chdr;
  xaLONG size;
  xaUBYTE *dptr;
  xaUBYTE *iptr;

  dptr = delta;

  if (chdr) { if (chdr->new_chdr) chdr = chdr->new_chdr; }
  
  size = imagex * imagey;
  iptr = image;
/*POD NOT: switch entire while for loop */
  while(size > 0)
  { xaULONG y0,y1,u,v,r,g,b;

/* THIS IS IT.  Y0 U Y1 V  encodes two pixel Y0/U/V and Y1/U/V */
    y0  = *dptr++;
    u   = (*dptr++)^0x80;
    y1  = *dptr++;
    v   = (*dptr++)^0x80;  size -= 2; /* note: size in pixels */

    yuv_to_rgb(y0,u,v,&r,&g,&b);
    if (special) { *iptr++ = r; *iptr++ = g; *iptr++ = b; }
    else *iptr++  = XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);

    yuv_to_rgb(y1,u,v,&r,&g,&b);
    if (special) { *iptr++ = r; *iptr++ = g; *iptr++ = b; }
    else *iptr++  = XA_RGB24_To_CLR32(r,g,b,map_flag,map,chdr);
  } 
 dec_info->xs = dec_info->ys = 0; dec_info->xe = imagex; dec_info->ye = imagey;
 if (map_flag==xaTRUE) return(ACT_DLTA_MAPD);
 else return(ACT_DLTA_NORM);
}



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
0x8a, 0x7c, 0x77
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

  if (cnum == 16)
  { 
    for(i=0;i<15;i++)
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


/****************************
 *
 *
 ****************/
xaLONG QT_Codec_Query(codec)
XA_CODEC_HDR *codec;
{ xaLONG ret = CODEC_UNKNOWN;   /* default */
  codec->extra = 0;
  codec->xapi_rev = 0x0001;
  codec->decoder = 0;
  codec->description = 0;
  codec->avi_read_ext = 0;

  switch(codec->compression)
  {
    case QT_rle:
        codec->compression      = QT_rle;
        codec->description      = "Apple Animation(RLE)";
        ret = CODEC_SUPPORTED;
	switch(codec->depth)
	{
	  case 40:
	  case 8:	codec->decoder = QT_Decode_RLE;
			codec->x = 4 * ((codec->x + 3)/4);
			break;
	  case 16:	codec->decoder = QT_Decode_RLE16;
			break;
	  case 24:	codec->decoder = QT_Decode_RLE24;
			break;
	  case 32:	codec->decoder = QT_Decode_RLE32;
			break;
	  case 33:
	  case  1:	codec->decoder = QT_Decode_RLE1;
			codec->x = 16 * ((codec->x + 15)/16);
			codec->depth = 1;
			break;
	  default:	ret = CODEC_UNSUPPORTED;	break;
	}
	break;

    case QT_smc:
        codec->compression      = QT_smc;
        codec->description      = "Apple Graphics(SMC)";
        ret = CODEC_SUPPORTED;
	if ((codec->depth==8) || (codec->depth==40))
	{ codec->decoder = QT_Decode_SMC;
	  codec->x = 4 * ((codec->x + 3)/4);
	  codec->y = 4 * ((codec->y + 3)/4);
	}
	else ret = CODEC_UNSUPPORTED;
	break;

    case QT_raw:
        codec->compression      = QT_raw;
        codec->description      = "Apple Uncompressed";
        ret = CODEC_SUPPORTED;
	switch(codec->depth)
	{
	  case 36:
	  case 4:	codec->decoder = QT_Decode_RAW4;
			break;
	  case 40:
	  case 8:	codec->decoder = QT_Decode_RAW;
			break;
	  case 16:	codec->decoder = QT_Decode_RAW16;
			break;
	  case 24:	codec->decoder = QT_Decode_RAW24;
			break;
	  case 32:	codec->decoder = QT_Decode_RAW32;
			break;
	  default:	ret = CODEC_UNSUPPORTED;	break;
	}
	break;

    case QT_rpza:
        codec->compression      = QT_rpza;
        codec->description      = "Apple Video(RPZA)";
        ret = CODEC_SUPPORTED;
	if (codec->depth == 16)
	{ codec->decoder = QT_Decode_RPZA;
	  codec->x = 4 * ((codec->x + 3)/4);
	  codec->y = 4 * ((codec->y + 3)/4);
	}
	else ret = CODEC_UNSUPPORTED;
	break;

    case QT_jpeg:
        codec->compression      = QT_jpeg;
        codec->description      = "JPEG";
        ret = CODEC_SUPPORTED;
	if ((codec->depth == 8) || (codec->depth == 24)
	    || (codec->depth == 40) )
	{ JPG_Alloc_MCU_Bufs(codec->anim_hdr,codec->x,0,xaFALSE);
	  codec->x = 4 * ((codec->x + 3)/4);
	  codec->y = 2 * ((codec->y + 1)/2);
	  codec->decoder = JFIF_Decode_JPEG;
	  if (codec->depth == 24) XA_Gen_YUV_Tabs(codec->anim_hdr);
	}
	else ret = CODEC_UNSUPPORTED;
	break;

    case QT_yuv2:
        codec->compression      = QT_yuv2;
        codec->description      = "Component Video(YUV2)";
        ret = CODEC_SUPPORTED;
	codec->decoder = QT_Decode_YUV2;
	codec->x = 2 * ((codec->x + 1)/2);
	codec->y = 2 * ((codec->y + 1)/2);
	break;
	
    default:
        ret = CODEC_UNKNOWN;
        break;
  }
  return(ret);
}

/****************************
 *
 ****************/
xaLONG QT_UNK_Codec_Query(codec)
XA_CODEC_HDR *codec;
{ xaLONG ret = CODEC_UNKNOWN;   /* default */
  codec->extra = 0;
  codec->xapi_rev = 0x0001;
  codec->decoder = 0;
  codec->description = 0;
  codec->avi_read_ext = 0;

  switch(codec->compression)
  {
    case QT_PGVV:
        codec->compression      = QT_PGVV;
        codec->description      = "Radius (PGVV)";
        ret = CODEC_UNSUPPORTED;
	break;

    case QT_cvid:
    case QT_CVID:
        codec->compression      = QT_CVID;
        codec->description      = "Radius Cinepak";
        ret = CODEC_UNSUPPORTED;
	break;

    case QT_SPIG:
        codec->compression      = QT_SPIG;
        codec->description      = "Radius Spigot";
        ret = CODEC_UNSUPPORTED;
	break;

    case QT_rt21:
    case QT_RT21:
        codec->compression      = QT_RT21;
        codec->description      = "Intel Indeo R2.1";
        ret = CODEC_UNSUPPORTED;
	break;

    case QT_iv31:
    case QT_IV31:
        codec->compression      = QT_IV31;
        codec->description      = "Intel Indeo R3.1";
        ret = CODEC_UNSUPPORTED;
	break;

    case QT_iv32:
    case QT_IV32:
        codec->compression      = QT_IV32;
        codec->description      = "Intel Indeo R3.2";
        ret = CODEC_UNSUPPORTED;
	break;

    case QT_YVU9:
    case QT_YUV9:
        codec->compression      = QT_YUV9;
        codec->description      = "Intel Raw(YUV9)";
        ret = CODEC_UNSUPPORTED;
	break;

    default:
        ret = CODEC_UNKNOWN;
        break;
  }
  return(ret);
}

