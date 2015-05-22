#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/xlog.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>

#include <linux/earlysuspend.h>

#include <mach/irqs.h>
#include <asm/io.h>

#include <mach/m4u.h>
#include <mach/m4u_port.h>

#include "mach/mt_clkmgr.h"
#include "mjc_kernel_driver.h"

//**************************************************//
// LOG
//**************************************************//
#define MJC_ASSERT(x) if(!(x)){xlog_printk(ANDROID_LOG_ERROR, "MJC", "assert fail, file:%s, line:%d", __FILE__, __LINE__);}

#define MTK_MJC_DBG
#ifdef MTK_MJC_DBG
#define MJCDBG(string, args...)	xlog_printk(ANDROID_LOG_DEBUG, "MJC", "[pid=%d]"string,current->tgid,##args);
#else
#define MJCDBG(string, args...)
#endif

#define MTK_MJC_MSG
#ifdef MTK_MJC_MSG
#define MJCMSG(string, args...)	xlog_printk(ANDROID_LOG_INFO, "MJC", "[pid=%d]"string,current->tgid,##args)
#else
#define MJCMSG(string, args...)
#endif

//**************************************************//
// Define
//**************************************************//
#define MJC_DEVNAME     "MJC"
#define MTK_MJC_DEV_MAJOR_NUMBER 168
#define MJC_FORCE_REG_NUM 100
#define MJC_REG_PA     (0x17001000)
#define MJC_REG_REGION (0x1000)
#define EFUSE_MJC_IDX (3)
#define EFUSE_MJC_BIT (1<<11)

//**************************************************//
// variable
//**************************************************//
static DEFINE_SPINLOCK(ContextLock);
static DEFINE_SPINLOCK(HWLock);


static MJC_CONTEXT_T grContext;        //spinlock : ContextLock
static MJC_CONTEXT_T grHWLockContext;  //spinlock : HWLock

static dev_t mjc_devno = MKDEV(MTK_MJC_DEV_MAJOR_NUMBER,0);
static struct cdev *g_mjc_cdev;
static struct class *pMjcClass = NULL;
static struct device* mjcDevice = NULL;
int gi4Register;
MJC_WRITE_REG_T gfWriteReg[MJC_FORCE_REG_NUM];


/*****************************************************************************
 * FUNCTION
 *    _mjc_CreateEvent
 * DESCRIPTION
 *
 * PARAMETERS
 *    None.
 * RETURNS
 *    None.
 ****************************************************************************/
static int _mjc_CreateEvent(MJC_EVENT_T *a_prParam)
{
    wait_queue_head_t *prWaitQueue;
    unsigned char *pu1Flag;

    prWaitQueue = (wait_queue_head_t *)kmalloc(sizeof(wait_queue_head_t),GFP_ATOMIC);
    pu1Flag     = (unsigned char *)kmalloc(sizeof(unsigned char),GFP_ATOMIC);
    if(prWaitQueue != 0)
    {
        init_waitqueue_head(prWaitQueue);
        a_prParam->pvWaitQueue = (void *)prWaitQueue;
    }
    else
    {
        MJCMSG("[Error]_mjc_CreateEvent() Event wait Queue failed to create\n");
    }

    if(pu1Flag != 0)
    {
        a_prParam->pvFlag= (void *)pu1Flag;
        *((unsigned char *)a_prParam->pvFlag) = 0;
    }
    else
    {
        MJCMSG("[Error]_mjc_CreateEvent() Event flag failed to create\n");
    }

    return 0;
}


/*****************************************************************************
 * FUNCTION
 *    _mjc_CloseEvent
 * DESCRIPTION
 *
 * PARAMETERS
 *    None.
 * RETURNS
 *    None.
 ****************************************************************************/
static int _mjc_CloseEvent(MJC_EVENT_T *a_prParam)
{
    wait_queue_head_t *prWaitQueue;
    unsigned char *pu1Flag;

    prWaitQueue = (wait_queue_head_t *)a_prParam->pvWaitQueue;
    pu1Flag     = (unsigned char *)a_prParam->pvFlag;

    if (prWaitQueue)
    {
        kfree(prWaitQueue);
    }
    if (pu1Flag)
    {
        kfree(pu1Flag);
    }

    a_prParam->pvWaitQueue = 0;
    a_prParam->pvFlag = 0;
    return 0;
}

/*****************************************************************************
 * FUNCTION
 *    _mjc_WaitEvent
 * DESCRIPTION
 *
 * PARAMETERS
 *    None.
 * RETURNS
 *    None.
 ****************************************************************************/
static int _mjc_WaitEvent(MJC_EVENT_T *a_prParam)
{
    wait_queue_head_t *prWaitQueue;
    long               timeout_jiff;
    int i4Status;

    prWaitQueue   = (wait_queue_head_t *)a_prParam->pvWaitQueue;
    timeout_jiff = (a_prParam->u4TimeoutMs) * HZ / 1000;
    //MJCDBG("_mjc_WaitEv(),a_prParam->u4TimeoutMs=%d, timeout = %ld\n",a_prParam->u4TimeoutMs,timeout_jiff);

    if (0 == wait_event_interruptible_timeout(*prWaitQueue, *((unsigned char *)a_prParam->pvFlag)/*g_mflexvideo_interrupt_handler*/, timeout_jiff))
    {
        i4Status = 1;//timeout
    }
    else
    {
        i4Status = 0;
    }
    *((unsigned char *)a_prParam->pvFlag) = 0;

    return i4Status;
}

/*****************************************************************************
 * FUNCTION
 *    _mjc_WaitEvent
 * DESCRIPTION
 *
 * PARAMETERS
 *    None.
 * RETURNS
 *    None.
 ****************************************************************************/
static int _mjc_SetEvent(MJC_EVENT_T *a_prParam)
{
    wait_queue_head_t *prWaitQueue;

    //MJCDBG("[MFV]eVideoSetEvent\n");

    prWaitQueue = (wait_queue_head_t *)a_prParam->pvWaitQueue;

    if (a_prParam->pvFlag != NULL)
    {
        *((unsigned char *)a_prParam->pvFlag) = 1;
    }
    else
    {
        MJCMSG("[Error] _mjc_SetEvent() Event flag should not be null\n");
    }

    if (prWaitQueue != NULL)
    {
        wake_up_interruptible(prWaitQueue);
    }
    else
    {
        MJCMSG("[[Error] _mjc_SetEvent() Wait Queue should not be null\n");
    }
    return 0;
}

/*****************************************************************************
 * FUNCTION
 *    _mjc_WaitEvent
 * DESCRIPTION
 *
 * PARAMETERS
 *    None.
 * RETURNS
 *    None.
 ****************************************************************************/
static void _mjc_m4uConfigPort(void)
{
    int u4Status;
    M4U_PORT_STRUCT rM4uPort;
    rM4uPort.Virtuality = 1;						   
    rM4uPort.Security = 0;
    rM4uPort.Distance = 1;
    rM4uPort.Direction = 0;
    rM4uPort.domain = 3;

	rM4uPort.ePortID = M4U_PORT_MJC_MV_RD;  
    u4Status = m4u_config_port(&rM4uPort);
    if (u4Status != 0)
    {
        MJCMSG("[ERROR] m4u_config_port(%d) fail!! status = %d\n", rM4uPort.ePortID, u4Status);
    }

	rM4uPort.ePortID = M4U_PORT_MJC_MV_WR;  
    u4Status = m4u_config_port(&rM4uPort);
    if (u4Status != 0)
    {
        MJCMSG("[ERROR] m4u_config_port(%d) fail!! status = %d\n", rM4uPort.ePortID, u4Status);
    }

	rM4uPort.ePortID = M4U_PORT_MJC_DMA_RD;  
    u4Status = m4u_config_port(&rM4uPort);
    if (u4Status != 0)
    {
        MJCMSG("[ERROR] m4u_config_port(%d) fail!! status = %d\n", rM4uPort.ePortID, u4Status);
    }

    rM4uPort.ePortID = M4U_PORT_MJC_DMA_WR;  
    u4Status = m4u_config_port(&rM4uPort);
    if (u4Status != 0)
    {
        MJCMSG("[ERROR] m4u_config_port(%d) fail!! status = %d\n", rM4uPort.ePortID, u4Status);
    }

}

/*****************************************************************************
 * FUNCTION
 *    mjc_fault_callback
 * DESCRIPTION
 *
 * PARAMETERS
 *    None.
 * RETURNS
 *    None.
 ****************************************************************************/
m4u_callback_ret_t mjc_m4u_fault_callback(int port, unsigned int mva, void* data)
{
    printk("[ERROR] mjc faut call port=%d, mva=0x%x, data=%d\n", port, mva, (int)data);

	printk("Sys Basic: 0x%x, Status: 0x%x, FS00: 0x%x, RDMA00: 0x%x, RDMA01: 0x%x\n", 
		   (*(volatile unsigned int*)(gi4Register+0x4)),
		   (*(volatile unsigned int*)(gi4Register+0x70)), (*(volatile unsigned int*)(gi4Register+0xc00)),
		   (*(volatile unsigned int*)(gi4Register+0x100)), (*(volatile unsigned int*)(gi4Register+0x104)) );

	printk("ACT FB Address (0x%x,0x%x,0x%x), (0x%x,0x%x,0x%x), (0x%x)\n",
           (*(volatile unsigned int*)(gi4Register+0xc4c)), (*(volatile unsigned int*)(gi4Register+0xc50)), (*(volatile unsigned int*)(gi4Register+0xc54)),
           (*(volatile unsigned int*)(gi4Register+0xc58)), (*(volatile unsigned int*)(gi4Register+0xc5c)), (*(volatile unsigned int*)(gi4Register+0xc60)),
           (*(volatile unsigned int*)(gi4Register+0xc64)) );
	printk("PLB FB Address (0x%x,0x%x,0x%x), (0x%x,0x%x,0x%x), (0x%x)\n",
           (*(volatile unsigned int*)(gi4Register+0xc68)), (*(volatile unsigned int*)(gi4Register+0xc6c)), (*(volatile unsigned int*)(gi4Register+0xc70)),
           (*(volatile unsigned int*)(gi4Register+0xc74)), (*(volatile unsigned int*)(gi4Register+0xc78)), (*(volatile unsigned int*)(gi4Register+0xc7c)),
           (*(volatile unsigned int*)(gi4Register+0xc80)) );

	printk("WD Address (0x%x,0x%x)\n",
		   (*(volatile unsigned int*)(gi4Register+0xc3c)), (*(volatile unsigned int*)(gi4Register+0xc40)) );

	printk("MV Address BI(0x%x,0x%x), TD(0x%x,0x%x), CUR(0x%x,0x%x), PV(0x%x)\n",
		   (*(volatile unsigned int*)(gi4Register+0xc1c)), (*(volatile unsigned int*)(gi4Register+0xc20)),
		   (*(volatile unsigned int*)(gi4Register+0xc30)), (*(volatile unsigned int*)(gi4Register+0xc34)),
		   (*(volatile unsigned int*)(gi4Register+0xc24)), (*(volatile unsigned int*)(gi4Register+0xc28)),
		   (*(volatile unsigned int*)(gi4Register+0xc2c)) );

	printk("Lab=0x%x, CG=0x%x \n", ioread32((void *)0xf4000100), ioread32((void *)0xf7000000));

	return M4U_CALLBACK_HANDLED;
}

/*****************************************************************************
 * FUNCTION
 *    mjc_open
 * DESCRIPTION
 *
 * PARAMETERS
 *    None.
 * RETURNS
 *    None.
 ****************************************************************************/
extern u32 get_devinfo_with_index(u32 index);
static int mjc_open(struct inode *pInode, struct file *pFile)
{
    unsigned long ulFlags;
    unsigned int u4efuse;
    
    MJCDBG("mjc_open() pid = %d\n", current->pid);
    
    u4efuse = get_devinfo_with_index(EFUSE_MJC_IDX);
     
    if ((u4efuse & EFUSE_MJC_BIT) != 0)
    {
    	  MJCMSG("[ERROR] mjc efuse no support %d\n", u4efuse);
        return -1;
    }

    // Gary todo: enable clock 
    enable_clock(MT_CG_DISP0_SMI_COMMON, "mjc");
    enable_clock(MT_CG_MJC_SMI_LARB, "mjc");
	enable_clock(MT_CG_MJC_TOP_GROUP0, "mjc");
	enable_clock(MT_CG_MJC_TOP_GROUP1, "mjc");
	enable_clock(MT_CG_MJC_TOP_GROUP2, "mjc");
	enable_clock(MT_CG_MJC_LARB4_AXI_ASIF, "mjc");

	spin_lock_irqsave(&ContextLock, ulFlags);
	grContext.rEvent.u4TimeoutMs = 0xFFFFFFFF;
	spin_unlock_irqrestore(&ContextLock, ulFlags);

	spin_lock_irqsave(&HWLock, ulFlags);
	grHWLockContext.rEvent.u4TimeoutMs = 0xFFFFFFFF;
	spin_unlock_irqrestore(&HWLock, ulFlags);

	_mjc_m4uConfigPort();

	m4u_register_fault_callback(M4U_PORT_MJC_MV_RD, mjc_m4u_fault_callback, (void *)0);
	m4u_register_fault_callback(M4U_PORT_MJC_MV_WR, mjc_m4u_fault_callback, (void *)0);
	m4u_register_fault_callback(M4U_PORT_MJC_DMA_RD, mjc_m4u_fault_callback, (void *)0);
	m4u_register_fault_callback(M4U_PORT_MJC_DMA_WR, mjc_m4u_fault_callback, (void *)0);

	return 0;
}

/*****************************************************************************
 * FUNCTION
 *    mjc_release
 * DESCRIPTION
 *
 * PARAMETERS
 *    None.
 * RETURNS
 *    None.
 ****************************************************************************/
static int mjc_release(struct inode *pInode, struct file *pFile)
{
    MJCDBG("mjc_release() pid = %d\n", current->pid);

	m4u_unregister_fault_callback(M4U_PORT_MJC_MV_RD);
	m4u_unregister_fault_callback(M4U_PORT_MJC_MV_WR);
	m4u_unregister_fault_callback(M4U_PORT_MJC_DMA_RD);
	m4u_unregister_fault_callback(M4U_PORT_MJC_DMA_WR);

	// Gary todo: disable clock
    disable_clock(MT_CG_MJC_SMI_LARB, "mjc");
	disable_clock(MT_CG_MJC_TOP_GROUP0, "mjc");
	disable_clock(MT_CG_MJC_TOP_GROUP1, "mjc");
	disable_clock(MT_CG_MJC_TOP_GROUP2, "mjc");
	disable_clock(MT_CG_DISP0_SMI_COMMON, "mjc");
	disable_clock(MT_CG_MJC_LARB4_AXI_ASIF, "mjc");

	return 0;
}

/*****************************************************************************
 * FUNCTION
 *    mjc_ioctl
 * DESCRIPTION
 *
 * PARAMETERS
 *    None.
 * RETURNS
 *    None.
 ****************************************************************************/
static long mjc_ioctl(struct file *pfile, unsigned int u4cmd, unsigned long u4arg)
{
    int ret = 0;
	int u4FirstUse, i4Value, i4Add;
	unsigned long ulFlags;
	MJC_IOCTL_LOCK_HW_T rLockHW;
	MJC_IOCTL_ISR_T rIsr;
	MJC_READ_REG_T rReadReg;
	MJC_WRITE_REG_T rWriteReg;
	MJC_IOCTL_SRC_CLK_T rSrcClk;
    int cnt = 0;

    MJCDBG("mjc_ioctl() command:%u\n", u4cmd);	

	switch(u4cmd)
    {
        case MJC_LOCKHW:
		{
			MJCDBG("+ mjc_ioctl() MJC_LOCKHW + tid = %d\n", current->pid);

			if(copy_from_user(&rLockHW, (void __user *)u4arg, sizeof(MJC_IOCTL_LOCK_HW_T)))
			{
			    MJCMSG("[ERROR] mjc_ioctl() MJC_LOCKHW copy_from_user fail\n");
				return -1;
			}

			if (sizeof(MJC_IOCTL_LOCK_HW_T) != rLockHW.u4StructSize)
			{
			   MJCMSG("[ERROR] mjc_ioctl() MJC_LOCKHW context size mismatch (user:%d, kernel:%d)\n", rLockHW.u4StructSize, sizeof(MJC_IOCTL_LOCK_HW_T));
			   return -1;
			}

			spin_lock_irqsave(&HWLock, ulFlags);
	    	if(grHWLockContext.rEvent.u4TimeoutMs == 0xFFFFFFFF)
	    	{
	    	    grHWLockContext.rEvent.u4TimeoutMs = 1;
	    	    u4FirstUse = 1;
	    	}
			else
			{
			    u4FirstUse = 0;
			}
	        spin_unlock_irqrestore(&HWLock, ulFlags);

            //MJCDBG("mjc_ioctl() MJC_LOCKHW start + tid = %d (%d)\n", current->pid, grHWLockContext.rEvent.u4TimeoutMs);
			if(u4FirstUse == 1)
			{
			    _mjc_WaitEvent(&(grHWLockContext.rEvent));
				spin_lock_irqsave(&HWLock, ulFlags);
				grHWLockContext.rEvent.u4TimeoutMs = 1000;
				spin_unlock_irqrestore(&HWLock, ulFlags);
			}
			else
			{
			    ret = _mjc_WaitEvent(&(grHWLockContext.rEvent));
			}
			//MJCDBG("mjc_ioctl() MJC_LOCKHW end + tid = %d (%d)\n", current->pid, grHWLockContext.rEvent.u4TimeoutMs);

			if (ret == 1)
			{
			    MJCMSG("[ERROR] mjc_ioctl() MJC_LOCKHW HW has been usaged\n");
			    return -1;
			}

            //Gary todo
			enable_irq(MJC_TOP_IRQ_BIT_ID);

			MJCDBG("- mjc_ioctl() MJC_LOCKHW + tid = %d\n", current->pid);
		}
		break;

        case MJC_WAITISR:
		{
			MJCDBG("mjc_ioctl() MJC_WAITISR + tid = %d\n", current->pid);

			if(copy_from_user(&rIsr, (void __user *)u4arg, sizeof(MJC_IOCTL_ISR_T)))
			{
			    MJCMSG("[ERROR] mjc_ioctl() MJC_WAITISR copy_from_user fail\n");
				return -1;
			}

			if (sizeof(MJC_IOCTL_ISR_T) != rIsr.u4StructSize)
			{
			   MJCMSG("[ERROR] mjc_ioctl() MJC_WAITISR context size mismatch (user:%d, kernel:%d)\n", rIsr.u4StructSize, sizeof(MJC_IOCTL_ISR_T));
			   return -1;
			}

			MJCDBG(" isrevent timeout setting (%x, %x)\n", grContext.rEvent.u4TimeoutMs, rIsr.u4TimeoutMs);
	    	spin_lock_irqsave(&ContextLock, ulFlags);
	    	grContext.rEvent.u4TimeoutMs = rIsr.u4TimeoutMs;
	        spin_unlock_irqrestore(&ContextLock, ulFlags);
			//MJCDBG(" new isrevent timeout =%x\n", grContext.rEvent.u4TimeoutMs);

	        ret = _mjc_WaitEvent(&(grContext.rEvent));
	        MJCDBG("mjc_ioctl() waitdone MJC_WAITISR TID:%d, ret:%d\n", current->pid, ret);

			if (ret != 0)
			{
			    MJCMSG("[ERROR] mjc_ioctl() MJC_WAITISR TimeOut\n");
				spin_lock_irqsave(&HWLock, ulFlags);
	            _mjc_SetEvent(&(grHWLockContext.rEvent));
	            spin_unlock_irqrestore(&HWLock, ulFlags);

				disable_irq_nosync(MJC_TOP_IRQ_BIT_ID);  

				return -2;
			}
        }
		break;

		case MJC_READ_REG:
		{
			MJCDBG("mjc_ioctl() MJC_READ_REG + tid = %d\n", current->pid);

			if(copy_from_user(&rReadReg, (void __user *)u4arg, sizeof(MJC_READ_REG_T)))
            {
                MJCMSG("[ERROR] mjc_ioctl() MJC_READ_REG copy_from_user failed\n");
                return -1;
            }

            if(rReadReg.reg>(MJC_REG_PA+MJC_REG_REGION)|| rReadReg.reg<MJC_REG_PA)
            {
                MJCMSG("[ERROR] mjc_ioctl() MJC_READ_REG, addr invalid, addr min=0x%x, max=0x%x, addr=0x%x \n",
                    MJC_REG_PA,
                    (MJC_REG_PA+MJC_REG_REGION),
                    rReadReg.reg);
                return -1;
            }

            i4Add = gi4Register+(rReadReg.reg & 0xfff);
            i4Value = (*(volatile unsigned int*)i4Add) & rReadReg.mask;

            MJCDBG("read 0x%x (0x%x)= 0x%x (0x%x)\n", rReadReg.reg, i4Add, i4Value, rReadReg.mask);

            if(copy_to_user(rReadReg.val, &i4Value, sizeof(unsigned int)))
            {
                MJCMSG("[ERROR] mjc_ioctl() MJC_READ_REG, copy_to_user failed\n");
                return -1;
            }
		}
		break;

		case MJC_WRITE_REG:
		{
			MJCDBG("mjc_ioctl() MJC_WRITE_REG + tid = %d\n", current->pid);

			if(copy_from_user(&rWriteReg, (void __user *)u4arg, sizeof(MJC_WRITE_REG_T)))
            {
                MJCMSG("[ERROR] mjc_ioctl() MJC_WRITE_REG copy_from_user failed\n");
                return -1;
            }

			MJCDBG("write  0x%x = 0x%x (0x%x)\n", rWriteReg.reg, rWriteReg.val, rWriteReg.mask);

            if(rWriteReg.reg>(MJC_REG_PA+MJC_REG_REGION) || rWriteReg.reg<MJC_REG_PA)
            {
                MJCMSG("[ERROR] mjc_ioctl() MJC_WRITE_REG, addr invalid, addr min=0x%x, max=0x%x, addr=0x%x \n",
                    MJC_REG_PA,
                    (MJC_REG_PA+MJC_REG_REGION),
                    rWriteReg.reg);
                return -1;
            }

            for(cnt=0;cnt<MJC_FORCE_REG_NUM;cnt++)
            {
                if(gfWriteReg[cnt].reg==0||gfWriteReg[cnt].reg==rWriteReg.reg)
                {
                    gfWriteReg[cnt].reg=rWriteReg.reg;
                    gfWriteReg[cnt].val=rWriteReg.val;
                    gfWriteReg[cnt].mask=rWriteReg.mask;
                    break;
                }
            }

            i4Add = gi4Register+(rWriteReg.reg & 0xfff);
            *(volatile unsigned int*)i4Add = (*(volatile unsigned int*)i4Add & ~rWriteReg.mask) | (rWriteReg.val & rWriteReg.mask);
		}
		break;

        case MJC_WRITE_REG_TBL:
        {
            MJCDBG("mjc_ioctl() MJC_WRITE_REG_TBL + tid = %d\n", current->pid);
            
            for(cnt=0;cnt<MJC_FORCE_REG_NUM;cnt++)
            {
                if(gfWriteReg[cnt].reg == 0) break;
                MJCMSG("s(%x %x)\n", gfWriteReg[cnt].reg, gfWriteReg[cnt].val);                
                i4Add = gi4Register+(gfWriteReg[cnt].reg & 0xfff);
                *(volatile unsigned int*)i4Add = (*(volatile unsigned int*)i4Add & ~gfWriteReg[cnt].mask) | (gfWriteReg[cnt].val & gfWriteReg[cnt].mask);
            }
        }
        break;

        case MJC_CLEAR_REG_TBL:
        {
            MJCDBG("mjc_ioctl() MJC_CLEAR_REG_TBL + tid = %d\n", current->pid);
            for(cnt=0;cnt<MJC_FORCE_REG_NUM;cnt++)
            {
                gfWriteReg[cnt].reg = 0;
                gfWriteReg[cnt].val = 0;
                gfWriteReg[cnt].mask = 0;                 
            }
        }
        break;

		case MJC_SOURCE_CLK:
		{
			MJCDBG("mjc_ioctl() MJC_SOURCE_CLK + tid = %d\n", current->pid);

			if(copy_from_user(&rSrcClk, (void __user *)u4arg, sizeof(MJC_IOCTL_SRC_CLK_T)))
			{
			    MJCMSG("[ERROR] mjc_ioctl() MJC_SOURCE_CLK copy_from_user fail\n");
				return -1;
			}

			if (sizeof(MJC_IOCTL_SRC_CLK_T) != rSrcClk.u4StructSize)
			{
			   MJCMSG("[ERROR] mjc_ioctl() MJC_SOURCE_CLK context size mismatch (user:%d, kernel:%d)\n", rSrcClk.u4StructSize, sizeof(MJC_IOCTL_SRC_CLK_T));
			   return -1;
			}

            MJCDBG(" frame rate =%d\n", rSrcClk.u2OutputFramerate);
			if (rSrcClk.u2OutputFramerate == 600) // frame rate 60 case
			{
			    clkmux_sel(MT_MUX_MJC, 7, "mjc"); // SYSPLL_D5 (218.4)
			}
			else if (rSrcClk.u2OutputFramerate == 1200) // frame rate 120 case
			{
			    clkmux_sel(MT_MUX_MJC, 1, "mjc"); // UNIVPLL_D3 (416)
			}
			else
			{
			    MJCMSG("[ERROR] mjc_ioctl() MJC_SOURCE_CLK fail frame rate : %d\n", rSrcClk.u2OutputFramerate);
				return -1;
			}
		}
		break;

		default:
			MJCMSG("[ERROR] mjc_ioctl() No such command 0x%x!!\n", u4cmd);
			break;

	}
	return ret;
}

/*****************************************************************************
 * FUNCTION
 *    mjc_mmap
 * DESCRIPTION
 *
 * PARAMETERS
 *    None.
 * RETURNS
 *    None.
 ****************************************************************************/
static int mjc_mmap(struct file *pFile, struct vm_area_struct *pVma)
{
    unsigned int u4Length;
	unsigned int u4Addr;
	
	MJCDBG("mjc_mmap \n");

	MJCMSG("start = 0x%x, pgoff= 0x%x vm_end= 0x%x, vm_page_prot= 0x%x\n",
	       (unsigned int)pVma->vm_start,
	       (unsigned int)pVma->vm_pgoff,
	       (unsigned int)pVma->vm_end,
	       (unsigned int)pVma->vm_page_prot );	

    pVma->vm_page_prot = pgprot_noncached(pVma->vm_page_prot);

	u4Length = pVma->vm_end - pVma->vm_start;
	u4Addr = pVma->vm_pgoff<<PAGE_SHIFT;
	
	if((u4Length > MJC_REG_REGION) || (u4Addr < MJC_REG_PA) || (u4Addr > MJC_REG_PA+MJC_REG_REGION))
	{
	    MJCMSG("[ERROR] mmap region error: Length(0x%x), pfn(0x%x)\n", u4Length, u4Addr);
		return -EAGAIN;
	}    

    if (remap_pfn_range(pVma, pVma->vm_start, pVma->vm_pgoff, u4Length, pVma->vm_page_prot))
    {
        MJCMSG("[ERROR] MMAP failed!!\n");
        return -EAGAIN;
    }


    return 0;
}

static const struct file_operations g_mjc_fops =
{
    .owner = THIS_MODULE,
    .open = mjc_open,
    .release = mjc_release,
    .unlocked_ioctl = mjc_ioctl,
    .mmap = mjc_mmap
};

/*****************************************************************************
 * FUNCTION
 *    mjc_probe
 * DESCRIPTION
 *    1. Register MJC Device Number.
 *    2. Allocate and Initial MJC cdev struct.
 *    3. Add MJC device to kernel. (call cdev_add)
 *    4. register MJC interrupt
 * PARAMETERS
 *    None.
 * RETURNS
 *    None.
 ****************************************************************************/
static irqreturn_t mjc_intr_dlr(int irq, void *dev_id)
{
    unsigned long ulFlags;

	MJCDBG("mjc_intr_dlr()");

	spin_lock_irqsave(&ContextLock, ulFlags);
	_mjc_SetEvent(&(grContext.rEvent));
	spin_unlock_irqrestore(&ContextLock, ulFlags);

	spin_lock_irqsave(&HWLock, ulFlags);
	_mjc_SetEvent(&(grHWLockContext.rEvent));
	spin_unlock_irqrestore(&HWLock, ulFlags);

    //Gary todo    
    disable_irq_nosync(MJC_TOP_IRQ_BIT_ID);    

    return IRQ_HANDLED;
}


/*****************************************************************************
 * FUNCTION
 *    mjc_probe
 * DESCRIPTION
 *    1. Register MJC Device Number.
 *    2. Allocate and Initial MJC cdev struct.
 *    3. Add MJC device to kernel. (call cdev_add)
 *    4. register MJC interrupt
 * PARAMETERS
 *    None.
 * RETURNS
 *    None.
 ****************************************************************************/
static int mjc_probe(struct platform_device *pDev)
{
    int ret;
	unsigned long ulFlags;

    MJCDBG("mjc_probe()");

	ret = register_chrdev_region(mjc_devno, 1, MJC_DEVNAME);
    if(ret)
    {
        MJCMSG("[ERROR] Can't Get Major number for MJC Device\n");
    }

	g_mjc_cdev = cdev_alloc();
    g_mjc_cdev->owner = THIS_MODULE;
    g_mjc_cdev->ops = &g_mjc_fops;

    ret = cdev_add(g_mjc_cdev, mjc_devno, 1);

	//create /dev/M4U_device automaticly
    pMjcClass = class_create(THIS_MODULE, MJC_DEVNAME);
    if (IS_ERR(pMjcClass)) {
        int ret = PTR_ERR(pMjcClass);
        MJCMSG("Unable to create class, err = %d", ret);
        return ret;
    }
    mjcDevice = device_create(pMjcClass, NULL, mjc_devno, NULL, MJC_DEVNAME);

	spin_lock_irqsave(&ContextLock, ulFlags);
	grContext.rEvent.u4TimeoutMs = 0xFFFFFFFF;
	_mjc_CreateEvent(&(grContext.rEvent));
	spin_unlock_irqrestore(&ContextLock, ulFlags);

	spin_lock_irqsave(&HWLock, ulFlags);
	grHWLockContext.rEvent.u4TimeoutMs = 0xFFFFFFFF;
	_mjc_CreateEvent(&(grHWLockContext.rEvent));
	spin_unlock_irqrestore(&HWLock, ulFlags);

	// register interrupt
	// level and low
	// Gary todo:	 
	if (request_irq(MJC_TOP_IRQ_BIT_ID , (irq_handler_t)mjc_intr_dlr, IRQF_TRIGGER_LOW, MJC_DEVNAME, NULL) < 0)
	{
	    MJCMSG("[ERROR] mjc_probe() error to request dec irq\n");
	}
	else
	{
	    MJCDBG("mjc_probe() success to request dec irq\n");
	}
	disable_irq(MJC_TOP_IRQ_BIT_ID);

	gi4Register = (int)ioremap(MJC_REG_PA, MJC_REG_REGION);

	return 0;
}


/*****************************************************************************
 * FUNCTION
 *    mjc_remove
 * DESCRIPTION
 *    1. Remove mjc device.
 *    2. Un-register mjc Device Number.
 *    3. Free IRQ
 * PARAMETERS
 *	  param1 : [IN] struct platform_device *pdev
 *				  No used in this function.
 * RETURNS
 *    Type: Integer. always zero.
 ****************************************************************************/
static int mjc_remove(struct platform_device *pdev)
{
    unsigned long ulFlags;

    MJCDBG("mjc_remove() \n");

    cdev_del(g_mjc_cdev);
    unregister_chrdev_region(mjc_devno, 1);

    //Release IRQ
    // Gary todo:    
    free_irq(MJC_TOP_IRQ_BIT_ID , NULL);    

    //memory allocated in probe function don't free.

	spin_lock_irqsave(&ContextLock, ulFlags);
    _mjc_CloseEvent(&(grContext.rEvent));
	spin_unlock_irqrestore(&ContextLock, ulFlags);

	spin_lock_irqsave(&HWLock, ulFlags);
    _mjc_CloseEvent(&(grHWLockContext.rEvent));
	spin_unlock_irqrestore(&HWLock, ulFlags);

    return 0;
}

static int mjc_suspend(struct platform_device *pdev, pm_message_t mesg)
{
    MJCMSG("mjc_early_suspend, tid = %d\n", current->pid);

    MJCDBG("mjc_early_suspend - tid = %d\n", current->pid);
	return 0;
}

static int mjc_resume(struct platform_device *pdev)
{

    MJCMSG("mjc_late_resume, tid = %d\n", current->pid);

    MJCDBG("mjc_late_resume - tid = %d\n", current->pid);
	return 0;
}

/*---------------------------------------------------------------------------*/
#ifdef CONFIG_PM
/*---------------------------------------------------------------------------*/
int mjc_pm_suspend(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	MJCDBG("calling %s()\n", __func__);

    BUG_ON(pdev == NULL);

    return mjc_suspend(pdev, PMSG_SUSPEND);
}

int mjc_pm_resume(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	MJCDBG("calling %s()\n", __func__);

    BUG_ON(pdev == NULL);

    return mjc_resume(pdev);
}

extern void mt_irq_set_sens(unsigned int irq, unsigned int sens);
extern void mt_irq_set_polarity(unsigned int irq, unsigned int polarity);
static int mjc_pm_restore_noirq(struct device *device)
{
    //IRQF_TRIGGER_LOW
    //Gary todo     
    mt_irq_set_sens(MJC_TOP_IRQ_BIT_ID, MT_LEVEL_SENSITIVE);
    mt_irq_set_polarity(MJC_TOP_IRQ_BIT_ID, MT_POLARITY_LOW);

    return 0;
}

struct dev_pm_ops mjc_pm_ops = {
    .suspend = mjc_pm_suspend,
    .resume = mjc_pm_resume,
    .freeze = mjc_pm_suspend,
    .thaw = mjc_pm_resume,
    .poweroff = mjc_pm_suspend,
    .restore = mjc_pm_resume,
    .restore_noirq = mjc_pm_restore_noirq
};
/*---------------------------------------------------------------------------*/
#endif /*CONFIG_PM*/
/*---------------------------------------------------------------------------*/

static struct platform_driver mjcDrv = {
    .probe	= mjc_probe,
    .remove	= mjc_remove,
    .suspend= mjc_suspend,
    .resume	= mjc_resume,
    .driver	= {
    .name	= MJC_DEVNAME,
#ifdef CONFIG_PM
    .pm     = &mjc_pm_ops,
#endif
    .owner	= THIS_MODULE,
    }
};


/*****************************************************************************
 * FUNCTION
 *    mjc_driver_init
 * DESCRIPTION
 *
 * PARAMETERS
 *    None.
 * RETURNS
 *    Type: Integer.  zero mean success and others mean fail.
 ****************************************************************************/
static int __init mjc_driver_init(void)
{
    int cnt;
    MJCDBG("mjc_driver_init()");

	if(platform_driver_register(&mjcDrv))
	{
		MJCMSG("failed to register MAU driver");
		return -ENODEV;
    }
    //clear force write register table for NCSTool tuning
    for(cnt=0;cnt<MJC_FORCE_REG_NUM;cnt++)
    {
        gfWriteReg[cnt].reg = 0;
        gfWriteReg[cnt].val = 0;
        gfWriteReg[cnt].mask = 0;                 
    }
	return 0;
}

/*****************************************************************************
 * FUNCTION
 *    mjc_driver_exit
 * DESCRIPTION
 *
 * PARAMETERS
 *    None.
 * RETURNS
 *    None.
 ****************************************************************************/
static void __exit mjc_driver_exit(void)
{
    MJCDBG("mjc_driver_exit()");

    platform_driver_unregister(&mjcDrv);
}

module_init(mjc_driver_init);
module_exit(mjc_driver_exit);

MODULE_AUTHOR("Gary Huang <gary.huang@mediatek.com>");
MODULE_DESCRIPTION("MTK MJC Driver");
MODULE_LICENSE("GPL");
