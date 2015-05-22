/* linux/drivers/hwmon/lis33de.c
 *
 * (C) Copyright 2008 
 * MediaTek <www.mediatek.com>
 *
 * STK8311 driver for MT6573
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
#ifndef STK8311_H
#define STK8311_H 
	 
#include <linux/ioctl.h>
	 
#define STK8311_I2C_SLAVE_ADDR		0x44 
	 
	 /* STK8311 Register Map  (Please refer to STK8311 Specifications) */

#define STK8311_REG_DEVID			0x0E //use  device id = 0x3A
#define STK8311_REG_MODE    		0x0A //use


#define STK8311_REG_OFSX			0x0F //use
#define STK8311_REG_OFSY			0x10 //use
#define STK8311_REG_OFSZ			0x11 //use


#define STK8311_REG_XYZ_DATA_CFG		0x16 //use
#define STK8311_REG_INT    		0x17 //use
#define STK8311_REG_RESET    		0x20 //use


#define STK8311_REG_DATAX0			0x00 //use



//end define register



#define STK8311_FIXED_DEVID			0x58 //use
	 

	 
#define STK8311_MEASURE_MODE		0x01 //use	

	 
#define STK8311_RANGE_2G			0x00 //use
#define STK8311_RANGE_4G			0x01 //use
#define STK8311_RANGE_8G			0x02 //use
#define STK8311_RANGE_16G			0x03 //use

#define STK8311_10BIT_RES	        0x00
#define STK8311_11BIT_RES	        0x01
#define STK8311_12BIT_RES	        0x02

#define STK8311_SUCCESS						0
#define STK8311_ERR_I2C						-1
#define STK8311_ERR_STATUS					-3
#define STK8311_ERR_SETUP_FAILURE			-4
#define STK8311_ERR_GETGSENSORDATA			-5
#define STK8311_ERR_IDENTIFICATION			-6
	 
	 
	 
#define STK8311_BUFSIZE				256
	 
#endif

