/* 
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/*
 * Definitions for JSA12xx als/ps sensor chip.
 */
#ifndef __JSA12xx_H__
#define __JSA12xx_H__

#include <linux/ioctl.h>

/*JSA12xx als/ps sensor register related macro*/
#define JSA12xx_REG_CONF 				0X01
#define JSA12xx_REG_INT_FLAG			0X02
#define JSA12xx_REG_PS_THD_LOW			0X03
#define JSA12xx_REG_PS_THD_HIGH			0X04
#define JSA12xx_REG_ALS_THDL 			0X05
#define JSA12xx_REG_ALS_THDM 			0X06
#define JSA12xx_REG_ALS_THDH 			0X07
#define JSA12xx_REG_PS_DATA				0X08
#define JSA12xx_REG_ALS_DATA_HIGH		0X0A
#define JSA12xx_REG_ALS_DATA_LOW		0X09
#define JSA12xx_REG_ALS_RANGE			0X0B

//#define JSA12xx_REG_ID_MODE			0X00


/*JSA12xx related driver tag macro*/
#define JSA12xx_SUCCESS				 		 0
#define JSA12xx_ERR_I2C						-1
#define JSA12xx_ERR_STATUS					-3
#define JSA12xx_ERR_SETUP_FAILURE			-4
#define JSA12xx_ERR_GETGSENSORDATA			-5
#define JSA12xx_ERR_IDENTIFICATION			-6


#endif

