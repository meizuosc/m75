#include <asm/page.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/mm.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <linux/hrtimer.h>

#include "core/met_drv.h"
#include "core/trace.h"

#include "mt_gpu_metmonitor.h"
#include "plf_trace.h"

//define if the hal implementation might re-schedule, cannot run inside softirq
//undefine this is better for sampling jitter if HAL support it
#define GPU_HAL_RUN_PREMPTIBLE

#ifdef GPU_HAL_RUN_PREMPTIBLE
static struct delayed_work gpu_dwork;
static struct delayed_work gpu_pwr_dwork;
#endif
//extern struct metdevice met_gpu;

/*
    GPU monitor HAL comes from alps\mediatek\kernel\include\linux\mtk_gpu_utility.h

    mtk_get_gpu_memory_usage(unsigned int* pMemUsage) in unit of bytes

    mtk_get_gpu_xxx_loading are in unit of %
*/
extern bool mtk_get_gpu_memory_usage(unsigned int* pMemUsage);

extern bool mtk_get_gpu_loading(unsigned int* pLoading);

extern bool mtk_get_gpu_GP_loading(unsigned int* pLoading);

extern bool mtk_get_gpu_PP_loading(unsigned int* pLoading);

extern bool mtk_get_gpu_power_loading(unsigned int* pLoading);

enum MET_GPU_PROFILE_INDEX
{
    eMET_GPU_LOADING = 0,
    eMET_GPU_GP_LOADING,   // 1
    eMET_GPU_PP_LOADING,   // 2
    eMET_GPU_PROFILE_CNT
};

static unsigned long g_u4AvailableInfo = 0;

noinline static void GPULoading(unsigned char cnt, unsigned int *value)
{
    switch (cnt)
    {
        case 1: 
            MET_PRINTK("%u\n", value[0]);
        break;

        case 2:
            MET_PRINTK("%u,%u\n", value[0] , value[1]);
        break;

        case 3:
            MET_PRINTK("%u,%u,%u\n", value[0] , value[1] , value[2]);
        break;

        case 4:
            MET_PRINTK("%u,%u,%u,%u\n", value[0] , value[1] , value[2] , value[3]);
        break;

        default: 
        break;
    }

}

#ifdef GPU_HAL_RUN_PREMPTIBLE
static void GPULoading_OUTTER(struct work_struct *work)
{
    unsigned int pu4Value[eMET_GPU_PROFILE_CNT];
    unsigned long u4Index = 0;

    if((1 << eMET_GPU_LOADING) & g_u4AvailableInfo)
    {
        mtk_get_gpu_loading(&pu4Value[u4Index]);
        u4Index += 1;
    }

    if((1 << eMET_GPU_GP_LOADING) & g_u4AvailableInfo)
    {
        mtk_get_gpu_GP_loading(&pu4Value[u4Index]);
        u4Index += 1;
    }

    if((1 << eMET_GPU_PP_LOADING) & g_u4AvailableInfo)
    {
        mtk_get_gpu_PP_loading(&pu4Value[u4Index]);
        u4Index += 1;
    }

    if(g_u4AvailableInfo)
    {
        GPULoading(u4Index , pu4Value);
    }
}

#else

static void GPULoading_OUTTER(unsigned long long stamp, int cpu)
{
    unsigned int pu4Value[eMET_GPU_PROFILE_CNT];
    unsigned long u4Index = 0;

    if((1 << eMET_GPU_LOADING) & g_u4AvailableInfo)
    {
        mtk_get_gpu_loading(&pu4Value[u4Index]);
        u4Index += 1;
    }

    if((1 << eMET_GPU_GP_LOADING) & g_u4AvailableInfo)
    {
        mtk_get_gpu_GP_loading(&pu4Value[u4Index]);
        u4Index += 1;
    }

    if((1 << eMET_GPU_PP_LOADING) & g_u4AvailableInfo)
    {
        mtk_get_gpu_PP_loading(&pu4Value[u4Index]);
        u4Index += 1;
    }

    if((1 << eMET_GPU_PW_LOADING) & g_u4AvailableInfo)
    {
        mtk_get_gpu_power_loading(&pu4Value[u4Index]);
        u4Index += 1;
    }

    if(g_u4AvailableInfo)
    {
        GPULoading(u4Index , pu4Value);
    }
}
#endif

static void gpu_monitor_start(void)
{
    //Check what information provided now
    unsigned int u4Value = 0;
    if(0 == g_u4AvailableInfo)
    {
        if(mtk_get_gpu_loading(&u4Value))
        {
            g_u4AvailableInfo |= (1 << eMET_GPU_LOADING);
        }

        if(mtk_get_gpu_GP_loading(&u4Value))
        {
            g_u4AvailableInfo |= (1 << eMET_GPU_GP_LOADING);
        }

        if(mtk_get_gpu_PP_loading(&u4Value))
        {
            g_u4AvailableInfo |= (1 << eMET_GPU_PP_LOADING);
        }
    }

#ifdef GPU_HAL_RUN_PREMPTIBLE
    INIT_DELAYED_WORK(&gpu_dwork , GPULoading_OUTTER);
#endif

    return;
}

#ifdef GPU_HAL_RUN_PREMPTIBLE
static void gpu_monitor_stop(void)
{
    cancel_delayed_work_sync(&gpu_dwork);
}

static void GPULoadingNotify(unsigned long long stamp, int cpu)
{
    schedule_delayed_work(&gpu_dwork, 0);
}
#endif

static char help[] = "  --gpu monitor                             monitor gpu status\n";
static int gpu_status_print_help(char *buf, int len)
{
    return snprintf(buf, PAGE_SIZE, help);
}

static char g_pComGPUStatusHeader[] =
"met-info [000] 0.0: ms_ud_sys_header: GPULoading,";
static int gpu_status_print_header(char *buf, int len)
{
    char buffer[256];
    unsigned long u4Cnt = 0;
    unsigned long u4Index = 0;

    strcpy(buffer , g_pComGPUStatusHeader);
    if((1 << eMET_GPU_LOADING) & g_u4AvailableInfo)
    {
        strcat(buffer , "Loading,");
        u4Cnt += 1;
    }

    if((1 << eMET_GPU_GP_LOADING) & g_u4AvailableInfo)
    {
        strcat(buffer , "GP Loading,");
        u4Cnt += 1;
    }

    if((1 << eMET_GPU_PP_LOADING) & g_u4AvailableInfo)
    {
        strcat(buffer , "PP Loading,");
        u4Cnt += 1;
    }

    for(u4Index = 0 ; u4Index < u4Cnt ; u4Index += 1)
    {
        strcat(buffer , "d");
        if((u4Index + 1) != u4Cnt)
        {
            strcat(buffer , ",");
        }
    }

    strcat(buffer , "\n");

    return snprintf(buf, PAGE_SIZE, buffer);
}

struct metdevice met_gpu = {
    .name = "gpu",
    .owner = THIS_MODULE,
    .type = MET_TYPE_BUS,
    .cpu_related = 0,
    .start = gpu_monitor_start,
    .mode = 0,
    .polling_interval = 1,//ms
#ifdef GPU_HAL_RUN_PREMPTIBLE
    .timed_polling = GPULoadingNotify,
    .stop = gpu_monitor_stop,
#else
    .timed_polling = GPULoading_OUTTER,
#endif
    .print_help = gpu_status_print_help,
    .print_header = gpu_status_print_header,
};

//GPU MEM monitor
static unsigned long g_u4MemProfileIsOn = 0;
static void gpu_mem_monitor_start(void)
{
    //Check what information provided now
    unsigned int u4Value = 0;
    if(mtk_get_gpu_memory_usage(&u4Value))
    {
        g_u4MemProfileIsOn = 1;
    }
}

noinline static void GPU_MEM(unsigned long long stamp, int cpu)
{
    unsigned int u4Value;

    if(1 == g_u4MemProfileIsOn)
    {
        mtk_get_gpu_memory_usage(&u4Value);
        MET_PRINTK("%d\n" , u4Value);
    }

}

static void gpu_mem_monitor_stop(void)
{
    g_u4MemProfileIsOn = 0;
}

static char g_pComGPUMemHeader[] =
"met-info [000] 0.0: ms_ud_sys_header: GPU_MEM,GPU_MEM,d\n";
static int gpu_mem_status_print_header(char *buf, int len)
{
    return snprintf(buf, PAGE_SIZE, g_pComGPUMemHeader);
}

struct metdevice met_gpumem = {
    .name = "gpu-mem",
    .owner = THIS_MODULE,
    .type = MET_TYPE_BUS,
    .cpu_related = 0,
    .start = gpu_mem_monitor_start,
    .stop = gpu_mem_monitor_stop,
    .mode = 0,
    .polling_interval = 10,//ms
    .timed_polling = GPU_MEM,
    .print_help = gpu_status_print_help,
    .print_header = gpu_mem_status_print_header,
};

//GPU power monitor
static unsigned long g_u4PowerProfileIsOn = 0;

#ifdef GPU_HAL_RUN_PREMPTIBLE
noinline static void GPU_Power(struct work_struct *work)
{
    unsigned int u4Value;

    mtk_get_gpu_power_loading(&u4Value);
    MET_PRINTK("%d\n" , u4Value);

}

static void GPU_PowerNotify(unsigned long long stamp, int cpu)
{
    if(1 == g_u4PowerProfileIsOn)
    {
        schedule_delayed_work(&gpu_pwr_dwork, 0);
    }
}
#else
noinline static void GPU_Power(unsigned long long stamp, int cpu)
{
    unsigned int u4Value;

    if(1 == g_u4PowerProfileIsOn)
    {
        mtk_get_gpu_power_loading(&u4Value);
        MET_PRINTK("%d\n" , u4Value);
    }

}
#endif

static void gpu_Power_monitor_start(void)
{
    //Check what information provided now
    unsigned int u4Value = 0;

    if(mtk_get_gpu_power_loading(&u4Value))
    {
        g_u4PowerProfileIsOn = 1;
    }

#ifdef GPU_HAL_RUN_PREMPTIBLE
    INIT_DELAYED_WORK(&gpu_pwr_dwork , GPU_Power);
#endif
}

static void gpu_Power_monitor_stop(void)
{
    g_u4PowerProfileIsOn = 0;

#ifdef GPU_HAL_RUN_PREMPTIBLE
    cancel_delayed_work_sync(&gpu_pwr_dwork);
#endif
}

static char g_pComGPUPowerHeader[] =
"met-info [000] 0.0: ms_ud_sys_header: GPU_Power,GPU_Power,d\n";
static int gpu_Power_status_print_header(char *buf, int len)
{
    return snprintf(buf, PAGE_SIZE, g_pComGPUPowerHeader);
}

struct metdevice met_gpupwr = {
    .name = "gpu-pwr",
    .owner = THIS_MODULE,
    .type = MET_TYPE_BUS,
    .cpu_related = 0,
    .start = gpu_Power_monitor_start,
    .stop = gpu_Power_monitor_stop,
    .mode = 0,
    .polling_interval = 10,//ms
#ifdef GPU_HAL_RUN_PREMPTIBLE
    .timed_polling = GPU_PowerNotify,
#else
    .timed_polling = GPU_Power,
#endif
    .print_help = gpu_status_print_help,
    .print_header = gpu_Power_status_print_header,
};


