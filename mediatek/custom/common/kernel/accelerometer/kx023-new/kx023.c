/* BEGIN PN:DTS2013062101609  ,Modified by y00187129, 2013/6/21*/
/* Add Rohm G-Sensor Driver */
/* KX023 motion sensor driver
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

//#include <mach/mt_devs.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>

#define POWER_NONE_MACRO MT65XX_POWER_NONE

#include <cust_acc.h>
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include "kx023.h"
#include <linux/hwmsen_helper.h>
#include <accel.h>
/*----------------------------------------------------------------------------*/
#define I2C_DRIVERID_KX023 150
/*----------------------------------------------------------------------------*/
#define DEBUG 1
/*----------------------------------------------------------------------------*/
//#define CONFIG_KX023_LOWPASS   /*apply low pass filter on output*/       
#define SW_CALIBRATION

/*----------------------------------------------------------------------------*/
#define KX023_AXIS_X          0
#define KX023_AXIS_Y          1
#define KX023_AXIS_Z          2
#define KX023_AXES_NUM        3
#define KX023_DATA_LEN        6
#define KX023_DEV_NAME        "KX023"
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static const struct i2c_device_id kx023_i2c_id[] = {{KX023_DEV_NAME,0},{}};
static struct i2c_board_info __initdata i2c_kx023={ I2C_BOARD_INFO(KX023_DEV_NAME, (KX023_I2C_SLAVE_ADDR>>1))};
/*the adapter id will be available in customization*/
//static unsigned short kx023_force[] = {0x00, KX023_I2C_SLAVE_ADDR, I2C_CLIENT_END, I2C_CLIENT_END};
//static const unsigned short *const kx023_forces[] = { kx023_force, NULL };
//static struct i2c_client_address_data kx023_addr_data = { .forces = kx023_forces,};

/*----------------------------------------------------------------------------*/
static int kx023_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id); 
static int kx023_i2c_remove(struct i2c_client *client);
static int kx023_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);

extern struct acc_hw * kx023_get_cust_acc_hw(void);
static int kx023_local_init(void);
static int kx023_remove(void);
static int kx023_init_flag =-1; // 0<==>OK -1 <==> fail
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
    s16 raw[C_MAX_FIR_LENGTH][KX023_AXES_NUM];
    int sum[KX023_AXES_NUM];
    int num;
    int idx;
};
/*----------------------------------------------------------------------------*/
static struct sensor_init_info kx023_init_info = {
		.name = "kx023",
		.init = kx023_local_init,
		.uninit = kx023_remove,
};
/*----------------------------------------------------------------------------*/
struct kx023_i2c_data {
    struct i2c_client *client;
    struct acc_hw *hw;
    struct hwmsen_convert   cvt;
    
    /*misc*/
    struct data_resolution *reso;
    atomic_t                trace;
    atomic_t                suspend;
    atomic_t                selftest;
	atomic_t				filter;
    s16                     cali_sw[KX023_AXES_NUM+1];

    /*data*/
    s8                      offset[KX023_AXES_NUM+1];  /*+1: for 4-byte alignment*/
    s16                     data[KX023_AXES_NUM+1];

#if defined(CONFIG_KX023_LOWPASS)
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
static struct i2c_driver kx023_i2c_driver = {
    .driver = {
//        .owner          = THIS_MODULE,
        .name           = KX023_DEV_NAME,
    },
	.probe      		= kx023_i2c_probe,
	.remove    			= kx023_i2c_remove,
	.detect				= kx023_i2c_detect,
#if !defined(CONFIG_HAS_EARLYSUSPEND)    
    .suspend            = kx023_suspend,
    .resume             = kx023_resume,
#endif
	.id_table = kx023_i2c_id,
//	.address_data = &kx023_addr_data,
};

/*----------------------------------------------------------------------------*/
static struct i2c_client *kx023_i2c_client = NULL;
static struct platform_driver kx023_gsensor_driver;
static struct kx023_i2c_data *obj_i2c_data = NULL;
static bool sensor_power = true;
static GSENSOR_VECTOR3D gsensor_gain;
static char selftestRes[8]= {0}; 


/*----------------------------------------------------------------------------*/
#define GSE_TAG                  "[Gsensor] "
#define GSE_FUN(f)               printk(KERN_INFO GSE_TAG"%s\n", __FUNCTION__)
#define GSE_ERR(fmt, args...)    printk(KERN_ERR GSE_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define GSE_LOG(fmt, args...)    printk(KERN_INFO GSE_TAG fmt, ##args)
/*----------------------------------------------------------------------------*/
static struct data_resolution kx023_data_resolution[1] = {
 /* combination by {FULL_RES,RANGE}*/
    {{ 0, 1}, 16384}, // dataformat +/-2g  in 16-bit resolution;  { 3, 9} = 3.9 = (2*2*1000)/(2^12);  256 = (2^12)/(2*2)          
};
/*----------------------------------------------------------------------------*/
static struct data_resolution kx023_offset_resolution = {{15, 6}, 64};
/*----------------------------------------------------------------------------*/
static int KX023_SetPowerMode(struct i2c_client *client, bool enable);
/*--------------------KX023 power control function----------------------------------*/
static void KX023_power(struct acc_hw *hw, unsigned int on) 
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
			if(!hwPowerOn(hw->power_id, hw->power_vol, "KX023"))
			{
				GSE_ERR("power on fails!!\n");
			}
		}
		else	// power off
		{
			if (!hwPowerDown(hw->power_id, "KX023"))
			{
				GSE_ERR("power off fail!!\n");
			}			  
		}
	}
	power_on = on;    
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static int KX023_SetDataResolution(struct kx023_i2c_data *obj)
{
	int err;
	u8  databuf[2];

	KX023_SetPowerMode(obj->client, false);

	if(hwmsen_read_block(obj->client, KX023_REG_DATA_RESOLUTION, databuf, 0x01))
	{
		printk("kx023 read Dataformat failt \n");
		return KX023_ERR_I2C;
	}

	databuf[0] &= ~KX023_RANGE_DATA_RESOLUTION_MASK;
	databuf[0] |= KX023_RANGE_DATA_RESOLUTION_MASK;//16bit
	databuf[1] = databuf[0];
	databuf[0] = KX023_REG_DATA_RESOLUTION;


	err = i2c_master_send(obj->client, databuf, 0x2);

	if(err <= 0)
	{
		return KX023_ERR_I2C;
	}

	KX023_SetPowerMode(obj->client, true);

	//kx023_data_resolution[0] has been set when initialize: +/-2g  in 8-bit resolution:  15.6 mg/LSB*/   
	obj->reso = &kx023_data_resolution[0];

	return 0;
}
/*----------------------------------------------------------------------------*/
static int KX023_ReadData(struct i2c_client *client, s16 data[KX023_AXES_NUM])
{
	struct kx023_i2c_data *priv = i2c_get_clientdata(client);        
	u8 addr = KX023_REG_DATAX0;
	u8 buf[KX023_DATA_LEN] = {0};
	int err = 0;
	int i;
	//int tmp=0;
	//u8 ofs[3];



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
		data[KX023_AXIS_X] = (s16)((buf[KX023_AXIS_X*2]) |
		         (buf[KX023_AXIS_X*2+1] << 8));
		data[KX023_AXIS_Y] = (s16)((buf[KX023_AXIS_Y*2]) |
		         (buf[KX023_AXIS_Y*2+1] << 8));
		data[KX023_AXIS_Z] = (s16)((buf[KX023_AXIS_Z*2]) |
		         (buf[KX023_AXIS_Z*2+1] << 8));

		for(i=0;i<3;i++)				
		{								//because the data is store in binary complement number formation in computer system
			if ( data[i] == 0x8000 )	//so we want to calculate actual number here
				data[i]= -16384;			//10bit resolution, 512= 2^(12-1)
			else if ( data[i] & 0x8000 )//transfor format
			{							//printk("data 0 step %x \n",data[i]);
				data[i] -= 0x1; 		//printk("data 1 step %x \n",data[i]);
				data[i] = ~data[i]; 	//printk("data 2 step %x \n",data[i]);
				data[i] &= 0x7fff;		//printk("data 3 step %x \n\n",data[i]);
				data[i] = -data[i]; 	
			}
		}	


		if(atomic_read(&priv->trace) & ADX_TRC_RAWDATA)
		{
			GSE_LOG("[%08X %08X %08X] => [%5d %5d %5d]\n", data[KX023_AXIS_X], data[KX023_AXIS_Y], data[KX023_AXIS_Z],
		                               data[KX023_AXIS_X], data[KX023_AXIS_Y], data[KX023_AXIS_Z]);
		}
#ifdef CONFIG_KX023_LOWPASS
		if(atomic_read(&priv->filter))
		{
			if(atomic_read(&priv->fir_en) && !atomic_read(&priv->suspend))
			{
				int idx, firlen = atomic_read(&priv->firlen);   
				if(priv->fir.num < firlen)
				{                
					priv->fir.raw[priv->fir.num][KX023_AXIS_X] = data[KX023_AXIS_X];
					priv->fir.raw[priv->fir.num][KX023_AXIS_Y] = data[KX023_AXIS_Y];
					priv->fir.raw[priv->fir.num][KX023_AXIS_Z] = data[KX023_AXIS_Z];
					priv->fir.sum[KX023_AXIS_X] += data[KX023_AXIS_X];
					priv->fir.sum[KX023_AXIS_Y] += data[KX023IK_AXIS_Y];
					priv->fir.sum[KX023_AXIS_Z] += data[KX023_AXIS_Z];
					if(atomic_read(&priv->trace) & ADX_TRC_FILTER)
					{
						GSE_LOG("add [%2d] [%5d %5d %5d] => [%5d %5d %5d]\n", priv->fir.num,
							priv->fir.raw[priv->fir.num][KX023_AXIS_X], priv->fir.raw[priv->fir.num][KX023_AXIS_Y], priv->fir.raw[priv->fir.num][KX023_AXIS_Z],
							priv->fir.sum[KX023_AXIS_X], priv->fir.sum[KX023_AXIS_Y], priv->fir.sum[KX023_AXIS_Z]);
					}
					priv->fir.num++;
					priv->fir.idx++;
				}
				else
				{
					idx = priv->fir.idx % firlen;
					priv->fir.sum[KX023_AXIS_X] -= priv->fir.raw[idx][KX023_AXIS_X];
					priv->fir.sum[KX023_AXIS_Y] -= priv->fir.raw[idx][KX023_AXIS_Y];
					priv->fir.sum[KX023_AXIS_Z] -= priv->fir.raw[idx][KX023_AXIS_Z];
					priv->fir.raw[idx][KX023_AXIS_X] = data[KX023_AXIS_X];
					priv->fir.raw[idx][KX023_AXIS_Y] = data[KX023_AXIS_Y];
					priv->fir.raw[idx][KX023_AXIS_Z] = data[KX023_AXIS_Z];
					priv->fir.sum[KX023_AXIS_X] += data[KX023_AXIS_X];
					priv->fir.sum[KX023_AXIS_Y] += data[KX023_AXIS_Y];
					priv->fir.sum[KX023_AXIS_Z] += data[KX023_AXIS_Z];
					priv->fir.idx++;
					data[KX023_AXIS_X] = priv->fir.sum[KX023_AXIS_X]/firlen;
					data[KX023_AXIS_Y] = priv->fir.sum[KX023_AXIS_Y]/firlen;
					data[KX023_AXIS_Z] = priv->fir.sum[KX023_AXIS_Z]/firlen;
					if(atomic_read(&priv->trace) & ADX_TRC_FILTER)
					{
						GSE_LOG("add [%2d] [%5d %5d %5d] => [%5d %5d %5d] : [%5d %5d %5d]\n", idx,
						priv->fir.raw[idx][KX023_AXIS_X], priv->fir.raw[idx][KX023_AXIS_Y], priv->fir.raw[idx][KX023_AXIS_Z],
						priv->fir.sum[KX023_AXIS_X], priv->fir.sum[KX023_AXIS_Y], priv->fir.sum[KX023_AXIS_Z],
						data[KX023_AXIS_X], data[KX023_AXIS_Y], data[KX023_AXIS_Z]);
					}
				}
			}
		}	
#endif         
	}
	return err;
}
/*----------------------------------------------------------------------------*/
static int KX023_ReadOffset(struct i2c_client *client, s8 ofs[KX023_AXES_NUM])
{    
	int err = 0;

	ofs[1]=ofs[2]=ofs[0]=0x00;

	printk("offesx=%x, y=%x, z=%x",ofs[0],ofs[1],ofs[2]);
	
	return err;    
}
/*----------------------------------------------------------------------------*/
static int KX023_ResetCalibration(struct i2c_client *client)
{
	struct kx023_i2c_data *obj = i2c_get_clientdata(client);
	//u8 ofs[4]={0,0,0,0};
	int err = 0;

	memset(obj->cali_sw, 0x00, sizeof(obj->cali_sw));
	memset(obj->offset, 0x00, sizeof(obj->offset));
	return err;    
}
/*----------------------------------------------------------------------------*/
static int KX023_ReadCalibration(struct i2c_client *client, int dat[KX023_AXES_NUM])
{
    struct kx023_i2c_data *obj = i2c_get_clientdata(client);
    //int err;
    int mul;

	#ifdef SW_CALIBRATION
		mul = 0;//only SW Calibration, disable HW Calibration
	#else
	    if ((err = KX023_ReadOffset(client, obj->offset))) {
        GSE_ERR("read offset fail, %d\n", err);
        return err;
    	}    
    	mul = obj->reso->sensitivity/kx023_offset_resolution.sensitivity;
	#endif

    dat[obj->cvt.map[KX023_AXIS_X]] = obj->cvt.sign[KX023_AXIS_X]*(obj->offset[KX023_AXIS_X]*mul + obj->cali_sw[KX023_AXIS_X]);
    dat[obj->cvt.map[KX023_AXIS_Y]] = obj->cvt.sign[KX023_AXIS_Y]*(obj->offset[KX023_AXIS_Y]*mul + obj->cali_sw[KX023_AXIS_Y]);
    dat[obj->cvt.map[KX023_AXIS_Z]] = obj->cvt.sign[KX023_AXIS_Z]*(obj->offset[KX023_AXIS_Z]*mul + obj->cali_sw[KX023_AXIS_Z]);                        
                                       
    return 0;
}
/*----------------------------------------------------------------------------*/
static int KX023_ReadCalibrationEx(struct i2c_client *client, int act[KX023_AXES_NUM], int raw[KX023_AXES_NUM])
{  
	/*raw: the raw calibration data; act: the actual calibration data*/
	struct kx023_i2c_data *obj = i2c_get_clientdata(client);
	//int err;
	int mul;

 

	#ifdef SW_CALIBRATION
		mul = 0;//only SW Calibration, disable HW Calibration
	#else
		if(err = KX023_ReadOffset(client, obj->offset))
		{
			GSE_ERR("read offset fail, %d\n", err);
			return err;
		}   
		mul = obj->reso->sensitivity/kx023_offset_resolution.sensitivity;
	#endif
	
	raw[KX023_AXIS_X] = obj->offset[KX023_AXIS_X]*mul + obj->cali_sw[KX023_AXIS_X];
	raw[KX023_AXIS_Y] = obj->offset[KX023_AXIS_Y]*mul + obj->cali_sw[KX023_AXIS_Y];
	raw[KX023_AXIS_Z] = obj->offset[KX023_AXIS_Z]*mul + obj->cali_sw[KX023_AXIS_Z];

	act[obj->cvt.map[KX023_AXIS_X]] = obj->cvt.sign[KX023_AXIS_X]*raw[KX023_AXIS_X];
	act[obj->cvt.map[KX023_AXIS_Y]] = obj->cvt.sign[KX023_AXIS_Y]*raw[KX023_AXIS_Y];
	act[obj->cvt.map[KX023_AXIS_Z]] = obj->cvt.sign[KX023_AXIS_Z]*raw[KX023_AXIS_Z];                        
	                       
	return 0;
}
/*----------------------------------------------------------------------------*/
static int KX023_WriteCalibration(struct i2c_client *client, int dat[KX023_AXES_NUM])
{
	struct kx023_i2c_data *obj = i2c_get_clientdata(client);
	int err;
	int cali[KX023_AXES_NUM], raw[KX023_AXES_NUM];
	//int lsb = kx023_offset_resolution.sensitivity;
	//int divisor = obj->reso->sensitivity/lsb;

	if((err = KX023_ReadCalibrationEx(client, cali, raw)))	/*offset will be updated in obj->offset*/
	{ 
		GSE_ERR("read offset fail, %d\n", err);
		return err;
	}

	GSE_LOG("OLDOFF: (%+3d %+3d %+3d): (%+3d %+3d %+3d) / (%+3d %+3d %+3d)\n", 
		raw[KX023_AXIS_X], raw[KX023_AXIS_Y], raw[KX023_AXIS_Z],
		obj->offset[KX023_AXIS_X], obj->offset[KX023_AXIS_Y], obj->offset[KX023_AXIS_Z],
		obj->cali_sw[KX023_AXIS_X], obj->cali_sw[KX023_AXIS_Y], obj->cali_sw[KX023_AXIS_Z]);

	/*calculate the real offset expected by caller*/
	cali[KX023_AXIS_X] += dat[KX023_AXIS_X];
	cali[KX023_AXIS_Y] += dat[KX023_AXIS_Y];
	cali[KX023_AXIS_Z] += dat[KX023_AXIS_Z];

	GSE_LOG("UPDATE: (%+3d %+3d %+3d)\n", 
		dat[KX023_AXIS_X], dat[KX023_AXIS_Y], dat[KX023_AXIS_Z]);

#ifdef SW_CALIBRATION
	obj->cali_sw[KX023_AXIS_X] = obj->cvt.sign[KX023_AXIS_X]*(cali[obj->cvt.map[KX023_AXIS_X]]);
	obj->cali_sw[KX023_AXIS_Y] = obj->cvt.sign[KX023_AXIS_Y]*(cali[obj->cvt.map[KX023_AXIS_Y]]);
	obj->cali_sw[KX023_AXIS_Z] = obj->cvt.sign[KX023_AXIS_Z]*(cali[obj->cvt.map[KX023_AXIS_Z]]);	
#else
	obj->offset[KX023_AXIS_X] = (s8)(obj->cvt.sign[KX023_AXIS_X]*(cali[obj->cvt.map[KX023_AXIS_X]])/(divisor));
	obj->offset[KX023_AXIS_Y] = (s8)(obj->cvt.sign[KX023_AXIS_Y]*(cali[obj->cvt.map[KX023_AXIS_Y]])/(divisor));
	obj->offset[KX023_AXIS_Z] = (s8)(obj->cvt.sign[KX023_AXIS_Z]*(cali[obj->cvt.map[KX023_AXIS_Z]])/(divisor));

	/*convert software calibration using standard calibration*/
	obj->cali_sw[KX023_AXIS_X] = obj->cvt.sign[KX023_AXIS_X]*(cali[obj->cvt.map[KX023_AXIS_X]])%(divisor);
	obj->cali_sw[KX023_AXIS_Y] = obj->cvt.sign[KX023_AXIS_Y]*(cali[obj->cvt.map[KX023_AXIS_Y]])%(divisor);
	obj->cali_sw[KX023_AXIS_Z] = obj->cvt.sign[KX023_AXIS_Z]*(cali[obj->cvt.map[KX023_AXIS_Z]])%(divisor);

	GSE_LOG("NEWOFF: (%+3d %+3d %+3d): (%+3d %+3d %+3d) / (%+3d %+3d %+3d)\n", 
		obj->offset[KX023_AXIS_X]*divisor + obj->cali_sw[KX023_AXIS_X], 
		obj->offset[KX023_AXIS_Y]*divisor + obj->cali_sw[KX023_AXIS_Y], 
		obj->offset[KX023_AXIS_Z]*divisor + obj->cali_sw[KX023_AXIS_Z], 
		obj->offset[KX023_AXIS_X], obj->offset[KX023_AXIS_Y], obj->offset[KX023_AXIS_Z],
		obj->cali_sw[KX023_AXIS_X], obj->cali_sw[KX023_AXIS_Y], obj->cali_sw[KX023_AXIS_Z]);

	if(err = hwmsen_write_block(obj->client, KX023_REG_OFSX, obj->offset, KX023_AXES_NUM))
	{
		GSE_ERR("write offset fail: %d\n", err);
		return err;
	}
#endif

	return err;
}
/*----------------------------------------------------------------------------*/
static int KX023_CheckDeviceID(struct i2c_client *client)
{
	u8 databuf[10];    
	int res = 0;

	memset(databuf, 0, sizeof(u8)*10);    
	databuf[0] = KX023_REG_DEVID;   

	res = i2c_master_send(client, databuf, 0x1);
	if(res <= 0)
	{
		goto exit_KX023_CheckDeviceID;
	}
	
	udelay(500);

	databuf[0] = 0x0;        
	res = i2c_master_recv(client, databuf, 0x01);
	if(res <= 0)
	{
		goto exit_KX023_CheckDeviceID;
	}
	

	if(databuf[0] == KX023_FIXED_DEVID)
	{
		printk("KX023_CheckDeviceID 0x%x pass!\n ", databuf[0]);
	}
	else
	{
		printk("KX023_CheckDeviceID 0x%x failt!\n ", databuf[0]);
		return KX023_ERR_IDENTIFICATION;
	}

	exit_KX023_CheckDeviceID:
	if (res <= 0)
	{
		return KX023_ERR_I2C;
	}
	
	return KX023_SUCCESS;
}
/*----------------------------------------------------------------------------*/
static int KX023_SetPowerMode(struct i2c_client *client, bool enable)
{
	u8 databuf[2];    
	int res = 0;
	u8 addr = KX023_REG_POWER_CTL;
	//struct kx023_i2c_data *obj = i2c_get_clientdata(client);
	
	
	if(enable == sensor_power)
	{
		GSE_LOG("Sensor power status is newest!\n");
		return KX023_SUCCESS;
	}

	if(hwmsen_read_block(client, addr, databuf, 0x01))
	{
		GSE_ERR("read power ctl register err!\n");
		return KX023_ERR_I2C;
	}

	
	if(enable == TRUE)
	{
		databuf[0] |= KX023_MEASURE_MODE;
	}
	else
	{
		databuf[0] &= ~KX023_MEASURE_MODE;
	}
	databuf[1] = databuf[0];
	databuf[0] = KX023_REG_POWER_CTL;
	

	res = i2c_master_send(client, databuf, 0x2);

	if(res <= 0)
	{
		return KX023_ERR_I2C;
	}


	GSE_LOG("KX023_SetPowerMode %d!\n ",enable);


	sensor_power = enable;

	mdelay(5);
	
	return KX023_SUCCESS;    
}
/*----------------------------------------------------------------------------*/
static int KX023_SetDataFormat(struct i2c_client *client, u8 dataformat)
{
	struct kx023_i2c_data *obj = i2c_get_clientdata(client);
	u8 databuf[10];    
	int res = 0;

	memset(databuf, 0, sizeof(u8)*10);  

	KX023_SetPowerMode(client, false);

	if(hwmsen_read_block(client, KX023_REG_DATA_FORMAT, databuf, 0x01))
	{
		printk("kx023 read Dataformat failt \n");
		return KX023_ERR_I2C;
	}

	databuf[0] &= ~KX023_RANGE_MASK;
	databuf[0] |= dataformat;
	databuf[1] = databuf[0];
	databuf[0] = KX023_REG_DATA_FORMAT;


	res = i2c_master_send(client, databuf, 0x2);

	if(res <= 0)
	{
		return KX023_ERR_I2C;
	}

	KX023_SetPowerMode(client, true);
	
	printk("KX023_SetDataFormat OK! \n");
	

	return KX023_SetDataResolution(obj);    
}
/*----------------------------------------------------------------------------*/
static int KX023_SetBWRate(struct i2c_client *client, u8 bwrate)
{
	u8 databuf[10];    
	int res = 0;

	memset(databuf, 0, sizeof(u8)*10);    

	if(hwmsen_read_block(client, KX023_REG_BW_RATE, databuf, 0x01))
	{
		printk("kx023 read rate failt \n");
		return KX023_ERR_I2C;
	}

	databuf[0] &= 0xf0;
	databuf[0] |= bwrate;
	databuf[1] = databuf[0];
	databuf[0] = KX023_REG_BW_RATE;


	res = i2c_master_send(client, databuf, 0x2);

	if(res <= 0)
	{
		return KX023_ERR_I2C;
	}
	
	printk("KX023_SetBWRate OK! \n");
	
	return KX023_SUCCESS;    
}
/*----------------------------------------------------------------------------*/
static int KX023_SetIntEnable(struct i2c_client *client, u8 intenable)
{
	u8 databuf[10];    
	int res = 0;

	memset(databuf, 0, sizeof(u8)*10);    
	databuf[0] = KX023_REG_INT_ENABLE;    
	databuf[1] = 0x00;

	res = i2c_master_send(client, databuf, 0x2);

	if(res <= 0)
	{
		return KX023_ERR_I2C;
	}
	
	return KX023_SUCCESS;    
}
/*----------------------------------------------------------------------------*/
static int kx023_init_client(struct i2c_client *client, int reset_cali)
{
	struct kx023_i2c_data *obj = i2c_get_clientdata(client);
	int res = 0;

	res = KX023_CheckDeviceID(client); 
	if(res != KX023_SUCCESS)
	{
		return res;
	}	

	res = KX023_SetPowerMode(client, false);
	if(res != KX023_SUCCESS)
	{
		return res;
	}
	

	res = KX023_SetBWRate(client, KX023_BW_100HZ);
	if(res != KX023_SUCCESS ) //0x2C->BW=100Hz
	{
		return res;
	}

	res = KX023_SetDataFormat(client, KX023_RANGE_2G);
	if(res != KX023_SUCCESS) //0x2C->BW=100Hz
	{
		return res;
	}

	gsensor_gain.x = gsensor_gain.y = gsensor_gain.z = obj->reso->sensitivity;


	res = KX023_SetIntEnable(client, 0x00);        
	if(res != KX023_SUCCESS)//0x2E->0x80
	{
		return res;
	}

	if(0 != reset_cali)
	{ 
		/*reset calibration only in power on*/
		res = KX023_ResetCalibration(client);
		if(res != KX023_SUCCESS)
		{
			return res;
		}
	}
	printk("kx023_init_client OK!\n");
#ifdef CONFIG_KX023_LOWPASS
	memset(&obj->fir, 0x00, sizeof(obj->fir));  
#endif

	return KX023_SUCCESS;
}
/*----------------------------------------------------------------------------*/
static int KX023_ReadChipInfo(struct i2c_client *client, char *buf, int bufsize)
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

	sprintf(buf, "KX023 Chip");
	return 0;
}
/*----------------------------------------------------------------------------*/
static int KX023_ReadSensorData(struct i2c_client *client, char *buf, int bufsize)
{
	struct kx023_i2c_data *obj = (struct kx023_i2c_data*)i2c_get_clientdata(client);
	u8 databuf[20];
	int acc[KX023_AXES_NUM];
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
		res = KX023_SetPowerMode(client, true);
		if(res)
		{
			GSE_ERR("Power on kx023 error %d!\n", res);
		}
	}

	if((res = KX023_ReadData(client, obj->data)))
	{        
		GSE_ERR("I2C error: ret value=%d", res);
		return -3;
	}
	else
	{
		//printk("raw data x=%d, y=%d, z=%d \n",obj->data[KX023_AXIS_X],obj->data[KX023_AXIS_Y],obj->data[KX023_AXIS_Z]);
		obj->data[KX023_AXIS_X] += obj->cali_sw[KX023_AXIS_X];
		obj->data[KX023_AXIS_Y] += obj->cali_sw[KX023_AXIS_Y];
		obj->data[KX023_AXIS_Z] += obj->cali_sw[KX023_AXIS_Z];
		
		//printk("cali_sw x=%d, y=%d, z=%d \n",obj->cali_sw[KX023_AXIS_X],obj->cali_sw[KX023_AXIS_Y],obj->cali_sw[KX023_AXIS_Z]);
		
		/*remap coordinate*/
		acc[obj->cvt.map[KX023_AXIS_X]] = obj->cvt.sign[KX023_AXIS_X]*obj->data[KX023_AXIS_X];
		acc[obj->cvt.map[KX023_AXIS_Y]] = obj->cvt.sign[KX023_AXIS_Y]*obj->data[KX023_AXIS_Y];
		acc[obj->cvt.map[KX023_AXIS_Z]] = obj->cvt.sign[KX023_AXIS_Z]*obj->data[KX023_AXIS_Z];
		//printk("cvt x=%d, y=%d, z=%d \n",obj->cvt.sign[KX023_AXIS_X],obj->cvt.sign[KX023_AXIS_Y],obj->cvt.sign[KX023_AXIS_Z]);


		//GSE_LOG("Mapped gsensor data: %d, %d, %d!\n", acc[KX023_AXIS_X], acc[KX023_AXIS_Y], acc[KX023_AXIS_Z]);

		//Out put the mg
		//printk("mg acc=%d, GRAVITY=%d, sensityvity=%d \n",acc[KX023_AXIS_X],GRAVITY_EARTH_1000,obj->reso->sensitivity);
		acc[KX023_AXIS_X] = acc[KX023_AXIS_X] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
		acc[KX023_AXIS_Y] = acc[KX023_AXIS_Y] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
		acc[KX023_AXIS_Z] = acc[KX023_AXIS_Z] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;		
		
	

		sprintf(buf, "%04x %04x %04x", acc[KX023_AXIS_X], acc[KX023_AXIS_Y], acc[KX023_AXIS_Z]);
		if(atomic_read(&obj->trace) & ADX_TRC_IOCTL)
		{
			GSE_LOG("gsensor data: %s!\n", buf);
		}
	}
	
	return 0;
}
/*----------------------------------------------------------------------------*/
static int KX023_ReadRawData(struct i2c_client *client, char *buf)
{
	struct kx023_i2c_data *obj = (struct kx023_i2c_data*)i2c_get_clientdata(client);
	int res = 0;

	if (!buf || !client)
	{
		return EINVAL;
	}
	
	if((res = KX023_ReadData(client, obj->data)))
	{        
		GSE_ERR("I2C error: ret value=%d", res);
		return EIO;
	}
	else
	{
		sprintf(buf, "KX023_ReadRawData %04x %04x %04x", obj->data[KX023_AXIS_X], 
			obj->data[KX023_AXIS_Y], obj->data[KX023_AXIS_Z]);
	
	}
	
	return 0;
}
/*----------------------------------------------------------------------------*/
static int KX023_InitSelfTest(struct i2c_client *client)
{
	int res = 0;
	u8  data,result;
	
	res = hwmsen_read_byte(client, KX023_REG_CTL_REG2, &data);
	if(res != KX023_SUCCESS)
	{
		return res;
	}
//enable selftest bit
	res = hwmsen_write_byte(client, KX023_REG_CTL_REG2,  KX023_SELF_TEST|data);
	if(res != KX023_SUCCESS) //0x2C->BW=100Hz
	{
		return res;
	}
//step 1
	res = hwmsen_read_byte(client, KX023_DCST_RESP, &result);
	if(res != KX023_SUCCESS)
	{
		return res;
	}
	printk("step1: result = %x",result);
	if(result != 0xaa)
		return -EINVAL;

//step 2
	res = hwmsen_write_byte(client, KX023_REG_CTL_REG2,  KX023_SELF_TEST|data);
	if(res != KX023_SUCCESS) //0x2C->BW=100Hz
	{
		return res;
	}
//step 3
	res = hwmsen_read_byte(client, KX023_DCST_RESP, &result);
	if(res != KX023_SUCCESS)
	{
		return res;
	}
	printk("step3: result = %x",result);
	if(result != 0xAA)
		return -EINVAL;
		
//step 4
	res = hwmsen_read_byte(client, KX023_DCST_RESP, &result);
	if(res != KX023_SUCCESS)
	{
		return res;
	}
	printk("step4: result = %x",result);
	if(result != 0x55)
		return -EINVAL;
	else
		return KX023_SUCCESS;
}
/*----------------------------------------------------------------------------*/
/*
static int KX023_JudgeTestResult(struct i2c_client *client, s32 prv[KX023_AXES_NUM], s32 nxt[KX023_AXES_NUM])
{

    int res=0;
	u8 test_result=0;
    if(res = hwmsen_read_byte(client, 0x0c, &test_result))
        return res;

	printk("test_result = %x \n",test_result);
    if ( test_result != 0xaa ) 
	{
        GSE_ERR("KX023_JudgeTestResult failt\n");
        res = -EINVAL;
    }
    return res;
}
*/
/*----------------------------------------------------------------------------*/
static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = kx023_i2c_client;
	char strbuf[KX023_BUFSIZE];
	if(NULL == client)
	{
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}
	
	KX023_ReadChipInfo(client, strbuf, KX023_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);        
}
/*
static ssize_t gsensor_init(struct device_driver *ddri, char *buf, size_t count)
{
		struct i2c_client *client = kx023_i2c_client;
		char strbuf[KX023_BUFSIZE];
		
		if(NULL == client)
		{
			GSE_ERR("i2c client is null!!\n");
			return 0;
		}
		kx023_init_client(client, 1);
		return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);			
}
*/


/*----------------------------------------------------------------------------*/
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = kx023_i2c_client;
	char strbuf[KX023_BUFSIZE];
	
	if(NULL == client)
	{
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}
	KX023_ReadSensorData(client, strbuf, KX023_BUFSIZE);
	//KX023_ReadRawData(client, strbuf);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);            
}
/*
static ssize_t show_sensorrawdata_value(struct device_driver *ddri, char *buf, size_t count)
{
		struct i2c_client *client = kx023_i2c_client;
		char strbuf[KX023_BUFSIZE];
		
		if(NULL == client)
		{
			GSE_ERR("i2c client is null!!\n");
			return 0;
		}
		//KX023_ReadSensorData(client, strbuf, KX023_BUFSIZE);
		KX023_ReadRawData(client, strbuf);
		return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);			
}
*/
/*----------------------------------------------------------------------------*/
static ssize_t show_cali_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = kx023_i2c_client;
	struct kx023_i2c_data *obj;
	int err, len = 0, mul;
	int tmp[KX023_AXES_NUM];

	if(NULL == client)
	{
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	obj = i2c_get_clientdata(client);



	if((err = KX023_ReadOffset(client, obj->offset)))
	{
		return -EINVAL;
	}
	else if((err = KX023_ReadCalibration(client, tmp)))
	{
		return -EINVAL;
	}
	else
	{    
		mul = obj->reso->sensitivity/kx023_offset_resolution.sensitivity;
		len += snprintf(buf+len, PAGE_SIZE-len, "[HW ][%d] (%+3d, %+3d, %+3d) : (0x%02X, 0x%02X, 0x%02X)\n", mul,                        
			obj->offset[KX023_AXIS_X], obj->offset[KX023_AXIS_Y], obj->offset[KX023_AXIS_Z],
			obj->offset[KX023_AXIS_X], obj->offset[KX023_AXIS_Y], obj->offset[KX023_AXIS_Z]);
		len += snprintf(buf+len, PAGE_SIZE-len, "[SW ][%d] (%+3d, %+3d, %+3d)\n", 1, 
			obj->cali_sw[KX023_AXIS_X], obj->cali_sw[KX023_AXIS_Y], obj->cali_sw[KX023_AXIS_Z]);

		len += snprintf(buf+len, PAGE_SIZE-len, "[ALL]    (%+3d, %+3d, %+3d) : (%+3d, %+3d, %+3d)\n", 
			obj->offset[KX023_AXIS_X]*mul + obj->cali_sw[KX023_AXIS_X],
			obj->offset[KX023_AXIS_Y]*mul + obj->cali_sw[KX023_AXIS_Y],
			obj->offset[KX023_AXIS_Z]*mul + obj->cali_sw[KX023_AXIS_Z],
			tmp[KX023_AXIS_X], tmp[KX023_AXIS_Y], tmp[KX023_AXIS_Z]);
		
		return len;
    }
}
/*----------------------------------------------------------------------------*/
static ssize_t store_cali_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct i2c_client *client = kx023_i2c_client;  
	int err, x, y, z;
	int dat[KX023_AXES_NUM];

	if(!strncmp(buf, "rst", 3))
	{
		if((err = KX023_ResetCalibration(client)))
		{
			GSE_ERR("reset offset err = %d\n", err);
		}	
	}
	else if(3 == sscanf(buf, "0x%02X 0x%02X 0x%02X", &x, &y, &z))
	{
		dat[KX023_AXIS_X] = x;
		dat[KX023_AXIS_Y] = y;
		dat[KX023_AXIS_Z] = z;
		if((err = KX023_WriteCalibration(client, dat)))
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
static ssize_t show_self_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = kx023_i2c_client;
	//struct kx023_i2c_data *obj;

	if(NULL == client)
	{
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	//obj = i2c_get_clientdata(client);
	
    return snprintf(buf, 8, "%s\n", selftestRes);
}
/*----------------------------------------------------------------------------*/
static ssize_t store_self_value(struct device_driver *ddri, const char *buf, size_t count)
{   /*write anything to this register will trigger the process*/
	struct item{
	s16 raw[KX023_AXES_NUM];
	};
	
	struct i2c_client *client = kx023_i2c_client;  
	int res, num;
	struct item *prv = NULL, *nxt = NULL;
	//s32 avg_prv[KX023_AXES_NUM] = {0, 0, 0};
	//s32 avg_nxt[KX023_AXES_NUM] = {0, 0, 0};
	u8 data;


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
	KX023_SetPowerMode(client,true); 

	/*initial setting for self test*/
	if(!KX023_InitSelfTest(client))
	{
		GSE_LOG("SELFTEST : PASS\n");
		strcpy(selftestRes,"y");
	}	
	else
	{
		GSE_LOG("SELFTEST : FAIL\n");		
		strcpy(selftestRes,"n");
	}

	res = hwmsen_read_byte(client, KX023_REG_CTL_REG2, &data);
	if(res != KX023_SUCCESS)
	{
		return res;
	}

	res = hwmsen_write_byte(client, KX023_REG_CTL_REG2,  ~KX023_SELF_TEST&data);
	if(res != KX023_SUCCESS) //0x2C->BW=100Hz
	{
		return res;
	}
	
	exit:
	/*restore the setting*/    
	kx023_init_client(client, 0);
	kfree(prv);
	kfree(nxt);
	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t show_selftest_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = kx023_i2c_client;
	struct kx023_i2c_data *obj;

	if(NULL == client)
	{
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	obj = i2c_get_clientdata(client);
	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&obj->selftest));
}
/*----------------------------------------------------------------------------*/
static ssize_t store_selftest_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct kx023_i2c_data *obj = obj_i2c_data;
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
			kx023_init_client(obj->client, 0);
		}
		else if(!atomic_read(&obj->selftest) && tmp)
		{
			/*disable -> enable*/
			KX023_InitSelfTest(obj->client);            
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
#ifdef CONFIG_KX023_LOWPASS
	struct i2c_client *client = kx023_i2c_client;
	struct kx023_i2c_data *obj = i2c_get_clientdata(client);
	if(atomic_read(&obj->firlen))
	{
		int idx, len = atomic_read(&obj->firlen);
		GSE_LOG("len = %2d, idx = %2d\n", obj->fir.num, obj->fir.idx);

		for(idx = 0; idx < len; idx++)
		{
			GSE_LOG("[%5d %5d %5d]\n", obj->fir.raw[idx][KX023_AXIS_X], obj->fir.raw[idx][KX023_AXIS_Y], obj->fir.raw[idx][KX023_AXIS_Z]);
		}
		
		GSE_LOG("sum = [%5d %5d %5d]\n", obj->fir.sum[KX023_AXIS_X], obj->fir.sum[KX023_AXIS_Y], obj->fir.sum[KX023_AXIS_Z]);
		GSE_LOG("avg = [%5d %5d %5d]\n", obj->fir.sum[KX023_AXIS_X]/len, obj->fir.sum[KX023_AXIS_Y]/len, obj->fir.sum[KX023_AXIS_Z]/len);
	}
	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&obj->firlen));
#else
	return snprintf(buf, PAGE_SIZE, "not support\n");
#endif
}
/*----------------------------------------------------------------------------*/
static ssize_t store_firlen_value(struct device_driver *ddri, const char *buf, size_t count)
{
#ifdef CONFIG_KX023_LOWPASS
	struct i2c_client *client = kx023_i2c_client;  
	struct kx023_i2c_data *obj = i2c_get_clientdata(client);
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
		if(NULL == firlen)
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
	struct kx023_i2c_data *obj = obj_i2c_data;
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
	struct kx023_i2c_data *obj = obj_i2c_data;
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
	struct kx023_i2c_data *obj = obj_i2c_data;
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
static ssize_t show_power_status_value(struct device_driver *ddri, char *buf)
{
	if(sensor_power)
		printk("G sensor is in work mode, sensor_power = %d\n", sensor_power);
	else
		printk("G sensor is in standby mode, sensor_power = %d\n", sensor_power);

	return 0;
}

/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(chipinfo,   S_IWUSR | S_IRUGO, show_chipinfo_value,      NULL);
static DRIVER_ATTR(sensordata, S_IWUSR | S_IRUGO, show_sensordata_value,    NULL);
static DRIVER_ATTR(cali,       S_IWUSR | S_IRUGO, show_cali_value,          store_cali_value);
static DRIVER_ATTR(selftest, S_IWUSR | S_IRUGO, show_self_value,  store_self_value);
static DRIVER_ATTR(self,   S_IWUSR | S_IRUGO, show_selftest_value,      store_selftest_value);
static DRIVER_ATTR(firlen,     S_IWUSR | S_IRUGO, show_firlen_value,        store_firlen_value);
static DRIVER_ATTR(trace,      S_IWUSR | S_IRUGO, show_trace_value,         store_trace_value);
static DRIVER_ATTR(status,               S_IRUGO, show_status_value,        NULL);
static DRIVER_ATTR(powerstatus,               S_IRUGO, show_power_status_value,        NULL);


/*----------------------------------------------------------------------------*/
static u8 i2c_dev_reg =0 ;

static ssize_t show_register(struct device_driver *pdri, char *buf)
{
	//int input_value;
		
	printk("i2c_dev_reg is 0x%2x \n", i2c_dev_reg);

	return 0;
}

static ssize_t store_register(struct device_driver *ddri, const char *buf, size_t count)
{
	//unsigned long input_value;

	i2c_dev_reg = simple_strtoul(buf, NULL, 16);
	printk("set i2c_dev_reg = 0x%2x \n", i2c_dev_reg);

	return 0;
}
static ssize_t store_register_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct kx023_i2c_data *obj = obj_i2c_data;
	u8 databuf[2];  
	unsigned long input_value;
	int res;
	
	memset(databuf, 0, sizeof(u8)*2);    

	input_value = simple_strtoul(buf, NULL, 16);
	printk("input_value = 0x%2lx \n", input_value);

	if(NULL == obj)
	{
		GSE_ERR("i2c data obj is null!!\n");
		return 0;
	}

	databuf[0] = i2c_dev_reg;
	databuf[1] = input_value;
	printk("databuf[0]=0x%2x  databuf[1]=0x%2x \n", databuf[0],databuf[1]);

	res = i2c_master_send(obj->client, databuf, 0x2);

	if(res <= 0)
	{
		return KX023_ERR_I2C;
	}
	return 0;
	
}

static ssize_t show_register_value(struct device_driver *ddri, char *buf)
{
		struct kx023_i2c_data *obj = obj_i2c_data;
		u8 databuf[1];	
		
		memset(databuf, 0, sizeof(u8)*1);	 
	
		if(NULL == obj)
		{
			GSE_ERR("i2c data obj is null!!\n");
			return 0;
		}
		
		if(hwmsen_read_block(obj->client, i2c_dev_reg, databuf, 0x01))
		{
			GSE_ERR("read power ctl register err!\n");
			return KX023_ERR_I2C;
		}

		printk("i2c_dev_reg=0x%2x  data=0x%2x \n", i2c_dev_reg,databuf[0]);
	
		return 0;
		
}


static DRIVER_ATTR(i2c,      S_IWUSR | S_IRUGO, show_register_value,         store_register_value);
static DRIVER_ATTR(register,      S_IWUSR | S_IRUGO, show_register,         store_register);


/*----------------------------------------------------------------------------*/
static struct driver_attribute *kx023_attr_list[] = {
	&driver_attr_chipinfo,     /*chip information*/
	&driver_attr_sensordata,   /*dump sensor data*/
	&driver_attr_cali,         /*show calibration data*/
	&driver_attr_self,         /*self test demo*/
	&driver_attr_selftest,     /*self control: 0: disable, 1: enable*/
	&driver_attr_firlen,       /*filter length: 0: disable, others: enable*/
	&driver_attr_trace,        /*trace log*/
	&driver_attr_status,
	&driver_attr_powerstatus,
	&driver_attr_register,
	&driver_attr_i2c,
};
/*----------------------------------------------------------------------------*/
static int kx023_create_attr(struct device_driver *driver) 
{
	int idx, err = 0;
	int num = (int)(sizeof(kx023_attr_list)/sizeof(kx023_attr_list[0]));
	if (driver == NULL)
	{
		return -EINVAL;
	}

	for(idx = 0; idx < num; idx++)
	{
		if((err = driver_create_file(driver, kx023_attr_list[idx])))
		{            
			GSE_ERR("driver_create_file (%s) = %d\n", kx023_attr_list[idx]->attr.name, err);
			break;
		}
	}    
	return err;
}
/*----------------------------------------------------------------------------*/
static int kx023_delete_attr(struct device_driver *driver)
{
	int idx ,err = 0;
	int num = (int)(sizeof(kx023_attr_list)/sizeof(kx023_attr_list[0]));

	if(driver == NULL)
	{
		return -EINVAL;
	}
	

	for(idx = 0; idx < num; idx++)
	{
		driver_remove_file(driver, kx023_attr_list[idx]);
	}
	

	return err;
}

/*----------------------------------------------------------------------------*/
int kx023_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value, sample_delay;	
	struct kx023_i2c_data *priv = (struct kx023_i2c_data*)self;
	hwm_sensor_data* gsensor_data;
	char buff[KX023_BUFSIZE];
	
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
					sample_delay = KX023_BW_200HZ;
				}
				else if(value <= 10)
				{
					sample_delay = KX023_BW_100HZ;
				}
				else
				{
					sample_delay = KX023_BW_50HZ;
				}
				
				err = KX023_SetBWRate(priv->client, sample_delay);
				if(err != KX023_SUCCESS ) //0x2C->BW=100Hz
				{
					GSE_ERR("Set delay parameter error!\n");
				}

				if(value >= 50)
				{
					atomic_set(&priv->filter, 0);
				}
				else
				{	
				#if defined(CONFIG_KX023_LOWPASS)
					priv->fir.num = 0;
					priv->fir.idx = 0;
					priv->fir.sum[KX023_AXIS_X] = 0;
					priv->fir.sum[KX023_AXIS_Y] = 0;
					priv->fir.sum[KX023_AXIS_Z] = 0;
					atomic_set(&priv->filter, 1);
				#endif
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
				if(((value == 0) && (sensor_power == false)) ||((value == 1) && (sensor_power == true)))
				{
					GSE_LOG("Gsensor device have updated!\n");
				}
				else
				{
					err = KX023_SetPowerMode( priv->client, !sensor_power);
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
				KX023_ReadSensorData(priv->client, buff, KX023_BUFSIZE);
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
static int kx023_open(struct inode *inode, struct file *file)
{
	file->private_data = kx023_i2c_client;

	if(file->private_data == NULL)
	{
		GSE_ERR("null pointer!!\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}
/*----------------------------------------------------------------------------*/
static int kx023_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}
/*----------------------------------------------------------------------------*/
//static int kx023_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
//       unsigned long arg)
static long kx023_unlocked_ioctl(struct file *file, unsigned int cmd,unsigned long arg)
{
	struct i2c_client *client = (struct i2c_client*)file->private_data;
	struct kx023_i2c_data *obj = (struct kx023_i2c_data*)i2c_get_clientdata(client);	
	char strbuf[KX023_BUFSIZE];
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
			kx023_init_client(client, 0);			
			break;

		case GSENSOR_IOCTL_READ_CHIPINFO:
			data = (void __user *) arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}
			
			KX023_ReadChipInfo(client, strbuf, KX023_BUFSIZE);
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
			
			KX023_ReadSensorData(client, strbuf, KX023_BUFSIZE);
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
			KX023_ReadRawData(client, strbuf);
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
				cali[KX023_AXIS_X] = sensor_data.x * obj->reso->sensitivity / GRAVITY_EARTH_1000;
				cali[KX023_AXIS_Y] = sensor_data.y * obj->reso->sensitivity / GRAVITY_EARTH_1000;
				cali[KX023_AXIS_Z] = sensor_data.z * obj->reso->sensitivity / GRAVITY_EARTH_1000;			  
				err = KX023_WriteCalibration(client, cali);			 
			}
			break;

		case GSENSOR_IOCTL_CLR_CALI:
			err = KX023_ResetCalibration(client);
			break;

		case GSENSOR_IOCTL_GET_CALI:
			data = (void __user*)arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}
			if((err = KX023_ReadCalibration(client, cali)))
			{
				break;
			}
			
			sensor_data.x = cali[KX023_AXIS_X] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
			sensor_data.y = cali[KX023_AXIS_Y] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
			sensor_data.z = cali[KX023_AXIS_Z] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
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
static struct file_operations kx023_fops = {
	.owner = THIS_MODULE,
	.open = kx023_open,
	.release = kx023_release,
	.unlocked_ioctl = kx023_unlocked_ioctl,
	//.ioctl = kx023_ioctl,
};
/*----------------------------------------------------------------------------*/
static struct miscdevice kx023_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gsensor",
	.fops = &kx023_fops,
};
/*----------------------------------------------------------------------------*/
#ifndef CONFIG_HAS_EARLYSUSPEND
/*----------------------------------------------------------------------------*/
static int kx023_suspend(struct i2c_client *client, pm_message_t msg) 
{
	struct kx023_i2c_data *obj = i2c_get_clientdata(client);    
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
		if(err = KX023_SetPowerMode(obj->client, false))
		{
			GSE_ERR("write power control fail!!\n");
			return;
		}

		sensor_power = false;      
		KX023_power(obj->hw, 0);
	}
	return err;
}
/*----------------------------------------------------------------------------*/
static int kx023_resume(struct i2c_client *client)
{
	struct kx023_i2c_data *obj = i2c_get_clientdata(client);        
	int err;
	GSE_FUN();

	if(obj == NULL)
	{
		GSE_ERR("null pointer!!\n");
		return -EINVAL;
	}

	KX023_power(obj->hw, 1);
	if(err = kx023_init_client(client, 0))
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
static void kx023_early_suspend(struct early_suspend *h) 
{
	struct kx023_i2c_data *obj = container_of(h, struct kx023_i2c_data, early_drv);   
	int err;
	GSE_FUN();    

	if(obj == NULL)
	{
		GSE_ERR("null pointer!!\n");
		return;
	}
	atomic_set(&obj->suspend, 1); 
	if((err = KX023_SetPowerMode(obj->client, false)))
	{
		GSE_ERR("write power control fail!!\n");
		return;
	}

	sensor_power = false;
	
	KX023_power(obj->hw, 0);
}
/*----------------------------------------------------------------------------*/
static void kx023_late_resume(struct early_suspend *h)
{
	struct kx023_i2c_data *obj = container_of(h, struct kx023_i2c_data, early_drv);         
	int err;
	GSE_FUN();

	if(obj == NULL)
	{
		GSE_ERR("null pointer!!\n");
		return;
	}

	KX023_power(obj->hw, 1);
	if((err = kx023_init_client(obj->client, 0)))
	{
		GSE_ERR("initialize client fail!!\n");
		return;        
	}
	atomic_set(&obj->suspend, 0);    
}
/*----------------------------------------------------------------------------*/
#endif /*CONFIG_HAS_EARLYSUSPEND*/
/*----------------------------------------------------------------------------*/
static int kx023_i2c_detect(struct i2c_client *client, struct i2c_board_info *info) 
{    
	strcpy(info->type, KX023_DEV_NAME);
	return 0;
}

// if use  this typ of enable , Gsensor should report inputEvent(x, y, z ,stats, div) to HAL
static int kx023_open_report_data(int open)
{
	//should queuq work to report event if  is_report_input_direct=true
	return 0;
}

// if use  this typ of enable , Gsensor only enabled but not report inputEvent to HAL

static int kx023_enable_nodata(int en)
{
	int res =0;
	bool power = false;
	
	if(1==en)
	{
		power = true;
	}
	if(0==en)
	{
		power = false;
	}
	res = KX023_SetPowerMode(obj_i2c_data->client, power);
	if(res != KX023_SUCCESS)
	{
		GSE_ERR("KX023_SetPowerMode fail!\n");
		return -1;
	}
	GSE_LOG("kx023_enable_nodata OK!\n");
	return 0;
}

static int kx023_set_delay(u64 ns)
{
    int value =0;
	int sample_delay=0;
	int err;
	value = (int)ns/1000/1000;
	if(value <= 5)
	{
		sample_delay = KX023_BW_200HZ;
	}
	else if(value <= 10)
	{
		sample_delay = KX023_BW_100HZ;
	}
	else
	{
		sample_delay = KX023_BW_50HZ;
	}
	err = KX023_SetBWRate(obj_i2c_data->client, sample_delay);
	if(err != KX023_SUCCESS ) //0x2C->BW=100Hz
	{
		GSE_ERR("Set delay parameter error!\n");
	}
	if(value >= 50)
	{
		atomic_set(&obj_i2c_data->filter, 0);
	}
	else
	{
	#if defined(CONFIG_KX023_LOWPASS)
		obj_i2c_data->fir.num = 0;
		obj_i2c_data->fir.idx = 0;
		obj_i2c_data->fir.sum[KX023_AXIS_X] = 0;
		obj_i2c_data->fir.sum[KX023_AXIS_Y] = 0;
		obj_i2c_data->fir.sum[KX023_AXIS_Z] = 0;
		atomic_set(&obj_i2c_data->filter, 1);
	#endif
	}
	
	GSE_LOG("kx023_set_delay (%d)\n",value);
	return 0;
}

static int kx023_get_data(int* x ,int* y,int* z, int* status)
{
	char buff[KX023_BUFSIZE];
	KX023_ReadSensorData(obj_i2c_data->client, buff, KX023_BUFSIZE);
	
	sscanf(buff, "%x %x %x", x, y, z);		
	*status = SENSOR_STATUS_ACCURACY_MEDIUM;

	return 0;
}



/*----------------------------------------------------------------------------*/
static int kx023_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_client *new_client;
	struct kx023_i2c_data *obj;
	//struct acc_drv_obj sobj;
	int err = 0;
	struct acc_control_path ctl={0};
	struct acc_data_path data={0};
	GSE_FUN();

	if(!(obj = kzalloc(sizeof(*obj), GFP_KERNEL)))
	{
		err = -ENOMEM;
		goto exit;
	}
	
	memset(obj, 0, sizeof(struct kx023_i2c_data));

	obj->hw = kx023_get_cust_acc_hw();
	
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
	
#ifdef CONFIG_KX023_LOWPASS
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

	kx023_i2c_client = new_client;	

	if((err = kx023_init_client(new_client, 1)))
	{
		goto exit_init_failed;
	}
	

	if((err = misc_register(&kx023_device)))
	{
		GSE_ERR("kx023_device register failed\n");
		goto exit_misc_device_register_failed;
	}

	if((err = kx023_create_attr(&(kx023_init_info.platform_diver_addr->driver))))
	{
		GSE_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}

	ctl.open_report_data= kx023_open_report_data;
	ctl.enable_nodata = kx023_enable_nodata;
	ctl.set_delay  = kx023_set_delay;
	ctl.is_report_input_direct = false;
	
	err = acc_register_control_path(&ctl);
	if(err)
	{
	 	GSE_ERR("register acc control path err\n");
		goto exit_kfree;
	}

	data.get_data = kx023_get_data;
	data.vender_div = 1000;
	err = acc_register_data_path(&data);
	if(err)
	{
	 	GSE_ERR("register acc data path err\n");
		goto exit_kfree;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	obj->early_drv.level    = EARLY_SUSPEND_LEVEL_DISABLE_FB - 1,
	obj->early_drv.suspend  = kx023_early_suspend,
	obj->early_drv.resume   = kx023_late_resume,    
	register_early_suspend(&obj->early_drv);
#endif 

	GSE_LOG("%s: OK\n", __func__);    
	kx023_init_flag = 0;    
	return 0;

	exit_create_attr_failed:
	misc_deregister(&kx023_device);
	exit_misc_device_register_failed:
	exit_init_failed:
	//i2c_detach_client(new_client);
	exit_kfree:
	kfree(obj);
	exit:
	GSE_ERR("%s: err = %d\n", __func__, err);        
	kx023_init_flag = -1;          
	return err;
}

/*----------------------------------------------------------------------------*/
static int kx023_i2c_remove(struct i2c_client *client)
{
	int err = 0;	
	
	if((err = kx023_delete_attr(&(kx023_init_info.platform_diver_addr->driver))))
	{
		GSE_ERR("kx023_delete_attr fail: %d\n", err);
	}
	
	if((err = misc_deregister(&kx023_device)))
	{
		GSE_ERR("misc_deregister fail: %d\n", err);
	}

	if((err = hwmsen_detach(ID_ACCELEROMETER)))
	    

	kx023_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));
	return 0;
}
/*----------------------------------------------------------------------------*/
static int kx023_remove(void)
{
    struct acc_hw *hw = kx023_get_cust_acc_hw();
    GSE_FUN();    
    KX023_power(hw, 0);    
    i2c_del_driver(&kx023_i2c_driver);
    return 0;
}
/*----------------------------------------------------------------------------*/
static int kx023_local_init(void)
{
   	struct acc_hw *hw = kx023_get_cust_acc_hw();
	GSE_FUN();

	KX023_power(hw, 1);
	if(i2c_add_driver(&kx023_i2c_driver))
	{
		GSE_ERR("add driver error\n");
		return -1;
	}
	if(-1 == kx023_init_flag)
	{
	   return -1;
	}
	
	return 0;
}
static int __init kx023_init(void)
{
	GSE_FUN();	
	struct acc_hw *hw = kx023_get_cust_acc_hw();
	GSE_LOG("%s: i2c_number=%d\n", __func__,hw->i2c_num);
	i2c_register_board_info(hw->i2c_num, &i2c_kx023, 1);
	acc_driver_add(&kx023_init_info);
	return 0;    
}
/*----------------------------------------------------------------------------*/
static void __exit kx023_exit(void)
{
	GSE_FUN();
	//platform_driver_unregister(&mma8452q_gsensor_driver);
}
/*----------------------------------------------------------------------------*/
module_init(kx023_init);
module_exit(kx023_exit);
/*----------------------------------------------------------------------------*/
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("KX023 I2C driver");
MODULE_AUTHOR("Dexiang.Liu@mediatek.com");
/* END PN:DTS2013062101609  ,Modified by y00187129, 2013/6/21*/