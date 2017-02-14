$!  Make.com
$!  VMS script file to compile and link XAnim (v2.69.7.8) 
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
$ write optf "xanim"
$ write optf "unpacker"
$ write optf "xanim_act"
$ write optf "xanim_audio"
$ write optf "xanim_avi"
$ write optf "xanim_cmap"
$ write optf "xanim_dl"
$ write optf "xanim_fli"
$ write optf "xanim_gif"
$ write optf "xanim_iff"
$ write optf "xanim_jpg"
$ write optf "xanim_mpg"
$ write optf "xanim_rle"
$ write optf "xanim_set"
$ write optf "xanim_qt"
$ write optf "xanim_show"
$ write optf "xanim_txt"
$ write optf "xanim_utils"
$ write optf "xanim_wav"
$ write optf "xanim_x11"
$ write optf "Identification=""XAnim 2.69.7.8"""
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
$!
$ write optf "sys$share:decw$xlibshr.exe/share"
$ close optf
$ On Error Then Continue
$!
$   CALL MAKE XANIM.OBJ "CC ''CCOPT' xanim" -
		xanim.c xanim.h xanim_config.h xanim_x11.h mytypes.h
$   CALL MAKE XANIM_X11.OBJ "CC ''CCOPT' xanim_X11" -
    		mytypes.h xanim.h xanim_x11.h xanim_x11.c
$   CALL MAKE XANIM_FLI.OBJ "CC ''CCOPT' xanim_FLI" -
		mytypes.h xanim.h xanim_fli.h xanim_fli.c
$   CALL MAKE XANIM_IFF.OBJ "CC ''CCOPT' xanim_IFF" -
		mytypes.h xanim.h xanim_iff.h xanim_iff.c
$   CALL MAKE XANIM_GIF.OBJ "CC ''CCOPT' xanim_GIF" -
		mytypes.h xanim.h xanim_gif.h xanim_gif.c 
$   CALL MAKE XANIM_TXT.OBJ "CC ''CCOPT' xanim_TXT" -
		mytypes.h xanim.h xanim_txt.h xanim_txt.c
$   CALL MAKE XANIM_AUDIO.obj "CC ''CCOPT' xanim_AUDIO" -
		xanim.h xanim_audio.h xanim_audio.c
$   CALL MAKE XANIM_SHOW.obj "CC ''CCOPT' xanim_SHOW" -
		xanim.h xanim_show.h xanim_show.c
$   CALL MAKE XANIM_RLE.OBJ "CC ''CCOPT' xanim_RLE" -
		mytypes.h xanim.h xanim_rle.h xanim_rle.c 
$   CALL MAKE XANIM_SET.OBJ "CC ''CCOPT' xanim_SET" -
		xanim.h xanim_iff.h xanim_iff.c xanim_set.h xanim_set.c
$   CALL MAKE XANIM_ACT.OBJ "CC ''CCOPT' xanim_ACT" -
		mytypes.h xanim.h xanim_act.h xanim_act.c
$   CALL MAKE XANIM_AVI.OBJ "CC ''CCOPT' xanim_AVI" -
		mytypes.h xanim.h xanim_avi.h xanim_avi.c
$   CALL MAKE XANIM_WAV.OBJ "CC ''CCOPT' xanim_WAV" -
		mytypes.h xanim.h xanim_avi.h xanim_wav.c
$   CALL MAKE XANIM_QT.OBJ "CC ''CCOPT' xanim_QT" -
		mytypes.h xanim.h xanim_qt.h xanim_qt.c
$   CALL MAKE XANIM_JPG.OBJ "CC ''CCOPT' xanim_JPG" -
		mytypes.h xanim.h xanim_jpg.h xanim_jpg.c
$   CALL MAKE XANIM_MPG.OBJ "CC ''CCOPT' xanim_MPG" -
		mytypes.h xanim.h xanim_mpg.h xanim_mpg.c
$   CALL MAKE XANIM_UTILS.OBJ "CC ''CCOPT' xanim_UTILS" -
		mytypes.h xanim.h xanim_utils.h xanim_utils.c
$   CALL MAKE XANIM_DL.OBJ  "CC ''CCOPT' XANIM_DL" -
		mytypes.h xanim.h xanim_dl.h xanim_dl.c
$   CALL MAKE XANIM_CMAP.OBJ  "CC ''CCOPT' XANIM_CMAP" -
		xanim.h xanim_cmap.c
$   CALL MAKE UNPACKER.OBJ "CC ''CCOPT' UNPACKER" -
		mytypes.h unpacker.c
$!   CALL MAKE TXTMERGE.OBJ "CC ''CCOPT' TXTMERGE" -
!		txtmerge.c
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

