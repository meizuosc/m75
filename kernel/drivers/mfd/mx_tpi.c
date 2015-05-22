/*
 * Touch key & led driver for meizu m65u
 *
 * Copyright (C) 2013 Meizu Technology Co.Ltd, Zhuhai, China
 * Author:		
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/mfd/core.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/fb.h>
#include <linux/gpio.h>
#include <linux/i2c-gpio.h>
#include <mach/mt_gpio.h>
#include <mach/eint.h>
#include <asm/mach-types.h>
#include <linux/firmware.h>
#include	<linux/mfd/mx_tpi.h>
#include <linux/dma-mapping.h>

#include <cust_eint.h>
#include <cust_gpio_usage.h>


#ifndef	CUST_EINT_MCU_INT_NUM

#define CUST_EINT_MCU_INT_NUM              1
#define CUST_EINT_MCU_INT_DEBOUNCE_CN      0
#define CUST_EINT_MCU_INT_TYPE							1//CUST_EINTF_TRIGGER_RISING
#define CUST_EINT_MCU_INT_DEBOUNCE_EN      0//CUST_EINT_DEBOUNCE_DISABLE

#define GPIO_MCU_EINT_PIN         (GPIO2 | 0x80000000)
#define GPIO_MCU_EINT_PIN_M_GPIO  GPIO_MODE_00
#define GPIO_MCU_EINT_PIN_M_CLK  GPIO_MODE_01
#define GPIO_MCU_EINT_PIN_M_MDEINT  GPIO_MODE_05
#define GPIO_MCU_EINT_PIN_M_EINT  GPIO_MCU_EINT_PIN_M_GPIO

#define GPIO_MCU_RESET_PIN         (GPIO131 | 0x80000000)
#define GPIO_MCU_RESET_PIN_M_GPIO  GPIO_MODE_00
#define GPIO_MCU_RESET_PIN_M_PWM  GPIO_MODE_05

#define GPIO_MCU_SLEEP_PIN         (GPIO195 | 0x80000000)
#define GPIO_MCU_SLEEP_PIN_M_GPIO  GPIO_MODE_00
#define GPIO_MCU_SLEEP_PIN_M_KCOL  GPIO_MODE_01

#endif

#define TPI_MX_FW 		"mx/mx_tpi3.bin"

#define 	RESET_COLD	1
#define	RESET_SOFT	0

//#define	VERIFY_CRC

//#define	TPI_FB_STATE_CHANGE_USED

static volatile int force_update = false;
static volatile int is_update = false;
static int key_wakeup_type = 0;
static u8 * tpi_i2cdmabuf_va = NULL;
static dma_addr_t tpi_i2cdmabuf_pa = 0;
#define	TPI_I2CDMABUF_SIZE	(128)

struct mx_tpi_reg_data {
	unsigned char addr;
	unsigned char value;
};

//initial registers
const struct mx_tpi_reg_data init_regs[] = {
	{TPI_REG_STATUS, TPI_STATE_NORMAL},
	//{LED_REG_PWM, 0x7F},
//	{TPI_REG_WAKEUP_TYPE, 0x02},// µ¥»÷
};
	
static struct mfd_cell mx_tpi_devs[] = {
	{ .name = "mx-tpi-led", .id = 0 },
	{ .name = "mx-tpi-key", .id = 1 },
};

static int mx_tpi_getimgfwversion(struct mx_tpi_data *mx);
static int mx_tpi_update(struct mx_tpi_data *mx);
static void mx_tpi_reset(struct mx_tpi_data *mx,int m);
static void mx_tpi_wakeup_mcu(struct mx_tpi_data *mx,int bEn);

static int tpi_fb_state_change(struct notifier_block *nb,unsigned long val, void *data);


static int mx_tpi_write(struct i2c_client *client, const uint8_t *buf, int len)
{
	if(tpi_i2cdmabuf_va)
		memcpy(tpi_i2cdmabuf_va,buf,len);

	if(len < 8)
	{
		client->addr= (client->addr & I2C_MASK_FLAG) | I2C_ENEXT_FLAG;
		return i2c_master_send(client, buf, len);
	}
	else
	{
		client->addr = (client->addr & I2C_MASK_FLAG) | I2C_DMA_FLAG | I2C_ENEXT_FLAG;
		return i2c_master_send(client, (unsigned char*)tpi_i2cdmabuf_pa, len);
	}    
}

static int mx_tpi_read(struct i2c_client *client, uint8_t *buf, int len)
{
    int ret = 0;
    
    if(len < 8)
    {
        client->addr = (client->addr & I2C_MASK_FLAG) | I2C_ENEXT_FLAG;
        return i2c_master_recv(client, buf, len);
    }
    else
    {
        client->addr = (client->addr & I2C_MASK_FLAG) | I2C_DMA_FLAG | I2C_ENEXT_FLAG;
        ret = i2c_master_recv(client, (unsigned char*)tpi_i2cdmabuf_pa, len);
    
        if(ret < 0)
        {
            return ret;
        }
		
	if(tpi_i2cdmabuf_va)
		memcpy(buf,tpi_i2cdmabuf_va,len);
    }
    return ret;
}

static int mx_tpi_readbyte(struct i2c_client *client, u8 reg)
{
	struct mx_tpi_data *mx = i2c_get_clientdata(client);
	int ret = 0;
	u8 data = 0xff;

	if( is_update )
		return -EBUSY;
	
	mutex_lock(&mx->iolock);	

	ret = mx_tpi_write(client,&reg,1);
	if (ret < 0)
		dev_err(&client->dev,
			"can not write register, returned %d at line %d\n", ret,__LINE__);

	msleep(1);
	
	ret = mx_tpi_read(client,&data, 1);
	if (ret < 0)
		dev_err(&client->dev,
			"can not read register, returned %d at line %d\n", ret,__LINE__);
	else
		ret = data;

	dev_dbg(&client->dev,"%s: R = 0x%X  D = 0x%X\n", __func__,reg,data);
	
	mutex_unlock(&mx->iolock);

	return ret;
}

static int mx_tpi_writebyte(struct i2c_client *client, u8 reg, u8 data)
{
	struct mx_tpi_data *mx = i2c_get_clientdata(client);
	int ret = 0;	
	unsigned char buf[2];

	if( is_update )
		return -EBUSY;

	mutex_lock(&mx->iolock);

	buf[0] = reg;
	buf[1] = data;
	ret = mx_tpi_write(client,buf,2);
	if (ret < 0)
		dev_err(&client->dev,
			"can not write register, returned %d at line %d\n", ret,__LINE__);

	mutex_unlock(&mx->iolock);

	return ret;
}

static int mx_tpi_readdata(struct i2c_client *client, u8 reg,int bytes,void *dest)
{
	struct mx_tpi_data *mx = i2c_get_clientdata(client);
	int ret;
	unsigned char * buf;

	if( is_update )
		return -EBUSY;

	mutex_lock(&mx->iolock);

	buf = (unsigned char *)dest;
	*buf = reg;
	ret = mx_tpi_read(client,buf, bytes);
	if (ret < 0)
		dev_err(&client->dev,
			"can not read register, returned %d\n", ret);

	mutex_unlock(&mx->iolock);

	return ret;
}

static int mx_tpi_writedata(struct i2c_client *client, u8 reg, int bytes, const void *src)
{
	struct mx_tpi_data *mx = i2c_get_clientdata(client);
	int ret;	
	unsigned char buf[256];
	int size;

	if( is_update )
		return -EBUSY;

	mutex_lock(&mx->iolock);
	if(bytes > 255 )
		bytes = 255;

	if( (reg == TPI_REG_STATUS))
		dev_err(&client->dev,
			"TPI_STATE_SLEEP %d\n", TPI_STATE_SLEEP);

	buf[0] = reg;
	memcpy(&buf[1],src,bytes);

	size = bytes + 1;
	ret = mx_tpi_write(client,buf,size);
	if (ret < 0)
		dev_err(&client->dev,
			"can not write register, returned %d\n", ret);


	mutex_unlock(&mx->iolock);

	return ret;
}

static void mx_tpi_init_registers(struct mx_tpi_data *mx)
{
	int i, ret;
	unsigned char buf[2];

	for (i=0; i<ARRAY_SIZE(init_regs); i++) {		
		buf[0] = init_regs[i].addr;
		buf[1] = init_regs[i].value;
		ret = mx_tpi_write(mx->client,buf,2);
		if (ret < 0) {
			dev_err(mx->dev, "failed to init reg[%d/%d], ret = %d\n", i, ARRAY_SIZE(init_regs),ret);
		}
	}
}

static bool  mx_tpi_identify(struct mx_tpi_data *mx)
{
	int id, ver,img_ver;
	struct i2c_client *client = mx->client;

	/* Read Chip ID */
	id = mx_tpi_readbyte(client, TPI_REG_DEVICE_ID);
	if (id != MX_TPI_DEVICE_ID) {
		dev_err(&client->dev, "ID %d not supported\n", id);
		goto upd_ext;
	}

	dev_err(&client->dev, "ID %c \n", id);

	/* Read firmware version */
	ver = mx_tpi_readbyte(client, TPI_REG_VERSION);
	if (ver < 0) {
		dev_err(&client->dev, "could not read the firmware version\n");
		goto upd_ext;
	}

	img_ver = mx_tpi_getimgfwversion(mx);
	if( img_ver == 0xFF )
		img_ver = FW_VERSION;
		
	if( ver < img_ver)
	{
		dev_err(&client->dev, "Old firmware version %d.%d , img ver %d.%d,need be update\n", ((ver>>4)&0x0F),(ver&0x0F), ((img_ver>>4)&0x0F),(img_ver&0x0F));
upd_ext:		
		mx_tpi_update(mx);
		mx_tpi_wakeup_mcu(mx,true);
		
		/* Read Chip ID */
		id = mx_tpi_readbyte(client, TPI_REG_DEVICE_ID);
		if (id != MX_TPI_DEVICE_ID) {
			dev_err(&client->dev, "ID %d not supported\n", id);
			return false;
		}
		
		/* Read firmware version again*/
		ver = mx_tpi_readbyte(client, TPI_REG_VERSION);
		if (ver < 0) {
			dev_err(&client->dev, "could not read the firmware version\n");
			return false;
		}
	}

	mx->AVer = ver;

	/* Read led ID */
	//mx->LedVer = mx_tpi_readbyte(client, TPI_REG_LEDVERSION);

	dev_err(&client->dev, "mx tpi3 id 0x%.2X firmware version %d.%d \n", id,((ver>>4)&0x0F),(ver&0x0F));

	return true;
}

/*This function forces a recalibration of touch sensor.*/
static void mx_tpi_recalibration(struct mx_tpi_data *mx)
{	
	unsigned char msg[2];
	msg[0] = TPI_REG_CONTROL;
	msg[1] = 1<<1;			
	mx_tpi_write(mx->client,msg,2);
	
	dev_dbg(&mx->client->dev, "force a recalibration\n");
}

static void mx_tpi_reset(struct mx_tpi_data *mx,int m)
{
	if(m)
	{	
		mt_set_gpio_pull_enable(mx->gpio_reset,1);
		mt_set_gpio_dir(mx->gpio_reset, GPIO_DIR_OUT);
		mt_set_gpio_out(mx->gpio_reset, GPIO_OUT_ZERO);
		msleep(100);
		mt_set_gpio_out(mx->gpio_reset, GPIO_OUT_ZERO);
		mt_set_gpio_dir(mx->gpio_reset, GPIO_DIR_IN);	
	}
	else
	{
		unsigned char msg[2];
		msg[0] = TPI_REG_CONTROL;
		msg[1] = 1<<0;			
		mx_tpi_write(mx->client,msg,2);
	}
	key_wakeup_type = 0;
	dev_info(&mx->client->dev, "%s reset. \n", m?"cold":"soft");
}


static void mx_tpi_wakeup_mcu(struct mx_tpi_data *mx,int bEn)
{
	if( bEn )
	{
		mt_set_gpio_pull_enable(mx->gpio_wake,1);
		mt_set_gpio_dir(mx->gpio_wake, GPIO_DIR_OUT);
		mt_set_gpio_out(mx->gpio_wake, GPIO_OUT_ZERO);
	}
	else
	{	
		mt_get_gpio_pull_enable(mx->gpio_wake);
		mt_set_gpio_dir(mx->gpio_wake, GPIO_DIR_IN);
	}	

	msleep(50);

	dev_dbg(&mx->client->dev, "wakeup mcu %s\n",bEn?"enable":"disable");
}

static int mx_tpi_getimgfwversion(struct mx_tpi_data *mx)
{
	int err = 0;
	const struct firmware *fw;
	const char *fw_name;
	int img_fwversion;
	s_device_info * pDevice_info;
	
	fw_name = TPI_MX_FW;

	err = request_firmware(&fw, fw_name,  &mx->client->dev);
	if (err) {
		printk(KERN_ERR "Failed to load firmware \"%s\"\n", fw_name);
		return err;
	}

	pDevice_info = (s_device_info *)(fw->data+fw->size-sizeof(s_device_info));
	memcpy(&mx->fw_info,pDevice_info,sizeof(s_device_info));
	dev_err(&mx->client->dev,"firmware:id = %c , ver = %d, crc = 0x%.4X .\n",mx->fw_info.id,mx->fw_info.ver,mx->fw_info.crc);
	img_fwversion = mx->fw_info.ver;
	
	release_firmware(fw);
	
	return img_fwversion;
}

#define TWI_CMD_BOOT  	 'D'
#define TWI_CMD_UPD  	 'U'
#define TWI_CMD_END  	 'E'
#define TWI_CMD_CRC  	 'C'//get the flash crc
#define TWI_CMD_BVER  	 'B'//get the bootloader revision
#define	PAGESIZE		64
static int mx_tpi_update(struct mx_tpi_data *mx)
{
	const struct firmware *fw;
	const char *fw_name;
	s_device_info * pDevice_info;

	int ret = 0;

	unsigned char cmd;
	int i,cnt,size,try_cnt;
	unsigned char buf[PAGESIZE+3];
	const unsigned char * ptr;
	unsigned short crc1;
	unsigned short crc2;
	char boot_ver;

	wake_lock(&mx->wake_lock);
		
	is_update = true;
	
	disable_irq(mx->irq);	
	msleep(100);

	mx_tpi_wakeup_mcu(mx,true);		// into bootloader
	mx_tpi_reset(mx,RESET_COLD);
	mx_tpi_wakeup_mcu(mx,true);		// into bootloader
	
	msleep(50);

	cmd = TWI_CMD_BOOT;
	ret = mx_tpi_write(mx->client,&cmd,1);
	if(ret < 0 )
	{
		dev_err(&mx->client->dev,"can not write register, returned %d at line %d\n", ret,__LINE__);
		goto err_exit10;

	}
	dev_err(&mx->client->dev,"mx tpi sensor updating ... \n");
	msleep(10);

	cmd = TWI_CMD_BVER; //
	ret = 0;
	
	ret = mx_tpi_write(mx->client,&cmd,1);
	if (ret < 0)
		dev_err(&mx->client->dev,
			"can not write register, returned %d at line %d\n", ret,__LINE__);
	
	ret = mx_tpi_read(mx->client,&boot_ver, 1);
	if (ret < 0)
		dev_err(&mx->client->dev,
			"can not read register, returned %d at line %d\n", ret,__LINE__);
	
	if(ret < 0 )
	{
		dev_err(&mx->client->dev,"can not read the bootloader revision, returned %d at line %d\n", ret,__LINE__);
		goto err_exit10;
	}
	
	mx->BVer = boot_ver;
	
	dev_err(&mx->client->dev,"bootloader revision %d.%d\n", ((boot_ver>>4)&0x0F),(boot_ver&0x0F));	
	
	fw_name = TPI_MX_FW;	
	ret = request_firmware(&fw, fw_name,  &mx->client->dev);
	if (ret) {
		dev_err(&mx->client->dev,"Failed to load firmware \"%s\"\n", fw_name);
		goto err_exit;
	}

	try_cnt = 3;
	size = fw->size;	// 1024*7
	ptr = fw->data;
	dev_err(&mx->client->dev,"load firmware %s size %d\n",fw_name,fw->size);
	pDevice_info = (s_device_info *)(fw->data+fw->size-sizeof(s_device_info));
	memcpy(&mx->fw_info,pDevice_info,sizeof(s_device_info));
	dev_err(&mx->client->dev,"firmware:id = %c,ver.%d,crc = 0x%.4X\n",mx->fw_info.id,mx->fw_info.ver,mx->fw_info.crc);
	do
	{
		buf[0] = TWI_CMD_UPD;
		for(i=0;i<size;i+=PAGESIZE)
		{
			buf[1] = i & 0xFF;
			buf[2] = (i>>8) & 0xFF;

			memcpy(&buf[3],ptr+i,PAGESIZE);

			cnt = 3;

			do
			{
				ret = mx_tpi_write(mx->client,buf,sizeof(buf));
			}while(ret < 0 && cnt--);
			
			if(ret < 0 )
				dev_err(&mx->client->dev,"can not write register, returned %d at addres 0x%.4X(page %d)\n", ret,i,(i/PAGESIZE+1));
			else
				dev_info(&mx->client->dev,"update page %d\n", ((i/PAGESIZE)+1));
		}	
		
		if(ret < 0 )
			goto err_exit;

		cmd = TWI_CMD_CRC;
		ret = mx_tpi_write(mx->client,&cmd,1);
		msleep(100);
		ret = mx_tpi_read(mx->client,(unsigned char *)&crc1,1);
		crc1 <<= 8;
		ret = mx_tpi_read(mx->client,(unsigned char *)&crc1,1);
		//crc2  = *(unsigned short *)(fw->data+fw->size-2);
		crc2  = pDevice_info->crc;
		if( crc1 != crc2)
		{
			dev_err(&mx->client->dev,"crc check 0x%.4X (0x%.4X) failed !!!\n", crc1,crc2);
		}
		else
		{
			//dev_err(&mx->client->dev,"crc check 0x%.4X (0x%.4X)\n", crc1,crc2);
			dev_err(&mx->client->dev,"Verifying flash OK!\n");
			break;
		}		
	}while(try_cnt--);
	
	cmd = TWI_CMD_END;
	ret = mx_tpi_write(mx->client,&cmd,1);
	if(ret < 0 )
	{
		dev_err(&mx->client->dev,"can not write register, returned %d at line %d\n", ret,__LINE__);
		goto err_exit;

	}
	
	dev_err(&mx->client->dev, "Update completed. \n");
	release_firmware(fw);
	goto exit;
	
err_exit:	
	release_firmware(fw);
err_exit10:	
	dev_err(&mx->client->dev, "Update failed !! \n");
exit:
	mx_tpi_wakeup_mcu(mx,false); // Into app
	mx_tpi_reset(mx,RESET_COLD); 
	msleep(250);	
	mx_tpi_wakeup_mcu(mx,false);
	msleep(250);	

	is_update = false;

	/*initial registers*/
	if(!ret)
		mx_tpi_init_registers(mx);
	
	enable_irq(mx->irq);
	
	wake_unlock(&mx->wake_lock);
	return ret;
}


#define	LINK_KOBJ_NAME	"mx_tpi"
static struct kobject *mx_tpi_devices_kobj = NULL;
/**
 * mx_create_link - create a sysfs link to an exported virtual node
 *	@target:	object we're pointing to.
 *	@name:		name of the symlink.
 *
 * Set up a symlink from /sys/class/input/inputX to 
 * /sys/devices/mx_tsp node. 
 *
 * Returns zero on success, else an error.
 */
static int mx_tpi_create_link(struct kobject *target, const char *name)
{
	int rc = 0;
	
	struct device *device_handle = NULL;
	struct kset *pdevices_kset;
	
	device_handle = kzalloc(sizeof(*device_handle), GFP_KERNEL);
	if (!device_handle){
		rc = -ENOMEM;
		return rc;
	}
	
	device_initialize(device_handle);
	pdevices_kset = device_handle->kobj.kset;
	mx_tpi_devices_kobj = &pdevices_kset->kobj;
	kfree(device_handle);	
	
	if( !mx_tpi_devices_kobj )
	{
		rc = -EINVAL;
		goto err_exit;
	}
	
	rc = sysfs_create_link(mx_tpi_devices_kobj,target, name);
	if(rc < 0)
	{
		pr_err("sysfs_create_link failed.\n");
		goto err_exit;
	}

	return rc;
	
err_exit:
	mx_tpi_devices_kobj = NULL;
	pr_err("mx_create_link failed %d \n",rc);
	return rc;
}
	 
static void mx_tpi_remove_link(const char *name)
{
 	if( mx_tpi_devices_kobj )
	{
		sysfs_remove_link(mx_tpi_devices_kobj, name);
		mx_tpi_devices_kobj = NULL;
	}
}


static ssize_t tpi_show_property(struct device *dev,
                                      struct device_attribute *attr,
                                      char *buf);
static ssize_t tpi_store(struct device *dev, 
			     struct device_attribute *attr,
			     const char *buf, size_t count);

#define TPI_ATTR(_name)\
{\
    .attr = { .name = #_name, .mode = S_IRUGO | S_IWUSR},\
    .show = tpi_show_property,\
    .store = tpi_store,\
}

static struct device_attribute tpi_attrs[] = {
    TPI_ATTR(status),
    TPI_ATTR(cmd),
    TPI_ATTR(reset),
    TPI_ATTR(calibrate),
    TPI_ATTR(version),
    TPI_ATTR(led),
    TPI_ATTR(gestrue),
    TPI_ATTR(update),
    TPI_ATTR(key_wakeup_count),
    TPI_ATTR(key_wakeup_type),
    TPI_ATTR(wakeup),
    TPI_ATTR(irq),
};
enum {
	TPI_STATUS,
	TPI_CMD,
	TPI_RESET,
	TPI_CAL,
	TPI_FWR_VER,
	TPI_LED,
	TPI_GESTRUE,
	TPI_UPD,
	TPI_CNT,
	TPI_WAKEUP_TYPE,
	TPI_WAKEUP,
	TPI_IRQ,
};
static ssize_t tpi_show_property(struct device *dev,
                                      struct device_attribute *attr,
                                      char *buf)
{
	int i = 0;
	ptrdiff_t off;
	struct mx_tpi_data *tpi_data = (struct mx_tpi_data*)dev_get_drvdata(dev);
	
	if(!tpi_data)
	{
		pr_err("%s(): failed!!!\n", __func__);
		return -ENODEV;
	}

	off = attr - tpi_attrs;

	switch(off){
	case TPI_STATUS:
		i += scnprintf(buf+i, PAGE_SIZE-i, "%d\n",mx_tpi_readbyte(tpi_data->client,TPI_REG_STATUS));
		break;
	case TPI_CMD:
		{
			int ret,val;

			val = 0x00;
			ret = mx_tpi_read(tpi_data->client,(unsigned char *)&val,1);
			if (ret < 0)
				pr_err("mx_tpi_readbyte error at %d line\n", __LINE__);
			i += scnprintf(buf+i, PAGE_SIZE-i, "0x%.2X\n",val);
		}
		break;
	case TPI_FWR_VER:
		{
			int ver,id;
			id = mx_tpi_readbyte(tpi_data->client,TPI_REG_DEVICE_ID);
			ver = mx_tpi_readbyte(tpi_data->client,TPI_REG_VERSION);
			i += scnprintf(buf+i, PAGE_SIZE-i, "ID = 0x%.2X(%c) ,Ver.0x%X\n",id,id,ver);
		}
		break;
	case TPI_RESET:
		i += scnprintf(buf+i, PAGE_SIZE-i, "\n");
		break;
	case TPI_CAL:
		mx_tpi_recalibration(tpi_data);
		i += scnprintf(buf+i, PAGE_SIZE-i, "Sensor recalibration. \n");
		break;
	case TPI_LED:
		i += scnprintf(buf+i, PAGE_SIZE-i, "func:0x%.2X\n",mx_tpi_readbyte(tpi_data->client,LED_REG_EN));
		break;
	case TPI_GESTRUE:
		i += scnprintf(buf+i, PAGE_SIZE-i, "func:0x%.2X\n",mx_tpi_readbyte(tpi_data->client,KEY_REG_EN));
		break;
	case TPI_UPD:
		mx_tpi_update(tpi_data);
		i += scnprintf(buf+i, PAGE_SIZE-i, "\n");
		break;
	case TPI_CNT:
		i += scnprintf(buf+i, PAGE_SIZE-i, "%d\n",mx_tpi_readbyte(tpi_data->client,KEY_REG_WAKEUP_CNT));
		break;
	case TPI_WAKEUP_TYPE:
		i += scnprintf(buf+i, PAGE_SIZE-i, "%d\n",key_wakeup_type);
		break;
	case TPI_WAKEUP:
		i += scnprintf(buf+i, PAGE_SIZE-i, "Error\n");
		break;
	case TPI_IRQ:
		i += scnprintf(buf+i, PAGE_SIZE-i, "Error\n");
		break;
	default:
		i += scnprintf(buf+i, PAGE_SIZE-i, "Error\n");
		break;	
	}
	return i;
}

static ssize_t tpi_store(struct device *dev, 
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	unsigned int reg,value;
	int ret = 0;
	ptrdiff_t off;
	struct mx_tpi_data *tpi_data = (struct mx_tpi_data*)dev_get_drvdata(dev);
	
	if(!tpi_data)
	{
		pr_err("%s(): failed!!!\n", __func__);
		return -ENODEV;
	}

	off = attr - tpi_attrs;

	switch(off){
	case TPI_STATUS:
		if (sscanf(buf, "%d\n", &value) == 1) {	
			ret = mx_tpi_writebyte(tpi_data->client,TPI_REG_STATUS,value);
			if (ret < 0)
				pr_err("mx_tpi_writebyte error at %d line\n", __LINE__);			
		}
		ret = count;
		break;
	case TPI_CMD:
		if (sscanf(buf, "%x %x", &reg, &value) == 2) {
			int ret;
			unsigned char msg[2];
			msg[0] = reg;
			msg[1] = value;			
			dev_info(dev, "R:0x%.2X V:0x%.2X \n", reg,value);
			ret = mx_tpi_write(tpi_data->client,msg,2);
			if (ret < 0)
				pr_err("mx_tpi_writebyte error at %d line\n", __LINE__);
		}
		else if (sscanf(buf, "%x\n", &value) == 1) {	
			int ret;
			unsigned char msg[2];
			msg[0] = value;			
			dev_info(dev, "V:0x%.2X \n", value);
			ret = mx_tpi_write(tpi_data->client,msg,1);
			if (ret < 0)
				pr_err("mx_tpi_writebyte error at %d line\n", __LINE__);
			
		}
		else {			
			pr_err("%s(): failed !!!\n", __func__);
		}
		ret = count;
		break;
	case TPI_RESET:
		if (sscanf(buf, "%x\n", &value) == 1) {	

			mx_tpi_reset(tpi_data,value);
			pr_info("Sensor %s reset. \n", value?"cold":"soft");
		}
		ret = count;
		break;
	case TPI_CAL:
		mx_tpi_recalibration(tpi_data);
		pr_info("Sensor recalibration. \n");
		ret = count;
		break;
	case TPI_FWR_VER:
		ret = count;
		break;
	case TPI_LED:
		if (sscanf(buf, "%x",  &value) == 1) {
			ret = mx_tpi_writebyte(tpi_data->client,LED_REG_EN,value);
			pr_info("%s led function. \n", value?"enable":"disable");
		}
		ret = count;
		break;
	case TPI_GESTRUE:
		if (sscanf(buf, "%x",  &value) == 1) {
			ret = mx_tpi_writebyte(tpi_data->client,KEY_REG_EN,value);
			pr_info("%s gesture function. \n", value?"enable":"disable");
		}
		ret = count;
		break;
	case TPI_UPD:
		mx_tpi_update(tpi_data);
		ret = count;
		break;
	case TPI_CNT:
		ret = count;
		break;
	case TPI_WAKEUP_TYPE:
		if (sscanf(buf, "%d\n", &value) == 1) {	
			int ret;
			dev_info(dev, "V:0x%.2X \n", value);
			if( value < 4 )
			{
				ret = mx_tpi_writebyte(tpi_data->client,KEY_REG_WAKEUP_TYPE,value);
				if (ret < 0)
					pr_err("mx_tpi_writebyte error at %d line\n", __LINE__);
				else
					key_wakeup_type = value;
			}		
			else
			{
				pr_err("Invalid argument\n");
			}
		}
		else {			
			pr_err("%s(): failed !!!\n", __func__);
		}
		ret = count;
		break;
	case TPI_WAKEUP:
		if (sscanf(buf, "%x\n", &value) == 1) {	
			if( value )
				tpi_data->wakeup_mcu(tpi_data,true);
			else
				tpi_data->wakeup_mcu(tpi_data,false);
			pr_info("%s gesture function. \n", value?"wake up":"sleep");
		}
		ret = count;
		break;
	case TPI_IRQ:
		if (sscanf(buf, "%x\n", &value) == 1) {	
			if( value )
				enable_irq(tpi_data->irq);
			else
				disable_irq(tpi_data->irq);
			pr_info("%s irq. \n", value?"enable":"disable");
		}
		ret = count;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;	
}

static int tpi_create_attrs(struct device * dev)
{
	int i, rc;
	rc = mx_tpi_create_link(&dev->kobj,LINK_KOBJ_NAME);
	if (rc  < 0 )
		goto exit_sysfs_create_link_failed;

	for (i = 0; i < ARRAY_SIZE(tpi_attrs); i++) {
		rc = device_create_file(dev, &tpi_attrs[i]);
		if (rc)
			goto tpi_attrs_failed;
	}
	goto succeed;

tpi_attrs_failed:
	printk(KERN_INFO "%s(): failed!!!\n", __func__);	
	while (i--)
		device_remove_file(dev, &tpi_attrs[i]);
exit_sysfs_create_link_failed:
	mx_tpi_remove_link(LINK_KOBJ_NAME);
succeed:		
	return rc;

}

static void tpi_destroy_atts(struct device * dev)
{
	int i;
	
	for (i = 0; i < ARRAY_SIZE(tpi_attrs); i++)
		device_remove_file(dev, &tpi_attrs[i]);
	
	mx_tpi_remove_link(LINK_KOBJ_NAME);
}

struct mx_tpi_data * gpmx_tpi = NULL;
int mx_tpi_setslopeperiod(u16 period)
{
	int ret = 0;
	
	if( !gpmx_tpi )
	{
		pr_err("%s, handle is NULL\n", __func__);
		return -EXDEV;
	}

	ret = mx_tpi_writedata(gpmx_tpi->client,LED_REG_SLPPRD,2,(u8 *)&period);

	return ret;
}
EXPORT_SYMBOL(mx_tpi_setslopeperiod);


int mx_tpi_setbrightness(u8 data)
{
	int ret = 0;
	
	if( !gpmx_tpi )
	{
		pr_err("%s, handle is NULL\n", __func__);
		return -EXDEV;
	}

	ret = mx_tpi_writebyte(gpmx_tpi->client,LED_REG_PWM,data);
	if(ret < 0)
	{
		pr_err("%s, mx_tpi_writebyte error %d\n", __func__,ret);
	}
	
	return ret;
}
EXPORT_SYMBOL(mx_tpi_setbrightness);

int mx_tpi_setslope(u8 data)
{
	int ret = 0;
	
	if( !gpmx_tpi )
	{
		pr_err("%s, handle is NULL\n", __func__);
		return -EXDEV;
	}

	ret = mx_tpi_writebyte(gpmx_tpi->client,LED_REG_SLOPE,data);
	if(ret < 0)
	{
		pr_err("%s, mx_tpi_writebyte error %d\n", __func__,ret);
	}

	return ret;
}
EXPORT_SYMBOL(mx_tpi_setslope);

int mx_tpi_trigger_fadeonoff(void )
{
	int ret = 0;
	if( !gpmx_tpi )
	{
		pr_err("%s, handle is NULL\n", __func__);
		return -EXDEV;
	}

	ret = mx_tpi_writebyte(gpmx_tpi->client,LED_REG_FADE,0x01);
	if(ret < 0)
	{
		pr_err("%s, mx_tpi_writebyte error %d\n", __func__,ret);
	}
	
	printk("%s\n", __func__);

	return ret;
}
EXPORT_SYMBOL(mx_tpi_trigger_fadeonoff);

int mx_tpi_wake_up(void )
{
	if( !gpmx_tpi )
	{
		pr_err("%s, handle is NULL\n", __func__);
		return -EXDEV;
	}

	mx_tpi_wakeup_mcu(gpmx_tpi,false);

	return 0;
}
EXPORT_SYMBOL(mx_tpi_wake_up);

int mx_tpi_setkeytype(struct mx_tpi_data *data,u8 type)
{
	int ret = 0;

	if(type >= MX_KEY_TYPE_MAX)
		return -1;
	
	ret = mx_tpi_writebyte(data->client,KEY_REG_WAKEUP_TYPE,type);
	if (ret < 0)
		pr_err("mx_tpi_writebyte error at %d line\n", __LINE__);
	else
		data->key_wakeup_type = type;
	
	pr_info("%s: %d \n", __func__,type);
	return ret;
}
EXPORT_SYMBOL(mx_tpi_setkeytype);

static int tpi_fb_state_change(struct notifier_block *nb,
		unsigned long val, void *data)
{
	struct mx_tpi_data *tpi_data = container_of(nb,
					struct mx_tpi_data, notifier);
	struct fb_event *evdata = data;
	unsigned int blank;
	
	dev_info(&tpi_data->client->dev, "%s: \n",__func__);

	if (val != FB_EVENT_BLANK)
		return 0;

	blank = *(int *)evdata->data;
	
	if(tpi_data->debug) 
		dev_info(&tpi_data->client->dev, "%s: blank (%d).\n",__func__,blank);

	switch (blank) {
	case FB_BLANK_POWERDOWN:
		tpi_data->wakeup_mcu(tpi_data,true);
		break;

	case FB_BLANK_UNBLANK:
		tpi_data->wakeup_mcu(tpi_data,false);
		break;
		
	default:
		break;
	}
	return NOTIFY_OK;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
 static void mx_tpi_early_suspend(struct early_suspend *h)
 {
	 struct mx_tpi_data *tpi =
			 container_of(h, struct mx_tpi_data, early_suspend);

	 mx_tpi_writebyte(tpi->client,KEY_REG_EN,1);
	 //tpi->i2c_writebyte(tpi->client, TPI_REG_STATUS,TPI_STATE_NORMAL);

	//mx_tpi_reset(tpi,0);// Soft reset sensor
	//mx_tpi_recalibration(tpi);// ReCalibration sensor
	tpi->wakeup_mcu(tpi,true);	
 }
 
 static void mx_tpi_late_resume(struct early_suspend *h)
 {
	 struct mx_tpi_data *tpi =
			 container_of(h, struct mx_tpi_data, early_suspend);

	tpi->wakeup_mcu(tpi,false);
	//mx_tpi_writebyte(tpi->client,KEY_REG_EN,0);
	//tpi->i2c_writebyte(tpi->client, TPI_REG_STATUS,TPI_STATE_IDLE);
 }
#endif

static int mx_tpi_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct mx_tpi_platform_data *pdata = client->dev.platform_data;
	struct mx_tpi_data *data;
	int err = 0;

	dev_info(&client->dev,"%s:++\n",__func__);
	
	client->timing = 200;
	
	if (!i2c_check_functionality(client->adapter,I2C_FUNC_I2C)) {
		dev_err(&client->dev, "i2c not Supported\n");
		return -EIO;
	}

	mt_set_gpio_mode(pdata->gpio_irq,GPIO_MCU_EINT_PIN_M_EINT);
	mt_set_gpio_pull_enable(pdata->gpio_irq,1);
	mt_set_gpio_pull_select(pdata->gpio_irq, GPIO_PULL_UP);
	mt_set_gpio_dir(pdata->gpio_irq, GPIO_DIR_IN);
	
	gpio_request(GPIO_MCU_SLEEP_PIN, "TPI WAK");
	
	mt_set_gpio_mode(pdata->gpio_wake,GPIO_MCU_SLEEP_PIN_M_GPIO);
	mt_set_gpio_pull_enable(pdata->gpio_wake,1);
	mt_set_gpio_pull_select(pdata->gpio_wake, GPIO_PULL_UP);
	mt_set_gpio_dir(pdata->gpio_wake, GPIO_DIR_IN);
	
	mt_set_gpio_pull_enable(pdata->gpio_reset,1);
	mt_set_gpio_pull_select(pdata->gpio_reset, GPIO_PULL_UP);
	mt_set_gpio_dir(pdata->gpio_reset, GPIO_DIR_OUT);
	mt_set_gpio_out(pdata->gpio_reset, GPIO_OUT_ZERO);
	msleep(50);
	mt_set_gpio_out(pdata->gpio_reset, GPIO_OUT_ONE);
	mt_set_gpio_dir(pdata->gpio_reset, GPIO_DIR_IN);	

	msleep(250);	

	data = kzalloc(sizeof(struct mx_tpi_data), GFP_KERNEL);
	data->dev = &client->dev;
	
	data->gpio_wake = pdata->gpio_wake;
	data->gpio_reset = pdata->gpio_reset;
	data->gpio_irq = pdata->gpio_irq;

	data->client = client;
	data->irq = CUST_EINT_MCU_INT_NUM;//client->irq;//__gpio_to_irq(data->gpio_irq);//
	data->i2c_readbyte = mx_tpi_readbyte;
	data->i2c_writebyte= mx_tpi_writebyte;
	data->i2c_readbuf = mx_tpi_readdata;
	data->i2c_writebuf = mx_tpi_writedata;
	data->reset = mx_tpi_reset;
	data->wakeup_mcu= mx_tpi_wakeup_mcu;
	data->update= mx_tpi_update;	
		
	i2c_set_clientdata(client, data);
	mutex_init(&data->iolock);
	wake_lock_init(&data->wake_lock, WAKE_LOCK_SUSPEND, "mx_tpi3");	

	//client->dev.coherent_dma_mask	= DMA_BIT_MASK(64);	
	//tpi_i2cdmabuf_va = (u8 *)dma_alloc_coherent(&client->dev, TPI_I2CDMABUF_SIZE, &tpi_i2cdmabuf_pa, GFP_KERNEL);
	tpi_i2cdmabuf_va = (u8 *)dma_alloc_coherent(NULL, TPI_I2CDMABUF_SIZE, &tpi_i2cdmabuf_pa, GFP_KERNEL);
	if(!tpi_i2cdmabuf_va)
	{
		dev_err(&client->dev,"Allocate TPI DMA I2C Buffer failed!\n");
		goto err_free_mem;
	}

#if 1	
	if( force_update )
	{
		dev_info(&client->dev,"mx_qm:force update...\n");
		mx_tpi_update(data);
	}
	
	mx_tpi_wakeup_mcu(data,false);
	/* Identify the mx_qm chip */
	if (!mx_tpi_identify(data))
	{
		mx_tpi_update(data);
		if (!mx_tpi_identify(data))
		{
			err = -ENODEV;	
			goto err_free_mem;	
		}
	}
#endif

	mx_tpi_wakeup_mcu(data,false);

	tpi_create_attrs(&client->dev);
		
	/*initial registers*/
	mx_tpi_init_registers(data);

	err = mfd_add_devices(data->dev, -1, mx_tpi_devs,
			ARRAY_SIZE(mx_tpi_devs), NULL, 0,NULL);
	if (err) {
		dev_err(&client->dev,"MX TPI  mfd add devices failed\n");
		goto err_free_mem;
	}
	 
#ifdef	TPI_FB_STATE_CHANGE_USED	
	 data->notifier.notifier_call = tpi_fb_state_change;
	 data->notifier.priority = 1;
	 if (fb_register_client(&data->notifier))
		 pr_err("unable to register tpi_fb_state_change\n");
#endif	

#ifdef CONFIG_HAS_EARLYSUSPEND
	 data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	 data->early_suspend.suspend = mx_tpi_early_suspend;
	 data->early_suspend.resume = mx_tpi_late_resume;
	 register_early_suspend(&data->early_suspend);
#endif

	gpmx_tpi = data;

	dev_info(&client->dev,"%s:--\n",__func__);
	return 0;

err_free_mem:	
	if(!tpi_i2cdmabuf_va)
	{
		dma_free_coherent(NULL, TPI_I2CDMABUF_SIZE, tpi_i2cdmabuf_va, tpi_i2cdmabuf_pa);
		tpi_i2cdmabuf_va = NULL;
		tpi_i2cdmabuf_pa = 0;
	}
	dev_err(&client->dev,"%s:init failed! \n",__func__);
	mutex_destroy(&data->iolock);
	wake_lock_destroy(&data->wake_lock);
	i2c_set_clientdata(client, NULL);
	kfree(data);
	return err;
}

#ifdef CONFIG_PM
static int mx_tpi_suspend(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct mx_tpi_data *mx = i2c_get_clientdata(i2c);

	//disable_irq(mx->irq);
	//enable_irq_wake(mx->irq);
	
	//mx->wakeup_mcu(mx,true);

	return 0;
}

static int mx_tpi_resume(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct mx_tpi_data *mx = i2c_get_clientdata(i2c);
	

	//mx->wakeup_mcu(mx,false);

	//disable_irq_wake(mx->irq);
	//enable_irq(mx->irq);

	return 0;
}

#else
#define mx_tpi_suspend	NULL
#define mx_tpi_resume	NULL
#endif /* CONFIG_PM */

const struct dev_pm_ops mx_tpi_pm = {
	.suspend = mx_tpi_suspend,
	.resume = mx_tpi_resume,
};

static int mx_tpi_i2c_remove(struct i2c_client *client)
{
	struct mx_tpi_data *data = i2c_get_clientdata(client);
	
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&data->early_suspend);
#endif

	mutex_destroy(&data->iolock);
	wake_lock_destroy(&data->wake_lock);
	tpi_destroy_atts(&client->dev);
	
	kfree(data);

	i2c_set_clientdata(client, NULL);
	
	if(!tpi_i2cdmabuf_va)
	{
		dma_free_coherent(NULL, TPI_I2CDMABUF_SIZE, tpi_i2cdmabuf_va, tpi_i2cdmabuf_pa);
		tpi_i2cdmabuf_va = NULL;
		tpi_i2cdmabuf_pa = 0;
	}

	return 0;
}

void mx_tpi_i2c_shutdown(struct i2c_client *client)
{
	int ret = 0;

	ret = mx_tpi_writebyte(client,TPI_REG_STATUS,TPI_STATE_SHUTDOWN);
	if (ret < 0)
		pr_err("mx_tpi_writebyte error at %d line\n", __LINE__);
}

static const struct i2c_device_id mx_tpi_id[] = {
	{ TPI_I2C_DRIVER_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, mx_tpi_id);

static struct i2c_driver mx_tpi_driver = {
	.driver	= {
		.name	= TPI_I2C_DRIVER_NAME,
		.owner	= THIS_MODULE,
		.pm = &mx_tpi_pm,
	},
	.id_table	= mx_tpi_id,
	.probe		= mx_tpi_i2c_probe,
	.remove		= mx_tpi_i2c_remove,
	.shutdown	= mx_tpi_i2c_shutdown,
};

/*----------------------------------------------------------------------------*/
#define	I2C_TPI_BUS_ID_NUM 	3
static struct mx_tpi_platform_data __initdata mxtpi_pd = {
	.gpio_wake 	= GPIO_MCU_SLEEP_PIN,
	.gpio_reset 	= GPIO_MCU_RESET_PIN,
	.gpio_irq 	= GPIO_MCU_EINT_PIN,
};

static struct i2c_board_info __initdata i2c_devs_tpi3[] = {
	{
		I2C_BOARD_INFO(TPI_I2C_DRIVER_NAME, TPI_I2C_DRIVER_ADRESS),
		.platform_data	= &mxtpi_pd,
		.irq = CUST_EINT_MCU_INT_NUM,
	},
};

/*----------------------------------------------------------------------------*/
static int __init mx_tpi_i2c_init(void)
{
	int err;
	i2c_register_board_info(I2C_TPI_BUS_ID_NUM, i2c_devs_tpi3, ARRAY_SIZE(i2c_devs_tpi3));
	err = i2c_add_driver(&mx_tpi_driver);
	return 0;    
}
/*----------------------------------------------------------------------------*/
static void __exit mx_tpi_i2c_exit(void)
{
    i2c_del_driver(&mx_tpi_driver);
}
/*----------------------------------------------------------------------------*/
module_init(mx_tpi_i2c_init);
module_exit(mx_tpi_i2c_exit);
/*----------------------------------------------------------------------------*/
MODULE_AUTHOR("Chwei <Chwei@meizu.com>");
MODULE_DESCRIPTION("MX TPI3 Sensor");
MODULE_LICENSE("GPL");
