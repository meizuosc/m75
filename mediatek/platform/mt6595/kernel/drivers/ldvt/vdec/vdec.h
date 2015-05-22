/*****************************************************************************
 *
 * Filename:
 * ---------
 *    auxadc.h
 *
 * Project:
 * --------
 *   MT6573 DVT
 *
 * Description:
 * ------------
 *   This file is for Auxiliary ADC Unit.
 *
 * Author:
 * -------
 *  Myron Li
 *
 *============================================================================
 *             HISTORY
 * Below this line, this part is controlled by PVCS VM. DO NOT MODIFY!!
 *------------------------------------------------------------------------------
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by PVCS VM. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/
 
#ifndef MT8320_LDVT_VDEC_TS_H
#define MT8320_LDVT_VDEC_TS_H

#include <mach/mt_irq.h>


#define MT8320_VDEC_IRQ          (82)

#endif
struct file *openFile(char *path,int flag,int mode);
int readFileOffset(struct file *fp,char *buf,int readlen, UINT32 u4Offset );
int readFileSize(struct file *fp,char *buf,int readlen ) ;
int readFile(struct file *fp,char *buf,int readlen );
int closeFile(struct file *fp);
void initKernelEnv(void); 



