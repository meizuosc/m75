/*
 * es705-spi.h  --  Audience eS705 SPI interface
 *
 * Copyright 2011 Audience, Inc.
 *
 * Author: Hemal Meghpara <hmeghpara@audience.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#ifndef _ES705_SPI_H
#define _ES705_SPI_H

extern struct spi_driver es705_spi_driver;

#define ES705_SPI_BOOT_ACK		0x01000100

extern struct es_stream_device spi_streamdev;

#endif

