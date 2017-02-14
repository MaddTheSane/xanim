
/*
 * xanim_iff.c
 *
 * Copyright (C) 1990,1991,1992,1993,1994,1995 by Mark Podlipec. 
 * All rights reserved.
 *
 * This software may be freely copied, modified and redistributed without
 * fee for non-commerical purposes provided that this copyright notice is
 * preserved intact on all copies and modified copies.
 * 
 * There is no warranty or other guarantee of fitness of this software.
 * It is provided solely "as is". The author(s) disclaim(s) all
 * responsibility and liability with respect to this software's usage
 * or its effect upon hardware or computer systems.
 *
 */
#include "xanim.h"
#include "xanim_iff.h"

ULONG IFF_Read_File();
void IFF_Adjust_For_EHB();
void IFF_Read_BODY();
LONG IFF_Read_Garb();
void IFF_Print_ID();
ULONG IFF_Delta_Body();
ULONG IFF_Delta_l();
ULONG IFF_Delta_3();
ULONG IFF_Delta_5();
ULONG IFF_Delta_7();
ULONG IFF_Delta_8();
ULONG IFF_Delta_J();
void IFF_Long_Mod();
LONG Is_IFF_File();
void IFF_Buffer_Action();
void IFF_Buffer_HAM6();
void IFF_Buffer_HAM8();
void IFF_Setup_HMAP();
void IFF_Setup_CMAP();
IFF_ACT_LST *IFF_Add_Frame();
void IFF_Image_To_Bufferable();
void IFF_Read_BMHD();
void IFF_Read_ANHD();
void IFF_Read_ANSQ();
void IFF_Register_CRNGs();
void IFF_Read_CRNG();
void IFF_Read_CMAP_0();
void IFF_Read_CMAP_1();
void IFF_Init_DLTA_HDR();
void IFF_Update_DLTA_HDR();
void IFF_Free_Stuff();
void IFF_Shift_CMAP();
void IFF_HAM6_As_True();
void IFF_HAM8_As_True();
void IFF_Hash_CleanUp();
void IFF_Hash_Init();
void IFF_Hash_Add();
XA_ACTION *IFF_Hash_Get();
ULONG IFF_Check_Same();


ULONG UTIL_Get_MSB_Long();
LONG UTIL_Get_MSB_Short();
ULONG UTIL_Get_MSB_UShort();
ULONG CMAP_Get_Or_Mask();
XA_CHDR *CMAP_Create_332();
XA_CHDR *CMAP_Create_Gray();
XA_CHDR *CMAP_Create_CHDR_From_True();
void ACT_Add_CHDR_To_Action();
void ACT_Del_CHDR_From_Action();
void UTIL_Mapped_To_Bitmap();
void UTIL_Mapped_To_Mapped();
UBYTE *UTIL_RGB_To_Map();
UBYTE *UTIL_RGB_To_FS_Map();
void ACT_Free_Act();
ULONG UTIL_Get_Buffer_Scale();
void UTIL_Scale_Pos_Size();

extern LONG xa_anim_cycling;

XA_ACTION *ACT_Get_Action();
void ACT_Setup_Mapped();
void UTIL_Sub_Image();
XA_CHDR *ACT_Get_CMAP();

#define IFF_SPEED_DEFAULT 3
#define ACT_IFF_HMAP6 0x2000
#define ACT_IFF_HMAP8 0x2001

typedef struct
{
  ULONG flag;
  XA_ACTION *dlta;
  XA_ACTION *src;
  XA_ACTION *dst;
  XA_ACTION *nxtdlta;
} IFF_HASH;

IFF_HASH *iff_hash_tbl;
ULONG iff_hash_cur = 0;


static IFF_ANSQ *iff_ansq;
static ULONG iff_ansq_cnt;
static IFF_DLTA_TABLE *iff_dlta_acts;

static LONG  iff_allow_cycling;
static ULONG iff_time;
static ULONG iff_anim_flags;
static ULONG iff_cmap_bits;
static ColorReg iff_hmap[XA_HMAP_SIZE];
static ColorReg *iff_cur_hmap;
static ColorReg iff_cmap[256];
static XA_CHDR *iff_chdr;

static LONG mask[8] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};

static IFF_ACT_LST *iff_act_start,*iff_act_cur;
static ULONG iff_act_cnt;
static ULONG iff_dlta_cnt;
static ULONG iff_dlta_compression,iff_dlta_bits;

static ULONG iff_imagex,iff_imagey,iff_imagec,iff_imaged;
static ULONG iff_max_imagex,iff_max_imagey,iff_max_imagec,iff_max_imaged;
static ULONG iff_or_mask;
static ULONG iff_max_fvid_size;

extern ULONG xa_ham_map_size;
extern ULONG *xa_ham_map;
extern XA_CHDR *xa_ham_chdr;
extern ULONG xa_ham_init;

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
ULONG type;
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
ULONG IFF_Read_File(fname,anim_hdr)
BYTE *fname;
XA_ANIM_HDR *anim_hdr;
{
  FILE *fin;
  LONG camg_flag,cmap_flag,chdr_flag,ret;
  LONG crng_flag,formtype;
  LONG ansq_flag;
  LONG face_flag,bmhd_flag,file_size,file_read;
  Bit_Map_Header bmhd;
  Chunk_Header  chunk;
  LONG prop_flag;
  LONG exit_flag;


  iff_allow_cycling = (anim_hdr->anim_flags & ANIM_CYCLE)?(TRUE):(FALSE);
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
  cmap_flag	= FALSE;
  chdr_flag	= FALSE;
  ansq_flag	= 0;
  iff_ansq	= 0;
  iff_ansq_cnt	= 0;
  iff_dlta_acts	= 0;
  iff_dlta_cnt	= 0;
  iff_chdr	= 0;
  iff_cmap_bits = 4;
  iff_cur_hmap  = 0;

  if ( (fin=fopen(fname,XA_OPEN_MODE)) == 0)
  { 
    fprintf(stderr,"can't open %s\n",fname); 
    return(FALSE);
  }

  exit_flag = 0; 
  while( !feof(fin) && !exit_flag)
  {
    /* read Chunk_Header 
    */
    chunk.id   = UTIL_Get_MSB_Long(fin);
    chunk.size = UTIL_Get_MSB_Long(fin);

    if ( feof(fin) ) break;    
    if (chunk.size == -1) ret = -1;
    else ret = 0;

    DEBUG_LEVEL1
    {
      fprintf(stderr,"Chunk.ID = ");
      IFF_Print_ID(stderr,chunk.id);
      fprintf(stderr,"  chunksize=%lx\n",chunk.size);
    }
    if (chunk.size & 1) chunk.size+=1; /* halfword pad it */
    if (ret==0)
    {
      switch(chunk.id)
      {
        case PROP: 
                prop_flag=1;
        case LIST: 
        case FORM: 
                formtype = UTIL_Get_MSB_Long(fin);
		file_size = chunk.size;
		file_read = -1;
                DEBUG_LEVEL2
                {
                  fprintf(stderr,"  IFF ");
                  IFF_Print_ID(stderr,chunk.id);
                  fprintf(stderr," = ");
                  IFF_Print_ID(stderr,formtype);
                  fprintf(stderr,"\n");
                }
                break;
        case BMHD:
		IFF_Read_BMHD(fin,&bmhd);
                if (xa_verbose)
                {
                 fprintf(stderr,"     Size %ldx%ldx%ld comp=%ld masking=%ld\n",
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

	case FACE: /* used in MovieSetter anims */
	      {
		ULONG garb;
		bmhd.pageWidth  = bmhd.width  = UTIL_Get_MSB_Short(fin);
		bmhd.pageHeight = bmhd.height = UTIL_Get_MSB_Short(fin);
		garb = UTIL_Get_MSB_Long(fin); /* read x, y */
		garb = UTIL_Get_MSB_Long(fin); /* read xoff, yoff */
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
                 fprintf(stderr,"     Size %ldx%ldx%ld comp=%ld masking=%ld\n",
                                bmhd.width,bmhd.height,bmhd.depth,
                                bmhd.compression,bmhd.masking);
                }
	      }
              break;

        case CAMG:
                {
                  DEBUG_LEVEL2 fprintf(stderr,"IFF CAMG\n");
                  if (chunk.size != 4) 
                  {
                    ret = IFF_Read_Garb(fin,chunk.size);
                    break;
                  }

                  camg_flag = UTIL_Get_MSB_Long(fin) | IFF_CAMG_NOP;

                  if ((camg_flag & IFF_CAMG_EHB) && (cmap_flag == TRUE))  
                  {
                    IFF_Adjust_For_EHB(iff_cmap,iff_cmap_bits);
		    iff_chdr = 
			ACT_Get_CMAP(iff_cmap,iff_imagec,iff_or_mask,
				iff_imagec,iff_or_mask,
				iff_cmap_bits,iff_cmap_bits,iff_cmap_bits);
		    chdr_flag = TRUE;
                    break;
                  }
                  if (camg_flag & IFF_CAMG_LACE) iff_anim_flags |= ANIM_LACE;

                  if ((camg_flag & IFF_CAMG_HAM) && (cmap_flag == TRUE)) 
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

                    if (cmap_true_to_332 == TRUE)
                    {
                      iff_chdr = CMAP_Create_332(iff_cmap,&iff_imagec);
		      iff_or_mask = 0;
                    }
                    else /* if (cmap_true_to_gray == TRUE) */
                    {
                      iff_chdr = CMAP_Create_Gray(iff_cmap,&iff_imagec);
		      iff_or_mask = 0;
                    }
                    chdr_flag = TRUE;
                  }
                }
                break;

        case CMAP:
                {
		  ULONG tmp;
                  DEBUG_LEVEL2 fprintf(stderr,"IFF CMAP\n");

                  if (chunk.size == 0x40) 
		  {
		    iff_imagec = chunk.size / 2; /* xR+GB */
		    IFF_Read_CMAP_1(iff_cmap,iff_imagec,fin);
		  }
                  else
		  {
		    iff_imagec = chunk.size / 3;  /* Rx+Gx+Bx 1 byte each */
		    IFF_Read_CMAP_0(iff_cmap,iff_imagec,fin);
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

                    if (cmap_true_to_332 == TRUE)
                    {
                      iff_chdr = CMAP_Create_332(iff_cmap,&iff_imagec);
                      iff_or_mask = 0;
                    }
                    else /* if (cmap_true_to_gray == TRUE) */
                    {
                      iff_chdr = CMAP_Create_Gray(iff_cmap,&iff_imagec);
                      iff_or_mask = 0;
                    }
		    chdr_flag = TRUE;
                  }
		  else if (camg_flag & IFF_CAMG_EHB)
		  {
                    IFF_Adjust_For_EHB(iff_cmap,iff_cmap_bits);
                    iff_chdr =
                      ACT_Get_CMAP(iff_cmap,iff_imagec,iff_or_mask,
				iff_imagec,iff_or_mask,
				iff_cmap_bits,iff_cmap_bits,iff_cmap_bits);
		    chdr_flag = TRUE;
		  }
		  else if (camg_flag) /* NOT HAM6,HAM8 or EHB */
		  {
		    if (iff_cmap_bits == 4) 
			IFF_Shift_CMAP(iff_cmap,iff_imagec);
                    iff_chdr = ACT_Get_CMAP(iff_cmap,iff_imagec,iff_or_mask,
				iff_imagec,iff_or_mask,
				iff_cmap_bits,iff_cmap_bits,iff_cmap_bits);
		    chdr_flag = TRUE;
		  }
                  cmap_flag = TRUE;
                }
                break;

	case BODY:
	  {
	    XA_ACTION *act;
	    ACT_DLTA_HDR *dlta_hdr;

	    DEBUG_LEVEL2 fprintf(stderr,"IFF BODY\n");
	    if (chdr_flag == FALSE)
	    {
	      if (cmap_flag==TRUE)
	      {   
	        if (iff_cmap_bits == 4) IFF_Shift_CMAP(iff_cmap,iff_imagec);
	        iff_chdr = ACT_Get_CMAP(iff_cmap,iff_imagec,iff_or_mask,
					iff_imagec,iff_or_mask,iff_cmap_bits,
					iff_cmap_bits,iff_cmap_bits);
	        chdr_flag = TRUE;
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
	    if (xa_file_flag == TRUE)
	    {
	      dlta_hdr =(ACT_DLTA_HDR *)malloc(sizeof(ACT_DLTA_HDR));
	      if (dlta_hdr == 0) IFF_TheEnd1("IFF_Read_BODY: malloc err");
	      act->data = (UBYTE *)dlta_hdr;
	      dlta_hdr->flags = ACT_DBL_BUF;
	      dlta_hdr->fpos  = ftell(fin);
	      dlta_hdr->fsize = chunk.size;
	      fseek(fin,chunk.size,1);
	      if (chunk.size > iff_max_fvid_size) iff_max_fvid_size =chunk.size;
	    }
	    else
*/
	    {
	      dlta_hdr = (ACT_DLTA_HDR *) malloc( sizeof(ACT_DLTA_HDR)
						+ (iff_imagex * iff_imagey) );
	      if (dlta_hdr == 0) IFF_TheEnd1("IFF_Read_BODY: malloc err");
	      act->data = (UBYTE *)dlta_hdr;
	      dlta_hdr->flags = ACT_DBL_BUF | DLTA_DATA;
	      dlta_hdr->fpos = 0; dlta_hdr->fsize = chunk.size;
	      IFF_Read_BODY(fin,dlta_hdr->data,chunk.size,
				iff_imagex, iff_imagey, iff_imaged,
				(int)(bmhd.compression),(int)(bmhd.masking),
				iff_or_mask);
	    }
	    dlta_hdr->delta = IFF_Delta_Body;
	    dlta_hdr->xsize = iff_imagex;	dlta_hdr->ysize = iff_imagey;
	    dlta_hdr->xpos = dlta_hdr->ypos = 0;
	    dlta_hdr->special = 0;
	    dlta_hdr->extra = (void *)(0);
	  }
	  break;

        case ANHD:
		{
		  Anim_Header   anhd;
		  DEBUG_LEVEL2 fprintf(stderr,"IFF ANHD\n");
		  if (chunk.size >= Anim_Header_SIZE)
		  {
		    IFF_Read_ANHD(fin,&anhd,chunk.size);
		    iff_dlta_compression = anhd.op;
		    iff_dlta_bits = anhd.bits;
/*
		    if (xa_verbose) 
			fprintf(stderr,"ANHD time = %ld\n",anhd.reltime); 
*/
		  }
		  else 
		  {
		    IFF_Read_Garb(fin,chunk.size); 
		    iff_dlta_compression = 0xffffffff;
		    iff_dlta_bits = 0x0;
		    fprintf(stderr,"ANHD chunksize mismatch %ld\n",chunk.size);
		  }
		}
		break;

        case DLTA:
	  {
	    ACT_DLTA_HDR *dlta_hdr;
	    XA_ACTION *act;
	    DEBUG_LEVEL2 fprintf(stderr,"IFF DLTA: ");
	    act = ACT_Get_Action(anim_hdr,ACT_DELTA);
	    ACT_Add_CHDR_To_Action(act,iff_chdr);
	    act->h_cmap = iff_cur_hmap;
  	    IFF_Add_Frame(1,act);

	    if (xa_file_flag == TRUE)
	    {
	      dlta_hdr =(ACT_DLTA_HDR *)malloc(sizeof(ACT_DLTA_HDR));
	      if (dlta_hdr == 0) IFF_TheEnd1("IFF_Read_DLTA: malloc err");
	      act->data = (UBYTE *)dlta_hdr;
	      dlta_hdr->flags = ACT_DBL_BUF;
	      dlta_hdr->fpos  = ftell(fin);
	      dlta_hdr->fsize = chunk.size;
	      fseek(fin,chunk.size,1);
	      if (chunk.size > iff_max_fvid_size) iff_max_fvid_size =chunk.size;
	    }
	    else
	    {
	      dlta_hdr =(ACT_DLTA_HDR *)malloc(sizeof(ACT_DLTA_HDR)+chunk.size);
	      if (dlta_hdr == 0) IFF_TheEnd1("IFF_Read_DLTA: malloc err");
	      act->data = (UBYTE *)dlta_hdr;
	      dlta_hdr->flags = ACT_DBL_BUF | DLTA_DATA;
	      dlta_hdr->fpos = 0; dlta_hdr->fsize = chunk.size;
	      ret=fread(dlta_hdr->data,chunk.size,1,fin);
	    }

	    dlta_hdr->xsize = iff_imagex;
	    dlta_hdr->ysize = iff_imagey;
	    dlta_hdr->xpos = dlta_hdr->ypos = 0;
	    dlta_hdr->special = 0;
	    dlta_hdr->extra = (void *)(0);
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
		fprintf(stderr,"Unsupported Delta %ld\n",iff_dlta_compression);
		break;
	    }
	  }
	break;

	case ANSQ:
	  {
	    DEBUG_LEVEL2 fprintf(stderr,"IFF ANSQ\n");
	    ansq_flag = 1;  /* we found an ansq chunk */
	    IFF_Read_ANSQ(fin,chunk.size);
	  }
	  break;

        case CRNG: 
	  DEBUG_LEVEL2 fprintf(stderr,"IFF CRNG\n");
	  IFF_Read_CRNG(anim_hdr,fin,chunk.size,&crng_flag);
	  break;

	case TINY : /* ignore */
	case DPI : /* ignore */
	case IMRT: /* ignore */
	case GRAB: /* ignore */
	case DPPS: /* ignore */
	case DPPV: /* ignore */
	case DPAN: /* ignore */
	case DRNG: /* ignore */
	case VHDR: /* sound ignore should kill next body until bmhd*/
	case ANNO: /* sound ignore */
	case CHAN: /* sound ignore */
	case ANFI: /* sound ignore */
		if (chunk.size & 0x01) chunk.size++;
                ret = IFF_Read_Garb(fin,chunk.size);
                break;
    default:
	if ( feof(fin) ) exit_flag = 1;   /* end of file */
	else
	{
	  fprintf(stderr,"Unknown IFF type="); IFF_Print_ID(stderr,chunk.id);
	  if (   (file_read < file_size)	/* there should be more  */
	      && (chunk.size < (file_size - file_read) ) /*  size n 2 big */
	     )
	  {
	    fprintf(stderr,"  Will Continue Reading File.\n");
	    ret = IFF_Read_Garb(fin,chunk.size);
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
 DEBUG_LEVEL2 fprintf(stderr,"Bytes Read = %lx\n",file_read);
 fclose(fin);

 /* 
  * Set up a map of delta's to their action numbers.
  */
 {
   ULONG i;
   ULONG inter_dlta_i,dlta_i,act_i;

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
  		  fprintf(stderr,"IFF_Read: dlta setup err  <%ld > %ld> \n",
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
   DEBUG_LEVEL1 fprintf(stderr,"%ld dltas found\n",dlta_i);

   iff_time = XA_GET_TIME(IFF_SPEED_DEFAULT * MS_PER_60HZ);

   if (ansq_flag)
   {
     LONG i,t_time;
     ULONG iff_frame_cnt,frame_i;

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
         fprintf(stderr,"IFF_ansq: frame err %ld %ld\n",frame_i,iff_frame_cnt);
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
	  ULONG dlta_j;
	  iff_ansq[i].frame = frame_i; /* for looping info */
          dlta_j = iff_ansq[i].dnum + 1;
          iff_time = XA_GET_TIME(iff_ansq[i].time * MS_PER_60HZ);
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
	      fprintf(stderr,"IFF_ansq: frame err %ld %ld\n",
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
    }
    else   /* no ansq chunk */
    {
      LONG frame_i,t_time;

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
          fprintf(stderr,"IFF_Read: frame inconsistency %ld %ld\n",
                frame_i,iff_act_cnt);
          IFF_TheEnd();
        }
      }
      /* Add JMP2END so deltas to beginning aren't displayed unless looping */
      if ( !(iff_anim_flags & ANIM_NOLOOP) && (iff_dlta_cnt > 3) && !face_flag)
      { ULONG kk = frame_i;
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
      }
      else
      {
	anim_hdr->loop_frame = 0; 
      }
      if (frame_i > 0 ) anim_hdr->last_frame = frame_i - 1;
      else anim_hdr->last_frame = 0;
      anim_hdr->frame_lst[frame_i].time_dur = 0;
      anim_hdr->frame_lst[frame_i].zztime = -1;
      anim_hdr->frame_lst[frame_i].act  = 0;
      frame_i++;
      if (xa_verbose) 
      {
	fprintf(stderr,"     dlta_cnt=%ld comp=%ld ",
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
    if ( (crng_flag) && (iff_allow_cycling == TRUE) )
					iff_anim_flags |= ANIM_CYCLE;
    else iff_anim_flags &= ~ANIM_CYCLE;
  }
  else /* animation IFF files */
  {
    if ( (crng_flag) && (iff_allow_cycling == TRUE) 
        && (xa_anim_cycling == TRUE) )   iff_anim_flags |= ANIM_CYCLE;
    else iff_anim_flags &= ~ANIM_CYCLE;
  }
  anim_hdr->anim_flags = iff_anim_flags;
  IFF_Free_Stuff();
  iff_chdr = 0;
  if (xa_buffer_flag) IFF_Buffer_Action(anim_hdr);
  else
  {
    anim_hdr->anim_flags |= ANIM_SNG_BUF;
    if (iff_dlta_cnt > 1) anim_hdr->anim_flags |= ANIM_DBL_BUF;
    if (iff_anim_flags | ANIM_HAM) anim_hdr->anim_flags |= ANIM_3RD_BUF;
  }
  if (xa_file_flag==TRUE) anim_hdr->anim_flags |= ANIM_USE_FILE;
  anim_hdr->fname = anim_hdr->name;
  anim_hdr->max_fvid_size = iff_max_fvid_size;
  return(TRUE);
}

/*
 *
 */
void IFF_Adjust_For_EHB(colormap,cmap_bits)
ColorReg colormap[];
ULONG cmap_bits;
{
  LONG i,cmap_num;

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
void IFF_Read_BODY(fin,image_out,bodysize,xsize,ysize,depth,
			compression,masking,or_mask)
FILE *fin;
UBYTE *image_out;
ULONG xsize,ysize,depth;
LONG bodysize,compression,masking;
ULONG or_mask;
{
 LONG i,ret,x,y,d,dmask,tmp,rowsize;
 LONG imagex_pad;
 BYTE *inbuff,*rowbuff,*sptr;
 BYTE *sbuff,*dbuff;

 if (   (compression != BMHD_COMP_NONE) 
     && (compression != BMHD_COMP_BYTERUN) 
    ) IFF_TheEnd1("IFF_Read_Body: unsupported compression");

 if (   (masking != BMHD_MSK_NONE)
     && (masking != BMHD_MSK_HAS)
     && (masking != BMHD_MSK_TRANS)
    ) IFF_TheEnd1("IFF_Read_Body: unsupported masking");

 inbuff = (BYTE *)malloc(bodysize);
 if (inbuff == 0) IFF_TheEnd1("IFF_Read_Body: malloc failed");
 ret=fread(inbuff,bodysize,1,fin);
 if (ret!=1) IFF_TheEnd1("IFF_Read_Body: read of BODY chunk failed");
 sbuff = inbuff;


 /* width is rounded to multiples of 16 in the BODY form */
 /* extra bits are ignored upon reading */
 imagex_pad = xsize / 16;
 if (xsize % 16) imagex_pad++;
 imagex_pad *= 16;

 rowbuff = (BYTE *)malloc( imagex_pad );
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
       if (ret) { fprintf(stderr,"error %ld in unpack\n",ret); IFF_TheEnd();}
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
       if (ret) { fprintf(stderr,"error %ld in unpack\n",ret); IFF_TheEnd();}
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
LONG IFF_Read_Garb(fp,size)
FILE *fp;
LONG size;
{
 BYTE *garb;

 garb = (BYTE *)malloc(size);
 if (garb==0)
 { fprintf(stderr,"readgarb malloc err size=%ld",size); return(-1);}
 fread(garb,size,1,fp);
 FREE(garb,0x1006); garb=0;
 return(0);
}

void IFF_Print_ID(fout,id)
FILE *fout;
LONG id;
{
 fprintf(fout,"%c",     ((id >> 24) & 0xff)   );
 fprintf(fout,"%c",     ((id >> 16) & 0xff)   );
 fprintf(fout,"%c",     ((id >>  8) & 0xff)   );
 fprintf(fout,"%c(%lx)", (id        & 0xff),id);
}


/* 
 *
 */
ULONG
IFF_Delta_5(image,delta,dsize,chdr,map,map_flag,imagex,imagey,imaged,
						xs,ys,xe,ye,special,extra)
UBYTE *image;		/* Image Buffer. */
UBYTE *delta;		/* delta data. */
ULONG dsize;		/* delta size */
XA_CHDR *chdr;		/* color map info */
ULONG *map;		/* used if it's going to be remapped. */
ULONG map_flag;		/* whether or not to use remap_map info. */
ULONG imagex,imagey;	/* Size of image buffer. */
ULONG imaged;		/* Depth of Image. (IFF specific) */
ULONG *xs,*ys;		/* pos of changed area. */
ULONG *xe,*ye;		/* size of changed area. */
ULONG special;		/* Special Info. */
void *extra;		/* Extra Info. */
{
 register LONG col,depth,dmask;
 register LONG rowsize,width;
 ULONG poff;
 register UBYTE *i_ptr;
 register UBYTE *dptr,opcnt,op,cnt;
 LONG miny,minx,maxy,maxx;

 /* set to opposites for min/max testing */
 *xe = *ye = 0; *ys = imagey; *xs = imagex;

 width = imagex;
 rowsize = width >> 3;
 dmask = 1;
 for(depth=0; depth<imaged; depth++)
 {
  minx = -1;
  maxx = -1;
  
  i_ptr = image;
  /* offset into delt chunk */
  { register USHORT ddepth = depth << 2;
    poff  = (ULONG)(delta[ ddepth++ ]) << 24;
    poff |= (ULONG)(delta[ ddepth++ ]) << 16;
    poff |= (ULONG)(delta[ ddepth++ ]) <<  8;
    poff |= (ULONG)(delta[ ddepth   ]);
  }

  if (poff)
  {
   dptr = (UBYTE *)(delta + poff);
   for(col=0;col<rowsize;col++)
   {
	/* start at top of column */
    i_ptr = (UBYTE *)(image + (col << 3));
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
          if (miny == -1) miny=(ULONG)( i_ptr - image ) / width;
          cnt = op & 0x7f;         /* get cnt */
      
          while(cnt--) /* loop through data */
          {
             register UBYTE data = *dptr++;
             IFF_Byte_Mod(i_ptr,data,dmask,0);
             i_ptr += width;
          }
        } /* end unique */
        else
        {
           if (op == 0)   /* type same */
           {
              register UBYTE data;
              if (miny == -1) miny=(ULONG)( i_ptr - image ) / width;
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
     maxy = (ULONG)( i_ptr - image ) / width;
     if ( (miny>=0) && (miny < *ys)) *ys = miny;
     if ( (maxy>=0) && (maxy > *ye)) *ye = maxy;
    } /* end of column loop */
   } /* end of valid pointer for this plane */
   dmask <<= 1;
   if ( (minx>=0) && (minx < *xs)) *xs = minx;
   if ( (maxx>=0) && (maxx > *xe)) *xe = maxx;
  } /* end of for depth */

  if (xa_optimize_flag == TRUE)
  {
    if (*xs >= imagex) *xs = 0;
    if (*ys >= imagey) *ys = 0;
    if (*xe <= 0)      *xe = imagex;
    if (*ye <= 0)      *ye = imagey;
  }
  else
  {
    *xs = 0;      *ys = 0;
    *xe = imagex; *ye = imagey;
  }
  return(ACT_DLTA_NORM); 
} /* end of routine */

/*
 * 
 */
ULONG
IFF_Delta_3(image,delta,dsize,chdr,map,map_flag,imagex,imagey,imaged,
						xs,ys,xe,ye,special,extra)
UBYTE *image;		/* Image Buffer. */
UBYTE *delta;		/* delta data. */
ULONG dsize;		/* delta size */
XA_CHDR *chdr;		/* color map info */
ULONG *map;		/* used if it's going to be remapped. */
ULONG map_flag;		/* whether or not to use remap_map info. */
ULONG imagex,imagey;	/* Size of image buffer. */
ULONG imaged;		/* Depth of Image. (IFF specific) */
ULONG *xs,*ys;		/* pos of changed area. */
ULONG *xe,*ye;		/* size of changed area. */
ULONG special;		/* Special Info. */
void *extra;		/* Extra Info. */
{
 register LONG i,depth,dmask;
 ULONG poff;
 register SHORT  offset;
 register USHORT s,data;
 register UBYTE  *i_ptr,*dptr;

 *xs = *ys = 0; *xe = imagex; *ye = imagey;
 dmask = 1;
 for(depth=0;depth<imaged;depth++)
 {
  i_ptr = image;

  /*poff = planeoff[depth];*/ /* offset into delt chunk */

  poff  = (ULONG)(delta[ 4 * depth    ]) << 24;
  poff |= (ULONG)(delta[ 4 * depth + 1]) << 16;
  poff |= (ULONG)(delta[ 4 * depth + 2]) <<  8;
  poff |= (ULONG)(delta[ 4 * depth + 3]);

  if (poff)
  {
   dptr = (UBYTE *)(delta + poff);
   while( (dptr[0] != 0xff) || (dptr[1] != 0xff) )
   {
     offset = (*dptr++)<<8; offset |= (*dptr++);
     if (offset >= 0)
     {
      data = (*dptr++)<<8; data |= (*dptr++);
      i_ptr += 16 * (ULONG)(offset);
      IFF_Short_Mod(i_ptr,data,dmask,0);
     } /* end of pos */
     else
     {
      i_ptr += 16 * (ULONG)(-(offset+2));
      s = (*dptr++)<<8; s |= (*dptr++); /* size of next */
      for(i=0; i < (ULONG)s; i++)
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

/* 
 *
 */
LONG Is_IFF_File(filename)
BYTE *filename;
{
 FILE *fp;
 ULONG firstword;

 if ( (fp=fopen(filename,XA_OPEN_MODE)) == 0) return(XA_NOFILE);
  /* by reading bytes we can ignore big/little endian problems */
 firstword  = (fgetc(fp) & 0xff) << 24;
 firstword |= (fgetc(fp) & 0xff) << 16;
 firstword |= (fgetc(fp) & 0xff) <<  8;
 firstword |= (fgetc(fp) & 0xff);

 fclose(fp);

 if (firstword == FORM) return(TRUE);
 if (firstword == LIST) return(TRUE);
 if (firstword == PROP) return(TRUE);
 return(FALSE);
}

void IFF_Hash_Init(num)
ULONG num;
{ register ULONG i;
  iff_hash_cur=0;
  iff_hash_tbl = (IFF_HASH *)malloc(num * sizeof(IFF_HASH));
  if (iff_hash_tbl == 0) TheEnd1("IFF_Hash_Init: malloc err");
  for(i=0;i<num;i++) iff_hash_tbl[i].flag = ACT_DLTA_BAD;
}

void IFF_Hash_CleanUp()
{
  if (iff_hash_tbl)
  { register ULONG i=0;
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
ULONG flag;
{ 
  register IFF_HASH *hptr = &iff_hash_tbl[iff_hash_cur];
  hptr->flag = flag;	
  hptr->dlta = dlta;	hptr->nxtdlta = nxtdlta;
  hptr->src = src;	hptr->dst = *dst;
  iff_hash_cur++;
}

ULONG pod_temp_i;
XA_ACTION *IFF_Hash_Get(dlta,src,nxtdlta)
XA_ACTION *dlta,*src,*nxtdlta;
{ register ULONG i = 0;
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
  LONG image_size;
  UBYTE *buff0,*buff1,*tmp,*dbl_buff;
  XA_ACTION *act,*old_act0,*old_act1;
  XA_FRAME *frame_lst;
  ULONG frame_i,frame_num;
  ULONG scale_x,scale_y,need_to_scale;

  iff_chdr = 0;
  image_size = iff_imagex * iff_imagey;
  dbl_buff = buff1 = buff0 = (UBYTE *) malloc( 2 * image_size );
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
        {
	  ACT_DLTA_HDR *dlta_hdr = (ACT_DLTA_HDR *)act->data;
          LONG minx,miny,maxx,maxy; 
          LONG pic_x,pic_y;
          UBYTE *t_pic;
	  ULONG dlta_flag = 1;

	  dlta_flag = dlta_hdr->delta(buff0, dlta_hdr->data, 
		dlta_hdr->fsize, 0, 0, FALSE,
		iff_imagex,iff_imagey,iff_imaged,
		&minx, &miny, &maxx, &maxy, dlta_hdr->special,dlta_hdr->extra);

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
	      ULONG px,py,sx,sy;
	      ULONG opx,opy,osx,osy;
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
	    ULONG disp_flag;

	    if (x11_display_type == XA_MONOCHROME)
		{minx=miny=0; pic_x=iff_imagex; pic_y=iff_imagey; }

            t_pic = (UBYTE *) malloc( XA_PIC_SIZE(pic_x * pic_y) );
	    if (x11_display_type & XA_X11_TRUE) disp_flag = TRUE;
	    else disp_flag = FALSE;
	    if (iff_anim_flags & ANIM_HAM6)
	    {
	      if (cmap_true_map_flag == TRUE)
	      {
	        XA_CHDR *tmp_chdr = 0;
		ACT_Del_CHDR_From_Action(new_act,new_act->chdr);
		IFF_HAM6_As_True (t_pic,buff0,&tmp_chdr,act->h_cmap,
			pic_x,pic_y,minx,miny,iff_imagex);
		ACT_Add_CHDR_To_Action(new_act,tmp_chdr);
	       }
	      else
		 IFF_Buffer_HAM6(t_pic,buff0,new_act->chdr,act->h_cmap,
			pic_x,pic_y,minx,miny,iff_imagex,FALSE);
	    }
	    else
	    {
	      if (cmap_true_map_flag == TRUE)
	      {
	        XA_CHDR *tmp_chdr = 0;
		ACT_Del_CHDR_From_Action(new_act,new_act->chdr);
		IFF_HAM8_As_True (t_pic,buff0,&tmp_chdr,act->h_cmap,
			pic_x,pic_y,minx,miny,iff_imagex);
		ACT_Add_CHDR_To_Action(new_act,tmp_chdr);
	       }
	      else
		IFF_Buffer_HAM8(t_pic,buff0,new_act->chdr,act->h_cmap,
			pic_x,pic_y,minx,miny,iff_imagex,FALSE);
	    }
	    ACT_Setup_Mapped(new_act, t_pic, new_act->chdr, 
		minx, miny, pic_x, pic_y, iff_imagex, iff_imagey,
		FALSE, 0, TRUE, FALSE, disp_flag);
	  }
	  else ACT_Setup_Mapped(new_act, buff0, new_act->chdr, 
		minx, miny, pic_x, pic_y, iff_imagex, iff_imagey,
		FALSE,0, FALSE, TRUE, FALSE);
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

ULONG
IFF_Delta_J(image,delta,dsize,chdr,map,map_flag,imagex,imagey,imaged,
						xs,ys,xe,ye,special,extra)
UBYTE *image;		/* Image Buffer. */
UBYTE *delta;		/* delta data. */
ULONG dsize;		/* delta size */
XA_CHDR *chdr;		/* color map info */
ULONG *map;		/* used if it's going to be remapped. */
ULONG map_flag;		/* whether or not to use remap_map info. */
ULONG imagex,imagey;	/* Size of image buffer. */
ULONG imaged;		/* Depth of Image. (IFF specific) */
ULONG *xs,*ys;		/* pos of changed area. */
ULONG *xe,*ye;		/* size of changed area. */
ULONG special;		/* Special Info. */
void *extra;		/* Extra Info. */
{
 register LONG rowsize,width;
 register UBYTE *i_ptr;
 register LONG exitflag;
 register ULONG  type,r_flag,b_cnt,g_cnt,r_cnt; 
 register ULONG b,g,r;
 register ULONG offset,dmask,depth;
 register UBYTE data;
 LONG changed,xor_flag;
 LONG tmp,minx,miny,maxx,maxy;
 LONG kludge_j;
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
	ULONG odd_flag;
        offset = (*delta++) << 8; offset |= (*delta++);

        offset <<= 3; /* counts bytes */
        if (kludge_j) 
             offset = ((offset/320) * imagex) + (offset%320) - kludge_j;

        i_ptr = (UBYTE *)(image + offset);

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
      { ULONG odd_flag;
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
            i_ptr = (UBYTE *)(image + offset + (r * width));
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
 if (changed==0)
 {
   *xs = *ys = *xe = *ye = 0;
   DEBUG_LEVEL2 fprintf(stderr,"DELTA J nothing changed\n");
   return(ACT_DLTA_NOP);
 }
 if (xa_optimize_flag == TRUE)
 {
   *xs = minx; *ys = miny;
   *xe = maxx; *ye = maxy;

   if (*xs >= imagex) *xs = 0;
   if (*ys >= imagey) *ys = 0;
   if (*xe <= 0)      *xe = imagex;
   if (*ye <= 0)      *ye = imagey;
   DEBUG_LEVEL2 fprintf(stderr,"xypos=<%ld,%ld> xysize=<%ld %ld>\n",
		*xs,*ys,*xe,*ye );
 }
 else
 {
   *xs = 0; *ys = 0;
   *xe = imagex; *ye = imagey;
 }
 if (xor_flag) return(ACT_DLTA_XOR);
 return(ACT_DLTA_NORM);
} /* end of IFF_DeltaJ routine */

/* 
 *  Decode IFF type l anims
 */
ULONG
IFF_Delta_l(image,delta,dsize,chdr,map,map_flag,imagex,imagey,imaged,
						xs,ys,xe,ye,special,vertflag)
UBYTE *image;		/* Image Buffer. */
UBYTE *delta;		/* delta data. */
ULONG dsize;		/* delta size */
XA_CHDR *chdr;		/* color map info */
ULONG *map;		/* used if it's going to be remapped. */
ULONG map_flag;		/* whether or not to use remap_map info. */
ULONG imagex,imagey;	/* Size of image buffer. */
ULONG imaged;		/* Depth of Image. (IFF specific) */
ULONG *xs,*ys;		/* pos of changed area. */
ULONG *xe,*ye;		/* size of changed area. */
ULONG special;		/* Special Info. */
ULONG vertflag;		/* Extra Info. 1 = vertical encoding*/
{
 register LONG i,depth,dmask,width;
 ULONG poff0,poff1;
 register UBYTE *i_ptr;
 register UBYTE *optr,*dptr;
 register SHORT cnt;
 register USHORT offset,data;

 *xs = *ys = 0; *xe = imagex; *ye = imagey;
 i_ptr = image;
 if (vertflag) width = imagex;
 else width = 16;
 dmask = 1;
 for(depth = 0; depth<imaged; depth++)
 {
   i_ptr = image;
   /*poff = planeoff[depth];*/ /* offset into delt chunk */
   poff0  = (ULONG)(delta[ 4 * depth    ]) << 24;
   poff0 |= (ULONG)(delta[ 4 * depth + 1]) << 16;
   poff0 |= (ULONG)(delta[ 4 * depth + 2]) <<  8;
   poff0 |= (ULONG)(delta[ 4 * depth + 3]);

   if (poff0)
   {
     poff1  = (ULONG)(delta[ 4 * (depth+8)    ]) << 24;
     poff1 |= (ULONG)(delta[ 4 * (depth+8) + 1]) << 16;
     poff1 |= (ULONG)(delta[ 4 * (depth+8) + 2]) <<  8;
     poff1 |= (ULONG)(delta[ 4 * (depth+8) + 3]);

     dptr = (UBYTE *)(delta + 2 * poff0); 
     optr = (UBYTE *)(delta + 2 * poff1); 

     /* while short *optr != -1 */
     while( (optr[0] != 0xff) || (optr[1] != 0xff) )
     {
       offset = (*optr++) << 8; offset |= (*optr++);
       cnt    = (*optr++) << 8; cnt    |= (*optr++);
 
       if (cnt < 0)  /* cnt negative */
       {
         i_ptr = image + 16 * (ULONG)(offset);
         cnt = -cnt;
         data = (*dptr++) << 8; data |= (*dptr++);
         for(i=0; i < (ULONG)cnt; i++)
         {
           IFF_Short_Mod(i_ptr,data,dmask,0);
           i_ptr += width;
         }
       }  /* end of neg */
       else/* cnt pos then */
       {
         i_ptr = image + 16 * (ULONG)(offset);
         for(i=0; i < (ULONG)cnt; i++)
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
ULONG bits;
{
  ColorReg *hptr;
  ULONG i,size,shift;

  if (bits == 8) {size = XA_HMAP8_SIZE; shift = 2;}
  else {size = XA_HMAP6_SIZE; shift = 4;}
  
  act->data = (UBYTE *) malloc(size * sizeof(ColorReg) );
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
IFF_Init_DLTA_HDR(max_x,max_y)
ULONG max_x,max_y;
{
  iff_dlta[0].minx = iff_dlta[1].minx = 0;
  iff_dlta[0].miny = iff_dlta[1].miny = 0;
  iff_dlta[0].maxx = iff_dlta[1].maxx = max_x;
  iff_dlta[0].maxy = iff_dlta[1].maxy = max_y;
}

void
IFF_Update_DLTA_HDR(min_x,min_y,max_x,max_y)
LONG *min_x,*min_y,*max_x,*max_y;
{
  register LONG tmin_x,tmin_y,tmax_x,tmax_y;

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
    iff_dlta[0].minx = XA_MIN(iff_dlta[0].minx, tmin_x);
    iff_dlta[0].miny = XA_MIN(iff_dlta[0].miny, tmin_y);
    iff_dlta[0].maxx = XA_MAX(iff_dlta[0].maxx, tmax_x);
    iff_dlta[0].maxy = XA_MAX(iff_dlta[0].maxy, tmax_y);
    *min_x = XA_MIN(iff_dlta[1].minx, iff_dlta[0].minx);
    *min_y = XA_MIN(iff_dlta[1].miny, iff_dlta[0].miny);
    *max_x = XA_MAX(iff_dlta[1].maxx, iff_dlta[0].maxx);
    *max_y = XA_MAX(iff_dlta[1].maxy, iff_dlta[0].maxy);
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
    *min_x = XA_MIN(iff_dlta[1].minx, iff_dlta[0].minx);
    *min_y = XA_MIN(iff_dlta[1].miny, iff_dlta[0].miny);
    *max_x = XA_MAX(iff_dlta[1].maxx, iff_dlta[0].maxx);
    *max_y = XA_MAX(iff_dlta[1].maxy, iff_dlta[0].maxy);
  }
}

void
IFF_Read_BMHD(fin,bmhd)
FILE *fin;
Bit_Map_Header *bmhd;
{
  /* read Bit_Map_Header into bmhd */
  /* read so as to avoid endian problems */
  bmhd->width              = UTIL_Get_MSB_Short(fin);
  bmhd->height             = UTIL_Get_MSB_Short(fin);
  bmhd->x                  = UTIL_Get_MSB_Short(fin);
  bmhd->y                  = UTIL_Get_MSB_Short(fin);
  bmhd->depth              = fgetc(fin);
  bmhd->masking            = fgetc(fin);
  bmhd->compression        = fgetc(fin);
  bmhd->pad1               = fgetc(fin);
  bmhd->transparentColor   = UTIL_Get_MSB_Short(fin);
  bmhd->xAspect            = fgetc(fin);
  bmhd->yAspect            = fgetc(fin);
  bmhd->pageWidth          = UTIL_Get_MSB_Short(fin);
  bmhd->pageHeight         = UTIL_Get_MSB_Short(fin);
} 

void
IFF_Read_ANHD(fin,anhd,chunk_size)
FILE *fin;
Anim_Header *anhd;
ULONG chunk_size;
{
  ULONG i;
  anhd->op		= fgetc(fin);
  anhd->mask		= fgetc(fin);
  anhd->w		= UTIL_Get_MSB_Short(fin);
  anhd->h		= UTIL_Get_MSB_Short(fin);
  anhd->x		= UTIL_Get_MSB_Short(fin);
  anhd->y		= UTIL_Get_MSB_Short(fin);
  anhd->abstime		= UTIL_Get_MSB_Long(fin);
  anhd->reltime		= UTIL_Get_MSB_Long(fin);
  anhd->interleave	= fgetc(fin);
  anhd->pad0		= fgetc(fin);
  anhd->bits		= UTIL_Get_MSB_Long(fin);
  fread((BYTE *)(anhd->pad),1,16,fin); /* read pad */
  i = Anim_Header_SIZE;
  while(i < chunk_size) {fgetc(fin); i++;}
}

void
IFF_Read_ANSQ(fin,chunk_size)
FILE *fin;
ULONG chunk_size;
{
  ULONG i;
  UBYTE *p;  /* data is actually big endian USHORT */
  BYTE *garb;

  iff_ansq_cnt = chunk_size / 4;
  iff_ansq_cnt++; /* adding dlta 0 up front */
  DEBUG_LEVEL2 fprintf(stderr,"    ansq_cnt=%ld dlta_cnt=%ld\n",
						iff_ansq_cnt,iff_dlta_cnt);
  /* allocate space for ansq variables
  */
  iff_ansq = (IFF_ANSQ *)malloc( iff_ansq_cnt * sizeof(IFF_ANSQ));
  if (iff_ansq == NULL) IFF_TheEnd1("IFF_Read_ANSQ: malloc err");
  if (xa_verbose) fprintf(stderr,"     frames=%ld dlts=%d comp=%ld\n",
			iff_ansq_cnt,iff_dlta_cnt,iff_dlta_compression);
  garb = (BYTE *)malloc(chunk_size);
  if (garb==0)
  {
    fprintf(stderr,"ansq malloc not enough\n");
    IFF_TheEnd();
  }
  fread(garb,chunk_size,1,fin);
  p = (UBYTE *)(garb);
  /* first delta is only used once and doesn't appear in
  * the ANSQ
  */
  iff_ansq[0].dnum  = 0;
  iff_ansq[0].time  = 1;
  for(i=1; i<iff_ansq_cnt; i++)
  {
    /* this is delta to apply */
    iff_ansq[i].dnum  = (ULONG)(*p++)<<8;
    iff_ansq[i].dnum |= (ULONG)(*p++);
    /* this is jiffy count or if 0xffff then a goto */
    iff_ansq[i].time  = (ULONG)(*p++)<<8;
    iff_ansq[i].time |= (ULONG)(*p++);
    iff_ansq[i].frame = 0;
    DEBUG_LEVEL2
    fprintf(stderr,"<%ld %ld> ",iff_ansq[i].dnum, iff_ansq[i].time);
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
IFF_Read_CRNG(anim_hdr,fin,chunk_size,crng_flag)
XA_ANIM_HDR *anim_hdr;
FILE *fin;
ULONG chunk_size;
ULONG *crng_flag;
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
    ULONG rate,active,low,high,csize;
    ACT_CYCLE_HDR *act_cycle;

    /* read CRNG chunk */
    rate   = UTIL_Get_MSB_UShort(fin);  /* throw away pad1 */
    rate   = UTIL_Get_MSB_UShort(fin);
    active = UTIL_Get_MSB_UShort(fin);
    low    = fgetc(fin);
    high   = fgetc(fin);
    /* make it an action only if its valid
    */
    if (   (active & IFF_CRNG_ACTIVE) && (low < high) 
	&& (rate > IFF_CRNG_DPII_KLUDGE) && (iff_allow_cycling == TRUE) )
    {
      ULONG i,*i_ptr;

      csize = high - low + 1;
      act_cycle = (ACT_CYCLE_HDR *)
	malloc( sizeof(ACT_CYCLE_HDR) + (csize * sizeof(ULONG)) );
      if (act_cycle == 0) IFF_TheEnd1("IFF_Read_CRNG: malloc failed");

      act_cycle->size = csize;
      act_cycle->curpos = 0;
      act_cycle->rate  = (ULONG)(IFF_CRNG_INTERVAL/rate);
      act_cycle->flags = ACT_CYCLE_ACTIVE;
      if (active & IFF_CRNG_REVERSE) act_cycle->flags |= ACT_CYCLE_REVERSE;

      i_ptr = (ULONG *)act_cycle->data;
      for(i=0; i<csize; i++) 
      {
        i_ptr[i] = low + i + iff_or_mask;
      }

      *crng_flag = *crng_flag + 1;
      act = ACT_Get_Action(anim_hdr,ACT_CYCLE);
      IFF_Add_Frame(0,act);
      act->data = (UBYTE *) act_cycle;
	/* register it now if iff_chdr valid, else wait to later */
      if (iff_chdr) ACT_Add_CHDR_To_Action(act,iff_chdr);
    }
    else DEBUG_LEVEL2 fprintf(stderr,"IFF_CRNG not used\n");
  }
  else
  {
    IFF_Read_Garb(fin,chunk_size);
    fprintf(stderr,"IFF_CRNG chunksize mismatch %ld\n",chunk_size);
  }
}


void
IFF_Read_CMAP_0(cmap,size,fin)
ColorReg *cmap;
ULONG size;
FILE *fin;
{
  ULONG i;
  for(i=0; i < size; i++)
  {
    cmap[i].red   = fgetc(fin);
    cmap[i].green = fgetc(fin);
    cmap[i].blue  = fgetc(fin);
  }
}

void
IFF_Read_CMAP_1(cmap,size,fin)
ColorReg *cmap;
ULONG size;
FILE *fin;
{
  ULONG i;
  for(i=0; i < size; i++)
  {
    ULONG d;
    d = fgetc(fin);
    cmap[i].red   = (d & 0x0f) << 4;
    d = fgetc(fin);
    cmap[i].green = (d & 0xf0);
    cmap[i].blue  = (d & 0x0f) << 4;
  }
}


void IFF_Buffer_HAM6(out,in,chdr,h_cmap,xosize,yosize,xip,yip,xisize,d_flag)
UBYTE *out;		/* output image (size of section) */
UBYTE *in;		/* input image */
XA_CHDR *chdr;		/* color header to map to */
ColorReg *h_cmap;	/* ham color map */
ULONG xosize,yosize;	/* size of section in input buffer */
ULONG xip,yip;		/* pos of section in input buffer */
ULONG xisize;		/* x size of input buffer */
ULONG d_flag;		/* map_flag */
{
  XA_CHDR *the_chdr;
  ULONG new_cmap_flag,*the_map,psize;
  register ULONG y,xend,the_moff,coff;
  USHORT g_adj[16];

  if (x11_display_type & XA_X11_TRUE) 
	for(y=0;y<16;y++) g_adj[y] = xa_gamma_adj[ (17 * y) ];

  the_map = chdr->map;
  coff = chdr->coff;
  if (chdr->new_chdr == 0) { the_chdr = chdr; new_cmap_flag = 0; }
  else { the_chdr = chdr->new_chdr; new_cmap_flag = 1; }
  the_moff = the_chdr->moff;
  if (x11_display_type & XA_X11_TRUE) d_flag = TRUE;
  if (d_flag==TRUE) psize = x11_bytes_pixel;
  else psize = 1;

  DEBUG_LEVEL1 fprintf(stderr,"ham_cmap: = %lx\n",(ULONG)h_cmap);

  if (xa_ham_map == 0)
  {
     xa_ham_map_size = XA_HAM6_CACHE_SIZE;
     xa_ham_map = (ULONG *)malloc( xa_ham_map_size * sizeof(ULONG) );
     if (xa_ham_map == 0) IFF_TheEnd1("IFF_Buffer_HAM6: h_map malloc err");
  }
  if ((the_chdr != xa_ham_chdr) || (xa_ham_init != 6))
  {
    register ULONG i;
    DEBUG_LEVEL1 fprintf(stderr,"xa_ham_map: old = %lx new = %lx\n",
					(ULONG)xa_ham_chdr,(ULONG)the_chdr);
    for(i=0; i<XA_HAM6_CACHE_SIZE; i++) xa_ham_map[i] = XA_HAM_MAP_INVALID;
    xa_ham_chdr = the_chdr; xa_ham_init = 6;
  }

  xend = xip + xosize; if (xend > xisize) xend = xisize;
  for (y=yip; y < (yip + yosize); y++)
  {
    register ULONG x;
    register UBYTE *i_ptr = (UBYTE *)( in + y * xisize );
    register UBYTE *o_ptr = (UBYTE *)( out + (y-yip)*xosize * psize );
    register ULONG pred,pgrn,pblu,data;
    pred = pgrn = pblu = 0;
    for (x=0; x<xend; x++)
    {
      data = (USHORT )(*i_ptr++);
      switch(data & 0x30)
      {
        case 0x00: /* use color register given by low */
          { register USHORT low = data & 0x0f;
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
        register ULONG t_color;
        register USHORT indx = (pred << 8) | (pgrn << 4) | pblu;

        if ( (t_color = xa_ham_map[indx]) == XA_HAM_MAP_INVALID) 
        {
          if (x11_display_type & XA_X11_TRUE) t_color = 
		X11_Get_True_Color( g_adj[pred],g_adj[pgrn],g_adj[pblu],16);
	  else /* don't gamma because chdr already adjusted */
	  {
            if (cmap_true_to_332 == TRUE)
		 t_color = CMAP_GET_332(pred,pgrn,pblu,CMAP_SCALE4);
	    else t_color = CMAP_GET_GRAY(pred,pgrn,pblu,CMAP_SCALE9);
            if (new_cmap_flag) t_color = the_map[t_color - coff] + the_moff;
	  }
          xa_ham_map[indx] = t_color;
	}

	if (d_flag)
	{
          if (x11_bytes_pixel == 4)
             { ULONG *ulp = (ULONG *)o_ptr; *ulp = t_color; o_ptr += 4; }
          else if (x11_bytes_pixel == 2) { USHORT *usp = (USHORT *)o_ptr;
			 *usp = (USHORT)(t_color); o_ptr += 2; }
          else *o_ptr++ = (UBYTE)t_color;
	} else *o_ptr++ = (UBYTE)t_color;
      } /* end of output */
    } /* end of x */
  } /* end of y */
}


void IFF_Buffer_HAM8(out,in,chdr,h_cmap,xosize,yosize,xip,yip,xisize,d_flag)
UBYTE *out;		/* output image (size of section) */
UBYTE *in;		/* input image */
XA_CHDR *chdr;		/* color header to map to */
ColorReg *h_cmap;	/* ham color map */
ULONG xosize,yosize;	/* size of section in input buffer */
ULONG xip,yip;		/* pos of section in input buffer */
ULONG xisize;		/* x size of input buffer */
ULONG d_flag;		/* map_flag */
{
  XA_CHDR *the_chdr;
  ULONG new_cmap_flag,*the_map,psize;
  register ULONG y,xend,the_moff,coff;
  USHORT g_adj[64];

  if (x11_display_type & XA_X11_TRUE) 
	for(y=0;y<64;y++) g_adj[y] = xa_gamma_adj[ ((65 * y) >> 4) ];
  the_map = chdr->map;
  coff = chdr->coff;
  if (chdr->new_chdr == 0) { the_chdr = chdr; new_cmap_flag = 0; }
  else { the_chdr = chdr->new_chdr; new_cmap_flag = 1; }
  the_moff = chdr->moff;
  if (x11_display_type & XA_X11_TRUE) d_flag = TRUE;
  if (d_flag==TRUE) psize = x11_bytes_pixel;
  else psize = 1;

  if (xa_ham_map_size != XA_HAM8_CACHE_SIZE)
  {
    if (xa_ham_map == 0)
    {
         xa_ham_map_size = XA_HAM8_CACHE_SIZE;
         xa_ham_map = (ULONG *)malloc( xa_ham_map_size * sizeof(ULONG) );
         if (xa_ham_map == 0) IFF_TheEnd1("IFF_Buffer_HAM8: h_map malloc err");
    }
    else
    {
      ULONG *tmp;
      xa_ham_map_size = XA_HAM8_CACHE_SIZE;
      tmp = (ULONG *) realloc(xa_ham_map,xa_ham_map_size * sizeof(ULONG));
      if (tmp == 0) IFF_TheEnd1("IFF_Buffer_HAM8: h_map malloc err");
      xa_ham_map = tmp;
    }
    xa_ham_chdr = 0;
  }
  if ( (the_chdr != xa_ham_chdr) || (xa_ham_init != 8))
  {
    register ULONG i;
    DEBUG_LEVEL1 fprintf(stderr,"xa_ham8_map: old = %lx new = %lx\n",
					(ULONG)xa_ham_chdr,(ULONG)the_chdr);
    for(i=0; i<XA_HAM8_CACHE_SIZE; i++) xa_ham_map[i] = XA_HAM_MAP_INVALID;
    xa_ham_chdr = the_chdr; xa_ham_init = 8;
  }

  xend = xip + xosize; if (xend > xisize) xend = xisize;
  for (y=yip; y < (yip + yosize); y++)
  {
    register ULONG x;
    register UBYTE *i_ptr = (UBYTE *)( in + y * xisize );
    register UBYTE *o_ptr = (UBYTE *)( out + (y-yip) *xosize * psize);
    register ULONG pred,pgrn,pblu,data;

    pred = pgrn = pblu = 0;
    for (x=0; x<xend; x++)
    {
      data = (ULONG )(*i_ptr++);
      switch(data & 0xc0)
      {
        case 0x00: /* use color register given by low */
          { register ULONG low = data & 0x3f;
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
        register ULONG t_color;
        register ULONG indx = (pred << 12) | (pgrn << 6) | pblu;

        if ( (t_color = xa_ham_map[indx]) == XA_HAM_MAP_INVALID) 
        {
          if (x11_display_type & XA_X11_TRUE) t_color = 
		X11_Get_True_Color( g_adj[pred],g_adj[pgrn],g_adj[pblu],16);
	  else /* no gamma here because it's already in cmap */
	  {
            if (cmap_true_to_332 == TRUE)
		 t_color = CMAP_GET_332(pred,pgrn,pblu,CMAP_SCALE6);
	    else t_color = CMAP_GET_GRAY(pred,pgrn,pblu,CMAP_SCALE11);
            if (new_cmap_flag) t_color = the_map[t_color - coff] + the_moff;
	  }
          xa_ham_map[indx] = t_color;
	}
	if (d_flag)
	{
          if (x11_bytes_pixel == 4)
             { ULONG *ulp = (ULONG *)o_ptr; *ulp = t_color; o_ptr += 4; }
          else if (x11_bytes_pixel == 2) { USHORT *usp = (USHORT *)o_ptr;
                         *usp = (USHORT)(t_color); o_ptr += 2; }
          else *o_ptr++ = (UBYTE)t_color;
	} else *o_ptr++ = (UBYTE)t_color;
      } /* end of output */
    } /* end of x */
  } /* end of y */
}

void
IFF_Shift_CMAP(cmap,csize)
ColorReg *cmap;
ULONG csize;
{
  ULONG i;
  for(i=0;i<csize;i++)
  {
    cmap[i].red   >>= 4;
    cmap[i].green >>= 4;
    cmap[i].blue  >>= 4;
  }
}

ULONG
IFF_Delta_7(image,delta,dsize,chdr,map,map_flag,imagex,imagey,imaged,
						xs,ys,xe,ye,special,extra)
UBYTE *image;		/* Image Buffer. */
UBYTE *delta;		/* delta data. */
ULONG dsize;		/* delta size */
XA_CHDR *chdr;		/* color map info */
ULONG *map;		/* used if it's going to be remapped. */
ULONG map_flag;		/* whether or not to use remap_map info. */
ULONG imagex,imagey;	/* Size of image buffer. */
ULONG imaged;		/* Depth of Image. (IFF specific) */
ULONG *xs,*ys;		/* pos of changed area. */
ULONG *xe,*ye;		/* size of changed area. */
ULONG special;		/* Special Info. */
void *extra;		/* Extra Info. 0 = short encoding, 1 = long encoding*/
{
 register LONG col,depth,dmask;
 register LONG rowsize,width;
 ULONG poff,doff,col_shift;
 register UBYTE *i_ptr;
 register UBYTE *optr,opcnt,op,cnt;
 UBYTE *d_ptr;
 LONG miny,minx,maxy,maxx;
 ULONG iextra = (ULONG)(extra);

 /* set to opposites for min/max testing */
 *xs = imagex;
 *ys = imagey;
 *xe = 0;
 *ye = 0;

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
  { register ULONG ddepth = depth << 2;
    poff  = (ULONG)(delta[ ddepth++ ]) << 24;
    poff |= (ULONG)(delta[ ddepth++ ]) << 16;
    poff |= (ULONG)(delta[ ddepth++ ]) <<  8;
    poff |= (ULONG)(delta[ ddepth   ]);
    ddepth +=29; /* move to data */
    doff  = (ULONG)(delta[ ddepth++ ]) << 24;
    doff |= (ULONG)(delta[ ddepth++ ]) << 16;
    doff |= (ULONG)(delta[ ddepth++ ]) <<  8;
    doff |= (ULONG)(delta[ ddepth   ]);
  }
  if (poff)
  {
   optr   = (UBYTE  *)(delta + poff);
   d_ptr =  (UBYTE *)(delta + doff);
   
   for(col=0;col<rowsize;col++)
   {
	/* start at top of column */
    i_ptr = (UBYTE *)(image + (col << col_shift));
    opcnt = *optr++;  /* get number of ops for this column */
    
    miny = -1;
    maxy = -1;

    while(opcnt)    /* execute ops */
    {
      /* keep track of min and max columns */
      if (minx == -1) minx = col;
      maxx = col;

      op = *optr++;   /* get op */
     
      if (iextra) /* SHORT Data */
      {
        if (op & 0x80)    /* if type uniqe */
        {
          if (miny == -1) miny = (ULONG)(i_ptr - image ) / width;
          cnt = op & 0x7f;         /* get cnt */
      
          while(cnt--) /* loop through data */
          {
             register ULONG data;
	     data = (*d_ptr++) << 8; data |= *d_ptr++;
             IFF_Short_Mod(i_ptr,data,dmask,0);
             i_ptr += width;
          }
        } /* end unique */
        else
        {
          if (op == 0)   /* type same */
          {
             register USHORT data;
             if (miny == -1) miny=(ULONG)(i_ptr - image) / width;
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
      else /* LONG Data */
      {
        if (op & 0x80)    /* if type uniqe */
        {
          if (miny == -1) miny=(ULONG)( i_ptr - image ) / width;
          cnt = op & 0x7f;         /* get cnt */
      
          while(cnt--) /* loop through data */
          { register ULONG data;
	    data = (*d_ptr++)<<24; data |= (*d_ptr++)<<16;
	    data |= (*d_ptr++)<<8; data |= *d_ptr++;
            IFF_Long_Mod(i_ptr,data,dmask,0); i_ptr += width;
          }
        } /* end unique */
        else
        {
           if (op == 0) /* type same */
           { register ULONG data;
             if (miny == -1) miny=(ULONG)( i_ptr - image ) / width;
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
     maxy = (ULONG)( i_ptr - image ) / width;
     if ( (miny>=0) && (miny < *ys)) *ys = miny;
     if ( (maxy>=0) && (maxy > *ye)) *ye = maxy;
    } /* end of column loop */
   } /* end of valid pointer for this plane */
   dmask <<= 1;
   minx <<= col_shift; maxx <<= col_shift;
   maxx = (iextra)?(maxx+15):(maxx+31);
   if ( (minx>=0) && (minx < *xs)) *xs = minx;
   if ( (maxx>=0) && (maxx > *xe)) *xe = maxx;
  } /* end of for depth */

  if (xa_optimize_flag == TRUE)
  {
    if (*xs >= imagex) *xs = 0;
    if (*ys >= imagey) *ys = 0;
    if (*xe <= 0)      *xe = imagex;
    if (*ye <= 0)      *ye = imagey;
  }
  else
  {
    *xs = 0;      *ys = 0;
    *xe = imagex; *ye = imagey;
  }
  return(ACT_DLTA_NORM);
} /* end of routine */



ULONG
IFF_Delta_8(image,delta,dsize,chdr,map,map_flag,imagex,imagey,imaged,
						xs,ys,xe,ye,special,extra)
UBYTE *image;		/* Image Buffer. */
UBYTE *delta;		/* delta data. */
ULONG dsize;		/* delta size */
XA_CHDR *chdr;		/* color map info */
ULONG *map;		/* used if it's going to be remapped. */
ULONG map_flag;		/* whether or not to use remap_map info. */
ULONG imagex,imagey;	/* Size of image buffer. */
ULONG imaged;		/* Depth of Image. (IFF specific) */
ULONG *xs,*ys;		/* pos of changed area. */
ULONG *xe,*ye;		/* size of changed area. */
ULONG special;		/* Special Info. */
void *extra;		/* Extra Info. 0 = short encoding, 1 = long encoding*/
{
 register LONG col,depth,dmask;
 register LONG rowsize,width;
 ULONG poff,col_shift;
 register UBYTE *i_ptr;
 register UBYTE *optr;
 register ULONG opcnt,op,cnt;
 LONG miny,minx,maxy,maxx;
 ULONG iextra = (ULONG)(extra);

 /* set to opposites for min/max testing */
 *xs = imagex;
 *ys = imagey;
 *xe = 0;
 *ye = 0;

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
  { register ULONG ddepth = depth << 2;
    poff  = (ULONG)(delta[ ddepth++ ]) << 24;
    poff |= (ULONG)(delta[ ddepth++ ]) << 16;
    poff |= (ULONG)(delta[ ddepth++ ]) <<  8;
    poff |= (ULONG)(delta[ ddepth   ]);
  }
  if (poff)
  {
   optr   = (UBYTE  *)(delta + poff);
   
   for(col=0;col<rowsize;col++)
   {
     /* start at top of column */
     i_ptr = (UBYTE *)(image + (col << col_shift));
     if (iextra) /* SHORT Data */
     {
       opcnt  = (ULONG)(*optr++) << 8;  /* get number of ops for this column */
       opcnt |= (ULONG)(*optr++);
    
       miny = -1;
       maxy = -1;

       while(opcnt)    /* execute ops */
       {
	 /* keep track of min and max columns */
	 if (minx == -1) minx = col;
	 maxx = col;

	 op  = (ULONG)(*optr++) << 8;   /* get op */
	 op |= (ULONG)(*optr++);
     
	 if (op & 0x8000)    /* if type uniqe */
	 {
	   if (miny == -1) miny = (ULONG)(i_ptr - image ) / width;
	   cnt = op & 0x7fff;         /* get cnt */
      
	   while(cnt--) /* loop through data */
	   {
             register ULONG data;
	     data = (*optr++) << 8; data |= *optr++;
             IFF_Short_Mod(i_ptr,data,dmask,0);
             i_ptr += width;
	   }
	 } /* end unique */
	 else
	 {
	   if (op == 0)   /* type same */
	   {
             register USHORT data;
             if (miny == -1) miny=(ULONG)(i_ptr - image) / width;
	     cnt  = (ULONG)(*optr++) << 8;
	     cnt |= (ULONG)(*optr++);
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
     else /* LONG Data */
     {
       opcnt  = (ULONG)(*optr++) << 24; /* get number of ops for this column */
       opcnt |= (ULONG)(*optr++) << 16;
       opcnt |= (ULONG)(*optr++) <<  8;
       opcnt |= (ULONG)(*optr++);
    
       miny = -1;
       maxy = -1;

       while(opcnt)    /* execute ops */
       {
	 /* keep track of min and max columns */
	 if (minx == -1) minx = col;
	 maxx = col;

	 op  = (ULONG)(*optr++) << 24;   /* get op */
	 op |= (ULONG)(*optr++) << 16;
	 op |= (ULONG)(*optr++) <<  8;
	 op |= (ULONG)(*optr++);

	 if (op & 0x80000000)    /* if type uniqe */
	 {
	   if (miny == -1) miny=(ULONG)( i_ptr - image ) / width;
	   cnt = op & 0x7fffffff;         /* get cnt */
      
	   while(cnt--) /* loop through data */
	   { register ULONG data;
	     data = (*optr++)<<24; data |= (*optr++)<<16;
	     data |= (*optr++)<<8; data |= *optr++;
	     IFF_Long_Mod(i_ptr,data,dmask,0); i_ptr += width;
	   }
	 } /* end unique */
	 else
	 {
           if (op == 0) /* type same */
	   { register ULONG data;
             if (miny == -1) miny=(ULONG)( i_ptr - image ) / width;
	     cnt  = (ULONG)(*optr++) << 24;
	     cnt |= (ULONG)(*optr++) << 16;
	     cnt |= (ULONG)(*optr++) <<  8;
	     cnt |= (ULONG)(*optr++);
	     data = (*optr++)<<24; data |= (*optr++)<<16;
	     data |= (*optr++)<<8; data |= *optr++;

             while(cnt--) {IFF_Long_Mod(i_ptr,data,dmask,0); i_ptr += width;}
           } /* end same */
           else { i_ptr += (width * op);  /* type skip */ }
         } /* end of hi bit clear */
	 opcnt--;
       } /* end of while opcnt */
     } /* end of long data */
     maxy = (ULONG)( i_ptr - image ) / width;
     if ( (miny>=0) && (miny < *ys)) *ys = miny;
     if ( (maxy>=0) && (maxy > *ye)) *ye = maxy;
    } /* end of column loop */
   } /* end of valid pointer for this plane */
   dmask <<= 1;
   minx <<= col_shift; maxx <<= col_shift;
   maxx = (iextra)?(maxx+15):(maxx+31);
   if ( (minx>=0) && (minx < *xs)) *xs = minx;
   if ( (maxx>=0) && (maxx > *xe)) *xe = maxx;
  } /* end of for depth */

  if (xa_optimize_flag == TRUE)
  {
    if (*xs >= imagex) *xs = 0;
    if (*ys >= imagey) *ys = 0;
    if (*xe <= 0)      *xe = imagex;
    if (*ye <= 0)      *ye = imagey;
  }
  else
  {
    *xs = 0;      *ys = 0;
    *xe = imagex; *ye = imagey;
  }
  return(ACT_DLTA_NORM);
} /* end of routine */


/* POD NOTE: no need to support xorflag yet */
void IFF_Long_Mod(ptr,data,dmask,xorflag) 
UBYTE *ptr;
ULONG data,dmask,xorflag;
{ register UBYTE *_iptr = ptr; 
  register UBYTE dmaskoff = ~dmask;
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
UBYTE *out,*in;
XA_CHDR **chdr;
ColorReg *h_cmap;
ULONG xosize,yosize,xip,yip,xisize;
{
  ULONG xend,y;
  UBYTE *tpic,*optr;
  USHORT g_adj[16];

  for(y=0;y<16;y++) g_adj[y] = xa_gamma_adj[ (17 * y) ] >> 8;
  tpic = (UBYTE *)malloc( 3 * xosize * yosize);
  if (tpic == 0) TheEnd1("IFF_HAM6_As_True: malloc err\n");
  optr = tpic;
  xend = xip + xosize; if (xend > xisize) xend = xisize;
  for (y=yip; y < (yip + yosize); y++)
  {
    register ULONG x;
    register UBYTE *i_ptr = (UBYTE *)( in + y * xisize );
    register ULONG pred,pgrn,pblu,data;

    pred = pgrn = pblu = 0;
    for (x=0; x<xend; x++)
    {
      data = (ULONG )(*i_ptr++);
      switch( (data & 0x30) )
      {
        case 0x00: /* use color register given by low */
          { register ULONG low;
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
  if (    (cmap_true_to_all == TRUE) 
      || ((cmap_true_to_1st == TRUE) && (iff_chdr == 0)) )	
	iff_chdr = CMAP_Create_CHDR_From_True(tpic,8,8,8,xosize,yosize,
					iff_cmap,&iff_imagec);
  else if ( (cmap_true_to_332 == TRUE) && (iff_chdr == 0) )
	iff_chdr = CMAP_Create_332(iff_cmap,&iff_imagec);
  else if ( (cmap_true_to_gray == TRUE) && (iff_chdr == 0) )
	iff_chdr = CMAP_Create_Gray(iff_cmap,&iff_imagec);
 
  if (cmap_dither_type == CMAP_DITHER_FLOYD)
        out = UTIL_RGB_To_FS_Map(out,tpic,iff_chdr,xosize,yosize,TRUE);
  else
        out = UTIL_RGB_To_Map(out,tpic,iff_chdr,xosize,yosize,TRUE);
  *chdr = iff_chdr; 
}

void IFF_HAM8_As_True(out,in,chdr,h_cmap,xosize,yosize,xip,yip,xisize)
UBYTE *out,*in;
XA_CHDR **chdr;
ColorReg *h_cmap;
ULONG xosize,yosize,xip,yip,xisize;
{
  ULONG xend,y;
  UBYTE *tpic,*optr;
  USHORT g_adj[64];

  for(y=0;y<64;y++) g_adj[y] = xa_gamma_adj[ ((65 * y) >> 4) ] >> 8;
  tpic = (UBYTE *)malloc( 3 * xosize * yosize);
  if (tpic == 0) TheEnd1("IFF_HAM8_As_True: malloc err\n");
  optr = tpic;
  xend = xip + xosize; if (xend > xisize) xend = xisize;
  for (y=yip; y < (yip + yosize); y++)
  {
    register ULONG x;
    register UBYTE *i_ptr = (UBYTE *)( in + y * xisize );
    register ULONG pred,pgrn,pblu,data;

    pred = pgrn = pblu = 0;
    for (x=0; x<xend; x++)
    {
      data = (ULONG )(*i_ptr++);
      switch( (data & 0xc0) )
      {
        case 0x00: /* use color register given by low */
          { register ULONG low = data & 0x3f;
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
  if (    (cmap_true_to_all == TRUE) 
      || ((cmap_true_to_1st == TRUE) && (iff_chdr == 0)) )	
	iff_chdr = CMAP_Create_CHDR_From_True(tpic,8,8,8,xosize,yosize,
					iff_cmap,&iff_imagec);
  else if ( (cmap_true_to_332 == TRUE) && (iff_chdr == 0) )
	iff_chdr = CMAP_Create_332(iff_cmap,&iff_imagec);
  else if ( (cmap_true_to_gray == TRUE) && (iff_chdr == 0) )
	iff_chdr = CMAP_Create_Gray(iff_cmap,&iff_imagec);
 
  if (cmap_dither_type == CMAP_DITHER_FLOYD)
        out = UTIL_RGB_To_FS_Map(out,tpic,iff_chdr,xosize,yosize,TRUE);
  else
        out = UTIL_RGB_To_Map(out,tpic,iff_chdr,xosize,yosize,TRUE);
  *chdr = iff_chdr; 
}


ULONG
IFF_Delta_Body(image,delta,dsize,chdr,map,map_flag,imagex,imagey,imaged,
						xs,ys,xe,ye,special,extra)
UBYTE *image;			/* ptr to image out */
UBYTE *delta;			/* ptr to delta */
ULONG dsize;			/* delta size */
XA_CHDR *chdr;			/* color map info */
ULONG *map;			/* pixel map - use if map_flag */
ULONG map_flag;			/* ignored */
ULONG imagex,imagey,imaged;     /* image size and depth */
ULONG *xs,*ys;			/* pos of changed area */
ULONG *xe,*ye;			/* size of changed area */
ULONG special; 	                /* reserved */
void *extra;			/* Extra Info. */
{
  ULONG image_size = imagex * imagey;
  memcpy( (char *)image, (char *)delta, image_size);
  *xs = *ys = 0;  *xe = imagex; *ye = imagey; 
  return(ACT_DLTA_BODY);
}

