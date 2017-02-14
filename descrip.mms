#
# MMS Description file for XAnim
#
# Based on the Unix Makefile
# Made by Rick Dyson (dyson@IowaSP.Physics.UIowa.EDU)
# Created  17-NOV-1993 for XAnim v2.60.0
# Modified 10-JUN-1994 for XAnim v2.68
# Modified 22-DEC-1994 for XAnim v2.69.7.4a
# Modified 19-JAN-1995 for XAnim v2.69.7.5
# Modified 31-JAN-1995 for XAnim v2.69.7.6
# Modified 02-MAY-1995 for XAnim v2.69.7.8

#####################################
# Choose your favorite compiler.
#
# standard VAX C
CC = cc

#####################################
# Machine Specific Flags
# 
# NO_INSTALL_CMAP prevents XAnim from specifically installing a cmap.
#  this causes problems on some PC versions of X11
#MACH = "NO_INSTALL_CMAP"

#####################################
#
# If you use DECWindows XUI (not Motif) use this define
# 
DEFS = /Define = R3_INTRINSICS

#####################################
# Optimization flags
#
# VAX C
OPTIMIZE= /Optimize

#####################################
# Debug flags
#
# VAX C
#DEBUG = /Debug /NoOptimize

#####################################
# Linker Options file
#
# Note sure about XA_REMOTE_DEFS. Comment them out if you have compile
# problems.
# Choose ONLY one of the following sections:
#
# for DECWindows XUI use this one
OPT = XANIMDW.OPT/Option
#XA_REMOTE_DEFS = -DXA_MOTIF -D_NO_PROTO
#
# for DECWindows Motif v1.1
#OPT = XANIM11.OPT/Option
#XA_REMOTE_DEFS = -DXA_MOTIF -D_NO_PROTO
#
# for DECWindows Motif v1.2
#OPT = XANIM12.OPT/Option
#XA_REMOTE_DEFS = -DXA_MOTIF -D_NO_PROTO

#####################################
#  FINAL CFLAGS
#
CFLAGS	=  $(OPTIMIZE) $(DEBUG) $(DEFS) $(XA_REMOTE_DEFS)

#####################################
#  Give the path to your include directories
#
.first
	@ Define /NoLog Sys Sys$Library
	@ Define /NoLog X11 DECW$Include
	@ Define /NoLog VAXC$INCLUDE DECW$Include,Sys$Library

CFILES = xanim.c xanim_x11.c xanim_fli.c xanim_iff.c xanim_gif.c xanim_txt.c unpacker.c xanim_utils.c xanim_act.c xanim_show.c xanim_set.c xanim_cmap.c xanim_rle.c xanim_avi.c xanim_qt.c xanim_wav.c xanim_jpg.c xanim_mpg.c xanim_dl.c

OFILES = xanim.obj xanim_x11.obj xanim_fli.obj xanim_iff.obj xanim_gif.obj xanim_txt.obj unpacker.obj xanim_utils.obj xanim_act.obj xanim_show.obj xanim_set.obj xanim_cmap.obj xanim_rle.obj xanim_avi.obj xanim_qt.obj xanim_wav.obj xanim_jpg.obj xanim_mpg.obj xanim_dl.obj

xanim :		xanim.exe
	! Successfull build of XANIM v2.69.7.6

xanim.exe : $(OFILES)
	$(LINK) $(LINKFLAGS) $(OPT)

.c.obj :
	$(CC) $(CFLAGS) $*.c

# DO NOT DELETE THIS LINE
xanim.obj : xanim.h
xanim.obj : xanim_config.h
xanim.obj : xanim_x11.h
xanim.obj : xanim.c
xanim_audio.obj : xanim.h
xanim_audio.obj : xanim_config.h
xanim_audio.obj : xanim_audio.c
xanim_x11.obj : xanim.h
xanim_x11.obj : xanim_config.h
xanim_x11.obj : xanim_x11.h
xanim_x11.obj : xanim_x11.c
xanim_fli.obj : xanim.h
xanim_fli.obj : xanim_config.h
xanim_fli.obj : xanim_fli.h
xanim_fli.obj : xanim_fli.c
xanim_iff.obj : xanim.h
xanim_iff.obj : xanim_config.h
xanim_iff.obj : xanim_iff.h
xanim_iff.obj : xanim_iff.c
xanim_gif.obj : xanim.h
xanim_gif.obj : xanim_config.h
xanim_gif.obj : xanim_gif.h
xanim_gif.obj : xanim_gif.c
xanim_txt.obj : xanim.h
xanim_txt.obj : xanim_config.h
xanim_txt.obj : xanim_txt.c
xanim_show.obj : xanim.h
xanim_show.obj : xanim_config.h
xanim_show.obj : xanim_show.h
xanim_show.obj : xanim_show.c
xanim_rle.obj : xanim.h
xanim_rle.obj : xanim_config.h
xanim_rle.obj : xanim_rle.h
xanim_rle.obj : xanim_rle.c
xanim_avi.obj : xanim.h
xanim_avi.obj : xanim_config.h
xanim_avi.obj : xanim_avi.h
xanim_avi.obj : xanim_avi.c
xanim_wav.obj : xanim.h
xanim_wav.obj : xanim_config.h
xanim_wav.obj : xanim_avi.h
xanim_wav.obj : xanim_wav.c
xanim_set.obj : xanim.h
xanim_set.obj : xanim_config.h
xanim_set.obj : xanim_iff.h
xanim_set.obj : xanim_iff.c
xanim_set.obj : xanim_set.h
xanim_set.obj : xanim_set.c
xanim_qt.obj : xanim.h
xanim_qt.obj : xanim_config.h
xanim_qt.obj : xanim_qt.h
xanim_qt.obj : xanim_qt.c
xanim_dl.obj : xanim.h
xanim_dl.obj : xanim_config.h
xanim_dl.obj : xanim_dl.h
xanim_dl.obj : xanim_dl.c
xanim_jpg.obj : xanim.h
xanim_jpg.obj : xanim_config.h
xanim_jpg.obj : xanim_jpg.h
xanim_jpg.obj : xanim_jpg.c
xanim_mpg.obj : xanim.h
xanim_mpg.obj : xanim_config.h
xanim_mpg.obj : xanim_mpg.h
xanim_mpg.obj : xanim_mpg.c
xanim_mpg.obj : xanim_jpg.c
xanim_act.obj : xanim.h
xanim_act.obj : xanim_config.h
xanim_act.obj : xanim_act.c
xanim_utils.obj : xanim.h
xanim_utils.obj : xanim_config.h
xanim_utils.obj : xanim_utils.c
xanim_cmap.obj : xanim.h
xanim_cmap.obj : xanim_config.h
xanim_cmap.obj : xanim_cmap.c
unpacker.obj : unpacker.c

clean :
	@- Delete /NoLog /NoConfirm *.obj;*
	@- Purge /NoLog /NoConfirm
