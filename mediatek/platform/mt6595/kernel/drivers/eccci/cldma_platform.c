#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <mach/mt_spm_sleep.h>
#include <mach/mt_gpio.h>
#include <mach/mt_clkbuf_ctl.h>
#include <mach/upmu_sw.h>
#include "ccci_core.h"
#include "ccci_platform.h"
#include "modem_cldma.h"
#include "cldma_platform.h"
#include "cldma_reg.h"

extern int md_power_on(int);
extern int md_power_off(int, unsigned);
extern void mt_irq_set_sens(unsigned int irq, unsigned int sens);
extern void mt_irq_set_polarity(unsigned int irq, unsigned int polarity);

#define TAG "mcd"

int md_cd_power_on(struct ccci_modem *md)
{
	int ret = 0;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;

	//config RFICx as BSI
	mutex_lock(&clk_buf_ctrl_lock); // fixme,clkbuf, ->down(&clk_buf_ctrl_lock_2);
	CCCI_INF_MSG(md->index, TAG, "clock buffer, BSI mode\n"); 
	mt_set_gpio_mode(GPIO_RFIC0_BSI_CK,  GPIO_MODE_01); 
    mt_set_gpio_mode(GPIO_RFIC0_BSI_D0,  GPIO_MODE_01);
    mt_set_gpio_mode(GPIO_RFIC0_BSI_D1,  GPIO_MODE_01);
    mt_set_gpio_mode(GPIO_RFIC0_BSI_D2,  GPIO_MODE_01);
    mt_set_gpio_mode(GPIO_RFIC0_BSI_CS,  GPIO_MODE_01);
	// power on MD_INFRA and MODEM_TOP
	ret = md_power_on(md->index);
	mutex_unlock(&clk_buf_ctrl_lock); // fixme,clkbuf, ->delete
	if(ret)
		return ret;
	// disable MD WDT
	cldma_write32(md_ctrl->md_rgu_base, WDT_MD_MODE, WDT_MD_MODE_KEY);
	return 0;
}

int md_cd_bootup_cleanup(struct ccci_modem *md, int success)
{
	int ret = 0;
#if 0 // fixme,clkbuf, ->delete
	// config RFICx as GPIO
	if(success) {
		CCCI_INF_MSG(md->index, TAG, "clock buffer, GPIO mode\n"); 
	    mt_set_gpio_mode(GPIO_RFIC0_BSI_CK,  GPIO_MODE_GPIO); 
	    mt_set_gpio_mode(GPIO_RFIC0_BSI_D0,  GPIO_MODE_GPIO);
	    mt_set_gpio_mode(GPIO_RFIC0_BSI_D1,  GPIO_MODE_GPIO);
	    mt_set_gpio_mode(GPIO_RFIC0_BSI_D2,  GPIO_MODE_GPIO);
	    mt_set_gpio_mode(GPIO_RFIC0_BSI_CS,  GPIO_MODE_GPIO);
	} else {
		CCCI_INF_MSG(md->index, TAG, "clock buffer, unlock when bootup fail\n"); 
	}
	up(&clk_buf_ctrl_lock_2); //mutex_unlock(&clk_buf_ctrl_lock);
#endif
	return ret;
}

int md_cd_let_md_go(struct ccci_modem *md)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	
	if(ccci_get_md_debug_mode(md)&DBG_FLAG_JTAG)
		return -1;
	CCCI_INF_MSG(md->index, TAG, "set MD boot slave\n"); 
	// set the start address to let modem to run
	cldma_write32(md_ctrl->md_boot_slave_Key, 0, 0x3567C766); // make boot vector programmable
	cldma_write32(md_ctrl->md_boot_slave_Vector, 0, 0x00000001); // after remap, MD ROM address is 0 from MD's view, MT6595 uses Thumb code
	cldma_write32(md_ctrl->md_boot_slave_En, 0, 0xA3B66175); // make boot vector take effect
	return 0;
}

int md_cd_power_off(struct ccci_modem *md, unsigned int timeout)
{
	int ret = 0;

	mutex_lock(&clk_buf_ctrl_lock); // fixme,clkbuf, ->down(&clk_buf_ctrl_lock_2);
	// power off MD_INFRA and MODEM_TOP
	ret = md_power_off(md->index, timeout);
	// config RFICx as GPIO
	CCCI_INF_MSG(md->index, TAG, "clock buffer, GPIO mode\n"); 
    mt_set_gpio_mode(GPIO_RFIC0_BSI_CK,  GPIO_MODE_GPIO); 
    mt_set_gpio_mode(GPIO_RFIC0_BSI_D0,  GPIO_MODE_GPIO);
    mt_set_gpio_mode(GPIO_RFIC0_BSI_D1,  GPIO_MODE_GPIO);
    mt_set_gpio_mode(GPIO_RFIC0_BSI_D2,  GPIO_MODE_GPIO);
    mt_set_gpio_mode(GPIO_RFIC0_BSI_CS,  GPIO_MODE_GPIO);
	mutex_unlock(&clk_buf_ctrl_lock); // fixme,clkbuf, ->up(&clk_buf_ctrl_lock_2);
	return ret;
}

void md_cd_lock_cldma_clock_src(int locked)
{
	spm_ap_mdsrc_req(locked);
}

int md_cd_low_power_notify(struct ccci_modem *md, LOW_POEWR_NOTIFY_TYPE type, int level)
{
	unsigned int reserve = 0xFFFFFFFF;
	int ret = 0;

	CCCI_INF_MSG(md->index, TAG, "low power notification type=%d, level=%d\n", type, level);
	/*
	 * byte3 byte2 byte1 byte0
	 *    0   4G   3G   2G
	 */
	switch(type) {
	case LOW_BATTERY:
		if(level == LOW_BATTERY_LEVEL_0) {
			reserve = 0; // 0
		} else if(level == LOW_BATTERY_LEVEL_1 || level == LOW_BATTERY_LEVEL_2) {
			reserve = (1<<6); // 64
		}
		ret = ccci_send_msg_to_md(md, CCCI_SYSTEM_TX, MD_LOW_BATTERY_LEVEL, reserve, 1);
		if(ret)
			CCCI_ERR_MSG(md->index, TAG, "send low battery notification fail, ret=%d\n", ret);
		break;
	case BATTERY_PERCENT:
		if(level == BATTERY_PERCENT_LEVEL_0) {
			reserve = 0; // 0
		} else if(level == BATTERY_PERCENT_LEVEL_1) {
			reserve = (1<<6); // 64
		}
		ret = ccci_send_msg_to_md(md, CCCI_SYSTEM_TX, MD_LOW_BATTERY_LEVEL, reserve, 1);
		if(ret)
			CCCI_ERR_MSG(md->index, TAG, "send battery percent notification fail, ret=%d\n", ret);
		break;
	default:
		break;
	};
	return ret;
}

int ccci_modem_remove(struct platform_device *dev)
{
	return 0;
}

void ccci_modem_shutdown(struct platform_device *dev)
{
}

int ccci_modem_suspend(struct platform_device *dev, pm_message_t state)
{
	struct ccci_modem *md = (struct ccci_modem *)dev->dev.platform_data;

	CCCI_INF_MSG(md->index, TAG, "AP_BUSY(%x)=%x\n", AP_CCIF0_BASE+APCCIF_BUSY, cldma_read32(AP_CCIF0_BASE, APCCIF_BUSY));
	CCCI_INF_MSG(md->index, TAG, "MD_BUSY(%x)=%x\n", _MD_CCIF0_BASE+APCCIF_BUSY, cldma_read32(_MD_CCIF0_BASE, APCCIF_BUSY));

	return 0;
}

int ccci_modem_resume(struct platform_device *dev)
{
	cldma_write32(AP_CCIF0_BASE, APCCIF_CON, 0x01); // arbitration
	return 0;
}

int ccci_modem_pm_suspend(struct device *device)
{
    struct platform_device *pdev = to_platform_device(device);
    BUG_ON(pdev == NULL);

    return ccci_modem_suspend(pdev, PMSG_SUSPEND);
}

int ccci_modem_pm_resume(struct device *device)
{
    struct platform_device *pdev = to_platform_device(device);
    BUG_ON(pdev == NULL);

    return ccci_modem_resume(pdev);
}

int ccci_modem_pm_restore_noirq(struct device *device)
{
	struct ccci_modem *md = (struct ccci_modem *)device->platform_data;
	// IPO-H
    // restore IRQ
#ifdef FEATURE_PM_IPO_H
    mt_irq_set_sens(CLDMA_AP_IRQ, MT_LEVEL_SENSITIVE);
    mt_irq_set_polarity(CLDMA_AP_IRQ, MT_POLARITY_HIGH);
    mt_irq_set_sens(MD_WDT_IRQ, MT_EDGE_SENSITIVE);
    mt_irq_set_polarity(MD_WDT_IRQ, MT_POLARITY_LOW);
#endif
	// set flag for next md_start
	md->config.setting |= MD_SETTING_RELOAD;
	md->config.setting |= MD_SETTING_FIRST_BOOT;
    return 0;
}

