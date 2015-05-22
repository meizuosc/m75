/* 
 * Author: yucong xiong <yucong.xion@mediatek.com>
 *
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
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <asm/atomic.h>

#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>
#define POWER_NONE_MACRO MT65XX_POWER_NONE

#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include <asm/io.h>
#include <cust_eint.h>
#include <cust_alsps.h>
#include "jsa1127.h"
#include <linux/sched.h>
/******************************************************************************
 * configuration
*******************************************************************************/
/*----------------------------------------------------------------------------*/

#define JSA1127_DEV_NAME     "jsa1127"
/*----------------------------------------------------------------------------*/
#define APS_TAG                  "[ALS/PS] "
#define APS_FUN(f)               printk(KERN_INFO APS_TAG"%s\n", __FUNCTION__)
#define APS_ERR(fmt, args...)    printk(KERN_ERR  APS_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define APS_LOG(fmt, args...)    printk(KERN_ERR APS_TAG fmt, ##args)
#define APS_DBG(fmt, args...)    printk(KERN_INFO APS_TAG fmt, ##args)    

#define I2C_FLAG_WRITE	0
#define I2C_FLAG_READ	1
#define I2C_FLAG_READ_jsa1127  2

/******************************************************************************
 * extern functions
*******************************************************************************/
#ifdef CUST_EINT_ALS_TYPE
extern void mt_eint_mask(unsigned int eint_num);
extern void mt_eint_unmask(unsigned int eint_num);
extern void mt_eint_set_hw_debounce(unsigned int eint_num, unsigned int ms);
extern void mt_eint_set_polarity(unsigned int eint_num, unsigned int pol);
extern unsigned int mt_eint_set_sens(unsigned int eint_num, unsigned int sens);
extern void mt_eint_registration(unsigned int eint_num, unsigned int flow, void (EINT_FUNC_PTR)(void), unsigned int is_auto_umask);
extern void mt_eint_print_status(void);
#else
extern void mt65xx_eint_mask(unsigned int line);
extern void mt65xx_eint_unmask(unsigned int line);
extern void mt65xx_eint_set_hw_debounce(unsigned int eint_num, unsigned int ms);
extern void mt65xx_eint_set_polarity(unsigned int eint_num, unsigned int pol);
extern unsigned int mt65xx_eint_set_sens(unsigned int eint_num, unsigned int sens);
extern void mt65xx_eint_registration(unsigned int eint_num, unsigned int is_deb_en, unsigned int pol, void (EINT_FUNC_PTR)(void), unsigned int is_auto_umask);
#endif
/*----------------------------------------------------------------------------*/
static int jsa1127_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id); 
static int jsa1127_i2c_remove(struct i2c_client *client);
static int jsa1127_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);
static int jsa1127_i2c_suspend(struct i2c_client *client, pm_message_t msg);
static int jsa1127_i2c_resume(struct i2c_client *client);
/*----------------------------------------------------------------------------*/
static const struct i2c_device_id jsa1127_i2c_id[] = {{JSA1127_DEV_NAME,0},{}};
static struct i2c_board_info __initdata i2c_jsa1127={ I2C_BOARD_INFO(JSA1127_DEV_NAME, 0x39)};
/* static unsigned long long int_top_time = 0; */
/*----------------------------------------------------------------------------*/
struct jsa1127_priv {
	struct alsps_hw  *hw;
	struct i2c_client *client;
	struct work_struct	eint_work;
	/*misc*/
	u16 		als_modulus;
	atomic_t	i2c_retry;
	atomic_t	als_suspend;
	atomic_t	als_debounce;	/*debounce time after enabling als*/
	atomic_t	als_deb_on; 	/*indicates if the debounce is on*/
	atomic_t	als_deb_end;	/*the jiffies representing the end of debounce*/
	atomic_t 	trace;
	/*data*/
	u16			als; 
	u8			_align;
	u16			als_level_num;
	u16			als_value_num;
	u32			als_level[C_CUST_ALS_LEVEL-1];
	u32			als_value[C_CUST_ALS_LEVEL];
	atomic_t	als_cmd_val;	/*the cmd value can't be read, stored in ram*/
	atomic_t	als_thd_val_high;	 /*the cmd value can't be read, stored in ram*/
	atomic_t	als_thd_val_low; 	/*the cmd value can't be read, stored in ram*/
	ulong		enable; 		/*enable mask*/
	ulong		pending_intr;	/*pending interrupt*/
	/*early suspend*/
	#if defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend	early_drv;
	#endif     
};
/*----------------------------------------------------------------------------*/

static struct i2c_driver jsa1127_i2c_driver = {	
	.probe      = jsa1127_i2c_probe,
	.remove     = jsa1127_i2c_remove,
	.detect     = jsa1127_i2c_detect,
	.suspend    = jsa1127_i2c_suspend,
	.resume     = jsa1127_i2c_resume,
	.id_table   = jsa1127_i2c_id,
	.driver = {
		.name = JSA1127_DEV_NAME,
	},
};

/*----------------------------------------------------------------------------*/
struct PS_CALI_DATA_STRUCT
{
	int close;
	int far_away;
	int valid;
};

/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static struct i2c_client *jsa1127_i2c_client = NULL;
static struct jsa1127_priv *g_jsa1127_ptr = NULL;
static struct jsa1127_priv *jsa1127_obj = NULL;
static struct platform_driver jsa1127_alsps_driver;
/*----------------------------------------------------------------------------*/

static DEFINE_MUTEX(jsa1127_mutex);


/*----------------------------------------------------------------------------*/
typedef enum {
	CMC_BIT_ALS    = 1,
}CMC_BIT;
/*-----------------------------CMC for debugging-------------------------------*/
typedef enum {
    CMC_TRC_ALS_DATA= 0x0001,
    CMC_TRC_EINT    = 0x0004,
    CMC_TRC_IOCTL   = 0x0008,
    CMC_TRC_I2C     = 0x0010,
    CMC_TRC_CVT_ALS = 0x0020,
    CMC_TRC_DEBUG   = 0x8000,
} CMC_TRC;
/*-----------------------------------------------------------------------------*/
#if 0
static int jsa1127_enable_eint(struct i2c_client *client)
{
	//struct jsa1127_priv *obj = i2c_get_clientdata(client);        
	//g_jsa1127_ptr = obj;
	
	mt_set_gpio_mode(GPIO_ALS_EINT_PIN, GPIO_ALS_EINT_PIN_M_EINT);
	mt_set_gpio_dir(GPIO_ALS_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_ALS_EINT_PIN, TRUE);
	mt_set_gpio_pull_select(GPIO_ALS_EINT_PIN, GPIO_PULL_UP); 
	
	mt_eint_unmask(GPIO_ALS_EINT_PIN);
    return 0;
}
#endif

static int jsa1127_disable_eint(struct i2c_client *client)
{
	//struct jsa1127_priv *obj = i2c_get_clientdata(client);        

	//g_jsa1127_ptr = obj;
	
	mt_set_gpio_mode(GPIO_ALS_EINT_PIN, GPIO_ALS_EINT_PIN_M_EINT);
	mt_set_gpio_dir(GPIO_ALS_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_ALS_EINT_PIN, FALSE);
	mt_set_gpio_pull_select(GPIO_ALS_EINT_PIN, GPIO_PULL_DOWN); 

	mt_eint_mask(GPIO_ALS_EINT_PIN);
    return 0;
}
/*-----------------------------------------------------------------------------*/
int JSA1127_i2c_master_operate(struct i2c_client *client, const char *buf, int count, int i2c_flag)
{
	int res = 0;
	mutex_lock(&jsa1127_mutex);
	switch(i2c_flag){	
	case I2C_FLAG_WRITE:
	client->addr &=I2C_MASK_FLAG;
	res = i2c_master_send(client, buf, count);
	client->addr &=I2C_MASK_FLAG;
	break;
	
	case I2C_FLAG_READ:
	client->addr &=I2C_MASK_FLAG;
	client->addr |=I2C_WR_FLAG;
	client->addr |=I2C_RS_FLAG;
	res = i2c_master_send(client, buf, count);
	//ret = i2c_master_recv(client, buf, 2);
	client->addr &=I2C_MASK_FLAG;
	break;
	case I2C_FLAG_READ_jsa1127:
	#if 0
	client->addr &=I2C_MASK_FLAG;
	client->addr |=I2C_WR_FLAG;
	client->addr |=I2C_RS_FLAG;
	//res = i2c_master_send(client, buf, count);
	res = i2c_master_recv(client, buf, 2);
	client->addr &=I2C_MASK_FLAG;
	break;
	#endif
	default:
	APS_LOG("JSA1127_i2c_master_operate i2c_flag command not support!\n");
	break;
	}
	if(res < 0)
	{
		goto EXIT_ERR;
	}
	mutex_unlock(&jsa1127_mutex);
	return res;
	EXIT_ERR:
	mutex_unlock(&jsa1127_mutex);
	APS_ERR("JSA1127_i2c_master_operate fail\n");
	return res;
}
/*----------------------------------------------------------------------------*/
static void jsa1127_power(struct alsps_hw *hw, unsigned int on) 
{
	static unsigned int power_on = 0;

	APS_LOG("power %s\n", on ? "on" : "off");

	if(hw->power_id != POWER_NONE_MACRO)
	{
		if(power_on == on)
		{
			APS_LOG("ignore power control: %d\n", on);
		}
		else if(on)
		{
			if(!hwPowerOn(hw->power_id, hw->power_vol, "JSA1127")) 
			{
				APS_ERR("power on fails!!\n");
			}
		}
		else
		{
			if(!hwPowerDown(hw->power_id, "JSA1127")) 
			{
				APS_ERR("power off fail!!\n");   
			}
		}
	}
	power_on = on;
}
/********************************************************************/

/********************************************************************/
int jsa1127_enable_als(struct i2c_client *client, int enable)
{
	struct jsa1127_priv *obj = i2c_get_clientdata(client);
	int res;
	u8 databuf[3];

	if(enable == 1)
		{
			APS_LOG("jsa1127_enable_als enable_als\n");
			databuf[0] = JSA1127_REG_ALS_CONF;
			client->addr &=I2C_MASK_FLAG;
			res = JSA1127_i2c_master_operate(client, databuf, 0x1, I2C_FLAG_WRITE);
			if(res < 0)
			{
				APS_ERR("i2c_master_send function err\n");
				goto ENABLE_ALS_EXIT_ERR;
			}
			atomic_set(&obj->als_deb_on, 1);
			atomic_set(&obj->als_deb_end, jiffies+atomic_read(&obj->als_debounce)/(1000/HZ));
		}
	else{
			APS_LOG("jsa1127_enable_als disable_als\n");
			databuf[0] = JSA1127_REG_ALS_CONF | 0x80;
			client->addr &=I2C_MASK_FLAG;

			res = JSA1127_i2c_master_operate(client, databuf, 0x01, I2C_FLAG_WRITE);
			if(res < 0)
			{
				APS_ERR("i2c_master_send function err\n");
				goto ENABLE_ALS_EXIT_ERR;
			}
			atomic_set(&obj->als_deb_on, 0);
		}
	return 0;
	ENABLE_ALS_EXIT_ERR:
	
	return res;
}
/********************************************************************/

/********************************************************************/
long jsa1127_read_als(struct i2c_client *client, u16 *data)
{
	long res;
	u8 databuf[2];
	//APS_FUN(f);
	//tabuf[0] = JSA1127_REG_ALS_DATA;
	res = i2c_master_recv(client, databuf, 2);
	if(res < 0)
	{
		APS_ERR("i2c_master_send function err\n");
		goto READ_ALS_EXIT_ERR;
	}
	else
		APS_ERR("ALS read success\n");
	APS_LOG("\n\n\n\nJSA1127_REG_ALS_DATA value value_low = %x, value_high = %x\n\n\n\n",databuf[0],databuf[1]);
	*data = (((databuf[1]&0x7F) << 8)|databuf[0]);
	
	return 0;
	READ_ALS_EXIT_ERR:
	return res;
}
/********************************************************************/

/********************************************************************/
static int jsa1127_get_als_value(struct jsa1127_priv *obj, u16 als)
{
		int idx;
		int invalid = 0;
		for(idx = 0; idx < obj->als_level_num; idx++)
		{
			if(als < obj->hw->als_level[idx])
			{
				break;
			}
		}
		if(idx >= obj->als_value_num)
		{
			APS_ERR("exceed range\n"); 
			idx = obj->als_value_num - 1;
		}
		
		if(1 == atomic_read(&obj->als_deb_on))
		{
			unsigned long endt = atomic_read(&obj->als_deb_end);
			if(time_after(jiffies, endt))
			{
				atomic_set(&obj->als_deb_on, 0);
			}
			
			if(1 == atomic_read(&obj->als_deb_on))
			{
				invalid = 1;
			}
		}
	
		if(!invalid)
		{
		#if defined(MTK_AAL_SUPPORT)
      	int level_high = obj->hw->als_level[idx];
    	int level_low = (idx > 0) ? obj->hw->als_level[idx-1] : 0;
        int level_diff = level_high - level_low;
		int value_high = obj->hw->als_value[idx];
        int value_low = (idx > 0) ? obj->hw->als_value[idx-1] : 0;
        int value_diff = value_high - value_low;
        int value = 0;
        if ((level_low >= level_high) || (value_low >= value_high))
            value = value_low;
        else
            value = (level_diff * value_low + (als - level_low) * value_diff + ((level_diff + 1) >> 1)) / level_diff;
		APS_DBG("ALS: %d [%d, %d] => %d [%d, %d] \n", als, level_low, level_high, value, value_low, value_high);
		return value;
		#endif
			if (atomic_read(&obj->trace) & CMC_TRC_CVT_ALS)
			{
				APS_DBG("ALS: %05d => %05d\n", als, obj->hw->als_value[idx]);
			}
			
			return obj->hw->als_value[idx];
		}
		else
		{
			if(atomic_read(&obj->trace) & CMC_TRC_CVT_ALS)
			{
				APS_DBG("ALS: %05d => %05d (-1)\n", als, obj->hw->als_value[idx]);	  
			}
			return -1;
		}

}


/*-------------------------------attribute file for debugging----------------------------------*/

/******************************************************************************
 * Sysfs attributes
*******************************************************************************/
static ssize_t jsa1127_show_config(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	
	if(!jsa1127_obj)
	{
		APS_ERR("jsa1127_obj is null!!\n");
		return 0;
	}
	
	res = snprintf(buf, PAGE_SIZE, "(%d %d)\n", 
		atomic_read(&jsa1127_obj->i2c_retry), 
		atomic_read(&jsa1127_obj->als_debounce));     
	return res;    
}
/*----------------------------------------------------------------------------*/
static ssize_t jsa1127_store_config(struct device_driver *ddri, const char *buf, size_t count)
{
	int retry, als_deb;
	if(!jsa1127_obj)
	{
		APS_ERR("jsa1127_obj is null!!\n");
		return 0;
	}
	
	if(5 == sscanf(buf, "%d %d", &retry, &als_deb ))
	{ 
		atomic_set(&jsa1127_obj->i2c_retry, retry);
		atomic_set(&jsa1127_obj->als_debounce, als_deb);
	}
	else
	{
		APS_ERR("invalid content: '%s', length = %d\n", buf, count);
	}
	return count;    
}
/*----------------------------------------------------------------------------*/
static ssize_t jsa1127_show_trace(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	if(!jsa1127_obj)
	{
		APS_ERR("jsa1127_obj is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&jsa1127_obj->trace));     
	return res;    
}
/*----------------------------------------------------------------------------*/
static ssize_t jsa1127_store_trace(struct device_driver *ddri, const char *buf, size_t count)
{
    int trace;
    if(!jsa1127_obj)
	{
		APS_ERR("jsa1127_obj is null!!\n");
		return 0;
	}
	
	if(1 == sscanf(buf, "0x%x", &trace))
	{
		atomic_set(&jsa1127_obj->trace, trace);
	}
	else 
	{
		APS_ERR("invalid content: '%s', length = %d\n", buf, count);
	}
	return count;    
}
/*----------------------------------------------------------------------------*/
static ssize_t jsa1127_show_als(struct device_driver *ddri, char *buf)
{
	int res;
	
	if(!jsa1127_obj)
	{
		APS_ERR("jsa1127_obj is null!!\n");
		return 0;
	}
	if((res = jsa1127_read_als(jsa1127_obj->client, &jsa1127_obj->als)))
	{
		return snprintf(buf, PAGE_SIZE, "ERROR: %d\n", res);
	}
	else
	{
		return snprintf(buf, PAGE_SIZE, "0x%04X\n", jsa1127_obj->als);     
	}
}
/*----------------------------------------------------------------------------*/
static ssize_t jsa1127_show_reg(struct device_driver *ddri, char *buf)
{
	if(!jsa1127_obj)
	{
		APS_ERR("jsa1127_obj is null!!\n");
		return 0;
	}
	
	
	return 0;
}
/*----------------------------------------------------------------------------*/
static ssize_t jsa1127_show_send(struct device_driver *ddri, char *buf)
{
    return 0;
}
/*----------------------------------------------------------------------------*/
static ssize_t jsa1127_store_send(struct device_driver *ddri, const char *buf, size_t count)
{
	int addr, cmd;
	u8 dat;

	if(!jsa1127_obj)
	{
		APS_ERR("jsa1127_obj is null!!\n");
		return 0;
	}
	else if(2 != sscanf(buf, "%x %x", &addr, &cmd))
	{
		APS_ERR("invalid format: '%s'\n", buf);
		return 0;
	}

	dat = (u8)cmd;
	//****************************
	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t jsa1127_show_recv(struct device_driver *ddri, char *buf)
{
    return 0;
}
/*----------------------------------------------------------------------------*/
static ssize_t jsa1127_store_recv(struct device_driver *ddri, const char *buf, size_t count)
{
	int addr;
	//u8 dat;
	if(!jsa1127_obj)
	{
		APS_ERR("jsa1127_obj is null!!\n");
		return 0;
	}
	else if(1 != sscanf(buf, "%x", &addr))
	{
		APS_ERR("invalid format: '%s'\n", buf);
		return 0;
	}

	//****************************
	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t jsa1127_show_status(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	
	if(!jsa1127_obj)
	{
		APS_ERR("jsa1127_obj is null!!\n");
		return 0;
	}
	
	if(jsa1127_obj->hw)
	{
		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: %d, (%d %d)\n", 
			jsa1127_obj->hw->i2c_num, jsa1127_obj->hw->power_id, jsa1127_obj->hw->power_vol);
	}
	else
	{
		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: NULL\n");
	}
	
	len += snprintf(buf+len, PAGE_SIZE-len, "REGS: %02X %02lX \n", 
				atomic_read(&jsa1127_obj->als_cmd_val), 
				jsa1127_obj->enable);
	
	len += snprintf(buf+len, PAGE_SIZE-len, "MISC: %d \n", 
		atomic_read(&jsa1127_obj->als_suspend));

	return len;
}
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
#define IS_SPACE(CH) (((CH) == ' ') || ((CH) == '\n'))
/*----------------------------------------------------------------------------*/
static int read_int_from_buf(struct jsa1127_priv *obj, const char* buf, size_t count, u32 data[], int len)
{
	int idx = 0;
	char *cur = (char*)buf, *end = (char*)(buf+count);

	while(idx < len)
	{
		while((cur < end) && IS_SPACE(*cur))
		{
			cur++;        
		}

		if(1 != sscanf(cur, "%d", &data[idx]))
		{
			break;
		}

		idx++; 
		while((cur < end) && !IS_SPACE(*cur))
		{
			cur++;
		}
	}
	return idx;
}
/*----------------------------------------------------------------------------*/
static ssize_t jsa1127_show_alslv(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	int idx;
	if(!jsa1127_obj)
	{
		APS_ERR("jsa1127_obj is null!!\n");
		return 0;
	}
	
	for(idx = 0; idx < jsa1127_obj->als_level_num; idx++)
	{
		len += snprintf(buf+len, PAGE_SIZE-len, "%d ", jsa1127_obj->hw->als_level[idx]);
	}
	len += snprintf(buf+len, PAGE_SIZE-len, "\n");
	return len;    
}
/*----------------------------------------------------------------------------*/
static ssize_t jsa1127_store_alslv(struct device_driver *ddri, const char *buf, size_t count)
{
	if(!jsa1127_obj)
	{
		APS_ERR("jsa1127_obj is null!!\n");
		return 0;
	}
	else if(!strcmp(buf, "def"))
	{
		memcpy(jsa1127_obj->als_level, jsa1127_obj->hw->als_level, sizeof(jsa1127_obj->als_level));
	}
	else if(jsa1127_obj->als_level_num != read_int_from_buf(jsa1127_obj, buf, count, 
			jsa1127_obj->hw->als_level, jsa1127_obj->als_level_num))
	{
		APS_ERR("invalid format: '%s'\n", buf);
	}    
	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t jsa1127_show_alsval(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	int idx;
	if(!jsa1127_obj)
	{
		APS_ERR("jsa1127_obj is null!!\n");
		return 0;
	}
	
	for(idx = 0; idx < jsa1127_obj->als_value_num; idx++)
	{
		len += snprintf(buf+len, PAGE_SIZE-len, "%d ", jsa1127_obj->hw->als_value[idx]);
	}
	len += snprintf(buf+len, PAGE_SIZE-len, "\n");
	return len;    
}
/*----------------------------------------------------------------------------*/
static ssize_t jsa1127_store_alsval(struct device_driver *ddri, const char *buf, size_t count)
{
	if(!jsa1127_obj)
	{
		APS_ERR("jsa1127_obj is null!!\n");
		return 0;
	}
	else if(!strcmp(buf, "def"))
	{
		memcpy(jsa1127_obj->als_value, jsa1127_obj->hw->als_value, sizeof(jsa1127_obj->als_value));
	}
	else if(jsa1127_obj->als_value_num != read_int_from_buf(jsa1127_obj, buf, count, 
			jsa1127_obj->hw->als_value, jsa1127_obj->als_value_num))
	{
		APS_ERR("invalid format: '%s'\n", buf);
	}    
	return count;
}
/*---------------------------------------------------------------------------------------*/
static DRIVER_ATTR(als,     S_IWUSR | S_IRUGO, jsa1127_show_als, NULL);
static DRIVER_ATTR(config,  S_IWUSR | S_IRUGO, jsa1127_show_config,	jsa1127_store_config);
static DRIVER_ATTR(alslv,   S_IWUSR | S_IRUGO, jsa1127_show_alslv, jsa1127_store_alslv);
static DRIVER_ATTR(alsval,  S_IWUSR | S_IRUGO, jsa1127_show_alsval, jsa1127_store_alsval);
static DRIVER_ATTR(trace,   S_IWUSR | S_IRUGO, jsa1127_show_trace,		jsa1127_store_trace);
static DRIVER_ATTR(status,  S_IWUSR | S_IRUGO, jsa1127_show_status, NULL);
static DRIVER_ATTR(send,    S_IWUSR | S_IRUGO, jsa1127_show_send, jsa1127_store_send);
static DRIVER_ATTR(recv,    S_IWUSR | S_IRUGO, jsa1127_show_recv, jsa1127_store_recv);
static DRIVER_ATTR(reg,     S_IWUSR | S_IRUGO, jsa1127_show_reg, NULL);
/*----------------------------------------------------------------------------*/
static struct driver_attribute *jsa1127_attr_list[] = {
    &driver_attr_als,  
    &driver_attr_trace,        /*trace log*/
    &driver_attr_config,
    &driver_attr_alslv,
    &driver_attr_alsval,
    &driver_attr_status,
    &driver_attr_send,
    &driver_attr_recv,
    &driver_attr_reg,
};

/*----------------------------------------------------------------------------*/
static int jsa1127_create_attr(struct device_driver *driver) 
{
	int idx, err = 0;
	int num = (int)(sizeof(jsa1127_attr_list)/sizeof(jsa1127_attr_list[0]));
	if (driver == NULL)
	{
		return -EINVAL;
	}

	for(idx = 0; idx < num; idx++)
	{
		if((err = driver_create_file(driver, jsa1127_attr_list[idx])))
		{            
			APS_ERR("driver_create_file (%s) = %d\n", jsa1127_attr_list[idx]->attr.name, err);
			break;
		}
	}    
	return err;
}
/*----------------------------------------------------------------------------*/
static int jsa1127_delete_attr(struct device_driver *driver)
{
	int idx ,err = 0;
	int num = (int)(sizeof(jsa1127_attr_list)/sizeof(jsa1127_attr_list[0]));

	if (!driver)
	return -EINVAL;

	for (idx = 0; idx < num; idx++) 
	{
		driver_remove_file(driver, jsa1127_attr_list[idx]);
	}
	
	return err;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------interrupt functions--------------------------------*/
/* static int intr_flag = 0; */
/*----------------------------------------------------------------------------*/
#if 0
static int jsa1127_check_intr(struct i2c_client *client) 
{
     return 0;
}
#endif
/*----------------------------------------------------------------------------*/
static void jsa1127_eint_work(struct work_struct *work)
{
	return;
}
/*----------------------------------------------------------------------------*/
static void jsa1127_eint_func(void)
{
	return;
}
static int jsa1127_setup_eint(struct i2c_client *client)
{
	struct jsa1127_priv *obj = i2c_get_clientdata(client);        
	g_jsa1127_ptr = obj;
	mt_set_gpio_mode(GPIO_ALS_EINT_PIN, GPIO_ALS_EINT_PIN_M_EINT);
	mt_set_gpio_dir(GPIO_ALS_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_ALS_EINT_PIN, TRUE);
	mt_set_gpio_pull_select(GPIO_ALS_EINT_PIN, GPIO_PULL_UP);

#ifdef CUST_EINT_ALS_TYPE
	mt_eint_set_hw_debounce(CUST_EINT_ALS_NUM, CUST_EINT_ALS_DEBOUNCE_CN);
	mt_eint_registration(CUST_EINT_ALS_NUM, CUST_EINT_ALS_TYPE, jsa1127_eint_func, 0);
#else
	mt65xx_eint_set_sens(CUST_EINT_ALS_NUM, CUST_EINT_ALS_SENSITIVE);
	mt65xx_eint_set_polarity(CUST_EINT_ALS_NUM, CUST_EINT_ALS_POLARITY);
	mt65xx_eint_set_hw_debounce(CUST_EINT_ALS_NUM, CUST_EINT_ALS_DEBOUNCE_CN);
	mt65xx_eint_registration(CUST_EINT_ALS_NUM, CUST_EINT_ALS_DEBOUNCE_EN, CUST_EINT_ALS_POLARITY, jsa1127_eint_func, 0);
#endif

#ifdef CUST_EINT_ALS_TYPE
	mt_eint_unmask(CUST_EINT_ALS_NUM);  
#else
	mt65xx_eint_unmask(CUST_EINT_ALS_NUM);  
#endif
    return 0;
}


/*-------------------------------MISC device related------------------------------------------*/


/************************************************************/
static int jsa1127_open(struct inode *inode, struct file *file)
{
	file->private_data = jsa1127_i2c_client;

	if (!file->private_data)
	{
		APS_ERR("null pointer!!\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}
/************************************************************/
static int jsa1127_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}
/************************************************************/

static long jsa1127_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
		struct i2c_client *client = (struct i2c_client*)file->private_data;
		struct jsa1127_priv *obj = i2c_get_clientdata(client);  
		long err = 0;
		void __user *ptr = (void __user*) arg;
		int dat;
		uint32_t enable;
		/* int threshold[2]; */
		
		switch (cmd)
		{
		
			case ALSPS_SET_ALS_MODE:
	
				if(copy_from_user(&enable, ptr, sizeof(enable)))
				{
					err = -EFAULT;
					goto err_out;
				}
				if(enable)
				{
					if((err = jsa1127_enable_als(obj->client, 1)))
					{
						APS_ERR("enable als fail: %ld\n", err); 
						goto err_out;
					}
					set_bit(CMC_BIT_ALS, &obj->enable);
				}
				else
				{
					if((err = jsa1127_enable_als(obj->client, 0)))
					{
						APS_ERR("disable als fail: %ld\n", err); 
						goto err_out;
					}
					clear_bit(CMC_BIT_ALS, &obj->enable);
				}
				break;
	
			case ALSPS_GET_ALS_MODE:
				enable = test_bit(CMC_BIT_ALS, &obj->enable) ? (1) : (0);
				if(copy_to_user(ptr, &enable, sizeof(enable)))
				{
					err = -EFAULT;
					goto err_out;
				}
				break;
	
			case ALSPS_GET_ALS_DATA: 
				if((err = jsa1127_read_als(obj->client, &obj->als)))
				{
					goto err_out;
				}
	
				dat = jsa1127_get_als_value(obj, obj->als);
				if(copy_to_user(ptr, &dat, sizeof(dat)))
				{
					err = -EFAULT;
					goto err_out;
				}			   
				break;
	
			case ALSPS_GET_ALS_RAW_DATA:	
				if((err = jsa1127_read_als(obj->client, &obj->als)))
				{
					goto err_out;
				}
	
				dat = obj->als;
				if(copy_to_user(ptr, &dat, sizeof(dat)))
				{
					err = -EFAULT;
					goto err_out;
				}			   
				break;
			
			/*------------------------------------------------------------------------------------------*/
			
			default:
				APS_ERR("%s not supported = 0x%04x", __FUNCTION__, cmd);
				err = -ENOIOCTLCMD;
				break;
		}
	
		err_out:
		return err;    
	}
/********************************************************************/
/*------------------------------misc device related operation functions------------------------------------*/
static struct file_operations jsa1127_fops = {
	.owner = THIS_MODULE,
	.open = jsa1127_open,
	.release = jsa1127_release,
	.unlocked_ioctl = jsa1127_unlocked_ioctl,
};

static struct miscdevice jsa1127_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "als_ps",
	.fops = &jsa1127_fops,
};

/*--------------------------------------------------------------------------------------*/
static void jsa1127_early_suspend(struct early_suspend *h)
{
		struct jsa1127_priv *obj = container_of(h, struct jsa1127_priv, early_drv);	
		int err;
		APS_FUN();	  
	
		if(!obj)
		{
			APS_ERR("null pointer!!\n");
			return;
		}
		
		atomic_set(&obj->als_suspend, 1);
		if((err = jsa1127_enable_als(obj->client, 0)))
		{
			APS_ERR("disable als fail: %d\n", err); 
		}
}

static void jsa1127_late_resume(struct early_suspend *h) 
{
		struct jsa1127_priv *obj = container_of(h, struct jsa1127_priv, early_drv);		  
		int err;
		hwm_sensor_data sensor_data;
		memset(&sensor_data, 0, sizeof(sensor_data));
		APS_FUN();
		if(!obj)
		{
			APS_ERR("null pointer!!\n");
			return;
		}
	
		atomic_set(&obj->als_suspend, 0);
		if(test_bit(CMC_BIT_ALS, &obj->enable))
		{
			if((err = jsa1127_enable_als(obj->client, 1)))
			{
				APS_ERR("enable als fail: %d\n", err);		  
	
			}
		}
}
/*--------------------------------------------------------------------------------*/
static int jsa1127_init_client(struct i2c_client *client)
{
	struct jsa1127_priv *obj = i2c_get_clientdata(client);
	u8 databuf[3];    
	int res = 0;
	databuf[0] = 0x0c;
	res = JSA1127_i2c_master_operate(client, databuf, 0x1, I2C_FLAG_WRITE);
	if(res <= 0)
	{
		APS_ERR("i2c_master_send function err\n");
		goto EXIT_ERR;
	}

	if(0 == obj->hw->polling_mode_als){
			databuf[0] = JSA1127_REG_ALS_THDH;
			databuf[1] = 0x00;
			databuf[2] = atomic_read(&obj->als_thd_val_high);
			res = JSA1127_i2c_master_operate(client, databuf, 0x3, I2C_FLAG_WRITE);
			if(res <= 0)
			{
				APS_ERR("i2c_master_send function err\n");
				goto EXIT_ERR;
			}
			databuf[0] = JSA1127_REG_ALS_THDL;
			databuf[1] = 0x00;
			databuf[2] = atomic_read(&obj->als_thd_val_low);//threshold value need to confirm
			res = JSA1127_i2c_master_operate(client, databuf, 0x3, I2C_FLAG_WRITE);
			if(res <= 0)
			{
				APS_ERR("i2c_master_send function err\n");
				goto EXIT_ERR;
			}

		}
	res = jsa1127_setup_eint(client);
	if(res!=0)
	{
		APS_ERR("setup eint: %d\n", res);
		return res;
	}
	
	res = jsa1127_disable_eint(client);
	if(res!=0)
	{
		APS_ERR("disable eint fail: %d\n", res);
		return res;
	}	
	
	return JSA1127_SUCCESS;
	
	EXIT_ERR:
	APS_ERR("init dev: %d\n", res);
	return res;
}
/*--------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------*/
int jsa1127_als_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
		int err = 0;
		int value;
		hwm_sensor_data* sensor_data;
		struct jsa1127_priv *obj = (struct jsa1127_priv *)self;
		//APS_FUN(f);
		switch (command)
		{
			case SENSOR_DELAY:
				//APS_ERR("jsa1127 als delay command!\n");
				if((buff_in == NULL) || (size_in < sizeof(int)))
				{
					APS_ERR("Set delay parameter error!\n");
					err = -EINVAL;
				}
				break;
	
			case SENSOR_ENABLE:
				//APS_ERR("jsa1127 als enable command!\n");
				if((buff_in == NULL) || (size_in < sizeof(int)))
				{
					APS_ERR("Enable sensor parameter error!\n");
					err = -EINVAL;
				}
				else
				{
					value = *(int *)buff_in;				
					if(value)
					{
						if((err = jsa1127_enable_als(obj->client, 1)))
						{
							APS_ERR("enable als fail: %d\n", err); 
							return -1;
						}
						set_bit(CMC_BIT_ALS, &obj->enable);
					}
					else
					{
						if((err = jsa1127_enable_als(obj->client, 0)))
						{
							APS_ERR("disable als fail: %d\n", err); 
							return -1;
						}
						clear_bit(CMC_BIT_ALS, &obj->enable);
					}
					
				}
				break;
	
			case SENSOR_GET_DATA:
				//APS_ERR("jsa1127 als get data command!\n");
				if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
				{
					APS_ERR("get sensor data parameter error!\n");
					err = -EINVAL;
				}
				else
				{
					sensor_data = (hwm_sensor_data *)buff_out;
									
					if((err = jsa1127_read_als(obj->client, &obj->als)))
					{
						err = -1;;
					}
					else
					{
						sensor_data->values[0] = jsa1127_get_als_value(obj, obj->als);
						sensor_data->value_divide = 1;
						sensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
					}				
				}
				break;
			default:
				APS_ERR("light sensor operate function no this parameter %d!\n", command);
				err = -1;
				break;
		}
		
		return err;

}
/*--------------------------------------------------------------------------------*/


/*-----------------------------------i2c operations----------------------------------*/
static int jsa1127_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct jsa1127_priv *obj;
	struct hwmsen_object obj_als;/* obj_ps */
	int err = 0;

	if(!(obj = kzalloc(sizeof(*obj), GFP_KERNEL)))
	{
		err = -ENOMEM;
		goto exit;
	}
	
	memset(obj, 0, sizeof(*obj));
	jsa1127_obj = obj;
	
	obj->hw = get_cust_alsps_hw();//get custom file data struct
	
	INIT_WORK(&obj->eint_work, jsa1127_eint_work);

	obj->client = client;
	i2c_set_clientdata(client, obj);

	/*-----------------------------value need to be confirmed-----------------------------------------*/
	atomic_set(&obj->als_debounce, 200);
	atomic_set(&obj->als_deb_on, 0);
	atomic_set(&obj->als_deb_end, 0);
	atomic_set(&obj->als_suspend, 0);
	atomic_set(&obj->als_cmd_val, 0xDF);
	atomic_set(&obj->als_thd_val_high,  obj->hw->als_threshold_high);
	atomic_set(&obj->als_thd_val_low,  obj->hw->als_threshold_low);
	obj->enable = 0;
	obj->pending_intr = 0;
	obj->als_level_num = sizeof(obj->hw->als_level)/sizeof(obj->hw->als_level[0]);
	obj->als_value_num = sizeof(obj->hw->als_value)/sizeof(obj->hw->als_value[0]);
	/*-----------------------------value need to be confirmed-----------------------------------------*/
	BUG_ON(sizeof(obj->als_level) != sizeof(obj->hw->als_level));
	memcpy(obj->als_level, obj->hw->als_level, sizeof(obj->als_level));
	BUG_ON(sizeof(obj->als_value) != sizeof(obj->hw->als_value));
	memcpy(obj->als_value, obj->hw->als_value, sizeof(obj->als_value));
	atomic_set(&obj->i2c_retry, 3);
	set_bit(CMC_BIT_ALS, &obj->enable);

	jsa1127_i2c_client = client;

	if((err = jsa1127_init_client(client)))
	{
		goto exit_init_failed;
	}
	APS_LOG("jsa1127_init_client() OK!\n");

	if((err = misc_register(&jsa1127_device)))
	{
		APS_ERR("jsa1127_device register failed\n");
		goto exit_misc_device_register_failed;
	}
	APS_LOG("jsa1127_device misc_register OK!\n");

	/*------------------------jsa1127 attribute file for debug--------------------------------------*/
	if((err = jsa1127_create_attr(&jsa1127_alsps_driver.driver)))
	{
		APS_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}
	/*------------------------jsa1127 attribute file for debug--------------------------------------*/
	obj_als.self = jsa1127_obj;
	obj_als.polling = obj->hw->polling_mode_als;;
	obj_als.sensor_operate = jsa1127_als_operate;
	if((err = hwmsen_attach(ID_LIGHT, &obj_als)))
	{
		APS_ERR("attach fail = %d\n", err);
		goto exit_sensor_obj_attach_fail;
	}
	#if defined(CONFIG_HAS_EARLYSUSPEND)
	obj->early_drv.level    = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 2,
	obj->early_drv.suspend  = jsa1127_early_suspend,
	obj->early_drv.resume   = jsa1127_late_resume,    
	register_early_suspend(&obj->early_drv);
	#endif

	APS_LOG("%s: OK\n", __func__);
	return 0;

	exit_create_attr_failed:
	exit_sensor_obj_attach_fail:
	exit_misc_device_register_failed:
		misc_deregister(&jsa1127_device);
	exit_init_failed:
		kfree(obj);
	exit:
	jsa1127_i2c_client = NULL;           
	APS_ERR("%s: err = %d\n", __func__, err);
	return err;
}

static int jsa1127_i2c_remove(struct i2c_client *client)
{
	int err;	
	/*------------------------jsa1127 attribute file for debug--------------------------------------*/	
	if((err = jsa1127_delete_attr(&jsa1127_i2c_driver.driver)))
	{
		APS_ERR("jsa1127_delete_attr fail: %d\n", err);
	} 
	/*----------------------------------------------------------------------------------------*/
	
	if((err = misc_deregister(&jsa1127_device)))
	{
		APS_ERR("misc_deregister fail: %d\n", err);    
	}
		
	jsa1127_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));
	return 0;

}

static int jsa1127_i2c_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	strcpy(info->type, JSA1127_DEV_NAME);
	return 0;

}

static int jsa1127_i2c_suspend(struct i2c_client *client, pm_message_t msg)
{
	APS_FUN();
	return 0;
}

static int jsa1127_i2c_resume(struct i2c_client *client)
{
	APS_FUN();
	return 0;
}

/*----------------------------------------------------------------------------*/

static int jsa1127_probe(struct platform_device *pdev) 
{
	//APS_FUN();  
	struct alsps_hw *hw = get_cust_alsps_hw();

	jsa1127_power(hw, 1); //*****************   
	
	if(i2c_add_driver(&jsa1127_i2c_driver))
	{
		APS_ERR("add driver error\n");
		return -1;
	} 
	return 0;
}
/*----------------------------------------------------------------------------*/
static int jsa1127_remove(struct platform_device *pdev)
{
	//APS_FUN(); 
	struct alsps_hw *hw = get_cust_alsps_hw();
	
	jsa1127_power(hw, 0);//*****************  
	
	i2c_del_driver(&jsa1127_i2c_driver);
	return 0;
}



/*----------------------------------------------------------------------------*/
static struct platform_driver jsa1127_alsps_driver = {
	.probe      = jsa1127_probe,
	.remove     = jsa1127_remove,    
	.driver     = {
		.name  = "als_ps",
	}
};

/*----------------------------------------------------------------------------*/
static int __init jsa1127_init(void)
{
	//APS_FUN();
	struct alsps_hw *hw = get_cust_alsps_hw();
	APS_LOG("%s: i2c_number=%d, i2c_addr: 0x%x\n", __func__, hw->i2c_num, hw->i2c_addr[0]);
	i2c_register_board_info(hw->i2c_num, &i2c_jsa1127, 1);
	if(platform_driver_register(&jsa1127_alsps_driver))
	{
		APS_ERR("failed to register driver");
		return -ENODEV;
	}
	return 0;
}
/*----------------------------------------------------------------------------*/
static void __exit jsa1127_exit(void)
{
	APS_FUN();
	platform_driver_unregister(&jsa1127_alsps_driver);
}
/*----------------------------------------------------------------------------*/
module_init(jsa1127_init);
module_exit(jsa1127_exit);
/*----------------------------------------------------------------------------*/
MODULE_AUTHOR("Solteam opto Inc");
MODULE_DESCRIPTION("jsa1127 driver");
MODULE_LICENSE("GPL");

