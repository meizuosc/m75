/* BEGIN PN:DTS2013062101609  ,Modified by y00187129, 2013/6/21*/
/* Add Rohm G-Sensor Driver */
/* linux/drivers/hwmon/adxl345.c
 *
 * (C) Copyright 2008 
 * MediaTek <www.mediatek.com>
 *
 * KX023 driver for MT6575
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA  KXTIK
 */
#ifndef KX023_H
#define KX023_H
	 
#include <linux/ioctl.h>
	 
#define KX023_I2C_SLAVE_ADDR		0x3C
	 
 /* KX023 Register Map  (Please refer to KX023 Specifications) */
#define KX023_REG_DEVID			0x0F
#define	KX023_REG_BW_RATE			0x1B
#define KX023_REG_POWER_CTL		0x18
#define KX023_REG_CTL_REG2		0x19
#define KX023_DCST_RESP			0x0c
#define KX023_REG_DATA_FORMAT		0x18
#define KX023_REG_DATA_RESOLUTION		0x18
#define KX023_RANGE_DATA_RESOLUTION_MASK	0x40
#define KX023_REG_DATAX0			0x06	 
#define KX023_FIXED_DEVID			0x15	 
#define KX023_BW_200HZ				0x04
#define KX023_BW_100HZ				0x03
#define KX023_BW_50HZ				0x02	 
#define KX023_MEASURE_MODE		0x80		 
#define KX023_RANGE_MASK		0x18
#define KX023_RANGE_2G			0x00
#define KX023_RANGE_4G			0x08
#define KX023_RANGE_8G			0x10
#define KX023_REG_INT_ENABLE	0x1C

#define KX023_SELF_TEST           0x40
	 	 
	 
#define KX023_SUCCESS						0
#define KX023_ERR_I2C						-1
#define KX023_ERR_STATUS					-3
#define KX023_ERR_SETUP_FAILURE				-4
#define KX023_ERR_GETGSENSORDATA			-5
#define KX023_ERR_IDENTIFICATION			-6
	 
	 
	 
#define KX023_BUFSIZE				256
	 
#endif
/* END PN:DTS2013062101609  ,Modified by y00187129, 2013/6/21*/