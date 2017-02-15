#ifndef __XAVID_MOD_H__
#define __XAVID_MOD_H__

typedef struct S_XAVID_FUNC_HDR
{
  xaULONG	what;
  xaULONG	id;
  xaLONG	(*iq_func)();	/* init/query function */
  xaULONG	(*dec_func)();  /* opt decode function */
} XAVID_FUNC_HDR;

#define XAVID_WHAT_NO_MORE	0x0000
#define XAVID_AVI_QUERY		0x0001
#define XAVID_QT_QUERY		0x0002
#define XAVID_DEC_FUNC		0x0100

#define XAVID_API_REV		0x0003

typedef struct
{
  xaULONG		api_rev;
  char			*desc;
  char			*rev;
  char			*copyright;
  char			*mod_author;
  char			*authors;
  xaULONG		num_funcs;
  XAVID_FUNC_HDR	*funcs;
} XAVID_MOD_HDR;


#endif
