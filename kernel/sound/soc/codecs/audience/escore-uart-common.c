/*
 * escore-uart-common.c  --  Audience eS705 UART interface
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
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/kthread.h>
#include <linux/esxxx.h>
#include <linux/serial_core.h>
#include <linux/tty.h>

#include "escore.h"
#include "escore-uart-common.h"

int escore_uart_read_internal(struct escore_priv *escore, void *buf, int len)
{
	int rc;
	mm_segment_t oldfs;
	int trys = 0;

	dev_dbg(escore->dev, "%s() size %d\n", __func__, len);

	/*
	 * we may call from user context via char dev, so allow
	 * read buffer in kernel address space
	 */
	oldfs = get_fs();
	set_fs(KERNEL_DS);

	while (trys < ES_UART_READ_TRYS) {
		rc = escore_uart.ld->ops->read(escore_uart.tty,
		escore_uart.file, (char __user *)buf, len);
		if (rc == -EAGAIN) {
			trys++;
			usleep_range(1000, 1000);
			continue;
		}
		break;
	}

	/* restore old fs context */
	set_fs(oldfs);

	dev_dbg(escore->dev, "%s() returning %d\n", __func__, rc);

	return rc;
}

int escore_uart_read(struct escore_priv *escore, void *buf, int len)
{
	int rc;

	rc = escore_uart_read_internal(escore, buf, len);

	if (rc < len)
		rc = -EIO;
	else
		rc = 0;

	return rc;
}

int escore_uart_write(struct escore_priv *escore, const void *buf, int len)
{
	int rc = 0;
	int count_remain = len;
	int bytes_wr = 0;
	mm_segment_t oldfs;

	 dev_dbg(escore->dev, "%s() size %d\n", __func__, len);

	/*
	 * we may call from user context via char dev, so allow
	 * read buffer in kernel address space
	 */
	oldfs = get_fs();
	set_fs(KERNEL_DS);

	while (count_remain > 0) {
		/* block until tx buffer space is available */
		while (tty_write_room(escore_uart.tty) < UART_TTY_WRITE_SZ)
			usleep_range(5000, 5000);

		rc = escore_uart.ld->ops->write(escore_uart.tty,
			escore_uart.file, buf + bytes_wr,
				min(UART_TTY_WRITE_SZ, count_remain));

		if (rc < 0) {
			goto err_out;
		}

		bytes_wr += rc;
		count_remain -= rc;
	}

err_out:
	/* restore old fs context */
	set_fs(oldfs);
	if (count_remain)
		rc = -EIO;
	else
		rc = 0;
	 pr_debug("%s() returning %d\n", __func__, rc);

	return rc;
}

int escore_uart_cmd(struct escore_priv *escore, u32 cmd, u32 *resp)
{
	int err = 0;
	int sr = cmd & BIT(28);
	u32 rv;
	int retry = ES_MAX_RETRIES + 1;

	dev_dbg(escore->dev,
			"%s: cmd=0x%08x sr=0x%08x\n", __func__, cmd, sr);

	cmd = cpu_to_be32(cmd);
	err = escore_uart_write(escore, &cmd, sizeof(cmd));
	if (err || sr)
		return err;

	do {
		if (escore->cmd_compl_mode == ES_CMD_COMP_INTR) {
			pr_debug("%s(): Waiting for API interrupt. Jiffies:%lu",
					__func__, jiffies);
			err = wait_for_completion_timeout(&escore->cmd_compl,
					msecs_to_jiffies(ES_RESP_TOUT_MSEC));
			if (!err) {
				pr_debug("%s(): API Interrupt wait timeout\n",
						__func__);
				err = -ETIMEDOUT;
				break;
			}
		} else {
			usleep_range(ES_RESP_POLL_TOUT,
					ES_RESP_POLL_TOUT + 500);
		}
		err = escore_uart_read(escore, &rv, sizeof(rv));
		dev_dbg(escore->dev, "%s: err=%d\n", __func__, err);
		*resp = be32_to_cpu(rv);
		dev_dbg(escore->dev, "%s: *resp=0x%08x\n", __func__, *resp);
		if (err) {
			dev_dbg(escore->dev,
				"%s: escore_uart_read() failure\n", __func__);
		} else if ((*resp & ES_ILLEGAL_CMD) == ES_ILLEGAL_CMD) {
			dev_err(escore->dev, "%s: illegal command 0x%08x\n",
				__func__, cmd);
			err = -EINVAL;
			goto cmd_exit;
		} else if (*resp == ES_NOT_READY) {
			dev_dbg(escore->dev,
				"%s: escore_uart_read() not ready\n", __func__);
			err = -ETIMEDOUT;
		} else {
			goto cmd_exit;
		}

		--retry;
	} while (retry != 0 && escore->cmd_compl_mode != ES_CMD_COMP_INTR);

cmd_exit:
	return err;
}

int escore_configure_tty(struct tty_struct *tty, u32 bps, int stop)
{
	int rc = 0;

	struct ktermios termios;
	termios = tty->termios;

	pr_debug("%s(): Requesting baud %u\n", __func__, bps);

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

	/* Added to set baudrate dynamically */
	tty_wait_until_sent(escore_uart.tty, 0);

	rc = tty_set_termios(tty, &termios);

	 pr_debug("%s(): New baud %u\n", __func__, tty->termios.c_ospeed);

	return rc;
}
EXPORT_SYMBOL_GPL(escore_configure_tty);

int escore_uart_open(struct escore_priv *escore)
{
	long err = 0;
	struct file *fp = NULL;
	unsigned long timeout = jiffies + msecs_to_jiffies(60000);
	int attempt = 0;

	/* try to probe tty node every 1 second for 60 seconds */
	do {

		if (attempt)
			ssleep(1);

		 pr_debug("%s(): probing for tty on %s (attempt %d)\n",
			 __func__, UART_TTY_DEVICE_NODE, ++attempt);

		fp = filp_open(UART_TTY_DEVICE_NODE,
			       O_RDWR | O_NONBLOCK | O_NOCTTY,
			       0);
		err = PTR_ERR(fp);
	} while (time_before(jiffies, timeout) && err == -ENOENT);

	if (IS_ERR_OR_NULL(fp)) {
		pr_err("%s(): UART device node open failed\n", __func__);
		return -ENODEV;
	}

	/* device node found */
	  pr_debug("%s(): successfully opened tty\n",
		  __func__);

	/* set uart_dev members */
	escore_uart.file = fp;
	escore_uart.tty =
		((struct tty_file_private *)fp->private_data)->tty;
	escore_uart.ld = tty_ldisc_ref(
		escore_uart.tty);

	pr_err("func %s(), ref \n", __func__);
	/* set baudrate to FW baud (common case) */
	escore_configure_tty(escore_uart.tty,
		UART_TTY_BAUD_RATE_3_M, UART_TTY_STOP_BITS);

	escore->uart_ready = 1;
	return 0;
}

int escore_uart_close(struct escore_priv *escore)
{
	pr_err("func %s(), deref \n", __func__);
	tty_ldisc_deref(escore_uart.ld);
	filp_close(escore_uart.file, 0);
	escore->uart_ready = 0;

	return 0;
}

int escore_uart_wait(struct escore_priv *escore)
{
	/* wait on tty read queue until awoken or for 50ms */
	return wait_event_interruptible_timeout(
		escore_uart.tty->read_wait,
		false,
		msecs_to_jiffies(50));
}

struct es_stream_device es_uart_streamdev = {
	.open = escore_uart_open,
	.read = escore_uart_read_internal,
	.close = escore_uart_close,
	.wait = escore_uart_wait,
	.intf = ES_UART_INTF,
};

MODULE_DESCRIPTION("ASoC ESCORE driver");
MODULE_AUTHOR("Greg Clemson <gclemson@audience.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:escore-codec");
