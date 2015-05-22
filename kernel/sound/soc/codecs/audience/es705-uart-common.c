/*
 * es705-uart-common.c  --  Audience eS705 UART interface
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
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/kthread.h>
#include <linux/esxxx.h>
#include <linux/serial_core.h>
#include <linux/tty.h>
#include <linux/fs.h>

#include "es705.h"
#include "es705-uart-common.h"

#define MAX_EAGAIN_RETRY	20
#define EAGAIN_RETRY_DELAY	1000
int es705_uart_read(struct es705_priv *es705, void *buf, int len)
{
	int rc;
	mm_segment_t oldfs;
	loff_t pos = 0;
	int retry = MAX_EAGAIN_RETRY;

	dev_dbg(es705->dev, "%s(): size %d\n", __func__, len);

	/*
	 * we may call from user context via char dev, so allow
	 * read buffer in kernel address space
	 */
	oldfs = get_fs();
	set_fs(KERNEL_DS);

	do {
		rc = vfs_read(es705->uart_dev.file,
				(char __user *)buf, len, &pos);
		if (rc == -EAGAIN) {
			usleep_range(EAGAIN_RETRY_DELAY, EAGAIN_RETRY_DELAY);
			retry--;
		}
	} while (rc == -EAGAIN && retry);

	/* restore old fs context */
	set_fs(oldfs);

	if (rc >= 0)
		dev_dbg(es705->dev, "%s(): read bytes %d\n", __func__, rc);
	else
		dev_dbg(es705->dev, "%s(): UART read error %d\n", __func__, rc);
	return rc;
}

int es705_uart_write(struct es705_priv *es705, const void *buf, int len)
{
	int rc = 0;
	int count_remain = len;
	int bytes_wr = 0;
	mm_segment_t oldfs;
	loff_t pos = 0;

	dev_dbg(es705->dev, "%s(): size %d\n", __func__, len);

	/*
	 * we may call from user context via char dev, so allow
	 * read buffer in kernel address space
	 */
	oldfs = get_fs();
	set_fs(KERNEL_DS);

	while (count_remain > 0) {
		/* block until tx buffer space is available */
		while (tty_write_room(es705->uart_dev.tty) < UART_TTY_WRITE_SZ)
			usleep_range(2000, 2000);

		rc = vfs_write(es705->uart_dev.file, buf + bytes_wr,
				min(UART_TTY_WRITE_SZ, count_remain), &pos);

		if (rc < 0) {
			bytes_wr = rc;
			goto err_out;
		}

		bytes_wr += rc;
		count_remain -= rc;
	}

err_out:
	/* restore old fs context */
	set_fs(oldfs);
	dev_dbg(es705->dev, "%s(): write bytes %d\n", __func__, bytes_wr);
	return bytes_wr;
}

int es705_uart_write_then_read(struct es705_priv *es705, const void *buf,
			       int len, u32 *rspn, int match)
{
	int rc;
	u32 response = 0;

	rc = es705_uart_write(es705, buf, len);
	if (rc > 0) {
		rc = es705_uart_read(es705, &response, len);
		response = le32_to_cpu(response);

		if (rc < 0) {
			dev_err(es705->dev, "%s(): UART read fail\n",
				__func__);
		} else if (match && *rspn != response) {
			dev_err(es705->dev, "%s(): expected 0x%08x got response 0x%08x\n",
				__func__, *rspn, be32_to_cpu(response));
			*rspn = response;
			rc = -EIO;
		} else {
			*rspn = response;
		}
	}
	return rc;
}

int es705_uart_cmd(struct es705_priv *es705, u32 cmd, int sr, u32 *resp)
{
	int err;
	u32 rv;
#if defined(CONFIG_MQ100_SENSOR_HUB)
	/* Take Mutex for the duration of this UART transaction. */
	mutex_lock(&es705_priv.uart_transaction_mutex);
#endif
	dev_dbg(es705->dev, "%s(): cmd=0x%08x  sr=%i\n", __func__, cmd, sr);

	cmd = cpu_to_be32(cmd);
	err = es705_uart_write(es705, &cmd, sizeof(cmd));
	if (err < 0 || sr)
		goto es705_uart_cmd_exit;

	err = es705_uart_read(es705, &rv, sizeof(rv));
	if (err > 0) {
		*resp = be32_to_cpu(rv);
		dev_dbg(es705->dev, "%s(): response=0x%08x\n", __func__, *resp);
	} else if (err == 0) {
		dev_dbg(es705->dev, "%s(): No response\n", __func__);
	} else {
		dev_err(es705->dev, "%s(): UART read fail\n", __func__);
	}
es705_uart_cmd_exit:
#if defined(CONFIG_MQ100_SENSOR_HUB)
	/* release UART transaction mutex */
	mutex_unlock(&es705_priv.uart_transaction_mutex);
#endif
	return err;
}

int es705_configure_tty(struct tty_struct *tty, u32 bps, int stop)
{
	int rc = 0;

	struct ktermios termios;
	termios = *tty->termios;

	dev_dbg(es705_priv.dev, "%s(): Requesting baud %u\n", __func__, bps);

	termios.c_cflag &= ~(CBAUD | CSIZE | PARENB);   /* clear csize, baud */
	termios.c_cflag |= BOTHER;	      /* allow arbitrary baud */
	termios.c_cflag |= CS8;

	if (stop == 2)
		termios.c_cflag |= CSTOPB;

	/* set uart port to raw mode (see termios man page for flags) */
	termios.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP
		| INLCR | IGNCR | ICRNL | IXON);
	termios.c_oflag &= ~(OPOST);
	termios.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);

	/* set baud rate */
	termios.c_ospeed = bps;
	termios.c_ispeed = bps;

	rc = tty_set_termios(tty, &termios);

	dev_dbg(es705_priv.dev,
		"%s(): New baud %u\n", __func__, tty->termios->c_ospeed);

	return rc;
}

int es705_uart_open(struct es705_priv *es705)
{
	long err = 0;
	struct file *fp = NULL;
	unsigned long timeout = jiffies + msecs_to_jiffies(6000);
	int attempt = 0;

	dev_dbg(es705->dev, "%s(): start open tty\n",
		  __func__);

	if (es705->uart_state == UART_OPEN)
		es705_uart_close(es705);

	/* try to probe tty node every 100 ms for 1 sec */
	do {
		if (attempt > 0)
			msleep(100);
		dev_dbg(es705->dev,
			"%s(): probing for tty on %s (attempt %d)\n",
			 __func__, UART_TTY_DEVICE_NODE, ++attempt);

		fp = filp_open(UART_TTY_DEVICE_NODE,
			       O_RDWR | O_NONBLOCK | O_NOCTTY,
			       0);
		err = PTR_ERR(fp);
	} while (time_before(jiffies, timeout) && err == -ENOENT);

	if (IS_ERR_OR_NULL(fp)) {
		dev_err(es705->dev, "%s(): UART device node open failed, err = %ld\n",
			__func__, err);
		return -ENODEV;
	}

	/* device node found */
	dev_dbg(es705->dev, "%s(): successfully opened tty\n",
		  __func__);

	/* set uart_dev members */
	es705_priv.uart_dev.file = fp;
	es705_priv.uart_dev.tty =
		((struct tty_file_private *)fp->private_data)->tty;

	es705->uart_state = UART_OPEN;
	return 0;
}

int es705_uart_close(struct es705_priv *es705)
{
#ifndef CONFIG_SND_SOC_ES705_UART
	dev_dbg(es705->dev, "%s(): start close tty\n",
		  __func__);
	if (es705->uart_state == UART_OPEN)
		filp_close(es705->uart_dev.file, 0);
	es705->uart_state = UART_CLOSE;
	dev_dbg(es705->dev, "%s(): successfully closed tty\n",
		  __func__);
#endif
	return 0;
}

int es705_uart_wait(struct es705_priv *es705)
{
	/* wait on tty read queue until awoken or for 50ms */
	return wait_event_interruptible_timeout(
		es705->uart_dev.tty->read_wait,
		es705->uart_dev.tty->read_cnt,
		msecs_to_jiffies(50));
}

struct es_stream_device uart_streamdev = {
	.open = es705_uart_open,
	.read = es705_uart_read,
	.close = es705_uart_close,
	.wait = es705_uart_wait,
	.intf = ES705_UART_INTF,
};

struct es_datablock_device uart_datablockdev = {
	.open = es705_uart_open,
	.read = es705_uart_read,
	.close = es705_uart_close,
	.wait = es705_uart_wait,
	.rdb = es705_uart_dev_rdb,
	.wdb = es705_uart_dev_wdb,
	.intf = ES705_UART_INTF,
};

MODULE_DESCRIPTION("ASoC ES705 driver");
MODULE_AUTHOR("Matt Lupfer <mlupfer@cardinalpeak.com>");
MODULE_AUTHOR("Genisim Tsilker <gtsilker@audience.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:es705-codec");
