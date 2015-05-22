/*
 * es705-i2c.c  --  Audience eS705 I2C interface
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
#include "es705.h"
#include "es705-i2c.h"

static int es705_i2c_read(struct es705_priv *es705, void *buf, int len)
{
	struct i2c_msg msg[] = {
		{
			.addr = es705->i2c_client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = buf,
		},
	};
	int rc = 0;

	rc = i2c_transfer(es705->i2c_client->adapter, msg, 1);
	/*
	 * i2c_transfer returns number of messages executed. Since we
	 * are always sending only 1 msg, return value should be 1 for
	 * success case
	 */
	if (rc != 1) {
		pr_err("%s(): i2c_transfer() failed, rc = %d, msg_len = %d\n",
			__func__, rc, len);
		return -EIO;
	} else {
		return 0;
	}
}

#define I2C_BUF_SIZE	512
static int es705_i2c_write(struct es705_priv *es705, const void *buf, int len)
{
	struct i2c_msg msg[] = {
		{
			.addr = es705->i2c_client->addr,
			.flags = 0,
		},
	};
	int rc = 0;
	int pos = 0;

	while (pos < len) {
		msg[0].len = min(len - pos, I2C_BUF_SIZE);
		/*
		 * The function i2c_master_send() indicates we
		 * can trust the i2c bus interface not to
		 * write in this buffer.
		 */
		msg[0].buf = (void *)(buf + pos);

		rc = i2c_transfer(es705->i2c_client->adapter, msg, 1);
		if (rc != 1) {
			dev_err(es705->dev, "%s(): i2c_transfer() failed, rc = %d, msg_len = %d\n",
			__func__, rc, len);
			return -EIO;
		}

		pos += msg[0].len;
	}

	return 0;
}

static int es705_i2c_write_then_read(struct es705_priv *es705,
				      const void *buf, int len,
				      u32 *rspn, int match)
{
	int rc;
	rc = es705_i2c_write(es705, buf, len);
	if (!rc)
		rc = es705_i2c_read(es705, rspn, match);
	return rc;
}

static int es705_i2c_cmd(struct es705_priv *es705, u32 cmd, int sr, u32 *resp)
{
	int err;
	u32 rv;

	dev_dbg(es705->dev, "%s(): cmd=0x%08x  sr=%i\n", __func__, cmd, sr);

	cmd = cpu_to_be32(cmd);
	err = es705_i2c_write(es705, &cmd, sizeof(cmd));
	if (err || sr)
		return err;

	/* The response must be actively read. Maximum response time
	 * is 10ms.
	 */
	usleep_range(10000, 10500);
	err = es705_i2c_read(es705, &rv, sizeof(rv));
	if (!err)
		*resp = be32_to_cpu(rv);
	dev_dbg(es705->dev, "%s(): resp=0x%08x\n", __func__, *resp);
	return err;
}

static int es705_i2c_boot_setup(struct es705_priv *es705)
{
	u16 boot_cmd = ES705_I2C_BOOT_CMD;
	u16 boot_ack = 0;
	char msg[2];
	int rc;

	dev_dbg(es705->dev, "%s(): write ES705_BOOT_CMD = 0x%04x\n",
		__func__, boot_cmd);
	cpu_to_be16s(&boot_cmd);
	memcpy(msg, (char *)&boot_cmd, 2);
	rc = es705->dev_write(es705, msg, 2);
	if (rc < 0) {
		dev_err(es705->dev, "%s(): firmware load failed boot write\n",
			__func__);
		goto es705_boot_setup_failed;
	}
	usleep_range(1000, 1000);
	memset(msg, 0, 2);
	rc = es705->dev_read(es705, msg, 2);
	if (rc < 0) {
		dev_err(es705->dev, "%s(): firmware load failed boot ack\n",
			__func__);
		goto es705_boot_setup_failed;
	}
	memcpy((char *)&boot_ack, msg, 2);
	dev_dbg(es705->dev, "%s(): boot_ack = 0x%04x\n", __func__, boot_ack);
	if (boot_ack != ES705_I2C_BOOT_ACK) {
		dev_err(es705->dev, "%s(): firmware load failed boot ack pattern",
			__func__);
		rc = -EIO;
		goto es705_boot_setup_failed;
	}
es705_boot_setup_failed:
	return rc;
}

static int es705_i2c_boot_finish(struct es705_priv *es705)
{
	u32 sync_cmd;
	u32 sync_rspn;
	int match = 1;
	int rc = 0;

	dev_dbg(es705->dev, "%s(): finish fw download\n", __func__);
	if (es705->es705_power_state == ES705_SET_POWER_STATE_VS_OVERLAY) {
		sync_cmd = (ES705_SYNC_CMD << 16) | ES705_SYNC_INTR_RISING_EDGE;
		dev_dbg(es705->dev, "%s(): FW type : VOICESENSE\n", __func__);
	} else {
		sync_cmd = (ES705_SYNC_CMD << 16) | ES705_SYNC_POLLING;
		dev_dbg(es705->dev, "%s(): fw type : STANDARD\n", __func__);
	}
	sync_rspn = sync_cmd;

	/* Give the chip some time to become ready after firmware download. */
	msleep(20);
	/* finish es705 boot, check es705 readiness */
	rc = es705_i2c_write_then_read(es705, &sync_cmd, sizeof(sync_cmd),
			&sync_rspn, match);
	if (rc)
		dev_err(es705->dev, "%s(): SYNC fail\n", __func__);
	return rc;
}

static int es705_i2c_probe(struct i2c_client *i2c,
			   const struct i2c_device_id *id)
{
	struct esxxx_platform_data *pdata = i2c->dev.platform_data;
	int rc;

	dev_dbg(&i2c->dev, "%s(): i2c->name = %s\n", __func__, i2c->name);

	es705_priv.i2c_client = i2c;

	if (pdata == NULL) {
		dev_err(&i2c->dev, "%s(): pdata is NULL", __func__);
		rc = -EIO;
		goto pdata_error;
	}

	i2c_set_clientdata(i2c, &es705_priv);

	es705_priv.intf = ES705_I2C_INTF;
	es705_priv.dev_read = es705_i2c_read;
	es705_priv.dev_write = es705_i2c_write;
	es705_priv.dev_write_then_read = es705_i2c_write_then_read;
	es705_priv.boot_setup = es705_i2c_boot_setup;
	es705_priv.boot_finish = es705_i2c_boot_finish;
	es705_priv.cmd = es705_i2c_cmd;
	es705_priv.dev = &i2c->dev;

	es705_priv.streamdev = i2c_streamdev;

	rc = es705_core_probe(&i2c->dev);
	if (rc) {
		dev_err(&i2c->dev, "%s(): es705_core_probe() failed %d\n",
			__func__, rc);
		goto es705_core_probe_error;
	}

	rc = es705_bootup(&es705_priv);
	if (rc) {
		dev_err(&i2c->dev, "%s(): es705_bootup failed %d\n",
			__func__, rc);
		goto bootup_error;
	}

	rc = snd_soc_register_codec(&i2c->dev, &soc_codec_dev_es705, es705_dai,
				    ES705_NUM_CODEC_DAIS);
	dev_dbg(&i2c->dev, "%s(): rc = snd_soc_regsiter_codec() = %d\n", __func__, rc);

	return rc;

bootup_error:
es705_core_probe_error:
pdata_error:
	dev_dbg(&i2c->dev, "%s(): exit with error\n", __func__);
	return rc;
}

static int es705_i2c_remove(struct i2c_client *i2c)
{
	struct esxxx_platform_data *pdata = i2c->dev.platform_data;

	gpio_free(pdata->reset_gpio);
	gpio_free(pdata->wakeup_gpio);
	gpio_free(pdata->gpioa_gpio);

	snd_soc_unregister_codec(&i2c->dev);

	kfree(i2c_get_clientdata(i2c));

	return 0;
}

struct es_stream_device i2c_streamdev = {
	.read = es705_i2c_read,
	.intf = ES705_I2C_INTF,
};

int es705_i2c_init(struct es705_priv *es705)
{
	int rc;
	rc = i2c_add_driver(&es705_i2c_driver);
	if (!rc) {
		dev_dbg(es705->dev, "%s(): registered as I2C", __func__);
		es705_priv.intf = ES705_I2C_INTF;
		/*
		es705_priv.device_read = ;
		es705_priv.device_write = ;
		*/
	}
	else
		dev_err(es705->dev, "%s(): i2c_add_driver failed, rc = %d",
			__func__, rc);

	return rc;
}

static const struct i2c_device_id es705_i2c_id[] = {
	{ "es705", 0},
	{ }
};
MODULE_DEVICE_TABLE(i2c, es705_i2c_id);

struct i2c_driver es705_i2c_driver = {
	.driver = {
		.name = "es705-codec",
		.owner = THIS_MODULE,
	},
	.probe = es705_i2c_probe,
	.remove = es705_i2c_remove,
	.id_table = es705_i2c_id,
};

MODULE_DESCRIPTION("ASoC ES705 driver");
MODULE_AUTHOR("Greg Clemson <gclemson@audience.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:es705-codec");
