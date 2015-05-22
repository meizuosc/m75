/* 
 * (C) Copyright 2010
 * MediaTek <www.MediaTek.com>
 *
 * Android Exception Device
 *
 */
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/hardirq.h>
#include <linux/init.h>
#include <linux/kallsyms.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/disp_assert_layer.h>
#include <mach/system.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/aee.h>
#include <linux/seq_file.h>
#include "aed.h"
#include <mach/mt_boot.h>


#ifdef CONFIG_MTK_AEE_POWERKEY_HANG_DETECT
static DEFINE_SPINLOCK(pwk_hang_lock);
static int wdt_kick_status;
static int hwt_kick_times;
static int pwk_start_monitor;
#endif


#ifdef CONFIG_MTK_AEE_POWERKEY_HANG_DETECT
/******************************************************************************
 * FUNCTION PROTOTYPES
 *****************************************************************************/
static long monitor_hang_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
#endif

#ifdef CONFIG_MTK_AEE_POWERKEY_HANG_DETECT
/******************************************************************************
 * hang detect File operations
 *****************************************************************************/
static int monitor_hang_open(struct inode *inode, struct file *filp)
{
	LOGD("%s\n", __func__);	
	return 0;
}

static int monitor_hang_release(struct inode *inode, struct file *filp)
{
	LOGD("%s\n", __func__);	
	return 0;
}

static unsigned int monitor_hang_poll(struct file *file, struct poll_table_struct *ptable)
{
	LOGD("%s\n", __func__);
	return 0;
}

static ssize_t monitor_hang_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	LOGD("%s\n", __func__);
	return 0;
}

static ssize_t monitor_hang_write(struct file *filp, const char __user *buf, size_t count,
			    loff_t *f_pos)
{
	
	LOGD("%s\n", __func__);
	return 0;
}
#endif

extern void aee_kernel_RT_Monitor_api(int lParam);
#define AEEIOCTL_RT_MON_Kick _IOR('p', 0x0A, int)

#ifdef CONFIG_MTK_AEE_POWERKEY_HANG_DETECT

/*
 * aed process daemon and other command line may access me 
 * concurrently
 */
static long monitor_hang_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	if (cmd == AEEIOCTL_WDT_KICK_POWERKEY) {
		if ((int)arg == WDT_SETBY_WMS_DISABLE_PWK_MONITOR) {
			/* pwk_start_monitor=0; */
			/* wdt_kick_status=0; */
			/* hwt_kick_times=0; */
		} else if ((int)arg == WDT_SETBY_WMS_ENABLE_PWK_MONITOR) {
			/* pwk_start_monitor=1; */
			/* wdt_kick_status=0; */
			/* hwt_kick_times=0; */
		} else if ((int)arg < 0xf) {
			aee_kernel_wdt_kick_Powkey_api("Powerkey ioctl",(int)arg);
		}
		
		LOGE("AEEIOCTL_WDT_Kick_Powerkey ( 0x%x)\n", (int)arg);
		
		return ret;

	}	
	
	if (cmd == AEEIOCTL_RT_MON_Kick) {
		#ifdef CONFIG_MTK_AEE_POWERKEY_HANG_DETECT
		LOGE("AEEIOCTL_RT_MON_Kick ( %d)\n", (int)arg);
		aee_kernel_RT_Monitor_api((int)arg);
		#endif
		return ret;
	}

	return ret;
}
#endif

#ifdef CONFIG_MTK_AEE_POWERKEY_HANG_DETECT

static struct file_operations aed_wdt_RT_Monitor_fops = {
	.owner   = THIS_MODULE,
	.open    = monitor_hang_open,
	.release = monitor_hang_release,
	.poll    = monitor_hang_poll,
	.read    = monitor_hang_read,
	.write   = monitor_hang_write,
	.unlocked_ioctl   = monitor_hang_ioctl,
};


static struct file_operations aed_wdt_tick_PowKey_fops = {
	.owner   = THIS_MODULE,
	.open	 = monitor_hang_open,
	.release = monitor_hang_release,
	.poll	 = monitor_hang_poll,
	.read	 = monitor_hang_read,
	.write	 = monitor_hang_write,
	.unlocked_ioctl   = monitor_hang_ioctl,

};
#endif



#ifdef CONFIG_MTK_AEE_POWERKEY_HANG_DETECT
static struct miscdevice aed_wdt_tick_PowKey_dev = {
    .minor   = MISC_DYNAMIC_MINOR,
    .name    = "kick_powerkey",
    .fops    = &aed_wdt_tick_PowKey_fops,
};


static struct miscdevice aed_wdt_RT_Monitor_dev = {
	.minor   = MISC_DYNAMIC_MINOR,
	.name    = "RT_Monitor",
	.fops    = &aed_wdt_RT_Monitor_fops,
};

#endif

/* bleow code is added for monitor_hang_init */
static int	monitor_hang_init (void) ;

#ifdef CONFIG_MTK_AEE_POWERKEY_HANG_DETECT
static int	hang_detect_init (void) ;
#endif
/* bleow code is added for hang detect */



static int __init monitor_hang_init(void)
{
	int err = 0;
	
	#ifdef CONFIG_MTK_AEE_POWERKEY_HANG_DETECT

	err = misc_register(&aed_wdt_RT_Monitor_dev);
	if(unlikely(err)) {
		LOGE("aee: failed to register aed_wdt_RT_Monitor_dev device!\n");
		return err;
	}
	err = misc_register(&aed_wdt_tick_PowKey_dev);
	if(unlikely(err)) {
		LOGE("aee: failed to register aed_wdt_tick_PowKey_dev device!\n");
	return err;
	}
	
	hang_detect_init () ;
	#endif

	return err;
}

static void __exit monitor_hang_exit(void)
{
	#ifdef CONFIG_MTK_AEE_POWERKEY_HANG_DETECT
	int err;
	err = misc_deregister(&aed_wdt_RT_Monitor_dev);
	if (unlikely(err))
		LOGE("failed to unregister RT_Monitor device!\n");
	
	err = misc_deregister(&aed_wdt_tick_PowKey_dev);
	if (unlikely(err))
		LOGE("failed to unregister kick_powerkey device!\n");
	#endif
}

/* For the condition, where kernel is still alive, but system server is not scheduled. */

#define HD_PROC "hang_detect"

/* static DEFINE_SPINLOCK(hd_locked_up); */
#define HD_INTER 30

static int hd_detect_enabled;
static int hd_timeout = 0x7fffffff ;
static int hang_detect_counter = 0x7fffffff ;

#ifdef CONFIG_MTK_AEE_POWERKEY_HANG_DETECT
static int FindTaskByName(char *name)
{
	struct task_struct * task ;
	for_each_process(task) {
		if (task && (strcmp(task->comm, name)==0)) {
			LOGE("[Hang_Detect] %s found:%d.\n", name, task->pid);
			return task->pid;
		}
	}
	LOGE("[Hang_Detect] system_server not found!\n");
	return -1 ;
}

extern void show_stack(struct task_struct *tsk, unsigned long *sp);
static void ShowStatus(void)
{
	struct task_struct * task ;
	for_each_process(task) {
		LOGE("[Hang_Detect] %s found:%d.,RT[%lld]\n", task->comm, task->pid, sched_clock());
		show_stack(task,NULL) ;
	}
}

static int hang_detect_thread(void *arg)
{

	/* unsigned long flags; */
	struct sched_param param = { .sched_priority = RTPM_PRIO_WDT};

	LOGE("[Hang_Detect] hang_detect thread starts.\n");

	sched_setscheduler(current, SCHED_FIFO, &param);

	while (1) {
		if ((1==hd_detect_enabled) && (FindTaskByName("system_server")!=-1)) {
			LOGE("[Hang_Detect] hang_detect thread counts down %d:%d.\n",
			     hang_detect_counter, hd_timeout);


			if (hang_detect_counter<=0)	{
				ShowStatus () ;
			}
			if (hang_detect_counter==0)	{
				LOGE("[Hang_Detect] we should triger	HWT	...	\n") ;

#ifdef CONFIG_MT_ENG_BUILD
				aee_kernel_warning("\nCR_DISPATCH_KEY:SS Hang\n",
						   "we triger	HWT");
				msleep_interruptible (10*1000) ;
#else
				aee_kernel_warning("\nCR_DISPATCH_KEY:SS Hang\n",
						   "we triger	HWT");
				msleep_interruptible (10*1000) ;
				local_irq_disable () ;
			
				while (1);

				BUG	() ;
#endif
			}

			hang_detect_counter -- ;
		} else {
			/* incase of system_server restart, we give 2 mins more.(4*HD_INTER) */
			if (1==hd_detect_enabled) {
				hang_detect_counter = hd_timeout + 4 ;
				hd_detect_enabled = 0 ;
			}
			LOGE("[Hang_Detect] hang_detect disabled.\n") ;
		}
		
		msleep_interruptible((HD_INTER) * 1000);
	}
	return 0 ;
}
#endif

void hd_test (void)
{
	hang_detect_counter = 0 ;
	hd_timeout = 0 ;
}

void aee_kernel_RT_Monitor_api(int lParam)
{
	if (0 == lParam) {
		hd_detect_enabled =	0 ;
		hang_detect_counter = hd_timeout ;    
		LOGE("[Hang_Detect] hang_detect disabled\n") ;
	} else if (lParam > 0) {
		hd_detect_enabled =	1 ;
		hang_detect_counter	= hd_timeout = ((long)lParam + HD_INTER	-1)	/ (HD_INTER) ;
		LOGE("[Hang_Detect] hang_detect enabled %d\n", hd_timeout) ;
	}
}

#if 0
static int hd_proc_cmd_read(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{

	/* with a read of the /proc/hang_detect, we reset the counter. */
	int len = 0 ;
	LOGE("[Hang_Detect] read proc	%d\n", count);

	len = sprintf (buf, "%d:%d\n", hang_detect_counter,hd_timeout) ;

	hang_detect_counter = hd_timeout ;

	return len ;
}

static int hd_proc_cmd_write(struct file *file, const char *buf, unsigned long count, void *data)
{

	/* with a write function , we set the time out, in seconds. */
	/* with a '0' argument, we set it to max int */
	/* with negative number, we will triger a timeout, or for extention functions (future use) */

	int counter = 0 ;
	int retval = 0 ;

	retval = sscanf (buf, "%d", &counter) ;

	LOGE("[Hang_Detect] write	proc %d, original %d: %d\n", counter, hang_detect_counter,
	     hd_timeout);

	if (counter > 0) {
		if (counter%HD_INTER != 0)
			hd_timeout = 1 ;
		else
			hd_timeout = 0 ;

		counter = counter / HD_INTER ;
		hd_timeout += counter ;
	} else if (counter == 0)
		hd_timeout = 0x7fffffff ;
	else if (counter == -1)
		hd_test () ;
	else 
		return count ;

	hang_detect_counter = hd_timeout ;

	return count ;
}
#endif

#ifdef CONFIG_MTK_AEE_POWERKEY_HANG_DETECT
int hang_detect_init(void)
{

	struct task_struct *hd_thread ;
	/* struct proc_dir_entry *de = create_proc_entry(HD_PROC, 0664, 0); */
	
	unsigned char *name = "hang_detect" ;
	
	LOGD("[Hang_Detect] Initialize proc\n");
	
	/* de->read_proc = hd_proc_cmd_read; */
	/* de->write_proc = hd_proc_cmd_write; */

	LOGD("[Hang_Detect] create hang_detect thread\n");

	hd_thread = kthread_create(hang_detect_thread, NULL, name);
	wake_up_process(hd_thread);    

	return 0 ;
}
#endif

int aee_kernel_Powerkey_is_press(void)
{
	int ret=0;
	#ifdef CONFIG_MTK_AEE_POWERKEY_HANG_DETECT
		ret=pwk_start_monitor;
	#endif
	return ret;
}
EXPORT_SYMBOL(aee_kernel_Powerkey_is_press);

void aee_kernel_wdt_kick_Powkey_api(const char *module,  int msg)
{
	#ifdef CONFIG_MTK_AEE_POWERKEY_HANG_DETECT
	spin_lock(&pwk_hang_lock);	
	wdt_kick_status|= msg ;
	spin_unlock(&pwk_hang_lock);
	LOGE("powerkey_kick:%s:%x,%x\r", module,msg,wdt_kick_status);
	#endif
	
}
EXPORT_SYMBOL(aee_kernel_wdt_kick_Powkey_api);


void aee_powerkey_notify_press(unsigned long pressed)
{
	#ifdef CONFIG_MTK_AEE_POWERKEY_HANG_DETECT
	if (pressed)		/* pwk down or up ???? need to check */
	{
		wdt_kick_status=0;
		hwt_kick_times=0;
		pwk_start_monitor=1;
		LOGE("(%s) HW keycode powerkey \n", pressed ? "pressed" : "released");
	}	
	#endif
}
EXPORT_SYMBOL(aee_powerkey_notify_press);


int aee_kernel_wdt_kick_api(int kinterval)
{
	int ret=0;		
#ifdef CONFIG_MTK_AEE_POWERKEY_HANG_DETECT
	if (pwk_start_monitor && (get_boot_mode() == NORMAL_BOOT)
	    && (FindTaskByName("system_server") != -1)) {
		/* Only in normal_boot! */
		LOGE("Press powerkey!!	g_boot_mode=%d,wdt_kick_status=0x%x,tickTimes=0x%x,g_kinterval=%d,RT[%lld]\n",get_boot_mode(),wdt_kick_status,hwt_kick_times,kinterval,sched_clock());
		hwt_kick_times++;	
		if ((kinterval * hwt_kick_times > 180))	/* only monitor 3 min */
		{
			pwk_start_monitor=0;
			/* check all modules is ok~~~ */
			if ((wdt_kick_status & (WDT_SETBY_Display | WDT_SETBY_SF)) !=
			    (WDT_SETBY_Display | WDT_SETBY_SF)) {
				#ifdef CONFIG_MT_ENG_BUILD
				ShowStatus();	/* catch task kernel bt */
				LOGE("[WDK] Powerkey Tick fail,kick_status 0x%08x,RT[%lld]\n ",
				     wdt_kick_status, sched_clock());
				aee_kernel_warning("\nCR_DISPATCH_KEY:UI Hang(Powerkey)\n",
						   "Powerkey Monitor");
				#else
				ShowStatus();	/* catch task kernel bt */
				LOGE("[WDK] Powerkey Tick fail,kick_status 0x%08x,RT[%lld]\n ",
				     wdt_kick_status, sched_clock());
				ret = WDT_PWK_HANG_FORCE_HWT;	/* trigger HWT */
				#endif
			}			
		}
		if ((wdt_kick_status & (WDT_SETBY_Display | WDT_SETBY_SF)) ==
		    (WDT_SETBY_Display | WDT_SETBY_SF)) {
			pwk_start_monitor=0;
			LOGE("[WDK] Powerkey Tick ok,kick_status 0x%08x,RT[%lld]\n ",
			     wdt_kick_status, sched_clock());
		}
			
	}
#endif
	return ret;
}
EXPORT_SYMBOL(aee_kernel_wdt_kick_api);


module_init(monitor_hang_init);
module_exit(monitor_hang_exit);


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek AED Driver");
MODULE_AUTHOR("MediaTek Inc.");
