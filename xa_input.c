
/*
 * xa_input.c
 *
 * Copyright (C) 1996,1998,1999 by Mark Podlipec.
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
/* REVISIONS ***********
 * 17Mar96  changed bcopy to be memcpy because many machines don't
 *          support bcopy.
 * 17May96  Since VMS compilers are brain-dead :^) well okay, they're
 *          just case insensitive. I needed to rename xa_ftp_open_file
 *          to be xaftp_open_file to avoid conflict with xa_FTP_Open_File. 
 *	    Also, redid the include files for VMS.
 */

#include "xanim.h"

#include <errno.h>

#ifdef VMS
#include <types.h>
#include <socket.h>
#include <string.h>
#include <netdb.h>
#include <stdlib.h>
#include <in.h>
#include <unixio.h>
#include <stdlib.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netdb.h>
#include <stdlib.h>
#include <netinet/in.h>
#endif


xaULONG XA_Buff_Read_U8();
xaULONG XA_Buff_Read_LSB_U16();
xaULONG XA_Buff_Read_MSB_U16();
xaULONG XA_Buff_Read_LSB_U32();
xaULONG XA_Buff_Read_MSB_U32();
xaULONG XA_Buff_Read_Block();

void XA_Set_EOF();

xaULONG XA_STDIN_Open_File();
xaULONG XA_STDIN_Close_File();
xaULONG XA_STDIN_Read_U8();
xaULONG XA_STDIN_Read_LSB_U16();
xaULONG XA_STDIN_Read_MSB_U16();
xaULONG XA_STDIN_Read_LSB_U32();
xaULONG XA_STDIN_Read_MSB_U32();
xaLONG  XA_STDIN_Read_Block();
xaLONG  XA_STDIN_EOF();
xaLONG  XA_STDIN_seek();
xaLONG  XA_STDIN_ftell();

xaULONG XA_FILE_Open_File();
xaULONG XA_FILE_Close_File();
xaULONG XA_FILE_Read_U8();
xaULONG XA_FILE_Read_LSB_U16();
xaULONG XA_FILE_Read_MSB_U16();
xaULONG XA_FILE_Read_LSB_U32();
xaULONG XA_FILE_Read_MSB_U32();
xaLONG  XA_FILE_Read_Block();
xaLONG  XA_FILE_EOF();
xaLONG  XA_FILE_seek();
xaLONG  XA_FILE_ftell();

xaULONG XA_Mem_Open_Init();
xaULONG XA_MEM_Open_File();
xaULONG XA_MEM_Close_File();
xaULONG XA_MEM_Read_U8();
xaULONG XA_MEM_Read_LSB_U16();
xaULONG XA_MEM_Read_MSB_U16();
xaULONG XA_MEM_Read_LSB_U32();
xaULONG XA_MEM_Read_MSB_U32();
xaLONG  XA_MEM_Read_Block();
xaLONG  XA_MEM_EOF();
xaLONG  XA_MEM_seek();
xaLONG  XA_MEM_ftell();

xaULONG XA_FTP_Open_File();
xaULONG XA_FTP_Close_File();
xaULONG XA_FTP_Read_U8();
xaULONG XA_FTP_Read_LSB_U16();
xaULONG XA_FTP_Read_MSB_U16();
xaULONG XA_FTP_Read_LSB_U32();
xaULONG XA_FTP_Read_MSB_U32();
xaLONG  XA_FTP_Read_Block();
xaLONG  XA_FTP_EOF();
xaLONG  XA_FTP_seek();
xaLONG  XA_FTP_ftell();
xaLONG	xa_ftp_get_msg();
xaLONG	xa_ftp_send_cmd();
xaLONG	xaftp_open_file();

xaULONG XA_File_Split();

#define BUFF_FILL_SIZE 0x20

xaULONG XA_Alloc_Input_Methods(anim_hdr)
XA_ANIM_HDR *anim_hdr;
{ XA_INPUT *xin;

	/* allocate */
  if (anim_hdr->xin == 0)
  {
    xin = (XA_INPUT *)malloc(sizeof(XA_INPUT));
    if (xin == 0)  TheEnd1("XA_INPUT: malloc err\n");
    anim_hdr->xin = xin;
    xin->buf = 0;
  }
  return(xaTRUE);
}

xaULONG XA_Setup_Input_Methods(xin,file)
XA_INPUT *xin;
char *file;
{
  xin->file = file;

  if (xin->buf == 0)
  {
    xin->buf_size	= BUFF_FILL_SIZE;
    xin->buf = (xaUBYTE *)malloc(xin->buf_size);
    if (xin->buf == 0) TheEnd1("stdin buf malloc err");
  }
  xin->buf_ptr	= xin->buf;
  xin->buf_cnt	= 0;
  xin->fpos		= 0;
  xin->eof		= 0;
  xin->load_flag	= XA_IN_LOAD_MEM; /* default for now */

  if ( strncmp(file,"ftp://",6) == 0)
  { xin->type_flag	= XA_IN_TYPE_SEQUENTIAL; 
    xin->Open_File	= XA_FTP_Open_File;
    xin->Close_File	= XA_FTP_Close_File;
    xin->Read_U8	= XA_FTP_Read_U8;
    xin->Read_LSB_U16	= XA_FTP_Read_LSB_U16;
    xin->Read_MSB_U16	= XA_FTP_Read_MSB_U16;
    xin->Read_LSB_U32	= XA_FTP_Read_LSB_U32;
    xin->Read_MSB_U32	= XA_FTP_Read_MSB_U32;
    xin->Read_Block	= XA_FTP_Read_Block;
    xin->Set_EOF	= XA_Set_EOF;
    xin->At_EOF		= XA_FTP_EOF;
    xin->Seek_FPos	= XA_FTP_seek;
    xin->Get_FPos	= XA_FTP_ftell;
    return(xaTRUE);
  }
  else if ( strcmp(file,"-") == 0)
  { xin->type_flag	= XA_IN_TYPE_SEQUENTIAL; 
    xin->Open_File	= XA_STDIN_Open_File;
    xin->Close_File	= XA_STDIN_Close_File;
    xin->Read_U8	= XA_STDIN_Read_U8;
    xin->Read_LSB_U16	= XA_STDIN_Read_LSB_U16;
    xin->Read_MSB_U16	= XA_STDIN_Read_MSB_U16;
    xin->Read_LSB_U32	= XA_STDIN_Read_LSB_U32;
    xin->Read_MSB_U32	= XA_STDIN_Read_MSB_U32;
    xin->Read_Block	= XA_STDIN_Read_Block;
    xin->Set_EOF	= XA_Set_EOF;
    xin->At_EOF		= XA_STDIN_EOF;
    xin->Seek_FPos	= XA_STDIN_seek;
    xin->Get_FPos	= XA_STDIN_ftell;
    return(xaTRUE);
  }
  else
  { xin->type_flag	= XA_IN_TYPE_RANDOM;
    xin->Open_File	= XA_FILE_Open_File;
    xin->Close_File	= XA_FILE_Close_File;
    xin->Read_U8	= XA_FILE_Read_U8;
    xin->Read_LSB_U16	= XA_FILE_Read_LSB_U16;
    xin->Read_MSB_U16	= XA_FILE_Read_MSB_U16;
    xin->Read_LSB_U32	= XA_FILE_Read_LSB_U32;
    xin->Read_MSB_U32	= XA_FILE_Read_MSB_U32;
    xin->Read_Block	= XA_FILE_Read_Block;
    xin->Set_EOF	= XA_Set_EOF;
    xin->At_EOF		= XA_FILE_EOF;
    xin->Seek_FPos	= XA_FILE_seek;
    xin->Get_FPos	= XA_FILE_ftell;
    return(xaTRUE);
  }
}

xaULONG XA_Mem_Open_Init(xin,buf,buf_sz)
XA_INPUT *xin;
unsigned char *buf;
xaULONG buf_sz;
{
  xin->file		= "mem";
  xin->buf		= buf;
  xin->buf_size		= buf_sz;
  xin->buf_ptr		= xin->buf;
  xin->buf_cnt		= xin->buf_size;
  xin->fpos		= 0;
  xin->eof		= 0;
  xin->load_flag	= XA_IN_LOAD_MEM; /* default for now */
  xin->type_flag	= XA_IN_TYPE_RANDOM;
  xin->Open_File	= XA_MEM_Open_File;
  xin->Close_File	= XA_MEM_Close_File;
  xin->Read_U8		= XA_MEM_Read_U8;
  xin->Read_LSB_U16	= XA_MEM_Read_LSB_U16;
  xin->Read_MSB_U16	= XA_MEM_Read_MSB_U16;
  xin->Read_LSB_U32	= XA_MEM_Read_LSB_U32;
  xin->Read_MSB_U32	= XA_MEM_Read_MSB_U32;
  xin->Read_Block	= XA_MEM_Read_Block;
  xin->Set_EOF		= XA_Set_EOF;
  xin->At_EOF		= XA_MEM_EOF;
  xin->Seek_FPos	= XA_MEM_seek;
  xin->Get_FPos		= XA_MEM_ftell;
  return(xaTRUE);
}

/***************************************************************************/
/********    Buffer Routines(Only used for Sequential Streams)    **********/
/***************************************************************************/

/* This is used to read the 1st section of the file so that we may
 * hopefully determine just what in the world it is(Not always
 * possible(thank you so very much Quicktime))
 */
xaLONG XA_BUF_Init_Fill(xin,len)
XA_INPUT *xin;
xaULONG len;
{ xaLONG ret;
  if (len > xin->buf_size) return(0); /*POD eventually bust up buf size*/
  xin->buf_cnt = 0;
  xin->buf_ptr = xin->buf;
  ret = xin->Read_Block(xin,xin->buf,len);
  if (ret < len) xin->err_flag |= XA_IN_ERR_ERR;
  xin->buf_cnt = len;
  xin->fpos = 0; /* keep this at 0 */
  return( ret );
}


/* The following BUF routines are only for sequential streams and that
 * is to drain input already read in and stored in the buffer. Random
 * access devices, we simply seek back to fpos 0 
 */
 
#define XA_BUF_U8(xin) ((xaULONG)(*xin->buf_ptr++))

#define XA_BUF_INC(xin) \
{ xin->buf_cnt--; if (xin->buf_cnt == 0)  xin->buf_ptr = 0; }

#define XA_BUF_VAL(xin)  xin->buf_cnt

/***************************************************
 * The XA_BUF_Read routines are never to be called directly. Only
 * by other input routines when xin->buf_cnt > 0.
 *
 **********/ 
xaULONG XA_BUF_Read_U8(xin)
XA_INPUT *xin;
{ xaULONG ret = XA_BUF_U8(xin); XA_BUF_INC(xin); return(ret);
}

xaULONG XA_BUF_Read_LSB_U16(xin)
XA_INPUT *xin;
{ xaULONG ret = XA_BUF_U8(xin);	XA_BUF_INC(xin);
  if (XA_BUF_VAL(xin)) { ret |= XA_BUF_U8(xin) << 8; XA_BUF_INC(xin);}
  else { ret |= xin->Read_U8(xin) << 8; xin->fpos--; }
  return(ret);
}

xaULONG XA_BUF_Read_MSB_U16(xin)
XA_INPUT *xin;
{ xaULONG ret = XA_BUF_U8(xin) << 8;	XA_BUF_INC(xin);
  if (XA_BUF_VAL(xin)) { ret |= XA_BUF_U8(xin); XA_BUF_INC(xin);}
  else { ret |= xin->Read_U8(xin); xin->fpos--; }
  return(ret);
}

xaULONG XA_BUF_Read_LSB_U32(xin)
XA_INPUT *xin;
{ xaULONG ret = XA_BUF_U8(xin);	XA_BUF_INC(xin);
  if (XA_BUF_VAL(xin)) { ret |= XA_BUF_U8(xin) << 8; XA_BUF_INC(xin);}
  else { ret |= xin->Read_U8(xin) << 8; xin->fpos--; }
  if (XA_BUF_VAL(xin)) { ret |= XA_BUF_U8(xin) << 16; XA_BUF_INC(xin);}
  else { ret |= xin->Read_U8(xin) << 16; xin->fpos--; }
  if (XA_BUF_VAL(xin)) { ret |= XA_BUF_U8(xin) << 24; XA_BUF_INC(xin);}
  else { ret |= xin->Read_U8(xin) << 24; xin->fpos--; }
  return(ret);
}

xaULONG XA_BUF_Read_MSB_U32(xin)
XA_INPUT *xin;
{ xaULONG ret = XA_BUF_U8(xin) << 24;	XA_BUF_INC(xin);
  if (XA_BUF_VAL(xin)) { ret |= XA_BUF_U8(xin) << 16; XA_BUF_INC(xin);}
  else { ret |= xin->Read_U8(xin) << 16; xin->fpos--; }
  if (XA_BUF_VAL(xin)) { ret |= XA_BUF_U8(xin) << 8; XA_BUF_INC(xin);}
  else { ret |= xin->Read_U8(xin) << 8; xin->fpos--; }
  if (XA_BUF_VAL(xin)) { ret |= XA_BUF_U8(xin); XA_BUF_INC(xin);}
  else { ret |= xin->Read_U8(xin); xin->fpos--; }
  return(ret);
}

xaLONG XA_BUF_Read_Block(xin,ptr,len)
XA_INPUT *xin;
xaUBYTE *ptr;
xaULONG len;
{ xaULONG tmp_len = len;
  while( xin->buf_cnt && len)
	{  *ptr++ = *xin->buf_ptr++; xin->buf_cnt--; len--; }  
  if (len) { xin->fpos -= len; return(xin->Read_Block(xin,ptr,len)); }
  else return(tmp_len);
}

/***************************************************************************/
/********    General INPUT ROUTINES                               **********/
/***************************************************************************/
void XA_Set_EOF(xin,len)
XA_INPUT *xin;
xaULONG len;
{ xin->eof = len;
}


/***************************************************************************/
/********    STDIN INPUT ROUTINES                                 **********/
/***************************************************************************/
xaULONG XA_STDIN_Open_File(xin)
XA_INPUT *xin;
{ xin->buf_ptr = xin->buf; xin->buf_cnt = 0; xin->fpos = 0;
  xin->err_flag = XA_IN_ERR_NONE;
  xin->fin = stdin;
  return(xaTRUE);
}

/***** */
xaULONG XA_STDIN_Close_File(xin)
XA_INPUT *xin;
{ return(xaTRUE);
}

/***** */
xaULONG XA_STDIN_Read_U8(xin)
XA_INPUT *xin;
{ xin->fpos++;
  if (xin->buf_cnt) return(XA_BUF_Read_U8(xin));
  else return(fgetc( xin->fin ));
}

/***** */
xaULONG XA_STDIN_Read_LSB_U16(xin)
XA_INPUT *xin;
{ xin->fpos += 2;
  if (xin->buf_cnt) return(XA_BUF_Read_LSB_U16(xin)); 
  else {	xaULONG ret = fgetc(xin->fin);
		ret |= fgetc(xin->fin) << 8; return(ret);}
}

/***** */
xaULONG XA_STDIN_Read_MSB_U16(xin)
XA_INPUT *xin;
{ xin->fpos += 2;
  if (xin->buf_cnt) return(XA_BUF_Read_MSB_U16(xin)); 
  else {	xaULONG ret = fgetc(xin->fin) << 8;
		ret |= fgetc(xin->fin); return(ret); }
}

/***** */
xaULONG XA_STDIN_Read_LSB_U32(xin)
XA_INPUT *xin;
{ xin->fpos += 4;
  if (xin->buf_cnt) return(XA_BUF_Read_LSB_U32(xin)); 
  else {	xaULONG ret = fgetc(xin->fin);	ret |= fgetc(xin->fin) << 8;
		ret |= fgetc(xin->fin) << 16;	ret |= fgetc(xin->fin) << 24;
		return(ret); }
}

/***** */
xaULONG XA_STDIN_Read_MSB_U32(xin)
XA_INPUT *xin;
{ xin->fpos += 4;
  if (xin->buf_cnt) return(XA_BUF_Read_MSB_U32(xin)); 
  else {xaULONG ret = fgetc(xin->fin)<<24;	ret |= fgetc(xin->fin) << 16;
		ret |= fgetc(xin->fin) << 8;	ret |= fgetc(xin->fin);
		return(ret); }
}

/***** */
xaLONG XA_STDIN_Read_Block(xin,ptr,len)
XA_INPUT *xin;
xaUBYTE *ptr;
xaULONG len;
{ xin->fpos += len;
  if (xin->buf_cnt) return(XA_BUF_Read_Block(xin,ptr,len));
  else {xaLONG ret = fread( (char *)(ptr), 1 , len, xin->fin);
	return(ret); }
}

/**************************
 *  Potential problem if File is shorter than initial buffer size 
 ********/
xaLONG XA_STDIN_EOF(xin,off)
XA_INPUT *xin;
xaULONG off;	/* not used */
{ xaLONG ret = feof((xin->fin)); if (ret) xin->err_flag |= XA_IN_ERR_EOF;
  return( ret );
}

/***** */
xaLONG XA_STDIN_ftell(xin)
XA_INPUT *xin;
{ return( xin->fpos );
}

/***** */
xaLONG XA_STDIN_seek(xin,fpos,flag)
XA_INPUT *xin;
xaLONG fpos;
xaLONG flag;
{ xaLONG ret = -1;
	/** Move Forward valid in Sequential Streams */
  if ( (flag == 1) && (fpos >= 0) )
  { xin->fpos += fpos;	ret = xin->fpos;
    while((fpos--) && !feof(xin->fin)) (void)(fgetc(xin->fin)); 
  }	/** Move til End kinda of a NOP, but with stdin we can do that */
  else if ( (flag == 2) && (fpos == 0) )
  { while(!feof(xin->fin)) { ret = getc(xin->fin); if (ret >= 0) xin->fpos++; }
    xin->err_flag |= XA_IN_ERR_EOF;
  }	/** Move beyond current position ok */
  else if ((flag == 0) && (fpos >= xin->fpos))
  { while(xin->fpos < fpos) { fgetc(xin->fin); xin->fpos++; } 
  }
  else	/** Otherwise - not possible - complain */
  {
    fprintf(stderr,"STDIN seek err: off = %d flag = %d  curpos %d\n",
			fpos,flag,xin->fpos);
    return(-1);
  }
  return(0);
}

/***************************************************************************/
/********    FILE INPUT ROUTINES                                  **********/
/***************************************************************************/

/***** */
xaULONG XA_FILE_Open_File(xin)
XA_INPUT *xin;
{ xin->buf_ptr = xin->buf;	xin->buf_cnt = 0;	xin->fpos = 0;
  xin->err_flag = XA_IN_ERR_NONE;
  xin->fin = fopen(xin->file, XA_OPEN_MODE);
  if (xin->fin == 0) return(xaFALSE);
  return(xaTRUE);
}

/***** */
xaULONG XA_FILE_Close_File(xin)
XA_INPUT *xin;
{ fclose(xin->fin);
  return(xaTRUE);
}


/***** */
xaULONG XA_FILE_Read_U8(xin)
XA_INPUT *xin;
{ xin->fpos++; 
  return( (fgetc(xin->fin)) & 0xff );
}

/***** */
xaULONG XA_FILE_Read_LSB_U16(xin)
XA_INPUT *xin;
{ xaULONG ret;			xin->fpos += 2; 
  ret  = ((fgetc(xin->fin)) & 0xff);
  ret |= ((fgetc(xin->fin)) & 0xff) << 8;
  return(ret);
}

/***** */
xaULONG XA_FILE_Read_MSB_U16(xin)
XA_INPUT *xin;
{ xaULONG ret;			xin->fpos += 2; 
  ret  = ((fgetc(xin->fin)) & 0xff) << 8;
  ret |= ((fgetc(xin->fin)) & 0xff);
  return(ret);
}

/***** */
xaULONG XA_FILE_Read_LSB_U32(xin)
XA_INPUT *xin;
{ xaULONG ret;			xin->fpos += 4; 
  ret  = ((fgetc(xin->fin)) & 0xff);
  ret |= ((fgetc(xin->fin)) & 0xff) << 8;
  ret |= ((fgetc(xin->fin)) & 0xff) << 16;
  ret |= ((fgetc(xin->fin)) & 0xff) << 24;
  return(ret);
}

/***** */
xaULONG XA_FILE_Read_MSB_U32(xin)
XA_INPUT *xin;
{ xaULONG ret;			xin->fpos += 4; 
  ret  = ((fgetc(xin->fin)) & 0xff) << 24;
  ret |= ((fgetc(xin->fin)) & 0xff) << 16;
  ret |= ((fgetc(xin->fin)) & 0xff) << 8;
  ret |= ((fgetc(xin->fin)) & 0xff);
  return(ret);
}

/***** */
xaLONG XA_FILE_Read_Block(xin,ptr,len)
XA_INPUT *xin;
xaUBYTE *ptr;
xaULONG len;
{ xaLONG ret;			xin->fpos += len;
  ret = fread(ptr,1,len,xin->fin);
  if (ret < len) xin->err_flag |= XA_IN_ERR_ERR;
  return(ret);
}

/***** */
xaLONG XA_FILE_EOF(xin,off)
XA_INPUT *xin;
xaULONG off;	/* not used */
{ return( feof(xin->fin) );
}

/***** */
xaLONG XA_FILE_ftell(xin)
XA_INPUT *xin;
{  return( ftell(xin->fin) ); 
}

/*****
 * flag  0 beginning, 1 current, 2 end 
 */
xaLONG XA_FILE_seek(xin,off,flag)
XA_INPUT *xin;
xaLONG off;
xaLONG flag;
{ return( fseek(xin->fin, off, flag) );
}


/***************************************************************************/
/********    Memory INPUT ROUTINES                                **********/
/***************************************************************************/

/***** */
xaULONG XA_MEM_Open_File(xin)
XA_INPUT *xin;
{ xin->buf_ptr = xin->buf;
  xin->buf_cnt = xin->buf_size;
  xin->fpos = 0;
  xin->err_flag = XA_IN_ERR_NONE;
  return(xaTRUE);
}

/***** */
xaULONG XA_MEM_Close_File(xin)
XA_INPUT *xin;
{ 
  return(xaTRUE);
}

/***** */
xaULONG XA_MEM_Read_U8(xin)
XA_INPUT *xin;
{ xaULONG ret;
  if (xin->buf_cnt < 1) { ret = 0xff; xin->err_flag |= XA_IN_ERR_EOF; }
  else { ret = *xin->buf_ptr++; xin->fpos++; xin->buf_cnt--; }
  return( ret );
}

/***** */
xaULONG XA_MEM_Read_LSB_U16(xin)
XA_INPUT *xin;
{ xaULONG ret;
  if (xin->buf_cnt < 2) { ret = 0xffff; xin->err_flag |= XA_IN_ERR_EOF; }
  else
  { ret = xin->buf_ptr[0]; ret |= xin->buf_ptr[1] << 8;
    xin->fpos += 2; xin->buf_cnt -= 2; xin->buf_ptr += 2;
  }
  return(ret);
}

/***** */
xaULONG XA_MEM_Read_MSB_U16(xin)
XA_INPUT *xin;
{ xaULONG ret;
  if (xin->buf_cnt < 2) { ret = 0xffff; xin->err_flag |= XA_IN_ERR_EOF; }
  else
  { ret = xin->buf_ptr[1]; ret |= xin->buf_ptr[0] << 8;
    xin->fpos += 2; xin->buf_cnt -= 2; xin->buf_ptr += 2;
  }
  return(ret);
}

/***** */
xaULONG XA_MEM_Read_LSB_U32(xin)
XA_INPUT *xin;
{ xaULONG ret;
  if (xin->buf_cnt < 4) { ret = 0xffffffff; xin->err_flag |= XA_IN_ERR_EOF; }
  else
  { ret  = xin->buf_ptr[0];
    ret |= xin->buf_ptr[1] << 8;
    ret |= xin->buf_ptr[2] << 16;
    ret |= xin->buf_ptr[3] << 24;
    xin->fpos += 4; xin->buf_cnt -= 4; xin->buf_ptr += 4;
  }
  return(ret);
}

/***** */
xaULONG XA_MEM_Read_MSB_U32(xin)
XA_INPUT *xin;
{ xaULONG ret;
  if (xin->buf_cnt < 4) { ret = 0xffffffff; xin->err_flag |= XA_IN_ERR_EOF; }
  else
  { ret  = xin->buf_ptr[3];
    ret |= xin->buf_ptr[2] << 8;
    ret |= xin->buf_ptr[1] << 16;
    ret |= xin->buf_ptr[0] << 24;
    xin->fpos += 4; xin->buf_cnt -= 4; xin->buf_ptr += 4;
  }
  return(ret);
}

/***** */
xaLONG XA_MEM_Read_Block(xin,ptr,len)
XA_INPUT *xin;
xaUBYTE *ptr;
xaULONG len;
{ 
  if (len > xin->buf_cnt)
	{ len = xin->buf_cnt; xin->err_flag |= XA_IN_ERR_EOF; }
  if (len)
  { memcpy(ptr, xin->buf_ptr, len);
    xin->fpos += len; xin->buf_cnt -= len; xin->buf_ptr += len;
  }
  return(len);
}

/***** */
xaLONG XA_MEM_EOF(xin,off)
XA_INPUT *xin;
xaULONG off;	/* not used */
{ 
  if (   (xin->buf_cnt == 0)
      || (xin->err_flag & XA_IN_ERR_EOF) ) return(1);
  return( 0 );
}

/***** */
xaLONG XA_MEM_ftell(xin)
XA_INPUT *xin;
{  return( xin->fpos ); 
}

/*****
 * flag  0 beginning, 1 current, 2 end 
 */
xaLONG XA_MEM_seek(xin,off,flag)
XA_INPUT *xin;
xaLONG off;
xaLONG flag;
{ 
  switch( flag )
  {
    case 0:  /* from beginning */
      if (off > xin->buf_size) off = xin->buf_size;
      break;
    case 1:  /* from current pos */
      off += xin->fpos;
      if (off > xin->buf_size) off = xin->buf_size;
      break;
    case 2:  /* from end */
      off = xin->buf_size - off;
      if (off < 0) off = 0;
      break;
    default:
      off = xin->fpos;
      break;
  }

  xin->buf_cnt = xin->buf_size - off;
  xin->buf_ptr = &xin->buf[ off ];
  xin->fpos = off;
  return(off);
}








/***************************************************************************/
/********    FTP INPUT ROUTINES                                  **********/
/***************************************************************************/

#define FTP_READ(xin,ptr,len,ret)					\
{ xaLONG cnt = 0; ret = 1;						\
  while((ret > 0) && (cnt < len))					\
  { ret = read(xin->dsock,ptr,1); if (ret <= 0) break;			\
    cnt++;  ptr++; }							\
  if (ret < 0)	{ xin->err_flag |= XA_IN_ERR_ERR; } 			\
  else { ret = cnt; if (ret == 0) xin->err_flag |= XA_IN_ERR_EOF; }	}
/* FOR DEBUG: fprintf(stderr,"FTP-TMP: errno %d\n",errno); */

/***** */
xaULONG XA_FTP_Read_U8(xin)
XA_INPUT *xin;
{ xin->fpos++;
  if (xin->buf_cnt) return(XA_BUF_Read_U8(xin));
  else
  { xaLONG ret; xaUBYTE *ptr = xin->buf;
    FTP_READ(xin,ptr,1,ret);   if (ret < 1) return(0);
    return( (xaULONG)(*xin->buf) );
  }
}

/***** */
xaULONG XA_FTP_Read_LSB_U16(xin)
XA_INPUT *xin;
{ xin->fpos += 2;
  if (xin->buf_cnt) return(XA_BUF_Read_LSB_U16(xin)); 
  else
  { xaLONG ret; xaUBYTE *ptr = xin->buf;
    FTP_READ(xin,ptr,2,ret);   if (ret < 2) return(0);
    return( (xaULONG)(xin->buf[0]) | ((xaULONG)(xin->buf[1])<<8) );
  }
}
/***** */
xaULONG XA_FTP_Read_MSB_U16(xin)
XA_INPUT *xin;
{ xin->fpos += 2;
  if (xin->buf_cnt) return(XA_BUF_Read_MSB_U16(xin));
  else
  { xaLONG ret; xaUBYTE *ptr = xin->buf;
    FTP_READ(xin,ptr,2,ret);   if (ret < 2) return(0);
    return( (xaULONG)(xin->buf[1]) | ((xaULONG)(xin->buf[0])<<8) );
  }
}
/***** */
xaULONG XA_FTP_Read_LSB_U32(xin)
XA_INPUT *xin;
{ xin->fpos += 4;
  if (xin->buf_cnt) return(XA_BUF_Read_LSB_U32(xin));
  else
  { xaLONG ret; xaUBYTE *ptr = xin->buf;
    FTP_READ(xin,ptr,4,ret);   if (ret < 4) return(0);
    return(    (xaULONG)(xin->buf[0])      | ((xaULONG)(xin->buf[1])<<8)
            | ((xaULONG)(xin->buf[2])<<16) | ((xaULONG)(xin->buf[3])<<24) );
  }
}
/***** */
xaULONG XA_FTP_Read_MSB_U32(xin)
XA_INPUT *xin;
{ xin->fpos += 4;
  if (xin->buf_cnt) return(XA_BUF_Read_MSB_U32(xin));
  else
  { xaLONG ret; xaUBYTE *ptr = xin->buf;
    FTP_READ(xin,ptr,4,ret);   if (ret < 4) return(0);
    return(    (xaULONG)(xin->buf[3])      | ((xaULONG)(xin->buf[2])<<8)
            | ((xaULONG)(xin->buf[1])<<16) | ((xaULONG)(xin->buf[0])<<24) );
  }
}



/***** */
xaLONG XA_FTP_Read_Block(xin,ptr,len)
XA_INPUT *xin;
xaUBYTE *ptr;
xaULONG len;
{ xin->fpos += len;
  if (xin->buf_cnt) return(XA_BUF_Read_Block(xin,ptr,len));
  else { xaLONG ret; FTP_READ(xin,ptr,len,ret); return(ret); } 
}

xaULONG XA_FTP_Open_File(xin)
XA_INPUT *xin;
{ xaLONG ret;
  char machine[256],user_cmd[256],pass_cmd[256],file_cmd[256],*tmp;

  xin->buf_ptr = xin->buf; xin->buf_cnt = 0; xin->fpos = 0;
  xin->err_flag = XA_IN_ERR_NONE;

  XA_File_Split(xin->file, machine, file_cmd);
	/* Get user name for ftp */
  strcpy(user_cmd,"USER \0");
  if ( (tmp = getenv("XANIM_USER")) != NULL) strcat(user_cmd,tmp);
  else			strcat(user_cmd,"anonymous\0");
	/* Get passwd for ftp */
  strcpy(pass_cmd,"PASS \0");
  if ( (tmp = getenv("XANIM_PASSWD")) != NULL) strcat(pass_cmd,tmp);
  else			strcat(pass_cmd,"xanim@xanim.com\0");
	/* Open Connection and Request remote file via ftp */
  ret = xaftp_open_file(xin, machine, user_cmd, pass_cmd, file_cmd);
  return(ret);
}

/***** */
xaULONG XA_FTP_Close_File(xin)
XA_INPUT *xin;
{ xaLONG retcode;
  xin->buf_cnt = 0;
  xin->buf_ptr = xin->buf;
  if (xin->dsock >= 0) { close(xin->dsock); xin->dsock = -1; }
  if (xa_ftp_get_msg(xin, &retcode) == xaFALSE) return(xaFALSE);
  if (xa_ftp_send_cmd(xin, "QUIT", &retcode) == xaFALSE) return(xaFALSE);
  if (xin->csock >= 0) { close(xin->csock); xin->csock = -1; }
  return(xaTRUE);
}

/***** */
xaLONG XA_FTP_EOF(xin, off)
XA_INPUT *xin;
xaLONG off;
{ if ((off >=0) && ((xin->fpos + off) > xin->eof)) return(xaTRUE);
  return(  (xin->err_flag==XA_IN_ERR_NONE)?(xaFALSE):(xaTRUE) );
}

/***** */
xaLONG XA_FTP_ftell(xin)
XA_INPUT *xin;
{ 
  return( xin->fpos );
}

/***** */
xaLONG XA_FTP_seek(xin,fpos,flag)
XA_INPUT *xin;
xaLONG fpos;
xaLONG flag;
{ char c;

	/** Move Forward valid in Sequential Streams */
  if ( (flag == 1) && (fpos >= 0) ) 
  { xin->fpos += fpos;
    while(fpos--) read( xin->dsock, &c, 1);
  }	/** Move til End kinda of a NOP */
  else if ((flag == 2) && (fpos == 0)) 
  {  xin->err_flag |= XA_IN_ERR_EOF;
  }	/** Move beyond current position ok */
  else if ((flag == 0) && (fpos >= xin->fpos) )
  { while(xin->fpos < fpos) { read( xin->dsock, &c, 1); xin->fpos++; }
  }
  else /** Otherwise - not possible - complain */
  {
    fprintf(stderr,"FTP seek err. off = %d flag = %d=n",fpos,flag);
    return(-1);
  }
  return( 0 );
}


xaULONG XA_File_Split(file,machine,file_name)
char *file, *machine, *file_name;
{ char *tmp;

	/* skip over ftp:// */
  tmp = &file[6];
  strcpy(machine, tmp);
  tmp = strchr(machine, '/' );
  if (tmp == 0) return(xaFALSE);
  strcpy(file_name, "RETR ");
  strcat(file_name, tmp);
	/* null terminate machine */
  *tmp = 0;  
  return(xaTRUE);
}


/************************
 *
 *********/
xaLONG xa_ftp_get_msg(xin, ret_code)
XA_INPUT *xin;
xaLONG *ret_code;
{ xaLONG code, msg3;
  *ret_code = 0; 
  while(1)
  { xaLONG ncnt = 0;
    xaLONG state = 0; 
    msg3 = 0;
    code = 0;
    while(1)
    { char tmp;

      if ( read(xin->csock, &tmp, 1) != 1) return(xaFALSE);

	/* construct return code */
      if      (ncnt == 0) code  = (tmp - '0') * 100;
      else if (ncnt == 1) code += (tmp - '0') * 10;
      else if (ncnt == 2) code += (tmp - '0');
      else if (ncnt == 3) msg3 = tmp;
      ncnt++;
	/* check for end of message */
      if ((state == 0) && (tmp == 0x0d)) state++;	/* detect ctl-M */
      else if (state == 1)
      { if (tmp == 0x0a)	break;		/* detect ctl-J */
	else if (tmp == 0x0d)	state = 1;	/* two ctl-M's ? */ 
	else state = 0;
      }
      else
      {
        state = 0;
        DEBUG_LEVEL2 fprintf(stderr,"%c",tmp);
      }
    }
    if ((ncnt < 5) || (msg3 != '-')) break;
  }
  DEBUG_LEVEL2 fprintf(stderr,"\n");
  *ret_code = code;
  return(xaTRUE);
}

/************************
 *
 *********/
xaLONG xa_ftp_send_cmd(xin,cmd,retcode)
XA_INPUT *xin;
char *cmd;
xaLONG *retcode;
{ xaLONG len = strlen(cmd);
  *retcode = 0;
  if (write(xin->csock, cmd, len) < len)	
	{ fprintf(stderr,"ftp send_cmd 1st write err\n");
	  return(xaFALSE); }
  if (write(xin->csock, "\015\012", 2) < 2)	
	{ fprintf(stderr,"ftp send_cmd 2nd write err\n");
	return(xaFALSE); }
  if (xa_ftp_get_msg(xin,retcode) == xaFALSE)	
	{ fprintf(stderr,"ftp get_msg err\n");
	return(xaFALSE); }

  DEBUG_LEVEL2 fprintf(stderr,"CMD: %s  retcode %d\n",cmd, *retcode);

  if ((*retcode >= 100) && (*retcode < 400))	return(xaTRUE);/* POD  Ness?*/
  else						return(xaFALSE);
}

xaULONG xa_ftp_abort(xin)
XA_INPUT xin;
{
  return(xaFALSE);
}


/************************
 *
 *********/
xaLONG xaftp_open_file(xin, machine, user_cmd, passwd_cmd, file_cmd)
XA_INPUT *xin;
char *machine, *user_cmd, *passwd_cmd, *file_cmd;
{ struct sockaddr_in unit, data, from;
  struct hostent *host;
  struct servent *service;
  char hostname[256];
  xaUBYTE *addr,*port;
  xaLONG retcode;
  int tmp_sock;
  int len;
  char port_cmd[256];

  xin->csock = -1;
  xin->dsock = -1;

  if ((host = gethostbyname(machine)) == 0)	return(xaFALSE);
#ifndef VMS
  if ((service = (struct servent *)getservbyname("ftp","tcp"))==0)
						return(xaFALSE);
#endif
  unit.sin_family = host -> h_addrtype;
  memcpy( &unit.sin_addr, host->h_addr_list[0], host->h_length);
  if ( (xin->csock = socket(unit.sin_family, SOCK_STREAM, 0)) < 0)
						return(xaFALSE);
#ifndef VMS
  unit.sin_port = service -> s_port;
#else
  unit.sin_port = htons(21);
#endif

  while((connect(xin->csock, (struct sockaddr *)(&unit),sizeof(unit))) < 0)
  {
    close(xin->csock);
    host->h_addr_list++;
    if (host->h_addr_list[0] == NULL) return(xaFALSE);
    memcpy(&unit.sin_addr, host->h_addr_list[0], host->h_length);
    if ((xin->csock = socket(unit.sin_family, SOCK_STREAM, 0)) < 0)
						return(xaFALSE);
  }

  if (xa_ftp_get_msg(xin, &retcode) == xaFALSE)	return(xaFALSE); 
  if ((retcode < 100) && (retcode > 399))
  { close(xin->csock);
    return(xaFALSE);
  }

  /* Now we should be connect to the remote machine */  
  if (xa_ftp_send_cmd(xin, user_cmd, &retcode) == xaFALSE) return(xaFALSE);
  if (retcode != 240) /* need passwd */
  {
    if (retcode == 332)
    {  return(xaFALSE);  /* POD for now no account support */
    }
    if (xa_ftp_send_cmd(xin, passwd_cmd, &retcode) == xaFALSE) return(xaFALSE);
  }
    /* Type Binary */
  if (xa_ftp_send_cmd(xin, "TYPE I", &retcode) == xaFALSE) return(xaFALSE);
   /* Open Up Data connection */

  memset(&data,0,sizeof(data));
  memset(&from,0,sizeof(from));
  if (gethostname(hostname, sizeof(hostname)) < 0)
						return(xa_ftp_abort(xin));
  if ((host= (struct hostent *)gethostbyname(hostname)) == 0)
						return(xa_ftp_abort(xin));
  data.sin_family = host->h_addrtype;
  memcpy( (char *)&data.sin_addr, (char *)host->h_addr_list[0], host->h_length);
  if ((tmp_sock = socket ( AF_INET  , SOCK_STREAM , 0 )) < 0)
						return(xa_ftp_abort(xin));
  len = 1;
  if (setsockopt(tmp_sock, SOL_SOCKET, SO_REUSEADDR,
			(char *)(&len), sizeof(len)) < 0)
			{ close(tmp_sock); return(xa_ftp_abort(xin)); }

  data.sin_port = 0;
  if ( bind(tmp_sock, (struct sockaddr *)&data, sizeof(data)) < 0 )
			{ close(tmp_sock); return(xa_ftp_abort(xin)); }

  len = sizeof(data);
  if (getsockname(tmp_sock, (struct sockaddr *)&data, &len) < 0 )
			{ close(tmp_sock); return(xa_ftp_abort(xin)); }

  if (listen(tmp_sock, 4) < 0 )
			{ close(tmp_sock); return(xa_ftp_abort(xin)); }

     /* POD add support for PORT command? */
  addr = (xaUBYTE *) (&data.sin_addr);
  port = (xaUBYTE *) (&data.sin_port);

  sprintf(port_cmd,"PORT %d,%d,%d,%d,%d,%d\0",
	(addr[0] & 0xff), (addr[1] & 0xff),
	(addr[2] & 0xff), (addr[3] & 0xff),
	(port[0] & 0xff), (port[1] & 0xff) );

  if (xa_ftp_send_cmd(xin, port_cmd, &retcode) == xaFALSE) 
	{ fprintf(stderr,"FTP: send cmd err\n"); 
	  close(tmp_sock); return(xa_ftp_abort(xin)); }

  if (xa_ftp_send_cmd(xin, file_cmd, &retcode) == xaFALSE) 
	{ fprintf(stderr,"FTP: send cmd err\n"); 
	  close(tmp_sock); return(xa_ftp_abort(xin)); }

  len = sizeof(from);
  xin->dsock = accept((int)tmp_sock, (struct sockaddr *) &from, (int *)&len);
  if (xin->dsock < 0) { close(tmp_sock); return(xa_ftp_abort(xin)); }
  close(tmp_sock);
  return(xaTRUE);
}


