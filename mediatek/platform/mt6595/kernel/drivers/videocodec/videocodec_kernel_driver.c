#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/mm_types.h>
#include <linux/mm.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <asm/page.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <mach/irqs.h>
#include <mach/x_define_irq.h>
#include <linux/wait.h>
#include <linux/proc_fs.h>
#include <linux/semaphore.h>
#include <linux/android_pmem.h>
#include <mach/dma.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <mach/mt_idle.h>
#include "mach/sync_write.h"
#include "mach/mt_reg_base.h"
#include "mach/mt_clkmgr.h"
#ifdef CONFIG_MTK_HIBERNATION
#include "mach/mtk_hibernate_dpm.h"
#endif

#include "videocodec_kernel_driver.h"

#include <asm/cacheflush.h>
#include <asm/io.h>
#include <asm/sizes.h>
#include "val_types_private.h"
#include "hal_types_private.h"
#include "val_api_private.h"
#include "val_log.h"
#include "drv_api.h"

//#define MFV_LOGD MFV_LOGE

#define VDO_HW_WRITE(ptr,data)     mt_reg_sync_writel(data,ptr)
#define VDO_HW_READ(ptr)           (*((volatile unsigned int * const)(ptr)))

#define VCODEC_DEVNAME     "Vcodec"
#define MT6582_VCODEC_DEV_MAJOR_NUMBER 160   //189
//#define VDEC_USE_L2C

static dev_t vcodec_devno = MKDEV(MT6582_VCODEC_DEV_MAJOR_NUMBER,0);
static struct cdev *vcodec_cdev;

static DEFINE_MUTEX(IsOpenedLock);
static DEFINE_MUTEX(PWRLock);
static DEFINE_MUTEX(VdecHWLock);
static DEFINE_MUTEX(VencHWLock);
static DEFINE_MUTEX(EncEMILock);
static DEFINE_MUTEX(L2CLock);
static DEFINE_MUTEX(DecEMILock);
static DEFINE_MUTEX(DriverOpenCountLock);
static DEFINE_MUTEX(DecHWLockEventTimeoutLock);
static DEFINE_MUTEX(EncHWLockEventTimeoutLock);

static DEFINE_MUTEX(VdecPWRLock);
static DEFINE_MUTEX(VencPWRLock);

static DEFINE_SPINLOCK(DecIsrLock);
static DEFINE_SPINLOCK(EncIsrLock);
static DEFINE_SPINLOCK(LockDecHWCountLock);
static DEFINE_SPINLOCK(LockEncHWCountLock);
static DEFINE_SPINLOCK(DecISRCountLock);
static DEFINE_SPINLOCK(EncISRCountLock);


static VAL_EVENT_T DecHWLockEvent;    //mutex : HWLockEventTimeoutLock
static VAL_EVENT_T EncHWLockEvent;    //mutex : HWLockEventTimeoutLock
static VAL_EVENT_T DecIsrEvent;    //mutex : HWLockEventTimeoutLock
static VAL_EVENT_T EncIsrEvent;    //mutex : HWLockEventTimeoutLock
static int MT6582Driver_Open_Count;         //mutex : DriverOpenCountLock
static VAL_UINT32_T gu4PWRCounter = 0;      //mutex : PWRLock
static VAL_UINT32_T gu4EncEMICounter = 0;   //mutex : EncEMILock
static VAL_UINT32_T gu4DecEMICounter = 0;   //mutex : DecEMILock
static VAL_UINT32_T gu4L2CCounter = 0;      //mutex : L2CLock 
static VAL_BOOL_T bIsOpened = VAL_FALSE;    //mutex : IsOpenedLock
static VAL_UINT32_T gu4HwVencIrqStatus = 0; //hardware VENC IRQ status (VP8/H264)

static VAL_UINT32_T gu4VdecPWRCounter = 0;  //mutex : VdecPWRLock
static VAL_UINT32_T gu4VencPWRCounter = 0;  //mutex : VencPWRLock

static VAL_UINT32_T gLockTimeOutCount = 0;

static VAL_UINT32_T gu4VdecLockThreadId = 0;

//#define MT6582_VCODEC_DEBUG
#ifdef MT6582_VCODEC_DEBUG
#undef VCODEC_DEBUG
#define VCODEC_DEBUG MFV_LOGE
#undef MFV_LOGD
#define MFV_LOGD  MFV_LOGE
#else
#define VCODEC_DEBUG(...)
#undef MFV_LOGD
#define MFV_LOGD(...)
#endif

// VENC physical base address
#undef VENC_BASE
#define VENC_BASE       0x18002000
#define VENC_REGION     0x1000
#define VENC_IRQ_STATUS_addr        VENC_BASE + 0x05C
#define VENC_IRQ_ACK_addr           VENC_BASE + 0x060
#define VENC_MP4_IRQ_ACK_addr       VENC_BASE + 0x678
#define VENC_MP4_IRQ_STATUS_addr    VENC_BASE + 0x67C
#define VENC_ZERO_COEF_COUNT_addr   VENC_BASE + 0x688
#define VENC_BYTE_COUNT_addr        VENC_BASE + 0x680
#define VENC_MP4_IRQ_ENABLE_addr    VENC_BASE + 0x668

#define VENC_MP4_STATUS_addr        VENC_BASE + 0x664
#define VENC_MP4_MVQP_STATUS_addr   VENC_BASE + 0x6E4

#define VENC_IRQ_STATUS_SPS         0x1
#define VENC_IRQ_STATUS_PPS         0x2
#define VENC_IRQ_STATUS_FRM         0x4
#define VENC_IRQ_STATUS_DRAM        0x8
#define VENC_IRQ_STATUS_PAUSE       0x10
#define VENC_IRQ_STATUS_SWITCH      0x20
#define VENC_IRQ_STATUS_VPS         0x80


//#define VENC_PWR_FPGA
// Cheng-Jung 20120621 VENC power physical base address (FPGA only, should use API) [
#ifdef VENC_PWR_FPGA
#define CLK_CFG_0_addr      0x10000140
#define CLK_CFG_4_addr      0x10000150
#define VENC_PWR_addr       0x10006230
#define VENCSYS_CG_SET_addr 0x15000004

#define PWR_ONS_1_D     3
#define PWR_CKD_1_D     4
#define PWR_ONN_1_D     2
#define PWR_ISO_1_D     1
#define PWR_RST_0_D     0

#define PWR_ON_SEQ_0    ((0x1 << PWR_ONS_1_D) | (0x1 << PWR_CKD_1_D) | (0x1 << PWR_ONN_1_D) | (0x1 << PWR_ISO_1_D) | (0x0 << PWR_RST_0_D))
#define PWR_ON_SEQ_1    ((0x1 << PWR_ONS_1_D) | (0x0 << PWR_CKD_1_D) | (0x1 << PWR_ONN_1_D) | (0x1 << PWR_ISO_1_D) | (0x0 << PWR_RST_0_D))
#define PWR_ON_SEQ_2    ((0x1 << PWR_ONS_1_D) | (0x0 << PWR_CKD_1_D) | (0x1 << PWR_ONN_1_D) | (0x0 << PWR_ISO_1_D) | (0x0 << PWR_RST_0_D))
#define PWR_ON_SEQ_3    ((0x1 << PWR_ONS_1_D) | (0x0 << PWR_CKD_1_D) | (0x1 << PWR_ONN_1_D) | (0x0 << PWR_ISO_1_D) | (0x1 << PWR_RST_0_D))
// ]
#endif

// VDEC virtual base address
#define VDEC_BASE_PHY   0x16000000
#define VDEC_REGION     0x29000
#define VDEC_MISC_BASE  VDEC_BASE + 0x0000
#define VDEC_VLD_BASE   VDEC_BASE + 0x1000

#define HW_BASE         0x7FFF000
#define HW_REGION       0x2000

#define INFO_BASE       0x10000000
#define INFO_REGION     0x1000

int KVA_VENC_IRQ_ACK_ADDR;
int KVA_VENC_IRQ_STATUS_ADDR;
#ifdef VENC_PWR_FPGA
// Cheng-Jung 20120621 VENC power physical base address (FPGA only, should use API) [
int KVA_VENC_CLK_CFG_0_ADDR, KVA_VENC_CLK_CFG_4_ADDR, KVA_VENC_PWR_ADDR, KVA_VENCSYS_CG_SET_ADDR;
// ]
#endif

extern unsigned int pmem_user_v2p_video(unsigned int va);
#ifdef VDEC_USE_L2C
extern int config_L2(int size);
#endif
///MEIZU_BSP{@
extern void disable_dpidle_by_bit(int id);
extern void disable_soidle_by_bit(int id);
extern void enable_dpidle_by_bit(int id);
extern void enable_soidle_by_bit(int id);
///@}

void vdec_power_on(void)
{
    mutex_lock(&VdecPWRLock);
    gu4VdecPWRCounter++;
    mutex_unlock(&VdecPWRLock);
    
    // Central power on
    enable_clock(MT_CG_DISP0_SMI_COMMON, "VDEC");
    enable_clock(MT_CG_VDEC0_VDEC, "VDEC");
    enable_clock(MT_CG_VDEC1_LARB, "VDEC");
//    printk ("vdec_power_on");
#ifdef VDEC_USE_L2C    
    //enable_clock(MT_CG_INFRA_L2C_SRAM, "VDEC");
#endif
}

void vdec_power_off(void)
{
    mutex_lock(&VdecPWRLock);
    if (gu4VdecPWRCounter == 0)
    {
    }
    else
    {
        gu4VdecPWRCounter--;
        // Central power off
        disable_clock(MT_CG_VDEC0_VDEC, "VDEC");
        disable_clock(MT_CG_VDEC1_LARB, "VDEC");
        disable_clock(MT_CG_DISP0_SMI_COMMON, "VDEC");
        // printk ("vdec_power_off");
#ifdef VDEC_USE_L2C    
    //disable_clock(MT_CG_INFRA_L2C_SRAM, "VDEC");
#endif
    }
    mutex_unlock(&VdecPWRLock);
}

void venc_power_on(void)
{
    mutex_lock(&VencPWRLock);
    gu4VencPWRCounter++;
    mutex_unlock(&VencPWRLock);
    
    MFV_LOGD("venc_power_on +\n");
#ifdef VENC_PWR_FPGA
    // Cheng-Jung 20120621 VENC power physical base address (FPGA only, should use API) [
    //VDO_HW_WRITE(KVA_VENC_CLK_CFG_0_ADDR, ((VDO_HW_READ(KVA_VENC_CLK_CFG_0_ADDR) & 0xfffffff8) | 0x00000001));
    //VDO_HW_WRITE(KVA_VENC_CLK_CFG_0_ADDR, ((VDO_HW_READ(KVA_VENC_CLK_CFG_0_ADDR) & 0xfffff8ff) | 0x00000100));
    //VDO_HW_WRITE(KVA_VENC_CLK_CFG_4_ADDR, ((VDO_HW_READ(KVA_VENC_CLK_CFG_4_ADDR) & 0xffff00ff) | 0x00000600));

    // MTCMOS on
    VDO_HW_WRITE(KVA_VENC_PWR_ADDR, ((VDO_HW_READ(KVA_VENC_PWR_ADDR) & 0xffffffc0) | PWR_ON_SEQ_0));
    VDO_HW_WRITE(KVA_VENC_PWR_ADDR, ((VDO_HW_READ(KVA_VENC_PWR_ADDR) & 0xffffffc0) | PWR_ON_SEQ_1));
    VDO_HW_WRITE(KVA_VENC_PWR_ADDR, ((VDO_HW_READ(KVA_VENC_PWR_ADDR) & 0xffffffc0) | PWR_ON_SEQ_2));
    VDO_HW_WRITE(KVA_VENC_PWR_ADDR, ((VDO_HW_READ(KVA_VENC_PWR_ADDR) & 0xffffffc0) | PWR_ON_SEQ_3));

    // CG (clock gate) on
    VDO_HW_WRITE(KVA_VENCSYS_CG_SET_ADDR, 0x00000001);
    // ]
#else
    enable_clock(MT_CG_DISP0_SMI_COMMON, "VENC");
    //enable_clock(MT_CG_VENC_JPGENC, "VENC");
    //enable_clock(MT_CG_IMAGE_LARB2_SMI, "VENC");
    enable_clock(MT_CG_VENC_VENC, "VENC");
    enable_clock(MT_CG_VENC_LARB , "VENC");
    //MFV_LOGD("venc_power_on: VENC_CG_GCON = 0x%08X, SMI = 0x%08X", VDO_HW_READ(0xF8000000), VDO_HW_READ(0xF4000100));
#endif
    MFV_LOGD("venc_power_on -\n");
}

void venc_power_off(void)
{
    mutex_lock(&VencPWRLock);
    if (gu4VencPWRCounter == 0)
    {
    }
    else
    {
        gu4VencPWRCounter--;
        MFV_LOGD("venc_power_off +\n");
        //disable_clock(MT_CG_VENC_JPGENC, "VENC");
        //disable_clock(MT_CG_IMAGE_LARB2_SMI, "VENC");
        disable_clock(MT_CG_VENC_VENC, "VENC");
        disable_clock(MT_CG_VENC_LARB, "VENC");
        disable_clock(MT_CG_DISP0_SMI_COMMON, "VENC");
        //MFV_LOGD("venc_power_off: VENC_CG_GCON = 0x%08X, SMI = 0x%08X", VDO_HW_READ(0xF8000000), VDO_HW_READ(0xF4000100));
        MFV_LOGD("venc_power_off -\n");
    }
    mutex_unlock(&VencPWRLock);
    
    
}


void dec_isr(void)
{
    VAL_RESULT_T  eValRet;
    unsigned long ulFlags, ulFlagsISR, ulFlagsLockHW;

    VAL_UINT32_T u4TempDecISRCount = 0;
    VAL_UINT32_T u4TempLockDecHWCount = 0;
    VAL_UINT32_T u4CgStatus = 0;
    VAL_UINT32_T u4DecDoneStatus = 0;

    u4CgStatus = VDO_HW_READ(0xF6000000);
    if ((u4CgStatus & 0x10) != 0)
    {
        MFV_LOGE("[MFV][ERROR] DEC ISR, VDEC active is not 0x0 (0x%08x)", u4CgStatus);
        return;
    }
    
    u4DecDoneStatus = VDO_HW_READ(0xF60200A4);
    if ((u4DecDoneStatus & (0x1 << 16)) != 0x10000)
    {
        MFV_LOGE("[MFV][ERROR] DEC ISR, Decode done status is not 0x1 (0x%08x)", u4DecDoneStatus);
        return;
    }
    
    
    spin_lock_irqsave(&DecISRCountLock, ulFlagsISR);
    gu4DecISRCount++;
    u4TempDecISRCount = gu4DecISRCount;
    spin_unlock_irqrestore(&DecISRCountLock, ulFlagsISR);

    spin_lock_irqsave(&LockDecHWCountLock, ulFlagsLockHW);
    u4TempLockDecHWCount = gu4LockDecHWCount;
    spin_unlock_irqrestore(&LockDecHWCountLock, ulFlagsLockHW);

    if (u4TempDecISRCount != u4TempLockDecHWCount)
    {
        //MFV_LOGE("[INFO] Dec ISRCount: 0x%x, LockHWCount:0x%x\n", u4TempDecISRCount, u4TempLockDecHWCount);
    }

    // Clear interrupt 
    VDO_HW_WRITE(VDEC_MISC_BASE+41*4, VDO_HW_READ(VDEC_MISC_BASE + 41*4) | 0x11);
    VDO_HW_WRITE(VDEC_MISC_BASE+41*4, VDO_HW_READ(VDEC_MISC_BASE + 41*4) & ~0x10);


    spin_lock_irqsave(&DecIsrLock, ulFlags);
    eValRet = eVideoSetEvent(&DecIsrEvent, sizeof(VAL_EVENT_T));
    if(VAL_RESULT_NO_ERROR != eValRet)
    {
        MFV_LOGE("[MFV][ERROR] ISR set DecIsrEvent error\n");
    }
    spin_unlock_irqrestore(&DecIsrLock, ulFlags);

    return;
}


void enc_isr(void)
{
    VAL_RESULT_T  eValRet;
    unsigned long ulFlagsISR, ulFlagsLockHW;


    VAL_UINT32_T u4TempEncISRCount = 0;
    VAL_UINT32_T u4TempLockEncHWCount = 0;
    //----------------------

    spin_lock_irqsave(&EncISRCountLock, ulFlagsISR);
    gu4EncISRCount++;
    u4TempEncISRCount = gu4EncISRCount;
    spin_unlock_irqrestore(&EncISRCountLock, ulFlagsISR);

    spin_lock_irqsave(&LockEncHWCountLock, ulFlagsLockHW);
    u4TempLockEncHWCount = gu4LockEncHWCount;
    spin_unlock_irqrestore(&LockEncHWCountLock, ulFlagsLockHW);

    if (u4TempEncISRCount != u4TempLockEncHWCount)
    {
        //MFV_LOGE("[INFO] Enc ISRCount: 0x%x, LockHWCount:0x%x\n", u4TempEncISRCount, u4TempLockEncHWCount);
    }

    if (grVcodecEncHWLock.pvHandle == 0)
    {
        MFV_LOGE("[ERROR] NO one Lock Enc HW, please check!!\n");

        // Clear all status
        //VDO_HW_WRITE(KVA_VENC_MP4_IRQ_ACK_ADDR, 1);
        VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_PAUSE);
        //VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_DRAM_VP8);
        VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_SWITCH);
        VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_DRAM);
        VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_SPS);
        VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_PPS);
        VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_FRM);
        return;
    }
    
    if (grVcodecEncHWLock.eDriverType == VAL_DRIVER_TYPE_H264_ENC) // hardwire
    {
        gu4HwVencIrqStatus = VDO_HW_READ(KVA_VENC_IRQ_STATUS_ADDR);
        if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_PAUSE)
        {
            VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_PAUSE);
        }
        if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_SWITCH)
        {
            VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_SWITCH);
        }
        if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_DRAM)
        {
            VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_DRAM);
        }
        if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_SPS)
        {
            VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_SPS);
        }
        if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_PPS)
        {
            VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_PPS);
        }
        if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_FRM)
        {
            VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_FRM);
        }
    }
#ifdef MTK_VIDEO_HEVC_SUPPORT
    else if (grVcodecEncHWLock.eDriverType == VAL_DRIVER_TYPE_HEVC_ENC) // hardwire
    {
        //MFV_LOGE("[enc_isr] VAL_DRIVER_TYPE_HEVC_ENC %d!!\n", gu4HwVencIrqStatus);
        
        gu4HwVencIrqStatus = VDO_HW_READ(KVA_VENC_IRQ_STATUS_ADDR);
        if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_PAUSE)
        {
            VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_PAUSE);
        }
        if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_SWITCH)
        {
            VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_SWITCH);
        }
        if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_DRAM)
        {
            VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_DRAM);
        }
        if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_SPS)
        {
            VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_SPS);
        }
        if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_PPS)
        {
            VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_PPS);
        }
        if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_FRM)
        {
            VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_FRM);
        }
        if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_VPS)
        {
            VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR, VENC_IRQ_STATUS_VPS);
        }
    }
#endif    
    else
    {
        MFV_LOGE("Invalid lock holder driver type = %d\n", grVcodecEncHWLock.eDriverType);
    }

    eValRet = eVideoSetEvent(&EncIsrEvent, sizeof(VAL_EVENT_T)); 
    if(VAL_RESULT_NO_ERROR != eValRet)
    {
        MFV_LOGE("[MFV][ERROR] ISR set EncIsrEvent error\n");
    }
}

static irqreturn_t video_intr_dlr(int irq, void *dev_id)
{   
    dec_isr();
    return IRQ_HANDLED;
}

static irqreturn_t video_intr_dlr2(int irq, void *dev_id)
{   
    enc_isr();
    return IRQ_HANDLED;
}


static long vcodec_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    VAL_INT32_T ret;
    VAL_UINT8_T *user_data_addr;
    VAL_RESULT_T  eValRet;
    VAL_UINT32_T ulFlags, ulFlagsLockHW;
    VAL_HW_LOCK_T rHWLock;
    VAL_BOOL_T  bLockedHW = VAL_FALSE;
    VAL_UINT32_T FirstUseDecHW = 0;
    VAL_UINT32_T FirstUseEncHW = 0;
    VAL_TIME_T rCurTime;
    VAL_UINT32_T u4TimeInterval;
    VAL_ISR_T  val_isr;
    VAL_VCODEC_CORE_LOADING_T rTempCoreLoading;
    VAL_VCODEC_CPU_OPP_LIMIT_T rCpuOppLimit;
    VAL_INT32_T temp_nr_cpu_ids;
    VAL_POWER_T rPowerParam;
#if 0
    VCODEC_DRV_CMD_QUEUE_T rDrvCmdQueue;
    P_VCODEC_DRV_CMD_T cmd_queue = VAL_NULL;
    VAL_UINT32_T u4Size, uValue, nCount;
#endif

    switch(cmd) {
        case VCODEC_SET_THREAD_ID:
            MFV_LOGE("[ROME] VCODEC_SET_THREAD_ID [EMPTY] + tid = %d\n", current->pid);
            
            MFV_LOGE("[ROME] VCODEC_SET_THREAD_ID [EMPTY] - tid = %d\n", current->pid);
        break;

        case VCODEC_ALLOC_NON_CACHE_BUFFER:
            MFV_LOGE("[ROME][M4U]! VCODEC_ALLOC_NON_CACHE_BUFFER [EMPTY] + tid = %d\n", current->pid);

            MFV_LOGE("[ROME][M4U]! VCODEC_ALLOC_NON_CACHE_BUFFER [EMPTY] - tid = %d\n", current->pid);
        break;

        case VCODEC_FREE_NON_CACHE_BUFFER:
            MFV_LOGE("[ROME][M4U]! VCODEC_FREE_NON_CACHE_BUFFER [EMPTY] + tid = %d\n", current->pid);            

            MFV_LOGE("[ROME][M4U]! VCODEC_FREE_NON_CACHE_BUFFER [EMPTY] - tid = %d\n", current->pid);
        break;

        case VCODEC_INC_DEC_EMI_USER:
            MFV_LOGD("[ROME] VCODEC_INC_DEC_EMI_USER + tid = %d\n", current->pid);

            mutex_lock(&DecEMILock);
            gu4DecEMICounter++;
            MFV_LOGE("DEC_EMI_USER = %d\n", gu4DecEMICounter);
            user_data_addr = (VAL_UINT8_T *)arg;
            ret = copy_to_user(user_data_addr, &gu4DecEMICounter, sizeof(VAL_UINT32_T));
            if (ret)
            {
            	MFV_LOGE("[ERROR] VCODEC_INC_DEC_EMI_USER, copy_to_user failed: %d\n", ret);
                mutex_unlock(&DecEMILock);
            	return -EFAULT;
            }
            mutex_unlock(&DecEMILock);

            MFV_LOGD("[ROME] VCODEC_INC_DEC_EMI_USER - tid = %d\n", current->pid);            
        break;

        case VCODEC_DEC_DEC_EMI_USER:
            MFV_LOGD("[ROME] VCODEC_DEC_DEC_EMI_USER + tid = %d\n", current->pid);

            mutex_lock(&DecEMILock);
            gu4DecEMICounter--;
            MFV_LOGE("DEC_EMI_USER = %d\n", gu4DecEMICounter);
            user_data_addr = (VAL_UINT8_T *)arg;
            ret = copy_to_user(user_data_addr, &gu4DecEMICounter, sizeof(VAL_UINT32_T));
            if (ret)
            {
            	MFV_LOGE("[ERROR] VCODEC_DEC_DEC_EMI_USER, copy_to_user failed: %d\n", ret);
                mutex_unlock(&DecEMILock);
            	return -EFAULT;
            }
            mutex_unlock(&DecEMILock);

            MFV_LOGD("[ROME] VCODEC_DEC_DEC_EMI_USER - tid = %d\n", current->pid);            
        break;

        case VCODEC_INC_ENC_EMI_USER:
            MFV_LOGD("[ROME] VCODEC_INC_ENC_EMI_USER + tid = %d\n", current->pid);

            mutex_lock(&EncEMILock);
            gu4EncEMICounter++;
            MFV_LOGE("ENC_EMI_USER = %d\n", gu4EncEMICounter);
            user_data_addr = (VAL_UINT8_T *)arg;
            ret = copy_to_user(user_data_addr, &gu4EncEMICounter, sizeof(VAL_UINT32_T));
            if (ret) {
                MFV_LOGE("[ERROR] VCODEC_INC_ENC_EMI_USER, copy_to_user failed: %d\n", ret);
                mutex_unlock(&EncEMILock);
                return -EFAULT;
            }
            mutex_unlock(&EncEMILock);

            MFV_LOGD("[ROME] VCODEC_INC_ENC_EMI_USER - tid = %d\n", current->pid);
        break;

        case VCODEC_DEC_ENC_EMI_USER:
            MFV_LOGD("[ROME] VCODEC_DEC_ENC_EMI_USER + tid = %d\n", current->pid);

            mutex_lock(&EncEMILock);
            gu4EncEMICounter--;
            MFV_LOGE("ENC_EMI_USER = %d\n", gu4EncEMICounter);
            user_data_addr = (VAL_UINT8_T *)arg;
            ret = copy_to_user(user_data_addr, &gu4EncEMICounter, sizeof(VAL_UINT32_T));
            if (ret) {
                MFV_LOGE("[ERROR] VCODEC_DEC_ENC_EMI_USER, copy_to_user failed: %d\n", ret);
                mutex_unlock(&EncEMILock);
                return -EFAULT;
            }
            mutex_unlock(&EncEMILock);

            MFV_LOGD("[ROME] VCODEC_DEC_ENC_EMI_USER - tid = %d\n", current->pid);
        break;

        case VCODEC_LOCKHW:
            MFV_LOGD("[ROME] VCODEC_LOCKHW + tid = %d\n", current->pid);
            user_data_addr = (VAL_UINT8_T *)arg;
            ret = copy_from_user(&rHWLock, user_data_addr, sizeof(VAL_HW_LOCK_T));
            if (ret) {
                MFV_LOGE("[ERROR] VCODEC_LOCKHW, copy_from_user failed: %d\n", ret);
                return -EFAULT;
            }

            MFV_LOGD("LOCKHW eDriverType = %d\n", rHWLock.eDriverType);
            eValRet = VAL_RESULT_INVALID_ISR;
            if (rHWLock.eDriverType == VAL_DRIVER_TYPE_MP4_DEC ||
#ifdef MTK_VIDEO_HEVC_SUPPORT
                rHWLock.eDriverType == VAL_DRIVER_TYPE_HEVC_DEC ||
#endif
                rHWLock.eDriverType == VAL_DRIVER_TYPE_H264_DEC ||
                rHWLock.eDriverType == VAL_DRIVER_TYPE_MP1_MP2_DEC ||
	        rHWLock.eDriverType == VAL_DRIVER_TYPE_VC1_DEC ||
   	        rHWLock.eDriverType == VAL_DRIVER_TYPE_VC1_ADV_DEC ||
	        rHWLock.eDriverType == VAL_DRIVER_TYPE_VP8_DEC)
            {
                while (bLockedHW == VAL_FALSE)
                {
                    mutex_lock(&DecHWLockEventTimeoutLock);
                    if (DecHWLockEvent.u4TimeoutMs == 1) {
                        MFV_LOGE("[NOT ERROR][VCODEC_LOCKHW] First Use Dec HW!!\n");
                        FirstUseDecHW = 1;
                    }
                    else {
                        FirstUseDecHW = 0;
                    }                    
                    mutex_unlock(&DecHWLockEventTimeoutLock);
                    if (FirstUseDecHW == 1)
                    {
                        eValRet = eVideoWaitEvent(&DecHWLockEvent, sizeof(VAL_EVENT_T));
                    }
                    mutex_lock(&DecHWLockEventTimeoutLock);
                    if (DecHWLockEvent.u4TimeoutMs != 1000)
                    {
                        DecHWLockEvent.u4TimeoutMs = 1000;
                        FirstUseDecHW = 1;
                    }
                    else
                    {
                        FirstUseDecHW = 0;
                    }
                    mutex_unlock(&DecHWLockEventTimeoutLock);

                    mutex_lock(&VdecHWLock);
                    // one process try to lock twice
                    if (grVcodecDecHWLock.pvHandle == (VAL_VOID_T*)pmem_user_v2p_video((unsigned int)rHWLock.pvHandle)) {
                        MFV_LOGE("[WARNING] one decoder instance try to lock twice, may cause lock HW timeout!! instance = 0x%x, CurrentTID = %d\n",
                            grVcodecDecHWLock.pvHandle, current->pid);
                    }
                    mutex_unlock(&VdecHWLock);

                    if (FirstUseDecHW == 0) {
                        MFV_LOGD("Not first time use HW, timeout = %d\n", DecHWLockEvent.u4TimeoutMs);
                        eValRet = eVideoWaitEvent(&DecHWLockEvent, sizeof(VAL_EVENT_T));
                    }

                    if (VAL_RESULT_INVALID_ISR == eValRet) {
                        MFV_LOGE("[ERROR][VCODEC_LOCKHW] DecHWLockEvent TimeOut, CurrentTID = %d\n", current->pid);
                        if (FirstUseDecHW != 1) {
                            mutex_lock(&VdecHWLock);
                            if (grVcodecDecHWLock.pvHandle == 0) {
                                MFV_LOGE("[WARNING] maybe mediaserver restart before, please check!!\n");
                            }
                            else {
                                MFV_LOGE("[WARNING] someone use HW, and check timeout value!!\n");
                            }
                            mutex_unlock(&VdecHWLock);
                        }
                    }
                    else if (VAL_RESULT_RESTARTSYS == eValRet)
                    {
                        MFV_LOGE("[WARNING] VAL_RESULT_RESTARTSYS return when HWLock!!\n");
                        return -ERESTARTSYS;
                    }

                    mutex_lock(&VdecHWLock);
                    if (grVcodecDecHWLock.pvHandle == 0) // No one holds dec hw lock now
                    {
                        gu4VdecLockThreadId = current->pid;
                        grVcodecDecHWLock.pvHandle = (VAL_VOID_T*)pmem_user_v2p_video((unsigned int)rHWLock.pvHandle);
                        grVcodecDecHWLock.eDriverType = rHWLock.eDriverType;
                        eVideoGetTimeOfDay(&grVcodecDecHWLock.rLockedTime, sizeof(VAL_TIME_T));

                        MFV_LOGD("No process use dec HW, so current process can use HW\n");
                        MFV_LOGD("LockInstance = 0x%x CurrentTID = %d, rLockedTime(s, us) = %d, %d\n",
                            grVcodecDecHWLock.pvHandle, current->pid, grVcodecDecHWLock.rLockedTime.u4Sec, grVcodecDecHWLock.rLockedTime.u4uSec);
                    
                        bLockedHW = VAL_TRUE;
                        if (VAL_RESULT_INVALID_ISR == eValRet && FirstUseDecHW != 1) {
                            MFV_LOGE("[WARNING] reset power/irq when HWLock!!\n");
                            vdec_power_off();
                            disable_irq(VDEC_IRQ_BIT_ID);
                        }
                        vdec_power_on();
                        //enable_irq(MT_VDEC_IRQ_ID);
                        enable_irq(VDEC_IRQ_BIT_ID);
                    }
                    else // Another one holding dec hw now
                    {
                        MFV_LOGE("[NOT ERROR][VCODEC_LOCKHW] E\n");
                        eVideoGetTimeOfDay(&rCurTime, sizeof(VAL_TIME_T));
                        u4TimeInterval = (((((rCurTime.u4Sec - grVcodecDecHWLock.rLockedTime.u4Sec) * 1000000) + rCurTime.u4uSec) 
                            - grVcodecDecHWLock.rLockedTime.u4uSec) / 1000);

                        MFV_LOGD("someone use dec HW, and check timeout value\n");
                        MFV_LOGD("Instance = 0x%x CurrentTID = %d, TimeInterval(ms) = %d, TimeOutValue(ms)) = %d\n",
                            grVcodecDecHWLock.pvHandle, current->pid, u4TimeInterval, rHWLock.u4TimeoutMs);

                        MFV_LOGE("[VCODEC_LOCKHW] Lock Instance = 0x%x, Lock TID = %d, CurrentTID = %d, rLockedTime(%d s, %d us), rCurTime(%d s, %d us)\n",
                            grVcodecDecHWLock.pvHandle, gu4VdecLockThreadId, current->pid, 
                            grVcodecDecHWLock.rLockedTime.u4Sec, grVcodecDecHWLock.rLockedTime.u4uSec,
                            rCurTime.u4Sec, rCurTime.u4uSec
                            );

                        // 2012/12/16. Cheng-Jung Never steal hardware lock
                        if (0)
                        //if (u4TimeInterval >= rHWLock.u4TimeoutMs)
                        {
                            grVcodecDecHWLock.pvHandle = (VAL_VOID_T*)pmem_user_v2p_video((unsigned int)rHWLock.pvHandle);
                            grVcodecDecHWLock.eDriverType = rHWLock.eDriverType;
                            eVideoGetTimeOfDay(&grVcodecDecHWLock.rLockedTime, sizeof(VAL_TIME_T));
                            bLockedHW = VAL_TRUE;
                            vdec_power_on();
                            // TODO: Error handling, VDEC break, reset?
                        }
                    }
                    mutex_unlock(&VdecHWLock);
                }
                spin_lock_irqsave(&LockDecHWCountLock, ulFlagsLockHW);
                gu4LockDecHWCount++;
                spin_unlock_irqrestore(&LockDecHWCountLock, ulFlagsLockHW);
            }
            else if (rHWLock.eDriverType == VAL_DRIVER_TYPE_H264_ENC ||
#ifdef MTK_VIDEO_HEVC_SUPPORT
                     rHWLock.eDriverType == VAL_DRIVER_TYPE_HEVC_ENC ||
#endif
                     rHWLock.eDriverType == VAL_DRIVER_TYPE_JPEG_ENC)
            {
                while (bLockedHW == VAL_FALSE)
                {
                    // Early break for JPEG VENC
                    if (rHWLock.u4TimeoutMs == 0)
                    {
                        if (grVcodecEncHWLock.pvHandle != 0)
                        {
                            break;
                        }
                    }
                
                    // Wait to acquire Enc HW lock
                    mutex_lock(&EncHWLockEventTimeoutLock);
                    if (EncHWLockEvent.u4TimeoutMs == 1) {
                        MFV_LOGE("[NOT ERROR][VCODEC_LOCKHW] First Use Enc HW %d!!\n", rHWLock.eDriverType);
                        FirstUseEncHW = 1;
                    }
                    else {
                        FirstUseEncHW = 0;
                    }                    
                    mutex_unlock(&EncHWLockEventTimeoutLock);
                    if (FirstUseEncHW == 1)
                    {
                        eValRet = eVideoWaitEvent(&EncHWLockEvent, sizeof(VAL_EVENT_T));
                    }
                    mutex_lock(&EncHWLockEventTimeoutLock);
                    if (EncHWLockEvent.u4TimeoutMs == 1)
                    {
                        EncHWLockEvent.u4TimeoutMs = 1000;
                        FirstUseEncHW = 1;
                    }
                    else
                    {
                        FirstUseEncHW = 0;
                        if (rHWLock.u4TimeoutMs == 0)
                        {
                            EncHWLockEvent.u4TimeoutMs = 0; // No wait
                        }
                        else
                        {
                            EncHWLockEvent.u4TimeoutMs = 1000; // Wait indefinitely
                        }
                    }
                    mutex_unlock(&EncHWLockEventTimeoutLock);

                    mutex_lock(&VencHWLock);
                    // one process try to lock twice
                    if (grVcodecEncHWLock.pvHandle == (VAL_VOID_T*)pmem_user_v2p_video((unsigned int)rHWLock.pvHandle)) {
                        MFV_LOGE("[WARNING] one encoder instance try to lock twice, may cause lock HW timeout!! instance = 0x%x, CurrentTID = %d, type:%d\n",
                            grVcodecEncHWLock.pvHandle, current->pid, rHWLock.eDriverType);
                    }
                    mutex_unlock(&VencHWLock);

                    if (FirstUseEncHW == 0) {
                        eValRet = eVideoWaitEvent(&EncHWLockEvent, sizeof(VAL_EVENT_T));
                    }

                    if (VAL_RESULT_INVALID_ISR == eValRet) {
                        MFV_LOGE("[ERROR][VCODEC_LOCKHW] EncHWLockEvent TimeOut, CurrentTID = %d\n", current->pid);
                        if (FirstUseEncHW != 1) {
                            mutex_lock(&VencHWLock);
                            if (grVcodecEncHWLock.pvHandle == 0) {
                                MFV_LOGE("[WARNING] maybe mediaserver restart before, please check!!\n");
                            }
                            else {                                
                                MFV_LOGE("[WARNING] someone use HW, and check timeout value!! %d\n", gLockTimeOutCount);
                                ++gLockTimeOutCount;
                                if (gLockTimeOutCount > 30)
                                {
                                    MFV_LOGE("VCODEC_LOCKHW - ID %d  fail, someone locked HW time out more than 30 times %x, %x, %x, type:%d\n", current->pid, grVcodecEncHWLock.pvHandle, pmem_user_v2p_video((unsigned int)rHWLock.pvHandle), rHWLock.pvHandle, rHWLock.eDriverType);
                                    gLockTimeOutCount = 0;
                                    mutex_unlock(&VencHWLock);
                                    return -EFAULT;
                                }
                                
                                if (rHWLock.u4TimeoutMs == 0)
                                {
                                    MFV_LOGE("VCODEC_LOCKHW - ID %d  fail, someone locked HW already %x, %x, %x, type:%d\n", current->pid, grVcodecEncHWLock.pvHandle, pmem_user_v2p_video((unsigned int)rHWLock.pvHandle), rHWLock.pvHandle, rHWLock.eDriverType);
                                    gLockTimeOutCount = 0;
                                    mutex_unlock(&VencHWLock);
                                    return -EFAULT;
                                }                    
                            }
                            mutex_unlock(&VencHWLock);
                        }
                    }

                    mutex_lock(&VencHWLock);
                    if (grVcodecEncHWLock.pvHandle == 0)   //No process use HW, so current process can use HW
                    {
                        if (rHWLock.eDriverType == VAL_DRIVER_TYPE_H264_ENC ||
#ifdef MTK_VIDEO_HEVC_SUPPORT
                            rHWLock.eDriverType == VAL_DRIVER_TYPE_HEVC_ENC ||
#endif                            
                            rHWLock.eDriverType == VAL_DRIVER_TYPE_JPEG_ENC)
                        {
                            grVcodecEncHWLock.pvHandle = (VAL_VOID_T*)pmem_user_v2p_video((unsigned int)rHWLock.pvHandle);
                            MFV_LOGD("[LOG][VCODEC_LOCKHW] No process use HW, so current process can use HW, handle = 0x%x\n", grVcodecEncHWLock.pvHandle);
                            grVcodecEncHWLock.eDriverType = rHWLock.eDriverType;
                            eVideoGetTimeOfDay(&grVcodecEncHWLock.rLockedTime, sizeof(VAL_TIME_T));
                    
                            MFV_LOGD("No process use HW, so current process can use HW\n");
                            MFV_LOGD("LockInstance = 0x%x CurrentTID = %d, rLockedTime(s, us) = %d, %d\n",
                                grVcodecEncHWLock.pvHandle, current->pid, grVcodecEncHWLock.rLockedTime.u4Sec, grVcodecEncHWLock.rLockedTime.u4uSec);
                            
                            bLockedHW = VAL_TRUE;
#ifdef MTK_VIDEO_HEVC_SUPPORT
                            if (rHWLock.eDriverType == VAL_DRIVER_TYPE_H264_ENC ||
                                rHWLock.eDriverType == VAL_DRIVER_TYPE_HEVC_ENC)
#else
                            if (rHWLock.eDriverType == VAL_DRIVER_TYPE_H264_ENC)
#endif
                            {
                                venc_power_on();
                                //enable_irq(MT_VENC_IRQ_ID);
                                enable_irq(VENC_IRQ_BIT_ID);
                            }
                        }
                    }
                    else    //someone use HW, and check timeout value
                    {
                        if (rHWLock.u4TimeoutMs == 0)
                        {                        
                            bLockedHW = VAL_FALSE;
                            mutex_unlock(&VencHWLock);
                            break;
                        }
                    
                        eVideoGetTimeOfDay(&rCurTime, sizeof(VAL_TIME_T));
                        u4TimeInterval = (((((rCurTime.u4Sec - grVcodecEncHWLock.rLockedTime.u4Sec) * 1000000) + rCurTime.u4uSec) 
                            - grVcodecEncHWLock.rLockedTime.u4uSec) / 1000);
                
                        MFV_LOGD("someone use enc HW, and check timeout value\n");
                        MFV_LOGD("LockInstance = 0x%x, CurrentInstance = 0x%x, CurrentTID = %d, TimeInterval(ms) = %d, TimeOutValue(ms)) = %d\n",
                            grVcodecEncHWLock.pvHandle, pmem_user_v2p_video((unsigned int)rHWLock.pvHandle), current->pid, u4TimeInterval, rHWLock.u4TimeoutMs);
                
                        MFV_LOGD("LockInstance = 0x%x, CurrentInstance = 0x%x, CurrentTID = %d, rLockedTime(s, us) = %d, %d, rCurTime(s, us) = %d, %d\n",
                            grVcodecEncHWLock.pvHandle, pmem_user_v2p_video((unsigned int)rHWLock.pvHandle), current->pid, 
                            grVcodecEncHWLock.rLockedTime.u4Sec, grVcodecEncHWLock.rLockedTime.u4uSec,
                            rCurTime.u4Sec, rCurTime.u4uSec
                            );
                        
                        ++gLockTimeOutCount;
                        if (gLockTimeOutCount > 30)
                        {
                            MFV_LOGE("VCODEC_LOCKHW - ID %d  fail, someone locked HW over 30 times without timeout %x, %x, %x, type:%d\n", current->pid, grVcodecEncHWLock.pvHandle, pmem_user_v2p_video((unsigned int)rHWLock.pvHandle), rHWLock.pvHandle, rHWLock.eDriverType);
                            gLockTimeOutCount = 0;
                            mutex_unlock(&VencHWLock);
                            return -EFAULT;
                        }

                        // 2013/04/10. Cheng-Jung Never steal hardware lock
                        if (0)
                        //if (u4TimeInterval >= rHWLock.u4TimeoutMs)  
                        {
                            if (rHWLock.eDriverType == VAL_DRIVER_TYPE_H264_ENC ||
#ifdef MTK_VIDEO_HEVC_SUPPORT
                                rHWLock.eDriverType == VAL_DRIVER_TYPE_HEVC_ENC ||
#endif                                
                                rHWLock.eDriverType == VAL_DRIVER_TYPE_JPEG_ENC)
                            {
                                grVcodecEncHWLock.pvHandle = (VAL_VOID_T*)pmem_user_v2p_video((unsigned int)rHWLock.pvHandle);
                                grVcodecEncHWLock.eDriverType = rHWLock.eDriverType;                                
                                eVideoGetTimeOfDay(&grVcodecEncHWLock.rLockedTime, sizeof(VAL_TIME_T));
                    
                                MFV_LOGD("LockInstance = 0x%x, CurrentTID = %d, rLockedTime(s, us) = %d, %d\n",
                                    grVcodecEncHWLock.pvHandle, current->pid, grVcodecEncHWLock.rLockedTime.u4Sec, grVcodecEncHWLock.rLockedTime.u4uSec);
                    
                                bLockedHW = VAL_TRUE;
#ifdef MTK_VIDEO_HEVC_SUPPORT
                                if (rHWLock.eDriverType == VAL_DRIVER_TYPE_H264_ENC ||
                                    rHWLock.eDriverType == VAL_DRIVER_TYPE_HEVC_ENC)
#else                                
                                if (rHWLock.eDriverType == VAL_DRIVER_TYPE_H264_ENC)
#endif                                    
                                {
                                    venc_power_on();
                                }
                            }
                        }
                    }
                
                    if (VAL_TRUE == bLockedHW)
                    {
                        MFV_LOGE("Lock ok grVcodecEncHWLock.pvHandle = 0x%x, va:%x, type:%d", grVcodecEncHWLock.pvHandle, (unsigned int)rHWLock.pvHandle, rHWLock.eDriverType);
                        gLockTimeOutCount = 0;
                    }
                    mutex_unlock(&VencHWLock);
                }

                if (VAL_FALSE == bLockedHW)
                {
                    MFV_LOGE("VCODEC_LOCKHW - ID %d  fail, someone locked HW already , %x, %x, %x, type:%d\n", current->pid, grVcodecEncHWLock.pvHandle, pmem_user_v2p_video((unsigned int)rHWLock.pvHandle), rHWLock.pvHandle, rHWLock.eDriverType); 
                    gLockTimeOutCount = 0;
                    return -EFAULT;                
                }
                
                spin_lock_irqsave(&LockEncHWCountLock, ulFlagsLockHW);
                gu4LockEncHWCount++;
                spin_unlock_irqrestore(&LockEncHWCountLock, ulFlagsLockHW);
                
                MFV_LOGD("get locked - ObjId =%d\n", current->pid);
                
                MFV_LOGD("VCODEC_LOCKHWed - tid = %d\n", current->pid); 
            }
            else
            {
                MFV_LOGE("[WARNING] VCODEC_LOCKHW Unknown instance\n");
                return -EFAULT;
            }
            MFV_LOGD("[ROME] VCODEC_LOCKHW - tid = %d\n", current->pid);
        break;

        case VCODEC_UNLOCKHW:
            MFV_LOGD("[ROME] VCODEC_UNLOCKHW + tid = %d\n", current->pid);
            user_data_addr = (VAL_UINT8_T *)arg;
            ret = copy_from_user(&rHWLock, user_data_addr, sizeof(VAL_HW_LOCK_T));
            if (ret) {
                MFV_LOGE("[ERROR] VCODEC_UNLOCKHW, copy_from_user failed: %d\n", ret);
                return -EFAULT;
            }
            
            MFV_LOGD("UNLOCKHW eDriverType = %d\n", rHWLock.eDriverType);
            eValRet = VAL_RESULT_INVALID_ISR;
            if (rHWLock.eDriverType == VAL_DRIVER_TYPE_MP4_DEC ||
#ifdef MTK_VIDEO_HEVC_SUPPORT
                rHWLock.eDriverType == VAL_DRIVER_TYPE_HEVC_DEC ||
#endif
                rHWLock.eDriverType == VAL_DRIVER_TYPE_H264_DEC ||
                rHWLock.eDriverType == VAL_DRIVER_TYPE_MP1_MP2_DEC ||
	        rHWLock.eDriverType == VAL_DRIVER_TYPE_VC1_DEC ||
  	        rHWLock.eDriverType == VAL_DRIVER_TYPE_VC1_ADV_DEC ||
	        rHWLock.eDriverType == VAL_DRIVER_TYPE_VP8_DEC)
            {
                mutex_lock(&VdecHWLock);
                if (grVcodecDecHWLock.pvHandle == (VAL_VOID_T*)pmem_user_v2p_video((unsigned int)rHWLock.pvHandle)) // Current owner give up hw lock
                {
                    grVcodecDecHWLock.pvHandle = 0;
                    grVcodecDecHWLock.eDriverType = VAL_DRIVER_TYPE_NONE;
                    //disable_irq(MT_VDEC_IRQ_ID);
                    disable_irq(VDEC_IRQ_BIT_ID);
                    // TODO: check if turning power off is ok
                    vdec_power_off();
                }
                else // Not current owner
                {
                    MFV_LOGD("[ERROR] Not owner trying to unlock dec hardware 0x%x\n", pmem_user_v2p_video((unsigned int)rHWLock.pvHandle));
                    mutex_unlock(&VdecHWLock);
                    return -EFAULT;
                }
                mutex_unlock(&VdecHWLock);
                eValRet = eVideoSetEvent(&DecHWLockEvent, sizeof(VAL_EVENT_T));
            }
            else if (rHWLock.eDriverType == VAL_DRIVER_TYPE_H264_ENC ||
#ifdef MTK_VIDEO_HEVC_SUPPORT
                     rHWLock.eDriverType == VAL_DRIVER_TYPE_HEVC_ENC ||
#endif
                     rHWLock.eDriverType == VAL_DRIVER_TYPE_JPEG_ENC)
            {
                mutex_lock(&VencHWLock);
                if (grVcodecEncHWLock.pvHandle == (VAL_VOID_T*)pmem_user_v2p_video((unsigned int)rHWLock.pvHandle)) // Current owner give up hw lock
                {
                    grVcodecEncHWLock.pvHandle = 0;
                    grVcodecEncHWLock.eDriverType = VAL_DRIVER_TYPE_NONE;
#ifdef MTK_VIDEO_HEVC_SUPPORT
                    if (rHWLock.eDriverType == VAL_DRIVER_TYPE_H264_ENC ||
                        rHWLock.eDriverType == VAL_DRIVER_TYPE_HEVC_ENC)
#else                    
                    if (rHWLock.eDriverType == VAL_DRIVER_TYPE_H264_ENC)
#endif                        
                    {                        
                        //disable_irq(MT_VENC_IRQ_ID);
                        disable_irq(VENC_IRQ_BIT_ID);
                        // turn venc power off
                        venc_power_off();
                    }
                }
                else // Not current owner
                {
                    // [TODO] error handling
                    MFV_LOGE("[ERROR] Not owner trying to unlock enc hardware 0x%x, pa:%x, va:%x type:%d\n", grVcodecEncHWLock.pvHandle, pmem_user_v2p_video((unsigned int)rHWLock.pvHandle), (unsigned int)rHWLock.pvHandle, rHWLock.eDriverType);
                    mutex_unlock(&VencHWLock);
                    return -EFAULT;
                }
                mutex_unlock(&VencHWLock);
                eValRet = eVideoSetEvent(&EncHWLockEvent, sizeof(VAL_EVENT_T));
            }
            else
            {
                MFV_LOGE("[WARNING] VCODEC_UNLOCKHW Unknown instance\n");
                return -EFAULT;            
            }
            MFV_LOGD("[ROME] VCODEC_UNLOCKHW - tid = %d\n", current->pid);
        break;

        case VCODEC_INC_PWR_USER:
            MFV_LOGD("[ROME] VCODEC_INC_PWR_USER + tid = %d\n", current->pid);
            user_data_addr = (VAL_UINT8_T *)arg;
            ret = copy_from_user(&rPowerParam, user_data_addr, sizeof(VAL_POWER_T));
            if (ret) {
                MFV_LOGE("[ERROR] VCODEC_INC_PWR_USER, copy_from_user failed: %d\n", ret);
                return -EFAULT;
            }
            MFV_LOGD("INC_PWR_USER eDriverType = %d\n", rPowerParam.eDriverType);
            mutex_lock(&L2CLock);
            if (rPowerParam.eDriverType == VAL_DRIVER_TYPE_H264_DEC ||
#ifdef MTK_VIDEO_HEVC_SUPPORT
                rPowerParam.eDriverType == VAL_DRIVER_TYPE_HEVC_DEC ||
#endif      
                rPowerParam.eDriverType == VAL_DRIVER_TYPE_MP4_DEC)
            {
                gu4L2CCounter++;
                MFV_LOGD("INC_PWR_USER L2C counter = %d\n", gu4L2CCounter);
            }
#ifdef VDEC_USE_L2C
            if (1 == gu4L2CCounter)
            {
                if (config_L2(SZ_256K))
                {
                    MFV_LOGE("[MFV][ERROR] Switch L2C size to 256K failed\n");
                    mutex_unlock(&L2CLock);
                    return -EFAULT;
                }
                else
                {
                    MFV_LOGD("[MFV][ERROR] Switch L2C size to 256K successful\n");
                }
            }
#endif
            mutex_unlock(&L2CLock);

            MFV_LOGD("[ROME] VCODEC_INC_PWR_USER - tid = %d\n", current->pid);
        break;

        case VCODEC_DEC_PWR_USER:
            MFV_LOGD("[ROME] VCODEC_DEC_PWR_USER + tid = %d\n", current->pid);
            user_data_addr = (VAL_UINT8_T *)arg;
            ret = copy_from_user(&rPowerParam, user_data_addr, sizeof(VAL_POWER_T));
            if (ret) {
                MFV_LOGE("[ERROR] VCODEC_DEC_PWR_USER, copy_from_user failed: %d\n", ret);
                return -EFAULT;
            }
            MFV_LOGD("DEC_PWR_USER eDriverType = %d\n", rPowerParam.eDriverType);

            mutex_lock(&L2CLock);
            if (rPowerParam.eDriverType == VAL_DRIVER_TYPE_H264_DEC ||
#ifdef MTK_VIDEO_HEVC_SUPPORT
                rPowerParam.eDriverType == VAL_DRIVER_TYPE_HEVC_DEC ||
#endif                
                rPowerParam.eDriverType == VAL_DRIVER_TYPE_MP4_DEC)
            {
                gu4L2CCounter--;
                MFV_LOGD("DEC_PWR_USER L2C counter  = %d\n", gu4L2CCounter);                
            }
#ifdef VDEC_USE_L2C            
            if (0 == gu4L2CCounter)
            {
                if (config_L2(SZ_512K))
                {
                    MFV_LOGE("[MFV][ERROR] Switch L2C size to 512K failed\n");
                    mutex_unlock(&L2CLock);
                    return -EFAULT;
                }
                else
                {
                    MFV_LOGD("[MFV][ERROR] Switch L2C size to 512K successful\n");
                }
            }
#endif
            mutex_unlock(&L2CLock);
            MFV_LOGD("[ROME] VCODEC_DEC_PWR_USER - tid = %d\n", current->pid);
        break;

        case VCODEC_WAITISR:
            
            MFV_LOGD("[ROME] VCODEC_WAITISR + tid = %d\n", current->pid);
            user_data_addr = (VAL_UINT8_T *)arg;
            ret = copy_from_user(&val_isr, user_data_addr, sizeof(VAL_ISR_T));
            if (ret) {
                MFV_LOGE("[ERROR] VCODEC_WAITISR, copy_from_user failed: %d\n", ret);
                return -EFAULT;
            }

            if (val_isr.eDriverType == VAL_DRIVER_TYPE_MP4_DEC ||
#ifdef MTK_VIDEO_HEVC_SUPPORT
                val_isr.eDriverType == VAL_DRIVER_TYPE_HEVC_DEC ||
#endif
                val_isr.eDriverType == VAL_DRIVER_TYPE_H264_DEC ||
                val_isr.eDriverType == VAL_DRIVER_TYPE_MP1_MP2_DEC ||
                val_isr.eDriverType == VAL_DRIVER_TYPE_VC1_DEC ||
                val_isr.eDriverType == VAL_DRIVER_TYPE_VC1_ADV_DEC ||
                val_isr.eDriverType == VAL_DRIVER_TYPE_VP8_DEC)
            {
                mutex_lock(&VdecHWLock);
                if (grVcodecDecHWLock.pvHandle == (VAL_VOID_T*)pmem_user_v2p_video((unsigned int)val_isr.pvHandle))
                {
                    bLockedHW = VAL_TRUE;
                }
                else
                {
                }
                mutex_unlock(&VdecHWLock);

                if (bLockedHW == VAL_FALSE)
                {
                    MFV_LOGE("[ERROR] DO NOT have HWLock, so return fail\n");
                    break;
                }

                spin_lock_irqsave(&DecIsrLock, ulFlags);
                DecIsrEvent.u4TimeoutMs = val_isr.u4TimeoutMs;
                spin_unlock_irqrestore(&DecIsrLock, ulFlags);
                                
                eValRet = eVideoWaitEvent(&DecIsrEvent, sizeof(VAL_EVENT_T));                
                if(VAL_RESULT_INVALID_ISR == eValRet)
                {
                    return -2;
                }
            }
#ifdef MTK_VIDEO_HEVC_SUPPORT
            else if (val_isr.eDriverType == VAL_DRIVER_TYPE_H264_ENC ||
                     val_isr.eDriverType == VAL_DRIVER_TYPE_HEVC_ENC)
#else            
            else if (val_isr.eDriverType == VAL_DRIVER_TYPE_H264_ENC)
#endif                
            {
                mutex_lock(&VencHWLock);
                if (grVcodecEncHWLock.pvHandle == (VAL_VOID_T*)pmem_user_v2p_video((unsigned int)val_isr.pvHandle))
                {
                    bLockedHW = VAL_TRUE;
                }
                else
                {
                }
                mutex_unlock(&VencHWLock);
                
                if (bLockedHW == VAL_FALSE)
                {
                    MFV_LOGE("[ERROR] DO NOT have enc HWLock, so return fail pa:%x, va:%x\n", pmem_user_v2p_video((unsigned int)val_isr.pvHandle), val_isr.pvHandle);
                    break;
                }
                
                spin_lock_irqsave(&EncIsrLock, ulFlags);
                EncIsrEvent.u4TimeoutMs = val_isr.u4TimeoutMs;
                spin_unlock_irqrestore(&EncIsrLock, ulFlags);

                eValRet = eVideoWaitEvent(&EncIsrEvent, sizeof(VAL_EVENT_T));                
                if(VAL_RESULT_INVALID_ISR == eValRet)
                {
                    return -2;
                }

                if (val_isr.u4IrqStatusNum > 0)
                {
                    val_isr.u4IrqStatus[0] = gu4HwVencIrqStatus;
                    ret = copy_to_user(user_data_addr, &val_isr, sizeof(VAL_ISR_T));
                    if (ret) {
                        MFV_LOGE("[ERROR] VCODEC_WAITISR, copy_to_user failed: %d\n", ret);
                        return -EFAULT;
                    }
                }
            }
            else
            {
                MFV_LOGE("[WARNING] VCODEC_WAITISR Unknown instance\n");
                return -EFAULT;
            }
            MFV_LOGD("[ROME] VCODEC_WAITISR - tid = %d\n", current->pid);
        break;

        case VCODEC_INITHWLOCK:
        {
            MFV_LOGE("[ROME] VCODEC_INITHWLOCK [EMPTY] + - tid = %d\n", current->pid);

            MFV_LOGE("[ROME] VCODEC_INITHWLOCK [EMPTY] - - tid = %d\n", current->pid);
        }
        break;

        case VCODEC_DEINITHWLOCK:
        { 
            MFV_LOGE("[ROME] VCODEC_DEINITHWLOCK [EMPTY] + - tid = %d\n", current->pid);

            MFV_LOGE("[ROME] VCODEC_DEINITHWLOCK [EMPTY] - - tid = %d\n", current->pid);
        }
        break;

        case VCODEC_GET_CPU_LOADING_INFO:
        {
            VAL_UINT8_T *user_data_addr;
            VAL_VCODEC_CPU_LOADING_INFO_T _temp;

            MFV_LOGD("[ROME] VCODEC_GET_CPU_LOADING_INFO +\n");
            user_data_addr = (VAL_UINT8_T *)arg;
            // TODO:
#if 0 // Morris Yang 20120112 mark temporarily
            _temp._cpu_idle_time = mt_get_cpu_idle(0);
            _temp._thread_cpu_time = mt_get_thread_cputime(0);
            spin_lock_irqsave(&OalHWContextLock, ulFlags);
            _temp._inst_count = getCurInstanceCount();
            spin_unlock_irqrestore(&OalHWContextLock, ulFlags);
            _temp._sched_clock = mt_sched_clock();
#endif
            ret = copy_to_user(user_data_addr, &_temp, sizeof(VAL_VCODEC_CPU_LOADING_INFO_T));
            if (ret) {
                MFV_LOGE("[ERROR] VCODEC_GET_CPU_LOADING_INFO, copy_to_user failed: %d\n", ret);
                return -EFAULT;
            }

            MFV_LOGD("[ROME] VCODEC_GET_CPU_LOADING_INFO -\n");
            break;
        }

        case VCODEC_GET_CORE_LOADING:
        {
            MFV_LOGD("[ROME] VCODEC_GET_CORE_LOADING + - tid = %d\n", current->pid);

            user_data_addr = (VAL_UINT8_T *)arg;
            ret = copy_from_user(&rTempCoreLoading, user_data_addr, sizeof(VAL_VCODEC_CORE_LOADING_T));
            if (ret)
            {
                MFV_LOGE("[ERROR] VCODEC_GET_CORE_LOADING, copy_from_user failed: %d\n", ret);
                return -EFAULT;
            }
            rTempCoreLoading.Loading = get_cpu_load(rTempCoreLoading.CPUid);
            ret = copy_to_user(user_data_addr, &rTempCoreLoading, sizeof(VAL_VCODEC_CORE_LOADING_T));
            if (ret) {
                MFV_LOGE("[ERROR] VCODEC_GET_CORE_LOADING, copy_to_user failed: %d\n", ret);
                return -EFAULT;
            }

            MFV_LOGD("[ROME] VCODEC_GET_CORE_LOADING - - tid = %d\n", current->pid);
            break;
        }

        case VCODEC_GET_CORE_NUMBER:
        {
            MFV_LOGD("[ROME] VCODEC_GET_CORE_NUMBER + - tid = %d\n", current->pid);

            user_data_addr = (VAL_UINT8_T *)arg;
            temp_nr_cpu_ids = nr_cpu_ids;
            ret = copy_to_user(user_data_addr, &temp_nr_cpu_ids, sizeof(int));
            if (ret) {
                MFV_LOGE("[ERROR] VCODEC_GET_CORE_NUMBER, copy_to_user failed: %d\n", ret);
                return -EFAULT;
            }
            MFV_LOGD("[ROME] VCODEC_GET_CORE_NUMBER - - tid = %d\n", current->pid);
            break;
        }
        case VCODEC_SET_CPU_OPP_LIMIT:
            MFV_LOGE("[ROME] VCODEC_SET_CPU_OPP_LIMIT [EMPTY] + - tid = %d\n", current->pid);
            user_data_addr = (VAL_UINT8_T *)arg;
            ret = copy_from_user(&rCpuOppLimit, user_data_addr, sizeof(VAL_VCODEC_CPU_OPP_LIMIT_T));
            if (ret) {
                MFV_LOGE("[ERROR] VCODEC_SET_CPU_OPP_LIMIT, copy_from_user failed: %d\n", ret);
                return -EFAULT;
            }
            MFV_LOGE("+VCODEC_SET_CPU_OPP_LIMIT (%d, %d, %d), tid = %d\n", rCpuOppLimit.limited_freq, rCpuOppLimit.limited_cpu, rCpuOppLimit.enable, current->pid);
            // TODO: Check if cpu_opp_limit is available
            //ret = cpu_opp_limit(EVENT_VIDEO, rCpuOppLimit.limited_freq, rCpuOppLimit.limited_cpu, rCpuOppLimit.enable); // 0: PASS, other: FAIL
            if (ret) {
                MFV_LOGE("[ERROR] cpu_opp_limit failed: %d\n", ret);
                return -EFAULT;
            }
            MFV_LOGE("-VCODEC_SET_CPU_OPP_LIMIT tid = %d, ret = %d\n", current->pid, ret);
            MFV_LOGE("[ROME] VCODEC_SET_CPU_OPP_LIMIT [EMPTY] - - tid = %d\n", current->pid);
            break;
        case VCODEC_MB:
            mb();
            break;
#if 0
        case MFV_SET_CMD_CMD:
            MFV_LOGD("[MFV] MFV_SET_CMD_CMD\n");
            MFV_LOGD("[MFV] Arg = %x\n",arg);
            user_data_addr = (VAL_UINT8_T *)arg;
            ret = copy_from_user(&rDrvCmdQueue, user_data_addr, sizeof(VCODEC_DRV_CMD_QUEUE_T));
            MFV_LOGD("[MFV] CmdNum = %d\n",rDrvCmdQueue.CmdNum);
            u4Size = (rDrvCmdQueue.CmdNum)*sizeof(VCODEC_DRV_CMD_T);

            cmd_queue = (P_VCODEC_DRV_CMD_T)kmalloc(u4Size,GFP_ATOMIC);
            if (cmd_queue != VAL_NULL && rDrvCmdQueue.pCmd != VAL_NULL) {
                ret = copy_from_user(cmd_queue, rDrvCmdQueue.pCmd, u4Size);
                while (cmd_queue->type != END_CMD) {
                    switch (cmd_queue->type)
                    {
                        case ENABLE_HW_CMD:
                            break;
                        case DISABLE_HW_CMD:
                            break;
                        case WRITE_REG_CMD:
                            VDO_HW_WRITE(cmd_queue->address + cmd_queue->offset, cmd_queue->value);
                            break;
                        case READ_REG_CMD:
                            uValue = VDO_HW_READ(cmd_queue->address + cmd_queue->offset);
                            copy_to_user((void *)cmd_queue->value, &uValue, sizeof(VAL_UINT32_T));
                            break;
                        case WRITE_SYSRAM_CMD:
                            VDO_HW_WRITE(cmd_queue->address + cmd_queue->offset, cmd_queue->value);
                            break;
                        case READ_SYSRAM_CMD:
                            uValue = VDO_HW_READ(cmd_queue->address + cmd_queue->offset);
                            copy_to_user((void *)cmd_queue->value, &uValue, sizeof(VAL_UINT32_T));
                            break;
                        case MASTER_WRITE_CMD:
                            uValue = VDO_HW_READ(cmd_queue->address + cmd_queue->offset);
                            VDO_HW_WRITE(cmd_queue->address + cmd_queue->offset, cmd_queue->value | (uValue & cmd_queue->mask));
                            break;
                        case SETUP_ISR_CMD:
                            break;
                        case WAIT_ISR_CMD:
                            MFV_LOGD("HAL_CMD_SET_CMD_QUEUE: WAIT_ISR_CMD+\n"); 
                            
                            MFV_LOGD("HAL_CMD_SET_CMD_QUEUE: WAIT_ISR_CMD-\n"); 
                            break;
                        case TIMEOUT_CMD:
                            break;
                        case WRITE_SYSRAM_RANGE_CMD:
                            break;
                        case READ_SYSRAM_RANGE_CMD:
                            break;
                        case POLL_REG_STATUS_CMD:
                            uValue = VDO_HW_READ(cmd_queue->address + cmd_queue->offset);
                            nCount = 0;
                            while ((uValue & cmd_queue->mask) != 0) {
                                nCount++;
                                if (nCount > 1000) {
                                    break;
                                }
                                uValue = VDO_HW_READ(cmd_queue->address + cmd_queue->offset);
                            }
                            break;
                        default:
                            break;
                    }
                    cmd_queue++;
                }

            }
            break;
#endif
        default:
            MFV_LOGE("========[ERROR] vcodec_ioctl default case======== %u\n", cmd);
        break;
    }
    return 0xFF;
}

static int vcodec_open(struct inode *inode, struct file *file)
{
    MFV_LOGD("[VCODEC_DEBUG] vcodec_open\n");

    mutex_lock(&DriverOpenCountLock);
    ///MEIZU_BSP{@
    /*disable soidle when open video codec*/
    if(MT6582Driver_Open_Count == 0){
        idle_lock_spm(IDLE_SPM_LOCK_VIDEO_CODEC);
    }
    ///@}
    MT6582Driver_Open_Count++;

    MFV_LOGD("vcodec_open pid = %d, MT6582Driver_Open_Count %d\n", current->pid, MT6582Driver_Open_Count);
    mutex_unlock(&DriverOpenCountLock);


    // TODO: Check upper limit of concurrent users?

    return 0;
}

static int vcodec_flush(struct file *file, fl_owner_t id)
{
    unsigned long ulFlagsLockHW, ulFlagsISR;

    //dump_stack();
    MFV_LOGD("[VCODEC_DEBUG] vcodec_flush, curr_tid =%d\n", current->pid);
    mutex_lock(&DriverOpenCountLock);
    MFV_LOGD("vcodec_flush pid = %d, MT6582Driver_Open_Count %d\n", current->pid, MT6582Driver_Open_Count);

    MT6582Driver_Open_Count--;

    if (MT6582Driver_Open_Count == 0) {

        mutex_lock(&VdecHWLock);
        gu4VdecLockThreadId = 0;
        grVcodecDecHWLock.pvHandle = 0;
        grVcodecDecHWLock.eDriverType = VAL_DRIVER_TYPE_NONE;
        grVcodecDecHWLock.rLockedTime.u4Sec = 0;
        grVcodecDecHWLock.rLockedTime.u4uSec = 0;
        mutex_unlock(&VdecHWLock);

        mutex_lock(&VencHWLock);
        grVcodecEncHWLock.pvHandle = 0;
        grVcodecEncHWLock.eDriverType = VAL_DRIVER_TYPE_NONE;
        grVcodecEncHWLock.rLockedTime.u4Sec = 0;
        grVcodecEncHWLock.rLockedTime.u4uSec = 0;
        mutex_unlock(&VencHWLock);

        mutex_lock(&DecEMILock);
        gu4DecEMICounter = 0;
        mutex_unlock(&DecEMILock);

        mutex_lock(&EncEMILock);
        gu4EncEMICounter = 0;
        mutex_unlock(&EncEMILock);

        mutex_lock(&PWRLock);
        gu4PWRCounter = 0;
        mutex_unlock(&PWRLock);

#ifdef VDEC_USE_L2C
        mutex_lock(&L2CLock);
        if (gu4L2CCounter != 0)
        {
            MFV_LOGD("vcodec_flush pid = %d, L2 user = %d, force restore L2 settings\n", current->pid, gu4L2CCounter);
            if (config_L2(SZ_512K))
            {
                MFV_LOGE("restore L2 settings failed\n");
            }
        }
        gu4L2CCounter = 0;
        mutex_unlock(&L2CLock);
#endif
        spin_lock_irqsave(&LockDecHWCountLock, ulFlagsLockHW);
        gu4LockDecHWCount = 0;
        spin_unlock_irqrestore(&LockDecHWCountLock, ulFlagsLockHW);

        spin_lock_irqsave(&LockEncHWCountLock, ulFlagsLockHW);
        gu4LockEncHWCount = 0;
        spin_unlock_irqrestore(&LockEncHWCountLock, ulFlagsLockHW);
        
        spin_lock_irqsave(&DecISRCountLock, ulFlagsISR);
        gu4DecISRCount = 0;
        spin_unlock_irqrestore(&DecISRCountLock, ulFlagsISR);

        spin_lock_irqsave(&EncISRCountLock, ulFlagsISR);
        gu4EncISRCount = 0;
        spin_unlock_irqrestore(&EncISRCountLock, ulFlagsISR);        

        ///MEIZU_BSP{@
         idle_unlock_spm(IDLE_SPM_LOCK_VIDEO_CODEC);
        ///@}
    }
    mutex_unlock(&DriverOpenCountLock);

    return 0;
}

static int vcodec_release(struct inode *inode, struct file *file)
{
    MFV_LOGD("[VCODEC_DEBUG] vcodec_release, curr_tid =%d\n", current->pid);

    return 0;
}

void vcodec_vma_open(struct vm_area_struct *vma)
{
    MFV_LOGD("vcodec VMA open, virt %lx, phys %lx\n", vma->vm_start, vma->vm_pgoff << PAGE_SHIFT);
}

void vcodec_vma_close(struct vm_area_struct *vma)
{
     MFV_LOGD("vcodec VMA close, virt %lx, phys %lx\n", vma->vm_start, vma->vm_pgoff << PAGE_SHIFT);
}

static struct vm_operations_struct vcodec_remap_vm_ops = {
    .open = vcodec_vma_open,
    .close = vcodec_vma_close,
};

static int vcodec_mmap(struct file* file, struct vm_area_struct* vma)
{
    VAL_UINT32_T u4I = 0;
    long length;
    unsigned int pfn;

    length = vma->vm_end - vma->vm_start;
    pfn = vma->vm_pgoff<<PAGE_SHIFT;
    
    if(((length > VENC_REGION) || (pfn < VENC_BASE) || (pfn > VENC_BASE+VENC_REGION)) &&
       ((length > VDEC_REGION) || (pfn < VDEC_BASE_PHY) || (pfn > VDEC_BASE_PHY+VDEC_REGION)) &&
	   ((length > HW_REGION) || (pfn < HW_BASE) || (pfn > HW_BASE+HW_REGION)) &&
	   ((length > INFO_REGION) || (pfn < INFO_BASE) || (pfn > INFO_BASE+INFO_REGION))
      )
    {
        VAL_UINT32_T u4Addr, u4Size;
        for(u4I = 0; u4I < VCODEC_MULTIPLE_INSTANCE_NUM_x_10; u4I++)
        {
            if ((grNonCacheMemoryList[u4I].u4KVA != 0xffffffff) && (grNonCacheMemoryList[u4I].u4KPA != 0xffffffff))
            {
                u4Addr = grNonCacheMemoryList[u4I].u4KPA;
                u4Size = (grNonCacheMemoryList[u4I].u4Size + 0x1000 -1) & ~(0x1000-1);
                if((length == u4Size) && (pfn == u4Addr))
                {
                    MFV_LOGD(" cache idx %d \n", u4I);
                    break;
                }
            }
        }

        if (u4I == VCODEC_MULTIPLE_INSTANCE_NUM_x_10)
        {
            MFV_LOGE("[ERROR] mmap region error: Length(0x%x), pfn(0x%x)\n", length, pfn);
            return -EAGAIN;
        }
    }

    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
    MFV_LOGD("[mmap] vma->start 0x%x, vma->end 0x%x, vma->pgoff 0x%x\n", 
             (unsigned int)vma->vm_start, (unsigned int)vma->vm_end, (unsigned int)vma->vm_pgoff);
    if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
        vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
        return -EAGAIN;
    }

    vma->vm_ops = &vcodec_remap_vm_ops;
    vcodec_vma_open(vma);

    return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void vcodec_early_suspend(struct early_suspend *h)
{
    mutex_lock(&PWRLock);
    MFV_LOGD("vcodec_early_suspend, tid = %d, PWR_USER = %d\n", current->pid, gu4PWRCounter);
    mutex_unlock(&PWRLock);
/*
    if (gu4PWRCounter != 0)
    {
        MFV_LOGE("[MT6589_VCodec_early_suspend] Someone Use HW, Disable Power!\n");
        disable_clock(MT65XX_PDN_MM_VBUF, "Video_VBUF");
        disable_clock(MT_CG_VDEC0_VDE, "VideoDec");
        disable_clock(MT_CG_VENC_VEN, "VideoEnc");
        disable_clock(MT65XX_PDN_MM_GDC_SHARE_MACRO, "VideoEnc");
    }
*/
    MFV_LOGD("vcodec_early_suspend - tid = %d\n", current->pid);
}

static void vcodec_late_resume(struct early_suspend *h)
{
    mutex_lock(&PWRLock);
    MFV_LOGD("vcodec_late_resume, tid = %d, PWR_USER = %d\n", current->pid, gu4PWRCounter);
    mutex_unlock(&PWRLock);
/*
    if (gu4PWRCounter != 0)
    {
        MFV_LOGE("[vcodec_late_resume] Someone Use HW, Enable Power!\n");
        enable_clock(MT65XX_PDN_MM_VBUF, "Video_VBUF");
        enable_clock(MT_CG_VDEC0_VDE, "VideoDec");
        enable_clock(MT_CG_VENC_VEN, "VideoEnc");
        enable_clock(MT65XX_PDN_MM_GDC_SHARE_MACRO, "VideoEnc");
    }
*/
    MFV_LOGD("vcodec_late_resume - tid = %d\n", current->pid);
}

static struct early_suspend vcodec_early_suspend_handler =
{
    .level = (EARLY_SUSPEND_LEVEL_DISABLE_FB - 1),
    .suspend = vcodec_early_suspend,
    .resume = vcodec_late_resume,
};
#endif

static struct file_operations vcodec_fops = {
    .owner      = THIS_MODULE,
    .unlocked_ioctl = vcodec_unlocked_ioctl,
    .open       = vcodec_open,
    .flush      = vcodec_flush,
    .release    = vcodec_release,
    .mmap       = vcodec_mmap,
};

static int vcodec_probe(struct platform_device *dev)
{
    int ret;
    MFV_LOGD("+vcodec_probe\n");

    mutex_lock(&DecEMILock);
    gu4DecEMICounter = 0;
    mutex_unlock(&DecEMILock);

    mutex_lock(&EncEMILock);
    gu4EncEMICounter = 0;
    mutex_unlock(&EncEMILock);

    mutex_lock(&PWRLock);
    gu4PWRCounter = 0;
    mutex_unlock(&PWRLock);

    mutex_lock(&L2CLock);
    gu4L2CCounter = 0;
    mutex_unlock(&L2CLock);

    ret = register_chrdev_region(vcodec_devno, 1, VCODEC_DEVNAME);
    if (ret) {
        MFV_LOGD("[VCODEC_DEBUG][ERROR] Can't Get Major number for VCodec Device\n");
    }

    vcodec_cdev = cdev_alloc();
    vcodec_cdev->owner = THIS_MODULE;
    vcodec_cdev->ops = &vcodec_fops;

    ret = cdev_add(vcodec_cdev, vcodec_devno, 1);

    //if (request_irq(MT_VDEC_IRQ_ID , (irq_handler_t)video_intr_dlr, IRQF_TRIGGER_LOW, VCODEC_DEVNAME, NULL) < 0)
    if (request_irq(VDEC_IRQ_BIT_ID , (irq_handler_t)video_intr_dlr, IRQF_TRIGGER_LOW, VCODEC_DEVNAME, NULL) < 0)
    {
       MFV_LOGD("[VCODEC_DEBUG][ERROR] error to request dec irq\n"); 
    }
    else
    {
       MFV_LOGD("[VCODEC_DEBUG] success to request dec irq\n");
    }

    //if (request_irq(MT_VENC_IRQ_ID , (irq_handler_t)video_intr_dlr2, IRQF_TRIGGER_LOW, VCODEC_DEVNAME, NULL) < 0)
    if (request_irq(VENC_IRQ_BIT_ID , (irq_handler_t)video_intr_dlr2, IRQF_TRIGGER_LOW, VCODEC_DEVNAME, NULL) < 0)
    {
       MFV_LOGD("[VCODEC_DEBUG][ERROR] error to request enc irq\n"); 
    }
    else
    {
       MFV_LOGD("[VCODEC_DEBUG] success to request enc irq\n");
    }

    //disable_irq(MT_VDEC_IRQ_ID);
    disable_irq(VDEC_IRQ_BIT_ID);
    //disable_irq(MT_VENC_IRQ_ID);
    disable_irq(VENC_IRQ_BIT_ID);

    MFV_LOGD("[VCODEC_DEBUG] vcodec_probe Done\n");

    return 0;
}

#ifdef CONFIG_MTK_HIBERNATION
extern void mt_irq_set_sens(unsigned int irq, unsigned int sens);
extern void mt_irq_set_polarity(unsigned int irq, unsigned int polarity);
static int vcodec_pm_restore_noirq(struct device *device)
{
    // vdec : IRQF_TRIGGER_LOW
    mt_irq_set_sens(VDEC_IRQ_BIT_ID, MT_LEVEL_SENSITIVE);
    mt_irq_set_polarity(VDEC_IRQ_BIT_ID, MT_POLARITY_LOW);
    // venc: IRQF_TRIGGER_LOW
    mt_irq_set_sens(VENC_IRQ_BIT_ID, MT_LEVEL_SENSITIVE);
    mt_irq_set_polarity(VENC_IRQ_BIT_ID, MT_POLARITY_LOW);

    return 0;
}
#endif

static int __init vcodec_driver_init(void)
{
    VAL_RESULT_T  eValHWLockRet;
    unsigned long ulFlags, ulFlagsLockHW, ulFlagsISR;

    MFV_LOGD("+vcodec_init !!\n");

    MT6582Driver_Open_Count = 0;

    KVA_VENC_IRQ_STATUS_ADDR =  (int)ioremap(VENC_IRQ_STATUS_addr, 4);
    KVA_VENC_IRQ_ACK_ADDR  = (int)ioremap(VENC_IRQ_ACK_addr, 4);
#ifdef VENC_PWR_FPGA
    KVA_VENC_CLK_CFG_0_ADDR = (int)ioremap(CLK_CFG_0_addr, 4);
    KVA_VENC_CLK_CFG_4_ADDR = (int)ioremap(CLK_CFG_4_addr, 4);
    KVA_VENC_PWR_ADDR = (int)ioremap(VENC_PWR_addr, 4);
    KVA_VENCSYS_CG_SET_ADDR = (int)ioremap(VENCSYS_CG_SET_addr, 4);
#endif

    spin_lock_irqsave(&LockDecHWCountLock, ulFlagsLockHW);
    gu4LockDecHWCount = 0;
    spin_unlock_irqrestore(&LockDecHWCountLock, ulFlagsLockHW);
    
    spin_lock_irqsave(&LockEncHWCountLock, ulFlagsLockHW);
    gu4LockEncHWCount = 0;
    spin_unlock_irqrestore(&LockEncHWCountLock, ulFlagsLockHW);
    
    spin_lock_irqsave(&DecISRCountLock, ulFlagsISR);
    gu4DecISRCount = 0;
    spin_unlock_irqrestore(&DecISRCountLock, ulFlagsISR);
    
    spin_lock_irqsave(&EncISRCountLock, ulFlagsISR);
    gu4EncISRCount = 0;
    spin_unlock_irqrestore(&EncISRCountLock, ulFlagsISR);
    
    mutex_lock(&VdecPWRLock);
    gu4VdecPWRCounter = 0;
    mutex_unlock(&VdecPWRLock);

    mutex_lock(&VencPWRLock);
    gu4VencPWRCounter = 0;
    mutex_unlock(&VencPWRLock);

    mutex_lock(&IsOpenedLock);
    if (VAL_FALSE == bIsOpened) {
        bIsOpened = VAL_TRUE;
        vcodec_probe(NULL);
    }
    mutex_unlock(&IsOpenedLock);

    mutex_lock(&VdecHWLock);
    gu4VdecLockThreadId = 0;
    grVcodecDecHWLock.pvHandle = 0;
    grVcodecDecHWLock.eDriverType = VAL_DRIVER_TYPE_NONE;
    grVcodecDecHWLock.rLockedTime.u4Sec = 0;
    grVcodecDecHWLock.rLockedTime.u4uSec = 0;
    mutex_unlock(&VdecHWLock);

    mutex_lock(&VencHWLock);
    grVcodecEncHWLock.pvHandle = 0;
    grVcodecEncHWLock.eDriverType = VAL_DRIVER_TYPE_NONE;
    grVcodecEncHWLock.rLockedTime.u4Sec = 0;
    grVcodecEncHWLock.rLockedTime.u4uSec = 0;
    mutex_unlock(&VencHWLock);

    //MT6582_HWLockEvent part
    mutex_lock(&DecHWLockEventTimeoutLock);
    DecHWLockEvent.pvHandle = "DECHWLOCK_EVENT";
    DecHWLockEvent.u4HandleSize = sizeof("DECHWLOCK_EVENT")+1;
    DecHWLockEvent.u4TimeoutMs = 1;
    mutex_unlock(&DecHWLockEventTimeoutLock);
    eValHWLockRet = eVideoCreateEvent(&DecHWLockEvent, sizeof(VAL_EVENT_T));
    if (VAL_RESULT_NO_ERROR != eValHWLockRet) {
        MFV_LOGE("[MFV][ERROR] create dec hwlock event error\n");
    }

    mutex_lock(&EncHWLockEventTimeoutLock);
    EncHWLockEvent.pvHandle = "ENCHWLOCK_EVENT";
    EncHWLockEvent.u4HandleSize = sizeof("ENCHWLOCK_EVENT")+1;
    EncHWLockEvent.u4TimeoutMs = 1;
    mutex_unlock(&EncHWLockEventTimeoutLock);
    eValHWLockRet = eVideoCreateEvent(&EncHWLockEvent, sizeof(VAL_EVENT_T));
    if (VAL_RESULT_NO_ERROR != eValHWLockRet) {
        MFV_LOGE("[MFV][ERROR] create enc hwlock event error\n");
    }

    //MT6582_IsrEvent part
    spin_lock_irqsave(&DecIsrLock, ulFlags);
    DecIsrEvent.pvHandle = "DECISR_EVENT";
    DecIsrEvent.u4HandleSize = sizeof("DECISR_EVENT")+1;
    DecIsrEvent.u4TimeoutMs = 1;
    spin_unlock_irqrestore(&DecIsrLock, ulFlags);
    eValHWLockRet = eVideoCreateEvent(&DecIsrEvent, sizeof(VAL_EVENT_T));
    if(VAL_RESULT_NO_ERROR != eValHWLockRet)
    {
        MFV_LOGE("[MFV][ERROR] create dec isr event error\n");
    }

    spin_lock_irqsave(&EncIsrLock, ulFlags);
    EncIsrEvent.pvHandle = "ENCISR_EVENT";
    EncIsrEvent.u4HandleSize = sizeof("ENCISR_EVENT")+1;
    EncIsrEvent.u4TimeoutMs = 1;
    spin_unlock_irqrestore(&EncIsrLock, ulFlags);
    eValHWLockRet = eVideoCreateEvent(&EncIsrEvent, sizeof(VAL_EVENT_T));
    if(VAL_RESULT_NO_ERROR != eValHWLockRet)
    {
        MFV_LOGE("[MFV][ERROR] create enc isr event error\n");
    }
    
    MFV_LOGD("[VCODEC_DEBUG] vcodec_driver_init Done\n");

#ifdef CONFIG_HAS_EARLYSUSPEND
    register_early_suspend(&vcodec_early_suspend_handler);
#endif

#ifdef CONFIG_MTK_HIBERNATION
    register_swsusp_restore_noirq_func(ID_M_VCODEC, vcodec_pm_restore_noirq, NULL);
#endif

    return 0;
}

static void __exit vcodec_driver_exit(void)
{
    VAL_RESULT_T  eValHWLockRet;

    MFV_LOGD("[VCODEC_DEBUG] mflexvideo_driver_exit\n");

    mutex_lock(&IsOpenedLock);
    if (VAL_TRUE == bIsOpened) {
        bIsOpened = VAL_FALSE;
    }
    mutex_unlock(&IsOpenedLock);

    cdev_del(vcodec_cdev);
    unregister_chrdev_region(vcodec_devno, 1);

    // [TODO] iounmap the following?
    iounmap((void*)KVA_VENC_IRQ_STATUS_ADDR);
    iounmap((void*)KVA_VENC_IRQ_ACK_ADDR);   
#ifdef VENC_PWR_FPGA
    iounmap((void*)KVA_VENC_CLK_CFG_0_ADDR);
    iounmap((void*)KVA_VENC_CLK_CFG_4_ADDR);
    iounmap((void*)KVA_VENC_PWR_ADDR);
    iounmap((void*)KVA_VENCSYS_CG_SET_ADDR);
#endif

    // [TODO] free IRQ here
    //free_irq(MT_VENC_IRQ_ID, NULL);
    free_irq(VENC_IRQ_BIT_ID, NULL);
    //free_irq(MT_VDEC_IRQ_ID, NULL);
    free_irq(VDEC_IRQ_BIT_ID, NULL);


    //MT6589_HWLockEvent part
    eValHWLockRet = eVideoCloseEvent(&DecHWLockEvent, sizeof(VAL_EVENT_T));
    if (VAL_RESULT_NO_ERROR != eValHWLockRet) {
        MFV_LOGE("[MFV][ERROR] close dec hwlock event error\n");
    }

    eValHWLockRet = eVideoCloseEvent(&EncHWLockEvent, sizeof(VAL_EVENT_T));
    if (VAL_RESULT_NO_ERROR != eValHWLockRet) {
        MFV_LOGE("[MFV][ERROR] close enc hwlock event error\n");
    }

    //MT6589_IsrEvent part
    eValHWLockRet = eVideoCloseEvent(&DecIsrEvent, sizeof(VAL_EVENT_T));
    if (VAL_RESULT_NO_ERROR != eValHWLockRet) {
        MFV_LOGE("[MFV][ERROR] close dec isr event error\n");
    }

    eValHWLockRet = eVideoCloseEvent(&EncIsrEvent, sizeof(VAL_EVENT_T));
    if (VAL_RESULT_NO_ERROR != eValHWLockRet) {
        MFV_LOGE("[MFV][ERROR] close enc isr event error\n");
    }



#ifdef CONFIG_HAS_EARLYSUSPEND
    unregister_early_suspend(&vcodec_early_suspend_handler);
#endif

#ifdef CONFIG_MTK_HIBERNATION
    unregister_swsusp_restore_noirq_func(ID_M_VCODEC);
#endif
}

module_init(vcodec_driver_init);
module_exit(vcodec_driver_exit);
MODULE_AUTHOR("Legis, Lu <legis.lu@mediatek.com>");
MODULE_DESCRIPTION("ROME Vcodec Driver");
MODULE_LICENSE("GPL");

