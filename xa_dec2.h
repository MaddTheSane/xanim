
/* REV 2 Decoder Info Structure */

typedef struct
{
  xaULONG cmd;			/* decode or query */
  xaULONG skip_flag;		/* skip_flag */
  xaULONG imagex,imagey;	/* Image Buffer Size */
  xaULONG imaged; 		/* Image depth */
  XA_CHDR *chdr;		/* Color Map Header */
  xaULONG map_flag;		/* remap image? */
  xaULONG *map;			/* map to use */
  xaULONG xs,ys;		/* pos of changed area */
  xaULONG xe,ye;		/* size of change area */
  xaULONG special;		/* Special Info */
  xaULONG bytes_pixel;		/* bytes per pixel */
  xaULONG image_type;		/* type of image */
  xaULONG tmp1;			/* future expansion */
  xaULONG tmp2;			/* future expansion */
  xaULONG tmp3;			/* future expansion */
  xaULONG tmp4;			/* future expansion */
  void *extra;			/* Decompression specific info */
} XA_DEC2_INFO;

#define XA_DEC_SPEC_RGB		0x0001
#define XA_DEC_SPEC_CF4		0x0002
#define XA_DEC_SPEC_DITH	0x0004

#define XA_IMTYPE_RGB		0x0001
#define XA_IMTYPE_GRAY		0x0002
#define XA_IMTYPE_CLR8		0x0003
#define XA_IMTYPE_CLR16		0x0004
#define XA_IMTYPE_CLR32		0x0005
#define XA_IMTYPE_332		0x0006
#define XA_IMTYPE_332DITH	0x0007
#define XA_IMTYPE_CF4		0x0008
#define XA_IMTYPE_CF4DITH	0x0009


