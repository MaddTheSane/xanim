
/*
 * xanim_audio.c
 *
 * Copyright (C) 1994,1995 by Mark Podlipec.
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
   void  XXX_Audio_On()
   ULONG XXX_Closest_Freq(target_freq)
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

 XA_Update_Ring is called in XA_Audio_On() just before audio is
 started up and is called in XA_Audio_Output() after a Audio Ring
 buffer has been sent to the audio device.

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

 NOTE: that the audio MUST be interrupt driven and self
 driving. It can't rely on the video sequencing to drive it. 
 XA_Audio_On() should enable and startup the audio. XA_Audio_Off()
 should stop the audio.

 XA_Audio_Init() opens the audio device and initializes the variables.
 XA_Audio_Kill() closes the device and frees up any hardware specific
 memory structures.
 
 ----

 NOTE: signals suck. On many system's the ALRM interrupt will cause
 a read() or a write() to fail. Some allow that call to be restarted,
 but not all of them.  I redid the audio interrupts to use
 XtAppAddTimeout() instead.

 ----

 Mark Podlipec - podlipec@Baynetworks.com  or  podlipec@@shell.portal.com
*/

/* TOP */

#include "xanim_audio.h"

extern XtAppContext  theContext;

/* POD NOTE: move into xanim.h */
#define XA_ABS(x) (((x)<0)?(-(x)):(x))

extern ULONG xa_audio_port;
ULONG XA_Audio_Speaker();


/***************** ITIMER EXPERIMENTATION ********************************/
/* number of ms we let audio buffer get ahead of actual audio. */
LONG xa_out_time = 250; /* PODNOTE: this is actually reset later on */
ULONG xa_out_init = 0;  /* number of initial audio ring bufs to write ahead*/

/* 1000/XA_OUT_FREQADJ is number of extra itimer calls  */
#define XA_OUT_FREQADJ 1000

/***************** XtAppAddTimeOut EXPERIMENTATION *************************/
XtIntervalId xa_interval_id = 0;
ULONG        xa_interval_time = 1;

extern Display       *theDisp;


/**** Non Hardware Specific Functions ************/
void XA_Audio_Init_Snd();
void Init_Audio_Ring();
void Kill_Audio_Ring();
void XA_Update_Ring();
void XA_Flush_Ring();
ULONG XA_Add_Sound();
XA_SND *XA_Audio_Next_Snd();
void XA_Read_Audio_Delta();
LONG XA_Read_AV_Time();
extern LONG xa_time_now;
extern UBYTE *xa_audcodec_buf;
extern ULONG xa_audcodec_maxsize;
extern int xa_aud_fd;
extern ULONG xa_audio_playrate; /* POD TESTING BUT MIGHT PROVE USEFUL */
extern ULONG xa_audio_divtest;  /* POD TESTING ONLY */
extern ULONG xa_audio_synctst;  /* POD TESTING ONLY */


typedef struct AUDIO_RING_STRUCT
{
  ULONG time;
  ULONG timelo;
  ULONG len;
  UBYTE *buf;
  struct AUDIO_RING_STRUCT *next;
} XA_AUDIO_RING_HDR;

XA_AUDIO_RING_HDR *xa_audio_ring = 0;
XA_AUDIO_RING_HDR *xa_audio_ring_t = 0;

#define XA_AUDIO_MAX_RING_BUFF 2048

/* NOTE: These must NOT be larger than XA_AUDIO_MAX_RING_BUFF above */
#define XA_HARD_BUFF_2K 2048
#define XA_HARD_BUFF_1K 1024


/* ULAW CONVERSION TABLES/ROUTINES Sparc specific??? */
UBYTE sign_2_ulaw[256];
ULONG XA_Audio_Linear_To_AU();
UBYTE XA_Signed_To_Ulaw();
void Gen_Signed_2_Ulaw();

/* AUDIO CODEC DELTA ROUTINES */ 
ULONG XA_Audio_1M_1M();
ULONG XA_Audio_PCM1M_PCM2M();
ULONG XA_Audio_PCM1S_PCM2M();
ULONG XA_Audio_PCMXM_PCM1M();
ULONG XA_Audio_PCMXS_PCM1M();
ULONG XA_Audio_ADPCMM_PCM2M();
ULONG XA_Audio_PCM2X_PCM2M();
ULONG XA_Audio_Silence();


/* AUDIO CODEC BUFFERING ROUTINES */
ULONG ms_adpcm_decode();

extern ULONG xa_audio_present;
extern ULONG xa_audio_status;
extern XA_SND *xa_snd_cur;

extern LONG  xa_time_audio;
extern LONG  xa_timelo_audio;
extern ULONG xa_audio_mute;
extern LONG  xa_audio_volume;
extern ULONG xa_audio_newvol;
extern double xa_audio_scale;
extern ULONG xa_audio_buffer;

ULONG xa_audio_hard_freq;		/* hardware frequency */
ULONG xa_audio_hard_buff;		/* preferred snd chunk size */
ULONG xa_audio_ring_size;		/* preferred num of ring entries */
ULONG xa_audio_hard_type;		/* hardware sound encoding type */
ULONG xa_audio_hard_bps;		/* hardware bytes per sample */
ULONG xa_audio_hard_chans;		/* hardware number of chan. not yet */
ULONG xa_update_ring_sem = 0;
ULONG xa_audio_out_sem = 0;
ULONG xa_audio_flushed = 0;

void  XA_Audio_Setup();
void (*XA_Audio_Init)();
void (*XA_Audio_Kill)();
void (*XA_Audio_Off)();
void (*XA_Audio_On)();
void (*XA_Adjust_Volume)();
ULONG (*XA_Closest_Freq)();
void  (*XA_Set_Output_Port)(); /* POD slowly replacing _Tog's */
void  (*XA_Speaker_Tog)();
void  (*XA_Headphone_Tog)();
void  (*XA_LineOut_Tog)();

#ifdef XA_AUD_OUT_MERGED
static void Merged_Audio_Output();
#endif


/****************************************************************************/
/**************** NULL AUDIO DEFINED ROUTINES********************************/
/****************************************************************************/
/* useful for backing out audio when device opens fails, etc */

void  XA_NoAudio_Nop();
ULONG XA_NoAudio_Nop1();
void  XA_NoAudio_Nop2();
void  XA_Null_Audio_Setup();

void XA_Null_Audio_Setup()
{
  XA_Audio_Init		= XA_NoAudio_Nop;
  XA_Audio_Kill		= XA_NoAudio_Nop;
  XA_Audio_Off		= XA_NoAudio_Nop2;
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
void XA_NoAudio_Nop2(flag)	ULONG flag;	{ return; }
ULONG XA_NoAudio_Nop1(num)	ULONG num;	{ return(0); }

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
#ifndef SOLARIS

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
void  Sparc_Audio_On();
void  Sparc_Adjust_Volume();
ULONG Sparc_Closest_Freq();
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
#ifdef SOLARIS
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
    else fprintf(stderr,"Audio_Init: Error opening audio device. - ");
    fprintf(stderr,"Will continue without audio\n");
    xa_audio_present = XA_AUDIO_ERR;
    return;
  }
  ret = ioctl(devAudio, AUDIO_GETDEV, &type);
/* POD NOTE: Sparc 5 has new audio device (CS4231) */
#ifdef SOLARIS
  if ( (   (strcmp(type.name, "SUNW,dbri"))   /* Not DBRI (SS10's) */
        && (strcmp(type.name, "SUNW,CS4231"))) /* and not CS4231 (SS5's) */
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
    Gen_Signed_2_Ulaw();
  }
  else /* DBRI or CS4231 */
  {
    DEBUG_LEVEL1 fprintf(stderr,"SPARC DBRI or CS4231 AUDIO\n");
    xa_audio_hard_type  = XA_AUDIO_SIGNED_2MB;
    xa_audio_hard_freq  = 11025;
    xa_audio_hard_buff  = XA_HARD_BUFF_1K;             /* default buffer size */
    xa_audio_hard_bps   = 2;
    xa_audio_hard_chans = 1;
    AUDIO_INITINFO(&audio_info);
#ifdef SOLARIS
/* POD-NOTE: Does this necessarily have to be 1024 (what the upper/lower
 * limits???)
 */
    audio_info.play.buffer_size = XA_HARD_BUFF_1K;
    xa_audio_hard_buff  = XA_HARD_BUFF_1K;   /* default buffer size */

#endif
    audio_info.play.sample_rate = 11025;
    audio_info.play.precision = 16;
    audio_info.play.channels = 1;
    audio_info.play.encoding = AUDIO_ENCODING_LINEAR;
    ret = ioctl(devAudio, AUDIO_SETINFO, &audio_info);
    if (ret)
    {
      fprintf(stderr,"AUDIO BRI FATAL ERROR %ld\n",errno);
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
ULONG flag;
{ long ret;

  DEBUG_LEVEL1 fprintf(stderr,"Sparc_Audio_Off\n");
  if (xa_audio_status != XA_AUDIO_STARTED) return;

  /* SET FLAG TO STOP OUTPUT ROUTINE */
  xa_audio_status = XA_AUDIO_STOPPED;

  /* TURN OFF SOUND ??? */
  Sparc_Adjust_Volume(XA_AUDIO_MINVOL);

  /* FLUSH AUDIO DEVICE */
  ret = ioctl(devAudio, I_FLUSH, FLUSHW);
  if (ret == -1) fprintf(stderr,"Sparc Audio: off flush err %ld\n",errno);

  xa_time_audio = -1;
  xa_audio_flushed = 0;

  /* FLUSH AUDIO DEVICE AGAIN */
  ret = ioctl(devAudio, I_FLUSH, FLUSHW);
  if (ret == -1) fprintf(stderr,"Sparc Audio: off flush err %ld\n",errno);

  /* RESTORE ORIGINAL VOLUME */
  Sparc_Adjust_Volume(xa_audio_volume);
}

/********** Sparc_Audio_On **********************
 * Turn On Audio Stream.
 *
 *****/
void Sparc_Audio_On()
{ 
  DEBUG_LEVEL2 
  {
    fprintf(stderr,"Sparc_Audio_On \n");
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
      if (ret == -1) fprintf(stderr,"audio setfreq: freq %lx errno %ld\n",
						xa_snd_cur->hfreq, errno);
      xa_audio_hard_freq = xa_snd_cur->hfreq;
    }

    /* xa_snd_cur gets changes in Update_Ring() */
    xa_out_time = 500;  /* keep audio fed 500ms ahead of video */
    xa_out_init = xa_audio_ring_size - 1;
    xa_interval_time = xa_snd_cur->ch_time / xa_audio_divtest;

DEBUG_LEVEL1 fprintf(stderr,"ch_time %ld out_time %ld int time %ld \n",xa_snd_cur->ch_time,xa_out_time,xa_interval_time);

    xa_audio_status = XA_AUDIO_STARTED;
    XA_Flush_Ring();
    XA_Update_Ring(1000);

    if (xa_interval_time == 0) xa_interval_time = 1;
    xa_time_now = XA_Read_AV_Time();  /* get new time */
    Merged_Audio_Output();
  }
}


/********** Sparc_Closest_Freq **********************************************
 *
 * Global Variable Affect:
 *   ULONG xa_audio_hard_buff		must set but not larger than
 *					XA_AUDIO_MAX_RING_BUF size
 ****************************************************************************/
ULONG Sparc_Closest_Freq(ifreq)
LONG ifreq;
{
  if (xa_audio_hard_type==XA_AUDIO_SIGNED_2MB)
  { static int valid[] = { 8000, 9600, 11025, 16000, 18900, 22050, 32000,
			      37800, 44100, 48000, 0};
    LONG i = 0;
    LONG best = 8000;

    xa_audio_hard_buff = XA_HARD_BUFF_1K;
    while(valid[i])
    { 
      if (XA_ABS(valid[i] - ifreq) < XA_ABS(best - ifreq)) best = valid[i];
      i++;
    }
    return(best);
  }
  else return(8000);
}


/* Eventually merge everything to one */
void Sparc_Set_Output_Port(aud_ports)
ULONG aud_ports;
{ audio_info_t a_info;
  LONG ret;
  ULONG sparc_ports = 0;
  if (aud_ports & XA_AUDIO_PORT_INT)  sparc_ports |= AUDIO_SPEAKER;
  if (aud_ports & XA_AUDIO_PORT_HEAD) sparc_ports |= AUDIO_HEADPHONE;
  if (aud_ports & XA_AUDIO_PORT_EXT)  sparc_ports |= AUDIO_LINE_OUT;
  AUDIO_INITINFO(&a_info);
  a_info.play.port = sparc_ports;
  ret = ioctl(devAudio, AUDIO_SETINFO, &a_info);
  if (ret < 0) fprintf(stderr,"Audio: couldn't set speaker port %ld\n",errno);
}

/************* Sparc_Speaker_Toggle *****************************************
 *
 * flag = 0  turn speaker off
 * flag = 1  turn speaker on
 * flag = 2  toggle speaker
 ****************************************************************************/
void Sparc_Speaker_Toggle(flag)
ULONG flag;
{ 
  switch(flag)
  {
    case  0: xa_audio_port &= ~XA_AUDIO_PORT_INT; break;
    case  1: xa_audio_port |=  XA_AUDIO_PORT_INT; break;
    default:  /* mutually exclusive set for now - never turn off */
    { if ( !(xa_audio_port & XA_AUDIO_PORT_INT)) 
		xa_audio_port = XA_AUDIO_PORT_INT;
    }
  }
  Sparc_Set_Output_Port(xa_audio_port);
}

/************* Sparc_Headphone_Toggle *****************************************
 *
 * flag = 0  turn headphones off
 * flag = 1  turn headphones on
 * flag = 2  toggle headphones
 ****************************************************************************/
void Sparc_Headphone_Toggle(flag)
ULONG flag;
{ 
  switch(flag)
  {
    case  0: xa_audio_port &= ~XA_AUDIO_PORT_HEAD; break;
    case  1: xa_audio_port |=  XA_AUDIO_PORT_HEAD; break;
    default:  /* mutually exclusive set for now - never turn off */
    { if ( !(xa_audio_port & XA_AUDIO_PORT_HEAD)) 
		xa_audio_port = XA_AUDIO_PORT_HEAD;
    }
  }
  Sparc_Set_Output_Port(xa_audio_port);
}


/********** Sparc_Adjust_Volume **********************
 * Routine for Adjusting Volume on a Sparc
 *
 * Volume is in the range [0,XA_AUDIO_MAXVOL]
 ****************************************************************************/
void Sparc_Adjust_Volume(volume)
ULONG volume;
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
void  AIX_Audio_On();

ULONG AIX_Closest_Freq();
void  AIX_Speaker_Tog();
void  AIX_Headphone_Tog();
void  AIX_LineOut_Tog();
void  AIX_Adjust_Volume();


static int     devAudio;
static ULONG   aix_audio_ports;
extern char  * xa_audio_device;

/*----------------------------------------------------------------------------*/
/*                                                                            */
/*----------------------------------------------------------------------------*/
void XA_Audio_Setup ( )
{
  XA_Audio_Init =     AIX_Audio_Init;
  XA_Audio_Kill =     AIX_Audio_Kill;
  XA_Audio_Off =      AIX_Audio_Off;
  XA_Audio_On =       AIX_Audio_On;
  XA_Closest_Freq =   AIX_Closest_Freq;
  XA_Speaker_Tog =    AIX_Speaker_Tog;
  XA_Headphone_Tog =  AIX_Headphone_Tog;
  XA_LineOut_Tog =    AIX_LineOut_Tog;
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
void AIX_Audio_On ( )
{
  audio_control control;
  int           rc;

  if ( xa_audio_status == XA_AUDIO_STARTED )
  {
    return;
  }

  if ( xa_audio_present != XA_AUDIO_OK )
  {
    return;
  }

  if ( xa_snd_cur )
  {
    /* Change the frequency if needed */
    if ( xa_audio_hard_freq != xa_snd_cur->hfreq )
    {
      audio_init init;

      printf ( "AIX_Audio_On: setting frequency to %d\n", xa_snd_cur->hfreq );

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
                  "AIX_Audio_On: AUDIO_INIT failed, rate = %d, errno = %d\n",
                  init.srate, errno );
      }

      xa_audio_hard_freq = xa_snd_cur->hfreq;
    }

    /* OLD xa_out_time = xa_snd_cur->ch_time * 4; */
    xa_out_time = 250;  /* keep audio fed 250ms ahead of video - may be 500*/
    xa_out_init = xa_audio_ring_size - 1;
    xa_interval_time = xa_snd_cur->ch_time / xa_audio_divtest;

    xa_audio_status = XA_AUDIO_STARTED;
    XA_Flush_Ring();
    XA_Update_Ring(1000);
    if ( xa_interval_time == 0 )
    {
      xa_interval_time = 1;
    }
    xa_time_now = XA_Read_AV_Time();  /* get new time */

    /* POD: The following 8 lines need to be before Merged_Audio_Output now */
    memset ( & control, '\0', sizeof ( control ) );
    control.ioctl_request = AUDIO_START;
    control.request_info  = NULL;
    control.position      = 0;
    if ( ( rc = ioctl ( devAudio, AUDIO_CONTROL, & control ) ) < 0 )
    {
      fprintf(stderr,"AIX_Audio_On: AUDIO_START failed, errno = %d\n", errno );
    }

    Merged_Audio_Output();
  }

}


/*----------------------------------------------------------------------------*/
/*                                                                            */
/*----------------------------------------------------------------------------*/
ULONG AIX_Closest_Freq ( ifreq )
LONG ifreq;
{
  static valid [ ] = {  8000,  9600, 11025, 16000, 18900,
                       22050, 32000, 37800, 44100, 48000, 0 };
  int  i = 0;
  LONG best = 8000;

  xa_audio_hard_buff = XA_HARD_BUFF_1K;
  while ( valid [ i ] )
  {
    if ( XA_ABS(valid[i] - ifreq) < XA_ABS(best - ifreq) )
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
ULONG settings;
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
ULONG flag;
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
ULONG flag;
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
ULONG flag;
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
ULONG volume;
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

ULONG ews_audio_ports = AUOUT_SP;

void  EWS_Audio_Init();
void  EWS_Audio_Kill();
void  EWS_Audio_Off();
void  EWS_Audio_On();
void  EWS_Adjust_Volume();
ULONG EWS_Closest_Freq();
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
      fprintf(stderr,"EWS: AUIOC_SETTYPE error %ld\n",errno);
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
ULONG flag;
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

/********** EWS_Audio_On **********************
 * Turn On Audio Stream.
 *
 *****/
void EWS_Audio_On()
{ 
  DEBUG_LEVEL2 
    fprintf(stderr,"EWS_Audio_On \n");
  if (xa_audio_status == XA_AUDIO_STARTED) return;
  else if (xa_audio_present != XA_AUDIO_OK) return;
  else if (xa_snd_cur)
  { int ret;

    /* CHANGE FREQUENCY IF NEEDED */
    if (xa_audio_hard_freq != xa_snd_cur->hfreq)
    { 
      
      ret = ioctl(devAudio, AUIOC_GETTYPE, &audio_type);
      if (ret == -1) fprintf(stderr,"AUIOC_GETTYPE: errno %ld\n",errno);
      audio_type.rate=EWS_Closest_Freq(xa_snd_cur->hfreq);
      ret = ioctl(devAudio, AUIOC_SETTYPE, &audio_type);
      if (ret == -1) fprintf(stderr,"AUIOC_SETTYPE: freq %lx errno %ld\n",
						xa_snd_cur->hfreq, errno);
      xa_audio_hard_freq = xa_snd_cur->hfreq;
    }
    EWS_Adjust_Volume(xa_audio_volume);

    /* xa_snd_cur gets changes in Update_Ring() */
    xa_out_time = 250; /* keep audio fed 250ms ahead of video */
    xa_out_init = xa_audio_ring_size - 1;
    xa_interval_time = xa_snd_cur->ch_time / xa_audio_divtest;

    xa_audio_status = XA_AUDIO_STARTED;
    XA_Flush_Ring();
    XA_Update_Ring(1000);

    if (xa_interval_time == 0) xa_interval_time = 1;
    xa_time_now = XA_Read_AV_Time();  /* get new time */
    Merged_Audio_Output();
  }
}


/********** EWS_Closest_Freq **********************************************
 *
 * Global Variable Affect:
 *   ULONG xa_audio_hard_buff		must set but not larger than
 *					XA_AUDIO_MAX_RING_BUF size
 ****************************************************************************/
ULONG EWS_Closest_Freq(ifreq)
LONG ifreq;
{
  static int valid[] = { AURATE5_5, AURATE6_6, AURATE8_0,
	AURATE9_6, AURATE11_0, AURATE16_0, AURATE18_9, AURATE22_1,
	AURATE27_4, AURATE32_0, AURATE33_1, AURATE37_8, AURATE44_1,
	AURATE48_0,0};
  LONG i = 0;
  LONG best = 8000;

  xa_audio_hard_buff = XA_HARD_BUFF_1K;
  while(valid[i])
  { 
    if (XA_ABS(valid[i] - ifreq) < XA_ABS(best - ifreq)) best = valid[i];
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
ULONG flag;
{ 
  ULONG old_ports = ews_audio_ports;
  LONG ret;

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
    fprintf(stderr,"Audio: couldn't toggle speaker %ld\n",errno);
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
ULONG flag;
{ 
  ULONG old_ports = ews_audio_ports;
  LONG ret;
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
    fprintf(stderr,"Audio: couldn't toggle headphone %ld\n",errno);
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
ULONG volume;
{ 
  int vol;
  int ret;
  
  ret = ioctl(devAudio, AUIOC_GETVOLUME, &audio_vol);
  if (ret == -1)
  {
    fprintf(stderr,"Audio: couldn't get volume%ld\n",errno);
    return;
  }

  vol= AUOUT_ATTEMAX-((volume*AUOUT_ATTEMAX)/XA_AUDIO_MAXVOL);
  audio_vol.out.left = audio_vol.out.right = vol;
  ret = ioctl(devAudio, AUIOC_SETVOLUME, &audio_vol);
  if (ret == -1)
  {
    fprintf(stderr,"Audio: couldn't set volume%ld\n",errno);
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
 */
void Sony_Audio_Init();
void Sony_Audio_Kill();
void Sony_Audio_Off();
void Sony_Audio_On();
void Sony_Adjust_Volume();
ULONG Sony_Closest_Freq();
void Sony_Speaker_Toggle();
void Sony_Headphone_Toggle();

/*
There are some Audio device type.
If machine type is NWS-1200/1400/1500/1700/1800/3200/3400/3700/3800
,set this flag.
#define SONY_TYPE2
*/

#ifndef SONY_TYPE2
/* NEWS5000/5900 */ 
#define	SONY_MAX_VOL	(16)
#define	SONY_MIN_VOL	(-16)
static int sony_audio_rate[] = { RATE8000, RATE9450, RATE11025, RATE12000, RATE16000, RATE18900, RATE22050, RATE24000, RATE32000, RATE37800, RATE44056, RATE44100, RATE48000, 0 } ;
#else
/* NEWS3000 */
#define	SONY_MAX_VOL	(0)
#define	SONY_MIN_VOL	(-32)
static int sony_audio_rate[] = 
		{ RATE8000, RATE9450, RATE18900, RATE37800, 0 } ;
#endif 

int sony_audio_buf_len ;

static int devAudio;
static struct sbparam	audio_info ;

/********** XA_Audio_Setup **********************
*
* Also defines Sony Specific variables.
*
*****/
void XA_Audio_Setup()
{
	XA_Audio_Init         = Sony_Audio_Init;
	XA_Audio_Kill         = Sony_Audio_Kill;
	XA_Audio_Off          = Sony_Audio_Off;
	XA_Audio_On           = Sony_Audio_On;
	XA_Closest_Freq       = Sony_Closest_Freq;
	XA_Set_Output_Port    = (void *)(0);
	XA_Speaker_Tog        = Sony_Speaker_Toggle;
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
	int	ret ;
	int	type ;

	DEBUG_LEVEL2 fprintf(stderr,"Sony_Audio_Init\n");
	if (xa_audio_present != XA_AUDIO_UNK) return;
	devAudio = open("/dev/sb0", O_WRONLY | O_NDELAY);
	if (devAudio == -1)
	{
	  if (errno == EBUSY) fprintf(stderr,"Audio_Init: Audio device is busy. - ");
	  else fprintf(stderr,"Audio_Init: Error opening audio device. - ");
	  fprintf(stderr,"Will continue without audio\n");
	  xa_audio_present = XA_AUDIO_ERR;
	  return;
	}

	DEBUG_LEVEL1 fprintf(stderr,"SONY AMD AUDIO\n");
	xa_audio_hard_type  = XA_AUDIO_SUN_AU;
	xa_audio_hard_freq  = 8000;
	xa_audio_hard_buff  = XA_HARD_BUFF_1K;         /* default buffer size */
	xa_audio_hard_bps   = 1;
	xa_audio_hard_chans = 1;
	Gen_Signed_2_Ulaw() ;

	audio_info.sb_mode    = LOGPCM ;
	audio_info.sb_format  = BSZ128 ;
	audio_info.sb_compress= MULAW ;
	audio_info.sb_rate    = RATE8000 ;
	audio_info.sb_channel = MONO ;
	audio_info.sb_bitwidth= RES8B ;
	audio_info.sb_emphasis= EMPH_OFF ;

	ret = ioctl(devAudio, SBIOCSETPARAM, &audio_info);
	if (ret)
	{
		fprintf(stderr,"AUDIO BRI FATAL ERROR %ld\n",errno);
		TheEnd1("SONY AUDIO BRI FATAL ERROR\n");
	}

	ret = ioctl(devAudio, SBIOCGETPARAM, &audio_info);
	if (ret == -1) fprintf(stderr,"Can't get audio_info %ld\n",errno);
	xa_interval_id = 0;
	xa_audio_present = XA_AUDIO_OK;
	DEBUG_LEVEL2 fprintf(stderr,"   success \n");
	Init_Audio_Ring(xa_audio_ring_size,
		(XA_AUDIO_MAX_RING_BUFF * xa_audio_hard_bps) );

	sony_audio_buf_len = 32768 ; /* 32Kbyte */
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
ULONG flag;
{ long ret;
	DEBUG_LEVEL1 fprintf(stderr,"Sony_Audio_Off\n");
	if (xa_audio_status != XA_AUDIO_STARTED) return;

	/* SET FLAG TO TELL FREE RUNNING OUTPUT ROUTINE */
	xa_audio_status = XA_AUDIO_STOPPED;

	/* TURN OFF SOUND ??? */ 
	Sony_Adjust_Volume(XA_AUDIO_MINVOL);

  	/* FLUSH AUDIO DEVICE */
    	ret = ioctl(devAudio, SBIOCFLUSH, 0);
      	if (ret == -1) 
	    fprintf(stderr,"Sony Audio: off flush err %ld\n",errno);

	xa_time_audio = -1;
	xa_audio_flushed = 0;

  	/* FLUSH AUDIO DEVICE AGAIN */
    	ret = ioctl(devAudio, SBIOCFLUSH, 0);
      	if (ret == -1) 
	    fprintf(stderr,"Sony Audio: off flush err %ld\n",errno);

	/* RESTORE ORIGINAL VOLUME */
	Sony_Adjust_Volume(xa_audio_volume);
}

/********** Sony_Audio_On **********************
* Turn On Audio Stream.
*
*****/
void Sony_Audio_On()
{
  int ret ;
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
      a_info.sb_format  = BSZ128 ;
      a_info.sb_compress= MULAW ;
      a_info.sb_rate    = xa_snd_cur->hfreq ;
      a_info.sb_channel = MONO ;
      a_info.sb_bitwidth= RES8B ;
      a_info.sb_emphasis= EMPH_OFF ;
      ret=ioctl(devAudio, SBIOCSETPARAM, &a_info); 
      if( ret == -1 )fprintf(stderr,"audio set freq: freq %lf errno %ld\n",
					xa_snd_cur->hfreq, errno);
      xa_audio_hard_freq = xa_snd_cur->hfreq;
    }

    /* xa_snd_cur gets changes in Update_Ring() */
    xa_out_time = 250; /* keep audio fed 250ms ahead of video - could be 500*/
    xa_out_init = xa_audio_ring_size - 1;
    xa_interval_time = xa_snd_cur->ch_time / xa_audio_divtest;

    DEBUG_LEVEL1 fprintf(stderr,"ch_time %ld out_time %ld int time %ld \n",xa_snd_cur->ch_time,xa_out_time,xa_interval_time);

    xa_audio_status = XA_AUDIO_STARTED;
    XA_Flush_Ring();
    XA_Update_Ring(1000);

    if (xa_interval_time == 0) xa_interval_time = 1;
    xa_time_now = XA_Read_AV_Time();  /* get new time */
    Merged_Audio_Output();
  }
}
/********** Sony_Closest_Freq **********************************************
*
****************************************************************************/
ULONG Sony_Closest_Freq(ifreq)
LONG	ifreq;
{
	LONG i = 0 ;
	LONG best = 8000 ;

	xa_audio_hard_buff = XA_HARD_BUFF_1K;
	while(sony_audio_rate[i])
	{
	 if (XA_ABS(sony_audio_rate[i] - ifreq) < XA_ABS(best - ifreq)) 
				best = sony_audio_rate[i];
	 i++;
	}
	return best ;
}
/************* Sony_Speaker_Toggle *****************************************
*
* flag = 0  turn speaker off
* flag = 1  turn speaker on
* flag = 2  toggle speaker
****************************************************************************/
void Sony_Speaker_Toggle(flag)
ULONG flag;
{
	 return ;
}

/************* Sony_Headphone_Toggle *****************************************
*
* flag = 0  turn headphones off
* flag = 1  turn headphones on
* flag = 2  toggle headphones
****************************************************************************/
void Sony_Headphone_Toggle(flag)
ULONG flag ;
{
	 return ;
}
/********** Sony_Adjust_Volume **********************/
void Sony_Adjust_Volume(volume)
ULONG volume;
{ 

  struct sblevel 	gain ;
  int			mflg ;

  if( volume == SONY_MIN_VOL )
  {
	mflg = MUTE_ON ;
	ioctl(devAudio,SBIOCMUTE, &mflg);
  }
  else
  {
	mflg = MUTE_OFF;
	ioctl(devAudio,SBIOCMUTE, &mflg);

  	gain.sb_left = SONY_MIN_VOL +
    	    ((volume * (SONY_MAX_VOL - SONY_MIN_VOL)) / XA_AUDIO_MAXVOL);
  	if (gain.sb_left > SONY_MAX_VOL) 
		gain.sb_left = SONY_MAX_VOL;

	gain.sb_left = gain.sb_right = gain.sb_left<<16 ; 
	ioctl(devAudio,SBIOCSETOUTLVL, &gain);
   }
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
void Linux_Audio_On();
ULONG Linux_Closest_Freq();
void Linux_Speaker_Toggle();
void Linux_Headphone_Toggle();
void Linux_Adjust_Volume();

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

  devAudio = open("/dev/dsp", O_WRONLY | O_NDELAY, 0);
  if (devAudio == -1)
  { fprintf(stderr,"Can't Open Linux Audio device\n");
    xa_audio_present = XA_AUDIO_ERR;
    return;
  }
  ioctl(devAudio, SNDCTL_DSP_SYNC, NULL);
  /* SETUP SAMPLE SIZE */
  tmp = 16;
#ifdef XA_LINUX_NEWER_SND
  ret = ioctl(devAudio, SNDCTL_DSP_SAMPLESIZE, &tmp);
#else
  ret = ioctl(devAudio, SNDCTL_DSP_SAMPLESIZE, tmp);
#endif
  if ((ret == -1) || (tmp == 8)) xa_audio_hard_bps = 1;
  else xa_audio_hard_bps = 2;

  /* SETUP Mono/Stereo */
  tmp = 0;  /* mono(0) stereo(1) */ 
#ifdef XA_LINUX_NEWER_SND
  ret = ioctl(devAudio, SNDCTL_DSP_STEREO, &tmp);
#else
  ret = ioctl(devAudio, SNDCTL_DSP_STEREO, tmp);
#endif
  if (ret == -1) fprintf(stderr,"Linux Audio: Error setting mono\n");
  xa_audio_hard_chans = 1;

  xa_audio_hard_freq  = 11025;  /* 22050 and sometimes 44100 */
  xa_audio_hard_buff  = XA_HARD_BUFF_1K;
#ifdef XA_LINUX_NEWER_SND
  ret = ioctl(devAudio, SNDCTL_DSP_GETBLKSIZE, &tmp);
#else
  ret = ioctl(devAudio, SNDCTL_DSP_GETBLKSIZE, 0);
#endif
  if (ret == -1) fprintf(stderr,"Linux Audio: Error getting buf size\n");
  /* POD NOTE: should probably do something with this value. :^) */
  /* Maybe XA_AUDIO_MAX_RING_BUFF should be a variable */
  
  ioctl(devAudio, SNDCTL_DSP_SYNC, NULL);

  devMixer = open("/dev/mixer",  O_RDONLY | O_NDELAY, 0);
  /* Mixer only present on SB Pro's and above */
  /* if not present then it's set to -1 and ignored later */
  /* THOUGHT: what about doing mixer ioctl to the /dev/dsp device??? */
  if (devMixer < 0) devMixer = devAudio;
   /* determine what volume settings exist */
  { int devices;
#ifdef XA_LINUX_NEWER_SND
    ret = ioctl(devMixer, SOUND_MIXER_READ_DEVMASK, &devices);
    if (ret == -1) devices = 0;
#else
    devices = ioctl(devMixer, SOUND_MIXER_READ_DEVMASK, 0 );
    if (devices == -1) devices = 0;
#endif
    if (devices & (1 << SOUND_MIXER_PCM)) 
		linux_vol_chan = SOUND_MIXER_PCM;
    else	linux_vol_chan = SOUND_MIXER_VOLUME;
  }
 
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
ULONG flag;
{
  if (xa_audio_status != XA_AUDIO_STARTED) return;
  /* SET FLAG TO STOP OUTPUT ROUTINE */
  xa_audio_status = XA_AUDIO_STOPPED;
  Linux_Adjust_Volume(XA_AUDIO_MINVOL);
  ioctl(devAudio, SNDCTL_DSP_SYNC, NULL);
  xa_time_audio = -1;
  xa_audio_flushed = 0;
  ioctl(devAudio, SNDCTL_DSP_SYNC, NULL);
  Linux_Adjust_Volume(xa_audio_volume);
}

/********** Linux_Audio_On ***************************************************
 * Startup Linux Audio
 *
 *****************************************************************************/
void Linux_Audio_On()
{
  DEBUG_LEVEL2 fprintf(stderr,"Linux_Audio_On \n");
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
#ifdef XA_LINUX_NEWER_SND
      ret = ioctl(devAudio, SNDCTL_DSP_SPEED, &tmp);
#else
      tmp = ioctl(devAudio, SNDCTL_DSP_SPEED, tmp);
#endif
      if ((ret == -1) || (tmp != xa_snd_cur->hfreq))
	fprintf(stderr,"Linux_Audio: err setting freq %lx\n"
						,xa_snd_cur->hfreq);
      xa_audio_hard_freq = xa_snd_cur->hfreq;
    }
    ioctl(devAudio, SNDCTL_DSP_SYNC, NULL);
    Linux_Adjust_Volume(xa_audio_volume);

    /* xa_snd_cur gets changes in Update_Ring() */
    xa_out_time = 250;  /* keep audio 250 ms ahead of video. could be 500ms */
    xa_out_init = xa_audio_ring_size - 1;
    xa_interval_time = xa_snd_cur->ch_time / xa_audio_divtest;

    xa_audio_status = XA_AUDIO_STARTED;
    XA_Flush_Ring();
    XA_Update_Ring(1000);

    if (xa_interval_time == 0) xa_interval_time = 1;
    xa_time_now = XA_Read_AV_Time();  /* get new time */
    Merged_Audio_Output();
  }
}

/********** Linux_Closest_Freq **********************************************
 * Return closest support frequeny and set hard_buff.
 *
 *****************************************************************************/
ULONG Linux_Closest_Freq(ifreq)
LONG ifreq;	/* incoming frequency */
{ static int valid[] = {11025, 22050, 44100, 0};
  LONG i = 0;
  LONG tmp_freq,ofreq = ifreq;
#ifdef XA_LINUX_NEWER_SND
  LONG ret;
#endif

   /* search up for closest frequency */
  while(valid[i])
  {
    if (XA_ABS(valid[i] - ifreq) < XA_ABS(ofreq - ifreq)) ofreq = valid[i];
    i++;
  }

  tmp_freq = ofreq;
#ifdef XA_LINUX_NEWER_SND
  ret = ioctl(devAudio, SNDCTL_DSP_SPEED, &tmp_freq);
  if (ret == -1) tmp_freq = 0;
#else
  tmp_freq = ioctl(devAudio, SNDCTL_DSP_SPEED, tmp_freq);
  if (tmp_freq == -1) tmp_freq = 0;
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
ULONG flag;
{
  return;
}

/********** Linux_Headphone_Toggle ********************************************
 * Turn off/on/toggle Linux's Headphone(if possible)
 * flag= 0   1   2 
 *****************************************************************************/
void Linux_Headphone_Toggle(flag)
ULONG flag;
{
  return;
}

/********** Linux_Adjust_Volume ***********************************************
 * Routine for Adjusting Volume on Linux
 *
 *****************************************************************************/
void Linux_Adjust_Volume(volume)
ULONG volume;
{ ULONG adj_volume;

  if (devMixer < 0) return;
  adj_volume = LINUX_MIN_VOL +
        ((volume * (LINUX_MAX_VOL - LINUX_MIN_VOL)) / XA_AUDIO_MAXVOL);
  if (adj_volume > LINUX_MAX_VOL) adj_volume = LINUX_MAX_VOL;
  adj_volume |= adj_volume << 8;	/* left channel | right channel */
#ifdef XA_LINUX_NEWER_SND
  ioctl(devMixer, MIXER_WRITE(linux_vol_chan), &adj_volume);
#else
  ioctl(devMixer, MIXER_WRITE(linux_vol_chan), adj_volume);
#endif
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
void SGI_Audio_On();
ULONG SGI_Closest_Freq();
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
ULONG flag;
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
  SGI_Adjust_Volume(xa_audio_volume);
}

/********** SGI_Audio_On **********************
 * Startup SGI Audio
 *****/
void SGI_Audio_On()
{
  DEBUG_LEVEL2 fprintf(stderr,"SGI_Audio_On \n");
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
    xa_interval_time = xa_snd_cur->ch_time / xa_audio_divtest;

    xa_audio_status = XA_AUDIO_STARTED;
    XA_Flush_Ring();
    XA_Update_Ring(1000);

    if (xa_interval_time == 0) xa_interval_time = 1;
    xa_time_now = XA_Read_AV_Time();  /* get new time */
    Merged_Audio_Output();
  }
}

/********** SGI_Closest_Freq **********************
 * Return closest support frequeny and set hard_buff.
 * The IRIX 4.0.5 audio.h file lists the following frequencies:
 * {8000,11025,16000,22050,32000,44100,48000}
 *****/
ULONG SGI_Closest_Freq(ifreq)
LONG ifreq;
{ static int valid[] = { 8000,11025,16000,22050,32000,44100,0};
  LONG i = 0;
  LONG ofreq = 8000;
  long sgi_params[2];

  while(valid[i])
  { 
    if (XA_ABS(valid[i] - ifreq) < XA_ABS(ofreq - ifreq)) ofreq = valid[i];
    i++;
  }
  sgi_params[0] = AL_OUTPUT_RATE;
  sgi_params[1] = ofreq;
  ALsetparams(AL_DEFAULT_DEVICE, sgi_params, 2); 
  ALgetparams(AL_DEFAULT_DEVICE, sgi_params, 2); 
  if (ofreq != sgi_params[1])
	fprintf(stderr,"SGI AUDIO: freq gotten %ld wasn't wanted %ld\n",
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
ULONG flag;
{
  return;
}

/********** SGI_Headphone_Toggle **********************
 * Turn off/on/toggle SGI's Headphone(if possible)
 * flag= 0   1   2 
 *****/
void SGI_Headphone_Toggle(flag)
ULONG flag;
{
  return;
}

/********** SGI_Adjust_Volume **********************
 * Routine for Adjusting Volume on SGI
 *
 *****/
void SGI_Adjust_Volume(volume)
ULONG volume;
{ ULONG adj_volume;
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
void HP_Audio_On();
ULONG HP_Closest_Freq();
void HP_Speaker_Toggle();
void HP_Headphone_Toggle();
void HP_Adjust_Volume();
static void HP_Audio_Output();
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
int hp_audio_paused = FALSE;

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
  hp_audio_paused = FALSE;
}

/********** HP_Print_Error **********************
 * print error message
 *****/
void HP_Print_Error(pre_str,post_str,status)
char *pre_str,*post_str;
AError status;
{ char err_buff[132];
  if (pre_str) fprintf(stderr,"%s (%ld)",pre_str,(int)(status));
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
  if (pre_str) fprintf(stderr,"%s (%ld)",pre_str,(int)(errorCode));
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
  fprintf(stderr,"HP_Audio_Init: theContext = %lx\n",(ULONG)theContext);

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

  fprintf(stderr,"REQ- format %ld  bits %ld  rate %ld  chans %ld bo %ld\n",
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

  fprintf(stderr,"REQ+ format %ld  bits %ld  rate %ld  chans %ld bo %ld\n",
	req_attribs.attr.sampled_attr.data_format,
	req_attribs.attr.sampled_attr.bits_per_sample,
	req_attribs.attr.sampled_attr.sampling_rate,
	req_attribs.attr.sampled_attr.channels,
	req_byteorder );

  fprintf(stderr,"PLAY format %ld  bits %ld  rate %ld  chans %ld\n",
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

DEBUG_LEVEL1 fprintf(stderr,"HP hard_type %lx\n",xa_audio_hard_type);

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
      { ULONG i;
        for(i=0; i < num; i++) hp_sample_rates[i] = tmp[i];
        hp_sample_rates[num] = 0;
/*
DEBUG_LEVEL1
*/
{ 
  fprintf(stderr,"HP sampling rates: ");
  for(i=0;i<num;i++) fprintf(stderr,"<%ld>",hp_sample_rates[i]);
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
	if (xa_audio_port & XA_AUDIO_PORT_INT)
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
	if (xa_audio_port & XA_AUDIO_PORT_INT)
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
  if (streamSocket < 0) fprintf(stderr,"HP_Audio: socket err %ld\n",errno);

  ret = connect(streamSocket,(struct sockaddr *)&audio_stream.tcp_sockaddr,
					sizeof(struct sockaddr_in) );
  if (ret < 0) fprintf(stderr,"HP_Audio: connect error %ld\n",errno);

  /* Pause Audio Stream */
  APauseAudio( audio_connection, hp_trans_id, NULL, NULL );
  hp_audio_paused = TRUE;
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
ULONG flag;
{
  DEBUG_LEVEL2 fprintf(stderr,"HP_Audio_Off\n");
  if (xa_audio_status != XA_AUDIO_STARTED) return;
  /* SET FLAG TO STOP OUTPUT ROUTINE */
  xa_audio_status = XA_AUDIO_STOPPED;

  /* POD NOTE: USE PAUSE FOR NOW */
/*
  APauseAudio(audio_connection,hp_trans_id, NULL, NULL);
  hp_audio_paused = TRUE;
*/
  xa_audio_status = XA_AUDIO_STOPPED;
  xa_time_audio = -1;
  xa_audio_flushed = 0;
}

/********** HP_Audio_On ***************************************************
 * Startup HP Audio
 *****/
void HP_Audio_On()
{ AudioAttrMask attr_mask;
  DEBUG_LEVEL2 fprintf(stderr,"HP_Audio_On \n");
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
    xa_out_time = xa_snd_cur->ch_time * 2;
    xa_out_init = xa_audio_ring_size - 1;
    xa_interval_time = xa_snd_cur->ch_time / xa_audio_divtest;

    xa_audio_status = XA_AUDIO_STARTED;
    XA_Flush_Ring();
    XA_Update_Ring(1000);

    if (xa_interval_time == 0) xa_interval_time = 1;
    xa_time_now = XA_Read_AV_Time();  /* get new time */
    xa_interval_id = XtAppAddTimeOut(theContext,1,
		(XtTimerCallbackProc)HP_Audio_Output,(XtPointer)(NULL));
  }
}

/********** HP_Closest_Freq **********************
 * Return closest support frequeny and set hard_buff.
 *****/
ULONG HP_Closest_Freq(ifreq)
LONG ifreq;
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
ULONG flag;
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
ULONG flag;
{
  return;
}

/********** HP_Adjust_Volume **********************
 * Routine for Adjusting Volume on HP
 *
 *****/

/* AUnityGain is 0. AZeroGain is 0x80000000 */
void HP_Adjust_Volume(volume)
LONG volume;
{ LONG adj_volume;
  AGainDB play_gain,tmp_gain;
  int ret;
  adj_volume = HP_MIN_VOL + 
	((volume * (HP_MAX_VOL - HP_MIN_VOL)) / XA_AUDIO_MAXVOL);
  if (adj_volume > HP_MAX_VOL) adj_volume = HP_MAX_VOL;
  play_gain = (AGainDB)adj_volume;
  AGetSystemChannelGain(audio_connection,ASGTPlay,ACTMono,&tmp_gain,NULL);
  fprintf(stderr,"Get Gain %lx adj_vol %lx  hpmin %lx hpmax %lx\n",tmp_gain,adj_volume,HP_MIN_VOL, HP_MAX_VOL);
/*
  ASetSystemChannelGain(audio_connection,ASGTPlay,ACTMono,play_gain,NULL);
  ASetChannelGain(audio_connection,hp_trans_id,ACTMono,play_gain,NULL);
  ASetGain(audio_connection,hp_trans_id,play_gain,NULL);
*/
  ASetSystemPlayGain(audio_connection,play_gain,NULL);
}

/********** HP_Audio_Output ***************************************************
 * Set Volume if needed and then write audio data to audio device.
 *
 *****/
static void HP_Audio_Output()
{ int ret,time_diff;
  
  xa_audio_out_sem = 1;
  xa_interval_id = 0;
  if (xa_audio_status != XA_AUDIO_STARTED) {xa_audio_out_sem = 0; return; }
  time_diff = xa_time_audio - xa_time_now; /* how far ahead is audio */
  if (time_diff > xa_out_time) /* potentially ahead */
  {
    xa_time_now = XA_Read_AV_Time();  /* get new time */
    time_diff = xa_time_audio - xa_time_now;
    if (time_diff > xa_out_time)  /* definitely ahead */
    { 
      xa_interval_id = XtAppAddTimeOut(theContext, xa_interval_time,
			(XtTimerCallbackProc)HP_Audio_Output,(XtPointer)(NULL));
      xa_audio_out_sem = 0;
      return;
    }
  }

    if (xa_audio_ring->len)
    { ULONG len,tmp_time,tmp_timelo,ret;

DEBUG_LEVEL1 fprintf(stderr,"HP-out-send\n");
      if (hp_audio_paused == TRUE)
      {
        DEBUG_LEVEL1 fprintf(stderr,"HP-out-unpause\n");
	AResumeAudio( audio_connection, hp_trans_id, NULL, NULL );
	hp_audio_paused = FALSE;
      }
      ret = write(streamSocket,xa_audio_ring->buf,xa_audio_ring->len);
DEBUG_LEVEL1 fprintf(stderr,"HP-out-ret %lx\n",ret);
/* now that somethings in the buffer, unpause audio if needed */
      if (xa_audio_synctst) XSync(theDisp, False); /* POD TEST EXPERIMENTAL */
      /* Adjust Volume if necessary */
      if (xa_audio_newvol==TRUE)
      { ULONG vol = (xa_audio_mute==TRUE)?(XA_AUDIO_MINVOL):(xa_audio_volume);
  	HP_Adjust_Volume(vol); 
  	xa_audio_newvol = FALSE;
      }
      xa_time_now = XA_Read_AV_Time(); /* up date time */
      xa_time_audio   += xa_audio_ring->time;
      xa_timelo_audio += xa_audio_ring->timelo;
      if (xa_timelo_audio & 0xff000000)
		{ xa_time_audio++; xa_timelo_audio &= 0x00ffffff; }
      xa_audio_ring->len = 0; /* above write must be blocking */
      xa_audio_ring = xa_audio_ring->next;  /* move to next */
      if (xa_audio_status != XA_AUDIO_STARTED) { xa_audio_out_sem = 0; return;}
      if (xa_update_ring_sem==0)
      {
        xa_update_ring_sem = 1;
        XA_Update_Ring(2);
        xa_update_ring_sem = 0;

      }
      xa_time_now = XA_Read_AV_Time();  /* get new time */
      if (xa_audio_status != XA_AUDIO_STARTED) { xa_audio_out_sem = 0; return;}
      xa_interval_id = XtAppAddTimeOut(theContext,xa_interval_time, 
			(XtTimerCallbackProc)HP_Audio_Output,(XtPointer)(NULL));
    } else xa_audio_status = XA_AUDIO_STOPPED;
    xa_audio_out_sem = 0;
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
void HPDEV_Audio_On();
ULONG HPDEV_Closest_Freq();
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

  /* Get description of /dev/audio: */
  if (ioctl (devAudio, AUDIO_DESCRIBE, &audioDesc))
  {
    perror ("ioctl AUDIO_DESCRIBE on /dev/audio");
    xa_audio_present = XA_AUDIO_ERR;
    return;
  }

  xa_audio_hard_type  = XA_AUDIO_SIGNED_2MB;
  xa_audio_hard_freq  = 0;	  /* none yet (HPDEV_Audio_On does this job) */
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
ULONG flag;
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

/********** HPDEV_Audio_On ***************************************************
 * Startup HP Audio
 *****/
void HPDEV_Audio_On()
{
  DEBUG_LEVEL2 fprintf(stderr,"HPDEV_Audio_On\n");
  if (xa_audio_status == XA_AUDIO_STARTED) return;
  else if (xa_audio_present != XA_AUDIO_OK) return;
  else if (xa_snd_cur)
  {
    int ret,tmp,utime;

    /* Change frequency if necessary */
    if (xa_audio_hard_freq != xa_snd_cur->hfreq)
    { 
      if (ioctl (devAudio, AUDIO_SET_SAMPLE_RATE, xa_snd_cur->hfreq))
	perror ("ioctl AUDIO_SET_SAMPLE_RATE on /dev/audio");
      xa_audio_hard_freq = xa_snd_cur->hfreq;
    }

    /* xa_snd_cur gets changes in Update_Ring() */
    xa_out_time = 250; /* keep audio fed 250ms ahead of video */
    xa_out_init = xa_audio_ring_size - 1;
    xa_interval_time = xa_snd_cur->ch_time / xa_audio_divtest;

    xa_audio_status = XA_AUDIO_STARTED;
    XA_Flush_Ring();
    XA_Update_Ring(1000);

    if (xa_interval_time == 0) xa_interval_time = 1;
    xa_time_now = XA_Read_AV_Time();  /* get new time */
    Merged_Audio_Output();
  }
}

/********** HPDEV_Closest_Freq **********************
 * Return closest support frequeny and set hard_buff.
 *****/
ULONG HPDEV_Closest_Freq(ifreq)
LONG ifreq;
{
  unsigned int rate_index;
  long nearest_frequency = 8000;
  long frequency_diff = 999999;

  xa_audio_hard_buff = XA_HARD_BUFF_1K;

  for (rate_index = 0; rate_index < audioDesc.nrates; ++rate_index)
    if (abs (audioDesc.sample_rate[rate_index] - ifreq) <= frequency_diff)
    {
      nearest_frequency = audioDesc.sample_rate[rate_index];
      frequency_diff = abs (audioDesc.sample_rate[rate_index] - ifreq);
    }

  return nearest_frequency;
}

/********** HPDEV_Speaker_Toggle **********************
 * Turn off/on/toggle HP's Speaker(if possible)
 * flag= 0   1   2 
 *****/
void HPDEV_Speaker_Toggle(flag)
ULONG flag;
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
ULONG flag;
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
ULONG flag;
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
LONG volume;
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
void  AF_Audio_On();
void  AF_Adjust_Volume();
ULONG AF_Closest_Freq();
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
  AFattributes.play_gain = AF_MAP_TO_C_GAIN(xa_audio_volume);
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
ULONG flag;
{ 
  DEBUG_LEVEL2 fprintf(stderr,"AF_Audio_Off\n");
  if (xa_audio_status == XA_AUDIO_STOPPED) return;

  /* XXX */

  xa_audio_status = XA_AUDIO_STOPPED;
}

/********** AF_Audio_On **********************
 * Turn On Audio Stream.
 *
 *****/
void AF_Audio_On()
{
  DEBUG_LEVEL2 fprintf(stderr,"AF_Audio_On \n");
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
    xa_interval_time = xa_snd_cur->ch_time / xa_audio_divtest;

    xa_audio_status = XA_AUDIO_STARTED;
    XA_Flush_Ring();
    XA_Update_Ring(1000);

    AFtime0 = AFbaseT = AFGetTime(ac) + TOFFSET;
    if (xa_interval_time == 0) xa_interval_time = 1;
    xa_time_now = XA_Read_AV_Time();  /* get new time */
    Merged_Audio_Output();
  }
}

/*************
 * flag = 0  turn speaker off
 * flag = 1  turn speaker on
 * flag = 2  toggle speaker
 */
void AF_Speaker_Toggle(flag)
ULONG flag;
{ 
  DEBUG_LEVEL2 fprintf(stderr,"AF_Speaker_Toggle\n");
}

void AF_Headphone_Toggle(flag)
ULONG flag;
{ 
  DEBUG_LEVEL2 fprintf(stderr,"AF_Headphone_Toggle\n");
}

/********** AF_Adjust_Volume **********************
 * Routine for Adjusting Volume on a AF
 *
 *****/
void AF_Adjust_Volume(volume)
ULONG volume;
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
 *   ULONG xa_audio_hard_buff		must set but not larger than
 *					XA_AUDIO_MAX_RING_BUF size
 */
ULONG AF_Closest_Freq(ifreq)
LONG ifreq;
{
 return (DFREQ(AFdevice));
}

#endif
/****************************************************************************/
/******************* END OF AF SPECIFIC ROUTINES *********************/
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
 */

void  NAS_Audio_Init();
void  NAS_Audio_Kill();
void  NAS_Audio_Off();
void  NAS_Audio_On();
void  NAS_Adjust_Volume();
ULONG NAS_Closest_Freq();
void NAS_Speaker_Toggle();
void NAS_Headphone_Toggle();


static AuServer *NASaud;
static AuDeviceID	NASdevice=-1;
static Sound NASSound;
static AuFlowID NASFlowID=-1;
static AuElement NASelements[2];
static int NAS_Flow_Initialized=0;

static AuDeviceAttributes *nas_da = 0;

/********** XA_Audio_Setup **********************
 * 
 * Also defines Sparc Specific variables.
 *
 *****/
void XA_Audio_Setup()
{
  DEBUG_LEVEL2 fprintf(stdout,"XA_Audio_Setup()\n");
  XA_Audio_Init		= NAS_Audio_Init;
  XA_Audio_Kill		= NAS_Audio_Kill;
  XA_Audio_Off		= NAS_Audio_Off;
  XA_Audio_On		= NAS_Audio_On;
  XA_Closest_Freq	= NAS_Closest_Freq;
  XA_Set_Output_Port	= (void *)(0);
  XA_Speaker_Tog	= NAS_Speaker_Toggle;
  XA_Headphone_Tog	= NAS_Headphone_Toggle;
  XA_LineOut_Tog	= NAS_Headphone_Toggle;
  XA_Adjust_Volume	= NAS_Adjust_Volume;

  xa_snd_cur = 0;
  xa_audio_present = XA_AUDIO_UNK;
  xa_audio_status  = XA_AUDIO_STOPPED;
  xa_audio_ring_size  = 8;
  nas_da = 0;
  NAS_Flow_Initialized = 0;
}


/* XANIM and NAS use the same 0 to 100 volume scale, so no mapping is
 * required
 */


/*
 * Find a suitable default device (the first mono output device ).
 *  Returns device number.
 *
 * Returns -1 if no suitable device can be found.
 */
static AuDeviceID FindDefaultDevice(aud)
AuServer *aud;
{
        int     i;
	int 	num=AuServerNumDevices(aud);
	AuDeviceAttributes *attr;

        for(i=0; i<num; i++) {
		attr=AuServerDevice(aud,i);
		/* Right now, we look for the first output device.
	 	 * Perhaps later, we will care about mono vs. stereo
		 * or left vs. right channel, or line vs. speaker.
		 * For now, just get this right.
		 */
		if ((AuDeviceKind(attr)==AuComponentKindPhysicalOutput) &&
		    (AuDeviceNumTracks(attr) == 1))
                        return AuDeviceIdentifier(attr);
        }
        return -1;
}

void XA_No_Audio_Support()
{
  fprintf(stderr,"AUDIO SUPPORT NOT COMPILED IN THIS EXECUTABLE.\n");
  return;
}

/********** NAS_Audio_Init **********************
 *****/
void NAS_Audio_Init()
{ 
  char *errmsg;
  AuStatus austat;
  AuMask mask,flags;

  DEBUG_LEVEL2 fprintf(stdout,"NAS_Audio_Init\n");
  if (xa_audio_present != XA_AUDIO_UNK) return;

  /* Open connection. */
  if ( (NASaud = AuOpenServer(NULL,0,NULL,0,NULL,&errmsg)) == NULL) {
      fprintf(stderr, "Could not open connection to NAS audio server.\n");
      fprintf(stderr, "Error is \"%s\".\n",errmsg);
      fprintf(stderr, "Disabling audio service.\n");
      /* As long as xa_audio_present isn't OK, audio is effectively disabled*/
      /* but might as well really disable it */
      AuFree(errmsg);
      XA_Null_Audio_Setup();
      xa_audio_present = XA_AUDIO_UNK;
      return;
  }
  AuXtAppAddAudioHandler(theContext,NASaud);

  NASdevice = FindDefaultDevice(NASaud);
  
DEBUG_LEVEL5 (void)fprintf(stderr,"Setting volume %d\n",xa_audio_volume);
  /* Parameter changing (line mode and gain) is not tricky, but it is a 
   * multi-step process.  The first step is to obtain the device attributes.
   * Then, with the device attributes in hand, check to see if the desired
   * parameters CAN be changed.  If not, bail out.  If so, then set up a 
   * device parameters structure and change the parameters. 
   * This code would be better off abstracted somewhere else, but it is only
   * used in a couple of places right now.  It is very expedient just to 
   * place it in-line.  Actually, it would be nice if the NAS library provided
   * a convenience function to do this -- HINT HINT.
   * Oh, yes -- I'm not happy with the error reporting either.   The problem
   * is that, on many legal server configurations, these attribute setting
   * calls will fail (notably, the headphone/speaker toggling).  Do I want
   * those messages blathering on, or do I want silent failures?  I opted to
   * be able to selectively turn the messages on with the debugging mechanism. 
   */
  nas_da=AuGetDeviceAttributes(NASaud,NASdevice,&austat);  
  if (austat != AuSuccess) 
  { 
    DEBUG_LEVEL1 fprintf(stderr, "NAS_Audio_Init:Could not get attributes for device 0x%x, NAS error %d\n",NASdevice,austat);
    AuCloseServer(NASaud);
    xa_audio_present = XA_AUDIO_ERR;
    return;
  }
  mask=AuDeviceChangableMask(nas_da);
  flags=0;
  if (mask&AuCompDeviceGainMask){
  	AuDeviceGain(nas_da)=AuFixedPointFromSum(xa_audio_volume,0);
	flags |= AuCompDeviceGainMask;
  } else {
	DEBUG_LEVEL1 (void)fprintf(stderr,"NAS_Audio_Init: Cannot adjust gain on device 0x%x\n",NASdevice);
  }
  if (mask&AuCompCommonFormatMask){
	/* The type here must agree with what is set for 
	 * xa_audio_hard_type, and with the type set when making the
	 * import element of the flow.
	 */
  	AuDeviceFormat(nas_da)= AuFormatLinearSigned16MSB; 
	flags |= AuCompCommonFormatMask;
  } 
  if (flags){
  	AuSetDeviceAttributes(NASaud,NASdevice,
		flags,nas_da,&austat);
  	if (austat != AuSuccess) 
        {
      	  fprintf(stderr, "NAS_Audio_Init: Could not set attributes for device 0x%x,NAS error %d\n",NASdevice,austat);
      	  AuCloseServer(NASaud);
	  xa_audio_present = XA_AUDIO_ERR;
	  return;
  	}
  }
  AuSync(NASaud,AuFalse);

  /* get default frequency - not really necessary */
  xa_audio_hard_freq = 8000; /*Not the best way to do this, but it will work */

  xa_audio_hard_bps = 2;
  xa_audio_hard_chans = 1;
  xa_audio_hard_buff  = XA_HARD_BUFF_1K;
  xa_audio_hard_type  = XA_AUDIO_SIGNED_2MB;

  xa_interval_id = 0;
  xa_audio_present = XA_AUDIO_OK;
  DEBUG_LEVEL2 fprintf(stdout,"   success \n");
  Init_Audio_Ring(xa_audio_ring_size,
			(XA_AUDIO_MAX_RING_BUFF * xa_audio_hard_bps) );
  NASFlowID=AuCreateFlow(NASaud,&austat);
  if (austat != AuSuccess)
  {
    (void)fprintf(stderr,"NAS_Audio_Init: AuCreateFlow is bad\n");
    AuCloseServer(NASaud);
    xa_audio_present = XA_AUDIO_ERR;
    return;
  }
}

/********** NAS_Audio_Kill **********************
 *****/
void NAS_Audio_Kill()
{ 
DEBUG_LEVEL2 fprintf(stdout,"NAS_Audio_Kill\n");

  NAS_Audio_Off(0);
  xa_audio_present = XA_AUDIO_UNK;

  /* XXX */
  AuFreeDeviceAttributes(NASaud,1,nas_da);
  if (NASFlowID!= -1)
	AuDestroyFlow(NASaud,NASFlowID,NULL);
  AuCloseServer(NASaud);

  Kill_Audio_Ring();
}

/********** NAS_Audio_Off **********************
 * Stop Audio Stream
 *
 *****/
void NAS_Audio_Off(flag)
ULONG flag;
{ AuStatus austat;

  DEBUG_LEVEL2 fprintf(stdout,"NAS_Audio_Off\n");
  if (xa_audio_status != XA_AUDIO_STARTED) return;

  /* SET FLAG TO STOP OUTPUT ROUTINE */
  xa_audio_status = XA_AUDIO_STOPPED;

  /* TURN OFF SOUND ??? */
  NAS_Adjust_Volume(XA_AUDIO_MINVOL);

  /* FLUSH AUDIO DEVICE */
/*
  AuFlush(NASaud);
  AuSync(NASaud,AuTrue);
*/
  DEBUG_LEVEL1 fprintf(stderr,"NAS: stop flow\n");
  AuStopFlow(NASaud,NASFlowID,&austat);
  if (austat != AuSuccess) 
  {
    (void) fprintf(stderr, "NAS_Audio_Off:Could not Stop Flow NAS error %d\n",austat);
  }
  AuSync(NASaud,AuTrue);

  xa_time_audio = -1;
  xa_audio_flushed = 0;

  /* FLUSH AUDIO DEVICE */
  AuFlush(NASaud);
  AuSync(NASaud,AuTrue);

  /* RESTORE ORIGINAL VOLUME */
  NAS_Adjust_Volume(xa_audio_volume);
}

/********** NAS_Audio_On **********************
 * Turn On Audio Stream.
 *
 *****/
void NAS_Audio_On()
{
  AuStatus austat;
  AuMask mask,flags;

  DEBUG_LEVEL2 fprintf(stdout,"NAS_Audio_On \n");
  /* IF  it is the first time through here (NAS_Flow_Initialized == 0),
   * then we want to set up the flow (we cannot set up the flow until
   * we have a valid xa_snd_cur).  IF we already have a valid flow, then
   * we don't want to screw that up by creating a new one.
   */
  if (xa_audio_status == XA_AUDIO_STARTED) return;
  else if (xa_audio_present != XA_AUDIO_OK) return;
  else if (xa_snd_cur)
  { 
    if (!NAS_Flow_Initialized) 
    {
DEBUG_LEVEL1 fprintf(stderr,"NAS: init flow\n");
  	mask=AuDeviceChangableMask(nas_da);
  	flags=0;
  	if (mask & AuCompDeviceGainMask)
	{
          AuDeviceGain(nas_da)=AuFixedPointFromSum(xa_audio_volume,0);
	  flags |= AuCompDeviceGainMask;
  	} else 
	{
	  (void)fprintf(stderr,"NAS_Audio_On: Cannot adjust gain on device 0x%x\n",NASdevice);
  	}
  	if (flags)
	{
  	  AuDeviceGain(nas_da)=AuFixedPointFromSum(0,0);
  	  AuSetDeviceAttributes(NASaud,NASdevice,flags,nas_da,&austat);
	  if (austat != AuSuccess) 
          {
	    fprintf(stderr, "NAS_Audio_On:Could not set attributes for device 0x%x, NAS error %d\n",NASdevice,austat);
  	  }
	}
	/* The audio type here must agree with xa_audio_hard_type
 	 * and with the type set in NAS_Audio_Init().
	 */
/* NAS doesn't have real blocking writes. It fails with BadValue if you
 * overflow the internal buffer. Set internal buffer to be 100 times larger
 * than need.(could be up to ~400k but oh well.
 */
  	AuMakeElementImportClient(&(NASelements[0]), xa_snd_cur->hfreq, 
			AuFormatLinearSigned16MSB,1,AuTrue,
			(100 * xa_audio_hard_buff * xa_audio_hard_bps),
			128,0,NULL);
  	AuMakeElementExportDevice(&(NASelements[1]), 0, NASdevice,
		xa_snd_cur->hfreq, AuUnlimitedSamples,0,NULL);
	AuSetElements(NASaud,NASFlowID,AuTrue,2,NASelements,&austat);
  	if (austat != AuSuccess)
	{ 
	  (void)fprintf(stderr,"NAS_Audio_On: AuSetElements fails with error %d\n",austat);
      	  AuCloseServer(NASaud);
      	  exit(1);
  	}
	NAS_Flow_Initialized=1;
    }
DEBUG_LEVEL1 fprintf(stderr,"NAS: start flow\n");
    AuStartFlow(NASaud,NASFlowID,&austat);
    if (austat != AuSuccess) 
    {
	fprintf(stderr, "NAS_Audio_On:Could not Start Flow NAS error %d\n",austat);
    }

    /* Change Frequency If necessary */
    if (xa_audio_hard_freq != xa_snd_cur->hfreq)
    { 
	/* XXX */
      xa_audio_hard_freq = xa_snd_cur->hfreq;
    }

    /* xa_snd_cur gets changes in Update_Ring() */
    /* FINE TUNED from 4 to 20 */
    xa_out_time = 250;  /* keep audio fed XXms ahead of video - could be 500 */
    xa_out_init = xa_audio_ring_size - 1;
    xa_interval_time = xa_snd_cur->ch_time / xa_audio_divtest;

    xa_audio_status = XA_AUDIO_STARTED;
    XA_Flush_Ring();
    XA_Update_Ring(1000);

    if (xa_interval_time == 0) xa_interval_time = 1;
    xa_time_now = XA_Read_AV_Time();  /* get new time */
    Merged_Audio_Output();
  }
}

/*************
 * flag = 0  turn speaker off
 * flag = 1  turn speaker on
 * flag = 2  toggle speaker
 */
void NAS_Speaker_Toggle(flag)
ULONG flag;
{ 
  AuStatus austat;

  DEBUG_LEVEL2 fprintf(stdout,"NAS_Speaker_Toggle\n");

  /* If you think that this could be neater, consider yourself no-prized.
   * Of course it could be neater.  
   */
  if (!(AuDeviceChangableMask(nas_da) & AuCompDeviceLineModeMask))
  {
    (void)fprintf(stderr,"NAS_Speaker_Toggle: Device lacks ability to toggle speaker\n");
    /* AuFreeDeviceAttributes(NASaud,1,nas_da); POD NOTE: don't want to free */
    return;
  }

  switch (flag) 
  {
	case 0:
		AuDeviceOutputMode(nas_da) &= ~AuDeviceOutputModeSpeaker;
  		AuSetDeviceAttributes(NASaud,NASdevice,AuCompDeviceOutputModeMask,nas_da,&austat);
  		if (austat != AuSuccess) { 
			fprintf(stderr, "NAS_Speaker_Toggle:Could not set attributes for device 0x%x, NAS error %d\n",NASdevice,austat);
  		}
		break;

	case 1:
		AuDeviceOutputMode(nas_da) |= AuDeviceOutputModeSpeaker;
  		AuSetDeviceAttributes(NASaud,NASdevice,AuCompDeviceOutputModeMask,nas_da,&austat);
  		if (austat != AuSuccess) { 
			fprintf(stderr, "NAS_Speaker_Toggle:Could not set attributes for device 0x%x, NAS error %d\n",NASdevice,austat);
  		}
		break;
		
	case 2:
		AuDeviceOutputMode(nas_da) ^= AuDeviceOutputModeSpeaker;
  		AuSetDeviceAttributes(NASaud,NASdevice,AuCompDeviceOutputModeMask,nas_da,&austat);
  		if (austat != AuSuccess) { 
			fprintf(stderr, "NAS_Speaker_Toggle:Could not set attributes for device 0x%x, NAS error %d\n",NASdevice,austat);
  		}
		break;

	default:
      		fprintf(stderr, "NAS_Speaker_Toggle: Invalid flag value %d\n",flag);
		break;
  }
}

void NAS_Headphone_Toggle(flag)
ULONG flag;
{ 
  AuStatus austat;

  DEBUG_LEVEL2 fprintf(stdout,"NAS_Headphone_Toggle\n");
  if (!(AuDeviceChangableMask(nas_da) & AuCompDeviceLineModeMask)){
	(void)fprintf(stderr,"NAS_Headphone_Toggle: Device lacks ability to toggle speaker\n");
  	/* AuFreeDeviceAttributes(NASaud,1,nas_da); POD don't want to free */
	return;
  }
  switch (flag) {
	case 0:
		AuDeviceOutputMode(nas_da) &= ~AuDeviceOutputModeHeadphone;
  		AuSetDeviceAttributes(NASaud,NASdevice,AuCompDeviceOutputModeMask,nas_da,&austat);
  		if (austat != AuSuccess) { 
			fprintf(stderr, "NAS_Headphone_Toggle:Could not set attributes for device 0x%x, NAS error %d\n",NASdevice,austat);
  		}
		break;

	case 1:
		AuDeviceOutputMode(nas_da) |= AuDeviceOutputModeHeadphone;
  		AuSetDeviceAttributes(NASaud,NASdevice,AuCompDeviceOutputModeMask,nas_da,&austat);
  		if (austat != AuSuccess) { 
			fprintf(stderr, "NAS_Headphone_Toggle:Could not set attributes for device 0x%x, NAS error %d\n",NASdevice,austat);
  		}
		break;
		
	case 2:
		AuDeviceOutputMode(nas_da) ^= AuDeviceOutputModeHeadphone;
  		AuSetDeviceAttributes(NASaud,NASdevice,AuCompDeviceOutputModeMask,nas_da,&austat);
  		if (austat != AuSuccess) { 
			fprintf(stderr, "NAS_Headphone_Toggle:Could not set attributes for device 0x%x, NAS error %d\n",NASdevice,austat);
  		}

		break;

	default:
      		fprintf(stderr, "NAS_Headphone_Toggle: Invalid flag value %d\n",flag);
		break;
  }
}


/********** NAS_Adjust_Volume **********************
 * Routine for Adjusting Volume on NAS
 *
 *****/
void NAS_Adjust_Volume(volume)
ULONG volume;
{ 
  AuStatus austat;

 DEBUG_LEVEL2 fprintf(stdout,"NAS_Audio_Volume %d\n",volume);

 /* In a sane model, this would not be necessary.  After all, how many 
  * output models prevent a change of gain???  The answer is, of course,
  * "I don't know."  I'm a Network Audio System neophyte, and I have no
  * idea how rational or irrational other server ports are.  So, leave this
  * ugly code here until someone who knows better can rip it out, bless it,
  * or beautify it.
  */

  if (!(AuDeviceChangableMask(nas_da) & AuCompDeviceGainMask))
  {
    (void)fprintf(stderr,"NAS_Volume_Adjust: Device lacks ability to adjust volume\n");
    /* AuFreeDeviceAttributes(NASaud,1,nas_da); POD don't want to free */
    return;
  }
 AuDeviceGain(nas_da) = AuFixedPointFromSum(volume,0);
 AuSetDeviceAttributes(NASaud,NASdevice,AuCompDeviceGainMask,nas_da,&austat);
 if (austat != AuSuccess) 
 { 
   fprintf(stderr, "NAS_Adjust_Volume:Could not set attributes for device 0x%x, NAS error %d\n",NASdevice,austat);
 }
/* POD-NAS This was flushing output buffer and waiting for all event and errors
   to be processed. Don't want to do this.
 AuSync(NASaud,AuFalse);
*/
}

/********** NAS_Closest_Freq **********************
 *
 * Global Variable Affect:
 *   ULONG xa_audio_hard_buff		must set but not larger than
 *					XA_AUDIO_MAX_RING_BUF size
 */
ULONG NAS_Closest_Freq(ifreq)
LONG ifreq;
{
 /* NAS automatically chooses the closest supported rate of the audio
  * server when it sets sampling frequencies.  However, it does not
  * make this rate accessible.  Short of actually attempting to set
  * the rate, there is no way (that I know of now) to get this information.
  * However, since the choice happens automatically, it _might_ be harmless
  * to pretend that the "closest" frequency is the one passed to this 
  * routine.
  * NAS probably ought to make the closest supported frequency easily 
  * accessible --  HINT HINT
  */

 xa_audio_hard_buff=XA_HARD_BUFF_1K;
 return (ifreq); /* PLACE HOLDER!!!!! */
}

#endif
/****************************************************************************/
/******************* END OF NAS SPECIFIC ROUTINES ***************************/
/****************************************************************************/



/* IMPORTANT: MERGED MUST BE AFTER ALL MACHINE SPECIFIC CODE */
/****************************************************************************/
/******************* MERGED AUDIO OUTPUT ROUTINE ****************************/
/****************************************************************************/
#ifdef XA_AUD_OUT_MERGED

/********** Merged_Audio_Output **********************
 * Set Volume if needed and then write audio data to audio device.
 *
 * IS 
 *****/
static void Merged_Audio_Output()
{ LONG time_diff, cnt = xa_audio_ring_size - 1;

  xa_audio_out_sem = 1;
  xa_interval_id = 0;

DEBUG_LEVEL1 fprintf(stderr,"AUD entry: c %ld\n",xa_time_now);

  /* normal exit is when audio gets ahead */
  while(cnt--)
  {
    if (xa_audio_status != XA_AUDIO_STARTED) { xa_audio_out_sem = 0; return; }
    time_diff = xa_time_audio - xa_time_now; /* how far ahead is audio */
    if (time_diff > xa_out_time) /* potentially ahead */
    {
      xa_time_now = XA_Read_AV_Time();  /* get new time */
      time_diff = xa_time_audio - xa_time_now;
      if (time_diff > xa_out_time)  /* definitely ahead */
      {
        DEBUG_LEVEL1 fprintf(stderr,"AUD_OUT: ahead c %ld a %ld\n",
						xa_time_now,xa_time_audio);
        xa_interval_id = XtAppAddTimeOut(theContext,xa_interval_time,
		(XtTimerCallbackProc)Merged_Audio_Output,(XtPointer)(NULL));
        xa_audio_out_sem = 0;
        return;
      }
    }
DEBUG_LEVEL1 fprintf(stderr,"AUD_OUT: okay c %ld a %ld\n",xa_time_now,xa_time_audio);

    /******************
     * METHOD 2: trying to limit flow into audio write buffer by using 
     * current and audio times. Only grab cur time every so often to
     * minimize cpu load.
     * Also, if we're behind, we want to drop through as quickly as possible
     * in order to get to the write(). Otherwise we might pop.
     ****************/

    /* If audio buffer is full, write() can't write all of the data. */
    /* So, next routine checks the rest length in buffer.            */
#ifdef XA_SONY_AUDIO
    { int buf_len;
      ret = ioctl(devAudio, SBIOCBUFRESID,&buf_len) ;
      if( ret == -1 ) fprintf(stderr,"SONY_AUDIO: SIOCBUFRESID error.\n");
      if(buf_len > sony_audio_buf_len - xa_audio_ring->len)
      { xa_interval_id = XtAppAddTimeOut(theContext, xa_interval_time,
            (XtTimerCallbackProc)Merged_Audio_Output,(XtPointer)(NULL));
        xa_audio_out_sem = 0;
        return;
      }
    }
#endif
#ifdef XA_SGI_AUDIO
    { int buf_len;
      buf_len = ALgetfillable(port) ;
      if (buf_len < (xa_audio_ring->len >> 1))
      { xa_interval_id = XtAppAddTimeOut(theContext, xa_interval_time,
		(XtTimerCallbackProc)Merged_Audio_Output,(XtPointer)(NULL));
	xa_audio_out_sem = 0;
	return;
      }
    }
#endif

    /************
     * Valid Audio Sample 
     *****/
    if (xa_audio_ring->len)
    { 

#ifdef XA_SPARC_AUDIO
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
      { AuStatus austat;
        AuWriteElement(NASaud,NASFlowID,0,xa_audio_ring->len,
				xa_audio_ring->buf,AuFalse,&austat);
        if (austat!=AuSuccess)
        { if (austat==AuBadValue) fprintf(stderr,"NASout: ovrflow\n");
        }
      }
#endif
#ifdef XA_SONY_AUDIO
      write(devAudio,xa_audio_ring->buf,xa_audio_ring->len);
      /* Buffer of Sony audio device is too large */ /* HP needs this */
      ret = ioctl(devAudio, SBIOCFLUSH, 0);
      if( ret == -1 ) fprintf(stderr,"audio output:SBIOCFLUSH error.\n");
#endif
#ifdef XA_EWS_AUDIO
      write(devAudio,xa_audio_ring->buf,xa_audio_ring->len);
#endif
#ifdef XA_AF_AUDIO
      { ATime act, atd = AFtime0;
	if (xa_audio_mute != TRUE)
	  act = AFPlaySamples(ac,AFtime0,xa_audio_ring->len,xa_audio_ring->buf);
	else act = AFGetTime(ac);
	if (AFtime0 < act)	AFtime0 = act+TOFFSET;
	else	AFtime0 += xa_audio_ring->len >> 1; /* Number of samples */
      }
#endif
#ifdef XA_HPDEV_AUDIO
      write (devAudio, xa_audio_ring->buf, xa_audio_ring->len);
#endif

      if (xa_audio_synctst) XSync(theDisp, False); /* POD TEST EXPERIMENTAL */

      /* Don't adjust volume until after write. If we're behind
       * the extra delay could cause a pop. */
      if (xa_audio_newvol==TRUE)
      { ULONG vol = (xa_audio_mute==TRUE)?(XA_AUDIO_MINVOL):(xa_audio_volume);
  	XA_Adjust_Volume(vol);
  	xa_audio_newvol = FALSE;
      }
      xa_time_audio   += xa_audio_ring->time;
      xa_timelo_audio += xa_audio_ring->timelo;
      if (xa_timelo_audio & 0xff000000)
		{ xa_time_audio++; xa_timelo_audio &= 0x00ffffff; }
      xa_audio_ring->len = 0; /* above write must be blocking */
      xa_audio_ring = xa_audio_ring->next;  /* move to next */
      if (xa_audio_status != XA_AUDIO_STARTED) { xa_audio_out_sem = 0; return;}
      if (xa_out_init) xa_out_init--;
      else if (xa_update_ring_sem==0)	/*semaphores aren't really used since */
      {					/*XtAppTimeOut is sequential */
        xa_update_ring_sem = 1;
        XA_Update_Ring(2);  /* only allow two to be updated */
        xa_update_ring_sem = 0;
      }
    } 
    else  /* Audio Sample finished */
    { xa_time_now = XA_Read_AV_Time();

DEBUG_LEVEL1 fprintf(stderr,"AUD_WAIT: c %ld a %ld\n",xa_time_now,xa_time_audio);
	/* Is audio still playing buffered samples? */
      if (xa_time_now < xa_time_audio) 
      { ULONG diff = xa_time_audio - xa_time_now;
        if (xa_audio_status != XA_AUDIO_STARTED) {xa_audio_out_sem = 0; return;}
        xa_interval_id = XtAppAddTimeOut(theContext, diff,
		(XtTimerCallbackProc)Merged_Audio_Output,(XtPointer)(NULL));
        xa_audio_out_sem = 0;	return;
      }
      else 
      { XA_Audio_Off(0);
	xa_audio_out_sem = 0;	return;
      }
    }
  } /* end of while audio is behind */

  if (xa_audio_status == XA_AUDIO_STARTED)
  {
    xa_time_now = XA_Read_AV_Time();
    if (xa_audio_status != XA_AUDIO_STARTED) { xa_audio_out_sem = 0; return;}
    xa_interval_id = XtAppAddTimeOut(theContext, xa_interval_time,
                (XtTimerCallbackProc)Merged_Audio_Output,(XtPointer)(NULL));
  }
  xa_audio_out_sem = 0;
}
#endif
/****************************************************************************/
/******************* END OF MERGED AUDIO OUTPUT ROUTINE *********************/
/****************************************************************************/




/********* XA_Add_Sound ****************************************
 *
 *
 * Global Variables Used:
 *   double xa_audio_scale	linear scale of frequency
 *   ULONG  xa_audio_hard_buff	size of sound chunk - set by XA_Closest_Freq().
 *   ULONG  xa_audio_buffer	TRUE if this routine is to convert and buffer
 * 				the audio data ahead of time.
 *   ULONG  xa_audio_hard_type	Audio Type of Current Hardware
 *
 * Add sound double checks xa_audio_present. 
 *   If UNK then it calls XA_Audio_Init().
 *   If OK adds the sound, else returns false.
 *****/
ULONG XA_Add_Sound(anim_hdr,isnd,itype,fpos,ifreq,ilen,stime,stimelo)
XA_ANIM_HDR *anim_hdr;
UBYTE *isnd;
ULONG itype;	/* sound type */
ULONG fpos;	/* file position */
ULONG ifreq;	/* input frequency */
ULONG ilen;	/* length of snd sample chunk in bytes */
LONG *stime;	/* start time of sample */
ULONG *stimelo;	/* fractional start time of sample */
{
  XA_SND *new_snd;
  ULONG bps,isamps,totsamps,hfreq,inc;
  double finc,ftime,fadj_freq;

  /* If audio hasn't been initialized, then init it */
  if (xa_audio_present == XA_AUDIO_UNK) XA_Audio_Init();
  /* Return FALSE if it's not OK */
  if (xa_audio_present != XA_AUDIO_OK) return(FALSE);

/* POD NOTE: AUDIO BUFFER FLAG WILL GO THIS ROUTE */
#ifdef THINKTHINK
  if ( (itype == XA_AUDIO_ADPCM_M) && (fpos == -1))
  { UBYTE *tsnd;
    ULONG tmp_len;
    LONG SampPerBlock,w,BytesPerBlock;

    itype = XA_AUDIO_SIGNED_2MB;  /* 16 bit signed mono big endian for now */
          /* 4 bits to 16 bits (ilen was in bytes)*/ 
     /* each byte is 2 samples with becomes 2 bytes */

    /* compute number of ms_adpcm out samples and SampPerBlock */
    BytesPerBlock = (256 * 1); /* 256 * chans */
    if (ifreq > 11025) BytesPerBlock *= (ifreq / 11000);
    w = 8 * (BytesPerBlock - (7 * 1));  /* - (7 * chans) */
    SampPerBlock = (w / (4 * 1)) + 2;

    w = (ilen / BytesPerBlock);
    tmp_len = SampPerBlock * w;  /* out len based on whole samples */

    w = ilen - (w * BytesPerBlock); /* remaining ilen */
    w -= (7 * 1);  /* subtract header */
    if (w > 0)  /* if anything left */
    {
      w = (8 * w) / ( 4 * 1); /* extra samples */
      w += 2; /* plus samples in header */
      tmp_len += w;
    }
    tsnd = (UBYTE *)malloc(tmp_len << 1);   
    if (tsnd==0) TheEnd1("add_sound: adpcm buff err");

    tmp_len = ms_adpcm_decode(isnd,tsnd,ilen,tmp_len,SampPerBlock);
    FREE(isnd,0x501); 
    isnd = tsnd;
    ilen = tmp_len << 1; /* 2 bytes per sample */
  }
#endif

  new_snd = (XA_SND *)malloc(sizeof(XA_SND));
  if (new_snd==0) TheEnd1("snd malloc err");

  bps = 1;
  if (itype == XA_AUDIO_ADPCM_M)
  { LONG SampPerBlock,w,BytesPerBlock,tmp_len;
    /* compute number of ms_adpcm out samples and SampPerBlock */
    BytesPerBlock = (256 * 1); /* 256 * chans */
    if (ifreq > 11025) BytesPerBlock *= (ifreq / 11000);
    w = 8 * (BytesPerBlock - (7 * 1));  /* - (7 * chans) */
    SampPerBlock = (w / (4 * 1)) + 2;

/*
DEBUG_LEVEL1 fprintf(stderr,"ADPCM ASND: snd %lx bytes %lx ifreq %ld samps %lx ilen %lx \n",new_snd,BytesPerBlock,ifreq,SampPerBlock,ilen);
*/

    /* for MSADPCM tot_samps is total sample in a block, not for chunk */
    isamps = SampPerBlock;
    /* need to calculate total samples in entire chunk for timing purposes */
    w = (ilen / BytesPerBlock);  
    tmp_len = SampPerBlock * w;  /* out len based on full blocks */

    w = ilen - (w * BytesPerBlock); /* bytes remaining */
    w -= (7 * 1);  /* subtract header */
    if (w >= 0)  /* if anything left */
    {
      w = (8 * w) / ( 4 * 1); /* extra samples */
      w += 2; /* plus samples in header */
      tmp_len += w;
    }
    totsamps = tmp_len;
  }
  else
  {
    if (itype & XA_AUDIO_STEREO_MSK) bps *= 2;
    if (itype & XA_AUDIO_BPS_2_MSK) bps *= 2;
    totsamps = isamps = ilen / bps;
  }
  new_snd->tot_samps = new_snd->samp_cnt = isamps;
  new_snd->tot_bytes = ilen;
  new_snd->byte_cnt = 0;

  new_snd->fpos = fpos;
  new_snd->type = itype;
  new_snd->flag = 0;
  new_snd->ifreq = ifreq;
  hfreq = (xa_audio_playrate)?(xa_audio_playrate):(ifreq);
  new_snd->hfreq = hfreq = XA_Closest_Freq(hfreq);
  if (hfreq==0)  /* could'nt find a supported freq */
  {
    fprintf(stderr,"Couldn't find a supported frequency. Audio Off.\n");
    FREE(new_snd,0x502); new_snd = 0; return(FALSE);
  }
  new_snd->ch_size = xa_audio_hard_buff; /* set by Closest_Freq */

  /* Setup and return Chunk Start Time */
  new_snd->snd_time = *stime;
  { ULONG tint;
    ftime = ((double)(totsamps) * 1000.0) / (double)(ifreq);
    tint = (ULONG)(ftime);   /* get integer time */
    *stime += tint;
    ftime -= (double)(tint); /* get fraction time */
    *stimelo += (ULONG)( ftime * (double)(1<<24) );
    while( (*stimelo) > (1<<24)) { *stime += 1; *stimelo -= (1<<24); }
  }

  /* Determine f2f inc */
  fadj_freq = (double)(ifreq) * xa_audio_scale;
  finc = (double)(fadj_freq)/ (double)(hfreq);
  new_snd->inc = inc = (ULONG)( finc * (double)(1<<24) );
  new_snd->inc_cnt = 0;

  /* Determine Chunk Time */
  ftime = ((double)(xa_audio_hard_buff) * 1000.0) / (double)(hfreq);
  new_snd->ch_time = (LONG)ftime;
  ftime -= (double)(new_snd->ch_time);
  new_snd->ch_timelo = (ULONG)(ftime * (double)(1<<24));

  new_snd->snd	= isnd;
  new_snd->prev = 0;
  new_snd->next = 0;

  /* Figure out which conversion routine to use */
  switch(xa_audio_hard_type)
  {
    case XA_AUDIO_SUN_AU:
      {
	switch(itype)
	{
	  case XA_AUDIO_SIGNED_1M:
	  case XA_AUDIO_SIGNED_2MB:
	  case XA_AUDIO_SIGNED_2ML:
	  case XA_AUDIO_LINEAR_1M:
	  case XA_AUDIO_LINEAR_2ML:
	  case XA_AUDIO_LINEAR_2MB:
	  case XA_AUDIO_SIGNED_1S:
	  case XA_AUDIO_SIGNED_2SB:
	  case XA_AUDIO_SIGNED_2SL:
	  case XA_AUDIO_LINEAR_1S:
	  case XA_AUDIO_LINEAR_2SL:
	  case XA_AUDIO_LINEAR_2SB:
		new_snd->spec = 
		    ((itype & XA_AUDIO_TYPE_MASK)==XA_AUDIO_LINEAR)?(0):(1);
		if (itype & XA_AUDIO_BPS_2_MSK)
		  new_snd->spec |= (itype & XA_AUDIO_BIGEND_MSK)?(2):(4);
		new_snd->spec |= 8; /* SUN AU bit */
		if (itype & XA_AUDIO_STEREO_MSK)
			new_snd->delta = XA_Audio_PCMXS_PCM1M;
		else
			new_snd->delta = XA_Audio_PCMXM_PCM1M;
		break;
	  case XA_AUDIO_SUN_AU:
		new_snd->delta = XA_Audio_1M_1M;
		break;
	  case XA_AUDIO_ADPCM_M:
	        new_snd->spec = 2 | 4 | 8;
		new_snd->delta = XA_Audio_ADPCMM_PCM2M;
		break;
	  case XA_AUDIO_NOP:
		new_snd->delta = XA_Audio_Silence;
		new_snd->spec = 0x00;
		break;
	  default: 
		fprintf(stderr,"SUN_AU_AUDIO: Unsupported Software Type\n");
		FREE(new_snd,0x503); new_snd = 0;
		return(FALSE);
		break;
	}
      }
      break;

    case XA_AUDIO_SIGNED_2ML:
    case XA_AUDIO_SIGNED_2MB:
      {
	switch(itype)
	{
	  case XA_AUDIO_LINEAR_1M:      /* LIN1M -> SIN2M*  */
		if (xa_audio_hard_type & XA_AUDIO_BIGEND_MSK)
			new_snd->spec = 1;
		else    new_snd->spec = 2;
		new_snd->delta = XA_Audio_PCM1M_PCM2M;
		break;
	  case XA_AUDIO_LINEAR_1S:      /* LIN1S -> SIN2M*  */
		if (xa_audio_hard_type & XA_AUDIO_BIGEND_MSK)
			new_snd->spec = 1;
		else    new_snd->spec = 2;
		new_snd->delta = XA_Audio_PCM1S_PCM2M;
		break;
	  case XA_AUDIO_SIGNED_1M:      /* SIN1M -> SIN2M*  */
		if (xa_audio_hard_type & XA_AUDIO_BIGEND_MSK)
			new_snd->spec = 2;
		else    new_snd->spec = 1;
		new_snd->delta = XA_Audio_PCM1M_PCM2M;
		break;
	  case XA_AUDIO_LINEAR_2MB:     /* LIN2M* -> SIN2M*  */
	  case XA_AUDIO_LINEAR_2ML:
	  case XA_AUDIO_SIGNED_2MB:     /* SIN2M* -> SIN2M*  */
	  case XA_AUDIO_SIGNED_2ML:
	  case XA_AUDIO_LINEAR_2SB:     /* LIN2S* -> SIN2M*  */
	  case XA_AUDIO_LINEAR_2SL:
	  case XA_AUDIO_SIGNED_2SB:     /* SIN2S* -> SIN2M*  */
	  case XA_AUDIO_SIGNED_2SL:
		  /* sign conversion? */
		if ( (itype & XA_AUDIO_TYPE_MASK) == XA_AUDIO_SIGNED)
			new_snd->spec = 0x10;
		else	new_snd->spec = 0x02;
		  /* src endian? */
		new_snd->spec |= (itype & XA_AUDIO_BIGEND_MSK)?(0):(1);
		  /* dst endian? */
		new_snd->spec |= 
			(xa_audio_hard_type & XA_AUDIO_BIGEND_MSK)?(0):(4);
		  /* stereo? */
		new_snd->spec |= (itype & XA_AUDIO_STEREO_MSK)?(8):(0); 
		new_snd->delta = XA_Audio_PCM2X_PCM2M;
		break;
	  case XA_AUDIO_ADPCM_M:
		if (xa_audio_hard_type & XA_AUDIO_BIGEND_MSK)
			new_snd->spec = 0;
		else	new_snd->spec = 1;
		new_snd->delta = XA_Audio_ADPCMM_PCM2M;
		break;
	  case XA_AUDIO_NOP:
		new_snd->delta = XA_Audio_Silence;
		new_snd->spec = 0x00;
		break;
	  default:
		FREE(new_snd,0x504); new_snd = 0;
		fprintf(stderr,"AUDIO_SIN2M: Unsupported Software Type\n");
		return(FALSE);
		break;

	}
      }
      break;

    case XA_AUDIO_LINEAR_1M:
      {
	switch(itype)
	{
	  case XA_AUDIO_SIGNED_1M:
	  case XA_AUDIO_SIGNED_2MB:
	  case XA_AUDIO_SIGNED_2ML:
	  case XA_AUDIO_LINEAR_1M:
	  case XA_AUDIO_LINEAR_2ML:
	  case XA_AUDIO_LINEAR_2MB:
	  case XA_AUDIO_SIGNED_1S:
	  case XA_AUDIO_SIGNED_2SB:
	  case XA_AUDIO_SIGNED_2SL:
	  case XA_AUDIO_LINEAR_1S:
	  case XA_AUDIO_LINEAR_2SL:
	  case XA_AUDIO_LINEAR_2SB:
		new_snd->spec = 
		    ((itype & XA_AUDIO_TYPE_MASK)==XA_AUDIO_LINEAR)?(0):(1);
		if (itype & XA_AUDIO_BPS_2_MSK)
		  new_snd->spec |= (itype & XA_AUDIO_BIGEND_MSK)?(2):(4);
		if (itype & XA_AUDIO_STEREO_MSK)
			new_snd->delta = XA_Audio_PCMXS_PCM1M;
		else
			new_snd->delta = XA_Audio_PCMXM_PCM1M;
		break;
	  case XA_AUDIO_ADPCM_M:
		new_snd->spec = 2 | 4;  /* 1 byte output */
		new_snd->delta = XA_Audio_ADPCMM_PCM2M;
		break;
	  case XA_AUDIO_NOP:
		new_snd->delta = XA_Audio_Silence;
		new_snd->spec = 0x00;
		break;
	  default:
		FREE(new_snd,0x505); new_snd = 0;
		fprintf(stderr,"AUDIO_LIN1M: Unsupported Software Type\n");
		return(FALSE);
		break;
	}
      }
      break;

    case XA_AUDIO_LINEAR_2ML:
    case XA_AUDIO_LINEAR_2MB:
      {
	switch(itype)
	{
	  case XA_AUDIO_LINEAR_1M:      /* LIN1M -> LIN2M*  */
		new_snd->spec = 0;
		new_snd->delta = XA_Audio_PCM1M_PCM2M;
		break;
	  case XA_AUDIO_SIGNED_1M:      /* SIN1M -> LIN2M*  */
		if (xa_audio_hard_type & XA_AUDIO_BIGEND_MSK)
			new_snd->spec = 1;
		else    new_snd->spec = 2;
		new_snd->delta = XA_Audio_PCM1M_PCM2M;
		break;
	  case XA_AUDIO_LINEAR_2MB:     /* LIN2M* -> LIN2M*  */
	  case XA_AUDIO_LINEAR_2ML:
	  case XA_AUDIO_SIGNED_2MB:     /* SIN2M* -> LIN2M*  */
	  case XA_AUDIO_SIGNED_2ML:
	  case XA_AUDIO_LINEAR_2SB:     /* LIN2S* -> LIN2M*  */
	  case XA_AUDIO_LINEAR_2SL:
	  case XA_AUDIO_SIGNED_2SB:     /* SIN2S* -> LIN2M*  */
	  case XA_AUDIO_SIGNED_2SL:
		  /* sign conversion? */
		if ( (itype & XA_AUDIO_TYPE_MASK) == XA_AUDIO_SIGNED)
			new_snd->spec = 0x12;
		else	new_snd->spec = 0;
		  /* src endian? */
		new_snd->spec |= (itype & XA_AUDIO_BIGEND_MSK)?(0):(1);
		  /* dst endian? */
		new_snd->spec |= 
			(xa_audio_hard_type & XA_AUDIO_BIGEND_MSK)?(0):(4);
		  /* stereo? */
		new_snd->spec |= (itype & XA_AUDIO_STEREO_MSK)?(8):(0); 
		new_snd->delta = XA_Audio_PCM2X_PCM2M;
		break;
	  case XA_AUDIO_ADPCM_M:
		if (xa_audio_hard_type & XA_AUDIO_BIGEND_MSK)
			new_snd->spec = 2;
		else	new_snd->spec = 3;
		new_snd->delta = XA_Audio_ADPCMM_PCM2M;
		break;
	  case XA_AUDIO_NOP:
		new_snd->delta = XA_Audio_Silence;
		new_snd->spec = 0x00;
		break;
	  default:
		FREE(new_snd,0x506); new_snd = 0;
		fprintf(stderr,"AUDIO_LIN2M: Unsupported Software Type\n");
		return(FALSE);
		break;
	}
      }
      break;


    default: 
      FREE(new_snd,0x507); new_snd = 0;
      fprintf(stderr,"AUDIO: Unknown Hardware Type\n");
      return(FALSE);
      break;
  }

  /** Set up prev pointer */
  if (anim_hdr->first_snd==0) new_snd->prev = 0;
  else new_snd->prev = anim_hdr->last_snd;

  /** Set up next pointer */
  if (anim_hdr->first_snd == 0)	anim_hdr->first_snd = new_snd;
  if (anim_hdr->last_snd)	anim_hdr->last_snd->next = new_snd;
  anim_hdr->last_snd = new_snd;
  return(TRUE);
}

XA_SND *XA_Audio_Next_Snd(snd_hdr)
XA_SND *snd_hdr;
{ 
DEBUG_LEVEL2 fprintf(stderr,"XA_Audio_Next_Snd: snd_hdr %lx \n",snd_hdr);
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
LONG fpos;
ULONG fsize;
char *buf;
{ int ret = lseek(fd,fpos,SEEK_SET);
  if (ret != fpos) TheEnd1("XA_Read_Audio_Delta:seek err");
  ret = read(fd, buf, fsize);
  if (ret != fsize) TheEnd1("XA_Read_Audio_Delta:read err");
}



/*************
 *  May not be needed
 *
 *****/
UBYTE XA_Signed_To_Ulaw(ch)
LONG ch;
{
  LONG mask;
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

void Gen_Signed_2_Ulaw()
{
  ULONG i;
  for(i=0;i<256;i++)
  { UBYTE d;
    BYTE ch = i;
    LONG chr = ch;
    d = XA_Signed_To_Ulaw(chr * 16);
    sign_2_ulaw[i] = d;
  }
}



/********* Init_Audio_Ring ************
 *
 * Global Variables:
 *  xa_audio_ring		pointer to somewhere in the ring
 *************************************/
void Init_Audio_Ring(ring_num,buf_size)
ULONG ring_num;
ULONG buf_size;
{
  ULONG i;
  XA_AUDIO_RING_HDR *t_ring,*t_first;
  UBYTE *t_buf;
  if (xa_audio_ring) Kill_Audio_Ring();
  t_first = xa_audio_ring = 0;
  for(i=0;i<ring_num;i++)
  {
    t_ring = (XA_AUDIO_RING_HDR *)malloc( sizeof(XA_AUDIO_RING_HDR) );
    if (t_ring==0) TheEnd1("Init Audio Ring: malloc err0");
    t_buf = (UBYTE *)malloc( buf_size );
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
ULONG cnt;
{ 
DEBUG_LEVEL1 fprintf(stderr,"UPDATE RING\n");
  while( (xa_audio_ring_t->len == 0) && (xa_snd_cur) && cnt)
  { ULONG tmp_time, tmp_timelo; LONG i,xx;
    cnt--;
    tmp_time = xa_snd_cur->ch_time;	/* save these */
    tmp_timelo = xa_snd_cur->ch_timelo;
	/* uncompress sound chunk */
    i = xa_audio_ring_t->len = xa_snd_cur->ch_size * xa_audio_hard_bps;
    /* NOTE: the following delta call may modify xa_snd_cur */
    xx = xa_snd_cur->delta(xa_snd_cur,xa_audio_ring_t->buf,0,
							xa_snd_cur->ch_size);

DEBUG_LEVEL1 if (xa_snd_cur) fprintf(stderr,"UPDATE RING ret: snd %lx i %ld xx %ld bps %ld chszi %ld\n",xa_snd_cur,i,xx,xa_audio_hard_bps,xa_snd_cur->ch_size);

    i -= xx * xa_audio_hard_bps;
    if (i > 0) /* Some system need fixed size chunks */
    { UBYTE *dptr = xa_audio_ring_t->buf;
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
{ XA_AUDIO_RING_HDR *tring = xa_audio_ring_t;

DEBUG_LEVEL1 fprintf(stderr,"FLUSH_RING\n");
  do
  {
    if (xa_audio_ring->len)
    {
      xa_audio_ring->len = 0;
      xa_audio_ring = xa_audio_ring->next;
    } else break;
  } while(xa_audio_ring != tring);
  xa_audio_flushed = 1;
}

/*************************************************************************/
/********** Audio Codec Conversion Routines ******************************/
/*************************************************************************/

/********** XA_Audio_1M_1M ************************************
 * NOP extract the next chunk and puts into audio ring for 1 byte samples.
 *
 * Global Variables:
 *   UBYTE sign_2_ulaw[256]	conversion table.
 *   XA_Audio_Next_Snd()	routine to move to next sound header.
 ***************************************************************/
ULONG XA_Audio_1M_1M(snd_hdr,obuf,ocnt,buff_size)
XA_SND *snd_hdr;
UBYTE *obuf;
ULONG ocnt,buff_size;
{ ULONG byte_cnt,samp_cnt;
  UBYTE *ibuf;

  if (snd_hdr==0) return(ocnt);
  byte_cnt = snd_hdr->byte_cnt;
  samp_cnt = snd_hdr->samp_cnt;
  ibuf = snd_hdr->snd;		ibuf += byte_cnt;

  while(ocnt < buff_size)
  { *obuf++ = *ibuf++;
    samp_cnt--;		ocnt++;	byte_cnt++;

    if (samp_cnt <= 0)
    {
	if ( (snd_hdr = XA_Audio_Next_Snd(snd_hdr)) != 0)
	  ocnt = snd_hdr->delta(snd_hdr,obuf,ocnt,buff_size);
	return(ocnt);
    }
  }
  snd_hdr->byte_cnt = byte_cnt;
  return(ocnt);
}


/********** XA_Audio_PCM1M_PCM2M *********************************
 * Convert PCM 1 BPS Mono Samples into PCM 2 BPS Mono Samples
 * The order flag takes care of the various linear/signed/endian
 * conversions
 *  Input        Ouput       Order  1st  2nd
 *  -----------------------------------------
 *  Linear to Linear Big       0    D     D
 *  Linear to Linear Little    0    D     D
 *  Signed to Linear Big       1    D^80  D
 *  Signed to Linear Little    2    D     D^80
 *  Linear to Signed Big       1    D^80  D
 *  Linear to Signed Little    2    D     D^80
 *  Signed to Signed Big       2    D     D^80
 *  Signed to Signed Little    1    D^80  D
 *
 * Global Variables:
 *   UBYTE sign_2_ulaw[256]	conversion table.
 *   XA_Audio_Next_Snd()	routine to move to next sound header.
 ***************************************************************/
ULONG XA_Audio_PCM1M_PCM2M(snd_hdr,obuf,ocnt,buff_size)
XA_SND *snd_hdr;
UBYTE *obuf;
ULONG ocnt,buff_size;
{ ULONG byte_cnt,inc,inc_cnt,spec,samp_cnt;
  UBYTE *ibuf;

  if (snd_hdr==0) return(ocnt);
  spec = snd_hdr->spec;
  inc = snd_hdr->inc;		inc_cnt = snd_hdr->inc_cnt;
  byte_cnt = snd_hdr->byte_cnt;
  samp_cnt = snd_hdr->samp_cnt;
  ibuf = snd_hdr->snd;		ibuf += byte_cnt;

  while(ocnt < buff_size)
  { register ULONG data = *ibuf;
    if (spec==1) { *obuf++ = data ^ 0x80; *obuf++ = data; }
    else if (spec==2) { *obuf++ = data; *obuf++ = data ^ 0x80; }
    else { *obuf++ = data; *obuf++ = data; }
    ocnt++;			inc_cnt += inc;
    while(inc_cnt >= (1<<24) )
    { inc_cnt -= (1<<24);
      samp_cnt--;	ibuf++;		byte_cnt++;
      if (samp_cnt <= 0)
      { 
        if ( (snd_hdr = XA_Audio_Next_Snd(snd_hdr)) != 0)
	  ocnt = snd_hdr->delta(snd_hdr,obuf,ocnt,buff_size);
	return(ocnt);
      }
    }
  }
  snd_hdr->inc_cnt = inc_cnt;
  snd_hdr->byte_cnt = byte_cnt;
  snd_hdr->samp_cnt = samp_cnt;
  return(ocnt);
}

/********** XA_Audio_PCM1S_PCM2M *********************************
 * Convert PCM 1 BPS Stereo Samples into PCM 2 BPS Mono Samples
 * The order flag takes care of the various linear/signed/endian
 * conversions
 *  Input        Ouput       Order  1st  2nd
 *  -----------------------------------------
 *  Linear to Linear Big       0    D     D
 *  Linear to Linear Little    0    D     D
 *  Signed to Linear Big       1    D^80  D
 *  Signed to Linear Little    2    D     D^80
 *  Linear to Signed Big       1    D^80  D
 *  Linear to Signed Little    2    D     D^80
 *  Signed to Signed Big       2    D     D^80
 *  Signed to Signed Little    1    D^80  D
 *
 * Global Variables:
 *   UBYTE sign_2_ulaw[256]	conversion table.
 *   XA_Audio_Next_Snd()	routine to move to next sound header.
 ***************************************************************/
ULONG XA_Audio_PCM1S_PCM2M(snd_hdr,obuf,ocnt,buff_size)
XA_SND *snd_hdr;
UBYTE *obuf;
ULONG ocnt,buff_size;
{ ULONG byte_cnt,inc,inc_cnt,spec,samp_cnt;
  UBYTE *ibuf;

  if (snd_hdr==0) return(ocnt);
  spec = snd_hdr->spec;
  inc = snd_hdr->inc;		inc_cnt = snd_hdr->inc_cnt;
  byte_cnt = snd_hdr->byte_cnt;
  samp_cnt = snd_hdr->samp_cnt;
  ibuf = snd_hdr->snd;		ibuf += byte_cnt;

  while(ocnt < buff_size)
  { register ULONG data = *ibuf;
    data = (data + (ULONG)(ibuf[1])) >> 1;
    if (spec==1) { *obuf++ = data ^ 0x80; *obuf++ = data; }
    else if (spec==2) { *obuf++ = data; *obuf++ = data ^ 0x80; }
    else { *obuf++ = data; *obuf++ = data; }
    ocnt++;			inc_cnt += inc;
    while(inc_cnt >= (1<<24) )
    { inc_cnt -= (1<<24);
      samp_cnt--;	ibuf+=2;	byte_cnt += 2;
      if (samp_cnt <= 0)
      { 
        if ( (snd_hdr = XA_Audio_Next_Snd(snd_hdr)) != 0)
	  ocnt = snd_hdr->delta(snd_hdr,obuf,ocnt,buff_size);
	return(ocnt);
      }
    }
  }
  snd_hdr->inc_cnt = inc_cnt;
  snd_hdr->byte_cnt = byte_cnt;
  snd_hdr->samp_cnt = samp_cnt;
  return(ocnt);
}

/********** XA_Audio_PCM2X_PCM2M *********************************
 * Convert PCM 2 BPS (mono/stereo little/big endian) Samples 
 * into PCM 2 BPS Mono (little/big endian) Samples.
 * The flag "spec' takes care of the various linear/signed/endian/stereo
 * conversions.
 * 
 * bit   mask  meaning
 * -----------------------------------------
 * bit 0:  1  src endian  0 = big  1 = little
 * bit 1:  2  linear/signed conversion.  0 = none  1 = ^0x8000
 * bit 2:  4  dst endian  0 = big  1 = little
 * bit 3:  8  src stereo  0 = no   1 = yes
 * bit 4: 10  src signed  0 = no   1 = yes
 *
 * Global Variables:
 *   XA_Audio_Next_Snd()	routine to move to next sound header.
 ***************************************************************/
ULONG XA_Audio_PCM2X_PCM2M(snd_hdr,obuf,ocnt,buff_size)
XA_SND *snd_hdr;
UBYTE *obuf;
ULONG ocnt,buff_size;
{ ULONG byte_cnt,inc,inc_cnt,spec,samp_cnt,binc;
  UBYTE *ibuf;

  if (snd_hdr==0) return(ocnt);
  spec = snd_hdr->spec;
  binc = (spec & 0x08)?(4):(2);
  inc = snd_hdr->inc;		inc_cnt = snd_hdr->inc_cnt;
  byte_cnt = snd_hdr->byte_cnt;
  samp_cnt = snd_hdr->samp_cnt;
  ibuf = snd_hdr->snd;		ibuf += byte_cnt;
  

  while(ocnt < buff_size)
  { register ULONG d0,d1;

    if (spec & 1)   /* Read Samples */
    { d0 = (ULONG)(ibuf[0]) | ((ULONG)(ibuf[1]) << 8);
      if (spec & 8) 
      {
        d1 = (ULONG)(ibuf[2]) | ((ULONG)(ibuf[3]) << 8); 
        if (spec & 0x10)
        { LONG da,db;
	  da = (d0 & 0x8000)?(d0 - 0x10000):(d0);
	  db = (d1 & 0x8000)?(d1 - 0x10000):(d1);
	  d0 = ((da + db) >> 1) & 0xffff;
        } else { d0 += d1; d0 >>=1; }
      }
    }
    else
    { d0 = (ULONG)(ibuf[1]) | ((ULONG)(ibuf[0]) << 8);  
      if (spec & 8) 
      {
        d1 = (ULONG)(ibuf[3]) | ((ULONG)(ibuf[2]) << 8);
        if (spec & 0x10)
        { LONG da,db;
	  da = (d0 & 0x8000)?(d0 - 0x10000):(d0);
	  db = (d1 & 0x8000)?(d1 - 0x10000):(d1);
	  d0 = ((da + db) >> 1) & 0xffff;
        } else { d0 += d1; d0 >>=1; }
      }
    }
    if (spec & 2) d0 ^= 0x8000; /* sign conversion */
    if (spec & 4)	{ d1 = d0 >> 8;   d0 &= 0xff; }
    else		{ d1 = d0 & 0xff; d0 >>= 8; }
    *obuf++ = d0;	
    *obuf++ = d1;
    ocnt ++;			inc_cnt += inc;
    while(inc_cnt >= (1<<24) )
    { inc_cnt -= (1<<24);
      samp_cnt--;	ibuf += binc;	byte_cnt += binc;
      if (samp_cnt <= 0)
      { 
        if ( (snd_hdr = XA_Audio_Next_Snd(snd_hdr)) != 0)
	  ocnt = snd_hdr->delta(snd_hdr,obuf,ocnt,buff_size);
	return(ocnt);
      }
    }
  }
  snd_hdr->inc_cnt = inc_cnt;
  snd_hdr->byte_cnt = byte_cnt;
  snd_hdr->samp_cnt = samp_cnt;
  return(ocnt);
}

/********** XA_Audio_PCMXM_PCM1M *********************************
 * Convert PCM 1+2 BPS Mono Samples into PCM 1 BPS Mono Samples
 * The order flag takes care of the various linear/signed/endian
 * conversions
 *  Input        order 1st  2nd
 *  -----------------------------------------
 *  Linear1M      0   D     -
 *  Signed1M      1   D^80  -
 *  Linear2MBig   2   D     skip
 *  Signed2MBig   3   D^80  skip
 *  Linear2MLit   4   skip  D
 *  Signed2MLit   5   skip  D^80
 *
 *  bit 3 (& 0x08) AU output instead of PCM.
 *
 * Global Variables:
 *   XA_Audio_Next_Snd()	routine to move to next sound header.
 ***************************************************************/
ULONG XA_Audio_PCMXM_PCM1M(snd_hdr,obuf,ocnt,buff_size)
XA_SND *snd_hdr;
UBYTE *obuf;
ULONG ocnt,buff_size;
{ ULONG byte_cnt,inc,inc_cnt,spec,bps,samp_cnt;
  UBYTE *ibuf;

  if (snd_hdr==0) return(ocnt);
  spec = snd_hdr->spec;
  bps = ((spec & 2) | (spec & 4))?(2):(1);
  inc = snd_hdr->inc;		inc_cnt = snd_hdr->inc_cnt;
  byte_cnt = snd_hdr->byte_cnt;
  samp_cnt = snd_hdr->samp_cnt;
  ibuf = snd_hdr->snd;		ibuf += byte_cnt;

  while(ocnt < buff_size)
  { register ULONG data;
    data = (spec & 4)?(ibuf[1]):(*ibuf);
    if (spec & 8)
    { /* note: ulaw takes signed input */
       if (spec & 1) *obuf++ = sign_2_ulaw[ data ];
       else *obuf++ = sign_2_ulaw[ (data ^ 0x80) ];
    }
    else *obuf++ = (spec & 1)?(data^0x80):(data);
    ocnt++;			inc_cnt += inc;
    while(inc_cnt >= (1<<24) )
    { inc_cnt -= (1<<24);	
      samp_cnt--;	ibuf += bps;		byte_cnt += bps;
      if (samp_cnt <= 0)
      { 
        if ( (snd_hdr = XA_Audio_Next_Snd(snd_hdr)) != 0)
	  ocnt = snd_hdr->delta(snd_hdr,obuf,ocnt,buff_size);
	return(ocnt);
      }
    }
  }
  snd_hdr->inc_cnt = inc_cnt;
  snd_hdr->byte_cnt = byte_cnt;
  snd_hdr->samp_cnt = samp_cnt;
  return(ocnt);
}

/********** XA_Audio_PCMXS_PCM1M *********************************
 * Convert PCM 1+2 BPS Stereo Samples into PCM 1 BPS Mono Samples
 * The spec flag takes care of the various linear/signed/endian
 * conversions
 *  Input        flag 1st  2nd    3rd  4th
 *  -----------------------------------------
 *  Linear1M      0   D     D)    -    -
 *  Signed1M      1   D     D     -    -      ^80
 *  Linear2MBig   2   D     skip  D    skip
 *  Signed2MBig   3   D     skip  D    skip   ^80
 *  Linear2MLit   4   skip  D     skip D
 *  Signed2MLit   5   skip  D     skip D      ^80
 *
 *  bit 3 (& 0x08) AU output instead of PCM.
 *
 * Global Variables:
 *   XA_Audio_Next_Snd()	routine to move to next sound header.
 ***************************************************************/
ULONG XA_Audio_PCMXS_PCM1M(snd_hdr,obuf,ocnt,buff_size)
XA_SND *snd_hdr;
UBYTE *obuf;
ULONG ocnt,buff_size;
{ ULONG byte_cnt,inc,inc_cnt,spec,bps,samp_cnt;
  UBYTE *ibuf;

  if (snd_hdr==0) return(ocnt);
  spec = snd_hdr->spec;
  bps = ((spec & 2) | (spec & 4))?(4):(2);
  inc = snd_hdr->inc;		inc_cnt = snd_hdr->inc_cnt;
  byte_cnt = snd_hdr->byte_cnt;
  samp_cnt = snd_hdr->samp_cnt;
  ibuf = snd_hdr->snd;		ibuf += byte_cnt;

  while(ocnt < buff_size)
  { register LONG d0,d1; ULONG data;
    if (spec & 2) {d0 = ibuf[0]; d1 = ibuf[2];}
    else if (spec & 4) {d0 = ibuf[1]; d1 = ibuf[3];}
    else {d0 = ibuf[0]; d1 = ibuf[1];}
    if (spec & 1) 
    { 
      if (d0 & 0x80) d0 = d0 - 0x100;
      if (d1 & 0x80) d1 = d1 - 0x100;
    }
    data = (ULONG)((d0 + d1) >> 1) & 0xff;
    if (spec & 8)
    { /* note: ulaw takes signed input */
       if (spec & 1) *obuf++ = sign_2_ulaw[ data ];
       else *obuf++ = sign_2_ulaw[ (data ^ 0x80) ];
    }
    else *obuf++ = (spec & 1)?(data^0x80):(data);
    ocnt++;			inc_cnt += inc;
    while(inc_cnt >= (1<<24) )
    { inc_cnt -= (1<<24);	
      samp_cnt--;		byte_cnt += bps;	ibuf += bps;
      if (samp_cnt <= 0)
      { 
        if ( (snd_hdr = XA_Audio_Next_Snd(snd_hdr)) != 0 )
	  ocnt = snd_hdr->delta(snd_hdr,obuf,ocnt,buff_size);
	return(ocnt);
      }
    }
  }
  snd_hdr->inc_cnt = inc_cnt;
  snd_hdr->byte_cnt = byte_cnt;
  snd_hdr->samp_cnt = samp_cnt;
  return(ocnt);
}

typedef struct 
{
  ULONG bpred;
  LONG delta;
  LONG samp1;
  LONG samp2;
  ULONG nyb_flag;
  LONG nyb1;
  ULONG flag;       /* 0 don't output samples, 1 output samp1, 2 output both */
                    /* 4 un-init */
} XA_MSADPCM_HDR;
XA_MSADPCM_HDR xa_msadpcm;

void XA_Audio_Init_Snd(snd_hdr)
XA_SND *snd_hdr;
{
  snd_hdr->inc_cnt  = 0;
  snd_hdr->byte_cnt = 0;
  snd_hdr->samp_cnt = snd_hdr->tot_samps;
  if (snd_hdr->type == XA_AUDIO_ADPCM_M) xa_msadpcm.flag = 4;
}

#define MSADPCM_NUM_COEF        (7)
#define MSADPCM_MAX_CHANNELS    (2)

#define MSADPCM_PSCALE          (8)
#define MSADPCM_PSCALE_NUM      (1 << MSADPCM_PSCALE)
#define MSADPCM_CSCALE          (8)
#define MSADPCM_CSCALE_NUM      (1 << MSADPCM_CSCALE)

#define MSADPCM_DELTA4_MIN      (16)

static LONG  gaiP4[] = { 230, 230, 230, 230, 307, 409, 512, 614,
                          768, 614, 512, 409, 307, 230, 230, 230 };
static LONG gaiCoef1[] = { 256,  512,  0, 192, 240,  460,  392 };
static LONG gaiCoef2[] = {   0, -256,  0,  64,   0, -208, -232 };


/********** XA_Audio_ADPCMM_PCM2M *********************************
 * Convert Microsoft ADPCM Mono Samples into PCM 2 BPS Mono Samples
 * The spec flag takes care of the various linear/signed/endian
 * conversions
 *  Input        Ouput        Spec  1st  2nd
 *  -----------------------------------------
 *  MSADPCM to Signed Big       0    D1    D0
 *  MSADPCM to Signed Little    1    D0    D1  
 *  MSADPCM to Linear Big       2    D1^80 D0
 *  MSADPCM to Linear Little    3    D0    D1^80
 *
 *  bit 2 (& 0x04) 1 byte output.
 *  bit 3 (& 0x08) AU output instead of linear PCM.
 *
 * Global Variables:
 *   UBYTE sign_2_ulaw[256]	conversion table.
 *   XA_Audio_Next_Snd()	routine to move to next sound header.
 ***************************************************************/
ULONG XA_Audio_ADPCMM_PCM2M(snd_hdr,obuf,ocnt,buff_size)
XA_SND *snd_hdr;
UBYTE *obuf;
ULONG ocnt,buff_size;
{ ULONG byte_cnt,inc,inc_cnt,spec,tbytes,samp_cnt;
  ULONG bpred,nyb_flag,tsamps;
  LONG delta,coef1,coef2,samp1,samp2,nyb1;
  UBYTE *ibuf;

  if (snd_hdr==0) return(ocnt);
  inc = snd_hdr->inc;		inc_cnt = snd_hdr->inc_cnt;
  tbytes = snd_hdr->tot_bytes;	byte_cnt = snd_hdr->byte_cnt;
  ibuf = snd_hdr->snd;		ibuf += byte_cnt;
  samp_cnt = snd_hdr->samp_cnt;	tsamps = snd_hdr->tot_samps;
  spec = snd_hdr->spec;

DEBUG_LEVEL1 fprintf(stderr,"snd %lx buff_size %lx  bcnt %lx\n",snd_hdr,buff_size,byte_cnt);

  if (xa_msadpcm.flag==4)
  {
    bpred = *ibuf++;
    if (bpred >= MSADPCM_NUM_COEF) 
	{ fprintf(stderr,"MSADPC bpred %lx ERR 0\n",bpred); return(ocnt); }
    delta = *ibuf++; delta |= (*ibuf++)<<8;
    if (delta & 0x8000) delta = delta - 0x10000;
    samp1 = *ibuf++; samp1 |= (*ibuf++)<<8;
    if (samp1 & 0x8000) samp1 = samp1 - 0x10000;
    samp2 = *ibuf++; samp2 |= (*ibuf++)<<8; 
    if (samp2 & 0x8000) samp2 = samp2 - 0x10000;
    byte_cnt += 7;
    samp_cnt = tsamps - 1;
    xa_msadpcm.flag = 2;
    nyb_flag = 1;
  }
  else
  {
    bpred = xa_msadpcm.bpred;
    delta = xa_msadpcm.delta;
    samp1 = xa_msadpcm.samp1;
    samp2 = xa_msadpcm.samp2;
    nyb_flag = xa_msadpcm.nyb_flag;
    nyb1     = xa_msadpcm.nyb1;
  }
  coef1 = gaiCoef1[bpred];
  coef2 = gaiCoef2[bpred];
  
  while(ocnt < buff_size)
  { LONG idelta,predict,lsamp;
    LONG nyb0;
    ULONG data;

    data = samp2;

DEBUG_LEVEL1 fprintf(stderr,"ADPCM: data %05lx samp1 %05lx samp2 %05lx  samp_cnt %lx byte_cnt %lx\n",data,(samp1 &0x1ffff),(samp2&0x1ffff),samp_cnt,byte_cnt);

    /* data output munging */
    if (spec & 4)  /* 1 byte output */
    {  UBYTE d0 = ((ULONG)(data) >> 8) & 0xff;
       if (spec & 8)	*obuf++ = sign_2_ulaw[ (ULONG)(d0) ];
       else		*obuf++ = d0;
    }
    else
    { UBYTE d1,d0; d1 = (data>>8) & 0xff; d0 = data & 0xff;
      if (spec & 0x02) d1 ^= 0x80;
      if (spec & 0x01) {*obuf++ = d0; *obuf++ = d1; }
      else {*obuf++ = d1; *obuf++ = d0; }
    }
    ocnt++;		inc_cnt += inc;
    while(inc_cnt >= (1<<24) )
    { inc_cnt -= (1<<24);	

      if (samp_cnt > 1) /* are there samples left? */
      { 
        /* assuming even number of samples */
        if ((byte_cnt < tbytes) || (nyb_flag==0))
	{
	  if (nyb_flag) /* get four bit sample */
	  {
	    nyb1 = *ibuf++;  byte_cnt++;
	    nyb0 = (nyb1 >> 4) & 0x0f;
	    nyb1 &= 0x0f;
	    nyb_flag = 0;
	  }
	  else { nyb0 = nyb1; nyb_flag = 1; }
	  /* Comput next Adaptive Scale Factor(ASF) */
	  idelta = delta;
	  delta = (gaiP4[nyb0] * idelta) >> MSADPCM_PSCALE;
	  if (delta < MSADPCM_DELTA4_MIN) delta = MSADPCM_DELTA4_MIN;
	  if (nyb0 & 0x08) nyb0 = nyb0 - 0x10;
	  /* Predict next sample */
	  predict = ((samp1 * coef1) + (samp2 * coef2)) >>  MSADPCM_CSCALE;
	  /* reconstruct original PCM */
	  lsamp = (nyb0 * idelta) + predict;
	  if (lsamp > 32767) lsamp = 32767;
	  else if (lsamp < -32768) lsamp = -32768;
	  samp2 = samp1; samp1 = lsamp;
          xa_msadpcm.flag = 2;
	  samp_cnt--;
        }
	/* no more bytes left, but we have 1 sample remaing */
	/* SHOULD THIS EVERY OCCUR?? */
	else 
	{  samp_cnt = 1; xa_msadpcm.flag = 1; samp2 = samp1; 
	   fprintf(stderr,"test\n");
	}
      }
      else if (samp_cnt == 1) { xa_msadpcm.flag = 1; samp2 = samp1; samp_cnt--;}
      else /* no more samples in that Block */
      { 
        if (byte_cnt >= tbytes)
        {
	  xa_msadpcm.flag = 4;
	  if ( (snd_hdr = XA_Audio_Next_Snd(snd_hdr)) != 0)
			ocnt = snd_hdr->delta(snd_hdr,obuf,ocnt,buff_size);
	  return(ocnt);
        }
	tsamps = snd_hdr->tot_samps;
	bpred = *ibuf++;
	if (bpred > MSADPCM_NUM_COEF) 
	    { fprintf(stderr,"MSADPC bpred %lx ERR 1\n",bpred); return(ocnt); }
	delta = *ibuf++; delta |= (*ibuf++)<<8;
	if (delta & 0x8000) delta = delta - 0x10000;
	samp1 = *ibuf++; samp1 |= (*ibuf++)<<8;
	if (samp1 & 0x8000) samp1 = samp1 - 0x10000;
	samp2 = *ibuf++; samp2 |= (*ibuf++)<<8; 
	if (samp2 & 0x8000) samp2 = samp2 - 0x10000;
	byte_cnt += 7;
	samp_cnt = tsamps - 1;
	xa_msadpcm.flag = 2;
	coef1 = gaiCoef1[bpred];
	coef2 = gaiCoef2[bpred];
	nyb_flag = 1;
      } /* end of valid next chunk */
    } /* end of while inc input loop */
  } /* end of still need output loop */
  snd_hdr->inc_cnt = inc_cnt;
  snd_hdr->byte_cnt = byte_cnt;
  snd_hdr->samp_cnt = samp_cnt;
  xa_msadpcm.bpred = bpred;
  xa_msadpcm.delta = delta;
  xa_msadpcm.samp1 = samp1;
  xa_msadpcm.samp2 = samp2;
  xa_msadpcm.nyb_flag = nyb_flag;
  xa_msadpcm.nyb1     = nyb1;
  return(ocnt);
}

/********** XA_Audio_Silence *********************************
 * Provides so many samples of silence( value of silence is in ->spec ).
 *
 * Global Variables:
 *   XA_Audio_Next_Snd()	routine to move to next sound header.
 ***************************************************************/
ULONG XA_Audio_Silence(snd_hdr,obuf,ocnt,buff_size)
XA_SND *snd_hdr;
UBYTE *obuf;
ULONG ocnt,buff_size;
{ ULONG inc,inc_cnt,spec,samp_cnt;
  if (snd_hdr==0) return(ocnt);
  spec = snd_hdr->spec;
  samp_cnt = snd_hdr->samp_cnt;
  inc = snd_hdr->inc;		inc_cnt = snd_hdr->inc_cnt;

  while(ocnt < buff_size)
  {
    *obuf++ = 0x00;
    if (spec == 1) *obuf++ = 0x00;  /* two byte cases */
    
    ocnt++;			inc_cnt += inc;
    while(inc_cnt >= (1<<24) )
    { inc_cnt -= (1<<24);	samp_cnt--;
      if (samp_cnt <= 0)
      { 
        if ( (snd_hdr = XA_Audio_Next_Snd(snd_hdr)) != 0)
	  ocnt = snd_hdr->delta(snd_hdr,obuf,ocnt,buff_size);
	return(ocnt);
      }
    }
  }
  snd_hdr->inc_cnt = inc_cnt;
  snd_hdr->samp_cnt = samp_cnt;
  return(ocnt);
}

/*************************************************************************/
/********** Audio Codec Conversion Buffer Routines ***********************/
/*************************************************************************/

/**************************************************************************
 *
 * Microsoft ADPCM code.
 *
 * NO LONGER USED
 *
 *************************************************************************/

ULONG ms_adpcm_decode(src, dst, slen, dlen, SampPerBlock)
UBYTE *src;
SHORT *dst;
LONG slen;		/* size in bytes of src */
LONG dlen;		/* size in sample of dst */
LONG SampPerBlock;
{ ULONG ocnt;

  ocnt = 0; 
  while(ocnt < dlen)
  { ULONG bpred,nyb_flag;
    LONG coef1,coef2,samp1,samp2,idelta,delta,predict,lsamp;
    LONG samp_cnt,nyb0,nyb1;
    /**** READ BLOCK HEADER ************/
    /*POD NOTE: check dlen < 7 */
    bpred = *src++;
    if (bpred > MSADPCM_NUM_COEF) return(0);
    coef1 = gaiCoef1[bpred];
    coef2 = gaiCoef2[bpred];
    delta = *src++; delta |= (*src++)<<8;
    if (delta & 0x8000) delta = delta - 0x10000;
    samp1 = *src++; samp1 |= (*src++)<<8;
    if (samp1 & 0x8000) samp1 = samp1 - 0x10000;
    samp2 = *src++; samp2 |= (*src++)<<8; slen -= 7;
    if (samp2 & 0x8000) samp2 = samp2 - 0x10000;
    *dst++ = (SHORT)samp1;
    *dst++ = (SHORT)samp2; ocnt += 2;

    samp_cnt = SampPerBlock - 2;
    nyb_flag = 1;
    while(samp_cnt)
    {
      /* get four bit sample */
      if (nyb_flag)
      {
        nyb1 = *src++;  slen--;
        nyb0 = (nyb1 >> 4) & 0x0f;
        nyb1 &= 0x0f;
        nyb_flag = 0;
      }
      else { nyb0 = nyb1; nyb_flag = 1; }
    
      /* Comput next Adaptive Scale Factor(ASF) */
      idelta = delta;
      delta = (gaiP4[nyb0] * idelta) >> MSADPCM_PSCALE;
      if (delta < MSADPCM_DELTA4_MIN) delta = MSADPCM_DELTA4_MIN;
      if (nyb0 & 0x08) nyb0 = nyb0 - 0x10;

      /* Predict next sample */
      predict = ((samp1 * coef1) + (samp2 * coef2)) >>  MSADPCM_CSCALE;
      /* reconstruct original PCM */
      lsamp = (nyb0 * idelta) + predict;
      if (lsamp > 32767) lsamp = 32767;
      else if (lsamp < -32768) lsamp = -32768;

      *dst++ = (SHORT)lsamp; ocnt++;
      samp2 = samp1; samp1 = lsamp;
      samp_cnt--;
    }
  }
  return(ocnt);
}


/* The following copyright pertains only to the MS_APDCM code routine
 * above and nothing else. - Podlipec
 */

/***********************************************************
Copyright 1992 by Stichting Mathematisch Centrum, Amsterdam, The
Netherlands.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the names of Stichting Mathematisch
Centrum or CWI not be used in advertising or publicity pertaining to
distribution of the software without specific, written prior permission.

STICHTING MATHEMATISCH CENTRUM DISCLAIMS ALL WARRANTIES WITH REGARD TO
THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS, IN NO EVENT SHALL STICHTING MATHEMATISCH CENTRUM BE LIABLE
FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

******************************************************************/

