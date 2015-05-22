/*****************************************************************************
 *
 * Filename:
 * ---------
 *   CFG_EXT_MDTYPE_Default.h
 *
 * Project:
 * --------
 *   ALPS
 *
 * Description:
 * ------------
 *   EXT Modem Type Structure headerfile.
 *
 * Author:
 * -------
 *   AP Wang (mtk05304)
 *
 *
 * ==========================================================================
 * $Log$
 ****************************************************************************/
#ifndef _CFG_EXT_MDFILE_D_H
#define _CFG_EXT_MDFILE_D_H


// the record structure define of md type nvram file
typedef struct
{

    unsigned int md_id;
    unsigned int md_type;
	
} ext_md_type_struct;


//the record size and number of md type nvram file
#define CFG_FILE_EXT_MDTYPE_CONFIG_SIZE    sizeof(ext_md_type_struct)
#define CFG_FILE_EXT_MDTYPE_CONFIG_TOTAL   1

#endif /* _CFG_MDTYPE_FILE_H */

