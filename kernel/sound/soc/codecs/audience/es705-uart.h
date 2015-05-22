/*
 * es705-uart.h  --  Audience eS705 UART interface
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

#ifndef _ES705_UART_H
#define _ES705_UART_H

struct es705_priv;

struct es705_uart_device {
	struct tty_struct *tty;
	struct file *file;
};

extern struct platform_driver es705_uart_driver;
extern struct platform_device es705_uart_device;

int es705_uart_bus_init(struct es705_priv *es705);
int es705_uart_fw_download(struct es705_priv *es705, int fw_type);
int es705_uart_es705_wakeup(struct es705_priv *es705);

#endif
