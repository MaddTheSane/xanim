
/*
 * xanim_audio.c
 *
 * Copyright (C) 1994,1995 by Mark Podlipec.
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
#include <Intrinsic.h>
#include <StringDefs.h>
#include <Shell.h>

#include "xanim_x11.h"

/* Rather than spend time figuring out which ones are common
 * across machine types, just redo them all each time.
 * Eventually I'll simplify.
 */


/*********************** SPARC INCLUDES ********************************/
#ifdef XA_SPARC_AUDIO
/* Sun 4.1 -I/usr/demo/SOUND/multimedia ??? */
#include <errno.h>
#include <fcntl.h>
#include <stropts.h>
#ifdef SOLARIS
#include <sys/audioio.h>
#else
#include <sun/audioio.h>
#endif
#include <sys/file.h>
#include <sys/stat.h>
#endif

/*********************** IBM S6000 INCLUDES ******************************/
#ifdef XA_AIX_AUDIO
#include <errno.h>
#include <fcntl.h>
#include <sys/audio.h>
#include <stropts.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/param.h>
#endif

/*********************** NEC EWS INCLUDES ******************************/
#ifdef XA_EWS_AUDIO
#include <errno.h>
#include <sys/audio.h>
#endif

/*********************** SONY NEWS INCLUDES ****************************/
#ifdef XA_SONY_AUDIO
#include <errno.h>
#include <newsiodev/sound.h>
#endif

/*********************** LINUX INCLUDES ********************************/
#ifdef XA_LINUX_AUDIO
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
/* POD NOTE: possibly <machine/soundcard.h> ???*/

#ifdef __FreeBSD__
#include <machine/soundcard.h>
#else
#include <sys/soundcard.h>
#endif

#endif

/*********************** SGI INCLUDES **********************************/
#ifdef XA_SGI_AUDIO
#include <errno.h>
#include <fcntl.h>
#include <stropts.h>
#include <sys/time.h>
#include <audio.h>
#include <math.h>
#endif

/*********************** HP INCLUDES ***********************************/
#ifdef XA_HP_AUDIO
#include <fcntl.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <audio/Alib.h>
#include <audio/CUlib.h>
#endif

#ifdef XA_HPDEV_AUDIO
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <sys/audio.h>
#endif


/*********************** AF(AudioFile) INCLUDES ************************/
#ifdef XA_AF_AUDIO
#include <AF/audio.h>
#include <AF/AFlib.h>
#include <AF/AFUtils.h>
#endif

/*********************** NAS(Network Audio System) INCLUDES *************/
#ifdef XA_NAS_AUDIO
#undef BYTE
#include <audio/audiolib.h>
#include <audio/soundlib.h>
#include <audio/Xtutil.h>
#endif

/*********************** END   INCLUDES ********************************/

