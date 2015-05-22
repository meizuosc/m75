/* drivers/hwmon/mt6516/amit/ISL29044.c - ISL29044 ALS/PS driver
 * 
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

#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include <asm/io.h>
#include <cust_eint.h>
#include <cust_alsps.h>
#include "ISL29044.h"
#include <linux/hwmsen_helper.h>

#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>


extern void mt_eint_mask(unsigned int eint_num);
extern void mt_eint_unmask(unsigned int eint_num);
extern void mt_eint_set_hw_debounce(unsigned int eint_num, unsigned int ms);
extern void mt_eint_set_polarity(unsigned int eint_num, unsigned int pol);
extern unsigned int mt_eint_set_sens(unsigned int eint_num, unsigned int sens);
extern void mt_eint_registration(unsigned int eint_num, unsigned int flow, void (EINT_FUNC_PTR)(void), unsigned int is_auto_umask);
extern void mt_eint_print_status(void);



#define POWER_NONE_MACRO MT65XX_POWER_NONE

static DEFINE_MUTEX(isl29044_i2c_mutex);
static DEFINE_MUTEX(isl29044_op_mutex);

#define USE_EARLY_SUSPEND

/*----------------------------------------------------------------------------*/

#define ISL29044_DEV_NAME     "ISL29044"
/*----------------------------------------------------------------------------*/
#define APS_TAG                  "[ALS/PS] "
#define APS_FUN(f)               printk(KERN_INFO APS_TAG"%s\n", __FUNCTION__);
#define APS_ERR(fmt, args...)    printk(KERN_ERR APS_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define APS_LOG(fmt, args...)    printk(KERN_INFO APS_TAG fmt, ##args)
#define APS_DBG(fmt, args...)    printk(KERN_INFO fmt, ##args)                        
/******************************************************************************
 * extern functions
*******************************************************************************/
/*----------------------------------------------------------------------------*/
static struct i2c_client *ISL29044_i2c_client = NULL;
/*----------------------------------------------------------------------------*/
static const struct i2c_device_id ISL29044_i2c_id[] = {{ISL29044_DEV_NAME,0},{}};
static struct i2c_board_info __initdata i2c_ISL29044={ I2C_BOARD_INFO("ISL29044", (ISL29044_I2C_SLAVE_ADDR>>1))};
/*the adapter id & i2c address will be available in customization*/
//static unsigned short ISL29044_force[] = {0x00, ISL29044_I2C_SLAVE_ADDR, I2C_CLIENT_END, I2C_CLIENT_END};
//static const unsigned short *const ISL29044_forces[] = { ISL29044_force, NULL };
//static struct i2c_client_address_data ISL29044_addr_data = { .forces = ISL29044_forces,};
/*----------------------------------------------------------------------------*/
static int ISL29044_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id); 
static int ISL29044_i2c_remove(struct i2c_client *client);
static int ISL29044_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);
/*----------------------------------------------------------------------------*/
static int ISL29044_i2c_suspend(struct i2c_client *client, pm_message_t msg);
static int ISL29044_i2c_resume(struct i2c_client *client);


static struct ISL29044_priv *g_ISL29044_ptr = NULL;

/*----------------------------------------------------------------------------*/
typedef enum {
    CMC_TRC_APS_DATA = 0x0002,
    CMC_TRC_EINT    = 0x0004,
    CMC_TRC_IOCTL   = 0x0008,
    CMC_TRC_I2C     = 0x0010,
    CMC_TRC_CVT_ALS = 0x0020,
    CMC_TRC_CVT_PS  = 0x0040,
    CMC_TRC_DEBUG   = 0x8000,
} CMC_TRC;
/*----------------------------------------------------------------------------*/
typedef enum {
    CMC_BIT_ALS    = 1,
    CMC_BIT_PS     = 2,
} CMC_BIT;
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
struct ISL29044_priv {
    struct alsps_hw  *hw;
    struct i2c_client *client;
    struct delayed_work  eint_work;
    
    /*misc*/
    atomic_t    trace;
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
    u8          ps;
    u8          _align;
    u16         als_level_num;
    u16         als_value_num;
    u32         als_level[C_CUST_ALS_LEVEL-1];
    u32         als_value[C_CUST_ALS_LEVEL];

    bool    als_enable;    /*record current als status*/
	unsigned int    als_widow_loss; 
	
    bool    ps_enable;     /*record current ps status*/
    atomic_t   ps_thd_val;     /*the cmd value can't be read, stored in ram*/
    ulong       enable;         /*record HAL enalbe status*/
    ulong       pending_intr;   /*pending interrupt*/
    //ulong        first_read;   // record first read ps and als
    /*early suspend*/
#if defined(USE_EARLY_SUSPEND)
    struct early_suspend    early_drv;
#endif     
};
/*----------------------------------------------------------------------------*/
static struct i2c_driver ISL29044_i2c_driver = {	
	.probe      = ISL29044_i2c_probe,
	.remove     = ISL29044_i2c_remove,
	.detect     = ISL29044_i2c_detect,
	.suspend    = ISL29044_i2c_suspend,
	.resume     = ISL29044_i2c_resume,
	.id_table   = ISL29044_i2c_id,
	//.address_data = &ISL29044_addr_data,
	.driver = {
		.name           = ISL29044_DEV_NAME,
	},
};

static struct ISL29044_priv *ISL29044_obj = NULL;
static struct platform_driver ISL29044_alsps_driver;

static int ISL29044_get_ps_value(struct ISL29044_priv *obj, u8 ps);
static int ISL29044_get_als_value(struct ISL29044_priv *obj, u16 als);

/*----------------------------------------------------------------------------*/
static int isl_i2c_read_block(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{
    u8 beg = addr;
	int err;
	struct i2c_msg msgs[2]={{0},{0}};
	
	mutex_lock(&isl29044_i2c_mutex);
	
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len =1;
	msgs[0].buf = &beg;

	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len =len;
	msgs[1].buf = data;
	
	if (!client)
	{
	    mutex_unlock(&isl29044_i2c_mutex);
		return -EINVAL;
	}
	else if (len > C_I2C_FIFO_SIZE) 
	{
		APS_ERR(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		mutex_unlock(&isl29044_i2c_mutex);
		return -EINVAL;
	}
	err = i2c_transfer(client->adapter, msgs, sizeof(msgs)/sizeof(msgs[0]));
	if (err != 2) 
	{
		APS_ERR("i2c_transfer error: (%d %p %d) %d\n",addr, data, len, err);
		err = -EIO;
	} 
	else 
	{
		err = 0;
	}
	mutex_unlock(&isl29044_i2c_mutex);
	return err;

}

static int  isl_i2c_write_block(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{   /*because address also occupies one byte, the maximum length for write is 7 bytes*/
    int err, idx, num;
    char buf[C_I2C_FIFO_SIZE];
    err =0;
	mutex_lock(&isl29044_i2c_mutex);
    if (!client)
    {
        mutex_unlock(&isl29044_i2c_mutex);
        return -EINVAL;
    }
    else if (len >= C_I2C_FIFO_SIZE) 
	{        
        APS_ERR(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		mutex_unlock(&isl29044_i2c_mutex);
        return -EINVAL;
    }    

    num = 0;
    buf[num++] = addr;
    for (idx = 0; idx < len; idx++)
    {
        buf[num++] = data[idx];
    }

    err = i2c_master_send(client, buf, num);
    if (err < 0)
	{
        APS_ERR("send command error!!\n");
		mutex_unlock(&isl29044_i2c_mutex);
        return -EFAULT;
    } 
	mutex_unlock(&isl29044_i2c_mutex);
    return err;
}
/*----------------------------------------------------------------------------*/


static void ISL29044_dumpReg(struct i2c_client *client)
{
  int i=0;
  u8 addr = 0x00;
  u8 regdata=0;
  for(i=0; i<15 ; i++)
  {
    //dump all
    isl_i2c_read_block(client, addr, &regdata, 0x1);
	APS_LOG("Reg addr=%x regdata=%x\n",addr,regdata);
	//snprintf(buf,1,"%c",regdata);
	addr++;
	if(addr > 0x0B)
		break;
  }
}

/*----------------------------------------------------------------------------*/
int ISL29044_get_timing(void)
{
return 200;
/*
	u32 base = I2C2_BASE; 
	return (__raw_readw(mt6516_I2C_HS) << 16) | (__raw_readw(mt6516_I2C_TIMING));
*/
}

int ISL29044_read_PS_data(struct i2c_client *client, u8 *data)
{
	struct ISL29044_priv *obj = i2c_get_clientdata(client);    
	int ret = 0;
	//u8 aps_data=0;
	//u8 addr = 0x00;

	//hwmsen_read_byte_sr(client,APS_BOTH_DATA,&aps_data);
	if(isl_i2c_read_block(client, REG_DATA_PROX, data, 0x1))
	{
		APS_ERR("reads aps data = %d\n", ret);
		return -EFAULT;
	}
	
	APS_LOG("data=%x \n",*data);
	if(atomic_read(&obj->trace) & CMC_TRC_APS_DATA)
	{
		APS_DBG("APS_PS:  0x%04X\n", (u32)(*data));
	}
	return 0;    
}

int ISL29044_read_ALS_data(struct i2c_client *client, u16 *data)
{
	struct ISL29044_priv *obj = i2c_get_clientdata(client);    
	u8 aps_data0=0, aps_data1=0;
	//u8 addr = 0x00;
	int ret = 0;

	//hwmsen_read_byte_sr(client,APS_BOTH_DATA,&aps_data);
	if(isl_i2c_read_block(client, REG_DATA_LSB_ALS, &aps_data0, 0x1))
	{
		APS_ERR("reads aps data = %d\n", ret);
		return -EFAULT;
	}
	if(isl_i2c_read_block(client, REG_DATA_MSB_ALS, &aps_data1, 0x1))
	{
		APS_ERR("reads aps data = %d\n", ret);
		return -EFAULT;
	}
	APS_LOG("aps_data0=%x,aps_data1=%x \n",aps_data0,aps_data1);
	*data = (aps_data0 | (aps_data1<<8));
	
	APS_LOG("aps_data=%x \n",*data);
	if(atomic_read(&obj->trace) & CMC_TRC_APS_DATA)
	{
		APS_DBG("APS_ALS:  0x%04X\n", (u32)(*data));
	}
	return 0;    
}
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/

int ISL29044_init_device(struct i2c_client *client)
{
	//struct ISL29044_priv *obj = i2c_get_clientdata(client);        
	APS_LOG("ISL29044_init_device.........\r\n");
	u8 buf =0;
	
	//refor to datasheet
	if(isl_i2c_write_block(client, REG_CMD_1, 0xC6, 0x1))//b 1011 0110  ps_enable, ps_peroid 100ms ,110mA, als enable ,2000lux, visable spectrum
	{
	  return -EFAULT;
	}
	if(isl_i2c_write_block(client, REG_CMD_2, 0x22, 0x1))//b 0010 0010  ps_persist 4 ,als_persist 4, int_or
	{
	  return -EFAULT;
	}
    if(isl_i2c_write_block(client, REG_INT_LOW_PROX, 0x20, 0x1))//ps int low threshold
    {
      return -EFAULT;
    }
    if(isl_i2c_write_block(client, REG_INT_HIGH_PROX, 0xE0, 0x1))//ps int high threshold
    {
      return -EFAULT;
    }
    if(isl_i2c_write_block(client, REG_INT_LOW_ALS, 0x00, 0x1)) //
    {
      return -EFAULT;
    }
    if(isl_i2c_write_block(client, REG_INT_LOW_HIGH_ALS, 0xF0, 0x1))
    {
      return -EFAULT;
    }
	//power up & enalbe both
	if(isl_i2c_write_block(client, REG_INT_HIGH_ALS, 0xFF, 0x1)) 
	{
	  return -EFAULT;
	}
	
	APS_LOG("ISL29044_init_device.........end \r\n");
	return 0;
}


/*----------------------------------------------------------------------------*/
static void ISL29044_power(struct alsps_hw *hw, unsigned int on) 
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
			if(!hwPowerOn(hw->power_id, hw->power_vol, "ISL29044")) 
			{
				APS_ERR("power on fails!!\n");
			}
		}
		else
		{
			if(!hwPowerDown(hw->power_id, "ISL29044")) 
			{
				APS_ERR("power off fail!!\n");   
			}
		}
	}
	power_on = on;
}
/*----------------------------------------------------------------------------*/
static int ISL29044_enable_als(struct i2c_client *client, bool enable)
{
    APS_LOG(" ISL29044_enable_als %d \n",enable); 
	struct ISL29044_priv *obj = i2c_get_clientdata(client);
	int err=0;
	int trc = atomic_read(&obj->trace);
	u8 regdata=0;
	if(enable == obj->als_enable)
	{
	   return 0;
	}

	if(isl_i2c_read_block(client, REG_CMD_1, &regdata, 0x1))
	{
		APS_ERR("read REG_CMD_1 register err!\n");
		return -1;
	}

	regdata &= 0b01111011; //first set bit7 for ps enable  , bit2 for als enable
	
	if(enable == TRUE)//enable als
	{
	     APS_LOG("first enable als!\n");
		 if(true == obj->ps_enable)
		 {
		   APS_LOG("ALS(1): enable both \n");
		   atomic_set(&obj->ps_deb_on, 1);
		   atomic_set(&obj->ps_deb_end, jiffies+atomic_read(&obj->ps_debounce)/(1000/HZ));
		   regdata |= 0b10000100; //enable both
		 }
		 if(false == obj->ps_enable)
		 {
		   APS_LOG("ALS(1): enable als only \n");
		   regdata |= 0b00000100; //only enable als
		 }
		 atomic_set(&obj->als_deb_on, 1);
		 atomic_set(&obj->als_deb_end, jiffies+atomic_read(&obj->als_debounce)/(1000/HZ));
		 set_bit(CMC_BIT_ALS,  &obj->pending_intr);
		 schedule_delayed_work(&obj->eint_work,230); //after enable the value is not accurate
		 APS_LOG("first enalbe als set pending interrupt %d\n",obj->pending_intr);
	}
	else
	{
		 regdata &= 0b11111011;//disable als only
	}
	

	if(isl_i2c_write_block(client,REG_CMD_1,regdata, 0x1))
	{
		APS_LOG("ISL29044_enable_als failed!\n");
		return -1;
	}
    obj->als_enable = enable;
	
#if 0
	if(isl_i2c_read_block(client, REG_CMD_1, &regdata,0x1))
	{
		APS_ERR("read REG_CMD_1 register err!\n");
		return -1;
	}
	//
	APS_LOG(" after ISL29044_enable_ps 00h=%x \n",regdata);
#endif

	if(trc & CMC_TRC_DEBUG)
	{
		APS_LOG("enable als (%d)\n", enable);
	}

	return err;
}
/*----------------------------------------------------------------------------*/
static int ISL29044_enable_ps(struct i2c_client *client, bool enable)
{
    APS_LOG(" ISL29044_enable_ps %d\n",enable); 
	struct ISL29044_priv *obj = i2c_get_clientdata(client);
	int err=0;
	int trc = atomic_read(&obj->trace);
	u8 regdata=0;
	if(enable == obj->ps_enable)
	{
	   return 0;
	}
	

	if(isl_i2c_read_block(client, REG_CMD_1, &regdata, 0x1))
	{
		APS_ERR("read REG_CMD_1 register err!\n");
		return -1;
	}
	regdata &= 0b01111011;

	APS_LOG(" ISL29044_enable_ps regdata=%x \n",regdata); 

	if(enable == TRUE)//enable ps
	{
	     APS_LOG("first enable ps!\n");
		 if(true == obj->als_enable)
		 {
		   regdata |= 0b10000100; //enable both
		   atomic_set(&obj->als_deb_on, 1);
		   atomic_set(&obj->als_deb_end, jiffies+atomic_read(&obj->als_debounce)/(1000/HZ));
		   APS_LOG("PS(1): enable ps both !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
		 }
		 if(false == obj->als_enable)
		 {
		   regdata |= 0b10000000; //only enable ps
		   APS_LOG("PS(1): enable ps only !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
		 }
		 atomic_set(&obj->ps_deb_on, 1);
		 atomic_set(&obj->ps_deb_end, jiffies+atomic_read(&obj->ps_debounce)/(1000/HZ));
		 set_bit(CMC_BIT_PS,  &obj->pending_intr);
		 schedule_delayed_work(&obj->eint_work,120);
		 APS_LOG("first enalbe ps set pending interrupt %d\n",obj->pending_intr);
	}
	else//disable ps
	{
		regdata &= 0b01111111;//disable Ps only
	}
	

	if(isl_i2c_write_block(client,REG_CMD_1,regdata, 0x1))
	{
		APS_LOG("ISL29044_enable_als failed!\n");
		return -1;
	}
	obj->ps_enable = enable;

#if 0
	if(isl_i2c_read_block(client, REG_CMD_1, &regdata,0x1))
	{
		APS_ERR("read REG_CMD_1 register err!\n");
		return -1;
	}
	//
	APS_LOG(" after ISL29044_enable_ps 00h=%x \n",regdata);
#endif
	
	if(trc & CMC_TRC_DEBUG)
	{
		APS_LOG("enable ps (%d)\n", enable);
	}

	return err;
}
/*----------------------------------------------------------------------------*/


static int ISL29044_check_intr(struct i2c_client *client) 
{
	struct ISL29044_priv *obj = i2c_get_clientdata(client);
	int err;
	u8 data=0;

	err = isl_i2c_read_block(client, REG_CMD_2, &data, 0x1);
	APS_LOG("INT flage: = %x  \n", data);

	if(err)
	{
		APS_ERR("WARNING: read int status: %d\n", err);
		return 0;
	}
    
	if(data & 0x08)//als
	{
		set_bit(CMC_BIT_ALS, &obj->pending_intr);
	}
	else
	{
	   clear_bit(CMC_BIT_ALS, &obj->pending_intr);
	}
	set_bit(CMC_BIT_PS,  &obj->pending_intr);
	
	if(atomic_read(&obj->trace) & CMC_TRC_DEBUG)
	{
		APS_LOG("check intr: 0x%08X\n", obj->pending_intr);
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
void ISL29044_eint_func(void)
{
	struct ISL29044_priv *obj = g_ISL29044_ptr;
	APS_LOG("fwq interrupt fuc\n");
	if(!obj)
	{
		return;
	}
	
	schedule_delayed_work(&obj->eint_work,0);
	if(atomic_read(&obj->trace) & CMC_TRC_EINT)
	{
		APS_LOG("eint: als/ps intrs\n");
	}
}
/*----------------------------------------------------------------------------*/
int Ps_status =0;
static void ISL29044_eint_work(struct work_struct *work)
{
	struct ISL29044_priv *obj = (struct ISL29044_priv *)container_of(work, struct ISL29044_priv, eint_work);
	int err;
	hwm_sensor_data sensor_data;
	
	memset(&sensor_data, 0, sizeof(sensor_data));

	APS_LOG("ISL29044_eint_work\n");

	if(0 == atomic_read(&obj->ps_deb_on)) // first enable do not check interrupt
	{
	   err = ISL29044_check_intr(obj->client);
	}
	
	if(err)
	{
		APS_ERR("check intrs: %d\n", err);
	}

    APS_LOG("ISL29044_eint_work &obj->pending_intr =%d\n",obj->pending_intr);
	
	if((1<<CMC_BIT_ALS) & obj->pending_intr &(0 == obj->hw->polling_mode_als))
	{
	  //get raw data
	  APS_LOG("fwq als INT\n");
	  if(err = ISL29044_read_ALS_data(obj->client, &obj->als))
	  {
		 APS_ERR("ISL29044 read als data: %d\n", err);;
	  }
	  //map and store data to hwm_sensor_data
	  while(-1 == ISL29044_get_als_value(obj, obj->als))
	  {
		 ISL29044_read_ALS_data(obj->client, &obj->als);
		 msleep(50);
	  }
 	  sensor_data.values[0] = ISL29044_get_als_value(obj, obj->als);
	  sensor_data.value_divide = 1;
	  sensor_data.status = SENSOR_STATUS_ACCURACY_MEDIUM;
	  //let up layer to know
	  if(err = hwmsen_get_interrupt_data(ID_LIGHT, &sensor_data))
	  {
		APS_ERR("call hwmsen_get_interrupt_data fail = %d\n", err);
	  }
	  
	}
	if((1<<CMC_BIT_PS) &  obj->pending_intr)
	{
	  //get raw data
	  APS_LOG("fwq ps INT\n");
	  if(err = ISL29044_read_PS_data(obj->client, &obj->ps))
	  {
		 APS_ERR("ISL29044 read ps data: %d\n", err);;
	  }
	  //map and store data to hwm_sensor_data
	  while(-1 == ISL29044_get_ps_value(obj, obj->ps))
	  {
		 ISL29044_read_PS_data(obj->client, &obj->ps);
		 msleep(50);
		 APS_LOG("ISL29044 read ps data delay\n");;
	  }
	  sensor_data.values[0] = ISL29044_get_ps_value(obj, obj->ps);
	  sensor_data.value_divide = 1;
	  sensor_data.status = SENSOR_STATUS_ACCURACY_MEDIUM;
	  //let up layer to know
	  APS_LOG("ISL29044 read ps data  = %d  Ps_status =%d \n",sensor_data.values[0],Ps_status);


/*  Ps near: interrupt pin is low,  ps far away, pin is high
       ----        -------------
            |       |
            |       |
            |       |
             -----
*/
	  //interrupt pin initial state is low edge trigger
	  if(Ps_status != sensor_data.values[0])
	  {
	  	  if(sensor_data.values[0] == 0)  //now is close, next time should be high edge trigger
	  	  	mt_eint_set_polarity(CUST_EINT_ALS_NUM, 1);
		  else     //now is close, next time should be low  edge trigger
		  	mt_eint_set_polarity(CUST_EINT_ALS_NUM, 0);
	  }

	  
	  Ps_status = sensor_data.values[0];   //record current status
	  
	  if(err = hwmsen_get_interrupt_data(ID_PROXIMITY, &sensor_data))
	  {
		APS_ERR("call hwmsen_get_interrupt_data fail = %d\n", err);
	  }
	}
	
	mt_eint_unmask(CUST_EINT_ALS_NUM);  
}

/*----------------------------------------------------------------------------*/
int ISL29044_setup_eint(struct i2c_client *client)
{
	struct ISL29044_priv *obj = i2c_get_clientdata(client);        

	g_ISL29044_ptr = obj;
	/*configure to GPIO function, external interrupt*/

	
	mt_set_gpio_mode(GPIO_ALS_EINT_PIN, GPIO_ALS_EINT_PIN_M_EINT);
	mt_set_gpio_dir(GPIO_ALS_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_ALS_EINT_PIN, TRUE);
	mt_set_gpio_pull_select(GPIO_ALS_EINT_PIN, GPIO_PULL_UP);

	mt_eint_set_hw_debounce(CUST_EINT_ALS_NUM, CUST_EINT_ALS_DEBOUNCE_CN);
	mt_eint_registration(CUST_EINT_ALS_NUM, CUST_EINT_ALS_TYPE, ISL29044_eint_func, 0);

	mt_eint_unmask(CUST_EINT_ALS_NUM);
	
    return 0;
}

/*----------------------------------------------------------------------------*/
static int ISL29044_init_client(struct i2c_client *client)
{
	struct ISL29044_priv *obj = i2c_get_clientdata(client);
	int err=0;
	APS_LOG("ISL29044_init_client.........\r\n");

	if((err = ISL29044_setup_eint(client)))
	{
		APS_ERR("setup eint: %d\n", err);
		return err;
	}
	
	
	if((err = ISL29044_init_device(client)))
	{
		APS_ERR("init dev: %d\n", err);
		return err;
	}

	
	return err;
}
/******************************************************************************
 * Sysfs attributes
*******************************************************************************/
static ssize_t ISL29044_show_config(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	
	if(!ISL29044_obj)
	{
		APS_ERR("ISL29044_obj is null!!\n");
		return 0;
	}
	
	res = snprintf(buf, PAGE_SIZE, "(%d %d %d %d %d)\n", 
		atomic_read(&ISL29044_obj->i2c_retry), atomic_read(&ISL29044_obj->als_debounce), 
		atomic_read(&ISL29044_obj->ps_mask), ISL29044_obj->ps_thd_val, atomic_read(&ISL29044_obj->ps_debounce));     
	return res;    
}
/*----------------------------------------------------------------------------*/
static ssize_t ISL29044_store_config(struct device_driver *ddri, char *buf, size_t count)
{
	int retry, als_deb, ps_deb, mask, thres;
	if(!ISL29044_obj)
	{
		APS_ERR("ISL29044_obj is null!!\n");
		return 0;
	}
	
	if(5 == sscanf(buf, "%d %d %d %d %d", &retry, &als_deb, &mask, &thres, &ps_deb))
	{ 
		atomic_set(&ISL29044_obj->i2c_retry, retry);
		atomic_set(&ISL29044_obj->als_debounce, als_deb);
		atomic_set(&ISL29044_obj->ps_mask, mask);
		atomic_set(&ISL29044_obj->ps_thd_val, thres);        
		atomic_set(&ISL29044_obj->ps_debounce, ps_deb);
	}
	else
	{
		APS_ERR("invalid content: '%s', length = %d\n", buf, count);
	}
	return count;    
}
/*----------------------------------------------------------------------------*/
static ssize_t ISL29044_show_trace(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	if(!ISL29044_obj)
	{
		APS_ERR("ISL29044_obj is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&ISL29044_obj->trace));     
	return res;    
}
/*----------------------------------------------------------------------------*/
static ssize_t ISL29044_store_trace(struct device_driver *ddri, char *buf, size_t count)
{
    int trace;
    if(!ISL29044_obj)
	{
		APS_ERR("ISL29044_obj is null!!\n");
		return 0;
	}
	
	if(1 == sscanf(buf, "0x%x", &trace))
	{
		atomic_set(&ISL29044_obj->trace, trace);
	}
	else 
	{
		APS_ERR("invalid content: '%s', length = %d\n", buf, count);
	}
	return count;    
}
/*----------------------------------------------------------------------------*/
static ssize_t ISL29044_show_als(struct device_driver *ddri, char *buf)
{
	int res;
	u8 dat = 0;
	
	if(!ISL29044_obj)
	{
		APS_ERR("ISL29044_obj is null!!\n");
		return 0;
	}
	if(res = ISL29044_read_ALS_data(ISL29044_obj->client, &ISL29044_obj->als))
	{
		return snprintf(buf, PAGE_SIZE, "ERROR: %d\n", res);
	}
	else
	{   dat = ISL29044_obj->als & 0x3f;
		return snprintf(buf, PAGE_SIZE, "0x%04X\n", dat);     
	}
}
/*----------------------------------------------------------------------------*/
static ssize_t ISL29044_show_ps(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	u8 dat=0;
	if(!ISL29044_obj)
	{
		APS_ERR("ISL29044_obj is null!!\n");
		return 0;
	}
	
	if(res = ISL29044_read_PS_data(ISL29044_obj->client, &ISL29044_obj->ps))
	{
		return snprintf(buf, PAGE_SIZE, "ERROR: %d\n", res);
	}
	else
	{
	    dat = ISL29044_obj->ps & 0x80;
		return snprintf(buf, PAGE_SIZE, "0x%04X\n", dat);     
	}
}
/*----------------------------------------------------------------------------*/
static ssize_t ISL29044_show_reg(struct device_driver *ddri, char *buf)
{
	if(!ISL29044_obj)
	{
		APS_ERR("ISL29044_obj is null!!\n");
		return 0;
	}
	
	/*read*/
	ISL29044_dumpReg(ISL29044_obj->client);
	
	return 0;
}

/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static ssize_t ISL29044_show_status(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	
	if(!ISL29044_obj)
	{
		APS_ERR("ISL29044_obj is null!!\n");
		return 0;
	}
	
	if(ISL29044_obj->hw)
	{
	
		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: %d, (%d %d)\n", 
			ISL29044_obj->hw->i2c_num, ISL29044_obj->hw->power_id, ISL29044_obj->hw->power_vol);
		
	}
	else
	{
		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: NULL\n");
	}


	len += snprintf(buf+len, PAGE_SIZE-len, "MISC: %d %d\n", atomic_read(&ISL29044_obj->als_suspend), atomic_read(&ISL29044_obj->ps_suspend));

	return len;
}
/*----------------------------------------------------------------------------*/
static ssize_t ISL29044_show_i2c(struct device_driver *ddri, char *buf)
{
/*	ssize_t len = 0;
	u32 base = I2C2_BASE;

  

	return len;*/
	return 0;
}
/*----------------------------------------------------------------------------*/
static ssize_t ISL29044_store_i2c(struct device_driver *ddri, char *buf, size_t count)
{
/*	int sample_div, step_div;
	unsigned long tmp;
	u32 base = I2C2_BASE;    

	if(!ISL29044_obj)
	{
		APS_ERR("ISL29044_obj is null!!\n");
		return 0;
	}
	else if(2 != sscanf(buf, "%d %d", &sample_div, &step_div))
	{
		APS_ERR("invalid format: '%s'\n", buf);
		return 0;
	}
	tmp  = __raw_readw(mt6516_I2C_TIMING) & ~((0x7 << 8) | (0x1f << 0));
	tmp  = (sample_div & 0x7) << 8 | (step_div & 0x1f) << 0 | tmp;
	__raw_writew(tmp, mt6516_I2C_TIMING);        

	return count;
	*/
	return 0;
}
/*----------------------------------------------------------------------------*/
#define IS_SPACE(CH) (((CH) == ' ') || ((CH) == '\n'))
/*----------------------------------------------------------------------------*/
static int read_int_from_buf(struct ISL29044_priv *obj, const char* buf, size_t count,
                             u32 data[], int len)
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
static ssize_t ISL29044_show_alslv(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	int idx;
	if(!ISL29044_obj)
	{
		APS_ERR("ISL29044_obj is null!!\n");
		return 0;
	}
	
	for(idx = 0; idx < ISL29044_obj->als_level_num; idx++)
	{
		len += snprintf(buf+len, PAGE_SIZE-len, "%d ", ISL29044_obj->hw->als_level[idx]);
	}
	len += snprintf(buf+len, PAGE_SIZE-len, "\n");
	return len;    
}
/*----------------------------------------------------------------------------*/
static ssize_t ISL29044_store_alslv(struct device_driver *ddri, char *buf, size_t count)
{
	struct ISL29044_priv *obj;
	if(!ISL29044_obj)
	{
		APS_ERR("ISL29044_obj is null!!\n");
		return 0;
	}
	else if(!strcmp(buf, "def"))
	{
		memcpy(ISL29044_obj->als_level, ISL29044_obj->hw->als_level, sizeof(ISL29044_obj->als_level));
	}
	else if(ISL29044_obj->als_level_num != read_int_from_buf(ISL29044_obj, buf, count, 
			ISL29044_obj->hw->als_level, ISL29044_obj->als_level_num))
	{
		APS_ERR("invalid format: '%s'\n", buf);
	}    
	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t ISL29044_show_alsval(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	int idx;
	if(!ISL29044_obj)
	{
		APS_ERR("ISL29044_obj is null!!\n");
		return 0;
	}
	
	for(idx = 0; idx < ISL29044_obj->als_value_num; idx++)
	{
		len += snprintf(buf+len, PAGE_SIZE-len, "%d ", ISL29044_obj->hw->als_value[idx]);
	}
	len += snprintf(buf+len, PAGE_SIZE-len, "\n");
	return len;    
}
/*----------------------------------------------------------------------------*/
static ssize_t ISL29044_store_alsval(struct device_driver *ddri, char *buf, size_t count)
{
	if(!ISL29044_obj)
	{
		APS_ERR("ISL29044_obj is null!!\n");
		return 0;
	}
	else if(!strcmp(buf, "def"))
	{
		memcpy(ISL29044_obj->als_value, ISL29044_obj->hw->als_value, sizeof(ISL29044_obj->als_value));
	}
	else if(ISL29044_obj->als_value_num != read_int_from_buf(ISL29044_obj, buf, count, 
			ISL29044_obj->hw->als_value, ISL29044_obj->als_value_num))
	{
		APS_ERR("invalid format: '%s'\n", buf);
	}    
	return count;
}

/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(als,     S_IWUSR | S_IRUGO, ISL29044_show_als,   NULL);
static DRIVER_ATTR(ps,      S_IWUSR | S_IRUGO, ISL29044_show_ps,    NULL);
static DRIVER_ATTR(config,  S_IWUSR | S_IRUGO, ISL29044_show_config,ISL29044_store_config);
static DRIVER_ATTR(alslv,   S_IWUSR | S_IRUGO, ISL29044_show_alslv, ISL29044_store_alslv);
static DRIVER_ATTR(alsval,  S_IWUSR | S_IRUGO, ISL29044_show_alsval,ISL29044_store_alsval);
static DRIVER_ATTR(trace,   S_IWUSR | S_IRUGO, ISL29044_show_trace, ISL29044_store_trace);
static DRIVER_ATTR(status,  S_IWUSR | S_IRUGO, ISL29044_show_status,  NULL);
static DRIVER_ATTR(reg,     S_IWUSR | S_IRUGO, ISL29044_show_reg,   NULL);
static DRIVER_ATTR(i2c,     S_IWUSR | S_IRUGO, ISL29044_show_i2c,   ISL29044_store_i2c);
/*----------------------------------------------------------------------------*/
static struct driver_attribute *ISL29044_attr_list[] = {
    &driver_attr_als,
    &driver_attr_ps,    
    &driver_attr_trace,        /*trace log*/
    &driver_attr_config,
    &driver_attr_alslv,
    &driver_attr_alsval,
    &driver_attr_status,
    &driver_attr_i2c,
    &driver_attr_reg,
};
/*----------------------------------------------------------------------------*/
static int ISL29044_create_attr(struct driver_attribute *driver) 
{
	int idx, err = 0;
	int num = (int)(sizeof(ISL29044_attr_list)/sizeof(ISL29044_attr_list[0]));

	if (driver == NULL)
	{
		return -EINVAL;
	}

	for(idx = 0; idx < num; idx++)
	{
		if(err = driver_create_file(driver, ISL29044_attr_list[idx]))
		{            
			APS_ERR("driver_create_file (%s) = %d\n", ISL29044_attr_list[idx]->attr.name, err);
			break;
		}
	}    
	return err;
}
/*----------------------------------------------------------------------------*/
	static int ISL29044_delete_attr(struct device_driver *driver)
	{
	int idx ,err = 0;
	int num = (int)(sizeof(ISL29044_attr_list)/sizeof(ISL29044_attr_list[0]));

	if (!driver)
	return -EINVAL;

	for (idx = 0; idx < num; idx++) 
	{
		driver_remove_file(driver, ISL29044_attr_list[idx]);
	}
	
	return err;
}
/****************************************************************************** 
 * Function Configuration
******************************************************************************/
static int ISL29044_get_als_value(struct ISL29044_priv *obj, u16 als)
{
	int idx;
	int invalid = 0;
	//APS_LOG("als =%x \n",als);

	for(idx = 0; idx < obj->als_level_num; idx++)
	{
		if(als < obj->hw->als_level[idx])
		{
			//APS_LOG("idx =%d, level =%d\n",idx,obj->hw->als_level[idx] );
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
			//clear_bit(CMC_BIT_ALS, &obj->first_read);
		}
		
		if(1 == atomic_read(&obj->als_deb_on))
		{
			invalid = 1;
		}
	}

	if(!invalid)
	{
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
/*----------------------------------------------------------------------------*/

static int ISL29044_get_ps_value(struct ISL29044_priv *obj, u8 ps)
{
  
    int val= -1;
	int invalid = 0;

	if(ps > atomic_read(&obj->ps_thd_val))
	{
		val = 0;  /*close*/
	}
	else
	{
		val = 1;  /*far away*/
	}
	
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
			//clear_bit(CMC_BIT_PS, &obj->first_read);
		}
		
		if (1 == atomic_read(&obj->ps_deb_on))
		{
			invalid = 1;
		}
	}

	if(!invalid)
	{
		if(unlikely(atomic_read(&obj->trace) & CMC_TRC_CVT_PS))
		{
		   APS_DBG("PS:  %05d => %05d\n", ps, val);
		}
		return val;
		
	}	
	else
	{
		if(unlikely(atomic_read(&obj->trace) & CMC_TRC_CVT_PS))
		{
			APS_DBG("PS:  %05d => %05d (-1)\n", ps, val);    
		}
		return -1;
	}	
	
}

/****************************************************************************** 
 * Function Configuration
******************************************************************************/
static int ISL29044_open(struct inode *inode, struct file *file)
{
	file->private_data = ISL29044_i2c_client;

	if (!file->private_data)
	{
		APS_ERR("null pointer!!\n");
		return -EINVAL;
	}
	
	return nonseekable_open(inode, file);
}
/*----------------------------------------------------------------------------*/
static int ISL29044_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}
/*----------------------------------------------------------------------------*/
static long ISL29044_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct i2c_client *client = (struct i2c_client*)file->private_data;
	struct ISL29044_priv *obj = i2c_get_clientdata(client);  
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
				if(err = ISL29044_enable_ps(obj->client, true))
				{
					APS_ERR("enable ps fail: %d\n", err); 
					goto err_out;
				}
				
				set_bit(CMC_BIT_PS, &obj->enable);
			}
			else
			{
				if(err = ISL29044_enable_ps(obj->client, false))
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
			
			if(err = ISL29044_read_PS_data(obj->client, &obj->ps))
			{
				goto err_out;
			}
			dat = ISL29044_get_ps_value(obj, obj->ps);
			if(copy_to_user(ptr, &dat, sizeof(dat)))
			{
				err = -EFAULT;
				goto err_out;
			}  
			break;

		case ALSPS_GET_PS_RAW_DATA:    
			
			if(err = ISL29044_read_PS_data(obj->client, &obj->ps))
			{
				goto err_out;
			}
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
				if(err = ISL29044_enable_als(obj->client, true))
				{
					APS_ERR("enable als fail: %d\n", err); 
					goto err_out;
				}
				set_bit(CMC_BIT_ALS, &obj->enable);
			}
			else
			{
				if(err = ISL29044_enable_als(obj->client, false))
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
			
			if(err = ISL29044_read_ALS_data(obj->client, &obj->als))
			{
				goto err_out;
			}

			dat = ISL29044_get_als_value(obj, obj->als);
			if(copy_to_user(ptr, &dat, sizeof(dat)))
			{
				err = -EFAULT;
				goto err_out;
			}              
			break;

		case ALSPS_GET_ALS_RAW_DATA:    
			
			if(err = ISL29044_read_ALS_data(obj->client, &obj->als))
			{
				goto err_out;
			}
			dat = obj->als ;

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
static struct file_operations ISL29044_fops = {
	.open = ISL29044_open,
	.release = ISL29044_release,
	.unlocked_ioctl = ISL29044_unlocked_ioctl,
};
/*----------------------------------------------------------------------------*/
static struct miscdevice ISL29044_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "als_ps",
	.fops = &ISL29044_fops,
};
/*----------------------------------------------------------------------------*/
static int ISL29044_i2c_suspend(struct i2c_client *client, pm_message_t msg) 
{
	struct ISL29044_priv *obj = i2c_get_clientdata(client);    
	int err;
	APS_FUN();    
/*	if(msg.event == PM_EVENT_SUSPEND)
	{   
		if(!obj)
		{
			APS_ERR("null pointer!!\n");
			return -EINVAL;
		}
		
		atomic_set(&obj->als_suspend, 1);
		if(err = ISL29044_enable_als(client, false))
		{
			APS_ERR("disable als: %d\n", err);
			return err;
		}

		atomic_set(&obj->ps_suspend, 1);
		if(err = ISL29044_enable_ps(client, false))
		{
			APS_ERR("disable ps:  %d\n", err);
			return err;
		}
		
		ISL29044_power(obj->hw, 0);
	}*/
	return 0;
}
/*----------------------------------------------------------------------------*/
static int ISL29044_i2c_resume(struct i2c_client *client)
{
	struct ISL29044_priv *obj = i2c_get_clientdata(client);        
	int err;
	APS_FUN();
/*
	if(!obj)
	{
		APS_ERR("null pointer!!\n");
		return -EINVAL;
	}

	ISL29044_power(obj->hw, 1);
	if(err = ISL29044_init_client(client))
	{
		APS_ERR("initialize client fail!!\n");
		return err;        
	}
	atomic_set(&obj->als_suspend, 0);
	if(test_bit(CMC_BIT_ALS, &obj->enable))
	{
		if(err = ISL29044_enable_als(client, true))
		{
			APS_ERR("enable als fail: %d\n", err);        
		}
	}
	atomic_set(&obj->ps_suspend, 0);
	if(test_bit(CMC_BIT_PS,  &obj->enable))
	{
		if(err = ISL29044_enable_ps(client, true))
		{
			APS_ERR("enable ps fail: %d\n", err);                
		}
	}
*/
	return 0;
}
/*----------------------------------------------------------------------------*/
static void ISL29044_early_suspend(struct early_suspend *h) 
{   /*early_suspend is only applied for ALS*/
	struct ISL29044_priv *obj = container_of(h, struct ISL29044_priv, early_drv);   
	int err;
	APS_FUN();    

	if(!obj)
	{
		APS_ERR("null pointer!!\n");
		return;
	}
	
	atomic_set(&obj->als_suspend, 1);    
	if(err = ISL29044_enable_als(obj->client, false))
	{
		APS_ERR("disable als fail: %d\n", err); 
	}
}
/*----------------------------------------------------------------------------*/
static void ISL29044_late_resume(struct early_suspend *h)
{   /*early_suspend is only applied for ALS*/
	struct ISL29044_priv *obj = container_of(h, struct ISL29044_priv, early_drv);         
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
		if(err = ISL29044_enable_als(obj->client, true))
		{
			APS_ERR("enable als fail: %d\n", err);        

		}
	}
}

int ISL29044_ps_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value;
	hwm_sensor_data* sensor_data;
	struct ISL29044_priv *obj = (struct ISL29044_priv *)self;
	
	APS_FUN(f);
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
					if(err = ISL29044_enable_ps(obj->client, true))
					{
						APS_ERR("enable ps fail: %d\n", err); 
						return -1;
					}
					set_bit(CMC_BIT_PS, &obj->enable);
				}
				else
				{
					if(err = ISL29044_enable_ps(obj->client, false))
					{
						APS_ERR("disable ps fail: %d\n", err); 
						return -1;
					}
					clear_bit(CMC_BIT_PS, &obj->enable);
				}
			}
			break;

		case SENSOR_GET_DATA:
			//APS_LOG("fwq get ps data !!!!!!\n");
			if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
			{
				APS_ERR("get sensor data parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				sensor_data = (hwm_sensor_data *)buff_out;				
				
				if(err = ISL29044_read_PS_data(obj->client, &obj->ps))
				{
					err = -1;;
				}
				else
				{
				    while(-1 == ISL29044_get_ps_value(obj, obj->ps))
				    {
				      ISL29044_read_PS_data(obj->client, &obj->ps);
				      msleep(50);
				    }
				   
					sensor_data->values[0] = ISL29044_get_ps_value(obj, obj->ps);
					sensor_data->value_divide = 1;
					sensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
					//APS_LOG("fwq get ps data =%d\n",sensor_data->values[0]);
				    
					
				}				
			}
			break;
		default:
			APS_ERR("proxmy sensor operate function no this parameter %d!\n", command);
			err = -1;
			break;
	}
	
	return err;
}

int ISL29044_als_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value;
	hwm_sensor_data* sensor_data;
	struct ISL29044_priv *obj = (struct ISL29044_priv *)self;
	
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
					if(err = ISL29044_enable_als(obj->client, true))
					{
						APS_ERR("enable als fail: %d\n", err); 
						return -1;
					}
					set_bit(CMC_BIT_ALS, &obj->enable);
				}
				else
				{
					if(err = ISL29044_enable_als(obj->client, false))
					{
						APS_ERR("disable als fail: %d\n", err); 
						return -1;
					}
					clear_bit(CMC_BIT_ALS, &obj->enable);
				}
				
			}
			break;

		case SENSOR_GET_DATA:
			//APS_LOG("fwq get als data !!!!!!\n");
			if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
			{
				APS_ERR("get sensor data parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				sensor_data = (hwm_sensor_data *)buff_out;
								
				if(err = ISL29044_read_ALS_data(obj->client, &obj->als))
				{
					err = -1;;
				}
				else
				{
				    while(-1 == ISL29044_get_als_value(obj, obj->als))
				    {
				      ISL29044_read_ALS_data(obj->client, &obj->als);
				      msleep(50);
				    }
					sensor_data->values[0] = ISL29044_get_als_value(obj, obj->als);
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


/*----------------------------------------------------------------------------*/
static int ISL29044_i2c_detect(struct i2c_client *client, struct i2c_board_info *info) 
{    
	strcpy(info->type, ISL29044_DEV_NAME);
	return 0;
}

/*----------------------------------------------------------------------------*/
static int ISL29044_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    APS_FUN();
	struct ISL29044_priv *obj;
	struct hwmsen_object obj_ps, obj_als;
	int err = 0;

	if(!(obj = kzalloc(sizeof(*obj), GFP_KERNEL)))
	{
		err = -ENOMEM;
		goto exit;
	}
	memset(obj, 0, sizeof(*obj));
	ISL29044_obj = obj;

	obj->hw = get_cust_alsps_hw();


	INIT_DELAYED_WORK(&obj->eint_work, ISL29044_eint_work);
	obj->client = client;
	i2c_set_clientdata(client, obj);	
	atomic_set(&obj->als_debounce, 1000);
	atomic_set(&obj->als_deb_on, 0);
	atomic_set(&obj->als_deb_end, 0);
	atomic_set(&obj->ps_debounce, 1000);
	atomic_set(&obj->ps_deb_on, 0);
	atomic_set(&obj->ps_deb_end, 0);
	atomic_set(&obj->ps_mask, 0);
	atomic_set(&obj->trace, 0x00);
	atomic_set(&obj->als_suspend, 0);

	obj->ps_enable = 0;
	obj->als_enable = 0;
	obj->enable = 0;
	obj->pending_intr = 0;
	obj->als_level_num = sizeof(obj->hw->als_level)/sizeof(obj->hw->als_level[0]);
	obj->als_value_num = sizeof(obj->hw->als_value)/sizeof(obj->hw->als_value[0]);   
	BUG_ON(sizeof(obj->als_level) != sizeof(obj->hw->als_level));
	memcpy(obj->als_level, obj->hw->als_level, sizeof(obj->als_level));
	BUG_ON(sizeof(obj->als_value) != sizeof(obj->hw->als_value));
	memcpy(obj->als_value, obj->hw->als_value, sizeof(obj->als_value));
	atomic_set(&obj->i2c_retry, 3);
    //pre set ps threshold
	atomic_set(&obj->ps_thd_val , obj->hw->ps_threshold);
	//pre set window loss
    obj->als_widow_loss = obj->hw->als_window_loss;
	
	ISL29044_i2c_client = client;

	
	if(err = ISL29044_init_client(client))
	{
		goto exit_init_failed;
	}
	
	if(err = misc_register(&ISL29044_device))
	{
		APS_ERR("ISL29044_device register failed\n");
		goto exit_misc_device_register_failed;
	}

	if(err = ISL29044_create_attr(&ISL29044_alsps_driver.driver))
	{
		APS_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}
	obj_ps.self = ISL29044_obj;
	if(1 == obj->hw->polling_mode_ps)
	{
	  obj_ps.polling = 1;
	}
	else
	{
	  obj_ps.polling = 0;//interrupt mode
	}
	obj_ps.sensor_operate = ISL29044_ps_operate;
	if(err = hwmsen_attach(ID_PROXIMITY, &obj_ps))
	{
		APS_ERR("attach fail = %d\n", err);
		goto exit_create_attr_failed;
	}
	
	obj_als.self = ISL29044_obj;
	if(1 == obj->hw->polling_mode_als)
	{
	  obj_als.polling = 1;
	  APS_LOG("polling mode\n");
	}
	else
	{
	  obj_als.polling = 0;//interrupt mode
	  APS_LOG("interrupt mode\n");
	}
	obj_als.sensor_operate = ISL29044_als_operate;
	if(err = hwmsen_attach(ID_LIGHT, &obj_als))
	{
		APS_ERR("attach fail = %d\n", err);
		goto exit_create_attr_failed;
	}


#if defined(USE_EARLY_SUSPEND)
	obj->early_drv.level    = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 2,
	obj->early_drv.suspend  = ISL29044_early_suspend,
	obj->early_drv.resume   = ISL29044_late_resume,    
	register_early_suspend(&obj->early_drv);
#endif

	APS_LOG("%s: OK\n", __func__);
	return 0;

	exit_create_attr_failed:
	misc_deregister(&ISL29044_device);
	exit_misc_device_register_failed:
	exit_init_failed:
	//i2c_detach_client(client);
	exit_kfree:
	kfree(obj);
	exit:
	ISL29044_i2c_client = NULL;           
	APS_ERR("%s: err = %d\n", __func__, err);
	return err;
}
/*----------------------------------------------------------------------------*/
static int ISL29044_i2c_remove(struct i2c_client *client)
{
	int err;	
	
	if(err = ISL29044_delete_attr(&ISL29044_i2c_driver.driver))
	{
		APS_ERR("ISL29044_delete_attr fail: %d\n", err);
	} 

	if(err = misc_deregister(&ISL29044_device))
	{
		APS_ERR("misc_deregister fail: %d\n", err);    
	}
	
	ISL29044_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));

	return 0;
}
/*----------------------------------------------------------------------------*/
static int ISL29044_probe(struct platform_device *pdev) 
{
	struct alsps_hw *hw = get_cust_alsps_hw();

	ISL29044_power(hw, 1);    
	if(i2c_add_driver(&ISL29044_i2c_driver))
	{
		APS_ERR("add driver error\n");
		return -1;
	} 
	return 0;
}
/*----------------------------------------------------------------------------*/
static int ISL29044_remove(struct platform_device *pdev)
{
	struct alsps_hw *hw = get_cust_alsps_hw();
	APS_FUN();    
	ISL29044_power(hw, 0);    
	i2c_del_driver(&ISL29044_i2c_driver);
	return 0;
}
/*----------------------------------------------------------------------------*/
static struct platform_driver ISL29044_alsps_driver = {
	.probe      = ISL29044_probe,
	.remove     = ISL29044_remove,    
	.driver     = {
		.name  = "als_ps",
	}
};
/*----------------------------------------------------------------------------*/
static int __init ISL29044_init(void)
{
	APS_FUN();
	struct alsps_hw *hw = get_cust_alsps_hw();
	APS_LOG("%s: i2c_number=%d\n", __func__,hw->i2c_num); 
	i2c_register_board_info(hw->i2c_num, &i2c_ISL29044, 1);
	if(platform_driver_register(&ISL29044_alsps_driver))
	{
		APS_ERR("failed to register driver");
		return -ENODEV;
	}
	return 0;
}
/*----------------------------------------------------------------------------*/
static void __exit ISL29044_exit(void)
{
	APS_FUN();
	platform_driver_unregister(&ISL29044_alsps_driver);
}
/*----------------------------------------------------------------------------*/
module_init(ISL29044_init);
module_exit(ISL29044_exit);
/*----------------------------------------------------------------------------*/
MODULE_AUTHOR("MingHsien Hsieh");
MODULE_DESCRIPTION("ADXL345 accelerometer driver");
MODULE_LICENSE("GPL");
