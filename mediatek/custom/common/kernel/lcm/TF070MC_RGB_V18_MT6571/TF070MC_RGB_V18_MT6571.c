#ifdef BUILD_LK
#include <platform/gpio_const.h>
#include <platform/mt_gpio.h>
#include <platform/upmu_common.h>
#else
#include <mach/gpio_const.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>
#include <linux/string.h>
#endif
#include "lcm_drv.h"

// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------
#define FRAME_WIDTH  (800)
#define FRAME_HEIGHT (480)

#define GPIO_LCM_PWR_EN      GPIO67
#define GPIO_LCM_RST      GPIO58

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------
static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)    (lcm_util.set_reset_pin((v)))
#define SET_CHIP_SELECT(v)    (lcm_util.set_chip_select((v)))

#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))

// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------
static __inline void send_ctrl_cmd(unsigned int cmd)
{
   lcm_util.send_cmd(cmd);
}

static __inline void send_data_cmd(unsigned int data)
{
   lcm_util.send_data(data);
}

static void lcm_set_gpio_output(unsigned int GPIO, unsigned int output)
{
   mt_set_gpio_mode(GPIO, GPIO_MODE_00);
   mt_set_gpio_dir(GPIO, GPIO_DIR_OUT);
   mt_set_gpio_out(GPIO, (output>0)? GPIO_OUT_ONE: GPIO_OUT_ZERO);
}

static __inline void set_lcm_register(unsigned int regIndex,
                                      unsigned int regData)
{
   send_ctrl_cmd(regIndex);
   send_data_cmd(regData);
}

static void init_lcm_registers(void)
{
}

// ---------------------------------------------------------------------------
//  LCM Driver Implementations
// ---------------------------------------------------------------------------
static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
   memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}

static void lcm_get_params(LCM_PARAMS *params)
{
   memset(params, 0, sizeof(LCM_PARAMS));
   
   params->type   = LCM_TYPE_DPI;
   params->width  = FRAME_WIDTH;
   params->height = FRAME_HEIGHT;
   
   
   /* RGB interface configurations */

   params->dpi.PLL_CLOCK      = 266;  // 33.3 (base on LCM Spec)* 8
   
   params->dpi.clk_pol           = LCM_POLARITY_FALLING;
   params->dpi.de_pol            = LCM_POLARITY_RISING;
   params->dpi.vsync_pol         = LCM_POLARITY_FALLING;
   params->dpi.hsync_pol         = LCM_POLARITY_FALLING;
   
   params->dpi.hsync_pulse_width = 48;
   params->dpi.hsync_back_porch  = 40;
   params->dpi.hsync_front_porch = 40;
   params->dpi.vsync_pulse_width = 1;
   params->dpi.vsync_back_porch  = 31;
   params->dpi.vsync_front_porch = 13;
   
   params->dpi.format            = LCM_DPI_FORMAT_RGB888;
   params->dpi.rgb_order         = LCM_COLOR_ORDER_RGB;
   
   
   params->dpi.intermediat_buffer_num = 2;
   params->dpi.io_driving_current = LCM_DRIVING_CURRENT_6575_4MA;
   params->dpi.i2x_en = 0;
   params->dpi.i2x_edge = 0;
   params->dpi.ssc_disable= 1;
}

static void lcm_init(void)
{
#ifdef BUILD_LK

	printf("[LK/LCM] lcm_init() enter \n");

	lcm_set_gpio_output(GPIO_LCM_RST, 0);
	MDELAY(20);

	lcm_set_gpio_output(GPIO_LCM_RST, 1);
	MDELAY(50);

	//VDD power on ->VGP3_PMU 1.8V
	upmu_set_rg_vgp3_vosel(3);
	upmu_set_rg_vgp3_en(0x1);
    MDELAY(20);
	//AVDD power on
	lcm_set_gpio_output(GPIO_LCM_PWR_EN, 1);
	MDELAY(20);
	

#elif (defined BUILD_UBOOT)
#else

	printk("[Kernel/LCM] lcm_init() enter \n");	

#endif	 
}


static void lcm_suspend(void)
{
#ifdef BUILD_LK

	printf("[LK/LCM] lcm_suspend() enter\n");

	lcm_set_gpio_output(GPIO_LCM_RST, 0);
	MDELAY(300);
    //AVDD power off
	lcm_set_gpio_output(GPIO_LCM_PWR_EN, 0);
	MDELAY(2);

	//VDD power off ->VGP3_PMU 1.8V
	upmu_set_rg_vgp3_vosel(0);
	upmu_set_rg_vgp3_en(0);	
	MDELAY(20);
	
#elif (defined BUILD_UBOOT)
#else

	printk("[LCM] lcm_suspend() enter\n");

    lcm_set_gpio_output(GPIO_LCM_RST, 0);
	MDELAY(300);
    //AVDD power off
	lcm_set_gpio_output(GPIO_LCM_PWR_EN, 0);
	MDELAY(2);

	//VDD power off ->VGP3_PMU 1.8V
	hwPowerOn(MT6323_POWER_LDO_VGP3 , VOL_1800 ,"LCM");
	MDELAY(20);
	
#endif	 
}


static void lcm_resume(void)
{
#ifdef BUILD_LK

	printf("[LK/LCM] lcm_resume() enter \n");


	lcm_set_gpio_output(GPIO_LCM_RST, 0);
	MDELAY(20);

	lcm_set_gpio_output(GPIO_LCM_RST, 1);
	MDELAY(20);

	//VGP3_PMU 1.8V
	upmu_set_rg_vgp3_vosel(3);
	upmu_set_rg_vgp3_en(0x1);
	MDELAY(20);

	lcm_set_gpio_output(GPIO_LCM_PWR_EN, 1);
	MDELAY(20);
	
	
#elif (defined BUILD_UBOOT)
#else

	printk("[Kernel/LCM] lcm_resume() enter \n");


	lcm_set_gpio_output(GPIO_LCM_RST, 0);
	MDELAY(20);

	lcm_set_gpio_output(GPIO_LCM_RST, 1);
	MDELAY(20);

	hwPowerOn(MT6323_POWER_LDO_VGP3 , VOL_1800 ,"LCM");
	MDELAY(20);

	lcm_set_gpio_output(GPIO_LCM_PWR_EN, 1);
	MDELAY(20);		
	
#endif	 
}

// ---------------------------------------------------------------------------
//  Get LCM Driver Hooks
// ---------------------------------------------------------------------------
LCM_DRIVER tf070mc_rgb_v18_mt6571_lcm_drv = 
{
   .name = "TF070MC_RGB_V18_MT6571",
   .set_util_funcs = lcm_set_util_funcs,
   .get_params     = lcm_get_params,
   .init           = lcm_init,
   .suspend        = lcm_suspend,
   .resume         = lcm_resume,
};

