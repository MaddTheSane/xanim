#ifndef __XA_FORMATS_H__
#define __XA_FORMATS_H__

#include "xanim.h"

extern xaULONG MPG_Read_File(const char *fname, XA_ANIM_HDR *anim_hdr);
extern xaULONG IFF_Read_File(const char *fname, XA_ANIM_HDR *anim_hdr);
extern xaULONG GIF_Read_Anim(const char *fname, XA_ANIM_HDR *anim_hdr);
extern xaULONG TXT_Read_File(const char *fname, XA_ANIM_HDR *anim_hdr);
extern xaULONG Fli_Read_File(const char *fname, XA_ANIM_HDR *anim_hdr);
extern xaULONG DL_Read_File(const char *fname, XA_ANIM_HDR *anim_hdr);
/* xaULONG PFX_Read_File(); */
extern xaULONG MOVI_Read_File(const char *fname, XA_ANIM_HDR *anim_hdr);
extern xaULONG SET_Read_File(const char *fname, XA_ANIM_HDR *anim_hdr);
extern xaULONG RLE_Read_File(const char *fname, XA_ANIM_HDR *anim_hdr);
/*!
 * \param audio_attempt \c xaTRUE if audio is to be attempted 
 */
extern xaULONG AVI_Read_File(const char *fname, XA_ANIM_HDR *anim_hdr, xaULONG audio_attempt);
/*!
 * \param audio_attempt \c xaTRUE if audio is to be attempted 
 */
extern xaULONG QT_Read_File(const char *fname, XA_ANIM_HDR *anim_hdr, xaULONG audio_attempt);
extern xaULONG JFIF_Read_File(const char *fname, XA_ANIM_HDR *anim_hdr);
/*!
 * \param audio_attempt \c xaTRUE if audio is to be attempted 
 */
extern xaULONG JMOV_Read_File(const char *fname, XA_ANIM_HDR *anim_hdr, xaULONG audio_attempt);
/*!
 * \param audio_attempt \c xaTRUE if audio is to be attempted 
 */
extern xaULONG ARM_Read_File(const char *fname, XA_ANIM_HDR *anim_hdr, xaULONG audio_attempt);
/*!
 * \param audio_attempt \c xaTRUE if audio is to be attempted 
 */
extern xaULONG WAV_Read_File(const char *fname, XA_ANIM_HDR *anim_hdr, xaULONG audio_attempt);
/*!
 * \param audio_attempt \c xaTRUE if audio is to be attempted 
 */
extern xaULONG AU_Read_File(const char *fname, XA_ANIM_HDR *anim_hdr, xaULONG audio_attempt);
/*!
 * \param audio_attempt \c xaTRUE if audio is to be attempted 
 */
extern xaULONG SVX_Read_File(const char *fname, XA_ANIM_HDR *anim_hdr, xaULONG audio_attempt);
extern xaULONG DUM_Read_File(const char *fname, XA_ANIM_HDR *anim_hdr);
/*!
 * \param audio_attempt \c xaTRUE if audio is to be attempted 
 */
extern xaULONG QCIF_Read_File(const char *fname, XA_ANIM_HDR *anim_hdr, xaULONG audio_attempt);
/*!
 * \param audio_attempt \c xaTRUE if audio is to be attempted 
 */
extern xaULONG CIF_Read_File(const char *fname, XA_ANIM_HDR *anim_hdr, xaULONG audio_attempt);
/*!
 * \param audio_attempt \c xaTRUE if audio is to be attempted 
 */
extern xaULONG J6I_Read_File(const char *fname, XA_ANIM_HDR *anim_hdr, xaULONG audio_attempt);


#endif
