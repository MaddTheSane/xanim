
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

typedef int             xaLONG;
typedef unsigned int    xaULONG;
typedef short           xaSHORT;
typedef unsigned short  xaUSHORT;
typedef char            xaBYTE;
typedef unsigned char   xaUBYTE;

#define xaxaTRUE  1
#define xaxaFALSE 0



/*----------- UnPackRow ------------------------------------------------*/

#define UGetByte()	(*source++)
#define UPutByte(c)	(*dest++ = (c))

/* Given POINTERS to POINTER variables, unpacks one row, updating the source
 * and destination pointers until it produces dstBytes bytes. */

xaLONG UnPackRow(pSource, pDest, srcBytes0, dstBytes0)
xaBYTE  **pSource, **pDest;  xaLONG *srcBytes0, *dstBytes0; 
{
  register xaBYTE *source = *pSource; 
  register xaBYTE *dest   = *pDest;
  register xaLONG n;
  register xaBYTE c;
  register xaLONG srcBytes = *srcBytes0, dstBytes = *dstBytes0;
  xaLONG error = xaxaTRUE; /* assume error until we make it through the loop */

  while( dstBytes > 0 )  
  {
    if ( (srcBytes -= 1) < 0 )  { error=1; goto ErrorExit; }
    n = UGetByte() & 0xff;

    if (!(n & 0x80)) 
    {
      n += 1;
      if ( (srcBytes -= n) < 0 )  {error=2; goto ErrorExit;}
      if ( (dstBytes -= n) < 0 )  {error=3; goto ErrorExit;}
      do {  UPutByte(UGetByte());  } while (--n > 0);
    }
    else if (n != 0x80) 
    {
      n = (256-n) + 1;
      if ( (srcBytes -= 1) < 0 )  {error=4; goto ErrorExit;}
      if ( (dstBytes -= n) < 0 )  {error=5; goto ErrorExit;}
      c = UGetByte();
      do 
      {
	UPutByte(c);
      } while (--n > 0);
    }
  }
  error = xaxaFALSE;	/* success! */

ErrorExit:
  *pSource = source;  *pDest = dest; 
   *srcBytes0 = srcBytes; *dstBytes0 = dstBytes;
   return(error);
}

