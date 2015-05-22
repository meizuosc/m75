#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include <linux/xlog.h>

#include "kd_camera_hw.h"

#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_camera_feature.h"

/******************************************************************************
 * Debug configuration
******************************************************************************/
#define PFX "[kd_camera_hw]"
#define PK_DBG_NONE(fmt, arg...)    do {} while (0)
#define PK_DBG_FUNC(fmt, arg...)    xlog_printk(ANDROID_LOG_INFO, PFX , fmt, ##arg)

#define DEBUG_CAMERA_HW_K
#ifdef DEBUG_CAMERA_HW_K
#define PK_DBG PK_DBG_FUNC
#define PK_ERR(fmt, arg...)         xlog_printk(ANDROID_LOG_ERR, PFX , fmt, ##arg)
#define PK_XLOG_INFO(fmt, args...) \
                do {    \
                   xlog_printk(ANDROID_LOG_INFO, PFX , fmt, ##arg); \
                } while(0)
#else
#define PK_DBG(a,...)
#define PK_ERR(a,...)
#define PK_XLOG_INFO(fmt, args...)
#endif

#ifndef BOOL
typedef unsigned char BOOL;
#endif

extern void ISP_MCLK1_EN(BOOL En);
extern void ISP_MCLK2_EN(BOOL En);
extern void ISP_MCLK3_EN(BOOL En);

/*
 * Camera VCM device power on func.
 */
int camera_af_poweron(char *mode_name, BOOL on)
{
	if (on) {
		/*
		 * VCAM_IO  enable since it is used by E2PROM, AF need to read info from E2PROM
		 */
		if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_D2, VOL_1800,mode_name))
		{
			PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
			return -EIO;
		}

		/*
		 * AF_VCC
		 * Just needed by main camera.
		 */
		if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_A2, VOL_2800, mode_name))
		{
			PK_DBG("[CAMERA SENSOR] Fail to enable af power\n");
			return -EIO;
		}
	} else {
		if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_D2,mode_name))
		{
			PK_DBG("[CAMERA SENSOR] Fail to disable digital power\n");
			return -EIO;
		}

		if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_A2, mode_name))
		{
			PK_DBG("[CAMERA SENSOR] Fail to disable af power\n");
			return -EIO;
		}
	}

	return 0;
}

int kdCISModulePowerOn(CAMERA_DUAL_CAMERA_SENSOR_ENUM SensorIdx, char *currSensorName, BOOL On, char* mode_name)
{

u32 pinSetIdx = 0;//default main sensor
u32 tmp_idx = 0;

#define IDX_PS_CMRST 0
#define IDX_PS_MODE 1
#define IDX_PS_ON   2
#define IDX_PS_OFF  3


u32 pinSet[2][5] = {
                        //for main sensor
                     {  CAMERA_CMRST_PIN,
                        CAMERA_CMRST_PIN_M_GPIO,   /* mode */
                        GPIO_OUT_ONE,              /* ON state */
                        GPIO_OUT_ZERO,             /* OFF state */
                        GPIO_CAMERA_INVALID,
                     },
                     //for sub sensor
                     {  CAMERA_CMRST1_PIN,
                        CAMERA_CMRST1_PIN_M_GPIO,
                        GPIO_OUT_ONE,
                        GPIO_OUT_ZERO,
                        GPIO_CAMERA_INVALID,
                     },
                   };

    if (DUAL_CAMERA_MAIN_SENSOR == SensorIdx){
        pinSetIdx = 0;
    }
    else if (DUAL_CAMERA_SUB_SENSOR == SensorIdx) {
        pinSetIdx = 1;
    }

    //power ON
    if (On) {

	/* the same MCLK1 port is used by main and sub camera sensor */
	ISP_MCLK1_EN(1);

	/* set XSHUTDOWN/camera_1v2_en pin to low before power-on */
        if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
		if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
		if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
		if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
	}

	usleep_range(3000, 3500);

        //VCAM_A
        if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_A, VOL_2800,mode_name))
        {
            PK_DBG("[CAMERA SENSOR] Fail to enable analog power\n");
            goto _kdCISModulePowerOn_exit_;
        }
 
        //DVDD
        if (currSensorName && (0 == strcmp(currSensorName,"imx135mipiraw"))) 
        {
            if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_D, VOL_1000,mode_name))
            {
                 PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
                 goto _kdCISModulePowerOn_exit_;
            }
        }
        else if (currSensorName && ((0 == strcmp(currSensorName,"imx220mipiraw"))
		|| (0 == strcmp(currSensorName, "imx220sharpmipiraw")))) 
        {
            if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_D, VOL_1000,mode_name))
            {
                 PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
                 goto _kdCISModulePowerOn_exit_;
            }
        }
        else if (currSensorName && ((0 == strcmp(currSensorName,"imx208mipiraw"))
		|| (0 == strcmp(currSensorName, "imx208sunnymipiraw"))))
        {
            if(TRUE != hwPowerOn(SUB_CAMERA_POWER_VCAM_D, VOL_1200,mode_name))
            {
                 PK_DBG("[CAMERA SENSOR] Fail to enable sub camera digital power\n");
                 goto _kdCISModulePowerOn_exit_;
            }
        }
        else if(currSensorName && (0 == strcmp(SENSOR_DRVNAME_OV5648_MIPI_RAW, currSensorName)))
        {
            if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_D, VOL_1500,mode_name))
            {
                 PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
                 goto _kdCISModulePowerOn_exit_;
            }
        }            
        else {
            if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_D, VOL_1800,mode_name))
            {
                 PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
                 goto _kdCISModulePowerOn_exit_;
            }
        
        }
 
        //VCAM_IO
        if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_D2, VOL_1800,mode_name))
        {
            PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
            goto _kdCISModulePowerOn_exit_;
        }

	/*
	 * AF_VCC
	 * Just needed by main camera.
	 */
        if(pinSetIdx == 0 ) {
		if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_A2, VOL_2800,mode_name))
		{
			PK_DBG("[CAMERA SENSOR] Fail to enable analog power\n");
			goto _kdCISModulePowerOn_exit_;
		}
	}

	usleep_range(5000, 5500);

        //enable active sensor
        if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
            if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_ON])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
            usleep_range(8000, 8500);
        }

	/* Disable unused camera sensor */
	tmp_idx = (pinSetIdx + 1) % 2;
        if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
		if(mt_set_gpio_mode(pinSet[tmp_idx][IDX_PS_CMRST],pinSet[tmp_idx][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
		if(mt_set_gpio_dir(pinSet[tmp_idx][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
		if(mt_set_gpio_out(pinSet[tmp_idx][IDX_PS_CMRST],pinSet[tmp_idx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");}
	}
    }
    else {//power OFF

	ISP_MCLK1_EN(0);

        if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST]) {
            if(mt_set_gpio_mode(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_MODE])){PK_DBG("[CAMERA SENSOR] set gpio mode failed!! \n");}
            if(mt_set_gpio_dir(pinSet[pinSetIdx][IDX_PS_CMRST],GPIO_DIR_OUT)){PK_DBG("[CAMERA SENSOR] set gpio dir failed!! \n");}
            if(mt_set_gpio_out(pinSet[pinSetIdx][IDX_PS_CMRST],pinSet[pinSetIdx][IDX_PS_CMRST+IDX_PS_OFF])){PK_DBG("[CAMERA SENSOR] set gpio failed!! \n");} //low == reset sensor
		usleep_range(3000, 3500);
        }
#if 1
        if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_D2,mode_name))
        {
            PK_DBG("[CAMERA SENSOR] Fail to enable digital power\n");
            goto _kdCISModulePowerOn_exit_;
        }
        if (currSensorName && ((0 == strcmp(currSensorName,"imx208mipiraw"))
	   || (0 == strcmp(currSensorName, "imx208sunnymipiraw")))) {
		if(TRUE != hwPowerDown(SUB_CAMERA_POWER_VCAM_D, mode_name)) {
			PK_DBG("[CAMERA SENSOR] Fail to OFF sub camera digital power\n");
			goto _kdCISModulePowerOn_exit_;
		}
	} else {
		if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_D, mode_name)) {
			PK_DBG("[CAMERA SENSOR] Fail to OFF digital power\n");
			goto _kdCISModulePowerOn_exit_;
		}
	}
        if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_A,mode_name)) {
            PK_DBG("[CAMERA SENSOR] Fail to OFF analog power\n");
            goto _kdCISModulePowerOn_exit_;
        }

	/* AF_VCC just be power on by main camera */
        if(pinSetIdx == 0 ) {
		if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_A2,mode_name))
		{
			PK_DBG("[CAMERA SENSOR] Fail to enable analog power\n");
			goto _kdCISModulePowerOn_exit_;
		}
	}
   #endif   
    }//

    return 0;

_kdCISModulePowerOn_exit_:
    return -EIO;
    
}

EXPORT_SYMBOL(kdCISModulePowerOn);
EXPORT_SYMBOL(camera_af_poweron);
