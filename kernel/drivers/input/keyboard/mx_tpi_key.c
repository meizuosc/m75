/*
 * Touch key & led driver for meizu m040
 *
 * Copyright (C) 2012 Meizu Technology Co.Ltd, Zhuhai, China
 * Author:		
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/kthread.h>
#include <linux/rtpm_prio.h>
#include <linux/wait.h>
#include <linux/earlysuspend.h>
#include	<linux/mfd/mx_tpi.h>

#include <mach/board.h>
#include <mach/irqs.h>
#include <mach/eint.h>

#include <cust_eint.h>
#include <cust_gpio_usage.h>

#ifndef	CUST_EINT_MCU_INT_NUM

#define CUST_EINT_MCU_INT_NUM              1
#define CUST_EINT_MCU_INT_DEBOUNCE_CN      0
#define CUST_EINT_MCU_INT_TYPE							1//CUST_EINTF_TRIGGER_RISING
#define CUST_EINT_MCU_INT_DEBOUNCE_EN      0//CUST_EINT_DEBOUNCE_DISABLE

#endif

struct mx_tpi_touchkey {
	 struct mx_tpi_data *data;
	struct input_dev *input_key;
	int irq;			/* irq issued by device		*/
	u8 keys_press;
#ifdef CONFIG_HAS_EARLYSUSPEND
	 struct early_suspend early_suspend;
	 int early_suspend_flag;
#endif
	struct task_struct * thread ;
};


void tpi_touch_report_key(struct input_dev *dev, unsigned int code, int value)
{
	printk("%s:KeyCode = %d  S = %d  \n",__func__,code,value);
	input_report_key(dev, code, value);
	input_sync(dev);
}

 
static int tpk_flag = 0;
static int tpk_halt = 0;
static int tpk_eint_mode=1;
static DECLARE_WAIT_QUEUE_HEAD(waiter);
static void tpk_eint_handler(void)
{
	printk("%s ...  \n",__func__);
	tpk_flag=1;
	wake_up_interruptible(&waiter);
}

static int tpk_event_handler(void *data)
{
	struct mx_tpi_touchkey *touchkey = data;
	struct mx_tpi_data *tpi = touchkey->data;
	struct sched_param param = { .sched_priority = RTPM_PRIO_TPD };

	sched_setscheduler(current, SCHED_RR, &param);
	do{
		set_current_state(TASK_INTERRUPTIBLE);

		while (tpk_halt) {
			tpk_flag = 0;
			msleep(20);
		}

		wait_event_interruptible(waiter, tpk_flag != 0);
		tpk_flag = 0;
		
		set_current_state(TASK_RUNNING);

		tpi_touch_report_key(touchkey->input_key,KEY_HOME,1 );
		tpi_touch_report_key(touchkey->input_key,KEY_HOME,0 );
		
		mt_eint_unmask(touchkey->irq);
	}while(1);

	return 0;
}

void tpk_eint_enable(struct mx_tpi_touchkey *touchkey,int enable)
{
	if (enable) 
	{
		mt_eint_set_hw_debounce(touchkey->irq, CUST_EINT_MCU_INT_DEBOUNCE_CN);
		//mt_eint_registration(touchkey->irq, CUST_EINT_MCU_INT_DEBOUNCE_EN, CUST_EINT_POLARITY_LOW, , 1);
		mt_eint_registration(touchkey->irq, CUST_EINT_MCU_INT_TYPE, tpk_eint_handler, 1);
		mt_eint_unmask(touchkey->irq);
	}
	else 
	{
		mt_eint_mask(touchkey->irq);
	}
}

#ifdef CONFIG_HAS_EARLYSUSPEND
 static void mx_tpi_touchkey_early_suspend(struct early_suspend *h)
 {
	 struct mx_tpi_touchkey *touchkey =
			 container_of(h, struct mx_tpi_touchkey, early_suspend);
	struct mx_tpi_data * tpi = touchkey->data;
	struct input_dev *input = touchkey->input_key;
	
	//set_irq_flags(touchkey->irq,IRQF_TRIGGER_LOW);

	touchkey->early_suspend_flag = true;	

}
 
 static void mx_tpi_touchkey_late_resume(struct early_suspend *h)
 {
	 struct mx_tpi_touchkey *touchkey =
			 container_of(h, struct mx_tpi_touchkey, early_suspend);
	struct mx_tpi_data * tpi = touchkey->data;

	touchkey->early_suspend_flag = false;
	//set_irq_flags(touchkey->irq,IRQF_TRIGGER_FALLING|IRQF_TRIGGER_RISING); 
}		
#endif 

static int mx_tpi_touchkey_probe(struct platform_device *pdev)
{
	struct mx_tpi_data *data = dev_get_drvdata(pdev->dev.parent);
	struct mx_tpi_touchkey*touchkey;
	struct input_dev *input_key;
	//struct input_dev *input_pad;

	int err;
	pr_debug("%s:++\n",__func__);
 
	 touchkey = kzalloc(sizeof(struct mx_tpi_touchkey), GFP_KERNEL);
	 input_key = input_allocate_device();
	 if (!touchkey || !input_key) {
		 pr_err("%s:insufficient memory\n",__func__);
		 err = -ENOMEM;
		 goto err_free_mem_key;
	 }
	 
	 touchkey->data = data;
	 touchkey->input_key = input_key;
	 touchkey->irq = data->irq;//client->irq
	 
	platform_set_drvdata(pdev, touchkey);;
	 
	 input_key->name = "mx-tpi-key";
	 input_key->dev.parent = &data->client->dev;
	 input_key->id.bustype = BUS_I2C;
	 input_key->id.vendor = 0x1111;
	
	 __set_bit(EV_KEY, input_key->evbit);
	 __set_bit(KEY_HOME, input_key->keybit);
	input_set_drvdata(input_key, data);
	
	/* Register the input_key device */
	err = input_register_device(touchkey->input_key);
	if (err) {
		 pr_err("%s:Failed to register input key device:%d\n",__func__,err);
		goto err_free_mem_key;
	}	 

	touchkey->thread = kthread_run(tpk_event_handler, touchkey, "mx-tpi-key");
	if ( IS_ERR(touchkey->thread) ) {
		err = PTR_ERR(touchkey->thread);
		pr_err(" %s: failed to create kernel thread: %d\n",__func__, err);
		 goto err_un_input_key;
	}
	tpk_eint_enable(touchkey,true);

#ifdef CONFIG_HAS_EARLYSUSPEND
	 touchkey->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 5;
	 touchkey->early_suspend.suspend = mx_tpi_touchkey_early_suspend;
	 touchkey->early_suspend.resume = mx_tpi_touchkey_late_resume;
	 register_early_suspend(&touchkey->early_suspend);
#endif

	pr_debug("%s:--\n",__func__);
	return 0;
 
err_free_irq:
	//free_irq(client->irq, touchkey);	
	kthread_stop(touchkey->thread);
	tpk_eint_handler();
 err_un_input_key:
	input_unregister_device(touchkey->input_key);
 err_free_mem_key:
	 input_free_device(input_key);
	 kfree(touchkey);
	 return err;

}

static int mx_tpi_touchkey_remove(struct platform_device *pdev)
{
	struct mx_tpi_touchkey * touchkey = platform_get_drvdata(pdev);
	struct mx_tpi_data * tpi = touchkey->data;

#ifdef CONFIG_HAS_EARLYSUSPEND
	 unregister_early_suspend(&touchkey->early_suspend);
#endif

	/* Release IRQ */
	kthread_stop(touchkey->thread);
	tpk_eint_handler();

	input_unregister_device(touchkey->input_key);

	kfree(touchkey);

	return 0;
}

const struct platform_device_id mx_tpi_touchkey_id[] = {
	{ "mx-tpi-key", 0 },
	{ },
};

static struct platform_driver mx_tpi_touchkey_driver = {
	.driver = {
		.name  = "mx-tpi-key",
		.owner = THIS_MODULE,
	},
	.probe = mx_tpi_touchkey_probe,
	.remove = mx_tpi_touchkey_remove,
	.id_table = mx_tpi_touchkey_id,
};

static int __init mx_tpi_touchkey_init(void)
{
	return platform_driver_register(&mx_tpi_touchkey_driver);
}
module_init(mx_tpi_touchkey_init);

static void __exit mx_tpi_touchkey_exit(void)
{
	platform_driver_unregister(&mx_tpi_touchkey_driver);
}
module_exit(mx_tpi_touchkey_exit); 

MODULE_AUTHOR("Chwei <Chwei@meizu.com>");
MODULE_DESCRIPTION("MX QMatrix Sensor Touch Pad");
MODULE_LICENSE("GPL");
