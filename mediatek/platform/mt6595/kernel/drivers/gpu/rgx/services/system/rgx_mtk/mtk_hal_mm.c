#include <linux/sched.h>
#include "mtk_hal_mm.h"

#ifdef MTK_HAL_MM_STATISTIC

atomic_t g_MtkSysRAMUseInByte_atomic = ATOMIC_INIT(0);


extern unsigned int (*mtk_get_gpu_memory_usage_fp)(void);

static IMG_UINT32  MTKGetGpuMemoryStatics(IMG_VOID)
{

//#if defined(PVRSRV_ENABLE_MEMORY_STATS)
    return MTKGetSysRAMStats();
//#else
//    return 0;
//#endif    
    
}

IMG_VOID MTKGpuHalMMInit(IMG_VOID)
{
   // g_psSGXDevNode = MTKGetDevNode(PVRSRV_DEVICE_TYPE_SGX);

    mtk_get_gpu_memory_usage_fp = MTKGetGpuMemoryStatics;   
}

#endif

