
/*
 * xa_x11.c
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
 * 12Feb95 - fixed calling params of Callbacks and Actions.
 * 22Jun95 - Translations added to Remote Control Window as well so
 *           now all keyboards commands should work there as well.
 * 14Jun98 - Moheb Mekhaiel submitted patch fo the case where local machine
 *           is of a different endianness than the remote machine.
 *******************************/


#include "xanim.h"
#include <Intrinsic.h>
#include <StringDefs.h>
#include <Shell.h>
#include <Xatom.h>

#ifdef XA_ATHENA
#include <Xaw/Form.h>
#include <Xaw/Command.h>
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

#include "xa_ipc.h"

extern xaULONG shm;
extern xaULONG mbuf;
extern XA_AUD_FLAGS *vaudiof;
extern xaLONG xa_pingpong_flag;


#include "xa_ipc_cmds.h"
extern xaULONG xa_forkit;

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

/* SMR 8 */
/* externs from xanim.c covering playing into another app's window */
extern xaLONG xa_window_x;
extern xaLONG xa_window_y;
extern xaULONG xa_window_id;
extern char *xa_window_propname;
extern xaULONG xa_window_center_flag;
extern xaULONG xa_window_prepare_flag;
extern xaULONG xa_window_refresh_flag;
extern xaULONG xa_window_resize_flag;
/* end SMR 8 */

int XA_Error_Handler();

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
void XA_Free_CMAP();

#ifdef XA_REMOTE_CONTROL

Widget play_widget,audio_widget,norm_widget;
Widget remote_widget,last_widget;

void XA_Create_Remote();
void XA_Realize_Remote();
void XA_Unrealize_Remote();
void XA_Remote_Free();

void XA_Remote_Pause();
void XA_Remote_PlayNext();
void XA_Remote_PlayPrev();
void XA_Remote_PlayStop();
void XA_Remote_StepPrev();
void XA_Remote_StepNext();
void XA_Remote_AudioOff();
void XA_Remote_AudioOn();
void XA_Remote_SpeedNorm();
void XA_Remote_SpeedDiff();
void XA_Remote_Adj_Volume();
#endif

void XA_Install_CMAP();
void IFF_Buffer_HAM6();
void IFF_Buffer_HAM8();
void UTIL_Mapped_To_Bitmap();
void UTIL_Mapped_To_Mapped();
xaULONG CMAP_Find_Closest();
void XA_Store_Title();



/*********************************** X11 stuff */
Display       *theDisp;
int	       theScreen;
Visual        *theVisual;
Colormap       theCmap = 0;
Window         mainW;

#ifdef XMBUF
Window		mainWreal;       /* actual main window */
Window		mainWbuffers[2]; /* two buffers for the main window */
int		mainWbufIndex;   /* which buffer is actually visible */
#endif

GC             theGC = 0;
XImage        *theImage = 0;
XColor         defs[256];
Widget	       theWG;

GC             remoteGC = 0;
Window         remoteW;
/******************************** Xt stuff */
extern XA_CHDR *xa_chdr_now;
extern XA_ANIM_HDR *cur_file;
extern xaULONG xa_title_flag;
extern xaULONG xa_anim_flags;
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
static void xact_playnext(), xact_playprev(), xact_playstop();
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
static void xanim_set_volume();
static void xanim_set_pingpong();

/*
 * gf: Replace KeyUp() and ButtonPress() with more specific actions
 */

#define ACTIONTABLE_SIZE 29
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
	{"PlayNext",xact_playnext},
	{"PlayPrev",xact_playprev},
	{"PingPong",xanim_set_pingpong},
	{"PlayStop",xact_playstop}
};

typedef struct {
  int anim;
  int remote_xpos;
  int remote_ypos;
  int window_xpos;
  int window_ypos;
  int audio_volume;
} xanim_resources;

xanim_resources xa_resources;

#define offset(field)   XtOffsetOf(xanim_resources, field)

XtResource application_resources[] = {
  {"anim", "Anim", XtRBoolean, sizeof(Boolean),
     offset(anim), XtRString, "False" },
  {"remote_xpos", "XAnim_Var", XtRInt, sizeof(int),
     offset(remote_xpos), XtRImmediate, (XtPointer)XA_REMW_XPOS },
  {"remote_ypos", "XAnim_Var", XtRInt, sizeof(int),
     offset(remote_ypos), XtRImmediate, (XtPointer)XA_REMW_YPOS },
  {"window_xpos", "XAnim_Var", XtRInt, sizeof(int),
     offset(window_xpos), XtRImmediate, (XtPointer)XA_MAINW_XPOS },
  {"window_ypos", "XAnim_Var", XtRInt, sizeof(int),
     offset(window_ypos), XtRImmediate, (XtPointer)XA_MAINW_YPOS },
  {"audio_volume", "XAnim_Var", XtRInt, sizeof(int),
     offset(audio_volume), XtRImmediate, (XtPointer)10 }
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
   <Key>p:		PingPong()\n\
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
    fprintf(stderr,"X11_Get_Shift: wierd mask value %x\n",mask);
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

	/* Thanks to Moheb Mekhaiel for this solution. It's more elegant that
	 * what I was thinking of.
	 */
  if (x11_byte_mismatch)
  { xaULONG temp = temp_color;
    if (x11_depth <= 16)	/* swap 2 bytes */
	temp_color	= ((temp & 0x000000ff) << 8)
			| ((temp & 0x0000ff00) >>  8)
			;
    else	/* swap 4 bytes */
	temp_color	= ((temp & 0x000000ff) << 24)
			| ((temp & 0x0000ff00) <<  8)
			| ((temp & 0x00ff0000) >>  8)
			| ((temp & 0xff000000) >>  24)
			;
  }

/*
temp = temp_color;
temp_color  = ((temp >> 24) & 0xff) ;
temp_color |= ((temp >> 16) & 0xff) <<  8;
temp_color |= ((temp >>  8) & 0xff) << 16;
temp_color |= ((temp      ) & 0xff) << 24;
*/
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
{ int i,vis_num,vis_i;
  XVisualInfo *vis;

 
  shm = 0;
#ifdef XSHM
  if ( (XShmQueryExtension(theDisp)) && (x11_shared_flag == xaTRUE)) shm = 1;
#endif

/* SMR 9 */
/* Use visual from window id user specified. */
  if (xa_window_id != 0)
  {
    XWindowAttributes wAttributes;
    XVisualInfo vis_template;
/* catch events while preparing movie */
    XSelectInput(theDisp, (Window)xa_window_id,
                 StructureNotifyMask | PropertyChangeMask);
    XGetWindowAttributes(theDisp, (Window)xa_window_id, &wAttributes);
    vis_template.visualid = XVisualIDFromVisual(wAttributes.visual);
    vis_template.screen = DefaultScreen(theDisp);
    vis = XGetVisualInfo (theDisp, VisualIDMask | VisualScreenMask,
                          &vis_template, &vis_num);
    xa_user_visual = 0;
  } /* end SMR 9 */
  else if (xa_user_class >= 0) /* only attempt to use select visuals */
  { XVisualInfo vis_template;
    vis_template.class  = xa_user_class;
    vis_template.screen = DefaultScreen(theDisp);
    vis = XGetVisualInfo (theDisp, (VisualClassMask | VisualScreenMask), 
						&vis_template, &vis_num);
    if ((vis == NULL) || (vis_num == 0) )
	TheEnd1("X11: Couldn't get any Visuals of the desired class.");
  }
  else  /* look at them all (for the screen in question) */
  { XVisualInfo vis_template;
    vis_template.screen = DefaultScreen(theDisp);
    vis = XGetVisualInfo (theDisp, VisualScreenMask, &vis_template, &vis_num);
    if ((vis == NULL) || (vis_num == 0) )
		TheEnd1("X11: Couldn't get any Visuals.");
  }

  vis_i = xa_user_visual;

  if (vis_i >= vis_num)
  {  fprintf(stdout,
        "X11: Couldn't get requested Visual. Using default instead.");
    vis_i = -1;
  }

        /* Use Default if None are selected */
#ifdef X11_USE_DEFAULT_VISUAL
  if ((vis_i < 0) || (vis_i >= vis_num))
  { int i; VisualID vis_d =
        XVisualIDFromVisual(DefaultVisual(theDisp, DefaultScreen(theDisp)));
    i = 0;
    while(i < vis_num)
    {  if (vis_d == vis[i].visualid) { vis_i = i; break; }
       i++;
    }
  }
#endif

  if (vis_i < 0)
  { int screen;
    xaLONG max_csize,max_depth,max_class;

    max_class = -1;
    max_depth = 0;
    max_csize = 0;
    screen = DefaultScreen(theDisp);

    /* look through visuals a choose a good one */
    for(i=0;i<vis_num;i++)
    { if (vis[i].screen != screen) continue; /* Visual for a diff screen */
      if (vis[i].depth > max_depth)
      { max_depth = vis[i].depth;
	max_class = vis[i].class;
	max_csize = vis[i].colormap_size;
	vis_i = i;
      }
      else if (vis[i].depth == max_depth)
      { if (vis[i].class > max_class) 
	{ if (   (vis[i].class < 4)	/* pseudo or less */
	      || (vis[i].depth > 8) )
	  { max_class = vis[i].class;
	    max_csize = vis[i].colormap_size;
	    vis_i = i;
	  }
	}
	else if (vis[i].class == max_class)
	{ if (vis[i].colormap_size > max_csize)
	  { max_csize = vis[i].colormap_size;
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
   /* for now only use 256 colors even if more avail */
  if (x11_cmap_size > 256) x11_cmap_size = 256;

  /* POD - For testing purposes only */
  if ( (pod_max_colors > 0) && (pod_max_colors < x11_cmap_size) )
		x11_cmap_size = pod_max_colors;
  theGC  = DefaultGC(theDisp,theScreen);

  /* Make sure x11_cmap_size is power of two */
  { xaULONG size;
    size = 0x01; x11_disp_bits = 0;
    while(size <= x11_cmap_size) { size <<= 1; x11_disp_bits++; }
    size >>=1; x11_disp_bits--;
    x11_cmap_size = 0x01 << x11_disp_bits;
  }

	/* Find Bit Order and use local defines instead */
  x11_bit_order   = BitmapBitOrder(theDisp);
  if (x11_bit_order == MSBFirst)	x11_bit_order = XA_MSBIT_1ST;
  else					x11_bit_order = XA_LSBIT_1ST;
  x11_bitmap_unit = BitmapUnit(theDisp);
  x11_depth_mask = (0x01 << x11_depth) - 1;
  x11_cmap_type = 0;

	/* Determine Local Machines Byte Order Used later in Get_TrueColor  */
  { union { char ba[2]; short sa; } onion;
    onion.ba[0] = 1;
    onion.ba[1] = 0;
    if (onion.sa == 1)	xam_byte_order = XA_LSBYTE_1ST;
    else		xam_byte_order = XA_MSBYTE_1ST;
  }

  XFree( (void *)vis);  

  if (x11_depth == 1)
  { x11_display_type = XA_MONOCHROME;
    x11_bytes_pixel = 1; x11_bitmap_pad = x11_bitmap_unit; 
    x11_cmap_flag = xaFALSE;
    x11_black = BlackPixel(theDisp,DefaultScreen(theDisp));
    x11_white = WhitePixel(theDisp,DefaultScreen(theDisp));
    x11_bits_per_pixel = 1;
    x11_byte_order = x11_bit_order;
  }
  else 
  { if (x11_depth > 16)		x11_bitmap_pad = 32;
    else if (x11_depth > 8)	x11_bitmap_pad = 16;
    else			x11_bitmap_pad = 8;

    theImage = XCreateImage(theDisp,theVisual,
			x11_depth,ZPixmap,0,0,7,7,
			x11_bitmap_pad,0);
    if (theImage != 0)
    { x11_bits_per_pixel = theImage->bits_per_pixel;
      x11_byte_order = theImage->byte_order;

	/* Find out Byte Order of Display */
      if (x11_byte_order == MSBFirst) x11_byte_order = XA_MSBYTE_1ST;
      else x11_byte_order = XA_LSBYTE_1ST;

	/* Determine if we have a byte order problem */
      if ((x11_depth > 8) && (xam_byte_order != x11_byte_order))
      {
        if (x11_verbose_flag == xaTRUE) 
		fprintf(stderr,"Byte Order Mismatch %d %d\n",
				xam_byte_order,x11_byte_order);
	x11_byte_mismatch = xaTRUE;
      }
      else	x11_byte_mismatch = xaFALSE;

      if (x11_verbose_flag == xaTRUE)
      { fprintf(stderr,"bpp=%d   bpl=%d   byteo=%d  bito=%d\n",
		x11_bits_per_pixel,theImage->bytes_per_line,
		x11_byte_order,theImage->bitmap_bit_order);
      }
      XDestroyImage(theImage); theImage = NULL;
    } else x11_bits_per_pixel = x11_depth;


    switch(x11_bits_per_pixel)
    {
      case 32:
	x11_bytes_pixel = 4;
	break;
      case 16:
	x11_bytes_pixel = 2;
	break;
      case  8:
	x11_bytes_pixel = 1;
	break;
      case 24:
	fprintf(stderr,
		"X11: Packed 24 bpp is non-standard and not yet supported.\n");
	fprintf(stderr,
		"     See the bug section in Rev_History for details.\n");
	fprintf(stderr,
		"     Until then an 8, 16 or 32 bpp Visual should work.\n");
	TheEnd();
	break;
      default:
	fprintf(stderr,"X11: %d bpp non-standard and not yet supported\n",x11_bits_per_pixel);
	TheEnd();
	break;
    }

    switch(x11_class)
    { case StaticGray:
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
	fprintf(stderr,"Unkown x11_class %x\n",x11_class);
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
    fprintf(stderr," (%x) \n",x11_display_type);
    fprintf(stderr,"  depth= %d  class= %d  cmap size=%d(%d) bytes_pixel=%d\n",
        x11_depth, x11_class, x11_cmap_size, x11_disp_bits, x11_bytes_pixel );
    if (x11_display_type & XA_X11_TRUE)
    {
      fprintf(stderr,"  X11 Color Masks =%x %x %x\n",
                  x11_red_mask,x11_green_mask ,x11_blue_mask);
      fprintf(stderr,"  X11 Color Shifts=%d %d %d\n",
                  x11_red_shift, x11_green_shift, x11_blue_shift );
      fprintf(stderr,"  X11 Color Sizes =%d %d %d\n",
                  x11_red_bits,x11_green_bits ,x11_blue_bits);
    }
    else if (x11_display_type == XA_MONOCHROME)
    { fprintf(stderr,"  Bit Order = %x  bitmapunit = %x bitmappad = %x\n",
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
{ xaLONG n;
  Arg arglist[20];
  XWMHints xwm_hints;

/* SMR 10 */
/* Use cmap from window id user specified. */
  if (xa_window_id != 0)
  {
     XWindowAttributes wAttributes;

     XGetWindowAttributes(theDisp, (Window)xa_window_id, &wAttributes);
     theCmap = wAttributes.colormap;
  }
/* end SMR 10 */
  else if (   (   (theVisual == DefaultVisual(theDisp,theScreen))
          && !(   (cmap_play_nice == xaFALSE) 
               && (x11_display_type & XA_X11_CMAP)
              )
         ) 
      || (x11_display_type == XA_MONOCHROME)
     )
  { DEBUG_LEVEL1 fprintf(stderr,"using default cmap\n");
    theCmap = DefaultColormap(theDisp,theScreen);
    x11_cmap_flag = xaFALSE; /* if nice */
  }
  else
  { DEBUG_LEVEL1 fprintf(stderr,"creating new cmap\n");
    if (x11_display_type & XA_X11_STATIC)
    { theCmap = XCreateColormap(theDisp,RootWindow(theDisp,theScreen),
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
/* TODO: does byte mismatch affect this!??? */
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
    else   /* re-instate here */
    {  xaULONG i,csize;
       theCmap = XCreateColormap(theDisp,RootWindow(theDisp,theScreen),
							theVisual, AllocAll);
       if (theCmap == 0) TheEnd1("X11_Setup_Window: create cmap err");

       csize = CellsOfScreen(DefaultScreenOfDisplay(theDisp));
       if (csize > x11_cmap_size) csize = x11_cmap_size;
       for(i=0;i<csize;i++)
       { if (x11_display_type & XA_X11_TRUE)
         { xaULONG d;
/* TODO: does byte mismatch affect this!??? */
           d  =  (i << x11_red_shift)   & x11_red_mask;
           d |=  (i << x11_green_shift) & x11_green_mask;
           d |=  (i << x11_blue_shift)  & x11_blue_mask;
           defs[i].pixel =  d;
         } else defs[i].pixel = i;
         defs[i].flags = DoRed | DoGreen | DoBlue; 
       }
       XQueryColors(theDisp,DefaultColormap(theDisp,theScreen),defs,csize);
       for(i=0;i<csize;i++)
       { xaULONG r,g,b;
         r = xa_cmap[i].red   = defs[i].red;
         g = xa_cmap[i].green = defs[i].green;
         b = xa_cmap[i].blue  = defs[i].blue;
         xa_cmap[i].gray = (xaUSHORT)( ( (r * 11) + (g * 16) + (b * 5) ) >> 5 );
         if (x11_display_type & XA_X11_GRAY)
         { defs[i].red   = xa_cmap[i].gray;
           defs[i].green = xa_cmap[i].gray;
           defs[i].blue  = xa_cmap[i].gray;
         }
         defs[i].pixel = i; /* probably redundant */
         defs[i].flags = DoRed | DoGreen | DoBlue;
      }
      XStoreColors(theDisp,theCmap,defs,csize);
    }
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

/* SMR 11 */
/* Fake mainW to be user specified window instead of widget window.
   Note that shell window is never realized. */
  if (xa_window_id != 0)
  {
	/* Check to see if it exists ??? */
    mainW = (Window)xa_window_id;
  }
  else
  {
    XtGetApplicationResources (theWG, &xa_resources, application_resources,
                               XtNumber(application_resources), NULL, 0);

    XtRealizeWidget(theWG);
    mainW = XtWindow(theWG);
  }
/* end SMR 11 */

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

    XSetFunction(theDisp, theGC, GXcopy);
    XSetPlaneMask(theDisp, theGC, ~0L);
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

  if (x11_cmap_flag == xaFALSE) /* static or monochrome displays */
  {
    xaULONG i;
    for(i=0;i<x11_cmap_size;i++)
    { 
      if (x11_display_type & XA_X11_TRUE)
      {
        xaULONG d;
/* TODO: does byte mismatch affect this!??? */
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
/* Copy Default Colormap into new one */
  else  if (!(x11_display_type & XA_X11_TRUE))
  {
    xaULONG i,csize;

    csize = CellsOfScreen(DefaultScreenOfDisplay(theDisp));
    if (csize > x11_cmap_size) csize = x11_cmap_size;
    for(i=0; i<csize; i++)
    {   /* POD eventually removed this when it's determined to be fixed */
      if (x11_display_type & XA_X11_TRUE)
      { xaULONG d;
/* TODO: does byte mismatch affect this!??? */
        d  =  (i << x11_red_shift)   & x11_red_mask;
        d |=  (i << x11_green_shift) & x11_green_mask;
        d |=  (i << x11_blue_shift)  & x11_blue_mask;
        defs[i].pixel =  d;
      }
      else defs[i].pixel = i;
      defs[i].flags = DoRed | DoGreen | DoBlue;
    }
    XQueryColors(theDisp,theCmap,defs,csize);
    for(i=0; i<csize; i++)
    { xaULONG r,g,b;
      r = xa_cmap[i].red   = defs[i].red;
      g = xa_cmap[i].green = defs[i].green;
      b = xa_cmap[i].blue  = defs[i].blue;
      xa_cmap[i].gray = (xaUSHORT)( ( (r * 11) + (g * 16) + (b * 5) ) >> 5 );
      if (x11_display_type & XA_X11_GRAY)
      { defs[i].red   = xa_cmap[i].gray;
        defs[i].green = xa_cmap[i].gray;
        defs[i].blue  = xa_cmap[i].gray;
      }
      defs[i].pixel = i; /* probably redundant */
      defs[i].flags = DoRed | DoGreen | DoBlue;
    }
    XStoreColors(theDisp,theCmap,defs,csize);
  }
  XSetWindowColormap(theDisp, mainW, theCmap);

  XInstallColormap(theDisp,theCmap);
  XSync(theDisp,False);
/*
#ifndef NO_INSTALL_CMAP
  XInstallColormap(theDisp,theCmap);
#endif
  XSync(theDisp,False);
*/

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
	fprintf(stderr,"X11 RESIZE %dx%d\n",x11_window_x,x11_window_y);
  }
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
    XA_Realize_Remote(remote_widget);
    xa_remote_busy = xaFALSE;
  }
  else
  {
    xa_remote_busy = xaTRUE;
    XA_Unrealize_Remote(remote_widget);
    xa_remote_busy = xaFALSE;
  }
#endif
}



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
  XtAppAddTimeOut(theContext, 1, (XtTimerCallbackProc)XAnim_Looped,
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
{ if (xa_anim_status & XA_STOP_MASK) /* if stopped */
  { xa_anim_status = XA_STEP_PREV;
    if (xa_anim_holdoff == xaTRUE) return;
    xa_anim_holdoff = xaTRUE;
    XtAppAddTimeOut(theContext, 1, (XtTimerCallbackProc)XAnim_Looped, 
						(XtPointer)XA_SHOW_NORM);
  }
  else xa_anim_status = XA_STOP_PREV;
#ifdef XA_REMOTE_CONTROL
  XA_Remote_StepPrev();
#endif
}

ACTION_PROC(xanim_step_next_action)
{ if (xa_anim_status & XA_STOP_MASK) /* if stopped */
  { xa_anim_status = XA_STEP_NEXT;
    if (xa_anim_holdoff == xaTRUE) return;
    xa_anim_holdoff = xaTRUE;
    XtAppAddTimeOut(theContext, 1, (XtTimerCallbackProc)XAnim_Looped, 
						(XtPointer)XA_SHOW_NORM);
  }
  else xa_anim_status = XA_STOP_NEXT;
#ifdef XA_REMOTE_CONTROL
  XA_Remote_StepNext();
#endif
}

ACTION_PROC(xanim_toggle_action)
{ int button; /* <0 PlayNext> <1 PlayPrev> <2 PlayStop> */
  switch(xa_anim_status)
  {
    case XA_RUN_PREV:  /* if running, then stop */
    case XA_RUN_NEXT:
      button = 2;
      xa_anim_status &= XA_CLEAR_MASK;
      xa_anim_status |= XA_STOP_MASK;
      xa_anim_flags &= ~(ANIM_CYCLE);
      break;
    case XA_STOP_PREV: /* if stopped, then run */
    case XA_STOP_NEXT:
      button = (xa_anim_status & XA_NEXT_MASK)?(0):(1);
      xa_anim_status &= XA_CLEAR_MASK;
      xa_anim_status |= XA_RUN_MASK;
      if (xa_anim_holdoff == xaTRUE) return;
      xa_anim_holdoff = xaTRUE;
      XtAppAddTimeOut(theContext, 1, (XtTimerCallbackProc)XAnim_Looped, 
						(XtPointer)XA_SHOW_NORM);
      break;
    case XA_STEP_PREV: /* if single stepping then run */
    case XA_STEP_NEXT:
    case XA_ISTP_PREV:
    case XA_ISTP_NEXT:
    case XA_FILE_PREV:
    case XA_FILE_NEXT:
      button = (xa_anim_status & XA_NEXT_MASK)?(0):(1);
      xa_anim_status &= XA_CLEAR_MASK;
      xa_anim_status |= XA_RUN_MASK;
      break;
    default:
      button = 2;
      if (xa_anim_status & XA_NEXT_MASK) xa_anim_status = XA_STOP_NEXT;
      else				 xa_anim_status = XA_STOP_PREV;
      break;
  }
  /* change Play Widget pixmap */
#ifdef XA_REMOTE_CONTROL
  if (button==0)	XA_Remote_PlayNext();
  else if (button==1)	XA_Remote_PlayPrev();
  else			XA_Remote_PlayStop();
#endif
  if ( (xa_title_flag == XA_TITLE_FILE) && (xa_anim_status & XA_RUN_MASK))
				XA_Store_Title(cur_file,0,xa_title_flag);
}

ACTION_PROC(xact_playnext)
{ if ((xa_anim_status == XA_STOP_PREV) || (xa_anim_status == XA_STOP_NEXT))
  { if (xa_anim_holdoff == xaTRUE) return;
    xa_anim_holdoff = xaTRUE;
    XtAppAddTimeOut(theContext, 1, (XtTimerCallbackProc)XAnim_Looped,
						    (XtPointer)XA_SHOW_NORM);
  }
  xa_anim_status = XA_RUN_NEXT;
#ifdef XA_REMOTE_CONTROL
  XA_Remote_PlayNext();
#endif
  if (xa_title_flag == XA_TITLE_FILE) XA_Store_Title(cur_file,0,xa_title_flag);
}

ACTION_PROC(xact_playprev)
{ if ((xa_anim_status == XA_STOP_PREV) || (xa_anim_status == XA_STOP_NEXT))
  { if (xa_anim_holdoff == xaTRUE) return;
    xa_anim_holdoff = xaTRUE;
    XtAppAddTimeOut(theContext, 1, (XtTimerCallbackProc)XAnim_Looped,
						    (XtPointer)XA_SHOW_NORM);
  }
  xa_anim_status = XA_RUN_PREV;
#ifdef XA_REMOTE_CONTROL
  XA_Remote_PlayPrev();
#endif
  if (xa_title_flag == XA_TITLE_FILE) XA_Store_Title(cur_file,0,xa_title_flag);
}

ACTION_PROC(xact_playstop)
{ xa_anim_status = (xa_anim_status & XA_NEXT_MASK)?(XA_STOP_NEXT)
						  :(XA_STOP_PREV); 
  xa_anim_flags &= ~(ANIM_CYCLE);
#ifdef XA_REMOTE_CONTROL
  XA_Remote_PlayStop();
#endif
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
  if (xa_speed_scale==XA_SPEED_NORM) XA_Remote_SpeedNorm();
#endif
  xa_speed_change = 1;
  if (xa_speed_scale > XA_SPEED_MIN) xa_speed_scale >>= 1;
  xa_speed_change = 2;
#ifdef XA_REMOTE_CONTROL
  if (xa_speed_scale==XA_SPEED_NORM) XA_Remote_SpeedDiff();
#endif
}

/* increase speed scale, but not more than 1000 */
ACTION_PROC(xanim_faster_action)
{ 
#ifdef XA_REMOTE_CONTROL
  if (xa_speed_scale==XA_SPEED_NORM) XA_Remote_SpeedNorm();
#endif
  xa_speed_change = 1;
  if (xa_speed_scale < XA_SPEED_MAX) xa_speed_scale <<= 1;
  xa_speed_change = 2;
#ifdef XA_REMOTE_CONTROL
  if (xa_speed_scale==XA_SPEED_NORM) XA_Remote_SpeedDiff();
#endif
}

/* increase speed scale, but not more than 1000 */
ACTION_PROC(xanim_set_pingpong)
{ 
  xa_pingpong_flag = (xa_pingpong_flag == xaTRUE)?(xaFALSE):(xaTRUE);
}


/*******************************
 * set speed to that at startup
 *******************************/
ACTION_PROC(xanim_speed_reset_action)
{
#ifdef XA_REMOTE_CONTROL
  if (xa_speed_scale != XA_SPEED_NORM) XA_Remote_SpeedDiff();
#endif
  xa_speed_change = 1;
  xa_speed_scale = XA_SPEED_NORM;
  xa_speed_change = 2;
}

/*******************************
 * single step across anims
 *******************************/
ACTION_PROC(xanim_next_anim_action)
{ if (xa_anim_status & XA_STOP_MASK) /* if stopped */
  { xa_anim_status = XA_FILE_NEXT;
    if (xa_anim_holdoff == xaTRUE) return;
    xa_anim_holdoff = xaTRUE;
    XtAppAddTimeOut(theContext, 1, (XtTimerCallbackProc)XAnim_Looped, 
						(XtPointer)XA_SHOW_NORM);
  }
  else xa_anim_status = XA_FILE_NEXT;
#ifdef XA_REMOTE_CONTROL
  XA_Remote_StepNext();
#endif
}

/*******************************
 * single step across anims
 *******************************/
ACTION_PROC(xanim_prev_anim_action)
{ if (xa_anim_status & XA_STOP_MASK) /* if stopped */
  { xa_anim_status = XA_FILE_PREV;
    if (xa_anim_holdoff == xaTRUE) return;
    xa_anim_holdoff = xaTRUE;
    XtAppAddTimeOut(theContext, 1, (XtTimerCallbackProc)XAnim_Looped, 
						(XtPointer)XA_SHOW_NORM);
  }
  else xa_anim_status = XA_FILE_PREV;
#ifdef XA_REMOTE_CONTROL
  XA_Remote_StepPrev();
#endif
}

/*******************************
 * single step within anim 
 *******************************/
ACTION_PROC(xanim_step_next_int_action)
{ if (xa_anim_status & XA_STOP_MASK) /* if stopped */
  { xa_anim_status = XA_ISTP_NEXT;
    if (xa_anim_holdoff == xaTRUE) return;
    xa_anim_holdoff = xaTRUE;
    XtAppAddTimeOut(theContext, 1, (XtTimerCallbackProc)XAnim_Looped, 
						(XtPointer)XA_SHOW_NORM);
  }
  else xa_anim_status = XA_STOP_NEXT;
#ifdef XA_REMOTE_CONTROL
  XA_Remote_StepNext();
#endif
}

/*******************************
 * single step within anim 
 *******************************/
ACTION_PROC(xanim_step_prev_int_action)
{ if (xa_anim_status & XA_STOP_MASK) /* if stopped */
  { xa_anim_status = XA_ISTP_PREV;
    if (xa_anim_holdoff == xaTRUE) return;
    xa_anim_holdoff = xaTRUE;
    XtAppAddTimeOut(theContext, 1, (XtTimerCallbackProc)XAnim_Looped, 
						(XtPointer)XA_SHOW_NORM);
  }
  else xa_anim_status = XA_STOP_PREV;
#ifdef XA_REMOTE_CONTROL
  XA_Remote_StepPrev();
#endif
}

/*******************************
 * decrement xa_audio_volume by 5
 *******************************/
ACTION_PROC(xanim_dec_audio_5)
{ vaudiof->volume -= 5; if (vaudiof->volume < 0) vaudiof->volume = 0;
  vaudiof->newvol = xaTRUE;
  XA_AUDIO_SET_VOLUME(vaudiof->volume);
#ifdef XA_REMOTE_CONTROL
  XA_Remote_Adj_Volume(vaudiof->volume,XA_AUDIO_MAXVOL);
#endif
}

/*******************************
 * decrement xa_audio_volume by 1 
 *******************************/
ACTION_PROC(xanim_dec_audio_1)
{ vaudiof->volume -= 1; if (vaudiof->volume < 0) vaudiof->volume = 0;
  vaudiof->newvol = xaTRUE;
  XA_AUDIO_SET_VOLUME(vaudiof->volume);
#ifdef XA_REMOTE_CONTROL
  XA_Remote_Adj_Volume(vaudiof->volume,XA_AUDIO_MAXVOL);
#endif
}

/*******************************
 * increment xa_audio_volume by 5 
 *******************************/
ACTION_PROC(xanim_inc_audio_5)
{ vaudiof->volume += 5;
  if (vaudiof->volume > XA_AUDIO_MAXVOL) vaudiof->volume = XA_AUDIO_MAXVOL;
  vaudiof->newvol = xaTRUE;
  XA_AUDIO_SET_VOLUME(vaudiof->volume);
#ifdef XA_REMOTE_CONTROL
  XA_Remote_Adj_Volume(vaudiof->volume,XA_AUDIO_MAXVOL);
#endif
}

/*******************************
 * increment xa_audio_volume by 1
 *******************************/
ACTION_PROC(xanim_inc_audio_1)
{ vaudiof->volume += 1;
  if (vaudiof->volume > XA_AUDIO_MAXVOL) vaudiof->volume = XA_AUDIO_MAXVOL;
  vaudiof->newvol = xaTRUE;
  XA_AUDIO_SET_VOLUME(vaudiof->volume);
#ifdef XA_REMOTE_CONTROL
  XA_Remote_Adj_Volume(vaudiof->volume,XA_AUDIO_MAXVOL);
#endif
}

/*******************************
 * mute audio 
 *******************************/
ACTION_PROC(xanim_mute_audio)
{ vaudiof->mute = (vaudiof->mute==xaTRUE)?(xaFALSE):(xaTRUE);
  vaudiof->newvol = xaTRUE;
  XA_AUDIO_SET_MUTE(vaudiof->mute);
#ifdef XA_REMOTE_CONTROL
  if (vaudiof->mute==xaTRUE)	XA_Remote_AudioOn();
  else				XA_Remote_AudioOff();
#endif
}

/*******************************
 * toggle speaker 
 *******************************/
ACTION_PROC(xanim_speaker_tog)
{
  XA_SPEAKER_TOG(2);
}

/*******************************
 * toggle headphone 
 *******************************/
ACTION_PROC(xanim_headphone_tog)
{
  XA_HEADPHONE_TOG(2);
}


/*******************************
 * Would you believe the main loop?
 *******************************/
void xanim_events()
{
/* SMR 12 */
/* If external window has been supplied, watch the properties on that
   window to determine what action to take. */
  if (xa_window_id == 0)	XtAppMainLoop(theContext);
  else
  {
    XEvent event;
    char *command = "";
    XDestroyWindowEvent *devent = (XDestroyWindowEvent *)&event;
    XConfigureEvent *cevent = (XConfigureEvent *)&event;
    XPropertyEvent *pevent = (XPropertyEvent *)&event;
    xaULONG window_atom = XInternAtom(theDisp, xa_window_propname, False);
    XChangeProperty(theDisp, mainW, window_atom, XA_STRING, 8,
		    PropModeReplace, (unsigned char *)command, 1);
    if (xa_window_prepare_flag == xaFALSE)
      xanim_expose(theWG, &event, NULL, 0);
    while (1)
    {
      XtAppNextEvent(theContext, &event);

/* POD NOTE:  eventually make these into Event Handlers that are
 * conditionally fired base on "xa_window_id" or some other option.
 */
      if (event.type == DestroyNotify && devent->window == mainW)
      {
	TheEnd();
      }
      else if (event.type == ConfigureNotify && cevent->window == mainW)
      {
	x11_window_x = cevent->width;
	x11_window_y = cevent->height;
      }
      else if (event.type == PropertyNotify && pevent->window == mainW)
      {
	if (pevent->atom == window_atom)
	{
	  Atom type;
	  int format;
	  long unsigned int nitems;
	  unsigned long extra;
	  char *command;
	  if (pevent->state == PropertyDelete) TheEnd();
	  XGetWindowProperty(theDisp, pevent->window, pevent->atom, 0,
			     4, False, AnyPropertyType,
			     &type, &format, &nitems, &extra,
			     (unsigned char **)&command);

	  switch (*command)
	  {
	  case 'q':
	    XDeleteProperty(theDisp, pevent->window, pevent->atom);
	    TheEnd();
	    break;
	  case ' ':
	    if (xa_anim_status == XA_UNSTARTED) 
	      xanim_expose(theWG, &event, NULL, 0);
	    else xanim_toggle_action(theWG, &event, NULL, 0);
	    break;
	  case ',':
	    xanim_step_prev_action(theWG, &event, NULL, 0);
	    break;
	  case '.':
	    xanim_step_next_action(theWG, &event, NULL, 0);
	    break;
	  case 'm':
	    xanim_step_prev_int_action(theWG, &event, NULL, 0);
	    break;
	  case '/':
	    xanim_step_next_int_action(theWG, &event, NULL, 0);
	    break;
	  case '-':
	    xanim_faster_action(theWG, &event, NULL, 0);
	    break;
	  case '=':
	    xanim_slower_action(theWG, &event, NULL, 0);
	    break;
	  case '0':
	    xanim_speed_reset_action(theWG, &event, NULL, 0);
	    break;
	  case '1':
	    xanim_dec_audio_5(theWG, &event, NULL, 0);
	    break;
	  case '2':
	    xanim_dec_audio_1(theWG, &event, NULL, 0);
	    break;
	  case '3':
	    xanim_inc_audio_1(theWG, &event, NULL, 0);
	    break;
	  case '4':
	    xanim_inc_audio_5(theWG, &event, NULL, 0);
	    break;
	  case 's':
	    xanim_mute_audio(theWG, &event, NULL, 0);
	    break;
	  case '8':
	    xanim_speaker_tog(theWG, &event, NULL, 0);
	    break;
	  case '9':
	    xanim_headphone_tog(theWG, &event, NULL, 0);
	    break;
	  case 'v':
	    vaudiof->volume = atoi( &command[1] );
	    if (vaudiof->volume < 0)
	      vaudiof->volume = 0;
	    if (vaudiof->volume > XA_AUDIO_MAXVOL)
	      vaudiof->volume = XA_AUDIO_MAXVOL;
	    vaudiof->newvol = xaTRUE;
	    XA_AUDIO_SET_VOLUME(vaudiof->volume);
	    break;
	  case 'z':
	    xanim_realize_remote(theWG, &event, NULL, 0);
	    break;
	  case 'e':
	    x11_expose_flag = xaTRUE;
/* Need a function to redraw window here! */
	    break;
	  default:
	    break;
	  }
	  XFree(command);
	}
      }
      else XtDispatchEvent(&event);
    }
  }
/* end SMR 12 */
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
DEBUG_LEVEL2 fprintf(stderr," InitImage %x <%d,%d>\n", (xaULONG)(image),xsize,ysize );
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
{ int i,vis_num;
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
      fprintf(stderr,"  visual %d) depth= %d  class= %d  cmap size=%d  bitsrgb %d",
                       i, vis[i].depth, vis[i].class, vis[i].colormap_size,
			vis[i].bits_per_rgb  );
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
/* TODO: does byte mismatch affect this!??? */
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
    DEBUG_LEVEL2 fprintf(stderr,"   %d) <%x %x %x> %x\n",i,r,g,b,chdr->cmap[i].gray);
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
/* TODO: does byte mismatch affect this!??? */
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
  { new_map = (xaULONG *)malloc(x11_cmap_size * sizeof(xaULONG) );
    if (new_map == 0) TheEnd1("X11_Make_Nice_CHDR: map malloc err");
    FREE(old_map,0x401); old_cmap=0;
    chdr->msize = x11_cmap_size;
    chdr->map = new_map;
  }
  
  DEBUG_LEVEL2 fprintf(stderr,"X11_Make_Nice_CHDR: \n");
  chdr->moff = chdr->coff = 0;
  for(i=0;i<x11_cmap_size;i++)
  { xaULONG r,g,b;
    r = chdr->cmap[i].red   = defs[i].red; 
    g = chdr->cmap[i].green = defs[i].green; 
    b = chdr->cmap[i].blue  = defs[i].blue; 
    chdr->cmap[i].gray = (xaUSHORT)( ( (r * 11) + (g * 16) + (b * 5) ) >> 5 );
    chdr->map[i] = i;
    DEBUG_LEVEL2 fprintf(stderr," %d) <%x %x %x> %x\n",i,r,g,b,chdr->cmap[i].gray);
  }
}


int XA_Error_Handler(errDisp,event)
Display *errDisp;
XErrorEvent *event;
{ char errbuff[255];
  if (x11_error_possible != 1) 
	XGetErrorText(errDisp,event->error_code, errbuff, 255);
  x11_error_possible = -1;
  return(0);
}


#ifdef XA_PETUNIA

void xa_remote_expose();

static void InitButtons();
static void InitButtonsTypes();
static void InitScrolls();
static void GetButtonColors();
static void DrawRemote();
static void DrawButton();
static void DrawScroll();
static xaULONG InButtonQuery();
static void AdjustScroll();
static xaLONG WhichButton();



/* Make xa_buttons.h */

typedef struct
{
  int width;
  int height;
  unsigned char *hi_bitmap;
  unsigned char *lo_bitmap;
  Pixmap hi;
  Pixmap lo;
} BUTTON_TYPE;

/** Buttons ***/
#include "buttons/but_quad10_hi.xbm"
#include "buttons/but_quad10_lo.xbm"
#include "buttons/but_quad16_hi.xbm"
#include "buttons/but_quad16_lo.xbm"
#include "buttons/but_rtri10_hi.xbm"
#include "buttons/but_rtri10_lo.xbm"
#include "buttons/but_ltri10_hi.xbm"
#include "buttons/but_ltri10_lo.xbm"
#include "buttons/but_rstp16_hi.xbm"
#include "buttons/but_rstp16_lo.xbm"
#include "buttons/but_lstp16_hi.xbm"
#include "buttons/but_lstp16_lo.xbm"
#include "buttons/but_plus16_hi.xbm"
#include "buttons/but_plus16_lo.xbm"
#include "buttons/but_dash16_hi.xbm"
#include "buttons/but_dash16_lo.xbm"
#include "buttons/but_sond16_hi.xbm"
#include "buttons/but_sond16_lo.xbm"
#include "buttons/but_next16_hi.xbm"
#include "buttons/but_next16_lo.xbm"
#include "buttons/but_prev16_hi.xbm"
#include "buttons/but_prev16_lo.xbm"
#include "buttons/but_circ16_hi.xbm"
#include "buttons/but_circ16_lo.xbm"
#include "buttons/but_vol56_lo.xbm"
#include "buttons/but_vol56_hi.xbm"

/** Scrollbars */
#include "buttons/bar_6x4_lo.xbm"
#include "buttons/bar_6x4_hi.xbm"

/** Decals ***/
#include "buttons/txt_exit.xbm"
#include "buttons/txt_auup.xbm"
#include "buttons/txt_audn.xbm"

#define BUTTON_TYPE_QUAD16       0
#define BUTTON_TYPE_RTRI16       1
#define BUTTON_TYPE_LTRI16       2
#define BUTTON_TYPE_RSTP16       3
#define BUTTON_TYPE_LSTP16       4
#define BUTTON_TYPE_PLUS16       5
#define BUTTON_TYPE_DASH16       6
#define BUTTON_TYPE_SOND16       7
#define BUTTON_TYPE_QUAD10       8
#define BUTTON_TYPE_NEXT16       9
#define BUTTON_TYPE_PREV16      10
#define BUTTON_TYPE_CIRC16      11
#define BUTTON_TYPE_VOL56       12


#define NUM_BUTTON_TYPES 13
BUTTON_TYPE button_types[] =    {
        { but_quad16_hi_width, but_quad16_hi_height,
          but_quad16_hi_bits, but_quad16_lo_bits, 0, 0 },
        { but_rtri10_hi_width, but_rtri10_hi_height,
          but_rtri10_hi_bits, but_rtri10_lo_bits, 0, 0 },
        { but_ltri10_hi_width, but_ltri10_hi_height,
          but_ltri10_hi_bits, but_ltri10_lo_bits, 0, 0 },
        { but_rstp16_hi_width, but_rstp16_hi_height,
          but_rstp16_hi_bits, but_rstp16_lo_bits, 0, 0 },
        { but_lstp16_hi_width, but_lstp16_hi_height,
          but_lstp16_hi_bits, but_lstp16_lo_bits, 0, 0 },
        { but_plus16_hi_width, but_plus16_hi_height,
          but_plus16_hi_bits, but_plus16_lo_bits, 0, 0 },
        { but_dash16_hi_width, but_dash16_hi_height,
          but_dash16_hi_bits, but_dash16_lo_bits, 0, 0 },
        { but_sond16_hi_width, but_sond16_hi_height,
          but_sond16_hi_bits, but_sond16_lo_bits, 0, 0 },
        { but_quad10_hi_width, but_quad10_hi_height,
          but_quad10_hi_bits, but_quad10_lo_bits, 0, 0 },
        { but_next16_hi_width, but_next16_hi_height,
          but_next16_hi_bits, but_next16_lo_bits, 0, 0 },
        { but_prev16_hi_width, but_prev16_hi_height,
          but_prev16_hi_bits, but_prev16_lo_bits, 0, 0 },
        { but_circ16_hi_width, but_circ16_hi_height,
          but_circ16_hi_bits, but_circ16_lo_bits, 0, 0 },
        { but_vol56_hi_width, but_vol56_hi_height,
          but_vol56_hi_bits, but_vol56_lo_bits, 0, 0 },
                                };

#define BUTTON_STATE_OFF	0x0000
#define BUTTON_STATE_ON		0x0001

#define BUTTON_STATE_FIXED	0x0100
#define BUTTON_STATE_AON	0x0100

#define BUTTON_STATE_SEL  	0x0200
#define BUTTON_STATE_SELON	0x0200
#define BUTTON_STATE_SELOFF	0x0201

#define BUTTON_STATE_FSEL 	0x0400

/* redefine into Colors */
static int button_back,button_lo,button_main,button_hi;


typedef struct
{ xaLONG type;
  xaULONG xpos, ypos;
  xaULONG state;
  void (*func)();
  xaULONG width, height;
  unsigned char *bitmap;
  Pixmap  pmap;
  xaLONG scroll;
} BUTTON;

/* These are for toggle functions implemented later below */
#define BUTTON_PLAYPREV 1
#define BUTTON_PLAYSTOP 2
#define BUTTON_PLAYNEXT 3
#define BUTTON_AUDIO	12
#define BUTTON_SPEED	9
#define BUTTON_VOLBAR	14


/* POD Eventually come up with a more elegant way of defining this */
#define REMOTE_HEIGHT (4 + 16 + 4 + 16 + 4 + 16 + 4 + 20 + 4 )
#define REMOTE_WIDTH  (4 + 16 + 4 + 10 + 4 + 10 + 4 + 10 + 4 + 16 + 4 )

#define NUM_BUTTONS 15
BUTTON buttons[] =      {
        { BUTTON_TYPE_PREV16,  4,  4 ,BUTTON_STATE_OFF, xanim_step_prev_action,
                0, 0, 0, 0, -1 },
        { BUTTON_TYPE_LTRI16, 24,  4 ,BUTTON_STATE_OFF, xact_playprev,
                0, 0, 0, 0, -1 },
        { BUTTON_TYPE_QUAD10, 38,  4 ,BUTTON_STATE_OFF, xact_playstop,
                0, 0, 0, 0, -1 },
        { BUTTON_TYPE_RTRI16, 52,  4 ,BUTTON_STATE_ON, xact_playnext,
                0, 0, 0, 0, -1 },
        { BUTTON_TYPE_NEXT16, 66,  4 ,BUTTON_STATE_OFF, xanim_step_next_action,
                0, 0, 0, 0, -1 },

        { BUTTON_TYPE_LSTP16,  11, 24 ,BUTTON_STATE_OFF, xanim_prev_anim_action,
                0, 0, 0, 0, -1 },
        { BUTTON_TYPE_QUAD16,  31, 24 ,BUTTON_STATE_OFF, xanim_quit_action,
                txt_exit_width, txt_exit_height, txt_exit_bits, 0, -1 },
        { BUTTON_TYPE_RSTP16,  51, 24 ,BUTTON_STATE_OFF, xanim_next_anim_action,
                0, 0, 0, 0, -1 },

        { BUTTON_TYPE_DASH16, 11, 44 ,BUTTON_STATE_OFF,xanim_slower_action,
                0, 0, 0, 0, 0},
        { BUTTON_TYPE_CIRC16, 31, 44 ,BUTTON_STATE_ON, xanim_speed_reset_action,
                0, 0, 0, 0, -1 },
        { BUTTON_TYPE_PLUS16, 51, 44 ,BUTTON_STATE_OFF,xanim_faster_action,
                0, 0, 0, 0, -1 },

        { BUTTON_TYPE_SOND16, 11, 64 ,BUTTON_STATE_OFF, xanim_dec_audio_5,
                txt_audn_width, txt_audn_height, txt_audn_bits, 0, -1 },
        { BUTTON_TYPE_SOND16, 31, 64 ,BUTTON_STATE_ON, xanim_mute_audio,
                0, 0, 0, 0, -1 },
        { BUTTON_TYPE_SOND16, 51, 64 ,BUTTON_STATE_OFF, xanim_inc_audio_5,
                txt_auup_width, txt_auup_height, txt_auup_bits, 0, -1 },
        { BUTTON_TYPE_VOL56 , 72, 26 ,BUTTON_STATE_AON, xanim_set_volume,
                0, 0, 0, 0, 0 },
                        };


static void xanim_set_volume(w,event,but)
Widget w;
XEvent *event;
BUTTON *but;
{ XButtonEvent *xbut = (XButtonEvent *)event;
  xaLONG len, pos, new_vol;

  /* border of scroll bar is 2 pixels */
  len = button_types[ but->type ].height - 4;
  pos = len - (xbut->y - (but->ypos + 2));
  if (pos < 0) pos = 0;
  new_vol = (XA_AUDIO_MAXVOL * pos) / len;
  if (new_vol > XA_AUDIO_MAXVOL) new_vol = XA_AUDIO_MAXVOL;
  vaudiof->volume = new_vol;
  vaudiof->newvol = xaTRUE;
  XA_AUDIO_SET_VOLUME(vaudiof->volume);
#ifdef XA_REMOTE_CONTROL
  XA_Remote_Adj_Volume(vaudiof->volume,XA_AUDIO_MAXVOL);
#endif
}

typedef struct
{
  xaULONG type;		/* 0 vert 1 horiz */
  xaULONG cur;
  xaULONG xpos, ypos;
  xaULONG length;
  xaULONG width, height;
  unsigned char *lo_bitmap;
  unsigned char *hi_bitmap;
  Pixmap  lo;
  Pixmap  hi;
} SCROLLBAR;



#define SCROLLBAR_VOLUME 0

#define NUM_SCROLLBARS 1
static SCROLLBAR scrollbars[] = {
	{ 0, 38, 73, 28, 48, bar_6x4_hi_width, bar_6x4_hi_height,
	  bar_6x4_lo_bits, bar_6x4_hi_bits, 0, 0 },
		};

static void remote_btn1dn(), remote_btn1up();

#define REMOTE_ACTIONTABLE_SIZE 2
XtActionsRec remote_actionTable[] = {
        {"RemBtn1Dn", remote_btn1dn},
        {"RemBtn1Up", remote_btn1up},
};


String   Remote_Translation =
  "<Expose>:            Expose()\n\
   <Btn1Down>:          RemBtn1Dn()\n\
   <Btn1Up>:            RemBtn1Up()\n\
   <Key>1:              DecAudio5()\n\
   <Key>2:              DecAudio1()\n\
   <Key>3:              IncAudio1()\n\
   <Key>4:              IncAudio5()\n\
   <Key>8:              SpeakerTog()\n\
   <Key>9:              HDPhoneTog()\n\
   <Key>p:		PingPong()\n\
   <Key>s:              AudioMute()\n\
   <Key>q:              Quit()\n\
   <Key>w:              Resize()\n\
   <Key>F:              InstallCmap()\n\
   <Key>z:              RealizeRemote()\n\
   <Key>g:              StopCmap()\n\
   <Key>r:              RestoreCmap()\n\
   <Key>-:              Slower()\n\
   <Key>=:              Faster()\n\
   <Key>0:              SpeedReset()\n\
   <Key>.:              StepNext()\n\
   <Key>comma:          StepPrev()\n\
   <Key>greater:        NextAnim()\n\
   <Key>less:           PrevAnim()\n\
   <Key>/:              StepNextInt()\n\
   <Key>m:              StepPrevInt()\n\
   <Key>space:          RunStop()";


/* PETUNIA */
void XA_Create_Remote(wg,remote_flag)
Widget wg;
xaLONG remote_flag;
{ xaLONG n;
  Arg arglist[20];
  XWMHints xwm_hints;

  XtAppAddActions(theContext, remote_actionTable, REMOTE_ACTIONTABLE_SIZE);

  GetButtonColors(0);

  n = 0;
#ifdef XtNvisual
  XtSetArg(arglist[n], XtNvisual, theVisual); n++;
#endif
  XtSetArg(arglist[n], XtNcolormap, theCmap); n++;
  XtSetArg(arglist[n], XtNdepth, x11_depth); n++;
  XtSetArg(arglist[n], XtNforeground, button_main); n++;
  XtSetArg(arglist[n], XtNbackground, button_back); n++;
  XtSetArg(arglist[n], XtNborderColor, button_main); n++;
  XtSetArg(arglist[n], XtNwidth, REMOTE_WIDTH); n++;
  XtSetArg(arglist[n], XtNheight, REMOTE_HEIGHT); n++;
  XtSetArg(arglist[n], XtNx, XA_REMW_XPOS); n++;
  XtSetArg(arglist[n], XtNy, XA_REMW_YPOS); n++;
  XtSetArg(arglist[n], XtNmaxWidth, REMOTE_WIDTH); n++;
  XtSetArg(arglist[n], XtNmaxHeight, REMOTE_HEIGHT); n++;

  XtSetArg(arglist[n], XtNtranslations,
                        XtParseTranslationTable(Remote_Translation)); n++;

  remote_widget = XtCreatePopupShell("Control",topLevelShellWidgetClass,
							wg,arglist,n);

  XtRealizeWidget(remote_widget);
  remoteW = XtWindow(remote_widget);

  { xaULONG gc_mask = 0;
    XGCValues gc_init;
    gc_init.function = GXcopy;                          gc_mask |= GCFunction;
    gc_init.foreground = button_main; gc_mask |= GCForeground;
    gc_init.background = button_back; gc_mask |= GCBackground;
    gc_init.graphics_exposures = False;         gc_mask |= GCGraphicsExposures;
    remoteGC  = XCreateGC(theDisp,remoteW,gc_mask,&gc_init);
  }
  xwm_hints.input = True;
  xwm_hints.flags = InputHint;
  XSetWMHints(theDisp,remoteW,&xwm_hints);
  XSync(theDisp,False);

  InitButtonsTypes(button_types,NUM_BUTTON_TYPES);
  InitButtons(buttons,NUM_BUTTONS);
  InitScrolls(scrollbars,NUM_SCROLLBARS);

  /* THIS IS VERY IMPORTANT AND IS TO DELAY STARTUP UNTIL AN EXPOSE EVENT
   * HAPPENS so everything is realized and mapped before animation starts.
   * xa_remote_ready is set to xaTRUE in the xa_remote_expose routine.
   */
  XtAddRawEventHandler(remote_widget, ExposureMask, False, 
						xa_remote_expose, NULL);

  /******** REALIZE THE WIDGET *******/
  if (remote_flag == xaTRUE) XA_Realize_Remote(remote_widget);
  else xa_remote_ready = xaTRUE;
}

/* 
 * uses global last_widget 
 */
void XA_Realize_Remote(remote)
Widget remote;
{
  xa_remote_realized = xaTRUE;
  XtRealizeWidget(remote_widget);
  remoteW = XtWindow(remote_widget);
  XSync(theDisp,False);
  while(XtIsRealized(remote)==False) XSync(theDisp,False);

  { xaULONG gc_mask = 0;
    XGCValues gc_init;
    gc_init.function = GXcopy;                          gc_mask |= GCFunction;
    gc_init.foreground = button_main; gc_mask |= GCForeground;
    gc_init.background = button_back; gc_mask |= GCBackground;
    gc_init.graphics_exposures = False;         gc_mask |= GCGraphicsExposures;
    remoteGC  = XCreateGC(theDisp,remoteW,gc_mask,&gc_init);
  }
  { XWMHints xwm_hints;
    xwm_hints.input = True;
    xwm_hints.flags = InputHint;
    XSetWMHints(theDisp,remoteW,&xwm_hints);
    XSync(theDisp,False);
  }
  XMapWindow(theDisp,remoteW);
  XRaiseWindow(theDisp, remoteW );
  XSetWindowColormap(theDisp, remoteW, theCmap); 
  GetButtonColors(xa_chdr_now);
  DrawRemote(buttons,NUM_BUTTONS);
}

void XA_Unrealize_Remote(remote)
Widget remote;
{
  XSync(theDisp,False);
  XtPopdown(remote);
  XSync(theDisp,False);
  XtUnrealizeWidget(remote);
  XSync(theDisp,False);
  while(XtIsRealized(remote)==True) XSync(theDisp,False);
  xa_remote_realized = xaFALSE;
}


/* Event Handler */
void xa_remote_expose(wg, closure, event, notused)
Widget          wg;		XtPointer	closure;
XEvent		*event;		Boolean		*notused;
{ xa_remote_ready = xaTRUE;
  GetButtonColors(xa_chdr_now);
  DrawRemote(buttons,NUM_BUTTONS);
}


static int button_wait = 0;

static void PetuniaButtonsUp()
{ int i;
  button_wait = 0;
  for(i=0;i<NUM_BUTTONS;i++)
  { if (buttons[i].state & BUTTON_STATE_SEL)
        { buttons[i].state &= ~BUTTON_STATE_SEL;
          DrawButton(&buttons[i]);
        }
    if (buttons[i].state & BUTTON_STATE_FSEL)
        { buttons[i].state &= ~BUTTON_STATE_FSEL; }
  }
}

/* This stuff is to make sure the buttons are held down for X amount
 * of milliseconds so the user gets to see the button being pressed.
 * Otherwise on fast machines, they may not see it at all.
 */
void PetuniaButtonWait(client_data,id)
void *client_data;
void *id;
{
  button_wait++;  if (button_wait >= 3) PetuniaButtonsUp();
}


ACTION_PROC(remote_btn1dn)
{ XButtonEvent *xbut = (XButtonEvent *)event;
  int but;

  if (button_wait) return;  /* Too Soon */

  but = WhichButton(buttons,NUM_BUTTONS,xbut->x,xbut->y);
  if ( (but >= 0) && (but < NUM_BUTTONS) )
  { if (buttons[but].state & BUTTON_STATE_FIXED)
    { buttons[but].state |= BUTTON_STATE_FSEL;
    }
    else
    { buttons[but].state |= BUTTON_STATE_SEL; 
      DrawButton( &buttons[but] );
    }
    button_wait = 1;
    XtAppAddTimeOut(theContext,50,(XtTimerCallbackProc)PetuniaButtonWait,NULL);

  }
}

ACTION_PROC(remote_btn1up)
{ XButtonEvent *xbut = (XButtonEvent *)event;
  int but = WhichButton(buttons,NUM_BUTTONS,xbut->x,xbut->y);
  if ((but >= 0) && (but < NUM_BUTTONS))
  { if (buttons[but].state & BUTTON_STATE_SEL)
    { 
      if (buttons[but].func)  buttons[but].func(w,event,params,num_params);
    }
    else if (buttons[but].state & BUTTON_STATE_FSEL)
    {
      if (buttons[but].func)  buttons[but].func(w,event,&buttons[but]);
    }
  }
  button_wait++;  if (button_wait >= 3) PetuniaButtonsUp();
}

/***************************************
 * Find closet color in chdr, but NOT for monochrome displays
 *  r,g,b  8 bits each.
 *******************************/
xaULONG PetuniaGetColor(r,g,b,chdr)
xaULONG r,g,b;
XA_CHDR *chdr;
{ 

  if (x11_display_type & XA_X11_TRUE) return(X11_Get_True_Color(r,g,b,8));
  else if ((chdr) || (xa_chdr_first))
  { XA_CHDR *tchdr = (chdr)?(chdr):(xa_chdr_first);
	/* using xaFALSE for GrayScale?? */
    return( CMAP_Find_Closest(tchdr->cmap,tchdr->csize,r,g,b,8,8,8,xaTRUE) );
  }
  else
  { XColor col;		
    col.red   = r; col.green = g; col.blue  = b;
    col.flags = DoRed | DoGreen | DoBlue;
    XAllocColor(theDisp,theCmap,&col);
    return( (xaULONG)(col.pixel) );
  }
}

static void GetButtonColors(chdr)
XA_CHDR *chdr;
{ if (x11_display_type == XA_MONOCHROME)
  { button_main = button_back = BlackPixel(theDisp,theScreen);
    button_hi = button_lo = WhitePixel(theDisp,theScreen);
  }
  else
  { int temp = x11_byte_mismatch;
	/* Don't want to swap these colors since they are used differently */
    x11_byte_mismatch = xaFALSE;
    button_back = PetuniaGetColor(0x00,0x00,0x00,chdr);
    button_lo   = PetuniaGetColor(0x60,0x60,0x60,chdr);
    button_main = PetuniaGetColor(0xa8,0xa8,0xa8,chdr);
    button_hi   = PetuniaGetColor(0xd0,0xd0,0xd0,chdr);
    x11_byte_mismatch = temp;
  }
}


/*
 * Use remoteW, remoteGC
 *
 *
 *
 */
static void DrawRemote(buttons,num)
BUTTON *buttons;
xaULONG num;
{ int i;

  XSetClipMask(theDisp,remoteGC,None);
  XSetForeground(theDisp,remoteGC,button_main);
  XFillRectangle(theDisp,remoteW,remoteGC,0,0,REMOTE_WIDTH,REMOTE_HEIGHT);
  for(i=0; i < num; i++) DrawButton( &buttons[i] );
}


static void InitButtonsTypes(button_types,num)
BUTTON_TYPE *button_types;
xaULONG num;
{ xaULONG i;

  for(i=0; i < num; i++)
  { if (button_types[i].width)
    { button_types[i].hi = XCreatePixmapFromBitmapData(theDisp, remoteW, 
	(char *)button_types[i].hi_bitmap,
	(int)button_types[i].width, (int)button_types[i].height,0x01,0x00,1);
      button_types[i].lo = XCreatePixmapFromBitmapData(theDisp, remoteW, 
	(char *)button_types[i].lo_bitmap,
	(int)button_types[i].width, (int)button_types[i].height,0x01,0x00,1);
    }
  }
}

/**** Make sure remoteW exists at this point */
static void InitButtons(buttons,num)
BUTTON *buttons;
xaULONG num;
{ xaULONG i;
  for(i=0; i < num; i++)
  { if (buttons[i].bitmap)
    { buttons[i].pmap = XCreatePixmapFromBitmapData(theDisp, remoteW, 
	(char *)buttons[i].bitmap,
	(int)buttons[i].width, (int)buttons[i].height,0x01,0x00,1);
    }
  }
}

static void InitScrolls(scrolls,num)
SCROLLBAR *scrolls;
xaULONG num;
{ xaULONG i;

  for(i=0; i < num; i++)
  { 
    scrolls[i].hi = XCreatePixmapFromBitmapData(theDisp, remoteW, 
	(char *)scrolls[i].hi_bitmap,
	(int)scrolls[i].width, (int)scrolls[i].height,0x01,0x00,1);
    scrolls[i].lo = XCreatePixmapFromBitmapData(theDisp, remoteW, 
	(char *)scrolls[i].lo_bitmap,
	(int)scrolls[i].width, (int)scrolls[i].height,0x01,0x00,1);
  }
}


static void DrawButton(button)
BUTTON *button;
{ xaULONG type,x,y,width,height;
  int hi, lo; 

  if (button == 0) return;
  type = button->type;
  x = button->xpos;			y = button->ypos;
  width  = button_types[type].width;	height = button_types[type].height;

  if (button->state == BUTTON_STATE_OFF) { hi = button_hi; lo = button_lo; }
  else { hi = button_lo; lo = button_hi; }

  XSetClipOrigin(theDisp,remoteGC,x,y);
	/** If SubButton erase area first **/
  if (button->scroll >= 0)
  { XSetForeground(theDisp,remoteGC,button_main);
    XSetClipMask(theDisp,remoteGC,None);
    XFillRectangle(theDisp,remoteW,remoteGC,
				(int)x,(int)y,(int)width,(int)height);
  }
	/** Low Color **/
  if (button_types[type].lo)
  { XSetForeground(theDisp,remoteGC,lo);
    XSetClipMask(theDisp,remoteGC,button_types[type].lo);
    XFillRectangle(theDisp,remoteW,remoteGC,
				(int)x,(int)y,(int)width,(int)height);
  }
	/** Hi Color **/
  if (button_types[type].hi)
  { XSetForeground(theDisp,remoteGC,hi);
    XSetClipMask(theDisp,remoteGC,button_types[type].hi);
    XFillRectangle(theDisp,remoteW,remoteGC,
				(int)x,(int)y,(int)width,(int)height);
  }
	/** Text Color **/
  if (button->pmap)
  { if (x11_display_type == XA_MONOCHROME)
			XSetForeground(theDisp,remoteGC,button_hi);
    else		XSetForeground(theDisp,remoteGC,button_back);
    XSetClipMask(theDisp,remoteGC,button->pmap);
    XFillRectangle(theDisp,remoteW,remoteGC,
			(int)x,(int)y,(int)button->width,(int)button->height);
  }
  if (button->scroll >= 0) DrawScroll( &scrollbars[button->scroll] );
  XSync(theDisp,False);
}

static void DrawScroll(scroll)
SCROLLBAR *scroll;
{ xaULONG x,y,width,height;

  if (scroll == 0) return;
  if (scroll->type == 0) { x = scroll->xpos;		y = scroll->cur; }
  else			 { x = scroll->cur;		y = scroll->ypos; }

  width  = scroll->width;	height = scroll->height;

  XSetClipOrigin(theDisp,remoteGC,x,y);
	/** Low Color **/
  if (scroll->lo)
  { XSetForeground(theDisp,remoteGC,button_lo);
    XSetClipMask(theDisp,remoteGC,scroll->lo);
    XFillRectangle(theDisp,remoteW,remoteGC,
				(int)x,(int)y,(int)width,(int)height);
  }
	/** Hi Color **/
  if (scroll->hi)
  { XSetForeground(theDisp,remoteGC,button_hi);
    XSetClipMask(theDisp,remoteGC,scroll->hi);
    XFillRectangle(theDisp,remoteW,remoteGC,
				(int)x,(int)y,(int)width,(int)height);
  }
  XSync(theDisp,False);
}

/* ret xaTRUE if in button
 *     xaFALSE if not
 */
static xaULONG InButtonQuery(button,x,y)
BUTTON *button;
int x,y;
{ BUTTON_TYPE  *btype; if (button == 0) return(xaFALSE);
  if (button->type >= NUM_BUTTON_TYPES) return(xaFALSE);
  btype = &button_types[ button->type ];
  if ((x < button->xpos) || (y < button->ypos)) return(xaFALSE);
  if (x > (button->xpos + btype->width)) return(xaFALSE);
  if (y > (button->ypos + btype->height)) return(xaFALSE);
/*POD TODO: separate active region of button */
  return(xaTRUE);
}


static xaLONG WhichButton(buttons,num,x,y)
BUTTON *buttons;
xaULONG num;
int x,y;
{ int i;
  for(i=0; i<num; i++) if (InButtonQuery(&buttons[i],x,y)==xaTRUE) return(i);
  return(-1);
}

static void AdjustScroll(sidx, val, scale)
xaULONG sidx;
xaULONG val, scale;
{ SCROLLBAR *scroll; xaULONG pos;
  if (sidx >= NUM_SCROLLBARS) return;
  if (scale == 0) return; 
  scroll = &scrollbars[sidx];
  if (val > scale) val = scale; 
  pos = (val * scroll->length) / scale;

  if (scroll->type == 0) scroll->cur = (scroll->ypos + scroll->length) - pos;
  else			 scroll->cur =  scroll->xpos + pos;

}


void XA_Remote_Play_Common(prev,stop,next)  
xaULONG prev,stop,next;
{ buttons[BUTTON_PLAYPREV].state = prev;
  buttons[BUTTON_PLAYSTOP].state = stop;
  buttons[BUTTON_PLAYNEXT].state = next;
  DrawButton(&buttons[BUTTON_PLAYPREV]);
  DrawButton(&buttons[BUTTON_PLAYSTOP]);
  DrawButton(&buttons[BUTTON_PLAYNEXT]);
}

void XA_Remote_PlayPrev()  
 { XA_Remote_Play_Common(BUTTON_STATE_ON, BUTTON_STATE_OFF, BUTTON_STATE_OFF); }
void XA_Remote_PlayStop()  
 { XA_Remote_Play_Common(BUTTON_STATE_OFF, BUTTON_STATE_ON, BUTTON_STATE_OFF); }
void XA_Remote_PlayNext()  
 { XA_Remote_Play_Common(BUTTON_STATE_OFF, BUTTON_STATE_OFF, BUTTON_STATE_ON); }
void XA_Remote_StepPrev() { XA_Remote_PlayStop(); }
void XA_Remote_StepNext() { XA_Remote_PlayStop(); }

void XA_Remote_Pause()	  { XA_Remote_PlayStop(); }

void XA_Remote_AudioOff()
{ buttons[BUTTON_AUDIO].state = BUTTON_STATE_ON;
  DrawButton(&buttons[BUTTON_AUDIO]);
}
void XA_Remote_AudioOn()
{ buttons[BUTTON_AUDIO].state = BUTTON_STATE_OFF;
  DrawButton(&buttons[BUTTON_AUDIO]);
}
void XA_Remote_SpeedNorm()
{ buttons[BUTTON_SPEED].state = BUTTON_STATE_OFF;
  DrawButton(&buttons[BUTTON_SPEED]);
}
void XA_Remote_SpeedDiff()
{ buttons[BUTTON_SPEED].state = BUTTON_STATE_ON;
  DrawButton(&buttons[BUTTON_SPEED]);
}
void XA_Remote_Adj_Volume(vol,maxvol)
xaULONG vol,maxvol;
{ AdjustScroll(SCROLLBAR_VOLUME,vol,maxvol);
  DrawButton(&buttons[BUTTON_VOLBAR]);
}
void XA_Remote_ColorUpdate(chdr)
XA_CHDR *chdr;
{
  GetButtonColors(chdr);
  DrawRemote(buttons,NUM_BUTTONS);
}

void XA_Remote_Free()
{ int i;
  for(i=0; i< NUM_BUTTON_TYPES; i++)
  { if (button_types[i].hi)
	{ XFreePixmap(theDisp,button_types[i].hi); button_types[i].hi = 0; }
    if (button_types[i].lo)
	{ XFreePixmap(theDisp,button_types[i].lo); button_types[i].lo = 0; }
  }
  for(i=0; i< NUM_BUTTONS; i++)
  { if (buttons[i].pmap)
	{ XFreePixmap(theDisp,buttons[i].pmap); buttons[i].pmap = 0; }
  }
  for(i=0; i< NUM_SCROLLBARS; i++)
  { if (scrollbars[i].hi)
	{ XFreePixmap(theDisp,scrollbars[i].hi); scrollbars[i].hi = 0; }
    if (scrollbars[i].lo)
	{ XFreePixmap(theDisp,scrollbars[i].lo); scrollbars[i].lo = 0; }
  }
}


#else
#ifdef XA_REMOTE_CONTROL
/****************************************************************************
 *  XA_Create_Remote with non-portable, troublesome, silly Widgets.
 *  Schedules to be removed with haste.
 ****************************************************************************/
void xa_remote_expose();

#include "buttons/BM_step_prev.xbm"
#include "buttons/BM_play.xbm"
#include "buttons/BM_step_next.xbm"
#include "buttons/BM_next.xbm"
#include "buttons/BM_quit.xbm"
#include "buttons/BM_prev.xbm"
#include "buttons/BM_slower.xbm"
#include "buttons/BM_speed1.xbm"
#include "buttons/BM_faster.xbm"
#include "buttons/BM_vol_dn.xbm"
#include "buttons/BM_vol_off.xbm"
#include "buttons/BM_vol_up.xbm"
#include "buttons/BM_back.xbm"
#include "buttons/BM_stop.xbm"
#include "buttons/BM_vol_on.xbm"
#include "buttons/BM_fuzz.xbm"

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
#define BM_BACK		12
#define BM_STOP		13
#define BM_FUZZ		15

#ifdef XA_AUDIO
#define BM_VDOWN	9
#define BM_VOFF		10
#define BM_VUP		11
#define BM_VON 		14
#else
#define BM_VDOWN	15
#define BM_VOFF		15
#define BM_VUP		15
#define BM_VON 		15
#endif

static Pixmap BM_pmap[BM_NUMBER] = { 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0 };

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
  { foregnd = WhitePixel(theDisp,theScreen);
    backgnd = BlackPixel(theDisp,theScreen);
  }
  else if (x11_display_type & XA_X11_TRUE)
  { backgnd = X11_Get_True_Color(0,0,0,8);
    foregnd = X11_Get_True_Color(255,255,255,8);
  }
  else if (xa_chdr_first)
  { XA_CHDR *chdr = xa_chdr_first;
   backgnd = CMAP_Find_Closest(chdr->cmap,chdr->csize,0,0,0,8,8,8,xaTRUE);
   foregnd = CMAP_Find_Closest(chdr->cmap,chdr->csize,255,255,255,8,8,8,xaTRUE);
   i = 2;
  }
  else
  { foregnd = WhitePixel(theDisp,theScreen);
    backgnd = BlackPixel(theDisp,theScreen);
  }

 if (x11_verbose_flag == xaTRUE)
	fprintf(stderr,"foregnd = %x backgnd = %x\n",foregnd,backgnd);

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
  if (remote_flag == xaTRUE) XA_Realize_Remote(remote_widget);
  else xa_remote_ready = xaTRUE;
}

void XA_Realize_Remote(remote)
Widget remote;
{
  xa_remote_realized = xaTRUE;
  XtPopup(remote,XtGrabNone);
  XSync(theDisp,False);
  while(XtIsRealized(remote)==False) XSync(theDisp,False);
  while(XtIsRealized(last_widget)==False) XSync(theDisp,False);
  XRaiseWindow(theDisp, XtWindow(remote) );
}

void XA_Unrealize_Remote(remote)
Widget remote;
{
  XSync(theDisp,False);
  XtPopdown(remote);
  XSync(theDisp,False);
  XtUnrealizeWidget(remote);
  XSync(theDisp,False);
  while(XtIsRealized(remote)==True) XSync(theDisp,False);
  xa_remote_realized = xaFALSE;
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

/* Event Handler */
void xa_remote_expose(wg, closure, event, notused)
Widget          wg;		XtPointer	closure;
XEvent		*event;		Boolean		*notused;
{ xa_remote_ready = xaTRUE;
}

/* convenience function so xanim.c code can change play button during
 * pauses. */
void XA_Remote_Pause()	   { XA_Remote_Change(play_widget,BM_PLAY); }
void XA_Remote_PlayNext()  { XA_Remote_Change(play_widget,BM_PLAY); }
void XA_Remote_PlayPrev()  { XA_Remote_Change(play_widget,BM_BACK); }
void XA_Remote_PlayStop()  { XA_Remote_Change(play_widget,BM_STOP); }
void XA_Remote_StepNext()  { XA_Remote_Change(play_widget,BM_PLAY); }
void XA_Remote_StepPrev()  { XA_Remote_Change(play_widget,BM_BACK); }
void XA_Remote_AudioOff()  { XA_Remote_Change(audio_widget,BM_VOFF);}
void XA_Remote_AudioOn()   { XA_Remote_Change(audio_widget,BM_VON); }
void XA_Remote_SpeedNorm() { XA_Remote_Change(norm_widget,BM_NORM); }
void XA_Remote_SpeedDiff() { XA_Remote_Change(norm_widget,BM_FUZZ); }
void XA_Remote_Adj_Volume(vol,maxvol)
xaULONG vol,maxvol;
{ xaULONG i = vol * maxvol; /* do nothing */
}

void XA_Remote_Free()
{ int i;
 for(i=0; i < BM_NUMBER; i++) 
   if (BM_pmap[i]) { XFreePixmap(theDisp,BM_pmap[i]); BM_pmap[i] = 0; }
}

#endif
#endif

void XA_Free_CMAP()
{
  if (theCmap && (theCmap != DefaultColormap(theDisp,theScreen)) )
			{ XFreeColormap(theDisp, theCmap); theCmap = 0; }
}
