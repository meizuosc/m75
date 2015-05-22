
/*
 * es-a300.c  --  Audience eS755 ALSA SoC Audio driver
 *
 * Copyright 2013 Audience, Inc.
 *
 * Author: Rajat Aggarwal <raggarwal@audience.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include "escore.h"
#include "es-a300-reg.h"

static DECLARE_TLV_DB_SCALE(spkr_tlv, -3600, 200, 1);
static DECLARE_TLV_DB_SCALE(lo_tlv, -2200, 200, 1);
static DECLARE_TLV_DB_SCALE(ep_tlv, -2200, 200, 1);
static DECLARE_TLV_DB_SCALE(aux_tlv, 0, 150, 0);
static DECLARE_TLV_DB_SCALE(mic_tlv, 0, 150, 0);

static const unsigned int hp_tlv[] = {
	TLV_DB_RANGE_HEAD(6),
	0, 0, TLV_DB_SCALE_ITEM(-6000, 0, 0),
	1, 1, TLV_DB_SCALE_ITEM(-5100, 0, 0),
	2, 2, TLV_DB_SCALE_ITEM(-4300, 0, 0),
	3, 5, TLV_DB_SCALE_ITEM(-3700, 500, 0),
	6, 11, TLV_DB_SCALE_ITEM(-2300, 300, 0),
	12, 15, TLV_DB_SCALE_ITEM(-600, 200, 0),
};

static const char * const aux_mono_text[] = {
	"Mono", "Stereo"
};

static const struct soc_enum auxl_mono_enum =
	SOC_ENUM_SINGLE(ES_AUX_L_CTRL, ES_AUX_STEREO_SHIFT,
		ARRAY_SIZE(aux_mono_text), aux_mono_text);

static const struct soc_enum auxr_mono_enum =
	SOC_ENUM_SINGLE(ES_AUX_R_CTRL, ES_AUX_STEREO_SHIFT,
		ARRAY_SIZE(aux_mono_text), aux_mono_text);

static const char * const i2s_ch_text[] = {
	"2", "4",
};

static const struct soc_enum i2s_ch_enum =
	SOC_ENUM_SINGLE(ES_DAC_DIG_CH, ES_I2S_CH_SHIFT,
		ARRAY_SIZE(i2s_ch_text), i2s_ch_text);

static const char * const fs_sel_text[] = {
	"48", "96", "192",
};

static const struct soc_enum fs_sel_enum =
	SOC_ENUM_SINGLE(ES_DAC_DIG_FS_SEL, ES_FS_SEL_SHIFT,
		ARRAY_SIZE(fs_sel_text), fs_sel_text);

static const char * const michs_sel_mux_text[] = {
	"MIC0_PGA", "MIC1_PGA",
};

static const struct soc_enum michs_sel_mux_enum =
	SOC_ENUM_SINGLE(ES_MICHS_CTRL, ES_MICHS_IN_SEL_SHIFT,
		ARRAY_SIZE(michs_sel_mux_text), michs_sel_mux_text);

static const char * const micx_input_type_text[] = {
	"Differential", "Single Ended",
};

static const struct soc_enum mic0_input_type_enum =
	SOC_ENUM_SINGLE(ES_MIC0_CTRL, ES_MIC0_SE_SHIFT,
		ARRAY_SIZE(micx_input_type_text), micx_input_type_text);

static const struct soc_enum mic1_input_type_enum =
	SOC_ENUM_SINGLE(ES_MIC1_CTRL, ES_MIC1_SE_SHIFT,
		ARRAY_SIZE(micx_input_type_text), micx_input_type_text);

static const struct soc_enum mic2_input_type_enum =
	SOC_ENUM_SINGLE(ES_MIC2_CTRL, ES_MIC2_SE_SHIFT,
		ARRAY_SIZE(micx_input_type_text), micx_input_type_text);

static const struct soc_enum michs_input_type_enum =
	SOC_ENUM_SINGLE(ES_MICHS_CTRL, ES_MICHS_SE_SHIFT,
		ARRAY_SIZE(micx_input_type_text), micx_input_type_text);

static const struct soc_enum auxin_input_type_enum =
	SOC_ENUM_SINGLE(ES_AUX_R_CTRL, ES_AUXR_SE_SHIFT,
		ARRAY_SIZE(micx_input_type_text), micx_input_type_text);

static const char * const micx_bias_output_voltage_text[] = {
	"1.6V", "1.8V", "2.0V", "2.2V", "2.4V", "2.6V", "2.8V", "3.0V",
};

static const unsigned int micx_bias_output_voltage_value[] = {
	0, 1, 2, 3, 4, 5, 6, 7,
};

static const struct soc_enum mic0_bias_output_voltage_enum =
	SOC_VALUE_ENUM_SINGLE(ES_MB_TRIM2, ES_MB0_TRIM_SHIFT,
		ES_MB0_TRIM_MASK, ARRAY_SIZE(micx_bias_output_voltage_text),
			micx_bias_output_voltage_text,
			micx_bias_output_voltage_value);

static const struct soc_enum mic1_bias_output_voltage_enum =
	SOC_VALUE_ENUM_SINGLE(ES_MB_TRIM2, ES_MB1_TRIM_SHIFT,
		ES_MB1_TRIM_MASK, ARRAY_SIZE(micx_bias_output_voltage_text),
			micx_bias_output_voltage_text,
			micx_bias_output_voltage_value);

static const struct soc_enum mic2_bias_output_voltage_enum =
	SOC_VALUE_ENUM_SINGLE(ES_MB_TRIM1, ES_MB2_TRIM_SHIFT,
		ES_MB2_TRIM_MASK, ARRAY_SIZE(micx_bias_output_voltage_text),
			micx_bias_output_voltage_text,
			micx_bias_output_voltage_value);

static const struct soc_enum michs_bias_output_voltage_enum =
	SOC_VALUE_ENUM_SINGLE(ES_MB_TRIM1, ES_MBHS_TRIM_SHIFT,
		ES_MBHS_TRIM_MASK, ARRAY_SIZE(micx_bias_output_voltage_text),
			micx_bias_output_voltage_text,
			micx_bias_output_voltage_value);

static const char * const micx_zin_mode_text[] = {
	"100kohm", "50kohm", "25kohm", "Attenuate by 3dB",
};

static const struct soc_enum mic0_zin_mode_enum =
	SOC_ENUM_SINGLE(ES_MIC_TUNE, ES_MIC0_ZIN_MODE_SHIFT,
		ARRAY_SIZE(micx_zin_mode_text), micx_zin_mode_text);

static const struct soc_enum mic1_zin_mode_enum =
	SOC_ENUM_SINGLE(ES_MIC_TUNE, ES_MIC1_ZIN_MODE_SHIFT,
		ARRAY_SIZE(micx_zin_mode_text), micx_zin_mode_text);

static const struct soc_enum mic2_zin_mode_enum =
	SOC_ENUM_SINGLE(ES_MIC_TUNE, ES_MIC2_ZIN_MODE_SHIFT,
		ARRAY_SIZE(micx_zin_mode_text), micx_zin_mode_text);

static const struct soc_enum michs_zin_mode_enum =
	SOC_ENUM_SINGLE(ES_MIC_TUNE, ES_MICHS_ZIN_MODE_SHIFT,
		ARRAY_SIZE(micx_zin_mode_text), micx_zin_mode_text);

static const char * const bps_text[] = {
	"16", "20", "24", "32",
};

static const unsigned int bps_value_text[] = {
	15, 19, 23, 31,
};

static const struct soc_enum bps_enum =
	SOC_VALUE_ENUM_SINGLE(ES_DAC_DIG_I2S1, ES_BITS_PER_SAMPLE_SHIFT,
		ES_BITS_PER_SAMPLE_MASK, ARRAY_SIZE(bps_text),
			bps_text, bps_value_text);

const struct snd_kcontrol_new es_codec_snd_controls[] = {

	SOC_DOUBLE_R_TLV("SPKR Gain", ES_SPKR_L_GAIN, ES_SPKR_R_GAIN,
			0, 0x1F, 0, spkr_tlv),
	SOC_DOUBLE_R_TLV("LO Gain", ES_LO_L_GAIN, ES_LO_R_GAIN,
			0, 0x0F, 0, lo_tlv),
	SOC_DOUBLE_R_TLV("HP Gain", ES_HP_L_GAIN, ES_HP_R_GAIN,
			0, 0x0F, 0, hp_tlv),
	SOC_DOUBLE_R_TLV("AUXIN Gain", ES_AUX_L_CTRL, ES_AUX_R_CTRL,
			1, 0x14, 0, aux_tlv),


	SOC_SINGLE_TLV("SPKRL Gain", ES_SPKR_L_GAIN, ES_SPKRL_GAIN_SHIFT,
			ES_SPKRL_GAIN_MAX, 0, spkr_tlv),
	SOC_SINGLE_TLV("SPKRR Gain", ES_SPKR_R_GAIN, ES_SPKRR_GAIN_SHIFT,
			ES_SPKRR_GAIN_MAX, 0, spkr_tlv),
	SOC_SINGLE_TLV("LOL Gain", ES_LO_L_GAIN, ES_LO_L_GAIN_SHIFT,
			ES_LO_L_GAIN_MAX, 0, lo_tlv),
	SOC_SINGLE_TLV("LOR Gain", ES_LO_R_GAIN, ES_LO_R_GAIN_SHIFT,
			ES_LO_R_GAIN_MAX, 0, lo_tlv),
	SOC_SINGLE_TLV("HPL Gain", ES_HP_L_GAIN, ES_HPL_GAIN_SHIFT,
			ES_HPL_GAIN_MAX, 0, hp_tlv),
	SOC_SINGLE_TLV("HPR Gain", ES_HP_R_GAIN, ES_HPR_GAIN_SHIFT,
			ES_HPR_GAIN_MAX, 0, hp_tlv),
	SOC_SINGLE_TLV("EP Gain", ES_EP_GAIN, ES_EP_GAIN_SHIFT,
			ES_EP_GAIN_MAX, 0, ep_tlv),
	SOC_SINGLE_TLV("AUXINL Gain", ES_AUX_L_CTRL, ES_AUXL_GAIN_SHIFT,
			ES_AUXL_GAIN_MAX, 0, aux_tlv),
	SOC_SINGLE_TLV("AUXINR Gain", ES_AUX_R_CTRL, ES_AUXR_GAIN_SHIFT,
			ES_AUXR_GAIN_MAX, 0, aux_tlv),


	SOC_SINGLE_TLV("MIC0 Gain", ES_MIC0_CTRL, ES_MIC0_GAIN_SHIFT,
			ES_MIC0_GAIN_MAX, 0, mic_tlv),
	SOC_SINGLE_TLV("MIC1 Gain", ES_MIC1_CTRL, ES_MIC1_GAIN_SHIFT,
			ES_MIC1_GAIN_MAX, 0, mic_tlv),
	SOC_SINGLE_TLV("MIC2 Gain", ES_MIC2_CTRL, ES_MIC2_GAIN_SHIFT,
			ES_MIC2_GAIN_MAX, 0, mic_tlv),
	SOC_SINGLE_TLV("MICHS Gain", ES_MICHS_CTRL, ES_MICHS_GAIN_SHIFT,
			ES_MICHS_GAIN_MAX, 0, mic_tlv),


	SOC_ENUM("AUXL Mono", auxl_mono_enum),
	SOC_ENUM("AUXR Mono", auxr_mono_enum),
	SOC_SINGLE("ADC Mute", ES_ADC_CTRL, ES_ADC_MUTE_SHIFT, 1, 0),
	SOC_SINGLE("EP Mute", ES_EP_GAIN, ES_EP_MUTE_SHIFT, 1, 0),
	SOC_SINGLE("HPL Mute", ES_HP_L_GAIN, ES_HPL_MUTE_SHIFT, 1, 0),
	SOC_SINGLE("HPR Mute", ES_HP_R_GAIN, ES_HPR_MUTE_SHIFT, 1, 0),
	SOC_SINGLE("SPKRL Mute", ES_SPKR_L_GAIN, ES_SPKRL_MUTE_SHIFT, 1, 0),
	SOC_SINGLE("SPKRR Mute", ES_SPKR_R_GAIN, ES_SPKRR_MUTE_SHIFT, 1, 0),
	SOC_SINGLE("LOL Mute", ES_LO_L_GAIN, ES_LO_L_MUTE_SHIFT, 1, 0),
	SOC_SINGLE("LOR Mute", ES_LO_R_GAIN, ES_LO_R_MUTE_SHIFT, 1, 0),

	SOC_ENUM("I2S Channels", i2s_ch_enum),
	SOC_ENUM("FrameSync SEL", fs_sel_enum),
	SOC_VALUE_ENUM("Bits per Sample", bps_enum),

	SOC_ENUM("MICHS SEL MUX", michs_sel_mux_enum),

	SOC_ENUM("MIC0 Input Type", mic0_input_type_enum),
	SOC_ENUM("MIC1 Input Type", mic1_input_type_enum),
	SOC_ENUM("MIC2 Input Type", mic2_input_type_enum),
	SOC_ENUM("MICHS Input Type", michs_input_type_enum),
	SOC_ENUM("AUXIN Input Type", auxin_input_type_enum),

	SOC_VALUE_ENUM("MIC0 Bias Output Voltage",
			mic0_bias_output_voltage_enum),
	SOC_VALUE_ENUM("MIC1 Bias Output Voltage",
			mic1_bias_output_voltage_enum),
	SOC_VALUE_ENUM("MIC2 Bias Output Voltage",
			mic2_bias_output_voltage_enum),
	SOC_VALUE_ENUM("MICHS Bias Output Voltage",
			michs_bias_output_voltage_enum),

	SOC_ENUM("MIC0 Input Impedance Mode", mic0_zin_mode_enum),
	SOC_ENUM("MIC1 Input Impedance Mode", mic1_zin_mode_enum),
	SOC_ENUM("MIC2 Input Impedance Mode", mic2_zin_mode_enum),
	SOC_ENUM("MICHS Input Impedance Mode", michs_zin_mode_enum),
};

static const char * const adc1_mux_text[] = {
	"MIC1_PGA", "MIC2_PGA",
};

static const struct soc_enum adc1_mux_enum =
	SOC_ENUM_SINGLE(ES_ADC_CTRL, ES_ADC1_IN_SEL_SHIFT,
		ARRAY_SIZE(adc1_mux_text), adc1_mux_text);

static const struct snd_kcontrol_new adc1_mux =
		SOC_DAPM_ENUM("ADC1 MUX Mux", adc1_mux_enum);

static const char * const adc2_mux_text[] = {
	"MIC2_PGA", "AUXR_PGA",
};

static const struct soc_enum adc2_mux_enum =
	SOC_ENUM_SINGLE(ES_ADC_CTRL, ES_ADC2_IN_SEL_SHIFT,
		ARRAY_SIZE(adc2_mux_text), adc2_mux_text);

static const struct snd_kcontrol_new adc2_mux =
		SOC_DAPM_ENUM("ADC2 MUX Mux", adc2_mux_enum);


static const struct snd_kcontrol_new ep_mix[] = {
	SOC_DAPM_SINGLE("DAC0L", ES_EP_CTRL, ES_DAC0L_TO_EP_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("AUXL", ES_EP_CTRL, ES_AUXL_TO_EP_SHIFT, 1, 0),
};

static const struct snd_kcontrol_new hpl_mix[] = {
	SOC_DAPM_SINGLE("DAC0L", ES_HP_L_CTRL, ES_DAC0L_TO_HPL_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("AUXL", ES_HP_L_CTRL, ES_AUXL_TO_HPL_SHIFT, 1, 0),
};

static const struct snd_kcontrol_new hpr_mix[] = {
	SOC_DAPM_SINGLE("DAC0R", ES_HP_R_CTRL, ES_DAC0R_TO_HPR_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("AUXR", ES_HP_R_CTRL, ES_AUXR_TO_HPR_SHIFT, 1, 0),
};

static const struct snd_kcontrol_new spkrl_mix[] = {
	SOC_DAPM_SINGLE("DAC0L", ES_SPKR_L_CTRL, ES_DAC0L_TO_SPKRL_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("DAC1L", ES_SPKR_L_CTRL, ES_DAC1L_TO_SPKRL_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("AUXL", ES_SPKR_L_CTRL, ES_AUXL_TO_SPKRL_SHIFT, 1, 0),
};

static const struct snd_kcontrol_new spkrr_mix[] = {
	SOC_DAPM_SINGLE("DAC0R", ES_SPKR_R_CTRL, ES_DAC0R_TO_SPKRR_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("DAC1R", ES_SPKR_R_CTRL, ES_DAC1R_TO_SPKRR_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("AUXR", ES_SPKR_R_CTRL, ES_AUXR_TO_SPKRR_SHIFT, 1, 0),
};

static const struct snd_kcontrol_new lo_l_mix[] = {
	SOC_DAPM_SINGLE("DAC0L", ES_LO_L_CTRL, ES_DAC0L_TO_LO_L_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("DAC1L", ES_LO_L_CTRL, ES_DAC1L_TO_LO_L_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("AUXL", ES_LO_L_CTRL, ES_AUXL_TO_LO_L_SHIFT, 1, 0),
};

static const struct snd_kcontrol_new lo_r_mix[] = {
	SOC_DAPM_SINGLE("DAC0R", ES_LO_R_CTRL, ES_DAC0R_TO_LO_R_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("DAC1R", ES_LO_R_CTRL, ES_DAC1R_TO_LO_R_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("AUXR", ES_LO_R_CTRL, ES_AUXR_TO_LO_R_SHIFT, 1, 0),
};

static const struct snd_kcontrol_new michs_control =
	SOC_DAPM_SINGLE("Switch", ES_MICHS_CTRL, ES_MICHS_ON_SHIFT, 1, 0);

static int es_lo_enable(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	pr_debug("%s LO event %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, ES_LO_L_CTRL, ES_LO_TRIM_DIS_MASK,
			ES_LO_TRIM_DIS);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, ES_LO_L_CTRL, ES_LO_TRIM_DIS_MASK,
			0);
		break;
	}
	return 0;
}

static int es_dac_enable(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	pr_debug("%s DAC%d event %d\n", __func__, w->shift, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, w->reg, 1<<w->shift, 1<<w->shift);
		snd_soc_update_bits(codec, ES_DAC_DIG_EN, ES_DIG_CLK_EN_MASK,
			ES_DIG_CLK_EN);
		snd_soc_update_bits(codec, ES_DAC_DIG_EN, ES_DAC_CLK_EN_MASK,
			ES_DAC_CLK_EN);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, w->reg, 1<<w->shift , 0);
		snd_soc_update_bits(codec, ES_DAC_DIG_EN, ES_DIG_CLK_EN_MASK,
			0);
		snd_soc_update_bits(codec, ES_DAC_DIG_EN, ES_DAC_CLK_EN_MASK,
				0);
		break;
	}
	return 0;
}

static int mic_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int event)
{
	struct snd_soc_codec *codec = w->codec;
	pr_debug("%s() %x\n", __func__, SND_SOC_DAPM_EVENT_ON(event));

	if (SND_SOC_DAPM_EVENT_ON(event)) {
		if (!strncmp(w->name, "MIC0", 4))
			snd_soc_update_bits(codec, ES_MIC0_CTRL,
					ES_MIC0_ON_MASK, 1);
		else if (!strncmp(w->name, "MIC1", 4))
			snd_soc_update_bits(codec, ES_MIC1_CTRL,
					ES_MIC1_ON_MASK, 1);
		else if (!strncmp(w->name, "MIC2", 4))
			snd_soc_update_bits(codec, ES_MIC2_CTRL,
					ES_MIC2_ON_MASK, 1);
		else {
			pr_err("%s() Invalid Mic Widget ON = %s\n",
					__func__, w->name);
			return -EINVAL;
		}

	} else {
		if (!strncmp(w->name, "MIC0", 4))
			snd_soc_update_bits(codec, ES_MIC0_CTRL,
				ES_MIC0_ON_MASK, 0);
		else if (!strncmp(w->name, "MIC1", 4))
			snd_soc_update_bits(codec, ES_MIC1_CTRL,
				ES_MIC1_ON_MASK, 0);
		else if (!strncmp(w->name, "MIC2", 4))
			snd_soc_update_bits(codec, ES_MIC2_CTRL,
				ES_MIC2_ON_MASK, 0);
		else {
			pr_err("%s() Invalid Mic Widget OFF = %s\n",
					__func__, w->name);
			return -EINVAL;
		}
	}
	return 0;
}

const struct snd_soc_dapm_widget es_codec_dapm_widgets[] = {

	/* Inputs */
	SND_SOC_DAPM_MIC("MIC0", mic_event),
	SND_SOC_DAPM_MIC("MIC1", mic_event),
	SND_SOC_DAPM_MIC("MIC2", mic_event),
	SND_SOC_DAPM_MIC("MICHS", NULL),
	SND_SOC_DAPM_MIC("AUXINM", NULL),
	SND_SOC_DAPM_MIC("AUXINP", NULL),

	/* Outputs */
	SND_SOC_DAPM_HP("HPL", NULL),
	SND_SOC_DAPM_HP("HPR", NULL),
	SND_SOC_DAPM_SPK("SPKRL", NULL),
	SND_SOC_DAPM_SPK("SPKRR", NULL),
	SND_SOC_DAPM_OUTPUT("EP"),
	SND_SOC_DAPM_LINE("AUXOUTL", NULL),
	SND_SOC_DAPM_LINE("AUXOUTR", NULL),

	/* Microphone bias */
	SND_SOC_DAPM_SUPPLY("MICHS Bias", ES_MICBIAS_CTRL,
		ES_MBIASHS_MODE_SHIFT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("MIC0 Bias", ES_MICBIAS_CTRL,
		ES_MBIAS0_MODE_SHIFT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("MIC1 Bias", ES_MICBIAS_CTRL,
		ES_MBIAS1_MODE_SHIFT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("MIC2 Bias", ES_MICBIAS_CTRL,
		ES_MBIAS2_MODE_SHIFT, 0, NULL, 0),

	/* Mic headset Switch ON/OFF */
	SND_SOC_DAPM_SWITCH("MICHS ON", SND_SOC_NOPM, 0, 0, &michs_control),

	/* ADC */
	SND_SOC_DAPM_ADC("ADC0", NULL, ES_ADC_CTRL, ES_ADC0_ON_SHIFT, 0),
	SND_SOC_DAPM_MUX("ADC1 MUX", SND_SOC_NOPM, 0, 0, &adc1_mux),
	SND_SOC_DAPM_ADC("ADC1", NULL, ES_ADC_CTRL, ES_ADC1_ON_SHIFT, 0),
	SND_SOC_DAPM_MUX("ADC2 MUX", SND_SOC_NOPM, 0, 0, &adc2_mux),
	SND_SOC_DAPM_ADC("ADC2", NULL, ES_ADC_CTRL, ES_ADC2_ON_SHIFT, 0),
	SND_SOC_DAPM_ADC("ADC3", NULL, ES_ADC_CTRL, ES_ADC3_ON_SHIFT, 0),

	/* DAC */
	SND_SOC_DAPM_DAC_E("DAC0L", NULL, ES_DAC_CTRL, ES_DAC0L_ON_SHIFT,
		0, es_dac_enable, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("DAC0R", NULL, ES_DAC_CTRL, ES_DAC0R_ON_SHIFT,
		0, es_dac_enable, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("DAC1L", NULL, ES_DAC_CTRL, ES_DAC1L_ON_SHIFT,
		0, es_dac_enable, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("DAC1R", NULL, ES_DAC_CTRL, ES_DAC1R_ON_SHIFT,
		0, es_dac_enable, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	/* Earphone Mixer */
	SND_SOC_DAPM_MIXER("EP MIXER", SND_SOC_NOPM, 0, 0,
		ep_mix, ARRAY_SIZE(ep_mix)),

	/* Headphone Mixer */
	SND_SOC_DAPM_MIXER("HPL MIXER", SND_SOC_NOPM, 0, 0,
		hpl_mix, ARRAY_SIZE(hpl_mix)),
	SND_SOC_DAPM_MIXER("HPR MIXER", SND_SOC_NOPM, 0, 0,
		hpr_mix, ARRAY_SIZE(hpr_mix)),

	/* Handsfree Mixer */
	SND_SOC_DAPM_MIXER("SPKRL MIXER", SND_SOC_NOPM, 0, 0,
		spkrl_mix, ARRAY_SIZE(spkrl_mix)),
	SND_SOC_DAPM_MIXER("SPKRR MIXER", SND_SOC_NOPM, 0, 0,
		spkrr_mix, ARRAY_SIZE(spkrr_mix)),

	/* LineOut Mixer */
	SND_SOC_DAPM_MIXER("LOL MIXER", SND_SOC_NOPM, 0, 0,
		lo_l_mix, ARRAY_SIZE(lo_l_mix)),
	SND_SOC_DAPM_MIXER("LOR MIXER", SND_SOC_NOPM, 0, 0,
		lo_r_mix, ARRAY_SIZE(lo_r_mix)),

	/* Output PGAs */
	SND_SOC_DAPM_PGA("SPKRL PGA", ES_SPKR_L_CTRL, ES_SPKRL_ON_SHIFT,
		0, NULL, 0),
	SND_SOC_DAPM_PGA("SPKRR PGA", ES_SPKR_R_CTRL, ES_SPKRR_ON_SHIFT,
		0, NULL, 0),
	SND_SOC_DAPM_PGA("HPL PGA", ES_HP_L_CTRL, ES_HPL_ON_SHIFT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("HPR PGA", ES_HP_R_CTRL, ES_HPR_ON_SHIFT, 0, NULL, 0),
	SND_SOC_DAPM_PGA_E("LOL PGA", ES_LO_L_CTRL, ES_LO_L_ON_SHIFT, 0, NULL,
		0, es_lo_enable, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("LOR PGA", ES_LO_R_CTRL, ES_LO_R_ON_SHIFT, 0, NULL,
		0, es_lo_enable, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA("EP PGA", ES_EP_CTRL, ES_EP_ON_SHIFT, 0, NULL, 0),

	/* Input PGAs */
	SND_SOC_DAPM_PGA("MIC0 PGA", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("MIC1 PGA", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("MIC2 PGA", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("AUXINL PGA", ES_AUX_L_CTRL, ES_AUXL_ON_SHIFT, 0,
		NULL, 0),
	SND_SOC_DAPM_PGA("AUXINR PGA", ES_AUX_R_CTRL, ES_AUXR_ON_SHIFT,	0,
		NULL, 0),

};

/* TODO */
static const struct snd_soc_dapm_route intercon[] = {

	/* Capture path */

	{"MICHS", NULL, "MICHS Bias"},
	{"MIC0", NULL, "MIC0 Bias"},
	{"MIC1", NULL, "MIC1 Bias"},
	{"MIC2", NULL, "MIC2 Bias"},

	{"MICHS ON", "Switch", "MICHS"},

	{"MIC0 PGA", NULL, "MIC0"},
	{"MIC1 PGA", NULL, "MIC1"},
	{"MIC1 PGA", NULL, "MICHS ON"},
	{"MIC2 PGA", NULL, "MIC2"},
	{"MIC2 PGA", NULL, "MICHS ON"},

	{"AUXINL PGA", NULL, "AUXINP"},
	{"AUXINR PGA", NULL, "AUXINM"},

	{"ADC0", NULL, "MIC0 PGA"},
	{"ADC1 MUX", "MIC1_PGA", "MIC1 PGA"},
	{"ADC1 MUX", "MIC2_PGA", "MIC2 PGA"},
	{"ADC2 MUX", "MIC2_PGA", "MIC2 PGA"},
	{"ADC2 MUX", "AUXR_PGA", "AUXINR PGA"},
	{"ADC3", NULL, "AUXINL PGA"},

	{"ADC1", NULL, "ADC1 MUX"},
	{"ADC2", NULL, "ADC2 MUX"},

	/* Playback path */

	{"SPKRL MIXER", "DAC0L", "DAC0L"},
	{"SPKRL MIXER", "DAC1L", "DAC1L"},
	{"SPKRL MIXER", "AUXL", "AUXINL PGA"},

	{"SPKRR MIXER", "DAC0R", "DAC0R"},
	{"SPKRR MIXER", "DAC1R", "DAC1R"},
	{"SPKRR MIXER", "AUXR", "AUXINR PGA"},

	{"HPL MIXER", "DAC0L", "DAC0L"},
	{"HPL MIXER", "AUXL", "AUXINL PGA"},

	{"HPR MIXER", "DAC0R", "DAC0R"},
	{"HPR MIXER", "AUXR", "AUXINR PGA"},

	{"EP MIXER", "DAC0L", "DAC0L"},
	{"EP MIXER", "AUXL", "AUXINL PGA"},

	{"LOL MIXER", "DAC0L", "DAC0L"},
	{"LOL MIXER", "DAC1L", "DAC1L"},
	{"LOL MIXER", "AUXL", "AUXINL PGA"},

	{"LOR MIXER", "DAC0R", "DAC0R"},
	{"LOR MIXER", "DAC1R", "DAC1R"},
	{"LOR MIXER", "AUXR", "AUXINR PGA"},

	{"LOL PGA", NULL, "LOL MIXER"},
	{"LOR PGA", NULL, "LOR MIXER"},
	{"HPL PGA", NULL, "HPL MIXER"},
	{"HPR PGA", NULL, "HPR MIXER"},
	{"SPKRL PGA", NULL, "SPKRL MIXER"},
	{"SPKRR PGA", NULL, "SPKRR MIXER"},
	{"EP PGA", NULL, "EP MIXER"},

	{"AUXOUTL", NULL, "LOL PGA"},
	{"AUXOUTR", NULL, "LOR PGA"},
	{"HPL", NULL, "HPL PGA"},
	{"HPR", NULL, "HPR PGA"},
	{"SPKRL", NULL, "SPKRL PGA"},
	{"SPKRR", NULL, "SPKRR PGA"},
	{"EP", NULL, "EP PGA"},

};

int es_analog_add_snd_soc_controls(struct snd_soc_codec *codec)
{
	int rc;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
	rc = snd_soc_add_codec_controls(codec, es_codec_snd_controls,
			ARRAY_SIZE(es_codec_snd_controls));
#else
	rc = snd_soc_add_controls(codec, es_codec_snd_controls,
			ARRAY_SIZE(es_codec_snd_controls));
#endif

	return rc;
}
int es_analog_add_snd_soc_dapm_controls(struct snd_soc_codec *codec)
{
	int rc;

	rc = snd_soc_dapm_new_controls(&codec->dapm, es_codec_dapm_widgets,
					ARRAY_SIZE(es_codec_dapm_widgets));

	return rc;
}
int es_analog_add_snd_soc_route_map(struct snd_soc_codec *codec)
{
	int rc;

	rc = snd_soc_dapm_add_routes(&codec->dapm, intercon,
					ARRAY_SIZE(intercon));

	return rc;
}
