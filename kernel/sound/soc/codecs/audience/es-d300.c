/*
 * es-d300.c  --  Audience es D300 component ALSA Audio driver
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
#include <linux/wait.h>
#include "escore.h"
#include "es755.h"
#include "escore-list.h"
#include "escore-slim.h"
#include "es-d300.h"

static u8 **cachedcmd_list;
static int es_vp_tx;
static int es_vp_rx;
static int es_az_tx;
static int es_az_rx;

static const u8 pcm_port[] = { 0x0, 0xA, 0xB, 0xC };
static u8 chn_mgr_cache[MAX_CHMGR];

static void clear_chn_mgr_cache(void)
{
	memset(chn_mgr_cache, 0x0, sizeof(chn_mgr_cache));
}

static const char * const proc_block_input_texts[] = {
	"None",
	"PCM0.0", "PCM0.1", "PCM0.2", "PCM0.3",
	"PCM1.0", "PCM1.1", "PCM1.2", "PCM1.3",
	"PCM2.0", "PCM2.1", "PCM2.2", "PCM2.3",
	"PDMI0", "PDMI1", "PDMI2", "PDMI3",
	"SBUS.RX0", "SBUS.RX1", "SBUS.RX2", "SBUS.RX3",
	"SBUS.RX4", "SBUS.RX5", "SBUS.RX6", "SBUS.RX7",
	"SBUS.RX8", "SBUS.RX9",
	"ADC0", "ADC1", "ADC2", "ADC3",
};

static const u16 es300_input_mux_text_to_api[] = {
	0xffff, /* Default value for all input MUXes */
	ES300_DATA_PATH(PCM0, 0, 0), ES300_DATA_PATH(PCM0, 1, 0),
	ES300_DATA_PATH(PCM0, 2, 0), ES300_DATA_PATH(PCM0, 3, 0),
	ES300_DATA_PATH(PCM1, 0, 0), ES300_DATA_PATH(PCM1, 1, 0),
	ES300_DATA_PATH(PCM1, 2, 0), ES300_DATA_PATH(PCM1, 3, 0),
	ES300_DATA_PATH(PCM2, 0, 0), ES300_DATA_PATH(PCM2, 1, 0),
	ES300_DATA_PATH(PCM2, 2, 0), ES300_DATA_PATH(PCM2, 3, 0),
	ES300_DATA_PATH(PDMI0, 0, 0),
	ES300_DATA_PATH(PDMI1, 0, 0),
	ES300_DATA_PATH(PDMI2, 0, 0),
	ES300_DATA_PATH(PDMI3, 0, 0),
	ES300_DATA_PATH(SBUS, 0, 0), ES300_DATA_PATH(SBUS, 1, 0),
	ES300_DATA_PATH(SBUS, 2, 0), ES300_DATA_PATH(SBUS, 3, 0),
	ES300_DATA_PATH(SBUS, 4, 0), ES300_DATA_PATH(SBUS, 5, 0),
	ES300_DATA_PATH(SBUS, 6, 0), ES300_DATA_PATH(SBUS, 7, 0),
	ES300_DATA_PATH(SBUS, 8, 0), ES300_DATA_PATH(SBUS, 9, 0),
	ES300_DATA_PATH(ADC0, 0, 0),
	ES300_DATA_PATH(ADC1, 0, 0),
	ES300_DATA_PATH(ADC2, 0, 0),
	ES300_DATA_PATH(ADC3, 0, 0),
};

static const char * const proc_block_output_texts[] = {
	"None",
	"VP CSOUT", "VP FEOUT1", "VP FEOUT2",
	"VP MONOUT1", "VP MONOUT2",
	"AudioZoom CSOUT",
	"AudioZoom AOUT1",
	"MM AUDOUT1", "MM AUDOUT2", "MM PASSOUT1", "MM PASSOUT2",
	"MM MONOUT1", "MM MONOUT2",
	"Pass AUDOUT1", "Pass AUDOUT2", "Pass AUDOUT3", "Pass AUDOUT4",
	"MONOUT1", "MONOUT2", "MONOUT3", "MONOUT4",
};

static const u16 es300_output_mux_text_to_api[] = {
	0xffff, /* Default value for all output MUXes */

	/* VP outputs */
	ES300_PATH_ID(TXCHMGR2, ES300_CSOUT1),
	ES300_PATH_ID(TXCHMGR3, ES300_FEOUT1),
	ES300_PATH_ID(TXCHMGR4, ES300_FEOUT2),

	/* VP MONOUT */
	ES300_PATH_ID(TXCHMGR0, ES300_MM_MONOUT1),
	ES300_PATH_ID(TXCHMGR1, ES300_MM_MONOUT2),

	/* AudioZoom */
	ES300_PATH_ID(TXCHMGR0, ES300_CSOUT1),
	ES300_PATH_ID(TXCHMGR1, ES300_AUDOUT1),

	/* MM AUDOUT */
	ES300_PATH_ID(TXCHMGR0, ES300_AUDOUT1),
	ES300_PATH_ID(TXCHMGR1, ES300_AUDOUT2),

	/* MM PASSOUT */
	ES300_PATH_ID(TXCHMGR4, ES300_PASSOUT1),
	ES300_PATH_ID(TXCHMGR5, ES300_PASSOUT2),

	/* MM MONOUT */
	ES300_PATH_ID(TXCHMGR2, ES300_MM_MONOUT1),
	ES300_PATH_ID(TXCHMGR3, ES300_MM_MONOUT2),

	/* Passthru AUDOUT */
	ES300_PATH_ID(TXCHMGR0, ES300_PASSOUT1),
	ES300_PATH_ID(TXCHMGR1, ES300_PASSOUT2),
	ES300_PATH_ID(TXCHMGR2, ES300_PASSOUT3),
	ES300_PATH_ID(TXCHMGR3, ES300_PASSOUT4),

	/* UI Tone MONOUT */
	ES300_PATH_ID(TXCHMGR1, ES300_MONOUT1),
	ES300_PATH_ID(TXCHMGR2, ES300_MONOUT2),
	ES300_PATH_ID(0, ES300_MONOUT3),
	ES300_PATH_ID(0, ES300_MONOUT4),
};

static const u32 es_base_route_preset[ALGO_MAX] = {
	[VP] = 0x90311771,
	[VP_MM] = 0x90311777,
	[AudioZoom] = 0x90311774,
#if defined(CONFIG_SND_SOC_ES_SLIM)
	[MM] = 0x90311773,
	[Passthru] = 0x90311776,
#else
	[MM] = 0x90311772,
	[Passthru] = 0x90311775,
#endif

};

static const struct es_ch_mgr_max es_chn_mgr_max[ALGO_MAX] = {
	[VP] = {
		.rx = VP_RXCHMGR_MAX,
		.tx = VP_TXCHMGR_MAX,
	},
	[MM] = {
		.rx = MM_RXCHMGR_MAX,
		.tx = MM_TXCHMGR_MAX,
	},
	[VP_MM] = {
		.rx = VP_MM_RXCHMGR_MAX,
		.tx = VP_MM_TXCHMGR_MAX,
	},
};
static int es300_codec_stop_algo(struct escore_priv *escore)
{
	u32 cmd = ES_STOP_ALGORITHM<<16;
	u32 resp;
	int ret = 0;

	/* Stop algorithm */
	switch (escore->algo_type) {
	case VP:
	case MM:
	case VP_MM:
	case AudioZoom:
		cmd |= escore->algo_type;
		break;
	case Passthru:
	case Passthru_VP:
	case Passthru_MM:
	case Passthru_VP_MM:
	case Passthru_AZ:
		cmd |= ES_PASSTHRU + (escore->algo_type-Passthru);
		break;
	case Voicesense:
		cmd |= ES_VOICESENSE;
		break;
	}
	ret = escore_cmd(escore, cmd, &resp);
	if (ret)
		pr_err("%s: algo stop failed\n", __func__);

	clear_chn_mgr_cache();
	return ret;
}
/* Mask to keep track of chmgrs set by UCM */
static u16 chn_mgr_mask;

static int convert_input_mux_to_cmd(struct escore_priv *escore, int reg)
{
	unsigned int value;
	int msg_len = escore->api_access[reg].write_msg_len;
	u32 msg[ES_CMD_ACCESS_WR_MAX] = {0};
	u16 port, chnum;
	u16 ch_mgr;
	u8 path_id;
	u8 update_cmds = 0;
	u8 update_chmgr_mask = 1;
	int mux = cachedcmd_list[escore->algo_type][reg];

	memcpy((char *)msg, (char *)escore->api_access[reg].write_msg,
			msg_len);
	value = es300_input_mux_text_to_api[mux];

	pr_debug("%s(): reg = %d mux = %d\n", __func__, reg, mux);
	msg[0] |= value;
	ch_mgr = ES_CHMGR_DATAPATH(msg[0]);
	path_id = msg[1] & 0xFF;

	switch (escore->algo_type) {
	case AudioZoom:
		if (reg == ES_TERTIARY_MUX) {
			/* Connect RxChMgr2.o0 */
			msg[2] = ES_API_WORD(ES_CONNECT_CMD,
					ES300_ENDPOINT(Filter_RxChanMgr2, OUT,
						RxChMgr_o0));
			msg_len += sizeof(*msg);

			/* Connect AZ.i2 */
			msg[3] = ES_API_WORD(ES_CONNECT_CMD,
					ES300_ENDPOINT(Filter_AZ, IN, az_i2));
			msg_len += sizeof(*msg);

			/* Set Rate 48k */
			msg[4] = ES_API_WORD(ES_SET_RATE_CMD,
					ES300_RATE(Filter_RxChanMgr2, 3));
			msg_len += sizeof(*msg);

			/* Set Rate 48k */
			msg[5] = ES_API_WORD(ES_SET_GROUP_CMD,
					ES300_GROUP(Filter_RxChanMgr2, 0));
			msg_len += sizeof(*msg);

		} else if (reg == ES_AZ_AI1_MUX) {
			port = (value >> 9) & 0x1f;
			chnum = (value >> 4) & 0x1f;

			/* Update the data path */
			msg[0] = ES_API_WORD(ES_SET_MUX_CMD,
					ES300_DATA_PATH(port, chnum, RXCHMGR3));

			/* Set corresponding path id */
			msg[1] = ES_API_WORD(ES_SET_PATH_ID_CMD,
				ES300_PATH_ID(RXCHMGR3, path_id));

			msg_len = 8;
			chn_mgr_mask |= 1 << RXCHMGR3;
			update_chmgr_mask = 0;

		}
		break;

	case Passthru:
		break;

	case MM:
		update_cmds = 1;
		port = (value >> 9) & 0x1f;
		chnum = (value >> 4) & 0x1f;

		switch (reg) {
		case ES_MM_PASSIN1_MUX:
			ch_mgr = RXCHMGR4;
			break;
		case ES_MM_PASSIN2_MUX:
			ch_mgr = RXCHMGR5;
			break;
		default:
			update_cmds = 0;
			break;
		}

		if (update_cmds) {
			/* Update the data path */
			msg[0] = ES_API_WORD(ES_SET_MUX_CMD,
					ES300_DATA_PATH(port, chnum, ch_mgr));

			/* Set corresponding path id */
			msg[1] = ES_API_WORD(ES_SET_PATH_ID_CMD,
					ES300_PATH_ID(ch_mgr, path_id));

			msg_len = 8;
			chn_mgr_mask |= 1 << ch_mgr;
			update_chmgr_mask = 0;
		}
		break;

	case VP_MM:
		update_cmds = 1;
		port = (value >> 9) & 0x1f;
		chnum = (value >> 4) & 0x1f;

		switch (reg) {
		case ES_AUDIN1_MUX:
			ch_mgr = RXCHMGR8;
			break;
		case ES_AUDIN2_MUX:
			ch_mgr = RXCHMGR9;
			break;
		default:
			update_cmds = 0;
			break;
		}

		if (update_cmds) {
			/* Update the data path */
			msg[0] = ES_API_WORD(ES_SET_MUX_CMD,
					ES300_DATA_PATH(port, chnum, ch_mgr));

			/* Set corresponding path id */
			msg[1] = ES_API_WORD(ES_SET_PATH_ID_CMD,
					ES300_PATH_ID(ch_mgr, path_id));

			msg_len = 8;
			chn_mgr_mask |= 1 << ch_mgr;
			update_chmgr_mask = 0;
		}
		break;
	}
	if (update_chmgr_mask)
		chn_mgr_mask |= 1 << ch_mgr;

	return escore_queue_msg_to_list(escore, (char *)msg, msg_len);
}

static int convert_output_mux_to_cmd(struct escore_priv *escore, int reg)
{
	unsigned int value;
	int msg_len = escore->api_access[reg].write_msg_len;
	u32 msg[ES_CMD_ACCESS_WR_MAX] = {0};
	u16 ch_mgr;
	u8 path_id;
	u8 update_cmds = 1;
	u8 update_chmgr_mask = 1;
	u16 ep_out = 0, filter_in = 0;
	int mux = cachedcmd_list[escore->algo_type][reg];

	memcpy((char *)msg, (char *)escore->api_access[reg].write_msg,
				msg_len);
	value = es300_output_mux_text_to_api[mux];

	pr_debug("%s(): reg = %d mux = %d\n", __func__, reg, mux);

	path_id = value & 0x3f;
	ch_mgr = (value >> 8) & 0xf;

	msg[0] |= ES300_DATA_PATH(0, 0, ch_mgr);
	msg[1] |= ES300_PATH_ID(ch_mgr, path_id);

	switch (escore->algo_type) {
	case VP:
		break;
	case AudioZoom:
		break;
	case MM:
		break;
	case Passthru:
		switch (ch_mgr) {
		case TXCHMGR1:
			ep_out = pass_o1;
			filter_in = Filter_TxChanMgr1;
			break;
		case TXCHMGR2:
			ep_out = pass_o2;
			filter_in = Filter_TxChanMgr2;
			break;
		case TXCHMGR3:
			ep_out = pass_o3;
			filter_in = Filter_TxChanMgr3;
			break;
		default:
			update_cmds = 0;
			break;
		}

		if (update_cmds) {
			/* Set OUT Endpoint */
			msg[2] = ES_API_WORD(0xB064,
				ES300_ENDPOINT(Filter_Passthru, OUT, ep_out));
			msg_len += sizeof(*msg);

			/* Set IN Endpoint */
			msg[3] = ES_API_WORD(0xB064,
				ES300_ENDPOINT(filter_in, IN, TxChMgr_i0));
			msg_len += sizeof(*msg);

			/* Set TxChanMgr Rate 48k */
			msg[4] = ES_API_WORD(0xB063,
				ES300_RATE(filter_in, 3));
			msg_len += sizeof(*msg);

			/* Set Group ID */
			msg[5] = ES_API_WORD(0xB068,
				ES300_GROUP(filter_in, 0));
			msg_len += sizeof(*msg);
		}
		break;
	}

	if (update_chmgr_mask)
		chn_mgr_mask |= 1 << ch_mgr;

	return escore_queue_msg_to_list(escore, (char *)msg, msg_len);
}

static void set_chmgr_null(struct escore_priv *escore)
{
	u32 msg = ES_SET_MUX_NULL;
	int i;

	pr_debug("%s: mask %04x\n", __func__, chn_mgr_mask);

	/* Set RXCHMGR NULL */
	for (i = 0; i < es_chn_mgr_max[escore->algo_type].rx; i++) {
		if (chn_mgr_mask & (1 << i) || chn_mgr_cache[i]) {
			chn_mgr_cache[i] = 1;
			continue;
		}
		msg |= i;
		escore_queue_msg_to_list(escore, (char *)&msg, sizeof(msg));
		msg &= ES_SET_MUX_NULL_MASK;
	}

	/* Set TXCHMGR NULL */
	for (i = 0; i < es_chn_mgr_max[escore->algo_type].tx; i++) {
		if (chn_mgr_mask & (1 << (i + TXCHMGR0)) ||
				chn_mgr_cache[i + TXCHMGR0]) {
			chn_mgr_cache[i + TXCHMGR0] = 1;
			continue;
		}
		msg |= i + TXCHMGR0;
		escore_queue_msg_to_list(escore, (char *)&msg, sizeof(msg));
		msg &= ES_SET_MUX_NULL_MASK;
	}
}

static int add_algo_base_route(struct escore_priv *escore)
{
	const u32 *msg;
	u32 cmd = ES_SYNC_CMD << 16;
	u32 rate_msg;
	int rc;
	int algo = escore->algo_type;

	/* Set unused CHMGRs to NULL */
	set_chmgr_null(escore);

	msg = &es_base_route_preset[algo];
	rc = escore_queue_msg_to_list(escore, (char *)msg, sizeof(*msg));

	/* Configure command completion mode */
	if (escore->cmd_compl_mode == ES_CMD_COMP_INTR) {
		cmd |= ES_RISING_EDGE;
		escore_queue_msg_to_list(escore, (char *)&cmd,
				sizeof(cmd));
	}

	/* Set algo rate if specified */
	if (escore->algo_rate) {
		rate_msg = (ES_SET_RATE_CMD << 16) |
			((escore->algo_rate << 8) | escore->algo_type);
		rc = escore_queue_msg_to_list(escore,
			(char *)&rate_msg, sizeof(rate_msg));
	}
	return rc;
}

static int es_clear_route(struct escore_priv *escore)
{

	pr_debug("%s\n", __func__);

	if (escore->algo_type == VP) {
		es_vp_tx = ES_VP_None;
		es_vp_rx = ES_VP_None;
	} else if (escore->algo_type == AudioZoom) {
		es_az_tx = ES_AZ_None;
		es_az_rx = ES_AZ_None;
	}

	return 0;
}

static int es_set_final_route(struct escore_priv *escore)
{
	int rc = 0, i;

	pr_debug("%s\n", __func__);

	if ((escore->algo_type == VP) && !escore->VP_Asr) {
		if (es_vp_tx != ES_VP_TX_INIT ||
				es_vp_rx != ES_VP_RX_INIT) {
			pr_debug("%s VP not ready rx=%d tx=%d\n", __func__,
				es_vp_rx, es_vp_tx);
			return 0;
		}
	} else if (escore->algo_type == AudioZoom) {
		if (es_az_tx != ES_AZ_TX_INIT ||
				es_az_rx != ES_AZ_RX_INIT) {
			pr_debug("%s AZ not ready rx=%d tx=%d\n", __func__,
				es_az_rx, es_az_tx);
			return 0;
		}
	}

	escore_flush_msg_list(escore);

	for (i = ES_PRIMARY_MUX; i <= ES_PASSIN4_MUX; i++) {
		if (cachedcmd_list[escore->algo_type][i] != 0) {
			rc = convert_input_mux_to_cmd(escore, i);
			if (rc)
				return rc;
		}
	}
	for (i = ES_DAC0_0_MUX; i <= ES_SBUSTX5_MUX; i++) {
		if (cachedcmd_list[escore->algo_type][i] != 0) {
			rc = convert_output_mux_to_cmd(escore, i);
			if (rc)
				return rc;
		}
	}

	/* add umbrella base route */
	rc = add_algo_base_route(escore);
	if (!rc)
		escore_write_msg_list(escore);
	return rc;
}

#if defined(CONFIG_SND_SOC_ES_SLIM)
static int es300_codec_enable_slimtx(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct escore_priv *escore = snd_soc_codec_get_drvdata(codec);
	u32  j = 0;
	int  ret = 0;
	int dai_id = 0;

	pr_debug("%s: %s %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:

		for (j = 0; j < escore->dai_nr; j++) {
			if ((escore->dai[j].id == ES_SLIM_1_PB) ||
					(escore->dai[j].id == ES_SLIM_2_PB) ||
					(escore->dai[j].id == ES_SLIM_3_PB))
				continue;

			if (!strncmp(w->sname,
				escore->dai[j].capture.stream_name, 13)) {
				dai_id = escore->dai[j].id - 1;
				++escore->slim_dai_data[dai_id].ch_act;
				break;
			}
		}

		if (escore->slim_dai_data[dai_id].ch_act ==
				escore->slim_dai_data[dai_id].ch_tot) {

			ret = es_set_final_route(escore);
			if (!ret) {
				ret = escore_cfg_slim_tx(escore->gen0_client,
					escore->slim_dai_data[dai_id].ch_num,
					escore->slim_dai_data[dai_id].ch_tot,
					escore->slim_dai_data[dai_id].rate);
				atomic_inc(&escore->active_streams);
			}

		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		for (j = 0; j < escore->dai_nr; j++) {
			if ((escore->dai[j].id == ES_SLIM_1_PB) ||
					(escore->dai[j].id == ES_SLIM_2_PB) ||
					(escore->dai[j].id == ES_SLIM_3_PB))
				continue;
			if (!strncmp(w->sname,
				escore->dai[j].capture.stream_name, 13)) {
				dai_id = escore->dai[j].id - 1;
				--escore->slim_dai_data[dai_id].ch_act;
				break;
			}
		}

		if (!escore->slim_dai_data[dai_id].ch_act) {
			ret = escore_close_slim_tx(escore->gen0_client,
					escore->slim_dai_data[dai_id].ch_num,
					escore->slim_dai_data[dai_id].ch_tot);

			atomic_dec(&escore->active_streams);
			memset(escore->slim_dai_data[dai_id].ch_num, 0,
			(sizeof(u32)*escore->slim_dai_data[dai_id].ch_tot));

			escore->slim_dai_data[dai_id].ch_tot = 0;

			if (codec && codec->dev && codec->dev->parent) {
				pm_runtime_mark_last_busy(codec->dev->parent);
				pm_runtime_put(codec->dev->parent);
			}
			if (!ret)
				ret = es_clear_route(escore);

		}
		if (atomic_read(&escore->active_streams) == 0)
			ret = es300_codec_stop_algo(escore);
	}
	return ret;
}

static int es300_codec_enable_slimrx(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct escore_priv *escore = snd_soc_codec_get_drvdata(codec);
	u32  j = 0;
	int  ret = 0;
	int dai_id = 0;

	pr_debug(" ==== AIF RX ==== %s: %s %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		for (j = 0; j < escore->dai_nr; j++) {
			if ((escore->dai[j].id == ES_SLIM_1_CAP) ||
					(escore->dai[j].id == ES_SLIM_2_CAP) ||
					(escore->dai[j].id == ES_SLIM_3_CAP))
				continue;

			if (!strncmp(w->sname,
				escore->dai[j].playback.stream_name, 13)) {
				dai_id = escore->dai[j].id - 1;
				++escore->slim_dai_data[dai_id].ch_act;
				break;
			}
		}

		if (escore->slim_dai_data[dai_id].ch_act ==
				escore->slim_dai_data[dai_id].ch_tot) {

			ret = es_set_final_route(escore);
			if (!ret) {
				ret = escore_cfg_slim_rx(escore->gen0_client,
					escore->slim_dai_data[dai_id].ch_num,
					escore->slim_dai_data[dai_id].ch_tot,
					escore->slim_dai_data[dai_id].rate);
				atomic_inc(&escore->active_streams);
			}

		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		for (j = 0; j < escore->dai_nr; j++) {
			if ((escore->dai[j].id == ES_SLIM_1_CAP) ||
					(escore->dai[j].id == ES_SLIM_2_CAP) ||
					(escore->dai[j].id == ES_SLIM_3_CAP))
				continue;
			if (!strncmp(w->sname,
				escore->dai[j].playback.stream_name, 13)) {
				dai_id = escore->dai[j].id - 1;
				--escore->slim_dai_data[dai_id].ch_act;
				break;
			}
		}
		if (!escore->slim_dai_data[dai_id].ch_act) {
			ret = escore_close_slim_rx(escore->gen0_client,
					escore->slim_dai_data[dai_id].ch_num,
					escore->slim_dai_data[dai_id].ch_tot);

			atomic_dec(&escore->active_streams);
			memset(escore->slim_dai_data[dai_id].ch_num, 0,
			(sizeof(u32)*escore->slim_dai_data[dai_id].ch_tot));

			escore->slim_dai_data[dai_id].ch_tot = 0;

			if (codec && codec->dev && codec->dev->parent) {
				pm_runtime_mark_last_busy(codec->dev->parent);
				pm_runtime_put(codec->dev->parent);
			}
			if (!ret)
				ret = es_clear_route(escore);
		}
		if (atomic_read(&escore->active_streams) == 0)
			ret = es300_codec_stop_algo(escore);
	}
	return ret;
}
static int es300_codec_enable_i2srx(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	return 0;
}
static int es300_codec_enable_i2stx(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	return 0;
}
#else
static int es300_codec_enable_slimrx(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	return 0;
}
static int es300_codec_enable_slimtx(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	return 0;
}
static int es300_codec_enable_i2srx(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct escore_priv *escore = snd_soc_codec_get_drvdata(codec);
	int ret = 0;
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		ret = es_set_final_route(escore);
		atomic_inc(&escore->active_streams);
		break;
	case SND_SOC_DAPM_POST_PMD:
		ret = es_clear_route(escore);
		if (atomic_dec_and_test(&escore->active_streams))
			ret = es300_codec_stop_algo(escore);
		break;
	}
	return ret;
}
static int es300_codec_enable_i2stx(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct escore_priv *escore = snd_soc_codec_get_drvdata(codec);
	int ret = 0;
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		ret = es_set_final_route(escore);
		atomic_inc(&escore->active_streams);
		break;
	case SND_SOC_DAPM_POST_PMD:
		ret = es_clear_route(escore);
		if (atomic_dec_and_test(&escore->active_streams))
			ret = es300_codec_stop_algo(escore);
		break;
	}
	return ret;
}
#endif

static int stereo_widening;

static int get_stereo_widening(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = stereo_widening;

	return 0;
}

static int put_stereo_widening(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = escore_priv.codec;
	struct escore_priv *escore = snd_soc_codec_get_drvdata(codec);
	unsigned int value = ucontrol->value.enumerated.item[0];
	u32 cmd = ES_SET_PRESET<<16;
	u32 resp;
	int ret;

	if (value)
		cmd |= ES_STEREO_WIDE_ON;
	else
		cmd |= ES_STEREO_WIDE_OFF;

	ret = escore_cmd(escore, cmd, &resp);

	if (!ret)
		stereo_widening = value;

	return ret;
}

static int put_az_mode(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = escore_priv.codec;
	struct soc_enum *e =
		(struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	unsigned int value = ucontrol->value.enumerated.item[0];

	if (value > ES_MAX_AZ_MODE) {
		pr_err("%s(): Invalid value %d for the control\n", __func__,
				value);
		return -EINVAL;
	}

	pr_debug("%s: Audio Zoom mode value %u\n", __func__, value);

	return escore_write(codec, reg, value);
}

static int get_az_mode(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = escore_priv.codec;
	struct soc_enum *e =
		(struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	u16 value;

	value = escore_read(codec, reg);

	if (value > ES_MAX_AZ_MODE)
		pr_err("%s(): Invalid value %d\n", __func__, value);

	ucontrol->value.enumerated.item[0] = value;

	pr_debug("%s: Audio Zoom mode value %u\n", __func__, value);

	return 0;
}

static int put_stereo_width(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = escore_priv.codec;
	struct soc_enum *e =
		(struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	unsigned int value = ucontrol->value.enumerated.item[0];
	u16 stereo_width;

	if (!value)
		stereo_width = ES_DEF_ST_WIDE_SETTING;
	else
		stereo_width = (u16)ES_FULL_NARROWING + (value - 1);

	pr_debug("%s: stereo width %04x value %u\n", __func__,
		stereo_width, value);

	return escore_write(codec, reg, stereo_width);
}

static int get_stereo_width(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = escore_priv.codec;
	struct soc_enum *e =
		(struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	u16 value;
	u16 stereo_width;

	stereo_width = escore_read(codec, reg);
	if (stereo_width == ES_DEF_ST_WIDE_SETTING)
		value = 0;
	else
		value = stereo_width - (u16)ES_FULL_NARROWING + 1;

	ucontrol->value.enumerated.item[0] = value;

	pr_debug("%s: stereo width %04x value %u\n", __func__,
		stereo_width, value);

	return 0;
}

static int put_pcm_port_sel(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = escore_priv.codec;
	struct escore_priv *escore = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e =
		(struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	unsigned int value = ucontrol->value.enumerated.item[0];

	escore->pcm_port = value;
	cachedcmd_list[escore->algo_type][reg] = value;

	return 0;
}

static int get_cmd_compl_mode_sel(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = escore_priv.codec;
	struct escore_priv *escore = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = escore->cmd_compl_mode;

	return 0;
}

static int put_cmd_compl_mode_sel(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = escore_priv.codec;
	struct escore_priv *escore = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e =
		(struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	unsigned int value = ucontrol->value.enumerated.item[0];

	if (value > ES_CMD_COMP_INTR) {
		pr_err("%s(): Invalid command completion mode :%d\n",
				__func__, value);
		return -EINVAL;
	}

	escore->cmd_compl_mode = value;
	cachedcmd_list[escore->algo_type][reg] = value;

	return 0;
}


static int es300_put_algo(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = escore_priv.codec;
	struct escore_priv *escore = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e =
		(struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	int ret = 0;
	unsigned int value = ucontrol->value.enumerated.item[0];

	escore->algo_type = value;
	chn_mgr_mask = 0; /* reset mask */
	cachedcmd_list[escore->algo_type][reg] = value;

	return ret;
}
static int es300_put_algo_rate(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = escore_priv.codec;
	struct escore_priv *escore = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e =
		(struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	int ret = 0;
	unsigned int value = ucontrol->value.enumerated.item[0];
	escore->algo_rate = value;
	cachedcmd_list[escore->algo_type][reg] = value;
	return ret;
}
static int get_control_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = escore_priv.codec;
	struct escore_priv *escore = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e =
		(struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	unsigned int value;

	value = cachedcmd_list[escore->algo_type][reg];
	ucontrol->value.enumerated.item[0] = value;

	return 0;
}

static int put_input_route_value(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = escore_priv.codec;
	struct escore_priv *escore = snd_soc_codec_get_drvdata(codec);
	struct snd_soc_dapm_widget_list *wlist = snd_kcontrol_chip(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct soc_enum *e =
		(struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	int mux = ucontrol->value.enumerated.item[0];
	int rc;

#if defined(CONFIG_ARCH_OMAP) || defined(CONFIG_ARCH_EXYNOS) || \
	defined(CONFIG_X86_32)
	rc = snd_soc_dapm_mux_update_power(widget, kcontrol, mux, e);
#else
	rc = snd_soc_dapm_mux_update_power(widget, kcontrol, mux, e);
#endif

	cachedcmd_list[escore->algo_type][reg] = mux;
	pr_debug("put input reg %d val %d\n", reg, mux);

	return rc;
}

static int get_input_route_value(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = escore_priv.codec;
	struct escore_priv *escore = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e =
		(struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	unsigned int value;

	value = cachedcmd_list[escore->algo_type][reg];

	/* TBD: translation */
	/* value = escore_read(NULL, reg); */
	ucontrol->value.enumerated.item[0] = value;
	pr_debug("get input reg %d val %d\n", reg, value);

	return 0;
}

static int put_output_route_value(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = escore_priv.codec;
	struct escore_priv *escore = snd_soc_codec_get_drvdata(codec);
	struct snd_soc_dapm_widget_list *wlist = snd_kcontrol_chip(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct soc_enum *e =
		(struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	int rc;
	int mux = ucontrol->value.enumerated.item[0];

	/* VP CSOUT signals Tx init and VP FEOUT signals Rx init */
	if (strncmp(proc_block_output_texts[mux], "VP CSOUT", 8) == 0)
		es_vp_tx = ES_VP_TX_INIT;
	else if (strncmp(proc_block_output_texts[mux], "VP FEOUT1", 9) == 0)
		es_vp_rx = ES_VP_RX_INIT;
	else if (strncmp(proc_block_output_texts[mux],
				"AudioZoom CSOUT", 15) == 0)
		es_az_tx = ES_AZ_TX_INIT;
	else if (strncmp(proc_block_output_texts[mux],
				"AudioZoom AOUT1", 15) == 0)
		es_az_rx = ES_AZ_RX_INIT;


#if defined(CONFIG_ARCH_OMAP) || defined(CONFIG_ARCH_EXYNOS) || \
	defined(CONFIG_X86_32)
	rc = snd_soc_dapm_mux_update_power(widget, kcontrol, mux, e);
#else
	rc = snd_soc_dapm_mux_update_power(widget, kcontrol, mux, e);
#endif

	cachedcmd_list[escore->algo_type][reg] = mux;

	pr_debug("put output reg %d val %d\n", reg, mux);
	return rc;
}

static int get_output_route_value(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = escore_priv.codec;
	struct escore_priv *escore = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e =
		(struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	unsigned int value;

	value = cachedcmd_list[escore->algo_type][reg];

	/* TBD: translation */
	/* value = escore_read(NULL, reg); */
	ucontrol->value.enumerated.item[0] = value;
	pr_debug("get output reg %d val %d\n", reg, value);

	return 0;
}

static int get_write_route_cmds(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int put_write_route_cmds(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.enumerated.item[0])
		escore_write_msg_list(NULL);

	return 0;
}

static int put_asr_sel(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = escore_priv.codec;
	struct escore_priv *escore = snd_soc_codec_get_drvdata(codec);

	escore->VP_Asr = ucontrol->value.enumerated.item[0];

	return 0;
}

static int get_flush_route_cmds(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int get_asr_sel(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = escore_priv.codec;
	struct escore_priv *escore = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = escore->VP_Asr;
	return 0;
}

static int put_flush_route_cmds(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.enumerated.item[0])
		escore_flush_msg_list(NULL);

	return 0;
}

static int get_pcm_port_param(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = escore_priv.codec;
	struct escore_priv *escore = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value = ucontrol->value.enumerated.item[0];
	struct escore_api_access *api_access;
	int msg_len;
	u32 msg[2] = { 0 };
	u32 resp;
	u8 pcm_port_id;
	int rc;

	if (!escore->pcm_port) {
		pr_err("%s(): PCM Port not selected\n", __func__);
		return -EINVAL;
	}

	if (reg > escore->api_addr_max) {
		pr_err("%s(): invalid address = 0x%04x\n", __func__, reg);
		return -EINVAL;
	}

	api_access = &escore->api_access[reg];
	pcm_port_id = pcm_port[escore->pcm_port];

	/* Update the Port info in read command */
	api_access->read_msg[0] |= ES_API_WORD(ES_GET_DEV_PARAM,
			pcm_port_id << 8);

	rc = escore_prepare_msg(escore, reg, value, (char *)msg, &msg_len,
			ES_MSG_READ);
	if (rc) {
		pr_err("%s(): Preparing write message failed\n", __func__);
		return rc;
	}
	/* Clear the Port info in read command */
	api_access->read_msg[0] &= ES_API_WORD(ES_GET_DEV_PARAM, 0x00ff);

	rc = escore_cmd(escore, msg[0], &resp);
	if (rc < 0) {
		pr_err("%s(): escore_cmd()", __func__);
		return rc;
	}
	msg[0] = escore->bus.last_response;

	value = msg[0] & 0xffff;
	ucontrol->value.enumerated.item[0] = value;
	pr_debug("get input reg %d value 0x%08x", reg, value);

	return 0;
}

static int put_pcm_port_param(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = escore_priv.codec;
	struct escore_priv *escore = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value = ucontrol->value.enumerated.item[0];
	struct escore_api_access *api_access;
	u8 pcm_port_id;
	int rc;

	if (!escore->pcm_port) {
		pr_err("%s(): PCM Port not selected\n", __func__);
		return -EINVAL;
	}

	if (reg > escore->api_addr_max) {
		pr_err("%s(): invalid address = 0x%04x\n", __func__, reg);
		return -EINVAL;
	}

	api_access = &escore->api_access[reg];
	pcm_port_id = pcm_port[escore->pcm_port];

	/* Update the Port info in write command */
	api_access->write_msg[0] |=  ES_API_WORD(ES_SET_DEV_PARAM_ID,
			(pcm_port_id << 8));

	value = ucontrol->value.enumerated.item[0];
	rc = escore_write(codec, reg, value);
	if (rc) {
		pr_err("%s(): Preparing write message failed\n", __func__);
		return rc;
	}

	/* Clear the Port info in write command */
	api_access->write_msg[0] &= ES_API_WORD(ES_SET_DEV_PARAM_ID, 0x00ff);

	return 0;
}

/* controls */
static const char * const algorithm_texts[] = {
	"None", "VP", "MM", "VP_MM", "AudioZoom", "Pass",
	"Pass_VP", "Pass_MM", "Pass_VP_MM", "Pass_AZ",
	"Voicesense"
};

static const char * const cmd_compl_mode_sel_texts[] = {
	"Polling", "Interrupt",
};

static const struct soc_enum cmd_compl_mode_sel_enum =
	SOC_ENUM_SINGLE(ES_CMD_COMPL_MODE, 0,
			ARRAY_SIZE(cmd_compl_mode_sel_texts),
			cmd_compl_mode_sel_texts);

static const char * const pcm_port_sel_texts[] = {
	"None", "Port A", "Port B", "Port C",
};

static const struct soc_enum pcm_port_sel_enum =
	SOC_ENUM_SINGLE(ES_PCM_PORT, 0,
			ARRAY_SIZE(pcm_port_sel_texts),
			pcm_port_sel_texts);

static const struct soc_enum algorithm_enum =
	SOC_ENUM_SINGLE(ES_ALGORITHM, 0,
			ARRAY_SIZE(algorithm_texts),
			algorithm_texts);

static const char * const es755_algo_rates_text[] = {
	"fs=8khz", "fs=16khz", "fs=24khz", "fs=48khz", "fs=96khz", "fs=192khz",
};
/*replaced for ES_ALGORITHM_RATE*/
static const struct soc_enum algorithm_rate_enum =
	SOC_ENUM_SINGLE(ES_ALGO_SAMPLE_RATE, 0,
			ARRAY_SIZE(es755_algo_rates_text),
			es755_algo_rates_text);

static const char * const stereo_widening_texts[] = {
	"OFF", "ON"
};

static const struct soc_enum stereo_widening_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0,
			ARRAY_SIZE(stereo_widening_texts),
			stereo_widening_texts);

static const char * const stereo_width_texts[] = {
	"Default", "Full narrowing", "90% narrowing", "80% narrowing",
	"70% narrowing", "60% narrowing", "50% narrowing", "40% narrowing",
	"30% narrowing", "20% narrowing", "10% narrowing", "No effect",
	"10% widening", "20% widening", "30% widening", "40% widening",
	"50% widening",	"60% widening", "70% widening", "80% widening",
	"90% widening", "Full widening"
};

static const struct soc_enum stereo_width_enum =
	SOC_ENUM_SINGLE(ES_STEREO_WIDTH, 0,
			ARRAY_SIZE(stereo_width_texts),
			stereo_width_texts);

static const char * const es755_audio_zoom_texts[] = {
	"Narrator", "Scene", "Narration"
};

static const struct soc_enum es755_az_mode_enum =
	SOC_ENUM_SINGLE(ES_AZ_MODE, 0, ARRAY_SIZE(es755_audio_zoom_texts),
			es755_audio_zoom_texts);

static const struct snd_kcontrol_new es_d300_snd_controls[] = {
	SOC_ENUM_EXT("Algorithm", algorithm_enum,
			 get_control_enum, es300_put_algo),
	SOC_ENUM_EXT("Algorithm Rate", algorithm_rate_enum,
			 get_control_enum, es300_put_algo_rate),
	SOC_SINGLE_EXT("ASR", SND_SOC_NOPM, 0, 1, 0,
			 get_asr_sel, put_asr_sel),
	SOC_SINGLE_EXT("Write Route Cmds", SND_SOC_NOPM, 0, 1, 0,
		       get_write_route_cmds, put_write_route_cmds),
	SOC_SINGLE_EXT("Flush Route Cmds", SND_SOC_NOPM, 0, 1, 0,
		       get_flush_route_cmds, put_flush_route_cmds),
	SOC_ENUM_EXT("Stereo Widening Effect", stereo_widening_enum,
			get_stereo_widening, put_stereo_widening),
	SOC_ENUM_EXT("Audio Zoom", es755_az_mode_enum,
			get_az_mode, put_az_mode),
	SOC_ENUM_EXT("Stereo Width", stereo_width_enum,
			get_stereo_width, put_stereo_width),
	SOC_ENUM_EXT("PCM Port Selection", pcm_port_sel_enum,
			get_control_enum, put_pcm_port_sel),
	SOC_ENUM_EXT("Command Mode Selection", cmd_compl_mode_sel_enum,
			get_cmd_compl_mode_sel, put_cmd_compl_mode_sel),
	SOC_SINGLE_EXT("PCM Port Word Length", ES_PORT_WORD_LEN, 0, 65535, 0,
			get_pcm_port_param, put_pcm_port_param),
	SOC_SINGLE_EXT("PCM Port TDM Slots Per Frame",
			ES_PORT_TDM_SLOTS_PER_FRAME, 0,	65535, 0,
			get_pcm_port_param, put_pcm_port_param),
	SOC_SINGLE_EXT("PCM Port TX Delay From FS",
			ES_PORT_TX_DELAY_FROM_FS, 0, 65535, 0,
			get_pcm_port_param, put_pcm_port_param),
	SOC_SINGLE_EXT("PCM Port Rx Delay From FS",
			ES_PORT_RX_DELAY_FROM_FS, 0, 65535, 0,
			get_pcm_port_param, put_pcm_port_param),
	SOC_SINGLE_EXT("PCM Port Latch Edge",
			ES_PORT_LATCH_EDGE, 0, 65535, 0,
			get_pcm_port_param, put_pcm_port_param),
	SOC_SINGLE_EXT("PCM Port Endian", ES_PORT_ENDIAN, 0, 65535, 0,
			get_pcm_port_param, put_pcm_port_param),
	SOC_SINGLE_EXT("PCM Port Tristate", ES_PORT_TRISTATE, 0, 65535, 0,
			get_pcm_port_param, put_pcm_port_param),
	SOC_SINGLE_EXT("PCM Port Audio Mode", ES_PORT_AUDIO_MODE, 0, 65535, 0,
			get_pcm_port_param, put_pcm_port_param),
	SOC_SINGLE_EXT("PCM Port TDM Enabled", ES_PORT_TDM_ENABLED, 0,
			65535, 0, get_pcm_port_param, put_pcm_port_param),
	SOC_SINGLE_EXT("PCM Port Clock Control", ES_PORT_CLOCK_CONTROL, 0,
			65535, 0, get_pcm_port_param, put_pcm_port_param),
	SOC_SINGLE_EXT("PCM Port Data Justification",
			ES_PORT_DATA_JUSTIFICATION, 0, 65535, 0,
			get_pcm_port_param, put_pcm_port_param),
	SOC_SINGLE_EXT("PCM Port FS Duration", ES_PORT_FS_DURATION, 0,
			65535, 0, get_pcm_port_param, put_pcm_port_param),

};

static const struct soc_enum vp_pri_enum =
	SOC_ENUM_SINGLE(ES_PRIMARY_MUX, 0,
		     ARRAY_SIZE(proc_block_input_texts),
		     proc_block_input_texts);
static const struct snd_kcontrol_new dapm_vp_pri_control =
	SOC_DAPM_ENUM_EXT("VP Primary MUX Mux", vp_pri_enum,
			  get_input_route_value, put_input_route_value);

static const struct soc_enum vp_sec_enum =
	SOC_ENUM_SINGLE(ES_SECONDARY_MUX, 0,
		     ARRAY_SIZE(proc_block_input_texts),
		     proc_block_input_texts);
static const struct snd_kcontrol_new dapm_vp_sec_control =
	SOC_DAPM_ENUM_EXT("VP Secondary MUX Mux", vp_sec_enum,
			  get_input_route_value, put_input_route_value);

static const struct soc_enum vp_ter_enum =
	SOC_ENUM_SINGLE(ES_TERTIARY_MUX, 0,
		     ARRAY_SIZE(proc_block_input_texts),
		     proc_block_input_texts);
static const struct snd_kcontrol_new dapm_vp_ter_control =
	SOC_DAPM_ENUM_EXT("VP Teritary MUX Mux", vp_ter_enum,
			  get_input_route_value, put_input_route_value);

static const struct soc_enum vp_fein_enum =
	SOC_ENUM_SINGLE(ES_FEIN_MUX, 0,
		     ARRAY_SIZE(proc_block_input_texts),
		     proc_block_input_texts);
static const struct snd_kcontrol_new dapm_vp_fein_control =
	SOC_DAPM_ENUM_EXT("VP FEIN MUX Mux", vp_fein_enum,
			  get_input_route_value, put_input_route_value);

static const struct soc_enum vp_uitone1_enum =
	SOC_ENUM_SINGLE(ES_UITONE1_MUX, 0,
		     ARRAY_SIZE(proc_block_input_texts),
		     proc_block_input_texts);
static const struct snd_kcontrol_new dapm_vp_uitone1_control =
	SOC_DAPM_ENUM_EXT("VP UITONE1 MUX Mux", vp_uitone1_enum,
			  get_input_route_value, put_input_route_value);

static const struct soc_enum vp_uitone2_enum =
	SOC_ENUM_SINGLE(ES_UITONE2_MUX, 0,
		     ARRAY_SIZE(proc_block_input_texts),
		     proc_block_input_texts);
static const struct snd_kcontrol_new dapm_vp_uitone2_control =
	SOC_DAPM_ENUM_EXT("VP UITONE2 MUX Mux", vp_uitone2_enum,
			  get_input_route_value, put_input_route_value);

static const struct soc_enum vp_uitone3_enum =
	SOC_ENUM_SINGLE(ES_UITONE3_MUX, 0,
		     ARRAY_SIZE(proc_block_input_texts),
		     proc_block_input_texts);
static const struct snd_kcontrol_new dapm_vp_uitone3_control =
	SOC_DAPM_ENUM_EXT("VP UITONE3 MUX Mux", vp_uitone3_enum,
			  get_input_route_value, put_input_route_value);

static const struct soc_enum vp_uitone4_enum =
	SOC_ENUM_SINGLE(ES_UITONE4_MUX, 0,
		     ARRAY_SIZE(proc_block_input_texts),
		     proc_block_input_texts);
static const struct snd_kcontrol_new dapm_vp_uitone4_control =
	SOC_DAPM_ENUM_EXT("VP UITONE4 MUX Mux", vp_uitone4_enum,
			  get_input_route_value, put_input_route_value);

static const struct soc_enum mm_passin1_enum =
	SOC_ENUM_SINGLE(ES_MM_PASSIN1_MUX, 0,
		     ARRAY_SIZE(proc_block_input_texts),
		     proc_block_input_texts);
static const struct snd_kcontrol_new dapm_mm_passin1_control =
	SOC_DAPM_ENUM_EXT("MM PASSIN1 MUX Mux", mm_passin1_enum,
			  get_input_route_value, put_input_route_value);

static const struct soc_enum mm_passin2_enum =
	SOC_ENUM_SINGLE(ES_MM_PASSIN2_MUX, 0,
		     ARRAY_SIZE(proc_block_input_texts),
		     proc_block_input_texts);
static const struct snd_kcontrol_new dapm_mm_passin2_control =
	SOC_DAPM_ENUM_EXT("MM PASSIN2 MUX Mux", mm_passin2_enum,
			  get_input_route_value, put_input_route_value);

static const struct soc_enum mm_uitone1_enum =
	SOC_ENUM_SINGLE(ES_MMUITONE1_MUX, 0,
		     ARRAY_SIZE(proc_block_input_texts),
		     proc_block_input_texts);
static const struct snd_kcontrol_new dapm_mm_uitone1_control =
	SOC_DAPM_ENUM_EXT("MM UITONE1 MUX Mux", mm_uitone1_enum,
			  get_input_route_value, put_input_route_value);

static const struct soc_enum mm_uitone2_enum =
	SOC_ENUM_SINGLE(ES_MMUITONE2_MUX, 0,
		     ARRAY_SIZE(proc_block_input_texts),
		     proc_block_input_texts);
static const struct snd_kcontrol_new dapm_mm_uitone2_control =
	SOC_DAPM_ENUM_EXT("MM UITONE2 MUX Mux", mm_uitone2_enum,
			  get_input_route_value, put_input_route_value);

static const struct soc_enum mm_audin1_enum =
	SOC_ENUM_SINGLE(ES_AUDIN1_MUX, 0,
		     ARRAY_SIZE(proc_block_input_texts),
		     proc_block_input_texts);
static const struct snd_kcontrol_new dapm_mm_audin1_control =
	SOC_DAPM_ENUM_EXT("MM AUDIN1 MUX Mux", mm_audin1_enum,
			  get_input_route_value, put_input_route_value);

static const struct soc_enum mm_audin2_enum =
	SOC_ENUM_SINGLE(ES_AUDIN2_MUX, 0,
		     ARRAY_SIZE(proc_block_input_texts),
		     proc_block_input_texts);
static const struct snd_kcontrol_new dapm_mm_audin2_control =
	SOC_DAPM_ENUM_EXT("MM AUDIN2 MUX Mux", mm_audin2_enum,
			  get_input_route_value, put_input_route_value);

static const struct soc_enum pass_audin1_enum =
	SOC_ENUM_SINGLE(ES_PASSIN1_MUX, 0,
		     ARRAY_SIZE(proc_block_input_texts),
		     proc_block_input_texts);
static const struct snd_kcontrol_new dapm_pass_audin1_control =
	SOC_DAPM_ENUM_EXT("Pass AUDIN1 MUX Mux", pass_audin1_enum,
			  get_input_route_value, put_input_route_value);

static const struct soc_enum pass_audin2_enum =
	SOC_ENUM_SINGLE(ES_PASSIN2_MUX, 0,
		     ARRAY_SIZE(proc_block_input_texts),
		     proc_block_input_texts);
static const struct snd_kcontrol_new dapm_pass_audin2_control =
	SOC_DAPM_ENUM_EXT("Pass AUDIN2 MUX Mux", pass_audin2_enum,
			  get_input_route_value, put_input_route_value);

static const struct soc_enum pass_audin3_enum =
	SOC_ENUM_SINGLE(ES_PASSIN3_MUX, 0,
		     ARRAY_SIZE(proc_block_input_texts),
		     proc_block_input_texts);
static const struct snd_kcontrol_new dapm_pass_audin3_control =
	SOC_DAPM_ENUM_EXT("Pass AUDIN3 MUX Mux", pass_audin3_enum,
			  get_input_route_value, put_input_route_value);

static const struct soc_enum pass_audin4_enum =
	SOC_ENUM_SINGLE(ES_PASSIN4_MUX, 0,
		     ARRAY_SIZE(proc_block_input_texts),
		     proc_block_input_texts);
static const struct snd_kcontrol_new dapm_pass_audin4_control =
	SOC_DAPM_ENUM_EXT("Pass AUDIN4 MUX Mux", pass_audin4_enum,
			  get_input_route_value, put_input_route_value);
static const struct soc_enum az_pri_enum =
	SOC_ENUM_SINGLE(ES_AZ_PRI_MUX, 0,
		     ARRAY_SIZE(proc_block_input_texts),
		     proc_block_input_texts);
static const struct snd_kcontrol_new dapm_az_pri_control =
	SOC_DAPM_ENUM_EXT("AudioZoom Primary MUX Mux", az_pri_enum,
			  get_input_route_value, put_input_route_value);

static const struct soc_enum az_sec_enum =
	SOC_ENUM_SINGLE(ES_AZ_SEC_MUX, 0,
		     ARRAY_SIZE(proc_block_input_texts),
		     proc_block_input_texts);
static const struct snd_kcontrol_new dapm_az_sec_control =
	SOC_DAPM_ENUM_EXT("AudioZoom Secondary MUX Mux", az_sec_enum,
			  get_input_route_value, put_input_route_value);

static const struct soc_enum az_ter_enum =
	SOC_ENUM_SINGLE(ES_AZ_TER_MUX, 0,
		     ARRAY_SIZE(proc_block_input_texts),
		     proc_block_input_texts);
static const struct snd_kcontrol_new dapm_az_ter_control =
	SOC_DAPM_ENUM_EXT("AudioZoom Tertiary MUX Mux", az_ter_enum,
			  get_input_route_value, put_input_route_value);

static const struct soc_enum az_ai1_enum =
	SOC_ENUM_SINGLE(ES_AZ_AI1_MUX, 0,
		     ARRAY_SIZE(proc_block_input_texts),
		     proc_block_input_texts);
static const struct snd_kcontrol_new dapm_az_ai1_control =
	SOC_DAPM_ENUM_EXT("AudioZoom AI1 MUX Mux", az_ai1_enum,
			  get_input_route_value, put_input_route_value);

static const struct soc_enum pcm0_0_enum =
	SOC_ENUM_SINGLE(ES_PCM0_0_MUX, 0,
		     ARRAY_SIZE(proc_block_output_texts),
		     proc_block_output_texts);
static const struct snd_kcontrol_new dapm_pcm0_0_control =
	SOC_DAPM_ENUM_EXT("PCM0.0 MUX Mux", pcm0_0_enum,
			  get_output_route_value, put_output_route_value);

static const struct soc_enum pcm0_1_enum =
	SOC_ENUM_SINGLE(ES_PCM0_1_MUX, 0,
		     ARRAY_SIZE(proc_block_output_texts),
		     proc_block_output_texts);
static const struct snd_kcontrol_new dapm_pcm0_1_control =
	SOC_DAPM_ENUM_EXT("PCM0.1 MUX Mux", pcm0_1_enum,
			  get_output_route_value, put_output_route_value);

static const struct soc_enum pcm0_2_enum =
	SOC_ENUM_SINGLE(ES_PCM0_2_MUX, 0,
		     ARRAY_SIZE(proc_block_output_texts),
		     proc_block_output_texts);
static const struct snd_kcontrol_new dapm_pcm0_2_control =
	SOC_DAPM_ENUM_EXT("PCM0.2 MUX Mux", pcm0_2_enum,
			  get_output_route_value, put_output_route_value);

static const struct soc_enum pcm0_3_enum =
	SOC_ENUM_SINGLE(ES_PCM0_3_MUX, 0,
		     ARRAY_SIZE(proc_block_output_texts),
		     proc_block_output_texts);
static const struct snd_kcontrol_new dapm_pcm0_3_control =
	SOC_DAPM_ENUM_EXT("PCM0.3 MUX Mux", pcm0_3_enum,
			  get_output_route_value, put_output_route_value);

static const struct soc_enum pcm1_0_enum =
	SOC_ENUM_SINGLE(ES_PCM1_0_MUX, 0,
		     ARRAY_SIZE(proc_block_output_texts),
		     proc_block_output_texts);
static const struct snd_kcontrol_new dapm_pcm1_0_control =
	SOC_DAPM_ENUM_EXT("PCM1.0 MUX Mux", pcm1_0_enum,
			  get_output_route_value, put_output_route_value);

static const struct soc_enum pcm1_1_enum =
	SOC_ENUM_SINGLE(ES_PCM1_1_MUX, 0,
		     ARRAY_SIZE(proc_block_output_texts),
		     proc_block_output_texts);
static const struct snd_kcontrol_new dapm_pcm1_1_control =
	SOC_DAPM_ENUM_EXT("PCM1.1 MUX Mux", pcm1_1_enum,
			  get_output_route_value, put_output_route_value);

static const struct soc_enum pcm1_2_enum =
	SOC_ENUM_SINGLE(ES_PCM1_2_MUX, 0,
		     ARRAY_SIZE(proc_block_output_texts),
		     proc_block_output_texts);
static const struct snd_kcontrol_new dapm_pcm1_2_control =
	SOC_DAPM_ENUM_EXT("PCM1.2 MUX Mux", pcm1_2_enum,
			  get_output_route_value, put_output_route_value);

static const struct soc_enum pcm1_3_enum =
	SOC_ENUM_SINGLE(ES_PCM1_3_MUX, 0,
		     ARRAY_SIZE(proc_block_output_texts),
		     proc_block_output_texts);
static const struct snd_kcontrol_new dapm_pcm1_3_control =
	SOC_DAPM_ENUM_EXT("PCM1.3 MUX Mux", pcm1_3_enum,
			  get_output_route_value, put_output_route_value);

static const struct soc_enum pcm2_0_enum =
	SOC_ENUM_SINGLE(ES_PCM2_0_MUX, 0,
		     ARRAY_SIZE(proc_block_output_texts),
		     proc_block_output_texts);
static const struct snd_kcontrol_new dapm_pcm2_0_control =
	SOC_DAPM_ENUM_EXT("PCM2.0 MUX Mux", pcm2_0_enum,
			  get_output_route_value, put_output_route_value);

static const struct soc_enum pcm2_1_enum =
	SOC_ENUM_SINGLE(ES_PCM2_1_MUX, 0,
		     ARRAY_SIZE(proc_block_output_texts),
		     proc_block_output_texts);
static const struct snd_kcontrol_new dapm_pcm2_1_control =
	SOC_DAPM_ENUM_EXT("PCM2.1 MUX Mux", pcm2_1_enum,
			  get_output_route_value, put_output_route_value);

static const struct soc_enum pcm2_2_enum =
	SOC_ENUM_SINGLE(ES_PCM2_2_MUX, 0,
		     ARRAY_SIZE(proc_block_output_texts),
		     proc_block_output_texts);
static const struct snd_kcontrol_new dapm_pcm2_2_control =
	SOC_DAPM_ENUM_EXT("PCM2.2 MUX Mux", pcm2_2_enum,
			  get_output_route_value, put_output_route_value);

static const struct soc_enum pcm2_3_enum =
	SOC_ENUM_SINGLE(ES_PCM2_3_MUX, 0,
		     ARRAY_SIZE(proc_block_output_texts),
		     proc_block_output_texts);
static const struct snd_kcontrol_new dapm_pcm2_3_control =
	SOC_DAPM_ENUM_EXT("PCM2.3 MUX Mux", pcm2_3_enum,
			  get_output_route_value, put_output_route_value);

static const struct soc_enum sbustx0_enum =
	SOC_ENUM_SINGLE(ES_SBUSTX0_MUX, 0,
		     ARRAY_SIZE(proc_block_output_texts),
		     proc_block_output_texts);
static const struct snd_kcontrol_new dapm_sbustx0_control =
	SOC_DAPM_ENUM_EXT("SBUS.TX0 MUX Mux", sbustx0_enum,
			  get_output_route_value, put_output_route_value);

static const struct soc_enum sbustx1_enum =
	SOC_ENUM_SINGLE(ES_SBUSTX1_MUX, 0,
		     ARRAY_SIZE(proc_block_output_texts),
		     proc_block_output_texts);
static const struct snd_kcontrol_new dapm_sbustx1_control =
	SOC_DAPM_ENUM_EXT("SBUS.TX1 MUX Mux", sbustx1_enum,
			  get_output_route_value, put_output_route_value);

static const struct soc_enum sbustx2_enum =
	SOC_ENUM_SINGLE(ES_SBUSTX2_MUX, 0,
		     ARRAY_SIZE(proc_block_output_texts),
		     proc_block_output_texts);
static const struct snd_kcontrol_new dapm_sbustx2_control =
	SOC_DAPM_ENUM_EXT("SBUS.TX2 MUX Mux", sbustx2_enum,
			  get_output_route_value, put_output_route_value);

static const struct soc_enum sbustx3_enum =
	SOC_ENUM_SINGLE(ES_SBUSTX3_MUX, 0,
		     ARRAY_SIZE(proc_block_output_texts),
		     proc_block_output_texts);
static const struct snd_kcontrol_new dapm_sbustx3_control =
	SOC_DAPM_ENUM_EXT("SBUS.TX3 MUX Mux", sbustx3_enum,
			  get_output_route_value, put_output_route_value);

static const struct soc_enum sbustx4_enum =
	SOC_ENUM_SINGLE(ES_SBUSTX4_MUX, 0,
		     ARRAY_SIZE(proc_block_output_texts),
		     proc_block_output_texts);
static const struct snd_kcontrol_new dapm_sbustx4_control =
	SOC_DAPM_ENUM_EXT("SBUS.TX4 MUX Mux", sbustx4_enum,
			  get_output_route_value, put_output_route_value);

static const struct soc_enum sbustx5_enum =
	SOC_ENUM_SINGLE(ES_SBUSTX5_MUX, 0,
		     ARRAY_SIZE(proc_block_output_texts),
		     proc_block_output_texts);
static const struct snd_kcontrol_new dapm_sbustx5_control =
	SOC_DAPM_ENUM_EXT("SBUS.TX5 MUX Mux", sbustx5_enum,
			  get_output_route_value, put_output_route_value);

static const struct soc_enum dac0_0_enum =
	SOC_ENUM_SINGLE(ES_DAC0_0_MUX, 0,
		     ARRAY_SIZE(proc_block_output_texts),
		     proc_block_output_texts);
static const struct snd_kcontrol_new dapm_dac0_0_control =
	SOC_DAPM_ENUM_EXT("DAC0.0 MUX Mux", dac0_0_enum,
			  get_output_route_value, put_output_route_value);

static const struct soc_enum dac0_1_enum =
	SOC_ENUM_SINGLE(ES_DAC0_1_MUX, 0,
		     ARRAY_SIZE(proc_block_output_texts),
		     proc_block_output_texts);

static const struct snd_kcontrol_new dapm_dac0_1_control =
	SOC_DAPM_ENUM_EXT("DAC0.1 MUX Mux", dac0_1_enum,
			  get_output_route_value, put_output_route_value);

static const struct soc_enum dac1_0_enum =
	SOC_ENUM_SINGLE(ES_DAC1_0_MUX, 0,
		     ARRAY_SIZE(proc_block_output_texts),
		     proc_block_output_texts);
static const struct snd_kcontrol_new dapm_dac1_0_control =
	SOC_DAPM_ENUM_EXT("DAC1.0 MUX Mux", dac1_0_enum,
			  get_output_route_value, put_output_route_value);

static const struct soc_enum dac1_1_enum =
	SOC_ENUM_SINGLE(ES_DAC1_1_MUX, 0,
		     ARRAY_SIZE(proc_block_output_texts),
		     proc_block_output_texts);
static const struct snd_kcontrol_new dapm_dac1_1_control =
	SOC_DAPM_ENUM_EXT("DAC1.1 MUX Mux", dac1_1_enum,
			  get_output_route_value, put_output_route_value);

static const struct snd_soc_dapm_widget es_d300_dapm_widgets[] = {

	/* AIF */
	SND_SOC_DAPM_AIF_IN_E("PCM0 RX", "PORTA Playback", 0,
			SND_SOC_NOPM, 1, 0, es300_codec_enable_i2srx,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_IN_E("PCM1 RX", "PORTB Playback", 0,
			SND_SOC_NOPM, 1, 0, es300_codec_enable_i2srx,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_IN_E("PCM2 RX", "PORTC Playback", 0,
			SND_SOC_NOPM, 1, 0, es300_codec_enable_i2srx,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_AIF_OUT_E("PCM0 TX", "PORTA Capture", 0,
			SND_SOC_NOPM, 1, 0, es300_codec_enable_i2stx,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_OUT_E("PCM1 TX", "PORTB Capture", 0,
			SND_SOC_NOPM, 1, 0, es300_codec_enable_i2stx,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_OUT_E("PCM2 TX", "PORTC Capture", 0,
			SND_SOC_NOPM, 1, 0, es300_codec_enable_i2stx,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_AIF_IN_E("SBUS.RX0", "SLIM_PORT-1 Playback", 0,
			SND_SOC_NOPM, 1, 0, es300_codec_enable_slimrx,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_IN_E("SBUS.RX1", "SLIM_PORT-1 Playback", 0,
			SND_SOC_NOPM, 1, 0, es300_codec_enable_slimrx,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_IN_E("SBUS.RX2", "SLIM_PORT-2 Playback", 0,
			SND_SOC_NOPM, 1, 0, es300_codec_enable_slimrx,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_IN_E("SBUS.RX3", "SLIM_PORT-2 Playback", 0,
			SND_SOC_NOPM, 1, 0, es300_codec_enable_slimrx,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_IN_E("SBUS.RX4", "SLIM_PORT-3 Playback", 0,
			SND_SOC_NOPM, 1, 0, es300_codec_enable_slimrx,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_IN_E("SBUS.RX5", "SLIM_PORT-3 Playback", 0,
			SND_SOC_NOPM, 1, 0, es300_codec_enable_slimrx,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_AIF_IN("SBUS.RX6", "", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("SBUS.RX7", "", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("SBUS.RX8", "", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("SBUS.RX9", "", 0, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_AIF_OUT_E("SBUS.TX0", "SLIM_PORT-1 Capture", 0,
			SND_SOC_NOPM, 1, 0, es300_codec_enable_slimtx,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_OUT_E("SBUS.TX1", "SLIM_PORT-1 Capture", 0,
			SND_SOC_NOPM, 1, 0, es300_codec_enable_slimtx,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_OUT_E("SBUS.TX2", "SLIM_PORT-2 Capture", 0,
			SND_SOC_NOPM, 1, 0, es300_codec_enable_slimtx,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_OUT_E("SBUS.TX3", "SLIM_PORT-2 Capture", 0,
			SND_SOC_NOPM, 1, 0, es300_codec_enable_slimtx,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_OUT_E("SBUS.TX4", "SLIM_PORT-3 Capture", 0,
			SND_SOC_NOPM, 1, 0, es300_codec_enable_slimtx,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_OUT_E("SBUS.TX5", "SLIM_PORT-3 Capture", 0,
			SND_SOC_NOPM, 1, 0, es300_codec_enable_slimtx,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	/* voice processing */
	SND_SOC_DAPM_MUX("VP Primary MUX", SND_SOC_NOPM, 0, 0,
			&dapm_vp_pri_control),
	SND_SOC_DAPM_MUX("VP Secondary MUX", SND_SOC_NOPM, 0, 0,
			&dapm_vp_sec_control),
	SND_SOC_DAPM_MUX("VP Teritary MUX", SND_SOC_NOPM, 0, 0,
			&dapm_vp_ter_control),
	SND_SOC_DAPM_MUX("VP FEIN MUX", SND_SOC_NOPM, 0, 0,
			&dapm_vp_fein_control),
	SND_SOC_DAPM_MUX("VP UITONE1 MUX", SND_SOC_NOPM, 0, 0,
			&dapm_vp_uitone1_control),
	SND_SOC_DAPM_MUX("VP UITONE2 MUX", SND_SOC_NOPM, 0, 0,
			&dapm_vp_uitone2_control),

	/* Passthrough */
	SND_SOC_DAPM_MUX("Pass AUDIN1 MUX", SND_SOC_NOPM, 0, 0,
			&dapm_pass_audin1_control),
	SND_SOC_DAPM_MUX("Pass AUDIN2 MUX", SND_SOC_NOPM, 0, 0,
			&dapm_pass_audin2_control),
	SND_SOC_DAPM_MUX("Pass AUDIN3 MUX", SND_SOC_NOPM, 0, 0,
			&dapm_pass_audin3_control),
	SND_SOC_DAPM_MUX("Pass AUDIN4 MUX", SND_SOC_NOPM, 0, 0,
			&dapm_pass_audin4_control),

	/* Multimedia */
	SND_SOC_DAPM_MUX("MM AUDIN1 MUX", SND_SOC_NOPM, 0, 0,
			&dapm_mm_audin1_control),
	SND_SOC_DAPM_MUX("MM AUDIN2 MUX", SND_SOC_NOPM, 0, 0,
			&dapm_mm_audin2_control),
	SND_SOC_DAPM_MUX("MM UITONE1 MUX", SND_SOC_NOPM, 0, 0,
			&dapm_mm_uitone1_control),
	SND_SOC_DAPM_MUX("MM UITONE2 MUX", SND_SOC_NOPM, 0, 0,
			&dapm_mm_uitone2_control),
	SND_SOC_DAPM_MUX("MM PASSIN1 MUX", SND_SOC_NOPM, 0, 0,
			&dapm_mm_passin1_control),
	SND_SOC_DAPM_MUX("MM PASSIN2 MUX", SND_SOC_NOPM, 0, 0,
			&dapm_mm_passin2_control),


	/* AudioZoom */
	SND_SOC_DAPM_MUX("AudioZoom Primary MUX", SND_SOC_NOPM, 0, 0,
			&dapm_az_pri_control),
	SND_SOC_DAPM_MUX("AudioZoom Secondary MUX", SND_SOC_NOPM, 0, 0,
			&dapm_az_sec_control),
	SND_SOC_DAPM_MUX("AudioZoom Teritary MUX", SND_SOC_NOPM, 0, 0,
			&dapm_az_ter_control),
	SND_SOC_DAPM_MUX("AudioZoom AI1 MUX", SND_SOC_NOPM, 0, 0,
			&dapm_az_ai1_control),

	SND_SOC_DAPM_MUX("PCM0.0 MUX", SND_SOC_NOPM, 0, 0,
			 &dapm_pcm0_0_control),
	SND_SOC_DAPM_MUX("PCM0.1 MUX", SND_SOC_NOPM, 0, 0,
			 &dapm_pcm0_1_control),
	SND_SOC_DAPM_MUX("PCM0.2 MUX", SND_SOC_NOPM, 0, 0,
			 &dapm_pcm0_2_control),
	SND_SOC_DAPM_MUX("PCM0.3 MUX", SND_SOC_NOPM, 0, 0,
			 &dapm_pcm0_3_control),

	SND_SOC_DAPM_MUX("PCM1.0 MUX", SND_SOC_NOPM, 0, 0,
			 &dapm_pcm1_0_control),
	SND_SOC_DAPM_MUX("PCM1.1 MUX", SND_SOC_NOPM, 0, 0,
			 &dapm_pcm1_1_control),
	SND_SOC_DAPM_MUX("PCM1.2 MUX", SND_SOC_NOPM, 0, 0,
			 &dapm_pcm1_2_control),
	SND_SOC_DAPM_MUX("PCM1.3 MUX", SND_SOC_NOPM, 0, 0,
			 &dapm_pcm1_3_control),

	SND_SOC_DAPM_MUX("PCM2.0 MUX", SND_SOC_NOPM, 0, 0,
			 &dapm_pcm2_0_control),
	SND_SOC_DAPM_MUX("PCM2.1 MUX", SND_SOC_NOPM, 0, 0,
			 &dapm_pcm2_1_control),
	SND_SOC_DAPM_MUX("PCM2.2 MUX", SND_SOC_NOPM, 0, 0,
			 &dapm_pcm2_2_control),
	SND_SOC_DAPM_MUX("PCM2.3 MUX", SND_SOC_NOPM, 0, 0,
			 &dapm_pcm2_3_control),

	SND_SOC_DAPM_MUX("SBUS.TX0 MUX", SND_SOC_NOPM, 0, 0,
			 &dapm_sbustx0_control),
	SND_SOC_DAPM_MUX("SBUS.TX1 MUX", SND_SOC_NOPM, 0, 0,
			 &dapm_sbustx1_control),
	SND_SOC_DAPM_MUX("SBUS.TX2 MUX", SND_SOC_NOPM, 0, 0,
			 &dapm_sbustx2_control),
	SND_SOC_DAPM_MUX("SBUS.TX3 MUX", SND_SOC_NOPM, 0, 0,
			 &dapm_sbustx3_control),
	SND_SOC_DAPM_MUX("SBUS.TX4 MUX", SND_SOC_NOPM, 0, 0,
			 &dapm_sbustx4_control),
	SND_SOC_DAPM_MUX("SBUS.TX5 MUX", SND_SOC_NOPM, 0, 0,
			 &dapm_sbustx5_control),

	SND_SOC_DAPM_MUX("DAC0.0 MUX", SND_SOC_NOPM, 0, 0,
			 &dapm_dac0_0_control),
	SND_SOC_DAPM_MUX("DAC0.1 MUX", SND_SOC_NOPM, 0, 0,
			 &dapm_dac0_1_control),
	SND_SOC_DAPM_MUX("DAC1.0 MUX", SND_SOC_NOPM, 0, 0,
			 &dapm_dac1_0_control),
	SND_SOC_DAPM_MUX("DAC1.1 MUX", SND_SOC_NOPM, 0, 0,
			&dapm_dac1_1_control),

	SND_SOC_DAPM_MIXER("VP CSOUT Mixer", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("VP FEOUT1 Mixer", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("VP FEOUT2 Mixer", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("VP MONOUT1 Mixer", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("VP MONOUT2 Mixer", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("MM AUDOUT1 Mixer", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("MM AUDOUT2 Mixer", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("MM PASSOUT1 Mixer", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("MM PASSOUT2 Mixer", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("MM MONOUT1 Mixer", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("MM MONOUT2 Mixer", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("Pass AUDOUT1 Mixer", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("Pass AUDOUT2 Mixer", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("Pass AUDOUT3 Mixer", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("Pass AUDOUT4 Mixer", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("AudioZoom CSOUT Mixer", SND_SOC_NOPM,
			0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("AudioZoom AOUT1 Mixer", SND_SOC_NOPM,
			0, 0, NULL, 0),

	SND_SOC_DAPM_OUTPUT("PCM PORT A"),
	SND_SOC_DAPM_OUTPUT("PCM PORT B"),
	SND_SOC_DAPM_OUTPUT("PCM PORT C"),

	SND_SOC_DAPM_MIC("PDMI0", NULL),
	SND_SOC_DAPM_MIC("PDMI1", NULL),
	SND_SOC_DAPM_MIC("PDMI2", NULL),
	SND_SOC_DAPM_MIC("PDMI3", NULL),
};

static const struct snd_soc_dapm_route intercon[] = {

	{"VP Primary MUX", "PCM0.0", "PCM0 RX"},
	{"VP Primary MUX", "PCM0.1", "PCM0 RX"},
	{"VP Primary MUX", "PCM0.2", "PCM0 RX"},
	{"VP Primary MUX", "PCM0.3", "PCM0 RX"},
	{"VP Primary MUX", "PCM1.0", "PCM1 RX"},
	{"VP Primary MUX", "PCM1.1", "PCM1 RX"},
	{"VP Primary MUX", "PCM1.2", "PCM1 RX"},
	{"VP Primary MUX", "PCM1.3", "PCM1 RX"},
	{"VP Primary MUX", "PCM2.0", "PCM2 RX"},
	{"VP Primary MUX", "PCM2.1", "PCM2 RX"},
	{"VP Primary MUX", "PCM2.2", "PCM2 RX"},
	{"VP Primary MUX", "PCM2.3", "PCM2 RX"},
	{"VP Primary MUX", "PDMI0", "PDMI0"},
	{"VP Primary MUX", "PDMI1", "PDMI1"},
	{"VP Primary MUX", "PDMI2", "PDMI2"},
	{"VP Primary MUX", "PDMI3", "PDMI3"},
	{"VP Primary MUX", "SBUS.RX0", "SBUS.RX0"},
	{"VP Primary MUX", "SBUS.RX1", "SBUS.RX1"},
	{"VP Primary MUX", "SBUS.RX2", "SBUS.RX2"},
	{"VP Primary MUX", "SBUS.RX3", "SBUS.RX3"},
	{"VP Primary MUX", "SBUS.RX4", "SBUS.RX4"},
	{"VP Primary MUX", "SBUS.RX5", "SBUS.RX5"},
	{"VP Primary MUX", "SBUS.RX6", "SBUS.RX6"},
	{"VP Primary MUX", "SBUS.RX7", "SBUS.RX7"},
	{"VP Primary MUX", "SBUS.RX8", "SBUS.RX8"},
	{"VP Primary MUX", "SBUS.RX9", "SBUS.RX9"},
	{"VP Primary MUX", "ADC0", "ADC0"},
	{"VP Primary MUX", "ADC1", "ADC1"},
	{"VP Primary MUX", "ADC2", "ADC2"},
	{"VP Primary MUX", "ADC3", "ADC3"},

	{"VP Secondary MUX", "PCM0.0", "PCM0 RX"},
	{"VP Secondary MUX", "PCM0.1", "PCM0 RX"},
	{"VP Secondary MUX", "PCM0.2", "PCM0 RX"},
	{"VP Secondary MUX", "PCM0.3", "PCM0 RX"},
	{"VP Secondary MUX", "PCM1.0", "PCM1 RX"},
	{"VP Secondary MUX", "PCM1.1", "PCM1 RX"},
	{"VP Secondary MUX", "PCM1.2", "PCM1 RX"},
	{"VP Secondary MUX", "PCM1.3", "PCM1 RX"},
	{"VP Secondary MUX", "PCM2.0", "PCM2 RX"},
	{"VP Secondary MUX", "PCM2.1", "PCM2 RX"},
	{"VP Secondary MUX", "PCM2.2", "PCM2 RX"},
	{"VP Secondary MUX", "PCM2.3", "PCM2 RX"},
	{"VP Secondary MUX", "PDMI0", "PDMI0"},
	{"VP Secondary MUX", "PDMI1", "PDMI1"},
	{"VP Secondary MUX", "PDMI2", "PDMI2"},
	{"VP Secondary MUX", "PDMI3", "PDMI3"},
	{"VP Secondary MUX", "SBUS.RX0", "SBUS.RX0"},
	{"VP Secondary MUX", "SBUS.RX1", "SBUS.RX1"},
	{"VP Secondary MUX", "SBUS.RX2", "SBUS.RX2"},
	{"VP Secondary MUX", "SBUS.RX3", "SBUS.RX3"},
	{"VP Secondary MUX", "SBUS.RX4", "SBUS.RX4"},
	{"VP Secondary MUX", "SBUS.RX5", "SBUS.RX5"},
	{"VP Secondary MUX", "SBUS.RX6", "SBUS.RX6"},
	{"VP Secondary MUX", "SBUS.RX7", "SBUS.RX7"},
	{"VP Secondary MUX", "SBUS.RX8", "SBUS.RX8"},
	{"VP Secondary MUX", "SBUS.RX9", "SBUS.RX9"},
	{"VP Secondary MUX", "ADC0", "ADC0"},
	{"VP Secondary MUX", "ADC1", "ADC1"},
	{"VP Secondary MUX", "ADC2", "ADC2"},
	{"VP Secondary MUX", "ADC3", "ADC3"},

	{"VP Teritary MUX", "PCM0.0", "PCM0 RX"},
	{"VP Teritary MUX", "PCM0.1", "PCM0 RX"},
	{"VP Teritary MUX", "PCM0.2", "PCM0 RX"},
	{"VP Teritary MUX", "PCM0.3", "PCM0 RX"},
	{"VP Teritary MUX", "PCM1.0", "PCM1 RX"},
	{"VP Teritary MUX", "PCM1.1", "PCM1 RX"},
	{"VP Teritary MUX", "PCM1.2", "PCM1 RX"},
	{"VP Teritary MUX", "PCM1.3", "PCM1 RX"},
	{"VP Teritary MUX", "PCM2.0", "PCM2 RX"},
	{"VP Teritary MUX", "PCM2.1", "PCM2 RX"},
	{"VP Teritary MUX", "PCM2.2", "PCM2 RX"},
	{"VP Teritary MUX", "PCM2.3", "PCM2 RX"},
	{"VP Teritary MUX", "PDMI0", "PDMI0"},
	{"VP Teritary MUX", "PDMI1", "PDMI1"},
	{"VP Teritary MUX", "PDMI2", "PDMI2"},
	{"VP Teritary MUX", "PDMI3", "PDMI3"},
	{"VP Teritary MUX", "SBUS.RX0", "SBUS.RX0"},
	{"VP Teritary MUX", "SBUS.RX1", "SBUS.RX1"},
	{"VP Teritary MUX", "SBUS.RX2", "SBUS.RX2"},
	{"VP Teritary MUX", "SBUS.RX3", "SBUS.RX3"},
	{"VP Teritary MUX", "SBUS.RX4", "SBUS.RX4"},
	{"VP Teritary MUX", "SBUS.RX5", "SBUS.RX5"},
	{"VP Teritary MUX", "SBUS.RX6", "SBUS.RX6"},
	{"VP Teritary MUX", "SBUS.RX7", "SBUS.RX7"},
	{"VP Teritary MUX", "SBUS.RX8", "SBUS.RX8"},
	{"VP Teritary MUX", "SBUS.RX9", "SBUS.RX9"},
	{"VP Teritary MUX", "ADC0", "ADC0"},
	{"VP Teritary MUX", "ADC1", "ADC1"},
	{"VP Teritary MUX", "ADC2", "ADC2"},
	{"VP Teritary MUX", "ADC3", "ADC3"},

	{"VP FEIN MUX", "PCM0.0", "PCM0 RX"},
	{"VP FEIN MUX", "PCM0.1", "PCM0 RX"},
	{"VP FEIN MUX", "PCM0.2", "PCM0 RX"},
	{"VP FEIN MUX", "PCM0.3", "PCM0 RX"},
	{"VP FEIN MUX", "PCM1.0", "PCM1 RX"},
	{"VP FEIN MUX", "PCM1.1", "PCM1 RX"},
	{"VP FEIN MUX", "PCM1.2", "PCM1 RX"},
	{"VP FEIN MUX", "PCM1.3", "PCM1 RX"},
	{"VP FEIN MUX", "PCM2.0", "PCM2 RX"},
	{"VP FEIN MUX", "PCM2.1", "PCM2 RX"},
	{"VP FEIN MUX", "PCM2.2", "PCM2 RX"},
	{"VP FEIN MUX", "PCM2.3", "PCM2 RX"},
	{"VP FEIN MUX", "SBUS.RX0", "SBUS.RX0"},
	{"VP FEIN MUX", "SBUS.RX1", "SBUS.RX1"},
	{"VP FEIN MUX", "SBUS.RX2", "SBUS.RX2"},
	{"VP FEIN MUX", "SBUS.RX3", "SBUS.RX3"},
	{"VP FEIN MUX", "SBUS.RX4", "SBUS.RX4"},
	{"VP FEIN MUX", "SBUS.RX5", "SBUS.RX5"},
	{"VP FEIN MUX", "SBUS.RX6", "SBUS.RX6"},
	{"VP FEIN MUX", "SBUS.RX7", "SBUS.RX7"},
	{"VP FEIN MUX", "SBUS.RX8", "SBUS.RX8"},
	{"VP FEIN MUX", "SBUS.RX9", "SBUS.RX9"},
	{"VP FEIN MUX", "ADC0", "ADC0"},
	{"VP FEIN MUX", "ADC1", "ADC1"},
	{"VP FEIN MUX", "ADC2", "ADC2"},
	{"VP FEIN MUX", "ADC3", "ADC3"},

	{"VP UITONE1 MUX", "PCM0.0", "PCM0 RX"},
	{"VP UITONE1 MUX", "PCM0.1", "PCM0 RX"},
	{"VP UITONE1 MUX", "PCM0.2", "PCM0 RX"},
	{"VP UITONE1 MUX", "PCM0.3", "PCM0 RX"},
	{"VP UITONE1 MUX", "PCM1.0", "PCM1 RX"},
	{"VP UITONE1 MUX", "PCM1.1", "PCM1 RX"},
	{"VP UITONE1 MUX", "PCM1.2", "PCM1 RX"},
	{"VP UITONE1 MUX", "PCM1.3", "PCM1 RX"},
	{"VP UITONE1 MUX", "PCM2.0", "PCM2 RX"},
	{"VP UITONE1 MUX", "PCM2.1", "PCM2 RX"},
	{"VP UITONE1 MUX", "PCM2.2", "PCM2 RX"},
	{"VP UITONE1 MUX", "PCM2.3", "PCM2 RX"},
	{"VP UITONE1 MUX", "SBUS.RX0", "SBUS.RX0"},
	{"VP UITONE1 MUX", "SBUS.RX1", "SBUS.RX1"},
	{"VP UITONE1 MUX", "SBUS.RX2", "SBUS.RX2"},
	{"VP UITONE1 MUX", "SBUS.RX3", "SBUS.RX3"},
	{"VP UITONE1 MUX", "SBUS.RX4", "SBUS.RX4"},
	{"VP UITONE1 MUX", "SBUS.RX5", "SBUS.RX5"},
	{"VP UITONE1 MUX", "SBUS.RX6", "SBUS.RX6"},
	{"VP UITONE1 MUX", "SBUS.RX7", "SBUS.RX7"},
	{"VP UITONE1 MUX", "SBUS.RX8", "SBUS.RX8"},
	{"VP UITONE1 MUX", "SBUS.RX9", "SBUS.RX9"},
	{"VP UITONE1 MUX", "ADC0", "ADC0"},
	{"VP UITONE1 MUX", "ADC1", "ADC1"},
	{"VP UITONE1 MUX", "ADC2", "ADC2"},
	{"VP UITONE1 MUX", "ADC3", "ADC3"},

	{"VP UITONE2 MUX", "PCM0.0", "PCM0 RX"},
	{"VP UITONE2 MUX", "PCM0.1", "PCM0 RX"},
	{"VP UITONE2 MUX", "PCM0.2", "PCM0 RX"},
	{"VP UITONE2 MUX", "PCM0.3", "PCM0 RX"},
	{"VP UITONE2 MUX", "PCM1.0", "PCM1 RX"},
	{"VP UITONE2 MUX", "PCM1.1", "PCM1 RX"},
	{"VP UITONE2 MUX", "PCM1.2", "PCM1 RX"},
	{"VP UITONE2 MUX", "PCM1.3", "PCM1 RX"},
	{"VP UITONE2 MUX", "PCM2.0", "PCM2 RX"},
	{"VP UITONE2 MUX", "PCM2.1", "PCM2 RX"},
	{"VP UITONE2 MUX", "PCM2.2", "PCM2 RX"},
	{"VP UITONE2 MUX", "PCM2.3", "PCM2 RX"},
	{"VP UITONE2 MUX", "SBUS.RX0", "SBUS.RX0"},
	{"VP UITONE2 MUX", "SBUS.RX1", "SBUS.RX1"},
	{"VP UITONE2 MUX", "SBUS.RX2", "SBUS.RX2"},
	{"VP UITONE2 MUX", "SBUS.RX3", "SBUS.RX3"},
	{"VP UITONE2 MUX", "SBUS.RX4", "SBUS.RX4"},
	{"VP UITONE2 MUX", "SBUS.RX5", "SBUS.RX5"},
	{"VP UITONE2 MUX", "SBUS.RX6", "SBUS.RX6"},
	{"VP UITONE2 MUX", "SBUS.RX7", "SBUS.RX7"},
	{"VP UITONE2 MUX", "SBUS.RX8", "SBUS.RX8"},
	{"VP UITONE2 MUX", "SBUS.RX9", "SBUS.RX9"},
	{"VP UITONE2 MUX", "ADC0", "ADC0"},
	{"VP UITONE2 MUX", "ADC1", "ADC1"},
	{"VP UITONE2 MUX", "ADC2", "ADC2"},
	{"VP UITONE2 MUX", "ADC3", "ADC3"},

	{"VP CSOUT Mixer", NULL, "VP Primary MUX"},
	{"VP CSOUT Mixer", NULL, "VP Secondary MUX"},
	{"VP CSOUT Mixer", NULL, "VP Teritary MUX"},
	{"VP FEOUT1 Mixer", NULL, "VP FEIN MUX"},
	{"VP FEOUT2 Mixer", NULL, "VP FEIN MUX"},
	{"VP MONOUT1 Mixer", NULL, "VP UITONE1 MUX"},
	{"VP MONOUT2 Mixer", NULL, "VP UITONE2 MUX"},

	{"MM AUDIN1 MUX", "PCM0.0", "PCM0 RX"},
	{"MM AUDIN1 MUX", "PCM0.1", "PCM0 RX"},
	{"MM AUDIN1 MUX", "PCM0.2", "PCM0 RX"},
	{"MM AUDIN1 MUX", "PCM0.3", "PCM0 RX"},
	{"MM AUDIN1 MUX", "PCM1.0", "PCM1 RX"},
	{"MM AUDIN1 MUX", "PCM1.1", "PCM1 RX"},
	{"MM AUDIN1 MUX", "PCM1.2", "PCM1 RX"},
	{"MM AUDIN1 MUX", "PCM1.3", "PCM1 RX"},
	{"MM AUDIN1 MUX", "PCM2.0", "PCM2 RX"},
	{"MM AUDIN1 MUX", "PCM2.1", "PCM2 RX"},
	{"MM AUDIN1 MUX", "PCM2.2", "PCM2 RX"},
	{"MM AUDIN1 MUX", "PCM2.3", "PCM2 RX"},
	{"MM AUDIN1 MUX", "PDMI0", "PDMI0"},
	{"MM AUDIN1 MUX", "PDMI1", "PDMI1"},
	{"MM AUDIN1 MUX", "PDMI2", "PDMI2"},
	{"MM AUDIN1 MUX", "PDMI3", "PDMI3"},
	{"MM AUDIN1 MUX", "SBUS.RX0", "SBUS.RX0"},
	{"MM AUDIN1 MUX", "SBUS.RX1", "SBUS.RX1"},
	{"MM AUDIN1 MUX", "SBUS.RX2", "SBUS.RX2"},
	{"MM AUDIN1 MUX", "SBUS.RX3", "SBUS.RX3"},
	{"MM AUDIN1 MUX", "SBUS.RX4", "SBUS.RX4"},
	{"MM AUDIN1 MUX", "SBUS.RX5", "SBUS.RX5"},
	{"MM AUDIN1 MUX", "SBUS.RX6", "SBUS.RX6"},
	{"MM AUDIN1 MUX", "SBUS.RX7", "SBUS.RX7"},
	{"MM AUDIN1 MUX", "SBUS.RX8", "SBUS.RX8"},
	{"MM AUDIN1 MUX", "SBUS.RX9", "SBUS.RX9"},
	{"MM AUDIN1 MUX", "ADC0", "ADC0"},
	{"MM AUDIN1 MUX", "ADC1", "ADC1"},
	{"MM AUDIN1 MUX", "ADC2", "ADC2"},
	{"MM AUDIN1 MUX", "ADC3", "ADC3"},

	{"MM AUDIN2 MUX", "PCM0.0", "PCM0 RX"},
	{"MM AUDIN2 MUX", "PCM0.1", "PCM0 RX"},
	{"MM AUDIN2 MUX", "PCM0.2", "PCM0 RX"},
	{"MM AUDIN2 MUX", "PCM0.3", "PCM0 RX"},
	{"MM AUDIN2 MUX", "PCM1.0", "PCM1 RX"},
	{"MM AUDIN2 MUX", "PCM1.1", "PCM1 RX"},
	{"MM AUDIN2 MUX", "PCM1.2", "PCM1 RX"},
	{"MM AUDIN2 MUX", "PCM1.3", "PCM1 RX"},
	{"MM AUDIN2 MUX", "PCM2.0", "PCM2 RX"},
	{"MM AUDIN2 MUX", "PCM2.1", "PCM2 RX"},
	{"MM AUDIN2 MUX", "PCM2.2", "PCM2 RX"},
	{"MM AUDIN2 MUX", "PCM2.3", "PCM2 RX"},
	{"MM AUDIN2 MUX", "PDMI0", "PDMI0"},
	{"MM AUDIN2 MUX", "PDMI1", "PDMI1"},
	{"MM AUDIN2 MUX", "PDMI2", "PDMI2"},
	{"MM AUDIN2 MUX", "PDMI3", "PDMI3"},
	{"MM AUDIN2 MUX", "SBUS.RX0", "SBUS.RX0"},
	{"MM AUDIN2 MUX", "SBUS.RX1", "SBUS.RX1"},
	{"MM AUDIN2 MUX", "SBUS.RX2", "SBUS.RX2"},
	{"MM AUDIN2 MUX", "SBUS.RX3", "SBUS.RX3"},
	{"MM AUDIN2 MUX", "SBUS.RX4", "SBUS.RX4"},
	{"MM AUDIN2 MUX", "SBUS.RX5", "SBUS.RX5"},
	{"MM AUDIN2 MUX", "SBUS.RX6", "SBUS.RX6"},
	{"MM AUDIN2 MUX", "SBUS.RX7", "SBUS.RX7"},
	{"MM AUDIN2 MUX", "SBUS.RX8", "SBUS.RX8"},
	{"MM AUDIN2 MUX", "SBUS.RX9", "SBUS.RX9"},
	{"MM AUDIN2 MUX", "ADC0", "ADC0"},
	{"MM AUDIN2 MUX", "ADC1", "ADC1"},
	{"MM AUDIN2 MUX", "ADC2", "ADC2"},
	{"MM AUDIN2 MUX", "ADC3", "ADC3"},

	{"MM PASSIN1 MUX", "PCM0.0", "PCM0 RX"},
	{"MM PASSIN1 MUX", "PCM0.1", "PCM0 RX"},
	{"MM PASSIN1 MUX", "PCM0.2", "PCM0 RX"},
	{"MM PASSIN1 MUX", "PCM0.3", "PCM0 RX"},
	{"MM PASSIN1 MUX", "PCM1.0", "PCM1 RX"},
	{"MM PASSIN1 MUX", "PCM1.1", "PCM1 RX"},
	{"MM PASSIN1 MUX", "PCM1.2", "PCM1 RX"},
	{"MM PASSIN1 MUX", "PCM1.3", "PCM1 RX"},
	{"MM PASSIN1 MUX", "PCM2.0", "PCM2 RX"},
	{"MM PASSIN1 MUX", "PCM2.1", "PCM2 RX"},
	{"MM PASSIN1 MUX", "PCM2.2", "PCM2 RX"},
	{"MM PASSIN1 MUX", "PCM2.3", "PCM2 RX"},
	{"MM PASSIN1 MUX", "PDMI0", "PDMI0"},
	{"MM PASSIN1 MUX", "PDMI1", "PDMI1"},
	{"MM PASSIN1 MUX", "PDMI2", "PDMI2"},
	{"MM PASSIN1 MUX", "PDMI3", "PDMI3"},
	{"MM PASSIN1 MUX", "SBUS.RX0", "SBUS.RX0"},
	{"MM PASSIN1 MUX", "SBUS.RX1", "SBUS.RX1"},
	{"MM PASSIN1 MUX", "SBUS.RX2", "SBUS.RX2"},
	{"MM PASSIN1 MUX", "SBUS.RX3", "SBUS.RX3"},
	{"MM PASSIN1 MUX", "SBUS.RX4", "SBUS.RX4"},
	{"MM PASSIN1 MUX", "SBUS.RX5", "SBUS.RX5"},
	{"MM PASSIN1 MUX", "SBUS.RX6", "SBUS.RX6"},
	{"MM PASSIN1 MUX", "SBUS.RX7", "SBUS.RX7"},
	{"MM PASSIN1 MUX", "SBUS.RX8", "SBUS.RX8"},
	{"MM PASSIN1 MUX", "SBUS.RX9", "SBUS.RX9"},
	{"MM PASSIN1 MUX", "ADC0", "ADC0"},
	{"MM PASSIN1 MUX", "ADC1", "ADC1"},
	{"MM PASSIN1 MUX", "ADC2", "ADC2"},
	{"MM PASSIN1 MUX", "ADC3", "ADC3"},

	{"MM PASSIN2 MUX", "PCM0.0", "PCM0 RX"},
	{"MM PASSIN2 MUX", "PCM0.1", "PCM0 RX"},
	{"MM PASSIN2 MUX", "PCM0.2", "PCM0 RX"},
	{"MM PASSIN2 MUX", "PCM0.3", "PCM0 RX"},
	{"MM PASSIN2 MUX", "PCM1.0", "PCM1 RX"},
	{"MM PASSIN2 MUX", "PCM1.1", "PCM1 RX"},
	{"MM PASSIN2 MUX", "PCM1.2", "PCM1 RX"},
	{"MM PASSIN2 MUX", "PCM1.3", "PCM1 RX"},
	{"MM PASSIN2 MUX", "PCM2.0", "PCM2 RX"},
	{"MM PASSIN2 MUX", "PCM2.1", "PCM2 RX"},
	{"MM PASSIN2 MUX", "PCM2.2", "PCM2 RX"},
	{"MM PASSIN2 MUX", "PCM2.3", "PCM2 RX"},
	{"MM PASSIN2 MUX", "PDMI0", "PDMI0"},
	{"MM PASSIN2 MUX", "PDMI1", "PDMI1"},
	{"MM PASSIN2 MUX", "PDMI2", "PDMI2"},
	{"MM PASSIN2 MUX", "PDMI3", "PDMI3"},
	{"MM PASSIN2 MUX", "SBUS.RX0", "SBUS.RX0"},
	{"MM PASSIN2 MUX", "SBUS.RX1", "SBUS.RX1"},
	{"MM PASSIN2 MUX", "SBUS.RX2", "SBUS.RX2"},
	{"MM PASSIN2 MUX", "SBUS.RX3", "SBUS.RX3"},
	{"MM PASSIN2 MUX", "SBUS.RX4", "SBUS.RX4"},
	{"MM PASSIN2 MUX", "SBUS.RX5", "SBUS.RX5"},
	{"MM PASSIN2 MUX", "SBUS.RX6", "SBUS.RX6"},
	{"MM PASSIN2 MUX", "SBUS.RX7", "SBUS.RX7"},
	{"MM PASSIN2 MUX", "SBUS.RX8", "SBUS.RX8"},
	{"MM PASSIN2 MUX", "SBUS.RX9", "SBUS.RX9"},
	{"MM PASSIN2 MUX", "ADC0", "ADC0"},
	{"MM PASSIN2 MUX", "ADC1", "ADC1"},
	{"MM PASSIN2 MUX", "ADC2", "ADC2"},
	{"MM PASSIN2 MUX", "ADC3", "ADC3"},

	{"MM UITONE1 MUX", "PCM0.0", "PCM0 RX"},
	{"MM UITONE1 MUX", "PCM0.1", "PCM0 RX"},
	{"MM UITONE1 MUX", "PCM0.2", "PCM0 RX"},
	{"MM UITONE1 MUX", "PCM0.3", "PCM0 RX"},
	{"MM UITONE1 MUX", "PCM1.0", "PCM1 RX"},
	{"MM UITONE1 MUX", "PCM1.1", "PCM1 RX"},
	{"MM UITONE1 MUX", "PCM1.2", "PCM1 RX"},
	{"MM UITONE1 MUX", "PCM1.3", "PCM1 RX"},
	{"MM UITONE1 MUX", "PCM2.0", "PCM2 RX"},
	{"MM UITONE1 MUX", "PCM2.1", "PCM2 RX"},
	{"MM UITONE1 MUX", "PCM2.2", "PCM2 RX"},
	{"MM UITONE1 MUX", "PCM2.3", "PCM2 RX"},
	{"MM UITONE1 MUX", "SBUS.RX0", "SBUS.RX0"},
	{"MM UITONE1 MUX", "SBUS.RX1", "SBUS.RX1"},
	{"MM UITONE1 MUX", "SBUS.RX2", "SBUS.RX2"},
	{"MM UITONE1 MUX", "SBUS.RX3", "SBUS.RX3"},
	{"MM UITONE1 MUX", "SBUS.RX4", "SBUS.RX4"},
	{"MM UITONE1 MUX", "SBUS.RX5", "SBUS.RX5"},
	{"MM UITONE1 MUX", "SBUS.RX6", "SBUS.RX6"},
	{"MM UITONE1 MUX", "SBUS.RX7", "SBUS.RX7"},
	{"MM UITONE1 MUX", "SBUS.RX8", "SBUS.RX8"},
	{"MM UITONE1 MUX", "SBUS.RX9", "SBUS.RX9"},

	{"MM UITONE2 MUX", "PCM0.0", "PCM0 RX"},
	{"MM UITONE2 MUX", "PCM0.1", "PCM0 RX"},
	{"MM UITONE2 MUX", "PCM0.2", "PCM0 RX"},
	{"MM UITONE2 MUX", "PCM0.3", "PCM0 RX"},
	{"MM UITONE2 MUX", "PCM1.0", "PCM1 RX"},
	{"MM UITONE2 MUX", "PCM1.1", "PCM1 RX"},
	{"MM UITONE2 MUX", "PCM1.2", "PCM1 RX"},
	{"MM UITONE2 MUX", "PCM1.3", "PCM1 RX"},
	{"MM UITONE2 MUX", "PCM2.0", "PCM2 RX"},
	{"MM UITONE2 MUX", "PCM2.1", "PCM2 RX"},
	{"MM UITONE2 MUX", "PCM2.2", "PCM2 RX"},
	{"MM UITONE2 MUX", "PCM2.3", "PCM2 RX"},
	{"MM UITONE2 MUX", "SBUS.RX0", "SBUS.RX0"},
	{"MM UITONE2 MUX", "SBUS.RX1", "SBUS.RX1"},
	{"MM UITONE2 MUX", "SBUS.RX2", "SBUS.RX2"},
	{"MM UITONE2 MUX", "SBUS.RX3", "SBUS.RX3"},
	{"MM UITONE2 MUX", "SBUS.RX4", "SBUS.RX4"},
	{"MM UITONE2 MUX", "SBUS.RX5", "SBUS.RX5"},
	{"MM UITONE2 MUX", "SBUS.RX6", "SBUS.RX6"},
	{"MM UITONE2 MUX", "SBUS.RX7", "SBUS.RX7"},
	{"MM UITONE2 MUX", "SBUS.RX8", "SBUS.RX8"},
	{"MM UITONE2 MUX", "SBUS.RX9", "SBUS.RX9"},

	{"MM AUDOUT1 Mixer", NULL, "MM AUDIN1 MUX"},
	{"MM AUDOUT2 Mixer", NULL, "MM AUDIN2 MUX"},
	{"MM PASSOUT1 Mixer", NULL, "MM PASSIN1 MUX"},
	{"MM PASSOUT2 Mixer", NULL, "MM PASSIN2 MUX"},
	{"MM MONOUT1 Mixer", NULL, "MM UITONE1 MUX"},
	{"MM MONOUT2 Mixer", NULL, "MM UITONE2 MUX"},

	{"Pass AUDIN1 MUX", "PCM0.0", "PCM0 RX"},
	{"Pass AUDIN1 MUX", "PCM0.1", "PCM0 RX"},
	{"Pass AUDIN1 MUX", "PCM0.2", "PCM0 RX"},
	{"Pass AUDIN1 MUX", "PCM0.3", "PCM0 RX"},
	{"Pass AUDIN1 MUX", "PCM1.0", "PCM1 RX"},
	{"Pass AUDIN1 MUX", "PCM1.1", "PCM1 RX"},
	{"Pass AUDIN1 MUX", "PCM1.2", "PCM1 RX"},
	{"Pass AUDIN1 MUX", "PCM1.3", "PCM1 RX"},
	{"Pass AUDIN1 MUX", "PCM2.0", "PCM2 RX"},
	{"Pass AUDIN1 MUX", "PCM2.1", "PCM2 RX"},
	{"Pass AUDIN1 MUX", "PCM2.2", "PCM2 RX"},
	{"Pass AUDIN1 MUX", "PCM2.3", "PCM2 RX"},
	{"Pass AUDIN1 MUX", "PDMI0", "PDMI0"},
	{"Pass AUDIN1 MUX", "PDMI1", "PDMI1"},
	{"Pass AUDIN1 MUX", "PDMI2", "PDMI2"},
	{"Pass AUDIN1 MUX", "PDMI3", "PDMI3"},
	{"Pass AUDIN1 MUX", "SBUS.RX0", "SBUS.RX0"},
	{"Pass AUDIN1 MUX", "SBUS.RX1", "SBUS.RX1"},
	{"Pass AUDIN1 MUX", "SBUS.RX2", "SBUS.RX2"},
	{"Pass AUDIN1 MUX", "SBUS.RX3", "SBUS.RX3"},
	{"Pass AUDIN1 MUX", "SBUS.RX4", "SBUS.RX4"},
	{"Pass AUDIN1 MUX", "SBUS.RX5", "SBUS.RX5"},
	{"Pass AUDIN1 MUX", "SBUS.RX6", "SBUS.RX6"},
	{"Pass AUDIN1 MUX", "SBUS.RX7", "SBUS.RX7"},
	{"Pass AUDIN1 MUX", "SBUS.RX8", "SBUS.RX8"},
	{"Pass AUDIN1 MUX", "SBUS.RX9", "SBUS.RX9"},
	{"Pass AUDIN1 MUX", "ADC0", "ADC0"},
	{"Pass AUDIN1 MUX", "ADC1", "ADC1"},
	{"Pass AUDIN1 MUX", "ADC2", "ADC2"},
	{"Pass AUDIN1 MUX", "ADC3", "ADC3"},

	{"Pass AUDIN2 MUX", "PCM0.0", "PCM0 RX"},
	{"Pass AUDIN2 MUX", "PCM0.1", "PCM0 RX"},
	{"Pass AUDIN2 MUX", "PCM0.2", "PCM0 RX"},
	{"Pass AUDIN2 MUX", "PCM0.3", "PCM0 RX"},
	{"Pass AUDIN2 MUX", "PCM1.0", "PCM1 RX"},
	{"Pass AUDIN2 MUX", "PCM1.1", "PCM1 RX"},
	{"Pass AUDIN2 MUX", "PCM1.2", "PCM1 RX"},
	{"Pass AUDIN2 MUX", "PCM1.3", "PCM1 RX"},
	{"Pass AUDIN2 MUX", "PCM2.0", "PCM2 RX"},
	{"Pass AUDIN2 MUX", "PCM2.1", "PCM2 RX"},
	{"Pass AUDIN2 MUX", "PCM2.2", "PCM2 RX"},
	{"Pass AUDIN2 MUX", "PCM2.3", "PCM2 RX"},
	{"Pass AUDIN2 MUX", "PDMI0", "PDMI0"},
	{"Pass AUDIN2 MUX", "PDMI1", "PDMI1"},
	{"Pass AUDIN2 MUX", "PDMI2", "PDMI2"},
	{"Pass AUDIN2 MUX", "PDMI3", "PDMI3"},
	{"Pass AUDIN2 MUX", "SBUS.RX0", "SBUS.RX0"},
	{"Pass AUDIN2 MUX", "SBUS.RX1", "SBUS.RX1"},
	{"Pass AUDIN2 MUX", "SBUS.RX2", "SBUS.RX2"},
	{"Pass AUDIN2 MUX", "SBUS.RX3", "SBUS.RX3"},
	{"Pass AUDIN2 MUX", "SBUS.RX4", "SBUS.RX4"},
	{"Pass AUDIN2 MUX", "SBUS.RX5", "SBUS.RX5"},
	{"Pass AUDIN2 MUX", "SBUS.RX6", "SBUS.RX6"},
	{"Pass AUDIN2 MUX", "SBUS.RX7", "SBUS.RX7"},
	{"Pass AUDIN2 MUX", "SBUS.RX8", "SBUS.RX8"},
	{"Pass AUDIN2 MUX", "SBUS.RX9", "SBUS.RX9"},
	{"Pass AUDIN2 MUX", "ADC0", "ADC0"},
	{"Pass AUDIN2 MUX", "ADC1", "ADC1"},
	{"Pass AUDIN2 MUX", "ADC2", "ADC2"},
	{"Pass AUDIN2 MUX", "ADC3", "ADC3"},

	{"Pass AUDIN3 MUX", "PCM0.0", "PCM0 RX"},
	{"Pass AUDIN3 MUX", "PCM0.1", "PCM0 RX"},
	{"Pass AUDIN3 MUX", "PCM0.2", "PCM0 RX"},
	{"Pass AUDIN3 MUX", "PCM0.3", "PCM0 RX"},
	{"Pass AUDIN3 MUX", "PCM1.0", "PCM1 RX"},
	{"Pass AUDIN3 MUX", "PCM1.1", "PCM1 RX"},
	{"Pass AUDIN3 MUX", "PCM1.2", "PCM1 RX"},
	{"Pass AUDIN3 MUX", "PCM1.3", "PCM1 RX"},
	{"Pass AUDIN3 MUX", "PCM2.0", "PCM2 RX"},
	{"Pass AUDIN3 MUX", "PCM2.1", "PCM2 RX"},
	{"Pass AUDIN3 MUX", "PCM2.2", "PCM2 RX"},
	{"Pass AUDIN3 MUX", "PCM2.3", "PCM2 RX"},
	{"Pass AUDIN3 MUX", "PDMI0", "PDMI0"},
	{"Pass AUDIN3 MUX", "PDMI1", "PDMI1"},
	{"Pass AUDIN3 MUX", "PDMI2", "PDMI2"},
	{"Pass AUDIN3 MUX", "PDMI3", "PDMI3"},
	{"Pass AUDIN3 MUX", "SBUS.RX0", "SBUS.RX0"},
	{"Pass AUDIN3 MUX", "SBUS.RX1", "SBUS.RX1"},
	{"Pass AUDIN3 MUX", "SBUS.RX2", "SBUS.RX2"},
	{"Pass AUDIN3 MUX", "SBUS.RX3", "SBUS.RX3"},
	{"Pass AUDIN3 MUX", "SBUS.RX4", "SBUS.RX4"},
	{"Pass AUDIN3 MUX", "SBUS.RX5", "SBUS.RX5"},
	{"Pass AUDIN3 MUX", "SBUS.RX6", "SBUS.RX6"},
	{"Pass AUDIN3 MUX", "SBUS.RX7", "SBUS.RX7"},
	{"Pass AUDIN3 MUX", "SBUS.RX8", "SBUS.RX8"},
	{"Pass AUDIN3 MUX", "SBUS.RX9", "SBUS.RX9"},
	{"Pass AUDIN3 MUX", "ADC0", "ADC0"},
	{"Pass AUDIN3 MUX", "ADC1", "ADC1"},
	{"Pass AUDIN3 MUX", "ADC2", "ADC2"},
	{"Pass AUDIN3 MUX", "ADC3", "ADC3"},

	{"Pass AUDIN4 MUX", "PCM0.0", "PCM0 RX"},
	{"Pass AUDIN4 MUX", "PCM0.1", "PCM0 RX"},
	{"Pass AUDIN4 MUX", "PCM0.2", "PCM0 RX"},
	{"Pass AUDIN4 MUX", "PCM0.3", "PCM0 RX"},
	{"Pass AUDIN4 MUX", "PCM1.0", "PCM1 RX"},
	{"Pass AUDIN4 MUX", "PCM1.1", "PCM1 RX"},
	{"Pass AUDIN4 MUX", "PCM1.2", "PCM1 RX"},
	{"Pass AUDIN4 MUX", "PCM1.3", "PCM1 RX"},
	{"Pass AUDIN4 MUX", "PCM2.0", "PCM2 RX"},
	{"Pass AUDIN4 MUX", "PCM2.1", "PCM2 RX"},
	{"Pass AUDIN4 MUX", "PCM2.2", "PCM2 RX"},
	{"Pass AUDIN4 MUX", "PCM2.3", "PCM2 RX"},
	{"Pass AUDIN4 MUX", "PDMI0", "PDMI0"},
	{"Pass AUDIN4 MUX", "PDMI1", "PDMI1"},
	{"Pass AUDIN4 MUX", "PDMI2", "PDMI2"},
	{"Pass AUDIN4 MUX", "PDMI3", "PDMI3"},
	{"Pass AUDIN4 MUX", "SBUS.RX0", "SBUS.RX0"},
	{"Pass AUDIN4 MUX", "SBUS.RX1", "SBUS.RX1"},
	{"Pass AUDIN4 MUX", "SBUS.RX2", "SBUS.RX2"},
	{"Pass AUDIN4 MUX", "SBUS.RX3", "SBUS.RX3"},
	{"Pass AUDIN4 MUX", "SBUS.RX4", "SBUS.RX4"},
	{"Pass AUDIN4 MUX", "SBUS.RX5", "SBUS.RX5"},
	{"Pass AUDIN4 MUX", "SBUS.RX6", "SBUS.RX6"},
	{"Pass AUDIN4 MUX", "SBUS.RX7", "SBUS.RX7"},
	{"Pass AUDIN4 MUX", "SBUS.RX8", "SBUS.RX8"},
	{"Pass AUDIN4 MUX", "SBUS.RX9", "SBUS.RX9"},
	{"Pass AUDIN4 MUX", "ADC0", "ADC0"},
	{"Pass AUDIN4 MUX", "ADC1", "ADC1"},
	{"Pass AUDIN4 MUX", "ADC2", "ADC2"},
	{"Pass AUDIN4 MUX", "ADC3", "ADC3"},

	{"Pass AUDOUT1 Mixer", NULL, "Pass AUDIN1 MUX"},
	{"Pass AUDOUT2 Mixer", NULL, "Pass AUDIN2 MUX"},
	{"Pass AUDOUT3 Mixer", NULL, "Pass AUDIN3 MUX"},
	{"Pass AUDOUT4 Mixer", NULL, "Pass AUDIN4 MUX"},

	{"AudioZoom Primary MUX", "PCM0.0", "PCM0 RX"},
	{"AudioZoom Primary MUX", "PCM0.1", "PCM0 RX"},
	{"AudioZoom Primary MUX", "PCM0.2", "PCM0 RX"},
	{"AudioZoom Primary MUX", "PCM0.3", "PCM0 RX"},
	{"AudioZoom Primary MUX", "PCM1.0", "PCM1 RX"},
	{"AudioZoom Primary MUX", "PCM1.1", "PCM1 RX"},
	{"AudioZoom Primary MUX", "PCM1.2", "PCM1 RX"},
	{"AudioZoom Primary MUX", "PCM1.3", "PCM1 RX"},
	{"AudioZoom Primary MUX", "PCM2.0", "PCM2 RX"},
	{"AudioZoom Primary MUX", "PCM2.1", "PCM2 RX"},
	{"AudioZoom Primary MUX", "PCM2.2", "PCM2 RX"},
	{"AudioZoom Primary MUX", "PCM2.3", "PCM2 RX"},
	{"AudioZoom Primary MUX", "PDMI0", "PDMI0"},
	{"AudioZoom Primary MUX", "PDMI1", "PDMI1"},
	{"AudioZoom Primary MUX", "PDMI2", "PDMI2"},
	{"AudioZoom Primary MUX", "PDMI3", "PDMI3"},
	{"AudioZoom Primary MUX", "SBUS.RX0", "SBUS.RX0"},
	{"AudioZoom Primary MUX", "SBUS.RX1", "SBUS.RX1"},
	{"AudioZoom Primary MUX", "SBUS.RX2", "SBUS.RX2"},
	{"AudioZoom Primary MUX", "SBUS.RX3", "SBUS.RX3"},
	{"AudioZoom Primary MUX", "SBUS.RX4", "SBUS.RX4"},
	{"AudioZoom Primary MUX", "SBUS.RX5", "SBUS.RX5"},
	{"AudioZoom Primary MUX", "SBUS.RX6", "SBUS.RX6"},
	{"AudioZoom Primary MUX", "SBUS.RX7", "SBUS.RX7"},
	{"AudioZoom Primary MUX", "SBUS.RX8", "SBUS.RX8"},
	{"AudioZoom Primary MUX", "SBUS.RX9", "SBUS.RX9"},
	{"AudioZoom Primary MUX", "ADC0", "ADC0"},
	{"AudioZoom Primary MUX", "ADC1", "ADC1"},
	{"AudioZoom Primary MUX", "ADC2", "ADC2"},
	{"AudioZoom Primary MUX", "ADC3", "ADC3"},

	{"AudioZoom Secondary MUX", "PCM0.0", "PCM0 RX"},
	{"AudioZoom Secondary MUX", "PCM0.1", "PCM0 RX"},
	{"AudioZoom Secondary MUX", "PCM0.2", "PCM0 RX"},
	{"AudioZoom Secondary MUX", "PCM0.3", "PCM0 RX"},
	{"AudioZoom Secondary MUX", "PCM1.0", "PCM1 RX"},
	{"AudioZoom Secondary MUX", "PCM1.1", "PCM1 RX"},
	{"AudioZoom Secondary MUX", "PCM1.2", "PCM1 RX"},
	{"AudioZoom Secondary MUX", "PCM1.3", "PCM1 RX"},
	{"AudioZoom Secondary MUX", "PCM2.0", "PCM2 RX"},
	{"AudioZoom Secondary MUX", "PCM2.1", "PCM2 RX"},
	{"AudioZoom Secondary MUX", "PCM2.2", "PCM2 RX"},
	{"AudioZoom Secondary MUX", "PCM2.3", "PCM2 RX"},
	{"AudioZoom Secondary MUX", "PDMI0", "PDMI0"},
	{"AudioZoom Secondary MUX", "PDMI1", "PDMI1"},
	{"AudioZoom Secondary MUX", "PDMI2", "PDMI2"},
	{"AudioZoom Secondary MUX", "PDMI3", "PDMI3"},
	{"AudioZoom Secondary MUX", "SBUS.RX0", "SBUS.RX0"},
	{"AudioZoom Secondary MUX", "SBUS.RX1", "SBUS.RX1"},
	{"AudioZoom Secondary MUX", "SBUS.RX2", "SBUS.RX2"},
	{"AudioZoom Secondary MUX", "SBUS.RX3", "SBUS.RX3"},
	{"AudioZoom Secondary MUX", "SBUS.RX4", "SBUS.RX4"},
	{"AudioZoom Secondary MUX", "SBUS.RX5", "SBUS.RX5"},
	{"AudioZoom Secondary MUX", "SBUS.RX6", "SBUS.RX6"},
	{"AudioZoom Secondary MUX", "SBUS.RX7", "SBUS.RX7"},
	{"AudioZoom Secondary MUX", "SBUS.RX8", "SBUS.RX8"},
	{"AudioZoom Secondary MUX", "SBUS.RX9", "SBUS.RX9"},
	{"AudioZoom Secondary MUX", "ADC0", "ADC0"},
	{"AudioZoom Secondary MUX", "ADC1", "ADC1"},
	{"AudioZoom Secondary MUX", "ADC2", "ADC2"},
	{"AudioZoom Secondary MUX", "ADC3", "ADC3"},

	{"AudioZoom Teritary MUX", "PCM0.0", "PCM0 RX"},
	{"AudioZoom Teritary MUX", "PCM0.1", "PCM0 RX"},
	{"AudioZoom Teritary MUX", "PCM0.2", "PCM0 RX"},
	{"AudioZoom Teritary MUX", "PCM0.3", "PCM0 RX"},
	{"AudioZoom Teritary MUX", "PCM1.0", "PCM1 RX"},
	{"AudioZoom Teritary MUX", "PCM1.1", "PCM1 RX"},
	{"AudioZoom Teritary MUX", "PCM1.2", "PCM1 RX"},
	{"AudioZoom Teritary MUX", "PCM1.3", "PCM1 RX"},
	{"AudioZoom Teritary MUX", "PCM2.0", "PCM2 RX"},
	{"AudioZoom Teritary MUX", "PCM2.1", "PCM2 RX"},
	{"AudioZoom Teritary MUX", "PCM2.2", "PCM2 RX"},
	{"AudioZoom Teritary MUX", "PCM2.3", "PCM2 RX"},
	{"AudioZoom Teritary MUX", "PDMI0", "PDMI0"},
	{"AudioZoom Teritary MUX", "PDMI1", "PDMI1"},
	{"AudioZoom Teritary MUX", "PDMI2", "PDMI2"},
	{"AudioZoom Teritary MUX", "PDMI3", "PDMI3"},
	{"AudioZoom Teritary MUX", "SBUS.RX0", "SBUS.RX0"},
	{"AudioZoom Teritary MUX", "SBUS.RX1", "SBUS.RX1"},
	{"AudioZoom Teritary MUX", "SBUS.RX2", "SBUS.RX2"},
	{"AudioZoom Teritary MUX", "SBUS.RX3", "SBUS.RX3"},
	{"AudioZoom Teritary MUX", "SBUS.RX4", "SBUS.RX4"},
	{"AudioZoom Teritary MUX", "SBUS.RX5", "SBUS.RX5"},
	{"AudioZoom Teritary MUX", "SBUS.RX6", "SBUS.RX6"},
	{"AudioZoom Teritary MUX", "SBUS.RX7", "SBUS.RX7"},
	{"AudioZoom Teritary MUX", "SBUS.RX8", "SBUS.RX8"},
	{"AudioZoom Teritary MUX", "SBUS.RX9", "SBUS.RX9"},
	{"AudioZoom Teritary MUX", "ADC0", "ADC0"},
	{"AudioZoom Teritary MUX", "ADC1", "ADC1"},
	{"AudioZoom Teritary MUX", "ADC2", "ADC2"},
	{"AudioZoom Teritary MUX", "ADC3", "ADC3"},

	{"AudioZoom AI1 MUX", "PCM0.0", "PCM0 RX"},
	{"AudioZoom AI1 MUX", "PCM0.1", "PCM0 RX"},
	{"AudioZoom AI1 MUX", "PCM0.2", "PCM0 RX"},
	{"AudioZoom AI1 MUX", "PCM0.3", "PCM0 RX"},
	{"AudioZoom AI1 MUX", "PCM1.0", "PCM1 RX"},
	{"AudioZoom AI1 MUX", "PCM1.1", "PCM1 RX"},
	{"AudioZoom AI1 MUX", "PCM1.2", "PCM1 RX"},
	{"AudioZoom AI1 MUX", "PCM1.3", "PCM1 RX"},
	{"AudioZoom AI1 MUX", "PCM2.0", "PCM2 RX"},
	{"AudioZoom AI1 MUX", "PCM2.1", "PCM2 RX"},
	{"AudioZoom AI1 MUX", "PCM2.2", "PCM2 RX"},
	{"AudioZoom AI1 MUX", "PCM2.3", "PCM2 RX"},
	{"AudioZoom AI1 MUX", "PDMI0", "PDMI0"},
	{"AudioZoom AI1 MUX", "PDMI1", "PDMI1"},
	{"AudioZoom AI1 MUX", "PDMI2", "PDMI2"},
	{"AudioZoom AI1 MUX", "PDMI3", "PDMI3"},
	{"AudioZoom AI1 MUX", "SBUS.RX0", "SBUS.RX0"},
	{"AudioZoom AI1 MUX", "SBUS.RX1", "SBUS.RX1"},
	{"AudioZoom AI1 MUX", "SBUS.RX2", "SBUS.RX2"},
	{"AudioZoom AI1 MUX", "SBUS.RX3", "SBUS.RX3"},
	{"AudioZoom AI1 MUX", "SBUS.RX4", "SBUS.RX4"},
	{"AudioZoom AI1 MUX", "SBUS.RX5", "SBUS.RX5"},
	{"AudioZoom AI1 MUX", "SBUS.RX6", "SBUS.RX6"},
	{"AudioZoom AI1 MUX", "SBUS.RX7", "SBUS.RX7"},
	{"AudioZoom AI1 MUX", "SBUS.RX8", "SBUS.RX8"},
	{"AudioZoom AI1 MUX", "SBUS.RX9", "SBUS.RX9"},
	{"AudioZoom AI1 MUX", "ADC0", "ADC0"},
	{"AudioZoom AI1 MUX", "ADC1", "ADC1"},
	{"AudioZoom AI1 MUX", "ADC2", "ADC2"},
	{"AudioZoom AI1 MUX", "ADC3", "ADC3"},

	{"AudioZoom CSOUT Mixer", NULL, "AudioZoom Primary MUX"},
	{"AudioZoom CSOUT Mixer", NULL, "AudioZoom Secondary MUX"},
	{"AudioZoom CSOUT Mixer", NULL, "AudioZoom Teritary MUX"},
	{"AudioZoom AOUT1 Mixer", NULL, "AudioZoom AI1 MUX"},

	{"PCM0.0 MUX", "VP CSOUT", "VP CSOUT Mixer"},
	{"PCM0.0 MUX", "VP FEOUT1", "VP FEOUT1 Mixer"},
	{"PCM0.0 MUX", "VP FEOUT2", "VP FEOUT2 Mixer"},
	{"PCM0.0 MUX", "VP MONOUT1", "VP MONOUT1 Mixer"},
	{"PCM0.0 MUX", "VP MONOUT2", "VP MONOUT2 Mixer"},
	{"PCM0.0 MUX", "AudioZoom CSOUT", "AudioZoom CSOUT Mixer"},
	{"PCM0.0 MUX", "AudioZoom AOUT1", "AudioZoom AOUT1 Mixer"},
	{"PCM0.0 MUX", "MM PASSOUT1", "MM PASSOUT1 Mixer"},
	{"PCM0.0 MUX", "MM PASSOUT1", "MM PASSOUT2 Mixer"},
	{"PCM0.0 MUX", "MM AUDOUT1", "MM AUDOUT1 Mixer"},
	{"PCM0.0 MUX", "MM AUDOUT2", "MM AUDOUT2 Mixer"},
	{"PCM0.0 MUX", "MM MONOUT1", "MM MONOUT1 Mixer"},
	{"PCM0.0 MUX", "MM MONOUT2", "MM MONOUT2 Mixer"},
	{"PCM0.0 MUX", "Pass AUDOUT1", "Pass AUDOUT1 Mixer"},
	{"PCM0.0 MUX", "Pass AUDOUT2", "Pass AUDOUT2 Mixer"},
	{"PCM0.0 MUX", "Pass AUDOUT3", "Pass AUDOUT3 Mixer"},
	{"PCM0.0 MUX", "Pass AUDOUT4", "Pass AUDOUT4 Mixer"},
	{"PCM0.1 MUX", "VP CSOUT", "VP CSOUT Mixer"},
	{"PCM0.1 MUX", "VP FEOUT1", "VP FEOUT1 Mixer"},
	{"PCM0.1 MUX", "VP FEOUT2", "VP FEOUT2 Mixer"},
	{"PCM0.1 MUX", "VP MONOUT1", "VP MONOUT1 Mixer"},
	{"PCM0.1 MUX", "VP MONOUT2", "VP MONOUT2 Mixer"},
	{"PCM0.1 MUX", "AudioZoom CSOUT", "AudioZoom CSOUT Mixer"},
	{"PCM0.1 MUX", "AudioZoom AOUT1", "AudioZoom AOUT1 Mixer"},
	{"PCM0.1 MUX", "MM PASSOUT1", "MM PASSOUT1 Mixer"},
	{"PCM0.1 MUX", "MM PASSOUT1", "MM PASSOUT2 Mixer"},
	{"PCM0.1 MUX", "MM AUDOUT1", "MM AUDOUT1 Mixer"},
	{"PCM0.1 MUX", "MM AUDOUT2", "MM AUDOUT2 Mixer"},
	{"PCM0.1 MUX", "MM MONOUT1", "MM MONOUT1 Mixer"},
	{"PCM0.1 MUX", "MM MONOUT2", "MM MONOUT2 Mixer"},
	{"PCM0.1 MUX", "Pass AUDOUT1", "Pass AUDOUT1 Mixer"},
	{"PCM0.1 MUX", "Pass AUDOUT2", "Pass AUDOUT2 Mixer"},
	{"PCM0.1 MUX", "Pass AUDOUT3", "Pass AUDOUT3 Mixer"},
	{"PCM0.1 MUX", "Pass AUDOUT4", "Pass AUDOUT4 Mixer"},
	{"PCM0.2 MUX", "VP CSOUT", "VP CSOUT Mixer"},
	{"PCM0.2 MUX", "VP FEOUT1", "VP FEOUT1 Mixer"},
	{"PCM0.2 MUX", "VP FEOUT2", "VP FEOUT2 Mixer"},
	{"PCM0.2 MUX", "VP MONOUT1", "VP MONOUT1 Mixer"},
	{"PCM0.2 MUX", "VP MONOUT2", "VP MONOUT2 Mixer"},
	{"PCM0.2 MUX", "AudioZoom CSOUT", "AudioZoom CSOUT Mixer"},
	{"PCM0.2 MUX", "AudioZoom AOUT1", "AudioZoom AOUT1 Mixer"},
	{"PCM0.2 MUX", "MM PASSOUT1", "MM PASSOUT1 Mixer"},
	{"PCM0.2 MUX", "MM PASSOUT1", "MM PASSOUT2 Mixer"},
	{"PCM0.2 MUX", "MM AUDOUT1", "MM AUDOUT1 Mixer"},
	{"PCM0.2 MUX", "MM AUDOUT2", "MM AUDOUT2 Mixer"},
	{"PCM0.2 MUX", "MM MONOUT1", "MM MONOUT1 Mixer"},
	{"PCM0.2 MUX", "MM MONOUT2", "MM MONOUT2 Mixer"},
	{"PCM0.2 MUX", "Pass AUDOUT1", "Pass AUDOUT1 Mixer"},
	{"PCM0.2 MUX", "Pass AUDOUT2", "Pass AUDOUT2 Mixer"},
	{"PCM0.2 MUX", "Pass AUDOUT3", "Pass AUDOUT3 Mixer"},
	{"PCM0.2 MUX", "Pass AUDOUT4", "Pass AUDOUT4 Mixer"},
	{"PCM0.3 MUX", "VP CSOUT", "VP CSOUT Mixer"},
	{"PCM0.3 MUX", "VP FEOUT1", "VP FEOUT1 Mixer"},
	{"PCM0.3 MUX", "VP FEOUT2", "VP FEOUT2 Mixer"},
	{"PCM0.3 MUX", "VP MONOUT1", "VP MONOUT1 Mixer"},
	{"PCM0.3 MUX", "VP MONOUT2", "VP MONOUT2 Mixer"},
	{"PCM0.3 MUX", "AudioZoom CSOUT", "AudioZoom CSOUT Mixer"},
	{"PCM0.3 MUX", "AudioZoom AOUT1", "AudioZoom AOUT1 Mixer"},
	{"PCM0.3 MUX", "MM PASSOUT1", "MM PASSOUT1 Mixer"},
	{"PCM0.3 MUX", "MM PASSOUT1", "MM PASSOUT2 Mixer"},
	{"PCM0.3 MUX", "MM AUDOUT1", "MM AUDOUT1 Mixer"},
	{"PCM0.3 MUX", "MM AUDOUT2", "MM AUDOUT2 Mixer"},
	{"PCM0.3 MUX", "MM MONOUT1", "MM MONOUT1 Mixer"},
	{"PCM0.3 MUX", "MM MONOUT2", "MM MONOUT2 Mixer"},
	{"PCM0.3 MUX", "Pass AUDOUT1", "Pass AUDOUT1 Mixer"},
	{"PCM0.3 MUX", "Pass AUDOUT2", "Pass AUDOUT2 Mixer"},
	{"PCM0.3 MUX", "Pass AUDOUT3", "Pass AUDOUT3 Mixer"},
	{"PCM0.3 MUX", "Pass AUDOUT4", "Pass AUDOUT4 Mixer"},

	{"PCM1.0 MUX", "VP CSOUT", "VP CSOUT Mixer"},
	{"PCM1.0 MUX", "VP FEOUT1", "VP FEOUT1 Mixer"},
	{"PCM1.0 MUX", "VP FEOUT2", "VP FEOUT2 Mixer"},
	{"PCM1.0 MUX", "VP MONOUT1", "VP MONOUT1 Mixer"},
	{"PCM1.0 MUX", "VP MONOUT2", "VP MONOUT2 Mixer"},
	{"PCM1.0 MUX", "AudioZoom CSOUT", "AudioZoom CSOUT Mixer"},
	{"PCM1.0 MUX", "AudioZoom AOUT1", "AudioZoom AOUT1 Mixer"},
	{"PCM1.0 MUX", "MM PASSOUT1", "MM PASSOUT1 Mixer"},
	{"PCM1.0 MUX", "MM PASSOUT1", "MM PASSOUT2 Mixer"},
	{"PCM1.0 MUX", "MM AUDOUT1", "MM AUDOUT1 Mixer"},
	{"PCM1.0 MUX", "MM AUDOUT2", "MM AUDOUT2 Mixer"},
	{"PCM1.0 MUX", "MM MONOUT1", "MM MONOUT1 Mixer"},
	{"PCM1.0 MUX", "MM MONOUT2", "MM MONOUT2 Mixer"},
	{"PCM1.0 MUX", "Pass AUDOUT1", "Pass AUDOUT1 Mixer"},
	{"PCM1.0 MUX", "Pass AUDOUT2", "Pass AUDOUT2 Mixer"},
	{"PCM1.0 MUX", "Pass AUDOUT3", "Pass AUDOUT3 Mixer"},
	{"PCM1.0 MUX", "Pass AUDOUT4", "Pass AUDOUT4 Mixer"},
	{"PCM1.1 MUX", "VP CSOUT", "VP CSOUT Mixer"},
	{"PCM1.1 MUX", "VP FEOUT1", "VP FEOUT1 Mixer"},
	{"PCM1.1 MUX", "VP FEOUT2", "VP FEOUT2 Mixer"},
	{"PCM1.1 MUX", "VP MONOUT1", "VP MONOUT1 Mixer"},
	{"PCM1.1 MUX", "VP MONOUT2", "VP MONOUT2 Mixer"},
	{"PCM1.1 MUX", "AudioZoom CSOUT", "AudioZoom CSOUT Mixer"},
	{"PCM1.1 MUX", "AudioZoom AOUT1", "AudioZoom AOUT1 Mixer"},
	{"PCM1.1 MUX", "MM PASSOUT1", "MM PASSOUT1 Mixer"},
	{"PCM1.1 MUX", "MM PASSOUT1", "MM PASSOUT2 Mixer"},
	{"PCM1.1 MUX", "MM AUDOUT1", "MM AUDOUT1 Mixer"},
	{"PCM1.1 MUX", "MM AUDOUT2", "MM AUDOUT2 Mixer"},
	{"PCM1.1 MUX", "MM MONOUT1", "MM MONOUT1 Mixer"},
	{"PCM1.1 MUX", "MM MONOUT2", "MM MONOUT2 Mixer"},
	{"PCM1.1 MUX", "Pass AUDOUT1", "Pass AUDOUT1 Mixer"},
	{"PCM1.1 MUX", "Pass AUDOUT2", "Pass AUDOUT2 Mixer"},
	{"PCM1.1 MUX", "Pass AUDOUT3", "Pass AUDOUT3 Mixer"},
	{"PCM1.1 MUX", "Pass AUDOUT4", "Pass AUDOUT4 Mixer"},
	{"PCM1.2 MUX", "VP CSOUT", "VP CSOUT Mixer"},
	{"PCM1.2 MUX", "VP FEOUT1", "VP FEOUT1 Mixer"},
	{"PCM1.2 MUX", "VP FEOUT2", "VP FEOUT2 Mixer"},
	{"PCM1.2 MUX", "VP MONOUT1", "VP MONOUT1 Mixer"},
	{"PCM1.2 MUX", "VP MONOUT2", "VP MONOUT2 Mixer"},
	{"PCM1.2 MUX", "AudioZoom CSOUT", "AudioZoom CSOUT Mixer"},
	{"PCM1.2 MUX", "AudioZoom AOUT1", "AudioZoom AOUT1 Mixer"},
	{"PCM1.2 MUX", "MM PASSOUT1", "MM PASSOUT1 Mixer"},
	{"PCM1.2 MUX", "MM PASSOUT1", "MM PASSOUT2 Mixer"},
	{"PCM1.2 MUX", "MM AUDOUT1", "MM AUDOUT1 Mixer"},
	{"PCM1.2 MUX", "MM AUDOUT2", "MM AUDOUT2 Mixer"},
	{"PCM1.2 MUX", "MM MONOUT1", "MM MONOUT1 Mixer"},
	{"PCM1.2 MUX", "MM MONOUT2", "MM MONOUT2 Mixer"},
	{"PCM1.2 MUX", "Pass AUDOUT1", "Pass AUDOUT1 Mixer"},
	{"PCM1.2 MUX", "Pass AUDOUT2", "Pass AUDOUT2 Mixer"},
	{"PCM1.2 MUX", "Pass AUDOUT3", "Pass AUDOUT3 Mixer"},
	{"PCM1.2 MUX", "Pass AUDOUT4", "Pass AUDOUT4 Mixer"},
	{"PCM1.3 MUX", "VP CSOUT", "VP CSOUT Mixer"},
	{"PCM1.3 MUX", "VP FEOUT1", "VP FEOUT1 Mixer"},
	{"PCM1.3 MUX", "VP FEOUT2", "VP FEOUT2 Mixer"},
	{"PCM1.3 MUX", "VP MONOUT1", "VP MONOUT1 Mixer"},
	{"PCM1.3 MUX", "VP MONOUT2", "VP MONOUT2 Mixer"},
	{"PCM1.3 MUX", "AudioZoom CSOUT", "AudioZoom CSOUT Mixer"},
	{"PCM1.3 MUX", "AudioZoom AOUT1", "AudioZoom AOUT1 Mixer"},
	{"PCM1.3 MUX", "MM PASSOUT1", "MM PASSOUT1 Mixer"},
	{"PCM1.3 MUX", "MM PASSOUT1", "MM PASSOUT2 Mixer"},
	{"PCM1.3 MUX", "MM AUDOUT1", "MM AUDOUT1 Mixer"},
	{"PCM1.3 MUX", "MM AUDOUT2", "MM AUDOUT2 Mixer"},
	{"PCM1.3 MUX", "MM MONOUT1", "MM MONOUT1 Mixer"},
	{"PCM1.3 MUX", "MM MONOUT2", "MM MONOUT2 Mixer"},
	{"PCM1.3 MUX", "Pass AUDOUT1", "Pass AUDOUT1 Mixer"},
	{"PCM1.3 MUX", "Pass AUDOUT2", "Pass AUDOUT2 Mixer"},
	{"PCM1.3 MUX", "Pass AUDOUT3", "Pass AUDOUT3 Mixer"},
	{"PCM1.3 MUX", "Pass AUDOUT4", "Pass AUDOUT4 Mixer"},

	{"PCM2.0 MUX", "VP CSOUT", "VP CSOUT Mixer"},
	{"PCM2.0 MUX", "VP FEOUT1", "VP FEOUT1 Mixer"},
	{"PCM2.0 MUX", "VP FEOUT2", "VP FEOUT2 Mixer"},
	{"PCM2.0 MUX", "VP MONOUT1", "VP MONOUT1 Mixer"},
	{"PCM2.0 MUX", "VP MONOUT2", "VP MONOUT2 Mixer"},
	{"PCM2.0 MUX", "AudioZoom CSOUT", "AudioZoom CSOUT Mixer"},
	{"PCM2.0 MUX", "AudioZoom AOUT1", "AudioZoom AOUT1 Mixer"},
	{"PCM2.0 MUX", "MM PASSOUT1", "MM PASSOUT1 Mixer"},
	{"PCM2.0 MUX", "MM PASSOUT1", "MM PASSOUT2 Mixer"},
	{"PCM2.0 MUX", "MM AUDOUT1", "MM AUDOUT1 Mixer"},
	{"PCM2.0 MUX", "MM AUDOUT2", "MM AUDOUT2 Mixer"},
	{"PCM2.0 MUX", "MM MONOUT1", "MM MONOUT1 Mixer"},
	{"PCM2.0 MUX", "MM MONOUT2", "MM MONOUT2 Mixer"},
	{"PCM2.0 MUX", "Pass AUDOUT1", "Pass AUDOUT1 Mixer"},
	{"PCM2.0 MUX", "Pass AUDOUT2", "Pass AUDOUT2 Mixer"},
	{"PCM2.0 MUX", "Pass AUDOUT3", "Pass AUDOUT3 Mixer"},
	{"PCM2.0 MUX", "Pass AUDOUT4", "Pass AUDOUT4 Mixer"},
	{"PCM2.1 MUX", "VP CSOUT", "VP CSOUT Mixer"},
	{"PCM2.1 MUX", "VP FEOUT1", "VP FEOUT1 Mixer"},
	{"PCM2.1 MUX", "VP FEOUT2", "VP FEOUT2 Mixer"},
	{"PCM2.1 MUX", "VP MONOUT1", "VP MONOUT1 Mixer"},
	{"PCM2.1 MUX", "VP MONOUT2", "VP MONOUT2 Mixer"},
	{"PCM2.1 MUX", "AudioZoom CSOUT", "AudioZoom CSOUT Mixer"},
	{"PCM2.1 MUX", "AudioZoom AOUT1", "AudioZoom AOUT1 Mixer"},
	{"PCM2.1 MUX", "MM PASSOUT1", "MM PASSOUT1 Mixer"},
	{"PCM2.1 MUX", "MM PASSOUT1", "MM PASSOUT2 Mixer"},
	{"PCM2.1 MUX", "MM AUDOUT1", "MM AUDOUT1 Mixer"},
	{"PCM2.1 MUX", "MM AUDOUT2", "MM AUDOUT2 Mixer"},
	{"PCM2.1 MUX", "MM MONOUT1", "MM MONOUT1 Mixer"},
	{"PCM2.1 MUX", "MM MONOUT2", "MM MONOUT2 Mixer"},
	{"PCM2.1 MUX", "Pass AUDOUT1", "Pass AUDOUT1 Mixer"},
	{"PCM2.1 MUX", "Pass AUDOUT2", "Pass AUDOUT2 Mixer"},
	{"PCM2.1 MUX", "Pass AUDOUT3", "Pass AUDOUT3 Mixer"},
	{"PCM2.1 MUX", "Pass AUDOUT4", "Pass AUDOUT4 Mixer"},
	{"PCM2.2 MUX", "VP CSOUT", "VP CSOUT Mixer"},
	{"PCM2.2 MUX", "VP FEOUT1", "VP FEOUT1 Mixer"},
	{"PCM2.2 MUX", "VP FEOUT2", "VP FEOUT2 Mixer"},
	{"PCM2.2 MUX", "VP MONOUT1", "VP MONOUT1 Mixer"},
	{"PCM2.2 MUX", "VP MONOUT2", "VP MONOUT2 Mixer"},
	{"PCM2.2 MUX", "AudioZoom CSOUT", "AudioZoom CSOUT Mixer"},
	{"PCM2.2 MUX", "AudioZoom AOUT1", "AudioZoom AOUT1 Mixer"},
	{"PCM2.2 MUX", "MM PASSOUT1", "MM PASSOUT1 Mixer"},
	{"PCM2.2 MUX", "MM PASSOUT1", "MM PASSOUT2 Mixer"},
	{"PCM2.2 MUX", "MM AUDOUT1", "MM AUDOUT1 Mixer"},
	{"PCM2.2 MUX", "MM AUDOUT2", "MM AUDOUT2 Mixer"},
	{"PCM2.2 MUX", "MM MONOUT1", "MM MONOUT1 Mixer"},
	{"PCM2.2 MUX", "MM MONOUT2", "MM MONOUT2 Mixer"},
	{"PCM2.2 MUX", "Pass AUDOUT1", "Pass AUDOUT1 Mixer"},
	{"PCM2.2 MUX", "Pass AUDOUT2", "Pass AUDOUT2 Mixer"},
	{"PCM2.2 MUX", "Pass AUDOUT3", "Pass AUDOUT3 Mixer"},
	{"PCM2.2 MUX", "Pass AUDOUT4", "Pass AUDOUT4 Mixer"},
	{"PCM2.3 MUX", "VP CSOUT", "VP CSOUT Mixer"},
	{"PCM2.3 MUX", "VP FEOUT1", "VP FEOUT1 Mixer"},
	{"PCM2.3 MUX", "VP FEOUT2", "VP FEOUT2 Mixer"},
	{"PCM2.3 MUX", "VP MONOUT1", "VP MONOUT1 Mixer"},
	{"PCM2.3 MUX", "VP MONOUT2", "VP MONOUT2 Mixer"},
	{"PCM2.3 MUX", "AudioZoom CSOUT", "AudioZoom CSOUT Mixer"},
	{"PCM2.3 MUX", "AudioZoom AOUT1", "AudioZoom AOUT1 Mixer"},
	{"PCM2.3 MUX", "MM PASSOUT1", "MM PASSOUT1 Mixer"},
	{"PCM2.3 MUX", "MM PASSOUT1", "MM PASSOUT2 Mixer"},
	{"PCM2.3 MUX", "MM AUDOUT1", "MM AUDOUT1 Mixer"},
	{"PCM2.3 MUX", "MM AUDOUT2", "MM AUDOUT2 Mixer"},
	{"PCM2.3 MUX", "MM MONOUT1", "MM MONOUT1 Mixer"},
	{"PCM2.3 MUX", "MM MONOUT2", "MM MONOUT2 Mixer"},
	{"PCM2.3 MUX", "Pass AUDOUT1", "Pass AUDOUT1 Mixer"},
	{"PCM2.3 MUX", "Pass AUDOUT2", "Pass AUDOUT2 Mixer"},
	{"PCM2.3 MUX", "Pass AUDOUT3", "Pass AUDOUT3 Mixer"},
	{"PCM2.3 MUX", "Pass AUDOUT4", "Pass AUDOUT4 Mixer"},


	{"SBUS.TX0 MUX", "VP CSOUT", "VP CSOUT Mixer"},
	{"SBUS.TX0 MUX", "VP FEOUT1", "VP FEOUT1 Mixer"},
	{"SBUS.TX0 MUX", "VP FEOUT2", "VP FEOUT2 Mixer"},
	{"SBUS.TX0 MUX", "VP MONOUT1", "VP MONOUT1 Mixer"},
	{"SBUS.TX0 MUX", "VP MONOUT2", "VP MONOUT2 Mixer"},
	{"SBUS.TX0 MUX", "AudioZoom CSOUT", "AudioZoom CSOUT Mixer"},
	{"SBUS.TX0 MUX", "AudioZoom AOUT1", "AudioZoom AOUT1 Mixer"},
	{"SBUS.TX0 MUX", "MM PASSOUT1", "MM PASSOUT1 Mixer"},
	{"SBUS.TX0 MUX", "MM PASSOUT1", "MM PASSOUT2 Mixer"},
	{"SBUS.TX0 MUX", "MM AUDOUT1", "MM AUDOUT1 Mixer"},
	{"SBUS.TX0 MUX", "MM AUDOUT2", "MM AUDOUT2 Mixer"},
	{"SBUS.TX0 MUX", "MM MONOUT1", "MM MONOUT1 Mixer"},
	{"SBUS.TX0 MUX", "MM MONOUT2", "MM MONOUT2 Mixer"},
	{"SBUS.TX0 MUX", "Pass AUDOUT1", "Pass AUDOUT1 Mixer"},
	{"SBUS.TX0 MUX", "Pass AUDOUT2", "Pass AUDOUT2 Mixer"},
	{"SBUS.TX0 MUX", "Pass AUDOUT3", "Pass AUDOUT3 Mixer"},
	{"SBUS.TX0 MUX", "Pass AUDOUT4", "Pass AUDOUT4 Mixer"},

	{"SBUS.TX1 MUX", "VP CSOUT", "VP CSOUT Mixer"},
	{"SBUS.TX1 MUX", "VP FEOUT1", "VP FEOUT1 Mixer"},
	{"SBUS.TX1 MUX", "VP FEOUT2", "VP FEOUT2 Mixer"},
	{"SBUS.TX1 MUX", "VP MONOUT1", "VP MONOUT1 Mixer"},
	{"SBUS.TX1 MUX", "VP MONOUT2", "VP MONOUT2 Mixer"},
	{"SBUS.TX1 MUX", "AudioZoom CSOUT", "AudioZoom CSOUT Mixer"},
	{"SBUS.TX1 MUX", "AudioZoom AOUT1", "AudioZoom AOUT1 Mixer"},
	{"SBUS.TX1 MUX", "MM PASSOUT1", "MM PASSOUT1 Mixer"},
	{"SBUS.TX1 MUX", "MM PASSOUT1", "MM PASSOUT2 Mixer"},
	{"SBUS.TX1 MUX", "MM AUDOUT1", "MM AUDOUT1 Mixer"},
	{"SBUS.TX1 MUX", "MM AUDOUT2", "MM AUDOUT2 Mixer"},
	{"SBUS.TX1 MUX", "MM MONOUT1", "MM MONOUT1 Mixer"},
	{"SBUS.TX1 MUX", "MM MONOUT2", "MM MONOUT2 Mixer"},
	{"SBUS.TX1 MUX", "Pass AUDOUT1", "Pass AUDOUT1 Mixer"},
	{"SBUS.TX1 MUX", "Pass AUDOUT2", "Pass AUDOUT2 Mixer"},
	{"SBUS.TX1 MUX", "Pass AUDOUT3", "Pass AUDOUT3 Mixer"},
	{"SBUS.TX1 MUX", "Pass AUDOUT4", "Pass AUDOUT4 Mixer"},

	{"SBUS.TX2 MUX", "VP CSOUT", "VP CSOUT Mixer"},
	{"SBUS.TX2 MUX", "VP FEOUT1", "VP FEOUT1 Mixer"},
	{"SBUS.TX2 MUX", "VP FEOUT2", "VP FEOUT2 Mixer"},
	{"SBUS.TX2 MUX", "VP MONOUT1", "VP MONOUT1 Mixer"},
	{"SBUS.TX2 MUX", "VP MONOUT2", "VP MONOUT2 Mixer"},
	{"SBUS.TX2 MUX", "AudioZoom CSOUT", "AudioZoom CSOUT Mixer"},
	{"SBUS.TX2 MUX", "AudioZoom AOUT1", "AudioZoom AOUT1 Mixer"},
	{"SBUS.TX2 MUX", "MM PASSOUT1", "MM PASSOUT1 Mixer"},
	{"SBUS.TX2 MUX", "MM PASSOUT1", "MM PASSOUT2 Mixer"},
	{"SBUS.TX2 MUX", "MM AUDOUT1", "MM AUDOUT1 Mixer"},
	{"SBUS.TX2 MUX", "MM AUDOUT2", "MM AUDOUT2 Mixer"},
	{"SBUS.TX2 MUX", "MM MONOUT1", "MM MONOUT1 Mixer"},
	{"SBUS.TX2 MUX", "MM MONOUT2", "MM MONOUT2 Mixer"},
	{"SBUS.TX2 MUX", "Pass AUDOUT1", "Pass AUDOUT1 Mixer"},
	{"SBUS.TX2 MUX", "Pass AUDOUT2", "Pass AUDOUT2 Mixer"},
	{"SBUS.TX2 MUX", "Pass AUDOUT3", "Pass AUDOUT3 Mixer"},
	{"SBUS.TX2 MUX", "Pass AUDOUT4", "Pass AUDOUT4 Mixer"},

	{"SBUS.TX3 MUX", "VP CSOUT", "VP CSOUT Mixer"},
	{"SBUS.TX3 MUX", "VP FEOUT1", "VP FEOUT1 Mixer"},
	{"SBUS.TX3 MUX", "VP FEOUT2", "VP FEOUT2 Mixer"},
	{"SBUS.TX3 MUX", "VP MONOUT1", "VP MONOUT1 Mixer"},
	{"SBUS.TX3 MUX", "VP MONOUT2", "VP MONOUT2 Mixer"},
	{"SBUS.TX3 MUX", "AudioZoom CSOUT", "AudioZoom CSOUT Mixer"},
	{"SBUS.TX3 MUX", "AudioZoom AOUT1", "AudioZoom AOUT1 Mixer"},
	{"SBUS.TX3 MUX", "MM PASSOUT1", "MM PASSOUT1 Mixer"},
	{"SBUS.TX3 MUX", "MM PASSOUT1", "MM PASSOUT2 Mixer"},
	{"SBUS.TX3 MUX", "MM AUDOUT1", "MM AUDOUT1 Mixer"},
	{"SBUS.TX3 MUX", "MM AUDOUT2", "MM AUDOUT2 Mixer"},
	{"SBUS.TX3 MUX", "MM MONOUT1", "MM MONOUT1 Mixer"},
	{"SBUS.TX3 MUX", "MM MONOUT2", "MM MONOUT2 Mixer"},
	{"SBUS.TX3 MUX", "Pass AUDOUT1", "Pass AUDOUT1 Mixer"},
	{"SBUS.TX3 MUX", "Pass AUDOUT2", "Pass AUDOUT2 Mixer"},
	{"SBUS.TX3 MUX", "Pass AUDOUT3", "Pass AUDOUT3 Mixer"},
	{"SBUS.TX3 MUX", "Pass AUDOUT4", "Pass AUDOUT4 Mixer"},

	{"SBUS.TX4 MUX", "VP CSOUT", "VP CSOUT Mixer"},
	{"SBUS.TX4 MUX", "VP FEOUT1", "VP FEOUT1 Mixer"},
	{"SBUS.TX4 MUX", "VP FEOUT2", "VP FEOUT2 Mixer"},
	{"SBUS.TX4 MUX", "VP MONOUT1", "VP MONOUT1 Mixer"},
	{"SBUS.TX4 MUX", "VP MONOUT2", "VP MONOUT2 Mixer"},
	{"SBUS.TX4 MUX", "AudioZoom CSOUT", "AudioZoom CSOUT Mixer"},
	{"SBUS.TX4 MUX", "AudioZoom AOUT1", "AudioZoom AOUT1 Mixer"},
	{"SBUS.TX4 MUX", "MM PASSOUT1", "MM PASSOUT1 Mixer"},
	{"SBUS.TX4 MUX", "MM PASSOUT1", "MM PASSOUT2 Mixer"},
	{"SBUS.TX4 MUX", "MM AUDOUT1", "MM AUDOUT1 Mixer"},
	{"SBUS.TX4 MUX", "MM AUDOUT2", "MM AUDOUT2 Mixer"},
	{"SBUS.TX4 MUX", "MM MONOUT1", "MM MONOUT1 Mixer"},
	{"SBUS.TX4 MUX", "MM MONOUT2", "MM MONOUT2 Mixer"},
	{"SBUS.TX4 MUX", "Pass AUDOUT1", "Pass AUDOUT1 Mixer"},
	{"SBUS.TX4 MUX", "Pass AUDOUT2", "Pass AUDOUT2 Mixer"},
	{"SBUS.TX4 MUX", "Pass AUDOUT3", "Pass AUDOUT3 Mixer"},
	{"SBUS.TX4 MUX", "Pass AUDOUT4", "Pass AUDOUT4 Mixer"},

	{"SBUS.TX5 MUX", "VP CSOUT", "VP CSOUT Mixer"},
	{"SBUS.TX5 MUX", "VP FEOUT1", "VP FEOUT1 Mixer"},
	{"SBUS.TX5 MUX", "VP FEOUT2", "VP FEOUT2 Mixer"},
	{"SBUS.TX5 MUX", "VP MONOUT1", "VP MONOUT1 Mixer"},
	{"SBUS.TX5 MUX", "VP MONOUT2", "VP MONOUT2 Mixer"},
	{"SBUS.TX5 MUX", "AudioZoom CSOUT", "AudioZoom CSOUT Mixer"},
	{"SBUS.TX5 MUX", "AudioZoom AOUT1", "AudioZoom AOUT1 Mixer"},
	{"SBUS.TX5 MUX", "MM PASSOUT1", "MM PASSOUT1 Mixer"},
	{"SBUS.TX5 MUX", "MM PASSOUT1", "MM PASSOUT2 Mixer"},
	{"SBUS.TX5 MUX", "MM AUDOUT1", "MM AUDOUT1 Mixer"},
	{"SBUS.TX5 MUX", "MM AUDOUT2", "MM AUDOUT2 Mixer"},
	{"SBUS.TX5 MUX", "MM MONOUT1", "MM MONOUT1 Mixer"},
	{"SBUS.TX5 MUX", "MM MONOUT2", "MM MONOUT2 Mixer"},
	{"SBUS.TX5 MUX", "Pass AUDOUT1", "Pass AUDOUT1 Mixer"},
	{"SBUS.TX5 MUX", "Pass AUDOUT2", "Pass AUDOUT2 Mixer"},
	{"SBUS.TX5 MUX", "Pass AUDOUT3", "Pass AUDOUT3 Mixer"},
	{"SBUS.TX5 MUX", "Pass AUDOUT4", "Pass AUDOUT4 Mixer"},

	{"DAC0.0 MUX", "VP CSOUT", "VP CSOUT Mixer"},
	{"DAC0.0 MUX", "VP FEOUT1", "VP FEOUT1 Mixer"},
	{"DAC0.0 MUX", "VP FEOUT2", "VP FEOUT2 Mixer"},
	{"DAC0.0 MUX", "VP MONOUT1", "VP MONOUT1 Mixer"},
	{"DAC0.0 MUX", "VP MONOUT2", "VP MONOUT2 Mixer"},
	{"DAC0.0 MUX", "AudioZoom CSOUT", "AudioZoom CSOUT Mixer"},
	{"DAC0.0 MUX", "AudioZoom AOUT1", "AudioZoom AOUT1 Mixer"},
	{"DAC0.0 MUX", "MM PASSOUT1", "MM PASSOUT1 Mixer"},
	{"DAC0.0 MUX", "MM PASSOUT1", "MM PASSOUT2 Mixer"},
	{"DAC0.0 MUX", "MM AUDOUT1", "MM AUDOUT1 Mixer"},
	{"DAC0.0 MUX", "MM AUDOUT2", "MM AUDOUT2 Mixer"},
	{"DAC0.0 MUX", "MM MONOUT1", "MM MONOUT1 Mixer"},
	{"DAC0.0 MUX", "MM MONOUT2", "MM MONOUT2 Mixer"},
	{"DAC0.0 MUX", "Pass AUDOUT1", "Pass AUDOUT1 Mixer"},
	{"DAC0.0 MUX", "Pass AUDOUT2", "Pass AUDOUT2 Mixer"},
	{"DAC0.0 MUX", "Pass AUDOUT3", "Pass AUDOUT3 Mixer"},
	{"DAC0.0 MUX", "Pass AUDOUT4", "Pass AUDOUT4 Mixer"},
	{"DAC0.1 MUX", "VP CSOUT", "VP CSOUT Mixer"},
	{"DAC0.1 MUX", "VP FEOUT1", "VP FEOUT1 Mixer"},
	{"DAC0.1 MUX", "VP FEOUT2", "VP FEOUT2 Mixer"},
	{"DAC0.1 MUX", "VP MONOUT1", "VP MONOUT1 Mixer"},
	{"DAC0.1 MUX", "VP MONOUT2", "VP MONOUT2 Mixer"},
	{"DAC0.1 MUX", "AudioZoom CSOUT", "AudioZoom CSOUT Mixer"},
	{"DAC0.1 MUX", "AudioZoom AOUT1", "AudioZoom AOUT1 Mixer"},
	{"DAC0.1 MUX", "MM PASSOUT1", "MM PASSOUT1 Mixer"},
	{"DAC0.1 MUX", "MM PASSOUT1", "MM PASSOUT2 Mixer"},
	{"DAC0.1 MUX", "MM AUDOUT1", "MM AUDOUT1 Mixer"},
	{"DAC0.1 MUX", "MM AUDOUT2", "MM AUDOUT2 Mixer"},
	{"DAC0.1 MUX", "MM MONOUT1", "MM MONOUT1 Mixer"},
	{"DAC0.1 MUX", "MM MONOUT2", "MM MONOUT2 Mixer"},
	{"DAC0.1 MUX", "Pass AUDOUT1", "Pass AUDOUT1 Mixer"},
	{"DAC0.1 MUX", "Pass AUDOUT2", "Pass AUDOUT2 Mixer"},
	{"DAC0.1 MUX", "Pass AUDOUT3", "Pass AUDOUT3 Mixer"},
	{"DAC0.1 MUX", "Pass AUDOUT4", "Pass AUDOUT4 Mixer"},

	{"DAC1.0 MUX", "VP CSOUT", "VP CSOUT Mixer"},
	{"DAC1.0 MUX", "VP FEOUT1", "VP FEOUT1 Mixer"},
	{"DAC1.0 MUX", "VP FEOUT2", "VP FEOUT2 Mixer"},
	{"DAC1.0 MUX", "VP MONOUT1", "VP MONOUT1 Mixer"},
	{"DAC1.0 MUX", "VP MONOUT2", "VP MONOUT2 Mixer"},
	{"DAC1.0 MUX", "AudioZoom CSOUT", "AudioZoom CSOUT Mixer"},
	{"DAC1.0 MUX", "AudioZoom AOUT1", "AudioZoom AOUT1 Mixer"},
	{"DAC1.0 MUX", "MM PASSOUT1", "MM PASSOUT1 Mixer"},
	{"DAC1.0 MUX", "MM PASSOUT1", "MM PASSOUT2 Mixer"},
	{"DAC1.0 MUX", "MM AUDOUT1", "MM AUDOUT1 Mixer"},
	{"DAC1.0 MUX", "MM AUDOUT2", "MM AUDOUT2 Mixer"},
	{"DAC1.0 MUX", "MM MONOUT1", "MM MONOUT1 Mixer"},
	{"DAC1.0 MUX", "MM MONOUT2", "MM MONOUT2 Mixer"},
	{"DAC1.0 MUX", "Pass AUDOUT1", "Pass AUDOUT1 Mixer"},
	{"DAC1.0 MUX", "Pass AUDOUT2", "Pass AUDOUT2 Mixer"},
	{"DAC1.0 MUX", "Pass AUDOUT3", "Pass AUDOUT3 Mixer"},
	{"DAC1.0 MUX", "Pass AUDOUT4", "Pass AUDOUT4 Mixer"},
	{"DAC1.1 MUX", "VP CSOUT", "VP CSOUT Mixer"},
	{"DAC1.1 MUX", "VP FEOUT1", "VP FEOUT1 Mixer"},
	{"DAC1.1 MUX", "VP FEOUT2", "VP FEOUT2 Mixer"},
	{"DAC1.1 MUX", "VP MONOUT1", "VP MONOUT1 Mixer"},
	{"DAC1.1 MUX", "VP MONOUT2", "VP MONOUT2 Mixer"},
	{"DAC1.1 MUX", "AudioZoom CSOUT", "AudioZoom CSOUT Mixer"},
	{"DAC1.1 MUX", "AudioZoom AOUT1", "AudioZoom AOUT1 Mixer"},
	{"DAC1.1 MUX", "MM PASSOUT1", "MM PASSOUT1 Mixer"},
	{"DAC1.1 MUX", "MM PASSOUT1", "MM PASSOUT2 Mixer"},
	{"DAC1.1 MUX", "MM AUDOUT1", "MM AUDOUT1 Mixer"},
	{"DAC1.1 MUX", "MM AUDOUT2", "MM AUDOUT2 Mixer"},
	{"DAC1.1 MUX", "MM MONOUT1", "MM MONOUT1 Mixer"},
	{"DAC1.1 MUX", "MM MONOUT2", "MM MONOUT2 Mixer"},
	{"DAC1.1 MUX", "Pass AUDOUT1", "Pass AUDOUT1 Mixer"},
	{"DAC1.1 MUX", "Pass AUDOUT2", "Pass AUDOUT2 Mixer"},
	{"DAC1.1 MUX", "Pass AUDOUT3", "Pass AUDOUT3 Mixer"},
	{"DAC1.1 MUX", "Pass AUDOUT4", "Pass AUDOUT4 Mixer"},

	/* AIF TX <--> PCM PORTA  */

	{"PCM0 TX", NULL, "PCM0.0 MUX"},
	{"PCM0 TX", NULL, "PCM0.1 MUX"},
	{"PCM0 TX", NULL, "PCM0.2 MUX"},
	{"PCM0 TX", NULL, "PCM0.3 MUX"},

	/* Digital PCM Port A <--> PCM PORTA MUX */

	{"PCM PORT A", NULL, "PCM0.0 MUX"},
	{"PCM PORT A", NULL, "PCM0.1 MUX"},
	{"PCM PORT A", NULL, "PCM0.2 MUX"},
	{"PCM PORT A", NULL, "PCM0.3 MUX"},


	/* AIF TX <--> PCM PORTB  */

	{"PCM1 TX", NULL, "PCM1.0 MUX"},
	{"PCM1 TX", NULL, "PCM1.1 MUX"},
	{"PCM1 TX", NULL, "PCM1.2 MUX"},
	{"PCM1 TX", NULL, "PCM1.3 MUX"},

	/* Digital PCM Port B <--> PCM PORTB MUX */

	{"PCM PORT B", NULL, "PCM1.0 MUX"},
	{"PCM PORT B", NULL, "PCM1.1 MUX"},
	{"PCM PORT B", NULL, "PCM1.2 MUX"},
	{"PCM PORT B", NULL, "PCM1.3 MUX"},

	/* AIF TX <--> PCM PORTC  */

	{"PCM2 TX", NULL, "PCM2.0 MUX"},
	{"PCM2 TX", NULL, "PCM2.1 MUX"},
	{"PCM2 TX", NULL, "PCM2.2 MUX"},
	{"PCM2 TX", NULL, "PCM2.3 MUX"},

	/* Digital PCM Port C <--> PCM PORTC MUX */

	{"PCM PORT C", NULL, "PCM2.0 MUX"},
	{"PCM PORT C", NULL, "PCM2.1 MUX"},
	{"PCM PORT C", NULL, "PCM2.2 MUX"},
	{"PCM PORT C", NULL, "PCM2.3 MUX"},

	/* AIF TX <--> SBUS.TX */

	{"SBUS.TX0", NULL, "SBUS.TX0 MUX"},
	{"SBUS.TX1", NULL, "SBUS.TX1 MUX"},
	{"SBUS.TX2", NULL, "SBUS.TX2 MUX"},
	{"SBUS.TX3", NULL, "SBUS.TX3 MUX"},
	{"SBUS.TX4", NULL, "SBUS.TX4 MUX"},
	{"SBUS.TX5", NULL, "SBUS.TX5 MUX"},

	/* A300 DAC <--> D300 RX  */

	{"DAC0L", NULL, "DAC0.0 MUX"},
	{"DAC0R", NULL, "DAC0.1 MUX"},
	{"DAC1L", NULL, "DAC1.0 MUX"},
	{"DAC1R", NULL, "DAC1.1 MUX"},
};

void es_d300_reset_cmdcache(void)
{
	int i;
	for (i = 0; i < ALGO_MAX; i++) {
		memset(cachedcmd_list[i], 0x0,
				ES_API_ADDR_MAX * sizeof(u8));
	}
}

int es_d300_fill_cmdcache(struct snd_soc_codec *codec)
{
	u8 i;
	const char *algo_text[ALGO_MAX] = {
		[VP] = "VP",
		[MM] = "MM",
		[AudioZoom] = "AudioZoom",
		[Passthru] = "Passthru",
	};

	cachedcmd_list = kzalloc(ALGO_MAX*sizeof(u8 *), GFP_KERNEL);
	if (!cachedcmd_list) {
		pr_err("%s(): Error while allocating regcache\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < ALGO_MAX; i++) {
		cachedcmd_list[i] = kzalloc(ES_API_ADDR_MAX*sizeof(u8),
				GFP_KERNEL);
		if (!cachedcmd_list[i]) {
			pr_err("%s(): Error while allocating regcache for %s\n",
					__func__, algo_text[i]);
			return -ENOMEM;
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(es_d300_fill_cmdcache);

int es_d300_add_snd_soc_controls(struct snd_soc_codec *codec)
{
	int rc;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
	rc = snd_soc_add_codec_controls(codec, es_d300_snd_controls,
			ARRAY_SIZE(es_d300_snd_controls));
#else
	rc = snd_soc_add_controls(codec, es_d300_snd_controls,
			ARRAY_SIZE(es_d300_snd_controls));
#endif

	return rc;
}

int es_d300_add_snd_soc_dapm_controls(struct snd_soc_codec *codec)
{
	int rc;

	rc = snd_soc_dapm_new_controls(&codec->dapm, es_d300_dapm_widgets,
			ARRAY_SIZE(es_d300_dapm_widgets));

	return rc;
}

int es_d300_add_snd_soc_route_map(struct snd_soc_codec *codec)
{
	int rc;

	rc = snd_soc_dapm_add_routes(&codec->dapm, intercon,
			ARRAY_SIZE(intercon));

	return rc;
}
