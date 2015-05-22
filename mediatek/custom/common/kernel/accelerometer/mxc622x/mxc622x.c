/* MXC622X motion sensor driver
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
	 
#include <mach/mt_pm_ldo.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_boot.h>

#define POWER_NONE_MACRO MT65XX_POWER_NONE
	 
#include <cust_acc.h>
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include "mxc622x.h"
#include <linux/hwmsen_helper.h>
	 /*----------------------------------------------------------------------------*/
#define I2C_DRIVERID_MXC622X 150
	 /*----------------------------------------------------------------------------*/
#define DEBUG 1
	 /*----------------------------------------------------------------------------*/
	 //#define CONFIG_MXC622X_LOWPASS   /*apply low pass filter on output*/		 
#define SW_CALIBRATION
	 
	 /*----------------------------------------------------------------------------*/
#define MXC622X_AXIS_X          0
#define MXC622X_AXIS_Y          1
#define MXC622X_AXIS_Z          2
#define MXC622X_AXES_NUM        2
#define MXC622X_DATA_LEN        2
#define MXC622X_DEV_NAME        "MXC622X"
	 /*----------------------------------------------------------------------------*/
	 
	 /*----------------------------------------------------------------------------*/
	 static const struct i2c_device_id mxc622x_i2c_id[] = {{MXC622X_DEV_NAME,1},{}};
	 /*the adapter id will be available in customization*/
	 static struct i2c_board_info __initdata i2c_mxc622x={ I2C_BOARD_INFO("MXC622X", MXC622X_I2C_SLAVE_ADDR>>1)};
	 
	 //static unsigned short mxc622x_force[] = {0x00, MXC622X_I2C_SLAVE_WRITE_ADDR, I2C_CLIENT_END, I2C_CLIENT_END};
	 //static const unsigned short *const mxc622x_forces[] = { mxc622x_force, NULL };
	 //static struct i2c_client_address_data mxc622x_addr_data = { .forces = mxc622x_forces,};
	 
	 /*----------------------------------------------------------------------------*/
	 static int mxc622x_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id); 
	 static int mxc622x_i2c_remove(struct i2c_client *client);
	 static int mxc622x_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);
	 
	 /*----------------------------------------------------------------------------*/
	 typedef enum {
		 ADX_TRC_FILTER  = 0x01,
		 ADX_TRC_RAWDATA = 0x02,
		 ADX_TRC_IOCTL	 = 0x04,
		 ADX_TRC_CALI	 = 0X08,
		 ADX_TRC_INFO	 = 0X10,
	 } ADX_TRC;
	 /*----------------------------------------------------------------------------*/
	 struct scale_factor{
		 u8  whole;
		 u8  fraction;
	 };
	 /*----------------------------------------------------------------------------*/
	 struct data_resolution {
		 struct scale_factor scalefactor;
		 int				 sensitivity;
	 };
	 /*----------------------------------------------------------------------------*/
#define C_MAX_FIR_LENGTH (32)
	 /*----------------------------------------------------------------------------*/
	 struct data_filter {
		 s16 raw[C_MAX_FIR_LENGTH][MXC622X_AXES_NUM];
		 int sum[MXC622X_AXES_NUM];
		 int num;
		 int idx;
	 };
	 /*----------------------------------------------------------------------------*/
	 struct mxc622x_i2c_data {
		 struct i2c_client *client;
		 struct acc_hw *hw;
		 struct hwmsen_convert	 cvt;
		 atomic_t layout; 
		 /*misc*/
		 struct data_resolution *reso;
		 atomic_t				 trace;
		 atomic_t				 suspend;
		 atomic_t				 selftest;
		 atomic_t				 filter;
		 s16					 cali_sw[MXC622X_AXES_NUM+1];
	 
		 /*data*/
		 s8 					 offset[MXC622X_AXES_NUM+1];  /*+1: for 4-byte alignment*/
		 s16					 data[MXC622X_AXES_NUM+1];
	 
#if defined(CONFIG_MXC622X_LOWPASS)
		 atomic_t				 firlen;
		 atomic_t				 fir_en;
		 struct data_filter 	 fir;
#endif 
		 /*early suspend*/
#if defined(CONFIG_HAS_EARLYSUSPEND)
		 struct early_suspend	 early_drv;
#endif     
	 };
	 /*----------------------------------------------------------------------------*/
	 static struct i2c_driver mxc622x_i2c_driver = {
		 .driver = {
			// .owner		   = THIS_MODULE,
			 .name			 = MXC622X_DEV_NAME,
		 },
		 .probe 			 = mxc622x_i2c_probe,
		 .remove			 = mxc622x_i2c_remove,
//		 .detect			 = mxc622x_i2c_detect,
#if !defined(CONFIG_HAS_EARLYSUSPEND)    
		 .suspend			 = mxc622x_suspend,
		 .resume			 = mxc622x_resume,
#endif
		 .id_table = mxc622x_i2c_id,
		 //.address_data = &mxc622x_addr_data,
	 };
	 
	 /*----------------------------------------------------------------------------*/
	 static struct i2c_client *mxc622x_i2c_client = NULL;
	 static struct platform_driver mxc622x_gsensor_driver;
	 static struct mxc622x_i2c_data *obj_i2c_data = NULL;
	 static bool sensor_power = true;
	 static GSENSOR_VECTOR3D gsensor_gain;
	 static char selftestRes[8]= {0}; 
	 
	 /*----------------------------------------------------------------------------*/
#define GSE_TAG                  "[Gsensor] "
#define GSE_FUN(f)               printk(GSE_TAG"%s\n", __FUNCTION__)
#define GSE_ERR(fmt, args...)    printk(KERN_ERR GSE_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define GSE_LOG(fmt, args...)    printk(GSE_TAG fmt, ##args)

	 /*----------------------------------------------------------------------------*/
	 static struct data_resolution mxc622x_data_resolution[1] = {
	  /* combination by {FULL_RES,RANGE}*/
		 {{ 15, 6}, 64},   // dataformat +/-2g	in 8-bit resolution;  { 15, 6} = 15.6 = (2*2*1000)/(2^8);  64 = (2^8)/(2*2)		   
	 };
	 /*----------------------------------------------------------------------------*/
	 static struct data_resolution mxc622x_offset_resolution = {{15, 6}, 64};
	 


#define BMA222_I2C_GPIO_MODE
//#define BMA222_I2C_GPIO_MODE_DEBUG

#define MXC622X_ABS(a) (((a) < 0) ? -(a) : (a))

static int mxc622x_sqrt(int high, int low, int value)
{
	int med;
	int high_diff = 0;
	int low_diff = 0;
	
	if (value <= 0)
		return 0;

	high_diff = MXC622X_ABS(high * high - value);
	low_diff = MXC622X_ABS(low * low - value);
	
	while (MXC622X_ABS(high - low) > 1)
	{
		med = (high + low ) / 2;
		if (med * med > value)
		{
			high = med;
			high_diff = high * high - value;
		}
		else
		{
			low = med;
			low_diff = value - low * low;
		}
	}

	return high_diff <= low_diff ? high : low;
}


#if defined(BMA222_I2C_GPIO_MODE)	//modified by zhaofei
#define BMA222_I2C_SLAVE_WRITE_ADDR	0x2A	
static struct mutex g_gsensor_mutex;
#define GPIO_GSE_SDA_PIN 90
#define GPIO_GSE_SCL_PIN 89
#define GPIO_GSE_SDA_PIN_M_GPIO GPIO_MODE_00
#define GPIO_GSE_SCL_PIN_M_GPIO GPIO_MODE_00
void cust_i2c_scl_set( unsigned char mode)
{
	if(mt_set_gpio_mode(GPIO_GSE_SCL_PIN, GPIO_GSE_SDA_PIN_M_GPIO))
	{
		printk("BMA222 cust_i2c_scl_set mode failed \n");
	}
	if(mt_set_gpio_dir(GPIO_GSE_SCL_PIN, GPIO_DIR_OUT))
	{
		printk("BMA222 cust_i2c_scl_set dir failed \n");
	}
	if( mode == 1)
	{
		if(mt_set_gpio_out(GPIO_GSE_SCL_PIN, GPIO_OUT_ONE))
		{
			printk("BMA222 cust_i2c_scl_set high failed \n");
		}
	}
	else
	{
		if(mt_set_gpio_out(GPIO_GSE_SCL_PIN, GPIO_OUT_ZERO))
		{
			printk("BMA222 cust_i2c_scl_set low failed \n");
		}
	}
}

void cust_i2c_sda_set( unsigned char mode)
{
	if(mt_set_gpio_mode(GPIO_GSE_SDA_PIN, GPIO_GSE_SDA_PIN_M_GPIO))
	{
		printk("BMA222 cust_i2c_sda_set mode failed \n");
	}
	if(mt_set_gpio_dir(GPIO_GSE_SDA_PIN, GPIO_DIR_OUT))
	{
		printk("BMA222 cust_i2c_sda_set dir failed \n");
	}
	if( mode == 1)
	{
		if(mt_set_gpio_out(GPIO_GSE_SDA_PIN, GPIO_OUT_ONE))
		{
			printk("BMA222 cust_i2c_sda_set high failed \n");
		}
	}
	else
	{
		if(mt_set_gpio_out(GPIO_GSE_SDA_PIN, GPIO_OUT_ZERO))
		{
			printk("BMA222 cust_i2c_sda_set low failed \n");
		}
	}
}

void cust_i2c_sda_dir_set( unsigned char mode)
{	
	if(mt_set_gpio_mode(GPIO_GSE_SDA_PIN, GPIO_GSE_SDA_PIN_M_GPIO))
	{
		printk("BMA222 cust_i2c_sda_dir_set mode failed \n");
	}
	if( mode == GPIO_DIR_IN)
	{
		if(mt_set_gpio_dir(GPIO_GSE_SDA_PIN, GPIO_DIR_IN))
		{
			printk("BMA222 cust_i2c_sda_dir_set in failed \n");
		}
	}
	else
	{
		if(mt_set_gpio_dir(GPIO_GSE_SDA_PIN, GPIO_DIR_OUT))
		{
			printk("BMA222 cust_i2c_sda_dir_set out failed \n");
		}
	}
}

unsigned char cust_i2c_sda_get(void)
{
	if(mt_set_gpio_mode(GPIO_GSE_SDA_PIN, GPIO_GSE_SDA_PIN_M_GPIO))
	{
		printk("BMA222 cust_i2c_sda_get mode failed \n");
	}
	if(mt_set_gpio_dir(GPIO_GSE_SDA_PIN,GPIO_DIR_IN))
	{
		printk("BMA222 cust_i2c_sda_get dir failed \n");
	}
	
	return  mt_get_gpio_in(GPIO_GSE_SDA_PIN);
}

void cust_i2c_start(void)
{
	//printk("cust_i2c_start \n");
	cust_i2c_scl_set(1);
	cust_i2c_sda_set(1);
	udelay(5);
	cust_i2c_sda_set(0);
	udelay(5);
	cust_i2c_scl_set(0);
}

void cust_i2c_stop(void)
{
	//printk("cust_i2c_stop \n");
	cust_i2c_scl_set(1);
	cust_i2c_sda_set(0);
	udelay(5);
	cust_i2c_sda_set(1);
	udelay(5);
}

char cust_i2c_get_ack(void)
{
	unsigned char get_bit;
	unsigned char i;
	//printk("cust_i2c_get_ack \n");
	//cust_i2c_sda_set(1);
	//udelay(5);
	cust_i2c_sda_dir_set(GPIO_DIR_IN);
	cust_i2c_scl_set(1);
	udelay(5);
	//cust_i2c_sda_get();
	for(i=0; i<10; i++)
	{
		get_bit =  mt_get_gpio_in(GPIO_GSE_SDA_PIN);
		udelay(5);
		if(0 == get_bit)
		{
			break;
		}
	}
	cust_i2c_scl_set(0);
	cust_i2c_sda_dir_set(GPIO_DIR_OUT);
	if(i == 10)
	{
		return -1;
	}
	
	return 0;
}

char cust_i2c_send_ack(void)
{
	//printk("cust_i2c_send_ack \n");
	cust_i2c_sda_set(0);
	udelay(5);
	cust_i2c_scl_set(1);
	udelay(5);
	cust_i2c_scl_set(0);
	return 0;
}

void cust_i2c_no_ack(void)
{
	//printk("cust_i2c_send_ack \n");
	cust_i2c_sda_set(1);
	cust_i2c_scl_set(1);
	udelay(5);
	cust_i2c_scl_set(0);
	udelay(5);
}

void cust_i2c_send_byte( unsigned char udata)
{
	char i; 

	//printk("cust_i2c_send_byte \n",udata);
	for( i = 0; i<8;i++ )
	{
		if( ((udata>>(7-i)) & 0x01) == 0x01)
		{
			cust_i2c_sda_set(1);
		}
		else
		{
			cust_i2c_sda_set(0);
		}
		
		udelay(2);
		cust_i2c_scl_set(1);
		udelay(5);
		cust_i2c_scl_set(0);
	}	
		udelay(5);
}

unsigned char cust_i2c_get_byte( void )
{
	char i;
	unsigned char data;
	unsigned char get_bit;
	
	data = 0;
	//printk("cust_i2c_get_byte \n");
	cust_i2c_sda_dir_set(GPIO_DIR_IN);	
	for( i = 0; i<8; i++ )
	{
		cust_i2c_scl_set(1);
		udelay(5);
		//get_bit = cust_i2c_sda_get();
		get_bit =  mt_get_gpio_in(GPIO_GSE_SDA_PIN);
		cust_i2c_scl_set(0);
		udelay(5);
		if( 1 == (get_bit &0x01))
		{
			data |= get_bit <<(7-i);
		}
	}	
	udelay(5);
	return data;
}

char cust_i2c_write_byte(unsigned char addr, unsigned char regaddr, unsigned char udata)
{
	char res;

	cust_i2c_start();
	cust_i2c_send_byte(addr);
	res = cust_i2c_get_ack();
	if(0 != res)
	{
		printk("BMA222 cust_i2c_write_byte device addr error \n");
		return -1;
	}
	cust_i2c_send_byte(regaddr);
	res = cust_i2c_get_ack();
	if( 0 != res )
	{
		printk("BMA222 cust_i2c_write_byte reg addr error \n");
		return -1;
	}
	cust_i2c_send_byte(udata);
	res = cust_i2c_get_ack();
	if( 0 != res )
	{
		printk("BMA222 cust_i2c_write_byte reg data error \n");
		return -1;
	}
	cust_i2c_stop();
	return 0;
}

char cust_i2c_read_byte(unsigned char addr, unsigned char regaddr, unsigned char *udata)
{
	unsigned char data;
	char res;

	cust_i2c_start();
	cust_i2c_send_byte( addr );
	res = cust_i2c_get_ack();
	if( 0 != res )
	{
		printk("BMA222 cust_i2c_read_byte device addr error \n");
		return -1;
	}
	cust_i2c_send_byte( regaddr );
	res = cust_i2c_get_ack();
	if( 0 != res )
	{
		printk("BMA222 cust_i2c_read_byte reg addr error \n");
		return -1;
	}
	cust_i2c_start();
	addr |= 0x01;
	cust_i2c_send_byte( addr );
	res = cust_i2c_get_ack();
	if( 0 != res )
	{
		printk("BMA222 cust_i2c_read_byte devce addr error \n");
		return -1;
	}
	*udata = cust_i2c_get_byte();
	cust_i2c_no_ack();
	cust_i2c_stop();

	return 0;
}

char cust_i2c_write_bytes(unsigned char addr, unsigned char regaddr, unsigned char *udata, unsigned int count)
{
	u32 i;
	char res;

	cust_i2c_start();
	cust_i2c_send_byte( addr );
	res = cust_i2c_get_ack();
	if( 0 != res )
	{
		printk("BMA222 cust_i2c_write_bytes device addr error \n");
		return -1;
	}
	cust_i2c_send_byte( regaddr );
	res = cust_i2c_get_ack();
	if( 0 != res )
	{
		printk("BMA222 cust_i2c_write_bytes reg addr error \n");
		return -1;
	}
	for( i = 0; i< count; i++)
	{
		cust_i2c_send_byte(udata[i]);
		res = cust_i2c_get_ack();
		if(0 != res)
		{
			printk("BMA222 cust_i2c_write_bytes reg data error \n",__FUNCTION__,__LINE__);
			return -1;
		}
	}
	cust_i2c_stop();

	return 0;
}

char cust_i2c_read_bytes(unsigned char addr, unsigned char regaddr, unsigned char *udata,unsigned int count)
{
	unsigned char data;
	unsigned int i;
	char res;

	cust_i2c_start();
	cust_i2c_send_byte( addr );
	res = cust_i2c_get_ack();
	if( 0 != res )
	{
		printk("BMA222 cust_i2c_read_bytes device addr error \n");
		return -1;
	}
	cust_i2c_send_byte( regaddr );
	res = cust_i2c_get_ack();
	if( 0 != res )
	{
		printk("BMA222 cust_i2c_read_bytes reg addr error \n");
		return -1;
	}
	cust_i2c_start();
	addr |= 0x01;
	cust_i2c_send_byte( addr );
	res = cust_i2c_get_ack();
	if( 0 != res )
	{
		printk("BMA222 cust_i2c_read_bytes reg addr error \n");
		return -1;
	}

	for( i = 0; i < count-1; i++)
	{
		udata[i] = cust_i2c_get_byte();
		res = cust_i2c_send_ack();
		if(0 != res)
		{
			printk("BMA222 cust_i2c_read_bytes reg data error \n");
			return -1;
		}
	}
	udata[i] = cust_i2c_get_byte();
	cust_i2c_no_ack();

	cust_i2c_stop();

	return data;
}

int cust_i2c_master_send(struct i2c_client *client, u8 *buf, u8 count)
{
	u8 slave_addr;
	u8 reg_addr;

	mutex_lock(&g_gsensor_mutex);
	slave_addr = BMA222_I2C_SLAVE_WRITE_ADDR ;	
	reg_addr = buf[0];
#if defined(BMA222_I2C_GPIO_MODE_DEBUG)
	printk("BMA222 cust_i2c_master_send_byte reg_addr=0x x% \n", reg_addr);
#endif
	cust_i2c_write_bytes(slave_addr,reg_addr, &buf[1],count-1);
	mutex_unlock(&g_gsensor_mutex);

	return count;
}

int cust_i2c_master_read(struct i2c_client *client, u8 *buf, u8 count)
{
	u8 slave_addr;
	u8 reg_addr;

	slave_addr = BMA222_I2C_SLAVE_WRITE_ADDR ;	
	reg_addr = buf[0];
#if defined(BMA222_I2C_GPIO_MODE_DEBUG)
	printk("BMA222 cust_i2c_master_read_byte reg_addr=0x %x\n", reg_addr);
#endif
	cust_i2c_read_bytes(slave_addr,reg_addr, &buf[0],count);

	return count;
}

int cust_hwmsen_write_block(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{
	u8 slave_addr;
	u8 reg_addr;

	mutex_lock(&g_gsensor_mutex);
	slave_addr = BMA222_I2C_SLAVE_WRITE_ADDR;	
	reg_addr = addr;
#if defined(BMA222_I2C_GPIO_MODE_DEBUG)
	printk("BMA222 cust_hwmsen_write_block reg_addr=0x%x\n", reg_addr);
#endif
	cust_i2c_write_bytes(slave_addr,reg_addr, data,len);
	mutex_unlock(&g_gsensor_mutex);

	return 0;
}

int cust_hwmsen_read_block(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{
	u8 buf[64] = {0};
	mutex_lock(&g_gsensor_mutex);
	buf[0] = addr;
	cust_i2c_master_read(client, buf, len);
	mutex_unlock(&g_gsensor_mutex);
#if defined(BMA222_I2C_GPIO_MODE_DEBUG)	
	printk("BMA222 cust_hwmsen_read_block addr=0x%x, buf=0x%x\n", addr, buf[0]);
#endif
	memcpy(data, buf, len);
	return 0;
}
#endif

	 /*--------------------MXC622X power control function----------------------------------*/
	 static void MXC622X_power(struct acc_hw *hw, unsigned int on) 
	 {
		 static unsigned int power_on = 0;
	 
		 if(hw->power_id != POWER_NONE_MACRO)		 // have externel LDO
		 {		  
			 GSE_LOG("power %s\n", on ? "on" : "off");
			 if(power_on == on)  // power status not change
			 {
				 GSE_LOG("ignore power control: %d\n", on);
			 }
			 else if(on) // power on
			 {
				 if(!hwPowerOn(hw->power_id, hw->power_vol, "MXC622X"))
				 {
					 GSE_ERR("power on fails!!\n");
				 }
			 }
			 else	 // power off
			 {
				 if (!hwPowerDown(hw->power_id, "MXC622X"))
				 {
					 GSE_ERR("power off fail!!\n");
				 }			   
			 }
		 }
		 power_on = on;    
	 }
	 /*----------------------------------------------------------------------------*/
	 
	 /*----------------------------------------------------------------------------*/
	 static int MXC622X_SetDataResolution(struct mxc622x_i2c_data *obj)
	 {
		 int err;
		 u8  dat, reso;
	 
	 /*set g sensor dataresolution here*/
	 
	 /*MXC622X only can set to 8-bit dataresolution, so do nothing in mxc622x driver here*/
	 
	 /*end of set dataresolution*/
	 
	 
	  
	  /*we set measure range from -2g to +2g in MXC622X_SetDataFormat(client, MXC622X_RANGE_2G), 
														 and set 10-bit dataresolution MXC622X_SetDataResolution()*/
														 
	  /*so mxc622x_data_resolution[0] set value as {{ 15, 6},64} when declaration, and assign the value to obj->reso here*/  
	 
		 obj->reso = &mxc622x_data_resolution[0];
		 return 0;
		 
	 /*if you changed the measure range, for example call: MXC622X_SetDataFormat(client, MXC622X_RANGE_4G), 
	 you must set the right value to mxc622x_data_resolution*/
	 
	 }
	 /*----------------------------------------------------------------------------*/
	 static int MXC622X_ReadData(struct i2c_client *client, s16 data[MXC622X_AXES_NUM])
	 {
		 struct mxc622x_i2c_data *priv = i2c_get_clientdata(client); 	   
		 u8 addr = MXC622X_REG_DATAX0;
		 u8 buf[MXC622X_DATA_LEN] = {0};
		 int err = 0;
		 int i;
		 int tmp=0;
		 u8 ofs[3];
	 
	 
	 
		 if(NULL == client)
		 {
			 err = -EINVAL;
		 }
		 else if(err = cust_hwmsen_read_block(client, addr, buf, 0x02))
		 {
			 //printk("gsensor 11111\n");
			 GSE_ERR("error: %d\n", err);
		 }
		 else
		 {
			 //printk("gsensor 222222\n");
			 data[MXC622X_AXIS_X] = (s16)buf[0];
			 data[MXC622X_AXIS_Y] = (s16)buf[1];
	 
	 		if(data[MXC622X_AXIS_X]&0x80)
			 {
					 data[MXC622X_AXIS_X] = ~data[MXC622X_AXIS_X];
					 data[MXC622X_AXIS_X] &= 0xff;
					 data[MXC622X_AXIS_X]+=1;
					 data[MXC622X_AXIS_X] = -data[MXC622X_AXIS_X];
			 }
			 if(data[MXC622X_AXIS_Y]&0x80)
			 {
					 data[MXC622X_AXIS_Y] = ~data[MXC622X_AXIS_Y];
					 data[MXC622X_AXIS_Y] &= 0xff;
					 data[MXC622X_AXIS_Y]+=1;
					 data[MXC622X_AXIS_Y] = -data[MXC622X_AXIS_Y];
			 }

		
			 if(atomic_read(&priv->trace) & ADX_TRC_RAWDATA)
			 {
				 GSE_LOG("[%08X %08X] => [%5d %5d]\n", data[MXC622X_AXIS_X], data[MXC622X_AXIS_Y],
											data[MXC622X_AXIS_X], data[MXC622X_AXIS_Y]);
			 }
#ifdef CONFIG_MXC622X_LOWPASS
			 if(atomic_read(&priv->filter))
			 {
				 if(atomic_read(&priv->fir_en) && !atomic_read(&priv->suspend))
				 {
					 int idx, firlen = atomic_read(&priv->firlen);	 
					 if(priv->fir.num < firlen)
					 {				  
						 priv->fir.raw[priv->fir.num][MXC622X_AXIS_X] = data[MXC622X_AXIS_X];
						 priv->fir.raw[priv->fir.num][MXC622X_AXIS_Y] = data[MXC622X_AXIS_Y];

						 priv->fir.sum[MXC622X_AXIS_X] += data[MXC622X_AXIS_X];
						 priv->fir.sum[MXC622X_AXIS_Y] += data[MXC622X_AXIS_Y];

						 if(atomic_read(&priv->trace) & ADX_TRC_FILTER)
						 {
							 GSE_LOG("add [%2d] [%5d %5d] => [%5d %5d]\n", priv->fir.num,
								 priv->fir.raw[priv->fir.num][MXC622X_AXIS_X], priv->fir.raw[priv->fir.num][MXC622X_AXIS_Y],
								 priv->fir.sum[MXC622X_AXIS_X], priv->fir.sum[MXC622X_AXIS_Y]);
						 }
						 priv->fir.num++;
						 priv->fir.idx++;
					 }
					 else
					 {
						 idx = priv->fir.idx % firlen;
						 priv->fir.sum[MXC622X_AXIS_X] -= priv->fir.raw[idx][MXC622X_AXIS_X];
						 priv->fir.sum[MXC622X_AXIS_Y] -= priv->fir.raw[idx][MXC622X_AXIS_Y];

						 priv->fir.raw[idx][MXC622X_AXIS_X] = data[MXC622X_AXIS_X];
						 priv->fir.raw[idx][MXC622X_AXIS_Y] = data[MXC622X_AXIS_Y];

						 priv->fir.sum[MXC622X_AXIS_X] += data[MXC622X_AXIS_X];
						 priv->fir.sum[MXC622X_AXIS_Y] += data[MXC622X_AXIS_Y];

						 priv->fir.idx++;
						 data[MXC622X_AXIS_X] = priv->fir.sum[MXC622X_AXIS_X]/firlen;
						 data[MXC622X_AXIS_Y] = priv->fir.sum[MXC622X_AXIS_Y]/firlen;

						 if(atomic_read(&priv->trace) & ADX_TRC_FILTER)
						 {
							 GSE_LOG("add [%2d] [%5d %5d] => [%5d %5d] : [%5d %5d]\n", idx,
							 priv->fir.raw[idx][MXC622X_AXIS_X], priv->fir.raw[idx][MXC622X_AXIS_Y],
							 priv->fir.sum[MXC622X_AXIS_X], priv->fir.sum[MXC622X_AXIS_Y],
							 data[MXC622X_AXIS_X], data[MXC622X_AXIS_Y]);
						 }
					 }
				 }
			 }	 
#endif         
		 }
		 return err;
	 }
	 /*----------------------------------------------------------------------------*/
	 static int MXC622X_ReadOffset(struct i2c_client *client, s8 ofs[MXC622X_AXES_NUM])
	 {	  
		 int err=0;
#ifdef SW_CALIBRATION
		 ofs[0]=ofs[1]=ofs[2]=0x0;
#else
/*
		 if(err = hwmsen_read_block(client, MXC622X_REG_OFSX, ofs, MXC622X_AXES_NUM))
		 {
			 GSE_ERR("error: %d\n", err);
		 }
 */
#endif
		 //printk("offesx=%x, y=%x, z=%x",ofs[0],ofs[1],ofs[2]);
		 
		 return err;	
	 }
	 /*----------------------------------------------------------------------------*/
	 static int MXC622X_ResetCalibration(struct i2c_client *client)
	 {
		 struct mxc622x_i2c_data *obj = i2c_get_clientdata(client);
		 u8 ofs[4]={0,0,0,0};
		 int err=0;
		 
	#ifdef SW_CALIBRATION
			 
	#else
	/*
			 if(err = hwmsen_write_block(client, MXC622X_REG_OFSX, ofs, 4))
			 {
				 GSE_ERR("error: %d\n", err);
			 }
	*/
	#endif
	 
		 memset(obj->cali_sw, 0x00, sizeof(obj->cali_sw));
		 memset(obj->offset, 0x00, sizeof(obj->offset));
		 return err;	
	 }
	 /*----------------------------------------------------------------------------*/
	 static int MXC622X_ReadCalibration(struct i2c_client *client, int dat[MXC622X_AXES_NUM])
	 {
		 struct mxc622x_i2c_data *obj = i2c_get_clientdata(client);
		 int err=0;
		 int mul;
	 
	#ifdef SW_CALIBRATION
			 mul = 0;//only SW Calibration, disable HW Calibration
	#else
	
			 if ((err = MXC622X_ReadOffset(client, obj->offset))) {
			 GSE_ERR("read offset fail, %d\n", err);
			 return err;
			 }	  
			 mul = obj->reso->sensitivity/mxc622x_offset_resolution.sensitivity;
	
	#endif
	 
		 dat[obj->cvt.map[MXC622X_AXIS_X]] = obj->cvt.sign[MXC622X_AXIS_X]*(obj->offset[MXC622X_AXIS_X]*mul + obj->cali_sw[MXC622X_AXIS_X]);
		 dat[obj->cvt.map[MXC622X_AXIS_Y]] = obj->cvt.sign[MXC622X_AXIS_Y]*(obj->offset[MXC622X_AXIS_Y]*mul + obj->cali_sw[MXC622X_AXIS_Y]);
		 					
											
		 return 0;
	 }
	 /*----------------------------------------------------------------------------*/
	 static int MXC622X_ReadCalibrationEx(struct i2c_client *client, int act[MXC622X_AXES_NUM], int raw[MXC622X_AXES_NUM])
	 {	
		 /*raw: the raw calibration data; act: the actual calibration data*/
		 struct mxc622x_i2c_data *obj = i2c_get_clientdata(client);
		 int err;
		 int mul;
	 
	  
	 
	#ifdef SW_CALIBRATION
			 mul = 0;//only SW Calibration, disable HW Calibration
	#else
			 if(err = MXC622X_ReadOffset(client, obj->offset))
			 {
				 GSE_ERR("read offset fail, %d\n", err);
				 return err;
			 }	 
			 mul = obj->reso->sensitivity/mxc622x_offset_resolution.sensitivity;
	#endif
		 
		 raw[MXC622X_AXIS_X] = obj->offset[MXC622X_AXIS_X]*mul + obj->cali_sw[MXC622X_AXIS_X];
		 raw[MXC622X_AXIS_Y] = obj->offset[MXC622X_AXIS_Y]*mul + obj->cali_sw[MXC622X_AXIS_Y];

	 
		 act[obj->cvt.map[MXC622X_AXIS_X]] = obj->cvt.sign[MXC622X_AXIS_X]*raw[MXC622X_AXIS_X];
		 act[obj->cvt.map[MXC622X_AXIS_Y]] = obj->cvt.sign[MXC622X_AXIS_Y]*raw[MXC622X_AXIS_Y];
					
								
		 return 0;
	 }
	 /*----------------------------------------------------------------------------*/
	 static int MXC622X_WriteCalibration(struct i2c_client *client, int dat[MXC622X_AXES_NUM])
	 {
		 struct mxc622x_i2c_data *obj = i2c_get_clientdata(client);
		 int err;
		 int cali[MXC622X_AXES_NUM], raw[MXC622X_AXES_NUM];
		 int lsb = mxc622x_offset_resolution.sensitivity;
		 int divisor = obj->reso->sensitivity/lsb;
	 
		 if(err = MXC622X_ReadCalibrationEx(client, cali, raw))	 /*offset will be updated in obj->offset*/
		 { 
			 GSE_ERR("read offset fail, %d\n", err);
			 return err;
		 }
	 
		 GSE_LOG("OLDOFF: (%+3d %+3d): (%+3d %+3d) / (%+3d %+3d)\n", 
			 raw[MXC622X_AXIS_X], raw[MXC622X_AXIS_Y],
			 obj->offset[MXC622X_AXIS_X], obj->offset[MXC622X_AXIS_Y],
			 obj->cali_sw[MXC622X_AXIS_X], obj->cali_sw[MXC622X_AXIS_Y]);
	 
		 /*calculate the real offset expected by caller*/
		 cali[MXC622X_AXIS_X] += dat[MXC622X_AXIS_X];
		 cali[MXC622X_AXIS_Y] += dat[MXC622X_AXIS_Y];

	 
		 GSE_LOG("UPDATE: (%+3d %+3d)\n", 
			 dat[MXC622X_AXIS_X], dat[MXC622X_AXIS_Y]);
	 
#ifdef SW_CALIBRATION
		 obj->cali_sw[MXC622X_AXIS_X] = obj->cvt.sign[MXC622X_AXIS_X]*(cali[obj->cvt.map[MXC622X_AXIS_X]]);
		 obj->cali_sw[MXC622X_AXIS_Y] = obj->cvt.sign[MXC622X_AXIS_Y]*(cali[obj->cvt.map[MXC622X_AXIS_Y]]);

#else
#if 0
		 obj->offset[MXC622X_AXIS_X] = (s8)(obj->cvt.sign[MXC622X_AXIS_X]*(cali[obj->cvt.map[MXC622X_AXIS_X]])/(divisor));
		 obj->offset[MXC622X_AXIS_Y] = (s8)(obj->cvt.sign[MXC622X_AXIS_Y]*(cali[obj->cvt.map[MXC622X_AXIS_Y]])/(divisor));
		 obj->offset[MXC622X_AXIS_Z] = (s8)(obj->cvt.sign[MXC622X_AXIS_Z]*(cali[obj->cvt.map[MXC622X_AXIS_Z]])/(divisor));
	 
		 /*convert software calibration using standard calibration*/
		 obj->cali_sw[MXC622X_AXIS_X] = obj->cvt.sign[MXC622X_AXIS_X]*(cali[obj->cvt.map[MXC622X_AXIS_X]])%(divisor);
		 obj->cali_sw[MXC622X_AXIS_Y] = obj->cvt.sign[MXC622X_AXIS_Y]*(cali[obj->cvt.map[MXC622X_AXIS_Y]])%(divisor);
		 obj->cali_sw[MXC622X_AXIS_Z] = obj->cvt.sign[MXC622X_AXIS_Z]*(cali[obj->cvt.map[MXC622X_AXIS_Z]])%(divisor);
	 
		 GSE_LOG("NEWOFF: (%+3d %+3d %+3d): (%+3d %+3d %+3d) / (%+3d %+3d %+3d)\n", 
			 obj->offset[MXC622X_AXIS_X]*divisor + obj->cali_sw[MXC622X_AXIS_X], 
			 obj->offset[MXC622X_AXIS_Y]*divisor + obj->cali_sw[MXC622X_AXIS_Y], 
			 obj->offset[MXC622X_AXIS_Z]*divisor + obj->cali_sw[MXC622X_AXIS_Z], 
			 obj->offset[MXC622X_AXIS_X], obj->offset[MXC622X_AXIS_Y], obj->offset[MXC622X_AXIS_Z],
			 obj->cali_sw[MXC622X_AXIS_X], obj->cali_sw[MXC622X_AXIS_Y], obj->cali_sw[MXC622X_AXIS_Z]);
	 
		 if(err = hwmsen_write_block(obj->client, MXC622X_REG_OFSX, obj->offset, MXC622X_AXES_NUM))
		 {
			 GSE_ERR("write offset fail: %d\n", err);
			 return err;
		 }
#endif
#endif
	 
		 return err;
	 }
	 /*----------------------------------------------------------------------------*/
	 static int MXC622X_SetPowerMode(struct i2c_client *client, bool enable)
	 {
		 u8 databuf[2];    
		 int res = 0;
		 u8 addr = MXC622X_REG_DETECTION;
		 struct mxc622x_i2c_data *obj = i2c_get_clientdata(client);
		 
		 
		 if(enable == sensor_power )
		 {
			 GSE_LOG("Sensor power status is newest!\n");
			 return MXC622X_SUCCESS;
		 }
#if 0	 
		 if(hwmsen_read_block(client, addr, databuf, 0x01))
		 {
			 GSE_ERR("read power ctl register err!\n");
			 return MXC622X_ERR_I2C;
		 }
#endif	 
		 
		 if(enable == TRUE)
		 {
			 databuf[1] =0x00;
		 }
		 else
		 {
			 databuf[1] =0x01<<7;
		 }
		 
		 databuf[0] = MXC622X_REG_DETECTION;
		 
	 
		 res = cust_i2c_master_send(client, databuf, 0x2);
	 
		 if(res <= 0)
		 {
			 GSE_LOG("set power mode failed!\n");
			 return MXC622X_ERR_I2C;
		 }
		 else if(atomic_read(&obj->trace) & ADX_TRC_INFO)
		 {
			 GSE_LOG("set power mode ok %d!\n", databuf[1]);
		 }
	 
		 //GSE_LOG("MXC622X_SetPowerMode ok!\n");
	 
	 
		 sensor_power = enable;
	 
		 mdelay(20);
		 
		 return MXC622X_SUCCESS;    
	 }
	 /*----------------------------------------------------------------------------*/
	 static int MXC622X_SetDataFormat(struct i2c_client *client, u8 dataformat)
	 {
	 
		 struct mxc622x_i2c_data *obj = i2c_get_clientdata(client);
		 u8 databuf[10];	
		 int res = 0;
	 #if 0
		 memset(databuf, 0, sizeof(u8)*10);    
	 
		 if(hwmsen_read_block(client, MXC622X_REG_DATA_FORMAT, databuf, 0x01))
		 {
			 printk("mxc622x read Dataformat failt \n");
			 return MXC622X_ERR_I2C;
		 }
	 
		 databuf[0] &= ~MXC622X_RANGE_MASK;
		 databuf[0] |= dataformat;
		 databuf[1] = databuf[0];
		 databuf[0] = MXC622X_REG_DATA_FORMAT;
	 
	 
		 res = cust_i2c_master_send(client, databuf, 0x2);
	 
		 if(res <= 0)
		 {
			 return MXC622X_ERR_I2C;
		 }
		 
		 //printk("MXC622X_SetDataFormat OK! \n");
		 
	 #endif
		 return MXC622X_SetDataResolution(obj);	  
	 }
	 /*----------------------------------------------------------------------------*/
	 static int MXC622X_SetBWRate(struct i2c_client *client, u8 bwrate)
	 {
	 #if 0
		 u8 databuf[10];	
		 int res = 0;
	 
		 memset(databuf, 0, sizeof(u8)*10);    
	 
		 if(hwmsen_read_block(client, MXC622X_REG_BW_RATE, databuf, 0x01))
		 {
			 printk("mxc622x read rate failt \n");
			 return MXC622X_ERR_I2C;
		 }
	 
		 databuf[0] &= ~MXC622X_BW_MASK;
		 databuf[0] |= bwrate;
		 databuf[1] = databuf[0];
		 databuf[0] = MXC622X_REG_BW_RATE;
	 
	 
		 res = cust_i2c_master_send(client, databuf, 0x2);
	 
		 if(res <= 0)
		 {
			 return MXC622X_ERR_I2C;
		 }
		 
		 //printk("MXC622X_SetBWRate OK! \n");
	  #endif	 
		 return MXC622X_SUCCESS;    
	 }
	 /*----------------------------------------------------------------------------*/
	 static int MXC622X_SetIntEnable(struct i2c_client *client, u8 intenable)
	 {
	 #if 0
				 u8 databuf[10];	
				 int res = 0;
			 
				 res = hwmsen_write_byte(client, MXC622X_INT_REG, 0x00);
				 if(res != MXC622X_SUCCESS) 
				 {
					 return res;
				 }
				 printk("MXC622X disable interrupt ...\n");
			 
				 /*for disable interrupt function*/
	 #endif			 
				 return MXC622X_SUCCESS;    
	 }
	
	static int MXC622X_CheckDeviceID(struct i2c_client *client)
	 {
		 u8 databuf[10];
		 int res = 0;

		 while(res < 50)
		 {
		cust_hwmsen_read_block(client, 0x08, databuf, 1);
		msleep(1);

		 printk("zhaofei databuf[0] is %x\n", databuf[0]);
		 res++;

		 if( (databuf[0] & 0x3F) == 0x25)
			 break;
		 }
		 if(res > 10)
			 return -1;

				 return MXC622X_SUCCESS;    
	 }
	 
	 /*----------------------------------------------------------------------------*/
	 static int mxc622x_init_client(struct i2c_client *client, int reset_cali)
	 {
		 struct mxc622x_i2c_data *obj = i2c_get_clientdata(client);
		 int res = 0;
	 
		 res = MXC622X_CheckDeviceID(client); 
		 if(res != MXC622X_SUCCESS)
		 {
		 	 return res;
		 }	 
		 
		 res = MXC622X_SetPowerMode(client, false);
		 if(res != MXC622X_SUCCESS)
		 {
			 return res;
		 }
		 printk("MXC622X_SetPowerMode OK!\n");
		 
		 res = MXC622X_SetBWRate(client, MXC622X_BW_100HZ);
		 if(res != MXC622X_SUCCESS ) 
		 {
			 return res;
		 }
		 printk("MXC622X_SetBWRate OK!\n");
		 
		 res = MXC622X_SetDataFormat(client, MXC622X_RANGE_2G);
		 if(res != MXC622X_SUCCESS) 
		 {
			 return res;
		 }
		 printk("MXC622X_SetDataFormat OK!\n");
	 
		 gsensor_gain.x = gsensor_gain.y = gsensor_gain.z = obj->reso->sensitivity;
	 
	 
		 res = MXC622X_SetIntEnable(client, 0x00);		 
		 if(res != MXC622X_SUCCESS)
		 {
			 return res;
		 }
		 printk("MXC622X disable interrupt function!\n");
	 
	 
		 if(0 != reset_cali)
		 { 
			 /*reset calibration only in power on*/
			 res = MXC622X_ResetCalibration(client);
			 if(res != MXC622X_SUCCESS)
			 {
				 return res;
			 }
		 }
		 printk("mxc622x_init_client OK!\n");
#ifdef CONFIG_MXC622X_LOWPASS
		 memset(&obj->fir, 0x00, sizeof(obj->fir));  
#endif
	 
		 mdelay(20);
	 
		 return MXC622X_SUCCESS;
	 }
	 /*----------------------------------------------------------------------------*/
	 static int MXC622X_ReadChipInfo(struct i2c_client *client, char *buf, int bufsize)
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
	 
		 sprintf(buf, "MXC622X Chip");
		 return 0;
	 }
	 /*----------------------------------------------------------------------------*/
	 static int MXC622X_ReadSensorData(struct i2c_client *client, char *buf, int bufsize)
	 {
		 struct mxc622x_i2c_data *obj = (struct mxc622x_i2c_data*)i2c_get_clientdata(client);
		 u8 databuf[20];
		 int acc[MXC622X_AXES_NUM];
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
			 res = MXC622X_SetPowerMode(client, true);
			 if(res)
			 {
				 GSE_ERR("Power on mxc622x error %d!\n", res);
			 }
		 }
	 
		 if(res = MXC622X_ReadData(client, obj->data))
		 {		  
			 GSE_ERR("I2C error: ret value=%d", res);
			 return -3;
		 }
		 else
		 {
			 //printk("raw data x=%d, y=%d, z=%d \n",obj->data[MXC622X_AXIS_X],obj->data[MXC622X_AXIS_Y],obj->data[MXC622X_AXIS_Z]);
			 obj->data[MXC622X_AXIS_X] += obj->cali_sw[MXC622X_AXIS_X];
			 obj->data[MXC622X_AXIS_Y] += obj->cali_sw[MXC622X_AXIS_Y];

			 
			 //printk("cali_sw x=%d, y=%d, z=%d \n",obj->cali_sw[MXC622X_AXIS_X],obj->cali_sw[MXC622X_AXIS_Y],obj->cali_sw[MXC622X_AXIS_Z]);
			 
			 /*remap coordinate*/
			 acc[obj->cvt.map[MXC622X_AXIS_X]] = obj->cvt.sign[MXC622X_AXIS_X]*obj->data[MXC622X_AXIS_X];
			 acc[obj->cvt.map[MXC622X_AXIS_Y]] = obj->cvt.sign[MXC622X_AXIS_Y]*obj->data[MXC622X_AXIS_Y];

			 //printk("cvt x=%d, y=%d, z=%d \n",obj->cvt.sign[MXC622X_AXIS_X],obj->cvt.sign[MXC622X_AXIS_Y],obj->cvt.sign[MXC622X_AXIS_Z]);
	 
            if(atomic_read(&obj->trace) & ADX_TRC_IOCTL)
            {
                GSE_LOG("Mapped gsensor data: %d, %d!\n", acc[MXC622X_AXIS_X], acc[MXC622X_AXIS_Y]);
            }
	 
			 //Out put the mg
			 //printk("mg acc=%d, GRAVITY=%d, sensityvity=%d \n",acc[MXC622X_AXIS_X],GRAVITY_EARTH_1000,obj->reso->sensitivity);
			 acc[MXC622X_AXIS_X] = acc[MXC622X_AXIS_X] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
			 acc[MXC622X_AXIS_Y] = acc[MXC622X_AXIS_Y] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;

            if(atomic_read(&obj->trace) & ADX_TRC_IOCTL)
            {
                GSE_LOG("after * GRAVITY_EARTH_1000 / obj->reso->sensitivity: %d, %d!\n", acc[MXC622X_AXIS_X], acc[MXC622X_AXIS_Y]);
            }
		 
	 		acc[MXC622X_AXIS_Z] = -mxc622x_sqrt(9807, 0, 9807*9807-acc[MXC622X_AXIS_X]*acc[MXC622X_AXIS_X]-acc[MXC622X_AXIS_Y]*acc[MXC622X_AXIS_Y]);
	 
			 sprintf(buf, "%04x %04x %04x", acc[MXC622X_AXIS_X], acc[MXC622X_AXIS_Y], acc[MXC622X_AXIS_Z]);
			 if(atomic_read(&obj->trace) & ADX_TRC_IOCTL)
			 {
				 GSE_LOG("gsensor data: %s!\n", buf);
			 }
		 }
		 
		 return 0;
	 }
	 /*----------------------------------------------------------------------------*/
	 static int MXC622X_ReadRawData(struct i2c_client *client, char *buf)
	 {
		 struct mxc622x_i2c_data *obj = (struct mxc622x_i2c_data*)i2c_get_clientdata(client);
		 int res = 0;
	 
		 if (!buf || !client)
		 {
			 return EINVAL;
		 }
		 
		 if(res = MXC622X_ReadData(client, obj->data))
		 {		  
			 GSE_ERR("I2C error: ret value=%d", res);
			 return EIO;
		 }
		 else
		 {
			 sprintf(buf, "MXC622X_ReadRawData %04x %04x", obj->data[MXC622X_AXIS_X], 
				 obj->data[MXC622X_AXIS_Y]);
		 
		 }
		 
		 return 0;
	 }
	 /*----------------------------------------------------------------------------*/
	 static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
	 {
		 struct i2c_client *client = mxc622x_i2c_client;
		 char strbuf[MXC622X_BUFSIZE];
		 if(NULL == client)
		 {
			 GSE_ERR("i2c client is null!!\n");
			 return 0;
		 }
		 
		 MXC622X_ReadChipInfo(client, strbuf, MXC622X_BUFSIZE);
		 return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);		 
	 }
	 	 
	 /*----------------------------------------------------------------------------*/
	 static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
	 {
		 struct i2c_client *client = mxc622x_i2c_client;
		 char strbuf[MXC622X_BUFSIZE];
		 
		 if(NULL == client)
		 {
			 GSE_ERR("i2c client is null!!\n");
			 return 0;
		 }
		 MXC622X_ReadSensorData(client, strbuf, MXC622X_BUFSIZE);
		 //MXC622X_ReadRawData(client, strbuf);
		 return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);			 
	 }
	 
	 static ssize_t show_sensorrawdata_value(struct device_driver *ddri, char *buf, size_t count)
		 {
			 struct i2c_client *client = mxc622x_i2c_client;
			 char strbuf[MXC622X_BUFSIZE];
			 
			 if(NULL == client)
			 {
				 GSE_ERR("i2c client is null!!\n");
				 return 0;
			 }
			 //MXC622X_ReadSensorData(client, strbuf, MXC622X_BUFSIZE);
			 MXC622X_ReadRawData(client, strbuf);
			 return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);			 
		 }
	 
	 /*----------------------------------------------------------------------------*/
	 static ssize_t show_cali_value(struct device_driver *ddri, char *buf)
	 {
		 struct i2c_client *client = mxc622x_i2c_client;
		 struct mxc622x_i2c_data *obj;
		 int err, len = 0, mul;
		 int tmp[MXC622X_AXES_NUM];
	 
		 if(NULL == client)
		 {
			 GSE_ERR("i2c client is null!!\n");
			 return 0;
		 }
	 
		 obj = i2c_get_clientdata(client);
	 
	 
	 
		 if(err = MXC622X_ReadOffset(client, obj->offset))
		 {
			 return -EINVAL;
		 }
		 else if(err = MXC622X_ReadCalibration(client, tmp))
		 {
			 return -EINVAL;
		 }
		 else
		 {	  
			 mul = obj->reso->sensitivity/mxc622x_offset_resolution.sensitivity;
			 len += snprintf(buf+len, PAGE_SIZE-len, "[HW ][%d] (%+3d, %+3d) : (0x%02X, 0x%02X)\n", mul,						  
				 obj->offset[MXC622X_AXIS_X], obj->offset[MXC622X_AXIS_Y],
				 obj->offset[MXC622X_AXIS_X], obj->offset[MXC622X_AXIS_Y]);
			 len += snprintf(buf+len, PAGE_SIZE-len, "[SW ][%d] (%+3d, %+3d)\n", 1, 
				 obj->cali_sw[MXC622X_AXIS_X], obj->cali_sw[MXC622X_AXIS_Y]);
	 
			 len += snprintf(buf+len, PAGE_SIZE-len, "[ALL]    (%+3d, %+3d) : (%+3d, %+3d)\n", 
				 obj->offset[MXC622X_AXIS_X]*mul + obj->cali_sw[MXC622X_AXIS_X],
				 obj->offset[MXC622X_AXIS_Y]*mul + obj->cali_sw[MXC622X_AXIS_Y],
				 tmp[MXC622X_AXIS_X], tmp[MXC622X_AXIS_Y]);
			 
			 return len;
		 }
	 }
	 /*----------------------------------------------------------------------------*/
	 static ssize_t store_cali_value(struct device_driver *ddri, char *buf, size_t count)
	 {
		 struct i2c_client *client = mxc622x_i2c_client;  
		 int err, x, y, z;
		 int dat[MXC622X_AXES_NUM];
	 
		 if(!strncmp(buf, "rst", 3))
		 {
			 if(err = MXC622X_ResetCalibration(client))
			 {
				 GSE_ERR("reset offset err = %d\n", err);
			 }	 
		 }
		 else if(2 == sscanf(buf, "0x%02X 0x%02X", &x, &y))
		 {
			 dat[MXC622X_AXIS_X] = x;
			 dat[MXC622X_AXIS_Y] = y;
			 
			 if(err = MXC622X_WriteCalibration(client, dat))
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
	 static ssize_t show_firlen_value(struct device_driver *ddri, char *buf)
	 {
#ifdef CONFIG_MXC622X_LOWPASS
		 struct i2c_client *client = mxc622x_i2c_client;
		 struct mxc622x_i2c_data *obj = i2c_get_clientdata(client);
		 if(atomic_read(&obj->firlen))
		 {
			 int idx, len = atomic_read(&obj->firlen);
			 GSE_LOG("len = %2d, idx = %2d\n", obj->fir.num, obj->fir.idx);
	 
			 for(idx = 0; idx < len; idx++)
			 {
				 GSE_LOG("[%5d %5d]\n", obj->fir.raw[idx][MXC622X_AXIS_X], obj->fir.raw[idx][MXC622X_AXIS_Y]);
			 }
			 
			 GSE_LOG("sum = [%5d %5d]\n", obj->fir.sum[MXC622X_AXIS_X], obj->fir.sum[MXC622X_AXIS_Y]);
			 GSE_LOG("avg = [%5d %5d]\n", obj->fir.sum[MXC622X_AXIS_X]/len, obj->fir.sum[MXC622X_AXIS_Y]/len);
		 }
		 return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&obj->firlen));
#else
		 return snprintf(buf, PAGE_SIZE, "not support\n");
#endif
	 }
	 /*----------------------------------------------------------------------------*/
	 static ssize_t store_firlen_value(struct device_driver *ddri, char *buf, size_t count)
	 {
#ifdef CONFIG_MXC622X_LOWPASS
		 struct i2c_client *client = mxc622x_i2c_client;  
		 struct mxc622x_i2c_data *obj = i2c_get_clientdata(client);
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
		 struct mxc622x_i2c_data *obj = obj_i2c_data;
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
		 struct mxc622x_i2c_data *obj = obj_i2c_data;
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
		 struct mxc622x_i2c_data *obj = obj_i2c_data;
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
static ssize_t show_layout_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = mxc622x_i2c_client;  
	struct mxc622x_i2c_data *data = i2c_get_clientdata(client);

	return sprintf(buf, "(%d, %d)\n[%+2d %+2d %+2d]\n[%+2d %+2d %+2d]\n",
		data->hw->direction,atomic_read(&data->layout),	data->cvt.sign[0], data->cvt.sign[1],
		data->cvt.sign[2],data->cvt.map[0], data->cvt.map[1], data->cvt.map[2]);            
}
/*----------------------------------------------------------------------------*/
static ssize_t store_layout_value(struct device_driver *ddri, char *buf, size_t count)
{
	struct i2c_client *client = mxc622x_i2c_client;  
	struct mxc622x_i2c_data *data = i2c_get_clientdata(client);
	int layout = 0;

	if(1 == sscanf(buf, "%d", &layout))
	{
		atomic_set(&data->layout, layout);
		if(!hwmsen_get_convert(layout, &data->cvt))
		{
			printk(KERN_ERR "HWMSEN_GET_CONVERT function error!\r\n");
		}
		else if(!hwmsen_get_convert(data->hw->direction, &data->cvt))
		{
			printk(KERN_ERR "invalid layout: %d, restore to %d\n", layout, data->hw->direction);
		}
		else
		{
			printk(KERN_ERR "invalid layout: (%d, %d)\n", layout, data->hw->direction);
			hwmsen_get_convert(0, &data->cvt);
		}
	}
	else
	{
		printk(KERN_ERR "invalid format = '%s'\n", buf);
	}
	
	return count;            
}
	 /*----------------------------------------------------------------------------*/
	 static DRIVER_ATTR(chipinfo,	S_IWUSR | S_IRUGO, show_chipinfo_value, 	 NULL);
	 static DRIVER_ATTR(sensordata, S_IWUSR | S_IRUGO, show_sensordata_value,	 NULL);
	 static DRIVER_ATTR(cali,		S_IWUSR | S_IRUGO, show_cali_value, 		 store_cali_value);
	 static DRIVER_ATTR(firlen, 	S_IWUSR | S_IRUGO, show_firlen_value,		 store_firlen_value);
	 static DRIVER_ATTR(trace,		S_IWUSR | S_IRUGO, show_trace_value,		 store_trace_value);
	 static DRIVER_ATTR(layout,      S_IRUGO | S_IWUSR, show_layout_value, store_layout_value);
	 static DRIVER_ATTR(status, 			  S_IRUGO, show_status_value,		 NULL);
	 static DRIVER_ATTR(powerstatus,			   S_IRUGO, show_power_status_value,		NULL);
	 
	 /*----------------------------------------------------------------------------*/
	 static struct driver_attribute *mxc622x_attr_list[] = {
		 &driver_attr_chipinfo, 	/*chip information*/
		 &driver_attr_sensordata,	/*dump sensor data*/
		 &driver_attr_cali, 		/*show calibration data*/
		 &driver_attr_firlen,		/*filter length: 0: disable, others: enable*/
		 &driver_attr_trace,		/*trace log*/
		 &driver_attr_layout,
		 &driver_attr_status,
		 &driver_attr_powerstatus,
	 };
	 /*----------------------------------------------------------------------------*/
	 static int mxc622x_create_attr(struct device_driver *driver) 
	 {
		 int idx, err = 0;
		 int num = (int)(sizeof(mxc622x_attr_list)/sizeof(mxc622x_attr_list[0]));
		 if (driver == NULL)
		 {
			 return -EINVAL;
		 }
	 
		 for(idx = 0; idx < num; idx++)
		 {
			 if(err = driver_create_file(driver, mxc622x_attr_list[idx]))
			 {			  
				 GSE_ERR("driver_create_file (%s) = %d\n", mxc622x_attr_list[idx]->attr.name, err);
				 break;
			 }
		 }	  
		 return err;
	 }
	 /*----------------------------------------------------------------------------*/
	 static int mxc622x_delete_attr(struct device_driver *driver)
	 {
		 int idx ,err = 0;
		 int num = (int)(sizeof(mxc622x_attr_list)/sizeof(mxc622x_attr_list[0]));
	 
		 if(driver == NULL)
		 {
			 return -EINVAL;
		 }
		 
	 
		 for(idx = 0; idx < num; idx++)
		 {
			 driver_remove_file(driver, mxc622x_attr_list[idx]);
		 }
		 
	 
		 return err;
	 }
	 
	 /*----------------------------------------------------------------------------*/
	 int gsensor_operate(void* self, uint32_t command, void* buff_in, int size_in,
			 void* buff_out, int size_out, int* actualout)
	 {
		 int err = 0;
		 int value, sample_delay;	 
		 struct mxc622x_i2c_data *priv = (struct mxc622x_i2c_data*)self;
		 hwm_sensor_data* gsensor_data;
		 char buff[MXC622X_BUFSIZE];
		 
		 //GSE_FUN(f);
		 switch (command)
		 {
			 case SENSOR_DELAY:
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
						 err = MXC622X_SetPowerMode( priv->client, !sensor_power);
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
					 MXC622X_ReadSensorData(priv->client, buff, MXC622X_BUFSIZE);
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
	 static int mxc622x_open(struct inode *inode, struct file *file)
	 {
		 file->private_data = mxc622x_i2c_client;
	 
		 if(file->private_data == NULL)
		 {
			 GSE_ERR("null pointer!!\n");
			 return -EINVAL;
		 }
		 return nonseekable_open(inode, file);
	 }
	 /*----------------------------------------------------------------------------*/
	 static int mxc622x_release(struct inode *inode, struct file *file)
	 {
		 file->private_data = NULL;
		 return 0;
	 }
	 /*----------------------------------------------------------------------------*/
	 //static int mxc622x_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
		 //   unsigned long arg)
	 static int mxc622x_unlocked_ioctl(struct file *file, unsigned int cmd,
			unsigned long arg)		 
	 {
		 struct i2c_client *client = (struct i2c_client*)file->private_data;
		 struct mxc622x_i2c_data *obj = (struct mxc622x_i2c_data*)i2c_get_clientdata(client);  
		 char strbuf[MXC622X_BUFSIZE];
		 void __user *data;
		 SENSOR_DATA sensor_data;
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
				 mxc622x_init_client(client, 0); 		 
				 break;
	 
			 case GSENSOR_IOCTL_READ_CHIPINFO:
				 data = (void __user *) arg;
				 if(data == NULL)
				 {
					 err = -EINVAL;
					 break;    
				 }
				 
				 MXC622X_ReadChipInfo(client, strbuf, MXC622X_BUFSIZE);
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
				 
				 MXC622X_ReadSensorData(client, strbuf, MXC622X_BUFSIZE);
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
				 MXC622X_ReadRawData(client, strbuf);
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
					 cali[MXC622X_AXIS_X] = sensor_data.x * obj->reso->sensitivity / GRAVITY_EARTH_1000;
					 cali[MXC622X_AXIS_Y] = sensor_data.y * obj->reso->sensitivity / GRAVITY_EARTH_1000;
					  		   
					 err = MXC622X_WriteCalibration(client, cali);			  
				 }
				 break;
	 
			 case GSENSOR_IOCTL_CLR_CALI:
				 err = MXC622X_ResetCalibration(client);
				 break;
	 
			 case GSENSOR_IOCTL_GET_CALI:
				 data = (void __user*)arg;
				 if(data == NULL)
				 {
					 err = -EINVAL;
					 break;    
				 }
				 if(err = MXC622X_ReadCalibration(client, cali))
				 {
					 break;
				 }
				 
				 sensor_data.x = cali[MXC622X_AXIS_X] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
				 sensor_data.y = cali[MXC622X_AXIS_Y] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
				 
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
	 static struct file_operations mxc622x_fops = {
		 //.owner = THIS_MODULE,
		 .open = mxc622x_open,
		 .release = mxc622x_release,
		 .unlocked_ioctl = mxc622x_unlocked_ioctl,
	 };
	 /*----------------------------------------------------------------------------*/
	 static struct miscdevice mxc622x_device = {
		 .minor = MISC_DYNAMIC_MINOR,
		 .name = "gsensor",
		 .fops = &mxc622x_fops,
	 };
	 /*----------------------------------------------------------------------------*/
#ifndef CONFIG_HAS_EARLYSUSPEND
	 /*----------------------------------------------------------------------------*/
	 static int mxc622x_suspend(struct i2c_client *client, pm_message_t msg) 
	 {
		 struct mxc622x_i2c_data *obj = i2c_get_clientdata(client);	  
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
			 if(err = MXC622X_SetPowerMode(obj->client, false))
			 {
				 GSE_ERR("write power control fail!!\n");
				 return;
			 }		 
			 MXC622X_power(obj->hw, 0);
		 }
		 return err;
	 }
	 /*----------------------------------------------------------------------------*/
	 static int mxc622x_resume(struct i2c_client *client)
	 {
		 struct mxc622x_i2c_data *obj = i2c_get_clientdata(client);		  
		 int err;
		 GSE_FUN();
	 
		 if(obj == NULL)
		 {
			 GSE_ERR("null pointer!!\n");
			 return -EINVAL;
		 }
	 
		 MXC622X_power(obj->hw, 1);
		 if(err = mxc622x_init_client(client, 0))
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
	 static void mxc622x_early_suspend(struct early_suspend *h) 
	 {
		 struct mxc622x_i2c_data *obj = container_of(h, struct mxc622x_i2c_data, early_drv);	 
		 int err;
		 GSE_FUN();    
	 
		 if(obj == NULL)
		 {
			 GSE_ERR("null pointer!!\n");
			 return;
		 }
		 atomic_set(&obj->suspend, 1); 
		 if(err = MXC622X_SetPowerMode(obj->client, false))
		 {
			 GSE_ERR("write power control fail!!\n");
			 return;
		 }
	 
		 sensor_power = false;
		 
		 MXC622X_power(obj->hw, 0);
	 }
	 /*----------------------------------------------------------------------------*/
	 static void mxc622x_late_resume(struct early_suspend *h)
	 {
		 struct mxc622x_i2c_data *obj = container_of(h, struct mxc622x_i2c_data, early_drv);		   
		 int err;
		 GSE_FUN();
	 
		 if(obj == NULL)
		 {
			 GSE_ERR("null pointer!!\n");
			 return;
		 }
	 
		 MXC622X_power(obj->hw, 1);
		 if(err = mxc622x_init_client(obj->client, 0))
		 {
			 GSE_ERR("initialize client fail!!\n");
			 return;		
		 }
		 atomic_set(&obj->suspend, 0);	  
	 }
	 /*----------------------------------------------------------------------------*/
#endif /*CONFIG_HAS_EARLYSUSPEND*/

	 /*----------------------------------------------------------------------------*/
	 static int mxc622x_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
	 {
		 struct i2c_client *new_client;
		 struct mxc622x_i2c_data *obj;
		 struct hwmsen_object sobj;
		 int err = 0;

		 GSE_FUN();
	 
		 if(!(obj = kzalloc(sizeof(*obj), GFP_KERNEL)))
		 {
			 err = -ENOMEM;
			 goto exit;
		 }
		 
		 memset(obj, 0, sizeof(struct mxc622x_i2c_data));
	 
		 obj->hw = get_cust_acc_hw();
		 atomic_set(&obj->layout, obj->hw->direction);
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
		 
#ifdef CONFIG_MXC622X_LOWPASS
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
	 
		 mxc622x_i2c_client = new_client; 
	 
		 if(err = mxc622x_init_client(new_client, 1))
		 {
			 goto exit_init_failed;
		 }
		 
	 
		 if(err = misc_register(&mxc622x_device))
		 {
			 GSE_ERR("mxc622x_device register failed\n");
			 goto exit_misc_device_register_failed;
		 }
	 
		 if(err = mxc622x_create_attr(&mxc622x_gsensor_driver.driver))
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
		 obj->early_drv.level	 = EARLY_SUSPEND_LEVEL_DISABLE_FB - 1,
		 obj->early_drv.suspend  = mxc622x_early_suspend,
		 obj->early_drv.resume	 = mxc622x_late_resume,	  
		 register_early_suspend(&obj->early_drv);
#endif 
	 
		 GSE_LOG("%s: OK\n", __func__);    
		 return 0;
	 
		 exit_create_attr_failed:
		 misc_deregister(&mxc622x_device);
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
	 static int mxc622x_i2c_remove(struct i2c_client *client)
	 {
		 int err = 0;	 
		 
		 if(err = mxc622x_delete_attr(&mxc622x_gsensor_driver.driver))
		 {
			 GSE_ERR("mxc622x_delete_attr fail: %d\n", err);
		 }
		 
		 if(err = misc_deregister(&mxc622x_device))
		 {
			 GSE_ERR("misc_deregister fail: %d\n", err);
		 }
	 
		 if(err = hwmsen_detach(ID_ACCELEROMETER))
			 
	 
		 mxc622x_i2c_client = NULL;
		 i2c_unregister_device(client);
		 kfree(i2c_get_clientdata(client));
		 return 0;
	 }
	 /*----------------------------------------------------------------------------*/
	 static int mxc622x_probe(struct platform_device *pdev) 
	 {
		 struct acc_hw *hw = get_cust_acc_hw();
		 GSE_FUN();
	 
		 MXC622X_power(hw, 1);
		 //mxc622x_force[0] = hw->i2c_num;
		 if(i2c_add_driver(&mxc622x_i2c_driver))
		 {
			 GSE_ERR("add driver error\n");
			 return -1;
		 }
		 return 0;
	 }
	 /*----------------------------------------------------------------------------*/
	 static int mxc622x_remove(struct platform_device *pdev)
	 {
		 struct acc_hw *hw = get_cust_acc_hw();
	 
		 GSE_FUN();    
		 MXC622X_power(hw, 0);	 
		 i2c_del_driver(&mxc622x_i2c_driver);
		 return 0;
	 }
	 /*----------------------------------------------------------------------------*/
	 static struct platform_driver mxc622x_gsensor_driver = {
		 .probe 	 = mxc622x_probe,
		 .remove	 = mxc622x_remove,	 
		 .driver	 = {
			 .name	= "gsensor",
			 //.owner = THIS_MODULE,
		 }
	 };
	 
	 /*----------------------------------------------------------------------------*/
	 static int __init mxc622x_init(void)
	 {
		 GSE_FUN();
		 struct acc_hw *hw = get_cust_acc_hw();
		 GSE_LOG("%s: i2c_number=%d\n", __func__,hw->i2c_num); 
		 i2c_register_board_info(hw->i2c_num, &i2c_mxc622x, 1);
		 if(platform_driver_register(&mxc622x_gsensor_driver))
		 {
			 GSE_ERR("failed to register driver");
			 return -ENODEV;
		 }
		mutex_init(&g_gsensor_mutex);
		 return 0;	  
	 }
	 /*----------------------------------------------------------------------------*/
	 static void __exit mxc622x_exit(void)
	 {
		 GSE_FUN();
		 platform_driver_unregister(&mxc622x_gsensor_driver);
	mutex_destroy(&g_gsensor_mutex);
	 }
	 /*----------------------------------------------------------------------------*/
	 module_init(mxc622x_init);
	 module_exit(mxc622x_exit);
	 /*----------------------------------------------------------------------------*/
	 MODULE_LICENSE("GPL");
	 MODULE_DESCRIPTION("MXC622X I2C driver");

