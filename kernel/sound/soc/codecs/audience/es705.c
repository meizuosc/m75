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
#if defined(CONFIG_MQ100_SENSOR_HUB)
#define DEBUG
#endif

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
#include <linux/clk.h>
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
#include <linux/slimbus/slimbus.h>
#include <linux/spi/spi.h>
#include <linux/esxxx.h>
#include <linux/wait.h>
#include <linux/kernel.h>
#include "es705.h"
#include "es705-routes.h"
#include "es705-profiles.h"
#include "es705-slim.h"
#include "es705-i2c.h"
#include "es705-i2s.h"
#include "es705-spi.h"
#include "es705-cdev.h"
#include "es705-uart.h"
#include "es705-uart-common.h"

#if defined(CONFIG_MQ100_SENSOR_HUB)
#include "es705-api.h"
#endif

/* local function proto type */
static int es705_dev_rdb(struct es705_priv *es705, void *buf, int len);
static int es705_dev_wdb(struct es705_priv *es705, const void *buf, int len);

#define ES705_CMD_ACCESS_WR_MAX 2
#define ES705_CMD_ACCESS_RD_MAX 2
struct es705_api_access {
	u32 read_msg[ES705_CMD_ACCESS_RD_MAX];
	unsigned int read_msg_len;
	u32 write_msg[ES705_CMD_ACCESS_WR_MAX];
	unsigned int write_msg_len;
	unsigned int val_shift;
	unsigned int val_max;
};

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
struct es705_priv es705_priv = {
	.pm_state = ES705_POWER_AWAKE,

	.rx1_route_enable = 0,
	.tx1_route_enable = 0,
	.rx2_route_enable = 0,

	.vs_wakeup_keyword = 0,

	.ap_tx1_ch_cnt = 2,

	.es705_power_state = ES705_SET_POWER_STATE_NORMAL,
	.streamdev.intf = -1,
	.ns = 1,
#if defined(CONFIG_MQ100_SENSOR_HUB)
	.sleep_enable = 0,
#else
	.sleep_enable = 1,
#endif
	.sleep_delay = 3000, /* 3 seconds */
	.wake_count = 0,
	.sleep_abort = 0,

	.dev_rdb = es705_dev_rdb,
	.dev_wdb = es705_dev_wdb,
	.no_more_bit = 0,
#if defined(CONFIG_MQ100_SENSOR_HUB)
	.uart_app_rate = UART_RATE_4608,
#else
	.uart_app_rate = UART_RATE_3kk,
#endif
};

const char *esxxx_mode[] = {
	"SBL",
	"STANDARD",
	"VOICESENSE",
};

struct snd_soc_dai_driver es705_dai[];

/* indexed by ES705 INTF number */
u32 es705_streaming_cmds[4] = {
	0x90250200,		/* ES705_SLIM_INTF */
	0x90250000,		/* ES705_I2C_INTF  */
	0x90250300,		/* ES705_SPI_INTF  */
	0x90250100,		/* ES705_UART_INTF */
};

#if defined(CONFIG_MQ100_SENSOR_HUB)
static struct es705_rwdb_device_callbacks callbacks;
#endif

static void es705_sleep_queue(struct es705_priv *es705);
static void es705_wakeup_request(struct es705_priv *es705);

static bool is_sleep_aborted(void)
{
	bool rc = false;
	mutex_lock(&es705_priv.abort_mutex);
	if (es705_priv.sleep_abort == true) {
		es705_priv.sleep_abort = false;
		dev_dbg(es705_priv.dev, "Abort kw sent\n");
		rc = true;
	}
	mutex_unlock(&es705_priv.abort_mutex);
	return rc;
}

void es705_gpio_reset(struct es705_priv *es705)
{
	if (es705->pdata->reset_gpio < 0)
		return;

	gpio_set_value(es705->pdata->reset_gpio, 0);
	/* Wait 1 ms then pull Reset signal in High */
	usleep_range(1000, 1000);
	gpio_set_value(es705->pdata->reset_gpio, 1);
	/* Wait 10 ms then */
	usleep_range(10000, 10000);
	/* eSxxx is READY */
}

int es705_dev_rdb(struct es705_priv *es705, void *buf, int len)
{
	dev_dbg(es705->dev, "%s - default\n", __func__);
	return 0;
}

int es705_dev_wdb(struct es705_priv *es705, const void *buf, int len)
{
	dev_dbg(es705->dev, "%s - default\n", __func__);
	return 0;
}

#if defined(CONFIG_MQ100_SENSOR_HUB)
/* Max Size = PAGE_SIZE * 2 */
/*
 * Reads buf from es705 using rdb
 * @param:  buf - rdb data Max Size supported - 2*PAGE_SIZE
 *			Ensure buffer allocated
 *			has enough space for rdb
 * @param:  id - type specifier
 * @return: no. of bytes read
 */
int es705_rdb(void *buf, int id)
{
	struct es705_priv *es705 = &es705_priv;

	/* too high freq for dev_dbg! */
	/* dev_dbg(es705->dev, "%s", __func__);  */

	if (!buf)
		return 0;
	if (es705->dev_rdb)
		es705->dev_rdb(es705, buf, id);
	return es705->rdb_read_count;
}
EXPORT_SYMBOL_GPL(es705_rdb);

/*
 * Writes buf to es705 using wdb,
 * this function will prepend 0x802F 0xffff
 * @param: buf - wdb data
 * @param: len - length
 * @return: no. of bytes written
 */
int es705_wdb(const void *buf, int len)
{
	struct es705_priv *es705 = &es705_priv;
	dev_dbg(es705->dev, "%s", __func__);
	if (es705->dev_wdb)
		return es705->dev_wdb(es705, buf, len);
	return 0;
}
EXPORT_SYMBOL_GPL(es705_wdb);
#endif

int es705_write_block(struct es705_priv *es705, const u32 *cmd_block)
{
	u32 api_word;
	u8 msg[4];
	int rc = 0;

	dev_dbg(es705->dev, "%s(): pm_runtime_get_sync()", __func__);
	mutex_lock(&es705->api_mutex);
	while (*cmd_block != 0xffffffff) {
		api_word = cpu_to_le32(*cmd_block);
		memcpy(msg, (char *)&api_word, 4);
		es705->dev_write(es705, msg, 4);
		usleep_range(1000, 1000);
		dev_dbg(es705->dev, "%s(): msg = %02x%02x%02x%02x\n",
			__func__, msg[0], msg[1], msg[2], msg[3]);
		cmd_block++;
	}
	dev_dbg(es705->dev, "%s(): mutex unlock\n", __func__);
	mutex_unlock(&es705->api_mutex);
	dev_dbg(es705->dev, "%s(): pm_runtime_put_autosuspend()\n", __func__);

	return rc;
}



/* Note: this may only end up being called in a api locked context. In
 * that case the mutex locks need to be removed.
 */
int es705_read_vs_data_block(struct es705_priv *es705)
{
	/* This function is not re-entrant so avoid stack bloat. */
	static u8 block[ES705_VS_KEYWORD_PARAM_MAX];

	u32 cmd;
	u32 resp;
	int ret;
	unsigned size;
	unsigned rdcnt;

	mutex_lock(&es705->api_mutex);

	/* Read voice sense keyword data block request. */
	cmd = cpu_to_le32(0x802e0006);
	es705->dev_write(es705, (char *)&cmd, 4);

	usleep_range(5000, 5000);

	ret = es705->dev_read(es705, (char *)&resp, 4);
	if (ret < 0) {
		dev_dbg(es705->dev, "%s(): error sending request = %d\n",
			__func__, ret);
		goto OUT;
	}

	le32_to_cpus(resp);
	size = resp & 0xffff;
	dev_dbg(es705->dev, "%s(): resp=0x%08x size=%d\n",
		__func__, resp, size);
	if ((resp & 0xffff0000) != 0x802e0000) {
		dev_err(es705->dev, "%s(): invalid read v-s data block response = 0x%08x\n",
			__func__, resp);
		goto OUT;
	}

	BUG_ON(size == 0);
	BUG_ON(size > ES705_VS_KEYWORD_PARAM_MAX);
	BUG_ON(size % 4 != 0);

	/* This assumes we need to transfer the block in 4 byte
	 * increments. This is true on slimbus, but may not hold true
	 * for other buses.
	 */
	for (rdcnt = 0; rdcnt < size; rdcnt += 4) {
		ret = es705->dev_read(es705, (char *)&resp, 4);
		if (ret < 0) {
			dev_dbg(es705->dev, "%s(): error reading data block at %d bytes\n",
				__func__, rdcnt);
			goto OUT;
		}
		memcpy(&block[rdcnt], &resp, 4);
	}

	memcpy(es705->vs_keyword_param, block, rdcnt);
	es705->vs_keyword_param_size = rdcnt;
	dev_dbg(es705->dev, "%s(): stored v-s keyword block of %d bytes\n",
		__func__, rdcnt);

OUT:
	mutex_unlock(&es705->api_mutex);
	if (ret)
		dev_err(es705->dev, "%s(): v-s read data block failure=%d\n",
			__func__, ret);
	return ret;
}

int es705_write_vs_data_block(struct es705_priv *es705)
{
	u32 cmd;
	u32 resp;
	int ret;
	u8 *dptr;
	u16 rem;
	u8 wdb[4];

	if (es705->vs_keyword_param_size == 0) {
		dev_warn(es705->dev, "%s(): attempt to write empty keyword data block\n",
			__func__);
		return -ENOENT;
	}

	BUG_ON(es705->vs_keyword_param_size % 4 != 0);

	mutex_lock(&es705->api_mutex);

	cmd = 0x802f0000 | (es705->vs_keyword_param_size & 0xffff);
	cmd = cpu_to_le32(cmd);
	ret = es705->dev_write(es705, (char *)&cmd, 4);
	if (ret < 0) {
		dev_err(es705->dev, "%s(): error writing cmd 0x%08x to device\n",
		    __func__, cmd);
		goto EXIT;
	}

	usleep_range(10000, 10000);
	ret = es705->dev_read(es705, (char *)&resp, 4);
	if (ret < 0) {
		dev_dbg(es705->dev, "%s(): error sending request = %d\n",
			__func__, ret);
		goto EXIT;
	}

	le32_to_cpus(resp);
	dev_dbg(es705->dev, "%s(): resp=0x%08x\n", __func__, resp);
	if ((resp & 0xffff0000) != 0x802f0000) {
		dev_err(es705->dev, "%s(): invalid write data block 0x%08x\n",
			__func__, resp);
		goto EXIT;
	}

	dptr = es705->vs_keyword_param;
	for (rem = es705->vs_keyword_param_size; rem > 0; rem -= 4, dptr += 4) {
		wdb[0] = dptr[3];
		wdb[1] = dptr[2];
		wdb[2] = dptr[1];
		wdb[3] = dptr[0];
		ret = es705->dev_write(es705, (char *)wdb, 4);
		if (ret < 0) {
			dev_err(es705->dev, "%s(): v-s wdb error offset=%hu\n",
			    __func__, dptr - es705->vs_keyword_param);
			goto EXIT;
		}
	}

	usleep_range(10000, 10000);
	memset(&resp, 0, 4);

	ret = es705->dev_read(es705, (char *)&resp, 4);
	if (ret < 0) {
		dev_dbg(es705->dev, "%s(): error sending request = %d\n",
			__func__, ret);
		goto EXIT;
	}

	le32_to_cpus(resp);
	dev_dbg(es705->dev, "%s(): resp=0x%08x\n", __func__, resp);
	if (resp & 0xffff) {
		dev_err(es705->dev, "%s(): write data block error 0x%08x\n",
			__func__, resp);
		goto EXIT;
	}

	dev_info(es705->dev, "%s(): v-s wdb success\n", __func__);
EXIT:
	mutex_unlock(&es705->api_mutex);
	if (ret != 0)
		dev_err(es705->dev, "%s(): v-s wdb failed ret=%d\n",
			__func__, ret);
	return ret;
}


#ifdef FIXED_CONFIG
static void es705_fixed_config(struct es705_priv *es705)
{
	int rc;

	rc = es705_write_block(es705, es705_route_config[ROUTE_OFF].route);
}
#endif

static void es705_switch_route(long route_index)
{
	struct es705_priv *es705 = &es705_priv;
	int rc;

	if (route_index >= ROUTE_MAX) {
		dev_dbg(es705->dev, "%s(): new es705_internal_route = %ld is out of range\n",
			 __func__, route_index);
		return;
	}

	dev_dbg(es705->dev, "%s(): switch current es705_internal_route = %ld to new route = %ld\n",
		__func__, es705->internal_route_num, route_index);
	es705->internal_route_num = route_index;
	rc = es705_write_block(es705,
			  es705_route_config[es705->internal_route_num].route);
}

int es705_cmd_without_sleep(struct es705_priv *es705, u32 cmd)
{
	int sr;
	int err;
	u32 resp;

	BUG_ON(!es705);

	sr = cmd & BIT(28);

	err = es705->cmd(es705, cmd, sr, &resp);
	if (err || sr)
		return err;

	if (resp == 0) {
		err = -ETIMEDOUT;
		dev_err(es705->dev, "%s(): no response to command 0x%08x\n",
			__func__, cmd);
	} else {
		es705->last_response = resp;
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
int es705_cmd(struct es705_priv *es705, u32 cmd)
{
	int rc;

	BUG_ON(!es705);
	es705_wakeup_request(es705);
	rc = es705_cmd_without_sleep(es705, cmd);
	es705_sleep_queue(es705);
	return rc;
}

/*
static void es705_ping(struct es705_priv *es705)
{
	unsigned int value = 0;

	value = es705_read(NULL, ES705_CHANGE_STATUS);

	pr_debug("%s(): ping ack = %04x\n", __func__, value);
}
*/

static unsigned int es705_read_without_sleep(struct snd_soc_codec *codec,
					   unsigned int reg)
{
	struct es705_priv *es705 = &es705_priv;
	struct es705_api_access *api_access;
	u32 api_word[2] = {0};
	char req_msg[8];
	u32 ack_msg;
	char *msg_ptr;
	unsigned int msg_len;
	unsigned int value;
	int match = 0;
	int rc;

	if (reg > ES705_API_ADDR_MAX) {
		dev_err(es705->dev, "%s(): invalid address = 0x%04x\n",
			__func__, reg);
		return -EINVAL;
	}
	api_access = &es705_api_access[reg];
	msg_len = api_access->read_msg_len;
	memcpy((char *)api_word, (char *)api_access->read_msg, msg_len);
	switch (msg_len) {
	case 8:
		cpu_to_le32s(&api_word[1]);
	case 4:
		cpu_to_le32s(&api_word[0]);
	}
	memcpy(req_msg, (char *)api_word, msg_len);

	msg_ptr = req_msg;
	mutex_lock(&es705->api_mutex);
	rc = es705->dev_write_then_read(es705, msg_ptr, msg_len,
					&ack_msg, match);
	mutex_unlock(&es705->api_mutex);
	if (rc < 0) {
		dev_err(es705->dev, "%s(): es705_xxxx_write()", __func__);
		return rc;
	}
	memcpy((char *)&api_word[0], &ack_msg, 4);
	le32_to_cpus(&api_word[0]);
	value = api_word[0] & 0xffff;

	return value;
}

static unsigned int es705_read(struct snd_soc_codec *codec,
			       unsigned int reg)
{
	unsigned int value;

	if (reg > ES705_API_ADDR_MAX) {
		dev_err(es705_priv.dev, "%s(): invalid address = 0x%04x\n",
			__func__, reg);
		return -EINVAL;
	}
	es705_wakeup_request(&es705_priv);
	value = es705_read_without_sleep(codec, reg);
	es705_sleep_queue(&es705_priv);
	return value;
}

static int es705_write_without_sleep(struct snd_soc_codec *codec,
			unsigned int reg, unsigned int value)
{
	struct es705_priv *es705 = &es705_priv;
	struct es705_api_access *api_access;
	u32 api_word[2] = {0};
	char msg[8];
	char *msg_ptr;
	int msg_len;
	unsigned int val_mask;
	int rc = 0;

	if (reg > ES705_API_ADDR_MAX) {
		dev_err(es705->dev, "%s(): invalid address = 0x%04x\n",
			__func__, reg);
		return -EINVAL;
	}

	api_access = &es705_api_access[reg];
	msg_len = api_access->write_msg_len;
	val_mask = (1 << get_bitmask_order(api_access->val_max)) - 1;
	dev_info(es705->dev,
		"%s(): reg=%d val=%d msg_len = %d val_mask = 0x%08x\n",
		__func__, reg, value, msg_len, val_mask);
	memcpy((char *)api_word, (char *)api_access->write_msg, msg_len);
	switch (msg_len) {
	case 8:
		api_word[1] |= (val_mask & value);
		cpu_to_le32s(&api_word[1]);
		cpu_to_le32s(&api_word[0]);
		break;
	case 4:
		api_word[0] |= (val_mask & value);
		cpu_to_le32s(&api_word[0]);
		break;
	}
	memcpy(msg, (char *)api_word, msg_len);

	msg_ptr = msg;
	mutex_lock(&es705->api_mutex);
	rc = es705->dev_write(es705, msg_ptr, msg_len);
	mutex_unlock(&es705->api_mutex);
	return rc;
}

static int es705_write(struct snd_soc_codec *codec, unsigned int reg,
		       unsigned int value)
{
	if (reg > ES705_API_ADDR_MAX) {
		dev_err(es705_priv.dev, "%s(): invalid address = 0x%04x\n",
			__func__, reg);
		return -EINVAL;
	}
	es705_wakeup_request(&es705_priv);
	value = es705_write_without_sleep(codec, reg, value);
	es705_sleep_queue(&es705_priv);
	return value;
}

static int es705_write_then_read(struct es705_priv *es705,
				const void *buf, int len,
				u32 *rspn, int match)
{
	int rc;
	rc = es705->dev_write_then_read(es705, buf, len, rspn, match);
	return rc;
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
/* /sys/devices/platform/msm_slim_ctrl.1/es705-codec-gen0/route_status */

static ssize_t es705_route_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct es705_priv *es705 = &es705_priv;

	dev_dbg(es705->dev, "%s(): route=%ld\n",
		__func__, es705->internal_route_num);
	return snprintf(buf, PAGE_SIZE, "route=%ld\n",
			es705->internal_route_num);
}

static DEVICE_ATTR(route, 0444, es705_route_show, NULL);
/* /sys/devices/platform/msm_slim_ctrl.1/es705-codec-gen0/route */

static ssize_t es705_rate_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct es705_priv *es705 = &es705_priv;

	dev_dbg(es705->dev, "%s(): rate=%ld\n", __func__, es705->internal_rate);
	return snprintf(buf, PAGE_SIZE, "rate=%ld\n",
			es705->internal_rate);
}

static DEVICE_ATTR(rate, 0444, es705_rate_show, NULL);
/* /sys/devices/platform/msm_slim_ctrl.1/es705-codec-gen0/rate */

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
/* /sys/devices/platform/msm_slim_ctrl.1/es705-codec-gen0/fw_version */

static ssize_t es705_clock_on_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	int ret = 0;
	return ret;
}

static ssize_t es705_clock_on_set(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int rc;
	int on;
	const char *state[] = { "OFF", "ON" };
	rc = kstrtoint(buf, 0, &on);
	dev_dbg(es705_priv.dev, "%s(): ext clk %s\n",
		__func__, state[on]);
	if (es705_priv.pdata && es705_priv.pdata->esxxx_clk_cb)
		es705_priv.pdata->esxxx_clk_cb(on);
	else
		dev_err(es705_priv.dev, "%s(): fail. pdata 0x%p esxxx_clk_on 0x%p\n",
			__func__, es705_priv.pdata,
					es705_priv.pdata->esxxx_clk_cb);
	return count;
}
static DEVICE_ATTR(clock_on, 0644, es705_clock_on_show, es705_clock_on_set);
/* /sys/devices/platform/msm_slim_ctrl.1/es705-codec-gen0/clock_on */

static ssize_t es705_vs_keyword_parameters_show(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	int ret = 0;

	if (es705_priv.vs_keyword_param_size > 0) {
		memcpy(buf, es705_priv.vs_keyword_param,
		       es705_priv.vs_keyword_param_size);
		ret = es705_priv.vs_keyword_param_size;
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

	if (count <= ES705_VS_KEYWORD_PARAM_MAX) {
		memcpy(es705_priv.vs_keyword_param, buf, count);
		es705_priv.vs_keyword_param_size = count;
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
/* /sys/devices/platform/msm_slim_ctrl.1/es705-codec-gen0/vs_keyword_parameters */

static ssize_t es705_vs_status_show(struct device *dev,
			            struct device_attribute *attr,
				    char *buf)
{
	int ret = 0;
	unsigned int value = 0;
	char *status_name = "Voice Sense Status";
	/* Disable vs status read for interrupt to work */
	struct es705_priv *es705 = &es705_priv;

	mutex_lock(&es705->api_mutex);
	value = es705->vs_get_event;
	/* Reset the detection status after read */
	es705->vs_get_event = NO_EVENT;
	mutex_unlock(&es705->api_mutex);

	ret = snprintf(buf, PAGE_SIZE,
		       "%s=0x%04x\n",
		       status_name, value);

	return ret;
}

static DEVICE_ATTR(vs_status, 0444, es705_vs_status_show, NULL);
/* /sys/devices/platform/msm_slim_ctrl.1/es705-codec-gen0/vs_status */

static ssize_t es705_ping_status_show(struct device *dev,
			            struct device_attribute *attr,
				    char *buf)
{
	struct es705_priv *es705 = &es705_priv;
	int rc = 0;
	u32 sync_cmd = (ES705_SYNC_CMD << 16) | ES705_SYNC_POLLING;
	u32 sync_ack;
	char msg[4];
	char *status_name = "Ping";

	cpu_to_le32s(&sync_cmd);
	memcpy(msg, (char *)&sync_cmd, 4);
	rc = es705->dev_write(es705, msg, 4);
	if (rc < 0) {
		dev_err(es705->dev, "%s(): firmware load failed sync write\n",
			__func__);
	}
	msleep(100);
	memset(msg, 0, 4);
	rc = es705->dev_read(es705, msg, 4);
	if (rc < 0) {
		dev_err(es705->dev, "%s(): error reading sync ack rc=%d\n",
		       __func__, rc);
	}
	memcpy((char *)&sync_ack, msg, 4);
	le32_to_cpus(&sync_ack);
	dev_dbg(es705->dev, "%s(): sync_ack = 0x%08x\n", __func__, sync_ack);

	rc = snprintf(buf, PAGE_SIZE,
		       "%s=0x%08x\n",
		       status_name, sync_ack);

	return rc;
}

static DEVICE_ATTR(ping_status, 0444, es705_ping_status_show, NULL);
/* /sys/devices/platform/msm_slim_ctrl.1/es705-codec-gen0/ping_status */

#if defined(CONFIG_MQ100_SENSOR_HUB)
int es705_rdb_register(const struct es705_rwdb_device_callbacks *_callbacks)
{
	callbacks = *_callbacks;
	return 0;
}

void es705_indicate_state_change(u8 val)
{
	if (callbacks.status != 0)
		callbacks.status(callbacks.priv, val);

	dev_info(es705_priv.dev, "%s: State %d\n", __func__, val);
}
EXPORT_SYMBOL_GPL(es705_indicate_state_change);
#endif

static ssize_t es705_gpio_reset_set(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct es705_priv *es705 = &es705_priv;
	dev_dbg(es705->dev, "%s(): GPIO reset\n", __func__);
	es705->mode = SBL;
	es705_gpio_reset(es705);
	dev_dbg(es705->dev, "%s(): Ready for STANDARD download by proxy\n",
		__func__);
	return count;
}

static DEVICE_ATTR(gpio_reset, 0644, NULL, es705_gpio_reset_set);
/* /sys/devices/platform/msm_slim_ctrl.1/es705-codec-gen0/gpio_reset */


static ssize_t es705_overlay_mode_set(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct es705_priv *es705 = &es705_priv;
	int rc;
	int value = ES705_SET_POWER_STATE_VS_OVERLAY;

	dev_dbg(es705->dev, "%s(): Set Overlay mode\n", __func__);

	es705->mode = SBL;
	rc = es705_write(NULL, ES705_POWER_STATE , value);
	if (rc) {
		dev_err(es705_priv.dev, "%s(): Set Overlay mode failed\n",
			__func__);
	} else {
		msleep(50);
		es705_priv.es705_power_state = ES705_SET_POWER_STATE_VS_OVERLAY;
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
/* /sys/devices/platform/msm_slim_ctrl.1/es705-codec-gen0/overlay_mode */

static ssize_t es705_vs_event_set(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct es705_priv *es705 = &es705_priv;
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
/* /sys/devices/platform/msm_slim_ctrl.1/es705-codec-gen0/vs_event */

static ssize_t es705_auto_sleep_show(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	int ret = 0;
	ret = snprintf(buf, PAGE_SIZE, "%d\n", es705_priv.sleep_enable);
	return ret;
}

static ssize_t es705_auto_sleep_set(struct device *dev,
					       struct device_attribute *attr,
					       const char *buf, size_t count)
{
	int rc = 0;
	rc = kstrtos32(buf, 0, &es705_priv.sleep_enable);

	dev_info(es705_priv.dev, "%s(): auto sleep = %d\n",
				__func__, es705_priv.sleep_enable);

	return count;
}

static DEVICE_ATTR(auto_sleep, 0644,
		   es705_auto_sleep_show,
		   es705_auto_sleep_set);
/* /sys/devices/platform/msm_slim_ctrl.1/es705-codec-gen0/auto_sleep */


static ssize_t es705_sleep_delay_show(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	int ret = 0;
	ret = snprintf(buf, PAGE_SIZE, "%d\n", es705_priv.sleep_delay);
	return ret;
}

static ssize_t es705_sleep_delay_set(struct device *dev,
					       struct device_attribute *attr,
					       const char *buf, size_t count)
{
	int rc = 0;
	rc = kstrtos32(buf, 0, &es705_priv.sleep_delay);

	dev_info(es705_priv.dev, "%s(): sleep delay = %d\n",
					__func__, es705_priv.sleep_delay);

	return count;
}

static DEVICE_ATTR(sleep_delay_msec, 0644,
		   es705_sleep_delay_show,
		   es705_sleep_delay_set);
/* /sys/devices/platform/msm_slim_ctrl.1/es705-codec-gen0/sleep_delay */



static struct attribute *core_sysfs_attrs[] = {
	&dev_attr_route_status.attr,
	&dev_attr_route.attr,
	&dev_attr_rate.attr,
	&dev_attr_fw_version.attr,
	&dev_attr_clock_on.attr,
	&dev_attr_vs_keyword_parameters.attr,
	&dev_attr_vs_status.attr,
	&dev_attr_ping_status.attr,
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

#ifndef CONFIG_SND_SOC_ES705_UART_FW_DOWNLOAD
static int es705_fw_download(struct es705_priv *es705, int fw_type)
{
	int rc = 0;

	dev_info(es705->dev, "%s(): firmware download type %d begin\n",
							__func__, fw_type);
	mutex_lock(&es705->api_mutex);
	if (fw_type != VOICESENSE && fw_type != STANDARD) {
		dev_err(es705->dev, "%s(): Unknown firmware type\n", __func__);
		goto es705_fw_download_failed;
	}

	if (!es705->boot_setup || !es705->boot_finish) {
		dev_err(es705->dev, "%s(): boot setup or finish func undef\n",
								__func__);
		goto es705_fw_download_failed;
	}

	dev_dbg(es705->dev, "%s(): GPIO reset\n", __func__);

	rc = es705->boot_setup(es705);
	if (rc) {
		dev_err(es705->dev, "%s(): firmware download start error %d\n",
								__func__, rc);
		goto es705_fw_download_failed;
	}

	if (fw_type == VOICESENSE)
		rc = es705->dev_write(es705, (char *)es705->vs->data,
				es705->vs->size);
	else
		rc = es705->dev_write(es705, (char *)es705->standard->data,
				es705->standard->size);
	if (rc) {
		dev_err(es705->dev, "%s(): firmware write error %d\n",
								__func__, rc);
		rc = -EIO;
		goto es705_fw_download_failed;
	}

	es705->mode = fw_type;
	rc = es705->boot_finish(es705);
	if (rc) {
		dev_err(es705->dev, "%s() firmware download finish error %d\n",
								__func__, rc);
			goto es705_fw_download_failed;
	}
	dev_info(es705->dev, "%s(): firmware download type %d done\n",
							__func__, fw_type);

es705_fw_download_failed:
	mutex_unlock(&es705->api_mutex);
	return rc;
}
#endif

static int es705_vs_fw_download(struct es705_priv *es705)
{
	int rc = 0;
	int write_size = ES705_VS_FW_CHUNK;
	int data_remaining = es705->vs->size;
	int fw_type = VOICESENSE;
	mutex_lock(&es705->pm_mutex);
	dev_info(es705->dev, "%s(): voicesense firmware download begin\n",
								__func__);

	if (es705->uart_fw_download &&
			es705->uart_fw_download(es705, fw_type) >= 0) {
		es705->mode = fw_type;
		goto es705_uart_fw_download_exit;
	}

	if (!es705->boot_setup || !es705->boot_finish) {
		dev_err(es705->dev, "%s(): boot setup or finish func undef\n",
								__func__);
		rc = -EIO;
		goto es705_vs_fw_download_failed;
	}

	rc = es705->boot_setup(es705);
	if (rc) {
		dev_err(es705->dev, "%s(): firmware download start error %d\n",
								__func__, rc);
		goto es705_vs_fw_download_failed;
	}

	while (data_remaining) {
		rc = es705->dev_write(es705, ((char *)es705->vs->data)
			+ (es705->vs->size - data_remaining), write_size);

		data_remaining -= write_size;

		dev_dbg(es705->dev,
			"data_remaining = %d, write_size = %d\n",
			data_remaining, write_size);

		if (rc) {
			dev_err(es705->dev,
				"%s(): firmware write error %d\n",
							__func__, rc);
			rc = -EIO;
			goto es705_vs_fw_download_failed;
		} else if (data_remaining < write_size) {
			write_size = data_remaining;
		}

		usleep_range(2000, 2000);
		if (is_sleep_aborted()) {
			es705_cmd_without_sleep(es705,
				es705_priv.vs_abort_kw);
			rc = -EIO;
			goto es705_vs_fw_download_failed;
		}
	}

	es705->mode = VOICESENSE;
	rc = es705->boot_finish(es705);
	if (rc) {
		dev_err(es705->dev, "%s() firmware download finish error %d\n",
								__func__, rc);
			goto es705_vs_fw_download_failed;
	}
	dev_info(es705->dev, "%s(): voicesense firmware download done\n",
								__func__);

es705_vs_fw_download_failed:
es705_uart_fw_download_exit:
	mutex_unlock(&es705->pm_mutex);
	return rc;
}

static int es705_vs_thread(void *priv)
{
	struct es705_priv *es705 = (struct es705_priv  *)priv;
	int rc;
	BUG_ON(es705->vs->size == 0);

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

	rc = es705_vs_fw_download(es705);
	if (rc) {
		dev_err(es705->dev, "%s(): voicesense vs download error %d\n",
								__func__, rc);
	} else {
		/* Enable Voice Sense Event INTR to Host */
		rc = es705_write_without_sleep(NULL, ES705_EVENT_RESPONSE,
				ES705_SYNC_INTR_RISING_EDGE);
		if (rc)
			dev_err(es705->dev, "%s(): Enable Event Intr fail %d\n",
								__func__, rc);
	}
	return 0;
}

int es705_bootup(struct es705_priv *es705)
{
	int rc;
	BUG_ON(es705->standard->size == 0);
	mutex_lock(&es705->pm_mutex);
	es705->pm_state = ES705_POWER_BOOT;
	mutex_unlock(&es705->pm_mutex);

#ifdef CONFIG_SND_SOC_ES705_UART_FW_DOWNLOAD
	rc = es705_uart_fw_download(es705, STANDARD);
#else
	rc = es705_fw_download(es705, STANDARD);
#endif
	if (rc) {
		dev_err(es705->dev, "%s(): STANDARD fw download error %d\n",
								__func__, rc);
	} else {
		mutex_lock(&es705->pm_mutex);
		es705->pm_state = ES705_POWER_AWAKE;
		mutex_unlock(&es705->pm_mutex);
	}
	return rc;
}

static int register_snd_soc(struct es705_priv *priv);

int fw_download(void *arg)
{
	struct es705_priv *priv = (struct es705_priv *)arg;
	int rc;

	dev_dbg(priv->dev, "%s(): GPIO reset\n", __func__);
	rc = es705_bootup(priv);
	dev_dbg(priv->dev, "%s(): bootup rc=%d\n", __func__, rc);

#ifdef CONFIG_SND_SOC_ES_SPI_SENSOR_HUB
	rc = es705_sensor_hub_enable_event_interrupt(priv);
	if (rc < 0) {
		dev_err(priv->dev,
		    "[SPI]: %s(): Failed to enable Event Interrupt GPIO\n",
		    __func__);
		return rc;
	}
#endif

	rc = register_snd_soc(priv);
	dev_dbg(priv->dev, "%s(): register_snd_soc rc=%d\n", __func__, rc);

#ifdef FIXED_CONFIG
	es705_fixed_config(priv);
#endif

	dev_dbg(priv->dev, "%s(): release module\n", __func__);
	module_put(THIS_MODULE);
	return 0;
}

/* Hold the pm_mutex before calling this function */
static int es705_sleep(struct es705_priv *es705)
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

	es705_priv.es705_power_state = ES705_SET_POWER_STATE_SLEEP;
	es705_priv.pm_state = ES705_POWER_SLEEP;

	/* clocks off */
	if (es705->pdata->esxxx_clk_cb)
		es705->pdata->esxxx_clk_cb(0);

	dev_dbg(es705_priv.dev, "Sleep done, wake_count=%d\n",
			es705_priv.wake_count);

	usleep_range(5000, 5000);
	return 0;
}

static void es705_delayed_sleep(struct work_struct *w)
{
	int ch_tot;
	int ports_active = (es705_priv.rx1_route_enable ||
		es705_priv.rx2_route_enable || es705_priv.tx1_route_enable);

	if (es705_priv.es705_power_state == ES705_SET_POWER_STATE_VS_OVERLAY
		|| es705_priv.es705_power_state == \
			ES705_SET_POWER_STATE_VS_LOWPWR) {
		dev_dbg(es705_priv.dev, "%s() Sleep not allowed in VS mode\n",
				__func__);
		es705_priv.pm_state = ES705_POWER_AWAKE;
		return;
	}

	/* If there are active streams we do not sleep.
	* Count the front end (FE) streams ONLY.
	*/

	ch_tot = 0;
	ch_tot += es705_priv.dai[ES705_SLIM_1_PB].ch_tot;
	ch_tot += es705_priv.dai[ES705_SLIM_2_PB].ch_tot;

	ch_tot += es705_priv.dai[ES705_SLIM_1_CAP].ch_tot;
	dev_dbg(es705_priv.dev, "%s %d active channels, ports_active: %d\n",
		__func__, ch_tot, ports_active);

	dev_dbg(es705_priv.dev, "%s Delayed Sleep entry\n",
				__func__);

	mutex_lock(&es705_priv.pm_mutex);

	if ((ports_active == 0) &&
		(es705_priv.pm_state == ES705_POWER_SLEEP_PENDING)) {
		dev_dbg(es705_priv.dev, "%s Delayed Sleep started\n",
					__func__);
		es705_sleep(&es705_priv);
	} else {
		dev_err(es705_priv.dev,
		"%s error ch_tot=%d, ports_active=%d, pm_state=%d\n",
		 __func__, ch_tot, ports_active, es705_priv.pm_state);
	}
	mutex_unlock(&es705_priv.pm_mutex);
}

static void es705_sleep_request(struct es705_priv *es705)
{
	mutex_lock(&es705->pm_mutex);
	dev_dbg(es705->dev, "%s internal es705_power_state = %d\n", __func__,
		es705_priv.pm_state);

	if ((es705_priv.pm_state != ES705_POWER_AWAKE) ||  \
			es705_priv.wake_count > 0)
		goto sleep_request_exit;

	if (es705_priv.es705_power_state == ES705_SET_POWER_STATE_VS_OVERLAY
		|| es705_priv.es705_power_state == \
			ES705_SET_POWER_STATE_VS_LOWPWR) {
		dev_dbg(es705_priv.dev, "%s() Sleep not allowed in VS mode\n",
				__func__);
		goto sleep_request_exit;
	}

	schedule_delayed_work(&es705->sleep_work,
			msecs_to_jiffies(es705->sleep_delay));

	dev_dbg(es705->dev, "%s ES705_POWER_SLEEP_PENDING\n",
			__func__);

	es705_priv.pm_state = ES705_POWER_SLEEP_PENDING;
sleep_request_exit:
	mutex_unlock(&es705->pm_mutex);
}

static void es705_sleep_queue(struct es705_priv *es705)
{
	if (es705->sleep_enable) {
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

#define SYNC_DELAY 30
static int es705_wakeup(struct es705_priv *es705)
{
	int rc = 0;
	u32 sync_cmd = (ES705_SYNC_CMD << 16) | ES705_SYNC_POLLING;
	u32 sync_rspn = sync_cmd;
	int match = 1;

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

		mutex_lock(&es705_priv.abort_mutex);
		es705->sleep_abort = true;
		dev_dbg(es705->dev, "%s(): abort set\n", __func__);
		mutex_unlock(&es705_priv.abort_mutex);

		dev_err(es705->dev,
		"%s(): before cancel_delayed_work_sync\n", __func__);
		cancel_delayed_work_sync(&es705->sleep_work);
		dev_err(es705->dev,
		"%s(): after cancel_delayed_work_sync\n", __func__);

		mutex_lock(&es705_priv.abort_mutex);
		es705->sleep_abort = false;
		mutex_unlock(&es705_priv.abort_mutex);

		dev_dbg(es705->dev, "%s(): cancel delayed work\n", __func__);

		mutex_lock(&es705->pm_mutex);
		es705->pm_state = ES705_POWER_AWAKE;
		dev_dbg(es705->dev, "%s() Exit from wakeup\n", __func__);
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

#ifdef CONFIG_SND_SOC_ES705_UART_WAKEUP
	es705_uart_es705_wakeup(es705);
#else
	dev_dbg(es705->dev, "%s(): generate gpio wakeup falling edge\n",
		__func__);

	gpio_set_value(es705->pdata->wakeup_gpio, 1);
	usleep_range(1000, 1000);
	gpio_set_value(es705->pdata->wakeup_gpio, 0);
#endif

	dev_dbg(es705->dev, "%s(): wait 30ms wakeup, then ping to es705\n",
		__func__);
	msleep(SYNC_DELAY);

	/* Take it out of OverlayMode in case if Overlay mode
		only when auto sleep is enabled.*/
	if ((es705->sleep_enable) &&
	(es705->es705_power_state == ES705_SET_POWER_STATE_VS_LOWPWR)) {
		rc = es705_write_then_read(es705, &sync_cmd, sizeof(sync_cmd),
							 &sync_rspn, match);
		if (rc) {
			dev_err(es705->dev,
				"%s(): sync in overlay FAIL\n", __func__);
			goto es705_wakeup_exit;
		}

		rc = es705_cmd_without_sleep(es705,
					(ES705_SET_POWER_STATE << 16) |
					ES705_SET_POWER_STATE_VS_OVERLAY);

		if (rc) {
			dev_err(es705->dev,
				"%s(): power state normal FAIL\n", __func__);
			goto es705_wakeup_exit;
		}

		dev_dbg(es705->dev, "%s(): wait 30ms wakeup, then ping to es705\n",
			__func__);
		msleep(30);
	}

	rc = es705_write_then_read(es705, &sync_cmd, sizeof(sync_cmd),
					 &sync_rspn, match);
	if (rc) {
		dev_err(es705->dev, "%s(): es705 wakeup FAIL\n", __func__);
		goto es705_wakeup_exit;
	}

	if (es705->es705_power_state == ES705_SET_POWER_STATE_VS_LOWPWR)
		es705->es705_power_state = ES705_SET_POWER_STATE_VS_OVERLAY;
	else
		es705->es705_power_state = ES705_SET_POWER_STATE_NORMAL;

	es705->pm_state = ES705_POWER_AWAKE;
	/* TODO use GPIO reset, if wakeup fail ? */

es705_wakeup_exit:
	mutex_unlock(&es705->pm_mutex);
	return rc;
}

static void es705_wakeup_request(struct es705_priv *es705)
{
	int rc = 0;
	if (es705->sleep_enable) {
		mutex_lock(&es705->wake_mutex);
		es705->wake_count += 1;

		dev_dbg(es705->dev, "%s wake_count=%d\n",
					__func__, es705->wake_count);

		rc = es705_wakeup(es705);
		if (rc)
			goto es705_wakeup_request_exit;

es705_wakeup_request_exit:
		mutex_unlock(&es705->wake_mutex);
	}

}

static int es705_put_control_value(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	/* struct snd_soc_codec *codec = es705_priv.codec; */
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
	/* struct snd_soc_codec *codec = es705_priv.codec; */
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
	if (es705_priv.pm_state == ES705_POWER_SLEEP)
		value = es705_priv.es705_power_state;
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
	struct task_struct *thread = NULL;

	value = ucontrol->value.enumerated.item[0];
	dev_dbg(es705_priv.dev, "%s(): Previous power state = %s, power set cmd = %s\n",
		__func__, power_state[es705_priv.pm_state],
		power_state_cmd[es705_priv.es705_power_state]);
	dev_dbg(es705_priv.dev, "%s(): Requested power set cmd = %s\n",
		__func__, power_state_cmd[value]);

	if (value == 0 || value == ES705_SET_POWER_STATE_MP_SLEEP ||
		value == ES705_SET_POWER_STATE_MP_CMD) {
		dev_err(es705_priv.dev, "%s(): Unsupported state in es705\n",
			__func__);
		rc = -EINVAL;
		goto es705_put_power_control_enum_exit;
	} else {
		if ((es705_priv.pm_state == ES705_POWER_SLEEP) &&
			(value != ES705_SET_POWER_STATE_NORMAL) &&
			(value != ES705_SET_POWER_STATE_VS_OVERLAY)) {
			dev_err(es705_priv.dev, "%s(): ES705 is in sleep mode."
				" Select the Normal Mode or Overlay"
				" if in Low Power mode.\n", __func__);
			rc = -EPERM;
			goto  es705_put_power_control_enum_exit;
		}

		if (value == ES705_SET_POWER_STATE_SLEEP) {
			/* Overlay mode doesn't support sleep */
			if (es705_priv.es705_power_state ==
				ES705_SET_POWER_STATE_VS_OVERLAY) {
				dev_err(es705_priv.dev, "%s(): %s",
				"Sleep not supported from VS overlay mode\n",
				__func__);
				rc = -EPERM;
				goto es705_put_power_control_enum_exit;
			}
			dev_dbg(es705_priv.dev, "%s(): Activate Sleep Request\n",
						__func__);
			es705_sleep_request(&es705_priv);
		} else if (value == ES705_SET_POWER_STATE_NORMAL) {
			/* Overlay mode doesn't need wakeup */
			if (es705_priv.es705_power_state !=
				ES705_SET_POWER_STATE_VS_OVERLAY) {
				rc = es705_wakeup(&es705_priv);
				if (rc)
					goto es705_put_power_control_enum_exit;
			}
			rc = es705_write(NULL, ES705_POWER_STATE, value);
			if (rc) {
				dev_err(es705_priv.dev, "%s(): Power state command write failed\n",
					__func__);
				goto es705_put_power_control_enum_exit;
			}
			/* Wait for 100ms to switch from Overlay mode */
			msleep(100);
			es705_priv.es705_power_state =
				ES705_SET_POWER_STATE_NORMAL;
			es705_priv.mode = STANDARD;

		} else if (value == ES705_SET_POWER_STATE_VS_LOWPWR) {
			if (es705_priv.es705_power_state ==
				ES705_SET_POWER_STATE_VS_OVERLAY) {
				int retry =
					MAX_RETRY_TO_SWITCH_TO_LOW_POWER_MODE;
				rc = es705_write(NULL,
					ES705_VS_INT_OSC_MEASURE_START, 0);
				if (rc) {
					dev_err(es705_priv.dev, "%s(): OSC Measure Start fail\n",
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
					dev_dbg(es705_priv.dev, "%s(): OSC Measure Status = 0x%04x\n",
						__func__, rc);
				} while (rc && --retry);

				if (rc < 0) {
					dev_err(es705_priv.dev,
						"%s(): OSC Measure Read Status fail\n",
						__func__);
					goto es705_put_power_control_enum_exit;
				} else if (rc > 0) {
					dev_err(es705_priv.dev, "%s(): Unexpected OSC Measure Status = 0x%04x\n",
						__func__, rc);
					dev_err(es705_priv.dev, "%s(): Can't switch to Low Power Mode\n",
						__func__);
					goto es705_put_power_control_enum_exit;
				}
				dev_dbg(es705_priv.dev, "%s(): Activate Low Power Mode\n",
					__func__);
				es705_write(NULL, ES705_POWER_STATE, value);
				es705_priv.es705_power_state =
					ES705_SET_POWER_STATE_VS_LOWPWR;
				es705_priv.pm_state = ES705_POWER_SLEEP;
				goto es705_put_power_control_enum_exit;
			} else {
				dev_err(es705_priv.dev, "%s(): ES705 should be in VS Overlay"
					"mode. Select the VS Overlay Mode.\n",
					__func__);
				rc = -EPERM;
				goto es705_put_power_control_enum_exit;
			}
		} else if (value == ES705_SET_POWER_STATE_VS_OVERLAY) {
			if (es705_priv.es705_power_state ==
					ES705_SET_POWER_STATE_VS_LOWPWR) {
				rc = es705_wakeup(&es705_priv);
				if (rc)
					goto es705_put_power_control_enum_exit;
				es705_priv.es705_power_state =
					     ES705_SET_POWER_STATE_VS_OVERLAY;
			} else {
				rc = es705_write(NULL, reg, value);
				if (rc) {
					dev_err(es705_priv.dev, "%s(): Power state command write failed\n",
						__func__);
					goto es705_put_power_control_enum_exit;
				} else {
					es705_priv.es705_power_state =
					ES705_SET_POWER_STATE_VS_OVERLAY;

					/* wait es705 SBL mode */
					msleep(50);
					/* es705_vs_load(&es705_priv); */
					es705_priv.gen0_client->laddr = 0xf0;
					es705_priv.intf_client->laddr = 0xf0;
					thread = kthread_run(es705_vs_thread,
					&es705_priv, "audience vs thread");
					if (IS_ERR(thread)) {
						dev_err(es705_priv.dev, "%s(): can't create es705 vs firmware thread = %p\n",
						__func__, thread);
						rc = -1;
					}
				}
			}
		}
	}

es705_put_power_control_enum_exit:
	dev_dbg(es705_priv.dev, "%s(): Current power state = %s, power set cmd = %s\n",
		__func__, power_state[es705_priv.pm_state],
		power_state_cmd[es705_priv.es705_power_state]);
	return rc;
}

static int es705_get_rx1_route_enable_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = es705_priv.rx1_route_enable;
	dev_dbg(es705_priv.dev, "%s(): rx1_route_enable = %d\n",
		__func__, es705_priv.rx1_route_enable);

	return 0;
}

static int es705_put_rx1_route_enable_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	es705_priv.rx1_route_enable = ucontrol->value.integer.value[0];
	if (ucontrol->value.integer.value[0])
		es705_wakeup_request(&es705_priv);
	else
		es705_sleep_queue(&es705_priv);

	dev_dbg(es705_priv.dev, "%s(): rx1_route_enable = %d\n",
		__func__, es705_priv.rx1_route_enable);

	return 0;
}

static int es705_get_auto_power_preset_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = es705_priv.auto_power_preset;
	dev_dbg(es705_priv.dev, "%s(): auto_power_preset = %d\n",
		__func__, es705_priv.auto_power_preset);

	return 0;
}

static int es705_put_auto_power_preset_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	es705_priv.auto_power_preset = ucontrol->value.integer.value[0];

	dev_dbg(es705_priv.dev, "%s(): auto_power_preset = %d\n",
		__func__, es705_priv.auto_power_preset);

	return 0;
}

static int es705_get_power_level_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	u32 es_get_power_level = ES705_GET_POWER_LEVEL << 16;
	u32 rspn = 0;

	es_get_power_level = cpu_to_le32(es_get_power_level);

	es705_write_then_read(&es705_priv, &es_get_power_level,
				sizeof(es_get_power_level),
				&rspn, 0);
	ucontrol->value.enumerated.item[0] = rspn & 0x0000ffff;
	dev_dbg(es705_priv.dev, "%s: Response 0x%08X", __func__, rspn);
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
	ucontrol->value.integer.value[0] = es705_priv.tx1_route_enable;
	dev_dbg(es705_priv.dev, "%s(): tx1_route_enable = %d\n",
		__func__, es705_priv.tx1_route_enable);

	return 0;
}

static int es705_put_tx1_route_enable_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	es705_priv.tx1_route_enable = ucontrol->value.integer.value[0];
	if (ucontrol->value.integer.value[0])
		es705_wakeup_request(&es705_priv);
	else
		es705_sleep_queue(&es705_priv);

	dev_dbg(es705_priv.dev, "%s(): tx1_route_enable = %d\n",
		__func__, es705_priv.tx1_route_enable);

	return 0;
}

static int es705_get_rx2_route_enable_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = es705_priv.rx2_route_enable;
	dev_dbg(es705_priv.dev, "%s(): rx2_route_enable = %d\n",
		__func__, es705_priv.rx2_route_enable);

	return 0;
}

static int es705_put_rx2_route_enable_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	es705_priv.rx2_route_enable = ucontrol->value.integer.value[0];
	if (ucontrol->value.integer.value[0])
		es705_wakeup_request(&es705_priv);
	else
		es705_sleep_queue(&es705_priv);

	dev_dbg(es705_priv.dev, "%s(): rx2_route_enable = %d\n",
		__func__, es705_priv.rx2_route_enable);

	return 0;
}

static int es705_get_ns_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	dev_dbg(es705_priv.dev, "%s(): NS = %d\n",
		__func__, es705_priv.ns);
	ucontrol->value.enumerated.item[0] = es705_priv.ns;

	return 0;
}

static int es705_put_ns_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	int value = ucontrol->value.enumerated.item[0];
	int rc = 0;
	dev_dbg(es705_priv.dev, "%s(): NS = %d\n", __func__, value);

	es705_priv.ns = value;

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
	dev_dbg(es705_priv.dev, "%s(): SW = %d\n", __func__, value);

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
	dev_dbg(es705_priv.dev, "%s(): STS = %d\n", __func__, value);

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
	dev_dbg(es705_priv.dev, "%s(): RX_NS = %d\n", __func__, value);

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
	dev_dbg(es705_priv.dev, "%s(): WNF = %d\n", __func__, value);

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
	dev_dbg(es705_priv.dev, "%s(): BWE = %d\n", __func__, value);

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
	dev_dbg(es705_priv.dev, "%s(): Avalon Wind Noise = %d\n",
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
	dev_dbg(es705_priv.dev, "%s(): Virtual Bass Boost = %d\n",
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
	dev_dbg(es705_priv.dev, "%s(): Zoom = %d\n",
		__func__, es705_priv.zoom);
	ucontrol->value.enumerated.item[0] = es705_priv.zoom;

	return 0;
}

static int es705_put_aud_zoom(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	int value = ucontrol->value.enumerated.item[0];
	int rc = 0;
	dev_dbg(es705_priv.dev, "%s(): Zoom = %d\n", __func__, value);

	es705_priv.zoom = value;

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

int es705_remote_route_enable(struct snd_soc_dai *dai)
{
	dev_dbg(es705_priv.dev, "%s():dai->name = %s dai->id = %d\n",
		__func__, dai->name, dai->id);

	switch (dai->id) {
	case ES705_SLIM_1_PB:
		return es705_priv.rx1_route_enable;
	case ES705_SLIM_1_CAP:
		return es705_priv.tx1_route_enable;
	case ES705_SLIM_2_PB:
		return es705_priv.rx2_route_enable;
	default:
		return 0;
	}
}
EXPORT_SYMBOL_GPL(es705_remote_route_enable);

static int es705_put_internal_route(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	es705_switch_route(ucontrol->value.integer.value[0]);
	es705_priv.auto_power_preset = 1;
	return 0;
}

static int es705_get_internal_route(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct es705_priv *es705 = &es705_priv;

	ucontrol->value.integer.value[0] = es705->internal_route_num;

	return 0;
}

static int es705_put_internal_rate(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct es705_priv *es705 = &es705_priv;
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

	rc = es705_write_block(es705, rate_macro);
	if (es705->auto_power_preset) {
		usleep_range(20000, 20000);
		dev_dbg(es705->dev, "%s: Applying power preset 0x%08X",
				__func__, power_preset[0]);
		rc = es705_write_block(es705, power_preset);
	}
	es705->internal_rate = ucontrol->value.enumerated.item[0];

	return rc;
}

static int es705_get_internal_rate(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct es705_priv *es705 = &es705_priv;

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
		dev_err(es705_priv.dev, "%s(): Set Preset failed\n",
			__func__);
		return rc;
	}

	es705_priv.preset = value;

	return rc;
}

static int es705_get_preset_value(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = es705_priv.preset;

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
	int index = ucontrol->value.integer.value[0];

	if (index < ES705_CUSTOMER_PROFILE_MAX)
		es705_write_block(&es705_priv,
				  &es705_audio_custom_profiles[index][0]);
	return 0;
}

static int es705_ap_put_tx1_ch_cnt(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	es705_priv.ap_tx1_ch_cnt = ucontrol->value.enumerated.item[0] + 1;
	return 0;
}

static int es705_ap_get_tx1_ch_cnt(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct es705_priv *es705 = &es705_priv;

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
		value = es705_index_to_gain(-12, 1, ucontrol->value.integer.value[0]);
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
		value = es705_index_to_gain(-10, 1, ucontrol->value.integer.value[0]);
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
		value = es705_index_to_gain(-20, 1, ucontrol->value.integer.value[0]);
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

static int es705_get_rdb_size(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = es705_priv.rdb_read_count;
	return 0;
}

static int es705_get_vs_status(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	unsigned int value = 0;
	mutex_lock(&es705_priv.api_mutex);
	value = es705_priv.vs_get_event;
	/* Reset the detection status after read */
	es705_priv.vs_get_event = NO_EVENT;
	mutex_unlock(&es705_priv.api_mutex);
	ucontrol->value.enumerated.item[0] = value;
	return 0;
}
static int es705_get_vs_wakeup_keyword(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = es705_priv.vs_wakeup_keyword;
	return 0;
}

static int es705_put_vs_wakeup_keyword(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	es705_priv.vs_wakeup_keyword = ucontrol->value.enumerated.item[0];
	return 0;
}


static int es705_put_vs_stored_keyword(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	int op;
	int ret;

	op = ucontrol->value.enumerated.item[0];
	dev_dbg(es705_priv.dev, "%s(): op=%d\n", __func__, op);

	ret = 0;
	switch (op) {
	case 0:
		dev_dbg(es705_priv.dev, "%s(): keyword params put...\n",
			__func__);
		ret = es705_write_vs_data_block(&es705_priv);
		break;
	case 1:
		dev_dbg(es705_priv.dev, "%s(): keyword params get...\n",
			__func__);
		ret = es705_read_vs_data_block(&es705_priv);
		break;
	case 2:
		dev_dbg(es705_priv.dev, "%s(): keyword params clear...\n",
			__func__);
		es705_priv.vs_keyword_param_size = 0;
		break;
	default:
		BUG_ON(0);
	};

	return ret;
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
	dev_dbg(es705_priv.dev, "%s(): ucontrol = %ld value = %d\n",
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

	dev_dbg(es705_priv.dev, "%s(): value = %d ucontrol = %ld\n",
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

	dev_dbg(es705_priv.dev, "%s(): ucontrol = %ld value = %d\n",
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

	dev_dbg(es705_priv.dev, "%s(): value = %d ucontrol = %ld\n",
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
		dev_err(es705_priv.dev, "%s(): Set CVS Preset failed\n",
			__func__);
		return rc;
	}

	es705_priv.cvs_preset = value;

	return rc;
}

static
int es705_get_cvs_preset_value(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = es705_priv.cvs_preset;

	return 0;
}

static int es705_put_uart_app_rate(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct es705_priv *es705 = &es705_priv;

	dev_dbg(es705->dev, "%s(): UART rate  %d new_rate %d\n",
		__func__, (int)es705->uart_app_rate,
		(int)ucontrol->value.enumerated.item[0]);

	es705->uart_app_rate = ucontrol->value.enumerated.item[0];
	return 0;
}

static int es705_get_uart_app_rate(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct es705_priv *es705 = &es705_priv;

	ucontrol->value.enumerated.item[0] = es705->uart_app_rate;
	dev_dbg(es705->dev, "%s():es705->internal_rate = %d ucontrol = %d\n",
		__func__, (int)es705->internal_rate,
		(int)ucontrol->value.enumerated.item[0]);

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

static const char * const es705_uart_rate_text[UART_RATE_MAX] = {
	"460.8k", "921.6k", "1000k",
	"1M", "1.152M", "2000k",
	"2M", "3000k", "3M"
};
static const struct soc_enum es705_uart_app_rate =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0,
			ARRAY_SIZE(es705_uart_rate_text),
			es705_uart_rate_text);

static struct snd_kcontrol_new es705_digital_ext_snd_controls[] = {
	SOC_SINGLE_EXT("Auto Power Preset", SND_SOC_NOPM, 0, 1, 0,
				es705_get_auto_power_preset_value,
				es705_put_auto_power_preset_value),
	SOC_ENUM_EXT("Power Level", es705_power_level_enum,
				es705_get_power_level_value,
				es705_put_power_level_value),
	SOC_SINGLE_EXT("ES705 RX1 Enable", SND_SOC_NOPM, 0, 1, 0,
		       es705_get_rx1_route_enable_value,
		       es705_put_rx1_route_enable_value),
	SOC_SINGLE_EXT("ES705 TX1 Enable", SND_SOC_NOPM, 0, 1, 0,
		       es705_get_tx1_route_enable_value,
		       es705_put_tx1_route_enable_value),
	SOC_SINGLE_EXT("ES705 RX2 Enable", SND_SOC_NOPM, 0, 1, 0,
		       es705_get_rx2_route_enable_value,
		       es705_put_rx2_route_enable_value),
	SOC_ENUM_EXT("Mic Config", es705_mic_config_enum,
		     es705_get_control_enum, es705_put_control_enum),
	SOC_ENUM_EXT("AEC Mode", es705_aec_mode_enum,
		     es705_get_control_enum, es705_put_control_enum),
	SOC_ENUM_EXT("VEQ Enable", es705_veq_enable_enum,
		     es705_get_control_enum, es705_put_control_enum),
	SOC_ENUM_EXT("Dereverb Enable", es705_dereverb_enable_enum,
		     es705_get_control_enum, es705_put_control_enum),
	SOC_SINGLE_EXT("Dereverb Gain",
		       ES705_DEREVERB_GAIN, 0, 100, 0,
		       es705_get_dereverb_gain_value, es705_put_dereverb_gain_value),
	SOC_ENUM_EXT("BWE Enable", es705_bwe_enable_enum,
		     es705_get_control_enum, es705_put_control_enum),
	SOC_SINGLE_EXT("BWE High Band Gain",
		       ES705_BWE_HIGH_BAND_GAIN, 0, 100, 0,
		       es705_get_bwe_high_band_gain_value,
		       es705_put_bwe_high_band_gain_value),
	SOC_SINGLE_EXT("BWE Max SNR",
		       ES705_BWE_MAX_SNR, 0, 100, 0,
		       es705_get_bwe_max_snr_value, es705_put_bwe_max_snr_value),
	SOC_ENUM_EXT("BWE Post EQ Enable", es705_bwe_post_eq_enable_enum,
		     es705_get_control_enum, es705_put_control_enum),
	SOC_SINGLE_EXT("SLIMbus Link Multi Channel",
		       ES705_SLIMBUS_LINK_MULTI_CHANNEL, 0, 65535, 0,
		       es705_get_control_value, es705_put_control_value),
	SOC_ENUM_EXT("Set Power State", es705_power_state_enum,
		       es705_get_power_state_enum, es705_put_power_state_enum),
	SOC_ENUM_EXT("Algorithm Processing", es705_algo_processing_enable_enum,
		     es705_get_control_enum, es705_put_control_enum),
	SOC_ENUM_EXT("Algorithm Sample Rate", es705_algo_sample_rate_enum,
		     es705_get_control_enum, es705_put_control_enum),
	SOC_ENUM_EXT("Algorithm", es705_algorithms_enum,
		     es705_get_control_enum, es705_put_control_enum),
	SOC_ENUM_EXT("Mix Sample Rate", es705_algo_mix_rate_enum,
		     es705_get_control_enum, es705_put_control_enum),
	SOC_SINGLE_EXT("Internal Route",
		       SND_SOC_NOPM, 0, 100, 0, es705_get_internal_route,
		       es705_put_internal_route),
	SOC_ENUM_EXT("Internal Rate", es705_internal_rate_enum,
		      es705_get_internal_rate,
		      es705_put_internal_rate),
	SOC_SINGLE_EXT("Preset",
		       ES705_PRESET, 0, 65535, 0, es705_get_preset_value,
		       es705_put_preset_value),
	SOC_SINGLE_EXT("Audio Custom Profile",
		       SND_SOC_NOPM, 0, 100, 0, es705_get_audio_custom_profile,
		       es705_put_audio_custom_profile),
	SOC_ENUM_EXT("ES705-AP Tx Channels", es705_ap_tx1_ch_cnt_enum,
		     es705_ap_get_tx1_ch_cnt, es705_ap_put_tx1_ch_cnt),
	SOC_ENUM_EXT("Voice Sense Set Wakeup Word",
		     es705_vs_wakeup_keyword_enum,
		     es705_get_vs_wakeup_keyword, es705_put_vs_wakeup_keyword),
	SOC_ENUM_EXT("Voice Sense Status",
		     es705_vs_event_enum,
		     es705_get_control_enum, NULL),
	SOC_ENUM_EXT("Voice Sense Training Mode",
			 es705_vs_training_mode_enum,
			 es705_get_control_enum, es705_put_control_enum),
	SOC_ENUM_EXT("Voice Sense Training Status",
		     es705_vs_training_status_enum,
		     es705_get_control_enum, NULL),
	SOC_ENUM_EXT("Voice Sense Training Record",
		     es705_vs_training_record_enum,
		     NULL, es705_put_control_enum),
	SOC_ENUM_EXT("Voice Sense Stored Keyword",
		     es705_vs_stored_keyword_enum,
		     NULL, es705_put_vs_stored_keyword),
	SOC_SINGLE_EXT("Voice Sense Detect Sensitivity",
			ES705_VOICE_SENSE_DETECTION_SENSITIVITY, 0, 10, 0,
			es705_get_vs_detection_sensitivity,
			es705_put_vs_detection_sensitivity),
	SOC_SINGLE_EXT("Voice Activity Detect Sensitivity",
			ES705_VOICE_ACTIVITY_DETECTION_SENSITIVITY, 0, 10, 0,
			es705_get_vad_sensitivity,
			es705_put_vad_sensitivity),
	SOC_SINGLE_EXT("Continuous Voice Sense Preset",
		       ES705_CVS_PRESET, 0, 65535, 0,
		       es705_get_cvs_preset_value,
		       es705_put_cvs_preset_value),
	SOC_ENUM_EXT("ES705 Power State", es705_vs_power_state_enum,
		     es705_get_power_control_enum,
		     es705_put_power_control_enum),
	SOC_ENUM_EXT("Noise Suppression", es705_ns_enable_enum,
		       es705_get_ns_value,
		       es705_put_ns_value),
	SOC_ENUM_EXT("Audio Zoom", es705_audio_zoom_enum,
		       es705_get_aud_zoom,
		       es705_put_aud_zoom),
	SOC_SINGLE_EXT("Enable/Disable Streaming PATH/Endpoint",
		       ES705_FE_STREAMING, 0, 65535, 0,
		       es705_get_streaming_select,
		       es705_put_control_value),
	SOC_ENUM_EXT("RX Enable", es705_rx_enable_enum,
		       es705_get_control_enum,
		       es705_put_control_enum),
	SOC_ENUM_EXT("Stereo Widening", es705_sw_enable_enum,
		       es705_get_sw_value,
		       es705_put_sw_value),
	SOC_ENUM_EXT("Speech Time Stretching", es705_sts_enable_enum,
			   es705_get_sts_value,
			   es705_put_sts_value),
	SOC_ENUM_EXT("RX Noise Suppression", es705_rx_ns_enable_enum,
			   es705_get_rx_ns_value,
			   es705_put_rx_ns_value),
	SOC_ENUM_EXT("Wind Noise Filter", es705_wnf_enable_enum,
			   es705_get_wnf_value,
			   es705_put_wnf_value),
	SOC_ENUM_EXT("BWE Preset", es705_bwe_preset_enable_enum,
			   es705_get_bwe_value,
			   es705_put_bwe_value),
	SOC_ENUM_EXT("AVALON Wind Noise", es705_avalon_wn_enable_enum,
			   es705_get_avalon_value,
			   es705_put_avalon_value),
	SOC_ENUM_EXT("Virtual Bass Boost", es705_vbb_enable_enum,
			   es705_get_vbb_value,
			   es705_put_vbb_value),
	SOC_SINGLE_EXT("ES705 Get RDB data size",
		       SND_SOC_NOPM, 0, 65535, 0,
		       es705_get_rdb_size, NULL),
	SOC_ENUM_EXT("UART app rate", es705_uart_app_rate,
		      es705_get_uart_app_rate,
		      es705_put_uart_app_rate),
	SOC_SINGLE_EXT("ES705 Get Event Status",
		       SND_SOC_NOPM, 0, 65535, 0,
		       es705_get_vs_status, NULL),
};

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
		.ops = &es705_slim_port_dai_ops,
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
		.ops = &es705_slim_port_dai_ops,
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
		.ops = &es705_slim_port_dai_ops,
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
		.ops = &es705_slim_port_dai_ops,
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
		.ops = &es705_slim_port_dai_ops,
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
		.ops = &es705_slim_port_dai_ops,
	},
	{
		.name = "es705-porta",
		.id = ES705_I2S_PORTA,
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
		.ops = &es705_i2s_port_dai_ops,
	},
	{
		.name = "es705-portb",
		.id = ES705_I2S_PORTB,
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
		.ops = &es705_i2s_port_dai_ops,
	},
	{
		.name = "es705-portc",
		.id = ES705_I2S_PORTC,
		.playback = {
			.stream_name = "PORTC Playback",
			.channels_min = 2,
			.channels_max = 2,
			.rates = ES705_RATES,
			.formats = ES705_FORMATS,
		},
		.capture = {
			.stream_name = "PORTC Capture",
			.channels_min = 2,
			.channels_max = 2,
			.rates = ES705_RATES,
			.formats = ES705_FORMATS,
		},
		.ops = &es705_i2s_port_dai_ops,
	},
	{
		.name = "es705-portd",
		.id = ES705_I2S_PORTD,
		.playback = {
			.stream_name = "PORTD Playback",
			.channels_min = 2,
			.channels_max = 2,
			.rates = ES705_RATES,
			.formats = ES705_FORMATS,
		},
		.capture = {
			.stream_name = "PORTD Capture",
			.channels_min = 2,
			.channels_max = 2,
			.rates = ES705_RATES,
			.formats = ES705_FORMATS,
		},
		.ops = &es705_i2s_port_dai_ops,
	},
};

int remote_esxxx_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	/* es705_wakeup_request(&es705_priv);*/
	return 0;
}

int remote_esxxx_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	/* es705_sleep_queue(&es705_priv);*/
	return 0;
}


#ifdef CONFIG_PM
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
static int es705_codec_suspend(struct snd_soc_codec *codec)
#else
static int es705_codec_suspend(struct snd_soc_codec *codec, pm_message_t state)
#endif
{
	struct es705_priv *es705 = snd_soc_codec_get_drvdata(codec);

	es705_set_bias_level(codec, SND_SOC_BIAS_OFF);

	es705_sleep(es705);

	return 0;
}

static int es705_codec_resume(struct snd_soc_codec *codec)
{
	struct es705_priv *es705 = snd_soc_codec_get_drvdata(codec);

	es705_wakeup(es705);

	es705_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	return 0;
}
#else
#define es705_codec_suspend NULL
#define es705_codec_resume NULL
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
		dev_err(codec->dev, "%s(): es705_digital_ext_snd_controls failed\n", __func__);

	return rc;
}

static int es705_codec_probe(struct snd_soc_codec *codec)
{
	struct es705_priv *es705 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s()\n", __func__);
	es705->codec = codec;

	codec->control_data = snd_soc_codec_get_drvdata(codec);

	es705_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	/*
	rc = snd_soc_add_controls(codec, es705_digital_snd_controls,
					ARRAY_SIZE(es705_digital_snd_controls));
	if (rc)
		dev_err(codec->dev, "%s(): es705_digital_snd_controls failed\n", __func__);
	*/

	return 0;
}

static int  es705_codec_remove(struct snd_soc_codec *codec)
{
	struct es705_priv *es705 = snd_soc_codec_get_drvdata(codec);

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

static void es705_event_status(struct es705_priv *es705)
{
#if defined(CONFIG_MQ100_SENSOR_HUB)
	if (callbacks.intr != 0)
		callbacks.intr(callbacks.priv);

	/* too high freq for dev_dbg! */
	/* dev_dbg(es705_priv.dev, "%s: Interrupt Event\n", __func__); */
#else
	int rc = 0;
	struct slim_device *sbdev = es705->gen0_client;
	const u32 vs_get_key_word_status = 0x806D0000;
	u32 rspn = 0;
	int match = 0;
	u16 event_response = 0;
	u32 smooth_mute = ES705_SET_SMOOTH_MUTE_ZERO;
	u32 es_set_power_level = ES705_SET_POWER_LEVEL << 16 |
		ES705_POWER_LEVEL_6;


	/* wait for command mode to be ready in case of CVS */
	msleep(50);

	if (es705_priv.cvs_preset != 0xFFFF &&
			es705_priv.cvs_preset != 0) {
		es705->es705_power_state = ES705_SET_POWER_STATE_NORMAL;
		es705->pm_state = ES705_POWER_AWAKE;
		es705->mode = STANDARD;
	}

	if (es705->es705_power_state == ES705_SET_POWER_STATE_VS_LOWPWR) {
		rc = es705_wakeup(es705);
		if (rc) {
			dev_err(es705->dev, "%s(): Get VS Status Fail\n",
					__func__);
			return;
		}
	}

	cpu_to_le32s(&vs_get_key_word_status);
	rc = es705_write_then_read(es705, &vs_get_key_word_status,
			sizeof(vs_get_key_word_status),
			&rspn, match);
	if (rc) {
		dev_err(es705->dev, "%s(): Get VS Status Fail\n", __func__);
		return;
	}

	le32_to_cpus(&rspn);
	event_response = rspn & 0xFFFF;
	/* Check VS detection status. */
	dev_dbg(es705->dev, "%s(): VS status 0x%04x\n",
			__func__, rspn);

	/* Work around not to check flag the status for testing */
	if ((event_response & 0x00FF) == KW_DETECTED) {
		dev_dbg(es705->dev, "%s(): Generate VS keyword detected event to User space\n",
				__func__);

		/* If CVS preset is set (other than 0xFFFF), es705 is
		 * in CVS mode. To make it switch from internal to
		 * external oscillator, send power level command with
		 * highest power level */
		if (es705_priv.cvs_preset != 0xFFFF &&
				es705_priv.cvs_preset != 0) {
			cpu_to_le32s(smooth_mute);
			rc = es705_write_then_read(es705, &smooth_mute,
					sizeof(smooth_mute),
					&rspn, 0);
			if (rc) {
				dev_err(es705->dev,
						"%s(): es705 smooth mute failed\n",
						__func__);
				return;
			}

			usleep_range(2000, 2005);
			if (es705_priv.pdata->esxxx_clk_cb)
				es705_priv.pdata->esxxx_clk_cb(1);

			es_set_power_level =
				cpu_to_le32(es_set_power_level);
			rc = es705_write_then_read(es705,
					&es_set_power_level,
					sizeof(es_set_power_level),
					&rspn, 0);
			if (rc) {
				dev_err(es705->dev,
					"%s(): es705 power level 6 failed\n",
					__func__);
				return;
			}
			usleep_range(2000, 2005);


			/* Each time es705 comes in BOSKO mode after
			 * VS detect, CVS mode will be disabled */
			es705_priv.cvs_preset = 0;
		}
		es705->vs_get_event = event_response;
		kobject_uevent(&sbdev->dev.kobj, KOBJ_CHANGE);
	}
#endif
}

static irqreturn_t es705_irq_event(int irq, void *irq_data)
{
	struct es705_priv *es705 = (struct es705_priv *)irq_data;

	mutex_lock(&es705->api_mutex);

	/* too high freq for dev_dbg! */
	/* dev_dbg(es705->dev, "%s(): %s mode, Interrupt event",
		__func__, esxxx_mode[es705->mode]);
	*/

#ifdef CONFIG_SND_SOC_ES_SPI_SENSOR_HUB
	if ((es705->mode == STANDARD) &&
		   (!work_pending(&es705->sensor_event_work))) {
		queue_work(es705->spi_workq, &es705->sensor_event_work);

	}
#else
	/* Get Event status, reset Interrupt */
	es705_event_status(es705);
#endif
	mutex_unlock(&es705->api_mutex);
	return IRQ_HANDLED;
}

static int register_snd_soc(struct es705_priv *priv)
{
	int rc;
	int i;
	int ch_cnt;
	struct slim_device *sbdev = priv->gen0_client;

	es705_init_slim_slave(sbdev);

	dev_dbg(&sbdev->dev, "%s(): name = %s\n", __func__, sbdev->name);
	rc = snd_soc_register_codec(&sbdev->dev, &soc_codec_dev_es705,
				    es705_dai, ARRAY_SIZE(es705_dai));
	dev_dbg(&sbdev->dev, "%s(): rc = snd_soc_regsiter_codec() = %d\n",
		__func__, rc);

	/* allocate ch_num array for each DAI */
	for (i = 0; i < ARRAY_SIZE(es705_dai); i++) {
		switch (es705_dai[i].id) {
		case ES705_SLIM_1_PB:
		case ES705_SLIM_2_PB:
		case ES705_SLIM_3_PB:
			ch_cnt = es705_dai[i].playback.channels_max;
			break;
		case ES705_SLIM_1_CAP:
		case ES705_SLIM_2_CAP:
		case ES705_SLIM_3_CAP:
			ch_cnt = es705_dai[i].capture.channels_max;
			break;
		default:
			continue;
		}
		es705_priv.dai[i].ch_num =
			kzalloc((ch_cnt * sizeof(unsigned int)), GFP_KERNEL);
	}
	/* front end for RX1 */
	es705_priv.dai[DAI_INDEX(ES705_SLIM_1_PB)].ch_num[0] = 152;
	es705_priv.dai[DAI_INDEX(ES705_SLIM_1_PB)].ch_num[1] = 153;
	/* back end for RX1 */
#ifdef CONFIG_SLIMBUS_MSM_NGD
	es705_priv.dai[DAI_INDEX(ES705_SLIM_2_CAP)].ch_num[0] = 144;
	es705_priv.dai[DAI_INDEX(ES705_SLIM_2_CAP)].ch_num[1] = 145;

#else
	es705_priv.dai[DAI_INDEX(ES705_SLIM_2_CAP)].ch_num[0] = 138;
	es705_priv.dai[DAI_INDEX(ES705_SLIM_2_CAP)].ch_num[1] = 139;
#endif
	/* front end for TX1 */
	es705_priv.dai[DAI_INDEX(ES705_SLIM_1_CAP)].ch_num[0] = 156;
	es705_priv.dai[DAI_INDEX(ES705_SLIM_1_CAP)].ch_num[1] = 157;
	/* back end for TX1 */
	es705_priv.dai[DAI_INDEX(ES705_SLIM_3_PB)].ch_num[0] = 134;
	es705_priv.dai[DAI_INDEX(ES705_SLIM_3_PB)].ch_num[1] = 135;
	/* front end for RX2 */
	es705_priv.dai[DAI_INDEX(ES705_SLIM_2_PB)].ch_num[0] = 154;
	es705_priv.dai[DAI_INDEX(ES705_SLIM_2_PB)].ch_num[1] = 155;
	/* back end for RX2 */
#ifdef CONFIG_SLIMBUS_MSM_NGD
	es705_priv.dai[DAI_INDEX(ES705_SLIM_3_CAP)].ch_num[0] = 144;
	es705_priv.dai[DAI_INDEX(ES705_SLIM_3_CAP)].ch_num[1] = 145;
#else
	es705_priv.dai[DAI_INDEX(ES705_SLIM_3_CAP)].ch_num[0] = 143;
	es705_priv.dai[DAI_INDEX(ES705_SLIM_3_CAP)].ch_num[1] = 144;
#endif

	return rc;
}

int es705_enable_ext_clk(int enable)
{
#ifdef CONFIG_SND_SOC_ES705_EXTCLK_OVER_GPIO
	dev_dbg(es705_priv.dev, "%s(): Ext Clk ON GPIO %d, wait 3ms and continue\n",
		__func__, es705_priv.pdata->extclk_gpio);
	gpio_set_value(es705_priv.pdata->extclk_gpio, enable);
	if (enable)
		usleep_range(3000, 3100);
#else
	static struct platform_device msm_esxxx_mclk_dev = {
		.name = "esxxx_mclk_dev_pdev",
		.id = -1,
		.dev = {
			.init_name = "esxxx_mclk_dev",
		},
	};

	struct clk *esxxx_codec_clk;
	dev_dbg(es705_priv.dev, "%s: clk_get osr_clk enable=%d\n",
		__func__, enable);

	esxxx_codec_clk = clk_get(&msm_esxxx_mclk_dev.dev, "osr_clk");
	if (!esxxx_codec_clk) {
		dev_err(es705_priv.dev, "%s: clk_get osr_clk FAIL\n",
		__func__);
		return -ENODEV;
	}

	if (enable)
		clk_prepare_enable(esxxx_codec_clk);
	else
		clk_disable_unprepare(esxxx_codec_clk);

	clk_put(esxxx_codec_clk);
#endif
	return 0;
}

int es705_core_probe(struct device *dev)
{
	struct esxxx_platform_data *pdata = dev->platform_data;
	int rc = 0;
#if defined(CONFIG_MQ100_SENSOR_HUB)
	const char *fw_filename = "audience-mq100-fw.bin";
#else
	const char *fw_filename = "audience-es705-fw.bin";
#endif
	const char *vs_filename = "audience-es705-vs.bin";

	if (pdata == NULL) {
		dev_err(dev, "%s(): pdata is NULL", __func__);
		rc = -EIO;
		goto pdata_error;
	}

	es705_priv.dev = dev;
	es705_priv.pdata = pdata;
	es705_priv.fw_requested = 0;
#ifdef CONFIG_SND_SOC_ES705_UART_FW_DOWNLOAD
	es705_priv.uart_fw_download = es705_uart_fw_download;
#else
	es705_priv.uart_fw_download = NULL;
#endif

	mutex_init(&es705_priv.datablock_read_mutex);
	mutex_init(&es705_priv.api_mutex);
	mutex_init(&es705_priv.pm_mutex);
	mutex_init(&es705_priv.wake_mutex);
	mutex_init(&es705_priv.abort_mutex);
	mutex_init(&es705_priv.streaming_mutex);
#if defined(CONFIG_MQ100_SENSOR_HUB)
	mutex_init(&es705_priv.uart_transaction_mutex);
#endif
	init_waitqueue_head(&es705_priv.stream_in_q);

#if defined(CONFIG_SND_SOC_ES705_DATABLOCK_BUS_UART)
	es705_priv.datablockdev = uart_datablockdev;
#elif defined(CONFIG_SND_SOC_ES705_DATABLOCK_BUS_SLIM)
	es705_priv.datablockdev = slim_datablockdev;
#else
	/* Use default interface */
#endif

#ifdef CONFIG_SND_SOC_ES_SPI_SENSOR_HUB
	/* Add initialization for sensor input devices */
	rc = es705_sensor_hub_init_data_driver(&es705_priv);
	if (rc != 0) {
		dev_err(es705_priv.dev,
		    "[SPI]: %s - could not create input device\n",
		    __func__);
		goto sens_init_err;
	}
#elif defined(CONFIG_MQ100_SENSOR_HUB)
	if (callbacks.probe != 0) {
		dev_info(es705_priv.dev, "%s: Probe MQ100\n", __func__);
		callbacks.priv = callbacks.probe(&es705_priv);
		if (!callbacks.priv)
			dev_err(es705_priv.dev, "[MQ100]: %s probe FAIL\n",
			    __func__);
	}
#endif

	/* No keyword parameters available until set. */
	es705_priv.vs_keyword_param_size = 0;

	rc = sysfs_create_group(&es705_priv.dev->kobj, &core_sysfs);
	if (rc) {
		dev_err(es705_priv.dev, "%s(): failed to create core sysfs entries: %d\n",
			__func__, rc);
	}

#ifdef CONFIG_SND_SOC_ES705_EXTCLK_OVER_GPIO
	dev_dbg(es705_priv.dev, "%s(): extclk_gpio = %d\n",
		__func__, pdata->extclk_gpio);
	rc = gpio_request(pdata->extclk_gpio, "es705_extclk");
	if (rc < 0) {
		dev_warn(es705_priv.dev, "%s(): es705_extclk already requested",
			__func__);
	} else {
		rc = gpio_direction_output(pdata->extclk_gpio, 1);
		if (rc < 0) {
			dev_err(es705_priv.dev, "%s(): es705_extclk direction failed",
				__func__);
			goto extclk_gpio_direction_error;
		}
		pdata->esxxx_clk_cb = es705_enable_ext_clk;
		pdata->esxxx_clk_cb(1);
	}
#endif

	dev_dbg(es705_priv.dev, "%s(): reset_gpio = %d\n",
		__func__, pdata->reset_gpio);
	rc = gpio_request(pdata->reset_gpio, "es705_reset");
	if (rc < 0) {
		dev_warn(es705_priv.dev, "%s(): es705_reset already requested",
			__func__);
	} else {
		rc = gpio_direction_output(pdata->reset_gpio, 0);
		if (rc < 0) {
			dev_err(es705_priv.dev, "%s(): es705_reset direction failed",
				__func__);
			goto reset_gpio_direction_error;
		}
		es705_gpio_reset(&es705_priv);
	}

#ifndef CONFIG_SND_SOC_ES705_UART_WAKEUP
	dev_dbg(es705_priv.dev, "%s(): wakeup_gpio = %d\n",
		__func__, pdata->wakeup_gpio);
	rc = gpio_request(pdata->wakeup_gpio, "es705_wakeup");
	if (rc < 0) {
		dev_err(es705_priv.dev, "%s(): es705_wakeup request failed",
			__func__);
		goto wakeup_gpio_request_error;
	}
	rc = gpio_direction_output(pdata->wakeup_gpio, 0);
	if (rc < 0) {
		dev_err(es705_priv.dev, "%s(): es705_wakeup direction failed",
			__func__);
		goto wakeup_gpio_direction_error;
	}
#endif

	rc = request_firmware((const struct firmware **)&es705_priv.standard,
			      fw_filename, es705_priv.dev);
	if (rc) {
		dev_err(es705_priv.dev, "%s(): request_firmware(%s) failed %d\n",
			__func__, fw_filename, rc);
		goto request_firmware_error;
	}

	rc = request_firmware((const struct firmware **)&es705_priv.vs,
			      vs_filename, es705_priv.dev);
	if (rc) {
		dev_err(es705_priv.dev, "%s(): request_firmware(%s) failed %d\n",
			__func__, vs_filename, rc);
		goto request_vs_firmware_error;
	}

	/* read 32 bit abort keyword from offset 0x000C from VS firmware */
	es705_priv.vs_abort_kw = *(u32 *)((es705_priv.vs->data) + 0x000c);

	INIT_DELAYED_WORK(&es705_priv.sleep_work, es705_delayed_sleep);

	es705_priv.pm_state = ES705_POWER_BOOT;
	es705_priv.fw_requested = 1;
#if defined(CONFIG_MQ100_SENSOR_HUB)
	dev_dbg(es705_priv.dev, "%s(): sensor hub INTR gpio = %d\n",
		__func__, pdata->gpioa_gpio);

	rc = request_threaded_irq(gpio_to_irq(pdata->gpioa_gpio),
					NULL,
					es705_irq_event, IRQF_TRIGGER_RISING,
					"es705-irq-event", &es705_priv);
	if (rc) {
		dev_err(es705_priv.dev, "%s(): event request_irq() failed\n",
			__func__);
		goto event_irq_request_error;
	}
	rc = irq_set_irq_wake(gpio_to_irq(pdata->gpioa_gpio), 1);
	if (rc < 0) {
		dev_err(es705_priv.dev, "%s(): set event irq wake failed\n",
			__func__);
		disable_irq(gpio_to_irq(pdata->gpioa_gpio));
		free_irq(gpio_to_irq(pdata->gpioa_gpio),
				&es705_priv);
		goto event_irq_wake_error;
	}
#else
	dev_dbg(es705_priv.dev, "%s(): Event Intr gpio = %d\n",
		__func__, pdata->gpiob_gpio);

	rc = request_threaded_irq(gpio_to_irq(pdata->gpiob_gpio),
					NULL,
					es705_irq_event, IRQF_TRIGGER_RISING,
					"es705-irq-event", &es705_priv);
	if (rc) {
		dev_err(es705_priv.dev, "%s(): event request_irq() failed\n",
			__func__);
		goto event_irq_request_error;
	}
	rc = irq_set_irq_wake(gpio_to_irq(pdata->gpiob_gpio), 1);
	if (rc < 0) {
		dev_err(es705_priv.dev, "%s(): set event irq wake failed\n",
			__func__);
		disable_irq(gpio_to_irq(pdata->gpiob_gpio));
		free_irq(gpio_to_irq(pdata->gpiob_gpio),
				&es705_priv);
		goto event_irq_wake_error;
	}
#endif
	return rc;

event_irq_wake_error:
event_irq_request_error:
request_vs_firmware_error:
	release_firmware(es705_priv.standard);
request_firmware_error:
#ifndef CONFIG_SND_SOC_ES705_UART_WAKEUP
wakeup_gpio_direction_error:
	gpio_free(pdata->wakeup_gpio);
wakeup_gpio_request_error:
#endif
#ifdef CONFIG_SND_SOC_ES705_EXTCLK_OVER_GPIO
extclk_gpio_direction_error:
	gpio_free(pdata->extclk_gpio);
#endif
reset_gpio_direction_error:
	gpio_free(pdata->reset_gpio);

#ifdef CONFIG_SND_SOC_ES_SPI_SENSOR_HUB
sens_init_err:
#endif

pdata_error:
	dev_dbg(es705_priv.dev, "%s(): exit with error\n", __func__);
	return rc;
}
EXPORT_SYMBOL_GPL(es705_core_probe);

static __init int es705_init(void)
{
	int rc = 0;
#if defined(CONFIG_SND_SOC_ES705_I2C)

	dev_dbg(es705_priv.dev, "ES705 using I2C\n");
	rc = es705_i2c_init(&es705_priv);

#elif defined(CONFIG_SND_SOC_ES705_SPI)

	dev_dbg(es705_priv.dev, "ES705 using SPI\n");
	rc = spi_register_driver(&es705_spi_driver);
	if (!rc) {
		pr_debug("%s() registered as SPI", __func__);
		es705_priv.intf = ES705_SPI_INTF;
	}
#else
	/* Bus specifc registration */
	/* FIXME: Temporary kludge until es705_bus_init abstraction
	 * is worked out */
#if !defined(CONFIG_SND_SOC_ES705_UART)
	dev_dbg(es705_priv.dev, "ES705 using SLIM\n");
	rc = es705_bus_init(&es705_priv);
#else
	dev_dbg(es705_priv.dev, "ES705 using UART\n");
	rc = es705_uart_bus_init(&es705_priv);
#endif

#endif
	if (rc) {
		pr_debug("Error registering Audience eS705 driver: %d\n", rc);
		goto INIT_ERR;
	}

#if !defined(CONFIG_SND_SOC_ES705_UART)
/* If CONFIG_SND_SOC_ES705_UART, UART probe will initialize char device
 * if a es705 is found */
	rc = es705_init_cdev(&es705_priv);
	if (rc) {
		pr_err("failed to initialize char device = %d\n", rc);
		goto INIT_ERR;
	}
#endif
#if defined(CONFIG_MQ100_SENSOR_HUB)
	rc = es705_rdb_register(&callbacks);
	if (rc) {
		pr_err("failed to initialize MQ100 sensors hub. Error %d\n",
		       rc);
		goto INIT_ERR;
	}
#endif

INIT_ERR:
	return rc;
}
module_init(es705_init);

static __exit void es705_exit(void)
{
	if (es705_priv.fw_requested) {
		release_firmware(es705_priv.standard);
		release_firmware(es705_priv.vs);
	}
	es705_cleanup_cdev(&es705_priv);

#if defined(CONFIG_SND_SOC_ES705_I2C)
	i2c_del_driver(&es705_i2c_driver);
#else
	/* no support from QCOM to unregister
	 * slim_driver_unregister(&es705_slim_driver);
	 */
#endif

#ifdef CONFIG_SND_SOC_ES_SPI_SENSOR_HUB
	es705_sensor_hub_remove_data_driver(&es705_priv);
#endif
}
module_exit(es705_exit);


MODULE_DESCRIPTION("ASoC ES705 driver");
MODULE_AUTHOR("Greg Clemson <gclemson@audience.com>");
MODULE_AUTHOR("Genisim Tsilker <gtsilker@audience.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:es705-codec");
MODULE_FIRMWARE("audience-es705-fw.bin");
