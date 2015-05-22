/*
 * escore-spi.h  --  Audience eS705 SPI interface
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


#ifndef _ESCORE_SPI_H
#define _ESCORE_SPI_H

extern struct spi_driver escore_spi_driver;

#define ES_SPI_BOOT_CMD			0x00000001
#define ES_SPI_BOOT_ACK			0x00010001
#define ES_SPI_SBL_SYNC_CMD		0x80000000
#define ES_SPI_SBL_SYNC_ACK		0x8000FFFF

extern struct es_stream_device spi_streamdev;
extern int escore_spi_init(void);
extern void escore_spi_exit(void);

#endif
