
/*
 * xa_acodec.c
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


/* This file contains Audio Codecs and the routines to add audio
 * into the XAnim structures.  The Hardware specific audio routines
 * for the various platforms is somewhere else.
 */

#include "xa_audio.h"

/* external */
XA_SND *XA_Audio_Next_Snd();
extern xaULONG xa_kludge2_dvi;

/* externals to make internal */
extern xaUBYTE xa_sign_2_ulaw[];
extern xaULONG xa_ulaw_2_sign[];
extern xaULONG xa_arm_2_signed[];
extern xaULONG xa_alaw_2_sign[];
extern void Gen_uLaw_2_Signed();
extern void Gen_aLaw_2_Signed();
extern void Gen_Arm_2_Signed();
extern void Gen_ALaw_2_Signed();

/* Internal */
xaULONG XA_ADecode_1M_1M();
xaULONG XA_ADecode_PCMXM_PCM1M();
xaULONG XA_ADecode_PCM1M_PCM2M();
xaULONG XA_ADecode_PCM1S_PCMxM();
xaULONG XA_ADecode_PCM2X_PCM2M();
xaULONG XA_ADecode_NOP_PCMXM();
xaULONG XA_ADecode_ULAWx_PCMxM();
xaULONG XA_ADecode_ALAWx_PCMxM();
xaULONG XA_ADecode_ARMLAWx_PCMxM();
xaULONG XA_ADecode_ADPCMS_PCM2M();
xaULONG XA_ADecode_ADPCMM_PCM2M();
xaULONG XA_ADecode_DVIM_PCMxM();
xaULONG XA_ADecode_DVIS_PCMxM();
xaULONG XA_ADecode_IMA4M_PCMxM();
xaULONG XA_ADecode_IMA4S_PCMxM();
#ifdef XA_GSM
extern xaULONG XA_ADecode_GSMM_PCMxM();
extern void XA_MSGSM_Decoder();
extern void XA_GSM_Decoder();
#endif

xaULONG XA_IPC_Sound();
xaULONG XA_Add_Sound();
extern xaULONG xa_vaudio_hard_buff;
extern xaULONG xa_audio_hard_type;


/***************************************************************************
 * Below is an Empty Audio Decode Routine. It's mainly for my benefit
 * to cut and paste for new ones.
 */ 

#ifdef XA_NEVER_DEFINED_32
xaULONG XA_ADecode_XXX_XXX(snd_hdr,obuf,ocnt,buff_size)
XA_SND *snd_hdr;
xaUBYTE *obuf;
xaULONG ocnt,buff_size;
{ XA_ADECODE_DECLARE_LOCAL; 
  if (snd_hdr==0) return(ocnt);
  XA_ADECODE_INIT_LOCAL;	/* Init Local Variables */
  /* Optionally Init Codec Specific Structure Here */ 
  while(ocnt < buff_size)
  {
    if (inc_cnt < (1<<24))
    {	
	/*** Decode Sample and increment byte_cnt ***/
      byte_cnt++;

      samp_cnt--;
      inc_cnt += inc;
    }
    while(inc_cnt >= (1<<24))
    { 
	/*** Output Sample ***/
      ocnt++;
      inc_cnt -= (1<<24);	
      if (ocnt >= buff_size) break;
    }
    if (samp_cnt <= 0)		{ XA_ADECODE_MOVE_ON; }
  }
  XA_ADECODE_SAVE_LOCAL;     /* save local variables */
  /* Optionally Save Codec Specific Structure Here */
  return(ocnt);
}
#endif


/*****************************************************************************/

#define XA_ADECODE_DECLARE_LOCAL xaUBYTE *ibuf; \
xaULONG byte_cnt, inc, inc_cnt, spec, samp_cnt; xaLONG dataL, dataR

#define XA_ADECODE_INIT_LOCAL 		\
{ spec		= snd_hdr->spec;	\
  inc		= snd_hdr->inc;		\
  inc_cnt	= snd_hdr->inc_cnt;	\
  byte_cnt	= snd_hdr->byte_cnt;	\
  samp_cnt	= snd_hdr->samp_cnt;	\
  dataL		= snd_hdr->dataL;	\
  dataR		= snd_hdr->dataR;	\
  ibuf		= snd_hdr->snd;		\
  ibuf += byte_cnt; 			}

#define XA_ADECODE_MOVE_ON 	{			\
if ( (snd_hdr = XA_Audio_Next_Snd(snd_hdr)) != 0)	\
{ snd_hdr->inc_cnt	= inc_cnt;			\
  snd_hdr->dataL	= dataL;			\
  snd_hdr->dataR	= dataL;			\
  ocnt = snd_hdr->delta(snd_hdr,obuf,ocnt,buff_size);	\
}							\
return(ocnt);						}		

#define XA_ADECODE_SAVE_LOCAL	\
{ snd_hdr->inc_cnt	= inc_cnt;	\
  snd_hdr->byte_cnt	= byte_cnt;	\
  snd_hdr->samp_cnt	= samp_cnt;	\
  snd_hdr->dataL	= dataL;	\
  snd_hdr->dataR	= dataL;	}

/********** XA_ADecode_1M_M *********************************
 * No conversion. simply pass along.
 *******************************************/
xaULONG XA_ADecode_1M_1M(snd_hdr,obuf,ocnt,buff_size)
XA_SND *snd_hdr;
xaUBYTE *obuf;
xaULONG ocnt,buff_size;
{ XA_ADECODE_DECLARE_LOCAL; 
  if (snd_hdr==0) return(ocnt);
  XA_ADECODE_INIT_LOCAL;	/* Init Local Variables */
  while(ocnt < buff_size)
  {
    if (inc_cnt < (1<<24))
    { dataL = dataR = (xaULONG)*ibuf++;		byte_cnt++;
      samp_cnt--;
      inc_cnt += inc;
    }
    while(inc_cnt >= (1<<24))
    { *obuf++ = dataL;
      ocnt++;
      inc_cnt -= (1<<24);	
      if (ocnt >= buff_size) break;
    }
    if (samp_cnt <= 0)		{ XA_ADECODE_MOVE_ON; }
  }
  XA_ADECODE_SAVE_LOCAL;     /* save local variables */
  return(ocnt);
}

/********** XA_ADecode_PCMXM_PCM1M *********************************
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
xaULONG XA_ADecode_PCMXM_PCM1M(snd_hdr,obuf,ocnt,buff_size)
XA_SND *snd_hdr;
xaUBYTE *obuf;
xaULONG ocnt,buff_size;
{ XA_ADECODE_DECLARE_LOCAL;	xaULONG bps;
  if (snd_hdr==0) return(ocnt);
  XA_ADECODE_INIT_LOCAL; 
  bps		= ((spec & 2) | (spec & 4))?(2):(1);

  while(ocnt < buff_size)
  {
    if (inc_cnt < (1<<24))
    {	/*** Decode Sample ***/
      dataL = (spec & 4)?(ibuf[1]):(*ibuf);	
      ibuf += bps;	byte_cnt += bps;	samp_cnt--;
      inc_cnt += inc;
    }

    while(inc_cnt >= (1<<24))
    {
	/*** Output Sample ***/
      if (spec & 8)
      { /* note: ulaw takes signed input */
	if (spec & 1) *obuf++ = xa_sign_2_ulaw[ dataL ];
	else *obuf++ = xa_sign_2_ulaw[ (dataL ^ 0x80) ];
      }
      else *obuf++ = (spec & 1)?(dataL^0x80):(dataL);

      ocnt++;
      inc_cnt -= (1<<24);	
      if (ocnt >= buff_size) break;
    }
    if (samp_cnt <= 0)		{ XA_ADECODE_MOVE_ON; }
  }
  XA_ADECODE_SAVE_LOCAL;
  return(ocnt);
}

/********** XA_ADecode_PCM1M_PCM2M *********************************
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
 *   xaUBYTE xa_sign_2_ulaw[256]	conversion table.
 *   XA_Audio_Next_Snd()	routine to move to next sound header.
 ***************************************************************/
xaULONG XA_ADecode_PCM1M_PCM2M(snd_hdr,obuf,ocnt,buff_size)
XA_SND *snd_hdr;
xaUBYTE *obuf;
xaULONG ocnt,buff_size;
{ XA_ADECODE_DECLARE_LOCAL;
  if (snd_hdr==0) return(ocnt);
  XA_ADECODE_INIT_LOCAL;

  while(ocnt < buff_size)
  {
    if (inc_cnt < (1<<24))
    {   /*** Decode Sample ***/
      dataL = *ibuf++;
      byte_cnt++;        samp_cnt--;
      inc_cnt += inc;
    }
    while(inc_cnt >= (1<<24))
    {  /*** Output Sample ****/
      if (spec==1) { *obuf++ = dataL ^ 0x80; *obuf++ = dataL; }
      else if (spec==2) { *obuf++ = dataL; *obuf++ = dataL ^ 0x80; }
      else { *obuf++ = dataL; *obuf++ = dataL; }
      ocnt++;
      inc_cnt -= (1<<24);
      if (ocnt >= buff_size) break;
    }
    if (samp_cnt <= 0)	{ XA_ADECODE_MOVE_ON; }
  }
  XA_ADECODE_SAVE_LOCAL;
  return(ocnt);
}

/********** XA_Audio_PCM1S_PCMxM *********************************
 * Convert PCM 1 BPS Stereo Samples into PCM 2 BPS Mono Samples
 * The order flag takes care of the various linear/signed/endian
 * conversions
 *  Input        Ouput       Order  1st  2nd
 *  -----------------------------------------
 *  Linear to Linear Big       0    D     D
 *  Linear to Linear Little    0    D     D
 *  Signed to Linear Big       1 5  D^80  D
 *  Signed to Linear Little    2 6  D     D^80
 *  Linear to Signed Big       1    D^80  D
 *  Linear to Signed Little    2    D     D^80
 *  Signed to Signed Big       2 6  D     D^80
 *  Signed to Signed Little    1 5  D^80  D
 *
 *  bit 2(0x04) indicates incoming is signed(necessary for proper averaging)
 *  bit 3(0x08) indicates output is single byte
 *  bit 4(0x10) indicates output is ulaw(X to signed to ulaw)
 *
 * Global Variables:
 *   xaUBYTE xa_sign_2_ulaw[256]        conversion table.
 *   XA_Audio_Next_Snd()        routine to move to next sound header.
 ***************************************************************/
xaULONG XA_ADecode_PCM1S_PCMxM(snd_hdr,obuf,ocnt,buff_size)
XA_SND *snd_hdr;
xaUBYTE *obuf;
xaULONG ocnt,buff_size;
{ XA_ADECODE_DECLARE_LOCAL; 
  if (snd_hdr==0) return(ocnt);
  XA_ADECODE_INIT_LOCAL;
  while(ocnt < buff_size)
  { if (inc_cnt < (1<<24))
    { /*** Decode Sample and increment byte_cnt ***/
      dataL = *ibuf++;
      dataR = *ibuf++;	
      if (spec & 0x04)	/* Signed Input */
      { if (dataL & 0x80) dataL -= 0x100;
        if (dataR & 0x80) dataR -= 0x100;
      }
      byte_cnt += 2;
      samp_cnt--;
      inc_cnt += inc;
    }
    while(inc_cnt >= (1<<24))
    { xaULONG data = ((dataL + dataR)>>1) & 0xff;
	/*** Output Sample ***/
      if (spec & 0x08) /* Single Byte out */
      { if (spec & 0x10)	
		*obuf++ = xa_sign_2_ulaw[((spec & 0x04)?(data):(data ^ 0x80))];
        else	*obuf++ = (spec & 0x01)?(data ^ 0x80):(data);
      }
      else
      {
	if (spec & 0x01)	{ *obuf++ = data ^ 0x80;   *obuf++ = data; }
	else if (spec & 0x02)	{ *obuf++ = data;   *obuf++ = data ^ 0x80; }
	else			{ *obuf++ = data;        *obuf++ = data; }
      }
/* POD Add uLaw and single byte output */

      ocnt++;
      inc_cnt -= (1<<24);	
      if (ocnt >= buff_size) break;
    }
    if (samp_cnt <= 0)		{ XA_ADECODE_MOVE_ON; }
  }
  XA_ADECODE_SAVE_LOCAL;
  return(ocnt);
}

/********** XA_ADecode_ULAWx_PCMxM *********************************
 * Convert Sun's ULAW Mono/Stereo Samples into PCM 1 or 2 BPS Mono Samples
 * The spec flag takes care of the various linear/signed/endian
 * conversions
 *  Input        Ouput      Spec  1st  2nd
 *  -----------------------------------------
 *  ULAW to Signed Big       0    D1    D0
 *  ULAW to Signed Little    1    D0    D1
 *  ULAW to Linear Big       2    D1^80 D0
 *  ULAW to Linear Little    3    D0    D1^80
 *
 *  bit 2 (& 0x04) 1 byte output.
 *  bit 3 (& 0x08) stereo input.
 *
 * Global Variables:
 *   xaUBYTE xa_sign_2_ulaw[256]        conversion table.
 *   XA_Audio_Next_Snd()        routine to move to next sound header.
 ***************************************************************/
xaULONG XA_ADecode_ULAWx_PCMxM(snd_hdr,obuf,ocnt,buff_size)
XA_SND *snd_hdr;
xaUBYTE *obuf;
xaULONG ocnt,buff_size;
{ XA_ADECODE_DECLARE_LOCAL; 
  if (snd_hdr==0) return(ocnt);
  XA_ADECODE_INIT_LOCAL;

  while(ocnt < buff_size)
  {
    if (inc_cnt < (1<<24))
    { /*** Decode Sample and increment byte_cnt ***/
      dataL = xa_ulaw_2_sign[ (*ibuf++) ];
      if (spec & 0x08)
      { dataR = xa_ulaw_2_sign[ (*ibuf++) ];
	if (dataL & 0x8000) dataL -= 0x10000;
	if (dataR & 0x8000) dataR -= 0x10000;
        byte_cnt += 2;
      } else byte_cnt++;
      samp_cnt--;
      inc_cnt += inc;
    }
    while(inc_cnt >= (1<<24))
    { xaULONG data = (spec & 0x08)?((dataL + dataR)>>1):(dataL);
	/*** Output Sample ***/
      if (spec & 0x04)  /* 1 byte output */
      {  xaUBYTE d0 = ((xaULONG)(data) >> 8) & 0xff;
         if (spec & 0x02)    *obuf++ = d0;
         else             *obuf++ = d0 ^ 0x80;
/* POD Add uLaw output */
      }
      else
      { xaUBYTE d1,d0; d1 = (data>>8) & 0xff; d0 = data & 0xff;
        if (spec & 0x02) d1 ^= 0x80;
        if (spec & 0x01) {*obuf++ = d0; *obuf++ = d1; }
        else {*obuf++ = d1; *obuf++ = d0; }
      }

      ocnt++;
      inc_cnt -= (1<<24);	
      if (ocnt >= buff_size) break;
    }
    if (samp_cnt <= 0)		{ XA_ADECODE_MOVE_ON; }
  }
  XA_ADECODE_SAVE_LOCAL;
  return(ocnt);
}

/********** XA_ADecode_ALAWx_PCMxM *********************************
 * Convert aLAW Mono/Stereo Samples into PCM 1 or 2 BPS Mono Samples
 * The spec flag takes care of the various linear/signed/endian
 * conversions
 *  Input        Ouput      Spec  1st  2nd
 *  -----------------------------------------
 *  ULAW to Signed Big       0    D1    D0
 *  ULAW to Signed Little    1    D0    D1
 *  ULAW to Linear Big       2    D1^80 D0
 *  ULAW to Linear Little    3    D0    D1^80
 *
 *  bit 2 (& 0x04) 1 byte output.
 *  bit 3 (& 0x08) stereo input.
 *
 * Global Variables:
 *   xaUBYTE xa_sign_2_ulaw[256]        conversion table.
 *   XA_Audio_Next_Snd()        routine to move to next sound header.
 ***************************************************************/
xaULONG XA_ADecode_ALAWx_PCMxM(snd_hdr,obuf,ocnt,buff_size)
XA_SND *snd_hdr;
xaUBYTE *obuf;
xaULONG ocnt,buff_size;
{ XA_ADECODE_DECLARE_LOCAL; 
  if (snd_hdr==0) return(ocnt);
  XA_ADECODE_INIT_LOCAL;

  while(ocnt < buff_size)
  {
    if (inc_cnt < (1<<24))
    { /*** Decode Sample and increment byte_cnt ***/
      dataL = xa_alaw_2_sign[ (*ibuf++) ];
      if (spec & 0x08)
      { dataR = xa_alaw_2_sign[ (*ibuf++) ];
	if (dataL & 0x8000) dataL -= 0x10000;
	if (dataR & 0x8000) dataR -= 0x10000;
        byte_cnt += 2;
      } else byte_cnt++;
      samp_cnt--;
      inc_cnt += inc;
    }
    while(inc_cnt >= (1<<24))
    { xaULONG data = (spec & 0x08)?((dataL + dataR)>>1):(dataL);
	/*** Output Sample ***/
      if (spec & 0x04)  /* 1 byte output */
      {  xaUBYTE d0 = ((xaULONG)(data) >> 8) & 0xff;
         if (spec & 0x02)    *obuf++ = d0;
         else             *obuf++ = d0 ^ 0x80;
/* POD Add uLaw output */
      }
      else
      { xaUBYTE d1,d0; d1 = (data>>8) & 0xff; d0 = data & 0xff;
        if (spec & 0x02) d1 ^= 0x80;
        if (spec & 0x01) {*obuf++ = d0; *obuf++ = d1; }
        else {*obuf++ = d1; *obuf++ = d0; }
      }

      ocnt++;
      inc_cnt -= (1<<24);	
      if (ocnt >= buff_size) break;
    }
    if (samp_cnt <= 0)		{ XA_ADECODE_MOVE_ON; }
  }
  XA_ADECODE_SAVE_LOCAL;
  return(ocnt);
}

/********** XA_ADecode_ARMLAWx_PCMxM *********************************
 * Convert ARM Logarithmic Mono/Stereo Samples into PCM 1 or 2 BPS Mono Samples
 * The spec flag takes care of the various linear/signed/endian
 * conversions
 *  Input        Ouput      Spec  1st  2nd
 *  -----------------------------------------
 *  ULAW to Signed Big       0    D1    D0
 *  ULAW to Signed Little    1    D0    D1
 *  ULAW to Linear Big       2    D1^80 D0
 *  ULAW to Linear Little    3    D0    D1^80
 *
 *  bit 2 (& 0x04) 1 byte output.
 *  bit 3 (& 0x08) stereo input.
 *
 * Global Variables:
 *   xaUBYTE xa_sign_2_ulaw[256]        conversion table.
 *   XA_Audio_Next_Snd()        routine to move to next sound header.
 ***************************************************************/
xaULONG XA_ADecode_ARMLAWx_PCMxM(snd_hdr,obuf,ocnt,buff_size)
XA_SND *snd_hdr;
xaUBYTE *obuf;
xaULONG ocnt,buff_size;
{ XA_ADECODE_DECLARE_LOCAL; 
  if (snd_hdr==0) return(ocnt);
  XA_ADECODE_INIT_LOCAL;

  while(ocnt < buff_size)
  {
    if (inc_cnt < (1<<24))
    { /*** Decode Sample and increment byte_cnt ***/
      dataL = xa_arm_2_signed[ (*ibuf++) ];
      if (spec & 0x08)
      { dataR = xa_arm_2_signed[ (*ibuf++) ];
	if (dataL & 0x8000) dataL -= 0x10000;
	if (dataR & 0x8000) dataR -= 0x10000;
        byte_cnt += 2;
      } else byte_cnt++;
      samp_cnt--;
      inc_cnt += inc;
    }
    while(inc_cnt >= (1<<24))
    { xaULONG data = (spec & 0x08)?((dataL + dataR)>>1):(dataL);
	/*** Output Sample ***/
      if (spec & 0x04)  /* 1 byte output */
      {  xaUBYTE d0 = ((xaULONG)(data) >> 8) & 0xff;
         if (spec & 0x02)    *obuf++ = d0;
         else             *obuf++ = d0 ^ 0x80;
/* POD Add uLaw output */
      }
      else
      { xaUBYTE d1,d0; d1 = (data>>8) & 0xff; d0 = data & 0xff;
        if (spec & 0x02) d1 ^= 0x80;
        if (spec & 0x01) {*obuf++ = d0; *obuf++ = d1; }
        else {*obuf++ = d1; *obuf++ = d0; }
      }

      ocnt++;
      inc_cnt -= (1<<24);	
      if (ocnt >= buff_size) break;
    }
    if (samp_cnt <= 0)		{ XA_ADECODE_MOVE_ON; }
  }
  XA_ADECODE_SAVE_LOCAL;
  return(ocnt);
}

/********** XA_ADecode_PCM2X_PCM2M *********************************
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
 *   XA_Audio_Next_Snd()        routine to move to next sound header.
 ***************************************************************/
xaULONG XA_ADecode_PCM2X_PCM2M(snd_hdr,obuf,ocnt,buff_size)
XA_SND *snd_hdr;
xaUBYTE *obuf;
xaULONG ocnt,buff_size;
{ XA_ADECODE_DECLARE_LOCAL; 
  if (snd_hdr==0) return(ocnt);
  XA_ADECODE_INIT_LOCAL;

  while(ocnt < buff_size)
  {
    if (inc_cnt < (1<<24))
    {	/*** Decode Sample and increment byte_cnt ***/
      if (spec & 1)   /* Little Endian Samples */
      { dataL = *ibuf++;  dataL |= *ibuf++ << 8;
	if (spec & 8)	/* stereo input */
        { dataR = *ibuf++;  dataR |= *ibuf++ << 8; byte_cnt += 4;
	  if (spec & 0x10)  /* signed stereo input */
	  { if (dataL & 0x8000) dataL -= 0x10000;
	    if (dataR & 0x8000) dataR -= 0x10000;
	  }
	} else byte_cnt += 2;
      }
      else /* Big Endian Samples */
      { dataL = *ibuf++ << 8;  dataL |= *ibuf++;
	if (spec & 8) /* stereo input */
        { dataR = *ibuf++ << 8;  dataR |= *ibuf++; byte_cnt += 4;
	  if (spec & 0x10)  /* signed stereo input */
	  { if (dataL & 0x8000) dataL -= 0x10000;
	    if (dataR & 0x8000) dataR -= 0x10000;
	  }
	} else byte_cnt += 2;
      }
      samp_cnt--;
      inc_cnt += inc;
    }
    while(inc_cnt >= (1<<24))
    { register xaULONG d1,d0 = (spec & 8)?((dataL+dataR)>>1):(dataL);

	/*** Output Sample ***/
      if (spec & 2)	d0 ^= 0x8000; /* sign conversion */

      if (spec & 4)	d1 = d0 >> 8;
      else		{ d1 = d0; d0 >>= 8; }
      *obuf++ = (d0 & 0xff);
      *obuf++ = (d1 & 0xff);
	/* POD add single byte and ulaw output */
      ocnt++;
      inc_cnt -= (1<<24);	
      if (ocnt >= buff_size) break;
    }
    if (samp_cnt <= 0)		{ XA_ADECODE_MOVE_ON; }
  }
  XA_ADECODE_SAVE_LOCAL;
  return(ocnt);
}

/********** XA_ADecode_PCMXS_PCM1M *********************************
 * Convert PCM 1+2 BPS Stereo Samples into PCM 1 BPS Mono Samples
 * The spec flag takes care of the various linear/signed/endian
 * conversions
 *  Input        flag 1st  2nd    3rd  4th
 *  -----------------------------------------
 *  Linear1S      0   D     D     -    -
 *  Signed1S      1   D     D     -    -      ^80
 *  Linear2SBig   2   D     skip  D    skip
 *  Signed2SBig   3   D     skip  D    skip   ^80
 *  Linear2SLit   4   skip  D     skip D
 *  Signed2SLit   5   skip  D     skip D      ^80
 *
 *  bit 3 (& 0x08) AU output instead of PCM.
 *
 * Global Variables:
 *   XA_Audio_Next_Snd()        routine to move to next sound header.
 ***************************************************************/
xaULONG XA_ADecode_PCMXS_PCM1M(snd_hdr,obuf,ocnt,buff_size)
XA_SND *snd_hdr;
xaUBYTE *obuf;
xaULONG ocnt,buff_size;
{ XA_ADECODE_DECLARE_LOCAL; 
  if (snd_hdr==0) return(ocnt);
  XA_ADECODE_INIT_LOCAL;
  while(ocnt < buff_size)
  {
    if (inc_cnt < (1<<24))
    {	/*** Decode Sample and increment byte_cnt ***/

      if (spec & 2)      /* only read most sig byte */
	{ dataL = *ibuf++; ibuf++; dataR = *ibuf++; ibuf++; byte_cnt += 4; }
      else if (spec & 4) 
	{ ibuf++; dataL = *ibuf++; ibuf++; dataR = *ibuf++; byte_cnt += 4; }
      else		 { dataL = *ibuf++; dataR = *ibuf++; byte_cnt += 2; }
      if (spec & 1) /* signed input */
      { if (dataL & 0x80) dataL -= 0x100;
        if (dataR & 0x80) dataR -= 0x100;
      }
      samp_cnt--;
      inc_cnt += inc;
    }
    while(inc_cnt >= (1<<24))
    { xaULONG data = ((dataL + dataR)>>1) & 0xff;
      if (spec & 8)
      { /* note: ulaw takes signed input */
	if (spec & 1)	*obuf++ = xa_sign_2_ulaw[ data ];
	else		*obuf++ = xa_sign_2_ulaw[ (data ^ 0x80) ];
      }
      else *obuf++ = (spec & 1)?(data^0x80):(data);
      ocnt++;
      inc_cnt -= (1<<24);	
      if (ocnt >= buff_size) break;
    }
    if (samp_cnt <= 0)		{ XA_ADECODE_MOVE_ON; }
  }
  XA_ADECODE_SAVE_LOCAL;
  return(ocnt);
}

/* NEW CODECS HERE */


/********** XA_ADecode_NOP_PCMXM *********************************
 * Convert Silence(16bit Signed PCM silence 0x8000) Samples into various
 * other PCM/uLAW samples.
 * The spec flag takes care of the various linear/signed/endian
 * conversions
 *
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
 *   XA_Audio_Next_Snd()        routine to move to next sound header.
 ***************************************************************/
xaULONG XA_ADecode_NOP_PCMXM(snd_hdr,obuf,ocnt,buff_size)
XA_SND *snd_hdr;
xaUBYTE *obuf;
xaULONG ocnt,buff_size;
{ XA_ADECODE_DECLARE_LOCAL; 
  if (snd_hdr==0) return(ocnt);
  XA_ADECODE_INIT_LOCAL; 

  while(ocnt < buff_size)
  {
    if (inc_cnt < (1<<24))
    {	/*** Decode Sample ***/
      byte_cnt++;	samp_cnt--;
      inc_cnt += inc;
    }

    while(inc_cnt >= (1<<24))
    { xaULONG data = 0x8000;
	/*** Output Sample ***/
      if (spec & 4)  /* 1 byte output */
      { xaUBYTE d0 = ((xaULONG)(data) >> 8) & 0xff;
        if (spec & 8)    *obuf++ = xa_sign_2_ulaw[ (xaULONG)(d0) ];
	else             *obuf++ = (spec & 2)?(d0 ^ 0x80):(d0);
      }
      else
      { xaUBYTE d1,d0; d1 = (data>>8) & 0xff; d0 = data & 0xff;
        if (spec & 0x02) d1 ^= 0x80;
        if (spec & 0x01) {*obuf++ = d0; *obuf++ = d1; }
        else {*obuf++ = d1; *obuf++ = d0; }
      }
      ocnt++;
      inc_cnt -= (1<<24);	
      if (ocnt >= buff_size) break;
    }
    if (samp_cnt <= 0)		{ XA_ADECODE_MOVE_ON; }
  }
  XA_ADECODE_SAVE_LOCAL;
  return(ocnt);
}


typedef struct 
{
  xaULONG	L_bpred ,R_bpred;
  xaLONG	L_delta ,R_delta;
  xaLONG	L_samp1 ,R_samp1;
  xaLONG	L_samp2 ,R_samp2;
  xaLONG	L_nyb1  ,R_nyb1;
  xaULONG	nyb_flag;
  xaULONG	flag;    
   /* 0 don't output samps, 1 output samp1, 2 output both  4 un-init'd */
} XA_MSADPCM_HDR;

XA_MSADPCM_HDR xa_msadpcm; 

#define MSADPCM_NUM_COEF        (7)
#define MSADPCM_MAX_CHANNELS    (2)

#define MSADPCM_PSCALE          (8)
#define MSADPCM_PSCALE_NUM      (1 << MSADPCM_PSCALE)
#define MSADPCM_CSCALE          (8)
#define MSADPCM_CSCALE_NUM      (1 << MSADPCM_CSCALE)

#define MSADPCM_DELTA4_MIN      (16)

static xaLONG  gaiP4[] = { 230, 230, 230, 230, 307, 409, 512, 614,
                          768, 614, 512, 409, 307, 230, 230, 230 };
static xaLONG gaiCoef1[] = { 256,  512,  0, 192, 240,  460,  392 };
static xaLONG gaiCoef2[] = {   0, -256,  0,  64,   0, -208, -232 };

#define AUD_READ_MSADPCM_HDR(ptr,bpred,delta,samp1,samp2)	\
{ bpred= *ptr++; 					\
  delta = *ptr++;	delta |= (*ptr++)<<8;		\
  samp1 = *ptr++;	samp1 |= (*ptr++)<<8;		\
  samp2 = *ptr++;	samp2 |= (*ptr++)<<8;		\
  if (delta & 0x8000)	delta -= 0x10000;	\
  if (samp1 & 0x8000)	samp1 -= 0x10000;	\
  if (samp2 & 0x8000)	samp2 -= 0x10000;	}


#define AUD_READ_MSADPCM_SHDR(ptr,Lbpred,Ldelta,Lsamp1,Lsamp2,Rbpred,Rdelta,Rsamp1,Rsamp2) \
{ Lbpred= *ptr++;	Rbpred= *ptr++;			\
  Ldelta = *ptr++;	Ldelta |= (*ptr++)<<8;		\
  Rdelta = *ptr++;	Rdelta |= (*ptr++)<<8;		\
  Lsamp1 = *ptr++;	Lsamp1 |= (*ptr++)<<8;		\
  Rsamp1 = *ptr++;	Rsamp1 |= (*ptr++)<<8;		\
  Lsamp2 = *ptr++;	Lsamp2 |= (*ptr++)<<8;		\
  Rsamp2 = *ptr++;	Rsamp2 |= (*ptr++)<<8;		\
  if (Ldelta & 0x8000)	Ldelta = Ldelta - 0x10000;	\
  if (Lsamp1 & 0x8000)	Lsamp1 = Lsamp1 - 0x10000;	\
  if (Lsamp2 & 0x8000)	Lsamp2 = Lsamp2 - 0x10000;	\
  if (Rdelta & 0x8000)	Rdelta = Rdelta - 0x10000;	\
  if (Rsamp1 & 0x8000)	Rsamp1 = Rsamp1 - 0x10000;	\
  if (Rsamp2 & 0x8000)	Rsamp2 = Rsamp2 - 0x10000;	}

  
#define AUD_CALC_MSADPCM(lsamp,nyb0,delta,samp1,samp2,coef1,coef2) \
{ xaLONG predict;						\
   /** Compute next Adaptive Scale Factor(ASF) */		\
  idelta = delta;						\
  delta = (gaiP4[nyb0] * idelta) >> MSADPCM_PSCALE;		\
  if (delta < MSADPCM_DELTA4_MIN) delta = MSADPCM_DELTA4_MIN;	\
  if (nyb0 & 0x08) nyb0 = nyb0 - 0x10;				\
   /** Predict next sample */					\
  predict = ((samp1 * coef1) + (samp2 * coef2)) >>  MSADPCM_CSCALE;	\
   /** reconstruct original PCM */				\
  lsamp = (nyb0 * idelta) + predict;				\
  if (lsamp > 32767) lsamp = 32767;				\
  else if (lsamp < -32768) lsamp = -32768;			}

/********** XA_ADecode_ADPCMM_PCM2M *********************************
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
 *   xaUBYTE xa_sign_2_ulaw[256]	conversion table.
 *   XA_Audio_Next_Snd()	routine to move to next sound header.
 ***************************************************************/
xaULONG XA_ADecode_ADPCMM_PCM2M(snd_hdr,obuf,ocnt,buff_size)
XA_SND *snd_hdr;
xaUBYTE *obuf;
xaULONG ocnt,buff_size;
{ XA_ADECODE_DECLARE_LOCAL; 
  xaULONG bpred, blk_cnt;
  xaLONG delta,coef1,coef2,samp1,samp2;

  if (snd_hdr==0) return(ocnt);
  XA_ADECODE_INIT_LOCAL;
  blk_cnt	= snd_hdr->blk_cnt;

  /* Init Codec specific Variables */
  bpred = xa_msadpcm.L_bpred;
  delta = xa_msadpcm.L_delta;
  samp1 = xa_msadpcm.L_samp1;
  samp2 = xa_msadpcm.L_samp2;
  coef1 = gaiCoef1[bpred];
  coef2 = gaiCoef2[bpred];
  
  while(ocnt < buff_size)
  { 
    if (inc_cnt < (1<<24))
    { inc_cnt += inc;  
         /*** Decode Sample ***/
      if (snd_hdr->flag == 0)
      {
        AUD_READ_MSADPCM_HDR(ibuf,bpred,delta,samp1,samp2);
	if (bpred >= 7)   /* 7 should be variable from AVI/WAV header */
	{ 
	  fprintf(stderr,"MSADPC bpred %x blk_cnt %d\n",bpred,blk_cnt);
	  return(ocnt);
	}
	coef1 = gaiCoef1[bpred];
	coef2 = gaiCoef2[bpred];
	byte_cnt += 7;
        blk_cnt = snd_hdr->blk_size - 7;
	snd_hdr->flag = 1;
	dataL = samp2;
      }
      else if (snd_hdr->flag == 1)
      {
	snd_hdr->flag = (blk_cnt)?(2):(0);
	dataL = samp1;
      }
      else /* if (snd_hdr->flag == 2) */
      { xaLONG nyb1,nyb0,idelta,lsamp;
	nyb1 = *ibuf++;		byte_cnt++;	blk_cnt--;
	nyb0 = (nyb1 >> 4) & 0x0f;
	nyb1 &= 0x0f;
	AUD_CALC_MSADPCM(lsamp,nyb0,delta,samp1,samp2,coef1,coef2);
	samp2 = samp1; samp1 = lsamp;
	AUD_CALC_MSADPCM(lsamp,nyb1,delta,samp1,samp2,coef1,coef2);
	samp2 = samp1; samp1 = lsamp;
	snd_hdr->flag = 1;
	dataL = samp2;
      }
      samp_cnt--;
    }

    while(inc_cnt >= (1<<24))
    { xaULONG data = samp2;
      inc_cnt -= (1<<24);
        /*** Output Sample ***/
      if (spec & 4)  /* 1 byte output */
      { xaUBYTE d0 = ((xaULONG)(data) >> 8) & 0xff;
	if (spec & 8)    *obuf++ = xa_sign_2_ulaw[ (xaULONG)(d0) ];
	else             *obuf++ = (spec & 2)?(d0 ^ 0x80):(d0);
      }
      else
      { xaUBYTE d1,d0; d1 = (data>>8) & 0xff; d0 = data & 0xff;
	if (spec & 0x02) d1 ^= 0x80;
	if (spec & 0x01) {*obuf++ = d0; *obuf++ = d1; }
	else {*obuf++ = d1; *obuf++ = d0; }
      }
      ocnt++;
      if (ocnt >= buff_size) break;
    }
    if (samp_cnt <= 0)          { XA_ADECODE_MOVE_ON; }
  }
  XA_ADECODE_SAVE_LOCAL;
  snd_hdr->blk_cnt	= blk_cnt;

  /* Save Codec Specific */
  xa_msadpcm.L_bpred	= bpred;
  xa_msadpcm.L_delta	= delta;
  xa_msadpcm.L_samp1	= samp1;
  xa_msadpcm.L_samp2	= samp2;
  return(ocnt);
}


/********** XA_ADecode_ADPCMS_PCM2M *********************************
 * Convert Microsoft ADPCM Stereo Samples into PCM 2 BPS Mono Samples
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
 *   xaUBYTE xa_sign_2_ulaw[256]        conversion table.
 *   XA_Audio_Next_Snd()        routine to move to next sound header.
 ***************************************************************/

xaULONG XA_ADecode_ADPCMS_PCM2M(snd_hdr,obuf,ocnt,buff_size)
XA_SND *snd_hdr;
xaUBYTE *obuf;
xaULONG ocnt,buff_size;
{ XA_ADECODE_DECLARE_LOCAL;
  xaULONG blk_cnt; 
  xaLONG L_bpred,L_delta,L_coef1,L_coef2,L_samp1,L_samp2;
  xaLONG R_bpred,R_delta,R_coef1,R_coef2,R_samp1,R_samp2;

  if (snd_hdr==0) return(ocnt);
  XA_ADECODE_INIT_LOCAL;
  blk_cnt       = snd_hdr->blk_cnt;
  /* Init Codec specific Variables */
  L_bpred = xa_msadpcm.L_bpred; R_bpred = xa_msadpcm.R_bpred;
  L_delta = xa_msadpcm.L_delta; R_delta = xa_msadpcm.R_delta;
  L_samp1 = xa_msadpcm.L_samp1; R_samp1 = xa_msadpcm.R_samp1;
  L_samp2 = xa_msadpcm.L_samp2; R_samp2 = xa_msadpcm.R_samp2;
  L_coef1 = gaiCoef1[L_bpred];  R_coef1 = gaiCoef1[R_bpred];
  L_coef2 = gaiCoef2[L_bpred];  R_coef2 = gaiCoef2[R_bpred];

  while(ocnt < buff_size)
  {
    if (inc_cnt < (1<<24))
    { inc_cnt += inc;
         /*** Decode Sample ***/
      if (snd_hdr->flag == 0)
      {
	AUD_READ_MSADPCM_SHDR(ibuf,L_bpred,L_delta,L_samp1,L_samp2,
				   R_bpred,R_delta,R_samp1,R_samp2);
		/* 7 should be variable from AVI/WAV header */
	if ((L_bpred >= 7) || (R_bpred >= 7))
	{ fprintf(stderr,"MSADPC bpred %x %x\n",L_bpred,R_bpred);
	  return(ocnt);  /* POD do better abort here */
	}
	L_coef1 = gaiCoef1[L_bpred];  R_coef1 = gaiCoef1[R_bpred];
	L_coef2 = gaiCoef2[L_bpred];  R_coef2 = gaiCoef2[R_bpred];
	byte_cnt += 14;
	blk_cnt = snd_hdr->blk_size - 14;
	snd_hdr->flag = 1;
	dataL = L_samp2;	dataR = R_samp2;
      }
      else if (snd_hdr->flag == 1)
      { snd_hdr->flag = 2;
	dataL = L_samp1;	dataR = R_samp1;
      }
      else /* if (snd_hdr->flag == 2) */
      { xaLONG R_nyb,L_nyb,idelta,lsamp;
	R_nyb = *ibuf++;
	L_nyb = (R_nyb >> 4) & 0x0f;
	R_nyb &= 0x0f;
	byte_cnt++;		blk_cnt--;
	AUD_CALC_MSADPCM(lsamp,L_nyb,L_delta,L_samp1,L_samp2,L_coef1,L_coef2);
	L_samp2 = L_samp1; L_samp1 = lsamp;
	AUD_CALC_MSADPCM(lsamp,R_nyb,R_delta,R_samp1,R_samp2,R_coef1,R_coef2);
	R_samp2 = R_samp1; R_samp1 = lsamp;
	dataL = L_samp1;	dataR = R_samp1;
	snd_hdr->flag = (blk_cnt > 0)?(2):(0);
      }
      samp_cnt--;
    }

    while(inc_cnt >= (1<<24))
    { xaULONG data = (L_samp2 + R_samp2) / 2;
      inc_cnt -= (1<<24);
        /*** Output Sample ***/
      if (spec & 4)  /* 1 byte output */
      {  xaUBYTE d0 = ((xaULONG)(data) >> 8) & 0xff;
	if (spec & 8)    *obuf++ = xa_sign_2_ulaw[ (xaULONG)(d0) ];
	else             *obuf++ = (spec & 2)?(d0 ^ 0x80):(d0);
      }
      else
      { xaUBYTE d1,d0; d1 = (data>>8) & 0xff; d0 = data & 0xff;
	if (spec & 0x02) d1 ^= 0x80;
	if (spec & 0x01) {*obuf++ = d0; *obuf++ = d1; }
	else {*obuf++ = d1; *obuf++ = d0; }
      }
      ocnt++;
      if (ocnt >= buff_size) break;
    }
    if (samp_cnt <= 0)          { XA_ADECODE_MOVE_ON; }
  }
  XA_ADECODE_SAVE_LOCAL;
  snd_hdr->blk_cnt      = blk_cnt;

  /* Save Codec Specific */
  xa_msadpcm.L_bpred = L_bpred; xa_msadpcm.R_bpred = R_bpred;
  xa_msadpcm.L_delta = L_delta; xa_msadpcm.R_delta = R_delta;
  xa_msadpcm.L_samp1 = L_samp1; xa_msadpcm.R_samp1 = R_samp1;
  xa_msadpcm.L_samp2 = L_samp2; xa_msadpcm.R_samp2 = R_samp2;
  return(ocnt);
}


typedef struct XA_AUDIO_DVI_STRUCT
{
    xaLONG	L_valprev, R_valprev;	/* Previous output value */
    xaBYTE	L_index,   R_index;	/* Index into stepsize table */
    xaLONG	L_store;
    xaLONG	R_store;
} XA_AUDIO_DVI_STATE;

XA_AUDIO_DVI_STATE xa_dvi_state;

/* Intel ADPCM step variation table */
static int xa_audio_DVI_index_table[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8
};

static int xa_audio_DVI_stepsize_table[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

#define AUD_CALC_DVI(valpred,delta,index,step)		\
{ xaLONG vpdiff, sign;					\
  /* Find new index value (for later) */		\
  index += xa_audio_DVI_index_table[delta];		\
  if ( index < 0 ) index = 0;				\
  else if ( index > 88 ) index = 88;			\
  /* Separate sign and magnitude */			\
  sign = delta & 8;					\
  delta = delta & 7;					\
  /* Compute difference and new predicted value */	\
  vpdiff = step >> 3;					\
  if (delta & 4)  vpdiff += step;			\
  if (delta & 2)  vpdiff += step>>1;			\
  if (delta & 1)  vpdiff += step>>2;			\
  if ( sign )     valpred -= vpdiff;			\
  else            valpred += vpdiff;			\
  if ( valpred > 32767 )          valpred = 32767;	\
  else if ( valpred < -32768 )    valpred = -32768;	\
  /* update step value */				\
  step = xa_audio_DVI_stepsize_table[index];		}

/********** XA_ADecode_DVIM_PCMxM *********************************
 * Convert DVI ADPCM Mono Samples into PCM 2 BPS Mono Samples
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
 *   xaUBYTE xa_sign_2_ulaw[256]        conversion table.
 *   XA_Audio_Next_Snd()        routine to move to next sound header.
 ***************************************************************/

xaULONG XA_ADecode_DVIM_PCMxM(snd_hdr,obuf,ocnt,buff_size)
XA_SND *snd_hdr;
xaUBYTE *obuf;
xaULONG ocnt,buff_size;
{ XA_ADECODE_DECLARE_LOCAL; 
  xaLONG blk_cnt,valpred, index, step;

  if (snd_hdr==0) return(ocnt);
  XA_ADECODE_INIT_LOCAL; 
  blk_cnt	= snd_hdr->blk_cnt;

  /* Init Codec specific Variables */
  valpred	= xa_dvi_state.L_valprev;
  index		= xa_dvi_state.L_index;
  step		= xa_audio_DVI_stepsize_table[index];

  while(ocnt < buff_size)
  {
    if (inc_cnt < (1<<24))
    { inc_cnt += inc;
	/*** Decode Sample ***/
      if (snd_hdr->flag == 0)
      {  /* Read Header - POD macro */
	valpred = *ibuf++;	valpred |= (*ibuf++)<<8;
	if (valpred & 0x8000)	valpred = valpred - 0x10000;
	index = *ibuf++;
	ibuf++;   /* pad */

	if (index > 88)
	{ fprintf(stderr,"DVI: index err %d\n",index);
	  index = 88;
	}
	step = xa_audio_DVI_stepsize_table[index];
	blk_cnt = snd_hdr->blk_size - 4;
	byte_cnt += 4;
	snd_hdr->flag = 1;
	dataL = valpred;
      }
      else if (snd_hdr->flag == 1)
      { xaLONG nyb0,nyb1;
	if (xa_kludge2_dvi)	/* old style MS DVI implementation */
	{ nyb1 = *ibuf++;		/* MSB held 1st sample */
	  nyb0 = (nyb1 >> 4) & 0x0f;
	  nyb1 &= 0x0f;
	}
	else			/* new style MS DVI implementation */
	{ nyb0 = *ibuf++;		/* LSB held 1st sample */
	  nyb1 = (nyb0 >> 4) & 0x0f;
	  nyb0 &= 0x0f;
	}
	byte_cnt++;	blk_cnt--;


	AUD_CALC_DVI(valpred,nyb0,index,step);
	dataL = valpred;
	AUD_CALC_DVI(valpred,nyb1,index,step);
	snd_hdr->flag = 2;
      }
      else /* if (snd_hdr->flag == 2) */
      { dataL = valpred;
	snd_hdr->flag = (blk_cnt>0)?(1):(0);
      }
      samp_cnt--;
    }

    while(inc_cnt >= (1<<24))
    { inc_cnt -= (1<<24);	
        /*** Output Sample ***/
      if (spec & 4)  /* 1 byte output */
      { xaUBYTE d0 = ((xaULONG)(dataL) >> 8) & 0xff;
	if (spec & 8)    *obuf++ = xa_sign_2_ulaw[ (xaULONG)(d0) ];
	else             *obuf++ = (spec & 2)?(d0 ^ 0x80):(d0);
      }
      else
      { xaUBYTE d1,d0; d1 = (dataL>>8) & 0xff; d0 = dataL & 0xff;
	if (spec & 0x02) d1 ^= 0x80;
	if (spec & 0x01) {*obuf++ = d0; *obuf++ = d1; }
	else {*obuf++ = d1; *obuf++ = d0; }
      }
      ocnt++;
      if (ocnt >= buff_size) break;
    }
    if (samp_cnt <= 0)		{ XA_ADECODE_MOVE_ON; }
  }
  XA_ADECODE_SAVE_LOCAL;
  snd_hdr->blk_cnt	= blk_cnt;
  /* Save Codec Specific */
  xa_dvi_state.L_valprev	= valpred;
  xa_dvi_state.L_index		= index;
  return(ocnt);
}


/********** XA_ADecode_DVIS_PCMxM *********************************
 * Convert DVI ADPCM Stereo Samples into PCM 2 BPS Mono Samples
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
 *   xaUBYTE xa_sign_2_ulaw[256]        conversion table.
 *   XA_Audio_Next_Snd()        routine to move to next sound header.
 ***************************************************************/

xaULONG XA_ADecode_DVIS_PCMxM(snd_hdr,obuf,ocnt,buff_size)
XA_SND *snd_hdr;
xaUBYTE *obuf;
xaULONG ocnt,buff_size;
{ XA_ADECODE_DECLARE_LOCAL; 
  xaLONG blk_cnt;
  xaLONG L_valpred, L_index, L_step, R_valpred, R_index, R_step;
  xaULONG L_store, R_store;

  if (snd_hdr==0) return(ocnt);
  XA_ADECODE_INIT_LOCAL; 
  blk_cnt	= snd_hdr->blk_cnt;

  /* Init Codec specific Variables */
  L_valpred	= xa_dvi_state.L_valprev;
  L_index	= xa_dvi_state.L_index;
  L_step	= xa_audio_DVI_stepsize_table[L_index];
  L_store	= xa_dvi_state.L_store;
  R_valpred	= xa_dvi_state.R_valprev;
  R_index	= xa_dvi_state.R_index;
  R_step	= xa_audio_DVI_stepsize_table[R_index];
  R_store	= xa_dvi_state.R_store;

  while(ocnt < buff_size)
  {
    if (inc_cnt < (1<<24))
    { inc_cnt += inc;
	/*** Decode Sample ***/
      if (snd_hdr->flag == 0)
      {  /* Read Header - POD macro */
	L_valpred = *ibuf++;	L_valpred |= (*ibuf++)<<8;
	if (L_valpred & 0x8000)	L_valpred = L_valpred - 0x10000;
	L_index = *ibuf++;	if (L_index > 88)	L_index = 88;
	L_step = xa_audio_DVI_stepsize_table[L_index];
	ibuf++;   /* pad */
	R_valpred = *ibuf++;	R_valpred |= (*ibuf++)<<8;
	if (R_valpred & 0x8000)	R_valpred = R_valpred - 0x10000;
	R_index = *ibuf++;	if (R_index > 88)	R_index = 88;
	R_step = xa_audio_DVI_stepsize_table[R_index];
	ibuf++;   /* pad */

	blk_cnt = snd_hdr->blk_size - 8;	byte_cnt += 8;
	snd_hdr->flag = 1;
	dataL = L_valpred;
	dataR = R_valpred;
      }
      else  /* flag == 1,2,3,4..8 */
      { xaLONG nyb; 
	if (snd_hdr->flag == 1)
	{ if (xa_kludge2_dvi)	/* old style MS DVI implementation */
	  { L_store = *ibuf++;			R_store = L_store >> 4;
	    byte_cnt += 1;			blk_cnt -= 1;
	  }
	  else			/* new style MS DVI implementation */ 
	  { L_store  = *ibuf++;		L_store |= *ibuf++ <<  8;
            L_store |= *ibuf++ << 16;	L_store |= *ibuf++ << 24;
            R_store  = *ibuf++;		R_store |= *ibuf++ <<  8;
            R_store |= *ibuf++ << 16;	R_store |= *ibuf++ << 24;
	    byte_cnt += 8;			blk_cnt -= 8;
	  }
	}
	else { L_store >>= 4;  R_store >>= 4; }

	nyb = L_store & 0x0f;
	AUD_CALC_DVI(L_valpred,nyb,L_index,L_step);
	dataL = L_valpred;

	nyb = R_store & 0x0f;
	AUD_CALC_DVI(R_valpred,nyb,R_index,R_step);
	dataR = R_valpred;

	if ((xa_kludge2_dvi) || (snd_hdr->flag >= 8))
				snd_hdr->flag = (blk_cnt > 0)?(1):(0);
	else			snd_hdr->flag++;
      }
      samp_cnt--;
    }

    while(inc_cnt >= (1<<24))
    { xaLONG data = (dataL + dataR) / 2;
      inc_cnt -= (1<<24);	
        /*** Output Sample ***/
      if (spec & 4)  /* 1 byte output */
      { xaUBYTE d0 = ((xaULONG)(data) >> 8) & 0xff;
	if (spec & 8)    *obuf++ = xa_sign_2_ulaw[ (xaULONG)(d0) ];
	else             *obuf++ = (spec & 2)?(d0 ^ 0x80):(d0);
      }
      else
      { xaUBYTE d1,d0; d1 = (data>>8) & 0xff; d0 = data & 0xff;
	if (spec & 0x02) d1 ^= 0x80;
	if (spec & 0x01) {*obuf++ = d0; *obuf++ = d1; }
	else {*obuf++ = d1; *obuf++ = d0; }
      }
      ocnt++;
      if (ocnt >= buff_size) break;
    }
    if (samp_cnt <= 0)		{ XA_ADECODE_MOVE_ON; }
  }
  XA_ADECODE_SAVE_LOCAL;
  snd_hdr->blk_cnt	= blk_cnt;
  /* Save Codec Specific */
  xa_dvi_state.L_valprev	= L_valpred;
  xa_dvi_state.L_index		= L_index;
  xa_dvi_state.R_valprev	= R_valpred;
  xa_dvi_state.R_index		= R_index;
  xa_dvi_state.L_store		= L_store;
  xa_dvi_state.R_store		= R_store;
  return(ocnt);
}

/********** XA_ADecode_IMA4M_PCMxM *********************************
 * Convert QT IMA4 ADPCM Mono Samples into PCM 2 BPS Mono Samples
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
 *   xaUBYTE xa_sign_2_ulaw[256]        conversion table.
 *   XA_Audio_Next_Snd()        routine to move to next sound header.
 ***************************************************************/
xaULONG XA_ADecode_IMA4M_PCMxM(snd_hdr,obuf,ocnt,buff_size)
XA_SND *snd_hdr;
xaUBYTE *obuf;
xaULONG ocnt,buff_size;
{ XA_ADECODE_DECLARE_LOCAL; 
  xaLONG blk_cnt,valpred, index, step;

  if (snd_hdr==0) return(ocnt);
  XA_ADECODE_INIT_LOCAL; 
  blk_cnt	= snd_hdr->blk_cnt;

  /* Init Codec specific Variables */
  valpred	= xa_dvi_state.L_valprev;
  index		= xa_dvi_state.L_index;
  step		= xa_audio_DVI_stepsize_table[index];

  while(ocnt < buff_size)
  {
    if (inc_cnt < (1<<24))
    { inc_cnt += inc;
	/*** Decode Sample ***/
      if (snd_hdr->flag == 2)
      { dataL = valpred;
	snd_hdr->flag = (blk_cnt>0)?(1):(0);
      }
      else
      { xaLONG nyb0,nyb1;
        if (snd_hdr->flag == 0)
        {  /* Read Header - POD macro */
	  valpred = *ibuf++ << 8;	valpred |= (*ibuf++);
	  index = valpred & 0x7f;
	  if (index > 88)
	  { fprintf(stderr,"IMA4: index err %d\n",index);
	    index = 88;
	  }
	  valpred &= 0xff80;
	  if (valpred & 0x8000)	valpred = valpred - 0x10000;
	  blk_cnt = snd_hdr->blk_size - 2;
	  byte_cnt += 2;
	  step = xa_audio_DVI_stepsize_table[index];
	  snd_hdr->flag = 1;
        }

	nyb0 = *ibuf++;		/* LSB held 1st sample */
	nyb1 = (nyb0 >> 4) & 0x0f;
	nyb0 &= 0x0f;
	byte_cnt++;	blk_cnt--;

	AUD_CALC_DVI(valpred,nyb0,index,step);
	dataL = valpred;
	AUD_CALC_DVI(valpred,nyb1,index,step);
	snd_hdr->flag = 2;
      }
      samp_cnt--;
    }

    while(inc_cnt >= (1<<24))
    { inc_cnt -= (1<<24);	
        /*** Output Sample ***/
      if (spec & 4)  /* 1 byte output */
      { xaUBYTE d0 = ((xaULONG)(dataL) >> 8) & 0xff;
	if (spec & 8)    *obuf++ = xa_sign_2_ulaw[ (xaULONG)(d0) ];
	else             *obuf++ = (spec & 2)?(d0 ^ 0x80):(d0);
      }
      else
      { xaUBYTE d1,d0; d1 = (dataL>>8) & 0xff; d0 = dataL & 0xff;
	if (spec & 0x02) d1 ^= 0x80;
	if (spec & 0x01) {*obuf++ = d0; *obuf++ = d1; }
	else {*obuf++ = d1; *obuf++ = d0; }
      }
      ocnt++;
      if (ocnt >= buff_size) break;
    }
    if (samp_cnt <= 0)		{ XA_ADECODE_MOVE_ON; }
  }
  XA_ADECODE_SAVE_LOCAL;
  snd_hdr->blk_cnt	= blk_cnt;
  /* Save Codec Specific */
  xa_dvi_state.L_valprev	= valpred;
  xa_dvi_state.L_index		= index;
  return(ocnt);
}

/********** XA_ADecode_IMA4S_PCMxM *********************************
 * Convert QT IMA4 ADPCM Stereo Samples into PCM 2 BPS Mono Samples
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
 *   xaUBYTE xa_sign_2_ulaw[256]        conversion table.
 *   XA_Audio_Next_Snd()        routine to move to next sound header.
 ***************************************************************/

xaULONG XA_ADecode_IMA4S_PCMxM(snd_hdr,obuf,ocnt,buff_size)
XA_SND *snd_hdr;
xaUBYTE *obuf;
xaULONG ocnt,buff_size;
{ XA_ADECODE_DECLARE_LOCAL; 
  xaUBYTE *ibufR;
  xaLONG blk_cnt,blk_size;
  xaLONG L_valpred, L_index, L_step, R_valpred, R_index, R_step;

  if (snd_hdr==0) return(ocnt);
  XA_ADECODE_INIT_LOCAL; 
  blk_cnt	= snd_hdr->blk_cnt;
  blk_size	= snd_hdr->blk_size >> 1;

  /* Init Codec specific Variables */
  L_valpred	= xa_dvi_state.L_valprev;
  L_index	= xa_dvi_state.L_index;
  L_step	= xa_audio_DVI_stepsize_table[L_index];
  R_valpred	= xa_dvi_state.R_valprev;
  R_index	= xa_dvi_state.R_index;
  R_step	= xa_audio_DVI_stepsize_table[R_index];

/* POD: assume Right channel is always within same block as left channel 
 * for now at least */
  ibufR = ibuf;
  ibufR += blk_size;
  while(ocnt < buff_size)
  {
    if (inc_cnt < (1<<24))
    { inc_cnt += inc;
	/*** Decode Sample ***/
      if (snd_hdr->flag == 2)
      { dataL = L_valpred;
	dataR = R_valpred;

	if (blk_cnt == 0)
        {  byte_cnt += blk_size;
	   ibuf += blk_size;	/* skip R channel */
	   ibufR += blk_size;
	   snd_hdr->flag = 0;
	}
        else snd_hdr->flag = 1;
      }
      else 
      { xaLONG nyb0,nyb1; 
	if (snd_hdr->flag == 0)
	{  /* Read Header - POD macro */
	  L_valpred = *ibuf++ << 8;	L_valpred |= (*ibuf++);
	  L_index = L_valpred & 0x7f;

	  if (L_index > 88)
	  { fprintf(stderr,"IMA4: L index err %d\n",L_index);
	    L_index = 88;
	  }
	  L_valpred &= 0xff80;
	  if (L_valpred & 0x8000)	L_valpred = L_valpred - 0x10000;
	  L_step = xa_audio_DVI_stepsize_table[L_index];

	  R_valpred = *ibufR++ << 8;	R_valpred |= (*ibufR++);
	  R_index = R_valpred & 0x7f;

	  if (R_index > 88)
	  { fprintf(stderr,"IMA4: R_index err %d\n",R_index);
	    R_index = 88;
	  }
	  R_valpred &= 0xff80;
	  if (R_valpred & 0x8000)	R_valpred = R_valpred - 0x10000;
	  R_step = xa_audio_DVI_stepsize_table[R_index];

	  blk_cnt = blk_size - 2;		byte_cnt += 2;
	  snd_hdr->flag = 1;
	}

	nyb0 = *ibuf++; nyb1 = (nyb0 >> 4) & 0x0f; nyb0 &= 0x0f;

	byte_cnt += 1;			blk_cnt -= 1;
	AUD_CALC_DVI(L_valpred,nyb0,L_index,L_step);
	dataL = L_valpred;
	AUD_CALC_DVI(L_valpred,nyb1,L_index,L_step);

	nyb0 = *ibufR++; nyb1 = (nyb0 >> 4) & 0x0f; nyb0 &= 0x0f;

	AUD_CALC_DVI(R_valpred,nyb0,R_index,R_step);
	dataR = R_valpred;
	AUD_CALC_DVI(R_valpred,nyb1,R_index,R_step);

        snd_hdr->flag = 2;
      }
      samp_cnt--;
    }

    while(inc_cnt >= (1<<24))
      { xaLONG data = (dataL + dataR) / 2;
/*    { xaULONG data = ( (dataL & 0xffff) + (dataR & 0xffff)) >> 1; */
      inc_cnt -= (1<<24);	
        /*** Output Sample ***/
      if (spec & 4)  /* 1 byte output */
      { xaUBYTE d0 = ((xaULONG)(data) >> 8) & 0xff;
	if (spec & 8)    *obuf++ = xa_sign_2_ulaw[ (xaULONG)(d0) ];
	else             *obuf++ = (spec & 2)?(d0 ^ 0x80):(d0);
      }
      else
      { xaUBYTE d1,d0; d1 = (data>>8) & 0xff; d0 = data & 0xff;
	if (spec & 0x02) d1 ^= 0x80;
	if (spec & 0x01) {*obuf++ = d0; *obuf++ = d1; }
	else {*obuf++ = d1; *obuf++ = d0; }
      }
      ocnt++;
      if (ocnt >= buff_size) break;
    }
    if (samp_cnt <= 0)		{ XA_ADECODE_MOVE_ON; }
  }
  XA_ADECODE_SAVE_LOCAL;
  snd_hdr->blk_cnt	= blk_cnt;
  /* Save Codec Specific */
  xa_dvi_state.L_valprev	= L_valpred;
  xa_dvi_state.L_index		= L_index;
  xa_dvi_state.R_valprev	= R_valpred;
  xa_dvi_state.R_index		= R_index;
  return(ocnt);
}

#ifdef XA_GSM

#include "xa_gsm_state.h"

static xaSHORT gsm_buf[320];
static XA_GSM_STATE gsm_state;

void GSM_Init()
{
  memset((char *)(&gsm_state), 0, sizeof(XA_GSM_STATE));
  gsm_state.nrp = 40;
}

/***************************************************************************
 *
 *
 */ 
xaULONG XA_ADecode_GSMM_PCMxM(snd_hdr,obuf,ocnt,buff_size)
XA_SND *snd_hdr;
xaUBYTE *obuf;
xaULONG ocnt,buff_size;
{ XA_ADECODE_DECLARE_LOCAL; 
  xaULONG out_cnt = 320;
  if (snd_hdr==0) return(ocnt);
  XA_ADECODE_INIT_LOCAL;	/* Init Local Variables */
  /* Optionally Init Codec Specific Structure Here */ 
  while(ocnt < buff_size)
  {
    if (inc_cnt < (1<<24))
    {
      if (snd_hdr->flag == 0)
      { xaULONG icnt;

/* Quicktime and AVI used different formats */
	if (spec & 0x80)
           XA_MSGSM_Decoder(&gsm_state, ibuf, gsm_buf, &icnt, &out_cnt);
	else
           XA_GSM_Decoder(&gsm_state, ibuf, gsm_buf, &icnt, &out_cnt);
	ibuf += icnt;
	byte_cnt += icnt;
      }

	/*** Decode Sample and increment byte_cnt ***/
      if (snd_hdr->flag < out_cnt)  dataL  = (xaLONG)gsm_buf[ snd_hdr->flag ];
      else /* ERROR */ dataL = 0;

      snd_hdr->flag++; if (snd_hdr->flag >= out_cnt) snd_hdr->flag = 0;
      samp_cnt--;
      inc_cnt += inc;
    }

    while(inc_cnt >= (1<<24))
    { 
        /*** Output Sample ***/
      if (spec & 4)  /* 1 byte output */
      { xaUBYTE d0 = ((xaULONG)(dataL) >> 8) & 0xff;
        if (spec & 8)    *obuf++ = xa_sign_2_ulaw[ (xaULONG)(d0) ];
        else             *obuf++ = (spec & 2)?(d0 ^ 0x80):(d0);
      }
      else
      { xaUBYTE d1,d0; d1 = (dataL>>8) & 0xff; d0 = dataL & 0xff;
        if (spec & 0x02) d1 ^= 0x80;
        if (spec & 0x01) {*obuf++ = d0; *obuf++ = d1; }
        else {*obuf++ = d1; *obuf++ = d0; }
      }
      ocnt++;
      inc_cnt -= (1<<24);	
      if (ocnt >= buff_size) break;
    }
    if (samp_cnt <= 0)		{ XA_ADECODE_MOVE_ON; }
  }
  XA_ADECODE_SAVE_LOCAL;     /* save local variables */
  /* Optionally Save Codec Specific Structure Here */
  return(ocnt);
}
#endif



/*+_+_+_+_+_+_+_+___+_+_+_+_+_+_+_+_+_++_+_++_+_+_++_+_+++_+_+_+_+_+*/
/*+_+_+_+_+_+_+_+___+_+_+_+_+_+_+_+_+_++_+_++_+_+_++_+_+++_+_+_+_+_+*/
/*+_+_+_+_+_+_+_+___+_+_+_+_+_+_+_+_+_++_+_++_+_+_++_+_+++_+_+_+_+_+*/

void XA_Audio_Init_Snd(snd_hdr)
XA_SND *snd_hdr;
{
  snd_hdr->flag     = 0;
  snd_hdr->inc_cnt  = 0;
  snd_hdr->byte_cnt = 0;
  snd_hdr->samp_cnt = snd_hdr->tot_samps;
  Gen_uLaw_2_Signed();
  Gen_aLaw_2_Signed();
  Gen_Arm_2_Signed();
}


extern XA_AUD_FLAGS *vaudiof;
extern xaULONG xa_vaudio_present;

/********* XA_Add_Sound ****************************************
 * IMPORTANT: THIS ROUTINE IS IN THE VIDEO DOMAIN
 *
 * Global Variables Used:
 *   double XAAUD->scale	linear scale of frequency
 *   xaULONG  xa_audio_hard_buff	size of sound chunk - set by XA_Closest_Freq().
 *   xaULONG  XAAUD->bufferit	xaTRUE if this routine is to convert and buffer
 * 				the audio data ahead of time.
 *   xaULONG  xa_audio_hard_type	Audio Type of Current Hardware
 *
 * Add sound double checks xa_audio_present. 
 *   If UNK then it calls XA_Audio_Init().
 *   If OK adds the sound, else returns false.
 * 
 * NOTE: isnd should be a separate buffer for each chunk since later
 *       one it gets free()'d
 *****/
xaULONG XA_Add_Sound(anim_hdr,isnd,itype,fpos,ifreq,ilen,stime,stimelo,
			blockalign, sampsblock)
XA_ANIM_HDR *anim_hdr;
xaUBYTE *isnd;
xaULONG itype;	/* sound type */
xaULONG fpos;	/* file position */
xaULONG ifreq;	/* input frequency */
xaULONG ilen;	/* length of snd sample chunk in bytes */
xaLONG *stime;	/* start time of sample */
xaULONG *stimelo;	/* fractional start time of sample */
xaULONG blockalign;
xaULONG sampsblock;
{ XA_SND *new_snd;
  xaULONG bps,isamps,totsamps,hfreq,inc,ret;
  double finc,ftime,fadj_freq;

DEBUG_LEVEL1
{
	fprintf(stderr,"isnd %i, itype %i, fpos %i, ifreq %i, ilen %i\n",
			(xaULONG)isnd,itype,fpos,ifreq,ilen);
	fprintf(stderr,"stime %i, stimelo %i, blockalign %i, sampsblock %i\n",
			*stime,*stimelo,blockalign,sampsblock);
}

#ifndef XA_AUDIO

  if (xa_verbose) 
  { fprintf(stderr,
	"Warning: Since Audio Support was not enabled in this executable\n");
    fprintf(stderr,"the audio portion of this file is ignored.\n");
  }
  return(xaFALSE);

#else

  /* If audio hasn't been initialized, then init it */
  if (xa_vaudio_present == XA_AUDIO_UNK)
  { XA_AUDIO_INIT(xa_vaudio_present);  /* note: also sets xa_forkit */
    if (xa_forkit==xaFALSE) xa_vaudio_present = XA_AUDIO_ERR;
  }
  /* Return xaFALSE if it's not OK */
  if (xa_vaudio_present != XA_AUDIO_OK)
  {
    if (xa_forkit == xaTRUE)
    { XA_AUDIO_EXIT();
      xa_forkit = xaFALSE;
    }
    return(xaFALSE);
  }

  new_snd = (XA_SND *)malloc(sizeof(XA_SND));
  if (new_snd==0) TheEnd1("snd malloc err");

  bps = 1;

  if ( (itype & XA_AUDIO_TYPE_MASK) == XA_AUDIO_IMA4)
  { xaLONG w,tmp_len;
    new_snd->blk_size = blockalign;

     /** calculate total samples in entire chunk for timing purposes */
    w = (ilen / blockalign);  
    tmp_len = sampsblock * w;  		/* out len based on full blocks */

    w = ilen - (w * blockalign);	/* bytes remaining for partial blocks*/

    if (itype == XA_AUDIO_IMA4_M)
    { w -= 2;	/* sub header */
      if (w >= 0)
      { w *= 2;  /* 2 samps per byte */
        w += 0;  /* 1 samp in header */
        tmp_len += w;
      }
    }
    if (itype == XA_AUDIO_IMA4_S)
    { w -= 2;	/* sub header */
      if (w >= 0)
      { w *= 1;  /* 1 samps per byte */
        w += 0;  /* 1 samp in header */
        tmp_len += w;
      }
    }
    totsamps = isamps = tmp_len;
  }
  else if ( (itype & XA_AUDIO_TYPE_MASK) == XA_AUDIO_DVI)
  { xaLONG w,tmp_len;
    new_snd->blk_size = blockalign;

     /** calculate total samples in entire chunk for timing purposes */
    w = (ilen / blockalign);  
    tmp_len = sampsblock * w;  		/* out len based on full blocks */

    w = ilen - (w * blockalign);	/* bytes remaining for partial blocks*/

    if (itype == XA_AUDIO_DVI_M)
    { w -= 4;	/* sub header */
      if (w >= 0)
      { w *= 2;  /* 2 samps per byte */
        w += 1;  /* 1 samp in header */
        tmp_len += w;
      }
    }
    if (itype == XA_AUDIO_DVI_S)
    { w -= 8;	/* sub header */
      if (w >= 0)
      { w *= 1;  /* 1 samps per byte */
        w += 1;  /* 1 samp in header */
        tmp_len += w;
      }
    }
    totsamps = isamps = tmp_len;
  }
  else if ((itype & XA_AUDIO_TYPE_MASK) == XA_AUDIO_ADPCM)
  { xaLONG w,tmp_len;

    new_snd->blk_size = blockalign;

     /** calculate total samples in entire chunk for timing purposes */
    w = (ilen / blockalign);  
    tmp_len = sampsblock * w;  		/* out len based on full blocks */

    w = ilen - (w * blockalign);	/* bytes remaining for partial blocks*/
    if (itype == XA_AUDIO_ADPCM_M)
    { 
      w -= (7 * 1);			/* subtract header */
      if (w >= 0)			/* if anything left */
      { w *= 2;				/* 2 samps per byte */
        w += 2;				/* plus samples in header */
        tmp_len += w;
      }
    }
    else if (itype == XA_AUDIO_ADPCM_S)
    { 
      w -= (7 * 2);			/* subtract header */
      if (w >= 0)			/* if anything left */
      { w *= 1;				/* 1 stereo samps per byte */
        w += 2;				/* plus samples in header */
        tmp_len += w;
      }
    }
    totsamps = isamps = tmp_len;
  }
#ifdef XA_GSM
  else if (   ((itype & XA_AUDIO_TYPE_MASK) == XA_AUDIO_GSM)
           || ((itype & XA_AUDIO_TYPE_MASK) == XA_AUDIO_MSGSM)
	  )
  { xaLONG w,tmp_len;
    new_snd->blk_size = blockalign;

     /** calculate total samples in entire chunk for timing purposes */
    w = (ilen / blockalign);
    tmp_len = sampsblock * w;           /* out len based on full blocks */

    totsamps = isamps = tmp_len;

    /* POD CHECK for partials??? */
    if ( (w * blockalign) != ilen)
    { fprintf(stderr,"GSM weird err. ilen %d block %d w %d quant %d\n",
                        ilen, blockalign, w, (w * blockalign) );
    }
  }
#endif
  else
  {
    if (itype & XA_AUDIO_STEREO_MSK) bps *= 2;
    if (itype & XA_AUDIO_BPS_2_MSK) bps *= 2;
    totsamps = isamps = ilen / bps;
    new_snd->blk_size = bps;
  }

  new_snd->tot_samps = new_snd->samp_cnt = isamps;
  new_snd->tot_bytes = ilen;
  new_snd->byte_cnt = 0;
  new_snd->blk_cnt  = 0;

  new_snd->fpos = fpos;
  new_snd->type = itype;
  new_snd->flag = 0;
  new_snd->ifreq = ifreq;
  hfreq = (vaudiof->playrate)?(vaudiof->playrate):(ifreq);

  if (xa_forkit == xaTRUE) xa_forkit = XA_Video_Send2_Audio(XA_IPC_GET_CFREQ,
					NULL,0,hfreq,2000,&hfreq);
  else hfreq = 0;
  if (xa_forkit == xaTRUE) xa_forkit = XA_Video_Send2_Audio(XA_IPC_GET_BSIZE,
					NULL,0,0,2000,&xa_vaudio_hard_buff);
  if (xa_forkit == xaFALSE) return(xaFALSE);

  new_snd->hfreq = hfreq;
  new_snd->ch_size = xa_vaudio_hard_buff;


  /* Setup and return Chunk Start Time */
  new_snd->snd_time = *stime;
  { xaULONG tint;
    ftime = ((double)(totsamps) * 1000.0) / (double)(ifreq);
    tint = (xaULONG)(ftime);   /* get integer time */
    *stime += tint;
    ftime -= (double)(tint); /* get fraction time */
    *stimelo += (xaULONG)( ftime * (double)(1<<24) );
    while( (*stimelo) > (1<<24)) { *stime += 1; *stimelo -= (1<<24); }
  }

  /* Determine f2f inc */
  /** NEW STYLE  hfreq/ifreq **/
  fadj_freq = (double)(hfreq) * vaudiof->scale;
  finc = (double)(fadj_freq)/ (double)(ifreq);
  new_snd->inc = inc = (xaULONG)( finc * (double)(1<<24) );
  new_snd->inc_cnt = 0;

  /* Determine Chunk Time */
  ftime = ((double)(xa_vaudio_hard_buff) * 1000.0) / (double)(hfreq);
  new_snd->ch_time = (xaLONG)ftime;
  ftime -= (double)(new_snd->ch_time);
  new_snd->ch_timelo = (xaULONG)(ftime * (double)(1<<24));

  new_snd->prev = 0;
  new_snd->next = 0;

/* POD
fprintf(stderr,"Fork_Add_Sound itype %x\n",new_snd->type);
*/

  /* Send SND HDR */
  if (xa_forkit == xaTRUE) xa_forkit = XA_Video_Send2_Audio(XA_IPC_SND_ADD,
	new_snd, (sizeof(XA_SND)), anim_hdr->file_num, 2000, &ret);
  if (xa_forkit == xaFALSE) return(xaFALSE);
  if (ret == xaFALSE) return(xaFALSE);

  if (isnd==0) ilen = 0;

  /* Send SND Buffer */
  if (xa_forkit == xaTRUE) 
  {
    xa_forkit = XA_Video_Send2_Audio(XA_IPC_SND_BUF, isnd, 
					ilen, anim_hdr->file_num, 2000, &ret);
    free(isnd); isnd = 0;
    if (xa_forkit == xaFALSE) return(xaFALSE);
    if (ret == xaFALSE) return(xaFALSE);
  } 
  else 
  { free(isnd); isnd = 0; 
    return(xaFALSE); 
  }


/* new_snd->snd = isnd; */
/* new_snd->spec  */
/* new_snd->delta */

  return(xaTRUE);
#endif
}


/*******************************
 *
 ******************/
xaULONG XA_IPC_Sound(aud_hdr,new_snd)
XA_AUD_HDR *aud_hdr;
XA_SND *new_snd;
{ 
  xaULONG itype = new_snd->type;

/*POD
fprintf(stderr,"XA_IPC_Sound itype %x\n",itype);
*/
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
			new_snd->delta = XA_ADecode_PCMXS_PCM1M;
		else
			new_snd->delta = XA_ADecode_PCMXM_PCM1M;
		break;
	  case XA_AUDIO_SUN_AU:
		new_snd->delta = XA_ADecode_1M_1M;
		break;
	  case XA_AUDIO_ADPCM_M:
	        new_snd->spec = 2 | 4 | 8;
		new_snd->delta = XA_ADecode_ADPCMM_PCM2M;
		break;
	  case XA_AUDIO_ADPCM_S:
	        new_snd->spec = 2 | 4 | 8;
		new_snd->delta = XA_ADecode_ADPCMS_PCM2M;
		break;
	  case XA_AUDIO_DVI_M:
	        new_snd->spec = 2 | 4 | 8;
		new_snd->delta = XA_ADecode_DVIM_PCMxM;
		break;
	  case XA_AUDIO_DVI_S:
	        new_snd->spec = 2 | 4 | 8;
		new_snd->delta = XA_ADecode_DVIS_PCMxM;
		break;
	  case XA_AUDIO_IMA4_M:
	        new_snd->spec = 2 | 4 | 8;
		new_snd->delta = XA_ADecode_IMA4M_PCMxM;
		break;
	  case XA_AUDIO_IMA4_S:
	        new_snd->spec = 2 | 4 | 8;
		new_snd->delta = XA_ADecode_IMA4S_PCMxM;
		break;
#ifdef XA_GSM
          case XA_AUDIO_GSM_M:
          case XA_AUDIO_MSGSM_M:
                new_snd->spec = 2 | 4 | 8;
		if (itype == XA_AUDIO_MSGSM_M) new_snd->spec |= 0x80;
                new_snd->delta = XA_ADecode_GSMM_PCMxM;
                break;
#endif
	  case XA_AUDIO_NOP:
	        new_snd->spec = 2 | 4 | 8;
		new_snd->delta = XA_ADecode_NOP_PCMXM;
		break;
	  default: 
	    fprintf(stderr,"SUN_AU_AUDIO: Unsupported Software Type %x\n",
				itype);
		return(xaFALSE);
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
		new_snd->delta = XA_ADecode_PCM1M_PCM2M;
		break;
	  case XA_AUDIO_LINEAR_1S:      /* LIN1S -> SIN2M*  */
		if (xa_audio_hard_type & XA_AUDIO_BIGEND_MSK)
			new_snd->spec = 1;
		else    new_snd->spec = 2;
		new_snd->delta = XA_ADecode_PCM1S_PCMxM;
		break;
	  case XA_AUDIO_SIGNED_1S:      /* SIN1S -> SIN2M*  */
		if (xa_audio_hard_type & XA_AUDIO_BIGEND_MSK)
			new_snd->spec = 6;
		else    new_snd->spec = 5;
		new_snd->delta = XA_ADecode_PCM1S_PCMxM;
		break;
	  case XA_AUDIO_SIGNED_1M:      /* SIN1M -> SIN2M*  */
		if (xa_audio_hard_type & XA_AUDIO_BIGEND_MSK)
			new_snd->spec = 2;
		else    new_snd->spec = 1;
		new_snd->delta = XA_ADecode_PCM1M_PCM2M;
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
		new_snd->delta = XA_ADecode_PCM2X_PCM2M;
		break;
	  case XA_AUDIO_ADPCM_M:
		if (xa_audio_hard_type & XA_AUDIO_BIGEND_MSK)
			new_snd->spec = 0;
		else	new_snd->spec = 1;
		new_snd->delta = XA_ADecode_ADPCMM_PCM2M;
		break;
	  case XA_AUDIO_ADPCM_S:
		if (xa_audio_hard_type & XA_AUDIO_BIGEND_MSK)
			new_snd->spec = 0;
		else	new_snd->spec = 1;
		new_snd->delta = XA_ADecode_ADPCMS_PCM2M;
		break;
	  case XA_AUDIO_DVI_M:
		if (xa_audio_hard_type & XA_AUDIO_BIGEND_MSK)
			new_snd->spec = 0;
		else	new_snd->spec = 1;
		new_snd->delta = XA_ADecode_DVIM_PCMxM;
		break;
	  case XA_AUDIO_DVI_S:
		if (xa_audio_hard_type & XA_AUDIO_BIGEND_MSK)
			new_snd->spec = 0;
		else	new_snd->spec = 1;
		new_snd->delta = XA_ADecode_DVIS_PCMxM;
		break;
	  case XA_AUDIO_IMA4_M:
		if (xa_audio_hard_type & XA_AUDIO_BIGEND_MSK)
			new_snd->spec = 0;
		else	new_snd->spec = 1;
		new_snd->delta = XA_ADecode_IMA4M_PCMxM;
		break;
	  case XA_AUDIO_IMA4_S:
		if (xa_audio_hard_type & XA_AUDIO_BIGEND_MSK)
			new_snd->spec = 0;
		else	new_snd->spec = 1;
		new_snd->delta = XA_ADecode_IMA4S_PCMxM;
		break;
#ifdef XA_GSM
	  case XA_AUDIO_GSM:
	  case XA_AUDIO_MSGSM:
		if (xa_audio_hard_type & XA_AUDIO_BIGEND_MSK)
			new_snd->spec = 0;
		else	new_snd->spec = 1;
		if (itype == XA_AUDIO_MSGSM_M) new_snd->spec |= 0x80;
		new_snd->delta = XA_ADecode_GSMM_PCMxM;
		break;
#endif
	  case XA_AUDIO_NOP:
		if (xa_audio_hard_type & XA_AUDIO_BIGEND_MSK)
			new_snd->spec = 0;
		else	new_snd->spec = 1;
		new_snd->delta = XA_ADecode_NOP_PCMXM;
		break;
	  case XA_AUDIO_ULAWS:
		if (xa_audio_hard_type & XA_AUDIO_BIGEND_MSK)
			new_snd->spec = 0 | 8;
		else	new_snd->spec = 1 | 8;
		new_snd->delta = XA_ADecode_ULAWx_PCMxM;
		break;
	  case XA_AUDIO_ULAW:
		if (xa_audio_hard_type & XA_AUDIO_BIGEND_MSK)
			new_snd->spec = 0;
		else	new_snd->spec = 1;
		new_snd->delta = XA_ADecode_ULAWx_PCMxM;
		break;
	  case XA_AUDIO_ALAWS:
		if (xa_audio_hard_type & XA_AUDIO_BIGEND_MSK)
			new_snd->spec = 0 | 8;
		else	new_snd->spec = 1 | 8;
		new_snd->delta = XA_ADecode_ALAWx_PCMxM;
		break;
	  case XA_AUDIO_ALAW:
		if (xa_audio_hard_type & XA_AUDIO_BIGEND_MSK)
			new_snd->spec = 0;
		else	new_snd->spec = 1;
		new_snd->delta = XA_ADecode_ALAWx_PCMxM;
		break;
	  case XA_AUDIO_ARMLAWS:
		if (xa_audio_hard_type & XA_AUDIO_BIGEND_MSK)
			new_snd->spec = 0 | 8;
		else	new_snd->spec = 1 | 8;
		new_snd->delta = XA_ADecode_ARMLAWx_PCMxM;
		break;
	  case XA_AUDIO_ARMLAW:
		if (xa_audio_hard_type & XA_AUDIO_BIGEND_MSK)
			new_snd->spec = 0;
		else	new_snd->spec = 1;
		new_snd->delta = XA_ADecode_ARMLAWx_PCMxM;
		break;
	  default:
	fprintf(stderr,"F_AUDIO_SIN2M: Unsupported Software Type(%x)\n",
			itype);
		return(xaFALSE);
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
			new_snd->delta = XA_ADecode_PCMXS_PCM1M;
		else
			new_snd->delta = XA_ADecode_PCMXM_PCM1M;
		break;
	  case XA_AUDIO_ULAWS:
		if (xa_audio_hard_type & XA_AUDIO_BIGEND_MSK)
			new_snd->spec = 0 | 4 | 8;
		else	new_snd->spec = 1 | 4 | 8;
		new_snd->delta = XA_ADecode_ULAWx_PCMxM;
		break;
	  case XA_AUDIO_ULAW:
		if (xa_audio_hard_type & XA_AUDIO_BIGEND_MSK)
			new_snd->spec = 0 | 4;
		else	new_snd->spec = 1 | 4;
		new_snd->delta = XA_ADecode_ULAWx_PCMxM;
		break;
	  case XA_AUDIO_ALAWS:
		if (xa_audio_hard_type & XA_AUDIO_BIGEND_MSK)
			new_snd->spec = 0 | 4 | 8;
		else	new_snd->spec = 1 | 4 | 8;
		new_snd->delta = XA_ADecode_ALAWx_PCMxM;
		break;
	  case XA_AUDIO_ALAW:
		if (xa_audio_hard_type & XA_AUDIO_BIGEND_MSK)
			new_snd->spec = 0 | 4;
		else	new_snd->spec = 1 | 4;
		new_snd->delta = XA_ADecode_ALAWx_PCMxM;
		break;
	  case XA_AUDIO_ARMLAWS:
		if (xa_audio_hard_type & XA_AUDIO_BIGEND_MSK)
			new_snd->spec = 0 | 4 | 8;
		else	new_snd->spec = 1 | 4 | 8;
		new_snd->delta = XA_ADecode_ARMLAWx_PCMxM;
		break;
	  case XA_AUDIO_ARMLAW:
		if (xa_audio_hard_type & XA_AUDIO_BIGEND_MSK)
			new_snd->spec = 0 | 4;
		else	new_snd->spec = 1 | 4;
		new_snd->delta = XA_ADecode_ARMLAWx_PCMxM;
		break;
	  case XA_AUDIO_ADPCM_M:
		new_snd->spec = 2 | 4;  /* 1 byte output */
		new_snd->delta = XA_ADecode_ADPCMM_PCM2M;
		break;
	  case XA_AUDIO_ADPCM_S:
		new_snd->spec = 2 | 4;  /* 1 byte output */
		new_snd->delta = XA_ADecode_ADPCMS_PCM2M;
		break;
	  case XA_AUDIO_DVI_M:
		new_snd->spec = 2 | 4;  /* 1 byte output */
		new_snd->delta = XA_ADecode_DVIM_PCMxM;
		break;
	  case XA_AUDIO_DVI_S:
		new_snd->spec = 2 | 4;  /* 1 byte output */
		new_snd->delta = XA_ADecode_DVIS_PCMxM;
		break;
	  case XA_AUDIO_IMA4_M:
		new_snd->spec = 2 | 4;  /* 1 byte output */
		new_snd->delta = XA_ADecode_IMA4M_PCMxM;
		break;
	  case XA_AUDIO_IMA4_S:
		new_snd->spec = 2 | 4;  /* 1 byte output */
		new_snd->delta = XA_ADecode_IMA4S_PCMxM;
		break;
#ifdef XA_GSM
	  case XA_AUDIO_GSM:
	  case XA_AUDIO_MSGSM:
		new_snd->spec = 2 | 4;  /* 1 byte output */
		if (itype == XA_AUDIO_MSGSM_M) new_snd->spec |= 0x80;
		new_snd->delta = XA_ADecode_GSMM_PCMxM;
		break;
#endif
	  case XA_AUDIO_NOP:
		new_snd->spec = 2 | 4;  /* 1 byte output */
		new_snd->delta = XA_ADecode_NOP_PCMXM;
		break;
	  default:
		fprintf(stderr,"AUDIO_LIN1M: Unsupported Software Type\n");
		return(xaFALSE);
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
		new_snd->delta = XA_ADecode_PCM1M_PCM2M;
		break;
	  case XA_AUDIO_SIGNED_1M:      /* SIN1M -> LIN2M*  */
		if (xa_audio_hard_type & XA_AUDIO_BIGEND_MSK)
			new_snd->spec = 1;
		else    new_snd->spec = 2;
		new_snd->delta = XA_ADecode_PCM1M_PCM2M;
		break;
	  case XA_AUDIO_LINEAR_1S:      /* LIN1S -> LIN2M*  */
		if (xa_audio_hard_type & XA_AUDIO_BIGEND_MSK)
			new_snd->spec = 0;
		else    new_snd->spec = 0;
		new_snd->delta = XA_ADecode_PCM1S_PCMxM;
		break;
	  case XA_AUDIO_SIGNED_1S:      /* SIN1S -> LIN2M*  */
		if (xa_audio_hard_type & XA_AUDIO_BIGEND_MSK)
			new_snd->spec = 5;
		else    new_snd->spec = 6;
		new_snd->delta = XA_ADecode_PCM1S_PCMxM;
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
		new_snd->delta = XA_ADecode_PCM2X_PCM2M;
		break;
	  case XA_AUDIO_ADPCM_M:
		if (xa_audio_hard_type & XA_AUDIO_BIGEND_MSK)
			new_snd->spec = 2;
		else	new_snd->spec = 3;
		new_snd->delta = XA_ADecode_ADPCMM_PCM2M;
		break;
	  case XA_AUDIO_ADPCM_S:
		if (xa_audio_hard_type & XA_AUDIO_BIGEND_MSK)
			new_snd->spec = 2;
		else	new_snd->spec = 3;
		new_snd->delta = XA_ADecode_ADPCMS_PCM2M;
		break;
	  case XA_AUDIO_DVI_M:
		if (xa_audio_hard_type & XA_AUDIO_BIGEND_MSK)
			new_snd->spec = 2;
		else	new_snd->spec = 3;
		new_snd->delta = XA_ADecode_DVIM_PCMxM;
		break;
	  case XA_AUDIO_DVI_S:
		if (xa_audio_hard_type & XA_AUDIO_BIGEND_MSK)
			new_snd->spec = 2;
		else	new_snd->spec = 3;
		new_snd->delta = XA_ADecode_DVIS_PCMxM;
		break;
	  case XA_AUDIO_IMA4_M:
		if (xa_audio_hard_type & XA_AUDIO_BIGEND_MSK)
			new_snd->spec = 2;
		else	new_snd->spec = 3;
		new_snd->delta = XA_ADecode_IMA4M_PCMxM;
		break;
	  case XA_AUDIO_IMA4_S:
		if (xa_audio_hard_type & XA_AUDIO_BIGEND_MSK)
			new_snd->spec = 2;
		else	new_snd->spec = 3;
		new_snd->delta = XA_ADecode_IMA4S_PCMxM;
		break;
#ifdef XA_GSM
	  case XA_AUDIO_GSM:
	  case XA_AUDIO_MSGSM:
		if (xa_audio_hard_type & XA_AUDIO_BIGEND_MSK)
			new_snd->spec = 2;
		else	new_snd->spec = 3;
		if (itype == XA_AUDIO_MSGSM_M) new_snd->spec |= 0x80;
		new_snd->delta = XA_ADecode_GSMM_PCMxM;
		break;
#endif
	  case XA_AUDIO_NOP:
		if (xa_audio_hard_type & XA_AUDIO_BIGEND_MSK)
			new_snd->spec = 2;
		else	new_snd->spec = 3;
		new_snd->delta = XA_ADecode_NOP_PCMXM;
		break;
	  default:
		fprintf(stderr,"AUDIO_LIN2M: Unsupported Software Type\n");
		return(xaFALSE);
		break;
	}
      }
      break;


    default: 
      FREE(new_snd,0x507); new_snd = 0;
      fprintf(stderr,"AUDIO: Unknown Hardware Type\n");
      return(xaFALSE);
      break;
  }

  /** Set up prev pointer */
  if (aud_hdr->first_snd==0) new_snd->prev = 0;
  else new_snd->prev = aud_hdr->last_snd;

  /** Set up next pointer */
  if (aud_hdr->first_snd == 0)	aud_hdr->first_snd = new_snd;
  if (aud_hdr->last_snd)	aud_hdr->last_snd->next = new_snd;
  aud_hdr->last_snd = new_snd;
  return(xaTRUE);
}
