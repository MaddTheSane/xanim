
/*
 * xanim_txt.c
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

#include "xanim_gif.h"


LONG Is_TXT_File();
ULONG TXT_Read_File();
void GIF_Free_Frame_List();
GIF_FRAME *GIF_Read_File();

ULONG UTIL_Get_MSB_Long();

typedef struct
{
  ULONG frame_cnt;
  GIF_FRAME *gframes;
} TXT_FRAME_LST;

static char gif_file_name[256];
static ULONG txt_max_imagex,txt_max_imagey,txt_max_imagec,txt_max_imaged;

/*
 * This file will open the filename passed to it and determine if
 * it is a txt91 file. If it is it returns TRUE, else FALSE. It
 * closes the file before returning.
 */
LONG Is_TXT_File(filename)
char *filename;
{
  FILE *fp;
  ULONG firstword;

  if ( (fp=fopen(filename,XA_OPEN_MODE)) == 0) return(XA_NOFILE);
  firstword = UTIL_Get_MSB_Long(fp);
  fclose(fp);
  /*                   t x t 9     so we ignore the 1 */
  if (firstword == 0x74787439) return(TRUE);
  return(FALSE);
}

/*
 * This file parse the txt animation file and converts it into actions. 
 *
 */
ULONG TXT_Read_File(fname,anim_hdr)
XA_ANIM_HDR *anim_hdr;
char *fname;
{
  FILE *fp;
  LONG ret,i,f_cnt;
  LONG num_of_files;
  LONG total_frame_cnt,txt_frame_cnt,t_time;
  LONG *txt_frames,txtframe_num;
  TXT_FRAME_LST *txt_frame_lst;

  txt_max_imagex = 0;
  txt_max_imagey = 0;
  txt_max_imagec = 0;
  txt_max_imaged = 0;

  if ( (fp=fopen(fname,XA_OPEN_MODE))==0)
  { 
    fprintf(stderr,"Can't open %s for reading.\n",fname); 
    return(FALSE);
  }

  /* read and throw away txt91 header */
  fscanf(fp,"%*s");

  /* Read the number of files */
  fscanf(fp,"%ld",&num_of_files);
  if (num_of_files <= 0)
  {
    fprintf(stderr,"num_of_file is invalid %ld\n",num_of_files);
    fclose(fp);
    return(FALSE);
  }
 
  txt_frame_lst = (TXT_FRAME_LST *) 
		malloc( sizeof(TXT_FRAME_LST) * num_of_files);
  if (txt_frame_lst == 0) TheEnd1("TXT_Read_File: malloc err");

  /* Read in the GIF files, use only the 1st one's colormap
   */
  for(i=0; i<num_of_files; i++)
  {
    fscanf(fp,"%s",gif_file_name);
    fprintf(stderr,"Reading %s\n",gif_file_name);
    txt_frame_lst[i].gframes 
	= GIF_Read_File(gif_file_name,anim_hdr,&ret);
    txt_frame_lst[i].frame_cnt = ret;
    if (anim_hdr->imagex > txt_max_imagex) txt_max_imagex = anim_hdr->imagex;
    if (anim_hdr->imagey > txt_max_imagey) txt_max_imagey = anim_hdr->imagey;
    if (anim_hdr->imagec > txt_max_imagec) txt_max_imagec = anim_hdr->imagec;
    if (anim_hdr->imaged > txt_max_imaged) txt_max_imaged = anim_hdr->imaged;
  }

  /* Check for Frame list at end of images.
   */
  ret=fscanf(fp,"%ld",&txtframe_num);
  if ( (ret == 1) && (txtframe_num >= 0))
  {
    LONG tmp_txtframe; 

    /* read in txt frame list, keep track of actual frames since each
     * txt_frame can have several frames(cmaps and images);
     */
    txt_frames = (LONG *) malloc(txtframe_num * sizeof(LONG) );
    if (txt_frames == 0) TheEnd1("TXT_Read_File: frames malloc err");
    total_frame_cnt = 0;
    txt_frame_cnt   = 0;

    for(i=0; i < txtframe_num; i++)
    {
      ret = fscanf(fp,"%ld",&tmp_txtframe);
      if ( (ret==1) && (tmp_txtframe >= 0) && (tmp_txtframe < num_of_files) )
      {
        txt_frames[txt_frame_cnt] = tmp_txtframe;
        total_frame_cnt += txt_frame_lst[ tmp_txtframe ].frame_cnt;
        txt_frame_cnt++;
      }
      else
        fprintf(stderr,"TXT_READ: bad frame number (%ld) in frame list\n",
							  tmp_txtframe);
    }
  } /* end of frame included at end */
  else
  {
    txt_frames = (LONG *) malloc(num_of_files * sizeof(LONG) );
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
    ULONG k,frame_cnt;
    GIF_FRAME *gtmp;

    gtmp      = txt_frame_lst[ txt_frames[i] ].gframes;
    frame_cnt = txt_frame_lst[ txt_frames[i] ].frame_cnt;
    k = 0;
    while(gtmp != 0)
    {
      if (k >= frame_cnt)
      {
        fprintf(stderr,"TXT_Read_Anim: frame inconsistency %ld %ld\n",
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
  return(TRUE);
}

