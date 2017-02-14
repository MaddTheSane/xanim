
/*
 * xa_ipc.c
 *
 * Copyright (C) 1995-1998,1999 by Mark Podlipec. 
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
 * 03Jun95 - Created
 * *****96 - did some stuff
 * *****97 - did a little more
 * *****98 - wished I could rewrite it
 * 21Feb99 - Change AUDIO_ON to be AUDIO_PREP then AUDIO_ON
 *
 *******************************/


#include "xanim.h"
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include <sys/signal.h>

#ifdef XA_SOCKET
#include <sys/socket.h>
#endif

#ifdef XA_SELECT
#include <sys/select.h>
#endif

#ifdef VMS
#include <lib$routines.h>
#include <starlet.h>
#ifdef R3_INTRINSICS
typedef void *XtPointer;
#endif
#endif

#include "xa_ipc.h"

XtAppContext	theAudContext;
Display		*theAudDisp;
xaULONG		xa_audio_present;
xaULONG		xa_audio_status;
XA_AUD_FLAGS *audiof;


#ifdef XA_AUDIO

/* POD NOTE: Check for NOFILE defined in parms.h??? */
#ifndef FD_SETSIZE
#define FD_SETSIZE 64
#endif

static xaULONG audio_debug_flag = xaFALSE;
xaLONG xa_child_last_time = 0;
int xa_audio_parent_pid = -1;

#define AUD_DEBUG if (audio_debug_flag == xaTRUE)


extern xaULONG xa_audio_hard_buff;   /* AUDIO DOMAIN */
extern xaLONG xa_av_time_off;
extern void New_Merged_Audio_Output();
extern int xa_aud_fd;
extern xaUBYTE *xa_audcodec_buf;
extern xaULONG xa_audcodec_maxsize;
extern xaULONG xa_kludge2_dvi;
extern xaULONG xa_kludge900_aud;

/**** Extern xa_audio Functions *****/
extern void XA_Audio_Setup();
extern xaULONG (*XA_Audio_Init)();
extern void (*XA_Audio_Kill)();
extern void (*XA_Audio_Off)();
extern void (*XA_Audio_Prep)();
extern void (*XA_Audio_On)();
extern xaULONG (*XA_Closest_Freq)();
extern void  (*XA_Set_Output_Port)();
extern void  (*XA_Speaker_Tog)();
extern void  (*XA_Headphone_Tog)();
extern void  (*XA_LineOut_Tog)();
void XA_Audio_Init_Snd();
extern xaULONG XA_IPC_Sound();
extern xaLONG XA_Time_Read();
extern void XA_Read_Audio_Delta();


XA_AUD_HDR *xa_aud_hdr_start,*xa_aud_hdr_cur;
xaULONG xa_aud_hdr_num = 0;

void XA_Child_Loop();
void XA_Child_BOFL();
void XA_Child_Dies();
void XA_Child_Dies1();

xaULONG XA_IPC_Receive();
xaULONG XA_IPC_Send();
void XA_Audio_Child();
xaULONG XA_Video_Send2_Audio();
xaULONG XA_Video_Receive_Ack();
xaULONG XA_Audio_Receive_Video_CMD();
xaUBYTE *XA_Audio_Receive_Video_Buf();
xaULONG XA_Audio_Send_ACK();
xaULONG XA_Child_Find_File();
void XA_IPC_Reset_AV_Time();
void Free_SNDs();

extern XA_SND *xa_snd_cur;
extern xaLONG xa_time_audio;
extern xaULONG xa_timelo_audio;

static xaULONG xa_ipc_cmd_id = 1;

int xa_audio_fd[2];    /* audio child reads this, video writes this */
int xa_video_fd[2];    /* video reads this, audio child writes this */

xaLONG xa_audio_child = -1;

/************ XAnim Audio Child Code ***********************************/

/***************************************
 *  Routine for sending a buffer across the pipe. Handles partial writes
 *  caused to interrupts, full buffers, etc.
 *
 *  ? Implement Timeout ?
 **************/
xaULONG XA_IPC_Send(fd,p,len,who)
int fd;
char *p;
int len;
int who;
{ while(len > 0)
  { int ret;
    ret = write( fd, p, len );
    if (ret < 0) 
    { AUD_DEBUG fprintf(stderr,"IPC(%d) Send ERR: %d\n",who,errno); 
      return(XA_IPC_ERR);
    }
    else { len -= ret; p += ret; }
  }
  return(XA_IPC_OK);
}

/***************************************
 *  Routine for receiving a buffer across the pipe. Handles partial reads
 *  caused to interrupts, empty buffers, etc.
 *
 *  ? Implement Select ?
 **************/
xaULONG XA_IPC_Receive(fd,p,len,timeout,who)
int fd;
char *p;
int len;
xaLONG timeout;
int who;
{ xaLONG cur_time,ack_time;

  cur_time = ack_time = XA_Time_Read();
  ack_time += timeout;
  do
  { int ret;
    ret = read( fd, p, len);
    if (ret < 0) 
    { AUD_DEBUG fprintf(stderr,"IPC(%d) Receive ERR %d\n",who,errno); 
      return(XA_IPC_ERR);
    }
    else  /* ?POD overrun ever possible??? */
    { len -= ret; if (len <= 0) break;
      p += ret;
    }
    cur_time = XA_Time_Read();
  } while(cur_time < ack_time);

  if (len != 0)
  { AUD_DEBUG fprintf(stderr,"IPC(%d) Receive TimeOut %dms\n",who,timeout);
    return(XA_IPC_TOD);
  }

  return(XA_IPC_OK);
}

/***************************************
 * This routine blocks until something is ready on fd or
 * until a timeout occurrs.
 * 
 * Timeout is in milliseconds. 
 **************/
xaULONG XA_IPC_Select(fd,timeout,who)
int fd;
xaLONG timeout;
int who;
{ int ret,tt_sec,tt_usec;
  int width = FD_SETSIZE;
  fd_set readfds, writefds, exceptfds;
  struct timeval t_timeout;
 
  tt_sec = timeout / 1000;
  tt_usec = timeout - (tt_sec * 1000);
  tt_usec *= 1000;
  
/* AUD_DEBUG fprintf(stderr,"tt_sec %d tt_usec %d\n",tt_sec,tt_usec); */

  FD_ZERO(&readfds);
  FD_SET( fd , &readfds);
  FD_ZERO(&writefds);
  FD_ZERO(&exceptfds);
  t_timeout.tv_sec =  tt_sec;
  t_timeout.tv_usec = tt_usec;
 
  ret = select(width, &readfds, &writefds, &exceptfds, &t_timeout);
 
  if (ret < 0) AUD_DEBUG fprintf(stderr,"AUD select ERR: %d\n",errno);
  return(ret);
}

/***************************************
 * Routine for having the Video Process receive keep alive message.
 *
 * ack_flag is number of ms to wait for ACK. 
 *********/
xaULONG XA_Video_Receive_Ack(ack_flag)
xaULONG ack_flag;
{
  XA_IPC_HDR ipc_ack;
  xaULONG ret;

  /*** Read ACK */
  ret = XA_IPC_Receive(xa_video_fd[XA_FD_READ],((char *)(&ipc_ack)),
                (sizeof(XA_IPC_HDR)),ack_flag,XA_IAM_VIDEO);
  if (ret != XA_IPC_OK)
        { AUD_DEBUG fprintf(stderr,"Vid chunk Ack Err\n"); return(xaFALSE); }
  return(xaTRUE);
}

/***************************************
 *  Routine for having the Video Process send a Command with
 *  optional Command Acknowledgement and option data buffer.
 *
 *  ack_flag is number of ms to wait for ACK. If 0, then no ACK is checked
 *  for.
 **************/
xaULONG XA_Video_Send2_Audio(cmd,buf,buf_len,value,ack_flag,ack_val)
xaULONG cmd;
xaUBYTE *buf;
xaULONG buf_len;
xaULONG value;
xaULONG ack_flag;
xaULONG *ack_val;
{ XA_IPC_HDR ipc_cmd,ipc_ack;
  xaULONG ret;
  xaLONG len;
  char *p;

  /*** Setup Command */
  ipc_cmd.cmd = cmd;
  ipc_cmd.time = XA_Time_Read();
  ipc_cmd.len = buf_len;
  ipc_cmd.value = value;
  ipc_cmd.id  = xa_ipc_cmd_id;   
  xa_ipc_cmd_id++;

  /*** SEND IPC Command */
  ret = XA_IPC_Send( xa_audio_fd[XA_FD_WRITE], ((char *)(&ipc_cmd)), 
					(sizeof(XA_IPC_HDR)), XA_IAM_VIDEO );
  if (ret == XA_IPC_ERR) 
	{ AUD_DEBUG fprintf(stderr,"Vid Send Cmd Err\n"); return(xaFALSE); }


  /* SEND Data if Any */
  p = (char *)(buf);
  len = buf_len;
  while(len > 0)
  { int sel_ret,tlen = len;

    if (tlen > XA_IPC_CHUNK) tlen = XA_IPC_CHUNK;
    AUD_DEBUG fprintf(stderr,"VID IPC Sendin Chunk tlen %d\n",tlen);
    ret = XA_IPC_Send( xa_audio_fd[XA_FD_WRITE], p, tlen, XA_IAM_VIDEO );
    if (ret == XA_IPC_ERR)
	{ AUD_DEBUG fprintf(stderr,"Vid Send Buf Err\n"); return(xaFALSE); }
    p += tlen;;  len -= tlen;

    /*** wait for ACK */
    sel_ret = XA_IPC_Select(xa_video_fd[XA_FD_READ], ack_flag,XA_IAM_VIDEO); 
    if (sel_ret <= 0)
    {
      AUD_DEBUG fprintf(stderr,"VID: chunk Ack Timeout/err: cmd %x %d\n",cmd,sel_ret);
      return(xaFALSE);
    }

    /*** Read ACK */
    do
    {
      ret = XA_IPC_Receive(xa_video_fd[XA_FD_READ],((char *)(&ipc_ack)),
		(sizeof(XA_IPC_HDR)),ack_flag,XA_IAM_VIDEO);
      if (ret != XA_IPC_OK)
	{ AUD_DEBUG fprintf(stderr,"Vid chunk Ack Err\n"); return(xaFALSE); }
    } while(ipc_ack.cmd == XA_IPC_BOFL);
    AUD_DEBUG fprintf(stderr,"VID IPC Sent Chunk OK\n");
  }

  /*** Look For ACK */
  if (ack_flag)
  { int sel_ret = XA_IPC_Select(xa_video_fd[XA_FD_READ], ack_flag,XA_IAM_VIDEO);
    if (sel_ret <= 0)
    {
      AUD_DEBUG fprintf(stderr,"VID: Ack Timeout/err: cmd %x %d\n",cmd,sel_ret);
      return(xaFALSE);
    }

    do
    {
      ret = XA_IPC_Receive(xa_video_fd[XA_FD_READ],((char *)(&ipc_ack)),
				(sizeof(XA_IPC_HDR)),ack_flag,XA_IAM_VIDEO);
      if (ret == XA_IPC_ERR) 
	   {AUD_DEBUG fprintf(stderr,"Vid IPC Ack Err\n"); return(xaFALSE);}
      else if (ret == XA_IPC_TOD) 
	   {AUD_DEBUG fprintf(stderr,"Vid IPC Ack Timeout\n"); return(xaFALSE);}
    } while(ipc_ack.cmd == XA_IPC_BOFL);

    if (ipc_cmd.id != ipc_ack.id) 
    {
      AUD_DEBUG fprintf(stderr,"VID IPC ID mismatch %d %d cmd %x\n",
					ipc_cmd.id,ipc_ack.id,ipc_cmd.cmd);
      return(xaFALSE); 
    }
    if (ack_val) *ack_val = ipc_ack.value;
  }

  AUD_DEBUG fprintf(stderr,"VID IPC Success %d %u cmd %x\n",
	ipc_cmd.time,ipc_ack.time,ipc_cmd.cmd);
  return(xaTRUE);
}

/***************************************
 * Accept Command from Video Process.  Returns CMD on success 
 * or XA_IPC_ERR on failure.
 * 
 **************/
xaULONG XA_Audio_Receive_Video_CMD(ipc_cmd,timeout)
XA_IPC_HDR *ipc_cmd;
xaLONG timeout;
{ xaULONG ret;

  ret = XA_IPC_Receive(xa_audio_fd[XA_FD_READ], ((char *)(ipc_cmd)),
				(sizeof(XA_IPC_HDR)),timeout, XA_IAM_AUDIO );
  if (ret == XA_IPC_ERR) 
	{ AUD_DEBUG fprintf(stderr,"AUD Receive CMD Err\n"); return(XA_IPC_ERR); }
  else if (ret == XA_IPC_TOD)
	{ AUD_DEBUG fprintf(stderr,"AUD Receive CMD TOD\n"); return(XA_IPC_TOD); }
  
  AUD_DEBUG fprintf(stderr,"AUD SUCCESS! cmd %x len %d id %d\n",
			ipc_cmd->cmd,ipc_cmd->len,ipc_cmd->id);
  return(XA_IPC_OK);
}


/***************************************
 * Accept Buffer from Video Process.  Returns buff_len on success 
 * or 0 on failure.
 * 
 **************/
xaUBYTE *XA_Audio_Receive_Video_Buf(len,timeout)
xaULONG len;	/* len of buffer being sent (from ipc_cmd.len) */
xaLONG timeout;
{ int blen;
  xaUBYTE *b,*p;
  
  b = (xaUBYTE *)malloc( len );
  if (b == 0) {  AUD_DEBUG fprintf(stderr,"AUD Rx BUF: malloc err\n"); return(0); }

  p = b;
  blen = len;
  while(blen > 0)
  { int ret,tlen;
    ret = XA_IPC_Select(xa_audio_fd[XA_FD_READ], 500,XA_IAM_AUDIO);
    if (ret <= 0) { free(b); return(0); }

    tlen = blen; if (tlen > XA_IPC_CHUNK) tlen = XA_IPC_CHUNK;
    ret = read( xa_audio_fd[XA_FD_READ], p, tlen);
    if (ret != tlen)  /* POD improve: make while() etc */
	{ AUD_DEBUG fprintf(stderr,"read err %d %d\n",ret,tlen); free(b); return(0); }
    p += tlen; 
    blen -= tlen;
    XA_Audio_Send_ACK(XA_IPC_OK,0,0);
    xa_child_last_time = XA_Time_Read();
  }
  return(b);
}


/***************************************
 * This routines sends an Acknowledgement back the Video Process.
 * 
 **************/
xaULONG XA_Audio_Send_ACK(ack,id,value)
xaULONG ack;
xaULONG id;
xaULONG value;
{ XA_IPC_HDR ipc_ack;
  xaULONG ret;

  /****************** Send ACK Back to Video */
  ipc_ack.cmd = ack;
  ipc_ack.time = XA_Time_Read();
  ipc_ack.len = 0;
  ipc_ack.id  = id;
  ipc_ack.value = value;
  ret = XA_IPC_Send( xa_video_fd[XA_FD_WRITE], ((char *)(&ipc_ack)), 
					(sizeof(XA_IPC_HDR)), XA_IAM_AUDIO );
  if (ret == XA_IPC_ERR) 
  { AUD_DEBUG fprintf(stderr,"AUD Send ACK Err\n"); 
    fprintf(stderr,"Audio_Send_ACK IPC ERR: dying\n");
    XA_Child_Dies((int)(0));
    return(XA_IPC_ERR);
  }
  if (ack == XA_IPC_ACK_ERR)  
  {
     AUD_DEBUG fprintf(stderr,"Sent XA_IPC_ACK_ERR, now dyin\n");
     XA_Child_Dies((int)(0));  /* terminate audio process */
  }
  return(ack);
}


/***************************************
 * This routines cleans up after the Child and then
 * exits.
 *
 **************/
void XA_Child_Dies(dummy)
int	dummy;
{ XA_AUD_HDR *aud_hdr;

AUD_DEBUG fprintf(stderr,"CHILD IS DYING\n");
  aud_hdr = xa_aud_hdr_start;
  if (aud_hdr) xa_aud_hdr_start->prev->next = 0;  /* break loop */
  while(aud_hdr)
  { XA_AUD_HDR *tmp_hdr = aud_hdr->next;
   /* FREE (aud_hdr->snd) loop */
   if (aud_hdr->filename) free(aud_hdr->filename);
   free(aud_hdr);
   aud_hdr = tmp_hdr;
  }
  if (audiof)
  {
    if (audiof->device) free(audiof->device);
    free(audiof);
  }
  XtDestroyApplicationContext(theAudContext);
AUD_DEBUG fprintf(stderr,"CHILD IS DEAD\n");
  exit(0);
}

/***************************************
 * This routine prints out a message and then calls XA_Child_Dies.
 *
 **************/
void XA_Child_Dies1(s)
char *s;
{
  AUD_DEBUG fprintf(stderr,"CHILD: %s\n",s);
  XA_Child_Dies((int)(0));
}

/***************************************
 * This routine returns a XA_AUD_HDR structure.
 * and removes it from the loop.
 *
 **************/
XA_AUD_HDR *Return_Aud_Hdr(aud_hdr)
XA_AUD_HDR *aud_hdr;
{ XA_AUD_HDR *tmp_hdr;
  AUD_DEBUG fprintf(stderr,"RETURN AUD HDR\n");
  if ((aud_hdr==0) || (xa_aud_hdr_start==0)) 
				XA_Child_Dies1("Return_Anim_Hdr err");
  xa_aud_hdr_num--;
  if (xa_aud_hdr_num == 0)
  {
    xa_aud_hdr_start = 0;
    tmp_hdr = 0;
  }
  else /* removed aud_hdr from the loop */
  {
    tmp_hdr		= aud_hdr->prev;
    tmp_hdr->next	= aud_hdr->next;
    aud_hdr->next->prev	= tmp_hdr;
  }
  if (aud_hdr->filename) free(aud_hdr->filename);
  free(aud_hdr);
  return(tmp_hdr);
}

void Free_SNDs(snd)
XA_SND *snd;
{ while (snd)
  { XA_SND *tmp = snd;
    if (snd->snd) { free(snd->snd); snd->snd = 0; }
    snd = snd->next;
    free(tmp);
  }
}


/***************************************
 * This routine allocates a XA_AUD_HDR structure.
 *  aud_file is assumed to be consumable.
 *
 **************/
XA_AUD_HDR *Get_Aud_Hdr(aud_hdr,num)
XA_AUD_HDR *aud_hdr;
xaULONG num;
{
  XA_AUD_HDR *temp_hdr;
  temp_hdr = (XA_AUD_HDR *)malloc( sizeof(XA_AUD_HDR) );
  if (temp_hdr == 0) XA_Child_Dies1("Get_AUD_Hdr: malloc failed\n");

  temp_hdr->num = num;
  temp_hdr->filename = 0;
  temp_hdr->max_faud_size = 0;
  temp_hdr->first_snd = 0;
  temp_hdr->last_snd = 0;

  if (aud_hdr == 0)
  {
    xa_aud_hdr_start  = temp_hdr;
    temp_hdr->next = temp_hdr;
    temp_hdr->prev = temp_hdr;
  }
  else
  {
    temp_hdr->prev   = aud_hdr;
    temp_hdr->next   = aud_hdr->next;
    aud_hdr->next    = temp_hdr;
    xa_aud_hdr_start->prev = temp_hdr;
  }
  return(temp_hdr);
}


/***************************************
 *
 *
 **************/
void XA_Audio_Child()
{ int argc = 0;
  xa_aud_hdr_start = xa_aud_hdr_cur = 0;
  xa_aud_hdr_num = 0;

  audiof = (XA_AUD_FLAGS *)malloc( sizeof(XA_AUD_FLAGS) );
  if (audiof==0) XA_Child_Dies1("audiof malloc err");

  xa_audio_parent_pid = getppid();

/* don't init */
  audiof->enable	= xaFALSE;
  audiof->mute		= xaFALSE;
  audiof->newvol	= xaTRUE;
  audiof->divtest	= 2;
  audiof->fromfile	= xaFALSE;
  audiof->bufferit	= xaFALSE;

  audiof->port		= DEFAULT_XA_AUDIO_PORT;
  audiof->volume	= 0;
  audiof->playrate	= 0;
  audiof->device	= 0;

  signal(SIGINT,XA_Child_Dies);
  signal(SIGPIPE,XA_Child_Dies);

  XtToolkitInitialize();
  theAudContext = XtCreateApplicationContext();
  /* do we need a Display? */
  theAudDisp = XtOpenDisplay(theAudContext, NULL, "xanimaud", "XAnimAud",
				NULL,0,&argc,0);
  if (theAudDisp == NULL) { TheEnd1("Unable to open display\n"); }

/* POD DEBUGGING PURPOSES */
  AUD_DEBUG fprintf(stderr,"CHILD IS AWAKE\n");

  XtAppAddInput(theAudContext, xa_audio_fd[XA_FD_READ],
		(XtPointer)XtInputReadMask,
		(XtInputCallbackProc)XA_Child_Loop, 0);

  /* Tell Video We're alive */
  XA_Audio_Send_ACK(XA_IPC_OK,0,0);

  /* Have Child check for Mommy/Daddy once in a while.
   * That way if they kick off and the Child is orphaned, it can join them.
   */
  XtAppAddTimeOut(theAudContext,5000,(XtTimerCallbackProc)XA_Child_BOFL,
                                	                (XtPointer)(NULL));

  XtAppMainLoop(theAudContext);

  XA_Child_Dies((int)(0));
}

void XA_Child_BOFL()
{ xaLONG now = XA_Time_Read();

  AUD_DEBUG fprintf(stderr,"CHILD_BOFL now %d last %d ppid %d\n",
				now,xa_child_last_time,xa_audio_parent_pid);
  /* 24 expiration fail safe */
  if ( (now - xa_child_last_time) > 86400000) XA_Child_Dies1("Parents??");

  /* Mom? Dad? every 5 seconds */
  if ( (now - xa_child_last_time) > 8000)
  { int ppid = getppid();
    if (ppid != xa_audio_parent_pid) XA_Child_Dies1("Parents??");
  }
  XtAppAddTimeOut(theAudContext,5000, (XtTimerCallbackProc)XA_Child_BOFL,
                                	                (XtPointer)(NULL));
}

/***************************************
 *
 *
 **************/
void XA_Child_Loop(w,fin,id)
XtPointer  w;
int	   *fin;
XtInputId  *id;
{ int ret;
  XA_IPC_HDR ipc_cmd;
  xaUBYTE *ipc_buff;

  AUD_DEBUG fprintf(stderr,"LoopED\n");

/* Technically we KNOW *fin is xa_audio_fd[XA_FD_READ] */

  ret = XA_Audio_Receive_Video_CMD(&ipc_cmd, 5000);
  if (ret == XA_IPC_ERR)
  {
    XtRemoveInput(*id);
  }
  else if (ret == XA_IPC_OK)
  {
    switch( ipc_cmd.cmd )
    {
	/* send back status = STOPPED */
      case XA_IPC_AUD_SETUP:
	AUD_DEBUG fprintf(stderr,"AUD IPC: AUD_SETUP\n");
	XA_Audio_Setup();
        XA_Audio_Send_ACK(XA_IPC_ACK_OK,ipc_cmd.id,xa_audio_status);
	break;

      case XA_IPC_AUD_INIT:
	AUD_DEBUG fprintf(stderr,"AUD IPC: AUD_INIT\n");
	if (xa_audio_present == XA_AUDIO_UNK) XA_Audio_Init();
        XA_Audio_Send_ACK(XA_IPC_ACK_OK,ipc_cmd.id,xa_audio_present);
	break;

      case XA_IPC_AUD_KILL:
	AUD_DEBUG fprintf(stderr,"AUD IPC: AUD_KILL\n");
	XA_Audio_Kill();
        XA_Audio_Send_ACK(XA_IPC_ACK_OK,ipc_cmd.id,xa_audio_status);
	break;

	/* solely for returning audio status */
      case XA_IPC_GET_STATUS:
	AUD_DEBUG fprintf(stderr,"AUD IPC: GET_STATUS\n");
        XA_Audio_Send_ACK(XA_IPC_ACK_OK,ipc_cmd.id,xa_audio_status);
	break;

      case XA_IPC_GET_PRESENT:
	AUD_DEBUG fprintf(stderr,"AUD IPC: GET_PRESENT\n");
        XA_Audio_Send_ACK(XA_IPC_ACK_OK,ipc_cmd.id,xa_audio_present);
	break;

      case XA_IPC_AUD_PREP:
	AUD_DEBUG fprintf(stderr,"AUD IPC: AUD_PREP  %x\n",(xaULONG)xa_snd_cur);
	XA_Audio_Prep();
        XA_Audio_Send_ACK(XA_IPC_ACK_OK,ipc_cmd.id,xa_audio_status);
	break;

      case XA_IPC_AUD_ON:
	AUD_DEBUG fprintf(stderr,"AUD IPC: AUD_ON  %x\n",(xaULONG)xa_snd_cur);
	XA_Audio_On();
        XA_Audio_Send_ACK(XA_IPC_ACK_OK,ipc_cmd.id,xa_audio_status);
	break;

      case XA_IPC_AUD_OFF:
	AUD_DEBUG fprintf(stderr,"AUD IPC: AUD_OFF\n");
	XA_Audio_Off(ipc_cmd.value);
        XA_Audio_Send_ACK(XA_IPC_ACK_OK,ipc_cmd.id,xa_audio_status);
	break;

      case XA_IPC_AUD_PORT:
	audiof->port = ipc_cmd.value;
	AUD_DEBUG fprintf(stderr,"AUD IPC: AUD_PORT\n");
	if (XA_Set_Output_Port) XA_Set_Output_Port(ipc_cmd.value);
        XA_Audio_Send_ACK(XA_IPC_ACK_OK,ipc_cmd.id,0);
	break;

      case XA_IPC_AUD_STOG:
	AUD_DEBUG fprintf(stderr,"AUD IPC: AUD_STOG\n");
	XA_Speaker_Tog(ipc_cmd.value);
        XA_Audio_Send_ACK(XA_IPC_ACK_OK,ipc_cmd.id,0);
	break;

      case XA_IPC_AUD_HTOG:
	AUD_DEBUG fprintf(stderr,"AUD IPC: AUD_HTOG\n");
	XA_Headphone_Tog(ipc_cmd.value);
        XA_Audio_Send_ACK(XA_IPC_ACK_OK,ipc_cmd.id,0);
	break;

      case XA_IPC_AUD_LTOG:
	AUD_DEBUG fprintf(stderr,"AUD IPC: AUD_LTOG\n");
	XA_LineOut_Tog(ipc_cmd.value);
        XA_Audio_Send_ACK(XA_IPC_ACK_OK,ipc_cmd.id,0);
	break;

      case XA_IPC_GET_CFREQ:
	{ xaULONG hfreq = ipc_cmd.value;
	  AUD_DEBUG fprintf(stderr,"AUD IPC: AUD_CFREQ %x freq %d\n",
					(xaULONG)(XA_Closest_Freq),hfreq);
	  hfreq = XA_Closest_Freq(hfreq);
	  AUD_DEBUG fprintf(stderr,"CFREQ: hfreq = %d\n",hfreq);
          XA_Audio_Send_ACK(XA_IPC_ACK_OK,ipc_cmd.id,hfreq);
	}
	break;

/*POD NOTE: CFREQ must be called before this */
      case XA_IPC_GET_BSIZE:
	{ AUD_DEBUG fprintf(stderr,"AUD IPC: AUD_BSIZE\n");
          XA_Audio_Send_ACK(XA_IPC_ACK_OK,ipc_cmd.id,xa_audio_hard_buff);
	}
	break;

      case XA_IPC_FILE:
	AUD_DEBUG fprintf(stderr,"AUD IPC: received FILE %d\n",ipc_cmd.value);
	xa_aud_hdr_cur = Get_Aud_Hdr(xa_aud_hdr_cur,ipc_cmd.value);
	XA_Audio_Send_ACK(XA_IPC_ACK_OK,ipc_cmd.id,0);
	break;

	/*************************
	 * This Command sets xa_aud_hdr_cur to Audio File num
	 * and then changes/sets the filename to incoming buffer
	 ********/
      case XA_IPC_FNAME:
	AUD_DEBUG fprintf(stderr,"AUD FNAME: %d\n",ipc_cmd.value);
        if (ipc_cmd.len) 
	{ int file_ret = XA_Child_Find_File(ipc_cmd.value,0);
	  ipc_buff     = XA_Audio_Receive_Video_Buf(ipc_cmd.len,500);
	     /* if no such file or couldn't read name - err and exit */
	  if ( (ipc_buff == 0) || (file_ret == xaNOFILE) )
	  { ipc_buff = 0;
	    XtRemoveInput(*id);
	    fprintf(stderr,"AUD FNAME: buf %x fnum %d fret %d\n",
			(xaULONG)ipc_buff, ipc_cmd.value, file_ret);
	    XA_Audio_Send_ACK(XA_IPC_ACK_ERR,ipc_cmd.id,0);
	    break;
	  }
	  AUD_DEBUG fprintf(stderr,"AUD FNAME: %s \n",ipc_buff);
	  
		/** Add/replace name in current audio header */
	  if (xa_aud_hdr_cur->filename) free(xa_aud_hdr_cur->filename);
	  xa_aud_hdr_cur->filename = (char *)ipc_buff;   ipc_buff = 0;
	  ipc_cmd.len = 0;  /* we've used it */
	} 
	XA_Audio_Send_ACK(XA_IPC_ACK_OK,ipc_cmd.id,0);
	break;

	/* Merges passed in file with previous file */
      case XA_IPC_MERGEFILE:
	AUD_DEBUG fprintf(stderr,"AUD IPC: rcvd MERGEFILE %d\n",ipc_cmd.value);
	{ XA_AUD_HDR *prev_hdr = xa_aud_hdr_cur;
	  XA_Child_Find_File(ipc_cmd.value,0);   /* POD add check for err */
		/* Free previous audio if any */
	  if (prev_hdr->first_snd) Free_SNDs(prev_hdr->first_snd);
	  if (prev_hdr->filename)  free(prev_hdr->filename);
		/* copy selected portions */
	  prev_hdr->filename	  = xa_aud_hdr_cur->filename;
	  prev_hdr->max_faud_size = xa_aud_hdr_cur->max_faud_size;
	  prev_hdr->first_snd	= xa_aud_hdr_cur->first_snd;
	  prev_hdr->last_snd	= xa_aud_hdr_cur->last_snd;
	  prev_hdr->init_aud	= xa_aud_hdr_cur->init_aud;
		/* Free up current one */
	  xa_aud_hdr_cur = Return_Aud_Hdr(xa_aud_hdr_cur);
	  XA_Audio_Send_ACK(XA_IPC_ACK_OK,ipc_cmd.id,0);
	}
	break;


      case XA_IPC_UNFILE:
	AUD_DEBUG fprintf(stderr,"AUD IPC: rcvd UNFILE %d\n",ipc_cmd.value);
	if ( XA_Child_Find_File(ipc_cmd.value,0) == xaNOFILE)
	{ XtRemoveInput(*id);
	  fprintf(stderr,"AUD IPC: UNFILE no such file %d\n",ipc_cmd.value);
	  XA_Audio_Send_ACK(XA_IPC_ACK_ERR,ipc_cmd.id,0); /* err and exit */
	  break;
	}
	/* Find_File set's xa_aud_hdr_cur correctly above - assume it's
         * the last one. */
	xa_aud_hdr_cur = Return_Aud_Hdr(xa_aud_hdr_cur);
	XA_Audio_Send_ACK(XA_IPC_ACK_OK,ipc_cmd.id,0);
	break;

      case XA_IPC_PLAY_FILE:
	{ xaULONG ok;
	  AUD_DEBUG fprintf(stderr,"AUD IPC: received PLAY_FILE\n");
	  if (xa_audio_present==XA_AUDIO_OK)
			ok = XA_Child_Find_File(ipc_cmd.value,0);
          if (ok == xaTRUE)
          {
	    if (xa_aud_fd>=0) { close(xa_aud_fd); xa_aud_fd = -1; }
	    if (xa_aud_hdr_cur->filename)
	    {
	      if ( (xa_aud_fd=open(xa_aud_hdr_cur->filename,O_RDONLY,NULL)) < 0)
	      {
	        fprintf(stderr,"AUD IPC: Open file %s for audio err\n",
				xa_aud_hdr_cur->filename);
	        XA_Audio_Send_ACK(XA_IPC_ACK_OK,ipc_cmd.id,xaFALSE);
	        break;
	      }
	    }
/*POD ?? Is buffer only used when playing from a file??? */
	    if ((xa_aud_hdr_cur->max_faud_size) && (xa_audcodec_buf==0))
	    { xa_audcodec_buf = (xaUBYTE *)malloc( xa_audcodec_maxsize );
	      if (xa_audcodec_buf==0) /* audio fatal */
	      { XtRemoveInput(*id);
		XA_Audio_Send_ACK(XA_IPC_ACK_ERR,ipc_cmd.id,xaFALSE);
		break;
	      }
	    }
	    if (xa_snd_cur->fpos >= 0)
	    {
	      xa_snd_cur->snd = xa_audcodec_buf;
	      XA_Read_Audio_Delta(xa_aud_fd,xa_snd_cur->fpos,
				xa_snd_cur->tot_bytes,xa_audcodec_buf);
	    }
          }
          XA_Audio_Send_ACK(XA_IPC_ACK_OK,ipc_cmd.id,ok);
	}
	break;

      case XA_IPC_N_FILE:
	{ xaULONG ok;
	  AUD_DEBUG fprintf(stderr,"AUD IPC: received N_FILE %d\n",ipc_cmd.value);
	  if (xa_audio_present==XA_AUDIO_OK)
			ok = XA_Child_Find_File(ipc_cmd.value,0);
	  else ok = xaNOFILE;
          XA_Audio_Send_ACK(XA_IPC_ACK_OK,ipc_cmd.id,ok);
	}
	break;

      case XA_IPC_P_FILE:
	{ xaULONG ok;
	  AUD_DEBUG fprintf(stderr,"AUD IPC: received P_FILE\n");
	  if (xa_audio_present==XA_AUDIO_OK)
			ok = XA_Child_Find_File(ipc_cmd.value,1);
	  else ok = xaNOFILE;
          XA_Audio_Send_ACK(XA_IPC_ACK_OK,ipc_cmd.id,ok);
	}
	break;

      case XA_IPC_SND_INIT:
	{ 
	  AUD_DEBUG fprintf(stderr,"AUD IPC: SND_INIT\n");
	  XA_Audio_Init_Snd(xa_snd_cur);
          XA_Audio_Send_ACK(XA_IPC_ACK_OK,ipc_cmd.id,0);
	}
	break;

      case XA_IPC_SND_ADD:
	{ XA_SND *new_snd = 0;
	  AUD_DEBUG fprintf(stderr,"AUD IPC: SND_ADD\n");
	  if (ipc_cmd.len)
	  { xaULONG sret;

	    if (ipc_cmd.value != xa_aud_hdr_cur->num) /* for different file */
            {
	      sret = XA_Child_Find_File(ipc_cmd.value,1);
	      if (sret == xaNOFILE)
		XA_Audio_Send_ACK(XA_IPC_ACK_OK,ipc_cmd.id,xaFALSE);
	    }

            new_snd = (XA_SND *)
			XA_Audio_Receive_Video_Buf(ipc_cmd.len,500);
            if (new_snd)	sret = XA_IPC_Sound(xa_aud_hdr_cur,new_snd);
	    else		sret = 0;
            ipc_cmd.len = 0;
            XA_Audio_Send_ACK(XA_IPC_ACK_OK,ipc_cmd.id,sret);
	  } 
	  else XA_Audio_Send_ACK(XA_IPC_ACK_OK,ipc_cmd.id,xaFALSE);

          /* SND_BUF MUST follow */
          XA_Audio_Receive_Video_CMD(&ipc_cmd, 5000);
          if (ipc_cmd.cmd != XA_IPC_SND_BUF)
		XA_Child_Dies1("SND_BUF Did NOT follow SND_ADD\n");
          AUD_DEBUG fprintf(stderr,"AUD IPC: SND_BUF\n");
          if (ipc_cmd.len)
          { xaULONG sret = xaFALSE;
	    if (new_snd) new_snd->snd = 
		(xaUBYTE *)XA_Audio_Receive_Video_Buf(ipc_cmd.len,500);
	    if (new_snd->snd) sret = xaTRUE;
            XA_Audio_Send_ACK(XA_IPC_ACK_OK,ipc_cmd.id,sret);
            ipc_cmd.len = 0;
          }
          else
	  {
	    new_snd->snd = 0;
            XA_Audio_Send_ACK(XA_IPC_ACK_OK,ipc_cmd.id,xaTRUE);
	  }
	}
	break;

      case XA_IPC_SET_AUDBUFF:
	AUD_DEBUG fprintf(stderr,"AUD IPC: SET_AUDBUFF\n");
        xa_aud_hdr_cur->max_faud_size = ipc_cmd.value;
        if (xa_aud_hdr_cur->max_faud_size > xa_audcodec_maxsize)
		xa_audcodec_maxsize = xa_aud_hdr_cur->max_faud_size;
	xa_av_time_off = ipc_cmd.value;
        XA_Audio_Send_ACK(XA_IPC_ACK_OK,ipc_cmd.id,0);
	break;

      case XA_IPC_SET_KLUDGE2:
	AUD_DEBUG fprintf(stderr,"AUD IPC: SET_KLUDGE2\n");
        xa_kludge2_dvi = ipc_cmd.value;
        XA_Audio_Send_ACK(XA_IPC_ACK_OK,ipc_cmd.id,0);
	break;

      case XA_IPC_SET_KLUDGE900:
	AUD_DEBUG fprintf(stderr,"AUD IPC: SET_KLUDGE900\n");
        xa_kludge900_aud = ipc_cmd.value;
        XA_Audio_Send_ACK(XA_IPC_ACK_OK,ipc_cmd.id,0);
	break;

      case XA_IPC_VID_TIME:
	AUD_DEBUG fprintf(stderr,"AUD IPC: VID_TIME\n");
	xa_av_time_off = ipc_cmd.value;
        XA_Audio_Send_ACK(XA_IPC_ACK_OK,ipc_cmd.id,0);
	break;

      case XA_IPC_RST_TIME:
	{ xaULONG tt = XA_Time_Read();
	  AUD_DEBUG fprintf(stderr,"AUD IPC: RST_TIME time %d\n",tt);
          XA_IPC_Reset_AV_Time(ipc_cmd.value);
	  tt = XA_Time_Read();
	  AUD_DEBUG fprintf(stderr,"AUD IPC: time end %d\n",tt);
          XA_Audio_Send_ACK(XA_IPC_ACK_OK,ipc_cmd.id,0);
	}
	break;

      case XA_IPC_AUD_ENABLE:
        AUD_DEBUG fprintf(stderr,"AUD IPC: AUD_ENABLE\n");
	audiof->enable = ipc_cmd.value;
        XA_Audio_Send_ACK(XA_IPC_ACK_OK,ipc_cmd.id,0);
	break;

      case XA_IPC_AUD_MUTE:
        AUD_DEBUG fprintf(stderr,"AUD IPC: AUD_MUTE\n");
	audiof->mute = ipc_cmd.value;
	audiof->newvol = xaTRUE;
        XA_Audio_Send_ACK(XA_IPC_ACK_OK,ipc_cmd.id,0);
	break;

      case XA_IPC_AUD_VOL:
        AUD_DEBUG fprintf(stderr,"AUD IPC: AUD_VOL\n");
	audiof->volume = ipc_cmd.value;
	audiof->newvol = xaTRUE;
        XA_Audio_Send_ACK(XA_IPC_ACK_OK,ipc_cmd.id,0);
	break;

      case XA_IPC_AUD_RATE:
        AUD_DEBUG fprintf(stderr,"AUD IPC: AUD_RATE\n");
	audiof->playrate = ipc_cmd.value;
        XA_Audio_Send_ACK(XA_IPC_ACK_OK,ipc_cmd.id,0);
	break;

      case XA_IPC_AUD_DEV:
        AUD_DEBUG fprintf(stderr,"AUD IPC: AUD_DEV\n");
        if (ipc_cmd.len) 
	{
	  ipc_buff =  XA_Audio_Receive_Video_Buf(ipc_cmd.len,500);
	  if (ipc_buff)
	  { AUD_DEBUG fprintf(stderr,"DEVICE: %s \n",ipc_buff);
	    audiof->device = (char *)(ipc_buff);
	    ipc_buff = 0;
	  }
	  else { AUD_DEBUG fprintf(stderr,"FILE: err\n"); }
	  ipc_cmd.len = 0; /* indicate we've read it */
	}
        XA_Audio_Send_ACK(XA_IPC_ACK_OK,ipc_cmd.id,0);
	break;

      case XA_IPC_AUD_FFLAG:
        AUD_DEBUG fprintf(stderr,"AUD IPC: AUD_FFLAG\n");
	audiof->fromfile = ipc_cmd.value;
        XA_Audio_Send_ACK(XA_IPC_ACK_OK,ipc_cmd.id,0);
	break;

      case XA_IPC_AUD_BFLAG:
        AUD_DEBUG fprintf(stderr,"AUD IPC: AUD_BFLAG\n");
	audiof->bufferit = ipc_cmd.value;
        XA_Audio_Send_ACK(XA_IPC_ACK_OK,ipc_cmd.id,0);
	break;

      case XA_IPC_ERR:
	XtRemoveInput(*id);
        XA_Audio_Send_ACK(XA_IPC_ACK_ERR,ipc_cmd.id,0); /* err and exit */
	break;

      case XA_IPC_HELLO:
	AUD_DEBUG fprintf(stderr,"AUD IPC: received Hello\n");
        XA_Audio_Send_ACK(XA_IPC_ACK_OK,ipc_cmd.id,0);
	break;

      case XA_IPC_EXIT:
	AUD_DEBUG fprintf(stderr,"AUD IPC: received EXIT\n");
	XtRemoveInput(*id);
        XA_Audio_Send_ACK(XA_IPC_ACK_BYE,ipc_cmd.id,0);
	XA_Child_Dies((int)(0));
	break;

      case XA_IPC_BOFL:
	AUD_DEBUG fprintf(stderr,"AUD IPC: received BOFL\n");
	/* No ACK needed */
	xa_child_last_time = XA_Time_Read();
	break;

      default:
	AUD_DEBUG fprintf(stderr,"AUD IPC: unknown cmd %d\n",ipc_cmd.cmd);
	XtRemoveInput(*id);
        XA_Audio_Send_ACK(XA_IPC_ACK_ERR,ipc_cmd.id,0); /* err and exit */
	break;
    }
    /* Flush len if no command uses it */
    if (ipc_cmd.len)
    {
	ipc_buff =  XA_Audio_Receive_Video_Buf(ipc_cmd.len,500);
        free(ipc_buff); ipc_buff = 0;
	XtRemoveInput(*id);
        XA_Audio_Send_ACK(XA_IPC_ACK_ERR,ipc_cmd.id,0); /* err and exit */
    } else ipc_buff = 0;
  } /* valid input */ 
}

/*********************************
 *  This routine sets up the xa_audio_fd and xa_video_fd pipes, then
 *  forks off the Audio Child process.
 *
 *   + The Parent returns(xaTRUE) on success, xaFALSE on failure.
 *   + The Child calls XA_Audio_Child().
 *
 ****************/
xaULONG XA_Give_Birth()
{ int ret;
#ifndef XA_SOCKET
  ret = pipe( xa_audio_fd );
  if (ret) { AUD_DEBUG fprintf(stderr,"PIPE for audio failed: %d\n",errno); }
  else
  {
    ret = pipe( xa_video_fd );
    if (ret) { AUD_DEBUG fprintf(stderr,"PIPE for video failed: %d\n",errno); }
    else
    {
#else
  {
    { int sv[2];
      ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
      if (ret)  { fprintf(stderr,"IPC socketpair failed %d\n",errno);
		  return(xaFALSE);
		}
      xa_audio_fd[0]	= sv[1];  /* audio reads  */
      xa_video_fd[1]	= sv[1];  /* audio writes */

      xa_video_fd[0]	= sv[0];  /* video reads  */
      xa_audio_fd[1]	= sv[0];  /* video writes */
#endif
      xa_audio_child = fork();
      if (xa_audio_child == 0)   /* I am the Audio Child */
      { 
	AUD_DEBUG fprintf(stderr,"I am the Child\n");
        XA_Audio_Child(); 
        exit(0);
      }
      else if (xa_audio_child < 0) /* Still Born */
      {
	AUD_DEBUG fprintf(stderr,"Audio Child Still Born: %d\n",errno);
      }
      else			/* I am the Video Parent */
      { 
	AUD_DEBUG fprintf(stderr,"I am the Parent(child %d)\n",xa_audio_child);
        return(xaTRUE); 
      }
    }
  }
  return(xaFALSE);
}

/*********************************
 * Move along Audio Header til num is matched. 
 * returns xaTRUE if found AND has valid snd.
 * returns xaNOFILE if no file has that num.
 * returns xaFALSE if file found, but no audio attached.
 * Else returns xaFALSE.
 *
 * Set's global variable xa_snd_cur
 ****************/
xaULONG XA_Child_Find_File(num,flag)
xaULONG num;
xaULONG flag;	/* 0 means next, 1 means prev */
{ XA_AUD_HDR *cur = xa_aud_hdr_cur;
  do
  {
AUD_DEBUG fprintf(stderr,"num %d cnum %d  cur %x n/p %x %x  hdr %x\n",
	num, cur->num, (xaULONG)cur, (xaULONG)cur->next, (xaULONG)cur->prev, 
	(xaULONG) xa_aud_hdr_cur);
    if (cur->num == num) 
    { 
      xa_aud_hdr_cur = cur; 
      xa_snd_cur = cur->first_snd;
      if (xa_snd_cur)		return(xaTRUE);
      else			return(xaFALSE);
    }
    cur = (flag == 0)?(cur->next):(cur->prev);
  } while(cur != xa_aud_hdr_cur);
  return(xaNOFILE);
}

/*********************************
 *
 ****************/
void XA_IPC_Reset_AV_Time(vid_time)
xaLONG vid_time;
{ int xflag = xaFALSE;

    if (xa_snd_cur==0) { AUD_DEBUG fprintf(stderr,"AA\n"); return; }
    XA_Audio_Init_Snd(xa_snd_cur);
    /* Move to the correct snd chunk */
    while(xflag == xaFALSE)
    { xaLONG snd_time = xa_snd_cur->snd_time;
      if (snd_time > vid_time)
      { XA_SND *p_snd = xa_snd_cur->prev;
        AUD_DEBUG fprintf(stderr,"s>v %d %d\n",snd_time,vid_time);
        if (p_snd) xa_snd_cur = p_snd;
        else xflag = xaTRUE;
      }
      else if (snd_time < vid_time)
      { XA_SND *n_snd = xa_snd_cur->next;
        AUD_DEBUG fprintf(stderr,"s<v %d %d\n",snd_time,vid_time);
        if (n_snd) 
        {
	  if (n_snd->snd_time <= vid_time) xa_snd_cur = n_snd;
	  else xflag = xaTRUE;
        }
        else xflag = xaTRUE;
      }
      else 
      {
        AUD_DEBUG fprintf(stderr,"s=v %d %d\n",snd_time,vid_time);
        xflag = xaTRUE;
      }
    } /* end while xflag */

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
      { xaULONG tmp_cnt; xaLONG diff;

	/* time diff in ms */
        diff =  (vid_time - shdr->snd_time); if (diff < 0) diff = 0;
	/* calc num of samples in that time frame */
	tmp_cnt = (diff * shdr->ifreq) / 1000; 
	if (tmp_cnt & 0x01) tmp_cnt--;  /* make multiple of 2 for ADPCM */
	/* Init snd_hdr */
        XA_Audio_Init_Snd(xa_snd_cur);
	if (tmp_cnt) /* not at beginning */
	{ char *garb; /* play sound into garb buffer */
	  garb = (char *)malloc(4 * tmp_cnt);
          if (garb) 
	  { diff = tmp_cnt - xa_snd_cur->delta(xa_snd_cur,garb,0,tmp_cnt);
	    free(garb);
	    if (diff != 0) fprintf(stderr,"AV Warn: rst sync err %x\n",diff);
	  }
	}
      }
      xa_time_audio = vid_time;
      xa_timelo_audio = 0;
    } /* end of valid xa_snd_cur */ 
}

void XA_IPC_Close_Pipes()
{
#ifdef XA_SOCKET
  if (xa_audio_fd[0] >= 0) { close(xa_audio_fd[0]); xa_audio_fd[0] = -1; }
  if (xa_audio_fd[1] >= 0) { close(xa_audio_fd[1]); xa_audio_fd[1] = -1; }
  xa_video_fd[0] = -1;
  xa_video_fd[1] = -1;
#else
  if (xa_audio_fd[0] >= 0) { close(xa_audio_fd[0]); xa_audio_fd[0] = -1; }
  if (xa_audio_fd[1] >= 0) { close(xa_audio_fd[1]); xa_audio_fd[1] = -1; }
  if (xa_video_fd[0] >= 0) { close(xa_video_fd[0]); xa_video_fd[0] = -1; }
  if (xa_video_fd[1] >= 0) { close(xa_video_fd[1]); xa_video_fd[1] = -1; }
#endif
}

void XA_IPC_Set_Debug(value)
xaULONG value;
{
  audio_debug_flag = value;
  xa_debug = value;
}

#else

/* prevents complaints from certain AR compilers */
void XA_IPC_DUMMY(c,a,b)
xaULONG *c,a,b;
{
  *c = a + b;
}
#endif

