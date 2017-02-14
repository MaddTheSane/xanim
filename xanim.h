
/*
 * xanim.h
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
#include <Xos.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdio.h>

#ifndef VMS

#ifndef __QNX__
#include <sys/param.h>
#include <memory.h>
#endif

#ifdef __bsdi__
#include <sys/malloc.h>
#else
#ifndef __CYGWIN32__
#ifndef __FreeBSD__
#include <malloc.h>
#endif
#endif
#endif


#include <unistd.h>
#else
#include <unixio.h>
#endif
#include <stdlib.h>
#include <Xlib.h>
#include "xa_config.h"

#ifdef XA_XTPOINTER
typedef void* XtPointer;
#endif

#ifdef XA_PETUNIA
#define XA_REMOTE_CONTROL 1
#endif

#ifdef XA_ATHENA
#define XA_REMOTE_CONTROL 1
#endif

#ifdef XA_MOTIF
#define XA_REMOTE_CONTROL 1
#endif

/*
 * Win32 utilizatizes same defines as MSDOS does
 */
#ifdef _WIN32
#define MSDOS 1
#endif

/*
 * MSDOS and Win32 needs to be specifically told to open file for binary 
 * reading.  For VMS systems, specify "Stream_LF" mode for VAX C.
 */
#ifdef MSDOS
#define XA_OPEN_MODE "rb"
#else
#ifdef VMS
#define XA_OPEN_MODE "r","ctx=stm"
#else
#define XA_OPEN_MODE "r"
#endif
#endif

typedef int		xaLONG;
typedef unsigned int	xaULONG;
typedef short		xaSHORT;
typedef unsigned short	xaUSHORT;
typedef char		xaBYTE;
typedef unsigned char	xaUBYTE;

#define xaFALSE  0
#define xaTRUE   1
#define xaNOFILE 2
#define xaERROR  3
#define xaPAUSE  4


#define xaMIN(x,y)   ( ((x)>(y))?(y):(x) )
#define xaMAX(x,y)   ( ((x)>(y))?(x):(y) )
#define xaABS(x)     (((x)<0)?(-(x)):(x))

/* X11 variables */

#define XA_GRAYSCALE	0x06
#define XA_STATICGRAY	0x03
#define XA_PSEUDOCOLOR	0x14
#define XA_STATICCOLOR	0x11
#define XA_DIRECTCOLOR	0x18
#define XA_TRUECOLOR	0x19
#define XA_MONOCHROME	0x00

#define XA_X11_STATIC	0x01
#define XA_X11_GRAY	0x02
#define XA_X11_CMAP	0x04
#define XA_X11_TRUE	0x08
#define XA_X11_COLOR	0x10

extern xaLONG x11_depth;
extern xaLONG x11_class;
extern xaLONG x11_bytes_pixel;
extern xaLONG x11_bits_per_pixel;
extern xaLONG x11_bitmap_pad;
extern xaLONG x11_bitmap_unit;
extern xaLONG x11_bit_order;
extern xaLONG x11_byte_order;
extern xaLONG xam_byte_order;
extern xaLONG x11_byte_mismatch;
extern xaLONG x11_pack_flag;
extern xaLONG x11_cmap_flag;
extern xaLONG x11_cmap_size;
extern xaLONG x11_disp_bits;
extern xaLONG x11_cmap_type;
extern xaLONG x11_depth_mask;
extern xaLONG x11_display_type;
extern xaLONG x11_red_mask;
extern xaLONG x11_green_mask;
extern xaLONG x11_blue_mask;
extern xaLONG x11_red_shift;
extern xaLONG x11_green_shift;
extern xaLONG x11_blue_shift;
extern xaLONG x11_red_bits;
extern xaLONG x11_green_bits;
extern xaLONG x11_blue_bits;
extern xaLONG x11_black;
extern xaLONG x11_white;
extern xaLONG x11_verbose_flag;
extern xaULONG x11_kludge_1;
extern xaLONG xa_root;

#define XA_MSBIT_1ST  1
#define XA_LSBIT_1ST  0

#define XA_MSBYTE_1ST  1
#define XA_LSBYTE_1ST  0

extern xaLONG xa_anim_holdoff;
extern xaLONG xa_anim_status;

/*------*/
#define XA_NEXT_MASK	0x01
#define XA_STOP_MASK	0x02
#define XA_STEP_MASK	0x04
#define XA_RUN_MASK	0x08
#define XA_ISTP_MASK	0x10
#define XA_FILE_MASK	0x20
#define XA_CLEAR_MASK	0x01
#define XA_BEGIN_MASK	0x01
/*------*/
#define XA_UNSTARTED   0x00
#define XA_BEGINNING   0x80
#define XA_STOP_PREV   0x02
#define XA_STOP_NEXT   0x03
#define XA_STEP_PREV   0x04
#define XA_STEP_NEXT   0x05
#define XA_RUN_PREV    0x08
#define XA_RUN_NEXT    0x09
#define XA_ISTP_PREV   0x14
#define XA_ISTP_NEXT   0x15
#define XA_FILE_PREV   0x24
#define XA_FILE_NEXT   0x25

#define XA_SHOW_NORM   0
#define XA_SHOW_SKIP   1

#define XA_SPEED_NORM (1<<4)
#define XA_SPEED_MIN  (1)
#define XA_SPEED_MAX  (1<<8)
#define XA_SPEED_ADJ(x,s)  (((x)*(s))>>4)

#define NOFILE_ANIM   0xffff
#define UNKNOWN_ANIM  0
/*************************** VIDEO FILES *************/
#define XA_IFF_ANIM      1
#define XA_FLI_ANIM      2
#define XA_GIF_ANIM      3
#define XA_TXT_ANIM      4
#define XA_FADE_ANIM     5
#define XA_DL_ANIM       6
#define XA_JFIF_ANIM     7
#define XA_PFX_ANIM      8
#define XA_SET_ANIM      9
#define XA_RLE_ANIM     10
#define XA_AVI_ANIM     11
#define XA_OLDQT_ANIM	12
#define XA_MPG_ANIM     13
#define XA_JMOV_ANIM    14
#define XA_ARM_ANIM     15
#define XA_SGI_ANIM     16
#define XA_RAW_ANIM     17
#define XA_J6I_ANIM     18
#define XA_QT_ANIM	19
/*************************** AUDIO FILES *************/
#define XA_WAV_ANIM     128
#define XA_AU_ANIM      129
#define XA_8SVX_ANIM    130

typedef struct
{
  xaUSHORT red,green,blue,gray;
} ColorReg;

typedef struct XA_ACTION_STRUCT
{
 xaLONG type;		/* type of action */
 xaLONG cmap_rev;          /* rev of cmap */
 xaUBYTE *data;		/* data ptr */
 struct XA_ACTION_STRUCT *next;
 struct XA_CHDR_STRUCT *chdr;
 ColorReg *h_cmap;	/* For IFF HAM images */
 xaULONG *map;
 struct XA_ACTION_STRUCT *next_same_chdr; /*ptr to next action with same cmap*/
} XA_ACTION;

typedef struct XA_CHDR_STRUCT
{
 xaLONG rev;
 ColorReg *cmap;
 xaULONG csize,coff;
 xaULONG *map;
 xaULONG msize,moff;
 struct XA_CHDR_STRUCT *next;
 XA_ACTION *acts;
 struct XA_CHDR_STRUCT *new_chdr;
} XA_CHDR;

typedef struct
{
 xaULONG csize,coff;
 xaUBYTE data[4];
} ACT_CMAP_HDR;

typedef struct XA_FRAME_STRUCT
{
  XA_ACTION *act;	/* ptr to relevant Action. */
  xaLONG time_dur;	/* duration of Frame in ms. */
  xaLONG zztime;		/* time at start of Frame in ms. */
} XA_FRAME;

typedef struct
{
  xaULONG count;    /* number of loops */
  xaLONG cnt_var;   /* var to keep track of loops */
  xaULONG end_frame; /* last frame of loop */
} ACT_BEG_LP_HDR;

typedef struct ACT_END_LP_STRUCT
{
  xaULONG *count;       /* points back to beg_lp->count */
  xaLONG *cnt_var;      /* points back to beg_lp->cnt_var */
  xaULONG begin_frame;  /* first frame of loop */
  xaULONG *end_frame;   /* points back to beg_lp->end_frame */
  XA_ACTION *prev_end_act; /* used to nest loops during creation */
} ACT_END_LP_HDR;

/** AUDIO SECTION ************************/

typedef struct
{
  xaULONG enable;
  xaULONG mute;
  xaLONG  volume;
  xaULONG newvol;
  xaULONG port;
  xaULONG playrate;
  xaULONG divtest;
  xaULONG synctst;
  xaULONG fromfile;
  xaULONG bufferit;
  double scale;
  char *device;
} XA_AUD_FLAGS;


/* POD temporary til complete overhaul */
#define XAAUD audiof

#ifdef XA_SPARC_AUDIO
#define XA_AUDIO 1
#endif
#ifdef XA_MMS_AUDIO
#define XA_AUDIO 1
#endif
#ifdef XA_AIX_AUDIO
#define XA_AUDIO 1
#endif
#ifdef XA_NetBSD_AUDIO
#define XA_AUDIO 1
#endif
#ifdef XA_LINUX_AUDIO
#define XA_AUDIO 1
#endif
#ifdef XA_SGI_AUDIO
#define XA_AUDIO 1
#endif
#ifdef XA_HPDEV_AUDIO
#define XA_AUDIO 1
#endif
#ifdef XA_HP_AUDIO
#define XA_AUDIO 1
#endif
#ifdef XA_EWS_AUDIO
#define XA_AUDIO 1
#endif
#ifdef XA_SONY_AUDIO
#define XA_AUDIO 1
#endif
#ifdef XA_AF_AUDIO
#define XA_AUDIO 1
#endif
#ifdef XA_NAS_AUDIO
#define XA_AUDIO 1
#endif
#ifdef XA_TOWNS_AUDIO
#define XA_AUDIO 1
#endif
#ifdef XA_TOWNS8_AUDIO
#define XA_AUDIO 1
#endif


/* AUDIO PORT MASKS */
#define XA_AUDIO_PORT_INT	1
#define XA_AUDIO_PORT_HEAD	2
#define XA_AUDIO_PORT_EXT	4
#define XA_AUDIO_PORT_NONE	8

#define XA_AUDIO_STEREO_MSK	0x000001
#define XA_AUDIO_BPS_2_MSK	0x000002
#define XA_AUDIO_BIGEND_MSK	0x000004
#define XA_AUDIO_INVALID	0x000000
#define XA_AUDIO_LINEAR		0x000010
#define XA_AUDIO_SIGNED		0x000020
#define XA_AUDIO_ULAW  		0x000030
#define XA_AUDIO_ADPCM		0x000040
#define XA_AUDIO_NOP  		0x000050
#define XA_AUDIO_ARMLAW		0x000060
#define XA_AUDIO_IMA4 		0x000070
#define XA_AUDIO_DVI  		0x000080
#define XA_AUDIO_GSM  		0x000090
#define XA_AUDIO_MSGSM 		0x0000A0
#define XA_AUDIO_ALAW  		0x0000B0
#define XA_AUDIO_TYPE_MASK  	0xfffff0

/*NOTES: 
 * last ending is [1|2](BPS),[M|S](Mono/Stereo),[B|L](big/little endian)
 */

#define XA_AUDIO_LINEAR_1M	0x000010
#define XA_AUDIO_LINEAR_1S	0x000011
#define XA_AUDIO_LINEAR_2ML	0x000012
#define XA_AUDIO_LINEAR_2SL	0x000013
#define XA_AUDIO_LINEAR_2MB	0x000016
#define XA_AUDIO_LINEAR_2SB	0x000017

#define XA_AUDIO_SIGNED_1M	0x000020
#define XA_AUDIO_SIGNED_1S	0x000021
#define XA_AUDIO_SIGNED_2ML	0x000022
#define XA_AUDIO_SIGNED_2SL	0x000023
#define XA_AUDIO_SIGNED_2MB	0x000026
#define XA_AUDIO_SIGNED_2SB	0x000027

#define XA_AUDIO_SUN_AU		0x000030
#define XA_AUDIO_ULAWM 		0x000030
#define XA_AUDIO_ULAWS 		0x000031

#define XA_AUDIO_ARMLAWM	0x000060
#define XA_AUDIO_ARMLAWS	0x000061

#define XA_AUDIO_ADPCM_M	0x000040
#define XA_AUDIO_ADPCM_S	0x000041

#define XA_AUDIO_IMA4_M 	0x000070
#define XA_AUDIO_IMA4_S 	0x000071

#define XA_AUDIO_DVI_M		0x000080
#define XA_AUDIO_DVI_S		0x000081

#define XA_AUDIO_GSM_M		0x000090
#define XA_AUDIO_MSGSM_M	0x0000A0

#define XA_AUDIO_ALAWM 		0x0000B0
#define XA_AUDIO_ALAWS 		0x0000B1

#define XA_AUDIO_FILE_FLAG   0x0001

typedef struct XA_SND_STRUCT
{
  xaULONG type;		/* type, chans, bps, */
  xaULONG flag;		/* flags */
  xaLONG  fpos;		/* starting file position */
  xaULONG itype;		/* input sample type */
  xaULONG ifreq;		/* input sample freq */
  xaULONG hfreq;		/* closest hardware freq */
  xaULONG inc;		/* inc for i to h converstion << 24 */
  xaULONG tot_bytes;	/* total size in bytes*/
  xaULONG tot_samps;	/* total samples in chunk */
  xaULONG inc_cnt;	/* dynamic var for freq conv */
  xaULONG bit_cnt;	/* dynamic var for partial bytes - not used yet */
  xaULONG byte_cnt;	/* dynamic var for counting bytes */
  xaULONG samp_cnt;	/* samples used so far */
  xaULONG blk_size;	/* size of blocks - used by some codecs */
  xaULONG blk_cnt;	/* dynamic var for cnting inside a block */
  xaULONG (*delta)();	/* conversion routine */
  xaULONG snd_time;	/* time at start of snd struct ms */ 
  xaULONG ch_time;	/* chunk time ms */
  xaULONG ch_timelo;	/* chunk time fractional ms */
  xaULONG ch_size;	/* size of chunk */
  xaULONG spec;		/* used by decoder */
  xaLONG dataL;		/* data Left channel */
  xaLONG dataR;		/* data Right channel */
  xaUBYTE *snd;		/* sound if present */
  struct XA_SND_STRUCT *prev;
  struct XA_SND_STRUCT *next;
} XA_SND;

#define XA_SND_CHUNK_SIZE 65536

typedef struct
{
  char		*file;		/* is this a duplication of xanim_hdr->file?*/
  FILE		*fin;           /* buffer file */
  int		csock;          /* control socket */
  int		dsock;          /* data socket */
  xaULONG	fpos;		/* bytes read so far */
  xaLONG	eof;		/* size of file - set by client */
  xaULONG	err_flag;	/* set on errors and/or EOF */
  xaULONG	type_flag;	/* random/sequential access? */
  xaULONG	load_flag;	/* from file, from memory, buffered? */
  xaULONG	(*Open_File)();
  xaULONG	(*Close_File)();
  xaULONG	(*Read_U8)();
  xaULONG	(*Read_LSB_U16)();
  xaULONG	(*Read_MSB_U16)();
  xaULONG	(*Read_LSB_U32)();
  xaULONG	(*Read_MSB_U32)();
  xaLONG	(*Read_Block)();
  xaLONG	(*At_EOF)();
  void		(*Set_EOF)();
  xaLONG	(*Seek_FPos)();
  xaLONG	(*Get_FPos)();
	/* This Buffer is used initially to Determine File type and later *
	 * to contruct u8, u16 and u32 entities for certain input methods */
  xaUBYTE	*buf;
  xaULONG	buf_size;
  xaUBYTE	*buf_ptr;
  xaULONG	buf_cnt;
  void		*net;		/* generic structure for addition info */
} XA_INPUT;

	/* load into memory and play from there */
#define XA_IN_LOAD_MEM		0x00
	/* don't load into mem, play from file */
#define XA_IN_LOAD_FILE		0x01
	/* decompress into X11 images and play those */
#define XA_IN_LOAD_BUF		0x02

	/* Input Method supports Random access (otherwise Sequential only) */
#define XA_IN_TYPE_RANDOM	0x01
#define XA_IN_TYPE_SEQUENTIAL	0x00

#define XA_IN_ERR_NONE		0x00
#define XA_IN_ERR_EOF		0x01
#define XA_IN_ERR_ERR		0x02

typedef struct XA_PAUSE_STRUCT
{
  xaULONG frame;
  struct XA_PAUSE_STRUCT *next;
} XA_PAUSE;

typedef struct XA_FUNC_STRUCT
{
  xaULONG var1,var2;
  void (*function)();
  struct XA_FUNC_STRUCT *next;
} XA_FUNC_CHAIN;

typedef struct XA_ANIM_HDR_STRUCT
{
  xaLONG	file_num;
  xaLONG	anim_type;	/* animation type */
  xaLONG	imagex;		/* width */
  xaLONG	imagey;		/* height */
  xaLONG	imagec;		/* number of colors */
  xaLONG	imaged;		/* depth in planes */
  xaLONG	dispx;		/* display width */
  xaLONG	dispy;		/* display height */
  xaLONG	buffx;		/* buffered width */
  xaLONG	buffy;		/* buffered height */
  xaLONG	anim_flags;
  xaLONG	loop_num;	/* number of times to loop animation */
  xaLONG	loop_frame;	/* index of loop frame */
  xaLONG	last_frame;	/* index of last frame */
  xaLONG	total_time;	/* Length of Anim in ms */
  char		*name;		/* name of anim */
  char		*fname;		/* name of anim data file(video and audio) */
  char		*desc;		/* descriptive string */
/* char		 *fsndname;	   eventually have separate sound file name */
  XA_INPUT	*xin;		/* XAnim Input structure */
  xaULONG	(*Read_File)();	/* Routine to Parse Animation */
  xaLONG	max_fvid_size;	/* Largest video codec size */
  xaLONG	max_faud_size;	/* Largest audio codec size */
  XA_FRAME	*frame_lst;	/* array of Frames making up the animation */
  XA_ACTION	*acts;		/* actions associated with this animation */
  XA_SND	*first_snd;	/* ptr to first sound chunk */
  XA_SND	*last_snd;	/* ptr to last sound chunk */
  XA_PAUSE	*pause_lst;	/* pause list */
  void		(*init_vid)();	/* routine to init video */
  void		(*init_aud)();	/* routine to init audio */
 
  XA_FUNC_CHAIN *free_chain;	/* list of routines to call on exit */
  struct XA_ANIM_HDR_STRUCT *next_file;
  struct XA_ANIM_HDR_STRUCT *prev_file;
} XA_ANIM_HDR;

#define ANIM_HAM	0x00000009
#define ANIM_HAM6	0x00000001
#define ANIM_LACE	0x00000002
#define ANIM_CYCLE	0x00000004
#define ANIM_HAM8	0x00000008
#define ANIM_PIXMAP	0x00000100
#define ANIM_FULL_IM	0x00001000
#define ANIM_PING	0x00010000
#define ANIM_NOLOOP	0x00020000
/* single buffered, x11_bytes_pixel */
#define ANIM_SNG_BUF	0x01000000
/* double buffered, 1 byte per pixel */
#define ANIM_DBL_BUF	0x02000000
#define ANIM_3RD_BUF	0x04000000
/* open anim_hdr->fname when starting anim */
#define ANIM_USE_FILE	0x08000000

typedef struct
{
 xaLONG imagex;
 xaLONG imagey;
 xaLONG xoff;
 xaLONG yoff;
} SIZE_HDR;

typedef struct
{ 
 xaULONG flag;
 xaLONG xpos;
 xaLONG ypos;
 xaLONG xsize;
 xaLONG ysize;
 XImage *image;
 xaUBYTE *clip;
} ACT_IMAGE_HDR;

typedef struct
{ 
 xaULONG flag;
 xaLONG xpos;
 xaLONG ypos;
 xaLONG xsize;
 xaLONG ysize;
 Pixmap pixmap;
 Pixmap clip;
} ACT_PIXMAP_HDR;

typedef struct
{ 
 xaLONG xpos;
 xaLONG ypos;
 xaLONG xsize;
 xaLONG ysize;
 XA_ACTION *act;
} ACT_APTR_HDR;

#define ACT_BUFF_VALID 0x01

typedef struct
{ 
  xaULONG xapi_rev;
  xaULONG (*delta)();
  xaULONG flags;
  xaULONG xpos,ypos;
  xaULONG xsize,ysize;
  xaULONG special;
  void *extra;
  xaULONG fpos;
  xaULONG fsize;
  xaUBYTE data[4];
} ACT_DLTA_HDR;

/* ACT_DLTA_HDR Flag Values */
/* ALSO ACT_BUFF_VALID  0x0001   defined above */
#define ACT_SNGL_BUF    0x0100   /* delta is from sngl buffer anim */
#define ACT_DBL_BUF     0x0200   /* delta is from sngl buffer anim */
#define ACT_3RD_BUF     0x0400   /* needs 3rd buffer for HAM or Dither */
#define DLTA_DATA	0x1000   /* delta data is present */

/* DELTA Return VALUES */
#define ACT_DLTA_NORM   0x00000000   /* nothing special */
#define ACT_DLTA_BODY   0x00000001   /* IFF BODY - used for dbl buffer */
#define ACT_DLTA_XOR    0x00000002   /* delta work in both directions */
#define ACT_DLTA_NOP    0x00000004   /* delta didn't change anything */
#define ACT_DLTA_MAPD   0x00000008   /* delta was able to map image */
#define ACT_DLTA_DROP   0x00000010   /* drop this one */
#define ACT_DLTA_BAD    0x80000000   /* uninitialize value if needed */


typedef struct
{
  xaULONG cfunc_type;
  void (*color_RGB)();
  void (*color_Gray)();
  void (*color_CLR8)();
  void (*color_CLR16)();
  void (*color_CLR32)();
  void (*color_332)();
  void (*color_CF4)();
  void (*color_332_Dither)();
  void (*color_CF4_Dither)();
} XA_COLOR_FUNC;


typedef struct
{
  xaULONG imagex,max_imagex;
  xaULONG imagey,max_imagey;
  xaULONG imagec;
  xaULONG depth;
  xaULONG compression;
  xaUBYTE *pic;
  xaULONG pic_size;
  xaULONG max_fvid_size,max_faud_size;
  xaULONG vid_time,vid_timelo;
  xaULONG aud_time,aud_timelo;
  xaULONG cmap_flag;
  xaULONG cmap_cnt;
  xaULONG color_cnt;
  xaULONG color_retries;
  xaULONG cmap_frame_num;
  ColorReg cmap[256];
  XA_CHDR  *chdr;
} XA_ANIM_SETUP;


typedef struct STRUCT_ACT_SETTER_HDR
{ 
 XA_ACTION *work;
 xaLONG xback,yback;
 xaLONG xpback,ypback; 
 XA_ACTION *back;
 xaLONG xface,yface;
 xaLONG xpface,ypface;
 xaLONG depth;
 XA_ACTION *face;
 struct STRUCT_ACT_SETTER_HDR *next;
} ACT_SETTER_HDR;

typedef struct
{ 
  xaLONG xpos;
  xaLONG ypos;
  xaLONG xsize;
  xaLONG ysize;
  xaLONG psize;
  xaUBYTE *clip;
  xaUBYTE *data; 
} ACT_MAPPED_HDR;

typedef struct
{ 
  xaLONG xpos;
  xaLONG ypos;
  xaLONG xsize;
  xaLONG ysize;
  xaLONG psize;
  xaLONG rbits;
  xaLONG gbits;
  xaLONG bbits;
  xaUBYTE *clip;
  xaUBYTE *data; 
} ACT_TRUE_HDR;

typedef struct
{ 
  xaLONG xpos;
  xaLONG ypos;
  xaLONG xsize;
  xaLONG ysize;
  xaLONG pk_size;
  xaUBYTE *clip;
  xaUBYTE data[4]; 
} ACT_PACKED_HDR;

typedef struct
{ 
 xaLONG xpos;
 xaLONG ypos;
 xaLONG xsize;
 xaLONG ysize;
 XImage *image;
 xaUBYTE *clip_ptr;
} ACT_CLIP_HDR;

#define ACT_NOP		0x0000
#define ACT_DELAY	0x0001
#define ACT_IMAGE	0x0002
#define ACT_CMAP	0x0003
#define ACT_SIZE	0x0004
#define ACT_FADE	0x0005
#define ACT_CLIP	0x0006
#define ACT_PIXMAP	0x0007
#define ACT_SETTER	0x0008
#define ACT_RAW		0x0009
#define ACT_PACKED	0x000A
#define ACT_DISP	0x000B
#define ACT_MAPPED	0x000C
#define ACT_TRUE	0x000D
#define ACT_PIXMAPS	0x000E
#define ACT_IMAGES	0x000F
#define ACT_CYCLE	0x0010
#define ACT_DELTA	0x0011
#define ACT_APTR 	0x0012
#define ACT_BEG_LP	0x0100
#define ACT_END_LP	0x0101
#define ACT_JMP2END	0x0102

/* flags */
extern xaLONG xa_verbose;
extern xaLONG xa_debug;
extern xaLONG xa_jiffy_flag;
extern xaLONG xa_buffer_flag;
extern xaLONG xa_file_flag;
extern xaLONG xa_optimize_flag;
extern xaLONG xa_use_depth_flag;

#define DEBUG_LEVEL1   if (xa_debug >= 1) 
#define DEBUG_LEVEL2   if (xa_debug >= 2) 
#define DEBUG_LEVEL3   if (xa_debug >= 3) 
#define DEBUG_LEVEL4   if (xa_debug >= 4) 
#define DEBUG_LEVEL5   if (xa_debug >= 5) 

#define XA_CMAP_SIZE 256
#define XA_HMAP_SIZE  64
#define XA_HMAP6_SIZE 16
#define XA_HMAP8_SIZE 64

/* CMAP function flags for ACT_Setup_CMAP */
#define CMAP_DIRECT		0x000000
#define CMAP_ALLOW_REMAP	0x000001


#define CMAP_SCALE4 4369
#define CMAP_SCALE5 2114
#define CMAP_SCALE6 1040
#define CMAP_SCALE8  257
#define CMAP_SCALE9  128
#define CMAP_SCALE10  64
#define CMAP_SCALE11  32
#define CMAP_SCALE13   8
extern xaULONG cmap_scale[17];
extern xaLONG cmap_true_to_332;
extern xaLONG cmap_true_to_gray;
extern xaLONG cmap_true_to_1st;
extern xaLONG cmap_true_to_all;
extern xaLONG cmap_true_map_flag;
extern xaLONG cmap_dither_type;

extern xaULONG cmap_sample_cnt;  /* how many times to sample colors for +CF4 */
extern xaULONG cmap_color_func;
extern xaLONG cmap_luma_sort;
extern xaLONG cmap_map_to_1st_flag;
extern xaLONG cmap_play_nice;
extern XA_CHDR *xa_chdr_start;
extern XA_CHDR *xa_chdr_cur;
extern XA_CHDR *xa_chdr_now;
extern ColorReg *xa_cmap;
extern xaULONG xa_cmap_size;
extern xaULONG xa_cmap_off;
extern xaLONG cmap_median_type;
extern xaSHORT cmap_floyd_error;
extern xaLONG cmap_map_to_one_flag;
extern xaLONG pod_max_colors;
extern xaLONG cmap_hist_flag;
extern xaLONG cmap_median_bits;
extern xaULONG cmap_cache_size;
extern xaULONG cmap_cache_bits;
extern xaULONG cmap_cache_rmask;
extern xaULONG cmap_cache_gmask;
extern xaULONG cmap_cache_bmask;
extern xaUSHORT *cmap_cache,*cmap_cache2;
extern XA_CHDR *cmap_cache_chdr;

extern xaULONG xa_gamma_flag;
extern xaUSHORT xa_gamma_adj[];


extern xaULONG xa_r_shift,xa_g_shift,xa_b_shift;
extern xaULONG xa_r_mask,xa_g_mask,xa_b_mask;
extern xaULONG xa_gray_bits,xa_gray_shift;

#define XA_HAM_MAP_INVALID 0xffffffff
#define XA_HAM6_CACHE_SIZE   4096
#define XA_HAM8_CACHE_SIZE 262144

typedef struct
{
 xaULONG rate;	/* rate at which to cycle colors in milliseconds */
 xaULONG flags;   /* flags */
 xaULONG size;    /* size of color array */
 xaULONG curpos;  /* curpos in array */
 xaUBYTE data[4];  /* array of cmap pixel values to cycle */
} ACT_CYCLE_HDR;

/* ACT_CYCLE flags values */
/* NOTE: ACTIVE isn't currently checked. It's assumed to be active or
 *       else it shouldn't have been created by anim reader. */
#define ACT_CYCLE_ACTIVE  0x01
#define ACT_CYCLE_REVERSE 0x02
#define ACT_CYCLE_STARTED 0x80000000

extern void TheEnd();
extern void TheEnd1();
extern void XAnim_Looped();
extern void Cycle_It();
extern xaULONG X11_Get_True_Color();
extern xaULONG X11_Get_Line_Size();


/* AUDIO STUFF */

#define XA_AUDIO_STOPPED 0
#define XA_AUDIO_PREPPED 1
#define XA_AUDIO_STARTED 2
#define XA_AUDIO_NICHTDA 3

#define XA_AUDIO_OK   0
#define XA_AUDIO_UNK  1
#define XA_AUDIO_NONE 2
#define XA_AUDIO_ERR  3

#define XA_AUDIO_MAXVOL  100
#define XA_AUDIO_MINVOL  0

extern xaULONG xa_audio_enable;


/* 
 * Useful Macros 
 */

#define CMAP_GET_GRAY(r,g,b,scale) \
( ((scale)*((r)*11+(g)*16+(b)*5) ) >> xa_gray_shift)

#define CMAP_GET_332(r,g,b,scale) ( \
( (((r)*(scale)) & xa_r_mask) >> xa_r_shift) | \
( (((g)*(scale)) & xa_g_mask) >> xa_g_shift) | \
( (((b)*(scale)) & xa_b_mask) >> xa_b_shift) )

#define X11_Get_Bitmap_Width(x) \
  ( ((x + x11_bitmap_unit - 1)/x11_bitmap_unit) * x11_bitmap_unit )

#define X11_Make_Pixel(p)  (x11_cmap_type == 0)?(p): \
		( (((p)<<24)|((p)<<16)|((p)<<8)|(p)) & x11_depth_mask )

#define XA_PIC_SIZE(p) ( (xa_use_depth_flag==xaTRUE)?((p) * x11_bytes_pixel): \
		(p) )

#define XA_GET_TIME(t) ( (xa_jiffy_flag)?(xa_jiffy_flag):(t) )

#define XA_MEMSET(p,d,size) \
{ if (x11_bytes_pixel==4) { xaULONG _sz=(size); \
    xaULONG *_lp=(xaULONG *)p; while(_sz--) *_lp++ = (xaULONG)(d); } \
  else if (x11_bytes_pixel==2) { xaULONG _sz=(size); \
    xaUSHORT *_sp=(xaUSHORT *)p; while(_sz--) *_sp++ = (xaUSHORT)(d); } \
  else { memset(p,d,size); } \
}

#define XA_REALLOC(p,cur_size,new_size) { if (new_size > cur_size) \
{ char *_tmp; \
  if (p == 0) _tmp=(char *)malloc(new_size); \
  else _tmp=(char *)realloc(p,new_size); \
  if (_tmp == 0) TheEnd1("XA_Realloc: malloc err"); \
  p = _tmp; cur_size = new_size; } \
} 

#define FREE(_p,_q) free(_p)
/* For Debug
#include <errno.h>
#define FREE(_p,_q) { int ret; ret = free(_p); \
fprintf(stderr,"FREE %lx %lx ret=%ld err=%ld\n",_p,_q,ret,errno); }
*/


/* REV 1 */
typedef struct
{
  xaULONG cmd;			/* decode or query */
  xaULONG skip_flag;		/* skip_flag */
  xaULONG imagex,imagey;	/* Image Buffer Size */
  xaULONG imaged; 		/* Image depth */
  XA_CHDR *chdr;		/* Color Map Header */
  xaULONG map_flag;		/* remap image? */
  xaULONG *map;			/* map to use */
  xaULONG xs,ys;		/* pos of changed area */
  xaULONG xe,ye;		/* size of change area */
  xaULONG special;		/* Special Info */
  void *extra;			/* Decompression specific info */
} XA_DEC_INFO;

#include "xa_dec2.h"



