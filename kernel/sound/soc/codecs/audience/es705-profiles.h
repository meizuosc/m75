/*
 * es705-routes.h  --  Audience eS705 ALSA SoC Audio driver
 *
 * Copyright 2013 Audience, Inc.
 *
 * Author: Genisim Tsilker <gtsilker@audience.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ES705_PROFILES_H
#define _ES705_PROFILES_H

#define ES705_CUSTOMER_PROFILE_MAX 4
static u32 es705_audio_custom_profiles[ES705_CUSTOMER_PROFILE_MAX][20] = {
	{
		0xffffffff	/* terminate */
	},
	{
		0xffffffff	/* terminate */
	},
	{
		0xffffffff	/* terminate */
	},
	{
		0xffffffff	/* terminate */
	},
};

#endif
