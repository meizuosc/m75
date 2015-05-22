 /* 
  *
  *
  * Copyright (C) 2008,2009 MediaTek <www.mediatek.com>
  * Authors: Infinity Chen <infinity.chen@mediatek.com>  
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

#ifndef __MT_BOOT_H__
#define __MT_BOOT_H__
#include <mach/mt_boot_common.h>

/*META COM port type*/
 typedef enum
{
    META_UNKNOWN_COM = 0,
    META_UART_COM,
    META_USB_COM
} META_COM_TYPE;

#define BOOT_DEV_NAME           "BOOT"
#define BOOT_SYSFS              "boot"
#define BOOT_SYSFS_ATTR         "boot_mode"
#define INFO_SYSFS_ATTR         "info"

typedef enum {
    CHIP_SW_VER_01 = 0x0000,
    CHIP_SW_VER_02 = 0x0001
} CHIP_SW_VER;

extern META_COM_TYPE g_meta_com_type;
extern unsigned int g_meta_com_id;

extern void set_meta_com(META_COM_TYPE type, unsigned int id);
extern META_COM_TYPE get_meta_com_type(void);
extern unsigned int get_meta_com_id(void);

extern unsigned int get_chip_code(void);
extern unsigned int get_chip_hw_subcode(void);
extern unsigned int get_chip_hw_ver_code(void);
extern unsigned int get_chip_sw_ver_code(void);
extern unsigned int mt_get_chip_id(void);
extern CHIP_SW_VER  mt_get_chip_sw_ver(void);

typedef enum {
    CHIP_INFO_NONE = 0,
    CHIP_INFO_HW_CODE,
    CHIP_INFO_HW_SUBCODE,
    CHIP_INFO_HW_VER,
    CHIP_INFO_SW_VER,
    CHIP_INFO_FUNCTION_CODE,
    CHIP_INFO_PROJECT_CODE,
    CHIP_INFO_DATE_CODE,
    CHIP_INFO_FAB_CODE,
    CHIP_INFO_MAX,
    CHIP_INFO_ALL,
} CHIP_INFO;

extern unsigned int mt_get_chip_info(CHIP_INFO id);

#endif 

