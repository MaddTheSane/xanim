
/*
 * xanim.c
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

#define DA_MAJOR_REV 2
#define DA_MINOR_REV 80.0

/*
 * Any problems, suggestions, solutions, anims to show off, etc contact:
 *
 * podlipec@wellfleet.com  or podlipec@shell.portal.com
 *
 */

#include "xanim.h"
#include "xa_cmap.h"
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>

#include <sys/types.h>
#ifndef __CYGWIN32__       /* Not needed for GNU-Win32 - used for audio proc */
#include <sys/ipc.h>
#include <sys/msg.h>
#endif


#ifdef __QNX__
#include <signal.h>
#else
#include <sys/signal.h>
#endif

#ifdef MSDOS
#include <sys/resource.h>
#else /* not MSDOS */
#ifndef VMS   /* NOT MSDOS and NOT VMS ie Unix */
#include <sys/time.h>
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

#ifdef XA_REMOTE_CONTROL
extern void XA_Create_Remote();
extern void XA_Remote_Pause();
extern void XA_Remote_Adj_Volume();
extern void XA_Remote_Free();

#ifdef XA_PETUNIA
extern void XA_Remote_ColorUpdate();
#endif
#endif

#include "xa_x11.h"
#include "xa_ipc.h"

#ifdef XA_AUDIO
void XA_Video_BOFL();
void XA_Setup_BOFL();
#endif

/* POD TEMP */
XA_AUD_FLAGS *vaudiof;
xaULONG xa_has_audio;

/*KLUDGES*/
extern void jpg_free_stuff();


/************************************************** Fork Audio Defines *******/

#include "xa_ipc_cmds.h"


xaULONG xa_forkit;
xaULONG xa_vaudio_present;
xaULONG xa_vaudio_enable;
xaULONG xa_vaudio_status;
xaULONG xa_vaudio_merge;
xaULONG xa_vaudio_merge_scale;

XA_SND *xa_snd_cur;

xaULONG XA_Audio_Speaker(); 
void XA_Read_Audio_Delta();

/* POD: NEED TO HAVE THIS INFO SENT ACROSS TO AUDIO CHILD IF CHANGED! */
char *xa_audio_device = DEFAULT_AUDIO_DEVICE_NAME;



extern Widget theWG;

/* POD TESTING */
xaULONG xa_kludge1_fli;
xaULONG xa_kludge2_dvi;
xaULONG xa_kludge900_aud;


void TheEnd();
void TheEnd1();
static void Hard_Death();
static void Usage();
static void Usage_Quick();
static void XA_Lets_Get_Looped();
void XAnim_Looped();
extern xaULONG XA_Open_And_ID_File();
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
xaLONG XA_Time_Read();
xaLONG XA_Time_Init();
void XA_Time_Check();
void XA_Reset_AV_Time();
xaLONG XA_Read_AV_Time();
void XA_Reset_Speed_Time();
void XA_Install_CMAP();
xaULONG XA_Mapped_To_Display();
static xaULONG XA_Read_Int();
static float XA_Read_Float();
static xaLONG XA_Get_Class();
static void XA_Add_Pause();
void XA_Store_Title();
static void XA_Resize_Window();
XA_ANIM_SETUP *XA_Get_Anim_Setup();
void XA_Free_Anim_Setup();
XA_FUNC_CHAIN *XA_Get_Func_Chain();
void XA_Add_Func_To_Free_Chain();
void XA_Walk_The_Chain();
void XA_Store_Title();
void XA_Adjust_Video_Timing();

extern void X11_Setup_Window();
extern void X11_Map_Window();
extern void X11_Init_Image_Struct();
extern void X11_Init();
extern void X11_Pre_Setup();
extern void ACT_Make_Images();
extern void XA_Show_Action();
extern void XA_Free_CMAP();
extern void Init_Video_Codecs();
extern void Free_Video_Codecs();

extern xaULONG XA_Alloc_Input_Methods();
extern xaULONG XA_Setup_Input_Methods();

xaULONG DUM_Read_File();


xaULONG shm = 0;
#ifdef XSHM
XShmSegmentInfo im0_shminfo;
XShmSegmentInfo im1_shminfo;
XShmSegmentInfo im2_shminfo;
XImage *im0_Image = 0;
XImage *im1_Image = 0;
XImage *im2_Image = 0;
XImage *sh_Image = 0;
#endif
static xaULONG XA_Setup_Them_Buffers();

xaULONG mbuf = 0;

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

xaLONG x11_error_possible = 0; /* -1 err occured. 0 no error. 1 err expected */
xaLONG x11_depth;
xaLONG x11_class;
xaLONG x11_bytes_pixel;
xaLONG x11_byte_order;
xaLONG xam_byte_order;
xaLONG x11_byte_mismatch = xaFALSE;
xaLONG x11_bits_per_pixel;
xaLONG x11_bitmap_pad;
xaLONG x11_pack_flag;
xaLONG x11_bitmap_unit;
xaLONG x11_bit_order;
xaLONG x11_cmap_flag;
xaLONG x11_cmap_size;
xaLONG x11_cmap_installed = xaFALSE;
xaLONG x11_disp_bits;
xaLONG x11_cmap_type;
xaLONG x11_depth_mask;
xaLONG x11_display_type;
xaLONG x11_red_mask;
xaLONG x11_green_mask;
xaLONG x11_blue_mask;
xaLONG x11_red_shift;
xaLONG x11_green_shift;
xaLONG x11_blue_shift;
xaLONG x11_red_bits;
xaLONG x11_green_bits;
xaLONG x11_blue_bits;
xaLONG x11_black;
xaLONG x11_white;
xaLONG x11_verbose_flag;
xaLONG pod_max_colors;
xaLONG xa_user_visual;
xaLONG xa_user_class;
xaULONG x11_kludge_1;
/*POD TEST */
xaULONG pod = 0;

/* SMR 1 */
/* variables used when playing into another app's window */
xaLONG xa_window_x;
xaLONG xa_window_y;
xaULONG xa_window_id;
char *xa_window_propname;
xaULONG xa_window_center_flag;
xaULONG xa_window_prepare_flag;
xaULONG xa_window_refresh_flag;
/* end SMR 1 */


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
static xaULONG xa_anim_type;
static xaULONG xa_merged_anim_flags;


/*
 * cmap keeps track of the current colors to the screen.
 *
 */
xaLONG cmap_true_to_332;
xaLONG cmap_true_to_gray;
xaLONG cmap_true_to_1st;
xaLONG cmap_true_to_all;
xaLONG cmap_true_map_flag;
xaLONG cmap_luma_sort;
xaLONG cmap_map_to_1st_flag;
xaLONG cmap_map_to_one_flag;
xaLONG cmap_play_nice;
xaLONG xa_allow_nice;
xaLONG cmap_hist_flag;
xaLONG cmap_dither_type;
xaLONG cmap_median_type;
xaLONG cmap_median_bits;
xaLONG cmap_use_combsort;
double xa_disp_gamma;
double xa_anim_gamma;
xaULONG xa_gamma_flag;
xaUSHORT xa_gamma_adj[256];
XA_CHDR *xa_chdr_start;
XA_CHDR *xa_chdr_cur = 0;
XA_CHDR *xa_chdr_now = 0;
XA_CHDR *xa_chdr_first = 0;
xaUSHORT *cmap_cache,*cmap_cache2;
xaULONG cmap_cache_size;
xaULONG cmap_cache_bits;
xaULONG cmap_cache_rmask;
xaULONG cmap_cache_gmask;
xaULONG cmap_cache_bmask;
XA_CHDR *cmap_cache_chdr;
xaSHORT cmap_floyd_error;

xaULONG cmap_color_func;
xaULONG cmap_sample_cnt;   /* sample anim every X many frames for colors */

/*
 * These are variables for HAM images
 *
 */
xaULONG xa_ham_map_size;
xaULONG *xa_ham_map;
XA_CHDR *xa_ham_chdr;
xaULONG xa_ham_init;
/*
 * These are for converting xaTRUE images to PSEUDO
 *
 */
xaULONG xa_r_shift,xa_g_shift,xa_b_shift;
xaULONG xa_r_mask,xa_g_mask,xa_b_mask;
xaULONG xa_gray_bits,xa_gray_shift;

ColorReg *xa_cmap = 0;
xaULONG  xa_cmap_size,xa_cmap_off;
xaULONG  *xa_map;
xaULONG  xa_map_size,xa_map_off;

/*
 * Global variable to keep track of Anim type
 */
xaLONG filetype;
xaULONG xa_title_flag;


/*
 * Global variables to keep track of current width, height, num of colors and
 * number of bit planes respectively. 
 *
 * the max_ variable are used for worst case allocation. Are useful for Anims
 * that have multiple image sizes.
 *
 * image_size and max_image_size are imagex * imagey, etc.
 */
xaULONG xa_image_size;
xaULONG xa_max_image_size;
xaULONG xa_imagex,xa_max_imagex;
xaULONG xa_imagey,xa_max_imagey;
xaULONG xa_imaged;

/*
 * Scaling Variable
 *
 */

xaULONG xa_need_to_scale_b;
xaULONG xa_need_to_scale_u;
float xa_scalex, xa_scaley;
float xa_bscalex, xa_bscaley;
xaULONG xa_buff_x,xa_buff_y;		/* anim buffering size */
xaULONG xa_allow_lace;
xaULONG xa_allow_resizing; 		/* allow window resizing */
xaULONG xa_disp_y, xa_max_disp_y;
xaULONG xa_disp_x, xa_max_disp_x;
xaULONG xa_disp_size;
xaULONG xa_max_disp_size;
xaULONG x11_display_x, x11_display_y;	/* max display size */
xaULONG x11_window_x, x11_window_y;	/* current window size */
xaULONG *xa_scale_row_buff;
xaULONG xa_scale_row_size;
xaULONG x11_expose_flag;

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
xaLONG xa_file_num;
xaLONG cur_floop,cur_frame;
xaLONG xa_cycle_cnt;      /* keeps track of number of cycling actions called */
xaLONG xa_now_cycling;    /* indicates that cycling is now in progress */
xaLONG xa_anim_cycling;   /* if set, allows cycling for animations */
xaLONG file_is_started;

int xa_vid_fd;		/* Used if anim is being read from a file */
xaUBYTE *xa_vidcodec_buf;
xaULONG xa_vidcodec_maxsize; 

int xa_aud_fd;		/* Used if anim is being read from a file */
xaUBYTE *xa_audcodec_buf;
xaULONG xa_audcodec_maxsize;

/*
 * Image buffers.
 * im_buff1 is used for double buffered anims(IFF).
 * xa_disp_buff is needed when the display is of a different format than the
 * double buffered images. (like HAM or xaTRUE_COLOR).
 *
 * xa_pic is a pointer to im_buff0 or im_buff1 during double buffering.
 * im_buff2 is used for dithered or HAM images
 */
char *im_buff0,*im_buff1,*im_buff2,*im_buff3;
char *xa_pic,*xa_disp_buff,*xa_scale_buff;
xaULONG xa_disp_buff_size,xa_scale_buff_size;

/*
 * Variables for statistics
 *
 */
xaLONG xa_time_now;
xaLONG xa_time_video,xa_time_audio,xa_ptime_video;
xaULONG xa_timelo_audio;
xaLONG xa_audio_start;
xaLONG xa_time_reset;
xaLONG xa_av_time_off;
xaLONG xa_skip_flag;	/* enables skipping */
xaLONG xa_skip_diff;	/* keeps track of how far behind in frames */
xaLONG xa_skip_video;	/* flag to Decoder  0 no 1 fallback/skip 2 skip */
xaLONG xa_skip_cnt;	/* keeps track of how many we skip */
xaLONG xa_time_start;
xaLONG xa_time_end;
xaLONG xa_time_off;
xaLONG xa_no_disp;
xaLONG xa_time_flag;
xaLONG xa_time_av;
xaLONG xa_frames_skipd, xa_frames_Sskipd;
xaLONG xa_time_num;
xaLONG xa_root;

struct timeval tv;


/* 
 * Global flags that are set on command line.
 */
xaLONG xa_buffer_flag;
xaLONG x11_shared_flag;
xaLONG x11_multibuf_flag;
xaLONG xa_file_flag;
xaLONG fade_flag,fade_time;
xaLONG xa_noresize_flag;
xaLONG xa_optimize_flag;
xaLONG xa_pixmap_flag;
xaLONG xa_dither_flag;
xaLONG xa_pack_flag;
xaLONG xa_debug;
xaLONG xa_verbose;
xaLONG xa_quiet;

xaLONG xa_loop_each_flag;
xaLONG xa_pingpong_flag;
xaLONG xa_jiffy_flag;
xaULONG xa_speed_scale,xa_speed_change;

xaLONG xa_anim_flags;
xaLONG xa_anim_holdoff;
xaLONG xa_anim_status,xa_old_status;
xaLONG xa_anim_ready = xaFALSE;
xaLONG xa_remote_ready = xaFALSE;
xaLONG xa_use_depth_flag;

xaLONG xa_exit_flag;
xaLONG xa_exit_early_flag;
xaLONG xa_remote_flag;

XA_PAUSE *xa_pause_hdr=0;
xaLONG xa_pause_last;

/* Used for specifying the animation format for raw streams */
char	*xa_raw_format = 0;

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
  fprintf(stdout,"   -h  lists some common options, but may be out of date.\n");
  fprintf(stdout,"   See xanim.readme or the man page for detailed help.\n");
  TheEnd();
}
void
Usage_Default_TF(flag,justify)
xaULONG flag,justify;
{
  if (justify == 1) fprintf(stdout,"            ");
  if (flag == xaTRUE) fprintf(stdout," default is on.\n");
  else fprintf(stdout," default is off.\n");
}

void
Usage_Default_Num(num,justify)
xaULONG num,justify;
{
  if (justify == 1) fprintf(stdout,"            ");
  fprintf(stdout," default is %d.\n",num);
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
 if (DEFAULT_PLUS_IS_ON == xaTRUE) 
      fprintf(stdout,"A + turns an option on and a - turns it off.\n");
 else fprintf(stdout,"A - turns an option on and a + turns it off.\n");
 fprintf(stdout,"\n");
 fprintf(stdout,"Options:\n");
 fprintf(stdout,"\n  A[aopts]  Audio SubMenu\n");
 fprintf(stdout,"      ADdev AIX Audio only.  dev  is audio device.\n");
 fprintf(stdout,"      Ae    ENABLE Audio.\n");
 fprintf(stdout,"      Ak    Enables video frame skipping to keep in sync with audio.\n");
 fprintf(stdout,"      Ap#   Play Audio from output port #(Sparc only).\n");
/* fprintf(stdout,"      As#   Scale Audio playback speed by #.\n"); */
 fprintf(stdout,"      Av#   Set Audio volume to #. range 0 to 100.\n");
 fprintf(stdout,"\n  C[copts]  Color SubMenu\n");
 fprintf(stdout,"      C1    Create cmap from 1st TrueColor frame. Map\n");
 fprintf(stdout,"            the rest to this first cmap.(Could be SLOW)\n");
 fprintf(stdout,"      Ca    Remap  all images to single new cmap.\n");
 Usage_Default_TF(DEFAULT_CMAP_MAP_TO_ONE,1);
 fprintf(stdout,"      Cd    Use floyd-steinberg dithering(buffered only).\n");
 Usage_Default_TF((DEFAULT_CMAP_DITHER_TYPE==CMAP_DITHER_FLOYD),1);
 fprintf(stdout,"      CF4   Better(?) Color mapping for TrueColor anims.\n");
 Usage_Default_TF(xaFALSE,1);
 fprintf(stdout,"      Cg    Convert TrueColor anims to gray scale.\n");
 Usage_Default_TF(DEFAULT_TRUE_TO_GRAY,1);
 fprintf(stdout,"      Cn    Be Nice: allocate colors from Default cmap.\n");
 Usage_Default_TF(DEFAULT_CMAP_PLAY_NICE,1);
 fprintf(stdout,"\n  G[gopts]  Gamma SubMenu\n");
 fprintf(stdout,"      Ga#   Set gamma of animation. Default %f\n",
							(DEFAULT_ANIM_GAMMA));
 fprintf(stdout,"      Gd#   Set gamma of display. Default %f\n",
							(DEFAULT_DISP_GAMMA));
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
/* SMR 2 */
 fprintf(stdout,"\n  W[wopts]  Window SubMenu\n");
 fprintf(stdout,"      W#    X11 Window ID of window to draw into.\n");
 fprintf(stdout,"      Wd    Don't refresh window at end of anim.\n");
 fprintf(stdout,"      Wnx   Use property x for communication.\n");
 fprintf(stdout,"      Wp    Prepare anim, but don't start playing it.\n");
 fprintf(stdout,"      Wr    Resize X11 Window to fit anim.\n");
 fprintf(stdout,"      Wx#   Position anim at x coordinate #.\n");
 fprintf(stdout,"      Wy#   Position anim at y coordinate #.\n");
 fprintf(stdout,"      Wc    Position relative to center of anim.\n");
/* end SMR 2 */
 fprintf(stdout,"\n  Normal Options\n");
 fprintf(stdout,"       b    Uncompress and buffer images ahead of time.\n"); 
 Usage_Default_TF(DEFAULT_BUFF_FLAG,1);
#ifdef XSHM
 fprintf(stdout,"       B    Use X11 Shared Memory Extension if supported.\n"); 
 Usage_Default_TF(xaTRUE,1);
#endif
 fprintf(stdout,"       c    disable looping for nonlooping IFF anims.\n"); 
 Usage_Default_TF(DEFAULT_IFF_LOOP_OFF,1);
 fprintf(stdout,"       d#   debug. 0(off) to 5(most) for level of detail.\n"); 
 Usage_Default_Num(DEFAULT_DEBUG,1);
 fprintf(stdout,"       F    Enable dithering for certain Video Codecs only. see readme\n");
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
 fprintf(stdout,"       p    Use Pixmap instead of Image in X11(buffered only).\n"); 
 Usage_Default_TF(DEFAULT_PIXMAP_FLAG,1);
 fprintf(stdout,"       q    quiet mode.\n"); 
 fprintf(stdout,"       r    Allow color cycling for IFF single images.\n");
 fprintf(stdout,"       +root    Tile video onto Root Window.\n");
 Usage_Default_TF(DEFAULT_CYCLE_IMAGE_FLAG,1);
 fprintf(stdout,"       R    Allow color cycling for IFF anims.\n");
 Usage_Default_TF(DEFAULT_CYCLE_ANIM_FLAG,1);
 fprintf(stdout,"       T#   Title Option. See readme.\n");
 fprintf(stdout,"       v    verbose mode.\n"); 
 Usage_Default_TF(DEFAULT_VERBOSE,1);
 fprintf(stdout,"       V#   Use Visual #. # is obtained by +X option.\n"); 
 fprintf(stdout,"       X    X11 verbose mode. Display Visual information.\n");
 fprintf(stdout,"       Ze   Have XAnim exit after playing cmd line.\n");
 fprintf(stdout,"       Zp#  Pause at specified frame number.\n");
 fprintf(stdout,"       Zpe  Pause at end of animation.\n");
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
 fprintf(stdout,"        1    Decrease audio volume by 5 percent.\n");
 fprintf(stdout,"        2    Decrease audio volume by 1 percent.\n");
 fprintf(stdout,"        3    Increase audio volume by 1 percent.\n");
 fprintf(stdout,"        4    Increase audio volume by 5 percent.\n");
 fprintf(stdout,"        8    Send audio to headphones.\n");
 fprintf(stdout,"        9    Send audio to speakers.\n");
 fprintf(stdout,"        s    Mute audio.\n");
 fprintf(stdout,"\n");
 fprintf(stdout,"Mouse Buttons.\n");
 fprintf(stdout,"\n");
 fprintf(stdout,"      <Left> Single step back one frame.\n");
 fprintf(stdout,"    <Middle> Toggle. Starts/Stops animation.\n");
 fprintf(stdout,"     <Right> Single step forward one frame.\n");
 fprintf(stdout,"\n");
#ifndef XA_IS_PLUGIN
 exit(0);
#endif
}

int main(argc, argv)
int argc;
char *argv[];
{
  char *filename,*in;
  xaLONG i,j;

  vaudiof = 0;
/* 
 * Initialize global variables.
 */
  theDisp = NULL;

  xa_av_time_off = XA_Time_Init();
  xa_forkit = xaFALSE;

  xa_time_now		= 0;
  xa_time_video		= xa_ptime_video	=  0;
  xa_time_audio		= -1;
  xa_timelo_audio	= 0;
  xa_vid_fd		= xa_aud_fd		= -1;
  xa_vidcodec_buf	= xa_audcodec_buf	=  0;
  xa_vidcodec_maxsize	= xa_audcodec_maxsize	=  0;
  xa_kludge2_dvi	= xaFALSE;
  xa_kludge900_aud	= 0;
  xa_root		= xaFALSE;

#ifdef XA_AUDIO
  xa_vaudio_present	= XA_AUDIO_UNK;
  xa_vaudio_enable	= DEFAULT_XA_AUDIO_ENABLE;
  xa_vaudio_status	= XA_AUDIO_NICHTDA;
  xa_vaudio_merge	= xaFALSE;
  xa_vaudio_merge_scale = xaFALSE;

	/*** POD FOR FORK TESTING PURPOSES ONLY */
  if ( strcmp( argv[0], "xad\0") == 0) XA_IPC_Set_Debug(xaTRUE);

  xa_forkit = XA_Give_Birth();  /* FireUp the Audio Child */

	/*** Wait for first XA_IPC_OK Acknowlege */
  if (xa_forkit == xaTRUE)	xa_forkit = XA_Video_Receive_Ack(5000);

  if (xa_forkit == xaTRUE)
	xa_forkit = XA_Video_Send2_Audio(XA_IPC_HELLO,NULL,0,0,2000,0);

#else  /* NO AUDIO */
 xa_vaudio_present	= XA_AUDIO_NONE;
 xa_vaudio_enable	= xaFALSE;
 xa_vaudio_status	= XA_AUDIO_NICHTDA;
#endif

 vaudiof = (XA_AUD_FLAGS *)malloc( sizeof(XA_AUD_FLAGS) );
 if (vaudiof==0) TheEnd1("vaudiof failure\n");

 cmap_color_func = 0;
 cmap_sample_cnt = 5; /* default value */

 xa_has_audio	= xaFALSE;
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
 x11_pack_flag	= xaFALSE;
 x11_expose_flag = xaFALSE;
 x11_error_possible = 0;

 first_file = cur_file = 0;
 xa_file_num = -1;

 xa_anim_holdoff	= xaTRUE;
 xa_anim_status		= XA_UNSTARTED;
 xa_old_status		= XA_UNSTARTED;
 xa_anim_ready		= xaFALSE;
 xa_remote_ready	= xaFALSE;

 xa_audio_start		= 0;
 xa_time_reset		= 0;

 vaudiof->device		= DEFAULT_AUDIO_DEVICE_NAME;
 vaudiof->scale		= 1.0;
 vaudiof->mute		= xaFALSE;
 vaudiof->volume		= DEFAULT_XA_AUDIO_VOLUME;
 if (vaudiof->volume > XA_AUDIO_MAXVOL) vaudiof->volume = XA_AUDIO_MAXVOL;
 vaudiof->newvol	= xaTRUE;
 vaudiof->playrate	= 0;
 vaudiof->port 		= XA_Audio_Speaker("SPEAKER");
 /* if DEFAULT_XA_AUDIO_PORT is 0, then XAnim doesn't reset it */
 if (vaudiof->port == 0) vaudiof->port = DEFAULT_XA_AUDIO_PORT;
 vaudiof->divtest	= 2;
 vaudiof->fromfile	= xaFALSE;
 vaudiof->bufferit	= xaFALSE;
 XA_AUDIO_SET_VOLUME(vaudiof->volume);
 XA_AUDIO_SET_MUTE(vaudiof->mute);
 XA_AUDIO_SET_RATE(vaudiof->playrate);

 xa_skip_flag		= xaTRUE;
 xa_skip_video		= 0;
 xa_skip_diff		= 0;
 xa_skip_cnt		= 0;

 xa_buffer_flag		= DEFAULT_BUFF_FLAG;
 if (xa_buffer_flag == xaTRUE) xa_file_flag = xaFALSE;
 else xa_file_flag	= DEFAULT_FILE_FLAG;
 x11_shared_flag	= xaTRUE;
 x11_multibuf_flag	= xaTRUE;
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
 x11_kludge_1		= xaFALSE;
 cmap_luma_sort		= DEFAULT_CMAP_LUMA_SORT;
 cmap_map_to_1st_flag	= DEFAULT_CMAP_MAP_TO_1ST;
 cmap_map_to_one_flag	= DEFAULT_CMAP_MAP_TO_ONE;
 cmap_play_nice		= DEFAULT_CMAP_PLAY_NICE;
 xa_allow_nice		= xaTRUE;
 cmap_hist_flag		= DEFAULT_CMAP_HIST_FLAG;
 cmap_dither_type	= DEFAULT_CMAP_DITHER_TYPE;

 cmap_median_type	= DEFAULT_CMAP_MEDIAN_TYPE;
 cmap_median_bits	= 6;
 cmap_use_combsort	= xaTRUE;
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
 xa_gamma_flag		= xaFALSE;

 xa_title_flag		= DEFAULT_XA_TITLE_FLAG;
 xa_exit_flag		= DEFAULT_XA_EXIT_FLAG;
 xa_exit_early_flag	= xaFALSE;
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
 xa_frames_skipd	= xa_frames_Sskipd	= 0;
 xa_time_num		= 0;
 xa_no_disp		= xaFALSE;
 xa_time_flag		= xaFALSE;
 pod_max_colors		= 0;
 xa_r_shift = xa_g_shift = xa_b_shift = 0;
 xa_r_mask = xa_g_mask = xa_b_mask = 0;
 xa_gray_bits = xa_gray_shift = 0;
 xa_ham_map_size = 0;
 xa_ham_map = 0;
 xa_ham_chdr = 0;
 xa_ham_init = 0;
 xa_kludge1_fli = xaFALSE;
 xa_pause_hdr = 0;
 xa_pause_last = xaFALSE;

/* SMR 3 */
  xa_window_x = 0;
  xa_window_y = 0;
  xa_window_id = 0;
  xa_window_propname = "XANIM_PROPERTY";
  xa_window_center_flag = xaFALSE;
  xa_window_prepare_flag = xaFALSE;
  xa_window_refresh_flag = xaTRUE;
/* end SMR 3 */

 xa_gamma_flag = CMAP_Gamma_Adjust(xa_gamma_adj,xa_disp_gamma,xa_anim_gamma);

 if (DEFAULT_IFF_LOOP_OFF  == xaTRUE) xa_anim_flags |= ANIM_NOLOOP;
 if (DEFAULT_CYCLE_IMAGE_FLAG == xaTRUE) xa_anim_flags |= ANIM_CYCLE;

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

/* SMR 4 */
/* Get the other applications window id.  Required to properly init
   visual and colormap */
   if ( (in[0]=='+') && (in[1]=='W') && (in[2] >= '0') && (in[2] <= '9') )
   {
     xa_window_id = strtol(&in[2], NULL, 0);
     xa_noresize_flag = xaTRUE;
   }
/* end SMR 4 */
   else if ( ((in[0]=='-') || (in[0]=='+')) && (in[1]=='V') )
   {
     if ( (in[2] >= '0') && (in[2] <= '9') )  /* number */
		xa_user_visual = atoi(&in[2]);
     else	xa_user_class = XA_Get_Class( &in[2] );
   }
   else if ( (in[0]=='-') && (in[1]=='X') ) x11_verbose_flag = xaFALSE;
   else if ( (in[0]=='+') && (in[1]=='X') ) x11_verbose_flag = xaTRUE;
   else if ( (in[0]=='+') && (in[1]=='B') ) x11_shared_flag = xaTRUE;
   else if ( (in[0]=='-') && (in[1]=='B') ) x11_shared_flag = xaFALSE;
   else if ( (in[0]=='+') && (in[1]=='D') ) x11_multibuf_flag = xaTRUE;
   else if ( (in[0]=='-') && (in[1]=='D') ) x11_multibuf_flag = xaFALSE;
   else if ( (in[0]=='+') && (in[1]=='q') ) xa_quiet = xaTRUE;
   else if ( (in[0]=='-') && (in[1]=='q') ) xa_quiet = xaFALSE;
   else if ( (in[0]=='+') && (in[1]=='v') ) xa_verbose = xaTRUE;
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
   else if ( ((in[0]=='-') || (in[0]=='+')) && (strcmp(in+1,"root")==0) )
							xa_root = xaTRUE;
 }

/* What REV are we running
 */
 if ( (xa_quiet==xaFALSE) || (xa_verbose==xaTRUE) || (xa_debug >= 1) )
    fprintf(stdout,"XAnim Rev %d.%3.1f by Mark Podlipec Copyright (C) 1991-1999. All Rights Reserved\n",DA_MAJOR_REV,DA_MINOR_REV);

/* quick command line check.
 */
 if (argc<2) Usage_Quick();


 /* PreSet of X11 Display to find out what we're dealing with
  */
 DEBUG_LEVEL5 (void)fprintf(stdout,"Calling X11_Pre_Setup()\n");
 X11_Init(&argc, argv);
 X11_Pre_Setup(xa_user_visual, xa_user_class);

#ifdef XA_AUDIO
 if (xa_forkit == xaTRUE)	XA_Setup_BOFL();
#endif

 if (xa_allow_nice == xaFALSE) cmap_play_nice = xaFALSE;
 if ( (x11_bits_per_pixel == 2) || (x11_bits_per_pixel == 4) )
						x11_pack_flag = xaTRUE;
  if (x11_display_type == XA_MONOCHROME) xa_dither_flag = DEFAULT_DITHER_FLAG;
  else if (   (x11_display_type == XA_PSEUDOCOLOR)
	   || (x11_display_type == XA_STATICCOLOR) )
  {
    if (cmap_color_func == 4) xa_dither_flag = xaTRUE;
    else if (cmap_color_func == 5) xa_dither_flag = xaFALSE;
    else xa_dither_flag = DEFAULT_DITHER_FLAG;
  }
  else xa_dither_flag = xaFALSE;

 /* Audio Setup */
 DEBUG_LEVEL5 (void)fprintf(stdout,"Calling XA_Audio_Setup()\n");
 XA_AUDIO_SETUP;

 /* Visual Dependent switches and flags */
 if (x11_display_type & XA_X11_TRUE)
 {
   cmap_true_to_332  = xaFALSE; cmap_true_to_gray = xaFALSE;
   cmap_true_to_1st  = xaFALSE; cmap_true_to_all  = xaFALSE;
   cmap_true_map_flag = xaFALSE;
 }
 else if (x11_display_type & XA_X11_COLOR)
 {
   cmap_true_to_332  = DEFAULT_TRUE_TO_332;
   cmap_true_to_gray = DEFAULT_TRUE_TO_GRAY;
   cmap_true_to_1st  = DEFAULT_TRUE_TO_1ST;
   cmap_true_to_all  = DEFAULT_TRUE_TO_ALL;
   if (cmap_true_to_1st || cmap_true_to_all) cmap_true_map_flag = xaTRUE;
   else cmap_true_map_flag = DEFAULT_TRUE_MAP_FLAG;
 }
 else { cmap_true_to_332  = xaFALSE; cmap_true_to_gray = xaTRUE; 
        cmap_true_to_1st  = xaFALSE; cmap_true_to_all  = xaFALSE;
        cmap_true_map_flag = xaFALSE;  }

/* Is here good? TODO */
 Init_Video_Codecs();

 xa_time_start = XA_Time_Read();

/* Parse command line.
 */
 for(i=1; i < argc; i++)
 {
   in = argv[i];
   if ( ((in[0] == '-') || (in[0] == '+')) && (in[1] != '\0'))
   {
     xaLONG len,opt_on;

     if (in[0] == '-') opt_on = xaFALSE;
     else opt_on = xaTRUE;
     /* if + turns off then reverse opt_on */
     if (DEFAULT_PLUS_IS_ON == xaFALSE) opt_on = (opt_on==xaTRUE)?xaFALSE:xaTRUE;

     len = strlen(argv[i]);
     j = 1;
     while( (j < len) && (in[j] != 0) )
     {
       switch(in[j])
       {
        case 'A':   /* audio options sub menu */
	{
	  xaULONG exit_flag = xaFALSE;
	  j++;
	  while( (exit_flag == xaFALSE) && (j<len) )
	  {
	    switch(in[j])
	    {
	      case 'b':   /* snd buffer on/off */
		j++; vaudiof->bufferit = opt_on;
		break;
	      case 'e':   /* snd enable on/off */
		j++; xa_vaudio_enable = opt_on;
		break;
	      case 'k':   /* toggle skip video */
		j++; xa_skip_flag = opt_on;
		break;
	      case 'p':   /* select audio port */
		{ xaULONG t,val;
		  j++; t = XA_Read_Int(in,&j);
		  switch(t)
		  {
		    case 0: val = XA_AUDIO_PORT_INT; break;
		    case 1: val = XA_AUDIO_PORT_HEAD; break;
		    case 2: val = XA_AUDIO_PORT_EXT; break;
		    default: val = 0;
		  }
		  if (opt_on == xaTRUE) vaudiof->port |= val;
		  else vaudiof->port &= ~(val);
		}
		break;
	      case 'm':  /* Merge audio into previous */
		j++; xa_vaudio_merge = opt_on;
		break;
	      case 'M':  /* Merge audio into previous and scale timing */
		j++; xa_vaudio_merge_scale = opt_on;
		break;
	      case 'd':   /* POD TEST */
		j++; vaudiof->divtest = XA_Read_Int(in,&j);
		if (vaudiof->divtest==0) vaudiof->divtest = 2;
		break;
	      case 'D':   /* s/6000 audio device name */
	      {
		j++; vaudiof->device = &in[j];
		j = len;
	      }
	      break;
	      case 'r':   /* POD TEST */
		j++; vaudiof->playrate = XA_Read_Int(in,&j);
		if (vaudiof->playrate==0) vaudiof->playrate = 8000;
		XA_AUDIO_SET_RATE(vaudiof->playrate);
		break;
	      case 's':   /* snd scale */
		j++; vaudiof->scale = XA_Read_Float(in,&j);
		fprintf(stdout,"XAnim: +As# temporarily disabled.\n");
		if (vaudiof->scale < 0.125) vaudiof->scale = 0.125;
		if (vaudiof->scale > 8.000) vaudiof->scale = 8.000;
		break;
	      case 'v':
		j++; vaudiof->volume = XA_Read_Int(in,&j);
		if (vaudiof->volume > XA_AUDIO_MAXVOL) 
					vaudiof->volume = XA_AUDIO_MAXVOL;
		XA_AUDIO_SET_VOLUME(vaudiof->volume);
		break;
	      default: exit_flag = xaTRUE;
	    }
	  }
	}
	break;
        case 'C':   /* colormap options sub menu */
	  {
	    xaULONG exit_flag = xaFALSE;
	    j++;
	    while( (exit_flag == xaFALSE) && (j<len) )
	    {
		switch(in[j])
		{
		  case 's':
			j++; cmap_sample_cnt = XA_Read_Int(in,&j);
			break;
		  case 'F':
			j++; cmap_color_func = XA_Read_Int(in,&j);
			if (cmap_color_func==4) xa_dither_flag = xaTRUE;
			else if (cmap_color_func==5)
			{ xa_dither_flag = xaFALSE;
			  cmap_color_func = 4;
			}
			break;
		  case '1':
 			if (   (x11_display_type & XA_X11_TRUE)
 			    || (x11_display_type == XA_MONOCHROME) ) 
							opt_on = xaFALSE;
			cmap_true_to_1st  = opt_on;
			if (cmap_true_to_1st == xaTRUE)
			{
			  cmap_true_to_gray = xaFALSE; cmap_true_to_332  = xaFALSE;
			  cmap_true_to_all  = xaFALSE; cmap_true_map_flag = xaTRUE;
			}
			j++; break;
		  case 'A':
 			if (   (x11_display_type & XA_X11_TRUE)
 			    || (x11_display_type == XA_MONOCHROME) ) 
							opt_on = xaFALSE;
			cmap_true_to_all  = opt_on;
			if (cmap_true_to_all == xaTRUE)
			{
			  cmap_true_to_gray = xaFALSE; cmap_true_to_332  = xaFALSE;
			  cmap_true_to_1st  = xaFALSE; cmap_true_map_flag = xaTRUE;
			}
			j++; break;
		  case '3':
 			if (   (x11_display_type & XA_X11_TRUE)
 			    || (!(x11_display_type & XA_X11_COLOR))
 			    || (x11_display_type == XA_MONOCHROME) ) 
							opt_on = xaFALSE;
			cmap_true_to_332  = opt_on;
			if (opt_on == xaTRUE)
			{
			  cmap_true_to_gray = xaFALSE; cmap_true_to_1st  = xaFALSE;
			  cmap_true_to_all  = xaFALSE;
			}
			j++; break;
		  case 'g':
 			if (x11_display_type & XA_X11_TRUE) opt_on = xaFALSE;
			cmap_true_to_gray  = opt_on;
			if (opt_on == xaTRUE)
			{
			  cmap_true_to_332 = xaFALSE; cmap_true_to_1st = xaFALSE;
			  cmap_true_to_all = xaFALSE;
			}
			j++; break;
		  case 'a': cmap_map_to_one_flag = opt_on; j++; break;
		  case 'd': cmap_dither_type = CMAP_DITHER_FLOYD; j++; break;
		  case 'l': cmap_luma_sort  = opt_on; j++; break;
		  case 'f': cmap_map_to_1st_flag = opt_on; j++; break;
		  case 'm': cmap_true_map_flag = opt_on; j++; break;
		  case 'n': j++;
		    if (xa_allow_nice == xaTRUE) cmap_play_nice  = opt_on;
		    break;
		  case 'h': cmap_hist_flag  = opt_on; j++; break;
		  default: exit_flag = xaTRUE;
		}
	    }
	  }
          break;
	case 'G': /* gamma options sub-menu */ 
	  {
	    xaULONG exit_flag = xaFALSE;
	    j++;
	    while( (exit_flag == xaFALSE) && (j<len) )
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
		  default: exit_flag = xaTRUE;
		}
	    }
	  }
          break;

	case 'M': /* median cut options sub-menu */ 
	  {
	    xaULONG exit_flag = xaFALSE;
	    j++;
	    while( (exit_flag == xaFALSE) && (j<len) )
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
		  case 'q': if (opt_on == xaTRUE) cmap_use_combsort = xaFALSE;
			    else cmap_use_combsort = xaTRUE;
			    j++; break;
		  default: exit_flag = xaTRUE;
		}
	    }
	  }
          break;
        case 'B':
                x11_shared_flag = opt_on;	j++;
                break;
        case 'D':
                x11_multibuf_flag = opt_on;	j++;
                break;
        case 'b':
                xa_buffer_flag = opt_on;	j++;
		if (xa_buffer_flag==xaTRUE) xa_file_flag = xaFALSE;
                break;
        case 'c':
                if (opt_on==xaTRUE) xa_anim_flags |= ANIM_NOLOOP;
                else xa_anim_flags &= (~ANIM_NOLOOP);
                j++; break;
        case 'd':
                j++; xa_debug = XA_Read_Int(in,&j);
                if (xa_debug <= 0) xa_debug = 0;
		break;
        case 'f':
                xa_file_flag = opt_on;	j++;
		if (xa_file_flag==xaTRUE) xa_buffer_flag = xaFALSE;
		break;
        case 'F':
                xa_dither_flag = opt_on;	j++;
		if (x11_display_type & XA_X11_TRUE) xa_dither_flag = xaFALSE;
		break;
        case 'h':
                Usage(); break;
	case 'J':  /*PODNOTE: make into defines */
		j++; xa_speed_scale = XA_Read_Int(in,&j);
		if (xa_speed_scale == 0) xa_speed_scale = 1;
		if (xa_speed_scale > 16) xa_speed_scale = 16;
		if (opt_on == xaTRUE) xa_speed_scale <<= 4;
		else	xa_speed_scale = (1<<4) / xa_speed_scale;
		break;
        case 'j':
                j++;
		if ( (in[j] >= '0') && (in[j] <= '9') )
		{ xa_jiffy_flag = XA_Read_Int(in,&j);
                  if (xa_jiffy_flag < 0) xa_jiffy_flag = 1;
		}
		else xa_jiffy_flag = 0; /* off */
                break;
        case 'k':
		{ xaULONG tmp;
                  j++;
		  if ( (in[j] >= '0') && (in[j] <= '9') )
		  { tmp = XA_Read_Int(in,&j);
                    if (tmp==1) xa_kludge1_fli = opt_on;
                    if (tmp==2) 
		    {	xa_kludge2_dvi = opt_on;
			XA_AUDIO_SET_KLUDGE2(xa_kludge2_dvi);
		    }
                    if (tmp >= 900) 
		    {	xa_kludge900_aud = tmp;
			XA_AUDIO_SET_KLUDGE900(xa_kludge900_aud);
		    }
		  }
		}
                break;
        case 'l':
		j++; 
		if (in[j] == 'p') { xa_pingpong_flag = opt_on; j++; }
		else xa_pingpong_flag = xaFALSE;
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
		if (opt_on==xaTRUE) xa_anim_flags |= ANIM_PIXMAP;
		else xa_anim_flags &= ~ANIM_PIXMAP;
                break;
        case 'P':
                j++; pod_max_colors = XA_Read_Int(in,&j);
                if (pod_max_colors <= 0) pod_max_colors = 0;
		break;
        case 'r':
		if ( (j <= (len - 4)) && (strncmp( &in[j], "root", 4)==0) )
		{ /* Already looked at this, ignore here */
		  j+=4;
		  break;
		}
                if (opt_on == xaTRUE)	xa_anim_flags |= ANIM_CYCLE;
                else			xa_anim_flags &= (~ANIM_CYCLE);
                j++;
                break;
        case 'R':
/* NO More - burning up argument space 
                xa_anim_cycling = opt_on; j++;	break;
*/
		if ((opt_on == xaTRUE) && (j <= (len - 1)) )
		{ j++;
		  xa_raw_format = &in[j];
		  j += len;
		}
		else
		{ j++;
		  xa_raw_format = 0;
		}
		break;

        case 'S':
	  {
	    xaULONG exit_flag = xaFALSE;
	    j++;
	    while( (exit_flag==xaFALSE) && (j<len) )
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
		  default: exit_flag = xaTRUE;
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
/* SMR 5 */
        case 'W':
          {
            xaULONG exit_flag = xaFALSE;
            j++;
            while( (exit_flag==xaFALSE) && (j<len) )
            {
                switch(in[j])
                {
                  case 'c':
                    j++;
                    xa_window_center_flag = xaTRUE;
                    break;
                  case 'd':
                    j++;
                    xa_window_refresh_flag = xaFALSE;
                    break;
                  case 'n':
                    j++;
                    xa_window_propname = &in[j];
                      j = len;
                    break;
                  case 'p':
                    j++;
                    xa_window_prepare_flag = xaTRUE;
                    break;
                  case 'r':
                    j++;
                    xa_noresize_flag = xaFALSE;
                    xa_allow_resizing = xaTRUE;
                    break;
                  case 'x':
                    j++;
                    xa_window_x = XA_Read_Int(in,&j);
                    break;
                  case 'y':
                    j++;
                    xa_window_y = XA_Read_Int(in,&j);
                    break;
                  default:
                    if ( (in[j] >= '0') && (in[j] <= '9') ) j = len;
                    else exit_flag = xaTRUE;
                }
              }
          }
           break;
/* end SMR 5 */
        case 'Z':
	  {
	    xaULONG exit_flag = xaFALSE;
	    j++;
	    while( (exit_flag==xaFALSE) && (j<len) )
	    { xaULONG tmp;
		switch(in[j])
		{
		  case 'e': xa_exit_flag = opt_on;	j++;	break;
		  case 'p':
		    j++;
		    if (in[j] == 'e') {xa_pause_last = opt_on; j++; }
		    else { tmp = XA_Read_Int(in,&j); XA_Add_Pause(tmp); }
		    break;
		  case 'r': xa_remote_flag = opt_on;	j++;	break;
		  case 'v': xa_exit_early_flag = opt_on; j++;	break;
		  case 'z': /* POD TEST SWITCH FOR TRY DIFFERENT THINGS */
		    j++; pod = XA_Read_Int(in,&j);
		    break;
		  default: exit_flag = xaTRUE;
		}
	    }
	  }
	  break;
        case '-':
                opt_on = (DEFAULT_PLUS_IS_ON==xaTRUE)?xaFALSE:xaTRUE; j++;	break;
        case '+':
                opt_on = (DEFAULT_PLUS_IS_ON==xaTRUE)?xaTRUE:xaFALSE; j++;	break;
        default:
                Usage_Quick();
       } /* end of option switch */
     } /* end of loop through options */
   } /* end of if - */
   else 
   /* If no hyphen in front of argument, assume it's a file.
    */
   { xaULONG result,audio_attempt;
     filename = argv[i];

     /* Visual Dependent switches and flags */
     if (x11_display_type & XA_X11_TRUE)
     {
       cmap_true_to_332  = cmap_true_to_gray = xaFALSE;
       cmap_true_to_1st  = cmap_true_to_all  = cmap_true_map_flag = xaFALSE;
     }
     else if (!(x11_display_type & XA_X11_COLOR))
     { cmap_true_to_gray = xaTRUE; cmap_true_to_332  = xaFALSE;
       cmap_true_to_1st  = cmap_true_to_all  = cmap_true_map_flag = xaFALSE;
     }
    
       /* disable audio if timing is forced */
       /* pass audio flag to routines */
     if ( (xa_vaudio_enable == xaTRUE) && (!xa_jiffy_flag) &&
         ((xa_vaudio_present==XA_AUDIO_UNK) || (xa_vaudio_present==XA_AUDIO_OK))
        ) audio_attempt = xaTRUE;
     else audio_attempt = xaFALSE;

     XA_AUDIO_SET_ENABLE(audio_attempt);
     XA_AUDIO_SET_FFLAG(xa_file_flag);
     XA_AUDIO_SET_BFLAG(vaudiof->bufferit);


     if ( (x11_display_type == XA_MONOCHROME) || (xa_pack_flag == xaTRUE) )
       xa_use_depth_flag = xaFALSE;
     else
       xa_use_depth_flag = xaTRUE;
     cur_file = Get_Anim_Hdr(cur_file,filename);
	/* POD why the voids? */
     (void)XA_Alloc_Input_Methods(cur_file);
     (void)XA_Setup_Input_Methods(cur_file->xin,cur_file->name);

     xa_anim_type = XA_Open_And_ID_File(cur_file);
     cur_file->anim_type = xa_anim_type;
     cur_file->anim_flags = xa_anim_flags;
     if (x11_display_type == XA_MONOCHROME) xa_optimize_flag = xaFALSE;
     switch(xa_anim_type)
     {
        case XA_OLDQT_ANIM:	/* potentially multiple files */
	case XA_JFIF_ANIM: /* cnvrt'd but not sequential */
	case XA_MPG_ANIM:  /* cnvrt'd but not sequential */
        case XA_ARM_ANIM:  /* cnvrt'd but not sequential */
        case XA_SGI_ANIM:	/* cnvrt'd but not sequential */
        case XA_TXT_ANIM:	/* used fscanf */
        case XA_JMOV_ANIM:	/* cnvrt'd but not sequential */
        case XA_AU_ANIM:
	/* New Style that does NOT support Sequential */
	  if (xa_buffer_flag)	 cur_file->xin->load_flag = XA_IN_LOAD_BUF;
	  else if (xa_file_flag) cur_file->xin->load_flag = XA_IN_LOAD_FILE;
	  else			 cur_file->xin->load_flag = XA_IN_LOAD_MEM;

/* POD FIX: this random stuff should be automatically done in Open_And_ID */
	  if (cur_file->xin->type_flag & XA_IN_TYPE_RANDOM)
 	  { cur_file->xin->Seek_FPos(cur_file->xin,0,0);
	    cur_file->xin->buf_cnt = 0;
	    cur_file->xin->fpos = 0;
	    if (xa_verbose) fprintf(stdout,"Reading %s File %s\n",
						cur_file->desc, filename);
	    result = cur_file->Read_File(filename,cur_file,audio_attempt);
	  }
	  else
	  { fprintf(stdout,
		"%s Files not yet supported for Sequential input\n",
							cur_file->desc);
	    result = xaFALSE;
	  }
	  break;

	/* New Style that supports Sequential */
	case XA_RAW_ANIM:
        case XA_SET_ANIM:	/* potentially multiple files */
        case XA_RLE_ANIM:
        case XA_IFF_ANIM:
        case XA_DL_ANIM:
        case XA_GIF_ANIM:
        case XA_FLI_ANIM:
        case XA_AVI_ANIM:
        case XA_QT_ANIM:
        case XA_WAV_ANIM:
        case XA_J6I_ANIM: /* maybe not sequential */
	case XA_8SVX_ANIM:
	  if (xa_buffer_flag)	 cur_file->xin->load_flag = XA_IN_LOAD_BUF;
	  else if (xa_file_flag) cur_file->xin->load_flag = XA_IN_LOAD_FILE;
	  else			 cur_file->xin->load_flag = XA_IN_LOAD_MEM;
	  if (cur_file->xin->type_flag & XA_IN_TYPE_RANDOM)
 	  { cur_file->xin->Seek_FPos(cur_file->xin,0,0);
	    cur_file->xin->buf_cnt = 0;
	    cur_file->xin->fpos = 0;
	  }
	  else  
          { if (xa_file_flag)
	    { fprintf(stdout,
		"You can't use \"+f\" option with sequential streams(stdin,ftp,etc).\n");
/* TODO: biff from file flag instead of failing */
	      result = xaFALSE;
	      break;
	    }
	  }
	  if (xa_verbose) fprintf(stdout,"Reading %s File %s\n",
						cur_file->desc, filename);
	  result = cur_file->Read_File(filename,cur_file,audio_attempt);
	  break;

        case XA_PFX_ANIM:
	  fprintf(stdout,"%s not currently supported\n",cur_file->desc);
	  result = xaFALSE;
	  break;
        case NOFILE_ANIM:
	  fprintf(stdout,"Unable to open/read animation: %s\n",filename);
	  result = xaFALSE;
	  break;
        default:
	  fprintf(stdout,"Unknown or unsupported animation type: %s\n",
								filename);
	  result = xaFALSE;
	  break;
      } 
	/* not supported OR no video or audio */
      if (result == xaFALSE) 
      {
	XA_AUDIO_UNFILE(cur_file->file_num);
	cur_file = Return_Anim_Hdr(cur_file);
      }
      else if (cur_file->frame_lst == 0)  /* No Video but with audio */
      { 
	/* If no previous file, add dummy video and move on */
	if (   (cur_file->prev_file == 0)
	    || (cur_file->prev_file == cur_file)
           )
        { DUM_Read_File(filename,cur_file);
          goto XA_MERGE_NOGO;
        }

	/* only add if previous file has no audio unless overridden
 	 * by vaudio_merge flags */
	{ int f_ret = xaFALSE;
          XA_AUDIO_N_FILE(cur_file->prev_file->file_num,f_ret);
          if (   (xa_forkit==xaTRUE) && (f_ret == xaTRUE)
	      && (xa_vaudio_merge == xaFALSE)
	      && (xa_vaudio_merge_scale == xaFALSE)
	    )
          { DUM_Read_File(filename,cur_file); 
	    goto XA_MERGE_NOGO;
	  }
	}
	/* Now we're free to merge audio */

	/* Set current file name to previous file name for Audio only */
	/* NOTE: this'll set AudioChild's cur_audio_hdr to prev_file */
	if (cur_file->fname)
	{ xaULONG length = strlen( cur_file->fname ) + 1;
	  XA_AUDIO_FNAME(cur_file->fname, length, 
					cur_file->prev_file->file_num);
	}
	XA_AUDIO_MERGEFILE(cur_file->file_num);
	if (xa_vaudio_merge_scale==xaTRUE)
	   XA_Adjust_Video_Timing(cur_file->prev_file,cur_file->total_time);
	/* these are one shot options */
	xa_vaudio_merge_scale = xa_vaudio_merge = xaFALSE;
	cur_file = Return_Anim_Hdr(cur_file);
	
/* what's this here for??? */
	if (cur_file->max_faud_size > xa_audcodec_maxsize)
			xa_audcodec_maxsize = cur_file->max_faud_size;
      }
      else
      { xaULONG tmpx,tmpy;

XA_MERGE_NOGO:

        /* Setup up anim header.  */
        cur_file->loop_num = xa_loop_each_flag;

        if (xa_pause_last == xaTRUE)
        { if ( (xa_anim_type == XA_FLI_ANIM) && (cur_file->last_frame > 1))
					XA_Add_Pause(cur_file->last_frame-2);
          else XA_Add_Pause(cur_file->last_frame);
          xa_pause_last = xaFALSE;
        }
	cur_file->pause_lst = xa_pause_hdr;
	xa_pause_hdr = 0;

	xa_imagex = cur_file->imagex;
	xa_imagey = cur_file->imagey;
	if (xa_imagex > xa_max_imagex) xa_max_imagex = xa_imagex;
	if (xa_imagey > xa_max_imagey) xa_max_imagey = xa_imagey;

	if (cur_file->max_fvid_size > xa_vidcodec_maxsize)
			xa_vidcodec_maxsize = cur_file->max_fvid_size;
	if ((cur_file->fname) && (xa_file_flag == xaTRUE))
	{ xaULONG length = strlen( cur_file->fname ) + 1;
	  XA_AUDIO_FNAME( (cur_file->fname), length, (cur_file->file_num) );
	}
	XA_AUDIO_SET_AUD_BUFF( (cur_file->max_faud_size) );
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
	  tmpx = (xaULONG)((float)(xa_imagex) * xa_bscalex);
	  if (tmpx == 0) tmpx = xa_imagex;
	  tmpy = (xaULONG)((float)(xa_imagey) * xa_bscaley);
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
	  tmpx = (xaULONG)((float)(xa_imagex) * xa_scalex);
	  if (tmpx == 0) tmpx = xa_imagex;
	  tmpy = (xaULONG)((float)(xa_imagey) * xa_scaley);
	  if (tmpy == 0) tmpy = xa_imagey;
	}
	/* handle any IFF laced images */
	if ( (xa_allow_lace==xaTRUE) && (cur_file->anim_flags & ANIM_LACE))
		tmpy >>= 1;
	else	cur_file->anim_flags &= ~ANIM_LACE;
	cur_file->dispx = tmpx;
	cur_file->dispy = tmpy;
	if (tmpx > xa_max_disp_x) xa_max_disp_x = tmpx;
	if (tmpy > xa_max_disp_y) xa_max_disp_y = tmpy;

	if ((cmap_dither_type == CMAP_DITHER_FLOYD) && (xa_buffer_flag==xaFALSE))
			cur_file->anim_flags |= ANIM_3RD_BUF;
	xa_merged_anim_flags |= cur_file->anim_flags;

	/* NOTE: removed fade, remember to readd eventually */

	if (xa_time_flag == xaTRUE)
	{
	  xaLONG time_int;
	  xa_time_end = XA_Time_Read();
	  time_int = xa_time_end - xa_time_start;
	  fprintf(stdout,"time = %d\n",time_int);
	  xa_time_start = XA_Time_Read();
	}
      } /* valid animation file */
    } /* end of read in file */
 } /* end of loopin through arguments */

 /* No anims listed
  */
 if (first_file == 0) Usage_Quick();

 if (xa_exit_early_flag == xaTRUE) TheEnd();

 /* If No Audio then Kill Audio Child
  */
  if (   (xa_vaudio_present==XA_AUDIO_NONE) 
      || (xa_vaudio_present==XA_AUDIO_ERR) ) XA_AUDIO_EXIT();

 /* Set up X11 Display
  */

 if (xa_noresize_flag==xaTRUE) X11_Setup_Window(xa_max_disp_x,xa_max_disp_y,
		xa_max_disp_x,xa_max_disp_y);
 else X11_Setup_Window(xa_max_disp_x,xa_max_disp_y,
		first_file->dispx, first_file->dispy);
 if (x11_display_type == XA_MONOCHROME) xa_optimize_flag = xaFALSE;
 
 /* color map manipulation */
 CMAP_Manipulate_CHDRS();

 /* Kludge for some X11 displays */
 if (x11_kludge_1 == xaTRUE) CMAP_Expand_Maps();

 xa_time_start = XA_Time_Read();
 cur_file = first_file;
 while(cur_file)
 {
   ACT_Make_Images(cur_file->acts);
   cur_file = cur_file->next_file;
   if (cur_file == first_file) cur_file = 0;
 }
 if (xa_time_flag == xaTRUE)
 {
   xaLONG time_int;
   xa_time_end = XA_Time_Read();
   time_int = xa_time_end - xa_time_start;
   fprintf(stdout,"ACT_Make_Images: time = %d\n",time_int);
   xa_time_start = XA_Time_Read();
 }

 /* Set Audio Ports */
#ifdef XA_AUDIO
  if (xa_vaudio_present == XA_AUDIO_OK)
  {
#ifdef XA_SPARC_AUDIO
    if (vaudiof->port) XA_SET_OUTPUT_PORT(vaudiof->port);
#else
    if (vaudiof->port & XA_AUDIO_PORT_INT) XA_SPEAKER_TOG(1)
    else XA_SPEAKER_TOG(0)
	/*** POD NOTE: HEADPHONES AND LINEOUT MUST BE DIFFERENT */
    if (vaudiof->port & XA_AUDIO_PORT_HEAD) XA_HEADPHONE_TOG(1)
    else XA_HEADPHONE_TOG(0)
    if (vaudiof->port & XA_AUDIO_PORT_EXT) XA_LINEOUT_TOG(1)
    else XA_LINEOUT_TOG(0)
#endif
  }
#endif

 /* Start off Animation.
  */
  XA_Lets_Get_Looped();
 /* Map window and then Wait for user input.
  */
  X11_Map_Window();
#ifdef XA_REMOTE_CONTROL
  XA_Create_Remote(theWG,xa_remote_flag);
#else
  xa_remote_ready = xaTRUE;
#endif
  xanim_events();

  XSync(theDisp,False);


 /* Self-Explanatory
  */
  TheEnd();
  return(0);
}


/*
 * XA_Lets_Get_Looped allocates and sets up required image buffers and
 * initializes animation variables.
 * It then kicks off the animation. Hey, the snow is melting today!
 */
void XA_Lets_Get_Looped()
{ /* POD TEMPORARY PARTIAL KLUDGE UNTIL REAL FIX IS IMPLEMENTED */
  if (xa_max_imagex & 0x03) shm = 0;

  xa_max_image_size = xa_max_imagex * xa_max_imagey;
  xa_max_disp_size = xa_max_disp_x * xa_max_disp_y;


  if (   (xa_merged_anim_flags & ANIM_SNG_BUF)
      || (xa_merged_anim_flags & ANIM_DBL_BUF)
     )
  { 
#ifdef XSHM
    if (XA_Setup_Them_Buffers(&im0_Image,&im0_shminfo,&im_buff0)==xaFALSE)
#else
    if (XA_Setup_Them_Buffers(&im_buff0)==xaFALSE)
#endif
				TheEnd1("Them_Buffers: im_buff0 malloc err");
  }
  if (xa_merged_anim_flags & ANIM_DBL_BUF)
  {
#ifdef XSHM
    if (XA_Setup_Them_Buffers(&im1_Image,&im1_shminfo,&im_buff1)==xaFALSE)
#else
    if (XA_Setup_Them_Buffers(&im_buff1)==xaFALSE)
#endif
				TheEnd1("Them_Buffers: im_buff1 malloc err");
  }
  if (xa_merged_anim_flags & ANIM_3RD_BUF)
  {
#ifdef XSHM
    if (XA_Setup_Them_Buffers(&im2_Image,&im2_shminfo,&im_buff2)==xaFALSE)
#else
    if (XA_Setup_Them_Buffers(&im_buff2)==xaFALSE)
#endif
				TheEnd1("Them_Buffers: im_buff2 malloc err");
  }

  if (x11_pack_flag == xaTRUE)
  {
    xaULONG tsize;
    xaULONG pbb = (8 / x11_bits_per_pixel);
    tsize = (xa_max_imagex + pbb - 1) / pbb;
    im_buff3 = (char *) malloc(xa_max_disp_y * tsize);
    if (im_buff3 == 0) TheEnd1("XAnim_Start_Loop: im_buff3 malloc err1");
  }
  xa_pic = im_buff0;
  cur_file = first_file;
  cur_floop = 0;
  cur_frame = -1;  /* since we step first */
  xa_audio_start = 0;
  xa_time_reset = 1;
  file_is_started = xaFALSE;
  xa_cycle_cnt = 0;
  xa_now_cycling = xaFALSE;

  xa_anim_ready = xaTRUE; /* allow expose event to start animation */
}

/*
 * This is the heart of this program. It does each action and then calls
 * itself with a timeout via the XtAppAddTimeOut call.
 */
void XAnim_Looped(dptr,id)
XtPointer dptr;
XtIntervalId *id;
{ xaULONG command = (xaULONG)dptr;
  xaLONG t_time_delta = 0;
  xaLONG snd_speed_now_ok = xaFALSE;

XAnim_Looped_Sneak_Entry:

 DEBUG_LEVEL2 fprintf(stdout,"SHOWACTION\n");

 if (command == XA_SHOW_NORM)
 {
  switch (xa_anim_status)
  {
    case XA_BEGINNING:  
	if ((xa_anim_ready == xaFALSE) || (xa_remote_ready == xaFALSE))
	{ /* try again 10ms later */
	  XtAppAddTimeOut(theContext,10, (XtTimerCallbackProc)XAnim_Looped,
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
	if (xa_old_status == XA_RUN_NEXT)
	{
#ifdef XA_AUDIO
	  XA_AUDIO_OFF(0);	xa_audio_start = 0;
#endif
	}
	break;
    case XA_ISTP_NEXT: 
#ifdef XA_AUDIO
	XA_AUDIO_OFF(0);	xa_audio_start = 0;
#endif
	Step_Frame_Next();
	break;
    case XA_ISTP_PREV: 
#ifdef XA_AUDIO
	XA_AUDIO_OFF(0);	xa_audio_start = 0;
#endif
	Step_Frame_Prev();
	break;
    case XA_FILE_NEXT:
#ifdef XA_AUDIO
	XA_AUDIO_OFF(0);	xa_audio_start = 0;
#endif
	Step_File_Next();
	xa_anim_status = XA_STEP_NEXT;
	break;
    case XA_FILE_PREV:
#ifdef XA_AUDIO
	XA_AUDIO_OFF(0);	xa_audio_start = 0;
#endif
	Step_File_Prev();
	xa_anim_status = XA_STEP_PREV;
	break;
    case XA_UNSTARTED:  
    case XA_STOP_NEXT:  
    case XA_STOP_PREV:  
#ifdef XA_AUDIO
	XA_AUDIO_OFF(0);	xa_audio_start = 0;
#endif
	if (xa_title_flag != XA_TITLE_NONE)
			XA_Store_Title(cur_file,cur_frame,XA_TITLE_FRAME);
	xa_anim_holdoff = xaFALSE;
	xa_old_status = xa_anim_status;
	return;
	break;
    default:
#ifdef XA_AUDIO
	XA_AUDIO_OFF(0);	xa_audio_start = 0;
#endif
	xa_anim_holdoff = xaFALSE;
	xa_old_status = xa_anim_status;
	return;
	break;
  } /* end of case */
 } /* end of command SHOW_NORM */

  DEBUG_LEVEL1 fprintf(stdout,"frame = %d\n",cur_frame);

 /* 1st throught this particular file.
  * Resize if necessary and init required variables.
  */
  if (file_is_started == xaFALSE)
  {
#ifdef XA_AUDIO

    XA_AUDIO_GET_STATUS( xa_vaudio_status );
    if (xa_vaudio_status == XA_AUDIO_STARTED)
    { XtAppAddTimeOut(theContext, 50, (XtTimerCallbackProc)XA_Audio_Wait,
							(XtPointer)(NULL) );
      return;
    }
#endif
     /* If previous anim is still cycling, wait for it to stop */
    if (xa_now_cycling == xaTRUE)
    { /*PODNOTE: This check might want to be before above switch */
      xa_anim_flags &= ~(ANIM_CYCLE);
      XtAppAddTimeOut(theContext, 50, (XtTimerCallbackProc)XA_Cycle_Wait,
							(XtPointer)(NULL) );
      return; 
    }

    file_is_started = xaTRUE;
    xa_anim_flags = cur_file->anim_flags;
    if (xa_anim_flags & ANIM_PIXMAP) xa_pixmap_flag = xaTRUE;
    else xa_pixmap_flag = xaFALSE;

	/* Resize Window if Needed */
    if (     (xa_allow_resizing == xaFALSE)
         || ( (xa_allow_resizing == xaTRUE) && (x11_window_x==0)))
	      XA_Resize_Window(cur_file,xa_noresize_flag,xa_disp_x,xa_disp_y);

	/* Change Title of Window */
    XA_Store_Title(cur_file,cur_frame,xa_title_flag);


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
        fprintf(stdout,"Open file %s for video err\n",cur_file->fname); 
        TheEnd();
      }
      /* Allocate video buffer if not done already */
      if ((cur_file->max_fvid_size) && (xa_vidcodec_buf==0))
      {
	xa_vidcodec_buf = (xaUBYTE *)malloc( xa_vidcodec_maxsize );
	if (xa_vidcodec_buf==0) TheEnd1("XAnim: malloc vidcodec_buf err");
      }
    }

    xa_pause_hdr = cur_file->pause_lst;
    xa_skip_cnt = xa_skip_video = xa_skip_diff = 0;
    /*** Call any Initializing routines needed by Audio/Video Codecs */
    if (cur_file->init_vid) cur_file->init_vid();
    /* is this still needed ?? */
    xa_time_start = XA_Time_Read();

    /* Is Audio Valid for this animations */
/* FORK NOTE: */
   { int f_ret = xaFALSE;

     XA_AUDIO_PLAY_FILE(cur_file->file_num,f_ret);
     if ( (xa_forkit==xaTRUE) && (f_ret == xaTRUE) )
     { xa_has_audio = xaTRUE;
       xa_vaudio_enable = xaTRUE;
       xa_vaudio_status = XA_AUDIO_STOPPED;
       XA_AUDIO_INIT_SND;
       xa_time_audio = 0;
     }
     else
     { xa_has_audio = xaFALSE;
       xa_time_audio = -1;
       xa_vaudio_enable = xaFALSE;
       xa_vaudio_status = XA_AUDIO_NICHTDA;
     }
   }
    xa_ptime_video = xa_time_video = 0;
    xa_av_time_off = XA_Time_Read();  /* for initial control frames if any */

    xa_old_status = XA_STOP_NEXT;
  } /* end of file is not started */

 
 /* OK. A quick sanity check and then we
  * initialize act and inc frame time and go to it.
  */
 if ( (act = cur_file->frame_lst[cur_frame].act) != 0)
 { xaULONG passed_go_flag = xaFALSE;

       /*** Time Adjustments */
    if (cur_file->frame_lst[cur_frame].zztime >= 0)
	xa_time_video = cur_file->frame_lst[cur_frame].zztime;

    if (!(xa_anim_status & XA_NEXT_MASK)) /* backwards */
    {
      xa_time_video += cur_file->frame_lst[cur_frame].time_dur;
      xa_time_video = cur_file->total_time - xa_time_video;
      if (xa_ptime_video > xa_time_video) passed_go_flag = xaTRUE;
    }
    else if (xa_ptime_video > xa_time_video) passed_go_flag = xaTRUE; 
    xa_ptime_video = xa_time_video;

    /* Adjust Timing if there's been a change in the speed scale */
    if (xa_speed_change) 
    {
      xa_speed_change = 0;
      XA_Reset_Speed_Time(xa_time_video,xa_speed_scale);
#ifdef XA_AUDIO
      if (xa_speed_scale == XA_SPEED_NORM) snd_speed_now_ok = xaTRUE;
      else
      {  snd_speed_now_ok = xaFALSE;
         if (xa_vaudio_status == XA_AUDIO_STARTED) 
			{ XA_AUDIO_OFF(1); xa_audio_start = 0; }
      }
#endif
    }

#ifdef XA_AUDIO
    if (passed_go_flag == xaTRUE) XA_AUDIO_GET_STATUS( xa_vaudio_status );
    if (xa_vaudio_status == XA_AUDIO_STARTED) 
    {
      if (passed_go_flag == xaTRUE)  /* Hitting 1st frame */
      { /* wait til audio is finished */
        XtAppAddTimeOut(theContext, 50, (XtTimerCallbackProc)XA_Audio_Wait,
							(XtPointer)(NULL) );
        return;
      }
      else if (    (xa_anim_status != XA_RUN_NEXT)
		|| (xa_speed_scale != XA_SPEED_NORM)  ) 
				{ XA_AUDIO_OFF(0); xa_audio_start = 0; }
    }
    else if (xa_vaudio_status == XA_AUDIO_STOPPED)
    {
      if   (    (xa_anim_status == XA_RUN_NEXT) 
	    &&  (xa_speed_scale == XA_SPEED_NORM)
            &&  ( (xa_old_status != XA_RUN_NEXT) || (snd_speed_now_ok==xaTRUE))
	   ) 
      {

	xa_audio_start = 1;
	xa_time_reset = 1;
      }
      else if (   (   (xa_old_status != XA_RUN_PREV)
                   && (xa_anim_status == XA_RUN_PREV))
		|| (passed_go_flag == xaTRUE)  )
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
        || (passed_go_flag == xaTRUE) )
    { 
       xa_time_reset = 1;
       xa_time_audio = -1;
    }

     /* Don't skip unless we are running */
    if (xa_anim_status != XA_RUN_NEXT)
		xa_skip_cnt = xa_skip_video = xa_skip_diff = 0;
    
    if ( (xa_allow_resizing == xaTRUE) && (x11_window_x != 0) )
        { xa_disp_x = x11_window_x; xa_disp_y = x11_window_y; }

    if ((xa_disp_x != xa_buff_x) || (xa_disp_y != xa_buff_y))
	 xa_need_to_scale_b = 1;
    else xa_need_to_scale_b = 0;
    if ((xa_disp_x != xa_imagex) || (xa_disp_y != xa_imagey))
	 xa_need_to_scale_u = 1;
    else xa_need_to_scale_u = 0;


    XA_Show_Action( act );

 } /* end of action valid */
 else
 {
   fprintf(stdout,"PODNOTE: CONTROL FRAME MADE IT THROUGH\n");
 }


/* NOTE/TODO: if this needed before audio init??? */
 /* Reset time if needed */
 if (xa_time_reset)
 { XA_Reset_AV_Time(xa_time_video,xa_speed_scale);
 }

 /* Start Audio is needed */
 if (xa_audio_start)
 { int f_ret = xaFALSE;
   XA_AUDIO_N_FILE(cur_file->file_num,f_ret);

   if (xa_forkit == xaFALSE)
   { xa_vaudio_status = XA_AUDIO_NICHTDA;
     xa_has_audio = xaFALSE;
     xa_audio_start = 0;
   }
   else /* if (xa_forkit == xaTRUE) */
   {
     if (f_ret == xaTRUE)
     { XA_AUDIO_RST_TIME(xa_time_video);

       xa_has_audio = xaTRUE;
       xa_time_audio = 0;
#ifdef XA_REMOTE_CONTROL
       XA_Remote_Adj_Volume(vaudiof->volume,XA_AUDIO_MAXVOL);
#endif
       XA_AUDIO_PREP();
       /* NOTE: xa_audio_start stays a 1 - see a bit below */
     }
     else
     {
       xa_has_audio = xaFALSE;
       xa_audio_start = 0;
     }
   }

  /* Reset time again since audio init can take a while */
   if (xa_time_reset)
   { XA_Reset_AV_Time(xa_time_video,xa_speed_scale);
     xa_time_reset = 0;
   }
   else
   {
      fprintf(stderr,"WHY WOULD WE EVERY BE HERE???\n"); /* CLEAN */
   }

   if (xa_audio_start == 1) 
   {
      XA_AUDIO_ON();
      xa_audio_start = 0;
   }
 }


  if (xa_pause_hdr)
  { if ( (cur_frame==xa_pause_hdr->frame) && (xa_anim_status & XA_RUN_MASK))
    { xa_pause_hdr = xa_pause_hdr->next;
      XA_AUDIO_OFF(0); xa_audio_start = 0;
#ifdef XA_REMOTE_CONTROL
      XA_Remote_Pause();
#endif
      goto XA_PAUSE_ENTRY_POINT; /* reuse single step code */
    }
  }

 if (xa_anim_status & XA_STEP_MASK) /* Single step if in that mode */
 {
   XA_PAUSE_ENTRY_POINT:
   if ( (xa_no_disp == xaFALSE) & (xa_title_flag != XA_TITLE_NONE) )
			XA_Store_Title(cur_file,cur_frame,XA_TITLE_FRAME);
   xa_anim_status &= XA_CLEAR_MASK; /* preserve direction and stop */
   xa_anim_status |= XA_STOP_MASK;
   xa_anim_holdoff = xaFALSE;
   xa_old_status = xa_anim_status;
   return;
 }

 if ( (xa_no_disp == xaFALSE) & (xa_title_flag == XA_TITLE_FRAME) )
			XA_Store_Title(cur_file,cur_frame,XA_TITLE_FRAME);

  /* Harry, what time is it? and how much left? default to 1 ms */
 xa_time_video += cur_file->frame_lst[cur_frame].time_dur;
 xa_time_video = XA_SPEED_ADJ(xa_time_video,xa_speed_scale);
 xa_time_now = XA_Read_AV_Time();

#ifdef XA_AUDIO
 if ((xa_time_audio >= 0) && (xa_has_audio == xaTRUE))
 { 
   DEBUG_LEVEL1 fprintf(stdout,"cav %d %d %d d: %d %d\n",
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
     xa_skip_video = 0;
   }
   else	
   { xaLONG diff = xa_time_now - xa_time_video;
     xaLONG f_time = cur_file->frame_lst[cur_frame].time_dur;
     if (diff > f_time)
     { /* potentially skip video at this point */
	if ((f_time > 0) && (xa_skip_flag==xaTRUE))
	{ xaULONG cnt = diff / f_time;  /* frame behind */
          if ((xa_skip_diff) && (cnt)) /* if skip & skipped last time */
	  {  if (cnt < 2) xa_skip_video = 0;
	     else if (cnt < 3) xa_skip_video = 1;
	     else xa_skip_video = 2;
	  }
	  else xa_skip_video = 0;  /* last wasn't skip, don't skip this */
	  xa_skip_diff = cnt;
	}
	else xa_skip_video = 0;
DEBUG_LEVEL1 fprintf(stdout,"vid behind dif %d skp %d\n",diff,xa_skip_video);
     }
     else xa_skip_video = 0;
     t_time_delta = 1;      /* audio ahead  speed up video */
   }
 }
 else
#endif
 {
   xa_skip_video = 0;
   DEBUG_LEVEL1 fprintf(stdout,"cv %d %d  d: %d %d scal %x\n",
	xa_time_now,xa_time_video,cur_file->frame_lst[cur_frame].zztime,
	cur_file->frame_lst[cur_frame].time_dur,xa_speed_scale);
   if (xa_time_video > xa_time_now)
		t_time_delta = xa_time_video - xa_time_now;
   else		t_time_delta = 1;
 }

 DEBUG_LEVEL1 fprintf(stdout,"t_time_delta %d\n",t_time_delta);
 if ( !(xa_anim_status & XA_STOP_MASK) )
 {
/*
   if (   (t_time_delta < 10) 
       && (looped < XA_ALLOWABLE_LOOPS)) 
		{ looped++; goto XAnim_Looped_Sneak_Entry; }
*/
/* if time is short and no events take a short cut */
/* 10 is chosen because on most systems 10ms is the time slice */
   if (   (t_time_delta < 10) 
       && (!XtAppPending(theContext)) ) goto XAnim_Looped_Sneak_Entry;

   else XtAppAddTimeOut(theContext,t_time_delta,
		(XtTimerCallbackProc)XAnim_Looped,(XtPointer)(XA_SHOW_NORM));
 }
 else xa_anim_holdoff = xaFALSE;
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
  { xaULONG jmp2end_flag = 0;
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
	if (xa_pingpong_flag==xaFALSE)
	{
	  if ( (cur_floop+1) >= cur_file->loop_num) /* done with this file */
	  {
             if (first_file->next_file != first_file) /* more to go */
	     {
	       cur_frame = cur_file->last_frame + 1; /* jmp to end */
	     }
	     else if (xa_exit_flag == xaTRUE) TheEnd(); /* we're outta here */
	     else cur_frame++;  /* step to beg */
          }  else cur_frame++; /* step to beg */
        } else jmp2end_flag = 1;
      }
      frame = &cur_file->frame_lst[cur_frame];
    }

    if ( (frame->act == 0) /* Are we at the end of an anim? */
        || (jmp2end_flag) )
    {
      if (xa_pingpong_flag == xaTRUE)
      { jmp2end_flag = 0;
        xa_anim_status &= ~(XA_NEXT_MASK);  /* change dir to prev */
        cur_frame--; 
        Step_Action_Prev();
        return;
      }
      cur_frame = cur_file->loop_frame;

      cur_floop++;
      DEBUG_LEVEL1 fprintf(stdout,"  loop = %d\n",cur_floop);

      /* Done looping animation. Move on to next file if present */
      if (   (cur_floop >= cur_file->loop_num)
	  || (xa_anim_status & XA_STEP_MASK)   ) /* or if single stepping */
      {
        cur_floop = 0;             /* Reset Loop Count */

	/* Are we on the last file and do we need to exit? */
	if (   (cur_file->next_file == first_file)
	    && (xa_exit_flag == xaTRUE) )   TheEnd(); /* later */

        /* This is a special case check.
         * If more that one file, reset file_is_started, otherwise
         * if we're only displaying 1 animation jump to the loop_frame
         * which has already been set up above.
	 * If cur_file has audio then we always restart.
         */
        if (   (first_file->next_file != first_file)
            || (xa_has_audio==xaTRUE) )
        {
          file_is_started = xaFALSE;
          cur_file = cur_file->next_file;
          cur_frame = 0;
        }
        DEBUG_LEVEL1 fprintf(stdout,"  file = %d\n",cur_file->file_num);
        if (xa_time_flag == xaTRUE) XA_Time_Check();
      } /* end done looping file */
      else if (xa_has_audio==xaTRUE)  /* need to restart audio */
      {
        file_is_started = xaFALSE;
        cur_frame = 0;
      }
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
    if (cur_frame < 0)		goto XA_Step_Action_Prev_0;
    if (frame->act == 0)	goto XA_Step_Action_Prev_0;
    if ((cur_frame < cur_file->loop_frame) && (xa_pingpong_flag == xaFALSE))
    {
      XA_Step_Action_Prev_0:  /* skip indexing with -1 */

      if (xa_pingpong_flag == xaTRUE)
      {
        xa_anim_status |= XA_NEXT_MASK;  /* change dir to forward */
        cur_floop++;

	/* Are we on the last file and do we need to exit? */
	if (   (cur_file->next_file == first_file)
	    && (xa_exit_flag == xaTRUE) )   TheEnd(); /* later */

         /* do we move to next file? */
        if (  (first_file->next_file != first_file)  /* more than 1 file */
	    && (   (cur_floop >= cur_file->loop_num)
                || (xa_anim_status & XA_STEP_MASK)  ) )
        {
          cur_floop = 0;
          file_is_started = xaFALSE;
          cur_file = cur_file->next_file;
          cur_frame = 0;
          break;
        }

        if (cur_floop >= cur_file->loop_num) 
        {
          cur_floop = 0;
          if (xa_time_flag == xaTRUE) XA_Time_Check();
        }
        cur_frame++;
        Step_Action_Next();
        return;
      } /* end of pingpong stuff */

      cur_frame = cur_file->last_frame;
      cur_floop--;
      DEBUG_LEVEL1 fprintf(stdout,"  loop = %d\n",cur_floop);
       /* Done looping animation. Move on to next file if present */
      if (   (cur_floop <= 0)
	  || (xa_anim_status & XA_STEP_MASK) ) /* or if single stepping */
      {
        cur_floop = cur_file->loop_num; /* Reset Loop Count */
 
        /* If more that one file, go to next file */
        if (first_file->next_file != first_file )
        {
          file_is_started = xaFALSE;
          cur_file = cur_file->prev_file;
          cur_frame = cur_file->last_frame;
          cur_floop = cur_file->loop_num; /* Reset Loop Count */
        }

        if (xa_time_flag == xaTRUE) XA_Time_Check();
        DEBUG_LEVEL1 fprintf(stdout,"  file = %d\n",cur_file->file_num);
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
  file_is_started = xaFALSE;
  cur_frame = 0;
  cur_file = cur_file->next_file;
  cur_floop = 0; /* used if things start up again */

  DEBUG_LEVEL1 fprintf(stdout,"  file = %d\n",cur_file->file_num);
}

/*
 *
 */
void Step_File_Prev()
{
  file_is_started = xaFALSE;
  cur_frame = 0;
  cur_file = cur_file->prev_file;
  cur_floop = 0; /* used if things start up again */

  DEBUG_LEVEL1 fprintf(stdout,"  file = %d\n",cur_file->file_num);
}


/*********** XA_Cycle_Wait *********************************************
 * This function waits until all the color cycling processes have halted.
 * Then it fires up XAnim_Looped with the XA_SHOW_SKIP command to cause
 * XAnim_Looped to skip the frame movement.
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
    xa_now_cycling = xaFALSE;
    XtAppAddTimeOut(theContext, 1, (XtTimerCallbackProc)XAnim_Looped, 
						(XtPointer)(XA_SHOW_SKIP));
  }
}

/*********** XA_Audio_Wait *********************************************
 * This function waits until the audio from the previous animation is
 * finished before starting up the next animation.
 * Then it fires up XAnim_Looped with the XA_SHOW_SKIP command to cause
 * XAnim_Looped to skip the frame movement.
 ***********************************************************************/
void XA_Audio_Wait(nothing, id)
char *nothing;
XtIntervalId *id;
{ DEBUG_LEVEL1 fprintf(stdout,"XA_Audio_Wait\n");
  XA_AUDIO_GET_STATUS( xa_vaudio_status );
  if (xa_vaudio_status == XA_AUDIO_STARTED) /* continue waiting */
  { if (xa_anim_status & XA_RUN_MASK)
    { XtAppAddTimeOut(theContext, 50, (XtTimerCallbackProc)XA_Audio_Wait,
							(XtPointer)(NULL));
      return;
    }
    XA_AUDIO_OFF(0);		xa_audio_start = 0;
  }
  XtAppAddTimeOut(theContext, 1, (XtTimerCallbackProc)XAnim_Looped, 
						(XtPointer)(XA_SHOW_SKIP));
}

/*
 *
 */
void XA_Cycle_It(act_cycle, id)
ACT_CYCLE_HDR   *act_cycle;
XtIntervalId *id;
{
  xaULONG i,*i_ptr,size,curpos;

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

  i_ptr = (xaULONG *)act_cycle->data;
  for(i=0;i<size;i++)
  {
    xaULONG j;

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
  if (xa_vaudio_enable != xaTRUE) XSync(theDisp,False);
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

/***************************************
 *
 **************/
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
  if (file_hdr->name) free(file_hdr->name);  /* POD ADD */

  FREE(file_hdr,0x02);
  return(tmp_hdr);
}

/***************************************
 *
 **************/
XA_ANIM_HDR *Get_Anim_Hdr(file_hdr,file_name)
XA_ANIM_HDR *file_hdr;
char *file_name;
{
  XA_ANIM_HDR *temp_hdr;
  xaLONG length;

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
  temp_hdr->free_chain	= 0;
  temp_hdr->xin		= 0;

  length = strlen(file_name) + 1;
  temp_hdr->name = (char *)malloc(length);
  strcpy(temp_hdr->name,file_name);
  XA_AUDIO_FILE(xa_file_num);
  xa_file_num++;
  return(temp_hdr);
}
  

void XA_Install_CMAP(chdr)
XA_CHDR *chdr;
{ ColorReg *tcmap;
  xaLONG j;

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

  DEBUG_LEVEL1 fprintf(stdout,"  Install CMAP %x old was %x\n",
					(xaULONG)chdr,(xaULONG)xa_chdr_now);

  if ( x11_cmap_flag == xaFALSE )
  {
    DEBUG_LEVEL1 fprintf(stdout,"  Fake Install since cmap not writable\n");
    xa_chdr_now = chdr;
    return;
  }
  else /* install the cmap */
  {
    DEBUG_LEVEL2 fprintf(stdout,"CMAP: size=%d off=%d\n",
		xa_cmap_size,xa_cmap_off);
    if (xa_cmap_size > x11_cmap_size)
    {
      fprintf(stdout,"Install CMAP: Error csize(%d) > x11 cmap(%d)\n",
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
        DEBUG_LEVEL3 fprintf(stdout," g %d) %x %x %x <%x>\n",
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
        DEBUG_LEVEL3 fprintf(stdout," %d) %x %x %x <%x>\n",
             j,xa_cmap[j].red,xa_cmap[j].green,xa_cmap[j].blue,xa_cmap[j].gray);
		
      }
    }
    if (x11_cmap_installed)
    { x11_cmap_installed = xaTRUE;
#ifndef NO_INSTALL_CMAP
      XInstallColormap(theDisp,theCmap);
#endif
    }
    XStoreColors(theDisp,theCmap,defs,xa_cmap_size);
    xa_chdr_now = chdr;
#ifdef XA_PETUNIA
    XA_Remote_ColorUpdate(chdr);
#endif
  }
}


/********************************
 * Initialize the global variable xa_time_off.
 *
 ***************/
xaLONG XA_Time_Init()
{
  gettimeofday(&tv, 0);
  xa_time_off = tv.tv_sec;
  return( xa_time_off );
}

/********************************
 * return time from start in milliseconds
 ***************/
xaLONG XA_Time_Read()
{
  xaLONG t;
  gettimeofday(&tv, 0);
  t = (tv.tv_sec - xa_time_off) * 1000 + (tv.tv_usec / 1000);
  return(t);
}


/*
 *
 */
void XA_Time_Check()
{
  xaLONG time_int;

  xa_time_end = XA_Time_Read();
  time_int = xa_time_end - xa_time_start;
  xa_time_av = (xa_time_av * xa_time_num) + time_int;
  xa_time_num++;
  xa_time_av /= xa_time_num;
  fprintf(stdout,"l_time = %d  av %d  skipd %d Sskipd %d\n",
		time_int,xa_time_av,xa_frames_skipd,xa_frames_Sskipd);
  xa_frames_skipd = xa_frames_Sskipd = 0;
  xa_time_start = XA_Time_Read();

}

xaULONG XA_Read_Int(s,j)
xaUBYTE *s;
xaULONG *j;
{
  xaULONG i,num;
  i = *j; num = 0;
  while( (s[i] >= '0') && (s[i] <= '9') )
	{ num *= 10; num += (xaULONG)(s[i]) - (xaULONG)('0'); i++; }
  *j = i;   
  return(num);
}

float XA_Read_Float(s,j)
xaUBYTE *s;
xaULONG *j;
{
  xaULONG i;
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

xaLONG XA_Get_Class(p)
char *p;
{
  xaULONG i,len;
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
xaULONG frame;		/* frame at which to pause */
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
xaLONG XA_Read_AV_Time()
{
  return( (XA_Time_Read() - xa_av_time_off) );
}

/***************************************************************************
 *
 */
void XA_Reset_Speed_Time(vid_time,speed_scale)
xaLONG vid_time; xaULONG speed_scale;
{ xaLONG scaled_vid_time = XA_SPEED_ADJ(vid_time,speed_scale);
  xa_av_time_off = XA_Time_Read() - scaled_vid_time;
}

/***************************************************************************
 * This Function accepts current video time and if Audio is enabled, it
 * adjusts the audio stream to sync with the video. Whether or not audio
 * is enabled, it adjusts animation start time to be now.
 */
void XA_Reset_AV_Time(vid_time,speed_scale)
xaLONG vid_time; xaULONG speed_scale;
{ xaLONG scaled_vid_time = XA_SPEED_ADJ(vid_time,speed_scale);
  DEBUG_LEVEL1 fprintf(stdout,"RESET_AV %d\n",vid_time);
  xa_av_time_off = XA_Time_Read() - scaled_vid_time;
  XA_AUDIO_VID_TIME(xa_av_time_off);
}

/***************************************************************************
 * Look at environmental variable and look for
 * INTERNAL
 * EXTERNAL
 * HEADPHONES
 * NONE
 ***************************************************************************/

xaULONG XA_Audio_Speaker(env_v)
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


/****************************************************************
 *
 ********************************/
#ifdef XSHM
xaULONG XA_Setup_Them_Buffers(im_Image,im_shminfo,im_buff)
XImage **im_Image;
XShmSegmentInfo *im_shminfo;
#else
xaULONG XA_Setup_Them_Buffers(im_buff)
#endif
char **im_buff;
{ xaULONG buf_size = xa_max_image_size * x11_bytes_pixel;
#ifdef XSHM
  if (shm)
  { *im_Image = XShmCreateImage(theDisp,theVisual,x11_depth,
                ZPixmap, 0, im_shminfo, xa_max_imagex, xa_max_imagey );
    if (*im_Image == 0) 
		{ shm = 0; x11_error_possible = 0; goto XA_SHM_FAILURE; }
    im_shminfo->shmid = shmget(IPC_PRIVATE, buf_size, (IPC_CREAT | 0777 ));
    if (im_shminfo->shmid < 0) 
    { perror("shmget"); 
	shm = 0; x11_error_possible = 0;
        if (*im_Image) 
	{ (*im_Image)->data = 0;
	  XDestroyImage(*im_Image); *im_Image = 0;
	}
	goto XA_SHM_FAILURE;
    }
    im_shminfo->shmaddr = (char *) shmat(im_shminfo->shmid, 0, 0);
    XSync(theDisp, False);
    x11_error_possible = 1;  /* if remote display. following will fail */
    XShmAttach(theDisp, im_shminfo);
    XSync(theDisp, False);
    shmctl(im_shminfo->shmid, IPC_RMID, 0 );
    if (x11_error_possible == -1) /* above failed - back off */
    { shm = 0;
	x11_error_possible = 0;
        if (xa_verbose) fprintf(stdout,"SHM Attach failed: backing off\n");
	shmdt(im_shminfo->shmaddr);	im_shminfo->shmaddr = 0;
	(*im_Image)->data = 0;
        if (*im_Image) { XDestroyImage(*im_Image); *im_Image = 0; }
	goto XA_SHM_FAILURE;
    }
    else
    {
      (*im_Image)->data = *im_buff = im_shminfo->shmaddr;
      sh_Image = *im_Image;
      DEBUG_LEVEL2 fprintf(stdout, "Using Shared Memory Extension\n");
    }
  } else
#endif /* XSHM */
  {
XA_SHM_FAILURE:
    *im_buff = (char *) malloc(buf_size);
    if (*im_buff == 0) return(xaFALSE);
  }
  memset(*im_buff,0x00,buf_size);  
  return(xaTRUE);
}


void XA_Store_Title(cur_file,cur_frame,xa_title_flag)
XA_ANIM_HDR *cur_file;
xaLONG	cur_frame;
xaULONG	xa_title_flag;
{ char xa_title[256];
  switch(xa_title_flag)
  { case XA_TITLE_FILE:
	sprintf(xa_title,"XAnim: %s",cur_file->name);
	break;
    case XA_TITLE_FRAME:
	if (cur_frame >= 0)
	sprintf(xa_title,"XAnim: %s %d",cur_file->name,cur_frame);
	else sprintf(xa_title,"XAnim: %s",cur_file->name);
	break;
    default:
	sprintf(xa_title,"XAnim");
	break;
  }
/* SMR 6 */
  if (xa_window_id == 0) XStoreName(theDisp,mainW,xa_title);
  else xa_title_flag = XA_TITLE_NONE;
/* end SMR 6 */

}


void XA_Resize_Window(cur_file,xa_noresize_flag,xa_disp_x,xa_disp_y)
XA_ANIM_HDR *cur_file;
xaULONG xa_disp_x, xa_disp_y;
xaLONG xa_noresize_flag;

{
  if (xa_noresize_flag == xaFALSE)
  {
    if ((xa_disp_x != cur_file->dispx) || (xa_disp_y != cur_file->dispy))
    {
	DEBUG_LEVEL1 fprintf(stdout,"resizing to <%d,%d>\n",
					cur_file->dispx,cur_file->dispy);
      XSync(theDisp,False);
      XResizeWindow(theDisp,mainW,cur_file->dispx,cur_file->dispy);
      x11_window_x = cur_file->dispx; x11_window_y = cur_file->dispy;
      XSync(theDisp,False);
    }
  }
  else /* fixed window size */
  {
    XSync(theDisp,False);
    if (xa_disp_x > cur_file->dispx)
                XClearArea(theDisp,mainW,cur_file->dispx,0, 0,0,False);
    if (xa_disp_y > cur_file->dispy)
                XClearArea(theDisp,mainW,0,cur_file->dispy, 0,0,False);
    XSync(theDisp,False);
  }
}

/* NEW CODE GOES HERE */

/***********************************************************************
 * 
 ***************/
void Hard_Death()
{
  if (xa_vaudio_present==XA_AUDIO_OK) XA_AUDIO_KILL();
  if (xa_vid_fd >= 0) { close(xa_vid_fd); xa_vid_fd = -1; }
  if (xa_aud_fd >= 0) { close(xa_aud_fd); xa_aud_fd = -1; }
  if (xa_vidcodec_buf) { FREE(xa_vidcodec_buf,0x05); xa_vidcodec_buf=0;}
  if (xa_audcodec_buf) { FREE(xa_audcodec_buf,0x99); xa_audcodec_buf=0;}

#ifdef XA_AUDIO
  XA_AUDIO_EXIT();
  XA_IPC_Close_Pipes();
#endif


  cur_file = first_file;
  if (cur_file)
  {
    first_file->prev_file->next_file = 0;  /* last file's next ptr to 0 */
  }
  while(cur_file)
  { XA_ANIM_HDR *tmp_hdr = cur_file->next_file;
    Free_Actions(cur_file->acts);
    if (cur_file->name) { FREE(cur_file->name,0x15); cur_file->name = 0; }
    XA_Walk_The_Chain(cur_file,xaTRUE);  /* Walk and Free functions */
    FREE(cur_file,0x16);
    cur_file = tmp_hdr;
  }

  Free_Video_Codecs(); /* should be after chain */

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
#ifndef XA_IS_PLUGIN
  exit(0);
#endif
}

/***********************************************
 * This function (hopefully) provides a clean exit from our code.
 *******************************/
void TheEnd()
{
/* SMR 7 */
/* force the other window to be redrawn */
  if (theDisp)
  { if (xa_window_id != 0 && xa_window_refresh_flag == xaTRUE)
			XClearArea(theDisp, mainW, 0, 0, 0, 0, True);
    XFlush(theDisp);
  }
/* end SMR 7 */

/*POD TEMP eventually move into free chain*/
  jpg_free_stuff();


  if (xa_vaudio_present==XA_AUDIO_OK)  XA_AUDIO_KILL();
  if (xa_vid_fd >= 0) { close(xa_vid_fd); xa_vid_fd = -1; }
  if (xa_aud_fd >= 0) { close(xa_aud_fd); xa_aud_fd = -1; }
  if (xa_vidcodec_buf) { FREE(xa_vidcodec_buf,0x14); xa_vidcodec_buf=0;}
  if (xa_audcodec_buf) { FREE(xa_audcodec_buf,0x99); xa_audcodec_buf=0;}

#ifdef XA_AUDIO
  XA_AUDIO_EXIT();
  XA_IPC_Close_Pipes();
#endif

  cur_file = first_file;
  if (cur_file)
  {
    first_file->prev_file->next_file = 0;  /* last file's next ptr to 0 */
  } 
  while(cur_file)
  { XA_ANIM_HDR *tmp_hdr = cur_file->next_file;
    Free_Actions(cur_file->acts);
    if (cur_file->name) { FREE(cur_file->name,0x15); cur_file->name = 0; }
    XA_Walk_The_Chain(cur_file,xaTRUE);  /* Walk and Free functions */
    FREE(cur_file,0x16);
    cur_file = tmp_hdr;
  }

  Free_Video_Codecs(); /* Should be after Chain */

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
#ifdef XA_REMOTE_CONTROL
  XA_Remote_Free();
#endif
  XA_Free_CMAP();
  if (theDisp) XtCloseDisplay(theDisp);


#ifndef XA_IS_PLUGIN
  exit(0);
#endif
}

/***********************************************
 * just prints a message before calling TheEnd()
 *******************************/
void TheEnd1(err_mess)
char *err_mess;
{
 fprintf(stdout,"%s\n",err_mess);
 TheEnd();
}

/***********************************************
 *
 *******************************/
XA_ANIM_SETUP *XA_Get_Anim_Setup()
{ int i;
  XA_ANIM_SETUP *setup = (XA_ANIM_SETUP *)malloc(sizeof(XA_ANIM_SETUP));
  if (setup==0) TheEnd1("XA_ANIM_SETUP: malloc err\n");
  setup->imagex		= setup->max_imagex	= 0;
  setup->imagey		= setup->max_imagey	= 0;
  setup->imagec		= setup->depth		= 0;
  setup->compression	= 0;
  setup->pic		= 0;
  setup->pic_size	= 0;
  setup->max_fvid_size	= setup->max_faud_size	= 0;
  setup->vid_time	= setup->vid_timelo	= 0;
  setup->aud_time	= setup->aud_timelo	= 0;
  setup->chdr		= 0;
  setup->cmap_flag	= 0;
  setup->cmap_cnt	= 0;
  setup->color_retries	= 0;
  setup->color_cnt	= 0;
  setup->cmap_frame_num	= 0;
  for(i=0; i< 256; i++)
	setup->cmap[i].red = setup->cmap[i].green = setup->cmap[i].blue = 0;
  return(setup);
}

/***********************************************
 *
 *******************************/
void XA_Free_Anim_Setup(setup)
XA_ANIM_SETUP *setup;
{
  if (setup) free(setup);
}

/***********************************************
 * Allocate and init a Fuction Chain Structure.
 *
 *******************************/
XA_FUNC_CHAIN *XA_Get_Func_Chain(anim_hdr,function)
XA_ANIM_HDR *anim_hdr;
void (*function)();
{ XA_FUNC_CHAIN *f_chain = (XA_FUNC_CHAIN *)malloc(sizeof(XA_FUNC_CHAIN));
  if (f_chain==0) TheEnd1("XA_FUNC_CHAIN: malloc err\n");
  f_chain->function = function;
  f_chain->next     = 0;
  return(f_chain);
}

/***********************************************
 * Add Function to List of functions to be called at Dying Time.
 *
 *******************************/
void XA_Add_Func_To_Free_Chain(anim_hdr,function)
XA_ANIM_HDR *anim_hdr;
void (*function)();
{ XA_FUNC_CHAIN *f_chain = XA_Get_Func_Chain(anim_hdr,function);
  f_chain->next = anim_hdr->free_chain;
  anim_hdr->free_chain = f_chain;
}

/***********************************************
 * 
 *
 *******************************/
void XA_Walk_The_Chain(anim_hdr,free_flag)
XA_ANIM_HDR *anim_hdr;
xaULONG free_flag;		/* xaTRUE free structs. xaFALSE don't free */
{ XA_FUNC_CHAIN *f_chain = anim_hdr->free_chain;

  while(f_chain)
  { XA_FUNC_CHAIN *tmp = f_chain;
    f_chain->function();
    f_chain = f_chain->next;
    if (free_flag == xaTRUE) free(tmp);
  }
  if (free_flag == xaTRUE) anim_hdr->free_chain = 0;
}


/**************** Send BOFL's to the Audio Child every 5 seconds(5000 ms) */

#ifdef XA_AUDIO

void XA_Setup_BOFL()
{
  XtAppAddTimeOut(theContext,5000, (XtTimerCallbackProc)XA_Video_BOFL,
                                                        (XtPointer)(NULL));
}

void XA_Video_BOFL()
{
  if (xa_forkit == xaTRUE)
	xa_forkit = XA_Video_Send2_Audio(XA_IPC_BOFL,NULL,0,0,0,0);
  if (xa_forkit == xaTRUE)
	XtAppAddTimeOut(theContext,5000, 
		(XtTimerCallbackProc)XA_Video_BOFL,(XtPointer)(NULL));
}

#endif


void XA_Adjust_Video_Timing(anim_hdr,total_time)
XA_ANIM_HDR *anim_hdr;
xaULONG total_time;
{ double ftime = (double)(total_time);
  xaULONG i,vid_time,t_time;
  xaLONG vid_timelo,t_timelo;

  if (total_time == 0) return;
  if (anim_hdr->last_frame == 0) return;

  /* this isn't perfect but it'll be close 90% of the time */
  ftime /= (double)(anim_hdr->last_frame);
  vid_time =  (xaULONG)(ftime);
  ftime -= (double)(vid_time);
  vid_timelo = (xaLONG)(ftime * (double)(1<<24));
  
  t_time = 0;
  t_timelo = 0;
  i = 0;
  while( anim_hdr->frame_lst[i].act )
  {
    if ( anim_hdr->frame_lst[i].zztime < 0) continue;
    anim_hdr->frame_lst[i].time_dur = vid_time;
    anim_hdr->frame_lst[i].zztime = t_time;
    t_time += vid_time;
    t_timelo += vid_timelo;
    while(t_timelo > (1<<24)) {t_time++; t_timelo -= (1<<24);}
    i++;
  }
}

