/*
 * es705-uart-common.h  --  Audience eS705 UART interface
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

#ifndef _ES705_UART_COMMON_H
#define _ES705_UART_COMMON_H

#define UART_TTY_DEVICE_NODE		"/dev/ttyHS1"
#define UART_TTY_BAUD_RATE_BOOTLOADER	460800
#define UART_TTY_BAUD_RATE_FW_DOWNLOAD	1000000
#define UART_TTY_BAUD_RATE_FIRMWARE	3000000
#define UART_TTY_STOP_BITS		2
#define UART_TTY_WRITE_SZ		512

#define ES705_SBL_SYNC_CMD		0x00
#define ES705_SBL_SYNC_ACK		ES705_SBL_SYNC_CMD
#define ES705_SBL_BOOT_CMD		0x01
#define ES705_SBL_BOOT_ACK		ES705_SBL_BOOT_CMD
#define ES705_SBL_FW_ACK		0x02

enum {
	UART_CLOSE,
	UART_OPEN,
};

int es705_uart_read(struct es705_priv *es705, void *buf, int len);
int es705_uart_write(struct es705_priv *es705, const void *buf, int len);
int es705_uart_write_then_read(struct es705_priv *es705, const void *buf,
			       int len, u32 *rspn, int match);
int es705_uart_cmd(struct es705_priv *es705, u32 cmd, int sr, u32 *resp);
int es705_configure_tty(struct tty_struct *tty, u32 bps, int stop);
int es705_uart_open(struct es705_priv *es705);
int es705_uart_close(struct es705_priv *es705);
int es705_uart_wait(struct es705_priv *es705);
extern int es705_uart_dev_rdb(struct es705_priv *es705, void *buf, int id);
extern int es705_uart_dev_wdb(struct es705_priv *es705,
				const void *buf, int len);


extern struct es_stream_device uart_streamdev;
extern struct es_datablock_device uart_datablockdev;

#endif
