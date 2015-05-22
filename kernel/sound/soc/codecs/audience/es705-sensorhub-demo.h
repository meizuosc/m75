/*
 * es705-sensorhub-demo.h  --  Audience eS705 Sensor Hub demo driver
 *
 * Copyright 2013 Sensor Platforms, Inc.
 *
 * Author: Rajiv Verma <rverma@sensorplatforms.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef ES705SENSORHUBDEMO_H
#define ES705SENSORHUBDEMO_H


#define ES705_SENSOR_HUB_BLOCK_MAX	100 /* bytes */
#define SENS_HUB_RDB			0x802E0008
#define GET_EVENT_ST			0x806D0000
#define ES705_CMD_EVT_INTR_ACTHIGH	0x801A0002

#define EVT_SENSOR_DATA_READY		0x0008

struct spi_data {
	struct input_dev *acc_input_dev;
	struct input_dev *gyro_input_dev;
	struct input_dev *mag_input_dev;

	struct device *acc_device;
	struct device *gyro_device;
	struct device *mag_device;
};

/* This structure is designed for the way the read-data block gets values as
  32-bit quantity. The actual formatting of the data is as follows (16-bits):
  | sensor-ID | x-axis || y-axis | z-axis || time | stamp |
  '||' demarkates the 32-bit boundary
  */

struct sensor_sample {
	s16 x;  /* x-axis data (signed 2's complement) */
	u16 id; /* Sensor Identifier */
	s16 z;
	s16 y;
	u32 timeStamp;
};

int es705_sensor_hub_enable_event_interrupt(struct es705_priv *es705);
int es705_sensor_hub_init_data_driver(struct es705_priv *es705);
void es705_sensor_hub_remove_data_driver(struct es705_priv *es705);

#endif /* ES705SENSORHUBDEMO_H */
