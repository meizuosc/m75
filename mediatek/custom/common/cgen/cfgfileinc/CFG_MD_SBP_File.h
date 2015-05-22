/*******************************************************************************
 *
 * Filename:
 * ---------
 *   CFG_MD_SBP_FILE.h
 *
 * Project:
 * --------
 *   KK
 *
 * Description:
 * ------------
 *    header file of md SBP config struct
 *
 * Author:
 * -------
 *    Jue Zhou (mtk80024)
 *
 *------------------------------------------------------------------------------
 * $Revision:$
 * $Modtime:$
 * $Log:$
 *
 *
 *
 *******************************************************************************/

#ifndef _CFG_MD_SBP_FILE_H
#define _CFG_MD_SBP_FILE_H

#define SBP_FLAG_UPDATED 1 // updated, need not send to md, 0: the latest setting, need update
// SBP: Single Binary Platfor, used for modem customization
typedef struct
{	
	unsigned int md_sbp_code;     // bit defined by modem;
	unsigned int flag_updated;    // 0: need updated, send to md, rm -r /data/nvram/md 1:updated, need not send to md
} MD_SBP_Struct;

#define CFG_FILE_MD_SBP_CONFIG_SIZE    sizeof(MD_SBP_Struct)
#define CFG_FILE_MD_SBP_CONFIG_TOTAL   1

#endif	// _CFG_MD_SBP_FILE_H

