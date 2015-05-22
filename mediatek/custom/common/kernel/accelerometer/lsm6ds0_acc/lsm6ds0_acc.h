/* linux/drivers/hwmon/LSM6DS0.c
 *
 * (C) Copyright 2008 
 * MediaTek <www.mediatek.com>
 *
 * LSM6DS0 driver for MT6516
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
#ifndef LSM6DS0_ACC_H
#define LSM6DS0_ACC_H

#include <linux/ioctl.h>

#define LSM6DS0_ACC_I2C_SLAVE_ADDR		0xD6     //SD0 high--->D6    SD0 low ----->D4
	 
	 /* LSM6DS0 Register Map  (Please refer to LSM6DS0 Specifications) */
#define LSM6DS0_ACC_REG_CTL_REG5		0x1F
#define LSM6DS0_ACC_REG_CTL_REG6		0x20
#define LSM6DS0_ACC_REG_CTL_REG4		0x1E
#define LSM6DS0_ACC_REG_CTL_REG8		0x22 

#define LSM6DS0_ACC_REG_OUT_X		    0x28
#define LSM6DS0_ACC_REG_OUT_Y		    0x2A
#define LSM6DS0_ACC_REG_OUT_Z		    0x2C

#define LSM6DS0_ACC_REG_DEVID			0x0F
#define WHO_AM_I 					0x68

	 
#define LSM6DS0_ACC_BW_400HZ			0xC0
#define LSM6DS0_ACC_BW_200HZ			0xA0
#define LSM6DS0_ACC_BW_100HZ			0x80 //400 or 100 on other choise //changed
#define LSM6DS0_ACC_BW_50HZ				0x60

	 
//#define LSM6DS0_ACC_FULL_RES			0x08
#define LSM6DS0_ACC_RANGE_2G			0x00
#define LSM6DS0_ACC_RANGE_4G			0x10
#define LSM6DS0_ACC_RANGE_8G			0x18 //8g or 2g no ohter choise//changed
	 
#define LSM6DS0_ACC_SUCCESS						0
#define LSM6DS0_ACC_ERR_I2C						-1
#define LSM6DS0_ACC_ERR_STATUS					-3
#define LSM6DS0_ACC_ERR_SETUP_FAILURE			-4
#define LSM6DS0_ACC_ERR_GETGSENSORDATA			-5
#define LSM6DS0_ACC_ERR_IDENTIFICATION			-6	 
#define LSM6DS0_ACC_BUFSIZE				256
	 
#endif

