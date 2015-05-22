/* drivers/i2c/chips/LSM6DS0_ACC.c - LSM6DS0 motion sensor driver
 *
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


#include <cust_acc.h>
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include "lsm6ds0_acc.h"
#include <linux/hwmsen_helper.h>

//#include <mach/mt_devs.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>


#define POWER_NONE_MACRO MT65XX_POWER_NONE



/*----------------------------------------------------------------------------*/
//#define I2C_DRIVERID_LSM6DS0 345
/*----------------------------------------------------------------------------*/
#define DEBUG 1
/*----------------------------------------------------------------------------*/
#define CONFIG_LSM6DS0_ACC_LOWPASS   /*apply low pass filter on output*/       
/*----------------------------------------------------------------------------*/
#define LSM6DS0_ACC_AXIS_X          0
#define LSM6DS0_ACC_AXIS_Y          1
#define LSM6DS0_ACC_AXIS_Z          2
#define LSM6DS0_ACC_AXES_NUM        3
#define LSM6DS0_ACC_DATA_LEN        6
#define LSM6DS0_ACC_DEV_NAME        "lsm6ds0_acc"
/*----------------------------------------------------------------------------*/
static const struct i2c_device_id LSM6DS0_ACC_i2c_id[] = {{LSM6DS0_ACC_DEV_NAME,0},{}};
/*the adapter id will be available in customization*/
static struct i2c_board_info __initdata i2c_LSM6DS0_ACC={ I2C_BOARD_INFO("lsm6ds0_acc", (LSM6DS0_ACC_I2C_SLAVE_ADDR>>1))};

//static unsigned short LSM6DS0_ACC_force[] = {0x00, LSM6DS0_ACC_I2C_SLAVE_ADDR, I2C_CLIENT_END, I2C_CLIENT_END};
//static const unsigned short *const LSM6DS0_ACC_forces[] = { LSM6DS0_ACC_force, NULL };
//static struct i2c_client_address_data LSM6DS0_ACC_addr_data = { .forces = LSM6DS0_ACC_forces,};

/*----------------------------------------------------------------------------*/
static int LSM6DS0_ACC_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id); 
static int LSM6DS0_ACC_i2c_remove(struct i2c_client *client);
static int LSM6DS0_ACC_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);
static int lis_i2c_read_block(struct i2c_client *client, u8 addr, u8 *data, u8 len);

/*----------------------------------------------------------------------------*/
typedef enum {
    ADX_TRC_FILTER  = 0x01,
    ADX_TRC_RAWDATA = 0x02,
    ADX_TRC_IOCTL   = 0x04,
    ADX_TRC_CALI	= 0X08,
    ADX_TRC_INFO	= 0X10,
} ADX_TRC;
/*----------------------------------------------------------------------------*/
struct scale_factor{
    u8  whole;
    u8  fraction;
};
/*----------------------------------------------------------------------------*/
struct data_resolution {
    struct scale_factor scalefactor;
    int                 sensitivity;
};
/*----------------------------------------------------------------------------*/
#define C_MAX_FIR_LENGTH (32)
/*----------------------------------------------------------------------------*/
struct data_filter {
    s16 raw[C_MAX_FIR_LENGTH][LSM6DS0_ACC_AXES_NUM];
    int sum[LSM6DS0_ACC_AXES_NUM];
    int num;
    int idx;
};
/*----------------------------------------------------------------------------*/
struct LSM6DS0_ACC_i2c_data {
    struct i2c_client *client;
    struct acc_hw *hw;
    struct hwmsen_convert   cvt;
    
    /*misc*/
    struct data_resolution *reso;
    atomic_t                trace;
    atomic_t                suspend;
    atomic_t                selftest;
	atomic_t				filter;
    s16                     cali_sw[LSM6DS0_ACC_AXES_NUM+1];

    /*data*/
    s8                      offset[LSM6DS0_ACC_AXES_NUM+1];  /*+1: for 4-byte alignment*/
    s16                     data[LSM6DS0_ACC_AXES_NUM+1];

#if defined(CONFIG_LSM6DS0_ACC_LOWPASS)
    atomic_t                firlen;
    atomic_t                fir_en;
    struct data_filter      fir;
#endif 
    /*early suspend*/
#if defined(CONFIG_HAS_EARLYSUSPEND)
    struct early_suspend    early_drv;
#endif     
};
/*----------------------------------------------------------------------------*/
static struct i2c_driver LSM6DS0_ACC_i2c_driver = {
    .driver = {
//        .owner          = THIS_MODULE,
        .name           = LSM6DS0_ACC_DEV_NAME,
    },
	.probe      		= LSM6DS0_ACC_i2c_probe,
	.remove    			= LSM6DS0_ACC_i2c_remove,
	.detect				= LSM6DS0_ACC_i2c_detect,
#if !defined(CONFIG_HAS_EARLYSUSPEND)    
    .suspend            = LSM6DS0_ACC_suspend,
    .resume             = LSM6DS0_ACC_resume,
#endif
	.id_table = LSM6DS0_ACC_i2c_id,
//	.address_data = &LSM6DS0_ACC_addr_data,
};

/*----------------------------------------------------------------------------*/
static struct i2c_client *LSM6DS0_ACC_i2c_client = NULL;
static struct platform_driver LSM6DS0_ACC_gsensor_driver;
static struct LSM6DS0_ACC_i2c_data *obj_i2c_data = NULL;
static bool sensor_power = true;
static GSENSOR_VECTOR3D gsensor_gain, gsensor_offset;
//static char selftestRes[10] = {0};



/*----------------------------------------------------------------------------*/
#define GSE_TAG                  "[Gsensor] "
#define GSE_FUN(f)               printk(KERN_INFO GSE_TAG"%s\n", __FUNCTION__)
#define GSE_ERR(fmt, args...)    printk(KERN_ERR GSE_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define GSE_LOG(fmt, args...)    printk(KERN_INFO GSE_TAG fmt, ##args)
/*----------------------------------------------------------------------------*/
static struct data_resolution LSM6DS0_ACC_data_resolution[] = {
     /* combination by {FULL_RES,RANGE}*/
    {{ 1, 0}, 16384},   // dataformat +/-2g  in 16-bit resolution;  { 1, 0} = 1.0 = (2*2*1000)/(2^12);  1024 = (2^12)/(2*2) 
    {{ 1, 9}, 8192},   // dataformat +/-4g  in 16-bit resolution;  { 1, 9} = 1.9 = (2*4*1000)/(2^12);  512 = (2^12)/(2*4) 
	 {{ 3, 9}, 4096},   // dataformat +/-8g  in 16-bit resolution;  { 1, 0} = 1.0 = (2*8*1000)/(2^12);  1024 = (2^12)/(2*8) 
};
/*----------------------------------------------------------------------------*/
static struct data_resolution LSM6DS0_ACC_offset_resolution = {{15, 6}, 64};


static int lsm6ds0_acc_read_byte_sr(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{
    int ret = 0;
	unsigned short length = 0;
	
    client->addr = (client->addr & I2C_MASK_FLAG) | I2C_WR_FLAG |I2C_RS_FLAG;
    data[0] = addr;
	length = (len << 8) | 1;
	
	ret = i2c_master_send(client, (const char*)data, length);
    if (ret < 0) {
        GSE_ERR("lsm6ds0_acc_read_byte_sr error!!\n");
        return -EFAULT;
    }
	
	client->addr &= I2C_MASK_FLAG;
	
    return 0;
}


/*--------------------read function----------------------------------*/
static int lis_i2c_read_block(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{
    u8 beg = addr;	
	struct i2c_msg msgs[2] = {
		{
			.addr = client->addr,	.flags = 0,
			.len = 1,	.buf = &beg
		},
		{
			.addr = client->addr,	.flags = I2C_M_RD,
			.len = len,	.buf = data,
		}
	};
	int err;

	if (!client)
		return -EINVAL;
	else if (len > C_I2C_FIFO_SIZE) {
		GSE_ERR(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		return -EINVAL;
	}

	err = i2c_transfer(client->adapter, msgs, sizeof(msgs)/sizeof(msgs[0]));
	if (err != 2) {
		GSE_ERR("i2c_transfer error: (%d %p %d) %d\n",
			addr, data, len, err);
		err = -EIO;
	} else {
		err = 0;
	}
	return err;

}
/*--------------------read function----------------------------------*/
static void dumpReg(struct i2c_client *client)
{
  int i=0;
  u8 addr = 0x20;
  u8 regdata=0;
  for(i=0; i<3 ; i++)
  {
    //dump all
    lsm6ds0_acc_read_byte_sr(client,addr,&regdata,1);
	GSE_LOG("Reg addr=%x regdata=%x\n",addr,regdata);
	addr++;
  }
}

/*--------------------ADXL power control function----------------------------------*/
static void LSM6DS0_ACC_power(struct acc_hw *hw, unsigned int on) 
{
	static unsigned int power_on = 0;

	if(hw->power_id != POWER_NONE_MACRO)		// have externel LDO
	{        
		GSE_LOG("power %s\n", on ? "on" : "off");
		if(power_on == on)	// power status not change
		{
			GSE_LOG("ignore power control: %d\n", on);
		}
		else if(on)	// power on
		{
			if(!hwPowerOn(hw->power_id, hw->power_vol, "LSM6DS0_ACC"))
			{
				GSE_ERR("power on fails!!\n");
			}
		}
		else	// power off
		{
			if (!hwPowerDown(hw->power_id, "LSM6DS0_ACC"))
			{
				GSE_ERR("power off fail!!\n");
			}			  
		}
	}
	power_on = on;    
}
/*----------------------------------------------------------------------------*/
static int LSM6DS0_ACC_SetDataResolution(struct LSM6DS0_ACC_i2c_data *obj)
{
	int err;
	u8  dat, reso;

	if((err = lsm6ds0_acc_read_byte_sr(obj->client, LSM6DS0_ACC_REG_CTL_REG6, &dat,0x01)))
	{
		GSE_ERR("write data format fail!!\n");
		return err;
	}
	GSE_LOG("SetDataResolution from register is %x",dat);

	/*the data_reso is combined by 3 bits: {FULL_RES, DATA_RANGE}*/
	reso  = (dat & 0x18) >> 3;

	if(reso == 2)
	{
		reso = 1;
	}

	if(reso == 3)
	{
		reso = 2;
	}
	
	GSE_LOG("after handle is %x",reso);

	if(reso < sizeof(LSM6DS0_ACC_data_resolution)/sizeof(LSM6DS0_ACC_data_resolution[0]))
	{        
		obj->reso = &LSM6DS0_ACC_data_resolution[reso];
		return 0;
	}
	else
	{
		return -EINVAL;
	}
}
/*----------------------------------------------------------------------------*/
static int LSM6DS0_ACC_ReadData(struct i2c_client *client, s16 data[LSM6DS0_ACC_AXES_NUM])
{
	struct LSM6DS0_ACC_i2c_data *priv = i2c_get_clientdata(client);        
	u8 addr = LSM6DS0_ACC_REG_OUT_X;
	u8 buf[LSM6DS0_ACC_DATA_LEN] = {0};
	int err = 0;
	int i = 0;

	if(NULL == client)
	{
		err = -EINVAL;
	}
	else if((err = lsm6ds0_acc_read_byte_sr(client, addr, buf, 0x06)))
	{
		GSE_ERR("LSM6DS0 read gsensor data error: %d\n", err);
		return LSM6DS0_ACC_ERR_GETGSENSORDATA;
	}
	else
	{
		data[LSM6DS0_ACC_AXIS_X] = (s16)((buf[LSM6DS0_ACC_AXIS_X*2]) |
		         (buf[LSM6DS0_ACC_AXIS_X*2+1] << 8));
		data[LSM6DS0_ACC_AXIS_Y] = (s16)((buf[LSM6DS0_ACC_AXIS_Y*2]) |
		         (buf[LSM6DS0_ACC_AXIS_Y*2+1] << 8));
		data[LSM6DS0_ACC_AXIS_Z] = (s16)((buf[LSM6DS0_ACC_AXIS_Z*2]) |
		         (buf[LSM6DS0_ACC_AXIS_Z*2+1] << 8));


		if(atomic_read(&priv->trace) & ADX_TRC_RAWDATA)
		{
			GSE_LOG("[%08X %08X %08X] => [%5d %5d %5d]\n", data[LSM6DS0_ACC_AXIS_X], data[LSM6DS0_ACC_AXIS_Y], data[LSM6DS0_ACC_AXIS_Z],
		                               data[LSM6DS0_ACC_AXIS_X], data[LSM6DS0_ACC_AXIS_Y], data[LSM6DS0_ACC_AXIS_Z]);
		}

		for(i=0;i<3;i++)				
		{								//because the data is store in binary complement number formation in computer system
			if ( data[i] == 0x8000 )	//so we want to calculate actual number here
				data[i]= -0x8000;		//10bit resolution, 512= 2^(12-1)
			else if ( data[i] & 0x8000 )//transfor format
			{							//printk("data 0 step %x \n",data[i]);
				data[i] -= 0x1; 		//printk("data 1 step %x \n",data[i]);
				data[i] = ~data[i]; 	//printk("data 2 step %x \n",data[i]);
				data[i] &= 0x07fff;		//printk("data 3 step %x \n\n",data[i]);
				data[i] = -data[i]; 	
			}
		}	


		if(atomic_read(&priv->trace) & ADX_TRC_RAWDATA)
		{
			GSE_LOG("[%08X %08X %08X] => [%5d %5d %5d] after\n", data[LSM6DS0_ACC_AXIS_X], data[LSM6DS0_ACC_AXIS_Y], data[LSM6DS0_ACC_AXIS_Z],
		                               data[LSM6DS0_ACC_AXIS_X], data[LSM6DS0_ACC_AXIS_Y], data[LSM6DS0_ACC_AXIS_Z]);
		}
				
#ifdef CONFIG_LSM6DS0_ACC_LOWPASS
		if(atomic_read(&priv->filter))
		{
			if(atomic_read(&priv->fir_en) && !atomic_read(&priv->suspend))
			{
				int idx, firlen = atomic_read(&priv->firlen);   
				if(priv->fir.num < firlen)
				{                
					priv->fir.raw[priv->fir.num][LSM6DS0_ACC_AXIS_X] = data[LSM6DS0_ACC_AXIS_X];
					priv->fir.raw[priv->fir.num][LSM6DS0_ACC_AXIS_Y] = data[LSM6DS0_ACC_AXIS_Y];
					priv->fir.raw[priv->fir.num][LSM6DS0_ACC_AXIS_Z] = data[LSM6DS0_ACC_AXIS_Z];
					priv->fir.sum[LSM6DS0_ACC_AXIS_X] += data[LSM6DS0_ACC_AXIS_X];
					priv->fir.sum[LSM6DS0_ACC_AXIS_Y] += data[LSM6DS0_ACC_AXIS_Y];
					priv->fir.sum[LSM6DS0_ACC_AXIS_Z] += data[LSM6DS0_ACC_AXIS_Z];
					if(atomic_read(&priv->trace) & ADX_TRC_FILTER)
					{
						GSE_LOG("add [%2d] [%5d %5d %5d] => [%5d %5d %5d]\n", priv->fir.num,
							priv->fir.raw[priv->fir.num][LSM6DS0_ACC_AXIS_X], priv->fir.raw[priv->fir.num][LSM6DS0_ACC_AXIS_Y], priv->fir.raw[priv->fir.num][LSM6DS0_ACC_AXIS_Z],
							priv->fir.sum[LSM6DS0_ACC_AXIS_X], priv->fir.sum[LSM6DS0_ACC_AXIS_Y], priv->fir.sum[LSM6DS0_ACC_AXIS_Z]);
					}
					priv->fir.num++;
					priv->fir.idx++;
				}
				else
				{
					idx = priv->fir.idx % firlen;
					priv->fir.sum[LSM6DS0_ACC_AXIS_X] -= priv->fir.raw[idx][LSM6DS0_ACC_AXIS_X];
					priv->fir.sum[LSM6DS0_ACC_AXIS_Y] -= priv->fir.raw[idx][LSM6DS0_ACC_AXIS_Y];
					priv->fir.sum[LSM6DS0_ACC_AXIS_Z] -= priv->fir.raw[idx][LSM6DS0_ACC_AXIS_Z];
					priv->fir.raw[idx][LSM6DS0_ACC_AXIS_X] = data[LSM6DS0_ACC_AXIS_X];
					priv->fir.raw[idx][LSM6DS0_ACC_AXIS_Y] = data[LSM6DS0_ACC_AXIS_Y];
					priv->fir.raw[idx][LSM6DS0_ACC_AXIS_Z] = data[LSM6DS0_ACC_AXIS_Z];
					priv->fir.sum[LSM6DS0_ACC_AXIS_X] += data[LSM6DS0_ACC_AXIS_X];
					priv->fir.sum[LSM6DS0_ACC_AXIS_Y] += data[LSM6DS0_ACC_AXIS_Y];
					priv->fir.sum[LSM6DS0_ACC_AXIS_Z] += data[LSM6DS0_ACC_AXIS_Z];
					priv->fir.idx++;
					data[LSM6DS0_ACC_AXIS_X] = priv->fir.sum[LSM6DS0_ACC_AXIS_X]/firlen;
					data[LSM6DS0_ACC_AXIS_Y] = priv->fir.sum[LSM6DS0_ACC_AXIS_Y]/firlen;
					data[LSM6DS0_ACC_AXIS_Z] = priv->fir.sum[LSM6DS0_ACC_AXIS_Z]/firlen;
					if(atomic_read(&priv->trace) & ADX_TRC_FILTER)
					{
						GSE_LOG("add [%2d] [%5d %5d %5d] => [%5d %5d %5d] : [%5d %5d %5d]\n", idx,
						priv->fir.raw[idx][LSM6DS0_ACC_AXIS_X], priv->fir.raw[idx][LSM6DS0_ACC_AXIS_Y], priv->fir.raw[idx][LSM6DS0_ACC_AXIS_Z],
						priv->fir.sum[LSM6DS0_ACC_AXIS_X], priv->fir.sum[LSM6DS0_ACC_AXIS_Y], priv->fir.sum[LSM6DS0_ACC_AXIS_Z],
						data[LSM6DS0_ACC_AXIS_X], data[LSM6DS0_ACC_AXIS_Y], data[LSM6DS0_ACC_AXIS_Z]);
					}
				}
			}
		}	
#endif         
	}
	return err;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static int LSM6DS0_ACC_ResetCalibration(struct i2c_client *client)
{
	struct LSM6DS0_ACC_i2c_data *obj = i2c_get_clientdata(client);	

	memset(obj->cali_sw, 0x00, sizeof(obj->cali_sw));
	return 0;     
}
/*----------------------------------------------------------------------------*/
static int LSM6DS0_ACC_ReadCalibration(struct i2c_client *client, int dat[LSM6DS0_ACC_AXES_NUM])
{
    struct LSM6DS0_ACC_i2c_data *obj = i2c_get_clientdata(client);

    dat[obj->cvt.map[LSM6DS0_ACC_AXIS_X]] = obj->cvt.sign[LSM6DS0_ACC_AXIS_X]*obj->cali_sw[LSM6DS0_ACC_AXIS_X];
    dat[obj->cvt.map[LSM6DS0_ACC_AXIS_Y]] = obj->cvt.sign[LSM6DS0_ACC_AXIS_Y]*obj->cali_sw[LSM6DS0_ACC_AXIS_Y];
    dat[obj->cvt.map[LSM6DS0_ACC_AXIS_Z]] = obj->cvt.sign[LSM6DS0_ACC_AXIS_Z]*obj->cali_sw[LSM6DS0_ACC_AXIS_Z];                        
                                       
    return 0;
}

/*----------------------------------------------------------------------------*/
static int LSM6DS0_ACC_WriteCalibration(struct i2c_client *client, int dat[LSM6DS0_ACC_AXES_NUM])
{
	struct LSM6DS0_ACC_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;
	//int cali[LSM6DS0_ACC_AXES_NUM];


	GSE_FUN();
	if(!obj || ! dat)
	{
		GSE_ERR("null ptr!!\n");
		return -EINVAL;
	}
	else
	{        
		s16 cali[LSM6DS0_ACC_AXES_NUM];
		cali[obj->cvt.map[LSM6DS0_ACC_AXIS_X]] = obj->cvt.sign[LSM6DS0_ACC_AXIS_X]*obj->cali_sw[LSM6DS0_ACC_AXIS_X];
		cali[obj->cvt.map[LSM6DS0_ACC_AXIS_Y]] = obj->cvt.sign[LSM6DS0_ACC_AXIS_Y]*obj->cali_sw[LSM6DS0_ACC_AXIS_Y];
		cali[obj->cvt.map[LSM6DS0_ACC_AXIS_Z]] = obj->cvt.sign[LSM6DS0_ACC_AXIS_Z]*obj->cali_sw[LSM6DS0_ACC_AXIS_Z]; 
		cali[LSM6DS0_ACC_AXIS_X] += dat[LSM6DS0_ACC_AXIS_X];
		cali[LSM6DS0_ACC_AXIS_Y] += dat[LSM6DS0_ACC_AXIS_Y];
		cali[LSM6DS0_ACC_AXIS_Z] += dat[LSM6DS0_ACC_AXIS_Z];

		obj->cali_sw[LSM6DS0_ACC_AXIS_X] += obj->cvt.sign[LSM6DS0_ACC_AXIS_X]*dat[obj->cvt.map[LSM6DS0_ACC_AXIS_X]];
        obj->cali_sw[LSM6DS0_ACC_AXIS_Y] += obj->cvt.sign[LSM6DS0_ACC_AXIS_Y]*dat[obj->cvt.map[LSM6DS0_ACC_AXIS_Y]];
        obj->cali_sw[LSM6DS0_ACC_AXIS_Z] += obj->cvt.sign[LSM6DS0_ACC_AXIS_Z]*dat[obj->cvt.map[LSM6DS0_ACC_AXIS_Z]];
	} 

	return err;
}
/*----------------------------------------------------------------------------*/
static int LSM6DS0_ACC_CheckDeviceID(struct i2c_client *client)
{
	u8 databuf[2];    
	int res = 0;
	u8 addr = LSM6DS0_ACC_REG_DEVID;
	memset(databuf, 0, sizeof(u8)*2);    
	databuf[0] = LSM6DS0_ACC_REG_DEVID;  
	
	if(lsm6ds0_acc_read_byte_sr(client, addr, databuf, 0x01))
	{
		GSE_ERR("read DeviceID register err!\n");
		return LSM6DS0_ACC_ERR_IDENTIFICATION;
		
	}
	GSE_LOG("LSM6DS0_ACC Device ID=0x%x,respect=0x%x\n",databuf[0],WHO_AM_I);

	return LSM6DS0_ACC_SUCCESS;
}
/*----------------------------------------------------------------------------*/
static int LSM6DS0_ACC_SetBWRate(struct i2c_client *client, u8 bwrate)
{
	u8 databuf[10];
	u8 addr = LSM6DS0_ACC_REG_CTL_REG6;
	int res = 0;

	memset(databuf, 0, sizeof(u8)*10);
	
	if(lsm6ds0_acc_read_byte_sr(client, addr, databuf, 0x01))
	{
		GSE_ERR("read reg_ctl_reg5 register err!\n");
		return LSM6DS0_ACC_ERR_I2C;
	}
	GSE_LOG("LSM6DS0_ACC_SetBWRate read from REG5 is 0x%x\n",databuf[0]);

	databuf[0] &= ~0xE0;
	databuf[0] |= bwrate;

	databuf[1] = databuf[0];
	databuf[0] = LSM6DS0_ACC_REG_CTL_REG6;

	res = i2c_master_send(client, databuf, 0x2);
	GSE_LOG("LSM6DS0_ACC_SetBWRate:write 0x%x to REG5",databuf[1]);

	if(res <= 0)
	{
		GSE_ERR("setBWRate failed!\n");
		return LSM6DS0_ACC_ERR_I2C;
	}
	
	return LSM6DS0_ACC_SUCCESS;    
}

/*----------------------------------------------------------------------------*/
static int LSM6DS0_ACC_SetPowerMode(struct i2c_client *client, bool enable)
{
	u8 databuf[2];    
	int res = 0;
	u8 addr = LSM6DS0_ACC_REG_CTL_REG6;
	struct LSM6DS0_ACC_i2c_data *obj = i2c_get_clientdata(client);
	
	//GSE_LOG("enter Sensor power status is sensor_power = %d\n",sensor_power);

	if(enable == sensor_power)
	{
		GSE_LOG("Sensor power status is newest!\n");
		return LSM6DS0_ACC_SUCCESS;
	}


	if(enable == TRUE)
	{
		res = LSM6DS0_ACC_SetBWRate(client, LSM6DS0_ACC_BW_100HZ);//400 or 100 no other choice
	}
	else
	{
		if(lsm6ds0_acc_read_byte_sr(client, addr, databuf, 0x01))
		{
			GSE_ERR("read power ctl register err!\n");
			return LSM6DS0_ACC_ERR_I2C;
		}
		GSE_LOG("LSM6DS0_ACC_SetPowerMode read from REG5 is 0x%x\n",databuf[0]);

		databuf[0] &= ~0xE0;
		databuf[0] |= 0x00;

		databuf[1] = databuf[0];
		databuf[0] = LSM6DS0_ACC_REG_CTL_REG6;
		res = i2c_master_send(client, databuf, 0x2);
		GSE_LOG("SetPowerMode:write 0x%x to REG5 \n",databuf[1]);
		if(res <= 0)
		{
			GSE_ERR("set power mode failed!\n");
			return LSM6DS0_ACC_ERR_I2C;
		}

	}

	if(atomic_read(&obj->trace) & ADX_TRC_INFO)
	{
		GSE_LOG("set power mode ok %d!\n", databuf[1]);
	}
	sensor_power = enable;

	GSE_LOG("leave SetPowerMode(),sensor_power = %d\n",sensor_power);
	return LSM6DS0_ACC_SUCCESS;    
}
/*----------------------------------------------------------------------------*/
static int LSM6DS0_ACC_SetDataFormat(struct i2c_client *client, u8 dataformat)
{
	struct LSM6DS0_ACC_i2c_data *obj = i2c_get_clientdata(client);
	u8 databuf[10];
	u8 addr = LSM6DS0_ACC_REG_CTL_REG6;
	int res = 0;

	memset(databuf, 0, sizeof(u8)*10);

	if(lsm6ds0_acc_read_byte_sr(client, addr, databuf, 0x01))
	{
		GSE_ERR("read reg_ctl_reg1 register err!\n");
		return LSM6DS0_ACC_ERR_I2C;
	}

	databuf[0] &= ~0x18;
	databuf[0] |= dataformat;

	databuf[1] = databuf[0];
	databuf[0] = LSM6DS0_ACC_REG_CTL_REG6;
	

	res = i2c_master_send(client, databuf, 0x2);

	if(res <= 0)
	{
		return LSM6DS0_ACC_ERR_I2C;
	}
	

	return LSM6DS0_ACC_SetDataResolution(obj);    
}
/*----------------------------------------------------------------------------*/

/*open drain mode set*/
static int LSM6DS0_ACC_PP_OD_init(client)
{
	u8 databuf[2] = {0};	
	int res = 0;

	databuf[1] = 0x14;
	databuf[0] = LSM6DS0_ACC_REG_CTL_REG8;	  
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		GSE_LOG("set PP_OD mode failed!\n");
		return LSM6DS0_ACC_ERR_I2C;
	}	

	return LSM6DS0_ACC_SUCCESS;    
}

/*----------------------------------------------------------------------------*/
static int LSM6DS0_ACC_Init(struct i2c_client *client, int reset_cali)
{
	struct LSM6DS0_ACC_i2c_data *obj = i2c_get_clientdata(client);
	int res = 0;
	int i = 0;
#if 0
	res = LSM6DS0_ACC_PP_OD_init(client);
	if(res != LSM6DS0_ACC_SUCCESS)
	{
		GSE_LOG("set PP_OD mode failed!\n");
		return res;
	}
#endif
	for(i = 0; i < 5; i++)
	{
		res = LSM6DS0_ACC_CheckDeviceID(client); 
		if(res = LSM6DS0_ACC_SUCCESS)
		{
			GSE_LOG("check success time %d !\n", i);
			break;
		}
	}
	
	res = LSM6DS0_ACC_SetBWRate(client, LSM6DS0_ACC_BW_100HZ);//400 or 100 no other choice
	if(res != LSM6DS0_ACC_SUCCESS )
	{
		return res;
	}

	res = LSM6DS0_ACC_SetDataFormat(client, LSM6DS0_ACC_RANGE_2G);//8g or 2G no oher choise
	if(res != LSM6DS0_ACC_SUCCESS) 
	{
		return res;
	}
	gsensor_gain.x = gsensor_gain.y = gsensor_gain.z = obj->reso->sensitivity;

	if(0 != reset_cali)
	{ 
		//reset calibration only in power on
		res = LSM6DS0_ACC_ResetCalibration(client);
		if(res != LSM6DS0_ACC_SUCCESS)
		{
			return res;
		}
	}

#ifdef CONFIG_LSM6DS0_ACC_LOWPASS
	memset(&obj->fir, 0x00, sizeof(obj->fir));  
#endif

	return LSM6DS0_ACC_SUCCESS;
}
/*----------------------------------------------------------------------------*/
static int LSM6DS0_ACC_ReadChipInfo(struct i2c_client *client, char *buf, int bufsize)
{
	u8 databuf[10];    

	memset(databuf, 0, sizeof(u8)*10);

	if((NULL == buf)||(bufsize<=30))
	{
		return -1;
	}
	
	if(NULL == client)
	{
		*buf = 0;
		return -2;
	}

	sprintf(buf, "LSM6DS0_ACC Chip");
	return 0;
}
/*----------------------------------------------------------------------------*/
static int LSM6DS0_ACC_ReadSensorData(struct i2c_client *client, char *buf, int bufsize)
{
	struct LSM6DS0_ACC_i2c_data *obj = (struct LSM6DS0_ACC_i2c_data*)i2c_get_clientdata(client);
	u8 databuf[20];
	int acc[LSM6DS0_ACC_AXES_NUM];
	int res = 0;
	memset(databuf, 0, sizeof(u8)*10);

	if(NULL == buf)
	{
		return -1;
	}
	if(NULL == client)
	{
		*buf = 0;
		return -2;
	}

	if(sensor_power == FALSE)
	{
		res = LSM6DS0_ACC_SetPowerMode(client, true);
		if(res)
		{
			GSE_ERR("Power on LSM6DS0_ACC error %d!\n", res);
		}
		msleep(20);
	}

	if((res = LSM6DS0_ACC_ReadData(client, obj->data)))
	{        
		GSE_ERR("I2C error: ret value=%d", res);
		return -3;
	}
	else
	{
		obj->data[LSM6DS0_ACC_AXIS_X] += obj->cali_sw[LSM6DS0_ACC_AXIS_X];
		obj->data[LSM6DS0_ACC_AXIS_Y] += obj->cali_sw[LSM6DS0_ACC_AXIS_Y];
		obj->data[LSM6DS0_ACC_AXIS_Z] += obj->cali_sw[LSM6DS0_ACC_AXIS_Z];
		
		/*remap coordinate*/
		acc[obj->cvt.map[LSM6DS0_ACC_AXIS_X]] = obj->cvt.sign[LSM6DS0_ACC_AXIS_X]*obj->data[LSM6DS0_ACC_AXIS_X];
		acc[obj->cvt.map[LSM6DS0_ACC_AXIS_Y]] = obj->cvt.sign[LSM6DS0_ACC_AXIS_Y]*obj->data[LSM6DS0_ACC_AXIS_Y];
		acc[obj->cvt.map[LSM6DS0_ACC_AXIS_Z]] = obj->cvt.sign[LSM6DS0_ACC_AXIS_Z]*obj->data[LSM6DS0_ACC_AXIS_Z];

		//GSE_LOG("Mapped gsensor data: %d, %d, %d!\n", acc[LSM6DS0_ACC_AXIS_X], acc[LSM6DS0_ACC_AXIS_Y], acc[LSM6DS0_ACC_AXIS_Z]);

		//Out put the mg
		acc[LSM6DS0_ACC_AXIS_X] = acc[LSM6DS0_ACC_AXIS_X] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
		acc[LSM6DS0_ACC_AXIS_Y] = acc[LSM6DS0_ACC_AXIS_Y] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
		acc[LSM6DS0_ACC_AXIS_Z] = acc[LSM6DS0_ACC_AXIS_Z] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;		
		

		sprintf(buf, "%04x %04x %04x", acc[LSM6DS0_ACC_AXIS_X], acc[LSM6DS0_ACC_AXIS_Y], acc[LSM6DS0_ACC_AXIS_Z]);
		if(atomic_read(&obj->trace) & ADX_TRC_IOCTL)//atomic_read(&obj->trace) & ADX_TRC_IOCTL
		{
			GSE_LOG("gsensor data: %s!\n", buf);
			dumpReg(client);
		}
	}
	
	return 0;
}
/*----------------------------------------------------------------------------*/
static int LSM6DS0_ACC_ReadRawData(struct i2c_client *client, char *buf)
{
	struct LSM6DS0_ACC_i2c_data *obj = (struct LSM6DS0_ACC_i2c_data*)i2c_get_clientdata(client);
	int res = 0;

	if (!buf || !client)
	{
		return EINVAL;
	}
	
	if((res = LSM6DS0_ACC_ReadData(client, obj->data)))
	{        
		GSE_ERR("I2C error: ret value=%d", res);
		return EIO;
	}
	else
	{
		sprintf(buf, "%04x %04x %04x", obj->data[LSM6DS0_ACC_AXIS_X], 
			obj->data[LSM6DS0_ACC_AXIS_Y], obj->data[LSM6DS0_ACC_AXIS_Z]);
	
	}
	
	return 0;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = LSM6DS0_ACC_i2c_client;
	char strbuf[LSM6DS0_ACC_BUFSIZE];
	if(NULL == client)
	{
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}
	
	LSM6DS0_ACC_ReadChipInfo(client, strbuf, LSM6DS0_ACC_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);        
}
/*----------------------------------------------------------------------------*/
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = LSM6DS0_ACC_i2c_client;
	char strbuf[LSM6DS0_ACC_BUFSIZE]={0};
	
	if(NULL == client)
	{
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}
	LSM6DS0_ACC_ReadSensorData(client, strbuf, LSM6DS0_ACC_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);            
}
/*----------------------------------------------------------------------------*/
static ssize_t show_cali_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = LSM6DS0_ACC_i2c_client;
	struct LSM6DS0_ACC_i2c_data *obj;
	int err, len, mul;
	int tmp[LSM6DS0_ACC_AXES_NUM];	
	len = 0;

	if(NULL == client)
	{
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	obj = i2c_get_clientdata(client);

	if((err = LSM6DS0_ACC_ReadCalibration(client, tmp)))
	{
		return -EINVAL;
	}
	else
	{    
		mul = obj->reso->sensitivity/LSM6DS0_ACC_offset_resolution.sensitivity;
		len += snprintf(buf+len, PAGE_SIZE-len, "[HW ][%d] (%+3d, %+3d, %+3d) : (0x%02X, 0x%02X, 0x%02X)\n", mul,                        
			obj->offset[LSM6DS0_ACC_AXIS_X], obj->offset[LSM6DS0_ACC_AXIS_Y], obj->offset[LSM6DS0_ACC_AXIS_Z],
			obj->offset[LSM6DS0_ACC_AXIS_X], obj->offset[LSM6DS0_ACC_AXIS_Y], obj->offset[LSM6DS0_ACC_AXIS_Z]);
		len += snprintf(buf+len, PAGE_SIZE-len, "[SW ][%d] (%+3d, %+3d, %+3d)\n", 1, 
			obj->cali_sw[LSM6DS0_ACC_AXIS_X], obj->cali_sw[LSM6DS0_ACC_AXIS_Y], obj->cali_sw[LSM6DS0_ACC_AXIS_Z]);

		len += snprintf(buf+len, PAGE_SIZE-len, "[ALL]    (%+3d, %+3d, %+3d) : (%+3d, %+3d, %+3d)\n", 
			obj->offset[LSM6DS0_ACC_AXIS_X]*mul + obj->cali_sw[LSM6DS0_ACC_AXIS_X],
			obj->offset[LSM6DS0_ACC_AXIS_Y]*mul + obj->cali_sw[LSM6DS0_ACC_AXIS_Y],
			obj->offset[LSM6DS0_ACC_AXIS_Z]*mul + obj->cali_sw[LSM6DS0_ACC_AXIS_Z],
			tmp[LSM6DS0_ACC_AXIS_X], tmp[LSM6DS0_ACC_AXIS_Y], tmp[LSM6DS0_ACC_AXIS_Z]);
		
		return len;
    }
}
/*----------------------------------------------------------------------------*/
static ssize_t store_cali_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct i2c_client *client = LSM6DS0_ACC_i2c_client;  
	int err, x, y, z;
	int dat[LSM6DS0_ACC_AXES_NUM];

	if(!strncmp(buf, "rst", 3))
	{
		if((err = LSM6DS0_ACC_ResetCalibration(client)))
		{
			GSE_ERR("reset offset err = %d\n", err);
		}	
	}
	else if(3 == sscanf(buf, "0x%02X 0x%02X 0x%02X", &x, &y, &z))
	{
		dat[LSM6DS0_ACC_AXIS_X] = x;
		dat[LSM6DS0_ACC_AXIS_Y] = y;
		dat[LSM6DS0_ACC_AXIS_Z] = z;
		if((err = LSM6DS0_ACC_WriteCalibration(client, dat)))
		{
			GSE_ERR("write calibration err = %d\n", err);
		}		
	}
	else
	{
		GSE_ERR("invalid format\n");
	}
	
	return count;
}
/*----------------------------------------------------------------------------*/

static ssize_t show_power_status(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = LSM6DS0_ACC_i2c_client;
	struct LSM6DS0_ACC_i2c_data *obj;
	u8 data;

	if(NULL == client)
	{
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	obj = i2c_get_clientdata(client);
	lsm6ds0_acc_read_byte_sr(client,LSM6DS0_ACC_REG_CTL_REG6,&data,0x01);
    return snprintf(buf, PAGE_SIZE, "%x\n", data);
}
/*----------------------------------------------------------------------------*/
static ssize_t show_firlen_value(struct device_driver *ddri, char *buf)
{
#ifdef CONFIG_LSM6DS0_ACC_LOWPASS
	struct i2c_client *client = LSM6DS0_ACC_i2c_client;
	struct LSM6DS0_ACC_i2c_data *obj = i2c_get_clientdata(client);
	if(atomic_read(&obj->firlen))
	{
		int idx, len = atomic_read(&obj->firlen);
		GSE_LOG("len = %2d, idx = %2d\n", obj->fir.num, obj->fir.idx);

		for(idx = 0; idx < len; idx++)
		{
			GSE_LOG("[%5d %5d %5d]\n", obj->fir.raw[idx][LSM6DS0_ACC_AXIS_X], obj->fir.raw[idx][LSM6DS0_ACC_AXIS_Y], obj->fir.raw[idx][LSM6DS0_ACC_AXIS_Z]);
		}
		
		GSE_LOG("sum = [%5d %5d %5d]\n", obj->fir.sum[LSM6DS0_ACC_AXIS_X], obj->fir.sum[LSM6DS0_ACC_AXIS_Y], obj->fir.sum[LSM6DS0_ACC_AXIS_Z]);
		GSE_LOG("avg = [%5d %5d %5d]\n", obj->fir.sum[LSM6DS0_ACC_AXIS_X]/len, obj->fir.sum[LSM6DS0_ACC_AXIS_Y]/len, obj->fir.sum[LSM6DS0_ACC_AXIS_Z]/len);
	}
	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&obj->firlen));
#else
	return snprintf(buf, PAGE_SIZE, "not support\n");
#endif
}
/*----------------------------------------------------------------------------*/
static ssize_t store_firlen_value(struct device_driver *ddri, const char *buf, size_t count)
{
#ifdef CONFIG_LSM6DS0_ACC_LOWPASS
	struct i2c_client *client = LSM6DS0_ACC_i2c_client;  
	struct LSM6DS0_ACC_i2c_data *obj = i2c_get_clientdata(client);
	int firlen;

	if(1 != sscanf(buf, "%d", &firlen))
	{
		GSE_ERR("invallid format\n");
	}
	else if(firlen > C_MAX_FIR_LENGTH)
	{
		GSE_ERR("exceeds maximum filter length\n");
	}
	else
	{ 
		atomic_set(&obj->firlen, firlen);
		if(0 == firlen)
		{
			atomic_set(&obj->fir_en, 0);
		}
		else
		{
			memset(&obj->fir, 0x00, sizeof(obj->fir));
			atomic_set(&obj->fir_en, 1);
		}
	}
#endif    
	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t show_trace_value(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	struct LSM6DS0_ACC_i2c_data *obj = obj_i2c_data;
	if (obj == NULL)
	{
		GSE_ERR("i2c_data obj is null!!\n");
		return 0;
	}
	
	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));     
	return res;    
}
/*----------------------------------------------------------------------------*/
static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct LSM6DS0_ACC_i2c_data *obj = obj_i2c_data;
	int trace;
	if (obj == NULL)
	{
		GSE_ERR("i2c_data obj is null!!\n");
		return 0;
	}
	
	if(1 == sscanf(buf, "0x%x", &trace))
	{
		atomic_set(&obj->trace, trace);
	}	
	else
	{
		GSE_ERR("invalid content: '%s', length = %d\n", buf, count);
	}
	
	return count;    
}
/*----------------------------------------------------------------------------*/
static ssize_t show_status_value(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;    
	struct LSM6DS0_ACC_i2c_data *obj = obj_i2c_data;
	if (obj == NULL)
	{
		GSE_ERR("i2c_data obj is null!!\n");
		return 0;
	}	
	
	if(obj->hw)
	{
		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: %d %d (%d %d)\n", 
	            obj->hw->i2c_num, obj->hw->direction, obj->hw->power_id, obj->hw->power_vol);   
	}
	else
	{
		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: NULL\n");
	}
	return len;    
}
/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(chipinfo,             S_IRUGO, show_chipinfo_value,      NULL);
static DRIVER_ATTR(sensordata,           S_IRUGO, show_sensordata_value,    NULL);
static DRIVER_ATTR(cali,       S_IWUSR | S_IRUGO, show_cali_value,          store_cali_value);
static DRIVER_ATTR(power,                S_IRUGO, show_power_status,          NULL);
static DRIVER_ATTR(firlen,     S_IWUSR | S_IRUGO, show_firlen_value,        store_firlen_value);
static DRIVER_ATTR(trace,      S_IWUSR | S_IRUGO, show_trace_value,         store_trace_value);
static DRIVER_ATTR(status,               S_IRUGO, show_status_value,        NULL);
/*----------------------------------------------------------------------------*/
static struct driver_attribute *LSM6DS0_ACC_attr_list[] = {
	&driver_attr_chipinfo,     /*chip information*/
	&driver_attr_sensordata,   /*dump sensor data*/
	&driver_attr_cali,         /*show calibration data*/
	&driver_attr_power,         /*show power reg*/
	&driver_attr_firlen,       /*filter length: 0: disable, others: enable*/
	&driver_attr_trace,        /*trace log*/
	&driver_attr_status,        
};
/*----------------------------------------------------------------------------*/
static int LSM6DS0_ACC_create_attr(struct device_driver *driver) 
{
	int idx, err = 0;
	int num = (int)(sizeof(LSM6DS0_ACC_attr_list)/sizeof(LSM6DS0_ACC_attr_list[0]));
	if (driver == NULL)
	{
		return -EINVAL;
	}

	for(idx = 0; idx < num; idx++)
	{
		if((err = driver_create_file(driver, LSM6DS0_ACC_attr_list[idx])))
		{            
			GSE_ERR("driver_create_file (%s) = %d\n", LSM6DS0_ACC_attr_list[idx]->attr.name, err);
			break;
		}
	}    
	return err;
}
/*----------------------------------------------------------------------------*/
static int LSM6DS0_ACC_delete_attr(struct device_driver *driver)
{
	int idx ,err = 0;
	int num = (int)(sizeof(LSM6DS0_ACC_attr_list)/sizeof(LSM6DS0_ACC_attr_list[0]));

	if(driver == NULL)
	{
		return -EINVAL;
	}
	
	for(idx = 0; idx < num; idx++)
	{
		driver_remove_file(driver, LSM6DS0_ACC_attr_list[idx]);
	}
	
	return err;
}

/*----------------------------------------------------------------------------*/
int gsensor_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value, sample_delay;	
	struct LSM6DS0_ACC_i2c_data *priv = (struct LSM6DS0_ACC_i2c_data*)self;
	hwm_sensor_data* gsensor_data;
	char buff[LSM6DS0_ACC_BUFSIZE];
	
	//GSE_FUN(f);
	switch (command)
	{
		case SENSOR_DELAY:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				GSE_ERR("Set delay parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				value = *(int *)buff_in;
				if(value <= 5)
				{
					sample_delay = LSM6DS0_ACC_BW_400HZ;
				}
				else if(value <= 10)
				{
					sample_delay = LSM6DS0_ACC_BW_100HZ;
				}
				else
				{
					sample_delay = LSM6DS0_ACC_BW_50HZ;
				}
				
				err = LSM6DS0_ACC_SetBWRate(priv->client, sample_delay);
				if(err != LSM6DS0_ACC_SUCCESS ) //0x2C->BW=100Hz
				{
					GSE_ERR("Set delay parameter error!\n");
				}

				if(value >= 50)
				{
					atomic_set(&priv->filter, 0);
				}
				else
				{					
					priv->fir.num = 0;
					priv->fir.idx = 0;
					priv->fir.sum[LSM6DS0_ACC_AXIS_X] = 0;
					priv->fir.sum[LSM6DS0_ACC_AXIS_Y] = 0;
					priv->fir.sum[LSM6DS0_ACC_AXIS_Z] = 0;
					atomic_set(&priv->filter, 1);
				}
			}
			break;

		case SENSOR_ENABLE:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				GSE_ERR("Enable sensor parameter error!\n");
				err = -EINVAL;
			}
			else
			{
			    
				value = *(int *)buff_in;
				GSE_LOG("enable value=%d, sensor_power =%d\n",value,sensor_power);
				if(((value == 0) && (sensor_power == false)) ||((value == 1) && (sensor_power == true)))
				{
					GSE_LOG("Gsensor device have updated!\n");
				}
				else
				{
					err = LSM6DS0_ACC_SetPowerMode( priv->client, !sensor_power);
				}
			}
			break;

		case SENSOR_GET_DATA:
			if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
			{
				GSE_ERR("get sensor data parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				gsensor_data = (hwm_sensor_data *)buff_out;
				LSM6DS0_ACC_ReadSensorData(priv->client, buff, LSM6DS0_ACC_BUFSIZE);
				sscanf(buff, "%x %x %x", &gsensor_data->values[0], 
					&gsensor_data->values[1], &gsensor_data->values[2]);				
				gsensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;				
				gsensor_data->value_divide = 1000;
				
			}
			break;
		default:
			GSE_ERR("gsensor operate function no this parameter %d!\n", command);
			err = -1;
			break;
	}
	
	return err;
}

/****************************************************************************** 
 * Function Configuration
******************************************************************************/
static int LSM6DS0_ACC_open(struct inode *inode, struct file *file)
{
	file->private_data = LSM6DS0_ACC_i2c_client;

	if(file->private_data == NULL)
	{
		GSE_ERR("null pointer!!\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}
/*----------------------------------------------------------------------------*/
static int LSM6DS0_ACC_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}
/*----------------------------------------------------------------------------*/
//static int LSM6DS0_ACC_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
//       unsigned long arg)
static long LSM6DS0_ACC_unlocked_ioctl(struct file *file, unsigned int cmd,
       unsigned long arg)

{
	struct i2c_client *client = (struct i2c_client*)file->private_data;
	struct LSM6DS0_ACC_i2c_data *obj = (struct LSM6DS0_ACC_i2c_data*)i2c_get_clientdata(client);	
	char strbuf[LSM6DS0_ACC_BUFSIZE];
	void __user *data;
	SENSOR_DATA sensor_data;
	long err = 0;
	int cali[3];

	//GSE_FUN(f);
	if(_IOC_DIR(cmd) & _IOC_READ)
	{
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	}
	else if(_IOC_DIR(cmd) & _IOC_WRITE)
	{
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	}

	if(err)
	{
		GSE_ERR("access error: %08X, (%2d, %2d)\n", cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
		return -EFAULT;
	}

	switch(cmd)
	{
		case GSENSOR_IOCTL_INIT:
			LSM6DS0_ACC_Init(client, 0);			
			break;

		case GSENSOR_IOCTL_READ_CHIPINFO:
			data = (void __user *) arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}
			
			LSM6DS0_ACC_ReadChipInfo(client, strbuf, LSM6DS0_ACC_BUFSIZE);
			if(copy_to_user(data, strbuf, strlen(strbuf)+1))
			{
				err = -EFAULT;
				break;
			}				 
			break;	  

		case GSENSOR_IOCTL_READ_SENSORDATA:
			data = (void __user *) arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}
			LSM6DS0_ACC_SetPowerMode(client,true);
			LSM6DS0_ACC_ReadSensorData(client, strbuf, LSM6DS0_ACC_BUFSIZE);
			if(copy_to_user(data, strbuf, strlen(strbuf)+1))
			{
				err = -EFAULT;
				break;	  
			}				 
			break;

		case GSENSOR_IOCTL_READ_GAIN:
			data = (void __user *) arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}			
			
			if(copy_to_user(data, &gsensor_gain, sizeof(GSENSOR_VECTOR3D)))
			{
				err = -EFAULT;
				break;
			}				 
			break;

		case GSENSOR_IOCTL_READ_OFFSET:
			data = (void __user *) arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}
			
			if(copy_to_user(data, &gsensor_offset, sizeof(GSENSOR_VECTOR3D)))
			{
				err = -EFAULT;
				break;
			}				 
			break;

		case GSENSOR_IOCTL_READ_RAW_DATA:
			data = (void __user *) arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}
			LSM6DS0_ACC_ReadRawData(client, strbuf);
			if(copy_to_user(data, &strbuf, strlen(strbuf)+1))
			{
				err = -EFAULT;
				break;	  
			}
			break;	  

		case GSENSOR_IOCTL_SET_CALI:
			data = (void __user*)arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}
			if(copy_from_user(&sensor_data, data, sizeof(sensor_data)))
			{
				err = -EFAULT;
				break;	  
			}
			if(atomic_read(&obj->suspend))
			{
				GSE_ERR("Perform calibration in suspend state!!\n");
				err = -EINVAL;
			}
			else
			{
				cali[LSM6DS0_ACC_AXIS_X] = sensor_data.x * obj->reso->sensitivity / GRAVITY_EARTH_1000;
				cali[LSM6DS0_ACC_AXIS_Y] = sensor_data.y * obj->reso->sensitivity / GRAVITY_EARTH_1000;
				cali[LSM6DS0_ACC_AXIS_Z] = sensor_data.z * obj->reso->sensitivity / GRAVITY_EARTH_1000;			  
				err = LSM6DS0_ACC_WriteCalibration(client, cali);			 
			}
			break;

		case GSENSOR_IOCTL_CLR_CALI:
			err = LSM6DS0_ACC_ResetCalibration(client);
			break;

		case GSENSOR_IOCTL_GET_CALI:
			data = (void __user*)arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}
			if((err = LSM6DS0_ACC_ReadCalibration(client, cali)))
			{
				break;
			}
			
			sensor_data.x = cali[LSM6DS0_ACC_AXIS_X] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
			sensor_data.y = cali[LSM6DS0_ACC_AXIS_Y] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
			sensor_data.z = cali[LSM6DS0_ACC_AXIS_Z] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
			if(copy_to_user(data, &sensor_data, sizeof(sensor_data)))
			{
				err = -EFAULT;
				break;
			}		
			break;
		

		default:
			GSE_ERR("unknown IOCTL: 0x%08x\n", cmd);
			err = -ENOIOCTLCMD;
			break;
			
	}

	return err;
}


/*----------------------------------------------------------------------------*/
static struct file_operations LSM6DS0_ACC_fops = {
	.owner = THIS_MODULE,
	.open = LSM6DS0_ACC_open,
	.release = LSM6DS0_ACC_release,
	//.ioctl = LSM6DS0_ACC_ioctl,
	.unlocked_ioctl = LSM6DS0_ACC_unlocked_ioctl,
};
/*----------------------------------------------------------------------------*/
static struct miscdevice LSM6DS0_ACC_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gsensor",
	.fops = &LSM6DS0_ACC_fops,
};
/*----------------------------------------------------------------------------*/
#ifndef CONFIG_HAS_EARLYSUSPEND
/*----------------------------------------------------------------------------*/
static int LSM6DS0_ACC_suspend(struct i2c_client *client, pm_message_t msg) 
{
	struct LSM6DS0_ACC_i2c_data *obj = i2c_get_clientdata(client);    
	int err = 0;
	u8 dat;
	GSE_FUN();    

	if(msg.event == PM_EVENT_SUSPEND)
	{   
		if(obj == NULL)
		{
			GSE_ERR("null pointer!!\n");
			return -EINVAL;
		}

		atomic_set(&obj->suspend, 1);
		if((err = LSM6DS0_ACC_SetPowerMode(obj->client, false)))
		{
			GSE_ERR("write power control fail!!\n");
			
			return err;        
		}      
		LSM6DS0_ACC_power(obj->hw, 0);
	}
	return err;
}
/*----------------------------------------------------------------------------*/
static int LSM6DS0_ACC_resume(struct i2c_client *client)
{
	struct LSM6DS0_ACC_i2c_data *obj = i2c_get_clientdata(client);        
	int err;
	GSE_FUN();

	if(obj == NULL)
	{
		GSE_ERR("null pointer!!\n");
		return -EINVAL;
	}

	LSM6DS0_ACC_power(obj->hw, 1);
	if(err = LSM6DS0_ACC_Init(client, 0))
	{
		GSE_ERR("initialize client fail!!\n");
		return err;        
	}
	atomic_set(&obj->suspend, 0);

	return 0;
}
/*----------------------------------------------------------------------------*/
#else /*CONFIG_HAS_EARLY_SUSPEND is defined*/
/*----------------------------------------------------------------------------*/

static void LSM6DS0_ACC_early_suspend(struct early_suspend *h) 
{
	struct LSM6DS0_ACC_i2c_data *obj = container_of(h, struct LSM6DS0_ACC_i2c_data, early_drv);   
	int err = 0;
	GSE_LOG("LSM6DS0_ACC_early_suspend\n");

	if(obj == NULL)
	{
		GSE_ERR("null pointer!!\n");
		return;
	}

	atomic_set(&obj->suspend, 1); 
	if((err = LSM6DS0_ACC_SetPowerMode(obj->client, false)))
	{
		GSE_ERR("LSM6DS0_ACC_early_suspend  write power control fail!!\n");	
		return;        
	}
	
	LSM6DS0_ACC_power(obj->hw, 0);
	return;
}
/*----------------------------------------------------------------------------*/
static void LSM6DS0_ACC_late_resume(struct early_suspend *h)
{
	struct LSM6DS0_ACC_i2c_data *obj = container_of(h, struct LSM6DS0_ACC_i2c_data, early_drv);         
	int err;
	GSE_LOG("LSM6DS0_ACC_late_resume\n");

	if(obj == NULL)
	{
		GSE_ERR("null pointer!!\n");
		return;
	}

	atomic_set(&obj->suspend, 0);
	if((err = LSM6DS0_ACC_SetPowerMode(obj->client, true)))
	{
		GSE_ERR("LSM6DS0_ACC_late_resume write power control fail!!\n");
		return;        
	}

	LSM6DS0_ACC_power(obj->hw, 1);
	
	return;
}
/*----------------------------------------------------------------------------*/
#endif /*CONFIG_HAS_EARLYSUSPEND*/
/*----------------------------------------------------------------------------*/
static int LSM6DS0_ACC_i2c_detect(struct i2c_client *client, struct i2c_board_info *info) 
{    
	strcpy(info->type, LSM6DS0_ACC_DEV_NAME);
	return 0;
}

/*----------------------------------------------------------------------------*/
static int LSM6DS0_ACC_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_client *new_client;
	struct LSM6DS0_ACC_i2c_data *obj;
	struct hwmsen_object sobj;
	int err = 0;
	GSE_FUN();

	if(!(obj = kzalloc(sizeof(*obj), GFP_KERNEL)))
	{
		err = -ENOMEM;
		goto exit;
	}
	
	memset(obj, 0, sizeof(struct LSM6DS0_ACC_i2c_data));

	obj->hw = get_cust_acc_hw();
	
	if((err = hwmsen_get_convert(obj->hw->direction, &obj->cvt)))
	{
		GSE_ERR("invalid direction: %d\n", obj->hw->direction);
		goto exit;
	}

	obj_i2c_data = obj;

	client->addr = 0x6a;
	
	obj->client = client;
	new_client = obj->client;
	
	i2c_set_clientdata(new_client,obj);
	
	atomic_set(&obj->trace, 0);
	atomic_set(&obj->suspend, 0);
	
#ifdef CONFIG_LSM6DS0_ACC_LOWPASS
	if(obj->hw->firlen > C_MAX_FIR_LENGTH)
	{
		atomic_set(&obj->firlen, C_MAX_FIR_LENGTH);
	}	
	else
	{
		atomic_set(&obj->firlen, obj->hw->firlen);
	}
	
	if(atomic_read(&obj->firlen) > 0)
	{
		atomic_set(&obj->fir_en, 1);
	}
	
#endif

	LSM6DS0_ACC_i2c_client = new_client;	

	if((err = LSM6DS0_ACC_Init(new_client, 1)))
	{
		goto exit_init_failed;
	}
	
	if((err = misc_register(&LSM6DS0_ACC_device)))
	{
		GSE_ERR("LSM6DS0_ACC_device register failed\n");
		goto exit_misc_device_register_failed;
	}

	if((err = LSM6DS0_ACC_create_attr(&LSM6DS0_ACC_gsensor_driver.driver)))
	{
		GSE_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}

	sobj.self = obj;
    sobj.polling = 1;
    sobj.sensor_operate = gsensor_operate;
	if((err = hwmsen_attach(ID_ACCELEROMETER, &sobj)))
	{
		GSE_ERR("attach fail = %d\n", err);
		goto exit_kfree;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	obj->early_drv.level    = EARLY_SUSPEND_LEVEL_DISABLE_FB - 1,
	obj->early_drv.suspend  = LSM6DS0_ACC_early_suspend,
	obj->early_drv.resume   = LSM6DS0_ACC_late_resume,    
	register_early_suspend(&obj->early_drv);
#endif 

	GSE_LOG("%s: OK\n", __func__);    
	return 0;

	exit_create_attr_failed:
	misc_deregister(&LSM6DS0_ACC_device);
	exit_misc_device_register_failed:
	exit_init_failed:
	//i2c_detach_client(new_client);
	exit_kfree:
	kfree(obj);
	exit:
	GSE_ERR("%s: err = %d\n", __func__, err);        
	return err;
}

/*----------------------------------------------------------------------------*/
static int LSM6DS0_ACC_i2c_remove(struct i2c_client *client)
{
	int err = 0;	
	
	if((err = LSM6DS0_ACC_delete_attr(&LSM6DS0_ACC_gsensor_driver.driver)))
	{
		GSE_ERR("LSM6DS0_ACC_delete_attr fail: %d\n", err);
	}
	
	if((err = misc_deregister(&LSM6DS0_ACC_device)))
	{
		GSE_ERR("misc_deregister fail: %d\n", err);
	}

	if((err = hwmsen_detach(ID_ACCELEROMETER)))
	    

	LSM6DS0_ACC_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));
	return 0;
}
/*----------------------------------------------------------------------------*/
static int LSM6DS0_ACC_probe(struct platform_device *pdev) 
{
	struct acc_hw *hw = get_cust_acc_hw();
	GSE_FUN();

	LSM6DS0_ACC_power(hw, 1);
	//LSM6DS0_ACC_force[0] = hw->i2c_num;
	if(i2c_add_driver(&LSM6DS0_ACC_i2c_driver))
	{
		GSE_ERR("add driver error\n");
		return -1;
	}
	return 0;
}
/*----------------------------------------------------------------------------*/
static int LSM6DS0_ACC_remove(struct platform_device *pdev)
{
    struct acc_hw *hw = get_cust_acc_hw();

    GSE_FUN();    
    LSM6DS0_ACC_power(hw, 0);    
    i2c_del_driver(&LSM6DS0_ACC_i2c_driver);
    return 0;
}
/*----------------------------------------------------------------------------*/
static struct platform_driver LSM6DS0_ACC_gsensor_driver = {
	.probe      = LSM6DS0_ACC_probe,
	.remove     = LSM6DS0_ACC_remove,    
	.driver     = {
		.name  = "gsensor",
		.owner = THIS_MODULE,
	}
};

/*----------------------------------------------------------------------------*/
static int __init LSM6DS0_ACC_init(void)
{
	//GSE_FUN();
	struct acc_hw *hw = get_cust_acc_hw();
	GSE_LOG("%s: i2c_number=%d\n", __func__,hw->i2c_num); 
	i2c_register_board_info(hw->i2c_num, &i2c_LSM6DS0_ACC, 1);
	if(platform_driver_register(&LSM6DS0_ACC_gsensor_driver))
	{
		GSE_ERR("failed to register driver");
		return -ENODEV;
	}
	return 0;    
}
/*----------------------------------------------------------------------------*/
static void __exit LSM6DS0_ACC_exit(void)
{
	GSE_FUN();
	platform_driver_unregister(&LSM6DS0_ACC_gsensor_driver);
}
/*----------------------------------------------------------------------------*/
module_init(LSM6DS0_ACC_init);
module_exit(LSM6DS0_ACC_exit);
/*----------------------------------------------------------------------------*/
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("LSM6DS0_ACC I2C driver");
MODULE_AUTHOR("Chunlei.Wang@mediatek.com");
