
/*
 * xa_config.h
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


/* These defines are used to initialize the various flags that control
 * the behavious of XAnim. They all can be overriden on the command
 * line except for DEFAULT_PLUS_IS_ON and DEFAULT_X11_VERBOSE_FLAG.
 */


/* If this is aTRUE then '+' will turn an option on and '-' will turn it off.
 * if this is xaFALSE then '+' will turn an option off and '-' will turn
 * it on.
 */
#define DEFAULT_PLUS_IS_ON	xaTRUE

/* If xaTRUE then animations will be buffered ahead of time. Uses more memory.
 */
#define DEFAULT_BUFF_FLAG	xaFALSE

/* If xaTRUE then animations will be read from disk. This and BUFF_FLAG are
 * mutually exclusive with BUFF_FLAG having priority.
 */
#define DEFAULT_FILE_FLAG	xaTRUE

/* If xaTRUE then iff animations will always loop back to 1st frame instead
 * of to the 2nd delta. There is no easy way of knowing this ahead of time
 * and and is rare and so it should be kept xaFALSE.
 */
#define DEFAULT_IFF_LOOP_OFF	xaFALSE

/* If xaTRUE then IFF CRNG (color cycling chunks) will be activated for
 * single image IFF files.
 */
#define DEFAULT_CYCLE_IMAGE_FLAG	xaTRUE

/* If xaTRUE then IFF CRNG (color cycling chunks) will be activated for
 * IFF animation files.
 */
#define DEFAULT_CYCLE_ANIM_FLAG		xaFALSE

/* If xaTRUE then image height is reduced by half if an IFF image/animation
 * is interlaced.
 */
#define DEFAULT_ALLOW_LACE_FLAG		xaFALSE

/* If xaTRUE then the cmap will fade to black between files. Only works
 * on PSEUDO_COLOR displays.
 */
/* NOTE: CURRENTLY NOT SUPPORTED */
#define DEFAULT_FADE_FLAG	xaFALSE
#define DEFAULT_FADE_TIME	20

/* If xaTRUE then window will be the size of the largest image. Smaller
 * images and animations will be in upper left hand corner.
 */

#define DEFAULT_NORESIZE_FLAG	xaFALSE

/* If xaTRUE then a window can be resized with the mouse. This results
 * in the animation being scaled to fit and window. This also overrides
 * DEFAULT_NORESIZE_FLAG.
 */
#define DEFAULT_ALLOW_RESIZING	xaFALSE

/* This affect IFF type 5 and J compressions as well as most FLI/FLC type
 * compressions. Only the minimum area of the screen is updated that
 * contains the changes from one image to the next. This is forced off
 * in MONOCHROME mode due to peculiarities of the Floyd-Steinburg
 * dithering algorithm. Having this on can cause "apparent" pauses in
 * the animation because of the time difference between updating the
 * entire screen and just part of it. This will occur if your hardware
 * can not display the images at the specified rate. Turning optimization
 * off will force the entire animation to go at the slower rate.
 */
#define DEFAULT_OPTIMIZE_FLAG	xaTRUE

/* If this is xaTRUE and DEFAULT_BUFF_FLAG is xaTRUE, the images will be
 * put into pixmaps. Pixmaps have the following advantages:
 *     they are stored locally  (in case you're running remotely)
 *     they aren't copied twice (like most X11 servers do with XImages)
 *     they could be in display memory(if your hardware has enough of it)
 * It usually speeds things up.
 */
#define DEFAULT_PIXMAP_FLAG	xaFALSE
/* If xaTRUE then Floyd-Steinberg dithering is used for MONOCHROME displays
 */
#define DEFAULT_DITHER_FLAG	xaTRUE

/* This cause XAnim to print out debugging information. Valid levels are
 * from 1 to 5, with 0 being off.
 */
#define DEFAULT_DEBUG		0

/* When this is xaTRUE it causes XAnim to print out extra information about
 * the animations
 */
#define DEFAULT_VERBOSE		xaFALSE

/* When this is xaTRUE it prevents XAnim from printing out revision and
 * copyright info. Useful if being called up from WWW browser.
 * This is overriden by -q option.  This is also overridden by verbose option
 * and debug option.
 */
#define DEFAULT_QUIET		xaFALSE

/* This is the default number of times to loop through each file before
 * continuing on.
 */
#define DEFAULT_LOOPEACH_FLAG	1

/* When this is xaTRUE it causes XAnim to "ping-pong" an animation. In other
 * words, the anim will be played forwards to the end and then backwards
 * to the beginning. This will be counted as one loop.
 */
#define DEFAULT_PINGPONG_FLAG	xaFALSE

/* This is the number of milliseconds between frames of the animation.
 * If 0 then the number of milliseconds is taken from the animation
 * itself.
 */
#define DEFAULT_JIFFY_FLAG	0

/* This is the number of milliseconds for single image cycling IFF files.
 */
#define DEFAULT_CYCLING_TIME   8000

/* Not yet supported
 */
#define DEFAULT_PACK_FLAG	xaFALSE

/* This causes XAnim to print out more information about the X11
 * display on which it is running.
 */
#define DEFAULT_X11_VERBOSE_FLAG	xaFALSE


/* COLOR DITHERING. Currently only NONE and FLOYD are supported. FLOYD
 * dithering can add substantially to run and start up times.
 */
#define CMAP_DITHER_NONE	0
#define CMAP_DITHER_FLOYD	1
#define CMAP_DITHER_ORDERED	2

/* COLOR MAP STUFF.
 *    luma_sort:   sorts the color map based on color's brightness
 *    map_to_1st:  remaps new cmaps into 1st cmap. If try_to_1st fails
 *		   or isn't set.
 *    map_to_one:  Creates one colormap from all color maps and then
 *		   remaps all images/anims to that cmap. Eliminates
 *		   flickering, but may reduce color quality.
 *    play_nice:   Allocate colors from X11 defaults cmap. Screen colors
 *		   won't change when mouse is moved into and out of 
 *		   animation window. Color Cyling impossible with this
 *		   option. If you are running with non-default Visual,
 *		   X11 might have to change the colormap anyways.
 *    hist_flag:   If XAnim needs to generate one cmap from multiple
 *		   (ie map_to_one/play_nice or cmap > X11's cmap) then
 *		   do histograms on any uncompressed images to aid in
 *		   color reductions if necessary. More time at startup
 *		   but might help color quality.
 */
#define DEFAULT_CMAP_LUMA_SORT	xaFALSE
#define DEFAULT_CMAP_MAP_TO_1ST	xaFALSE
#define DEFAULT_CMAP_MAP_TO_ONE	xaFALSE
#define DEFAULT_CMAP_PLAY_NICE  xaTRUE
#define DEFAULT_CMAP_HIST_FLAG  xaFALSE
#define DEFAULT_CMAP_DITHER_TYPE CMAP_DITHER_NONE

/*
 * Options for Median Cut stuff.
 */
#define CMAP_MEDIAN_SUM    0
#define CMAP_MEDIAN_CENTER 1
#define DEFAULT_CMAP_MEDIAN_TYPE 0

/* These are for TrueColor animatons such and HAM,HAM8 or 24 bit RLE files.
 * true_to_332 means images are truncated to 3 bits red, 3 bits green and
 * 	2 bits blue in order to fit in a 256 entry cmap. If your X11 display
 * 	supports less than 256 cmap entries, these numbers will be less.
 *
 * true_to_gray means TrueColor anims are converted to 8bits of gray.
 * true_to_1st means TrueColor anims have a cmap created by running
 * 	a median cut algorithm on their 1st image and then using that
 *	for the remainder of the images. Anim must be buffered.
 * true_to_all means TrueColor anims have a cmap created by running a
 *	median cut algorithm on each image to create a cmap for each
 *	image. Adds substantially to start up time. Anim must be buffered.
 * true_to_map: This is automatically set when true_to_1st and true_to_all
 *	are turned on. It can be optionally used with true_to_332 and
 *	true_to_gray to used more bits(than332) when dithering TrueColor
 *	anims down to the Displays cmap. Really doesn't make any sense
 *	to set this with true_to_gray unless your display has less than
 *	8 bits of grayscale.
 */
#define DEFAULT_TRUE_TO_332     xaTRUE
#define DEFAULT_TRUE_TO_GRAY    xaFALSE
#define DEFAULT_TRUE_TO_1ST     xaFALSE
#define DEFAULT_TRUE_TO_ALL     xaFALSE
#define DEFAULT_TRUE_MAP_FLAG	xaFALSE

/* 6 is 256K, 7 is 2M and 8 is 16M */
#define CMAP_CACHE_MAX_BITS 6
#define DEFAULT_CMAP_MEDIAN_CACHE xaFALSE

/*
 * Title Options
 *
 * NONE		Title is just "XAnim"
 * FILE		Title is just anim name while running. When stopped the
 *		frame number is included.
 * FRAME	Title is anim anim and frame number while running.
 */

#define XA_TITLE_NONE   0
#define XA_TITLE_FILE   1
#define XA_TITLE_FRAME  2

#define DEFAULT_XA_TITLE_FLAG  1

/*
 * GAMMA Options.
 *
 * DISP_GAMMA:	Is the default gamma of your display. This is to be
 *		used to universally darken or lighten animations. Also
 *		may be used to help gamma correct your display. :^)
 *
 * ANIM_GAMMA:	This is used to specify that all anims(unless specified
 *		on the command line) are treated as if they were color
 *		compensated to this gamma value. Unfortunately, for
 *		the majority of the cases, you have no clue what gamma
 *		value the animation was compensated to.
 */
#define DEFAULT_DISP_GAMMA 1.0
#define DEFAULT_ANIM_GAMMA 1.0

/* 
 * At the end of displaying the command line, xanim will either loop
 * through again(xaFALSE), exit(xaTRUE), or pause and wait user input(xaPAUSE).
 */
#define DEFAULT_XA_EXIT_FLAG  xaFALSE

/*
 * This determines if a remote control window should come up by default
 * or not. It can still be overridden by the command line. This is only
 * valid if REMOTE control support is compiled in.
 */
#define DEFAULT_XA_REMOTE_FLAG  xaTRUE

/* xaTRUE or xaFALSE */
#define DEFAULT_XA_AUDIO_ENABLE xaTRUE

/* 0 - 100 */
#define DEFAULT_XA_AUDIO_VOLUME 40

/* This is the default Audio Port that is enabled.
 * This value may be overridden by the env SPEAKER variable if defined. 
 * NOTE: on some machines EXTERNAL and HEADPHONES are the same.
 *
 * DEFAULT_XA_AUDIO_PORT settings:
 *   XA_AUDIO_PORT_INT      Internal speaker
 *   XA_AUDIO_PORT_HEAD     Headphones
 *   XA_AUDIO_PORT_EXT      Line Out
 *   XA_AUDIO_PORT_NONE	  None - no sound OR no change from before(SPARC)
 *
 * SPEAKER environment variable settings:
 *   INTERNAL		Internal speaker(also speaker box on Sparcs)
 *   HEADPHONES		Headphones
 *   EXTERNAL		LineOut/External speakers
 */

#define DEFAULT_XA_AUDIO_PORT   XA_AUDIO_PORT_INT

/* Currently only used for AIX */
/* Some AIX machines have "/dev/acpa0/1" instead. */
/* Some AIX machines have "/dev/baud0/1" instead. UltiMedia Sound system */
#ifdef XA_AIX_AUDIO
#define DEFAULT_AUDIO_DEVICE_NAME   "/dev/paud0/1"
#else
#define DEFAULT_AUDIO_DEVICE_NAME   "/dev/audio"
#endif

