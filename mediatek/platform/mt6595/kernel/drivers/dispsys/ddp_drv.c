#define LOG_TAG "ddp_drv"

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <generated/autoconf.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/param.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/timer.h>

#include <mach/mt_smi.h>

#include <linux/xlog.h>
#include <linux/proc_fs.h>  //proc file use
//ION
#include <linux/ion.h>
#include <linux/ion_drv.h>
//#include <mach/m4u.h>

#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>

#include <asm/io.h>


#include <mach/irqs.h>
#include <mach/mt_reg_base.h>
#include <mach/mt_irq.h>
#include <mach/irqs.h>
#include <mach/mt_clkmgr.h> // ????
#include <mach/mt_irq.h>
#include <mach/sync_write.h>

#include "cmdq_record.h"
#include "cmdq_core.h"

#include "ddp_drv.h"
#include "ddp_reg.h"
#include "ddp_hal.h"
#include "ddp_path.h"
#include "ddp_debug.h"
#include "ddp_color.h"
#include "disp_drv_ddp.h"
#include "ddp_wdma.h"
#include "ddp_aal.h"
#include "cmdq_def.h"
#include "cmdq_record.h"
#include "cmdq_core.h"
#include "ddp_log.h"
#include "ddp_irq.h"

unsigned int dbg_log = 0;
unsigned int irq_log = 0;  // must disable irq level log by default, else will block uart output, open it only for debug use
unsigned int irq_err_log = 1;

#define DISP_DEVNAME "mtk_disp"
// device and driver
static dev_t disp_devno;
static struct cdev *disp_cdev;
static struct class *disp_class = NULL;

//NCSTool for Color Tuning
unsigned char ncs_tuning_mode = 0;

//flag for gamma lut update
unsigned char bls_gamma_dirty = 0;

//-------------------------------------------------------------------------------//
// global variables

typedef struct
{
    pid_t open_pid;
    pid_t open_tgid;
    struct list_head testList;
    unsigned int u4LockedMutex;
    unsigned int u4LockedResource;
    unsigned int u4Clock;
    spinlock_t node_lock;
} disp_node_struct;

static DECLARE_WAIT_QUEUE_HEAD(g_disp_irq_done_queue);
static DECLARE_WAIT_QUEUE_HEAD(gMutexWaitQueue);

unsigned int cmdqDdpClockOn(uint64_t engineFlag);
unsigned int cmdqDdpClockOff(uint64_t engineFlag);
unsigned int cmdqDdpDumpInfo(uint64_t engineFlag, char *pOutBuf, unsigned int bufSize);
unsigned int cmdqDdpResetEng(uint64_t engineFlag);
unsigned int g_cmdq_mode = 0;
unsigned int g_cmdq_index = 0;
cmdqRecHandle g_cmdq_handle;

// Hardware Mutex Variables
#define ENGINE_MUTEX_NUM 8
spinlock_t gMutexLock;
int mutex_used[ENGINE_MUTEX_NUM] = {1, 1, 1, 1, 0, 0, 0, 0};    // 0 for FB, 1 for Bitblt, 2 for HDMI, 3 for BLS

//G2d Variables
spinlock_t gResourceLock;
unsigned int gLockedResource;//lock dpEngineType_6582
static DECLARE_WAIT_QUEUE_HEAD(gResourceWaitQueue);

// Overlay Variables
spinlock_t gOvlLock;
int disp_run_dp_framework = 0;
int disp_layer_enable = 0;
int disp_mutex_status = 0;

DISP_OVL_INFO disp_layer_info[DDP_OVL_LAYER_MUN];

//AAL variables
static unsigned long u4UpdateFlag = 0;

//Register update lock
spinlock_t gRegisterUpdateLock;
spinlock_t gPowerOperateLock;
//Clock gate management
//static unsigned long g_u4ClockOnTbl = 0;

//PQ variables
 UINT32 fb_width;
 UINT32 fb_height;

static DISPLAY_TDSHP_T g_TDSHP_Index;

// internal function
static int disp_get_mutex_status(void);
static int disp_set_needupdate(DISP_MODULE_ENUM eModule , unsigned long u4En);
static void disp_power_off(DISP_MODULE_ENUM eModule , unsigned int * pu4Record);
static void disp_power_on(DISP_MODULE_ENUM eModule , unsigned int * pu4Record);
extern void DpEngine_COLORonConfig(int id, unsigned int srcWidth,unsigned int srcHeight);
extern void DpEngine_COLORonInit(int id);
//-------------------------------------------------------------------------------//
// functions

static unsigned int ddp_ms2jiffies(unsigned long ms)
{
    return ((ms*HZ + 512) >> 10);
}

#if 1
int disp_lock_mutex(void)
{
    int id = -1;
    int i;
    spin_lock(&gMutexLock);

    for(i = 0 ; i < ENGINE_MUTEX_NUM ; i++)
        if(mutex_used[i] == 0)
        {
            id = i;
            mutex_used[i] = 1;
            //DISP_REG_SET_FIELD((1 << i) , DISP_REG_CONFIG_MUTEX_INTEN , 1);
            break;
        }
    spin_unlock(&gMutexLock);

    return id;
}

void ddp_enable_cmdq(int enable)
{
	DDPMSG("%s, enable=%d\n", __func__, enable);
	g_cmdq_mode = enable;
}

void ddp_set_cmdq(void *handle)
{
	DDPMSG("%s, handle=0x%08x\n", __func__, (unsigned int)handle);

	g_cmdq_handle = handle;
}

int disp_unlock_mutex(int id)
{
    if(id < 0 && id >= ENGINE_MUTEX_NUM)
        return -1;

    spin_lock(&gMutexLock);

    mutex_used[id] = 0;
    //DISP_REG_SET_FIELD((1 << id) , DISP_REG_CONFIG_MUTEX_INTEN , 0);

    spin_unlock(&gMutexLock);

    return 0;
}
#endif //0

static void disp_power_on(DISP_MODULE_ENUM eModule , unsigned int * pu4Record)
{
    unsigned long flag;
    //unsigned int ret = 0;
    spin_lock_irqsave(&gPowerOperateLock , flag);


    spin_unlock_irqrestore(&gPowerOperateLock , flag);
}

static void disp_power_off(DISP_MODULE_ENUM eModule , unsigned int * pu4Record)
{
    unsigned long flag;
    //unsigned int ret = 0;
    spin_lock_irqsave(&gPowerOperateLock , flag);


    spin_unlock_irqrestore(&gPowerOperateLock , flag);
}

unsigned int inAddr=0, outAddr=0;

static int disp_set_needupdate(DISP_MODULE_ENUM eModule , unsigned long u4En)
{
    unsigned long flag;
    spin_lock_irqsave(&gRegisterUpdateLock , flag);

    if(u4En)
    {
        u4UpdateFlag |= (1 << eModule);
    }
    else
    {
        u4UpdateFlag &= ~(1 << eModule);
    }

    spin_unlock_irqrestore(&gRegisterUpdateLock , flag);

    return 0;
}

static long disp_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    return 0;

    DISP_WRITE_REG wParams;
    DISP_READ_REG rParams;
    DISP_EXEC_COMMAND cParams;
    unsigned int ret = 0;
    unsigned int value;
    int taskID;
    DISP_MODULE_ENUM module;
    DISP_OVL_INFO ovl_info;
    DISP_PQ_PARAM * pq_param;
    DISP_PQ_PARAM * pq_cam_param;
    DISP_PQ_PARAM * pq_gal_param;
    DISPLAY_PQ_T * pq_index;
    DISPLAY_TDSHP_T * tdshp_index;
    int layer, mutex_id;
    disp_wait_irq_struct wait_irq_struct;
    unsigned long lcmindex = 0;
    unsigned long flags;
    struct timeval tv;
    int count;

#ifdef DDP_DBG_DDP_PATH_CONFIG
    struct disp_path_config_struct config;
#endif

    disp_node_struct *pNode = (disp_node_struct *)file->private_data;

#if 0
    if(inAddr==0)
    {
        inAddr = kmalloc(800*480*4, GFP_KERNEL);
        memset((void*)inAddr, 0x55, 800*480*4);
        DDPMSG("inAddr=0x%x \n", inAddr);
    }
    if(outAddr==0)
    {
        outAddr = kmalloc(800*480*4, GFP_KERNEL);
        memset((void*)outAddr, 0xff, 800*480*4);
        DDPMSG("outAddr=0x%x \n", outAddr);
    }
#endif
    DDPDBG("cmd=0x%x, arg=0x%x \n", cmd, (unsigned int)arg);
    switch(cmd)
    {
        case DISP_IOCTL_WRITE_REG:

            if(copy_from_user(&wParams, (void *)arg, sizeof(DISP_WRITE_REG )))
            {
                DDPERR("DISP_IOCTL_WRITE_REG, copy_from_user failed\n");
                return -EFAULT;
            }

            DDPDBG("write  0x%x = 0x%x (0x%x)\n", wParams.reg, wParams.val, wParams.mask);
            if(wParams.reg>DISPSYS_REG_ADDR_MAX || wParams.reg<DISPSYS_REG_ADDR_MIN)
            {
                DDPERR("reg write, addr invalid, addr min=0x%x, max=0x%x, addr=0x%x \n",
                    DISPSYS_REG_ADDR_MIN,
                    DISPSYS_REG_ADDR_MAX,
                    wParams.reg);
                return -EFAULT;
            }
            DISP_CPU_REG_SET(wParams.reg,(*(volatile unsigned int*)wParams.reg & ~wParams.mask) | (wParams.val & wParams.mask));
            //*(volatile unsigned int*)wParams.reg = (*(volatile unsigned int*)wParams.reg & ~wParams.mask) | (wParams.val & wParams.mask);
            //mt65xx_reg_sync_writel(wParams.reg, value);
            break;

        case DISP_IOCTL_READ_REG:
            if(copy_from_user(&rParams, (void *)arg, sizeof(DISP_READ_REG)))
            {
                DDPERR("DISP_IOCTL_READ_REG, copy_from_user failed\n");
                return -EFAULT;
            }
            if(rParams.reg>DISPSYS_REG_ADDR_MAX || rParams.reg<DISPSYS_REG_ADDR_MIN)
            {
                DDPERR("reg read, addr invalid, addr min=0x%x, max=0x%x, addr=0x%x \n",
                    DISPSYS_REG_ADDR_MIN,
                    DISPSYS_REG_ADDR_MAX,
                    rParams.reg);
                return -EFAULT;
            }

            //value = (*(volatile unsigned int*)rParams.reg) & rParams.mask;
            value = DISP_REG_GET((*(volatile unsigned int*)rParams.reg) & rParams.mask);

            DDPDBG("read 0x%x = 0x%x (0x%x)\n", rParams.reg, value, rParams.mask);

            if(copy_to_user(rParams.val, &value, sizeof(unsigned int)))
            {
                DDPERR("DISP_IOCTL_READ_REG, copy_to_user failed\n");
                return -EFAULT;
            }
            break;

        case DISP_IOCTL_WAIT_IRQ:
            if(copy_from_user(&wait_irq_struct, (void*)arg , sizeof(wait_irq_struct)))
            {
                DDPERR("DISP_IOCTL_WAIT_IRQ, copy_from_user failed\n");
                return -EFAULT;
            }
            ret = disp_wait_intr(wait_irq_struct.module, wait_irq_struct.timeout_ms);
            break;

        case DISP_IOCTL_DUMP_REG:
            if(copy_from_user(&module, (void*)arg , sizeof(module)))
            {
                DDPERR("DISP_IOCTL_DUMP_REG, copy_from_user failed\n");
                return -EFAULT;
            }
            ret = disp_dump_reg(module);
            break;

        case DISP_IOCTL_LOCK_RESOURCE:
            if(copy_from_user(&mutex_id, (void*)arg , sizeof(int)))
            {
                DDPERR("DISP_IOCTL_LOCK_RESOURCE, copy_from_user failed\n");
                return -EFAULT;
            }
            if((-1) != mutex_id)
            {
                int ret = wait_event_interruptible_timeout(
                gResourceWaitQueue,
                (gLockedResource & (1 << mutex_id)) == 0,
                ddp_ms2jiffies(50) );

                if(ret <= 0)
                {
                    DDPERR("DISP_IOCTL_LOCK_RESOURCE, mutex_id 0x%x failed\n",gLockedResource);
                    return -EFAULT;
                }

                spin_lock(&gResourceLock);
                gLockedResource |= (1 << mutex_id);
                spin_unlock(&gResourceLock);

                spin_lock(&pNode->node_lock);
                pNode->u4LockedResource = gLockedResource;
                spin_unlock(&pNode->node_lock);
            }
            else
            {
                DDPERR("DISP_IOCTL_LOCK_RESOURCE, mutex_id = -1 failed\n");
                return -EFAULT;
            }
            break;


        case DISP_IOCTL_UNLOCK_RESOURCE:
            if(copy_from_user(&mutex_id, (void*)arg , sizeof(int)))
            {
                DDPERR("DISP_IOCTL_UNLOCK_RESOURCE, copy_from_user failed\n");
                return -EFAULT;
            }
            if((-1) != mutex_id)
            {
                spin_lock(&gResourceLock);
                gLockedResource &= ~(1 << mutex_id);
                spin_unlock(&gResourceLock);

                spin_lock(&pNode->node_lock);
                pNode->u4LockedResource = gLockedResource;
                spin_unlock(&pNode->node_lock);

                wake_up_interruptible(&gResourceWaitQueue);
            }
            else
            {
                DDPERR("DISP_IOCTL_UNLOCK_RESOURCE, mutex_id = -1 failed\n");
                return -EFAULT;
            }
            break;

    #if 1
        case DISP_IOCTL_LOCK_MUTEX:
        {
            wait_event_interruptible_timeout(
            gMutexWaitQueue,
            (mutex_id = disp_lock_mutex()) != -1,
            ddp_ms2jiffies(200) );

            if((-1) != mutex_id)
            {
                spin_lock(&pNode->node_lock);
                pNode->u4LockedMutex |= (1 << mutex_id);
                spin_unlock(&pNode->node_lock);
            }

            if(copy_to_user((void *)arg, &mutex_id, sizeof(int)))
            {
                DDPERR("disp driver : Copy to user error (mutex)\n");
                return -EFAULT;
            }
            break;
        }
        case DISP_IOCTL_UNLOCK_MUTEX:
            if(copy_from_user(&mutex_id, (void*)arg , sizeof(int)))
            {
                DDPERR("DISP_IOCTL_UNLOCK_MUTEX, copy_from_user failed\n");
                return -EFAULT;
            }
            disp_unlock_mutex(mutex_id);

            if((-1) != mutex_id)
            {
                spin_lock(&pNode->node_lock);
                pNode->u4LockedMutex &= ~(1 << mutex_id);
                spin_unlock(&pNode->node_lock);
            }

            wake_up_interruptible(&gMutexWaitQueue);

            break;
    #endif // 0

        case DISP_IOCTL_SYNC_REG:
            mb();
            break;

        case DISP_IOCTL_SET_INTR:
            DDPDBG("DISP_IOCTL_SET_INTR! \n");
            if(copy_from_user(&value, (void*)arg , sizeof(int)))
            {
                DDPERR("DISP_IOCTL_SET_INTR, copy_from_user failed\n");
                return -EFAULT;
            }

            // enable intr
            if( (value&0xffff0000) !=0)
            {
                disable_irq(value&0xff);
                printk("disable_irq %d \n", value&0xff);
            }
            else
            {
                disp_register_irq(value&0xff,DISP_DEVNAME);
                printk("enable irq: %d \n", value&0xff);
            }
            break;

         case DISP_IOCTL_CHECK_OVL:
            DDPDBG("DISP_IOCTL_CHECK_OVL! \n");
            value = disp_layer_enable;

            if(copy_to_user((void *)arg, &value, sizeof(int)))
            {
                DDPERR("disp driver : Copy to user error (result)\n");
                return -EFAULT;
            }
            break;

        case DISP_IOCTL_GET_OVL:
            DDPDBG("DISP_IOCTL_GET_OVL! \n");
            if(copy_from_user(&ovl_info, (void*)arg , sizeof(DISP_OVL_INFO)))
            {
                DDPERR("DISP_IOCTL_SET_INTR, copy_from_user failed, %d\n", ret);
                return -EFAULT;
            }

            layer = ovl_info.layer;

            spin_lock(&gOvlLock);
            ovl_info = disp_layer_info[layer];
            spin_unlock(&gOvlLock);

            if(copy_to_user((void *)arg, &ovl_info, sizeof(DISP_OVL_INFO)))
            {
                DDPERR("disp driver : Copy to user error (result)\n");
                return -EFAULT;
            }

            break;

         case DISP_IOCTL_SET_CLKON:
            if(copy_from_user(&module, (void *)arg, sizeof(DISP_MODULE_ENUM)))
            {
                printk("disp driver : DISP_IOCTL_SET_CLKON Copy from user failed\n");
                return -EFAULT;
            }

            disp_power_on(module , &(pNode->u4Clock));
            break;

        case DISP_IOCTL_SET_CLKOFF:
            if(copy_from_user(&module, (void *)arg, sizeof(DISP_MODULE_ENUM)))
            {
                printk("disp driver : DISP_IOCTL_SET_CLKOFF Copy from user failed\n");
                return -EFAULT;
            }

            disp_power_off(module , &(pNode->u4Clock));
            break;

        case DISP_IOCTL_GET_LCMINDEX:

            //lcmindex = DISP_GetLCMIndex();
            if(copy_to_user((void *)arg, &lcmindex, sizeof(unsigned long)))
            {
                printk("disp driver : DISP_IOCTL_GET_LCMINDEX Copy to user failed\n");
                return -EFAULT;
            }

            break;

        case DISP_IOCTL_TEST_PATH:
#ifdef DDP_DBG_DDP_PATH_CONFIG
            if(copy_from_user(&value, (void*)arg , sizeof(value)))
            {
                    DDPERR("DISP_IOCTL_MARK_CMQ, copy_from_user failed\n");
                    return -EFAULT;
            }

            config.layer = 0;
            config.layer_en = 1;
            config.source = OVL_LAYER_SOURCE_MEM;
            config.addr = virt_to_phys(inAddr);
            config.inFormat = OVL_INPUT_FORMAT_RGB565;
            config.pitch = 480;
            config.srcROI.x = 0;        // ROI
            config.srcROI.y = 0;
            config.srcROI.width = 480;
            config.srcROI.height = 800;
            config.bgROI.x = config.srcROI.x;
            config.bgROI.y = config.srcROI.y;
            config.bgROI.width = config.srcROI.width;
            config.bgROI.height = config.srcROI.height;
            config.bgColor = 0xff;  // background color
            config.key = 0xff;     // color key
            config.aen = 0;             // alpha enable
            config.alpha = 0;
            DDPMSG("value=%d \n", value);
            if(value==0) // mem->ovl->rdma0->dpi0
            {
                config.srcModule = DISP_MODULE_OVL;
                config.outFormat = RDMA_OUTPUT_FORMAT_ARGB;
                config.dstModule = DISP_MODULE_DPI;
                config.dstAddr = 0;
            }
            else if(value==1) // mem->ovl-> wdma1->mem
            {
                config.srcModule = DISP_MODULE_OVL;
                config.outFormat = WDMA_OUTPUT_FORMAT_RGB888;
                config.dstModule = DISP_MODULE_WDMA0;
                config.dstAddr = virt_to_phys(outAddr);
            }
            else if(value==2)  // mem->rdma0 -> dpi0
            {
                config.srcModule = DISP_MODULE_RDMA0;
                config.outFormat = RDMA_OUTPUT_FORMAT_ARGB;
                config.dstModule = DISP_MODULE_DPI;
                config.dstAddr = 0;
            }
            disp_path_config(&config);
            disp_path_enable();
#endif
            break;

        // <<<  PQ start  >>>
        case DISP_IOCTL_SET_PQPARAM:
        //case DISP_IOCTL_SET_C0_PQPARAM:

            pq_param = get_Color_config(COLOR_ID_0);
            if(copy_from_user(pq_param, (void *)arg, sizeof(DISP_PQ_PARAM)))
            {
                printk("disp driver : DISP_IOCTL_SET_PQPARAM Copy from user failed\n");

                return -EFAULT;
            }

            if(ncs_tuning_mode == 0) //normal mode
            {
                // commit PQ PARAM
                DpEngine_COLORonInit(COLOR_ID_0);
                // macross: fix me with right width/height
                DpEngine_COLORonConfig(COLOR_ID_0, 1152, 1920);
            }
            else
            {
                ncs_tuning_mode = 0;
            }

            break;

        case DISP_IOCTL_GET_PQPARAM:
        //case DISP_IOCTL_GET_C0_PQPARAM:

            pq_param = get_Color_config(COLOR_ID_0);
            if(copy_to_user((void *)arg, pq_param, sizeof(DISP_PQ_PARAM)))
            {
                printk("disp driver : DISP_IOCTL_GET_PQPARAM Copy to user failed\n");
                return -EFAULT;
            }

            break;

        case DISP_IOCTL_SET_C1_PQPARAM:


            pq_param = get_Color_config(COLOR_ID_1);
            if(copy_from_user(pq_param, (void *)arg, sizeof(DISP_PQ_PARAM)))
            {
                printk("disp driver : DISP_IOCTL_SET_PQPARAM Copy from user failed\n");

                return -EFAULT;
            }

            if(ncs_tuning_mode == 0) //normal mode
            {
                // commit PQ PARAM
                DpEngine_COLORonInit(1);
                // macross: fix me with right width/height
                DpEngine_COLORonConfig(1, 1152, 1920);
            }
            else
            {
                ncs_tuning_mode = 0;
            }

            break;

        case DISP_IOCTL_GET_C1_PQPARAM:

            pq_param = get_Color_config(COLOR_ID_1);
            if(copy_to_user((void *)arg, pq_param, sizeof(DISP_PQ_PARAM)))
            {
                printk("disp driver : DISP_IOCTL_GET_PQPARAM Copy to user failed\n");
                return -EFAULT;
            }

            break;

        case DISP_IOCTL_SET_PQINDEX:

            pq_index = get_Color_index();
            if(copy_from_user(pq_index, (void *)arg, sizeof(DISPLAY_PQ_T)))
            {
                printk("disp driver : DISP_IOCTL_SET_PQINDEX Copy from user failed\n");
                return -EFAULT;
            }

            break;

        case DISP_IOCTL_SET_TDSHPINDEX:

            tdshp_index = get_TDSHP_index();
            if(copy_from_user(tdshp_index, (void *)arg, sizeof(DISPLAY_TDSHP_T)))
            {
                printk("disp driver : DISP_IOCTL_SET_TDSHPINDEX Copy from user failed\n");
                return -EFAULT;
            }

            break;

        case DISP_IOCTL_GET_TDSHPINDEX:

            tdshp_index = get_TDSHP_index();
            if(copy_to_user((void *)arg, tdshp_index, sizeof(DISPLAY_TDSHP_T)))
            {
                printk("disp driver : DISP_IOCTL_GET_TDSHPINDEX Copy to user failed\n");
                return -EFAULT;
            }

            break;

        case DISP_IOCTL_SET_PQ_CAM_PARAM:

            pq_cam_param = get_Color_Cam_config();
            if(copy_from_user(pq_cam_param, (void *)arg, sizeof(DISP_PQ_PARAM)))
            {
                printk("disp driver : DISP_IOCTL_SET_PQ_CAM_PARAM Copy from user failed\n");
                return -EFAULT;
            }

            break;

        case DISP_IOCTL_GET_PQ_CAM_PARAM:

            pq_cam_param = get_Color_Cam_config();
            if(copy_to_user((void *)arg, pq_cam_param, sizeof(DISP_PQ_PARAM)))
            {
                printk("disp driver : DISP_IOCTL_GET_PQ_CAM_PARAM Copy to user failed\n");
                return -EFAULT;
            }

            break;

        case DISP_IOCTL_SET_PQ_GAL_PARAM:

            pq_gal_param = get_Color_Gal_config();
            if(copy_from_user(pq_gal_param, (void *)arg, sizeof(DISP_PQ_PARAM)))
            {
                printk("disp driver : DISP_IOCTL_SET_PQ_GAL_PARAM Copy from user failed\n");
                return -EFAULT;
            }

            break;

        case DISP_IOCTL_GET_PQ_GAL_PARAM:

            pq_gal_param = get_Color_Gal_config();
            if(copy_to_user((void *)arg, pq_gal_param, sizeof(DISP_PQ_PARAM)))
            {
                printk("disp driver : DISP_IOCTL_GET_PQ_GAL_PARAM Copy to user failed\n");
                return -EFAULT;
            }

            break;

        case DISP_IOCTL_MUTEX_CONTROL:
            if(copy_from_user(&value, (void *)arg, sizeof(int)))
            {
                DDPERR("disp driver : DISP_IOCTL_MUTEX_CONTROL Copy from user failed\n");
                return -EFAULT;
            }

            if(value == 1)
            {
                ncs_tuning_mode = 1;
            }
            else if(value == 2)
            {

                ncs_tuning_mode = 0;
            }
            else
            {
                DDPERR("disp driver : DISP_IOCTL_MUTEX_CONTROL invalid control\n");
                return -EFAULT;
            }

            DDPMSG("DISP_IOCTL_MUTEX_CONTROL done: %d\n", value);

            break;

        // <<<  PQ end  >>>

        default :
            DDPERR("Ddp drv dose not have such command : %d\n" , cmd);
            break;
    }

    return ret;
}

static int disp_open(struct inode *inode, struct file *file)
{
    disp_node_struct *pNode = NULL;

    DDPDBG("enter disp_open() process:%s\n",current->comm);

    //Allocate and initialize private data
    file->private_data = kmalloc(sizeof(disp_node_struct) , GFP_ATOMIC);
    if(NULL == file->private_data)
    {
        DDPMSG("Not enough entry for DDP open operation\n");
        return -ENOMEM;
    }

    pNode = (disp_node_struct *)file->private_data;
    pNode->open_pid = current->pid;
    pNode->open_tgid = current->tgid;
    INIT_LIST_HEAD(&(pNode->testList));
    pNode->u4LockedMutex = 0;
    pNode->u4LockedResource = 0;
    pNode->u4Clock = 0;
    spin_lock_init(&pNode->node_lock);

    return 0;

}

static ssize_t disp_read(struct file *file, char __user *data, size_t len, loff_t *ppos)
{
    return 0;
}

static int disp_release(struct inode *inode, struct file *file)
{
    disp_node_struct *pNode = NULL;
    unsigned int index = 0;
    DDPDBG("enter disp_release() process:%s\n",current->comm);

    pNode = (disp_node_struct *)file->private_data;

    spin_lock(&pNode->node_lock);

    if(pNode->u4LockedMutex)
    {
        DDPERR("Proccess terminated[Mutex] ! :%s , mutex:%u\n"
            , current->comm , pNode->u4LockedMutex);

        for(index = 0 ; index < ENGINE_MUTEX_NUM ; index += 1)
        {
            if((1 << index) & pNode->u4LockedMutex)
            {
                disp_unlock_mutex(index);
                DDPDBG("unlock index = %d ,mutex_used[ %d %d %d %d ]\n",index,mutex_used[0],mutex_used[1] ,mutex_used[2],mutex_used[3]);
            }
        }

    }

    if(pNode->u4LockedResource)
    {
        DDPERR("Proccess terminated[REsource] ! :%s , resource:%d\n"
            , current->comm , pNode->u4LockedResource);
        spin_lock(&gResourceLock);
        gLockedResource = 0;
        spin_unlock(&gResourceLock);
    }

    if(pNode->u4Clock)
    {
        DDPERR("Process safely terminated [Clock] !:%s , clock:%u\n"
            , current->comm , pNode->u4Clock);

        for(index  = 0 ; index < DISP_MODULE_NUM; index += 1)
        {
            if((1 << index) & pNode->u4Clock)
            {
                disp_power_off((DISP_MODULE_ENUM)index , &pNode->u4Clock);
            }
        }
    }

    spin_unlock(&pNode->node_lock);

    if(NULL != file->private_data)
    {
        kfree(file->private_data);
        file->private_data = NULL;
    }

    return 0;
}

static int disp_flush(struct file * file , fl_owner_t a_id)
{
    return 0;
}

// remap register to user space
static int disp_mmap(struct file * file, struct vm_area_struct * a_pstVMArea)
{

    a_pstVMArea->vm_page_prot = pgprot_noncached(a_pstVMArea->vm_page_prot);
    if(remap_pfn_range(a_pstVMArea ,
                 a_pstVMArea->vm_start ,
                 a_pstVMArea->vm_pgoff ,
                 (a_pstVMArea->vm_end - a_pstVMArea->vm_start) ,
                 a_pstVMArea->vm_page_prot))
    {
        DDPMSG("MMAP failed!!\n");
        return -1;
    }


    return 0;
}


/* Kernel interface */
static struct file_operations disp_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl = disp_unlocked_ioctl,
	.open		= disp_open,
	.release	= disp_release,
	.flush		= disp_flush,
	.read       = disp_read,
	.mmap       = disp_mmap
};

static int disp_probe(struct platform_device *pdev)
{
    struct class_device;

	int ret;
	int i;
    struct class_device *class_dev = NULL;

    DDPMSG("\ndisp driver probe...\n\n");
	ret = alloc_chrdev_region(&disp_devno, 0, 1, DISP_DEVNAME);

	if(ret)
	{
	    DDPERR("Error: Can't Get Major number for DISP Device\n");
	}
	else
	{
	    DDPMSG("Get DISP Device Major number (%d)\n", disp_devno);
    }

	disp_cdev = cdev_alloc();
    disp_cdev->owner = THIS_MODULE;
	disp_cdev->ops = &disp_fops;

	ret = cdev_add(disp_cdev, disp_devno, 1);

    disp_class = class_create(THIS_MODULE, DISP_DEVNAME);
    class_dev = (struct class_device *)device_create(disp_class, NULL, disp_devno, NULL, DISP_DEVNAME);
    if(0)
    {
        unsigned int ret = 0;
        ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_MEMOUT,&g_cmdq_handle);
        if(ret)
        {
            DDPERR("cmdqRecCreate failed, ret=%d \n", ret);
        }
        else
        {
            DDPMSG("cmdqRecCreate success, g_cmdq_handle=0x%x \n", g_cmdq_handle);
        }
        ret = cmdqCoreRegisterCB(CMDQ_GROUP_DISP,
                           cmdqDdpClockOn,
                           cmdqDdpDumpInfo,
                           cmdqDdpResetEng,
                           cmdqDdpClockOff);
        if(ret)
        {
            DDPERR("cmdqCoreRegisterCB failed, ret=%d \n", ret);
        }

    }
	DDPMSG("DISP Probe Done\n");

	NOT_REFERENCED(class_dev);
	return 0;
}

static int disp_remove(struct platform_device *pdev)
{
#if 0
    disable_irq(MT_DISP_OVL0_IRQ_ID  );
    disable_irq(MT_DISP_OVL1_IRQ_ID  );
    disable_irq(MT_DISP_RDMA0_IRQ_ID );
    disable_irq(MT_DISP_RDMA1_IRQ_ID );
    disable_irq(MT_DISP_RDMA2_IRQ_ID );
    disable_irq(MT_DISP_WDMA0_IRQ_ID );
    disable_irq(MT_DISP_WDMA1_IRQ_ID );
    //disable_irq(MT_DISP_COLOR0_IRQ_ID);
    //disable_irq(MT_DISP_COLOR1_IRQ_ID);
    //disable_irq(MT_DISP_AAL_IRQ_ID   );
    //disable_irq(MT_DISP_GAMMA_IRQ_ID );
    //disable_irq(MT_DISP_UFOE_IRQ_ID  );
    disable_irq(MT_DISP_MUTEX_IRQ_ID );

#endif
    return 0;
}

static void disp_shutdown(struct platform_device *pdev)
{
	/* Nothing yet */
}


/* PM suspend */
static int disp_suspend(struct platform_device *pdev, pm_message_t mesg)
{
    printk("\n\n==== DISP suspend is called ====\n");


    return 0;
}

/* PM resume */
static int disp_resume(struct platform_device *pdev)
{
    printk("\n\n==== DISP resume is called ====\n");


    return 0;
}

static struct platform_driver disp_driver =
{
	.probe		= disp_probe,
	.remove		= disp_remove,
	.shutdown	= disp_shutdown,
	.suspend	= disp_suspend,
	.resume		= disp_resume,
	.driver     =
	{
	    .name = DISP_DEVNAME,
	    //.pm   = &disp_pm_ops,
	},
};

static int __init disp_init(void)
{
    int ret;

    spin_lock_init(&gResourceLock);
    spin_lock_init(&gOvlLock);
    spin_lock_init(&gRegisterUpdateLock);
    spin_lock_init(&gPowerOperateLock);
    spin_lock_init(&gMutexLock);

    DDPMSG("Register the disp driver\n");
    if(platform_driver_register(&disp_driver))
    {
        DDPERR("failed to register disp driver\n");
        //platform_device_unregister(&disp_device);
        ret = -ENODEV;
        return ret;
    }

    ddp_debug_init();
    disp_init_irq(DISP_DEVNAME);

    return 0;
}

static void __exit disp_exit(void)
{
    cdev_del(disp_cdev);
    unregister_chrdev_region(disp_devno, 1);

    platform_driver_unregister(&disp_driver);
    //platform_device_unregister(&disp_device);

    device_destroy(disp_class, disp_devno);
    class_destroy(disp_class);

    ddp_debug_exit();

    DDPMSG("Done\n");
}


static int disp_get_mutex_status()
{
    return disp_mutex_status;
}

int disp_module_clock_on(DISP_MODULE_ENUM module, char* caller_name)
{
    return 0;
}

int disp_module_clock_off(DISP_MODULE_ENUM module, char* caller_name)
{
    return 0;
}


const char *ddp_get_module_string(DISP_MODULE_ENUM module)
{
	switch(module)
	{
		case DISP_MODULE_UFOE:		return "DISP_MODULE_UFOE";
		case DISP_MODULE_AAL:		return "DISP_MODULE_AAL";
		case DISP_MODULE_COLOR0:	return "DISP_MODULE_COLOR0";
		case DISP_MODULE_COLOR1:	return "DISP_MODULE_COLOR1";
		case DISP_MODULE_RDMA0:	return "DISP_MODULE_RDMA0";
		case DISP_MODULE_RDMA1:	return "DISP_MODULE_RDMA1";
		case DISP_MODULE_RDMA2:	return "DISP_MODULE_RDMA2";
		case DISP_MODULE_WDMA0:	return "DISP_MODULE_WDMA0";
		case DISP_MODULE_WDMA1:	return "DISP_MODULE_WDMA1";
		case DISP_MODULE_OVL0:		return "DISP_MODULE_OVL0";
		case DISP_MODULE_OVL1:		return "DISP_MODULE_OVL1";
		case DISP_MODULE_GAMMA:	return "DISP_MODULE_GAMMA";
		case DISP_MODULE_PWM0:		return "DISP_MODULE_PWM0";
		case DISP_MODULE_PWM1:		return "DISP_MODULE_PWM1";
		case DISP_MODULE_OD:		return "DISP_MODULE_OD";
		case DISP_MODULE_MERGE:		return "DISP_MODULE_MERGE";
		case DISP_MODULE_SPLIT0:		return "DISP_MODULE_SPLIT0";
		case DISP_MODULE_SPLIT1:		return "DISP_MODULE_SPLIT1";
		case DISP_MODULE_DSI0:		return "DISP_MODULE_DSI0";
		case DISP_MODULE_DSI1:		return "DISP_MODULE_DSI1";
		case DISP_MODULE_DSIDUAL:	return "DISP_MODULE_DSIDUAL";
		case DISP_MODULE_DPI:		return "DISP_MODULE_DPI";
		case DISP_MODULE_SMI:		return "DISP_MODULE_SMI";
		case DISP_MODULE_CONFIG:	return "DISP_MODULE_CONFIG";
		case DISP_MODULE_CMDQ:		return "DISP_MODULE_CMDQ";
		case DISP_MODULE_MUTEX:		return "DISP_MODULE_MUTEX";
		case DISP_MODULE_UNKNOWN:	return "DISP_MODULE_UNKNOWN";
		default:						return "DISP_MODULE_UNKNOWN";
	}
}
DISP_MODULE_ENUM disp_cmdq_convert_module(CMDQ_ENG_ENUM module)
{
  switch(module)
  {
      case CMDQ_ENG_DISP_UFOE    : return DISP_MODULE_UFOE    ;
      case CMDQ_ENG_DISP_AAL     : return DISP_MODULE_AAL     ;
      case CMDQ_ENG_DISP_COLOR0  : return DISP_MODULE_COLOR0  ;
      case CMDQ_ENG_DISP_COLOR1  : return DISP_MODULE_COLOR1  ;
      case CMDQ_ENG_DISP_RDMA0   : return DISP_MODULE_RDMA0   ;
      case CMDQ_ENG_DISP_RDMA1   : return DISP_MODULE_RDMA1   ;
      case CMDQ_ENG_DISP_RDMA2   : return DISP_MODULE_RDMA2   ;
      case CMDQ_ENG_DISP_WDMA0   : return DISP_MODULE_WDMA0   ;
      case CMDQ_ENG_DISP_WDMA1   : return DISP_MODULE_WDMA1   ;
      case CMDQ_ENG_DISP_OVL0    : return DISP_MODULE_OVL0    ;
      case CMDQ_ENG_DISP_OVL1    : return DISP_MODULE_OVL1    ;
      case CMDQ_ENG_DISP_GAMMA   : return DISP_MODULE_GAMMA   ;
      case CMDQ_ENG_DISP_MERGE   : return DISP_MODULE_MERGE   ;
      case CMDQ_ENG_DISP_SPLIT0  : return DISP_MODULE_SPLIT0  ;
      case CMDQ_ENG_DISP_SPLIT1  : return DISP_MODULE_SPLIT1  ;
      case CMDQ_ENG_DISP_DSI0    :
      case CMDQ_ENG_DISP_DSI0_CMD:
      case CMDQ_ENG_DISP_DSI0_VDO:
                                   return DISP_MODULE_DSI0;
      case CMDQ_ENG_DISP_DSI1    :
      case CMDQ_ENG_DISP_DSI1_CMD:
      case CMDQ_ENG_DISP_DSI1_VDO:
                                   return DISP_MODULE_DSI1;
      case CMDQ_ENG_DISP_DPI     : return DISP_MODULE_DPI;
      default:
          DDPERR("unknown CMDQ engine=%d", module);
          return DISP_MODULE_UNKNOWN;
  }
}

unsigned int disp_cmdq_convert_modules(uint64_t engineFlag)
{
    unsigned int ddp_modules = 0;
    unsigned int i=0;
    for(i=CMDQ_ENG_DISP_UFOE;i<=CMDQ_ENG_DISP_DPI;i++)
    {
        if(engineFlag&(1<<i))
        {
            ddp_modules |= (1<<disp_cmdq_convert_module(i));
        }
    }
    return ddp_modules;
}

unsigned int cmdqDdpClockOn(uint64_t engineFlag)
{
    // todo: use clock driver's api instead
    // todo: just start engineFlag related engine
    //*(volatile unsigned int*)(DISP_REG_CONFIG_MMSYS_CG_CLR0) = 0xffffffff;
    //*(volatile unsigned int*)(DISP_REG_CONFIG_MMSYS_CG_CLR1) = 0xffffffff;
    //*(volatile unsigned int*)(DISP_REG_CONFIG_C08) = 0xffffffff;
}
unsigned int cmdqDdpClockOff(uint64_t engineFlag)
{
    // todo: use clock driver's api instead
    //*(volatile unsigned int*)(DISP_REG_CONFIG_MMSYS_CG_CLR0) = 0;

}

unsigned int cmdqDdpDumpInfo(uint64_t engineFlag,
                        char     *pOutBuf,
                        unsigned int bufSize)
{
    unsigned int ddp_modules = disp_cmdq_convert_modules(engineFlag);
    unsigned int i=0;
	printk("%s, engineFlag=%d\n", __func__, engineFlag);


    DDPMSG("cmdqDdpDumpInfo: engineFlag=0x%x, ddp_modules=0x%x\n", engineFlag, ddp_modules);
    for(i=0;i<DISP_MODULE_UNKNOWN;i++)
    {
        if(ddp_modules&(1<<i))
        {
            ddp_dump_info(i);
        }
    }
}

unsigned int cmdqDdpResetEng(uint64_t engineFlag)
{
    unsigned int ddp_modules = disp_cmdq_convert_modules(engineFlag);
    unsigned int i=0;

    DDPMSG("cmdqDdpResetEng: engineFlag=0x%x, ddp_modules=0x%x\n", engineFlag, ddp_modules);
    for(i=0;i<DISP_MODULE_UNKNOWN;i++)
    {
        if(ddp_modules&(1<<i))
        {
            ddp_dump_info(i);
        }
    }

}

module_init(disp_init);
module_exit(disp_exit);
MODULE_AUTHOR("Tzu-Meng, Chung <Tzu-Meng.Chung@mediatek.com>");
MODULE_DESCRIPTION("Display subsystem Driver");
MODULE_LICENSE("GPL");
