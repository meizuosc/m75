/*******************************************************************************
 *
 * Filename:
 * ---------
 *   cfg_mdtype_file.h
 *
 * Project:
 * --------
 *   YuSu
 *
 * Description:
 * ------------
 *    header file of main function
 *
 * Author:
 * -------
 *   Haow Wang(MTK81183)
 *
 *------------------------------------------------------------------------------
 * $Revision:$
 * $Modtime:$
 * $Log:$
 *
 *******************************************************************************/



#ifndef _CFG_MDTYPE_FILE_H
#define _CFG_MDTYPE_FILE_H


// the record structure define of md type nvram file
typedef struct
{

    unsigned int md_type;
	
} md_type_struct;


//the record size and number of md type nvram file
#define CFG_FILE_MDTYPE_CONFIG_SIZE    sizeof(md_type_struct)
#define CFG_FILE_MDTYPE_CONFIG_TOTAL   1

#endif /* _CFG_MDTYPE_FILE_H */
