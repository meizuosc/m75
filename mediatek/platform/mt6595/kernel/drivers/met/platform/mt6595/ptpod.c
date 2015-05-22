#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/random.h>
#include <linux/fs.h>

#include "core/met_drv.h"
#include "core/trace.h"

//#ifdef MET_BUILD_IN
    //#include "core/mt_ptp.h"
    #include "mach/mt_cpufreq.h"
    #include "mach/mt_gpufreq.h"
//#else
#if 0
    //Cautions !! Got to check below data structure to fit mt_ptp.h
    enum mt_cpu_dvfs_id {

    #ifdef MTK_FORCE_CLUSTER1
  	MT_CPU_DVFS_BIG,
  	MT_CPU_DVFS_LITTLE,
    #else
	MT_CPU_DVFS_LITTLE,
	MT_CPU_DVFS_BIG,
    #endif

	NR_MT_CPU_DVFS,
    };
    extern unsigned int mt_cpufreq_cur_vproc(enum mt_cpu_dvfs_id id);//CPU voltage

    extern unsigned int mt_gpufreq_get_cur_freq(void);//GPU freq

    extern unsigned int mt_gpufreq_get_cur_volt(void);//GPU voltage

    extern void mt_cpufreq_setvolt_registerCB(cpuVoltsampler_func pCB);

    extern void mt_gpufreq_setvolt_registerCB(sampler_func pCB);

    extern void mt_gpufreq_setfreq_registerCB(sampler_func pCB);
#endif

enum MET_PTPOD_DEVICE {
    MET_CPU_BIG,
    MET_CPU_LITTLE,
    MET_GPU
};

extern struct metdevice met_ptpod;

//FIXME : it is a workaround to generate rectangular waveform, it generates double information , fix it after fronend start to support
static unsigned int g_u4BigCPUVolt = 0;
static unsigned int g_u4LittleCPUVolt = 0;
static unsigned int g_u4GPUVolt = 0;
noinline void ptpod(enum MET_PTPOD_DEVICE id , unsigned int a_u4VoltInmv)
{


    MET_PRINTK("%u,%u,%u\n" , g_u4BigCPUVolt , g_u4LittleCPUVolt , g_u4GPUVolt);

    switch(id)
    {
        case MET_CPU_BIG :
            g_u4BigCPUVolt = a_u4VoltInmv;
        break;

        case MET_CPU_LITTLE :
            g_u4LittleCPUVolt = a_u4VoltInmv;
        break;

        case MET_GPU :
            g_u4GPUVolt = a_u4VoltInmv;
        break;
        default :
        break;
    }

    MET_PRINTK("%u,%u,%u\n" , g_u4BigCPUVolt , g_u4LittleCPUVolt , g_u4GPUVolt);
}

static void ptpod_cpu_voltSampler(enum mt_cpu_dvfs_id id , unsigned int a_u4LittleVolt)
{
    switch(id)
    {
        case MT_CPU_DVFS_BIG :
            ptpod(MET_CPU_BIG , a_u4LittleVolt);
        break;

        case MT_CPU_DVFS_LITTLE :
            ptpod(MET_CPU_LITTLE , a_u4LittleVolt);
        break;

        default :
        break;
    }
}

static void ptpod_gpu_voltSampler(unsigned int a_u4Volt)
{
    ptpod(MET_GPU , ((a_u4Volt + 50)/100));
}
/*
 * Called from "met-cmd --start"
 */
static void ptpod_start(void)
{
    g_u4BigCPUVolt = mt_cpufreq_cur_vproc(MT_CPU_DVFS_BIG);

    g_u4LittleCPUVolt = mt_cpufreq_cur_vproc(MT_CPU_DVFS_LITTLE);

    g_u4GPUVolt = ((mt_gpufreq_get_cur_volt() + 50)/100);

    ptpod(MET_CPU_BIG , g_u4BigCPUVolt);

    ptpod(MET_CPU_LITTLE , g_u4LittleCPUVolt);

    ptpod(MET_GPU , g_u4GPUVolt);

    mt_gpufreq_setvolt_registerCB(ptpod_gpu_voltSampler);

    mt_cpufreq_setvolt_registerCB(ptpod_cpu_voltSampler);
}

/*
 * Called from "met-cmd --stop"
 */
static void ptpod_stop(void)
{
    //TODO : UnRegister callback
    mt_cpufreq_setvolt_registerCB(NULL);

    mt_gpufreq_setvolt_registerCB(NULL);

    ptpod(MET_CPU_BIG , mt_cpufreq_cur_vproc(MT_CPU_DVFS_BIG));

    ptpod(MET_CPU_LITTLE , mt_cpufreq_cur_vproc(MT_CPU_DVFS_LITTLE));

    ptpod(MET_GPU , ((mt_gpufreq_get_cur_volt() + 50)/100));

    return;
}

static char help[] = "  --ptpod                               Measure CPU/GPU voltage&freq\n";
static int ptpod_print_help(char *buf, int len)
{
	return snprintf(buf, PAGE_SIZE, help);
}

/*
 * It will be called back when run "met-cmd --extract" and mode is 1
 */
static char header[] = "met-info [000] 0.0: ms_ud_sys_header: ptpod,bigCPUVolt,LittleCPUVolt,GPUVolt,d,d,d\n";
static int ptpod_print_header(char *buf, int len)
{
	return snprintf(buf, PAGE_SIZE, header);
}

struct metdevice met_ptpod = {
	.name = "ptpod",
	.owner = THIS_MODULE,
	.type = MET_TYPE_BUS,
	.cpu_related = 0,
	.start = ptpod_start,
	.stop = ptpod_stop,
	.print_help = ptpod_print_help,
	.print_header = ptpod_print_header,
};
EXPORT_SYMBOL(met_ptpod);

//FIXME : it is a workaround to generate rectangular waveform, it generates double information , fix it after fronend start to support
static unsigned int g_u4GPUFreq = 0;
noinline static void GPUDVFS(unsigned int a_u4Freq)
{
    static unsigned int g_u4GPUThermalLimitFreq = 0;

    MET_PRINTK("%u,%u\n" , g_u4GPUFreq , g_u4GPUThermalLimitFreq);

    g_u4GPUFreq = a_u4Freq;

    g_u4GPUThermalLimitFreq = mt_gpufreq_get_thermal_limit_freq();

    MET_PRINTK("%u,%u\n" , a_u4Freq , g_u4GPUThermalLimitFreq);
}

static void gpu_dvfs_monitor_start(void)
{
    g_u4GPUFreq = mt_gpufreq_get_cur_freq();

    GPUDVFS(g_u4GPUFreq);

    mt_gpufreq_setfreq_registerCB(GPUDVFS);
}

static void gpu_dvfs_monitor_stop(void)
{
    mt_gpufreq_setfreq_registerCB(NULL);

    GPUDVFS(mt_gpufreq_get_cur_freq());
}

static char gpu_dvfs_help[] = "  --gpu_dvfs                               Measure GPU freq\n";
static int ptpod_gpudvfs_print_help(char *buf, int len)
{
	return snprintf(buf, PAGE_SIZE, gpu_dvfs_help);
}

/*
 * It will be called back when run "met-cmd --extract" and mode is 1
 */
static char gpu_dvfs_header[] = "met-info [000] 0.0: ms_ud_sys_header: GPUDVFS,freq(Hz),ThermalLimit,d,d\n";
static int ptpod_gpudvfs_print_header(char *buf, int len)
{
	return snprintf(buf, PAGE_SIZE, gpu_dvfs_header);
}

struct metdevice met_gpudvfs = {
    .name = "GPU-DVFS",
    .owner = THIS_MODULE,
    .type = MET_TYPE_BUS,
    .cpu_related = 0,
    .start = gpu_dvfs_monitor_start,
    .stop = gpu_dvfs_monitor_stop,
    .print_help = ptpod_gpudvfs_print_help,
    .print_header = ptpod_gpudvfs_print_header,
};
EXPORT_SYMBOL(met_gpudvfs);

