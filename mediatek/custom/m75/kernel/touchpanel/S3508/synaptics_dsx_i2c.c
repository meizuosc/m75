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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/types.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
//#include <linux/input/synaptics_dsx.h>
#include "synaptics_dsx_core.h"
#include <linux/dma-mapping.h>

#define SYN_I2C_RETRY_TIMES 2
static u8 *gpwDMABuf_va = NULL;
static u32 gpwDMABuf_pa = 0;
static u8 *gprDMABuf_va = NULL;
static u32 gprDMABuf_pa = 0;
static struct i2c_msg *read_msg;
#define I2C_DMA_LIMIT 252
#define I2C_DMA_RBUF_SIZE 4096
#define I2C_DMA_WBUF_SIZE 1024

#if 0
static int parse_dt(struct device *dev, struct synaptics_dsx_board_data *bdata)
{
	int retval;
	u32 value;
	const char *name;
	struct property *prop;
	struct device_node *np = dev->of_node;

	bdata->irq_gpio = of_get_named_gpio_flags(np,
			"synaptics,irq-gpio", 0, NULL);

	retval = of_property_read_u32(np, "synaptics,irq-flags", &value);
	if (retval < 0)
		return retval;
	else
		bdata->irq_flags = value;

	retval = of_property_read_string(np, "synaptics,pwr-reg-name", &name);
	if (retval == -EINVAL)
		bdata->pwr_reg_name = NULL;
	else if (retval < 0)
		return retval;
	else
		bdata->pwr_reg_name = name;

	retval = of_property_read_string(np, "synaptics,bus-reg-name", &name);
	if (retval == -EINVAL)
		bdata->bus_reg_name = NULL;
	else if (retval < 0)
		return retval;
	else
		bdata->bus_reg_name = name;

	if (of_property_read_bool(np, "synaptics,power-gpio")) {
		bdata->power_gpio = of_get_named_gpio_flags(np,
				"synaptics,power-gpio", 0, NULL);
		retval = of_property_read_u32(np, "synaptics,power-on-state",
				&value);
		if (retval < 0)
			return retval;
		else
			bdata->power_on_state = value;
	} else {
		bdata->power_gpio = -1;
	}

	if (of_property_read_bool(np, "synaptics,power-delay-ms")) {
		retval = of_property_read_u32(np, "synaptics,power-delay-ms",
				&value);
		if (retval < 0)
			return retval;
		else
			bdata->power_delay_ms = value;
	} else {
		bdata->power_delay_ms = 0;
	}

	if (of_property_read_bool(np, "synaptics,reset-gpio")) {
		bdata->reset_gpio = of_get_named_gpio_flags(np,
				"synaptics,reset-gpio", 0, NULL);
		retval = of_property_read_u32(np, "synaptics,reset-on-state",
				&value);
		if (retval < 0)
			return retval;
		else
			bdata->reset_on_state = value;
		retval = of_property_read_u32(np, "synaptics,reset-active-ms",
				&value);
		if (retval < 0)
			return retval;
		else
			bdata->reset_active_ms = value;
	} else {
		bdata->reset_gpio = -1;
	}

	if (of_property_read_bool(np, "synaptics,reset-delay-ms")) {
		retval = of_property_read_u32(np, "synaptics,reset-delay-ms",
				&value);
		if (retval < 0)
			return retval;
		else
			bdata->reset_delay_ms = value;
	} else {
		bdata->reset_delay_ms = 0;
	}

	bdata->swap_axes = of_property_read_bool(np, "synaptics,swap-axes");

	bdata->x_flip = of_property_read_bool(np, "synaptics,x-flip");

	bdata->y_flip = of_property_read_bool(np, "synaptics,y-flip");

	prop = of_find_property(np, "synaptics,cap-button-map", NULL);
	if (prop && prop->length) {
		bdata->cap_button_map->map = devm_kzalloc(dev,
				prop->length,
				GFP_KERNEL);
		if (!bdata->cap_button_map->map)
			return -ENOMEM;
		bdata->cap_button_map->nbuttons = prop->length / sizeof(u32);
		retval = of_property_read_u32_array(np,
				"synaptics,cap-button-map",
				bdata->cap_button_map->map,
				bdata->cap_button_map->nbuttons);
		if (retval < 0) {
			bdata->cap_button_map->nbuttons = 0;
			bdata->cap_button_map->map = NULL;
		}
	} else {
		bdata->cap_button_map->nbuttons = 0;
		bdata->cap_button_map->map = NULL;
	}

	return 0;
}
#endif


static void syanptics_rmi4_dump_gpio(struct synaptics_rmi4_data * rmi4_data)
{
	
	printk(KERN_ERR"%s:SDA:%d SCL:%d IRQ:%d RST:%d IR:%d\n",__func__,
				(rmi4_data->data->in.hw_get_gpio_value(SDA_GPIO_INDX)),
				(rmi4_data->data->in.hw_get_gpio_value(SCL_GPIO_INDX)),
				(rmi4_data->data->in.hw_get_gpio_value(IRQ_GPIO_INDX)),
				(rmi4_data->data->in.hw_get_gpio_value(RST_GPIO_INDX)),
				(rmi4_data->data->in.hw_get_gpio_value(IR_GPIO_INDX)));
	
	return ;
}


static int synaptics_rmi4_i2c_set_page(struct synaptics_rmi4_data *rmi4_data,
		unsigned short addr)
{
	int retval;
	unsigned char retry;
	unsigned char buf[PAGE_SELECT_LEN];
	unsigned char page;
	struct i2c_client *i2c = to_i2c_client(rmi4_data->pdev->dev.parent);

	page = ((addr >> 8) & MASK_8BIT);
	if (page != rmi4_data->current_page) {
		buf[0] = MASK_8BIT;
		buf[1] = page;
		for (retry = 0; retry < SYN_I2C_RETRY_TIMES; retry++) {
			retval = i2c_master_send(i2c, buf, PAGE_SELECT_LEN);
			if (retval != PAGE_SELECT_LEN) {
				dev_err(rmi4_data->pdev->dev.parent,
						"%s: I2C retry %d\n",
						__func__, retry + 1);
				syanptics_rmi4_dump_gpio(rmi4_data);
				msleep(20);
			} else {
				rmi4_data->current_page = page;
				break;
			}
		}
	} else {
		retval = PAGE_SELECT_LEN;
	}

	return retval;
}

/**
 * synaptics_rmi4_i2c_read()
 *
 * Called by various functions in this driver, and also exported to
 * other expansion Function modules such as rmi_dev.
 *
 * This function reads data of an arbitrary length from the sensor,
 * starting from an assigned register address of the sensor, via I2C
 * with a retry mechanism.
 */
static int synaptics_rmi4_i2c_read(struct synaptics_rmi4_data *rmi4_data,
			 unsigned short addr, unsigned char *data, unsigned short length)
{
	 int retval;
	 unsigned char retry;
	 unsigned char buf;
	 unsigned char *buf_va = NULL;
	 int full = length / I2C_DMA_LIMIT;
	 int partial = length % I2C_DMA_LIMIT;
	 int total;
	 int last;
	 int ii;
	 static int msg_length;
 
	 mutex_lock(&(rmi4_data->rmi4_io_ctrl_mutex));
 
	 if(!gprDMABuf_va){
	   gprDMABuf_va = (u8 *)dma_alloc_coherent(NULL, I2C_DMA_RBUF_SIZE, &gprDMABuf_pa, GFP_KERNEL);
	   if(!gprDMABuf_va){
		 printk("[Error] Allocate DMA I2C Buffer failed!\n");
	   }
	 }
 
	 buf_va = gprDMABuf_va;
 
	 if ((full + 2) > msg_length) {
		 kfree(read_msg);
		 msg_length = full + 2;
		 read_msg = kcalloc(msg_length, sizeof(struct i2c_msg), GFP_KERNEL);
	 }
 
	 read_msg[0].addr = rmi4_data->i2c_client->addr;
	 read_msg[0].flags = 0;
	 read_msg[0].len = 1;
	 read_msg[0].buf = &buf;
	 read_msg[0].timing = 400;
 
	 if (partial) {
		 total = full + 1;
		 last = partial;
	 } else {
		 total = full;
		 last = I2C_DMA_LIMIT;
	 }
 
	 for (ii = 1; ii <= total; ii++) {
		 read_msg[ii].addr = rmi4_data->i2c_client->addr;
		 read_msg[ii].flags = I2C_M_RD;
		 read_msg[ii].len = (ii == total) ? last : I2C_DMA_LIMIT;
		 read_msg[ii].buf = (unsigned char*)(gprDMABuf_pa + I2C_DMA_LIMIT * (ii - 1));
		 read_msg[ii].ext_flag = (rmi4_data->i2c_client->ext_flag | I2C_ENEXT_FLAG | I2C_DMA_FLAG);
		 read_msg[ii].timing = 400;
	 }
 
	 buf = addr & MASK_8BIT;
 
	 retval = synaptics_rmi4_i2c_set_page(rmi4_data, addr);
	 if (retval != PAGE_SELECT_LEN)
		 goto exit;
 
	 for (retry = 0; retry < SYN_I2C_RETRY_TIMES; retry++) {
		 if (i2c_transfer(rmi4_data->i2c_client->adapter, read_msg, (total + 1)) == (total + 1)) {
 
			 retval = length;
			 break;
		 }
		 dev_err(&rmi4_data->i2c_client->dev,
				 "%s: I2C retry %d\n",
				 __func__, retry + 1);
		 syanptics_rmi4_dump_gpio(rmi4_data);
		 msleep(20);
	 }
 
	 if (retry == SYN_I2C_RETRY_TIMES) {
		 dev_err(&rmi4_data->i2c_client->dev,
				 "%s: I2C read over retry limit\n",
				 __func__);
		 retval = -EIO;
	 }
 
	 memcpy(data, buf_va, length);
 
 exit:
		 /* if(gprDMABuf_va){ */
		 /* 		dma_free_coherent(NULL, 4096, gprDMABuf_va, gprDMABuf_pa); */
		 /* 		gprDMABuf_va = NULL; */
		 /* 		gprDMABuf_pa = NULL; */
		 /* } */
	 mutex_unlock(&(rmi4_data->rmi4_io_ctrl_mutex));
 
	 return retval;
 }
 
  /**
  * synaptics_rmi4_i2c_write()
  *
  * Called by various functions in this driver, and also exported to
  * other expansion Function modules such as rmi_dev.
  *
  * This function writes data of an arbitrary length to the sensor,
  * starting from an assigned register address of the sensor, via I2C with
  * a retry mechanism.
  */
 static int synaptics_rmi4_i2c_write(struct synaptics_rmi4_data *rmi4_data,
		 unsigned short addr, unsigned char *data, unsigned short length)
 {
	 int retval;
	 unsigned char retry;
	 //unsigned char buf[length + 1];
	 unsigned char *buf_va = NULL;
	 mutex_lock(&(rmi4_data->rmi4_io_ctrl_mutex));
 
	 if(!gpwDMABuf_va){
	   gpwDMABuf_va = (u8 *)dma_alloc_coherent(NULL, I2C_DMA_WBUF_SIZE, &gpwDMABuf_pa, GFP_KERNEL);
	   if(!gpwDMABuf_va){
		 printk("[Error] Allocate DMA I2C Buffer failed!\n");
	   }
	 }
	 buf_va = gpwDMABuf_va;
 
	 struct i2c_msg msg[] = {
		 {
			 .addr = rmi4_data->i2c_client->addr,
			 .flags = 0,
			 .len = length + 1,
			 .buf = (unsigned char*)gpwDMABuf_pa,
			 .ext_flag=(rmi4_data->i2c_client->ext_flag|I2C_ENEXT_FLAG|I2C_DMA_FLAG),
			 .timing = 400,
		 }
	 };
 
	 retval = synaptics_rmi4_i2c_set_page(rmi4_data, addr);
	 if (retval != PAGE_SELECT_LEN)
		 goto exit;
 
	 buf_va[0] = addr & MASK_8BIT;
 
	 memcpy(&buf_va[1],&data[0] , length);
 
	 for (retry = 0; retry < SYN_I2C_RETRY_TIMES; retry++) {
		 if (i2c_transfer(rmi4_data->i2c_client->adapter, msg, 1) == 1) {
			 retval = length;
			 break;
		 }
		 dev_err(&rmi4_data->i2c_client->dev,
				 "%s: I2C retry %d\n",
				 __func__, retry + 1);		 
		 syanptics_rmi4_dump_gpio(rmi4_data);
		 msleep(20);
	 }
 
	 if (retry == SYN_I2C_RETRY_TIMES) {
		 dev_err(&rmi4_data->i2c_client->dev,
				 "%s: I2C write over retry limit\n",
				 __func__);
		 retval = -EIO;
	 }
 
 exit:
	 /* if(gpwDMABuf_va){ */
	 /* 			dma_free_coherent(NULL, 1024, gpwDMABuf_va, gpwDMABuf_pa); */
	 /* 			gpwDMABuf_va = NULL; */
	 /* 			gpwDMABuf_pa = NULL; */
	 /* 	} */
	 mutex_unlock(&(rmi4_data->rmi4_io_ctrl_mutex));
 
	 return retval;
 }


static struct synaptics_dsx_bus_access bus_access = {
	.type = BUS_I2C,
	.read = synaptics_rmi4_i2c_read,
	.write = synaptics_rmi4_i2c_write,
};

// Unity8 is expecting home key to generate KEY_LEFTMETA key even (key code 125)
static unsigned char rmi4_button_codes[] = {KEY_LEFTMETA};
static struct synaptics_dsx_cap_button_map cap_button_map = {
   .nbuttons   = ARRAY_SIZE(rmi4_button_codes),
   .map 	   = rmi4_button_codes,
};


static struct synaptics_dsx_hw_interface hw_if;
struct synaptics_dsx_board_data board_data = {
 .power_delay_ms = 20,
 .reset_delay_ms = 20,
 .reset_active_ms = 20,
 .cap_button_map = &cap_button_map,

};

 struct platform_device *synaptics_dsx_i2c_device ;

static void synaptics_rmi4_i2c_dev_release(struct device *dev)
{
	kfree(synaptics_dsx_i2c_device);
		
	return;
}

 int synaptics_rmi4_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *dev_id)
{
	int retval;


	synaptics_dsx_i2c_device = kzalloc(
			sizeof(struct platform_device),
			GFP_KERNEL);
	if (!synaptics_dsx_i2c_device) {
		dev_err(&client->dev,
				"%s: Failed to allocate memory for synaptics_dsx_i2c_device\n",
				__func__);
		return -ENOMEM;
	}

#if 0
	if (client->dev.of_node) {
		hw_if.board_data = devm_kzalloc(&client->dev,
				sizeof(struct synaptics_dsx_board_data),
				GFP_KERNEL);
		if (!hw_if.board_data) {
			dev_err(&client->dev,
					"%s: Failed to allocate memory for board data\n",
					__func__);
			return -ENOMEM;
		}
		hw_if.board_data->cap_button_map = devm_kzalloc(&client->dev,
				sizeof(struct synaptics_dsx_cap_button_map),
				GFP_KERNEL);
		if (!hw_if.board_data->cap_button_map) {
			dev_err(&client->dev,
					"%s: Failed to allocate memory for button map\n",
					__func__);
			return -ENOMEM;
		}
		parse_dt(&client->dev, hw_if.board_data);
	}
#else
//	hw_if.board_data = client->dev.platform_data;
	hw_if.board_data = &board_data;
#endif

	hw_if.bus_access = &bus_access;

	synaptics_dsx_i2c_device->name = PLATFORM_DRIVER_NAME;
	synaptics_dsx_i2c_device->id = 0;
	synaptics_dsx_i2c_device->num_resources = 0;
	synaptics_dsx_i2c_device->dev.parent = &client->dev;
	synaptics_dsx_i2c_device->dev.platform_data = &hw_if;
	synaptics_dsx_i2c_device->dev.release = synaptics_rmi4_i2c_dev_release;

	tpd_load_status = 1;

	return 0;
}

 int synaptics_rmi4_i2c_remove(struct i2c_client *client)
{
	
	
	kfree(read_msg) ;
	if(gpwDMABuf_va){ 
		 dma_free_coherent(NULL, I2C_DMA_WBUF_SIZE, gpwDMABuf_va, gpwDMABuf_pa); 
		 gpwDMABuf_va = NULL; 
		 gpwDMABuf_pa = NULL; 
	}
	if(gprDMABuf_va){ 
		 dma_free_coherent(NULL, I2C_DMA_RBUF_SIZE, gprDMABuf_va, gprDMABuf_pa); 
		 gprDMABuf_va = NULL; 
		 gprDMABuf_pa = NULL; 
	}
	return 0;
}

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("Synaptics DSX I2C Bus Support Module");
MODULE_LICENSE("GPL v2");
