/* linux/drivers/hwmon/adxl345.c
 *
 * (C) Copyright 2008 
 * MediaTek <www.mediatek.com>
 *
 * BMA150 driver for MT6516
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA  BMA150
 */
#ifndef DMARD09_H
#define DMARD09_H
	 
#include <linux/ioctl.h>
	 
	//#define DMARD08_I2C_SLAVE_WRITE_ADDR		0x38
	#define DMARD09_I2C_SLAVE_WRITE_ADDR		0x3a 
	
#define DMARD09_SUCCESS						0
#define DMARD09_ERR_I2C						-1
#define DMARD09_ERR_STATUS					-3
#define DMARD09_ERR_SETUP_FAILURE			-4
#define DMARD09_ERR_GETGSENSORDATA			-5
#define DMARD09_ERR_IDENTIFICATION			-6
	 
	 
	 
#define DMARD09_BUFSIZE				256

#define REG_ACTR 				0x00
#define REG_STAT 				0x0A
#define REG_DX					0x0C
#define REG_DY					0x0E
#define REG_DZ	 				0x10
#define REG_DT	 				0x12
#define REG_INL 				0x16
#define REG_DC	 				0x18
#define REG_CNT_L1 				0x1B
#define REG_CNT_L2				0x1C
#define REG_CNT_L3				0x1D
#define REG_INC 				0x1E
#define REG_DSP 				0x20
#define REG_THR1 				0x62
//#define REG_THR2 				0x64

#define MODE_ACTIVE				0x61	/* active  with rewind*/
#define MODE_POWERDOWN			0x60	/* powerdown */

#define VALUE_INIT_READY        0x02    /*IC init ok*/
#define VALUE_WHO_AM_I			0x95	/* D09 WMI */
//#define VALUE_ODR_200			0x9C	/* conversion rate 200Hz	*/
#define VALUE_ODR_100			0x98	/* conversion rate 100Hz	*/
#define VALUE_ODR_50			0x94	/* conversion rate 50Hz	*/
#define VALUE_ODR_20			0x90	/* conversion rate 20Hz	*/
#define VALUE_ODR_10			0x8C	/* conversion rate 10Hz	*/
#define VALUE_ODR_5				0x88	/* conversion rate 5Hz	*/
#define VALUE_ODR_1				0x84	/* conversion rate 1Hz	*/
#define VALUE_ODR_0_5			0x80	/* conversion rate 0.5Hz	*/
#define VALUE_CNT_L2			0xE4	/* Disable IEN	*/
/* Optional Digital Filter [Low Byte and High Byte Order] */
#define	ODF_NoFilter	0x00	/* No filter */
#define	ODF_Ave_4		0x03	/* smooth filter 1/4 Bandwidth */
#define	ODF_Ave_8		0x07	/* smooth filter 1/8 Bandwidth */
#define	ODF_Ave_16		0x0f	/* smooth filter 1/16 Bandwidth */

#define USE_MTK_CALIBRATE
#ifndef USE_MTK_CALIBRATE
#define AVG_NUM 				16
#define SENSOR_DATA_SIZE 		3 
#define DEFAULT_SENSITIVITY 	256

typedef union {
	struct {
		s16	x;
		s16	y;
		s16	z;
	} u;
	s16	v[SENSOR_DATA_SIZE];
} raw_data;
#endif
	 
#endif

