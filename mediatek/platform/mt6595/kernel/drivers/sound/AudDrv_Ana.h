/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/*******************************************************************************
 *
 * Filename:
 * ---------
 *   AudDrv_Ana.h
 *
 * Project:
 * --------
 *   MT6583  Audio Driver Ana
 *
 * Description:
 * ------------
 *   Audio register
 *
 * Author:
 * -------
 *   Chipeng Chang (mtk02308)
 *
 *------------------------------------------------------------------------------
 * $Revision: #1 $
 * $Modtime:$
 * $Log:$
 *
 *
 *******************************************************************************/

#ifndef _AUDDRV_ANA_H_
#define _AUDDRV_ANA_H_

/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#include "AudDrv_Common.h"
#include "AudDrv_Def.h"


/*****************************************************************************
 *                         D A T A   T Y P E S
 *****************************************************************************/


/*****************************************************************************
 *                         M A C R O
 *****************************************************************************/

/*****************************************************************************
 *                  R E G I S T E R       D E F I N I T I O N
 *****************************************************************************/

#define PMIC_ABB_AFE_REG_BASE (0x4000)
#define ABB_AFE_CON0    (PMIC_ABB_AFE_REG_BASE+0x0000)
#define ABB_AFE_CON1    (PMIC_ABB_AFE_REG_BASE+0x0002)
#define ABB_AFE_CON2    (PMIC_ABB_AFE_REG_BASE+0x0004)
#define ABB_AFE_CON3    (PMIC_ABB_AFE_REG_BASE+0x0006)
#define ABB_AFE_CON4    (PMIC_ABB_AFE_REG_BASE+0x0008)
#define ABB_AFE_CON5    (PMIC_ABB_AFE_REG_BASE+0x000A)
#define ABB_AFE_CON6    (PMIC_ABB_AFE_REG_BASE+0x000C)
#define ABB_AFE_CON7    (PMIC_ABB_AFE_REG_BASE+0x000E)
#define ABB_AFE_CON8    (PMIC_ABB_AFE_REG_BASE+0x0010)
#define ABB_AFE_CON9    (PMIC_ABB_AFE_REG_BASE+0x0012)
#define ABB_AFE_CON10   (PMIC_ABB_AFE_REG_BASE+0x0014)
#define ABB_AFE_CON11   (PMIC_ABB_AFE_REG_BASE+0x0016)
#define ABB_AFE_STA0    (PMIC_ABB_AFE_REG_BASE+0x0018)
#define ABB_AFE_STA1    (PMIC_ABB_AFE_REG_BASE+0x001A)
#define ABB_AFE_STA2    (PMIC_ABB_AFE_REG_BASE+0x001C)
#define ABB_AFE_UP8X_FIFO_CFG0  (PMIC_ABB_AFE_REG_BASE+0x001E)
#define ABB_AFE_UP8X_FIFO_LOG_MON0  (PMIC_ABB_AFE_REG_BASE+0x0020)
#define ABB_AFE_UP8X_FIFO_LOG_MON1  (PMIC_ABB_AFE_REG_BASE+0x0022)
#define ABB_AFE_PMIC_NEWIF_CFG0 (PMIC_ABB_AFE_REG_BASE+0x0024)
#define ABB_AFE_PMIC_NEWIF_CFG1 (PMIC_ABB_AFE_REG_BASE+0x0026)
#define ABB_AFE_PMIC_NEWIF_CFG2 (PMIC_ABB_AFE_REG_BASE+0x0028)
#define ABB_AFE_PMIC_NEWIF_CFG3 (PMIC_ABB_AFE_REG_BASE+0x002A)
#define ABB_AFE_TOP_CON0    (PMIC_ABB_AFE_REG_BASE+0x002C)
#define ABB_AFE_MON_DEBUG0  (PMIC_ABB_AFE_REG_BASE+0x002E)
//---------------digital pmic  register define -------------------------------------------
#if 0//
#define AFE_PMICDIG_AUDIO_BASE        (0x4000)
#define AFE_UL_DL_CON0               (AFE_PMICDIG_AUDIO_BASE+0x0000)
#define AFE_DL_SRC2_CON0_H     (AFE_PMICDIG_AUDIO_BASE+0x0002)
#define AFE_DL_SRC2_CON0_L     (AFE_PMICDIG_AUDIO_BASE+0x0004)
#define AFE_DL_SRC2_CON1_H     (AFE_PMICDIG_AUDIO_BASE+0x0006)
#define AFE_DL_SRC2_CON1_L     (AFE_PMICDIG_AUDIO_BASE+0x0008)
#define AFE_DL_SDM_CON0           (AFE_PMICDIG_AUDIO_BASE+0x000A)
#define AFE_DL_SDM_CON1           (AFE_PMICDIG_AUDIO_BASE+0x000C)
#define AFE_UL_SRC_CON0_H       (AFE_PMICDIG_AUDIO_BASE+0x000E)
#define AFE_UL_SRC_CON0_L       (AFE_PMICDIG_AUDIO_BASE+0x0010)
#define AFE_UL_SRC_CON1_H      (AFE_PMICDIG_AUDIO_BASE+0x0012)
#define AFE_UL_SRC_CON1_L       (AFE_PMICDIG_AUDIO_BASE+0x0014)
#define AFE_PREDIS_CON0_H       (AFE_PMICDIG_AUDIO_BASE+0x0016)
#define AFE_PREDIS_CON0_L       (AFE_PMICDIG_AUDIO_BASE+0x0018)
#define AFE_PREDIS_CON1_H       (AFE_PMICDIG_AUDIO_BASE+0x001a)
#define AFE_PREDIS_CON1_L       (AFE_PMICDIG_AUDIO_BASE+0x001c)
#define ANA_AFE_I2S_CON1                  (AFE_PMICDIG_AUDIO_BASE+0x001e)
#define AFE_I2S_FIFO_UL_CFG0  (AFE_PMICDIG_AUDIO_BASE+0x0020)
#define AFE_I2S_FIFO_DL_CFG0  (AFE_PMICDIG_AUDIO_BASE+0x0022)
#define ANA_AFE_TOP_CON0                  (AFE_PMICDIG_AUDIO_BASE+0x0024)
#define ANA_AUDIO_TOP_CON0             (AFE_PMICDIG_AUDIO_BASE+0x0026)
#define AFE_UL_SRC_DEBUG       (AFE_PMICDIG_AUDIO_BASE+0x0028)
#define AFE_DL_SRC_DEBUG       (AFE_PMICDIG_AUDIO_BASE+0x002a)
#define AFE_UL_SRC_MON0        (AFE_PMICDIG_AUDIO_BASE+0x002c)
#define AFE_DL_SRC_MON0        (AFE_PMICDIG_AUDIO_BASE+0x002e)
#define AFE_DL_SDM_TEST0       (AFE_PMICDIG_AUDIO_BASE+0x0030)
#define AFE_MON_DEBUG0         (AFE_PMICDIG_AUDIO_BASE+0x0032)
#define AFUNC_AUD_CON0         (AFE_PMICDIG_AUDIO_BASE+0x0034)
#define AFUNC_AUD_CON1         (AFE_PMICDIG_AUDIO_BASE+0x0036)
#define AFUNC_AUD_CON2         (AFE_PMICDIG_AUDIO_BASE+0x0038)
#define AFUNC_AUD_CON3         (AFE_PMICDIG_AUDIO_BASE+0x003A)
#define AFUNC_AUD_CON4         (AFE_PMICDIG_AUDIO_BASE+0x003C)
#define AFUNC_AUD_MON0         (AFE_PMICDIG_AUDIO_BASE+0x003E)
#define AFUNC_AUD_MON1         (AFE_PMICDIG_AUDIO_BASE+0x0040)
#define AUDRC_TUNE_MON0        (AFE_PMICDIG_AUDIO_BASE+0x0042)
#define AFE_I2S_FIFO_MON0      (AFE_PMICDIG_AUDIO_BASE+0x0044)
#define AFE_DL_DC_COMP_CFG0    (AFE_PMICDIG_AUDIO_BASE+0x0046)
#define AFE_DL_DC_COMP_CFG1    (AFE_PMICDIG_AUDIO_BASE+0x0048)
#define AFE_DL_DC_COMP_CFG2    (AFE_PMICDIG_AUDIO_BASE+0x004a)
#define AFE_MBIST_CFG0                 (AFE_PMICDIG_AUDIO_BASE+0x004c)
#define AFE_MBIST_CFG1                 (AFE_PMICDIG_AUDIO_BASE+0x004e)
#define AFE_MBIST_CFG2                 (AFE_PMICDIG_AUDIO_BASE+0x0050)
#define AFE_I2S_FIFO_CFG0           (AFE_PMICDIG_AUDIO_BASE+0x0052)
#endif
//---------------digital pmic  register define end ---------------------------------------

//---------------analog pmic  register define start --------------------------------------
#if 0
#define AFE_PMICANA_AUDIO_BASE        (0x0)

#define CID                  (AFE_PMICANA_AUDIO_BASE + 0x100)
#define TOP_CKPDN0                  (AFE_PMICANA_AUDIO_BASE + 0x102)
#define TOP_CKPDN0_SET              (AFE_PMICANA_AUDIO_BASE + 0x104)
#define TOP_CKPDN0_CLR              (AFE_PMICANA_AUDIO_BASE + 0x106)
#define TOP_CKPDN1                  (AFE_PMICANA_AUDIO_BASE + 0x108)
#define TOP_CKPDN1_SET              (AFE_PMICANA_AUDIO_BASE + 0x10A)
#define TOP_CKPDN1_CLR              (AFE_PMICANA_AUDIO_BASE + 0x10C)
#define TOP_CKPDN2                  (AFE_PMICANA_AUDIO_BASE + 0x10E)
#define TOP_CKPDN2_SET              (AFE_PMICANA_AUDIO_BASE + 0x110)
#define TOP_CKPDN2_CLR              (AFE_PMICANA_AUDIO_BASE + 0x112)
#define TOP_CKCON1                  (AFE_PMICANA_AUDIO_BASE + 0x126)

#define SPK_CON0                    (AFE_PMICANA_AUDIO_BASE + 0x052)
#define SPK_CON1                    (AFE_PMICANA_AUDIO_BASE + 0x054)
#define SPK_CON2                    (AFE_PMICANA_AUDIO_BASE + 0x056)
#define SPK_CON6                    (AFE_PMICANA_AUDIO_BASE + 0x05E)
#define SPK_CON7                    (AFE_PMICANA_AUDIO_BASE + 0x060)
#define SPK_CON8                    (AFE_PMICANA_AUDIO_BASE + 0x062)
#define SPK_CON9                    (AFE_PMICANA_AUDIO_BASE + 0x064)
#define SPK_CON10                   (AFE_PMICANA_AUDIO_BASE + 0x066)
#define SPK_CON11                   (AFE_PMICANA_AUDIO_BASE + 0x068)
#define SPK_CON12                   (AFE_PMICANA_AUDIO_BASE + 0x06A)

#define AUDTOP_CON0                 (AFE_PMICANA_AUDIO_BASE + 0x700)
#define AUDTOP_CON1                 (AFE_PMICANA_AUDIO_BASE + 0x702)
#define AUDTOP_CON2                 (AFE_PMICANA_AUDIO_BASE + 0x704)
#define AUDTOP_CON3                 (AFE_PMICANA_AUDIO_BASE + 0x706)
#define AUDTOP_CON4                 (AFE_PMICANA_AUDIO_BASE + 0x708)
#define AUDTOP_CON5                 (AFE_PMICANA_AUDIO_BASE + 0x70A)
#define AUDTOP_CON6                 (AFE_PMICANA_AUDIO_BASE + 0x70C)
#define AUDTOP_CON7                 (AFE_PMICANA_AUDIO_BASE + 0x70E)
#define AUDTOP_CON8                 (AFE_PMICANA_AUDIO_BASE + 0x710)
#define AUDTOP_CON9                 (AFE_PMICANA_AUDIO_BASE + 0x712)
#else
#include <mach/upmu_hw.h>
#endif
void Ana_Set_Reg(uint32 offset, uint32 value, uint32 mask);
uint32  Ana_Get_Reg(uint32 offset);

// for debug usage
void Ana_Log_Print(void);

#endif

