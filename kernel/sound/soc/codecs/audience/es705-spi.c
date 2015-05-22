/*
 * es705-spi.c  --  Audience eS705 SPI interface
 *
 * Copyright 2011 Audience, Inc.
 *
 * Author: Hemal Meghpara <hmeghpara@audience.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/firmware.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>

#include "es705.h"
#include "es705-spi.h"


static int es705_spi_read(struct es705_priv *es705, void *buf, int len)
{
	struct spi_device *spi = es705->spi_client;
	int rc;

	rc = spi_read(spi, buf, len);

	if (rc < 0) {
		dev_err(&spi->dev, "%s(): error %d reading SR\n",
			__func__, rc);
		return rc;
	}
	return rc;
}

static int es705_spi_write(struct es705_priv *es705, const void *buf, int len)
{
	struct spi_device *spi = es705->spi_client;
	int rc;

	rc = spi_write(spi, buf, len);
	if (rc != 0)
		dev_err(&spi->dev, "%s(): error %d writing SR\n",
				__func__, rc);
	return rc;
}

static int es705_spi_write_then_read(struct es705_priv *es705,
				      const void *buf, int len,
				      u32 *rspn, int match)
{
	int rc;
	rc = es705_spi_write(es705, buf, len);
	if (!rc)
		rc = es705_spi_read(es705, rspn, match);
	return rc;
}

static int es705_spi_cmd(struct es705_priv *es705, u32 cmd, int sr, u32 *resp)
{
	int err;
	u32 rv;
	int read_retries = 15;

	dev_dbg(es705->dev, "%s(): cmd=0x%08x  sr=%i\n", __func__, cmd, sr);

	cmd = cpu_to_be32(cmd);
	err = es705_spi_write(es705, &cmd, sizeof(cmd));
	if (err || sr)
		return err;

	usleep_range(20000, 20500);

	do {
		err = es705_spi_read(es705, &rv, sizeof(rv));
		dev_dbg(es705->dev, "%s(): spi read err = %d, rv = 0x%08x\n",
			__func__, err, be32_to_cpu(rv));

		if (err < 0)
			break;

		usleep_range(1000, 1050);
	} while (rv == 0 && read_retries--);

	if (rv == 0 && read_retries < 0)
		err = -ETIMEDOUT;

	if (!err)
		*resp = be32_to_cpu(rv);
	dev_dbg(es705->dev, "%s(): resp=0x%08x\n", __func__, *resp);
	return err;
}

#define ES705_SPI_BOOT_MAX_RETRY 15

static int es705_spi_boot_setup(struct es705_priv *es705)
{
	u32 boot_cmd = ES705_BOOT_CMD;
	u32 sync_cmd = (ES705_SYNC_CMD << 16) | ES705_SYNC_POLLING;
	u32 sbl_rspn = ES705_SBL_ACK;
	u32 ack_rspn = ES705_BOOT_ACK;
	int match = 1;
	int rc;

	dev_dbg(es705->dev, "%s(): prepare for fw download\n", __func__);
	rc = es705_spi_write_then_read(es705, &sync_cmd, sizeof(sync_cmd),
			      &sbl_rspn, match);
	if (rc) {
		dev_err(es705->dev, "%s(): SYNC_SBL fail\n", __func__);
		goto es705_spi_boot_setup_failed;
	}

	es705->mode = SBL;

	rc = es705_spi_write_then_read(es705, &boot_cmd, sizeof(boot_cmd),
			&ack_rspn, match);
	if (rc)
		dev_err(es705->dev, "%s(): BOOT_CMD fail\n", __func__);

es705_spi_boot_setup_failed:
	return rc;
}

static int es705_spi_boot_finish(struct es705_priv *es705)
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
	rc = es705_spi_write_then_read(es705, &sync_cmd, sizeof(sync_cmd),
			&sync_rspn, match);
	if (rc)
		dev_err(es705->dev, "%s(): SYNC fail\n", __func__);
	return rc;
}

static int __devinit es705_spi_probe(struct spi_device *spi)
{
	int rc;

	es705_priv.spi_client = spi;
	es705_priv.intf = ES705_SPI_INTF;
	es705_priv.dev_read = es705_spi_read;
	es705_priv.dev_write = es705_spi_write;
	es705_priv.dev_write_then_read = es705_spi_write_then_read;
	es705_priv.boot_setup = es705_spi_boot_setup;
	es705_priv.boot_finish = es705_spi_boot_finish;
	es705_priv.cmd = es705_spi_cmd;
	dev_info(&spi->dev, "%s()\n", __func__);

	es705_priv.streamdev = spi_streamdev;

	rc = es705_core_probe(&spi->dev);
	if (rc) {
		dev_err(&spi->dev, "%s(): es705_core_probe() failed %d\n",
		       __func__, rc);
		goto es705_core_probe_error;
	}

	rc = es705_bootup(&es705_priv);
	if (rc) {
		dev_err(&spi->dev, "%s(): es705_bootup failed %d\n",
		       __func__, rc);
		goto bootup_error;
	}
	return rc;

bootup_error:
es705_core_probe_error:
	dev_dbg(&spi->dev, "%s(): exit with error\n", __func__);
	return rc;

}

struct es_stream_device spi_streamdev = {
	.read = es705_spi_read,
	.intf = ES705_SPI_INTF,
};

static int __devexit es705_spi_remove(struct spi_device *spi)
{
	snd_soc_unregister_codec(&spi->dev);
	return 0;
}


struct spi_driver es705_spi_driver = {
	.driver = {
		.name   = "es705_spi",
		.bus    = &spi_bus_type,
		.owner  = THIS_MODULE,
	},
	/* .id_table    = m25p_ids,*/
	.probe  = es705_spi_probe,
	.remove = __devexit_p(es705_spi_remove),
};


static int __init es705_spi_init(void)
{
	return 0;
}


static void __exit es705_spi_exit(void)
{
	spi_unregister_driver(&es705_spi_driver);
}


module_init(es705_spi_init);
module_exit(es705_spi_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Hemal Meghpara <hmeghpara@audience.com>");
MODULE_DESCRIPTION("es705 SPI driver for es705 ALSA control");
MODULE_ALIAS("platform:es705-codec");

