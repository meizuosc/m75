/*
 * Atmel maXTouch Touchscreen driver
 *
 * Copyright (C) 2010 Samsung Electronics Co.Ltd
 * Copyright (C) 2011-2012 Atmel Corporation
 * Copyright (C) 2012 Google, Inc.
 *
 * Author: Joonyoung Shim <jy0922.shim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
//#include <linux/i2c/atmel_ts641t.h>

#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/dma-mapping.h>
#include "atmel_mxt_ts.h"
#include "tpd.h"

#include<linux/cred.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <asm/current.h>
#include <asm/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/version.h>
#include <linux/notifier.h>
#include <linux/gpio_keys.h>


/* Configuration file */
#define MXT_CFG_MAGIC		"OBP_RAW V1"
#define MXT_CFG_NAME  "atmel_mxt_ts.raw"

#define MXT_UPDATE_CFG  1 
#define MXT_UPDATE_NONE 0

#define HALL_X_P0 26
#define HALL_Y_P0 0
#define HALL_X_P1 1126
#define HALL_Y_P1 1097


#if 0
#define dbg_printk(fmt,args...) \
	printk(KERN_ERR"mxt640T->%s_%d:"fmt,__func__,__LINE__,##args)
#endif

static int _mxt_configure_objects(struct mxt_data *data);

#ifdef __UPDATE_CFG_FS__
int mxt_patch_get (
    char *pPatchName,
    struct firmware **ppPatch,
    int padSzBuf);
#define FW_PATH "/system/etc/firmware/M75MXT.xcfg"
#endif
	

/* Registers */
#define MXT_OBJECT_START	0x07
#define MXT_OBJECT_SIZE		6
#define MXT_INFO_CHECKSUM_SIZE	3
#define MXT_MAX_BLOCK_WRITE	256



/* MXT_GEN_MESSAGE_T5 object */
#define MXT_RPTID_NOMSG		0xff

/* MXT_GEN_COMMAND_T6 field */
#define MXT_COMMAND_RESET	0
#define MXT_COMMAND_BACKUPNV	1
#define MXT_COMMAND_CALIBRATE	2
#define MXT_COMMAND_REPORTALL	3
#define MXT_COMMAND_DIAGNOSTIC	5

/* Define for T6 status byte */
#define MXT_T6_STATUS_RESET	(1 << 7)
#define MXT_T6_STATUS_OFL	(1 << 6)
#define MXT_T6_STATUS_SIGERR	(1 << 5)
#define MXT_T6_STATUS_CAL	(1 << 4)
#define MXT_T6_STATUS_CFGERR	(1 << 3)
#define MXT_T6_STATUS_COMSERR	(1 << 2)



#define MXT_POWER_CFG_RUN		0
#define MXT_POWER_CFG_DEEPSLEEP		1

/* MXT_TOUCH_MULTI_T9 field */
#define MXT_T9_ORIENT		9
#define MXT_T9_RANGE		18

/* MXT_TOUCH_MULTI_T9 status */
#define MXT_T9_UNGRIP		(1 << 0)
#define MXT_T9_SUPPRESS		(1 << 1)
#define MXT_T9_AMP		(1 << 2)
#define MXT_T9_VECTOR		(1 << 3)
#define MXT_T9_MOVE		(1 << 4)
#define MXT_T9_RELEASE		(1 << 5)
#define MXT_T9_PRESS		(1 << 6)
#define MXT_T9_DETECT		(1 << 7)

static struct mutex g_read_mutex ;
static struct mutex g_update_mutex ;
static void mxt_reset_device(struct mxt_data* data,int value);

struct t9_range {
	u16 x;
	u16 y;
} __packed;

/* MXT_TOUCH_MULTI_T9 orient */
#define MXT_T9_ORIENT_SWITCH	(1 << 0)

/* MXT_SPT_COMMSCONFIG_T18 */
#define MXT_COMMS_CTRL		0
#define MXT_COMMS_CMD		1
#define MXT_COMMS_RETRIGEN      (1 << 6)

/* Define for MXT_GEN_COMMAND_T6 */
#define MXT_BOOT_VALUE		0xa5
#define MXT_RESET_VALUE		0x01
#define MXT_BACKUP_VALUE	0x55

/* Define for MXT_PROCI_TOUCHSUPPRESSION_T42 */
#define MXT_T42_MSG_TCHSUP	(1 << 0)

/* T47 Stylus */
#define MXT_TOUCH_MAJOR_T47_STYLUS	1

/* T63 Stylus */
#define MXT_T63_STYLUS_PRESS	(1 << 0)
#define MXT_T63_STYLUS_RELEASE	(1 << 1)
#define MXT_T63_STYLUS_MOVE		(1 << 2)
#define MXT_T63_STYLUS_SUPPRESS	(1 << 3)

#define MXT_T63_STYLUS_DETECT	(1 << 4)
#define MXT_T63_STYLUS_TIP		(1 << 5)
#define MXT_T63_STYLUS_ERASER	(1 << 6)
#define MXT_T63_STYLUS_BARREL	(1 << 7)

#define MXT_T63_STYLUS_PRESSURE_MASK	0x3F

/* T100 Multiple Touch Touchscreen */
#define MXT_T100_CTRL		0
#define MXT_T100_CFG1		1
#define MXT_T100_TCHAUX		3
#define MXT_T100_XRANGE		13
#define MXT_T100_YRANGE		24

#define MXT_T100_CFG_SWITCHXY	(1 << 5)

#define MXT_T100_TCHAUX_VECT	(1 << 0)
#define MXT_T100_TCHAUX_AMPL	(1 << 1)
#define MXT_T100_TCHAUX_AREA	(1 << 2)

#define MXT_T100_DETECT		(1 << 7)
#define MXT_T100_TYPE_MASK	0x70
#define MXT_T100_TYPE_STYLUS	0x20

/* Delay times */
#define MXT_BACKUP_TIME		50	/* msec */
#define MXT_RESET_TIME		200	/* msec */
#define MXT_RESET_TIMEOUT	3000	/* msec */
#define MXT_CRC_TIMEOUT		1000	/* msec */
#define MXT_FW_RESET_TIME	3000	/* msec */
#define MXT_FW_CHG_TIMEOUT	300	/* msec */
#define MXT_WAKEUP_TIME		25	/* msec */
#define MXT_REGULATOR_DELAY	150	/* msec */
#define MXT_POWERON_DELAY	150	/* msec */

/* Command to unlock bootloader */
#define MXT_UNLOCK_CMD_MSB	0xaa
#define MXT_UNLOCK_CMD_LSB	0xdc

/* Bootloader mode status */
#define MXT_WAITING_BOOTLOAD_CMD	0xc0	/* valid 7 6 bit only */
#define MXT_WAITING_FRAME_DATA	0x80	/* valid 7 6 bit only */
#define MXT_FRAME_CRC_CHECK	0x02
#define MXT_FRAME_CRC_FAIL	0x03
#define MXT_FRAME_CRC_PASS	0x04
#define MXT_APP_CRC_FAIL	0x40	/* valid 7 8 bit only */
#define MXT_BOOT_STATUS_MASK	0x3f
#define MXT_BOOT_EXTENDED_ID	(1 << 5)
#define MXT_BOOT_ID_MASK	0x1f

/* Touchscreen absolute values */
#define MXT_MAX_AREA		0xff

#define MXT_PIXELS_PER_MM	20

#define DEBUG_MSG_MAX		200

#define HALL_X_OFFSET 8 
#define HALL_Y_OFFSET 19 
#define HALL_OFFSET_T78 8

int mxt_bootloader_read(struct mxt_data *mxt_data,
			   unsigned char *data,unsigned short length);

static int mxt_bootloader_write(struct mxt_data *mxt_data,
				const u8 * const data, unsigned int length);

int __mxt_write_reg(struct i2c_client *i2c_client,
		 unsigned short addr,unsigned short length,unsigned char *data);


/* Each client has this additional data */


static int mxt_suspend(struct device *dev);
static int mxt_resume(struct device *dev);
static struct mxt_data * g_data=NULL ;
#undef disable_irq
#define disable_irq(x) \
{ \
 if(g_data&&g_data->data&&g_data->data->in.hw_irq_disable){ \
	g_data->data->in.hw_irq_disable();\
 }\
}

#undef enable_irq(x) 

#define enable_irq(x)  \
{ \
  if(g_data&&g_data->data&&g_data->data->in.hw_irq_enable){ \
  g_data->data->in.hw_irq_enable();\
  }\
}

static void mxt_irq_onoff(struct mxt_data *data,int on)
{
	if(!data)
		return ;
	if(on)
		data->data->in.hw_irq_enable();
	else
		data->data->in.hw_irq_disable();
	return ;
}

/*
Get /dev/devices  sysfs kobject struct 

*/
static struct kobject * sysfs_get_devices_parent(void)
{
	struct device *tmp = NULL;
	struct kset *pdevices_kset;
	 
	 tmp = kzalloc(sizeof(*tmp), GFP_KERNEL);
	 if (!tmp){
		 return NULL;
	 }
	 
	 device_initialize(tmp);
	 pdevices_kset = tmp->kobj.kset;
	 kfree(tmp);  
	 return &pdevices_kset->kobj;
}
static void mxt_input_sync(struct input_dev *input_dev)
{
	input_mt_report_pointer_emulation(input_dev, false);
	input_sync(input_dev);
}

static void mxt_reset_slots(struct mxt_data *data)
{
	struct input_dev *input_dev = data->input_dev;
	unsigned int num_mt_slots;
	int id;
	
	if (!input_dev)
		return;	
	
	num_mt_slots = data->num_touchids + data->num_stylusids;

	for (id = 0; id < num_mt_slots; id++) {
		input_mt_slot(input_dev, id);
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 0);
	}
	
	mxt_input_sync(input_dev);
}

static bool mxt_is_report_hall_mode(struct mxt_data*data,int x,int y)
{
	if(data->mode==MXT_HALL_COVER&&(x<HALL_X_P0 \
		||x>HALL_X_P1 ||y>HALL_Y_P1))
		return true ;

	return false ;
}
int mxt_set_sensor_mode(struct mxt_data *data,int mode)
{
   if(!data)
   	  return 0 ;
   
   	mutex_lock(&data->hall_lock);
     data->mode = mode ;
	mutex_unlock(&data->hall_lock);
  return 0;
}

 int mxt_set_hall_mode(struct mxt_data *data,struct mxt_hall_data * range)
{
  struct mxt_object *object= NULL ;
  int retval = -1 ;

 info_printk("+++\n");

  if(!data || !range)
  	return -EIO ;


 object = mxt_get_object(data,MXT_HALL_OBJECT_T78);
 if(!object){
	info_printk("get object T78 error\n");
	return -EIO ;
 }
  retval = __mxt_write_reg(data->client,object->start_address+HALL_OFFSET_T78,
  	         sizeof(range->T78_data),range->T78_data);
  if(retval<0){
	info_printk("write T78 error \n");
	return retval ;
  }
	object = mxt_get_object(data,MXT_HALL_OBJECT_T80);
 if(!object){
	info_printk("get object T80 error\n");
	return -EIO ;
 }
  retval = __mxt_write_reg(data->client,object->start_address,
  	         sizeof(range->T80),&range->T80);
  if(retval<0){
	info_printk("write T80 error \n");
	return retval ;
  }
  
  info_printk("---\n");
  return 0 ;
}

static int mxt_hall_notity_func(struct notifier_block *nb,
			unsigned long action, void *notify_data)
{
	struct mxt_data * mxt_data =
				container_of(nb, struct mxt_data,notify);

	info_printk("hall notify action[%d]\n",action);

   struct mxt_hall_data hall_data ;

	if(action==HALL_COVER){
		/*case4 :screen off->gesture mode->hall cover-->touch deepsleep */
		if(mxt_data->mode==MXT_GESTURE_MODE){
			info_printk("case4 :screen off->gesture mode->hall cover-->touch deepsleep \n");
			mxt_suspend(&mxt_data->client->dev);			
			mxt_set_sensor_mode(mxt_data,MXT_HALL_COVER) ;
			return 0;
		}
		
    	mxt_data->gesture_enable = false ;
		 mxt_set_sensor_mode(mxt_data,MXT_HALL_COVER) ;
		 hall_data.T78_data[0] = 0 ;/*untouch:60ms check */
		 hall_data.T78_data[1] = 0 ;/*touch :10ms check */
		 hall_data.T78_data[2] = 0 ;
		 hall_data.T78_data[3] = 0 ;
		 hall_data.T80 = 0 ;

	}else {	
	 mxt_set_sensor_mode(mxt_data,MXT_HALL_UNCOVER) ;
	 hall_data.T78_data[0] = 0 ;/*untouch:60ms check */
	 hall_data.T78_data[1] = 12 ;/*touch :10ms check */
	 hall_data.T78_data[2] = 3 ;
	 hall_data.T78_data[3] = 1 ;
	 hall_data.T80 = 3 ;
	}
	mxt_set_hall_mode(mxt_data,&hall_data);
 return 0 ;
}


int mxt_register_hall_notify(struct mxt_data *data)
{
#ifdef MXT_HALL_SUPPORT
	data->notify.notifier_call = mxt_hall_notity_func ;
	register_gpio_key_notifier(&(data->notify));
#endif
 return 0 ;
}



static void mxt_early_suspend(struct early_suspend * h)
{
	struct mxt_data * data =
			container_of(h, struct mxt_data,early_suspend);

  info_printk("touch mode [%d]\n",data->mode);
#ifdef M75_TP_GESTURE_SUPPORT
		if(!data->disable_all&&!(data->mode==MXT_HALL_COVER)){
		   mxt_gesture_enable(data);
		   mxt_reset_slots(data);
		   mxt_set_sensor_mode(data,MXT_GESTURE_MODE);
		   return ;/*enter gesture mode */
		}
#endif
	
	/*case1:screen on-->hall cover-->hall mode-->screen off->touch deepsleep */
	/*enter normal mode */
	mxt_suspend(&data->client->dev);
 
	return 0 ;
}
int mxt_write_reg(struct i2c_client *client, u16 reg, u8 val)
{
	return __mxt_write_reg(client, reg, 1, &val);
}

static int mxt_t6_command(struct mxt_data *data, u16 cmd_offset,
			  u8 value, bool wait)
{
	u16 reg;
	u8 command_register;
	int timeout_counter = 0;
	int ret;

	reg = data->T6_address + cmd_offset;

	ret = mxt_write_reg(data->client, reg, value);
	if (ret)
		return ret;

	if (!wait)
		return 0;

	do {
		msleep(20);
		ret = __mxt_read_reg(data->client, reg, 1, &command_register);
		if (ret)
			return ret;
	} while ((command_register != 0) && (timeout_counter++ <= 100));

	if (timeout_counter > 100) {
		dev_err(&data->client->dev, "Command failed!\n");
		return -EIO;
	}

	return 0;
}

static int mxt_early_resume(struct early_suspend *h)
{
	struct mxt_data * data =
			container_of(h, struct mxt_data,early_suspend);

	info_printk("touch mode[%d]\n",data->mode);
#ifdef M75_TP_GESTURE_SUPPORT
		if(!data->disable_all&&(data->mode==MXT_GESTURE_MODE)){
		 mxt_gesture_disable(data);		
		//mxt_reset_slots(data);
		 mxt_set_sensor_mode(data,MXT_NORMAL_MODE);
		return;
		}
#endif
	/*case2: screen off-->hall uncover-->screen on */
	mxt_resume(&data->client->dev);

	if(data->gesture_enable)
		mxt_gesture_disable(data);
	
	/*case3:hall cover-->screen on-->hall mode */
	if(data->mode==MXT_HALL_COVER){
		info_printk("case3:hall cover-->screen on-->hall mode\n");
		mxt_hall_notity_func(&data->notify,HALL_COVER,NULL);
	}
 	data->gesture_enable = false ;
	return 0 ;
}

static inline size_t mxt_obj_size(const struct mxt_object *obj)
{
	return obj->size_minus_one + 1;
}

static inline size_t mxt_obj_instances(const struct mxt_object *obj)
{
	return obj->instances_minus_one + 1;
}

static bool mxt_object_readable(unsigned int type)
{
	switch (type) {
	case MXT_GEN_COMMAND_T6:
	case MXT_GEN_POWER_T7:
	case MXT_GEN_ACQUIRE_T8:
	case MXT_GEN_DATASOURCE_T53:
	case MXT_TOUCH_MULTI_T9:
	case MXT_TOUCH_KEYARRAY_T15:
	case MXT_TOUCH_PROXIMITY_T23:
	case MXT_TOUCH_PROXKEY_T52:
	case MXT_PROCI_GRIPFACE_T20:
	case MXT_PROCG_NOISE_T22:
	case MXT_PROCI_ONETOUCH_T24:
	case MXT_PROCI_TWOTOUCH_T27:
	case MXT_PROCI_GRIP_T40:
	case MXT_PROCI_PALM_T41:
	case MXT_PROCI_TOUCHSUPPRESSION_T42:
	case MXT_PROCI_STYLUS_T47:
	case MXT_PROCG_NOISESUPPRESSION_T48:
	case MXT_SPT_COMMSCONFIG_T18:
	case MXT_SPT_GPIOPWM_T19:
	case MXT_SPT_SELFTEST_T25:
	case MXT_SPT_CTECONFIG_T28:
	case MXT_SPT_USERDATA_T38:
	case MXT_SPT_DIGITIZER_T43:
	case MXT_SPT_CTECONFIG_T46:
		return true;
	default:
		return false;
	}
}

static void mxt_dump_message(struct mxt_data *data, u8 *message)
{
	print_hex_dump(KERN_DEBUG, "MXT MSG:", DUMP_PREFIX_NONE, 16, 1,
		       message, data->T5_msg_size, false);
}

static void mxt_debug_msg_enable(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;

	if (data->debug_v2_enabled)
		return;

	mutex_lock(&data->debug_msg_lock);

	data->debug_msg_data = kcalloc(DEBUG_MSG_MAX,
				data->T5_msg_size, GFP_KERNEL);
	if (!data->debug_msg_data) {
		dev_err(&data->client->dev, "Failed to allocate buffer\n");
		return;
	}

	data->debug_v2_enabled = true;
	mutex_unlock(&data->debug_msg_lock);

	dev_info(dev, "Enabled message output\n");
}

static void mxt_debug_msg_disable(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;

	if (!data->debug_v2_enabled)
		return;

	dev_info(dev, "disabling message output\n");
	data->debug_v2_enabled = false;

	mutex_lock(&data->debug_msg_lock);
	kfree(data->debug_msg_data);
	data->debug_msg_data = NULL;
	data->debug_msg_count = 0;
	mutex_unlock(&data->debug_msg_lock);
	dev_info(dev, "Disabled message output\n");
}

static void mxt_debug_msg_add(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;

	mutex_lock(&data->debug_msg_lock);

	if (!data->debug_msg_data) {
		dev_err(dev, "No buffer!\n");
		return;
	}

	if (data->debug_msg_count < DEBUG_MSG_MAX) {
		memcpy(data->debug_msg_data + data->debug_msg_count * data->T5_msg_size,
		       msg,
		       data->T5_msg_size);
		data->debug_msg_count++;
	} else {
		dev_dbg(dev, "Discarding %u messages\n", data->debug_msg_count);
		data->debug_msg_count = 0;
	}

	mutex_unlock(&data->debug_msg_lock);

	sysfs_notify(&data->client->dev.kobj, NULL, "debug_notify");
}

static ssize_t mxt_debug_msg_write(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr, char *buf, loff_t off,
	size_t count)
{
	return -EIO;
}

static ssize_t mxt_debug_msg_read(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr, char *buf, loff_t off, size_t bytes)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct mxt_data *data = dev_get_drvdata(dev);
	int count;
	size_t bytes_read;

	if (!data->debug_msg_data) {
		dev_err(dev, "No buffer!\n");
		return 0;
	}

	count = bytes / data->T5_msg_size;

	if (count > DEBUG_MSG_MAX)
		count = DEBUG_MSG_MAX;

	mutex_lock(&data->debug_msg_lock);

	if (count > data->debug_msg_count)
		count = data->debug_msg_count;

	bytes_read = count * data->T5_msg_size;

	memcpy(buf, data->debug_msg_data, bytes_read);
	data->debug_msg_count = 0;

	mutex_unlock(&data->debug_msg_lock);

	return bytes_read;
}

static int mxt_debug_msg_init(struct mxt_data *data)
{
	sysfs_bin_attr_init(&data->debug_msg_attr);
	data->debug_msg_attr.attr.name = "debug_msg";
	data->debug_msg_attr.attr.mode = 0666;
	data->debug_msg_attr.read = mxt_debug_msg_read;
	data->debug_msg_attr.write = mxt_debug_msg_write;
	data->debug_msg_attr.size = data->T5_msg_size * DEBUG_MSG_MAX;

	if (sysfs_create_bin_file(&data->client->dev.kobj,
				  &data->debug_msg_attr) < 0) {
		dev_err(&data->client->dev, "Failed to create %s\n",
			data->debug_msg_attr.attr.name);
		return -EINVAL;
	}

	return 0;
}

static void mxt_debug_msg_remove(struct mxt_data *data)
{
	if (data->debug_msg_attr.attr.name)
		sysfs_remove_bin_file(&data->client->dev.kobj,
				      &data->debug_msg_attr);
}

static int mxt_wait_for_completion(struct mxt_data *data,
			struct completion *comp, unsigned int timeout_ms)
{
	struct device *dev = &data->client->dev;
	unsigned long timeout = msecs_to_jiffies(timeout_ms);
	long ret;

	ret = wait_for_completion_interruptible_timeout(comp, timeout);
	if (ret < 0) {
		dev_err(dev, "Wait for completion interrupted.\n");
		return -EINTR;
	} else if (ret == 0) {
		dev_err(dev, "Wait for completion timed out.\n");
		return -ETIMEDOUT;
	}
	return 0;
}

static int mxt_lookup_bootloader_address(struct mxt_data *data, bool retry)
{
	u8 appmode = data->client->addr;
	u8 bootloader;
	u8 family_id = 0;

	if (data->info)
		family_id = data->info->family_id;

	switch (appmode) {
	case 0x4a:
	case 0x4b:
		/* Chips after 1664S use different scheme */
		if (retry || family_id >= 0xa2) {
			bootloader = appmode - 0x24;
			break;
		}
		/* Fall through for normal case */
	case 0x4c:
	case 0x4d:
	case 0x5a:
	case 0x5b:
		bootloader = appmode - 0x26;
		break;
	default:
		dev_err(&data->client->dev,
			"Appmode i2c address 0x%02x not found\n",
			appmode);
		return -EINVAL;
	}

	data->bootloader_addr = bootloader;
	return 0;
}

static int mxt_probe_bootloader(struct mxt_data *data, bool retry)
{
	struct device *dev = &data->client->dev;
	int ret;
	u8 val;
	bool crc_failure;

	
	
	ret = mxt_lookup_bootloader_address(data, retry);
	if (ret)
		return ret;

	ret = mxt_bootloader_read(data, &val, 1);
	if (ret)
		return ret;

	/* Check app crc fail mode */
	crc_failure = (val & ~MXT_BOOT_STATUS_MASK) == MXT_APP_CRC_FAIL;

	dev_err(dev, "Detected bootloader, status:%02X%s\n",
			val, crc_failure ? ", APP_CRC_FAIL" : "");

	return 0;
}

static u8 mxt_get_bootloader_version(struct mxt_data *data, u8 val)
{
	struct device *dev = &data->client->dev;
	u8 buf[3];

	if (val & MXT_BOOT_EXTENDED_ID) {
		if (mxt_bootloader_read(data, &buf[0], 3) != 0) {
			dev_err(dev, "%s: i2c failure\n", __func__);
			return -EIO;
		}

		dev_info(dev, "Bootloader ID:%d Version:%d\n", buf[1], buf[2]);

		return buf[0];
	} else {
		dev_info(dev, "Bootloader ID:%d\n", val & MXT_BOOT_ID_MASK);

		return val;
	}
}

static int mxt_check_bootloader(struct mxt_data *data, unsigned int state,
				bool wait)
{
	struct device *dev = &data->client->dev;
	u8 val;
	int ret;

recheck:
	if (wait) {
		/*
		 * In application update mode, the interrupt
		 * line signals state transitions. We must wait for the
		 * CHG assertion before reading the status byte.
		 * Once the status byte has been read, the line is deasserted.
		 */
		ret = mxt_wait_for_completion(data, &data->bl_completion,
					      MXT_FW_CHG_TIMEOUT);
		if (ret) {
			/*
			 * TODO: handle -EINTR better by terminating fw update
			 * process before returning to userspace by writing
			 * length 0x000 to device (iff we are in
			 * WAITING_FRAME_DATA state).
			 */
			dev_err(dev, "Update wait error %d\n", ret);
			return ret;
		}
	}

	ret = mxt_bootloader_read(data, &val, 1);
	if (ret)
		return ret;

	if (state == MXT_WAITING_BOOTLOAD_CMD)
		val = mxt_get_bootloader_version(data, val);

	switch (state) {
	case MXT_WAITING_BOOTLOAD_CMD:
	case MXT_WAITING_FRAME_DATA:
	case MXT_APP_CRC_FAIL:
		val &= ~MXT_BOOT_STATUS_MASK;
		break;
	case MXT_FRAME_CRC_PASS:
		if (val == MXT_FRAME_CRC_CHECK) {
			goto recheck;
		} else if (val == MXT_FRAME_CRC_FAIL) {
			dev_err(dev, "Bootloader CRC fail\n");
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	if (val != state) {
		dev_err(dev, "Invalid bootloader state %02X != %02X\n",
			val, state);
		return -EINVAL;
	}

	return 0;
}

static int mxt_send_bootloader_cmd(struct mxt_data *data, bool unlock)
{
	int ret;
	u8 buf[2];

	if (unlock) {
		buf[0] = MXT_UNLOCK_CMD_LSB;
		buf[1] = MXT_UNLOCK_CMD_MSB;
	} else {
		buf[0] = 0x01;
		buf[1] = 0x01;
	}

	ret = mxt_bootloader_write(data, buf, 2);
	if (ret)
		return ret;

	return 0;
}

#if 0
static int __mxt_read_reg(struct i2c_client *client,
			       u16 reg, u16 len, void *val)
{
	struct i2c_msg xfer[2];
	u8 buf[2];
	int ret;
	bool retry = false;

	buf[0] = reg & 0xff;
	buf[1] = (reg >> 8) & 0xff;

	/* Write register */
	xfer[0].addr = client->addr;
	xfer[0].flags = 0;
	xfer[0].len = 2;
	xfer[0].buf = buf;

	/* Read data */
	xfer[1].addr = client->addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].len = len;
	xfer[1].buf = val;

retry_read:
	ret = i2c_transfer(client->adapter, xfer, ARRAY_SIZE(xfer));
	if (ret != ARRAY_SIZE(xfer)) {
		if (!retry) {
			dev_err(&client->dev, "%s: i2c retry\n", __func__);
			msleep(MXT_WAKEUP_TIME);
			retry = true;
			goto retry_read;
		} else {
			dev_err(&client->dev, "%s: i2c transfer failed (%d)\n",
				__func__, ret);
			return -EIO;
		}
	}

	return 0;
}

static int __mxt_write_reg(struct i2c_client *client, u16 reg, u16 len,
			   const void *val)
{
	u8 *buf;
	size_t count;
	int ret;
	bool retry = false;

	count = len + 2;
	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	buf[0] = reg & 0xff;
	buf[1] = (reg >> 8) & 0xff;
	memcpy(&buf[2], val, len);

retry_write:
	ret = i2c_master_send(client, buf, count);
	if (ret != count) {
		if (!retry) {
			dev_dbg(&client->dev, "%s: i2c retry\n", __func__);
			msleep(MXT_WAKEUP_TIME);
			retry = true;
			goto retry_write;
		} else {
			dev_err(&client->dev, "%s: i2c send failed (%d)\n",
				__func__, ret);
			ret = -EIO;
		}
	} else {
		ret = 0;
	}

	kfree(buf);
	return ret;
}

#else 
static u8 *gpwDMABuf_va = NULL;
static u32 gpwDMABuf_pa = 0;
static u8 *gprDMABuf_va = NULL;
static u32 gprDMABuf_pa = 0;
static struct i2c_msg *read_msg;
#define I2C_DMA_LIMIT 252
#define I2C_DMA_RBUF_SIZE 4096
#define I2C_DMA_WBUF_SIZE 1024
#define I2C_RETRY_TIMES 3
#define MASK_8BIT 0xFF
static void mxt_dump_gpio(void)
{
	struct mxt_data * data = g_data ;
	if(data&&data->data&&data->data->in.hw_get_gpio_value){
	printk(KERN_ERR"%s:SDA:%d SCL:%d IRQ:%d RST:%d\n",__func__,
				(data->data->in.hw_get_gpio_value(SDA_GPIO_INDX)),
				(data->data->in.hw_get_gpio_value(SCL_GPIO_INDX)),
				(data->data->in.hw_get_gpio_value(IRQ_GPIO_INDX)),
				(data->data->in.hw_get_gpio_value(RST_GPIO_INDX)));
	
	}
	return ;
}


 

 int  __mxt_read_reg(struct i2c_client *i2c_client,
			 unsigned short addr, unsigned short length, unsigned char *data)
{
	 int retval;
	 unsigned char retry;
	 unsigned char buf[2];
	 unsigned char *buf_va = NULL;
	 int full = length / I2C_DMA_LIMIT;
	 int partial = length % I2C_DMA_LIMIT;
	 int total;
	 int last;
	 int ii;
	 struct mxt_data * mxt_data = i2c_get_clientdata(i2c_client);
	 static int msg_length;
 

	mutex_lock(&g_read_mutex);
 
	 if(!gprDMABuf_va){
	   gprDMABuf_va = (u8 *)dma_alloc_coherent(NULL, I2C_DMA_RBUF_SIZE, &gprDMABuf_pa, GFP_KERNEL);
	   if(!gprDMABuf_va){
		 printk("[Error] Allocate DMA I2C Buffer failed!\n");
	   }
	 }
 
	 buf_va = gprDMABuf_va;
 
	 if ((full + 2) > msg_length) {
		 kfree(read_msg);
		 msg_length = full + 2;
		 read_msg = kcalloc(msg_length, sizeof(struct i2c_msg), GFP_KERNEL);
	 }
 
	 read_msg[0].addr = i2c_client->addr;
	 read_msg[0].flags = 0;
	 read_msg[0].len = 2;
	 read_msg[0].buf = &buf[0];
	 read_msg[0].timing = 100;
 
	 if (partial) {
		 total = full + 1;
		 last = partial;
	 } else {
		 total = full;
		 last = I2C_DMA_LIMIT;
	 }
 
	 for (ii = 1; ii <= total; ii++) {
		 read_msg[ii].addr = i2c_client->addr;
		 read_msg[ii].flags = I2C_M_RD;
		 read_msg[ii].len = (ii == total) ? last : I2C_DMA_LIMIT;
		 read_msg[ii].buf = (unsigned char*)(gprDMABuf_pa + I2C_DMA_LIMIT * (ii - 1));
		 read_msg[ii].ext_flag = (i2c_client->ext_flag | I2C_ENEXT_FLAG | I2C_DMA_FLAG);
		 read_msg[ii].timing = 100;
	 }
 
	 buf[0] = addr & MASK_8BIT;
     buf[1] = (addr>>8)& MASK_8BIT ;
	 for (retry = 0; retry < I2C_RETRY_TIMES; retry++) {
		 if (i2c_transfer(i2c_client->adapter, read_msg, (total + 1)) == (total + 1)) {
 
			 retval = length;
			 break;
		 }
		 dev_err(&i2c_client->dev,
				 "%s: I2C retry %d\n",
				 __func__, retry + 1);
		 mxt_dump_gpio();
		 if(mxt_data)
		  mxt_reset_device(mxt_data,2);
		 msleep(20);
	 }
 
	 if (retry == I2C_RETRY_TIMES) {
		 dev_err(&i2c_client->dev,
				 "%s: I2C read over retry limit\n",
				 __func__);
		 retval = -EIO;
	 }
 
	 memcpy(data, buf_va, length);
 
 exit:
		 /* if(gprDMABuf_va){ */
		 /* 		dma_free_coherent(NULL, 4096, gprDMABuf_va, gprDMABuf_pa); */
		 /* 		gprDMABuf_va = NULL; */
		 /* 		gprDMABuf_pa = NULL; */
		 /* } */
	mutex_unlock(&g_read_mutex);
 
	 return 0;
 }
 

  int mxt_bootloader_read(struct mxt_data *mxt_data,
			   unsigned char *data,unsigned short length)
 {
	
#if 0
	 int retval;
	 unsigned char retry;
	struct i2c_client *i2c_client = mxt_data->client ;
	 unsigned char *buf_va = NULL;
	 int full = length / I2C_DMA_LIMIT;
	 int partial = length % I2C_DMA_LIMIT;
	 int total;
	 int last;
	 int ii;
	 static int msg_length;
 

	mutex_lock(&g_read_mutex);
 
	 if(!gprDMABuf_va){
	   gprDMABuf_va = (u8 *)dma_alloc_coherent(NULL, I2C_DMA_RBUF_SIZE, &gprDMABuf_pa, GFP_KERNEL);
	   if(!gprDMABuf_va){
		 printk("[Error] Allocate DMA I2C Buffer failed!\n");
	   }
	 }
 
	 buf_va = gprDMABuf_va;
 
	 if ((full + 2) > msg_length) {
		 kfree(read_msg);
		 msg_length = full + 2;
		 read_msg = kcalloc(msg_length, sizeof(struct i2c_msg), GFP_KERNEL);
	 }
 
	 if (partial) {
		 total = full + 1;
		 last = partial;
	 } else {
		 total = full;
		 last = I2C_DMA_LIMIT;
	 }
 
	 for (ii = 0; ii < total; ii++) {
		 read_msg[ii].addr = mxt_data->bootloader_addr;
		 read_msg[ii].flags = I2C_M_RD;
		 read_msg[ii].len = (ii == total - 1) ? last : I2C_DMA_LIMIT;
		 read_msg[ii].buf = (unsigned char*)(gprDMABuf_pa + I2C_DMA_LIMIT * (ii - 1));
		 read_msg[ii].ext_flag = (i2c_client->ext_flag | I2C_ENEXT_FLAG | I2C_DMA_FLAG);
		 read_msg[ii].timing = 100;
	 }
 
	 for (retry = 0; retry < I2C_RETRY_TIMES; retry++) {
		 if (i2c_transfer(i2c_client->adapter, read_msg, (total )) == (total)) {
 
			 retval = length;
			 break;
		 }
		 dev_err(&i2c_client->dev,
				 "%s: I2C retry %d\n",
				 __func__, retry + 1);
		 mxt_dump_gpio();
		 msleep(20);
	 }
 
	 if (retry == I2C_RETRY_TIMES) {
		 dev_err(&i2c_client->dev,
				 "%s: I2C read over retry limit\n",
				 __func__);
		 retval = -EIO;
	 }
 
	 memcpy(data, buf_va, length);
 
 exit:
		 /* if(gprDMABuf_va){ */
		 /* 		dma_free_coherent(NULL, 4096, gprDMABuf_va, gprDMABuf_pa); */
		 /* 		gprDMABuf_va = NULL; */
		 /* 		gprDMABuf_pa = NULL; */
		 /* } */
	mutex_unlock(&g_read_mutex);
 #endif
	 return 0;
 }


  /**
  * __mxt_write_reg()
  *
  * Called by various functions in this driver, and also exported to
  * other expansion Function modules such as rmi_dev.
  *
  * This function writes data of an arbitrary length to the sensor,
  * starting from an assigned register address of the sensor, via I2C with
  * a retry mechanism.
  */
  int __mxt_write_reg(struct i2c_client *i2c_client,
		 unsigned short addr,unsigned short length,unsigned char *data)
 {
	 int retval;
	 unsigned char retry;
	 //unsigned char buf[length + 1];
	 unsigned char *buf_va = NULL;
	 struct i2c_msg msg ;
	 struct mxt_data * mxt_data = i2c_get_clientdata(i2c_client);
	mutex_lock(&g_read_mutex);
 
	 if(!gpwDMABuf_va){
	   gpwDMABuf_va = (u8 *)dma_alloc_coherent(NULL, I2C_DMA_WBUF_SIZE, &gpwDMABuf_pa, GFP_KERNEL);
	   if(!gpwDMABuf_va){
		 printk("[Error] Allocate DMA I2C Buffer failed!\n");
	   }
	 }
	 buf_va = gpwDMABuf_va;
 
	 

	 msg.addr = i2c_client->addr;
	 msg.flags = 0;
	 msg.len = length + 2;
	 msg.buf = (unsigned char*)gpwDMABuf_pa ;
	 msg.ext_flag= i2c_client->ext_flag|I2C_ENEXT_FLAG|I2C_DMA_FLAG;
	 msg.timing = 100;

	
	 buf_va[0] = addr & MASK_8BIT;
     buf_va[1] = (addr>>8)&MASK_8BIT;
	 memcpy(&buf_va[2],&data[0],length);

	 for (retry = 0; retry < I2C_RETRY_TIMES; retry++) {
		 if (i2c_transfer(i2c_client->adapter, &msg, 1) == 1) {
			 retval = length;
			 break;
		 }
		 dev_err(&i2c_client->dev,
				 "%s: I2C retry %d\n",
				 __func__, retry + 1);		 
		 mxt_dump_gpio();
		 if(mxt_data)
		 mxt_reset_device(mxt_data,2);
		 msleep(20);
	 }
 
	 if (retry == I2C_RETRY_TIMES) {
		 dev_err(&i2c_client->dev,
				 "%s: I2C write over retry limit\n",
				 __func__);
		 retval = -EIO;
	 }
 
 exit:
	 /* if(gpwDMABuf_va){ */
	 /* 			dma_free_coherent(NULL, 1024, gpwDMABuf_va, gpwDMABuf_pa); */
	 /* 			gpwDMABuf_va = NULL; */
	 /* 			gpwDMABuf_pa = NULL; */
	 /* 	} */
	
 	mutex_unlock(&g_read_mutex);
	 return 0;
 }



static int mxt_bootloader_write(struct mxt_data *mxt_data,
				const u8 * const data, unsigned int length)
 {
#if 0
	 int retval;
	 unsigned char retry;
	 //unsigned char buf[length + 1];
	 unsigned char *buf_va = NULL;
	 struct i2c_msg msg ;
	
	struct i2c_client *i2c_client = mxt_data->client ;
	mutex_lock(&g_update_mutex);
 
	 if(!gpwDMABuf_va){
	   gpwDMABuf_va = (u8 *)dma_alloc_coherent(NULL, I2C_DMA_WBUF_SIZE, &gpwDMABuf_pa, GFP_KERNEL);
	   if(!gpwDMABuf_va){
		 printk("[Error] Allocate DMA I2C Buffer failed!\n");
	   }
	 }
	 buf_va = gpwDMABuf_va;
 
	 

	 msg.addr = mxt_data->bootloader_addr;
	 msg.flags = 0;
	 msg.len = length;
	 msg.buf = (unsigned char*)gpwDMABuf_pa ;
	 msg.ext_flag= i2c_client->ext_flag|I2C_ENEXT_FLAG|I2C_DMA_FLAG;
	 msg.timing = 100;

	 memcpy(&buf_va[0],&data[0],length);
 
	 for (retry = 0; retry < I2C_RETRY_TIMES; retry++) {
		 if (i2c_transfer(i2c_client->adapter, &msg, 1) == 1) {
			 retval = length;
			 break;
		 }
		 dev_err(&i2c_client->dev,
				 "%s: I2C retry %d\n",
				 __func__, retry + 1);		 
		 mxt_dump_gpio();
		 msleep(20);
	 }
 
	 if (retry == I2C_RETRY_TIMES) {
		 dev_err(&i2c_client->dev,
				 "%s: I2C write over retry limit\n",
				 __func__);
		 retval = -EIO;
	 }
 
 exit:
	 /* if(gpwDMABuf_va){ */
	 /* 			dma_free_coherent(NULL, 1024, gpwDMABuf_va, gpwDMABuf_pa); */
	 /* 			gpwDMABuf_va = NULL; */
	 /* 			gpwDMABuf_pa = NULL; */
	 /* 	} */
	
 	mutex_unlock(&g_update_mutex);
#endif
	 return 0;
 }


#endif


 
 struct mxt_object *
mxt_get_object(struct mxt_data *data, u8 type)
{
	struct mxt_object *object;
	int i;

	for (i = 0; i < data->info->object_num; i++) {
		object = data->object_table + i;
		if (object->type == type)
			return object;
	}

	dev_warn(&data->client->dev, "Invalid object type T%u\n", type);
	return NULL;
}

static void mxt_proc_t6_messages(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;
	u8 status = msg[1];
	u32 crc = msg[2] | (msg[3] << 8) | (msg[4] << 16);

	if (crc != data->config_crc) {
		data->config_crc = crc;
		dev_dbg(dev, "T6 Config Checksum: 0x%06X\n", crc);
		complete(&data->crc_completion);
	}

	/* Detect transition out of reset */
	if ((data->t6_status & MXT_T6_STATUS_RESET) &&
	    !(status & MXT_T6_STATUS_RESET))
		complete(&data->reset_completion);

	/* Output debug if status has changed */
	if (status != data->t6_status) {
		dev_dbg(dev, "T6 Status 0x%02X%s%s%s%s%s%s%s\n",
			status,
			(status == 0) ? " OK" : "",
			(status & MXT_T6_STATUS_RESET) ? " RESET" : "",
			(status & MXT_T6_STATUS_OFL) ? " OFL" : "",
			(status & MXT_T6_STATUS_SIGERR) ? " SIGERR" : "",
			(status & MXT_T6_STATUS_CAL) ? " CAL" : "",
			(status & MXT_T6_STATUS_CFGERR) ? " CFGERR" : "",
			(status & MXT_T6_STATUS_COMSERR) ? " COMSERR" : "");
		
		if(status & MXT_T6_STATUS_CAL)
			mxt_reset_slots(data); //release all points in calibration for safe
	}
	/* Save current status */
	data->t6_status = status;
}

static void mxt_input_button(struct mxt_data *data, u8 *message)
{
	struct input_dev *input = data->input_dev;
	const struct mxt_platform_data *pdata = data->pdata;
	bool button;
	int i;

	/* do not report events if input device not yet registered */
	if (!data->enable_reporting)
		return;

	/* Active-low switch */
	for (i = 0; i < pdata->t19_num_keys; i++) {
		if (pdata->t19_keymap[i] == KEY_RESERVED)
			continue;
		button = !(message[1] & (1 << i));
		input_report_key(input, pdata->t19_keymap[i], button);
	}
}


static void mxt_proc_t9_message(struct mxt_data *data, u8 *message)
{
	struct device *dev = &data->client->dev;
	struct input_dev *input_dev = data->input_dev;
	int id;
	u8 status;
	int x;
	int y;
	int area;
	int amplitude;
	u8 vector;
	int tool;

	/* do not report events if input device not yet registered */
	if (!data->enable_reporting)
		return;

	id = message[0] - data->T9_reportid_min;
	status = message[1];
	x = (message[2] << 4) | ((message[4] >> 4) & 0xf);
	y = (message[3] << 4) | ((message[4] & 0xf));

	/* Handle 10/12 bit switching */
	if (data->max_x < 1024)
		x >>= 2;
	if (data->max_y < 1024)
		y >>= 2;

	area = message[5];

	amplitude = message[6];
	vector = message[7];

	dev_dbg(dev,
		"[%u] %c%c%c%c%c%c%c%c x: %5u y: %5u area: %3u amp: %3u vector: %02X\n",
		id,
		(status & MXT_T9_DETECT) ? 'D' : '.',
		(status & MXT_T9_PRESS) ? 'P' : '.',
		(status & MXT_T9_RELEASE) ? 'R' : '.',
		(status & MXT_T9_MOVE) ? 'M' : '.',
		(status & MXT_T9_VECTOR) ? 'V' : '.',
		(status & MXT_T9_AMP) ? 'A' : '.',
		(status & MXT_T9_SUPPRESS) ? 'S' : '.',
		(status & MXT_T9_UNGRIP) ? 'U' : '.',
		x, y, area, amplitude, vector);

	input_mt_slot(input_dev, id);

	if (status & MXT_T9_DETECT) {
		/* Multiple bits may be set if the host is slow to read the
		 * status messages, indicating all the events that have
		 * happened */
		if (status & MXT_T9_RELEASE) {
			input_mt_report_slot_state(input_dev,
						   MT_TOOL_FINGER, 0);
			mxt_input_sync(input_dev);
		}

		/* A reported size of zero indicates that the reported touch
		 * is a stylus from a linked Stylus T47 object. */
		if (area == 0) {
			area = MXT_TOUCH_MAJOR_T47_STYLUS;
			tool = MT_TOOL_PEN;
		} else {
			tool = MT_TOOL_FINGER;
		}

		/* Touch active */
		input_mt_report_slot_state(input_dev, tool, 1);
		input_report_abs(input_dev, ABS_MT_POSITION_X, x);
		input_report_abs(input_dev, ABS_MT_POSITION_Y, y);
		input_report_abs(input_dev, ABS_MT_PRESSURE, amplitude);
		input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, area);
		input_report_abs(input_dev, ABS_MT_ORIENTATION, vector);
	} else {
		/* Touch no longer active, close out slot */
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 0);
	}

	set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
	data->update_input = true;
}

static void mxt_proc_t100_message(struct mxt_data *data, u8 *message)
{
	struct device *dev = &data->client->dev;
	struct input_dev *input_dev = data->input_dev;
	int id;
	u8 status;
	int x;
	int y;
	int tool;

	if(data->touch_debug)
		info_printk(" enable reporing %d\n",data->enable_reporting);

	/* do not report events if input device not yet registered */
	if (!data->enable_reporting){
		info_printk("ERROR  enable reporting %d\n",data->enable_reporting);
		return;
	}

	id = message[0] - data->T100_reportid_min - 2;

	/* ignore SCRSTATUS events */
	if (id < 0)
		return;

	status = message[1];
	x = (message[3] << 8) | message[2];
	y = (message[5] << 8) | message[4];

	if(mxt_is_report_hall_mode(data,x,y))
		goto OUT ;
	
	if(data->touch_debug)
	dev_err(dev,
		"[%u] status:%02X x:%u y:%u area:%02X amp:%02X vec:%02X\n",
		id,
		status,
		x, y,
		(data->t100_aux_area) ? message[data->t100_aux_area] : 0,
		(data->t100_aux_ampl) ? message[data->t100_aux_ampl] : 0,
		(data->t100_aux_vect) ? message[data->t100_aux_vect] : 0);

	input_mt_slot(input_dev, id);

	if (status & MXT_T100_DETECT) {
		/* A reported size of zero indicates that the reported touch
		 * is a stylus from a linked Stylus T47 object. */
		
		tool = MT_TOOL_FINGER;

		/* Touch active */
		input_mt_report_slot_state(input_dev, tool, 1);
		input_report_abs(input_dev, ABS_MT_POSITION_X, x);
		input_report_abs(input_dev, ABS_MT_POSITION_Y, y);

		if(data->touch_debug)
		  info_printk("touch_x[%d],touch_y[%d]\n",x,y);
		
		if (data->t100_aux_ampl)
			input_report_abs(input_dev, ABS_MT_PRESSURE,
					 message[data->t100_aux_ampl]+1);

		if (data->t100_aux_area) {
			if (tool == MT_TOOL_PEN)
				input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR,
						 MXT_TOUCH_MAJOR_T47_STYLUS);
			else
				input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR,
						 message[data->t100_aux_area]);
		}

		if (data->t100_aux_vect)
			input_report_abs(input_dev, ABS_MT_ORIENTATION,
					 message[data->t100_aux_vect]);
	} else {
OUT:
		/* Touch no longer active, close out slot */
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 0);
	}
  
	data->update_input = true;
}

#ifdef CONFIG_MTK_LEDS

extern void (*led_trigger_fadeonoff_func)();

#endif

static void mxt_proc_t221_messages(struct mxt_data*data,u8 *msg)
{
	mxt_handler_uni_sw_gesture(data,msg);
}

static void mxt_proc_t93_messages(struct mxt_data*data,u8 *msg)
{
	
	mxt_handler_tap_gesture(data,msg);

}


static void mxt_proc_t25_messages(struct mxt_data *data, u8 *msg)
{
	unsigned char result ;
	if(!data || !msg)
		return ;

	result = *msg ;
	data->T25_result = result ;
	
	switch(result){
	case 0xFE :/*all test passed */
    info_printk("T25 test all passed\n");
	break ;
	case 0x01 :/*AVdd is not present */
	info_printk("T25 AVdd is not present\n");
	break ;
	case 0x12 :/*Pin error */
	info_printk("T25 Pin test error\n");
	break ;
	case 0x17 :/*singal test error */
	info_printk("T25 signal limit fault\n");
	break ;
	default :
		info_printk("T25 other error [%x]\n",result);
	}
	if(result !=0xFE){
		info_printk("msg0(%x),msg1(%x),msg2(%x),msg3(%x),msg4(%x),msg5(%x)\n",
			*msg,*(msg+1),*(msg+2),*(msg+3),*(msg+4),*(msg+5));
	}
	complete(&data->t25_completion);
	return ;

}


static void mxt_proc_t15_messages(struct mxt_data *data, u8 *msg)
{
	struct input_dev *input_dev = data->input_dev;
	struct device *dev = &data->client->dev;
	int key;
	bool curr_state, new_state;
	bool sync = false;
	unsigned long keystates = !!(le32_to_cpu(msg[2]));
	
	/* do not report events if input device not yet registered */
	if (!data->enable_reporting)
		return;
	if(data->mode==MXT_HALL_COVER)
		     return ;
//	for (key = 0; key < data->pdata->t15_num_keys; key++) {
		for (key = 0; key < 1; key++) {
		curr_state = test_bit(key, &data->t15_keystatus);
		new_state = test_bit(key, &keystates);
		if (!curr_state && new_state) {
			dev_err(dev, "T15 key press: %u\n", key);
			__set_bit(key, &data->t15_keystatus);
			input_event(input_dev, EV_KEY,
				    KEY_HOME, 1);
			sync = true;
			#ifdef CONFIG_MTK_LEDS
				if(led_trigger_fadeonoff_func != NULL)
					led_trigger_fadeonoff_func();
			#endif	
		} else if (curr_state && !new_state) {
			dev_err(dev, "T15 key release: %u\n", key);
			__clear_bit(key, &data->t15_keystatus);
			input_event(input_dev, EV_KEY,
				    KEY_HOME, 0);
			sync = true;
		}
	}

	if (sync)
		input_sync(input_dev);
}

static void mxt_proc_t42_messages(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;
	u8 status = msg[1];

	if (status & MXT_T42_MSG_TCHSUP)
		dev_info(dev, "T42 suppress\n");
	else
		dev_info(dev, "T42 normal\n");
}

static int mxt_proc_t48_messages(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;
	u8 status, state;

	status = msg[1];
	state  = msg[4];

	dev_dbg(dev, "T48 state %d status %02X %s%s%s%s%s\n",
			state,
			status,
			(status & 0x01) ? "FREQCHG " : "",
			(status & 0x02) ? "APXCHG " : "",
			(status & 0x04) ? "ALGOERR " : "",
			(status & 0x10) ? "STATCHG " : "",
			(status & 0x20) ? "NLVLCHG " : "");

	return 0;
}

static void mxt_proc_t63_messages(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;
	struct input_dev *input_dev = data->input_dev;
	u8 id;
	u16 x, y;
	u8 pressure;

	/* do not report events if input device not yet registered */
	if (!data->enable_reporting)
		return;

	/* stylus slots come after touch slots */
	id = data->num_touchids + (msg[0] - data->T63_reportid_min);

	if (id < 0 || id > (data->num_touchids + data->num_stylusids)) {
		dev_err(dev, "invalid stylus id %d, max slot is %d\n",
			id, data->num_stylusids);
		return;
	}

	x = msg[3] | (msg[4] << 8);
	y = msg[5] | (msg[6] << 8);
	pressure = msg[7] & MXT_T63_STYLUS_PRESSURE_MASK;

	dev_dbg(dev,
		"[%d] %c%c%c%c x: %d y: %d pressure: %d stylus:%c%c%c%c\n",
		id,
		(msg[1] & MXT_T63_STYLUS_SUPPRESS) ? 'S' : '.',
		(msg[1] & MXT_T63_STYLUS_MOVE)     ? 'M' : '.',
		(msg[1] & MXT_T63_STYLUS_RELEASE)  ? 'R' : '.',
		(msg[1] & MXT_T63_STYLUS_PRESS)    ? 'P' : '.',
		x, y, pressure,
		(msg[2] & MXT_T63_STYLUS_BARREL) ? 'B' : '.',
		(msg[2] & MXT_T63_STYLUS_ERASER) ? 'E' : '.',
		(msg[2] & MXT_T63_STYLUS_TIP)    ? 'T' : '.',
		(msg[2] & MXT_T63_STYLUS_DETECT) ? 'D' : '.');

	input_mt_slot(input_dev, id);

	if (msg[2] & MXT_T63_STYLUS_DETECT) {
		input_mt_report_slot_state(input_dev, MT_TOOL_PEN, 1);
		input_report_abs(input_dev, ABS_MT_POSITION_X, x);
		input_report_abs(input_dev, ABS_MT_POSITION_Y, y);
		input_report_abs(input_dev, ABS_MT_PRESSURE, pressure);
	} else {
		input_mt_report_slot_state(input_dev, MT_TOOL_PEN, 0);
	}

	input_report_key(input_dev, BTN_STYLUS,
			 (msg[2] & MXT_T63_STYLUS_ERASER));
	input_report_key(input_dev, BTN_STYLUS2,
			 (msg[2] & MXT_T63_STYLUS_BARREL));

	set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
	mxt_input_sync(input_dev);
}

static int mxt_proc_message(struct mxt_data *data, u8 *message)
{
	u8 report_id = message[0];
	bool dump = data->debug_enabled;

	if (report_id == MXT_RPTID_NOMSG)
		return 0;
	dbg_printk("report id[%d]\n",report_id);
	if (report_id == data->T6_reportid) {
		mxt_proc_t6_messages(data, message);
	} else if (report_id >= data->T9_reportid_min
	    && report_id <= data->T9_reportid_max) {
		mxt_proc_t9_message(data, message);
	} else if (report_id >= data->T100_reportid_min
	    && report_id <= data->T100_reportid_max) {
		mxt_proc_t100_message(data, message);
	} else if (report_id == data->T19_reportid) {
		mxt_input_button(data, message);
		data->update_input = true;
	} else if (report_id >= data->T63_reportid_min
		   && report_id <= data->T63_reportid_max) {
		mxt_proc_t63_messages(data, message);
	} else if (report_id >= data->T42_reportid_min
		   && report_id <= data->T42_reportid_max) {
		mxt_proc_t42_messages(data, message);
	} else if (report_id == data->T48_reportid) {
		mxt_proc_t48_messages(data, message);
	} else if (report_id >= data->T15_reportid_min
		   && report_id <= data->T15_reportid_max) {
		mxt_proc_t15_messages(data, message);
	}else if(report_id==data->T25_reportid){
         mxt_proc_t25_messages(data,message);
	}else if(report_id==data->T221_reportid&&data->gesture_enable){
		mxt_proc_t221_messages(data,message);
	}else if(report_id==data->T93_reportid&&data->gesture_enable){
		mxt_proc_t93_messages(data,message);
	}else {
		dump = true;
	}
	dbg_printk("\n");
	if (dump)
		mxt_dump_message(data, message);

	if (data->debug_v2_enabled)
		mxt_debug_msg_add(data, message);
 dbg_printk("----\n");
	return 1;
}

static int mxt_read_and_process_messages(struct mxt_data *data, u8 count)
{
	struct device *dev = &data->client->dev;
	int ret;
	int i;
	u8 num_valid = 0;

	/* Safety check for msg_buf */
	if (count > data->max_reportid)
		return -EINVAL;

	/* Process remaining messages if necessary */
	ret = __mxt_read_reg(data->client, data->T5_address,
				data->T5_msg_size * count, data->msg_buf);
	if (ret) {
		dev_err(dev, "Failed to read %u messages (%d)\n", count, ret);
		return ret;
	}

	for (i = 0;  i < count; i++) {
		ret = mxt_proc_message(data,
			data->msg_buf + data->T5_msg_size * i);

		if (ret == 1)
			num_valid++;
	}

	/* return number of messages read */
	return num_valid;
}

static irqreturn_t mxt_process_messages_t44(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	int ret;
	u8 count, num_left;
	dbg_printk("+++\n");
	/* Read T44 and T5 together */
	ret = __mxt_read_reg(data->client, data->T44_address,
		data->T5_msg_size + 1, data->msg_buf);
	if (ret) {
		dev_err(dev, "Failed to read T44 and T5 (%d)\n", ret);
		return IRQ_NONE;
	}
	
	count = data->msg_buf[0];

	if (count == 0) {
		//dbg_printk("Interrupt triggered but zero messages\n");
		return IRQ_NONE;
	} else if (count > data->max_reportid) {
		dev_err(dev, "T44 count %d exceeded max report id\n", count);
		count = data->max_reportid;
	}

	dbg_printk("T44 count %d report id\n", count);


	/* Process first message */
	ret = mxt_proc_message(data, data->msg_buf + 1);
	if (ret < 0) {
		dev_warn(dev, "Unexpected invalid message\n");
		return IRQ_NONE;
	}

	num_left = count - 1;

	/* Process remaining messages if necessary */
	if (num_left) {
		ret = mxt_read_and_process_messages(data, num_left);
		if (ret < 0)
			goto end;
		else if (ret != num_left)
			dev_warn(dev, "Unexpected invalid message\n");
	}

end:
	dbg_printk("\n");
	if (data->update_input) {
		mxt_input_sync(data->input_dev);
		data->update_input = false;
	}
	dbg_printk("\n");
	return IRQ_HANDLED;
}

static int mxt_process_messages_until_invalid(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	int count, read;
	u8 tries = 2;

	count = data->max_reportid;

	/* Read messages until we force an invalid */
	do {
		read = mxt_read_and_process_messages(data, count);
		if (read < count)
			return 0;
	} while (--tries);

	if (data->update_input) {
		mxt_input_sync(data->input_dev);
		data->update_input = false;
	}

	dev_err(dev, "CHG pin isn't cleared\n");
	return -EBUSY;
}

static irqreturn_t mxt_process_messages(struct mxt_data *data)
{
	int total_handled, num_handled;
	u8 count = data->last_message_count;

	if (count < 1 || count > data->max_reportid)
		count = 1;

	/* include final invalid message */
	total_handled = mxt_read_and_process_messages(data, count + 1);
	if (total_handled < 0)
		return IRQ_NONE;
	/* if there were invalid messages, then we are done */
	else if (total_handled <= count)
		goto update_count;

	/* read two at a time until an invalid message or else we reach
	 * reportid limit */
	do {
		num_handled = mxt_read_and_process_messages(data, 2);
		if (num_handled < 0)
			return IRQ_NONE;

		total_handled += num_handled;

		if (num_handled < 2)
			break;
	} while (total_handled < data->num_touchids);

update_count:
	data->last_message_count = total_handled;

	if (data->enable_reporting && data->update_input) {
		mxt_input_sync(data->input_dev);
		data->update_input = false;
	}

	return IRQ_HANDLED;
}

static int mxt_interrupt(void *arg)
{
	struct mxt_data *data = (struct mxt_data*)arg;
	
	mutex_lock(&g_update_mutex);
	if (data->in_bootloader) {
		/* bootloader state transition completion */
		complete(&data->bl_completion);
		goto OUT;
	}

	if (!data->object_table)
		goto OUT;

	if (data->T44_address) {
		 mxt_process_messages_t44(data);
	} else {
		 mxt_process_messages(data);
	}
OUT :
	mutex_unlock(&g_update_mutex);
	return IRQ_HANDLED ;
}


static int mxt_soft_reset(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	int ret = 0;

	dev_info(dev, "Resetting chip\n");

	INIT_COMPLETION(data->reset_completion);

	ret = mxt_t6_command(data, MXT_COMMAND_RESET, MXT_RESET_VALUE, false);
	if (ret)
		return ret;

	ret = mxt_wait_for_completion(data, &data->reset_completion,
				      MXT_RESET_TIMEOUT);
	if (ret)
		return ret;

	return 0;
}

static void mxt_update_crc(struct mxt_data *data, u8 cmd, u8 value)
{
	/* on failure, CRC is set to 0 and config will always be downloaded */
	data->config_crc = 0;
	INIT_COMPLETION(data->crc_completion);
	 
	mxt_t6_command(data, cmd, value, true);

	/* Wait for crc message. On failure, CRC is set to 0 and config will
	 * always be downloaded */
	mxt_wait_for_completion(data, &data->crc_completion, MXT_CRC_TIMEOUT);
}

static void mxt_calc_crc24(u32 *crc, u8 firstbyte, u8 secondbyte)
{
	static const unsigned int crcpoly = 0x80001B;
	u32 result;
	u32 data_word;

	data_word = (secondbyte << 8) | firstbyte;
	result = ((*crc << 1) ^ data_word);

	if (result & 0x1000000)
		result ^= crcpoly;

	*crc = result;
}

static u32 mxt_calculate_crc(u8 *base, off_t start_off, off_t end_off)
{
	u32 crc = 0;
	u8 *ptr = base + start_off;
	u8 *last_val = base + end_off - 1;

	if (end_off < start_off)
		return -EINVAL;

	while (ptr < last_val) {
		mxt_calc_crc24(&crc, *ptr, *(ptr + 1));
		ptr += 2;
	}

	/* if len is odd, fill the last byte with 0 */
	if (ptr == last_val)
		mxt_calc_crc24(&crc, *ptr, 0);

	/* Mask to 24-bit */
	crc &= 0x00FFFFFF;

	return crc;
}

static int mxt_check_retrigen(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	int error;
	int val;

	if (data->pdata->irqflags & IRQF_TRIGGER_LOW)
		return 0;

	if (data->T18_address) {
		error = __mxt_read_reg(client,
				       data->T18_address + MXT_COMMS_CTRL,
				       1, &val);
		if (error)
			return error;

		if (val & MXT_COMMS_RETRIGEN)
			return 0;
	}

	dev_warn(&client->dev, "Enabling RETRIGEN workaround\n");
	data->use_retrigen_workaround = true;
	return 0;
}

static int mxt_init_t7_power_cfg(struct mxt_data *data);

/*
 * mxt_check_reg_init - download configuration to chip
 *
 * Atmel Raw Config File Format
 *
 * The first four lines of the raw config file contain:
 *  1) Version
 *  2) Chip ID Information (first 7 bytes of device memory)
 *  3) Chip Information Block 24-bit CRC Checksum
 *  4) Chip Configuration 24-bit CRC Checksum
 *
 * The rest of the file consists of one line per object instance:
 *   <TYPE> <INSTANCE> <SIZE> <CONTENTS>
 *
 *   <TYPE> - 2-byte object type as hex
 *   <INSTANCE> - 2-byte object instance number as hex
 *   <SIZE> - 2-byte object size as hex
 *   <CONTENTS> - array of <SIZE> 1-byte hex values
 */
static int mxt_check_reg_init(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	struct mxt_info cfg_info;
	struct mxt_object *object;
	const struct firmware *cfg = NULL;
	int ret;
	int offset;
	int data_pos;
	int byte_offset;
	int i;
	int cfg_start_ofs;
	u32 info_crc, config_crc, calculated_crc;
	u8 *config_mem;
	size_t config_mem_size;
	unsigned int type, instance, size;
	u8 val;
	u16 reg;
	u16 T38_addr =NULL ;
    int addr_offset ,config_offset;
    u8 *object_offset ;
	char ram_buf[256] ;

	
	if (!data->cfg_name) {
		dev_err(dev, "Skipping cfg download\n");
		return 0;
	}
	
    dev_err(dev,"Ready to request FW\n");
	ret = request_firmware(&cfg, data->cfg_name, dev);
	if (ret < 0) {
		dev_err(dev, "Failure to request config file %s\n",
			data->cfg_name);
		return 0;
	}else {
		dev_err(dev,"request firmware %s success\n",data->cfg_name);
	}

	mxt_update_crc(data, MXT_COMMAND_REPORTALL, 1);

	if (strncmp(cfg->data, MXT_CFG_MAGIC, strlen(MXT_CFG_MAGIC))) {
		dev_err(dev, "Unrecognised config file\n");
		ret = -EINVAL;
		goto release;
	}

	data_pos = strlen(MXT_CFG_MAGIC);

	/* Load information block and check */
	for (i = 0; i < sizeof(struct mxt_info); i++) {
		ret = sscanf(cfg->data + data_pos, "%hhx%n",
			     (unsigned char *)&cfg_info + i,
			     &offset);
		if (ret != 1) {
			dev_err(dev, "Bad format\n");
			ret = -EINVAL;
			goto release;
		}
	   //dev_err(dev,"write cfg to ic offset[%d]\n",offset);
		data_pos += offset;
	}

	if (cfg_info.family_id != data->info->family_id) {
		dev_err(dev, "Family ID mismatch!\n");
		ret = -EINVAL;
		goto release;
	}

	if (cfg_info.variant_id != data->info->variant_id) {
		dev_err(dev, "Variant ID mismatch!\n");
		ret = -EINVAL;
		goto release;
	}

	/* Read CRCs */
	ret = sscanf(cfg->data + data_pos, "%x%n", &info_crc, &offset);
	if (ret != 1) {
		dev_err(dev, "Bad format: failed to parse Info CRC\n");
		ret = -EINVAL;
		goto release;
	}
	data_pos += offset;

	ret = sscanf(cfg->data + data_pos, "%x%n", &config_crc, &offset);
	if (ret != 1) {
		dev_err(dev, "Bad format: failed to parse Config CRC\n");
		ret = -EINVAL;
		goto release;
	}
	data_pos += offset;

	/* The Info Block CRC is calculated over mxt_info and the object table
	 * If it does not match then we are trying to load the configuration
	 * from a different chip or firmware version, so the configuration CRC
	 * is invalid anyway. */
	if (info_crc == data->info_crc) {
		if (config_crc == 0 || data->config_crc == 0) {
			dev_info(dev, "CRC zero, attempting to apply config\n");
		} else if ((config_crc == data->config_crc)&&(!data->force_update)) {
			dev_info(dev, "Config CRC 0x%06X: OK\n",
				 data->config_crc);
			ret = MXT_UPDATE_NONE;
			goto release;
		} else {
			dev_info(dev, "Config CRC 0x%06X: does not match file 0x%06X\n",
				 data->config_crc, config_crc);
		}
	} else {
		dev_warn(dev,
			 "Warning: Info CRC error - device=0x%06X file=0x%06X\n",
			data->info_crc, info_crc);
	}

	/* Malloc memory to store configuration */
	cfg_start_ofs = MXT_OBJECT_START
		+ data->info->object_num * sizeof(struct mxt_object)
		+ MXT_INFO_CHECKSUM_SIZE;
	config_mem_size = data->mem_size - cfg_start_ofs;
	config_mem = kzalloc(config_mem_size, GFP_KERNEL);
	if (!config_mem) {
		dev_err(dev, "Failed to allocate memory\n");
		ret = -ENOMEM;
		goto release;
	}

	while (data_pos < cfg->size) {
		/* Read type, instance, length */
		ret = sscanf(cfg->data + data_pos, "%x %x %x%n",
			     &type, &instance, &size, &offset);
#ifdef MXT_UPDATE_BY_OBJECT
		object_offset = data->object_buf ;
#endif
		if (ret == 0) {
			/* EOF */
			break;
		} else if (ret != 3) {
			dev_err(dev, "Bad format: failed to parse object\n");
			ret = -EINVAL;
			goto release_mem;
		}
		data_pos += offset;

		object = mxt_get_object(data, type);
		if (!object) {
			/* Skip object */
			for (i = 0; i < size; i++) {
				ret = sscanf(cfg->data + data_pos, "%hhx%n",
					     &val,
					     &offset);
				data_pos += offset;
			}
			continue;
		}
		if(type==MXT_SPT_USERDATA_T38){
			T38_addr = object->start_address ;
		}
			
		if (size > mxt_obj_size(object)) {
			/* Either we are in fallback mode due to wrong
			 * config or config from a later fw version,
			 * or the file is corrupt or hand-edited */
			dev_warn(dev, "Discarding %u byte(s) in T%u\n",
				 size - mxt_obj_size(object), type);
		} else if (mxt_obj_size(object) > size) {
			/* If firmware is upgraded, new bytes may be added to
			 * end of objects. It is generally forward compatible
			 * to zero these bytes - previous behaviour will be
			 * retained. However this does invalidate the CRC and
			 * will force fallback mode until the configuration is
			 * updated. We warn here but do nothing else - the
			 * malloc has zeroed the entire configuration. */
			dev_warn(dev, "Zeroing %u byte(s) in T%d\n",
				 mxt_obj_size(object) - size, type);
		}

		if (instance >= mxt_obj_instances(object)) {
			dev_err(dev, "Object instances exceeded!\n");
			ret = -EINVAL;
			goto release_mem;
		}

		reg = object->start_address + mxt_obj_size(object) * instance;

		for (i = 0; i < size; i++) {
			ret = sscanf(cfg->data + data_pos, "%hhx%n",
				     &val,
				     &offset);
			if (ret != 1) {
				dev_err(dev, "Bad format in T%d\n", type);
				ret = -EINVAL;
				goto release_mem;
			}
			data_pos += offset;

			if (i > mxt_obj_size(object))
				continue;

			byte_offset = reg + i - cfg_start_ofs;

			if ((byte_offset >= 0)
			    && (byte_offset <= config_mem_size)) {
				*(config_mem + byte_offset) = val;
		#ifdef MXT_UPDATE_BY_OBJECT
				*(object_offset++)=val ;
		#endif
				
			} else {
				dev_err(dev, "Bad object: reg:%d, T%d, ofs=%d\n",
					reg, object->type, byte_offset);
				ret = -EINVAL;
				goto release_mem;
			}
		}
#ifdef MXT_UPDATE_BY_OBJECT
#if 0
		if(object->type==MXT_SPT_USERDATA_T38){
			dev_err(dev,"!!!!ERROR:image file include T38\n");
			continue ;
		}
#endif
		ret = __mxt_write_reg(data->client,reg,size,data->object_buf);
		if(ret!=0){
			dev_err(dev,"write object[%d] error\n",object->type);
			goto release_mem ;
		}
#endif
	}

	/* calculate crc of the received configs (not the raw config file) */
	if (data->T7_address < cfg_start_ofs) {
		dev_err(dev, "Bad T7 address, T7addr = %x, config offset %x\n",
			data->T7_address, cfg_start_ofs);
		ret = 0;
		goto release_mem;
	}

	calculated_crc = mxt_calculate_crc(config_mem,
					   data->T7_address - cfg_start_ofs,
					   config_mem_size);

	if (config_crc > 0 && (config_crc != calculated_crc))
		dev_err(dev, "Config CRC error, calculated=%06X, file=%06X\n",
			 calculated_crc, config_crc);
	
	dev_err(dev,"Need Update config file to IC\n");
	/* Write configuration as blocks */
	byte_offset = 0;
#ifndef MXT_UPDATE_BY_OBJECT
	while (byte_offset < config_mem_size) {
		size = config_mem_size - byte_offset;

	    
		if (size > MXT_MAX_BLOCK_WRITE)
			size = MXT_MAX_BLOCK_WRITE;
		
		addr_offset = cfg_start_ofs + byte_offset ;
		config_offset = config_mem + byte_offset ;
		
		ret = __mxt_write_reg(data->client,
				      cfg_start_ofs + byte_offset,
				      size, config_mem + byte_offset);
		if (ret != 0) {
			dev_err(dev, "Config write error, ret=%d\n", ret);
			goto release_mem;
		}
		
		 byte_offset += size;
		
	}
#endif

	mxt_update_crc(data, MXT_COMMAND_BACKUPNV, MXT_BACKUP_VALUE);

	ret = mxt_check_retrigen(data);
	if (ret)
		goto release_mem;

	ret = mxt_soft_reset(data);
	if (ret)
		goto release_mem;
	ret = MXT_UPDATE_CFG ;
	
	dev_err(dev, "Config written success\n");

	/* T7 config may have changed */
	mxt_init_t7_power_cfg(data);

release_mem:
	kfree(config_mem);
release:
	release_firmware(cfg);
	
	return ret;
}

static int mxt_set_t7_power_cfg(struct mxt_data *data, u8 sleep)
{
	struct device *dev = &data->client->dev;
	int error;
	struct t7_config *new_config;
	struct t7_config deepsleep = { .active = 0, .idle = 0 };
	if (sleep == MXT_POWER_CFG_DEEPSLEEP)
		new_config = &deepsleep;
	else
		new_config = &data->t7_cfg;
	
	error = __mxt_write_reg(data->client, data->T7_address,
			sizeof(data->t7_cfg),
			new_config);
	if (error)
		return error;
	dev_dbg(dev, "Set T7 ACTV:%d IDLE:%d\n",
		new_config->active, new_config->idle);

	return 0;
}

static int mxt_init_t7_power_cfg(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	int error;
	bool retry = false;

recheck:
	error = __mxt_read_reg(data->client, data->T7_address,
				sizeof(data->t7_cfg), &data->t7_cfg);
	if (error)
		return error;

	if (data->t7_cfg.active == 0 || data->t7_cfg.idle == 0) {
		if (!retry) {
			dev_info(dev, "T7 cfg zero, resetting\n");
			mxt_soft_reset(data);
			retry = true;
			goto recheck;
		} else {
		    dev_dbg(dev, "T7 cfg zero after reset, overriding\n");
		    data->t7_cfg.active = 20;
		    data->t7_cfg.idle = 100;
		    return mxt_set_t7_power_cfg(data, MXT_POWER_CFG_RUN);
		}
	} else {
		dev_info(dev, "Initialised power cfg: ACTV %d, IDLE %d\n",
				data->t7_cfg.active, data->t7_cfg.idle);
		return 0;
	}
}

static int mxt_acquire_irq(struct mxt_data *data)
{
	int error;

	enable_irq(data->irq);

	if (data->use_retrigen_workaround) {
		error = mxt_process_messages_until_invalid(data);
		if (error)
			return error;
	}

	return 0;
}

static void mxt_free_input_device(struct mxt_data *data)
{
	if (data->input_dev) {
		input_unregister_device(data->input_dev);
		data->input_dev = NULL;
	}
}

static void mxt_free_object_table(struct mxt_data *data)
{
	//mxt_debug_msg_remove(data);

	kfree(data->raw_info_block);
	data->object_table = NULL;
	data->info = NULL;
	data->raw_info_block = NULL;
	kfree(data->msg_buf);
	data->msg_buf = NULL;

	//mxt_free_input_device(data);

	#ifdef MXT_UPDATE_BY_OBJECT
	kfree(data->object_buf);
	data->object_buf = NULL ;
	#endif

	data->enable_reporting = false;
	data->T5_address = 0;
	data->T5_msg_size = 0;
	data->T6_reportid = 0;
	data->T7_address = 0;
	data->T9_reportid_min = 0;
	data->T9_reportid_max = 0;
	data->T15_reportid_min = 0;
	data->T15_reportid_max = 0;
	data->T18_address = 0;
	data->T19_reportid = 0;
	data->T42_reportid_min = 0;
	data->T42_reportid_max = 0;
	data->T44_address = 0;
	data->T48_reportid = 0;
	data->T63_reportid_min = 0;
	data->T63_reportid_max = 0;
	data->T100_reportid_min = 0;
	data->T100_reportid_max = 0;
	data->max_reportid = 0;
}

static int mxt_parse_object_table(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	int i;
	u8 reportid;
	u16 end_address;

	/* Valid Report IDs start counting from 1 */
	reportid = 1;
	data->mem_size = 0;
	for (i = 0; i < data->info->object_num; i++) {
		struct mxt_object *object = data->object_table + i;
		u8 min_id, max_id;

		le16_to_cpus(&object->start_address);

		if (object->num_report_ids) {
			min_id = reportid;
			reportid += object->num_report_ids *
					mxt_obj_instances(object);
			max_id = reportid - 1;
		} else {
			min_id = 0;
			max_id = 0;
		}

		if(data->object_max_size<object->size_minus_one)
			data->object_max_size=object->size_minus_one ;

		dev_dbg(&data->client->dev,
			"T%u Start:%u Size:%u Instances:%u Report IDs:%u-%u\n",
			object->type, object->start_address,
			mxt_obj_size(object), mxt_obj_instances(object),
			min_id, max_id);

		switch (object->type) {
		case MXT_GEN_MESSAGE_T5:
			if (data->info->family_id == 0x80) {
				/* On mXT224 read and discard unused CRC byte
				 * otherwise DMA reads are misaligned */
				data->T5_msg_size = mxt_obj_size(object);
			} else {
				/* CRC not enabled, so skip last byte */
				data->T5_msg_size = mxt_obj_size(object) - 1;
			}
			data->T5_address = object->start_address;
		case MXT_GEN_COMMAND_T6:
			data->T6_reportid = min_id;
			data->T6_address = object->start_address;
			break;
		case MXT_GEN_POWER_T7:
			data->T7_address = object->start_address;
			break;
		case MXT_TOUCH_MULTI_T9:
			/* Only handle messages from first T9 instance */
			data->T9_reportid_min = min_id;
			data->T9_reportid_max = min_id +
						object->num_report_ids - 1;
			data->num_touchids = object->num_report_ids;
			break;
		case MXT_TOUCH_KEYARRAY_T15:
			data->T15_reportid_min = min_id;
			data->T15_reportid_max = max_id;
			break;
		case MXT_SPT_COMMSCONFIG_T18:
			data->T18_address = object->start_address;
			break;
		case MXT_PROCI_TOUCHSUPPRESSION_T42:
			data->T42_reportid_min = min_id;
			data->T42_reportid_max = max_id;
			break;
		case MXT_SPT_MESSAGECOUNT_T44:
			data->T44_address = object->start_address;
			break;
		case MXT_SPT_GPIOPWM_T19:
			data->T19_reportid = min_id;
			break;
		case MXT_PROCG_NOISESUPPRESSION_T48:
			data->T48_reportid = min_id;
			break;
		case MXT_PROCI_ACTIVE_STYLUS_T63:
			/* Only handle messages from first T63 instance */
			data->T63_reportid_min = min_id;
			data->T63_reportid_max = min_id;
			data->num_stylusids = 1;
			break;
		case MXT_TOUCH_MULTITOUCHSCREEN_T100:
			data->T100_reportid_min = min_id;
			data->T100_reportid_max = max_id;
			/* first two report IDs reserved */
			data->num_touchids = object->num_report_ids - 2;
			break;
		case MXT_SPT_SELFTEST_T25 :
			data->T25_reportid = min_id ;
		   break ;
		case MXT_GESTURE_UNICODE_CTR_T221 :
			data->T221_reportid = min_id ;
			break ;
		case MXT_GESTURE_TAP_T93:
			data->T93_reportid = min_id ;
			break ;
		}

		end_address = object->start_address
			+ mxt_obj_size(object) * mxt_obj_instances(object) - 1;

		if (end_address >= data->mem_size)
			data->mem_size = end_address + 1;
	}

	/* Store maximum reportid */
	data->max_reportid = reportid;

	/* If T44 exists, T5 position has to be directly after */
	if (data->T44_address && (data->T5_address != data->T44_address + 1)) {
		dev_err(&client->dev, "Invalid T44 position\n");
		return -EINVAL;
	}

	data->msg_buf = kcalloc(data->max_reportid,
				data->T5_msg_size, GFP_KERNEL);
	if (!data->msg_buf) {
		dev_err(&client->dev, "Failed to allocate message buffer\n");
		return -ENOMEM;
	}
	dev_err(&client->dev,"object max size [%d]\n",data->object_max_size);
#ifdef MXT_UPDATE_BY_OBJECT
	data->object_buf = kmalloc(data->object_max_size+64,GFP_KERNEL);
	if(!data->object_buf){
		dev_err(&client->dev,"Failed to allocate object buffer\n");
		return -ENOMEM ;
		}
#endif
	return 0;
}

static int mxt_read_info_block(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	int error;
	size_t size;
	void *buf;
	uint8_t num_objects;
	u32 calculated_crc;
	u8 *crc_ptr;

	/* If info block already allocated, free it */
	if (data->raw_info_block != NULL)
		mxt_free_object_table(data);

	/* Read 7-byte ID information block starting at address 0 */
	size = sizeof(struct mxt_info);
	buf = kzalloc(size, GFP_KERNEL);
	if (!buf) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	error = __mxt_read_reg(client, 0, size, buf);
	if (error){
		err_printk("%s:read register error\n",__func__);
		goto err_free_mem;
	}
	/* Resize buffer to give space for rest of info block */
	num_objects = ((struct mxt_info *)buf)->object_num;
	size += (num_objects * sizeof(struct mxt_object))
		+ MXT_INFO_CHECKSUM_SIZE;

	buf = krealloc(buf, size, GFP_KERNEL);
	if (!buf) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		error = -ENOMEM;
		goto err_free_mem;
	}

	/* Read rest of info block */
	error = __mxt_read_reg(client, MXT_OBJECT_START,
			       size - MXT_OBJECT_START,
			       buf + MXT_OBJECT_START);
	if (error){
		err_printk("read rest of info block error\n");
		goto err_free_mem;
	}
	/* Extract & calculate checksum */
	crc_ptr = buf + size - MXT_INFO_CHECKSUM_SIZE;
	data->info_crc = crc_ptr[0] | (crc_ptr[1] << 8) | (crc_ptr[2] << 16);

	calculated_crc = mxt_calculate_crc(buf, 0,
					   size - MXT_INFO_CHECKSUM_SIZE);

	/* CRC mismatch can be caused by data corruption due to I2C comms
	 * issue or else device is not using Object Based Protocol */
	if ((data->info_crc == 0) || (data->info_crc != calculated_crc)) {
		dev_err(&client->dev,
			"Info Block CRC error calculated=0x%06X read=0x%06X\n",
			data->info_crc, calculated_crc);
		return -EIO;
	}

	/* Save pointers in device data structure */
	data->raw_info_block = buf;
	data->info = (struct mxt_info *)buf;
	data->object_table = (struct mxt_object *)(buf + MXT_OBJECT_START);

	dev_err(&client->dev,
		 "Family: %u Variant: %u Firmware V%u.%u.%02X Objects: %u\n",
		 data->info->family_id, data->info->variant_id,
		 data->info->version >> 4, data->info->version & 0xf,
		 data->info->build, data->info->object_num);

	/* Parse object table information */
	error = mxt_parse_object_table(data);
	if (error) {
		dev_err(&client->dev, "Error %d reading object table\n", error);
		mxt_free_object_table(data);
		return error;
	}

	return 0;

err_free_mem:
	kfree(buf);
	return error;
}

static int mxt_read_t9_resolution(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	int error;
	struct t9_range range;
	unsigned char orient;
	struct mxt_object *object;

	object = mxt_get_object(data, MXT_TOUCH_MULTI_T9);
	if (!object)
		return -EINVAL;

	error = __mxt_read_reg(client,
			       object->start_address + MXT_T9_RANGE,
			       sizeof(range), &range);
	if (error)
		return error;

	le16_to_cpus(range.x);
	le16_to_cpus(range.y);

	error =  __mxt_read_reg(client,
				object->start_address + MXT_T9_ORIENT,
				1, &orient);
	if (error)
		return error;

	/* Handle default values */
	if (range.x == 0)
		range.x = 1023;

	if (range.y == 0)
		range.y = 1023;

	if (orient & MXT_T9_ORIENT_SWITCH) {
		data->max_x = range.y;
		data->max_y = range.x;
	} else {
		data->max_x = range.x;
		data->max_y = range.y;
	}

	dev_info(&client->dev,
		 "Touchscreen size X%uY%u\n", data->max_x, data->max_y);

	return 0;
}

static void mxt_regulator_enable(struct mxt_data *data)
{

	printk("%s:reset gpio :%d\n",data->pdata->gpio_reset);
	gpio_set_value(data->pdata->gpio_reset, 0);

	regulator_enable(data->reg_vdd);
	regulator_enable(data->reg_avdd);
	msleep(MXT_REGULATOR_DELAY);
	//completion_init
	INIT_COMPLETION(data->bl_completion);
	gpio_set_value(data->pdata->gpio_reset, 1);
	mxt_wait_for_completion(data, &data->bl_completion, MXT_POWERON_DELAY);
}

static void mxt_regulator_disable(struct mxt_data *data)
{
	//regulator_disable(data->reg_vdd);
	//regulator_disable(data->reg_avdd);
}

static void mxt_probe_regulators(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	int error;

	/* According to maXTouch power sequencing specification, RESET line
	 * must be kept low until some time after regulators come up to
	 * voltage */
	if (!data->pdata->gpio_reset) {
		dev_warn(dev, "Must have reset GPIO to use regulator support\n");
		goto fail;
	}
#if 0
	data->reg_vdd = regulator_get(dev, "vdd18_tou");//vdd,vdd18_tou
	if (IS_ERR(data->reg_vdd)) {
		error = PTR_ERR(data->reg_vdd);
		dev_err(dev, "Error %d getting vdd regulator\n", error);
		goto fail;
	}

	data->reg_avdd = regulator_get(dev, "vcc28_tou");//avdd
	if (IS_ERR(data->reg_vdd)) {
		error = PTR_ERR(data->reg_vdd);
		dev_err(dev, "Error %d getting avdd regulator\n", error);
		goto fail_release;
	}

	data->use_regulator = true;
#endif
	mxt_regulator_enable(data);

	dev_dbg(dev, "Initialised regulators\n");
	return;

fail_release:
	regulator_put(data->reg_vdd);
fail:
	data->reg_vdd = NULL;
	data->reg_avdd = NULL;
	data->use_regulator = false;
}

static int mxt_read_t100_config(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	int error;
	struct mxt_object *object;
	u16 range_x, range_y;
	u8 cfg, tchaux;
	u8 aux;

	object = mxt_get_object(data, MXT_TOUCH_MULTITOUCHSCREEN_T100);
	if (!object)
		return -EINVAL;

	error = __mxt_read_reg(client,
			       object->start_address + MXT_T100_XRANGE,
			       sizeof(range_x), &range_x);
	if (error)
		return error;

	le16_to_cpus(range_x);

	error = __mxt_read_reg(client,
			       object->start_address + MXT_T100_YRANGE,
			       sizeof(range_y), &range_y);
	if (error)
		return error;

	le16_to_cpus(range_y);

	error =  __mxt_read_reg(client,
				object->start_address + MXT_T100_CFG1,
				1, &cfg);
	if (error)
		return error;

	error =  __mxt_read_reg(client,
				object->start_address + MXT_T100_TCHAUX,
				1, &tchaux);
	if (error)
		return error;

	/* Handle default values */
	if (range_x == 0)
		range_x = 1023;

	/* Handle default values */
	if (range_x == 0)
		range_x = 1023;

	if (range_y == 0)
		range_y = 1023;

	if (cfg & MXT_T100_CFG_SWITCHXY) {
		data->max_x = range_y;
		data->max_y = range_x;
	} else {
		data->max_x = range_x;
		data->max_y = range_y;
	}

	/* allocate aux bytes */
	aux = 6;

	if (tchaux & MXT_T100_TCHAUX_VECT)
		data->t100_aux_vect = aux++;

	if (tchaux & MXT_T100_TCHAUX_AMPL)
		data->t100_aux_ampl = aux++;

	if (tchaux & MXT_T100_TCHAUX_AREA)
		data->t100_aux_area = aux++;

	if(!data->pri_max_x){
		data->pri_max_x = data->max_x ;
		data->pri_max_y = data->max_y ;
	}

	dev_err(&client->dev,
		 "T100 Touchscreen size X%uY%u\n", data->max_x, data->max_y);
	dev_err(&client->dev,
			 "T100 Touchscreen pri size X%uY%u\n", data->pri_max_x, data->pri_max_y);

	return 0;
}

static int mxt_input_open(struct input_dev *dev);
static void mxt_input_close(struct input_dev *dev);

static int mxt_initialize_t100_input_device(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	struct input_dev *input_dev;
	int error;

	error = mxt_read_t100_config(data);
	if (error)
		dev_warn(dev, "Failed to initialize T9 resolution\n");

	if(((data->pri_max_x!=data->max_x)||(data->pri_max_y!=data->max_y))){
		info_printk("Need Free input device\n");
		data->pri_max_x = data->max_x ;
		data->pri_max_y = data->max_y ;
		mxt_free_input_device(data);
	} else if(data->input_dev){
		info_printk("No Need to allocte input device\n");
		return 0 ;
	}

	input_dev = input_allocate_device();
	if (!data || !input_dev) {
		dev_err(dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	input_dev->name = "atmel_mxt_ts";
	//input_dev->name = ATMEL_DRIVER_NAME ;
	input_dev->phys = data->phys;
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &data->client->dev;
	input_dev->open = mxt_input_open;
	input_dev->close = mxt_input_close;

	set_bit(EV_ABS, input_dev->evbit);
	input_set_capability(input_dev, EV_KEY, BTN_TOUCH);
    set_bit(KEY_HOME,input_dev->keybit);
	/* For single touch */
	input_set_abs_params(input_dev, ABS_X,
			     0, data->max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_Y,
			     0, data->max_y, 0, 0);

	if (data->t100_aux_ampl)
		input_set_abs_params(input_dev, ABS_PRESSURE,
				     0, 255, 0, 0);

	/* For multi touch */
	error = input_mt_init_slots(input_dev, data->num_touchids,INPUT_MT_DIRECT);
	if (error) {
		dev_err(dev, "Error %d initialising slots\n", error);
		goto err_free_mem;
	}

	//input_set_abs_params(input_dev, ABS_MT_TOOL_TYPE, 0, MT_TOOL_MAX, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X,
			     0, data->max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
			     0, data->max_y, 0, 0);

	if (data->t100_aux_area)
		input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR,
				     0, MXT_MAX_AREA, 0, 0);

	if (data->t100_aux_ampl)
		input_set_abs_params(input_dev, ABS_MT_PRESSURE,
				     0, 255, 0, 0);

	if (data->t100_aux_vect)
		input_set_abs_params(input_dev, ABS_MT_ORIENTATION,
				     0, 255, 0, 0);

	input_set_drvdata(input_dev, data);

	set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
	error = input_register_device(input_dev);
	if (error) {
		dev_err(dev, "Error %d registering input device\n", error);
		goto err_free_mem;
	}

	data->input_dev = input_dev;

	return 0;

err_free_mem:
	input_free_device(input_dev);
	return error;
}

static int mxt_initialize_t9_input_device(struct mxt_data *data);
static int mxt_configure_objects(struct mxt_data *data);

static int mxt_initialize(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	int error;
	bool alt_bootloader_addr = false;
	bool retry = false;

retry_info:
	error = mxt_read_info_block(data);
	if(error)
		return error ;

	if (error) {
retry_bootloader:
		error = mxt_probe_bootloader(data, alt_bootloader_addr);
		if (error) {
			if (alt_bootloader_addr) {
				/* Chip is not in appmode or bootloader mode */
				return error;
			}

			dev_info(&client->dev, "Trying alternate bootloader address\n");
			alt_bootloader_addr = true;
			goto retry_bootloader;
		} else {
			if (retry) {
				dev_err(&client->dev,
						"Could not recover device from "
						"bootloader mode\n");
				/* this is not an error state, we can reflash
				 * from here */
				data->in_bootloader = true;
				return 0;
			}

			/* Attempt to exit bootloader into app mode */
			mxt_send_bootloader_cmd(data, false);
			msleep(MXT_FW_RESET_TIME);
			retry = true;
			goto retry_info;
		}
	}

	error = mxt_check_retrigen(data);
	if (error)
		return error;

	data->data->out.driver_data = data ;
	data->data->out.report_func = mxt_interrupt ;
	data->data->in.hw_register_irq();
    data->data->in.lancher_thread(data->data,1);
	

	error = mxt_configure_objects(data);
	if (error)
		return error;
       error = mxt_acquire_irq(data);
	if (error)
		return error;

	return 0;
}

static void mxt_exp_fn_work(struct work_struct *queue_work)
{
	struct mxt_data * data = container_of(queue_work,struct mxt_data,work);

	info_printk("Enter update work\n");
	data->cfg_name = MXT_CFG_NAME ;
	_mxt_configure_objects(data);
	data->cfg_name = NULL ;
	
	return ;
}

static int _mxt_configure_objects(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	int error;

	/* Check register init values */
	error = mxt_check_reg_init(data);
	if(error==MXT_UPDATE_NONE){
	 info_printk("No Need to update config file\n");
	 goto OUT ;
	}else 
		info_printk("Need to update config file\n");

	mxt_irq_onoff(data,0);
	
	mutex_lock(&g_update_mutex);
	//mxt_read_info_block(data);

	if (data->T9_reportid_min) {
		error = mxt_initialize_t9_input_device(data);
		if (error){			
			mutex_unlock(&g_update_mutex);
			goto OUT;
		}
	} else if (data->T100_reportid_min) {
		error = mxt_initialize_t100_input_device(data);
		if (error){			
			mutex_unlock(&g_update_mutex);
			goto OUT;
		}
	} else {
		dev_warn(&client->dev, "No touch object detected\n");
	}	
	mutex_unlock(&g_update_mutex);
	mxt_irq_onoff(data,1);
	
OUT:	
	data->enable_reporting = true;
	return 0;
}


static int mxt_configure_objects(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	int error;

	error = mxt_debug_msg_init(data);
	if (error)
		return error;

	error = mxt_init_t7_power_cfg(data);
	if (error) {
		dev_err(&client->dev, "Failed to initialize power cfg\n");
		return error;
	}

/*update config file in work */
#if 0 
	/* Check register init values */
	error = mxt_check_reg_init(data);
	if (error) {
		dev_err(&client->dev, "Error %d initialising configuration\n",
			error);
		return error;
	}
#endif
	if (data->T9_reportid_min) {
		error = mxt_initialize_t9_input_device(data);
		if (error)
			return error;
	} else if (data->T100_reportid_min) {
		error = mxt_initialize_t100_input_device(data);
		if (error)
			return error;
	} else {
		dev_warn(&client->dev, "No touch object detected\n");
	}

	data->enable_reporting = true;
	return 0;
}

/* Firmware Version is returned as Major.Minor.Build */
static ssize_t mxt_fw_version_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "%u.%u.%02X\n",
			 data->info->version >> 4, data->info->version & 0xf,
			 data->info->build);
}

/* Hardware Version is returned as FamilyID.VariantID */
static ssize_t mxt_hw_version_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "%u.%u\n",
			data->info->family_id, data->info->variant_id);
}

static ssize_t mxt_show_instance(char *buf, int count,
				 struct mxt_object *object, int instance,
				 const u8 *val)
{
	int i;

	if (mxt_obj_instances(object) > 1)
		count += scnprintf(buf + count, PAGE_SIZE - count,
				   "Instance %u\n", instance);

	for (i = 0; i < mxt_obj_size(object); i++)
		count += scnprintf(buf + count, PAGE_SIZE - count,
				"\t[%2u]: %02x (%d)\n", i, val[i], val[i]);
	count += scnprintf(buf + count, PAGE_SIZE - count, "\n");

	return count;
}

static ssize_t mxt_object_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	struct mxt_object *object;
	int count = 0;
	int i, j;
	int error;
	u8 *obuf;

	/* Pre-allocate buffer large enough to hold max sized object. */
	obuf = kmalloc(256, GFP_KERNEL);
	if (!obuf)
		return -ENOMEM;

	error = 0;
	for (i = 0; i < data->info->object_num; i++) {
		object = data->object_table + i;

		if (!mxt_object_readable(object->type))
			continue;

		count += scnprintf(buf + count, PAGE_SIZE - count,
				"T%u:\n", object->type);

		for (j = 0; j < mxt_obj_instances(object); j++) {
			u16 size = mxt_obj_size(object);
			u16 addr = object->start_address + j * size;

			error = __mxt_read_reg(data->client, addr, size, obuf);
			if (error)
				goto done;

			count = mxt_show_instance(buf, count, object, j, obuf);
		}
	}

done:
	kfree(obuf);
	return error ?: count;
}

static int mxt_check_firmware_format(struct device *dev,
				     const struct firmware *fw)
{
	unsigned int pos = 0;
	char c;

	while (pos < fw->size) {
		c = *(fw->data + pos);

		if (c < '0' || (c > '9' && c < 'A') || c > 'F')
			return 0;

		pos++;
	}

	/* To convert file try
	 * xxd -r -p mXTXXX__APP_VX-X-XX.enc > maxtouch.fw */
	dev_err(dev, "Aborting: firmware file must be in binary format\n");

	return -1;
}

static int mxt_load_fw(struct device *dev)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	const struct firmware *fw = NULL;
	unsigned int frame_size;
	unsigned int pos = 0;
	unsigned int retry = 0;
	unsigned int frame = 0;
	int ret;

	ret = request_firmware(&fw, data->fw_name, dev);
	if (ret) {
		dev_err(dev, "Unable to open firmware %s\n", data->fw_name);
		return ret;
	}

	/* Check for incorrect enc file */
	ret = mxt_check_firmware_format(dev, fw);
	if (ret)
		goto release_firmware;

	if (data->suspended) {
		if (data->use_regulator)
			mxt_regulator_enable(data);

		enable_irq(data->irq);
		data->suspended = false;
	}

	if (!data->in_bootloader) {
		/* Change to the bootloader mode */
		data->in_bootloader = true;

		ret = mxt_t6_command(data, MXT_COMMAND_RESET,
				     MXT_BOOT_VALUE, false);
		if (ret)
			goto release_firmware;

		msleep(MXT_RESET_TIME);

		/* At this stage, do not need to scan since we know
		 * family ID */
		ret = mxt_lookup_bootloader_address(data, 0);
		if (ret)
			goto release_firmware;
	} else {
		enable_irq(data->irq);
	}

	mxt_free_object_table(data);
	INIT_COMPLETION(data->bl_completion);

	ret = mxt_check_bootloader(data, MXT_WAITING_BOOTLOAD_CMD, false);
	if (ret) {
		/* Bootloader may still be unlocked from previous update
		 * attempt */
		ret = mxt_check_bootloader(data, MXT_WAITING_FRAME_DATA, false);
		if (ret)
			goto disable__irq;
	} else {
		dev_info(dev, "Unlocking bootloader\n");

		/* Unlock bootloader */
		ret = mxt_send_bootloader_cmd(data, true);
		if (ret)
			goto disable__irq;
	}

	while (pos < fw->size) {
		ret = mxt_check_bootloader(data, MXT_WAITING_FRAME_DATA, true);
		if (ret)
			goto disable__irq;

		frame_size = ((*(fw->data + pos) << 8) | *(fw->data + pos + 1));

		/* Take account of CRC bytes */
		frame_size += 2;

		/* Write one frame to device */
		ret = mxt_bootloader_write(data, fw->data + pos, frame_size);
		if (ret)
			goto disable__irq;

		ret = mxt_check_bootloader(data, MXT_FRAME_CRC_PASS, true);
		if (ret) {
			retry++;

			/* Back off by 20ms per retry */
			msleep(retry * 20);

			if (retry > 20) {
				dev_err(dev, "Retry count exceeded\n");
				goto disable__irq;
			}
		} else {
			retry = 0;
			pos += frame_size;
			frame++;
		}

		if (frame % 50 == 0)
			dev_info(dev, "Sent %d frames, %d/%zd bytes\n",
				 frame, pos, fw->size);
	}

	/* Wait for flash. */
	ret = mxt_wait_for_completion(data, &data->bl_completion,
				MXT_FW_RESET_TIME);
	if (ret)
		goto disable__irq;

	dev_info(dev, "Sent %d frames, %zd bytes\n", frame, pos);

	/* Wait for device to reset. Some bootloader versions do not assert
	 * the CHG line after bootloading has finished, so ignore error */
	mxt_wait_for_completion(data, &data->bl_completion,
				MXT_FW_RESET_TIME);

	data->in_bootloader = false;

disable__irq:
	disable_irq(data->irq);
release_firmware:
	release_firmware(fw);
	return ret;
}

static int mxt_update_file_name(struct device *dev, char **file_name,
				const char *buf, size_t count)
{
	char *file_name_tmp;

	/* Simple sanity check */
	if (count > 64) {
		dev_warn(dev, "File name too long\n");
		return -EINVAL;
	}

	file_name_tmp = krealloc(*file_name, count + 1, GFP_KERNEL);
	if (!file_name_tmp) {
		dev_warn(dev, "no memory\n");
		return -ENOMEM;
	}

	*file_name = file_name_tmp;
	memcpy(*file_name, buf, count);

	/* Echo into the sysfs entry may append newline at the end of buf */
	if (buf[count - 1] == '\n')
		(*file_name)[count - 1] = '\0';
	else
		(*file_name)[count] = '\0';

	return 0;
}

static ssize_t mxt_update_fw_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int error;

	error = mxt_update_file_name(dev, &data->fw_name, buf, count);
	if (error)
		return error;

	error = mxt_load_fw(dev);
	if (error) {
		dev_err(dev, "The firmware update failed(%d)\n", error);
		count = error;
	} else {
		dev_info(dev, "The firmware update succeeded\n");

		data->suspended = false;

		error = mxt_initialize(data);
		if (error)
			return error;
	}

	return count;
}

static ssize_t mxt_update_cfg_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int ret;

	if (data->in_bootloader) {
		dev_err(dev, "Not in appmode\n");
		return -EINVAL;
	}
		
	ret = mxt_update_file_name(dev, &data->cfg_name, buf, count);
	if (ret)
		return ret;

	data->enable_reporting = false;
    data->force_update = true ;
	if (data->suspended) {
		if (data->use_regulator)
			mxt_regulator_enable(data);
		else
			mxt_set_t7_power_cfg(data, MXT_POWER_CFG_RUN);

		mxt_acquire_irq(data);

		data->suspended = false;
	}

	ret = _mxt_configure_objects(data);
	if (ret)
		goto out;

	info_printk("update cfg done\n");
	
	ret = count;
out:
	data->force_update = false ;
	return ret;
}


static ssize_t mxt_debug_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	char c;

	c = data->debug_enabled ? '1' : '0';
	return scnprintf(buf, PAGE_SIZE, "%c\n", c);
}

static ssize_t mxt_debug_notify_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "0\n");
}

static ssize_t mxt_dump_gpio_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "SDA:%d SCL:%d IRQ:%d RST:%d IR:%d\n",
			(data->data->in.hw_get_gpio_value(SDA_GPIO_INDX)),
			(data->data->in.hw_get_gpio_value(SCL_GPIO_INDX)),
			(data->data->in.hw_get_gpio_value(IRQ_GPIO_INDX)),
			(data->data->in.hw_get_gpio_value(RST_GPIO_INDX)),
                         (data->data->in.hw_get_gpio_value(IR_GPIO_INDX)));
}



#define T6_OFFSET 0x5
#define T6_APPID  0xF9
#define T37_OFFSET 0x05 
static ssize_t mxt_mode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	char *pstr ;
	if(data->mode==MXT_HALL_COVER)
		pstr = "MXT_HALL_COVER" ;
	else if(data->mode==MXT_HALL_UNCOVER)
		pstr = "MXT_HALL_UNCOVER" ;
	else if(data->mode==MXT_GESTURE_MODE)
		pstr = "MXT_GESTURE_MODE";
	else if(data->mode=MXT_NORMAL_MODE)
		pstr = "MXT_NORMAR_MODE";
	else 
		pstr = "MXT MODE ERROR";
	
	return snprintf(buf,PAGE_SIZE,"mode:%s\n",pstr) ;
}

static ssize_t mxt_appid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);	
	struct mxt_object * object=NULL ;
	char config_id[20]={0} ;
	int count = 0 ;
	int retval = -1 ;
	unsigned char value = T6_APPID ;
	unsigned char t37_value[2] ;
	
	object = mxt_get_object(data,MXT_GEN_COMMAND_T6);
	if(!object){
		info_printk("get object T6 error \n");
		return -EIO ;
	}
	
	retval = __mxt_write_reg(data->client,object->start_address+T6_OFFSET,sizeof(value),&value);
	if(retval){
		info_printk("write T6 error \n");
		return -EIO ;
	}
	while(1){

		object = mxt_get_object(data,MXT_DEBUG_DIAGNOSTIC_T37);
		if(!object){
			info_printk("get T37 error \n");
			return -EIO ;
		}
		retval = __mxt_read_reg(data->client,object->start_address,sizeof(t37_value),t37_value);
		if((t37_value[0]!=0xF9)||(t37_value[1]!=0)||retval){
			info_printk("value %x,retval %d\n",t37_value[0],t37_value[1]);
			msleep(100);
			continue ;
		}
		retval = __mxt_read_reg(data->client,object->start_address+T37_OFFSET,10,config_id);
		config_id[10] = 0 ;
		break ;

	}
	
	count += snprintf(buf+count,PAGE_SIZE,"%s:",config_id);

	object = mxt_get_object(data,MXT_SPT_USERDATA_T38);
	if(!object){
		return snprintf(buf,PAGE_SIZE,"read appid error\n");
		
	}
	
	retval = __mxt_read_reg(data->client,object->start_address,MXT_CONFIG_ID_SIZE,config_id);
    if(retval<0){
		return snprintf(buf,PAGE_SIZE,"read appid error\n");
		
    }
	config_id[4] = 0 ;
	
	count += snprintf(buf+count,PAGE_SIZE,"%s:",config_id);
	count += snprintf(buf+count,PAGE_SIZE,"atmel:");
	count += snprintf(buf+count,PAGE_SIZE,"1.0.0,,\n");
	return count ;
		
}


static ssize_t mxt_testmode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);	
	struct mxt_object * object=NULL ;
	unsigned char testmode = 0;
	int retval = -1 ;
	
	wait_for_completion_interruptible_timeout(&data->t25_completion,5*HZ);

	object = mxt_get_object(data,MXT_SPT_SELFTEST_T25);
	if(!object){
		return -EINVAL ;
	}
	
	retval = __mxt_write_reg(data->client,object->start_address,sizeof(testmode),&testmode);
	if(retval!=0){
		return -EINVAL ;
	}
	
	return snprintf(buf,PAGE_SIZE,"%d",data->T25_result);
}

static void mxt_reset_device(struct mxt_data* data,int value)
{
	
	if(value==1){/*soft reset */
	  mxt_soft_reset(data);
	  msleep(200);
	}else if(value==2){/*hardware reset */
	data->data->in.hw_set_gpio_value(RST_GPIO_INDX,0);
	msleep(20);
	data->data->in.hw_set_gpio_value(RST_GPIO_INDX,1);
	msleep(100);			
	}else 
		printk(KERN_ERR"%s:reset value error[%d]\n",__func__,value);

	printk(KERN_ERR"%s:reset value success\n",__func__);

	
}

static ssize_t mxt_hall_mode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	
	int value = 0 ;
	if(sscanf(buf, "%u", &value)!=1){
		return -EIO ;
	}
	
	mxt_hall_notity_func(&data->notify,(!!value)?HALL_COVER:HALL_UNCOVER,NULL);
	return count ;
}


static ssize_t mxt_reset_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	struct mxt_object *object = NULL ;
	int value = 0 ;
	if(sscanf(buf, "%u", &value)!=1){
		return -EIO ;
	}
	mxt_reset_device(data,value);
	
	return count ;
}


static ssize_t mxt_testmode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	struct mxt_object *object = NULL ;
	int i;
	unsigned char testmode[2]  ;
	int retval = -1 ;
	
	if(sscanf(buf, "%u", &i)!=1&&(i!=1)){
		return -EINVAL;
	}
	object = mxt_get_object(data,MXT_SPT_SELFTEST_T25);
	if(!object){
		return -EINVAL ;
	}
	
	testmode[0] = MXT_T25_ENABLE ;
	testmode[1] = MXT_T25_TEST_ALL;
	retval = __mxt_write_reg(data->client,object->start_address,sizeof(testmode),testmode);
	if(retval!=0){
		return -EINVAL ;
	}
	return count ;
}


static ssize_t mxt_debug_v2_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int i;

	if (sscanf(buf, "%u", &i) == 1 && i < 2) {
		if (i == 1)
			mxt_debug_msg_enable(data);
		else
			mxt_debug_msg_disable(data);

		return count;
	} else {
		dev_dbg(dev, "debug_enabled write error\n");
		return -EINVAL;
	}
}

static ssize_t mxt_touch_debug_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int i;

	if (sscanf(buf, "%u", &i) == 1 && i < 2) {
		if (i == 1)
			data->touch_debug = true ;
		else
			data->touch_debug = false ;

		return count;
	} else {
		dev_dbg(dev, "debug_enabled write error\n");
		return -EINVAL;
	}
}

static ssize_t mxt_debug_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int i;

	if (sscanf(buf, "%u", &i) == 1 && i < 2) {
		data->debug_enabled = (i == 1);

		dev_dbg(dev, "%s\n", i ? "debug enabled" : "debug disabled");
		return count;
	} else {
		dev_dbg(dev, "debug_enabled write error\n");
		return -EINVAL;
	}
}

static int mxt_check_mem_access_params(struct mxt_data *data, loff_t off,
				       size_t *count)
{
	if (off >= data->mem_size)
		return -EIO;

	if (off + *count > data->mem_size)
		*count = data->mem_size - off;

	if (*count > MXT_MAX_BLOCK_WRITE)
		*count = MXT_MAX_BLOCK_WRITE;

	return 0;
}

static ssize_t mxt_mem_access_read(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr, char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct mxt_data *data = dev_get_drvdata(dev);
	int ret = 0;

	ret = mxt_check_mem_access_params(data, off, &count);
	if (ret < 0)
		return ret;

	if (count > 0)
		ret = __mxt_read_reg(data->client, off, count, buf);

	return ret == 0 ? count : ret;
}

static ssize_t mxt_mem_access_write(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr, char *buf, loff_t off,
	size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct mxt_data *data = dev_get_drvdata(dev);
	int ret = 0;

	ret = mxt_check_mem_access_params(data, off, &count);
	if (ret < 0)
		return ret;

	if (count > 0)
		ret = __mxt_write_reg(data->client, off, count, buf);

	return ret == 0 ? count : 0;
}

static DEVICE_ATTR(fw_version, S_IRUGO, mxt_fw_version_show, NULL);
static DEVICE_ATTR(hw_version, S_IRUGO, mxt_hw_version_show, NULL);
static DEVICE_ATTR(object, S_IRUGO, mxt_object_show, NULL);
static DEVICE_ATTR(update_fw, S_IWUSR, NULL, mxt_update_fw_store);
static DEVICE_ATTR(update_cfg, S_IWUSR, NULL, mxt_update_cfg_store);
static DEVICE_ATTR(debug_v2_enable, S_IWUSR | S_IRUSR, NULL, mxt_debug_v2_enable_store);
static DEVICE_ATTR(touch_debug_enable, S_IWUSR | S_IRUSR, NULL, mxt_touch_debug_enable_store);
static DEVICE_ATTR(debug_notify, S_IRUGO, mxt_debug_notify_show, NULL);
static DEVICE_ATTR(dump_gpio, S_IRUGO, mxt_dump_gpio_show, NULL);
static DEVICE_ATTR(appid, S_IRUGO, mxt_appid_show, NULL);
static DEVICE_ATTR(mode, S_IRUGO, mxt_mode_show, NULL);
static DEVICE_ATTR(testmode, S_IRUGO|S_IWGRP|S_IWUSR, mxt_testmode_show, mxt_testmode_store);
static DEVICE_ATTR(hall_mode, S_IWGRP|S_IWUSR, NULL, mxt_hall_mode_store);
static DEVICE_ATTR(reset, S_IWGRP|S_IWUSR,NULL, mxt_reset_store);

#ifdef M75_TP_GESTURE_SUPPORT
static DEVICE_ATTR(gesture_data, S_IRUGO, mxt_gesture_value_read, NULL);
static DEVICE_ATTR(gesture_test, S_IRUGO, mxt_gesture_test, NULL);
static DEVICE_ATTR(gesture_control, S_IRUGO|S_IWGRP|S_IWUSR, mxt_gesture_control_read, mxt_gesture_control_write);
static DEVICE_ATTR(gesture_hex, S_IRUGO|S_IWGRP|S_IWUSR, mxt_gesture_hex_read, mxt_gesture_hex_write);
#endif

static DEVICE_ATTR(debug_enable, S_IWUSR | S_IRUSR, mxt_debug_enable_show,
		   mxt_debug_enable_store);


static struct attribute *mxt_attrs[] = {
	&dev_attr_fw_version.attr,
	&dev_attr_hw_version.attr,
	&dev_attr_object.attr,
	&dev_attr_update_fw.attr,
	&dev_attr_update_cfg.attr,
	&dev_attr_debug_enable.attr,
	&dev_attr_debug_v2_enable.attr,
	&dev_attr_touch_debug_enable.attr,
	&dev_attr_debug_notify.attr,
	&dev_attr_dump_gpio.attr,
	&dev_attr_appid.attr,
	&dev_attr_reset.attr,
	&dev_attr_testmode.attr,
	&dev_attr_hall_mode.attr,
	&dev_attr_mode.attr,
#ifdef M75_TP_GESTURE_SUPPORT
	&dev_attr_gesture_data.attr,
	&dev_attr_gesture_control.attr,
	&dev_attr_gesture_test.attr,
	&dev_attr_gesture_hex.attr,
#endif
	NULL
};

static const struct attribute_group mxt_attr_group = {
	.attrs = mxt_attrs,
};

#ifdef  __UPDATE_CFG_FS__
static int mxt_read_file (
    char *pName,
    const u8 **ppBufPtr,
    int offset,
    int padSzBuf
    )
{
    int iRet = -1;
    struct file *fd;
    int file_len;
    int read_len;
    void *pBuf;
    const struct cred *cred = get_current_cred();
    
    if (!ppBufPtr ) {
        info_printk("invalid ppBufptr!\n");
        return -1;
    }
    *ppBufPtr = NULL;
    fd = filp_open(pName, O_RDONLY, 0);
    if (!fd || IS_ERR(fd) || !fd->f_op || !fd->f_op->read) {
        info_printk("failed to open or read !(0x%p, %d, %d)\n",fd, cred->fsuid, cred->fsgid);
		return -1 ;
    }

    file_len = fd->f_path.dentry->d_inode->i_size;
    pBuf = vmalloc((file_len + 8 + 3) & ~0x3UL);
    if (!pBuf) {
        info_printk("failed to vmalloc(%d)\n", (int)((file_len + 3) & ~0x3UL));
        goto read_file_done;
    }

    do {
        if (fd->f_pos != offset) {
            if (fd->f_op->llseek) {
                if (fd->f_op->llseek(fd, offset, 0) != offset) {
                    info_printk("failed to seek!!\n");
                    goto read_file_done;
                }
            }
            else {
                fd->f_pos = offset;
            }
        }

        read_len = fd->f_op->read(fd, pBuf + padSzBuf, file_len, &fd->f_pos);
        if (read_len != file_len) {
            info_printk("read abnormal: read_len(%d), file_len(%d)\n", read_len, file_len);
        }
    } while (false);

    iRet = 0;
    *ppBufPtr = pBuf;

read_file_done:
    if (iRet) {
        if (pBuf) {
            vfree(pBuf);
        }
    }

    filp_close(fd, NULL);

    return (iRet) ? iRet : read_len;
}

 int mxt_patch_get (
    char *pPatchName,
    struct firmware **ppPatch,
    int padSzBuf)
{
    int iRet = -1;
    struct firmware *pfw;
    uid_t orig_uid;
    gid_t orig_gid;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29))
    //struct cred *cred = get_task_cred(current);
    struct cred *cred = (struct cred *)get_current_cred();
#endif

    mm_segment_t orig_fs = get_fs();

    if (*ppPatch) {
        info_printk("f/w patch already exists \n");
        if ((*ppPatch)->data) {
            vfree((*ppPatch)->data);
        }
        kfree(*ppPatch);
        *ppPatch = NULL;
    }

    if (!strlen(pPatchName)) {
        info_printk("empty f/w name\n");
        return -1;
    }

    pfw = kzalloc(sizeof(struct firmware), /*GFP_KERNEL*/GFP_ATOMIC);
    if (!pfw) {
        info_printk("kzalloc(%d) fail\n", sizeof(struct firmware));
        return -2;
    }

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29))
    orig_uid = cred->fsuid;
    orig_gid = cred->fsgid;
    cred->fsuid = cred->fsgid = 0;
#else
    orig_uid = current->fsuid;
    orig_gid = current->fsgid;
    current->fsuid = current->fsgid = 0;
#endif

    set_fs(get_ds());

    /* load patch file from fs */
    iRet = mxt_read_file(pPatchName, &pfw->data, 0, padSzBuf);
    set_fs(orig_fs);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29))
    cred->fsuid = orig_uid;
    cred->fsgid = orig_gid;
#else
    current->fsuid = orig_uid;
    current->fsgid = orig_gid;
#endif

    if (iRet > 0) {
        pfw->size = iRet;
        *ppPatch = pfw;
        info_printk("load (%s) to addr(0x%p) success\n", pPatchName, pfw->data);
        return 0;
    }
    else {
        kfree(pfw);
        *ppPatch = NULL;
        info_printk("load file (%s) fail, iRet(%d) \n", pPatchName, iRet);
        return -1;
    }
}

#endif


static void mxt_start(struct mxt_data *data)
{
	if (!data->suspended || data->in_bootloader)
		return;
#if 1
	/*if (data->use_regulator) {
		mxt_regulator_enable(data);
	} else */{
		/* Discard any messages still in message buffer from before
		 * chip went to sleep */
		mxt_process_messages_until_invalid(data);

		mxt_set_t7_power_cfg(data, MXT_POWER_CFG_RUN);

		/* Recalibrate since chip has been in deep sleep */
		mxt_t6_command(data, MXT_COMMAND_CALIBRATE, 1, false);
	}
#endif
	mxt_reset_slots(data);
	mxt_acquire_irq(data);
	data->enable_reporting = true;
	data->suspended = false;
}

static void mxt_stop(struct mxt_data *data)
{
	if (data->suspended || data->in_bootloader)
		return;

	data->enable_reporting = false;
	disable_irq(data->irq);
#if 1
	/*if (data->use_regulator)
		mxt_regulator_disable(data);
	else*/
		mxt_set_t7_power_cfg(data, MXT_POWER_CFG_DEEPSLEEP);
#endif
	
	mxt_reset_slots(data);
	data->suspended = true;
}

static int mxt_input_open(struct input_dev *dev)
{
	struct mxt_data *data = input_get_drvdata(dev);

	info_printk(" input open\n");
	mxt_start(data);

	return 0;
}

static void mxt_input_close(struct input_dev *dev)
{
	struct mxt_data *data = input_get_drvdata(dev);
	info_printk("input close\n");
	mxt_stop(data);
}

static int mxt_handle_pdata(struct mxt_data *data)
{
	data->pdata = dev_get_platdata(&data->client->dev);

	/* Use provided platform data if present */
	if (data->pdata) {
		if (data->pdata->cfg_name)
			mxt_update_file_name(&data->client->dev,
					     &data->cfg_name,
					     data->pdata->cfg_name,
					     strlen(data->pdata->cfg_name));

		return 0;
	}

	data->pdata = kzalloc(sizeof(*data->pdata), GFP_KERNEL);
	if (!data->pdata) {
		dev_err(&data->client->dev, "Failed to allocate pdata\n");
		return -ENOMEM;
	}

	/* Set default parameters */
	data->pdata->irqflags = IRQF_TRIGGER_LOW;

	return 0;
}

static int mxt_initialize_t9_input_device(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	const struct mxt_platform_data *pdata = data->pdata;
	struct input_dev *input_dev;
	int error;
	unsigned int num_mt_slots;
	int i;

	error = mxt_read_t9_resolution(data);
	if (error)
		dev_warn(dev, "Failed to initialize T9 resolution\n");

	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	input_dev->name = ATMEL_DRIVER_NAME;
	input_dev->phys = data->phys;
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = dev;
	input_dev->open = mxt_input_open;
	input_dev->close = mxt_input_close;

	__set_bit(EV_ABS, input_dev->evbit);
	input_set_capability(input_dev, EV_KEY, BTN_TOUCH);

	if (pdata->t19_num_keys) {
		__set_bit(INPUT_PROP_BUTTONPAD, input_dev->propbit);

		for (i = 0; i < pdata->t19_num_keys; i++)
			if (pdata->t19_keymap[i] != KEY_RESERVED)
				input_set_capability(input_dev, EV_KEY,
						     pdata->t19_keymap[i]);

		__set_bit(BTN_TOOL_FINGER, input_dev->keybit);
		__set_bit(BTN_TOOL_DOUBLETAP, input_dev->keybit);
		__set_bit(BTN_TOOL_TRIPLETAP, input_dev->keybit);
		__set_bit(BTN_TOOL_QUADTAP, input_dev->keybit);

		input_abs_set_res(input_dev, ABS_X, MXT_PIXELS_PER_MM);
		input_abs_set_res(input_dev, ABS_Y, MXT_PIXELS_PER_MM);
		input_abs_set_res(input_dev, ABS_MT_POSITION_X,
				  MXT_PIXELS_PER_MM);
		input_abs_set_res(input_dev, ABS_MT_POSITION_Y,
				  MXT_PIXELS_PER_MM);

		input_dev->name = "Atmel maXTouch Touchpad";
	}

	/* For single touch */
	input_set_abs_params(input_dev, ABS_X,
			     0, data->max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_Y,
			     0, data->max_y, 0, 0);
	input_set_abs_params(input_dev, ABS_PRESSURE,
			     0, 255, 0, 0);

	/* For multi touch */
	num_mt_slots = data->num_touchids + data->num_stylusids;
	error = input_mt_init_slots(input_dev, num_mt_slots,INPUT_MT_DIRECT);
	if (error) {
		dev_err(dev, "Error %d initialising slots\n", error);
		goto err_free_mem;
	}

	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR,
			     0, MXT_MAX_AREA, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X,
			     0, data->max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
			     0, data->max_y, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_PRESSURE,
			     0, 255, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_ORIENTATION,
			     0, 255, 0, 0);

	/* For T63 active stylus */
	if (data->T63_reportid_min) {
		input_set_capability(input_dev, EV_KEY, BTN_STYLUS);
		input_set_capability(input_dev, EV_KEY, BTN_STYLUS2);
		input_set_abs_params(input_dev, ABS_MT_TOOL_TYPE,
			0, MT_TOOL_MAX, 0, 0);
	}

	/* For T15 key array */
	if (data->T15_reportid_min) {
		data->t15_keystatus = 0;

		for (i = 0; i < data->pdata->t15_num_keys; i++)
			input_set_capability(input_dev, EV_KEY,
					     data->pdata->t15_keymap[i]);
	}

	input_set_drvdata(input_dev, data);

	error = input_register_device(input_dev);
	if (error) {
		dev_err(dev, "Error %d registering input device\n", error);
		goto err_free_mem;
	}

	data->input_dev = input_dev;

	return 0;

err_free_mem:
	input_free_device(input_dev);
	return error;
}

static int  mxt_probe(struct i2c_client *client,
			       const struct i2c_device_id *id)
{
	struct mxt_data *data;
	int error;
	int i = 0 ;
	struct tp_driver_data * tp_data = meizu_get_tp_hw_data();

	info_printk("enter atmel probe function\n");

	if(!tp_data || tp_data->probed){
		err_printk("synaptics probed,return\n");
		return -ENODEV ;
	}
	
	data = kzalloc(sizeof(struct mxt_data), GFP_KERNEL);
	if (!data) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}
	g_data = data ;
	snprintf(data->phys, sizeof(data->phys), "i2c-%u-%04x/input0",
		 client->adapter->nr, client->addr);

	data->client = client;
	//client->irq = gpio_to_irq(client->irq);
	//data->irq = client->irq;
	i2c_set_clientdata(client, data);
    
	error = mxt_handle_pdata(data);
	if (error)
		goto err_free_mem;

	init_completion(&data->bl_completion);
	init_completion(&data->reset_completion);
	init_completion(&data->crc_completion);	
	init_completion(&data->t25_completion);
	mutex_init(&data->debug_msg_lock);
	mutex_init(&g_read_mutex);
	mutex_init(&g_update_mutex);
    mutex_init(&data->hall_lock);
    tp_data->in.hw_power_onoff(1);
	data->data = tp_data ;
	msleep(150);
	
	data->pri_max_x = 0 ;
	data->pri_max_y = 0 ;
	data->mode = MXT_NORMAL_MODE ;
	
	error = mxt_initialize(data);
	if (error)
		goto err_free_irq;
    tp_data->probed =1 ;
	
	error = sysfs_create_group(&client->dev.kobj, &mxt_attr_group);
	if (error) {
		dev_err(&client->dev, "Failure %d creating sysfs group\n",
			error);
		goto err_free_object;
	}

#ifdef M75_TP_GESTURE_SUPPORT
	data->gesture_enable = false ;
	data->gesture_value  = 0 ;
#endif
	
	sysfs_bin_attr_init(&data->mem_access_attr);
	data->mem_access_attr.attr.name = "mem_access";
	data->mem_access_attr.attr.mode = S_IRUGO | S_IWUSR;
	data->mem_access_attr.read = mxt_mem_access_read;
	data->mem_access_attr.write = mxt_mem_access_write;
	data->mem_access_attr.size = data->mem_size;

	if (sysfs_create_bin_file(&client->dev.kobj,
				  &data->mem_access_attr) < 0) {
		dev_err(&client->dev, "Failed to create %s\n",
			data->mem_access_attr.attr.name);
		goto err_remove_sysfs_group;
	}

	data->devices = sysfs_get_devices_parent();

	sysfs_create_link(data->devices,&client->dev.kobj,ATMEL_DRIVER_NAME);	
	sysfs_create_link(data->devices,&client->dev.kobj,MEIZU_DRIVER_NAME);
#ifdef MXT_HALL_SUPPORT
	mxt_register_hall_notify(data);
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	data->early_suspend.level = 0 ;
	data->early_suspend.suspend = mxt_early_suspend;
	data->early_suspend.resume = mxt_early_resume;
	register_early_suspend(&data->early_suspend);
#endif

#if 1
	data->workqueue = create_singlethread_workqueue("mxt_exp_workqueue");	
	INIT_DELAYED_WORK(&data->work, mxt_exp_fn_work);
	queue_delayed_work(data->workqueue,&data->work,10*HZ);
#endif

	return 0;

err_remove_sysfs_group:
	sysfs_remove_group(&client->dev.kobj, &mxt_attr_group);
err_free_object:
	mxt_free_object_table(data);
err_free_irq:
	//free_irq(data->irq, data);
err_free_pdata:
	if (!dev_get_platdata(&data->client->dev))
		kfree(data->pdata);
err_free_mem:
	//tp_data->in.hw_power_onoff(0);
	tp_data->probed = 0 ;
	kfree(data);
	return error;
}

static int  mxt_remove(struct i2c_client *client)
{
	struct mxt_data *data = i2c_get_clientdata(client);

	if (data->mem_access_attr.attr.name)
		sysfs_remove_bin_file(&client->dev.kobj,
				      &data->mem_access_attr);

	sysfs_remove_group(&client->dev.kobj, &mxt_attr_group);
	free_irq(data->irq, data);
	regulator_put(data->reg_avdd);
	regulator_put(data->reg_vdd);
	mxt_free_object_table(data);
	if (!dev_get_platdata(&data->client->dev))
		kfree(data->pdata);
	kfree(data);

	return 0;
}


#ifdef CONFIG_PM_SLEEP
static int mxt_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mxt_data *data = i2c_get_clientdata(client);
	struct input_dev *input_dev = data->input_dev;
	info_printk(" enter suspend\n");
	mutex_lock(&input_dev->mutex);
	if (input_dev->users)
		mxt_stop(data);
	mutex_unlock(&input_dev->mutex);

	return 0;
}

static int mxt_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mxt_data *data = i2c_get_clientdata(client);
	struct input_dev *input_dev = data->input_dev;

	mutex_lock(&input_dev->mutex);

	if (input_dev->users)
		mxt_start(data);

	mutex_unlock(&input_dev->mutex);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(mxt_pm_ops, mxt_suspend, mxt_resume);

static void mxt_shutdown(struct i2c_client *client)
{
	struct mxt_data *data = i2c_get_clientdata(client);

	disable_irq(data->irq);
}




static const struct i2c_device_id mxt_id[] = {
	{ATMEL_DRIVER_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mxt_id);

static struct i2c_board_info  atmel_board =
{ 
  I2C_BOARD_INFO(ATMEL_DRIVER_NAME, (0x4a))
};


static struct i2c_driver mxt_driver = {
	.driver = {
		.name	= ATMEL_DRIVER_NAME,
		.owner	= THIS_MODULE,
#ifndef CONFIG_HAS_EARLYSUSPEND
		.pm	= &mxt_pm_ops,
#endif
	},
	.probe		= mxt_probe,
	.remove		= mxt_remove,
	.shutdown	= mxt_shutdown,
	.id_table	= mxt_id,
};

static int __init mxt_init(void)
{
#ifdef ATMEL_TP_SUPPORT
	info_printk("mxt_init\n");
	i2c_register_board_info(2, &atmel_board, 1);
	return i2c_add_driver(&mxt_driver);
#else 	
	info_printk("atmel  touchpanel not support\n");
	return 0 ;
#endif
}

static void  mxt_exit(void)
{
#ifdef ATMEL_TP_SUPPORT
	info_printk("mxt_exit\n");
	i2c_del_driver(&mxt_driver);
#else 
	info_printk("atmel touchpanel not support\n");
#endif
}

module_init(mxt_init);
module_exit(mxt_exit);

/* Module information */
MODULE_AUTHOR("Joonyoung Shim <jy0922.shim@samsung.com>");
MODULE_DESCRIPTION("Atmel maXTouch Touchscreen driver");
MODULE_LICENSE("GPL");

