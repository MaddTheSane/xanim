/*
 * xa_j6i.h
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

#ifndef __XA_J6I_H__
#define __XA_J6I_H__

#include "xanim.h"

typedef struct J6I_FRAME_STRUCT
{
  xaULONG time;
  xaULONG timelo;
  XA_ACTION *act;
  struct J6I_FRAME_STRUCT *next;
} J6I_FRAME;
 
 
typedef struct
{
  xaUBYTE	*desc;
  xaULONG	jpg_beg;
  xaULONG	jpg_end;
  xaULONG	hdr_len;
  xaUBYTE	year;
  xaUBYTE	month;
  xaUBYTE	day;
  xaUBYTE	hour;
  xaUBYTE	min;
  xaUBYTE	sec;
} J6I_HDR;
 
#endif
