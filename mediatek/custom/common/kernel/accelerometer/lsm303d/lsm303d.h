/* linux/drivers/hwmon/LSM303D.c
 *
 * (C) Copyright 2008 
 * MediaTek <www.mediatek.com>
 *
 * LSM303D driver for MT6516
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


#ifndef LSM303D
#define LSM303D
#include <linux/ioctl.h>

#define	I2C_AUTO_INCREMENT	(0x80)



#define	LSM303D_REG_CTL0			0x1F	  
#define LSM303D_REG_DEVID			0x0F
#define	LSM303D_REG_BW_RATE			0x20
#define LSM303D_REG_DATA_FORMAT		0x21
#define LSM303D_REG_DATA_FILTER		0x21
#define LSM303D_REG_DATAX0			0x28


#define LSM303D_REG_OFSX            0XFF




#define LSM303D_FIXED_DEVID			0x49

/* Accelerometer Sensor Full Scale */
#define	LSM303D_ACC_FS_MASK	(0x18)
#define LSM303D_ACC_FS_2G 	(0x00)	/* Full scale 2g */
#define LSM303D_ACC_FS_4G 	(0x08)	/* Full scale 4g */
#define LSM303D_ACC_FS_8G 	(0x10)	/* Full scale 8g */
#define LSM303D_ACC_FS_16G	(0x18)	/* Full scale 16g */

/* Sensitivity */
#define SENSITIVITY_ACC_2G	(16*1024)	/**	60ug/LSB	*/
#define SENSITIVITY_ACC_4G	(8*1024)	/**	120ug/LSB	*/
#define SENSITIVITY_ACC_8G	(4*1024)	/**	240ug/LSB	*/
#define SENSITIVITY_ACC_16G	(2*1024)	/**	480ug/LSB	*/


//#define LSM303D_BW_200HZ			0x0C
//#define LSM303D_BW_100HZ			0x0B
//#define LSM303D_BW_50HZ				0x0A
/* ODR */
#define ODR_ACC_MASK		(0XF0)	/* Mask for odr change on acc */
#define LSM303D_ACC_ODR_OFF	(0x00)  /* Power down */
#define LSM303D_ACC_ODR3_125 (0x10)  /* 3.25Hz output data rate */
#define LSM303D_ACC_ODR6_25	(0x20)  /* 6.25Hz output data rate */
#define LSM303D_ACC_ODR12_5	(0x30)  /* 12.5Hz output data rate */
#define LSM303D_ACC_ODR25	(0x40)  /* 25Hz output data rate */
#define LSM303D_ACC_ODR50	(0x50)  /* 50Hz output data rate */
#define LSM303D_ACC_ODR100	(0x60)  /* 100Hz output data rate */
#define LSM303D_ACC_ODR200	(0x70)  /* 200Hz output data rate */
#define LSM303D_ACC_ODR400	(0x80)  /* 400Hz output data rate */
#define LSM303D_ACC_ODR800	(0x90)  /* 800Hz output data rate */
#define LSM303D_ACC_ODR1600	(0xA0)  /* 1600Hz output data rate */
#define LSM303D_ACC_ODR_ENABLE (0x07)


/* Accelerometer Filter */
#define LSM303D_ACC_FILTER_MASK	(0xC0)
#define FILTER_773		773
#define FILTER_362		362
#define FILTER_194		194
#define FILTER_50		50


#define LSM303D_SUCCESS						0
#define LSM303D_ERR_I2C						-1
#define LSM303D_ERR_STATUS					-3
#define LSM303D_ERR_SETUP_FAILURE			-4
#define LSM303D_ERR_GETGSENSORDATA			-5
#define LSM303D_ERR_IDENTIFICATION			-6
	  
	  
	  
#define LSM303D_BUFSIZE				256
	  
#endif



 /////////////////////////

/**
#ifndef ADXL345_H
#define ADXL345_H
	 
#include <linux/ioctl.h>
	 
#define ADXL345_I2C_SLAVE_ADDR		0xA6
**/
	 
	 /* ADXL345 Register Map  (Please refer to ADXL345 Specifications) */

/**
#define ADXL345_REG_DEVID			0x00
#define ADXL345_REG_THRESH_TAP		0x1D
#define ADXL345_REG_OFSX			0x1E
#define ADXL345_REG_OFSY			0x1F
#define ADXL345_REG_OFSZ			0x20
#define ADXL345_REG_DUR				0x21
#define ADXL345_REG_THRESH_ACT		0x24
#define ADXL345_REG_THRESH_INACT	0x25
#define ADXL345_REG_TIME_INACT		0x26
#define ADXL345_REG_ACT_INACT_CTL	0x27
#define ADXL345_REG_THRESH_FF		0x28
#define ADXL345_REG_TIME_FF			0x29
#define ADXL345_REG_TAP_AXES		0x2A
#define ADXL345_REG_ACT_TAP_STATUS	0x2B
#define	ADXL345_REG_BW_RATE			0x2C
#define ADXL345_REG_POWER_CTL		0x2D
#define ADXL345_REG_INT_ENABLE		0x2E
#define ADXL345_REG_INT_MAP			0x2F
#define ADXL345_REG_INT_SOURCE		0x30
#define ADXL345_REG_DATA_FORMAT		0x31
#define ADXL345_REG_DATAX0			0x32
#define ADXL345_REG_FIFO_CTL		0x38
#define ADXL345_REG_FIFO_STATUS		0x39
	 
#define ADXL345_FIXED_DEVID			0xE5
	 
#define ADXL345_BW_200HZ			0x0C
#define ADXL345_BW_100HZ			0x0B
#define ADXL345_BW_50HZ				0x0A

#define	ADXL345_FULLRANG_LSB		0XFF
	 
#define ADXL345_MEASURE_MODE		0x08	
#define ADXL345_DATA_READY			0x80
	 
#define ADXL345_FULL_RES			0x08
#define ADXL345_RANGE_2G			0x00
#define ADXL345_RANGE_4G			0x01
#define ADXL345_RANGE_8G			0x02
#define ADXL345_RANGE_16G			0x03
#define ADXL345_SELF_TEST           0x80
	 
#define ADXL345_STREAM_MODE			0x80
#define ADXL345_SAMPLES_15			0x0F
	 
#define ADXL345_FS_8G_LSB_G			64
#define ADXL345_FS_4G_LSB_G			128
#define ADXL345_FS_2G_LSB_G			256
	 
#define ADXL345_LEFT_JUSTIFY		0x04
#define ADXL345_RIGHT_JUSTIFY		0x00
	 
	 
#define ADXL345_SUCCESS						0
#define ADXL345_ERR_I2C						-1
#define ADXL345_ERR_STATUS					-3
#define ADXL345_ERR_SETUP_FAILURE			-4
#define ADXL345_ERR_GETGSENSORDATA			-5
#define ADXL345_ERR_IDENTIFICATION			-6
	 
	 
	 
#define ADXL345_BUFSIZE				256
	 
#endif

**/

