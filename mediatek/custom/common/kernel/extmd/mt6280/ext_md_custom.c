//#include "extmd_mt6252d.h"
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <mach/mt_gpio.h>
#include <cust_eint.h>

#if defined (MTK_KERNEL_POWER_OFF_CHARGING)
#include <linux/xlog.h>
#include <mach/mt_boot.h>
#endif


extern unsigned int mt65xx_eint_set_sens(unsigned int, unsigned int);
extern void mt65xx_eint_set_polarity(unsigned char, unsigned char);
extern void mt65xx_eint_set_hw_debounce(unsigned char, unsigned int);
extern void mt65xx_eint_registration(unsigned char, unsigned char, unsigned char, void(*func)(void),
					unsigned char);
extern void mt65xx_eint_unmask(unsigned int);
extern void mt65xx_eint_mask(unsigned int);
extern void mt_eint_unmask(unsigned int eint_num);
extern void mt_eint_mask(unsigned int eint_num);

int cm_do_md_power_on(void)
{
#if defined (MTK_KERNEL_POWER_OFF_CHARGING)
    if(g_boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT || g_boot_mode == LOW_POWER_OFF_CHARGING_BOOT)
    {
        xlog_printk(ANDROID_LOG_INFO, "MD 6280", "KPOC, cm_do_md_power_on skip!\r\n");
        return 0;
    }
#endif
    mt_set_gpio_out(GPIO_6280_RST, GPIO_OUT_ONE);  
    msleep(10);
    mt_set_gpio_out(GPIO_6280_KCOL0, GPIO_OUT_ONE);
	return 0;
}
int cm_do_md_power_off(void)
{
    mt_set_gpio_out(GPIO_6280_KCOL0, GPIO_OUT_ZERO);
    mt_set_gpio_out(GPIO_6280_RST, GPIO_OUT_ZERO);  
    msleep(10);
	return 0;
}
int cm_do_md_switch_r8_to_pc(void)
{
    mt_set_gpio_out(GPIO_6280_USB_SW1, GPIO_OUT_ONE);
    mt_set_gpio_out(GPIO_6280_USB_SW2, GPIO_OUT_ONE);
	return 0;
}

int cm_do_md_switch_r8_to_ap(void)
{
    mt_set_gpio_out(GPIO_6280_USB_SW1, GPIO_OUT_ZERO);
    mt_set_gpio_out(GPIO_6280_USB_SW2, GPIO_OUT_ZERO);
	return 0;
}
int cm_do_md_download_r8(void)
{
    mt_set_gpio_out(GPIO_6280_KCOL0, GPIO_OUT_ZERO);
    msleep(1000);
    mt_set_gpio_out(GPIO_6280_RST, GPIO_OUT_ONE);  
	return 0;
}

void cm_hold_rst_signal(void)
{
//	mt_set_gpio_dir(GPIO_DT_MD_RST_PIN, 1);
//	mt_set_gpio_out(GPIO_DT_MD_RST_PIN, 0);
}

void cm_relese_rst_signal(void)
{
//	mt_set_gpio_out(GPIO_DT_MD_RST_PIN, 1);
//	mt_set_gpio_dir(GPIO_DT_MD_RST_PIN, 0);
}

int cm_do_md_go(void)
{
	//int high_signal_check_num=0;
	int ret = 0;
#if 0
	unsigned int retry = 100;
	#if 0
	cm_relese_rst_signal();
	msleep(10);

	mt_set_gpio_dir(GPIO_DT_MD_PWR_KEY_PIN, 1);
	mt_set_gpio_out(GPIO_DT_MD_PWR_KEY_PIN, EXT_MD_PWR_KEY_ACTIVE_LVL);

	msleep(5000); 
	mt_set_gpio_dir(GPIO_DT_MD_PWR_KEY_PIN, 0);
	//mt_set_gpio_out(GPIO_OTG_DRVVBUS_PIN, 1); // VBus
	#endif
	// Release download key to let md can enter normal boot
	//mt_set_gpio_dir(102, 1);
	//mt_set_gpio_out(102, 1);
	mt_set_gpio_dir(GPIO_DT_MD_DL_PIN, 1);
	mt_set_gpio_out(GPIO_DT_MD_DL_PIN, 1);
	// Press power key
	mt_set_gpio_dir(GPIO_DT_MD_PWR_KEY_PIN, 1);
	mt_set_gpio_out(GPIO_DT_MD_PWR_KEY_PIN, 1);
	msleep(10);
	cm_relese_rst_signal();

	// Check WDT pin to high
	while(retry>0){
		retry--;
		if(mt_get_gpio_in(GPIO_DT_MD_WDT_PIN)==0)
			msleep(10);
		else
			return 100-retry;
	}
	//msleep(5000); 
	ret = -1;
#endif
	return ret;
}

void cm_do_md_rst_and_hold(void)
{
}

void cm_hold_wakeup_md_signal(void)
{
//	mt_set_gpio_out(GPIO_DT_AP_WK_MD_PIN, 0);
}

void cm_release_wakeup_md_signal(void)
{
//	mt_set_gpio_out(GPIO_DT_AP_WK_MD_PIN, 1);
}

void cm_gpio_setup(void)
{
#if 1     
    mt_set_gpio_mode(GPIO_6280_USB_SW1, GPIO_MODE_00);
    mt_set_gpio_mode(GPIO_6280_USB_SW2, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_6280_USB_SW1, GPIO_DIR_OUT);
    mt_set_gpio_dir(GPIO_6280_USB_SW2, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_6280_USB_SW1, GPIO_OUT_ZERO);
    mt_set_gpio_out(GPIO_6280_USB_SW2, GPIO_OUT_ZERO);

    /* Press MT6280 DL key to enter download mode
     * GPIO_6280_KCOL0(GPIO_KCOL0)=0 
     */        
    mt_set_gpio_mode(GPIO_6280_KCOL0, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_6280_KCOL0, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_6280_KCOL0, GPIO_OUT_ZERO);

    /* Power on MT6280:
     * Pull high GPIO_6280_RST, 0->1
     */
    mt_set_gpio_mode(GPIO_6280_RST, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_6280_RST, GPIO_DIR_OUT);
    //mt_set_gpio_out(GPIO_6280_RST, GPIO_OUT_ZERO);
                
    //msleep(200);

    //mt_set_gpio_mode(GPIO_6280_RST, GPIO_MODE_00);
    //mt_set_gpio_dir(GPIO_6280_RST, GPIO_DIR_OUT);
    //mt_set_gpio_out(GPIO_6280_RST, GPIO_OUT_ONE);  

    //msleep(10);
    /* Press MT6280 DL key to enter download mode
     * GPIO_6280_KCOL0(GPIO_KCOL0)=0 
     */        
    mt_set_gpio_mode(GPIO_6280_KCOL0, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_6280_KCOL0, GPIO_DIR_OUT);
    //mt_set_gpio_out(GPIO_6280_KCOL0, GPIO_OUT_ONE);
    cm_do_md_power_off();
    cm_do_md_power_on();
    
    //printk("[%s] 2modem download .. while(1)...\n", "mt6280");
#endif
#if 0
	// MD wake up AP pin
	mt_set_gpio_pull_enable(GPIO_DT_MD_WK_AP_PIN, !0);
	mt_set_gpio_pull_select(GPIO_DT_MD_WK_AP_PIN, 1);
	mt_set_gpio_dir(GPIO_DT_MD_WK_AP_PIN, 0);
	mt_set_gpio_mode(GPIO_DT_MD_WK_AP_PIN, GPIO_DT_MD_WK_AP_PIN_M_EINT); // EINT3

	// AP wake up MD pin
	mt_set_gpio_mode(GPIO_DT_AP_WK_MD_PIN, GPIO_DT_AP_WK_MD_PIN_M_GPIO); // GPIO Mode
	mt_set_gpio_dir(GPIO_DT_AP_WK_MD_PIN, 1);
	mt_set_gpio_out(GPIO_DT_AP_WK_MD_PIN, 0);

	// Rest MD pin
	mt_set_gpio_mode(GPIO_DT_MD_RST_PIN, GPIO_DT_MD_RST_PIN_M_GPIO); //GPIO202 is reset pin
	mt_set_gpio_pull_enable(GPIO_DT_MD_RST_PIN, 0);
	mt_set_gpio_pull_select(GPIO_DT_MD_RST_PIN, 1);
	mt_set_gpio_dir(GPIO_DT_MD_RST_PIN, 1);
	mt_set_gpio_out(GPIO_DT_MD_RST_PIN, 0);// Default @ reset state

	// MD power key pin
	mt_set_gpio_mode(GPIO_DT_MD_PWR_KEY_PIN, GPIO_DT_MD_PWR_KEY_PIN_M_GPIO); //GPIO 200 is power key
	mt_set_gpio_pull_enable(GPIO_DT_MD_PWR_KEY_PIN, 0);
	mt_set_gpio_dir(GPIO_DT_MD_PWR_KEY_PIN, 0);// Using input floating
	//mt_set_gpio_out(GPIO_DT_MD_PWR_KEY_PIN, 1);// Default @ reset state

	// MD WDT irq pin
	mt_set_gpio_pull_enable(GPIO_DT_MD_WDT_PIN, !0);
	mt_set_gpio_pull_select(GPIO_DT_MD_WDT_PIN, 1);
	mt_set_gpio_dir(GPIO_DT_MD_WDT_PIN, 0);
	mt_set_gpio_mode(GPIO_DT_MD_WDT_PIN, GPIO_DT_MD_WDT_PIN_M_EINT); // EINT9

	// MD Download pin
	//.......
#endif	
}

void cm_ext_md_rst(void)
{
	cm_hold_rst_signal();
//	mt_set_gpio_out(GPIO_OTG_DRVVBUS_PIN, 0); // VBus EMD_VBUS_TMP_PIN
}

void cm_enable_ext_md_wdt_irq(void)
{
//	mt65xx_eint_unmask(CUST_EINT_DT_EXT_MD_WDT_NUM);
}

void cm_disable_ext_md_wdt_irq(void)
{
//	mt65xx_eint_mask(CUST_EINT_DT_EXT_MD_WDT_NUM);
}

void cm_enable_ext_md_wakeup_irq(void)
{
    mt_eint_unmask(CUST_EINT_MT6280_USB_WAKEUP_NUM);
//	mt65xx_eint_unmask(CUST_EINT_DT_EXT_MD_WK_UP_NUM);
}

void cm_disable_ext_md_wakeup_irq(void)
{
    mt_eint_mask(CUST_EINT_MT6280_USB_WAKEUP_NUM);	
//	mt65xx_eint_mask(CUST_EINT_DT_EXT_MD_WK_UP_NUM);
}


