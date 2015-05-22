/*
 * es705-uart.c  --  Audience eS705 UART interface
 *
 * Copyright 2013 Audience, Inc.
 *
 * Author: Matt Lupfer <mlupfer@cardinalpeak.com>
 *
 *  Code Updates:
 *       Genisim Tsilker <gtsilker@audience.com>
 *            - Modify UART read / write / write_then_read functions
 *            - Optimize UART open / close functions
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
#if defined(CONFIG_MQ100_SENSOR_HUB)
#include <es705-api.h>
#endif
#include "es705.h"
#include "es705-uart.h"
#include "es705-uart-common.h"
#include "es705-cdev.h"

static int es705_uart_boot_setup(struct es705_priv *es705);
static int es705_uart_boot_finish(struct es705_priv *es705);
static int es705_uart_probe(struct platform_device *dev);
static int es705_uart_remove(struct platform_device *dev);

#ifdef ES705_FW_LOAD_BUF_SZ
#undef ES705_FW_LOAD_BUF_SZ
#endif
#define ES705_FW_LOAD_BUF_SZ 1024
#define ES705_CHANGE_BAUD_CMD 0x8019
/* Firmware repsonses are non deterministic in SBL mode
 * This parameter can be tuned further
 */
#define ES705_MAX_READ_RETRIES 100

#define ES705_SBL_SET_RATE_REQ_CMD 0x8019
u32 es705_uart_baud[UART_RATE_MAX] = {
	460800, 921600, 1000000,
	1024000, 1152000, 2000000,
	2048000, 3000000, 3072000
};

int es705_uart_boot_setup(struct es705_priv *es705)
{
	u8 sbl_sync_cmd = ES705_SBL_SYNC_CMD;
	u8 sbl_boot_cmd = ES705_SBL_BOOT_CMD;
	u32 baudrate_change_cmd = ES705_SBL_SET_RATE_REQ_CMD << 16;
	u32 baudrate_change_resp;
	u8 i;
	int uart_tty_baud_rate = 0;
	u8 read_cnt = 0;
	char msg[4];
	int rc;

#ifndef CONFIG_SND_SOC_ES705_CLOCK_INDEX
	dev_err(es705->dev, "%s(): CONFIG_SND_SOC_ES705_CLOCK_INDEX undefined !!! UART bootup fail\n",
		__func__);
	goto es705_bootup_failed;
#else
	es705->pdata->ext_clk_rate = CONFIG_SND_SOC_ES705_CLOCK_INDEX;
#endif

#if defined(CONFIG_MQ100_SENSOR_HUB)
	es705_indicate_state_change(MQ100_STATE_RESET);
#endif
	es705_configure_tty(es705_priv.uart_dev.tty,
		UART_TTY_BAUD_RATE_BOOTLOADER, UART_TTY_STOP_BITS);

	/* SBL SYNC BYTE 0x00 */
	rc = es705_uart_write(es705, &sbl_sync_cmd, sizeof(u8));
	if (rc < 0) {
		dev_err(es705->dev, "%s(): sbl sync write\n",
				__func__);
		goto es705_bootup_failed;
	}

synca_retry:
	rc = es705_uart_read(es705, msg, sizeof(u8));
	if (rc < 1) {

		dev_err(es705->dev, "%s(): sync ack rc = %d\n",
				__func__, rc);
		goto es705_bootup_failed;
	}
	dev_dbg(es705->dev, "%s(): msg 0x%02X",
			__func__, msg[0]);
	if (msg[0] != ES705_SBL_SYNC_ACK
			&& read_cnt < ES705_MAX_READ_RETRIES) {

		dev_err(es705->dev, "%s(): sync ack 0x%02x\n",
				__func__, msg[0]);
		read_cnt++;
		goto synca_retry;
	} else if (read_cnt == ES705_MAX_READ_RETRIES) {

		rc = -EIO;
		dev_err(es705->dev, "%s(): Invalid response, maxout\n",
				__func__);
		goto es705_bootup_failed;
	}
	read_cnt = 0;

	/* SBL API - SetRateChangeReq */
	for (i = 0; i < ARRAY_SIZE(es705_uart_baud); i++) {

		if (es705_uart_baud[i] == UART_TTY_BAUD_RATE_FIRMWARE)
			uart_tty_baud_rate = i;
	}

	baudrate_change_cmd |= uart_tty_baud_rate << 8;
	baudrate_change_cmd |= (es705->pdata->ext_clk_rate & 0xff);
	baudrate_change_cmd = cpu_to_be32(baudrate_change_cmd);
	rc = es705_uart_write(es705, &baudrate_change_cmd, sizeof(u32));
	baudrate_change_cmd = cpu_to_be32(baudrate_change_cmd);
	dev_dbg(es705->dev, "%s(): baudrate_change_cmd 0x%08X\n",
			__func__, baudrate_change_cmd);

	if (rc < 0) {
		dev_err(es705->dev, "%s(): baud write\n", __func__);
		goto es705_bootup_failed;
	}
	baudrate_change_resp = 0;
	i = sizeof(baudrate_change_resp);

	do {
		msleep(20);
		msg[0] = 0;
		rc = es705_uart_read(es705, msg, sizeof(char));
		if (msg[0] != 0) {

			i--;
			/* Read a byte and move it to
			 * it's appropriate position
			 * Actual response 0x0x0x1980
			 * Response 0x80190x0x */
			baudrate_change_resp |=
				((0x00000000 | msg[0]) << (i*8));
		}
		if (rc < 1) {

			pr_err("%s(): Set Rate Request read rc = %d\n",
					__func__, rc);
			rc = rc < 0 ? rc : -EIO;
			goto es705_bootup_failed;
		}
		dev_dbg(es705->dev, "%s(): baudrate_change_resp 0x%08X\n",
				__func__, baudrate_change_resp);
		read_cnt++;
		if (baudrate_change_resp == baudrate_change_cmd)
			break;
	} while (read_cnt < ES705_MAX_READ_RETRIES);

	if (rc < 1) {

		dev_err(es705->dev, "%s(): baud ack rc = %d\n",
				__func__, rc);
		goto es705_bootup_failed;
	}
	es705_configure_tty(es705_priv.uart_dev.tty,
		es705_uart_baud[UART_RATE_3kk], UART_TTY_STOP_BITS);
	read_cnt = 0;

	/* SBL BOOT BYTE 0x01 */
	rc = es705_uart_write(es705, &sbl_boot_cmd, sizeof(u8));
	if (rc < 0) {

		dev_err(es705->dev, "%s(): sbl boot cmd\n", __func__);
		goto es705_bootup_failed;
	}

bootb_retry:
	rc = es705_uart_read(es705, msg, sizeof(u8));
	if (rc < 1) {

		dev_err(es705->dev, "%s(): sbl boot nack rc %d\n",
				__func__, rc);
		goto es705_bootup_failed;
	}
	if (msg[0] != ES705_SBL_BOOT_ACK
			&& read_cnt < ES705_MAX_READ_RETRIES) {

		dev_err(es705->dev, "%s(): sbl boot nack 0x%02x\n",
				__func__, msg[0]);
		read_cnt++;
		goto bootb_retry;
	} else if (read_cnt == ES705_MAX_READ_RETRIES) {

		rc = -EIO;
		dev_err(es705->dev, "%s(): Invalid response, maxout\n",
				__func__);
		goto es705_bootup_failed;
	}
	rc = 0;

es705_bootup_failed:
	return rc;
}

int es705_uart_boot_finish(struct es705_priv *es705)
{
	u32 sync_cmd = (ES705_SYNC_CMD << 16) | ES705_SYNC_POLLING;
	u32 sync_resp = 0;
	char msg[4];
	int rc;

	/*
	 * Give the chip some time to become ready after firmware
	 * download. (FW is still transferring)
	 */
	msleep(200);

	/* discard up to two extra bytes from es705 during firmware load */
	memset(msg, 0, sizeof(u32));
	rc = es705_uart_read(es705, msg, sizeof(u32));
	if (rc < 1) {
		dev_err(es705->dev, "%s(): firmware read\n", __func__);
		rc = rc < 0 ? rc : -EIO;
		goto es705_bootup_failed;
	}

	/* now switch to firmware baud to talk to chip */
	es705_configure_tty(es705_priv.uart_dev.tty,
		es705_uart_baud[es705->uart_app_rate],
		UART_TTY_STOP_BITS);

	dev_dbg(es705->dev, "%s(): write ES705_SYNC_CMD = 0x%08x\n",
		__func__, sync_cmd);
	rc = es705_uart_cmd(es705, sync_cmd, 0, &sync_resp);
	if (rc < 0) {
		dev_err(es705->dev, "%s(): firmware load failed (no sync response)\n",
			__func__);
		goto es705_bootup_failed;
	}
	if (sync_cmd != sync_resp) {
		dev_err(es705->dev, "%s(): firmware load failed (invalid sync response)\n",
			__func__);
		dev_err(es705->dev, "%s(): firmware load failed (response was: 0x%08x)\n",
			__func__, sync_resp);
		goto es705_bootup_failed;
	}
#if defined(CONFIG_MQ100_SENSOR_HUB)
	es705_indicate_state_change(MQ100_STATE_NORMAL);
#endif
	dev_info(es705->dev, "%s(): firmware load success", __func__);

es705_bootup_failed:
	return rc;
}
#if defined(CONFIG_MQ100_SENSOR_HUB)
#define PAD4(len) ((4 - (len%4)) & 3)
#endif
int es705_uart_dev_rdb(struct es705_priv *es705, void *buf, int id)
{
	u32 cmd;
	u32 resp;
	u8 *dptr;
	int ret;
	int size;
	int rdcnt = 0;
#if defined(CONFIG_MQ100_SENSOR_HUB)
	u8 pad;
	/* Take Mutex for the duration of this UART transaction. */
	mutex_lock(&es705_priv.uart_transaction_mutex);
#endif
	dptr = (u8 *)buf;
	/* Read voice sense keyword data block request. */
	cmd = (ES705_RDB_CMD << 16) | (id & 0xFFFF);
	cmd = cpu_to_be32(cmd);
	ret = es705_uart_write(es705, (char *)&cmd, 4);
	if (ret < 0) {
		dev_err(es705->dev, "%s(): RDB cmd write err = %d\n",
			__func__, ret);
		goto rdb_err;
	}

	/* Refer to "ES705 Advanced API Guide" for details of interface */
	usleep_range(10000, 10000);

	ret = es705_uart_read(es705, (char *)&resp, 4);
	if (ret < 0) {
		dev_err(es705->dev, "%s(): error sending request = %d\n",
			__func__, ret);
		goto rdb_err;
	}

	be32_to_cpus(&resp);
	size = resp & 0xffff;
	dev_dbg(es705->dev, "%s(): resp=0x%08x size=%d\n",
		__func__, resp, size);
	if ((resp & 0xffff0000) != (ES705_RDB_CMD << 16)) {
		dev_err(es705->dev,
			"%s(): invalid read v-s data block response = 0x%08x\n",
			__func__, resp);
		goto rdb_err;
	}

	if (size == 0) {
		dev_err(es705->dev, "%s(): read request return size of 0\n",
			__func__);
		goto rdb_err;
	}
	if (size > PARSE_BUFFER_SIZE)
		size = PARSE_BUFFER_SIZE;

	for (rdcnt = 0; rdcnt < size; rdcnt++, dptr++) {
		ret = es705_uart_read(es705, dptr, 1);
		if (ret < 0) {
			dev_err(es705->dev,
				"%s(): data block ed error %d bytes ret = %d\n",
				__func__, rdcnt, ret);
			goto rdb_err;
		}
	}

	es705->rdb_read_count = size;

#if defined(CONFIG_MQ100_SENSOR_HUB)
	size = PAD4(size);
	dev_dbg(es705->dev, "%s: Discarding %d pad bytes\n",
			__func__, size);
	ret = 0;
	while ((size > 0) && (ret >= 0)) {
		ret = es705_uart_read(es705, &pad, 1);
		size--;
	}
#endif
	ret = 0;
	goto exit;

rdb_err:
	es705->rdb_read_count = 0;
	ret = -EIO;
exit:
#if defined(CONFIG_MQ100_SENSOR_HUB)
	/* release UART transaction mutex */
	mutex_unlock(&es705_priv.uart_transaction_mutex);
#endif
	return ret;
}

int es705_uart_dev_wdb(struct es705_priv *es705, const void *buf,
			int len)
{
	/* At this point the command has been determined and is not part
	 * of the data in the buffer. Its just data. Note that we donot
	 * evaluate the contents of the data. It is up to the higher layers
	 * to insure the the codes mode of operation matchs what is being
	 * sent.
	 */
	int ret;
	u32 resp;
	u8 *dptr;

	u32 cmd = ES705_WDB_CMD << 16;
	dev_dbg(es705->dev, "%s(): len = 0x%08x\n", __func__, len);
	dptr = (char *)buf;

#if defined(CONFIG_MQ100_SENSOR_HUB)
	/* Take Mutex for the duration of this UART transaction. */
	mutex_lock(&es705_priv.uart_transaction_mutex);
#endif

	cmd = cmd | (len & 0xFFFF);
	dev_dbg(es705->dev, "%s(): cmd = 0x%08x\n", __func__, cmd);
	cmd = cpu_to_be32(cmd);
	ret = es705_uart_write_then_read(es705, &cmd, sizeof(cmd), &resp, 0);
	if (ret < 0) {
		dev_err(es705->dev, "%s(): cmd write err=%hu\n",
			__func__, ret);
		goto wdb_err;
	}

	be32_to_cpus(&resp);
	dev_dbg(es705->dev, "%s(): resp = 0x%08x\n", __func__, resp);
	if ((resp & 0xffff0000) != (ES705_WDB_CMD << 16)) {
		dev_err(es705->dev, "%s(): invalid write data block 0x%08x\n",
			__func__, resp);
		goto wdb_err;
	}

	/* The API requires that the subsequent writes are to be
	 * a byte stream (one byte per transport transaction)
	 */
	ret = es705_uart_write(es705, dptr, len);
	if (ret < 0) {
		dev_err(es705->dev, "%s(): wdb error =%d\n", __func__, ret);
		goto wdb_err;
	}

	/* One last ACK read */
	ret = es705_uart_read(es705, &resp, 4);
	if (ret < 0) {
		dev_err(es705->dev, "%s(): last ack %d\n", __func__, ret);
		goto wdb_err;
	}
	if (resp & 0xff000000) {
		dev_err(es705->dev, "%s(): write data block error 0x%0x\n",
			__func__, resp);
		goto wdb_err;
	}
	dev_dbg(es705->dev, "%s(): len = %d\n", __func__, len);
	goto exit;

wdb_err:
	len = -EIO;
exit:
#if defined(CONFIG_MQ100_SENSOR_HUB)
	/* release UART transaction mutex */
	mutex_unlock(&es705_priv.uart_transaction_mutex);
#endif
	return len;
}

int es705_uart_es705_wakeup(struct es705_priv *es705)
{
	int rc;
	char wakeup_char = 'A';

	rc = es705_uart_open(es705);
	if (rc) {
		dev_err(es705->dev, "%s(): uart open error\n",
			__func__);
		goto es705_uart_es705_wakeup_exit;
	}

	/* eS705 wakeup. Write wakeup character to UART */
	rc = es705_uart_write(es705, &wakeup_char, sizeof(wakeup_char));
	if (rc < 0)
		dev_err(es705->dev, "%s(): wakeup via uart FAIL\n",
			__func__);

es705_uart_es705_wakeup_exit:
	es705_uart_close(es705);

	return rc;
}

int es705_uart_fw_download(struct es705_priv *es705, int fw_type)
{
	int rc;

	rc = es705_uart_open(es705);
	if (rc) {
		dev_err(es705->dev, "%s(): uart open error\n",
			__func__);
		goto uart_open_error;
	}

	rc = es705_uart_boot_setup(es705);
	if (rc < 0) {
		dev_err(es705->dev, "%s(): uart boot setup error\n",
			__func__);
		goto uart_download_error;
	}

	if (fw_type == VOICESENSE)
		rc = es705_uart_write(es705, (char *)es705->vs->data,
				es705->vs->size);
	else
		rc = es705_uart_write(es705, (char *)es705->standard->data,
				es705->standard->size);
	if (rc < 0) {
		dev_err(es705->dev, "%s(): uart %s image write fail\n",
			__func__, fw_type == VOICESENSE ? "vs" : "standard");
		rc = -EIO;
		goto uart_download_error;
	}

	dev_dbg(es705->dev, "%s(): UART VS load done\n", __func__);

	rc = es705_uart_boot_finish(es705);
	if (rc < 0)
		dev_err(es705->dev, "%s(): uart boot finish error\n",
			__func__);

uart_download_error:
	es705_uart_close(es705);

uart_open_error:
	return rc;
}

int es705_uart_probe_thread(void *ptr)
{
	int rc = 0;
	struct device *dev = (struct device *) ptr;

	rc = es705_uart_open(&es705_priv);
	if (rc) {
		dev_err(dev, "%s(): es705_uart_open() failed %d\n",
			__func__, rc);
		return rc;
	}

	/* set es705 function pointers */
	es705_priv.dev_read = es705_uart_read;
	es705_priv.dev_write = es705_uart_write;
	es705_priv.dev_write_then_read = es705_uart_write_then_read;
	es705_priv.dev_wdb = es705_uart_dev_wdb;
	es705_priv.dev_rdb = es705_uart_dev_rdb;
	es705_priv.cmd = es705_uart_cmd;
	es705_priv.boot_setup = es705_uart_boot_setup;
	es705_priv.boot_finish = es705_uart_boot_finish;

	es705_priv.streamdev = uart_streamdev;
	es705_priv.datablockdev = uart_datablockdev;

	rc = es705_core_probe(dev);
	if (rc) {
		dev_err(dev, "%s(): es705_core_probe() failed %d\n",
			__func__, rc);
		goto bootup_error;
	}

	rc = es705_bootup(&es705_priv);

	if (rc) {
		dev_err(dev, "%s(): es705_bootup failed %d\n",
			__func__, rc);
		goto bootup_error;
	}

	rc = snd_soc_register_codec(dev, &soc_codec_dev_es705,
		es705_dai, ES705_NUM_CODEC_DAIS);
	dev_dbg(dev, "%s(): rc = snd_soc_regsiter_codec() = %d\n", __func__,
		rc);

	/* init es705 character device here, now that the UART is discovered */
	rc = es705_init_cdev(&es705_priv);
	if (rc) {
		dev_err(dev, "%s(): failed to initialize char device = %d\n",
			__func__, rc);
		goto cdev_init_error;
	}

	return rc;

bootup_error:
	/* close filp */
	es705_uart_close(&es705_priv);
cdev_init_error:
	dev_dbg(es705_priv.dev, "%s(): exit with error\n", __func__);
	return rc;
}

static int es705_uart_probe(struct platform_device *dev)
{
	int rc = 0;
	struct task_struct *uart_probe_thread = NULL;

	uart_probe_thread = kthread_run(es705_uart_probe_thread,
					(void *) &dev->dev,
					"es705 uart thread");
	if (IS_ERR_OR_NULL(uart_probe_thread)) {
		dev_err(&dev->dev, "%s(): can't create es705 UART probe thread = %p\n",
			__func__, uart_probe_thread);
		rc = -ENOMEM;
	}

	return rc;
}

static int es705_uart_remove(struct platform_device *dev)
{
	int rc = 0;
	/*
	 * ML: GPIO pins are not connected
	 *
	 * struct esxxx_platform_data *pdata = es705_priv.pdata;
	 *
	 * gpio_free(pdata->reset_gpio);
	 * gpio_free(pdata->wakeup_gpio);
	 * gpio_free(pdata->gpioa_gpio);
	 */

	if (es705_priv.uart_dev.file)
		es705_uart_close(&es705_priv);

	es705_priv.uart_dev.tty = NULL;
	es705_priv.uart_dev.file = NULL;

	snd_soc_unregister_codec(es705_priv.dev);

	return rc;
}

struct platform_driver es705_uart_driver = {
	.driver = {
		.name = "es705-codec",
		.owner = THIS_MODULE,
	},
	.probe = es705_uart_probe,
	.remove = es705_uart_remove,
};

/* FIXME: Kludge for es705_bus_init abstraction */
int es705_uart_bus_init(struct es705_priv *es705)
{
	int rc;

	rc = platform_driver_register(&es705_uart_driver);
	if (rc)
		return rc;

	dev_dbg(es705->dev, "%s(): registered as UART", __func__);

	return rc;
}

MODULE_DESCRIPTION("ASoC ES705 driver");
MODULE_AUTHOR("Matt Lupfer <mlupfer@cardinalpeak.com>");
MODULE_AUTHOR("Genisim Tsilker <gtsilker@audience.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:es705-codec");
