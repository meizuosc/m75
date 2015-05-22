#include <mach/mt6573_pll.h>
#include <mach/mt6573_typedefs.h> 
#include <mach/mt6573_reg_base.h>
#include <mach/mt6573_gpio.h>
#include <cust_gpio_usage.h>


#define MATV_I2C_DEVNAME "MT6573_I2C_MATV"


//zte73v1
#define MATV_I2C_CHANNEL     (1)        //I2C Channel 1
extern int cust_matv_power_on(void);
extern int cust_matv_power_off(void);
//customize matv i2s gpio and close fm i2s mode.
extern int cust_matv_gpio_on(void);
extern int cust_matv_gpio_off(void);


#define GPIO_MATV_I2S_DATA   GPIO2
#define GPIO_MATV_I2S_DATA_M  GPIO_MODE_01

#ifndef GPIO_MATV_PWR_ENABLE
#define GPIO_MATV_PWR_ENABLE GPIO190
#endif
#ifndef GPIO_MATV_N_RST
#define GPIO_MATV_N_RST      GPIO189
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


