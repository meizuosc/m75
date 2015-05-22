/*
 * escore-uart.c  --  Audience eS705 UART interface
 *
 * Copyright 2013 Audience, Inc.
 *
 * Author: Matt Lupfer <mlupfer@cardinalpeak.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/completion.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/kthread.h>
#include <linux/esxxx.h>
#include <linux/serial_core.h>
#include <linux/tty.h>

#include "escore.h"
#include "escore-uart.h"
#include "escore-uart-common.h"
#include "escore-cdev.h"

static int escore_uart_boot_setup(struct escore_priv *escore);
static int escore_uart_boot_finish(struct escore_priv *escore);
static int escore_uart_probe(struct platform_device *dev);
static int escore_uart_remove(struct platform_device *dev);
struct escore_uart_device escore_uart;
#ifdef ESCORE_FW_LOAD_BUF_SZ
#undef ESCORE_FW_LOAD_BUF_SZ
#endif
#define ESCORE_FW_LOAD_BUF_SZ 1024

static u32 escore_default_uart_baud = 460800;

static u32 escore_uarths_baud[] = { 460800, 921600, 1000000, 1024000, 1152000,
	2000000, 2048000, 3000000, 3072000 };

int escore_uart_boot_setup(struct escore_priv *escore)
{
	u8 sbl_sync_cmd = ESCORE_SBL_SYNC_CMD;
	u8 sbl_boot_cmd = ESCORE_SBL_BOOT_CMD;
	u32 sbl_rate_req_cmd = ESCORE_SBL_SET_RATE_REQ_CMD << 16;
	char msg[4] = {0};
	int uart_tty_baud_rate = UART_TTY_BAUD_RATE_460_8_K;
	int rc;
	u8 i;
	u8 retry = 10;

	/* set speed to bootloader baud */
	escore_configure_tty(escore_uart.tty,
		escore_uart.baudrate_bootloader, UART_TTY_STOP_BITS);

	pr_debug("%s()\n", __func__);

	/* SBL SYNC BYTE 0x00 */
	pr_debug("%s(): write ESCORE_SBL_SYNC_CMD = 0x%02x\n", __func__,
		sbl_sync_cmd);
	memcpy(msg, (char *)&sbl_sync_cmd, 1);
	rc = escore_uart_write(escore, msg, 1);
	if (rc) {
		rc = -EIO;
		goto escore_bootup_failed;
	}
	pr_debug("%s(): firmware load sbl sync write rc=%d\n", __func__, rc);
	usleep_range(10000, 10500);
	memset(msg, 0, 4);
	rc = escore_uart_read(escore, msg, 1);
	if (rc) {
		pr_err("%s(): firmware load failed sync ack rc = %d\n",
			__func__, rc);
		goto escore_bootup_failed;
	}
	pr_debug("%s(): sbl sync ack = 0x%02x\n", __func__, msg[0]);
	if (msg[0] != ESCORE_SBL_SYNC_ACK) {
		pr_err("%s(): firmware load failed sync ack pattern", __func__);
		rc = -EIO;
		goto escore_bootup_failed;
	}

	/* UART Baud rate and Clock setting:-
	 *
	 * Default baud rate will be 460.8Khz and external clock
	 * will be to 6MHz unless high speed firmware download
	 * is enabled.
	 */
	if (escore->pdata->enable_hs_uart_intf) {
#if defined(CONFIG_SND_SOC_ES_UARTHS_BAUD)
		escore_default_uart_baud = CONFIG_SND_SOC_ES_UARTHS_BAUD;
#endif
		/* Set Baud rate and external clock */
		uart_tty_baud_rate = ES_UARTHS_BAUD_RATE_3000K;
		for (i = 0; i < ARRAY_SIZE(escore_uarths_baud); i++) {
			if (escore_uarths_baud[i] == escore_default_uart_baud)
				uart_tty_baud_rate = i;
		}

		sbl_rate_req_cmd |= uart_tty_baud_rate << 8;
		sbl_rate_req_cmd |= (escore->pdata->ext_clk_rate & 0xff);

		pr_debug("Setting uart baud rate %x\n", sbl_rate_req_cmd);
		sbl_rate_req_cmd = cpu_to_be32(sbl_rate_req_cmd);
		rc = escore_uart_write(escore, &sbl_rate_req_cmd,
				sizeof(sbl_rate_req_cmd));
		if (rc) {
			pr_err("%s(): Baud rate setting for UART failed\n",
					__func__);
			rc = -EIO;
			goto escore_bootup_failed;
		}

		/* Sometimes the correct response is received in 2nd read */
		do {
			msleep(20);
			memset(msg, 0, 4);
			rc = escore_uart_read(escore, msg, 4);
			if (rc) {
				pr_err("%s(): Set Rate Request read rc = %d\n",
						__func__, rc);
				goto escore_bootup_failed;
			}
			if (!memcmp(msg, &sbl_rate_req_cmd, sizeof(msg)))
				break;
		} while (retry-- > 0);

		/* Set Host UART speed */
		escore_configure_tty(escore_uart.tty,
				escore_uarths_baud[uart_tty_baud_rate],
				UART_TTY_STOP_BITS);
	}

	msleep(20);

	/* SBL BOOT BYTE 0x01 */
	memset(msg, 0, 4);
	pr_debug("%s(): write ESCORE_SBL_BOOT_CMD = 0x%02x\n", __func__,
		sbl_boot_cmd);
	memcpy(msg, (char *)&sbl_boot_cmd, 1);
	rc = escore_uart_write(escore, msg, 1);
	if (rc) {
		pr_err("%s(): firmware load failed sbl boot write\n", __func__);
		rc = -EIO;
		goto escore_bootup_failed;
	}
	msleep(20);
	memset(msg, 0, 4);
	rc = escore_uart_read(escore, msg, 1);
	if (rc) {
		pr_err("%s(): firmware load failed boot ack\n", __func__);
		goto escore_bootup_failed;
	}
	pr_debug("%s(): sbl boot ack = 0x%02x\n", __func__, msg[0]);

	if (msg[0] != ESCORE_SBL_BOOT_ACK) {
		pr_err("%s(): firmware load failed boot ack pattern", __func__);
		rc = -EIO;
		goto escore_bootup_failed;
	}
	rc = 0;

escore_bootup_failed:
	return rc;
}

int escore_uart_boot_finish(struct escore_priv *escore)
{
	char msg[4];
	int rc;
	u32 sync_cmd = (ES_SYNC_CMD << 16) | ES_SYNC_POLLING;
	u32 sync_ack;
	int sync_retry = ES_SYNC_MAX_RETRY;

	/*
	 * Give the chip some time to become ready after firmware
	 * download. (FW is still transferring)
	 */
	msleep(200);

	/* discard up to two extra bytes from escore during firmware load */
	memset(msg, 0, 4);
	rc = escore_uart_read(escore, msg, 1);
	if (rc) {
		pr_err("%s(): firmware read\n", __func__);
		rc = rc < 0 ? rc : -EIO;
		goto escore_boot_finish_failed;
	}

	pr_debug("%s(): read byte = 0x%02x\n", __func__, msg[0]);

	if (msg[0] != 0x02) {
		rc = escore_uart_read(escore, msg, 1);
		if (rc) {
			rc = rc < 0 ? rc : -EIO;
			goto escore_boot_finish_failed;
		}

		pr_debug("%s(): read byte = 0x%02x\n", __func__, msg[0]);
	}

	/* We get one more extra bytes after VS firmware download.
	 * Bug 19441.
	 */
	rc = escore_uart_read(escore, msg, 1);

	/* now switch to firmware baud to talk to chip */
	escore_configure_tty(escore_uart.tty,
		escore_uart.baudrate_fw, UART_TTY_STOP_BITS);

	/* sometimes earSmart chip sends success in second sync command */
	do {
		pr_debug("%s(): write ES_SYNC_CMD = 0x%08x\n",
			__func__, sync_cmd);
		rc = escore_cmd(escore, sync_cmd, &sync_ack);
		if (rc) {
			pr_err("%s(): firmware load failed sync write - %d\n",
				__func__, rc);
			continue;
		}
		pr_debug("%s(): sync_ack = 0x%08x\n", __func__, sync_ack);
		if (sync_ack != ES_SYNC_ACK) {
			pr_err("%s(): firmware load failed sync ack pattern",
				__func__);
			rc = -EIO;
		} else {
			pr_info("%s(): firmware load success\n", __func__);
			break;
		}
	} while (sync_retry--);

escore_boot_finish_failed:
	return rc;
}

static struct platform_device *escore_dummy_device;

static void escore_uart_setup_pri_intf(struct escore_priv *escore)
{
	escore->bus.ops.read = escore_uart_read;
	escore->bus.ops.write = escore_uart_write;
	escore->bus.ops.cmd = escore_uart_cmd;
	escore->streamdev = es_uart_streamdev;
}

static int escore_uart_setup_high_bw_intf(struct escore_priv *escore)
{
	int rc;

	escore->bus.ops.high_bw_write = escore_uart_write;
	escore->bus.ops.high_bw_read = escore_uart_read;
	escore->bus.ops.high_bw_cmd = escore_uart_cmd;
	escore->bus.ops.high_bw_open = escore_uart_open;
	escore->bus.ops.high_bw_close = escore_uart_close;
	escore->boot_ops.setup = escore_uart_boot_setup;
	escore->boot_ops.finish = escore_uart_boot_finish;
#if defined(CONFIG_SND_SOC_ES_1ST_STAGE_UART_BAUD)
	escore_uart.baudrate_bootloader =
		CONFIG_SND_SOC_ES_1ST_STAGE_UART_BAUD;
#else
	escore_uart.baudrate_bootloader = 28800;
#endif
#if defined(CONFIG_SND_SOC_ES_2ND_STAGE_UART_BAUD)
	escore_uart.baudrate_fw = CONFIG_SND_SOC_ES_2ND_STAGE_UART_BAUD;
#else
	escore_uart.baudrate_fw = 3000000;
#endif
	rc = escore->probe(escore->dev);
	if (rc)
		goto out;

	rc = escore->boot_ops.bootup(escore);
	if (rc)
		goto out;

	release_firmware(escore->standard);

out:
	return rc;
}

int escore_uart_add_dummy_dev(struct escore_priv *escore)
{
	int rc = 0;

	/* HACK: Trigger the deferred probe */
	escore_dummy_device = platform_device_alloc("escore-codec", -1);
	if (!escore_dummy_device) {
		pr_err("%s(): Dummy platform device allocation failed\n",
				__func__);
		rc = -ENOMEM;
		goto out;
	}

	if (platform_device_add(escore_dummy_device)) {
		pr_err("%s(): Error while adding UART device\n", __func__);
		platform_device_put(escore_dummy_device);
	}
out:
	return rc;
}
int escore_uart_probe_thread(void *ptr)
{
	int rc = 0;
	struct device *dev = (struct device *) ptr;
	struct escore_priv *escore = &escore_priv;
	int probe_intf = ES_INVAL_INTF;

	rc = escore_uart_open(escore);
	if (rc) {
		dev_err(dev, "%s(): es705_uart_open() failed %d\n",
			__func__, rc);
		return rc;
	}

	/* device node found, continue */
	dev_dbg(dev, "%s(): successfully opened tty\n",
		  __func__);

	if (escore->pri_intf == ES_UART_INTF) {
		escore->bus.setup_prim_intf = escore_uart_setup_pri_intf;
		probe_intf = ES_UART_INTF;
	}
	if (escore->high_bw_intf == ES_UART_INTF) {
		escore->bus.setup_high_bw_intf = escore_uart_setup_high_bw_intf;
		probe_intf = ES_UART_INTF;
	}

	rc = escore_probe(escore, dev, probe_intf);

	if (rc) {
		dev_err(dev, "%s(): UART common probe failed\n", __func__);
		goto bootup_error;
	}
	escore_uart_close(escore);

	return rc;

bootup_error:
	release_firmware(escore_priv.standard);
	/* close filp */
	filp_close(escore_uart.file, 0);
	snd_soc_unregister_codec(escore_priv.dev);
	pr_err("%s(): exit with error\n", __func__);
	return rc;
}

static int escore_uart_probe(struct platform_device *dev)
{
	int rc = 0;
	static struct task_struct *uart_probe_thread;

	dev_dbg(&dev->dev, "%s():\n", __func__);

	/* HACK: Ugly trick to handle deferred probe */
	if (uart_probe_thread) {
		pr_debug("%s(): Dummy device probe. Exiting\n", __func__);
		return rc;
	}
	uart_probe_thread = kthread_run(escore_uart_probe_thread,
					(void *) &dev->dev,
					"escore uart thread");
	if (IS_ERR_OR_NULL(uart_probe_thread)) {
		pr_err("%s(): can't create escore UART probe thread = %p\n",
			__func__, uart_probe_thread);
		rc = -ENOMEM;
	}

	return rc;
}

static int escore_uart_remove(struct platform_device *dev)
{
	int rc = 0;
	/*
	 * ML: GPIO pins are not connected
	 *
	 * struct esxxx_platform_data *pdata = escore_priv.pdata;
	 *
	 * gpio_free(pdata->reset_gpio);
	 * gpio_free(pdata->wakeup_gpio);
	 * gpio_free(pdata->gpioa_gpio);
	 */

	if (escore_uart.ld)
		tty_ldisc_deref(escore_uart.ld);

	if (escore_uart.file)
		rc = filp_close(escore_uart.file, 0);

	escore_uart.tty = NULL;
	escore_uart.file = NULL;

	snd_soc_unregister_codec(escore_priv.dev);
	platform_device_unregister(escore_dummy_device);

	return rc;
}

struct platform_driver escore_uart_driver = {
	.driver = {
#ifdef CONFIG_ARCH_MSM8974
		.name = "escore-codec.0",
#else
		.name = "escore-codec",
#endif
		.owner = THIS_MODULE,
		.pm = &escore_pm_ops,
	},
	.probe = escore_uart_probe,
	.remove = escore_uart_remove,
};


/* FIXME: Kludge for escore_bus_init abstraction */
int escore_uart_bus_init(struct escore_priv *escore)
{
	int rc;

	rc = platform_driver_register(&escore_uart_driver);
	if (rc)
		return rc;

	pr_debug("%s(): registered as UART", __func__);

	return rc;
}

MODULE_DESCRIPTION("ASoC ESCORE driver");
MODULE_AUTHOR("Greg Clemson <gclemson@audience.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:escore-codec");
