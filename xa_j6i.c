
/*
 * xa_j6i.c
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

/*******************************
 * Revision
 *
 *
 ********************************/


#include "xa_j6i.h" 

xaULONG J6I_Read_File();
J6I_FRAME *J6I_Add_Frame();
void J6I_Free_Frame_List();
void ACT_Setup_Delta();
xaULONG J6I_Read_Header();
void J6I_Read_Image();
void J6I_Read_Audio();


/* CODEC ROUTINES */
xaULONG JFIF_Decode_JPEG();
void JFIF_Read_IJPG_Tables();
extern void XA_Gen_YUV_Tabs();
extern void JPG_Alloc_MCU_Bufs();
extern xaULONG jpg_search_marker();
extern char IJPG_Tab1[64];
extern char IJPG_Tab2[64];
extern void JFIF_Init_IJPG_Tables();
extern void g72x_init_state();

XA_ACTION *ACT_Get_Action();
XA_CHDR *ACT_Get_CMAP();
XA_CHDR *CMAP_Create_332();
XA_CHDR *CMAP_Create_422();
XA_CHDR *CMAP_Create_Gray();
void ACT_Add_CHDR_To_Action();
void ACT_Setup_Mapped();
void ACT_Get_CCMAP();
XA_CHDR *CMAP_Create_CHDR_From_True();
xaULONG CMAP_Find_Closest();
xaUBYTE *UTIL_RGB_To_FS_Map();
xaUBYTE *UTIL_RGB_To_Map();

void  UTIL_FPS_2_Time();
extern XA_ANIM_SETUP *XA_Get_Anim_Setup();
void XA_Free_Anim_Setup();


xaULONG j6i_audio_attempt;
xaULONG j6i_audio_type;
xaULONG j6i_audio_freq;
xaULONG j6i_audio_chans;
xaULONG j6i_audio_bps;
xaULONG j6i_audio_blocks;
xaULONG j6i_audio_samps;
xaULONG XA_Add_Sound();


xaULONG j6i_frame_cnt;
J6I_FRAME *j6i_frame_start,*j6i_frame_cur;

J6I_FRAME *J6I_Add_Frame(time,timelo,act)
xaULONG time,timelo;
XA_ACTION *act;
{
  J6I_FRAME *fframe;
 
  fframe = (J6I_FRAME *) malloc(sizeof(J6I_FRAME));
  if (fframe == 0) TheEnd1("J6I_Add_Frame: malloc err");
 
  fframe->time   = time;
  fframe->timelo = timelo;
  fframe->act = act;
  fframe->next = 0;
 
  if (j6i_frame_start == 0) j6i_frame_start = fframe;
  else j6i_frame_cur->next = fframe;
 
  j6i_frame_cur = fframe;
  j6i_frame_cnt++;
  return(fframe);
}

void J6I_Free_Frame_List(fframes)
J6I_FRAME *fframes;
{
  J6I_FRAME *ftmp;
  while(fframes != 0)
  {
    ftmp = fframes;
    fframes = fframes->next;
    FREE(ftmp,0xA000);
  }
}


xaULONG J6I_Read_File(fname,anim_hdr,audio_attempt)
char *fname;
XA_ANIM_HDR *anim_hdr;
xaULONG audio_attempt;	/* xaTRUE if audio is to be attempted */
{ XA_INPUT *xin = anim_hdr->xin;
  xaLONG i,t_time;
  xaULONG t_timelo;
  XA_ANIM_SETUP *j6i;
  J6I_HDR   j6i_hdr;
 
  j6i = XA_Get_Anim_Setup();
  j6i->vid_time = XA_GET_TIME( 100 ); /* default */

  j6i_frame_cnt		= 0;
  j6i_frame_start	= 0;
  j6i_frame_cur		= 0;
  j6i_audio_attempt	= audio_attempt;

  if (J6I_Read_Header(xin,anim_hdr,j6i,&j6i_hdr) == xaFALSE)
  {
    fprintf(stderr,"J6I: read header error\n");
    xin->Close_File(xin);
    return(xaFALSE);
  }

  J6I_Read_Image(xin,anim_hdr,j6i,&j6i_hdr);

#ifdef NOT_YET_EXP_UNK_FORMAT
  if (j6i_audio_attempt==xaTRUE)
	J6I_Read_Audio(xin,anim_hdr,j6i,&j6i_hdr);
#endif

  if (xa_verbose) 
  {
    fprintf(stdout,"J6I %dx%dx%d frames %d\n",
		j6i->imagex,j6i->imagey,j6i->imagec,j6i_frame_cnt);
  }
  if (j6i_frame_cnt == 0)
  { 
    fprintf(stderr,"J6I: No supported video frames exist in this file.\n");
    return(xaFALSE);
  }

  anim_hdr->frame_lst = (XA_FRAME *)
				malloc( sizeof(XA_FRAME) * (j6i_frame_cnt+1));
  if (anim_hdr->frame_lst == NULL) TheEnd1("J6I_Read_File: frame malloc err");

  j6i_frame_cur = j6i_frame_start;
  i = 0;
  t_time = 0;
  t_timelo = 0;
  while(j6i_frame_cur != 0)
  {
    if (i > j6i_frame_cnt)
    {
      fprintf(stderr,"J6I_Read_Anim: frame inconsistency %d %d\n",
                i,j6i_frame_cnt);
      break;
    }
    anim_hdr->frame_lst[i].time_dur = j6i_frame_cur->time;
    anim_hdr->frame_lst[i].zztime = t_time;
    t_time += j6i_frame_cur->time;
    t_timelo += j6i_frame_cur->timelo;
    while(t_timelo > (1<<24)) {t_time++; t_timelo -= (1<<24);}
    anim_hdr->frame_lst[i].act = j6i_frame_cur->act;
    j6i_frame_cur = j6i_frame_cur->next;
    i++;
  }
  anim_hdr->imagex = j6i->imagex;
  anim_hdr->imagey = j6i->imagey;
  anim_hdr->imagec = j6i->imagec;
  anim_hdr->imaged = 8; /* nop */
  anim_hdr->frame_lst[i].time_dur = 0;
  anim_hdr->frame_lst[i].zztime = -1;
  anim_hdr->frame_lst[i].act  = 0;
  anim_hdr->loop_frame = 0;
  if (!(xin->load_flag & XA_IN_LOAD_BUF)) anim_hdr->anim_flags |= ANIM_SNG_BUF;
  if (xin->load_flag & XA_IN_LOAD_FILE) anim_hdr->anim_flags |= ANIM_USE_FILE;
  anim_hdr->anim_flags |= ANIM_FULL_IM;
  anim_hdr->max_fvid_size = j6i->max_fvid_size;
  anim_hdr->max_faud_size = j6i->max_faud_size;
  anim_hdr->fname = anim_hdr->name;
  if (i > 0) 
  {
    anim_hdr->last_frame = i - 1;
    anim_hdr->total_time = anim_hdr->frame_lst[i-1].zztime
				+ anim_hdr->frame_lst[i-1].time_dur;
  }
  else
  {
    anim_hdr->last_frame = 0;
    anim_hdr->total_time = 0;
  }
  J6I_Free_Frame_List(j6i_frame_start);
  XA_Free_Anim_Setup(j6i);
  return(xaTRUE);
} /* end of read file */


xaULONG J6I_Read_Header(xin,anim_hdr,j6i,j6i_hdr)
XA_INPUT *xin;
XA_ANIM_HDR *anim_hdr;
XA_ANIM_SETUP *j6i;
J6I_HDR *j6i_hdr;
{ xaULONG	code, len, t;
  xaULONG	len_read;
  xaULONG	hdr_done;


  j6i_hdr->jpg_end = 0;
  len_read	= 0;
  hdr_done	= xaFALSE;
  while( !xin->At_EOF(xin,-1) && (hdr_done == xaFALSE))
  {
    code	= xin->Read_U8(xin);
    len		= xin->Read_U8(xin);
    len_read	+= 2 + len; 
	
    if (xa_verbose) fprintf(stdout," code %02x len %02x\n",code,len);

    switch( code )
    {
      case 0x80:
	/* DSCIM  VIDEO1 blah blah blah??? */
	if (len != 0x3e)  return( xaFALSE );		/* error or diff? */
	xin->Seek_FPos(xin,0x36,1);
	j6i_hdr->jpg_beg = xin->Read_MSB_U32(xin);	/* len of video */
	j6i_hdr->jpg_end = xin->Read_MSB_U32(xin);	/* len of video */
	break;

      case 0x81: /* Creation Date/Time */
	t		= xin->Read_MSB_U16(xin);	/* 0x0900 ?? */
	j6i_hdr->year	= xin->Read_U8(xin);
	j6i_hdr->month	= xin->Read_U8(xin);
	j6i_hdr->day	= xin->Read_U8(xin);
	j6i_hdr->hour	= xin->Read_U8(xin);
	j6i_hdr->min	= xin->Read_U8(xin);
	j6i_hdr->sec	= xin->Read_U8(xin);
        if (xa_verbose)
		fprintf(stdout,"date: %02x/%02x/%02x  time %02x:%02x:%02x\n",
			j6i_hdr->year, j6i_hdr->month, j6i_hdr->day,
			j6i_hdr->hour, j6i_hdr->min, j6i_hdr->sec);
	break;
			
      case 0x82:	/* Huh??? */
	xin->Seek_FPos(xin,len,1);
	break;

      case 0xFF:	/* Done */
	hdr_done = xaTRUE;
	break;

      default:
	fprintf(stderr,"J6I: unknown chunk %02x %02x\n",code,len);
	return(xaFALSE);
	break;
    }
  }

  j6i_hdr->hdr_len = len_read;
  if (j6i_hdr->jpg_end)
  { xaLONG diff = j6i_hdr->jpg_beg - j6i_hdr->hdr_len;
    if (diff < 0) return(xaFALSE);
    xin->Seek_FPos(xin,diff,1);
  }
  else return(xaFALSE);

  j6i->depth = 24;
  j6i->imagex = 768;
  j6i->imagey = 480;
  XA_Gen_YUV_Tabs(anim_hdr);
  JPG_Alloc_MCU_Bufs(anim_hdr,j6i->imagex,0,xaFALSE);

  if (   (cmap_true_map_flag == xaFALSE) /* depth 16 and not true_map */
      || (!(xin->load_flag & XA_IN_LOAD_BUF)) )
  {
     if (cmap_true_to_332 == xaTRUE)
             j6i->chdr = CMAP_Create_332(j6i->cmap,&j6i->imagec);
     else    j6i->chdr = CMAP_Create_Gray(j6i->cmap,&j6i->imagec);
  }
  if ( (j6i->pic==0) && (xin->load_flag & XA_IN_LOAD_BUF))
  {
    j6i->pic_size = j6i->imagex * j6i->imagey;
    if ( (cmap_true_map_flag == xaTRUE) && (j6i->depth > 8) )
                j6i->pic = (xaUBYTE *) malloc( 3 * j6i->pic_size );
    else j6i->pic = (xaUBYTE *) malloc( XA_PIC_SIZE(j6i->pic_size) );
    if (j6i->pic == 0) TheEnd1("J6I_Buffer_Action: malloc failed");
  }
  return(xaTRUE);
}


void J6I_Read_Image(xin,anim_hdr,j6i,j6i_hdr)
XA_INPUT	*xin;
XA_ANIM_HDR	*anim_hdr;
XA_ANIM_SETUP	*j6i;
J6I_HDR		*j6i_hdr;
{ xaULONG dlta_len;
  XA_ACTION *act;
  ACT_DLTA_HDR *dlta_hdr;

  dlta_len = j6i_hdr->jpg_end - j6i_hdr->jpg_beg + 1;

  fprintf(stderr,"J6I: JPEG  len %x beg %x end %x\n",
		dlta_len, j6i_hdr->jpg_beg, j6i_hdr->jpg_end);
 

  act = ACT_Get_Action(anim_hdr,ACT_DELTA);
  if (xin->load_flag & XA_IN_LOAD_FILE)
  {
    dlta_hdr = (ACT_DLTA_HDR *) malloc(sizeof(ACT_DLTA_HDR));
    if (dlta_hdr == 0) TheEnd1("J6I: dlta malloc err");
    act->data = (xaUBYTE *)dlta_hdr;
    dlta_hdr->flags = ACT_SNGL_BUF;
    dlta_hdr->fsize = dlta_len;
    dlta_hdr->fpos  = xin->Get_FPos(xin);
    if (dlta_len > j6i->max_fvid_size) j6i->max_fvid_size = dlta_len;
    xin->Seek_FPos(xin,dlta_len,1);
  }
  else
  { xaULONG d; xaLONG ret;
    d = dlta_len + (sizeof(ACT_DLTA_HDR));
    dlta_hdr = (ACT_DLTA_HDR *) malloc( d );
    if (dlta_hdr == 0) TheEnd1("J6I: malloc failed");
    act->data = (xaUBYTE *)dlta_hdr;
    dlta_hdr->flags = ACT_SNGL_BUF | DLTA_DATA;
    dlta_hdr->fpos = 0; dlta_hdr->fsize = dlta_len;
    ret = xin->Read_Block(xin, dlta_hdr->data, dlta_len);
    if (ret < dlta_len) { fprintf(stderr,"J6I: read err\n"); return; }
  }
  J6I_Add_Frame(j6i->vid_time,j6i->vid_timelo,act);
  dlta_hdr->xpos = dlta_hdr->ypos = 0;
  dlta_hdr->xsize = j6i->imagex;
  dlta_hdr->ysize = j6i->imagey;
  dlta_hdr->special = 0;
  dlta_hdr->extra = (void *)(0x0);
  dlta_hdr->xapi_rev = 0x0002;
  dlta_hdr->delta = JFIF_Decode_JPEG;
  ACT_Setup_Delta(j6i,act,dlta_hdr,xin);
}


#include "xa_g72x.h"

/****----------------------------------------------------------------****
 *
 ****----------------------------------------------------------------****/
void J6I_Read_Audio(xin,anim_hdr,j6i,j6i_hdr)
XA_INPUT	*xin;
XA_ANIM_HDR	*anim_hdr;
XA_ANIM_SETUP	*j6i;
J6I_HDR		*j6i_hdr;
{ xaLONG	skip;
  xaLONG	snd_size;
  xaULONG	aud_beg;
  xaLONG	ret;
  

  j6i_audio_bps	= 1;
  j6i_audio_chans = 1;
  j6i_audio_freq  = 8000;

  skip	  = j6i_hdr->jpg_end + 1;
  aud_beg = 0x400 * ((skip + 0x400 - 1) / 0x400);
  snd_size = 0x00030000 - aud_beg;

  skip = aud_beg - skip;

fprintf(stderr,"skip %x, beg %x, size %x\n", skip, aud_beg, snd_size);

	/* Move to start of Audio */
  xin->Seek_FPos(xin,skip,1);
  
  if (xa_file_flag == xaTRUE)
  { 
#ifdef NOT_YET
     xaLONG rets, cur_fpos = xin->Get_FPos(xin);

    rets = XA_Add_Sound(anim_hdr,0,j6i_audio_type, cur_fpos,
                        j6i_audio_freq, snd_size,
                        &j6i->aud_time,&j6i->aud_timelo, 
			j6i_audio_blocks, j6i_audio_samps);
    if (rets==xaFALSE) j6i_audio_attempt = xaFALSE;
    xin->Seek_FPos(xin,snd_size,1); /* move past this chunk */
    if (snd_size > j6i->max_faud_size)
                                j6i->max_faud_size = snd_size;
#endif
fprintf(stderr,"NOT YET\n");

  }
  else
  { xaUBYTE *snd = (xaUBYTE *)malloc(snd_size);
    if (snd==0) TheEnd1("J6I: snd malloc err");
    ret = xin->Read_Block(xin, snd, snd_size);
fprintf(stderr,"ret %x snd_size %x\n",ret, snd_size);
    if (ret != snd_size) fprintf(stderr,"J6I: snd rd err\n");
    else
    { int i, rets;


#ifdef IF_PCM
  j6i_audio_bps	= 1;
  j6i_audio_chans = 1;
  j6i_audio_freq  = 8000;
  j6i_audio_blocks = 1;
  j6i_audio_samps = 1;
  j6i_audio_type  = XA_AUDIO_SIGNED_1M;

      rets = XA_Add_Sound(anim_hdr, snd, j6i_audio_type, -1,
			j6i_audio_freq, snd_size,
			&j6i->aud_time, &j6i->aud_timelo,
			j6i_audio_blocks, j6i_audio_samps);
      if (rets==xaFALSE) j6i_audio_attempt = xaFALSE;
    }
#endif

#ifdef IF_G721 
#endif
      xaUBYTE *dp,*dsnd = (xaUBYTE *)malloc(4 * snd_size);
      struct g72x_state       state;

      if (dsnd==0) TheEnd1("J6I: dsnd malloc err");

      dp = dsnd;
      g72x_init_state(&state);
      for(i=0; i<snd_size; i++)
      { int samp,d = snd[i];

/*
fprintf(stderr,"d %02x ",d);
*/

        samp = g721_decoder( (d>>4), AUDIO_ENCODING_LINEAR, &state);

/*
fprintf(stderr,"s0 %04x ",samp);
*/
        *dp++ = (samp>>8) & 0xff;
        *dp++ =  samp & 0xff;

        samp = g721_decoder(  d    , AUDIO_ENCODING_LINEAR, &state);
/*
fprintf(stderr,"s1 %04x\n",samp);
*/
        *dp++ = (samp>>8) & 0xff;
        *dp++ =  samp & 0xff;
      }

{ FILE *fout;

  fout = fopen("dump.out","w");
  if (fout != NULL)
  { int i = snd_size;
    for(i=0; i < snd_size; i+=2)
    { int d =  dsnd[i] ^ 0x80;
      fputc(d, fout);
    }
    fclose(fout);
  }

  fout = fopen("dump.raw","w");
  if (fout != NULL)
  {
    fwrite(snd,snd_size,1,fout);
    fclose(fout);
  }
}

  j6i_audio_bps	= 2;
  j6i_audio_chans = 1;
  j6i_audio_freq  = 8000;
  j6i_audio_blocks = 2;
  j6i_audio_samps = 1;
  j6i_audio_type  = XA_AUDIO_SIGNED_2MB;

      rets = XA_Add_Sound(anim_hdr, dsnd, j6i_audio_type, -1,
			j6i_audio_freq, (4 * snd_size),
			&j6i->aud_time, &j6i->aud_timelo,
			j6i_audio_blocks, j6i_audio_samps);
      if (rets==xaFALSE) j6i_audio_attempt = xaFALSE;
    }

  anim_hdr->total_time =  (xaULONG)
        (((float)snd_size * 1000.0)
                / (float)(j6i_audio_chans * j6i_audio_bps) )
                / (float)(j6i_audio_freq);


  }
}


