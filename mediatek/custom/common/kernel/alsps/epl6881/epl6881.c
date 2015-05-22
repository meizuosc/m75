/* drivers/hwmon/mt6516/amit/epl6881.c - EPL6881 ALS/PS driver
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

// last modified in 2012.12.12 by renato pan

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
#include <linux/hwmsen_helper.h>
#include "epl6881.h"

#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>
#include <mach/eint.h>


//#include <Mt6575.h>
//bob.chen add begin 
//add for fix resume issue
#include <linux/earlysuspend.h> 
#include <linux/wakelock.h>
//add for fix resume issue end
//bob.chen add end 


#define POWER_NONE_MACRO MT65XX_POWER_NONE


//temp
#if 1 //it should be defined with dct 
#define CUST_EINT_ALS_NUM            1  
#define CUST_EINT_ALS_DEBOUNCE_CN      0
#define CUST_EINT_ALS_POLARITY         CUST_EINT_POLARITY_LOW
#define CUST_EINT_ALS_SENSITIVE        CUST_EINT_LEVEL_SENSITIVE
#define CUST_EINT_ALS_DEBOUNCE_EN      CUST_EINT_DEBOUNCE_DISABLE
#endif


/******************************************************************************
 * configuration
*******************************************************************************/

// TODO: change ps/als integrationtime
#define PS_INTT 					4
#define ALS_INTT					7

#define TXBYTES 				2
#define RXBYTES 				2
#define PACKAGE_SIZE 			2
#define I2C_RETRY_COUNT 		10

// TODO: change delay time
#define PS_DELAY 			30
#define ALS_DELAY 			55



typedef struct _epl_raw_data
{
    u8 raw_bytes[PACKAGE_SIZE];
    u16 ps_raw;
    u16 ps_state;
	u16 ps_int_state;
    u16 als_ch0_raw;
    u16 als_ch1_raw;
} epl_raw_data;


#define EPL6881_DEV_NAME     "EPL6881"


/*----------------------------------------------------------------------------*/
#define APS_TAG                  "[ALS/PS] "
#define APS_FUN(f)               printk(KERN_INFO APS_TAG"%s\n", __FUNCTION__)
#define APS_ERR(fmt, args...)    printk(KERN_ERR  APS_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define APS_LOG(fmt, args...)    printk(KERN_INFO APS_TAG fmt, ##args)
#define APS_DBG(fmt, args...)    printk(KERN_INFO fmt, ##args)                 
#define FTM_CUST_ALSPS "/data/epl6881"


/*----------------------------------------------------------------------------*/
#define mt6516_I2C_DATA_PORT        ((base) + 0x0000)
#define mt6516_I2C_SLAVE_ADDR       ((base) + 0x0004)
#define mt6516_I2C_INTR_MASK        ((base) + 0x0008)
#define mt6516_I2C_INTR_STAT        ((base) + 0x000c)
#define mt6516_I2C_CONTROL          ((base) + 0x0010)
#define mt6516_I2C_TRANSFER_LEN     ((base) + 0x0014)
#define mt6516_I2C_TRANSAC_LEN      ((base) + 0x0018)
#define mt6516_I2C_DELAY_LEN        ((base) + 0x001c)
#define mt6516_I2C_TIMING           ((base) + 0x0020)
#define mt6516_I2C_START            ((base) + 0x0024)
#define mt6516_I2C_FIFO_STAT        ((base) + 0x0030)
#define mt6516_I2C_FIFO_THRESH      ((base) + 0x0034)
#define mt6516_I2C_FIFO_ADDR_CLR    ((base) + 0x0038)
#define mt6516_I2C_IO_CONFIG        ((base) + 0x0040)
#define mt6516_I2C_DEBUG            ((base) + 0x0044)
#define mt6516_I2C_HS               ((base) + 0x0048)
#define mt6516_I2C_DEBUGSTAT        ((base) + 0x0064)
#define mt6516_I2C_DEBUGCTRL        ((base) + 0x0068)
/*----------------------------------------------------------------------------*/
static struct i2c_client *epl6881_i2c_client = NULL;


/*----------------------------------------------------------------------------*/
static const struct i2c_device_id epl6881_i2c_id[] = {{"EPL6881",0},{}};
static struct i2c_board_info __initdata i2c_EPL6881={ I2C_BOARD_INFO("EPL6881", (0X92>>1))};
/*the adapter id & i2c address will be available in customization*/
//static unsigned short epl6881_force[] = {0x00, 0x92, I2C_CLIENT_END, I2C_CLIENT_END};
//static const unsigned short *const epl6881_forces[] = { epl6881_force, NULL };
//static struct i2c_client_address_data epl6881_addr_data = { .forces = epl6881_forces,};


/*----------------------------------------------------------------------------*/
static int epl6881_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id); 
static int epl6881_i2c_remove(struct i2c_client *client);
static int epl6881_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);


/*----------------------------------------------------------------------------*/
static int epl6881_i2c_suspend(struct i2c_client *client, pm_message_t msg);
static int epl6881_i2c_resume(struct i2c_client *client);
static void epl6881_eint_func(void);
static int set_psensor_intr_threshold(uint16_t low_thd, uint16_t high_thd);
	
static struct epl6881_priv *g_epl6881_ptr = NULL;
static bool isInterrupt = false;

static DEFINE_MUTEX(sensor_lock);

/*----------------------------------------------------------------------------*/
typedef enum 
{
    CMC_TRC_ALS_DATA = 0x0001,
    CMC_TRC_PS_DATA = 0X0002,
    CMC_TRC_EINT    = 0x0004,
    CMC_TRC_IOCTL   = 0x0008,
    CMC_TRC_I2C     = 0x0010,
    CMC_TRC_CVT_ALS = 0x0020,
    CMC_TRC_CVT_PS  = 0x0040,
    CMC_TRC_DEBUG   = 0x0800,
} CMC_TRC;

/*----------------------------------------------------------------------------*/
typedef enum 
{
    CMC_BIT_ALS    = 1,
    CMC_BIT_PS     = 2,
} CMC_BIT;

/*----------------------------------------------------------------------------*/
struct epl6881_i2c_addr {    /*define a series of i2c slave address*/
    u8  write_addr;  
    u8  ps_thd;     /*PS INT threshold*/
};

/*----------------------------------------------------------------------------*/
struct epl6881_priv 
{
    struct alsps_hw  *hw;
    struct i2c_client *client;
    struct delayed_work  eint_work;

    /*i2c address group*/
   struct epl6881_i2c_addr  addr;
    
    int enable_pflag;
    int enable_lflag;

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
    u16          ps;
    u8          _align;
    u8   		als_thd_level;
    bool   		als_enable;    /*record current als status*/
    bool    	ps_enable;     /*record current ps status*/
    ulong       enable;         /*record HAL enalbe status*/
    ulong       pending_intr;   /*pending interrupt*/
    //ulong        first_read;   // record first read ps and als

	/*data*/
    u16         als_level_num;
    u16         als_value_num;
    u32         als_level[C_CUST_ALS_LEVEL-1];
    u32         als_value[C_CUST_ALS_LEVEL];

	/*early suspend*/
#if defined(CONFIG_HAS_EARLYSUSPEND)
    struct early_suspend    early_drv;
#endif
};



/*----------------------------------------------------------------------------*/
static struct i2c_driver epl6881_i2c_driver =
{
	.probe      = epl6881_i2c_probe,
	.remove     = epl6881_i2c_remove,
	.detect     = epl6881_i2c_detect,
	.suspend    = epl6881_i2c_suspend,
	.resume     = epl6881_i2c_resume,
	.id_table   = epl6881_i2c_id,
	//.address_data = &epl6881_addr_data,
	.driver = {
		//.owner          = THIS_MODULE,
		.name           = EPL6881_DEV_NAME,
	},
};


static struct epl6881_priv *epl6881_obj = NULL;
static struct platform_driver epl6881_alsps_driver;
static struct wake_lock ps_lock; /* Bob.chen add for if ps run, the system forbid to goto sleep mode. */
static epl_raw_data	gRawData;

//static struct wake_lock als_lock; /* Bob.chen add for if ps run, the system forbid to goto sleep mode. */


/*
//====================I2C write operation===============//
//regaddr: ELAN epl6881 Register Address.
//bytecount: How many bytes to be written to epl6881 register via i2c bus.
//txbyte: I2C bus transmit byte(s). Single byte(0X01) transmit only slave address.
//data: setting value.
//
// Example: If you want to write single byte to 0x1D register address, show below
//	      elan_epl6881_I2C_Write(client,0x1D,0x01,0X02,0xff);
//
*/
static int elan_epl6881_I2C_Write(struct i2c_client *client, uint8_t regaddr, uint8_t bytecount, uint8_t txbyte, uint8_t data)
{
    uint8_t buffer[2];
    int ret = 0;
    int retry;

   // printk("[ELAN epl6881] %s\n", __func__);

    buffer[0] = (regaddr<<3) | bytecount ;
    buffer[1] = data;


    //printk("---buffer data (%x) (%x)---\n",buffer[0],buffer[1]);

    for(retry = 0; retry < I2C_RETRY_COUNT; retry++)
    {
        ret = i2c_master_send(client, buffer, txbyte);

        if (ret == txbyte)
        {
            break;
        }

        printk("i2c write error,TXBYTES %d\r\n",ret);
        mdelay(10);
    }


    if(retry>=I2C_RETRY_COUNT)
    {
        printk(KERN_ERR "[ELAN epl6881 error] %s i2c write retry over %d\n",__func__, I2C_RETRY_COUNT);
        return -EINVAL;
    }

    return ret;
}




/*
//====================I2C read operation===============//
*/
static int elan_epl6881_I2C_Read(struct i2c_client *client)
{
    uint8_t buffer[RXBYTES];
    int ret = 0, i =0;
    int retry;

    //printk("[ELAN epl6881] %s\n", __func__);

    for(retry = 0; retry < I2C_RETRY_COUNT; retry++)
    {
        ret = i2c_master_recv(client, buffer, RXBYTES);

        if (ret == RXBYTES)
            break;

        APS_ERR("i2c read error,RXBYTES %d\r\n",ret);
        mdelay(10);
    }

    if(retry>=I2C_RETRY_COUNT)
    {
        APS_ERR(KERN_ERR "[ELAN epl6881 error] %s i2c read retry over %d\n",__func__, I2C_RETRY_COUNT);
        return -EINVAL;
    }

    for(i=0; i<PACKAGE_SIZE; i++)
        gRawData.raw_bytes[i] = buffer[i];

    //printk("-----Receive data byte1 (%x) byte2 (%x)-----\n",buffer[0],buffer[1]);


    return ret;
}




static int elan_epl6881_psensor_enable(struct epl6881_priv *epl_data, int enable)
{
    int ret = 0;
    uint8_t regdata;
    struct i2c_client *client = epl_data->client;

    APS_LOG("----[ELAN epl6881] %s enable = %d\n", __func__, enable);

    epl_data->enable_pflag = enable;
    ret = elan_epl6881_I2C_Write(client,REG_9,W_SINGLE_BYTE,0x02,EPL_INT_DISABLE);
	
    if(enable)
    {    
        regdata = EPL_SENSING_2_TIME | EPL_PS_MODE | EPL_L_GAIN ;
        regdata = regdata | (isInterrupt ? EPL_C_SENSING_MODE : EPL_S_SENSING_MODE);
        ret = elan_epl6881_I2C_Write(client,REG_0,W_SINGLE_BYTE,0X02,regdata);

        regdata = PS_INTT<<4 | EPL_PST_1_TIME | EPL_10BIT_ADC;
        ret = elan_epl6881_I2C_Write(client,REG_1,W_SINGLE_BYTE,0X02,regdata);

	set_psensor_intr_threshold(epl_data->hw ->ps_threshold_low,epl_data->hw ->ps_threshold_high);
	
        ret = elan_epl6881_I2C_Write(client,REG_7,W_SINGLE_BYTE,0X02,EPL_C_RESET);
        ret = elan_epl6881_I2C_Write(client,REG_7,W_SINGLE_BYTE,0x02,EPL_C_START_RUN);
				
	//zijian	msleep(PS_DELAY);
		mdelay(PS_DELAY);
	
        if(isInterrupt)
        {
			elan_epl6881_I2C_Write(client,REG_13,R_SINGLE_BYTE,0x01,0);
			elan_epl6881_I2C_Read(client);
			gRawData.ps_state= !((gRawData.raw_bytes[0]&0x04)>>2);
        
			if(gRawData.ps_state != gRawData.ps_int_state)
			{
			elan_epl6881_I2C_Write(client,REG_9,W_SINGLE_BYTE,0x02,EPL_INT_FRAME_ENABLE);
			}
			else
			{
            		elan_epl6881_I2C_Write(client,REG_9,W_SINGLE_BYTE,0x02,EPL_INT_ACTIVE_LOW);
			}
	}

    }
    else
    {
        regdata = EPL_SENSING_2_TIME | EPL_PS_MODE | EPL_L_GAIN ;
        regdata = regdata | EPL_S_SENSING_MODE;
        ret = elan_epl6881_I2C_Write(client,REG_0,W_SINGLE_BYTE,0X02,regdata);

    }

    if(ret<0)
    {
        APS_ERR("[ELAN epl6881 error]%s: ps enable %d fail\n",__func__,ret);
    }
    else
    {
    	ret = 0;
    }

    return ret;
}


static int elan_epl6881_lsensor_enable(struct epl6881_priv *epl_data, int enable)
{
    int ret = 0;
    uint8_t regdata;
	int mode;
    struct i2c_client *client = epl_data->client;


    APS_LOG("[ELAN epl6881] %s enable = %d\n", __func__, enable);


    epl_data->enable_lflag = enable;

    if(enable)
    {
        regdata = EPL_INT_DISABLE;
        ret = elan_epl6881_I2C_Write(client,REG_9,W_SINGLE_BYTE,0x02, regdata);

        regdata = EPL_S_SENSING_MODE | EPL_SENSING_8_TIME | EPL_ALS_MODE | EPL_AUTO_GAIN;
        ret = elan_epl6881_I2C_Write(client,REG_0,W_SINGLE_BYTE,0X02,regdata);

        regdata = ALS_INTT<<4 | EPL_PST_1_TIME | EPL_10BIT_ADC;
        ret = elan_epl6881_I2C_Write(client,REG_1,W_SINGLE_BYTE,0X02,regdata);

        ret = elan_epl6881_I2C_Write(client,REG_10,W_SINGLE_BYTE,0X02,0x3e);
        ret = elan_epl6881_I2C_Write(client,REG_11,W_SINGLE_BYTE,0x02,0x3e);
		
        ret = elan_epl6881_I2C_Write(client,REG_7,W_SINGLE_BYTE,0X02,EPL_C_RESET);
        ret = elan_epl6881_I2C_Write(client,REG_7,W_SINGLE_BYTE,0x02,EPL_C_START_RUN);
//		msleep(ALS_DELAY);
		mdelay(ALS_DELAY);
		
    }


    if(ret<0)
    {
        APS_ERR("[ELAN epl6881 error]%s: als_enable %d fail\n",__func__,ret);
    }
    else
    {
    	ret = 0;
    }
	
    return ret;
}

static int epl6881_get_als_value(struct epl6881_priv *obj, u16 als)
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
        APS_DBG("ALS: %05d => %05d\n", als, obj->hw->als_value[idx]);
        return obj->hw->als_value[idx];
    }
    else
    {
        APS_ERR("ALS: %05d => %05d (-1)\n", als, obj->hw->als_value[idx]);
        return -1;
    }
}


static int set_psensor_intr_threshold(uint16_t low_thd, uint16_t high_thd)
{
    int ret = 0;
    struct epl6881_priv *epld = epl6881_obj;
    struct i2c_client *client = epld->client;

    uint8_t high_msb ,high_lsb, low_msb, low_lsb;

    APS_LOG("%s\n", __func__);

    high_msb = (uint8_t) (high_thd >> 8);
    high_lsb = (uint8_t) (high_thd & 0x00ff);
    low_msb  = (uint8_t) (low_thd >> 8);
    low_lsb  = (uint8_t) (low_thd & 0x00ff);

    APS_LOG("%s: low_thd = 0x%X, high_thd = 0x%x \n",__func__, low_thd, high_thd);

    elan_epl6881_I2C_Write(client,REG_2,W_SINGLE_BYTE,0x02,high_lsb);
    elan_epl6881_I2C_Write(client,REG_3,W_SINGLE_BYTE,0x02,high_msb);
    elan_epl6881_I2C_Write(client,REG_4,W_SINGLE_BYTE,0x02,low_lsb);
    elan_epl6881_I2C_Write(client,REG_5,W_SINGLE_BYTE,0x02,low_msb);

    return ret;
}



/*----------------------------------------------------------------------------*/
static void epl6881_dumpReg(struct i2c_client *client)
{
  APS_LOG("chip id REG 0x00 value = %8x\n", i2c_smbus_read_byte_data(client, 0x00));
  APS_LOG("chip id REG 0x01 value = %8x\n", i2c_smbus_read_byte_data(client, 0x08));
  APS_LOG("chip id REG 0x02 value = %8x\n", i2c_smbus_read_byte_data(client, 0x10));
  APS_LOG("chip id REG 0x03 value = %8x\n", i2c_smbus_read_byte_data(client, 0x18));
  APS_LOG("chip id REG 0x04 value = %8x\n", i2c_smbus_read_byte_data(client, 0x20));
  APS_LOG("chip id REG 0x05 value = %8x\n", i2c_smbus_read_byte_data(client, 0x28));
  APS_LOG("chip id REG 0x06 value = %8x\n", i2c_smbus_read_byte_data(client, 0x30));
  APS_LOG("chip id REG 0x07 value = %8x\n", i2c_smbus_read_byte_data(client, 0x38));
  APS_LOG("chip id REG 0x09 value = %8x\n", i2c_smbus_read_byte_data(client, 0x48));
  APS_LOG("chip id REG 0x0D value = %8x\n", i2c_smbus_read_byte_data(client, 0x68));
  APS_LOG("chip id REG 0x0E value = %8x\n", i2c_smbus_read_byte_data(client, 0x70));
  APS_LOG("chip id REG 0x0F value = %8x\n", i2c_smbus_read_byte_data(client, 0x71));
  APS_LOG("chip id REG 0x10 value = %8x\n", i2c_smbus_read_byte_data(client, 0x80));
  APS_LOG("chip id REG 0x11 value = %8x\n", i2c_smbus_read_byte_data(client, 0x88));
  APS_LOG("chip id REG 0x13 value = %8x\n", i2c_smbus_read_byte_data(client, 0x98));

}


/*----------------------------------------------------------------------------*/
int hw8k_init_device(struct i2c_client *client)
{
	APS_LOG("hw8k_init_device.........\r\n");

	epl6881_i2c_client=client;
	
	APS_LOG(" I2C Addr==[0x%x],line=%d\n",epl6881_i2c_client->addr,__LINE__);

	return 0;
}

/*----------------------------------------------------------------------------*/
int epl6881_get_addr(struct alsps_hw *hw, struct epl6881_i2c_addr *addr)
{
	if(!hw || !addr)
	{
		return -EFAULT;
	}
	addr->write_addr= hw->i2c_addr[0];
	return 0;
}


/*----------------------------------------------------------------------------*/
static void epl6881_power(struct alsps_hw *hw, unsigned int on) 
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
			if(!hwPowerOn(hw->power_id, hw->power_vol, "EPL6881")) 
			{
				APS_ERR("power on fails!!\n");
			}
		}
		else
		{
			if(!hwPowerDown(hw->power_id, "EPL6881")) 
			{
				APS_ERR("power off fail!!\n");   
			}
		}
	}
	power_on = on;
}



/*----------------------------------------------------------------------------*/
static int epl6881_check_intr(struct i2c_client *client) 
{
	struct epl6881_priv *obj = i2c_get_clientdata(client);
	int mode;

	APS_LOG("int pin = %d\n", mt_get_gpio_in(GPIO_ALS_EINT_PIN));
	
	//if (mt_get_gpio_in(GPIO_ALS_EINT_PIN) == 1) /*skip if no interrupt*/  
	 //   return 0;

	elan_epl6881_I2C_Write(obj->client,REG_13,R_SINGLE_BYTE,0x01,0);
	elan_epl6881_I2C_Read(obj->client);
	mode = gRawData.raw_bytes[0]&(3<<4);
	APS_LOG("	---mode %0x\n", mode);

	if(mode==0x10)// PS
	{
		set_bit(CMC_BIT_PS, &obj->pending_intr);
	}
	else
	{
	   	clear_bit(CMC_BIT_PS, &obj->pending_intr);
	}


	if(atomic_read(&obj->trace) & CMC_TRC_DEBUG)
	{
		APS_LOG("check intr: 0x%08X\n", obj->pending_intr);
	}

	return 0;

}



/*----------------------------------------------------------------------------*/

int epl6881_read_als(struct i2c_client *client, u16 *data)
{
    struct epl6881_priv *obj = i2c_get_clientdata(client);

    if(client == NULL)
    {
        APS_DBG("CLIENT CANN'T EQUL NULL\n");
        return -1;
    }

    elan_epl6881_I2C_Write(obj->client,REG_14,R_TWO_BYTE,0x01,0x00);
    elan_epl6881_I2C_Read(obj->client);
    gRawData.als_ch0_raw = (gRawData.raw_bytes[1]<<8) | gRawData.raw_bytes[0];

    elan_epl6881_I2C_Write(obj->client,REG_16,R_TWO_BYTE,0x01,0x00);
    elan_epl6881_I2C_Read(obj->client);
    gRawData.als_ch1_raw = (gRawData.raw_bytes[1]<<8) | gRawData.raw_bytes[0];
    *data =  gRawData.als_ch1_raw;

	if(atomic_read(&obj->trace) & CMC_TRC_ALS_DATA)
	{
		APS_LOG("read als raw data = %d\n", gRawData.als_ch1_raw);
	}
    return 0;
}


#if 1


/*----------------------------------------------------------------------------*/
long epl6881_read_ps(struct i2c_client *client, u16 *data)
{
    struct epl6881_priv *obj = i2c_get_clientdata(client);
	
    if(client == NULL)
    {
        APS_DBG("CLIENT CANN'T EQUL NULL\n");
        return -1;
    }

    elan_epl6881_I2C_Write(obj->client,REG_13,R_SINGLE_BYTE,0x01,0);
    elan_epl6881_I2C_Read(obj->client);
    gRawData.ps_state= !((gRawData.raw_bytes[0]&0x04)>>2);

    elan_epl6881_I2C_Write(obj->client,REG_16,R_TWO_BYTE,0x01,0x00);
    elan_epl6881_I2C_Read(obj->client);
    gRawData.ps_raw = (gRawData.raw_bytes[1]<<8) | gRawData.raw_bytes[0];

    *data = gRawData.ps_raw ;
    APS_LOG("read ps raw data = %d\n", gRawData.ps_raw);
	APS_LOG("read ps binary data = %d\n", gRawData.ps_state);

    return 0;
}

#else
/*----------------------------------------------------------------------------*/
long epl6881_read_ps(struct i2c_client *client, u16 *data)
{
    struct epl6881_priv *obj = i2c_get_clientdata(client);
	
    if(client == NULL)
    {
        APS_DBG("CLIENT CANN'T EQUL NULL\n");
        return -1;
    }

    elan_epl6881_I2C_Write(obj->client,REG_13,R_SINGLE_BYTE,0x01,0);
    elan_epl6881_I2C_Read(obj->client);
    //gRawData.ps_state= !((gRawData.raw_bytes[0]&0x04)>>2);
    gRawData.ps_state= gRawData.raw_bytes[0];

    elan_epl6881_I2C_Write(obj->client,REG_16,R_TWO_BYTE,0x01,0x00);
    elan_epl6881_I2C_Read(obj->client);
    gRawData.ps_raw = (gRawData.raw_bytes[1]<<8) | gRawData.raw_bytes[0];

    *data = gRawData.ps_raw ;
	if(atomic_read(&obj->trace) & CMC_TRC_PS_DATA)
	{
	elan_epl6881_I2C_Write(obj->client,REG_0,R_SINGLE_BYTE,0x01,0);
    elan_epl6881_I2C_Read(obj->client);
    
    	APS_LOG("read ps reg0x00 = %d\n", gRawData.raw_bytes[0]);		
	elan_epl6881_I2C_Write(obj->client,REG_7,R_SINGLE_BYTE,0x01,0);
    elan_epl6881_I2C_Read(obj->client);
    
    	APS_LOG("read ps reg0x07 = %d\n", gRawData.raw_bytes[0]);	
    	APS_LOG("read ps raw data = %d\n", gRawData.ps_raw);
		APS_LOG("read ps binary data = %d\n", gRawData.ps_state);
	}
    return 0;
}

#endif


/*----------------------------------------------------------------------------*/
void epl6881_eint_func(void)
{
	struct epl6881_priv *obj = g_epl6881_ptr;
	
	 APS_LOG(" interrupt fuc\n");

	if(!obj)
	{
		return;
	}

   mt65xx_eint_mask(CUST_EINT_ALS_NUM);  
   schedule_delayed_work(&obj->eint_work, 0);
}



/*----------------------------------------------------------------------------*/
static void epl6881_eint_work(struct work_struct *work)
{
	struct epl6881_priv *epld = g_epl6881_ptr;
	int err;
	hwm_sensor_data sensor_data;
	int static is_update = false;
	
	APS_LOG("zijian------------xxxxx eint work entry\n");

	if(epld->enable_pflag==0)
		goto exit;

	mutex_lock(&sensor_lock);
	if((err = epl6881_check_intr(epld->client)))
	{
		APS_ERR("check intrs: %d\n", err);
	}
	is_update = false;

	
	APS_LOG("zijian------------epld->pending_intr=%d \n",epld->pending_intr);
	if((1<<CMC_BIT_PS) & epld->pending_intr)
	{
		elan_epl6881_I2C_Write(epld->client,REG_13,R_SINGLE_BYTE,0x01,0);
		elan_epl6881_I2C_Read(epld->client);
		gRawData.ps_int_state= !((gRawData.raw_bytes[0]&0x04)>>2);
		APS_LOG("zijian		---ps state = %d\n", gRawData.ps_int_state);

	    elan_epl6881_I2C_Write(epld->client,REG_16,R_TWO_BYTE,0x01,0x00);
	    elan_epl6881_I2C_Read(epld->client);
	    gRawData.ps_raw = (gRawData.raw_bytes[1]<<8) | gRawData.raw_bytes[0];
		APS_LOG("zijian ps raw_data = %d\n", gRawData.ps_raw);

		sensor_data.values[0] = gRawData.ps_int_state;
		sensor_data.value_divide = 1;
		sensor_data.status = SENSOR_STATUS_ACCURACY_MEDIUM;
		
		is_update = true;
#if 0 //zijian move it to ##1
		//let up layer to know
		if((err = hwmsen_get_interrupt_data(ID_PROXIMITY, &sensor_data)))
		{
			APS_ERR("get interrupt data failed\n");
			APS_ERR("call hwmsen_get_interrupt_data fail = %d\n", err);
		}
#endif		
	}

	elan_epl6881_I2C_Write(epld->client,REG_9,W_SINGLE_BYTE,0x02,EPL_INT_ACTIVE_LOW);
	elan_epl6881_I2C_Write(epld->client,REG_7,W_SINGLE_BYTE,0x02,EPL_DATA_UNLOCK);

	mutex_unlock(&sensor_lock);

			//let up layer to know
	if(is_update == true)		
	{
		APS_LOG("zijian------- update psensor data , value = %d \n",sensor_data.values[0]);
		if((err = hwmsen_get_interrupt_data(ID_PROXIMITY, &sensor_data)))
		{
			APS_ERR("get interrupt data failed\n");
			APS_ERR("call hwmsen_get_interrupt_data fail = %d\n", err);
		}
	}
	
exit:		

	APS_LOG("##############xxxxx eint work exit\n");			

	mt65xx_eint_unmask(CUST_EINT_ALS_NUM);  


}



/*----------------------------------------------------------------------------*/
int epl6881_setup_eint(struct i2c_client *client)
{
	struct epl6881_priv *obj = i2c_get_clientdata(client);        

	APS_LOG("epl6881_setup_eint\n");
	

	g_epl6881_ptr = obj;

	/*configure to GPIO function, external interrupt*/

	mt_set_gpio_mode(GPIO_ALS_EINT_PIN, GPIO_ALS_EINT_PIN_M_EINT);
	mt_set_gpio_dir(GPIO_ALS_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_ALS_EINT_PIN, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(GPIO_ALS_EINT_PIN, GPIO_PULL_UP);


	mt65xx_eint_set_sens(CUST_EINT_ALS_NUM, CUST_EINT_EDGE_SENSITIVE);
	mt65xx_eint_set_polarity(CUST_EINT_ALS_NUM, CUST_EINT_ALS_POLARITY);
	mt65xx_eint_set_hw_debounce(CUST_EINT_ALS_NUM, CUST_EINT_ALS_DEBOUNCE_CN);
	mt65xx_eint_registration(CUST_EINT_ALS_NUM, CUST_EINT_ALS_DEBOUNCE_EN, CUST_EINT_POLARITY_LOW, epl6881_eint_func, 0);
	mt65xx_eint_unmask(CUST_EINT_ALS_NUM);	


    return 0;
}




/*----------------------------------------------------------------------------*/
static int epl6881_init_client(struct i2c_client *client)
{
	struct epl6881_priv *obj = i2c_get_clientdata(client);
	int err=0;

	APS_LOG("[Agold spl] I2C Addr==[0x%x],line=%d\n",epl6881_i2c_client->addr,__LINE__);
	
/*  interrupt mode */


	APS_FUN();

	if(obj->hw->polling_mode_ps == 0)
	{
		mt65xx_eint_mask(CUST_EINT_ALS_NUM);  

		if((err = epl6881_setup_eint(client)))
		{
			APS_ERR("setup eint: %d\n", err);
			return err;
		}
		APS_LOG("epl6881 interrupt setup\n");
	}


	if((err = hw8k_init_device(client)) != 0)
	{
		APS_ERR("init dev: %d\n", err);
		return err;
	}


	if((err = epl6881_check_intr(client)))
	{
		APS_ERR("check/clear intr: %d\n", err);
		return err;
	}


/*  interrupt mode */
 //if(obj->hw->polling_mode_ps == 0)
   //     mt65xx_eint_unmask(CUST_EINT_ALS_NUM);  	

	return err;
}


/*----------------------------------------------------------------------------*/
static ssize_t epl6881_show_reg(struct device_driver *ddri, char *buf)
{
	if(!epl6881_obj)
	{
		APS_ERR("epl6881_obj is null!!\n");
		return 0;
	}
	ssize_t len = 0;
	struct i2c_client *client = epl6881_obj->client;

	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x00 value = %8x\n", i2c_smbus_read_byte_data(client, 0x00));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x01 value = %8x\n", i2c_smbus_read_byte_data(client, 0x08));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x02 value = %8x\n", i2c_smbus_read_byte_data(client, 0x10));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x03 value = %8x\n", i2c_smbus_read_byte_data(client, 0x18));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x04 value = %8x\n", i2c_smbus_read_byte_data(client, 0x20));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x05 value = %8x\n", i2c_smbus_read_byte_data(client, 0x28));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x06 value = %8x\n", i2c_smbus_read_byte_data(client, 0x30));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x07 value = %8x\n", i2c_smbus_read_byte_data(client, 0x38));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x09 value = %8x\n", i2c_smbus_read_byte_data(client, 0x48));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x0D value = %8x\n", i2c_smbus_read_byte_data(client, 0x68));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x0E value = %8x\n", i2c_smbus_read_byte_data(client, 0x70));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x0F value = %8x\n", i2c_smbus_read_byte_data(client, 0x71));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x10 value = %8x\n", i2c_smbus_read_byte_data(client, 0x80));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x11 value = %8x\n", i2c_smbus_read_byte_data(client, 0x88));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x13 value = %8x\n", i2c_smbus_read_byte_data(client, 0x98));
	
    return len;

}

/*----------------------------------------------------------------------------*/
static ssize_t epl6881_show_status(struct device_driver *ddri, char *buf)
{
    ssize_t len = 0;
    struct epl6881_priv *epld = epl6881_obj;

    if(!epl6881_obj)
    {
        APS_ERR("epl6881_obj is null!!\n");
        return 0;
    }
    elan_epl6881_I2C_Write(epld->client,REG_7,W_SINGLE_BYTE,0x02,EPL_DATA_LOCK);

    elan_epl6881_I2C_Write(epld->client,REG_16,R_TWO_BYTE,0x01,0x00);
    elan_epl6881_I2C_Read(epld->client);
    gRawData.ps_raw = (gRawData.raw_bytes[1]<<8) | gRawData.raw_bytes[0];
    APS_LOG("ch1 raw_data = %d\n", gRawData.ps_raw);

    elan_epl6881_I2C_Write(epld->client,REG_7,W_SINGLE_BYTE,0x02,EPL_DATA_UNLOCK);
    len += snprintf(buf+len, PAGE_SIZE-len, "ch1 raw is %d\n",gRawData.ps_raw);
    return len;
}



/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(status,  S_IWUSR | S_IRUGO, epl6881_show_status,  NULL);
static DRIVER_ATTR(reg,     S_IWUSR | S_IRUGO, epl6881_show_reg,   NULL);

/*----------------------------------------------------------------------------*/
static struct device_attribute * epl6881_attr_list[] =
{
    &driver_attr_status,
    &driver_attr_reg,
};

/*----------------------------------------------------------------------------*/
static int epl6881_create_attr(struct device_driver *driver) 
{
	int idx, err = 0;
	int num = (int)(sizeof(epl6881_attr_list)/sizeof(epl6881_attr_list[0]));
	if (driver == NULL)
	{
		return -EINVAL;
	}

	for(idx = 0; idx < num; idx++)
	{
		if(err = driver_create_file(driver, epl6881_attr_list[idx]))
		{            
			APS_ERR("driver_create_file (%s) = %d\n", epl6881_attr_list[idx]->attr.name, err);
			break;
		}
	}    
	return err;
}



/*----------------------------------------------------------------------------*/
static int epl6881_delete_attr(struct device_driver *driver)
	{
	int idx ,err = 0;
	int num = (int)(sizeof(epl6881_attr_list)/sizeof(epl6881_attr_list[0]));

	if (!driver)
	return -EINVAL;

	for (idx = 0; idx < num; idx++) 
	{
		driver_remove_file(driver, epl6881_attr_list[idx]);
	}

	return err;
}



/****************************************************************************** 
 * Function Configuration
******************************************************************************/
static int epl6881_open(struct inode *inode, struct file *file)
{
	file->private_data = epl6881_i2c_client;

	APS_FUN();

	if (!file->private_data)
	{
		APS_ERR("null pointer!!\n");
		return -EINVAL;
	}

	return nonseekable_open(inode, file);
}

/*----------------------------------------------------------------------------*/
static int epl6881_release(struct inode *inode, struct file *file)
{
        APS_FUN();
	file->private_data = NULL;
	return 0;
}

/*----------------------------------------------------------------------------*/
static long epl6881_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct i2c_client *client = (struct i2c_client*)file->private_data;
	struct epl6881_priv *obj = i2c_get_clientdata(client);  
	int err = 0;
	void __user *ptr = (void __user*) arg;
	int dat;
	uint32_t enable;

	//APS_LOG("---epl6881_ioctll- ALSPS_SET_PS_CALIBRATION  = %x, cmd = %x........\r\n", ALSPS_SET_PS_CALIBRATION, cmd);
	
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
				if(isInterrupt)
				{
					if((err = elan_epl6881_psensor_enable(obj, 1))!=0)
                    {
                        APS_ERR("enable ps fail: %d\n", err);
                        return -1;
                    }
				}
				set_bit(CMC_BIT_PS, &obj->enable);
			}
			else
			{
				if(isInterrupt)
				{
					if((err = elan_epl6881_psensor_enable(obj, 0))!=0)
                    {
                        APS_ERR("disable ps fail: %d\n", err);
                        return -1;
                    }
				}
				clear_bit(CMC_BIT_PS, &obj->enable);
			}
			break;


		case ALSPS_GET_PS_MODE:
			enable=test_bit(CMC_BIT_PS, &obj->enable);
			if(copy_to_user(ptr, &enable, sizeof(enable)))
			{
				err = -EFAULT;
				goto err_out;
			}
			break;
			

		case ALSPS_GET_PS_DATA: 
			if((err = elan_epl6881_psensor_enable(obj, 1))!=0)
            {
                APS_ERR("enable ps fail: %d\n", err);
                return -1;
            }
	        epl6881_read_ps(obj->client, &obj->ps);
			dat = gRawData.ps_state;

            APS_LOG("ioctl ps state value = %d \n", dat);

			if(copy_to_user(ptr, &dat, sizeof(dat)))
			{
				err = -EFAULT;
				goto err_out;
			}  
			break;


		case ALSPS_GET_PS_RAW_DATA:  
			if((err = elan_epl6881_psensor_enable(obj, 1))!=0)
            {
                APS_ERR("enable ps fail: %d\n", err);
                return -1;
            }
            epl6881_read_ps(obj->client, &obj->ps);
			dat = gRawData.ps_raw;

            APS_LOG("ioctl ps raw value = %d \n", dat);
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
				set_bit(CMC_BIT_ALS, &obj->enable);
			}
			else
			{
				clear_bit(CMC_BIT_ALS, &obj->enable);
			}
			break;



		case ALSPS_GET_ALS_MODE:
			enable=test_bit(CMC_BIT_ALS, &obj->enable);
			if(copy_to_user(ptr, &enable, sizeof(enable)))
			{
				err = -EFAULT;
				goto err_out;
			}
			break;



		case ALSPS_GET_ALS_DATA: 
            if((err = elan_epl6881_lsensor_enable(obj, 1))!=0)
            {
                APS_ERR("disable als fail: %d\n", err);
                return -1;
            }

            epl6881_read_als(obj->client, &obj->als);
            dat = epl6881_get_als_value(obj, obj->als);
            APS_LOG("ioctl get als data = %d\n", dat);


            if(obj->enable_pflag && isInterrupt)
            {
                if((err = elan_epl6881_psensor_enable(obj, 1))!=0)
                {
                    APS_ERR("disable ps fail: %d\n", err);
                    return -1;
                }
            }

             if(copy_to_user(ptr, &dat, sizeof(dat)))
			{
				err = -EFAULT;
				goto err_out;
			}              
			break;


		case ALSPS_GET_ALS_RAW_DATA:    
            if((err = elan_epl6881_lsensor_enable(obj, 1))!=0)
            {
                APS_ERR("disable als fail: %d\n", err);
                return -1;
            }

            epl6881_read_als(obj->client, &obj->als);
			dat = obj->als;
            printk("ioctl get als raw data = %d\n", dat);


            if(obj->enable_pflag && isInterrupt)
            {
                if((err = elan_epl6881_psensor_enable(obj, 1))!=0)
                {
                    APS_ERR("disable ps fail: %d\n", err);
                    return -1;
                }
            }
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
static struct file_operations epl6881_fops =
{
	.owner = THIS_MODULE,
	.open = epl6881_open,
	.release = epl6881_release,
	.unlocked_ioctl = epl6881_unlocked_ioctl,
};


/*----------------------------------------------------------------------------*/
static struct miscdevice epl6881_device =
{
	.minor = MISC_DYNAMIC_MINOR,
	.name = "als_ps",
	.fops = &epl6881_fops,
};


/*----------------------------------------------------------------------------*/
static int epl6881_i2c_suspend(struct i2c_client *client, pm_message_t msg) 
{
	struct epl6881_priv *obj = i2c_get_clientdata(client);    
	int err;
	APS_FUN();    
#if 0
	if(msg.event == PM_EVENT_SUSPEND)
	{   
		if(!obj)
		{
			APS_ERR("null pointer!!\n");
			return -EINVAL;
		}

		atomic_set(&obj->als_suspend, 1);
		if((err = elan_epl6881_lsensor_enable(obj, 0))!=0)
		{
			APS_ERR("disable als: %d\n", err);
			return err;
		}

		atomic_set(&obj->ps_suspend, 1);
		if((err = elan_epl6881_psensor_enable(obj, 0))!=0)
		{
			APS_ERR("disable ps:  %d\n", err);
			return err;
		}

		epl6881_power(obj->hw, 0);
	}
#endif
	return 0;

}



/*----------------------------------------------------------------------------*/
static int epl6881_i2c_resume(struct i2c_client *client)
{
	struct epl6881_priv *obj = i2c_get_clientdata(client);        
	int err;
	APS_FUN();
#if 0
	if(!obj)
	{
		APS_ERR("null pointer!!\n");
		return -EINVAL;
	}	

	epl6881_power(obj->hw, 1);

	msleep(50);

	if(err = epl6881_init_client(client))
	{
		APS_ERR("initialize client fail!!\n");
		return err;
	}

	atomic_set(&obj->als_suspend, 0);
    if(test_bit(CMC_BIT_ALS, &obj->enable))
    {
        if((err = elan_epl6881_lsensor_enable(obj, 1))!=0)
        {
            APS_ERR("enable als fail: %d\n", err);
        }
    }
    atomic_set(&obj->ps_suspend, 0);
    if(test_bit(CMC_BIT_PS,  &obj->enable))
    {
        if((err = elan_epl6881_psensor_enable(obj, 1))!=0)
        {
            APS_ERR("enable ps fail: %d\n", err);
        }
    }


    if(obj->hw->polling_mode_ps == 0)
        epl6881_setup_eint(client);
#endif

	return 0;
}



/*----------------------------------------------------------------------------*/
static void epl6881_early_suspend(struct early_suspend *h) 
{   
	/*early_suspend is only applied for ALS*/
	struct epl6881_priv *obj = container_of(h, struct epl6881_priv, early_drv);   
	int err;
	APS_FUN();    

	if(!obj)
	{
		APS_ERR("null pointer!!\n");
		return;
	}
	
	mutex_lock(&sensor_lock);

	atomic_set(&obj->als_suspend, 1);    

	if(test_bit(CMC_BIT_ALS, &obj->enable))
	{
		if((err = elan_epl6881_lsensor_enable(obj, 0)) != 0)
		{
			APS_ERR("disable als fail: %d\n", err);        
		}
	}
	mutex_unlock(&sensor_lock);
	
}



/*----------------------------------------------------------------------------*/
static void epl6881_late_resume(struct early_suspend *h)
{  
	/*late_resume is only applied for ALS*/
	struct epl6881_priv *obj = container_of(h, struct epl6881_priv, early_drv);         
	int err;
	APS_FUN();

	if(!obj)
	{
		APS_ERR("null pointer!!\n");
		return;
	}

	mutex_lock(&sensor_lock);

	atomic_set(&obj->als_suspend, 0);

	if(test_bit(CMC_BIT_ALS, &obj->enable))
	{

		if((err = elan_epl6881_lsensor_enable(obj, 1)) != 0)
		{
			APS_ERR("enable als fail: %d\n", err);        
		}
	}
	mutex_unlock(&sensor_lock);

	

}


/*----------------------------------------------------------------------------*/
int epl6881_ps_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value;
	hwm_sensor_data* sensor_data;
	struct epl6881_priv *obj = (struct epl6881_priv *)self;

	APS_FUN();


	APS_LOG("epl6881_ps_operate command = %x\n",command);
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
				APS_LOG("ps enable = %d\n", value);

				mutex_lock(&sensor_lock);

	            if(value)
                {
                    if((err = elan_epl6881_psensor_enable(obj, 1))!=0)
                    {
                        APS_ERR("enable ps fail: %d\n", err);
						mutex_unlock(&sensor_lock);
                        return -1;
                    }
              
                    set_bit(CMC_BIT_PS, &obj->enable);
                }
                else
                {
					if((err = elan_epl6881_psensor_enable(obj, 0))!=0)
					{
						APS_ERR("disable ps fail: %d\n", err);
						mutex_unlock(&sensor_lock);						
						return -1;
					}
                	clear_bit(CMC_BIT_PS, &obj->enable);
					
                }
				
				mutex_unlock(&sensor_lock);
			}

			break;



		case SENSOR_GET_DATA:
			APS_LOG(" get ps data !!!!!!\n");
            if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
            {
                APS_ERR("get sensor data parameter error!\n");
                err = -EINVAL;
            }
            else
            {
            
				mutex_lock(&sensor_lock);
                if((err = elan_epl6881_psensor_enable(epl6881_obj, 1))!=0)
                {
                    APS_ERR("```````enable ps fail: %d\n", err);
					mutex_unlock(&sensor_lock);
                    return -1;
                }
                epl6881_read_ps(epl6881_obj->client, &epl6881_obj->ps);
				mutex_unlock(&sensor_lock);

                APS_LOG("---SENSOR_GET_DATA---\n\n");

                sensor_data = (hwm_sensor_data *)buff_out;
                sensor_data->values[0] =gRawData.ps_state;;
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



int epl6881_als_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value;
	hwm_sensor_data* sensor_data;
	struct epl6881_priv *obj = (struct epl6881_priv *)self;

	APS_FUN();
	APS_LOG("epl6881_als_operate command = %x\n",command);

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
				
				mutex_lock(&sensor_lock);
				if(value)
				{
					if((err = elan_epl6881_lsensor_enable(epl6881_obj, 1))!=0)
		            {
		                APS_ERR("enable als fail: %d\n", err);
						mutex_unlock(&sensor_lock);
		                return -1;
		            }
					set_bit(CMC_BIT_ALS, &obj->enable);
				}
				else
				{
					if((err = elan_epl6881_lsensor_enable(epl6881_obj, 0))!=0)
		            {
		                APS_ERR("disable als fail: %d\n", err);
						mutex_unlock(&sensor_lock);						
		                return -1;
		            }	
					
					clear_bit(CMC_BIT_ALS, &obj->enable);
				}
				
				mutex_unlock(&sensor_lock);
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
            
				mutex_lock(&sensor_lock);

                if((err = elan_epl6881_lsensor_enable(epl6881_obj, 1))!=0) //why?
                {
                    APS_ERR("disable als fail: %d\n", err);
					mutex_unlock(&sensor_lock);
                    return -1;
                }
                epl6881_read_als(epl6881_obj->client, &epl6881_obj->als);
                if(epl6881_obj->enable_pflag && isInterrupt)
                {
                    if((err = elan_epl6881_psensor_enable(epl6881_obj, 1))!=0)
                    {
                        APS_ERR("disable ps fail: %d\n", err);
						mutex_unlock(&sensor_lock);
                        return -1;
                    }
                }
				mutex_unlock(&sensor_lock);
				
				
                sensor_data = (hwm_sensor_data *)buff_out;
				//sensor_data->values[0] = epl6881_obj->als;
                sensor_data->values[0] = epl6881_get_als_value(epl6881_obj, epl6881_obj->als);
                sensor_data->value_divide = 1;
                sensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
                APS_LOG("get als data->values[0] = %d\n", sensor_data->values[0]);
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

static int epl6881_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info)
{    
	strcpy(info->type, EPL6881_DEV_NAME);
	return 0;
}


/*----------------------------------------------------------------------------*/
static int epl6881_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct epl6881_priv *obj;
	struct hwmsen_object obj_ps, obj_als;
	int err = 0;
	APS_FUN();

	epl6881_dumpReg(client);

	if(!(obj = kzalloc(sizeof(*obj), GFP_KERNEL)))
	{
		err = -ENOMEM;
		goto exit;
	}

	memset(obj, 0, sizeof(*obj));

	epl6881_obj = obj;
	obj->hw = get_cust_alsps_hw();

	epl6881_get_addr(obj->hw, &obj->addr);

    epl6881_obj->als_level_num = sizeof(epl6881_obj->hw->als_level)/sizeof(epl6881_obj->hw->als_level[0]);
    epl6881_obj->als_value_num = sizeof(epl6881_obj->hw->als_value)/sizeof(epl6881_obj->hw->als_value[0]);
    BUG_ON(sizeof(epl6881_obj->als_level) != sizeof(epl6881_obj->hw->als_level));
    memcpy(epl6881_obj->als_level, epl6881_obj->hw->als_level, sizeof(epl6881_obj->als_level));
    BUG_ON(sizeof(epl6881_obj->als_value) != sizeof(epl6881_obj->hw->als_value));
    memcpy(epl6881_obj->als_value, epl6881_obj->hw->als_value, sizeof(epl6881_obj->als_value));

	INIT_DELAYED_WORK(&obj->eint_work, epl6881_eint_work);

	obj->client = client;

	i2c_set_clientdata(client, obj);	

	atomic_set(&obj->als_debounce, 2000);
	atomic_set(&obj->als_deb_on, 0);
	atomic_set(&obj->als_deb_end, 0);
	atomic_set(&obj->ps_debounce, 1000);
	atomic_set(&obj->ps_deb_on, 0);
	atomic_set(&obj->ps_deb_end, 0);
	atomic_set(&obj->ps_mask, 0);
	atomic_set(&obj->trace, 0x00);
	atomic_set(&obj->als_suspend, 0);

//zijian for test
//	atomic_set(&obj->trace, 0xffff);

	obj->ps_enable = 0;
	obj->als_enable = 0;
	obj->enable = 0;
	obj->pending_intr = 0;
	obj->als_thd_level = 2;

	atomic_set(&obj->i2c_retry, 3);

	epl6881_i2c_client = client;

    elan_epl6881_I2C_Write(client,REG_0,W_SINGLE_BYTE,0x02, EPL_S_SENSING_MODE);
	elan_epl6881_I2C_Write(client,REG_9,W_SINGLE_BYTE,0x02,EPL_INT_DISABLE);

	if(err = epl6881_init_client(client))
	{
		goto exit_init_failed;
	}


	if(err = misc_register(&epl6881_device))
	{
		APS_ERR("epl6881_device register failed\n");
		goto exit_misc_device_register_failed;
	}

	if(err = epl6881_create_attr(&epl6881_alsps_driver.driver))
	{
		APS_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}

	obj_ps.self = epl6881_obj;

	if( obj->hw->polling_mode_ps == 1)
	{
	  obj_ps.polling = 1;
	  APS_LOG("isInterrupt == false\n");
	}
	else
	{
	 obj_ps.polling = 0;//interrupt mode
	
	//  obj_ps.polling = 1;//interrupt mode
	  isInterrupt=true;
	  APS_LOG("isInterrupt == true\n");
	}



	obj_ps.sensor_operate = epl6881_ps_operate;



	if(err = hwmsen_attach(ID_PROXIMITY, &obj_ps))
	{
		APS_ERR("attach fail = %d\n", err);
		goto exit_create_attr_failed;
	}


	obj_als.self = epl6881_obj;
	obj_als.polling = 1;
	obj_als.sensor_operate = epl6881_als_operate;
	APS_LOG("als polling mode\n");


	if(err = hwmsen_attach(ID_LIGHT, &obj_als))
	{
		APS_ERR("attach fail = %d\n", err);
		goto exit_create_attr_failed;
	}



#if defined(CONFIG_HAS_EARLYSUSPEND)
	obj->early_drv.level    = EARLY_SUSPEND_LEVEL_DISABLE_FB - 1,
	obj->early_drv.suspend  = epl6881_early_suspend,
	obj->early_drv.resume   = epl6881_late_resume,    
	register_early_suspend(&obj->early_drv);
#endif

     if(isInterrupt)
            epl6881_setup_eint(client);

	APS_LOG("%s: OK\n", __func__);
	return 0;

	exit_create_attr_failed:
	misc_deregister(&epl6881_device);
	exit_misc_device_register_failed:
	exit_init_failed:
	//i2c_detach_client(client);
//	exit_kfree:
	kfree(obj);
	exit:
	epl6881_i2c_client = NULL;           


	APS_ERR("%s: err = %d\n", __func__, err);
	return err;



}



/*----------------------------------------------------------------------------*/
static int epl6881_i2c_remove(struct i2c_client *client)
{
	int err;	
//	int data;

	if(err = epl6881_delete_attr(&epl6881_i2c_driver.driver))
	{
		APS_ERR("epl6881_delete_attr fail: %d\n", err);
	} 

	if(err = misc_deregister(&epl6881_device))
	{
		APS_ERR("misc_deregister fail: %d\n", err);    
	}

	epl6881_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));

	return 0;
}



/*----------------------------------------------------------------------------*/



static int epl6881_probe(struct platform_device *pdev) 
{
	struct alsps_hw *hw = get_cust_alsps_hw();

	epl6881_power(hw, 1);    

	/* Bob.chen add for if ps run, the system forbid to goto sleep mode. */
	//wake_lock_init(&ps_lock, WAKE_LOCK_SUSPEND, "ps wakelock");
	//wake_lock_init(&als_lock, WAKE_LOCK_SUSPEND, "als wakelock");
	//wake_lock_init(&als_lock, WAKE_LOCK_SUSPEND, "als wakelock");
	/* Bob.chen add end. */

	//epl6881_force[0] = hw->i2c_num;

	if(i2c_add_driver(&epl6881_i2c_driver))
	{
		APS_ERR("add driver error\n");
		return -1;
	} 
	return 0;
}



/*----------------------------------------------------------------------------*/
static int epl6881_remove(struct platform_device *pdev)
{
	struct alsps_hw *hw = get_cust_alsps_hw();
	APS_FUN();    
	epl6881_power(hw, 0);    

	APS_ERR("EPL6881 remove \n");
	i2c_del_driver(&epl6881_i2c_driver);
	return 0;
}



/*----------------------------------------------------------------------------*/
static struct platform_driver epl6881_alsps_driver = {
	.probe      = epl6881_probe,
	.remove     = epl6881_remove,    
	.driver     = {
		.name  = "als_ps",
		//.owner = THIS_MODULE,
	}
};
/*----------------------------------------------------------------------------*/
static int __init epl6881_init(void)
{
	APS_FUN();
	
	struct alsps_hw *hw = get_cust_alsps_hw();
	
	i2c_register_board_info(hw->i2c_num , &i2c_EPL6881, 1);
	if(platform_driver_register(&epl6881_alsps_driver))
	{
		APS_ERR("failed to register driver");
		return -ENODEV;
	}

	return 0;
}
/*----------------------------------------------------------------------------*/
static void __exit epl6881_exit(void)
{
	APS_FUN();
	platform_driver_unregister(&epl6881_alsps_driver);
}
/*----------------------------------------------------------------------------*/
module_init(epl6881_init);
module_exit(epl6881_exit);
/*----------------------------------------------------------------------------*/
MODULE_AUTHOR("Hivemotion haifeng@hivemotion.com");
MODULE_DESCRIPTION("EPL6881 ALPsr driver");
MODULE_LICENSE("GPL");





