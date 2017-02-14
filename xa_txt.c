
/*
 * xa_txt.c
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

#include "xa_gif.h"


xaULONG TXT_Read_File();
extern void GIF_Free_Frame_List();
extern GIF_FRAME *GIF_Read_File();

extern xaULONG XA_Open_And_ID_File();
extern xaULONG XA_Setup_Input_Methods();

typedef struct
{
  xaULONG frame_cnt;
  GIF_FRAME *gframes;
} TXT_FRAME_LST;

static char gif_file_name[256];
static xaULONG txt_max_imagex,txt_max_imagey,txt_max_imagec,txt_max_imaged;

/*
 * This file parse the txt animation file and converts it into actions. 
 *
 */
xaULONG TXT_Read_File(fname,anim_hdr)
XA_ANIM_HDR *anim_hdr;
char *fname;
{ XA_INPUT *xin = anim_hdr->xin;
  FILE *fp;
  xaLONG ret,i,f_cnt;
  xaLONG num_of_files;
  xaLONG total_frame_cnt,txt_frame_cnt,t_time;
  xaLONG *txt_frames,txtframe_num;
  TXT_FRAME_LST *txt_frame_lst;
  char *tmp_fname = anim_hdr->name;

  txt_max_imagex = 0;
  txt_max_imagey = 0;
  txt_max_imagec = 0;
  txt_max_imaged = 0;

  xin->Close_File(xin); /* POD Temporary */
  if ( (fp=fopen(fname,XA_OPEN_MODE))==0)
  { 
    fprintf(stderr,"Can't open %s for reading.\n",fname); 
    return(xaFALSE);
  }

  /* read and throw away txt91 header */
  fscanf(fp,"%*s");

  /* Read the number of files */
  fscanf(fp,"%d",&num_of_files);
  if (num_of_files <= 0)
  {
    fprintf(stderr,"num_of_file is invalid %d\n",num_of_files);
    fclose(fp);
    return(xaFALSE);
  }
 
  txt_frame_lst = (TXT_FRAME_LST *) 
		malloc( sizeof(TXT_FRAME_LST) * num_of_files);
  if (txt_frame_lst == 0) TheEnd1("TXT_Read_File: malloc err");

  /* Read in the GIF files, use only the 1st one's colormap
   */
  for(i=0; i<num_of_files; i++)
  { xaULONG anim_type;

    fscanf(fp,"%s",gif_file_name);
    fprintf(stderr,"Reading %s\n",gif_file_name);

    anim_hdr->name = gif_file_name;

    if (XA_Setup_Input_Methods(anim_hdr->xin, anim_hdr->name) == xaFALSE)
								    continue;

    anim_type = XA_Open_And_ID_File(anim_hdr);
    if (anim_hdr->xin->type_flag & XA_IN_TYPE_RANDOM)
    { anim_hdr->xin->Seek_FPos(anim_hdr->xin,0,0);
      anim_hdr->xin->buf_cnt = 0;
      anim_hdr->xin->fpos = 0;
    }
    if (anim_type == XA_GIF_ANIM)
    {
      txt_frame_lst[i].gframes 
	= GIF_Read_File(gif_file_name,anim_hdr,&ret);
      txt_frame_lst[i].frame_cnt = ret;
      if (anim_hdr->imagex > txt_max_imagex) txt_max_imagex = anim_hdr->imagex;
      if (anim_hdr->imagey > txt_max_imagey) txt_max_imagey = anim_hdr->imagey;
      if (anim_hdr->imagec > txt_max_imagec) txt_max_imagec = anim_hdr->imagec;
      if (anim_hdr->imaged > txt_max_imaged) txt_max_imaged = anim_hdr->imaged;
    }
    else
    {
      fprintf(stderr,"TXT files only allow GIF images to be listed %x\n",
								anim_type);
    }
  }

  anim_hdr->name = tmp_fname;
  /* Check for Frame list at end of images.
   */
  ret=fscanf(fp,"%d",&txtframe_num);
  if ( (ret == 1) && (txtframe_num >= 0))
  {
    xaLONG tmp_txtframe; 

    /* read in txt frame list, keep track of actual frames since each
     * txt_frame can have several frames(cmaps and images);
     */
    txt_frames = (xaLONG *) malloc(txtframe_num * sizeof(xaLONG) );
    if (txt_frames == 0) TheEnd1("TXT_Read_File: frames malloc err");
    total_frame_cnt = 0;
    txt_frame_cnt   = 0;

    for(i=0; i < txtframe_num; i++)
    {
      ret = fscanf(fp,"%d",&tmp_txtframe);
      if ( (ret==1) && (tmp_txtframe >= 0) && (tmp_txtframe < num_of_files) )
      {
        txt_frames[txt_frame_cnt] = tmp_txtframe;
        total_frame_cnt += txt_frame_lst[ tmp_txtframe ].frame_cnt;
        txt_frame_cnt++;
      }
      else
        fprintf(stderr,"TXT_READ: bad frame number (%d) in frame list\n",
							  tmp_txtframe);
    }
  } /* end of frame included at end */
  else
  {
    txt_frames = (xaLONG *) malloc(num_of_files * sizeof(xaLONG) );
    if (txt_frames == 0) TheEnd1("TXT_Read_File: no frames malloc err");

    txt_frame_cnt = num_of_files;
    total_frame_cnt = 0;
    for(i=0; i<txt_frame_cnt; i++)
    {
      txt_frames[i] = i;
      total_frame_cnt += txt_frame_lst[ i ].frame_cnt;
    }
  }

  /* Allocate a frame_lst of that size.
   */
  anim_hdr->frame_lst 
	= (XA_FRAME *)malloc(sizeof(XA_FRAME) * (total_frame_cnt + 1));
  if (anim_hdr->frame_lst == NULL) TheEnd1("TXT_ANIM: frame malloc err");

  f_cnt = 0;
  t_time = 0;
  /* loop through valid frame list, adding gif_frames */
  for(i = 0; i < txt_frame_cnt; i++)
  {
    xaULONG k,frame_cnt;
    GIF_FRAME *gtmp;

    gtmp      = txt_frame_lst[ txt_frames[i] ].gframes;
    frame_cnt = txt_frame_lst[ txt_frames[i] ].frame_cnt;
    k = 0;
    while(gtmp != 0)
    {
      if (k >= frame_cnt)
      {
        fprintf(stderr,"TXT_Read_Anim: frame inconsistency %d %d\n",
		k,frame_cnt);
        break;
      }
      anim_hdr->frame_lst[f_cnt].time_dur = gtmp->time;
      anim_hdr->frame_lst[f_cnt].zztime = t_time;
      t_time += gtmp->time;
      anim_hdr->frame_lst[f_cnt].act  = gtmp->act;
      gtmp = gtmp->next;
      k++; f_cnt++;
    } /* end of while */
  } /* end of for */
  anim_hdr->imagex = txt_max_imagex;
  anim_hdr->imagey = txt_max_imagey;
  anim_hdr->imagec = txt_max_imagec;
  anim_hdr->imaged = txt_max_imaged;
  anim_hdr->frame_lst[f_cnt].time_dur = 0;
  anim_hdr->frame_lst[f_cnt].zztime = -1;
  anim_hdr->frame_lst[f_cnt].act  = 0;
  anim_hdr->loop_frame = 0;
  anim_hdr->last_frame = f_cnt - 1;
  FREE(txt_frames,0x4000); txt_frames=0;
  for(i=0; i < num_of_files; i++)
    GIF_Free_Frame_List(txt_frame_lst[i].gframes);
  FREE(txt_frame_lst,0x4001); txt_frame_lst=0;
  fclose(fp);
  return(xaTRUE);
}


