/*
 * es705-slim.c  --  Audience eS705 ALSA SoC Audio driver
 *
 * Copyright 2011 Audience, Inc.
 *
 * Author: Greg Clemson <gclemson@audience.com>
 *
 * Code Updates:
 *	Genisim Tsilker <gtsilker@audience.com>
 *              - Code refactoring
 *              - FW download functions update
 *              - Rewrite esxxx SLIMBus write / read functions
 *              - Add write_then_read function
 *              - Unify log messages format
 *              - Add detection eS70x (704 / 705)
 *                when using SLIMBus v2 (QDSP) and DTS
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * The specifics of the behavior of the ES705 are described in
 * "ES705 Audio Processor - Advanced API Guide - Ver 0.34"
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/completion.h>
#include <linux/i2c.h>
#include <linux/slimbus/slimbus.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/kthread.h>
#include <linux/device.h>
#include <linux/pm_runtime.h>
#include <linux/mutex.h>
#include <linux/slimbus/slimbus.h>
#include <linux/esxxx.h>
#include <linux/of_gpio.h>
#include <linux/of.h>

#include "es705.h"
#include "es705-uart-common.h"

#define ES705_SLIM_1_PB_MAX_CHANS	2
#define ES705_SLIM_1_CAP_MAX_CHANS	2
#define ES705_SLIM_2_PB_MAX_CHANS	2
#define ES705_SLIM_2_CAP_MAX_CHANS	2
#define ES705_SLIM_3_PB_MAX_CHANS	2
#define ES705_SLIM_3_CAP_MAX_CHANS	2

#define ES705_SLIM_1_PB_OFFSET	0
#define ES705_SLIM_2_PB_OFFSET	2
#define ES705_SLIM_3_PB_OFFSET	4
#define ES705_SLIM_1_CAP_OFFSET	0
#define ES705_SLIM_2_CAP_OFFSET	2
#define ES705_SLIM_3_CAP_OFFSET	4

/*
 * Delay for receiving response can be up to 20 ms.
 * To minimize waiting time, response is checking
 * up to 20 times with 1ms delay.
 */
#define MAX_SMB_TRIALS	3
#define MAX_WRITE_THEN_READ_TRIALS	20
#define SMB_DELAY	1000

static int es705_slim_rx_port_to_ch[ES705_SLIM_RX_PORTS] = {
	152, 153, 154, 155, 134, 135
};

#ifdef CONFIG_SLIMBUS_MSM_NGD
static int es705_slim_tx_port_to_ch[ES705_SLIM_TX_PORTS] = {
	156, 157, 144, 145, 144, 145
};
#else
static int es705_slim_tx_port_to_ch[ES705_SLIM_TX_PORTS] = {
	156, 157, 138, 139, 143, 144
};
#endif

static int es705_slim_be_id[ES705_NUM_CODEC_SLIM_DAIS] = {
	ES705_SLIM_2_CAP, /* for ES705_SLIM_1_PB tx from es705 */
	ES705_SLIM_3_PB, /* for ES705_SLIM_1_CAP rx to es705 */
	ES705_SLIM_3_CAP, /* for ES705_SLIM_2_PB tx from es705 */
	-1, /* for ES705_SLIM_2_CAP */
	-1, /* for ES705_SLIM_3_PB */
	-1, /* for ES705_SLIM_3_CAP */
};

#ifdef CONFIG_SLIMBUS_MSM_NGD
static int slim_device_up_started; /* initial value by default is 0 */
#endif

static void es705_alloc_slim_rx_chan(struct slim_device *sbdev);
static void es705_alloc_slim_tx_chan(struct slim_device *sbdev);
static int es705_cfg_slim_rx(struct slim_device *sbdev, unsigned int *ch_num,
			     unsigned int ch_cnt, unsigned int rate);
static int es705_cfg_slim_tx(struct slim_device *sbdev, unsigned int *ch_num,
			     unsigned int ch_cnt, unsigned int rate);
static int es705_close_slim_rx(struct slim_device *sbdev, unsigned int *ch_num,
			       unsigned int ch_cnt);
static int es705_close_slim_tx(struct slim_device *sbdev, unsigned int *ch_num,
			       unsigned int ch_cnt);

static int es705_rx_ch_num_to_idx(int ch_num)
{
	int i;
	int idx = -1;

	for (i = 0; i < ES705_SLIM_RX_PORTS; i++) {
		if (ch_num == es705_slim_rx_port_to_ch[i]) {
			idx = i;
			break;
		}
	}

	return idx;
}

static int es705_tx_ch_num_to_idx(int ch_num)
{
	int i;
	int idx = -1;

	for (i = 0; i < ES705_SLIM_TX_PORTS; i++) {
		if (ch_num == es705_slim_tx_port_to_ch[i]) {
			idx = i;
			break;
		}
	}

	return idx;
}

/* es705 -> codec - alsa playback function */
static int es705_codec_cfg_slim_tx(struct es705_priv *es705, int dai_id)
{
	struct slim_device *sbdev = es705->gen0_client;
	int rc;

	dev_dbg(&sbdev->dev, "%s(dai_id = %d)\n", __func__, dai_id);
	/* start slim channels associated with id */
	rc = es705_cfg_slim_tx(es705->gen0_client,
			       es705->dai[DAI_INDEX(dai_id)].ch_num,
			       es705->dai[DAI_INDEX(dai_id)].ch_tot,
			       es705->dai[DAI_INDEX(dai_id)].rate);

	return rc;
}

/* es705 <- codec - alsa capture function */
static int es705_codec_cfg_slim_rx(struct es705_priv *es705, int dai_id)
{
	struct slim_device *sbdev = es705->gen0_client;
	int rc;

	dev_dbg(&sbdev->dev, "%s(dai_id = %d)\n", __func__, dai_id);
	/* start slim channels associated with id */
	rc = es705_cfg_slim_rx(es705->gen0_client,
			       es705->dai[DAI_INDEX(dai_id)].ch_num,
			       es705->dai[DAI_INDEX(dai_id)].ch_tot,
			       es705->dai[DAI_INDEX(dai_id)].rate);

	return rc;
}

/* es705 -> codec - alsa playback function */
static int es705_codec_close_slim_tx(struct es705_priv *es705, int dai_id)
{
	struct slim_device *sbdev = es705->gen0_client;
	int rc;

	dev_dbg(&sbdev->dev, "%s(dai_id = %d)\n", __func__, dai_id);
	/* close slim channels associated with id */
	rc = es705_close_slim_tx(es705->gen0_client,
				 es705->dai[DAI_INDEX(dai_id)].ch_num,
				 es705->dai[DAI_INDEX(dai_id)].ch_tot);

	return rc;
}

/* es705 <- codec - alsa capture function */
static int es705_codec_close_slim_rx(struct es705_priv *es705, int dai_id)
{
	struct slim_device *sbdev = es705->gen0_client;
	int rc;

	dev_dbg(&sbdev->dev, "%s(dai_id = %d)\n", __func__, dai_id);
	/* close slim channels associated with id */
	rc = es705_close_slim_rx(es705->gen0_client,
				 es705->dai[DAI_INDEX(dai_id)].ch_num,
				 es705->dai[DAI_INDEX(dai_id)].ch_tot);

	return rc;
}

static void es705_alloc_slim_rx_chan(struct slim_device *sbdev)
{
	struct es705_priv *es705_priv = slim_get_devicedata(sbdev);
	struct es705_slim_ch *rx = es705_priv->slim_rx;
	int i;
	int port_id;

	/* for (i = 0; i < ES705_SLIM_RX_PORTS; i++) { */
	for (i = 0; i < 6; i++) {
		port_id = i;
		rx[i].ch_num = es705_slim_rx_port_to_ch[i];
		slim_get_slaveport(sbdev->laddr, port_id, &rx[i].sph,
				   SLIM_SINK);
		slim_query_ch(sbdev, rx[i].ch_num, &rx[i].ch_h);
	}
}

static void es705_alloc_slim_tx_chan(struct slim_device *sbdev)
{
	struct es705_priv *es705_priv = slim_get_devicedata(sbdev);
	struct es705_slim_ch *tx = es705_priv->slim_tx;
	int i;
	int port_id;

	for (i = 0; i < ES705_SLIM_TX_PORTS; i++) {
		port_id = i + 10; /* ES705_SLIM_RX_PORTS; */
		tx[i].ch_num = es705_slim_tx_port_to_ch[i];
		slim_get_slaveport(sbdev->laddr, port_id, &tx[i].sph,
				   SLIM_SRC);
		slim_query_ch(sbdev, tx[i].ch_num, &tx[i].ch_h);
	}
}

static int es705_cfg_slim_rx(struct slim_device *sbdev, unsigned int *ch_num,
			     unsigned int ch_cnt, unsigned int rate)
{
	struct es705_priv *es705_priv = slim_get_devicedata(sbdev);
	struct es705_slim_ch *rx = es705_priv->slim_rx;
	u16 grph;
	u32 sph[ES705_SLIM_RX_PORTS] = {0};
	u16 ch_h[ES705_SLIM_RX_PORTS] = {0};
	struct slim_ch prop;
	int i;
	int idx;
	int rc = 0;

	dev_dbg(&sbdev->dev, "%s(): ch_cnt = %d, rate = %d\n",
		__func__, ch_cnt, rate);

	for (i = 0; i < ch_cnt; i++) {
		dev_dbg(&sbdev->dev, "%s(): ch_num = %d\n",
			__func__, ch_num[i]);
		idx = es705_rx_ch_num_to_idx(ch_num[i]);
		ch_h[i] = rx[idx].ch_h;
		sph[i] = rx[idx].sph;
	}

	prop.prot = SLIM_AUTO_ISO;
	prop.baser = SLIM_RATE_4000HZ;
	prop.dataf = SLIM_CH_DATAF_NOT_DEFINED;
	prop.auxf = SLIM_CH_AUXF_NOT_APPLICABLE;
	prop.ratem = (rate/4000);
	prop.sampleszbits = 16;

	rc = slim_define_ch(sbdev, &prop, ch_h, ch_cnt, true, &grph);
	if (rc < 0) {
		dev_err(&sbdev->dev, "%s(): slim_define_ch() failed: %d\n",
			__func__, rc);
		goto slim_define_ch_error;
	}
	for (i = 0; i < ch_cnt; i++) {
		rc = slim_connect_sink(sbdev, &sph[i], 1, ch_h[i]);
		if (rc < 0) {
			dev_err(&sbdev->dev, "%s(): slim_connect_sink() failed: %d\n",
				__func__, rc);
			goto slim_connect_sink_error;
		}
	}
	rc = slim_control_ch(sbdev, grph, SLIM_CH_ACTIVATE, true);
	if (rc < 0) {
		dev_err(&sbdev->dev,
			"%s(): slim_control_ch() failed: %d\n",
			__func__, rc);
		goto slim_control_ch_error;
	}
	for (i = 0; i < ch_cnt; i++) {
		dev_dbg(&sbdev->dev, "%s(): ch_num = %d\n",
			__func__, ch_num[i]);
		idx = es705_rx_ch_num_to_idx(ch_num[i]);
		rx[idx].grph = grph;
	}
	return rc;
slim_control_ch_error:
slim_connect_sink_error:
	es705_close_slim_rx(sbdev, ch_num, ch_cnt);
slim_define_ch_error:
	return rc;
}

static int es705_cfg_slim_tx(struct slim_device *sbdev, unsigned int *ch_num,
			     unsigned int ch_cnt, unsigned int rate)
{
	struct es705_priv *es705_priv = slim_get_devicedata(sbdev);
	struct es705_slim_ch *tx = es705_priv->slim_tx;
	u16 grph;
	u32 sph[ES705_SLIM_TX_PORTS] = {0};
	u16 ch_h[ES705_SLIM_TX_PORTS] = {0};
	struct slim_ch prop;
	int i;
	int idx;
	int rc;

	dev_dbg(&sbdev->dev, "%s(): ch_cnt = %d, rate = %d\n",
		__func__, ch_cnt, rate);

	for (i = 0; i < ch_cnt; i++) {
		dev_dbg(&sbdev->dev, "%s(): ch_num = %d\n",
			__func__, ch_num[i]);
		idx = es705_tx_ch_num_to_idx(ch_num[i]);
		ch_h[i] = tx[idx].ch_h;
		sph[i] = tx[idx].sph;
	}

	prop.prot = SLIM_AUTO_ISO;
	prop.baser = SLIM_RATE_4000HZ;
	prop.dataf = SLIM_CH_DATAF_NOT_DEFINED;
	prop.auxf = SLIM_CH_AUXF_NOT_APPLICABLE;
	prop.ratem = (rate/4000);
	prop.sampleszbits = 16;

	rc = slim_define_ch(sbdev, &prop, ch_h, ch_cnt, true, &grph);
	if (rc < 0) {
		dev_err(&sbdev->dev, "%s(): slim_define_ch() failed: %d\n",
			__func__, rc);
		goto slim_define_ch_error;
	}
	for (i = 0; i < ch_cnt; i++) {
		rc = slim_connect_src(sbdev, sph[i], ch_h[i]);
		if (rc < 0) {
			dev_err(&sbdev->dev, "%s(): slim_connect_src() failed: %d\n",
				__func__, rc);
			dev_err(&sbdev->dev, "%s(): ch_num[0] = %d\n",
				__func__, ch_num[0]);
			goto slim_connect_src_error;
		}
	}
	rc = slim_control_ch(sbdev, grph, SLIM_CH_ACTIVATE, true);
	if (rc < 0) {
		dev_err(&sbdev->dev, "%s(): slim_control_ch() failed: %d\n",
			__func__, rc);
		goto slim_control_ch_error;
	}
	for (i = 0; i < ch_cnt; i++) {
		dev_dbg(&sbdev->dev, "%s(): ch_num = %d\n",
			__func__, ch_num[i]);
		idx = es705_tx_ch_num_to_idx(ch_num[i]);
		tx[idx].grph = grph;
	}
	return rc;
slim_control_ch_error:
slim_connect_src_error:
	es705_close_slim_tx(sbdev, ch_num, ch_cnt);
slim_define_ch_error:
	return rc;
}

static int es705_close_slim_rx(struct slim_device *sbdev, unsigned int *ch_num,
			       unsigned int ch_cnt)
{
	struct es705_priv *es705_priv = slim_get_devicedata(sbdev);
	struct es705_slim_ch *rx = es705_priv->slim_rx;
	u16 grph = 0;
	u32 sph[ES705_SLIM_RX_PORTS] = {0};
	int i;
	int idx;
	int rc;

	dev_dbg(&sbdev->dev, "%s(ch_cnt = %d)\n", __func__, ch_cnt);

	for (i = 0; i < ch_cnt; i++) {
		dev_dbg(&sbdev->dev, "%s(): ch_num = %d\n",
			__func__, ch_num[i]);
		idx = es705_rx_ch_num_to_idx(ch_num[i]);
		sph[i] = rx[idx].sph;
		grph = rx[idx].grph;
	}

	rc = slim_control_ch(sbdev, grph, SLIM_CH_REMOVE, true);
	if (rc < 0) {
		dev_err(&sbdev->dev, "%s(): slim_control_ch() failed: %d\n",
			__func__, rc);
		goto slim_control_ch_error;
	}
	for (i = 0; i < ch_cnt; i++) {
		dev_dbg(&sbdev->dev, "%s(): ch_num = %d\n",
			__func__, ch_num[i]);
		idx = es705_rx_ch_num_to_idx(ch_num[i]);
		rx[idx].grph = 0;
	}
	rc = slim_disconnect_ports(sbdev, sph, ch_cnt);
	if (rc < 0) {
		dev_err(&sbdev->dev, "%s(): slim_disconnect_ports() failed: %d\n",
			__func__, rc);
	}
slim_control_ch_error:
	return rc;
}

static int es705_close_slim_tx(struct slim_device *sbdev, unsigned int *ch_num,
			       unsigned int ch_cnt)
{
	struct es705_priv *es705_priv = slim_get_devicedata(sbdev);
	struct es705_slim_ch *tx = es705_priv->slim_tx;
	u16 grph = 0;
	u32 sph[ES705_SLIM_TX_PORTS] = {0};
	int i;
	int idx;
	int rc;

	dev_dbg(&sbdev->dev, "%s(): ch_cnt = %d\n", __func__, ch_cnt);

	for (i = 0; i < ch_cnt; i++) {
		dev_dbg(&sbdev->dev, "%s(): ch_num = %d\n",
			__func__, ch_num[i]);
		idx = es705_tx_ch_num_to_idx(ch_num[i]);
		sph[i] = tx[idx].sph;
		grph = tx[idx].grph;
	}

	rc = slim_control_ch(sbdev, grph, SLIM_CH_REMOVE, true);
	if (rc < 0) {
		dev_err(&sbdev->dev, "%s(): slim_control_ch() failed: %d\n",
			__func__, rc);
		goto slim_control_ch_error;
	}
	for (i = 0; i < ch_cnt; i++) {
		dev_dbg(&sbdev->dev, "%s(): ch_num = %d\n",
			__func__, ch_num[i]);
		idx = es705_tx_ch_num_to_idx(ch_num[i]);
		tx[idx].grph = 0;
	}
	rc = slim_disconnect_ports(sbdev, sph, ch_cnt);
	if (rc < 0) {
		dev_err(&sbdev->dev, "%s(): slim_disconnect_ports() failed: %d\n",
			__func__, rc);
	}
slim_control_ch_error:
	return rc;
}

int es705_remote_cfg_slim_rx(int dai_id)
{
	struct es705_priv *es705 = &es705_priv;
	struct slim_device *sbdev = es705->gen0_client;
	int be_id;
	int rc = 0;

	dev_dbg(&sbdev->dev, "%s(): dai_id = %d\n", __func__, dai_id);

	if (dai_id != ES705_SLIM_1_PB
	    && dai_id != ES705_SLIM_2_PB)
		return rc;

	if (es705->dai[DAI_INDEX(dai_id)].ch_tot != 0) {
		/* start slim channels associated with id */
		rc = es705_cfg_slim_rx(sbdev,
				       es705->dai[DAI_INDEX(dai_id)].ch_num,
				       es705->dai[DAI_INDEX(dai_id)].ch_tot,
				       es705->dai[DAI_INDEX(dai_id)].rate);

		be_id = es705_slim_be_id[DAI_INDEX(dai_id)];
		es705->dai[DAI_INDEX(be_id)].ch_tot = es705->dai[DAI_INDEX(dai_id)].ch_tot;
		es705->dai[DAI_INDEX(be_id)].rate = es705->dai[DAI_INDEX(dai_id)].rate;
		rc = es705_codec_cfg_slim_tx(es705, be_id);

		dev_dbg(&sbdev->dev, "%s(): MDM->>>[%d][%d]ES705[%d][%d]->>>WCD channel mapping\n",
			__func__,
			es705->dai[DAI_INDEX(dai_id)].ch_num[0],
			es705->dai[DAI_INDEX(dai_id)].ch_tot == 1 ? 0 :
					es705->dai[DAI_INDEX(dai_id)].ch_num[1],
			es705->dai[DAI_INDEX(be_id)].ch_num[0],
			es705->dai[DAI_INDEX(be_id)].ch_tot == 1 ? 0 :
					es705->dai[DAI_INDEX(be_id)].ch_num[1]);
	}

	return rc;
}
EXPORT_SYMBOL_GPL(es705_remote_cfg_slim_rx);

int es705_remote_cfg_slim_tx(int dai_id)
{
	struct es705_priv *es705 = &es705_priv;
	struct slim_device *sbdev = es705->gen0_client;
	int be_id;
	int ch_cnt;
	int rc = 0;

	dev_dbg(&sbdev->dev, "%s(): dai_id = %d\n", __func__, dai_id);

	if (dai_id != ES705_SLIM_1_CAP)
		return rc;

	if (es705->dai[DAI_INDEX(dai_id)].ch_tot != 0) {
		/* start slim channels associated with id */
		ch_cnt = es705->ap_tx1_ch_cnt;
		rc = es705_cfg_slim_tx(es705->gen0_client,
				       es705->dai[DAI_INDEX(dai_id)].ch_num,
				       ch_cnt,
				       es705->dai[DAI_INDEX(dai_id)].rate);

		be_id = es705_slim_be_id[DAI_INDEX(dai_id)];
		es705->dai[DAI_INDEX(be_id)].ch_tot = es705->dai[DAI_INDEX(dai_id)].ch_tot;
		es705->dai[DAI_INDEX(be_id)].rate = es705->dai[DAI_INDEX(dai_id)].rate;
		rc = es705_codec_cfg_slim_rx(es705, be_id);

		dev_dbg(&sbdev->dev, "%s(): MDM<<<-[%d][%d]ES705[%d][%d]<<<-WCD channel mapping\n",
			__func__,
			es705->dai[DAI_INDEX(dai_id)].ch_num[0],
			es705->dai[DAI_INDEX(dai_id)].ch_tot == 1 ? 0 :
					es705->dai[DAI_INDEX(dai_id)].ch_num[1],
			es705->dai[DAI_INDEX(be_id)].ch_num[0],
			es705->dai[DAI_INDEX(be_id)].ch_tot == 1 ? 0 :
					es705->dai[DAI_INDEX(be_id)].ch_num[1]);
	}

	return rc;
}
EXPORT_SYMBOL_GPL(es705_remote_cfg_slim_tx);

int es705_remote_close_slim_rx(int dai_id)
{
	struct es705_priv *es705 = &es705_priv;
	struct slim_device *sbdev = es705->gen0_client;
	int be_id;
	int rc = 0;

	dev_dbg(&sbdev->dev, "%s(): dai_id = %d\n", __func__, dai_id);

	if (dai_id != ES705_SLIM_1_PB
	    && dai_id != ES705_SLIM_2_PB)
		return rc;

	if (es705->dai[DAI_INDEX(dai_id)].ch_tot != 0) {
		dev_dbg(&sbdev->dev, "%s(): dai_id = %d, ch_tot =%d\n",
			__func__, dai_id, es705->dai[DAI_INDEX(dai_id)].ch_tot);

		es705_close_slim_rx(es705->gen0_client,
				    es705->dai[DAI_INDEX(dai_id)].ch_num,
				    es705->dai[DAI_INDEX(dai_id)].ch_tot);

		be_id = es705_slim_be_id[DAI_INDEX(dai_id)];
		rc = es705_codec_close_slim_tx(es705, be_id);

		es705->dai[DAI_INDEX(dai_id)].ch_tot = 0;
	}

	return rc;
}
EXPORT_SYMBOL_GPL(es705_remote_close_slim_rx);

int es705_remote_close_slim_tx(int dai_id)
{
	struct es705_priv *es705 = &es705_priv;
	struct slim_device *sbdev = es705->gen0_client;
	int be_id;
	int ch_cnt;
	int rc = 0;

	dev_dbg(&sbdev->dev, "%s(): dai_id = %d\n", __func__, dai_id);

	if (dai_id != ES705_SLIM_1_CAP)
		return rc;

	if (es705->dai[DAI_INDEX(dai_id)].ch_tot != 0) {
		dev_dbg(&sbdev->dev, "%s(): dai_id = %d, ch_tot = %d\n",
			__func__, dai_id, es705->dai[DAI_INDEX(dai_id)].ch_tot);

		ch_cnt = es705->ap_tx1_ch_cnt;
		es705_close_slim_tx(es705->gen0_client,
				    es705->dai[DAI_INDEX(dai_id)].ch_num,
				    ch_cnt);

		be_id = es705_slim_be_id[DAI_INDEX(dai_id)];
		rc = es705_codec_close_slim_rx(es705, be_id);

		es705->dai[DAI_INDEX(dai_id)].ch_tot = 0;
	}

	return rc;
}
EXPORT_SYMBOL_GPL(es705_remote_close_slim_tx);

void es705_init_slim_slave(struct slim_device *sbdev)
{
	dev_dbg(&sbdev->dev, "%s()\n", __func__);

	es705_alloc_slim_rx_chan(sbdev);
	es705_alloc_slim_tx_chan(sbdev);
}

static u32 bytes2u32(char *ptr, int len)
{
	u32 ret = 0;

	if (len > sizeof(ret) || len < 0) {
		pr_debug("%s: out of range: len %d ", __func__, len);
		return 0xbadbadba;
	}
	memmove(((unsigned char *)&ret) + sizeof(ret) - len, ptr, len);
	return ret;
}

		/*
		 * NOTE:
		 * According to SLIMBus protocol master assigns
		 * available logical addresses to devices  after receiving
		 * "REPORT PRESENT" notification generated by devices
		 * at initialization or reset time and in order receiving
		 * notifications.
		 * If in the work time happen reset of one or more devices
		 * master reassigns addresses for these devices and new
		 * addresses can be different from addresses used before reset.
		 * Device driver handles device initialization and reset events
		 * and get assigned logical address makes sense after
		 * these events.
		 * But was observed "REPORT PRESENT" notifications and logical
		 * addresses reassignment after uncontrolled events -
		 *       Audience Internal BUG #16973.
		 * To avoid possible situation when logical address was changed
		 * but driver continues to use old logical address,
		 * request to getting logical address is using before each
		 * SLIMBus read and write operations.
		 */

static int es705_slim_read(struct es705_priv *es705, void *rspn, int len)
{
	int rc = 0;
	int i;
	const int sz = ES705_READ_VE_WIDTH;
	struct slim_device *sbdev = es705->gen0_client;
	char buf[sz];
	memset(buf, 0, sz);

	BUG_ON(len < 0);

	for (i = 0; i < MAX_SMB_TRIALS; i++) {
		struct slim_ele_access msg = {
			.start_offset = ES705_READ_VE_OFFSET,
			.num_bytes = sz,
			.comp = NULL,
		};

		if (rc)
			dev_dbg(&sbdev->dev, "%s(): get logical addr err %d\n",
								__func__, rc);

		rc = slim_request_val_element(sbdev, &msg, buf, sz);
		memcpy(rspn, buf, sz);
		if (!rc)
			break;

		usleep_range(SMB_DELAY, SMB_DELAY);
	}

	if (!rc)
		return rc;

	dev_dbg(&sbdev->dev,
		"%s: after %d tries returns: rc %d size %d bytes 0x%08x\n",
		__func__, i, rc, sz, bytes2u32(buf, sz));
	return rc;
}

/*
 * As long as the caller expects the most significant
 * bytes of the VE value written to be zero, this is
 * valid.
 */

/*
 * Get LA address. Will be change after
 * uncontrolled REPORT PRESENT notification.
 * Ref: NOTE: in es705_slim_read function
 */

static int es705_slim_write(struct es705_priv *es705,
				const void *buf, int len)
{
	struct slim_device *sbdev = es705->gen0_client;
	int rc = 0;
	int wr = 0;
	int i;

	BUG_ON(len < 0);

	while (wr < len) {
		const int sz = min(len - wr, ES705_WRITE_VE_WIDTH);
		if (sz != ES705_WRITE_VE_WIDTH)
			dev_dbg(&sbdev->dev,
				"%s(): smb write: size %d while VE %d\n",
					__func__, sz, ES705_WRITE_VE_WIDTH);

		for (i = 0; i < MAX_SMB_TRIALS; i++) {
			struct slim_ele_access msg = {
				.start_offset = ES705_WRITE_VE_OFFSET,
				.num_bytes = sz,
				.comp = NULL,
			};

			rc = slim_change_val_element(sbdev, &msg,
						(char *)buf + wr, sz);
			if (!rc)
				break;

			usleep_range(SMB_DELAY, SMB_DELAY);
		}
		if (i == MAX_SMB_TRIALS)
			break;
		wr += sz;
	}

	if (!rc)
		return rc;

	dev_dbg(&sbdev->dev, "%s: after %d tries returns: rc %d len %d wr %d\n",
						__func__, i, rc, len, wr);
	return rc;
}

static int es705_slim_write_then_read(struct es705_priv *es705,
					const void *buf, int len,
					u32 *rspn, int match)
{
	struct slim_device *sbdev = es705->gen0_client;
	const u32 NOT_READY = 0;
	u32 response = NOT_READY;
	int rc = 0;
	int trials = 0;

	rc = es705_slim_write(es705, buf, len);
	if (rc)
		return rc;

	do {
		usleep_range(SMB_DELAY, SMB_DELAY);

		rc = es705_slim_read(es705, &response, 4);
		if (rc)
			break;

		if (response != NOT_READY) {
			if (match && *rspn != response) {
				dev_err(&sbdev->dev,
					"%s(): unexpected response 0x%08x\n",
					__func__, response);
				rc = -EIO;
			}
			*rspn = response;
			break;
		} else {
			rc = -EIO;
		}
		trials++;
	} while (trials < MAX_WRITE_THEN_READ_TRIALS);

	if (trials == MAX_WRITE_THEN_READ_TRIALS)
		dev_err(&sbdev->dev, "%s(): max trials %d\n", __func__, trials);
	return rc;
}

static int es705_slim_dev_rdb(struct es705_priv *es705, void *buf, int id)
{

	u32 cmd;
	u32 resp;
	u8 *dptr;
	int ret;
	int size;
	int new_size;
	int rdcnt = 0;

	/* This is only valid if running VS firmware */
	if (es705->mode != VOICESENSE) {
		dev_err(es705->dev, "%s(): wrong firmware loaded for RDB\n",
			__func__);
		return -EINVAL;
	}

	dptr = (u8 *)buf;
	/* Read voice sense keyword data block request. */
	cmd = (ES705_RDB_CMD << 16) | (id & 0xFFFF);
	cmd = cpu_to_le32(cmd);
	es705->dev_write(es705, (char *)&cmd, 4);

	/* Refer to "ES705 Advanced API Guide" for details of interface */
	usleep_range(10000, 10000);

	ret = es705->dev_read(es705, (char *)&resp, 4);
	if (ret < 0) {
		dev_err(es705->dev, "%s(): error sending request = %d\n",
			__func__, ret);
		goto rdb_err;
	}

	le32_to_cpus(resp);
	size = resp & 0xffff;
	dev_dbg(es705->dev, "%s(): resp=0x%08x size=%d\n",
		__func__, resp, size);
	if ((resp & 0xffff0000) != 0x802e0000) {
		dev_err(es705->dev,
			"%s(): invalid read v-s data block response = 0x%08x\n",
			__func__, resp);
		goto rdb_err;
	}

	if (size == 0) {
		dev_err(es705->dev, "%s(): read request return size of 0\n",
			__func__);
		goto rdb_err;
	}
	if (size > PARSE_BUFFER_SIZE)
		size = PARSE_BUFFER_SIZE;
	if ((size % 4) != 0) {
		new_size = size % 4;
		new_size = new_size ? 4-new_size : 0;
		new_size = size + new_size;
	} else {
		new_size = size;
	}
	/* This assumes we need to transfer the block in 4 byte
	 * increments. This is true on slimbus, but may not hold true
	 * for other buses.
	 */
	for (rdcnt = 0; rdcnt < new_size; rdcnt += 4) {
		ret = es705->dev_read(es705, dptr, 4);
		if (ret < 0) {
			dev_err(es705->dev,
				"%s(): data block ed error %d bytes ret = %d\n",
				__func__, rdcnt, ret);
			goto rdb_err;
		}
		dptr += 4;
	}

	es705->rdb_read_count = size;
	return 0;

rdb_err:
	es705->rdb_read_count = 0;
	return -EIO;
}

static int es705_slim_dev_wdb(struct es705_priv *es705, const void *buf,
			int len)
{
	/* At this point the command has been determined and is not part
	 * of the data in the buffer. Its just data. Note that we donot
	 * evaluate the contents of the data. It is up to the higher layers
	 * to insure the the codes mode of operation matchs what is being
	 * sent.
	 */
	int i, err;
	u32 newlen, padlen;
	u32 resp;
	u8 *dptr;
	u8 wdb[4];

	u32 cmd = ES705_WDB_CMD << 16;
	/* length of the sent data needs to be mod 4, padded with zeros
	 * at the end.
	 */
	padlen = len % 4;
	padlen = padlen ? 4-padlen : 0;
	newlen = len + padlen;
	if (newlen > ES705_WDB_MAX_SIZE) {
		err = -EINVAL;
		goto wdb_err;
	}
	/* The buffer is a statically allocated buffer of 4K+4bytes,
	 * we don't need to worry about doing damage here
	 */
	dptr = (char *)buf;
	for (i = 0; i < padlen; i++)
		dptr[len+i] = 0;

	cmd = cmd | (newlen & 0xFFFF);
	cmd = cpu_to_le32(cmd);
	/* Send the WDB command */
	err = es705_cmd(es705, cmd);
	if (err) {
		dev_err(es705->dev, "%s(): rdb cmd err: %d\n",
			__func__, err);
		goto wdb_err;
	}

	resp = es705->last_response;
	le32_to_cpus(resp);
	if ((resp & 0xffff0000) != (cmd & 0xffff0000)) {
		dev_err(es705->dev, "%s(): invalid write data block 0x%08x\n",
			__func__, resp);
		goto wdb_err;
	}
	/* The API requires that the subsequent writes are to be
	 * a byte stream (one byte per transport transaction), but
	 * with the SLIM bus, its a 4 byte transaction
	 */
	dptr = (char *)buf;
	for (i = newlen; i > 0; i -= 4, dptr += 4) {
		wdb[0] = dptr[3];
		wdb[1] = dptr[2];
		wdb[2] = dptr[1];
		wdb[3] = dptr[0];
		err = es705->dev_write(es705, (char *)wdb, 4);
		if (err < 0) {
			dev_err(es705->dev, "%s(): v-s wdb error offset=%hu\n",
				__func__, dptr - es705->vs_keyword_param);
			goto wdb_err;
		}
	}

	/* Refer to "ES705 Advanced API Guide" for details of interface */
	usleep_range(10000, 10000);

	/* One last ACK read */
	err = es705->dev_read(es705, (char *)&resp, 4);
	if (err < 0) {
		dev_err(es705->dev, "%s(): error sending request = %d\n",
			__func__, err);
		goto wdb_err;
	}

	le32_to_cpus(resp);
	dev_dbg(es705->dev, "%s(): resp=0x%08x\n", __func__, resp);
	if (resp & 0xffff) {
		dev_err(es705->dev, "%s(): write data block error 0x%08x\n",
			__func__, resp);
		err = -EIO;
		goto wdb_err;
	}

	dev_dbg(es705->dev, "%s(): rdb cmd ack: 0x%08x\n",
				__func__, es705->last_response);
	return len;
wdb_err:
	return err;
}

int es705_slim_cmd(struct es705_priv *es705, u32 cmd, int sr, u32 *resp)
{
	int rc;

	dev_dbg(es705->dev, "%s(): cmd=0x%08x  sr=%i\n",
		__func__, cmd, sr);

	cmd = cpu_to_le32(cmd);
	if (sr) {
		rc = es705_slim_write(es705, &cmd, sizeof(cmd));
	} else {
		u32 rv;
		int match = 0;
		rc = es705_slim_write_then_read(es705, &cmd,
						sizeof(cmd), &rv, match);
		if (!rc) {
			*resp = le32_to_cpu(rv);
			dev_dbg(es705->dev, "%s(): resp=0x%08x\n",
				__func__, *resp);
		}
	}
	return rc;
}

int es705_slim_set_channel_map(struct snd_soc_dai *dai,
			       unsigned int tx_num, unsigned int *tx_slot,
			       unsigned int rx_num, unsigned int *rx_slot)
{
	struct snd_soc_codec *codec = dai->codec;
	/* local codec access */
	/* struct es705_priv *es705 = snd_soc_codec_get_drvdata(codec); */
	/* remote codec access */
	struct es705_priv *es705 = &es705_priv;
	int id = dai->id;
	int i;
	int rc = 0;

	dev_dbg(codec->dev, "%s(): dai->name = %s, dai->id = %d\n",
		__func__, dai->name, dai->id);

	if (id == ES705_SLIM_1_PB ||
	    id == ES705_SLIM_2_PB ||
	    id == ES705_SLIM_3_PB) {
		es705->dai[DAI_INDEX(id)].ch_tot = rx_num;
		es705->dai[DAI_INDEX(id)].ch_act = 0;
		for (i = 0; i < rx_num; i++)
			es705->dai[DAI_INDEX(id)].ch_num[i] = rx_slot[i];
	} else if (id == ES705_SLIM_1_CAP ||
		 id == ES705_SLIM_2_CAP ||
		 id == ES705_SLIM_3_CAP) {
		es705->dai[DAI_INDEX(id)].ch_tot = tx_num;
		es705->dai[DAI_INDEX(id)].ch_act = 0;
		for (i = 0; i < tx_num; i++) {
			es705->dai[DAI_INDEX(id)].ch_num[i] = tx_slot[i];
		}
	}

	return rc;
}
EXPORT_SYMBOL_GPL(es705_slim_set_channel_map);

int es705_slim_get_channel_map(struct snd_soc_dai *dai,
			       unsigned int *tx_num, unsigned int *tx_slot,
			       unsigned int *rx_num, unsigned int *rx_slot)
{
	struct snd_soc_codec *codec = dai->codec;
	/* local codec access */
	/* struct es705_priv *es705 = snd_soc_codec_get_drvdata(codec); */
	/* remote codec access */
	struct es705_priv *es705 = &es705_priv;
	struct es705_slim_ch *rx = es705->slim_rx;
	struct es705_slim_ch *tx = es705->slim_tx;
	int id = dai->id;
	int i;
	int rc = 0;

	dev_dbg(codec->dev, "%s(): dai->name = %s, dai->id = %d\n", __func__,
		dai->name, dai->id);

	if (id == ES705_SLIM_1_PB) {
		*rx_num = es705_dai[DAI_INDEX(id)].playback.channels_max;
		for (i = 0; i < *rx_num; i++) {
			rx_slot[i] = rx[ES705_SLIM_1_PB_OFFSET + i].ch_num;
		}
	} else if (id == ES705_SLIM_2_PB) {
		*rx_num = es705_dai[DAI_INDEX(id)].playback.channels_max;
		for (i = 0; i < *rx_num; i++) {
			rx_slot[i] = rx[ES705_SLIM_2_PB_OFFSET + i].ch_num;
		}
	} else if (id == ES705_SLIM_3_PB) {
		*rx_num = es705_dai[DAI_INDEX(id)].playback.channels_max;
		for (i = 0; i < *rx_num; i++) {
			rx_slot[i] = rx[ES705_SLIM_3_PB_OFFSET + i].ch_num;
		}
	} else if (id == ES705_SLIM_1_CAP) {
		*tx_num = es705_dai[DAI_INDEX(id)].capture.channels_max;
		for (i = 0; i < *tx_num; i++) {
			tx_slot[i] = tx[ES705_SLIM_1_CAP_OFFSET + i].ch_num;
		}
	} else if (id == ES705_SLIM_2_CAP) {
		*tx_num = es705_dai[DAI_INDEX(id)].capture.channels_max;
		for (i = 0; i < *tx_num; i++) {
			tx_slot[i] = tx[ES705_SLIM_2_CAP_OFFSET + i].ch_num;
		}
	} else if (id == ES705_SLIM_3_CAP) {
		*tx_num = es705_dai[DAI_INDEX(id)].capture.channels_max;
		for (i = 0; i < *tx_num; i++) {
			tx_slot[i] = tx[ES705_SLIM_3_CAP_OFFSET + i].ch_num;
		}
	}

	return rc;
}
EXPORT_SYMBOL_GPL(es705_slim_get_channel_map);

int es705_slim_hw_params(struct snd_pcm_substream *substream,
			 struct snd_pcm_hw_params *params,
			 struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	/* local codec access */
	/* struct es705_priv *es705 = snd_soc_codec_get_drvdata(codec); */
	/* remote codec access */
	struct es705_priv *es705 = &es705_priv;
	int id = dai->id;
	int channels;
	int rate;
	int rc = 0;

	dev_dbg(codec->dev, "%s(): dai->name = %s, dai->id = %d\n",
		__func__, dai->name, dai->id);

	channels = params_channels(params);
	switch (channels) {
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
		es705->dai[DAI_INDEX(id)].ch_tot = channels;
		break;
	default:
		dev_err(codec->dev, "%s(): unsupported number of channels, %d\n",
			__func__, channels);
		return -EINVAL;
	}
	rate = params_rate(params);
	switch (rate) {
	case 8000:
	case 16000:
	case 32000:
	case 48000:
		es705->dai[DAI_INDEX(id)].rate = rate;
		break;
	default:
		dev_err(codec->dev, "%s(): unsupported rate, %d\n",
			__func__, rate);
		return -EINVAL;
	}

	return rc;
}
EXPORT_SYMBOL_GPL(es705_slim_hw_params);

static int es705_slim_boot_setup(struct es705_priv *es705)
{
	u32 boot_cmd = ES705_BOOT_CMD;
	u32 sync_cmd = (ES705_SYNC_CMD << 16) | ES705_SYNC_POLLING;
	u32 sbl_rspn = ES705_SBL_ACK;
	u32 ack_rspn = ES705_BOOT_ACK;
	int match = 1;
	int rc;

	dev_dbg(es705->dev, "%s(): prepare for fw download\n", __func__);
	rc = es705_slim_write_then_read(es705, &sync_cmd, sizeof(sync_cmd),
			      &sbl_rspn, match);
	if (rc) {
		dev_err(es705->dev, "%s(): SYNC_SBL fail %d\n", __func__, rc);
		goto es705_slim_boot_setup_failed;
	}

	es705->mode = SBL;

	rc = es705_slim_write_then_read(es705, &boot_cmd, sizeof(boot_cmd),
			&ack_rspn, match);
	if (rc)
		dev_err(es705->dev, "%s(): BOOT_CMD fail rc %d\n",
							__func__, rc);

es705_slim_boot_setup_failed:
	return rc;
}

static int es705_slim_boot_finish(struct es705_priv *es705)
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
	rc = es705_slim_write_then_read(es705, &sync_cmd, sizeof(sync_cmd),
			&sync_rspn, match);
	if (rc)
		dev_err(es705->dev, "%s(): SYNC fail %d\n", __func__, rc);
	return rc;
}

#define ES705_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |\
			SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 |\
			SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |\
			SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000)
#define ES705_SLIMBUS_RATES (SNDRV_PCM_RATE_48000)

#define ES705_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S16_BE |\
			SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_S20_3BE |\
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S24_BE |\
			SNDRV_PCM_FMTBIT_S32_LE | SNDRV_PCM_FMTBIT_S32_BE)
#define ES705_SLIMBUS_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
			SNDRV_PCM_FMTBIT_S16_BE)

struct snd_soc_dai_ops es705_slim_port_dai_ops = {
	.set_fmt	= NULL,
	.set_channel_map	= es705_slim_set_channel_map,
	.get_channel_map	= es705_slim_get_channel_map,
	.set_tristate	= NULL,
	.digital_mute	= NULL,
	.startup	= NULL,
	.shutdown	= NULL,
	.hw_params	= es705_slim_hw_params,
	.hw_free	= NULL,
	.prepare	= NULL,
	.trigger	= NULL,
};

static struct esxxx_platform_data *es705_populate_dt_pdata(struct device *dev)
{
	struct esxxx_platform_data *pdata;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "%s(): platform data allocation failed\n",
			__func__);
		goto err;
	}
	pdata->reset_gpio = of_get_named_gpio(dev->of_node,
					      "reset-gpio", 0);
	if (pdata->reset_gpio < 0) {
		dev_err(dev, "%s(): get reset_gpio failed\n", __func__);
		goto dt_populate_err;
	}
	dev_dbg(dev, "%s(): reset gpio %d\n", __func__, pdata->reset_gpio);

#ifdef CONFIG_SND_SOC_ES705_EXTCLK_OVER_GPIO
	pdata->extclk_gpio = of_get_named_gpio(dev->of_node,
					      "extclk-gpio", 0);
	if (pdata->extclk_gpio < 0) {
		dev_err(dev, "%s(): get extclk_gpio failed\n", __func__);
		goto dt_populate_err;
	}
	dev_dbg(dev, "%s(): extclk gpio %d\n", __func__, pdata->extclk_gpio);
#endif
	pdata->gpioa_gpio = of_get_named_gpio(dev->of_node,
					      "gpioa-gpio", 0);
	if (pdata->gpioa_gpio < 0) {
		dev_err(dev, "%s(): get gpioa_gpio failed\n", __func__);
		goto dt_populate_err;
	}
	dev_dbg(dev, "%s(): gpioa gpio %d\n", __func__, pdata->gpioa_gpio);

	pdata->gpiob_gpio = of_get_named_gpio(dev->of_node,
					      "gpiob-gpio", 0);
	if (pdata->gpiob_gpio < 0) {
		dev_err(dev, "%s(): get gpiob_gpio failed\n", __func__);
		goto dt_populate_err;
	}
	dev_dbg(dev, "%s(): gpiob gpio %d\n", __func__, pdata->gpiob_gpio);

	pdata->wakeup_gpio = of_get_named_gpio(dev->of_node,
					     "wakeup-gpio", 0);
	if (pdata->wakeup_gpio < 0) {
		dev_err(dev, "%s(): get wakeup_gpio failed\n", __func__);
		pdata->wakeup_gpio = -1;
	}
	dev_dbg(dev, "%s(): wakeup gpio %d\n", __func__, pdata->wakeup_gpio);

	return pdata;
dt_populate_err:
	devm_kfree(dev, pdata);
err:
	return NULL;
}

#define CODEC_ID "es705-codec"
#define CODEC_GEN0_ID "es705-codec-gen0"
#define CODEC_INTF_ID "es705-codec-intf"

#define CODEC_ID_ES704 "es704-codec"
#define CODEC_GEN0_ID_ES704 "es704-codec-gen0"
#define CODEC_INTF_ID_ES704 "es704-codec-ifd"

#define CODEC_INTF_PROP "slim-ifd"
#define CODEC_ELEMENTAL_ADDR "slim-ifd-elemental-addr"

static int es705_slim_probe(struct slim_device *sbdev);
static int es705_slim_device_up(struct slim_device *sbdev)
{
	int rc;

	dev_info(&sbdev->dev, "%s: name=%s laddr=%d\n",
		 __func__, sbdev->name, sbdev->laddr);
	/* Start the firmware download in the workqueue context. */
	slim_get_devicedata(sbdev);
	if (strncmp(sbdev->name, CODEC_GEN0_ID,
			strnlen(CODEC_GEN0_ID, SLIMBUS_NAME_SIZE)) &&
		strncmp(sbdev->name, CODEC_GEN0_ID_ES704,
			strnlen(CODEC_GEN0_ID_ES704, SLIMBUS_NAME_SIZE)))
		return 0;

#ifdef CONFIG_SLIMBUS_MSM_NGD
	slim_device_up_started = 1;
	es705_slim_probe(sbdev);
#endif
	rc = fw_download(&es705_priv);
	BUG_ON(rc != 0);

	return rc;
}

#ifndef CONFIG_SLIMBUS_MSM_NGD
static int es705_fw_thread(void *priv)
{
	struct es705_priv *es705 = (struct es705_priv  *)priv;

	do {
		slim_get_logical_addr(es705->gen0_client,
				      es705->gen0_client->e_addr,
				      6, &(es705->gen0_client->laddr));
		usleep_range(1000, 2000);
	} while (es705->gen0_client->laddr == 0xf0);
	dev_dbg(&es705->gen0_client->dev, "%s(): gen0_client LA = %d\n",
		__func__, es705->gen0_client->laddr);
	do {
		slim_get_logical_addr(es705->intf_client,
				      es705->intf_client->e_addr,
				      6, &(es705->intf_client->laddr));
		usleep_range(1000, 2000);
	} while (es705->intf_client->laddr == 0xf0);
	dev_dbg(&es705->intf_client->dev, "%s(): intf_client LA = %d\n",
		__func__, es705->intf_client->laddr);

	es705_slim_device_up(es705->gen0_client);
	return 0;
}
#endif

static int es705_dt_parse_slim_interface_dev_info(struct device *dev,
						struct slim_device *slim_ifd)
{
	int rc = 0;
	struct property *prop;

	rc = of_property_read_string(dev->of_node,
			CODEC_INTF_PROP, &slim_ifd->name);
	if (rc < 0) {
		dev_err(dev, "%s(): %s fail for %s",
			__func__, dev->of_node->full_name,
			slim_ifd->name);
		return rc;
	}

	prop = of_find_property(dev->of_node,
			CODEC_ELEMENTAL_ADDR, NULL);
	if (!prop)
		rc = -EINVAL;
	if (!prop->value)
		rc = -ENODATA;
	if (prop->length < sizeof(slim_ifd->e_addr))
		rc = -EOVERFLOW;

	if (!rc) {
		memcpy(slim_ifd->e_addr, prop->value,
					sizeof(slim_ifd->e_addr));
		dev_info(dev, "%s(): slim ifd addr 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
			__func__, slim_ifd->e_addr[0], slim_ifd->e_addr[1],
			slim_ifd->e_addr[2], slim_ifd->e_addr[3],
			slim_ifd->e_addr[4], slim_ifd->e_addr[5]);
	}
	return rc;
}

static int es705_slim_probe_dts(struct slim_device *sbdev)
{
	static struct slim_device intf_device;
	struct esxxx_platform_data *pdata;
	int rc = 0;

	dev_info(&sbdev->dev, "%s(): Platform data from device tree\n",
			__func__);
	dev_info(&sbdev->dev, "%s(): slim gen0 addr 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
		__func__, sbdev->e_addr[0], sbdev->e_addr[1],
		sbdev->e_addr[2], sbdev->e_addr[3],
		sbdev->e_addr[4], sbdev->e_addr[5]);
	pdata = es705_populate_dt_pdata(&sbdev->dev);
	if (pdata == NULL) {
		dev_err(&sbdev->dev, "%s(): pdata is NULL", __func__);
		rc = -EIO;
		goto pdata_error;
	}

	sbdev->dev.platform_data = pdata;
	es705_priv.gen0_client = sbdev;

	rc = es705_dt_parse_slim_interface_dev_info(&sbdev->dev,
							&intf_device);

	if (rc) {
		dev_err(&sbdev->dev, "%s(): Error, parsing slim interface\n",
			__func__);
		devm_kfree(&sbdev->dev, pdata);
		rc = -EINVAL;
		goto pdata_error;
	}
	es705_priv.intf_client = &intf_device;
pdata_error:
	return rc;
}

static void es705_slim_probe_nodts(struct slim_device *sbdev)
{
	if (strncmp(sbdev->name, CODEC_INTF_ID,
		strnlen(CODEC_INTF_ID, SLIMBUS_NAME_SIZE)) == 0) {
		dev_dbg(&sbdev->dev, "%s(): interface device probe\n",
			__func__);
		es705_priv.intf_client = sbdev;
	}
	if (strncmp(sbdev->name, CODEC_GEN0_ID,
		strnlen(CODEC_GEN0_ID, SLIMBUS_NAME_SIZE)) == 0) {
		dev_dbg(&sbdev->dev, "%s(): generic device probe\n",
			__func__);
		es705_priv.gen0_client = sbdev;
	}
}

struct es_datablock_device slim_datablockdev = {
	.read = es705_slim_read,
	.rdb = es705_slim_dev_rdb,
	.wdb = es705_slim_dev_wdb,
	.intf = ES705_SLIM_INTF,
};

static int es705_slim_probe(struct slim_device *sbdev)
{
	int rc;

#ifndef CONFIG_SLIMBUS_MSM_NGD
	struct task_struct *thread = NULL;
#endif

	dev_dbg(&sbdev->dev, "%s(): sbdev->name = %s\n", __func__, sbdev->name);
	if (sbdev->dev.of_node) {
		/*
		 * Enable handling when probe is calling
		 * by device wakeup function only
		 */
#ifdef CONFIG_SLIMBUS_MSM_NGD
		if (!slim_device_up_started)
			return 0;
		slim_device_up_started = 0;
#endif
		rc = es705_slim_probe_dts(sbdev);
		if (rc)
			goto es705_core_probe_error;
	} else {
		es705_slim_probe_nodts(sbdev);
	}

	if (es705_priv.intf_client == NULL ||
	    es705_priv.gen0_client == NULL) {
		dev_dbg(&sbdev->dev, "%s(): incomplete initialization\n",
			__func__);
		return 0;
	}

	slim_set_clientdata(sbdev, &es705_priv);

	es705_priv.intf = ES705_SLIM_INTF;
	es705_priv.dev_read = es705_slim_read;
	es705_priv.dev_write = es705_slim_write;
	es705_priv.dev_write_then_read = es705_slim_write_then_read;

	es705_priv.dev_rdb = es705_slim_dev_rdb;
	es705_priv.dev_wdb = es705_slim_dev_wdb;

	es705_priv.streamdev = uart_streamdev;

	/* This datablock device may be overwritten in core_probe()
	 * if different datablock interface is selected
	 */
	es705_priv.datablockdev = slim_datablockdev;

	es705_priv.boot_setup = es705_slim_boot_setup;
	es705_priv.boot_finish = es705_slim_boot_finish;

	es705_priv.cmd = es705_slim_cmd;
	es705_priv.dev = &es705_priv.gen0_client->dev;
	rc = es705_core_probe(&es705_priv.gen0_client->dev);
	if (rc) {
		dev_err(&sbdev->dev, "%s(): es705_core_probe() failed %d\n",
			__func__, rc);
		goto es705_core_probe_error;
	}

#ifndef CONFIG_SLIMBUS_MSM_NGD
	thread = kthread_run(es705_fw_thread, &es705_priv, "audience thread");
	if (IS_ERR(thread)) {
		dev_err(&sbdev->dev, "%s(): can't create es705 firmware thread = %p\n",
			__func__, thread);
		return -1;
	}
#endif

	return 0;

es705_core_probe_error:
	dev_dbg(&sbdev->dev, "%s(): exit with error\n", __func__);
	return rc;
}

static int es705_slim_remove(struct slim_device *sbdev)
{
	struct esxxx_platform_data *pdata = sbdev->dev.platform_data;

	dev_dbg(&sbdev->dev, "%s(): sbdev->name = %s\n", __func__, sbdev->name);

	gpio_free(pdata->reset_gpio);
	gpio_free(pdata->wakeup_gpio);
	gpio_free(pdata->gpioa_gpio);

	snd_soc_unregister_codec(&sbdev->dev);

	return 0;
}

static const struct slim_device_id es705_slim_id[] = {
	/* es705 */
	{ CODEC_ID, 0 },
	{ CODEC_INTF_ID, 0 },
	{ CODEC_GEN0_ID, 0 },
	/* es704 */
	{ CODEC_ID_ES704, 0 },
	{ CODEC_INTF_ID_ES704, 0 },
	{ CODEC_GEN0_ID_ES704, 0 },
	{  }
};
MODULE_DEVICE_TABLE(slim, es705_slim_id);

struct slim_driver es705_slim_driver = {
	.driver = {
		.name = CODEC_ID,
		.owner = THIS_MODULE,
	},
	.probe = es705_slim_probe,
	.remove = es705_slim_remove,
#ifdef CONFIG_SLIMBUS_MSM_NGD
	.device_up = es705_slim_device_up,
#endif
	.id_table = es705_slim_id,
};

int es705_bus_init(struct es705_priv *es705)
{
	int rc;
	rc = slim_driver_register(&es705_slim_driver);
	if (!rc)
		dev_dbg(es705->dev, "%s(): ES705 registered as SLIMBus",
			__func__);
	return rc;
}

MODULE_DESCRIPTION("ASoC ES705 driver");
MODULE_AUTHOR("Greg Clemson <gclemson@audience.com>");
MODULE_AUTHOR("Genisim Tsilker <gtsilker@audience.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:es705-codec");
