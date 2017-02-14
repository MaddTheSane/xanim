
/*
 * xa_8svx.h
 *
 * Copyright (C) 1998 by Andreas Micklei.
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

/* this one could be in xanim.h, we used to adapt the IFF 8SVX spec with
 * minimal changes
 */
#define xaUWORD xaUSHORT

typedef struct 
{
 xaLONG id;
 xaLONG size;
} Chunk_Header;

/* Grouping Stuff (unsupported) */
#define LIST 0x4c495354
#define PROP 0x50524f50

/* Sound stuff */
#define FORM 0x464f524d
#define BODY 0x424f4459	
#define VHDR 0x56484452		/* Voice8Header */
#define COPY 0x28632920		/* "(c) " */
#define	AUTH 0x41555448		/* Author */
#define ANNO 0x414e4e4f		/* Annotation */
#define ATAK 0x4154414b		/* hope we never encounter this */
#define RLSE 0x524C5345		/* nor this */
#define CHAN 0x4348414e		/* Stereo extension (ignored) */
#define PAN  0x50414e20		/* Panning extension (ignored) */
#define SEQN 0x5345514e		/* Multi Loop extension (ignored) */
#define FADE 0x46414445		/* Fading extension (ignored) */


/* Shamelessly stolen from the IFF 8SVX standard definition
 * look there for additional comments
 */

/* sCompression */
#define sCmpNone	0
#define sCmpFibDelta	1	/* presently not supported */

typedef struct
{
	xaULONG	oneShotHiSamples,
		repeatHiSamples,
		samplesPerHiCycle;
	xaUWORD	samplesPerSec;
	xaUBYTE	ctOctave,
		sCompression;
	xaLONG	volume;		/* fixed point value */
} Voice8Header;

extern xaULONG SVX_Read_File();

