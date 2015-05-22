/* LSM303D motion sensor driver
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
#include "lsm303d.h"
#include <linux/hwmsen_helper.h>



#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>

/*-------------------------MT6516&MT6573 define-------------------------------*/

#define POWER_NONE_MACRO MT65XX_POWER_NONE



#define SW_CALIBRATION

/*----------------------------------------------------------------------------*/
#define I2C_DRIVERID_LSM303D 345
/*----------------------------------------------------------------------------*/
#define DEBUG 1
/*----------------------------------------------------------------------------*/
#define CONFIG_LSM303D_LOWPASS   /*apply low pass filter on output*/       
/*----------------------------------------------------------------------------*/
#define LSM303D_AXIS_X          0
#define LSM303D_AXIS_Y          1
#define LSM303D_AXIS_Z          2
#define LSM303D_AXES_NUM        3
#define LSM303D_DATA_LEN        6
#define LSM303D_DEV_NAME        "lsm303d"
/*----------------------------------------------------------------------------*/
static const struct i2c_device_id lsm303d_i2c_id[] = {{LSM303D_DEV_NAME,0},{}};
static struct i2c_board_info __initdata i2c_lsm303d={ I2C_BOARD_INFO("lsm303d", 0x3C>>1)};
/*the adapter id will be available in customization*/
//static unsigned short lsm303d_force[] = {0x00, LSM303D_I2C_SLAVE_ADDR, I2C_CLIENT_END, I2C_CLIENT_END};
//static const unsigned short *const lsm303d_forces[] = { lsm303d_force, NULL };
//static struct i2c_client_address_data lsm303d_addr_data = { .forces = lsm303d_forces,};

/*----------------------------------------------------------------------------*/
static int lsm303d_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id); 
static int lsm303d_i2c_remove(struct i2c_client *client);
//static int lsm303d_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);
static int lsm303d_suspend(struct i2c_client *client, pm_message_t msg) ;
static int lsm303d_resume(struct i2c_client *client);

/*----------------------------------------------------------------------------*/
typedef enum {
    ADX_TRC_FILTER  =     0x01,
    ADX_TRC_RAWDATA =     0x02,
    ADX_TRC_IOCTL   =     0x04,
    ADX_TRC_CALI	= 0X08,
    ADX_TRC_INFO	= 0X10,
} ADX_TRC;

static DEFINE_MUTEX(lsm303d_mutex);

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
    s16 raw[C_MAX_FIR_LENGTH][LSM303D_AXES_NUM];
    int sum[LSM303D_AXES_NUM];
    int num;
    int idx;
};
/*----------------------------------------------------------------------------*/
struct lsm303d_i2c_data {
    struct i2c_client *client;
    struct acc_hw *hw;
    struct hwmsen_convert   cvt;
    
    /*misc*/
    struct data_resolution  reso;
    atomic_t                trace;
    atomic_t                suspend;
    atomic_t                selftest;
	atomic_t				filter;
    s16                     cali_sw[LSM303D_AXES_NUM+1];

    /*data*/
    s8                      offset[LSM303D_AXES_NUM+1];  /*+1: for 4-byte alignment*/
    s32                     data[LSM303D_AXES_NUM+1];

#if defined(CONFIG_LSM303D_LOWPASS)
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
static struct i2c_driver lsm303d_i2c_driver = {
    .driver = {
//        .owner          = THIS_MODULE,
        .name           = LSM303D_DEV_NAME,
    },
	.probe      		= lsm303d_i2c_probe,
	.remove    			= lsm303d_i2c_remove,
//	.detect				= lsm303d_i2c_detect,
//#if !defined(CONFIG_HAS_EARLYSUSPEND)    
    .suspend            = lsm303d_suspend,
    .resume             = lsm303d_resume,
//#endif
	.id_table = lsm303d_i2c_id,
	//.address_list= lsm303d_forces,
};

/*----------------------------------------------------------------------------*/
static struct i2c_client *lsm303d_i2c_client = NULL;
static struct platform_driver lsm303d_gsensor_driver;
static struct lsm303d_i2c_data *obj_i2c_data = NULL;
static bool sensor_power = false;
static GSENSOR_VECTOR3D gsensor_gain;
static char selftestRes[8]= {0}; 


/*----------------------------------------------------------------------------*/
#define GSE_TAG                  "[Gsensor] "
#define GSE_FUN(f)               printk(KERN_ERR GSE_TAG"%s\n", __FUNCTION__)
#define GSE_ERR(fmt, args...)    printk(KERN_ERR GSE_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define GSE_LOG(fmt, args...)    printk(KERN_ERR GSE_TAG fmt, ##args)
/*----------------------------------------------------------------------------*/
static struct data_resolution lsm303d_data_resolution[] = {
 /*8 combination by {FULL_RES,RANGE}*/
    {{ 3, 9}, 256},   /*+/-2g  in 10-bit resolution:  3.9 mg/LSB*/
    {{ 7, 8}, 128},   /*+/-4g  in 10-bit resolution:  7.8 mg/LSB*/
    {{15, 6},  64},   /*+/-8g  in 10-bit resolution: 15.6 mg/LSB*/
    {{31, 2},  32},   /*+/-16g in 10-bit resolution: 31.2 mg/LSB*/
    {{ 3, 9}, 256},   /*+/-2g  in 10-bit resolution:  3.9 mg/LSB (full-resolution)*/
    {{ 3, 9}, 256},   /*+/-4g  in 11-bit resolution:  3.9 mg/LSB (full-resolution)*/
    {{ 3, 9}, 256},   /*+/-8g  in 12-bit resolution:  3.9 mg/LSB (full-resolution)*/
    {{ 3, 9}, 256},   /*+/-16g in 13-bit resolution:  3.9 mg/LSB (full-resolution)*/            
};
/*----------------------------------------------------------------------------*/
static struct data_resolution lsm303d_offset_resolution = {{15, 6}, 64};

/*--------------------ADXL power control function----------------------------------*/
static void lsm303d_power(struct acc_hw *hw, unsigned int on) 
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
			if(!hwPowerOn(hw->power_id, hw->power_vol, "LSM303D"))
			{
				GSE_ERR("power on fails!!\n");
			}
		}
		else	// power off
		{
			if (!hwPowerDown(hw->power_id, "LSM303D"))
			{
				GSE_ERR("power off fail!!\n");
			}			  
		}
	}
	power_on = on;    
}



/*----------------------------------------- OK -----------------------------------*/
static int lsm303d_CheckDeviceID(struct i2c_client *client)
{
	//GSE_LOG("++++++++++++++++++++++LSM303D_CheckDeviceID!");

	GSE_FUN();

	u8 databuf[10];    
	int res = 0;

	memset(databuf, 0, sizeof(u8)*10);    
	databuf[0] = LSM303D_REG_DEVID;    

	res = i2c_master_send(client, databuf, 0x1);
	if(res <= 0)
	{
		goto exit_LSM303D_CheckDeviceID;
	}
	
	udelay(500);

	databuf[0] = 0x0;        
	res = i2c_master_recv(client, databuf, 0x01);
	if(res <= 0)
	{
		goto exit_LSM303D_CheckDeviceID;
	}
	

	if(databuf[0]!=LSM303D_FIXED_DEVID)
	{
		return LSM303D_ERR_IDENTIFICATION;
	}

	exit_LSM303D_CheckDeviceID:
	if (res <= 0)
	{
		return LSM303D_ERR_I2C;
	}
	
	return LSM303D_SUCCESS;
}





/*-----------------------------------   ok  -----------------------------------------*/
static int lsm303d_SetBWRate(struct i2c_client *client, u8 bwrate)
{

	GSE_FUN();
	u8 databuf[10];    
	int res = 0;

	memset(databuf, 0, sizeof(u8)*10); 

	databuf[0] = LSM303D_REG_BW_RATE;   

	mutex_lock(&lsm303d_mutex);
	/**
	
	res = i2c_master_send(client, databuf, 0x1);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
		
	udelay(500);
	GSE_LOG("send cmd");
	
	databuf[0] = 0x0;		 
	res = i2c_master_recv(client, databuf, 0x01);
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	**/
	GSE_LOG("recv data ");
	//bwrate = ((ODR_ACC_MASK & bwrate) | ((~ODR_ACC_MASK) & databuf[0]) | LSM303D_ACC_ODR_ENABLE);
		
	bwrate = ((ODR_ACC_MASK & bwrate) | LSM303D_ACC_ODR_ENABLE);

	
	databuf[0] = LSM303D_REG_BW_RATE;    
	databuf[1] = bwrate;

	res = i2c_master_send(client, databuf, 0x2);

	GSE_LOG("send rate");

	if(res <=0)
	{
		goto EXIT_ERR;
	
	}
	mutex_unlock(&lsm303d_mutex);
	
	return LSM303D_SUCCESS; 

	EXIT_ERR:
	mutex_unlock(&lsm303d_mutex);
	return LSM303D_ERR_I2C;
	
}


/*----------------------------------  done  ------------------------------------------*/
static int lsm303d_SetDataResolution(struct lsm303d_i2c_data *obj,  u8 new_fs_range)
{
	GSE_FUN();
	int err;
	u8  dat, reso;

	switch (new_fs_range) {
	case LSM303D_ACC_FS_2G:
		obj->reso.sensitivity= SENSITIVITY_ACC_2G;
		break;
	case LSM303D_ACC_FS_4G:
		obj->reso.sensitivity = SENSITIVITY_ACC_4G;
		break;
	case LSM303D_ACC_FS_8G:
		obj->reso.sensitivity = SENSITIVITY_ACC_8G;
		break;
	case LSM303D_ACC_FS_16G:
		obj->reso.sensitivity = SENSITIVITY_ACC_16G;
		break;
	default:
		obj->reso.sensitivity = SENSITIVITY_ACC_2G;
		GSE_LOG("invalid magnetometer fs range requested: %u\n", new_fs_range);
		return -EINVAL;
	}

	return 0;
	
}


/*---------------------------------  ok -------------------------------------------*/
static int lsm303d_SetDataFormat(struct i2c_client *client, u8 dataformat)
{
	GSE_FUN();
	struct lsm303d_i2c_data *obj = i2c_get_clientdata(client);
	u8 databuf[10];    
	int res = 0;

	memset(databuf, 0, sizeof(u8)*10); 

	databuf[0] = LSM303D_REG_DATA_FORMAT;    
	
	res = i2c_master_send(client, databuf, 0x1);
	if(res <= 0)
	{
		return LSM303D_ERR_I2C;
	}
		
	udelay(500);
	
	databuf[0] = 0x0;		 
	res = i2c_master_recv(client, databuf, 0x01);
	if(res <= 0)
	{
		return LSM303D_ERR_I2C;
	}
		
	dataformat = ((LSM303D_ACC_FS_MASK & dataformat) | 
				((~LSM303D_ACC_FS_MASK) & databuf[0]));

	   
	databuf[0] = LSM303D_REG_DATA_FORMAT;    
	databuf[1] = dataformat;

	res = i2c_master_send(client, databuf, 0x2);

	if(res <= 0)
	{
		return LSM303D_ERR_I2C;
	}
	
	return lsm303d_SetDataResolution(obj, dataformat);  

}



/*---------------------------------------  OK   -------------------------------------*/
static int lsm303d_SetPowerMode(struct i2c_client *client, bool enable)
{
  
	GSE_FUN();
	int res = 0;

	
	if(enable == sensor_power)
	{
		GSE_LOG("Sensor power status is newest!\n");
		return LSM303D_SUCCESS;
	}

	
	if(enable == TRUE)
	{
		res = lsm303d_SetBWRate(client, LSM303D_ACC_ODR100);
	}
	else
	{
		res = lsm303d_SetBWRate(client, LSM303D_ACC_ODR_OFF);
	}
	
	if(res < 0)
	{
		GSE_LOG("set power mode failed!\n");
		return LSM303D_ERR_I2C;
	}
	

	sensor_power = enable;
	return LSM303D_SUCCESS;   
 
}


/*---------------------------------  ok -------------------------------------------*/
static int lsm303d_SetFilterLen(struct i2c_client *client, u8 new_bandwidth)
{
	GSE_FUN();
	struct lsm303d_i2c_data *obj = i2c_get_clientdata(client);
	u8 databuf[10];    
	int res = 0;

	memset(databuf, 0, sizeof(u8)*10); 

	databuf[0] = LSM303D_REG_DATA_FILTER;    
	
	res = i2c_master_send(client, databuf, 0x1);
	if(res <= 0)
	{
		return LSM303D_ERR_I2C;
	}
		
	udelay(500);
	
	databuf[0] = 0x0;		 
	res = i2c_master_recv(client, databuf, 0x01);
	if(res <= 0)
	{
		return LSM303D_ERR_I2C;
	}
		
	new_bandwidth = ((LSM303D_ACC_FILTER_MASK & new_bandwidth) | 
					((~LSM303D_ACC_FILTER_MASK) & databuf[0]));

	   
	databuf[0] = LSM303D_REG_DATA_FILTER;    
	databuf[1] = new_bandwidth;

	res = i2c_master_send(client, databuf, 0x2);

	if(res <= 0)
	{
		return LSM303D_ERR_I2C;
	}
	

	return 0;

}

/*----------------------------------------------------------------------------*/
static int lsm303d_ReadChipInfo(struct i2c_client *client, char *buf, int bufsize)
{
	GSE_FUN();
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

	sprintf(buf, "LSM303D Chip");
	return 0;
}


/*----------------------------------------------------------------------------*/
static int lsm303d_ReadOffset(struct i2c_client *client, s8 ofs[LSM303D_AXES_NUM])
{    
	int err = 0;;

#ifdef SW_CALIBRATION
	ofs[0]=ofs[1]=ofs[2]=0x0;
#else
	if((err = hwmsen_read_block(client, MC3210_REG_OFSX, ofs, LSM303D_AXES_NUM)))
	{
		GSE_ERR("error: %d\n", err);
	}
#endif

	return err;    
}



/*----------------------------------------------------------------------------*/
static int lsm303d_ReadCalibrationEx(struct i2c_client *client, int act[LSM303D_AXES_NUM], int raw[LSM303D_AXES_NUM])
{  
	/*raw: the raw calibration data; act: the actual calibration data*/
	struct lsm303d_i2c_data *obj = i2c_get_clientdata(client);
	int err;
	int mul;

	#ifdef SW_CALIBRATION
		mul = 0;//only SW Calibration, disable HW Calibration	
	#else
	      if((err = lsm303d_ReadOffset(client, obj->offset)))
       	{
		GSE_ERR("read offset fail, %d\n", err);
		return err;
	     }    
       	mul = obj->reso.sensitivity/mc3210_offset_resolution.sensitivity;
	#endif
	
	raw[LSM303D_AXIS_X] = obj->offset[LSM303D_AXIS_X]*mul + obj->cali_sw[LSM303D_AXIS_X];
	raw[LSM303D_AXIS_Y] = obj->offset[LSM303D_AXIS_Y]*mul + obj->cali_sw[LSM303D_AXIS_Y];
	raw[LSM303D_AXIS_Z] = obj->offset[LSM303D_AXIS_Z]*mul + obj->cali_sw[LSM303D_AXIS_Z];

	act[obj->cvt.map[LSM303D_AXIS_X]] = obj->cvt.sign[LSM303D_AXIS_X]*raw[LSM303D_AXIS_X];
	act[obj->cvt.map[LSM303D_AXIS_Y]] = obj->cvt.sign[LSM303D_AXIS_Y]*raw[LSM303D_AXIS_Y];
	act[obj->cvt.map[LSM303D_AXIS_Z]] = obj->cvt.sign[LSM303D_AXIS_Z]*raw[LSM303D_AXIS_Z];                        

	return 0;
}


/*----------------------------------------------------------------------------*/
static int lsm303d_ResetCalibration(struct i2c_client *client)
{
	struct lsm303d_i2c_data *obj = i2c_get_clientdata(client);
	s8 ofs[LSM303D_AXES_NUM] = {0x00, 0x00, 0x00};
	int err;
	
	#ifdef SW_CALIBRATION
		
	#else
		if(err = hwmsen_write_block(client, LSM303D_REG_OFSX, ofs, 4))
		{
			GSE_ERR("error: %d\n", err);
		}
	#endif

	memset(obj->cali_sw, 0x00, sizeof(obj->cali_sw));
	memset(obj->offset, 0x00, sizeof(obj->offset));
	return err;    
}

/*----------------------------------------------------------------------------*/
static int lsm303d_ReadCalibration(struct i2c_client *client, int dat[LSM303D_AXES_NUM])
{
    struct lsm303d_i2c_data *obj = i2c_get_clientdata(client);
    int err = 0;
    int mul;

      #ifdef SW_CALIBRATION
	  struct lsm303d_i2c_data *priv = i2c_get_clientdata(client);        
	  u8 addr = LSM303D_REG_DATAX0;
	  u8 buf[LSM303D_DATA_LEN] = {0};
	  int i;
	  s16 data[LSM303D_AXES_NUM] = {0x00,0x00,0x00};

	  if(NULL == client)
	  {
	  	err = -EINVAL;
	  }
	  else if((err = hwmsen_read_block(client, addr, buf, 0x06)))
	  {
		GSE_ERR("error: %d\n", err);
          }
	  else
	  {
		data[LSM303D_AXIS_X] = (s16)((buf[LSM303D_AXIS_X*2]) |
		         (buf[LSM303D_AXIS_X*2+1] << 8));
		data[LSM303D_AXIS_Y] = (s16)((buf[LSM303D_AXIS_Y*2]) |
		         (buf[LSM303D_AXIS_Y*2+1] << 8));
		data[LSM303D_AXIS_Z] = (s16)((buf[LSM303D_AXIS_Z*2]) |
		         (buf[LSM303D_AXIS_Z*2+1] << 8));
	  }	
       #endif


       #ifdef SW_CALIBRATION
		mul = 0;//only SW Calibration, disable HW Calibration	 
	#else
        if ((err = lsm303d_ReadOffset(client, obj->offset))) {
        GSE_ERR("read offset fail, %d\n", err);
        return err;
        }    
        mul = obj->reso.sensitivity/lsm303d_offset_resolution.sensitivity;
       #endif 

   dat[obj->cvt.map[LSM303D_AXIS_X]] = obj->cvt.sign[LSM303D_AXIS_X]*(obj->offset[LSM303D_AXIS_X]*mul + obj->cali_sw[LSM303D_AXIS_X]);
   dat[obj->cvt.map[LSM303D_AXIS_Y]] = obj->cvt.sign[LSM303D_AXIS_Y]*(obj->offset[LSM303D_AXIS_Y]*mul + obj->cali_sw[LSM303D_AXIS_Y]);
   dat[obj->cvt.map[LSM303D_AXIS_Z]] = obj->cvt.sign[LSM303D_AXIS_Z]*(obj->offset[LSM303D_AXIS_Z]*mul + obj->cali_sw[LSM303D_AXIS_Z]);                        

    return 0;
}


/*----------------------------------------------------------------------------*/
static int lsm303d_WriteCalibration(struct i2c_client *client, int dat[LSM303D_AXES_NUM])
{
	struct lsm303d_i2c_data *obj = i2c_get_clientdata(client);
	int err;
	int cali[LSM303D_AXES_NUM], raw[LSM303D_AXES_NUM];
	int lsb = lsm303d_offset_resolution.sensitivity;
	int divisor = obj->reso.sensitivity/lsb;
	
#define LSM303D_AXIS_X          0
#define LSM303D_AXIS_Y          1
#define LSM303D_AXIS_Z          2

	if((err = lsm303d_ReadCalibrationEx(client, cali, raw)))	/*offset will be updated in obj->offset*/
	{ 
		GSE_ERR("read offset fail, %d\n", err);
		return err;
	}

	GSE_LOG("OLDOFF: (%+3d %+3d %+3d): (%+3d %+3d %+3d) / (%+3d %+3d %+3d)\n", 
		raw[LSM303D_AXIS_X], raw[LSM303D_AXIS_Y], raw[LSM303D_AXIS_Z],
		obj->offset[LSM303D_AXIS_X], obj->offset[LSM303D_AXIS_Y], obj->offset[LSM303D_AXIS_Z],
		obj->cali_sw[LSM303D_AXIS_X], obj->cali_sw[LSM303D_AXIS_Y], obj->cali_sw[LSM303D_AXIS_Z]);

	/*calculate the real offset expected by caller*/
	cali[LSM303D_AXIS_X] += dat[LSM303D_AXIS_X];
	cali[LSM303D_AXIS_Y] += dat[LSM303D_AXIS_Y];
	cali[LSM303D_AXIS_Z] += dat[LSM303D_AXIS_Z];

	GSE_LOG("UPDATE: (%+3d %+3d %+3d)\n", 
		dat[LSM303D_AXIS_X], dat[LSM303D_AXIS_Y], dat[LSM303D_AXIS_Z]);

#ifdef SW_CALIBRATION
	obj->cali_sw[LSM303D_AXIS_X] = obj->cvt.sign[LSM303D_AXIS_X]*(cali[obj->cvt.map[LSM303D_AXIS_X]]);
	obj->cali_sw[LSM303D_AXIS_Y] = obj->cvt.sign[LSM303D_AXIS_Y]*(cali[obj->cvt.map[LSM303D_AXIS_Y]]);
	obj->cali_sw[LSM303D_AXIS_Z] = obj->cvt.sign[LSM303D_AXIS_Z]*(cali[obj->cvt.map[LSM303D_AXIS_Z]]);	

	GSE_LOG("NEWOFF: (%+3d %+3d %+3d): (%+3d %+3d %+3d) / (%+3d %+3d %+3d)\n", 
		obj->offset[LSM303D_AXIS_X]*divisor + obj->cali_sw[LSM303D_AXIS_X], 
		obj->offset[LSM303D_AXIS_Y]*divisor + obj->cali_sw[LSM303D_AXIS_Y], 
		obj->offset[LSM303D_AXIS_Z]*divisor + obj->cali_sw[LSM303D_AXIS_Z], 
		obj->offset[LSM303D_AXIS_X], obj->offset[LSM303D_AXIS_Y], obj->offset[LSM303D_AXIS_Z],
		obj->cali_sw[LSM303D_AXIS_X], obj->cali_sw[LSM303D_AXIS_Y], obj->cali_sw[LSM303D_AXIS_Z]);
#else
	obj->offset[LSM303D_AXIS_X] = (s8)(obj->cvt.sign[LSM303D_AXIS_X]*(cali[obj->cvt.map[LSM303D_AXIS_X]])/(divisor));
	obj->offset[LSM303D_AXIS_Y] = (s8)(obj->cvt.sign[LSM303D_AXIS_Y]*(cali[obj->cvt.map[LSM303D_AXIS_Y]])/(divisor));
	obj->offset[LSM303D_AXIS_Z] = (s8)(obj->cvt.sign[LSM303D_AXIS_Z]*(cali[obj->cvt.map[LSM303D_AXIS_Z]])/(divisor));

	/*convert software calibration using standard calibration*/
	obj->cali_sw[LSM303D_AXIS_X] = obj->cvt.sign[LSM303D_AXIS_X]*(cali[obj->cvt.map[LSM303D_AXIS_X]])%(divisor);
	obj->cali_sw[LSM303D_AXIS_Y] = obj->cvt.sign[LSM303D_AXIS_Y]*(cali[obj->cvt.map[LSM303D_AXIS_Y]])%(divisor);
	obj->cali_sw[LSM303D_AXIS_Z] = obj->cvt.sign[LSM303D_AXIS_Z]*(cali[obj->cvt.map[LSM303D_AXIS_Z]])%(divisor);

	GSE_LOG("NEWOFF: (%+3d %+3d %+3d): (%+3d %+3d %+3d) / (%+3d %+3d %+3d)\n", 
		obj->offset[LSM303D_AXIS_X]*divisor + obj->cali_sw[LSM303D_AXIS_X], 
		obj->offset[LSM303D_AXIS_Y]*divisor + obj->cali_sw[LSM303D_AXIS_Y], 
		obj->offset[LSM303D_AXIS_Z]*divisor + obj->cali_sw[LSM303D_AXIS_Z], 
		obj->offset[LSM303D_AXIS_X], obj->offset[LSM303D_AXIS_Y], obj->offset[LSM303D_AXIS_Z],
		obj->cali_sw[LSM303D_AXIS_X], obj->cali_sw[LSM303D_AXIS_Y], obj->cali_sw[LSM303D_AXIS_Z]);

	if((err = hwmsen_write_block(obj->client, MC3210_REG_OFSX, obj->offset, MC3210_AXES_NUM)))
	{
		GSE_ERR("write offset fail: %d\n", err);
		return err;
	}
#endif

	return err;
}


static int lsm303d_init_client(struct i2c_client *client, int reset_cali)
{
	struct lsm303d_i2c_data *obj = i2c_get_clientdata(client);
	int res = 0;

	GSE_FUN();

	// 1 check ID  ok
	res = lsm303d_CheckDeviceID(client); 
	if(res != LSM303D_SUCCESS)
	{
	    GSE_ERR("Check ID error\n");
		return res;
	}	

	// 2 POWER MODE  YES
	res = lsm303d_SetPowerMode(client, false);
	if(res != LSM303D_SUCCESS)
	{
	    GSE_ERR("set power error\n");
		return res;
	}
	
	// 3 RATE  ok
	res = lsm303d_SetBWRate(client, LSM303D_ACC_ODR_OFF);
	if(res != LSM303D_SUCCESS ) //0x2C->BW=100Hz
	{
	    GSE_ERR("set power error\n");
		return res;
	}

	// 4 RANGE  ok
	res = lsm303d_SetDataFormat(client, LSM303D_ACC_FS_2G);
	if(res != LSM303D_SUCCESS) //0x2C->BW=100Hz
	{
	    GSE_ERR("set data format error\n");
		return res;
	}

	// 5 GAIN  ?
	gsensor_gain.x = gsensor_gain.y = gsensor_gain.z = obj->reso.sensitivity;

	//lsm303d_SetFilterLen(client, FILTER_194);

	// 6 eint diable
	/***
	res = LSM303D_SetIntEnable(client, 0x00);   //disable INT        
	if(res != LSM303D_SUCCESS) 
	{
	    GSE_ERR("LSM303D_SetIntEnable error\n");
		return res;
	}
	***/
	// 7 cali
	if(0 != reset_cali)
	{ 
		/*reset calibration only in power on*/
		res = lsm303d_ResetCalibration(client);
		if(res != LSM303D_SUCCESS)
		{
			return res;
		}
	}

#ifdef CONFIG_LSM303D_LOWPASS
	memset(&obj->fir, 0x00, sizeof(obj->fir));  
#endif

	return LSM303D_SUCCESS;
}

/*----------------------------------------------------------------------------*/
static int lsm303d_ReadData(struct i2c_client *client, s32 data[LSM303D_AXES_NUM])
{
	struct lsm303d_i2c_data *priv = i2c_get_clientdata(client);        
	u8 addr = LSM303D_REG_DATAX0 | I2C_AUTO_INCREMENT;
	u8 buf[LSM303D_DATA_LEN] = {0};
	int err = 0;
	GSE_FUN();

	if(NULL == client)
	{
		err = -EINVAL;
	}
	else if((err = hwmsen_read_block(client, addr, buf, 0x06)))
	{
		GSE_ERR("error: %d\n", err);
	}
	else
	{
		if(atomic_read(&priv->trace) & ADX_TRC_RAWDATA)
		{
			GSE_LOG("transfer before [%08X %08X %08X] => [%08X %08X %08X]\n", buf[LSM303D_AXIS_X*2],buf[LSM303D_AXIS_X*2+1], 
						buf[LSM303D_AXIS_Y*2], buf[LSM303D_AXIS_Y*2+1], buf[LSM303D_AXIS_Z*2], buf[LSM303D_AXIS_Z*2+1]);
		}

		data[LSM303D_AXIS_X] = ((s32)( (s16)((buf[LSM303D_AXIS_X*2+1] << 8) | (buf[LSM303D_AXIS_X*2]))));
		data[LSM303D_AXIS_Y] = ((s32)( (s16)((buf[LSM303D_AXIS_Y*2+1] << 8) | (buf[LSM303D_AXIS_Y*2]))));
		data[LSM303D_AXIS_Z] = ((s32)( (s16)((buf[LSM303D_AXIS_Z*2+1] << 8) | (buf[LSM303D_AXIS_Z*2]))));
		
		

	/******
		data[LSM303D_AXIS_X] = (s16)((buf[LSM303D_AXIS_X*2]) |
		         (buf[LSM303D_AXIS_X*2+1] << 8));
		data[LSM303D_AXIS_Y] = (s16)((buf[LSM303D_AXIS_Y*2]) |
		         (buf[LSM303D_AXIS_Y*2+1] << 8));
		data[LSM303D_AXIS_Z] = (s16)((buf[LSM303D_AXIS_Z*2]) |
		         (buf[LSM303D_AXIS_Z*2+1] << 8));

		         ***/
		/***
		data[LSM303D_AXIS_X] += priv->cali_sw[LSM303D_AXIS_X];
		data[LSM303D_AXIS_Y] += priv->cali_sw[LSM303D_AXIS_Y];
		data[LSM303D_AXIS_Z] += priv->cali_sw[LSM303D_AXIS_Z];
		***/
		if(atomic_read(&priv->trace) & ADX_TRC_RAWDATA)
		{
			GSE_LOG("[%08X %08X %08X] => [%5d %5d %5d]\n", data[LSM303D_AXIS_X], data[LSM303D_AXIS_Y], data[LSM303D_AXIS_Z],
		                               data[LSM303D_AXIS_X], data[LSM303D_AXIS_Y], data[LSM303D_AXIS_Z]);
		}
#ifdef CONFIG_LSM303D_LOWPASS
		if(atomic_read(&priv->filter))
		{
			if(atomic_read(&priv->fir_en) && !atomic_read(&priv->suspend))
			{
				int idx, firlen = atomic_read(&priv->firlen);   
				if(priv->fir.num < firlen)
				{                
					priv->fir.raw[priv->fir.num][LSM303D_AXIS_X] = data[LSM303D_AXIS_X];
					priv->fir.raw[priv->fir.num][LSM303D_AXIS_Y] = data[LSM303D_AXIS_Y];
					priv->fir.raw[priv->fir.num][LSM303D_AXIS_Z] = data[LSM303D_AXIS_Z];
					priv->fir.sum[LSM303D_AXIS_X] += data[LSM303D_AXIS_X];
					priv->fir.sum[LSM303D_AXIS_Y] += data[LSM303D_AXIS_Y];
					priv->fir.sum[LSM303D_AXIS_Z] += data[LSM303D_AXIS_Z];
					if(atomic_read(&priv->trace) & ADX_TRC_FILTER)
					{
						GSE_LOG("add [%2d] [%5d %5d %5d] => [%5d %5d %5d]\n", priv->fir.num,
							priv->fir.raw[priv->fir.num][LSM303D_AXIS_X], priv->fir.raw[priv->fir.num][LSM303D_AXIS_Y], priv->fir.raw[priv->fir.num][LSM303D_AXIS_Z],
							priv->fir.sum[LSM303D_AXIS_X], priv->fir.sum[LSM303D_AXIS_Y], priv->fir.sum[LSM303D_AXIS_Z]);
					}
					priv->fir.num++;
					priv->fir.idx++;
				}
				else
				{
					idx = priv->fir.idx % firlen;
					priv->fir.sum[LSM303D_AXIS_X] -= priv->fir.raw[idx][LSM303D_AXIS_X];
					priv->fir.sum[LSM303D_AXIS_Y] -= priv->fir.raw[idx][LSM303D_AXIS_Y];
					priv->fir.sum[LSM303D_AXIS_Z] -= priv->fir.raw[idx][LSM303D_AXIS_Z];
					priv->fir.raw[idx][LSM303D_AXIS_X] = data[LSM303D_AXIS_X];
					priv->fir.raw[idx][LSM303D_AXIS_Y] = data[LSM303D_AXIS_Y];
					priv->fir.raw[idx][LSM303D_AXIS_Z] = data[LSM303D_AXIS_Z];
					priv->fir.sum[LSM303D_AXIS_X] += data[LSM303D_AXIS_X];
					priv->fir.sum[LSM303D_AXIS_Y] += data[LSM303D_AXIS_Y];
					priv->fir.sum[LSM303D_AXIS_Z] += data[LSM303D_AXIS_Z];
					priv->fir.idx++;
					data[LSM303D_AXIS_X] = priv->fir.sum[LSM303D_AXIS_X]/firlen;
					data[LSM303D_AXIS_Y] = priv->fir.sum[LSM303D_AXIS_Y]/firlen;
					data[LSM303D_AXIS_Z] = priv->fir.sum[LSM303D_AXIS_Z]/firlen;
					if(atomic_read(&priv->trace) & ADX_TRC_FILTER)
					{
						GSE_LOG("add [%2d] [%5d %5d %5d] => [%5d %5d %5d] : [%5d %5d %5d]\n", idx,
						priv->fir.raw[idx][LSM303D_AXIS_X], priv->fir.raw[idx][LSM303D_AXIS_Y], priv->fir.raw[idx][LSM303D_AXIS_Z],
						priv->fir.sum[LSM303D_AXIS_X], priv->fir.sum[LSM303D_AXIS_Y], priv->fir.sum[LSM303D_AXIS_Z],
						data[LSM303D_AXIS_X], data[LSM303D_AXIS_Y], data[LSM303D_AXIS_Z]);
					}
				}
			}
		}	
#endif         
	}
	return err;
}


/*----------------------------------------------------------------------------*/
static int lsm303d_ReadRawData(struct i2c_client *client, char *buf)
{
	struct lsm303d_i2c_data *obj = (struct lsm303d_i2c_data*)i2c_get_clientdata(client);
	int res = 0;

	if (!buf || !client)
	{
		return EINVAL;
	}
	
	if((res = lsm303d_ReadData(client, obj->data)))
	{        
		GSE_ERR("I2C error: ret value=%d", res);
		return EIO;
	}
	else
	{
		sprintf(buf, "%04x %04x %04x", obj->data[LSM303D_AXIS_X], 
			obj->data[LSM303D_AXIS_Y], obj->data[LSM303D_AXIS_Z]);
	
	}
	
	return 0;
}


/*----------------------------------------------------------------------------*/
static int lsm303d_ReadSensorData(struct i2c_client *client, char *buf, int bufsize)
{
	int acc[LSM303D_DATA_LEN];
	int res = 0;	
	struct lsm303d_i2c_data *obj = obj_i2c_data; //(struct lsm303d_i2c_data*)i2c_get_clientdata(client);
	client = obj->client;
	//u8 databuf[20];
	
	//memset(databuf, 0, sizeof(u8)*10);

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
		res = lsm303d_SetPowerMode(client, true);
		if(res)
		{
			GSE_ERR("Power on lsm303d error %d!\n", res);
		}
		msleep(20);
	}

	if((res = lsm303d_ReadData(client, obj->data)))
	{        
		GSE_ERR("I2C error: ret value=%d", res);
		return -3;
	}
	else
	{
		obj->data[LSM303D_AXIS_X] += obj->cali_sw[LSM303D_AXIS_X];
		obj->data[LSM303D_AXIS_Y] += obj->cali_sw[LSM303D_AXIS_Y];
		obj->data[LSM303D_AXIS_Z] += obj->cali_sw[LSM303D_AXIS_Z];
		
		/*remap coordinate*/
		acc[obj->cvt.map[LSM303D_AXIS_X]] = obj->cvt.sign[LSM303D_AXIS_X]*obj->data[LSM303D_AXIS_X];
		acc[obj->cvt.map[LSM303D_AXIS_Y]] = obj->cvt.sign[LSM303D_AXIS_Y]*obj->data[LSM303D_AXIS_Y];
		acc[obj->cvt.map[LSM303D_AXIS_Z]] = obj->cvt.sign[LSM303D_AXIS_Z]*obj->data[LSM303D_AXIS_Z];

		//GSE_LOG("Mapped gsensor data: %d, %d, %d!\n", acc[LSM303D_AXIS_X], acc[LSM303D_AXIS_Y], acc[LSM303D_AXIS_Z]);

		//Out put the mg
		acc[LSM303D_AXIS_X] = acc[LSM303D_AXIS_X] * GRAVITY_EARTH_1000 / obj->reso.sensitivity;
		acc[LSM303D_AXIS_Y] = acc[LSM303D_AXIS_Y] * GRAVITY_EARTH_1000 / obj->reso.sensitivity;
		acc[LSM303D_AXIS_Z] = acc[LSM303D_AXIS_Z] * GRAVITY_EARTH_1000 / obj->reso.sensitivity;		
		

		sprintf(buf, "%04x %04x %04x", acc[LSM303D_AXIS_X], acc[LSM303D_AXIS_Y], acc[LSM303D_AXIS_Z]);
		if(atomic_read(&obj->trace) & ADX_TRC_IOCTL)
		{
			GSE_LOG("gsensor data: %s!\n", buf);
		}
	}
	
	return 0;
}



/*----------------------------------------------------------------------------*/
static int lsm303d_InitSelfTest(struct i2c_client *client)
{
	int res = 0;
	u8  data;

	res = lsm303d_SetBWRate(client, LSM303D_ACC_ODR100);
	if(res != LSM303D_SUCCESS ) //0x2C->BW=100Hz
	{
		return res;
	}
	
	res = hwmsen_read_byte(client, LSM303D_REG_DATA_FORMAT, &data);
	if(res != LSM303D_SUCCESS)
	{
		return res;
	}
	//res = lsm303d_SetDataFormat(client, LSM303D_SELF_TEST|data);

	res = lsm303d_SetDataFormat(client, LSM303D_ACC_FS_2G);
	if(res != LSM303D_SUCCESS) //0x2C->BW=100Hz
	{
		return res;
	}
	
	return LSM303D_SUCCESS;
}
/*----------------------------------------------------------------------------*/
static int lsm303d_JudgeTestResult(struct i2c_client *client, s32 prv[LSM303D_AXES_NUM], s32 nxt[LSM303D_AXES_NUM])
{
/**
    struct criteria {
        int min;
        int max;
    };
	
    struct criteria self[4][3] = {
        {{50, 540}, {-540, -50}, {75, 875}},
        {{25, 270}, {-270, -25}, {38, 438}},
        {{12, 135}, {-135, -12}, {19, 219}},            
        {{ 6,  67}, {-67,  -6},  {10, 110}},            
    };
    struct criteria (*ptr)[3] = NULL;
    u8 format;
    int res;
    if((res = hwmsen_read_byte(client, LSM303D_REG_DATA_FORMAT, &format)))
        return res;
    if(format & LSM303D_FULL_RES)
        ptr = &self[0];
    else if ((format & LSM303D_RANGE_4G))
        ptr = &self[1];
    else if ((format & LSM303D_RANGE_8G))
        ptr = &self[2];
    else if ((format & LSM303D_RANGE_16G))
        ptr = &self[3];

    if (!ptr) {
        GSE_ERR("null pointer\n");
        return -EINVAL;
    }
	GSE_LOG("format=%x\n",format);

    if (((nxt[LSM303D_AXIS_X] - prv[LSM303D_AXIS_X]) > (*ptr)[LSM303D_AXIS_X].max) ||
        ((nxt[LSM303D_AXIS_X] - prv[LSM303D_AXIS_X]) < (*ptr)[LSM303D_AXIS_X].min)) {
        GSE_ERR("X is over range\n");
        res = -EINVAL;
    }
    if (((nxt[LSM303D_AXIS_Y] - prv[LSM303D_AXIS_Y]) > (*ptr)[LSM303D_AXIS_Y].max) ||
        ((nxt[LSM303D_AXIS_Y] - prv[LSM303D_AXIS_Y]) < (*ptr)[LSM303D_AXIS_Y].min)) {
        GSE_ERR("Y is over range\n");
        res = -EINVAL;
    }
    if (((nxt[LSM303D_AXIS_Z] - prv[LSM303D_AXIS_Z]) > (*ptr)[LSM303D_AXIS_Z].max) ||
        ((nxt[LSM303D_AXIS_Z] - prv[LSM303D_AXIS_Z]) < (*ptr)[LSM303D_AXIS_Z].min)) {
        GSE_ERR("Z is over range\n");
        res = -EINVAL;
    }
    ***/
    int res = 0;
    return res;
}
/*----------------------------------------------------------------------------*/
static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = lsm303d_i2c_client;
	char strbuf[LSM303D_BUFSIZE];
	if(NULL == client)
	{
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}
	
	lsm303d_ReadChipInfo(client, strbuf, LSM303D_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);        
}
/*----------------------------------------------------------------------------*/
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = lsm303d_i2c_client;
	char strbuf[LSM303D_BUFSIZE];
	
	if(NULL == client)
	{
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}
	lsm303d_ReadSensorData(client, strbuf, LSM303D_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);            
}
/*----------------------------------------------------------------------------*/
static ssize_t show_cali_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = lsm303d_i2c_client;
	struct lsm303d_i2c_data *obj;
	int err, len = 0, mul;
	int tmp[LSM303D_AXES_NUM];

	if(NULL == client)
	{
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	obj = i2c_get_clientdata(client);



	if((err = lsm303d_ReadOffset(client, obj->offset)))
	{
		return -EINVAL;
	}
	else if((err = lsm303d_ReadCalibration(client, tmp)))
	{
		return -EINVAL;
	}
	else
	{    
		mul = obj->reso.sensitivity/lsm303d_offset_resolution.sensitivity;
		len += snprintf(buf+len, PAGE_SIZE-len, "[HW ][%d] (%+3d, %+3d, %+3d) : (0x%02X, 0x%02X, 0x%02X)\n", mul,                        
			obj->offset[LSM303D_AXIS_X], obj->offset[LSM303D_AXIS_Y], obj->offset[LSM303D_AXIS_Z],
			obj->offset[LSM303D_AXIS_X], obj->offset[LSM303D_AXIS_Y], obj->offset[LSM303D_AXIS_Z]);
		len += snprintf(buf+len, PAGE_SIZE-len, "[SW ][%d] (%+3d, %+3d, %+3d)\n", 1, 
			obj->cali_sw[LSM303D_AXIS_X], obj->cali_sw[LSM303D_AXIS_Y], obj->cali_sw[LSM303D_AXIS_Z]);

		len += snprintf(buf+len, PAGE_SIZE-len, "[ALL]    (%+3d, %+3d, %+3d) : (%+3d, %+3d, %+3d)\n", 
			obj->offset[LSM303D_AXIS_X]*mul + obj->cali_sw[LSM303D_AXIS_X],
			obj->offset[LSM303D_AXIS_Y]*mul + obj->cali_sw[LSM303D_AXIS_Y],
			obj->offset[LSM303D_AXIS_Z]*mul + obj->cali_sw[LSM303D_AXIS_Z],
			tmp[LSM303D_AXIS_X], tmp[LSM303D_AXIS_Y], tmp[LSM303D_AXIS_Z]);
		
		return len;
    }
}
/*----------------------------------------------------------------------------*/
static ssize_t store_cali_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct i2c_client *client = lsm303d_i2c_client;  
	int err, x, y, z;
	int dat[LSM303D_AXES_NUM];

	if(!strncmp(buf, "rst", 3))
	{
		if((err = lsm303d_ResetCalibration(client)))
		{
			GSE_ERR("reset offset err = %d\n", err);
		}	
	}
	else if(3 == sscanf(buf, "0x%02X 0x%02X 0x%02X", &x, &y, &z))
	{
		dat[LSM303D_AXIS_X] = x;
		dat[LSM303D_AXIS_Y] = y;
		dat[LSM303D_AXIS_Z] = z;
		if((err = lsm303d_WriteCalibration(client, dat)))
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
/*---------------------------------  NO -------------------------------------------*/
static ssize_t show_self_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = lsm303d_i2c_client;
	//struct lsm303d_i2c_data *obj;

	if(NULL == client)
	{
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	//obj = i2c_get_clientdata(client);

    return snprintf(buf, 8, "%s\n", selftestRes);
}
/*-------------------------------------  NO ---------------------------------------*/
static ssize_t store_self_value(struct device_driver *ddri, const char *buf, size_t count)
{   /*write anything to this register will trigger the process*/
	struct item{
	s16 raw[LSM303D_AXES_NUM];
	};
	
	struct i2c_client *client = lsm303d_i2c_client;  
	int idx, res, num;
	struct item *prv = NULL, *nxt = NULL;
	s32 avg_prv[LSM303D_AXES_NUM] = {0, 0, 0};
	s32 avg_nxt[LSM303D_AXES_NUM] = {0, 0, 0};


	if(1 != sscanf(buf, "%d", &num))
	{
		GSE_ERR("parse number fail\n");
		return count;
	}
	else if(num == 0)
	{
		GSE_ERR("invalid data count\n");
		return count;
	}

	prv = kzalloc(sizeof(*prv) * num, GFP_KERNEL);
	nxt = kzalloc(sizeof(*nxt) * num, GFP_KERNEL);
	if (!prv || !nxt)
	{
		goto exit;
	}


	GSE_LOG("NORMAL:\n");
	lsm303d_SetPowerMode(client,true);
	for(idx = 0; idx < num; idx++)
	{
		if((res = lsm303d_ReadData(client, prv[idx].raw)))
		{            
			GSE_ERR("read data fail: %d\n", res);
			goto exit;
		}
		
		avg_prv[LSM303D_AXIS_X] += prv[idx].raw[LSM303D_AXIS_X];
		avg_prv[LSM303D_AXIS_Y] += prv[idx].raw[LSM303D_AXIS_Y];
		avg_prv[LSM303D_AXIS_Z] += prv[idx].raw[LSM303D_AXIS_Z];        
		GSE_LOG("[%5d %5d %5d]\n", prv[idx].raw[LSM303D_AXIS_X], prv[idx].raw[LSM303D_AXIS_Y], prv[idx].raw[LSM303D_AXIS_Z]);
	}
	
	avg_prv[LSM303D_AXIS_X] /= num;
	avg_prv[LSM303D_AXIS_Y] /= num;
	avg_prv[LSM303D_AXIS_Z] /= num;    

	//initial setting for self test
	lsm303d_InitSelfTest(client);
	GSE_LOG("SELFTEST:\n");    
	for(idx = 0; idx < num; idx++)
	{
		if((res = lsm303d_ReadData(client, nxt[idx].raw)))
		{            
			GSE_ERR("read data fail: %d\n", res);
			goto exit;
		}
		avg_nxt[LSM303D_AXIS_X] += nxt[idx].raw[LSM303D_AXIS_X];
		avg_nxt[LSM303D_AXIS_Y] += nxt[idx].raw[LSM303D_AXIS_Y];
		avg_nxt[LSM303D_AXIS_Z] += nxt[idx].raw[LSM303D_AXIS_Z];        
		GSE_LOG("[%5d %5d %5d]\n", nxt[idx].raw[LSM303D_AXIS_X], nxt[idx].raw[LSM303D_AXIS_Y], nxt[idx].raw[LSM303D_AXIS_Z]);
	}
	
	avg_nxt[LSM303D_AXIS_X] /= num;
	avg_nxt[LSM303D_AXIS_Y] /= num;
	avg_nxt[LSM303D_AXIS_Z] /= num;    

	GSE_LOG("X: %5d - %5d = %5d \n", avg_nxt[LSM303D_AXIS_X], avg_prv[LSM303D_AXIS_X], avg_nxt[LSM303D_AXIS_X] - avg_prv[LSM303D_AXIS_X]);
	GSE_LOG("Y: %5d - %5d = %5d \n", avg_nxt[LSM303D_AXIS_Y], avg_prv[LSM303D_AXIS_Y], avg_nxt[LSM303D_AXIS_Y] - avg_prv[LSM303D_AXIS_Y]);
	GSE_LOG("Z: %5d - %5d = %5d \n", avg_nxt[LSM303D_AXIS_Z], avg_prv[LSM303D_AXIS_Z], avg_nxt[LSM303D_AXIS_Z] - avg_prv[LSM303D_AXIS_Z]); 

	if(!lsm303d_JudgeTestResult(client, avg_prv, avg_nxt))
	{
		GSE_LOG("SELFTEST : PASS\n");
		strcpy(selftestRes,"y");
	}	
	else
	{
		GSE_LOG("SELFTEST : FAIL\n");
		strcpy(selftestRes,"n");
	}
	
	exit:
	//restore the setting    
	lsm303d_init_client(client, 0);
	kfree(prv);
	kfree(nxt);
	return count;
}
/*---------------------------------  NO -------------------------------------------*/
static ssize_t show_selftest_value(struct device_driver *ddri, char *buffer)
{
	struct i2c_client *client = lsm303d_i2c_client;
	
		struct lsm303d_i2c_data *priv = i2c_get_clientdata(client); 	   
		u8 addr = LSM303D_REG_CTL0 | I2C_AUTO_INCREMENT;
		u8 buf[8] = {0};
		int err = 0;
		ssize_t len = 0;
		
	
		if(NULL == client)
		{
			err = -EINVAL;
		}
		else if((err = hwmsen_read_block(client, addr, buf, 0x08)))
		{
			GSE_ERR("error: %d\n", err);
		}
	
		len += snprintf(buffer+len, PAGE_SIZE, "0x%04X , \t 0x%04X , \t 0x%04X, \t0x%04X ,   \n  0x%04X , \t  0x%04X, \t0x%04X,  \t	0x%04X ,  \t  \n ", 
						buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]); 

		

		return len;

/***

	struct i2c_client *client = lsm303d_i2c_client;
	struct lsm303d_i2c_data *obj;

	if(NULL == client)
	{
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	obj = i2c_get_clientdata(client);
	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&obj->selftest));
	**/
}
/*--------------------------------  NO --------------------------------------------*/
static ssize_t store_selftest_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct lsm303d_i2c_data *obj = obj_i2c_data;
	int tmp;

	if(NULL == obj)
	{
		GSE_ERR("i2c data obj is null!!\n");
		return 0;
	}
	
	
	if(1 == sscanf(buf, "%d", &tmp))
	{        
		if(atomic_read(&obj->selftest) && !tmp)
		{
			/*enable -> disable*/
			lsm303d_init_client(obj->client, 0);
		}
		else if(!atomic_read(&obj->selftest) && tmp)
		{
			/*disable -> enable*/
			lsm303d_InitSelfTest(obj->client);            
		}
		
		GSE_LOG("selftest: %d => %d\n", atomic_read(&obj->selftest), tmp);
		atomic_set(&obj->selftest, tmp); 
	}
	else
	{ 
		GSE_ERR("invalid content: '%s', length = %d\n", buf, count);   
	}
	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t show_firlen_value(struct device_driver *ddri, char *buf)
{
#ifdef CONFIG_LSM303D_LOWPASS
	struct i2c_client *client = lsm303d_i2c_client;
	struct lsm303d_i2c_data *obj = i2c_get_clientdata(client);
	if(atomic_read(&obj->firlen))
	{
		int idx, len = atomic_read(&obj->firlen);
		GSE_LOG("len = %2d, idx = %2d\n", obj->fir.num, obj->fir.idx);

		for(idx = 0; idx < len; idx++)
		{
			GSE_LOG("[%5d %5d %5d]\n", obj->fir.raw[idx][LSM303D_AXIS_X], obj->fir.raw[idx][LSM303D_AXIS_Y], obj->fir.raw[idx][LSM303D_AXIS_Z]);
		}
		
		GSE_LOG("sum = [%5d %5d %5d]\n", obj->fir.sum[LSM303D_AXIS_X], obj->fir.sum[LSM303D_AXIS_Y], obj->fir.sum[LSM303D_AXIS_Z]);
		GSE_LOG("avg = [%5d %5d %5d]\n", obj->fir.sum[LSM303D_AXIS_X]/len, obj->fir.sum[LSM303D_AXIS_Y]/len, obj->fir.sum[LSM303D_AXIS_Z]/len);
	}
	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&obj->firlen));
#else
	return snprintf(buf, PAGE_SIZE, "not support\n");
#endif
}
/*----------------------------------------------------------------------------*/
static ssize_t store_firlen_value(struct device_driver *ddri, const char *buf, size_t count)
{
#ifdef CONFIG_LSM303D_LOWPASS
	struct i2c_client *client = lsm303d_i2c_client;  
	struct lsm303d_i2c_data *obj = i2c_get_clientdata(client);
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
	struct lsm303d_i2c_data *obj = obj_i2c_data;
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
	struct lsm303d_i2c_data *obj = obj_i2c_data;
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
static ssize_t show_status_value(struct device_driver *ddri, char *buffer)
{
	struct i2c_client *client = lsm303d_i2c_client;

	struct lsm303d_i2c_data *priv = i2c_get_clientdata(client);        
	u8 addr = LSM303D_REG_CTL0 | I2C_AUTO_INCREMENT;
	u8 buf[8] = {0};
	int err = 0;
	ssize_t len = 0;
	

	if(NULL == client)
	{
		err = -EINVAL;
	}
	else if((err = hwmsen_read_block(client, addr, buf, 0x08)))
	{
		GSE_ERR("error: %d\n", err);
	}

	len += snprintf(buffer+len, PAGE_SIZE, "0x%04X ,¡\\t 0x%04X ,¡\\t 0x%04X , \t0x%04X,   \n  0x%04X ,¡\\t  0x%04X ,¡\\t0x%04X ,\t  0x%04X , \t  \n ", buf[0],	buf[1],	buf[2],	buf[3],	buf[4],	buf[5],	buf[6],	buf[7]); 

	
	

	    
	struct lsm303d_i2c_data *obj = obj_i2c_data;
	if (obj == NULL)
	{
		GSE_ERR("i2c_data obj is null!!\n");
		return 0;
	}

	
	
	if(obj->hw)
	{
		len += snprintf(buffer+len, PAGE_SIZE-len, "CUST: %d %d (%d %d)\n", 
	            obj->hw->i2c_num, obj->hw->direction, obj->hw->power_id, obj->hw->power_vol);   
	}
	else
	{
		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: NULL\n");
	}
	return len;    
}

static ssize_t show_power_status_value(struct device_driver *ddri, char *buf)
{
	int relv = 0;
	if(sensor_power)
		relv = snprintf(buf, PAGE_SIZE, "1\n"); 
	else
		relv = snprintf(buf, PAGE_SIZE, "0\n"); 

	return relv;
}

/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(chipinfo,             S_IRUGO, show_chipinfo_value,      NULL);
static DRIVER_ATTR(sensordata,           S_IRUGO, show_sensordata_value,    NULL);
static DRIVER_ATTR(cali,       S_IWUSR | S_IRUGO, show_cali_value,          store_cali_value);
static DRIVER_ATTR(self,       S_IWUSR | S_IRUGO, show_selftest_value,          store_selftest_value);
static DRIVER_ATTR(selftest,   S_IWUSR | S_IRUGO, show_self_value ,      store_self_value );
static DRIVER_ATTR(firlen,     S_IWUSR | S_IRUGO, show_firlen_value,        store_firlen_value);
static DRIVER_ATTR(trace,      S_IWUSR | S_IRUGO, show_trace_value,         store_trace_value);
static DRIVER_ATTR(status,               S_IRUGO, show_status_value,        NULL);
static DRIVER_ATTR(powerstatus,          S_IRUGO, show_power_status_value,        NULL);

/*----------------------------------------------------------------------------*/
static struct driver_attribute *lsm303d_attr_list[] = {
	&driver_attr_chipinfo,     /*chip information*/
	&driver_attr_sensordata,   /*dump sensor data*/
	&driver_attr_cali,         /*show calibration data*/
	&driver_attr_self,         /*self test demo*/
	&driver_attr_selftest,     /*self control: 0: disable, 1: enable*/
	&driver_attr_firlen,       /*filter length: 0: disable, others: enable*/
	&driver_attr_trace,        /*trace log*/
	&driver_attr_status,
	&driver_attr_powerstatus,        
};


/*----------------------------------------------------------------------------*/
static int lsm303d_create_attr(struct device_driver *driver) 
{
	int idx, err = 0;
	int num = (int)(sizeof(lsm303d_attr_list)/sizeof(lsm303d_attr_list[0]));
	if (driver == NULL)
	{
		return -EINVAL;
	}

	for(idx = 0; idx < num; idx++)
	{
		if((err = driver_create_file(driver, lsm303d_attr_list[idx])))
		{            
			GSE_ERR("driver_create_file (%s) = %d\n", lsm303d_attr_list[idx]->attr.name, err);
			break;
		}
	}    
	return err;
}
/*----------------------------------------------------------------------------*/
static int lsm303d_delete_attr(struct device_driver *driver)
{
	int idx ,err = 0;
	int num = (int)(sizeof(lsm303d_attr_list)/sizeof(lsm303d_attr_list[0]));

	if(driver == NULL)
	{
		return -EINVAL;
	}
	

	for(idx = 0; idx < num; idx++)
	{
		driver_remove_file(driver, lsm303d_attr_list[idx]);
	}
	

	return err;
}



/*----------------------------------------------------------------------------*/
int gsensor_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value, sample_delay;	
	struct lsm303d_i2c_data *priv = (struct lsm303d_i2c_data*)self;
	hwm_sensor_data* gsensor_data;
	char buff[LSM303D_BUFSIZE];
	
	GSE_FUN(f);
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
				GSE_ERR("Set delay parameter %d !\n", value);
				if(value <= 5)
				{
					sample_delay = LSM303D_ACC_ODR100;
				}
				else if(value <= 10)
				{
					sample_delay = LSM303D_ACC_ODR100;
				}
				else
				{
					sample_delay = LSM303D_ACC_ODR50;
				}
				
				err = lsm303d_SetBWRate(priv->client, sample_delay);
				if(err != LSM303D_SUCCESS ) //0x2C->BW=100Hz
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
					priv->fir.sum[LSM303D_AXIS_X] = 0;
					priv->fir.sum[LSM303D_AXIS_Y] = 0;
					priv->fir.sum[LSM303D_AXIS_Z] = 0;
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
				
				if(value == 0) 
				{
					err = lsm303d_SetPowerMode( priv->client, 0);
					GSE_LOG("Gsensor device false!\n");
				}
				else
				{
					err = lsm303d_SetPowerMode( priv->client, 1);
					GSE_LOG("Gsensor device true!\n");
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
				err = lsm303d_ReadSensorData(priv->client, buff, LSM303D_BUFSIZE);
				if(!err)
				{
				   sscanf(buff, "%x %x %x", &gsensor_data->values[0], 
					   &gsensor_data->values[1], &gsensor_data->values[2]);				
				   gsensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;				
				   gsensor_data->value_divide = 1000;
				}
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
static int lsm303d_open(struct inode *inode, struct file *file)
{
	file->private_data = lsm303d_i2c_client;

	if(file->private_data == NULL)
	{
		GSE_ERR("null pointer!!\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}



/*----------------------------------------------------------------------------*/
//static int lsm303d_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
//       unsigned long arg)
static long lsm303d_unlocked_ioctl(struct file *file, unsigned int cmd,
       unsigned long arg)
{
	struct i2c_client *client = (struct i2c_client*)file->private_data;
	struct lsm303d_i2c_data *obj = (struct lsm303d_i2c_data*)i2c_get_clientdata(client);	
	char strbuf[LSM303D_BUFSIZE];
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
			lsm303d_init_client(client, 0);			
			break;

		case GSENSOR_IOCTL_READ_CHIPINFO:
			data = (void __user *) arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}
			
			lsm303d_ReadChipInfo(client, strbuf, LSM303D_BUFSIZE);
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
			
			lsm303d_ReadSensorData(client, strbuf, LSM303D_BUFSIZE);
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

		case GSENSOR_IOCTL_READ_RAW_DATA:
			data = (void __user *) arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}
			lsm303d_ReadRawData(client, strbuf);
			if(copy_to_user(data, strbuf, strlen(strbuf)+1))
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
				cali[LSM303D_AXIS_X] = sensor_data.x * obj->reso.sensitivity / GRAVITY_EARTH_1000;
				cali[LSM303D_AXIS_Y] = sensor_data.y * obj->reso.sensitivity / GRAVITY_EARTH_1000;
				cali[LSM303D_AXIS_Z] = sensor_data.z * obj->reso.sensitivity / GRAVITY_EARTH_1000;			  
				err = lsm303d_WriteCalibration(client, cali);			 
			}
			break;

		case GSENSOR_IOCTL_CLR_CALI:
			err = lsm303d_ResetCalibration(client);
			break;

		case GSENSOR_IOCTL_GET_CALI:
			data = (void __user*)arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}
			if((err = lsm303d_ReadCalibration(client, cali)))
			{
				break;
			}
			
			sensor_data.x = cali[LSM303D_AXIS_X] * GRAVITY_EARTH_1000 / obj->reso.sensitivity;
			sensor_data.y = cali[LSM303D_AXIS_Y] * GRAVITY_EARTH_1000 / obj->reso.sensitivity;
			sensor_data.z = cali[LSM303D_AXIS_Z] * GRAVITY_EARTH_1000 / obj->reso.sensitivity;
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
static int lsm303d_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}


/*----------------------------------------------------------------------------*/
static struct file_operations lsm303d_fops = {
//	.owner = THIS_MODULE,
	.open = lsm303d_open,
	.release = lsm303d_release,
	.unlocked_ioctl = lsm303d_unlocked_ioctl,
};
/*----------------------------------------------------------------------------*/
static struct miscdevice lsm303d_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gsensor",
	.fops = &lsm303d_fops,
};


/*----------------------------------------------------------------------------*/
//#ifndef CONFIG_HAS_EARLYSUSPEND
/*----------------------------------------------------------------------------*/
static int lsm303d_suspend(struct i2c_client *client, pm_message_t msg) 
{
	struct lsm303d_i2c_data *obj = i2c_get_clientdata(client);    
	int err = 0;
	GSE_FUN();    

	if(msg.event == PM_EVENT_SUSPEND)
	{   
		if(obj == NULL)
		{
			GSE_ERR("null pointer!!\n");
			return -EINVAL;
		}
		atomic_set(&obj->suspend, 1);
		if((err = lsm303d_SetPowerMode(obj->client, false)))
		{
			GSE_ERR("write power control fail!!\n");
			return err;
		}        
		lsm303d_power(obj->hw, 0);
		GSE_LOG("lsm303d_suspend ok\n");
	}
	return err;
}
/*----------------------------------------------------------------------------*/
static int lsm303d_resume(struct i2c_client *client)
{
	struct lsm303d_i2c_data *obj = i2c_get_clientdata(client);        
	int err;
	GSE_FUN();

	if(obj == NULL)
	{
		GSE_ERR("null pointer!!\n");
		return -EINVAL;
	}

	lsm303d_power(obj->hw, 1);
	
	/**

	if((err = lsm303d_SetPowerMode(obj->client, true)))
	{
		GSE_ERR("write power control fail!!\n");
		return err;
	} 
	**/
	
	atomic_set(&obj->suspend, 0);
	GSE_LOG("lsm303d_resume ok\n");

	return 0;
}
/*----------------------------------------------------------------------------*/
//#else /*CONFIG_HAS_EARLY_SUSPEND is defined*/
/*----------------------------------------------------------------------------*/
static void lsm303d_early_suspend(struct early_suspend *h) 
{
	struct lsm303d_i2c_data *obj = container_of(h, struct lsm303d_i2c_data, early_drv);   
	int err;
	GSE_FUN();    

	if(obj == NULL)
	{
		GSE_ERR("null pointer!!\n");
		return;
	}
	atomic_set(&obj->suspend, 1); 
	/**
	if((err = lsm303d_SetPowerMode(obj->client, false)))
	{
		GSE_ERR("write power control fail!!\n");
		return;
	}
	***/

	sensor_power = false;
	
	lsm303d_power(obj->hw, 0);
}
/*----------------------------------------------------------------------------*/
static void lsm303d_late_resume(struct early_suspend *h)
{
	struct lsm303d_i2c_data *obj = container_of(h, struct lsm303d_i2c_data, early_drv);         
	int err;
	GSE_FUN();

	if(obj == NULL)
	{
		GSE_ERR("null pointer!!\n");
		return;
	}

	lsm303d_power(obj->hw, 1);

/**
	if((err = lsm303d_SetPowerMode(obj->client, true)))
	{
		GSE_ERR("write power control fail!!\n");
		return err;
	}
	***/
	atomic_set(&obj->suspend, 0);    
}


/*----------------------------------------------------------------------------*/
static int lsm303d_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	//GSE_LOG("++++++++++++++++++++++++++++++++++++lsm303d_i2c_probe!");
	struct i2c_client *new_client;
	struct lsm303d_i2c_data *obj;
	struct hwmsen_object sobj;
	int err = 0;
	GSE_FUN();

	if(!(obj = kzalloc(sizeof(*obj), GFP_KERNEL)))
	{
		err = -ENOMEM;
		goto exit;
	}
	
	memset(obj, 0, sizeof(struct lsm303d_i2c_data));

	obj->hw = get_cust_acc_hw();
	
	if((err = hwmsen_get_convert(obj->hw->direction, &obj->cvt)))
	{
		GSE_ERR("invalid direction: %d\n", obj->hw->direction);
		goto exit;
	}

	obj_i2c_data = obj;
	obj->client = client;
	new_client = obj->client;
	i2c_set_clientdata(new_client,obj);
	
	atomic_set(&obj->trace, 0);
	atomic_set(&obj->suspend, 0);
	
#ifdef CONFIG_LSM303D_LOWPASS
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

	lsm303d_i2c_client = new_client;	

	if((err = lsm303d_init_client(new_client, 1)))
	{
		goto exit_init_failed;
	}
	

	if((err = misc_register(&lsm303d_device)))
	{
		GSE_ERR("lsm303d_device register failed\n");
		goto exit_misc_device_register_failed;
	}

	if((err = lsm303d_create_attr(&lsm303d_gsensor_driver.driver)))
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
	obj->early_drv.suspend  = lsm303d_early_suspend,
	obj->early_drv.resume   = lsm303d_late_resume,    
	register_early_suspend(&obj->early_drv);
#endif 

	GSE_LOG("%s: OK\n", __func__);    
	return 0;

	exit_create_attr_failed:
	misc_deregister(&lsm303d_device);
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
static int lsm303d_i2c_remove(struct i2c_client *client)
{
	int err = 0;	
	
	if((err = lsm303d_delete_attr(&lsm303d_gsensor_driver.driver)))
	{
		GSE_ERR("lsm303d_delete_attr fail: %d\n", err);
	}
	
	if((err = misc_deregister(&lsm303d_device)))
	{
		GSE_ERR("misc_deregister fail: %d\n", err);
	}

	if((err = hwmsen_detach(ID_ACCELEROMETER)))
	    

	lsm303d_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));
	return 0;
}
/*----------------------------------------------------------------------------*/
static int lsm303d_probe(struct platform_device *pdev) 
{
	struct acc_hw *hw = get_cust_acc_hw();
	GSE_FUN();

	lsm303d_power(hw, 1);
	//lsm303d_force[0] = hw->i2c_num;//modified
	if(i2c_add_driver(&lsm303d_i2c_driver))
	{
		GSE_ERR("add driver error\n");
		return -1;
	}
	return 0;
}
/*----------------------------------------------------------------------------*/
static int lsm303d_remove(struct platform_device *pdev)
{
    struct acc_hw *hw = get_cust_acc_hw();

    GSE_FUN();    
    lsm303d_power(hw, 0);    
    i2c_del_driver(&lsm303d_i2c_driver);
    return 0;
}
/*----------------------------------------------------------------------------*/
static struct platform_driver lsm303d_gsensor_driver = {
	.probe      = lsm303d_probe,
	.remove     = lsm303d_remove,    
	.driver     = {
		.name  = "gsensor",
//		.owner = THIS_MODULE,
	}
};

/*----------------------------------------------------------------------------*/
static int __init lsm303d_init(void)
{
	GSE_FUN();
	
	struct acc_hw *hw = get_cust_acc_hw();
	GSE_LOG("%s: i2c_number=%d\n", __func__,hw->i2c_num); 
		
	i2c_register_board_info(hw->i2c_num, &i2c_lsm303d, 1);
	if(platform_driver_register(&lsm303d_gsensor_driver))
	{
		GSE_ERR("failed to register driver");
		return -ENODEV;
	}
	return 0;    
}
/*----------------------------------------------------------------------------*/
static void __exit lsm303d_exit(void)
{
	GSE_FUN();
	platform_driver_unregister(&lsm303d_gsensor_driver);
}
/*----------------------------------------------------------------------------*/
module_init(lsm303d_init);
module_exit(lsm303d_exit);
/*----------------------------------------------------------------------------*/
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("LSM303D I2C driver");
MODULE_AUTHOR("RUO.liang@mediatek.com");

