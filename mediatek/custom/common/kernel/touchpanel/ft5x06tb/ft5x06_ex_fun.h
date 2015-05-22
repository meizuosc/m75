#ifndef __LINUX_FT5X06_EX_FUN_H__
#define __LINUX_FT5X06_EX_FUN_H__

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <mach/irqs.h>

#include <linux/syscalls.h>
#include <asm/unistd.h>
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/string.h>

#define IC_FT5X06	0	/*x=2,3,4*/
#define IC_FT5606	1	/*ft5506*/
#define IC_FT5316	2	/*5x16*/
#define IC_FT6208	3


#define FT_UPGRADE_AA	0xAA
#define FT_UPGRADE_55 	0x55


/*upgrade config of FT5606*/
#define FT5606_UPGRADE_AA_DELAY 		50
#define FT5606_UPGRADE_55_DELAY 		10
#define FT5606_UPGRADE_ID_1			0x79
#define FT5606_UPGRADE_ID_2			0x06
#define FT5606_UPGRADE_READID_DELAY 	100
#define FT5606_UPGRADE_EARSE_DELAY	2000


/*upgrade config of FT5316*/
#define FT5316_UPGRADE_AA_DELAY 		50
#define FT5316_UPGRADE_55_DELAY 		30
#define FT5316_UPGRADE_ID_1			0x79
#define FT5316_UPGRADE_ID_2			0x07
#define FT5316_UPGRADE_READID_DELAY 	1
#define FT5316_UPGRADE_EARSE_DELAY	1500


/*upgrade config of FT5x06(x=2,3,4)*/
#define FT5X06_UPGRADE_AA_DELAY 		50
#define FT5X06_UPGRADE_55_DELAY 		30
#define FT5X06_UPGRADE_ID_1			0x79
#define FT5X06_UPGRADE_ID_2			0x03
#define FT5X06_UPGRADE_READID_DELAY 	1
#define FT5X06_UPGRADE_EARSE_DELAY	2000

/*upgrade config of FT6208*/
#define FT6208_UPGRADE_AA_DELAY 		60
#define FT6208_UPGRADE_55_DELAY 		10
#define FT6208_UPGRADE_ID_1			0x79
#define FT6208_UPGRADE_ID_2			0x05
#define FT6208_UPGRADE_READID_DELAY 	10
#define FT6208_UPGRADE_EARSE_DELAY	2000

#define DEVICE_IC_TYPE	IC_FT5606

#define FTS_PACKET_LENGTH        128
#define FTS_SETTING_BUF_LEN        128

#define FTS_UPGRADE_LOOP	3

#define FTS_TX_MAX				40
#define FTS_RX_MAX				30
#define FTS_DEVICE_MODE_REG	0x00
#define FTS_TXNUM_REG			0x03
#define FTS_RXNUM_REG			0x04
#define FTS_RAW_READ_REG		0x01
#define FTS_RAW_BEGIN_REG		0x10
#define FTS_VOLTAGE_REG		0x05
#define FTS_ROW_OFFSET0_REG	0xD3
#define FTS_ROW_CAP0_REG		0xA0


#define FTS_FACTORYMODE_VALUE		0x40
#define FTS_WORKMODE_VALUE		0x00

#define    AUTO_CLB
#define FTS_DBG
#ifdef FTS_DBG
#define DBG(fmt, args...) printk("[FTS]" fmt, ## args)
#else
#define DBG(fmt, args...) do{}while(0)
#endif


/*create sysfs for debug*/
int ft5x0x_create_sysfs(struct i2c_client * client);

void ft5x0x_release_sysfs(struct i2c_client * client);

int ft5x0x_create_apk_debug_channel(struct i2c_client *client);
void ft5x0x_release_apk_debug_channel(void);
int fts_ctpm_auto_upgrade(struct i2c_client *client);
/*
*ft5x0x_write_reg- write register
*@client: handle of i2c
*@regaddr: register address
*@regvalue: register value
*
*/
int ft5x0x_write_reg(struct i2c_client * client,u8 regaddr, u8 regvalue);

int ft5x0x_read_reg(struct i2c_client * client,u8 regaddr, u8 *regvalue);

#endif
