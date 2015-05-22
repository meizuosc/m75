/*
 * es325.c  --  Audience eS325 ALSA SoC Audio driver
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

#include "es325.h"
#include "escore.h"
#include "escore-i2c.h"
#include "escore-i2s.h"
#include "escore-slim.h"
#include "escore-cdev.h"
#include "escore-uart.h"
#include "escore-uart-common.h"
#include "es325-access.h"

/* codec private data */
struct escore_priv escore_priv = {
	.pm_state = ES_PM_ACTIVE,
	.probe = es325_core_probe,
	.slim_setup = es325_slim_setup,
	.set_streaming = es325_set_streaming,
};

static int es325_slim_rx_port_to_ch[ES_SLIM_RX_PORTS] = {
	152, 153, 154, 155, 134, 135
};
static int es325_slim_tx_port_to_ch[ES_SLIM_TX_PORTS] = {
	156, 157, 138, 139, 143, 144
};

#if defined(CONFIG_SND_SOC_ES_SLIM)
static int es325_slim_be_id[ES_NUM_CODEC_SLIM_DAIS] = {
	ES_SLIM_2_CAP, /* for ES_SLIM_1_PB tx from es325 */
	ES_SLIM_3_PB, /* for ES_SLIM_1_CAP rx to es325 */
	ES_SLIM_3_CAP, /* for ES_SLIM_2_PB tx from es325 */
	-1, /* for ES_SLIM_2_CAP */
	-1, /* for ES_SLIM_3_PB */
	-1, /* for ES_SLIM_3_CAP */
};
#endif

static struct escore_slim_dai_data dai_data[ES_NUM_CODEC_SLIM_DAIS];
static struct escore_slim_ch slim_rx[ES_SLIM_RX_PORTS];
static struct escore_slim_ch slim_tx[ES_SLIM_TX_PORTS];

static const u32 es325_streaming_cmds[4] = {
	0x90250200,		/* ES_SLIM_INTF */
	0x90250000,		/* ES_I2C_INTF  */
	0x90250300,		/* ES_SPI _INTF */
	0x90250100,		/* ES_UART_INTF */
};

static unsigned int es325_valid_power_states[] = {
	ES_POWER_STATE_SLEEP,
	ES_POWER_STATE_NORMAL,
};

#define ES325_CUSTOMER_PROFILE_MAX 4
static u32 es325_audio_custom_profiles[ES325_CUSTOMER_PROFILE_MAX][20] = {
	{
		0xffffffff	/* terminate */
	},
	{
		0xffffffff	/* terminate */
	},
	{
		0xffffffff	/* terminate */
	},
	{
		0xffffffff	/* terminate */
	},
};
#define ES325_INTERNAL_ROUTE_MAX 20
static long es325_internal_route_num;
static u32 es325_internal_route_configs[ES325_INTERNAL_ROUTE_MAX][40] = {
	/* [0]: Audio route reset */
	{
		0xb04e0000,	/* Gain rate change = 0 */
		0x905c0000,	/* stop route */
		0xffffffff	/* terminate */
	},
	/* [1]: Audio playback, 1 channel */
	{
		0xb05c0004,	/* Algo = passthrough */
		0xb04c0001,	/* Algo rate = 16kHz */
		0xb05a18a0,	/* SBUS.Rx0 -> AUDIN1 */
		0xb05a4cac,	/* SBUS.Tx2 <- AUDOUT1 */
		0x901c0000,	/* Algo processing = off (COMMIT) */
		0xffffffff	/* terminate */
	},
	/* [2]: Audio playback, 2 channels */
	{
		0xb05c0004,	/* Algo = passthrough */
		0xb04c0001,	/* Algo rate = 16kHz */
		0xb05a18a0,	/* SBUS.Rx0 -> AUDIN1 */
		0xb05a1ca1,	/* SBUS.Rx1 -> AUDIN2 */
		0xb05a4cac,	/* SBUS.Tx2 <- AUDOUT1 */
		0xb05a50ad,	/* SBUS.Tx3 <- AUDOUT2 */
		0x901c0000,	/* Algo processing = off (COMMIT) */
		0xffffffff	/* terminate */
	},
	/* [3]: Audio record, 1 channel */
	{
		0xb05c0004,	/* Algo = passthrough */
		0xb04c0001,	/* Algo rate = 16kHz */
		0xb05a18a4,	/* SBUS.Rx4 -> AUDIN1 */
		0xb05a4caa,	/* SBUS.Tx0 <- AUDOUT1 */
		0x901c0000,	/* Algo processing = off (COMMIT) */
		0xffffffff	/* terminate */
	},
	/* [4]: Audio record, 2 channels */
	{
		0xb05c0004,	/* Algo = passthrough */
		0xb04c0001,	/* Algo rate = 16kHz */
		0xb05a18a4,	/* SBUS.Rx4 -> AUDIN1 */
		0xb05a1ca5,	/* SBUS.Rx5 -> AUDIN2 */
		0xb05a4caa,	/* SBUS.Tx0 <- AUDOUT1 */
		0xb05a50ab,	/* SBUS.Tx1 <- AUDOUT2 */
		0x901c0000,	/* Algo processing = off (COMMIT) */
		0xffffffff	/* terminate */
	},
	/* [5]: 1-mic Headset */
	{
		0xb05c0001,	/* Algo = Voice processing */
		0xb04c0001,	/* Algo rate = 16kHz */
		0xb05e0001,	/* Mix rate = 16 kHz */
		0xb05a28a0,	/* SBUS.Rx0 -> UITONE1 (host to 325) */
		0xb05a14a2,	/* SBUS.Rx2 -> FEIN (host to 325) */
		0xb05a04a4,	/* SBUS.Rx4 -> PRI (codec to 325) */
		0xb05a40aa,	/* SBUS.Tx0 <- CSOUT (325 to host) */
		0xb05a44ae,	/* SBUS.Tx4 <- FEOUT1 (325 to codec) */
		0xb05a48af,	/* SBUS.Tx5 <- FEOUT2 (325 to codec) */
		0x901c0000,	/* Algo processing = off (COMMIT) */
		0xffffffff	/* terminate */
	},
	/* [6]: 1-mic CS Voice (CT) */
	{
		0xb05c0001,	/* Algo = Voice processing */
		0xb04c0001,	/* Algo rate = 16kHz */
		0xb05e0001,	/* Mix rate = 16 kHz */
		0xb05a28a0,	/* SBUS.Rx0 -> UITONE1 (host to 325) */
		0xb05a14a2,	/* SBUS.Rx2 -> FEIN (host to 325) */
		0xb05a04a4,	/* SBUS.Rx4 -> PRI (codec to 325) */
		0xb05a40aa,	/* SBUS.Tx0 <- CSOUT (325 to host) */
		0xb05a44ae,	/* SBUS.Tx4 <- FEOUT1 (325 to codec) */
		0xb0170002,	/* Mic config = ... */
		0xb0180002,	/*      ... 1-MIC CT */
		0x901c0000,	/* Algo processing = off  */
		0xffffffff	/* terminate */
	},
	/* [7]: 2-mic CS Voice (CT) */
	{
		0xb05c0001,	/* Algo = Voice processing */
		0xb04c0001,	/* Algo rate = 16 kHz */
		0xb05e0001,	/* Mix rate = 16 kHz */
		0xb05a28a0,	/* SBUS.Rx0 -> UITONE1 (host to 325) */
		0xb05a14a2,	/* SBUS.Rx2 -> FEIN (host to 325) */
		0xb05a04a4,	/* SBUS.Rx4 -> PRI (codec to 325) */
		0xb05a08a5,	/* SBUS.Rx5 -> SEC (codec to 325) */
		0xb05a40aa,	/* SBUS.Tx0 <- CSOUT (325 to host) */
		0xb05a44ae,	/* SBUS.Tx4 <- FEOUT1 (325 to codec) */
		0xb0170002,	/* Mic config = ... */
		0xb0180000,	/*      ... 2-MIC CT */
		0x901c0000,	/* Algo processing = off (COMMIT) */
		0xffffffff	/* terminate */
	},
	/* [8]: 1-mic VOIP (CT) */
	{
		0xb05c0001,	/* Algo = Voice processing */
		0xb04c0001,	/* Algo rate = 16 kHz */
		0xb05e0001,	/* Mix rate = 16 kHz */
		0xb05a14a0,	/* SBUS.Rx0 -> FEIN */
		0xb05a04a4,	/* SBUS.Rx4 -> PRI (codec to 325) */
		0xb05a40aa,	/* SBUS.Tx0 <- CSOUT (325 to host) */
		0xb05a44ac,	/* SBUS.Tx2 <- FEOUT1 */
		0xb0170002,	/* Mic config = ... */
		0xb0180002,	/*      ... 1-MIC CT */
		0x901c0000,	/* Algo processing = off (COMMIT) */
		0xffffffff	/* terminate */
	},
	/* [9]: 2-mic VOIP (CT) */
	{
		0xb05c0001,	/* Algo = Voice processing */
		0xb04c0001,	/* Algo rate = 16 kHz */
		0xb05e0001,	/* Mix rate = 16 kHz */
		0xb05a14a0,	/* SBUS.Rx0 -> FEIN */
		0xb05a04a4,	/* SBUS.Rx4 -> PRI (codec to 325) */
		0xb05a08a5,	/* SBUS.Rx5 -> SEC (codec to 325) */
		0xb05a40aa,	/* SBUS.Tx0 <- CSOUT (325 to host) */
		0xb05a44ac,	/* SBUS.Tx2 <- FEOUT1 */
		0xb0170002,	/* Mic config = ... */
		0xb0180000,	/*      ... 2-MIC CT */
		0x901c0000,	/* Algo processing = off (COMMIT) */
		0xffffffff	/* terminate */
	},
	/* [10]: 1-mic CS Voice (FT) */
	{
		0xb05c0001,	/* Algo = Voice processing */
		0xb04c0001,	/* Algo rate = 16kHz */
		0xb05e0001,	/* Mix rate = 16 kHz */
		0xb05a28a0,	/* SBUS.Rx0 -> UITONE1 (host to 325) */
		0xb05a14a2,	/* SBUS.Rx2 -> FEIN (host to 325) */
		0xb05a04a4,	/* SBUS.Rx4 -> PRI (codec to 325) */
		0xb05a40aa,	/* SBUS.Tx0 <- CSOUT (325 to host) */
		0xb05a44ae,	/* SBUS.Tx4 <- FEOUT1 (325 to codec) */
		0xb0170002,	/* Mic config = ... */
		0xb0180002,	/*      ... 1-MIC FT */
		0x901c0000,	/* Algo processing = off  */
		0xffffffff	/* terminate */
	},
	/* [11]: 2-mic CS Voice (FT) */
	{
		0xb05c0001,	/* Algo = Voice processing */
		0xb04c0001,	/* Algo rate = 16 kHz */
		0xb05e0001,	/* Mix rate = 16 kHz */
		0xb05a28a0,	/* SBUS.Rx0 -> UITONE1 (host to 325) */
		0xb05a14a2,	/* SBUS.Rx2 -> FEIN (host to 325) */
		0xb05a04a4,	/* SBUS.Rx4 -> PRI (codec to 325) */
		0xb05a08a5,	/* SBUS.Rx5 -> SEC (codec to 325) */
		0xb05a40aa,	/* SBUS.Tx0 <- CSOUT (325 to host) */
		0xb05a44ae,	/* SBUS.Tx4 <- FEOUT1 (325 to codec) */
		0xb0170002,	/* Mic config = ... */
		0xb0180001,	/*      ... 2-MIC FT */
		0x901c0000,	/* Algo processing = off (COMMIT) */
		0xffffffff	/* terminate */
	},
	/* [12]: 1-mic VOIP (FT) */
	{
		0xb05c0001,	/* Algo = Voice processing */
		0xb04c0001,	/* Algo rate = 16 kHz */
		0xb05e0001,	/* Mix rate = 16 kHz */
		0xb05a14a0,	/* SBUS.Rx0 -> FEIN */
		0xb05a04a4,	/* SBUS.Rx4 -> PRI (codec to 325) */
		0xb05a40aa,	/* SBUS.Tx0 <- CSOUT (325 to host) */
		0xb05a44ac,	/* SBUS.Tx2 <- FEOUT1 */
		0xb0170002,	/* Mic config = ... */
		0xb0180002,	/*      ... 1-MIC FT */
		0x901c0000,	/* Algo processing = off (COMMIT) */
		0xffffffff	/* terminate */
	},
	/* [13]: 2-mic VOIP (FT) */
	{
		0xb05c0001,	/* Algo = Voice processing */
		0xb04c0001,	/* Algo rate = 16 kHz */
		0xb05e0001,	/* Mix rate = 16 kHz */
		0xb05a14a0,	/* SBUS.Rx0 -> FEIN */
		0xb05a04a4,	/* SBUS.Rx4 -> PRI (codec to 325) */
		0xb05a08a5,	/* SBUS.Rx5 -> SEC (codec to 325) */
		0xb05a40aa,	/* SBUS.Tx0 <- CSOUT (325 to host) */
		0xb05a44ac,	/* SBUS.Tx2 <- FEOUT1 */
		0xb0170002,	/* Mic config = ... */
		0xb0180001,	/*      ... 2-MIC FT */
		0x901c0000,	/* Algo processing = off (COMMIT) */
		0xffffffff	/* terminate */
	},
	/* [14]: Passthrough_Playback - I2C */
	{
		0xB05C0004,	/* SetAlgoType Pass-Through Staged */
		0xB05A1840,	/* SetDataPath AudIn1 PCM2 Channel0 Staged */
		0xB05A4C00,	/* SetDataPath AudOut1 PCM0 Channel0 Staged */
		0xB05A1C41,	/* SetDataPath AudIn2 PCM2 Channel1 Staged */
		0x905A5001,	/* SetDataPath AudOut2 PCM0 Channel1 Commit */
		0xffffffff	/* terminate */
	},
	/* [15]: Passthrough_Capture - I2C */
	{
		0xB05C0004,	/* SetAlgoType Pass-Through Staged */
		0xB05A1800,	/* SetDataPath AudIn1 PCM0 Channel0 Staged */
		0xB05A4C40,	/* SetDataPath AudOut1 PCM2 Channel0 Staged */
		0xB05A1C01,	/* SetDataPath AudIn2 PCM0 Channel1 Staged */
		0x905A5041,	/* SetDataPath AudOut2 PCM2 Channel1 Commit */
		0xffffffff	/* terminate */
	},


};

static void es325_switch_route(long route_index)
{
	struct escore_priv *es325 = &escore_priv;
	int rc;

	if (route_index > ES325_INTERNAL_ROUTE_MAX) {
		pr_debug("%s(): es325_internal_route = %ld is out of range\n",
				__func__, route_index);
		return;
	}

	pr_debug("%s():current es325_internal_route = %ldto new route = %ld\n",
			__func__, es325_internal_route_num, route_index);
	es325_internal_route_num = route_index;
	rc = escore_write_block(es325,
		  &es325_internal_route_configs[es325_internal_route_num][0]);
}


static ssize_t es325_route_status_show(struct device *dev,
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

static DEVICE_ATTR(route_status, 0444, es325_route_status_show, NULL);
/* /sys/devices/platform/msm_slim_ctrl.1/es325-codec-gen0/route_status */

static ssize_t es325_route_config_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	struct escore_priv *es325 = &escore_priv;
	pr_info("%s(): route=%ld\n", __func__, es325->internal_route_num);
	return snprintf(buf, PAGE_SIZE, "route=%ld\n",
			es325->internal_route_num);
}

static DEVICE_ATTR(route_config, 0444, es325_route_config_show, NULL);
/* /sys/devices/platform/msm_slim_ctrl.1/es325-codec-gen0/route_config */

#define SIZE_OF_VERBUF 256
static ssize_t es325_fw_version_show(struct device *dev,
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

static DEVICE_ATTR(fw_version, 0444, es325_fw_version_show, NULL);
/* /sys/devices/platform/msm_slim_ctrl.1/es325-codec-gen0/fw_version */

static ssize_t es325_txhex_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	pr_debug("%s called\n", __func__);
	return 0;
}

/* TODO: fix for new read write */
static ssize_t es325_txhex_set(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct escore_priv *es325 = &escore_priv;
	u8 cmd[128];
	int cmdlen;
	int offset;
	u8 resp[4];
	int rc = 0;

	pr_debug("%s called\n", __func__);
	pr_debug("%s count=%i\n", __func__, count);

	/* No command sequences larger than 128 bytes. */
	BUG_ON(count > (128 * 2) + 1);
	/* Expect a even number of hexadecimal digits terminated by a
	 * newline. */
	BUG_ON(!(count & 1));

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
	rc = hex2bin(cmd, buf, count / 2);
#else
	hex2bin(cmd, buf, count / 2);
#endif

	BUG_ON(rc != 0);
	pr_debug("%s rc==%i\n", __func__, rc);
	cmdlen = count / 2;
	offset = 0;
	pr_debug("%s cmdlen=%i\n", __func__, cmdlen);
	pr_debug("%s(): mutex lock\n", __func__);
	mutex_lock(&es325->api_mutex);
	while (offset < cmdlen) {
		/* Commands must be written in 4 byte blocks. */
		int wrsize = (cmdlen - offset > 4) ? 4 : cmdlen - offset;
		es325->bus.ops.write(es325, &cmd[offset], wrsize);
		usleep_range(10000, 10000);
		es325->bus.ops.read(es325, resp, 4);
		pr_debug("%s: %02x%02x%02x%02x\n", __func__,
			 resp[0], resp[1], resp[2], resp[3]);
		offset += wrsize;
	}
	pr_debug("%s(): mutex unlock\n", __func__);
	mutex_unlock(&es325->api_mutex);

	return count;
}

static DEVICE_ATTR(txhex, 0644, es325_txhex_show, es325_txhex_set);
/* /sys/devices/platform/msm_slim_ctrl.1/es325-codec-gen0/txhex */

static ssize_t es325_clock_on_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	int ret = 0;

	return ret;
}

static DEVICE_ATTR(clock_on, 0444, es325_clock_on_show, NULL);
/* /sys/devices/platform/msm_slim_ctrl.1/es325-codec-gen0/clock_on */

static struct attribute *core_sysfs_attrs[] = {
	&dev_attr_route_status.attr,
	&dev_attr_route_config.attr,
	&dev_attr_fw_version.attr,
	&dev_attr_txhex.attr,
	&dev_attr_clock_on.attr,
	NULL
};

static struct attribute_group core_sysfs = {
	.attrs = core_sysfs_attrs
};

#if defined(CONFIG_SND_SOC_ES_UART) || \
	defined(CONFIG_SND_SOC_ES_HIGH_BW_BUS_UART)

int es325_uart_check_sbl_mode(struct escore_priv *es325)
{
	int rc = 0;
	char msg;

	/* Write SBL Sync command */
	msg = ESCORE_SBL_SYNC_CMD;

	rc = escore_uart_write(es325, &msg, 1);
	if (rc < sizeof(msg)) {
		pr_err("%s: firmware load failed sbl sync write, rc = %d\n",
				__func__, rc);
		rc = rc < 0 ? rc : -EIO;
		goto es325_sbl_bootup_failed;
	}
	/* Wait > 100 us to get ack */
	usleep_range(100, 100);

	/* Read SBL sync ack */
	rc = escore_uart_read(es325, &msg, 1);
	if (rc < 0) {
		pr_err("%s: read() failure for SBL sync ack, rc = %d\n",
			__func__, rc);
		goto es325_sbl_bootup_failed;
	}
	dev_dbg(es325->dev,
		"%s: es325->bus.ops.read() returning %d msg = 0x%2x\n",
		__func__, rc, msg);

	if (msg != ESCORE_SBL_SYNC_ACK) {
		pr_err("%s: SBL sync ack failure, msg = 0x%2x\n",
			__func__, msg);
		rc = -EIO;
		goto es325_sbl_bootup_failed;
	}

	/* Write SBL Boot command */
	msg = ESCORE_SBL_BOOT_CMD;
	rc = escore_uart_write(es325, &msg, 1);
	if (rc < sizeof(msg)) {
		pr_err("%s: firmware load failed sbl boot write, rc = %d\n",
				__func__, rc);
		rc = rc < 0 ? rc : -EIO;
		goto es325_sbl_bootup_failed;
	}

	/* Wait > 100 us to get ack */
	usleep_range(100, 100);

	/* Read SBL boot ack */
	rc = escore_uart_read(es325, &msg, 1);
	if (rc < 0) {
		pr_err("%s: bus read() failure for SBL boot ack, rc = %d\n",
			__func__, rc);
		goto es325_sbl_bootup_failed;
	}
	dev_dbg(es325->dev,
		"%s() : es325 bus read() returning %d msg = %2x\n",
		__func__, rc, msg);
	if (msg != ESCORE_SBL_BOOT_ACK) {
		pr_err("%s: SBL boot ack failure, msg = 0x%2x\n",
			__func__, msg);
		rc = -EIO;
		goto es325_sbl_bootup_failed;
	}

es325_sbl_bootup_failed:
	pr_debug("%s: Return code = %d\n", __func__, rc);
	return rc;
}

/* Download 1st stage bootloader to configure Baudrate in ES325 */
int es325_uart_stage1_fw_download(struct escore_priv *es325)
{
	int rc = 0;

	/* Send stage 1 bootloader to set eS325 baudrate to 3M */
	rc = escore_uart_write(es325,
				UARTFirstStageBoot_InputClk_19_200_Baud_3M,
				UART_FW_IMAGE_1_SIZE);

	if (rc < 0) {
		pr_err("%s(): tty_write failed with %d for fw image 1\n",
			__func__, rc);
		return rc;
	}

	pr_debug("%s: Standard 1st stage download finished", __func__);

	/* Setup Baudrate for standard fw download */
	escore_configure_tty(escore_uart.tty, UART_TTY_BAUD_RATE_3_M,
				UART_TTY_STOP_BITS);

	return es325_uart_check_sbl_mode(es325);
}
#endif

#define ES325_FW_LOAD_BUF_SZ 4
int es325_bootup(struct escore_priv *es325)
{
	int rc;

	pr_info("%s()\n", __func__);

	BUG_ON(es325->standard->size == 0);

	mutex_lock(&es325->api_mutex);

	if (es325->boot_ops.setup) {
		pr_info("%s(): calling bus specific boot setup\n", __func__);
		rc = es325->boot_ops.setup(es325);
		if (rc < 0) {
			pr_err("%s() bus specific boot setup error\n",
				__func__);
			goto es325_bootup_failed;
		}
	}

#if defined(CONFIG_SND_SOC_ES_UART) || \
	defined(CONFIG_SND_SOC_ES_HIGH_BW_BUS_UART)

	/* Download 1st stage bootloader to configure Baudrate in ES325 */
	rc = es325_uart_stage1_fw_download(es325);
	if (rc < 0) {
		pr_err("%s() UART 1st stage fw download failure, RC = %d\n",
			__func__, rc);
		goto es325_bootup_failed;
	}
#endif

	/* write firmware */
	pr_debug("%s(): write firmware image\n", __func__);
	rc = es325->bus.ops.high_bw_write(es325, (char *)es325->standard->data,
						es325->standard->size);
	if (rc < 0) {
		pr_err("%s(): firmware download failed\n", __func__);
		rc = -EIO;
		goto es325_bootup_failed;
	}

	/* Give the chip some time to become ready after firmware
	 * download. */
	msleep(20);

	if (es325->boot_ops.finish) {
		pr_info("%s(): calling bus specific boot finish\n", __func__);
		rc = es325->boot_ops.finish(es325);
		if (rc != 0) {
			pr_err("%s() bus specific boot finish error\n",
				__func__);
			goto es325_bootup_failed;
		}
		/* Firmware is downloaded successfully */
		pr_info("%s(): Firmware load success\n", __func__);
		es325->flag.is_fw_ready = 1;
	}

es325_bootup_failed:
	mutex_unlock(&es325->api_mutex);
	return rc;
}

static int es325_sleep(struct escore_priv *es325)
{
	int rc;
	u32 reg = ES_POWER_STATE;
	u32 power_state = ES_POWER_STATE_SLEEP;
	u32 smooth_mute = ES_SMOOTH_MUTE;
	u32 resp;

	pr_debug("%s()\n", __func__);

	rc = escore_cmd(es325, smooth_mute, &resp);
	if (rc < 0) {
		pr_err("%s(): Smooth mute write failed\n", __func__);
		return rc;
	}
	/* Send Sleep Command */
	rc = escore_write(NULL, reg, power_state);
	if (rc < 0) {
		pr_err("%s(): failed power state write\n",
				__func__);
		return rc;
	}
	/* sleep range 20 ms to 25 ms */
	usleep_range(20000, 25000);

	/* Clock off */
	if (es325->pdata->esxxx_clk_cb)
		es325->pdata->esxxx_clk_cb(0);

	return rc;
}

static int es325_wakeup(struct escore_priv *es325)
{
	u32 sync_cmd = (ES_SYNC_CMD << 16) | ES_SYNC_POLLING;
	u32 sync_ack;
	int retryCount = 10, rc;

	pr_debug("%s()\n", __func__);

	/* Clock On */
	if (es325->pdata->esxxx_clk_cb)
		es325->pdata->esxxx_clk_cb(0);

	/* Wakeup GPIO H */
	gpio_set_value(es325->pdata->wakeup_gpio, 1);

	/* Sleep range 1 ms to 3 ms */
	usleep_range(1000, 3000);
	gpio_set_value(es325->pdata->wakeup_gpio, 0);

	mutex_lock(&es325->api_mutex);
	do {
		/* Sleep 30 ms */
		msleep(30);
		pr_debug("%s(): write ES_SYNC_CMD = 0x%08x\n",
				__func__, sync_cmd);

		rc = escore_cmd(es325, sync_cmd, &sync_ack);
		if (rc < 0) {
			pr_err("%s(): wakeup sync write failed\n", __func__);
			continue;
		}
		pr_info("%s(): sync_ack = 0x%08x\n", __func__, sync_ack);
		if (sync_ack != ES_SYNC_ACK) {
			pr_err("%s(): failed sync ack pattern", __func__);
			rc = -EIO;
		} else {
			pr_info("%s(): wakeup sync write success\n", __func__);
			break;
		}

	} while (--retryCount);

	mutex_unlock(&es325->api_mutex);
	gpio_set_value(es325->pdata->wakeup_gpio, 1);

	return rc;
}

static int es325_put_control_enum(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = escore_priv.codec;
	struct soc_enum *e =
		(struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	unsigned int value;
	int rc = 0;

	value = ucontrol->value.enumerated.item[0];
	rc = escore_write(codec, reg, value);

	return 0;
}

static int es325_get_control_enum(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = escore_priv.codec;
	struct soc_enum *e =
		(struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	unsigned int value;

	value = escore_read(codec, reg);
	ucontrol->value.enumerated.item[0] = value;

	return 0;
}

static int es325_get_rx1_route_enable_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		escore_priv.flag.rx1_route_enable;
	pr_debug("%s(): rx1_route_enable = %d\n", __func__,
		escore_priv.flag.rx1_route_enable);

	return 0;
}

static int es325_put_rx1_route_enable_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	/* fw is not downloaded yet */
	if (!escore_priv.flag.is_fw_ready) {
		pr_info("%s(): Firmware is not ready\n", __func__);
		return 0;
	}

	escore_priv.flag.rx1_route_enable =
		ucontrol->value.integer.value[0];
	pr_debug("%s(): rx1_route_enable = %d\n", __func__,
		escore_priv.flag.rx1_route_enable);

	return 0;
}

static int es325_get_tx1_route_enable_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		escore_priv.flag.tx1_route_enable;
	pr_debug("%s(): tx1_route_enable = %d\n", __func__,
		escore_priv.flag.tx1_route_enable);

	return 0;
}

static int es325_put_tx1_route_enable_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	/* fw is not downloaded yet */
	if (!escore_priv.flag.is_fw_ready) {
		pr_info("%s(): Firmware is not ready\n", __func__);
		return 0;
	}

	escore_priv.flag.tx1_route_enable =
		ucontrol->value.integer.value[0];
	pr_debug("%s(): tx1_route_enable = %d\n", __func__,
		escore_priv.flag.tx1_route_enable);

	return 0;
}

static int es325_get_rx2_route_enable_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		escore_priv.flag.rx2_route_enable;
	pr_debug("%s(): rx2_route_enable = %d\n", __func__,
		escore_priv.flag.rx2_route_enable);

	return 0;
}

static int es325_put_rx2_route_enable_value(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	/* fw is not downloaded yet */
	if (!escore_priv.flag.is_fw_ready) {
		pr_info("%s(): Firmware is not ready\n", __func__);
		return 0;
	}

	escore_priv.flag.rx2_route_enable =
		ucontrol->value.integer.value[0];
	pr_debug("%s(): rx2_route_enable = %d\n", __func__,
		escore_priv.flag.rx2_route_enable);

	return 0;
}

static int es325_put_internal_route_config(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	/* fw is not downloaded yet */
	if (!escore_priv.flag.is_fw_ready) {
		pr_info("%s(): Firmware is not ready\n", __func__);
		return 0;
	}

	es325_switch_route(ucontrol->value.integer.value[0]);
	return 0;
}

static int es325_get_internal_route_config(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = es325_internal_route_num;

	return 0;
}

static int es325_get_audio_custom_profile(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int es325_put_audio_custom_profile(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	int index = ucontrol->value.integer.value[0];

	if (index < ES325_CUSTOMER_PROFILE_MAX)
		escore_write_block(&escore_priv,
				  &es325_audio_custom_profiles[index][0]);
	return 0;
}

static int es325_ap_put_tx1_ch_cnt(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	escore_priv.ap_tx1_ch_cnt = ucontrol->value.enumerated.item[0] + 1;
	return 0;
}

static int es325_ap_get_tx1_ch_cnt(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = escore_priv.ap_tx1_ch_cnt - 1;

	return 0;
}

static const char * const es325_ap_tx1_ch_cnt_texts[] = {
	"One", "Two"
};
static const struct soc_enum es325_ap_tx1_ch_cnt_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0,
			ARRAY_SIZE(es325_ap_tx1_ch_cnt_texts),
			es325_ap_tx1_ch_cnt_texts);

static const char * const es325_mic_config_texts[] = {
	"CT 2-mic", "FT 2-mic", "DV 1-mic", "EXT 1-mic", "BT 1-mic",
	"CT ASR 2-mic", "FT ASR 2-mic", "EXT ASR 1-mic", "FT ASR 1-mic",
};
static const struct soc_enum es325_mic_config_enum =
	SOC_ENUM_SINGLE(ES_MIC_CONFIG, 0,
			ARRAY_SIZE(es325_mic_config_texts),
			es325_mic_config_texts);

static const char * const es325_algo_rates_text[] = {
	"fs=8khz", "fs=16khz", "fs=24khz", "fs=48khz", "fs=96khz", "fs=192khz"
};
static const struct soc_enum es325_algo_sample_rate_enum =
	SOC_ENUM_SINGLE(ES_ALGO_SAMPLE_RATE, 0,
			ARRAY_SIZE(es325_algo_rates_text),
			es325_algo_rates_text);

static const char * const es325_algorithms_text[] = {
	"None", "VP", "Two CHREC", "AUDIO", "Four CHPASS"
};
static const struct soc_enum es325_algorithms_enum =
	SOC_ENUM_SINGLE(ES_ALGO_SAMPLE_RATE, 0,
			ARRAY_SIZE(es325_algorithms_text),
			es325_algorithms_text);

static const char * const es325_off_on_texts[] = {
	"Off", "On"
};
static const struct soc_enum es325_algo_processing_enable_enum =
	SOC_ENUM_SINGLE(ES_ALGO_PROCESSING, 0,
			ARRAY_SIZE(es325_off_on_texts),
			es325_off_on_texts);

static int es325_update_power_state(struct escore_priv *es325, int new_state)
{
	int rc = 0;

	if (new_state == es325->pm_state) {
		pr_info("[%s]: No Power State change\n", __func__);
		return rc;
	}

	switch (es325->pm_state) {
	case ES_POWER_STATE_NORMAL:
		if (new_state == ES_POWER_STATE_SLEEP) {
			new_state = ES_POWER_STATE_SLEEP_PENDING;
			/* Defer Sleep */
			schedule_delayed_work(&es325->sleep_work,
				msecs_to_jiffies(ES_SLEEP_DELAY));
			break;
		}
	case ES_POWER_STATE_SLEEP_REQUESTED:

		rc = es325_sleep(es325);
		if (rc < 0) {
			/* Sleep Power State Failed,
			 * setting Active power state in es325->pm_state
			 */
			new_state = ES_POWER_STATE_NORMAL;
		}
		break;
	case ES_POWER_STATE_SLEEP_PENDING:
		if (new_state == ES_POWER_STATE_NORMAL)
			cancel_delayed_work(&es325->sleep_work);

		break;
	case ES_POWER_STATE_SLEEP:

		es325_wakeup(es325);
		break;
	}
	es325->pm_state = new_state;

	return rc;
}

static void es325_delayed_sleep(struct work_struct *work_req)
{
	struct delayed_work *d_work = to_delayed_work(work_req);
	struct escore_priv *es325 = container_of(d_work, struct escore_priv,
			sleep_work);
	int ch_tot, ports_active;

	mutex_lock(&es325->pm_mutex);
	ports_active = (es325->flag.rx1_route_enable ||
			es325->flag.rx2_route_enable ||
			es325->flag.tx1_route_enable);

	ch_tot = es325->slim_dai_data[ES_SLIM_1_PB - 1].ch_tot;
	ch_tot += es325->slim_dai_data[ES_SLIM_2_PB - 1].ch_tot;
	ch_tot += es325->slim_dai_data[ES_SLIM_1_CAP - 1].ch_tot;

	pr_info("%s %d active channels, ports_active: %d\n", __func__,
				ch_tot, ports_active);
	if ((ch_tot > 0) || (ports_active != 0)) {
		/* There are still active stream which needs to be
		 * freed first.
		 */
		es325->pm_state = ES_POWER_STATE_NORMAL;
		pr_info("%s: Active streams, do not SLEEP\n", __func__);

	} else if (es325->pm_state == ES_POWER_STATE_NORMAL) {
		/* Race between put_power_state() and delayed_sleep()
		 * can cause the power_state to be changed to NORMAL if
		 * put_power_state() wins and acquires the mutex.
		 * Do nothing in that case
		 */
		pr_info("%s: Chip is awake, do not SLEEP\n", __func__);

	} else if (es325->pm_state == ES_POWER_STATE_SLEEP_PENDING) {
		/* Going to send SLEEP command to chip. It takes 30 msec
		 * to put the chip into sleep. Changing the power_state
		 * in software little bit early
		 */
		es325->pm_state = ES_POWER_STATE_SLEEP_REQUESTED;
		es325_update_power_state(es325, ES_POWER_STATE_SLEEP);

	}
	mutex_unlock(&es325->pm_mutex);
}


static int es325_put_power_state_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct escore_priv *es325 = &escore_priv;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int new_state;
	int rc = 0;

	if (ucontrol->value.enumerated.item[0] >= e->max) {
		pr_err("[%s]:Value out of range\n", __func__);
		return -EINVAL;
	}

	/* fw is not downloaded yet */
	if (!escore_priv.flag.is_fw_ready) {
		pr_info("%s(): Firmware is not ready\n", __func__);
		return 0;
	}

	new_state =
		es325_valid_power_states[ucontrol->value.enumerated.item[0]];

	mutex_lock(&es325->pm_mutex);

	rc = es325_update_power_state(es325, new_state);

	mutex_unlock(&es325->pm_mutex);

	return rc;
}

static int es325_get_power_state_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	int value = -EINVAL;
	int i, mapped_state;
	struct escore_priv *es325 = &escore_priv;

	switch (es325->pm_state) {

	case ES_POWER_STATE_SLEEP_PENDING:
	case ES_POWER_STATE_SLEEP_REQUESTED:
		mapped_state = ES_POWER_STATE_SLEEP;
		break;
	default:
		mapped_state = es325->pm_state;
		break;
	}

	for (i = 0; i < sizeof(es325_valid_power_states); i++) {
		if (es325_valid_power_states[i] == mapped_state) {
			value = i;
			break;
		}
	}
	ucontrol->value.enumerated.item[0] = value;

	return (value > 0) ? 0 : value;
}
static const char * const es325_power_state_texts[] = {
	"Sleep", "Active"
};
static const struct soc_enum es325_power_state_enum =
	SOC_ENUM_SINGLE(ES_POWER_STATE, 0,
			ARRAY_SIZE(es325_power_state_texts),
			es325_power_state_texts);

static struct snd_kcontrol_new es325_digital_ext_snd_controls[] = {
	/* commit controls */
	SOC_SINGLE_EXT("ES325 RX1 Enable", SND_SOC_NOPM, 0, 1, 0,
		       es325_get_rx1_route_enable_value,
		       es325_put_rx1_route_enable_value),
	SOC_SINGLE_EXT("ES325 TX1 Enable", SND_SOC_NOPM, 0, 1, 0,
		       es325_get_tx1_route_enable_value,
		       es325_put_tx1_route_enable_value),
	SOC_SINGLE_EXT("ES325 RX2 Enable", SND_SOC_NOPM, 0, 1, 0,
		       es325_get_rx2_route_enable_value,
		       es325_put_rx2_route_enable_value),
	SOC_ENUM_EXT("Mic Config", es325_mic_config_enum,
		     es325_get_control_enum, es325_put_control_enum),
	SOC_ENUM_EXT("Set Power State", es325_power_state_enum,
		       es325_get_power_state_enum, es325_put_power_state_enum),
	SOC_ENUM_EXT("Algorithm Processing", es325_algo_processing_enable_enum,
		     es325_get_control_enum, es325_put_control_enum),
	SOC_ENUM_EXT("Algorithm Sample Rate", es325_algo_sample_rate_enum,
		     es325_get_control_enum, es325_put_control_enum),
	SOC_ENUM_EXT("Algorithm", es325_algorithms_enum,
		     es325_get_control_enum, es325_put_control_enum),
	SOC_SINGLE_EXT("Internal Route Config",
		       SND_SOC_NOPM, 0, 100, 0, es325_get_internal_route_config,
		       es325_put_internal_route_config),
	SOC_SINGLE_EXT("Audio Custom Profile",
		       SND_SOC_NOPM, 0, 100, 0, es325_get_audio_custom_profile,
		       es325_put_audio_custom_profile),
	SOC_ENUM_EXT("ES325-AP Tx Channels", es325_ap_tx1_ch_cnt_enum,
		     es325_ap_get_tx1_ch_cnt, es325_ap_put_tx1_ch_cnt)
};


int es325_remote_add_codec_controls(struct snd_soc_codec *codec)
{
	int rc;

	dev_dbg(codec->dev, "%s()\n", __func__);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
	rc = snd_soc_add_codec_controls(codec, es325_digital_ext_snd_controls,
				ARRAY_SIZE(es325_digital_ext_snd_controls));
#else
	rc = snd_soc_add_controls(codec, es325_digital_ext_snd_controls,
				ARRAY_SIZE(es325_digital_ext_snd_controls));
#endif
	if (rc)
		dev_err(codec->dev,
			"%s(): es325_digital_ext_snd_controls failed\n",
			__func__);

	return rc;
}

struct snd_soc_dai_driver es325_dai[] = {
#if defined(CONFIG_SND_SOC_ES_I2S)
	{
		.name = "es325-porta",
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
		.name = "es325-portb",
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
		.name = "es325-portc",
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
	{
		.name = "es325-portd",
		.id = ES_I2S_PORTD,
		.playback = {
			.stream_name = "PORTD Playback",
			.channels_min = 2,
			.channels_max = 2,
			.rates = ES_RATES,
			.formats = ES_FORMATS,
		},
		.capture = {
			.stream_name = "PORTD Capture",
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
		.name = "es325-slim-rx1",
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
		.name = "es325-slim-tx1",
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
		.name = "es325-slim-rx2",
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
		.name = "es325-slim-tx2",
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
		.name = "es325-slim-rx3",
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
		.name = "es325-slim-tx3",
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

int es325_set_streaming(struct escore_priv *es325, int value)
{
	u32 resp;
	return escore_cmd(es325,
		es325_streaming_cmds[es325->streamdev.intf] | value, &resp);
}

void es325_slim_setup(struct escore_priv *es325)
{
	int i;
	int ch_cnt;

	/* allocate ch_num array for each DAI */
	for (i = 0; i < ARRAY_SIZE(es325_dai); i++) {
		switch (es325->dai[i].id) {
		case ES_SLIM_1_PB:
		case ES_SLIM_2_PB:
		case ES_SLIM_3_PB:
			ch_cnt = es325->dai[i].playback.channels_max;
			break;
		case ES_SLIM_1_CAP:
		case ES_SLIM_2_CAP:
		case ES_SLIM_3_CAP:
			ch_cnt = es325->dai[i].capture.channels_max;
			break;
		default:
				continue;
		}
		es325->slim_dai_data[i].ch_num =
			kzalloc((ch_cnt * sizeof(unsigned int)), GFP_KERNEL);
	}
	/* front end for RX1 */
	es325->slim_dai_data[DAI_INDEX(ES_SLIM_1_PB)].ch_num[0] = 152;
	es325->slim_dai_data[DAI_INDEX(ES_SLIM_1_PB)].ch_num[1] = 153;
	/* back end for RX1 */
	es325->slim_dai_data[DAI_INDEX(ES_SLIM_2_CAP)].ch_num[0] = 138;
	es325->slim_dai_data[DAI_INDEX(ES_SLIM_2_CAP)].ch_num[1] = 139;
	/* front end for TX1 */
	es325->slim_dai_data[DAI_INDEX(ES_SLIM_1_CAP)].ch_num[0] = 156;
	es325->slim_dai_data[DAI_INDEX(ES_SLIM_1_CAP)].ch_num[1] = 157;
	/* back end for TX1 */
	es325->slim_dai_data[DAI_INDEX(ES_SLIM_3_PB)].ch_num[0] = 134;
	es325->slim_dai_data[DAI_INDEX(ES_SLIM_3_PB)].ch_num[1] = 135;
	/* front end for RX2 */
	es325->slim_dai_data[DAI_INDEX(ES_SLIM_2_PB)].ch_num[0] = 154;
	es325->slim_dai_data[DAI_INDEX(ES_SLIM_2_PB)].ch_num[1] = 155;
	/* back end for RX2 */
	es325->slim_dai_data[DAI_INDEX(ES_SLIM_3_CAP)].ch_num[0] = 143;
	es325->slim_dai_data[DAI_INDEX(ES_SLIM_3_CAP)].ch_num[1] = 144;
}

int es325_slim_set_channel_map(struct snd_soc_dai *dai,
			       unsigned int tx_num, unsigned int *tx_slot,
			       unsigned int rx_num, unsigned int *rx_slot)
{
	struct snd_soc_codec *codec = dai->codec;
	/* remote codec access */
	struct escore_priv *escore = &escore_priv;
	int id = dai->id;
	int i;
	int rc = 0;

	dev_dbg(codec->dev, "%s() dai->name = %s, dai->id = %d\n", __func__,
		dai->name, dai->id);

	if (id == ES_SLIM_1_PB ||
	    id == ES_SLIM_2_PB ||
	    id == ES_SLIM_3_PB) {
		escore->slim_dai_data[id - 1].ch_tot = rx_num;
		escore->slim_dai_data[id - 1].ch_act = 0;
		for (i = 0; i < rx_num; i++)
			escore->slim_dai_data[id - 1].ch_num[i] = rx_slot[i];
	} else if (id == ES_SLIM_1_CAP ||
		 id == ES_SLIM_2_CAP ||
		 id == ES_SLIM_3_CAP) {
		escore->slim_dai_data[id - 1].ch_tot = tx_num;
		escore->slim_dai_data[id - 1].ch_act = 0;
		for (i = 0; i < tx_num; i++)
			escore->slim_dai_data[id - 1].ch_num[i] = tx_slot[i];
	}

	return rc;
}

int es325_slim_get_channel_map(struct snd_soc_dai *dai,
			       unsigned int *tx_num, unsigned int *tx_slot,
			       unsigned int *rx_num, unsigned int *rx_slot)
{
	struct snd_soc_codec *codec = dai->codec;
	/* remote codec access */
	struct escore_priv *escore = &escore_priv;
	struct escore_slim_ch *rx = escore->slim_rx;
	struct escore_slim_ch *tx = escore->slim_tx;
	int id = dai->id;
	int i;
	int rc = 0;

	dev_dbg(codec->dev, "%s() dai->name = %s, dai->id = %d\n", __func__,
		dai->name, dai->id);

	if (id == ES_SLIM_1_PB) {
		*rx_num = escore->dai[id - 1].playback.channels_max;
		for (i = 0; i < *rx_num; i++)
			rx_slot[i] = rx[ES_SLIM_1_PB_OFFSET + i].ch_num;
	} else if (id == ES_SLIM_2_PB) {
		*rx_num = escore->dai[id - 1].playback.channels_max;
		for (i = 0; i < *rx_num; i++)
			rx_slot[i] = rx[ES_SLIM_2_PB_OFFSET + i].ch_num;
	} else if (id == ES_SLIM_3_PB) {
		*rx_num = escore->dai[id - 1].playback.channels_max;
		for (i = 0; i < *rx_num; i++)
			rx_slot[i] = rx[ES_SLIM_3_PB_OFFSET + i].ch_num;
	} else if (id == ES_SLIM_1_CAP) {
		*tx_num = escore->dai[id - 1].capture.channels_max;
		for (i = 0; i < *tx_num; i++)
			tx_slot[i] = tx[ES_SLIM_1_CAP_OFFSET + i].ch_num;
	} else if (id == ES_SLIM_2_CAP) {
		*tx_num = escore->dai[id - 1].capture.channels_max;
		for (i = 0; i < *tx_num; i++)
			tx_slot[i] = tx[ES_SLIM_2_CAP_OFFSET + i].ch_num;
	} else if (id == ES_SLIM_3_CAP) {
		*tx_num = escore->dai[id - 1].capture.channels_max;
		for (i = 0; i < *tx_num; i++)
			tx_slot[i] = tx[ES_SLIM_3_CAP_OFFSET + i].ch_num;
	}

	return rc;
}

int es325_remote_route_enable(struct snd_soc_dai *dai)
{

	pr_debug("%s():dai->name = %s dai->id = %d\n", __func__,
		 dai->name, dai->id);

	switch (dai->id) {
	case ES_SLIM_1_PB:
		pr_debug("%s():rx1_en = %d\n", __func__,
			 escore_priv.flag.rx1_route_enable);
		return escore_priv.flag.rx1_route_enable;
	case ES_SLIM_1_CAP:
		pr_debug("%s():tx1_en = %d\n", __func__,
			 escore_priv.flag.tx1_route_enable);
		return escore_priv.flag.tx1_route_enable;
	case ES_SLIM_2_PB:
		pr_debug("%s():rx2_en = %d\n", __func__,
			 escore_priv.flag.rx2_route_enable);
		return escore_priv.flag.rx2_route_enable;
	default:
		pr_debug("%s():default = 0\n", __func__);
		return 0;
	}
}

int es325_core_probe(struct device *dev)
{
	struct esxxx_platform_data *pdata = dev->platform_data;
	int rc = 0;
	const char *fw_filename = "audience-es325-fw.bin";

	if (pdata == NULL) {
		dev_err(dev, "%s(): pdata is NULL", __func__);
		rc = -EIO;
		goto pdata_error;
	}
	escore_priv.dev = dev;
	escore_priv.pdata = pdata;
	if (pdata->esxxx_clk_cb)
		pdata->esxxx_clk_cb(1);

	mutex_init(&escore_priv.api_mutex);
	mutex_init(&escore_priv.pm_mutex);

	escore_priv.pm_state = ES_POWER_STATE_NORMAL;
	INIT_DELAYED_WORK(&escore_priv.sleep_work, es325_delayed_sleep);

	/* Create sysfs entries */
	rc = sysfs_create_group(&escore_priv.dev->kobj, &core_sysfs);
	if (rc) {
		dev_err(escore_priv.dev,
			"failed to create core sysfs entries: %d\n", rc);
	}

	dev_dbg(escore_priv.dev, "%s(): reset_gpio = %d\n", __func__,
		pdata->reset_gpio);
	if (pdata->reset_gpio != -1) {
		rc = gpio_request(pdata->reset_gpio, "es325_reset");
		if (rc < 0) {
			dev_err(escore_priv.dev, "%s(): es325_reset request failed",
				__func__);
			goto reset_gpio_request_error;
		}
		rc = gpio_direction_output(pdata->reset_gpio, 1);
		if (rc < 0) {
			dev_err(escore_priv.dev, "%s(): es325_reset direction failed",
				__func__);
			goto reset_gpio_direction_error;
		}
		gpio_set_value(pdata->reset_gpio, 1);
	}
	else {
		dev_warn(escore_priv.dev, "%s(): es325_reset undefined\n",
			 __func__);
	}

	dev_dbg(escore_priv.dev, "%s(): wakeup_gpio = %d\n", __func__,
		pdata->wakeup_gpio);

	if (pdata->wakeup_gpio != -1) {
		rc = gpio_request(pdata->wakeup_gpio, "es325_wakeup");
		if (rc < 0) {
			dev_err(escore_priv.dev, "%s(): es325_wakeup request failed",
				__func__);
			goto wakeup_gpio_request_error;
		}
		rc = gpio_direction_output(pdata->wakeup_gpio, 1);
		if (rc < 0) {
			dev_err(escore_priv.dev, "%s(): es325_wakeup direction failed",
				__func__);
			goto wakeup_gpio_direction_error;
		}
	}
	else {
		dev_warn(escore_priv.dev, "%s(): es325_wakeup undefined\n",
			 __func__);
	}

	dev_dbg(escore_priv.dev, "%s(): gpioa_gpio = %d\n", __func__,
		pdata->gpioa_gpio);
	if (pdata->gpioa_gpio != -1) {
		rc = gpio_request(pdata->gpioa_gpio, "es325_gpioa");
		if (rc < 0) {
			dev_err(escore_priv.dev, "%s(): es325_gpioa request failed",
				__func__);
			goto gpioa_gpio_request_error;
		}
		rc = gpio_direction_input(pdata->gpioa_gpio);
		if (rc < 0) {
			dev_err(escore_priv.dev, "%s(): es325_gpioa direction failed",
				__func__);
			goto gpioa_gpio_direction_error;
		}
	}
	else {
		dev_warn(escore_priv.dev, "%s(): es325_gpioa undefined\n",
			 __func__);
	}

	dev_dbg(escore_priv.dev, "%s(): gpiob_gpio = %d\n", __func__,
		pdata->gpiob_gpio);

	if (pdata->gpiob_gpio != -1) {
		rc = gpio_request(pdata->gpiob_gpio, "es325_gpiob");
		if (rc < 0) {
			dev_err(escore_priv.dev, "%s(): es325_gpiob request failed",
				__func__);
			goto gpiob_gpio_request_error;
		}
		rc = gpio_direction_input(pdata->gpiob_gpio);
		if (rc < 0) {
			dev_err(escore_priv.dev, "%s(): es325_gpiob direction failed",
				__func__);
			goto gpiob_gpio_direction_error;
		}
	}
	else {
		dev_warn(escore_priv.dev, "%s(): es325_gpiob undefined\n",
			 __func__);
	}

	rc = request_firmware((const struct firmware **)&escore_priv.standard,
			      fw_filename, escore_priv.dev);
	if (rc) {
		dev_err(escore_priv.dev, "%s(): request_firmware(%s) failed %d\n",
			__func__, fw_filename, rc);
		goto request_firmware_error;
	}
	escore_priv.flag.is_codec = 0;
	escore_priv.flag.is_fw_ready = 0;
	escore_priv.boot_ops.bootup = es325_bootup;
	escore_priv.soc_codec_dev_escore = NULL;
	escore_priv.dai = es325_dai;
	escore_priv.dai_nr = ES_NUM_CODEC_DAIS;
	escore_priv.api_addr_max = ES_API_ADDR_MAX;
	escore_priv.api_access = es325_api_access;
	escore_priv.ap_tx1_ch_cnt = 2;
	if (escore_priv.pri_intf == ES_SLIM_INTF) {

		escore_priv.slim_rx = slim_rx;
		escore_priv.slim_tx = slim_tx;
		escore_priv.slim_dai_data = dai_data;

		escore_priv.slim_rx_ports = ES_SLIM_RX_PORTS;
		escore_priv.slim_tx_ports = ES_SLIM_TX_PORTS;
		escore_priv.codec_slim_dais = ES_NUM_CODEC_SLIM_DAIS;

		escore_priv.slim_tx_port_to_ch_map = es325_slim_tx_port_to_ch;
		escore_priv.slim_rx_port_to_ch_map = es325_slim_rx_port_to_ch;

#if defined(CONFIG_SND_SOC_ES_SLIM)
		/* Initialization of be_id goes here if required */
		escore_priv.slim_be_id = es325_slim_be_id;
#endif

		/* Initialization of _remote_ routines goes here if required */
		escore_priv.remote_route_enable = es325_remote_route_enable;
		escore_priv.slim_dai_ops.set_channel_map =
					es325_slim_set_channel_map;
#if defined(CONFIG_ARCH_MSM)
		escore_priv.slim_dai_ops.get_channel_map =
					es325_slim_get_channel_map;
#endif

		escore_priv.flag.local_slim_ch_cfg = 0;
		escore_priv.channel_dir = NULL;
	}

	return rc;

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
EXPORT_SYMBOL_GPL(es325_core_probe);

static __init int es325_init(void)
{
	int rc = 0;

	pr_info("%s()", __func__);

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
		pr_err("Error registering Audience eS325 driver: %d\n", rc);
		goto INIT_ERR;
	}

#if defined(CONFIG_SND_SOC_ES_CDEV)
	rc = escore_cdev_init(&escore_priv);
	if (rc) {
		pr_err("Error enabling CDEV interface: %d\n", rc);
		goto INIT_ERR;
	}
#endif

INIT_ERR:
	return rc;
}
module_init(es325_init);

static __exit void es325_exit(void)
{
	pr_info("%s()\n", __func__);

#if defined(CONFIG_SND_SOC_ES_I2C)
	i2c_del_driver(&escore_i2c_driver);
#endif
}
module_exit(es325_exit);


MODULE_DESCRIPTION("ASoC ES325 driver");
MODULE_AUTHOR("Greg Clemson <gclemson@audience.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:es325-codec");
MODULE_FIRMWARE("audience-es325-fw.bin");
