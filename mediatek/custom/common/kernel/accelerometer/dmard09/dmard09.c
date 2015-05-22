/* DMARD09 motion sensor driver
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

#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>

#define POWER_NONE_MACRO MT65XX_POWER_NONE

#include <cust_acc.h>
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include <cust_eint.h>
#include <cust_alsps.h>


#include "dmard09.h"
#include <linux/hwmsen_helper.h>

#ifndef USE_MTK_CALIBRATE
#include <linux/fs.h>
#include <linux/string.h>
#endif

/*----------------------------------------------------------------------------*/
#define I2C_DRIVERID_DMARD09 09
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
//#define CONFIG_DMARD09_LOWPASS   /*apply low pass filter on output*/       
#define SW_CALIBRATION

/*----------------------------------------------------------------------------*/
#define DMARD09_AXIS_X          0
#define DMARD09_AXIS_Y          1
#define DMARD09_AXIS_Z          2
#define DMARD09_AXES_NUM        3
#define DMARD09_DATA_LEN        8//cause we donot read from REG_X
#define DMARD09_DEV_NAME        "DMARD09"
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static const struct i2c_device_id dmard09_i2c_id[] = {{DMARD09_DEV_NAME,0},{}};
/*the adapter id will be available in customization*/
static struct i2c_board_info __initdata i2c_dmard09={ I2C_BOARD_INFO("DMARD09", DMARD09_I2C_SLAVE_WRITE_ADDR>>1)};

//static unsigned short dmard09_force[] = {0x00, DMARD09_I2C_SLAVE_WRITE_ADDR, I2C_CLIENT_END, I2C_CLIENT_END};
//static const unsigned short *const dmard09_forces[] = { dmard09_force, NULL };
//static struct i2c_client_address_data dmard09_addr_data = { .forces = dmard09_forces,};

/*----------------------------------------------------------------------------*/
static int dmard09_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id); 
static int dmard09_i2c_remove(struct i2c_client *client);
static int dmard09_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);

#ifndef USE_MTK_CALIBRATE
void D09_write_offset_to_file(struct i2c_client *client, char*);
void D09_read_offset_from_file(struct i2c_client *client);

char OffsetFileName[] = "/data/misc/gsensor_offset.txt";	/* FILE offset.txt */
char DmtXXFileName[] = "/data/misc/dmt_sensor.txt";
#endif
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
    s16 raw[C_MAX_FIR_LENGTH][DMARD09_AXES_NUM];
    int sum[DMARD09_AXES_NUM];
    int num;
    int idx;
};
/*----------------------------------------------------------------------------*/
struct dmard09_i2c_data {
    struct i2c_client *client;
    struct acc_hw *hw;
    struct hwmsen_convert   cvt;
    atomic_t layout;
    /*misc*/
    struct data_resolution *reso;
    atomic_t                trace;
    atomic_t                suspend;
    atomic_t                selftest;
	atomic_t				filter;
    s16                     cali_sw[DMARD09_AXES_NUM+1];

    /*data*/
    s8                      offset[DMARD09_AXES_NUM+1];  /*+1: for 4-byte alignment*/
    s16                     data[DMARD09_AXES_NUM+1];
#ifndef USE_MTK_CALIBRATE
    raw_data				offset;			/* D09 Offset */
#endif
#if defined(CONFIG_DMARD09_LOWPASS)
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
static struct i2c_driver dmard09_i2c_driver = {
    .driver = {
      //  .owner          = THIS_MODULE,
        .name           = DMARD09_DEV_NAME,
    },
	.probe      		= dmard09_i2c_probe,
	.remove    			= dmard09_i2c_remove,
	.detect				= dmard09_i2c_detect,
#if !defined(CONFIG_HAS_EARLYSUSPEND)    
    .suspend            = dmard09_suspend,
    .resume             = dmard09_resume,
#endif
	.id_table = dmard09_i2c_id,
	//.address_data = &dmard09_addr_data,
};

/*----------------------------------------------------------------------------*/
static struct i2c_client *dmard09_i2c_client = NULL;
static struct platform_driver dmard09_gsensor_driver;
static struct dmard09_i2c_data *obj_i2c_data = NULL;
static bool sensor_power = true;
static GSENSOR_VECTOR3D gsensor_gain;
static char selftestRes[8]= {0}; 
#define DEBUG
/*----------------------------------------------------------------------------*/
#define GSE_TAG                  "[Gsensor] "
#ifdef DEBUG
#define GSE_FUN(f)               printk(KERN_INFO GSE_TAG"%s\n", __FUNCTION__)
#define GSE_ERR(fmt, args...)    printk(KERN_ERR GSE_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define GSE_LOG(fmt, args...)    printk(KERN_INFO GSE_TAG fmt, ##args)
#else
#define GSE_ERR(fmt, args...)
#define GSE_LOG(fmt, args...)
#define GSE_FUN(f)
#define DMT_DATA(dev, format, ...)
#endif
/*----------------------------------------------------------------------------*/
static struct data_resolution dmard09_data_resolution[1] = {
 /* combination by {FULL_RES,RANGE}*/
    //{{ 2, 9}, 256},   // D08 dataformat +/-3g  in 11-bit resolution;  { 2, 9} = 2.9 = (6*1000)/(2^11);  341 = (2^11)/(6)   
    //{{ 7, 8}, 128},   // D10 dataformat +/-4g  in 10-bit resolution;  { 7, 8} = 7.8 = (4*1000)/(2^10);  341 = (2^10)/(4)
    {{ 31, 2}, 32},   // D09 dataformat +/-8g  in 12-bit resolution;  { 3, 9} = 3.9 = (8*1000)/(2^12);  512 = (2^12)/(8)        
};
/*----------------------------------------------------------------------------*/
static struct data_resolution dmard09_offset_resolution = {{15, 6}, 64};	// uncertainty
/*--------------------DMARD09 power control function----------------------------------*/
static void DMARD09_power(struct acc_hw *hw, unsigned int on) 
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
			if(!hwPowerOn(hw->power_id, hw->power_vol, "DMARD09"))
			{
				GSE_ERR("power on fails!!\n");
			}
		}
		else	// power off
		{
			if (!hwPowerDown(hw->power_id, "DMARD09"))
			{
				GSE_ERR("power off fail!!\n");
			}			  
		}
	}
	power_on = on;    
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static int DMARD09_SetDataResolution(struct i2c_client *client)
{
	int err;
	u8  dat, reso;
    struct dmard09_i2c_data *obj = i2c_get_clientdata(client);

 	obj->reso = &dmard09_data_resolution[0];
	return 0;
}
/*----------------------------------------------------------------------------*/
static int DMARD09_ReadData(struct i2c_client *client, s16 data[DMARD09_AXES_NUM])
{
	struct dmard09_i2c_data *priv = i2c_get_clientdata(client);        
	u8 addr = REG_STAT;
	u8 buf[DMARD09_DATA_LEN] = {0};
	int err = 0;
	int i;
	int tmp=0;
	u8 ofs[3];

	if(NULL == client)
	{
		err = -EINVAL;
	}
	else if(err = hwmsen_read_block(client, addr, buf, DMARD09_DATA_LEN))
	{
		GSE_ERR("error: %d\n", err);
	}
	else
	{
		data[DMARD09_AXIS_X] = (s16)((buf[(DMARD09_AXIS_X+1)*2+1] << 8) |
		         (buf[(DMARD09_AXIS_X+1)*2] ));
		data[DMARD09_AXIS_Y] = (s16)((buf[(DMARD09_AXIS_Y+1)*2+1] << 8) |
		         (buf[(DMARD09_AXIS_Y+1)*2] ));
		data[DMARD09_AXIS_Z] = (s16)((buf[(DMARD09_AXIS_Z+1)*2+1] << 8) |
		         (buf[(DMARD09_AXIS_Z+1)*2] ));
		
		if(atomic_read(&priv->trace) & ADX_TRC_RAWDATA)
		{
			GSE_LOG("from register[%08X %08X %08X]\n",data[DMARD09_AXIS_X], data[DMARD09_AXIS_Y], data[DMARD09_AXIS_Z]);
		}

		for(i=0;i<3;i++)				
		{		
			data[i]>>=3;
			data[i]&=0x01ff;
		//because the data is store in binary complement number formation in computer system
			//if ( data[i] == 0x0100 )	//so we want to calculate actual number here
			//	data[i]= -1024;			//11bit resolution, 512= 2^(11-1)
			//else 
				if ( data[i] & 0x0100 )//transfor format , neg num -
			{							//GSE_LOG("data 0 step %x \n",data[i]);
				data[i] -= 0x1;			//GSE_LOG("data 1 step %x \n",data[i]);
				data[i] = ~data[i];		//GSE_LOG("data 2 step %x \n",data[i]);
				data[i] &= 0x00ff;		//GSE_LOG("data 3 step %x \n\n",data[i]);
				data[i] = -data[i];		
			}
			
			  
			  //data[i]<<=3;
			  #ifndef USE_MTK_CALIBRATE
			  data[i] -= priv->offset.v[i]; //D09 raw data - D09 offset
			  #endif
		}	


		if(atomic_read(&priv->trace) & ADX_TRC_RAWDATA)
		{
			GSE_LOG("after transfer[%08X %08X %08X] => [%5d %5d %5d]\n", data[DMARD09_AXIS_X], data[DMARD09_AXIS_Y], data[DMARD09_AXIS_Z],
		                               data[DMARD09_AXIS_X], data[DMARD09_AXIS_Y], data[DMARD09_AXIS_Z]);
		}
#ifdef CONFIG_DMARD09_LOWPASS
		if(atomic_read(&priv->filter))
		{
			if(atomic_read(&priv->fir_en) && !atomic_read(&priv->suspend))
			{
				int idx, firlen = atomic_read(&priv->firlen);   
				if(priv->fir.num < firlen)
				{                
					priv->fir.raw[priv->fir.num][DMARD09_AXIS_X] = data[DMARD09_AXIS_X];
					priv->fir.raw[priv->fir.num][DMARD09_AXIS_Y] = data[DMARD09_AXIS_Y];
					priv->fir.raw[priv->fir.num][DMARD09_AXIS_Z] = data[DMARD09_AXIS_Z];
					priv->fir.sum[DMARD09_AXIS_X] += data[DMARD09_AXIS_X];
					priv->fir.sum[DMARD09_AXIS_Y] += data[DMARD09_AXIS_Y];
					priv->fir.sum[DMARD09_AXIS_Z] += data[DMARD09_AXIS_Z];
					if(atomic_read(&priv->trace) & ADX_TRC_FILTER)
					{
						GSE_LOG("add [%2d] [%5d %5d %5d] => [%5d %5d %5d]\n", priv->fir.num,
							priv->fir.raw[priv->fir.num][DMARD09_AXIS_X], priv->fir.raw[priv->fir.num][DMARD09_AXIS_Y], priv->fir.raw[priv->fir.num][DMARD09_AXIS_Z],
							priv->fir.sum[DMARD09_AXIS_X], priv->fir.sum[DMARD09_AXIS_Y], priv->fir.sum[DMARD09_AXIS_Z]);
					}
					priv->fir.num++;
					priv->fir.idx++;
				}
				else
				{
					idx = priv->fir.idx % firlen;
					priv->fir.sum[DMARD09_AXIS_X] -= priv->fir.raw[idx][DMARD09_AXIS_X];
					priv->fir.sum[DMARD09_AXIS_Y] -= priv->fir.raw[idx][DMARD09_AXIS_Y];
					priv->fir.sum[DMARD09_AXIS_Z] -= priv->fir.raw[idx][DMARD09_AXIS_Z];
					priv->fir.raw[idx][DMARD09_AXIS_X] = data[DMARD09_AXIS_X];
					priv->fir.raw[idx][DMARD09_AXIS_Y] = data[DMARD09_AXIS_Y];
					priv->fir.raw[idx][DMARD09_AXIS_Z] = data[DMARD09_AXIS_Z];
					priv->fir.sum[DMARD09_AXIS_X] += data[DMARD09_AXIS_X];
					priv->fir.sum[DMARD09_AXIS_Y] += data[DMARD09_AXIS_Y];
					priv->fir.sum[DMARD09_AXIS_Z] += data[DMARD09_AXIS_Z];
					priv->fir.idx++;
					data[DMARD09_AXIS_X] = priv->fir.sum[DMARD09_AXIS_X]/firlen;
					data[DMARD09_AXIS_Y] = priv->fir.sum[DMARD09_AXIS_Y]/firlen;
					data[DMARD09_AXIS_Z] = priv->fir.sum[DMARD09_AXIS_Z]/firlen;
					if(atomic_read(&priv->trace) & ADX_TRC_FILTER)
					{
						GSE_LOG("add [%2d] [%5d %5d %5d] => [%5d %5d %5d] : [%5d %5d %5d]\n", idx,
						priv->fir.raw[idx][DMARD09_AXIS_X], priv->fir.raw[idx][DMARD09_AXIS_Y], priv->fir.raw[idx][DMARD09_AXIS_Z],
						priv->fir.sum[DMARD09_AXIS_X], priv->fir.sum[DMARD09_AXIS_Y], priv->fir.sum[DMARD09_AXIS_Z],
						data[DMARD09_AXIS_X], data[DMARD09_AXIS_Y], data[DMARD09_AXIS_Z]);
					}
				}
			}
		}	
#endif         
	}
	return err;
}
/*----------------------------------------------------------------------------*/
static int DMARD09_ReadOffset(struct i2c_client *client, s8 ofs[DMARD09_AXES_NUM])
{    
	int err;
#ifdef SW_CALIBRATION
	ofs[0]=ofs[1]=ofs[2]=0x0;
#else
	
#endif
	//GSE_LOG("offesx=%x, y=%x, z=%x",ofs[0],ofs[1],ofs[2]);
	
	return err;    
}
/*----------------------------------------------------------------------------*/
static int DMARD09_ResetCalibration(struct i2c_client *client)
{
	struct dmard09_i2c_data *obj = i2c_get_clientdata(client);
	u8 ofs[4]={0,0,0,0};
	int err;
	
	#ifdef SW_CALIBRATION
		
	#else
		
	#endif

	memset(obj->cali_sw, 0x00, sizeof(obj->cali_sw));
	memset(obj->offset, 0x00, sizeof(obj->offset));
	#ifndef USE_MTK_CALIBRATE
	memset(obj->offset.v, 0x00, sizeof(obj->offset.v)); /* initialize the offset value */
	#endif
	return err;    
}
/*----------------------------------------------------------------------------*/
static int DMARD09_ReadCalibration(struct i2c_client *client, int dat[DMARD09_AXES_NUM])
{
    struct dmard09_i2c_data *obj = i2c_get_clientdata(client);
    int err;
    int mul;

	#ifdef SW_CALIBRATION
		mul = 0;//only SW Calibration, disable HW Calibration
	#else
	    
	#endif

    dat[obj->cvt.map[DMARD09_AXIS_X]] = obj->cvt.sign[DMARD09_AXIS_X]*(obj->offset[DMARD09_AXIS_X]*mul + obj->cali_sw[DMARD09_AXIS_X]);
    dat[obj->cvt.map[DMARD09_AXIS_Y]] = obj->cvt.sign[DMARD09_AXIS_Y]*(obj->offset[DMARD09_AXIS_Y]*mul + obj->cali_sw[DMARD09_AXIS_Y]);
    dat[obj->cvt.map[DMARD09_AXIS_Z]] = obj->cvt.sign[DMARD09_AXIS_Z]*(obj->offset[DMARD09_AXIS_Z]*mul + obj->cali_sw[DMARD09_AXIS_Z]);                        
                                       
    return 0;
}
/*----------------------------------------------------------------------------*/
static int DMARD09_ReadCalibrationEx(struct i2c_client *client, int act[DMARD09_AXES_NUM], int raw[DMARD09_AXES_NUM])
{  
	/*raw: the raw calibration data; act: the actual calibration data*/
	struct dmard09_i2c_data *obj = i2c_get_clientdata(client);
	int err;
	int mul;

 

	#ifdef SW_CALIBRATION
		mul = 0;//only SW Calibration, disable HW Calibration
	#else
		
	#endif
	
	raw[DMARD09_AXIS_X] = obj->offset[DMARD09_AXIS_X]*mul + obj->cali_sw[DMARD09_AXIS_X];
	raw[DMARD09_AXIS_Y] = obj->offset[DMARD09_AXIS_Y]*mul + obj->cali_sw[DMARD09_AXIS_Y];
	raw[DMARD09_AXIS_Z] = obj->offset[DMARD09_AXIS_Z]*mul + obj->cali_sw[DMARD09_AXIS_Z];

	act[obj->cvt.map[DMARD09_AXIS_X]] = obj->cvt.sign[DMARD09_AXIS_X]*raw[DMARD09_AXIS_X];
	act[obj->cvt.map[DMARD09_AXIS_Y]] = obj->cvt.sign[DMARD09_AXIS_Y]*raw[DMARD09_AXIS_Y];
	act[obj->cvt.map[DMARD09_AXIS_Z]] = obj->cvt.sign[DMARD09_AXIS_Z]*raw[DMARD09_AXIS_Z];                        
	                       
	return 0;
}
/*----------------------------------------------------------------------------*/
static int DMARD09_WriteCalibration(struct i2c_client *client, int dat[DMARD09_AXES_NUM])
{
	struct dmard09_i2c_data *obj = i2c_get_clientdata(client);
	int err;
	int cali[DMARD09_AXES_NUM], raw[DMARD09_AXES_NUM];
	int lsb = dmard09_offset_resolution.sensitivity;
	int divisor = obj->reso->sensitivity/lsb;

	if(err = DMARD09_ReadCalibrationEx(client, cali, raw))	/*offset will be updated in obj->offset*/
	{ 
		GSE_ERR("read offset fail, %d\n", err);
		return err;
	}

	GSE_LOG("OLDOFF: (%+3d %+3d %+3d): (%+3d %+3d %+3d) / (%+3d %+3d %+3d)\n", 
		raw[DMARD09_AXIS_X], raw[DMARD09_AXIS_Y], raw[DMARD09_AXIS_Z],
		obj->offset[DMARD09_AXIS_X], obj->offset[DMARD09_AXIS_Y], obj->offset[DMARD09_AXIS_Z],
		obj->cali_sw[DMARD09_AXIS_X], obj->cali_sw[DMARD09_AXIS_Y], obj->cali_sw[DMARD09_AXIS_Z]);

	/*calculate the real offset expected by caller*/
	cali[DMARD09_AXIS_X] += dat[DMARD09_AXIS_X];
	cali[DMARD09_AXIS_Y] += dat[DMARD09_AXIS_Y];
	cali[DMARD09_AXIS_Z] += dat[DMARD09_AXIS_Z];

	GSE_LOG("UPDATE: (%+3d %+3d %+3d)\n", 
		dat[DMARD09_AXIS_X], dat[DMARD09_AXIS_Y], dat[DMARD09_AXIS_Z]);

#ifdef SW_CALIBRATION
	obj->cali_sw[DMARD09_AXIS_X] = obj->cvt.sign[DMARD09_AXIS_X]*(cali[obj->cvt.map[DMARD09_AXIS_X]]);
	obj->cali_sw[DMARD09_AXIS_Y] = obj->cvt.sign[DMARD09_AXIS_Y]*(cali[obj->cvt.map[DMARD09_AXIS_Y]]);
	obj->cali_sw[DMARD09_AXIS_Z] = obj->cvt.sign[DMARD09_AXIS_Z]*(cali[obj->cvt.map[DMARD09_AXIS_Z]]);	
#else
	obj->offset[DMARD09_AXIS_X] = (s8)(obj->cvt.sign[DMARD09_AXIS_X]*(cali[obj->cvt.map[DMARD09_AXIS_X]])/(divisor));
	obj->offset[DMARD09_AXIS_Y] = (s8)(obj->cvt.sign[DMARD09_AXIS_Y]*(cali[obj->cvt.map[DMARD09_AXIS_Y]])/(divisor));
	obj->offset[DMARD09_AXIS_Z] = (s8)(obj->cvt.sign[DMARD09_AXIS_Z]*(cali[obj->cvt.map[DMARD09_AXIS_Z]])/(divisor));

	/*convert software calibration using standard calibration*/
	obj->cali_sw[DMARD09_AXIS_X] = obj->cvt.sign[DMARD09_AXIS_X]*(cali[obj->cvt.map[DMARD09_AXIS_X]])%(divisor);
	obj->cali_sw[DMARD09_AXIS_Y] = obj->cvt.sign[DMARD09_AXIS_Y]*(cali[obj->cvt.map[DMARD09_AXIS_Y]])%(divisor);
	obj->cali_sw[DMARD09_AXIS_Z] = obj->cvt.sign[DMARD09_AXIS_Z]*(cali[obj->cvt.map[DMARD09_AXIS_Z]])%(divisor);

	GSE_LOG("NEWOFF: (%+3d %+3d %+3d): (%+3d %+3d %+3d) / (%+3d %+3d %+3d)\n", 
		obj->offset[DMARD09_AXIS_X]*divisor + obj->cali_sw[DMARD09_AXIS_X], 
		obj->offset[DMARD09_AXIS_Y]*divisor + obj->cali_sw[DMARD09_AXIS_Y], 
		obj->offset[DMARD09_AXIS_Z]*divisor + obj->cali_sw[DMARD09_AXIS_Z], 
		obj->offset[DMARD09_AXIS_X], obj->offset[DMARD09_AXIS_Y], obj->offset[DMARD09_AXIS_Z],
		obj->cali_sw[DMARD09_AXIS_X], obj->cali_sw[DMARD09_AXIS_Y], obj->cali_sw[DMARD09_AXIS_Z]);

	
#endif

	return err;
}
int hwmsen_read_byte_sr(struct i2c_client *client, u8 addr, u8 *data)
{
   u8 buf;
    int ret = 0;
	
    client->addr = client->addr& I2C_MASK_FLAG | I2C_WR_FLAG |I2C_RS_FLAG;
    buf = addr;
	ret = i2c_master_send(client, (const char*)&buf, 1<<8 | 1);
    //ret = i2c_master_send(client, (const char*)&buf, 1);
    if (ret < 0) {
        GSE_LOG("send command error!!\n");
        return -EFAULT;
    }
     *data = buf;
	client->addr = client->addr& I2C_MASK_FLAG;
    return 0;
}

/*----------------------------------------------------------------------------*/
static int DMARD09_CheckDeviceID(struct i2c_client *client)
{
	u8 databuf[3];    
	int res = 0;
	int retry_count=0;

	memset(databuf, 0, sizeof(u8)*3);
	/* 1. Active Mode  */
	databuf[0] = REG_ACTR;
	databuf[1] = MODE_ACTIVE;
	res = i2c_master_send(client,databuf,2);
	if (res < 0)
	{
	    GSE_ERR("write ACTR fail, %d\n", res);
	    return DMARD09_ERR_I2C;
	}
	/*check REG_ACTR to make sure IC Init ready*/
	while(retry_count<3)
	{
		res = hwmsen_read_block(client, REG_ACTR, databuf, 1);
	    if(res)
		{   
		    GSE_LOG("check REG_ACTR ready bit error, res is %d !!\n",res);
			return DMARD09_ERR_I2C;
		}

		if(databuf[0] & VALUE_INIT_READY)
		{
			GSE_LOG("DMARD09_IC_INIT_DONE!\n");
			break;
		}
		retry_count++;
		mdelay(1);
	}
	if(retry_count>=3)
	{
		GSE_LOG("DMARD09_IC_INIT_FAIL!!!\n");
	}

	/* 2. check D09 who am I */
    res = hwmsen_read_block(client, REG_DC, databuf, 1);
    if(res)
	{   
	    GSE_LOG("DMARD09 check Who am I fail!! res is %d !!\n",res);
		return DMARD09_ERR_I2C;
	}

	if( databuf[0] == VALUE_WHO_AM_I)
	{
		GSE_LOG("D09 WHO_AM_I_VALUE = %d \n", databuf[0]);
		GSE_LOG("DMARD09_CheckDeviceID %d success!\n ", databuf[0]);
	}
	else
	{
		GSE_LOG("DMARD09_CheckDeviceID %d failt!\n ", databuf[0]);
		return DMARD09_ERR_IDENTIFICATION;
	}
	
	/* 3. Set Data conversion rate  use default 100HZ,so do not set*/
	databuf[0] = REG_CNT_L1;
	databuf[1] = VALUE_ODR_100;
	res = i2c_master_send(client, databuf, 1);
	if (res < 0)
	{
	    GSE_ERR("write REG_CNT_L1 fail, %d\n", res);
	    return DMARD09_ERR_I2C;
	}
	
	/* 4. open hardware filter  ,do not sue  REG_ODF is REG_DSP*/
	/*
	buffer[0] = ODF_Ave_4;
	buffer[1] = 0x00;
	res = dmard09_i2c_write_block(client, REG_ODF, databuf, 2);
	if (res < 0)
	{
	    GSE_ERR("write REG_ODF fail, %d\n", res);
	    return DMARD09_ERR_I2C;
	}
	msleep(2);

	res = dmard09_i2c_read_block(client, REG_ODF, databuf, 2);
	GSE_LOG(" REG_ODF = %x , %x\n", databuf[0] , databuf[1]);
	*/
	return DMARD09_SUCCESS;
}
/*----------------------------------------------------------------------------*/
static int DMARD09_SetPowerMode(struct i2c_client *client, bool enable)
{
	u8 databuf[2];    
	int res = 0;	

    if(enable == sensor_power )
	{
		GSE_LOG("Sensor power status is newest!\n");
		return DMARD09_SUCCESS;
	}
	sensor_power = enable;

	mdelay(20);
	
	return DMARD09_SUCCESS;    
}
/*----------------------------------------------------------------------------*/
static int DMARD09_SetDataFormat(struct i2c_client *client, u8 dataformat)
{
	struct dmard09_i2c_data *obj = i2c_get_clientdata(client);
	u8 databuf[10];    
	int res = 0;

	memset(databuf, 0, sizeof(u8)*10);    

	return DMARD09_SetDataResolution(client);    
}
/*----------------------------------------------------------------------------*/
static int DMARD09_SetBWRate(struct i2c_client *client, u8 bwrate)
{
	u8 databuf[10];    
	int res = 0;

	memset(databuf, 0, sizeof(u8)*10);    

	return DMARD09_SUCCESS;    
}
/*----------------------------------------------------------------------------*/
static int DMARD09_SetIntEnable(struct i2c_client *client, u8 intenable)
{
			u8 databuf[10];    
			int res = 0;
		
			
			/*for disable interrupt function*/
			
			return DMARD09_SUCCESS;	  
}

/*----------------------------------------------------------------------------*/
static int dmard09_init_client(struct i2c_client *client, int reset_cali)
{
	struct dmard09_i2c_data *obj = i2c_get_clientdata(client);
	int res = 0;
	static bool firsttime=true;

    res = DMARD09_CheckDeviceID(client); 
	if(res != DMARD09_SUCCESS)
	{
		return res;
	}	
	DMARD09_SetDataResolution(client);
	res = DMARD09_SetPowerMode(client, false);
	if(res != DMARD09_SUCCESS)
	{
		return res;
	}
	GSE_LOG("DMARD09_SetPowerMode OK!\n");
	
	gsensor_gain.x = gsensor_gain.y = gsensor_gain.z = obj->reso->sensitivity;

	if(0 != reset_cali)
	{ 
		/*reset calibration only in power on*/
		res = DMARD09_ResetCalibration(client);
		if(res != DMARD09_SUCCESS)
		{
			return res;
		}
	}
	#ifndef USE_MTK_CALIBRATE
	if(firsttime){
		D09_read_offset_from_file(client);	
	 	firsttime=false;
	}
	#endif
	GSE_LOG("dmard09_init_client OK!\n");
#ifdef CONFIG_DMARD09_LOWPASS
	memset(&obj->fir, 0x00, sizeof(obj->fir));  
#endif

	mdelay(20);

	return DMARD09_SUCCESS;
}
/*----------------------------------------------------------------------------*/
static int DMARD09_ReadChipInfo(struct i2c_client *client, char *buf, int bufsize)
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

	sprintf(buf, "DMARD09 Chip");
	return 0;
}
/*----------------------------------------------------------------------------*/
static int DMARD09_ReadSensorData(struct i2c_client *client, char *buf, int bufsize)
{
	struct dmard09_i2c_data *obj = (struct dmard09_i2c_data*)i2c_get_clientdata(client);
	u8 databuf[20];
	int acc[DMARD09_AXES_NUM];
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
		res = DMARD09_SetPowerMode(client, true);
		if(res)
		{
			GSE_ERR("Power on DMARD09 error %d!\n", res);
		}
	}

	if(res = DMARD09_ReadData(client, obj->data))
	{        
		GSE_ERR("I2C error: ret value=%d", res);
		return -3;
	}
	else
	{
		//GSE_LOG("raw data x=%d, y=%d, z=%d \n",obj->data[DMARD09_AXIS_X],obj->data[DMARD09_AXIS_Y],obj->data[DMARD09_AXIS_Z]);
		obj->data[DMARD09_AXIS_X] += obj->cali_sw[DMARD09_AXIS_X];
		obj->data[DMARD09_AXIS_Y] += obj->cali_sw[DMARD09_AXIS_Y];
		obj->data[DMARD09_AXIS_Z] += obj->cali_sw[DMARD09_AXIS_Z];
		
		//GSE_LOG("cali_sw x=%d, y=%d, z=%d \n",obj->cali_sw[BMA150_AXIS_X],obj->cali_sw[BMA150_AXIS_Y],obj->cali_sw[BMA150_AXIS_Z]);
		
		/*remap coordinate*/
		acc[obj->cvt.map[DMARD09_AXIS_X]] = obj->cvt.sign[DMARD09_AXIS_X]*obj->data[DMARD09_AXIS_X];
		acc[obj->cvt.map[DMARD09_AXIS_Y]] = obj->cvt.sign[DMARD09_AXIS_Y]*obj->data[DMARD09_AXIS_Y];
		acc[obj->cvt.map[DMARD09_AXIS_Z]] = obj->cvt.sign[DMARD09_AXIS_Z]*obj->data[DMARD09_AXIS_Z];
		//GSE_LOG("cvt x=%d, y=%d, z=%d \n",obj->cvt.sign[BMA150_AXIS_X],obj->cvt.sign[BMA150_AXIS_Y],obj->cvt.sign[BMA150_AXIS_Z]);


		//GSE_LOG("Mapped gsensor data: %d, %d, %d!\n", acc[BMA150_AXIS_X], acc[BMA150_AXIS_Y], acc[BMA150_AXIS_Z]);

		//Out put the mg
		//GSE_LOG("mg acc=%d, GRAVITY=%d, sensityvity=%d \n",acc[BMA150_AXIS_X],GRAVITY_EARTH_1000,obj->reso->sensitivity);
		acc[DMARD09_AXIS_X] = acc[DMARD09_AXIS_X] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
		acc[DMARD09_AXIS_Y] = acc[DMARD09_AXIS_Y] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
		acc[DMARD09_AXIS_Z] = acc[DMARD09_AXIS_Z] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;		
		
	

		sprintf(buf, "%04x %04x %04x", acc[DMARD09_AXIS_X], acc[DMARD09_AXIS_Y], acc[DMARD09_AXIS_Z]);
		if(atomic_read(&obj->trace) & ADX_TRC_IOCTL)
		{
			GSE_LOG("gsensor data: %s!\n", buf);
		}
	}
	
	return 0;
}
/*----------------------------------------------------------------------------*/
static int DMARD09_ReadRawData(struct i2c_client *client, char *buf)
{
	struct dmard09_i2c_data *obj = (struct dmard09_i2c_data*)i2c_get_clientdata(client);
	int res = 0;

	if (!buf || !client)
	{
		return EINVAL;
	}
	
	if(res = DMARD09_ReadData(client, obj->data))
	{        
		GSE_ERR("I2C error: ret value=%d", res);
		return EIO;
	}
	else
	{
		sprintf(buf, "DMARD09_ReadRawData %04x %04x %04x", obj->data[DMARD09_AXIS_X], 
			obj->data[DMARD09_AXIS_Y], obj->data[DMARD09_AXIS_Z]);
	
	}
	
	return 0;
}
/*----------------------------------------------------------------------------*/
static int DMARD09_InitSelfTest(struct i2c_client *client)
{
	int res = 0;

	DMARD09_SetPowerMode(client, true);

	
	
	return DMARD09_SUCCESS;
}
/*----------------------------------------------------------------------------*/
/*
static int DMARD09_JudgeTestResult(struct i2c_client *client)
{

	struct dmard09_i2c_data *obj = (struct dmard09_i2c_data*)i2c_get_clientdata(client);
	int res = 0;
	s16  acc[DMARD09_AXES_NUM];
	int  self_result;

	
	if(res = DMARD09_ReadData(client, acc))
	{        
		GSE_ERR("I2C error: ret value=%d", res);
		return EIO;
	}
	else
	{			
		GSE_LOG("0 step: %d %d %d\n", acc[0],acc[1],acc[2]);

		acc[DMARD09_AXIS_X] = acc[DMARD09_AXIS_X] * 1000 / 128;
		acc[DMARD09_AXIS_Y] = acc[DMARD09_AXIS_Y] * 1000 / 128;
		acc[DMARD09_AXIS_Z] = acc[DMARD09_AXIS_Z] * 1000 / 128;
		
		GSE_LOG("1 step: %d %d %d\n", acc[0],acc[1],acc[2]);
		
		self_result = acc[DMARD09_AXIS_X]*acc[DMARD09_AXIS_X] 
			+ acc[DMARD09_AXIS_Y]*acc[DMARD09_AXIS_Y] 
			+ acc[DMARD09_AXIS_Z]*acc[DMARD09_AXIS_Z];
			
		
		GSE_LOG("2 step: result = %d", self_result);

	    if ( (self_result>550000) && (self_result<1700000) ) //between 0.55g and 1.7g 
	    {												 
			GSE_ERR("DMARD09_JudgeTestResult successful\n");
			return DMARD09_SUCCESS;
		}
		{
	        GSE_ERR("DMARD09_JudgeTestResult failt\n");
	        return -EINVAL;
	    }
	
	}
	
}
*/
/*----------------------------------------------------------------------------*/
static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = dmard09_i2c_client;
	char strbuf[DMARD09_BUFSIZE];
	if(NULL == client)
	{
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}
	
	DMARD09_ReadChipInfo(client, strbuf, DMARD09_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);        
}

static ssize_t gsensor_init(struct device_driver *ddri, char *buf, size_t count)
	{
		struct i2c_client *client = dmard09_i2c_client;
		char strbuf[DMARD09_BUFSIZE];
		
		if(NULL == client)
		{
			GSE_ERR("i2c client is null!!\n");
			return 0;
		}
		dmard09_init_client(client, 1);
		return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);			
	}



/*----------------------------------------------------------------------------*/
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = dmard09_i2c_client;
	char strbuf[DMARD09_BUFSIZE];
	
	if(NULL == client)
	{
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}
	DMARD09_ReadSensorData(client, strbuf, DMARD09_BUFSIZE);
	//BMA150_ReadRawData(client, strbuf);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);            
}

static ssize_t show_sensorrawdata_value(struct device_driver *ddri, char *buf, size_t count)
	{
		struct i2c_client *client = dmard09_i2c_client;
		char strbuf[DMARD09_BUFSIZE];
		
		if(NULL == client)
		{
			GSE_ERR("i2c client is null!!\n");
			return 0;
		}
		DMARD09_ReadRawData(client, strbuf);
		return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);			
	}

/*----------------------------------------------------------------------------*/
static ssize_t show_cali_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = dmard09_i2c_client;
	struct dmard09_i2c_data *obj;
	int err, len = 0, mul;
	int tmp[DMARD09_AXES_NUM];

	if(NULL == client)
	{
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	obj = i2c_get_clientdata(client);



	if(err = DMARD09_ReadOffset(client, obj->offset))
	{
		return -EINVAL;
	}
	else if(err = DMARD09_ReadCalibration(client, tmp))
	{
		return -EINVAL;
	}
	else
	{    
		mul = obj->reso->sensitivity/dmard09_offset_resolution.sensitivity;
		len += snprintf(buf+len, PAGE_SIZE-len, "[HW ][%d] (%+3d, %+3d, %+3d) : (0x%02X, 0x%02X, 0x%02X)\n", mul,                        
			obj->offset[DMARD09_AXIS_X], obj->offset[DMARD09_AXIS_Y], obj->offset[DMARD09_AXIS_Z],
			obj->offset[DMARD09_AXIS_X], obj->offset[DMARD09_AXIS_Y], obj->offset[DMARD09_AXIS_Z]);
		len += snprintf(buf+len, PAGE_SIZE-len, "[SW ][%d] (%+3d, %+3d, %+3d)\n", 1, 
			obj->cali_sw[DMARD09_AXIS_X], obj->cali_sw[DMARD09_AXIS_Y], obj->cali_sw[DMARD09_AXIS_Z]);

		len += snprintf(buf+len, PAGE_SIZE-len, "[ALL]    (%+3d, %+3d, %+3d) : (%+3d, %+3d, %+3d)\n", 
			obj->offset[DMARD09_AXIS_X]*mul + obj->cali_sw[DMARD09_AXIS_X],
			obj->offset[DMARD09_AXIS_Y]*mul + obj->cali_sw[DMARD09_AXIS_Y],
			obj->offset[DMARD09_AXIS_Z]*mul + obj->cali_sw[DMARD09_AXIS_Z],
			tmp[DMARD09_AXIS_X], tmp[DMARD09_AXIS_Y], tmp[DMARD09_AXIS_Z]);
		
		return len;
    }
}
/*----------------------------------------------------------------------------*/
static ssize_t store_cali_value(struct device_driver *ddri, char *buf, size_t count)
{
	struct i2c_client *client = dmard09_i2c_client;  
	int err, x, y, z;
	int dat[DMARD09_AXES_NUM];

	if(!strncmp(buf, "rst", 3))
	{
		if(err = DMARD09_ResetCalibration(client))
		{
			GSE_ERR("reset offset err = %d\n", err);
		}	
	}
	else if(3 == sscanf(buf, "0x%02X 0x%02X 0x%02X", &x, &y, &z))
	{
		dat[DMARD09_AXIS_X] = x;
		dat[DMARD09_AXIS_Y] = y;
		dat[DMARD09_AXIS_Z] = z;
		if(err = DMARD09_WriteCalibration(client, dat))
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
/*
static ssize_t show_self_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = dmard09_i2c_client;
	struct dmard09_i2c_data *obj;

	if(NULL == client)
	{
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	//obj = i2c_get_clientdata(client);
	
    return snprintf(buf, 8, "%s\n", selftestRes);
}
*/
/*----------------------------------------------------------------------------*/
/*
static ssize_t store_self_value(struct device_driver *ddri, char *buf, size_t count)
{   
	struct item{
	s16 raw[DMARD09_AXES_NUM];
	};
	
	struct i2c_client *client = dmard09_i2c_client;  
	int idx, res, num;
	struct item *prv = NULL, *nxt = NULL;


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
	DMARD09_SetPowerMode(client,true); 

	
	DMARD09_InitSelfTest(client);
	GSE_LOG("SELFTEST:\n");    

	if(!DMARD09_JudgeTestResult(client))
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
	   
	dmard09_init_client(client, 0);
	kfree(prv);
	kfree(nxt);
	return count;
}
*/
/*----------------------------------------------------------------------------*/
/*
static ssize_t show_selftest_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = dmard09_i2c_client;
	struct dmard09_i2c_data *obj;

	if(NULL == client)
	{
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	obj = i2c_get_clientdata(client);
	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&obj->selftest));
}
*/
/*----------------------------------------------------------------------------*/
/*
static ssize_t store_selftest_value(struct device_driver *ddri, char *buf, size_t count)
{
	struct dmard09_i2c_data *obj = obj_i2c_data;
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
			
			dmard09_init_client(obj->client, 0);
		}
		else if(!atomic_read(&obj->selftest) && tmp)
		{
			
			DMARD09_InitSelfTest(obj->client);            
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
*/
/*----------------------------------------------------------------------------*/
static ssize_t show_firlen_value(struct device_driver *ddri, char *buf)
{
#ifdef CONFIG_DMARD09_LOWPASS
	struct i2c_client *client = dmard09_i2c_client;
	struct dmard09_i2c_data *obj = i2c_get_clientdata(client);
	if(atomic_read(&obj->firlen))
	{
		int idx, len = atomic_read(&obj->firlen);
		GSE_LOG("len = %2d, idx = %2d\n", obj->fir.num, obj->fir.idx);

		for(idx = 0; idx < len; idx++)
		{
			GSE_LOG("[%5d %5d %5d]\n", obj->fir.raw[idx][DMARD09_AXIS_X], obj->fir.raw[idx][DMARD09_AXIS_Y], obj->fir.raw[idx][DMARD09_AXIS_Z]);
		}
		
		GSE_LOG("sum = [%5d %5d %5d]\n", obj->fir.sum[DMARD09_AXIS_X], obj->fir.sum[DMARD09_AXIS_Y], obj->fir.sum[DMARD09_AXIS_Z]);
		GSE_LOG("avg = [%5d %5d %5d]\n", obj->fir.sum[DMARD09_AXIS_X]/len, obj->fir.sum[DMARD09_AXIS_Y]/len, obj->fir.sum[DMARD09_AXIS_Z]/len);
	}
	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&obj->firlen));
#else
	return snprintf(buf, PAGE_SIZE, "not support\n");
#endif
}
/*----------------------------------------------------------------------------*/
static ssize_t store_firlen_value(struct device_driver *ddri, char *buf, size_t count)
{
#ifdef CONFIG_DMARD09_LOWPASS
	struct i2c_client *client = dmard09_i2c_client;  
	struct dmard09_i2c_data *obj = i2c_get_clientdata(client);
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
	struct dmard09_i2c_data *obj = obj_i2c_data;
	if (obj == NULL)
	{
		GSE_ERR("i2c_data obj is null!!\n");
		return 0;
	}
	
	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));     
	return res;    
}
/*----------------------------------------------------------------------------*/
static ssize_t store_trace_value(struct device_driver *ddri, char *buf, size_t count)
{
	struct dmard09_i2c_data *obj = obj_i2c_data;
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
	struct dmard09_i2c_data *obj = obj_i2c_data;
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
		GSE_LOG("G sensor is in work mode, sensor_power = %d\n", sensor_power);
	else
		GSE_LOG("G sensor is in standby mode, sensor_power = %d\n", sensor_power);

	return 0;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_layout_value(struct device_driver *ddri, char *buf)
{
	struct dmard09_i2c_data *data = obj_i2c_data;

	return sprintf(buf, "(%d, %d)\n[%+2d %+2d %+2d]\n[%+2d %+2d %+2d]\n",
		data->hw->direction,atomic_read(&data->layout),	data->cvt.sign[0], data->cvt.sign[1],
		data->cvt.sign[2],data->cvt.map[0], data->cvt.map[1], data->cvt.map[2]);            
}
/*----------------------------------------------------------------------------*/
static ssize_t store_layout_value(struct device_driver *ddri, char *buf, size_t count)
{
	struct dmard09_i2c_data *data = obj_i2c_data;
	int layout = 0;

	if(1 == sscanf(buf, "%d", &layout))
	{
		atomic_set(&data->layout, layout);
		if(!hwmsen_get_convert(layout, &data->cvt))
		{
			GSE_LOG("HWMSEN_GET_CONVERT function error!\r\n");
		}
		else if(!hwmsen_get_convert(data->hw->direction, &data->cvt))
		{
			GSE_LOG("invalid layout: %d, restore to %d\n", layout, data->hw->direction);
		}
		else
		{
			GSE_LOG("invalid layout: (%d, %d)\n", layout, data->hw->direction);
			hwmsen_get_convert(0, &data->cvt);
		}
	}
	else
	{
		GSE_LOG("invalid format = '%s'\n", buf);
	}
	
	return count;            
}

/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(chipinfo,   S_IWUSR | S_IRUGO, show_chipinfo_value,      NULL);
static DRIVER_ATTR(sensordata, S_IWUSR | S_IRUGO, show_sensordata_value,    NULL);
static DRIVER_ATTR(cali,       S_IWUSR | S_IRUGO, show_cali_value,          store_cali_value);
//static DRIVER_ATTR(selftest, S_IWUSR | S_IRUGO, show_self_value,  store_self_value);
//static DRIVER_ATTR(self,   S_IWUSR | S_IRUGO, show_selftest_value,      store_selftest_value);
static DRIVER_ATTR(firlen,     S_IWUSR | S_IRUGO, show_firlen_value,        store_firlen_value);
static DRIVER_ATTR(trace,      S_IWUSR | S_IRUGO, show_trace_value,         store_trace_value);
static DRIVER_ATTR(status,               S_IRUGO, show_status_value,        NULL);
static DRIVER_ATTR(powerstatus,               S_IRUGO, show_power_status_value,        NULL);
static DRIVER_ATTR(layout,      S_IRUGO | S_IWUSR, show_layout_value, store_layout_value);


/*----------------------------------------------------------------------------*/
static struct driver_attribute *dmard09_attr_list[] = {
	&driver_attr_chipinfo,     /*chip information*/
	&driver_attr_sensordata,   /*dump sensor data*/
	&driver_attr_cali,         /*show calibration data*/
//	&driver_attr_self,         /*self test demo*/
//	&driver_attr_selftest,     /*self control: 0: disable, 1: enable*/
	&driver_attr_firlen,       /*filter length: 0: disable, others: enable*/
	&driver_attr_trace,        /*trace log*/
	&driver_attr_status,
	&driver_attr_powerstatus,
	&driver_attr_layout,
};
/*----------------------------------------------------------------------------*/
static int dmard09_create_attr(struct device_driver *driver) 
{
	int idx, err = 0;
	int num = (int)(sizeof(dmard09_attr_list)/sizeof(dmard09_attr_list[0]));
	if (driver == NULL)
	{
		return -EINVAL;
	}

	for(idx = 0; idx < num; idx++)
	{
		if(err = driver_create_file(driver, dmard09_attr_list[idx]))
		{            
			GSE_ERR("driver_create_file (%s) = %d\n", dmard09_attr_list[idx]->attr.name, err);
			break;
		}
	}    
	return err;
}
/*----------------------------------------------------------------------------*/
static int dmard09_delete_attr(struct device_driver *driver)
{
	int idx ,err = 0;
	int num = (int)(sizeof(dmard09_attr_list)/sizeof(dmard09_attr_list[0]));

	if(driver == NULL)
	{
		return -EINVAL;
	}
	

	for(idx = 0; idx < num; idx++)
	{
		driver_remove_file(driver, dmard09_attr_list[idx]);
	}
	

	return err;
}

/*----------------------------------------------------------------------------*/
int gsensor_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value, sample_delay;	
	struct dmard09_i2c_data *priv = (struct dmard09_i2c_data*)self;
	hwm_sensor_data* gsensor_data;
	char buff[DMARD09_BUFSIZE];
	
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
				

				if(value >= 50)
				{
					atomic_set(&priv->filter, 0);
				}
				else
				{	
				#if defined(CONFIG_DMARD09_LOWPASS)
					priv->fir.num = 0;
					priv->fir.idx = 0;
					priv->fir.sum[DMARD09_AXIS_X] = 0;
					priv->fir.sum[DMARD09_AXIS_Y] = 0;
					priv->fir.sum[DMARD09_AXIS_Z] = 0;
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
					GSE_LOG("Gsensor device have updated! sensor_power is %d \n",sensor_power);
				}
				else
				{
					err = DMARD09_SetPowerMode( priv->client, !sensor_power);
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
				DMARD09_ReadSensorData(priv->client, buff, DMARD09_BUFSIZE);
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
#ifndef USE_MTK_CALIBRATE
int D09_calibrate(struct i2c_client *client)
{	
	struct dmard09_i2c_data *obj = (struct dmard09_i2c_data*)i2c_get_clientdata(client);

	raw_data avg;
	int i, j;
	long xyz_acc[SENSOR_DATA_SIZE];   
  	s16 xyz[SENSOR_DATA_SIZE];
	/* initialize the offset value */
	for(i = 0; i < SENSOR_DATA_SIZE; ++i)
		obj->offset.v[i] = 0;
	/* initialize the accumulation buffer */
  	for(i = 0; i < SENSOR_DATA_SIZE; ++i) 
		xyz_acc[i] = 0;

	for(i = 0; i < AVG_NUM; i++) 
	{      
		DMARD09_ReadData(client, obj->data);
		for(j = 0; j < SENSOR_DATA_SIZE; ++j) 
			xyz_acc[j] += obj->data[j];
  	}
	/* calculate averages */
  	for(i = 0; i < SENSOR_DATA_SIZE; ++i) 
		avg.v[i] = xyz_acc[i] / AVG_NUM;
		
	if(avg.v[2] < 0)
	{
		obj->offset.v[0] =  avg.v[0] ;    
		obj->offset.v[1] =  avg.v[1] ;
		ovj->offset.v[2] =  avg.v[2] + DEFAULT_SENSITIVITY;
	}
	else
	{	
		obj->offset.v[0] =  avg.v[0] ;    
		obj->offset.v[1] =  avg.v[1] ;
		obj->offset.v[2] =  avg.v[2] - DEFAULT_SENSITIVITY;
	}
	return 0;
}
#endif
/****************************************************************************** 
 * Function Configuration
******************************************************************************/
static int dmard09_open(struct inode *inode, struct file *file)
{
	file->private_data = dmard09_i2c_client;

	if(file->private_data == NULL)
	{
		GSE_ERR("null pointer!!\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}
/*----------------------------------------------------------------------------*/
static int dmard09_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}
/*----------------------------------------------------------------------------*/
//static int dmard09_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
  //     unsigned long arg)
static int dmard09_unlocked_ioctl(struct file *file, unsigned int cmd,
       unsigned long arg)
{
	struct i2c_client *client = (struct i2c_client*)file->private_data;
	struct dmard09_i2c_data *obj = (struct dmard09_i2c_data*)i2c_get_clientdata(client);	
	char strbuf[DMARD09_BUFSIZE];
	void __user *data;
	SENSOR_DATA sensor_data;
	int err = 0;
	int cali[3];
	//int intBuf[SENSOR_DATA_SIZE];
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
			dmard09_init_client(client, 0);			
			break;

		case GSENSOR_IOCTL_READ_CHIPINFO:
			data = (void __user *) arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}
			
			DMARD09_ReadChipInfo(client, strbuf, DMARD09_BUFSIZE);
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
			
			DMARD09_ReadSensorData(client, strbuf, DMARD09_BUFSIZE);
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
			DMARD09_ReadRawData(client, strbuf);
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
				cali[DMARD09_AXIS_X] = sensor_data.x * obj->reso->sensitivity / GRAVITY_EARTH_1000;
				cali[DMARD09_AXIS_Y] = sensor_data.y * obj->reso->sensitivity / GRAVITY_EARTH_1000;
				cali[DMARD09_AXIS_Z] = sensor_data.z * obj->reso->sensitivity / GRAVITY_EARTH_1000;			  
				err = DMARD09_WriteCalibration(client, cali);			 
			}
			break;

		case GSENSOR_IOCTL_CLR_CALI:
			err = DMARD09_ResetCalibration(client);
			break;

		case GSENSOR_IOCTL_GET_CALI:
			data = (void __user*)arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}
			if(err = DMARD09_ReadCalibration(client, cali))
			{
				break;
			}
			
			sensor_data.x = cali[DMARD09_AXIS_X] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
			sensor_data.y = cali[DMARD09_AXIS_Y] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
			sensor_data.z = cali[DMARD09_AXIS_Z] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
			if(copy_to_user(data, &sensor_data, sizeof(sensor_data)))
			{
				err = -EFAULT;
				break;
			}		
			break;
		#ifndef USE_MTK_CALIBRATE
		case SENSOR_CALIBRATION:
			err = D09_calibrate(client);
			GSE_LOG("Sensor_calibration:%d %d %d\n", s_dmt->offset.u.x, s_dmt->offset.u.y, s_dmt->offset.u.z);
			/* save file */
			D09_write_offset_to_file(client, OffsetFileName);
			
			/* return the offset */
			for(i = 0; i < SENSOR_DATA_SIZE; ++i)
				intBuf[i] = (int)obj->offset.v[i];

			copy_to_user((int *)arg, &intBuf, sizeof(intBuf));
			break;
		#endif
		default:
			GSE_ERR("unknown IOCTL: 0x%08x\n", cmd);
			err = -ENOIOCTLCMD;
			break;
			
	}

	return err;
}


/*----------------------------------------------------------------------------*/
static struct file_operations dmard09_fops = {
	//.owner = THIS_MODULE,
	.open = dmard09_open,
	.release = dmard09_release,
	.unlocked_ioctl = dmard09_unlocked_ioctl,
};
/*----------------------------------------------------------------------------*/
static struct miscdevice dmard09_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gsensor",
	.fops = &dmard09_fops,
};
/*----------------------------------------------------------------------------*/
#ifndef CONFIG_HAS_EARLYSUSPEND
/*----------------------------------------------------------------------------*/
static int dmard09_suspend(struct i2c_client *client, pm_message_t msg) 
{
	struct dmard09_i2c_data *obj = i2c_get_clientdata(client);    
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
		if(err = DMARD09_SetPowerMode(obj->client, false))
		{
			GSE_ERR("write power control fail!!\n");
			return;
		}       
		DMARD09_power(obj->hw, 0);
	}
	return err;
}
/*----------------------------------------------------------------------------*/
static int dmard09_resume(struct i2c_client *client)
{
	struct dmard09_i2c_data *obj = i2c_get_clientdata(client);        
	int err;
	GSE_FUN();

	if(obj == NULL)
	{
		GSE_ERR("null pointer!!\n");
		return -EINVAL;
	}

	DMARD09_power(obj->hw, 1);
	if(err = dmard09_init_client(client, 0))
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
static void dmard09_early_suspend(struct early_suspend *h) 
{
	struct dmard09_i2c_data *obj = container_of(h, struct dmard09_i2c_data, early_drv);   
	int err;
	GSE_FUN();    

	if(obj == NULL)
	{
		GSE_ERR("null pointer!!\n");
		return;
	}
	atomic_set(&obj->suspend, 1); 
	if(err = DMARD09_SetPowerMode(obj->client, false))
	{
		GSE_ERR("write power control fail!!\n");
		return;
	}

	sensor_power = false;
	
	DMARD09_power(obj->hw, 0);
}
/*----------------------------------------------------------------------------*/
static void dmard09_late_resume(struct early_suspend *h)
{
	struct dmard09_i2c_data *obj = container_of(h, struct dmard09_i2c_data, early_drv);         
	int err;
	GSE_FUN();

	if(obj == NULL)
	{
		GSE_ERR("null pointer!!\n");
		return;
	}

	DMARD09_power(obj->hw, 1);
	if(err = dmard09_init_client(obj->client, 0))
	{
		GSE_ERR("initialize client fail!!\n");
		return;        
	}
	atomic_set(&obj->suspend, 0);    
}
/*----------------------------------------------------------------------------*/
#endif /*CONFIG_HAS_EARLYSUSPEND*/
/*----------------------------------------------------------------------------*/
static int dmard09_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info) 
{    
	strcpy(info->type, DMARD09_DEV_NAME);
	return 0;
}

/*----------------------------------------------------------------------------*/
static int dmard09_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_client *new_client;
	struct dmard09_i2c_data *obj;
	struct hwmsen_object sobj;
	int err = 0;
	GSE_FUN();

	if(!(obj = kzalloc(sizeof(*obj), GFP_KERNEL)))
	{
		err = -ENOMEM;
		goto exit;
	}
	
	memset(obj, 0, sizeof(struct dmard09_i2c_data));

	obj->hw = get_cust_acc_hw();
	
	if(err = hwmsen_get_convert(obj->hw->direction, &obj->cvt))
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
	
#ifdef CONFIG_DMARD09_LOWPASS
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

	dmard09_i2c_client = new_client;	

	if(err = dmard09_init_client(new_client, 1))
	{
		goto exit_init_failed;
	}
	

	if(err = misc_register(&dmard09_device))
	{
		GSE_ERR("dmard09_device register failed\n");
		goto exit_misc_device_register_failed;
	}

	if(err = dmard09_create_attr(&dmard09_gsensor_driver.driver))
	{
		GSE_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}

	sobj.self = obj;
    sobj.polling = 1;
    sobj.sensor_operate = gsensor_operate;
	if(err = hwmsen_attach(ID_ACCELEROMETER, &sobj))
	{
		GSE_ERR("attach fail = %d\n", err);
		goto exit_kfree;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	obj->early_drv.level    = EARLY_SUSPEND_LEVEL_DISABLE_FB - 1,
	obj->early_drv.suspend  = dmard09_early_suspend,
	obj->early_drv.resume   = dmard09_late_resume,    
	register_early_suspend(&obj->early_drv);
#endif 

	GSE_LOG("%s: OK\n", __func__);    
	return 0;

	exit_create_attr_failed:
	misc_deregister(&dmard09_device);
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
static int dmard09_i2c_remove(struct i2c_client *client)
{
	int err = 0;	
	
	if(err = dmard09_delete_attr(&dmard09_gsensor_driver.driver))
	{
		GSE_ERR("dmard09_delete_attr fail: %d\n", err);
	}
	
	if(err = misc_deregister(&dmard09_device))
	{
		GSE_ERR("misc_deregister fail: %d\n", err);
	}

	if(err = hwmsen_detach(ID_ACCELEROMETER))
	    

	dmard09_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));
	return 0;
}
/*----------------------------------------------------------------------------*/
static int dmard09_probe(struct platform_device *pdev) 
{
	struct acc_hw *hw = get_cust_acc_hw();
	GSE_FUN();

	DMARD09_power(hw, 1);
	//dmard09_force[0] = hw->i2c_num;
	if(i2c_add_driver(&dmard09_i2c_driver))
	{
		GSE_ERR("add driver error\n");
		return -1;
	}
	return 0;
}

#ifndef USE_MTK_CALIBRATE
void D09_write_offset_to_file(struct i2c_client *client , char * offset)
{
	struct dmard09_i2c_data *obj = (struct dmard09_i2c_data*)i2c_get_clientdata(client);
	char data[18];
	mm_segment_t orgfs;
	struct file *fp;

	sprintf(data,"%5d %5d %5d", obj->offset.v[0], obj->offset.v[1], obj->offset.v[2]);
	fp = filp_open(offset, O_RDWR | O_CREAT, 0666);
	if(IS_ERR(fp))
		GSE_ERR("filp_open %s error!!.\n",offset);
	else
	{
		orgfs = get_fs();
		/* Set segment descriptor associated to kernel space */
		set_fs(KERNEL_DS);
		GSE_LOG("filp_open %s SUCCESS!!.\n",offset);
		fp->f_op->write(fp,data,18, &fp->f_pos);
 		filp_close(fp,NULL);
	}
	set_fs(orgfs);
}

void D09_read_offset_from_file(struct i2c_client *client)
{
	struct dmard09_i2c_data *obj = (struct dmard09_i2c_data*)i2c_get_clientdata(client);
	unsigned int orgfs;
	char data[18];
	struct file *fp , *fs;/* *fp open gsensor_offset.txt , *fs open dmt_sensor.txt */
	s16 ux,uy,uz;
	orgfs = get_fs();
	/* Set segment descriptor associated to kernel space */
	set_fs(KERNEL_DS);

	fp = filp_open(OffsetFileName, O_RDWR , 0);
	if(IS_ERR(fp))
	{
		GSE_ERR("Sorry,filp_open ERROR !\n");
		D09_write_offset_to_file(client , OffsetFileName);
#ifdef AUTO_CALIBRATION
/* get acceleration average reading */
			D09_calibrate(client);
#endif
#ifdef DMT_BROADCAST_APK_ENABLE
		D09_write_offset_to_file(client , DmtXXFileName);
#endif
	}
	else{
		GSE_LOG("filp_open %s SUCCESS!!.\n",OffsetFileName);
		fp->f_op->read( fp, data, 18, &fp->f_pos); // read gsensor_offset.txt
		GSE_LOG("filp_read result %s\n",data);
		sscanf(data,"%d %d %d",&ux,&uy,&uz);
		if(ux == 0 && uy == 0 && uz == 0)
		{
			filp_close(fp,NULL);// close gsensor_offset.txt
			fs = filp_open(DmtXXFileName, O_RDWR , 0); // open dmt_sensor.txt
			fs->f_op->read( fs, data, 18, &fs->f_pos); // read dmt_sensor.txt
			GSE_LOG("filp_read result %s\n", data);
			GSE_FUN();
			sscanf(data,"%d %d %d",&ux,&uy,&uz);
			obj->offset.u.x=ux;
			obj->offset.u.y=uy;
			obj->offset.u.z=uz;
			GSE_LOG("ux/uy/uz = %d/%d/%d\n",ux,uy,uz);
			filp_close(fs,NULL);
		}	
		else
		{

			obj->offset.u.x=ux;
			obj->offset.u.y=uy;
			obj->offset.u.z=uz;
			GSE_LOG("ux/uy/uz = %d/%d/%d\n",ux,uy,uz);
			filp_close(fp,NULL);
		}
	}
	set_fs(orgfs);
}
#endif
/*----------------------------------------------------------------------------*/
static int dmard09_remove(struct platform_device *pdev)
{
    struct acc_hw *hw = get_cust_acc_hw();

    GSE_FUN();    
    DMARD09_power(hw, 0);    
    i2c_del_driver(&dmard09_i2c_driver);
    return 0;
}
/*----------------------------------------------------------------------------*/
static struct platform_driver dmard09_gsensor_driver = {
	.probe      = dmard09_probe,
	.remove     = dmard09_remove,    
	.driver     = {
		.name  = "gsensor",
		//.owner = THIS_MODULE,
	}
};

/*----------------------------------------------------------------------------*/
static int __init dmard09_init(void)
{
	GSE_FUN();
	struct acc_hw *hw = get_cust_acc_hw();
	GSE_LOG("%s: i2c_number=%d\n", __func__,hw->i2c_num); 
	i2c_register_board_info(hw->i2c_num, &i2c_dmard09, 1);
	if(platform_driver_register(&dmard09_gsensor_driver))
	{
		GSE_ERR("failed to register driver");
		return -ENODEV;
	}
	return 0;    
}
/*----------------------------------------------------------------------------*/
static void __exit dmard09_exit(void)
{
	GSE_FUN();
	platform_driver_unregister(&dmard09_gsensor_driver);
}
/*----------------------------------------------------------------------------*/
module_init(dmard09_init);
module_exit(dmard09_exit);
/*----------------------------------------------------------------------------*/
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("DMARD09 I2C driver");
MODULE_AUTHOR("Zhilin.Chen@mediatek.com");
