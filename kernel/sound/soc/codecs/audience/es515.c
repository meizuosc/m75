/*
 * es515.c  --  Audience eS515 ALSA SoC Audio driver
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

#include "es515.h"
#include "escore.h"
#include "escore-i2c.h"
#include "escore-i2s.h"
#include "escore-slim.h"
#include "escore-cdev.h"
#include "escore-uart.h"
#include "escore-uart-common.h"
#include "es515-access.h"
#include "es-a212-reg.h"
#include "es-d202.h"

/* codec private data TODO: move to runtime init */
struct escore_priv escore_priv = {
	.pm_state = ES_PM_ACTIVE,
	.probe = es515_core_probe,
	.slim_setup = es515_slim_setup,
	.set_streaming = es515_set_streaming,
};

struct snd_soc_dai_driver es515_dai[];

static int es515_slim_rx_port_to_ch[ES_SLIM_RX_PORTS] = {
	152, 153, 154, 155, 134, 135
};
static int es515_slim_tx_port_to_ch[ES_SLIM_TX_PORTS] = {
	156, 157, 138, 139, 143, 144
};

static struct escore_slim_dai_data dai[ES_NUM_CODEC_SLIM_DAIS];
static struct escore_slim_ch slim_rx[ES_SLIM_RX_PORTS];
static struct escore_slim_ch slim_tx[ES_SLIM_TX_PORTS];

static const u32 es515_streaming_cmds[4] = {
	0x90250200,		/* ES_SLIM_INTF */
	0x90250000,		/* ES_I2C_INTF  */
	0x90250300,		/* ES_SPI _INTF */
	0x90250100,		/* ES_UART_INTF */
};

static int es515_channel_dir(int dai_id)
{
	int dir = ES_SLIM_CH_UND;

	if (dai_id == ES_SLIM_1_PB ||
			dai_id == ES_SLIM_2_PB ||
			dai_id == ES_SLIM_3_PB) {
		dir = ES_SLIM_CH_RX;
	} else if (dai_id == ES_SLIM_1_CAP ||
			dai_id == ES_SLIM_2_CAP ||
			dai_id == ES_SLIM_3_CAP)  {
		dir = ES_SLIM_CH_TX;
	}

	return dir;
}
static ssize_t es515_route_status_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	int ret = 0;
	unsigned int value = 0;
	char *status_name = "Route Status";

	value = escore_read(NULL, ES_CHANGE_STATUS);

	ret = snprintf(buf, PAGE_SIZE,
		       "%s=0x%04x\n",
		       status_name, value);

	return ret;
}

static DEVICE_ATTR(route_status, 0444, es515_route_status_show, NULL);
/* /sys/devices/platform/msm_slim_ctrl.1/es515-codec-gen0/route_status */

static ssize_t es515_route_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct escore_priv *es515 = &escore_priv;

	pr_info("%s(): route=%ld\n", __func__, es515->internal_route_num);
	return snprintf(buf, PAGE_SIZE, "route=%ld\n",
			es515->internal_route_num);
}

static DEVICE_ATTR(route, 0444, es515_route_show, NULL);
/* /sys/devices/platform/msm_slim_ctrl.1/es515-codec-gen0/route */

#define SIZE_OF_VERBUF 256
/* TODO: fix for new read/write. use es515_read() instead of BUS ops */
static ssize_t es515_fw_version_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	int idx = 0;
	unsigned int value;
	char versionbuffer[SIZE_OF_VERBUF];
	char *verbuf = versionbuffer;

	memset(verbuf, 0, SIZE_OF_VERBUF);

	value = escore_read(NULL, ES_FW_FIRST_CHAR);
	*verbuf++ = (value & 0x00ff);
	for (idx = 0; idx < (SIZE_OF_VERBUF-2); idx++) {
		value = escore_read(NULL, ES_FW_NEXT_CHAR);
		*verbuf++ = (value & 0x00ff);
	}
	/* Null terminate the string*/
	*verbuf = '\0';
	pr_info("Audience fw ver %s\n", versionbuffer);
	return snprintf(buf, PAGE_SIZE, "FW Version = %s\n", versionbuffer);
}

static DEVICE_ATTR(fw_version, 0444, es515_fw_version_show, NULL);
/* /sys/devices/platform/msm_slim_ctrl.1/es515-codec-gen0/fw_version */

static ssize_t es515_clock_on_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	int ret = 0;

	return ret;
}

static DEVICE_ATTR(clock_on, 0444, es515_clock_on_show, NULL);
/* /sys/devices/platform/msm_slim_ctrl.1/es515-codec-gen0/clock_on */

static ssize_t es515_ping_status_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct escore_priv *es515 = &escore_priv;
	int rc = 0;
	u32 sync_cmd = (ES_SYNC_CMD << 16) | ES_SYNC_POLLING;
	u32 sync_ack;
	char *status_name = "Ping";

	rc = escore_cmd(es515, sync_cmd);
	if (rc < 0) {
		pr_err("%s(): firmware load failed sync write\n",
				__func__);
		goto cmd_err;
	}
	sync_ack = es515->bus.last_response;
	pr_info("%s(): sync_ack = 0x%08x\n", __func__, sync_ack);

	rc = snprintf(buf, PAGE_SIZE,
		       "%s=0x%08x\n",
		       status_name, sync_ack);
cmd_err:
	return rc;
}

static DEVICE_ATTR(ping_status, 0444, es515_ping_status_show, NULL);
/* /sys/devices/platform/msm_slim_ctrl.1/es515-codec-gen0/ping_status */
static struct attribute *core_sysfs_attrs[] = {
	&dev_attr_route_status.attr,
	&dev_attr_route.attr,
	&dev_attr_clock_on.attr,
	&dev_attr_fw_version.attr,
	&dev_attr_ping_status.attr,
	NULL
};

static struct attribute_group core_sysfs = {
	.attrs = core_sysfs_attrs
};

int es515_bootup(struct escore_priv *es515)
{
	int rc;

	pr_info("%s()\n", __func__);

	BUG_ON(es515->standard->size == 0);

	mutex_lock(&es515->api_mutex);

	if (es515->boot_ops.setup) {
		pr_info("%s(): calling bus specific boot setup\n", __func__);
		rc = es515->boot_ops.setup(es515);
		if (rc != 0) {
			pr_err("%s() bus specific boot setup error\n",
				__func__);
			goto es515_bootup_failed;
		}
	}

	rc = es515->bus.ops.write(es515, (char *)es515->standard->data,
			      es515->standard->size);
	if (rc < 0) {
		pr_err("%s(): firmware download failed\n", __func__);
		rc = -EIO;
		goto es515_bootup_failed;
	}

	if (es515->boot_ops.finish) {
		pr_info("%s(): calling bus specific boot finish\n", __func__);
		rc = es515->boot_ops.finish(es515);
		if (rc != 0) {
			pr_err("%s() bus specific boot finish error\n",
				__func__);
			goto es515_bootup_failed;
		}
	}

es515_bootup_failed:
	mutex_unlock(&es515->api_mutex);
	return rc;
}

int es515_slim_sleep(struct escore_priv *escore)
{
	/* TODO:
	 * Add the code for slimbus
	 */
	return 0;
}

int es515_slim_wakeup(struct escore_priv *escore)
{
	/* TODO:
	 * Add the code for slimbus
	 */
	return 0;
}

int es515_i2c_sleep(struct escore_priv *escore)
{
	u32 cmd;
	int err;

	/* Set Smooth rate to 0 */
	cmd = 0x904e0000;
	err = escore_cmd(escore, cmd);
	if (err < 0)
		return err;

	/* Set power state to SLEEP */
	cmd = (ES_SET_POWER_STATE << 16);
	cmd |= (ES_PM_SLEEP & 0xffff);
	err = escore_cmd(escore, cmd);
	if (err < 0)
		return err;

	msleep(25);

	/* TODO: Add the platform specific to cut the system clock off */

	return err;
}

int es515_i2c_wakeup(struct escore_priv *escore)
{
	u32 sync_cmd = (ES_SYNC_CMD << 16) | ES_SYNC_POLLING;
	u32 sync_ack;
	int err;

	/* TODO: Add the platform specific code to turn on system clock */

	gpio_set_value(escore->pdata->wakeup_gpio, 0);
	msleep(30);

	/* Set the wakeup GPIO level to normal */
	gpio_set_value(escore->pdata->wakeup_gpio, 1);

	/* Set Smooth rate to 0 */
	err = escore_cmd(escore, sync_cmd);
	if (err < 0) {
		pr_err("%s(): sync write failed after wakeup\n", __func__);
		goto cmd_err;
	}

	sync_ack = escore->bus.last_response;
	if (sync_ack != sync_cmd) {
		pr_err("%s(): sync ack failed after wakeup %x\n", __func__,
				sync_ack);
		goto sync_ack_err;
	}
	pr_info("%s(): sync_ack = 0x%08x\n", __func__, sync_ack);

cmd_err:
sync_ack_err:
	return err;
}



/*
 * eS515 Power stat transition table:
 * The table indicates -1 as invalid transition.
 * Value 0 indicates the current state and next state are same.
 * Value > 0 indicates the number of state transition required to move to
 * next state.
 */
static int es515_pstt[5][5] = {
	{ -1, -1, -1, -1, -1 },
	{ -1,  0,  2,  2,  1 },
	{ -1,  2,  0,  1,  2 },
	{ -1,  1,  1,  0,  1 },
	{ -1,  1,  1,  1,  0 },
};

/*
 * TODO:
 * As of now there is no way to enter into low-power MP modes.
 * These modes can be activated by defining a Kcontrol if required.
 */
static int es515_update_power_state(struct escore_priv *escore,
		int next_state)
{
	int curr_state;
	int interim_state;
	int nr_state_traversed = 0;
	int rc = 0;
	u32 cmd;

	pr_info("%s: Curr state:%d next state:%d\n", __func__,
			escore->pm_state, next_state);
	/* If wakeup GPIO is not initialized, the power state transitions
	 * SHOULD not be made, because the only transition between ACTIVE
	 * state and MP_CMD state are possible without using the wakeup_gpio.
	 */
	if (escore->pdata->wakeup_gpio == -1) {
		rc = -EINVAL;
		pr_err("%s(): Power state transition not allowed\n",
				__func__);
		goto es515_no_wakeup_gpio;
	}

	mutex_lock(&escore->api_mutex);

	curr_state = escore->pm_state;

	if (es515_pstt[curr_state][next_state] == -1) {
		rc = -EINVAL;
		pr_err("%s(): Invalid power state transition\n", __func__);
		goto es515_inval_state;
	}

	interim_state = curr_state;
	while (nr_state_traversed < es515_pstt[curr_state][next_state]) {
		switch (interim_state) {
		case ES_PM_ACTIVE:
		case ES_PM_MP_CMD:
			if (next_state == ES_PM_SLEEP)
				rc = escore->sleep(escore);
			else {
				cmd = ES_SET_POWER_STATE << 16;
				cmd |= (next_state & 0xffff);
				rc = escore_cmd(escore, cmd);
			}
			if (rc < 0)
				goto es515_io_fail;
			interim_state = next_state;
			break;
		case ES_PM_MP_SLEEP:
		case ES_PM_SLEEP:
			rc = escore->wakeup(escore);
			if (rc < 0)
				goto es515_io_fail;
			interim_state = (curr_state == ES_PM_MP_SLEEP) ?
				ES_PM_MP_CMD : ES_PM_ACTIVE;
			break;
		default:
			break;
		}
		nr_state_traversed++;
	}
	escore->pm_state = next_state;

es515_io_fail:
es515_inval_state:
	mutex_unlock(&escore->api_mutex);

es515_no_wakeup_gpio:
	return rc;
}

static int es515_set_bias_level(struct snd_soc_codec *codec,
				enum snd_soc_bias_level level)
{
	struct escore_priv *es515 = snd_soc_codec_get_drvdata(codec);
	int rc = 0;

	pr_info("%s:%d level:%d\n", __func__, __LINE__, level);
	switch (level) {
	case SND_SOC_BIAS_ON:
		break;

	case SND_SOC_BIAS_PREPARE:
		break;

	case SND_SOC_BIAS_STANDBY:
		es515_update_power_state(es515, ES_PM_ACTIVE);
		break;

	case SND_SOC_BIAS_OFF:
		es515_update_power_state(es515, ES_PM_SLEEP);
		break;
	}
	codec->dapm.bias_level = level;

	return rc;
}

struct snd_soc_dai_driver es515_dai[] = {
#if defined(CONFIG_SND_SOC_ES_I2S)
	{
		.name = "earSmart-porta",
		.id = ES_I2S_PORTA,
		.playback = {
			.stream_name = "PORTA Playback",
			.channels_min = 2,
			.channels_max = 2,
			.rates = ES_RATES,
			.formats = ES_FORMATS,
		},
		.capture = {
			.stream_name = "PORTA Capture",
			.channels_min = 2,
			.channels_max = 2,
			.rates = ES_RATES,
			.formats = ES_FORMATS,
		},
		.ops = &escore_i2s_port_dai_ops,
	},
	{
		.name = "earSmart-portb",
		.id = ES_I2S_PORTB,
		.playback = {
			.stream_name = "PORTB Playback",
			.channels_min = 2,
			.channels_max = 2,
			.rates = ES_RATES,
			.formats = ES_FORMATS,
		},
		.capture = {
			.stream_name = "PORTB Capture",
			.channels_min = 2,
			.channels_max = 2,
			.rates = ES_RATES,
			.formats = ES_FORMATS,
		},
		.ops = &escore_i2s_port_dai_ops,
	},
	{
		.name = "earSmart-portc",
		.id = ES_I2S_PORTC,
		.playback = {
			.stream_name = "PORTC Playback",
			.channels_min = 2,
			.channels_max = 2,
			.rates = ES_RATES,
			.formats = ES_FORMATS,
		},
		.capture = {
			.stream_name = "PORTC Capture",
			.channels_min = 2,
			.channels_max = 2,
			.rates = ES_RATES,
			.formats = ES_FORMATS,
		},
		.ops = &escore_i2s_port_dai_ops,
	},
#endif
#if defined(CONFIG_SND_SOC_ES_SLIM)
	{
		.name = "es515-slim-rx1",
		.id = ES_SLIM_1_PB,
		.playback = {
			.stream_name = "SLIM_PORT-1 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES_SLIMBUS_RATES,
			.formats = ES_SLIMBUS_FORMATS,
		},
		.ops = &escore_slim_port_dai_ops,
	},
	{
		.name = "es515-slim-tx1",
		.id = ES_SLIM_1_CAP,
		.capture = {
			.stream_name = "SLIM_PORT-1 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES_SLIMBUS_RATES,
			.formats = ES_SLIMBUS_FORMATS,
		},
		.ops = &escore_slim_port_dai_ops,
	},
	{
		.name = "es515-slim-rx2",
		.id = ES_SLIM_2_PB,
		.playback = {
			.stream_name = "SLIM_PORT-2 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES_SLIMBUS_RATES,
			.formats = ES_SLIMBUS_FORMATS,
		},
		.ops = &escore_slim_port_dai_ops,
	},
	{
		.name = "es515-slim-tx2",
		.id = ES_SLIM_2_CAP,
		.capture = {
			.stream_name = "SLIM_PORT-2 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES_SLIMBUS_RATES,
			.formats = ES_SLIMBUS_FORMATS,
		},
		.ops = &escore_slim_port_dai_ops,
	},
	{
		.name = "es515-slim-rx3",
		.id = ES_SLIM_3_PB,
		.playback = {
			.stream_name = "SLIM_PORT-3 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES_SLIMBUS_RATES,
			.formats = ES_SLIMBUS_FORMATS,
		},
		.ops = &escore_slim_port_dai_ops,
	},
	{
		.name = "es515-slim-tx3",
		.id = ES_SLIM_3_CAP,
		.capture = {
			.stream_name = "SLIM_PORT-3 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES_SLIMBUS_RATES,
			.formats = ES_SLIMBUS_FORMATS,
		},
		.ops = &escore_slim_port_dai_ops,
	}
#endif
};

#ifdef CONFIG_PM
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
static int es515_codec_suspend(struct snd_soc_codec *codec)
#else
static int es515_codec_suspend(struct snd_soc_codec *codec,
		pm_message_t state)
#endif
{
	es515_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static int es515_codec_resume(struct snd_soc_codec *codec)
{
	es515_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	return 0;
}
#else
#define es515_codec_suspend NULL
#define es515_codec_resume NULL
#endif

static int es515_codec_probe(struct snd_soc_codec *codec)
{
	int ret;
	struct escore_priv *es515 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s()\n", __func__);
	es515->codec = codec;

	codec->control_data = snd_soc_codec_get_drvdata(codec);

	ret = es_d202_add_snd_soc_controls(codec);
	if (ret) {
		dev_err(codec->dev,
			"%s(): es_d202_snd_controls failed\n", __func__);
		return ret;
	}
	ret = es_a212_add_snd_soc_controls(codec);
	if (ret) {
		dev_err(codec->dev,
			"%s(): es_a212_snd_controls failed\n", __func__);
		return ret;
	}
	ret = es_d202_add_snd_soc_dapm_controls(codec);
	if (ret) {
		dev_err(codec->dev,
			"%s(): es_d202_dapm_widgets failed\n", __func__);
		return ret;
	}
	ret = es_a212_add_snd_soc_dapm_controls(codec);
	if (ret) {
		dev_err(codec->dev,
			"%s(): es_a212_dapm_widgets failed\n", __func__);
		return ret;
	}
	ret = es_d202_add_snd_soc_route_map(codec);
	if (ret) {
		dev_err(codec->dev,
			"%s(): es_d202_add_routes failed\n", __func__);
		return ret;
	}
	ret = es_a212_add_snd_soc_route_map(codec);
	if (ret) {
		dev_err(codec->dev,
			"%s(): es_a212_add_routes failed\n", __func__);
		return ret;
	}

	es_d202_fill_cmdcache(escore_priv.codec);

	return 0;
}

static int  es515_codec_remove(struct snd_soc_codec *codec)
{
	struct escore_priv *es515 = snd_soc_codec_get_drvdata(codec);

	es515_set_bias_level(codec, SND_SOC_BIAS_OFF);

	kfree(es515);

	return 0;
}

static unsigned int es515_codec_read(struct snd_soc_codec *codec,
				unsigned int reg)
{
	struct escore_priv *escore = &escore_priv;
	struct escore_api_access *api_access;
	u32 cmd;
	unsigned int msg_len;
	int rc;

	if (reg > ES_MAX_REGISTER) {
		dev_err(codec->dev, "read out of range reg %d", reg);
		return 0;
	}
	api_access = &escore->api_access[ES_CODEC_VALUE];
	msg_len = api_access->read_msg_len;
	memcpy((char *)&cmd, (char *)api_access->read_msg, msg_len);

	mutex_lock(&escore->api_mutex);
	rc = escore_cmd(escore, cmd | reg<<8);
	if (rc < 0) {
		dev_err(codec->dev, "codec reg read err %d()", rc);
		mutex_unlock(&escore->api_mutex);
		return rc;
	}
	cmd = escore->bus.last_response;
	mutex_unlock(&escore->api_mutex);

	return cmd & 0xff;
}

static int es515_codec_write(struct snd_soc_codec *codec, unsigned int reg,
	unsigned int value)
{
	int ret;

	if (reg > ES_MAX_REGISTER) {
		dev_err(codec->dev, "write out of range reg %d", reg);
		return 0;
	}

	ret = escore_write(codec, ES_CODEC_VALUE, reg<<8 | value);
	if (ret < 0) {
		dev_err(codec->dev, "codec reg %x write err %d\n",
			reg, ret);
		return ret;
	}
	return 0;
}

struct snd_soc_codec_driver soc_codec_dev_es515 = {
	.probe =	es515_codec_probe,
	.remove =	es515_codec_remove,
	.suspend =	es515_codec_suspend,
	.resume =	es515_codec_resume,
	.read =		es515_codec_read,
	.write =	es515_codec_write,
	.set_bias_level =	es515_set_bias_level,
};

int es515_set_streaming(struct escore_priv *escore, int value)
{
	return escore_cmd(escore,
		es515_streaming_cmds[escore->streamdev.intf] | value);
}

void es515_slim_setup(struct escore_priv *escore_priv)
{
	int i;
	int ch_cnt;

	/* allocate ch_num array for each DAI */
	for (i = 0; i < ARRAY_SIZE(es515_dai); i++) {
		switch (es515_dai[i].id) {
		case ES_SLIM_1_PB:
		case ES_SLIM_2_PB:
		case ES_SLIM_3_PB:
			ch_cnt = es515_dai[i].playback.channels_max;
			break;
		case ES_SLIM_1_CAP:
		case ES_SLIM_2_CAP:
		case ES_SLIM_3_CAP:
			ch_cnt = es515_dai[i].capture.channels_max;
			break;
		default:
				continue;
		}
		escore_priv->slim_dai_data[i].ch_num =
			kzalloc((ch_cnt * sizeof(unsigned int)), GFP_KERNEL);
	}
	/* front end for RX1 */
	escore_priv->slim_dai_data[DAI_INDEX(ES_SLIM_1_PB)].ch_num[0] = 152;
	escore_priv->slim_dai_data[DAI_INDEX(ES_SLIM_1_PB)].ch_num[1] = 153;
	/* back end for RX1 */
	escore_priv->slim_dai_data[DAI_INDEX(ES_SLIM_2_CAP)].ch_num[0] = 138;
	escore_priv->slim_dai_data[DAI_INDEX(ES_SLIM_2_CAP)].ch_num[1] = 139;
	/* front end for TX1 */
	escore_priv->slim_dai_data[DAI_INDEX(ES_SLIM_1_CAP)].ch_num[0] = 156;
	escore_priv->slim_dai_data[DAI_INDEX(ES_SLIM_1_CAP)].ch_num[1] = 157;
	/* back end for TX1 */
	escore_priv->slim_dai_data[DAI_INDEX(ES_SLIM_3_PB)].ch_num[0] = 134;
	escore_priv->slim_dai_data[DAI_INDEX(ES_SLIM_3_PB)].ch_num[1] = 135;
	/* front end for RX2 */
	escore_priv->slim_dai_data[DAI_INDEX(ES_SLIM_2_PB)].ch_num[0] = 154;
	escore_priv->slim_dai_data[DAI_INDEX(ES_SLIM_2_PB)].ch_num[1] = 155;
	/* back end for RX2 */
	escore_priv->slim_dai_data[DAI_INDEX(ES_SLIM_3_CAP)].ch_num[0] = 143;
	escore_priv->slim_dai_data[DAI_INDEX(ES_SLIM_3_CAP)].ch_num[1] = 144;
}

int es515_core_probe(struct device *dev)
{
	struct esxxx_platform_data *pdata = dev->platform_data;
	int rc = 0;
	const char *fw_filename = "audience_fw_es515.bin";

	if (pdata == NULL) {
		dev_err(dev, "%s(): pdata is NULL", __func__);
		rc = -EIO;
		goto pdata_error;
	}

	escore_priv.dev = dev;
	escore_priv.pdata = pdata;

	mutex_init(&escore_priv.api_mutex);
	mutex_init(&escore_priv.pm_mutex);
	mutex_init(&escore_priv.streaming_mutex);
	mutex_init(&escore_priv.msg_list_mutex);

	init_waitqueue_head(&escore_priv.stream_in_q);
	INIT_LIST_HEAD(&escore_priv.msg_list);

	rc = sysfs_create_group(&escore_priv.dev->kobj, &core_sysfs);
	if (rc) {
		dev_err(escore_priv.dev,
			"failed to create core sysfs entries: %d\n", rc);
	}

	dev_dbg(escore_priv.dev, "%s(): reset_gpio = %d\n", __func__,
		pdata->reset_gpio);
	if (pdata->reset_gpio != -1) {
		rc = gpio_request(pdata->reset_gpio, "es515_reset");
		if (rc < 0) {
			dev_err(escore_priv.dev, "%s(): es515_reset request failed",
				__func__);
			goto reset_gpio_request_error;
		}
		rc = gpio_direction_output(pdata->reset_gpio, 1);
		if (rc < 0) {
			dev_err(escore_priv.dev, "%s(): es515_reset direction failed",
				__func__);
			goto reset_gpio_direction_error;
		}
		gpio_set_value(pdata->reset_gpio, 1);
	} else {
		dev_warn(escore_priv.dev, "%s(): es515_reset undefined\n",
			 __func__);
	}

	dev_dbg(escore_priv.dev, "%s(): wakeup_gpio = %d\n", __func__,
		pdata->wakeup_gpio);

	if (pdata->wakeup_gpio != -1) {
		rc = gpio_request(pdata->wakeup_gpio, "es515_wakeup");
		if (rc < 0) {
			dev_err(escore_priv.dev, "%s(): es515_wakeup request failed",
				__func__);
			goto wakeup_gpio_request_error;
		}
		rc = gpio_direction_output(pdata->wakeup_gpio, 1);
		if (rc < 0) {
			dev_err(escore_priv.dev, "%s(): es515_wakeup direction failed",
				__func__);
			goto wakeup_gpio_direction_error;
		}
	} else {
		dev_warn(escore_priv.dev, "%s(): es515_wakeup undefined\n",
			 __func__);
	}

	dev_dbg(escore_priv.dev, "%s(): gpioa_gpio = %d\n", __func__,
		pdata->gpioa_gpio);
	if (pdata->gpioa_gpio != -1) {
		rc = gpio_request(pdata->gpioa_gpio, "es515_gpioa");
		if (rc < 0) {
			dev_err(escore_priv.dev, "%s(): es515_gpioa request failed",
				__func__);
			goto gpioa_gpio_request_error;
		}
		rc = gpio_direction_input(pdata->gpioa_gpio);
		if (rc < 0) {
			dev_err(escore_priv.dev, "%s(): es515_gpioa direction failed",
				__func__);
			goto gpioa_gpio_direction_error;
		}
	} else {
		dev_warn(escore_priv.dev, "%s(): es515_gpioa undefined\n",
			 __func__);
	}

	dev_dbg(escore_priv.dev, "%s(): gpiob_gpio = %d\n", __func__,
		pdata->gpiob_gpio);

	if (pdata->gpiob_gpio != -1) {
		rc = gpio_request(pdata->gpiob_gpio, "es515_gpiob");
		if (rc < 0) {
			dev_err(escore_priv.dev, "%s(): es515_gpiob request failed",
				__func__);
			goto gpiob_gpio_request_error;
		}
		rc = gpio_direction_input(pdata->gpiob_gpio);
		if (rc < 0) {
			dev_err(escore_priv.dev, "%s(): es515_gpiob direction failed",
				__func__);
			goto gpiob_gpio_direction_error;
		}
	} else {
		dev_warn(escore_priv.dev, "%s(): es515_gpiob undefined\n",
			 __func__);
	}

	rc = request_firmware((const struct firmware **)&escore_priv.standard,
			      fw_filename, escore_priv.dev);
	if (rc) {
		dev_err(escore_priv.dev, "%s(): request_firmware(%s) failed %d\n",
			__func__, fw_filename, rc);
		goto request_firmware_error;
	}

	/* Enable accessory detection for ES515 */
	escore_priv.process_analog = 1;
	escore_priv.regs = kmalloc(sizeof(struct escore_intr_regs), GFP_KERNEL);
	if (escore_priv.regs == NULL) {
		dev_err(escore_priv.dev, "%s(): memory alloc failed for regs\n",
				__func__);
		rc = -ENOMEM;
		goto regs_memalloc_error;
	}

	escore_priv.regs->get_intr_status = ES_GET_SYS_INTERRUPT_STATUS;
	escore_priv.regs->clear_intr_status = ES_CLEAR_SYS_INTERRUPT_STATUS;
	escore_priv.regs->accdet_config = ES_ACCESSORY_DET_CONFIG;
	escore_priv.regs->accdet_status = ES_ACCESSORY_DET_STATUS;
	escore_priv.regs->enable_btndet = ES_BUTTON_DETECTION_ENABLE;

	escore_priv.regs->btn_serial_cfg = ES_BUTTON_SERIAL_CONFIG;
	escore_priv.regs->btn_parallel_cfg = ES_BUTTON_PARALLEL_CONFIG;
	escore_priv.regs->btn_detection_rate = ES_BUTTON_DETECTION_RATE;
	escore_priv.regs->btn_bounce_time = ES_BUTTON_BOUNCE_TIME;
	escore_priv.regs->btn_press_settling_time =
		ES_BUTTON_PRESS_SETTLING_TIME;
	escore_priv.regs->btn_long_press_time =
		ES_BUTTON_DETECTION_LONG_PRESS_TIME;

	escore_priv.boot_ops.bootup = es515_bootup;
	escore_priv.soc_codec_dev_escore = &soc_codec_dev_es515;
	escore_priv.dai = es515_dai;
	escore_priv.dai_nr = ES_NUM_CODEC_DAIS;
	escore_priv.api_addr_max = ES_API_ADDR_MAX;
	escore_priv.api_access = es515_api_access;
	escore_priv.flag.is_codec = 1;

	if (escore_priv.pri_intf == ES_SLIM_INTF) {

		escore_priv.slim_rx = slim_rx;
		escore_priv.slim_tx = slim_tx;
		escore_priv.slim_dai_data = dai;

		escore_priv.slim_rx_ports = ES_SLIM_RX_PORTS;
		escore_priv.slim_tx_ports = ES_SLIM_TX_PORTS;
		escore_priv.codec_slim_dais = ES_NUM_CODEC_SLIM_DAIS;

		escore_priv.slim_tx_port_to_ch_map = es515_slim_tx_port_to_ch;
		escore_priv.slim_rx_port_to_ch_map = es515_slim_rx_port_to_ch;

		/* Initialization of be_id goes here if required */
		escore_priv.slim_be_id = NULL;

		/* Initialization of _remote_ routines goes here if required */
		escore_priv.remote_cfg_slim_rx = NULL;
		escore_priv.remote_cfg_slim_tx = NULL;
		escore_priv.remote_close_slim_rx = NULL;
		escore_priv.remote_close_slim_tx = NULL;

		escore_priv.flag.local_slim_ch_cfg = 1;
		escore_priv.channel_dir = es515_channel_dir;

		escore_priv.sleep  = es515_slim_sleep;
		escore_priv.wakeup = es515_slim_wakeup;

	} else if (escore_priv.pri_intf == ES_I2C_INTF) {
		escore_priv.sleep  = es515_i2c_sleep;
		escore_priv.wakeup = es515_i2c_wakeup;
	}
#if defined(CONFIG_SND_SOC_ES_UART)
	escore_priv.uart_dev.baudrate_bootloader = UART_TTY_BAUD_RATE_460_8_K;
	escore_priv.uart_dev.baudrate_fw = UART_TTY_BAUD_RATE_3_M;
#endif

	if (pdata->gpiob_gpio != -1) {
		rc = request_threaded_irq(gpio_to_irq(pdata->gpiob_gpio), NULL,
				escore_irq_work, IRQF_TRIGGER_RISING,
				"escore-irq-work", &escore_priv);
		if (rc < 0) {
			pr_err("Error in registering interrupt :%d", rc);
			goto interrupt_direction_error;
		}
	}

	return rc;

regs_memalloc_error:
interrupt_direction_error:
request_firmware_error:
gpiob_gpio_direction_error:
	gpio_free(pdata->gpiob_gpio);
gpiob_gpio_request_error:
gpioa_gpio_direction_error:
	gpio_free(pdata->gpioa_gpio);
gpioa_gpio_request_error:
wakeup_gpio_direction_error:
	gpio_free(pdata->wakeup_gpio);
wakeup_gpio_request_error:
reset_gpio_direction_error:
	gpio_free(pdata->reset_gpio);
reset_gpio_request_error:
pdata_error:
	dev_dbg(escore_priv.dev, "%s(): exit with error\n", __func__);

	return rc;
}
EXPORT_SYMBOL_GPL(es515_core_probe);

static __init int es515_init(void)
{
	int rc = 0;

	pr_info("%s()", __func__);

#if defined(CONFIG_SND_SOC_ES_I2C)
	rc = escore_i2c_init();
#elif defined(CONFIG_SND_SOC_ES_SLIM)
	rc = escore_slimbus_init();
#elif defined(CONFIG_SND_SOC_ES_UART)
	rc = escore_uart_bus_init(&escore_priv);
#endif
	if (rc) {
		pr_info("Error registering Audience eS515 driver: %d\n", rc);
		goto INIT_ERR;
	}

#if defined(CONFIG_SND_SOC_ES_CDEV) && !defined(CONFIG_SND_SOC_ES_UART)
	rc = escore_cdev_init(&escore_priv);
	if (rc) {
		pr_info("Error enabling CDEV interface: %d\n", rc);
		goto INIT_ERR;
	}
#endif
INIT_ERR:
	return rc;
}
module_init(es515_init);

static __exit void es515_exit(void)
{
	pr_info("%s()\n", __func__);

#if defined(CONFIG_SND_SOC_ES_I2C)
	i2c_del_driver(&escore_i2c_driver);
#endif
}
module_exit(es515_exit);


MODULE_DESCRIPTION("ASoC ES515 driver");
MODULE_AUTHOR("Greg Clemson <gclemson@audience.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:es515-codec");
MODULE_FIRMWARE("audience_fw_es515.bin");
