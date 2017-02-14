
/*
 * xa_audio.c
 *
 * Copyright (C) 1994-1998,1999 by Mark Podlipec.
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

/***************************************************************************
 * Fixes (Most are sent in by beta testers for which I'm grateful)
 *
 *	   - Linux: jixer should be mixer.
 *	   - Linux: Added 8 bit converison routines.
 *	   - Solaris audioio.h in include/sys not include/sun.
 * 3Aug94  - Signals Suck. Use XtAppAddTimeOut instead.
 * 4Aug94  - Linux 8 bit init incorrectly returning XA_AUDIO_SIGNED_1M.
 *           Now it's XA_AUDIO_LINEAR_1M.
 * 4Aug94  - Solaris opening ioctl using devtype instead of type and
 *	     type should be audio_device_t (missing _t before).
 * 4Aug94  - Linux: ioctl call to set volume was be reference only. It's
 *	     now properly #ifdef'd depending on XA_LINUX_NEWER_SND.
 * 5Aug94  - Linux: volume ioctl uses SOUND_MIXER_VOLUME now. Did try
 *	     SOUND_MIXER_PCM, but some machines didn't like that.
 * 8Aug94  - All: added XA_Audio_Pause routine for those devices that
 *	     support pause/resume. Much better that having to flush it.
 *	   - Solaris: XA_SOLARIS should've been SOLARIS. was causing
 *	     core dumps etc.
 *	   - ALL(but only affects Solaris): Solaris doesn't like writes to
 *	     the audio buffer that aren't multiples of it's size. Is this
 *	     the problem with SGI's? Anyways UpdateRing now enforces this.
 *	   - Sparc: Detects if audio device is already in use and continues
 *	     on without it. Before it used to hang until audio was freed.
 *	   - Sparc: Volume turned off when flushing and then back on after.
 *	     Gets rid of clicks/pops when exiting.
 * 10Aug94 - Fixed problem with SGI machines. Samples not bytes and 
 * 	   - SIGNED_2MB not LINEAR_2MB.
 * 10Aug94 - Change volume to 0-100%. May have broken them all. :^)
 * 11Aug94 - Added code for HP9000/700 series. Have no clue if it works.
 * 31Aug94 - Linux: Finally got the linux docs. I'm not querying the device
 *	     to see what volume channel to use.
 * 12Sep94 - SPARC 5: now look for CS4231 Audio devices. Should be treated
 *	     the same as  Sparc 10's DBRI device.
 * 21Sep94 - ALL: Closest_Freq() must have ifreq be signed in order for the
 *           comparison's to be done correctly.
 * 22Sep94 - Screw Pausing for now. Will put back later if deemed necessary.
 * 27Sep94 - HP: found serious typo bug that caused the Audio Ring buffers
 *	     to be overwritten by a large amount, trashing all sorts of
 *	     memory and causing much grief. Thanks to John Atwood for
 *	     helping find this one.
 * 13Oct94 - SGI: really change LINEAR_2MB to be SIGNED_2MB this time. Arrgh.
 * 26Oct94 - Added code to support NAS 1.2 p1 
 *					- Bob Phillips (bobp@syl.nj.nec.com)
 * 28Oct94 - Added code to support NEC EWS 4800/330 - NEC
 *					- Bob Phillips (bobp@syl.nj.nec.com)
 * 02Nov94 - AF hardware type was set to 16 bit little endian, yet the AF
 *	     library was told to expect native endian. This didn't work
 *	     on Suns(nor would it on other big endian machines).
 *	                        - Andrzej Filinski <andrzej+@POP.CS.CMU.EDU>
 * 13Nov94 - NAS: fine tuned code for less drop out. Fixed up stop/start
 *	     problems(echos).
 * 13Jan95 - SGI: sgi volume is exponentional, not linear - Paul Close
 * 19Jan95 - Sparc: Older Sparc can only have 1 audio output at a time
 *           and not 0. This made it impossible to enable the headphones
 *	     because the code would first try turning off the speaker and
 *	     take an error.
 * 30Jan95 - SONY: added support for Sony NEWS-5000 workstations - code
 *	     written by Kazushi Yoshida.
 * 16Mar95 - Finished remodified audio/vidoe sync code to handle more
 *           compute intensive video(large frame or jpeg/mpeg)
 * 17Mar95 - Merged Audio_Outputs into one routine so I didn't have to
 *           keep modifying all of them(95% of the code was the same).
 * 17Mar95 - Fixed bug in NAS audio that caused memory corruption after
 *           an "unable to Toggle" error.
 * 17Mar95 - Added Audio support for IBM AIX S6000 machines - 
 *		Code written by jeffk@watson.ibm.com (Jeff Kusnitz)
 * 06Apr95 - Added Audio support for directly using HP's Audio Device
 *		rather than going through all the upper audio library
 *		layers. (HPDEV) Code written by Tobias Bading.
 * 10Apr95 - Fixed bug where when Audio finished before Video, it wasn't
 *	     being turned off before being turned on during the next loop
 *	     or for the next animation.
 * 10Apr95 - Fixed where hard_buff size was being set larger than the
 *	     allocated buffer size for the following machines(Linux/AIX.
 *           (All sorts of bad behavior - core's dumps etc).
 * 10Apr95 - Fixed MS ADPCM bug when 11Khx and 22Khz ADPCM freq were scaled
 *	     to 8Khz.
 * 30Apr95 - Added audio support for Digital Alpha workstations
 *           running MMS (morris@zko.dec.com)
 * 11May95 - Added Audio support for NetBSD - written by Roland C Dowdeswell.
 * 13Jun95 - SONY: modified for SVR4/NEWS-OS 6.x machines 
 *	     by Takashi Hagiwara, Sony Corporation (hagiwara@sm.sony.co.jp)
 * 04Sep95 - SOLARIS: replaced SOLARIS define with SVR4. see what happens.
 * 12Apr96 - HPDEV: occasionally audio device reports busy when trying to
 *           set the sample rate. Now loop on this condition.
 * 25Nov96 - HP AServer. Code fixed up by Steve Waterworth.
 * 18Mar97 - Linux patch by Corey Minyard improves video/audio syncing on
 *           some soundcards by using SNDCTL_DSP_SETFRAGMENT.
 * 25Sep97 - Kevin Hildebrand recommended using AUDIO_ENCODING_SLINEAR
 *           for NetBSD audio. Jarkko Torppa also reported this.
 * 02Jan98 - SETFRAGMENT patch was causing 1/2 second delay on start on my
 *	     linux system. Temporarily backing patch out til I can further
 *	     debug it.  search for SETFRAG if you want to put it back in.
 * 27Apr98 - Updates to NetBSD audio init code from Charles Hannum.
 * 21Feb99 - Added routine *_Audio_Prep  to hide initialization delays
 *	     when starting audio.
 * 02Mar99 - Linux: Change XA_LINUX_NEWER_SND TO OLDER_SND to avoid confusion.
 *
 ****************************************************************************/


/****************************************************************************/
/**************** AUDIO DOCUMENTATION ***************************************/
/****************************************************************************/
/*
 In order to support other machines type - XXX, the following routines must
 be created and should be inside an #ifdef XA_XXX_AUDIO.

 Currently Needed Routines:
 -----------------------------------
   void  XA_Audio_Setup()
   void  XXX_Audio_Init()
   void  XXX_Audio_Kill()
   void  XXX_Audio_Off()
   void  XXX_Audio_Prep()
   void  XXX_Audio_On()
   xaULONG XXX_Closest_Freq(target_freq)
   void  XXX_Set_Audio_Out(flag)   slowly replacing toggles 
   void  XXX_Speaker_Tog(flag)
   void  XXX_Headphone_Tog(flag)
   void  XXX_LineOut_Tog(flag)
   void  XXX_Audio_Output()

 Currently Optional Routines:
 -----------------------------------
   XXX_Adjust_Volume(volume)

 NOTES: Currently stereo isn't supported. It could/can/will be added
 later.

 The routine XA_Audio_Setup is responsible for mapping these routines
 to the same routines(with XXX replaced by XA). See the XA_Audio_Setup
 in the Sun section.

 Also the Hardware Audio Type should be defined in xanim.h. See the
 AUDIO SECTION of xanim.h. This is then used in XA_Add_Sound routine
 to determine the correct audio codec routine to convert the anim's
 audio format to the hardware's audio format.
(NOTE: moving toward asking hardware what it can support for each
       type of audio. )

 Brief description of how XAnim does the sound(may not be correct or finished
 at the time you read this :^)

 As XAnim intially parses through the animations, a link list of SND_HDR's
 is created(with the XA_Add_Sound routine) and attached to that animation
 (in it's ANIM_HDR). These SND_HDR's contain audio information/data as it
 existed in the animation file(it may just have a file postion and length so
 the audio data can be read into memory or it may have a buffer already 
 containing the audio data).
 
 XA_Add_Sound determines if the audio format is support and what routine
 should be used to decompress the audio. It also determines the closest
 hardware frequency(with XA_Closest_Freq) and calculates how much time
 (ms and fractional ms) it will take to play a converted sample that
 is xa_audio_hard_buf(set by XA_Closest_Freq) samples in size along
 with other information.

 A Ring of Audio buffers is created at init time(by XA_Audio_Init).
 The XA_Update_Ring routine fills these buffers with identical sized
 chunks of converted audio. The fact they're the same size simplifies
 the keeping of audio time(since each audio chunk takes exactly the
 same amount of time-which has been pre-calculated). audio time is
 used to keep the video in sync with the audio.

 XA_Update_Ring is called in XA_Audio_Prep() just before audio is
 started up(by XA_Audio_On) and is called in XA_Audio_Output() after
 a Audio Ring buffer has been sent to the audio device.

 XA_Update_Ring fills empty ring positions by walking along the list
 of SND_HDRs, converting the anim's audio to the proper audio format
 a chunk at a time. It does this by calling the audio codec function
 for that SND_HDR. The audio codec function converts and moves a set
 number of audio samples into the given ring buffer and then returns
 how many bytes in size that is.
 
 XA_Audio_Output just has to look at the current audio ring pointer,
 send that many bytes to the audio device, update the audio time,
 move along the ring and then call XA_Update_Ring.

 I might make XA/XXX_Audio_Output generic and then make machine
 specific sub portions of it. As more machine types are added this
 might make more sense.

 ----

 NOTE: I'm moving toward a separate audio process, this allows
       blocking when writing to the audio device.  However, the
       audio device should be opened non-blocking so that
       XAnim may continue on without audio if the audio device
       is in use by another program.

 XA_Audio_Prep() should enable and prep the audio. 

 XA_Audio_On() should actually start the audio.

 XA_Audio_Off() should stop the audio. Ideally this routine should
 immediate stop audio and flush what ever information has piled
 up. Unfortunately, many audio drivers are (deficient?) lacking
 this support.

 XA_Audio_Init() opens the audio device and initializes the variables.
 XA_Audio_Kill() closes the device and frees up any hardware specific
 memory structures.
 
 ----
 Mark Podlipec - podlipec@Baynetworks.com  or  podlipec@@shell.portal.com
*/

/* TOP */

#include "xa_audio.h"

/* POD note:  For opening audio device non-blocking. Should probably set it
 * back to blocking after successful open, but ONLY if compiling with
 * the audio child.
 */

#ifndef O_NDELAY
#ifdef O_NONBLOCK
#define O_NDELAY  O_NONBLOCK
#else
#define O_NDELAY 0
#endif
#endif

extern XA_AUD_FLAGS *vaudiof;
extern xaULONG xa_vaudio_present;
extern XtAppContext  theAudContext;


/***************** ITIMER EXPERIMENTATION ********************************/
/* number of ms we let audio buffer get ahead of actual audio. */
xaLONG xa_out_time = 250; /* PODNOTE: this is actually reset later on */
xaULONG xa_out_init = 0;  /* number of initial audio ring bufs to write ahead*/

/* 1000/XA_OUT_FREQADJ is number of extra itimer calls  */
#define XA_OUT_FREQADJ 1000

/***************** XtAppAddTimeOut EXPERIMENTATION *************************/
XtIntervalId xa_interval_id = 0;
xaULONG        xa_interval_time = 1;

extern Display	*theDisp;
extern xaULONG	xa_kludge900_aud;


/**** Non Hardware Specific Functions ************/
void XA_Audio_Init_Snd();
static void Init_Audio_Ring(xaULONG ring_num,xaULONG buf_size);
static void Kill_Audio_Ring();
static void XA_Update_Ring();
static void XA_Flush_Ring();
XA_SND *XA_Audio_Next_Snd();
void XA_Read_Audio_Delta();
xaLONG XA_Read_AV_Time();
extern xaLONG xa_time_now;
extern xaUBYTE *xa_audcodec_buf;
extern xaULONG xa_audcodec_maxsize;
extern int xa_aud_fd;


extern xaULONG XA_ADecode_PCMXM_PCM1M();
extern xaULONG XA_ADecode_PCM1M_PCM2M();
extern xaULONG XA_ADecode_DVIM_PCMxM();
extern xaULONG XA_ADecode_DVIS_PCMxM();
extern xaULONG XA_ADecode_IMA4M_PCMxM();
extern xaULONG XA_ADecode_IMA4S_PCMxM();


typedef struct AUDIO_RING_STRUCT
{
  xaULONG time;
  xaULONG timelo;
  xaULONG len;
  xaUBYTE *buf;
  struct AUDIO_RING_STRUCT *next;
} XA_AUDIO_RING_HDR;

XA_AUDIO_RING_HDR *xa_audio_ring = 0;
XA_AUDIO_RING_HDR *xa_audio_ring_t = 0;

#define XA_AUDIO_MAX_RING_BUFF 2048

/* NOTE: These must NOT be larger than XA_AUDIO_MAX_RING_BUFF above */
#define XA_HARD_BUFF_2K 2048
#define XA_HARD_BUFF_1K 1024


void Gen_Signed_2_uLaw();
void Gen_uLaw_2_Signed();
void Gen_aLaw_2_Signed();
void Gen_Arm_2_Signed();

xaLONG  XA_uLaw_to_Signed();
xaUBYTE XA_Signed_To_uLaw();
xaULONG XA_Audio_Linear_To_AU();

/*POD NOTE: Make these tables dynamically allocated */
/* Sun ULAW CONVERSION TABLES/ROUTINES */
xaUBYTE xa_sign_2_ulaw[256];
xaULONG xa_ulaw_2_sign[256];
xaULONG xa_alaw_2_sign[256];

/* ARM VIDC MULAW CONVERSION TABLES/ROUTINES */
xaULONG xa_arm_2_signed[256];

/* AUDIO CODEC DELTA ROUTINES */ 
xaULONG XA_Audio_1M_1M();
xaULONG XA_Audio_PCM1M_PCM2M();
xaULONG XA_Audio_PCM1S_PCM2M();
xaULONG XA_Audio_PCMXM_PCM1M();
xaULONG XA_Audio_PCMXS_PCM1M();
xaULONG XA_Audio_PCM2X_PCM2M();


/* AUDIO CODEC BUFFERING ROUTINES */

extern xaULONG xa_audio_present;
extern xaULONG xa_audio_status;

extern XA_SND *xa_snd_cur;
extern xaLONG  xa_time_audio;
extern xaLONG  xa_timelo_audio;

extern XA_AUD_FLAGS *XAAUD;
/*
extern xaULONG xa_audio_mute;
extern xaLONG  xa_audio_volume;
extern xaULONG xa_audio_newvol;
extern double xa_audio_scale;
extern xaULONG xa_audio_buffer;
extern xaULONG xa_audio_playrate;
extern xaULONG xa_audio_divtest;  Z* testing only *Z
extern xaULONG xa_audio_port;
*/

xaULONG xa_vaudio_hard_buff = 0;		/* VID Domain snd chunk size */

static xaULONG xa_audio_hard_freq;		/* hardware frequency */
xaULONG xa_audio_hard_buff;				/* preferred snd chunk size */
static xaULONG xa_audio_ring_size;		/* preferred num of ring entries */
xaULONG xa_audio_hard_type;				/* hardware sound encoding type */
static xaULONG xa_audio_hard_bps;		/* hardware bytes per sample */
static xaULONG xa_audio_hard_chans;		/* hardware number of chan. not yet */
static xaULONG xa_audio_flushed = 0;

void  XA_Audio_Setup();
void (*XA_Audio_Init)();
void (*XA_Audio_Kill)();
void (*XA_Audio_Off)();
void (*XA_Audio_Prep)();
void (*XA_Audio_On)();
void (*XA_Adjust_Volume)();
xaULONG (*XA_Closest_Freq)();
void  (*XA_Set_Output_Port)(); /* POD slowly replacing _Tog's */
void  (*XA_Speaker_Tog)();
void  (*XA_Headphone_Tog)();
void  (*XA_LineOut_Tog)();

void New_Merged_Audio_Output();


/****************************************************************************/
/**************** NULL AUDIO DEFINED ROUTINES********************************/
/****************************************************************************/
/* useful for backing out audio when device opens fails, etc */

static void  XA_NoAudio_Nop();
static xaULONG XA_NoAudio_Nop1();
static void  XA_NoAudio_Nop2();
static void  XA_Null_Audio_Setup();

void XA_Null_Audio_Setup()
{
  XA_Audio_Init		= XA_NoAudio_Nop;
  XA_Audio_Kill		= XA_NoAudio_Nop;
  XA_Audio_Off		= XA_NoAudio_Nop2;
  XA_Audio_Prep		= XA_NoAudio_Nop;
  XA_Audio_On		= XA_NoAudio_Nop;
  XA_Closest_Freq	= XA_NoAudio_Nop1;
  XA_Set_Output_Port	= XA_NoAudio_Nop2;
  XA_Speaker_Tog	= XA_NoAudio_Nop;
  XA_Headphone_Tog	= XA_NoAudio_Nop;
  XA_LineOut_Tog	= XA_NoAudio_Nop;

  xa_snd_cur = 0;
  xa_audio_present = XA_AUDIO_UNK;
  xa_audio_status  = XA_AUDIO_STOPPED;
  xa_audio_ring_size  = 8;
}
void XA_NoAudio_Nop()				{ return; }
void XA_NoAudio_Nop2(flag)	xaULONG flag;	{ return; }
xaULONG XA_NoAudio_Nop1(num)	xaULONG num;	{ return(0); }

/****************************************************************************/
/**************** NO AUDIO DEFINED ROUTINES *********************************/
/****************************************************************************/
#ifndef XA_AUDIO

void XA_No_Audio_Support();

void XA_Audio_Setup()
{ 
  XA_Null_Audio_Setup();
  XA_Audio_Init	= XA_No_Audio_Support;
}

void XA_No_Audio_Support()
{
  fprintf(stderr,"AUDIO SUPPORT NOT COMPILED IN THIS EXECUTABLE.\n");
  return;
}
#endif
/****************************************************************************/
/******************* END OF NO AUDIO SPECIFIC ROUTINES **********************/
/****************************************************************************/


/****************************************************************************/
/**************** SPARC SPECIFIC ROUTINES ***********************************/
/****************************************************************************/
#ifdef XA_SPARC_AUDIO

/******** START OF FIX SUN **********************************/
/* SINCE CC, GCC and SUN can't get their act together, let's do it
 * for them. This is to fix some SERIOUS Bugs with Sun's include files.
 *
 * CC works fine with /usr/include/sys/ioccom.h, but not gnu's
 * GCC works fine with it's version of Sun's ioccom.h, but not sun's
 * You mix them up and your code doesn't run. ARGH!!!
 */

/* POD NOTE: We'll NOW HAVE TO RELY ON XA_???_AUDIO defines to determine
 * SUN IO compatiblities. Wadda pain.
 */
#ifndef SVR4    /* was SOLARIS */

/* from sys/ioccom.h */
#undef _IO
#define _IO(x,y)        (_IOC_VOID|((x)<<8)|y)
#undef _IOR
#define _IOR(x,y,t)     (_IOC_OUT|((sizeof(t)&_IOCPARM_MASK)<<16)|((x)<<8)|y)
#undef _IOWR
#define _IOWR(x,y,t)    (_IOC_INOUT|((sizeof(t)&_IOCPARM_MASK)<<16)|((x)<<8)|y)

/* from audioio.h  A = 0x41 */
#undef AUDIO_GETINFO
#define AUDIO_GETINFO   _IOR(0x41, 1, audio_info_t)
#undef AUDIO_SETINFO
#define AUDIO_SETINFO   _IOWR(0x41, 2, audio_info_t)
#undef AUDIO_DRAIN
#define AUDIO_DRAIN     _IO(0x41, 3)
#undef AUDIO_GETDEV
#define AUDIO_GETDEV    _IOR(0x41, 4, int)

/* from stropts.h S = 0x53*/
#undef I_FLUSH
#define I_FLUSH         _IO(0x53,05)
/******** END OF FIX SUN **********************************/
#endif

#ifndef AUDIO_ENCODING_LINEAR
#define AUDIO_ENCODING_LINEAR (3)
#endif
#ifndef AUDIO_ENCODING_ULAW
#define AUDIO_ENCODING_ULAW (3)
#endif
#ifndef AUDIO_DEV_UNKNOWN
#define AUDIO_DEV_UNKNOWN (0)
#endif
#ifndef AUDIO_DEV_AMD
#define AUDIO_DEV_AMD (1)
#endif
#ifndef AUDIO_SPEAKER
#define AUDIO_SPEAKER 0x01
#endif
#ifndef AUDIO_HEADPHONE
#define AUDIO_HEADPHONE 0x02
#endif
#ifndef AUDIO_LINE_OUT
#define AUDIO_LINE_OUT 0x04
#endif
#ifndef AUDIO_MIN_GAIN
#define AUDIO_MIN_GAIN (0)
#endif
#ifndef AUDIO_MAX_GAIN
#define AUDIO_MAX_GAIN (255)
#endif


void  Sparc_Audio_Init();
void  Sparc_Audio_Kill();
void  Sparc_Audio_Off();
void  Sparc_Audio_Prep();
void  Sparc_Audio_On();
void  Sparc_Adjust_Volume();
xaULONG Sparc_Closest_Freq();
void Sparc_Set_Output_Port();
void Sparc_Speaker_Toggle();
void Sparc_Headphone_Toggle();

#define SPARC_MAX_VOL AUDIO_MAX_GAIN
#define SPARC_MIN_VOL AUDIO_MIN_GAIN

static int devAudio;
static audio_info_t audio_info;


/********** XA_Audio_Setup **********************
 * 
 * Also defines Sparc Specific variables.
 *
 *****/
void XA_Audio_Setup()
{

  XA_Audio_Init		= Sparc_Audio_Init;
  XA_Audio_Kill		= Sparc_Audio_Kill;
  XA_Audio_Off		= Sparc_Audio_Off;
  XA_Audio_Prep		= Sparc_Audio_Prep;
  XA_Audio_On		= Sparc_Audio_On;
  XA_Closest_Freq	= Sparc_Closest_Freq;
  XA_Set_Output_Port	= Sparc_Set_Output_Port;
  XA_Speaker_Tog	= Sparc_Speaker_Toggle;
  XA_Headphone_Tog	= Sparc_Headphone_Toggle;
  XA_LineOut_Tog	= Sparc_Headphone_Toggle;
  XA_Adjust_Volume	= Sparc_Adjust_Volume;

  xa_snd_cur = 0;
  xa_audio_present = XA_AUDIO_UNK;
  xa_audio_status  = XA_AUDIO_STOPPED;
  xa_audio_ring_size  = 8;
}


/********** Sparc_Audio_Init **********************
 * Open /dev/audio and /dev/audioctl for Sparc's.
 *
 *****/
void Sparc_Audio_Init()
{ int ret;
#ifdef SVR4	/* was SOLARIS */
  audio_device_t type;
#else
  int type;
#endif
  DEBUG_LEVEL2 fprintf(stderr,"Sparc_Audio_Init\n");
  if (xa_audio_present != XA_AUDIO_UNK) return;
  devAudio = open("/dev/audio", O_WRONLY | O_NDELAY);
  if (devAudio == -1)
  {
    if (errno == EBUSY) fprintf(stderr,"Audio_Init: Audio device is busy. - ");
    else fprintf(stderr,"Audio_Init: Error %x opening audio device. - ",errno);
    fprintf(stderr,"Will continue without audio\n");
    xa_audio_present = XA_AUDIO_ERR;
    return;
  }
  ret = ioctl(devAudio, AUDIO_GETDEV, &type);
  /* POD NOTE: Sparc 5 has new audio device (CS4231) */
#ifdef SVR4	/* was SOLARIS */
  if ( (   (strcmp(type.name, "SUNW,dbri"))   /* Not DBRI (SS10's) */
        && (strcmp(type.name, "SUNW,CS4231")) /* and not CS4231 (SS5's) */
        && (strcmp(type.name, "SUNW,sb16")))  /* and SoundBlaster 16 */
      || ret) /* or ioctrl failed */
#else
  if (ret || (type==AUDIO_DEV_UNKNOWN) || (type==AUDIO_DEV_AMD) )
#endif
  { /* SPARC AU AUDIO */
    DEBUG_LEVEL1 fprintf(stderr,"SPARC AMD AUDIO\n");
    xa_audio_hard_type  = XA_AUDIO_SUN_AU;
    xa_audio_hard_freq  = 8000;
    xa_audio_hard_buff  = XA_HARD_BUFF_1K;             /* default buffer size */
    xa_audio_hard_bps   = 1;
    xa_audio_hard_chans = 1;
    Gen_Signed_2_uLaw();
  }
  else /* DBRI or CS4231 or SB16 */
  {
    DEBUG_LEVEL1 fprintf(stderr,"SPARC DBRI or CS4231 or SB16 AUDIO\n");
#ifdef SVR4
#ifdef _LITTLE_ENDIAN
    xa_audio_hard_type  = XA_AUDIO_SIGNED_2ML;
#else
    xa_audio_hard_type  = XA_AUDIO_SIGNED_2MB; /* default to this */
#endif
#else
    xa_audio_hard_type  = XA_AUDIO_SIGNED_2MB;
#endif
    xa_audio_hard_freq  = 11025;
    xa_audio_hard_buff  = XA_HARD_BUFF_1K;             /* default buffer size */
    xa_audio_hard_bps   = 2;
    xa_audio_hard_chans = 1;
    AUDIO_INITINFO(&audio_info);
#ifdef SVR4	/* was SOLARIS */
/* POD-NOTE: Does this necessarily have to be 1024 (what the upper/lower
 * limits???)
 */
    audio_info.play.buffer_size = XA_HARD_BUFF_1K;
    xa_audio_hard_buff  = XA_HARD_BUFF_1K;   /* default buffer size */
#endif

	/* POD NOTE: this is currently used for testing purposes */
    switch(xa_kludge900_aud)
    {
      case 900:
fprintf(stderr,"Sun Audio: uLAW\n");
	xa_audio_hard_type  = XA_AUDIO_SUN_AU;
	xa_audio_hard_freq  = 8000;
	xa_audio_hard_bps   = 1;
	audio_info.play.sample_rate = 8000;
	audio_info.play.channels = 1;
	xa_audio_hard_bps   = 1;
	audio_info.play.encoding = AUDIO_ENCODING_ULAW;
	Gen_Signed_2_uLaw();
	break;
      case 901:
fprintf(stderr,"Sun Audio: 8 bit PCM\n");
	audio_info.play.sample_rate = 11025;
	audio_info.play.precision = 8;
	audio_info.play.channels = 1;
	audio_info.play.encoding = AUDIO_ENCODING_LINEAR;
	break;
      case 0:
      default:
	audio_info.play.sample_rate = 11025;
	audio_info.play.precision = 16;
	audio_info.play.channels = 1;
	audio_info.play.encoding = AUDIO_ENCODING_LINEAR;
	break;
    }

    ret = ioctl(devAudio, AUDIO_SETINFO, &audio_info);
    if (ret)
    {
      fprintf(stderr,"AUDIO BRI FATAL ERROR %d\n",errno);
      xa_audio_present = XA_AUDIO_ERR;
      return;
    }
  }
  xa_interval_id = 0;
  xa_audio_present = XA_AUDIO_OK;
  DEBUG_LEVEL2 fprintf(stderr,"   success \n");
  Init_Audio_Ring(xa_audio_ring_size,
			(XA_AUDIO_MAX_RING_BUFF * xa_audio_hard_bps) );
}

/********** Sparc_Audio_Kill **********************
 * Close /dev/audio and /dev/audioctl.
 *
 *****/
void Sparc_Audio_Kill()
{ 
  /* TURN AUDIO OFF */
  Sparc_Audio_Off(0);
  xa_audio_present = XA_AUDIO_UNK;
  /* SHUT THINGS DOWN  */
  close(devAudio);
  Kill_Audio_Ring();
}

/********** Sparc_Audio_Off **********************
 * Stop Audio Stream
 *
 *****/
void Sparc_Audio_Off(flag)
xaULONG flag;
{ long ret;

  if (xa_audio_status != XA_AUDIO_STARTED) return;

  /* SET FLAG TO STOP OUTPUT ROUTINE */
  xa_audio_status = XA_AUDIO_STOPPED;

  /* TURN OFF SOUND ??? */
  Sparc_Adjust_Volume(XA_AUDIO_MINVOL);

  /* FLUSH AUDIO DEVICE */
  ret = ioctl(devAudio, I_FLUSH, FLUSHW);
  if (ret == -1) fprintf(stderr,"Sparc Audio: off flush err %d\n",errno);

  xa_time_audio = -1;
  xa_audio_flushed = 0;

  /* FLUSH AUDIO DEVICE AGAIN */
  ret = ioctl(devAudio, I_FLUSH, FLUSHW);
  if (ret == -1) fprintf(stderr,"Sparc Audio: off flush err %d\n",errno);

  /* RESTORE ORIGINAL VOLUME */
  if (XAAUD->mute != xaTRUE) Sparc_Adjust_Volume(XAAUD->volume);
}

/********** Sparc_Audio_Prep **********************
 * Turn On Audio Stream.
 *
 *****/
void Sparc_Audio_Prep()
{ 
  DEBUG_LEVEL2 
  {
    fprintf(stderr,"Sparc_Audio_Prep \n");
  }
  if (xa_audio_status == XA_AUDIO_STARTED) return;
  else if (xa_audio_present != XA_AUDIO_OK) return;
  else if (xa_snd_cur)
  { int ret;

    /* CHANGE FREQUENCY IF NEEDED */
    if (xa_audio_hard_freq != xa_snd_cur->hfreq)
    { audio_info_t a_info;
      AUDIO_INITINFO(&a_info);
      a_info.play.sample_rate = xa_snd_cur->hfreq;
      ret = ioctl(devAudio, AUDIO_SETINFO, &a_info);
      if (ret == -1) fprintf(stderr,"audio setfreq: freq %x errno %d\n",
						xa_snd_cur->hfreq, errno);
      xa_audio_hard_freq = xa_snd_cur->hfreq;
    }

    /* xa_snd_cur gets changes in Update_Ring() */
    xa_out_time = 500;  /* keep audio fed 500ms ahead of video */
    xa_out_init = xa_audio_ring_size - 1;
    xa_interval_time = xa_snd_cur->ch_time / XAAUD->divtest;
    if (xa_interval_time == 0) xa_interval_time = 1;

DEBUG_LEVEL1 fprintf(stderr,"ch_time %d out_time %d int time %d \n",xa_snd_cur->ch_time,xa_out_time,xa_interval_time);

    XA_Flush_Ring();
    XA_Update_Ring(1000);
    xa_audio_status = XA_AUDIO_PREPPED;
  }
}

/****-------------------------------------------------------------------****
 *
 ****-------------------------------------------------------------------****/
void Sparc_Audio_On()
{ 
  DEBUG_LEVEL2 fprintf(stderr,"Sparc_Audio_On \n");

  if (   (xa_snd_cur)
      && (xa_audio_present == XA_AUDIO_OK)
      && (xa_audio_status == XA_AUDIO_PREPPED) )
  { 
    xa_audio_status = XA_AUDIO_STARTED;
    xa_time_now = XA_Read_AV_Time();  /* get new time */
    New_Merged_Audio_Output();
  }
}


/********** Sparc_Closest_Freq **********************************************
 *
 * Global Variable Affect:
 *   xaULONG xa_audio_hard_buff		must set but not larger than
 *					XA_AUDIO_MAX_RING_BUF size
 ****************************************************************************/
xaULONG Sparc_Closest_Freq(ifreq)
xaLONG ifreq;
{

/* POD
fprintf(stderr,"Sparc_Closest_Freq: hardtype %x freq %d\n",
			xa_audio_hard_type,ifreq);
*/

  if (xa_audio_hard_type==XA_AUDIO_SIGNED_2MB)
  { static int valid[] = { 8000, 9600, 11025, 16000, 18900, 22050, 32000,
			      37800, 44100, 48000, 0};
    xaLONG i = 0;
    xaLONG best = 8000;

    xa_audio_hard_buff = XA_HARD_BUFF_1K;
    while(valid[i])
    { 
      if (xaABS(valid[i] - ifreq) < xaABS(best - ifreq)) best = valid[i];
      i++;
    }
    return(best);
  }
  else return(8000);
}


/* Eventually merge everything to one */
void Sparc_Set_Output_Port(aud_ports)
xaULONG aud_ports;
{ audio_info_t a_info;
  xaLONG ret;
  xaULONG sparc_ports = 0;
  if (aud_ports & XA_AUDIO_PORT_INT)  sparc_ports |= AUDIO_SPEAKER;
  if (aud_ports & XA_AUDIO_PORT_HEAD) sparc_ports |= AUDIO_HEADPHONE;
  if (aud_ports & XA_AUDIO_PORT_EXT)  sparc_ports |= AUDIO_LINE_OUT;
  AUDIO_INITINFO(&a_info);
  a_info.play.port = sparc_ports;
  ret = ioctl(devAudio, AUDIO_SETINFO, &a_info);
  if (ret < 0) fprintf(stderr,"Audio: couldn't set speaker port %d\n",errno);
}

/************* Sparc_Speaker_Toggle *****************************************
 *
 * flag = 0  turn speaker off
 * flag = 1  turn speaker on
 * flag = 2  toggle speaker
 ****************************************************************************/
void Sparc_Speaker_Toggle(flag)
xaULONG flag;
{ 
  switch(flag)
  {
    case  0: XAAUD->port &= ~XA_AUDIO_PORT_INT; break;
    case  1: XAAUD->port |=  XA_AUDIO_PORT_INT; break;
    default:  /* mutually exclusive set for now - never turn off */
    { if ( !(XAAUD->port & XA_AUDIO_PORT_INT)) 
		XAAUD->port = XA_AUDIO_PORT_INT;
    }
  }
  Sparc_Set_Output_Port(XAAUD->port);
}

/************* Sparc_Headphone_Toggle *****************************************
 *
 * flag = 0  turn headphones off
 * flag = 1  turn headphones on
 * flag = 2  toggle headphones
 ****************************************************************************/
void Sparc_Headphone_Toggle(flag)
xaULONG flag;
{ 
  switch(flag)
  {
    case  0: XAAUD->port &= ~XA_AUDIO_PORT_HEAD; break;
    case  1: XAAUD->port |=  XA_AUDIO_PORT_HEAD; break;
    default:  /* mutually exclusive set for now - never turn off */
    { if ( !(XAAUD->port & XA_AUDIO_PORT_HEAD)) 
		XAAUD->port = XA_AUDIO_PORT_HEAD;
    }
  }
  Sparc_Set_Output_Port(XAAUD->port);
}


/********** Sparc_Adjust_Volume **********************
 * Routine for Adjusting Volume on a Sparc
 *
 * Volume is in the range [0,XA_AUDIO_MAXVOL]
 ****************************************************************************/
void Sparc_Adjust_Volume(volume)
xaULONG volume;
{ audio_info_t a_info;

  AUDIO_INITINFO(&a_info);
  a_info.play.gain = SPARC_MIN_VOL +
	((volume * (SPARC_MAX_VOL - SPARC_MIN_VOL)) / XA_AUDIO_MAXVOL);
  if (a_info.play.gain > SPARC_MAX_VOL) a_info.play.gain = SPARC_MAX_VOL;
  ioctl(devAudio, AUDIO_SETINFO, &a_info);
}
#endif
/****************************************************************************/
/******************* END OF SPARC SPECIFIC ROUTINES *********************/
/****************************************************************************/

/****************************************************************************/
/**************** S/6000  SPECIFIC ROUTINES *********************************/
/****************************************************************************/
#ifdef XA_AIX_AUDIO

void  AIX_Audio_Init();
void  AIX_Audio_Kill();
void  AIX_Audio_Off();
void  AIX_Audio_Prep();
void  AIX_Audio_On();

xaULONG AIX_Closest_Freq();
void  AIX_Speaker_Tog();
void  AIX_Headphone_Tog();
void  AIX_LineOut_Tog();
void  AIX_Adjust_Volume();


static int     devAudio;
static xaULONG   aix_audio_ports;
extern char  * xa_audio_device;

/*----------------------------------------------------------------------------*/
/*                                                                            */
/*----------------------------------------------------------------------------*/
void XA_Audio_Setup ( )
{
  XA_Audio_Init		= AIX_Audio_Init;
  XA_Audio_Kill		= AIX_Audio_Kill;
  XA_Audio_Off		= AIX_Audio_Off;
  XA_Audio_Prep		= AIX_Audio_Prep;
  XA_Audio_On		= AIX_Audio_On;
  XA_Closest_Freq	= AIX_Closest_Freq;
  XA_Speaker_Tog	= AIX_Speaker_Tog;
  XA_Headphone_Tog	= AIX_Headphone_Tog;
  XA_LineOut_Tog	= AIX_LineOut_Tog;
  XA_Adjust_Volume	= AIX_Adjust_Volume;

  xa_snd_cur = 0;
  xa_audio_present = XA_AUDIO_UNK;
  xa_audio_status = XA_AUDIO_STOPPED;
  xa_audio_ring_size = 8;
}


/*----------------------------------------------------------------------------*/
/*                                                                            */
/*----------------------------------------------------------------------------*/
void AIX_Audio_Init ( )
{
  char         * devname;
  audio_init     init;
  audio_control  control;
  audio_change   change;
  int            rc;

  if ( xa_audio_present != XA_AUDIO_UNK )
  {
    return;
  }

  devname = xa_audio_device;
  if ( ( devAudio = open ( devname, O_WRONLY | O_NDELAY ) ) < 0 )
  {
    fprintf ( stderr, "AIX_Audio_Init: Open failed for '%s', errno = %d\n",
              devname, errno );
    xa_audio_present = XA_AUDIO_ERR;
    return;
  }

  /* Set the codec to some default values (the same ones the sun has   */
  /* seem as good as any)                                              */

  xa_audio_hard_type  = XA_AUDIO_SIGNED_2MB;
  xa_audio_hard_freq  = 11025;
  xa_audio_hard_buff  = XA_HARD_BUFF_1K;
  xa_audio_hard_bps   = 2;
  xa_audio_hard_chans = 1;

  memset ( & init, '\0', sizeof ( init ) );

  init.srate =           11025;
  init.mode =            PCM;
  init.operation =       PLAY;
  init.channels =        1;
  init.flags =           BIG_ENDIAN | TWOS_COMPLEMENT;
  init.bits_per_sample = 16;
  init.bsize =           AUDIO_IGNORE;

  if ( ( rc = ioctl ( devAudio, AUDIO_INIT, & init ) ) < 0 )
  {
    fprintf ( stderr, "AIX_Audio_Init: AUDIO_INIT failed, errno = %d\n",
              errno );
  }


  memset ( & control, '\0', sizeof ( control ) );
  memset ( & change, '\0', sizeof ( change ) );

  aix_audio_ports = EXTERNAL_SPEAKER | INTERNAL_SPEAKER | OUTPUT_1;
  aix_audio_ports = OUTPUT_1;

  change.balance       = 0x3fff0000;
  change.balance_delay = 0;

  change.volume        = ( long ) ( 0x7fff << 16 );
  change.volume_delay  = 0;

  change.input         = AUDIO_IGNORE;
  change.output        = aix_audio_ports;
  change.monitor       = AUDIO_IGNORE;
  change.dev_info      = ( char * ) NULL;

  control.ioctl_request = AUDIO_CHANGE;
  control.position =      0;
  control.request_info =  ( char * ) & change;

  if ( ( rc = ioctl ( devAudio, AUDIO_CONTROL, & control ) ) < 0 )
  {
    fprintf ( stderr, "AIX_Audio_Init: AUDIO_CONTROL failed, errno = %d\n",
              errno );
  }

  xa_interval_id = 0;
  xa_audio_present = XA_AUDIO_OK;
  Init_Audio_Ring ( xa_audio_ring_size,
                    ( XA_AUDIO_MAX_RING_BUFF * xa_audio_hard_bps ) );
}


/*----------------------------------------------------------------------------*/
/*                                                                            */
/*----------------------------------------------------------------------------*/
void AIX_Audio_Kill ( )
{
  AIX_Audio_Off ( 0 );
  xa_audio_present = XA_AUDIO_UNK;
  close ( devAudio );
  Kill_Audio_Ring ( );
}


/*----------------------------------------------------------------------------*/
/*                                                                            */
/*----------------------------------------------------------------------------*/
void AIX_Audio_Off ( )
{
  audio_control control;
  int           rc;

  if ( xa_audio_status != XA_AUDIO_STARTED )
  {
    return;
  }

  xa_audio_status = XA_AUDIO_STOPPED;

  memset ( & control, '\0', sizeof ( control ) );

  control.ioctl_request = AUDIO_STOP;
  control.request_info  = NULL;
  control.position      = 0;

  if ( ( rc = ioctl ( devAudio, AUDIO_CONTROL, & control ) ) < 0 )
  {
    fprintf ( stderr, "AIX_Audio_Off: AUDIO_STOP failed, errno = %d\n", errno );
  }

#ifdef SOME_DAY
  if ( ( rc = ioctl ( devAudio, AUDIO_WAIT, NULL ) ) != 0 )
  {
    fprintf ( stderr, "AIX_Audio_Off: AUDIO_WAIT failed, errno = %d\n", errno );
  }
#endif

  xa_time_audio = -1;
  xa_audio_flushed = 0;
}


/*----------------------------------------------------------------------------*/
/*                                                                            */
/*----------------------------------------------------------------------------*/
void AIX_Audio_Prep( )
{
  audio_control control;
  int           rc;

  if ( xa_audio_status == XA_AUDIO_STARTED ) return;
  if ( xa_audio_present != XA_AUDIO_OK ) return;
  if ( xa_snd_cur )
  {
    /* Change the frequency if needed */
    if ( xa_audio_hard_freq != xa_snd_cur->hfreq )
    {
      audio_init init;

      printf ( "AIX_Audio_Prep: setting frequency to %d\n", xa_snd_cur->hfreq );

      memset ( & init, '\0', sizeof ( init ) );

      init.srate           = xa_snd_cur->hfreq;
      init.mode            = PCM;
      init.operation       = PLAY;
      init.channels        = 1;
      init.flags           = BIG_ENDIAN | TWOS_COMPLEMENT;
      init.bits_per_sample = 16;
      init.bsize           = AUDIO_IGNORE;

      if ( ( rc = ioctl ( devAudio, AUDIO_INIT, & init ) ) < 0 )
      {
        fprintf ( stderr,
                  "AIX_Audio_Prep: AUDIO_INIT failed, rate = %d, errno = %d\n",
                  init.srate, errno );
      }

      xa_audio_hard_freq = xa_snd_cur->hfreq;
    }

    /* OLD xa_out_time = xa_snd_cur->ch_time * 4; */
    xa_out_time = 250;  /* keep audio fed 250ms ahead of video - may be 500*/
    xa_out_init = xa_audio_ring_size - 1;
    xa_interval_time = xa_snd_cur->ch_time / XAAUD->divtest;
    if ( xa_interval_time == 0 ) xa_interval_time = 1;

    XA_Flush_Ring();
    XA_Update_Ring(1000);

    /* POD: The following 8 lines need to be before Merged_Audio_Output now */
    memset ( & control, '\0', sizeof ( control ) );
    control.ioctl_request = AUDIO_START;
    control.request_info  = NULL;
    control.position      = 0;
    if ( ( rc = ioctl ( devAudio, AUDIO_CONTROL, & control ) ) < 0 )
    {
      fprintf(stderr,"AIX_Audio_Prep: AUDIO_START failed, errno = %d\n", errno );
    }
    xa_audio_status = XA_AUDIO_PREPPED;
  }
}


void AIX_Audio_On()
{
  if (   (xa_snd_cur)
      && (xa_audio_present == XA_AUDIO_OK)
      && (xa_audio_status == XA_AUDIO_PREPPED) )
  {
    xa_audio_status = XA_AUDIO_STARTED;
    xa_time_now = XA_Read_AV_Time();  /* get new time */
    New_Merged_Audio_Output();
  }
}


/*----------------------------------------------------------------------------*/
/*                                                                            */
/*----------------------------------------------------------------------------*/
xaULONG AIX_Closest_Freq ( ifreq )
xaLONG ifreq;
{
  static valid [ ] = {  8000,  9600, 11025, 16000, 18900,
                       22050, 32000, 37800, 44100, 48000, 0 };
  int  i = 0;
  xaLONG best = 8000;

  xa_audio_hard_buff = XA_HARD_BUFF_1K;
  while ( valid [ i ] )
  {
    if ( xaABS(valid[i] - ifreq) < xaABS(best - ifreq) )
    {
      best = valid [ i ];
    }
    i = i + 1;
  }
  return ( best );
}


/*----------------------------------------------------------------------------*/
/*                                                                            */
/*----------------------------------------------------------------------------*/
int AIX_Output_Change ( settings )
xaULONG settings;
{
  audio_control control;
  audio_change  change;
  int           rc;

  memset ( & control, '\0', sizeof ( control ) );
  memset ( & change, '\0', sizeof ( change ) );

  change.balance       = 0x3fff0000;
  change.balance_delay = 0;

  change.volume        = AUDIO_IGNORE;
  change.volume_delay  = AUDIO_IGNORE;

  change.input         = AUDIO_IGNORE;
  change.output        = settings;
  change.monitor       = AUDIO_IGNORE;
  change.dev_info      = ( char * ) NULL;

  control.ioctl_request = AUDIO_CHANGE;
  control.position      = 0;
  control.request_info  = ( char * ) & change;

  if ( ( rc = ioctl ( devAudio, AUDIO_CONTROL, & control ) ) < 0 )
  {
    fprintf ( stderr, "AIX_Output_Change: AUDIO_CONTROL failed, errno = %d\n",
              errno );
  }
  return ( rc );
}


/*----------------------------------------------------------------------------*/
/*                                                                            */
/*----------------------------------------------------------------------------*/
void AIX_Speaker_Tog ( flag )
xaULONG flag;
{
  int  rc;

  switch ( flag )
  {
    case 0:
         aix_audio_ports &= ~INTERNAL_SPEAKER;
         break;

    case 1:
         aix_audio_ports |= INTERNAL_SPEAKER;
         break;

    default:
         aix_audio_ports ^= INTERNAL_SPEAKER;
         break;
  }

  if ( ( rc = AIX_Output_Change ( aix_audio_ports ) ) < 0 )
  {
    fprintf ( stderr, 
	"AIX_Speaker_Tog: AUDIO_CONTROL failed, errno = %d\n", errno );
  }
}


/*----------------------------------------------------------------------------*/
/*                                                                            */
/*----------------------------------------------------------------------------*/
void AIX_Headphone_Tog ( flag )
xaULONG flag;
{
  int  rc;

  switch ( flag )
  {
    case 0:
         aix_audio_ports &= ~EXTERNAL_SPEAKER;
         break;

    case 1:
         aix_audio_ports |= EXTERNAL_SPEAKER;
         break;

    default:
         aix_audio_ports ^= EXTERNAL_SPEAKER;
         break;
  }

  if ( ( rc = AIX_Output_Change ( aix_audio_ports ) ) < 0 )
  {
    fprintf(stderr,
      "AIX_Headphone_Tog: AUDIO_CONTROL failed, errno = %d\n",errno);
  }
}


/*----------------------------------------------------------------------------*/
/*                                                                            */
/*----------------------------------------------------------------------------*/
void AIX_LineOut_Tog ( flag )
xaULONG flag;
{
  int  rc;

  switch ( flag )
  {
    case 0:
         aix_audio_ports &= ~OUTPUT_1;
         break;

    case 1:
         aix_audio_ports |= OUTPUT_1;
         break;

    default:
         aix_audio_ports ^= OUTPUT_1;
         break;
  }

  if ( ( rc = AIX_Output_Change ( aix_audio_ports ) ) < 0 )
  {
    fprintf ( stderr, "AIX_LineOut_Tog: AUDIO_CONTROL failed, errno = %d\n",
              errno );
  }
}

/*----------------------------------------------------------------------------*/
/*                                                                            */
/*----------------------------------------------------------------------------*/
void AIX_Adjust_Volume ( volume )
xaULONG volume;
{
  audio_control control;
  audio_change  change;
  float         percent;
  long          new_val;
  int           rc;
  float         base_vol = 0x7fff;

  memset ( & control, '\0', sizeof ( control ) );
  memset ( & change, '\0', sizeof ( change ) );

  if ( volume > XA_AUDIO_MAXVOL )
  {
    volume = XA_AUDIO_MAXVOL;
  }

  if ( volume < XA_AUDIO_MINVOL )
  {
    volume = XA_AUDIO_MINVOL;
  }

  percent = ( double ) volume / XA_AUDIO_MAXVOL;
  new_val = base_vol * percent;
  new_val = new_val << 16;

  change.balance       = AUDIO_IGNORE;
  change.balance_delay = AUDIO_IGNORE;

  change.volume        = new_val;
  change.volume_delay  = 0;

  change.input         = AUDIO_IGNORE;
  change.output        = AUDIO_IGNORE;
  change.monitor       = AUDIO_IGNORE;
  change.dev_info      = ( char * ) NULL;

  control.ioctl_request = AUDIO_CHANGE;
  control.position =      0;
  control.request_info =  ( char * ) & change;

  if ( ( rc = ioctl ( devAudio, AUDIO_CONTROL, & control ) ) < 0 )
  {
    fprintf ( stderr, "AIX_Audio_Init: AUDIO_CONTROL failed, errno = %d\n",
              errno );
  }

}
#endif

/****************************************************************************/
/**************** S/6000  SPECIFIC ROUTINES *********************************/
/****************************************************************************/



/****************************************************************************/
/**************** NEC EWS SPECIFIC ROUTINES *********************************/
/****************************************************************************/
#ifdef XA_EWS_AUDIO

/*
 * EWS port provided by Bob Phillips,
 *	bobp@syl.nj.nec.com
 * Heavily stolen from the Sparc port
 * Friday October 26, 1994
 */

xaULONG ews_audio_ports = AUOUT_SP;

void  EWS_Audio_Init();
void  EWS_Audio_Kill();
void  EWS_Audio_Off();
void  EWS_Audio_Prep();
void  EWS_Audio_On();
void  EWS_Adjust_Volume();
xaULONG EWS_Closest_Freq();
void EWS_Speaker_Toggle();
void EWS_Headphone_Toggle();

/* EWS audio output volume is controlled with attenuation, not gain.
 * The upshot is that the control values run from 62 to 0, with low
 * numbers representing louder volumes.
 */

static int devAudio;
static struct AU_Volume audio_vol;
static struct AU_Type audio_type;
static struct AU_Line audio_line;
static struct AU_Status audio_status;


/********** XA_Audio_Setup **********************
 * 
 * Also defines NEC Specific variables.
 *
 *****/
void XA_Audio_Setup()
{
  XA_Audio_Init		= EWS_Audio_Init;
  XA_Audio_Kill		= EWS_Audio_Kill;
  XA_Audio_Off		= EWS_Audio_Off;
  XA_Audio_Prep		= EWS_Audio_Prep;
  XA_Audio_On		= EWS_Audio_On;
  XA_Closest_Freq	= EWS_Closest_Freq;
  XA_Set_Output_Port	= (void *)(0);
  XA_Speaker_Tog	= EWS_Speaker_Toggle;
  XA_Headphone_Tog	= EWS_Headphone_Toggle;
  XA_LineOut_Tog	= EWS_Headphone_Toggle;
  XA_Adjust_Volume	= EWS_Adjust_Volume;


  xa_snd_cur = 0;
  xa_audio_present = XA_AUDIO_UNK;
  xa_audio_status  = XA_AUDIO_STOPPED;
  xa_audio_ring_size  = 8;
}


/********** EWS_Audio_Init **********************
 * Open /dev/audio/audio on EWS.
 *
 *****/
void EWS_Audio_Init()
{ int ret;
  int type;


  DEBUG_LEVEL2 fprintf(stderr,"EWS_Audio_Init\n");
/* The NEC EWS audio driver is very similar to the Sparc, but it does
 * not use an audio control device.  All ioctls are made directly to
 * the audio device.  Also, it does not support a wide variety of
 * ioctls.  In fact, it only supports 
 * AUIOC_[SG]ETTYPE, AUIOC_[SG]ETLINE, AUIOC_[SG]ETVOLUME, and AUIOC_STATUS.
 * It doesn't support FLUSH, or any of the other funky Sparc ioctls
 */
  if (xa_audio_present != XA_AUDIO_UNK) return;
  devAudio = open("/dev/audio/audio", O_WRONLY | O_NDELAY);
  if (devAudio == -1)
  {
    if (errno == EBUSY) fprintf(stderr,"Audio_Init: Audio device is busy. - ");
    else fprintf(stderr,"Audio_Init: Error opening audio device. - ");
    fprintf(stderr,"Will continue without audio\n");
    xa_audio_present = XA_AUDIO_ERR;
    return;
  }
  DEBUG_LEVEL1 fprintf(stderr,"NEC EWS AUDIO\n");
    xa_audio_hard_type  = XA_AUDIO_SIGNED_2MB;
    xa_audio_hard_freq  = AURATE11_0;
    xa_audio_hard_bps   = 2;
    xa_audio_hard_chans = 1;
    xa_audio_hard_buff  = XA_HARD_BUFF_1K;             /* default buffer size */

    audio_type.rate=AURATE11_0;
    audio_type.bit_type=AU_PCM16;
    audio_type.channel=AU_MONO;
    ret = ioctl(devAudio, AUIOC_SETTYPE, &audio_type);
    if (ret)
    {
      fprintf(stderr,"EWS: AUIOC_SETTYPE error %d\n",errno);
      xa_audio_present = XA_AUDIO_ERR;
      return;
    }

  xa_interval_id = 0;
  xa_audio_present = XA_AUDIO_OK;
  DEBUG_LEVEL2 fprintf(stderr,"   success \n");
  Init_Audio_Ring(xa_audio_ring_size,
			(XA_AUDIO_MAX_RING_BUFF * xa_audio_hard_bps) );
}

/********** EWS_Audio_Kill **********************
 * Close /dev/audio/audio
 *
 *****/
void EWS_Audio_Kill()
{ 
  /* TURN AUDIO OFF */
  EWS_Audio_Off(0);
  xa_audio_present = XA_AUDIO_UNK;
  /* SHUT THINGS DOWN  */
  close(devAudio);
  Kill_Audio_Ring();
}

/********** EWS_Audio_Off **********************
 * Stop Audio Stream
 *
 *****/
void EWS_Audio_Off(flag)
xaULONG flag;
{ long ret;

  DEBUG_LEVEL1 fprintf(stderr,"EWS_Audio_Off\n");
  if (xa_audio_status != XA_AUDIO_STARTED) return;

  /* SET FLAG TO TELL FREE RUNNING OUTPUT ROUTINE */
  xa_audio_status = XA_AUDIO_STOPPED;

  /* The Sparc port flushes here.  We cannot. So, just nuke volume.
   */
  /* TURN OFF SOUND ??? */
  EWS_Adjust_Volume(XA_AUDIO_MINVOL);


  xa_time_audio = -1;
  xa_audio_flushed = 0;

}

/********** EWS_Audio_Prep **********************
 * Turn On Audio Stream.
 *
 *****/
void EWS_Audio_Prep()
{ 
  DEBUG_LEVEL2 
    fprintf(stderr,"EWS_Audio_Prep \n");
  if (xa_audio_status == XA_AUDIO_STARTED) return;
  else if (xa_audio_present != XA_AUDIO_OK) return;
  else if (xa_snd_cur)
  { int ret;

    /* CHANGE FREQUENCY IF NEEDED */
    if (xa_audio_hard_freq != xa_snd_cur->hfreq)
    { 
      
      ret = ioctl(devAudio, AUIOC_GETTYPE, &audio_type);
      if (ret == -1) fprintf(stderr,"AUIOC_GETTYPE: errno %d\n",errno);
      audio_type.rate=EWS_Closest_Freq(xa_snd_cur->hfreq);
      ret = ioctl(devAudio, AUIOC_SETTYPE, &audio_type);
      if (ret == -1) fprintf(stderr,"AUIOC_SETTYPE: freq %x errno %d\n",
						xa_snd_cur->hfreq, errno);
      xa_audio_hard_freq = xa_snd_cur->hfreq;
    }
    if (XAAUD->mute != xaTRUE) EWS_Adjust_Volume(XAAUD->volume);

    /* xa_snd_cur gets changes in Update_Ring() */
    xa_out_time = 250; /* keep audio fed 250ms ahead of video */
    xa_out_init = xa_audio_ring_size - 1;
    xa_interval_time = xa_snd_cur->ch_time / XAAUD->divtest;
    if (xa_interval_time == 0) xa_interval_time = 1;

    XA_Flush_Ring();
    XA_Update_Ring(1000);
    xa_audio_status = XA_AUDIO_PREPPED;
  }
}

/****-------------------------------------------------------------------****
 *
 ****-------------------------------------------------------------------****/
void EWS_Audio_On()
{
  if (   (xa_snd_cur)
      && (xa_audio_present == XA_AUDIO_OK)
      && (xa_audio_status == XA_AUDIO_PREPPED) )
  { 
    xa_audio_status = XA_AUDIO_STARTED;
    xa_time_now = XA_Read_AV_Time();  /* get new time */
    New_Merged_Audio_Output();
  }
}



/********** EWS_Closest_Freq **********************************************
 *
 * Global Variable Affect:
 *   xaULONG xa_audio_hard_buff		must set but not larger than
 *					XA_AUDIO_MAX_RING_BUF size
 ****************************************************************************/
xaULONG EWS_Closest_Freq(ifreq)
xaLONG ifreq;
{
  static int valid[] = { AURATE5_5, AURATE6_6, AURATE8_0,
	AURATE9_6, AURATE11_0, AURATE16_0, AURATE18_9, AURATE22_1,
	AURATE27_4, AURATE32_0, AURATE33_1, AURATE37_8, AURATE44_1,
	AURATE48_0,0};
  xaLONG i = 0;
  xaLONG best = 8000;

  xa_audio_hard_buff = XA_HARD_BUFF_1K;
  while(valid[i])
  { 
    if (xaABS(valid[i] - ifreq) < xaABS(best - ifreq)) best = valid[i];
    i++;
  }
  if (valid[i])
  {
    if (best >= 25000) xa_audio_hard_buff = XA_HARD_BUFF_2K;
    return(best);
  }
  else return(AURATE8_0);
}

/************* EWS_Speaker_Toggle *****************************************
 *
 * flag = 0  turn speaker off
 * flag = 1  turn speaker on
 * flag = 2  toggle speaker
 ****************************************************************************/
void EWS_Speaker_Toggle(flag)
xaULONG flag;
{ 
  xaULONG old_ports = ews_audio_ports;
  xaLONG ret;

  switch(flag)
  {
    case  0: ews_audio_ports &= ~AUOUT_SP; break;
    case  1: ews_audio_ports |=  AUOUT_SP; break;
    default: ews_audio_ports ^=  AUOUT_SP; break;
  }
  audio_line.outline = ews_audio_ports;
  ret = ioctl(devAudio, AUIOC_SETLINE, &audio_line);
  if (ret == -1)
  {
    fprintf(stderr,"Audio: couldn't toggle speaker %d\n",errno);
    ews_audio_ports = old_ports;
  }
}

/************* EWS_Headphone_Toggle *****************************************
 *
 * flag = 0  turn headphones off
 * flag = 1  turn headphones on
 * flag = 2  toggle headphones
 ****************************************************************************/
void EWS_Headphone_Toggle(flag)
xaULONG flag;
{ 
  xaULONG old_ports = ews_audio_ports;
  xaLONG ret;
  switch(flag)
  {
    case  0: ews_audio_ports &= ~AUOUT_PHONE; break;
    case  1: ews_audio_ports |=  AUOUT_PHONE; break;
    default: ews_audio_ports ^=  AUOUT_PHONE; break;
  }
  audio_line.outline = ews_audio_ports;
  ret = ioctl(devAudio, AUIOC_SETLINE, &audio_line);
  if (ret == -1)
  {
    fprintf(stderr,"Audio: couldn't toggle headphone %d\n",errno);
    ews_audio_ports = old_ports;
  }
}


/********** EWS_Adjust_Volume **********************
 * Routine for Adjusting Volume on EWS
 *
 * Volume is in the range [0,XA_AUDIO_MAXVOL]
 * EWS Output is a measure of attenuation, so it ranges from [62,0]
 ****************************************************************************/
void EWS_Adjust_Volume(volume)
xaULONG volume;
{ 
  int vol;
  int ret;
  
  ret = ioctl(devAudio, AUIOC_GETVOLUME, &audio_vol);
  if (ret == -1)
  {
    fprintf(stderr,"Audio: couldn't get volume%d\n",errno);
    return;
  }

  vol= AUOUT_ATTEMAX-((volume*AUOUT_ATTEMAX)/XA_AUDIO_MAXVOL);
  audio_vol.out.left = audio_vol.out.right = vol;
  ret = ioctl(devAudio, AUIOC_SETVOLUME, &audio_vol);
  if (ret == -1)
  {
    fprintf(stderr,"Audio: couldn't set volume%d\n",errno);
  }

}

#endif
/****************************************************************************/
/******************* END OF NEC EWS SPECIFIC ROUTINES ***********************/
/****************************************************************************/

/****************************************************************************/
/**************** SONY SPECIFIC ROUTINES ***********************************/
/****************************************************************************/
#ifdef	XA_SONY_AUDIO
/*
 * SONY NEWS port provided by Kazushi Yoshida,
 *	30eem043@keyaki.cc.u-tokai.ac.jp
 * Heavily stolen from the Sparc port
 * Thu Jan 26, 1995
 *
 * SVR4/NEWS-OS 6.x port by Takashi Hagiwara, Sony Corporation
 * Add SB device check routine and some fixes
 *      hagiwara@sm.sony.co.jp
 */
void Sony_Audio_Init();
void Sony_Audio_Kill();
void Sony_Audio_Off();
void Sony_Audio_Prep();
void Sony_Audio_On();
void Sony_Adjust_Volume();
xaULONG Sony_Closest_Freq();
void Sony_Speaker_Toggle();
void Sony_Headphone_Toggle();

/*
 */
static int sony_audio_rate1[] =  /* NWS-5000,4000 */
	{ RATE8000, RATE9450, RATE11025, RATE12000, RATE16000,
	  RATE18900, RATE22050, RATE24000, RATE32000, RATE37800,
	  RATE44056, RATE44100, RATE48000, 0 } ;
 
static int sony_audio_rate2[] =  /* NWS-3200, 3400, 3700, 3800 */
	{ RATE8000, RATE9450, RATE18900, RATE37800, 0 } ;
 
static int sony_audio_rate3[] =  /* NWS-3100 */
	{ RATE8000, 0 } ;

int sony_audio_buf_len ;
static int devAudio;
static struct sbparam	audio_info ;
static int *sony_audio_rate;
static int sony_max_vol, sony_min_vol;


/********** XA_Audio_Setup **********************
*
* Also defines Sony Specific variables.
*
*****/
void XA_Audio_Setup()
{
	XA_Audio_Init		= Sony_Audio_Init;
	XA_Audio_Kill		= Sony_Audio_Kill;
	XA_Audio_Off		= Sony_Audio_Off;
	XA_Audio_Prep		= Sony_Audio_Prep;
	XA_Audio_On		= Sony_Audio_On;
	XA_Closest_Freq		= Sony_Closest_Freq;
	XA_Set_Output_Port	= (void *)(0);
	XA_Speaker_Tog		= Sony_Speaker_Toggle;
	XA_Headphone_Tog	= Sony_Headphone_Toggle;
	XA_LineOut_Tog		= Sony_Headphone_Toggle;
	XA_Adjust_Volume	= Sony_Adjust_Volume;


	xa_snd_cur = 0;
	xa_audio_present = XA_AUDIO_UNK;
	xa_audio_status  = XA_AUDIO_STOPPED;
	xa_audio_ring_size  = 8;
}

/********** Sony_Audio_Init **********************
* Open /dev/sb.
*
*****/
void Sony_Audio_Init()
{
	int	ret;
	int	type;
	int 	sbtype;

	DEBUG_LEVEL2 fprintf(stderr,"Sony_Audio_Init\n");

	if (xa_audio_present != XA_AUDIO_UNK) 
		return;
	devAudio = open("/dev/sb0", O_WRONLY | O_NDELAY);
	if (devAudio == -1) {
	  if (errno == EBUSY) 
		fprintf(stderr,"Audio_Init: Audio device is busy. - ");
	  else 
		fprintf(stderr,"Audio_Init: Error opening audio device. - ");
	  fprintf(stderr,"Will continue without audio\n");
	  xa_audio_present = XA_AUDIO_ERR;
	  return;
	}

	/* Under NEWS-OS 6.x SBIOCGETTYPE is supported by all machines. */
	/* However, NEWS-OS 4.x SBIOCGETYPE is supported by only NEWS 5000 */
	/* We should guess if SBUICGETTYPE would generate error,  */
	/* that machine  would be NEWS-3xxxx */ 

	if (ioctl(devAudio, SBIOCGETTYPE, (char *)&sbtype) < 0) {
#ifdef SVR4
		fprintf(stderr,"CANNOT GET SBTYPE ERROR %d\n",errno);
#endif 
		sbtype = SBTYPE_AIF2;	/* I guess so */	
	}
	switch(sbtype) {
		case SBTYPE_AIF2:
		case SBTYPE_AIF2_L:
		case SBTYPE_AIF2_E:
			sony_max_vol = 0;
			sony_min_vol = -32;
			sony_audio_rate = &sony_audio_rate2[0];
			break;
		case SBTYPE_AIF3:
			sony_max_vol = 8;
			sony_min_vol = -8;
			sony_audio_rate = &sony_audio_rate3[0];
			break;
		case SBTYPE_AIF5:
		case SBTYPE_AD1848:
		default:
			sony_max_vol = 16;
			sony_min_vol = -16;
			sony_audio_rate = &sony_audio_rate1[0];
			break;
	}

	DEBUG_LEVEL1 fprintf(stderr,"SONY AMD AUDIO\n");

	xa_audio_hard_type  = XA_AUDIO_SUN_AU;
	xa_audio_hard_freq  = 8000;
	xa_audio_hard_buff  = 1024;         /* default buffer size */
	xa_audio_hard_bps   = 1;
	xa_audio_hard_chans = 1;
	Gen_Signed_2_uLaw() ;

	audio_info.sb_mode    = LOGPCM ;
	audio_info.sb_format  = 0 ;		/* was BSZ128 */
	audio_info.sb_compress= MULAW ;
	audio_info.sb_rate    = RATE8000 ;
	audio_info.sb_channel = MONO ;
	audio_info.sb_bitwidth= RES8B ;
	audio_info.sb_emphasis= 0 ;		/* was EMPH_OFF */

	ret = ioctl(devAudio, SBIOCSETPARAM, &audio_info);
	if (ret) {
		fprintf(stderr,"AUDIO BRI FATAL ERROR %d\n",errno);
		TheEnd1("SONY AUDIO BRI FATAL ERROR\n");
	}
	ret = ioctl(devAudio, SBIOCGETPARAM, &audio_info);
	if (ret == -1)
		 fprintf(stderr,"Can't get audio_info %d\n",errno);
	xa_interval_id = 0;
	xa_audio_present = XA_AUDIO_OK;

	DEBUG_LEVEL2 fprintf(stderr,"success \n");

	Init_Audio_Ring(xa_audio_ring_size,
		(XA_AUDIO_MAX_RING_BUFF * xa_audio_hard_bps) );

	sony_audio_buf_len = 32 * 1024 ; /* 4 sec... */
}


/********** Sony_Audio_Kill **********************
* Close /dev/sb0.
*
*****/
void Sony_Audio_Kill()
{
  DEBUG_LEVEL1 fprintf(stderr,"Sony_Audio_Kill\n");
  /* TURN AUDIO OFF */
  Sony_Audio_Off(0);
  xa_audio_present = XA_AUDIO_UNK;
  /* SHUT THINGS DOWN  */
  close(devAudio);
  Kill_Audio_Ring();
}

/********** Sony_Audio_Off **********************
* Stop Audio Stream
*
*****/
void Sony_Audio_Off(flag)
xaULONG flag;
{ long ret;

  DEBUG_LEVEL1 fprintf(stderr,"Sony_Audio_Off\n");
  if (xa_audio_status != XA_AUDIO_STARTED) return;

  /* SET FLAG TO TELL FREE RUNNING OUTPUT ROUTINE */
  xa_audio_status = XA_AUDIO_STOPPED;

  /* TURN OFF SOUND ??? */ 
  Sony_Adjust_Volume(XA_AUDIO_MINVOL);

  /* FLUSH AUDIO DEVICE */
  ret = ioctl(devAudio, SBIOCFLUSH, 0);
  if (ret == -1) fprintf(stderr,"Sony Audio: off flush err %d\n",errno);

  xa_time_audio = -1;
  xa_audio_flushed = 0;

  /* OK... Wait for Sound device */
  ret = ioctl(devAudio, SBIOCWAIT, 0);

  /* RESTORE ORIGINAL VOLUME */
  if (XAAUD->mute != xaTRUE) Sony_Adjust_Volume(XAAUD->volume);
}

/********** Sony_Audio_Prep **********************
* Turn On Audio Stream.
*
*****/
void Sony_Audio_Prep()
{ int ret ;
  struct sbparam a_info ;

  DEBUG_LEVEL1 fprintf(stderr,"SONY AUDIO ON\n");

  if (xa_audio_status == XA_AUDIO_STARTED) return;
  else if (xa_audio_present != XA_AUDIO_OK) return;
  else if (xa_snd_cur)
  {
    /* CHANGE FREQUENCY IF NEEDED */
    if( xa_audio_hard_freq != xa_snd_cur->hfreq )
    {
      a_info.sb_mode    = LOGPCM ;
      a_info.sb_format  = 0 ;
      a_info.sb_compress= MULAW ;
      a_info.sb_rate    = xa_snd_cur->hfreq ;
      a_info.sb_channel = MONO ;
      a_info.sb_bitwidth= RES8B ;
      a_info.sb_emphasis= 0 ;
      ret=ioctl(devAudio, SBIOCSETPARAM, &a_info); 
      if( ret == -1 )fprintf(stderr,"audio set freq: freq %lf errno %d\n",
					xa_snd_cur->hfreq, errno);
      xa_audio_hard_freq = xa_snd_cur->hfreq;
    }

    /* xa_snd_cur gets changes in Update_Ring() */
    xa_out_time = 250; /* keep audio fed 250ms ahead of video - could be 500*/
    xa_out_init = xa_audio_ring_size - 1;
    xa_interval_time = xa_snd_cur->ch_time / XAAUD->divtest;
    if (xa_interval_time == 0) xa_interval_time = 1;

    XA_Flush_Ring();
    XA_Update_Ring(1000);
    xa_audio_status = XA_AUDIO_PREPPED;
  }
}

/****-------------------------------------------------------------------****
 *
 ****-------------------------------------------------------------------****/
void Sony_Audio_On()
{
  if (   (xa_snd_cur)
      && (xa_audio_present == XA_AUDIO_OK)
      && (xa_audio_status == XA_AUDIO_PREPPED) )
  { 
    xa_audio_status = XA_AUDIO_STARTED;
    xa_time_now = XA_Read_AV_Time();  /* get new time */
    New_Merged_Audio_Output();
  }
}

/********** Sony_Closest_Freq **********************************************
*
****************************************************************************/
xaULONG Sony_Closest_Freq(ifreq)
xaLONG	ifreq;
{ xaLONG i = 0 ;
  xaLONG best = 8000 ;

  xa_audio_hard_buff = XA_HARD_BUFF_1K;
  while(sony_audio_rate[i])
  {
    if (xaABS(sony_audio_rate[i] - ifreq) < xaABS(best - ifreq)) 
				best = sony_audio_rate[i];
    i++;
  }
  return(best);
}
/************* Sony_Speaker_Toggle *****************************************
*
* flag = 0  turn speaker off
* flag = 1  turn speaker on
* flag = 2  toggle speaker
****************************************************************************/
void Sony_Speaker_Toggle(flag)
xaULONG flag;
{
	 /* do nothing */return ;
}

/************* Sony_Headphone_Toggle *****************************************
*
* flag = 0  turn headphones off
* flag = 1  turn headphones on
* flag = 2  toggle headphones
****************************************************************************/
void Sony_Headphone_Toggle(flag)
xaULONG flag ;
{
	 /* do nothing */return ;
}
/********** Sony_Adjust_Volume **********************/
void Sony_Adjust_Volume(volume)
xaULONG volume;
{ 
  struct sblevel 	gain;
  int			mflg;
  int			o_level;
  mflg = MUTE_OFF;
  ioctl(devAudio, SBIOCMUTE, &mflg);
  o_level = sony_min_vol +
                 ((volume * (sony_max_vol - sony_min_vol)) / XA_AUDIO_MAXVOL);
  if (o_level > sony_max_vol)	o_level = sony_max_vol;
  if (o_level < sony_min_vol)	o_level = sony_min_vol;
 
  if (o_level == sony_min_vol) 
  {
    mflg = MUTE_ON;
    ioctl(devAudio, SBIOCMUTE, &mflg);
  }
  gain.sb_right = gain.sb_left = (o_level << 16);
  ioctl(devAudio, SBIOCSETOUTLVL, &gain);
}
#endif
/****************************************************************************/
/******************* END OF SONY SPECIFIC ROUTINES *********************/
/****************************************************************************/


/****************************************************************************/
/**************** LINUX SPECIFIC ROUTINES *******************************/
/****************************************************************************/
#ifdef XA_LINUX_AUDIO


void Linux_Audio_Init();
void Linux_Audio_Kill();
void Linux_Audio_Off();
void Linux_Audio_Prep();
void Linux_Audio_On();
xaULONG Linux_Closest_Freq();
void Linux_Speaker_Toggle();
void Linux_Headphone_Toggle();
void Linux_Adjust_Volume();
xaULONG Linux_Read_Volume();

int linux_vol_chan;

#define LINUX_MAX_VOL (100)
#define LINUX_MIN_VOL (0)

int devAudio;
int devMixer;

/********** XA_Audio_Setup **********************
 * Sets up Linux specific Routines
 *****/
void XA_Audio_Setup()
{
  XA_Audio_Init		= Linux_Audio_Init;
  XA_Audio_Kill		= Linux_Audio_Kill;
  XA_Audio_Off		= Linux_Audio_Off;
  XA_Audio_Prep		= Linux_Audio_Prep;
  XA_Audio_On		= Linux_Audio_On;
  XA_Closest_Freq	= Linux_Closest_Freq;
  XA_Set_Output_Port	= (void *)(0);
  XA_Speaker_Tog	= Linux_Speaker_Toggle;
  XA_Headphone_Tog	= Linux_Headphone_Toggle;
  XA_LineOut_Tog	= Linux_Headphone_Toggle;
  XA_Adjust_Volume	= Linux_Adjust_Volume;

  xa_snd_cur = 0;
  xa_audio_present = XA_AUDIO_UNK;
  xa_audio_status  = XA_AUDIO_STOPPED;
  xa_audio_ring_size  = 8;
  linux_vol_chan = SOUND_MIXER_VOLUME;  /* default */
}

/********** Linux_Audio_Init **********************
 * Init Linux Audio, init variables, open Audio Devices.
 *****/
void Linux_Audio_Init()
{ int ret,tmp;
  DEBUG_LEVEL2 fprintf(stderr,"Linux_Audio_Init\n");

  if (xa_audio_present != XA_AUDIO_UNK) return;

  devAudio = open(_FILE_DSP, O_WRONLY | O_NDELAY, 0);
  if (devAudio == -1)
  { fprintf(stderr,"Can't Open %s device\n",_FILE_DSP);
    xa_audio_present = XA_AUDIO_ERR;
    return;
  }

#ifdef SNDCTL_DSP_SETFRAGMENT
  tmp = 0x0008000b; /* 8 2048 byte buffers */
#ifdef XA_LINUX_OLDER_SND
  ret = ioctl(devAudio, SNDCTL_DSP_SETFRAGMENT, tmp);
#else
  ret = ioctl(devAudio, SNDCTL_DSP_SETFRAGMENT, &tmp);
#endif
#else
  ret = 0;
#endif
  if (ret == -1) fprintf(stderr,"Linux Audio: Error setting fragment size\n");

  ioctl(devAudio, SNDCTL_DSP_SYNC, NULL);
  /* SETUP SAMPLE SIZE */
  if (xa_kludge900_aud == 900) tmp = 8;  /* POD testing purposes */
  else tmp = 16;
#ifdef SNDCTL_DSP_SAMPLESIZE
#ifdef XA_LINUX_OLDER_SND
  ret = ioctl(devAudio, SNDCTL_DSP_SAMPLESIZE, tmp);
#else
  ret = ioctl(devAudio, SNDCTL_DSP_SAMPLESIZE, &tmp);
#endif
#else
  ret = -1;
#endif
  if ((ret == -1) || (tmp == 8)) xa_audio_hard_bps = 1;
  else xa_audio_hard_bps = 2;

/* TESTING
fprintf(stderr,"audio hard bps %d\n",xa_audio_hard_bps);
*/

  /* SETUP Mono/Stereo */
  tmp = 0;  /* mono(0) stereo(1) */ 
#ifdef XA_LINUX_OLDER_SND
  ret = ioctl(devAudio, SNDCTL_DSP_STEREO, tmp);
#else
  ret = ioctl(devAudio, SNDCTL_DSP_STEREO, &tmp);
#endif
  if (ret == -1) fprintf(stderr,"Linux Audio: Error setting mono\n");
  xa_audio_hard_chans = 1;

  xa_audio_hard_freq  = 11025;  /* 22050 and sometimes 44100 */
  xa_audio_hard_buff  = XA_HARD_BUFF_1K;

#ifdef SNDCTL_DSP_GETBLKSIZE
#ifdef XA_LINUX_OLDER_SND
  ret = ioctl(devAudio, SNDCTL_DSP_GETBLKSIZE, 0);
#else
  ret = ioctl(devAudio, SNDCTL_DSP_GETBLKSIZE, &tmp);
#endif
#else
  ret = 0;
#endif
  if (ret == -1) fprintf(stderr,"Linux Audio: Error getting buf size\n");
  /* POD NOTE: should probably do something with this value. :^) */
  /* Maybe XA_AUDIO_MAX_RING_BUFF should be a variable */
  
  ioctl(devAudio, SNDCTL_DSP_SYNC, NULL);

  devMixer = open(_FILE_MIXER,  O_RDONLY | O_NDELAY, 0);
  /* Mixer only present on SB Pro's and above */
  /* if not present then it's set to -1 and ignored later */
  /* THOUGHT: what about doing mixer ioctl to the /dev/dsp device??? */
  if (devMixer < 0) devMixer = devAudio;
   /* determine what volume settings exist */
  { int devices;
#ifdef XA_LINUX_OLDER_SND
    devices = ioctl(devMixer, SOUND_MIXER_READ_DEVMASK, 0 );
    if (devices == -1) devices = 0;
#else
    ret = ioctl(devMixer, SOUND_MIXER_READ_DEVMASK, &devices);
    if (ret == -1) devices = 0;
#endif
    if (devices & (1 << SOUND_MIXER_PCM)) 
		linux_vol_chan = SOUND_MIXER_PCM;
    else	linux_vol_chan = SOUND_MIXER_VOLUME;
  }

/*
  XAAUD->volume = Linux_Read_Volume();
*/
 
  if (xa_audio_hard_bps == 1) xa_audio_hard_type  = XA_AUDIO_LINEAR_1M;
  else		xa_audio_hard_type  = XA_AUDIO_SIGNED_2ML;

  xa_interval_id = 0;
  xa_audio_present = XA_AUDIO_OK;
  DEBUG_LEVEL2 fprintf(stderr,"   success \n");
  Init_Audio_Ring(xa_audio_ring_size,
			(XA_AUDIO_MAX_RING_BUFF * xa_audio_hard_bps) );
}

/********** Linux_Audio_Kill *************************************************
 * Stop Linux Audio and Close Linux Audio Devices. Free up memory.
 *
 *****************************************************************************/
void Linux_Audio_Kill()
{
  Linux_Audio_Off(0);
  xa_audio_present = XA_AUDIO_UNK;
  if ( (devMixer != -1) && (devMixer != devAudio)) close(devMixer);
  close(devAudio);
  Kill_Audio_Ring();
}

/********** Linux_Audio_Off *************************************************
 * Stop Linux Audio
 *
 *****************************************************************************/
void Linux_Audio_Off(flag)
xaULONG flag;
{
  if (xa_audio_status != XA_AUDIO_STARTED) return;
  /* SET FLAG TO STOP OUTPUT ROUTINE */
  xa_audio_status = XA_AUDIO_STOPPED;

  ioctl(devAudio, SNDCTL_DSP_RESET, NULL);

  /* Linux_Adjust_Volume(XA_AUDIO_MINVOL); */
  /* ioctl(devAudio, SNDCTL_DSP_SYNC, NULL); */
  xa_time_audio = -1;
  xa_audio_flushed = 0;
  ioctl(devAudio, SNDCTL_DSP_SYNC, NULL);
  /* if (XAAUD->mute != xaTRUE) Linux_Adjust_Volume(XAAUD->volume); */

}

/********** Linux_Audio_Prep ***************************************************
 * Startup Linux Audio
 *
 *****************************************************************************/
void Linux_Audio_Prep()
{
  DEBUG_LEVEL2 fprintf(stderr,"Linux_Audio_Prep \n");
  if (xa_audio_status == XA_AUDIO_STARTED) return;
  else if (xa_audio_present != XA_AUDIO_OK) return;
  else if (xa_snd_cur)
  { int ret,tmp;
  

    Linux_Adjust_Volume(XA_AUDIO_MINVOL);
    ioctl(devAudio, SNDCTL_DSP_SYNC, NULL);

    /* CHANGE FREQUENCY */
    if (xa_audio_hard_freq != xa_snd_cur->hfreq)
    {
      tmp = xa_snd_cur->hfreq;
#ifdef XA_LINUX_OLDER_SND
      tmp = ioctl(devAudio, SNDCTL_DSP_SPEED, tmp);
#else
      ret = ioctl(devAudio, SNDCTL_DSP_SPEED, &tmp);
#endif
      if ((ret == -1) || (tmp != xa_snd_cur->hfreq))
	fprintf(stderr,"Linux_Audio: err setting freq %x\n"
						,xa_snd_cur->hfreq);
      xa_audio_hard_freq = xa_snd_cur->hfreq;
    }
    ioctl(devAudio, SNDCTL_DSP_SYNC, NULL);
    if (XAAUD->mute != xaTRUE) Linux_Adjust_Volume(XAAUD->volume);

    /* xa_snd_cur gets changes in Update_Ring() */
    xa_out_time = 250;  /* keep audio 250 ms ahead of video. could be 500ms */
    xa_out_init = xa_audio_ring_size - 1;
    xa_interval_time = xa_snd_cur->ch_time / XAAUD->divtest;
    if (xa_interval_time == 0) xa_interval_time = 1;

    XA_Flush_Ring();
    XA_Update_Ring(1000);
    xa_audio_status = XA_AUDIO_PREPPED;
  }
}

/****-------------------------------------------------------------------****
 *
 ****-------------------------------------------------------------------****/
void Linux_Audio_On()
{
  DEBUG_LEVEL2 fprintf(stderr,"Linux_Audio_On\n");
  if (   (xa_snd_cur)
      && (xa_audio_present == XA_AUDIO_OK)
      && (xa_audio_status == XA_AUDIO_PREPPED) )
  { 
    xa_audio_status = XA_AUDIO_STARTED;
    xa_time_now = XA_Read_AV_Time();  /* get new time */
    New_Merged_Audio_Output();
  }
}

/********** Linux_Closest_Freq **********************************************
 * Return closest support frequeny and set hard_buff.
 *
 *****************************************************************************/
xaULONG Linux_Closest_Freq(ifreq)
xaLONG ifreq;	/* incoming frequency */
{ static int valid[] = {11025, 22050, 44100, 0};
  xaLONG i = 0;
  xaLONG tmp_freq,ofreq = ifreq;
#ifndef XA_LINUX_OLDER_SND
  xaLONG ret;
#endif

   /* search up for closest frequency */
  while(valid[i])
  {
    if (xaABS(valid[i] - ifreq) < xaABS(ofreq - ifreq)) ofreq = valid[i];
    i++;
  }

  tmp_freq = ofreq;
#ifdef XA_LINUX_OLDER_SND
  tmp_freq = ioctl(devAudio, SNDCTL_DSP_SPEED, tmp_freq);
  if (tmp_freq == -1) tmp_freq = 0;
#else
  ret = ioctl(devAudio, SNDCTL_DSP_SPEED, &tmp_freq);
  if (ret == -1) tmp_freq = 0;
#endif
  if (tmp_freq) ofreq = tmp_freq;

  if (ofreq >= 25000) xa_audio_hard_buff = XA_HARD_BUFF_2K;
  else xa_audio_hard_buff = XA_HARD_BUFF_1K;
  return(ofreq);
}

/********** Linux_Speaker_Toggle **********************************************
 * Turn off/on/toggle Linux's Speaker(if possible)
 * flag= 0   1   2 
 *****************************************************************************/
void Linux_Speaker_Toggle(flag)
xaULONG flag;
{
  return;
}

/********** Linux_Headphone_Toggle ********************************************
 * Turn off/on/toggle Linux's Headphone(if possible)
 * flag= 0   1   2 
 *****************************************************************************/
void Linux_Headphone_Toggle(flag)
xaULONG flag;
{
  return;
}

/********** Linux_Adjust_Volume ***********************************************
 * Routine for Adjusting Volume on Linux
 *
 *****************************************************************************/
void Linux_Adjust_Volume(volume)
xaULONG volume;
{ xaULONG adj_volume;

  if (devMixer < 0) return;
  adj_volume = LINUX_MIN_VOL +
        ((volume * (LINUX_MAX_VOL - LINUX_MIN_VOL)) / XA_AUDIO_MAXVOL);
  if (adj_volume > LINUX_MAX_VOL) adj_volume = LINUX_MAX_VOL;
  adj_volume |= adj_volume << 8;	/* left channel | right channel */
#ifdef XA_LINUX_OLDER_SND
  ioctl(devMixer, MIXER_WRITE(linux_vol_chan), adj_volume);
#else
  ioctl(devMixer, MIXER_WRITE(linux_vol_chan), &adj_volume);
#endif
}

/********** Linux_Adjust_Volume ***********************************************
 * Routine for Adjusting Volume on Linux
 *
 *****************************************************************************/
xaULONG Linux_Read_Volume()
{ xaLONG ret,the_volume = 0;
  if (devMixer < 0) return(0);

#ifdef XA_LINUX_OLDER_SND
  the_volume = ioctl(devMixer, MIXER_READ(linux_vol_chan), 0);
  if (the_volume < 0) the_volume = 0;
#else
  ret = ioctl(devMixer, MIXER_READ(linux_vol_chan), &the_volume);
  if (ret < 0) the_volume = 0;
#endif

  the_volume =  (XA_AUDIO_MAXVOL * (the_volume - LINUX_MIN_VOL)) 
						/ (LINUX_MAX_VOL);

  fprintf(stderr,"volume = %d %d %d",the_volume,LINUX_MIN_VOL,LINUX_MAX_VOL);
  return(the_volume);
}
#endif
/****************************************************************************/
/******************* END OF LINUX SPECIFIC ROUTINES *********************/
/****************************************************************************/


/****************************************************************************/
/**************** SGI SPECIFIC ROUTINES *******************************/
/****************************************************************************/
#ifdef XA_SGI_AUDIO

void SGI_Audio_Init();
void SGI_Audio_Kill();
void SGI_Audio_Off();
void SGI_Audio_Prep();
void SGI_Audio_On();
xaULONG SGI_Closest_Freq();
void SGI_Speaker_Toggle();
void SGI_Headphone_Toggle();
void SGI_Adjust_Volume();

#define SGI_MAX_VOL (255)
#define SGI_MIN_VOL (0)

static ALport port;
static ALconfig conf;

/********** XA_Audio_Setup **********************
 * Sets up SGI specific Routines
 *****/
void XA_Audio_Setup()
{
  XA_Audio_Init		= SGI_Audio_Init;
  XA_Audio_Kill		= SGI_Audio_Kill;
  XA_Audio_Off		= SGI_Audio_Off;
  XA_Audio_Prep		= SGI_Audio_Prep;
  XA_Audio_On		= SGI_Audio_On;
  XA_Closest_Freq	= SGI_Closest_Freq;
  XA_Set_Output_Port	= (void *)(0);
  XA_Speaker_Tog	= SGI_Speaker_Toggle;
  XA_Headphone_Tog	= SGI_Headphone_Toggle;
  XA_LineOut_Tog	= SGI_Headphone_Toggle;
  XA_Adjust_Volume	= SGI_Adjust_Volume;

  xa_snd_cur = 0;
  xa_audio_present = XA_AUDIO_UNK;
  xa_audio_status  = XA_AUDIO_STOPPED;
  xa_audio_ring_size  = 8;
}

/********** SGI_Audio_Init **********************
 * Init SGI Audio, init variables, open Audio Devices.
 *****/
void SGI_Audio_Init()
{ long sgi_params[4];
  DEBUG_LEVEL2 fprintf(stderr,"SGI_Audio_Init\n");

  if (xa_audio_present != XA_AUDIO_UNK) return;

  conf = ALnewconfig();
  ALsetwidth(conf, AL_SAMPLE_16);
  ALsetchannels(conf, AL_MONO);
  ALsetqueuesize(conf, 16384);
  port = ALopenport("XAnim Mono", "w", conf);
  if (port == 0)
  { fprintf(stderr,"Can't Open SGI Audio device\n");
    xa_audio_present = XA_AUDIO_ERR;
    return;
  }

  /* get default frequency - no really necessary */
  sgi_params[0] = AL_OUTPUT_RATE;
  sgi_params[1] = 11025;
  ALsetparams(AL_DEFAULT_DEVICE, sgi_params, 2);
  ALgetparams(AL_DEFAULT_DEVICE, sgi_params, 2);
  xa_audio_hard_freq = sgi_params[1];

  xa_audio_hard_bps = 2;
  xa_audio_hard_chans = 1;
  xa_audio_hard_buff  = XA_HARD_BUFF_2K;   /* was XA_HARD_BUFF_1K */
  xa_audio_hard_type  = XA_AUDIO_SIGNED_2MB;

  xa_interval_id = 0;
  xa_audio_present = XA_AUDIO_OK;
  DEBUG_LEVEL2 fprintf(stderr,"   success \n");
  Init_Audio_Ring(xa_audio_ring_size,
			(XA_AUDIO_MAX_RING_BUFF * xa_audio_hard_bps) );
}
/********** SGI_Audio_Kill **********************
 * Stop SGI Audio and Close SGI Audio Devices. Free up memory.
 *****/
void SGI_Audio_Kill()
{
  /* TURN AUDIO OFF */
  SGI_Audio_Off(0);
  xa_audio_present = XA_AUDIO_UNK;
  /* SHUT THINGS DOWN  */
  ALcloseport(port);
  ALfreeconfig(conf);
  Kill_Audio_Ring();
  return;
}

/********** SGI_Audio_Off **********************
 * Stop SGI Audio
 *****/
void SGI_Audio_Off(flag)
xaULONG flag;
{
  DEBUG_LEVEL2 fprintf(stderr,"SGI_Audio_Off\n");
  if (xa_audio_status != XA_AUDIO_STARTED) return;

  /* SET FLAG TO STOP OUTPUT ROUTINE */
  xa_audio_status = XA_AUDIO_STOPPED;

  /* TURN OFF SOUND ??? */
  SGI_Adjust_Volume(XA_AUDIO_MINVOL);

  /* FLUSH DEVICE SHOULD GO HERE */
  /* POD NOTE: DOES SGI HAVE A FLUSH???? */

  /* Wait until all samples drained */
  while(ALgetfilled(port) != 0) sginap(1);

  xa_time_audio = -1;
  xa_audio_flushed = 0;

  /* RESTORE ORIGINAL VOLUME */
  if (XAAUD->mute != xaTRUE) SGI_Adjust_Volume(XAAUD->volume);
}

/********** SGI_Audio_Prep **********************
 * Startup SGI Audio
 *****/
void SGI_Audio_Prep()
{
  DEBUG_LEVEL2 fprintf(stderr,"SGI_Audio_Prep \n");
  if (xa_audio_status == XA_AUDIO_STARTED) return;
  else if (xa_audio_present != XA_AUDIO_OK) return;
  else if (xa_snd_cur)
  { 
    /* Change Frequency If necessary */
    if (xa_audio_hard_freq != xa_snd_cur->hfreq)
    { long sgi_params[2];
      sgi_params[0] = AL_OUTPUT_RATE;
      sgi_params[1] = xa_snd_cur->hfreq;
      ALsetparams(AL_DEFAULT_DEVICE, sgi_params, 2);
      xa_audio_hard_freq = xa_snd_cur->hfreq;
    }

    /* xa_snd_cur gets changes in Update_Ring() */
    xa_out_time = 500;  /* keep audio fed 500ms ahead of video */
    xa_out_init = xa_audio_ring_size - 1;
    xa_interval_time = xa_snd_cur->ch_time / XAAUD->divtest;
    if (xa_interval_time == 0) xa_interval_time = 1;

    XA_Flush_Ring();
    XA_Update_Ring(1000);
    xa_audio_status = XA_AUDIO_PREPPED;
  }
}

/****-------------------------------------------------------------------****
 *
 ****-------------------------------------------------------------------****/
void SGI_Audio_On()
{
  if (   (xa_snd_cur)
      && (xa_audio_present == XA_AUDIO_OK)
      && (xa_audio_status == XA_AUDIO_PREPPED) )
  { 
    xa_audio_status = XA_AUDIO_STARTED;
    xa_time_now = XA_Read_AV_Time();  /* get new time */
    New_Merged_Audio_Output();
  }
}

/********** SGI_Closest_Freq **********************
 * Return closest support frequeny and set hard_buff.
 * The IRIX 4.0.5 audio.h file lists the following frequencies:
 * {8000,11025,16000,22050,32000,44100,48000}
 *****/
xaULONG SGI_Closest_Freq(ifreq)
xaLONG ifreq;
{ static int valid[] = { 8000,11025,16000,22050,32000,44100,0};
  xaLONG i = 0;
  xaLONG ofreq = 8000;
  long sgi_params[2];

  while(valid[i])
  { 
    if (xaABS(valid[i] - ifreq) < xaABS(ofreq - ifreq)) ofreq = valid[i];
    i++;
  }
  sgi_params[0] = AL_OUTPUT_RATE;
  sgi_params[1] = ofreq;
  ALsetparams(AL_DEFAULT_DEVICE, sgi_params, 2); 
  ALgetparams(AL_DEFAULT_DEVICE, sgi_params, 2); 
  if (ofreq != sgi_params[1])
	fprintf(stderr,"SGI AUDIO: freq gotten %d wasn't wanted %d\n",
		sgi_params[1],ofreq);
  ofreq = sgi_params[1];
  xa_audio_hard_buff = XA_HARD_BUFF_2K; /* fixed for SGI */
  return(ofreq);
}


/********** SGI_Speaker_Toggle **********************
 * Turn off/on/toggle SGI's Speaker(if possible)
 * flag= 0   1   2 
 *****/
void SGI_Speaker_Toggle(flag)
xaULONG flag;
{
  return;
}

/********** SGI_Headphone_Toggle **********************
 * Turn off/on/toggle SGI's Headphone(if possible)
 * flag= 0   1   2 
 *****/
void SGI_Headphone_Toggle(flag)
xaULONG flag;
{
  return;
}

/********** SGI_Adjust_Volume **********************
 * Routine for Adjusting Volume on SGI
 *
 *****/
void SGI_Adjust_Volume(volume)
xaULONG volume;
{ xaULONG adj_volume;
  long sgi_params[4];
  adj_volume =
      (int)(pow(10.0, (float)volume / XA_AUDIO_MAXVOL * 2.406540183) + 0.5);
  if (adj_volume > SGI_MAX_VOL) adj_volume = SGI_MAX_VOL;
  sgi_params[0] = AL_LEFT_SPEAKER_GAIN;
  sgi_params[1] = adj_volume;
  sgi_params[2] = AL_RIGHT_SPEAKER_GAIN;
  sgi_params[3] = adj_volume;
  ALsetparams(AL_DEFAULT_DEVICE, sgi_params, 4);
}

#endif
/****************************************************************************/
/******************* END OF SGI SPECIFIC ROUTINES *********************/
/****************************************************************************/

/****************************************************************************/
/**************** HP SPECIFIC ROUTINES *******************************/
/****************************************************************************/
#ifdef XA_HP_AUDIO

void HP_Audio_Init();
void HP_Audio_Kill();
void HP_Audio_Off();
void HP_Audio_Prep();
void HP_Audio_On();
xaULONG HP_Closest_Freq();
void HP_Speaker_Toggle();
void HP_Headphone_Toggle();
void HP_Adjust_Volume();
void HP_Print_Error();
void HP_Print_ErrEvent();
long HP_Audio_Err_Handler();
void TheEnd();
int  streamSocket; /*POD NOTE: rename */


#define HP_MAX_VOL (audio_connection->max_output_gain)
#define HP_MIN_VOL (audio_connection->min_output_gain)

Audio        *audio_connection = 0;

/* structures */
AudioAttributes	play_attribs;
SSPlayParams	streamParams;
SStream 	audio_stream;
/* longs */
ATransID	hp_trans_id;
AudioAttrMask	hp_attr_mask;
AErrorHandler	hp_prev_handler;
AGainEntry	gainEntry[4];
long		hp_status;

long *hp_sample_rates = 0;
int hp_audio_paused = xaFALSE;

long HP_Audio_Err_Handler(audio,err_event)
Audio *audio;
AErrorEvent *err_event;
{
  fprintf(stderr,"HP_Audio_Err_Handler:\n");
  xa_audio_present = XA_AUDIO_ERR;
  xa_audio_status  = XA_AUDIO_STOPPED;
  HP_Print_ErrEvent("HP_Audio_Err_Handler: ","\n",err_event->error_code);
  /* PODNOTE: temporary: eventually we'll try a gracefull recovery
   * and shift back to visual only 
   */
  TheEnd();
}

/********** XA_Audio_Setup **********************
 * Sets up HP specific Routines
 *****/
void XA_Audio_Setup()
{
  XA_Audio_Init		= HP_Audio_Init;
  XA_Audio_Kill		= HP_Audio_Kill;
  XA_Audio_Off		= HP_Audio_Off;
  XA_Audio_Prep		= HP_Audio_Prep;
  XA_Audio_On		= HP_Audio_On;
  XA_Closest_Freq	= HP_Closest_Freq;
  XA_Set_Output_Port	= (void *)(0);
  XA_Speaker_Tog	= HP_Speaker_Toggle;
  XA_Headphone_Tog	= HP_Headphone_Toggle;
  XA_LineOut_Tog	= HP_Headphone_Toggle;

  xa_snd_cur = 0;
  xa_audio_present = XA_AUDIO_UNK;
  xa_audio_status  = XA_AUDIO_STOPPED;
  xa_audio_ring_size  = 8;
  hp_audio_paused = xaFALSE;
}

/********** HP_Print_Error **********************
 * print error message
 *****/
void HP_Print_Error(pre_str,post_str,status)
char *pre_str,*post_str;
AError status;
{ char err_buff[132];
  if (pre_str) fprintf(stderr,"%s (%d)",pre_str,(int)(status));
  AGetErrorText(audio_connection,status,err_buff,131);
  fprintf(stderr,"%s",err_buff);
  if (post_str) fprintf(stderr,"%s",post_str);
}

/********** HP_Print_ErrEvent **********************
 * print error message
 *****/
void HP_Print_ErrEvent(pre_str,post_str,errorCode)
char *pre_str,*post_str;
int  errorCode;
{ char err_buff[132];
  if (pre_str) fprintf(stderr,"%s (%d)",pre_str,(int)(errorCode));
  AGetErrorText(audio_connection,errorCode,err_buff,131);
  fprintf(stderr,"%s",err_buff);
  if (post_str) fprintf(stderr,"%s",post_str);
}


/********** HP_Audio_Init ***************************************************
 * Init HP Audio, init variables, open Audio Devices.
 *****/
void HP_Audio_Init()
{ int ret,tmp,type;
  AudioAttributes req_attribs;
  AByteOrder  req_byteorder;
  AudioAttrMask req_mask;
  char server[256];
  server[0] = '\0';

  DEBUG_LEVEL2 fprintf(stderr,"HP_Audio_Init\n");
  DEBUG_LEVEL2 fprintf(stderr,"HP_Audio_Init: theAudContext = %x\n",
						(xaULONG)theAudContext);

  if (xa_audio_present != XA_AUDIO_UNK) return;

  /* Setup Error Handler */
  hp_prev_handler = ASetErrorHandler(HP_Audio_Err_Handler);

  /* Open Up Audio */
  audio_connection = AOpenAudio( server, &hp_status);
  if (audio_connection == NULL)
  { char buf[256];
    HP_Print_Error("HP_Audio: couldn't connect to Audio -","\n",hp_status);
    xa_audio_present = XA_AUDIO_ERR;
    return;
  }


  /* Request the following attributes and see what get's returned */
  req_attribs.type = ATSampled;
  req_attribs.attr.sampled_attr.data_format = ADFLin16;
  req_attribs.attr.sampled_attr.bits_per_sample = 16;
  req_attribs.attr.sampled_attr.sampling_rate = 8000;
  req_attribs.attr.sampled_attr.channels = 1;  /* MONO */
  req_attribs.attr.sampled_attr.interleave = 1; /*???*/
  req_byteorder = AMSBFirst;

  play_attribs.type = ATSampled;
  play_attribs.attr.sampled_attr.data_format = ADFLin16;
  play_attribs.attr.sampled_attr.bits_per_sample = 16;
  play_attribs.attr.sampled_attr.sampling_rate = 8000;
  play_attribs.attr.sampled_attr.channels = 1;  /* MONO */
  play_attribs.attr.sampled_attr.interleave = 1; /*???*/

  fprintf(stderr,"REQ- format %d  bits %d  rate %d  chans %d bo %d\n",
	req_attribs.attr.sampled_attr.data_format,
	req_attribs.attr.sampled_attr.bits_per_sample,
	req_attribs.attr.sampled_attr.sampling_rate,
	req_attribs.attr.sampled_attr.channels,
	req_byteorder );

/*
  req_mask =   ASDataFormatMask | ASBitsPerSampleMask | ASSamplingRateMask
	    | ASChannelsMask;
*/
  req_mask = ASDurationMask;

  AChoosePlayAttributes(audio_connection, &req_attribs,req_mask,
				&play_attribs, &req_byteorder, NULL);

  fprintf(stderr,"REQ+ format %d  bits %d  rate %d  chans %d bo %d\n",
	req_attribs.attr.sampled_attr.data_format,
	req_attribs.attr.sampled_attr.bits_per_sample,
	req_attribs.attr.sampled_attr.sampling_rate,
	req_attribs.attr.sampled_attr.channels,
	req_byteorder );

  fprintf(stderr,"PLAY format %d  bits %d  rate %d  chans %d\n",
	play_attribs.attr.sampled_attr.data_format,
	play_attribs.attr.sampled_attr.bits_per_sample,
	play_attribs.attr.sampled_attr.sampling_rate,
	play_attribs.attr.sampled_attr.channels	);

  switch(play_attribs.attr.sampled_attr.data_format)
  {
    case ADFLin16:	xa_audio_hard_type  = XA_AUDIO_SIGNED_2MB; break;
    case ADFLin8:	xa_audio_hard_type  = XA_AUDIO_LINEAR_1M; break;
    case ADFLin8Offset:	xa_audio_hard_type  = XA_AUDIO_SIGNED_1M; break;
    default:
	fprintf(stderr,"HP_Audio: unknown hardware format\n");
	xa_audio_present = XA_AUDIO_ERR;
	return;
	break;
  }

  if (play_attribs.attr.sampled_attr.bits_per_sample == 16)
  		xa_audio_hard_bps = 2;
  else		xa_audio_hard_bps = 1;

  if ( (req_byteorder == AMSBFirst) && (xa_audio_hard_bps > 1) )
			xa_audio_hard_type |= XA_AUDIO_BIGEND_MSK;

  xa_audio_hard_chans = play_attribs.attr.sampled_attr.channels;
  if (xa_audio_hard_chans == 2) 
			xa_audio_hard_type |= XA_AUDIO_STEREO_MSK;

DEBUG_LEVEL1 fprintf(stderr,"HP hard_type %x\n",xa_audio_hard_type);

  xa_audio_hard_freq = play_attribs.attr.sampled_attr.sampling_rate;

  xa_audio_hard_buff  = XA_HARD_BUFF_1K;

/* NOT USED FOR NOW, BUT HOPEFULLY EVENTUALLY */
  { unsigned long *tmp;
    long num;
    hp_sample_rates = 0;
    num = ANumSamplingRates(audio_connection);
    if (num >= 0)
    {
      hp_sample_rates = (long *)malloc( (num + 2) * sizeof(long) );
      tmp = ASamplingRates(audio_connection);
      if (tmp)
      { xaULONG i;
        for(i=0; i < num; i++) hp_sample_rates[i] = tmp[i];
        hp_sample_rates[num] = 0;
/*
DEBUG_LEVEL1
*/
{ 
  fprintf(stderr,"HP sampling rates: ");
  for(i=0;i<num;i++) fprintf(stderr,"<%d>",hp_sample_rates[i]);
  fprintf(stderr,"\n");
}
      }
      else {FREE(hp_sample_rates,0x500); hp_sample_rates=0;}
      if (hp_sample_rates==0)
      {
	fprintf(stderr,"Audio_Init: couldn't get sampling rates\n");
	xa_audio_present = XA_AUDIO_ERR;
	return;
      }
    }
  }

  xa_interval_id = 0;
  xa_audio_present = XA_AUDIO_OK;

  Init_Audio_Ring(xa_audio_ring_size,
			(XA_AUDIO_MAX_RING_BUFF * xa_audio_hard_bps) );

  switch(play_attribs.attr.sampled_attr.channels)
  {
    case 1:
	gainEntry[0].u.o.out_ch = AOCTMono;
	gainEntry[0].gain = AUnityGain;
	if (XAAUD->port & XA_AUDIO_PORT_INT)
		gainEntry[0].u.o.out_dst = AODTMonoIntSpeaker; /* internal */
	else	gainEntry[0].u.o.out_dst = AODTMonoJack; /* ext or hdphones */
	break;
    case 2:
    default:
	fprintf(stderr,"HP_AUDIO: Warning hardware wants stereo\n");
	gainEntry[0].u.o.out_ch = AOCTLeft;
	gainEntry[0].gain = AUnityGain;
	gainEntry[1].u.o.out_ch = AOCTRight;
	gainEntry[1].gain = AUnityGain;
	if (XAAUD->port & XA_AUDIO_PORT_INT)
	{
	  gainEntry[0].u.o.out_dst = AODTLeftIntSpeaker;
	  gainEntry[1].u.o.out_dst = AODTRightIntSpeaker;
	}
	else
	{
	  gainEntry[0].u.o.out_dst = AODTLeftJack;
	  gainEntry[1].u.o.out_dst = AODTRightJack;
	}
        break;
  }
  streamParams.gain_matrix.type = AGMTOutput;
  streamParams.gain_matrix.num_entries = 
			play_attribs.attr.sampled_attr.channels;  
  streamParams.gain_matrix.gain_entries = gainEntry;
  streamParams.play_volume = AUnityGain;
  streamParams.priority = APriorityNormal;
  streamParams.event_mask = 0;
   

  /* START UP SOCKET CONNECTION TO HP's AUDIO DEVICE */
/*
  hp_attr_mask = ASDataFormatMask | ASBitsPerSampleMask | 
			ASSamplingRateMask | ASChannelsMask | ASInterleaveMask;
*/
  hp_attr_mask = ~0;

  /* create the stream */
  hp_trans_id = APlaySStream(audio_connection,hp_attr_mask,&play_attribs,
				&streamParams,&audio_stream,NULL);

  /* create socket */
  streamSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (streamSocket < 0) fprintf(stderr,"HP_Audio: socket err %d\n",errno);

  ret = connect(streamSocket,(struct sockaddr *)&audio_stream.tcp_sockaddr,
					sizeof(struct sockaddr_in) );
  if (ret < 0) fprintf(stderr,"HP_Audio: connect error %d\n",errno);

  /* Pause Audio Stream */
  APauseAudio( audio_connection, hp_trans_id, NULL, NULL );
  hp_audio_paused = xaTRUE;
}

/********** HP_Audio_Kill ***************************************************
 * Stop HP Audio and Close HP Audio Devices. Free up memory.
 *****/
void HP_Audio_Kill()
{
  DEBUG_LEVEL2 fprintf(stderr,"HP_Audio_Kill\n");
  HP_Audio_Off(0);
  close(streamSocket);
  /* IS THIS NEEDED? need to get man page for ASetCloseDownMode */
  ASetCloseDownMode(audio_connection, AKeepTransactions, NULL );
  ACloseAudio(audio_connection,NULL);
  Kill_Audio_Ring();
}

/********** HP_Audio_Off ***************************************************
 * Stop HP Audio
 *****/
void HP_Audio_Off(flag)
xaULONG flag;
{
  DEBUG_LEVEL2 fprintf(stderr,"HP_Audio_Off\n");
  if (xa_audio_status != XA_AUDIO_STARTED) return;
  /* SET FLAG TO STOP OUTPUT ROUTINE */
  xa_audio_status = XA_AUDIO_STOPPED;

  /* POD NOTE: USE PAUSE FOR NOW */
/*
  APauseAudio(audio_connection,hp_trans_id, NULL, NULL);
  hp_audio_paused = xaTRUE;
*/
  xa_audio_status = XA_AUDIO_STOPPED;
  xa_time_audio = -1;
  xa_audio_flushed = 0;
}

/********** HP_Audio_Prep ***************************************************
 * Startup HP Audio
 *****/
void HP_Audio_Prep()
{ AudioAttrMask attr_mask;
  DEBUG_LEVEL2 fprintf(stderr,"HP_Audio_Prep \n");
  if (xa_audio_status == XA_AUDIO_STARTED) return;
  else if (xa_audio_present != XA_AUDIO_OK) return;
  else if (xa_snd_cur)
  { int ret,tmp,utime;

    /* Change Frequency If necessary */
    if (xa_audio_hard_freq != xa_snd_cur->hfreq)
    { 
      play_attribs.attr.sampled_attr.sampling_rate = xa_snd_cur->hfreq;
      xa_audio_hard_freq = xa_snd_cur->hfreq;
    }

    /* xa_snd_cur gets changes in Update_Ring() */
    xa_out_time = 250;  /* keep audio fed 500ms ahead of video */
    xa_out_init = xa_audio_ring_size - 1;
    xa_interval_time = xa_snd_cur->ch_time / XAAUD->divtest;
    if (xa_interval_time == 0) xa_interval_time = 1;

    XA_Flush_Ring();
    XA_Update_Ring(1000);
    xa_audio_status = XA_AUDIO_PREPPED;
  }
}

/****-------------------------------------------------------------------****
 *
 ****-------------------------------------------------------------------****/
void HP_Audio_On()
{
  if (   (xa_snd_cur)
      && (xa_audio_present == XA_AUDIO_OK)
      && (xa_audio_status == XA_AUDIO_PREPPED) )
  { 
    xa_audio_status = XA_AUDIO_STARTED;
    xa_time_now = XA_Read_AV_Time();  /* get new time */
    New_Merged_Audio_Output();
  }
}

/********** HP_Closest_Freq **********************
 * Return closest support frequeny and set hard_buff.
 *****/
xaULONG HP_Closest_Freq(ifreq)
xaLONG ifreq;
{
 /* POD TEMPORARY */
  xa_audio_hard_buff = XA_HARD_BUFF_1K;
  return(8000);
}

/********** HP_Speaker_Toggle **********************
 * Turn off/on/toggle HP's Speaker(if possible)
 * flag= 0   1   2 
 *****/
void HP_Speaker_Toggle(flag)
xaULONG flag;
{
  return;
}

/********** HP_Headphone_Toggle **********************
 * Turn off/on/toggle HP's Headphone(if possible)
 * flag= 0   don't use Headphones(defaults to internal speaker)
 * flag= 1   Use Headphones.
 * flag= 2   Toggle. If headphone then internal and vice_versa. 
 *****/
void HP_Headphone_Toggle(flag)
xaULONG flag;
{
  return;
}

/********** HP_Adjust_Volume **********************
 * Routine for Adjusting Volume on HP
 *
 *****/

/* AUnityGain is 0. AZeroGain is 0x80000000 */
void HP_Adjust_Volume(volume)
xaLONG volume;
{ xaLONG adj_volume;
  AGainDB play_gain,tmp_gain;
  int ret;
  adj_volume = HP_MIN_VOL + 
	((volume * (HP_MAX_VOL - HP_MIN_VOL)) / XA_AUDIO_MAXVOL);
  if (adj_volume > HP_MAX_VOL) adj_volume = HP_MAX_VOL;
  play_gain = (AGainDB)adj_volume;
/*
  AGetSystemChannelGain(audio_connection,ASGTPlay,ACTMono,&tmp_gain,NULL);
  fprintf(stderr,"Get Gain %x adj_vol %x  hpmin %x hpmax %x\n",
				tmp_gain,adj_volume,HP_MIN_VOL, HP_MAX_VOL);
  ASetSystemChannelGain(audio_connection,ASGTPlay,ACTMono,play_gain,NULL);
  ASetChannelGain(audio_connection,hp_trans_id,ACTMono,play_gain,NULL);
  ASetGain(audio_connection,hp_trans_id,play_gain,NULL);
*/
  ASetSystemPlayGain(audio_connection,play_gain,NULL);
}

#endif
/****************************************************************************/
/******************* END OF HP SPECIFIC ROUTINES ****************************/
/****************************************************************************/


/****************************************************************************/
/**************** HPDEV SPECIFIC ROUTINES ***************************/
/****************************************************************************/
#ifdef XA_HPDEV_AUDIO

void HPDEV_Audio_Init();
void HPDEV_Audio_Kill();
void HPDEV_Audio_Off();
void HPDEV_Audio_Prep();
void HPDEV_Audio_On();
xaULONG HPDEV_Closest_Freq();
void HPDEV_Speaker_Toggle();
void HPDEV_Headphone_Toggle();
void HPDEV_LineOut_Toggle();
void HPDEV_Adjust_Volume();
static void HPDEV_Audio_Output();

static int devAudio;
static struct audio_describe audioDesc;


/********** XA_Audio_Setup **********************
 * Sets up HP specific routines
 *****/
void XA_Audio_Setup()
{
  XA_Audio_Init		= HPDEV_Audio_Init;
  XA_Audio_Kill		= HPDEV_Audio_Kill;
  XA_Audio_Off		= HPDEV_Audio_Off;
  XA_Audio_Prep		= HPDEV_Audio_Prep;
  XA_Audio_On		= HPDEV_Audio_On;
  XA_Closest_Freq	= HPDEV_Closest_Freq;
  XA_Set_Output_Port	= (void *)(0);
  XA_Speaker_Tog	= HPDEV_Speaker_Toggle;
  XA_Headphone_Tog	= HPDEV_Headphone_Toggle;
  XA_LineOut_Tog	= HPDEV_LineOut_Toggle;
  XA_Adjust_Volume	= HPDEV_Adjust_Volume;

  xa_snd_cur = 0;
  xa_audio_present = XA_AUDIO_UNK;
  xa_audio_status = XA_AUDIO_STOPPED;
  xa_audio_ring_size = 8;
}

/********** HPDEV_Audio_Init ***************************************************
 * open /dev/audio and init
 *****/
void HPDEV_Audio_Init()
{
  DEBUG_LEVEL2 fprintf(stderr,"HPDEV_Audio_Init\n");
  if (xa_audio_present != XA_AUDIO_UNK) return;

  if ((devAudio = open ("/dev/audio", O_WRONLY | O_NDELAY, 0)) < 0)
  {
    if (errno == EBUSY) fprintf(stderr,"Audio_Init: Audio device is busy. - ");
    else fprintf(stderr,"Audio_Init: Error opening audio device. - ");
    fprintf(stderr,"Will continue without audio\n");
    xa_audio_present = XA_AUDIO_ERR;
    return;
  }

  fcntl(devAudio,F_SETFL,O_NDELAY);


  /* Get description of /dev/audio: */
  if (ioctl (devAudio, AUDIO_DESCRIBE, &audioDesc))
  {
    perror ("ioctl AUDIO_DESCRIBE on /dev/audio");
    xa_audio_present = XA_AUDIO_ERR;
    return;
  }

  xa_audio_hard_type  = XA_AUDIO_SIGNED_2MB;
  xa_audio_hard_freq  = 0; /* none yet (HPDEV_Audio_Prep does this job) */
  xa_audio_hard_bps   = 2;
  xa_audio_hard_chans = 1;
  xa_audio_hard_buff  = XA_HARD_BUFF_1K;

  if (ioctl (devAudio, AUDIO_SET_DATA_FORMAT, AUDIO_FORMAT_LINEAR16BIT))
  {
    perror ("ioctl AUDIO_SET_DATA_FORMAT on /dev/audio");
    xa_audio_present = XA_AUDIO_ERR;
    return;
  }
  if (ioctl (devAudio, AUDIO_SET_CHANNELS, 1))
  {
    perror ("ioctl AUDIO_SET_CHANNELS on /dev/audio");
    xa_audio_present = XA_AUDIO_ERR;
    return;
  }

  /* set device internal buffer size: */
  if (ioctl (devAudio, AUDIO_SET_TXBUFSIZE, 16 * 1024))
    perror ("ioctl AUDIO_SET_TXBUFSIZE on /dev/audio");

  xa_interval_id = 0;
  xa_audio_present = XA_AUDIO_OK;
  Init_Audio_Ring(xa_audio_ring_size, (XA_AUDIO_MAX_RING_BUFF * xa_audio_hard_bps) );
}

/********** HPDEV_Audio_Kill ***************************************************
 * Stop HP Audio and Close HP Audio Devices. Free up memory.
 *****/
void HPDEV_Audio_Kill()
{
  DEBUG_LEVEL2 fprintf(stderr,"HPDEV_Audio_Kill\n");
  HPDEV_Audio_Off(0);
  xa_audio_present = XA_AUDIO_UNK;
  close(devAudio);
  Kill_Audio_Ring();
}

/********** HPDEV_Audio_Off ***************************************************
 * Stop HP Audio
 *****/
void HPDEV_Audio_Off(flag)
xaULONG flag;
{
  DEBUG_LEVEL2 fprintf(stderr,"HPDEV_Audio_Off\n");
  if (xa_audio_status != XA_AUDIO_STARTED) return;

  /* SET FLAG TO STOP OUTPUT ROUTINE */
  xa_audio_status = XA_AUDIO_STOPPED;

  if (ioctl (devAudio, AUDIO_DRAIN, 0))
    perror ("ioctl AUDIO_DRAIN on /dev/audio");

  xa_time_audio = -1;
  xa_audio_flushed = 0;
}

/********** HPDEV_Audio_Prep **************************************************
 * Startup HP Audio
 *****/
void HPDEV_Audio_Prep()
{
  DEBUG_LEVEL2 fprintf(stderr,"HPDEV_Audio_Prep\n");
  if (xa_audio_status == XA_AUDIO_STARTED) return;
  else if (xa_audio_present != XA_AUDIO_OK) return;
  else if (xa_snd_cur)
  { int ret,tmp,utime;

    /* Change frequency if necessary */
    if (xa_audio_hard_freq != xa_snd_cur->hfreq)
    { 
      do { ret = ioctl(devAudio, AUDIO_SET_SAMPLE_RATE, xa_snd_cur->hfreq);
         } while( (ret < 0) && (errno == EBUSY) );
      if (ret < 0) perror ("ioctl AUDIO_SET_SAMPLE_RATE on /dev/audio");
      xa_audio_hard_freq = xa_snd_cur->hfreq;
    }

    /* xa_snd_cur gets changes in Update_Ring() */
    xa_out_time = 250; /* keep audio fed 250ms ahead of video */
    xa_out_init = xa_audio_ring_size - 1;
    xa_interval_time = xa_snd_cur->ch_time / XAAUD->divtest;
    if (xa_interval_time == 0) xa_interval_time = 1;

    XA_Flush_Ring();
    XA_Update_Ring(1000);
    xa_audio_status = XA_AUDIO_PREPPED;
  }
}

/****-------------------------------------------------------------------****
 *
 ****-------------------------------------------------------------------****/
void HPDEV_Audio_On()
{
  if (   (xa_snd_cur)
      && (xa_audio_present == XA_AUDIO_OK)
      && (xa_audio_status == XA_AUDIO_PREPPED) )
  { 
    xa_audio_status = XA_AUDIO_STARTED;
    xa_time_now = XA_Read_AV_Time();  /* get new time */
    New_Merged_Audio_Output();
  }
}

/********** HPDEV_Closest_Freq **********************
 * Return closest support frequeny and set hard_buff.
 *****/
xaULONG HPDEV_Closest_Freq(ifreq)
xaLONG ifreq;
{
  unsigned int rate_index;
  long nearest_frequency = 8000;
  long frequency_diff = 999999;

  xa_audio_hard_buff = XA_HARD_BUFF_1K;

  for (rate_index = 0; rate_index < audioDesc.nrates; ++rate_index)
  { if (abs (audioDesc.sample_rate[rate_index] - ifreq) <= frequency_diff)
    { nearest_frequency = audioDesc.sample_rate[rate_index];
      frequency_diff = abs (audioDesc.sample_rate[rate_index] - ifreq);
    }
  }
  return nearest_frequency;
}

/********** HPDEV_Speaker_Toggle **********************
 * Turn off/on/toggle HP's Speaker(if possible)
 * flag= 0   1   2 
 *****/
void HPDEV_Speaker_Toggle(flag)
xaULONG flag;
{
  int output_channel;

  if (ioctl (devAudio, AUDIO_GET_OUTPUT, &output_channel))
  {
    perror ("ioctl AUDIO_GET_OUTPUT on /dev/audio");
    output_channel = 0;
  }
  switch (flag)
  {
    case 0:
      output_channel &= ~AUDIO_OUT_SPEAKER; break;
    case 1:
      output_channel |= AUDIO_OUT_SPEAKER; break;
    case 2:
      output_channel ^= AUDIO_OUT_SPEAKER;
  }
  if (ioctl (devAudio, AUDIO_SET_OUTPUT, output_channel))
    perror ("ioctl AUDIO_SET_OUTPUT on /dev/audio");
}

/********** HPDEV_Headphone_Toggle **********************
 * Turn off/on/toggle HP's Headphone(if possible)
 * flag= 0   don't use Headphones(defaults to internal speaker)
 * flag= 1   Use Headphones.
 * flag= 2   Toggle. If headphone then internal and vice_versa. 
 *****/
void HPDEV_Headphone_Toggle(flag)
xaULONG flag;
{
  int output_channel;

  if (ioctl (devAudio, AUDIO_GET_OUTPUT, &output_channel))
  {
    perror ("ioctl AUDIO_GET_OUTPUT on /dev/audio");
    output_channel = 0;
  }
  switch (flag)
  {
    case 0:
      output_channel &= ~AUDIO_OUT_HEADPHONE; break;
    case 1:
      output_channel |= AUDIO_OUT_HEADPHONE; break;
    case 2:
      output_channel ^= AUDIO_OUT_HEADPHONE;
  }
  if (ioctl (devAudio, AUDIO_SET_OUTPUT, output_channel))
    perror ("ioctl AUDIO_SET_OUTPUT on /dev/audio");
}

/********** HPDEV_LineOut_Toggle **********************
 * Turn off/on/toggle HP's LineOut(if possible)
 * flag= 0   don't use LineOut(defaults to internal speaker)
 * flag= 1   Use LineOut.
 * flag= 2   Toggle. If LineOut then internal and vice_versa. 
 *****/
void HPDEV_LineOut_Toggle(flag)
xaULONG flag;
{
  int output_channel;

  if (ioctl (devAudio, AUDIO_GET_OUTPUT, &output_channel))
  {
    perror ("ioctl AUDIO_GET_OUTPUT on /dev/audio");
    output_channel = 0;
  }
  switch (flag)
  {
    case 0:
      output_channel &= ~AUDIO_OUT_LINE; break;
    case 1:
      output_channel |= AUDIO_OUT_LINE; break;
    case 2:
      output_channel ^= AUDIO_OUT_LINE;
  }
  if (ioctl (devAudio, AUDIO_SET_OUTPUT, output_channel))
    perror ("ioctl AUDIO_SET_OUTPUT on /dev/audio");
}

/********** HPDEV_Adjust_Volume **********************
 * Routine for Adjusting Volume on HP
 *
 *****/
void HPDEV_Adjust_Volume(volume)
xaLONG volume;
{
  struct audio_describe description;
  struct audio_gains gains;
  float floatvolume = ((float)volume - (float)XA_AUDIO_MINVOL) /
		      ((float)XA_AUDIO_MAXVOL - (float)XA_AUDIO_MINVOL);

  if (ioctl (devAudio, AUDIO_DESCRIBE, &description))
  {
    perror ("ioctl AUDIO_DESCRIBE on /dev/audio");
    return;
  }
  if (ioctl (devAudio, AUDIO_GET_GAINS, &gains))
  {
    perror ("ioctl AUDIO_GET_GAINS on /dev/audio");
    return;
  }
  gains.transmit_gain = (int)((float)description.min_transmit_gain +
			      (float)(description.max_transmit_gain
				      - description.min_transmit_gain) * floatvolume);
  if (ioctl (devAudio, AUDIO_SET_GAINS, &gains))
    perror ("ioctl AUDIO_SET_GAINS on /dev/audio");
}

#endif
/****************************************************************************/
/******************* END OF HPDEV SPECIFIC ROUTINES ******************/
/****************************************************************************/

/****************************************************************************/
/**************** AF SPECIFIC ROUTINES **************************************/
/****************************************************************************/
#ifdef XA_AF_AUDIO

/*
 * AF port provided by Tom Levergood,
 *	tml@crl.dec.com
 * Thu Aug 25 14:05:47 1994
 * This is a quick hack, I need to stare at the xa buffering
 * further to clean up the port.
 */

void  AF_Audio_Init();
void  AF_Audio_Kill();
void  AF_Audio_Off();
void  AF_Audio_Prep();
void  AF_Audio_On();
void  AF_Adjust_Volume();
xaULONG AF_Closest_Freq();
void AF_Speaker_Toggle();
void AF_Headphone_Toggle();


static AFAudioConn *AFaud;
static AC ac;
static AFSetACAttributes AFattributes;
static int	AFdevice;
static ATime AFtime0=0;
static ATime AFbaseT=0;

/********** XA_Audio_Setup **********************
 * 
 * Also defines Sparc Specific variables.
 *
 *****/
void XA_Audio_Setup()
{
  XA_Audio_Init		= AF_Audio_Init;
  XA_Audio_Kill		= AF_Audio_Kill;
  XA_Audio_Off		= AF_Audio_Off;
  XA_Audio_Prep		= AF_Audio_Prep;
  XA_Audio_On		= AF_Audio_On;
  XA_Closest_Freq	= AF_Closest_Freq;
  XA_Set_Output_Port	= (void *)(0);
  XA_Speaker_Tog	= AF_Speaker_Toggle;
  XA_Headphone_Tog	= AF_Headphone_Toggle;
  XA_LineOut_Tog	= AF_Headphone_Toggle;
  XA_Adjust_Volume	= AF_Adjust_Volume;

  xa_snd_cur = 0;
  xa_audio_present = XA_AUDIO_UNK;
  xa_audio_status  = XA_AUDIO_STOPPED;
  xa_audio_ring_size  = 8;
}


#define	AF_C_MIN	-30
#define	AF_C_MAX	30
#define AF_MAP_TO_C_GAIN(vol) (( ((vol)*AF_C_MAX)/XA_AUDIO_MAXVOL) + (AF_C_MIN))

#define SPU(type)       AF_sample_sizes[type].samps_per_unit
#define BPU(type)       AF_sample_sizes[type].bytes_per_unit

#define DPHONE(od) (int)(((AAudioDeviceDescriptor(AFaud, (od)))->outputsToPhone))
#define DFREQ(od) (int)(((AAudioDeviceDescriptor(AFaud, (od)))->playSampleFreq))
#define DCHAN(od) (int)((AAudioDeviceDescriptor(AFaud, (od)))->playNchannels)
#define DTYPE(od) ((AAudioDeviceDescriptor(AFaud, (od)))->playBufType)

#define	TOFFSET	250


/*
 * Find a suitable default device (the first device not connected to the phone),
 *  returns device number.
 *
 * Returns -1 if no suitable device can be found.
 */
static int FindDefaultDevice(aud)
AFAudioConn *aud;
{
  int i;
  char *ep;

  if ((ep = getenv("AF_DEVICE")) != NULL) 
  { int udevice;
    udevice = atoi(ep);
    if ((udevice >= 0) && (udevice < ANumberOfAudioDevices(aud)))
	return udevice;
  }
  for(i=0; i<ANumberOfAudioDevices(aud); i++) 
  {
    if ( DPHONE(i) == 0 && DCHAN(i) == 1 ) return i;
  }
  return -1;
}

void XA_No_Audio_Support()
{
  fprintf(stderr,"AUDIO SUPPORT NOT COMPILED IN THIS EXECUTABLE.\n");
  return;
}

/********** AF_Audio_Init **********************
 *****/
void AF_Audio_Init()
{ 
  DEBUG_LEVEL2 fprintf(stderr,"AF_Audio_Init\n");
  if (xa_audio_present != XA_AUDIO_UNK) return;

  /* Open connection. */
  if ( (AFaud = AFOpenAudioConn("")) == NULL) 
  {
      fprintf(stderr, "Could not open connection to AF audio server.\n");
      fprintf(stderr, "Disabling audio service.\n");
      /* As long as xa_audio_present isn't OK, audio is effectively disabled*/
      /* but might as well really disable it */
      XA_Null_Audio_Setup();
      xa_audio_present = XA_AUDIO_UNK;
      return;
  }
  AFdevice = FindDefaultDevice(AFaud);
  AFattributes.type = LIN16;
  AFattributes.endian = ALittleEndian;
  AFattributes.play_gain = AF_MAP_TO_C_GAIN(XAAUD->volume);
  ac = AFCreateAC(AFaud, AFdevice, (ACPlayGain | ACEncodingType | ACEndian), &AFattributes);
  AFSync(AFaud, 0);     /* Make sure we confirm encoding type support. */
  if (ac == NULL)
  {
    fprintf(stderr,"AF: Could not create audio context for device %d\n",AFdevice);
    xa_audio_present = XA_AUDIO_ERR;
    return;
  }

DEBUG_LEVEL2 fprintf(stderr,"Created audio context, SR %d device %d\n",
			 DFREQ(AFdevice),AFdevice);

  /* get default frequency - not really necessary */
  xa_audio_hard_freq = DFREQ(AFdevice);

  xa_audio_hard_bps = 2;
  xa_audio_hard_chans = 1;
  xa_audio_hard_buff  = XA_HARD_BUFF_1K;
  xa_audio_hard_type  = XA_AUDIO_SIGNED_2ML;

  xa_interval_id = 0;
  xa_audio_present = XA_AUDIO_OK;
  DEBUG_LEVEL2 fprintf(stderr,"   success \n");
  Init_Audio_Ring(xa_audio_ring_size,
			(XA_AUDIO_MAX_RING_BUFF * xa_audio_hard_bps) );
}

/********** AF_Audio_Kill **********************
 *****/
void AF_Audio_Kill()
{ 
DEBUG_LEVEL2 fprintf(stderr,"AF_Audio_Kill\n");

  AF_Audio_Off(0);
  xa_audio_present = XA_AUDIO_UNK;

  /* XXX */
  AFCloseAudioConn(AFaud);

  Kill_Audio_Ring();
}

/********** AF_Audio_Off **********************
 * Stop Audio Stream
 *
 *****/
void AF_Audio_Off(flag)
xaULONG flag;
{ 
  DEBUG_LEVEL2 fprintf(stderr,"AF_Audio_Off\n");
  if (xa_audio_status == XA_AUDIO_STOPPED) return;

  /* XXX */

  xa_audio_status = XA_AUDIO_STOPPED;
}

/********** AF_Audio_Prep **********************
 * Turn On Audio Stream.
 *
 *****/
void AF_Audio_Prep()
{
  DEBUG_LEVEL2 fprintf(stderr,"AF_Audio_Prep \n");
  if (xa_audio_status == XA_AUDIO_STARTED) return;
  else if (xa_audio_present != XA_AUDIO_OK) return;
  else if (xa_snd_cur)
  { 
    /* Change Frequency If necessary */
    if (xa_audio_hard_freq != xa_snd_cur->hfreq)
    { 
	/* XXX */
      xa_audio_hard_freq = xa_snd_cur->hfreq;
    }

    /* xa_snd_cur gets changes in Update_Ring() */
    xa_out_time = 250; /* keep audio fed 250ms ahead of video - could be 500*/
    xa_out_init = xa_audio_ring_size - 1;
    xa_interval_time = xa_snd_cur->ch_time / XAAUD->divtest;
    if (xa_interval_time == 0) xa_interval_time = 1;

    XA_Flush_Ring();
    XA_Update_Ring(1000);
    xa_audio_status = XA_AUDIO_PREPPED;
  }
}

/****-------------------------------------------------------------------****
 *
 ****-------------------------------------------------------------------****/
void AF_Audio_On()
{
  if (   (xa_snd_cur)
      && (xa_audio_present == XA_AUDIO_OK)
      && (xa_audio_status == XA_AUDIO_PREPPED) )
  { 
    xa_audio_status = XA_AUDIO_STARTED;
    AFtime0 = AFbaseT = AFGetTime(ac) + TOFFSET;
    xa_time_now = XA_Read_AV_Time();  /* get new time */
    New_Merged_Audio_Output();
  }
}

/*************
 * flag = 0  turn speaker off
 * flag = 1  turn speaker on
 * flag = 2  toggle speaker
 */
void AF_Speaker_Toggle(flag)
xaULONG flag;
{ 
  DEBUG_LEVEL2 fprintf(stderr,"AF_Speaker_Toggle\n");
}

void AF_Headphone_Toggle(flag)
xaULONG flag;
{ 
  DEBUG_LEVEL2 fprintf(stderr,"AF_Headphone_Toggle\n");
}

/********** AF_Adjust_Volume **********************
 * Routine for Adjusting Volume on a AF
 *
 *****/
void AF_Adjust_Volume(volume)
xaULONG volume;
{ 
 DEBUG_LEVEL2 fprintf(stderr,"AF_Audio_Volume %d\n",volume);

 /* Client gain settings range from -30 to +30 dB. */
 AFattributes.play_gain = AF_MAP_TO_C_GAIN(volume);
 AFChangeACAttributes(ac, ACPlayGain, &AFattributes);
 AFSync(AFaud,0);
}

/********** AF_Closest_Freq **********************
 *
 * Global Variable Affect:
 *   xaULONG xa_audio_hard_buff		must set but not larger than
 *					XA_AUDIO_MAX_RING_BUF size
 */
xaULONG AF_Closest_Freq(ifreq)
xaLONG ifreq;
{
 return (DFREQ(AFdevice));
}

#endif
/****************************************************************************/
/******************* END OF AF SPECIFIC ROUTINES ****************************/
/****************************************************************************/


/****************************************************************************/
/**************** DEC Multimedia Services SPECIFIC ROUTINES *****************/
/****************************************************************************/
#ifdef XA_MMS_AUDIO

/*
 * MMS port provided by Tom Morris (morris@zko.dec.com)
 */

void  MMS_Audio_Init();
void  MMS_Audio_Kill();
void  MMS_Audio_Off();
void  MMS_Audio_Prep();
void  MMS_Audio_On();
void  MMS_Adjust_Volume();
xaULONG MMS_Closest_Freq();
void MMS_Speaker_Toggle();
void MMS_Headphone_Toggle();

/* Global variables */

static HWAVEOUT		mms_device_handle = 0;
static UINT		mms_device_id = 0;
static xaULONG		mms_device_volume = xaFALSE;
static LPWAVEHDR	mms_lpWaveHeader;
static LPSTR		mms_audio_buffer;
static int		mms_buffers_outstanding = 0;
static int		mms_next_buffer = 0;

/********** XA_Audio_Setup **********************
 *
 *****/
void XA_Audio_Setup()
{
  XA_Audio_Init		= MMS_Audio_Init;
  XA_Audio_Kill		= MMS_Audio_Kill;
  XA_Audio_Off		= MMS_Audio_Off;
  XA_Audio_Prep		= MMS_Audio_Prep;
  XA_Audio_On		= MMS_Audio_On;
  XA_Closest_Freq	= MMS_Closest_Freq;
  XA_Set_Output_Port	= (void *)(0);
  XA_Speaker_Tog	= MMS_Speaker_Toggle;
  XA_Headphone_Tog	= MMS_Headphone_Toggle;
  XA_LineOut_Tog	= MMS_Headphone_Toggle;
  XA_Adjust_Volume	= MMS_Adjust_Volume;

  xa_snd_cur = 0;
  xa_audio_present = XA_AUDIO_UNK;
  xa_audio_status  = XA_AUDIO_STOPPED;
  xa_audio_ring_size  = 8;
}

void XA_No_Audio_Support()
{
  fprintf(stderr,"AUDIO SUPPORT NOT COMPILED IN THIS EXECUTABLE.\n");
  return;
}

/********** MMS_Audio_Init **********************

  This just does some generic initialization.  The actual audio
  output device open is done in the Audio_Prep routine so that we
  can get a device which matches the requested sample rate & format

 *****/
void MMS_Audio_Init()
{
  MMRESULT	status;
  LPWAVEOUTCAPS	lpCaps;

  DEBUG_LEVEL2 fprintf(stderr,"MMS_Audio_Init\n");
  if (xa_audio_present != XA_AUDIO_UNK) return;

  if( waveOutGetNumDevs() < 1 ) 
  {
    xa_audio_present = XA_AUDIO_ERR;
    fprintf(stderr,"Audio disabled - No Multimedia Services compatible audio devices available\n");
    return;
  }

  /* Figure out device capabilities  - Just use device 0 for now */

  if((lpCaps = (LPWAVEOUTCAPS)mmeAllocMem(sizeof(WAVEOUTCAPS))) == NULL ) {
      fprintf(stderr,"Failed to allocate WAVEOUTCAPS struct\n");
      return;
  }
  status = waveOutGetDevCaps( 0, lpCaps, sizeof(WAVEOUTCAPS));
  if( status != MMSYSERR_NOERROR ) {
      fprintf(stderr,"waveOutGetDevCaps failed - status = %d\n", status);
  }

  /* Multimedia Services will do sample rate & format conversion for
  ** simpler devices, so just go for the max
  */
/* No stereo support currently?
  xa_audio_hard_type  = XA_AUDIO_SIGNED_2SL;
  xa_audio_hard_chans = 2;
  xa_audio_hard_bps = 4;
*/

  xa_audio_hard_type  = XA_AUDIO_SIGNED_2ML;
  xa_audio_hard_chans = 1;
  xa_audio_hard_bps = 2;
 
  /* Make sure device gets openend first time through */
  xa_audio_hard_freq = 0;


  /* Remember whether the volume control will work */
  if( lpCaps->dwSupport & WAVECAPS_VOLUME )
    mms_device_volume = xaTRUE;
  else
    mms_device_volume = xaFALSE;

  mmeFreeMem(lpCaps);

  /*
  ** We don't really care what this value is - use something easy
  */
  xa_audio_hard_buff  = XA_HARD_BUFF_1K;

  xa_interval_id = 0;
  xa_audio_present = XA_AUDIO_OK;

  /* Register our callback routine to be called whenever there is work */
  XtAppAddInput(theAudContext,
		mmeServerFileDescriptor(),
		(XtPointer)XtInputReadMask,
		(XtInputCallbackProc)mmeProcessCallbacks,
		0);

  Init_Audio_Ring(xa_audio_ring_size,
			(XA_AUDIO_MAX_RING_BUFF * xa_audio_hard_bps) );
}

/********** MMS_Audio_Kill **********************
 *****/
void MMS_Audio_Kill()
{
  MMRESULT status;
  DEBUG_LEVEL2 fprintf(stderr,"MMS_Audio_Kill\n");

  MMS_Audio_Off(0);
  xa_audio_present = XA_AUDIO_UNK;

  status = waveOutReset(mms_device_handle);
  if( status != MMSYSERR_NOERROR ) {
      fprintf(stderr,"waveOutReset failed - status = %d\n", status);
  }
  status = waveOutClose(mms_device_handle);
  if( status != MMSYSERR_NOERROR ) {
      fprintf(stderr,"waveOutClose failed - status = %d\n", status);
  }

  mmeFreeBuffer(mms_audio_buffer);
  mmeFreeMem(mms_lpWaveHeader);

  Kill_Audio_Ring();
}

/********** MMS_Audio_Off **********************
 * Stop Audio Stream
 *
 *****/
void MMS_Audio_Off(flag)
xaULONG flag;
{
  DEBUG_LEVEL2 fprintf(stderr,"MMS_Audio_Off\n");
  if (xa_audio_status == XA_AUDIO_STOPPED) return;

  /* Just set our flag and let things drain */
  /* we could use waveOutReset if we were in a hurry */
  xa_audio_status = XA_AUDIO_STOPPED;
}

/********** MMS_Audio_Prep **********************
 * Turn On Audio Stream.
 *
 *****/
/*POD TEMP
static void MMS_wave_callback(HANDLE hWaveOut,
			      UINT wMsg,
			      DWORD dwInstance,
			      LPARAM lParam1,
			      LPARAM lParam2)
*/
static void MMS_wave_callback(hWaveOut,wMsg,dwInstance,lParam1,lParam2)
HANDLE hWaveOut;
UINT wMsg;
DWORD dwInstance;
LPARAM lParam1;
LPARAM lParam2;
{
    switch(wMsg)
      {
	  case WOM_OPEN:
	  case WOM_CLOSE: {
	      /* Ignore these */
	      break;
	  }
	  case WOM_DONE: {
	      LPWAVEHDR lpwh = (LPWAVEHDR)lParam1;
	      mms_buffers_outstanding--;
	      break;
	  }
	  default: {
	      fprintf(stderr,
		      "Unknown MMS waveOut callback messages receieved.\n");
	      break;
	  }
      }
}

void MMS_Audio_Prep()
{
  MMRESULT status;
  LPPCMWAVEFORMAT lpWaveFormat;

  DEBUG_LEVEL2 fprintf(stderr,"MMS_Audio_Prep \n");
  if (xa_audio_status == XA_AUDIO_STARTED) return;
  else if (xa_audio_present != XA_AUDIO_OK) return;
  else if (xa_snd_cur)
  {
    /* Close & Reopen device if necessary */
    if (xa_audio_hard_freq != xa_snd_cur->hfreq )
    {
	if( mms_device_handle != 0 ) {
	    status = waveOutReset(mms_device_handle);
	    if( status != MMSYSERR_NOERROR ) {
		fprintf(stderr,"waveOutReset failed - status = %d\n", status);
	    }
	    status = waveOutClose(mms_device_handle);
	    if( status != MMSYSERR_NOERROR ) {
		fprintf(stderr,"waveOutClose failed - status = %d\n", status);
	    }
	}
	if((lpWaveFormat = (LPPCMWAVEFORMAT)
			mmeAllocMem(sizeof(PCMWAVEFORMAT))) == NULL ) 
	{
	    fprintf(stderr,"Failed to allocate PCMWAVEFORMAT struct\n");
	    return;
	}
	lpWaveFormat->wf.nSamplesPerSec = xa_snd_cur->hfreq;

	lpWaveFormat->wf.nChannels = xa_audio_hard_chans;
	lpWaveFormat->wBitsPerSample = xa_audio_hard_bps*8/xa_audio_hard_chans;
	if( xa_audio_hard_type == XA_AUDIO_SUN_AU )
	  lpWaveFormat->wf.wFormatTag = WAVE_FORMAT_MULAW;
	else if((xa_audio_hard_type & XA_AUDIO_TYPE_MASK) == XA_AUDIO_SIGNED ||
		(xa_audio_hard_type & XA_AUDIO_TYPE_MASK) == XA_AUDIO_LINEAR )
	  lpWaveFormat->wf.wFormatTag = WAVE_FORMAT_PCM;
	else {
	    fprintf(stderr,"Unsupported audio format\n");
	    return;
	}

	/* The audio format to convert to has already been picked based on what
	   we said the hardware capabilities were, so don't change now or we'll
	   confuse everyone.  If we were going to take maximum advantage of the
	   MMS & hardware capabilities, we'd do it something like this:
	*/
#if 0
	if (xa_audio_hard_freq != xa_snd_cur->hfreq ||
	    xa_audio_hard_type != xa_snd_cur->type );
	 
	if( xa_snd_cur->type & XA_AUDIO_STEREO_MSK )
	  lpWaveFormat->wf.nChannels = 2;
	else
	  lpWaveFormat->wf.nChannels = 1;
	if( xa_snd_cur->type & XA_AUDIO_BPS_2_MSK )
	  lpWaveFormat->wBitsPerSample = 16;
	else
	  lpWaveFormat->wBitsPerSample = 8;
	if( (xa_snd_cur->type & XA_AUDIO_TYPE_MASK) == XA_AUDIO_SUN_AU )
	  lpWaveFormat->wf.wFormatTag = WAVE_FORMAT_MULAW;
	else if(   (xa_snd_cur->type & XA_AUDIO_TYPE_MASK) == XA_AUDIO_SIGNED 
		|| (xa_snd_cur->type & XA_AUDIO_TYPE_MASK) == XA_AUDIO_LINEAR )
	  lpWaveFormat->wf.wFormatTag = WAVE_FORMAT_PCM;
	else if( (xa_snd_cur->type & XA_AUDIO_TYPE_MASK) == XA_AUDIO_ADPCM)
	  lpWaveFormat->wf.wFormatTag = WAVE_FORMAT_IMA_ADPCM;
	else {
	    fprintf(stderr,"Unsupported audio format\n");
	    return;
	}
#endif

	lpWaveFormat->wf.nBlockAlign = lpWaveFormat->wf.nChannels *
	                               ((lpWaveFormat->wBitsPerSample+7)/8);
	lpWaveFormat->wf.nAvgBytesPerSec = lpWaveFormat->wf.nBlockAlign *
	  				  lpWaveFormat->wf.nSamplesPerSec;

	/* Open the audio device in the appropriate rate/format */
	mms_device_handle = 0;
	status = waveOutOpen( &mms_device_handle,
			     WAVE_MAPPER,
			     (LPWAVEFORMAT)lpWaveFormat,
			     MMS_wave_callback,
			     NULL,
			     CALLBACK_FUNCTION | WAVE_OPEN_SHAREABLE );
	if( status != MMSYSERR_NOERROR ) {
	    fprintf(stderr,"waveOutOpen failed - status = %d\n", status);
	}

	/* Get & save the device ID for volume control */
	status = waveOutGetID( mms_device_handle, &mms_device_id );
	if( status != MMSYSERR_NOERROR ) {
	    fprintf(stderr,"waveOutGetID failed - status = %d\n", status);
	}

	DEBUG_LEVEL2 fprintf(stderr,"Opened waveOut device #%d \n",
		mms_device_id);
	DEBUG_LEVEL2 fprintf(stderr,
			     "Format=%d, Rate=%d, channels=%d, bps=%d \n",
			     lpWaveFormat->wf.wFormatTag,
			     lpWaveFormat->wf.nSamplesPerSec,
			     lpWaveFormat->wf.nChannels,
			     lpWaveFormat->wBitsPerSample );

	mmeFreeMem(lpWaveFormat);

	/* Allocate wave header for use in write */
	if((mms_lpWaveHeader = (LPWAVEHDR)
				mmeAllocMem(sizeof(WAVEHDR))) == NULL ) 
	{
	    fprintf(stderr,"Failed to allocate WAVEHDR struct\n");
	    return;
	}

	/* Allocate shared audio buffer for communicating with audio device */
	/* NOTE: It's just a matter of convenience that it's the same size  */
	/*	as  XAnim's audio ring buffer - don't confuse the two	    */
	if ( (mms_audio_buffer = (LPSTR)
	      mmeAllocBuffer(xa_audio_hard_buff*xa_audio_ring_size )) == NULL)
        {
	  fprintf(stderr,"Failed to allocate shared audio buffer\n");
	  return;
	}

	/* Save current hardware settings */
	xa_audio_hard_freq = xa_snd_cur->hfreq;
	xa_audio_hard_type = xa_snd_cur->type;
    }

    /* These values are all cut & paste from other devices - hope they got
       them right there! - tfm */
    xa_out_time = 100;
    xa_out_init = xa_audio_ring_size - 1;
    xa_interval_time = xa_snd_cur->ch_time / XAAUD->divtest;
    if (xa_interval_time == 0) xa_interval_time = 1;

    XA_Flush_Ring();
    XA_Update_Ring(1000);
    xa_audio_status = XA_AUDIO_PREPPED;
  }
}

/****-------------------------------------------------------------------****
 *
 ****-------------------------------------------------------------------****/
void MMS_Audio_On()
{
  if (   (xa_snd_cur)
      && (xa_audio_present == XA_AUDIO_OK)
      && (xa_audio_status == XA_AUDIO_PREPPED) )
  { 
    xa_audio_status = XA_AUDIO_STARTED;
    xa_time_now = XA_Read_AV_Time();  /* get new time */
    New_Merged_Audio_Output();
  }
}

/********** MMS_Closest_Freq **********************************************
 *
 * Global Variable Affect:
 *   xaULONG xa_audio_hard_buff		must set but not larger than
 *					XA_AUDIO_MAX_RING_BUF size
 ****************************************************************************/
xaULONG MMS_Closest_Freq(ifreq)
xaLONG ifreq;
{
  xa_audio_hard_buff = XA_HARD_BUFF_1K;

  /* Query our audio devices to see what sample rates are supported */

  if (xa_audio_hard_type == XA_AUDIO_SIGNED_2ML)
  { static int valid[] = { 8000, 11025, 16000, 22050, 32000, 44100, 48000, 0};
    xaLONG i = 0;
    xaLONG best = valid[0];

    while(valid[i])
    {
      if (xaABS(valid[i] - ifreq) < xaABS(best - ifreq)) best = valid[i];
      i++;
    }

    return(best);
  }
  else return(8000);
}

/* Select the appropriate output port */

void MMS_Set_Output_Port(aud_ports)
xaULONG aud_ports;
{
  MMRESULT status;
  DWORD ports = 0;
/* dummy definitions to support pseudo code below - needs to be fixed - tfm */
  int device_type = 0;
#define J300 0
#define BBA 1

  if (aud_ports & XA_AUDIO_PORT_INT &&
      device_type == BBA )
    ports |= MME_PORTMASK_02;
  if (aud_ports & XA_AUDIO_PORT_HEAD) {
      if(device_type = BBA)
	ports |= MME_PORTMASK_02;
      if(device_type = J300)
	ports |= MME_PORTMASK_01;
  }
  if (aud_ports & XA_AUDIO_PORT_EXT &&
      device_type == J300)
    ports |= MME_PORTMASK_02;

  status = waveOutSelectPorts(mms_device_id,ports);
  if( status != MMSYSERR_NOERROR ) {
      fprintf(stderr,"waveOutSelectPorts failed - status = %d\n", status); }
}



/*************
 * flag = 0  turn speaker off
 * flag = 1  turn speaker on
 * flag = 2  toggle speaker
 */
void MMS_Speaker_Toggle(flag)
xaULONG flag;
{
  DEBUG_LEVEL2 fprintf(stderr,"MMS_Speaker_Toggle\n");
}

void MMS_Headphone_Toggle(flag)
xaULONG flag;
{
  DEBUG_LEVEL2 fprintf(stderr,"MMS_Headphone_Toggle\n");
}

/********** MMS_Adjust_Volume **********************
 * Routine for adjusting output volume
 *
 * NOTE: It isn't really a good idea to do this on a per application
 *       basis, particularly at startup where we are going to override
 *       the value the user has set up
 *	 I've left this disabled for now
 *
 * POD NOTE: once xanim leaves default volume alone at startup, remember
 *           to renable this.
 *****/
void MMS_Adjust_Volume(volume)
xaULONG volume;
{
 DWORD vol;
 DEBUG_LEVEL2 fprintf(stderr,"MMS_Audio_Volume %d\n",volume);

 vol = (0xFFFF * volume) / 100;	/* convert to 16 bit scale */
 vol = (vol << 16) | vol;	/* duplicate for left & right channels */

#ifdef MMS_VOL
 waveOutSetVolume(mms_device_id, vol);
#endif

}

#endif
/****************************************************************************/
/************************* END OF MMS SPECIFIC ROUTINES *********************/
/****************************************************************************/

/****************************************************************************/
/**************** NAS SPECIFIC ROUTINES *************************************/
/****************************************************************************/
#ifdef XA_NAS_AUDIO

/*
 * NAS port provided by Bob Phillips,
 *	bobp@syl.nj.nec.com
 * Adapted from AF port by Tom Levergood
 * Wednesday October 26, 1994
 *
 * Re-written by Greg Renda (greg@ncd.com) 5/10/95
 */
  
static void		NAS_Audio_Init();
static void		NAS_Audio_Kill();
static void		NAS_Audio_Off();
static void		NAS_Audio_Prep();
static void		NAS_Audio_On();
static void		NAS_Adjust_Volume();
static xaULONG		NAS_Closest_Freq();
static void		NAS_Speaker_Toggle();
static void		NAS_Headphone_Toggle();

static xaULONG		NAS_pod_flag = 0;

typedef struct _NASChunk
{
    xaUBYTE          *buf;
    xaULONG           len,
                    offset;
    struct _NASChunk *next;
}               NASChunk, *NASChunkPtr;

typedef struct
{
    AuServer       *aud;
    AuFlowID        flow;
    AuDeviceID      device;
    NASChunkPtr     head,
                    tail;
    xaULONG           bytes;
    AuDeviceAttributes *da;
    AuEventHandlerRec *handler;
    AuBool flowRunning;
}               NASInfo, *NASInfoPtr;

static NASInfo  nas;

static AuBool
NAS_Event_Handler(aud, ev, handler)
AuServer       *aud;
AuEvent        *ev;
AuEventHandlerRec *handler;
{
    NASInfoPtr      np = (NASInfoPtr) handler->data;

    switch (ev->type)
    {
	case AuEventTypeElementNotify:
	    {
		AuElementNotifyEvent *event = (AuElementNotifyEvent *) ev;

		switch (event->kind)
		{
		    case AuElementNotifyKindLowWater:
			np->bytes += event->num_bytes;
			break;
		    case AuElementNotifyKindState:
			switch (event->cur_state)
			{
			    case AuStatePause:
				if (event->reason != AuReasonUser)
				    np->bytes += event->num_bytes;
				break;
			    case AuStateStop:
				np->flowRunning = AuFalse;
				break;
			}
		}
	    }
    }

    return AuTrue;
}

static void
NAS_Audio_Init()
{
    char           *errmsg;
    int             i;

    DEBUG_LEVEL2    fprintf(stdout, "NAS_Audio_Init\n");

    if (xa_audio_present != XA_AUDIO_UNK)
	return;

    /* Open connection. */
    if (!(nas.aud = AuOpenServer(NULL, 0, NULL, 0, NULL, &errmsg)))
    {
	fprintf(stderr, "Could not open connection to NAS audio server.\n");
	fprintf(stderr, "Error is \"%s\".\n", errmsg);
	fprintf(stderr, "Disabling audio service.\n");
	/*
	 * As long as xa_audio_present isn't OK, audio is effectively
	 * disabled
	 */
	/* but might as well really disable it */
	AuFree(errmsg);
	XA_Null_Audio_Setup();
	xa_audio_present = XA_AUDIO_UNK;
	return;
    }

    /* look for an output device */
    for (i = 0; i < AuServerNumDevices(nas.aud); i++)
	if ((AuDeviceKind(AuServerDevice(nas.aud, i)) ==
	     AuComponentKindPhysicalOutput) &&
	    AuDeviceNumTracks(AuServerDevice(nas.aud, i)) == 1)
	{
	    nas.device = AuDeviceIdentifier(AuServerDevice(nas.aud, i));
	    nas.da = AuServerDevice(nas.aud, i);
	    break;
	}

    if (i == AuServerNumDevices(nas.aud))
    {
	fprintf(stderr, "NAS: Couldn't find an appropriate output device\n");
	XA_Null_Audio_Setup();
	xa_audio_present = XA_AUDIO_UNK;
	return;
    }

    if (!(nas.flow = AuCreateFlow(nas.aud, NULL)))
    {
	fprintf(stderr, "NAS: Couldn't create flow\n");
	XA_Null_Audio_Setup();
	xa_audio_present = XA_AUDIO_UNK;
	return;
    }

    nas.handler = AuRegisterEventHandler(nas.aud, AuEventHandlerIDMask, 0,
					 nas.flow, NAS_Event_Handler,
					 (AuPointer) &nas);

    xa_audio_present = XA_AUDIO_OK;
    xa_audio_hard_type = XA_AUDIO_SIGNED_2MB;
    xa_audio_hard_bps = 2;
    xa_audio_hard_chans = 1;
    NAS_pod_flag = 0;
    Init_Audio_Ring(xa_audio_ring_size,
		    (XA_AUDIO_MAX_RING_BUFF * xa_audio_hard_bps));
}

static void
NAS_Audio_Kill()
{
    DEBUG_LEVEL2    fprintf(stdout, "NAS_Audio_Kill\n");

    NAS_Audio_Off(0);
    xa_audio_present = XA_AUDIO_UNK;

    AuUnregisterEventHandler(nas.aud, nas.handler);
    AuCloseServer(nas.aud);

    Kill_Audio_Ring();
}


static void
NAS_Audio_Off(flag)
xaULONG           flag;
{
    NASChunkPtr     p = nas.head,
                    next;

    if (NAS_pod_flag) return; NAS_pod_flag = 1; /* POD TEST */

    DEBUG_LEVEL2    fprintf(stdout, "NAS_Audio_Off\n");

    if (!nas.flowRunning && xa_audio_status != XA_AUDIO_STARTED)
	return;
/* POD new */
    xa_audio_status = XA_AUDIO_STOPPED;

    AuStopFlow(nas.aud, nas.flow, NULL);

    while (nas.flowRunning == AuTrue) AuHandleEvents(nas.aud);

    while (p)
    {
	next = p->next;
	free(p->buf);
	free(p);
	p = next;
    }

    nas.head = nas.tail = NULL;

    /* SET FLAG TO STOP OUTPUT ROUTINE */
/* POD Was
    xa_audio_status = XA_AUDIO_STOPPED;
*/

    xa_time_audio = -1;
    xa_audio_flushed = 0;

    NAS_pod_flag = 0; /* POD TEST */
}

static void
NAS_Audio_Prep()
{
    if (NAS_pod_flag) return; NAS_pod_flag = 1; /* POD TEST */

    DEBUG_LEVEL2    fprintf(stdout, "NAS_Audio_Prep \n");

    if ( (xa_audio_status == XA_AUDIO_STARTED) ||
	 (xa_audio_present != XA_AUDIO_OK) || (!xa_snd_cur) )
	return;

    if (nas.flowRunning == AuTrue) NAS_Audio_Off(0);

#define NASBufSize 30000
    /* Change Frequency If necessary */
    if (xa_audio_hard_freq != xa_snd_cur->hfreq)
    {
	AuElement       elements[3];

	AuMakeElementImportClient(&elements[0], xa_snd_cur->hfreq,
				  AuFormatLinearSigned16MSB,
				  1, AuTrue, NASBufSize,
				  NASBufSize / 4 * 3, 0, NULL);
	AuMakeElementMultiplyConstant(&elements[1], 0,
				      AuFixedPointFromFraction(100, 100));
	AuMakeElementExportDevice(&elements[2], 1, nas.device,
				  xa_snd_cur->hfreq, AuUnlimitedSamples,
				  0, NULL);

	AuSetElements(nas.aud, nas.flow, AuTrue, 3, elements, NULL);

	xa_audio_hard_freq = xa_snd_cur->hfreq;
    }

    nas.bytes = 0;
    AuStartFlow(nas.aud, nas.flow, NULL);
    nas.flowRunning = AuTrue;

    /* xa_snd_cur gets changes in Update_Ring() */
    /* FINE TUNED from 4 to 20 */
    xa_out_time = 250;			       /* keep audio fed XXms ahead
					        * of video - could be 500 */
    xa_out_init = xa_audio_ring_size - 1;
    xa_interval_time = xa_snd_cur->ch_time / XAAUD->divtest;
    if (xa_interval_time == 0) xa_interval_time = 1;

    XA_Flush_Ring();
    XA_Update_Ring(1000);
    xa_audio_status = XA_AUDIO_PREPPED;
    NAS_pod_flag = 0;
}

/****-------------------------------------------------------------------****
 *
 ****-------------------------------------------------------------------****/
void NAS_Audio_On()
{
  if (   (xa_snd_cur)
      && (xa_audio_present == XA_AUDIO_OK)
      && (xa_audio_status == XA_AUDIO_PREPPED) )
  { 
    xa_audio_status = XA_AUDIO_STARTED;
    xa_time_now = XA_Read_AV_Time();  /* get new time */
    New_Merged_Audio_Output();
  }
}

static void
NAS_Toggle_OutputMode(flag, mode)
xaULONG flag, mode;
{
    if (!(AuDeviceChangableMask(nas.da) & AuCompDeviceOutputModeMask))
	return;

    switch(flag)
    {
	case 0:
	    AuDeviceOutputMode(nas.da) &= ~mode;
	    break;
	case 1:
	    AuDeviceOutputMode(nas.da) |= mode;
	    break;
  	case 2:
	    AuDeviceOutputMode(nas.da) ^= mode;
	    break;
    }

    AuSetDeviceAttributes(nas.aud, AuDeviceIdentifier(nas.da),
			  AuCompDeviceOutputModeMask, nas.da, NULL);
    AuFlush(nas.aud);
}

/*************
 * flag = 0  turn speaker off
 * flag = 1  turn speaker on
 * flag = 2  toggle speaker
 */
static void
NAS_Speaker_Toggle(flag)
xaULONG           flag;
{
    DEBUG_LEVEL2    fprintf(stdout, "NAS_Speaker_Toggle\n");
    NAS_Toggle_OutputMode(flag, AuDeviceOutputModeSpeaker);
}

static void
NAS_Headphone_Toggle(flag)
xaULONG           flag;
{
    DEBUG_LEVEL2    fprintf(stdout, "NAS_Headphone_Toggle\n");
    NAS_Toggle_OutputMode(flag, AuDeviceOutputModeHeadphone);
}


/********** NAS_Adjust_Volume **********************
 * Routine for Adjusting Volume on NAS
 *
 *****/
static void
NAS_Adjust_Volume(volume)
xaULONG           volume;
{
    AuElementParameters parms;

    DEBUG_LEVEL2    fprintf(stdout, "NAS_Audio_Volume %d\n", volume);
    parms.flow = nas.flow;
    parms.element_num = 1;
    parms.num_parameters = AuParmsMultiplyConstant;
    parms.parameters[AuParmsMultiplyConstantConstant] =
	AuFixedPointFromFraction(volume, 100);
    AuSetElementParameters(nas.aud, 1, &parms, NULL);
    AuFlush(nas.aud);
}

/********** NAS_Closest_Freq **********************
 *
 * Global Variable Affect:
 *   xaULONG xa_audio_hard_buff		must set but not larger than
 *					XA_AUDIO_MAX_RING_BUF size
 */
static xaULONG
NAS_Closest_Freq(ifreq)
xaLONG            ifreq;
{
    xa_audio_hard_buff = XA_AUDIO_MAX_RING_BUFF;
    return ifreq;
}

static void
NAS_Write_Data(buf, len)
xaUBYTE          *buf;
xaULONG           len;
{
    NASChunkPtr     p;

    if (!(p = (NASChunkPtr) malloc(sizeof(NASChunk))))
	return;

    if (!(p->buf = (xaUBYTE *) malloc(len)))
    {
	free(p);
	return;
    }

    memcpy((char *) p->buf, (char *) buf, (int) len);
    p->len = len;
    p->offset = 0;
    p->next = NULL;

    if (nas.tail)
    {
	nas.tail->next = p;
	nas.tail = p;
    }
    else
	nas.head = nas.tail = p;

    AuHandleEvents(nas.aud);

    while (nas.head && nas.bytes && (xa_audio_status == XA_AUDIO_STARTED) )
    {
	int             n = xaMIN(nas.bytes, nas.head->len);
	AuWriteElement(nas.aud, nas.flow, 0, n,
		       nas.head->buf + nas.head->offset, AuFalse, NULL);

	nas.bytes -= n;
	nas.head->len -= n;
	nas.head->offset += n;

	if (!nas.head->len)
	{
	    NASChunkPtr     next = nas.head->next;

	    free(nas.head->buf);
	    free(nas.head);

	    if (!(nas.head = next))
		nas.tail = nas.head;
	}

	AuHandleEvents(nas.aud);
    }
}

void
XA_Audio_Setup()
{
    DEBUG_LEVEL2    fprintf(stdout, "XA_Audio_Setup()\n");
    XA_Audio_Init = NAS_Audio_Init;
    XA_Audio_Kill = NAS_Audio_Kill;
    XA_Audio_Off = NAS_Audio_Off;
    XA_Audio_Prep = NAS_Audio_Prep;
    XA_Audio_On = NAS_Audio_On;
    XA_Closest_Freq = NAS_Closest_Freq;
    XA_Set_Output_Port = (void *) (0);
    XA_Speaker_Tog = NAS_Speaker_Toggle;
    XA_Headphone_Tog = NAS_Headphone_Toggle;
    XA_LineOut_Tog = NAS_Headphone_Toggle;
    XA_Adjust_Volume = NAS_Adjust_Volume;

    xa_snd_cur = 0;
    xa_audio_present = XA_AUDIO_UNK;
    xa_audio_status = XA_AUDIO_STOPPED;
    xa_audio_ring_size = 8;
}

#endif
/****************************************************************************/
/******************* END OF NAS SPECIFIC ROUTINES ***************************/
/****************************************************************************/


/****************************************************************************/
/**************** NetBSD SPECIFIC ROUTINES **********************************/
/****************************************************************************/

/*
 * NetBSD port provided by Roland C Dowdeswell
 * roland@imrryr.org
 * Heavily stolen from the Sparc port (like the others)
 * Tuesday 9/May 1995 -- very early -- still dark.
 */

#ifdef XA_NetBSD_AUDIO

void  NetBSD_Audio_Init();
void  NetBSD_Audio_Kill();
void  NetBSD_Audio_Off();
void  NetBSD_Audio_Prep();
void  NetBSD_Audio_On();
void  NetBSD_Adjust_Volume();
xaULONG NetBSD_Closest_Freq();
void NetBSD_Set_Output_Port();
void NetBSD_Speaker_Toggle();
void NetBSD_Headphone_Toggle();

#define NetBSD_MAX_VOL AUDIO_MAX_GAIN
#define NetBSD_MIN_VOL AUDIO_MIN_GAIN

static int devAudio;

/********** XA_Audio_Setup **********************
 * 
 * Also defines NetBSD Specific variables.
 *
 *****/
void XA_Audio_Setup()
{
  XA_Audio_Init		= NetBSD_Audio_Init;
  XA_Audio_Kill		= NetBSD_Audio_Kill;
  XA_Audio_Off		= NetBSD_Audio_Off;
  XA_Audio_Prep		= NetBSD_Audio_Prep;
  XA_Audio_On		= NetBSD_Audio_On;
  XA_Closest_Freq	= NetBSD_Closest_Freq;
  XA_Set_Output_Port	= NetBSD_Set_Output_Port;
  XA_Speaker_Tog	= NetBSD_Speaker_Toggle;
  XA_Headphone_Tog	= NetBSD_Headphone_Toggle;
  XA_LineOut_Tog	= NetBSD_Headphone_Toggle;
  XA_Adjust_Volume	= NetBSD_Adjust_Volume;

  xa_snd_cur = 0;
  xa_audio_present = XA_AUDIO_UNK;
  xa_audio_status  = XA_AUDIO_STOPPED;
  xa_audio_ring_size  = 8;
}


/********** NetBSD_Audio_Init **********************
 * Open /dev/audio and NetBSD.
 *
 *****/
void NetBSD_Audio_Init()
{ int ret;
  int type;
  audio_info_t a_info;
  DEBUG_LEVEL2 fprintf(stderr,"NetBSD_Audio_Init\n");
  if (xa_audio_present != XA_AUDIO_UNK) return;
  devAudio = open("/dev/audio", O_WRONLY | O_NDELAY);
  if (devAudio == -1)
  {
    if (errno == EBUSY) fprintf(stderr,"Audio_Init: Audio device is busy. - ");
    else fprintf(stderr,"Audio_Init: Error opening audio device. - ");
    fprintf(stderr,"Will continue without audio\n");
    xa_audio_present = XA_AUDIO_ERR;
    return;
  }

  DEBUG_LEVEL1 fprintf(stderr,"NetBSD AUDIO\n");

  AUDIO_INITINFO(&a_info);
  a_info.blocksize = 1024;
  ioctl(devAudio, AUDIO_SETINFO, &a_info);
  AUDIO_INITINFO(&a_info);

  a_info.mode = AUMODE_PLAY | AUMODE_PLAY_ALL;
  ioctl(devAudio, AUDIO_SETINFO, &a_info);
#ifdef AUDIO_ENCODING_SLINEAR
  /* Use new encoding names */
  AUDIO_INITINFO(&a_info);
  a_info.play.encoding = AUDIO_ENCODING_SLINEAR;
  a_info.play.precision = 16;
  if ( ioctl(devAudio, AUDIO_SETINFO, &a_info) < 0)
  {
    AUDIO_INITINFO(&a_info);
    a_info.play.encoding = AUDIO_ENCODING_ULINEAR;
    a_info.play.precision = 8;
    ioctl(devAudio, AUDIO_SETINFO, &a_info);
  }
#else
  AUDIO_INITINFO(&a_info);
  a_info.play.encoding = AUDIO_ENCODING_PCM16;
  a_info.play.precision = 16;
  if (ioctl(devAudio, AUDIO_SETINFO, &a_info) < 0)
  {
    AUDIO_INITINFO(&a_info);
    a_info.play.encoding = AUDIO_ENCODING_PCM;
    a_info.play.precision = 8;
    ioctl(devAudio, AUDIO_SETINFO, &a_info);
  }
#endif
  AUDIO_INITINFO(&a_info);
  a_info.play.channels = /*2*/1;  /* Eventually test for and support stereo */
  ioctl(devAudio, AUDIO_SETINFO, &a_info);

  AUDIO_INITINFO(&a_info);
  a_info.play.sample_rate = 11025;   /* this is changed later */
  ioctl(devAudio, AUDIO_SETINFO, &a_info);
  ioctl(devAudio, AUDIO_GETINFO, &a_info);

/* Eventually support stereo 
  if (a_info.play.channels == 2)
    if (a_info.play.precision == 8)
      xa_audio_hard_type = XA_AUDIO_LINEAR_1S;
    else
      xa_audio_hard_type = XA_AUDIO_SIGNED_2SL;
  else
*/
    if (a_info.play.precision == 8)
      xa_audio_hard_type = XA_AUDIO_LINEAR_1M;
    else
      xa_audio_hard_type = XA_AUDIO_SIGNED_2ML;

  xa_audio_hard_freq  = a_info.play.sample_rate;
  xa_audio_hard_buff  = a_info.blocksize;

	/* only precision 8 and 16 are supported. Fail if otherwise?? */
  xa_audio_hard_bps   = (a_info.play.precision==8)?1:2;
  xa_audio_hard_chans = a_info.play.channels;
  Gen_uLaw_2_Signed();
  Gen_Signed_2_uLaw();

  xa_interval_id = 0;
  xa_audio_present = XA_AUDIO_OK;
  DEBUG_LEVEL2 fprintf(stderr,"   success \n");
  Init_Audio_Ring(xa_audio_ring_size,
			(XA_AUDIO_MAX_RING_BUFF * xa_audio_hard_bps) );
}

/********** NetBSD_Audio_Kill **********************
 * Close /dev/audio.
 *
 *****/
void NetBSD_Audio_Kill()
{ 
  /* TURN AUDIO OFF */
  NetBSD_Audio_Off(0);
  xa_audio_present = XA_AUDIO_UNK;
  /* SHUT THINGS DOWN  */
  close(devAudio);
  Kill_Audio_Ring();
}

/********** NetBSD_Audio_Off **********************
 * Stop Audio Stream
 *
 *****/
void NetBSD_Audio_Off(flag)
xaULONG flag;
{ /* long ret; */

  DEBUG_LEVEL1 fprintf(stderr,"NetBSD_Audio_Off\n");
  if (xa_audio_status != XA_AUDIO_STARTED) return;

  /* SET FLAG TO STOP OUTPUT ROUTINE */
  xa_audio_status = XA_AUDIO_STOPPED;

  /* TURN OFF SOUND ??? */
  NetBSD_Adjust_Volume(XA_AUDIO_MINVOL);

  /* FLUSH AUDIO DEVICE */ /* NOT! */
/*
  ret = ioctl(devAudio, AUDIO_FLUSH, NULL);
  if (ret == -1) fprintf(stderr,"NetBSD Audio: off flush err %d\n",errno);
*/

  xa_time_audio = -1;
  xa_audio_flushed = 0;

  /* FLUSH AUDIO DEVICE AGAIN */ /* NOT! */
/*
  ret = ioctl(devAudio, AUDIO_FLUSH, NULL);
  if (ret == -1) fprintf(stderr,"NetBSD Audio: off flush err %d\n",errno);
*/

  /* RESTORE ORIGINAL VOLUME */
  if (XAAUD->mute != xaTRUE) NetBSD_Adjust_Volume(XAAUD->volume);
}

/********** NetBSD_Audio_Prep **********************
 * Turn On Audio Stream.
 *
 *****/
void NetBSD_Audio_Prep()
{
  DEBUG_LEVEL2 
  {
    fprintf(stderr,"NetBSD_Audio_Prep \n");
  }
  if (xa_audio_status == XA_AUDIO_STARTED) return;
  else if (xa_audio_present != XA_AUDIO_OK) return;

  else if (xa_snd_cur)
  { int ret;

    /* CHANGE FREQUENCY IF NEEDED */
    if (xa_audio_hard_freq != xa_snd_cur->hfreq)
    { audio_info_t a_info;
      AUDIO_INITINFO(&a_info);
      a_info.play.sample_rate = xa_snd_cur->hfreq;
      ret = ioctl(devAudio, AUDIO_SETINFO, &a_info);
      if (ret == -1) fprintf(stderr,"audio setfreq: freq %x errno %d\n",
						xa_snd_cur->hfreq, errno);
      xa_audio_hard_freq = xa_snd_cur->hfreq;
    }

    /* xa_snd_cur gets changes in Update_Ring() */
    xa_out_time = 100;  /* keep audio fed 500ms ahead of video */  /* was 500, changed it to 100 - rcd */
    xa_out_init = xa_audio_ring_size - 1;
    xa_interval_time = xa_snd_cur->ch_time / XAAUD->divtest;
    if (xa_interval_time == 0) xa_interval_time = 1;

    XA_Flush_Ring();
    XA_Update_Ring(1000);
    xa_audio_status = XA_AUDIO_PREPPED;
  }
}

/****-------------------------------------------------------------------****
 *
 ****-------------------------------------------------------------------****/
void NetBSD_Audio_On()
{
  if (   (xa_snd_cur)
      && (xa_audio_present == XA_AUDIO_OK)
      && (xa_audio_status == XA_AUDIO_PREPPED) )
  { 
    xa_audio_status = XA_AUDIO_STARTED;
    xa_time_now = XA_Read_AV_Time();  /* get new time */
    New_Merged_Audio_Output();
  }
}


/********** NetBSD_Closest_Freq **********************************************
 *
 * Global Variable Affect:
 *   xaULONG xa_audio_hard_buff		must set but not larger than
 *					XA_AUDIO_MAX_RING_BUF size
 ****************************************************************************/
xaULONG NetBSD_Closest_Freq(ifreq)
xaLONG ifreq;
{ audio_info_t a_info;

  AUDIO_INITINFO(&a_info);
  a_info.play.sample_rate = ifreq;
  ioctl(devAudio, AUDIO_SETINFO, &a_info);

  ioctl(devAudio, AUDIO_GETINFO, &a_info);

  xa_audio_hard_buff  = a_info.blocksize;
  return (a_info.play.sample_rate);
}


/* Eventually merge everything to one */
void NetBSD_Set_Output_Port(aud_ports)
xaULONG aud_ports;
{
/* Commented out for now ;-) */
/*
audio_info_t a_info;
  xaLONG ret;
  xaULONG NetBSD_ports = 0;
  if (aud_ports & XA_AUDIO_PORT_INT)  NetBSD_ports |= AUDIO_SPEAKER;
  if (aud_ports & XA_AUDIO_PORT_HEAD) NetBSD_ports |= AUDIO_HEADPHONE;
  if (aud_ports & XA_AUDIO_PORT_EXT)  NetBSD_ports |= AUDIO_LINE_OUT;
  AUDIO_INITINFO(&a_info);
  a_info.play.port = NetBSD_ports;
  ret = ioctl(devAudio, AUDIO_SETINFO, &a_info);
  if (ret < 0) fprintf(stderr,"Audio: couldn't set speaker port %d\n",errno);
*/
}

/************* NetBSD_Speaker_Toggle *****************************************
 *
 * flag = 0  turn speaker off
 * flag = 1  turn speaker on
 * flag = 2  toggle speaker
 ****************************************************************************/
void NetBSD_Speaker_Toggle(flag)
xaULONG flag;
{ 
  switch(flag)
  {
    case  0: XAAUD->port &= ~XA_AUDIO_PORT_INT; break;
    case  1: XAAUD->port |=  XA_AUDIO_PORT_INT; break;
    default:  /* mutually exclusive set for now - never turn off */
    { if ( !(XAAUD->port & XA_AUDIO_PORT_INT)) 
		XAAUD->port = XA_AUDIO_PORT_INT;
    }
  }
  NetBSD_Set_Output_Port(XAAUD->port);
}

/************* NetBSD_Headphone_Toggle *****************************************
 *
 * flag = 0  turn headphones off
 * flag = 1  turn headphones on
 * flag = 2  toggle headphones
 ****************************************************************************/
void NetBSD_Headphone_Toggle(flag)
xaULONG flag;
{ 
  switch(flag)
  {
    case  0: XAAUD->port &= ~XA_AUDIO_PORT_HEAD; break;
    case  1: XAAUD->port |=  XA_AUDIO_PORT_HEAD; break;
    default:  /* mutually exclusive set for now - never turn off */
    { if ( !(XAAUD->port & XA_AUDIO_PORT_HEAD)) 
		XAAUD->port = XA_AUDIO_PORT_HEAD;
    }
  }
  NetBSD_Set_Output_Port(XAAUD->port);
}


/********** NetBSD_Adjust_Volume **********************
 * Routine for Adjusting Volume on NetBSD
 *
 * Volume is in the range [0,XA_AUDIO_MAXVOL]
 ****************************************************************************/
void NetBSD_Adjust_Volume(volume)
xaULONG volume;
{ audio_info_t a_info;

  AUDIO_INITINFO(&a_info);
  a_info.play.gain = NetBSD_MIN_VOL +
	((volume * (NetBSD_MAX_VOL - NetBSD_MIN_VOL)) / XA_AUDIO_MAXVOL);
  if (a_info.play.gain > NetBSD_MAX_VOL) a_info.play.gain = NetBSD_MAX_VOL;
  ioctl(devAudio, AUDIO_SETINFO, &a_info);

}
#endif
/****************************************************************************/
/******************* END OF NetBSD SPECIFIC ROUTINES ************************/
/****************************************************************************/

 /****************************************************************************/
/**************** TOWNS SPECIFIC ROUTINES ***********************************/
/****************************************************************************/
#ifdef XA_TOWNS_AUDIO

/*
 * FUJITSU FM-TOWNS  port provided by Osamu KURATI,
 *	kurati@bigfoot.com
 * Heavily stolen from the Sparc port
 * Sun Sep. 2, 1997
 *
 */

static int auTownsSamplingRate[12]={
	48000,44100,32000,22050,
	19200,16000,11025, 9600,
	 8000, 5512,24000,12000
	};

void  Towns_Audio_Init();
void  Towns_Audio_Kill();
void  Towns_Audio_Off();
void  Towns_Audio_Prep();
void  Towns_Audio_On();
void  Towns_Adjust_Volume();
xaULONG Towns_Closest_Freq();
void Towns_Set_Output_Port();
void Towns_Speaker_Toggle();
void Towns_Headphone_Toggle();


static int devAudio;


/********** XA_Audio_Setup **********************
 * 
 * Also defines Towns Specific variables.
 *
 *****/
void XA_Audio_Setup()
{

  XA_Audio_Init		= Towns_Audio_Init;
  XA_Audio_Kill		= Towns_Audio_Kill;
  XA_Audio_Off		= Towns_Audio_Off;
  XA_Audio_Prep		= Towns_Audio_Prep;
  XA_Audio_On		= Towns_Audio_On;
  XA_Closest_Freq	= Towns_Closest_Freq;
  XA_Set_Output_Port	= (void *)(0);
  XA_Speaker_Tog	= Towns_Speaker_Toggle;
  XA_Headphone_Tog	= Towns_Headphone_Toggle;
  XA_LineOut_Tog	= Towns_Headphone_Toggle;
  XA_Adjust_Volume	= Towns_Adjust_Volume;

  xa_snd_cur = 0;
  xa_audio_present = XA_AUDIO_UNK;
  xa_audio_status  = XA_AUDIO_STOPPED;
  xa_audio_ring_size  = 8;
}


/********** Towns_Audio_Init **********************
 * Open /dev/pcm16 for Towns's.
 *
 *****/
void Towns_Audio_Init()
{ int ret;
  int type;

  DEBUG_LEVEL2 fprintf(stderr,"Towns_Audio_Init\n");
  if (xa_audio_present != XA_AUDIO_UNK) return;
  devAudio = open("/dev/pcm16",O_WRONLY | O_NDELAY);
  if (devAudio == -1)
  {
    if (errno == EBUSY) fprintf(stderr,"Audio_Init: Audio device is busy. - ");
    else fprintf(stderr,"Audio_Init: Error %x opening audio device. - ",errno);
    fprintf(stderr,"Will continue without audio\n");
    xa_audio_present = XA_AUDIO_ERR;
    return;
  }

    ioctl(devAudio,PCM16_CTL_SNDWAV,0);  /* select  wav */

    /***** 8000 Hz : cnt == 8 *****/
    /***** 22050 Hz : cnt == 3 *****/
    ioctl(devAudio,PCM16_CTL_RATE,3); /* sampling rate  set */

    /***** monoral *****/
    ioctl(devAudio,PCM16_CTL_STMONO,0); /* monoral=0 */

    ioctl(devAudio,PCM16_CTL_BITS,1);   /* 16bit data */

    xa_audio_hard_freq  = 22050;
    xa_audio_hard_type  = XA_AUDIO_SIGNED_2ML;
    xa_audio_hard_buff  = XA_HARD_BUFF_1K;             /* default buffer size */
    xa_audio_hard_bps   = 2;
    xa_audio_hard_chans = 1;

  xa_interval_id = 0;
  xa_audio_present = XA_AUDIO_OK;
  DEBUG_LEVEL2 fprintf(stderr,"   success \n");
  Init_Audio_Ring(xa_audio_ring_size,
			(XA_AUDIO_MAX_RING_BUFF * xa_audio_hard_bps) );
}

/********** Towns_Audio_Kill **********************
 * Close /dev/pcm16
 *
 *****/
void Towns_Audio_Kill()
{ 
  /* TURN AUDIO OFF */
  Towns_Audio_Off(0);
  xa_audio_present = XA_AUDIO_UNK;
  /* SHUT THINGS DOWN  */
  close(devAudio);
  Kill_Audio_Ring();
}

/********** Towns_Audio_Off **********************
 * Stop Audio Stream
 *
 *****/
void Towns_Audio_Off(flag)
xaULONG flag;
{ long ret;

  if (xa_audio_status != XA_AUDIO_STARTED) return;

  /* SET FLAG TO STOP OUTPUT ROUTINE */
  xa_audio_status = XA_AUDIO_STOPPED;

  /* TURN OFF SOUND ??? */
  /*
  Towns_Adjust_Volume(XA_AUDIO_MINVOL);
  */

  /* FLUSH AUDIO DEVICE */
  /***** none *****/


  xa_time_audio = -1;
  xa_audio_flushed = 0;

  /* FLUSH AUDIO DEVICE AGAIN */
  /***** none *****/

  /* RESTORE ORIGINAL VOLUME */
  /*
  if (XAAUD->mute != xaTRUE) Towns_Adjust_Volume(XAAUD->volume);
  */
}

/********** Towns_Audio_Prep **********************
 * Turn On Audio Stream.
 *
 *****/
void Towns_Audio_Prep()
{ 
  DEBUG_LEVEL2 
  {
    fprintf(stderr,"Towns_Audio_Prep \n");
  }
  if (xa_audio_status == XA_AUDIO_STARTED) return;
  else if (xa_audio_present != XA_AUDIO_OK) return;
  else if (xa_snd_cur)
  { int ret;

    /* CHANGE FREQUENCY IF NEEDED */
    if (xa_audio_hard_freq != xa_snd_cur->hfreq)
    {
       int i_;

       ret = -1;
       for(i_=0; i_<12; i_++){
	  if (xa_snd_cur->hfreq == auTownsSamplingRate[i_]) {
	     ret = i_;
	  }
       }
       if (ret == -1) fprintf(stderr,"audio setfreq: freq %x can't set\n",
			      xa_snd_cur->hfreq);
       xa_audio_hard_freq = xa_snd_cur->hfreq;
    }

    /* xa_snd_cur gets changes in Update_Ring() */
    xa_out_time = 500;  /* keep audio fed 500ms ahead of video */
    xa_out_init = xa_audio_ring_size - 1;
    xa_interval_time = xa_snd_cur->ch_time / XAAUD->divtest;
    if (xa_interval_time == 0) xa_interval_time = 1;

    XA_Flush_Ring();
    XA_Update_Ring(1000);
    xa_audio_status = XA_AUDIO_PREPPED;
  }
}

/****-------------------------------------------------------------------****
 *
 ****-------------------------------------------------------------------****/
void Towns_Audio_On()
{
  if (   (xa_snd_cur)
      && (xa_audio_present == XA_AUDIO_OK)
      && (xa_audio_status == XA_AUDIO_PREPPED) )
  { 
    xa_audio_status = XA_AUDIO_STARTED;
    xa_time_now = XA_Read_AV_Time();  /* get new time */
    New_Merged_Audio_Output();
  }
}


/********** Towns_Closest_Freq **********************************************
 *
 * Global Variable Affect:
 *   xaULONG xa_audio_hard_buff		must set but not larger than
 *					XA_AUDIO_MAX_RING_BUF size
 ****************************************************************************/
xaULONG Towns_Closest_Freq(ifreq)
xaLONG ifreq;
{

   static int valid[] = {5512, 8000, 9600,
			  11025, 12000, 16000, 19200,
			  22050, 24000, 32000, 44100, 48000,
			  0};

    xaLONG i = 0;
    xaLONG best = 22050;

    xa_audio_hard_buff = XA_HARD_BUFF_1K;
    while(valid[i])
    { 
      if (xaABS(valid[i] - ifreq) < xaABS(best - ifreq)) best = valid[i];
      i++;
    }
    return(best);
}

/************* Towns_Speaker_Toggle *****************************************
 *
 * flag = 0  turn speaker off
 * flag = 1  turn speaker on
 * flag = 2  toggle speaker
 ****************************************************************************/
void Towns_Speaker_Toggle(flag)
xaULONG flag;
{ 
   return;
}

/************* Towns_Headphone_Toggle *****************************************
 *
 * flag = 0  turn headphones off
 * flag = 1  turn headphones on
 * flag = 2  toggle headphones
 ****************************************************************************/
void Towns_Headphone_Toggle(flag)
xaULONG flag;
{ 
   return;
}


/********** Towns_Adjust_Volume **********************
 * Routine for Adjusting Volume on a Towns
 *
 * Volume is in the range [0,XA_AUDIO_MAXVOL]
 ****************************************************************************/
void Towns_Adjust_Volume(volume)
xaULONG volume;
{
   return;
}
#endif
/****************************************************************************/
/******************* END OF TOWNS SPECIFIC ROUTINES *************************/
/****************************************************************************/

/****************************************************************************/
/**************** TOWNS 8 bit SPECIFIC ROUTINES *****************************/
/****************************************************************************/
#ifdef XA_TOWNS8_AUDIO

/*
 * FUJITSU FM-TOWNS  port provided by Osamu KURATI,
 *	kurati@bigfoot.com
 * Heavily stolen from the Sparc port
 * Sun Sep. 2, 1997
 *
 */

void  Towns8_Audio_Init();
void  Towns8_Audio_Kill();
void  Towns8_Audio_Off();
void  Towns8_Audio_Prep();
void  Towns8_Audio_On();
void  Towns8_Adjust_Volume();
xaULONG Towns8_Closest_Freq();
void Towns8_Set_Output_Port();
void Towns8_Speaker_Toggle();
void Towns8_Headphone_Toggle();


static int devAudio;


/********** XA_Audio_Setup **********************
 * 
 * Also defines Towns8 Specific variables.
 *
 *****/
void XA_Audio_Setup()
{

  XA_Audio_Init		= Towns8_Audio_Init;
  XA_Audio_Kill		= Towns8_Audio_Kill;
  XA_Audio_Off		= Towns8_Audio_Off;
  XA_Audio_Prep		= Towns8_Audio_Prep;
  XA_Audio_On		= Towns8_Audio_On;
  XA_Closest_Freq	= Towns8_Closest_Freq;
  XA_Set_Output_Port	= (void *)(0);
  XA_Speaker_Tog	= Towns8_Speaker_Toggle;
  XA_Headphone_Tog	= Towns8_Headphone_Toggle;
  XA_LineOut_Tog	= Towns8_Headphone_Toggle;
  XA_Adjust_Volume	= Towns8_Adjust_Volume;

  xa_snd_cur = 0;
  xa_audio_present = XA_AUDIO_UNK;
  xa_audio_status  = XA_AUDIO_STOPPED;
  xa_audio_ring_size  = 8;
}


/********** Towns8_Audio_Init **********************
 * Open /dev/pcm for Towns 8 bit PCM
 *
 *****/
void Towns8_Audio_Init()
{ int ret;
  int type;

  DEBUG_LEVEL2 fprintf(stderr,"Towns8_Audio_Init\n");
  if (xa_audio_present != XA_AUDIO_UNK) return;
  devAudio = open("/dev/pcm",O_WRONLY | O_NDELAY);
  if (devAudio == -1)
  {
    if (errno == EBUSY) fprintf(stderr,"Audio_Init: Audio device is busy. - ");
    else fprintf(stderr,"Audio_Init: Error %x opening audio device. - ",errno);
    fprintf(stderr,"Will continue without audio\n");
    xa_audio_present = XA_AUDIO_ERR;
    return;
  }

  /***** 19200 Hz *****/
  {
     short s_;
     s_ = 1920;
     write(devAudio,&s_,sizeof(s_));
  }

    xa_audio_hard_freq  = 19200;
    xa_audio_hard_type  = XA_AUDIO_SIGNED_2MB;
    xa_audio_hard_buff  = XA_HARD_BUFF_1K;             /* default buffer size */
    xa_audio_hard_bps   = 2;
    xa_audio_hard_chans = 1;

  xa_interval_id = 0;
  xa_audio_present = XA_AUDIO_OK;
  DEBUG_LEVEL2 fprintf(stderr,"   success \n");
  Init_Audio_Ring(xa_audio_ring_size,
			(XA_AUDIO_MAX_RING_BUFF * xa_audio_hard_bps) );
}

/********** Towns8_Audio_Kill **********************
 * Close /dev/pcm
 *
 *****/
void Towns8_Audio_Kill()
{ 
  /* TURN AUDIO OFF */
  Towns8_Audio_Off(0);
  xa_audio_present = XA_AUDIO_UNK;
  /* SHUT THINGS DOWN  */
  close(devAudio);
  Kill_Audio_Ring();
}

/********** Towns8_Audio_Off **********************
 * Stop Audio Stream
 *
 *****/
void Towns8_Audio_Off(flag)
xaULONG flag;
{ long ret;

  if (xa_audio_status != XA_AUDIO_STARTED) return;

  /* SET FLAG TO STOP OUTPUT ROUTINE */
  xa_audio_status = XA_AUDIO_STOPPED;

  /* TURN OFF SOUND ??? */
  /*
  Towns8_Adjust_Volume(XA_AUDIO_MINVOL);
  */

  /* FLUSH AUDIO DEVICE */
  /***** none *****/


  xa_time_audio = -1;
  xa_audio_flushed = 0;

  /* FLUSH AUDIO DEVICE AGAIN */
  /***** none *****/

  /* RESTORE ORIGINAL VOLUME */
  /*
  if (XAAUD->mute != xaTRUE) Towns8_Adjust_Volume(XAAUD->volume);
  */
}

/********** Towns8_Audio_Prep **********************
 * Turn On Audio Stream.
 *
 *****/
void Towns8_Audio_Prep()
{ 
  DEBUG_LEVEL2 
  {
    fprintf(stderr,"Towns8_Audio_Prep \n");
  }
  if (xa_audio_status == XA_AUDIO_STARTED) return;
  else if (xa_audio_present != XA_AUDIO_OK) return;
  else if (xa_snd_cur)
  { int ret;

    /* CHANGE FREQUENCY IF NEEDED */
  /***** cant change *****/

    /* xa_snd_cur gets changes in Update_Ring() */
    xa_out_time = 500;  /* keep audio fed 500ms ahead of video */
    xa_out_init = xa_audio_ring_size - 1;
    xa_interval_time = xa_snd_cur->ch_time / XAAUD->divtest;
    if (xa_interval_time == 0) xa_interval_time = 1;

    XA_Flush_Ring();
    XA_Update_Ring(1000);
    xa_audio_status = XA_AUDIO_PREPPED;
  }
}

/****-------------------------------------------------------------------****
 *
 ****-------------------------------------------------------------------****/
void Towns8_Audio_On()
{
  if (   (xa_snd_cur)
      && (xa_audio_present == XA_AUDIO_OK)
      && (xa_audio_status == XA_AUDIO_PREPPED) )
  { 
    xa_audio_status = XA_AUDIO_STARTED;
    xa_time_now = XA_Read_AV_Time();  /* get new time */
    New_Merged_Audio_Output();
  }
}

/********** Towns8_Closest_Freq **********************************************
 *
 * Global Variable Affect:
 *   xaULONG xa_audio_hard_buff		must set but not larger than
 *					XA_AUDIO_MAX_RING_BUF size
 ****************************************************************************/
xaULONG Towns8_Closest_Freq(ifreq)
xaLONG ifreq;
{
    xa_audio_hard_buff = XA_HARD_BUFF_1K;
    return(19200);
}

/************* Towns8_Speaker_Toggle *****************************************
 *
 * flag = 0  turn speaker off
 * flag = 1  turn speaker on
 * flag = 2  toggle speaker
 ****************************************************************************/
void Towns8_Speaker_Toggle(flag)
xaULONG flag;
{ 
   return;
}

/************* Towns8_Headphone_Toggle *****************************************
 *
 * flag = 0  turn headphones off
 * flag = 1  turn headphones on
 * flag = 2  toggle headphones
 ****************************************************************************/
void Towns8_Headphone_Toggle(flag)
xaULONG flag;
{ 
   return;
}


/********** Towns8_Adjust_Volume **********************
 * Routine for Adjusting Volume on a Towns8
 *
 * Volume is in the range [0,XA_AUDIO_MAXVOL]
 ****************************************************************************/
void Towns8_Adjust_Volume(volume)
xaULONG volume;
{
   return;
}
#endif
/****************************************************************************/
/******************* END OF Towns8 SPECIFIC ROUTINES *********************/
/****************************************************************************/

/****************************************************************************/
/**************** CoreAudio SPECIFIC ROUTINES *****************************/
/****************************************************************************/
#ifdef XA_COREAUDIO_AUDIO

/*
 * Mac OS X CoreAudio  port provided by C.W. "Madd the Sane" Betts,
 *	computers57@hotmail.com
 * Using c89 because K&R sucks.
 * Tues Feb. 14, 2017
 *
 */

static void  CoreAudio_Audio_Init(void);
static void  CoreAudio_Audio_Kill(void);
static void  CoreAudio_Audio_Off(xaULONG flag);
static void  CoreAudio_Audio_Prep(void);
static void  CoreAudio_Audio_On(void);
static void  CoreAudio_Adjust_Volume(xaULONG volume);
static xaULONG CoreAudio_Closest_Freq(xaLONG ifreq);
static void CoreAudio_Set_Output_Port(xaULONG aud_ports);
static void CoreAudio_Speaker_Toggle(xaLONG ifreq);
static void CoreAudio_Headphone_Toggle(xaULONG flag);

void XA_Audio_Setup()
{

  XA_Audio_Init		= CoreAudio_Audio_Init;
  XA_Audio_Kill		= CoreAudio_Audio_Kill;
  XA_Audio_Off		= CoreAudio_Audio_Off;
  XA_Audio_Prep		= CoreAudio_Audio_Prep;
  XA_Audio_On		= CoreAudio_Audio_On;
  XA_Closest_Freq	= CoreAudio_Closest_Freq;
  XA_Set_Output_Port	= (void *)(0);
  XA_Speaker_Tog	= CoreAudio_Speaker_Toggle;
  XA_Headphone_Tog	= CoreAudio_Headphone_Toggle;
  XA_LineOut_Tog	= CoreAudio_Headphone_Toggle;
  XA_Adjust_Volume	= CoreAudio_Adjust_Volume;

  xa_snd_cur = 0;
  xa_audio_present = XA_AUDIO_UNK;
  xa_audio_status  = XA_AUDIO_STOPPED;
  xa_audio_ring_size  = 8;
}

void  CoreAudio_Audio_Init(void)
{
	
}

void  CoreAudio_Audio_Kill(void)
{
	
}

void  CoreAudio_Audio_Off(xaULONG flag)
{
	
}

void  CoreAudio_Audio_Prep(void)
{
	
}
void  CoreAudio_Audio_On(void)
{
	
}

void  CoreAudio_Adjust_Volume(xaULONG volume)
{

}

xaULONG CoreAudio_Closest_Freq(xaLONG ifreq)
{
	xa_audio_hard_buff = XA_HARD_BUFF_2K;
	return ifreq;
}

void CoreAudio_Set_Output_Port(xaULONG aud_ports)
{

}

void CoreAudio_Speaker_Toggle(xaLONG ifreq)
{

}

void CoreAudio_Headphone_Toggle(xaULONG flag)
{

}


#endif
/****************************************************************************/
/******************* END OF Towns8 SPECIFIC ROUTINES *********************/
/****************************************************************************/

/* NEW CODE */
XA_SND *XA_Audio_Next_Snd(snd_hdr)
XA_SND *snd_hdr;
{ DEBUG_LEVEL2 fprintf(stderr,"XA_Audio_Next_Snd: snd_hdr %p \n",
							snd_hdr);
  xa_snd_cur = snd_hdr->next;
  /* brief clean up of old header */
  snd_hdr->inc_cnt = 0;
  snd_hdr->byte_cnt = 0;
  snd_hdr->samp_cnt = snd_hdr->tot_samps;
  if (snd_hdr->fpos >= 0) snd_hdr->snd = 0;
  if (xa_snd_cur==0) return(0);
  /* full init of new header */
  XA_Audio_Init_Snd(xa_snd_cur);
  /* read in info from file if necessary */
  if (xa_snd_cur->fpos >= 0)
  { xa_snd_cur->snd = xa_audcodec_buf;
    XA_Read_Audio_Delta(xa_aud_fd,xa_snd_cur->fpos,
				xa_snd_cur->tot_bytes,xa_audcodec_buf);
  }
  return(xa_snd_cur);
}

void XA_Read_Audio_Delta(fd,fpos,fsize,buf)
int fd;
xaLONG fpos;
xaULONG fsize;
char *buf;
{ int ret;
  ret  = lseek(fd,fpos,SEEK_SET);
  if (ret != fpos) TheEnd1("XA_Read_Audio_Delta:seek err");
  ret = read(fd, buf, fsize);
  if (ret != fsize) TheEnd1("XA_Read_Audio_Delta:read err");
}



/*************
 *  May not be needed
 *
 *****/
xaUBYTE XA_Signed_To_uLaw(ch)
xaLONG ch;
{
  xaLONG mask;
  if (ch < 0) { ch = -ch; mask = 0x7f; }
  else { mask = 0xff; }
  if (ch < 32)		{ ch = 0xF0 | (15 - (ch / 2)); }
  else if (ch < 96)	{ ch = 0xE0 | (15 - (ch - 32) / 4); }
  else if (ch < 224)	{ ch = 0xD0 | (15 - (ch - 96) / 8); }
  else if (ch < 480)	{ ch = 0xC0 | (15 - (ch - 224) / 16); }
  else if (ch < 992)	{ ch = 0xB0 | (15 - (ch - 480) / 32); }
  else if (ch < 2016)	{ ch = 0xA0 | (15 - (ch - 992) / 64); }
  else if (ch < 4064)	{ ch = 0x90 | (15 - (ch - 2016) / 128); }
  else if (ch < 8160)	{ ch = 0x80 | (15 - (ch - 4064) /  256); }
  else			{ ch = 0x80; }
  return (mask & ch);
}

void Gen_Signed_2_uLaw()
{
  xaULONG i;
  for(i=0;i<256;i++)
  { xaUBYTE d;
    xaBYTE ch = i;
    xaLONG chr = ch;
    d = XA_Signed_To_uLaw(chr * 16);
    xa_sign_2_ulaw[i] = d;
  }
}


/*
** This routine converts from ulaw to 16 bit linear.
**
** Craig Reese: IDA/Supercomputing Research Center
** 29 September 1989
**
** References:
** 1) CCITT Recommendation G.711  (very difficult to follow)
** 2) MIL-STD-188-113,"Interoperability and Performance Standards
**     for Analog-to_Digital Conversion Techniques,"
**     17 February 1987
**
** Input: 8 bit ulaw sample
** Output: signed 16 bit linear sample
*/

xaLONG XA_uLaw_to_Signed( ulawbyte )
xaUBYTE ulawbyte;
{
  static int exp_lut[8] = { 0, 132, 396, 924, 1980, 4092, 8316, 16764 };
  int sign, exponent, mantissa, sample;
 
  ulawbyte = ~ ulawbyte;
  sign = ( ulawbyte & 0x80 );
  exponent = ( ulawbyte >> 4 ) & 0x07;
  mantissa = ulawbyte & 0x0F;
  sample = exp_lut[exponent] + ( mantissa << ( exponent + 3 ) );
  if ( sign != 0 ) sample = -sample;
 
  return sample;
}

/****-----------------------------------------------------------------****
 *
 ****-----------------------------------------------------------------****/
void Gen_uLaw_2_Signed()
{ xaULONG i;
  for(i=0;i<256;i++)
  { xaUBYTE data = (xaUBYTE)(i);
    xaLONG d = XA_uLaw_to_Signed( data );
    xa_ulaw_2_sign[i] = (xaULONG)((xaULONG)(d) & 0xffff);
  }
}

/****-----------------------------------------------------------------****
 *
 ****-----------------------------------------------------------------****/
void Gen_aLaw_2_Signed()
{ xaULONG i;
  for(i=0;i<256;i++)
  { xaUBYTE data = (xaUBYTE)(i);
    xaLONG d, t, seg;

    data ^= 0x55;

    t = (data & 0xf) << 4;
    seg = (data & 0x70) >> 4;
    if (seg == 0)	t += 8;
    else if (seg == 1)	t += 0x108;
    else	{ t += 108; t <<= seg - 1; }

    d =  (data & 0x80)?(t):(-t);
    xa_alaw_2_sign[i] = (xaULONG)((xaULONG)(d) & 0xffff);
  }
}



/* Pod this was 4096, I changed to 32768 */
#define XA_ARM_VAL_LIMIT       32768
/* Mu-law format constants */
#define XA_ARM_SIGN_BIT        1
#define XA_ARM_NCHORDS         8
#define XA_ARM_NPOINTS         16
    /* limit of "magnitude" (logarithmic) */
#define XA_ARM_DAC_LIMIT       (XA_ARM_NCHORDS*XA_ARM_NPOINTS)  

xaULONG arm_dac_to_val[XA_ARM_DAC_LIMIT];     /* obsolete */

/**************************************
 *
 * original code courtesy of Mark Taunton, Acorn Computers Ltd, June 1992.
 * hacked by Mark Podlipec
 *************************************/
void Gen_Arm_2_Signed()
{ int c, p, ux, step, curval, max;
  double adjust;

  ux = 0; curval = 0;
  for (c = 0; c < XA_ARM_NCHORDS; ++c)
  {
    step = 1 << c;
    for (p = 0; p < XA_ARM_NPOINTS; ++p)
    {
      arm_dac_to_val[ux++] = curval;
      curval += step;
    }
  }
  
  /* Now adjust for 0..4095 notional linear range rather than 0..4016 */
  /* i.e. half-step over max nominal: 3952+128/2 = 4016 */
  max = curval - step/2;  
  adjust = (double)(XA_ARM_VAL_LIMIT-1) / (double)max;
  /* Scale dac_to_val table to give 0..4095 linear range coverage */
  for (ux = 0; ux < XA_ARM_DAC_LIMIT; ++ux)
  {                       /* implement rounding */
    arm_dac_to_val[ux] = (int) (arm_dac_to_val[ux] * adjust + 0.5); 
        /* Compute signed table values also */
    xa_arm_2_signed[ux << 1 |        0]        =  arm_dac_to_val[ux];
    xa_arm_2_signed[ux << 1 | XA_ARM_SIGN_BIT] = -arm_dac_to_val[ux];
  }
}

/********* Init_Audio_Ring ************
 *
 * Global Variables:
 *  xa_audio_ring		pointer to somewhere in the ring
 *************************************/
void Init_Audio_Ring(xaULONG ring_num,xaULONG buf_size)
{
  xaULONG i;
  XA_AUDIO_RING_HDR *t_ring,*t_first;
  xaUBYTE *t_buf;
  if (xa_audio_ring) Kill_Audio_Ring();
  t_first = xa_audio_ring = 0;
  for(i=0;i<ring_num;i++)
  {
    t_ring = (XA_AUDIO_RING_HDR *)malloc( sizeof(XA_AUDIO_RING_HDR) );
    if (t_ring==0) TheEnd1("Init Audio Ring: malloc err0");
    t_buf = (xaUBYTE *)malloc( buf_size );
    if (t_buf==0) TheEnd1("Init Audio Ring: malloc err1");
    t_ring->time = t_ring->timelo = 0;
    t_ring->len = 0;
    t_ring->buf = t_buf;
    if (t_first == 0) t_first = xa_audio_ring = t_ring;
    t_ring->next = t_first;
    xa_audio_ring->next = t_ring;
    xa_audio_ring = t_ring;
  }
  xa_audio_ring_t = xa_audio_ring;
}

/********* Kill_Audio_Ring ************
 * Move around ring, free'ing up memory structures.
 *
 * Global Variables:
 *  xa_audio_ring		pointer to somewhere in the ring
 **************************************/
void Kill_Audio_Ring()
{
  XA_AUDIO_RING_HDR *t_first;

  if (xa_audio_ring==0) return;
  t_first = xa_audio_ring;

  do
  { XA_AUDIO_RING_HDR *t_tmp = xa_audio_ring;
    if (xa_audio_ring->buf) FREE(xa_audio_ring->buf,0x508);
    xa_audio_ring->time = xa_audio_ring->timelo = 0;
    xa_audio_ring->len = 0;
    xa_audio_ring->buf = 0;
    xa_audio_ring = xa_audio_ring->next;
    FREE(t_tmp,0x509);
  } while(xa_audio_ring != t_first);
  xa_audio_ring = 0;
  xa_audio_ring_t = 0;
}

/****************************************************************
 * Fill up any unused Audio Ring Entries with sound.
 *
 ****************************************************************/

void XA_Update_Ring(cnt)
xaULONG cnt;
{ 
DEBUG_LEVEL1 fprintf(stderr,"UPDATE RING %d\n",cnt);
  while( (xa_audio_ring_t->len == 0) && (xa_snd_cur) && cnt)
  { xaULONG tmp_time, tmp_timelo; xaLONG i,xx;
    cnt--;
    tmp_time = xa_snd_cur->ch_time;	/* save these */
    tmp_timelo = xa_snd_cur->ch_timelo;
	/* uncompress sound chunk */
    i = xa_audio_ring_t->len = xa_snd_cur->ch_size * xa_audio_hard_bps;
    /* NOTE: the following delta call may modify xa_snd_cur */
    xx = xa_snd_cur->delta(xa_snd_cur,xa_audio_ring_t->buf,0,
							xa_snd_cur->ch_size);
    i -= xx * xa_audio_hard_bps;
    if (i > 0) /* Some system need fixed size chunks */
    { xaUBYTE *dptr = xa_audio_ring_t->buf;
      while(i--) *dptr++ = 0;
    }
    xa_audio_ring_t->time = tmp_time;
    xa_audio_ring_t->timelo = tmp_timelo;
    xa_audio_ring_t = xa_audio_ring_t->next; 
  }
}

/***************************************************************8
 * Flush Ring of All Conversions
 * SHOULD ONLY BE CALLED BY OUTPUT ROUTINE.
 */

void XA_Flush_Ring()
{ XA_AUDIO_RING_HDR *tring = xa_audio_ring;

DEBUG_LEVEL1 fprintf(stderr,"FLUSH_RING\n");
  do
  { xa_audio_ring->len = 0;
    xa_audio_ring = xa_audio_ring->next;
  } while(xa_audio_ring != tring);
  xa_audio_ring_t = xa_audio_ring;	/* resync  fill and empty */
  xa_audio_flushed = 1;
}


/********** New_Merged_Audio_Output **********************
 * Set Volume if needed and then write audio data to audio device.
 *
 * IS 
 *****/
void New_Merged_Audio_Output()
{ xaLONG time_diff, loop_cnt = xa_audio_ring_size;

  /* normal exit is when audio gets ahead */
  while(loop_cnt--)
  {
    if (xa_audio_status != XA_AUDIO_STARTED) { return; }
    time_diff = xa_time_audio - xa_time_now; /* how far ahead is audio */
    if (time_diff > xa_out_time) /* potentially ahead */
    {
      xa_time_now = XA_Read_AV_Time();  /* get new time */
      time_diff = xa_time_audio - xa_time_now;
      if (time_diff > xa_out_time)  /* definitely ahead */
      { /* If we're ahead, update the ring. */
        XA_Update_Ring(2);
	xa_interval_id = XtAppAddTimeOut(theAudContext,xa_interval_time,
		(XtTimerCallbackProc)New_Merged_Audio_Output,(XtPointer)(NULL));
        return;
      }
    }


    if (xa_audio_ring->len)
    { 
      /* If audio buffer is full, write() can't write all of the data. */
      /* So, next routine checks the rest length in buffer.            */
#ifdef XA_SONY_AUDIO
      { int ret, buf_len;
	ret = ioctl(devAudio, SBIOCBUFRESID,&buf_len) ;
	if( ret == -1 ) fprintf(stderr,"SONY_AUDIO: SIOCBUFRESID error.\n");
	if(buf_len > sony_audio_buf_len - xa_audio_ring->len)
	{
	  xa_interval_id = XtAppAddTimeOut(theAudContext,xa_interval_time,
		(XtTimerCallbackProc)New_Merged_Audio_Output,(XtPointer)(NULL));
	  return;
	}
      }
#endif

#ifdef XA_SGI_AUDIO
      { int buf_len;
	buf_len = ALgetfillable(port) ;
	if (buf_len < (xa_audio_ring->len >> 1))
	{ 
	  xa_interval_id = XtAppAddTimeOut(theAudContext,xa_interval_time,
		(XtTimerCallbackProc)New_Merged_Audio_Output,(XtPointer)(NULL));
	  return;
	}
      }
#endif

#ifdef XA_MMS_AUDIO
      {
        mmeProcessCallbacks();
	/* NOTE: These buffers aren't part of the XAnim audio ring buffer */
	/*       Our ring buffer just happens to be the same size */
	if( mms_buffers_outstanding  >= xa_audio_ring_size ) 
	{
	  xa_interval_id = XtAppAddTimeOut(theAudContext,xa_interval_time,
		(XtTimerCallbackProc)New_Merged_Audio_Output,(XtPointer)(NULL));
 	  return ;
	}
      }
#endif

#ifdef XA_HP_AUDIO
      if (hp_audio_paused == xaTRUE)
      { DEBUG_LEVEL1 fprintf(stderr,"HP-out-unpause\n");
        AResumeAudio( audio_connection, hp_trans_id, NULL, NULL );
        hp_audio_paused = xaFALSE;
      }
#endif


/*---------------- Now for the Write Segments -------------------------------*/

#ifdef XA_SPARC_AUDIO
      write(devAudio,xa_audio_ring->buf,xa_audio_ring->len); 
#endif

#ifdef XA_NetBSD_AUDIO
      write(devAudio,xa_audio_ring->buf,xa_audio_ring->len);
#endif

#ifdef XA_AIX_AUDIO
      { int rc;
        rc = write ( devAudio, xa_audio_ring->buf, xa_audio_ring->len );
      }
#endif

#ifdef XA_SGI_AUDIO
      /* # of Samples, not Bytes. Note: assume 16 bit samples. */
      ALwritesamps(port,xa_audio_ring->buf, (xa_audio_ring->len >> 1) );
#endif

#ifdef XA_LINUX_AUDIO
      write(devAudio,xa_audio_ring->buf,xa_audio_ring->len);
#endif

#ifdef XA_NAS_AUDIO
      NAS_Write_Data(xa_audio_ring->buf, xa_audio_ring->len);
#endif

#ifdef XA_SONY_AUDIO
      { int ret;
        write(devAudio,xa_audio_ring->buf,xa_audio_ring->len);
        /* Buffer of Sony audio device is too large */ /* HP needs this */
        ret = ioctl(devAudio, SBIOCFLUSH, 0);
        if( ret == -1 ) fprintf(stderr,"audio output:SBIOCFLUSH error.\n");
      }
#endif

#ifdef XA_EWS_AUDIO
      write(devAudio,xa_audio_ring->buf,xa_audio_ring->len);
#endif

#ifdef XA_AF_AUDIO
      { ATime act, atd = AFtime0;
	if (XAAUD->mute != xaTRUE)
	  act = AFPlaySamples(ac,AFtime0,xa_audio_ring->len,xa_audio_ring->buf);
	else act = AFGetTime(ac);
	if (AFtime0 < act)	AFtime0 = act+TOFFSET;
	else	AFtime0 += xa_audio_ring->len >> 1; /* Number of samples */
      }
#endif

#ifdef XA_HP_AUDIO
      write(streamSocket,xa_audio_ring->buf,xa_audio_ring->len);
/* Some way to flush streamsocket???? */
#endif

#ifdef XA_HPDEV_AUDIO
      write (devAudio, xa_audio_ring->buf, xa_audio_ring->len);
#endif

#ifdef XA_MMS_AUDIO
      /* As currently implemented, this copies the audio data into a separate
         shared memory buffer for communication with the multimedia server. We
         could actually work directly out of the audio ring buffer if it was
         allocated in shared memory, but this keeps things more independent of
         each other - tfm
      */
      {  
	MMRESULT	status;
	int		bytes,len;
	if (XAAUD->mute != xaTRUE) 
	{
	  bytes = xa_audio_hard_buff * xa_audio_hard_bps;
	  mms_lpWaveHeader->lpData = (LPSTR)(mms_audio_buffer
						+ mms_next_buffer * bytes);
	  mms_lpWaveHeader->dwBufferLength = xa_audio_ring->len;
	  if(xa_audio_ring->len > bytes) 
	  {
	    len = bytes;
	    fprintf(stderr,"Audio chunk truncated to %d bytes\n",len);
	  }
	  else len = xa_audio_ring->len;
	  memcpy( mms_lpWaveHeader->lpData, xa_audio_ring->buf, len);
	  mms_next_buffer++;
	  if(mms_next_buffer >= xa_audio_ring_size) mms_next_buffer = 0;
	  status = waveOutWrite(mms_device_handle, mms_lpWaveHeader,
							sizeof(WAVEHDR));
	  if( status != MMSYSERR_NOERROR ) 
	  {
	    fprintf(stderr,"waveOutWrite failed - status = %d\n",status);
	  }
	  else { mms_buffers_outstanding++; }
	}
      }
#endif

#ifdef XA_TOWNS_AUDIO
      write(devAudio,xa_audio_ring->buf,xa_audio_ring->len);
#endif

#ifdef XA_TOWNS8_AUDIO
      { int i_;
	char *cp_,*cp2_;

	i_=0;
	cp_ = cp2_ = xa_audio_ring->buf;

	while (i_<xa_audio_ring->len)
	{ int i1_,i2_;
	  i1_ = *cp_++;
	  cp_++;
	  if (i1_ < 0) { i1_ += 256; }
	  if (i1_ >= 128) { i1_ = 128 - i1_; }
	  *cp2_++ = (char) i1_;
	  i_+=2;
        }
        write(devAudio,xa_audio_ring->buf,xa_audio_ring->len/2);
      }
#endif


      /* Don't adjust volume until after write. If we're behind
       * the extra delay could cause a pop. */
      if (XAAUD->newvol==xaTRUE)
      { xaULONG vol = (XAAUD->mute==xaTRUE)?(XA_AUDIO_MINVOL):(XAAUD->volume);
  	XA_Adjust_Volume(vol);
  	XAAUD->newvol = xaFALSE;
      }
      xa_time_audio   += xa_audio_ring->time;
      xa_timelo_audio += xa_audio_ring->timelo;
      if (xa_timelo_audio & 0xff000000)
		{ xa_time_audio++; xa_timelo_audio &= 0x00ffffff; }
      xa_audio_ring->len = 0; 
      xa_audio_ring = xa_audio_ring->next;  /* move to next */
      if (xa_audio_status != XA_AUDIO_STARTED) { return;}

      if (xa_out_init) xa_out_init--;
      else XA_Update_Ring(2);  /* only allow two to be updated */
/* NOT HERE - loop first
      xa_interval_id = XtAppAddTimeOut(theAudContext,xa_interval_time,
	    (XtTimerCallbackProc)New_Merged_Audio_Output,(XtPointer)(NULL));
      return;
*/
    } 
    else  /* Audio Sample finished */
    { xa_time_now = XA_Read_AV_Time();

	/* Is audio still playing buffered samples? */
      if (xa_time_now < xa_time_audio) 
      { xaULONG diff = xa_time_audio - xa_time_now;
	/* POD note 50ms is arbitrarily chosen to be long, but not noticeable*/
        if (xa_audio_status != XA_AUDIO_STARTED) {return;}
	xa_interval_id = XtAppAddTimeOut(theAudContext,diff,
		(XtTimerCallbackProc)New_Merged_Audio_Output,(XtPointer)(NULL));
        return;
      }
      else 
      { XA_Audio_Off(0);
	return;
      }
    } /* end of ring_len */
  } /* end of while ring */
  xa_interval_id = XtAppAddTimeOut(theAudContext,xa_interval_time,
	    (XtTimerCallbackProc)New_Merged_Audio_Output,(XtPointer)(NULL));
  return;
}  /* end of function */

