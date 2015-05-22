/*
 * es325-access.h  --  ES325 Soc Audio access values
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ES325_ACCESS_H
#define _ES325_ACCESS_H

#define ES_API_WORD(upper, lower) ((upper << 16) | lower)

static struct escore_api_access es325_api_access[ES_API_ADDR_MAX] = {
	[ES_MIC_CONFIG] = {
		.read_msg = { ES_API_WORD(ES_GET_ALGO_PARAM, 0x0002) },
		.read_msg_len = 4,
		.write_msg = { ES_API_WORD(ES_SET_ALGO_PARAM_ID, 0x0002),
			       ES_API_WORD(ES_SET_ALGO_PARAM, 0x0000) },
		.write_msg_len = 8,
		.val_shift = 0,
		.val_max = 65535,
	},
	[ES_POWER_STATE] = {
		.read_msg = { ES_API_WORD(0x8010, 0x0000) },
		.read_msg_len = 4,
		.write_msg = { ES_API_WORD(0x9010, 0x0000) },
		.write_msg_len = 4,
		.val_shift = 0,
		.val_max = 1,
	},
	[ES_ALGO_PROCESSING] = {
		.read_msg = { ES_API_WORD(0x8043, 0x0000) },
		.read_msg_len = 4,
		.write_msg = { ES_API_WORD(0x801c, 0x0000) },
		.write_msg_len = 4,
		.val_shift = 0,
		.val_max = 1,
	},
	[ES_ALGO_SAMPLE_RATE] = {
		.read_msg = { ES_API_WORD(0x804b, 0x0000) },
		.read_msg_len = 4,
		.write_msg = { ES_API_WORD(0x804c, 0x0000) },
		.write_msg_len = 4,
		.val_shift = 0,
		.val_max = 5,
	},
	[ES_CHANGE_STATUS] = {
		.read_msg = { ES_API_WORD(0x804f, 0x0000) },
		.read_msg_len = 4,
		.write_msg = { ES_API_WORD(0x804f, 0x0000) },
		.write_msg_len = 4,
		.val_shift = 0,
		.val_max = 4,
	},
	[ES_FW_FIRST_CHAR] = {
		.read_msg = { ES_API_WORD(0x8020, 0x0000) },
		.read_msg_len = 4,
		.write_msg = { ES_API_WORD(0x8020, 0x0000) },
		.write_msg_len = 4,
		.val_shift = 0,
		.val_max = 255,
	},
	[ES_FW_NEXT_CHAR] = {
		.read_msg = { ES_API_WORD(0x8021, 0x0000) },
		.read_msg_len = 4,
		.write_msg = { ES_API_WORD(0x8021, 0x0000) },
		.write_msg_len = 4,
		.val_shift = 0,
		.val_max = 255,
	},
};

#endif /* _ES325_ACCESS_H */
