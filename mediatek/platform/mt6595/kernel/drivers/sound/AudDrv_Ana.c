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
 *   AudDrv_Ana.c
 *
 * Project:
 * --------
 *   MT6583  Audio Driver ana Register setting
 *
 * Description:
 * ------------
 *   Audio register
 *
 * Author:
 * -------
 * Chipeng Chang
 *
 *------------------------------------------------------------------------------
 * $Revision: #1 $
 * $Modtime:$
 * $Log:$
 *
 *
 *******************************************************************************/


/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#include "AudDrv_Common.h"
#include "AudDrv_Ana.h"
#include "AudDrv_Clk.h"

// define this to use wrapper to control
#define AUDIO_USING_WRAP_DRIVER
#ifdef AUDIO_USING_WRAP_DRIVER
#include <mach/mt_pmic_wrap.h>
#endif

/*****************************************************************************
 *                         D A T A   T Y P E S
 *****************************************************************************/

void Ana_Set_Reg(uint32 offset, uint32 value, uint32 mask)
{
    // set pmic register or analog CONTROL_IFACE_PATH
    int ret = 0;
#ifdef AUDIO_USING_WRAP_DRIVER
    uint32 Reg_Value = Ana_Get_Reg(offset);
    Reg_Value &= (~mask);
    Reg_Value |= (value & mask);
    ret = pwrap_write(offset, Reg_Value);
    Reg_Value = Ana_Get_Reg(offset);
    if ((Reg_Value & mask) != (value & mask))
    {
        printk("Ana_Set_Reg offset= 0x%x , value = 0x%x mask = 0x%x ret = %d Reg_Value = 0x%x\n", offset, value, mask, ret, Reg_Value);
    }
#endif
}

uint32 Ana_Get_Reg(uint32 offset)
{
    // get pmic register
    int ret = 0;
    uint32 Rdata = 0;
#ifdef AUDIO_USING_WRAP_DRIVER
    ret = pwrap_read(offset, &Rdata);
#endif
    //PRINTK_ANA_REG ("Ana_Get_Reg offset= 0x%x  Rdata = 0x%x ret = %d\n",offset,Rdata,ret);
    return Rdata;
}

void Ana_Log_Print(void)
{
    AudDrv_ANA_Clk_On();
    printk("ABB_AFE_CON0	= 0x%x\n", Ana_Get_Reg(ABB_AFE_CON0));
    printk("ABB_AFE_CON1	= 0x%x\n", Ana_Get_Reg(ABB_AFE_CON1));
    printk("ABB_AFE_CON2	= 0x%x\n", Ana_Get_Reg(ABB_AFE_CON2));
    printk("ABB_AFE_CON3	= 0x%x\n", Ana_Get_Reg(ABB_AFE_CON3));
    printk("ABB_AFE_CON4	= 0x%x\n", Ana_Get_Reg(ABB_AFE_CON4));
    printk("ABB_AFE_CON5  = 0x%x\n", Ana_Get_Reg(ABB_AFE_CON5));
    printk("ABB_AFE_CON6  = 0x%x\n", Ana_Get_Reg(ABB_AFE_CON6));
    printk("ABB_AFE_CON7  = 0x%x\n", Ana_Get_Reg(ABB_AFE_CON7));
    printk("ABB_AFE_CON8  = 0x%x\n", Ana_Get_Reg(ABB_AFE_CON8));
    printk("ABB_AFE_CON9  = 0x%x\n", Ana_Get_Reg(ABB_AFE_CON9));
    printk("ABB_AFE_CON10  = 0x%x\n", Ana_Get_Reg(ABB_AFE_CON10));
    printk("ABB_AFE_CON11  = 0x%x\n", Ana_Get_Reg(ABB_AFE_CON11));

    printk("ABB_AFE_STA0  = 0x%x\n", Ana_Get_Reg(ABB_AFE_STA0));
    printk("ABB_AFE_STA1  = 0x%x\n", Ana_Get_Reg(ABB_AFE_STA1));
    printk("ABB_AFE_STA2  = 0x%x\n", Ana_Get_Reg(ABB_AFE_STA2));

    printk("ABB_AFE_UP8X_FIFO_CFG0	= 0x%x\n", Ana_Get_Reg(ABB_AFE_UP8X_FIFO_CFG0));
    printk("ABB_AFE_UP8X_FIFO_LOG_MON0	= 0x%x\n", Ana_Get_Reg(ABB_AFE_UP8X_FIFO_LOG_MON0));
    printk("ABB_AFE_UP8X_FIFO_LOG_MON1	= 0x%x\n", Ana_Get_Reg(ABB_AFE_UP8X_FIFO_LOG_MON1));
    printk("ABB_AFE_PMIC_NEWIF_CFG0	= 0x%x\n", Ana_Get_Reg(ABB_AFE_PMIC_NEWIF_CFG0));
    printk("ABB_AFE_PMIC_NEWIF_CFG1	= 0x%x\n", Ana_Get_Reg(ABB_AFE_PMIC_NEWIF_CFG1));
    printk("ABB_AFE_PMIC_NEWIF_CFG2  = 0x%x\n", Ana_Get_Reg(ABB_AFE_PMIC_NEWIF_CFG2));
    printk("ABB_AFE_PMIC_NEWIF_CFG3  = 0x%x\n", Ana_Get_Reg(ABB_AFE_PMIC_NEWIF_CFG3));
    printk("ABB_AFE_TOP_CON0  = 0x%x\n", Ana_Get_Reg(ABB_AFE_TOP_CON0));
    printk("ABB_AFE_MON_DEBUG0  = 0x%x\n", Ana_Get_Reg(ABB_AFE_MON_DEBUG0));


    printk("SPK_CON0  = 0x%x\n", Ana_Get_Reg(SPK_CON0));
    printk("SPK_CON1  = 0x%x\n", Ana_Get_Reg(SPK_CON1));
    printk("SPK_CON2  = 0x%x\n", Ana_Get_Reg(SPK_CON2));
    printk("SPK_CON6  = 0x%x\n", Ana_Get_Reg(SPK_CON6));
    printk("SPK_CON7  = 0x%x\n", Ana_Get_Reg(SPK_CON7));
    printk("SPK_CON8  = 0x%x\n", Ana_Get_Reg(SPK_CON8));
    printk("SPK_CON9  = 0x%x\n", Ana_Get_Reg(SPK_CON9));
    printk("SPK_CON10  = 0x%x\n", Ana_Get_Reg(SPK_CON10));
    printk("SPK_CON11  = 0x%x\n", Ana_Get_Reg(SPK_CON11));
    printk("SPK_CON12  = 0x%x\n", Ana_Get_Reg(SPK_CON12));

    printk("CID	= 0x%x\n", Ana_Get_Reg(CID));
    printk("TOP_CKPDN0  = 0x%x\n", Ana_Get_Reg(TOP_CKPDN0));
    printk("TOP_CKPDN0_SET  = 0x%x\n", Ana_Get_Reg(TOP_CKPDN0_SET));
    printk("TOP_CKPDN0_CLR  = 0x%x\n", Ana_Get_Reg(TOP_CKPDN0_CLR));
    printk("TOP_CKPDN1  = 0x%x\n", Ana_Get_Reg(TOP_CKPDN1));
    printk("TOP_CKPDN1_SET  = 0x%x\n", Ana_Get_Reg(TOP_CKPDN1_SET));
    printk("TOP_CKPDN1_CLR  = 0x%x\n", Ana_Get_Reg(TOP_CKPDN1_CLR));
    printk("TOP_CKPDN2  = 0x%x\n", Ana_Get_Reg(TOP_CKPDN2));
    printk("TOP_CKPDN2_SET  = 0x%x\n", Ana_Get_Reg(TOP_CKPDN2_SET));
    printk("TOP_CKPDN2_CLR  = 0x%x\n", Ana_Get_Reg(TOP_CKPDN2_CLR));
    printk("TOP_CKCON1  = 0x%x\n", Ana_Get_Reg(TOP_CKCON1));

    printk("AUDTOP_CON0  = 0x%x\n", Ana_Get_Reg(AUDTOP_CON0));
    printk("AUDTOP_CON1  = 0x%x\n", Ana_Get_Reg(AUDTOP_CON1));
    printk("AUDTOP_CON2  = 0x%x\n", Ana_Get_Reg(AUDTOP_CON2));
    printk("AUDTOP_CON3  = 0x%x\n", Ana_Get_Reg(AUDTOP_CON3));
    printk("AUDTOP_CON4  = 0x%x\n", Ana_Get_Reg(AUDTOP_CON4));
    printk("AUDTOP_CON5  = 0x%x\n", Ana_Get_Reg(AUDTOP_CON5));
    printk("AUDTOP_CON6  = 0x%x\n", Ana_Get_Reg(AUDTOP_CON6));
    printk("AUDTOP_CON7  = 0x%x\n", Ana_Get_Reg(AUDTOP_CON7));
    printk("AUDTOP_CON8  = 0x%x\n", Ana_Get_Reg(AUDTOP_CON8));
    printk("AUDTOP_CON9  = 0x%x\n", Ana_Get_Reg(AUDTOP_CON9));
    AudDrv_ANA_Clk_Off();
    printk("-Ana_Log_Print \n");
}


// export symbols for other module using
EXPORT_SYMBOL(Ana_Log_Print);
EXPORT_SYMBOL(Ana_Set_Reg);
EXPORT_SYMBOL(Ana_Get_Reg);


