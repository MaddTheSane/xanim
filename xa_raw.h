/*
 * xa_raw.h
 *
 * Copyright (C) 1998,1999 by Mark Podlipec. 
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

/* The following copyright applies to all Ultimotion Segments of the Code:
 *
 * "Copyright International Business Machines Corporation 1994, All rights
 *  reserved. This product uses Ultimotion(tm) IBM video technology."
 *
 */

#ifndef __XA_RAW_H__
#define __XA_RAW_H__

#include "xanim.h"

#define RAW_QCIF 0x51434946
#define RAW_CIF  0x43494620

typedef struct RAW_FRAME_STRUCT
{
  xaULONG time;
  xaULONG timelo;
  XA_ACTION *act;
  struct RAW_FRAME_STRUCT *next;
} RAW_FRAME;

#endif
