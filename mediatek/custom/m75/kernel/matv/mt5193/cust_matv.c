//For mt6573_evb
///#include <mach/mt6575_pll.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/workqueue.h>

#include <linux/hrtimer.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>

#include <linux/jiffies.h>
#include <linux/timer.h>

#include <mach/mt6575_typedefs.h>
#include <mach/mt6575_pm_ldo.h>

#include "cust_matv.h"
#include "cust_matv_comm.h"


int cust_matv_power_on(void)
{  
	MATV_LOGE("[MATV] cust_matv_power_on Start\n");

    ///cust_matv_gpio_on();
    u32 pinSetIdx = 0;//default main sensor
    u32 pinSet[2][4] = {
    			//for main sensor 
    			{GPIO_CAMERA_CMRST_PIN,
    			 GPIO_CAMERA_CMRST_PIN_M_GPIO,
    			 GPIO_CAMERA_CMPDN_PIN,
    			 GPIO_CAMERA_CMPDN_PIN_M_GPIO},
    			//for sub sensor 
    			{GPIO_CAMERA_CMRST1_PIN,
    			 GPIO_CAMERA_CMRST1_PIN_M_GPIO,
    			 GPIO_CAMERA_CMPDN1_PIN,
    			 GPIO_CAMERA_CMPDN1_PIN_M_GPIO}
    		   };

    if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_D2, VOL_2800,"MT5192"))
    {
    MATV_LOGE("[CAMERA SENSOR] Fail to enable digital power\n");
    //return -EIO;
    return 0;
    }                    

    if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_A, VOL_2800,"MT5192"))
    {
    MATV_LOGE("[CAMERA SENSOR] Fail to enable analog power\n");
    //return -EIO;
    return 0;
    }


    if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_D, VOL_1500,"MT5192"))
    {
    MATV_LOGE("[CAMERA SENSOR] Fail to enable digital power\n");
    //return -EIO;
    return 0;
    }

    if(TRUE != hwPowerOn(CAMERA_POWER_VCAM_A2, VOL_2800,"MT5192"))
    {
    MATV_LOGE("[CAMERA SENSOR] Fail to enable analog power\n");
    //return -EIO;
    return 0;
    }        


    {
    mt_set_gpio_mode(pinSet[pinSetIdx][0],pinSet[pinSetIdx][1]);
    mt_set_gpio_dir(pinSet[pinSetIdx][0],GPIO_DIR_OUT);
    mt_set_gpio_out(pinSet[pinSetIdx][0],GPIO_OUT_ZERO);
    ///mdelay(10);
    mt_set_gpio_out(pinSet[pinSetIdx][0],GPIO_OUT_ONE);
    ///mdelay(1);

    //PDN pin
    mt_set_gpio_mode(pinSet[pinSetIdx][2],pinSet[pinSetIdx][3]);
    mt_set_gpio_dir(pinSet[pinSetIdx][2],GPIO_DIR_OUT);
    mt_set_gpio_out(pinSet[pinSetIdx][2],GPIO_OUT_ZERO);
    }

    return 0;
}


int cust_matv_power_off(void)
{  
	u32 pinSetIdx = 0;//default main sensor
	u32 pinSet[2][4] = {
						//for main sensor 
						{GPIO_CAMERA_CMRST_PIN,
						 GPIO_CAMERA_CMRST_PIN_M_GPIO,
						 GPIO_CAMERA_CMPDN_PIN,
						 GPIO_CAMERA_CMPDN_PIN_M_GPIO},
						//for sub sensor 
						{GPIO_CAMERA_CMRST1_PIN,
						 GPIO_CAMERA_CMRST1_PIN_M_GPIO,
						 GPIO_CAMERA_CMPDN1_PIN,
						 GPIO_CAMERA_CMPDN1_PIN_M_GPIO}
					   };

    MATV_LOGE("[MATV] cust_matv_power_off Start\n");
    ///cust_matv_gpio_off();

	mt_set_gpio_mode(pinSet[pinSetIdx][0],pinSet[pinSetIdx][1]);
	mt_set_gpio_mode(pinSet[pinSetIdx][2],pinSet[pinSetIdx][3]);
	mt_set_gpio_dir(pinSet[pinSetIdx][0],GPIO_DIR_OUT);
	mt_set_gpio_dir(pinSet[pinSetIdx][2],GPIO_DIR_OUT);
	mt_set_gpio_out(pinSet[pinSetIdx][0],GPIO_OUT_ZERO);//low == reset sensor
	mt_set_gpio_out(pinSet[pinSetIdx][2],GPIO_OUT_ONE); //high == power down lens module			

    if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_A,"MT5192")) {
        MATV_LOGE("[CAMERA SENSOR] Fail to OFF analog power\n");
        //return -EIO;
        return 0;
    }
    if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_A2,"MT5192"))
    {
        MATV_LOGE("[CAMERA SENSOR] Fail to enable analog power\n");
        //return -EIO;
        return 0;
    }       
    if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_D, "MT5192")) {
        MATV_LOGE("[CAMERA SENSOR] Fail to OFF digital power\n");
        //return -EIO;
        return 0;
    }
    if(TRUE != hwPowerDown(CAMERA_POWER_VCAM_D2,"MT5192"))
    {
        MATV_LOGE("[CAMERA SENSOR] Fail to enable digital power\n");
        //return -EIO;
        return 0;
    }                    
	
    return 0;
}

int cust_matv_gpio_on(void)
{
	MATV_LOGE("[MATV] mt5193 cust_matv_gpio_on Start\n");
    mt_set_gpio_mode(GPIO_I2S0_DAT_PIN, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_I2S0_DAT_PIN,GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_I2S0_DAT_PIN,GPIO_OUT_ZERO);
    mt_set_gpio_mode(GPIO_I2S0_WS_PIN, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_I2S0_WS_PIN,GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_I2S0_WS_PIN,GPIO_OUT_ZERO);
    mt_set_gpio_mode(GPIO_I2S0_CK_PIN, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_I2S0_CK_PIN,GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_I2S0_CK_PIN,GPIO_OUT_ZERO);

    mt_set_gpio_mode(GPIO_I2S1_CK_PIN, GPIO_I2S1_CK_PIN_M_I2S0_CK);
    mt_set_gpio_mode(GPIO_I2S1_WS_PIN, GPIO_I2S1_WS_PIN_M_I2S0_WS);
    mt_set_gpio_mode(GPIO_I2S1_DAT_PIN, GPIO_I2S1_DAT_PIN_M_I2S0_DAT);


}

int cust_matv_gpio_off(void)
{
	MATV_LOGE("[MATV] mt5193 cust_matv_gpio_off Start\n");
    
    mt_set_gpio_mode(GPIO_I2S1_CK_PIN, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_I2S1_CK_PIN,GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_I2S1_CK_PIN,GPIO_OUT_ZERO);
    mt_set_gpio_mode(GPIO_I2S1_WS_PIN, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_I2S1_WS_PIN,GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_I2S1_WS_PIN,GPIO_OUT_ZERO);
    mt_set_gpio_mode(GPIO_I2S1_DAT_PIN, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_I2S1_DAT_PIN,GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_I2S1_DAT_PIN,GPIO_OUT_ZERO);
}



