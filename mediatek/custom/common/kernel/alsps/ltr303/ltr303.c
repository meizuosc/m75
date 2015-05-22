/*
 * 
 * Author: MingHsien Hsieh <minghsien.hsieh@mediatek.com>
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
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include <asm/io.h>
#include <cust_eint.h>
#include <cust_alsps.h>
#include <linux/hwmsen_helper.h>
#include "ltr303.h"




#define POWER_NONE_MACRO MT65XX_POWER_NONE
#define AGC 1

/******************************************************************************
 * configuration
*******************************************************************************/
/*----------------------------------------------------------------------------*/

#define LTR303_DEV_NAME   "LTR_303ALS"

/*----------------------------------------------------------------------------*/
#define APS_TAG                  "[ALS] "
#define APS_FUN(f)               printk(KERN_INFO APS_TAG"%s\n", __FUNCTION__)
#define APS_ERR(fmt, args...)    printk(KERN_ERR  APS_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define APS_LOG(fmt, args...)    printk(KERN_INFO APS_TAG fmt, ##args)
#define APS_DBG(fmt, args...)    printk(KERN_INFO APS_TAG fmt, ##args)                 
/******************************************************************************
 * extern functions
*******************************************************************************/
extern void mt_eint_mask(unsigned int eint_num);
extern void mt_eint_unmask(unsigned int eint_num);
extern void mt_eint_set_hw_debounce(unsigned int eint_num, unsigned int ms);
extern void mt_eint_set_polarity(unsigned int eint_num, unsigned int pol);
extern unsigned int mt_eint_set_sens(unsigned int eint_num, unsigned int sens);
extern void mt_eint_registration(unsigned int eint_num, unsigned int flow, void (EINT_FUNC_PTR)(void), unsigned int is_auto_umask);
extern void mt_eint_print_status(void);

/*----------------------------------------------------------------------------*/

static struct i2c_client *ltr303_i2c_client = NULL;

/*----------------------------------------------------------------------------*/
static const struct i2c_device_id ltr303_i2c_id[] = {{LTR303_DEV_NAME,0},{}};
/*the adapter id & i2c address will be available in customization*/
static struct i2c_board_info __initdata i2c_ltr303={ I2C_BOARD_INFO("LTR_303ALS", 0x29)};

//static unsigned short ltr303_force[] = {0x00, 0x46, I2C_CLIENT_END, I2C_CLIENT_END};
//static const unsigned short *const ltr303_forces[] = { ltr303_force, NULL };
//static struct i2c_client_address_data ltr303_addr_data = { .forces = ltr303_forces,};
/*----------------------------------------------------------------------------*/
static int ltr303_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id); 
static int ltr303_i2c_remove(struct i2c_client *client);
//static int ltr303_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);
/*----------------------------------------------------------------------------*/
static int ltr303_i2c_suspend(struct i2c_client *client, pm_message_t msg);
static int ltr303_i2c_resume(struct i2c_client *client);
//static int ltr303_ps_enable(int gainrange);


//static int ps_gainrange;
static int als_gainrange;

//static int final_prox_val;
static int final_lux_val;

/*----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------*/
static int ltr303_als_read(int gainrange);
//static int ltr303_ps_read(void);


/*----------------------------------------------------------------------------*/


typedef enum {
    CMC_BIT_ALS    = 1,
    CMC_BIT_PS     = 2,
} CMC_BIT;

/*----------------------------------------------------------------------------*/
struct ltr303_i2c_addr {    /*define a series of i2c slave address*/
    u8  write_addr;  
    u8  ps_thd;     /*PS INT threshold*/
};

/*----------------------------------------------------------------------------*/

struct ltr303_priv {
    struct alsps_hw  *hw;
    struct i2c_client *client;
    struct work_struct  eint_work;
    struct mutex lock;
	/*i2c address group*/
    struct ltr303_i2c_addr  addr;

     /*misc*/
    u16		    als_modulus;
    atomic_t    i2c_retry;
    atomic_t    als_debounce;   /*debounce time after enabling als*/
    atomic_t    als_deb_on;     /*indicates if the debounce is on*/
    atomic_t    als_deb_end;    /*the jiffies representing the end of debounce*/
    //atomic_t    ps_mask;        /*mask ps: always return far away*/
    //atomic_t    ps_debounce;    /*debounce time after enabling ps*/
    //atomic_t    ps_deb_on;      /*indicates if the debounce is on*/
    //atomic_t    ps_deb_end;     /*the jiffies representing the end of debounce*/
    //atomic_t    ps_suspend;
    atomic_t    als_suspend;

    /*data*/
    u16         als;
    //u16          ps;
    u8          _align;
    u16         als_level_num;
    u16         als_value_num;
    u32         als_level[C_CUST_ALS_LEVEL-1];
    u32         als_value[C_CUST_ALS_LEVEL];

    atomic_t    als_cmd_val;    /*the cmd value can't be read, stored in ram*/
    //atomic_t    ps_cmd_val;     /*the cmd value can't be read, stored in ram*/
    //atomic_t    ps_thd_val;     /*the cmd value can't be read, stored in ram*/
	//atomic_t    ps_thd_val_high;     /*the cmd value can't be read, stored in ram*/
	//atomic_t    ps_thd_val_low;     /*the cmd value can't be read, stored in ram*/
    ulong       enable;         /*enable mask*/
    ulong       pending_intr;   /*pending interrupt*/

    /*early suspend*/
#if defined(CONFIG_HAS_EARLYSUSPEND)
    struct early_suspend    early_drv;
#endif     
};
/*
 struct PS_CALI_DATA_STRUCT
{
    int close;
    int far_away;
    int valid;
} ;

static struct PS_CALI_DATA_STRUCT ps_cali={0,0,0};
static int intr_flag_value = 0;
*/

static struct ltr303_priv *ltr303_obj = NULL;
static struct platform_driver ltr303_alsps_driver;

/*----------------------------------------------------------------------------*/
static struct i2c_driver ltr303_i2c_driver = {	
	.probe      = ltr303_i2c_probe,
	.remove     = ltr303_i2c_remove,
	//.detect     = ltr303_i2c_detect,
	.suspend    = ltr303_i2c_suspend,
	.resume     = ltr303_i2c_resume,
	.id_table   = ltr303_i2c_id,
	//.address_data = &ltr303_addr_data,
	.driver = {
		//.owner          = THIS_MODULE,
		.name           = LTR303_DEV_NAME,
	},
};


/* 
 * #########
 * ## I2C ##
 * #########
 */

// I2C Read
static int ltr303_i2c_read_reg(u8 regnum)
{
    u8 buffer[1],reg_value[1];
	int res = 0;
	
	buffer[0]= regnum;
	res = i2c_master_send(ltr303_obj->client, buffer, 0x1);
	if(res <= 0)	{
	   
	   APS_ERR("read reg send res = %d\n",res);
		return res;
	}
	res = i2c_master_recv(ltr303_obj->client, reg_value, 0x1);
	if(res <= 0)
	{
		APS_ERR("read reg recv res = %d\n",res);
		return res;
	}
	return reg_value[0];
}

// I2C Write
static int ltr303_i2c_write_reg(u8 regnum, u8 value)
{
	u8 databuf[2];    
	int res = 0;
   
	databuf[0] = regnum;   
	databuf[1] = value;
	res = i2c_master_send(ltr303_obj->client, databuf, 0x2);

	if (res < 0)
		{
			APS_ERR("wirte reg send res = %d\n",res);
		   	return res;
		}
		
	else
		return 0;
}


/*----------------------------------------------------------------------------*/
static ssize_t ltr303_show_als(struct device_driver *ddri, char *buf)
{
	int res;
	//u8 dat = 0;
	
	if(!ltr303_obj)
	{
		APS_ERR("ltr303_obj is null!!\n");
		return 0;
	}
	res = ltr303_als_read(als_gainrange);
    return snprintf(buf, PAGE_SIZE, "0x%04X\n", res);    
	
}
/*----------------------------------------------------------------------------*/


static ssize_t ltr303_show_status(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	
	if(!ltr303_obj)
	{
		APS_ERR("ltr303_obj is null!!\n");
		return 0;
	}
	
	if(ltr303_obj->hw)
	{
	
		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: %d, (%d %d)\n", 
			ltr303_obj->hw->i2c_num, ltr303_obj->hw->power_id, ltr303_obj->hw->power_vol);
		
	}
	else
	{
		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: NULL\n");
	}


	len += snprintf(buf+len, PAGE_SIZE-len, "MISC: %d\n", atomic_read(&ltr303_obj->als_suspend));

	return len;
}

/*----------------------------------------------------------------------------*/
static ssize_t ltr303_show_reg(struct device_driver *ddri, char *buf)
{
	int i,len=0;
	int reg[]={0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8a,0x8b,0x8c,
		0x8d,0x8e,0x8f,0x90,0x91,0x92,0x93,0x94,0x95,0x97,0x98,0x99,0x9a,0x9e};
	for(i=0;i<27;i++)
		{
		len += snprintf(buf+len, PAGE_SIZE-len, "reg:0x%04X value: 0x%04X\n", reg[i],ltr303_i2c_read_reg(reg[i]));	

	    }
	return len;
}
/*----------------------------------------------------------------------------*/
/*
static ssize_t ltr303_store_reg(struct device_driver *ddri, char *buf, size_t count)
{
	int ret,value;
	u8 reg;
	if(!ltr303_obj)
	{
		APS_ERR("ltr303_obj is null!!\n");
		return 0;
	}
	
	if(2 == sscanf(buf, "%x %x ", &reg,&value))
	{ 
		APS_DBG("before write reg: %x, reg_value = %x  write value=%x\n", reg,ltr303_i2c_read_reg(reg),value);
	    ret=ltr303_i2c_write_reg(reg,value);
		APS_DBG("after write reg: %x, reg_value = %x\n", reg,ltr303_i2c_read_reg(reg));
	}
	else
	{
		APS_DBG("invalid content: '%s', length = %d\n", buf, count);
	}
	return count;    
}
*/
/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(als,     S_IWUSR | S_IRUGO, ltr303_show_als,   NULL);
//static DRIVER_ATTR(ps,      S_IWUSR | S_IRUGO, ltr303_show_ps,    NULL);
//static DRIVER_ATTR(config,  S_IWUSR | S_IRUGO, ltr303_show_config,ltr303_store_config);
//static DRIVER_ATTR(alslv,   S_IWUSR | S_IRUGO, ltr303_show_alslv, ltr303_store_alslv);
//static DRIVER_ATTR(alsval,  S_IWUSR | S_IRUGO, ltr303_show_alsval,ltr303_store_alsval);
//static DRIVER_ATTR(trace,   S_IWUSR | S_IRUGO,ltr303_show_trace, ltr303_store_trace);
static DRIVER_ATTR(status,  S_IWUSR | S_IRUGO, ltr303_show_status,  NULL);
static DRIVER_ATTR(reg,     S_IWUSR | S_IRUGO, ltr303_show_reg,   NULL);
//static DRIVER_ATTR(i2c,     S_IWUSR | S_IRUGO, ltr303_show_i2c,   ltr303_store_i2c);
/*----------------------------------------------------------------------------*/
static struct driver_attribute *ltr303_attr_list[] = {
    &driver_attr_als,
   // &driver_attr_ps,    
   // &driver_attr_trace,        /*trace log*/
   // &driver_attr_config,
   // &driver_attr_alslv,
   // &driver_attr_alsval,
    &driver_attr_status,
   //&driver_attr_i2c,
    &driver_attr_reg,
};
/*----------------------------------------------------------------------------*/
static int ltr303_create_attr(struct device_driver *driver) 
{
	int idx, err = 0;
	int num = (int)(sizeof(ltr303_attr_list)/sizeof(ltr303_attr_list[0]));

	if (driver == NULL)
	{
		return -EINVAL;
	}

	for(idx = 0; idx < num; idx++)
	{
		if((err = driver_create_file(driver, ltr303_attr_list[idx])))
		{            
			APS_ERR("driver_create_file (%s) = %d\n", ltr303_attr_list[idx]->attr.name, err);
			break;
		}
	}    
	return err;
}
/*----------------------------------------------------------------------------*/
	static int ltr303_delete_attr(struct device_driver *driver)
	{
	int idx ,err = 0;
	int num = (int)(sizeof(ltr303_attr_list)/sizeof(ltr303_attr_list[0]));

	if (!driver)
	return -EINVAL;

	for (idx = 0; idx < num; idx++) 
	{
		driver_remove_file(driver, ltr303_attr_list[idx]);
	}
	
	return err;
}

/*----------------------------------------------------------------------------*/

/* 
 * ################
 * ## ALS CONFIG ##
 * ################
 */

static int ltr303_als_enable(int gainrange)
{
	int error;
	APS_LOG("gainrange = %d\n",gainrange);
	switch (gainrange)
	{
		case ALS_RANGE_64K:
			error = ltr303_i2c_write_reg(LTR303_ALS_CONTR, MODE_ALS_ON_Range1);
			break;

		case ALS_RANGE_32K:
			error = ltr303_i2c_write_reg(LTR303_ALS_CONTR, MODE_ALS_ON_Range2);
			break;

		case ALS_RANGE_16K:
			error = ltr303_i2c_write_reg(LTR303_ALS_CONTR, MODE_ALS_ON_Range3);
			break;
			
		case ALS_RANGE_8K:
			error = ltr303_i2c_write_reg(LTR303_ALS_CONTR, MODE_ALS_ON_Range4);
			break;
			
		case ALS_RANGE_1300:
			error = ltr303_i2c_write_reg(LTR303_ALS_CONTR, MODE_ALS_ON_Range5);
			break;

		case ALS_RANGE_600:
			error = ltr303_i2c_write_reg(LTR303_ALS_CONTR, MODE_ALS_ON_Range6);
			break;
			
		default:
			error = ltr303_i2c_write_reg(LTR303_ALS_CONTR, MODE_ALS_ON_Range1);			
			APS_ERR("proxmy sensor gainrange %d!\n", gainrange);
			break;
	}

	mdelay(WAKEUP_DELAY);

	/* =============== 
	 * ** IMPORTANT **
	 * ===============
	 * Other settings like timing and threshold to be set here, if required.
 	 * Not set and kept as device default for now.
 	 */
 	if(error<0)
 	    APS_LOG("ltr303_als_enable ...ERROR\n");
 	else
        APS_LOG("ltr303_als_enable ...OK\n");
        
	return error;
}


// Put ALS into Standby mode
static int ltr303_als_disable(void)
{
	int error;
	error = ltr303_i2c_write_reg(LTR303_ALS_CONTR, MODE_ALS_StdBy); 
	if(error<0)
 	    APS_LOG("ltr303_als_disable ...ERROR\n");
 	else
        APS_LOG("ltr303_als_disable ...OK\n");
	return error;
}

static int ltr303_als_read(int gainrange)
{
	int alsval_ch0_lo, alsval_ch0_hi, alsval_ch0;
	int alsval_ch1_lo, alsval_ch1_hi, alsval_ch1;
	int luxdata_int;
    int ratio;
	
	
	alsval_ch0_lo = ltr303_i2c_read_reg(LTR303_ALS_DATA_CH0_0);
	alsval_ch0_hi = ltr303_i2c_read_reg(LTR303_ALS_DATA_CH0_1);
	alsval_ch0 = (alsval_ch0_hi * 256) + alsval_ch0_lo;
	APS_DBG("alsval_ch0_lo = %d,alsval_ch0_hi=%d,alsval_ch0=%d\n",alsval_ch0_lo,alsval_ch0_hi,alsval_ch0);
	alsval_ch1_lo = ltr303_i2c_read_reg(LTR303_ALS_DATA_CH1_0);
	alsval_ch1_hi = ltr303_i2c_read_reg(LTR303_ALS_DATA_CH1_1);
	alsval_ch1 = (alsval_ch1_hi * 256) + alsval_ch1_lo;
	APS_DBG("alsval_ch1_lo = %d,alsval_ch1_hi=%d,alsval_ch1=%d\n",alsval_ch1_lo,alsval_ch1_hi,alsval_ch1);
	

	
    if((alsval_ch1==0)||(alsval_ch0==0))
    {
        luxdata_int = 0;
        goto err;
    }

	ratio = (alsval_ch1*100) /(alsval_ch0+alsval_ch1);
	APS_DBG("ratio = %d  gainrange = %d\n",ratio,gainrange);
	if (ratio < 45){
		luxdata_int = (((17743 * alsval_ch0)+(11059 * alsval_ch1))/gainrange)/10000;
	}
	else if ((ratio < 64) && (ratio >= 45)){
		luxdata_int = (((42785 * alsval_ch0)-(19548 * alsval_ch1))/gainrange)/10000;
	}
	else if ((ratio < 85) && (ratio >= 64)) {
		luxdata_int = (((5926 * alsval_ch0)+(1185 * alsval_ch1))/gainrange)/10000;
	}
	else {
		luxdata_int = 0;
		}
	
	
	if(AGC==1) // AGC=1 for Automated Gain control - added by Wee Liat on 26-Mar-2014
	{
    	if((alsval_ch1+alsval_ch0) > 40000)
    	{
    		gainrange = ALS_RANGE_64K;
			ltr303_als_enable(gainrange);
			
		
    	}
    	else if((alsval_ch1+alsval_ch0) < 5000)
    	{
    		gainrange = ALS_RANGE_8K;
			ltr303_als_enable(gainrange);
    	}
	}		
	
	APS_DBG("als_value_lux = %d\n", luxdata_int);
	return luxdata_int;

	
err:
	final_lux_val = luxdata_int;
	APS_DBG("err als_value_lux = 0x%x\n", luxdata_int);
	return luxdata_int;
}



/*----------------------------------------------------------------------------*/
int ltr303_get_addr(struct alsps_hw *hw, struct ltr303_i2c_addr *addr)
{
	/***
	if(!hw || !addr)
	{
		return -EFAULT;
	}
	addr->write_addr= hw->i2c_addr[0];
	***/
	return 0;
}


/*-----------------------------------------------------------------------------*/
void ltr303_eint_func(void)
{
	struct ltr303_priv *obj = ltr303_obj;

	APS_FUN();
	if(!obj)
	{
		return;
	}
	
	schedule_work(&obj->eint_work);
	//schedule_delayed_work(&obj->eint_work);
}



/*----------------------------------------------------------------------------*/
int ltr303_setup_eint(struct i2c_client *client)
{
	struct ltr303_priv *obj = (struct ltr303_priv *)i2c_get_clientdata(client);        

	APS_FUN();
	ltr303_obj = obj;
	mt_set_gpio_dir(GPIO_ALS_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_mode(GPIO_ALS_EINT_PIN, GPIO_ALS_EINT_PIN_M_EINT);
	mt_set_gpio_pull_enable(GPIO_ALS_EINT_PIN, TRUE);
	mt_set_gpio_pull_select(GPIO_ALS_EINT_PIN, GPIO_PULL_UP);

	mt_eint_set_hw_debounce(CUST_EINT_ALS_NUM, CUST_EINT_ALS_DEBOUNCE_CN);
	mt_eint_registration(CUST_EINT_ALS_NUM, CUST_EINT_ALS_TYPE, ltr303_eint_func, 0);
	mt_eint_unmask(CUST_EINT_ALS_NUM);

    return 0;
}


/*----------------------------------------------------------------------------*/
static void ltr303_power(struct alsps_hw *hw, unsigned int on) 
{
	static unsigned int power_on = 0;

	//APS_LOG("power %s\n", on ? "on" : "off");

	if(hw->power_id != POWER_NONE_MACRO)
	{
		if(power_on == on)
		{
			APS_LOG("ignore power control: %d\n", on);
		}
		else if(on)
		{
			if(!hwPowerOn(hw->power_id, hw->power_vol, "LTR303")) 
			{
				APS_ERR("power on fails!!\n");
			}
		}
		else
		{
			if(!hwPowerDown(hw->power_id, "LTR303")) 
			{
				APS_ERR("power off fail!!\n");   
			}
		}
	}
	power_on = on;
}

/*----------------------------------------------------------------------------*/
/*for interrup work mode support -- by liaoxl.lenovo 12.08.2011*/
static int ltr303_check_and_clear_intr(struct i2c_client *client) 
{
//***
	int res,intp,intl;
	u8 buffer[2];	
	u8 temp;

	APS_FUN();
		//if (mt_get_gpio_in(GPIO_ALS_EINT_PIN) == 1) /*skip if no interrupt*/	
		//	  return 0;
	
		buffer[0] = LTR303_ALS_PS_STATUS;
		res = i2c_master_send(client, buffer, 0x1);
		if(res <= 0)
		{
			goto EXIT_ERR;
		}
		res = i2c_master_recv(client, buffer, 0x1);
		if(res <= 0)
		{
			goto EXIT_ERR;
		}
		temp = buffer[0];
		res = 1;
		intp = 0;
		intl = 0;
		if(0 != (buffer[0] & 0x02))
		{
			res = 0;
			intp = 1;
		}
		if(0 != (buffer[0] & 0x08))
		{
			res = 0;
			intl = 1;		
		}
	
		if(0 == res)
		{
			if((1 == intp) && (0 == intl))
			{
				buffer[1] = buffer[0] & 0xfD;
				
			}
			else if((0 == intp) && (1 == intl))
			{
				buffer[1] = buffer[0] & 0xf7;
			}
			else
			{
				buffer[1] = buffer[0] & 0xf5;
			}
			buffer[0] = LTR303_ALS_PS_STATUS	;
			res = i2c_master_send(client, buffer, 0x2);
			if(res <= 0)
			{
				goto EXIT_ERR;
			}
			else
			{
				res = 0;
			}
		}
	
		return res;
	
	EXIT_ERR:
		APS_ERR("ltr303_check_and_clear_intr fail\n");
		return 1;

}
/*----------------------------------------------------------------------------*/


static int ltr303_check_intr(struct i2c_client *client) 
{
	int res,intp,intl;
	u8 buffer[2];

	APS_FUN();
	//if (mt_get_gpio_in(GPIO_ALS_EINT_PIN) == 1) /*skip if no interrupt*/  
	//    return 0;

	buffer[0] = LTR303_ALS_PS_STATUS;
	res = i2c_master_send(client, buffer, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	res = i2c_master_recv(client, buffer, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	res = 1;
	intp = 0;
	intl = 0;
	if(0 != (buffer[0] & 0x02))
	{
		res = 0;
		intp = 1;
	}
	if(0 != (buffer[0] & 0x08))
	{
		res = 0;
		intl = 1;		
	}

	return res;

EXIT_ERR:
	APS_ERR("ltr303_check_intr fail\n");
	return 1;
}
/*
static int ltr303_clear_intr(struct i2c_client *client) 
{
	int res;
	u8 buffer[2];

	APS_FUN();
	
	buffer[0] = LTR303_ALS_PS_STATUS;
	res = i2c_master_send(client, buffer, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	res = i2c_master_recv(client, buffer, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	APS_DBG("buffer[0] = %d \n",buffer[0]);
	buffer[1] = buffer[0] & 0x01;
	buffer[0] = LTR303_ALS_PS_STATUS	;

	res = i2c_master_send(client, buffer, 0x2);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	else
	{
		res = 0;
	}

	return res;

EXIT_ERR:
	APS_ERR("ltr303_check_and_clear_intr fail\n");
	return 1;
}
*/



static int ltr303_devinit(void)
{
	int res;
	//int init_ps_gain;
	int init_als_gain;
	//u8 databuf[2];	

	struct i2c_client *client = ltr303_obj->client;

	//struct ltr303_priv *obj = ltr303_obj;   
	
	mdelay(PON_DELAY);

	
	// Enable ALS to 8Klux Range at startup
	init_als_gain = ALS_RANGE_8K;
	als_gainrange = init_als_gain;

	res = ltr303_als_enable(init_als_gain);
	if (res < 0)
		goto EXIT_ERR;


	/*for interrup work mode support */
	/*
	if(0 == obj->hw->polling_mode_ps)
	{	
		APS_LOG("eint enable");
		ltr303_ps_set_thres();
		
		databuf[0] = LTR303_INTERRUPT;	
		databuf[1] = 0x01;
		res = i2c_master_send(client, databuf, 0x2);
		if(res <= 0)
		{
			goto EXIT_ERR;
			return ltr303_ERR_I2C;
		}

		databuf[0] = LTR303_INTERRUPT_PERSIST;	
		databuf[1] = 0x20;
		res = i2c_master_send(client, databuf, 0x2);
		if(res <= 0)
		{
			goto EXIT_ERR;
			return ltr303_ERR_I2C;
		}

	}
	*/
	if((res = ltr303_setup_eint(client))!=0)
	{
		APS_ERR("setup eint: %d\n", res);
		return res;
	}
	
	if((res = ltr303_check_and_clear_intr(client)))
	{
		APS_ERR("check/clear intr: %d\n", res);
		//    return res;
	}

	res = 0;

	EXIT_ERR:
	APS_ERR("init dev: %d\n", res);
	return res;

}
/*----------------------------------------------------------------------------*/


static int ltr303_get_als_value(struct ltr303_priv *obj, u16 als)
{
	int idx;
	int invalid = 0;
	APS_DBG("als  = %d\n",als); 
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
		APS_DBG("ALS: %05d => %05d\n", als, obj->hw->als_value[idx]);	
		return obj->hw->als_value[idx];
	}
	else
	{
		APS_ERR("ALS: %05d => %05d (-1)\n", als, obj->hw->als_value[idx]);    
		return -1;
	}
}

/*----------------------------------------------------------------------------*/
/*for interrup work mode support */
static void ltr303_eint_work(struct work_struct *work)
{
	struct ltr303_priv *obj = (struct ltr303_priv *)container_of(work, struct ltr303_priv, eint_work);
	int err;
	//hwm_sensor_data sensor_data;
//	u8 buffer[1];
//	u8 reg_value[1];
	//u8 databuf[2];
	//int res = 0;
	APS_FUN();
	err = ltr303_check_intr(obj->client);
	if(err < 0)
	{
		APS_ERR("ltr303_eint_work check intrs: %d\n", err);
	}
	else
	{
		//get raw data
		/*
		obj->ps = ltr303_ps_read();
    	if(obj->ps < 0)
    	{
    		err = -1;
    		return;
    	}
				
		APS_DBG("ltr303_eint_work rawdata ps=%d als_ch0=%d!\n",obj->ps,obj->als);
		sensor_data.values[0] = ltr303_get_ps_value(obj, obj->ps);
		sensor_data.value_divide = 1;
		sensor_data.status = SENSOR_STATUS_ACCURACY_MEDIUM;	
		*/		
/*singal interrupt function add*/
		/*
		APS_DBG("intr_flag_value=%d\n",intr_flag_value);
		if(intr_flag_value){
				APS_DBG(" interrupt value ps will < ps_threshold_low");

				databuf[0] = LTR303_PS_THRES_LOW_0;	
				databuf[1] = (u8)((atomic_read(&obj->ps_thd_val_low)) & 0x00FF);
				res = i2c_master_send(obj->client, databuf, 0x2);
				if(res <= 0)
				{
					return;
				}
				databuf[0] = LTR303_PS_THRES_LOW_1;	
				databuf[1] = (u8)(((atomic_read(&obj->ps_thd_val_low)) & 0xFF00) >> 8);
				res = i2c_master_send(obj->client, databuf, 0x2);
				if(res <= 0)
				{
					return;
				}
				databuf[0] = LTR303_PS_THRES_UP_0;	
				databuf[1] = (u8)(0x00FF);
				res = i2c_master_send(obj->client, databuf, 0x2);
				if(res <= 0)
				{
					return;
				}
				databuf[0] = LTR303_PS_THRES_UP_1; 
				databuf[1] = (u8)((0xFF00) >> 8);;
				res = i2c_master_send(obj->client, databuf, 0x2);
				if(res <= 0)
				{
					return;
				}
		}
		else{	
				APS_DBG(" interrupt value ps will > ps_threshold_high");
				databuf[0] = LTR303_PS_THRES_LOW_0;	
				databuf[1] = (u8)(0 & 0x00FF);
				res = i2c_master_send(obj->client, databuf, 0x2);
				if(res <= 0)
				{
					return;
				}
				databuf[0] = LTR303_PS_THRES_LOW_1;	
				databuf[1] = (u8)((0 & 0xFF00) >> 8);
				res = i2c_master_send(obj->client, databuf, 0x2);
				if(res <= 0)
				{
					return;
				}
				databuf[0] = LTR303_PS_THRES_UP_0;	
				databuf[1] = (u8)((atomic_read(&obj->ps_thd_val_high)) & 0x00FF);
				res = i2c_master_send(obj->client, databuf, 0x2);
				if(res <= 0)
				{
					return;
				}
				databuf[0] = LTR303_PS_THRES_UP_1; 
				databuf[1] = (u8)(((atomic_read(&obj->ps_thd_val_high)) & 0xFF00) >> 8);;
				res = i2c_master_send(obj->client, databuf, 0x2);
				if(res <= 0)
				{
					return;
				}
		}
		//let up layer to know
		if((err = hwmsen_get_interrupt_data(ID_PROXIMITY, &sensor_data)))
		{
		  APS_ERR("call hwmsen_get_interrupt_data fail = %d\n", err);
		}
		*/
	}
	//ltr303_clear_intr(obj->client);
	mt_eint_unmask(CUST_EINT_ALS_NUM);      
}



/****************************************************************************** 
 * Function Configuration
******************************************************************************/
static int ltr303_open(struct inode *inode, struct file *file)
{
	file->private_data = ltr303_i2c_client;

	if (!file->private_data)
	{
		APS_ERR("null pointer!!\n");
		return -EINVAL;
	}
	
	return nonseekable_open(inode, file);
}
/*----------------------------------------------------------------------------*/
static int ltr303_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}
/*----------------------------------------------------------------------------*/


static long ltr303_unlocked_ioctl(struct file *file, unsigned int cmd,
       unsigned long arg)       
{
	struct i2c_client *client = (struct i2c_client*)file->private_data;
	struct ltr303_priv *obj = i2c_get_clientdata(client);  
	int err = 0;
	void __user *ptr = (void __user*) arg;
	int dat;
	uint32_t enable;
	APS_DBG("cmd= %d\n", cmd); 
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
			    err = ltr303_als_enable(als_gainrange);
				if(err < 0)
				{
					APS_ERR("enable als fail: %d\n", err); 
					goto err_out;
				}
				set_bit(CMC_BIT_ALS, &obj->enable);
			}
			else
			{
			    err = ltr303_als_disable();
				if(err < 0)
				{
					APS_ERR("disable als fail: %d\n", err); 
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
		    obj->als = ltr303_als_read(als_gainrange);
			if(obj->als < 0)
			{
				goto err_out;
			}

			dat = ltr303_get_als_value(obj, obj->als);
			if(copy_to_user(ptr, &dat, sizeof(dat)))
			{
				err = -EFAULT;
				goto err_out;
			}              
			break;

		case ALSPS_GET_ALS_RAW_DATA:    
			obj->als = ltr303_als_read(als_gainrange);
			if(obj->als < 0)
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

		default:
			APS_ERR("%s not supported = 0x%04x", __FUNCTION__, cmd);
			err = -ENOIOCTLCMD;
			break;
	}

	err_out:
	return err;    
}

/*----------------------------------------------------------------------------*/
static struct file_operations ltr303_fops = {
	//.owner = THIS_MODULE,
	.open = ltr303_open,
	.release = ltr303_release,
	.unlocked_ioctl = ltr303_unlocked_ioctl,
};
/*----------------------------------------------------------------------------*/
static struct miscdevice ltr303_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "als_ps",
	.fops = &ltr303_fops,
};

static int ltr303_i2c_suspend(struct i2c_client *client, pm_message_t msg) 
{
	struct ltr303_priv *obj = i2c_get_clientdata(client);    
	int err;
	APS_FUN();    

	if(msg.event == PM_EVENT_SUSPEND)
	{   
		if(!obj)
		{
			APS_ERR("null pointer!!\n");
			return -EINVAL;
		}
		
		atomic_set(&obj->als_suspend, 1);
		err = ltr303_als_disable();
		if(err < 0)
		{
			APS_ERR("disable als: %d\n", err);
			return err;
		}

		ltr303_power(obj->hw, 0);
	}
	return 0;
}
/*----------------------------------------------------------------------------*/
static int ltr303_i2c_resume(struct i2c_client *client)
{
	struct ltr303_priv *obj = i2c_get_clientdata(client);        
	int err;
	APS_FUN();

	if(!obj)
	{
		APS_ERR("null pointer!!\n");
		return -EINVAL;
	}

	ltr303_power(obj->hw, 1);
/*	err = ltr303_devinit();
	if(err < 0)
	{
		APS_ERR("initialize client fail!!\n");
		return err;        
	}*/
	atomic_set(&obj->als_suspend, 0);
	if(test_bit(CMC_BIT_ALS, &obj->enable))
	{
	    err = ltr303_als_enable(als_gainrange);
	    if (err < 0)
		{
			APS_ERR("enable als fail: %d\n", err);        
		}
	}
	
	return 0;
}

static void ltr303_early_suspend(struct early_suspend *h) 
{   /*early_suspend is only applied for ALS*/
	struct ltr303_priv *obj = container_of(h, struct ltr303_priv, early_drv);   
	int err;
	APS_FUN();    

	if(!obj)
	{
		APS_ERR("null pointer!!\n");
		return;
	}
	
	atomic_set(&obj->als_suspend, 1); 
	err = ltr303_als_disable();
	if(err < 0)
	{
		APS_ERR("disable als fail: %d\n", err); 
	}
}

static void ltr303_late_resume(struct early_suspend *h)
{   /*early_suspend is only applied for ALS*/
	struct ltr303_priv *obj = container_of(h, struct ltr303_priv, early_drv);         
	int err;
	APS_FUN();

	if(!obj)
	{
		APS_ERR("null pointer!!\n");
		return;
	}

	atomic_set(&obj->als_suspend, 0);
	if(test_bit(CMC_BIT_ALS, &obj->enable))
	{
	    err = ltr303_als_enable(als_gainrange);
		if(err < 0)
		{
			APS_ERR("enable als fail: %d\n", err);        

		}
	}
}


int ltr303_als_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value;
	hwm_sensor_data* sensor_data;
	struct ltr303_priv *obj = (struct ltr303_priv *)self;

	switch (command)
	{
		case SENSOR_DELAY:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				APS_ERR("Set delay parameter error!\n");
				err = -EINVAL;
			}
			// Do nothing
			break;

		case SENSOR_ENABLE:
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
				    err = ltr303_als_enable(als_gainrange);
					if(err < 0)
					{
						APS_ERR("enable als fail: %d\n", err); 
						return -1;
					}
					set_bit(CMC_BIT_ALS, &obj->enable);
				}
				else
				{
				    err = ltr303_als_disable();
					if(err < 0)
					{
						APS_ERR("disable als fail: %d\n", err); 
						return -1;
					}
					clear_bit(CMC_BIT_ALS, &obj->enable);
				}
				
			}
			break;

		case SENSOR_GET_DATA:
			if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
			{
				APS_ERR("get sensor data parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				//APS_ERR("get sensor als data !\n");
				sensor_data = (hwm_sensor_data *)buff_out;
				obj->als = ltr303_als_read(als_gainrange);
                #if defined(MTK_AAL_SUPPORT)
				sensor_data->values[0] = obj->als;
				#else
				sensor_data->values[0] = ltr303_get_als_value(obj, obj->als);
				#endif
				sensor_data->value_divide = 1;
				sensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
			}
			break;
		default:
			APS_ERR("light sensor operate function no this parameter %d!\n", command);
			err = -1;
			break;
	}
	
	return err;
}


/*----------------------------------------------------------------------------*/
/*
static int ltr303_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info) 
{    
	strcpy(info->type, LTR303_DEV_NAME);
	return 0;
}
*/
/*----------------------------------------------------------------------------*/
static int ltr303_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct ltr303_priv *obj;
	struct hwmsen_object obj_ps, obj_als;
	int err = 0;

	if(!(obj = kzalloc(sizeof(*obj), GFP_KERNEL)))
	{
		err = -ENOMEM;
		goto exit;
	}
	memset(obj, 0, sizeof(*obj));
	ltr303_obj = obj;

	obj->hw = get_cust_alsps_hw();
	ltr303_get_addr(obj->hw, &obj->addr);

	INIT_WORK(&obj->eint_work, ltr303_eint_work);
	obj->client = client;
	i2c_set_clientdata(client, obj);	
	atomic_set(&obj->als_debounce, 300);
	atomic_set(&obj->als_deb_on, 0);
	atomic_set(&obj->als_deb_end, 0);
	//atomic_set(&obj->ps_debounce, 300);
	//atomic_set(&obj->ps_deb_on, 0);
	//atomic_set(&obj->ps_deb_end, 0);
	//atomic_set(&obj->ps_mask, 0);
	atomic_set(&obj->als_suspend, 0);
	//atomic_set(&obj->ps_thd_val_high,  obj->hw->ps_threshold_high);
	//atomic_set(&obj->ps_thd_val_low,  obj->hw->ps_threshold_low);
	//atomic_set(&obj->als_cmd_val, 0xDF);
	//atomic_set(&obj->ps_cmd_val,  0xC1);
	//atomic_set(&obj->ps_thd_val,  obj->hw->ps_threshold);
	obj->enable = 0;
	obj->pending_intr = 0;
	obj->als_level_num = sizeof(obj->hw->als_level)/sizeof(obj->hw->als_level[0]);
	obj->als_value_num = sizeof(obj->hw->als_value)/sizeof(obj->hw->als_value[0]);   
	obj->als_modulus = (400*100)/(16*150);//(1/Gain)*(400/Tine), this value is fix after init ATIME and CONTROL register value
										//(400)/16*2.72 here is amplify *100
	BUG_ON(sizeof(obj->als_level) != sizeof(obj->hw->als_level));
	memcpy(obj->als_level, obj->hw->als_level, sizeof(obj->als_level));
	BUG_ON(sizeof(obj->als_value) != sizeof(obj->hw->als_value));
	memcpy(obj->als_value, obj->hw->als_value, sizeof(obj->als_value));
	atomic_set(&obj->i2c_retry, 3);
	set_bit(CMC_BIT_ALS, &obj->enable);
	set_bit(CMC_BIT_PS, &obj->enable);

	APS_LOG("ltr303_devinit() start...!\n");
	ltr303_i2c_client = client;

	if((err = ltr303_devinit()))
	{
		goto exit_init_failed;
	}
	APS_LOG("ltr303_devinit() ...OK!\n");

	//printk("@@@@@@ manufacturer value:%x\n",ltr303_i2c_read_reg(0x87));

	if((err = misc_register(&ltr303_device)))
	{
		APS_ERR("ltr303_device register failed\n");
		goto exit_misc_device_register_failed;
	}

	
	/* Register sysfs attribute */
	if((err = ltr303_create_attr(&ltr303_alsps_driver.driver)))
	{
		APS_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}


	obj_ps.self = ltr303_obj;
	/*for interrup work mode support -- by liaoxl.lenovo 12.08.2011*/
	
	obj_als.self = ltr303_obj;
	obj_als.polling = 1;
	obj_als.sensor_operate = ltr303_als_operate;
	if((err = hwmsen_attach(ID_LIGHT, &obj_als)))
	{
		APS_ERR("attach fail = %d\n", err);
		goto exit_create_attr_failed;
	}


#if defined(CONFIG_HAS_EARLYSUSPEND)
	obj->early_drv.level    = EARLY_SUSPEND_LEVEL_DISABLE_FB - 1,
	obj->early_drv.suspend  = ltr303_early_suspend,
	obj->early_drv.resume   = ltr303_late_resume,    
	register_early_suspend(&obj->early_drv);
#endif

	APS_LOG("%s: OK\n", __func__);
	return 0;

	exit_create_attr_failed:
	misc_deregister(&ltr303_device);
	exit_misc_device_register_failed:
	exit_init_failed:
	//i2c_detach_client(client);
	//exit_kfree:
	kfree(obj);
	exit:
	ltr303_i2c_client = NULL;           
//	MT6516_EINTIRQMask(CUST_EINT_ALS_NUM);  /*mask interrupt if fail*/
	APS_ERR("%s: err = %d\n", __func__, err);
	return err;
}

/*----------------------------------------------------------------------------*/

static int ltr303_i2c_remove(struct i2c_client *client)
{
	int err;	
	if((err = ltr303_delete_attr(&ltr303_i2c_driver.driver)))
	{
		APS_ERR("ltr303_delete_attr fail: %d\n", err);
	} 

	if((err = misc_deregister(&ltr303_device)))
	{
		APS_ERR("misc_deregister fail: %d\n", err);    
	}
	
	ltr303_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));

	return 0;
}
/*----------------------------------------------------------------------------*/
static int ltr303_probe(struct platform_device *pdev) 
{
	struct alsps_hw *hw = get_cust_alsps_hw();

	ltr303_power(hw, 1);
	//ltr303_force[0] = hw->i2c_num;
	//ltr303_force[1] = hw->i2c_addr[0];
	//APS_DBG("I2C = %d, addr =0x%x\n",ltr303_force[0],ltr303_force[1]);
	if(i2c_add_driver(&ltr303_i2c_driver))
	{
		APS_ERR("add driver error\n");
		return -1;
	} 
	return 0;
}
/*----------------------------------------------------------------------------*/
static int ltr303_remove(struct platform_device *pdev)
{
	struct alsps_hw *hw = get_cust_alsps_hw();
	APS_FUN();    
	ltr303_power(hw, 0);    
	i2c_del_driver(&ltr303_i2c_driver);
	return 0;
}
/*----------------------------------------------------------------------------*/
static struct platform_driver ltr303_alsps_driver = {
	.probe      = ltr303_probe,
	.remove     = ltr303_remove,    
	.driver     = {
		.name  = "als_ps",
		//.owner = THIS_MODULE,
	}
};
/*----------------------------------------------------------------------------*/
static int __init ltr303_init(void)
{
	struct alsps_hw *hw = get_cust_alsps_hw();

	APS_FUN();
	i2c_register_board_info(hw->i2c_num, &i2c_ltr303, 1);
	if(platform_driver_register(&ltr303_alsps_driver))
	{
		APS_ERR("failed to register driver");
		return -ENODEV;
	}
	return 0;
}
/*----------------------------------------------------------------------------*/
static void __exit ltr303_exit(void)
{
	APS_FUN();
	platform_driver_unregister(&ltr303_alsps_driver);
}
/*----------------------------------------------------------------------------*/
module_init(ltr303_init);
module_exit(ltr303_exit);
/*----------------------------------------------------------------------------*/
MODULE_AUTHOR("XX Xx");
MODULE_DESCRIPTION("LTR-303ALS Driver");
MODULE_LICENSE("GPL");

