/* 
 *
 * (C) Copyright 2008 
 * MediaTek <www.mediatek.com>
 *
 * MXC622X driver for MT6516
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
 
#include <linux/ioctl.h>

#define MXC622X_I2C_SLAVE_ADDR        0x2A

/* MXC622X register address */
#define MXC622X_REG_DATAX0        0x00
#define MXC622X_REG_DATAY0        0x01
#define MXC622X_REG_ORIENTATION_SHAKE_STATUS    0x02
#define MXC622X_REG_DETECTION     0x04

#define MXC622X_REG_CHIP_ID      0x08

#define MXC622X_BW_100HZ        0x00
#define MXC622X_RANGE_2G        0x00

#define MXC622X_BUFSIZE				256



#define MXC622X_SUCCESS						0
#define MXC622X_ERR_I2C						-1
#define MXC622X_ERR_STATUS					-3
#define MXC622X_ERR_SETUP_FAILURE			-4
#define MXC622X_ERR_GETGSENSORDATA			-5
#define MXC622X_ERR_IDENTIFICATION			-6


