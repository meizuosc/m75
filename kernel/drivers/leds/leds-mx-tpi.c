/*
 * Touch key & led driver for meizu m65u
 *
 * Copyright (C) 2013 Meizu Technology Co.Ltd, Zhuhai, China
 * Author:		
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
	  
#include <linux/module.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/leds.h>
#include <linux/platform_device.h>
#include <linux/earlysuspend.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include	<linux/mfd/mx_tpi.h>

#define	MODE_PWM			0x00
#define	MODE_SLOPE			0x01
#define	MODE_FADE			0x02
#define	MODE_STATE			0x04

#define	DEVICE_STATE_IDLE		(0)
#define	DEVICE_STATE_NORMAL	(1)
#define	DEVICE_STATE_SLEEP	(2)
#define	DEVICE_STATE_SHUTDOWN	(3)

#define	GET_MODE(x)	((x>>8)&0x0F)


 /*led private data*/
struct mx_tpi_led {
	 struct mx_tpi_data *data;
	 struct led_classdev led_cdev;
	 struct mutex mutex;
	 int id;
#ifdef CONFIG_HAS_EARLYSUSPEND
	 struct early_suspend early_suspend;
#endif
};
static int gSlope = 0;
static int gPWM = 0x00;
 
static int mx_tpi_set_led_pwm(struct led_classdev *led_cdev, int pwm)
{
	struct mx_tpi_led *led =
		 container_of(led_cdev, struct mx_tpi_led, led_cdev);
	struct mx_tpi_data *mx = led->data;
	int ret = 0;
	
	gPWM = pwm;

	pr_info("%s: pwm  = 0x%X\n", __func__,pwm); 

	ret = mx->i2c_writebyte(mx->client, LED_REG_PWM, (u8)pwm);

	return ret;
}
 
static int mx_tpi_set_led_slope(struct led_classdev *led_cdev, int enable)
{
	struct mx_tpi_led *led =
		 container_of(led_cdev, struct mx_tpi_led, led_cdev);
	struct mx_tpi_data *mx = led->data;
	int ret = 0;

	 gSlope = !!enable;

	pr_info("%s: enable  = 0x%X\n", __func__,enable); 
  
	ret = mx->i2c_writebyte(mx->client,LED_REG_SLOPE,enable);	  

	return ret;
}  

static int mx_tpi_set_led_fade(struct led_classdev *led_cdev, int enable)
{
	struct mx_tpi_led *led =
		 container_of(led_cdev, struct mx_tpi_led, led_cdev);
	struct mx_tpi_data *mx = led->data;
	int ret = 0;

	pr_debug("%s: enable  = 0x%X\n", __func__,enable); 

	if(enable )
		ret = mx->i2c_writebyte(mx->client,LED_REG_FADE,enable);	  

	return ret;
}  

static int mx_tpi_set_led_state(struct led_classdev *led_cdev, int state)
{
	 struct mx_tpi_led *led =container_of(led_cdev, struct mx_tpi_led, led_cdev);
	 struct mx_tpi_data *mx = led->data;
	 int mx_state = -1;
	 int ret = 0;
	 
	printk( "%s: 0x%X \n",__func__,state);
	switch( state )
	{
		case DEVICE_STATE_IDLE:
			mx_state  = TPI_STATE_IDLE;
			break;
			
		case DEVICE_STATE_NORMAL:
			mx_state  = TPI_STATE_NORMAL;
			break;
			
		case DEVICE_STATE_SLEEP:
			mx_state  = TPI_STATE_SLEEP;
			break;
			
		case DEVICE_STATE_SHUTDOWN:
			mx_state  = TPI_STATE_SHUTDOWN;
			break;

		default:
			ret = -1;
			break;
	}

	if( mx_state != -1 )
		ret = mx->i2c_writebyte(mx->client,TPI_REG_STATUS,mx_state);

	return ret;	 
}


static void mx_tpi_led_brightness_set(struct led_classdev *led_cdev,
		 enum led_brightness value)
{
	//struct mx_tpi_led *led =container_of(led_cdev, struct mx_tpi_led, led_cdev);
	//struct mx_tpi_data *mx = led->data;
	int ret = 0;
	int mode;
	int data;

	dev_info(led_cdev->dev, "value = 0x%.4X \n",value);
	printk( "mx tpi set brightness 0x%.3X \n",value);

	mode = GET_MODE(value);
	data = value & 0xFF; 

	switch( mode )
	{
	case MODE_PWM:
		ret = mx_tpi_set_led_pwm(led_cdev,data);
		break;

	case MODE_SLOPE:
		ret = mx_tpi_set_led_slope(led_cdev,data);
		break;
		
	case MODE_FADE:
		ret = mx_tpi_set_led_fade(led_cdev,data);
		break;

	case MODE_STATE:
		//ret = mx_tpi_set_led_state(led_cdev,data);	
		break;

	default:		
		dev_err(led_cdev->dev, "mode  %d is valite \n",mode);
		ret = -EINVAL;			
		break;
	}

	if(ret < 0)
	dev_err(led_cdev->dev, "brightness set failed ret = %d \n",ret);

}
 
#ifdef CONFIG_HAS_EARLYSUSPEND
static void mx_tpi_led_early_suspend(struct early_suspend *h)
{
	 struct mx_tpi_led *led =
			 container_of(h, struct mx_tpi_led, early_suspend);
 
	int PWM = gPWM;
	led->led_cdev.brightness_set(&led->led_cdev,((MODE_PWM<<8) | 0)); 	
	gPWM = PWM;
}
 
static void mx_tpi_led_late_resume(struct early_suspend *h)
{
	 struct mx_tpi_led *led =
			 container_of(h, struct mx_tpi_led, early_suspend);
 
	 led->led_cdev.brightness_set(&led->led_cdev,((MODE_PWM<<8) | gPWM)); 	
}
#endif
 
static int mx_tpi_led_probe(struct platform_device *pdev)
{
	 struct mx_tpi_data *data = dev_get_drvdata(pdev->dev.parent);
	 struct mx_tpi_platform_data *pdata = dev_get_platdata(data->dev);
	 struct mx_tpi_led *led;
	 char name[20];
	 int ret;
 
	 if (!pdata) {
		 dev_err(&pdev->dev, "no platform data\n");
		 ret = -ENODEV;
	 }
 
	 led = kzalloc(sizeof(*led), GFP_KERNEL);
	 if (led == NULL) {
		 ret = -ENOMEM;
		 goto err_mem;
	 }
 
	 led->id = pdev->id; 
	 led->data = data;
 
	// snprintf(name, sizeof(name), "%s%d", pdev->name, pdev->id);
	 snprintf(name, sizeof(name), "mx-led");
	 led->led_cdev.name = name;
	 led->led_cdev.brightness = 100;
	 led->led_cdev.max_brightness= 0xFFF;
	 
	 led->led_cdev.brightness_set = mx_tpi_led_brightness_set;
 
	 mutex_init(&led->mutex);
	 platform_set_drvdata(pdev, led);
 
	 ret = led_classdev_register(&pdev->dev, &led->led_cdev);
	 if (ret < 0)
		 goto err_register_led;
 
#ifdef CONFIG_HAS_EARLYSUSPEND
	 led->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	 led->early_suspend.suspend = mx_tpi_led_early_suspend;
	 led->early_suspend.resume = mx_tpi_led_late_resume;
	 register_early_suspend(&led->early_suspend);
#endif

	 led->led_cdev.brightness_set(&led->led_cdev,(led->led_cdev.brightness)); 
	//led->led_cdev.brightness_set(&led->led_cdev,((MODE_FADE<<8) | 1)); 
 
	 return 0;
 
 err_register_led:
	 kfree(led);
 err_mem:
	 return ret;
}
 
static int mx_tpi_led_remove(struct platform_device *pdev)
{
	 struct mx_tpi_led *led = platform_get_drvdata(pdev);
 
#ifdef CONFIG_HAS_EARLYSUSPEND
	 unregister_early_suspend(&led->early_suspend);
#endif
	 led_classdev_unregister(&led->led_cdev);
	 kfree(led);
 
	 return 0;
}
 
const struct platform_device_id mx_tpi_id[] = {
	 { "mx-tpi-led",0 },
	 { },
};
 
static struct platform_driver mx_tpi_led_driver = {
	 .driver = {
		 .name	= "mx-tpi-led",
		 .owner = THIS_MODULE,
	 },
	 .probe  = mx_tpi_led_probe,
	 .remove = mx_tpi_led_remove,
	 .id_table = mx_tpi_id,
};
 
static int __init mx_tpi_led_init(void)
{
	return platform_driver_register(&mx_tpi_led_driver);
}
module_init(mx_tpi_led_init);
 
static void __exit mx_tpi_led_exit(void)
{
	platform_driver_unregister(&mx_tpi_led_driver);
}
module_exit(mx_tpi_led_exit); 


MODULE_AUTHOR("Chwei <Chwei@meizu.com>");
MODULE_DESCRIPTION("MX TPI3 Leds");
MODULE_LICENSE("GPL");
