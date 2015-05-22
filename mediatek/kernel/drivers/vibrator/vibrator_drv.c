/******************************************************************************
 * mt6575_vibrator.c - MT6575 Android Linux Vibrator Device Driver
 * 
 * Copyright 2009-2010 MediaTek Co.,Ltd.
 * 
 * DESCRIPTION:
 *     This file provid the other drivers vibrator relative functions
 *
 ******************************************************************************/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/workqueue.h>

#include "timed_output.h"

#include <linux/hrtimer.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>

#include <linux/jiffies.h>
#include <linux/timer.h>

#include <mach/mt_typedefs.h>
//#include <mach/mt6577_pm_ldo.h>

#include <cust_vibrator.h>
#include <vibrator_hal.h>


#define VERSION					        "v 0.1"
#define VIB_DEVICE	        			"mtk_vibrator"


/******************************************************************************
Error Code No.
******************************************************************************/
#define RSUCCESS        0

/******************************************************************************
Debug Message Settings
******************************************************************************/

/* Debug message event */
#define DBG_EVT_NONE		0x00000000	/* No event */
#define DBG_EVT_INT			0x00000001	/* Interrupt related event */
#define DBG_EVT_TASKLET		0x00000002	/* Tasklet related event */

#define DBG_EVT_ALL			0xffffffff
 
#define DBG_EVT_MASK      	(DBG_EVT_TASKLET)

#if 1
#define MSG(evt, fmt, args...) \
do {	\
	if ((DBG_EVT_##evt) & DBG_EVT_MASK) { \
		printk(fmt, ##args); \
	} \
} while(0)

#define MSG_FUNC_ENTRY(f)	MSG(FUC, "<FUN_ENT>: %s\n", __FUNCTION__)
#else
#define MSG(evt, fmt, args...) do{}while(0)
#define MSG_FUNC_ENTRY(f)	   do{}while(0)
#endif


/******************************************************************************
Global Definations
******************************************************************************/
static struct workqueue_struct *vibrator_queue;
static struct work_struct vibrator_work;
static struct hrtimer vibe_timer;
static spinlock_t vibe_lock;
static int vibe_state;
static int ldo_state;
static int shutdown_flag;


static int vibr_Enable(void)
{
	if(!ldo_state) {
		vibr_Enable_HW();
		ldo_state=1;
	}
	return 0;
}

static int vibr_Disable(void)
{
	if(ldo_state) {
		vibr_Disable_HW();
		ldo_state=0;
	}
   	return 0;
}


static void update_vibrator(struct work_struct *work)
{
	if(!vibe_state)
		vibr_Disable();
	else
		vibr_Enable();
}

static int vibrator_get_time(struct timed_output_dev *dev)
{
	if (hrtimer_active(&vibe_timer)) 
	{
		ktime_t r = hrtimer_get_remaining(&vibe_timer);
		return ktime_to_ms(r);
	} 
	else
		return 0;
}

static void vibrator_enable(struct timed_output_dev *dev, int value)
{
		unsigned long flags;
		
		
#if 1
		//struct vibrator_hw* hw = get_cust_vibrator_hw(); 
		struct vibrator_hw* hw = mt_get_cust_vibrator_hw();

#endif
		printk("[vibrator]vibrator_enable: vibrator first in value = %d\n", value);

		spin_lock_irqsave(&vibe_lock, flags);
		while(hrtimer_cancel(&vibe_timer))
                {
                      printk("[vibrator]vibrator_enable: try to cancel hrtimer \n");
                }

		if (value == 0 || shutdown_flag == 1) {
			printk("[vibrator]vibrator_enable: shutdown_flag = %d\n", shutdown_flag);
			vibe_state = 0;
		}
		else 
		{
#if 1
			printk("[vibrator]vibrator_enable: vibrator cust timer: %d \n", hw->vib_timer);
#ifdef CUST_VIBR_LIMIT
			if(value > hw->vib_limit && value < hw->vib_timer)
				
#else
			if(value >= 10 && value < hw->vib_timer)
#endif
				value = hw->vib_timer;
#endif

			value = (value > 15000 ? 15000 : value);
			vibe_state = 1;
			hrtimer_start(&vibe_timer, 
							ktime_set(value / 1000, (value % 1000) * 1000000),
							HRTIMER_MODE_REL);
		}
		spin_unlock_irqrestore(&vibe_lock, flags);
        printk("[vibrator]vibrator_enable: vibrator start: %d \n", value); 
		queue_work(vibrator_queue, &vibrator_work);
}

static enum hrtimer_restart vibrator_timer_func(struct hrtimer *timer)
{
 		vibe_state = 0;
         printk(KERN_DEBUG "[vibrator]vibrator_timer_func: vibrator will disable \n");
 		queue_work(vibrator_queue, &vibrator_work);
 		return HRTIMER_NORESTART;
}

static struct timed_output_dev mtk_vibrator = 
{
	.name = "vibrator",
	.get_time = vibrator_get_time,
	.enable = vibrator_enable,
};

static int vib_probe(struct platform_device *pdev)
{
	return 0;
}

static int vib_remove(struct platform_device *pdev)
{
	return 0;
}

static void vib_shutdown(struct platform_device *pdev)
{
    unsigned long  flags;
        printk("[vibrator]vib_shutdown: enter!\n");
	spin_lock_irqsave(&vibe_lock, flags);
	shutdown_flag = 1;
	if(vibe_state) {
        	printk("[vibrator]vib_shutdown: vibrator will disable \n");
		vibe_state = 0;
		spin_unlock_irqrestore(&vibe_lock, flags);
		vibr_Disable();
		goto vibr_end;  /*don't spinunlock twice*/
	}
	spin_unlock_irqrestore(&vibe_lock, flags);
vibr_end:
	return;
}
/******************************************************************************
Device driver structure
*****************************************************************************/
static struct platform_driver vibrator_driver = 
{
    .probe		= vib_probe,
	.remove	    = vib_remove,
    .shutdown = vib_shutdown,
    .driver     = {
    .name = VIB_DEVICE,
    .owner = THIS_MODULE,
    },
};

static struct platform_device vibrator_device =
{
    .name = "mtk_vibrator",
    .id   = -1,
};

static ssize_t store_vibr_on(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	if(buf != NULL && size != 0)
	{
		printk("[vibrator]buf is %s and size is %d \n",buf,size);
		if(buf[0]== '0')
		{
			vibr_Disable();
		}else
		{
			vibr_Enable();
		}
	}
	return size;
}

static DEVICE_ATTR(vibr_on, 0220, NULL, store_vibr_on);

/******************************************************************************
 * vib_mod_init
 * 
 * DESCRIPTION:
 *   Register the vibrator device driver ! 
 * 
 * PARAMETERS: 
 *   None
 * 
 * RETURNS: 
 *   None
 * 
 * NOTES: 
 *   RSUCCESS : Success
 * 
 ******************************************************************************/

static int vib_mod_init(void)
{	
	s32 ret;

	printk("MediaTek MTK vibrator driver register, version %s\n", VERSION);
	vibr_power_set();//set vibr voltage if needs.  Before MT6320 vibr default voltage=2.8v but in MT6323 vibr default voltage=1.2v
	ret = platform_device_register(&vibrator_device);
	if (ret != 0){
		printk("[vibrator]Unable to register vibrator device (%d)\n", ret);
        	return ret;
	}

	vibrator_queue = create_singlethread_workqueue(VIB_DEVICE);
	if(!vibrator_queue) {
		printk("[vibrator]Unable to create workqueue\n");
		return -ENODATA;
	}
	INIT_WORK(&vibrator_work, update_vibrator);

	spin_lock_init(&vibe_lock);
	shutdown_flag = 0;
	vibe_state = 0;
	hrtimer_init(&vibe_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	vibe_timer.function = vibrator_timer_func;

	timed_output_dev_register(&mtk_vibrator);

    ret = platform_driver_register(&vibrator_driver);

    if(ret) 
    {
		printk("[vibrator]Unable to register vibrator driver (%d)\n", ret);
		return ret;
    }	

	ret = device_create_file(mtk_vibrator.dev,&dev_attr_vibr_on);
    if(ret)
    {
        printk("[vibrator]device_create_file vibr_on fail! \n");
    }
    
	printk("[vibrator]vib_mod_init Done \n");
 
    return RSUCCESS;
}

/******************************************************************************
 * vib_mod_exit
 * 
 * DESCRIPTION: 
 *   Free the device driver ! 
 * 
 * PARAMETERS: 
 *   None
 * 
 * RETURNS: 
 *   None
 * 
 * NOTES: 
 *   None
 * 
 ******************************************************************************/
 
static void vib_mod_exit(void)
{
	printk("MediaTek MTK vibrator driver unregister, version %s \n", VERSION);
	if(vibrator_queue) {
		destroy_workqueue(vibrator_queue);
	}
	printk("[vibrator]vib_mod_exit Done \n");
}

module_init(vib_mod_init);
module_exit(vib_mod_exit);
MODULE_AUTHOR("MediaTek Inc.");
MODULE_DESCRIPTION("MTK Vibrator Driver (VIB)");
MODULE_LICENSE("GPL");
