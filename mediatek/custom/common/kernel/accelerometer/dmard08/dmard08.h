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
#ifndef DMARD08_H
#define DMARD08_H
	 
#include <linux/ioctl.h>
	 
	#define DMARD08_I2C_SLAVE_WRITE_ADDR		0x38
	 
	 
	#define DMARD08_REG_DATAXLOW			0x02
	

	
#define DMARD08_SUCCESS						0
#define DMARD08_ERR_I2C						-1
#define DMARD08_ERR_STATUS					-3
#define DMARD08_ERR_SETUP_FAILURE			-4
#define DMARD08_ERR_GETGSENSORDATA			-5
#define DMARD08_ERR_IDENTIFICATION			-6
	 
	 
	 
#define DMARD08_BUFSIZE				256
	 
#endif

