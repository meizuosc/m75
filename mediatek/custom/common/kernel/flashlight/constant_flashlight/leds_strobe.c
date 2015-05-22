#ifndef MTK_MT6333_SUPPORT
//###########################################
//##                                       ##
//##        MT6333 NOT support             ##
//## (MT6333 support code from L426)       ##
//## (key word MT63333)                    ##
//###########################################

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/time.h>
#include "kd_flashlight.h"
#include <asm/io.h>
#include <asm/uaccess.h>
#include "kd_camera_hw.h"
#include <cust_gpio_usage.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/xlog.h>
#include <linux/version.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
#include <linux/mutex.h>
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
#include <linux/semaphore.h>
#else
#include <asm/semaphore.h>
#endif
#endif

/******************************************************************************
 * Debug configuration
******************************************************************************/
// availible parameter
// ANDROID_LOG_ASSERT
// ANDROID_LOG_ERROR
// ANDROID_LOG_WARNING
// ANDROID_LOG_INFO
// ANDROID_LOG_DEBUG
// ANDROID_LOG_VERBOSE
#define TAG_NAME "leds_strobe.c"
#define PK_DBG_NONE(fmt, arg...)    do {} while (0)
#define PK_DBG_FUNC(fmt, arg...)    xlog_printk(ANDROID_LOG_DEBUG  , TAG_NAME, KERN_INFO  "%s: " fmt, __FUNCTION__ ,##arg)
#define PK_WARN(fmt, arg...)        xlog_printk(ANDROID_LOG_WARNING, TAG_NAME, KERN_WARNING  "%s: " fmt, __FUNCTION__ ,##arg)
#define PK_NOTICE(fmt, arg...)      xlog_printk(ANDROID_LOG_DEBUG  , TAG_NAME, KERN_NOTICE  "%s: " fmt, __FUNCTION__ ,##arg)
#define PK_INFO(fmt, arg...)        xlog_printk(ANDROID_LOG_INFO   , TAG_NAME, KERN_INFO  "%s: " fmt, __FUNCTION__ ,##arg)
#define PK_TRC_FUNC(f)              xlog_printk(ANDROID_LOG_DEBUG  , TAG_NAME,  "<%s>\n", __FUNCTION__);
#define PK_TRC_VERBOSE(fmt, arg...) xlog_printk(ANDROID_LOG_VERBOSE, TAG_NAME,  fmt, ##arg)
#define PK_ERROR(fmt, arg...)       xlog_printk(ANDROID_LOG_ERROR  , TAG_NAME, KERN_ERR "%s: " fmt, __FUNCTION__ ,##arg)


#define DEBUG_LEDS_STROBE
#ifdef  DEBUG_LEDS_STROBE
	#define PK_DBG PK_DBG_FUNC
	#define PK_VER PK_TRC_VERBOSE
	#define PK_ERR PK_ERROR
#else
	#define PK_DBG(a,...)
	#define PK_VER(a,...)
	#define PK_ERR(a,...)
#endif

/******************************************************************************
 * local variables
******************************************************************************/

static DEFINE_SPINLOCK(g_strobeSMPLock); /* cotta-- SMP proection */


static u32 strobe_Res = 0;
static u32 strobe_Timeus = 0;
static BOOL g_strobe_On = 0;

static int g_duty=-1;
static int g_timeOutTimeMs=0;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
static DEFINE_MUTEX(g_strobeSem);
#else
static DECLARE_MUTEX(g_strobeSem);
#endif


#define STROBE_DEVICE_ID 0xC6


static struct work_struct workTimeOut;

//@@ #define FLASH_GPIO_ENF GPIO12
//@@ #define FLASH_GPIO_ENT GPIO13

/*****************************************************************************
Functions
*****************************************************************************/
extern int iWriteRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u16 i2cId);
extern int iReadRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u8 * a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
static void work_timeOutFunc(struct work_struct *data);



int FL_Enable(void)
{
	char buf[2];
	char bufR[2];
	buf[0]=1;
	buf[1]=0x00;
	iWriteRegI2C(buf , 2, STROBE_DEVICE_ID);

	buf[0]=10;
	buf[1]=0x02;
	iWriteRegI2C(buf , 2, STROBE_DEVICE_ID);

	buf[0]=9;
	buf[1]=0x73;
	iWriteRegI2C(buf , 2, STROBE_DEVICE_ID);
	PK_DBG(" FL_Enable line=%d\n",__LINE__);

//@@ 	mt_set_gpio_out(FLASH_GPIO_ENF,GPIO_OUT_ONE);
//@@ 	mt_set_gpio_out(FLASH_GPIO_ENT,GPIO_OUT_ONE);
	//////////////////////////////////////////////
	//////////////////////////////////////////////

	buf[0]=0;
	iReadRegI2C(buf, 1, bufR, 1, STROBE_DEVICE_ID);
	PK_DBG(" FL_Enable line=%d reg0 %d\n",__LINE__, (int)bufR[0]);

	buf[0]=1;
	iReadRegI2C(buf, 1, bufR, 1, STROBE_DEVICE_ID);
	PK_DBG(" FL_Enable line=%d reg1 %d\n",__LINE__, (int)bufR[0]);

	buf[0]=6;
	iReadRegI2C(buf, 1, bufR, 1, STROBE_DEVICE_ID);
	PK_DBG(" FL_Enable line=%d reg6 %d\n",__LINE__, (int)bufR[0]);

	buf[0]=8;
	iReadRegI2C(buf, 1, bufR, 1, STROBE_DEVICE_ID);
	PK_DBG(" FL_Enable line=%d reg8 %d\n",__LINE__, (int)bufR[0]);

	buf[0]=9;
	iReadRegI2C(buf, 1, bufR, 1, STROBE_DEVICE_ID);
	PK_DBG(" FL_Enable line=%d reg9 %d\n",__LINE__, (int)bufR[0]);

	buf[0]=10;
	iReadRegI2C(buf, 1, bufR, 1, STROBE_DEVICE_ID);
	PK_DBG(" FL_Enable line=%d regA %d\n",__LINE__, (int)bufR[0]);

	buf[0]=11;
	iReadRegI2C(buf, 1, bufR, 1, STROBE_DEVICE_ID);
	PK_DBG(" FL_Enable line=%d regB %d\n",__LINE__, (int)bufR[0]);




    return 0;
}



int FL_Disable(void)
{
		char buf[2];
		char bufR[2];


		buf[0]=0;
	iReadRegI2C(buf, 1, bufR, 1, STROBE_DEVICE_ID);
	PK_DBG(" FL_Enable0 line=%d reg0 %d\n",__LINE__, (int)bufR[0]);

	buf[0]=1;
	iReadRegI2C(buf, 1, bufR, 1, STROBE_DEVICE_ID);
	PK_DBG(" FL_Enable0 line=%d reg1 %d\n",__LINE__, (int)bufR[0]);

	buf[0]=6;
	iReadRegI2C(buf, 1, bufR, 1, STROBE_DEVICE_ID);
	PK_DBG(" FL_Enable0 line=%d reg6 %d\n",__LINE__, (int)bufR[0]);

	buf[0]=8;
	iReadRegI2C(buf, 1, bufR, 1, STROBE_DEVICE_ID);
	PK_DBG(" FL_Enable0 line=%d reg8 %d\n",__LINE__, (int)bufR[0]);

	buf[0]=9;
	iReadRegI2C(buf, 1, bufR, 1, STROBE_DEVICE_ID);
	PK_DBG(" FL_Enable0 line=%d reg9 %d\n",__LINE__, (int)bufR[0]);

	buf[0]=10;
	iReadRegI2C(buf, 1, bufR, 1, STROBE_DEVICE_ID);
	PK_DBG(" FL_Enable0 line=%d regA %d\n",__LINE__, (int)bufR[0]);

	buf[0]=11;
	iReadRegI2C(buf, 1, bufR, 1, STROBE_DEVICE_ID);
	PK_DBG(" FL_Enable0 line=%d regB %d\n",__LINE__, (int)bufR[0]);

///////////////////////
	buf[0]=10;
	buf[1]=0x00;
	iWriteRegI2C(buf , 2, STROBE_DEVICE_ID);
//@@ 	mt_set_gpio_out(FLASH_GPIO_ENF,GPIO_OUT_ZERO);
//@@ 	mt_set_gpio_out(FLASH_GPIO_ENT,GPIO_OUT_ZERO);
	PK_DBG(" FL_Disable line=%d\n",__LINE__);
    return 0;
}

int FL_dim_duty(kal_uint32 duty)
{
	PK_DBG(" FL_dim_duty line=%d\n",__LINE__);
	g_duty =  duty;
    return 0;
}


int FL_Init(void)
{
//@@ 	if(mt_set_gpio_mode(FLASH_GPIO_ENT,GPIO_MODE_00)){PK_DBG("[constant_flashlight] set gpio mode failed!! \n");}
//@@     if(mt_set_gpio_dir(FLASH_GPIO_ENT,GPIO_DIR_OUT)){PK_DBG("[constant_flashlight] set gpio dir failed!! \n");}
//@@     if(mt_set_gpio_out(FLASH_GPIO_ENT,GPIO_OUT_ZERO)){PK_DBG("[constant_flashlight] set gpio failed!! \n");}

//@@     	if(mt_set_gpio_mode(FLASH_GPIO_ENF,GPIO_MODE_00)){PK_DBG("[constant_flashlight] set gpio mode failed!! \n");}
//@@     if(mt_set_gpio_dir(FLASH_GPIO_ENF,GPIO_DIR_OUT)){PK_DBG("[constant_flashlight] set gpio dir failed!! \n");}
//@@     if(mt_set_gpio_out(FLASH_GPIO_ENF,GPIO_OUT_ZERO)){PK_DBG("[constant_flashlight] set gpio failed!! \n");}



	INIT_WORK(&workTimeOut, work_timeOutFunc);
    PK_DBG(" FL_Init line=%d\n",__LINE__);
    return 0;
}


int FL_Uninit(void)
{
	FL_Disable();
    return 0;
}

/*****************************************************************************
User interface
*****************************************************************************/

static void work_timeOutFunc(struct work_struct *data)
{
    FL_Disable();
    PK_DBG("ledTimeOut_callback\n");
    //printk(KERN_ALERT "work handler function./n");
}



enum hrtimer_restart ledTimeOutCallback(struct hrtimer *timer)
{
    schedule_work(&workTimeOut);
    return HRTIMER_NORESTART;
}
static struct hrtimer g_timeOutTimer;
void timerInit(void)
{
	g_timeOutTimeMs=1000; //1s
	hrtimer_init( &g_timeOutTimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL );
	g_timeOutTimer.function=ledTimeOutCallback;

}



static int constant_flashlight_ioctl(MUINT32 cmd, MUINT32 arg)
{
	int i4RetValue = 0;
	int ior_shift;
	int iow_shift;
	int iowr_shift;
	ior_shift = cmd - (_IOR(FLASHLIGHT_MAGIC,0, int));
	iow_shift = cmd - (_IOW(FLASHLIGHT_MAGIC,0, int));
	iowr_shift = cmd - (_IOWR(FLASHLIGHT_MAGIC,0, int));
	PK_DBG("constant_flashlight_ioctl() line=%d ior_shift=%d, iow_shift=%d iowr_shift=%d arg=%d\n",__LINE__, ior_shift, iow_shift, iowr_shift, arg);
    switch(cmd)
    {

		case FLASH_IOC_SET_TIME_OUT_TIME_MS:
			PK_DBG("FLASH_IOC_SET_TIME_OUT_TIME_MS: %d\n",arg);
			g_timeOutTimeMs=arg;
		break;


    	case FLASH_IOC_SET_DUTY :
    		PK_DBG("FLASHLIGHT_DUTY: %d\n",arg);
    		FL_dim_duty(arg);
    		break;


    	case FLASH_IOC_SET_STEP:
    		PK_DBG("FLASH_IOC_SET_STEP: %d\n",arg);

    		break;

    	case FLASH_IOC_SET_ONOFF :
    		PK_DBG("FLASHLIGHT_ONOFF: %d\n",arg);
    		if(arg==1)
    		{
				if(g_timeOutTimeMs!=0)
	            {
	            	ktime_t ktime;
					ktime = ktime_set( 0, g_timeOutTimeMs*1000000 );
					hrtimer_start( &g_timeOutTimer, ktime, HRTIMER_MODE_REL );
	            }
    			FL_Enable();
    		}
    		else
    		{
    			FL_Disable();
				hrtimer_cancel( &g_timeOutTimer );
    		}
    		break;
		default :
    		PK_DBG(" No such command \n");
    		i4RetValue = -EPERM;
    		break;
    }
    return i4RetValue;
}




static int constant_flashlight_open(void *pArg)
{
    int i4RetValue = 0;
    PK_DBG("constant_flashlight_open line=%d\n", __LINE__);

	if (0 == strobe_Res)
	{
	    FL_Init();
		timerInit();
	}
	PK_DBG("constant_flashlight_open line=%d\n", __LINE__);
	spin_lock_irq(&g_strobeSMPLock);


    if(strobe_Res)
    {
        PK_ERR(" busy!\n");
        i4RetValue = -EBUSY;
    }
    else
    {
        strobe_Res += 1;
    }


    spin_unlock_irq(&g_strobeSMPLock);
    PK_DBG("constant_flashlight_open line=%d\n", __LINE__);

    return i4RetValue;

}


static int constant_flashlight_release(void *pArg)
{
    PK_DBG(" constant_flashlight_release\n");

    if (strobe_Res)
    {
        spin_lock_irq(&g_strobeSMPLock);

        strobe_Res = 0;
        strobe_Timeus = 0;

        /* LED On Status */
        g_strobe_On = FALSE;

        spin_unlock_irq(&g_strobeSMPLock);

    	FL_Uninit();
    }

    PK_DBG(" Done\n");

    return 0;

}


FLASHLIGHT_FUNCTION_STRUCT	constantFlashlightFunc=
{
	constant_flashlight_open,
	constant_flashlight_release,
	constant_flashlight_ioctl
};


MUINT32 constantFlashlightInit(PFLASHLIGHT_FUNCTION_STRUCT *pfFunc)
{
    if (pfFunc != NULL)
    {
        *pfFunc = &constantFlashlightFunc;
    }
    return 0;
}



/* LED flash control for high current capture mode*/
ssize_t strobe_VDIrq(void)
{

    return 0;
}

EXPORT_SYMBOL(strobe_VDIrq);


#else
//###########################################
//##                                       ##
//##           MT6333 support              ##
//##                                       ##
//###########################################

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/time.h>
#include "kd_flashlight.h"
#include <asm/io.h>
#include <asm/uaccess.h>
#include "kd_camera_hw.h"
#include <cust_gpio_usage.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/xlog.h>
#include <linux/version.h>
#include <mach/upmu_common.h>
#include <mach/mt6333.h>
#include <mach/pmic_sw.h>
/******************************************************************************
 * Debug configuration
******************************************************************************/
// availible parameter
// ANDROID_LOG_ASSERT
// ANDROID_LOG_ERROR
// ANDROID_LOG_WARNING
// ANDROID_LOG_INFO
// ANDROID_LOG_DEBUG
// ANDROID_LOG_VERBOSE
#define TAG_NAME "leds_strobe.c"
#define PK_DBG_NONE(fmt, arg...)    do {} while (0)
#define PK_DBG_FUNC(fmt, arg...)    xlog_printk(ANDROID_LOG_DEBUG  , TAG_NAME, KERN_INFO  "%s: " fmt, __FUNCTION__ ,##arg)
#define PK_WARN(fmt, arg...)        xlog_printk(ANDROID_LOG_WARNING, TAG_NAME, KERN_WARNING  "%s: " fmt, __FUNCTION__ ,##arg)
#define PK_NOTICE(fmt, arg...)      xlog_printk(ANDROID_LOG_DEBUG  , TAG_NAME, KERN_NOTICE  "%s: " fmt, __FUNCTION__ ,##arg)
#define PK_INFO(fmt, arg...)        xlog_printk(ANDROID_LOG_INFO   , TAG_NAME, KERN_INFO  "%s: " fmt, __FUNCTION__ ,##arg)
#define PK_TRC_FUNC(f)              xlog_printk(ANDROID_LOG_DEBUG  , TAG_NAME,  "<%s>\n", __FUNCTION__);
#define PK_TRC_VERBOSE(fmt, arg...) xlog_printk(ANDROID_LOG_VERBOSE, TAG_NAME,  fmt, ##arg)
#define PK_ERROR(fmt, arg...)       xlog_printk(ANDROID_LOG_ERROR  , TAG_NAME, KERN_ERR "%s: " fmt, __FUNCTION__ ,##arg)


#define DEBUG_LEDS_STROBE
#ifdef  DEBUG_LEDS_STROBE
	#define PK_DBG PK_DBG_FUNC
	#define PK_VER PK_TRC_VERBOSE
	#define PK_ERR PK_ERROR
#else
	#define PK_DBG(a,...)
	#define PK_VER(a,...)
	#define PK_ERR(a,...)
#endif

/******************************************************************************
 * local variables
******************************************************************************/
static DEFINE_SPINLOCK(g_strobeSMPLock); /* cotta-- SMP proection */
static struct work_struct workTimeOut;
static struct work_struct workWDReset;
static int g_duty=0;
static int g_timeOutTimeMs=0;
                       //0, 1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15
static int dimLevel[] = {-1,-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
static int flashCur[] = { 1, 2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14};
static int torchEn [] = { 1, 1,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0};

static int g_reg;
static int g_val;
static u32 strobe_Res = 0;
/*****************************************************************************
Functions
*****************************************************************************/
static void work_timeOutFunc(struct work_struct *data);
static void work_WDResetFunc(struct work_struct *data);
extern kal_uint32 mt6333_config_interface (kal_uint8 RegNum, kal_uint8 val, kal_uint8 MASK, kal_uint8 SHIFT);
extern kal_uint32 mt6333_read_interface (kal_uint8 RegNum, kal_uint8 *val, kal_uint8 MASK, kal_uint8 SHIFT);

void mt6333_set_rg_chrwdt_en(kal_uint8 val); //enable
extern void mt6333_set_rg_chrwdt_wr(kal_uint8 val); //reset
extern void mt6333_set_rg_chrwdt_td(kal_uint8 val); //wdog time set

static void setReg(int reg, int val)
{
    mt6333_config_interface(reg, val, 0xff, 0);
}
static int getReg(int reg)
{
    kal_uint8 valTemp;
    mt6333_read_interface(reg, &valTemp, 0xff, 0);
    return valTemp;
}
static void setRegEx(int reg, int val, int mask, int shift)
{
    int v;
    v=getReg(reg);
    v &= ~(mask << shift);
    v |= (val << shift);
    setReg(reg, v);
}
static int FL_preOn(void)
{
	PK_DBG("FL_preOn");
    setRegEx(0xF3,0x1,0x1,5); //work around for chip issue
    setRegEx(0xF7,0x1,0x1,5); //work around for chip issue
    if(dimLevel[g_duty]>=0)
    {
        setRegEx(0x32,0x1,0x1,4);  //torch mode
        setRegEx(0x35,flashCur[g_duty],0xf,0); //current
        setRegEx(0x33,dimLevel[g_duty],0x1f,0); //dimming
        //setRegEx(0x34,0,0xff,0); //dim freq
        setRegEx(0x34,2,0xff,0); //dim freq
        setRegEx(0x32,1,0x1,0); //dim en
        setRegEx(0x32,1,0x1,6); //dim charger in en
        setRegEx(0xe2,0x40,0xff,0); //workaround, must call
        setRegEx(0xeb,0x40,0xff,0); //workaround, must call
        setRegEx(0x16,0x1,0x1,0); //power source on
    }
	else if(torchEn[g_duty]==1)
    {
        setRegEx(0x32,0x1,0x1,4);  //torch mode
        setRegEx(0x35,flashCur[g_duty],0xf,0); //current
        setRegEx(0x32,0,0x1,0); //dim en
        setRegEx(0xe2,0x40,0xff,0); //workaround, must call
        setRegEx(0xeb,0x40,0xff,0); //workaround, must call
        setRegEx(0x16,0x1,0x1,0); //power source on
    }
    else
    {
        setRegEx(0x32,0x0,0x1,4);  //torch mode=0, flash mode
        setRegEx(0x35,flashCur[g_duty],0xf,0); //current
        setRegEx(0x37,0x3,0x3,6); //time out, 800ms
        setRegEx(0xe2,0x40,0xff,0); //workaround, must call
        setRegEx(0xeb,0x40,0xff,0); //workaround, must call
        setRegEx(0x16,0x1,0x1,0); //power source on
    }
	return 0;
    }

static int FL_Enable(void)
{
    PK_DBG("FL_Enable+");
    setRegEx(0x31,0x1,0x1,0); //flash en
    PK_DBG("FL_Enable-");
    return 0;
}

static int FL_Disable(void)
{
	PK_DBG("FL_Disable line=%d\n",__LINE__);
    setRegEx(0x31,0x0,0x1,0);
    setRegEx(0x16,0x0,0x1,0);

    setRegEx(0xF7,0x1,0x1,0);
    setRegEx(0xF3,0x1,0x1,0);
    mdelay(10);
    setRegEx(0xF7,0x0,0x1,0);
    setRegEx(0xF3,0x0,0x1,0);

    return 0;
}

static int FL_dim_duty(kal_uint32 duty)
{
	PK_DBG("FL_dim_duty line=%d\n",__LINE__);
	g_duty = duty;
    return 0;
}


static int g_lowPowerLevel=LOW_BATTERY_LEVEL_0;
static void lowPowerCB(LOW_BATTERY_LEVEL lev)
{
	g_lowPowerLevel=lev;
}

static int FL_Init(void)
{
    PK_DBG(" FL_Init line=%d\n",__LINE__);
    INIT_WORK(&workTimeOut, work_timeOutFunc);
  //  register_low_battery_callback(&lowPowerCB, LOW_BATTERY_PRIO_FLASHLIGHT);
    register_low_battery_notify(&lowPowerCB, LOW_BATTERY_PRIO_FLASHLIGHT);



    return 0;
}
static int FL_Uninit(void)
{
	FL_Disable();
    return 0;
}

static int FL_hasLowPowerDetect()
{

	return 1;
}

static int detLowPowerStart()
{

	g_lowPowerLevel=LOW_BATTERY_LEVEL_0;

}


static int detLowPowerEnd()
{
	if(g_lowPowerLevel!=LOW_BATTERY_LEVEL_0)
		return 1;
	else
		return 0;
}

/*****************************************************************************
User interface
*****************************************************************************/
static struct hrtimer g_timeOutTimer;
static struct hrtimer g_WDResetTimer;
int g_b1stInit=1;
int g_bOpen = 0;
static void work_timeOutFunc(struct work_struct *data)
{
    FL_Disable();
    PK_DBG("ledTimeOut_callback\n");
}

static void work_WDResetFunc(struct work_struct *data)
{
	ktime_t ktime;
	mt6333_set_rg_chrwdt_wr(1); // write 1 to kick chr wdt
	if(g_bOpen==1)
	{		
		ktime = ktime_set( 0, 1000*1000000 );//1s
		hrtimer_start( &g_WDResetTimer, ktime, HRTIMER_MODE_REL );
	}
}

static enum hrtimer_restart ledTimeOutCallback(struct hrtimer *timer)
{
    schedule_work(&workTimeOut);
    return HRTIMER_NORESTART;
}
static enum hrtimer_restart ledWDResetCallback(struct hrtimer *timer)
{
    schedule_work(&workWDReset);
    return HRTIMER_NORESTART;
}


static void timerInit(void)
{
	ktime_t ktime;


	//mt6333_set_rg_chrwdt_en(0);
    mt6333_set_rg_chrwdt_wr(1); // write 1 to kick chr wdt
    //mt6333_set_rg_chrwdt_td(0); //4 sec
    //mt6333_set_rg_chrwdt_en(1);

    //mt6333_set_rg_chrwdt_en(0);

	if(g_b1stInit==1)
{
		g_b1stInit=0;
	    INIT_WORK(&workWDReset, work_WDResetFunc);
	    hrtimer_init( &g_WDResetTimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL );
	    g_WDResetTimer.function=ledWDResetCallback;

  INIT_WORK(&workTimeOut, work_timeOutFunc);
	g_timeOutTimeMs=1000; //1s
	hrtimer_init( &g_timeOutTimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL );
	g_timeOutTimer.function=ledTimeOutCallback;

}
	ktime = ktime_set( 0, 1000*1000000 );//1s
	hrtimer_start( &g_WDResetTimer, ktime, HRTIMER_MODE_REL );

}


static int constant_flashlight_ioctl(MUINT32 cmd, MUINT32 arg)
{
	int temp;
	int i4RetValue = 0;
	int ior_shift;
	int iow_shift;
	int iowr_shift;
	kal_uint8 valTemp;
	ior_shift = cmd - (_IOR(FLASHLIGHT_MAGIC,0, int));
	iow_shift = cmd - (_IOW(FLASHLIGHT_MAGIC,0, int));
	iowr_shift = cmd - (_IOWR(FLASHLIGHT_MAGIC,0, int));
	PK_DBG("constant_flashlight_ioctl() line=%d ior_shift=%d, iow_shift=%d iowr_shift=%d arg=%d\n",__LINE__, ior_shift, iow_shift, iowr_shift, arg);
    switch(cmd)
    {

		case FLASH_IOC_SET_TIME_OUT_TIME_MS:
			PK_DBG("FLASH_IOC_SET_TIME_OUT_TIME_MS: %d\n",arg);
			g_timeOutTimeMs=arg;
		break;


    	case FLASH_IOC_SET_DUTY :
    		PK_DBG("FLASHLIGHT_DUTY: %d\n",arg);
    		FL_dim_duty(arg);
    		break;


    	case FLASH_IOC_SET_STEP:
    		PK_DBG("FLASH_IOC_SET_STEP: %d\n",arg);

    		break;

    	case FLASH_IOC_SET_ONOFF :
    		PK_DBG("FLASHLIGHT_ONOFF: %d\n",arg);
    		if(arg==1)
    		{
				if(g_timeOutTimeMs!=0)
	            {
	            	ktime_t ktime;
					ktime = ktime_set( 0, g_timeOutTimeMs*1000000 );
					hrtimer_start( &g_timeOutTimer, ktime, HRTIMER_MODE_REL );
	            }
    			FL_Enable();
    		}
    		else
    		{
    			FL_Disable();
				hrtimer_cancel( &g_timeOutTimer );
    		}
    		break;

    	case FLASH_IOC_PRE_ON:
    		PK_DBG("FLASH_IOC_PRE_ON\n");
			FL_preOn();
    		break;
    	case FLASH_IOC_GET_PRE_ON_TIME_MS:
    		PK_DBG("FLASH_IOC_GET_PRE_ON_TIME_MS: %d\n",arg);
    		temp=13;
    		if(copy_to_user((void __user *) arg , (void*)&temp , 4))
            {
                PK_DBG(" ioctl copy to user failed\n");
                return -1;
            }
    		break;

        case FLASH_IOC_SET_REG_ADR:
            PK_DBG("FLASH_IOC_SET_REG_ADR: %d\n",arg);
            g_reg = arg;
            break;
        case FLASH_IOC_SET_REG_VAL:
            PK_DBG("FLASH_IOC_SET_REG_VAL: %d\n",arg);
            g_val = arg;
            break;
        case FLASH_IOC_SET_REG:
            PK_DBG("FLASH_IOC_SET_REG: %d %d\n",g_reg, g_val);
            mt6333_config_interface(g_reg, g_val, 0xff, 0);
            break;

        case FLASH_IOC_GET_REG:
            PK_DBG("FLASH_IOC_GET_REG: %d\n",arg);
            mt6333_read_interface(arg, &valTemp, 0xff, 0);
            i4RetValue = valTemp;
            PK_DBG("FLASH_IOC_GET_REG: v=%d\n",valTemp);
            break;

        case FLASH_IOC_HAS_LOW_POWER_DETECT:
    		PK_DBG("FLASH_IOC_HAS_LOW_POWER_DETECT");
    		temp=FL_hasLowPowerDetect();
    		if(copy_to_user((void __user *) arg , (void*)&temp , 4))
            {
                PK_DBG(" ioctl copy to user failed\n");
                return -1;
            }
    		break;
    	case FLASH_IOC_LOW_POWER_DETECT_START:
    		detLowPowerStart();
            break;
    	case FLASH_IOC_LOW_POWER_DETECT_END:
    		i4RetValue = detLowPowerEnd();
    		break;

        default :
    		PK_DBG(" No such command \n");
    		i4RetValue = -EPERM;
    		break;
    }
    return i4RetValue;
}




static int constant_flashlight_open(void *pArg)
{
    int i4RetValue = 0;
    PK_DBG("constant_flashlight_open line=%d\n", __LINE__);

	if (0 == strobe_Res)
	{
	    FL_Init();
		timerInit();
	}
	PK_DBG("constant_flashlight_open line=%d\n", __LINE__);
	spin_lock_irq(&g_strobeSMPLock);
	g_bOpen=1;

    if(strobe_Res)
    {
        PK_ERR(" busy!\n");
        i4RetValue = -EBUSY;
    }
    else
    {
        strobe_Res += 1;
    }


    spin_unlock_irq(&g_strobeSMPLock);
    PK_DBG("constant_flashlight_open line=%d\n", __LINE__);

    return i4RetValue;

}


static int constant_flashlight_release(void *pArg)
{
    PK_DBG(" constant_flashlight_release\n");

    if (strobe_Res)
    {
        spin_lock_irq(&g_strobeSMPLock);
        g_bOpen=0;
        strobe_Res = 0;


        spin_unlock_irq(&g_strobeSMPLock);

    	FL_Uninit();
    }
    PK_DBG(" Done\n");
    return 0;
}


static FLASHLIGHT_FUNCTION_STRUCT	constantFlashlightFunc=
{
	constant_flashlight_open,
	constant_flashlight_release,
	constant_flashlight_ioctl
};


MUINT32 constantFlashlightInit(PFLASHLIGHT_FUNCTION_STRUCT *pfFunc)
{
    if (pfFunc != NULL)
    {
        *pfFunc = &constantFlashlightFunc;
    }
    return 0;
}



/* LED flash control for high current capture mode*/
ssize_t strobe_VDIrq(void)
{

    return 0;
}

EXPORT_SYMBOL(strobe_VDIrq);


#endif //#ifndef MTK_MT6333_SUPPORT



