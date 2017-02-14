
/*
 * xanim.c
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
/****************
 * Rev History
 *
 * 04Aug94 - fixed bug where xa_cmap was re-assigned to a cmap during
 *           a cmap install. This was bad because when xa_cmap was being
 *	     free'd, it was freeing something else that got freed later.
 *	     This would cause core dumps on some systems.
 * 05Aug94 - Modified audio/video sync code to account for XtAppAddTimeOut
 *	     calls.
 * 09Aug94 - Removed XSync's when audio is enabled. This was causing stalls
 *           in both audio and video whenever ANY window was moved or 
 *	     resized.
 *	   - Added XA_Time_Reset function to bring audio and video streams
 *	     up to date after a pause.
 *	   - Added XA_Audio_Pause routine, for those stops/starts since
 *	     Sparc Audio supports pausing.
 *	   - Audio should now properly resync itself after single stepping
 *	     in either direction(UNLESS YOU ACTUALLY RAN BACKWARDS).
 * 12Aug94 - Setup X11 error handler to detec XShmAttach failures when Disp
 *	     is remote so I can backoff Shm support and continue on. 
 * 12Sep94 - Now freeing up Actions. Before was accidently only freeing up
 *	     memory associated with Actions and not the actually Actions
 *	     themselves.
 * 15Sep94 - Redid Audio and Video Timing to simply the synchronization
 *	     process and allow more features.
 * 19Sep94 - Now free'ing up cmap_cache in TheEnd() if it was allocated.
 *
 * 16Mar95 - redid audio/video syncing routine to handle higher cpu
 *           loads and more cpu intensive frames.
 * 10Apr95 - Fixed bug where movies without sound would turn off sound
 *	     is movies listed later on the command line.
 *
 *******************************/

#define DA_REV 2.69
#define DA_MINOR_REV 7
#define DA_BETA_REV 8

/*
 * Any problems, suggestions, solutions, anims to show off, etc contact:
 *
 * podlipec@wellfleet.com  or podlipec@shell.portal.com
 *
 */

#include "xanim.h"
#include <Intrinsic.h>
#include <StringDefs.h>
#include <Shell.h>
#include <sys/signal.h>
#ifdef MSDOS
#include <sys/resource.h>
#else /* not MSDOS */
#ifndef VMS   /* NOT MSDOS and NOT VMS ie Unix */
#include <sys/times.h>
#else   /* VMS systems */
#include <lib$routines.h>
#include <starlet.h>
#ifdef R3_INTRINSICS
typedef void *XtPointer;
#endif
#endif
#endif
#include <ctype.h>

#ifdef XSHM
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
extern Visual        *theVisual;
#endif /*XSHM*/

#ifdef XA_REMOTE_DEFS
extern void XA_Remote_Pause();
#endif

#include "xanim_x11.h"

ULONG xa_audio_init;
ULONG xa_audio_present;
ULONG xa_audio_enable;
ULONG xa_audio_status;
ULONG xa_audio_mute;		/* mute audio T/F */
LONG  xa_audio_volume;		/* volume 0 - 100 */
ULONG xa_audio_newvol;		/* volume/mute has changed T/F */
ULONG xa_audio_port;		/* Audio output port */
ULONG xa_audio_playrate;	/* forcibly select audio rate */
ULONG xa_audio_divtest;		/* select an interval time */
ULONG xa_audio_synctst;		/* testing only */
double xa_audio_scale;		/* scale audio speed */
ULONG xa_audio_buffer;
XA_SND *xa_snd_cur;
/*Sound variables */

/*KLUDGES*/
extern jpg_free_stuff();
extern mpg_free_stuff();

extern void XA_Audio_Setup();
extern ULONG (*XA_Audio_Init)();
extern void (*XA_Audio_Kill)();
extern void (*XA_Audio_Off)();
extern void (*XA_Audio_On)();
extern void (*XA_Output_Audio)();
extern void  (*XA_Set_Output_Port)();
extern void  (*XA_Speaker_Tog)();
extern void  (*XA_Headphone_Tog)();
extern void  (*XA_LineOut_Tog)();
void XA_Audio_Init_Snd();
ULONG XA_Audio_Speaker(); 

void XA_Read_Audio_Delta();

extern Widget theWG;

/* POD TESTING */
ULONG fli_pad_kludge;

void TheEnd();
void TheEnd1();
void Hard_Death();
void Usage();
void Usage_Quick();
void ShowAnimation();
void ShowAction();
LONG Determine_Anim_Type();
void XA_Audio_Wait();
void XA_Cycle_Wait();
void XA_Cycle_It();
void Step_File_Next();
void Step_File_Prev();
void Step_Frame_Next();
void Step_Frame_Prev();
void Step_Action_Next();
void Free_Actions();
void ACT_Free_Act();
XA_ANIM_HDR *Get_Anim_Hdr();
XA_ANIM_HDR *Return_Anim_Hdr();
void Step_Action_Prev();
LONG XA_Time_Read();
void XA_Time_Init();
void XA_Time_Check();
void XA_Reset_AV_Time();
LONG XA_Read_AV_Time();
void XA_Reset_Speed_Time();
void XA_Install_CMAP();
ULONG XA_Mapped_To_Display();
ULONG XA_Read_Int();
float XA_Read_Float();
LONG XA_Get_Class();
void XA_Add_Pause();
void XA_SHOW_IMAGE();
void XA_SHOW_PIXMAP();
void XA_SHOW_IMAGES();
void XA_SHOW_PIXMAPS();
void XA_SHOW_DELTA();
XA_ANIM_SETUP *XA_Get_Anim_Setup();
void XA_Free_Anim_Setup();

void X11_Setup_Window();
void X11_Map_Window();
void X11_Init_Image_Struct();
void X11_Pre_Setup();
void ACT_Make_Images();
#ifdef XA_REMOTE_CONTROL
void XA_Create_Remote();
#endif

void CMAP_Manipulate_CHDRS();
void CMAP_Expand_Maps();
ULONG CMAP_Gamma_Adjust();

ULONG IFF_Read_File();
LONG Is_IFF_File();
ULONG GIF_Read_Anim();
LONG Is_GIF_File();
ULONG TXT_Read_File();
LONG Is_TXT_File();
ULONG Fli_Read_File();
LONG Is_FLI_File();
ULONG DL_Read_File();
LONG Is_DL_File();
/*POD TEMP
ULONG PFX_Read_File();
LONG Is_PFX_File();
*/
ULONG SET_Read_File();
LONG Is_SET_File();
ULONG RLE_Read_File();
LONG Is_RLE_File();
ULONG AVI_Read_File();
LONG Is_AVI_File();
ULONG QT_Read_File();
LONG Is_QT_File();
ULONG JFIF_Read_File();
LONG Is_JFIF_File();
ULONG MPG_Read_File();
LONG Is_MPG_File();
LONG Is_WAV_File();
ULONG WAV_Read_File();


ULONG shm = 0;
#ifdef XSHM
XShmSegmentInfo im0_shminfo;
XShmSegmentInfo im1_shminfo;
XShmSegmentInfo im2_shminfo;
XImage *im0_Image = 0;
XImage *im1_Image = 0;
XImage *im2_Image = 0;
XImage *sh_Image = 0;
#endif

#ifdef VMS
/*      
 *      Provide the UNIX gettimeofday() function for VMS C.
 *      The timezone is currently unsupported.
 */

struct timeval {
    long tv_sec;
    long tv_usec;
};
 
struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};
 
int gettimeofday( tp, tzp)
struct timeval *tp;
struct timezone *tzp;
{
   long curr_time[2];   /* Eight byte VAX time variable */
   long jan_01_1970[2] = { 0x4BEB4000,0x7C9567} ;
   long diff[2];
   long result;
   long vax_sec_conv = 10000000;
 
   result = sys$gettim( &curr_time );
 
   result = lib$subx( &curr_time, &jan_01_1970, &diff);
 
   if ( tp != 0) {
       result = lib$ediv( &vax_sec_conv, &diff,
                          &(tp->tv_sec), &(tp->tv_usec) );
       tp->tv_usec = tp->tv_usec / 10;  /* convert 1.e-7 to 1.e-6 */
   }
   if ( tzp !=0) { tzp->tz_minuteswest = 0; tzp->tz_dsttime=0;}
 
   return ( 0);
}
#endif


/*
 * Global X11 display configuation variables
 *
 * These are set by X11_Pre_Setup();
 *
 */

LONG x11_error_possible = 0; /* -1 err occured. 0 no error. 1 err expected */
LONG x11_depth;
LONG x11_class;
LONG x11_bytes_pixel;
LONG x11_byte_order;
LONG x11_bits_per_pixel;
LONG x11_bitmap_pad;
LONG x11_pack_flag;
LONG x11_bitmap_unit;
LONG x11_bit_order;
LONG x11_cmap_flag;
LONG x11_cmap_size;
LONG x11_disp_bits;
LONG x11_cmap_type;
LONG x11_depth_mask;
LONG x11_display_type;
LONG x11_red_mask;
LONG x11_green_mask;
LONG x11_blue_mask;
LONG x11_red_shift;
LONG x11_green_shift;
LONG x11_blue_shift;
LONG x11_red_bits;
LONG x11_green_bits;
LONG x11_blue_bits;
LONG x11_black;
LONG x11_white;
LONG x11_verbose_flag;
LONG pod_max_colors;
LONG xa_user_visual;
LONG xa_user_class;
ULONG x11_kludge_1;
/*POD TEST */
ULONG pod = 0;

/*
 * Each animation is broken up into a series of individual actions.
 * For example, a gif image would become two actions, 1 to setup the
 * colormap and 1 to display the image.
 *
 * XA_ACTION is defined in xanim.h. 
 *
 * action_cnt is a global variable that points to the next unused action.
 * Currently, individual routines access this. This will change.
 *
 * action_start is a variable passed to the Read_Animation routines, It
 * keeps track of the 1st action available to routine.
 *
 */

/*
 * anim_type is one of IFF_,FLI_,GIF_,TXT_,FADE_ANIM. 
 *
 * merged_anim_flags is the or of all anims read in. FLI's anims need
 * only 1 buffer. IFF anims need two. This allows software to allocate
 * for the worst case.
 *
 */
ULONG xa_anim_type;
ULONG xa_merged_anim_flags;


/*
 * cmap keeps track of the current colors to the screen.
 *
 */
LONG cmap_true_to_332;
LONG cmap_true_to_gray;
LONG cmap_true_to_1st;
LONG cmap_true_to_all;
LONG cmap_true_map_flag;
LONG cmap_luma_sort;
LONG cmap_map_to_1st_flag;
LONG cmap_map_to_one_flag;
LONG cmap_play_nice;
LONG cmap_force_load;
LONG xa_allow_nice;
LONG cmap_hist_flag;
LONG cmap_dither_type;
LONG cmap_median_type;
LONG cmap_median_bits;
LONG cmap_use_combsort;
double xa_disp_gamma;
double xa_anim_gamma;
ULONG xa_gamma_flag;
USHORT xa_gamma_adj[256];
XA_CHDR *xa_chdr_start;
XA_CHDR *xa_chdr_cur;
XA_CHDR *xa_chdr_now;
XA_CHDR *xa_chdr_first;
USHORT *cmap_cache,*cmap_cache2;
ULONG cmap_cache_size;
ULONG cmap_cache_bits;
ULONG cmap_cache_rmask;
ULONG cmap_cache_gmask;
ULONG cmap_cache_bmask;
XA_CHDR *cmap_cache_chdr;
SHORT cmap_floyd_error;

ULONG cmap_color_func;
ULONG cmap_sample_cnt;   /* sample anim every X many frames for colors */

/*
 * These are variables for HAM images
 *
 */
ULONG xa_ham_map_size;
ULONG *xa_ham_map;
XA_CHDR *xa_ham_chdr;
ULONG xa_ham_init;
/*
 * These are for converting TRUE images to PSEUDO
 *
 */
ULONG xa_r_shift,xa_g_shift,xa_b_shift;
ULONG xa_r_mask,xa_g_mask,xa_b_mask;
ULONG xa_gray_bits,xa_gray_shift;

ColorReg *xa_cmap = 0;
ULONG  xa_cmap_size,xa_cmap_off;
ULONG  *xa_map;
ULONG  xa_map_size,xa_map_off;

/*
 * Global variable to keep track of Anim type
 */
LONG filetype;
ULONG xa_title_flag;
char xa_title[256];


/*
 * Global variables to keep track of current width, height, num of colors and
 * number of bit planes respectively. 
 *
 * the max_ variable are used for worst case allocation. Are useful for Anims
 * that have multiple image sizes.
 *
 * image_size and max_image_size are imagex * imagey, etc.
 */
ULONG xa_image_size;
ULONG xa_max_image_size;
ULONG xa_imagex,xa_max_imagex;
ULONG xa_imagey,xa_max_imagey;
ULONG xa_imaged;

/*
 * Scaling Variable
 *
 */

ULONG xa_need_to_scale_b;
ULONG xa_need_to_scale_u;
float xa_scalex, xa_scaley;
float xa_bscalex, xa_bscaley;
ULONG xa_buff_x,xa_buff_y;		/* anim buffering size */
ULONG xa_allow_lace;
ULONG xa_allow_resizing; 		/* allow window resizing */
ULONG xa_disp_y, xa_max_disp_y;
ULONG xa_disp_x, xa_max_disp_x;
ULONG xa_disp_size;
ULONG xa_max_disp_size;
ULONG x11_display_x, x11_display_y;	/* max display size */
ULONG x11_window_x, x11_window_y;	/* current window size */
ULONG *xa_scale_row_buff;
ULONG xa_scale_row_size;
ULONG x11_expose_flag;

/* 
 * These variable keep track of where we are in the animation.
 * cur_file  ptr to the header of the current anim file. 
 * cur_floop keeps track of how many times we've looped a file.
 * cur_frame keeps track of which frame(action) we're on.
 *
 * xa_now_cycling and cycle_cnt are used for color cycling.
 *
 * file_is_started indicates whether this is the 1st time we've looped a file
 * or not. Is used to initialize variables and resize window if necessary.
 * 
 */
XA_ANIM_HDR *cur_file   = 0;
XA_ANIM_HDR *first_file = 0;
LONG xa_file_num;
LONG cur_floop,cur_frame;
LONG xa_cycle_cnt;      /* keeps track of number of cycling actions called */
LONG xa_now_cycling;    /* indicates that cycling is now in progress */
LONG xa_anim_cycling;   /* if set, allows cycling for animations */
LONG file_is_started;
int xa_vid_fd;		/* Used if anim is being read from a file */
int xa_aud_fd;		/* Used if anim is being read from a file */
UBYTE *xa_vidcodec_buf,*xa_audcodec_buf;
ULONG xa_vidcodec_maxsize, xa_audcodec_maxsize;
/*
 * Image buffers.
 * im_buff1 is used for double buffered anims(IFF).
 * xa_disp_buff is needed when the display is of a different format than the
 * double buffered images. (like HAM or TRUE_COLOR).
 *
 * xa_pic is a pointer to im_buff0 or im_buff1 during double buffering.
 * im_buff2 is used for dithered or HAM images
 */
char *im_buff0,*im_buff1,*im_buff2,*im_buff3;
char *xa_pic,*xa_disp_buff,*xa_scale_buff;
ULONG xa_disp_buff_size,xa_scale_buff_size;

/*
 * Variables for statistics
 *
 */
LONG xa_time_now;
LONG xa_time_video,xa_time_audio,xa_timelo_audio;
LONG xa_audio_start;
LONG xa_time_reset;
LONG xa_av_time_off;
LONG xa_skip_flag;
LONG xa_serious_skip_flag;
LONG xa_skip_video;
LONG xa_skip_cnt;	/* keeps track of how many we skip */
#define XA_SKIP_MAX 25  /* don't skip more than 5 frames in a row */
LONG xa_time_start;
LONG xa_time_end;
LONG xa_time_off;
LONG xa_no_disp;
LONG xa_time_flag;
LONG xa_time_av;
LONG xa_time_num;
struct timeval tv;


/* 
 * Global flags that are set on command line.
 */
LONG xa_buffer_flag;
LONG x11_shared_flag;
LONG xa_file_flag;
LONG fade_flag,fade_time;
LONG xa_noresize_flag;
LONG xa_optimize_flag;
LONG xa_pixmap_flag;
LONG xa_dither_flag;
LONG xa_pack_flag;
LONG xa_debug;
LONG xa_verbose;
LONG xa_quiet;

LONG xa_loop_each_flag;
LONG xa_pingpong_flag;
LONG xa_jiffy_flag;
ULONG xa_speed_scale,xa_speed_change;

LONG xa_anim_flags;
LONG xa_anim_holdoff;
LONG xa_anim_status,xa_old_status;
LONG xa_anim_ready = FALSE;
LONG xa_remote_ready = FALSE;
LONG xa_use_depth_flag;

LONG xa_exit_flag;
LONG xa_remote_flag;

XA_PAUSE *xa_pause_hdr=0;
LONG xa_pause_last;

char *xa_audio_device = DEFAULT_AUDIO_DEVICE_NAME;

/*
 * act is a global pointer to the current action.
 *
 */
XA_ACTION *act;

void
Usage_Quick()
{
  fprintf(stdout,"Usage:\n");
  fprintf(stdout,"   XAnim [options] anim [ [options] anim ... ]\n");
  fprintf(stdout,"   XAnim -h   for more detailed help.\n");
  TheEnd();
}
void
Usage_Default_TF(flag,justify)
ULONG flag,justify;
{
  if (justify == 1) fprintf(stdout,"            ");
  if (flag == TRUE) fprintf(stdout," default is on.\n");
  else fprintf(stdout," default is off.\n");
}

void
Usage_Default_Num(num,justify)
ULONG num,justify;
{
  if (justify == 1) fprintf(stdout,"            ");
  fprintf(stdout," default is %ld.\n",num);
}

/*
 * This function attempts to expain XAnim's usage if the command line
 * wasn't correct.
 */
void Usage()
{
 fprintf(stdout,"Usage:\n\n");
 fprintf(stdout,"xanim [+V#] [ [+|-]opts ...] animfile [ [ [+|-opts] animfile] ... ]\n");
 fprintf(stdout,"\n");
#ifdef VMS
 fprintf(stdout,"VMS users need to enclose opts in double qotes: \"+Cn\".\n");
 fprintf(stdout,"\n");
#endif
 if (DEFAULT_PLUS_IS_ON == TRUE) 
      fprintf(stdout,"A + turns an option on and a - turns it off.\n");
 else fprintf(stdout,"A - turns an option on and a + turns it off.\n");
 fprintf(stdout,"\n");
 fprintf(stdout,"Options:\n");
 fprintf(stdout,"\n  A[aopts]  Audio SubMenu\n");
 fprintf(stdout,"      ADdev AIX Audio only.  dev  is audio device.\n");
 fprintf(stdout,"      Ae    ENABLE Audio.\n");
 fprintf(stdout,"      Ak    Enables video frame skipping to keep in sync with audio.\n");
 fprintf(stdout,"      Ap#   Play Audio from output port #(Sparc only).\n");
 fprintf(stdout,"      As#   Scale Audio playback speed by #.\n");
 fprintf(stdout,"      Av#   Set Audio volume to #. range 0 to 100.\n");
 fprintf(stdout,"\n  C[copts]  Color SubMenu\n");
 fprintf(stdout,"      C1    Create cmap from 1st TrueColor frame. Map\n");
 fprintf(stdout,"            the rest to this first cmap.(Could be SLOW)\n");
 fprintf(stdout,"      C3    Convert TrueColor anims to 332(StaticColor).\n");
 Usage_Default_TF(DEFAULT_TRUE_TO_332,1);
 fprintf(stdout,"      Ca    Remap  all images to single new cmap.\n");
 Usage_Default_TF(DEFAULT_CMAP_MAP_TO_ONE,1);
 fprintf(stdout,"      Cd    Use floyd-steinberg dithering.\n");
 Usage_Default_TF((DEFAULT_CMAP_DITHER_TYPE==CMAP_DITHER_FLOYD),1);
 fprintf(stdout,"      Cf    Forcibly remap to 1st cmap\n");
 Usage_Default_TF(DEFAULT_CMAP_MAP_TO_1ST,1);
 fprintf(stdout,"      CF4   Better(?) Color mapping for TrueColor anims.\n");
 Usage_Default_TF(FALSE,1);
 fprintf(stdout,"      Cg    Convert TrueColor anims to gray scale.\n");
 Usage_Default_TF(DEFAULT_TRUE_TO_GRAY,1);
 fprintf(stdout,"      Ch    Use histogram to aid in color reduction.\n");
 Usage_Default_TF(DEFAULT_CMAP_HIST_FLAG,1);
 fprintf(stdout,"      Cn    Be Nice: allocate colors from Default cmap.\n");
 Usage_Default_TF(DEFAULT_CMAP_PLAY_NICE,1);
 fprintf(stdout,"\n  G[gopts]  Gamma SubMenu\n");
 fprintf(stdout,"      Ga#   Set gamma of animation. Default %f\n",
							(DEFAULT_ANIM_GAMMA));
 fprintf(stdout,"      Gd#   Set gamma of display. Default %f\n",
							(DEFAULT_DISP_GAMMA));
 fprintf(stdout,"\n  M[mopts]  Median-Cut SubMenu\n");
 fprintf(stdout,"      Ma    compute box color from average of box.\n");
 Usage_Default_TF( (DEFAULT_CMAP_MEDIAN_TYPE==CMAP_MEDIAN_SUM),1);
 fprintf(stdout,"      Mc    compute box color as center of box.\n");
 Usage_Default_TF( (DEFAULT_CMAP_MEDIAN_TYPE==CMAP_MEDIAN_CENTER),1);
 fprintf(stdout,"      Mb#   Truncate rgb to # bits.\n");
 Usage_Default_Num(CMAP_CACHE_MAX_BITS,1);
 fprintf(stdout,"\n  S[sopts]  Scaling and Sizing SubMenu\n");
 fprintf(stdout,"      Si    Half the height of IFF anims if Interlaced.\n");
 Usage_Default_TF(DEFAULT_ALLOW_LACE_FLAG,1);
 fprintf(stdout,"      Sn    Prevents X11 window from resizing to match anim's size.\n"); 
 Usage_Default_TF(DEFAULT_NORESIZE_FLAG,1);
 fprintf(stdout,"      Sr    Allow user to resize anim on the fly.\n");
 Usage_Default_TF(DEFAULT_ALLOW_RESIZING,1);
 fprintf(stdout,"      Ss#   Scale size of anim by # before displaying.\n");
 fprintf(stdout,"      Sh#   Scale width of anim by # before displaying.\n");
 fprintf(stdout,"      Sv#   Scale height of anim by # before displaying.\n");
 fprintf(stdout,"      Sx#   Scale anim to have width # before displaying.\n");
 fprintf(stdout,"      Sy#   Scale anim to have height # before displaying.\n");
 fprintf(stdout,"      Sc    Copy display scaling factors to buffer scaling factors\n");
 fprintf(stdout,"      SS#   Scale size of anim by # before buffering.\n");
 fprintf(stdout,"      SH#   Scale width of anim by # before buffering.\n");
 fprintf(stdout,"      SV#   Scale height of anim by # before buffering.\n");
 fprintf(stdout,"      SX#   Scale anim to have width # before buffering.\n");
 fprintf(stdout,"      SY#   Scale anim to have height # before buffering.\n");
 fprintf(stdout,"      SC    Copy buffer scaling factors to display scaling factors\n");
 fprintf(stdout,"\n  Normal Options\n");
 fprintf(stdout,"       b    Uncompress and buffer images ahead of time.\n"); 
 Usage_Default_TF(DEFAULT_BUFF_FLAG,1);
#ifdef XSHM
 fprintf(stdout,"       B    Use X11 Shared Memory Extention if supported.\n"); 
 Usage_Default_TF(FALSE,1);
#endif
 fprintf(stdout,"       c    disable looping for nonlooping iff anims.\n"); 
 Usage_Default_TF(DEFAULT_IFF_LOOP_OFF,1);
 fprintf(stdout,"       d#   debug. 0(off) to 5(most) for level of detail.\n"); 
 Usage_Default_Num(DEFAULT_DEBUG,1);
 fprintf(stdout,"       F    Floyd-Steinberg dithering for Cinepak Video compression\n");
 fprintf(stdout,"             or for Monochrome displays.\n");
 Usage_Default_TF(DEFAULT_DITHER_FLAG,1);
 fprintf(stdout,"       f    Don't load anims into memory, but read from file as needed\n"); 
 Usage_Default_TF(DEFAULT_BUFF_FLAG,1);
 fprintf(stdout,"       j#   # is number of milliseconds between frames.\n");
 fprintf(stdout,"	     if 0 then default depends on the animation.\n");
 Usage_Default_Num(DEFAULT_JIFFY_FLAG,1);
 fprintf(stdout,"       l#   loop anim # times before moving on.\n");
 Usage_Default_Num(DEFAULT_LOOPEACH_FLAG,1);
 fprintf(stdout,"       lp#  ping-pong anim # times before moving on.\n");
 Usage_Default_Num(DEFAULT_PINGPONG_FLAG,1);
 fprintf(stdout,"       N    No display. Useful for benchmarking.\n");
 fprintf(stdout,"       o    turns on certain optimizations. See readme.\n"); 
 Usage_Default_TF(DEFAULT_OPTIMIZE_FLAG,1);
 fprintf(stdout,"       p    Use Pixmap instead of Image in X11.\n"); 
 Usage_Default_TF(DEFAULT_PIXMAP_FLAG,1);
 fprintf(stdout,"       q    quiet mode.\n"); 
 fprintf(stdout,"       r    Allow color cycling for IFF single images.\n");
 Usage_Default_TF(DEFAULT_CYCLE_IMAGE_FLAG,1);
 fprintf(stdout,"       R    Allow color cycling for IFF anims.\n");
 Usage_Default_TF(DEFAULT_CYCLE_ANIM_FLAG,1);
 fprintf(stdout,"       T#   Title Option. See readme.\n");
 fprintf(stdout,"       v    verbose mode.\n"); 
 fprintf(stdout,"       V#   Use Visual #. # is obtained by +X option.\n"); 
 fprintf(stdout,"       X    X11 verbose mode. Display Visual information.\n");
 fprintf(stdout,"       Z    Have XAnim exit after playing cmd line.\n");
 Usage_Default_TF(DEFAULT_VERBOSE,1);
 fprintf(stdout,"\n");
 fprintf(stdout,"Window commands.\n");
 fprintf(stdout,"\n");
 fprintf(stdout,"        q    quit.\n");
 fprintf(stdout,"        Q    Quit.\n");
 fprintf(stdout,"        g    Stop color cycling.\n");
 fprintf(stdout,"        r    Restore original Colors(useful after g).\n");
 fprintf(stdout,"    <space>  Toggle. Starts/Stops animation.\n");
 fprintf(stdout,"        ,    Single step back one frame.\n");
 fprintf(stdout,"        .    Single step forward one frame.\n");
 fprintf(stdout,"        <    Go back to start of previous anim.\n");
 fprintf(stdout,"        >    Go forward to start of next anim.\n");
 fprintf(stdout,"        m    Single step back one frame staying within anim.\n");
 fprintf(stdout,"        /    Single step forward one frame staying within anim.\n");
 fprintf(stdout,"        -    Increase animation playback speed.\n");
 fprintf(stdout,"        =    Decrease animation playback speed.\n");
 fprintf(stdout,"        0    Reset animation playback speed to original values.\n");
 fprintf(stdout,"\n");
 fprintf(stdout,"Mouse Buttons.\n");
 fprintf(stdout,"\n");
 fprintf(stdout,"      <Left> Single step back one frame.\n");
 fprintf(stdout,"    <Middle> Toggle. Starts/Stops animation.\n");
 fprintf(stdout,"     <Right> Single step forward one frame.\n");
 fprintf(stdout,"\n");
 exit(0);
}

int main(argc, argv)
int argc;
char *argv[];
{
 char *filename,*in;
 LONG i,j;

/* 
 * Initialize global variables.
 */
 theDisp = NULL;

 cmap_color_func = 0;
 cmap_sample_cnt = 5; /* default value */

#ifdef XSHM
 im0_shminfo.shmaddr = 0;
 im1_shminfo.shmaddr = 0;
 im2_shminfo.shmaddr = 0;
#endif
 im_buff0 = im_buff1 = im_buff2 = im_buff3 = 0;
 xa_disp_buff = 0;
 xa_disp_buff_size = 0;
 xa_scale_row_buff = 0;
 xa_scale_row_size = 0;
 xa_scale_buff	= 0;
 xa_scale_buff_size = 0;
 xa_imagex	= 0;
 xa_imagey	= 0;
 xa_disp_x	= 0;
 xa_disp_y	= 0;
 xa_scalex	= 1.0;
 xa_scaley	= 1.0;
 xa_buff_x	= 0;
 xa_buff_y	= 0;
 xa_bscalex	= 1.0;
 xa_bscaley	= 1.0;
 xa_need_to_scale_b = 0;
 xa_need_to_scale_u = 0;
 xa_max_imagex	= 0;
 xa_max_imagey	= 0;
 xa_max_disp_x	= 0;
 xa_max_disp_y	= 0;
 xa_anim_flags		= 0;
 xa_merged_anim_flags	= 0;
 x11_pack_flag	= FALSE;
 x11_expose_flag = FALSE;
 x11_error_possible = 0;

 first_file = cur_file = 0;
 xa_file_num = -1;

 xa_anim_holdoff	= TRUE;
 xa_anim_status		= XA_UNSTARTED;
 xa_old_status		= XA_UNSTARTED;
 xa_anim_ready		= FALSE;
 xa_remote_ready	= FALSE;

 xa_audio_init		= FALSE;
#ifdef XA_AUDIO
 xa_audio_present	= XA_AUDIO_UNK;
 xa_audio_enable	= DEFAULT_XA_AUDIO_ENABLE;
 xa_audio_status	= XA_AUDIO_NICHTDA;
#else
 xa_audio_present	= XA_AUDIO_NONE;
 xa_audio_enable	= FALSE;
 xa_audio_status	= XA_AUDIO_NICHTDA;
#endif
 xa_audio_scale		= 1.0;
 xa_audio_mute		= FALSE;
 xa_audio_volume	= DEFAULT_XA_AUDIO_VOLUME;
 if (xa_audio_volume > XA_AUDIO_MAXVOL) xa_audio_volume = XA_AUDIO_MAXVOL;
 xa_audio_newvol	= TRUE;
 xa_audio_playrate	= 0;
 xa_audio_start		= 0;
 xa_time_reset		= 0;
 xa_audio_port 		= XA_Audio_Speaker("SPEAKER");
 /* if DEFAULT_XA_AUDIO_PORT is 0, then XAnim doesn't reset it */
 if (xa_audio_port == 0) xa_audio_port = DEFAULT_XA_AUDIO_PORT;
/*
#ifdef XA_SGI_AUDIO
 xa_audio_divtest	= 2;
#else
 xa_audio_divtest	= 5;
#endif
*/
 xa_audio_divtest	= 2;
 xa_audio_synctst	= 0;
 xa_time_now		= 0;
 xa_time_video		= 0;
 xa_time_audio		= -1;
 xa_timelo_audio	= 0;
 xa_skip_flag		= TRUE;
 xa_serious_skip_flag	= FALSE;
 xa_skip_video		= 0;
 xa_skip_cnt		= 0;
 xa_audio_buffer	= FALSE;

 xa_vid_fd		= xa_aud_fd	= -1;
 xa_vidcodec_buf	= xa_audcodec_buf	= 0;
 xa_vidcodec_maxsize	= xa_audcodec_maxsize	= 0;
 xa_buffer_flag		= DEFAULT_BUFF_FLAG;
 if (xa_buffer_flag == TRUE) xa_file_flag = FALSE;
 else xa_file_flag	= DEFAULT_FILE_FLAG;
 x11_shared_flag	= TRUE;
 xa_pack_flag		= DEFAULT_PACK_FLAG;
 xa_noresize_flag	= DEFAULT_NORESIZE_FLAG;
 xa_verbose		= DEFAULT_VERBOSE;
 xa_quiet		= DEFAULT_QUIET;
 xa_debug		= DEFAULT_DEBUG;
 xa_anim_cycling	= DEFAULT_CYCLE_ANIM_FLAG;
 xa_allow_lace		= DEFAULT_ALLOW_LACE_FLAG;
 xa_allow_resizing	= DEFAULT_ALLOW_RESIZING;
 xa_optimize_flag	= DEFAULT_OPTIMIZE_FLAG;
 xa_pixmap_flag		= DEFAULT_PIXMAP_FLAG;
 xa_dither_flag		= DEFAULT_DITHER_FLAG;
 xa_loop_each_flag	= DEFAULT_LOOPEACH_FLAG;
 xa_pingpong_flag	= DEFAULT_PINGPONG_FLAG;
 xa_jiffy_flag		= DEFAULT_JIFFY_FLAG;
 xa_speed_scale		= XA_SPEED_NORM;
 xa_speed_change	= 0;
 x11_verbose_flag	= DEFAULT_X11_VERBOSE_FLAG;
 x11_kludge_1		= FALSE;
 cmap_luma_sort		= DEFAULT_CMAP_LUMA_SORT;
 cmap_map_to_1st_flag	= DEFAULT_CMAP_MAP_TO_1ST;
 cmap_map_to_one_flag	= DEFAULT_CMAP_MAP_TO_ONE;
 cmap_play_nice		= DEFAULT_CMAP_PLAY_NICE;
 cmap_force_load	= FALSE;
 xa_allow_nice		= TRUE;
 cmap_hist_flag		= DEFAULT_CMAP_HIST_FLAG;
 cmap_dither_type	= DEFAULT_CMAP_DITHER_TYPE;

 cmap_median_type	= DEFAULT_CMAP_MEDIAN_TYPE;
 cmap_median_bits	= 6;
 cmap_use_combsort	= TRUE;
 cmap_floyd_error	= 256; 
 cmap_cache		= cmap_cache2		= 0;
 cmap_cache_size	= 0;
 cmap_cache_bits	= 0;
 cmap_cache_rmask	= 0;
 cmap_cache_gmask	= 0;
 cmap_cache_bmask	= 0;
 cmap_cache_chdr	= 0;
 xa_disp_gamma		= DEFAULT_DISP_GAMMA;
 xa_anim_gamma		= DEFAULT_ANIM_GAMMA;
 xa_gamma_flag		= FALSE;

 xa_title_flag		= DEFAULT_XA_TITLE_FLAG;
 xa_exit_flag		= DEFAULT_XA_EXIT_FLAG;
 xa_remote_flag		= DEFAULT_XA_REMOTE_FLAG;

 xa_chdr_start		= 0;
 xa_chdr_cur		= 0;
 xa_chdr_now		= 0;
 xa_chdr_first		= 0;
 xa_cmap		= 0;
 xa_cmap_size		= 0;
 xa_cmap_off		= 0;
 xa_map			= 0;
 xa_map_size		= 0;
 xa_map_off		= 0;
 xa_time_start		= 0;
 xa_time_end		= 0;
 xa_time_av 		= 0;
 xa_time_num		= 0;
 xa_no_disp		= FALSE;
 xa_time_flag		= FALSE;
 pod_max_colors		= 0;
 xa_r_shift = xa_g_shift = xa_b_shift = 0;
 xa_r_mask = xa_g_mask = xa_b_mask = 0;
 xa_gray_bits = xa_gray_shift = 0;
 xa_ham_map_size = 0;
 xa_ham_map = 0;
 xa_ham_chdr = 0;
 xa_ham_init = 0;
 fli_pad_kludge = FALSE;
 xa_pause_hdr = 0;
 xa_pause_last = FALSE;

 DEBUG_LEVEL5 (void)fprintf(stdout,"Calling XA_Time_Init()\n");
 XA_Time_Init();
 xa_gamma_flag = CMAP_Gamma_Adjust(xa_gamma_adj,xa_disp_gamma,xa_anim_gamma);

 if (DEFAULT_IFF_LOOP_OFF  == TRUE) xa_anim_flags |= ANIM_NOLOOP;
 if (DEFAULT_CYCLE_IMAGE_FLAG == TRUE) xa_anim_flags |= ANIM_CYCLE;

/* setup for dying time.
 */
 signal(SIGINT,Hard_Death);

 /* pre-check for user visual */
 /* Note -- we also have to pre-check for the d option, since we could
  * run through a good deal of code before we even set xa_debug!!!
  */
 xa_user_visual = -1;
 xa_user_class  = -1;
 for(i=1;i<argc;i++)
 {
   in = argv[i];

   if ( ((in[0]=='-') || (in[0]=='+')) && (in[1]=='V') )
   {
     if ( (in[2] >= '0') && (in[2] <= '9') )  /* number */
		xa_user_visual = atoi(&in[2]);
     else	xa_user_class = XA_Get_Class( &in[2] );
   }
   else if ( (in[0]=='-') && (in[1]=='X') ) x11_verbose_flag = FALSE;
   else if ( (in[0]=='+') && (in[1]=='X') ) x11_verbose_flag = TRUE;
   else if ( (in[0]=='+') && (in[1]=='B') ) x11_shared_flag = TRUE;
   else if ( (in[0]=='-') && (in[1]=='B') ) x11_shared_flag = FALSE;
   else if ( (in[0]=='+') && (in[1]=='q') ) xa_quiet = TRUE;
   else if ( (in[0]=='-') && (in[1]=='q') ) xa_quiet = FALSE;
   else if ( ( (in[0]=='-') && (in[1]=='d') )   ||
             ( (in[0]=='+') && (in[1]=='d') ) ){
     if ( (in[2] >= '0') && (in[2] <= '9') )  /* number */
                xa_debug = atoi(&in[2]);
     if (xa_debug <= 0) xa_debug = 0;
   }
   else if ( (in[0]=='+') && (in[1]=='P') )
   {
     pod_max_colors = atoi(&in[2]); /* POD Testing only */
   }
   else if ( ((in[0]=='-') || (in[0]=='+')) && (in[1]=='h') ) Usage();
 }

/* What REV are we running
 */
 if ( (xa_quiet==FALSE) || (xa_verbose==TRUE) || (xa_debug >= 1) )
 fprintf(stdout,"XAnim Rev %2.2f.%ld.%lda by Mark Podlipec (c) 1991-1995\n",DA_REV,DA_MINOR_REV,DA_BETA_REV);

/* quick command line check.
 */
 if (argc<2) Usage_Quick();


 /* PreSet of X11 Display to find out what we're dealing with
  */
 DEBUG_LEVEL5 (void)fprintf(stdout,"Calling X11_Pre_Setup()\n");
 X11_Pre_Setup(&argc, argv, xa_user_visual, xa_user_class);
 if (xa_allow_nice == FALSE) cmap_play_nice = FALSE;
 if ( (x11_bits_per_pixel == 2) || (x11_bits_per_pixel == 4) )
						x11_pack_flag = TRUE;
  if (x11_display_type == XA_MONOCHROME) xa_dither_flag = DEFAULT_DITHER_FLAG;
  else if (   (x11_display_type == XA_PSEUDOCOLOR)
	   || (x11_display_type == XA_STATICCOLOR) )
  {
    if (cmap_color_func == 4) xa_dither_flag = FALSE;
    else if (cmap_color_func == 5) xa_dither_flag = TRUE;
    else xa_dither_flag = DEFAULT_DITHER_FLAG;
  }
  else xa_dither_flag = FALSE;

 /* Audio Setup */
 DEBUG_LEVEL5 (void)fprintf(stdout,"Calling XA_Audio_Setup()\n");
 XA_Audio_Setup();

 /* Visual Dependent switches and flags */
 if (x11_display_type & XA_X11_TRUE)
 {
   cmap_true_to_332  = FALSE; cmap_true_to_gray = FALSE;
   cmap_true_to_1st  = FALSE; cmap_true_to_all  = FALSE;
   cmap_true_map_flag = FALSE;
 }
 else if (x11_display_type & XA_X11_COLOR)
 {
   cmap_true_to_332  = DEFAULT_TRUE_TO_332;
   cmap_true_to_gray = DEFAULT_TRUE_TO_GRAY;
   cmap_true_to_1st  = DEFAULT_TRUE_TO_1ST;
   cmap_true_to_all  = DEFAULT_TRUE_TO_ALL;
   if (cmap_true_to_1st || cmap_true_to_all) cmap_true_map_flag = TRUE;
   else cmap_true_map_flag = DEFAULT_TRUE_MAP_FLAG;
 }
 else { cmap_true_to_332  = FALSE; cmap_true_to_gray = TRUE; 
        cmap_true_to_1st  = FALSE; cmap_true_to_all  = FALSE;
        cmap_true_map_flag = FALSE;  }

 xa_time_start = XA_Time_Read();

/* Parse command line.
 */
 for(i=1; i < argc; i++)
 {
   in = argv[i];
   if ( (in[0] == '-') || (in[0] == '+') )
   {
     LONG len,opt_on;

     if (in[0] == '-') opt_on = FALSE;
     else opt_on = TRUE;
     /* if + turns off then reverse opt_on */
     if (DEFAULT_PLUS_IS_ON == FALSE) opt_on = (opt_on==TRUE)?FALSE:TRUE;

     len = strlen(argv[i]);
     j = 1;
     while( (j < len) && (in[j] != 0) )
     {
       switch(in[j])
       {
        case 'A':   /* audio options sub menu */
	{
	  ULONG exit_flag = FALSE;
	  j++;
	  while( (exit_flag == FALSE) && (j<len) )
	  {
	    switch(in[j])
	    {
	      case 'b':   /* snd buffer on/off */
		j++; xa_audio_buffer = opt_on;
		break;
	      case 'e':   /* snd enable on/off */
		j++; xa_audio_enable = opt_on;
		break;
	      case 'k':   /* toggle skip video */
		j++; xa_skip_flag = opt_on;
		break;
	      case 'p':   /* select audio port */
		{ ULONG t,val;
		  j++; t = XA_Read_Int(in,&j);
		  switch(t)
		  {
		    case 0: val = XA_AUDIO_PORT_INT; break;
		    case 1: val = XA_AUDIO_PORT_HEAD; break;
		    case 2: val = XA_AUDIO_PORT_EXT; break;
		    default: val = 0;
		  }
		  if (opt_on == TRUE) xa_audio_port |= val;
		  else xa_audio_port &= ~(val);
		}
		break;
	      case 'd':   /* POD TEST */
		j++; xa_audio_divtest = XA_Read_Int(in,&j);
		if (xa_audio_divtest==0) xa_audio_divtest = 2;
		break;
	      case 'D':   /* s/6000 audio device name */
	      {
		j++; xa_audio_device = &in[j];
		j = len;
	      }
	      break;
	      case 'r':   /* POD TEST */
		j++; xa_audio_playrate = XA_Read_Int(in,&j);
		if (xa_audio_playrate==0) xa_audio_playrate = 8000;
		break;
	      case 's':   /* snd scale */
		j++; xa_audio_scale = XA_Read_Float(in,&j);
		fprintf(stderr,"XAnim: +As# temporarily disabled.\n");
		if (xa_audio_scale < 0.125) xa_audio_scale = 0.125;
		if (xa_audio_scale > 8.000) xa_audio_scale = 8.000;
		break;
	      case 't':   /* POD TEST */
		j++; xa_audio_synctst = XA_Read_Int(in,&j);
		break;
	      case 'v':
		j++; xa_audio_volume = XA_Read_Int(in,&j);
		if (xa_audio_volume > XA_AUDIO_MAXVOL) 
					xa_audio_volume = XA_AUDIO_MAXVOL;
		break;
	      default: exit_flag = TRUE;
	    }
	  }
	}
	break;
        case 'C':   /* colormap options sub menu */
	  {
	    ULONG exit_flag = FALSE;
	    j++;
	    while( (exit_flag == FALSE) && (j<len) )
	    {
		switch(in[j])
		{
		  case 's':
			j++; cmap_sample_cnt = XA_Read_Int(in,&j);
			break;
		  case 'F':
			j++; cmap_color_func = XA_Read_Int(in,&j);
			if (cmap_color_func==4) xa_dither_flag = FALSE;
			else if (cmap_color_func==5)
			{ xa_dither_flag = TRUE;
			  cmap_color_func = 4;
			}
			break;
		  case '1':
 			if (   (x11_display_type & XA_X11_TRUE)
 			    || (x11_display_type == XA_MONOCHROME) ) 
							opt_on = FALSE;
			cmap_true_to_1st  = opt_on;
			if (cmap_true_to_1st == TRUE)
			{
			  cmap_true_to_gray = FALSE; cmap_true_to_332  = FALSE;
			  cmap_true_to_all  = FALSE; cmap_true_map_flag = TRUE;
			}
			j++; break;
		  case 'A':
 			if (   (x11_display_type & XA_X11_TRUE)
 			    || (x11_display_type == XA_MONOCHROME) ) 
							opt_on = FALSE;
			cmap_true_to_all  = opt_on;
			if (cmap_true_to_all == TRUE)
			{
			  cmap_true_to_gray = FALSE; cmap_true_to_332  = FALSE;
			  cmap_true_to_1st  = FALSE; cmap_true_map_flag = TRUE;
			}
			j++; break;
		  case '3':
 			if (   (x11_display_type & XA_X11_TRUE)
 			    || (!(x11_display_type & XA_X11_COLOR))
 			    || (x11_display_type == XA_MONOCHROME) ) 
							opt_on = FALSE;
			cmap_true_to_332  = opt_on;
			if (opt_on == TRUE)
			{
			  cmap_true_to_gray = FALSE; cmap_true_to_1st  = FALSE;
			  cmap_true_to_all  = FALSE;
			}
			j++; break;
		  case 'g':
 			if (x11_display_type & XA_X11_TRUE) opt_on = FALSE;
			cmap_true_to_gray  = opt_on;
			if (opt_on == TRUE)
			{
			  cmap_true_to_332 = FALSE; cmap_true_to_1st = FALSE;
			  cmap_true_to_all = FALSE;
			}
			j++; break;
		  case 'a': cmap_map_to_one_flag = opt_on; j++; break;
		  case 'c': cmap_force_load = opt_on; j++; break;
		  case 'd': cmap_dither_type = CMAP_DITHER_FLOYD; j++; break;
		  case 'l': cmap_luma_sort  = opt_on; j++; break;
		  case 'f': cmap_map_to_1st_flag = opt_on; j++; break;
		  case 'm': cmap_true_map_flag = opt_on; j++; break;
		  case 'n': j++;
		    if (xa_allow_nice == TRUE) cmap_play_nice  = opt_on;
		    break;
		  case 'h': cmap_hist_flag  = opt_on; j++; break;
		  default: exit_flag = TRUE;
		}
	    }
	  }
          break;
	case 'G': /* gamma options sub-menu */ 
	  {
	    ULONG exit_flag = FALSE;
	    j++;
	    while( (exit_flag == FALSE) && (j<len) )
	    {
		switch(in[j])
		{
		  case 'a': /* set anim gamma */
			j++;
			xa_anim_gamma = XA_Read_Float(in,&j);
			if (xa_anim_gamma <= 0.0001) xa_anim_gamma = 0.0;
			xa_gamma_flag = CMAP_Gamma_Adjust(xa_gamma_adj,
						xa_disp_gamma,xa_anim_gamma);
			break;
		  case 'd': /* set display gamma */
			j++;
			xa_disp_gamma = XA_Read_Float(in,&j);
			if (xa_disp_gamma <= 0.0001) xa_disp_gamma = 0.0;
			xa_gamma_flag = CMAP_Gamma_Adjust(xa_gamma_adj,
						xa_disp_gamma,xa_anim_gamma);
			break;
		  default: exit_flag = TRUE;
		}
	    }
	  }
          break;

	case 'M': /* median cut options sub-menu */ 
	  {
	    ULONG exit_flag = FALSE;
	    j++;
	    while( (exit_flag == FALSE) && (j<len) )
	    {
		switch(in[j])
		{
		  case 'a': cmap_median_type = CMAP_MEDIAN_SUM; j++; break;
		  case 'c': cmap_median_type = CMAP_MEDIAN_CENTER; j++; break;
		  /* POD Testing only */
		  case 'e':
			j++; cmap_floyd_error = XA_Read_Int(in,&j);
			if (cmap_floyd_error <= 0) cmap_floyd_error = 0;
			break;
		  case 'b':
			j++; cmap_median_bits = XA_Read_Int(in,&j);
			if (cmap_median_bits <= 5) cmap_median_bits = 5;
			if (cmap_median_bits >  8) cmap_median_bits = 8;
			break;
		  /* POD benchmark only */
		  case 'q': if (opt_on == TRUE) cmap_use_combsort = FALSE;
			    else cmap_use_combsort = TRUE;
			    j++; break;
		  default: exit_flag = TRUE;
		}
	    }
	  }
          break;
        case 'B':
                x11_shared_flag = opt_on;	j++;
                break;
        case 'b':
                xa_buffer_flag = opt_on;	j++;
		if (xa_buffer_flag==TRUE) xa_file_flag = FALSE;
                break;
        case 'c':
                if (opt_on==TRUE) xa_anim_flags |= ANIM_NOLOOP;
                else xa_anim_flags &= (~ANIM_NOLOOP);
                j++; break;
        case 'd':
                j++; xa_debug = XA_Read_Int(in,&j);
                if (xa_debug <= 0) xa_debug = 0;
		break;
        case 'f':
                xa_file_flag = opt_on;	j++;
		if (xa_file_flag==TRUE) xa_buffer_flag = FALSE;
		break;
        case 'F':
                xa_dither_flag = opt_on;	j++;
		if (x11_display_type & XA_X11_TRUE) xa_dither_flag = FALSE;
		break;
        case 'h':
                Usage(); break;
	case 'J':  /*PODNOTE: make into defines */
		j++; xa_speed_scale = XA_Read_Int(in,&j);
		if (xa_speed_scale == 0) xa_speed_scale = 1;
		if (xa_speed_scale > 16) xa_speed_scale = 16;
		if (opt_on == TRUE) xa_speed_scale <<= 4;
		else	xa_speed_scale = (1<<4) / xa_speed_scale;
		break;
        case 'j':
                j++;
		if ( (in[j] >= '0') || (in[j] <= '9') )
		{ 
                  xa_jiffy_flag = XA_Read_Int(in,&j);
                  if (xa_jiffy_flag <= 0) xa_jiffy_flag = 1;
		}
		else xa_jiffy_flag = 0; /* off */
                break;
        case 'k':
		{ ULONG tmp;
                  j++;
		  if ( (in[j] >= '0') || (in[j] <= '9') )
		  { 
                    tmp = XA_Read_Int(in,&j);
                    if (tmp==1) fli_pad_kludge = opt_on;
		  }
		}
                break;
        case 'l':
		j++; 
		if (in[j] == 'p')
		{
		  xa_pingpong_flag = opt_on; j++;
		}
		else xa_pingpong_flag = FALSE;
                xa_loop_each_flag = XA_Read_Int(in,&j);
                if (xa_loop_each_flag<=0) xa_loop_each_flag = 1;
		break;
        case 'N':
		xa_no_disp = opt_on;
                xa_time_flag = opt_on;	j++;	break;
        case 'o':
                xa_optimize_flag = opt_on; j++;	break;
        case 'p':
                xa_pixmap_flag = opt_on; j++;
		if (opt_on==TRUE) xa_anim_flags |= ANIM_PIXMAP;
		else xa_anim_flags &= ~ANIM_PIXMAP;
                break;
        case 'P':
                j++; pod_max_colors = XA_Read_Int(in,&j);
                if (pod_max_colors <= 0) pod_max_colors = 0;
		break;
        case 'r':
                if (opt_on == TRUE)	xa_anim_flags |= ANIM_CYCLE;
                else			xa_anim_flags &= (~ANIM_CYCLE);
                j++;
                break;
        case 'R':
                xa_anim_cycling = opt_on; j++;	break;
        case 'S':
	  {
	    ULONG exit_flag = FALSE;
	    j++;
	    while( (exit_flag==FALSE) && (j<len) )
	    {
		switch(in[j])
		{
		  case 'i': xa_allow_lace = opt_on;  j++; break;
		  case 'n': xa_noresize_flag = opt_on;  j++; break;
		  case 'r': xa_allow_resizing = opt_on; j++; break;
		  case 'c': /* copy display scaling to buffer scaling */
		    j++;
		    xa_bscalex = xa_scalex; xa_bscaley = xa_scaley;
		    xa_buff_x = xa_disp_x; xa_buff_y = xa_disp_y;
		    break;
		  case 's': /* scale anim display size */
		    j++;
                    xa_scalex = XA_Read_Float(in,&j);
		    if (xa_scalex == 0.0) xa_scalex = 1.0;
		    xa_scaley = xa_scalex;
		    xa_disp_x = xa_disp_y = 0; break;
		  case 'x':  /* set anim display x size */
		    j++; xa_scalex = 1.0;
                    xa_disp_x = XA_Read_Int(in,&j);
		    break;
		  case 'y':  /* set anim display y size */
		    j++; xa_scaley = 1.0;
                    xa_disp_y = XA_Read_Int(in,&j);
		    break;
		  case 'h':  /* scale anim display horizontally */
		    j++;
                    xa_scalex = XA_Read_Float(in,&j);
		    if (xa_scalex == 0.0) xa_scalex = 1.0;
		    xa_disp_x = 0; break;
		  case 'v':  /* scale anim display vertically */
		    j++;
                    xa_scaley = XA_Read_Float(in,&j);
		    if (xa_scaley == 0.0) xa_scaley = 1.0;
		    xa_disp_y = 0; break;
		  case 'C': /* copy buffer scaling to display scaling */
		    j++;
		    xa_scalex = xa_bscalex; xa_scaley = xa_bscaley;
		    xa_disp_x = xa_buff_x; xa_disp_y = xa_buff_y;
		    break;
		  case 'S': /* scale anim buffering size */
		    j++;
                    xa_bscalex = XA_Read_Float(in,&j);
		    if (xa_bscalex == 0.0) xa_bscalex = 1.0;
		    xa_bscaley = xa_bscalex;
		    xa_buff_x = xa_buff_y = 0; break;
		  case 'X':  /* set anim buffering x size */
		    j++; xa_bscalex = 1.0;
                    xa_buff_x = XA_Read_Int(in,&j);
		    break;
		  case 'Y':  /* set anim buffering y size */
		    j++; xa_bscaley = 1.0;
                    xa_buff_y = XA_Read_Int(in,&j);
		    break;
		  case 'H':  /* scale anim buffering horizontally */
		    j++;
                    xa_bscalex = XA_Read_Float(in,&j);
		    if (xa_bscalex == 0.0) xa_bscalex = 1.0;
		    xa_buff_x = 0; break;
		  case 'V':  /* scale anim buffering vertically */
		    j++;
                    xa_bscaley = XA_Read_Float(in,&j);
		    if (xa_bscaley == 0.0) xa_bscaley = 1.0;
		    xa_buff_y = 0; break;
		  default: exit_flag = TRUE;
		}
	    }
	  }
	  break;
        case 't':
                xa_time_flag = opt_on;	j++;	break;
        case 'T':
                j++; xa_title_flag = XA_Read_Int(in,&j);
                if (xa_title_flag > 2) xa_title_flag = 2;
		break;
        case 'q':
                xa_quiet = opt_on;	j++;	break;
        case 'v':
                xa_verbose = opt_on;	j++;	break;
        case 'X':
		X11_Show_Visuals();	j++;	break;
        case 'V':
		j += len; break;   /* ignore reset and move on */
        case 'Z':
	  {
	    ULONG exit_flag = FALSE;
	    j++;
	    while( (exit_flag==FALSE) && (j<len) )
	    { ULONG tmp;
		switch(in[j])
		{
		  case 'e': xa_exit_flag = opt_on;	j++;	break;
		  case 'p':
		    j++;
		    if (in[j] == 'e') {xa_pause_last = opt_on; j++; }
		    else { tmp = XA_Read_Int(in,&j); XA_Add_Pause(tmp); }
		    break;
		  case 'r': xa_remote_flag = opt_on;	j++;	break;
		  case 'z': /* POD TEST SWITCH FOR TRY DIFFERENT THINGS */
		    j++; pod = XA_Read_Int(in,&j);
		    break;
		  default: exit_flag = TRUE;
		}
	    }
	  }
	  break;
        case '-':
                opt_on = (DEFAULT_PLUS_IS_ON==TRUE)?FALSE:TRUE; j++;	break;
        case '+':
                opt_on = (DEFAULT_PLUS_IS_ON==TRUE)?TRUE:FALSE; j++;	break;
        default:
                Usage_Quick();
       } /* end of option switch */
     } /* end of loop through options */
   } /* end of if - */
   else 
   /* If no hyphen in front of argument, assume it's a file.
    */
   { ULONG result,audio_attempt;
     filename = argv[i];

     /* Visual Dependent switches and flags */
     if (x11_display_type & XA_X11_TRUE)
     {
       cmap_true_to_332  = cmap_true_to_gray = FALSE;
       cmap_true_to_1st  = cmap_true_to_all  = cmap_true_map_flag = FALSE;
     }
     else if (!(x11_display_type & XA_X11_COLOR))
     { cmap_true_to_gray = TRUE; cmap_true_to_332  = FALSE;
       cmap_true_to_1st  = cmap_true_to_all  = cmap_true_map_flag = FALSE;
     }
    
     /* disable audio is timing has changed */
     if (xa_jiffy_flag) xa_audio_enable = FALSE;
     /* pass audio flag to routines */
     if ( (xa_audio_enable == TRUE) &&
          ((xa_audio_present==XA_AUDIO_UNK) || (xa_audio_present==XA_AUDIO_OK))
        ) audio_attempt = TRUE;
     else audio_attempt = FALSE;

     if ( (x11_display_type == XA_MONOCHROME) || (xa_pack_flag == TRUE) )
       xa_use_depth_flag = FALSE;
     else
       xa_use_depth_flag = TRUE;
        /* default is FLI  */
     cur_file = Get_Anim_Hdr(cur_file,filename);
     xa_anim_type = Determine_Anim_Type(filename);
     cur_file->anim_type = xa_anim_type;
     cur_file->anim_flags = xa_anim_flags;
     if (x11_display_type == XA_MONOCHROME) xa_optimize_flag = FALSE;
     switch(xa_anim_type)
     {
        case IFF_ANIM:
	  if (xa_verbose) fprintf(stderr,"Reading IFF File %s\n",filename);
	  result = IFF_Read_File(filename,cur_file);
	  break;
        case GIF_ANIM:
	  if (xa_verbose) fprintf(stderr,"Reading GIF File %s\n",filename);
	  result = GIF_Read_Anim(filename,cur_file);
	  break;
        case TXT_ANIM:
	  if (xa_verbose) fprintf(stderr,"Reading TXT File %s\n",filename);
	  result = TXT_Read_File(filename,cur_file);
	  break;
        case FLI_ANIM:
	  if (xa_verbose) fprintf(stderr,"Reading FLI File %s\n",filename);
	  result = Fli_Read_File(filename,cur_file);
	  break;
        case DL_ANIM:
	  if (xa_verbose) fprintf(stderr,"Reading DL File %s\n",filename);
	  result = DL_Read_File(filename,cur_file);
	  break;
/*PODTEMP
        case PFX_ANIM:
	  if (xa_verbose) fprintf(stderr,"Reading PFX File %s\n",filename);
	  result = PFX_Read_File(filename,cur_file);
	  break;
*/
        case SET_ANIM:
	  if (xa_verbose) fprintf(stderr,"Reading SET File %s\n",filename);
	  result = SET_Read_File(filename,cur_file);
	  break;
        case RLE_ANIM:
	  if (xa_verbose) fprintf(stderr,"Reading RLE File %s\n",filename);
	  result = RLE_Read_File(filename,cur_file);
	  break;
	case JFIF_ANIM:
	  if (xa_verbose) fprintf(stderr,"Reading JFIF File %s\n",filename);
	  result = JFIF_Read_File(filename,cur_file);
	  break;
	case MPG_ANIM:
	  if (xa_verbose) fprintf(stderr,"Reading MPG File %s\n",filename);
	  result = MPG_Read_File(filename,cur_file);
	  break;
        case AVI_ANIM:
	  if (xa_verbose) fprintf(stderr,"Reading AVI File %s\n",filename);
	  result = AVI_Read_File(filename,cur_file,audio_attempt);
	  break;
        case WAV_ANIM:
	  result = FALSE;
	  if (   (cur_file->prev_file == 0) 
              || (cur_file->prev_file == cur_file) ) break;
	  if (cur_file->prev_file->first_snd != 0) break;
	  if (xa_verbose) fprintf(stderr,"Reading WAV File %s\n",filename);
	  result = WAV_Read_File(filename,cur_file->prev_file,audio_attempt);
	  result = FALSE; /* always want to free up cur_file */
	  break;
        case QT_ANIM:
	  if (xa_verbose) fprintf(stderr,"Reading QT File %s\n",filename);
	  result = QT_Read_File(filename,cur_file,audio_attempt);
	  break;
        case NOFILE_ANIM:
	  fprintf(stderr,"File %s not found\n",filename);
	  result = FALSE;
	  break;
        default:
	  fprintf(stderr,"Unknown or unsupported animation type: %s\n",
								filename);
	  result = FALSE;
	  break;
      } 
      if (result == FALSE) cur_file = Return_Anim_Hdr(cur_file);
      else
      { ULONG tmpx,tmpy;
        /* Setup up anim header.  */
        cur_file->loop_num = xa_loop_each_flag;

        if (xa_pause_last == TRUE)
        { if ( (xa_anim_type == FLI_ANIM) && (cur_file->last_frame > 1))
					XA_Add_Pause(cur_file->last_frame-2);
          else XA_Add_Pause(cur_file->last_frame);
          xa_pause_last = FALSE;
        }
	cur_file->pause_lst = xa_pause_hdr;
	xa_pause_hdr = 0;

	xa_imagex = cur_file->imagex;
	xa_imagey = cur_file->imagey;
	if (xa_imagex > xa_max_imagex) xa_max_imagex = xa_imagex;
	if (xa_imagey > xa_max_imagey) xa_max_imagey = xa_imagey;

	if (cur_file->max_fvid_size > xa_vidcodec_maxsize)
			xa_vidcodec_maxsize = cur_file->max_fvid_size;
	if (cur_file->max_faud_size > xa_audcodec_maxsize)
			xa_audcodec_maxsize = cur_file->max_faud_size;

	/* Handle Buffer Scaling Here */
	if ( (xa_buff_x != 0) && (xa_buff_y != 0) )
		{tmpx = xa_buff_x; tmpy = xa_buff_y;}
	else if (xa_buff_x != 0) /* if just x, then adjust y */
		{tmpx = xa_buff_x; tmpy = (xa_imagey * xa_buff_x) / xa_imagex;}
	else if (xa_buff_y != 0) /* if just y, then adjust x */
		{tmpy = xa_buff_y; tmpx = (xa_imagex * xa_buff_y) / xa_imagey;}
	else
	{
		/* handle any scaling */
	  tmpx = (ULONG)((float)(xa_imagex) * xa_bscalex);
	  if (tmpx == 0) tmpx = xa_imagex;
	  tmpy = (ULONG)((float)(xa_imagey) * xa_bscaley);
	  if (tmpy == 0) tmpy = xa_imagey;
	}
	cur_file->buffx = tmpx;
	cur_file->buffy = tmpy;

		/* Handle Display Scaling Here */
	if ( (xa_disp_x != 0) && (xa_disp_y != 0) ) 
		{tmpx = xa_disp_x; tmpy = xa_disp_y;}
	else if (xa_disp_x != 0) /* if just x, then adjust y */
		{tmpx = xa_disp_x; tmpy = (xa_imagey * xa_disp_x) / xa_imagex;}
	else if (xa_disp_y != 0) /* if just y, then adjust x */
		{tmpy = xa_disp_y; tmpx = (xa_imagex * xa_disp_y) / xa_imagey;}
	else
	{
		/* handle any scaling */
	  tmpx = (ULONG)((float)(xa_imagex) * xa_scalex);
	  if (tmpx == 0) tmpx = xa_imagex;
	  tmpy = (ULONG)((float)(xa_imagey) * xa_scaley);
	  if (tmpy == 0) tmpy = xa_imagey;
	}
	/* handle any IFF laced images */
	if ( (xa_allow_lace==TRUE) && (cur_file->anim_flags & ANIM_LACE))
		tmpy >>= 1;
	else	cur_file->anim_flags &= ~ANIM_LACE;
	cur_file->dispx = tmpx;
	cur_file->dispy = tmpy;
	if (tmpx > xa_max_disp_x) xa_max_disp_x = tmpx;
	if (tmpy > xa_max_disp_y) xa_max_disp_y = tmpy;

	if ((cmap_dither_type == CMAP_DITHER_FLOYD) && (xa_buffer_flag==FALSE))
			cur_file->anim_flags |= ANIM_3RD_BUF;
	xa_merged_anim_flags |= cur_file->anim_flags;

	/* NOTE: removed fade, remember to readd eventually */

	if (xa_time_flag == TRUE)
	{
	  LONG time_int;
	  xa_time_end = XA_Time_Read();
	  time_int = xa_time_end - xa_time_start;
	  fprintf(stderr,"time = %ld\n",time_int);
	  xa_time_start = XA_Time_Read();
	}
      } /* valid animation file */
    } /* end of read in file */
 } /* end of loopin through arguments */

 /* No anims listed
  */
 if (first_file == 0) Usage_Quick();

 /* Set up X11 Display
  */

 if (xa_noresize_flag==TRUE) X11_Setup_Window(xa_max_disp_x,xa_max_disp_y,
		xa_max_disp_x,xa_max_disp_y);
 else X11_Setup_Window(xa_max_disp_x,xa_max_disp_y,
		first_file->dispx, first_file->dispy);
 if (x11_display_type == XA_MONOCHROME) xa_optimize_flag = FALSE;
 
 /* color map manipulation */
 CMAP_Manipulate_CHDRS();

 /* Kludge for some X11 displays */
 if (x11_kludge_1 == TRUE) CMAP_Expand_Maps();

 xa_time_start = XA_Time_Read();
 cur_file = first_file;
 while(cur_file)
 {
   ACT_Make_Images(cur_file->acts);
   cur_file = cur_file->next_file;
   if (cur_file == first_file) cur_file = 0;
 }
 if (xa_time_flag == TRUE)
 {
   LONG time_int;
   xa_time_end = XA_Time_Read();
   time_int = xa_time_end - xa_time_start;
   fprintf(stderr,"ACT_Make_Images: time = %ld\n",time_int);
   xa_time_start = XA_Time_Read();
 }

 /* Set Audio Ports */
  if (xa_audio_present == XA_AUDIO_OK)
  {
#ifdef XA_SPARC_AUDIO
    if (xa_audio_port) XA_Set_Output_Port(xa_audio_port);
#else
    if (xa_audio_port & XA_AUDIO_PORT_INT) XA_Speaker_Tog(1);
    else XA_Speaker_Tog(0);
    if (XA_Headphone_Tog == XA_LineOut_Tog) /* if same */
    {
      if (   (xa_audio_port & XA_AUDIO_PORT_HEAD) 
	  || (xa_audio_port & XA_AUDIO_PORT_EXT) ) XA_Headphone_Tog(1);
      else XA_Headphone_Tog(0);
    }
    else /* else treat differently */
    {
      if (xa_audio_port & XA_AUDIO_PORT_HEAD) XA_Headphone_Tog(1);
      else XA_Headphone_Tog(0);
      if (xa_audio_port & XA_AUDIO_PORT_EXT) XA_LineOut_Tog(1);
      else XA_LineOut_Tog(0);
    }
#endif
  }

 /* Start off Animation.
  */
  ShowAnimation();
 /* Map window and then Wait for user input.
  */
  X11_Map_Window();
#ifdef XA_REMOTE_CONTROL
  XA_Create_Remote(theWG,xa_remote_flag);
#else
  xa_remote_ready = TRUE;
#endif
  xanim_events();

  XSync(theDisp,False);


 /* Self-Explanatory
  */
  TheEnd();
  return(0);
}


/*
 * ShowAnimation allocates and sets up required image buffers and
 * initializes animation variables.
 * It then kicks off the animation.
 */
void ShowAnimation()
{ ULONG buf_size;
  /* POD TEMPORARY PARTIAL KLUDGE UNTIL REAL FIX IS IMPLEMENTED */
  if (xa_max_imagex & 0x03) shm = 0;

  xa_max_image_size = xa_max_imagex * xa_max_imagey;
  xa_max_disp_size = xa_max_disp_x * xa_max_disp_y;

  buf_size = xa_max_image_size * x11_bytes_pixel;

  if (   (xa_merged_anim_flags & ANIM_SNG_BUF)
      || (xa_merged_anim_flags & ANIM_DBL_BUF)
     )
  { 

#ifdef XSHM
    if (shm)
    {
      im0_Image = XShmCreateImage(theDisp,theVisual,x11_depth,
                ZPixmap, 0, &im0_shminfo, xa_max_imagex, xa_max_imagey );
      if (im0_Image == 0) 
		{ shm = 0; x11_error_possible = 0; goto XA_SHM_FAILURE; }
      im0_shminfo.shmid = shmget(IPC_PRIVATE, buf_size, (IPC_CREAT | 0777 ));
      if (im0_shminfo.shmid < 0) 
      { perror("shmget"); 
	shm = 0; x11_error_possible = 0;
        if (im0_Image) 
	{ im0_Image->data = 0;
	  XDestroyImage(im0_Image); im0_Image = 0;
	}
	goto XA_SHM_FAILURE;
      }
      im0_shminfo.shmaddr = (char *) shmat(im0_shminfo.shmid, 0, 0);
      XSync(theDisp, False);
      x11_error_possible = 1;  /* if remote display. following will fail */
      XShmAttach(theDisp, &im0_shminfo);
      XSync(theDisp, False);
      if (x11_error_possible == -1) /* above failed - back off */
      {
	shm = 0;
	x11_error_possible = 0;
        if (xa_verbose) fprintf(stderr,"SHM Attach failed: backing off\n");
	shmdt(im0_shminfo.shmaddr);	im0_shminfo.shmaddr = 0;
	im0_Image->data = 0;
        if (im0_Image) { XDestroyImage(im0_Image); im0_Image = 0; }
	goto XA_SHM_FAILURE;
      }
      else
      {
	shmctl(im0_shminfo.shmid, IPC_RMID, 0 );
	im0_Image->data = im_buff0 = im0_shminfo.shmaddr;
	sh_Image = im0_Image;
        if (xa_verbose) fprintf(stderr, "Using Shared Memory Extension\n");

DEBUG_LEVEL2 fprintf(stderr,"IM_BUFF0 = %lx im0_IMAGE = %lx\n",im_buff0,im0_Image);
      }
    } else
#endif /* XSHM */
    {
XA_SHM_FAILURE:
      im_buff0 = (char *) malloc(buf_size);
      if (im_buff0 == 0) TheEnd1("ShowAnimation: im_buff0 malloc err0");
    }
    memset(im_buff0,0x00,(xa_max_image_size * x11_bytes_pixel) );  
  }
  if (xa_merged_anim_flags & ANIM_DBL_BUF)
  {
#ifdef XSHM
    if (shm)
    {
      im1_Image = XShmCreateImage(theDisp,theVisual,x11_depth,
                ZPixmap, 0, &im1_shminfo, xa_max_imagex, xa_max_imagey );
      im1_shminfo.shmid = shmget(IPC_PRIVATE, buf_size, (IPC_CREAT | 0777 ));
      if (im1_shminfo.shmid < 0) { perror("shmget"); exit(0); }
      im1_shminfo.shmaddr = (char *) shmat(im1_shminfo.shmid, 0, 0);
      XSync(theDisp, False);
      XShmAttach(theDisp, &im1_shminfo);
      XSync(theDisp, False);
      shmctl(im1_shminfo.shmid, IPC_RMID, 0 );
      im1_Image->data = im_buff1 = im1_shminfo.shmaddr;
DEBUG_LEVEL2 fprintf(stderr,"IM_BUFF1 = %lx im1_IMAGE = %lx\n",im_buff1,im1_Image);
    } else
#endif /* XSHM */
    {
      im_buff1 = (char *) malloc(buf_size);
      if (im_buff1 == False) TheEnd1("ShowAnimation: im_buff1 malloc err");
    }
  }
  if (xa_merged_anim_flags & ANIM_3RD_BUF)
  {
#ifdef XSHM
    if (shm)
    {
      im2_Image = XShmCreateImage(theDisp,theVisual,x11_depth,
                ZPixmap, 0, &im2_shminfo, xa_max_imagex, xa_max_imagey );
      im2_shminfo.shmid = shmget(IPC_PRIVATE, buf_size, (IPC_CREAT | 0777 ));
      if (im2_shminfo.shmid < 0) { perror("shmget"); exit(0); }
      im2_shminfo.shmaddr = (char *) shmat(im2_shminfo.shmid, 0, 0);
      XSync(theDisp, False);
      XShmAttach(theDisp, &im2_shminfo);
      XSync(theDisp, False);
      shmctl(im2_shminfo.shmid, IPC_RMID, 0 );
      im2_Image->data = im_buff2 = im2_shminfo.shmaddr;
DEBUG_LEVEL2 fprintf(stderr,"IM_BUFF2 = %lx im2_IMAGE = %lx\n",im_buff2,im2_Image);
    } else
#endif /* XSHM */
    {
      im_buff2 = (char *) malloc(buf_size);
      if (im_buff2 == 0) TheEnd1("ShowAnimation: im_buff2 malloc err1");
    }
  }
  if (x11_pack_flag == TRUE)
  {
    ULONG tsize;
    ULONG pbb = (8 / x11_bits_per_pixel);
    tsize = (xa_max_imagex + pbb - 1) / pbb;
    im_buff3 = (char *) malloc(xa_max_disp_y * tsize);
    if (im_buff3 == 0) TheEnd1("ShowAnimation: im_buff3 malloc err1");
  }
  xa_pic = im_buff0;
  cur_file = first_file;
  cur_floop = 0;
  cur_frame = -1;  /* since we step first */
  xa_audio_start = 0;
  xa_time_reset = 1;
  file_is_started = FALSE;
  xa_cycle_cnt = 0;
  xa_now_cycling = FALSE;

  xa_anim_ready = TRUE; /* allow expose event to start animation */
}

/*
 * This is the heart of this program. It does each action and then calls
 * itself with a timeout via the XtAppAddTimeOut call.
 */
void ShowAction(dptr,id)
XtPointer dptr;
XtIntervalId *id;
{ ULONG command = (ULONG)dptr;
  LONG t_time_delta = 0;
  LONG snd_speed_now_ok = FALSE;

/* just for benchmarking, currently isn't(and shouldn't be) used */

ShowAction_Loop:

 DEBUG_LEVEL2 fprintf(stderr,"SHOWACTION\n");

 if (command == XA_SHOW_NORM)
 {
  switch (xa_anim_status)
  {
    case XA_BEGINNING:  
	if ((xa_anim_ready == FALSE) || (xa_remote_ready == FALSE))
	{ /* try again 10ms later */
	  XtAppAddTimeOut(theContext,10, (XtTimerCallbackProc)ShowAction,
						(XtPointer)(XA_SHOW_NORM));
	  return;
	}
	xa_anim_status = XA_RUN_NEXT; /* drop through */
    case XA_STEP_NEXT: 
    case XA_RUN_NEXT:   
	Step_Action_Next();
	break;
    case XA_STEP_PREV:
    case XA_RUN_PREV:   
	Step_Action_Prev();
	break;
    case XA_ISTP_NEXT: 
#ifdef XA_AUDIO
	XA_Audio_Off(0);	xa_audio_start = 0;
#endif
	Step_Frame_Next();
	break;
    case XA_ISTP_PREV: 
#ifdef XA_AUDIO
	XA_Audio_Off(0);	xa_audio_start = 0;
#endif
	Step_Frame_Prev();
	break;
    case XA_FILE_NEXT:
#ifdef XA_AUDIO
	XA_Audio_Off(0);	xa_audio_start = 0;
#endif
	Step_File_Next();
	xa_anim_status = XA_STEP_NEXT;
	break;
    case XA_FILE_PREV:
#ifdef XA_AUDIO
	XA_Audio_Off(0);	xa_audio_start = 0;
#endif
	Step_File_Prev();
	xa_anim_status = XA_STEP_PREV;
	break;
    case XA_UNSTARTED:  
    case XA_STOP_NEXT:  
    case XA_STOP_PREV:  
#ifdef XA_AUDIO
	XA_Audio_Off(0);	xa_audio_start = 0;
#endif
	if (xa_title_flag != XA_TITLE_NONE)
	{
	  sprintf(xa_title,"XAnim: %s %ld",cur_file->name,cur_frame);
	  XStoreName(theDisp,mainW,xa_title);
	}
	xa_anim_holdoff = FALSE;
	xa_old_status = xa_anim_status;
	return;
	break;
    default:
#ifdef XA_AUDIO
	XA_Audio_Off(0);	xa_audio_start = 0;
#endif
	xa_anim_holdoff = FALSE;
	xa_old_status = xa_anim_status;
	return;
	break;
  } /* end of case */
 } /* end of command SHOW_NORM */

  DEBUG_LEVEL1 fprintf(stderr,"frame = %ld\n",cur_frame);

 /* 1st throught this particular file.
  * Resize if necessary and init required variables.
  */
  if (file_is_started == FALSE)
  {
#ifdef XA_AUDIO
    if (xa_audio_status == XA_AUDIO_STARTED)
    {
      XtAppAddTimeOut(theContext, 50, (XtTimerCallbackProc)XA_Audio_Wait,
							(XtPointer)(NULL) );
      return;
    }
#endif
     /* If previous anim is still cycling, wait for it to stop */
    if (xa_now_cycling == TRUE)
    { /*PODNOTE: This check might want to be before above switch */
      xa_anim_flags &= ~(ANIM_CYCLE);
      XtAppAddTimeOut(theContext, 50, (XtTimerCallbackProc)XA_Cycle_Wait,
							(XtPointer)(NULL) );
      return; 
    }

    file_is_started = TRUE;
    xa_anim_flags = cur_file->anim_flags;
    if (xa_anim_flags & ANIM_PIXMAP) xa_pixmap_flag = TRUE;
    else xa_pixmap_flag = FALSE;

    /* If anim consists of only full frames, then allow serious skipping */
    xa_serious_skip_flag =(xa_anim_flags & ANIM_FULL_IM)?(xa_skip_flag):(FALSE);

      /* user has not resized window yet */
    if (     (xa_allow_resizing == FALSE)
         || ( (xa_allow_resizing == TRUE) && (x11_window_x==0) )
       )
    {   /* anim if allowed to cause window resizing */
      if (xa_noresize_flag == FALSE)
      {
        if ((xa_disp_x != cur_file->dispx) || (xa_disp_y != cur_file->dispy)) 
        {
DEBUG_LEVEL1 fprintf(stderr,"resizing to <%ld,%ld>\n",cur_file->dispx,cur_file->dispy);
	   XResizeWindow(theDisp,mainW,cur_file->dispx,cur_file->dispy);
	   x11_window_x = cur_file->dispx; x11_window_y = cur_file->dispy;
    	   XSync(theDisp,False);
    	   /* XFlush(theDisp);*/
        }
      }
      else /* fixed window size */
      {
        if (xa_disp_x > cur_file->dispx)
		XClearArea(theDisp,mainW,cur_file->dispx,0, 0,0,FALSE);
        if (xa_disp_y > cur_file->dispy)
		XClearArea(theDisp,mainW,0,cur_file->dispy, 0,0,FALSE);
      }
    }
    /* Do Title if set */
    switch(xa_title_flag)
    { 
      case XA_TITLE_FILE:
	sprintf(xa_title,"XAnim: %s",cur_file->name);
	break;
      case XA_TITLE_FRAME:
	if (cur_frame >= 0)
	     sprintf(xa_title,"XAnim: %s %ld",cur_file->name,cur_frame);
	else sprintf(xa_title,"XAnim: %s",cur_file->name);
	break;
      default:
	sprintf(xa_title,"XAnim");
	break;
    }
    XStoreName(theDisp,mainW,xa_title);
    /* Initialize variables
     */
#ifdef XSHM
    if (xa_anim_flags & ANIM_SNG_BUF) sh_Image = im0_Image;
#endif
    if (xa_anim_flags & ANIM_SNG_BUF) xa_pic = im_buff0; 
    xa_imagex = cur_file->imagex;
    xa_imagey = cur_file->imagey;
    xa_disp_x = cur_file->dispx;
    xa_disp_y = cur_file->dispy;
    xa_buff_x = cur_file->buffx;
    xa_buff_y = cur_file->buffy;
    xa_imaged = cur_file->imaged;
    xa_cmap_size = cur_file->imagec;
    xa_image_size = xa_imagex * xa_imagey;
    X11_Init_Image_Struct(theImage,xa_disp_x,xa_disp_y);
    if (xa_anim_flags & ANIM_USE_FILE)
    { /* Close old video file and open new one */
      if (xa_vid_fd>=0) { close(xa_vid_fd); xa_vid_fd = -1; }
      if ( (xa_vid_fd=open(cur_file->fname,O_RDONLY,NULL)) < 0)
      { 
        fprintf(stderr,"Open file %s for video err\n",cur_file->fname); 
        TheEnd();
      }
      /* Close old audio file and open new one */
      if (xa_aud_fd>=0) { close(xa_aud_fd); xa_aud_fd = -1; }
      if ( (xa_aud_fd=open(cur_file->fname,O_RDONLY,NULL)) < 0)
      { 
        fprintf(stderr,"Open file %s for audio err\n",cur_file->fname); 
        TheEnd();
      }
      if ((cur_file->max_fvid_size) && (xa_vidcodec_buf==0))
      {
	xa_vidcodec_buf = (UBYTE *)malloc( xa_vidcodec_maxsize );
	if (xa_vidcodec_buf==0) TheEnd1("XAnim: malloc vidcodec_buf err");
      }
      if ((cur_file->max_faud_size) && (xa_audcodec_buf==0))
      {
	xa_audcodec_buf = (UBYTE *)malloc( xa_audcodec_maxsize );
	if (xa_audcodec_buf==0) TheEnd1("XAnim: malloc audcodec_buf err");
      }
    }
    xa_pause_hdr = cur_file->pause_lst;
    xa_snd_cur = cur_file->first_snd;
    xa_skip_cnt = xa_skip_video = 0;
    /*** Call any Initializing routines needed by Audio/Video Codecs */
    if (cur_file->init_vid) cur_file->init_vid();
    if (cur_file->init_aud) cur_file->init_aud();
    /*** Initialize audio timing ****/
    xa_time_start = XA_Time_Read();

    /* Is Audio Valid for this animations */
    if ( (xa_audio_present==XA_AUDIO_OK) && (xa_snd_cur) )
    { 
      xa_audio_enable = TRUE;
      xa_audio_status = XA_AUDIO_STOPPED;
      XA_Audio_Init_Snd(xa_snd_cur);
      xa_time_audio = 0;
      if (xa_snd_cur->fpos >= 0)
      {
	xa_snd_cur->snd = xa_audcodec_buf;
	XA_Read_Audio_Delta(xa_aud_fd,xa_snd_cur->fpos,
			xa_snd_cur->tot_bytes,xa_audcodec_buf);
      }
    } 
    else
    {
      xa_time_audio = -1;
      xa_audio_enable = FALSE;
      xa_audio_status = XA_AUDIO_NICHTDA;
    }
    xa_time_video = 0;
    xa_av_time_off = XA_Time_Read();  /* for initial control frames if any */

    xa_old_status = XA_STOP_NEXT;
  } /* end of file is not started */

 
 /* OK. A quick sanity check and then we
  * initialize act and inc frame time and go to it.
  */
 if ( (act = cur_file->frame_lst[cur_frame].act) != 0)
 {
    if (cur_file->frame_lst[cur_frame].zztime >= 0)
	xa_time_video = cur_file->frame_lst[cur_frame].zztime;
    if (!(xa_anim_status & XA_NEXT_MASK)) /* backwards */
    {
      xa_time_video += cur_file->frame_lst[cur_frame].time_dur;
      xa_time_video = cur_file->total_time - xa_time_video;
    }
    /* Adjust Timing if there's been a change in the speed scale */
    if (xa_speed_change) 
    {
      xa_speed_change = 0;
      XA_Reset_Speed_Time(xa_time_video,xa_speed_scale);
#ifdef XA_AUDIO
      if (xa_speed_scale == XA_SPEED_NORM) snd_speed_now_ok = TRUE;
      else
      {
         snd_speed_now_ok = FALSE;
         if (xa_audio_status == XA_AUDIO_STARTED) 
			{ XA_Audio_Off(1); xa_audio_start = 0; }
      }
#endif
    }

#ifdef XA_AUDIO
    if (xa_audio_status == XA_AUDIO_STARTED) 
    {
      if (xa_time_video == 0)  /* Hitting 1st frame */
      { /* wait til audio is finished */
#ifdef XA_AUDIO
        XtAppAddTimeOut(theContext, 50, (XtTimerCallbackProc)XA_Audio_Wait,
							(XtPointer)(NULL) );
        return;
#endif
      }
      else if (    (xa_anim_status != XA_RUN_NEXT)
		|| (xa_speed_scale != XA_SPEED_NORM)  ) 
				{ XA_Audio_Off(0); xa_audio_start = 0; }
    }
    else if (xa_audio_status == XA_AUDIO_STOPPED)
    {
      if (    (xa_anim_status == XA_RUN_NEXT) 
	  &&  (xa_speed_scale == XA_SPEED_NORM)
          &&  ( (xa_old_status != XA_RUN_NEXT) || (snd_speed_now_ok==TRUE)) )
      {
	xa_audio_start = 1;
	xa_time_reset = 1;
      }
      else if (   (   (xa_old_status != XA_RUN_PREV)
                   && (xa_anim_status == XA_RUN_PREV))
	       || (xa_time_video == 0)  )
      {
	xa_time_reset = 1;
        xa_time_audio = -1;
      }
    }
    else  /* DROP THROUGH CASE WHERE NO AUDIO IS PRESENT */
#endif
    /**** Readjust Timing if Run is New */
    if (   ((xa_old_status != XA_RUN_PREV)&&(xa_anim_status==XA_RUN_PREV))
        || ((xa_old_status != XA_RUN_NEXT)&&(xa_anim_status==XA_RUN_NEXT))
        || (xa_time_video == 0) )
    { 
       xa_time_reset = 1;
       xa_time_audio = -1;
    }

    
    if ( (xa_allow_resizing == TRUE) && (x11_window_x != 0) )
        { xa_disp_x = x11_window_x; xa_disp_y = x11_window_y; }

    if ((xa_disp_x != xa_buff_x) || (xa_disp_y != xa_buff_y))
	 xa_need_to_scale_b = 1;
    else xa_need_to_scale_b = 0;
    if ((xa_disp_x != xa_imagex) || (xa_disp_y != xa_imagey))
	 xa_need_to_scale_u = 1;
    else xa_need_to_scale_u = 0;

    /* lesdoit */
    switch(act->type)
    {
       /* 
        * NOP and DELAY don't change anything but can have timing info
        * that might prove useful. ie dramatic pauses :^)
        */
     case ACT_NOP:      
                        DEBUG_LEVEL2 fprintf(stderr,"ACT_NOP:\n");
                        break;
     case ACT_DELAY:    
                        DEBUG_LEVEL2 fprintf(stderr,"ACT_DELAY:\n");
                        break;
       /* 
        * Change Color Map.
        */
     case ACT_CMAP:     
        if (   (cmap_play_nice == FALSE) 
	    && (!(x11_display_type & XA_X11_STATIC)) )
	{ ColorReg *tcmap;
	  LONG j;
	  ACT_CMAP_HDR *cmap_hdr;

	  cmap_hdr	= (ACT_CMAP_HDR *)act->data;
	  xa_cmap_size	= cmap_hdr->csize;
	  if (xa_cmap_size > x11_cmap_size) xa_cmap_size = x11_cmap_size;
	  xa_cmap_off	= cmap_hdr->coff;
	  tcmap		= (ColorReg *)cmap_hdr->data;
          for(j=0; j<xa_cmap_size;j++)
	  {
		xa_cmap[j].red   = tcmap[j].red;
		xa_cmap[j].green = tcmap[j].green;
		xa_cmap[j].blue  = tcmap[j].blue;
		xa_cmap[j].gray  = tcmap[j].gray;
	  }
/* POD TEMP share routine whith CHDR install */
  if (x11_cmap_flag == TRUE)
  {
    DEBUG_LEVEL2 fprintf(stderr,"CMAP: size=%ld off=%ld\n",
                xa_cmap_size,xa_cmap_off);
    if (x11_display_type & XA_X11_GRAY)
    {
      for(j=0;j<xa_cmap_size;j++)
      {
        defs[j].pixel = xa_cmap_off + j;
        defs[j].red   = xa_cmap[j].gray;
        defs[j].green = xa_cmap[j].gray;
        defs[j].blue  = xa_cmap[j].gray;
        defs[j].flags = DoRed | DoGreen | DoBlue;
      }
    }
    else
    {
      for(j=0; j<xa_cmap_size;j++)
      {
        defs[j].pixel = xa_cmap_off + j;
        defs[j].red   = xa_cmap[j].red;
        defs[j].green = xa_cmap[j].green;
        defs[j].blue  = xa_cmap[j].blue;
        defs[j].flags = DoRed | DoGreen | DoBlue;
        DEBUG_LEVEL3 fprintf(stderr," %ld) %lx %lx %lx <%ld>\n",
          j,xa_cmap[j].red,xa_cmap[j].green,xa_cmap[j].blue,xa_cmap[j].gray);

      }
    }
    XStoreColors(theDisp,theCmap,defs,xa_cmap_size);
    if (xa_audio_enable != TRUE) XSync(theDisp,False);
  }

	
	}
	break;
       /* 
        * Lower Color Intensity by 1/16.
        */
     case ACT_FADE:     
                        {
                         LONG j;

                         DEBUG_LEVEL2 fprintf(stderr,"ACT_FADE:\n");
                         if ( (x11_display_type & XA_X11_CMAP) &&
                              (x11_cmap_flag    == TRUE) )
                         {
                           for(j=0;j<xa_cmap_size;j++)
                           {
                             if (xa_cmap[j].red   <= 16) xa_cmap[j].red   = 0;
                             else xa_cmap[j].red   -= 16;
                             if (xa_cmap[j].green <= 16) xa_cmap[j].green = 0;
                             else xa_cmap[j].green -= 16;
                             if (xa_cmap[j].blue  <= 16) xa_cmap[j].blue  = 0;
                             else xa_cmap[j].blue  -= 16;

                             defs[j].pixel = j;
                             defs[j].red   = xa_cmap[j].red   << 8;
                             defs[j].green = xa_cmap[j].green << 8;
                             defs[j].blue  = xa_cmap[j].blue  << 8;
                             defs[j].flags = DoRed | DoGreen | DoBlue;
                           }
                           XStoreColors(theDisp,theCmap,defs,xa_cmap_size);
                           XFlush(theDisp);
                         }
                        }
                        break;
     case ACT_APTR:   
	{ ACT_APTR_HDR *aptr_hdr = (ACT_APTR_HDR *)act->data;
    	  if (aptr_hdr->act->type == ACT_IMAGE)
		XA_SHOW_IMAGE(aptr_hdr->act,aptr_hdr->xpos,aptr_hdr->ypos,
				aptr_hdr->xsize,aptr_hdr->ysize,1);
    	  else if (aptr_hdr->act->type == ACT_PIXMAP)
		XA_SHOW_PIXMAP(aptr_hdr->act,aptr_hdr->xpos,aptr_hdr->ypos,
				aptr_hdr->xsize,aptr_hdr->ysize,1);
	  else
	  {
  	    fprintf(stderr,"ACT_APTR: error invalid action %lx\n",act->type);
	  }
	}
	break;

     case ACT_IMAGE:   
	XA_SHOW_IMAGE(act,0,0,0,0,0);
	break;

     case ACT_PIXMAP:   
	XA_SHOW_PIXMAP(act,0,0,0,0,0);
	break;

     case ACT_IMAGES:
	XA_SHOW_IMAGES(act);
	break;

     case ACT_PIXMAPS:
	XA_SHOW_PIXMAPS(act);
	break;

       /* 
        * Act upon IFF Color Cycling chunk.
        */
     case ACT_CYCLE:
{
/* if there is a new_chdr, install it, not the old one */
/*
 XA_CHDR *the_chdr;
if (act->chdr->new_chdr) the_chdr = act->chdr->new_chdr;
else the_chdr = act->chdr;
*/

        if (   (cmap_play_nice == FALSE) 
	    && (x11_display_type & XA_X11_CMAP)
	    && (xa_anim_flags & ANIM_CYCLE) )
	{
	  ACT_CYCLE_HDR *act_cycle;
	  act_cycle = (ACT_CYCLE_HDR *)act->data;

	  DEBUG_LEVEL2 fprintf(stderr,"ACT_CYCLE:\n");
	  if ( !(act_cycle->flags & ACT_CYCLE_STARTED) )
	  {
	      if ( (act->chdr != 0) && (act->chdr != xa_chdr_now) )
			XA_Install_CMAP(act->chdr);
	      xa_cycle_cnt++;
	      act_cycle->flags |= ACT_CYCLE_STARTED;
	      xa_now_cycling = TRUE;
	      XtAppAddTimeOut(theContext,(int)(act_cycle->rate), 
		(XtTimerCallbackProc)XA_Cycle_It, (XtPointer)(act_cycle));
	  }
	}
} /*POD*/
	break;

     case ACT_DELTA:        
	XA_SHOW_DELTA(act);
	break;

     default:           
                        fprintf(stderr,"Unknown not supported %lx\n",act->type);
    } /* end of switch of action type */
 } /* end of action valid */
 else
 {
   fprintf(stderr,"PODNOTE: CONTROL FRAME MADE IT THROUGH\n");
 }

 /* Reset time if needed */
 if (xa_time_reset)
 { XA_Reset_AV_Time(xa_time_video,xa_speed_scale);
   xa_time_reset = 0;
 }
 /* Start Audio is needed */
 if (xa_audio_start)
 {
   XA_Audio_On();
   xa_audio_start = 0;
 }

 if (xa_pause_hdr)
 {
    if ( (cur_frame==xa_pause_hdr->frame) && (xa_anim_status & XA_RUN_MASK))
    {
      xa_pause_hdr = xa_pause_hdr->next;
      XA_Audio_Off(0); xa_audio_start = 0;
#ifdef XA_REMOTE_DEFS
      XA_Remote_Pause();
#endif
      goto XA_PAUSE_ENTRY_POINT; /* reuse single step code */
    }
 }

 if (xa_anim_status & XA_STEP_MASK) /* Single step if in that mode */
 {
   XA_PAUSE_ENTRY_POINT:
   if ( (xa_no_disp == FALSE) & (xa_title_flag != XA_TITLE_NONE) )
   {
     sprintf(xa_title,"XAnim: %s %ld",cur_file->name,cur_frame);
     XStoreName(theDisp,mainW,xa_title);
   }
   xa_anim_status &= XA_CLEAR_MASK; /* preserve direction and stop */
   xa_anim_status |= XA_STOP_MASK;
   xa_anim_holdoff = FALSE;
   xa_old_status = xa_anim_status;
   return;
 }

 if ( (xa_no_disp == FALSE) & (xa_title_flag == XA_TITLE_FRAME) )
 {
   sprintf(xa_title,"XAnim: %s %ld",cur_file->name,cur_frame);
   XStoreName(theDisp,mainW,xa_title);
 }

  /* Harry, what time is it? and how much left? default to 1 ms */
 xa_time_video += cur_file->frame_lst[cur_frame].time_dur;
 xa_time_video = XA_SPEED_ADJ(xa_time_video,xa_speed_scale);
 xa_time_now = XA_Read_AV_Time();

#ifdef XA_AUDIO
 if (xa_time_audio >= 0)
 { 
   DEBUG_LEVEL1 fprintf(stderr,"cav %ld %ld %ld d: %ld %ld\n",
	xa_time_now,xa_time_audio,xa_time_video,
	cur_file->frame_lst[cur_frame].zztime,
	cur_file->frame_lst[cur_frame].time_dur);
#ifdef NEVERNEVER
   if (xa_time_video > xa_time_audio) /* this is bad, but for now we kludge*/
   {
     t_time_delta = xa_time_video - xa_time_audio;
   }
   else 
#endif
       if (xa_time_video > xa_time_now) /* video ahead, slow down video */
   { 
     t_time_delta = xa_time_video - xa_time_now;
   }
   else	
   { LONG diff = xa_time_now - xa_time_video;
     LONG f_time = cur_file->frame_lst[cur_frame].time_dur;
     if (diff > f_time)
     {
	if (f_time > 0) xa_skip_video += diff / f_time;
	else xa_skip_video++;
DEBUG_LEVEL1 fprintf(stderr,"vid behind dif %ld skp %ld\n",diff,xa_skip_video);
     }
     else xa_skip_video = 0;
     t_time_delta = 1;      /* audio ahead  speed up video */
   }
 }
 else
#endif
 {
   xa_skip_video = 0;
   DEBUG_LEVEL1 fprintf(stderr,"cv %ld %ld  d: %ld %ld scal %lx\n",
	xa_time_now,xa_time_video,cur_file->frame_lst[cur_frame].zztime,
	cur_file->frame_lst[cur_frame].time_dur,xa_speed_scale);
   if (xa_time_video > xa_time_now)
		t_time_delta = xa_time_video - xa_time_now;
   else		t_time_delta = 1;
 }

 DEBUG_LEVEL1 fprintf(stderr,"t_time_delta %ld\n",t_time_delta);
 if ( !(xa_anim_status & XA_STOP_MASK) )
   XtAppAddTimeOut(theContext,t_time_delta,(XtTimerCallbackProc)ShowAction,
						  (XtPointer)(XA_SHOW_NORM));
 else xa_anim_holdoff = FALSE;
 xa_old_status = xa_anim_status;
}

/*
 *
 */
void Step_Action_Next()
{
  XA_FRAME *frame;

  cur_frame++; frame = &cur_file->frame_lst[cur_frame];
  do
  { ULONG jmp2end_flag = 0;
    if ( (frame->zztime == -1) && (frame->act != 0) ) /* check for loops */
    {
      XA_ACTION *lp_act = frame->act;
      if (lp_act->type == ACT_BEG_LP)
      {
        ACT_BEG_LP_HDR *beg_lp = (ACT_BEG_LP_HDR *)lp_act->data;
        beg_lp->cnt_var = beg_lp->count;
        cur_frame++; /* move on */
      }
      else if (lp_act->type == ACT_END_LP)
      {
        ACT_END_LP_HDR *end_lp = (ACT_END_LP_HDR *)lp_act->data;
        *end_lp->cnt_var = *end_lp->cnt_var - 1;
        if (*end_lp->cnt_var > 0) cur_frame = end_lp->begin_frame;
        else cur_frame++;
      }
      else if (lp_act->type == ACT_JMP2END)
      { 
	if (xa_pingpong_flag==FALSE)
	{
	  if ( (cur_floop+1) >= cur_file->loop_num) /* done with this file */
	  {
             if (first_file->next_file != first_file) /* more to go */
	     {
	       cur_frame = cur_file->last_frame + 1; /* jmp to end */
	     }
	     else if (xa_exit_flag == TRUE) TheEnd(); /* we're outta here */
	     else cur_frame++;  /* step to beg */
          }  else cur_frame++; /* step to beg */
        } else jmp2end_flag = 1;
      }
      frame = &cur_file->frame_lst[cur_frame];
    }

    if ( (frame->act == 0) /* Are we at the end of an anim? */
        || (jmp2end_flag) )
    {
      if (xa_pingpong_flag == TRUE)
      { jmp2end_flag = 0;
        xa_anim_status &= ~(XA_NEXT_MASK);  /* change dir to prev */
        cur_frame--; 
        Step_Action_Prev();
        return;
      }
      cur_frame = cur_file->loop_frame;

      cur_floop++;
      DEBUG_LEVEL1 fprintf(stderr,"  loop = %ld\n",cur_floop);

      /* Done looping animation. Move on to next file if present */
      if (   (cur_floop >= cur_file->loop_num)
	  || (xa_anim_status & XA_STEP_MASK)   ) /* or if single stepping */
      {
        cur_floop = 0;             /* Reset Loop Count */

	/* Are we on the last file and do we need to exit? */
	if (   (cur_file->next_file == first_file)
	    && (xa_exit_flag == TRUE) )   TheEnd(); /* later */

        /* This is a special case check.
         * If more that one file, reset file_is_started, otherwise
         * if we're only displaying 1 animation jump to the loop_frame
         * which has already been set up above.
	 * If cur_file has audio then we always restart.
         */
        if (   (first_file->next_file != first_file)
            || (cur_file->first_snd) )
        {
          file_is_started = FALSE;
          cur_file = cur_file->next_file;
          cur_frame = 0;
        }
        DEBUG_LEVEL1 fprintf(stderr,"  file = %ld\n",cur_file->file_num);
        if (xa_time_flag == TRUE) XA_Time_Check();
      } /* end done looping file */
    } /* end done with frames in file */
    frame = &cur_file->frame_lst[cur_frame];
  } while( (frame->zztime == -1) || (frame->act == 0) );
}

/*
 *
 */
void Step_Action_Prev()
{
  XA_FRAME *frame;
  cur_frame--; if (cur_frame < 0) goto XA_Step_Action_Prev_0;
  frame = &cur_file->frame_lst[cur_frame];

  do
  {
    if ( (frame->zztime == -1) && (frame->act != 0) ) /* check for loops */
    {
      XA_ACTION *lp_act = frame->act;
      if (lp_act->type == ACT_BEG_LP)
      {
        ACT_BEG_LP_HDR *beg_lp = (ACT_BEG_LP_HDR *)lp_act->data;
        beg_lp->cnt_var++;
        if (beg_lp->cnt_var < beg_lp->count) cur_frame = beg_lp->end_frame;
        else cur_frame--;
      }
      else if (lp_act->type == ACT_END_LP)
      {
        ACT_END_LP_HDR *end_lp = (ACT_END_LP_HDR *)lp_act->data;
        *end_lp->cnt_var = 0;
        cur_frame--;
      }
      else if (lp_act->type == ACT_JMP2END)
      { /* not valid in this direction so just skip over it */
        cur_frame--;
      }
      if (cur_frame < 0) goto XA_Step_Action_Prev_0;
      frame = &cur_file->frame_lst[cur_frame];
    }

    /* Are we at the beginning of an anim? */
    if (cur_frame < 0) goto XA_Step_Action_Prev_0;
    if (   (frame->act == 0) || (cur_frame < cur_file->loop_frame)
       )
    {
      XA_Step_Action_Prev_0:  /* skip indexing with -1 */

      if (xa_pingpong_flag == TRUE)
      {
        xa_anim_status |= XA_NEXT_MASK;  /* change dir to forward */
        cur_floop++;

	/* Are we on the last file and do we need to exit? */
	if (   (cur_file->next_file == first_file)
	    && (xa_exit_flag == TRUE) )   TheEnd(); /* later */

         /* do we move to next file? */
        if (  (first_file->next_file != first_file)  /* more than 1 file */
	    && (   (cur_floop >= cur_file->loop_num)
                || (xa_anim_status & XA_STEP_MASK)  ) )
        {
          cur_floop = 0;
          file_is_started = FALSE;
          cur_file = cur_file->next_file;
          cur_frame = 0;
          break;
        }

        if (cur_floop >= cur_file->loop_num) 
        {
          cur_floop = 0;
          if (xa_time_flag == TRUE) XA_Time_Check();
        }
        cur_frame++;
        Step_Action_Next();
        return;
      } /* end of pingpong stuff */

      cur_frame = cur_file->last_frame;
      cur_floop--;
      DEBUG_LEVEL1 fprintf(stderr,"  loop = %ld\n",cur_floop);
       /* Done looping animation. Move on to next file if present */
      if (   (cur_floop <= 0)
	  || (xa_anim_status & XA_STEP_MASK) ) /* or if single stepping */
      {
        cur_floop = cur_file->loop_num; /* Reset Loop Count */
 
        /* If more that one file, go to next file */
        if (first_file->next_file != first_file )
        {
          file_is_started = FALSE;
          cur_file = cur_file->prev_file;
          cur_frame = cur_file->last_frame;
          cur_floop = cur_file->loop_num; /* Reset Loop Count */
        }

        if (xa_time_flag == TRUE) XA_Time_Check();
        DEBUG_LEVEL1 fprintf(stderr,"  file = %ld\n",cur_file->file_num);
      } /* end done looping file */
    } /* end done with frames in file */
    frame = &cur_file->frame_lst[cur_frame];
  } while( (frame->zztime == -1) || (frame->act == 0) );
}


/*
 *
 */
void Step_Frame_Next()
{
  cur_frame++;
  do
  {
      /* skip over loops */
    if (   (cur_file->frame_lst[cur_frame].zztime == -1)
        && (cur_file->frame_lst[cur_frame].act != 0) ) cur_frame++;

      /* Are we at the end of an anim? */
    if (cur_file->frame_lst[cur_frame].act == 0)
    {
      cur_frame = cur_file->loop_frame;
    }
  } while(   (cur_file->frame_lst[cur_frame].zztime == -1)
          || (cur_file->frame_lst[cur_frame].act == 0)  );
}

/*
 *
 */
void Step_Frame_Prev()
{
  cur_frame--;

  do
  {
      /* skip over loops */
    if (cur_frame < 0) goto XA_Step_Frame_Prev_0;
    if (   (cur_file->frame_lst[cur_frame].zztime == -1)
        && (cur_file->frame_lst[cur_frame].act != 0) ) cur_frame--;

    /* Are we at the beginning of an anim? */
    if (cur_frame < 0) goto XA_Step_Frame_Prev_0;
    if (   (cur_file->frame_lst[cur_frame].act == 0)
        || (cur_frame < cur_file->loop_frame)        )
    {
      XA_Step_Frame_Prev_0: /* prevent indexing with -1 */
      cur_frame = cur_file->last_frame;
    }
  } while(   (cur_file->frame_lst[cur_frame].zztime == -1)
          || (cur_file->frame_lst[cur_frame].act == 0)  );
}

/*
 *
 */
void Step_File_Next()
{
  file_is_started = FALSE;
  cur_frame = 0;
  cur_file = cur_file->next_file;
  cur_floop = 0; /* used if things start up again */

  DEBUG_LEVEL1 fprintf(stderr,"  file = %ld\n",cur_file->file_num);
}

/*
 *
 */
void Step_File_Prev()
{
  file_is_started = FALSE;
  cur_frame = 0;
  cur_file = cur_file->prev_file;
  cur_floop = 0; /* used if things start up again */

  DEBUG_LEVEL1 fprintf(stderr,"  file = %ld\n",cur_file->file_num);
}


/*
 * Simple routine to find out the file type. Defaults to FLI.
 */
LONG Determine_Anim_Type(filename)
char *filename;
{ LONG ret;
 if ( Is_RLE_File(filename)==TRUE)	return(RLE_ANIM);
 if ( Is_IFF_File(filename)==TRUE)	return(IFF_ANIM); 
 if ( Is_GIF_File(filename)==TRUE)	return(GIF_ANIM); 
 if ( Is_TXT_File(filename)==TRUE)	return(TXT_ANIM); 
 if ( Is_FLI_File(filename)==TRUE)	return(FLI_ANIM); 
/*PODTEMP
 if ( Is_PFX_File(filename)==TRUE)	return(PFX_ANIM);
*/
 if ( Is_SET_File(filename)==TRUE)	return(SET_ANIM);
 if ( Is_JFIF_File(filename)==TRUE)     return(JFIF_ANIM);
 if ( Is_AVI_File(filename)==TRUE)	return(AVI_ANIM); 
 if ( Is_WAV_File(filename)==TRUE)	return(WAV_ANIM); 
 if ( (ret=Is_QT_File(filename))==TRUE) return(QT_ANIM);
 if ( Is_MPG_File(filename)==TRUE)	return(MPG_ANIM);
 if (ret == XA_NOFILE)			return(NOFILE_ANIM);
 if ( Is_DL_File(filename)==TRUE)	return(DL_ANIM);
 return(UNKNOWN_ANIM);
}

 
/*********** XA_Cycle_Wait *********************************************
 * This function waits until all the color cycling processes have halted.
 * Then it fires up ShowAction with the XA_SHOW_SKIP command to cause
 * ShowAction to skip the frame movement.
 ***********************************************************************/
void XA_Cycle_Wait(nothing, id)
char *nothing;
XtIntervalId *id;
{

  if (xa_cycle_cnt) /* wait until cycles are done */
  {
    XtAppAddTimeOut(theContext, 50, (XtTimerCallbackProc)XA_Cycle_Wait,
							(XtPointer)(NULL));
    return;
  }
  else /* then move on */
  {
    xa_now_cycling = FALSE;
    XtAppAddTimeOut(theContext, 1, (XtTimerCallbackProc)ShowAction, 
						(XtPointer)(XA_SHOW_SKIP));
  }
}

/*********** XA_Audio_Wait *********************************************
 * This function waits until the audio from the previous animation is
 * finished before starting up the next animation.
 * Then it fires up ShowAction with the XA_SHOW_SKIP command to cause
 * ShowAction to skip the frame movement.
 ***********************************************************************/
void XA_Audio_Wait(nothing, id)
char *nothing;
XtIntervalId *id;
{
DEBUG_LEVEL1 fprintf(stderr,"XA_Audio_Wait\n");
  if (xa_audio_status == XA_AUDIO_STARTED) /* continue waiting */
  {
    if (xa_anim_status & XA_RUN_MASK)
    {
      XtAppAddTimeOut(theContext, 50, (XtTimerCallbackProc)XA_Audio_Wait,
							(XtPointer)(NULL));
      return;
    }
    XA_Audio_Off(0);		xa_audio_start = 0;
  }
  XtAppAddTimeOut(theContext, 1, (XtTimerCallbackProc)ShowAction, 
						(XtPointer)(XA_SHOW_SKIP));
}

/*
 *
 */
void XA_Cycle_It(act_cycle, id)
ACT_CYCLE_HDR   *act_cycle;
XtIntervalId *id;
{
  ULONG i,*i_ptr,size,curpos;

  if (xa_anim_flags & ANIM_CYCLE) XtAppAddTimeOut(theContext,
		(int)(act_cycle->rate),(XtTimerCallbackProc)XA_Cycle_It,
						    (XtPointer)(act_cycle));
  else
  {
    xa_cycle_cnt--;
    act_cycle->flags &= ~ACT_CYCLE_STARTED;
    return;
  }

  size = act_cycle->size;
  curpos = act_cycle->curpos;

  /* increment or decrement curpos */
  if (act_cycle->flags & ACT_CYCLE_REVERSE) 
      curpos = (curpos)?(curpos - 1):(size - 1); 
  else
      curpos = (curpos >= (size - 1))?(0):(curpos + 1); 
  act_cycle->curpos = curpos;

  i_ptr = (ULONG *)act_cycle->data;
  for(i=0;i<size;i++)
  {
    ULONG j;

    j = i_ptr[i] - xa_cmap_off;
    defs[i].pixel = i_ptr[curpos];
    defs[i].flags = DoRed | DoGreen | DoBlue;
    if (x11_display_type & XA_X11_GRAY)
    {
      defs[i].red = defs[i].green = defs[i].blue = xa_cmap[j].gray;
    }
    else
    {
      defs[i].red   = xa_cmap[j].red;
      defs[i].green = xa_cmap[j].green;
      defs[i].blue  = xa_cmap[j].blue;
    }
    if (act_cycle->flags & ACT_CYCLE_REVERSE) 
      curpos = (curpos)?(curpos - 1):(size - 1); 
    else
      curpos = (curpos >= (size - 1))?(0):(curpos + 1); 
  }
  XStoreColors(theDisp,theCmap,defs,act_cycle->size);
  if (xa_audio_enable != TRUE) XSync(theDisp,False);
}

/*
 *
 */
void Free_Actions(acts)
XA_ACTION *acts;
{
  XA_ACTION *act;
  act = acts;
  while(act != 0)
  { XA_ACTION *tmp = act;
    act = act->next;
    ACT_Free_Act(tmp);
    FREE(tmp,0x01);
  }
}

XA_ANIM_HDR *Return_Anim_Hdr(file_hdr)
XA_ANIM_HDR *file_hdr;
{
  XA_ANIM_HDR *tmp_hdr;
  if ((file_hdr==0) || (first_file==0)) TheEnd1("Return_Anim_Hdr err");
  xa_file_num--;
  if (first_file == file_hdr)
  {
    first_file = 0;
    tmp_hdr = 0;
  }
  else /* removed file_hdr from the loop */
  {
    tmp_hdr = file_hdr->prev_file;
    tmp_hdr->next_file = file_hdr->next_file;
    file_hdr->next_file->prev_file = tmp_hdr;
  }
  FREE(file_hdr,0x02);
  return(tmp_hdr);
}

XA_ANIM_HDR *Get_Anim_Hdr(file_hdr,file_name)
XA_ANIM_HDR *file_hdr;
char *file_name;
{
  XA_ANIM_HDR *temp_hdr;
  LONG length;

  temp_hdr = (XA_ANIM_HDR *)malloc( sizeof(XA_ANIM_HDR) );
  if (temp_hdr == 0) TheEnd1("Get_Anim_Hdr: malloc failed\n");

  if (first_file == 0) first_file = temp_hdr;
  if (file_hdr == 0)
  {
    xa_file_num = 0;
    temp_hdr->next_file = temp_hdr;
    temp_hdr->prev_file = temp_hdr;
  }
  else
  {
    temp_hdr->prev_file = file_hdr;
    temp_hdr->next_file = file_hdr->next_file;
    file_hdr->next_file = temp_hdr;
    first_file->prev_file = temp_hdr;
  }

  temp_hdr->anim_type = 0;
  temp_hdr->imagex = 0;
  temp_hdr->imagey = 0;
  temp_hdr->imagec = 0;
  temp_hdr->imaged = 0;
  temp_hdr->anim_flags	= 0;
  temp_hdr->loop_num	= 0;
  temp_hdr->loop_frame	= 0;
  temp_hdr->last_frame	= 0;
  temp_hdr->total_time	= 0;
  temp_hdr->frame_lst	= 0;
  temp_hdr->acts	= 0;
  temp_hdr->first_snd	= 0;
  temp_hdr->last_snd	= 0;
  temp_hdr->file_num	= xa_file_num;
  temp_hdr->fname	= 0;
  temp_hdr->max_fvid_size	= 0;
  temp_hdr->max_faud_size	= 0;
  temp_hdr->init_vid	= 0;
  temp_hdr->init_aud	= 0;
  xa_file_num++;

  length = strlen(file_name);
  temp_hdr->name = (char *)malloc(length + 1);
  strcpy(temp_hdr->name,file_name);
  return(temp_hdr);
}
  

void XA_Install_CMAP(chdr)
XA_CHDR *chdr;
{ ColorReg *tcmap;
  LONG j;

  tcmap		= chdr->cmap;
  xa_cmap_size	= chdr->csize;
  if (xa_cmap_size > x11_cmap_size) xa_cmap_size = x11_cmap_size;
  xa_cmap_off	= chdr->coff;
  xa_map	= chdr->map;
  xa_map_size	= chdr->msize;
  xa_map_off	= chdr->moff;
  for(j=0; j<xa_cmap_size;j++)
  {
    xa_cmap[j].red   = tcmap[j].red;
    xa_cmap[j].green = tcmap[j].green;
    xa_cmap[j].blue  = tcmap[j].blue;
    xa_cmap[j].gray  = tcmap[j].gray;
  }

  DEBUG_LEVEL1 fprintf(stderr,"  Install CMAP %lx old was %lx\n",
					(ULONG)chdr,(ULONG)xa_chdr_now);

  if ( x11_cmap_flag == FALSE )
  {
    DEBUG_LEVEL1 fprintf(stderr,"  Fake Install since cmap not writable\n");
    xa_chdr_now = chdr;
    return;
  }
  else /* install the cmap */
  {
    DEBUG_LEVEL2 fprintf(stderr,"CMAP: size=%ld off=%ld\n",
		xa_cmap_size,xa_cmap_off);
    if (xa_cmap_size > x11_cmap_size)
    {
      fprintf(stderr,"Install CMAP: Error csize(%ld) > x11 cmap(%ld)\n",
		xa_cmap_size,x11_cmap_size);
      return;
    }
    if (x11_display_type & XA_X11_GRAY)
    {
      for(j=0;j<xa_cmap_size;j++)
      {
        defs[j].pixel = xa_cmap_off + j;
        defs[j].red   = xa_cmap[j].gray;
        defs[j].green = xa_cmap[j].gray;
        defs[j].blue  = xa_cmap[j].gray;
        defs[j].flags = DoRed | DoGreen | DoBlue;
        DEBUG_LEVEL3 fprintf(stderr," g %ld) %lx %lx %lx <%lx>\n",
             j,xa_cmap[j].red,xa_cmap[j].green,xa_cmap[j].blue,xa_cmap[j].gray);
		
      }
    }
    else
    {
      for(j=0; j<xa_cmap_size;j++)
      {
        defs[j].pixel = xa_cmap_off + j;
        defs[j].red   = xa_cmap[j].red;
        defs[j].green = xa_cmap[j].green;
        defs[j].blue  = xa_cmap[j].blue;
        defs[j].flags = DoRed | DoGreen | DoBlue;
        DEBUG_LEVEL3 fprintf(stderr," %ld) %lx %lx %lx <%lx>\n",
             j,xa_cmap[j].red,xa_cmap[j].green,xa_cmap[j].blue,xa_cmap[j].gray);
		
      }
    }
    XStoreColors(theDisp,theCmap,defs,xa_cmap_size);
    /* XSync(theDisp,False); */
    /* following is a kludge to force the cmap to be loaded each time 
     * this sometimes helps synchronize image to vertical refresh on
     * machines that wait until vertical refresh to load colormap
     */
    if (cmap_force_load == FALSE) xa_chdr_now = chdr;
  }
}


void XA_Time_Init()
{
  gettimeofday(&tv, 0);
  xa_av_time_off = xa_time_off = tv.tv_sec;
}

/*
 * return time from start in milliseconds
 */
LONG XA_Time_Read()
{
  LONG t;
  gettimeofday(&tv, 0);
  t = (tv.tv_sec - xa_time_off) * 1000 + (tv.tv_usec / 1000);
  return(t);
}


/*
 *
 */
void XA_Time_Check()
{
  LONG time_int;

  xa_time_end = XA_Time_Read();
  time_int = xa_time_end - xa_time_start;
  xa_time_av = (xa_time_av * xa_time_num) + time_int;
  xa_time_num++;
  xa_time_av /= xa_time_num;
  fprintf(stderr,"l_time = %ld  av %ld\n",time_int,xa_time_av);
  xa_time_start = XA_Time_Read();

}

ULONG XA_Read_Int(s,j)
UBYTE *s;
ULONG *j;
{
  ULONG i,num;
  i = *j; num = 0;
  while( (s[i] >= '0') && (s[i] <= '9') )
	{ num *= 10; num += (ULONG)(s[i]) - (ULONG)('0'); i++; }
  *j = i;   
  return(num);
}

float XA_Read_Float(s,j)
UBYTE *s;
ULONG *j;
{
  ULONG i;
  float num;
  i = *j; num = 0.0;
  while( (s[i] >= '0') && (s[i] <= '9') )
	{ num *= 10; num += (float)(s[i]) - (float)('0'); i++; }
  if (s[i] == '.')
  {
    float pos = 10.0;
    i++; 
    while( (s[i] >= '0') && (s[i] <= '9') )
    { 
      num += ((float)(s[i]) - (float)('0')) / pos;
      pos *= 10.0; i++;
    }
  }
  *j = i;   
  return(num);
}

LONG XA_Get_Class(p)
char *p;
{
  ULONG i,len;
  char tmp[16];
  
  /* copy and convert to lower case */
  len = strlen(p); if (len > 16) return( -1 );
  for(i=0; i < len; i++) tmp[i] = (char)tolower( (int)(p[i]) );
  tmp[i] = 0;
  if      (strcmp(tmp,"staticgray\0" ) == 0)	return( StaticGray );
  else if (strcmp(tmp,"grayscale\0"  ) == 0)	return( GrayScale );
  else if (strcmp(tmp,"staticcolor\0") == 0)	return( StaticColor );
  else if (strcmp(tmp,"pseudocolor\0") == 0)	return( PseudoColor );
  else if (strcmp(tmp,"truecolor\0"  ) == 0)	return( TrueColor );
  else if (strcmp(tmp,"directcolor\0") == 0)	return( DirectColor );
  else return( -1 );
}

/**********************
 * Add frame to current pause list
 */
void XA_Add_Pause(frame)
ULONG frame;		/* frame at which to pause */
{ XA_PAUSE *new_phdr;

  new_phdr = (XA_PAUSE *)malloc(sizeof(XA_PAUSE));
  if (new_phdr==0) TheEnd1("XA_Add_Pause: malloc err\n");
  new_phdr->frame = frame;
  new_phdr->next = 0;
  if (xa_pause_hdr==0) xa_pause_hdr = new_phdr;
  else
  { XA_PAUSE *t_phdr = xa_pause_hdr;
    while(t_phdr->next != 0) t_phdr = t_phdr->next;
    t_phdr->next = new_phdr;
  }
}

/***************************************************************************
 *  Function to return Time since Start of Animation.
 */ 
LONG XA_Read_AV_Time()
{
  return( (XA_Time_Read() - xa_av_time_off) );
}

/***************************************************************************
 *
 */
void XA_Reset_Speed_Time(vid_time,speed_scale)
LONG vid_time; ULONG speed_scale;
{ LONG scaled_vid_time = XA_SPEED_ADJ(vid_time,speed_scale);
  xa_av_time_off = XA_Time_Read() - scaled_vid_time;
}

/***************************************************************************
 * This Function accepts current video time and if Audio is enabled, it
 * adjusts the audio stream to sync with the video. Whether or not audio
 * is enabled, it adjusts animation start time to be now.
 */
void XA_Reset_AV_Time(vid_time,speed_scale)
LONG vid_time; ULONG speed_scale;
{ int xflag = FALSE;
  LONG scaled_vid_time = XA_SPEED_ADJ(vid_time,speed_scale);

  if ( (xa_audio_enable == TRUE) && (speed_scale==XA_SPEED_NORM) )
  {
    if (xa_snd_cur==0) xa_snd_cur = cur_file->first_snd;
    XA_Audio_Init_Snd(xa_snd_cur);
    /* Move to the correct snd chunk */
    while(xflag == FALSE)
    { LONG snd_time = xa_snd_cur->snd_time;
      if (snd_time > vid_time)
      { XA_SND *p_snd = xa_snd_cur->prev;
        DEBUG_LEVEL1 fprintf(stderr,"s>v %ld %ld\n",snd_time,vid_time);
        if (p_snd) xa_snd_cur = p_snd;
        else xflag = TRUE;
      }
      else if (snd_time < vid_time)
      { XA_SND *n_snd = xa_snd_cur->next;
        DEBUG_LEVEL1 fprintf(stderr,"s<v %ld %ld\n",snd_time,vid_time);
        if (n_snd) 
        {
	  if (n_snd->snd_time <= vid_time) xa_snd_cur = n_snd;
	  else xflag = TRUE;
        }
        else xflag = TRUE;
      }
      else 
      {
        DEBUG_LEVEL1 fprintf(stderr,"s=v %ld %ld\n",snd_time,vid_time);
        xflag = TRUE;
      }
    }
    /* Move within the snd chunk - HAVE NOP FLAG */
    if (xa_snd_cur)
    { XA_SND *shdr = xa_snd_cur;
	/* read in from file if needed */
      if (xa_snd_cur->fpos >= 0)
      { 
        xa_snd_cur->snd = xa_audcodec_buf;
        XA_Read_Audio_Delta(xa_aud_fd,xa_snd_cur->fpos,
				xa_snd_cur->tot_bytes,xa_audcodec_buf);
      } 
      { ULONG tmp_cnt; LONG diff;

	/* time diff in ms */
        diff =  (vid_time - shdr->snd_time); if (diff < 0) diff = 0;
	/* calc num of samples in that time frame */
	tmp_cnt = (diff * shdr->ifreq) / 1000; 
	if (tmp_cnt & 0x01) tmp_cnt--;  /* make multiple of 2 for ADPCM */
	/* Init snd_hdr */
        XA_Audio_Init_Snd(xa_snd_cur);
DEBUG_LEVEL1 fprintf(stderr,"AV rst: snd %lx cnt %ld\n",xa_snd_cur,tmp_cnt);
	if (tmp_cnt) /* not at beginning */
	{ char *garb; /* play sound into garb buffer */
	  garb = (char *)malloc(4 * tmp_cnt);
          if (garb) 
	  { diff = tmp_cnt - xa_snd_cur->delta(xa_snd_cur,garb,0,tmp_cnt);
	    free(garb);
	    if (diff != 0) fprintf(stderr,"AV Warn: rst sync err %lx\n",diff);
DEBUG_LEVEL1 fprintf(stderr,"AV rst: snd %lx samps %ld bytes %ld diff %ld\n",
		xa_snd_cur,xa_snd_cur->samp_cnt,xa_snd_cur->byte_cnt,diff);
	  }
	}
      }
      xa_time_audio = vid_time;
      xa_timelo_audio = 0;
    } /* end of valid xa_snd_cur */ 
  } /* end of audio enable true */
DEBUG_LEVEL1 fprintf(stderr,"RESET_AV %ld\n",vid_time);
  xa_av_time_off = XA_Time_Read() - scaled_vid_time;
}

/***************************************************************************
 * Look at environmental variable and look for
 * INTERNAL
 * EXTERNAL
 * HEADPHONES
 * NONE
 ***************************************************************************/

ULONG XA_Audio_Speaker(env_v)
char *env_v;
{
  char *env_d;

  env_d = getenv(env_v);
  if (env_d)
  {
    if ((*env_d == 'i') || (*env_d == 'I')) return(XA_AUDIO_PORT_INT);
    else if ((*env_d == 'h') || (*env_d == 'H')) return(XA_AUDIO_PORT_HEAD);
    else if ((*env_d == 'e') || (*env_d == 'E')) return(XA_AUDIO_PORT_EXT);
    else if ((*env_d == 'n') || (*env_d == 'N')) return(XA_AUDIO_PORT_NONE);
  }
  return(0);
}


/* NEW CODE GOES HERE */

/***********************************************************************
 *
 *
 ***************/
void Hard_Death()
{
  XA_ANIM_HDR *tmp_hdr;

  if (xa_audio_present==XA_AUDIO_OK) XA_Audio_Kill();
  if (xa_vid_fd >= 0) { close(xa_vid_fd); xa_vid_fd = -1; }
  if (xa_aud_fd >= 0) { close(xa_aud_fd); xa_aud_fd = -1; }
  if (xa_vidcodec_buf) { FREE(xa_vidcodec_buf,0x05); xa_vidcodec_buf=0;}
  if (xa_audcodec_buf) { FREE(xa_audcodec_buf,0x99); xa_audcodec_buf=0;}
  if (cur_file !=0 )
  {
    cur_file = first_file;
    first_file->prev_file->next_file = 0;  /* last file's next ptr to 0 */
  }
  while( cur_file != 0)
  {
    Free_Actions(cur_file->acts);
    tmp_hdr = cur_file->next_file;
    if (cur_file->name) FREE(cur_file->name,0x06);
    FREE(cur_file,0x07);
    cur_file = tmp_hdr;
  }
  if (!shm)  /* otherwise it's free'd below as imX_shminfo.shmaddr */
  {
    if (im_buff0) { FREE(im_buff0,0x08); im_buff0=0; }
    if (im_buff1) { FREE(im_buff1,0x09); im_buff1=0; }
    if (im_buff2) { FREE(im_buff2,0x0a); im_buff2=0; }
  }
  if (im_buff3) { FREE(im_buff3,0x0b); im_buff3=0; }
  if (xa_disp_buff) { FREE(xa_disp_buff,0x0c); xa_disp_buff=0; }
  if (xa_scale_buff) { FREE(xa_scale_buff,0x0d); xa_scale_buff=0; }
  if (xa_scale_row_buff) { FREE(xa_scale_row_buff,0x0e); xa_scale_row_buff=0;}
  if (xa_cmap) { FREE(xa_cmap,0x0f); xa_cmap=0; }
  if (xa_ham_map) { FREE(xa_ham_map,0x10); xa_ham_map=0;}
  if (cmap_cache != 0) { FREE(cmap_cache,0x23); }
  if (cmap_cache2 != 0) { FREE(cmap_cache2,0x99); }
  while(xa_chdr_start)
  {
    XA_CHDR *tmp;
    tmp = xa_chdr_start;
    xa_chdr_start = xa_chdr_start->next;
    if (tmp->cmap) { FREE(tmp->cmap,0x11); tmp->cmap=0; }
    if (tmp->map) { FREE(tmp->map,0x12); tmp->map=0; }
    FREE(tmp,0x13);
  }
#ifdef XSHM
  if (shm)
  {
    if (im0_shminfo.shmaddr) { XShmDetach(theDisp,&im0_shminfo);
		     shmdt(im0_shminfo.shmaddr); }
    if (im1_shminfo.shmaddr) { XShmDetach(theDisp,&im1_shminfo);
		     shmdt(im1_shminfo.shmaddr); }
    if (im2_shminfo.shmaddr) { XShmDetach(theDisp,&im2_shminfo);
		     shmdt(im2_shminfo.shmaddr); }
    if (im0_Image)
    {
      im0_Image->data = 0;
      XDestroyImage(im0_Image);
    }
    if (im1_Image)
    {
      im1_Image->data = 0;
      XDestroyImage(im1_Image);
    }
    if (im2_Image)
    {
      im2_Image->data = 0;
      XDestroyImage(im2_Image);
    }
  }
#endif
  exit(0);
}

/*
 * This function (hopefully) provides a clean exit from our code.
 */
void TheEnd()
{
  XA_ANIM_HDR *tmp_hdr;

/*POD TEMP*/
  jpg_free_stuff();
  mpg_free_stuff();

  if (xa_audio_present==XA_AUDIO_OK) XA_Audio_Kill();
  if (xa_vid_fd >= 0) { close(xa_vid_fd); xa_vid_fd = -1; }
  if (xa_aud_fd >= 0) { close(xa_aud_fd); xa_aud_fd = -1; }
  if (xa_vidcodec_buf) { FREE(xa_vidcodec_buf,0x14); xa_vidcodec_buf=0;}
  if (xa_audcodec_buf) { FREE(xa_audcodec_buf,0x99); xa_audcodec_buf=0;}
  if (cur_file !=0 )
  {
    cur_file = first_file;
    first_file->prev_file->next_file = 0;  /* last file's next ptr to 0 */
  }
  while( cur_file != 0)
  {
    Free_Actions(cur_file->acts);
    tmp_hdr = cur_file->next_file;
    if (cur_file->name) { FREE(cur_file->name,0x15); cur_file->name = 0; }
    FREE(cur_file,0x16);
    cur_file = tmp_hdr;
  }
  if (!shm)  /* otherwise it's free'd below as imX_shminfo.shmaddr */
  {
    if (im_buff0) { FREE(im_buff0,0x17); im_buff0=0; }
    if (im_buff1) { FREE(im_buff1,0x18); im_buff1=0; }
    if (im_buff2) { FREE(im_buff2,0x19); im_buff2=0; }
  }
  if (im_buff3) { FREE(im_buff3,0x1a); im_buff3=0; }
  if (xa_disp_buff) { FREE(xa_disp_buff,0x1b); xa_disp_buff=0; }
  if (xa_scale_buff) { FREE(xa_scale_buff,0x1c); xa_scale_buff=0; }
  if (xa_scale_row_buff) { FREE(xa_scale_row_buff,0x1d); xa_scale_row_buff=0;}
  if (xa_cmap) { FREE(xa_cmap,0x1e); xa_cmap=0; }
  if (xa_ham_map) { FREE(xa_ham_map,0x1f); xa_ham_map=0;}
  if (cmap_cache != 0) { FREE(cmap_cache,0x23); }
  if (cmap_cache2 != 0) { FREE(cmap_cache2,0x99); }
  while(xa_chdr_start)
  {
    XA_CHDR *tmp;
    tmp = xa_chdr_start;
    xa_chdr_start = xa_chdr_start->next;
    if (tmp->cmap) { FREE(tmp->cmap,0x20); tmp->cmap=0; }
    if (tmp->map) { FREE(tmp->map,0x21); tmp->map=0; }
    FREE(tmp,0x22);
  }
#ifdef XSHM
  if (shm)
  {
    if (im0_shminfo.shmaddr) { XShmDetach(theDisp,&im0_shminfo);
		     shmdt(im0_shminfo.shmaddr); }
    if (im1_shminfo.shmaddr) { XShmDetach(theDisp,&im1_shminfo);
		     shmdt(im1_shminfo.shmaddr); }
    if (im2_shminfo.shmaddr) { XShmDetach(theDisp,&im2_shminfo);
		     shmdt(im2_shminfo.shmaddr); }
    if (im0_Image)
    {
      im0_Image->data = 0;
      XDestroyImage(im0_Image);
    }
    if (im1_Image)
    {
      im1_Image->data = 0;
      XDestroyImage(im1_Image);
    }
    if (im2_Image)
    {
      im2_Image->data = 0;
      XDestroyImage(im2_Image);
    }
  }
#endif
  if (theImage) 
  {
    theImage->data = 0;
    XDestroyImage(theImage);
  }
  if (theDisp) XtCloseDisplay(theDisp);
  exit(0);
}


/*
 * just prints a message before calling TheEnd()
 */
void TheEnd1(err_mess)
char *err_mess;
{
 fprintf(stderr,"%s\n",err_mess);
 TheEnd();
}

XA_ANIM_SETUP *XA_Get_Anim_Setup()
{
  XA_ANIM_SETUP *setup = (XA_ANIM_SETUP *)malloc(sizeof(XA_ANIM_SETUP));
  if (setup==0) TheEnd1("XA_ANIM_SETUP: malloc err\n");
  setup->imagex = setup->max_imagex = 0;
  setup->imagey = setup->max_imagey = 0;
  setup->imagec = setup->depth = 0;
  setup->compression = 0;
  setup->pic = 0;
  setup->pic_size = 0;
  setup->max_fvid_size = setup->max_faud_size = 0;
  setup->vid_time = setup->vid_timelo = 0;
  setup->aud_time = setup->aud_timelo = 0;
  setup->chdr = 0;
  setup->cmap_flag      = 0;
  setup->cmap_cnt       = 0;
  setup->cmap_frame_num = 0;
  return(setup);
}
 

void XA_Free_Anim_Setup(setup)
XA_ANIM_SETUP *setup;
{
  if (setup) free(setup);
}
