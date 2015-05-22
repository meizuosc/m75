/*
 * escore-i2c.c  --  I2C interface for Audience earSmart chips
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

#include "escore.h"
#include "escore-i2c.h"

static const struct i2c_client *escore_i2c;
int escore_i2c_read(struct escore_priv *escore, void *buf, int len)
{
	struct i2c_msg msg[] = {
		{
			.addr = escore_i2c->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = buf,
			.timing = 400,
			.ext_flag = 0,
		},
	};
	int rc = 0;

	rc = i2c_transfer(escore_i2c->adapter, msg, 1);
	/*
	 * i2c_transfer returns number of messages executed. Since we
	 * are always sending only 1 msg, return value should be 1 for
	 * success case
	 */
	if (rc != 1) {
		pr_err("%s(): i2c_transfer() failed, rc = %d, msg = 0x%4x, msg_len = %d\n",
			__func__, rc, *(int*)buf, len);
		return -EIO;
	} else {
		return 0;
	}
}

int escore_i2c_write(struct escore_priv *escore, const void *buf, int len)
{
	struct i2c_msg msg;
	int max_xfer_len = ES_MAX_I2C_XFER_LEN;
	int rc = 0, written = 0, xfer_len;

	msg.addr = escore_i2c->addr;
	msg.flags = 0;
	msg.timing = 400;
	msg.ext_flag = 0;

	while (written < len) {
		xfer_len = min(len - written, max_xfer_len);

		msg.len = xfer_len;
		msg.buf = (void *)(buf + written);
		rc = i2c_transfer(escore_i2c->adapter, &msg, 1);
		if (rc != 1) {
			pr_err("%s(): i2c_transfer() failed, rc:%d\n",
					__func__, rc);
			return -EIO;
		}
		written += xfer_len;
	}
	return 0;
}

int escore_i2c_cmd(struct escore_priv *escore, u32 cmd, u32 *resp)
{
	int err = 0;
	int sr = cmd & BIT(28);
	u32 rv;
	int retry = ES_MAX_RETRIES + 1;

	dev_dbg(escore->dev,
			"%s: cmd=0x%08x  sr=0x%08x\n", __func__, cmd, sr);

	cmd = cpu_to_be32(cmd);
	err = escore_i2c_write(escore, &cmd, sizeof(cmd));
	if (err || sr)
		return err;

	do {
		if (escore->cmd_compl_mode == ES_CMD_COMP_INTR) {
			pr_debug("%s(): Waiting for API interrupt. Jiffies:%lu",
					__func__, jiffies);
			err = wait_for_completion_timeout(&escore->cmd_compl,
					msecs_to_jiffies(ES_RESP_TOUT_MSEC));
			if (!err) {
				pr_debug("%s(): API Interrupt wait timeout\n",
						__func__);
				err = -ETIMEDOUT;
				break;
			}
		} else {
			usleep_range(ES_RESP_POLL_TOUT,
					ES_RESP_POLL_TOUT + 500);
		}
		err = escore_i2c_read(escore, &rv, sizeof(rv));
		dev_dbg(escore->dev, "%s: err=%d\n", __func__, err);
		*resp = be32_to_cpu(rv);
		dev_dbg(escore->dev, "%s: *resp=0x%08x\n", __func__, *resp);
		if (err) {
			dev_dbg(escore->dev,
				"%s: escore_i2c_read() failure\n", __func__);
		} else if ((*resp & ES_ILLEGAL_CMD) == ES_ILLEGAL_CMD) {
			dev_err(escore->dev, "%s: illegal command 0x%08x\n",
				__func__, cmd);
			err = -EINVAL;
			goto cmd_exit;
		} else if (*resp == ES_NOT_READY) {
			dev_dbg(escore->dev,
				"%s: escore_i2c_read() not ready\n", __func__);
			err = -ETIMEDOUT;
		} else {
			goto cmd_exit;
		}

		--retry;
	} while (retry != 0 && escore->cmd_compl_mode != ES_CMD_COMP_INTR);

cmd_exit:
	return err;
}

int escore_i2c_boot_setup(struct escore_priv *escore)
{
	u16 boot_cmd = ES_I2C_BOOT_CMD;
	u16 boot_ack = 0;
	char msg[2];
	int rc;

	pr_info("%s()\n", __func__);
	pr_info("%s(): write ES_BOOT_CMD = 0x%04x\n", __func__, boot_cmd);
	cpu_to_be16s(&boot_cmd);
	memcpy(msg, (char *)&boot_cmd, 2);
	rc = escore_i2c_write(escore, msg, 2);
	if (rc < 0) {
		pr_err("%s(): firmware load failed boot write\n", __func__);
		goto escore_bootup_failed;
	}
	usleep_range(1000, 1000);
	memset(msg, 0, 2);
	rc = escore_i2c_read(escore, msg, 2);
	if (rc < 0) {
		pr_err("%s(): firmware load failed boot ack\n", __func__);
		goto escore_bootup_failed;
	}
	memcpy((char *)&boot_ack, msg, 2);
	pr_info("%s(): boot_ack = 0x%04x\n", __func__, boot_ack);
	if (boot_ack != ES_I2C_BOOT_ACK) {
		pr_err("%s(): firmware load failed boot ack pattern", __func__);
		rc = -EIO;
		goto escore_bootup_failed;
	}

escore_bootup_failed:
	return rc;
}

int escore_i2c_boot_finish(struct escore_priv *escore)
{
	u32 sync_cmd = (ES_SYNC_CMD << 16) | ES_SYNC_POLLING;
	u32 sync_ack;
	int rc = 0;
	int sync_retry = ES_SYNC_MAX_RETRY;

	/* sometimes earSmart chip sends success in second sync command */
	do {
		pr_debug("%s(): write ES_SYNC_CMD = 0x%08x\n",
				__func__, sync_cmd);
		rc = escore_i2c_cmd(escore, sync_cmd, &sync_ack);
		if (rc < 0) {
			pr_err("%s(): firmware load failed sync write\n",
					__func__);
			continue;
		}
		pr_debug("%s(): sync_ack = 0x%08x\n", __func__, sync_ack);
		if (sync_ack != ES_SYNC_ACK) {
			pr_err("%s(): firmware load failed sync ack pattern",
					__func__);
			rc = -EIO;
		} else {
			pr_info("%s(): firmware load success", __func__);
			break;
		}
	} while (sync_retry--);

	return rc;
}

static void escore_i2c_setup_pri_intf(struct escore_priv *escore)
{
	escore->bus.ops.read = escore_i2c_read;
	escore->bus.ops.write = escore_i2c_write;
	escore->bus.ops.cmd = escore_i2c_cmd;
	escore->streamdev = es_i2c_streamdev;
}

static int escore_i2c_setup_high_bw_intf(struct escore_priv *escore)
{
	int rc;

	escore->bus.ops.high_bw_write = escore_i2c_write;
	escore->bus.ops.high_bw_read = escore_i2c_read;
	escore->bus.ops.high_bw_cmd = escore_i2c_cmd;
	escore->boot_ops.setup = escore_i2c_boot_setup;
	escore->boot_ops.finish = escore_i2c_boot_finish;
	rc = escore->probe(escore->dev);
	if (rc)
		goto out;

	rc = escore->boot_ops.bootup(escore);
	if (rc)
		goto out;

	release_firmware(escore->standard);
out:
	return rc;
}

static int escore_i2c_thread(void *data)
{
	int rc = 0;
	struct escore_priv *escore = (struct escore_priv *)data;

	rc = escore_probe(escore, &escore_i2c->dev, ES_I2C_INTF);
	if (rc)
		pr_err("%s(): escore_probe failed, rc = %d\n", __func__, rc);

	return rc;
}

static int escore_i2c_probe(struct i2c_client *i2c,
			   const struct i2c_device_id *id)
{
	struct esxxx_platform_data *pdata = i2c->dev.platform_data;
	struct escore_priv *escore = &escore_priv;
	struct task_struct *thread;
	int rc = 0;

	if (pdata == NULL) {
		dev_err(&i2c->dev, "%s(): pdata is NULL", __func__);
		return -EIO;
	}

	escore_i2c = i2c;

	if (escore->pri_intf == ES_I2C_INTF)
		escore->bus.setup_prim_intf = escore_i2c_setup_pri_intf;
	if (escore->high_bw_intf == ES_I2C_INTF)
		escore->bus.setup_high_bw_intf = escore_i2c_setup_high_bw_intf;

	i2c_set_clientdata(i2c, escore);
	thread = kthread_run(escore_i2c_thread, (void *)escore, "escore_i2c_thread");
	if (IS_ERR(thread)) {
		rc = PTR_ERR(thread);
		pr_err("%s() : create kthread failed, %d\n", __func__, rc);
	}
out:
	return rc;
}

static int escore_i2c_remove(struct i2c_client *i2c)
{
	struct esxxx_platform_data *pdata = i2c->dev.platform_data;
	if (pdata->reset_gpio != -1)
		gpio_free(pdata->reset_gpio);
	gpio_free(pdata->wakeup_gpio);
	gpio_free(pdata->gpioa_gpio);

	snd_soc_unregister_codec(&i2c->dev);

	kfree(i2c_get_clientdata(i2c));

	return 0;
}

struct es_stream_device es_i2c_streamdev = {
	.read = escore_i2c_read,
	.intf = ES_I2C_INTF,
};

int escore_i2c_init(void)
{
	int rc;

	rc = i2c_add_driver(&escore_i2c_driver);
	if (!rc)
		pr_info("%s() registered as I2C\n", __func__);

	else
		pr_err("%s(): i2c_add_driver failed, rc = %d\n", __func__, rc);

	return rc;
}

static const struct i2c_device_id escore_i2c_id[] = {
	{ "earSmart", 0},
	{ }
};
MODULE_DEVICE_TABLE(i2c, escore_i2c_id);

struct i2c_driver escore_i2c_driver = {
	.driver = {
		.name = "earSmart-codec",
		.owner = THIS_MODULE,
		.pm = &escore_pm_ops,
	},
	.probe = escore_i2c_probe,
	.remove = escore_i2c_remove,
	.id_table = escore_i2c_id,
};

MODULE_DESCRIPTION("Audience earSmart I2C core driver");
MODULE_AUTHOR("Greg Clemson <gclemson@audience.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:earSmart-codec");
