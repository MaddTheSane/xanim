
/*
 * xa_ipc_cmds.h
 *
 * Copyright (C) 1996-1998,1999 by Mark Podlipec. 
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
 * 01Mar96 - created.
 *
 ****************/


#ifdef XA_AUDIO

extern xaULONG XA_Give_Birth();
extern xaULONG XA_Video_Receive_Ack();
extern xaULONG XA_Video_Send2_Audio();
extern void XA_IPC_Close_Pipes();
extern void XA_IPC_Set_Debug();

#define XA_AUDIO_SETUP		{ if (xa_forkit == xaTRUE) xa_forkit = \
  XA_Video_Send2_Audio(XA_IPC_AUD_SETUP,NULL,0,0,2000,&xa_vaudio_status); }
#define XA_AUDIO_KILL()		{ if (xa_forkit == xaTRUE) xa_forkit = \
  XA_Video_Send2_Audio(XA_IPC_AUD_KILL,NULL,0,0,2000,&xa_vaudio_status); }
#define XA_AUDIO_PREP()		{ if (xa_forkit == xaTRUE) xa_forkit = \
  XA_Video_Send2_Audio(XA_IPC_AUD_PREP,NULL,0,0,10000,&xa_vaudio_status); }
#define XA_AUDIO_ON()		{ if (xa_forkit == xaTRUE) xa_forkit = \
  XA_Video_Send2_Audio(XA_IPC_AUD_ON,NULL,0,0,2000,&xa_vaudio_status); }
#define XA_AUDIO_OFF(x)		{ if (xa_forkit == xaTRUE) xa_forkit = \
  XA_Video_Send2_Audio(XA_IPC_AUD_OFF,NULL,0,x,2000,&xa_vaudio_status); }
#define XA_AUDIO_INIT(x)	{ if (xa_forkit == xaTRUE) xa_forkit = \
  XA_Video_Send2_Audio(XA_IPC_AUD_INIT,NULL,0,0,2000,&x); }
#define XA_SET_OUTPUT_PORT(x)	{ if (xa_forkit == xaTRUE) xa_forkit = \
  XA_Video_Send2_Audio(XA_IPC_AUD_PORT,NULL,0,x,2000,0); }
#define XA_SPEAKER_TOG(x)	{ if (xa_forkit == xaTRUE) xa_forkit = \
  XA_Video_Send2_Audio(XA_IPC_AUD_STOG,NULL,0,x,2000,0); }
#define XA_HEADPHONE_TOG(x)	{ if (xa_forkit == xaTRUE) xa_forkit = \
  XA_Video_Send2_Audio(XA_IPC_AUD_HTOG,NULL,0,x,2000,0); }
#define XA_LINEOUT_TOG(x)	{ if (xa_forkit == xaTRUE) xa_forkit = \
  XA_Video_Send2_Audio(XA_IPC_AUD_LTOG,NULL,0,x,2000,0); }
#define XA_AUDIO_INIT_SND	{ if (xa_forkit == xaTRUE) xa_forkit = \
  XA_Video_Send2_Audio(XA_IPC_SND_INIT,NULL,0,0,2000,0); }
#define XA_AUDIO_SET_VOLUME(x)	{ if (xa_forkit == xaTRUE) xa_forkit = \
  XA_Video_Send2_Audio(XA_IPC_AUD_VOL,0,0,x,1000,0);  }
#define XA_AUDIO_SET_MUTE(x)	{ if (xa_forkit == xaTRUE) xa_forkit = \
  XA_Video_Send2_Audio(XA_IPC_AUD_MUTE,0,0,x,1000,0);  }
#define XA_AUDIO_SET_RATE(x);	{ if (xa_forkit == xaTRUE) xa_forkit = \
  XA_Video_Send2_Audio(XA_IPC_AUD_RATE,0,0,x,1000,0);  }
#define XA_AUDIO_SET_ENABLE(x)	{ if (xa_forkit == xaTRUE) xa_forkit = \
  XA_Video_Send2_Audio(XA_IPC_AUD_ENABLE,0,0,x,1000,0); }
#define XA_AUDIO_SET_FFLAG(x)	{ if (xa_forkit == xaTRUE) xa_forkit = \
  XA_Video_Send2_Audio(XA_IPC_AUD_FFLAG,0,0,x,1000,0); }
#define XA_AUDIO_SET_BFLAG(x)	{ if (xa_forkit == xaTRUE) xa_forkit = \
  XA_Video_Send2_Audio(XA_IPC_AUD_BFLAG,0,0,x,1000,0); }
#define XA_AUDIO_GET_STATUS(x)	{ if (xa_forkit == xaTRUE) xa_forkit = \
  XA_Video_Send2_Audio(XA_IPC_GET_STATUS,0,0,0,1000,&x); \
  else x = XA_AUDIO_NICHTDA; } 
#define XA_AUDIO_PLAY_FILE(x,y)	{ if (xa_forkit == xaTRUE) xa_forkit = \
  XA_Video_Send2_Audio(XA_IPC_PLAY_FILE, 0,0,x,1000,&y);  }
#define XA_AUDIO_SET_AUD_BUFF(x) { if (xa_forkit == xaTRUE) xa_forkit = \
  XA_Video_Send2_Audio(XA_IPC_SET_AUDBUFF, 0,0,(x),1000,0);  }
#define XA_AUDIO_SET_KLUDGE2(x) { if (xa_forkit == xaTRUE) xa_forkit = \
  XA_Video_Send2_Audio(XA_IPC_SET_KLUDGE2, 0,0,(x),1000,0);  }
#define XA_AUDIO_SET_KLUDGE900(x) { if (xa_forkit == xaTRUE) xa_forkit = \
  XA_Video_Send2_Audio(XA_IPC_SET_KLUDGE900, 0,0,(x),1000,0);  }
#define XA_AUDIO_N_FILE(x,y)	{ if (xa_forkit == xaTRUE) xa_forkit = \
  XA_Video_Send2_Audio(XA_IPC_N_FILE,0,0,x,1000,&y);  }
#define XA_AUDIO_EXIT()		{ if (xa_forkit == xaTRUE) xa_forkit = \
  XA_Video_Send2_Audio(XA_IPC_EXIT,NULL,0,0,1000,0); \
  xa_forkit = xaFALSE; }
#define XA_AUDIO_VID_TIME(x)	{ if (xa_forkit == xaTRUE) xa_forkit = \
  XA_Video_Send2_Audio(XA_IPC_VID_TIME,NULL,0,x,1000,0); }
#define XA_AUDIO_FILE(num)	{ if (xa_forkit == xaTRUE) xa_forkit = \
  XA_Video_Send2_Audio(XA_IPC_FILE,0,0,num,1000,0); }
#define XA_AUDIO_FNAME(fn,len,num)  { if (xa_forkit == xaTRUE) xa_forkit = \
  XA_Video_Send2_Audio(XA_IPC_FNAME,fn,len,num,1000,0); }
#define XA_AUDIO_RST_TIME(x)	{ if (xa_forkit == xaTRUE) xa_forkit = \
  XA_Video_Send2_Audio(XA_IPC_RST_TIME,0,0,x,2000,0); }
#define XA_AUDIO_UNFILE(num)	{ if (xa_forkit == xaTRUE) xa_forkit = \
  XA_Video_Send2_Audio(XA_IPC_UNFILE,0,0,num,1000,0); }
#define XA_AUDIO_MERGEFILE(num)	{ if (xa_forkit == xaTRUE) xa_forkit = \
  XA_Video_Send2_Audio(XA_IPC_MERGEFILE,0,0,num,1000,0); }

#else

#define XA_AUDIO_SETUP
#define XA_AUDIO_KILL()
#define XA_AUDIO_PREP()
#define XA_AUDIO_ON()
#define XA_AUDIO_OFF(x)
#define XA_AUDIO_INIT(x)	{ x = XA_AUDIO_ERR; }
#define XA_SET_OUTPUT_PORT(x)
#define XA_SPEAKER_TOG(x)
#define XA_HEADPHONE_TOG(x)
#define XA_LINEOUT_TOG(x)
#define XA_AUDIO_INIT_SND
#define XA_AUDIO_SET_VOLUME(x)
#define XA_AUDIO_SET_MUTE(x)
#define XA_AUDIO_SET_RATE(x)
#define XA_AUDIO_SET_ENABLE(x);
#define XA_AUDIO_SET_FFLAG(x);
#define XA_AUDIO_SET_BFLAG(x);
#define XA_AUDIO_GET_STATUS(x)	{ x = xaFALSE;; }
#define XA_AUDIO_PLAY_FILE(x,y)
#define XA_AUDIO_SET_AUD_BUFF(x)
#define XA_AUDIO_SET_KLUDGE2(x)
#define XA_AUDIO_SET_KLUDGE900(x)
#define XA_AUDIO_N_FILE(x,y)
#define XA_AUDIO_EXIT()
#define XA_AUDIO_VID_TIME(x)
#define XA_AUDIO_FILE(num)
#define XA_AUDIO_FNAME(fn,len,num)
#define XA_AUDIO_RST_TIME(x)
#define XA_AUDIO_UNFILE(num)
#define XA_AUDIO_MERGEFILE(num)

#endif

