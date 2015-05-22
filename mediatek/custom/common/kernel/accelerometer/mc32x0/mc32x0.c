/******************************************************************************
Mcube Inc. (C) 2010
*****************************************************************************/

/*****************************************************************************
 *
 * Filename:
 * ---------
 *   MC32X0.c
 *
 * Project:
 * --------
 *   Mcube acceleration sensor
 *
 * Description:
 * ------------
 *   This file implements basic dirver for MTK android
 *
 * Author:
 * -------
 * Tan Liang
 ****************************************************************************/
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
#include "mc32x0.h"
#include <linux/hwmsen_helper.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>



#define POWER_NONE_MACRO MT65XX_POWER_NONE




/*----------------------------------------------------------------------------*/
#define DEBUG 1
/*----------------------------------------------------------------------------*/
#define CONFIG_MC32X0_LOWPASS   /*apply low pass filter on output*/       
/*----------------------------------------------------------------------------*/
#define MC32X0_AXIS_X          0
#define MC32X0_AXIS_Y          1
#define MC32X0_AXIS_Z          2
#define MC32X0_AXES_NUM        3
#define MC32X0_DATA_LEN        6
#define MC32X0_DEV_NAME        "MC32X0"
/*----------------------------------------------------------------------------*/
#define MC32X0_2G //MC32X0_8G_14BIT

#ifdef MC32X0_8G_14BIT
#define MC32X0_Sensitivity 1024
#endif

#ifdef MC32X0_2G
#define MC32X0_Sensitivity 256
#endif

#ifdef MC32X0_4G
#define MC32X0_Sensitivity 128
#endif

#ifdef MC32X0_8G
#define MC32X0_Sensitivity 64
#endif

#define CALIB_PATH              "/data/data/com.mcube.acc/files/mcube-calib.txt"
#define DATA_PATH              "/sdcard/mcube-register-map.txt"

//#define SELF_VERIFICATION
#define GSENSOR_MCUBE_IOCMD 0

#define _OFFSET_GAIN_SIZE     9
#define _OFFSET_14BIT_MASK    0x3FFF
#define _OFFSET_SIGN_BIT      0x2000
#define _GAIN_UPPER_BIT       0x8000
#define _DFLT_GAIN_NUM        128
#define _ACCEL_G_TO_MG        1000
#define _OFFSET_G_RANGE       12
#define _OFFSET_LSB_RNG       16384
#define _MAX_OFFSET           8191
#define _MIN_OFFSET           -8192

#define _TWO_BYTES_TO_OFFSET(a,b)                        ((b << 8) | (a))
#define _MG_TO_ACCEL_OFFSET(val)                         (((val) * _OFFSET_LSB_RNG) / (_OFFSET_G_RANGE * _ACCEL_G_TO_MG))
#define _ACCEL_LSB_TO_MG(val)                            (((val) * (_OFFSET_G_RANGE * _ACCEL_G_TO_MG) / (_OFFSET_LSB_RNG)))
#define _OFFSET_GAIN_ADJUST(val, numerator, gain)        (((val) * (numerator))/(40 + (gain)))
#define _INV_OFFSET_GAIN_ADJUST(val, numerator, gain)    (((val) * (40 + (gain)))/(numerator))

#if defined(GSENSOR_MCUBE_IOCMD)
#define GSENSOR_MCUBE_IOCTL_READ_RBM_DATA		_IOR(GSENSOR, 0x09, SENSOR_DATA)
#define GSENSOR_MCUBE_IOCTL_SET_RBM_MODE		_IOW(GSENSOR, 0x0a, int)
#define GSENSOR_MCUBE_IOCTL_CLEAR_RBM_MODE		_IO(GSENSOR, 0x0b)
#define GSENSOR_MCUBE_IOCTL_SET_CALI			_IOW(GSENSOR, 0x0c, SENSOR_DATA)
#define GSENSOR_MCUBE_IOCTL_REGISTER_MAP		_IO(GSENSOR, 0x0d)
#define GSENSOR_IOCTL_SET_CALI_MODE   			_IOW(GSENSOR, 0x0e,int)
#define GSENSOR_MCUBE_IOCTL_READ_OFFSET   			_IOR(GSENSOR, 0x0f,SENSOR_DATA)
#define GSENSOR_MCUBE_IOCTL_SET_OFFSET   			_IOW(GSENSOR, 0x10,SENSOR_DATA)
#endif
static const struct i2c_device_id mc32x0_i2c_id[] = {{MC32X0_DEV_NAME,0},{}};
static struct i2c_board_info __initdata i2c_mc32x0={ I2C_BOARD_INFO("MC32X0", 0x4C)};
/*the adapter id will be available in customization*/
//static unsigned short mc32x0_force[] = {0x00, MC32X0_I2C_SLAVE_ADDR, I2C_CLIENT_END, I2C_CLIENT_END};
//static const unsigned short *const mc32x0_forces[] = { mc32x0_force, NULL };
//static struct i2c_client_address_data mc32x0_addr_data = { .forces = mc32x0_forces,};

/*----------------------------------------------------------------------------*/
static int mc32x0_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id); 
static int mc32x0_i2c_remove(struct i2c_client *client);
static int mc32x0_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);
static int mc32x0_suspend(struct i2c_client *client, pm_message_t msg) ;
static int mc32x0_resume(struct i2c_client *client);

/*----------------------------------------------------------------------------*/
static int MC32X0_SetPowerMode(struct i2c_client *client, bool enable);

#define IS_MC3230 1
#define IS_MC3210 2

static unsigned char mc32x0_type;
/*------------------------------------------------------------------------------*/
typedef enum {
    MCUBE_TRC_FILTER  = 0x01,
    MCUBE_TRC_RAWDATA = 0x02,
    MCUBE_TRC_IOCTL   = 0x04,
    MCUBE_TRC_CALI	= 0X08,
    MCUBE_TRC_INFO	= 0X10,
    MCUBE_TRC_REGXYZ	= 0X20,
} MCUBE_TRC;
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
    s16 raw[C_MAX_FIR_LENGTH][MC32X0_AXES_NUM];
    int sum[MC32X0_AXES_NUM];
    int num;
    int idx;
};
/*----------------------------------------------------------------------------*/
struct mc32x0_i2c_data {
    struct i2c_client *client;
    struct acc_hw *hw;
    struct hwmsen_convert   cvt;
    
    /*misc*/
    struct data_resolution *reso;
    atomic_t                trace;
    atomic_t                suspend;
    atomic_t                selftest;
	atomic_t				filter;
    s16                     cali_sw[MC32X0_AXES_NUM+1];

    /*data*/
    s16                     offset[MC32X0_AXES_NUM+1];  /*+1: for 4-byte alignment*/
    s16                     data[MC32X0_AXES_NUM+1];

#if defined(CONFIG_MC32X0_LOWPASS)
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
static struct i2c_driver mc32x0_i2c_driver = {
    .driver = {
        //.owner          = THIS_MODULE,
        .name           = MC32X0_DEV_NAME,
    },
	.probe      		= mc32x0_i2c_probe,
	.remove    			= mc32x0_i2c_remove,
	//.detect				= mc32x0_i2c_detect,
//#if !defined(CONFIG_HAS_EARLYSUSPEND)    
    .suspend            = mc32x0_suspend,
    .resume             = mc32x0_resume,
//#endif
	.id_table = mc32x0_i2c_id,
	//.address_data = &mc32x0_addr_data,
};

/*----------------------------------------------------------------------------*/
static struct i2c_client *mc32x0_i2c_client = NULL;
static struct platform_driver mc32x0_gsensor_driver;
static struct mc32x0_i2c_data *obj_i2c_data = NULL;
static bool sensor_power = false;
static GSENSOR_VECTOR3D gsensor_gain, gsensor_offset;
static char selftestRes[10] = {0};

static int fd_file = -1;
static int load_cali_flg = 0;
static mm_segment_t oldfs;
//add by Liang for storage offset data
static unsigned char offset_buf[6]; 
static signed int offset_data[3];
static signed int gain_data[3];
static signed int enable_RBM_calibration = 0;
//end add by Liang

static int LPF_data = 5;

static unsigned int iAReal0_X ;
static unsigned int iAcc0Lpf0_X ;
static unsigned int iAcc0Lpf1_X ;
static unsigned int iAcc1Lpf0_X ;
static unsigned int iAcc1Lpf1_X ;

static unsigned int iAReal0_Y ;
static unsigned int iAcc0Lpf0_Y ;
static unsigned int iAcc0Lpf1_Y ;
static unsigned int iAcc1Lpf0_Y ;
static unsigned int iAcc1Lpf1_Y ;

static unsigned int iAReal0_Z ;
static unsigned int iAcc0Lpf0_Z ;
static unsigned int iAcc0Lpf1_Z ;
static unsigned int iAcc1Lpf0_Z ;
static unsigned int iAcc1Lpf1_Z ;
// LPF add by Liang
#ifdef SELF_VERIFICATION
static int Verify_Counts; 
#endif
/*----------------------------------------------------------------------------*/
#if 0
#define GSE_TAG                  "[Gsensor] "
#define GSE_FUN(f)               printk(KERN_INFO GSE_TAG"%s\n", __FUNCTION__)
#define GSE_ERR(fmt, args...)    printk(KERN_ERR GSE_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define GSE_LOG(fmt, args...)    printk(KERN_INFO GSE_TAG fmt, ##args)
#else
#define GSE_TAG
#define GSE_FUN(f)               do {} while (0)
#define GSE_ERR(fmt, args...)    do {} while (0)
#define GSE_LOG(fmt, args...)    do {} while (0)
#endif
/*----------------------------------------------------------------------------*/
static struct data_resolution mc32x0_data_resolution[] = {
 /*8 combination by {FULL_RES,RANGE}*/
    {{15, 6},  256},   /*+/-2g  in 10-bit resolution: 15.6 mg/LSB*/
    {{ 15, 6}, 64},   /*+/-2g  in 8-bit resolution:  /LSB*/         
};
/*----------------------------------------------------------------------------*/
static struct data_resolution mc32x0_offset_resolution = {{7, 8}, 256};

static int MC32X0_WriteCalibration(struct i2c_client *client, int dat[MC32X0_AXES_NUM]);

unsigned int GetLowPassFilter(unsigned int X0,unsigned int Y1)
{
    unsigned int lTemp;
    lTemp=Y1;
    lTemp*=0x00000004;		// 4HZ LPF RC=0.04
    X0*=LPF_data;		
    lTemp+=X0;
    lTemp+=0x00000004;
    lTemp/=0x0000004 + LPF_data;		
    Y1=lTemp;
    return Y1;
}

/*
float InvSqrt (float x)
{
        float xhalf = 0.5f*x;
        int i = *(int*)&x;
        i = 0x5f3759df - (i >> 1);        // ËÆ°Á?Á¨¨‰?‰∏™Ë?‰ººÊ†π
        x = *(float*)&i;
        x = x*(1.5f - xhalf*x*x);       // ?õÈ°øËø≠‰ª£Ê≥?
        return x;
}


float SqrtByBisection(float n)
{
	    //Â∞è‰?0?ÑÊ??ß‰??ÄË¶ÅÁ?Â§ÑÁ?
	    if(n < 0)
	        return n;
	    float mid,last;
	    float low,up;
	    low=0,up=n;
	    mid=(low+up)/2;
	    do
	    {
	        if(mid*mid>n)
	            up=mid;
	        else
	            low=mid;
	        last=mid;
	        mid=(up+low)/2;
	    }
	    //Á≤æÂ∫¶?ßÂà∂
	    while(abs(mid-last) > eps);
	    return mid;
}*/

#ifdef SELF_VERIFICATION
int Verify_Z_Axis_Data(int X, int Y, int Z, int Sensitivity)
{
	static char status =0; 
	int temp, max, min;
	float i;
	
	temp = Sensitivity*Sensitivity - X*X - Y*Y - Z*Z;
	max = (Sensitivity/3*4)*(Sensitivity/3*4)- Sensitivity*Sensitivity;
	min = (Sensitivity/3*2)*(Sensitivity/3*2)- Sensitivity*Sensitivity;

	//printk(" Verify_Z_Axis_Data %d %d %d \n\r" ,temp ,max ,min);
	if(( (temp > max) || (temp < min)  ) && (Verify_Counts < 10) && (status == 0))
	{
		Verify_Counts++;
	}
	else if(((min <temp) && (temp < max)) && (Verify_Counts < 10) && (status == 1))
	{
		Verify_Counts++;
	}

	if( Verify_Counts >= 10 && status == 0)
	{
		status = 1;
		Verify_Counts =0;
	}
	else if( Verify_Counts >= 10 && status == 1)
	{
		status = 0;
		Verify_Counts =0;
	}
	//i = InvSqrt(temp);
	return status;
}
#endif
struct file *McopenFile(char *path,int flag,int mode) 
{ 
	struct file *fp; 
	 
	fp=filp_open(path, flag, mode); 
	if (IS_ERR(fp) || !fp->f_op) 
	{
		GSE_LOG("Calibration File filp_open return NULL\n");
		return NULL; 
	}
	else 
	{

		return fp; 
	}
} 
 
int McreadFile(struct file *fp,char *buf,int readlen) 
{ 
	if (fp->f_op && fp->f_op->read) 
		return fp->f_op->read(fp,buf,readlen, &fp->f_pos); 
	else 
		return -1; 
} 

int McwriteFile(struct file *fp,char *buf,int writelen) 
{ 
	if (fp->f_op && fp->f_op->write) 
		return fp->f_op->write(fp,buf,writelen, &fp->f_pos); 
	else 
		return -1; 
}
 
int MccloseFile(struct file *fp) 
{ 
	filp_close(fp,NULL); 
	return 0; 
} 
void McinitKernelEnv(void) 
{ 
	oldfs = get_fs(); 
	set_fs(KERNEL_DS);
	printk(KERN_INFO "McinitKernelEnv\n");
} 


static int mcube_read_cali_file(struct i2c_client *client)
{
	struct mc32x0_i2c_data *obj = i2c_get_clientdata(client);
	int cali_data[3],cali_data1[3];
	int err =0;
	char buf[64];

	McinitKernelEnv();
	fd_file = McopenFile(CALIB_PATH,O_RDONLY,0); 
	if (fd_file == NULL) 
	{
		GSE_LOG("fail to open\n");
		cali_data[0] = 0;
		cali_data[1] = 0;
		cali_data[2] = 0;	
	}
	else
	{
		memset(buf,0,64); 
		if ((err = McreadFile(fd_file,buf,128))>0) 
			GSE_LOG("buf:%s\n",buf); 
		else 
			GSE_LOG("read file error %d\n",err); 

		set_fs(oldfs); 
		MccloseFile(fd_file); 

		sscanf(buf, "%d %d %d",&cali_data[MC32X0_AXIS_X], &cali_data[MC32X0_AXIS_Y], &cali_data[MC32X0_AXIS_Z]);
		GSE_LOG("cali_data: %d %d %d\n", cali_data[MC32X0_AXIS_X], cali_data[MC32X0_AXIS_Y], cali_data[MC32X0_AXIS_Z]); 	
				
		cali_data1[MC32X0_AXIS_X] = cali_data[MC32X0_AXIS_X] * gsensor_gain.x / GRAVITY_EARTH_1000;
		cali_data1[MC32X0_AXIS_Y] = cali_data[MC32X0_AXIS_Y] * gsensor_gain.y / GRAVITY_EARTH_1000;
		cali_data1[MC32X0_AXIS_Z] = cali_data[MC32X0_AXIS_Z] * gsensor_gain.z / GRAVITY_EARTH_1000;

		GSE_LOG("cali_data1: %d %d %d\n", cali_data1[MC32X0_AXIS_X], cali_data1[MC32X0_AXIS_Y], cali_data1[MC32X0_AXIS_Z]); 	
			  
		MC32X0_WriteCalibration(client, cali_data1);
	}
	return 0;
}

static int mcube_write_log_data(struct i2c_client *client, u8 data[0x3f])
{
	struct mc32x0_i2c_data *obj = i2c_get_clientdata(client);
	int cali_data[3],cali_data1[3];
	s16 rbm_data[3], raw_data[3];
	int err =0;
	char buf[66*50];
	int n=0,i=0;

	McinitKernelEnv();
	fd_file = McopenFile(DATA_PATH ,O_RDWR | O_CREAT,0); 
	if (fd_file == NULL) 
	{
		GSE_LOG("mcube_write_log_data fail to open\n");	
	}
	else
	{
		rbm_data[MC32X0_AXIS_X] = (s16)((data[0x0d]) | (data[0x0e] << 8));
		rbm_data[MC32X0_AXIS_Y] = (s16)((data[0x0f]) | (data[0x10] << 8));
		rbm_data[MC32X0_AXIS_Z] = (s16)((data[0x11]) | (data[0x12] << 8));

		raw_data[MC32X0_AXIS_X] = (rbm_data[MC32X0_AXIS_X] + offset_data[0]/2)*gsensor_gain.x/gain_data[0];
		raw_data[MC32X0_AXIS_Y] = (rbm_data[MC32X0_AXIS_Y] + offset_data[1]/2)*gsensor_gain.y/gain_data[1];
		raw_data[MC32X0_AXIS_Z] = (rbm_data[MC32X0_AXIS_Z] + offset_data[2]/2)*gsensor_gain.z/gain_data[2];

		
		memset(buf,0,66*50); 
		n += sprintf(buf+n, "G-sensor RAW X = %d  Y = %d  Z = %d\n", raw_data[0] ,raw_data[1] ,raw_data[2]);
		n += sprintf(buf+n, "G-sensor RBM X = %d  Y = %d  Z = %d\n", rbm_data[0] ,rbm_data[1] ,rbm_data[2]);
		for(i=0; i<64; i++)
		{
		n += sprintf(buf+n, "mCube register map Register[%x] = 0x%x\n",i,data[i]);
		}
		msleep(50);		
		if ((err = McwriteFile(fd_file,buf,n))>0) 
			GSE_LOG("buf:%s\n",buf); 
		else 
			GSE_LOG("write file error %d\n",err); 

		set_fs(oldfs); 
		MccloseFile(fd_file); 
	}
	return 0;
}
static int hwmsen_read_byte_sr(struct i2c_client *client, u8 addr, u8 *data)
{
   u8 buf;
    int ret = 0;
	
    client->addr = client->addr& I2C_MASK_FLAG | I2C_WR_FLAG |I2C_RS_FLAG;
    buf = addr;
	ret = i2c_master_send(client, (const char*)&buf, 1<<8 | 1);
    //ret = i2c_master_send(client, (const char*)&buf, 1);
    if (ret < 0) {
        HWM_ERR("send command error!!\n");
        return -EFAULT;
    }

    *data = buf;
	client->addr = client->addr& I2C_MASK_FLAG;
    return 0;
}

static void dumpReg(struct i2c_client *client)
{
  int i=0;
  u8 addr = 0x00;
  u8 regdata=0;
  for(i=0; i<49 ; i++)
  {
    //dump all
    hwmsen_read_byte_sr(client,addr,&regdata);
	HWM_LOG("Reg addr=%x regdata=%x\n",addr,regdata);
	addr++;
	if(addr ==01)
		addr=addr+0x06;
	if(addr==0x09)
		addr++;
	if(addr==0x0A)
		addr++;
  }
}

int hwmsen_read_block_sr(struct i2c_client *client, u8 addr, u8 *data)
{
   u8 buf[10];
    int ret = 0;
	memset(buf, 0, sizeof(u8)*10); 
	
    client->addr = client->addr& I2C_MASK_FLAG | I2C_WR_FLAG |I2C_RS_FLAG;
    buf[0] = addr;
	ret = i2c_master_send(client, (const char*)&buf, 6<<8 | 1);
    //ret = i2c_master_send(client, (const char*)&buf, 1);
    if (ret < 0) {
        HWM_ERR("send command error!!\n");
        return -EFAULT;
    }

    *data = buf;
	client->addr = client->addr& I2C_MASK_FLAG;
    return 0;
}

static void MC32X0_power(struct acc_hw *hw, unsigned int on) 
{
	static unsigned int power_on = 0;

	if(hw->power_id != MT65XX_POWER_NONE)		// have externel LDO
	{        
		GSE_LOG("power %s\n", on ? "on" : "off");
		if(power_on == on)	// power status not change
		{
			GSE_LOG("ignore power control: %d\n", on);
		}
		else if(on)	// power on
		{
			if(!hwPowerOn(hw->power_id, hw->power_vol, "MC32X0"))
			{
				GSE_ERR("power on fails!!\n");
			}
		}
		else	// power off
		{
			if (!hwPowerDown(hw->power_id, "MC32X0"))
			{
				GSE_ERR("power off fail!!\n");
			}			  
		}
	}
	power_on = on;    
}
/*----------------------------------------------------------------------------*/

static void MC32X0_rbm(struct i2c_client *client, int enable)
{
	u8 buf1[3];
	int err; 

	if(enable == 1 )
	{
		buf1[0] = 0x43; 
		err = hwmsen_write_block(client, 0x07, buf1, 0x01);

		buf1[0] = 0x02; 
		err = hwmsen_write_block(client, 0x14, buf1, 0x01);

		buf1[0] = 0x41; 
		err = hwmsen_write_block(client, 0x07, buf1, 0x01);

		enable_RBM_calibration =1;
		
		GSE_LOG("set rbm!!\n");

		msleep(10);
	}
	else if(enable == 0 )  
	{
		buf1[0] = 0x43; 
		err = hwmsen_write_block(client, 0x07, buf1, 0x01);

		buf1[0] = 0x00; 
		err = hwmsen_write_block(client, 0x14, buf1, 0x01);

		buf1[0] = 0x41; 
		err = hwmsen_write_block(client, 0x07, buf1, 0x01);
	
		enable_RBM_calibration =0;

		GSE_LOG("clear rbm!!\n");

		msleep(10);
	}
}

/*----------------------------------------------------------------------------*/

static int MC32X0_ReadData_RBM(struct i2c_client *client, int data[MC32X0_AXES_NUM])
{
	struct mc32x0_i2c_data *priv = i2c_get_clientdata(client);        
	//u8 uData;
	u8 addr = 0x0d;
	u8 buf[MC32X0_DATA_LEN] = {0};
	u8 buf1[MC32X0_DATA_LEN] = {0};
	u8 rbm_buf[MC32X0_DATA_LEN] = {0};
	int err = 0;
	int i;

	if(NULL == client)
	{
		err = -EINVAL;
		return err;
	}


	
/********************************************/
	
	err = hwmsen_read_block(client, addr, rbm_buf, 0x06);

	data[MC32X0_AXIS_X] = (s16)((rbm_buf[0]) | (rbm_buf[1] << 8));
	data[MC32X0_AXIS_Y] = (s16)((rbm_buf[2]) | (rbm_buf[3] << 8));
	data[MC32X0_AXIS_Z] = (s16)((rbm_buf[4]) | (rbm_buf[5] << 8));

	GSE_LOG("rbm_buf<<<<<[%02x %02x %02x %02x %02x %02x]\n",rbm_buf[0], rbm_buf[2], rbm_buf[2], rbm_buf[3], rbm_buf[4], rbm_buf[5]);
	GSE_LOG("RBM<<<<<[%04x %04x %04x]\n", data[MC32X0_AXIS_X], data[MC32X0_AXIS_Y], data[MC32X0_AXIS_Z]);
	GSE_LOG("RBM<<<<<[%04d %04d %04d]\n", data[MC32X0_AXIS_X], data[MC32X0_AXIS_Y], data[MC32X0_AXIS_Z]);


/********************************************/

		

     
	
	return err;
}



static int MC32X0_Read_Reg_Map(struct i2c_client *client)
{
	struct mc32x0_i2c_data *priv = i2c_get_clientdata(client);        
	u8 data[128];
	u8 addr = 0x00;
	int err = 0;
	int i;

	if(NULL == client)
	{
		err = -EINVAL;
		return err;
	}


	
/********************************************/
	
	err = hwmsen_read_block(client, addr, data, 0x06);
	addr +=6;
	err = hwmsen_read_block(client, addr, data+6, 0x06);
	addr +=6;
	err = hwmsen_read_block(client, addr, data+12, 0x06);
	addr +=6;
	err = hwmsen_read_block(client, addr, data+18, 0x06);
	addr +=6;
	err = hwmsen_read_block(client, addr, data+24, 0x06);
	addr +=6;
	err = hwmsen_read_block(client, addr, data+30, 0x06);
	addr +=6;
	err = hwmsen_read_block(client, addr, data+36, 0x06);
	addr +=6;
	err = hwmsen_read_block(client, addr, data+42, 0x06);
	addr +=6;
	err = hwmsen_read_block(client, addr, data+48, 0x06);
	addr +=6;
	err = hwmsen_read_block(client, addr, data+54, 0x06);
	addr +=6;
	err = hwmsen_read_block(client, addr, data+60, 0x06);

	for(i = 0; i<64; i++)
	{
		printk(KERN_INFO "mcube register map Register[%x] = 0x%x\n", i ,data[i]);
	}

	msleep(50);	
	
	mcube_write_log_data(client, data);

	msleep(50);
     	
	return err;
}

/*----------------------------------------------------------------------------*/
static int MC32X0_ReadData(struct i2c_client *client, s16 data[MC32X0_AXES_NUM])
{
	struct mc32x0_i2c_data *priv = i2c_get_clientdata(client);        
	//u8 uData;
	u8 addr = 0x0d;
	u8 buf[MC32X0_DATA_LEN] = {0};
	u8 buf1[MC32X0_DATA_LEN] = {0};
	u8 rbm_buf[MC32X0_DATA_LEN] = {0};
	int err = 0;
	int i;

	if(NULL == client)
	{
		err = -EINVAL;
		return err;
	}

	if ( enable_RBM_calibration == 0)
	{
		//err = hwmsen_read_block(client, addr, buf, 0x06);
	}
	else if (enable_RBM_calibration == 1)
	{


		err = hwmsen_read_block(client, addr, rbm_buf, 0x06);


	}

	/*	
        if ((err = hwmsen_read_block(client, MC32X0_XOUT_EX_L_REG, buf, MC32X0_DATA_LEN))) 
        {
    	   GSE_ERR("error: %d\n", err);
    	   return err;
        }*/
	
/********************************************/
	if ( enable_RBM_calibration == 0)
	{
		if ( mc32x0_type == IS_MC3230 )
		{
			if ((err = hwmsen_read_block(client, 0, buf, 3))) 
			{
				GSE_ERR("error: %d\n", err);
				return err;
			}


			data[MC32X0_AXIS_X] = (s8)buf[0];
			data[MC32X0_AXIS_Y] = (s8)buf[1];
			data[MC32X0_AXIS_Z] = (s8)buf[2];	

			#ifdef SELF_VERIFICATION
			if ( Verify_Z_Axis_Data(data[MC32X0_AXIS_X], data[MC32X0_AXIS_Y], data[MC32X0_AXIS_Z], 86))
			data[MC32X0_AXIS_Z] = 0;
			#endif

			GSE_LOG("fwq read MC32X0_data =%d %d %d in %s \n",data[MC32X0_AXIS_X],data[MC32X0_AXIS_Y],data[MC32X0_AXIS_Z],__FUNCTION__);

			if(atomic_read(&priv->trace) & MCUBE_TRC_REGXYZ)
			{
				GSE_LOG("raw from reg(SR) [%08X %08X %08X] => [%5d %5d %5d]\n", data[MC32X0_AXIS_X], data[MC32X0_AXIS_Y], data[MC32X0_AXIS_Z],
						data[MC32X0_AXIS_X], data[MC32X0_AXIS_Y], data[MC32X0_AXIS_Z]);
			}
		}
		else if ( mc32x0_type == IS_MC3210 )
		{
			if ((err = hwmsen_read_block(client, MC32X0_XOUT_EX_L_REG, buf, MC32X0_DATA_LEN))) 
			{
				GSE_ERR("error: %d\n", err);
				return err;
			}


			data[MC32X0_AXIS_X] = ((s16)(buf[0]))|((s16)(buf[1])<<8);
			data[MC32X0_AXIS_Y] = ((s16)(buf[2]))|((s16)(buf[3])<<8);
			data[MC32X0_AXIS_Z] = ((s16)(buf[4]))|((s16)(buf[5])<<8);

			#ifdef SELF_VERIFICATION
			if ( Verify_Z_Axis_Data(data[MC32X0_AXIS_X], data[MC32X0_AXIS_Y], data[MC32X0_AXIS_Z], 1024))
			data[MC32X0_AXIS_Z] = 0;
			#endif	   


			GSE_LOG("fwq read MC32X0_data =%d %d %d in %s \n",data[MC32X0_AXIS_X],data[MC32X0_AXIS_Y],data[MC32X0_AXIS_Z],__FUNCTION__);

			if(atomic_read(&priv->trace) & MCUBE_TRC_REGXYZ)
			{
				GSE_LOG("raw from reg(SR) [%08X %08X %08X] => [%5d %5d %5d]\n", data[MC32X0_AXIS_X], data[MC32X0_AXIS_Y], data[MC32X0_AXIS_Z],
						data[MC32X0_AXIS_X], data[MC32X0_AXIS_Y], data[MC32X0_AXIS_Z]);
			}
		}


		GSE_LOG("RAW<<<<<[%08d %08d %08d]\n", data[MC32X0_AXIS_X], data[MC32X0_AXIS_Y], data[MC32X0_AXIS_Z]);
	}
	else if (enable_RBM_calibration == 1)
	{
		data[MC32X0_AXIS_X] = (s16)((rbm_buf[0]) | (rbm_buf[1] << 8));
		data[MC32X0_AXIS_Y] = (s16)((rbm_buf[2]) | (rbm_buf[3] << 8));
		data[MC32X0_AXIS_Z] = (s16)((rbm_buf[4]) | (rbm_buf[5] << 8));

		GSE_LOG("RBM<<<<<[%08d %08d %08d]\n", data[MC32X0_AXIS_X], data[MC32X0_AXIS_Y], data[MC32X0_AXIS_Z]);

		//data[MC32X0_AXIS_X] = (data[MC32X0_AXIS_X] + offset_data[0]/2)*gsensor_gain.x/gain_data[0];
		//data[MC32X0_AXIS_Y] = (data[MC32X0_AXIS_Y] + offset_data[1]/2)*gsensor_gain.y/gain_data[1];
		//data[MC32X0_AXIS_Z] = (data[MC32X0_AXIS_Z] + offset_data[2]/2)*gsensor_gain.z/gain_data[2];

		data[MC32X0_AXIS_X] = (data[MC32X0_AXIS_X] + offset_data[0]/2)*1024/gain_data[0]+8096;
       		data[MC32X0_AXIS_Y] = (data[MC32X0_AXIS_Y] + offset_data[1]/2)*1024/gain_data[1]+8096;
		data[MC32X0_AXIS_Z] = (data[MC32X0_AXIS_Z] + offset_data[2]/2)*1024/gain_data[2]+8096;

		GSE_LOG("RBM->RAW <<<<<[%08d %08d %08d]\n", data[MC32X0_AXIS_X], data[MC32X0_AXIS_Y], data[MC32X0_AXIS_Z]);

		iAReal0_X = 0x0010*data[MC32X0_AXIS_X];
    		iAcc1Lpf0_X   = GetLowPassFilter(iAReal0_X,iAcc1Lpf1_X);
    		iAcc0Lpf0_X   = GetLowPassFilter(iAcc1Lpf0_X,iAcc0Lpf1_X);
    		data[MC32X0_AXIS_X] = iAcc0Lpf0_X /0x0010;

		iAReal0_Y = 0x0010*data[MC32X0_AXIS_Y];
    		iAcc1Lpf0_Y   = GetLowPassFilter(iAReal0_Y,iAcc1Lpf1_Y);
    		iAcc0Lpf0_Y   = GetLowPassFilter(iAcc1Lpf0_Y,iAcc0Lpf1_Y);
    		data[MC32X0_AXIS_Y] = iAcc0Lpf0_Y/0x0010;

		iAReal0_Z = 0x0010*data[MC32X0_AXIS_Z];
    		iAcc1Lpf0_Z   = GetLowPassFilter(iAReal0_Z,iAcc1Lpf1_Z);
    		iAcc0Lpf0_Z   = GetLowPassFilter(iAcc1Lpf0_Z,iAcc0Lpf1_Z);
    		data[MC32X0_AXIS_Z] = iAcc0Lpf0_Z/0x0010;

		GSE_LOG("RBM->RAW->LPF <<<<<[%08d %08d %08d]\n", data[MC32X0_AXIS_X], data[MC32X0_AXIS_Y], data[MC32X0_AXIS_Z]);	

		data[MC32X0_AXIS_X] = (data[MC32X0_AXIS_X]-8096)*gsensor_gain.x/1024;
		data[MC32X0_AXIS_Y] = (data[MC32X0_AXIS_Y]-8096)*gsensor_gain.y/1024;
		data[MC32X0_AXIS_Z] = (data[MC32X0_AXIS_Z]-8096)*gsensor_gain.z/1024;

		GSE_LOG("RBM->RAW->LPF->RAW <<<<<[%08d %08d %08d]\n", data[MC32X0_AXIS_X], data[MC32X0_AXIS_Y], data[MC32X0_AXIS_Z]);

	

		iAcc0Lpf1_X=iAcc0Lpf0_X;
    		iAcc1Lpf1_X=iAcc1Lpf0_X;

		iAcc0Lpf1_Y=iAcc0Lpf0_Y;
    		iAcc1Lpf1_Y=iAcc1Lpf0_Y;

		iAcc0Lpf1_Z=iAcc0Lpf0_Z;
    		iAcc1Lpf1_Z=iAcc1Lpf0_Z;

	}

		//remove by Liang for calibrate by writing offset regesiter
		//data[MC32X0_AXIS_X] += priv->cali_sw[MC32X0_AXIS_X];
		//data[MC32X0_AXIS_Y] += priv->cali_sw[MC32X0_AXIS_Y];
		//data[MC32X0_AXIS_Z] += priv->cali_sw[MC32X0_AXIS_Z];


/********************************************/

		

     
	
	return err;
}
/*----------------------------------------------------------------------------*/
static int MC32X0_ReadOffset(struct i2c_client *client, s16 ofs[MC32X0_AXES_NUM])
{    
	int err;
	u8 off_data[6];
	

	if ( mc32x0_type == IS_MC3210 )
	{
		if ((err = hwmsen_read_block(client, MC32X0_XOUT_EX_L_REG, off_data, MC32X0_DATA_LEN))) 
    		{
    			GSE_ERR("error: %d\n", err);
    			return err;
    		}
		ofs[MC32X0_AXIS_X] = ((s16)(off_data[0]))|((s16)(off_data[1])<<8);
		ofs[MC32X0_AXIS_Y] = ((s16)(off_data[2]))|((s16)(off_data[3])<<8);

		ofs[MC32X0_AXIS_Z] = ((s16)(off_data[4]))|((s16)(off_data[5])<<8);
	}
	else if (mc32x0_type == IS_MC3230) 
	{
		if ((err = hwmsen_read_block(client, 0, off_data, 3))) 
    		{
    			GSE_ERR("error: %d\n", err);
    			return err;
    		}
		ofs[MC32X0_AXIS_X] = (s8)off_data[0];
		ofs[MC32X0_AXIS_Y] = (s8)off_data[1];
		ofs[MC32X0_AXIS_Z] = (s8)off_data[2];			
	}

	GSE_LOG("MC32X0_ReadOffset %d %d %d \n",ofs[MC32X0_AXIS_X] ,ofs[MC32X0_AXIS_Y],ofs[MC32X0_AXIS_Z]);

    return 0;  
}
/*----------------------------------------------------------------------------*/
static int MC32X0_ResetCalibration(struct i2c_client *client)
{
	struct mc32x0_i2c_data *obj = i2c_get_clientdata(client);
	u8 buf[MC32X0_AXES_NUM] = {0x00, 0x00, 0x00};
	s16 tmp;
	int err;
		
		buf[0] = 0x43;
		if(err = hwmsen_write_block(client, 0x07, buf, 1))
		{
			GSE_ERR("error 0x07: %d\n", err);
		}


		if(err = hwmsen_write_block(client, 0x21, offset_buf, 6)) // add by liang for writing offset register as OTP value 
		{
			GSE_ERR("error: %d\n", err);
		}
	
		buf[0] = 0x41;
		if(err = hwmsen_write_block(client, 0x07, buf, 1))
		{
			GSE_ERR("error: %d\n", err);
		}
		msleep(20);

		tmp = ((offset_buf[1] & 0x3f) << 8) + offset_buf[0];  // add by Liang for set offset_buf as OTP value 
		if (tmp & 0x2000)
			tmp |= 0xc000;
		offset_data[0] = tmp;
					
		tmp = ((offset_buf[3] & 0x3f) << 8) + offset_buf[2];  // add by Liang for set offset_buf as OTP value 
			if (tmp & 0x2000)
				tmp |= 0xc000;
		offset_data[1] = tmp;
					
		tmp = ((offset_buf[5] & 0x3f) << 8) + offset_buf[4];  // add by Liang for set offset_buf as OTP value 
		if (tmp & 0x2000)
			tmp |= 0xc000;
		offset_data[2] = tmp;	

	memset(obj->cali_sw, 0x00, sizeof(obj->cali_sw));
	return 0;  

}
/*----------------------------------------------------------------------------*/
static int MC32X0_ReadCalibration(struct i2c_client *client, int dat[MC32X0_AXES_NUM])
{
	
    struct mc32x0_i2c_data *obj = i2c_get_clientdata(client);
    int err;
    int mul;
	
    if ((err = MC32X0_ReadOffset(client, obj->offset))) {
        GSE_ERR("read offset fail, %d\n", err);
        return err;
    }    
    
    dat[MC32X0_AXIS_X] = obj->offset[MC32X0_AXIS_X];
    dat[MC32X0_AXIS_Y] = obj->offset[MC32X0_AXIS_Y];
    dat[MC32X0_AXIS_Z] = obj->offset[MC32X0_AXIS_Z];  
	GSE_LOG("MC32X0_ReadCalibration %d %d %d \n",dat[obj->cvt.map[MC32X0_AXIS_X]] ,dat[obj->cvt.map[MC32X0_AXIS_Y]],dat[obj->cvt.map[MC32X0_AXIS_Z]]);
                                      
    return 0;
}
/*----------------------------------------------------------------------------*/
static int MC32X0_ReadCalibrationEx(struct i2c_client *client, int act[MC32X0_AXES_NUM], int raw[MC32X0_AXES_NUM])
{      
    GSE_LOG("Sensor MC32X0_ReadCalibration!\n");  
    return 0;
}
/*----------------------------------------------------------------------------*/
static int MC32X0_WriteCalibration(struct i2c_client *client, int dat[MC32X0_AXES_NUM])
{
	struct mc32x0_i2c_data *obj = i2c_get_clientdata(client);
	int err;
	int i;
	int cali_temp[MC32X0_AXES_NUM],cali[MC32X0_AXES_NUM], raw[MC32X0_AXES_NUM],sensor_temp[MC32X0_AXES_NUM];
	int lsb = mc32x0_offset_resolution.sensitivity;
	u8 buf[9];
	s16 tmp, x_gain, y_gain, z_gain ;
	s32 x_off, y_off, z_off;
#if 1

	//GSE_LOG("UPDATE dat: (%+3d %+3d %+3d)\n", 
	//dat[MC32X0_AXIS_X], dat[MC32X0_AXIS_Y], dat[MC32X0_AXIS_Z]);

	/*calculate the real offset expected by caller*/
	//cali_temp[MC32X0_AXIS_X] = dat[MC32X0_AXIS_X];
	//cali_temp[MC32X0_AXIS_Y] = dat[MC32X0_AXIS_Y];
	//cali_temp[MC32X0_AXIS_Z] = dat[MC32X0_AXIS_Z];

	GSE_LOG("UPDATE dat: (%+3d %+3d %+3d)\n", 
	dat[MC32X0_AXIS_X], dat[MC32X0_AXIS_Y], dat[MC32X0_AXIS_Z]);


	cali[MC32X0_AXIS_X] = obj->cvt.sign[MC32X0_AXIS_X]*(dat[obj->cvt.map[MC32X0_AXIS_X]]);
	cali[MC32X0_AXIS_Y] = obj->cvt.sign[MC32X0_AXIS_Y]*(dat[obj->cvt.map[MC32X0_AXIS_Y]]);
	cali[MC32X0_AXIS_Z] = obj->cvt.sign[MC32X0_AXIS_Z]*(dat[obj->cvt.map[MC32X0_AXIS_Z]]);	
	
	GSE_LOG("UPDATE dat: (%+3d %+3d %+3d)\n", 
	cali[MC32X0_AXIS_X], cali[MC32X0_AXIS_Y], cali[MC32X0_AXIS_Z]);

#endif	
// read register 0x21~0x28
#if 1
	if ((err = hwmsen_read_block(client, 0x21, buf, 3))) 
	{
		GSE_ERR("error: %d\n", err);
		return err;
	}
	if ((err = hwmsen_read_block(client, 0x24, &buf[3], 3))) 
	{
		GSE_ERR("error: %d\n", err);
		return err;
	}
	if ((err = hwmsen_read_block(client, 0x27, &buf[6], 3))) 
	{
		GSE_ERR("error: %d\n", err);
		return err;

	}
#endif
#if 1
	// get x,y,z offset
	tmp = ((buf[1] & 0x3f) << 8) + buf[0];
		if (tmp & 0x2000)
			tmp |= 0xc000;
		x_off = tmp;
					
	tmp = ((buf[3] & 0x3f) << 8) + buf[2];
		if (tmp & 0x2000)
			tmp |= 0xc000;
		y_off = tmp;
					
	tmp = ((buf[5] & 0x3f) << 8) + buf[4];
		if (tmp & 0x2000)
			tmp |= 0xc000;
		z_off = tmp;
					
	// get x,y,z gain
	x_gain = ((buf[1] >> 7) << 8) + buf[6];
	y_gain = ((buf[3] >> 7) << 8) + buf[7];
	z_gain = ((buf[5] >> 7) << 8) + buf[8];
								
	// prepare new offset
	x_off = x_off + 16 * cali[MC32X0_AXIS_X] * 256 * 128 / 3 / gsensor_gain.x / (40 + x_gain);
	y_off = y_off + 16 * cali[MC32X0_AXIS_Y] * 256 * 128 / 3 / gsensor_gain.y / (40 + y_gain);
	z_off = z_off + 16 * cali[MC32X0_AXIS_Z] * 256 * 128 / 3 / gsensor_gain.z / (40 + z_gain);

	//storege the cerrunt offset data with DOT format
	offset_data[0] = x_off;
	offset_data[1] = y_off;
	offset_data[2] = z_off;

	//storege the cerrunt Gain data with GOT format
	gain_data[0] = 256*8*128/3/(40+x_gain);
	gain_data[1] = 256*8*128/3/(40+y_gain);
	gain_data[2] = 256*8*128/3/(40+z_gain);
#endif
	buf[0]=0x43;
	hwmsen_write_block(client, 0x07, buf, 1);
					
	buf[0] = x_off & 0xff;
	buf[1] = ((x_off >> 8) & 0x3f) | (x_gain & 0x0100 ? 0x80 : 0);
	buf[2] = y_off & 0xff;
	buf[3] = ((y_off >> 8) & 0x3f) | (y_gain & 0x0100 ? 0x80 : 0);
	buf[4] = z_off & 0xff;
	buf[5] = ((z_off >> 8) & 0x3f) | (z_gain & 0x0100 ? 0x80 : 0);
					
	hwmsen_write_block(client, 0x21, buf, 6);

	buf[0]=0x41;
	hwmsen_write_block(client, 0x07, buf, 1);		

    return err;

}
/*----------------------------------------------------------------------------*/
static int MC32X0_CheckDeviceID(struct i2c_client *client)
{
	return MC32X0_SUCCESS;
}
/*----------------------------------------------------------------------------*/
//normal
//High resolution
//low noise low power
//low power

/*---------------------------------------------------------------------------*/
static int MC32X0_SetPowerMode(struct i2c_client *client, bool enable)
{
	u8 databuf[2];    
	int res = 0;
	u8 addr = MC32X0_Mode_Feature_REG;
	struct mc32x0_i2c_data *obj = i2c_get_clientdata(client);
	
	
	if(enable == sensor_power)
	{
		GSE_LOG("Sensor power status need not to be set again!!!\n");
		return MC32X0_SUCCESS;
	}

	if(hwmsen_read_byte_sr(client, addr, databuf))
	{
		GSE_ERR("read power ctl register err!\n");
		return MC32X0_ERR_I2C;
	}
	GSE_LOG("set power read MC32X0_Mode_Feature_REG =%x\n",databuf[0]);

	
	if(enable){
		databuf[1] = 0x41;
		databuf[0] = MC32X0_Mode_Feature_REG;
		res = i2c_master_send(client, databuf, 0x2);
	}
	else{
		databuf[1] = 0x43;
		databuf[0] = MC32X0_Mode_Feature_REG;
		res = i2c_master_send(client, databuf, 0x2);
	}

	
	if(res <= 0)
	{
		GSE_LOG("fwq set power mode failed!\n");
		return MC32X0_ERR_I2C;
	}
	else if(atomic_read(&obj->trace) & MCUBE_TRC_INFO)
	{
		GSE_LOG("fwq set power mode ok %d!\n", databuf[1]);
	}

	sensor_power = enable;
	return MC32X0_SUCCESS;    
}
/*----------------------------------------------------------------------------*/


static int MC32X0_SetBWRate(struct i2c_client *client, u8 bwrate)
{
	u8 databuf[10];    
	int res = 0;

	if(hwmsen_read_byte_sr(client, MC32X0_RANGE_Control_REG, databuf))
	{
		GSE_ERR("read power ctl register err!\n");
		return MC32X0_ERR_I2C;
	}

	databuf[0] &= ~0x0c;//clear original  data rate 

	if(bwrate == MC32X0_2G_LSB_G)	
	databuf[0] = databuf[0]; //set data rate
	else if(bwrate == MC32X0_4G_LSB_G)
	databuf[0] |= 0x04;
	else if(bwrate == MC32X0_8G_LSB_G)
	databuf[0] |= 0x08;	
	databuf[1]= databuf[0];
	databuf[0]= MC32X0_RANGE_Control_REG;
	
	res = i2c_master_send(client, databuf, 0x2);

	if(res <= 0)
	{
		return MC32X0_ERR_I2C;
	}	
	return MC32X0_SUCCESS;    
}


static int MC32X0_Init(struct i2c_client *client, int reset_cali)
{
	struct mc32x0_i2c_data *obj = i2c_get_clientdata(client);
	int res = 0,err =0;
	u8 uData = 0;
	u8 databuf[2];
	int cali_data[3];
	char buf[128]; 

    	GSE_LOG("mc32x0 addr %x!\n",client->addr);


	databuf[1] = 0x43;
	databuf[0] = MC32X0_Mode_Feature_REG;
	res = i2c_master_send(client, databuf, 0x2);

	res = hwmsen_read_block(client, 0x3b, databuf, 1);

GSE_LOG(">>>>>> product code: 0x%X\n", databuf[0]);
	if( databuf[0] == 0x19 )
	{
		mc32x0_type = IS_MC3230;
	}
	else if ( databuf[0] == 0x90 )
	{
		mc32x0_type = IS_MC3210;
	}

	databuf[1] = 0x00;
	databuf[0] = MC32X0_Sample_Rate_REG;
	res = i2c_master_send(client, databuf, 0x2);

	databuf[1] = 0x00;
	databuf[0] = MC32X0_Tap_Detection_Enable_REG;
	res = i2c_master_send(client, databuf, 0x2);

	databuf[1] = 0x00;
	databuf[0] = MC32X0_Interrupt_Enable_REG;
	res = i2c_master_send(client, databuf, 0x2);

	if ( mc32x0_type == IS_MC3230 )
	{
		databuf[1] = 0x32;
	}
	else if ( mc32x0_type == IS_MC3210 )
	{
		databuf[1] = 0x3F;
	}


	databuf[0] = MC32X0_RANGE_Control_REG;
	res = i2c_master_send(client, databuf, 0x2);

	if ( mc32x0_type == IS_MC3230 )
	{
		gsensor_gain.x = gsensor_gain.y = gsensor_gain.z = 86;
	}
	else if ( mc32x0_type == IS_MC3210 )
	{
		gsensor_gain.x = gsensor_gain.y = gsensor_gain.z = 1024;
	}

	MC32X0_rbm(client,0);

	databuf[1] = 0x43;
	databuf[0] = MC32X0_Mode_Feature_REG;
	res = i2c_master_send(client, databuf, 0x2);


	databuf[1] = 0x41;
	databuf[0] = MC32X0_Mode_Feature_REG;
	res = i2c_master_send(client, databuf, 0x2);	
	
	
    	GSE_LOG("fwq mc32x0 Init OK\n");
	return MC32X0_SUCCESS;
}
/*----------------------------------------------------------------------------*/
static int MC32X0_ReadChipInfo(struct i2c_client *client, char *buf, int bufsize)
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

	sprintf(buf, "MC32X0 Chip");
	return 0;
}
/*----------------------------------------------------------------------------*/
static int MC32X0_ReadSensorData(struct i2c_client *client, char *buf, int bufsize)
{
	struct mc32x0_i2c_data *obj = (struct mc32x0_i2c_data*)i2c_get_clientdata(client);
	u8 databuf[20];
	int acc[MC32X0_AXES_NUM];
	int temp[MC32X0_AXES_NUM];
	int res = 0;
	memset(databuf, 0, sizeof(u8)*10);


	GSE_LOG("MC32X0_ReadSensorData");	
	
	if(NULL == buf)
	{
		return -1;
	}
	if(NULL == client)
	{
		*buf = 0;
		return -2;
	}

	if(sensor_power == false)
	{
		res = MC32X0_SetPowerMode(client, true);
		if(res)
		{
			GSE_ERR("Power on mc32x0 error %d!\n", res);
		}
	}

	if( load_cali_flg == 0)
	{
		mcube_read_cali_file(client);
		load_cali_flg = 1;
		GSE_LOG("load_cali");
	}

	if(res = MC32X0_ReadData(client, obj->data))
	{        
		GSE_ERR("I2C error: ret value=%d", res);
		return -3;
	}
	else
	{
//Louis, 2012.11.15, get timestamp for mCube eCompass ALG =============================
        struct timeval    _tTimeStamp;
        
        do_gettimeofday(&_tTimeStamp);
//Louis, 2012.11.15, get timestamp for mCube eCompass ALG ================ END Here ===

		GSE_LOG("Mapped gsensor data: %d, %d, %d!\n", obj->data[MC32X0_AXIS_X], obj->data[MC32X0_AXIS_Y], obj->data[MC32X0_AXIS_Z]);

		//Out put the mg
		GSE_LOG("MC32X0_ReadSensorData rawdata: %d, %d, %d!\n", obj->data[MC32X0_AXIS_X], obj->data[MC32X0_AXIS_Y], obj->data[MC32X0_AXIS_Z]);
	
		acc[(obj->cvt.map[MC32X0_AXIS_X])] = obj->cvt.sign[MC32X0_AXIS_X] * obj->data[MC32X0_AXIS_X];
		acc[(obj->cvt.map[MC32X0_AXIS_Y])] = obj->cvt.sign[MC32X0_AXIS_Y] * obj->data[MC32X0_AXIS_Y];
		acc[(obj->cvt.map[MC32X0_AXIS_Z])] = obj->cvt.sign[MC32X0_AXIS_Z] * obj->data[MC32X0_AXIS_Z];

		GSE_LOG("MC32X0_ReadSensorData mapdata: %d, %d, %d!\n", acc[MC32X0_AXIS_X], acc[MC32X0_AXIS_Y], acc[MC32X0_AXIS_Z]);
		
		acc[MC32X0_AXIS_X] = (acc[MC32X0_AXIS_X]*GRAVITY_EARTH_1000/gsensor_gain.x);
		acc[MC32X0_AXIS_Y] = (acc[MC32X0_AXIS_Y]*GRAVITY_EARTH_1000/gsensor_gain.y);
		acc[MC32X0_AXIS_Z] = (acc[MC32X0_AXIS_Z]*GRAVITY_EARTH_1000/gsensor_gain.z);	

		GSE_LOG("MC32X0_ReadSensorData mapdata1: %d, %d, %d!\n", acc[MC32X0_AXIS_X], acc[MC32X0_AXIS_Y], acc[MC32X0_AXIS_Z]);
		
		//acc[MC32X0_AXIS_X] += obj->cali_sw[MC32X0_AXIS_X];
		//acc[MC32X0_AXIS_Y] += obj->cali_sw[MC32X0_AXIS_Y];
		//acc[MC32X0_AXIS_Z] += obj->cali_sw[MC32X0_AXIS_Z];

		GSE_LOG("MC32X0_ReadSensorData mapdata2: %d, %d, %d!\n", acc[MC32X0_AXIS_X], acc[MC32X0_AXIS_Y], acc[MC32X0_AXIS_Z]);

//Louis, 2012.11.15, get timestamp for mCube eCompass ALG
//		sprintf(buf, "%04x %04x %04x", acc[MC32X0_AXIS_X], acc[MC32X0_AXIS_Y], acc[MC32X0_AXIS_Z]);
		sprintf(buf, "%04x %04x %04x %ld %ld", acc[MC32X0_AXIS_X], acc[MC32X0_AXIS_Y], acc[MC32X0_AXIS_Z], _tTimeStamp.tv_sec, _tTimeStamp.tv_usec);

		GSE_LOG("gsensor data: %s!\n", buf);
		if(atomic_read(&obj->trace) & MCUBE_TRC_IOCTL)
		{
			GSE_LOG("gsensor data: %s!\n", buf);
			GSE_LOG("gsensor raw data: %d %d %d!\n", acc[obj->cvt.map[MC32X0_AXIS_X]],acc[obj->cvt.map[MC32X0_AXIS_Y]],acc[obj->cvt.map[MC32X0_AXIS_Z]]);
			GSE_LOG("gsensor data:  sensitivity x=%d \n",gsensor_gain.z);
			 
		}
	}

	
	
	return 0;
}
/*----------------------------------------------------------------------------*/
static int MC32X0_ReadRawData(struct i2c_client *client, char *buf)
{
	struct mc32x0_i2c_data *obj = (struct mc32x0_i2c_data*)i2c_get_clientdata(client);
	int res = 0;
	s16 sensor_data[3];

	if (!buf || !client)
	{
		return EINVAL;
	}

	if(sensor_power == false)
	{
		res = MC32X0_SetPowerMode(client, true);
		if(res)
		{
			GSE_ERR("Power on mc32x0 error %d!\n", res);
		}
	}
	
	if(res = MC32X0_ReadData(client, sensor_data))
	{        
		GSE_ERR("I2C error: ret value=%d", res);
		return EIO;
	}
	else
	{
		sprintf(buf, "%02x %02x %02x", sensor_data[MC32X0_AXIS_X], 
			sensor_data[MC32X0_AXIS_Y], sensor_data[MC32X0_AXIS_Z]);
	
	}
	
	return 0;
}

static int MC32X0_ReadRBMData(struct i2c_client *client, char *buf)
{
	struct mc32x0_i2c_data *obj = (struct mc32x0_i2c_data*)i2c_get_clientdata(client);
	int res = 0;
	int data[3];

	if (!buf || !client)
	{
		return EINVAL;
	}

	if(sensor_power == false)
	{
		res = MC32X0_SetPowerMode(client, true);
		if(res)
		{
			GSE_ERR("Power on mc32x0 error %d!\n", res);
		}
	}
	
	if(res = MC32X0_ReadData_RBM(client, data))
	{        
		GSE_ERR("I2C error: ret value=%d", res);
		return EIO;
	}
	else
	{
		sprintf(buf, "%04x %04x %04x", data[MC32X0_AXIS_X], 
			data[MC32X0_AXIS_Y], data[MC32X0_AXIS_Z]);
	
	}
	
	return 0;
}
/*----------------------------------------------------------------------------*/
static int MC32X0_ReadMcubeOffset(struct i2c_client *client, s16 waOffset[MC32X0_AXES_NUM])
{    
    int    _nErr;
    u8     _baOffsetGainData[_OFFSET_GAIN_SIZE];
    u16    _wAxisIndex;
    s16    _waCurrentOffset[MC32X0_AXES_NUM];
    u16    _waCurrentGain[MC32X0_AXES_NUM];

    GSE_LOG("[%s]\n", __func__);

    if ((_nErr = hwmsen_read_block(client, MC32X0_XOFFSET_L_REG, _baOffsetGainData, 4))) 
    {
        GSE_ERR("error: %d\n", _nErr);
        return _nErr;
    }

    if ((_nErr = hwmsen_read_block(client, MC32X0_ZOFFSET_L_REG, &_baOffsetGainData[4], 5))) 
    {
        GSE_ERR("error: %d\n", _nErr);
        return _nErr;
    }

    GSE_LOG("%X %X %X %X %X %X %X %X %X\n",
            _baOffsetGainData[0], _baOffsetGainData[1], _baOffsetGainData[2],
            _baOffsetGainData[3], _baOffsetGainData[4], _baOffsetGainData[5],
            _baOffsetGainData[6], _baOffsetGainData[7], _baOffsetGainData[8] );

    _waCurrentOffset[MC32X0_AXIS_X] = _TWO_BYTES_TO_OFFSET(_baOffsetGainData[0], _baOffsetGainData[1]);
    _waCurrentOffset[MC32X0_AXIS_X] = _TWO_BYTES_TO_OFFSET(_baOffsetGainData[2], _baOffsetGainData[3]);
    _waCurrentOffset[MC32X0_AXIS_Z] = _TWO_BYTES_TO_OFFSET(_baOffsetGainData[4], _baOffsetGainData[5]);
    _waCurrentGain[MC32X0_AXIS_X]   = _baOffsetGainData[6];
    _waCurrentGain[MC32X0_AXIS_Y]   = _baOffsetGainData[7];
    _waCurrentGain[MC32X0_AXIS_Z]   = _baOffsetGainData[8];

    for (_wAxisIndex = MC32X0_AXIS_X; _wAxisIndex < MC32X0_AXES_NUM; _wAxisIndex++)
    {
        _waCurrentGain[_wAxisIndex] |= ((_GAIN_UPPER_BIT & _waCurrentOffset[_wAxisIndex]) >> 7);

        _waCurrentOffset[_wAxisIndex] &= _OFFSET_14BIT_MASK;

        if (_waCurrentOffset[_wAxisIndex] & _OFFSET_SIGN_BIT)
            _waCurrentOffset[_wAxisIndex] |= ~_OFFSET_14BIT_MASK;

        waOffset[_wAxisIndex] = _INV_OFFSET_GAIN_ADJUST(_waCurrentOffset[_wAxisIndex], _DFLT_GAIN_NUM, _waCurrentGain[_wAxisIndex]);
        waOffset[_wAxisIndex] = _ACCEL_LSB_TO_MG(waOffset[_wAxisIndex]);
    }

    GSE_LOG("Offset: %d %d %d, Gain: %d %d %d\n", waOffset[0], waOffset[1], waOffset[2], _waCurrentGain[0], _waCurrentGain[1], _waCurrentGain[2]);

    return 0;  
}
/*----------------------------------------------------------------------------*/
static int MC32X0_WriteMcubeOffset(struct i2c_client *client, s16 waOffset[MC32X0_AXES_NUM])
{    
    int    _nErr;
    u8     _baBuf[MC32X0_DATA_LEN];
    u8     _baOffsetGainData[_OFFSET_GAIN_SIZE];
    u16    _wAxisIndex;
    s16    _waCurrentOffset[MC32X0_AXES_NUM];
    u16    _waCurrentGain[MC32X0_AXES_NUM];
    s16    _waNewOffset[MC32X0_AXES_NUM];

    GSE_LOG("[%s]\n", __func__);

    if ((_nErr = hwmsen_read_block(client, MC32X0_XOFFSET_L_REG, _baOffsetGainData, 4))) 
    {
        GSE_ERR("error: %d\n", _nErr);
        return _nErr;
    }

    if ((_nErr = hwmsen_read_block(client, MC32X0_ZOFFSET_L_REG, &_baOffsetGainData[4], 5))) 
    {
        GSE_ERR("error: %d\n", _nErr);
        return _nErr;
    }

    _waCurrentOffset[MC32X0_AXIS_X] = _TWO_BYTES_TO_OFFSET(_baOffsetGainData[0], _baOffsetGainData[1]);
    _waCurrentOffset[MC32X0_AXIS_X] = _TWO_BYTES_TO_OFFSET(_baOffsetGainData[2], _baOffsetGainData[3]);
    _waCurrentOffset[MC32X0_AXIS_Z] = _TWO_BYTES_TO_OFFSET(_baOffsetGainData[4], _baOffsetGainData[5]);
    _waCurrentGain[MC32X0_AXIS_X]   = _baOffsetGainData[6];
    _waCurrentGain[MC32X0_AXIS_Y]   = _baOffsetGainData[7];
    _waCurrentGain[MC32X0_AXIS_Z]   = _baOffsetGainData[8];

    GSE_LOG("Offset: %d %d %d, Gain: %d %d %d\n", waOffset[0], waOffset[1], waOffset[2], _waCurrentGain[0], _waCurrentGain[1], _waCurrentGain[2]);

    for (_wAxisIndex = MC32X0_AXIS_X; _wAxisIndex < MC32X0_AXES_NUM; _wAxisIndex++)
    {
        _waCurrentGain[_wAxisIndex] |= ((_GAIN_UPPER_BIT & _waCurrentOffset[_wAxisIndex]) >> 7);

        _waCurrentOffset[_wAxisIndex] &= _OFFSET_14BIT_MASK;

        if (_waCurrentOffset[_wAxisIndex] & _OFFSET_SIGN_BIT)
            _waCurrentOffset[_wAxisIndex] |= ~_OFFSET_14BIT_MASK;

        _waNewOffset[_wAxisIndex] = _MG_TO_ACCEL_OFFSET(waOffset[_wAxisIndex]);
        _waNewOffset[_wAxisIndex] = _OFFSET_GAIN_ADJUST(_waNewOffset[_wAxisIndex], _DFLT_GAIN_NUM, _waCurrentGain[_wAxisIndex]);
    }

    GSE_LOG("%X %X %X %X %X %X %X %X %X\n",
            _baOffsetGainData[0], _baOffsetGainData[1], _baOffsetGainData[2],
            _baOffsetGainData[3], _baOffsetGainData[4], _baOffsetGainData[5],
            _baOffsetGainData[6], _baOffsetGainData[7], _baOffsetGainData[8] );

    //Louis, 2012.12.17, under test stage
    #if 0
    if (   ((_waNewOffset[MC32X0_AXIS_X] <= _MAX_OFFSET) && (_waNewOffset[MC32X0_AXIS_X] >= _MIN_OFFSET))
        && ((_waNewOffset[MC32X0_AXIS_Y] <= _MAX_OFFSET) && (_waNewOffset[MC32X0_AXIS_Y] >= _MIN_OFFSET))
        && ((_waNewOffset[MC32X0_AXIS_Z] <= _MAX_OFFSET) && (_waNewOffset[MC32X0_AXIS_Z] >= _MIN_OFFSET)))
    {
        _baBuf[0]=0x43;
        hwmsen_write_block(client, MC32X0_Mode_Feature_REG, _baBuf, 1);
        		
        _baBuf[0] = _waNewOffset[MC32X0_AXIS_X] & 0xff;
        _baBuf[1] = ((_waNewOffset[MC32X0_AXIS_X] >> 8) & 0x3f) | (_waCurrentGain[MC32X0_AXIS_X] & 0x0100 ? 0x80 : 0);
        _baBuf[2] = _waNewOffset[MC32X0_AXIS_Y] & 0xff;
        _baBuf[3] = ((_waNewOffset[MC32X0_AXIS_Y] >> 8) & 0x3f) | (_waCurrentGain[MC32X0_AXIS_Y] & 0x0100 ? 0x80 : 0);
        _baBuf[4] = _waNewOffset[MC32X0_AXIS_Z] & 0xff;
        _baBuf[5] = ((_waNewOffset[MC32X0_AXIS_Z] >> 8) & 0x3f) | (_waCurrentGain[MC32X0_AXIS_Z] & 0x0100 ? 0x80 : 0);
        		
        hwmsen_write_block(client, MC32X0_XOFFSET_L_REG, _baBuf, MC32X0_DATA_LEN);
        
        _baBuf[0]=0x41;
        hwmsen_write_block(client, MC32X0_Mode_Feature_REG, _baBuf, 1);		
    }
    #endif

    return 0;  
}
/*----------------------------------------------------------------------------*/
static int MC32X0_InitSelfTest(struct i2c_client *client)
{
	u8 databuf[10];    
	int res = 0;

	MC32X0_SetPowerMode(client, false);

    if (hwmsen_read_byte_sr(client, 0x20, &databuf[0]))
    {
        GSE_ERR("error read RandControlRegister\n");
    }

    GSE_ERR("before RandControlRegister: 0x%X\n", databuf[0]);

	databuf[0] = 0x20;
	databuf[1] = 0xAF;   // 128Hz, 2g with 10 bit
	
	res = i2c_master_send(client, databuf, 0x2);

	if (res <= 0)
		return MC32X0_ERR_I2C;

	MC32X0_SetPowerMode(client, true);
	
	mdelay(60);

    if (hwmsen_read_byte_sr(client, 0x20, &databuf[0]))
    {
        GSE_ERR("error read RandControlRegister\n");
    }

    GSE_ERR("after RandControlRegister: 0x%X\n", databuf[0]);

	return MC32X0_SUCCESS;
}
/*----------------------------------------------------------------------------*/
static int MC32X0_JudgeTestResult(struct i2c_client *client)
{
	struct mc32x0_i2c_data *obj = (struct mc32x0_i2c_data*)i2c_get_clientdata(client);
	int res = 0;
	s16  acc[MC32X0_AXES_NUM];
	int  self_result;
	
	if(res = MC32X0_ReadData(client, acc))
	{        
		GSE_ERR("I2C error: ret value=%d", res);
		return EIO;
	}
	else
	{			
		printk("0 step: %d %d %d\n", acc[0],acc[1],acc[2]);

		acc[MC32X0_AXIS_X] = acc[MC32X0_AXIS_X] * 1000 / 1024;
		acc[MC32X0_AXIS_Y] = acc[MC32X0_AXIS_Y] * 1000 / 1024;
		acc[MC32X0_AXIS_Z] = acc[MC32X0_AXIS_Z] * 1000 / 1024;
		
		printk("1 step: %d %d %d\n", acc[0],acc[1],acc[2]);
		
		self_result = acc[MC32X0_AXIS_X]*acc[MC32X0_AXIS_X] 
			+ acc[MC32X0_AXIS_Y]*acc[MC32X0_AXIS_Y] 
			+ acc[MC32X0_AXIS_Z]*acc[MC32X0_AXIS_Z];
			
		
		printk("2 step: result = %d", self_result);

//	    if ( (self_result > 215092) && (self_result < 987670) ) //between 0.7g and 1.5g 
	    if ( (self_result > 475923) && (self_result < 2185360) ) //between 0.7g and 1.5g 
	    {												 
			GSE_ERR("MC32X0_JudgeTestResult successful\n");
			return MC32X0_SUCCESS;
		}
		{
	        GSE_ERR("MC32X0_JudgeTestResult failt\n");
	        return -EINVAL;
	    }
	
	}
}
/*----------------------------------------------------------------------------*/
static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
    GSE_LOG("fwq show_chipinfo_value \n");
	struct i2c_client *client = mc32x0_i2c_client;
	char strbuf[MC32X0_BUFSIZE];
	if(NULL == client)
	{
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}
	
	MC32X0_ReadChipInfo(client, strbuf, MC32X0_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);        
}
/*----------------------------------------------------------------------------*/
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = mc32x0_i2c_client;
	char strbuf[MC32X0_BUFSIZE];
	
	if(NULL == client)
	{
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}
	MC32X0_ReadSensorData(client, strbuf, MC32X0_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);            
}
/*----------------------------------------------------------------------------*/
static ssize_t show_cali_value(struct device_driver *ddri, char *buf)
{
    GSE_LOG("fwq show_cali_value \n");
	struct i2c_client *client = mc32x0_i2c_client;
	struct mc32x0_i2c_data *obj;

	if(NULL == client)
	{
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	obj = i2c_get_clientdata(client);

	int err, len = 0, mul;
	int tmp[MC32X0_AXES_NUM];

	if(err = MC32X0_ReadOffset(client, obj->offset))
	{
		return -EINVAL;
	}
	else if(err = MC32X0_ReadCalibration(client, tmp))
	{
		return -EINVAL;
	}
	else
	{    
		mul = obj->reso->sensitivity/mc32x0_offset_resolution.sensitivity;
		len += snprintf(buf+len, PAGE_SIZE-len, "[HW ][%d] (%+3d, %+3d, %+3d) : (0x%02X, 0x%02X, 0x%02X)\n", mul,                        
			obj->offset[MC32X0_AXIS_X], obj->offset[MC32X0_AXIS_Y], obj->offset[MC32X0_AXIS_Z],
			obj->offset[MC32X0_AXIS_X], obj->offset[MC32X0_AXIS_Y], obj->offset[MC32X0_AXIS_Z]);
		len += snprintf(buf+len, PAGE_SIZE-len, "[SW ][%d] (%+3d, %+3d, %+3d)\n", 1, 
			obj->cali_sw[MC32X0_AXIS_X], obj->cali_sw[MC32X0_AXIS_Y], obj->cali_sw[MC32X0_AXIS_Z]);

		len += snprintf(buf+len, PAGE_SIZE-len, "[ALL]    (%+3d, %+3d, %+3d) : (%+3d, %+3d, %+3d)\n", 
			obj->offset[MC32X0_AXIS_X]*mul + obj->cali_sw[MC32X0_AXIS_X],
			obj->offset[MC32X0_AXIS_Y]*mul + obj->cali_sw[MC32X0_AXIS_Y],
			obj->offset[MC32X0_AXIS_Z]*mul + obj->cali_sw[MC32X0_AXIS_Z],
			tmp[MC32X0_AXIS_X], tmp[MC32X0_AXIS_Y], tmp[MC32X0_AXIS_Z]);
		
		return len;
    }
}
/*----------------------------------------------------------------------------*/
static ssize_t store_cali_value(struct device_driver *ddri, char *buf, size_t count)
{
	struct i2c_client *client = mc32x0_i2c_client;  
	int err, x, y, z;
	int dat[MC32X0_AXES_NUM];

	if(!strncmp(buf, "rst", 3))
	{
		if(err = MC32X0_ResetCalibration(client))
		{
			GSE_ERR("reset offset err = %d\n", err);
		}	
	}
	else if(3 == sscanf(buf, "0x%02X 0x%02X 0x%02X", &x, &y, &z))
	{
		dat[MC32X0_AXIS_X] = x;
		dat[MC32X0_AXIS_Y] = y;
		dat[MC32X0_AXIS_Z] = z;
		if(err = MC32X0_WriteCalibration(client, dat))
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
static ssize_t show_selftest_value(struct device_driver *ddri, char *buf)
{

	return 0;
}
/*----------------------------------------------------------------------------*/
static ssize_t store_selftest_value(struct device_driver *ddri, char *buf, size_t count)
{   /*write anything to this register will trigger the process*/
    struct item
    {
        s16    raw[MC32X0_AXES_NUM];
    };

    struct i2c_client *client = mc32x0_i2c_client;  
    int idx, res, num;
    struct item *prv = NULL, *nxt = NULL;
    
    if (1 != sscanf(buf, "%d", &num))
    {
        GSE_ERR("parse number fail\n");
        return count;
    }
    else if(0 == num)
    {
        GSE_ERR("invalid data count\n");
        return count;
    }
    
    prv = kzalloc(sizeof(*prv) * num, GFP_KERNEL);
    nxt = kzalloc(sizeof(*nxt) * num, GFP_KERNEL);

    if (!prv || !nxt)
        goto exit;
    
    GSE_LOG("NORMAL:\n");
    MC32X0_SetPowerMode(client, true); 
//    MC32X0_InitSelfTest(client);
    GSE_LOG("SELFTEST:\n");    
    
    if (!MC32X0_JudgeTestResult(client))
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
    //mc32X0_init_client(client, 0);
    kfree(prv);
    kfree(nxt);

    return count;
}
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
static ssize_t show_firlen_value(struct device_driver *ddri, char *buf)
{
    GSE_LOG("fwq show_firlen_value \n");

	return snprintf(buf, PAGE_SIZE, "not support\n");
}
/*----------------------------------------------------------------------------*/
static ssize_t store_firlen_value(struct device_driver *ddri, char *buf, size_t count)
{
    GSE_LOG("fwq store_firlen_value \n");
	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t show_trace_value(struct device_driver *ddri, char *buf)
{
    GSE_LOG("fwq show_trace_value \n");
	ssize_t res;
	struct mc32x0_i2c_data *obj = obj_i2c_data;
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
    GSE_LOG("fwq store_trace_value \n");
	struct mc32x0_i2c_data *obj = obj_i2c_data;
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
    GSE_LOG("fwq show_status_value \n");
	ssize_t len = 0;    
	struct mc32x0_i2c_data *obj = obj_i2c_data;
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

static ssize_t show_power_status(struct device_driver *ddri, char *buf)
{
	
	ssize_t res;
	u8 uData;
	struct mc32x0_i2c_data *obj = obj_i2c_data;
	if (obj == NULL)
	{
		GSE_ERR("i2c_data obj is null!!\n");
		return 0;
	}
	hwmsen_read_byte(obj->client, MC32X0_Mode_Feature_REG, &uData);
	
	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", uData);     
	return res;   
}


/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(chipinfo,             S_IRUGO, show_chipinfo_value,      NULL);
static DRIVER_ATTR(sensordata,           S_IRUGO, show_sensordata_value,    NULL);
static DRIVER_ATTR(cali,       S_IWUSR | S_IRUGO, show_cali_value,          store_cali_value);
static DRIVER_ATTR(selftest,       S_IWUSR | S_IRUGO, show_selftest_value,          store_selftest_value);
static DRIVER_ATTR(firlen,     S_IWUSR | S_IRUGO, show_firlen_value,        store_firlen_value);
static DRIVER_ATTR(trace,      S_IWUSR | S_IRUGO, show_trace_value,         store_trace_value);
static DRIVER_ATTR(status,               S_IRUGO, show_status_value,        NULL);
static DRIVER_ATTR(power,               S_IRUGO, show_power_status,        NULL);

/*----------------------------------------------------------------------------*/
static struct driver_attribute *mc32x0_attr_list[] = {
	&driver_attr_chipinfo,     /*chip information*/
	&driver_attr_sensordata,   /*dump sensor data*/
	&driver_attr_cali,         /*show calibration data*/
	&driver_attr_selftest,         /*self test demo*/
	&driver_attr_firlen,       /*filter length: 0: disable, others: enable*/
	&driver_attr_trace,        /*trace log*/
	&driver_attr_status,
	&driver_attr_power, 
};
/*----------------------------------------------------------------------------*/
static int mc32x0_create_attr(struct device_driver *driver) 
{
	int idx, err = 0;
	int num = (int)(sizeof(mc32x0_attr_list)/sizeof(mc32x0_attr_list[0]));
	if (driver == NULL)
	{
		return -EINVAL;
	}

	for(idx = 0; idx < num; idx++)
	{
		if(err = driver_create_file(driver, mc32x0_attr_list[idx]))
		{            
			GSE_ERR("driver_create_file (%s) = %d\n", mc32x0_attr_list[idx]->attr.name, err);
			break;
		}
	}    
	return err;
}
/*----------------------------------------------------------------------------*/
static int mc32x0_delete_attr(struct device_driver *driver)
{
	int idx ,err = 0;
	int num = (int)(sizeof(mc32x0_attr_list)/sizeof(mc32x0_attr_list[0]));

	if(driver == NULL)
	{
		return -EINVAL;
	}
	

	for(idx = 0; idx < num; idx++)
	{
		driver_remove_file(driver, mc32x0_attr_list[idx]);
	}
	

	return err;
}

/*----------------------------------------------------------------------------*/

int gsensor_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value, sample_delay;	
	struct mc32x0_i2c_data *priv = (struct mc32x0_i2c_data*)self;
	hwm_sensor_data* gsensor_data;
	char buff[MC32X0_BUFSIZE];
	
	GSE_FUN(f);
	switch (command)
	{
		case SENSOR_DELAY:
			GSE_LOG("fwq set delay\n");
			break;

		case SENSOR_ENABLE:
			GSE_LOG("fwq sensor enable gsensor\n");
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
					err = MC32X0_SetPowerMode( priv->client, !sensor_power);
				}
			}
			break;

		case SENSOR_GET_DATA:
			GSE_LOG("fwq sensor operate get data\n");
			if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
			{
				GSE_ERR("get sensor data parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				gsensor_data = (hwm_sensor_data *)buff_out;
				MC32X0_ReadSensorData(priv->client, buff, MC32X0_BUFSIZE);
				sscanf(buff, "%x %x %x", &gsensor_data->values[0], 
					&gsensor_data->values[1], &gsensor_data->values[2]);				
				gsensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;				
				gsensor_data->value_divide = 1000;
				GSE_LOG("X :%d,Y: %d, Z: %d\n",gsensor_data->values[0],gsensor_data->values[1],gsensor_data->values[2]);
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
static int mc32x0_open(struct inode *inode, struct file *file)
{
	file->private_data = mc32x0_i2c_client;

	if(file->private_data == NULL)
	{
		GSE_ERR("null pointer!!\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}
/*----------------------------------------------------------------------------*/
static int mc32x0_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}
/*----------------------------------------------------------------------------*/
//static int mc32x0_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
//       unsigned long arg)
static long mc32x0_unlocked_ioctl(struct file *file, unsigned int cmd,
       unsigned long arg)
{
	struct i2c_client *client = (struct i2c_client*)file->private_data;
	struct mc32x0_i2c_data *obj = (struct mc32x0_i2c_data*)i2c_get_clientdata(client);	
	char strbuf[MC32X0_BUFSIZE];
	void __user *data;
	SENSOR_DATA sensor_data;
	int temp;
	int err = 0;
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
			GSE_LOG("fwq GSENSOR_IOCTL_INIT\n");
			MC32X0_Init(client, 0);			
			break;

		case GSENSOR_IOCTL_READ_CHIPINFO:
			GSE_LOG("fwq GSENSOR_IOCTL_READ_CHIPINFO\n");
			data = (void __user *) arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}
			
			MC32X0_ReadChipInfo(client, strbuf, MC32X0_BUFSIZE);
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
			
			MC32X0_ReadSensorData(client, strbuf, MC32X0_BUFSIZE);
			if(copy_to_user(data, strbuf, strlen(strbuf)+1))
			{
				err = -EFAULT;
				break;	  
			}				 
			break;

		case GSENSOR_IOCTL_READ_GAIN:
			GSE_LOG("fwq GSENSOR_IOCTL_READ_GAIN\n");
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
			GSE_LOG("fwq GSENSOR_IOCTL_READ_OFFSET\n");
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
			GSE_LOG("fwq GSENSOR_IOCTL_READ_RAW_DATA\n");
			data = (void __user *) arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}
			MC32X0_ReadRawData(client, &strbuf);
			if(copy_to_user(data, &strbuf, strlen(strbuf)+1))
			{
				err = -EFAULT;
				break;	  
			}
			break;	  

		case GSENSOR_IOCTL_SET_CALI:
			GSE_LOG("fwq GSENSOR_IOCTL_SET_CALI!!\n");
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
				obj->cali_sw[MC32X0_AXIS_X] += sensor_data.x;
				obj->cali_sw[MC32X0_AXIS_Y] += sensor_data.y;
				obj->cali_sw[MC32X0_AXIS_Z] += sensor_data.z;
				
				cali[MC32X0_AXIS_X] = sensor_data.x * gsensor_gain.x / GRAVITY_EARTH_1000;
				cali[MC32X0_AXIS_Y] = sensor_data.y * gsensor_gain.y / GRAVITY_EARTH_1000;
				cali[MC32X0_AXIS_Z] = sensor_data.z * gsensor_gain.z / GRAVITY_EARTH_1000;	
			  
				err = MC32X0_WriteCalibration(client, cali);			 
			}
			break;

		case GSENSOR_IOCTL_CLR_CALI:
			GSE_LOG("fwq GSENSOR_IOCTL_CLR_CALI!!\n");
			err = MC32X0_ResetCalibration(client);
			break;

		case GSENSOR_IOCTL_GET_CALI:
			GSE_LOG("fwq mc32x0 GSENSOR_IOCTL_GET_CALI\n");
			data = (void __user*)arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}
			if((err = MC32X0_ReadCalibration(client, cali)))
			{
				break;
			}

			sensor_data.x = obj->cali_sw[MC32X0_AXIS_X];
			sensor_data.y = obj->cali_sw[MC32X0_AXIS_Y];
			sensor_data.z = obj->cali_sw[MC32X0_AXIS_Z];
			if(copy_to_user(data, &sensor_data, sizeof(sensor_data)))
			{
				err = -EFAULT;
				break;
			}		
			break;

		// add by liang ****
		//add in Sensors_io.h
		//#define GSENSOR_IOCTL_SET_CALI_MODE   _IOW(GSENSOR, 0x0e, int)
#if defined(GSENSOR_MCUBE_IOCMD)
		case GSENSOR_MCUBE_IOCTL_SET_CALI:
			GSE_LOG("fwq GSENSOR_IOCTL_SET_CALI!!\n");
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
				//obj->cali_sw[MC32X0_AXIS_X] += sensor_data.x;
				//obj->cali_sw[MC32X0_AXIS_Y] += sensor_data.y;
				//obj->cali_sw[MC32X0_AXIS_Z] += sensor_data.z;
				
				cali[MC32X0_AXIS_X] = sensor_data.x * gsensor_gain.x / GRAVITY_EARTH_1000;
				cali[MC32X0_AXIS_Y] = sensor_data.y * gsensor_gain.y / GRAVITY_EARTH_1000;
				cali[MC32X0_AXIS_Z] = sensor_data.z * gsensor_gain.z / GRAVITY_EARTH_1000;	
			  
				err = MC32X0_WriteCalibration(client, cali);			 
			}
			break;

		case GSENSOR_IOCTL_SET_CALI_MODE:
			GSE_LOG("fwq mc32x0 GSENSOR_IOCTL_SET_CALI_MODE\n");
			break;

		case GSENSOR_MCUBE_IOCTL_READ_RBM_DATA:
			GSE_LOG("fwq GSENSOR_MCUBE_IOCTL_READ_RBM_DATA\n");
			data = (void __user *) arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}
			MC32X0_ReadRBMData(client, &strbuf);
			if(copy_to_user(data, &strbuf, strlen(strbuf)+1))
			{
				err = -EFAULT;
				break;	  
			}
			break;

		case GSENSOR_MCUBE_IOCTL_SET_RBM_MODE:
			GSE_LOG("fwq GSENSOR_MCUBE_IOCTL_SET_RBM_MODE\n");
			data = (void __user*)arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}
			if(copy_from_user(&LPF_data, data, sizeof(int)))
			{
				err = -EFAULT;
				break;	  
			}

			MC32X0_rbm(client,1);

			break;

		case GSENSOR_MCUBE_IOCTL_CLEAR_RBM_MODE:
			GSE_LOG("fwq GSENSOR_MCUBE_IOCTL_SET_RBM_MODE\n");

			MC32X0_rbm(client,0);

			break;

		case GSENSOR_MCUBE_IOCTL_REGISTER_MAP:
			GSE_LOG("fwq GSENSOR_MCUBE_IOCTL_REGISTER_MAP\n");

			MC32X0_Read_Reg_Map(client);

			break;

        case GSENSOR_MCUBE_IOCTL_READ_OFFSET:
            {
                s16    _waOffset[MC32X0_AXES_NUM];

                GSE_LOG("fwq GSENSOR_MCUBE_IOCTL_READ_OFFSET\n");
    
                data = ((void __user *) arg);
    
                if (NULL == data)
                {
                    err = -EINVAL;
                    break;	  
                }
    
                MC32X0_ReadMcubeOffset(client, _waOffset);
    
                if (copy_to_user(data, _waOffset, (sizeof(s16) * MC32X0_AXES_NUM)))
                {
                    err = -EFAULT;
                    break;	  
                }
            }
            break;

		case GSENSOR_MCUBE_IOCTL_SET_OFFSET:
		    {
                s16    _waOffset[MC32X0_AXES_NUM];

                GSE_LOG("fwq GSENSOR_MCUBE_IOCTL_SET_OFFSET\n");

                data = ((void __user*)arg);

                if (NULL == data)
                {
                    err = -EINVAL;
                    break;	  
                }

                if (copy_from_user(_waOffset, data, (sizeof(s16) * MC32X0_AXES_NUM)))
                {
                    err = -EFAULT;
                    break;	  
                }

                #if 0   // necessary? check with Liang
                if(atomic_read(&obj->suspend))
                {
                    GSE_ERR("Perform calibration in suspend state!!\n");
                    err = -EINVAL;
                }
                else
                #endif
                {
                    err = MC32X0_WriteMcubeOffset(client, _waOffset);
                }
            }
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
static struct file_operations mc32x0_fops = {
	.owner = THIS_MODULE,
	.open = mc32x0_open,
	.release = mc32x0_release,
	.unlocked_ioctl = mc32x0_unlocked_ioctl,
};
/*----------------------------------------------------------------------------*/
static struct miscdevice mc32x0_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gsensor",
	.fops = &mc32x0_fops,
};
/*----------------------------------------------------------------------------*/
//#ifndef CONFIG_HAS_EARLYSUSPEND
/*----------------------------------------------------------------------------*/
static int mc32x0_suspend(struct i2c_client *client, pm_message_t msg) 
{
	struct mc32x0_i2c_data *obj = i2c_get_clientdata(client);    
	int err = 0;
	u8  dat=0;
	GSE_FUN();    

	if(msg.event == PM_EVENT_SUSPEND)
	{   
		if(obj == NULL)
		{
			GSE_ERR("null pointer!!\n");
			return -EINVAL;
		}
		
		atomic_set(&obj->suspend, 1);
		if (err = MC32X0_SetPowerMode(client,false)){
            GSE_ERR("write power control fail!!\n");
            return err;
        }     
		MC32X0_power(obj->hw, 0);
	}
	return err;
}
/*----------------------------------------------------------------------------*/
static int mc32x0_resume(struct i2c_client *client)
{
	struct mc32x0_i2c_data *obj = i2c_get_clientdata(client);        
	int err;
	GSE_FUN();

	if(obj == NULL)
	{
		GSE_ERR("null pointer!!\n");
		return -EINVAL;
	}

	MC32X0_power(obj->hw, 1);
	if(err = MC32X0_Init(client, 0))
	{
		GSE_ERR("initialize client fail!!\n");
		return err;        
	}
	atomic_set(&obj->suspend, 0);

	return 0;
}
/*----------------------------------------------------------------------------*/
//#else /*CONFIG_HAS_EARLY_SUSPEND is defined*/
/*----------------------------------------------------------------------------*/
static void mc32x0_early_suspend(struct early_suspend *h) 
{
	struct mc32x0_i2c_data *obj = container_of(h, struct mc32x0_i2c_data, early_drv);   
	int err;
	GSE_FUN();    

	if(obj == NULL)
	{
		GSE_ERR("null pointer!!\n");
		return;
	}
	atomic_set(&obj->suspend, 1); 
	/*
	if(err = hwmsen_write_byte(obj->client, MC32X0_REG_POWER_CTL, 0x00))
	{
		GSE_ERR("write power control fail!!\n");
		return;
	}  
	*/
	if(err = MC32X0_SetPowerMode(obj->client, false))
	{
		GSE_ERR("write power control fail!!\n");
		return;
	}

	sensor_power = false;
	
	MC32X0_power(obj->hw, 0);
}
/*----------------------------------------------------------------------------*/
static void mc32x0_late_resume(struct early_suspend *h)
{
	struct mc32x0_i2c_data *obj = container_of(h, struct mc32x0_i2c_data, early_drv);         
	int err;
	GSE_FUN();

	if(obj == NULL)
	{
		GSE_ERR("null pointer!!\n");
		return;
	}

	MC32X0_power(obj->hw, 1);
	if(err = MC32X0_Init(obj->client, 0))
	{

		GSE_ERR("initialize client fail!!\n");
		return;        
	}
	atomic_set(&obj->suspend, 0);    
}
/*----------------------------------------------------------------------------*/
//#endif /*CONFIG_HAS_EARLYSUSPEND*/
/*----------------------------------------------------------------------------*/
static int mc32x0_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info) 
{    
	strcpy(info->type, MC32X0_DEV_NAME);
	return 0;
}
/*----------------------------------------------------------------------------
static void mc32x0_init_hardware()
{
	#if defined( K8_DH_P110 )
	mt_set_gpio_mode(GPIO_GSE_1_EINT_PIN,GPIO_MODE_01);
	mt_set_gpio_dir(GPIO_GSE_1_EINT_PIN,GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_GSE_1_EINT_PIN,GPIO_OUT_ONE);
	#endif	
}

----------------------------------------------------------------------------*/
static int mc32x0_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_client *new_client;
	struct mc32x0_i2c_data *obj;
	struct hwmsen_object sobj;
	int err = 0;
	GSE_FUN();

	//mc32x0_init_hardware();

	if(!(obj = kzalloc(sizeof(*obj), GFP_KERNEL)))
	{
		err = -ENOMEM;
		goto exit;
	}
	
	memset(obj, 0, sizeof(struct mc32x0_i2c_data));

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
	
#ifdef CONFIG_MC32X0_LOWPASS
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

	mc32x0_i2c_client = new_client;		
	{
		unsigned char buf[2];

		// add by Liang for reset sensor: Fix software system reset issue!!!!!!!!!
		buf[0]=0x43;
		hwmsen_write_block(client, 0x07, buf, 1);	

		buf[0]=0x80;
		hwmsen_write_block(client, 0x1C, buf, 1);	
		buf[0]=0x80;
		hwmsen_write_block(client, 0x17, buf, 1);	
		msleep(5);
		
		buf[0]=0x00;
		hwmsen_write_block(client, 0x1C, buf, 1);	
		buf[0]=0x00;
		hwmsen_write_block(client, 0x17, buf, 1);	
		msleep(5);
	}
	if ((err = hwmsen_read_block(new_client, 0x21, offset_buf, 6))) //add by Liang for storeage OTP offsef register value
	{
		GSE_ERR("error: %d\n", err);
		return err;
	}

	if(err = MC32X0_Init(new_client, 1))
	{
		goto exit_init_failed;
	}
	

	if(err = misc_register(&mc32x0_device))
	{
		GSE_ERR("mc32x0_device register failed\n");
		goto exit_misc_device_register_failed;
	}

	if(err = mc32x0_create_attr(&mc32x0_gsensor_driver.driver))
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
	obj->early_drv.suspend  = mc32x0_early_suspend,
	obj->early_drv.resume   = mc32x0_late_resume,    
	register_early_suspend(&obj->early_drv);
#endif 

	GSE_LOG("%s: OK\n", __func__);    
	return 0;

exit_create_attr_failed:
	misc_deregister(&mc32x0_device);
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
static int mc32x0_i2c_remove(struct i2c_client *client)
{
	int err = 0;	
	
	if(err = mc32x0_delete_attr(&mc32x0_gsensor_driver.driver))
	{
		GSE_ERR("mc32x0_delete_attr fail: %d\n", err);
	}
	
	if(err = misc_deregister(&mc32x0_device))
	{
		GSE_ERR("misc_deregister fail: %d\n", err);
	}

	if(err = hwmsen_detach(ID_ACCELEROMETER))
	    

	mc32x0_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));
	return 0;
}
/*----------------------------------------------------------------------------*/
static int mc32x0_probe(struct platform_device *pdev) 
{
	struct acc_hw *hw = get_cust_acc_hw();
	GSE_FUN();

	MC32X0_power(hw, 1);
	//mc32x0_force[0] = hw->i2c_num;
	if(i2c_add_driver(&mc32x0_i2c_driver))
	{
		GSE_ERR("add driver error\n");
		return -1;
	}
	return 0;
}
/*----------------------------------------------------------------------------*/
static int mc32x0_remove(struct platform_device *pdev)
{
    struct acc_hw *hw = get_cust_acc_hw();

    GSE_FUN();    
    MC32X0_power(hw, 0);    
    i2c_del_driver(&mc32x0_i2c_driver);
    return 0;
}
/*----------------------------------------------------------------------------*/
static struct platform_driver mc32x0_gsensor_driver = {
	.probe      = mc32x0_probe,
	.remove     = mc32x0_remove,    
	.driver     = {
		.name  = "gsensor",
		.owner = THIS_MODULE,
	}
};

/*----------------------------------------------------------------------------*/
static int __init mc32x0_init(void)
{
	struct acc_hw *hw = get_cust_acc_hw();
	GSE_FUN();
	i2c_register_board_info(hw->i2c_num, &i2c_mc32x0, 1);
	if(platform_driver_register(&mc32x0_gsensor_driver))
	{
		GSE_ERR("failed to register driver");
		return -ENODEV;
	}
	return 0;    
}
/*----------------------------------------------------------------------------*/
static void __exit mc32x0_exit(void)
{
	GSE_FUN();
	platform_driver_unregister(&mc32x0_gsensor_driver);
}
/*----------------------------------------------------------------------------*/
module_init(mc32x0_init);
module_exit(mc32x0_exit);
/*----------------------------------------------------------------------------*/
MODULE_DESCRIPTION("MC32X0 I2C driver");
