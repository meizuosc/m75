#ifndef __MTK_HAL_MM_H__
#define __MTK_HAL_MM_H__

#ifdef MTK_HAL_MM_STATISTIC
#include <linux/sched.h>

#include "img_types.h"

#include <linux/string.h>
#include "pvr_debug.h"
//#include "mtk_debug.h"
//#include "servicesext.h"
//#include "mutex.h"
//#include "services.h"
//#include "osfunc.h"

extern atomic_t g_MtkSysRAMUseInByte_atomic;
static inline IMG_VOID MTKSysRAMInc(IMG_UINT32 uiByte)
{
    //PVR_DPF((PVR_DBG_ERROR,"MTKSysRAMInc ++: byte: %d",(unsigned int)uiByte));
    atomic_add (uiByte, &g_MtkSysRAMUseInByte_atomic);    
}
static inline IMG_VOID MTKSysRAMDec(IMG_UINT32 uiByte)
{
    // PVR_DPF((PVR_DBG_ERROR,"MTKSysRAMDec --: byte: %d",(unsigned int)uiByte));
    atomic_sub (uiByte, &g_MtkSysRAMUseInByte_atomic);   
}

static inline IMG_UINT32 MTKGetSysRAMStats(IMG_VOID)
{
    //PVR_DPF((PVR_DBG_ERROR,"MTKGetSysRAMStats: total: %d",(unsigned int)atomic_read (&g_MtkSysRAMUseInByte_atomic)));
    return ((unsigned int)atomic_read (&g_MtkSysRAMUseInByte_atomic));
}

IMG_VOID MTKGpuHalMMInit(IMG_VOID);

#endif

#endif
