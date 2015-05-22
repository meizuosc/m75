/*
 * Atmel maXTouch Touchscreen driver
 *
 * Copyright (C) 2010 Samsung Electronics Co.Ltd
 * Author: Joonyoung Shim <jy0922.shim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __LINUX_ATMEL_MXT_TS_H
#define __LINUX_ATMEL_MXT_TS_H

#include <linux/types.h>
#include <linux/completion.h>

#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/dma-mapping.h>


#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include "tpd.h"

#define ATMEL_DRIVER_NAME "atmel_mxt_ts"
#define MEIZU_DRIVER_NAME "mx_tsp"
#define ATMLE_I2C_ADDR    0x4a
#define TP_IRQ_GPIO    296
#define TP_RST_GPIO    231

/* Object types */
#define MXT_DEBUG_DIAGNOSTIC_T37	37
#define MXT_GEN_MESSAGE_T5		5
#define MXT_GEN_COMMAND_T6		6
#define MXT_GEN_POWER_T7		7
#define MXT_GEN_ACQUIRE_T8		8
#define MXT_GEN_DATASOURCE_T53		53
#define MXT_TOUCH_MULTI_T9		9
#define MXT_TOUCH_KEYARRAY_T15		15
#define MXT_TOUCH_PROXIMITY_T23		23
#define MXT_TOUCH_PROXKEY_T52		52
#define MXT_PROCI_GRIPFACE_T20		20
#define MXT_PROCG_NOISE_T22		22
#define MXT_PROCI_ONETOUCH_T24		24
#define MXT_PROCI_TWOTOUCH_T27		27
#define MXT_PROCI_GRIP_T40		40
#define MXT_PROCI_PALM_T41		41
#define MXT_PROCI_TOUCHSUPPRESSION_T42	42
#define MXT_PROCI_STYLUS_T47		47
#define MXT_PROCG_NOISESUPPRESSION_T48	48
#define MXT_SPT_COMMSCONFIG_T18		18
#define MXT_SPT_GPIOPWM_T19		19
#define MXT_SPT_SELFTEST_T25		25
#define MXT_SPT_CTECONFIG_T28		28
#define MXT_SPT_USERDATA_T38		38
#define MXT_SPT_DIGITIZER_T43		43
#define MXT_SPT_MESSAGECOUNT_T44	44
#define MXT_SPT_CTECONFIG_T46		46
#define MXT_PROCI_ACTIVE_STYLUS_T63	63
#define MXT_TOUCH_MULTITOUCHSCREEN_T100 100
#define MXT_GESTURE_TAP_T93  93
#define MXT_GESTURE_UNICODE_CTR_T221 221
#define MXT_GESTURE_UNICODE_T220 220
#define MXT_NOISE_SUPPRESS_T72 72
#define MXT_IR_OBJECT_T70 70
#define MXT_HALL_OBJECT_T78 78
#define MXT_HALL_OBJECT_T80 80
#define MXT_IR_INST_NUM 7 


#define MXT_CONFIG_ID_SIZE 4
#define MXT_CONFIG_ID_OFFSET 10 
#define MXT_T25_TEST_ALL 0xFE
#define MXT_T25_ENABLE 0x03
#define MXT_UPDATE_BY_OBJECT

#define MXT_ENABLE_UNI_SW  0x07
#define MXT_UNICODE_MAX 0x08 


#define MXT_HALL_SUPPORT 

#define MXT_HALL_UNCOVER 4
#define MXT_HALL_COVER 3
#define MXT_GESTURE_MODE 2
#define MXT_NORMAL_MODE 1

#define  HALL_COVER  2 
#define  HALL_UNCOVER 3


struct unicode_entity{
u16 offset ;
u8 value ;
};

struct unicode_control_info {
u8 code ;
u8 number ;
struct unicode_entity * entity ;
};


/* The platform data for the Atmel maXTouch touchscreen driver */
struct mxt_platform_data {
	unsigned long irqflags;
	u8 t19_num_keys;
	const unsigned int *t19_keymap;
	int t15_num_keys;
	const unsigned int *t15_keymap;
	unsigned long gpio_reset;
	const char *cfg_name;
};


/* MXT_GEN_POWER_T7 field */
struct t7_config {
	u8 idle;
	u8 active;
} __packed;

struct mxt_info {
	u8 family_id;
	u8 variant_id;
	u8 version;
	u8 build;
	u8 matrix_xsize;
	u8 matrix_ysize;
	u8 object_num;
};

struct mxt_object {
	u8 type;
	u16 start_address;
	u8 size_minus_one;
	u8 instances_minus_one;
	u8 num_report_ids;
} __packed;



struct mxt_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	char phys[64];		/* device physical location */
	struct mxt_platform_data *pdata;
    bool touch_debug ;	
	bool force_update ;

	bool gesture_enable ;
	bool disable_all ;
	char  gesture_value  ;
	unsigned char gesture_mask[4];/*byte0:unicode,byte1:swipe,byte3:tap */
	bool t220_query ;
	u8 T221_reportid ;
	u8 T93_reportid ;	
    struct unicode_control_info unicode_info[MXT_UNICODE_MAX];

    struct notifier_block notify;
   int mode ;/*1:hall cover */

	struct delayed_work work;
	struct workqueue_struct *workqueue;
	
	struct tp_driver_data *data ;
	struct mxt_object *object_table;
	struct mxt_info *info;
	void *raw_info_block;
	unsigned int irq;
	unsigned int max_x;
	unsigned int max_y;
	unsigned int pri_max_x;
	unsigned int pri_max_y;
	bool in_bootloader;
	u16 mem_size;
	u16 object_max_size ;
	u8 t100_aux_ampl;
	u8 t100_aux_area;
	u8 t100_aux_vect;
	struct bin_attribute mem_access_attr;
	bool debug_enabled;
	bool debug_v2_enabled;
	u8 *debug_msg_data;
	u16 debug_msg_count;
	struct bin_attribute debug_msg_attr;
	struct mutex debug_msg_lock;
	struct mutex hall_lock ;
	u8 max_reportid;
	u32 config_crc;
	u32 info_crc;
	u8 bootloader_addr;
	struct t7_config t7_cfg;
	u8 *msg_buf;
	u8 *object_buf ;
	u8 t6_status;
	bool update_input;
	u8 last_message_count;
	u8 num_touchids;
	u8 num_stylusids;
	unsigned long t15_keystatus;
	bool use_retrigen_workaround;
	bool use_regulator;
	struct regulator *reg_vdd;
	struct regulator *reg_avdd;
	char *fw_name;
	char *cfg_name;

	/* Cached parameters from object table */
	u16 T5_address;
	u8 T5_msg_size;
	u8 T6_reportid;
	u16 T6_address;
	u16 T7_address;
	u8 T9_reportid_min;
	u8 T9_reportid_max;
	u8 T15_reportid_min;
	u8 T15_reportid_max;
	u16 T18_address;
	u8 T19_reportid;
	u8 T42_reportid_min;
	u8 T42_reportid_max;
	u16 T44_address;
	u8 T48_reportid;
	u8 T63_reportid_min;
	u8 T63_reportid_max;
	u8 T100_reportid_min;
	u8 T100_reportid_max;
	u8 T25_reportid ;
	u8 T25_result ;

	/* for fw update in bootloader */
	struct completion bl_completion;
	struct completion t25_completion ;
	/* for reset handling */
	struct completion reset_completion;

	/* for reset handling */
	struct completion crc_completion;

	/* Enable reporting of input events */
	bool enable_reporting;

	/* Indicates whether device is in suspend */
	bool suspended;
	struct kobject * devices ;
	struct early_suspend early_suspend ;
	
};
int __mxt_write_reg(struct i2c_client *i2c_client,
		 unsigned short addr,unsigned short length,unsigned char *data);
int  __mxt_read_reg(struct i2c_client *i2c_client,
			 unsigned short addr, unsigned short length, unsigned char *data);


extern int mxt_handler_uni_sw_gesture(struct mxt_data *mxt_data,char *data);
extern void mxt_gesture_enable(struct mxt_data *mxt_data);
extern void mxt_gesture_disable(struct mxt_data *mxt_data);
extern int mxt_handler_tap_gesture(struct mxt_data *mxt_data,char *data);

extern ssize_t mxt_gesture_value_read(struct device *dev,
		 struct device_attribute *attr, char *buf);
extern ssize_t mxt_gesture_test(struct device *dev,
		  struct device_attribute *attr, char *buf);

extern ssize_t mxt_gesture_hex_read(struct device *dev,
		 struct device_attribute *attr, char *buf);
extern ssize_t mxt_gesture_hex_write(struct device *dev,
			 struct device_attribute *attr, const char *buf, size_t count);


extern ssize_t mxt_gesture_control_read(struct device *dev,
		 struct device_attribute *attr, char *buf);
		 
extern ssize_t mxt_gesture_control_write(struct device *dev,
			 struct device_attribute *attr, const char *buf, size_t count);
extern struct mxt_object *
mxt_get_object(struct mxt_data *data, u8 type);

#ifdef MXT_HALL_SUPPORT
struct mxt_hall_data{
 char  x_pos ;
 char  x_len ;
 char  y_pos ;
 char  y_len ;
 unsigned char  T80 ;
 unsigned char  T78_data[4] ;
 
};

int mxt_register_hall_notify(struct mxt_data *);
#endif
#endif /* __LINUX_ATMEL_MXT_TS_H */
