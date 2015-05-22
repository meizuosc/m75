/*
 * Synaptics DSX touchscreen driver
 *
 * Copyright (C) 2012 Synaptics Incorporated
 *
 * Copyright (C) 2012 Alexandra Chin <alexandra.chin@tw.synaptics.com>
 * Copyright (C) 2012 Scott Lin <scott.lin@tw.synaptics.com>
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
 */
#ifndef _SYNAPTICS_DSX_RMI4_H_
#define _SYNAPTICS_DSX_RMI4_H_

#define SYNAPTICS_DS4 (1 << 0)
#define SYNAPTICS_DS5 (1 << 1)
#define SYNAPTICS_DSX_DRIVER_PRODUCT (SYNAPTICS_DS4 | SYNAPTICS_DS5)
#define SYNAPTICS_DSX_DRIVER_VERSION 0x2002

#include <linux/version.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include "tpd.h"

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 38))
#define KERNEL_ABOVE_2_6_38
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
#define KERNEL_ABOVE_3_10_0
#endif


#ifdef KERNEL_ABOVE_2_6_38
#define sstrtoul(...) kstrtoul(__VA_ARGS__)
#else
#define sstrtoul(...) strict_strtoul(__VA_ARGS__)
#endif

#define RMI_RO_ATTR S_IRUGO
#define RMI_WO_ATTR (S_IWUSR|S_IWGRP)
#define RMI_RW_ATTR (RMI_RO_ATTR | RMI_WO_ATTR)


#define PDT_PROPS (0X00EF)
#define PDT_START (0x00E9)
#define PDT_END (0x00D0)
#define PDT_ENTRY_SIZE (0x0006)
#define PAGES_TO_SERVICE (10)
#define PAGE_SELECT_LEN (2)
#define ADDRESS_WORD_LEN (2)

#define SYNAPTICS_RMI4_F01 (0x01)
#define SYNAPTICS_RMI4_F11 (0x11)
#define SYNAPTICS_RMI4_F12 (0x12)
#define SYNAPTICS_RMI4_F1A (0x1a)
#define SYNAPTICS_RMI4_F34 (0x34)
#define SYNAPTICS_RMI4_F51 (0x51)
#define SYNAPTICS_RMI4_F54 (0x54)
#define SYNAPTICS_RMI4_F55 (0x55)
#define SYNAPTICS_RMI4_FDB (0xdb)

#define SYNAPTICS_RMI4_PRODUCT_INFO_SIZE 2
#define SYNAPTICS_RMI4_PRODUCT_ID_SIZE 10
#define SYNAPTICS_RMI4_BUILD_ID_SIZE 3

#define F12_FINGERS_TO_SUPPORT 10
#define F12_NO_OBJECT_STATUS 0x00
#define F12_FINGER_STATUS 0x01
#define F12_STYLUS_STATUS 0x02
#define F12_PALM_STATUS 0x03
#define F12_HOVERING_FINGER_STATUS 0x05
#define F12_GLOVED_FINGER_STATUS 0x06

#define MAX_NUMBER_OF_BUTTONS 4
#define MAX_INTR_REGISTERS 4

#define MASK_16BIT 0xFFFF
#define MASK_8BIT 0xFF
#define MASK_7BIT 0x7F
#define MASK_6BIT 0x3F
#define MASK_5BIT 0x1F
#define MASK_4BIT 0x0F
#define MASK_3BIT 0x07
#define MASK_2BIT 0x03
#define MASK_1BIT 0x01

#define SYN_INVALID_MODE (0)
#define SYN_NORMAL_MODE (1)
#define SYN_GESTURM_MODE (2)
#define SYN_HALL_COVER_MODE (3)
#define SYN_HALL_UNCOVER_MODE (4)
#define SYN_SUSPEND_SYSTEM   (5)
#define SYN_RESUME_SYSTEM   (6)

#define SYN_HALL_ADDR (0x0428)
#define SYN_HALL_BIT (1)


#define PLATFORM_DRIVER_NAME "mtk-tpd"
#define I2C_DRIVER_NAME "synaptics_dsx_i2c"
#define SPI_DRIVER_NAME "synaptics_dsx_spi"
#define _STATISTICS_GESTURE_ 

extern int register_probe_func(probe_func_t  probe_func);



/*
 * struct synaptics_dsx_cap_button_map - 0D button map
 * @nbuttons: number of 0D buttons
 * @map: pointer to array of button types
 */
struct synaptics_dsx_cap_button_map {
	unsigned char nbuttons;
	unsigned char *map;
};

/*
 * struct synaptics_dsx_board_data - DSX board data
 * @x_flip: x flip flag
 * @y_flip: y flip flag
 * @swap_axes: swap axes flag
 * @irq_gpio: attention interrupt GPIO
 * @irq_on_state: attention interrupt active state
 * @power_gpio: power switch GPIO
 * @power_on_state: power switch active state
 * @reset_gpio: reset GPIO
 * @reset_on_state: reset active state
 * @irq_flags: IRQ flags
 * @device_descriptor_addr: HID device descriptor address
 * @panel_x: x-axis resolution of display panel
 * @panel_y: y-axis resolution of display panel
 * @power_delay_ms: delay time to wait after powering up device
 * @reset_delay_ms: delay time to wait after resetting device
 * @reset_active_ms: reset active time
 * @byte_delay_us: delay time between two bytes of SPI data
 * @block_delay_us: delay time between two SPI transfers
 * @pwr_reg_name: pointer to name of regulator for power control
 * @bus_reg_name: pointer to name of regulator for bus pullup control
 * @cap_button_map: pointer to 0D button map
 */
struct synaptics_dsx_board_data {
	bool x_flip;
	bool y_flip;
	bool swap_axes;
	int irq_gpio;
	int irq_on_state;
	int power_gpio;
	int power_on_state;
	int reset_gpio;
	int reset_on_state;
	unsigned long irq_flags;
	unsigned short device_descriptor_addr;
	unsigned int panel_x;
	unsigned int panel_y;
	unsigned int power_delay_ms;
	unsigned int reset_delay_ms;
	unsigned int reset_active_ms;
	unsigned int byte_delay_us;
	unsigned int block_delay_us;
	const char *pwr_reg_name;
	const char *bus_reg_name;
	struct synaptics_dsx_cap_button_map *cap_button_map;
};



enum exp_fn {
	RMI_DEV = 0,
	RMI_FW_UPDATER,
	RMI_TEST_REPORTING,
	RMI_PROXIMITY,
	RMI_ACTIVE_PEN,
	RMI_DEBUG,
	RMI_LAST,
};

/*
 * struct synaptics_rmi4_fn_desc - function descriptor fields in PDT entry
 * @query_base_addr: base address for query registers
 * @cmd_base_addr: base address for command registers
 * @ctrl_base_addr: base address for control registers
 * @data_base_addr: base address for data registers
 * @intr_src_count: number of interrupt sources
 * @fn_number: function number
 */
struct synaptics_rmi4_fn_desc {
	unsigned char query_base_addr;
	unsigned char cmd_base_addr;
	unsigned char ctrl_base_addr;
	unsigned char data_base_addr;
	unsigned char intr_src_count;
	unsigned char fn_number;
};

/*
 * synaptics_rmi4_fn_full_addr - full 16-bit base addresses
 * @query_base: 16-bit base address for query registers
 * @cmd_base: 16-bit base address for command registers
 * @ctrl_base: 16-bit base address for control registers
 * @data_base: 16-bit base address for data registers
 */
struct synaptics_rmi4_fn_full_addr {
	unsigned short query_base;
	unsigned short cmd_base;
	unsigned short ctrl_base;
	unsigned short data_base;
};

/*
 * struct synaptics_rmi4_f12_extra_data - extra data of F$12
 * @data1_offset: offset to F12_2D_DATA01 register
 * @data15_offset: offset to F12_2D_DATA15 register
 * @data15_size: size of F12_2D_DATA15 register
 * @data15_data: buffer for reading F12_2D_DATA15 register
 */
struct synaptics_rmi4_f12_extra_data {
	unsigned char data1_offset;
	unsigned char data15_offset;
	unsigned char data15_size;
	unsigned char data15_data[(F12_FINGERS_TO_SUPPORT + 7) / 8];
};

/*
 * struct synaptics_rmi4_fn - RMI function handler
 * @fn_number: function number
 * @num_of_data_sources: number of data sources
 * @num_of_data_points: maximum number of fingers supported
 * @size_of_data_register_block: data register block size
 * @intr_reg_num: index to associated interrupt register
 * @intr_mask: interrupt mask
 * @full_addr: full 16-bit base addresses of function registers
 * @link: linked list for function handlers
 * @data_size: size of private data
 * @data: pointer to private data
 * @extra: pointer to extra data
 */
struct synaptics_rmi4_fn {
	unsigned char fn_number;
	unsigned char num_of_data_sources;
	unsigned char num_of_data_points;
	unsigned char size_of_data_register_block;
	unsigned char intr_reg_num;
	unsigned char intr_mask;
	struct synaptics_rmi4_fn_full_addr full_addr;
	struct list_head link;
	int data_size;
	void *data;
	void *extra;
};

/*
 * struct synaptics_rmi4_device_info - device information
 * @version_major: RMI protocol major version number
 * @version_minor: RMI protocol minor version number
 * @manufacturer_id: manufacturer ID
 * @product_props: product properties
 * @product_info: product information
 * @product_id_string: product ID
 * @build_id: firmware build ID
 * @support_fn_list: linked list for function handlers
 */
struct synaptics_rmi4_device_info {
	unsigned int version_major;
	unsigned int version_minor;
	unsigned char manufacturer_id;
	unsigned char product_props;
	unsigned char product_info[SYNAPTICS_RMI4_PRODUCT_INFO_SIZE];
	unsigned char product_id_string[SYNAPTICS_RMI4_PRODUCT_ID_SIZE + 1];
	unsigned char build_id[SYNAPTICS_RMI4_BUILD_ID_SIZE];
	struct list_head support_fn_list;
};

/*
 * struct synaptics_rmi4_data - RMI4 device instance data
 * @pdev: pointer to platform device
 * @input_dev: pointer to associated input device
 * @hw_if: pointer to hardware interface data
 * @rmi4_mod_info: device information
 * @pwr_reg: pointer to regulator for power control
 * @bus_reg: pointer to regulator for bus pullup control
 * @rmi4_reset_mutex: mutex for software reset
 * @rmi4_report_mutex: mutex for input event reporting
 * @rmi4_io_ctrl_mutex: mutex for communication interface I/O
 * @early_suspend: early suspend power management
 * @current_page: current RMI page for register access
 * @button_0d_enabled: switch for enabling 0d button support
 * @full_pm_cycle: switch for enabling full power management cycle
 * @num_of_tx: number of Tx channels for 2D touch
 * @num_of_rx: number of Rx channels for 2D touch
 * @num_of_fingers: maximum number of fingers for 2D touch
 * @max_touch_width: maximum touch width
 * @report_enable: input data to report for F$12
 * @no_sleep_setting: default setting of NoSleep in F01_RMI_CTRL00 register
 * @intr_mask: interrupt enable mask
 * @button_txrx_mapping: Tx Rx mapping of 0D buttons
 * @num_of_intr_regs: number of interrupt registers
 * @f01_query_base_addr: query base address for f$01
 * @f01_cmd_base_addr: command base address for f$01
 * @f01_ctrl_base_addr: control base address for f$01
 * @f01_data_base_addr: data base address for f$01
 * @firmware_id: firmware build ID
 * @irq: attention interrupt
 * @sensor_max_x: maximum x coordinate for 2D touch
 * @sensor_max_y: maximum y coordinate for 2D touch
 * @flash_prog_mode: flag to indicate flash programming mode status
 * @irq_enabled: flag to indicate attention interrupt enable status
 * @fingers_on_2d: flag to indicate presence of fingers in 2D area
 * @sensor_sleep: flag to indicate sleep state of sensor
 * @stay_awake: flag to indicate whether to stay awake during suspend
 * @irq_enable: pointer to interrupt enable function
 * @reset_device: pointer to device reset function
 */
struct synaptics_rmi4_data {
	struct platform_device *pdev;
	struct input_dev *input_dev;
	struct tp_driver_data * data;
	const struct synaptics_dsx_hw_interface *hw_if;
	struct i2c_client * i2c_client ;
	struct synaptics_rmi4_device_info rmi4_mod_info;
	struct regulator *pwr_reg;
	struct regulator *bus_reg;
	struct kobject * sensor00 ;
	struct kobject * devices  ;
	struct mutex rmi4_reset_mutex;
	struct mutex rmi4_report_mutex;
	struct mutex rmi4_io_ctrl_mutex;
	struct mutex rmi4_hall_mutex ;
	struct notifier_block notify;
	int mode ;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
	unsigned char current_page;
	unsigned char button_0d_enabled;
	unsigned char full_pm_cycle;
	unsigned char num_of_tx;
	unsigned char num_of_rx;
	unsigned char num_of_fingers;
	unsigned char max_touch_width;
	unsigned char report_enable;
	unsigned char no_sleep_setting;
	unsigned char intr_mask[MAX_INTR_REGISTERS];
	unsigned char *button_txrx_mapping;
	unsigned short num_of_intr_regs;
	unsigned short f01_query_base_addr;
	unsigned short f01_cmd_base_addr;
	unsigned short f01_ctrl_base_addr;
	unsigned short f01_data_base_addr;
	unsigned int firmware_id;
	int irq;
	int sensor_max_x;
	int sensor_max_y;
	bool flash_prog_mode;
	bool irq_enabled;
	bool fingers_on_2d;
	/*gesture control  */
	bool gesture_enable ;
	bool disable_all ;
	int  gesture_value  ;
	unsigned char gesture_mask[4];/*byte0:unicode,byte1:swipe,byte3:tap */
	bool touch_debug ;
	bool sensor_sleep;
	bool stay_awake;
	int (*irq_enable)(struct synaptics_rmi4_data *rmi4_data, bool enable);
	int (*reset_device)(struct synaptics_rmi4_data *rmi4_data);
};

struct synaptics_dsx_bus_access {
	unsigned char type;
	int (*read)(struct synaptics_rmi4_data *rmi4_data, unsigned short addr,
		unsigned char *data, unsigned short length);
	int (*write)(struct synaptics_rmi4_data *rmi4_data, unsigned short addr,
		unsigned char *data, unsigned short length);
};

struct synaptics_dsx_hw_interface {
	struct synaptics_dsx_board_data *board_data;
	const struct synaptics_dsx_bus_access *bus_access;
	int (*bl_hw_init)(struct synaptics_rmi4_data *rmi4_data);
	int (*ui_hw_init)(struct synaptics_rmi4_data *rmi4_data);
};

struct synaptics_rmi4_exp_fn_ptr {
	int (*read)(struct synaptics_rmi4_data *rmi4_data, unsigned short addr,
			unsigned char *data, unsigned short length);
	int (*write)(struct synaptics_rmi4_data *rmi4_data, unsigned short addr,
			unsigned char *data, unsigned short length);
	int (*enable)(struct synaptics_rmi4_data *rmi4_data, bool enable);
};

struct synaptics_rmi4_exp_fn {
	enum exp_fn fn_type;
	int (*init)(struct synaptics_rmi4_data *rmi4_data);
	void (*remove)(struct synaptics_rmi4_data *rmi4_data);
	void (*reset)(struct synaptics_rmi4_data *rmi4_data);
	void (*reinit)(struct synaptics_rmi4_data *rmi4_data);
	void (*early_suspend)(struct synaptics_rmi4_data *rmi4_data);
	void (*suspend)(struct synaptics_rmi4_data *rmi4_data);
	void (*resume)(struct synaptics_rmi4_data *rmi4_data);
	void (*late_resume)(struct synaptics_rmi4_data *rmi4_data);
	void (*attn)(struct synaptics_rmi4_data *rmi4_data,
			unsigned char intr_mask);
};

#ifdef M75_TP_GESTURE_SUPPORT
void synaptics_rmi4_gesture_enable(struct synaptics_rmi4_data *rmi4_data);
void synaptics_rmi4_gesture_disable(struct synaptics_rmi4_data *rmi4_data);
int synaptics_rmi4_f12_handler_gesture(struct synaptics_rmi4_data *rmi4_data);
ssize_t synaptics_rmi4_gesture_control_write(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);
ssize_t synaptics_rmi4_gesture_control_read(struct device *dev,
		 struct device_attribute *attr, char *buf);
ssize_t synaptics_rmi4_gesture_hex_write(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);
ssize_t synaptics_rmi4_gesture_hex_read(struct device *dev,
		 struct device_attribute *attr, char *buf);

ssize_t synaptics_rmi4_gesture_value_read(struct device *dev,
		 struct device_attribute *attr, char *buf);
ssize_t synaptics_rmi4_gesture_test(struct device *dev,
		  struct device_attribute *attr, char *buf);
#ifdef _STATISTICS_GESTURE_
ssize_t synaptics_rmi4_statistics_gesture(struct device*dev,
	  struct device_attribute*attr,char *buf);
#endif

#endif


void synaptics_rmi4_sensor_wake(struct synaptics_rmi4_data *rmi4_data);

int synaptics_rmi4_bus_init(void);

void synaptics_rmi4_bus_exit(void);

void synaptics_rmi4_new_function(struct synaptics_rmi4_exp_fn *exp_fn_module,
		bool insert);

int synaptics_fw_updater(unsigned char *fw_data);
void fwu_show_product_id(void);

static inline int synaptics_rmi4_reg_read(
		struct synaptics_rmi4_data *rmi4_data,
		unsigned short addr,
		unsigned char *data,
		unsigned short len)
{
	return rmi4_data->hw_if->bus_access->read(rmi4_data, addr, data, len);
}

static inline int synaptics_rmi4_reg_write(
		struct synaptics_rmi4_data *rmi4_data,
		unsigned short addr,
		unsigned char *data,
		unsigned short len)
{
	return rmi4_data->hw_if->bus_access->write(rmi4_data, addr, data, len);
}

static inline ssize_t synaptics_rmi4_show_error(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	pr_warn("%s Attempted to read from write-only attribute %s\n",
			__func__, attr->attr.name);
	return -EPERM;
}

static inline ssize_t synaptics_rmi4_store_error(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	pr_warn("%s Attempted to write to read-only attribute %s\n",
			__func__, attr->attr.name);
	return -EPERM;
}

static inline void batohs(unsigned short *dest, unsigned char *src)
{
	*dest = src[1] * 0x100 + src[0];
}

static inline void hstoba(unsigned char *dest, unsigned short src)
{
	dest[0] = src % 0x100;
	dest[1] = src / 0x100;
}

#endif
