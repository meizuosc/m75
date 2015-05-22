/*
 * es705.c  --  Audience eS705 ALSA SoC Audio driver
 *
 * Copyright 2011 Audience, Inc.
 *
 * Author: Greg Clemson <gclemson@audience.com>
 *
 * Code Updates:
 *       Genisim Tsilker <gtsilker@audience.com>
 *            - Code refactoring
 *            - FW download functions update
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
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/version.h>
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
#include <linux/spi/spi.h>
#include <linux/esxxx.h>
#include <linux/wait.h>
#include <linux/kernel.h>
#include "es705_escore.h"
#include "es705-routes.h"
#include "escore.h"
#include "escore-slim.h"
#include "escore-spi.h"
#include "escore-uart.h"
#include "escore-uart-common.h"
#include "escore-i2c.h"
#include "escore-i2s.h"
#include "escore-cdev.h"
#include "escore-vs.h"

/* local function proto type */
/* static int es705_dev_rdb(struct escore_priv *es705, void *buf, int len);
static int es705_dev_wdb(struct escore_priv *es705, const void *buf, int len);
*/

#define ES705_CMD_ACCESS_WR_MAX 2
#define ES705_CMD_ACCESS_RD_MAX 2

#if defined(CONFIG_SND_SOC_ES_SLIM)
static int es705_slim_be_id[ES_NUM_CODEC_SLIM_DAIS] = {
	ES_SLIM_2_CAP, /* for ES_SLIM_1_PB tx from es705 */
	ES_SLIM_3_PB, /* for ES_SLIM_1_CAP rx to es705 */
	ES_SLIM_3_CAP, /* for ES_SLIM_2_PB tx from es705 */
	-1, /* for ES_SLIM_2_CAP */
	-1, /* for ES_SLIM_3_PB */
	-1, /* for ES_SLIM_3_CAP */
};
#endif

#include "es705-access.h"

/* Route state for Internal state management */
enum es705_power_state {
ES705_POWER_BOOT,
ES705_POWER_SLEEP,
ES705_POWER_SLEEP_PENDING,
ES705_POWER_AWAKE
};

static const char *power_state[] = {
	"boot",
	"sleep",
	"sleep pending",
	"awake",
};

static const char *power_state_cmd[] = {
	"not defined",
	"sleep",
	"mp_sleep",
	"mp_cmd",
	"normal",
	"vs_overlay",
	"vs_lowpwr",
};

/* codec private data TODO: move to runtime init */
struct escore_priv escore_priv = {
	.pm_state = ES705_POWER_AWAKE,
	.probe = es705_core_probe,

	.set_streaming = es705_set_streaming,
	.pm_state = ES705_POWER_AWAKE,

	.flag.rx1_route_enable = 0,
	.flag.tx1_route_enable = 0,
	.flag.rx2_route_enable = 0,

	.flag.vs_enable = 0,
	.ap_tx1_ch_cnt = 2,

	.es705_power_state = ES705_SET_POWER_STATE_NORMAL,
	.streamdev.intf = ES_UART_INTF,
	.flag.ns = 1,
	.flag.sleep_enable = 0, /*Auto sleep disabled default*/
	.sleep_delay = 3000,
	.wake_count = 0,
	.flag.sleep_abort = 0,
};

const char *esxxx_mode[] = {
	"SBL",
	"STANDARD",
	"VOICESENSE",
};

struct snd_soc_dai_driver es705_dai[];

/* indexed by ES705 INTF number */
static const u32 es705_streaming_cmds[] = {
	0x00000000,		/* ES_NULL_INTF */
	0x90250200,		/* ES_SLIM_INTF */
	0x90250000,		/* ES_I2C_INTF  */
	0x90250300,		/* ES_SPI_INTF  */
	0x90250101,		/* ES_UART_INTF */
};

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

struct snd_soc_dai_driver es705_dai[] = {
#if defined(CONFIG_SND_SOC_ES_SLIM)
	{
		.name = "es705-slim-rx1",
		.id = ES705_SLIM_1_PB,
		.playback = {
			.stream_name = "SLIM_PORT-1 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES705_SLIMBUS_RATES,
			.formats = ES705_SLIMBUS_FORMATS,
		},
		.ops = &escore_slim_port_dai_ops,
	},
	{
		.name = "es705-slim-tx1",
		.id = ES705_SLIM_1_CAP,
		.capture = {
			.stream_name = "SLIM_PORT-1 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES705_SLIMBUS_RATES,
			.formats = ES705_SLIMBUS_FORMATS,
		},
		.ops = &escore_slim_port_dai_ops,
	},
	{
		.name = "es705-slim-rx2",
		.id = ES705_SLIM_2_PB,
		.playback = {
			.stream_name = "SLIM_PORT-2 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES705_SLIMBUS_RATES,
			.formats = ES705_SLIMBUS_FORMATS,
		},
		.ops = &escore_slim_port_dai_ops,
	},
	{
		.name = "es705-slim-tx2",
		.id = ES705_SLIM_2_CAP,
		.capture = {
			.stream_name = "SLIM_PORT-2 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES705_SLIMBUS_RATES,
			.formats = ES705_SLIMBUS_FORMATS,
		},
		.ops = &escore_slim_port_dai_ops,
	},
	{
		.name = "es705-slim-rx3",
		.id = ES705_SLIM_3_PB,
		.playback = {
			.stream_name = "SLIM_PORT-3 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES705_SLIMBUS_RATES,
			.formats = ES705_SLIMBUS_FORMATS,
		},
		.ops = &escore_slim_port_dai_ops,
	},
	{
		.name = "es705-slim-tx3",
		.id = ES705_SLIM_3_CAP,
		.capture = {
			.stream_name = "SLIM_PORT-3 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES705_SLIMBUS_RATES,
			.formats = ES705_SLIMBUS_FORMATS,
		},
		.ops = &escore_slim_port_dai_ops,
	},
#endif
#if defined(CONFIG_SND_SOC_ES_I2S)
	{
		.name = "earSmart-porta",
		.id = ES_I2S_PORTA,
		.playback = {
			.stream_name = "PORTA Playback",
			.channels_min = 2,
			.channels_max = 2,
			.rates = ES705_RATES,
			.formats = ES705_FORMATS,
		},
		.capture = {
			.stream_name = "PORTA Capture",
			.channels_min = 2,
			.channels_max = 2,
			.rates = ES705_RATES,
			.formats = ES705_FORMATS,
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
			.rates = ES705_RATES,
			.formats = ES705_FORMATS,
		},
		.capture = {
			.stream_name = "PORTB Capture",
			.channels_min = 2,
			.channels_max = 2,
			.rates = ES705_RATES,
			.formats = ES705_FORMATS,
		},
		.ops = &escore_i2s_port_dai_ops,
	},
#endif
};

static struct escore_slim_dai_data slim_dai_data[ES_NUM_CODEC_SLIM_DAIS];
static struct escore_slim_ch slim_rx[ES_SLIM_RX_PORTS];
static struct escore_slim_ch slim_tx[ES_SLIM_TX_PORTS];

static int es705_slim_rx_port_to_ch[ES_SLIM_RX_PORTS] = {
	152, 153, 154, 155, 134, 135
};
static int es705_slim_tx_port_to_ch[ES_SLIM_TX_PORTS] = {
	156, 157, 138, 139, 143, 144
};

static void es705_sleep_queue(struct escore_priv *es705);
static void es705_wakeup_request(struct escore_priv *es705);

int es705_dev_rdb(struct escore_priv *es705, void *buf, int len)
{
	dev_dbg(es705->dev, "%s - default\n", __func__);
	return 0;
}

int es705_dev_wdb(struct escore_priv *es705, const void *buf, int len)
{
	dev_dbg(es705->dev, "%s - default\n", __func__);
	return 0;
}

static void es705_switch_route(long route_index)
{
	struct escore_priv *es705 = &escore_priv;
	int rc;

	if (route_index >= ROUTE_MAX) {
		dev_dbg(es705->dev, "%s(): new es705_internal_route = %ld is out of range\n",
			 __func__, route_index);
		return;
	}

	dev_dbg(es705->dev, "%s(): switch current es705_internal_route = %ld to new route = %ld\n",
		__func__, es705->internal_route_num, route_index);
	es705->internal_route_num = route_index;
	if (es705->pm_state == ES705_POWER_BOOT) {
		dev_err(es705->dev, "PM_STATE of ES705 is ES705_POWER_BOOT. Don't set route\n");
		return -EINVAL;
	}
	rc = escore_write_block(es705,
			  es705_route_config[es705->internal_route_num].route);
}

int es705_cmd_without_sleep(struct escore_priv *es705, u32 cmd)
{
	int sr;
	int err;
	u32 resp;

	BUG_ON(!es705);

	sr = cmd & BIT(28);

	err = es705->bus.ops.cmd(es705, cmd, &resp);
	if (err || sr)
		return err;

	if (resp == 0) {
		err = -ETIMEDOUT;
		dev_err(es705->dev, "%s(): no response to command 0x%08x\n",
			__func__, cmd);
	} else {
		es705->bus.last_response = resp;
		get_monotonic_boottime(&es705->last_resp_time);
	}
	return err;
}


/* Send a single command to the chip.
 *
 * If the SR (suppress response bit) is NOT set, will read the
 * response and cache it the driver object retrieve with es705_resp().
 *
 * Returns:
 * 0 - on success.
 * EITIMEDOUT - if the chip did not respond in within the expected time.
 * E* - any value that can be returned by the underlying HAL.
 */
int es705_cmd(struct escore_priv *es705, u32 cmd)
{
	int rc;

	BUG_ON(!es705);
	es705_wakeup_request(es705);
	rc = es705_cmd_without_sleep(es705, cmd);
	es705_sleep_queue(es705);
	return rc;
}

static unsigned int es705_read_without_sleep(struct snd_soc_codec *codec,
				unsigned int reg)
{
	return escore_read(codec, reg);
}

static unsigned int es705_read(struct snd_soc_codec *codec,
			       unsigned int reg)
{
	unsigned int value;

	es705_wakeup_request(&escore_priv);
	value = es705_read_without_sleep(codec, reg);
	es705_sleep_queue(&escore_priv);
	return value;
}

static int es705_write_without_sleep(struct snd_soc_codec *codec,
			unsigned int reg, unsigned int value)
{
	return escore_write(codec, reg, value);
}

static int es705_write(struct snd_soc_codec *codec, unsigned int reg,
		       unsigned int value)
{
	es705_wakeup_request(&escore_priv);
	value = es705_write_without_sleep(codec, reg, value);
	es705_sleep_queue(&escore_priv);
	return value;
}

static ssize_t es705_route_status_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	int ret = 0;
	unsigned int value = 0;
	char *status_name = "Route Status";

	value = es705_read(NULL, ES705_CHANGE_STATUS);

	ret = snprintf(buf, PAGE_SIZE,
		       "%s=0x%04x\n",
		       status_name, value);

	return ret;
}

static DEVICE_ATTR(route_status, 0444, es705_route_status_show, NULL);
/* /sys/devices/platform/msm_slim_ctrl.1/earSmart-codec-gen0/route_status */

static ssize_t es705_route_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct escore_priv *es705 = &escore_priv;

	dev_dbg(es705->dev, "%s(): route=%ld\n",
		__func__, es705->internal_route_num);
	return snprintf(buf, PAGE_SIZE, "route=%ld\n",
			es705->internal_route_num);
}

static DEVICE_ATTR(route, 0444, es705_route_show, NULL);
/* /sys/devices/platform/msm_slim_ctrl.1/earSmart-codec-gen0/route */

static ssize_t es705_rate_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct escore_priv *es705 = &escore_priv;

	dev_dbg(es705->dev, "%s(): rate=%ld\n", __func__, es705->internal_rate);
	return snprintf(buf, PAGE_SIZE, "rate=%ld\n",
			es705->internal_rate);
}

static DEVICE_ATTR(rate, 0444, es705_rate_show, NULL);
/* /sys/devices/platform/msm_slim_ctrl.1/earSmart-codec-gen0/rate */

#define SIZE_OF_VERBUF 256
/* TODO: fix for new read/write. use es705_read() instead of BUS ops */
static ssize_t es705_fw_version_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	int idx = 0;
	unsigned int value;
	char versionbuffer[SIZE_OF_VERBUF];
	char *verbuf = versionbuffer;

	memset(verbuf, 0, SIZE_OF_VERBUF);

	value = es705_read(NULL, ES705_FW_FIRST_CHAR);
	*verbuf++ = (value & 0x00ff);
	for (idx = 0; idx < (SIZE_OF_VERBUF-2); idx++) {
		value = es705_read(NULL, ES705_FW_NEXT_CHAR);
		*verbuf++ = (value & 0x00ff);
	}
	/* Null terminate the string*/
	*verbuf = '\0';
	dev_info(dev, "Audience fw ver %s\n", versionbuffer);
	return snprintf(buf, PAGE_SIZE, "FW Version = %s\n", versionbuffer);
}

static DEVICE_ATTR(fw_version, 0444, es705_fw_version_show, NULL);
/* /sys/devices/platform/msm_slim_ctrl.1/earSmart-codec-gen0/fw_version */

static ssize_t es705_clock_on_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	int ret = 0;

	return ret;
}

static DEVICE_ATTR(clock_on, 0444, es705_clock_on_show, NULL);
/* /sys/devices/platform/msm_slim_ctrl.1/earSmart-codec-gen0/clock_on */

static ssize_t es705_vs_keyword_parameters_show(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	int ret = 0;
	struct escore_voice_sense *voice_sense =
		(struct escore_voice_sense *)escore_priv.voice_sense;

	if (voice_sense->vs_keyword_param_size > 0) {
		memcpy(buf, &(voice_sense->vs_keyword_param[0]),
		       voice_sense->vs_keyword_param_size);
		ret = voice_sense->vs_keyword_param_size;
		dev_dbg(dev, "%s(): keyword param size=%hu\n", __func__, ret);
	} else {
		dev_dbg(dev, "%s(): keyword param not set\n", __func__);
	}

	return ret;
}

static ssize_t es705_vs_keyword_parameters_set(struct device *dev,
					       struct device_attribute *attr,
					       const char *buf, size_t count)
{
	int ret = 0;
	struct escore_voice_sense *voice_sense =
		(struct escore_voice_sense *)escore_priv.voice_sense;

	if (count <= ES705_VS_KEYWORD_PARAM_MAX) {
		memcpy(&(voice_sense->vs_keyword_param[0]), buf, count);
		voice_sense->vs_keyword_param_size = count;
		dev_dbg(dev, "%s(): keyword param block set size = %zi\n",
			 __func__, count);
		ret = count;
	} else {
		dev_dbg(dev, "%s(): keyword param block too big = %zi\n",
			 __func__, count);
		ret = -ENOMEM;
	}

	return ret;
}

static DEVICE_ATTR(vs_keyword_parameters, 0644,
		   es705_vs_keyword_parameters_show,
		   es705_vs_keyword_parameters_set);
/* /sys/devices/platform/msm_slim_ctrl.1/earSmart-codec-gen0/
		vs_keyword_parameters */

static ssize_t es705_gpio_reset_set(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct escore_priv *es705 = &escore_priv;
	dev_dbg(es705->dev, "%s(): GPIO reset\n", __func__);
	es705->mode = SBL;
	escore_gpio_reset(es705);
	dev_dbg(es705->dev, "%s(): Ready for STANDARD download by proxy\n",
		__func__);
	return count;
}

static DEVICE_ATTR(gpio_reset, 0644, NULL, es705_gpio_reset_set);
/* /sys/devices/platform/msm_slim_ctrl.1/earSmart-codec-gen0/gpio_reset */


static ssize_t es705_overlay_mode_set(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct escore_priv *es705 = &escore_priv;
	int rc;
	int value = ES705_SET_POWER_STATE_VS_OVERLAY;

	dev_dbg(es705->dev, "%s(): Set Overlay mode\n", __func__);

	es705->mode = SBL;
	rc = es705_write(NULL, ES705_POWER_STATE , value);
	if (rc) {
		dev_err(escore_priv.dev, "%s(): Set Overlay mode failed\n",
			__func__);
	} else {
		msleep(50);
		escore_priv.es705_power_state =
			ES705_SET_POWER_STATE_VS_OVERLAY;
		/* wait until es705 SBL mode activating */
		dev_dbg(es705->dev, "%s(): Ready for VOICESENSE download by proxy\n",
		__func__);
		dev_info(es705->dev, "%s(): After successful VOICESENSE download,"
			"Enable Event Intr to Host\n",
			__func__);
	}
	return count;
}

static DEVICE_ATTR(overlay_mode, 0644, NULL, es705_overlay_mode_set);
/* /sys/devices/platform/msm_slim_ctrl.1/earSmart-codec-gen0/overlay_mode */

static ssize_t es705_vs_event_set(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct escore_priv *es705 = &escore_priv;
	int rc;
	int value = ES705_SYNC_INTR_RISING_EDGE;
	dev_dbg(es705->dev, "%s(): Enable Voice Sense Event to Host\n",
		__func__);

	es705->mode = VOICESENSE;
	/* Enable Voice Sense Event INTR to Host */
	rc = es705_write(NULL, ES705_EVENT_RESPONSE, value);
	if (rc)
		dev_err(es705->dev, "%s(): Enable Event Intr fail\n",
			__func__);
	return count;
}

static DEVICE_ATTR(vs_event, 0644, NULL, es705_vs_event_set);
/* /sys/devices/platform/msm_slim_ctrl.1/earSmart-codec-gen0/vs_event */

static ssize_t es705_auto_sleep_show(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	int ret = 0;
	ret = snprintf(buf, PAGE_SIZE, "%d\n", escore_priv.flag.sleep_enable);
	return ret;
}

static ssize_t es705_auto_sleep_set(struct device *dev,
					       struct device_attribute *attr,
					       const char *buf, size_t count)
{
	int rc = 0;
	int sleep_enable = 0;
	rc = kstrtos32(buf, 0, &sleep_enable);
	escore_priv.flag.sleep_enable = sleep_enable;

	dev_info(escore_priv.dev, "%s(): auto sleep = %d\n",
				__func__, escore_priv.flag.sleep_enable);

	return count;
}

static DEVICE_ATTR(auto_sleep, 0644,
		   es705_auto_sleep_show,
		   es705_auto_sleep_set);
/* /sys/devices/platform/msm_slim_ctrl.1/earSmart-codec-gen0/auto_sleep */


static ssize_t es705_sleep_delay_show(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	int ret = 0;
	ret = snprintf(buf, PAGE_SIZE, "%d\n", escore_priv.sleep_delay);
	return ret;
}

static ssize_t es705_sleep_delay_set(struct device *dev,
					       struct device_attribute *attr,
					       const char *buf, size_t count)
{
	int rc = 0;
	rc = kstrtos32(buf, 0, &escore_priv.sleep_delay);

	dev_info(escore_priv.dev, "%s(): sleep delay = %d\n",
					__func__, escore_priv.sleep_delay);

	return count;
}

static DEVICE_ATTR(sleep_delay_msec, 0644,
		   es705_sleep_delay_show,
		   es705_sleep_delay_set);
/* /sys/devices/platform/msm_slim_ctrl.1/earSmart-codec-gen0/sleep_delay */



static struct attribute *core_sysfs_attrs[] = {
	&dev_attr_route_status.attr,
	&dev_attr_route.attr,
	&dev_attr_rate.attr,
	&dev_attr_fw_version.attr,
	&dev_attr_clock_on.attr,
	&dev_attr_vs_keyword_parameters.attr,
	&dev_attr_gpio_reset.attr,
	&dev_attr_overlay_mode.attr,
	&dev_attr_vs_event.attr,
	&dev_attr_auto_sleep.attr,
	&dev_attr_sleep_delay_msec.attr,
	NULL
};

static struct attribute_group core_sysfs = {
	.attrs = core_sysfs_attrs
};

static int es705_fw_download(struct escore_priv *es705, int fw_type)
{
	int rc = 0;
	struct escore_voice_sense *voice_sense =
		(struct escore_voice_sense *) es705->voice_sense;

	dev_err(es705->dev, "%s(): firmware download type %d begin\n",
							__func__, fw_type);
	if (fw_type != VOICESENSE && fw_type != STANDARD) {
		dev_err(es705->dev, "%s(): Unknown firmware type\n", __func__);
		goto es705_fw_download_failed;
	}

	if (!es705->boot_ops.setup || !es705->boot_ops.finish) {
		dev_err(es705->dev, "%s(): boot setup or finish func undef\n",
								__func__);
		goto es705_fw_download_failed;
	}

	rc = es705->boot_ops.setup(es705);
	if (rc) {
		dev_err(es705->dev, "%s(): firmware download start error %d\n",
								__func__, rc);
		goto es705_fw_download_failed;
	}

	if (fw_type == VOICESENSE)
		rc = es705->bus.ops.high_bw_write(es705,
				(char *)voice_sense->vs->data,
				voice_sense->vs->size);
	else
		rc = es705->bus.ops.high_bw_write(es705,
				(char *)es705->standard->data,
				es705->standard->size);
	if (rc) {
		dev_err(es705->dev, "%s(): firmware write error %d\n",
								__func__, rc);
		rc = -EIO;
		goto es705_fw_download_failed;
	}

	es705->mode = fw_type;
	rc = es705->boot_ops.finish(es705);
	if (rc) {
		dev_err(es705->dev, "%s() firmware download finish error %d\n",
								__func__, rc);
			goto es705_fw_download_failed;
	}
	dev_err(es705->dev, "%s(): firmware download type %d done\n",
							__func__, fw_type);

es705_fw_download_failed:
	return rc;
}

void es705_slim_setup(struct escore_priv *escore_priv)
{
	int i;
	int ch_cnt;
	dev_dbg(escore_priv->dev, "%s():\n", __func__);

	escore_priv->init_slim_slave(escore_priv);

	/* allocate ch_num array for each DAI */
	for (i = 0; i < (ARRAY_SIZE(es705_dai)); i++) {
		switch (es705_dai[i].id) {
		case ES_SLIM_1_PB:
		case ES_SLIM_2_PB:
		case ES_SLIM_3_PB:
			ch_cnt = es705_dai[i].playback.channels_max;
			break;
		case ES_SLIM_1_CAP:
		case ES_SLIM_2_CAP:
		case ES_SLIM_3_CAP:
			ch_cnt = es705_dai[i].capture.channels_max;
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


static int es705_channel_dir(int dai_id)
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

int es705_slim_sleep(struct escore_priv *escore)
{
	/* TODO:
	 * Add the code for slimbus
	 */
	return 0;
}

int es705_slim_wakeup(struct escore_priv *escore)
{
	/* TODO:
	 * Add the code for slimbus
	 */
	return 0;
}

int es705_i2c_sleep(struct escore_priv *escore)
{
	/* TODO:
	 * Add the code for i2c
	 */
	return 0;
}

int es705_i2c_wakeup(struct escore_priv *escore)
{
	/* TODO:
	 * Add the code for i2c
	 */
	return 0;
}

int es705_bootup(struct escore_priv *es705)
{
	int rc;
	int retries = 0;
	BUG_ON(es705->standard->size == 0);
	mutex_lock(&es705->pm_mutex);
	es705->pm_state = ES705_POWER_BOOT;
	mutex_unlock(&es705->pm_mutex);
Retry:
	escore_gpio_reset(es705);

	rc = es705_fw_download(es705, STANDARD);

	if (rc) {
		msleep(500);
		retries++;
		dev_err(es705->dev, "%s(): STANDARD fw download error %d, retry %d\n",
								__func__, rc, retries);
		goto Retry;
	} else {
		mutex_lock(&es705->pm_mutex);
		es705->pm_state = ES705_POWER_AWAKE;
		mutex_unlock(&es705->pm_mutex);
	}
	return rc;
}

/* Hold the pm_mutex before calling this function */
static int es705_sleep(struct escore_priv *es705)
{
	u32 cmd = (ES705_SET_SMOOTH << 16) | ES705_SET_SMOOTH_RATE;
	int rc;

	rc = es705_cmd_without_sleep(es705, cmd);

	/* write 0x8000_0001
	 * sleep 20 ms
	 * clocks off
	 */
	cmd = (ES705_SET_POWER_STATE << 16) | ES705_SET_POWER_STATE_SLEEP;

	rc = es705_cmd_without_sleep(es705, cmd);
	/* There will not be any response after sleep command from chip */

	usleep_range(25000, 25000);

	escore_priv.es705_power_state = ES705_SET_POWER_STATE_SLEEP;
	escore_priv.pm_state = ES705_POWER_SLEEP;

	/* clocks off */
	if (es705->pdata->esxxx_clk_cb)
		es705->pdata->esxxx_clk_cb(0);

	dev_dbg(escore_priv.dev, "Sleep done, wake_count=%d\n",
			escore_priv.wake_count);

	usleep_range(5000, 5000);
	return 0;
}

static void es705_delayed_sleep(struct work_struct *w)
{
	int ports_active = (escore_priv.flag.rx1_route_enable ||
		escore_priv.flag.rx2_route_enable ||
		escore_priv.flag.tx1_route_enable);
	/* If there are active streams we do not sleep.
	* Count the front end (FE) streams ONLY.
	*/

	dev_dbg(escore_priv.dev, "%s ports_active: %d\n",
		__func__, ports_active);

	dev_dbg(escore_priv.dev, "%s Delayed Sleep entry\n",
				__func__);

	mutex_lock(&escore_priv.pm_mutex);

	if ((ports_active == 0) &&
		(escore_priv.pm_state == ES705_POWER_SLEEP_PENDING)) {
		dev_dbg(escore_priv.dev, "%s Delayed Sleep started\n",
					__func__);
		if (escore_priv.flag.vs_enable)
			schedule_delayed_work(&(escore_priv.vs_fw_load), 0);
		else
			es705_sleep(&escore_priv);
	} else {
		dev_err(escore_priv.dev,
		"%s error ports_active=%d, pm_state=%d\n",
		 __func__, ports_active, escore_priv.pm_state);
	}
	mutex_unlock(&escore_priv.pm_mutex);
}

static void es705_sleep_request(struct escore_priv *es705)
{
	mutex_lock(&es705->pm_mutex);
	dev_dbg(es705->dev, "%s internal es705_power_state = %d\n", __func__,
		escore_priv.pm_state);

	if ((escore_priv.pm_state != ES705_POWER_AWAKE) ||  \
			escore_priv.wake_count > 0)
		goto sleep_request_exit;

	if (escore_priv.es705_power_state == ES705_SET_POWER_STATE_VS_OVERLAY
		|| escore_priv.es705_power_state == \
			ES705_SET_POWER_STATE_VS_LOWPWR) {
		dev_dbg(escore_priv.dev, "%s() Sleep not allowed in VS mode\n",
				__func__);
		goto sleep_request_exit;
	}
	/* If we reach here means pm_state == ES705_POWER_AWAKE
	   and escore_priv.wake_count == 0 */
	schedule_delayed_work(&es705->sleep_work,
			msecs_to_jiffies(es705->sleep_delay));
	dev_dbg(es705->dev, "%s ES705_POWER_SLEEP_PENDING\n",
			__func__);
	escore_priv.pm_state = ES705_POWER_SLEEP_PENDING;

sleep_request_exit:
	mutex_unlock(&es705->pm_mutex);
}

static void es705_sleep_queue(struct escore_priv *es705)
{
	if (es705->flag.sleep_enable) {
		mutex_lock(&es705->wake_mutex);
		es705->wake_count -= 1;
		dev_dbg(es705->dev, "%s wake_count=%d\n",
				__func__, es705->wake_count);

		if (es705->wake_count <= 0)	{
			es705->wake_count = 0;
			/*reset if count goes negative*/
			es705_sleep_request(es705);
		}
		mutex_unlock(&es705->wake_mutex);
	}
}

static void es705_send_wakeup(struct escore_priv *es705)
{
#if defined(CONFIG_SND_SOC_ES_HIGH_BW_BUS_UART) || \
	defined(CONFIG_SND_SOC_ES_UART)
	int rc = 0;
	char cmd[4] = {ES_UART_WAKE_CMD, 0, 0, 0};

	rc = escore_uart_write(es705, (void *)cmd,
				sizeof(cmd));
#else
#ifdef CONFIG_ARCH_MT6595
	mt_set_gpio_out(es705->pdata->wakeup_gpio, 1);
	usleep_range(1000, 1000);
	mt_set_gpio_out(es705->pdata->wakeup_gpio, 0);
#else
	gpio_set_value(es705->pdata->wakeup_gpio, 1);
	usleep_range(1000, 1000);
	gpio_set_value(es705->pdata->wakeup_gpio, 0);
#endif
#endif

	return;
}

#define SYNC_DELAY 30
static int es705_wakeup(struct escore_priv *es705)
{
	int rc = 0;
	u32 sync_cmd = (ES705_SYNC_CMD << 16) | ES705_SYNC_POLLING;
	u32 sync_rspn = sync_cmd;
	u32 resp;

	/* 1 - clocks on
	 * 2 - wakeup 1 -> 0
	 * 3 - sleep 30 ms
	 * 4 - Send sync command (0x8000, 0x0001)
	 * 5 - Read sync ack
	 * 6 - wakeup 0 -> 1
	 */
	mutex_lock(&es705->pm_mutex);
	if (delayed_work_pending(&es705->sleep_work) ||
		(es705->pm_state == ES705_POWER_SLEEP_PENDING)) {
		mutex_unlock(&es705->pm_mutex);

		mutex_lock(&escore_priv.abort_mutex);
		es705->flag.sleep_abort = true;
		dev_dbg(es705->dev, "%s(): abort set\n", __func__);
		mutex_unlock(&escore_priv.abort_mutex);

		dev_dbg(es705->dev,
			"%s(): before cancel_delayed_work_sync\n",
			__func__);
		cancel_delayed_work_sync(&es705->vs_fw_load);
		cancel_delayed_work_sync(&es705->sleep_work);
		dev_dbg(es705->dev,
			"%s(): after cancel_delayed_work_sync\n",
			__func__);

		mutex_lock(&escore_priv.abort_mutex);
		es705->flag.sleep_abort = false;
		mutex_unlock(&escore_priv.abort_mutex);

		dev_dbg(es705->dev, "%s(): cancel delayed work\n", __func__);

		mutex_lock(&es705->pm_mutex);
		es705->pm_state = ES705_POWER_AWAKE;
		goto es705_wakeup_exit;
	}

	/* Check if previous power state is not sleep then return */
	if (es705->pm_state != ES705_POWER_SLEEP) {
		dev_dbg(es705->dev, "%s(): no need to go to Normal Mode\n",
			__func__);
		goto es705_wakeup_exit;
	}

	if (es705->pdata->esxxx_clk_cb) {
		es705->pdata->esxxx_clk_cb(1);
		usleep_range(3000, 3100);
	}

	dev_dbg(es705->dev, "%s(): generate gpio wakeup falling edge\n",
		__func__);

	es705_send_wakeup(es705);

	dev_dbg(es705->dev, "%s(): wait 30ms wakeup, then ping to es705\n",
		__func__);
	msleep(SYNC_DELAY);

	/* Take it out of OverlayMode in case if Overlay mode
		only when auto sleep is enabled.*/
	if ((es705->flag.sleep_enable) &&
			(es705->es705_power_state ==
			ES705_SET_POWER_STATE_VS_LOWPWR)) {
		rc = escore_cmd(es705, sync_cmd, &resp);
		if (rc < 0) {
			dev_err(es705->dev, "%s(): escore_cmd failed\n",
				__func__);
			goto es705_wakeup_exit;
		}
		if (resp != sync_rspn) {
			dev_err(es705->dev,
				"%s(): sync in overlay FAIL\n", __func__);
			goto es705_wakeup_exit;
		}

		rc = es705_cmd_without_sleep(es705,
		(ES705_SET_POWER_STATE << 16) | ES705_SET_POWER_STATE_NORMAL);

		if (rc) {
			dev_err(es705->dev,
				"%s(): power state normal FAIL\n", __func__);
			goto es705_wakeup_exit;
		}

		/* Wait for 100ms to switch from Overlay mode */
		dev_dbg(es705->dev, "%s(): wait 30ms wakeup, then ping to es705\n",
			__func__);
		msleep(100);
	}

	rc = escore_cmd(es705, sync_cmd, &resp);
	if (rc < 0) {
		dev_err(es705->dev, "%s(): escore_cmd failed\n", __func__);
		goto es705_wakeup_exit;
	}

	if (resp != sync_rspn) {
		dev_err(es705->dev, "%s(): es705 wakeup FAIL\n", __func__);
		goto es705_wakeup_exit;
	}
	es705->pm_state = ES705_POWER_AWAKE;

	/* TODO use GPIO reset, if wakeup fail ? */

es705_wakeup_exit:
	mutex_unlock(&es705->pm_mutex);
	return rc;
}

static void es705_wakeup_request(struct escore_priv *es705)
{
	int rc = 0;
	if (es705->flag.sleep_enable) {
		mutex_lock(&es705->wake_mutex);
		es705->wake_count += 1;

		dev_dbg(es705->dev, "%s wake_count=%d\n",
					__func__, es705->wake_count);

		rc = es705_wakeup(es705);
		if (rc)
			goto es705_wakeup_request_exit;
		es705->es705_power_state = ES705_SET_POWER_STATE_NORMAL;
		es705->mode = STANDARD;

es705_wakeup_request_exit:
		mutex_unlock(&es705->wake_mutex);
	}

}

static int es705_put_control_value(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	/* struct snd_soc_codec *codec = escore_priv.codec; */
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	int rc = 0;

	value = ucontrol->value.integer.value[0];
	rc = es705_write(NULL, reg, value);

	return 0;
}

static int es705_get_control_value(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	/* struct snd_soc_codec *codec = escore_priv.codec; */
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;

	value = es705_read(NULL, reg);
	ucontrol->value.integer.value[0] = value;

	return 0;
}

static int es705_put_control_enum(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e =
		(struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	unsigned int value;
	int rc = 0;

	value = ucontrol->value.enumerated.item[0];
	rc = es705_write(NULL, reg, value);

	return 0;
}

static int es705_get_control_enum(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e =
		(struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	unsigned int value;

	value = es705_read(NULL, reg);

	ucontrol->value.enumerated.item[0] = value;

	return 0;
}

static int es705_get_power_control_enum(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e =
		(struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	unsigned int value;

	/* Don't read if already in Sleep Mode */
	if (escore_priv.pm_state == ES705_POWER_SLEEP)
		value = escore_priv.es705_power_state;
	else
		value = es705_read(NULL, reg);

	ucontrol->value.enumerated.item[0] = value;

	return 0;
}

static int es705_put_power_control_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	unsigned int value;
	int rc = 0;

	value = ucontrol->value.enumerated.item[0];
	dev_dbg(escore_priv.dev, "%s(): Previous power state = %s, power set cmd = %s\n",
		__func__, power_state[escore_priv.pm_state],
		power_state_cmd[escore_priv.es705_power_state]);
	dev_dbg(escore_priv.dev, "%s(): Requested power set cmd = %s\n",
		__func__, power_state_cmd[value]);

	if (value == 0 || value == ES705_SET_POWER_STATE_MP_SLEEP ||
		value == ES705_SET_POWER_STATE_MP_CMD) {
		dev_err(escore_priv.dev, "%s(): Unsupported state in es705\n",
			__func__);
		rc = -EINVAL;
		goto es705_put_power_control_enum_exit;
	} else {
		if ((escore_priv.pm_state == ES705_POWER_SLEEP) &&
			(value != ES705_SET_POWER_STATE_NORMAL) &&
			(value != ES705_SET_POWER_STATE_VS_OVERLAY)) {
			dev_err(escore_priv.dev, "%s(): ES705 is in sleep mode."
				" Select the Normal Mode or Overlay"
				" if in Low Power mode.\n", __func__);
			rc = -EPERM;
			goto  es705_put_power_control_enum_exit;
		}

		if (value == ES705_SET_POWER_STATE_SLEEP) {
			dev_dbg(escore_priv.dev, "%s(): Activate Sleep Request\n",
						__func__);
			es705_sleep_request(&escore_priv);
		} else if (value == ES705_SET_POWER_STATE_NORMAL) {
			/* Overlay mode doesn't need wakeup */
			if (escore_priv.es705_power_state !=
				ES705_SET_POWER_STATE_VS_OVERLAY) {
				rc = es705_wakeup(&escore_priv);
				if (rc)
					goto es705_put_power_control_enum_exit;
			}
			rc = es705_write(NULL, ES705_POWER_STATE, value);
			if (rc) {
				dev_err(escore_priv.dev, "%s(): Power state command write failed\n",
					__func__);
				goto es705_put_power_control_enum_exit;
			}
			/* Wait for 100ms to switch from Overlay mode */
			msleep(100);
			escore_priv.es705_power_state =
				ES705_SET_POWER_STATE_NORMAL;
			escore_priv.mode = STANDARD;

		} else if (value == ES705_SET_POWER_STATE_VS_LOWPWR) {
			if (escore_priv.es705_power_state ==
				ES705_SET_POWER_STATE_VS_OVERLAY) {
				int retry =
					MAX_RETRY_TO_SWITCH_TO_LOW_POWER_MODE;
				rc = es705_write(NULL,
					ES705_VS_INT_OSC_MEASURE_START, 0);
				if (rc) {
					dev_err(escore_priv.dev, "%s(): OSC Measure Start fail\n",
						__func__);
					goto es705_put_power_control_enum_exit;
				}
				do {
					/*
					 * Wait 20ms each time before reading
					 * up to 100ms
					 */
					msleep(20);
					rc = es705_read(NULL,
					ES705_VS_INT_OSC_MEASURE_STATUS);

					if (rc < 0)
						break;
					dev_dbg(escore_priv.dev, "%s(): OSC Measure Status = 0x%04x\n",
						__func__, rc);
				} while (rc && --retry);

				if (rc < 0) {
					dev_err(escore_priv.dev,
						"%s(): OSC Measure Read Status fail\n",
						__func__);
					goto es705_put_power_control_enum_exit;
				} else if (rc > 0) {
					dev_err(escore_priv.dev, "%s(): Unexpected OSC Measure Status = 0x%04x\n",
						__func__, rc);
					dev_err(escore_priv.dev, "%s(): Can't switch to Low Power Mode\n",
						__func__);
					goto es705_put_power_control_enum_exit;
				}
				dev_dbg(escore_priv.dev, "%s(): Activate Low Power Mode\n",
					__func__);
				es705_write(NULL, ES705_POWER_STATE, value);
				escore_priv.es705_power_state =
					ES705_SET_POWER_STATE_VS_LOWPWR;
				escore_priv.pm_state = ES705_POWER_SLEEP;
				goto es705_put_power_control_enum_exit;
			} else {
				dev_err(escore_priv.dev, "%s(): ES705 should be in VS Overlay"
					"mode. Select the VS Overlay Mode.\n",
					__func__);
				rc = -EPERM;
				goto es705_put_power_control_enum_exit;
			}
		} else if (value == ES705_SET_POWER_STATE_VS_OVERLAY) {
			if (escore_priv.es705_power_state ==
					ES705_SET_POWER_STATE_VS_LOWPWR) {
				rc = es705_wakeup(&escore_priv);
				if (rc)
					goto es705_put_power_control_enum_exit;
				escore_priv.es705_power_state =
					     ES705_SET_POWER_STATE_VS_OVERLAY;
			} else {
				rc = es705_write(NULL, reg, value);
				if (rc) {
					dev_err(escore_priv.dev, "%s(): Power state command write failed\n",
						__func__);
					goto es705_put_power_control_enum_exit;
				} else {
					escore_priv.es705_power_state =
					ES705_SET_POWER_STATE_VS_OVERLAY;

					/* wait es705 SBL mode */
					msleep(50);
					escore_vs_load(&escore_priv);
				}
			}
		}
	}

es705_put_power_control_enum_exit:
	dev_dbg(escore_priv.dev, "%s(): Current power state = %s, power set cmd = %s\n",
		__func__, power_state[escore_priv.pm_state],
		power_state_cmd[escore_priv.es705_power_state]);
	return rc;
}

static int es705_get_rx1_route_enable_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = escore_priv.flag.rx1_route_enable;
	dev_dbg(escore_priv.dev, "%s(): rx1_route_enable = %d\n",
		__func__, escore_priv.flag.rx1_route_enable);

	return 0;
}

static int es705_put_rx1_route_enable_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	escore_priv.flag.rx1_route_enable = ucontrol->value.integer.value[0];
	if (ucontrol->value.integer.value[0])
		es705_wakeup_request(&escore_priv);
	else
		es705_sleep_queue(&escore_priv);

	dev_dbg(escore_priv.dev, "%s(): rx1_route_enable = %d\n",
		__func__, escore_priv.flag.rx1_route_enable);

	return 0;
}

static int es705_get_auto_power_preset_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = escore_priv.auto_power_preset;
	dev_dbg(escore_priv.dev, "%s(): auto_power_preset = %d\n",
		__func__, escore_priv.auto_power_preset);

	return 0;
}

static int es705_put_auto_power_preset_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	escore_priv.auto_power_preset = ucontrol->value.integer.value[0];

	dev_dbg(escore_priv.dev, "%s(): auto_power_preset = %d\n",
		__func__, escore_priv.auto_power_preset);

	return 0;
}

static int es705_get_power_level_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	u32 es_get_power_level = ES705_GET_POWER_LEVEL << 16;
	u32 rspn = 0;
	int rc;

	rc = escore_cmd(&escore_priv, es_get_power_level, &rspn);
	if (rc < 0) {
		dev_err(escore_priv.dev, "codec reg read err %d()", rc);
		return rc;
	}

	ucontrol->value.enumerated.item[0] = rspn & 0x0000ffff;
	dev_dbg(escore_priv.dev, "%s: Response 0x%08X", __func__, rspn);
	return 0;
}

static int es705_put_power_level_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int es705_get_tx1_route_enable_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = escore_priv.flag.tx1_route_enable;
	dev_dbg(escore_priv.dev, "%s(): tx1_route_enable = %d\n",
		__func__, escore_priv.flag.tx1_route_enable);

	return 0;
}

static int es705_put_tx1_route_enable_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	escore_priv.flag.tx1_route_enable = ucontrol->value.integer.value[0];
	if (ucontrol->value.integer.value[0])
		es705_wakeup_request(&escore_priv);
	else
		es705_sleep_queue(&escore_priv);

	dev_dbg(escore_priv.dev, "%s(): tx1_route_enable = %d\n",
		__func__, escore_priv.flag.tx1_route_enable);

	return 0;
}

static int es705_get_rx2_route_enable_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = escore_priv.flag.rx2_route_enable;
	dev_dbg(escore_priv.dev, "%s(): rx2_route_enable = %d\n",
		__func__, escore_priv.flag.rx2_route_enable);

	return 0;
}

static int es705_put_rx2_route_enable_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	escore_priv.flag.rx2_route_enable = ucontrol->value.integer.value[0];
	if (ucontrol->value.integer.value[0])
		es705_wakeup_request(&escore_priv);
	else
		es705_sleep_queue(&escore_priv);

	dev_dbg(escore_priv.dev, "%s(): rx2_route_enable = %d\n",
		__func__, escore_priv.flag.rx2_route_enable);

	return 0;
}

static int es705_get_ns_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	dev_dbg(escore_priv.dev, "%s(): NS = %d\n",
		__func__, escore_priv.flag.ns);
	ucontrol->value.enumerated.item[0] = escore_priv.flag.ns;

	return 0;
}

static int es705_put_ns_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	int value = ucontrol->value.enumerated.item[0];
	int rc = 0;
	dev_dbg(escore_priv.dev, "%s(): NS = %d\n", __func__, value);

	escore_priv.flag.ns = value;

	/* 0 = NS off, 1 = NS on*/
	if (value)
		rc = es705_write(NULL, ES705_PRESET,
			ES705_NS_ON_PRESET);
	else
		rc = es705_write(NULL, ES705_PRESET,
			ES705_NS_OFF_PRESET);

	return rc;
}

static int es705_get_sw_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int es705_put_sw_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	int value = ucontrol->value.enumerated.item[0];
	int rc = 0;
	dev_dbg(escore_priv.dev, "%s(): SW = %d\n", __func__, value);

	/* 0 = off, 1 = on*/
	if (value)
		rc = es705_write(NULL, ES705_PRESET,
			ES705_SW_ON_PRESET);
	else
		rc = es705_write(NULL, ES705_PRESET,
			ES705_SW_OFF_PRESET);

	return rc;
}

static int es705_get_sts_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int es705_put_sts_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	int value = ucontrol->value.enumerated.item[0];
	int rc = 0;
	dev_dbg(escore_priv.dev, "%s(): STS = %d\n", __func__, value);

	/* 0 = off, 1 = on*/
	if (value)
		rc = es705_write(NULL, ES705_PRESET,
			ES705_STS_ON_PRESET);
	else
		rc = es705_write(NULL, ES705_PRESET,
			ES705_STS_OFF_PRESET);

	return rc;
}

static int es705_get_rx_ns_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int es705_put_rx_ns_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	int value = ucontrol->value.enumerated.item[0];
	int rc = 0;
	dev_dbg(escore_priv.dev, "%s(): RX_NS = %d\n", __func__, value);

	/* 0 = off, 1 = on*/
	if (value)
		rc = es705_write(NULL, ES705_PRESET,
			ES705_RX_NS_ON_PRESET);
	else
		rc = es705_write(NULL, ES705_PRESET,
			ES705_RX_NS_OFF_PRESET);

	return rc;
}

static int es705_get_wnf_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int es705_put_wnf_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	int value = ucontrol->value.enumerated.item[0];
	int rc = 0;
	dev_dbg(escore_priv.dev, "%s(): WNF = %d\n", __func__, value);

	/* 0 = off, 1 = on */
	if (value)
		rc = es705_write(NULL, ES705_PRESET,
			ES705_WNF_ON_PRESET);
	else
		rc = es705_write(NULL, ES705_PRESET,
			ES705_WNF_OFF_PRESET);

	return rc;
}

static int es705_get_bwe_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int es705_put_bwe_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	int value = ucontrol->value.enumerated.item[0];
	int rc = 0;
	dev_dbg(escore_priv.dev, "%s(): BWE = %d\n", __func__, value);

	/* 0 = off, 1 = on */
	if (value)
		rc = es705_write(NULL, ES705_PRESET,
			ES705_BWE_ON_PRESET);
	else
		rc = es705_write(NULL, ES705_PRESET,
			ES705_BWE_OFF_PRESET);

	return rc;
}

static int es705_get_avalon_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int es705_put_avalon_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	int value = ucontrol->value.enumerated.item[0];
	int rc = 0;
	dev_dbg(escore_priv.dev, "%s(): Avalon Wind Noise = %d\n",
		__func__, value);

	/* 0 = off, 1 = on */
	if (value)
		rc = es705_write(NULL, ES705_PRESET,
			ES705_AVALON_WN_ON_PRESET);
	else
		rc = es705_write(NULL, ES705_PRESET,
			ES705_AVALON_WN_OFF_PRESET);

	return rc;
}

static int es705_get_vbb_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int es705_put_vbb_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	int value = ucontrol->value.enumerated.item[0];
	int rc = 0;
	dev_dbg(escore_priv.dev, "%s(): Virtual Bass Boost = %d\n",
		__func__, value);

	/* 0 = off, 1 = on */
	if (value)
		rc = es705_write(NULL, ES705_PRESET,
			ES705_VBB_ON_PRESET);
	else
		rc = es705_write(NULL, ES705_PRESET,
			ES705_VBB_OFF_PRESET);

	return rc;
}

static int es705_get_aud_zoom(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	dev_dbg(escore_priv.dev, "%s(): Zoom = %d\n",
		__func__, escore_priv.flag.zoom);
	ucontrol->value.enumerated.item[0] = escore_priv.flag.zoom;

	return 0;
}

static int es705_put_aud_zoom(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	int value = ucontrol->value.enumerated.item[0];
	int rc = 0;
	dev_dbg(escore_priv.dev, "%s(): Zoom = %d\n", __func__, value);

	escore_priv.flag.zoom = value;

	if (value == ES705_AUD_ZOOM_NARRATOR) {
		rc = es705_write(NULL, ES705_PRESET,
			ES705_AUD_ZOOM_PRESET);
		rc = es705_write(NULL, ES705_PRESET,
			ES705_AUD_ZOOM_NARRATOR_PRESET);
	} else if (value == ES705_AUD_ZOOM_SCENE) {
		rc = es705_write(NULL, ES705_PRESET,
			ES705_AUD_ZOOM_PRESET);
		rc = es705_write(NULL, ES705_PRESET,
			ES705_AUD_ZOOM_SCENE_PRESET);
	} else if (value == ES705_AUD_ZOOM_NARRATION) {
		rc = es705_write(NULL, ES705_PRESET,
			ES705_AUD_ZOOM_PRESET);
		rc = es705_write(NULL, ES705_PRESET,
			ES705_AUD_ZOOM_NARRATION_PRESET);
	} else
		rc = es705_write(NULL, ES705_PRESET, 0);

	return rc;
}

/* Get for streming is not avaiable. Tinymix "set" method first executes get
 * and then put method. Thus dummy get method is implemented. */
static int es705_get_streaming_select(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = -1;

	return 0;
}

int es705_set_streaming(struct escore_priv *escore, int value)
{
	u32 resp;
	return escore_cmd(escore,
		es705_streaming_cmds[escore->streamdev.intf] | value, &resp);
}

int es705_remote_route_enable(struct snd_soc_dai *dai)
{
	dev_dbg(escore_priv.dev, "%s():dai->name = %s dai->id = %d\n",
		__func__, dai->name, dai->id);

	switch (dai->id) {
	case ES705_SLIM_1_PB:
		return escore_priv.flag.rx1_route_enable;
	case ES705_SLIM_1_CAP:
		return escore_priv.flag.tx1_route_enable;
	case ES705_SLIM_2_PB:
		return escore_priv.flag.rx2_route_enable;
	default:
		return 0;
	}
}
EXPORT_SYMBOL_GPL(es705_remote_route_enable);

static int es705_put_internal_route(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	es705_switch_route(ucontrol->value.integer.value[0]);
	escore_priv.auto_power_preset = 1;
	return 0;
}

static int es705_get_internal_route(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct escore_priv *es705 = &escore_priv;

	ucontrol->value.integer.value[0] = es705->internal_route_num;

	return 0;
}

static int es705_put_internal_rate(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct escore_priv *es705 = &escore_priv;
	const u32 *rate_macro = NULL;
	const u32 *power_preset = NULL;
	int rc = 0;
	dev_dbg(es705->dev, "%s:internal_rate  %d ucontrol %d auto_power %d",
		__func__, (int)es705->internal_rate,
		(int)ucontrol->value.enumerated.item[0],
		es705->auto_power_preset);

	switch (ucontrol->value.enumerated.item[0]) {
	case RATE_NB:
		rate_macro =
			es705_route_config[es705->internal_route_num].nb;
		power_preset =
			es705_route_config[es705->internal_route_num].pnb;
		break;
	case RATE_WB:
		rate_macro =
			es705_route_config[es705->internal_route_num].wb;
		power_preset =
			es705_route_config[es705->internal_route_num].pwb;
		break;
	case RATE_SWB:
		rate_macro =
			es705_route_config[es705->internal_route_num].swb;
		power_preset =
			es705_route_config[es705->internal_route_num].pswb;
		break;
	case RATE_FB:
		rate_macro =
			es705_route_config[es705->internal_route_num].fb;
		power_preset =
			es705_route_config[es705->internal_route_num].pfb;
		break;
	default:
		break;
	}

	if (!rate_macro) {
		dev_err(es705->dev, "%s(): internal rate, %d, out of range\n",
			__func__, ucontrol->value.enumerated.item[0]);
		return -EINVAL;
	}

	if (!power_preset)
		power_preset = pxx_default_power_preset;
	dev_dbg(es705->dev, "%s: power_preset[0] x%08X",
			__func__, power_preset[0]);

	rc = escore_write_block(es705, rate_macro);
	if (es705->auto_power_preset) {
		usleep_range(20000, 20000);
		dev_dbg(es705->dev, "%s: Applying power preset 0x%08X",
				__func__, power_preset[0]);
		rc = escore_write_block(es705, power_preset);
	}
	es705->internal_rate = ucontrol->value.enumerated.item[0];

	return rc;
}

static int es705_get_internal_rate(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct escore_priv *es705 = &escore_priv;

	ucontrol->value.enumerated.item[0] = es705->internal_rate;
	dev_dbg(es705->dev, "%s():es705->internal_rate = %d ucontrol = %d\n",
		__func__, (int)es705->internal_rate,
		(int)ucontrol->value.enumerated.item[0]);

	return 0;
}

static int es705_put_preset_value(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	int rc = 0;

	value = ucontrol->value.integer.value[0];

	rc = es705_write(NULL, reg, value);
	if (rc) {
		dev_err(escore_priv.dev, "%s(): Set Preset failed\n",
			__func__);
		return rc;
	}

	escore_priv.preset = value;

	return rc;
}

static int es705_get_preset_value(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = escore_priv.preset;

	return 0;
}
static int es705_get_audio_custom_profile(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int es705_put_audio_custom_profile(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
/*	int index = ucontrol->value.integer.value[0];

	if (index < ES705_CUSTOMER_PROFILE_MAX)
		escore_write_block(&escore_priv,
				  &es705_audio_custom_profiles[index][0]);
*/
	return 0;
}

static int es705_ap_put_tx1_ch_cnt(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	escore_priv.ap_tx1_ch_cnt = ucontrol->value.enumerated.item[0] + 1;
	return 0;
}

static int es705_ap_get_tx1_ch_cnt(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct escore_priv *es705 = &escore_priv;

	ucontrol->value.enumerated.item[0] = es705->ap_tx1_ch_cnt - 1;

	return 0;
}

static const char * const es705_ap_tx1_ch_cnt_texts[] = {
	"One", "Two"
};
static const struct soc_enum es705_ap_tx1_ch_cnt_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0,
			ARRAY_SIZE(es705_ap_tx1_ch_cnt_texts),
			es705_ap_tx1_ch_cnt_texts);

static const char * const es705_vs_power_state_texts[] = {
	"None", "Sleep", "MP_Sleep", "MP_Cmd", "Normal", "Overlay", "Low_Power"
};
static const struct soc_enum es705_vs_power_state_enum =
	SOC_ENUM_SINGLE(ES705_POWER_STATE, 0,
			ARRAY_SIZE(es705_vs_power_state_texts),
			es705_vs_power_state_texts);

/* generic gain translation */
static int es705_index_to_gain(int min, int step, int index)
{
	return	min + (step * index);
}
static int es705_gain_to_index(int min, int step, int gain)
{
	return	(gain - min) / step;
}

/* dereverb gain */
static int es705_put_dereverb_gain_value(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	int rc = 0;

	if (ucontrol->value.integer.value[0] <= 12) {
		value = es705_index_to_gain(-12, 1,
				  ucontrol->value.integer.value[0]);
		rc = es705_write(NULL, reg, value);
	}

	return rc;
}

static int es705_get_dereverb_gain_value(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;

	value = es705_read(NULL, reg);
	ucontrol->value.integer.value[0] = es705_gain_to_index(-12, 1, value);

	return 0;
}

/* bwe high band gain */
static int es705_put_bwe_high_band_gain_value(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	int rc = 0;

	if (ucontrol->value.integer.value[0] <= 30) {
		value = es705_index_to_gain(-10, 1,
				  ucontrol->value.integer.value[0]);
		rc = es705_write(NULL, reg, value);
	}

	return 0;
}

static int es705_get_bwe_high_band_gain_value(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;

	value = es705_read(NULL, reg);
	ucontrol->value.integer.value[0] = es705_gain_to_index(-10, 1, value);

	return 0;
}

/* bwe max snr */
static int es705_put_bwe_max_snr_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	int rc = 0;

	if (ucontrol->value.integer.value[0] <= 70) {
		value = es705_index_to_gain(-20, 1,
				  ucontrol->value.integer.value[0]);
		rc = es705_write(NULL, reg, value);
	}

	return 0;
}

static int es705_get_bwe_max_snr_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;

	value = es705_read(NULL, reg);
	ucontrol->value.integer.value[0] = es705_gain_to_index(-20, 1, value);

	return 0;
}

static const char * const es705_mic_config_texts[] = {
	"CT Multi-mic", "FT Multi-mic", "DV 1-mic", "EXT 1-mic", "BT 1-mic",
	"CT ASR Multi-mic", "FT ASR Multi-mic", "EXT ASR 1-mic", "FT ASR 1-mic",
};
static const struct soc_enum es705_mic_config_enum =
	SOC_ENUM_SINGLE(ES705_MIC_CONFIG, 0,
			ARRAY_SIZE(es705_mic_config_texts),
			es705_mic_config_texts);

static const char * const es705_aec_mode_texts[] = {
	"Off", "On", "rsvrd2", "rsvrd3", "rsvrd4", "On half-duplex"
};
static const struct soc_enum es705_aec_mode_enum =
	SOC_ENUM_SINGLE(ES705_AEC_MODE, 0, ARRAY_SIZE(es705_aec_mode_texts),
			es705_aec_mode_texts);

static const char * const es705_algo_rates_text[] = {
	"fs=8khz", "fs=16khz", "fs=24khz", "fs=48khz", "fs=96khz", "fs=192khz"
};
static const struct soc_enum es705_algo_sample_rate_enum =
	SOC_ENUM_SINGLE(ES705_ALGO_SAMPLE_RATE, 0,
			ARRAY_SIZE(es705_algo_rates_text),
			es705_algo_rates_text);
static const struct soc_enum es705_algo_mix_rate_enum =
	SOC_ENUM_SINGLE(ES705_MIX_SAMPLE_RATE, 0,
			ARRAY_SIZE(es705_algo_rates_text),
			es705_algo_rates_text);

static const char * const es705_internal_rate_text[] = {
	"NB", "WB", "SWB", "FB"
};
static const struct soc_enum es705_internal_rate_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0,
			ARRAY_SIZE(es705_internal_rate_text),
			es705_internal_rate_text);

static const char * const es705_algorithms_text[] = {
	"None", "VP", "Two CHREC", "AUDIO", "Four CHPASS"
};
static const struct soc_enum es705_algorithms_enum =
	SOC_ENUM_SINGLE(ES705_ALGO_SAMPLE_RATE, 0,
			ARRAY_SIZE(es705_algorithms_text),
			es705_algorithms_text);
static const char * const es705_off_on_texts[] = {
	"Off", "On"
};
static const char * const es705_audio_zoom_texts[] = {
	"disabled", "Narrator", "Scene", "Narration"
};
static const struct soc_enum es705_veq_enable_enum =
	SOC_ENUM_SINGLE(ES705_VEQ_ENABLE, 0, ARRAY_SIZE(es705_off_on_texts),
			es705_off_on_texts);
static const struct soc_enum es705_dereverb_enable_enum =
	SOC_ENUM_SINGLE(ES705_DEREVERB_ENABLE, 0,
			ARRAY_SIZE(es705_off_on_texts),
			es705_off_on_texts);
static const struct soc_enum es705_bwe_enable_enum =
	SOC_ENUM_SINGLE(ES705_BWE_ENABLE, 0, ARRAY_SIZE(es705_off_on_texts),
			es705_off_on_texts);
static const struct soc_enum es705_bwe_post_eq_enable_enum =
	SOC_ENUM_SINGLE(ES705_BWE_POST_EQ_ENABLE, 0,
			ARRAY_SIZE(es705_off_on_texts),
			es705_off_on_texts);
static const struct soc_enum es705_algo_processing_enable_enum =
	SOC_ENUM_SINGLE(ES705_ALGO_PROCESSING, 0,
			ARRAY_SIZE(es705_off_on_texts),
			es705_off_on_texts);
static const struct soc_enum es705_ns_enable_enum =
	SOC_ENUM_SINGLE(ES705_PRESET, 0, ARRAY_SIZE(es705_off_on_texts),
			es705_off_on_texts);
static const struct soc_enum es705_audio_zoom_enum =
	SOC_ENUM_SINGLE(ES705_PRESET, 0, ARRAY_SIZE(es705_audio_zoom_texts),
			es705_audio_zoom_texts);
static const struct soc_enum es705_rx_enable_enum =
	SOC_ENUM_SINGLE(ES705_RX_ENABLE, 0, ARRAY_SIZE(es705_off_on_texts),
			es705_off_on_texts);
static const struct soc_enum es705_sw_enable_enum =
	SOC_ENUM_SINGLE(ES705_PRESET, 0, ARRAY_SIZE(es705_off_on_texts),
			es705_off_on_texts);
static const struct soc_enum es705_sts_enable_enum =
	SOC_ENUM_SINGLE(ES705_PRESET, 0, ARRAY_SIZE(es705_off_on_texts),
			es705_off_on_texts);
static const struct soc_enum es705_rx_ns_enable_enum =
	SOC_ENUM_SINGLE(ES705_PRESET, 0, ARRAY_SIZE(es705_off_on_texts),
			es705_off_on_texts);
static const struct soc_enum es705_wnf_enable_enum =
	SOC_ENUM_SINGLE(ES705_PRESET, 0, ARRAY_SIZE(es705_off_on_texts),
			es705_off_on_texts);
static const struct soc_enum es705_bwe_preset_enable_enum =
	SOC_ENUM_SINGLE(ES705_PRESET, 0, ARRAY_SIZE(es705_off_on_texts),
			es705_off_on_texts);
static const struct soc_enum es705_avalon_wn_enable_enum =
	SOC_ENUM_SINGLE(ES705_PRESET, 0, ARRAY_SIZE(es705_off_on_texts),
			es705_off_on_texts);
static const struct soc_enum es705_vbb_enable_enum =
	SOC_ENUM_SINGLE(ES705_PRESET, 0, ARRAY_SIZE(es705_off_on_texts),
			es705_off_on_texts);

static int es705_put_power_state_enum(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int es705_get_power_state_enum(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}
static const char * const es705_power_state_texts[] = {
	"Sleep", "Active"
};
static const struct soc_enum es705_power_state_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0,
			ARRAY_SIZE(es705_power_state_texts),
			es705_power_state_texts);

static int es705_get_vs_wakeup_keyword(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct escore_voice_sense *voice_sense =
		(struct escore_voice_sense *)escore_priv.voice_sense;
	ucontrol->value.enumerated.item[0] = voice_sense->vs_wakeup_keyword;
	return 0;
}

static int es705_put_vs_wakeup_keyword(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct escore_voice_sense *voice_sense =
		(struct escore_voice_sense *)escore_priv.voice_sense;
	voice_sense->vs_wakeup_keyword = ucontrol->value.enumerated.item[0];
	return 0;
}

/* Voice Sense Detection Sensitivity */
static
int es705_put_vs_detection_sensitivity(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	int rc = 0;

	value = ucontrol->value.integer.value[0];
	dev_dbg(escore_priv.dev, "%s(): ucontrol = %ld value = %d\n",
		__func__, ucontrol->value.integer.value[0], value);

	rc = es705_write(NULL, reg, value);

	return rc;
}

static
int es705_get_vs_detection_sensitivity(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;

	value = es705_read(NULL, reg);
	ucontrol->value.integer.value[0] = value;

	dev_dbg(escore_priv.dev, "%s(): value = %d ucontrol = %ld\n",
		__func__, value, ucontrol->value.integer.value[0]);

	return 0;
}

static
int es705_put_vad_sensitivity(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	int rc = 0;

	value = ucontrol->value.integer.value[0];

	dev_dbg(escore_priv.dev, "%s(): ucontrol = %ld value = %d\n",
		__func__, ucontrol->value.integer.value[0], value);

	rc = es705_write(NULL, reg, value);

	return rc;
}

static
int es705_get_vad_sensitivity(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;

	value = es705_read(NULL, reg);
	ucontrol->value.integer.value[0] = value;

	dev_dbg(escore_priv.dev, "%s(): value = %d ucontrol = %ld\n",
		__func__, value, ucontrol->value.integer.value[0]);

	return 0;
}

static
int es705_put_cvs_preset_value(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	int rc = 0;

	value = ucontrol->value.integer.value[0];

	rc = es705_write(NULL, reg, value);
	if (rc) {
		dev_err(escore_priv.dev, "%s(): Set CVS Preset failed\n",
			__func__);
		return rc;
	}

	escore_priv.cvs_preset = value;

	msleep(96);

	return rc;
}

static
int es705_get_cvs_preset_value(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = escore_priv.cvs_preset;

	return 0;
}

static const char * const es705_vs_wakeup_keyword_texts[] = {
	"Default", "One", "Two", "Three", "Four"
};
static const struct soc_enum es705_vs_wakeup_keyword_enum =
	SOC_ENUM_SINGLE(ES705_VOICE_SENSE_SET_KEYWORD, 0,
			ARRAY_SIZE(es705_vs_wakeup_keyword_texts),
			es705_vs_wakeup_keyword_texts);

static const char * const es705_vs_event_texts[] = {
	"No Event", "Codec Event", "VS Keyword Event",
};
static const struct soc_enum es705_vs_event_enum =
	SOC_ENUM_SINGLE(ES705_VOICE_SENSE_EVENT, 0,
			ARRAY_SIZE(es705_vs_event_texts),
			es705_vs_event_texts);

static const char * const es705_vs_training_status_texts[] = {
	"busy", "Success", "Utterance Long",
	"Utterance Short", "Verification Failed",
};
static const struct soc_enum es705_vs_training_status_enum =
	SOC_ENUM_SINGLE(ES705_VOICE_SENSE_TRAINING_STATUS, 0,
			ARRAY_SIZE(es705_vs_training_status_texts),
			es705_vs_training_status_texts);

static const char * const es705_vs_training_record_texts[] = {
	"Previous Keyword", "Keyword_1", "Keyword_2",
	"Keyword_3", "Keyword_4", "Keyword_5",
};


static const char * const es705_vs_stored_keyword_texts[] = {
	"Put", "Get", "Clear"
};

static const struct soc_enum es705_vs_stored_keyword_enum =
	SOC_ENUM_SINGLE(ES705_VS_STORED_KEYWORD, 0,
			ARRAY_SIZE(es705_vs_stored_keyword_texts),
			es705_vs_stored_keyword_texts);

static const struct soc_enum es705_vs_training_record_enum =
	SOC_ENUM_SINGLE(ES705_VOICE_SENSE_TRAINING_RECORD, 0,
			ARRAY_SIZE(es705_vs_training_record_texts),
			es705_vs_training_record_texts);

static const char * const es705_vs_training_mode_texts[] = {
	"Detect Keyword", "N/A", "Train User-defined Keyword",
};

static const struct soc_enum es705_vs_training_mode_enum =
	SOC_ENUM_SINGLE(ES705_VOICE_SENSE_TRAINING_MODE, 0,
			ARRAY_SIZE(es705_vs_training_mode_texts),
			es705_vs_training_mode_texts);

static const char * const es705_power_level_texts[] = {
	"0 [Min]", "1", "2", "3", "4", "5", "6 [Max, Def]"
};

static const struct soc_enum es705_power_level_enum =
	SOC_ENUM_SINGLE(0, 0,
			ARRAY_SIZE(es705_power_level_texts),
			es705_power_level_texts);

static struct snd_kcontrol_new es705_digital_ext_snd_controls[] = {
//	SOC_SINGLE_EXT("Auto Power Preset", SND_SOC_NOPM, 0, 1, 0,
//				es705_get_auto_power_preset_value,
//				es705_put_auto_power_preset_value),
//	SOC_ENUM_EXT("Power Level", es705_power_level_enum,
//				es705_get_power_level_value,
//				es705_put_power_level_value),
//	SOC_SINGLE_EXT("ES705 RX1 Enable", SND_SOC_NOPM, 0, 1, 0,
//		       es705_get_rx1_route_enable_value,
//		       es705_put_rx1_route_enable_value),
//	SOC_SINGLE_EXT("ES705 TX1 Enable", SND_SOC_NOPM, 0, 1, 0,
//		       es705_get_tx1_route_enable_value,
//		       es705_put_tx1_route_enable_value),
//	SOC_SINGLE_EXT("ES705 RX2 Enable", SND_SOC_NOPM, 0, 1, 0,
//		       es705_get_rx2_route_enable_value,
//		       es705_put_rx2_route_enable_value),
//	SOC_ENUM_EXT("Mic Config", es705_mic_config_enum,
//		     es705_get_control_enum, es705_put_control_enum),
//	SOC_ENUM_EXT("AEC Mode", es705_aec_mode_enum,
//		     es705_get_control_enum, es705_put_control_enum),
//	SOC_ENUM_EXT("VEQ Enable", es705_veq_enable_enum,
//		     es705_get_control_enum, es705_put_control_enum),
//	SOC_ENUM_EXT("Dereverb Enable", es705_dereverb_enable_enum,
//		     es705_get_control_enum, es705_put_control_enum),
//	SOC_SINGLE_EXT("Dereverb Gain",
//		       ES705_DEREVERB_GAIN, 0, 100, 0,
//		       es705_get_dereverb_gain_value,
//		       es705_put_dereverb_gain_value),
//	SOC_ENUM_EXT("BWE Enable", es705_bwe_enable_enum,
//		     es705_get_control_enum, es705_put_control_enum),
//	SOC_SINGLE_EXT("BWE High Band Gain",
//		       ES705_BWE_HIGH_BAND_GAIN, 0, 100, 0,
//		       es705_get_bwe_high_band_gain_value,
//		       es705_put_bwe_high_band_gain_value),
//	SOC_SINGLE_EXT("BWE Max SNR",
//		       ES705_BWE_MAX_SNR, 0, 100, 0,
//		       es705_get_bwe_max_snr_value,
//		       es705_put_bwe_max_snr_value),
//	SOC_ENUM_EXT("BWE Post EQ Enable", es705_bwe_post_eq_enable_enum,
//		     es705_get_control_enum, es705_put_control_enum),
//	SOC_SINGLE_EXT("SLIMbus Link Multi Channel",
//		       ES705_SLIMBUS_LINK_MULTI_CHANNEL, 0, 65535, 0,
//		       es705_get_control_value, es705_put_control_value),
//	SOC_ENUM_EXT("Set Power State", es705_power_state_enum,
//		       es705_get_power_state_enum, es705_put_power_state_enum),
//	SOC_ENUM_EXT("Algorithm Processing", es705_algo_processing_enable_enum,
//		     es705_get_control_enum, es705_put_control_enum),
//	SOC_ENUM_EXT("Algorithm Sample Rate", es705_algo_sample_rate_enum,
//		     es705_get_control_enum, es705_put_control_enum),
//	SOC_ENUM_EXT("Algorithm", es705_algorithms_enum,
//		     es705_get_control_enum, es705_put_control_enum),
//	SOC_ENUM_EXT("Mix Sample Rate", es705_algo_mix_rate_enum,
//		     es705_get_control_enum, es705_put_control_enum),
	SOC_SINGLE_EXT("Internal Route",
		       SND_SOC_NOPM, 0, 100, 0, es705_get_internal_route,
		       es705_put_internal_route),
//	SOC_ENUM_EXT("Internal Rate", es705_internal_rate_enum,
//		      es705_get_internal_rate,
//		      es705_put_internal_rate),
//	SOC_SINGLE_EXT("Preset",
//		       ES705_PRESET, 0, 65535, 0, es705_get_preset_value,
//		       es705_put_preset_value),
//	SOC_SINGLE_EXT("Audio Custom Profile",
//		       SND_SOC_NOPM, 0, 100, 0, es705_get_audio_custom_profile,
//		       es705_put_audio_custom_profile),
//	SOC_ENUM_EXT("ES705-AP Tx Channels", es705_ap_tx1_ch_cnt_enum,
//		     es705_ap_get_tx1_ch_cnt, es705_ap_put_tx1_ch_cnt),
//	SOC_ENUM_EXT("Voice Sense Set Wakeup Word",
//		     es705_vs_wakeup_keyword_enum,
//		     es705_get_vs_wakeup_keyword, es705_put_vs_wakeup_keyword),
//	SOC_ENUM_EXT("Voice Sense Status",
//		     es705_vs_event_enum,
//		     es705_get_control_enum, NULL),
//	SOC_ENUM_EXT("Voice Sense Training Mode",
//			 es705_vs_training_mode_enum,
//			 es705_get_control_enum, es705_put_control_enum),
//	SOC_ENUM_EXT("Voice Sense Training Status",
//		     es705_vs_training_status_enum,
//		     es705_get_control_enum, NULL),
//	SOC_ENUM_EXT("Voice Sense Training Record",
//		     es705_vs_training_record_enum,
//		     NULL, es705_put_control_enum),
//	SOC_ENUM_EXT("Voice Sense Stored Keyword",
//		     es705_vs_stored_keyword_enum,
//		     NULL, escore_put_vs_stored_keyword),
//	SOC_SINGLE_EXT("Voice Sense Detect Sensitivity",
//			ES705_VOICE_SENSE_DETECTION_SENSITIVITY, 0, 10, 0,
//			es705_get_vs_detection_sensitivity,
//			es705_put_vs_detection_sensitivity),
//	SOC_SINGLE_EXT("Voice Activity Detect Sensitivity",
//			ES705_VOICE_ACTIVITY_DETECTION_SENSITIVITY, 0, 10, 0,
//			es705_get_vad_sensitivity,
//			es705_put_vad_sensitivity),
//	SOC_SINGLE_EXT("Continuous Voice Sense Preset",
//		       ES705_CVS_PRESET, 0, 65535, 0,
//		       es705_get_cvs_preset_value,
//		       es705_put_cvs_preset_value),
//	SOC_ENUM_EXT("ES705 Power State", es705_vs_power_state_enum,
//		     es705_get_power_control_enum,
//		     es705_put_power_control_enum),
//	SOC_ENUM_EXT("Noise Suppression", es705_ns_enable_enum,
//		       es705_get_ns_value,
//		       es705_put_ns_value),
//	SOC_ENUM_EXT("Audio Zoom", es705_audio_zoom_enum,
//		       es705_get_aud_zoom,
//		       es705_put_aud_zoom),
//	SOC_SINGLE_EXT("Enable/Disable Streaming PATH/Endpoint",
//		       ES705_FE_STREAMING, 0, 65535, 0,
//		       es705_get_streaming_select,
//		       es705_put_control_value),
//	SOC_ENUM_EXT("RX Enable", es705_rx_enable_enum,
//		       es705_get_control_enum,
//		       es705_put_control_enum),
//	SOC_ENUM_EXT("Stereo Widening", es705_sw_enable_enum,
//		       es705_get_sw_value,
//		       es705_put_sw_value),
//	SOC_ENUM_EXT("Speech Time Stretching", es705_sts_enable_enum,
//			   es705_get_sts_value,
//			   es705_put_sts_value),
//	SOC_ENUM_EXT("RX Noise Suppression", es705_rx_ns_enable_enum,
//			   es705_get_rx_ns_value,
//			   es705_put_rx_ns_value),
//	SOC_ENUM_EXT("Wind Noise Filter", es705_wnf_enable_enum,
//			   es705_get_wnf_value,
//			   es705_put_wnf_value),
//	SOC_ENUM_EXT("BWE Preset", es705_bwe_preset_enable_enum,
//			   es705_get_bwe_value,
//			   es705_put_bwe_value),
//	SOC_ENUM_EXT("AVALON Wind Noise", es705_avalon_wn_enable_enum,
//			   es705_get_avalon_value,
//			   es705_put_avalon_value),
//	SOC_ENUM_EXT("Virtual Bass Boost", es705_vbb_enable_enum,
//			   es705_get_vbb_value,
//			   es705_put_vbb_value),
};

static int es705_slim_set_channel_map(struct snd_soc_dai *dai,
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
int es705_slim_get_channel_map(struct snd_soc_dai *dai,
			       unsigned int *tx_num, unsigned int *tx_slot,
			       unsigned int *rx_num, unsigned int *rx_slot)
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
static int es705_set_bias_level(struct snd_soc_codec *codec,
				enum snd_soc_bias_level level)
{
	int rc = 0;

	switch (level) {
	case SND_SOC_BIAS_ON:
		break;

	case SND_SOC_BIAS_PREPARE:
		break;

	case SND_SOC_BIAS_STANDBY:
		break;

	case SND_SOC_BIAS_OFF:
		break;
	}
	codec->dapm.bias_level = level;

	return rc;
}

int remote_esxxx_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	/* es705_wakeup_request(&escore_priv);*/
	return 0;
}

int remote_esxxx_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	/* es705_sleep_queue(&escore_priv);*/
	return 0;
}


#ifdef CONFIG_PM
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
static int es705_codec_suspend(struct snd_soc_codec *codec)
#else
static int es705_codec_suspend(struct snd_soc_codec *codec, pm_message_t state)
#endif
{
	struct escore_priv *es705 = snd_soc_codec_get_drvdata(codec);

	es705_set_bias_level(codec, SND_SOC_BIAS_OFF);

	es705_sleep(es705);

	return 0;
}

static int es705_codec_resume(struct snd_soc_codec *codec)
{
	struct escore_priv *es705 = snd_soc_codec_get_drvdata(codec);

	es705_wakeup(es705);

	es705_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	return 0;
}
#else
#define es705_codec_suspend NULL
#define es705_codec_resume NULL
#endif

#if defined(CONFIG_SND_SOC_ES_I2S)
int es705_i2s_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	es705_wakeup_request(&escore_priv);
	return 0;
}

void es705_i2s_shutdown(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	es705_sleep_queue(&escore_priv);
}
#endif

int es705_remote_add_codec_controls(struct snd_soc_codec *codec)
{
	int rc;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
	rc = snd_soc_add_codec_controls(codec, es705_digital_ext_snd_controls,
				ARRAY_SIZE(es705_digital_ext_snd_controls));
#else
	rc = snd_soc_add_controls(codec, es705_digital_ext_snd_controls,
				ARRAY_SIZE(es705_digital_ext_snd_controls));
#endif
	if (rc)
		dev_err(codec->dev,
		  "%s(): es705_digital_ext_snd_controls failed\n", __func__);

	return rc;
}

static int es705_codec_probe(struct snd_soc_codec *codec)
{
	struct escore_priv *es705 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s()\n", __func__);
	es705->codec = codec;

	codec->control_data = snd_soc_codec_get_drvdata(codec);


	es705_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	return 0;
}

static int  es705_codec_remove(struct snd_soc_codec *codec)
{
	struct escore_priv *es705 = snd_soc_codec_get_drvdata(codec);

	es705_set_bias_level(codec, SND_SOC_BIAS_OFF);

	kfree(es705);

	return 0;
}

struct snd_soc_codec_driver soc_codec_dev_es705 = {
	.probe =	es705_codec_probe,
	.remove =	es705_codec_remove,
	.suspend =	es705_codec_suspend,
	.resume =	es705_codec_resume,
	.read =		es705_read,
	.write =	es705_write,
	.set_bias_level =	es705_set_bias_level,
};

irqreturn_t es705_irq_work(int irq, void *data)
{
	struct escore_priv *escore = (struct escore_priv *)data;
	int rc;
	u32 event_type, cmd = 0;

	if (!escore) {
		pr_err("%s(): Invalid IRQ data\n", __func__);
		goto irq_exit;
	}

	if (escore->cvs_preset != 0xFFFF &&
			escore->cvs_preset != 0) {
		escore->es705_power_state = ES705_SET_POWER_STATE_NORMAL;
		escore->pm_state = ES705_POWER_AWAKE;
		escore->mode = STANDARD;
		msleep(30);
	} else if (escore->es705_power_state ==
				ES705_SET_POWER_STATE_VS_LOWPWR) {
		rc = es705_wakeup(escore);
		if (rc) {
			pr_err("%s(): Failed to wakeup\n",
					__func__);
			goto irq_exit;
		}
	}
	cmd = ES_GET_EVENT << 16;

	rc = escore_cmd(escore, cmd, &event_type);
	if (rc < 0) {
		pr_err("%s(): Error reading IRQ event\n", __func__);
		goto irq_exit;
	}

	if (event_type != ES_NO_EVENT) {
		pr_debug("%s(): Notify subscribers about 0x%04x event\n",
				__func__, event_type);
		blocking_notifier_call_chain(escore->irq_notifier_list,
				event_type, escore);
	}

irq_exit:
	return IRQ_HANDLED;
}

static BLOCKING_NOTIFIER_HEAD(es705_irq_notifier_list);

int es705_core_probe(struct device *dev)
{
	struct esxxx_platform_data *pdata = dev->platform_data;
	int rc = 0;
	const char *fw_filename = "audience-es705-fw.bin";
	const char *vs_filename = "audience-es705-vs.bin";
	struct escore_voice_sense *voice_sense;

	if (pdata == NULL) {
		dev_err(dev, "%s(): pdata is NULL", __func__);
		rc = -EIO;
		goto pdata_error;
	}

	escore_priv.dev = dev;
	escore_priv.pdata = pdata;
	escore_priv.fw_requested = 0;
	if (pdata->esxxx_clk_cb)
		pdata->esxxx_clk_cb(1);

#if defined(CONFIG_SND_SOC_ES_HIGH_BW_BUS_UART)
	escore_uart.baudrate_bootloader = UART_TTY_BAUD_RATE_460_8_K;
	escore_uart.baudrate_fw = UART_TTY_BAUD_RATE_3_M;
#endif

	escore_priv.boot_ops.bootup = es705_bootup;
	escore_priv.soc_codec_dev_escore = &soc_codec_dev_es705;
	escore_priv.dai = es705_dai;
	escore_priv.dai_nr = ES_NUM_CODEC_DAIS;
	escore_priv.api_addr_max = ES_API_ADDR_MAX;
	escore_priv.api_access = es705_api_access;
	escore_priv.irq_notifier_list = &es705_irq_notifier_list;

/*	escore_priv.dev_rdb = es705_dev_rdb,
	escore_priv.dev_wdb = es705_dev_wdb,
*/
/*	escore_priv.reg_cache = a300_reg_cache;*/

#if defined(CONFIG_SND_SOC_ES_I2S)
			escore_priv.i2s_dai_ops.startup = es705_i2s_startup;
			escore_priv.i2s_dai_ops.shutdown = es705_i2s_shutdown;
#endif

	escore_priv.flag.is_codec = 0;
	if (escore_priv.pri_intf == ES_SLIM_INTF) {
		escore_priv.slim_rx = slim_rx;
		escore_priv.slim_tx = slim_tx;
		escore_priv.slim_dai_data = slim_dai_data;
		escore_priv.slim_setup = es705_slim_setup;

		escore_priv.slim_rx_ports = ES_SLIM_RX_PORTS;
		escore_priv.slim_tx_ports = ES_SLIM_TX_PORTS;
		escore_priv.codec_slim_dais = ES_NUM_CODEC_SLIM_DAIS;

		escore_priv.slim_tx_port_to_ch_map = es705_slim_tx_port_to_ch;
		escore_priv.slim_rx_port_to_ch_map = es705_slim_rx_port_to_ch;

#if defined(CONFIG_ARCH_MSM)
		escore_priv.slim_dai_ops.get_channel_map =
			es705_slim_get_channel_map;
#endif
		escore_priv.slim_dai_ops.set_channel_map =
			es705_slim_set_channel_map;

#if defined(CONFIG_SND_SOC_ES_SLIM)
		/* Initialization of be_id goes here if required */
		escore_priv.slim_be_id = es705_slim_be_id;
#else
		escore_priv.slim_be_id = NULL;
#endif

		/* Initialization of _remote_ routines goes here if required */
		escore_priv.remote_cfg_slim_rx = NULL;
		escore_priv.remote_cfg_slim_tx = NULL;
		escore_priv.remote_close_slim_rx = NULL;
		escore_priv.remote_close_slim_tx = NULL;

		/* Set local_slim_ch_cfg to 0 for digital only codec */
		escore_priv.flag.local_slim_ch_cfg = 0;
		escore_priv.channel_dir = es705_channel_dir;

		escore_priv.sleep  = es705_slim_sleep;
		escore_priv.wakeup = es705_slim_wakeup;
		escore_priv.slim_setup(&escore_priv);

/*	}	else if (escore_priv.intf == ES_I2C_INTF) {

		escore_priv.sleep  = es705_i2c_sleep;
		escore_priv.wakeup = es705_i2c_wakeup;
*/
	}
#if defined(CONFIG_SND_SOC_ES_UART_STREAMDEV)
		escore_priv.streamdev = es_uart_streamdev;
#endif

	mutex_init(&escore_priv.pm_mutex);
	mutex_init(&escore_priv.wake_mutex);
	mutex_init(&escore_priv.abort_mutex);
	mutex_init(&escore_priv.streaming_mutex);
	mutex_init(&escore_priv.msg_list_mutex);
	mutex_init(&escore_priv.datablock_dev.datablock_mutex);
	mutex_init(&escore_priv.datablock_dev.datablock_read_mutex);

	init_waitqueue_head(&escore_priv.stream_in_q);
	init_completion(&escore_priv.cmd_compl);
	INIT_LIST_HEAD(&escore_priv.msg_list);

#ifdef CONFIG_SND_SOC_ES_SPI_SENSOR_HUB
	/* Add initialization for sensor input devices */
	rc = es705_sensor_hub_init_data_driver(&escore_priv);
	if (rc != 0) {
		dev_err(escore_priv.dev,
		    "[SPI]: %s - could not create input device\n",
		    __func__);
		goto sens_init_err;
	}
#endif

	rc = sysfs_create_group(&escore_priv.dev->kobj, &core_sysfs);
	if (rc) {
		dev_err(escore_priv.dev, "%s(): failed to create core sysfs entries: %d\n",
			__func__, rc);
	}

	dev_dbg(escore_priv.dev, "%s(): reset_gpio = %d\n", __func__,
		pdata->reset_gpio);
	if (pdata->reset_gpio != -1) {
#ifdef CONFIG_ARCH_MT6595
		mt_set_gpio_out(pdata->reset_gpio, 0);
#else
		rc = gpio_request(pdata->reset_gpio, "es705_reset");
		if (rc < 0) {
			dev_err(escore_priv.dev, "%s(): es705_reset request failed",
				__func__);
			goto reset_gpio_request_error;
		}
		rc = gpio_direction_output(pdata->reset_gpio, 0);
		if (rc < 0) {
			dev_err(escore_priv.dev, "%s(): es705_reset direction failed",
				__func__);
			goto reset_gpio_direction_error;
		}
#endif
	} else {
		dev_warn(escore_priv.dev, "%s(): es705_reset undefined\n",
			 __func__);
	}

	dev_dbg(escore_priv.dev, "%s(): wakeup_gpio = %d\n", __func__,
		pdata->wakeup_gpio);

	if (pdata->wakeup_gpio != -1) {
#ifdef CONFIG_ARCH_MT6595
		mt_set_gpio_out(pdata->wakeup_gpio, 0);
#else
		rc = gpio_request(pdata->wakeup_gpio, "es705_wakeup");
		if (rc < 0) {
			dev_err(escore_priv.dev, "%s(): es705_wakeup request failed",
				__func__);
			goto wakeup_gpio_request_error;
		}
		rc = gpio_direction_output(pdata->wakeup_gpio, 0);
		if (rc < 0) {
			dev_err(escore_priv.dev, "%s(): es705_wakeup direction failed",
				__func__);
			goto wakeup_gpio_direction_error;
		}
#endif
	} else {
		dev_warn(escore_priv.dev, "%s(): es705_wakeup undefined\n",
			 __func__);
	}

	rc = request_firmware((const struct firmware **)&escore_priv.standard,
			      fw_filename, escore_priv.dev);
	if (rc) {
		dev_err(escore_priv.dev, "%s(): request_firmware(%s) failed %d\n",
			__func__, fw_filename, rc);
		goto request_firmware_error;
	}

	escore_vs_init(&escore_priv);

	rc = escore_vs_request_firmware(&escore_priv, vs_filename);
	if (rc) {
		dev_err(escore_priv.dev,
			"%s(): request_firmware(%s) failed %d\n",
			__func__, vs_filename, rc);
		goto request_vs_firmware_error;
	}
	voice_sense = (struct escore_voice_sense *)escore_priv.voice_sense;

	/* No keyword parameters available until set. */
	voice_sense->vs_keyword_param_size = 0;

	/* read 32 bit abort keyword from offset 0x000C from VS firmware */
	escore_priv.vs_abort_kw = *(u32 *)((voice_sense)->vs->data + 0x000c);
	voice_sense->vs_irq = false;

	INIT_DELAYED_WORK(&escore_priv.sleep_work, es705_delayed_sleep);

	escore_priv.fw_requested = 1;

#ifndef CONFIG_ARCH_MT6595
	if (pdata->gpiob_gpio != -1) {
		rc = request_threaded_irq(gpio_to_irq(pdata->gpiob_gpio),
					  NULL,
					  es705_irq_work, IRQF_TRIGGER_RISING,
					  "es705_irq_work", &escore_priv);
		if (rc) {
			dev_err(escore_priv.dev, "%s(): event request_irq() failed\n",
				__func__);
			goto event_irq_request_error;
		}
		rc = irq_set_irq_wake(gpio_to_irq(pdata->gpiob_gpio), 1);
		if (rc < 0) {
			dev_err(escore_priv.dev, "%s(): set event irq wake failed\n",
				__func__);
			disable_irq(gpio_to_irq(pdata->gpiob_gpio));
			free_irq(gpio_to_irq(pdata->gpiob_gpio),
				 &escore_priv);
			goto event_irq_wake_error;
		}
	} else {
		dev_warn(escore_priv.dev, "%s(): es705_gpiob undefined\n",
			 __func__);
	}
#endif
	return rc;

event_irq_wake_error:
event_irq_request_error:
request_vs_firmware_error:
	release_firmware(escore_priv.standard);
request_firmware_error:
#ifndef CONFIG_ARCH_MT6595
wakeup_gpio_direction_error:
	gpio_free(pdata->wakeup_gpio);
wakeup_gpio_request_error:
reset_gpio_direction_error:
	gpio_free(pdata->reset_gpio);
reset_gpio_request_error:
#endif

#ifdef CONFIG_SND_SOC_ES_SPI_SENSOR_HUB
sens_init_err:
#endif

pdata_error:
	dev_dbg(escore_priv.dev, "%s(): exit with error\n", __func__);
	return rc;
}

static __init int es705_init(void)
{
	int rc = 0;

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

#if defined(CONFIG_SND_SOC_ES_I2C) || \
		defined(CONFIG_SND_SOC_ES_HIGH_BW_BUS_I2C)
	rc = escore_i2c_init();
#endif
#if defined(CONFIG_SND_SOC_ES_SPI) || \
		defined(CONFIG_SND_SOC_ES_HIGH_BW_BUS_SPI)
	rc = escore_spi_init();
#endif
#if defined(CONFIG_SND_SOC_ES_SLIM) || \
		defined(CONFIG_SND_SOC_ES_HIGH_BW_BUS_SLIM)
	rc = escore_slimbus_init();
#endif
#if defined(CONFIG_SND_SOC_ES_UART) || \
		defined(CONFIG_SND_SOC_ES_HIGH_BW_BUS_UART)
	rc = escore_uart_bus_init(&escore_priv);
#endif
	if (rc) {
		pr_debug("Error registering Audience driver: %d\n",
			rc);
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
module_init(es705_init);

static __exit void es705_exit(void)
{
	struct escore_voice_sense *voice_sense =
		(struct escore_voice_sense *) escore_priv.voice_sense;

	if (escore_priv.fw_requested) {
		release_firmware(escore_priv.standard);
		release_firmware(voice_sense->vs);
	}
#if defined(CONFIG_SND_SOC_ES_UART_STREAMDEV)
	escore_cdev_cleanup(&escore_priv);
#endif

#if defined(CONFIG_SND_SOC_ES705_I2C)
	i2c_del_driver(&es705_i2c_driver);
#else
	/* no support from QCOM to unregister
	 * slim_driver_unregister(&es705_slim_driver);
	 */
#endif

#ifdef CONFIG_SND_SOC_ES_SPI_SENSOR_HUB
	es705_sensor_hub_remove_data_driver(&escore_priv);
#endif
}
module_exit(es705_exit);


MODULE_DESCRIPTION("ASoC ES705 driver");
MODULE_AUTHOR("Greg Clemson <gclemson@audience.com>");
MODULE_AUTHOR("Genisim Tsilker <gtsilker@audience.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:es705-codec");
MODULE_FIRMWARE("audience-es705-fw.bin");
