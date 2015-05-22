/*
 *
 * (C) Copyright 2009
 * MediaTek <www.mediatek.com>
 * Charlie Lu <charlie.lu@mediatek.com>
 *
 * VOW Device Driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/earlysuspend.h>
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
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/semaphore.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/xlog.h>
#include <linux/miscdevice.h>
#ifdef SIGTEST
#include <asm/siginfo.h>
#endif

#include "vow.h"

/*****************************************************************************
 * Definition
****************************************************************************/
#define VOW_TAG "VOW:"

//#define VOW_TEMP_UT
//#define VOW_TEMP_UT2 //wakeup by vowcommand
#define VOW_EARLY_SUSPEND_UT
// Type re-definition
#ifndef int8
typedef signed char int8;
#endif

#ifndef uint8
typedef unsigned char uint8;
#endif

#ifndef int16
typedef short int16;
#endif

#ifndef uint16
typedef unsigned short uint16;
#endif

#ifndef int32
typedef int int32;
#endif

#ifndef uint32
typedef unsigned int uint32;
#endif

#ifndef int64
typedef long long int64;
#endif

#ifndef uint64
typedef unsigned long long uint64;
#endif

#define DEBUG_VOWDRV

#ifdef DEBUG_VOWDRV
#define PRINTK_VOWDRV(format, args...) printk(format, ##args )
#else
#define PRINTK_VOWDRV(format, args...)
#endif

/*****************************************************************************
 * Variable Definition
****************************************************************************/
static char   vowdrv_name[]         = "VOW_driver_device";
static int    VowDrv_Flush_counter  = 0;
static int    VowDrv_EINTStatus         = VOW_EINT_DISABLE;
static int    VowDrv_HWStatus   = VOW_PWR_OFF;
static int    VowDrv_EarlySuspend_status = 0;
static uint32 VowDrv_Wait_Queue_flag= 0;
DECLARE_WAIT_QUEUE_HEAD(VowDrv_Wait_Queue);

static DEFINE_SPINLOCK(vowdrv_lock);

#ifdef SIGTEST
#define SIG_TEST 44
struct task_struct *t_vow = NULL;
struct siginfo info_vow;
#endif

/*****************************************************************************
 * Function Definition
****************************************************************************/
static int VowDrv_SetHWStatus(int status)
{
    int ret = 0;
    if((status < NUM_OF_VOW_PWR_STATUS) && (status >= VOW_PWR_OFF))
    {
        spin_lock(&vowdrv_lock);
        VowDrv_HWStatus = status;
        spin_unlock(&vowdrv_lock);
        if (status == VOW_PWR_OFF)
        {
            VowDrv_Wait_Queue_flag = 1;
            wake_up_interruptible(&VowDrv_Wait_Queue);
        }
    }
    else
    {
  	    PRINTK_VOWDRV("VowDrv_SetHWStatus error input:%d \n", status);
        ret = -1;
    }
    return ret;
}

static int VowDrv_GetHWStatus(void)
{
    int ret = 0;
    spin_lock(&vowdrv_lock);
    ret = VowDrv_HWStatus;
    spin_unlock(&vowdrv_lock);
    return ret;
}

int VowDrv_EnableHW(int status)
{
    int ret = 0;
    int pwr_status = 0;
    if (status < 0){
  	    PRINTK_VOWDRV("VowDrv_EnableHW error input:%d \n", status);
        ret = -1;
    }
    else
    {
        pwr_status = (status == 0)?VOW_PWR_OFF:VOW_PWR_ON;
        VowDrv_SetHWStatus(pwr_status);
    }
    return ret;
}

int VowDrv_ChangeStatus(void)
{
    int ret = 0;
    int current_status = VowDrv_GetHWStatus();
    if( current_status != VOW_PWR_ON)
    {
  	    PRINTK_VOWDRV("VowDrv_ChangeStatus error status:%d\nn", current_status);
        ret = -1;
    }
#ifndef VOW_EARLY_SUSPEND_UT
	VowDrv_Wait_Queue_flag =1;
    wake_up_interruptible(&VowDrv_Wait_Queue);
#endif
    VowDrv_SetHWStatus(VOW_PWR_RESET);
    return ret;
}

static int VowDrv_SetEarlySuspendStatus(int status)
{
    int ret = 0;
    spin_lock(&vowdrv_lock);
    if (status)
    {
        VowDrv_EarlySuspend_status = 1;
    }
    else
    {
        VowDrv_EarlySuspend_status = 0;
    }
    spin_unlock(&vowdrv_lock);
    return ret;
}

#ifdef VOW_EARLY_SUSPEND_UT
static int VowDrv_GetEarlySuspendStatus(void)
{
    int ret = 0;
    spin_lock(&vowdrv_lock);
    ret = VowDrv_EarlySuspend_status;
    spin_unlock(&vowdrv_lock);
    return ret;
}
#endif

static int VowDrv_SetVowEINTStatus(int status)
{
    int ret = 0;
    int wakeup_event = 0;
    if (( status <NUM_OF_VOW_EINT_STATUS) && (status >= VOW_EINT_DISABLE ))
    {
        spin_lock(&vowdrv_lock);
        if((VowDrv_EINTStatus != VOW_EINT_PASS) && (status == VOW_EINT_PASS))
        {
            wakeup_event = 1;
        }
        VowDrv_EINTStatus = status;
        spin_unlock(&vowdrv_lock);
#ifdef VOW_TEMP_UT2
        if(wakeup_event == 1)
        {
            VowDrv_ChangeStatus();
        }
#endif
    }
    else
    {
    	  PRINTK_VOWDRV("VowDrv_SetVowEINTStatus error input:%d \n", status);
        ret = -1;
    }
    return ret;
}

static int VowDrv_QueryVowEINTStatus(void)
{
    int ret = 0;
    spin_lock(&vowdrv_lock);
    ret = VowDrv_EINTStatus;
    spin_unlock(&vowdrv_lock);
    PRINTK_VOWDRV("VowDrv_QueryVowEINTStatus :%d \n",ret);
    return ret;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void vow_early_suspend(struct early_suspend *h)
{
    xlog_printk(ANDROID_LOG_INFO, "VOW", "******** MT pmic driver early suspend!! ********\n" );
    VowDrv_SetEarlySuspendStatus(true);
#ifdef VOW_EARLY_SUSPEND_UT
    //if (VowDrv_GetAudDrvVowEnable()==VOW_PWR_ON)
    {
        VowDrv_Wait_Queue_flag =1;
        wake_up_interruptible(&VowDrv_Wait_Queue);
    }
#endif
}

static void vow_early_resume(struct early_suspend *h)
{
    xlog_printk(ANDROID_LOG_INFO, "VOW", "******** MT pmic driver early resume!! ********\n" );
    VowDrv_SetEarlySuspendStatus(false);
}

static struct early_suspend vow_early_suspend_desc = {
    .level        = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1,
    .suspend    = vow_early_suspend,
    .resume        = vow_early_resume,
};
#endif

static int VowDrv_open(struct inode *inode, struct file *fp)
{
    PRINTK_VOWDRV("VowDrv_open do nothing inode:%p, file:%p \n", inode, fp);
    return 0;
}

static int VowDrv_release(struct inode *inode, struct file *fp)
{
    PRINTK_VOWDRV("VowDrv_release inode:%p, file:%p \n", inode, fp);

    if (!(fp->f_mode & FMODE_WRITE || fp->f_mode & FMODE_READ))
    {
        return -ENODEV;
    }
    return 0;
}

static long VowDrv_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
	  int  ret = 0;
    PRINTK_VOWDRV("VowDrv_ioctl cmd = %u arg = %lu\n", cmd, arg);
    switch (cmd)
    {
        case TEST_VOW_PRINT:
        {
            xlog_printk(ANDROID_LOG_INFO, VOW_TAG, "TEST_VOW_PRINT\n");
#ifdef SIGTEST
            pid_t  pid;                   /* user program process id */
            pid = arg;
         	printk("pid = %d\n", pid);

    	    /* send the signal */
    	    memset(&info_vow, 0, sizeof(struct siginfo));
    	    info_vow.si_signo = SIG_TEST;
    	    info_vow.si_code = SI_QUEUE;	// this is bit of a trickery: SI_QUEUE is normally used by sigqueue from user space,
    	    				// and kernel space should use SI_KERNEL. But if SI_KERNEL is used the real_time data
    	    				// is not delivered to the user space signal handler function.
    	    info_vow.si_int = 1234;  		//real time signals may have 32 bits of data.
            printk("+find_task_by_vpid\n");

        	t_vow = find_task_by_vpid(pid);
        	//t = find_task_by_pid_type(PIDTYPE_PID, pid);  //find the task_struct associated with this pid
    	    if(t_vow == NULL){
    		    printk("no such pid\n");
        		return -ENODEV;
         	}
            printk("-find_task_by_vpid\n");
    	    ret = send_sig_info(SIG_TEST, &info_vow, t_vow);    //send the signal
    	    printk("-send_sig_info\n");
        	if (ret < 0) {
        		printk("error sending signal\n");
    	    }
#endif
            break;
        }
        case VOWEINT_GET_BUFSIZE:
        {
            xlog_printk(ANDROID_LOG_INFO, VOW_TAG, "VOWEINT_GET_BUFSIZE\n");
            ret = sizeof(VOW_EINT_DATA_STRUCT);
            break;
        }
        case VOW_GET_STATUS:
        {
            xlog_printk(ANDROID_LOG_INFO, VOW_TAG, "VOW_GET_STATUS\n");
            ret = VowDrv_QueryVowEINTStatus();
            break;
        }
        case VOW_SET_STATUS:
        {
            xlog_printk(ANDROID_LOG_INFO, VOW_TAG, "VOW_SET_STATUS = %lu\n",arg);
            ret = VowDrv_SetVowEINTStatus(arg);
            break;
        }
        default:
        {
        	break;
        }
    }
    return ret;
}

static ssize_t VowDrv_write(struct file *fp, const char __user *data, size_t count, loff_t *offset)
{
    PRINTK_VOWDRV("+VowDrv_write = %p count = %d\n",fp ,count);
    return 0;
}

static ssize_t VowDrv_read(struct file *fp,  char __user *data, size_t count, loff_t *offset)
{
    uint32 read_count = 0;
    int32 ret = 0;

    PRINTK_VOWDRV("+VowDrv_read count = %d, read_count = %d\n",count, read_count);
#ifdef VOW_TEMP_UT
    while(VowDrv_QueryVowStatus()!=VOW_EINT_PASS)
    {
    	PRINTK_VOWDRV("Check Vow Status %d ... \n",VowDrv_QueryVowStatus());
    	msleep(1000);
    }
#else

#ifdef VOW_EARLY_SUSPEND_UT
    if ((VowDrv_Wait_Queue_flag == 0) && (VowDrv_GetEarlySuspendStatus() == 0))
    {
        ret = wait_event_interruptible(VowDrv_Wait_Queue, VowDrv_Wait_Queue_flag);
    }
    if ((VowDrv_Wait_Queue_flag ==1) || (VowDrv_GetEarlySuspendStatus() == 1))
    {
        xlog_printk(ANDROID_LOG_INFO, VOW_TAG, "Wakeup by EINT (%d,%d,%d)\n",VowDrv_Wait_Queue_flag,VowDrv_GetEarlySuspendStatus(),VowDrv_GetHWStatus());
        VowDrv_Wait_Queue_flag =0; //reset signal
        if (VowDrv_GetEarlySuspendStatus() == 1)
        {
            VowDrv_SetVowEINTStatus(VOW_EINT_PASS);
        }
        else
        {
            VowDrv_SetVowEINTStatus(VOW_EINT_RETRY);
        }
    }
    else
    {
        xlog_printk(ANDROID_LOG_INFO, VOW_TAG, "Wakeup by other signal (%d,%d,%d)\n",VowDrv_Wait_Queue_flag,VowDrv_GetEarlySuspendStatus(),VowDrv_GetHWStatus());
        if (VowDrv_Wait_Queue_flag ==1) VowDrv_Wait_Queue_flag =0; //reset signal
    }
#else
    if (VowDrv_Wait_Queue_flag == 0)
    {
        ret = wait_event_interruptible(VowDrv_Wait_Queue, VowDrv_Wait_Queue_flag);
    }
    if ((VowDrv_Wait_Queue_flag ==1) && (VowDrv_GetHWStatus()!=VOW_PWR_OFF))
    {
        VowDrv_Wait_Queue_flag =0;
        VowDrv_SetVowEINTStatus(VOW_EINT_PASS);
        xlog_printk(ANDROID_LOG_INFO, VOW_TAG, "Wakeup by EINT\n");
    }
    else
    {
        xlog_printk(ANDROID_LOG_INFO, VOW_TAG, "Wakeup by other signal(%d,%d)\n",VowDrv_Wait_Queue_flag,VowDrv_GetHWStatus());
        if (VowDrv_Wait_Queue_flag ==1) VowDrv_Wait_Queue_flag =0;//reset signal
    }
#endif //#ifdef VOW_EARLY_SUSPEND_UT

#endif //#ifdef VOW_TEMP_UT
    VOW_EINT_DATA_STRUCT.eint_status = VowDrv_QueryVowEINTStatus();
    read_count = copy_to_user((void __user *)data, &VOW_EINT_DATA_STRUCT, sizeof(VOW_EINT_DATA_STRUCT));

    //Reset status
    VowDrv_SetVowEINTStatus(VOW_EINT_DISABLE);

    PRINTK_VOWDRV("-VowDrv_read count = %d, read_count = %d\n",count, read_count);
    return read_count;
}

static int VowDrv_flush(struct file *flip, fl_owner_t id)
{
    PRINTK_VOWDRV("+VowDrv_flush \n");
    VowDrv_Flush_counter ++;
    PRINTK_VOWDRV("-VowDrv_flush \n");
    return 0;
}

static int VowDrv_fasync(int fd, struct file *flip, int mode)
{
    PRINTK_VOWDRV("VowDrv_fasync \n");
    //return fasync_helper(fd, flip, mode, &VowDrv_fasync);
    return 0;
}

static int VowDrv_remap_mmap(struct file *flip, struct vm_area_struct *vma)
{
    PRINTK_VOWDRV("VowDrv_remap_mmap \n");
    return 0;
}

/*
 * VOW platform driver operations
 */

static int VowDrv_probe(struct platform_device *dev)
{
    PRINTK_VOWDRV("+VowDrv_probe \n");
    return 0;
}

static int VowDrv_remove(struct platform_device *dev)
{
    PRINTK_VOWDRV("+VowDrv_remove \n");
    //[Todo]Add opearations
    PRINTK_VOWDRV("-VowDrv_remove \n");
    return 0;
}

static void VowDrv_shutdown(struct platform_device *dev)
{
    PRINTK_VOWDRV("+VowDrv_shutdown \n");
    PRINTK_VOWDRV("-VowDrv_shutdown \n");
}

static int VowDrv_suspend(struct platform_device *dev, pm_message_t state)
// only one suspend mode
{
	  PRINTK_VOWDRV("VowDrv_suspend \n");
    return 0;
}

static int VowDrv_resume(struct platform_device *dev) // wake up
{
    PRINTK_VOWDRV("VowDrv_resume \n");
    return 0;
}

static struct file_operations VOW_fops =
{
    .owner   = THIS_MODULE,
    .open    = VowDrv_open,
    .release = VowDrv_release,
    .unlocked_ioctl   = VowDrv_ioctl,
    .write   = VowDrv_write,
    .read    = VowDrv_read,
    .flush   = VowDrv_flush,
    .fasync  = VowDrv_fasync,
    .mmap    = VowDrv_remap_mmap
};

static struct miscdevice VowDrv_misc_device =
{
    .minor = MISC_DYNAMIC_MINOR,
    .name = VOW_DEVNAME,
    .fops = &VOW_fops,
};

struct dev_pm_ops VowDrv_pm_ops =
{
    .suspend = NULL,
    .resume = NULL,
    .freeze = NULL,
    .thaw = NULL,
    .poweroff = NULL,
    .restore = NULL,
    .restore_noirq = NULL,
};


static struct platform_driver VowDrv_driver =
{
    .probe    = VowDrv_probe,
    .remove   = VowDrv_remove,
    .shutdown = VowDrv_shutdown,
    .suspend  = VowDrv_suspend,
    .resume   = VowDrv_resume,
    .driver   = {
#ifdef CONFIG_PM
        .pm     = &VowDrv_pm_ops,
#endif
        .name = vowdrv_name,
    },
};

static int VowDrv_mod_init(void)
{
    int ret = 0;
    PRINTK_VOWDRV("+VowDrv_mod_init \n");

    // Register platform DRIVER
    ret = platform_driver_register(&VowDrv_driver);
    if (ret)
    {
        PRINTK_VOWDRV("VowDrv Fail:%d - Register DRIVER \n", ret);
        return ret;
    }

    // register MISC device
    if ((ret = misc_register(&VowDrv_misc_device)))
    {
        PRINTK_VOWDRV("VowDrv_probe misc_register Fail:%d \n", ret);
        return ret;
    }
/*
    // register cat /proc/audio
    create_proc_read_entry("audio",
                           0,
                           NULL,
                           AudDrv_Read_Procmem,
                           NULL);

    create_proc_read_entry("ExtCodec",
                           0,
                           NULL,
                           AudDrv_Read_Proc_ExtCodec,
                           NULL);


    wake_lock_init(&Audio_wake_lock, WAKE_LOCK_SUSPEND, "Audio_WakeLock");
    wake_lock_init(&Audio_record_wake_lock, WAKE_LOCK_SUSPEND, "Audio_Record_WakeLock");
*/

#ifdef CONFIG_HAS_EARLYSUSPEND
    register_early_suspend(&vow_early_suspend_desc);
#endif

    PRINTK_VOWDRV("VowDrv_mod_init: Init Audio WakeLock\n");

    return 0;
}

static void  VowDrv_mod_exit(void)
{
    PRINTK_VOWDRV("+VowDrv_mod_exit \n");
    PRINTK_VOWDRV("-VowDrv_mod_exit \n");
}
module_init(VowDrv_mod_init);
module_exit(VowDrv_mod_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek VOW Driver");
MODULE_AUTHOR("Charlie Lu<charlie.lu@mediatek.com>");
