/*
 * es705-i2c.h  --  Audience eS705 I2C interface
 *
 * Copyright 2011 Audience, Inc.
 *
 * Author: Greg Clemson <gclemson@audience.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ES705_I2C_H
#define _ES705_I2C_H

#include "es705.h"

#define ES705_I2C_BOOT_CMD			0x0001
#define ES705_I2C_BOOT_ACK			0x0101

extern struct i2c_driver es705_i2c_driver;
int es705_i2c_init(struct es705_priv *es705);

extern struct es_stream_device i2c_streamdev;

#endif
