/*
 * escore-uart.h  --  Audience eS705 UART interface
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

#ifndef _ESCORE_UART_H
#define _ESCORE_UART_H

struct escore_priv;

struct escore_uart_device {
	struct tty_struct *tty;
	struct file *file;
	struct tty_ldisc *ld;
	unsigned int baudrate_bootloader;
	unsigned int baudrate_fw;
};

extern struct platform_driver escore_uart_driver;
extern struct platform_device escore_uart_device;
extern struct escore_uart_device escore_uart;
int escore_uart_bus_init(struct escore_priv *escore);
#if defined(CONFIG_SND_SOC_ES_UART) ||\
	defined(CONFIG_SND_SOC_ES_HIGH_BW_BUS_UART) ||\
	defined(CONFIG_SND_SOC_ES_WAKEUP_UART)
int escore_uart_add_dummy_dev(struct escore_priv *escore);
#else
#define escore_uart_add_dummy_dev(x) ({ do {} while (0); 0; })
#endif

#endif
