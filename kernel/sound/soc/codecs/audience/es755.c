/*
 * es755.c  --  Audience eS755 ALSA SoC Audio driver
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
#include "es755.h"
#include "escore.h"
#include "escore-i2c.h"
#include "escore-i2s.h"
#include "escore-spi.h"
#include "escore-slim.h"
#include "escore-cdev.h"
#include "escore-uart.h"
#include "escore-vs.h"
#include "escore-uart-common.h"
#include "es755-access.h"
#include "es-d300.h"
#include "es-a300-reg.h"
#include <linux/sort.h>
#ifdef CONFIG_QPNP_CLKDIV
#include <linux/qpnp/clkdiv.h>
#else
#include <linux/clk.h>
#endif

union es755_accdet_reg {
	u16 value;
	struct {
		u16 res1:1;
		u16 plug_det_fsm:1;
		u16 debounce_timer:2;
		u16 res2:1;
		u16 mic_det_fsm:1;
		u16 mg_sel_force:1;
		u16 mg_select:1;
	} fields;
};
/* codec private data TODO: move to runtime init */
struct escore_priv escore_priv = {
	.pm_state = ES_PM_ACTIVE,
	.probe = es755_core_probe,
	.set_streaming = es755_set_streaming,
	.set_datalogging = es755_set_datalogging,
	.streamdev.no_more_bit = 0,
};

struct snd_soc_dai_driver es755_dai[];

#ifdef CONFIG_ARCH_MSM8974
/*Slimbus channel map for APQ8074*/
static int es755_slim_rx_port_to_ch[ES_SLIM_RX_PORTS] = {
		152, 153, 154, 155, 134, 135
};
static int es755_slim_tx_port_to_ch[ES_SLIM_TX_PORTS] = {
		156, 157, 144, 145, 146, 147
};
#else
/*Slimbus channel map for APQ8060*/
static int es755_slim_rx_port_to_ch[ES_SLIM_RX_PORTS] = {
	152, 153, 154, 155, 134, 135
};
static int es755_slim_tx_port_to_ch[ES_SLIM_TX_PORTS] = {
	156, 157, 138, 139, 143, 144
};


#endif

#ifdef CONFIG_QPNP_CLKDIV
static struct q_clkdiv *codec_clk;
#else
static struct clk *codec_clk;
#endif

static struct escore_slim_dai_data slim_dai_data[ES_NUM_CODEC_SLIM_DAIS];
static struct escore_slim_ch slim_rx[ES_SLIM_RX_PORTS];
static struct escore_slim_ch slim_tx[ES_SLIM_TX_PORTS];

static const u32 es755_streaming_cmds[] = {
	0x00000000,		/* ES_NULL_INTF */
	0x90250200,		/* ES_SLIM_INTF */
	0x90250000,		/* ES_I2C_INTF  */
	0x90250300,		/* ES_SPI_INTF  */
	0x90250100,		/* ES_UART_INTF */
};

#ifdef CONFIG_SND_SOC_ES755_A1
static char *es755_device  = "es755-A1-device";
#else
static char *es755_device  = "es755-A0-device";
#endif

static int es755_clk_ctl(int enable)
{
	int ret = 0;

	if (enable)
#ifdef CONFIG_QPNP_CLKDIV
		ret = qpnp_clkdiv_enable(codec_clk);
#else
	ret = clk_enable(codec_clk);
#endif
	else
#ifdef CONFIG_QPNP_CLKDIV
		ret = qpnp_clkdiv_disable(codec_clk);
#else
	clk_disable(codec_clk);
#endif

	if (EINVAL != ret)
		ret = 0;

	return ret;
}
static int es755_channel_dir(int dai_id)
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
static ssize_t es755_route_status_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	int ret = 0;
	unsigned int value = 0;
	char *status_name = "Route Status";
	struct escore_priv *escore = &escore_priv;
	unsigned int msg_len;
	u32 api_word = 0;
	const char *algo_text[ALGO_MAX] = {
		[VP] = "VP",
		[MM] = "MM",
		[AudioZoom] = "AudioZoom",
		[Passthru] = "Passthru",
	};

	const char *status_text[] = {
		"Active",
		"Muting",
		"Switching",
		"Unmuting",
		"Inactive",
	};

	const u8 algo_id[ALGO_MAX] = {
		[VP] = 0,
		[MM] = 1,
		[AudioZoom] = 2,
		[Passthru] = 3,
	};

	if (escore->algo_type != None) {
		ret = escore_prepare_msg(escore, ES_CHANGE_STATUS, value,
				(char *)&api_word, &msg_len, ES_MSG_READ);
		if (ret) {
			pr_err("%s(): Failed to prepare message\n", __func__);
			goto out;
		}

		/* Set the algo type in message */
		api_word |= algo_id[escore->algo_type];
		ret = escore_cmd(escore, api_word, &value);
		if (ret < 0) {
			pr_err("%s(): escore_cmd()", __func__);
			goto out;
		}
		value &= 0xFF;

		ret = snprintf(buf, PAGE_SIZE, "%s for %s Algo is %s\n",
				status_name, algo_text[escore->algo_type],
				status_text[value]);

	} else {
		pr_err("Algo type not set\n");
		ret = -EINVAL;
	}
out:
	return ret;
}

static DEVICE_ATTR(route_status, 0444, es755_route_status_show, NULL);
/* /sys/devices/platform/msm_slim_ctrl.1/es755-codec-gen0/route_status */

static ssize_t es755_get_pm_enable(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", escore_priv.pm_enable ?
			"on" : "off");
}
static ssize_t es755_set_pm_enable(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	pr_info("%s(): requested - %s\n", __func__, buf);
	if (!strncmp(buf, "on", 2))
		escore_pm_enable();
	else if (!strncmp(buf, "off", 3))
		escore_pm_disable();
	return count;

}
static DEVICE_ATTR(pm_enable, 0666, es755_get_pm_enable, es755_set_pm_enable);

static ssize_t get_power_state(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	if (escore_priv.non_vs_pm_state == ES_PM_NORMAL)
		return snprintf(buf, PAGE_SIZE, "%s\n", "awake");
	else if (escore_priv.non_vs_pm_state == ES_PM_ASLEEP)
		return snprintf(buf, PAGE_SIZE, "%s\n", "asleep");
	else
		return snprintf(buf, PAGE_SIZE, "%s\n", "in transition");
}
static ssize_t set_power_state(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct escore_priv *escore = &escore_priv;
	int rc;

	if (count > 32) {
		pr_err("%s(): Invalid number of bytes sent:%d\n",
				__func__, count);
		return -EFAULT;
	}

	pr_info("%s(): Power state requested - %s\n", __func__, buf);
	if (!strncmp(buf, "sleep", 5)) {
		rc = escore_suspend(escore->dev);
		if (rc < 0)
			pr_err("Sleep error\n");
	} else if (!strncmp(buf, "wakeup", 6)) {
		rc = escore_resume(escore->dev);
		if (rc < 0)
			pr_err("Wakeup error\n");
	} else
		pr_err("%s(): Invalid command: %s\n", __func__, buf);

	return count;
}

static DEVICE_ATTR(power_state, 0666, get_power_state, set_power_state);

#define SIZE_OF_VERBUF 256
/* TODO: fix for new read/write. use es755_read() instead of BUS ops */
static ssize_t es755_fw_version_show(struct device *dev,
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
		if (!value)
			break;
	}
	/* Null terminate the string*/
	*verbuf = '\0';
	pr_debug("Audience fw ver %s\n", versionbuffer);
	return snprintf(buf, PAGE_SIZE, "FW Version = %s\n", versionbuffer);
}

static DEVICE_ATTR(fw_version, 0444, es755_fw_version_show, NULL);
/* /sys/devices/platform/msm_slim_ctrl.1/es755-codec-gen0/fw_version */

static ssize_t es755_clock_on_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	int ret = 0;

	return ret;
}

static DEVICE_ATTR(clock_on, 0444, es755_clock_on_show, NULL);
/* /sys/devices/platform/msm_slim_ctrl.1/es755-codec-gen0/clock_on */

static ssize_t es755_reset_control_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	es_d300_reset_cmdcache();
	return 0;
}

static DEVICE_ATTR(reset_control, 0444, es755_reset_control_show, NULL);

static ssize_t es755_ping_status_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct escore_priv *es755 = &escore_priv;
	int rc = 0;
	u32 sync_cmd = (ES_SYNC_CMD << 16) | ES_SYNC_POLLING;
	u32 sync_ack;
	char *status_name = "Ping";

	rc = escore_cmd(es755, sync_cmd, &sync_ack);
	if (rc < 0) {
		pr_err("%s(): firmware load failed sync write\n",
				__func__);
		goto cmd_err;
	}
	pr_debug("%s(): sync_ack = 0x%08x\n", __func__, sync_ack);

	rc = snprintf(buf, PAGE_SIZE,
		       "%s=0x%08x\n",
		       status_name, sync_ack);
cmd_err:
	return rc;
}

static DEVICE_ATTR(ping_status, 0444, es755_ping_status_show, NULL);

/* /sys/devices/platform/msm_slim_ctrl.1/es755-codec-gen0/ping_status */
static ssize_t es755_cmd_history_show(struct device *dev,
		struct device_attribute *attr,
				   char *buf)
{
	int i = 0, j = 0;

	for (i = cmd_hist_index; i < ES_MAX_ROUTE_MACRO_CMD; i++) {
		if (cmd_hist[i].cmd)
			j += snprintf(buf+j,
					PAGE_SIZE,
					"cmd=0x%04x 0x%04x,tstamp=%lu\n",
					cmd_hist[i].cmd>>16,
					cmd_hist[i].cmd & 0xffff,
					cmd_hist[i].timestamp);
	}
	for (i = 0; i < cmd_hist_index; i++) {
		if (cmd_hist[i].cmd)
			j += snprintf(buf+j,
					PAGE_SIZE,
					"cmd=0x%04x 0x%04x,tstamp=%lu\n",
					cmd_hist[i].cmd>>16,
					cmd_hist[i].cmd & 0xffff,
					cmd_hist[i].timestamp);
	}
	return	j;
}
static ssize_t es755_cmd_history_clear(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	pr_info("%s(): requested - %s\n", __func__, buf);
	if (!strncmp(buf, "clear", 5)) {
		memset(cmd_hist, 0,  ES_MAX_ROUTE_MACRO_CMD *
						sizeof(cmd_hist[0]));
		cmd_hist_index = 0;
	} else
		pr_err("%s(): Invalid command: %s\n", __func__, buf);
	return count;
}
static DEVICE_ATTR(cmd_history, 0666, es755_cmd_history_show,
						es755_cmd_history_clear);
static struct attribute *core_sysfs_attrs[] = {
	&dev_attr_route_status.attr,
	&dev_attr_reset_control.attr,
	&dev_attr_clock_on.attr,
	&dev_attr_fw_version.attr,
	&dev_attr_ping_status.attr,
	&dev_attr_cmd_history.attr,
	&dev_attr_power_state.attr,
	&dev_attr_pm_enable.attr,
	NULL
};

static struct attribute_group core_sysfs = {
	.attrs = core_sysfs_attrs
};

int es755_bootup(struct escore_priv *es755)
{
	int rc;
	u32 cmd, resp;

	pr_debug("%s()\n", __func__);

	BUG_ON(es755->standard->size == 0);
	escore_gpio_reset(es755);

	if (es755->boot_ops.setup) {
		pr_debug("%s(): calling bus specific boot setup\n", __func__);
		rc = es755->boot_ops.setup(es755);
		if (rc != 0) {
			pr_err("%s() bus specific boot setup error\n",
				__func__);
			goto es755_bootup_failed;
		}
	}
	es755->mode = SBL;

	rc = es755->bus.ops.high_bw_write(es755, (char *)es755->standard->data,
			      es755->standard->size);
	if (rc < 0) {
		pr_err("%s(): firmware download failed\n", __func__);
		rc = -EIO;
		goto es755_bootup_failed;
	}

	/* Give the chip some time to become ready after firmware
	 * download. */
	msleep(20);

	if (es755->boot_ops.finish) {
		pr_debug("%s(): calling bus specific boot finish\n", __func__);
		rc = es755->boot_ops.finish(es755);
		if (rc != 0) {
			pr_err("%s() bus specific boot finish error\n",
				__func__);
			goto es755_bootup_failed;
		}
	}
	es755->mode = STANDARD;

	if (es755->pdata->cmd_comp_mode == ES_CMD_COMP_INTR) {
		cmd = (ES_SYNC_CMD << 16) | ES_RISING_EDGE;
		rc = escore_cmd(es755, cmd, &resp);
		if (rc < 0) {
			pr_err("%s(): API interrupt config failed:%d\n",
					__func__, rc);

			goto es755_bootup_failed;
		}
	}

	es755->cmd_compl_mode = es755->pdata->cmd_comp_mode;

es755_bootup_failed:
	return rc;
}


static int es755_slim_set_channel_map(struct snd_soc_dai *dai,
				      unsigned int tx_num,
				      unsigned int *tx_slot,
				      unsigned int rx_num,
				      unsigned int *rx_slot)
{
	struct snd_soc_codec *codec = dai->codec;
	struct escore_priv *escore = &escore_priv;
	int id = dai->id;
	int i;
	int rc = 0;

	dev_dbg(codec->dev, "%s(): dai->name = %s, dai->id = %d\n", __func__,
		dai->name, dai->id);

	if (id == ES_SLIM_1_PB ||
	    id == ES_SLIM_2_PB ||
	    id == ES_SLIM_3_PB) {
		escore->slim_dai_data[DAI_INDEX(id)].ch_tot = rx_num;
		escore->slim_dai_data[DAI_INDEX(id)].ch_act = 0;
		for (i = 0; i < rx_num; i++)
			escore->slim_dai_data[DAI_INDEX(id)].ch_num[i] =
				rx_slot[i];
	} else if (id == ES_SLIM_1_CAP ||
		 id == ES_SLIM_2_CAP ||
		 id == ES_SLIM_3_CAP) {
		escore->slim_dai_data[DAI_INDEX(id)].ch_tot = tx_num;
		escore->slim_dai_data[DAI_INDEX(id)].ch_act = 0;
		for (i = 0; i < tx_num; i++)
			escore->slim_dai_data[DAI_INDEX(id)].ch_num[i] =
				tx_slot[i];
	}

	return rc;
}

#if defined(CONFIG_ARCH_MSM)
static int es755_slim_get_channel_map(struct snd_soc_dai *dai,
				      unsigned int *tx_num,
				      unsigned int *tx_slot,
				      unsigned int *rx_num,
				      unsigned int *rx_slot)
{
	struct snd_soc_codec *codec = dai->codec;
	struct escore_priv *escore = &escore_priv;
	struct escore_slim_ch *rx = escore->slim_rx;
	struct escore_slim_ch *tx = escore->slim_tx;
	int id = dai->id;
	int i;
	int rc = 0;

	dev_dbg(codec->dev, "%s(): dai->name = %s, dai->id = %d\n", __func__,
		dai->name, dai->id);

	if (id == ES_SLIM_1_PB) {
		*rx_num = escore->dai[DAI_INDEX(id)].playback.channels_max;
		for (i = 0; i < *rx_num; i++)
			rx_slot[i] = rx[ES_SLIM_1_PB_OFFSET + i].ch_num;
	} else if (id == ES_SLIM_2_PB) {
		*rx_num = escore->dai[DAI_INDEX(id)].playback.channels_max;
		for (i = 0; i < *rx_num; i++)
			rx_slot[i] = rx[ES_SLIM_2_PB_OFFSET + i].ch_num;
	} else if (id == ES_SLIM_3_PB) {
		*rx_num = escore->dai[DAI_INDEX(id)].playback.channels_max;
		for (i = 0; i < *rx_num; i++)
			rx_slot[i] = rx[ES_SLIM_3_PB_OFFSET + i].ch_num;
	} else if (id == ES_SLIM_1_CAP) {
		*tx_num = escore->dai[DAI_INDEX(id)].capture.channels_max;
		for (i = 0; i < *tx_num; i++)
			tx_slot[i] = tx[ES_SLIM_1_CAP_OFFSET + i].ch_num;
	} else if (id == ES_SLIM_2_CAP) {
		*tx_num = escore->dai[DAI_INDEX(id)].capture.channels_max;
		for (i = 0; i < *tx_num; i++)
			tx_slot[i] = tx[ES_SLIM_2_CAP_OFFSET + i].ch_num;
	} else if (id == ES_SLIM_3_CAP) {
		*tx_num = escore->dai[DAI_INDEX(id)].capture.channels_max;
		for (i = 0; i < *tx_num; i++)
			tx_slot[i] = tx[ES_SLIM_3_CAP_OFFSET + i].ch_num;
	}

	return rc;
}
#endif

int es755_digital_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	unsigned int val;

	dev_dbg(codec->dev, "%s() mute %d\n", __func__, mute);

	if (mute) {
		val = 0;
	} else {
		/* find which DACs are ON */
		val = snd_soc_read(codec, ES_DAC_CTRL);
	}

	return snd_soc_update_bits(codec, ES_DAC_DIG_EN, ES_DAC0_LEFT_EN_MASK |
		ES_DAC0_RIGHT_EN_MASK | ES_DAC1_LEFT_EN_MASK |
		ES_DAC1_RIGHT_EN_MASK, val);
}

struct snd_soc_dai_driver es755_dai[] = {
#if defined(CONFIG_SND_SOC_ES_I2S)
	{
		.name = "earSmart-porta",
		.id = ES_I2S_PORTA,
		.playback = {
			.stream_name = "PORTA Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES_RATES,
			.formats = ES_FORMATS,
		},
		.capture = {
			.stream_name = "PORTA Capture",
			.channels_min = 1,
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
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES_RATES,
			.formats = ES_FORMATS,
		},
		.capture = {
			.stream_name = "PORTB Capture",
			.channels_min = 1,
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
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES_RATES,
			.formats = ES_FORMATS,
		},
		.capture = {
			.stream_name = "PORTC Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES_RATES,
			.formats = ES_FORMATS,
		},
		.ops = &escore_i2s_port_dai_ops,
	},
#endif
#if defined(CONFIG_SND_SOC_ES_SLIM)
	{
		.name = "es755-slim-rx1",
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
		.name = "es755-slim-tx1",
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
		.name = "es755-slim-rx2",
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
		.name = "es755-slim-tx2",
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
		.name = "es755-slim-rx3",
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
		.name = "es755-slim-tx3",
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

#if defined(CONFIG_SND_SOC_ES_VS)
static const char * const es755_vs_event_texts[] = {
	"No Event", "Codec Event", "VS Keyword Event",
};
static const struct soc_enum es755_vs_event_enum =
	SOC_ENUM_SINGLE(ES_VOICE_SENSE_EVENT, 0,
			ARRAY_SIZE(es755_vs_event_texts),
			es755_vs_event_texts);

static const char * const es755_vs_training_mode_texts[] = {
	"Detect Keyword", "N/A", "Train User-defined Keyword",
};

static const struct soc_enum es755_vs_training_mode_enum =
	SOC_ENUM_SINGLE(ES_VOICE_SENSE_TRAINING_MODE, 0,
			ARRAY_SIZE(es755_vs_training_mode_texts),
			es755_vs_training_mode_texts);

static const char * const es755_vs_training_status_texts[] = {
	"Busy", "Success", "Utterance Long",
	"Utterance Short", "Verification Failed",
};
static const struct soc_enum es755_vs_training_status_enum =
	SOC_ENUM_SINGLE(ES_VOICE_SENSE_TRAINING_STATUS, 0,
			ARRAY_SIZE(es755_vs_training_status_texts),
			es755_vs_training_status_texts);

static const char * const es755_vs_training_record_texts[] = {
	"Previous Keyword", "Keyword_1", "Keyword_2",
	"Keyword_3", "Keyword_4", "Keyword_5",
};
static const struct soc_enum es755_vs_training_record_enum =
	SOC_ENUM_SINGLE(ES_VOICE_SENSE_TRAINING_RECORD, 0,
			ARRAY_SIZE(es755_vs_training_record_texts),
			es755_vs_training_record_texts);

static const char * const es755_vs_stored_keyword_texts[] = {
	"Put", "Get", "Clear"
};
static const struct soc_enum es755_vs_stored_keyword_enum =
	SOC_ENUM_SINGLE(ES_VS_STORED_KEYWORD, 0,
			ARRAY_SIZE(es755_vs_stored_keyword_texts),
			es755_vs_stored_keyword_texts);

/* Use for NULL "get" handler */
static int es755_get_null_control_enum(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	/* Do Nothing */
	return 0;
}

static struct snd_kcontrol_new es755_voice_sense_snd_controls[] = {
	SOC_ENUM_EXT("Voice Sense Status",
		     es755_vs_event_enum,
		     escore_get_control_enum, NULL),
	SOC_ENUM_EXT("Voice Sense Training Mode",
			 es755_vs_training_mode_enum,
			 escore_get_control_enum, escore_put_control_enum),
	SOC_ENUM_EXT("Voice Sense Training Status",
		     es755_vs_training_status_enum,
		     escore_get_control_enum, NULL),
	SOC_ENUM_EXT("Voice Sense Training Record",
		     es755_vs_training_record_enum,
		     es755_get_null_control_enum, escore_put_control_enum),
	SOC_ENUM_EXT("Voice Sense Stored Keyword",
		     es755_vs_stored_keyword_enum,
		     es755_get_null_control_enum, escore_put_control_enum),
	SOC_SINGLE_EXT("Voice Sense Detect Sensitivity",
			ES_VOICE_SENSE_DETECTION_SENSITIVITY, 0, 10, 0,
			escore_get_control_value,
			escore_put_control_value),
	SOC_SINGLE_EXT("Voice Activity Detect Sensitivity",
			ES_VOICE_ACTIVITY_DETECTION_SENSITIVITY, 0, 10, 0,
			escore_get_control_value,
			escore_put_control_value),
	SOC_SINGLE_EXT("Continuous Voice Sense Preset",
		       ES_CVS_PRESET, 0, 65535, 0,
		       escore_get_cvs_preset_value,
		       escore_put_cvs_preset_value),
};

int es755_start_int_osc(void)
{
	int rc = 0;
	int retry = MAX_RETRY_TO_SWITCH_TO_LOW_POWER_MODE;

	dev_info(escore_priv.dev, "%s()\n", __func__);

	/* Start internal Osc. */
	rc = escore_write(NULL, ES_VS_INT_OSC_MEASURE_START, 0);
	if (rc) {
		dev_err(escore_priv.dev,
			"%s(): OSC Measure Start fail\n",
			__func__);
		return rc;
	}

	/* Poll internal Osc. status */
	do {
		/*
		 * Wait 20ms each time before reading
		 * up to 100ms
		 */
		msleep(20);
		rc = escore_read(NULL, ES_VS_INT_OSC_MEASURE_STATUS);

		if (rc < 0) {
			dev_err(escore_priv.dev,
				"%s(): OSC Measure Read Status fail\n",
				__func__);
			break;
		}
		dev_dbg(escore_priv.dev,
			"%s(): OSC Measure Status = 0x%04x\n",
			__func__, rc);
	} while (rc && --retry);

	if (rc > 0) {
		dev_err(escore_priv.dev,
			"%s(): Unexpected OSC Measure Status = 0x%04x\n",
			__func__, rc);
		dev_err(escore_priv.dev,
			"%s(): Can't switch to Low Power Mode\n",
			__func__);
	}

	return rc;
}

int es755_wakeup(void)
{
	u32 cmd = ES_SYNC_CMD << 16;
	u32 rsp;
	int rc = 0;

	dev_info(escore_priv.dev, "%s()\n", __func__);

#if defined(CONFIG_SND_SOC_ES_WAKEUP_GPIO) || \
	defined(CONFIG_SND_SOC_ES_WAKEUP_UART)

	/* Enable the clocks */
	if (escore_priv.pdata->esxxx_clk_cb)
		escore_priv.pdata->esxxx_clk_cb(1);

	/* Setup for clock stablization delay */
	usleep_range(1000, 1000);

	/* Toggle the wakeup pin H->L the L->H */
	if (escore_priv.pdata->wakeup_gpio != -1) {
		gpio_set_value(escore_priv.pdata->wakeup_gpio, 0);
		usleep_range(1000, 1000);
		gpio_set_value(escore_priv.pdata->wakeup_gpio, 1);
	} else {
#ifdef CONFIG_SND_SOC_ES_WAKEUP_UART
		char wakeup_byte = 'A';
		rc = escore_uart_write(&escore_priv, &wakeup_byte, 1);
		if (rc < 0) {
			dev_err(escore_priv.dev, "%s() UART wakeup failed:%d\n",
				__func__, rc);
		/* Probably should do somethting like reset, but... */
		}
#else
		dev_err(escore_priv.dev, "%s() No wakeup mechanism\n",
			__func__);
#endif
	}
	/* Give the device time to "wakeup" */
	msleep(30);
	/* Send a Sync command - not sure about the response */
	rc = escore_priv.bus.ops.cmd(&escore_priv, cmd, &rsp);
	if (rc < 0)
		dev_info(escore_priv.dev,
				"%s() - failed sync cmd resume\n", __func__);
	if (cmd != rsp)
		dev_info(escore_priv.dev,
				"%s() - failed sync rsp resume\n", __func__);
#endif
	return rc;
}

/* Power state transition */
int es755_power_transition(int next_power_state,
				unsigned int set_power_state_cmd)
{
	int rc = 0;

	dev_info(escore_priv.dev, "%s()\n", __func__);

	/* Power state transition */
	while (next_power_state != escore_priv.escore_power_state) {
		switch (escore_priv.escore_power_state) {
		case ES_SET_POWER_STATE_SLEEP:
			/* Wakeup Chip */
			rc = es755_wakeup();
			if (rc) {
				dev_err(escore_priv.dev,
					"%s(): es755 wakeup failed\n",
					__func__);
				goto es755_power_transition_exit;
			}
			escore_priv.escore_power_state =
					ES_SET_POWER_STATE_NORMAL;
			break;

		case ES_SET_POWER_STATE_NORMAL:
			/* Either switch to Sleep or VS Overlay mode */
			if (next_power_state == ES_SET_POWER_STATE_SLEEP)
				escore_priv.escore_power_state =
					ES_SET_POWER_STATE_SLEEP;
			else
				escore_priv.escore_power_state =
					ES_SET_POWER_STATE_VS_OVERLAY;

			rc = escore_write(NULL, set_power_state_cmd,
					escore_priv.escore_power_state);
			if (rc) {
				dev_err(escore_priv.dev,
					"%s(): Power state cmd write failed\n",
					__func__);
				escore_priv.escore_power_state =
					ES_SET_POWER_STATE_NORMAL;
				goto es755_power_transition_exit;
			}

			/* VS fw download */
			if (escore_priv.escore_power_state ==
				ES_SET_POWER_STATE_VS_OVERLAY) {
				/* wait es755 SBL mode */
				msleep(50);
				rc = escore_vs_load(&escore_priv);
				if (rc) {
					dev_err(escore_priv.dev,
						"%s(): vs fw downlaod failed\n",
						__func__);
					goto es755_power_transition_exit;
				}
				/* Enable Voice Sense Event INTR to Host */
				rc = escore_write(NULL, ES_EVENT_RESPONSE,
						  ES_SYNC_INTR_RISING_EDGE);
				if (rc) {
					dev_err(escore_priv.dev,
						"%s(): Enable Event Intr fail\n",
						__func__);
					goto es755_power_transition_exit;
				}
			}
			break;

		case ES_SET_POWER_STATE_VS_OVERLAY:
			/* Either switch to VS low power or Normal mode */
			if (next_power_state == ES_SET_POWER_STATE_VS_LOWPWR) {
				/* Start internal oscilator */
				rc = es755_start_int_osc();
				if (rc)
					goto es755_power_transition_exit;

				escore_priv.escore_power_state =
					ES_SET_POWER_STATE_VS_LOWPWR;
			} else
				escore_priv.escore_power_state =
					ES_SET_POWER_STATE_NORMAL;

			rc = escore_write(NULL, set_power_state_cmd,
					escore_priv.escore_power_state);
			if (rc) {
				dev_err(escore_priv.dev,
					"%s(): Power state cmd write failed\n",
					__func__);
				escore_priv.escore_power_state =
					ES_SET_POWER_STATE_VS_OVERLAY;
				goto es755_power_transition_exit;
			}
			break;

		case ES_SET_POWER_STATE_VS_LOWPWR:
			/* Wakeup Chip */
			rc = es755_wakeup();
			if (rc) {
				dev_err(escore_priv.dev,
					"%s(): es755 wakeup failed\n",
					__func__);
				goto es755_power_transition_exit;
			}
			escore_priv.escore_power_state =
				ES_SET_POWER_STATE_VS_OVERLAY;
			break;

		default:
			dev_err(escore_priv.dev,
				"%s(): Unsupported state in es755\n",
				__func__);
			rc = -EINVAL;
			goto es755_power_transition_exit;
		}
		dev_dbg(escore_priv.dev,
			"%s(): Current state = %d, val=%d\n",
			__func__, escore_priv.escore_power_state,
			next_power_state);
	}

	dev_dbg(escore_priv.dev, "%s(): Power state change successful\n",
		__func__);
es755_power_transition_exit:
	return rc;
}

static int es755_get_power_control_enum(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e =
		(struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	unsigned int value;

	/* Don't read if already in Sleep or VS low power Mode */
	if ((escore_priv.escore_power_state == ES_SET_POWER_STATE_SLEEP) ||
	(escore_priv.escore_power_state == ES_SET_POWER_STATE_MP_SLEEP) ||
	(escore_priv.escore_power_state == ES_SET_POWER_STATE_VS_LOWPWR))
		value = escore_priv.escore_power_state;
	else
		value = escore_read(NULL, reg);

	ucontrol->value.enumerated.item[0] = value;

	return 0;
}

static int es755_put_power_control_enum(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	unsigned int value;
	int rc = 0;

	value = ucontrol->value.enumerated.item[0];

	/* Not supported */
	if (!value ||
		(value == ES_SET_POWER_STATE_MP_SLEEP) ||
		(value == ES_SET_POWER_STATE_MP_CMD)) {
		dev_err(escore_priv.dev,
			"%s(): Unsupported state\n",
			__func__);
		return -EINVAL;
	}

	rc = es755_power_transition(value, reg);
	if (rc) {
		dev_err(escore_priv.dev,
			"%s(): es755_power_transition() failed\n",
			__func__);
	}

	return rc;
}

static int es755_put_preset_value(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	int rc = 0;

	value = ucontrol->value.integer.value[0];

	rc = escore_write(NULL, reg, value);
	if (rc) {
		dev_err(escore_priv.dev, "%s(): Set Preset failed\n",
			__func__);
		return rc;
	}

	escore_priv.preset = value;

	return rc;
}

static int es755_get_preset_value(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = escore_priv.preset;

	return 0;
}

static int es755_get_rdb_size(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] =
				escore_priv.datablock_dev.rdb_read_count;

	return 0;
}

static int es755_get_event_status(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = escore_priv.escore_event_type;

	/* Reset the event status after read */
	escore_priv.escore_event_type = ES_NO_EVENT;

	return 0;
}

static const char * const es755_vs_power_state_texts[] = {
	"None", "Sleep", "MP_Sleep", "MP_Cmd", "Normal", "Overlay", "Low_Power"
};

static const struct soc_enum es755_vs_power_state_enum =
	SOC_ENUM_SINGLE(ES_POWER_STATE, 0,
			ARRAY_SIZE(es755_vs_power_state_texts),
			es755_vs_power_state_texts);

static struct snd_kcontrol_new es755_snd_controls[] = {
	SOC_ENUM_EXT("ES755 Power State", es755_vs_power_state_enum,
		     es755_get_power_control_enum,
		     es755_put_power_control_enum),
	SOC_SINGLE_EXT("Preset",
		       ES_PRESET, 0, 65535, 0, es755_get_preset_value,
		       es755_put_preset_value),
	SOC_SINGLE_EXT("ES755 Get Event Status",
		       SND_SOC_NOPM, 0, 65535, 0, es755_get_event_status, NULL),
	SOC_SINGLE_EXT("ES755 Get RDB data size",
		       SND_SOC_NOPM, 0, 65535, 0,
		       es755_get_rdb_size, NULL),
};

static int es_voice_sense_add_snd_soc_controls(struct snd_soc_codec *codec)
{
	int ret;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
	ret = snd_soc_add_codec_controls(codec, es755_voice_sense_snd_controls,
			ARRAY_SIZE(es755_voice_sense_snd_controls));
#else
	ret = snd_soc_add_controls(codec, es755_voice_sense_snd_controls,
			ARRAY_SIZE(es755_voice_sense_snd_controls));
#endif
	return ret;
}

#endif /* CONFIG_SND_SOC_ES_VS */


#define es755_set_bias_level NULL

static int es755_codec_probe(struct snd_soc_codec *codec)
{
	int ret;
	struct escore_priv *es755 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s()\n", __func__);
	es755->codec = codec;

	codec->control_data = snd_soc_codec_get_drvdata(codec);
	ret = es_d300_add_snd_soc_controls(codec);
	if (ret) {
		dev_err(codec->dev,
			"%s(): es_d300_snd_controls failed\n", __func__);
		return ret;
	}
	ret = es_analog_add_snd_soc_controls(codec);
	if (ret) {
		dev_err(codec->dev,
			"%s(): es_analog_snd_controls failed\n", __func__);
		return ret;
	}
	ret = es_d300_add_snd_soc_dapm_controls(codec);
	if (ret) {
		dev_err(codec->dev,
			"%s(): es_d300_dapm_widgets failed\n", __func__);
		return ret;
	}
	ret = es_analog_add_snd_soc_dapm_controls(codec);
	if (ret) {
		dev_err(codec->dev,
			"%s(): es_analog_dapm_widgets failed\n", __func__);
		return ret;
	}
	ret = es_d300_add_snd_soc_route_map(codec);
	if (ret) {
		dev_err(codec->dev,
			"%s(): es_d300_add_routes failed\n", __func__);
		return ret;
	}
	ret = es_analog_add_snd_soc_route_map(codec);
	if (ret) {
		dev_err(codec->dev,
			"%s(): es_analog_add_routes failed\n", __func__);
		return ret;
	}

#if defined(CONFIG_SND_SOC_ES_VS)
	ret = es_voice_sense_add_snd_soc_controls(codec);
	if (ret) {
		dev_err(codec->dev,
			"%s(): es755 VS snd control failed\n", __func__);
		return ret;
	}

	device_set_wakeup_capable(codec->dev, true);

	/* TBD - Need to move this to a ALSA control function */
	escore_pm_vs_enable(&escore_priv, true);

#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
	ret = snd_soc_add_codec_controls(codec, es755_snd_controls,
			ARRAY_SIZE(es755_snd_controls));
#else
	ret = snd_soc_add_controls(codec, es755_snd_controls,
			ARRAY_SIZE(es755_snd_controls));
#endif

	ret = es_d300_fill_cmdcache(escore_priv.codec);
	if (ret) {
		dev_err(codec->dev,
			"%s(): Cache initialization failed\n", __func__);
		return ret;
	}

	return ret;
}

static int  es755_codec_remove(struct snd_soc_codec *codec)
{

	pr_debug("%s(): Codec removed\n", __func__);
	return 0;
}

static unsigned int es755_codec_read(struct snd_soc_codec *codec,
				unsigned int reg)
{
	struct escore_priv *escore = &escore_priv;
	struct escore_api_access *api_access;
	u32 cmd;
	u32 resp;
	unsigned int msg_len;
	int rc;

	if (reg > ES_MAX_REGISTER) {
		/*dev_err(codec->dev, "read out of range reg %d", reg);*/
		return 0;
	}

	if (!escore->reg_cache[reg].is_volatile)
		return escore->reg_cache[reg].value & 0xff;

	api_access = &escore->api_access[ES_CODEC_VALUE];
	msg_len = api_access->read_msg_len;
	memcpy((char *)&cmd, (char *)api_access->read_msg, msg_len);

	rc = escore_cmd(escore, cmd | reg<<8, &resp);
	if (rc < 0) {
		dev_err(codec->dev, "codec reg read err %d()", rc);
		return rc;
	}
	cmd = escore->bus.last_response;

	return cmd & 0xff;

}

static int es755_codec_write(struct snd_soc_codec *codec, unsigned int reg,
	unsigned int value)
{
	struct escore_priv *escore = &escore_priv;
	int ret;

	if (reg > ES_MAX_REGISTER) {
		/*dev_err(codec->dev, "write out of range reg %d", reg);*/
		return 0;
	}

	ret = escore_write(codec, ES_CODEC_VALUE, reg<<8 | value);
	if (ret < 0) {
		dev_err(codec->dev, "codec reg %x write err %d\n",
			reg, ret);
		return ret;
	}
	escore->reg_cache[reg].value = value;
	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
struct snd_soc_codec_driver soc_codec_dev_es755 = {
	.probe =	es755_codec_probe,
	.remove =	es755_codec_remove,
	.read =		es755_codec_read,
	.write =	es755_codec_write,
	.set_bias_level =	es755_set_bias_level,
	.idle_bias_off = true,
};
#else
struct snd_soc_codec_driver soc_codec_dev_es755 = {
	.probe =	es755_codec_probe,
	.remove =	es755_codec_remove,
	.read =		es755_codec_read,
	.write =	es755_codec_write,
	.set_bias_level =	es755_set_bias_level,
};
#endif

int es755_set_streaming(struct escore_priv *escore, int value)
{
	u32 resp;
	return escore_cmd(escore,
		es755_streaming_cmds[escore->streamdev.intf] | value, &resp);
}

int es755_set_datalogging(struct escore_priv *escore, int value)
{
	u32 resp;
	return escore_cmd(escore, value, &resp);
}

void es755_slim_setup(struct escore_priv *escore_priv)
{
	int i;
	int ch_cnt;

	escore_priv->init_slim_slave(escore_priv);

	/* allocate ch_num array for each DAI */
	for (i = 0; i < ARRAY_SIZE(es755_dai); i++) {
		switch (es755_dai[i].id) {
		case ES_SLIM_1_PB:
		case ES_SLIM_2_PB:
		case ES_SLIM_3_PB:
			ch_cnt = es755_dai[i].playback.channels_max;
			break;
		case ES_SLIM_1_CAP:
		case ES_SLIM_2_CAP:
		case ES_SLIM_3_CAP:
			ch_cnt = es755_dai[i].capture.channels_max;
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

int es755_detect(struct snd_soc_codec *codec, struct snd_soc_jack *jack)
{
	struct escore_priv *escore = snd_soc_codec_get_drvdata(codec);
	struct esxxx_accdet_config *accdet_cfg = &escore->pdata->accdet_cfg;
	union es755_accdet_reg accdet_reg;
	u32 cmd;
	u32 resp;
	int rc;

	/* Setup the Event response */
	cmd = (ES_SET_EVENT_RESP << 16) | escore->pdata->irq_type;
	rc = escore_cmd(escore, cmd, &resp);
	if (rc < 0) {
		pr_err("%s(): Error in setting event response\n", __func__);
		goto detect_exit;
	}

	accdet_reg.value = 0;
	accdet_reg.fields.plug_det_fsm = accdet_cfg->plug_det_enabled & (0x1);
	accdet_reg.fields.debounce_timer = accdet_cfg->debounce_timer & (0x3);

	/* Setup the debounce timer for plug event */
	cmd = (ES_ACCDET_CONFIG_CMD << 16) | (accdet_reg.value);
	rc = escore_cmd(escore, cmd, &resp);
	if (rc < 0) {
		pr_err("%s(): Error in setting debounce timer\n", __func__);
		goto detect_exit;
	}

	escore->jack = jack;

detect_exit:
	return rc;
}
EXPORT_SYMBOL_GPL(es755_detect);


static struct esxxx_platform_data *es755_populate_dt_pdata(struct device *dev)
{
	struct esxxx_platform_data *pdata;
	struct property *prop;
	struct es755_btn_cfg  *es755_btn_cfg;

	u8 *temp;
	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "could not allocate memory for platform data\n");
		return NULL;
	}

	es755_btn_cfg = devm_kzalloc(dev, sizeof(*es755_btn_cfg), GFP_KERNEL);
	if (!es755_btn_cfg) {
		dev_err(dev, "could not allocate memory for es755_btn_cfg\n");
		return NULL;
	}

	pdata->priv = (struct es755_btn_cfg *)es755_btn_cfg;
	pdata->reset_gpio = of_get_named_gpio(dev->of_node,
			"adnc,reset-gpio", 0);
	if (pdata->reset_gpio < 0) {
		dev_err(dev, "Looking up %s property in node %s failed %d\n",
				"adnc,reset-gpio", dev->of_node->full_name,
				pdata->reset_gpio);
		pdata->reset_gpio = -1;
	}
	dev_dbg(dev, "%s: reset gpio %d", __func__, pdata->reset_gpio);

	pdata->wakeup_gpio = of_get_named_gpio(dev->of_node,
						"adnc,wakeup-gpio", 0);
	if (pdata->wakeup_gpio < 0) {
		dev_err(dev, "Looking up %s property in node %s failed %d\n",
				"adnc,wakeup-gpio", dev->of_node->full_name,
				pdata->wakeup_gpio);
		pdata->wakeup_gpio = -1;
	}
	dev_dbg(dev, "%s: wakeup gpio %d", __func__, pdata->wakeup_gpio);
	pdata->uart_gpio = of_get_named_gpio(dev->of_node,
			"adnc,int-gpio", 0);
	if (pdata->uart_gpio < 0) {
		dev_err(dev, "Looking up %s property in node %s failed %d\n",
				"adnc,uart-gpio", dev->of_node->full_name,
				pdata->uart_gpio);
		pdata->uart_gpio = -1;
	}
	dev_dbg(dev, "%s(): int gpio %d",
			__func__, pdata->uart_gpio);

	printk(KERN_INFO "%s(): int gpio %d",
			__func__, pdata->uart_gpio);

	pdata->gpioa_gpio = of_get_named_gpio(dev->of_node,
			"adnc,gpioa-gpio", 0);
	if (pdata->gpioa_gpio < 0) {
		dev_err(dev, "Looking up %s property in node %s failed %d\n",
				"adnc,gpioa-gpio", dev->of_node->full_name,
				pdata->gpioa_gpio);
		pdata->gpioa_gpio = -1;
	}
	dev_dbg(dev, "%s: gpioa_gpio %d", __func__, pdata->gpioa_gpio);

	pdata->gpiob_gpio = of_get_named_gpio(dev->of_node,
						"adnc,gpiob-gpio", 0);
	if (pdata->gpiob_gpio < 0) {
		dev_err(dev, "Looking up %s property in node %s failed %d\n",
				"adnc,gpiob-gpio", dev->of_node->full_name,
				pdata->gpiob_gpio);
		pdata->gpiob_gpio = -1;
	}
	dev_dbg(dev, "%s: gpiob_gpio %d", __func__, pdata->gpiob_gpio);

	prop = of_find_property(dev->of_node, "adnc,enable_hs_uart_intf", NULL);
	temp =	(u8 *)prop->value;
	if (temp[3] == 1)
		pdata->enable_hs_uart_intf = true;
	else
		pdata->enable_hs_uart_intf = false;


	prop = of_find_property(dev->of_node, "adnc,ext_clk_rate", NULL);
	temp = (u8 *)prop->value;
	if (temp[3] != 0)
		pdata->ext_clk_rate = temp[3];

	prop = of_find_property(dev->of_node, "adnc,btn_press_settling_time",
			NULL);
	temp = (u8 *)prop->value;
	if (temp[3] != 0)
		es755_btn_cfg->btn_press_settling_time = temp[3];

	prop = of_find_property(dev->of_node, "adnc,btn_press_polling_rate",
			NULL);
	temp = (u8 *)prop->value;
	if (temp[3] != 0)
		es755_btn_cfg->btn_press_polling_rate = temp[3];

	prop = of_find_property(dev->of_node, "adnc,btn_press_det_act", NULL);
	temp = (u8 *)prop->value;
	if (temp[3] != 0)
		es755_btn_cfg->btn_press_det_act = temp[3];

	prop = of_find_property(dev->of_node, "adnc,double_btn_timer", NULL);
	temp = (u8 *)prop->value;
	if (temp[3] != 0)
		es755_btn_cfg->double_btn_timer = temp[3];

	prop = of_find_property(dev->of_node, "adnc,mic_det_settling_timer",
			NULL);
	temp = (u8 *)prop->value;
	if (temp[3] != 0)
		es755_btn_cfg->mic_det_settling_timer = temp[3];

	prop = of_find_property(dev->of_node, "adnc,long_btn_timer", NULL);
	temp = (u8 *)prop->value;
	if (temp[3] != 0)
		es755_btn_cfg->long_btn_timer = temp[3];

	prop = of_find_property(dev->of_node, "adnc,adc_btn_mute", NULL);
	temp = (u8 *)prop->value;
	if (temp[3] != 0)
		es755_btn_cfg->adc_btn_mute = temp[3];

	prop = of_find_property(dev->of_node, "adnc,valid_levels", NULL);
	temp =	(u8 *)prop->value;
	if (temp[3] != 0)
		es755_btn_cfg->valid_levels = temp[3];

	prop = of_find_property(dev->of_node, "adnc,impd_det_timer", NULL);
	temp = (u8 *)prop->value;
	if (temp[3] != 0)
		es755_btn_cfg->impd_det_timer = temp[3];

	prop = of_find_property(dev->of_node, "adnc,irq_type", NULL);
	temp = (u8 *)prop->value;
	if (temp[3] != 0)
		pdata->irq_type = temp[3];

	prop = of_find_property(dev->of_node, "adnc,debounce_timer", NULL);
	temp = (u8 *)prop->value;
	if (temp[3] != 0)
		pdata->accdet_cfg.debounce_timer = temp[3];

	prop = of_find_property(dev->of_node, "adnc,plug_det_enabled", NULL);
	temp = (u8 *)prop->value;
	if (temp[3] != 0)
		pdata->accdet_cfg.plug_det_enabled = temp[3];

	prop = of_find_property(dev->of_node, "adnc,mic_det_enabled", NULL);
	temp = (u8 *)prop->value;
	if (temp[3] != 0)
		pdata->accdet_cfg.mic_det_enabled = temp[3];

	prop = of_find_property(dev->of_node, "adnc,cmd_comp_mode", NULL);
	temp = (u8 *)prop->value;
	if (temp[3] != 0)
		pdata->cmd_comp_mode = temp[3];

/*TODO:Acce detect GPIO*/
	/*
pdata->gpiob_gpio = of_get_named_gpio(dev->of_node,
						"adnc,gpiob-gpio", 0);
	if (pdata->gpiob_gpio < 0) {
		dev_err(dev, "Looking up %s property in node %s failed %d\n",
				"adnc,gpiob-gpio", dev->of_node->full_name,
				pdata->gpiob_gpio);
		pdata->gpiob_gpio = -1;
	}
	dev_dbg(dev, "%s: gpiob_gpio %d", __func__, pdata->gpiob_gpio);
*/
#ifdef CONFIG_QPNP_CLKDIV
	codec_clk = qpnp_clkdiv_get(dev, "es755-mclk");
#else
	codec_clk = clk_get(dev, "es755-mclk");
#endif
	if (IS_ERR(codec_clk)) {
		dev_err(dev,
				"%s: Failed to request es755 mclk from pmic %ld\n",
				__func__, PTR_ERR(codec_clk));
		pdata->esxxx_clk_cb = NULL;
	} else
		pdata->esxxx_clk_cb = es755_clk_ctl;

	return pdata;
}


static int es755_mic_config(struct escore_priv *escore)
{
	struct snd_soc_codec *codec = escore->codec;
	union es755_accdet_reg accdet_reg;
	struct esxxx_accdet_config accdet_cfg = escore->pdata->accdet_cfg;

	accdet_reg.value = 0;

	accdet_reg.fields.mic_det_fsm = 1;
	accdet_reg.fields.plug_det_fsm = accdet_cfg.plug_det_enabled & (0x1);
	accdet_reg.fields.debounce_timer = accdet_cfg.debounce_timer & (0x3);

	/* This allows detection of both type of headsets: LRGM and LRMG */
	accdet_reg.fields.mg_sel_force = accdet_cfg.mic_det_enabled & (0x1);

	pr_debug("%s()\n", __func__);

	return escore_write(codec, ES_ACCDET_CONFIG, accdet_reg.value);
}

static int es755_button_config(struct escore_priv *escore)
{
	struct esxxx_platform_data *pdata = escore->pdata;
	struct es755_btn_cfg *btn_cfg;
	union es755_btn_ctl1 btn_ctl1;
	union es755_btn_ctl2 btn_ctl2;
	union es755_btn_ctl3 btn_ctl3;
	union es755_btn_ctl4 btn_ctl4;
	u8 invalid = (u8) -1;
	u8 update = 0;
	int rc = 0;

	btn_cfg = (struct es755_btn_cfg *)pdata->priv;

	btn_ctl1.value = 0;
	btn_ctl2.value = 0;
	btn_ctl3.value = 0;
	btn_ctl4.value = 0;

	/* Config for Button Control 1 */
	if (btn_cfg->btn_press_settling_time != invalid) {
		btn_ctl1.fields.btn_press_settling_time =
			(btn_cfg->btn_press_settling_time & 0x7);
		update = 1;
	}
	if (btn_cfg->btn_press_polling_rate != invalid) {
		btn_ctl1.fields.btn_press_polling_rate =
			(btn_cfg->btn_press_polling_rate & 0x3);
		update = 1;
	}
	if (btn_cfg->btn_press_det_act != invalid) {
		btn_ctl1.fields.btn_press_det =
			(btn_cfg->btn_press_det_act & 0x1);
		update = 1;
	}

	if (update) {
		rc = escore_write(NULL, ES_BUTTON_CTRL1, btn_ctl1.value);
		if (rc < 0) {
			pr_err("%s(): Error setting button control 1\n",
					__func__);
			goto btn_cfg_exit;
		}
		update = 0;
	}

	/* Config for Button Control 2 */
	if (btn_cfg->double_btn_timer != invalid) {
		btn_ctl2.fields.double_btn_timer =
			(btn_cfg->double_btn_timer & 0xf);
		update = 1;
	}
	if (btn_cfg->mic_det_settling_timer != invalid) {
		btn_ctl2.fields.mic_det_settling_timer =
			(btn_cfg->mic_det_settling_timer & 0x3);
		update = 1;
	}
	if (update) {
		rc = escore_write(NULL, ES_BUTTON_CTRL2, btn_ctl2.value);
		if (rc < 0) {
			pr_err("%s(): Error setting button control 2\n",
					__func__);
			goto btn_cfg_exit;
		}
		update = 0;
	}

	/* Config for Button Control 3 */
	if (btn_cfg->long_btn_timer != invalid) {
		btn_ctl3.fields.long_btn_timer =
			(btn_cfg->long_btn_timer & 0xf);
		update = 1;
	}
	if (btn_cfg->adc_btn_mute != invalid) {
		btn_ctl3.fields.adc_btn_mute =
			(btn_cfg->adc_btn_mute & 0x1);
		update = 1;
	}
	if (update) {
		rc = escore_write(NULL, ES_BUTTON_CTRL3, btn_ctl3.value);
		if (rc < 0) {
			pr_err("%s(): Error setting button control 3\n",
					__func__);
			goto btn_cfg_exit;
		}
		update = 0;
	}

	/* Config for Button Control 4 */
	if (btn_cfg->valid_levels != invalid) {
		btn_ctl4.fields.valid_levels =
			(btn_cfg->valid_levels & 0x3f);
		update = 1;
	}
	if (btn_cfg->impd_det_timer != invalid) {
		btn_ctl4.fields.impd_det_timer =
			(btn_cfg->impd_det_timer & 0x3);
		update = 1;
	}
	if (update) {
		rc = escore_write(NULL, ES_BUTTON_CTRL4, btn_ctl4.value);
		if (rc < 0) {
			pr_err("%s(): Error setting button control 4\n",
					__func__);
			goto btn_cfg_exit;
		}
		update = 0;
	}

btn_cfg_exit:
	return rc;

}

static int es755_codec_intr(struct notifier_block *self, unsigned long action,
		void *dev)
{
	struct escore_priv *escore = (struct escore_priv *)dev;
	union es755_accdet_status_reg accdet_reg;
	int value;
	int rc = 0;
	u8 impd_level;

	pr_debug("%s(): Event: 0x%04x\n", __func__, (u32)action);

	if (action & ES_CODEC_INTR_EVENT) {
		value = escore_read(NULL, ES_GET_SYS_INTERRUPT);
		if (value < 0) {
			pr_err("%s(): Get System Event failed\n", __func__);
			goto intr_exit;
		}

		if (ES_PLUG_EVENT(value)) {

			pr_debug("%s(): Plug event\n", __func__);
			/* Enable MIC Detection */
			rc = es755_mic_config(escore);
			if (rc < 0) {
				pr_err("%s(): MIC config failed\n", __func__);
				goto intr_exit;
			}
		} else if (ES_MICDET_EVENT(value)) {

			value = escore_read(NULL, ES_GET_ACCDET_STATUS);
			if (value < 0) {
				pr_err("%s(): Accessory detect status failed\n",
						__func__);
				goto intr_exit;
			}

			accdet_reg.value = value;
			impd_level = accdet_reg.fields.impd_level;

			if (impd_level) {
				pr_debug("%s(): Headset detected\n", __func__);
				if (impd_level < MIC_IMPEDANCE_LEVEL_LRGM)
					pr_debug("LRMG Headset\n");
				else if (impd_level == MIC_IMPEDANCE_LEVEL_LRGM)
					pr_debug("LRGM Headset\n");
				else {
					pr_err("Invalid type:%d\n", impd_level);
					rc = -EINVAL;
					goto intr_exit;
				}

				snd_soc_jack_report(escore->jack,
					SND_JACK_HEADSET, JACK_DET_MASK);

				rc = es755_button_config(escore);
				if (rc < 0)
					goto intr_exit;

			} else {
				pr_debug("%s(): Headphone detected\n",
						__func__);
				snd_soc_jack_report(escore->jack,
					SND_JACK_HEADPHONE,
					JACK_DET_MASK);
			}

			pr_debug("%s(): AccDet status 0x%04x\n", __func__,
					value);
		} else if (ES_BUTTON_PRESS_EVENT(value)) {
			pr_debug("%s(): Button event\n", __func__);

		} else if (ES_BUTTON_RELEASE_EVENT(value)) {
			pr_debug("%s(): Button released\n", __func__);
			value = escore_read(NULL, ES_GET_ACCDET_STATUS);
			if (value < 0) {
				pr_err("%s(): Accessory detect status failed\n",
						__func__);
				goto intr_exit;
			}

			accdet_reg.value = value;

			pr_debug("Impd:%d\n", accdet_reg.fields.impd_level);

			switch (accdet_reg.fields.impd_level) {
			case 0:
				snd_soc_jack_report(escore->jack,
						SND_JACK_BTN_0,
						JACK_DET_MASK);
				/* Button release event must be sent */
				snd_soc_jack_report(escore->jack,
						0, SND_JACK_BTN_0);

				break;
			case 2:
				snd_soc_jack_report(escore->jack,
						SND_JACK_BTN_1,
						JACK_DET_MASK);
				/* Button release event must be sent */
				snd_soc_jack_report(escore->jack,
						0, SND_JACK_BTN_1);
				break;
			case 3:
				snd_soc_jack_report(escore->jack,
						SND_JACK_BTN_2,
						JACK_DET_MASK);
				/* Button release event must be sent */
				snd_soc_jack_report(escore->jack,
						0, SND_JACK_BTN_2);
				break;
			default:
				pr_debug("No report of event\n");
				break;
			}
		} else if (ES_UNPLUG_EVENT(value)) {
			pr_debug("%s(): Unplug detected\n", __func__);
			snd_soc_jack_report(escore->jack, 0, JACK_DET_MASK);
		}
	}

intr_exit:
	return NOTIFY_OK;
}

irqreturn_t es755_cmd_completion_isr(int irq, void *data)
{
	struct escore_priv *escore = (struct escore_priv *)data;

	BUG_ON(!escore);

	if (escore->cmd_compl_mode == ES_CMD_COMP_INTR) {
		pr_debug("%s(): API completion. Jiffies:%lu\n",
				__func__, jiffies);
		complete(&escore->cmd_compl);
	} else {
		pr_err("%s(): Invalid API Interrupt\n", __func__);
	}
	return IRQ_HANDLED;
}

irqreturn_t es755_irq_work(int irq, void *data)
{
	struct escore_priv *escore = (struct escore_priv *)data;
	int rc;
	u32 cmd = 0;

	if (!escore) {
		pr_err("%s(): Invalid IRQ data\n", __func__);
		goto irq_exit;
	}

	/* Delay required for firmware to be ready in case of CVQ mode */
	msleep(50);

	cmd = ES_GET_EVENT << 16;

	rc = escore_cmd(escore, cmd, &escore->escore_event_type);
	if (rc < 0) {
		pr_err("%s(): Error reading IRQ event\n", __func__);
		goto irq_exit;
	}

	escore->escore_event_type &= ES_MASK_INTR_EVENT;

	if (escore->escore_event_type != ES_NO_EVENT) {
		pr_debug("%s(): Notify subscribers about 0x%04x event",
				__func__, escore->escore_event_type);
		blocking_notifier_call_chain(escore->irq_notifier_list,
				escore->escore_event_type, escore);
	}

irq_exit:
	return IRQ_HANDLED;
}

static BLOCKING_NOTIFIER_HEAD(es755_irq_notifier_list);

static struct notifier_block es755_codec_intr_cb = {
	.notifier_call = es755_codec_intr,
	.priority = ES_NORMAL,
};


int es755_core_probe(struct device *dev)
{
	struct esxxx_platform_data *pdata ;
	int rc = 0;
	unsigned long irq_flags = IRQF_DISABLED;
	const char *fw_filename = "audience-es755-fw.bin";
#if defined(CONFIG_SND_SOC_ES_VS)
	const char *vs_filename = "audience-es755-vs.bin";
#endif

	/**/
	if (dev->of_node) {
		dev_info(dev, "Platform data from device tree\n");
		pdata = es755_populate_dt_pdata(dev);
		dev->platform_data = pdata;
	} else {
		dev_info(dev, "Platform data from board file\n");
		pdata = dev->platform_data;
	}

	escore_priv.pdata = pdata;

	mutex_init(&escore_priv.datablock_dev.datablock_read_mutex);
	mutex_init(&escore_priv.pm_mutex);
	mutex_init(&escore_priv.streaming_mutex);
	mutex_init(&escore_priv.msg_list_mutex);
	mutex_init(&escore_priv.datablock_dev.datablock_mutex);

	init_completion(&escore_priv.cmd_compl);
	init_waitqueue_head(&escore_priv.stream_in_q);
	INIT_LIST_HEAD(&escore_priv.msg_list);
	escore_priv.irq_notifier_list = &es755_irq_notifier_list;

#if defined(CONFIG_SND_SOC_ES_VS)
	rc = escore_vs_init(&escore_priv);
#endif

	rc = sysfs_create_group(&escore_priv.dev->kobj, &core_sysfs);
	if (rc) {
		dev_err(escore_priv.dev,
			"failed to create core sysfs entries: %d\n", rc);
	}

	dev_dbg(escore_priv.dev, "%s(): reset_gpio = %d\n", __func__,
		pdata->reset_gpio);
	if (pdata->reset_gpio != -1) {
		rc = gpio_request(pdata->reset_gpio, "es755_reset");
		if (rc < 0) {
			dev_err(escore_priv.dev,
				"%s(): es755_reset request failed :%d",
				__func__, pdata->reset_gpio);
			goto reset_gpio_request_error;
		}
		rc = gpio_direction_output(pdata->reset_gpio, 1);
		if (rc < 0) {
			dev_err(escore_priv.dev,
				"%s(): es755_reset direction failed", __func__);
			goto reset_gpio_direction_error;
		}
		gpio_set_value(pdata->reset_gpio, 1);
	} else {
		dev_warn(escore_priv.dev, "%s(): es755_reset undefined\n",
			 __func__);
	}

	dev_dbg(escore_priv.dev, "%s(): wakeup_gpio = %d\n", __func__,
		pdata->wakeup_gpio);

	if (pdata->wakeup_gpio != -1) {
		rc = gpio_request(pdata->wakeup_gpio, "es755_wakeup");
		if (rc < 0) {
			dev_err(escore_priv.dev,
				"%s(): es755_wakeup request failed", __func__);
			goto wakeup_gpio_request_error;
		}
		rc = gpio_direction_output(pdata->wakeup_gpio, 1);
		if (rc < 0) {
			dev_err(escore_priv.dev,
				"%s(): es755_wakeup direction failed",
				__func__);
			goto wakeup_gpio_direction_error;
		}
	} else {
		dev_warn(escore_priv.dev, "%s(): es755_wakeup undefined\n",
			 __func__);
	}

	dev_dbg(escore_priv.dev, "%s(): gpioa_gpio = %d\n", __func__,
		pdata->gpioa_gpio);
	if (pdata->gpioa_gpio != -1) {
		rc = gpio_request(pdata->gpioa_gpio, "es755_gpioa");
		if (rc < 0) {
			dev_err(escore_priv.dev,
				"%s(): es755_gpioa request failed", __func__);
			goto gpioa_gpio_request_error;
		}
		rc = gpio_direction_input(pdata->gpioa_gpio);
		if (rc < 0) {
			dev_err(escore_priv.dev,
				"%s(): es755_gpioa direction failed", __func__);
			goto gpioa_gpio_direction_error;
		}
	} else {
		dev_warn(escore_priv.dev, "%s(): es755_gpioa undefined\n",
			 __func__);
	}

	dev_dbg(escore_priv.dev, "%s(): gpiob_gpio = %d\n", __func__,
		pdata->gpiob_gpio);

	if (pdata->gpiob_gpio != -1) {
		rc = gpio_request(pdata->gpiob_gpio, "es755_gpiob");
		if (rc < 0) {
			dev_err(escore_priv.dev,
				"%s(): es755_gpiob request failed", __func__);
			goto gpiob_gpio_request_error;
		}
		rc = gpio_direction_input(pdata->gpiob_gpio);
		if (rc < 0) {
			dev_err(escore_priv.dev,
				"%s(): es755_gpiob direction failed", __func__);
			goto gpiob_gpio_direction_error;
		}
	} else {
		dev_warn(escore_priv.dev, "%s(): es755_gpiob undefined\n",
			 __func__);
	}

	rc = request_firmware((const struct firmware **)&escore_priv.standard,
			      fw_filename, escore_priv.dev);
	if (rc) {
		dev_err(escore_priv.dev,
			"%s(): request_firmware(%s) failed %d\n",
			__func__, fw_filename, rc);
		goto request_firmware_error;
	}
#if defined(CONFIG_SND_SOC_ES_VS)
	rc = escore_vs_request_firmware(&escore_priv, vs_filename);
	if (rc) {
		dev_err(escore_priv.dev,
			"%s(): request_firmware(%s) failed %d\n",
			__func__, vs_filename, rc);
		goto request_vs_firmware_error;
	}
#endif
	/* Enable accessory detection for ES755 */
	escore_priv.process_analog = 1;
	escore_priv.regs = kmalloc(sizeof(struct escore_intr_regs), GFP_KERNEL);
	if (escore_priv.regs == NULL) {
		dev_err(escore_priv.dev, "%s(): memory alloc failed for regs\n",
				__func__);
		rc = -ENOMEM;
		goto regs_memalloc_error;
	}

	escore_priv.boot_ops.bootup = es755_bootup;
	escore_priv.soc_codec_dev_escore = &soc_codec_dev_es755;
	escore_priv.dai = es755_dai;
	escore_priv.dai_nr = ES_NUM_CODEC_DAIS;
	escore_priv.api_addr_max = ES_API_ADDR_MAX;
	escore_priv.api_access = es755_api_access;
	escore_priv.reg_cache = a300_reg_cache;
	escore_priv.flag.is_codec = 1;
	escore_priv.escore_power_state = ES_SET_POWER_STATE_NORMAL;
	escore_priv.escore_event_type = ES_NO_EVENT;
	if (escore_priv.pri_intf == ES_SLIM_INTF) {

		escore_priv.slim_rx = slim_rx;
		escore_priv.slim_tx = slim_tx;
		escore_priv.slim_dai_data = slim_dai_data;
		escore_priv.slim_setup = es755_slim_setup;

		escore_priv.slim_rx_ports = ES_SLIM_RX_PORTS;
		escore_priv.slim_tx_ports = ES_SLIM_TX_PORTS;
		escore_priv.codec_slim_dais = ES_NUM_CODEC_SLIM_DAIS;

		escore_priv.slim_tx_port_to_ch_map = es755_slim_tx_port_to_ch;
		escore_priv.slim_rx_port_to_ch_map = es755_slim_rx_port_to_ch;

#if defined(CONFIG_ARCH_MSM)
		escore_priv.slim_dai_ops.get_channel_map =
				es755_slim_get_channel_map;
#endif
		escore_priv.slim_dai_ops.set_channel_map =
				es755_slim_set_channel_map;

		/* Initialization of be_id goes here if required */
		escore_priv.slim_be_id = NULL;

		/* Initialization of _remote_ routines goes here if required */
		escore_priv.remote_cfg_slim_rx = NULL;
		escore_priv.remote_cfg_slim_tx = NULL;
		escore_priv.remote_close_slim_rx = NULL;
		escore_priv.remote_close_slim_tx = NULL;

		escore_priv.flag.local_slim_ch_cfg = 1;
		escore_priv.channel_dir = es755_channel_dir;
		escore_priv.slim_setup(&escore_priv);
	} else if (escore_priv.pri_intf == ES_I2C_INTF) {
	}
#if defined(CONFIG_SND_SOC_ES_I2S)
	escore_priv.i2s_dai_ops.digital_mute =
		es755_digital_mute;
#endif
#if defined(CONFIG_SND_SOC_ES_SLIM)
	escore_priv.slim_dai_ops.digital_mute =
		es755_digital_mute;
#endif

	/* API Interrupt registration */
	if (pdata->gpioa_gpio != -1) {
		rc = request_threaded_irq(gpio_to_irq(pdata->gpioa_gpio), NULL,
				es755_cmd_completion_isr, IRQF_TRIGGER_RISING,
				"es755-cmd-completion-isr", &escore_priv);
		if (rc < 0) {
			pr_err("%s() API interrupt registration failed :%d",
					__func__, rc);
			goto api_intr_error;
		}
	}

	/* Event Interrupt registration */
	if (pdata->gpiob_gpio != -1 && pdata->irq_type) {
		switch (pdata->irq_type) {
		case ES_ACTIVE_LOW:
			irq_flags = IRQF_TRIGGER_LOW;
			break;
		case ES_ACTIVE_HIGH:
			irq_flags = IRQF_TRIGGER_HIGH;
			break;
		case ES_FALLING_EDGE:
			irq_flags = IRQF_TRIGGER_FALLING;
			break;
		case ES_RISING_EDGE:
			irq_flags = IRQF_TRIGGER_RISING;
			break;
		}
		rc = request_threaded_irq(gpio_to_irq(pdata->gpiob_gpio), NULL,
				es755_irq_work, irq_flags,
				"escore-irq-work", &escore_priv);
		if (rc < 0) {
			pr_err("Error in registering interrupt :%d", rc);
			goto event_intr_error;
		}

		/* Disable the interrupt till needed */
		if (escore_priv.pdata->gpiob_gpio != -1)
			disable_irq(gpio_to_irq(pdata->gpiob_gpio));

		escore_register_notify(escore_priv.irq_notifier_list,
				&es755_codec_intr_cb);

	}
#if defined(CONFIG_SND_SOC_ES_UART_STREAMDEV)
	escore_priv.streamdev = es_uart_streamdev;
#endif
	return rc;

regs_memalloc_error:
event_intr_error:
	free_irq(gpio_to_irq(pdata->gpioa_gpio), NULL);
api_intr_error:
#if defined(CONFIG_SND_SOC_ES_VS)
request_vs_firmware_error:
#endif
	release_firmware(escore_priv.standard);
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
	dev_dbg(escore_priv.dev, "%s(): exit with error\n", __func__);

	return rc;
}
EXPORT_SYMBOL_GPL(es755_core_probe);

static __init int es755_init(void)
{
	int rc = 0;

	pr_debug("%s()", __func__);

	escore_pm_init();

	escore_priv.device_name  = es755_device;

	mutex_init(&escore_priv.api_mutex);
#if defined(CONFIG_SND_SOC_ES_I2C)
	escore_priv.pri_intf = ES_I2C_INTF;
#elif defined(CONFIG_SND_SOC_ES_SLIM)
	escore_priv.pri_intf = ES_SLIM_INTF;
#elif defined(CONFIG_SND_SOC_ES_UART)
	escore_priv.pri_intf = ES_UART_INTF;
#elif defined(CONFIG_SND_SOC_ES_SPI)
	escore_priv.pri_intf = ES_SPI_INTF;
#endif

#if defined(CONFIG_SND_SOC_ES_HIGH_BW_BUS_I2C)
	escore_priv.high_bw_intf = ES_I2C_INTF;
#elif defined(CONFIG_SND_SOC_ES_HIGH_BW_BUS_SLIM)
	escore_priv.high_bw_intf = ES_SLIM_INTF;
#elif defined(CONFIG_SND_SOC_ES_HIGH_BW_BUS_UART)
	escore_priv.high_bw_intf = ES_UART_INTF;
#elif defined(CONFIG_SND_SOC_ES_HIGH_BW_BUS_SPI)
	escore_priv.high_bw_intf = ES_SPI_INTF;
#elif defined(CONFIG_SND_SOC_ES_HIGH_BW_BUS_DEFAULT)
	escore_priv.high_bw_intf = escore_priv.pri_intf;
#endif

#if defined(CONFIG_SND_SOC_ES_WAKEUP_UART)
	escore_priv.wakeup_intf = ES_UART_INTF;
#endif

#if defined(CONFIG_SND_SOC_ES_I2C) || \
	defined(CONFIG_SND_SOC_ES_HIGH_BW_BUS_I2C)
	rc = escore_i2c_init();
#endif
#if defined(CONFIG_SND_SOC_ES_SLIM) || \
	defined(CONFIG_SND_SOC_ES_HIGH_BW_BUS_SLIM)
	rc = escore_slimbus_init();
#endif
#if defined(CONFIG_SND_SOC_ES_SPI) || \
	defined(CONFIG_SND_SOC_ES_HIGH_BW_BUS_SPI)
	rc = escore_spi_init();
#endif
#if defined(CONFIG_SND_SOC_ES_UART) || \
	defined(CONFIG_SND_SOC_ES_HIGH_BW_BUS_UART) || \
	defined(CONFIG_SND_SOC_ES_WAKEUP_UART)
	rc = escore_uart_bus_init(&escore_priv);
#endif
	if (rc) {
		pr_debug("Error registering Audience eS755 driver: %d\n", rc);
		goto INIT_ERR;
	}

#if defined(CONFIG_SND_SOC_ES_CDEV)
	rc = escore_cdev_init(&escore_priv);
	if (rc) {
		pr_debug("Error enabling CDEV interface: %d\n", rc);
		goto INIT_ERR;
	}
#endif
INIT_ERR:
	return rc;
}
module_init(es755_init);

static __exit void es755_exit(void)
{
	pr_debug("%s()\n", __func__);
#if defined(CONFIG_SND_SOC_ES_VS)
	escore_vs_release_firmware(&escore_priv);
#endif

#if defined(CONFIG_SND_SOC_ES_I2C)
	i2c_del_driver(&escore_i2c_driver);
#elif defined(CONFIG_SND_SOC_ES_SPI)
	escore_spi_exit();
#endif
}
module_exit(es755_exit);


MODULE_DESCRIPTION("ASoC ES755 driver");
MODULE_AUTHOR("Greg Clemson <gclemson@audience.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:es755-codec");
MODULE_FIRMWARE("audience-es755-fw.bin");
