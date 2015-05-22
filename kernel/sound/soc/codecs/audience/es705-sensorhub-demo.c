/*
 * es705-sensorhub-demo.c  --  Audience eS705 Sensor Hub demo driver
 *
 * Copyright 2013 Sensor Platforms, Inc.
 *
 * Author: Rajiv Verma <rverma@sensorplatforms.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/input.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include "es705.h"

#define SUCCESS				0

/* Forward declarations */
static void report_accel_data(struct spi_data *data, struct sensor_sample *acc);
static void report_mag_data(struct spi_data *data, struct sensor_sample *mag);
static void report_gyro_data(struct spi_data *data, struct sensor_sample *gyro);
static void sensor_hub_irq_worker(struct work_struct *work);

static int es705_sensor_hub_handle_data(struct es705_priv *es705)
{
	static u32 buffer[ES705_SENSOR_HUB_BLOCK_MAX/sizeof(u32)];

	u32 cmd;
	u32 resp;
	int ret;
	unsigned size;
	unsigned rdcnt;
	struct sensor_sample *pSensData[3];
	struct spi_data *spiData = es705->sensData;
	static u32 counter;

	/* Note: mutex_lock is done by calling function (IRQ worker) */

	/* Read sensor hub data block request. */
	cmd = cpu_to_le32(SENS_HUB_RDB);
	ret = es705->dev_write(es705, (char *)&cmd, 4);
	if (ret < 0) {
		dev_err(es705->dev,
		    "[SPI]: error sending write request = %d\n",
		    ret);
		goto OUT;
	}

	usleep_range(3000, 4000); /* Wait for response */

	ret = es705->dev_read(es705, (char *)&resp, 4);
	if (ret < 0) {
		dev_err(es705->dev,
		    "[SPI]: error sending read request = %d\n",
		    ret);
		goto OUT;
	}


	le32_to_cpus(resp);
	size = resp & 0xffff;
	/* pr_info("[SPI]: 2.Resp: %08X\n", resp); */
	if ((resp & 0xffff0000) != 0x802E0000) {
		dev_dbg(es705->dev,
		    "[SPI]: invalid read sensor data block response = 0x%08x\n",
		    resp);
		goto OUT;
	}

	BUG_ON(size == 0);
	BUG_ON(size > ES705_SENSOR_HUB_BLOCK_MAX);
	BUG_ON(size % 4 != 0);

	/* This assumes we need to transfer the block in 4 byte
	 * increments. This is true on slimbus, but may not hold true
	 * for other buses.
	 */
	for (rdcnt = 0; rdcnt < size; rdcnt += 4) {
		ret = es705->dev_read(es705, (char *)&resp, 4);
		if (ret < 0) {
			dev_dbg(es705->dev,
			    "[SPI]: error reading data block at %d bytes\n",
			    rdcnt);
			goto OUT;
		}
		buffer[rdcnt/4] = resp;
	}

	/* Parse Sensor Data and publish to respective device nodes */
	pSensData[0] = (struct sensor_sample *)buffer;
	report_accel_data(spiData, pSensData[0]);
	if ((counter % 50) == 0) {
		pr_debug("[SPI]: Sensor Data: %d:\t%d\t%d\t%d\t%d",
				pSensData[0]->id,
				pSensData[0]->x,
				pSensData[0]->y,
				pSensData[0]->z,
				pSensData[0]->timeStamp);
	}

	pSensData[1] = (struct sensor_sample *)&buffer[
			sizeof(struct sensor_sample)/sizeof(u32)];
	report_gyro_data(spiData, pSensData[1]);
	if ((counter % 50) == 0) {
		pr_debug("[SPI]: Sensor Data: %d:\t%d\t%d\t%d\t%d",
				pSensData[1]->id,
				pSensData[1]->x,
				pSensData[1]->y,
				pSensData[1]->z,
				pSensData[1]->timeStamp);
	}

	pSensData[2] = (struct sensor_sample *)&buffer[
			2*sizeof(struct sensor_sample)/sizeof(u32)];
	report_mag_data(spiData, pSensData[2]);
	if ((counter % 50) == 0) {
		pr_debug("[SPI]: Sensor Data: %d:\t%d\t%d\t%d\t%d",
				pSensData[2]->id,
				pSensData[2]->x,
				pSensData[2]->y,
				pSensData[2]->z,
				pSensData[2]->timeStamp);
	}
	counter++;

OUT:
	/* Note: mutex_unlock is done by calling function (IRQ worker) */
	if (ret)
		dev_err(es705->dev, "sens read data block failure=%d\n", ret);
	return ret;
}


int es705_sensor_hub_enable_event_interrupt(struct es705_priv *es705)
{
	u32 sync_ack;
	int rc;

	/* Send command to setup the event interrupt to be active high */
	rc = es705_cmd(es705, ES705_CMD_EVT_INTR_ACTHIGH);
	if (rc == 0) {
		sync_ack = es705->last_response;
		pr_debug("[SPI]: %s(): EvtIntr_ack = 0x%08x\n",
		    __func__, sync_ack);
	}
	return rc;
}

static void sensor_hub_irq_worker(struct work_struct *work)
{
	struct es705_priv *es705 = container_of(work, struct es705_priv,
						sensor_event_work);
	u32 cmd;
	u32 resp = 0;
	int ret;

	mutex_lock(&es705->api_mutex);

	/* Get Event Status */
	cmd = cpu_to_le32(GET_EVENT_ST);
	ret = es705->dev_write(es705, (char *)&cmd, 4);
	if (ret < 0) {
		dev_err(es705->dev,
		    "[SPI]: error sending cmd = %d\n",
		    ret);
		goto IRQ_WRKR_EXIT;
	}

	usleep_range(3000, 4000);

	ret = es705->dev_read(es705, (char *)&resp, 4);
	if (ret < 0) {
		dev_err(es705->dev,
		    "[SPI]: error reading resp = %d\n",
		    ret);
		goto IRQ_WRKR_EXIT;
	}
	/* pr_info("[SPI]: 1.Resp: %08X\n", resp); */
	if ((resp & 0xFFFF0000) != GET_EVENT_ST) {
		dev_dbg(es705->dev,
		    "[SPI]: Invalid response (%d)\n",
		    resp);
		goto IRQ_WRKR_EXIT;
	}

	/* Read sensor hub data block request. */
	if ((resp & EVT_SENSOR_DATA_READY) == EVT_SENSOR_DATA_READY)
		es705_sensor_hub_handle_data(es705);

IRQ_WRKR_EXIT:
	mutex_unlock(&es705->api_mutex);
}

int es705_sensor_hub_init_data_driver(struct es705_priv *es705)
{
	struct spi_data *sensdata;
	int iRet = 0;
	struct input_dev *acc_input_dev, *gyro_input_dev, *mag_input_dev;

	/* Add initialization for sensor input devices */
	sensdata = kzalloc(sizeof(struct spi_data), GFP_KERNEL);
	if (sensdata == NULL) {
		dev_err(es705->dev,
			"[SPI]: %s - failed to allocate memory for sensdata\n",
			__func__);
		return -ENOMEM;
	}

	es705->sensData = sensdata;
	es705->spi_workq = create_singlethread_workqueue(
				"spi_es705_sensor_workq");
	if (es705->spi_workq == NULL) {
		dev_err(es705->dev,
		    "[SPI]: Failed to create workq\n");
		goto iRet_work_queue_err;
	}

	INIT_WORK(&es705->sensor_event_work, sensor_hub_irq_worker);

	/* allocate input_device */
	acc_input_dev = input_allocate_device();
	if (acc_input_dev == NULL)
		goto iRet_acc_input_free_device;

	gyro_input_dev = input_allocate_device();
	if (gyro_input_dev == NULL)
		goto iRet_gyro_input_free_device;

	mag_input_dev = input_allocate_device();
	if (mag_input_dev == NULL)
		goto iRet_mag_input_free_device;

	input_set_drvdata(acc_input_dev, sensdata);
	input_set_drvdata(gyro_input_dev, sensdata);
	input_set_drvdata(mag_input_dev, sensdata);

	acc_input_dev->name  = "accelerometer_sensor";
	gyro_input_dev->name = "gyro_sensor";
	mag_input_dev->name  = "magnetic_sensor";

	input_set_capability(acc_input_dev, EV_ABS, ABS_X);
	input_set_capability(acc_input_dev, EV_ABS, ABS_Y);
	input_set_capability(acc_input_dev, EV_ABS, ABS_Z);
	input_set_abs_params(acc_input_dev, ABS_X, INT_MIN, INT_MAX, 0, 0);
	input_set_abs_params(acc_input_dev, ABS_Y, INT_MIN, INT_MAX, 0, 0);
	input_set_abs_params(acc_input_dev, ABS_Z, INT_MIN, INT_MAX, 0, 0);

	input_set_capability(gyro_input_dev, EV_ABS, ABS_X);
	input_set_capability(gyro_input_dev, EV_ABS, ABS_Y);
	input_set_capability(gyro_input_dev, EV_ABS, ABS_Z);
	input_set_abs_params(gyro_input_dev, ABS_X, INT_MIN, INT_MAX, 0, 0);
	input_set_abs_params(gyro_input_dev, ABS_Y, INT_MIN, INT_MAX, 0, 0);
	input_set_abs_params(gyro_input_dev, ABS_Z, INT_MIN, INT_MAX, 0, 0);

	input_set_capability(mag_input_dev, EV_ABS, ABS_X);
	input_set_capability(mag_input_dev, EV_ABS, ABS_Y);
	input_set_capability(mag_input_dev, EV_ABS, ABS_Z);
	input_set_abs_params(mag_input_dev, ABS_X, INT_MIN, INT_MAX, 0, 0);
	input_set_abs_params(mag_input_dev, ABS_Y, INT_MIN, INT_MAX, 0, 0);
	input_set_abs_params(mag_input_dev, ABS_Z, INT_MIN, INT_MAX, 0, 0);

	/* register input_device */
	iRet = input_register_device(acc_input_dev);
	if (iRet < 0)
		goto iRet_acc_input_unreg_device;

	iRet = input_register_device(gyro_input_dev);
	if (iRet < 0) {
		input_free_device(gyro_input_dev);
		input_free_device(mag_input_dev);
		goto iRet_gyro_input_unreg_device;
	}

	iRet = input_register_device(mag_input_dev);
	if (iRet < 0) {
		input_free_device(mag_input_dev);
		goto iRet_mag_input_unreg_device;
	}

	sensdata->acc_input_dev  = acc_input_dev;
	sensdata->gyro_input_dev = gyro_input_dev;
	sensdata->mag_input_dev  = mag_input_dev;

	dev_dbg(es705->dev,
	    "[SPI]: %s - Done creating input dev nodes for sensors",
	    __func__);

	return SUCCESS;

iRet_mag_input_unreg_device:
	input_unregister_device(gyro_input_dev);
iRet_gyro_input_unreg_device:
	input_unregister_device(acc_input_dev);
	return -EIO;
iRet_acc_input_unreg_device:
	pr_err("[SPI]: %s - could not register input device\n", __func__);
	input_free_device(mag_input_dev);
iRet_mag_input_free_device:
	input_free_device(gyro_input_dev);
iRet_gyro_input_free_device:
	input_free_device(acc_input_dev);
iRet_acc_input_free_device:
	pr_err("[SPI]: %s - could not allocate input device\n", __func__);
iRet_work_queue_err:
	kfree(sensdata);
	return -EIO;
}

void es705_sensor_hub_remove_data_driver(struct es705_priv *es705)
{
	struct spi_data *data = es705->sensData;
	input_unregister_device(data->acc_input_dev);
	input_unregister_device(data->gyro_input_dev);
	input_unregister_device(data->mag_input_dev);
	kfree(data);
	cancel_work_sync(&es705->sensor_event_work);
	destroy_workqueue(es705->spi_workq);
}


static void report_accel_data(struct spi_data *data, struct sensor_sample *acc)
{
	/*
	 * Read Accel data or it is passed as argument
	 * Publish to input event
	 */
	input_report_abs(data->acc_input_dev, ABS_X, acc->x);
	input_report_abs(data->acc_input_dev, ABS_Y, acc->y);
	input_report_abs(data->acc_input_dev, ABS_Z, acc->z);
	input_sync(data->acc_input_dev);

}

static void report_gyro_data(struct spi_data *data, struct sensor_sample *gyr)
{
	/* Publish to input event */
	input_report_abs(data->gyro_input_dev, ABS_X, gyr->x);
	input_report_abs(data->gyro_input_dev, ABS_Y, gyr->y);
	input_report_abs(data->gyro_input_dev, ABS_Z, gyr->z);
	input_sync(data->gyro_input_dev);

}

static void report_mag_data(struct spi_data *data, struct sensor_sample *mag)
{
	/* Publish to input event */
	input_report_abs(data->mag_input_dev, ABS_X, mag->x);
	input_report_abs(data->mag_input_dev, ABS_Y, mag->y);
	input_report_abs(data->mag_input_dev, ABS_Z, mag->z);
	input_sync(data->mag_input_dev);

}


MODULE_DESCRIPTION("ASoC ES705 Sensor Hub driver");
MODULE_AUTHOR("Rajiv Verma <rverma@sensorplatforms.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:es705-codec");
