
/*
 * es-a212.c  --  Audience eS515 ALSA SoC Audio driver
 *
 * Copyright 2011 Audience, Inc.
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

#include "es-a212-reg.h"

static DECLARE_TLV_DB_SCALE(hf_tlv, -3600, 200, 1);
static DECLARE_TLV_DB_SCALE(lo_tlv, -2200, 200, 1);
static DECLARE_TLV_DB_SCALE(hp_tlv, -2200, 200, 1);
static DECLARE_TLV_DB_SCALE(ep_tlv, -2200, 200, 1);
static DECLARE_TLV_DB_SCALE(aux_tlv, 0, 300, 0);
static DECLARE_TLV_DB_SCALE(aux_atten_tlv, -3000, 300, 0);
static DECLARE_TLV_DB_SCALE(mic_tlv, 0, 150, 0);


static const char * const aux_mono_text[] = {
	"Stereo", "Mono",
};

static const struct soc_enum auxl_mono_enum =
	SOC_ENUM_SINGLE(ES_AUXL_CTRL, ES_AUXL_MONO_SHIFT,
		ARRAY_SIZE(aux_mono_text), aux_mono_text);

static const struct soc_enum auxr_mono_enum =
	SOC_ENUM_SINGLE(ES_AUXR_CTRL, ES_AUXR_MONO_SHIFT,
		ARRAY_SIZE(aux_mono_text), aux_mono_text);

static const char * const i2s_ch_text[] = {
	"2", "4",
};

static const struct soc_enum i2s_ch_enum =
	SOC_ENUM_SINGLE(ES_DAC_DIG_CH, ES_I2S_CH_SHIFT,
		ARRAY_SIZE(i2s_ch_text), i2s_ch_text);

const struct snd_kcontrol_new es_codec_snd_controls[] = {

	SOC_DOUBLE_R_TLV("HF Gain", ES_HFL_GAIN, ES_HFR_GAIN,
			0, 0x1F, 0, hf_tlv),
	SOC_DOUBLE_R_TLV("LO Gain", ES_LO_L_GAIN, ES_LO_R_GAIN,
			0, 0x0F, 0, lo_tlv),
	SOC_DOUBLE_R_TLV("HP Gain", ES_HPL_GAIN, ES_HPR_GAIN,
			0, 0x0F, 0, hp_tlv),
	SOC_DOUBLE_R_TLV("AUX Gain", ES_AUXL_GAIN, ES_AUXR_GAIN,
			4, 0x04, 0, aux_tlv),


	SOC_SINGLE_TLV("HFL Gain", ES_HFL_GAIN, ES_HFL_GAIN_SHIFT,
			ES_HFL_GAIN_MAX, 0, hf_tlv),
	SOC_SINGLE_TLV("HFR Gain", ES_HFR_GAIN, ES_HFR_GAIN_SHIFT,
			ES_HFR_GAIN_MAX, 0, ep_tlv),
	SOC_SINGLE_TLV("LOL Gain", ES_LO_L_GAIN, ES_LO_L_GAIN_SHIFT,
			ES_LO_L_GAIN_MAX, 0, lo_tlv),
	SOC_SINGLE_TLV("LOR Gain", ES_LO_R_GAIN, ES_LO_R_GAIN_SHIFT,
			ES_LO_R_GAIN_MAX, 0, lo_tlv),
	SOC_SINGLE_TLV("HPL Gain", ES_HPL_GAIN, ES_HPL_GAIN_SHIFT,
			ES_HPL_GAIN_MAX, 0, hp_tlv),
	SOC_SINGLE_TLV("HPR Gain", ES_HPR_GAIN, ES_HPR_GAIN_SHIFT,
			ES_HPR_GAIN_MAX, 0, hp_tlv),
	SOC_SINGLE_TLV("EP Gain", ES_EP_GAIN, ES_EP_GAIN_SHIFT,
			ES_EP_GAIN_MAX, 0, ep_tlv),
	SOC_SINGLE_TLV("AUXINL Gain", ES_AUXL_GAIN, ES_AUX_L_GAIN_SHIFT,
			ES_AUX_L_GAIN_MAX, 0, aux_tlv),
	SOC_SINGLE_TLV("AUXINR Gain", ES_AUXR_GAIN, ES_AUX_R_GAIN_SHIFT,
			ES_AUX_R_GAIN_MAX, 0, aux_tlv),
	SOC_SINGLE_TLV("AUXP Atten", ES_AUXL_GAIN, ES_AUX_P_ATTEN_SHIFT,
			ES_AUX_P_ATTEN_MAX, 0, aux_atten_tlv),
	SOC_SINGLE_TLV("AUXM Atten", ES_AUXR_GAIN, ES_AUX_M_ATTEN_SHIFT,
			ES_AUX_M_ATTEN_MAX, 0, aux_atten_tlv),


	SOC_SINGLE_TLV("MICL Gain", ES_MICL_GAIN, ES_MICL_GAIN_SHIFT,
			ES_MICL_GAIN_MAX, 0, mic_tlv),
	SOC_SINGLE_TLV("MICR Gain", ES_MICR_GAIN, ES_MICR_GAIN_SHIFT,
			ES_MICR_GAIN_MAX, 0, mic_tlv),

	SOC_ENUM("AUXL Mono", auxl_mono_enum),
	SOC_ENUM("AUXR Mono", auxr_mono_enum),
	SOC_SINGLE("ADC Mute", ES_ADC_CTRL, ES_ADC_MUTE_SHIFT, 1, 0),
	SOC_SINGLE("EP Mute", ES_EP_GAIN, ES_EP_MUTE_SHIFT, 1, 0),
	SOC_SINGLE("HPL Mute", ES_HPL_GAIN, ES_HPL_MUTE_SHIFT, 1, 0),
	SOC_SINGLE("HPR Mute", ES_HPR_GAIN, ES_HPR_MUTE_SHIFT, 1, 0),
	SOC_SINGLE("HFL Mute", ES_HFL_GAIN, ES_HFL_MUTE_SHIFT, 1, 0),
	SOC_SINGLE("HFR Mute", ES_HFR_GAIN, ES_HFR_MUTE_SHIFT, 1, 0),
	SOC_SINGLE("LOL Mute", ES_LO_L_GAIN, ES_LO_L_MUTE_SHIFT, 1, 0),
	SOC_SINGLE("LOR Mute", ES_LO_R_GAIN, ES_LO_R_MUTE_SHIFT, 1, 0),

	SOC_ENUM("I2S Channels", i2s_ch_enum),
};

static const char * const micl_mux_text[] = {
	"None", "MIC1", "MICHS",
};

static const struct soc_enum micl_mux_enum =
	SOC_ENUM_SINGLE(ES_MIC_CTRL, ES_MICL_IN_SEL_SHIFT,
		ARRAY_SIZE(micl_mux_text), micl_mux_text);

static const struct snd_kcontrol_new micl_mux =
		SOC_DAPM_ENUM("MICL MUX Mux", micl_mux_enum);

static const char * const micr_mux_text[] = {
	"None", "MIC2", "MICHS",
};

static const struct soc_enum micr_mux_enum =
	SOC_ENUM_SINGLE(ES_MIC_CTRL, ES_MICR_IN_SEL_SHIFT,
		ARRAY_SIZE(micr_mux_text), micr_mux_text);

static const struct snd_kcontrol_new micr_mux =
		SOC_DAPM_ENUM("MICR MUX Mux", micr_mux_enum);


static const char * const auxl_mux_text[] = {
	"None", "AUX_IN_M", "AUX_IN_P",
};

static const struct soc_enum auxl_mux_enum =
	SOC_ENUM_SINGLE(ES_AUXL_CTRL, ES_APGAL_IN_AUX_SHIFT,
		ARRAY_SIZE(auxl_mux_text), auxl_mux_text);

static const struct snd_kcontrol_new auxl_mux =
		SOC_DAPM_ENUM("AUXL MUX Mux", auxl_mux_enum);


static const char * const auxr_mux_text[] = {
	"None", "AUX_IN_P", "AUX_IN_M",
};

static const struct soc_enum auxr_mux_enum =
	SOC_ENUM_SINGLE(ES_AUXR_CTRL, ES_APGAR_IN_AUX_SHIFT,
		ARRAY_SIZE(auxr_mux_text), auxr_mux_text);

static const struct snd_kcontrol_new auxr_mux =
		SOC_DAPM_ENUM("AUXR MUX Mux", auxr_mux_enum);


static const char * const adc1_mux_text[] = {
	"MICL", "MICR",
};

static const struct soc_enum adc1_mux_enum =
	SOC_ENUM_SINGLE(ES_ADC_CTRL, ES_ADC1_IN_SEL_SHIFT,
		ARRAY_SIZE(adc1_mux_text), adc1_mux_text);

static const struct snd_kcontrol_new adc1_mux =
		SOC_DAPM_ENUM("ADC1 MUX Mux", adc1_mux_enum);

static const char * const adc2_mux_text[] = {
	"MICR", "AUXR",
};

static const struct soc_enum adc2_mux_enum =
	SOC_ENUM_SINGLE(ES_ADC_CTRL, ES_ADC2_IN_SEL_SHIFT,
		ARRAY_SIZE(adc2_mux_text), adc2_mux_text);

static const struct snd_kcontrol_new adc2_mux =
		SOC_DAPM_ENUM("ADC2 MUX Mux", adc2_mux_enum);

static const char * const adc3_mux_text[] = {
	"AUXL", "ADIR",
};

static const struct soc_enum adc3_mux_enum =
	SOC_ENUM_SINGLE(ES_ADC_CTRL, ES_ADC3_IN_SEL_SHIFT,
		ARRAY_SIZE(adc3_mux_text), adc3_mux_text);

static const struct snd_kcontrol_new adc3_mux =
		SOC_DAPM_ENUM("ADC3 MUX Mux", adc3_mux_enum);

static const struct snd_kcontrol_new ep_mix[] = {
	SOC_DAPM_SINGLE("DAC1L", ES_EP_CTRL, ES_DAC1L_TO_EP_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("AUXL", ES_EP_CTRL, ES_AUXL_TO_EP_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("AUXR", ES_EP_CTRL, ES_AUXR_TO_EP_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("MICL", ES_EP_CTRL, ES_MICL_TO_EP_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("MICR", ES_EP_CTRL, ES_MICR_TO_EP_SHIFT, 1, 0),
};

static const struct snd_kcontrol_new hpl_mix[] = {
	SOC_DAPM_SINGLE("DAC1L", ES_HPL_CTRL, ES_DAC1L_TO_HPL_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("DAC2L", ES_HPL_CTRL, ES_DAC2L_TO_HPL_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("AUXL", ES_HPL_CTRL, ES_AUXL_TO_HPL_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("MICL", ES_HPL_CTRL, ES_MICL_TO_HPL_SHIFT, 1, 0),
};

static const struct snd_kcontrol_new hpr_mix[] = {
	SOC_DAPM_SINGLE("DAC1R", ES_HPR_CTRL, ES_DAC1R_TO_HPR_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("DAC2R", ES_HPR_CTRL, ES_DAC2R_TO_HPR_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("AUXR", ES_HPR_CTRL, ES_AUXR_TO_HPR_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("MICR", ES_HPR_CTRL, ES_MICR_TO_HPR_SHIFT, 1, 0),
};

static const struct snd_kcontrol_new hfl_mix[] = {
	SOC_DAPM_SINGLE("DAC1L", ES_HFL_CTRL, ES_DAC1L_TO_HFL_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("DAC2L", ES_HFL_CTRL, ES_DAC2L_TO_HFL_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("AUXL", ES_HFL_CTRL, ES_AUXL_TO_HFL_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("MICL", ES_HFL_CTRL, ES_MICL_TO_HFL_SHIFT, 1, 0),
};

static const struct snd_kcontrol_new hfr_mix[] = {
	SOC_DAPM_SINGLE("DAC1R", ES_HFR_CTRL, ES_DAC1R_TO_HFR_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("DAC2R", ES_HFR_CTRL, ES_DAC2R_TO_HFR_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("AUXR", ES_HFR_CTRL, ES_AUXR_TO_HFR_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("MICR", ES_HFR_CTRL, ES_MICR_TO_HFR_SHIFT, 1, 0),
};

static const struct snd_kcontrol_new lo_l_mix[] = {
	SOC_DAPM_SINGLE("DAC1L", ES_LO_L_CTRL, ES_DAC1L_TO_LO_L_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("DAC2L", ES_LO_L_CTRL, ES_DAC2L_TO_LO_L_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("AUXL", ES_LO_L_CTRL, ES_AUXL_TO_LO_L_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("MICL", ES_LO_L_CTRL, ES_MICL_TO_LO_L_SHIFT, 1, 0),
};

static const struct snd_kcontrol_new lo_r_mix[] = {
	SOC_DAPM_SINGLE("DAC1R", ES_LO_R_CTRL, ES_DAC1R_TO_LO_R_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("DAC2R", ES_LO_R_CTRL, ES_DAC2R_TO_LO_R_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("AUXR", ES_LO_R_CTRL, ES_AUXR_TO_LO_R_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("MICR", ES_LO_R_CTRL, ES_MICR_TO_LO_R_SHIFT, 1, 0),
};

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
		snd_soc_update_bits(codec, ES_DAC_DIG_EN, 1<<w->shift,
			1<<w->shift);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, w->reg, 1<<w->shift , 0);
		snd_soc_update_bits(codec, ES_DAC_DIG_EN, ES_DIG_CLK_EN_MASK,
			0);
		snd_soc_update_bits(codec, ES_DAC_DIG_EN, ES_DAC_CLK_EN_MASK,
			0);
		snd_soc_update_bits(codec, ES_DAC_DIG_EN, 1<<w->shift, 0);
		break;
	}
	return 0;
}

const struct snd_soc_dapm_widget es_codec_dapm_widgets[] = {

	/* Inputs */
	SND_SOC_DAPM_INPUT("MIC1"),
	SND_SOC_DAPM_INPUT("MICHS"),
	SND_SOC_DAPM_INPUT("MIC2"),
	SND_SOC_DAPM_INPUT("AUXINM"),
	SND_SOC_DAPM_INPUT("AUXINP"),

	/* Outputs */
	SND_SOC_DAPM_OUTPUT("HPL"),
	SND_SOC_DAPM_OUTPUT("HPR"),
	SND_SOC_DAPM_OUTPUT("HFL"),
	SND_SOC_DAPM_OUTPUT("HFR"),
	SND_SOC_DAPM_OUTPUT("EP"),
	SND_SOC_DAPM_OUTPUT("AUXOUTL"),
	SND_SOC_DAPM_OUTPUT("AUXOUTR"),

	/* Microphone bias */
	SND_SOC_DAPM_SUPPLY("MICHS Bias", ES_MICBIAS_CTRL,
		ES_MBIAS2_ENA_SHIFT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("MIC1 Bias", ES_MICBIAS_CTRL,
		ES_MBIAS1_ENA_SHIFT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("MIC2 Bias", ES_MICBIAS_CTRL,
		ES_MBIAS2_ENA_SHIFT, 0, NULL, 0),

	/* Input Mux */
	SND_SOC_DAPM_MUX("MICL MUX", SND_SOC_NOPM, 0, 0, &micl_mux),
	SND_SOC_DAPM_MUX("MICR MUX", SND_SOC_NOPM, 0, 0, &micr_mux),
	SND_SOC_DAPM_MUX("AUXINL MUX", SND_SOC_NOPM, 0, 0, &auxl_mux),
	SND_SOC_DAPM_MUX("AUXINR MUX", SND_SOC_NOPM, 0, 0, &auxr_mux),

	/* ADC */
	SND_SOC_DAPM_MUX("ADC1 MUX", SND_SOC_NOPM, 0, 0, &adc1_mux),
	SND_SOC_DAPM_ADC("ADC1", NULL, ES_ADC_CTRL, ES_ADC1_ON_SHIFT, 0),
	SND_SOC_DAPM_MUX("ADC2 MUX", SND_SOC_NOPM, 0, 0, &adc2_mux),
	SND_SOC_DAPM_ADC("ADC2", NULL, ES_ADC_CTRL, ES_ADC2_ON_SHIFT, 0),
	SND_SOC_DAPM_MUX("ADC3 MUX", SND_SOC_NOPM, 0, 0, &adc3_mux),
	SND_SOC_DAPM_ADC("ADC3", NULL, ES_ADC_CTRL, ES_ADC3_ON_SHIFT, 0),

	/* DAC */
	SND_SOC_DAPM_DAC_E("DAC1L", NULL, ES_DAC_CTRL, ES_DAC1L_ON_SHIFT,
		0, es_dac_enable, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("DAC1R", NULL, ES_DAC_CTRL, ES_DAC1R_ON_SHIFT,
		0, es_dac_enable, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("DAC2L", NULL, ES_DAC_CTRL, ES_DAC2L_ON_SHIFT,
		0, es_dac_enable, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("DAC2R", NULL, ES_DAC_CTRL, ES_DAC2R_ON_SHIFT,
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
	SND_SOC_DAPM_MIXER("HFL MIXER", SND_SOC_NOPM, 0, 0,
		hfl_mix, ARRAY_SIZE(hfl_mix)),
	SND_SOC_DAPM_MIXER("HFR MIXER", SND_SOC_NOPM, 0, 0,
		hfr_mix, ARRAY_SIZE(hfr_mix)),

	/* LineOut Mixer */
	SND_SOC_DAPM_MIXER("LOL MIXER", SND_SOC_NOPM, 0, 0,
		lo_l_mix, ARRAY_SIZE(lo_l_mix)),
	SND_SOC_DAPM_MIXER("LOR MIXER", SND_SOC_NOPM, 0, 0,
		lo_r_mix, ARRAY_SIZE(lo_r_mix)),

	/* Output PGAs */
	SND_SOC_DAPM_PGA("HFL PGA", ES_HFL_CTRL, ES_HFL_ON_SHIFT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("HFR PGA", ES_HFR_CTRL, ES_HFR_ON_SHIFT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("HPL PGA", ES_HPL_CTRL, ES_HPL_ON_SHIFT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("HPR PGA", ES_HPR_CTRL, ES_HPR_ON_SHIFT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("LOL PGA", ES_LO_L_CTRL, ES_LO_L_ON_SHIFT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("LOR PGA", ES_LO_R_CTRL, ES_LO_R_ON_SHIFT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("EP PGA", ES_EP_CTRL, ES_EP_ON_SHIFT, 0, NULL, 0),

	/* Input PGAs */
	SND_SOC_DAPM_PGA("MICL PGA", ES_MIC_CTRL, ES_MICL_ON_SHIFT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("MICR PGA", ES_MIC_CTRL, ES_MICR_ON_SHIFT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("AUXINL PGA", ES_AUXL_CTRL, ES_AUXL_ON_SHIFT,
		0, NULL, 0),
	SND_SOC_DAPM_PGA("AUXINR PGA", ES_AUXR_CTRL, ES_AUXR_ON_SHIFT,
		0, NULL, 0),

};

static const struct snd_soc_dapm_route intercon[] = {
	/* Capture path */

	{"MICHS", NULL, "MICHS Bias"},
	{"MIC1", NULL, "MIC1 Bias"},
	{"MIC2", NULL, "MIC2 Bias"},

	{"MICL MUX", "MIC1", "MIC1"},
	{"MICL MUX", "MICHS", "MICHS"},
	{"MICR MUX", "MIC2", "MIC2"},
	{"MICR MUX", "MICHS", "MICHS"},
	{"AUXINL MUX", "AUX_IN_M", "AUXINM"},
	{"AUXINL MUX", "AUX_IN_P", "AUXINP"},
	{"AUXINR MUX", "AUX_IN_M", "AUXINM"},
	{"AUXINR MUX", "AUX_IN_P", "AUXINP"},

	{"MICL PGA", NULL, "MICL MUX"},
	{"MICR PGA", NULL, "MICR MUX"},
	{"AUXINL PGA", NULL, "AUXINL MUX"},
	{"AUXINR PGA", NULL, "AUXINR MUX"},

	{"ADC1 MUX", "MICL", "MICL PGA"},
	{"ADC1 MUX", "MICR", "MICR PGA"},
	{"ADC2 MUX", "AUXR", "AUXINR PGA"},
	{"ADC2 MUX", "MICR", "MICR PGA"},
	{"ADC3 MUX", "AUXL", "AUXINL PGA"},

	{"ADC1", NULL, "ADC1 MUX"},
	{"ADC2", NULL, "ADC2 MUX"},
	{"ADC3", NULL, "ADC3 MUX"},

	/*{"PORTA Capture", NULL, "ADC1"},
	{"PORTA Capture", NULL, "ADC2"},
	{"PORTA Capture", NULL, "ADC3"},*/

	/* Playback path */

	/*{"DAC1L", NULL, "PORTA Playback"},
	{"DAC1R", NULL, "PORTA Playback"},
	{"DAC2L", NULL, "PORTA Playback"},
	{"DAC2R", NULL, "PORTA Playback"},*/

	{"HFL MIXER", "DAC1L", "DAC1L"},
	{"HFL MIXER", "DAC2L", "DAC2L"},
	{"HFL MIXER", "AUXL", "AUXINL PGA"},
	{"HFL MIXER", "MICL", "MICL PGA"},

	{"HFR MIXER", "DAC1R", "DAC1R"},
	{"HFR MIXER", "DAC2R", "DAC2R"},
	{"HFR MIXER", "AUXR", "AUXINR PGA"},
	{"HFR MIXER", "MICR", "MICR PGA"},

	{"HPL MIXER", "DAC1L", "DAC1L"},
	{"HPL MIXER", "DAC2L", "DAC2L"},
	{"HPL MIXER", "AUXL", "AUXINL PGA"},
	{"HPL MIXER", "MICL", "MICL PGA"},

	{"HPR MIXER", "DAC1R", "DAC1R"},
	{"HPR MIXER", "DAC2R", "DAC2R"},
	{"HPR MIXER", "AUXR", "AUXINR PGA"},
	{"HPR MIXER", "MICR", "MICR PGA"},

	{"EP MIXER", "DAC1L", "DAC1L"},
	{"EP MIXER", "AUXL", "AUXINL PGA"},
	{"EP MIXER", "AUXR", "AUXINR PGA"},
	{"EP MIXER", "MICL", "MICL PGA"},
	{"EP MIXER", "MICR", "MICR PGA"},

	{"LOL MIXER", "DAC1L", "DAC1L"},
	{"LOL MIXER", "DAC2L", "DAC2L"},
	{"LOL MIXER", "AUXL", "AUXINL PGA"},
	{"LOL MIXER", "MICL", "MICL PGA"},

	{"LOR MIXER", "DAC1R", "DAC1R"},
	{"LOR MIXER", "DAC2R", "DAC2R"},
	{"LOR MIXER", "AUXR", "AUXINR PGA"},
	{"LOR MIXER", "MICR", "MICR PGA"},

	{"LOL PGA", NULL, "LOL MIXER"},
	{"LOR PGA", NULL, "LOR MIXER"},
	{"HPL PGA", NULL, "HPL MIXER"},
	{"HPR PGA", NULL, "HPR MIXER"},
	{"HFL PGA", NULL, "HFL MIXER"},
	{"HFR PGA", NULL, "HFR MIXER"},
	{"EP PGA", NULL, "EP MIXER"},

	{"AUXOUTL", NULL, "LOL PGA"},
	{"AUXOUTR", NULL, "LOR PGA"},
	{"HPL", NULL, "HPL PGA"},
	{"HPR", NULL, "HPR PGA"},
	{"HFL", NULL, "HFL PGA"},
	{"HFR", NULL, "HFR PGA"},
	{"EP", NULL, "EP PGA"},

};

int es_a212_add_snd_soc_controls(struct snd_soc_codec *codec)
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

int es_a212_add_snd_soc_dapm_controls(struct snd_soc_codec *codec)
{
	int rc;

	rc = snd_soc_dapm_new_controls(&codec->dapm, es_codec_dapm_widgets,
					ARRAY_SIZE(es_codec_dapm_widgets));

	return rc;
}
int es_a212_add_snd_soc_route_map(struct snd_soc_codec *codec)
{
	int rc;

	rc = snd_soc_dapm_add_routes(&codec->dapm, intercon,
					ARRAY_SIZE(intercon));

	return rc;
}

int a212_probe(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm = &codec->dapm;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
	snd_soc_add_codec_controls(codec, es_codec_snd_controls,
		ARRAY_SIZE(es_codec_snd_controls));
#else
	snd_soc_add_controls(codec, es_codec_snd_controls,
		ARRAY_SIZE(es_codec_snd_controls));
#endif
	snd_soc_dapm_new_controls(dapm, es_codec_dapm_widgets,
		ARRAY_SIZE(es_codec_dapm_widgets));
	snd_soc_dapm_add_routes(dapm, intercon, ARRAY_SIZE(intercon));

	return 0;
}
