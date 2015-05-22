/* drivers/hwmon/mt6516/amit/em3071.c - em3071 ALS/PS driver
 * 
 * Author: 
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
#if 0 //def MT6516
#include <mach/mt6516_devs.h>
#include <mach/mt6516_typedefs.h>
#include <mach/mt6516_gpio.h>
#include <mach/mt6516_pll.h>
#define POWER_NONE_MACRO MT6516_POWER_NONE

#endif

#if 0 //def MT6573
#include <mach/mt6573_devs.h>
#include <mach/mt6573_typedefs.h>
#include <mach/mt6573_gpio.h>
#include <mach/mt6573_pll.h>
#define POWER_NONE_MACRO MT65XX_POWER_NONE

#endif

#if 0//def MT6575
#include <mach/mt6575_devs.h>
#include <mach/mt6575_typedefs.h>
#include <mach/mt6575_gpio.h>
#include <mach/mt6575_pm_ldo.h>
#endif

#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>
#include <mach/eint.h>


#if 1 //defined(MT6575)||defined(MT6589)
#define POWER_NONE_MACRO MT65XX_POWER_NONE
#endif

//temp
#if 1 //it should be defined with dct 
#define CUST_EINT_ALS_NUM            1  
#define CUST_EINT_ALS_DEBOUNCE_CN      0
#define CUST_EINT_ALS_POLARITY         CUST_EINT_POLARITY_LOW
#define CUST_EINT_ALS_SENSITIVE        CUST_EINT_LEVEL_SENSITIVE
#define CUST_EINT_ALS_DEBOUNCE_EN      CUST_EINT_DEBOUNCE_DISABLE
#endif



#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include <asm/io.h>
#include <cust_eint.h>
#include <cust_alsps.h>
#include "em3071.h"
/******************************************************************************
 * configuration
*******************************************************************************/
/*----------------------------------------------------------------------------*/

#define EM3071_DEV_NAME     "EM3071"
/*----------------------------------------------------------------------------*/
#define APS_TAG                  "[ALS/PS] "
#define APS_FUN(f)               printk(APS_TAG"%s %d \n", __FUNCTION__ , __LINE__)
#define APS_ERR(fmt, args...)    printk(APS_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define APS_LOG(fmt, args...)    printk(APS_TAG fmt, ##args)
#define APS_DBG(fmt, args...)    printk(APS_TAG fmt, ##args)                 
/******************************************************************************
 * extern functions
*******************************************************************************/
/*for interrup work mode support --*/
#if 0 //def MT6575
	extern void mt65xx_eint_unmask(unsigned int line);
	extern void mt65xx_eint_mask(unsigned int line);
	extern void mt65xx_eint_set_polarity(kal_uint8 eintno, kal_bool ACT_Polarity);
	extern void mt65xx_eint_set_hw_debounce(kal_uint8 eintno, kal_uint32 ms);
	extern kal_uint32 mt65xx_eint_set_sens(kal_uint8 eintno, kal_bool sens);
	extern void mt65xx_eint_registration(kal_uint8 eintno, kal_bool Dbounce_En,
										 kal_bool ACT_Polarity, void (EINT_FUNC_PTR)(void),
										 kal_bool auto_umask);
	
#endif
#if 0 //def MT6516
extern void MT6516_EINTIRQUnmask(unsigned int line);
extern void MT6516_EINTIRQMask(unsigned int line);
extern void MT6516_EINT_Set_Polarity(kal_uint8 eintno, kal_bool ACT_Polarity);
extern void MT6516_EINT_Set_HW_Debounce(kal_uint8 eintno, kal_uint32 ms);
extern kal_uint32 MT6516_EINT_Set_Sensitivity(kal_uint8 eintno, kal_bool sens);
extern void MT6516_EINT_Registration(kal_uint8 eintno, kal_bool Dbounce_En,
                                     kal_bool ACT_Polarity, void (EINT_FUNC_PTR)(void),
                                     kal_bool auto_umask);
#endif

#define PS_DRIVE 0XA1  //50MA  //0XB8 //200MA  0XB6 100MA  //Psensor 驱动力
/*----------------------------------------------------------------------------*/
static struct i2c_client *em3071_i2c_client = NULL;
/*----------------------------------------------------------------------------*/
static const struct i2c_device_id em3071_i2c_id[] = {{EM3071_DEV_NAME,0},{}};
/*the adapter id & i2c address will be available in customization*/
//static unsigned short em3071_force[] = {0x00, 0x48, I2C_CLIENT_END, I2C_CLIENT_END};
//static const unsigned short *const em3071_forces[] = { em3071_force, NULL };
//static struct i2c_client_address_data em3071_addr_data = { .forces = em3071_forces,};
/*----------------------------------------------------------------------------*/

static struct i2c_board_info __initdata i2c_em3071={ I2C_BOARD_INFO(EM3071_DEV_NAME, 0x48>>1)};

static int em3071_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id); 
static int em3071_i2c_remove(struct i2c_client *client);
static int em3071_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);
/*----------------------------------------------------------------------------*/
static int em3071_i2c_suspend(struct i2c_client *client, pm_message_t msg);
static int em3071_i2c_resume(struct i2c_client *client);
int em3071_setup_eint(struct i2c_client *client);

static struct em3071_priv *g_em3071_ptr = NULL;


static int intr_flag_value = 0;

static DEFINE_MUTEX(sensor_lock);
/*----------------------------------------------------------------------------*/
typedef enum {
    CMC_BIT_ALS    = 1,
    CMC_BIT_PS     = 2,
} CMC_BIT;
/*----------------------------------------------------------------------------*/
struct em3071_i2c_addr {    /*define a series of i2c slave address*/
    u8  write_addr;  
    u8  ps_thd;     /*PS INT threshold*/
};
/*----------------------------------------------------------------------------*/
struct em3071_priv {
    struct alsps_hw  *hw;
    struct i2c_client *client;
    struct work_struct  eint_work;

    /*i2c address group*/
    struct em3071_i2c_addr  addr;
    
    /*misc*/
//    u16		    als_modulus;
    atomic_t    i2c_retry;
    atomic_t    als_suspend;
    atomic_t    als_debounce;   /*debounce time after enabling als*/
    atomic_t    als_deb_on;     /*indicates if the debounce is on*/
    atomic_t    als_deb_end;    /*the jiffies representing the end of debounce*/
    atomic_t    ps_mask;        /*mask ps: always return far away*/
    atomic_t    ps_debounce;    /*debounce time after enabling ps*/
    atomic_t    ps_deb_on;      /*indicates if the debounce is on*/
    atomic_t    ps_deb_end;     /*the jiffies representing the end of debounce*/
    atomic_t    ps_suspend;


    /*data*/
    u16         als;
    u16          ps;
    u8          _align;
    u16         als_level_num;
    u16         als_value_num;
    u32         als_level[C_CUST_ALS_LEVEL-1];
    u32         als_value[C_CUST_ALS_LEVEL];

    atomic_t    als_cmd_val;    /*the cmd value can't be read, stored in ram*/
    atomic_t    ps_cmd_val;     /*the cmd value can't be read, stored in ram*/
    atomic_t    ps_thd_val;     /*the cmd value can't be read, stored in ram*/
    ulong       enable;         /*enable mask*/
    ulong       pending_intr;   /*pending interrupt*/

    /*early suspend*/
#if defined(CONFIG_HAS_EARLYSUSPEND)
    struct early_suspend    early_drv;
#endif     
};
/*----------------------------------------------------------------------------*/
static struct i2c_driver em3071_i2c_driver = {	
	.probe      = em3071_i2c_probe,
	.remove     = em3071_i2c_remove,
	//.detect     = em3071_i2c_detect,
	.suspend    = em3071_i2c_suspend,
	.resume     = em3071_i2c_resume,
	.id_table   = em3071_i2c_id,
	//.address_data = &em3071_addr_data,
	.driver = {
		.owner          = THIS_MODULE,
		.name           = EM3071_DEV_NAME,
	},
};

static int ps_enabled=0;

static struct em3071_priv *em3071_obj = NULL;
static struct platform_driver em3071_alsps_driver;
/*----------------------------------------------------------------------------*/
int em3071_get_addr(struct alsps_hw *hw, struct em3071_i2c_addr *addr)
{
	if(!hw || !addr)
	{
		return -EFAULT;
	}
	addr->write_addr= hw->i2c_addr[0];
	return 0;
}
/*----------------------------------------------------------------------------*/
static void em3071_power(struct alsps_hw *hw, unsigned int on) 
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
			if(!hwPowerOn(hw->power_id, hw->power_vol, "EM3071")) 
			{
				APS_ERR("power on fails!!\n");
			}
		}
		else
		{
			if(!hwPowerDown(hw->power_id, "EM3071")) 
			{
				APS_ERR("power off fail!!\n");   
			}
		}
	}

#ifdef GPIO_ALS_PWREN_PIN
    if(power_on == on)
    {
            APS_LOG("ignore power control: %d\n", on);
    }
    else if( on)
    {
        mt_set_gpio_mode(GPIO_ALS_PWREN_PIN, GPIO_ALS_PWREN_PIN_M_GPIO);
        mt_set_gpio_dir(GPIO_ALS_PWREN_PIN, GPIO_DIR_OUT);
        mt_set_gpio_out(GPIO_ALS_PWREN_PIN, 1);
        msleep(100);
    }
    else
    {
        mt_set_gpio_mode(GPIO_ALS_PWREN_PIN, GPIO_ALS_PWREN_PIN_M_GPIO);
        mt_set_gpio_dir(GPIO_ALS_PWREN_PIN, GPIO_DIR_OUT);
        mt_set_gpio_out(GPIO_ALS_PWREN_PIN, 0);
    }
#endif
	power_on = on;
}

/*----------------------------------------------------------------------------*/
static int em3071_enable_als(struct i2c_client *client, int enable)
{
        struct em3071_priv *obj = i2c_get_clientdata(client);
        u8 databuf[2];      
        int res = 0;
        u8 reg_value[1];
        u8 buffer[2];
        if(client == NULL)
        {
            APS_DBG("CLIENT CANN'T EQUL NULL\n");
            return -1;
        }

	      databuf[0] = EM3071_CMM_ENABLE;    
            res = i2c_master_send(client, databuf, 0x1);
            if(res <= 0)
            {
                goto EXIT_ERR;
            }
            res = i2c_master_recv(client, databuf, 0x1);
            if(res <= 0)
            {
                goto EXIT_ERR;
            }

        if(enable)
        {			
            databuf[1] = ((databuf[0] & 0xF8) | 0x06);
    
            databuf[0] = EM3071_CMM_ENABLE;    
            //databuf[1] = 0xBE;
            res = i2c_master_send(client, databuf, 0x2);
            if(res <= 0)
            {
                goto EXIT_ERR;
            }
           
            atomic_set(&obj->als_deb_on, 1);
            atomic_set(&obj->als_deb_end, jiffies+atomic_read(&obj->als_debounce)/(1000/HZ));
            APS_DBG("em3071 power on\n");
        }
        else
        {
            
            databuf[1] = (databuf[0] & 0xF8);
                     
            databuf[0] = EM3071_CMM_ENABLE;    
        //    databuf[1] = 0xB8;    
            res = i2c_master_send(client, databuf, 0x2);
            if(res <= 0)
            {
                goto EXIT_ERR;
            }
            atomic_set(&obj->als_deb_on, 0);
            APS_DBG("EM3071 power off\n");
        }
		
        return 0;
        
    EXIT_ERR:
        APS_ERR("EM3071_enable_als fail\n");
        return res;
}

/*----------------------------------------------------------------------------*/
static int em3071_enable_ps(struct i2c_client *client, int enable)
{
	struct em3071_priv *obj = i2c_get_clientdata(client);
	u8 databuf[2];    
	int res = 0;
	u8 reg_value[1];
	u8 buffer[2];

	if(client == NULL)
	{
		APS_DBG("CLIENT CANN'T EQUL NULL\n");
		return -1;
	}

	
	APS_DBG("em3071_enable_ps, enable = %d\n", enable);

		databuf[0] = EM3071_CMM_ENABLE;	
		res = i2c_master_send(client, databuf, 0x1);
		if(res <= 0)
		{
			goto EXIT_ERR;
		}
		res = i2c_master_recv(client, databuf, 0x1);
		if(res <= 0)
		{
			goto EXIT_ERR;
		}

	if(enable)
	{
		databuf[1] = ((databuf[0] & 0x07) | PS_DRIVE);  //0xb8 :200MA	 0XB6:100MA 
			
		databuf[0] = EM3071_CMM_ENABLE;    

		res = i2c_master_send(client, databuf, 0x2);
		if(res <= 0)
		{
			goto EXIT_ERR;
		}
		atomic_set(&obj->ps_deb_on, 1);
		atomic_set(&obj->ps_deb_end, jiffies+atomic_read(&obj->ps_debounce)/(1000/HZ));
		APS_DBG("em3071 power on\n");

		/*for interrup work mode support */
		if(0 == obj->hw->polling_mode_ps)
		{

				databuf[0] = EM3071_CMM_INT_PS_LT;	
				databuf[1] = (u8)(((atomic_read(&obj->ps_thd_val))-40) & 0x00FF);
				res = i2c_master_send(client, databuf, 0x2);
				if(res <= 0)
				{
					goto EXIT_ERR;
					return EM3071_ERR_I2C;
				}
				databuf[0] = EM3071_CMM_INT_PS_HT;	
				databuf[1] = (u8)((atomic_read(&obj->ps_thd_val)) & 0x00FF);
				res = i2c_master_send(client, databuf, 0x2);
				if(res <= 0)
				{
					goto EXIT_ERR;
					return EM3071_ERR_I2C;
				}
			//zijian 
			//printk("zijian em3071_enable_ps set interrupt\n");	

				mt65xx_eint_unmask(CUST_EINT_ALS_NUM);
			
		}
	}
	else
	{
		databuf[1] = (databuf[0] & 0x07);
		
		databuf[0] = EM3071_CMM_ENABLE;    
                       
		res = i2c_master_send(client, databuf, 0x2);
		if(res <= 0)
		{
			goto EXIT_ERR;
		}
		atomic_set(&obj->ps_deb_on, 0);
		APS_DBG("em3071 power off\n");

		/*for interrup work mode support */
		if(0 == obj->hw->polling_mode_ps)
		{
			cancel_work_sync(&obj->eint_work);
			mt65xx_eint_mask(CUST_EINT_ALS_NUM);
		}
	}
	ps_enabled=enable;
	return 0;
	
EXIT_ERR:
	APS_ERR("em3071_enable_ps fail\n");
	return res;
}
/*----------------------------------------------------------------------------*/
static int em3071_enable(struct i2c_client *client, int enable)
{
	struct em3071_priv *obj = i2c_get_clientdata(client);
	u8 databuf[2];    
	int res = 0;
	//u8 buffer[1];
	u8 buffer[2];
	u8 reg_value[1];

	if(client == NULL)
	{
		APS_DBG("CLIENT CANN'T EQUL NULL\n");
		return -1;
	}

	if(enable)
	{
		databuf[0] = EM3071_CMM_ENABLE;    
		databuf[1] = PS_DRIVE;//0XBE 200MA ,B6 100MA ,
		res = i2c_master_send(client, databuf, 0x2);
		if(res <= 0)
		{
			goto EXIT_ERR;
		}
		databuf[0] = 0x0F;    
		databuf[1] = 0x00;
		res = i2c_master_send(client, databuf, 0x2);
		if(res <= 0)
		{
			goto EXIT_ERR;
		}
		APS_DBG("EM3071 power on\n");
	}
	else
	{
		databuf[0] = EM3071_CMM_ENABLE;    
		databuf[1] = 0x00;
		res = i2c_master_send(client, databuf, 0x2);
		if(res <= 0)
		{
			goto EXIT_ERR;
		}
		atomic_set(&obj->ps_deb_on, 0);
		atomic_set(&obj->als_deb_on, 0);
		APS_DBG("EM3071 power off\n");
	}
	return 0;
	
EXIT_ERR:
	APS_ERR("EM3071_enable fail\n");
	return res;
}

/*----------------------------------------------------------------------------*/
/*for interrup work mode support --*/
static int em3071_check_and_clear_intr(struct i2c_client *client) 
{
	struct em3071_priv *obj = i2c_get_clientdata(client);
	int res,intp,intl;
	u8 buffer[2];

	//if (mt_get_gpio_in(GPIO_ALS_EINT_PIN) == 1) /*skip if no interrupt*/  
	//    return 0;
	
	buffer[0] = EM3071_CMM_STATUS;
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
	//APS_ERR("em3071_check_and_clear_intr status=0x%x\n", buffer[0]);
	res = 1;
	intp = 0;
	intl = 0;
	if(0 != (buffer[0] & 0x80))
	{
		res = 0;
		intp = 1;
	}

	if(0 == res)
	{
		if(1 == intp)
		{
			buffer[0] = EM3071_CMM_STATUS;
			buffer[1] = 0x00;
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
		
	}

	return res;

EXIT_ERR:
	APS_ERR("em3071_check_and_clear_intr fail\n");
	return 1;
}
/*----------------------------------------------------------------------------*/
void em3071_eint_func(void)
{
	struct em3071_priv *obj = g_em3071_ptr;
	if(!obj)
	{
		return;
	}
//zijian test
//	printk("zijian , em3071_eint_func \n");
	schedule_work(&obj->eint_work);
}

/*----------------------------------------------------------------------------*/
/*for interrup work mode support --*/
int em3071_setup_eint(struct i2c_client *client)
{
	struct em3071_priv *obj = i2c_get_clientdata(client);        

	g_em3071_ptr = obj;
	
	mt_set_gpio_dir(GPIO_ALS_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_mode(GPIO_ALS_EINT_PIN, GPIO_ALS_EINT_PIN_M_EINT);
	mt_set_gpio_pull_enable(GPIO_ALS_EINT_PIN, TRUE);
	mt_set_gpio_pull_select(GPIO_ALS_EINT_PIN, GPIO_PULL_UP);

	mt65xx_eint_set_sens(CUST_EINT_ALS_NUM, CUST_EINT_ALS_SENSITIVE);
	mt65xx_eint_set_polarity(CUST_EINT_ALS_NUM, CUST_EINT_ALS_POLARITY);
	mt65xx_eint_set_hw_debounce(CUST_EINT_ALS_NUM, CUST_EINT_ALS_DEBOUNCE_CN);
	mt65xx_eint_registration(CUST_EINT_ALS_NUM, CUST_EINT_ALS_DEBOUNCE_EN, CUST_EINT_ALS_POLARITY, em3071_eint_func, 0);

	mt65xx_eint_unmask(CUST_EINT_ALS_NUM);  
    return 0;
}

static int em3071_init_client(struct i2c_client *client)
{
	struct em3071_priv *obj = i2c_get_clientdata(client);
	u8 databuf[2];    
	int res = 0;
	  	   
	   databuf[0] = EM3071_CMM_ENABLE;	 
	   databuf[1] = PS_DRIVE;//0XBE 200MA
	   res = i2c_master_send(client, databuf, 0x2);
	   if(res <= 0)
	   {
			APS_FUN();
		   goto EXIT_ERR;
	   }
	   databuf[0] = 0x0F;	 
	   databuf[1] = 0x00;
	   res = i2c_master_send(client, databuf, 0x2);
	   if(res <= 0)
	   {
	  	   APS_FUN();
		   goto EXIT_ERR;
	   }
	   	/*for interrup work mode support*/
		if(0 == obj->hw->polling_mode_ps)
		{
			databuf[0] = EM3071_CMM_INT_PS_LT;	
			databuf[1] = (u8)(((atomic_read(&obj->ps_thd_val))-40) & 0x00FF);
			res = i2c_master_send(client, databuf, 0x2);
			if(res <= 0)
			{
			    
				APS_FUN();
				goto EXIT_ERR;
				return EM3071_ERR_I2C;
			}
			databuf[0] = EM3071_CMM_INT_PS_HT;	
			databuf[1] = (u8)((atomic_read(&obj->ps_thd_val)) & 0x00FF);
			res = i2c_master_send(client, databuf, 0x2);
			if(res <= 0)
			{
				 APS_FUN();
				goto EXIT_ERR;
				return EM3071_ERR_I2C;
			}

		mt65xx_eint_unmask(CUST_EINT_ALS_NUM);
	}

	/*for interrup work mode support */
	if(res = em3071_setup_eint(client))
	{
		APS_ERR("setup eint: %d\n", res);
		return res;
	}
	if(res = em3071_check_and_clear_intr(client))
	{
		APS_ERR("check/clear intr: %d\n", res);
		//    return res;
	}
	
	return EM3071_SUCCESS;

EXIT_ERR:
	APS_ERR("init dev: %d\n", res);
	return res;
}

/****************************************************************************** 
**
*****************************************************************************/
int em3071_read_als(struct i2c_client *client, u16 *data)
{
	struct em3071_priv *obj = i2c_get_clientdata(client);	 
	u16 c0_value;	 
	u8 als_value_low[1], als_value_high[1];
	u8 buffer[1];
	u16 atio;
	u16 als_value;
	int res = 0;
	
	if(client == NULL)
	{
		APS_DBG("CLIENT CANN'T EQUL NULL\n");
		return -1;
	}
//get adc channel 0 value
	buffer[0]=EM3071_CMM_C0DATA_L;
	res = i2c_master_send(client, buffer, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	res = i2c_master_recv(client, als_value_low, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	
	buffer[0]=EM3071_CMM_C0DATA_H;
	res = i2c_master_send(client, buffer, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	res = i2c_master_recv(client, als_value_high, 0x01);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	
	c0_value = als_value_low[0] | ((0X0F&als_value_high[0])<<8);
	*data = c0_value;
	printk("jack alps ===c0_value = %d\t\n",c0_value);
	return 0;	 

	
	
EXIT_ERR:
	APS_ERR("em3071_read_ps fail\n");
	return res;
}
/*----------------------------------------------------------------------------*/

static int em3071_get_als_value(struct em3071_priv *obj, u16 als)
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
		//APS_DBG("ALS: %05d => %05d\n", als, obj->hw->als_value[idx]);	
		return obj->hw->als_value[idx];
	}
	else
	{
		//APS_ERR("ALS: %05d => %05d (-1)\n", als, obj->hw->als_value[idx]);    
		return -1;
	}
}
/*----------------------------------------------------------------------------*/
int em3071_read_ps(struct i2c_client *client, u16 *data)
{
	struct em3071_priv *obj = i2c_get_clientdata(client);    
	u16 ps_value;    
	u8 ps_value_low[1];
	u8 buffer[1];
	int res = 0;

	if(client == NULL)
	{
		APS_DBG("CLIENT CANN'T EQUL NULL\n");
		return -1;
	}

	buffer[0]=EM3071_CMM_PDATA_L;
	res = i2c_master_send(client, buffer, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	res = i2c_master_recv(client, ps_value_low, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}

	*data = ps_value_low[0];
	//zijian 
	//APS_DBG("ps_data=%d, low:%d \n ", *data, ps_value_low[0]);
	return 0;    

EXIT_ERR:
	APS_ERR("em3071_read_ps fail\n");
	return res;
}
/*----------------------------------------------------------------------------*/
static int em3071_get_ps_value(struct em3071_priv *obj, u16 ps)
{
	int val, mask = atomic_read(&obj->ps_mask);
	int invalid = 0;
	static int val_temp=1;
	u16 temp_ps[1];

#if 1//zijian
	temp_ps[0] = ps;
#else
	mdelay(160);
	em3071_read_ps(obj->client,temp_ps);
#endif
	//zijian
	/*
	if(temp_ps == 0)
	{
		return -1;
	}*/
	
	if((ps > atomic_read(&obj->ps_thd_val))&&(temp_ps[0]  >atomic_read(&obj->ps_thd_val)))
	{
		val = 0;  /*close*/
		val_temp = 0;
		intr_flag_value = 1;
	}
	else if((ps < (atomic_read(&obj->ps_thd_val)-40))&&(temp_ps[0]  < (atomic_read(&obj->ps_thd_val)-40)))
	{
		val = 1;  /*far away*/
		val_temp = 1;
		intr_flag_value = 0;
	}
	else
	       val = val_temp;	
				
	if(atomic_read(&obj->ps_suspend))
	{
		invalid = 1;
	}
	else if(1 == atomic_read(&obj->ps_deb_on))
	{
		unsigned long endt = atomic_read(&obj->ps_deb_end);
		if(time_after(jiffies, endt))
		{
			atomic_set(&obj->ps_deb_on, 0);
		}
		
		if (1 == atomic_read(&obj->ps_deb_on))
		{
			invalid = 1;
		}
	}
	else if (obj->als > 45000)
	{
		//invalid = 1;
		APS_DBG("ligh too high will result to failt proximiy\n");
		return 1;  /*far away*/
	}

	if(!invalid)
	{
		//APS_DBG("PS:  %05d => %05d\n", ps, val);
		return val;
	}	
	else
	{
		return -1;
	}	
}

/*for interrup work mode support --*/
static void em3071_eint_work(struct work_struct *work)
{
	struct em3071_priv *obj = (struct em3071_priv *)container_of(work, struct em3071_priv, eint_work);
	int err;
	hwm_sensor_data sensor_data;
	u8 buffer[1];
	u8 reg_value[1];
	u8 databuf[2];
	int res = 0;
	u8  need_drop = 0;
	
	if((err = em3071_check_and_clear_intr(obj->client)))
	{
		APS_ERR("em3071_eint_work check intrs: %d\n", err);
	}
	else
	{
		//get raw data
		em3071_read_ps(obj->client, &obj->ps);
		
		//zijian
		/*
		if(obj->ps == 0)
		{
			need_drop = 1 ;
			APS_FUN();   //no interrupt after this	
			goto EXIT_ERR ;//return ;
		}*/
		
		//mdelay(160);
		em3071_read_als(obj->client, &obj->als);
		APS_DBG("em3071_eint_work rawdata ps=%d als_ch0=%d!\n",obj->ps,obj->als);
		sensor_data.values[0] = em3071_get_ps_value(obj, obj->ps);
		sensor_data.value_divide = 1;
		sensor_data.status = SENSOR_STATUS_ACCURACY_MEDIUM;			
/*singal interrupt function add*/
#if 1
		if(intr_flag_value){
				//printk(" interrupt value ps will > 90");
			
					databuf[0] = EM3071_CMM_INT_PS_LT;	
					databuf[1] = (u8)(((atomic_read(&obj->ps_thd_val))-40) & 0x00FF);
					res = i2c_master_send(obj->client, databuf, 0x2);
					if(res <= 0)
					{
						APS_FUN();
						goto EXIT_ERR ;//return;
					}
				
				databuf[0] = EM3071_CMM_INT_PS_HT;		
				databuf[1] = (u8)(0x00FF);
				res = i2c_master_send(obj->client, databuf, 0x2);
				if(res <= 0)
				{
					APS_FUN();
					goto EXIT_ERR ;//return;
				}


		}
		else{	
				//printk(" interrupt value ps will < 90");
											
					databuf[0] = EM3071_CMM_INT_PS_HT;
					databuf[1] = (u8)((atomic_read(&obj->ps_thd_val)) & 0x00FF);
					res = i2c_master_send(obj->client, databuf, 0x2);
					if(res <= 0)
					{
					     APS_FUN();
						goto EXIT_ERR ;//return;
					}
				
		}
#endif
		//let up layer to know

		if((err = hwmsen_get_interrupt_data(ID_PROXIMITY, &sensor_data)))
		{
	  		APS_ERR("call hwmsen_get_interrupt_data fail = %d\n", err);
		}
		
	}

	//printk("zijian em3071_eint_work ,mt65xx_eint_unmask\n ");
	mt65xx_eint_unmask(CUST_EINT_ALS_NUM);   
	return;

EXIT_ERR:
	//printk("zijian em3071_eint_work ,mt65xx_eint_unmask\n ");	
	mt65xx_eint_unmask(CUST_EINT_ALS_NUM);	 
	APS_ERR("em3071_eint_work error \n");
	return;
}


/****************************************************************************** 
 * Function Configuration
******************************************************************************/
static int em3071_open(struct inode *inode, struct file *file)
{
	file->private_data = em3071_i2c_client;

	if (!file->private_data)
	{
		APS_ERR("null pointer!!\n");
		return -EINVAL;
	}
	
	return nonseekable_open(inode, file);
}
/*----------------------------------------------------------------------------*/
static int em3071_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}


/*************************************************************/

static int em3071_ioctl( struct file *file, unsigned int cmd,
       unsigned long arg)
{
	struct i2c_client *client = (struct i2c_client*)file->private_data;
	struct em3071_priv *obj = i2c_get_clientdata(client);  
	int err = 0;
	void __user *ptr = (void __user*) arg;
	int dat;
	uint32_t enable;

	switch (cmd)
	{
		case ALSPS_SET_PS_MODE:
			if(copy_from_user(&enable, ptr, sizeof(enable)))
			{
				err = -EFAULT;
				goto err_out;
			}
			if(enable)
			{
				if(err = em3071_enable_ps(obj->client, 1))
				{
					APS_ERR("enable ps fail: %d\n", err); 
					goto err_out;
				}
				
				set_bit(CMC_BIT_PS, &obj->enable);
			}
			else
			{
				if(err = em3071_enable_ps(obj->client, 0))
				{
					APS_ERR("disable ps fail: %d\n", err); 
					goto err_out;
				}
				
				clear_bit(CMC_BIT_PS, &obj->enable);
			}
			break;

		case ALSPS_GET_PS_MODE:
			enable = test_bit(CMC_BIT_PS, &obj->enable) ? (1) : (0);
			if(copy_to_user(ptr, &enable, sizeof(enable)))
			{
				err = -EFAULT;
				goto err_out;
			}
			break;

		case ALSPS_GET_PS_DATA:    
			if(err = em3071_read_ps(obj->client, &obj->ps))
			{
				goto err_out;
			}
			
			dat = em3071_get_ps_value(obj, obj->ps);
			if(dat == -1)
			{
				err = -EFAULT;
				goto err_out;
			}
			
			if(copy_to_user(ptr, &dat, sizeof(dat)))
			{
				err = -EFAULT;
				goto err_out;
			}  
			break;

		case ALSPS_GET_PS_RAW_DATA:    
			if(err = em3071_read_ps(obj->client, &obj->ps))
			{
				goto err_out;
			}
			//zijian
			/*
		 	if(obj->ps == 0)
			{
				err = -EFAULT;
				goto err_out;
			}*/
			
			dat = obj->ps;
			if(copy_to_user(ptr, &dat, sizeof(dat)))
			{
				err = -EFAULT;
				goto err_out;
			}  
			break;              

		case ALSPS_SET_ALS_MODE:
			if(copy_from_user(&enable, ptr, sizeof(enable)))
			{
				err = -EFAULT;
				goto err_out;
			}
			if(enable)
			{
				if(err = em3071_enable_als(obj->client, 1))
				{
					APS_ERR("enable als fail: %d\n", err); 
					goto err_out;
				}
				set_bit(CMC_BIT_ALS, &obj->enable);
			}
			else
			{
				if(err = em3071_enable_als(obj->client, 0))
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
			if(err = em3071_read_als(obj->client, &obj->als))
			{
				goto err_out;
			}

			dat = em3071_get_als_value(obj, obj->als);
			if(copy_to_user(ptr, &dat, sizeof(dat)))
			{
				err = -EFAULT;
				goto err_out;
			}              
			break;

		case ALSPS_GET_ALS_RAW_DATA:    
			if(err = em3071_read_als(obj->client, &obj->als))
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
static struct file_operations em3071_fops = {
	.owner = THIS_MODULE,
	.open = em3071_open,
	.release = em3071_release,
	.unlocked_ioctl = em3071_ioctl,
};
/*----------------------------------------------------------------------------*/
static struct miscdevice em3071_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "als_ps",
	.fops = &em3071_fops,
};
/*----------------------------------------------------------------------------*/
static int em3071_i2c_suspend(struct i2c_client *client, pm_message_t msg) 
{
//	struct em3071_priv *obj = i2c_get_clientdata(client);    
//	int err;
	APS_FUN(); 
	if(ps_enabled)
	{
		return -EACCES;
	}
	
	return 0;
}
/*----------------------------------------------------------------------------*/
static int em3071_i2c_resume(struct i2c_client *client)
{
//	struct em3071_priv *obj = i2c_get_clientdata(client);        
	APS_FUN();
	return 0;
}
/*----------------------------------------------------------------------------*/
static void em3071_early_suspend(struct early_suspend *h) 
{   /*early_suspend is only applied for ALS*/
	struct em3071_priv *obj = container_of(h, struct em3071_priv, early_drv);   
	int err;
	APS_FUN();    

	if(!obj)
	{
		APS_ERR("null pointer!!\n");
		return;
	}

	#if 1
	
	atomic_set(&obj->als_suspend, 1);
	
	mutex_lock(&sensor_lock);
		if(err = em3071_enable_als(obj->client, 0))
		{
			APS_ERR("disable als fail: %d\n", err); 
		}
	mutex_unlock(&sensor_lock);
		
	#endif
}
/*----------------------------------------------------------------------------*/
static void em3071_late_resume(struct early_suspend *h)
{   /*late_resume is only applied for ALS*/
	struct em3071_priv *obj = container_of(h, struct em3071_priv, early_drv);         
	int err;
	APS_FUN();

	if(!obj)
	{
		APS_ERR("null pointer!!\n");
		return;
	}

        #if 1
		
	atomic_set(&obj->als_suspend, 0);
	if(test_bit(CMC_BIT_ALS, &obj->enable))
	{
		mutex_lock(&sensor_lock);		
		if(err = em3071_enable_als(obj->client, 1))
		{
			APS_ERR("enable als fail: %d\n", err);        

		}
		
		mutex_unlock(&sensor_lock);
	}
	#endif
}

int em3071_ps_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value;
	hwm_sensor_data* sensor_data;
	struct em3071_priv *obj = (struct em3071_priv *)self;
	
	//APS_FUN(f);
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
				
					mutex_lock(&sensor_lock);
					if(err = em3071_enable_ps(obj->client, 1))
					{
						APS_ERR("enable ps fail: %d\n", err); 
						
						mutex_unlock(&sensor_lock);
						return -1;
					}
					mutex_unlock(&sensor_lock);
					
					set_bit(CMC_BIT_PS, &obj->enable);
					
				}
				else
				{
				
					mutex_lock(&sensor_lock);
					if(err = em3071_enable_ps(obj->client, 0))
					{
						APS_ERR("disable ps fail: %d\n", err); 
						
						mutex_unlock(&sensor_lock);
						return -1;
					}
					
					mutex_unlock(&sensor_lock);
					clear_bit(CMC_BIT_PS, &obj->enable);
					
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
				sensor_data = (hwm_sensor_data *)buff_out;	
				
				mutex_lock(&sensor_lock);
				em3071_read_ps(obj->client, &obj->ps);
				
                                //mdelay(160);
				em3071_read_als(obj->client, &obj->als);
								
				mutex_unlock(&sensor_lock);
				APS_ERR("em3071_ps_operate als data=%d!\n",obj->als);
				sensor_data->values[0] = em3071_get_ps_value(obj, obj->ps);
				sensor_data->value_divide = 1;
				sensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;			
			}
			break;
		default:
			APS_ERR("proxmy sensor operate function no this parameter %d!\n", command);
			err = -1;
			break;
	}
	
	return err;
}

int em3071_als_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value;
	hwm_sensor_data* sensor_data;
	struct em3071_priv *obj = (struct em3071_priv *)self;

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
				
					mutex_lock(&sensor_lock);
					if(err = em3071_enable_als(obj->client, 1))
					{
						APS_ERR("enable als fail: %d\n", err); 
						
						mutex_unlock(&sensor_lock);
						return -1;
					}
					
					mutex_unlock(&sensor_lock);
					set_bit(CMC_BIT_ALS, &obj->enable);
				}
				else
				{
					mutex_lock(&sensor_lock);
					if(err = em3071_enable_als(obj->client, 0))
					{
					
						mutex_unlock(&sensor_lock);
						APS_ERR("disable als fail: %d\n", err); 
						return -1;
					}
					
					mutex_unlock(&sensor_lock);
					
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
				sensor_data = (hwm_sensor_data *)buff_out;
				
				mutex_lock(&sensor_lock);
				em3071_read_als(obj->client, &obj->als);
				mutex_unlock(&sensor_lock);
								
				sensor_data->values[0] = em3071_get_als_value(obj, obj->als);
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
static int em3071_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info) 
{    
	strcpy(info->type, EM3071_DEV_NAME);
	return 0;
}

/*----------------------------------------------------------------------------*/
static int em3071_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct em3071_priv *obj;
	struct hwmsen_object obj_ps, obj_als;
	int err = 0;

	if(!(obj = kzalloc(sizeof(*obj), GFP_KERNEL)))
	{
		err = -ENOMEM;
		goto exit;
	}
	memset(obj, 0, sizeof(*obj));
	em3071_obj = obj;

	APS_LOG("em3071_init_client() in!\n");
	obj->hw = get_cust_alsps_hw();
	em3071_get_addr(obj->hw, &obj->addr);

	/*for interrup work mode support --*/
	INIT_WORK(&obj->eint_work, em3071_eint_work);
	obj->client = client;
	i2c_set_clientdata(client, obj);	
	atomic_set(&obj->als_debounce, 300);
	atomic_set(&obj->als_deb_on, 0);
	atomic_set(&obj->als_deb_end, 0);
	atomic_set(&obj->ps_debounce, 300);
	atomic_set(&obj->ps_deb_on, 0);
	atomic_set(&obj->ps_deb_end, 0);
	atomic_set(&obj->ps_mask, 0);
	atomic_set(&obj->als_suspend, 0);
	atomic_set(&obj->als_cmd_val, 0xDF);
	atomic_set(&obj->ps_cmd_val,  0xC1);
	atomic_set(&obj->ps_thd_val,  obj->hw->ps_threshold);
	obj->enable = 0;
	obj->pending_intr = 0;
	obj->als_level_num = sizeof(obj->hw->als_level)/sizeof(obj->hw->als_level[0]);
	obj->als_value_num = sizeof(obj->hw->als_value)/sizeof(obj->hw->als_value[0]);  
	
	BUG_ON(sizeof(obj->als_level) != sizeof(obj->hw->als_level));
	memcpy(obj->als_level, obj->hw->als_level, sizeof(obj->als_level));
	BUG_ON(sizeof(obj->als_value) != sizeof(obj->hw->als_value));
	memcpy(obj->als_value, obj->hw->als_value, sizeof(obj->als_value));
	atomic_set(&obj->i2c_retry, 3);
	set_bit(CMC_BIT_ALS, &obj->enable);
	set_bit(CMC_BIT_PS, &obj->enable);

	
	em3071_i2c_client = client;

	//client->timing = 20;

	if(err = em3071_init_client(client))
	{
		goto exit_init_failed;
	}
	APS_LOG("em3071_init_client() OK!\n");

	if(err = misc_register(&em3071_device))
	{
		APS_ERR("em3071_device register failed\n");
		goto exit_misc_device_register_failed;
	}
	obj_ps.self = em3071_obj;
	/*for interrup work mode support */
	if(1 == obj->hw->polling_mode_ps)
	{
		obj_ps.polling = 1;
	}
	else
	{
		obj_ps.polling = 0;
	}

	obj_ps.sensor_operate = em3071_ps_operate;
	if(err = hwmsen_attach(ID_PROXIMITY, &obj_ps))
	{
		APS_ERR("attach fail = %d\n", err);
		goto exit_create_attr_failed;
	}
	
	obj_als.self = em3071_obj;
	obj_als.polling = 1;
	obj_als.sensor_operate = em3071_als_operate;
	if(err = hwmsen_attach(ID_LIGHT, &obj_als))
	{
		APS_ERR("attach fail = %d\n", err);
		goto exit_create_attr_failed;
	}


#if defined(CONFIG_HAS_EARLYSUSPEND)
	obj->early_drv.level    = EARLY_SUSPEND_LEVEL_DISABLE_FB - 1,
	obj->early_drv.suspend  = em3071_early_suspend,
	obj->early_drv.resume   = em3071_late_resume,    
	register_early_suspend(&obj->early_drv);
#endif

	APS_LOG("%s: OK\n", __func__);
	return 0;

	exit_create_attr_failed:
	misc_deregister(&em3071_device);
	exit_misc_device_register_failed:
	exit_init_failed:
	//i2c_detach_client(client);
	exit_kfree:
	kfree(obj);
	exit:
	em3071_i2c_client = NULL;           
//	MT6516_EINTIRQMask(CUST_EINT_ALS_NUM);  /*mask interrupt if fail*/
	APS_ERR("%s: err = %d\n", __func__, err);
	return err;
}
/*----------------------------------------------------------------------------*/
static int em3071_i2c_remove(struct i2c_client *client)
{
	int err;	
	if(err = misc_deregister(&em3071_device))
	{
		APS_ERR("misc_deregister fail: %d\n", err);    
	}
	
	em3071_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));

	return 0;
}
/*----------------------------------------------------------------------------*/
static int em3071_probe(struct platform_device *pdev) 
{
	struct alsps_hw *hw = get_cust_alsps_hw();

	em3071_power(hw, 1);    
//	em3071_force[0] = hw->i2c_num;
//	em3071_force[1] = hw->i2c_addr[0];
	APS_DBG("%s, I2C = %d, addr =0x%x\n",__func__, hw->i2c_num,hw->i2c_addr[0]);
	if(i2c_add_driver(&em3071_i2c_driver))
	{
		APS_ERR("add driver error\n");
		return -1;
	} 
	return 0;
}
/*----------------------------------------------------------------------------*/
static int em3071_remove(struct platform_device *pdev)
{
	struct alsps_hw *hw = get_cust_alsps_hw();
	APS_FUN();    
	em3071_power(hw, 0);    
	i2c_del_driver(&em3071_i2c_driver);
	return 0;
}
/*----------------------------------------------------------------------------*/
static struct platform_driver em3071_alsps_driver = {
	.probe      = em3071_probe,
	.remove     = em3071_remove,    
	.driver     = {
		.name  = "als_ps",
		.owner = THIS_MODULE,
	}
};
/*----------------------------------------------------------------------------*/
static int __init em3071_init(void)
{
	struct alsps_hw *hw = get_cust_alsps_hw();
	
	APS_LOG("%s: i2c_number=%d\n", __func__,hw->i2c_num); 
	i2c_em3071.addr = hw->i2c_addr[0] ;
	i2c_register_board_info(hw->i2c_num, &i2c_em3071, 1);
	
	if(platform_driver_register(&em3071_alsps_driver))
	{
		APS_ERR("failed to register driver");
		return -ENODEV;
	}
	return 0;
}
/*----------------------------------------------------------------------------*/
static void __exit em3071_exit(void)
{
	APS_FUN();
	platform_driver_unregister(&em3071_alsps_driver);  //删除设备
}
/*----------------------------------------------------------------------------*/
module_init(em3071_init);     //向linux系统记录设备初始化函数名称
module_exit(em3071_exit);   //向linux系统记录设备退出函数名称
/*----------------------------------------------------------------------------*/
MODULE_AUTHOR("Dexiang Liu");
MODULE_DESCRIPTION("em3071 driver");
MODULE_LICENSE("GPL");
