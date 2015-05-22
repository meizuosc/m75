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
 * Definitions for JSA1127 als/ps sensor chip.
 */
#ifndef __JSA1127_H__
#define __JSA1127_H__

#include <linux/ioctl.h>

/*JSA1127 als/ps sensor register related macro*/
#define JSA1127_REG_ALS_CONF 		0X0C
#define JSA1127_REG_ALS_THDH 		0X01
#define JSA1127_REG_ALS_THDL 		0X02
#define JSA1127_REG_PS_CONF1_2		0X03
#define JSA1127_REG_PS_CONF3_MS		0X04
#define JSA1127_REG_PS_CANC			0X05
#define JSA1127_REG_PS_THD			0X06
#define JSA1127_REG_PS_DATA			0X08
#define JSA1127_REG_ALS_DATA		0X09
#define JSA1127_REG_INT_FLAG		0X0B
#define JSA1127_REG_ID_MODE			0X00

/*JSA1127 related driver tag macro*/
#define JSA1127_SUCCESS				 		 0
#define JSA1127_ERR_I2C						-1
#define JSA1127_ERR_STATUS					-3
#define JSA1127_ERR_SETUP_FAILURE			-4
#define JSA1127_ERR_GETGSENSORDATA			-5
#define JSA1127_ERR_IDENTIFICATION			-6


#endif

