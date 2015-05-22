/*
 * es325-i2c.c  --  Audience eS325 I2C interface
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

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/completion.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/kthread.h>
#include <linux/esxxx.h>
#include "es325.h"

int es325_i2c_read(struct es325_priv *es325, u8 *buf, int len)
{
	struct i2c_msg msg[] = {
		{
			.addr = es325->i2c_client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = buf,
		},
	};
	int rc;
	int i;

	rc = i2c_transfer(es325->i2c_client->adapter, msg, 1);
	/* Return value of i2c_transfer() is the number of executed messages.
	 * We are sending only one message at a time for execution,
	 * so we will get return value 1 on success and error on failure.
	 */

	if (rc != 1) {
		pr_err("%s(): i2c_transfer() failed, rc = %d", __func__, rc);
		return rc;
	}

	pr_debug("%s(): i2c msg:\n", __func__);
	for (i = 0; i < len; ++i)
		pr_debug("\t[%d] = 0x%02x\n", i, buf[i]);

	return 0;
}

int es325_i2c_write(struct es325_priv *es325, const u8 *buf, int len)
{
	struct i2c_msg msg[] = {
		{
			.addr = es325->i2c_client->addr,
			.flags = 0,
			.len = len,
			.buf = (u8 *)buf,
		},
	};
	int rc;

	rc = i2c_transfer(es325->i2c_client->adapter, msg, 1);
	if (rc != 1) {
		pr_err("%s(): i2c_transfer() failed, rc = %d",
		__func__, rc);
		return rc;
	}

	return 0;
}

static int es325_i2c_probe(struct i2c_client *i2c,
			   const struct i2c_device_id *id)
{
	struct esxxx_platform_data *pdata = i2c->dev.platform_data;
	const char *filename = "audience-es325-fw.bin";
	int rc;

	dev_dbg(&i2c->dev, "%s() i2c->name = %s\n", __func__, i2c->name);

	es325_priv.i2c_client = i2c;

	if (pdata == NULL) {
		dev_err(&i2c->dev, "%s(): pdata is NULL", __func__);
		rc = -EIO;
		goto pdata_error;
	}

	i2c_set_clientdata(i2c, &es325_priv);

	es325_priv.intf = ES325_I2C_INTF;
	es325_priv.dev_read = es325_i2c_read;
	es325_priv.dev_write = es325_i2c_write;
	rc = es325_core_probe(&i2c->dev);
	if (rc) {
		dev_err(&i2c->dev, "%s(): es325_core_probe() failed %d\n",
			__func__, rc);
		goto es325_core_probe_error;
	}

	rc = request_firmware((const struct firmware **)&es325_priv.fw,
			      filename, &i2c->dev);
	if (rc) {
		dev_err(&i2c->dev, "%s(): request_firmware(%s) failed %d\n",
			__func__, filename, rc);
		goto request_firmware_error;
	}
	rc = es325_bootup(&es325_priv);
	if (rc) {
		dev_err(&i2c->dev, "%s(): es325_bootup failed %d\n",
			__func__, rc);
		goto bootup_error;
	}
	release_firmware(es325_priv.fw);

	rc = snd_soc_register_codec(&i2c->dev, &soc_codec_dev_es325, es325_dai,
				    ES325_NUM_CODEC_DAIS);
	dev_dbg(&i2c->dev, "%s(): rc = snd_soc_regsiter_codec() = %d\n", __func__, rc);

	return rc;

bootup_error:
request_firmware_error:
es325_core_probe_error:
pdata_error:
	dev_dbg(&i2c->dev, "%s(): exit with error\n", __func__);
	return rc;
}

static int es325_i2c_remove(struct i2c_client *i2c)
{
	struct esxxx_platform_data *pdata = i2c->dev.platform_data;

	gpio_free(pdata->reset_gpio);
	gpio_free(pdata->wakeup_gpio);
	gpio_free(pdata->gpioa_gpio);

	snd_soc_unregister_codec(&i2c->dev);

	kfree(i2c_get_clientdata(i2c));

	return 0;
}

static int es325_i2c_suspend(struct device *dev)
{
	return 0;
}

static int es325_i2c_resume(struct device *dev)
{
	return 0;
}

static int es325_i2c_runtime_suspend(struct device *dev)
{
	return 0;
}

static int es325_i2c_runtime_resume(struct device *dev)
{
	return 0;
}

static int es325_i2c_runtime_idle(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops es325_i2c_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(
		es325_i2c_suspend,
		es325_i2c_resume
	)
	SET_RUNTIME_PM_OPS(
		es325_i2c_runtime_suspend,
		es325_i2c_runtime_resume,
		es325_i2c_runtime_idle
	)
};

static const struct i2c_device_id es325_i2c_id[] = {
	{ "es325", 0},
	{ }
};
MODULE_DEVICE_TABLE(i2c, es325_i2c_id);

struct i2c_driver es325_i2c_driver = {
	.driver = {
		.name = "es325-codec",
		.owner = THIS_MODULE,
		.pm = &es325_i2c_dev_pm_ops,
	},
	.probe = es325_i2c_probe,
	.remove = es325_i2c_remove,
	.id_table = es325_i2c_id,
};

MODULE_DESCRIPTION("ASoC ES325 driver");
MODULE_AUTHOR("Greg Clemson <gclemson@audience.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:es325-codec");
