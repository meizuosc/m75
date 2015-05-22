
/* Lite-On LTR-303ALS Linux Driver
 *
 * Copyright (C) 2014 Lite-On Technology Corp (Singapore)
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#ifndef _LTR303_H
#define _LTR303_H

#include <linux/ioctl.h>
/* LTR-303 Read/Write Registers */
#define LTR303_ALS_CONTR		0x80
#define LTR303_ALS_MEAS_RATE	0x85

#define LTR303_INTERRUPT		0x8F
#define LTR303_ALS_THRES_UP_0	0x97
#define LTR303_ALS_THRES_UP_1	0x98
#define LTR303_ALS_THRES_LOW_0	0x99
#define LTR303_ALS_THRES_LOW_1	0x9A
#define LTR303_INTERRUPT_PERSIST 0x9E

/* 303's Read Only Registers */
#define LTR303_PART_ID			0x86
#define LTR303_MANUFACTURER_ID	0x87

#define LTR303_ALS_DATA_CH1_0	0x88
#define LTR303_ALS_DATA_CH1_1	0x89
#define LTR303_ALS_DATA_CH0_0	0x8A
#define LTR303_ALS_DATA_CH0_1	0x8B
#define LTR303_ALS_PS_STATUS	0x8C



/* Basic Operating Modes */
#define MODE_ALS_ON_Range1	0x01  ///for als gain x1
#define MODE_ALS_ON_Range2	0x05  ///for als  gain x2
#define MODE_ALS_ON_Range3	0x09  ///for als  gain x4
#define MODE_ALS_ON_Range4	0x0D  ///for als gain x8
#define MODE_ALS_ON_Range5	0x19  ///for als gain x48
#define MODE_ALS_ON_Range6	0x1D  ///for als gain x96

#define MODE_ALS_StdBy		0x00

#define ALS_RANGE_64K	1
#define ALS_RANGE_32K 	2
#define ALS_RANGE_16K 	4
#define ALS_RANGE_8K 	8
#define ALS_RANGE_1300 48
#define ALS_RANGE_600 	96


/* 
 * Magic Number
 * ============
 * Refer to file ioctl-number.txt for allocation
 */
#define LTR303_IOCTL_MAGIC      'c'

/* IOCTLs for ltr303 device */

#define LTR303_IOCTL_ALS_ENABLE		_IOW(LTR303_IOCTL_MAGIC, 1, char *)
#define LTR303_IOCTL_READ_ALS_DATA	_IOR(LTR303_IOCTL_MAGIC, 4, char *)
#define LTR303_IOCTL_READ_ALS_INT	_IOR(LTR303_IOCTL_MAGIC, 5, char *)


/* Power On response time in ms */
#define PON_DELAY	600
#define WAKEUP_DELAY	10

#define ltr303_SUCCESS						0
#define ltr303_ERR_I2C						-1
#define ltr303_ERR_STATUS					-3
#define ltr303_ERR_SETUP_FAILURE			-4
#define ltr303_ERR_GETGSENSORDATA			-5
#define ltr303_ERR_IDENTIFICATION			-6




/* Interrupt vector number to use when probing IRQ number.
 * User changeable depending on sys interrupt.
 * For IRQ numbers used, see /proc/interrupts.
 */
#define GPIO_INT_NO	32

#endif
