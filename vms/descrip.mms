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
# Modified 12-MAR-1996 by Podlipec for XAnim 2.70.4    (untested?)
# Modified 02-AUG-1996 by Podlipec for XAnim 2.70.6.3  (untested?)

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
# Linker Options Section
#
# Choose ONLY one of the following sections:
#
# for DECWindows XUI use this one
OPT = XANIMDW.OPT/Option
#
# for DECWindows Motif v1.1
#OPT = XANIM11.OPT/Option
#
# for DECWindows Motif v1.2
#OPT = XANIM12.OPT/Option
#
#####################################
# REMOTE CONTROL
#
# This is the recommended remote control. It's not dependent upon
# what type of widgets you have(Athena, Motif, other)
#
XA_REMOTE_DEFS = -DXA_PETUNIA -D_NO_PROTO

#####################################
#  FINAL CFLAGS
#
CFLAGS	=  $(OPTIMIZE) $(DEBUG) $(DEFS) $(XA_REMOTE_DEFS) -DXA_KPCD

#####################################
#  Give the path to your include directories
#
.first
	@ Define /NoLog Sys Sys$Library
	@ Define /NoLog X11 DECW$Include
	@ Define /NoLog VAXC$INCLUDE DECW$Include,Sys$Library

CFILES = unpacker.c xa_acodec.c xa_act.c xa_au.c xa_audio.c xa_avi.c xa_cmap.c xa_color.c xa_dl.c xa_dumfx.c xa_fli.c xa_formats.c xa_gif.c xa_iff.c xa_input.c xa_ipc.c xa_jmov.c xa_jpg.c xa_movi.c xa_mpg.c xa_qt.c xa_qt_decs.c xa_replay.c xa_rle.c xa_set.c xa_show.c xa_txt.c xa_utils.c xa_wav.c xa1.0_kpcd.c xa_x11.c xanim.c

OFILES = unpacker.obj xa_acodec.obj xa_act.obj xa_au.obj xa_audio.obj xa_avi.obj xa_cmap.obj xa_color.obj xa_dl.obj xa_dumfx.obj xa_fli.obj xa_formats.obj xa_gif.obj xa_iff.obj xa_input.obj xa_ipc.obj xa_jmov.obj xa_jpg.obj xa_movi.obj xa_mpg.obj xa_qt.obj xa_qt_decs.obj xa_replay.obj xa_rle.obj xa_set.obj xa_show.obj xa_txt.obj xa_utils.obj xa_wav.obj xa1.0_kpcd.obj xa_x11.obj xanim.obj


xanim :		xanim.exe
	! Successfull build of XANIM v2.70.4

xanim.exe : $(OFILES)
	$(LINK) $(LINKFLAGS) $(OPT)

.c.obj :
	$(CC) $(CFLAGS) $*.c

# DO NOT DELETE THIS LINE
xanim.obj : xanim.h
xanim.obj : xa_config.h
xanim.obj : xa_x11.h
xanim.obj : xa_ipc_cmds.h
xanim.obj : xanim.c
xa_audio.obj : xanim.h
xa_audio.obj : xa_config.h
xa_audio.obj : xa_audio.c
xa_x11.obj : xanim.h
xa_x11.obj : xa_config.h
xa_x11.obj : xa_x11.h
xa_x11.obj : xa_x11.c
xa_fli.obj : xanim.h
xa_fli.obj : xa_config.h
xa_fli.obj : xa_fli.h
xa_fli.obj : xa_fli.c
xa_iff.obj : xanim.h
xa_iff.obj : xa_config.h
xa_iff.obj : xa_iff.h
xa_iff.obj : xa_iff.c
xa_gif.obj : xanim.h
xa_gif.obj : xa_config.h
xa_gif.obj : xa_gif.h
xa_gif.obj : xa_gif.c
xa_txt.obj : xanim.h
xa_txt.obj : xa_config.h
xa_txt.obj : xa_txt.c
xa_show.obj : xanim.h
xa_show.obj : xa_config.h
xa_show.obj : xa_show.h
xa_show.obj : xa_show.c
xa_rle.obj : xanim.h
xa_rle.obj : xa_config.h
xa_rle.obj : xa_rle.h
xa_rle.obj : xa_rle.c
xa_avi.obj : xanim.h
xa_avi.obj : xa_avi.h
xa_avi.obj : xa_xmpg.h
xa_avi.obj : xa_codecs.h
xa_avi.obj : xa_avi.c
xa_wav.obj : xanim.h
xa_wav.obj : xa_config.h
xa_wav.obj : xa_avi.h
xa_wav.obj : xa_wav.c
xa_set.obj : xanim.h
xa_set.obj : xa_config.h
xa_set.obj : xa_iff.h
xa_set.obj : xa_iff.c
xa_set.obj : xa_set.h
xa_set.obj : xa_set.c
xa_qt.obj : xanim.h
xa_qt.obj : xa_config.h
xa_qt.obj : xa_qt.h
xa_qt.obj : xa_codecs.h
xa_qt.obj : xa_qt.c
xa_dl.obj : xanim.h
xa_dl.obj : xa_config.h
xa_dl.obj : xa_dl.h
xa_dl.obj : xa_dl.c
xa_dumfx.obj : xanim.h
xa_dumfx.obj : xa_config.h
xa_dumfx.obj : xa_dumfx.h
xa_dumfx.obj : xa_dumfx.c
xa_acodec.obj : xanim.h
xa_acodec.obj : xa_config.h
xa_acodec.obj : xa_acodec.h
xa_acodec.obj : xa_acodec.c
xa_color.obj : xanim.h
xa_color.obj : xa_config.h
xa_color.obj : xa_color.h
xa_color.obj : xa_color.c
xa_formats.obj : xanim.h
xa_formats.obj : xa_formats.c
xa_input.obj : xanim.h
xa_input.obj : xa_input.c
xa_ipc.obj : xanim.h
xa_ipc.obj : xa_ipc.h
xa_ipc.obj : xa_ipc.c
xa_qt_decs.obj : xanim.h
xa_qt_decs.obj : xa_qt.h
xa_qt_decs.obj : xa_codecs.h
xa_qt_decs.obj : xa_qt_decs.h
xa_qt_decs.obj : xa_qt_decs.c
xa_au.obj : xanim.h
xa_au.obj : xa_au.c
xa_wav.obj : xanim.h
xa_wav.obj : xa_wav.c
xa_movi.obj : xanim.h
xa_movi.obj : xa_movi.h
xa_movi.obj : xa_movi.c
xa_replay.obj : xanim.h
xa_replay.obj : xa_replay.h
xa_replay.obj : xa_replay.c
xa_jmov.obj : xanim.h
xa_jmov.obj : xa_jmov.h
xa_jmov.obj : xa_jmov.c
xa_jpg.obj : xanim.h
xa_jpg.obj : xa_config.h
xa_jpg.obj : xa_jpg.h
xa_jpg.obj : xa_jpg.c
xa_mpg.obj : xanim.h
xa_mpg.obj : xa_xmpg.h
xa_mpg.obj : xa_mpg.h
xa_mpg.obj : xa_mpg.c
xa_mpg.obj : xa_jpg.c
xa_act.obj : xanim.h
xa_act.obj : xa_config.h
xa_act.obj : xa_act.c
xa_utils.obj : xanim.h
xa_utils.obj : xa_config.h
xa_utils.obj : xa_utils.c
xa_cmap.obj : xanim.h
xa_cmap.obj : xa_config.h
xa_cmap.obj : xa_cmap.c
xa1.0_kpcd.obj : xa1.0_kpcd.c
unpacker.obj : unpacker.c

clean :
	@- Delete /NoLog /NoConfirm *.obj;*
	@- Purge /NoLog /NoConfirm
