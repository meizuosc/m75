#ifndef __CLDMA_PLATFORM_H__
#define __CLDMA_PLATFORM_H__

#include <mach/mt_irq.h>

// this is the platform header file for CLDMA MODEM, not just CLDMA!

//MD peripheral register: MD bank8; AP bank2
#define CLDMA_AP_BASE 0x200F0000
#define CLDMA_AP_LENGTH 0x3000
#define CLDMA_MD_BASE 0x200E0000
#define CLDMA_MD_LENGTH 0x3000
#define MD_BOOT_VECTOR 0x20190000
#define MD_BOOT_VECTOR_KEY 0x2019379C
#define MD_BOOT_VECTOR_EN 0x20195488
#define MD_RGU_BASE 0x20050000
#define MD_GLOBAL_CON0 0x20000450
#define MD_GLOBAL_CON0_CLDMA_BIT 12
//#define MD_PEER_WAKEUP 0x20030B00
#define MD_BUS_STATUS_BASE 0x20000000
#define MD_BUS_STATUS_LENGTH 0x468
#define MD_PC_MONITOR_BASE 0x201F0004
#define MD_PC_MONITOR_LENGTH 0x110
#define MD_TOPSM_STATUS_BASE 0x20030800
#define MD_TOPSM_STATUS_LENGTH 0x228
#define MD_OST_STATUS_BASE 0x20040010
#define MD_OST_STATUS_LENGTH 0x60
#define _MD_CCIF0_BASE (AP_CCIF0_BASE+0x1000)
#define CCIF_SRAM_SIZE 512

#define CCIF0_AP_IRQ CCIF0_AP_IRQ_BIT_ID //164
#define MD_WDT_IRQ MD_WDT_IRQ_BIT_ID //257
#define CLDMA_AP_IRQ CLDMA_AP_IRQ_BIT_ID //258
#define AP2MD_BUS_TIMEOUT_IRQ AP2MD_BUS_TIMEOUT_IRQ_BIT_ID //259

#define EMI_IDLE_STATE_REG 0xF0001224
#define EMI_MD_IDLE_MASK 0x38 //bit3:MDMCU idle, bit4:MDHW idle, bit5:MD->APB path idle

/* Modem WDT */
#define WDT_MD_MODE		(0x00)
#define WDT_MD_LENGTH	(0x04)
#define WDT_MD_RESTART	(0x08)
#define WDT_MD_STA		(0x0C)
#define WDT_MD_SWRST	(0x1C)
#define WDT_MD_MODE_KEY	(0x0000220E)

/* CCIF */
#define APCCIF_CON    (0x00)
#define APCCIF_BUSY   (0x04)
#define APCCIF_START  (0x08)
#define APCCIF_TCHNUM (0x0C)
#define APCCIF_RCHNUM (0x10)
#define APCCIF_ACK    (0x14)
#define APCCIF_CHDATA (0x100)
#define APCCIF_SRAM_SIZE 512
// channel usage
#define EXCEPTION_NONE (0)
// AP to MD
#define H2D_EXCEPTION_ACK (1)
#define H2D_EXCEPTION_CLEARQ_ACK (2)
#define H2D_FORCE_MD_ASSERT (3)
// MD to AP
#define D2H_EXCEPTION_INIT (1)
#define D2H_EXCEPTION_INIT_DONE (2)
#define D2H_EXCEPTION_CLEARQ_DONE (3)
#define D2H_EXCEPTION_ALLQ_RESET (4)
// peer wakeup
#define AP_MD_PEER_WAKEUP (5)
#define AP_MD_SEQ_ERROR (6)

int ccci_modem_remove(struct platform_device *dev);
void ccci_modem_shutdown(struct platform_device *dev);
int ccci_modem_suspend(struct platform_device *dev, pm_message_t state);
int ccci_modem_resume(struct platform_device *dev);
int ccci_modem_pm_suspend(struct device *device);
int ccci_modem_pm_resume(struct device *device);
int ccci_modem_pm_restore_noirq(struct device *device);
int md_cd_power_on(struct ccci_modem *md);
int md_cd_power_off(struct ccci_modem *md, unsigned int timeout);
int md_cd_let_md_go(struct ccci_modem *md);
void md_cd_lock_cldma_clock_src(int locked);
int md_cd_bootup_cleanup(struct ccci_modem *md, int success);
int md_cd_low_power_notify(struct ccci_modem *md, LOW_POEWR_NOTIFY_TYPE type, int level);

#endif //__CLDMA_PLATFORM_H__
