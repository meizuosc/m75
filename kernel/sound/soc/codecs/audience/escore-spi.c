/*
 * escore-spi.c  --  SPI interface for Audience earSmart chips
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
#include <linux/spi/spi.h>
#include "escore.h"
#include "escore-spi.h"
static struct spi_device *escore_spi;
struct spi_transfer tr[2];

static int escore_spi_read(struct escore_priv *escore, void *buf, int len)
{
	int rc;

#ifdef CONFIG_ARCH_MT6595
	u8 tx[32] = {0};
	struct spi_message m;
	tr[1].tx_buf = tx;
	tr[1].rx_buf = buf;
	tr[1].len = len;

	spi_message_init(&m);
	spi_message_add_tail(&tr[1], &m);
	rc = spi_sync(escore_spi, &m);
#else
	rc = spi_read(escore_spi, buf, len);
#endif
	if (rc < 0) {
		dev_err(&escore_spi->dev, "%s(): error %d reading SR\n",
			__func__, rc);
		return rc;
	}
	return rc;
}

static int escore_spi_write(struct escore_priv *escore,
			    const void *buf, int len)
{
	int rc;

#ifdef CONFIG_ARCH_MT6595
	u8 rx[32] = {0};
	struct spi_message m;

	tr[0].tx_buf = buf;
	tr[0].rx_buf = rx;
	tr[0].len = len;
	tr[0].bits_per_word = 0;

	spi_message_init(&m);
	spi_message_add_tail(&tr[0], &m);
	rc = spi_sync(escore_spi, &m);
#else
	rc = spi_write(escore_spi, buf, len);
#endif
	if (rc != 0)
		dev_err(&escore_spi->dev, "%s(): error %d writing SR\n",
				__func__, rc);
	return rc;
}

static int escore_spi_cmd(struct escore_priv *escore,
			  u32 cmd, u32 *resp)
{
	int err = 0;
	int sr = cmd & BIT(28);
	u32 rv;
	int retry = ES_MAX_RETRIES + 1;

	dev_dbg(escore->dev,
			"%s: cmd=0x%08x  sr=0x%08x\n", __func__, cmd, sr);

	cmd = cpu_to_be32(cmd);
	err = escore_spi_write(escore, &cmd, sizeof(cmd));
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
		err = escore_spi_read(escore, &rv, sizeof(rv));
		dev_dbg(escore->dev, "%s: err=%d\n", __func__, err);
		*resp = be32_to_cpu(rv);
		dev_dbg(escore->dev, "%s: *resp=0x%08x\n", __func__, *resp);
		if (err) {
			dev_dbg(escore->dev,
				"%s: escore_spi_read() failure\n", __func__);
		} else if ((*resp & ES_ILLEGAL_CMD) == ES_ILLEGAL_CMD) {
			dev_err(escore->dev, "%s: illegal command 0x%08x\n",
				__func__, cmd);
			err = -EINVAL;
			goto cmd_exit;
		} else if (*resp == ES_NOT_READY) {
			dev_dbg(escore->dev,
				"%s: escore_spi_read() not ready\n", __func__);
			err = -ETIMEDOUT;
		} else {
			goto cmd_exit;
		}

		--retry;
	} while (retry != 0 && escore->cmd_compl_mode != ES_CMD_COMP_INTR);


cmd_exit:
	return err;
}

static int escore_spi_boot_setup(struct escore_priv *escore)
{
	u32 boot_cmd = ES_SPI_BOOT_CMD;
	u32 boot_ack;
	u32 sbl_sync_cmd = ES_SPI_SBL_SYNC_CMD;
	u32 sbl_sync_ack;
	int rc;

	pr_debug("%s(): prepare for fw download\n", __func__);

	sbl_sync_cmd = cpu_to_be32(sbl_sync_cmd);
	rc = escore_spi_write(escore, &sbl_sync_cmd, sizeof(sbl_sync_cmd));
	if (rc < 0) {
		pr_err("%s(): firmware load failed sync write\n",
			__func__);
		goto escore_spi_boot_setup_failed;
	}
	usleep_range(1000, 1000);
	rc = escore_spi_read(escore, &sbl_sync_ack, sizeof(sbl_sync_ack));
	if (rc < 0) {
		pr_err("%s(): firmware load failed sync ack\n",
			__func__);
		goto escore_spi_boot_setup_failed;
	}

	sbl_sync_ack = be32_to_cpu(sbl_sync_ack);
	pr_debug("%s(): SBL SYNC ACK = 0x%08x\n", __func__, sbl_sync_ack);
	if (sbl_sync_ack != ES_SPI_SBL_SYNC_ACK) {
		pr_err("%s(): boot ack pattern fail\n", __func__);
		rc = -EIO;
		goto escore_spi_boot_setup_failed;
	}
	pr_debug("%s(): write ES_BOOT_CMD = 0x%04x\n", __func__, boot_cmd);
	boot_cmd = cpu_to_be32(boot_cmd);
	rc = escore_spi_write(escore, &boot_cmd, sizeof(boot_cmd));
	if (rc < 0) {
		pr_err("%s(): firmware load failed boot write\n",
			__func__);
		goto escore_spi_boot_setup_failed;
	}

	usleep_range(1000, 1000);
	rc = escore_spi_read(escore, &boot_ack, sizeof(boot_ack));
	if (rc < 0) {
		pr_err("%s(): firmware load failed boot ack\n",
			__func__);
		goto escore_spi_boot_setup_failed;
	}

	boot_ack = be32_to_cpu(boot_ack);
	pr_debug("%s(): BOOT ACK = 0x%08x\n", __func__, boot_ack);

	if (boot_ack != ES_SPI_BOOT_ACK) {
		pr_err("%s(): boot ack pattern fail\n", __func__);
		rc = -EIO;
		goto escore_spi_boot_setup_failed;
	}
escore_spi_boot_setup_failed:
	return rc;
}

int escore_spi_boot_finish(struct escore_priv *escore)
{
	u32 sync_cmd = (ES_SYNC_CMD << 16) | ES_SYNC_POLLING;
	u32 sync_ack;
	int rc = 0;
	int sync_retry = ES_SYNC_MAX_RETRY;

	/* sometimes earSmart chip sends success in second sync command */
	do {
		pr_debug("%s(): write ES_SYNC_CMD = 0x%08x\n",
				__func__, sync_cmd);
		rc = escore_spi_cmd(escore, sync_cmd, &sync_ack);
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

static void escore_spi_setup_pri_intf(struct escore_priv *escore)
{
	escore->bus.ops.read = escore_spi_read;
	escore->bus.ops.write = escore_spi_write;
	escore->bus.ops.cmd = escore_spi_cmd;
	escore->streamdev = spi_streamdev;
}

static int escore_spi_setup_high_bw_intf(struct escore_priv *escore)
{
	int rc;

	escore->boot_ops.setup = escore_spi_boot_setup;
	escore->boot_ops.finish = escore_spi_boot_finish;
	escore->bus.ops.high_bw_write = escore_spi_write;
	escore->bus.ops.high_bw_read = escore_spi_read;
	escore->bus.ops.high_bw_cmd = escore_spi_cmd;
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

static int escore_spi_probe(struct spi_device *spi)
{
	int rc;
	struct escore_priv *escore = &escore_priv;

	dev_set_drvdata(&spi->dev, &escore_priv);

	escore_spi = spi;

	if (escore->pri_intf == ES_SPI_INTF)
		escore->bus.setup_prim_intf = escore_spi_setup_pri_intf;
	if (escore->high_bw_intf == ES_SPI_INTF)
		escore->bus.setup_high_bw_intf = escore_spi_setup_high_bw_intf;

	rc = escore_probe(escore, &spi->dev, ES_SPI_INTF);
	return rc;
}

struct es_stream_device spi_streamdev = {
	.read = escore_spi_read,
	.intf = ES_SPI_INTF,
};

static int escore_spi_remove(struct spi_device *spi)
{
	snd_soc_unregister_codec(&spi->dev);
	return 0;
}

int __init escore_spi_init(void)
{
	return spi_register_driver(&escore_spi_driver);
}

void __exit escore_spi_exit(void)
{
	spi_unregister_driver(&escore_spi_driver);
}

struct spi_driver escore_spi_driver = {
	.driver = {
		.name   = "earSmart-codec",
		.bus    = &spi_bus_type,
		.owner  = THIS_MODULE,
	},
	.probe  = escore_spi_probe,
	.remove = escore_spi_remove,
};

MODULE_DESCRIPTION("Audience earSmart SPI core driver");
MODULE_AUTHOR("Greg Clemson <gclemson@audience.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:earSmart-codec");
