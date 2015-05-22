///#include <mach/mt6575_pll.h>
#include <mach/mt6575_typedefs.h> 
#include <mach/mt6575_reg_base.h>
#include <mach/mt6575_gpio.h>
#include <cust_gpio_usage.h>


#define MATV_I2C_DEVNAME "MT6573_I2C_MATV"

#define CAMERA_IO_DRV_1800
//6573_EVB
#define MATV_I2C_CHANNEL     (1)        //I2C Channel 1
//zte73v1
//#define MATV_I2C_CHANNEL     (1)        //I2C Channel 1
extern int cust_matv_power_on(void);
extern int cust_matv_power_off(void);
//customize matv i2s gpio and close fm i2s mode.
extern int cust_matv_gpio_on(void);
extern int cust_matv_gpio_off(void);


#if 1 
//FIXME, should remove when DCT done 
//
#ifndef GPIO_CAMERA_LDO_EN_PIN 
#define GPIO_CAMERA_LDO_EN_PIN GPIO94
#endif 

//
#ifndef GPIO_CAMERA_CMRST_PIN 
#define GPIO_CAMERA_CMRST_PIN GPIO78
#endif 

//
#ifndef GPIO_CAMERA_CMRST_PIN_M_GPIO
#define GPIO_CAMERA_CMRST_PIN_M_GPIO GPIO_MODE_00
#endif 

//
#ifndef GPIO_CAMERA_CMPDN_PIN 
#define GPIO_CAMERA_CMPDN_PIN GPIO79
#endif 

//
#ifndef GPIO_CAMERA_LDO_EN_PIN_M_GPIO
#define GPIO_CAMERA_LDO_EN_PIN_M_GPIO GPIO_MODE_00
#endif 

//
#ifndef GPIO_CAMERA_CMPDN_PIN_M_GPIO
#define GPIO_CAMERA_CMPDN_PIN_M_GPIO  GPIO_MODE_00 
#endif 

//
#ifndef GPIO_CAMERA_CMRST1_PIN
#define GPIO_CAMERA_CMRST1_PIN GPIO97
#endif

//
#ifndef GPIO_CAMERA_CMRST1_PIN_M_GPIO
#define GPIO_CAMERA_CMRST1_PIN_M_GPIO GPIO_MODE_00
#endif

//
#ifndef GPIO_CAMERA_CMPDN1_PIN
#define GPIO_CAMERA_CMPDN1_PIN GPIO96
#endif
//

#ifndef GPIO_CAMERA_CMPDN1_PIN_M_GPIO
#define GPIO_CAMERA_CMPDN1_PIN_M_GPIO GPIO_MODE_00
#endif



#define CAMERA_POWER_VCAM_A  MT65XX_POWER_LDO_VCAMA
#define CAMERA_POWER_VCAM_D  MT65XX_POWER_LDO_VCAMD
#define CAMERA_POWER_VCAM_A2 MT65XX_POWER_LDO_VCAM_AF
#define CAMERA_POWER_VCAM_D2 MT65XX_POWER_LDO_VCAM_IO


#endif 



#define GPIO_MATV_I2S_DATA   GPIO58
#define GPIO_MATV_I2S_DATA_M   GPIO_MODE_04


#ifndef GPIO_MATV_PWR_ENABLE
#define GPIO_MATV_PWR_ENABLE GPIO69
#endif
#ifndef GPIO_MATV_N_RST
#define GPIO_MATV_N_RST      GPIO70
#endif
#if 1
#define MATV_LOGD printk
#else
#define MATV_LOGD(...)
#endif
#if 1
#define MATV_LOGE printk
#else
#define MATV_LOGE(...)
#endif


