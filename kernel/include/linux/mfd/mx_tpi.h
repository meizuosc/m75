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
#ifndef	__MX_QMATRIX__
#define	__MX_QMATRIX__

#include <linux/wakelock.h>
#include <linux/earlysuspend.h>

#define	FW_VERSION     				0x01
#define	FLASH_ADDR_FW_VERSION   		0x1BFD
#define	MX_TPI_DEVICE_ID     			'M'
#define	TPI_I2C_DRIVER_NAME		"mx_tpi"	
#define	TPI_I2C_DRIVER_ADRESS	'M'//0x4D//

enum mx_tpi_reg
{
    TPI_REG_DEVICE_ID	= 0x00,
    TPI_REG_VERSION		= 0x01,
    TPI_REG_CONTROL    	= 0x02, // DBG x x x xx cal rst
    TPI_REG_STATUS    	= 0x03,

    TPI_REG_INT_TYPE    = 0x04,
    TPI_REG_INT_REQ    	= 0x05,

    
    LED_REG_EN             = 0x10,
    LED_REG_PWM             = 0x11,
    LED_REG_SLOPE           = 0x12,
    LED_REG_FADE            = 0x13,
    LED_REG_SLPPRD	        = 0x14,
    LED_REG_SLPPRD2	        = 0x15,

    /* reg  for touch key */
    KEY_REG_BASE 		= 0x20,
    KEY_REG_EN                  = (KEY_REG_BASE + 0x00),
    KEY_REG_WAKEUP_TYPE    	        = (KEY_REG_BASE + 0x01),
    KEY_REG_WAKEUP_CNT    	        = (KEY_REG_BASE + 0x02),
    KEY_REG_WAKEUP_KEY    	        = (KEY_REG_BASE + 0x03),
    KEY_REG_GESTURE_X_UNMASK	= (KEY_REG_BASE + 0x04),
    KEY_REG_GESTURE_Y_UNMASK	= (KEY_REG_BASE + 0x05),
    KEY_REG_GESTURE_TAP		= (KEY_REG_BASE + 0x06),

    TPI_REG_MAX,
};



#define LED_REG_MAX   LED_REG15
#define MXTPI_REG_INVALID	(0xff)

enum mx_tpi_state {
	TPI_STATE_SLEEP = 0,
	TPI_STATE_NORMAL = 1,
	TPI_STATE_IDLE = 2,
	TPI_STATE_SHUTDOWN = 3,

	TPI_STATE_MAX,
};

/* key type definitions */
enum {
	MX_KEY_GESTURE = 0,
	MX_KEY_SINGLE,
	MX_KEY_DOUBLE,
	MX_KEY_LONGPRESS,
	MX_KEY_GESTURE2,
	
	MX_KEY_NONE,

	MX_KEY_TYPE_MAX,
};


enum mx_tpi_cmd {
	TPI_RESET_SENSOR = 0,
	TPI_CALIBRATE = 1,

	TPI_CMD_MAX,
};

typedef struct _device_info
{
	unsigned char id;
	unsigned char ver;
	unsigned short crc;
}s_device_info;


/* Calibrate */
#define MX_TPI_CAL_TIME    200

/* Reset */
#define MX_TPI_RESET_TIME  255

struct mx_tpi_platform_data {
	unsigned int gpio_wake;
	unsigned int gpio_reset;
	unsigned int gpio_irq;
};

struct mx_tpi_data {
	struct device *dev;
	struct i2c_client *client;
	unsigned int irq;
	struct mutex iolock;
	struct wake_lock wake_lock;
	
	unsigned int LedVer;
	unsigned int AVer;
	unsigned int BVer;
	unsigned int gpio_wake;
	unsigned int gpio_reset;
	unsigned int gpio_irq;	
	int (*i2c_readbyte)(struct i2c_client *, u8);
	int (*i2c_writebyte)(struct i2c_client *, u8, u8);
	int (*i2c_readbuf)(struct i2c_client *,u8,int,void *);
	int (*i2c_writebuf)(struct i2c_client *,u8,int,const void *);
	void(*reset)(struct mx_tpi_data *, int);
	void(*wakeup_mcu)(struct mx_tpi_data *,int);
	int(*update)(struct mx_tpi_data *);
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
	struct notifier_block notifier;

	int key_wakeup_type;
	int debug;
	
	s_device_info fw_info;


};

#endif
