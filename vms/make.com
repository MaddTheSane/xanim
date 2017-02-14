$!  Make.com
$!  VMS script file to compile and link XAnim (v2.70.6.3) 
$!    02Aug96  Podlipec - modified for 27063 - untested
$!
$ on error then continue
$ CCOPT = "/Optimize"
$!
$! Switches for AXP/VAX
$!
$ if f$getsyi("HW_MODEL").ge.1024
$ then
$  ccopt = ccopt + "/prefix=all"
$   Define/NoLog DECC$SYSTEM_INCLUDE SYS$LIBRARY,DECW$INCLUDE
$   Define/NoLog Sys          DECC$SYSTEM_INCLUDE
$ else
$   Define/NoLog VAXC$INCLUDE SYS$LIBRARY,DECW$INCLUDE
$   Define/NoLog Sys          VAXC$INCLUDE
$ endif
$!
$! LNKOPT = "/MAP"
$ If P1 .EQS. "DEBUG" Then CCOPT  = CCOPT  + "/DEBUG/NOOPT"
$ If P1 .EQS. "DEBUG" Then LNKOPT = LNKOPT + "/DEBUG"
$!
$!	Build the option-file
$!
$ open/write optf xanim.opt
$ write optf "unpacker"
$ write optf "xa_acodec"
$ write optf "xa_act"
$ write optf "xa_au"
$ write optf "xa_audio"
$ write optf "xa_avi"
$ write optf "xa_cmap"
$ write optf "xa_color"
$ write optf "xa_dl"
$ write optf "xa_dumfx"
$ write optf "xa_fli"
$ write optf "xa_formats"
$ write optf "xa_gif"
$ write optf "xa_iff"
$ write optf "xa_input"
$ write optf "xa_ipc"
$ write optf "xa_jmov"
$ write optf "xa_jpg"
$ write optf "xa_movi"
$ write optf "xa_mpg"
$ write optf "xa_qt"
$ write optf "xa_qt_decs"
$ write optf "xa_replay"
$ write optf "xa_rle"
$ write optf "xa_set"
$ write optf "xa_show"
$ write optf "xa_txt"
$ write optf "xa_utils"
$ write optf "xa_wav"
$ write optf "xa_x11"
$ write optf "xanim"
$ write optf "Identification=""XAnim 2.70.6.3"""
$!
$!
$!  Find out which X-Version we're running.  This will fail for older
$!  VMS versions (i.e., v5.5-1).  Therefore, choose DECWindows XUI for
$!  default.
$!
$ On Error Then GoTo XUI
$ @sys$update:decw$get_image_version sys$share:decw$xlibshr.exe decw$version
$ if f$extract(4,3,decw$version).eqs."1.0"
$ then
$   write optf "Sys$share:DECW$DWTLIBSHR.EXE/Share"
$ endif
$ if f$extract(4,3,decw$version).eqs."1.1"
$ then
$   write optf "sys$share:decw$xmlibshr.exe/share"
$   write optf "sys$share:decw$xtshr.exe/share"
$ endif
$ if f$extract(4,3,decw$version).eqs."1.2"
$ then
$   write optf "sys$share:decw$xmlibshr12.exe/share"
$   write optf "sys$share:decw$xtlibshrr5.exe/share"
$ endif
$ GoTo MAIN
$!
$XUI:
$!
$   CCOPT = CCOPT + " /Define = R3_INTRINSICS"
$   write optf "Sys$share:DECW$DWTLIBSHR.EXE/Share"
$MAIN:
$! The following line should enable the Remote Control - not yet verified.
$   CCOPT = CCOPT + " /Define = XA_PETUNIA"
$!
$ write optf "sys$share:decw$xlibshr.exe/share"
$ close optf
$ On Error Then Continue
$!
$   CALL MAKE XANIM.OBJ "CC ''CCOPT' XANIM" -
		xanim.c xanim.h xa_config.h xa_x11.h xa_ipc_cmds.h
$   CALL MAKE XA_X11.OBJ "CC ''CCOPT' XA_X11" -
    		xanim.h xa_x11.h xa_x11.c
$   CALL MAKE UNPACKER.OBJ "CC ''CCOPT' UNPACKER" -
		unpacker.c
$   CALL MAKE XA_ACODEC.OBJ "CC ''CCOPT' XA_ACODEC" -
		xanim.h xa_acodec.c
$   CALL MAKE XA_ACT.OBJ "CC ''CCOPT' XA_ACT" -
		xanim.h xa_act.h xa_act.c
$   CALL MAKE XA_AU.OBJ "CC ''CCOPT' XA_AU" -
		xanim.h xa_au.c
$   CALL MAKE XA_AUDIO.OBJ "CC ''CCOPT' XA_AUDIO" -
		xanim.h xa_audio.h XA_audio.c
$   CALL MAKE XA_AVI.OBJ "CC ''CCOPT' XA_AVI" -
		xanim.h xa_avi.h xa_xmpg.h xa_codecs.h xa_avi.c
$   CALL MAKE XA_CMAP.OBJ  "CC ''CCOPT' XA_CMAP" -
		xanim.h xa_cmap.c
$   CALL MAKE XA_COLOR.OBJ  "CC ''CCOPT' XA_COLOR" -
		xanim.h xa_color.c
$   CALL MAKE XA_DL.OBJ  "CC ''CCOPT' XA_DL" -
		xanim.h xa_dl.h xa_dl.c
$   CALL MAKE XA_DUMFX.OBJ  "CC ''CCOPT' XA_DUMFX" -
		xanim.h xa_dumfx.c
$   CALL MAKE XA_FLI.OBJ "CC ''CCOPT' XA_FLI" -
		xanim.h xa_fli.h xa_fli.c
$   CALL MAKE XA_FORMATS.OBJ  "CC ''CCOPT' XA_FORMATS" -
		xanim.h xa_formats.c
$   CALL MAKE XA_GIF.OBJ "CC ''CCOPT' XA_GIF" -
		xanim.h xa_gif.h xa_gif.c 
$   CALL MAKE XA_IFF.OBJ "CC ''CCOPT' XA_IFF" -
		xanim.h xa_iff.h xa_iff.c
$   CALL MAKE XA_INPUT.OBJ  "CC ''CCOPT' XA_INPUT" -
		xanim.h xa_input.c
$   CALL MAKE XA_IPC.OBJ  "CC ''CCOPT' XA_IPC" -
		xanim.h xa_ipc.h xa_ipc.c
$   CALL MAKE XA_JMOV.OBJ "CC ''CCOPT' XA_JMOV" -
		xanim.h xa_jmov.h xa_jmov.c
$   CALL MAKE XA_JPG.OBJ "CC ''CCOPT' XA_JPG" -
		xanim.h xa_jpg.h xa_jpg.c
$   CALL MAKE XA_MOVI.OBJ "CC ''CCOPT' XA_MOVI" -
		xanim.h xa_movi.h xa_movi.c
$   CALL MAKE XA_MPG.OBJ "CC ''CCOPT' XA_MPG" -
		xanim.h xa_mpg.h xa_xmpg.h xa_jpg.h xa_mpg.c
$   CALL MAKE XA_QT.OBJ "CC ''CCOPT' XA_QT" -
		xanim.h xa_qt.h xa_codecs.h xa_qt.c
$   CALL MAKE XA_QT_DECS.OBJ "CC ''CCOPT' XA_QT_DECS" -
		xanim.h xa_qt.h xa_codecs.h xa_qt_decs.c
$   CALL MAKE XA_REPLAY.OBJ "CC ''CCOPT' XA_REPLAY" -
		xanim.h xa_replay.h xa_replay.c
$   CALL MAKE XA_RLE.OBJ "CC ''CCOPT' XA_RLE" -
		xanim.h xa_rle.h xa_rle.c 
$   CALL MAKE XA_SET.OBJ "CC ''CCOPT' XA_SET" -
		xanim.h xa_iff.h xa_iff.c xa_set.h xa_set.c
$   CALL MAKE XA_SHOW.obj "CC ''CCOPT' XA_SHOW" -
		xanim.h xa_show.h xa_show.c
$   CALL MAKE XA_TXT.OBJ "CC ''CCOPT' XA_TXT" -
		xanim.h xa_txt.h xa_txt.c
$   CALL MAKE XA_UTILS.OBJ "CC ''CCOPT' XA_UTILS" -
		xanim.h xa_utils.h xa_utils.c
$   CALL MAKE XA_WAV.OBJ "CC ''CCOPT' XA_WAV" -
		xanim.h xa_avi.h xa_wav.c
$!
$!
$!
$ CALL MAKE XANIM.EXE "LINK ''LNKOPT' xanim.opt/OPT" *.OBJ
$!
$!
$ EXIT
$!
$!
$!
$!
$MAKE: SUBROUTINE   !SUBROUTINE TO CHECK DEPENDENCIES
$ V = 'F$Verify(0)
$! P1 = What we are trying to make
$! P2 = Command to make it
$! P3 - P8  What it depends on
$
$ If F$Search(P1) .Eqs. "" Then Goto Makeit
$ Time = F$CvTime(F$File(P1,"RDT"))
$arg=3
$Loop:
$       Argument = P'arg
$       If Argument .Eqs. "" Then Goto Exit
$       El=0
$Loop2:
$       File = F$Element(El," ",Argument)
$       If File .Eqs. " " Then Goto Endl
$       AFile = ""
$Loop3:
$       OFile = AFile
$       AFile = F$Search(File)
$       If AFile .Eqs. "" .Or. AFile .Eqs. OFile Then Goto NextEl
$       If F$CvTime(F$File(AFile,"RDT")) .Ges. Time Then Goto Makeit
$       Goto Loop3
$NextEL:
$       El = El + 1
$       Goto Loop2
$EndL:
$ arg=arg+1
$ If arg .Le. 8 Then Goto Loop
$ Goto Exit
$
$Makeit:
$ VV=F$VERIFY(0)
$ write sys$output P2
$ 'P2
$ VV='F$Verify(VV)
$Exit:
$ If V Then Set Verify
$ENDSUBROUTINE

