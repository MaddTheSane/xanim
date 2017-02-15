
/*
 * xa_iff.c
 *
 * Copyright (C) 1990-1998,1999 by Mark Podlipec. 
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
#include "xanim.h"
#include "xa_iff.h"
#include "xa_cmap.h"
#include "xa_formats.h"

static void IFF_Adjust_For_EHB();
static xaULONG IFF_Delta_Body();
static xaULONG IFF_Delta_l();
static xaULONG IFF_Delta_3();
static xaULONG IFF_Delta_5();
static xaULONG IFF_Delta_7();
static xaULONG IFF_Delta_8();
static xaULONG IFF_Delta_J();
static void IFF_Long_Mod();
static void IFF_Buffer_Action();
static void IFF_Setup_HMAP();
static void IFF_Setup_CMAP();
static IFF_ACT_LST *IFF_Add_Frame();
static void IFF_Image_To_Bufferable();
static void IFF_Read_ANHD();
static void IFF_Read_ANSQ();
static void IFF_Register_CRNGs();
static void IFF_Read_CRNG();
static void IFF_Free_Stuff();
static void IFF_HAM6_As_True();
static void IFF_HAM8_As_True();
static void IFF_Hash_CleanUp();
static void IFF_Hash_Init();
static void IFF_Hash_Add();
static XA_ACTION *IFF_Hash_Get();
static xaULONG IFF_Check_Same();


extern void ACT_Add_CHDR_To_Action();
extern void ACT_Del_CHDR_From_Action();
extern void UTIL_Mapped_To_Bitmap();
extern void UTIL_Mapped_To_Mapped();
extern xaUBYTE *UTIL_RGB_To_Map();
extern xaUBYTE *UTIL_RGB_To_FS_Map();
extern void ACT_Free_Act();
extern xaULONG UTIL_Get_Buffer_Scale();
extern void UTIL_Scale_Pos_Size();

extern xaLONG xa_anim_cycling;

extern XA_ACTION *ACT_Get_Action();
extern void ACT_Setup_Mapped();
extern void UTIL_Sub_Image();
extern XA_CHDR *ACT_Get_CMAP();

#define IFF_SPEED_DEFAULT 3
#define IFF_MS_PER_60HZ  17
#define ACT_IFF_HMAP6 0x2000
#define ACT_IFF_HMAP8 0x2001

typedef struct
{
  xaULONG flag;
  XA_ACTION *dlta;
  XA_ACTION *src;
  XA_ACTION *dst;
  XA_ACTION *nxtdlta;
} IFF_HASH;

static IFF_HASH *iff_hash_tbl;
static xaULONG iff_hash_cur = 0;


static IFF_ANSQ_HDR *iff_ansq;
static xaULONG iff_ansq_cnt;
static IFF_DLTA_TABLE *iff_dlta_acts;

static xaLONG  iff_allow_cycling;
static xaULONG iff_time;
static xaULONG iff_anim_flags;
static xaULONG iff_cmap_bits;
static ColorReg iff_hmap[XA_HMAP_SIZE];
static ColorReg *iff_cur_hmap;
static ColorReg iff_cmap[256];
static XA_CHDR *iff_chdr;

static const xaLONG mask[8] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};

static IFF_ACT_LST *iff_act_start,*iff_act_cur;
static xaULONG iff_act_cnt;
static xaULONG iff_dlta_cnt;
static xaULONG iff_dlta_compression,iff_dlta_bits;

static xaULONG iff_imagex,iff_imagey,iff_imagec,iff_imaged;
static xaULONG iff_max_imagex,iff_max_imagey,iff_max_imagec,iff_max_imaged;
static xaULONG iff_or_mask;
static xaULONG iff_max_fvid_size;

extern xaULONG xa_ham_map_size;
extern xaULONG *xa_ham_map;
extern XA_CHDR *xa_ham_chdr;
extern xaULONG xa_ham_init;

/*
 * NOTES: the iff_cmap is read in as 8 bits. It's shifted down to 4 bits
 * before ACT_GET_CMAP is called(unless there are 8 bit planes)
 * How does one know whether is 4 bits or 8 bits per color component?
 */

void
IFF_TheEnd()
{
  IFF_Free_Stuff();
  TheEnd();
}

void
IFF_TheEnd1(p)
char *p;
{
  IFF_Free_Stuff();
  TheEnd1(p);
}

IFF_ACT_LST *IFF_Add_Frame(type,act)
xaULONG type;
XA_ACTION *act;
{
  IFF_ACT_LST *iframe;

  iframe = (IFF_ACT_LST *) malloc(sizeof(IFF_ACT_LST));
  if (iframe == 0) IFF_TheEnd1("IFF_Add_Frame: malloc err");

  if (type == 1) iff_dlta_cnt++;
  iframe->type = type;
  iframe->act = act;
  iframe->next = 0;

  if (iff_act_start == 0) iff_act_start = iframe;
  else iff_act_cur->next = iframe;

  iff_act_cur = iframe;
  iff_act_cnt++;
  return(iframe);
}

void
IFF_Free_Stuff()
{
  IFF_ACT_LST *itmp;

  if (iff_ansq) FREE(iff_ansq,0x1000);
  if (iff_dlta_acts) FREE(iff_dlta_acts,0x1001); 
  iff_ansq = 0; iff_dlta_acts = 0;
  while(iff_act_start != 0)
  {
    itmp = iff_act_start;
    iff_act_start = iff_act_start->next;
    FREE(itmp,0x1003);
  }
  iff_ansq = 0;
  iff_dlta_acts = 0;
  iff_act_start = 0;
}


/*
 *
 */
xaULONG IFF_Read_File(const char *fname,XA_ANIM_HDR *anim_hdr)
{ XA_INPUT *xin = anim_hdr->xin;
  xaLONG camg_flag,cmap_flag,chdr_flag,ret;
  xaLONG crng_flag,formtype;
  xaLONG ansq_flag;
  xaLONG face_flag,bmhd_flag,file_size,file_read;
  Bit_Map_Header bmhd;
  Chunk_Header  chunk;
  xaLONG prop_flag;
  xaLONG exit_flag;
  int name_flag = xaFALSE;
  char NAME_string[128];

  iff_allow_cycling = (anim_hdr->anim_flags & ANIM_CYCLE)?(xaTRUE):(xaFALSE);
  iff_act_start	= 0;
  iff_act_cur	= 0;
  iff_act_cnt	= 0;
  iff_dlta_compression = 0xffff;
  iff_dlta_bits = 0;
  iff_anim_flags = anim_hdr->anim_flags & ~(ANIM_LACE | ANIM_HAM | ANIM_CYCLE);
  file_size	= 0;
  file_read	= 0;
  iff_imagex = iff_max_imagex = 0;
  iff_imagey = iff_max_imagey = 0;
  iff_imagec = iff_max_imagec = 0;
  iff_imaged = iff_max_imaged = 0;
  iff_or_mask = 0;
  iff_max_fvid_size = 0;
  prop_flag	= 0;
  bmhd_flag	= 0;
  face_flag	= 0;
  crng_flag	= 0;
  camg_flag	= 0;
  cmap_flag	= xaFALSE;
  chdr_flag	= xaFALSE;
  ansq_flag	= 0;
  iff_ansq	= 0;
  iff_ansq_cnt	= 0;
  iff_dlta_acts	= 0;
  iff_dlta_cnt	= 0;
  iff_chdr	= 0;
  iff_cmap_bits = 4;
  iff_cur_hmap  = 0;

  exit_flag = 0; 
  while( !xin->At_EOF(xin,-1) && !exit_flag)
  {
    /* read Chunk_Header 
    */
    chunk.id   = xin->Read_MSB_U32(xin);
    chunk.size = xin->Read_MSB_U32(xin);

    if ( xin->At_EOF(xin,-1) ) break;    
    if (chunk.size == -1) ret = -1;
    else ret = 0;

    DEBUG_LEVEL1
    {
      fprintf(stderr,"Chunk.ID = ");
      IFF_Print_ID(stderr,chunk.id);
      fprintf(stderr,"  chunksize=%x\n",chunk.size);
    }
    if (chunk.size & 1) chunk.size += 1; /* halfword pad it */
    if (ret==0)
    {
      switch(chunk.id)
      {
        case IFF_PROP: 
                prop_flag=1;
        case IFF_LIST: 
        case IFF_FORM: 
                formtype = xin->Read_MSB_U32(xin);
		file_size = chunk.size;
		if (file_size & 0x01) file_size++;

		if (   (formtype != IFF_ILBM)
		    && (formtype != IFF_ANIM)
		    && (formtype != IFF_8SVX) )
		{
		  fprintf(stderr,"IFF: unsupported FORM Type: ");
		  IFF_Print_ID(stderr,formtype);
                  fprintf(stderr,"\n");
		  xin->Seek_FPos(xin,file_size,1);
		  file_read = 0;
		}
		else	file_read = -1;

                DEBUG_LEVEL2
                {
                  fprintf(stderr,"  IFF ");
                  IFF_Print_ID(stderr,chunk.id);
                  fprintf(stderr," = ");
                  IFF_Print_ID(stderr,formtype);
                  fprintf(stderr,"\n");
                }
                break;
        case IFF_BMHD:
		IFF_Read_BMHD(xin,&bmhd);
                if (xa_verbose)
                { fprintf(stderr,"     Size %dx%dx%d comp=%d masking=%d\n",
                                bmhd.width,bmhd.height,bmhd.depth,
                                bmhd.compression,bmhd.masking);
                }
                iff_imagex = bmhd.width;
                iff_imagey = bmhd.height;
                iff_imaged = bmhd.depth;
		if (iff_imaged == 8) iff_cmap_bits = 8;
		else iff_cmap_bits = 4;
                if (iff_imagex > iff_max_imagex) iff_max_imagex = iff_imagex;
                if (iff_imagey > iff_max_imagey) iff_max_imagey = iff_imagey;
                if (iff_imaged > iff_max_imaged) iff_max_imaged = iff_imaged;
		/* or_mask is used to move pixels to upper reaches of cmap
		 */
		iff_or_mask = CMAP_Get_Or_Mask(1 << iff_imaged);
                bmhd_flag = 1;
                break;

	case IFF_FACE: /* used in MovieSetter anims */
	      {
		xaULONG garb;
		bmhd.pageWidth  = bmhd.width  = xin->Read_MSB_U16(xin);
		bmhd.pageHeight = bmhd.height = xin->Read_MSB_U16(xin);
		garb = xin->Read_MSB_U32(xin); /* read x, y */
		garb = xin->Read_MSB_U32(xin); /* read xoff, yoff */
		bmhd.depth = 5;
		bmhd.compression = BMHD_COMP_BYTERUN;
		bmhd.x = bmhd.y = bmhd.masking = 0;
		bmhd.transparentColor = 0;
		bmhd.xAspect = bmhd.yAspect = 0;
                face_flag = bmhd_flag = 1;
                iff_imagex = bmhd.width;
                iff_imagey = bmhd.height;
                iff_imaged = bmhd.depth;
                if (iff_imagex > iff_max_imagex) iff_max_imagex = iff_imagex;
                if (iff_imagey > iff_max_imagey) iff_max_imagey = iff_imagey;
                if (iff_imaged > iff_max_imaged) iff_max_imaged = iff_imaged;
		iff_or_mask = CMAP_Get_Or_Mask(1 << iff_imaged);
                if (xa_verbose)
                {
                 fprintf(stderr,"     Size %dx%dx%d comp=%d masking=%d\n",
                                bmhd.width,bmhd.height,bmhd.depth,
                                bmhd.compression,bmhd.masking);
                }
	      }
              break;

        case IFF_CAMG:
                {
                  DEBUG_LEVEL2 fprintf(stderr,"IFF CAMG\n");
                  if (chunk.size != 4) 
                  { 
		    xin->Seek_FPos(xin,chunk.size,1);
                    break;
                  }

                  camg_flag = xin->Read_MSB_U32(xin) | IFF_CAMG_NOP;

                  if ((camg_flag & IFF_CAMG_EHB) && (cmap_flag == xaTRUE))  
                  {
                    IFF_Adjust_For_EHB(iff_cmap,iff_cmap_bits);
		    iff_chdr = 
			ACT_Get_CMAP(iff_cmap,iff_imagec,iff_or_mask,
				iff_imagec,iff_or_mask,
				iff_cmap_bits,iff_cmap_bits,iff_cmap_bits);
		    chdr_flag = xaTRUE;
                    break;
                  }
                  if (camg_flag & IFF_CAMG_LACE) iff_anim_flags |= ANIM_LACE;

                  if ((camg_flag & IFF_CAMG_HAM) && (cmap_flag == xaTRUE)) 
                  {
                    XA_ACTION *act;

                        /* CREATE ACT_IFF_HMAP chunk */
                    if (iff_cmap_bits == 8)
                    {
                      act = ACT_Get_Action(anim_hdr,ACT_IFF_HMAP8);
                      iff_anim_flags |= ANIM_HAM8;
                    }
                    else
                    {
                      act = ACT_Get_Action(anim_hdr,ACT_IFF_HMAP6);
                      iff_anim_flags |= ANIM_HAM6;
                    }
                    IFF_Setup_HMAP(act,iff_hmap,iff_cmap,iff_cmap_bits);
                    iff_cur_hmap = (ColorReg *)act->data;

                    if (cmap_true_to_332 == xaTRUE)
                    {
                      iff_chdr = CMAP_Create_332(iff_cmap,&iff_imagec);
		      iff_or_mask = 0;
                    }
                    else /* if (cmap_true_to_gray == xaTRUE) */
                    {
                      iff_chdr = CMAP_Create_Gray(iff_cmap,&iff_imagec);
		      iff_or_mask = 0;
                    }
                    chdr_flag = xaTRUE;
                  }
                }
                break;

        case IFF_CMAP:
                {
		  xaULONG tmp;
                  DEBUG_LEVEL2 fprintf(stderr,"IFF CMAP\n");

                  if (chunk.size == 0x40) 
		  {
		    iff_imagec = chunk.size / 2; /* xR+GB */
		    IFF_Read_CMAP_1(iff_cmap,iff_imagec,xin);
		  }
                  else
		  {
		    iff_imagec = chunk.size / 3;  /* Rx+Gx+Bx 1 byte each */
		    IFF_Read_CMAP_0(iff_cmap,iff_imagec,xin);
		  }

                  /* Typically iff_imaged matches iff_imagec but not always 
                   * HAM and EHB are frequent examples
		   */
                  if (bmhd_flag)
		  {
		    tmp = 0x01 << iff_imaged;
		    if (tmp < iff_imagec) iff_imagec = tmp;
		  }

                  if (camg_flag & IFF_CAMG_HAM) 
                  {
                    XA_ACTION *act;
                    if (iff_cmap_bits == 8)
		    {
                      act = ACT_Get_Action(anim_hdr,ACT_IFF_HMAP8);
		      iff_anim_flags |= ANIM_HAM8;
		    }
                    else
		    {
                      act = ACT_Get_Action(anim_hdr,ACT_IFF_HMAP6);
                      iff_anim_flags |= ANIM_HAM6;
		    }
                    IFF_Setup_HMAP(act,iff_hmap,iff_cmap,iff_cmap_bits);
                    iff_cur_hmap = (ColorReg *)act->data;

                    if (cmap_true_to_332 == xaTRUE)
                    {
                      iff_chdr = CMAP_Create_332(iff_cmap,&iff_imagec);
                      iff_or_mask = 0;
                    }
                    else /* if (cmap_true_to_gray == xaTRUE) */
                    {
                      iff_chdr = CMAP_Create_Gray(iff_cmap,&iff_imagec);
                      iff_or_mask = 0;
                    }
		    chdr_flag = xaTRUE;
                  }
		  else if (camg_flag & IFF_CAMG_EHB)
		  {
                    IFF_Adjust_For_EHB(iff_cmap,iff_cmap_bits);
                    iff_chdr =
                      ACT_Get_CMAP(iff_cmap,iff_imagec,iff_or_mask,
				iff_imagec,iff_or_mask,
				iff_cmap_bits,iff_cmap_bits,iff_cmap_bits);
		    chdr_flag = xaTRUE;
		  }
		  else if (camg_flag) /* NOT HAM6,HAM8 or EHB */
		  {
		    if (iff_cmap_bits == 4) 
			IFF_Shift_CMAP(iff_cmap,iff_imagec);
                    iff_chdr = ACT_Get_CMAP(iff_cmap,iff_imagec,iff_or_mask,
				iff_imagec,iff_or_mask,
				iff_cmap_bits,iff_cmap_bits,iff_cmap_bits);
		    chdr_flag = xaTRUE;
		  }
                  cmap_flag = xaTRUE;
                }
                break;

	case IFF_NAME:		/* seek over the filename */
	  xin->Read_Block(xin,NAME_string,chunk.size);
	  DEBUG_LEVEL2 fprintf(stderr,"Read NAME \"%s\"\n", NAME_string);
	  name_flag = xaTRUE; /* and flag we want the next BODY nuked */
	  break;

	case IFF_BODY:
	  {
	    XA_ACTION *act;
	    ACT_DLTA_HDR *dlta_hdr;

	    if (name_flag == xaTRUE)    /* seek over everything */
	    {
		DEBUG_LEVEL2 fprintf(stderr,"NAME BODY\n");
		xin->Seek_FPos(xin,chunk.size,1);
		name_flag = xaFALSE;
		break;
	    }

	    DEBUG_LEVEL2 fprintf(stderr,"IFF BODY\n");
	    if (chdr_flag == xaFALSE)
	    {
	      if (cmap_flag==xaTRUE)
	      {   
	        if (iff_cmap_bits == 4) IFF_Shift_CMAP(iff_cmap,iff_imagec);
	        iff_chdr = ACT_Get_CMAP(iff_cmap,iff_imagec,iff_or_mask,
					iff_imagec,iff_or_mask,iff_cmap_bits,
					iff_cmap_bits,iff_cmap_bits);
	        chdr_flag = xaTRUE;
	        IFF_Register_CRNGs(anim_hdr,iff_chdr);
	        camg_flag = IFF_CAMG_NOP; /* so future CMAPs okay */
	      }   
	      else IFF_TheEnd1("IFF_Read_BODY: no cmap\n");
	    }

	    act = ACT_Get_Action(anim_hdr,ACT_DELTA);
	    ACT_Add_CHDR_To_Action(act,iff_chdr);
	    act->h_cmap = iff_cur_hmap;
	    IFF_Add_Frame(1,act);

/* POD TEMP FINISH THIS eventually. For now body's are read in
	    if (xin->load_flag & XA_IN_LOAD_FILE)
	    {
	      dlta_hdr =(ACT_DLTA_HDR *)malloc(sizeof(ACT_DLTA_HDR));
	      if (dlta_hdr == 0) IFF_TheEnd1("IFF_Read_BODY: malloc err");
	      act->data = (xaUBYTE *)dlta_hdr;
	      dlta_hdr->flags = ACT_DBL_BUF;
	      dlta_hdr->fpos  = xin->Seek_FPos(xin);
	      dlta_hdr->fsize = chunk.size;
	      xin->Seek_FPos(xin,chunk.size,1);
	      if (chunk.size > iff_max_fvid_size) iff_max_fvid_size =chunk.size;
	    }
	    else
*/
	    {
	      dlta_hdr = (ACT_DLTA_HDR *) malloc( sizeof(ACT_DLTA_HDR)
						+ (iff_imagex * iff_imagey) );
	      if (dlta_hdr == 0) IFF_TheEnd1("IFF_Read_BODY: malloc err");
	      act->data = (xaUBYTE *)dlta_hdr;
	      dlta_hdr->flags = ACT_DBL_BUF | DLTA_DATA;
	      dlta_hdr->fpos = 0; dlta_hdr->fsize = chunk.size;
	      IFF_Read_BODY(xin,dlta_hdr->data,chunk.size,
				iff_imagex, iff_imagey, iff_imaged,
				(int)(bmhd.compression),(int)(bmhd.masking),
				iff_or_mask);
	    }
	    dlta_hdr->xapi_rev = 0x0001;
	    dlta_hdr->delta = IFF_Delta_Body;
	    dlta_hdr->xsize = iff_imagex;	dlta_hdr->ysize = iff_imagey;
	    dlta_hdr->xpos = dlta_hdr->ypos = 0;
	    dlta_hdr->special = 0;
	    dlta_hdr->extra = (void *)(0);
	  }
	  break;

        case IFF_ANHD:
		{
		  Anim_Header   anhd;
		  DEBUG_LEVEL2 fprintf(stderr,"IFF ANHD\n");
		  if (chunk.size >= Anim_Header_SIZE)
		  {
		    IFF_Read_ANHD(xin,&anhd,chunk.size);
		    iff_dlta_compression = anhd.op;
		    iff_dlta_bits = anhd.bits;
/*
		    if (xa_verbose) 
			fprintf(stderr,"ANHD time = %d\n",anhd.reltime); 
*/
		  }
		  else 
		  {
		    xin->Seek_FPos(xin,chunk.size,1);
		    iff_dlta_compression = 0xffffffff;
		    iff_dlta_bits = 0x0;
		    fprintf(stderr,"ANHD chunksize mismatch %d\n",chunk.size);
		  }
		}
		break;

        case IFF_DLTA:
	  {
	    ACT_DLTA_HDR *dlta_hdr;
	    XA_ACTION *act;
	    DEBUG_LEVEL2 fprintf(stderr,"IFF DLTA: ");
	    act = ACT_Get_Action(anim_hdr,ACT_DELTA);
	    ACT_Add_CHDR_To_Action(act,iff_chdr);
	    act->h_cmap = iff_cur_hmap;
  	    IFF_Add_Frame(1,act);

	    if (xin->load_flag & XA_IN_LOAD_FILE)
	    {
	      dlta_hdr =(ACT_DLTA_HDR *)malloc(sizeof(ACT_DLTA_HDR));
	      if (dlta_hdr == 0) IFF_TheEnd1("IFF_Read_DLTA: malloc err");
	      act->data = (xaUBYTE *)dlta_hdr;
	      dlta_hdr->flags = ACT_DBL_BUF;
	      dlta_hdr->fpos  = xin->Get_FPos(xin);
	      dlta_hdr->fsize = chunk.size;
	      xin->Seek_FPos(xin,chunk.size,1);
	      if (chunk.size > iff_max_fvid_size) iff_max_fvid_size =chunk.size;
	    }
	    else
	    {
	      dlta_hdr =(ACT_DLTA_HDR *)malloc(sizeof(ACT_DLTA_HDR)+chunk.size);
	      if (dlta_hdr == 0) IFF_TheEnd1("IFF_Read_DLTA: malloc err");
	      act->data = (xaUBYTE *)dlta_hdr;
	      dlta_hdr->flags = ACT_DBL_BUF | DLTA_DATA;
	      dlta_hdr->fpos = 0; dlta_hdr->fsize = chunk.size;
	      ret=xin->Read_Block(xin,dlta_hdr->data,chunk.size);
	    }

	    dlta_hdr->xsize = iff_imagex;
	    dlta_hdr->ysize = iff_imagey;
	    dlta_hdr->xpos = dlta_hdr->ypos = 0;
	    dlta_hdr->special = 0;
	    dlta_hdr->extra = (void *)(0);
	    dlta_hdr->xapi_rev = 0x0001;
	    switch(iff_dlta_compression)
	    {
	      case  3:
		DEBUG_LEVEL2 fprintf(stderr,"type 3\n");
		dlta_hdr->delta = IFF_Delta_3;
		break;
	      case 5:
		DEBUG_LEVEL2 fprintf(stderr,"type 5\n");
		dlta_hdr->delta = IFF_Delta_5;
		break;
	      case 7:
		dlta_hdr->delta = IFF_Delta_7;
		if (iff_dlta_bits & IFF_ANHD_LDATA)
		{ 
		  DEBUG_LEVEL2 fprintf(stderr,"type 7L\n");
		  dlta_hdr->extra =  (void *)(0);
		} 
		else 
		{ 
		  DEBUG_LEVEL2 fprintf(stderr,"type 7S\n");
		  dlta_hdr->extra =  (void *)(1);
		} 
		break;
	      case 8:
		dlta_hdr->delta = IFF_Delta_8;
		if (iff_dlta_bits & IFF_ANHD_LDATA)
		{
		  DEBUG_LEVEL2 fprintf(stderr,"type 8L\n");
		  dlta_hdr->extra =  (void *)(0);
		}
		else
		{
		  DEBUG_LEVEL2 fprintf(stderr,"type 8S\n");
		  dlta_hdr->extra =  (void *)(1);
		}
		break;
	      case 74:
		DEBUG_LEVEL2 fprintf(stderr,"type J\n");
		dlta_hdr->delta = IFF_Delta_J;
		break;
	      case  108:
		dlta_hdr->delta = IFF_Delta_l;
		dlta_hdr->extra =  (void *)(1);
		DEBUG_LEVEL2 fprintf(stderr,"type l\n");
		break;
	      default:  
		act->type = ACT_NOP;
		fprintf(stderr,"Unsupported Delta %d\n",iff_dlta_compression);
		break;
	    }
	  }
	break;

	case IFF_ANSQ:
	  {
	    DEBUG_LEVEL2 fprintf(stderr,"IFF ANSQ\n");
	    ansq_flag = 1;  /* we found an ansq chunk */
	    IFF_Read_ANSQ(xin,chunk.size);
	  }
	  break;

        case IFF_CRNG: 
	  DEBUG_LEVEL2 fprintf(stderr,"IFF CRNG\n");
	  IFF_Read_CRNG(anim_hdr,xin,chunk.size,&crng_flag);
	  break;

	case IFF_TINY : /* ignore */
	case IFF_DPI : /* ignore */
	case IFF_IMRT: /* ignore */
	case IFF_GRAB: /* ignore */
	case IFF_DPPS: /* ignore */
	case IFF_DPPV: /* ignore */
	case IFF_DPAN: /* ignore */
	case IFF_DRNG: /* ignore */
	case IFF_VHDR: /* sound ignore should kill next body until bmhd*/
	case IFF_ANNO: /* sound ignore */
	case IFF_CHAN: /* sound ignore */
	case IFF_ANFI: /* sound ignore */
		xin->Seek_FPos(xin,chunk.size,1);
                break;
    default:
	if ( xin->At_EOF(xin,-1) ) exit_flag = 1;   /* end of file */
	else
	{
	  fprintf(stderr,"Unknown IFF type="); IFF_Print_ID(stderr,chunk.id);
	  if (   (file_read < file_size)	/* there should be more  */
	      && (chunk.size < (file_size - file_read) ) /*  size n 2 big */
	     )
	  {
	    fprintf(stderr,"  Will Continue Reading File.\n");
	    xin->Seek_FPos(xin,chunk.size,1);
	  }
	  else
	  {
	    fprintf(stderr,"  Will Stop Reading File.\n");
	    exit_flag = 1;
	  }
	}
	break;
   } /* end chunk switch */
   /*
    * keep track of number of bytes read. This allows us to distinguish
    * valid unknown chunks from garbage tacked to the end of a file.
    */
   if (!exit_flag)
   {
     if (file_read == -1) file_read = 4; /* assuming FORM chunk or similar */
     else file_read += chunk.size + 8; /* add ID and SIZE of other chunks */
   }

  } /* end if ret==0 */
 } /* end of while not eof or exit_flag */
 DEBUG_LEVEL2 fprintf(stderr,"Bytes Read = %x\n",file_read);
 xin->Close_File(xin);

 /* 
  * Set up a map of delta's to their action numbers.
  */
 {
   xaULONG i;
   xaULONG inter_dlta_i,dlta_i,act_i;

   iff_dlta_acts = 
	(IFF_DLTA_TABLE *)malloc( (iff_dlta_cnt + 1) * sizeof(IFF_DLTA_TABLE));
   if (iff_dlta_acts == 0) IFF_TheEnd1("IFF_Read_File: dlta_cnts malloc err");
   for(i=0; i < iff_dlta_cnt; i++) 
   {
     iff_dlta_acts[i].cnt   = 0;
     iff_dlta_acts[i].frame = 0;
     iff_dlta_acts[i].start = 0;
     iff_dlta_acts[i].end   = 0;
   }
   dlta_i = 0;
   act_i = 0;
   inter_dlta_i = 0;

   iff_act_cur = iff_act_start;
   iff_dlta_acts[dlta_i].start = iff_act_cur;
   iff_dlta_acts[dlta_i].frame = act_i;
   while(iff_act_cur != 0)
   {
     inter_dlta_i++;
     act_i++;
     switch(iff_act_cur->act->type)
     {
       case ACT_DELTA:
		iff_dlta_acts[dlta_i].cnt = inter_dlta_i;
		iff_dlta_acts[dlta_i].end = iff_act_cur;
		inter_dlta_i = 0;
		iff_act_cur = iff_act_cur->next;
		dlta_i++; 
		if (dlta_i > iff_dlta_cnt)
		{
  		  fprintf(stderr,"IFF_Read: dlta setup err  <%d > %d> \n",
						dlta_i,iff_dlta_cnt);
  		  IFF_TheEnd();
		}
		if (dlta_i < iff_dlta_cnt)
		{
		  iff_dlta_acts[dlta_i].start = iff_act_cur;
		  iff_dlta_acts[dlta_i].frame = act_i;
		}
		break;
       default:
		iff_act_cur = iff_act_cur->next;
		break;
     }
   } /* end of while */
   DEBUG_LEVEL1 fprintf(stderr,"%d dltas found\n",dlta_i);

   iff_time = XA_GET_TIME(IFF_SPEED_DEFAULT * IFF_MS_PER_60HZ);

   if (ansq_flag)
   {
     xaLONG i,t_time;
     xaULONG iff_frame_cnt,frame_i;

     t_time = 0;
     iff_frame_cnt = iff_dlta_acts[0].cnt;  /* start with body cnt */
     for(i=0; i < iff_ansq_cnt; i++) /* count frames */
     {
       if (iff_ansq[i].time != 0xffff) /* if not loop frame */
	  iff_frame_cnt += iff_dlta_acts[ iff_ansq[i].dnum + 1 ].cnt;
     }
     anim_hdr->frame_lst = 
	(XA_FRAME *)malloc(sizeof(XA_FRAME) * (iff_frame_cnt + 1));
     if (anim_hdr->frame_lst == NULL) 
	IFF_TheEnd1("IFF ANSQ: frame_lst malloc err");
  
       /* For movies default loop frame is 0 */
     anim_hdr->loop_frame = 0;

     /* take care of frame up to BODY */
     frame_i = 0;
     iff_act_cur = iff_dlta_acts[0].start;
     while(iff_act_cur != 0)
     {
       anim_hdr->frame_lst[frame_i].act = iff_act_cur->act;
       if (iff_act_cur == iff_dlta_acts[0].end)
       {
	 anim_hdr->frame_lst[frame_i].time_dur = iff_time;
	 anim_hdr->frame_lst[frame_i].zztime = t_time;
	 t_time += iff_time;
	 frame_i++;
	 break;
       }
       else 
       {
	 anim_hdr->frame_lst[frame_i].time_dur = 0;
	 anim_hdr->frame_lst[frame_i].zztime = t_time;
       }
       frame_i++;  
       iff_act_cur = iff_act_cur->next;
       if ( (frame_i > iff_frame_cnt) && (iff_act_cur != 0) )
       {
         fprintf(stderr,"IFF_ansq: frame err %d %d\n",frame_i,iff_frame_cnt);
         IFF_TheEnd();
       }
     }
     for(i=0; i < iff_ansq_cnt; i++)
     {
        if (iff_ansq[i].time == 0xffff) /* loop frame */
        {
          anim_hdr->loop_frame = iff_ansq[ iff_ansq[i].dnum ].frame;
        }
        else
        {
	  xaULONG dlta_j;
	  iff_ansq[i].frame = frame_i; /* for looping info */
          dlta_j = iff_ansq[i].dnum + 1;
          iff_time = XA_GET_TIME(iff_ansq[i].time * IFF_MS_PER_60HZ);
	  iff_act_cur = iff_dlta_acts[dlta_j].start;
	  while(iff_act_cur != 0)
	  {
	    anim_hdr->frame_lst[frame_i].act = iff_act_cur->act;
            if (iff_act_cur == iff_dlta_acts[dlta_j].end) 
	    {
	      anim_hdr->frame_lst[frame_i].time_dur = iff_time;
	      anim_hdr->frame_lst[frame_i].zztime = t_time;
	      t_time += iff_time;
	      frame_i++;  
	      break;
	    }
	    else 
	    {
	      anim_hdr->frame_lst[frame_i].time_dur = 0;
	      anim_hdr->frame_lst[frame_i].zztime = t_time;
	    }
	    frame_i++;  
            iff_act_cur = iff_act_cur->next;
	    if ( (frame_i > iff_frame_cnt) && (iff_act_cur != 0) )
	    {
	      fprintf(stderr,"IFF_ansq: frame err %d %d\n",
						frame_i,iff_frame_cnt);
	      IFF_TheEnd();
	    }
	  }
        }
      } /* end of for */
      anim_hdr->frame_lst[frame_i].time_dur = 0;
      anim_hdr->frame_lst[frame_i].zztime = -1;
      anim_hdr->frame_lst[frame_i].act  = 0;
      anim_hdr->last_frame = frame_i - 1;
      anim_hdr->total_time = anim_hdr->frame_lst[frame_i-1].zztime 
                                + anim_hdr->frame_lst[frame_i-1].time_dur;
    }
    else   /* no ansq chunk */
    {
      xaLONG frame_i,t_time;

      t_time = 0;
      /* extra for end and JMP2END */
      anim_hdr->frame_lst = 
	  (XA_FRAME *)malloc(sizeof(XA_FRAME) * (iff_act_cnt + 2));
      if (anim_hdr->frame_lst == NULL)
		IFF_TheEnd1("IFF_Read: frame malloc err");
      iff_act_cur = iff_act_start;
      frame_i = 0;
      while(iff_act_cur != 0)
      {
        anim_hdr->frame_lst[frame_i].act  = iff_act_cur->act;
	if (iff_act_cur->type == 1)
	{
	  if ( (iff_dlta_cnt == 1) && (crng_flag) && (xa_jiffy_flag == 0) )
/* && (iff_act_cur->act->type == ACT_IFF_BODY) ) */
	  {
            anim_hdr->frame_lst[frame_i].time_dur = DEFAULT_CYCLING_TIME;
            anim_hdr->frame_lst[frame_i].zztime = t_time;
	    t_time += DEFAULT_CYCLING_TIME;
	  }
          else
	  {
	    anim_hdr->frame_lst[frame_i].time_dur = iff_time;
	    anim_hdr->frame_lst[frame_i].zztime   = t_time;
	    t_time += iff_time;
	  }
	}
	else
	{
	  anim_hdr->frame_lst[frame_i].time_dur = 0;
	  anim_hdr->frame_lst[frame_i].zztime = t_time;
	}
        iff_act_cur = iff_act_cur->next;
        frame_i++;
        if (frame_i > iff_act_cnt)
        {
          fprintf(stderr,"IFF_Read: frame inconsistency %d %d\n",
                frame_i,iff_act_cnt);
          IFF_TheEnd();
        }
      }
      /* Add JMP2END so deltas to beginning aren't displayed unless looping */
      if ( !(iff_anim_flags & ANIM_NOLOOP) && (iff_dlta_cnt > 3) && !face_flag)
      { xaULONG kk = frame_i;
	anim_hdr->frame_lst[kk].time_dur = 
			anim_hdr->frame_lst[iff_dlta_acts[1].frame].time_dur;
	anim_hdr->frame_lst[kk].zztime = 
			anim_hdr->frame_lst[iff_dlta_acts[1].frame].zztime;
	anim_hdr->frame_lst[kk].act  = anim_hdr->frame_lst[kk-1].act;
	anim_hdr->frame_lst[kk-1].time_dur = 
			anim_hdr->frame_lst[iff_dlta_acts[0].frame].time_dur;
	anim_hdr->frame_lst[kk-1].zztime = 
			anim_hdr->frame_lst[iff_dlta_acts[0].frame].zztime;
	anim_hdr->frame_lst[kk-1].act  = anim_hdr->frame_lst[kk-2].act;
        anim_hdr->frame_lst[kk-2].time_dur = 0;
        anim_hdr->frame_lst[kk-2].zztime = -1;
	anim_hdr->frame_lst[kk-2].act = ACT_Get_Action(anim_hdr,ACT_JMP2END);
	/* PODNOTE: IFF last two frames need to have zztime start at 0 again*/
        frame_i++;
        anim_hdr->loop_frame = iff_dlta_acts[2].frame;
        anim_hdr->total_time = anim_hdr->frame_lst[frame_i-4].zztime 
                                + anim_hdr->frame_lst[frame_i-4].time_dur;
      }
      else
      {
	anim_hdr->loop_frame = 0; 
        anim_hdr->total_time = anim_hdr->frame_lst[frame_i-1].zztime 
                                + anim_hdr->frame_lst[frame_i-1].time_dur;
      }
      if (frame_i > 0 ) anim_hdr->last_frame = frame_i - 1;
      else anim_hdr->last_frame = 0;
      anim_hdr->frame_lst[frame_i].time_dur = 0;
      anim_hdr->frame_lst[frame_i].zztime = -1;
      anim_hdr->frame_lst[frame_i].act  = 0;
      frame_i++;
      if (xa_verbose) 
      {
	fprintf(stderr,"     dlta_cnt=%d comp=%d ",
				iff_dlta_cnt,iff_dlta_compression);
	if (camg_flag & IFF_CAMG_EHB) fprintf(stderr," EHB\n");
	else if (iff_anim_flags & ANIM_HAM8) fprintf(stderr," HAM8\n");
	else if (iff_anim_flags & ANIM_HAM6) fprintf(stderr," HAM6\n");
	else fprintf(stderr,"\n");
      }
    }
  }
  anim_hdr->imagex = iff_max_imagex;
  anim_hdr->imagey = iff_max_imagey;
  anim_hdr->imagec = iff_imagec;
  anim_hdr->imaged = iff_max_imaged;

  /* Some older IFF files don't include a CAMG chunk to indicate they
   * are interlaced. They just assume the viewer will figure it out.
   * Below is an arbitrary ruleset that'll hopefully work for most cases.
   */
   if ((iff_max_imagex >= 400) && (iff_max_imagey > iff_max_imagex))
	iff_anim_flags |= ANIM_LACE;

  /* NOTE: A lot of IFF animations have active color cycle chunks, yet
   *       they were never intended to cycle. AAARRRRGH!!!
   */
  if (iff_dlta_cnt == 1)  /* single image IFF files */
  {
    if ( (crng_flag) && (iff_allow_cycling == xaTRUE) )
					iff_anim_flags |= ANIM_CYCLE;
    else iff_anim_flags &= ~ANIM_CYCLE;
  }
  else /* animation IFF files */
  {
    if ( (crng_flag) && (iff_allow_cycling == xaTRUE) 
        && (xa_anim_cycling == xaTRUE) )   iff_anim_flags |= ANIM_CYCLE;
    else iff_anim_flags &= ~ANIM_CYCLE;
  }
  anim_hdr->anim_flags = iff_anim_flags;
  IFF_Free_Stuff();
  iff_chdr = 0;
  if (xin->load_flag & XA_IN_LOAD_BUF) IFF_Buffer_Action(anim_hdr);
  else
  {
    anim_hdr->anim_flags |= ANIM_SNG_BUF;
    if (iff_dlta_cnt > 1) anim_hdr->anim_flags |= ANIM_DBL_BUF;
    if (iff_anim_flags | ANIM_HAM) anim_hdr->anim_flags |= ANIM_3RD_BUF;
  }
  if (xin->load_flag & XA_IN_LOAD_FILE) anim_hdr->anim_flags |= ANIM_USE_FILE;
  anim_hdr->fname = anim_hdr->name;
  anim_hdr->max_fvid_size = iff_max_fvid_size;
  return(xaTRUE);
}

/*
 *
 */
void IFF_Adjust_For_EHB(colormap,cmap_bits)
ColorReg colormap[];
xaULONG cmap_bits;
{
  xaLONG i,cmap_num;

  DEBUG_LEVEL1 
	fprintf(stderr,"Adjusting CMAP for Amiga Extra Half-Brite Mode\n");
  if (cmap_bits == 8) cmap_num = 128;
  else 
  { /* 4 bits per rgb, shift down from 8 bits to 4 bits */
    cmap_num = 32;
    IFF_Shift_CMAP(colormap,cmap_num);
  }

   /* make upper half a darkened version of the lower half */
  for(i=0;i<cmap_num;i++)
  {
    colormap[i + cmap_num].red   = colormap[i].red   >> 2;
    colormap[i + cmap_num].green = colormap[i].green >> 2;
    colormap[i + cmap_num].blue  = colormap[i].blue  >> 2;
  }
  iff_imagec = 2 * cmap_num;
  iff_or_mask = CMAP_Get_Or_Mask(iff_imagec);
}


/*
 *
 */
void IFF_Read_BODY(XA_INPUT *xin,xaUBYTE *image_out,xaLONG bodysize,xaULONG xsize,xaULONG ysize,xaULONG depth,
			xaLONG compression,xaLONG masking,xaULONG or_mask)
{
 xaLONG i,ret,x,y,d,dmask,tmp,rowsize;
 xaLONG imagex_pad;
 xaBYTE *inbuff,*rowbuff,*sptr;
 xaBYTE *sbuff,*dbuff;

 if (   (compression != BMHD_COMP_NONE) 
     && (compression != BMHD_COMP_BYTERUN) 
    ) IFF_TheEnd1("IFF_Read_Body: unsupported compression");

 if (   (masking != BMHD_MSK_NONE)
     && (masking != BMHD_MSK_HAS)
     && (masking != BMHD_MSK_TRANS)
    ) IFF_TheEnd1("IFF_Read_Body: unsupported masking");

 inbuff = (xaBYTE *)malloc(bodysize);
 if (inbuff == 0) IFF_TheEnd1("IFF_Read_Body: malloc failed");
 ret=xin->Read_Block(xin,inbuff,bodysize);
 if (ret<bodysize) IFF_TheEnd1("IFF_Read_Body: read of BODY chunk failed");
 sbuff = inbuff;


 /* width is rounded to multiples of 16 in the BODY form */
 /* extra bits are ignored upon reading */
 imagex_pad = xsize / 16;
 if (xsize % 16) imagex_pad++;
 imagex_pad *= 16;

 rowbuff = (xaBYTE *)malloc( imagex_pad );
 if (rowbuff == 0) IFF_TheEnd1("IFF_Read_Body: malloc failed");

 memset(image_out,or_mask,(xsize * ysize) );

 if (compression==BMHD_COMP_NONE) sptr = inbuff;

 for(y=0; y<ysize; y++)
 {
   tmp = y * xsize;
   dmask=1;
   for(d=0; d<depth; d++)
   {

     if (compression == BMHD_COMP_BYTERUN)
     {
       rowsize = imagex_pad / 8; 
       dbuff = rowbuff;
       ret=UnPackRow(&sbuff,&dbuff,&bodysize,&rowsize);
       if (ret) { fprintf(stderr,"error %d in unpack\n",ret); IFF_TheEnd();}
       sptr = rowbuff;
     }

     i = 0;
     for(x=0; x<xsize; x++)
     {
       if (mask[i] & (*sptr)) image_out[tmp+x] |= dmask;
       i++;
       if (i >= 8)
       {
	 i = 0;
	 sptr++;
       }
     }
     if (imagex_pad >= (xsize+8)) sptr++;

     dmask <<= 1;
   } /* end of depth loop */

   if (masking == BMHD_MSK_HAS)
   {
     /* read the mask row and then throw out for now */
     if (compression == BMHD_COMP_BYTERUN)
     {
       rowsize = imagex_pad / 8;
       dbuff = rowbuff;
       ret=UnPackRow(&sbuff,&dbuff,&bodysize,&rowsize);
       if (ret) { fprintf(stderr,"error %d in unpack\n",ret); IFF_TheEnd();}
     }
     else sptr += xsize/8;
   }
 } /* end of y loop */
 FREE(inbuff,0x1004);  inbuff=0;
 FREE(rowbuff,0x1005); rowbuff=0;
}



/*
 *
 */
void IFF_Print_ID(FILE *fout,xaLONG id)
{
 fprintf(fout,"%c",     (char)((id >> 24) & 0xff)   );
 fprintf(fout,"%c",     (char)((id >> 16) & 0xff)   );
 fprintf(fout,"%c",     (char)((id >>  8) & 0xff)   );
 fprintf(fout,"%c(%x)",(char)(id        & 0xff),id);
}


/* 
 *
 */
xaULONG
IFF_Delta_5(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG imaged = dec_info->imaged;
  register xaLONG col,depth,dmask;
  register xaLONG rowsize,width;
  xaULONG poff;
  register xaUBYTE *i_ptr;
  register xaUBYTE *dptr,opcnt,op,cnt;
  xaLONG miny,minx,maxy,maxx;

  /* set to opposites for min/max testing */
  dec_info->xe = dec_info->ye = 0; dec_info->ys = imagey; dec_info->xs = imagex;

  width = imagex;
  rowsize = width >> 3;
  dmask = 1;
 for(depth=0; depth<imaged; depth++)
 {
  minx = -1;
  maxx = -1;
  
  i_ptr = image;
  /* offset into delt chunk */
  { register xaUSHORT ddepth = depth << 2;
    poff  = (xaULONG)(delta[ ddepth++ ]) << 24;
    poff |= (xaULONG)(delta[ ddepth++ ]) << 16;
    poff |= (xaULONG)(delta[ ddepth++ ]) <<  8;
    poff |= (xaULONG)(delta[ ddepth   ]);
  }

  if (poff)
  {
   dptr = (xaUBYTE *)(delta + poff);
   for(col=0;col<rowsize;col++)
   {
	/* start at top of column */
    i_ptr = (xaUBYTE *)(image + (col << 3));
    opcnt = *dptr++;  /* get number of ops for this column */
    
    miny = -1;
    maxy = -1;

    while(opcnt)    /* execute ops */
    {
       /* keep track of min and max columns */
       if (minx == -1) minx = col << 3;
       maxx = (col << 3) + 7;  

       op = *dptr++;   /* get op */
     
       if (op & 0x80)    /* if type uniqe */
       {
          if (miny == -1) miny=(xaULONG)( i_ptr - image ) / width;
          cnt = op & 0x7f;         /* get cnt */
      
          while(cnt--) /* loop through data */
          {
             register xaUBYTE data = *dptr++;
             IFF_Byte_Mod(i_ptr,data,dmask,0);
             i_ptr += width;
          }
        } /* end unique */
        else
        {
           if (op == 0)   /* type same */
           {
              register xaUBYTE data;
              if (miny == -1) miny=(xaULONG)( i_ptr - image ) / width;
              cnt = *dptr++;
              data = *dptr++;

              while(cnt--) /* loop through data */
              { 
                 IFF_Byte_Mod(i_ptr,data,dmask,0);
                 i_ptr += width;
              }
            } /* end same */
            else
            {
               i_ptr += (width * op);  /* type skip */
            }
         } /* end of hi bit clear */
       opcnt--;
     } /* end of while opcnt */
     maxy = (xaULONG)( i_ptr - image ) / width;
     if ( (miny>=0) && (miny < dec_info->ys)) dec_info->ys = miny;
     if ( (maxy>=0) && (maxy > dec_info->ye)) dec_info->ye = maxy;
    } /* end of column loop */
   } /* end of valid pointer for this plane */
   dmask <<= 1;
   if ( (minx>=0) && (minx < dec_info->xs)) dec_info->xs = minx;
   if ( (maxx>=0) && (maxx > dec_info->xe)) dec_info->xe = maxx;
  } /* end of for depth */

  if (xa_optimize_flag == xaTRUE)
  {
    if (dec_info->xs >= imagex) dec_info->xs = 0;
    if (dec_info->ys >= imagey) dec_info->ys = 0;
    if (dec_info->xe <= 0)      dec_info->xe = imagex;
    if (dec_info->ye <= 0)      dec_info->ye = imagey;
  }
  else
  {
    dec_info->xs = 0;      dec_info->ys = 0;
    dec_info->xe = imagex; dec_info->ye = imagey;
  }
  return(ACT_DLTA_NORM); 
} /* end of routine */

/*
 * 
 */
xaULONG
IFF_Delta_3(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG imaged = dec_info->imaged;
 register xaLONG i,depth,dmask;
 xaULONG poff;
 register xaSHORT  offset;
 register xaUSHORT s,data;
 register xaUBYTE  *i_ptr,*dptr;

 dec_info->xs = dec_info->ys = 0; dec_info->xe = imagex; dec_info->ye = imagey;
 dmask = 1;
 for(depth=0;depth<imaged;depth++)
 {
  i_ptr = image;

  /*poff = planeoff[depth];*/ /* offset into delt chunk */

  poff  = (xaULONG)(delta[ 4 * depth    ]) << 24;
  poff |= (xaULONG)(delta[ 4 * depth + 1]) << 16;
  poff |= (xaULONG)(delta[ 4 * depth + 2]) <<  8;
  poff |= (xaULONG)(delta[ 4 * depth + 3]);

  if (poff)
  {
   dptr = (xaUBYTE *)(delta + poff);
   while( (dptr[0] != 0xff) || (dptr[1] != 0xff) )
   {
     offset = (*dptr++)<<8; offset |= (*dptr++);
     if (offset >= 0)
     {
      data = (*dptr++)<<8; data |= (*dptr++);
      i_ptr += 16 * (xaULONG)(offset);
      IFF_Short_Mod(i_ptr,data,dmask,0);
     } /* end of pos */
     else
     {
      i_ptr += 16 * (xaULONG)(-(offset+2));
      s = (*dptr++)<<8; s |= (*dptr++); /* size of next */
      for(i=0; i < (xaULONG)s; i++)
      {
       data = (*dptr++)<<8; data |= (*dptr++);
       i_ptr += 16;
       IFF_Short_Mod(i_ptr,data,dmask,0);
      }
    }  /* end of neg */
   } /* end of delta for this plane */
  } /* plane has changed data */
  dmask <<= 1;
 } /* end of d */
 return(ACT_DLTA_NORM); 
}


void IFF_Hash_Init(num)
xaULONG num;
{ register xaULONG i;
  iff_hash_cur=0;
  iff_hash_tbl = (IFF_HASH *)malloc(num * sizeof(IFF_HASH));
  if (iff_hash_tbl == 0) TheEnd1("IFF_Hash_Init: malloc err");
  for(i=0;i<num;i++) iff_hash_tbl[i].flag = ACT_DLTA_BAD;
}

void IFF_Hash_CleanUp()
{
  if (iff_hash_tbl)
  { register xaULONG i=0;
    while(i<iff_hash_cur)
    {
      register XA_ACTION *tact = iff_hash_tbl[i].dlta;
      if (tact)
      {
	if (tact->type == ACT_DELTA) ACT_Free_Act(tact);
      }
      i++;
    }
    FREE(iff_hash_tbl,0x100E); iff_hash_tbl=0;
  }
}

void IFF_Hash_Add(dlta,nxtdlta,src,dst,flag)
XA_ACTION *dlta,*nxtdlta,*src,**dst;
xaULONG flag;
{ 
  register IFF_HASH *hptr = &iff_hash_tbl[iff_hash_cur];
  hptr->flag = flag;	
  hptr->dlta = dlta;	hptr->nxtdlta = nxtdlta;
  hptr->src = src;	hptr->dst = *dst;
  iff_hash_cur++;
}

xaULONG pod_temp_i;
XA_ACTION *IFF_Hash_Get(dlta,src,nxtdlta)
XA_ACTION *dlta,*src,*nxtdlta;
{ register xaULONG i = 0;
  while(1)
  { IFF_HASH *hptr = &iff_hash_tbl[i];
pod_temp_i = i;
    if (hptr->flag & ACT_DLTA_BAD) return(0); /* end and nothing found */
    else if (hptr->dlta == dlta)
    {
      if (hptr->flag & ACT_DLTA_NOP)			return(src);
      else if (    (src == hptr->src)
		|| (hptr->flag & ACT_DLTA_BODY) )	return(hptr->dst);
      else if (    (src == hptr->dst)
		&& (hptr->flag & ACT_DLTA_XOR) )	return(hptr->src);
      else if (hptr->nxtdlta == nxtdlta)		return(hptr->dst);
    }
    i++;
  }
}
 

/* 
 *
 */
void IFF_Buffer_Action(anim_hdr)
XA_ANIM_HDR *anim_hdr;
{
  xaLONG image_size;
  xaUBYTE *buff0,*buff1,*tmp,*dbl_buff;
  XA_ACTION *act,*old_act0,*old_act1;
  XA_FRAME *frame_lst;
  xaULONG frame_i,frame_num;
  xaULONG scale_x,scale_y,need_to_scale;

  iff_chdr = 0;
  image_size = iff_imagex * iff_imagey;
  dbl_buff = buff1 = buff0 = (xaUBYTE *) malloc( 2 * image_size );
  if (buff0 == 0) TheEnd1("IFF Buffer Action: malloc failed 0");
  buff1 += image_size;
  frame_num = anim_hdr->last_frame + 1;

  IFF_Hash_Init(frame_num);
  need_to_scale = 
	UTIL_Get_Buffer_Scale(iff_imagex,iff_imagey,&scale_x,&scale_y);

  frame_i = 0;  old_act0 = old_act1 = 0;
  frame_lst = anim_hdr->frame_lst; 
  while(frame_lst[frame_i].act != 0)
  {
    XA_ACTION *dst_act,*new_act,*nxt_act;


    act = frame_lst[frame_i].act;
    nxt_act = frame_lst[frame_i + 1].act;

      switch(act->type)
      {
	case ACT_DELTA:
        { XA_DEC_INFO dec_info;
	  ACT_DLTA_HDR *dlta_hdr = (ACT_DLTA_HDR *)act->data;
          xaLONG minx,miny,maxx,maxy; 
          xaLONG pic_x,pic_y;
          xaUBYTE *t_pic;
	  xaULONG dlta_flag = 1;
	  dec_info.imagex    = iff_imagex;
	  dec_info.imagey    = iff_imagey;
	  dec_info.imaged    = iff_imaged;
	  dec_info.chdr      = 0;
	  dec_info.map_flag  = xaFALSE;
	  dec_info.map       = 0;
	  dec_info.special   = dlta_hdr->special;
	  dec_info.extra     = dlta_hdr->extra;
	  dec_info.skip_flag = 0;

	  dlta_flag = dlta_hdr->delta(buff0, dlta_hdr->data, 
						dlta_hdr->fsize, &dec_info);
	  minx = dec_info.xs;	miny = dec_info.ys;
	  maxx = dec_info.xe;	maxy = dec_info.ye;

          if (dlta_flag & ACT_DLTA_BODY)
	  {
	    memcpy((char *)buff1, (char *)buff0, image_size);
	    maxx = dlta_hdr->xsize; maxy = dlta_hdr->ysize;
            IFF_Init_DLTA_HDR(maxx,maxy);
	  }


          IFF_Update_DLTA_HDR(&minx,&miny,&maxx,&maxy);
	  dst_act = IFF_Hash_Get(act,old_act0,nxt_act); 
	/* IF unbuffered action has been previously setup and IF it's
	 * smaller than old one, use the old one. IF it's larger than
	 * the old one, then free up the old one and replace with this one.
	 * Same goes for deltas that don't change an images(the NOPs).
	 */
	  if ( (dst_act) || (dlta_flag & ACT_DLTA_NOP) )
	  {
	    XA_ACTION *tst_act;
	    if (!dst_act) /* if here because no change */
	    {
	      if (old_act0 == 0) TheEnd1("IFF Buff: No Body err");
	      IFF_Hash_Add(act,nxt_act,old_act0,&old_act0,dlta_flag);
	      tst_act = old_act0;
	    } else tst_act = dst_act;

            if ( (tst_act->type==ACT_MAPPED) || (tst_act->type==ACT_DISP) )
	    { 
	      ACT_MAPPED_HDR *maphdr = (ACT_MAPPED_HDR *)tst_act->data;
	      xaULONG px,py,sx,sy;
	      xaULONG opx,opy,osx,osy;
	      px = minx;  sx = maxx - minx; py = miny;  sy = maxy - miny;
	      if (need_to_scale) UTIL_Scale_Pos_Size(&px,&py,&sx,&sy,
					iff_imagex,iff_imagey,scale_x,scale_y);
	      sx += px;	opx = maphdr->xpos;	osx = maphdr->xsize + opx;
	      sy += py;	opy = maphdr->ypos;	osy = maphdr->ysize + opy;
	      if (   ((px >= opx) && (px <= osx))  /* new is <= old */
	          && ((sx >= opx) && (sx <= osx))
	          && ((py >= opy) && (py <= osy))
	          && ((sy >= opy) && (sy <= osy)) )
	      {
		frame_lst[frame_i].act = tst_act;
		old_act0 = old_act1; old_act1 = tst_act;
		tmp = buff0; buff0 = buff1; buff1 = tmp;
		break;
	      }
	    } /* NOTE: this might should be an error */
	    ACT_Free_Act(tst_act);
	    new_act = tst_act;   /* free_act should wipe chdr info */ 
	  }
	  else 
	  {
          /* get new action for unbuffered frame */
	    new_act = ACT_Get_Action(anim_hdr,0);
	    ACT_Add_CHDR_To_Action(new_act,act->chdr);
	  }

          pic_x = maxx - minx; pic_y = maxy - miny;
	  /* now get into shape */
          if (iff_anim_flags & ANIM_HAM)
	  { 
	    xaULONG disp_flag;

	    if (x11_display_type == XA_MONOCHROME)
		{minx=miny=0; pic_x=iff_imagex; pic_y=iff_imagey; }

            t_pic = (xaUBYTE *) malloc( XA_PIC_SIZE(pic_x * pic_y) );
	    if (x11_display_type & XA_X11_TRUE) disp_flag = xaTRUE;
	    else disp_flag = xaFALSE;
	    if (iff_anim_flags & ANIM_HAM6)
	    {
	      if (cmap_true_map_flag == xaTRUE)
	      {
	        XA_CHDR *tmp_chdr = 0;
		ACT_Del_CHDR_From_Action(new_act,new_act->chdr);
		IFF_HAM6_As_True (t_pic,buff0,&tmp_chdr,act->h_cmap,
			pic_x,pic_y,minx,miny,iff_imagex);
		ACT_Add_CHDR_To_Action(new_act,tmp_chdr);
	       }
	      else
		 IFF_Buffer_HAM6(t_pic,buff0,new_act->chdr,act->h_cmap,
			pic_x,pic_y,minx,miny,iff_imagex,xaFALSE);
	    }
	    else
	    {
	      if (cmap_true_map_flag == xaTRUE)
	      {
	        XA_CHDR *tmp_chdr = 0;
		ACT_Del_CHDR_From_Action(new_act,new_act->chdr);
		IFF_HAM8_As_True (t_pic,buff0,&tmp_chdr,act->h_cmap,
			pic_x,pic_y,minx,miny,iff_imagex);
		ACT_Add_CHDR_To_Action(new_act,tmp_chdr);
	       }
	      else
		IFF_Buffer_HAM8(t_pic,buff0,new_act->chdr,act->h_cmap,
			pic_x,pic_y,minx,miny,iff_imagex,xaFALSE);
	    }
	    ACT_Setup_Mapped(new_act, t_pic, new_act->chdr, 
		minx, miny, pic_x, pic_y, iff_imagex, iff_imagey,
		xaFALSE, 0, xaTRUE, xaFALSE, disp_flag);
	  }
	  else ACT_Setup_Mapped(new_act, buff0, new_act->chdr, 
		minx, miny, pic_x, pic_y, iff_imagex, iff_imagey,
		xaFALSE,0, xaFALSE, xaTRUE, xaFALSE);
          if (dlta_flag & ACT_DLTA_BODY) old_act0 = old_act1 = new_act;
	  IFF_Hash_Add(act,nxt_act,old_act0,&new_act,dlta_flag);
	  old_act0 = old_act1; old_act1 = new_act; 
	  frame_lst[frame_i].act = new_act;
          tmp = buff0; buff0 = buff1; buff1 = tmp;
        } /* end of delta7,5,j */
        break;
      } /* end of switch */
    frame_i++; /* move to next action in list */
  } /* end of while */
  if (dbl_buff) { FREE(dbl_buff,0x100A); dbl_buff = buff0 = buff1 = 0;}
  IFF_Hash_CleanUp();
  iff_chdr = 0;
}

xaULONG
IFF_Delta_J(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG imaged = dec_info->imaged;
  register xaLONG rowsize,width;
  register xaUBYTE *i_ptr;
  register xaLONG exitflag;
  register xaULONG  type,r_flag,b_cnt,g_cnt,r_cnt; 
  register xaULONG b,g,r;
  register xaULONG offset,dmask,depth;
  register xaUBYTE data;
  xaLONG changed,xor_flag;
  xaLONG tmp,minx,miny,maxx,maxy;
  xaLONG kludge_j;
 /* this kludge is because animations with width less than 320 are considered
  * centered in the middle of a 320 screen. Does this happen with
  * animations greater than lores overscan(374) and less than hi-res(640)????
  */

 if (imagex >= 320) kludge_j = 0;
 else kludge_j = (320-imagex)/2;

 maxx = maxy = 0; minx = imagex; miny = imagey;

 changed = xor_flag = 0;
 width = imagex;
 rowsize = width / 8;
 exitflag = 0;
 while(!exitflag)
 {
  /* read compression type and reversible_flag(xor data not just set) 
   */
  type   = (*delta++) << 8; type   |= (*delta++);

  if (type != 0 )
  {
    r_flag = (*delta++) << 8; r_flag |= (*delta++);
  }
  else r_flag = 0; /* Might possibly have to read it anyways */
  /* switch on compression type */
  switch(type)
  {
   case 0: exitflag = 1; break; /* end of list */
   case 1:
      /* Get byte count and group count 
       */
      xor_flag |= r_flag;
      b_cnt = (*delta++) << 8; b_cnt |= (*delta++);
      g_cnt = (*delta++) << 8; g_cnt |= (*delta++);
      
      /* Loop thru groups
       */
      for(g=0; g<g_cnt; g++)
      {
	xaULONG odd_flag;
        offset = (*delta++) << 8; offset |= (*delta++);

        offset <<= 3; /* counts bytes */
        if (kludge_j) 
             offset = ((offset/320) * imagex) + (offset%320) - kludge_j;

        i_ptr = (xaUBYTE *)(image + offset);

        tmp = offset%imagex; if (tmp<minx) minx=tmp;
        tmp += 8;            if (tmp>maxx) maxx=tmp;
        tmp = offset/imagex; if (tmp<miny) miny=tmp;
        tmp += b_cnt;        if (tmp>maxy) maxy=tmp;
       
        odd_flag = (b_cnt * imaged) & 0x01;
        /* Loop thru byte count 
         */
        for(b=0; b < b_cnt; b++)
        {
          dmask = 1;
          for(depth=0;depth<imaged;depth++) /* loop thru planes */
          {
            data = *delta++;
            changed |= data;          /* CHECKFORZERO change */
            IFF_Byte_Mod(i_ptr,data,dmask,r_flag);
            dmask <<= 1;
          } /* end of depth loop */
          i_ptr += width; /* direction is vertical */
        } /* end of byte loop */
        if (odd_flag) delta++; /* pad to short */
      } /* end of group loop */
      break;
   case 2:
      /* Read row count, byte count and group count 
       */
      xor_flag |= r_flag;
      r_cnt = (*delta++) << 8; r_cnt |= (*delta++);
      b_cnt = (*delta++) << 8; b_cnt |= (*delta++);
      g_cnt = (*delta++) << 8; g_cnt |= (*delta++);
      
      /* Loop thru groups
       */
      for(g=0; g < g_cnt; g++)
      { xaULONG odd_flag;
        offset = (*delta++) << 8; offset |= (*delta++);
        offset <<= 3; /* counts bytes */
        if (kludge_j) 
             offset = ((offset/320) * imagex) + (offset%320) - kludge_j;

        tmp = offset%imagex;     if (tmp<minx) minx=tmp;
        tmp += b_cnt * 8;        if (tmp>maxx) maxx=tmp;
        tmp = offset/imagex;     if (tmp<miny) miny=tmp;
        tmp += r_cnt;            if (tmp>maxy) maxy=tmp;
       
        odd_flag = (r_cnt * b_cnt * imaged) & 0x01;
        /* Loop thru rows of group
         */
        for(r=0; r < r_cnt; r++)
        {
          dmask = 1;
          for(depth=0;depth<imaged;depth++) /* loop thru planes */
          {
            i_ptr = (xaUBYTE *)(image + offset + (r * width));
            for(b=0; b < b_cnt; b++) /* loop thru byte count */
            {
              data = *delta++;
              changed |= data;        /* CHECKFORZERO */
           
              IFF_Byte_Mod(i_ptr,data,dmask,r_flag);
              i_ptr += 8;        /* data is horizontal */
            } /* end of byte loop */
            dmask <<= 1;
          } /* end of depth loop */
        } /* end of row loop */
        if (odd_flag) delta++; /* pad to short */
      } /* end of group loop */
      break;
   default: /* don't know this one yet */
            fprintf(stderr,"Unknown J-type %x\n",type);
            type = 0;      /* force an exit */
            exitflag = 1;
            break;
  } /* end of type switch */
 } /* end of while loop */

 /* if changed is zero then this Delta didn't change the image at all */ 
 /* BUT we can't return a NOP because we still need the double buffers to
  * be swapped
  */
 if (changed==0)
 {
   dec_info->xs = dec_info->ys = 0;
   dec_info->xe = 8; dec_info->ye = 1;
   DEBUG_LEVEL2 fprintf(stderr,"DELTA J nothing changed\n");
   if (xor_flag) return(ACT_DLTA_XOR);
   return(ACT_DLTA_NORM);
 }
 if (xa_optimize_flag == xaTRUE)
 {
   dec_info->xs = minx; dec_info->ys = miny;
   dec_info->xe = maxx; dec_info->ye = maxy;

   if (dec_info->xs >= imagex) dec_info->xs = 0;
   if (dec_info->ys >= imagey) dec_info->ys = 0;
   if (dec_info->xe <= 0)      dec_info->xe = imagex;
   if (dec_info->ye <= 0)      dec_info->ye = imagey;
   DEBUG_LEVEL2 fprintf(stderr,"xypos=<%d,%d> xysize=<%d %d>\n",
		dec_info->xs,dec_info->ys,dec_info->xe,dec_info->ye );
 }
 else
 {
   dec_info->xs = 0; dec_info->ys = 0;
   dec_info->xe = imagex; dec_info->ye = imagey;
 }
 if (xor_flag) return(ACT_DLTA_XOR);
 return(ACT_DLTA_NORM);
} /* end of IFF_DeltaJ routine */

/* 
 *  Decode IFF type l anims
 */
xaULONG
IFF_Delta_l(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG imaged = dec_info->imaged;
  xaULONG vertflag = (xaULONG)(dec_info->extra);
 register xaLONG i,depth,dmask,width;
 xaULONG poff0,poff1;
 register xaUBYTE *i_ptr;
 register xaUBYTE *optr,*dptr;
 register xaSHORT cnt;
 register xaUSHORT offset,data;

 dec_info->xs = dec_info->ys = 0; dec_info->xe = imagex; dec_info->ye = imagey;
 i_ptr = image;
 if (vertflag) width = imagex;
 else width = 16;
 dmask = 1;
 for(depth = 0; depth<imaged; depth++)
 {
   i_ptr = image;
   /*poff = planeoff[depth];*/ /* offset into delt chunk */
   poff0  = (xaULONG)(delta[ 4 * depth    ]) << 24;
   poff0 |= (xaULONG)(delta[ 4 * depth + 1]) << 16;
   poff0 |= (xaULONG)(delta[ 4 * depth + 2]) <<  8;
   poff0 |= (xaULONG)(delta[ 4 * depth + 3]);

   if (poff0)
   {
     poff1  = (xaULONG)(delta[ 4 * (depth+8)    ]) << 24;
     poff1 |= (xaULONG)(delta[ 4 * (depth+8) + 1]) << 16;
     poff1 |= (xaULONG)(delta[ 4 * (depth+8) + 2]) <<  8;
     poff1 |= (xaULONG)(delta[ 4 * (depth+8) + 3]);

     dptr = (xaUBYTE *)(delta + 2 * poff0); 
     optr = (xaUBYTE *)(delta + 2 * poff1); 

     /* while short *optr != -1 */
     while( (optr[0] != 0xff) || (optr[1] != 0xff) )
     {
       offset = (*optr++) << 8; offset |= (*optr++);
       cnt    = (*optr++) << 8; cnt    |= (*optr++);
 
       if (cnt < 0)  /* cnt negative */
       {
         i_ptr = image + 16 * (xaULONG)(offset);
         cnt = -cnt;
         data = (*dptr++) << 8; data |= (*dptr++);
         for(i=0; i < (xaULONG)cnt; i++)
         {
           IFF_Short_Mod(i_ptr,data,dmask,0);
           i_ptr += width;
         }
       }  /* end of neg */
       else/* cnt pos then */
       {
         i_ptr = image + 16 * (xaULONG)(offset);
         for(i=0; i < (xaULONG)cnt; i++)
         {
           data = (*dptr++) << 8; data |= (*dptr++);
           IFF_Short_Mod(i_ptr,data,dmask,0);
           i_ptr += width;
         }
       } /* end of pos */
     } /* end of delta for this plane */
   } /* plane has changed data */
   dmask <<= 1;
 } /* end of d */
 return(ACT_DLTA_NORM);
}

void
IFF_Setup_HMAP(act,hmap,cmap,bits)
XA_ACTION *act;
ColorReg *hmap,*cmap;
xaULONG bits;
{
  ColorReg *hptr;
  xaULONG i,size,shift;

  if (bits == 8) {size = XA_HMAP8_SIZE; shift = 2;}
  else {size = XA_HMAP6_SIZE; shift = 4;}
  
  act->data = (xaUBYTE *) malloc(size * sizeof(ColorReg) );
  if (act->data == 0) IFF_TheEnd1("IFF_Setup_HMAP: malloc failed\n");
  hptr = (ColorReg *) act->data;
  for(i=0; i < size; i++) 
  {
    hptr[i].red   = hmap[i].red   = cmap[i].red   >> shift;
    hptr[i].green = hmap[i].green = cmap[i].green >> shift;
    hptr[i].blue  = hmap[i].blue  = cmap[i].blue  >> shift;
  }
}

 
IFF_DLTA_HDR iff_dlta[2];

void
IFF_Init_DLTA_HDR(xaULONG max_x,xaULONG max_y)
{
  iff_dlta[0].minx = iff_dlta[1].minx = 0;
  iff_dlta[0].miny = iff_dlta[1].miny = 0;
  iff_dlta[0].maxx = iff_dlta[1].maxx = max_x;
  iff_dlta[0].maxy = iff_dlta[1].maxy = max_y;
}

void
IFF_Update_DLTA_HDR(xaLONG *min_x,xaLONG *min_y,xaLONG *max_x,xaLONG *max_y)
{
  register xaLONG tmin_x,tmin_y,tmax_x,tmax_y;

 /* This mess keeps track of the largest rectangle needed to
  * display all changes. Since things are double buffered, the
  * min/maxes of the corners of the current and previous two
  * images are taken. If the animation is in single step mode
  * it's best to display the entire image.
  */
  /* Special condition if max_x is 0, then return previous largest values*/

  if (max_x)
  {
    tmin_x = *min_x;
    tmin_y = *min_y;
    tmax_x = *max_x;
    tmax_y = *max_y;
    iff_dlta[0].minx = xaMIN(iff_dlta[0].minx, tmin_x);
    iff_dlta[0].miny = xaMIN(iff_dlta[0].miny, tmin_y);
    iff_dlta[0].maxx = xaMAX(iff_dlta[0].maxx, tmax_x);
    iff_dlta[0].maxy = xaMAX(iff_dlta[0].maxy, tmax_y);
    *min_x = xaMIN(iff_dlta[1].minx, iff_dlta[0].minx);
    *min_y = xaMIN(iff_dlta[1].miny, iff_dlta[0].miny);
    *max_x = xaMAX(iff_dlta[1].maxx, iff_dlta[0].maxx);
    *max_y = xaMAX(iff_dlta[1].maxy, iff_dlta[0].maxy);
    /* Throw out oldest rectangle and store current.  */
    iff_dlta[1].minx = iff_dlta[0].minx;
    iff_dlta[1].miny = iff_dlta[0].miny;
    iff_dlta[1].maxx = iff_dlta[0].maxx;
    iff_dlta[1].maxy = iff_dlta[0].maxy;
    iff_dlta[0].minx = tmin_x;
    iff_dlta[0].miny = tmin_y;
    iff_dlta[0].maxx = tmax_x;
    iff_dlta[0].maxy = tmax_y;
  }
  else
  {
    *min_x = xaMIN(iff_dlta[1].minx, iff_dlta[0].minx);
    *min_y = xaMIN(iff_dlta[1].miny, iff_dlta[0].miny);
    *max_x = xaMAX(iff_dlta[1].maxx, iff_dlta[0].maxx);
    *max_y = xaMAX(iff_dlta[1].maxy, iff_dlta[0].maxy);
  }
}

void
IFF_Read_BMHD(XA_INPUT *xin,Bit_Map_Header *bmhd)
{
  /* read Bit_Map_Header into bmhd */
  /* read so as to avoid endian problems */
  bmhd->width              = xin->Read_MSB_U16(xin);
  bmhd->height             = xin->Read_MSB_U16(xin);
  bmhd->x                  = xin->Read_MSB_U16(xin);
  bmhd->y                  = xin->Read_MSB_U16(xin);
  bmhd->depth              = xin->Read_U8(xin);
  bmhd->masking            = xin->Read_U8(xin);
  bmhd->compression        = xin->Read_U8(xin);
  bmhd->pad1               = xin->Read_U8(xin);
  bmhd->transparentColor   = xin->Read_MSB_U16(xin);
  bmhd->xAspect            = xin->Read_U8(xin);
  bmhd->yAspect            = xin->Read_U8(xin);
  bmhd->pageWidth          = xin->Read_MSB_U16(xin);
  bmhd->pageHeight         = xin->Read_MSB_U16(xin);
} 

void
IFF_Read_ANHD(xin,anhd,chunk_size)
XA_INPUT *xin;
Anim_Header *anhd;
xaULONG chunk_size;
{
  xaULONG i;
  anhd->op		= xin->Read_U8(xin);
  anhd->mask		= xin->Read_U8(xin);
  anhd->w		= xin->Read_MSB_U16(xin);
  anhd->h		= xin->Read_MSB_U16(xin);
  anhd->x		= xin->Read_MSB_U16(xin);
  anhd->y		= xin->Read_MSB_U16(xin);
  anhd->abstime		= xin->Read_MSB_U32(xin);
  anhd->reltime		= xin->Read_MSB_U32(xin);
  anhd->interleave	= xin->Read_U8(xin);
  anhd->pad0		= xin->Read_U8(xin);
  anhd->bits		= xin->Read_MSB_U32(xin);
  xin->Read_Block(xin,(xaBYTE *)(anhd->pad),16); /* read pad */
  i = Anim_Header_SIZE;
  while(i < chunk_size) {xin->Read_U8(xin); i++;}
}

void
IFF_Read_ANSQ(xin,chunk_size)
XA_INPUT *xin;
xaULONG chunk_size;
{ xaULONG i;
  xaUBYTE *p;  /* data is actually big endian xaUSHORT */
  xaBYTE *garb;

  iff_ansq_cnt = chunk_size / 4;
  iff_ansq_cnt++; /* adding dlta 0 up front */
  DEBUG_LEVEL2 fprintf(stderr,"    ansq_cnt=%d dlta_cnt=%d\n",
						iff_ansq_cnt,iff_dlta_cnt);
  /* allocate space for ansq variables
  */
  iff_ansq = (IFF_ANSQ_HDR *)malloc( iff_ansq_cnt * sizeof(IFF_ANSQ_HDR));
  if (iff_ansq == NULL) IFF_TheEnd1("IFF_Read_ANSQ: malloc err");
  if (xa_verbose) fprintf(stderr,"     frames=%d dlts=%d comp=%d\n",
			iff_ansq_cnt,iff_dlta_cnt,iff_dlta_compression);
  garb = (xaBYTE *)malloc(chunk_size);
  if (garb==0)
  {
    fprintf(stderr,"ansq malloc not enough\n");
    IFF_TheEnd();
  }
  xin->Read_Block(xin,garb,chunk_size);
  p = (xaUBYTE *)(garb);
  /* first delta is only used once and doesn't appear in
  * the ANSQ
  */
  iff_ansq[0].dnum  = 0;
  iff_ansq[0].time  = 1;
  for(i=1; i<iff_ansq_cnt; i++)
  {
    /* this is delta to apply */
    iff_ansq[i].dnum  = (xaULONG)(*p++)<<8;
    iff_ansq[i].dnum |= (xaULONG)(*p++);
    /* this is jiffy count or if 0xffff then a goto */
    iff_ansq[i].time  = (xaULONG)(*p++)<<8;
    iff_ansq[i].time |= (xaULONG)(*p++);
    iff_ansq[i].frame = 0;
    DEBUG_LEVEL2
    fprintf(stderr,"<%d %d> ",iff_ansq[i].dnum, iff_ansq[i].time);
  }
  FREE(garb,0x100C); garb=0;
}

/*
 * Function to register any CRNGs that occur before CMAP in IFF file
 */
void
IFF_Register_CRNGs(anim_hdr,chdr)
XA_ANIM_HDR *anim_hdr;
XA_CHDR *chdr;
{
  XA_ACTION *act;

  act = (XA_ACTION *)anim_hdr->acts;

  while(act)
  {
    if ( (act->type == ACT_CYCLE) && (act->chdr == 0) )
             ACT_Add_CHDR_To_Action(act,chdr);
    act = act->next;
  }
}


void
IFF_Read_CRNG(anim_hdr,xin,chunk_size,crng_flag)
XA_ANIM_HDR *anim_hdr;
XA_INPUT *xin;
xaULONG chunk_size;
xaULONG *crng_flag;
{
/* CRNG_HDR
 * word pad1,rate,active;
 * byte low,high;
 */

  /* is the chunk the correct size ?
  */
  if (chunk_size == IFF_CRNG_HDR_SIZE)
  {
    XA_ACTION *act;
    xaULONG rate,active,low,high,csize;
    ACT_CYCLE_HDR *act_cycle;

    /* read CRNG chunk */
    rate   = xin->Read_MSB_U16(xin);  /* throw away pad1 */
    rate   = xin->Read_MSB_U16(xin);
    active = xin->Read_MSB_U16(xin);
    low    = xin->Read_U8(xin);
    high   = xin->Read_U8(xin);
    /* make it an action only if its valid
    */
    if (   (active & IFF_CRNG_ACTIVE) && (low < high) 
	&& (rate > IFF_CRNG_DPII_KLUDGE) && (iff_allow_cycling == xaTRUE) )
    {
      xaULONG i,*i_ptr;

      csize = high - low + 1;
      act_cycle = (ACT_CYCLE_HDR *)
	malloc( sizeof(ACT_CYCLE_HDR) + (csize * sizeof(xaULONG)) );
      if (act_cycle == 0) IFF_TheEnd1("IFF_Read_CRNG: malloc failed");

      act_cycle->size = csize;
      act_cycle->curpos = 0;
      act_cycle->rate  = (xaULONG)(IFF_CRNG_INTERVAL/rate);
      act_cycle->flags = ACT_CYCLE_ACTIVE;
      if (active & IFF_CRNG_REVERSE) act_cycle->flags |= ACT_CYCLE_REVERSE;

      i_ptr = (xaULONG *)act_cycle->data;
      for(i=0; i<csize; i++) 
      {
        i_ptr[i] = low + i + iff_or_mask;
      }

      *crng_flag = *crng_flag + 1;
      act = ACT_Get_Action(anim_hdr,ACT_CYCLE);
      IFF_Add_Frame(0,act);
      act->data = (xaUBYTE *) act_cycle;
	/* register it now if iff_chdr valid, else wait to later */
      if (iff_chdr) ACT_Add_CHDR_To_Action(act,iff_chdr);
    }
    else DEBUG_LEVEL2 fprintf(stderr,"IFF_CRNG not used\n");
  }
  else
  { xin->Seek_FPos(xin,chunk_size,1);
    fprintf(stderr,"IFF_CRNG chunksize mismatch %d\n",chunk_size);
  }
}


void
IFF_Read_CMAP_0(ColorReg *cmap,xaULONG size,XA_INPUT *xin)
{
  xaULONG i;
  for(i=0; i < size; i++)
  {
    cmap[i].red   = xin->Read_U8(xin);
    cmap[i].green = xin->Read_U8(xin);
    cmap[i].blue  = xin->Read_U8(xin);
  }
}

void
IFF_Read_CMAP_1(ColorReg *cmap,xaULONG size,XA_INPUT *xin)
{
  xaULONG i;
  for(i=0; i < size; i++)
  {
    xaULONG d;
    d = xin->Read_U8(xin);
    cmap[i].red   = (d & 0x0f) << 4;
    d = xin->Read_U8(xin);
    cmap[i].green = (d & 0xf0);
    cmap[i].blue  = (d & 0x0f) << 4;
  }
}


/*!
 * \param out output image (size of section)
 * \param in input image
 * \param chdr color header to map to
 * \param h_cmap ham color map
 * \param xosize size of section in input buffer
 * \param yosize size of section in input buffer
 * \param xip pos of section in input buffer
 * \param yip pos of section in input buffer
 * \param xisize x size of input buffer
 * \param d_flag map_flag
 */
void IFF_Buffer_HAM6(xaUBYTE *out, xaUBYTE *in, XA_CHDR *chdr, ColorReg *h_cmap, xaULONG xosize, xaULONG yosize, xaULONG xip,xaULONG yip, xaULONG xisize, xaULONG d_flag)
{
  XA_CHDR *the_chdr;
  xaULONG new_cmap_flag,*the_map,psize;
  register xaULONG y,xend,the_moff,coff;
  xaUSHORT g_adj[16];

  if (x11_display_type & XA_X11_TRUE) 
	for(y=0;y<16;y++) g_adj[y] = xa_gamma_adj[ (17 * y) ];

  the_map = chdr->map;
  coff = chdr->coff;
  if (chdr->new_chdr == 0) { the_chdr = chdr; new_cmap_flag = 0; }
  else { the_chdr = chdr->new_chdr; new_cmap_flag = 1; }
  the_moff = the_chdr->moff;
  if (x11_display_type & XA_X11_TRUE) d_flag = xaTRUE;
  if (d_flag==xaTRUE) psize = x11_bytes_pixel;
  else psize = 1;

  DEBUG_LEVEL1 fprintf(stderr,"ham_cmap: = %x\n",(xaULONG)h_cmap);

  if (xa_ham_map == 0)
  {
     xa_ham_map_size = XA_HAM6_CACHE_SIZE;
     xa_ham_map = (xaULONG *)malloc( xa_ham_map_size * sizeof(xaULONG) );
     if (xa_ham_map == 0) IFF_TheEnd1("IFF_Buffer_HAM6: h_map malloc err");
  }
  if ((the_chdr != xa_ham_chdr) || (xa_ham_init != 6))
  {
    register xaULONG i;
    DEBUG_LEVEL1 fprintf(stderr,"xa_ham_map: old = %x new = %x\n",
					(xaULONG)xa_ham_chdr,(xaULONG)the_chdr);
    for(i=0; i<XA_HAM6_CACHE_SIZE; i++) xa_ham_map[i] = XA_HAM_MAP_INVALID;
    xa_ham_chdr = the_chdr; xa_ham_init = 6;
  }

  xend = xip + xosize; if (xend > xisize) xend = xisize;
  for (y=yip; y < (yip + yosize); y++)
  {
    register xaULONG x;
    register xaUBYTE *i_ptr = (xaUBYTE *)( in + y * xisize );
    register xaUBYTE *o_ptr = (xaUBYTE *)( out + (y-yip)*xosize * psize );
    register xaULONG pred,pgrn,pblu,data;
    pred = pgrn = pblu = 0;
    for (x=0; x<xend; x++)
    {
      data = (xaUSHORT )(*i_ptr++);
      switch(data & 0x30)
      {
        case 0x00: /* use color register given by low */
          { register xaUSHORT low = data & 0x0f;
            pred = h_cmap[low].red;
            pgrn = h_cmap[low].green;
            pblu = h_cmap[low].blue;
          } break;
        case 0x10: pblu = data & 0x0f; break; /* change blue */
        case 0x20: pred = data & 0x0f; break; /* change red */
        case 0x30: pgrn = data & 0x0f; break; /* change green */
      }
      if ( (x >= xip) && (x < xend) )
      {
        register xaULONG t_color;
        register xaUSHORT indx = (pred << 8) | (pgrn << 4) | pblu;

        if ( (t_color = xa_ham_map[indx]) == XA_HAM_MAP_INVALID) 
        {
          if (x11_display_type & XA_X11_TRUE) t_color = 
		X11_Get_True_Color( g_adj[pred],g_adj[pgrn],g_adj[pblu],16);
	  else /* don't gamma because chdr already adjusted */
	  {
            if (cmap_true_to_332 == xaTRUE)
		 t_color = CMAP_GET_332(pred,pgrn,pblu,CMAP_SCALE4);
	    else t_color = CMAP_GET_GRAY(pred,pgrn,pblu,CMAP_SCALE9);
            if (new_cmap_flag) t_color = the_map[t_color - coff] + the_moff;
	  }
          xa_ham_map[indx] = t_color;
	}

	if (d_flag)
	{
          if (x11_bytes_pixel == 4)
             { xaULONG *ulp = (xaULONG *)o_ptr; *ulp = t_color; o_ptr += 4; }
          else if (x11_bytes_pixel == 2) { xaUSHORT *usp = (xaUSHORT *)o_ptr;
			 *usp = (xaUSHORT)(t_color); o_ptr += 2; }
          else *o_ptr++ = (xaUBYTE)t_color;
	} else *o_ptr++ = (xaUBYTE)t_color;
      } /* end of output */
    } /* end of x */
  } /* end of y */
}


void IFF_Buffer_HAM8(xaUBYTE *out, xaUBYTE *in, XA_CHDR *chdr, ColorReg *h_cmap, xaULONG xosize, xaULONG yosize, xaULONG xip,xaULONG yip, xaULONG xisize, xaULONG d_flag)
{
  XA_CHDR *the_chdr;
  xaULONG new_cmap_flag,*the_map,psize;
  register xaULONG y,xend,the_moff,coff;
  xaUSHORT g_adj[64];

  if (x11_display_type & XA_X11_TRUE) 
	for(y=0;y<64;y++) g_adj[y] = xa_gamma_adj[ ((65 * y) >> 4) ];
  the_map = chdr->map;
  coff = chdr->coff;
  if (chdr->new_chdr == 0) { the_chdr = chdr; new_cmap_flag = 0; }
  else { the_chdr = chdr->new_chdr; new_cmap_flag = 1; }
  the_moff = chdr->moff;
  if (x11_display_type & XA_X11_TRUE) d_flag = xaTRUE;
  if (d_flag==xaTRUE) psize = x11_bytes_pixel;
  else psize = 1;

  if (xa_ham_map_size != XA_HAM8_CACHE_SIZE)
  {
    if (xa_ham_map == 0)
    {
         xa_ham_map_size = XA_HAM8_CACHE_SIZE;
         xa_ham_map = (xaULONG *)malloc( xa_ham_map_size * sizeof(xaULONG) );
         if (xa_ham_map == 0) IFF_TheEnd1("IFF_Buffer_HAM8: h_map malloc err");
    }
    else
    {
      xaULONG *tmp;
      xa_ham_map_size = XA_HAM8_CACHE_SIZE;
      tmp = (xaULONG *) realloc(xa_ham_map,xa_ham_map_size * sizeof(xaULONG));
      if (tmp == 0) IFF_TheEnd1("IFF_Buffer_HAM8: h_map malloc err");
      xa_ham_map = tmp;
    }
    xa_ham_chdr = 0;
  }
  if ( (the_chdr != xa_ham_chdr) || (xa_ham_init != 8))
  {
    register xaULONG i;
    DEBUG_LEVEL1 fprintf(stderr,"xa_ham8_map: old = %x new = %x\n",
					(xaULONG)xa_ham_chdr,(xaULONG)the_chdr);
    for(i=0; i<XA_HAM8_CACHE_SIZE; i++) xa_ham_map[i] = XA_HAM_MAP_INVALID;
    xa_ham_chdr = the_chdr; xa_ham_init = 8;
  }

  xend = xip + xosize; if (xend > xisize) xend = xisize;
  for (y=yip; y < (yip + yosize); y++)
  {
    register xaULONG x;
    register xaUBYTE *i_ptr = (xaUBYTE *)( in + y * xisize );
    register xaUBYTE *o_ptr = (xaUBYTE *)( out + (y-yip) *xosize * psize);
    register xaULONG pred,pgrn,pblu,data;

    pred = pgrn = pblu = 0;
    for (x=0; x<xend; x++)
    {
      data = (xaULONG )(*i_ptr++);
      switch(data & 0xc0)
      {
        case 0x00: /* use color register given by low */
          { register xaULONG low = data & 0x3f;
            pred  = h_cmap[low].red;
            pgrn  = h_cmap[low].green;
            pblu  = h_cmap[low].blue;
          } break;
        case 0x40: pblu = data & 0x3f; break; /* change blue */
        case 0x80: pred = data & 0x3f; break; /* change red */
        case 0xc0: pgrn = data & 0x3f; break; /* change green */
      }
      if ( (x >= xip) && (x < xend) )
      {
        register xaULONG t_color;
        register xaULONG indx = (pred << 12) | (pgrn << 6) | pblu;

        if ( (t_color = xa_ham_map[indx]) == XA_HAM_MAP_INVALID) 
        {
          if (x11_display_type & XA_X11_TRUE) t_color = 
		X11_Get_True_Color( g_adj[pred],g_adj[pgrn],g_adj[pblu],16);
	  else /* no gamma here because it's already in cmap */
	  {
            if (cmap_true_to_332 == xaTRUE)
		 t_color = CMAP_GET_332(pred,pgrn,pblu,CMAP_SCALE6);
	    else t_color = CMAP_GET_GRAY(pred,pgrn,pblu,CMAP_SCALE11);
            if (new_cmap_flag) t_color = the_map[t_color - coff] + the_moff;
	  }
          xa_ham_map[indx] = t_color;
	}
	if (d_flag)
	{
          if (x11_bytes_pixel == 4)
             { xaULONG *ulp = (xaULONG *)o_ptr; *ulp = t_color; o_ptr += 4; }
          else if (x11_bytes_pixel == 2) { xaUSHORT *usp = (xaUSHORT *)o_ptr;
                         *usp = (xaUSHORT)(t_color); o_ptr += 2; }
          else *o_ptr++ = (xaUBYTE)t_color;
	} else *o_ptr++ = (xaUBYTE)t_color;
      } /* end of output */
    } /* end of x */
  } /* end of y */
}

void
IFF_Shift_CMAP(ColorReg *cmap,xaULONG csize)
{ xaULONG i;
  for(i=0;i<csize;i++)
  { cmap[i].red   >>= 4;
    cmap[i].green >>= 4;
    cmap[i].blue  >>= 4;
  }
}

 /* Extra Info. 0 = short encoding, 1 = long encoding*/
xaULONG
IFF_Delta_7(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG imaged = dec_info->imaged;
  void *extra = dec_info->extra;
 register xaLONG col,depth,dmask;
 register xaLONG rowsize,width;
 xaULONG poff,doff,col_shift;
 register xaUBYTE *i_ptr;
 register xaUBYTE *optr,opcnt,op,cnt;
 xaUBYTE *d_ptr;
 xaLONG miny,minx,maxy,maxx;
 xaULONG iextra = (xaULONG)(extra);

 /* set to opposites for min/max testing */
 dec_info->xs = imagex;
 dec_info->ys = imagey;
 dec_info->xe = 0;
 dec_info->ye = 0;

 i_ptr = image;
 width = imagex;
 col_shift = (iextra)?4:5;
 rowsize = width >> col_shift;
 dmask = 1;
 for(depth=0; depth<imaged; depth++)
 {
  minx = -1;
  maxx = -1;
  
  i_ptr = image;
  /* offset into delt chunk */
  { register xaULONG ddepth = depth << 2;
    poff  = (xaULONG)(delta[ ddepth++ ]) << 24;
    poff |= (xaULONG)(delta[ ddepth++ ]) << 16;
    poff |= (xaULONG)(delta[ ddepth++ ]) <<  8;
    poff |= (xaULONG)(delta[ ddepth   ]);
    ddepth +=29; /* move to data */
    doff  = (xaULONG)(delta[ ddepth++ ]) << 24;
    doff |= (xaULONG)(delta[ ddepth++ ]) << 16;
    doff |= (xaULONG)(delta[ ddepth++ ]) <<  8;
    doff |= (xaULONG)(delta[ ddepth   ]);
  }
  if (poff)
  {
   optr   = (xaUBYTE  *)(delta + poff);
   d_ptr =  (xaUBYTE *)(delta + doff);
   
   for(col=0;col<rowsize;col++)
   {
	/* start at top of column */
    i_ptr = (xaUBYTE *)(image + (col << col_shift));
    opcnt = *optr++;  /* get number of ops for this column */
    
    miny = -1;
    maxy = -1;

    while(opcnt)    /* execute ops */
    {
      /* keep track of min and max columns */
      if (minx == -1) minx = col;
      maxx = col;

      op = *optr++;   /* get op */
     
      if (iextra) /* xaSHORT Data */
      {
        if (op & 0x80)    /* if type uniqe */
        {
          if (miny == -1) miny = (xaULONG)(i_ptr - image ) / width;
          cnt = op & 0x7f;         /* get cnt */
      
          while(cnt--) /* loop through data */
          {
             register xaULONG data;
	     data = (*d_ptr++) << 8; data |= *d_ptr++;
             IFF_Short_Mod(i_ptr,data,dmask,0);
             i_ptr += width;
          }
        } /* end unique */
        else
        {
          if (op == 0)   /* type same */
          {
             register xaUSHORT data;
             if (miny == -1) miny=(xaULONG)(i_ptr - image) / width;
             cnt = *optr++;
	     data = (*d_ptr++) << 8; data |= *d_ptr++;

             while(cnt--) /* loop through data */
             { 
                IFF_Short_Mod(i_ptr,data,dmask,0);
                i_ptr += width;
             }
           } /* end same */
           else
           {
              i_ptr += (width * op);  /* type skip */
           }
         } /* end of hi bit clear */
      } /* end of short data */
      else /* xaLONG Data */
      {
        if (op & 0x80)    /* if type uniqe */
        {
          if (miny == -1) miny=(xaULONG)( i_ptr - image ) / width;
          cnt = op & 0x7f;         /* get cnt */
      
          while(cnt--) /* loop through data */
          { register xaULONG data;
	    data = (*d_ptr++)<<24; data |= (*d_ptr++)<<16;
	    data |= (*d_ptr++)<<8; data |= *d_ptr++;
            IFF_Long_Mod(i_ptr,data,dmask,0); i_ptr += width;
          }
        } /* end unique */
        else
        {
           if (op == 0) /* type same */
           { register xaULONG data;
             if (miny == -1) miny=(xaULONG)( i_ptr - image ) / width;
             cnt = *optr++;
	     data = (*d_ptr++)<<24; data |= (*d_ptr++)<<16;
	     data |= (*d_ptr++)<<8; data |= *d_ptr++;

             while(cnt--) {IFF_Long_Mod(i_ptr,data,dmask,0); i_ptr += width;}
           } /* end same */
           else { i_ptr += (width * op);  /* type skip */ }
         } /* end of hi bit clear */
       } /* end of long data */
       opcnt--;
     } /* end of while opcnt */
     maxy = (xaULONG)( i_ptr - image ) / width;
     if ( (miny>=0) && (miny < dec_info->ys)) dec_info->ys = miny;
     if ( (maxy>=0) && (maxy > dec_info->ye)) dec_info->ye = maxy;
    } /* end of column loop */
   } /* end of valid pointer for this plane */
   dmask <<= 1;
   minx <<= col_shift; maxx <<= col_shift;
   maxx = (iextra)?(maxx+15):(maxx+31);
   if ( (minx>=0) && (minx < dec_info->xs)) dec_info->xs = minx;
   if ( (maxx>=0) && (maxx > dec_info->xe)) dec_info->xe = maxx;
  } /* end of for depth */

  if (xa_optimize_flag == xaTRUE)
  {
    if (dec_info->xs >= imagex) dec_info->xs = 0;
    if (dec_info->ys >= imagey) dec_info->ys = 0;
    if (dec_info->xe <= 0)      dec_info->xe = imagex;
    if (dec_info->ye <= 0)      dec_info->ye = imagey;
  }
  else
  {
    dec_info->xs = 0;      dec_info->ys = 0;
    dec_info->xe = imagex; dec_info->ye = imagey;
  }
  return(ACT_DLTA_NORM);
} /* end of routine */



/* Extra Info. 0 = short encoding, 1 = long encoding*/
xaULONG
IFF_Delta_8(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG imaged = dec_info->imaged;
  void *extra = dec_info->extra;
  register xaLONG col,depth,dmask;
  register xaLONG rowsize,width;
  xaULONG poff,col_shift;
  register xaUBYTE *i_ptr;
  register xaUBYTE *optr;
  register xaULONG opcnt,op,cnt;
  xaLONG miny,minx,maxy,maxx;
  xaULONG iextra = (xaULONG)(extra);

 /* set to opposites for min/max testing */
  dec_info->xs = imagex;
  dec_info->ys = imagey;
  dec_info->xe = 0;
  dec_info->ye = 0;

 i_ptr = image;
 width = imagex;
 col_shift = (iextra)?4:5;
 rowsize = width >> col_shift;
 dmask = 1;
 for(depth=0; depth<imaged; depth++)
 {
  minx = -1;
  maxx = -1;
  
  i_ptr = image;
  /* offset into delt chunk */
  { register xaULONG ddepth = depth << 2;
    poff  = (xaULONG)(delta[ ddepth++ ]) << 24;
    poff |= (xaULONG)(delta[ ddepth++ ]) << 16;
    poff |= (xaULONG)(delta[ ddepth++ ]) <<  8;
    poff |= (xaULONG)(delta[ ddepth   ]);
  }
  if (poff)
  {
   optr   = (xaUBYTE  *)(delta + poff);
   
   for(col=0;col<rowsize;col++)
   {
     /* start at top of column */
     i_ptr = (xaUBYTE *)(image + (col << col_shift));
     if (iextra) /* xaSHORT Data */
     {
       opcnt  = (xaULONG)(*optr++) << 8;  /* get number of ops for this column */
       opcnt |= (xaULONG)(*optr++);
    
       miny = -1;
       maxy = -1;

       while(opcnt)    /* execute ops */
       {
	 /* keep track of min and max columns */
	 if (minx == -1) minx = col;
	 maxx = col;

	 op  = (xaULONG)(*optr++) << 8;   /* get op */
	 op |= (xaULONG)(*optr++);
     
	 if (op & 0x8000)    /* if type uniqe */
	 {
	   if (miny == -1) miny = (xaULONG)(i_ptr - image ) / width;
	   cnt = op & 0x7fff;         /* get cnt */
      
	   while(cnt--) /* loop through data */
	   {
             register xaULONG data;
	     data = (*optr++) << 8; data |= *optr++;
             IFF_Short_Mod(i_ptr,data,dmask,0);
             i_ptr += width;
	   }
	 } /* end unique */
	 else
	 {
	   if (op == 0)   /* type same */
	   {
             register xaUSHORT data;
             if (miny == -1) miny=(xaULONG)(i_ptr - image) / width;
	     cnt  = (xaULONG)(*optr++) << 8;
	     cnt |= (xaULONG)(*optr++);
	     data = (*optr++) << 8; data |= *optr++;

             while(cnt--) /* loop through data */
	     { 
	       IFF_Short_Mod(i_ptr,data,dmask,0);
	       i_ptr += width;
             }
           } /* end same */
           else
           {
	     i_ptr += (width * op);  /* type skip */
           }
         } /* end of hi bit clear */
	 opcnt--;
       } /* end of while opcnt */
     } /* end of short data */
     else /* xaLONG Data */
     {
       opcnt  = (xaULONG)(*optr++) << 24; /* get number of ops for this column */
       opcnt |= (xaULONG)(*optr++) << 16;
       opcnt |= (xaULONG)(*optr++) <<  8;
       opcnt |= (xaULONG)(*optr++);
    
       miny = -1;
       maxy = -1;

       while(opcnt)    /* execute ops */
       {
	 /* keep track of min and max columns */
	 if (minx == -1) minx = col;
	 maxx = col;

	 op  = (xaULONG)(*optr++) << 24;   /* get op */
	 op |= (xaULONG)(*optr++) << 16;
	 op |= (xaULONG)(*optr++) <<  8;
	 op |= (xaULONG)(*optr++);

	 if (op & 0x80000000)    /* if type uniqe */
	 {
	   if (miny == -1) miny=(xaULONG)( i_ptr - image ) / width;
	   cnt = op & 0x7fffffff;         /* get cnt */
      
	   while(cnt--) /* loop through data */
	   { register xaULONG data;
	     data = (*optr++)<<24; data |= (*optr++)<<16;
	     data |= (*optr++)<<8; data |= *optr++;
	     IFF_Long_Mod(i_ptr,data,dmask,0); i_ptr += width;
	   }
	 } /* end unique */
	 else
	 {
           if (op == 0) /* type same */
	   { register xaULONG data;
             if (miny == -1) miny=(xaULONG)( i_ptr - image ) / width;
	     cnt  = (xaULONG)(*optr++) << 24;
	     cnt |= (xaULONG)(*optr++) << 16;
	     cnt |= (xaULONG)(*optr++) <<  8;
	     cnt |= (xaULONG)(*optr++);
	     data = (*optr++)<<24; data |= (*optr++)<<16;
	     data |= (*optr++)<<8; data |= *optr++;

             while(cnt--) {IFF_Long_Mod(i_ptr,data,dmask,0); i_ptr += width;}
           } /* end same */
           else { i_ptr += (width * op);  /* type skip */ }
         } /* end of hi bit clear */
	 opcnt--;
       } /* end of while opcnt */
     } /* end of long data */
     maxy = (xaULONG)( i_ptr - image ) / width;
     if ( (miny>=0) && (miny < dec_info->ys)) dec_info->ys = miny;
     if ( (maxy>=0) && (maxy > dec_info->ye)) dec_info->ye = maxy;
    } /* end of column loop */
   } /* end of valid pointer for this plane */
   dmask <<= 1;
   minx <<= col_shift; maxx <<= col_shift;
   maxx = (iextra)?(maxx+15):(maxx+31);
   if ( (minx>=0) && (minx < dec_info->xs)) dec_info->xs = minx;
   if ( (maxx>=0) && (maxx > dec_info->xe)) dec_info->xe = maxx;
  } /* end of for depth */

  if (xa_optimize_flag == xaTRUE)
  {
    if (dec_info->xs >= imagex) dec_info->xs = 0;
    if (dec_info->ys >= imagey) dec_info->ys = 0;
    if (dec_info->xe <= 0)      dec_info->xe = imagex;
    if (dec_info->ye <= 0)      dec_info->ye = imagey;
  }
  else
  {
    dec_info->xs = 0;      dec_info->ys = 0;
    dec_info->xe = imagex; dec_info->ye = imagey;
  }
  return(ACT_DLTA_NORM);
} /* end of routine */


/* POD NOTE: no need to support xorflag yet */
void IFF_Long_Mod(ptr,data,dmask,xorflag) 
xaUBYTE *ptr;
xaULONG data,dmask,xorflag;
{ register xaUBYTE *_iptr = ptr; 
  register xaUBYTE dmaskoff = ~dmask;
  if (xorflag) TheEnd1("IFF comp7: xorflag not supported yet. Contact Author");
  if (0x80000000 & data)  *_iptr++  |= dmask; else  *_iptr++  &= dmaskoff;
  if (0x40000000 & data)  *_iptr++  |= dmask; else  *_iptr++  &= dmaskoff;
  if (0x20000000 & data)  *_iptr++  |= dmask; else  *_iptr++  &= dmaskoff;
  if (0x10000000 & data)  *_iptr++  |= dmask; else  *_iptr++  &= dmaskoff;
  if (0x08000000 & data)  *_iptr++  |= dmask; else  *_iptr++  &= dmaskoff;
  if (0x04000000 & data)  *_iptr++  |= dmask; else  *_iptr++  &= dmaskoff;
  if (0x02000000 & data)  *_iptr++  |= dmask; else  *_iptr++  &= dmaskoff;
  if (0x01000000 & data)  *_iptr++  |= dmask; else  *_iptr++  &= dmaskoff;
  if (0x00800000 & data)  *_iptr++  |= dmask; else  *_iptr++  &= dmaskoff;
  if (0x00400000 & data)  *_iptr++  |= dmask; else  *_iptr++  &= dmaskoff;
  if (0x00200000 & data) *_iptr++   |= dmask; else *_iptr++   &= dmaskoff;
  if (0x00100000 & data) *_iptr++   |= dmask; else *_iptr++   &= dmaskoff;
  if (0x00080000 & data) *_iptr++   |= dmask; else *_iptr++   &= dmaskoff;
  if (0x00040000 & data) *_iptr++   |= dmask; else *_iptr++   &= dmaskoff;
  if (0x00020000 & data) *_iptr++   |= dmask; else *_iptr++   &= dmaskoff;
  if (0x00010000 & data) *_iptr++   |= dmask; else *_iptr++   &= dmaskoff;
  if (0x00008000 & data) *_iptr++   |= dmask; else *_iptr++   &= dmaskoff;
  if (0x00004000 & data) *_iptr++   |= dmask; else *_iptr++   &= dmaskoff;
  if (0x00002000 & data) *_iptr++   |= dmask; else *_iptr++   &= dmaskoff;
  if (0x00001000 & data) *_iptr++   |= dmask; else *_iptr++   &= dmaskoff;
  if (0x00000800 & data) *_iptr++   |= dmask; else *_iptr++   &= dmaskoff;
  if (0x00000400 & data) *_iptr++   |= dmask; else *_iptr++   &= dmaskoff;
  if (0x00000200 & data) *_iptr++   |= dmask; else *_iptr++   &= dmaskoff;
  if (0x00000100 & data) *_iptr++   |= dmask; else *_iptr++   &= dmaskoff;
  if (0x00000080 & data) *_iptr++   |= dmask; else *_iptr++   &= dmaskoff;
  if (0x00000040 & data) *_iptr++   |= dmask; else *_iptr++   &= dmaskoff;
  if (0x00000020 & data) *_iptr++   |= dmask; else *_iptr++   &= dmaskoff;
  if (0x00000010 & data) *_iptr++   |= dmask; else *_iptr++   &= dmaskoff;
  if (0x00000008 & data) *_iptr++   |= dmask; else *_iptr++   &= dmaskoff;
  if (0x00000004 & data) *_iptr++   |= dmask; else *_iptr++   &= dmaskoff;
  if (0x00000002 & data) *_iptr++   |= dmask; else *_iptr++   &= dmaskoff;
  if (0x00000001 & data) *_iptr     |= dmask; else *_iptr     &= dmaskoff;
}




void IFF_HAM6_As_True(out,in,chdr,h_cmap,xosize,yosize,xip,yip,xisize)
xaUBYTE *out,*in;
XA_CHDR **chdr;
ColorReg *h_cmap;
xaULONG xosize,yosize,xip,yip,xisize;
{
  xaULONG xend,y;
  xaUBYTE *tpic,*optr;
  xaUSHORT g_adj[16];

  for(y=0;y<16;y++) g_adj[y] = xa_gamma_adj[ (17 * y) ] >> 8;
  tpic = (xaUBYTE *)malloc( 3 * xosize * yosize);
  if (tpic == 0) TheEnd1("IFF_HAM6_As_True: malloc err\n");
  optr = tpic;
  xend = xip + xosize; if (xend > xisize) xend = xisize;
  for (y=yip; y < (yip + yosize); y++)
  {
    register xaULONG x;
    register xaUBYTE *i_ptr = (xaUBYTE *)( in + y * xisize );
    register xaULONG pred,pgrn,pblu,data;

    pred = pgrn = pblu = 0;
    for (x=0; x<xend; x++)
    {
      data = (xaULONG )(*i_ptr++);
      switch( (data & 0x30) )
      {
        case 0x00: /* use color register given by low */
          { register xaULONG low;
            low = data & 0x0f;
            pred  = g_adj[ h_cmap[low].red   ];
            pgrn  = g_adj[ h_cmap[low].green ];
            pblu  = g_adj[ h_cmap[low].blue  ];
          } break;
        case 0x10: pblu = g_adj[ (data & 0x0f) ]; break;
        case 0x20: pred = g_adj[ (data & 0x0f) ]; break;
        case 0x30: pgrn = g_adj[ (data & 0x0f) ]; break;
      } 
      if ( (x >= xip) && (x < xend) )
      {
	*optr++ = pred;
	*optr++ = pgrn;
	*optr++ = pblu;
      }
    } /* end of x */
  } /* end of y */
  if (    (cmap_true_to_all == xaTRUE) 
      || ((cmap_true_to_1st == xaTRUE) && (iff_chdr == 0)) )	
	iff_chdr = CMAP_Create_CHDR_From_True(tpic,8,8,8,xosize,yosize,
					iff_cmap,&iff_imagec);
  else if ( (cmap_true_to_332 == xaTRUE) && (iff_chdr == 0) )
	iff_chdr = CMAP_Create_332(iff_cmap,&iff_imagec);
  else if ( (cmap_true_to_gray == xaTRUE) && (iff_chdr == 0) )
	iff_chdr = CMAP_Create_Gray(iff_cmap,&iff_imagec);
 
  if (cmap_dither_type == CMAP_DITHER_FLOYD)
        out = UTIL_RGB_To_FS_Map(out,tpic,iff_chdr,xosize,yosize,xaTRUE);
  else
        out = UTIL_RGB_To_Map(out,tpic,iff_chdr,xosize,yosize,xaTRUE);
  *chdr = iff_chdr; 
}

void IFF_HAM8_As_True(out,in,chdr,h_cmap,xosize,yosize,xip,yip,xisize)
xaUBYTE *out,*in;
XA_CHDR **chdr;
ColorReg *h_cmap;
xaULONG xosize,yosize,xip,yip,xisize;
{
  xaULONG xend,y;
  xaUBYTE *tpic,*optr;
  xaUSHORT g_adj[64];

  for(y=0;y<64;y++) g_adj[y] = xa_gamma_adj[ ((65 * y) >> 4) ] >> 8;
  tpic = (xaUBYTE *)malloc( 3 * xosize * yosize);
  if (tpic == 0) TheEnd1("IFF_HAM8_As_True: malloc err\n");
  optr = tpic;
  xend = xip + xosize; if (xend > xisize) xend = xisize;
  for (y=yip; y < (yip + yosize); y++)
  {
    register xaULONG x;
    register xaUBYTE *i_ptr = (xaUBYTE *)( in + y * xisize );
    register xaULONG pred,pgrn,pblu,data;

    pred = pgrn = pblu = 0;
    for (x=0; x<xend; x++)
    {
      data = (xaULONG )(*i_ptr++);
      switch( (data & 0xc0) )
      {
        case 0x00: /* use color register given by low */
          { register xaULONG low = data & 0x3f;
            pred  = g_adj[ h_cmap[low].red   ];
            pgrn  = g_adj[ h_cmap[low].green ];
            pblu  = g_adj[ h_cmap[low].blue  ];
          } break;
        case 0x40: pblu = g_adj[ (data & 0x3f) ]; break;
        case 0x80: pred = g_adj[ (data & 0x3f) ]; break;
        case 0xc0: pgrn = g_adj[ (data & 0x3f) ]; break;
      } 
      if ( (x >= xip) && (x < xend) )
      {
	*optr++ = pred;
	*optr++ = pgrn;
	*optr++ = pblu;
      }
    } /* end of x */
  } /* end of y */
  if (    (cmap_true_to_all == xaTRUE) 
      || ((cmap_true_to_1st == xaTRUE) && (iff_chdr == 0)) )	
	iff_chdr = CMAP_Create_CHDR_From_True(tpic,8,8,8,xosize,yosize,
					iff_cmap,&iff_imagec);
  else if ( (cmap_true_to_332 == xaTRUE) && (iff_chdr == 0) )
	iff_chdr = CMAP_Create_332(iff_cmap,&iff_imagec);
  else if ( (cmap_true_to_gray == xaTRUE) && (iff_chdr == 0) )
	iff_chdr = CMAP_Create_Gray(iff_cmap,&iff_imagec);
 
  if (cmap_dither_type == CMAP_DITHER_FLOYD)
        out = UTIL_RGB_To_FS_Map(out,tpic,iff_chdr,xosize,yosize,xaTRUE);
  else
        out = UTIL_RGB_To_Map(out,tpic,iff_chdr,xosize,yosize,xaTRUE);
  *chdr = iff_chdr; 
}


xaULONG
IFF_Delta_Body(image,delta,dsize,dec_info)
xaUBYTE *image;         /* Image Buffer. */
xaUBYTE *delta;         /* delta data. */
xaULONG dsize;          /* delta size */
XA_DEC_INFO *dec_info;  /* Decoder Info Header */
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG image_size = imagex * imagey;
  memcpy( (char *)image, (char *)delta, image_size);
  dec_info->xs = dec_info->ys = 0;  dec_info->xe = imagex; dec_info->ye = imagey; 
  return(ACT_DLTA_BODY);
}

