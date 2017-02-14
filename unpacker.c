
/*----------------------------------------------------------------------*
 * unpacker.c Convert data from "cmpByteRun1" run compression. 11/15/85
 *
 * By Jerry Morrison and Steve Shaw, Electronic Arts.
 * This software is in the public domain.
 *
 *	control bytes:
 *	 [0..127]   : followed by n+1 bytes of data.
 *	 [-1..-127] : followed by byte to be repeated (-n)+1 times.
 *	 -128       : NOOP.
 *
 * This version for the Commodore-Amiga computer.
 * Modified for use with Unix
 *----------------------------------------------------------------------*/
#include <stdio.h>
#define LONG int
#define ULONG unsigned int
#define BYTE char
#define UBYTE unsigned char
#define SHORT short
#define USHORT unsigned short
#define WORD short int
#define UWORD unsigned short int

#define TRUE 1
#define FALSE 0



/*----------- UnPackRow ------------------------------------------------*/

#define UGetByte()	(*source++)
#define UPutByte(c)	(*dest++ = (c))

/* Given POINTERS to POINTER variables, unpacks one row, updating the source
 * and destination pointers until it produces dstBytes bytes. */

LONG UnPackRow(pSource, pDest, srcBytes0, dstBytes0)
BYTE  **pSource, **pDest;  LONG *srcBytes0, *dstBytes0; 
{
    register BYTE *source = *pSource; 
    register BYTE *dest   = *pDest;
    register LONG n;
    register BYTE c;
    register LONG srcBytes = *srcBytes0, dstBytes = *dstBytes0;
    LONG error = TRUE;	/* assume error until we make it through the loop */

    while( dstBytes > 0 )  {
	if ( (srcBytes -= 1) < 0 )  {error=1; goto ErrorExit;}
    	n = UGetByte() & 0xff;

    	if (!(n & 0x80)) {
	    n += 1;
	    if ( (srcBytes -= n) < 0 )  {error=2; goto ErrorExit;}
	    if ( (dstBytes -= n) < 0 )  {error=3; goto ErrorExit;}
	    do {  UPutByte(UGetByte());  } while (--n > 0);
	    }

    	else if (n != 0x80) {
	    n = (256-n) + 1;
	    if ( (srcBytes -= 1) < 0 )  {error=4; goto ErrorExit;}
	    if ( (dstBytes -= n) < 0 )  {error=5; goto ErrorExit;}
	    c = UGetByte();
	    do {  UPutByte(c);  } while (--n > 0);
	    }
	}
    error = FALSE;	/* success! */

  ErrorExit:
    *pSource = source;  *pDest = dest; 
    *srcBytes0 = srcBytes; *dstBytes0 = dstBytes;
    return(error);
    }

