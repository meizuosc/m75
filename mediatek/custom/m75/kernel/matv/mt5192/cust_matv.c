//For zte73v1
#include <mach/mt6573_pll.h>
#include <linux/delay.h>
#include "cust_matv.h"
#include "cust_matv_comm.h"

int cust_matv_power_on(void)
{  

    //set GPIO94 for power
    mt_set_gpio_mode(GPIO_CAMERA_LDO_EN_PIN,GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_CAMERA_LDO_EN_PIN, GPIO_DIR_OUT);
    mt_set_gpio_pull_enable(GPIO_CAMERA_LDO_EN_PIN,GPIO_PULL_DISABLE);
    mt_set_gpio_out(GPIO_CAMERA_LDO_EN_PIN, GPIO_OUT_ONE);
    
    
    //disable sub sensor        
    mt_set_gpio_mode(GPIO_CAMERA_CMPDN1_PIN, GPIO_MODE_00);
    mt_set_gpio_mode(GPIO_CAMERA_CMRST1_PIN ,GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_CAMERA_CMPDN1_PIN, GPIO_DIR_OUT);
    mt_set_gpio_dir(GPIO_CAMERA_CMRST1_PIN, GPIO_DIR_OUT);
    //workaround for mAtv green image
    //caused by sub sensor didn't really enter PDN mode.
    //There is power leakage to Vio in official PDN sequence
    mt_set_gpio_out(GPIO_CAMERA_CMPDN1_PIN, GPIO_OUT_ONE);

    if(TRUE != hwPowerOn(MT65XX_POWER_LDO_VCAMA,VOL_2800,"MT5192"))
    {
        MATV_LOGE("[MATV] Fail to enable analog gain VCAMA\n");
        return -EIO;
    }  
    if(TRUE != hwPowerOn(MT65XX_POWER_LDO_VCAMA2,VOL_2800,"MT5192"))
    {
        MATV_LOGE("[MATV] Fail to enable analog gain VCAMA2\n");
        return -EIO;
    } 
    if(TRUE != hwPowerOn(MT65XX_POWER_LDO_VCAMD,VOL_1500,"MT5192"))
    {
        MATV_LOGE("[MATV] Fail to enable analog gain VCAMD\n");
        return -EIO;
    } 
#ifdef CAMERA_IO_DRV_1800	
    if(TRUE != hwPowerOn(MT65XX_POWER_LDO_VCAMD2,VOL_1800,"MT5192"))
    {
        MATV_LOGE("[MATV] Fail to enable analog gain VCAMD2\n");
        return -EIO;
    }
#else
    if(TRUE != hwPowerOn(MT65XX_POWER_LDO_VCAMD2,VOL_2800,"MT5192"))
    {
        MATV_LOGE("[MATV] Fail to enable analog gain VCAMD2\n");
        return -EIO;
    }
#endif
    mdelay(5);  //wait for power stable 
    //disable main sensor
    mt_set_gpio_mode(GPIO_CAMERA_CMRST_PIN,GPIO_MODE_00);
    mt_set_gpio_mode(GPIO_CAMERA_CMPDN_PIN,GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_CAMERA_CMRST_PIN,GPIO_DIR_OUT);
    mt_set_gpio_dir(GPIO_CAMERA_CMPDN_PIN,GPIO_DIR_OUT);

    mt_set_gpio_out(GPIO_CAMERA_CMRST_PIN,GPIO_OUT_ZERO);
    mt_set_gpio_out(GPIO_CAMERA_CMPDN_PIN,GPIO_OUT_ZERO);

    //disable sub sensor
    //workaround for mAtv green image
    //caused by sub sensor didn't really enter PDN mode.
    //There is power leakage to Vio in official PDN sequence
    mdelay(5);
    mt_set_gpio_out(GPIO_CAMERA_CMPDN1_PIN, GPIO_OUT_ZERO);
    mdelay(5);
    mt_set_gpio_out(GPIO_CAMERA_CMPDN1_PIN, GPIO_OUT_ONE);
    mdelay(10); 
    printk("cust_matv_power_on \n"); 
    return 0;
}


int cust_matv_power_off(void)
{  
    if(TRUE != hwPowerDown(MT65XX_POWER_LDO_VCAMA,"MT5192"))
    {
        MATV_LOGE("[MATV] Fail to disable analog gain VCAMA\n");
        return -EIO;
    }  
    if(TRUE != hwPowerDown(MT65XX_POWER_LDO_VCAMA2,"MT5192"))
    {
        MATV_LOGE("[MATV] Fail to disable analog gain VCAMA2\n");
        return -EIO;
    } 
    if(TRUE != hwPowerDown(MT65XX_POWER_LDO_VCAMD,"MT5192"))
    {
        MATV_LOGE("[MATV] Fail to disable analog gain VCAMD\n");
        return -EIO;
    } 
    if(TRUE != hwPowerDown(MT65XX_POWER_LDO_VCAMD2,"MT5192"))
    {
        MATV_LOGE("[MATV] Fail to disable analog gain VCAMD2\n");
        return -EIO;
    }    
    mt_set_gpio_out(GPIO_CAMERA_LDO_EN_PIN, GPIO_OUT_ZERO);
    printk("cust_matv_power_off \n");     
    return 0;
}

int cust_matv_gpio_on(void)
{
	MATV_LOGE("[MATV] mt5192 cust_matv_gpio_on Start\n");

}

int cust_matv_gpio_off(void)
{
	MATV_LOGE("[MATV] mt5192 cust_matv_gpio_off Start\n");

}


