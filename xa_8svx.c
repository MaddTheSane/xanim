
/*
 * xa_8svx.c
 *
 * Copyright (C) 1998 by Andreas Micklei
 * based on xa_wav.c
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

/*******************************
 * Revision
 *
 ********************************/


/*#include "xa_avi.h"*/
/*#include "xa_iff.h" consider it */
#include "xanim.h"
#include "xa_8svx.h"

#define XA_ID_8SVX	0x38535658

/* NOTE: Also many varibale names here start with "svx" they should really
 * start with 8svx but the compiler might not like this.
 */

/* Forward declarations */
xaULONG SVX_Read_File();
static void SVX_Read_VHDR();
static void SVX_Read_Chunk_Header();
static void SVX_Print_VHDR();

extern void IFF_Print_ID(); /* borrowed from xa_iff.c */

/* Leave this in, until I know, what it is for
static AUDS_HDR auds_hdr;

static xaULONG wav_max_faud_size;
static xaULONG wav_snd_time,wav_snd_timelo;
static xaULONG wav_audio_type;
xaULONG XA_Add_Sound();
*/

extern xaULONG XA_Add_Sound();

static Chunk_Header svx_form_hdr;	/* FORM Header */
#ifdef NOT_USED
static Chunk_Header svx_vhdr_hdr;	/* VHDR Header */
#endif
static Voice8Header svx_vhdr;		/* VHDR */
static xaULONG svx_snd_time, svx_snd_timelo;

xaULONG SVX_Read_File(fname,anim_hdr,audio_attempt)
char *fname;
XA_ANIM_HDR *anim_hdr;
xaULONG audio_attempt;    /* xaTRUE if audio is to be attempted */
{
	XA_INPUT	*xin = anim_hdr->xin;
	xaLONG		svx_type;	/* must be 8SVX */
	Chunk_Header	chdr;		/* temporary chunk header storage */
	xaLONG		svx_size;	/* size countdown variable */
	xaBYTE		*body;		/* pointer to body */
	xaLONG		the_snd_size;
	xaULONG		max_snd_chunk_size = XA_SND_CHUNK_SIZE;
	xaBYTE		*snd;
	xaLONG		snd_size;

	/* 8svx samples are audio only files */
	if (audio_attempt == xaFALSE) return(xaFALSE);

	/* initialization */

	/* Read Form Header and type */
	SVX_Read_Chunk_Header(xin, &svx_form_hdr);
	svx_type = xin->Read_MSB_U32(xin);
DEBUG_LEVEL1
{
	fprintf(stdout,"SVX Audio type: ");
	IFF_Print_ID(stdout, svx_type);
	fprintf(stdout," size: %08x\n",svx_form_hdr.size);
}
	if (svx_type != XA_ID_8SVX) {
		fprintf(stdout,"8SVX: Unsupported Audio type: ");
		IFF_Print_ID(stdout, svx_type);
		fprintf(stdout,"\n");
		return(xaFALSE);
	}

	/* Read Chunks */
	svx_size = svx_form_hdr.size;
	while(svx_size >= 8) {
		SVX_Read_Chunk_Header(xin, &chdr);
		svx_size -= 8;

		switch(chdr.id) {
			/* Read Voice8Header */
			case VHDR:
			SVX_Read_VHDR(xin, &svx_vhdr);
DEBUG_LEVEL1
{
			fprintf(stdout,"8SVX: ---VHDR---\n");
			SVX_Print_VHDR(stdout,&svx_vhdr);
			fprintf(stdout,"8SVX: ----------\n");
}
			break;

			/* Read sample data (always last chunk in file) */
			case BODY:
DEBUG_LEVEL1		fprintf(stdout,"8SVX: BODY length: %i\n",chdr.size);
			svx_size = chdr.size;
			if (chdr.size & 1)	/* odd byte padding */
				svx_size++;
			if ((body = malloc(svx_size)) == NULL) {
				fprintf(stderr,"Out of memory");
				return(xaFALSE);
			}
			xin->Read_Block(xin,body,chdr.size);
			if (chdr.size & 1)	/* pad byte init */
				body[svx_size-1]=0;
			break;
			
			/* Ignore unknown/unimportant chunks */
			default:
DEBUG_LEVEL1
{
			fprintf(stdout,"8SVX: Unknown chunk: ");
			IFF_Print_ID(stdout, chdr.id);
			fprintf(stdout,"\n");
}
			xin->Seek_FPos(xin,chdr.size,1);	/* skip chunk */
		} /* end of switch(chdr.id) */
		svx_size -= chdr.size;
		/* odd byte padding */
		if (chdr.size & 1)
			svx_size -= 1;
	} /* end of Read Chunks */

	/* Hand sample data to audio Subsystem */
	the_snd_size = svx_vhdr.oneShotHiSamples;
	if (the_snd_size & 1) the_snd_size++;
	snd = body;
	snd_size = the_snd_size;
	while(the_snd_size) {
		/* Calculate length of chunk to hand to XA_Add_Sound */
		/* This is commented out because xanim will core dump
		 * otherwise (most of the time). why?*/
		if (snd_size > max_snd_chunk_size)
			snd_size = max_snd_chunk_size;
		if (snd_size > the_snd_size)
			snd_size = the_snd_size;
		the_snd_size -= snd_size;

		/* Sample is already in memory, so hand it to XA_Add_Sound
		 * without calculating file positions, etc. */
		XA_Add_Sound(anim_hdr,snd,XA_AUDIO_SIGNED_1M, -1,
			svx_vhdr.samplesPerSec, snd_size, &svx_snd_time,
			&svx_snd_timelo,
			1,1);
		snd += snd_size;
	} /* end of while */

	/* Calculate total time of animation. Is this neccessary? */
	if (svx_vhdr.samplesPerSec) 
		anim_hdr->total_time = (xaULONG)
			(((float)the_snd_size * 1000.0) 
			/ (float)svx_vhdr.samplesPerSec);
	else	anim_hdr->total_time = 0;

	/* Clean up */
	/*free(body);*/	/* Am I supposed to do this? if not, who does it? */

	return(xaTRUE);
} /* end of read file */

void SVX_Read_VHDR(xin, vhdr)
	XA_INPUT *xin;
	Voice8Header *vhdr;
{

	vhdr->oneShotHiSamples = xin->Read_MSB_U32(xin);
	vhdr->repeatHiSamples = xin->Read_MSB_U32(xin);
	vhdr->samplesPerHiCycle = xin->Read_MSB_U32(xin);
	vhdr->samplesPerSec = xin->Read_MSB_U16(xin);
	vhdr->ctOctave = xin->Read_U8(xin);
	vhdr->sCompression = xin->Read_U8(xin);
	vhdr->volume = xin->Read_MSB_U32(xin);
}

void SVX_Read_Chunk_Header(xin, ch)
	XA_INPUT *xin;
	Chunk_Header *ch;
{
	ch->id = xin->Read_MSB_U32(xin);
	ch->size = xin->Read_MSB_U32(xin);
}

void SVX_Print_VHDR(fout, vhdr)
	FILE *fout;
	Voice8Header *vhdr;
{
	fprintf(fout,"VHDR: oneShotHiSamples: %i\n", vhdr->oneShotHiSamples);
	fprintf(fout,"VHDR: repeatHiSamples: %i\n", vhdr->repeatHiSamples);
	fprintf(fout,"VHDR: samplesPerHiCycle: %i\n", vhdr->samplesPerHiCycle);
	fprintf(fout,"VHDR: samplesPerSec: %i\n", vhdr->samplesPerSec);
	fprintf(fout,"VHDR: ctOctave: %i\n", vhdr->ctOctave);
	fprintf(fout,"VHDR: sCompression: %i\n", vhdr->sCompression);
	fprintf(fout,"VHDR: volume: %i\n", vhdr->volume);
}

