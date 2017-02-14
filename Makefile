.SUFFIXES: .c .o

# XAnim Makefile.unx for Rev 2.69.7.8

# !*!*!*! IMPORTANT: If you use OpenWindows with SunOS 4.1.3
# !*!*!*! then see the "XA_REMOTE_LIBS" and "XLIBS" defines for
# !*!*!*! work-arounds for bugs with the OpenWindows libraries.
#
# !*!*!*! IMPORTANT: If you're using Solaris/SunOS 5.x you must
# !*!*!*! include the following libraries  "-lsocket -lnsl". See the 
# !*!*!*! Machine Specific Section(XA_SOLARIS_LIBS).
#
# !*!*!*! IMPORTANT: If you're using HP/UX you'll want to look in
# !*!*!*! Machine Specific Section(XA_HPUX_DEFS).
#
# !*!*!*! MOTIF: Most of you DON'T have it(having the motif window manager
# !*!*!*! is not the same as having the motif libraries). So double check
# !*!*!*! to make sure you have libXm.a in you X11 library directory before
# !*!*!*! defining the XA_REMOTE_* defines to use Motif. Same goes
# !*!*!*! with Athena 3D.
#
# Makefile for XAnim Rev 2.69.7 BETA
#
############################################################################
# -- Video Support Defines
############################################################################
# =========
XA_VIDEO_DEFS =
XA_VIDEO_LIBS =
XA_VIDEO_INCS =
XA_VIDEO_DEPS =
#
############################################################################
# -- Audio Support Defines - Select which Machine you have
############################################################################
# -- SPARC Workstations SunOS 4.1.x -------------------------------
# 
XA_AUDIO_DEFS = -DXA_SPARC_AUDIO  
#
# -- SPARC Workstations Solaris/SunOS 5.x -------------------------
# XA_AUDIO_DEFS = -DXA_SPARC_AUDIO -DSOLARIS
#
# -- S/6000 - AIX 4.1 (3.2.5??) --------- -------------------------
#XA_AUDIO_DEFS = -DXA_AIX_AUDIO
#
# -- NEC EWS Workstations -----------------------------------------
# XA_AUDIO_DEFS = -DXA_EWS_AUDIO
#
# -- SONY Workstations NEWS-5000 NEWS-OS 4.2 ----------------------
# XA_AUDIO_DEFS = -DXA_SONY_AUDIO
#
# -- Linux PC's with OLD Rev sound drivers ------------------------
# -- might also need -lc
# XA_AUDIO_DEFS = -DXA_LINUX_AUDIO
# XA_AUDIO_LIBS = -lc
#
# -- Linux PC's with NEWER Rev sound drivers(Rev 2.5 and up) ------
# -- might also need -lc
# XA_AUDIO_DEFS = -DXA_LINUX_AUDIO -DXA_LINUX_NEWER_SND
# XA_AUDIO_LIBS = -lc
#
# -- SGI Indigo ---------------------------------------------------
# XA_AUDIO_DEFS = -DXA_SGI_AUDIO
# XA_AUDIO_LIBS = -laudio
#
# -- HP 9000/700 Use Upper Level Audio Layers--Requires Aserver----
# XA_AUDIO_DEFS = -DXA_HP_AUDIO 
# XA_AUDIO_LIBS = -lAlib
#
# -- HP 9000/700 Direct access to Audio Device---------------------
# XA_AUDIO_DEFS = -DXA_HPDEV_AUDIO 
# XA_AUDIO_LIBS = 
#
# ------ AF (AudioFile) Support -----------------------------------
# -- You need to indicate where the AF libraries libAFUtil.a and libAF.a
# -- are located. Typically this is /usr/local/lib, but not always.
# -- Same with the AF/include directory.
# XA_AUDIO_DEFS = -DXA_AF_AUDIO
# XA_AUDIO_LIBS = -L/usr/local/lib -lAFUtil -lAF
# XA_AUDIO_INCS = -I/usr/local/include
#
# ------ NAS (Network Audio System) Support -----------------------
# -- You need to indicate where the NAS library libaudio.a
# -- is located. Typically this is /usr/local/lib, but not always.
# -- Same with the include/audio directory.
# XA_AUDIO_DEFS = -DXA_NAS_AUDIO
# XA_AUDIO_LIBS = -L/usr1/podlipec/nas/nas/lib/audio -laudio
# XA_AUDIO_INCS = -I/usr1/podlipec/nas/nas/include -I/usr1/podlipec/nas/nas/lib
#
############

############################################################################
# X11 WIDGET Toolkit Options for the Remote Control
############################################################################
#********** IMPORTANT ***********************************************
#*** IF YOU ARE USING SunOS 4.1.3 and  OpenWindows you must replace 
#*** "-lXmu" below with "/usr/openwin/lib/libXmu.a" because of a bug
#*** in the OpenWin shared libs. See example at end of this section
#********************************************************************
######## USE THE ATHENA WIDGET SET FOR THE REMOTE CONTROL
XA_REMOTE_LIBS = -lXaw -lXmu
XA_REMOTE_DEFS = -DXA_ATHENA
#
######## USE THE 3D ATHENA WIDGET SET FOR THE REMOTE CONTROL
#XA_REMOTE_LIBS = -lXaw3d -lXmu
#XA_REMOTE_DEFS = -DXA_ATHENA
#
######## USE THE MOTIF WIDGET SET FOR THE REMOTE CONTROL
#XA_REMOTE_LIBS = -lXm
#XA_REMOTE_DEFS = -DXA_MOTIF -D_NO_PROTO
#
# OPENWINDOWS AND SUNOS 4.1.3  Example
#XA_REMOTE_LIBS = -lXaw /usr/openwin/lib/libXmu.a
#XA_REMOTE_DEFS = -DXA_ATHENA
#
# HP/UX Motif Example
#XA_REMOTE_LIBS = -lXm
#XA_REMOTE_DEFS = -DXA_MOTIF -D_NO_PROTO -I/usr/include/Motif1.2

############################################################################
# Machine Specific Defines and Libs
############################################################################
#------------------------------------------
# IMPORTANT: If you're using Solaris/SunOS 5.x then uncomment out the
# define below.
#XA_SOLARIS_LIBS = -lsocket -lnsl
#
# Some revs of Solaris also require -lgen when using Motif.
#XA_SOLARIS_LIBS = -lsocket -lnsl -lgen
#
#------------------------------------------
# IMPORTANT: If you're using HP/UX then uncomment out the define below:
#XA_HPUX_DEFS = -Wp,-H150000
# This increases the macro symbol table(default is 128000).
#
#------------------------------------------
# -DNO_INSTALL_CMAP prevents XAnim from specifically installing a cmap.
#  this causes problems on some PC versions of X11
#
#XA_CMAP = -DNO_INSTALL_CMAP
# 
#------------------------------------------
# -DXA_XTPOINTER typedefs XtPointer as void*  if your system doesn't
#   typedef it already.
#  
#XA_XTPTR = -DXA_XTPOINTER
#  
#------------------------------------------
# X11 Shared Memory
# Allow use of shared memory if specified on cmd line. Comment this out
# if get compiler errors about not finding XShm.h or the following
# symbols(XShmCreateImage,XShmAttach,XShmDetach,XShmPutImage,etc).
XA_SHARED = -DXSHM
#
#------------------------------------------
# -DMSDOS for DESQview X on MSDOS machines.
#
#XA_MSDOS = -DMSDOS
#
#------------------------------------------
# If you have a i486 box running Interactive UNIX v2.2.1 you might
# have to uncomment the following line.
#
#XA_INET_LIB = -linet
#
############
XA_MACH_DEFS = $(XA_CMAP) $(XA_XTPTR) $(XA_SHARED) $(XA_MSDOS)
XA_MACH_LIBS = $(XA_INET_LIB) $(XA_SOLARIS_LIBS)



############################################################################
# Choose your favorite compiler.
#
# standard
CC = cc
#
# GNU C
#CC = gcc 


############################################################################
# Optimization flags for your favorite compiler.
#
# CC
#OPTIMIZE= -O
#OPTIMIZE= -O3
#
#  GNU C
# the -fno-builtin prevents GNU C from using builtin include files.
#OPTIMIZE= -O
#OPTIMIZE= -O2


############################################################################
# Debug flags for your favorite compiler.
#
# CC
DEBUG = -g
#
# GNU C
#DEBUG = -g

#####################################
#  Give the path to your include directories
#
INCLUDE =  -I/usr/include -I/usr/include/X11
#
# Sparc's
#INCLUDE = -I/usr/include -I/usr/openwin/include -I/usr/openwin/include/X11
# or 
#INCLUDE = -I/usr/include -I/usr/openwin3/include -I/usr/openwin3/include/X11
#
# OTHER
#INCLUDE = -I/usr/include -I/usr/include/X11R5
#INCLUDE = -I/usr/include -I/usr/include/X11R6
#INCLUDE = -I/usr/include/X11R5 -I/usr/include/X11R5/X11
#INCLUDE = -I/usr/include/X11R6 -I/usr/include/X11R6/X11


#####################################
#
# If your're using X11R6 you might need these libraries
# 
#XA_X11R6_LIBS = -lSM -lICE

#####################################
#  Give the path to your X11 libraries
#
XLIBDIR = -L/usr/lib/X11 
#
# Sparc's 
#XLIBDIR = -L/usr/openwin/lib
#
# Other's
#XLIBDIR = -L/usr/lib/X11R3
#XLIBDIR = -L/usr/lib/X11R4
#XLIBDIR = -L/usr/lib/X11R5
#XLIBDIR = -L/usr/lib/X11R6

#####################################
#  FINAL CFLAGS and OTHER_LIBS
#

XA_DEFS = $(XA_MACH_DEFS) $(XA_VIDEO_DEFS) $(XA_AUDIO_DEFS) $(XA_REMOTE_DEFS)
XA_LIBS = $(XA_X11R6_LIBS) $(XA_MACH_LIBS) $(XA_VIDEO_LIBS) $(XA_AUDIO_LIBS) 
XA_INCS = $(INCLUDE) $(XA_VIDEO_INCS) $(XA_AUDIO_INCS)
CFLAGS	=  $(DEBUG) $(OPTIMIZE)

#####################################
# FINAL LIBS
#
XLIBS	= $(XA_LIBS) $(XA_REMOTE_LIBS) -lXext -lXt -lX11 -lm -lc
#
# IMPORTANT: If you are using OpenWindows/SunOS4.1.3 then use the following:
#XLIBS	= $(XA_LIBS) $(XA_REMOTE_LIBS) -lXext -lXt -lX11 -lm -lXext

# xanim_pfx.c TEMP REMOVED
CFILES = xanim.c xanim_show.c xanim_x11.c xanim_fli.c xanim_iff.c  \
	xanim_gif.c xanim_txt.c unpacker.c xanim_utils.c xanim_act.c \
	xanim_set.c xanim_cmap.c xanim_rle.c xanim_wav.c \
	xanim_avi.c xanim_qt.c xanim_audio.c \
	xanim_jpg.c xanim_mpg.c xanim_dl.c


OFILES = xanim.o xanim_show.o xanim_x11.o xanim_fli.o xanim_iff.o  \
	xanim_gif.o xanim_txt.o unpacker.o xanim_utils.o xanim_act.o \
	xanim_set.o xanim_cmap.o xanim_rle.o xanim_wav.o \
	xanim_avi.o xanim_qt.o xanim_audio.o \
	xanim_jpg.o xanim_mpg.o xanim_dl.o

xanim: $(OFILES) $(XA_VIDEO_DEPS)
	$(CC) $(CFLAGS) -o xanim $(OFILES) $(XLIBDIR) $(XLIBS)

.c.o:
	$(CC) $(CFLAGS) $(XA_DEFS) $(XA_INCS) -c $*.c

txtmerge:	
	$(CC) $(CFLAGS) -o txtmerge txtmerge.c


# DO NOT DELETE THIS LINE
xanim.o: Makefile
xanim.o: xanim.h
xanim.o: xanim_config.h
xanim.o: xanim_x11.h
xanim.o: xanim.c
xanim_show.o: Makefile
xanim_show.o: xanim.h
xanim_show.o: xanim_show.c
xanim_x11.o: Makefile
xanim_x11.o: xanim.h
xanim_x11.o: xanim_config.h
xanim_x11.o: xanim_x11.h
xanim_x11.o: xanim_x11.c
xanim_fli.o: xanim.h
xanim_fli.o: xanim_config.h
xanim_fli.o: xanim_fli.h
xanim_fli.o: xanim_fli.c
xanim_iff.o: xanim.h
xanim_iff.o: xanim_config.h
xanim_iff.o: xanim_iff.h
xanim_iff.o: xanim_iff.c
xanim_gif.o: xanim.h
xanim_gif.o: xanim_config.h
xanim_gif.o: xanim_gif.h
xanim_gif.o: xanim_gif.c
xanim_txt.o: xanim.h
xanim_txt.o: xanim_config.h
xanim_txt.o: xanim_txt.c
xanim_dl.o: xanim.h
xanim_dl.o: xanim_config.h
xanim_dl.o: xanim_dl.h
xanim_dl.o: xanim_dl.c
xanim_pfx.o: xanim.h
xanim_pfx.o: xanim_config.h
xanim_pfx.o: xanim_pfx.h
xanim_pfx.o: xanim_pfx.c
xanim_rle.o: xanim.h
xanim_rle.o: xanim_config.h
xanim_rle.o: xanim_rle.h
xanim_rle.o: xanim_rle.c
xanim_avi.o: xanim.h
xanim_avi.o: xanim_config.h
xanim_avi.o: xanim_avi.h
xanim_avi.o: xanim_avi.c
xanim_wav.o: xanim.h
xanim_wav.o: xanim_config.h
xanim_wav.o: xanim_avi.h
xanim_wav.o: xanim_wav.c
xanim_qt.o: xanim.h
xanim_qt.o: xanim_config.h
xanim_qt.o: xanim_qt.h
xanim_qt.o: xanim_qt.c
xanim_jpg.o: xanim.h
xanim_jpg.o: xanim_config.h
xanim_jpg.o: xanim_jpg.h
xanim_jpg.o: xanim_jpg.c
xanim_mpg.o: xanim.h
xanim_mpg.o: xanim_config.h
xanim_mpg.o: xanim_mpg.h
xanim_mpg.o: xanim_mpg.c
xanim_mpg.o: xanim_jpg.c
xanim_set.o: xanim.h
xanim_set.o: xanim_config.h
xanim_set.o: xanim_iff.h
xanim_set.o: xanim_iff.c
xanim_set.o: xanim_set.h
xanim_set.o: xanim_set.c
xanim_act.o: xanim.h
xanim_act.o: xanim_config.h
xanim_act.o: xanim_act.c
xanim_utils.o: xanim.h
xanim_utils.o: xanim_config.h
xanim_utils.o: xanim_utils.c
xanim_cmap.o: xanim.h
xanim_cmap.o: xanim_config.h
xanim_cmap.o: xanim_cmap.c
xanim_audio.o: Makefile
xanim_audio.o: xanim.h
xanim_audio.o: xanim_config.h
xanim_audio.o: xanim_audio.h
xanim_audio.o: xanim_audio.c
unpacker.o: unpacker.c
