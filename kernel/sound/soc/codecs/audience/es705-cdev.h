/*
 * es705-slim.h  --  Audience eS705 character device interface.
 *
 * Copyright 2013 Audience, Inc.
 *
 * Author: Marc Butler <mbutler@audience.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _ES705_CDEV_H
#define _ES705_CDEV_H

/* This interface is used to support development and deployment
 * tasks. It does not replace ALSA as a controll interface.
 */

int es705_init_cdev(struct es705_priv *es705);
void es705_cleanup_cdev(struct es705_priv *es705);

#endif
