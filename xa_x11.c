
/*
 * xa_x11.c
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
 * 12Feb95 - fixed calling params of Callbacks and Actions.
 * 22Jun95 - Translations added to Remote Control Window as well so
 *           now all keyboards commands should work there as well.
 *******************************/


#include "xanim.h"
#include <Intrinsic.h>
#include <StringDefs.h>
#include <Shell.h>

#ifdef XA_ATHENA
#include <Xaw/Form.h>			/* gf */
#include <Xaw/Command.h>		/* gf */
#endif

#ifdef XA_MOTIF
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/PushB.h>
#endif

#include <sys/signal.h>
#ifndef VMS
#include <sys/times.h>
#endif
#include <ctype.h>

#ifdef XSHM
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#endif /*XSHM*/
#ifdef XMBUF
#include <X11/extensions/multibuf.h>
#endif


#include "xa_x11.h"

#ifdef XA_FORK
#include "xa_ipc.h"
#endif

extern xaULONG shm;
extern xaULONG mbuf;
extern XA_AUD_FLAGS *vaudiof;

#ifdef XA_FORK
extern xaULONG xa_forkit;
#define XA_SPEAKER_TOG(x)       { if (xa_forkit == xaTRUE) xa_forkit = \
  XA_Video_Send2_Audio(XA_IPC_AUD_STOG,NULL,0,x,2000,0); }
#define XA_HEADPHONE_TOG(x)     { if (xa_forkit == xaTRUE) xa_forkit = \
  XA_Video_Send2_Audio(XA_IPC_AUD_HTOG,NULL,0,x,2000,0); }
#define XA_AUDIO_SET_VOLUME(x)  { if (xa_forkit == xaTRUE) xa_forkit = \
  XA_Video_Send2_Audio(XA_IPC_AUD_VOL,0,0,x,1000,0);  }
#define XA_AUDIO_SET_MUTE(x)    { if (xa_forkit == xaTRUE) xa_forkit = \
  XA_Video_Send2_Audio(XA_IPC_AUD_MUTE,0,0,x,1000,0);  }
#else
extern void (*XA_Speaker_Tog)();
extern void (*XA_Headphone_Tog)();
#define XA_SPEAKER_TOG(x)       { XA_Speaker_Tog(x); }
#define XA_HEADPHONE_TOG(x)     { XA_Headphone_Tog(x); }
#define XA_AUDIO_SET_MUTE(x)
#define XA_AUDIO_SET_VOLUME(x)
#endif

/* These are the default X,Y Positions of the Main Window and the
 * RemoteControl Window.
 */
#define XA_REMW_XPOS    20
#define XA_REMW_YPOS    20
#define XA_MAINW_XPOS   (XA_REMW_XPOS + 120)
#define XA_MAINW_YPOS   20

extern XA_CHDR *xa_chdr_first;
extern xaULONG x11_shared_flag;
extern xaULONG x11_multibuf_flag;
extern xaULONG x11_expose_flag;
extern xaLONG x11_error_possible;
extern xaULONG xa_speed_change;
extern xaLONG xa_remote_ready;
xaLONG xa_remote_realized = xaFALSE;
xaLONG xa_remote_busy = xaFALSE;

int XA_Error_Handler();

void xa_remote_expose();
void xanim_expose();
void xanim_key();
void xanim_button();
void xanim_resize();
void xanim_events();
void X11Setup();
void X11_Show_Visuals();
void X11_OutPut_Visual_Class();
void X11_Init_Image_Struct();
void X11_Get_Shift();
void X11_Init();
void X11_Pre_Setup();
void X11_Setup_Window();
void X11_Map_Window();
void X11_Make_Nice_CHDR();
void X11_Get_Colormap();
#ifdef XA_REMOTE_CONTROL
Widget play_widget,audio_widget,norm_widget;
Widget remote_widget,last_widget;
void XA_Create_Remote();
void XA_Remote_Pause();
void XA_Remote_Change();
void XA_Realize_Remote();
void XA_Unrealize_Remote();
#endif

void XA_Install_CMAP();
void IFF_Buffer_HAM6();
void IFF_Buffer_HAM8();
void UTIL_Mapped_To_Bitmap();
void UTIL_Mapped_To_Mapped();
xaULONG CMAP_Find_Closest();




/*********************************** X11 stuff */
Display       *theDisp;
int	       theScreen;
Visual        *theVisual;
Colormap       theCmap;
Window         mainW;

#ifdef XMBUF
Window		mainWreal;       /* actual main window */
Window		mainWbuffers[2]; /* two buffers for the main window */
int		mainWbufIndex;   /* which buffer is actually visible */
#endif

GC             theGC;
XImage        *theImage;
XColor         defs[256];
Widget	       theWG;
/******************************** Xt stuff */
extern XA_CHDR *xa_chdr_now;
extern XA_ANIM_HDR *cur_file;
extern xaULONG xa_title_flag;
extern xaULONG xa_anim_flags;
extern char xa_title[];
extern xaULONG x11_window_x,x11_window_y;
extern xaULONG xa_buff_x,xa_buff_y;
extern xaULONG xa_allow_resizing;
extern xaULONG xa_allow_nice;
extern xaULONG xa_speed_scale;

extern XA_AUD_FLAGS *AUD;

extern xaULONG xa_audio_newvol;
extern xaLONG xa_audio_volume;
extern xaULONG xa_audio_mute;



XtAppContext  theContext;

/*
 * gf: Forward definitions of action procedures:
 */
static void xanim_step_prev_action(),xanim_step_next_action();
static void xanim_toggle_action(),xanim_quit_action();
static void xanim_resize_action();
static void xanim_install_cmap_action(),xanim_stop_cmap_action();
static void xanim_restore_cmap_action();
static void xanim_faster_action(),xanim_slower_action();
static void xanim_speed_reset_action(); 
static void xanim_next_anim_action(),xanim_prev_anim_action();
static void xanim_step_prev_int_action(),xanim_step_next_int_action();
static void xanim_dec_audio_5(),xanim_dec_audio_1();
static void xanim_inc_audio_5(),xanim_inc_audio_1(),xanim_mute_audio();
static void xanim_speaker_tog(),xanim_headphone_tog();
static void xanim_realize_remote();

/*
 * gf: Replace KeyUp() and ButtonPress() with more specific actions
 */
#define ACTIONTABLE_SIZE 25
XtActionsRec actionTable[] = {
        {"Expose", xanim_expose},
        {"Configure", xanim_resize},
	{"StepPrev", xanim_step_prev_action},
	{"StepNext", xanim_step_next_action},
	{"RunStop", xanim_toggle_action},
	{"Quit", xanim_quit_action},
	{"Resize", xanim_resize_action},
	{"InstallCmap", xanim_install_cmap_action},
	{"RealizeRemote", xanim_realize_remote},
	{"StopCmap", xanim_stop_cmap_action},
	{"RestoreCmap", xanim_restore_cmap_action},
	{"Slower", xanim_slower_action},
	{"Faster", xanim_faster_action},
	{"SpeedReset", xanim_speed_reset_action},
	{"NextAnim", xanim_next_anim_action},
	{"PrevAnim", xanim_prev_anim_action},
	{"StepNextInt", xanim_step_next_int_action},
	{"StepPrevInt", xanim_step_prev_int_action},
	{"DecAudio5", xanim_dec_audio_5},
	{"DecAudio1", xanim_dec_audio_1},
	{"IncAudio5", xanim_inc_audio_5},
	{"IncAudio1", xanim_inc_audio_1},
	{"AudioMute", xanim_mute_audio},
	{"SpeakerTog",xanim_speaker_tog},
	{"HDPhoneTog",xanim_headphone_tog},
};

/*
 * gf: Bitmaps for control buttons
 */
#include "BM_step_prev.xbm"
#include "BM_play.xbm"
#include "BM_step_next.xbm"
#include "BM_next.xbm"
#include "BM_quit.xbm"
#include "BM_prev.xbm"
#include "BM_slower.xbm"
#include "BM_speed1.xbm"
#include "BM_faster.xbm"
#include "BM_vol_dn.xbm"
#include "BM_vol_off.xbm"
#include "BM_vol_up.xbm"
#include "BM_back.xbm"
#include "BM_stop.xbm"
#include "BM_vol_on.xbm"
#include "BM_fuzz.xbm"

#define BM_NUMBER 16
#define BM_STPP		0
#define BM_PLAY		1
#define BM_STPN		2
#define BM_NEXT		3
#define BM_QUIT		4
#define BM_PREV		5
#define BM_SLOW		6
#define BM_NORM		7
#define BM_FAST		8
#define BM_VDOWN	9
#define BM_VOFF		10
#define BM_VUP		11
#define BM_BACK		12
#define BM_STOP		13
#define BM_VON 		14
#define BM_FUZZ		15

static Pixmap BM_pmap[BM_NUMBER];
static int BM_width[] 
		= {BM_step_prev_width,BM_play_width,BM_step_next_width,
		BM_next_width,BM_quit_width,BM_prev_width,BM_slower_width,
		BM_speed1_width,BM_faster_width,BM_vol_dn_width,
		BM_vol_off_width,BM_vol_up_width,BM_back_width,
		BM_stop_width,BM_vol_on_width,BM_fuzz_width};
static int BM_height[] 
		= {BM_step_prev_height,BM_play_height,BM_step_next_height,
		BM_next_height,BM_quit_height,BM_prev_height,BM_slower_height,
		BM_speed1_height,BM_faster_height,BM_vol_dn_height,
		BM_vol_off_height,BM_vol_up_height,BM_back_height,
		BM_stop_height,BM_vol_on_height,BM_fuzz_height};
static unsigned char *BM_bits[] 
		= {BM_step_prev_bits,BM_play_bits,BM_step_next_bits,
		BM_next_bits,BM_quit_bits,BM_prev_bits,BM_slower_bits,
		BM_speed1_bits,BM_faster_bits,BM_vol_dn_bits,
		BM_vol_off_bits,BM_vol_up_bits,BM_back_bits,
		BM_stop_bits,BM_vol_on_bits,BM_fuzz_bits};


static struct _resource {
  int anim;
} resource;

#define offset(field)   XtOffset(struct _resource *, field)

/* POD TEMP 

static char *ReadOneXDefault(char *Entry)
{
    XrmString Type;
    XrmValue Result;
    char Line[LINE_LEN_LONG];
 
    sprintf(Line, "%s.%s", PROGRAM_NAME, Entry);
    if ( XrmGetResource(XDisplay->db, Line, "Program.Name", &Type, &Result ) )
        return Result.addr;
    else
        return NULL;
}

*ViewTextColor = ReadOneXDefault("View.TextColor"),
XParseColor(XDisplay, XColorMap, ViewTextColor, &Color)
bwidth = atoi(ViewBorderWidthStr);
XParseGeometry(ViewGeometry, &ViewPosX, &ViewPosY, &ViewWidth, &ViewHeight);

*/

XtResource application_resources[] = {
  {"anim", "Anim", XtRBoolean, sizeof (Boolean),
     offset(anim), XtRString, "False" },
};

String   Translation =
  "<Expose>:		Expose()\n\
   <Configure>:		Configure()\n\
   <Btn1Down>,<Btn1Up>:	StepPrev()\n\
   <Btn2Down>,<Btn2Up>:	RunStop()\n\
   <Btn3Down>,<Btn3Up>:	StepNext()\n\
   <Key>1:		DecAudio5()\n\
   <Key>2:		DecAudio1()\n\
   <Key>3:		IncAudio1()\n\
   <Key>4:		IncAudio5()\n\
   <Key>8:		SpeakerTog()\n\
   <Key>9:		HDPhoneTog()\n\
   <Key>s:		AudioMute()\n\
   <Key>q:		Quit()\n\
   <Key>w:		Resize()\n\
   <Key>F:		InstallCmap()\n\
   <Key>z:		RealizeRemote()\n\
   <Key>g:		StopCmap()\n\
   <Key>r:		RestoreCmap()\n\
   <Key>-:		Slower()\n\
   <Key>=:		Faster()\n\
   <Key>0:		SpeedReset()\n\
   <Key>.:		StepNext()\n\
   <Key>comma:		StepPrev()\n\
   <Key>greater:	NextAnim()\n\
   <Key>less:		PrevAnim()\n\
   <Key>/:		StepNextInt()\n\
   <Key>m:		StepPrevInt()\n\
   <Key>space:		RunStop()";

void X11_Get_Shift(mask,shift,size)
xaULONG mask,*shift,*size;
{
  xaLONG i,j;

  i=0;
  while( (i < 32) && !(mask & 0x01) ) 
  {
    mask >>= 1;
    i++;
  }
  if (i >= 32)
  {
    fprintf(stderr,"X11_Get_Shift: wierd mask value %lx\n",mask);
    i = 0;
  }
  *shift = i;
  j=0;
  while( (i < 32) && (mask & 0x01) ) 
  {
    mask >>= 1;
    i++;
    j++;
  }
  *size = j;
}

xaULONG X11_Get_True_Color(r,g,b,bits)
register xaULONG r,g,b,bits;
{
  register xaULONG temp,temp_color;

  temp = (x11_red_bits >= bits)?(r << (x11_red_bits - bits))
                               :(r >> (bits - x11_red_bits));
  temp_color  = (temp << x11_red_shift) & x11_red_mask;

  temp = (x11_green_bits >= bits)?(g << (x11_green_bits - bits))
                                 :(g >> (bits - x11_green_bits));
  temp_color |= (temp << x11_green_shift) & x11_green_mask;

  temp = (x11_blue_bits >= bits)?(b << (x11_blue_bits - bits))
                                :(b >> (bits - x11_blue_bits));
  temp_color |= (temp << x11_blue_shift) & x11_blue_mask;

  return(temp_color);
}


/****************************************************
 * Initialize X11 Display, etc.
 *
 *
 ****************/
void X11_Init(argcp, argv)
int *argcp;
char *argv[];
{
  XtToolkitInitialize();
  theContext = XtCreateApplicationContext();
  XtAppAddActions(theContext, actionTable, ACTIONTABLE_SIZE);

  theDisp = XtOpenDisplay(theContext, NULL, "xanim", "XAnim",NULL,0,argcp,argv);
  if (theDisp == NULL)
  {
    TheEnd1("Unable to open display\n");
  }

  XSetErrorHandler(XA_Error_Handler);
}

/****************************************************
 *
 ****************/
void X11_Pre_Setup(xa_user_visual,xa_user_class)
xaLONG xa_user_visual;
xaLONG xa_user_class;
{
  xaLONG i,vis_num,vis_i;
  XVisualInfo *vis;

 
#ifdef XSHM
  shm = 0;
  if (XShmQueryExtension(theDisp))
  {
    if (x11_shared_flag == xaTRUE) shm = 1;
  }
#endif

  if (xa_user_class >= 0) /* only attempt to use select visuals */
  {
    XVisualInfo vis_template;
    vis_template.class  = xa_user_class;
    vis_template.screen = DefaultScreen(theDisp);
    vis = XGetVisualInfo (theDisp, (VisualClassMask | VisualScreenMask), 
						&vis_template, &vis_num);
    if ((vis == NULL) || (vis_num == 0) )
	TheEnd1("X11: Couldn't get any Visuals of the desired class.");
  }
  else  /* look at them all (for the screen in question) */
  {
    XVisualInfo vis_template;
    vis_template.screen = DefaultScreen(theDisp);
    vis = XGetVisualInfo (theDisp, VisualScreenMask, &vis_template, &vis_num);
    if ((vis == NULL) || (vis_num == 0) )
		TheEnd1("X11: Couldn't get any Visuals.");
  }

  vis_i = xa_user_visual;
  if (vis_i >= vis_num) TheEnd1("X11: Couldn't get requested Visual.");
  if (vis_i < 0)
  {
    int screen;
    xaLONG max_csize,max_depth,max_class;

    max_class = -1;
    max_depth = 0;
    max_csize = 0;
    screen = DefaultScreen(theDisp);

    /* look through visuals a choose a good one */
    for(i=0;i<vis_num;i++)
    {
      if (vis[i].screen != screen) continue; /* Visual for a diff screen */
      if (vis[i].depth > max_depth)
      {
	max_depth = vis[i].depth;
	max_class = vis[i].class;
	max_csize = vis[i].colormap_size;
	vis_i = i;
      }
      else if (vis[i].depth == max_depth)
      {
	if (vis[i].class > max_class) 
	{
	  if (   (vis[i].class < 4)	/* pseudo or less */
	      || (vis[i].depth > 8) )
	  {
	    max_class = vis[i].class;
	    max_csize = vis[i].colormap_size;
	    vis_i = i;
	  }
	}
	else if (vis[i].class == max_class)
	{
	  if (vis[i].colormap_size > max_csize)
	  {
	    max_csize = vis[i].colormap_size;
	    vis_i = i;
	  } 
	} /* end same class */
      } /* end same depth */
    } /* end of vis loop */
  } /* no valid user visuals */

  /* setup up X11 variables */

  theScreen = vis[vis_i].screen;
  theVisual = vis[vis_i].visual;
  x11_depth = vis[vis_i].depth;
  x11_class = vis[vis_i].class;
  x11_cmap_size   = vis[vis_i].colormap_size;
  /* POD - For testing purposes only */
  if ( (pod_max_colors > 0) && (pod_max_colors < x11_cmap_size) )
		x11_cmap_size = pod_max_colors;
  theGC  = 0; /* DefaultGC(theDisp,theScreen); */

  /* Make sure x11_cmap_size is power of two */
  { 
    xaULONG size;
    size = 0x01; x11_disp_bits = 0;
    while(size <= x11_cmap_size) { size <<= 1; x11_disp_bits++; }
    size >>=1; x11_disp_bits--;
    x11_cmap_size = 0x01 << x11_disp_bits;
  }

  x11_bit_order   = BitmapBitOrder(theDisp);
  if (x11_bit_order == MSBFirst) x11_bit_order = X11_MSB;
  else x11_bit_order = X11_LSB;
  x11_bitmap_unit = BitmapUnit(theDisp);
  x11_depth_mask = (0x01 << x11_depth) - 1;
  x11_cmap_type = 0;

  XFree( (void *)vis);  

  if (x11_depth == 1)
  {
    x11_display_type = XA_MONOCHROME;
    x11_bytes_pixel = 1; x11_bitmap_pad = x11_bitmap_unit; 
    x11_cmap_flag = xaFALSE;
    x11_black = BlackPixel(theDisp,DefaultScreen(theDisp));
    x11_white = WhitePixel(theDisp,DefaultScreen(theDisp));
    x11_bits_per_pixel = 1;
    x11_byte_order = x11_bit_order;
  }
  else 
  {
    if (x11_depth > 16)
        { x11_bytes_pixel = 4; x11_bitmap_pad = 32; }
    else if (x11_depth > 8)
        { x11_bytes_pixel = 2; x11_bitmap_pad = 16; }
    else { x11_bytes_pixel = 1; x11_bitmap_pad = 8; }

    theImage = XCreateImage(theDisp,theVisual,
			x11_depth,ZPixmap,0,0,7,7,
			x11_bitmap_pad,0);
    if (theImage != 0)
    {
      x11_bits_per_pixel = theImage->bits_per_pixel;
      x11_byte_order = theImage->byte_order;
      if (x11_byte_order == MSBFirst) x11_byte_order = X11_MSB;
      else x11_byte_order = X11_LSB;
      if (x11_verbose_flag == xaTRUE)
      {
	fprintf(stderr,"bpp=%ld   bpl=%ld   byteo=%ld  bito=%ld\n",
		x11_bits_per_pixel,theImage->bytes_per_line,
		x11_byte_order,theImage->bitmap_bit_order);
      }
      XDestroyImage(theImage); theImage = NULL;
    } else x11_bits_per_pixel = x11_depth;

    switch(x11_class)
    {

      case StaticGray:
	x11_display_type = XA_STATICGRAY;
	x11_cmap_flag = xaFALSE;
	break;
      case GrayScale:
	x11_display_type = XA_GRAYSCALE;
	x11_cmap_flag = xaTRUE;
	break;
      case StaticColor:
	x11_display_type = XA_STATICCOLOR;
	x11_cmap_flag = xaFALSE;
	break;
      case PseudoColor:
	x11_display_type = XA_PSEUDOCOLOR;
	x11_cmap_flag = xaTRUE;
	break;
      case TrueColor:
	x11_display_type = XA_TRUECOLOR;
	x11_cmap_flag = xaFALSE;
	break;
      case DirectColor:
	x11_display_type = XA_DIRECTCOLOR;
	x11_cmap_flag = xaFALSE;
	break;
      default:
	fprintf(stderr,"Unkown x11_class %lx\n",x11_class);
	TheEnd();
    }
  }

  if (x11_display_type & XA_X11_TRUE)
  {
    x11_red_mask = theVisual->red_mask;
    x11_green_mask = theVisual->green_mask;
    x11_blue_mask = theVisual->blue_mask;
    X11_Get_Shift(x11_red_mask  , &x11_red_shift  , &x11_red_bits  );
    X11_Get_Shift(x11_green_mask, &x11_green_shift, &x11_green_bits);
    X11_Get_Shift(x11_blue_mask , &x11_blue_shift , &x11_blue_bits );
  }
  else if ( (x11_depth == 24) && (x11_cmap_size <= 256) ) x11_cmap_type = 1;

  xa_cmap = (ColorReg *) malloc( x11_cmap_size * sizeof(ColorReg) );
  if (xa_cmap==0) fprintf(stderr,"X11 CMAP: couldn't malloc\n");

  if (x11_verbose_flag == xaTRUE)
  {
    fprintf(stderr,"Selected Visual:  ");
    X11_OutPut_Visual_Class(x11_class);
    fprintf(stderr," (%lx) \n",x11_display_type);
    fprintf(stderr,"  depth= %ld  class= %ld  cmap size=%ld(%ld) bytes_pixel=%ld\n",
        x11_depth, x11_class, x11_cmap_size, x11_disp_bits, x11_bytes_pixel );
    if (x11_display_type & XA_X11_TRUE)
    {
      fprintf(stderr,"  X11 Color Masks =%lx %lx %lx\n",
                  x11_red_mask,x11_green_mask ,x11_blue_mask);
      fprintf(stderr,"  X11 Color Shifts=%ld %ld %ld\n",
                  x11_red_shift, x11_green_shift, x11_blue_shift );
      fprintf(stderr,"  X11 Color Sizes =%ld %ld %ld\n",
                  x11_red_bits,x11_green_bits ,x11_blue_bits);
    }
    else if (x11_display_type == XA_MONOCHROME)
    {
      fprintf(stderr,"  Bit Order = %lx  bitmapunit = %lx bitmappad = %lx\n",
                x11_bit_order,x11_bitmap_unit,BitmapPad(theDisp) );
    }
    fprintf(stderr,"\n");
  }

  if (   (theVisual != DefaultVisual(theDisp,theScreen))
      || (x11_display_type & XA_X11_TRUE)
      || (x11_display_type == XA_MONOCHROME) ) xa_allow_nice = xaFALSE;
  else xa_allow_nice = xaTRUE;

  /* kludges */
  if (   (!(x11_display_type & XA_X11_TRUE))
      && (x11_depth == 24) && (x11_cmap_size <= 256) ) x11_kludge_1 = xaTRUE;
  XSync(theDisp,False);
}

/*
 * Setup X11 Display, Window and Toolkit
 */

void X11_Setup_Window(max_imagex,max_imagey,startx,starty)
xaLONG max_imagex,max_imagey;
xaLONG startx,starty;
{
  xaLONG n;
  Arg arglist[20];
  XWMHints xwm_hints;

  if (   (   (theVisual == DefaultVisual(theDisp,theScreen))
          && (cmap_play_nice == xaTRUE) ) 
      || (x11_display_type == XA_MONOCHROME)
     )
  {
    DEBUG_LEVEL1 fprintf(stderr,"using default cmap\n");
    theCmap = DefaultColormap(theDisp,theScreen);
    x11_cmap_flag = xaFALSE; /* if nice */
  }
  else
  {
    DEBUG_LEVEL1 fprintf(stderr,"creating new cmap\n");
    if (x11_display_type & XA_X11_STATIC)
    {
       theCmap = XCreateColormap(theDisp,RootWindow(theDisp,theScreen),
							theVisual, AllocNone);
    }
    else if (   (x11_display_type == XA_DIRECTCOLOR)
             && (theVisual != DefaultVisual(theDisp,theScreen))  )
    {     /* Fake Direct Color, usually on top of a PseudoColor */
      xaULONG i,r_scale,g_scale,b_scale;
      xaULONG rmsk,gmsk,bmsk;

      theCmap = XCreateColormap(theDisp,RootWindow(theDisp,theScreen),
							theVisual, AllocAll);
      r_scale = cmap_scale[x11_red_bits];
      g_scale = cmap_scale[x11_green_bits];
      b_scale = cmap_scale[x11_blue_bits];
      rmsk = (0x01 << x11_red_bits)   - 1;
      gmsk = (0x01 << x11_green_bits) - 1;
      bmsk = (0x01 << x11_blue_bits)  - 1;
      for(i=0; i < x11_cmap_size; i++)
      {
        defs[i].pixel  =  (i << x11_red_shift)   & x11_red_mask;
        defs[i].pixel |=  (i << x11_green_shift) & x11_green_mask;
        defs[i].pixel |=  (i << x11_blue_shift)  & x11_blue_mask;
        defs[i].red   = (i & rmsk) * r_scale;
        defs[i].green = (i & gmsk) * g_scale;
        defs[i].blue  = (i & bmsk) * b_scale;
        defs[i].flags = DoRed | DoGreen | DoBlue;
      }
      XStoreColors(theDisp,theCmap,defs,x11_cmap_size);
    }
    else 
    {
       theCmap = XCreateColormap(theDisp,RootWindow(theDisp,theScreen),
							theVisual, AllocAll);
    }
    if (theCmap == 0) TheEnd1("X11_Setup_Window: create cmap err");
  }

  n = 0;
#ifdef XtNvisual
  XtSetArg(arglist[n], XtNvisual, theVisual); n++;
#endif
  XtSetArg(arglist[n], XtNcolormap, theCmap); n++;
  XtSetArg(arglist[n], XtNdepth, x11_depth); n++;
  XtSetArg(arglist[n], XtNforeground, WhitePixel(theDisp,theScreen)); n++;
  XtSetArg(arglist[n], XtNbackground, BlackPixel(theDisp,theScreen)); n++;
  XtSetArg(arglist[n], XtNborderColor, WhitePixel(theDisp,theScreen)); n++;
  XtSetArg(arglist[n], XtNwidth, startx); n++;
  XtSetArg(arglist[n], XtNheight, starty); n++;
  XtSetArg(arglist[n], XtNx, XA_MAINW_XPOS); n++;
  XtSetArg(arglist[n], XtNy, XA_MAINW_YPOS); n++;
  if (xa_allow_resizing==xaTRUE)
  {
    XtSetArg(arglist[n], XtNallowShellResize, True); n++;
  }
  else
  {
    XtSetArg(arglist[n], XtNmaxWidth, max_imagex); n++;
    XtSetArg(arglist[n], XtNmaxHeight, max_imagey); n++;
  }
  XtSetArg(arglist[n], XtNtranslations, 
			XtParseTranslationTable(Translation)); n++;
  theWG = XtAppCreateShell("xanim", "XAnim", applicationShellWidgetClass, 
                         theDisp, arglist, n);

  XtGetApplicationResources (theWG, &resource, application_resources,
                             XtNumber(application_resources), NULL, 0);


  XtRealizeWidget(theWG);
  mainW = XtWindow(theWG);

#ifdef XMBUF
  mainWreal = mainW;
  if (mbuf) {
    DEBUG_LEVEL1 fprintf(stderr, "Creating multiple buffers\n");
    if (XmbufCreateBuffers(theDisp, mainW, 2,
      MultibufferUpdateActionBackground, MultibufferUpdateHintFrequent,
      mainWbuffers) < 2) {
      fprintf(stderr,"X11 Setup: creating multiple buffers failed\n");
      mbuf = 0;
    } else
      mainW = mainWbuffers[mainWbufIndex = 0];
  }
#endif


  /* Need to Create New GC for Visuals that have a different depth than
   * the default GC
   */
  {
    xaULONG gc_mask;
    XGCValues gc_init;
 
    gc_mask = 0;
    gc_init.function = GXcopy;                          gc_mask |= GCFunction;
    gc_init.foreground = WhitePixel(theDisp,theScreen); gc_mask |= GCForeground;
    gc_init.background = BlackPixel(theDisp,theScreen); gc_mask |= GCBackground;
    gc_init.graphics_exposures = False;         gc_mask |= GCGraphicsExposures;
    theGC  = XCreateGC(theDisp,mainW,gc_mask,&gc_init);
  }


  if (x11_display_type == XA_MONOCHROME)
  { xaULONG line_size;
    line_size = X11_Get_Line_Size(max_imagex);
    theImage = XCreateImage(theDisp,theVisual,
			x11_depth,XYPixmap,0,0,max_imagex,max_imagey,
			x11_bitmap_pad,line_size);
    if (theImage == 0)
    {
      fprintf(stderr,"X11 Setup: create XY image failed\n");
      TheEnd();
    }
  }
  else
  { xaULONG line_size;
    line_size = X11_Get_Line_Size(x11_bytes_pixel * max_imagex);
    if (x11_bits_per_pixel==2) line_size = (line_size + 3) / 4;
    else if (x11_bits_per_pixel==4) line_size = (line_size + 1) / 2;

    theImage = XCreateImage(theDisp,theVisual,
			x11_depth,ZPixmap,0,0,max_imagex,max_imagey,
			x11_bitmap_pad,line_size);
    if (theImage == 0)
    {
      fprintf(stderr,"X11 Setup: create Z image failed\n");
      TheEnd();
    }
  }

  xwm_hints.input = True;	/* ask for keyboard input */
/*xwm_hints.icon_pixmap = ???;      Eventually have a icon pixmap */
  xwm_hints.flags = InputHint;   /* IconPixmapHint */
  XSetWMHints(theDisp,mainW,&xwm_hints);
  XSync(theDisp,False);
}

void X11_Map_Window()
{
  
  XMapWindow(theDisp,mainW);
  XSync(theDisp,False);

/*POD TEST */

  if (x11_cmap_flag == xaFALSE) /* static or monochrome displays */
  {
    xaULONG i;
    for(i=0;i<x11_cmap_size;i++)
    { 
      if (x11_display_type & XA_X11_TRUE)
      {
        xaULONG d;
        d  =  (i << x11_red_shift)   & x11_red_mask;
        d |=  (i << x11_green_shift) & x11_green_mask;
        d |=  (i << x11_blue_shift)  & x11_blue_mask;
        defs[i].pixel =  d;
      }
      else defs[i].pixel = i;
      defs[i].flags = DoRed | DoGreen | DoBlue; 
    }
    XQueryColors(theDisp,theCmap,defs,x11_cmap_size);
    for(i=0;i<x11_cmap_size;i++)
    {
      xaULONG r,g,b;
      r = xa_cmap[i].red   = defs[i].red;
      g = xa_cmap[i].green = defs[i].green;
      b = xa_cmap[i].blue  = defs[i].blue;
      xa_cmap[i].gray = (xaUSHORT)( ( (r * 11) + (g * 16) + (b * 5) ) >> 5 );
    }
  }
  else /* Copy Default Colormap into new one */
  {
    xaULONG i,csize;

    csize = CellsOfScreen(DefaultScreenOfDisplay(theDisp));
    if (csize > x11_cmap_size) csize = x11_cmap_size;
    for(i=0; i<csize; i++)
    {
      if (x11_display_type & XA_X11_TRUE)
      {
        xaULONG d;
        d  =  (i << x11_red_shift)   & x11_red_mask;
        d |=  (i << x11_green_shift) & x11_green_mask;
        d |=  (i << x11_blue_shift)  & x11_blue_mask;
        defs[i].pixel =  d;
      }
      else defs[i].pixel = i;
      defs[i].flags = DoRed | DoGreen | DoBlue;
    }
    XQueryColors(theDisp,DefaultColormap(theDisp,theScreen),defs,csize);
    for(i=0; i<csize; i++)
    {
      xaULONG r,g,b;
      r = xa_cmap[i].red   = defs[i].red;
      g = xa_cmap[i].green = defs[i].green;
      b = xa_cmap[i].blue  = defs[i].blue;
      xa_cmap[i].gray = (xaUSHORT)( ( (r * 11) + (g * 16) + (b * 5) ) >> 5 );
      if (x11_display_type & XA_X11_GRAY)
      {
        defs[i].red   = xa_cmap[i].gray;
        defs[i].green = xa_cmap[i].gray;
        defs[i].blue  = xa_cmap[i].gray;
      }
      defs[i].pixel = i; /* probably redundant */
      defs[i].flags = DoRed | DoGreen | DoBlue;
    }
    XStoreColors(theDisp,theCmap,defs,csize);
  }
  XSetWindowColormap(theDisp, mainW, theCmap);
#ifndef NO_INSTALL_CMAP
  XInstallColormap(theDisp,theCmap);
#endif
  XSync(theDisp,False);
}

void xanim_resize(wg, event, str, np)
Widget		wg;
XConfigureEvent	*event;
String		*str;
int		*np;
{
  if (xa_anim_status == XA_UNSTARTED)  return;
#ifdef XMBUF
  if (event->window == mainWreal)
#else
  if (event->window == mainW)
#endif
  {
    x11_window_x = event->width;
    x11_window_y = event->height;
    DEBUG_LEVEL1 
	fprintf(stderr,"X11 RESIZE %ldx%ld\n",x11_window_x,x11_window_y);
  }
}


/* Event Handler */
void xa_remote_expose(wg, closure, event, notused)
Widget          wg;
XtPointer	closure;
XEvent		*event;
Boolean		*notused;
{
  xa_remote_ready = xaTRUE;
}

static void xanim_realize_remote(wg, event, str, np)
Widget          wg;
XExposeEvent   *event;
String         *str;
int            *np;
{
  if (xa_remote_ready != xaTRUE) return;
  if (xa_remote_busy == xaTRUE) return;
#ifdef XA_REMOTE_CONTROL
  if (xa_remote_realized == xaFALSE)
  {
    xa_remote_busy = xaTRUE;
    XA_Realize_Remote(remote_widget,last_widget);
    xa_remote_busy = xaFALSE;
  }
  else
  {
    xa_remote_busy = xaTRUE;
    XA_Unrealize_Remote(remote_widget,last_widget);
    xa_remote_busy = xaFALSE;
  }
#endif
}



/* POD QUESTION THIS */
void xanim_expose(wg, event, str, np)
Widget		wg;
XExposeEvent	*event;
String		*str;
int		*np;
{
 x11_expose_flag = xaTRUE;
 if (xa_anim_status == XA_UNSTARTED) 
 {
  xa_anim_status = XA_BEGINNING;
  xa_anim_holdoff = xaTRUE;
  XtAppAddTimeOut(theContext, 1, (XtTimerCallbackProc)ShowAction,
						(XtPointer)(XA_SHOW_NORM));
 }
}

/*    -       -       -       -       -       -       -       -       */
/*
 * gf: Broke xanim_button() and xanim_key() into more specific action
 *     procedures.
 */
#define ACTION_PROC(NAME)     static void NAME(w,event,params,num_params) \
                                      Widget w; \
                                      XEvent *event; \
                                      String *params; \
                                      Cardinal *num_params;

ACTION_PROC(xanim_step_prev_action)
{
  if (xa_anim_status & XA_STOP_MASK) /* if stopped */
  { int oldstatus = xa_anim_status;
    xa_anim_status = XA_STEP_PREV;
    if (xa_anim_holdoff == xaTRUE) return;
    xa_anim_holdoff = xaTRUE;
    XtAppAddTimeOut(theContext, 1, (XtTimerCallbackProc)ShowAction, 
						(XtPointer)XA_SHOW_NORM);
#ifdef XA_REMOTE_CONTROL
    if (oldstatus & XA_NEXT_MASK) XA_Remote_Change(play_widget,BM_BACK);
#endif
  }
  else
  {
    xa_anim_status = XA_STOP_PREV;
#ifdef XA_REMOTE_CONTROL
    XA_Remote_Change(play_widget,BM_BACK);
#endif
  }
}

ACTION_PROC(xanim_step_next_action)
{ 
  if (xa_anim_status & XA_STOP_MASK) /* if stopped */
  { int oldstatus = xa_anim_status;
    xa_anim_status = XA_STEP_NEXT;
    if (xa_anim_holdoff == xaTRUE) return;
    xa_anim_holdoff = xaTRUE;
    XtAppAddTimeOut(theContext, 1, (XtTimerCallbackProc)ShowAction, 
						(XtPointer)XA_SHOW_NORM);
#ifdef XA_REMOTE_CONTROL
    if (!(oldstatus & XA_NEXT_MASK)) XA_Remote_Change(play_widget,BM_PLAY);
#endif
  }
  else 
  { 
    xa_anim_status = XA_STOP_NEXT;
#ifdef XA_REMOTE_CONTROL
    XA_Remote_Change(play_widget,BM_PLAY);
#endif
  }
}

ACTION_PROC(xanim_toggle_action)
{ int button;
  switch(xa_anim_status)
  {
    case XA_RUN_PREV:  /* if running, then stop */
    case XA_RUN_NEXT:
      button = (xa_anim_status & XA_RUN_MASK)?(BM_PLAY):(BM_BACK); 
      xa_anim_status &= XA_CLEAR_MASK;
      xa_anim_status |= XA_STOP_MASK;
      xa_anim_flags &= ~(ANIM_CYCLE);
      break;
    case XA_STOP_PREV: /* if stopped, then run */
    case XA_STOP_NEXT:
      button = BM_STOP;
      xa_anim_status &= XA_CLEAR_MASK;
      xa_anim_status |= XA_RUN_MASK;
      if (xa_anim_holdoff == xaTRUE) return;
      xa_anim_holdoff = xaTRUE;
      XtAppAddTimeOut(theContext, 1, (XtTimerCallbackProc)ShowAction, 
						(XtPointer)XA_SHOW_NORM);
      break;
    case XA_STEP_PREV: /* if single stepping then run */
    case XA_STEP_NEXT:
    case XA_ISTP_PREV:
    case XA_ISTP_NEXT:
    case XA_FILE_PREV:
    case XA_FILE_NEXT:
      button = BM_STOP;
      xa_anim_status &= XA_CLEAR_MASK;
      xa_anim_status |= XA_RUN_MASK;
      break;
    default:
      if (xa_anim_status & XA_NEXT_MASK)
	{ button = BM_PLAY; xa_anim_status = XA_STOP_NEXT; }
      else
	{ button = BM_BACK; xa_anim_status = XA_STOP_PREV; }
      break;
  }
  /* change Play Widget pixmap */
#ifdef XA_REMOTE_CONTROL
  XA_Remote_Change(play_widget,button);
#endif
  if (xa_title_flag == XA_TITLE_FILE)
  {
    if (xa_anim_status & XA_RUN_MASK)
    {
      sprintf(xa_title,"XAnim: %s",cur_file->name);
      XStoreName(theDisp,mainW,xa_title);
    }
  }
}

ACTION_PROC(xanim_quit_action)
{
  TheEnd();
}

ACTION_PROC(xanim_resize_action)
{
  if ( (xa_buff_x != 0) && (xa_buff_y != 0) )
    XResizeWindow(theDisp,mainW,xa_buff_x,xa_buff_y);
}

ACTION_PROC(xanim_install_cmap_action)
{
  XInstallColormap(theDisp,theCmap);
}

ACTION_PROC(xanim_stop_cmap_action)
{
  xa_anim_flags &= ~(ANIM_CYCLE);
}

ACTION_PROC(xanim_restore_cmap_action)
{
  if (xa_chdr_now != 0) XA_Install_CMAP(xa_chdr_now);
}

/* decrease speed scale, but not to zero */
ACTION_PROC(xanim_slower_action)
{
#ifdef XA_REMOTE_CONTROL
  if (xa_speed_scale==XA_SPEED_NORM) XA_Remote_Change(norm_widget,BM_NORM);
#endif
  xa_speed_change = 1;
  if (xa_speed_scale > XA_SPEED_MIN) xa_speed_scale >>= 1;
  xa_speed_change = 2;
#ifdef XA_REMOTE_CONTROL
  if (xa_speed_scale==XA_SPEED_NORM) XA_Remote_Change(norm_widget,BM_FUZZ);
#endif
}

/* increase speed scale, but not more than 1000 */
ACTION_PROC(xanim_faster_action)
{ 
#ifdef XA_REMOTE_CONTROL
  if (xa_speed_scale==XA_SPEED_NORM) XA_Remote_Change(norm_widget,BM_NORM);
#endif
  xa_speed_change = 1;
  if (xa_speed_scale < XA_SPEED_MAX) xa_speed_scale <<= 1;
  xa_speed_change = 2;
#ifdef XA_REMOTE_CONTROL
  if (xa_speed_scale==XA_SPEED_NORM) XA_Remote_Change(norm_widget,BM_FUZZ);
#endif
}

/* set to unity */
ACTION_PROC(xanim_speed_reset_action)
{
#ifdef XA_REMOTE_CONTROL
  if (xa_speed_scale != XA_SPEED_NORM) XA_Remote_Change(norm_widget,BM_FUZZ);
#endif
  xa_speed_change = 1;
  xa_speed_scale = XA_SPEED_NORM;
  xa_speed_change = 2;
}

/* single step across anims */
ACTION_PROC(xanim_next_anim_action)
{
  if (xa_anim_status & XA_STOP_MASK) /* if stopped */
  { int oldstatus = xa_anim_status;
    xa_anim_status = XA_FILE_NEXT;
    if (xa_anim_holdoff == xaTRUE) return;
    xa_anim_holdoff = xaTRUE;
    XtAppAddTimeOut(theContext, 1, (XtTimerCallbackProc)ShowAction, 
						(XtPointer)XA_SHOW_NORM);
#ifdef XA_REMOTE_CONTROL
    if (!(oldstatus & XA_NEXT_MASK)) XA_Remote_Change(play_widget,BM_PLAY);
#endif
  }
  else
  {
    xa_anim_status = XA_FILE_NEXT;
#ifdef XA_REMOTE_CONTROL
    XA_Remote_Change(play_widget,BM_PLAY);
#endif
  }
}

/* single step across anims */
ACTION_PROC(xanim_prev_anim_action)
{
  if (xa_anim_status & XA_STOP_MASK) /* if stopped */
  { int oldstatus = xa_anim_status;
    xa_anim_status = XA_FILE_PREV;
    if (xa_anim_holdoff == xaTRUE) return;
    xa_anim_holdoff = xaTRUE;
    XtAppAddTimeOut(theContext, 1, (XtTimerCallbackProc)ShowAction, 
						(XtPointer)XA_SHOW_NORM);
#ifdef XA_REMOTE_CONTROL
    if (oldstatus & XA_NEXT_MASK) XA_Remote_Change(play_widget,BM_BACK);
#endif
  }
  else
  {
    xa_anim_status = XA_FILE_PREV;
#ifdef XA_REMOTE_CONTROL
    XA_Remote_Change(play_widget,BM_BACK);
#endif
  }
}

/* single step within anim */
ACTION_PROC(xanim_step_next_int_action)
{
  if (xa_anim_status & XA_STOP_MASK) /* if stopped */
  { int oldstatus = xa_anim_status;
    xa_anim_status = XA_ISTP_NEXT;
    if (xa_anim_holdoff == xaTRUE) return;
    xa_anim_holdoff = xaTRUE;
    XtAppAddTimeOut(theContext, 1, (XtTimerCallbackProc)ShowAction, 
						(XtPointer)XA_SHOW_NORM);
#ifdef XA_REMOTE_CONTROL
    if (!(oldstatus & XA_NEXT_MASK)) XA_Remote_Change(play_widget,BM_PLAY);
#endif
  }
  else
  {
    xa_anim_status = XA_STOP_NEXT;
#ifdef XA_REMOTE_CONTROL
    XA_Remote_Change(play_widget,BM_PLAY);
#endif
  }
}

/* single step within anim */
ACTION_PROC(xanim_step_prev_int_action)
{
  if (xa_anim_status & XA_STOP_MASK) /* if stopped */
  { int oldstatus = xa_anim_status;
    xa_anim_status = XA_ISTP_PREV;
    if (xa_anim_holdoff == xaTRUE) return;
    xa_anim_holdoff = xaTRUE;
    XtAppAddTimeOut(theContext, 1, (XtTimerCallbackProc)ShowAction, 
						(XtPointer)XA_SHOW_NORM);
#ifdef XA_REMOTE_CONTROL
    if (oldstatus & XA_NEXT_MASK) XA_Remote_Change(play_widget,BM_BACK);
#endif
  }
  else
  {
    xa_anim_status = XA_STOP_PREV;
#ifdef XA_REMOTE_CONTROL
    XA_Remote_Change(play_widget,BM_BACK);
#endif
  }

}


/* decrement xa_audio_volume by 5 */
ACTION_PROC(xanim_dec_audio_5)
{
  vaudiof->volume -= 5; if (vaudiof->volume < 0) vaudiof->volume = 0;
  vaudiof->newvol = xaTRUE;
  XA_AUDIO_SET_VOLUME(vaudiof->volume);
}
/* decrement xa_audio_volume by 1 */
ACTION_PROC(xanim_dec_audio_1)
{
  vaudiof->volume -= 1; if (vaudiof->volume < 0) vaudiof->volume = 0;
  vaudiof->newvol = xaTRUE;
  XA_AUDIO_SET_VOLUME(vaudiof->volume);
}

/* increment xa_audio_volume by 5 */
ACTION_PROC(xanim_inc_audio_5)
{
  vaudiof->volume += 5;
  if (vaudiof->volume > XA_AUDIO_MAXVOL) vaudiof->volume = XA_AUDIO_MAXVOL;
  vaudiof->newvol = xaTRUE;
  XA_AUDIO_SET_VOLUME(vaudiof->volume);
}
/* increment xa_audio_volume by 1 */
ACTION_PROC(xanim_inc_audio_1)
{
  vaudiof->volume += 1;
  if (vaudiof->volume > XA_AUDIO_MAXVOL) vaudiof->volume = XA_AUDIO_MAXVOL;
  vaudiof->newvol = xaTRUE;
  XA_AUDIO_SET_VOLUME(vaudiof->volume);
}
/* mute audio */
ACTION_PROC(xanim_mute_audio)
{ int button = (vaudiof->mute==xaTRUE)?(BM_VOFF):(BM_VON);
  vaudiof->mute = (vaudiof->mute==xaTRUE)?(xaFALSE):(xaTRUE);
  vaudiof->newvol = xaTRUE;
  XA_AUDIO_SET_MUTE(vaudiof->mute);
#ifdef XA_REMOTE_CONTROL
  XA_Remote_Change(audio_widget,button);
#endif
}
/* toggle speaker */
ACTION_PROC(xanim_speaker_tog)
{
  XA_SPEAKER_TOG(2);
}
/* toggle headphone */
ACTION_PROC(xanim_headphone_tog)
{
  XA_HEADPHONE_TOG(2);
}

/*    -       -       -       -       -       -       -       -       */

void xanim_events()
{
 XtAppMainLoop(theContext);
}

/* PODNOTE:
 * macro this
 */
xaULONG X11_Get_Line_Size(xsize)
xaULONG xsize;
{
  xaULONG line_size;

  if (x11_display_type == XA_MONOCHROME)
       line_size = X11_Get_Bitmap_Width(xsize) / 8;
  else line_size = xsize * x11_bytes_pixel;
  return(line_size);
}

  /*
   * What's this!? Direct access to X11 structures. tsch tsch.
   */
void X11_Init_Image_Struct(image,xsize,ysize)
XImage *image;
xaULONG xsize,ysize;
{
  xaULONG line_size;
  line_size = X11_Get_Line_Size(xsize);
  /*PACK*/
  if (x11_bits_per_pixel==2) line_size = (line_size + 3) / 4;
  else if (x11_bits_per_pixel==4) line_size = (line_size + 1) / 2;
  image->width = xsize;
  image->height = ysize;
  image->bytes_per_line = line_size;
DEBUG_LEVEL2 fprintf(stderr," InitImage %lx <%ld,%ld>\n", (xaULONG)(image),xsize,ysize );
}


void X11_OutPut_Visual_Class(vis_class)
xaULONG vis_class;
{
  switch(vis_class)
  {
   case StaticGray:  fprintf(stderr,"StaticGray"); break;
   case GrayScale:   fprintf(stderr,"GrayScale"); break;
   case StaticColor: fprintf(stderr,"StaticColor"); break;
   case PseudoColor: fprintf(stderr,"PseudoColor"); break;
   case TrueColor:   fprintf(stderr,"TrueColor"); break;
   case DirectColor: fprintf(stderr,"DirectColor"); break;
  }
}

void X11_Show_Visuals()
{
  xaLONG i,vis_num;
  XVisualInfo *vis;

  vis = XGetVisualInfo (theDisp, VisualNoMask, NULL, &vis_num);
  if ((vis == NULL) || (vis_num == 0) )
  {
    fprintf(stderr,"X11: Couldn't get any Visuals\n");
    return;
  }
  else
  {
    for(i=0;i<vis_num;i++)
    {
      fprintf(stderr,"  visual %ld) depth= %ld  class= %ld  cmap size=%ld  ",
                       i, vis[i].depth, vis[i].class, vis[i].colormap_size );
      X11_OutPut_Visual_Class(vis[i].class);
      fprintf(stderr,"\n");
    }
  }
}

void X11_Get_Colormap(chdr)
XA_CHDR *chdr;
{
  ColorReg *cmap;
  xaULONG i,*map;

  /* grab the current cmap and lay it down */
  for(i=0;i<x11_cmap_size;i++)
  {
    if (x11_display_type & XA_X11_TRUE)
    {
      xaULONG d;
      d  =  (i << x11_red_shift)   & x11_red_mask;
      d |=  (i << x11_green_shift) & x11_green_mask;
      d |=  (i << x11_blue_shift)  & x11_blue_mask;
      defs[i].pixel =  d;
    }
    else defs[i].pixel = i;
    defs[i].flags = DoRed | DoGreen | DoBlue;
  }
  XQueryColors(theDisp,theCmap,defs,x11_cmap_size);
  
  cmap = chdr->cmap;
  map = chdr->map;
  if (cmap == 0) TheEnd1("X11_Get_Colormap: cmap = 0");
  if (map == 0) TheEnd1("X11_Get_Colormap: map = 0");
DEBUG_LEVEL2 fprintf(stderr,"X11_Get_Colormap:\n");
  for(i=0;i<x11_cmap_size;i++)
  {
    xaULONG r,g,b;
    r = cmap[i].red   = (xaUSHORT)defs[i].red; 
    g = cmap[i].green = (xaUSHORT)defs[i].green; 
    b = cmap[i].blue  = (xaUSHORT)defs[i].blue; 
    cmap[i].gray =  (xaUSHORT)( ((r * 11) + (g * 16) + (b * 5)) >> 5 ); 
    map[i] = i;
    DEBUG_LEVEL2 fprintf(stderr,"   %ld) <%lx %lx %lx> %lx\n",i,r,g,b,chdr->cmap[i].gray);
  }
}

void X11_Make_Nice_CHDR(chdr)
XA_CHDR *chdr;
{
  ColorReg *old_cmap,*new_cmap;
  xaULONG i,*old_map,*new_map;
  xaULONG old_csize,old_msize;

  if ( !(x11_display_type & XA_X11_CMAP)) return;
  
  old_cmap = chdr->cmap;
  old_map  = chdr->map;
  old_csize = chdr->csize;
  old_msize = chdr->msize;

  /* try allocating colors */
  for(i=0;i<old_csize;i++)
  {
    if (x11_display_type & XA_X11_GRAY)
    {
      defs[i].red = defs[i].green = defs[i].blue = old_cmap[i].gray;
    }
    else
    {
      defs[i].red   = old_cmap[i].red;
      defs[i].green = old_cmap[i].green;
      defs[i].blue  = old_cmap[i].blue;
    }
    defs[i].flags = DoRed | DoGreen | DoBlue;
    XAllocColor(theDisp,theCmap,&defs[i]);
  }
  
  /* Query the cmap */
  for(i=0;i<x11_cmap_size;i++)
  {
    if (x11_display_type & XA_X11_TRUE)
    {
      xaULONG d;
      d  =  (i << x11_red_shift)   & x11_red_mask;
      d |=  (i << x11_green_shift) & x11_green_mask;
      d |=  (i << x11_blue_shift)  & x11_blue_mask;
      defs[i].pixel =  d;
    }
    else defs[i].pixel = i;
    defs[i].flags = DoRed | DoGreen | DoBlue;
  }
  XQueryColors(theDisp,theCmap, defs,x11_cmap_size);

  if (old_csize != x11_cmap_size)
  {
    new_cmap = (ColorReg *)malloc(x11_cmap_size * sizeof(ColorReg) );
    if (new_cmap == 0) TheEnd1("X11_Make_Nice_CHDR: cmap malloc err");
    FREE(old_cmap,0x400); old_cmap=0;
    chdr->csize = x11_cmap_size;
    chdr->cmap = new_cmap;
  }

  if (old_msize != x11_cmap_size)
  {
    new_map = (xaULONG *)malloc(x11_cmap_size * sizeof(xaULONG) );
    if (new_map == 0) TheEnd1("X11_Make_Nice_CHDR: map malloc err");
    FREE(old_map,0x401); old_cmap=0;
    chdr->msize = x11_cmap_size;
    chdr->map = new_map;
  }
  
  DEBUG_LEVEL2 fprintf(stderr,"X11_Make_Nice_CHDR: \n");
  chdr->moff = chdr->coff = 0;
  for(i=0;i<x11_cmap_size;i++)
  {
    xaULONG r,g,b;
    r = chdr->cmap[i].red   = defs[i].red; 
    g = chdr->cmap[i].green = defs[i].green; 
    b = chdr->cmap[i].blue  = defs[i].blue; 
    chdr->cmap[i].gray = (xaUSHORT)( ( (r * 11) + (g * 16) + (b * 5) ) >> 5 );
    chdr->map[i] = i;
    DEBUG_LEVEL2 fprintf(stderr," %ld) <%lx %lx %lx> %lx\n",i,r,g,b,chdr->cmap[i].gray);
  }
}

#ifdef XA_REMOTE_CONTROL
/****************************************************************************
 *  XA_Create_Remote
 *
 *
 ****************************************************************************/

/****** ATHENA DEFINES **********/
#ifdef XA_ATHENA
#define XA_FORM_CLASS	formWidgetClass
#define XA_BUTTON_CLASS	commandWidgetClass

#define XA_REMOTE_TOPFORM(arg,n)    {}
#define XA_REMOTE_BOTTOMFORM(arg,n) {}
#define XA_REMOTE_LEFTFORM(arg,n)   {}
#define XA_REMOTE_RIGHTFORM(arg,n)  {}
#define XA_REMOTE_TOPWIDGET(arg,n,the_w) {XtSetArg(arg[n],XtNfromVert,the_w); n++;}
#define XA_REMOTE_LEFTWIDGET(arg,n,the_w) {XtSetArg(arg[n],XtNfromHoriz,the_w); n++;}

#define XA_REMOTE_PIXMAP(arg,n,pixmap)	{ \
 XtSetArg(arg[n],XtNbitmap, pixmap); n++; }

#endif  /******** END OF ATHENA **********/

/****** MOTIF DEFINES **********/
#ifdef XA_MOTIF
#define XA_FORM_CLASS	xmFormWidgetClass
#define XA_BUTTON_CLASS	xmPushButtonWidgetClass

#define XA_REMOTE_TOPFORM(arg,n)    {XtSetArg(arg[n],XmNtopAttachment,XmATTACH_FORM); n++;}
#define XA_REMOTE_BOTTOMFORM(arg,n) {XtSetArg(arg[n],XmNbottomAttachment,XmATTACH_FORM); n++;}
#define XA_REMOTE_LEFTFORM(arg,n)   {XtSetArg(arg[n],XmNleftAttachment,XmATTACH_FORM); n++;}
#define XA_REMOTE_RIGHTFORM(arg,n)  {XtSetArg(arg[n],XmNrightAttachment,XmATTACH_FORM); n++;}

#define XA_REMOTE_TOPWIDGET(arg,n,the_w)	{ \
  XtSetArg(arg[n],XmNtopAttachment,XmATTACH_WIDGET); n++; \
  XtSetArg(arg[n],XmNtopWidget,the_w); n++;	}

#define XA_REMOTE_LEFTWIDGET(arg,n,the_w)	{ \
  XtSetArg(arg[n],XmNleftAttachment,XmATTACH_WIDGET); n++; \
  XtSetArg(arg[n],XmNleftWidget,the_w); n++;	}

#define XA_REMOTE_PIXMAP(arg,n,pixmap)	{ \
 XtSetArg(arg[n],XmNlabelPixmap, pixmap); n++; \
 XtSetArg(arg[n],XmNlabelType,XmPIXMAP); n++;	}
#endif  /******** END OF MOTIF **********/


void XA_Create_Remote(wg,remote_flag)
Widget wg;
xaLONG remote_flag;
{ int i,n,nb,nc,foregnd,backgnd;
  Widget form,w,oldw;
  Arg arglist[20];
  Window t_window;

/* Query Black and White */
  if (x11_display_type == XA_MONOCHROME)
  {
    foregnd = WhitePixel(theDisp,theScreen);
    backgnd = BlackPixel(theDisp,theScreen);
  }
  else if (x11_display_type & XA_X11_TRUE)
  {
    backgnd = X11_Get_True_Color(0,0,0,8);
    foregnd = X11_Get_True_Color(255,255,255,8);
  }
  else if (xa_chdr_first)
  { XA_CHDR *chdr = xa_chdr_first;
    backgnd = CMAP_Find_Closest(chdr->cmap,chdr->csize,0,0,0,8,8,8,xaTRUE);
    foregnd = CMAP_Find_Closest(chdr->cmap,chdr->csize,255,255,255,8,8,8,xaTRUE);
  }
  else
  {
    foregnd = WhitePixel(theDisp,theScreen);
    backgnd = BlackPixel(theDisp,theScreen);
  }

 if (x11_verbose_flag == xaTRUE)
	fprintf(stderr,"foregnd = %lx backgnd = %lx\n",foregnd,backgnd);

 for(i=0; i < BM_NUMBER; i++)
 {
   BM_pmap[i] = XCreatePixmapFromBitmapData(theDisp,mainW,(char *)(BM_bits[i]),
			BM_width[i],BM_height[i],foregnd,backgnd,x11_depth);
 }



  nb = 0;
#ifdef XtNvisual
  XtSetArg(arglist[nb], XtNvisual, theVisual); nb++;
#endif
  XtSetArg(arglist[nb], XtNcolormap, theCmap); nb++;
  XtSetArg(arglist[nb], XtNdepth, x11_depth); nb++;
  XtSetArg(arglist[nb], XtNforeground, foregnd); nb++;
  XtSetArg(arglist[nb], XtNbackground, backgnd); nb++;
  XtSetArg(arglist[nb], XtNborderColor, foregnd); nb++;
  XtSetArg(arglist[nb], XtNx, XA_REMW_XPOS); nb++;
  XtSetArg(arglist[nb], XtNy, XA_REMW_YPOS); nb++;

/*POD TEST add Translations to remote_window */
  XtSetArg(arglist[nb], XtNtranslations,
                        XtParseTranslationTable(Translation)); nb++;

  remote_widget = XtCreatePopupShell("Control",topLevelShellWidgetClass,wg,arglist,nb);
  form = XtCreateManagedWidget("form",XA_FORM_CLASS,remote_widget,arglist,nb);

  t_window = mainW;
  /************* TOP ROW: ***/
  nc = nb; w = XtCreateManagedWidget("stepPrevButton",XA_BUTTON_CLASS, form,NULL,0);
  XA_REMOTE_TOPFORM(arglist,nc);
  n = nc;
  XA_REMOTE_LEFTFORM(arglist,n);
  XA_REMOTE_PIXMAP(arglist,n,BM_pmap[BM_STPP]);
  XtSetValues(w,arglist,n); oldw = w;
  XtOverrideTranslations(w, 
		XtParseTranslationTable("<Btn1Down>,<Btn1Up>: StepPrev()"));

  n = nc; play_widget = XtCreateManagedWidget("toggleButton",XA_BUTTON_CLASS,form,NULL,0);
  XA_REMOTE_LEFTWIDGET(arglist,n,oldw);
  XA_REMOTE_PIXMAP(arglist,n,BM_pmap[BM_STOP]);
  XtSetValues(play_widget,arglist,n); oldw = play_widget;
  XtOverrideTranslations(play_widget, 
		XtParseTranslationTable("<Btn1Down>,<Btn1Up>: RunStop()"));

  n = nc; w = XtCreateManagedWidget("stepNextButton",XA_BUTTON_CLASS,form,NULL,0);
  XA_REMOTE_RIGHTFORM(arglist,n);
  XA_REMOTE_LEFTWIDGET(arglist,n,oldw);
  XA_REMOTE_PIXMAP(arglist,n,BM_pmap[BM_STPN]);
  XtSetValues(w,arglist,n); oldw = w;
  XtOverrideTranslations(w, 
		XtParseTranslationTable("<Btn1Down>,<Btn1Up>: StepNext()"));
  /************** MIDDLE ROW: **********/
  nc = nb; w = XtCreateManagedWidget("prevAnimButton",XA_BUTTON_CLASS,form,NULL,0);
  XA_REMOTE_TOPWIDGET(arglist,nc,oldw);
  n = nc;
  XA_REMOTE_LEFTFORM(arglist,n);
  XA_REMOTE_PIXMAP(arglist,n,BM_pmap[BM_PREV]);
  XtSetValues(w,arglist,n); oldw = w;
  XtOverrideTranslations(w, 
		XtParseTranslationTable("<Btn1Down>,<Btn1Up>: PrevAnim()"));

  n = nc; w = XtCreateManagedWidget("quitButton",XA_BUTTON_CLASS,form,NULL,0);
  XA_REMOTE_LEFTWIDGET(arglist,n,oldw);
  XA_REMOTE_PIXMAP(arglist,n,BM_pmap[BM_QUIT]);
  XtSetValues(w,arglist,n); oldw = w;
  XtOverrideTranslations(w, 
		XtParseTranslationTable("<Btn1Down>,<Btn1Up>: Quit()"));

  n = nc; w = XtCreateManagedWidget("nextAnimButton",XA_BUTTON_CLASS,form,NULL,0);
  XA_REMOTE_RIGHTFORM(arglist,n);
  XA_REMOTE_LEFTWIDGET(arglist,n,oldw);
  XA_REMOTE_PIXMAP(arglist,n,BM_pmap[BM_NEXT]);
  XtSetValues(w,arglist,n); oldw = w;
  XtOverrideTranslations(w, 
		XtParseTranslationTable("<Btn1Down>,<Btn1Up>: NextAnim()"));
  /*************** BOTTOM ROW: ********/
  nc = nb; w = XtCreateManagedWidget("slowerButton",XA_BUTTON_CLASS,form,NULL,0);
  XA_REMOTE_TOPWIDGET(arglist,nc,oldw);
  n = nc;
  XA_REMOTE_LEFTFORM(arglist,n);
  XA_REMOTE_PIXMAP(arglist,n,BM_pmap[BM_SLOW]);
  XtSetValues(w,arglist,n); oldw = w;
  XtOverrideTranslations(w, 
		XtParseTranslationTable("<Btn1Down>,<Btn1Up>: Slower()"));

  n = nc; norm_widget = XtCreateManagedWidget("speedResetButton",XA_BUTTON_CLASS,form,NULL,0);
  XA_REMOTE_LEFTWIDGET(arglist,n,oldw);
  XA_REMOTE_PIXMAP(arglist,n,BM_pmap[BM_FUZZ]);
  XtSetValues(norm_widget,arglist,n); oldw = norm_widget;
  XtOverrideTranslations(norm_widget, 
		XtParseTranslationTable("<Btn1Down>,<Btn1Up>: SpeedReset()"));

  n = nc; w = XtCreateManagedWidget("fasterButton",XA_BUTTON_CLASS,form,NULL,0);
  XA_REMOTE_RIGHTFORM(arglist,n);
  XA_REMOTE_LEFTWIDGET(arglist,n,oldw);
  XA_REMOTE_PIXMAP(arglist,n,BM_pmap[BM_FAST]);
  XtSetValues(w,arglist,n); oldw = w;
  XtOverrideTranslations(w, 
		XtParseTranslationTable("<Btn1Down>,<Btn1Up>: Faster()"));
  /********* AUDIO VERY BOTTOM ROW: ****/
  nc = nb; w = XtCreateManagedWidget("decAudioButton",XA_BUTTON_CLASS,form,NULL,0);
  XA_REMOTE_TOPWIDGET(arglist,nc,oldw);
  XA_REMOTE_BOTTOMFORM(arglist,nc);
  n = nc;
  XA_REMOTE_LEFTFORM(arglist,n);
  XA_REMOTE_PIXMAP(arglist,n,BM_pmap[BM_VDOWN]);
  XtSetValues(w,arglist,n); oldw = w;
  XtOverrideTranslations(w, 
		XtParseTranslationTable("<Btn1Down>,<Btn1Up>: DecAudio5()"));

  n = nc; audio_widget = XtCreateManagedWidget("audioMuteButton",XA_BUTTON_CLASS,form,NULL,0);
  XA_REMOTE_LEFTWIDGET(arglist,n,oldw);
  XA_REMOTE_PIXMAP(arglist,n,BM_pmap[BM_VOFF]);
  XtSetValues(audio_widget,arglist,n); oldw = audio_widget;
  XtOverrideTranslations(audio_widget, 
		XtParseTranslationTable("<Btn1Down>,<Btn1Up>: AudioMute()"));

  n = nc; last_widget = XtCreateManagedWidget("incAudioButton",XA_BUTTON_CLASS,form,NULL,0);
  XA_REMOTE_RIGHTFORM(arglist,n);
  XA_REMOTE_LEFTWIDGET(arglist,n,oldw);
  XA_REMOTE_PIXMAP(arglist,n,BM_pmap[BM_VUP]);
  XtSetValues(last_widget,arglist,n); oldw = last_widget;
  XtOverrideTranslations(last_widget, 
		XtParseTranslationTable("<Btn1Down>,<Btn1Up>: IncAudio5()"));

  /* THIS IS VERY IMPORTANT AND IS TO DELAY STARTUP UNTIL AN EXPOSE EVENT
   * HAPPENS so everything is realized and mapped before animation starts.
   * xa_remote_ready is set to xaTRUE in the xa_remote_expose routine.
   */
  XtAddRawEventHandler(last_widget, ExposureMask, False, xa_remote_expose, NULL);
  /******** REALIZE THE WIDGET *******/
  if (remote_flag == xaTRUE) XA_Realize_Remote(remote_widget,last_widget);
  else xa_remote_ready = xaTRUE;
}

void XA_Realize_Remote(remote,last)
Widget remote,last;
{
  xa_remote_realized = xaTRUE;
  XtPopup(remote,XtGrabNone);
  XSync(theDisp,False);
  while(XtIsRealized(remote)==False) XSync(theDisp,False);
  while(XtIsRealized(last)==False) XSync(theDisp,False);
  XRaiseWindow(theDisp, XtWindow(remote) );
}

void XA_Unrealize_Remote(remote,last)
Widget remote,last;
{
  XSync(theDisp,False);
  XtPopdown(remote);
  XSync(theDisp,False);
  XtUnrealizeWidget(remote);
  XSync(theDisp,False);
  while(XtIsRealized(remote)==True) XSync(theDisp,False);
  xa_remote_realized = xaFALSE;
}

/* convenience function so xanim.c code can change play button during
 * pauses. */
void XA_Remote_Pause()
{
  XA_Remote_Change(play_widget,BM_PLAY);
}

/* Change pixmap of a button */
void XA_Remote_Change(widg,button)
Widget widg;
int button;
{ Arg arglist[5];
  int n = 0;
  XA_REMOTE_PIXMAP(arglist,n,BM_pmap[button]);
  XtSetValues(widg,arglist,n);
}
#endif

int XA_Error_Handler(errDisp,event)
Display *errDisp;
XErrorEvent *event;
{
  char errbuff[255];
  if (x11_error_possible != 1) 
	XGetErrorText(errDisp,event->error_code, errbuff, 255);
  x11_error_possible = -1;
  return(0);
}

