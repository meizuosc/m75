#include <cust_cmmb.h>
#include <mach/mt_gpio.h>
#include <cust_gpio_usage.h>
#include <mach/eint.h>
#include <cust_eint.h>
#include <linux/module.h>
#include <linux/spi/spi.h>

#include <mach/mt_pm_ldo.h>


#define inno_msg(fmt, arg...)	printk(KERN_ERR "[cmmb-drv]%s: " fmt "\n", __func__, ##arg)


void cust_cmmb_power_on(void)
{

	//hwPowerOn(MT65XX_POWER_LDO_VGP2,VOL_1800,"CMMB");
	//hwPowerOn(MT65XX_POWER_LDO_VGP3,VOL_1200,"CMMB");
	//mt_set_gpio_out(GPIO_ANT_SW_PIN, GPIO_OUT_ONE); 

	mt_set_gpio_mode(GPIO_CMMB_LDO_EN_PIN, GPIO_CMMB_LDO_EN_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_CMMB_LDO_EN_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CMMB_LDO_EN_PIN, GPIO_OUT_ONE);  
  //mt_set_gpio_pull_enable(GPIO_CMMB_LDO_EN_PIN, GPIO_PULL_ENABLE);      // no need to pull, beause BB output power is enough
	//mt_set_gpio_pull_select(GPIO_CMMB_LDO_EN_PIN, GPIO_PULL_UP);
	inno_msg("CMMB GPIO LDO PIN mode:num:%d, %d,out:%d, dir:%d,pullen:%d,pullup%d",GPIO_CMMB_LDO_EN_PIN,mt_get_gpio_mode(GPIO_CMMB_LDO_EN_PIN),mt_get_gpio_out(GPIO_CMMB_LDO_EN_PIN),mt_get_gpio_dir(GPIO_CMMB_LDO_EN_PIN),mt_get_gpio_pull_enable(GPIO_CMMB_LDO_EN_PIN),mt_get_gpio_pull_select(GPIO_CMMB_LDO_EN_PIN));    

}
EXPORT_SYMBOL(cust_cmmb_power_on);

void cust_cmmb_power_off(void)
{
	//mt_set_gpio_out(GPIO_ANT_SW_PIN, GPIO_OUT_ZERO); 


	//hwPowerDown(MT65XX_POWER_LDO_VGP2,"CMMB");
	//hwPowerDown(MT65XX_POWER_LDO_VGP3,"CMMB");

	mt_set_gpio_mode(GPIO_CMMB_LDO_EN_PIN, GPIO_CMMB_LDO_EN_PIN_M_GPIO);
	mt_set_gpio_out(GPIO_CMMB_LDO_EN_PIN, GPIO_OUT_ZERO);  
	mt_set_gpio_dir(GPIO_CMMB_LDO_EN_PIN, GPIO_DIR_IN);
	inno_msg("CMMB GPIO LDO PIN mode:num:%d, %d,out:%d, dir:%d,pullen:%d,pullup%d",GPIO_CMMB_LDO_EN_PIN,mt_get_gpio_mode(GPIO_CMMB_LDO_EN_PIN),mt_get_gpio_out(GPIO_CMMB_LDO_EN_PIN),mt_get_gpio_dir(GPIO_CMMB_LDO_EN_PIN),mt_get_gpio_pull_enable(GPIO_CMMB_LDO_EN_PIN),mt_get_gpio_pull_select(GPIO_CMMB_LDO_EN_PIN));    

	mt_set_gpio_mode(GPIO_CMMB_RST_PIN, GPIO_CMMB_RST_PIN_M_GPIO);
	mt_set_gpio_out(GPIO_CMMB_RST_PIN, GPIO_OUT_ZERO); 			 
	mt_set_gpio_dir(GPIO_CMMB_RST_PIN, GPIO_DIR_IN);
	inno_msg("CMMB GPIO RST PIN mode:num:%d, %d,out:%d, dir:%d,pullen:%d,pullup%d",GPIO_CMMB_RST_PIN,mt_get_gpio_mode(GPIO_CMMB_RST_PIN),mt_get_gpio_out(GPIO_CMMB_RST_PIN),mt_get_gpio_dir(GPIO_CMMB_RST_PIN),mt_get_gpio_pull_enable(GPIO_CMMB_RST_PIN),mt_get_gpio_pull_select(GPIO_CMMB_RST_PIN));    	 
}
EXPORT_SYMBOL(cust_cmmb_power_off);

static struct spi_board_info spi_board_devs __initdata = {
	.modalias="cmmb-spi",
	.bus_num = 0,
	.chip_select=0,
	.mode = SPI_MODE_3,
};
static int __init init_cust_cmmb_spi(void)
{
      spi_register_board_info(&spi_board_devs, 1);
      return 0;
}
static void __exit exit_cust_cmmb_spi(void)
{
       return;
}

module_init(init_cust_cmmb_spi);
module_exit(exit_cust_cmmb_spi);