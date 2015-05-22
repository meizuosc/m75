/*
 * wm8998.c -- ALSA SoC Audio driver for WM8998 codecs
 *
 * Copyright 2014 Wolfson Microelectronics plc
 *
 * Author: Richard Fitzgerald <rf@opensource.wolfsonmicro.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include <linux/mfd/arizona/core.h>
#include <linux/mfd/arizona/registers.h>

#include "arizona.h"
#include "wm8998.h"
#include "../../../drivers/mfd/arizona.h"

struct wm8998_priv {
	struct arizona_priv core;
	struct arizona_fll fll[2];
};

static int wm8998_in1mux_ev(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol,
				int event);

static int wm8998_in2mux_ev(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol,
				int event);

static const char * const wm8998_in1mux_texts[] = {
	"IN1A",
	"IN1B",
};

static const char * const wm8998_in2mux_texts[] = {
	"IN2A",
	"IN2B",
};

static const SOC_ENUM_SINGLE_DECL(wm8998_in1muxl_enum,
				  ARIZONA_ADC_DIGITAL_VOLUME_1L,
				  ARIZONA_IN1L_SRC_SHIFT,
				  wm8998_in1mux_texts);

static const SOC_ENUM_SINGLE_DECL(wm8998_in1muxr_enum,
				  ARIZONA_ADC_DIGITAL_VOLUME_1R,
				  ARIZONA_IN1R_SRC_SHIFT,
				  wm8998_in1mux_texts);

static const SOC_ENUM_SINGLE_DECL(wm8998_in2mux_enum,
				  ARIZONA_ADC_DIGITAL_VOLUME_2L,
				  ARIZONA_IN2L_SRC_SHIFT,
				  wm8998_in2mux_texts);

static const struct snd_kcontrol_new wm8998_in1mux[2] = {
	SOC_DAPM_ENUM("Route", wm8998_in1muxl_enum),
	SOC_DAPM_ENUM("Route", wm8998_in1muxr_enum),
};

static const struct snd_kcontrol_new wm8998_in2mux =
	SOC_DAPM_ENUM("Route", wm8998_in2mux_enum);

static DECLARE_TLV_DB_SCALE(ana_tlv, 0, 100, 0);
static DECLARE_TLV_DB_SCALE(eq_tlv, -1200, 100, 0);
static DECLARE_TLV_DB_SCALE(digital_tlv, -6400, 50, 0);
static DECLARE_TLV_DB_SCALE(noise_tlv, 0, 600, 0);
static DECLARE_TLV_DB_SCALE(ng_tlv, -10200, 600, 0);

#define WM8998_NG_SRC(name, base) \
	SOC_SINGLE(name " NG HPOUTL Switch",  base,  0, 1, 0), \
	SOC_SINGLE(name " NG HPOUTR Switch",  base,  1, 1, 0), \
	SOC_SINGLE(name " NG EPOUT Switch",   base,  4, 1, 0), \
	SOC_SINGLE(name " NG SPKOUTL Switch",  base,  6, 1, 0)

static const struct snd_kcontrol_new wm8998_snd_controls[] = {
SOC_ENUM("IN1 OSR", arizona_in_dmic_osr[0]),
SOC_ENUM("IN2 OSR", arizona_in_dmic_osr[1]),

SOC_SINGLE_RANGE_TLV("IN1L Volume", ARIZONA_IN1L_CONTROL,
		     ARIZONA_IN1L_PGA_VOL_SHIFT, 0x40, 0x5f, 0, ana_tlv),
SOC_SINGLE_RANGE_TLV("IN1R Volume", ARIZONA_IN1R_CONTROL,
		     ARIZONA_IN1R_PGA_VOL_SHIFT, 0x40, 0x5f, 0, ana_tlv),
SOC_SINGLE_RANGE_TLV("IN2 Volume", ARIZONA_IN2L_CONTROL,
		     ARIZONA_IN2L_PGA_VOL_SHIFT, 0x40, 0x5f, 0, ana_tlv),

SOC_ENUM("IN HPF Cutoff Frequency", arizona_in_hpf_cut_enum),

SOC_SINGLE("IN1L HPF Switch", ARIZONA_IN1L_CONTROL,
	   ARIZONA_IN1L_HPF_SHIFT, 1, 0),
SOC_SINGLE("IN1R HPF Switch", ARIZONA_IN1R_CONTROL,
	   ARIZONA_IN1R_HPF_SHIFT, 1, 0),
SOC_SINGLE("IN2 HPF Switch", ARIZONA_IN2L_CONTROL,
	   ARIZONA_IN2L_HPF_SHIFT, 1, 0),

SOC_SINGLE_TLV("IN1L Digital Volume", ARIZONA_ADC_DIGITAL_VOLUME_1L,
	       ARIZONA_IN1L_DIG_VOL_SHIFT, 0xbf, 0, digital_tlv),
SOC_SINGLE_TLV("IN1R Digital Volume", ARIZONA_ADC_DIGITAL_VOLUME_1R,
	       ARIZONA_IN1R_DIG_VOL_SHIFT, 0xbf, 0, digital_tlv),
SOC_SINGLE_TLV("IN2 Digital Volume", ARIZONA_ADC_DIGITAL_VOLUME_2L,
	       ARIZONA_IN2L_DIG_VOL_SHIFT, 0xbf, 0, digital_tlv),

SOC_ENUM("Input Ramp Up", arizona_in_vi_ramp),
SOC_ENUM("Input Ramp Down", arizona_in_vd_ramp),

ARIZONA_GAINMUX_CONTROLS("EQ1", ARIZONA_EQ1MIX_INPUT_1_SOURCE),
ARIZONA_GAINMUX_CONTROLS("EQ2", ARIZONA_EQ2MIX_INPUT_1_SOURCE),

SND_SOC_BYTES("EQ1 Coefficients", ARIZONA_EQ1_3, 19),
SOC_SINGLE("EQ1 Mode Switch", ARIZONA_EQ1_2, ARIZONA_EQ1_B1_MODE_SHIFT, 1, 0),
SOC_SINGLE_TLV("EQ1 B1 Volume", ARIZONA_EQ1_1, ARIZONA_EQ1_B1_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ1 B2 Volume", ARIZONA_EQ1_1, ARIZONA_EQ1_B2_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ1 B3 Volume", ARIZONA_EQ1_1, ARIZONA_EQ1_B3_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ1 B4 Volume", ARIZONA_EQ1_2, ARIZONA_EQ1_B4_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ1 B5 Volume", ARIZONA_EQ1_2, ARIZONA_EQ1_B5_GAIN_SHIFT,
	       24, 0, eq_tlv),

SND_SOC_BYTES("EQ2 Coefficients", ARIZONA_EQ2_3, 19),
SOC_SINGLE("EQ2 Mode Switch", ARIZONA_EQ2_2, ARIZONA_EQ2_B1_MODE_SHIFT, 1, 0),
SOC_SINGLE_TLV("EQ2 B1 Volume", ARIZONA_EQ2_1, ARIZONA_EQ2_B1_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ2 B2 Volume", ARIZONA_EQ2_1, ARIZONA_EQ2_B2_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ2 B3 Volume", ARIZONA_EQ2_1, ARIZONA_EQ2_B3_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ2 B4 Volume", ARIZONA_EQ2_2, ARIZONA_EQ2_B4_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ2 B5 Volume", ARIZONA_EQ2_2, ARIZONA_EQ2_B5_GAIN_SHIFT,
	       24, 0, eq_tlv),

ARIZONA_GAINMUX_CONTROLS("DRC1L", ARIZONA_DRC1LMIX_INPUT_1_SOURCE),
ARIZONA_GAINMUX_CONTROLS("DRC1R", ARIZONA_DRC1RMIX_INPUT_1_SOURCE),

SND_SOC_BYTES_MASK("DRC1", ARIZONA_DRC1_CTRL1, 5,
		   ARIZONA_DRC1R_ENA | ARIZONA_DRC1L_ENA),

ARIZONA_MIXER_CONTROLS("LHPF1", ARIZONA_HPLP1MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("LHPF2", ARIZONA_HPLP2MIX_INPUT_1_SOURCE),

SND_SOC_BYTES("LHPF1 Coefficients", ARIZONA_HPLPF1_2, 1),
SND_SOC_BYTES("LHPF2 Coefficients", ARIZONA_HPLPF2_2, 1),

SOC_ENUM("LHPF1 Mode", arizona_lhpf1_mode),
SOC_ENUM("LHPF2 Mode", arizona_lhpf2_mode),

SOC_VALUE_ENUM("Sample Rate 2", arizona_sample_rate[0]),
SOC_VALUE_ENUM("Sample Rate 3", arizona_sample_rate[1]),

SOC_VALUE_ENUM("ISRC1 FSL", arizona_isrc_fsl[0]),
SOC_VALUE_ENUM("ISRC2 FSL", arizona_isrc_fsl[1]),
SOC_VALUE_ENUM("ISRC1 FSH", arizona_isrc_fsh[0]),
SOC_VALUE_ENUM("ISRC2 FSH", arizona_isrc_fsh[1]),
SOC_VALUE_ENUM("ASRC RATE 1", arizona_asrc_rate1),

SOC_SINGLE_TLV("Noise Generator Volume", ARIZONA_COMFORT_NOISE_GENERATOR,
	       ARIZONA_NOISE_GEN_GAIN_SHIFT, 0x16, 0, noise_tlv),

ARIZONA_MIXER_CONTROLS("HPOUTL", ARIZONA_OUT1LMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("HPOUTR", ARIZONA_OUT1RMIX_INPUT_1_SOURCE),
//ARIZONA_MIXER_CONTROLS("LINEOUTL", ARIZONA_OUT2LMIX_INPUT_1_SOURCE),
//ARIZONA_MIXER_CONTROLS("LINEOUTR", ARIZONA_OUT2RMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("EPOUT", ARIZONA_OUT3LMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("SPKOUTL", ARIZONA_OUT4LMIX_INPUT_1_SOURCE),
//ARIZONA_MIXER_CONTROLS("SPKOUTR", ARIZONA_OUT4RMIX_INPUT_1_SOURCE),
//ARIZONA_MIXER_CONTROLS("SPKDATL", ARIZONA_OUT5LMIX_INPUT_1_SOURCE),
//ARIZONA_MIXER_CONTROLS("SPKDATR", ARIZONA_OUT5RMIX_INPUT_1_SOURCE),

SOC_DOUBLE_R("HPOUT Digital Switch", ARIZONA_DAC_DIGITAL_VOLUME_1L,
	     ARIZONA_DAC_DIGITAL_VOLUME_1R, ARIZONA_OUT1L_MUTE_SHIFT, 1, 1),
//SOC_DOUBLE_R("LINEOUT Digital Switch", ARIZONA_DAC_DIGITAL_VOLUME_2L,
//	     ARIZONA_DAC_DIGITAL_VOLUME_2R, ARIZONA_OUT2L_MUTE_SHIFT, 1, 1),
SOC_SINGLE("EPOUT Digital Switch", ARIZONA_DAC_DIGITAL_VOLUME_3L,
	     ARIZONA_OUT3L_MUTE_SHIFT, 1, 1),
SOC_DOUBLE_R("Speaker Digital Switch", ARIZONA_DAC_DIGITAL_VOLUME_4L,
	     ARIZONA_DAC_DIGITAL_VOLUME_4R, ARIZONA_OUT4L_MUTE_SHIFT, 1, 1),
//SOC_DOUBLE_R("SPKDAT Digital Switch", ARIZONA_DAC_DIGITAL_VOLUME_5L,
//	     ARIZONA_DAC_DIGITAL_VOLUME_5R, ARIZONA_OUT5L_MUTE_SHIFT, 1, 1),

SOC_DOUBLE_R_TLV("HPOUT Digital Volume", ARIZONA_DAC_DIGITAL_VOLUME_1L,
		 ARIZONA_DAC_DIGITAL_VOLUME_1R, ARIZONA_OUT1L_VOL_SHIFT,
		 0xbf, 0, digital_tlv),
//SOC_DOUBLE_R_TLV("LINEOUT Digital Volume", ARIZONA_DAC_DIGITAL_VOLUME_2L,
//		 ARIZONA_DAC_DIGITAL_VOLUME_2R, ARIZONA_OUT2L_VOL_SHIFT,
//		 0xbf, 0, digital_tlv),
SOC_SINGLE_TLV("EPOUT Digital Volume", ARIZONA_DAC_DIGITAL_VOLUME_3L,
		 ARIZONA_OUT3L_VOL_SHIFT, 0xbf, 0, digital_tlv),
SOC_DOUBLE_R_TLV("Speaker Digital Volume", ARIZONA_DAC_DIGITAL_VOLUME_4L,
		 ARIZONA_DAC_DIGITAL_VOLUME_4R, ARIZONA_OUT4L_VOL_SHIFT,
		 0xbf, 0, digital_tlv),
//SOC_DOUBLE_R_TLV("SPKDAT Digital Volume", ARIZONA_DAC_DIGITAL_VOLUME_5L,
//		 ARIZONA_DAC_DIGITAL_VOLUME_5R, ARIZONA_OUT5L_VOL_SHIFT,
//		 0xbf, 0, digital_tlv),

//SOC_DOUBLE("SPKDAT Switch", ARIZONA_PDM_SPK1_CTRL_1, ARIZONA_SPK1L_MUTE_SHIFT,
//	   ARIZONA_SPK1R_MUTE_SHIFT, 1, 1),

SOC_DOUBLE("HPOUT DRE Switch", ARIZONA_DRE_ENABLE,
	   ARIZONA_DRE1L_ENA_SHIFT, ARIZONA_DRE1R_ENA_SHIFT, 1, 0),
//SOC_DOUBLE("LINEOUT DRE Switch", ARIZONA_DRE_ENABLE,
//	   ARIZONA_DRE2L_ENA_SHIFT, ARIZONA_DRE2R_ENA_SHIFT, 1, 0),
SOC_SINGLE("EPOUT DRE Switch", ARIZONA_DRE_ENABLE,
	   ARIZONA_DRE3L_ENA_SHIFT, 1, 0),

SOC_SINGLE("DRE Threshold", ARIZONA_DRE_CONTROL_2,
	   ARIZONA_DRE_T_LOW_SHIFT, 63, 0),

SOC_SINGLE("DRE Low Level ABS", ARIZONA_DRE_CONTROL_3,
	   ARIZONA_DRE_LOW_LEVEL_ABS_SHIFT, 15, 0),

SOC_SINGLE("DRE TC Fast", ARIZONA_DRE_CONTROL_1,
	   ARIZONA_DRE_ENV_TC_FAST_SHIFT, 15, 0),

SOC_SINGLE("DRE Analogue Volume Delay", ARIZONA_DRE_CONTROL_2,
	   ARIZONA_DRE_ALOG_VOL_DELAY_SHIFT, 15, 0),

SOC_ENUM("Output Ramp Up", arizona_out_vi_ramp),
SOC_ENUM("Output Ramp Down", arizona_out_vd_ramp),

SOC_SINGLE("Noise Gate Switch", ARIZONA_NOISE_GATE_CONTROL,
	   ARIZONA_NGATE_ENA_SHIFT, 1, 0),
SOC_SINGLE_TLV("Noise Gate Threshold Volume", ARIZONA_NOISE_GATE_CONTROL,
	       ARIZONA_NGATE_THR_SHIFT, 7, 1, ng_tlv),
SOC_ENUM("Noise Gate Hold", arizona_ng_hold),

WM8998_NG_SRC("HPOUTL", ARIZONA_NOISE_GATE_SELECT_1L),
WM8998_NG_SRC("HPOUTR", ARIZONA_NOISE_GATE_SELECT_1R),
//WM8998_NG_SRC("LINEOUTL", ARIZONA_NOISE_GATE_SELECT_2L),
//WM8998_NG_SRC("LINEOUTR", ARIZONA_NOISE_GATE_SELECT_2R),
WM8998_NG_SRC("EPOUT",  ARIZONA_NOISE_GATE_SELECT_3L),
WM8998_NG_SRC("SPKOUTL", ARIZONA_NOISE_GATE_SELECT_4L),
//WM8998_NG_SRC("SPKOUTR", ARIZONA_NOISE_GATE_SELECT_4R),
//WM8998_NG_SRC("SPKDATL", ARIZONA_NOISE_GATE_SELECT_5L),
//WM8998_NG_SRC("SPKDATR", ARIZONA_NOISE_GATE_SELECT_5R),

ARIZONA_MIXER_CONTROLS("AIF1TX1", ARIZONA_AIF1TX1MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF1TX2", ARIZONA_AIF1TX2MIX_INPUT_1_SOURCE),

ARIZONA_MIXER_CONTROLS("AIF2TX1", ARIZONA_AIF2TX1MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF2TX2", ARIZONA_AIF2TX2MIX_INPUT_1_SOURCE),

ARIZONA_MIXER_CONTROLS("AIF3TX1", ARIZONA_AIF3TX1MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF3TX2", ARIZONA_AIF3TX2MIX_INPUT_1_SOURCE),
};

ARIZONA_MUX_ENUMS(EQ1, ARIZONA_EQ1MIX_INPUT_1_SOURCE);
ARIZONA_MUX_ENUMS(EQ2, ARIZONA_EQ2MIX_INPUT_1_SOURCE);

ARIZONA_MUX_ENUMS(DRC1L, ARIZONA_DRC1LMIX_INPUT_1_SOURCE);
ARIZONA_MUX_ENUMS(DRC1R, ARIZONA_DRC1RMIX_INPUT_1_SOURCE);

ARIZONA_MIXER_ENUMS(LHPF1, ARIZONA_HPLP1MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(LHPF2, ARIZONA_HPLP2MIX_INPUT_1_SOURCE);

ARIZONA_MIXER_ENUMS(PWM1, ARIZONA_PWM1MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(PWM2, ARIZONA_PWM2MIX_INPUT_1_SOURCE);

ARIZONA_MIXER_ENUMS(OUT1L, ARIZONA_OUT1LMIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(OUT1R, ARIZONA_OUT1RMIX_INPUT_1_SOURCE);
//ARIZONA_MIXER_ENUMS(OUT2L, ARIZONA_OUT2LMIX_INPUT_1_SOURCE);
//ARIZONA_MIXER_ENUMS(OUT2R, ARIZONA_OUT2RMIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(OUT3,  ARIZONA_OUT3LMIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(SPKOUTL, ARIZONA_OUT4LMIX_INPUT_1_SOURCE);
//ARIZONA_MIXER_ENUMS(SPKOUTR, ARIZONA_OUT4RMIX_INPUT_1_SOURCE);
//ARIZONA_MIXER_ENUMS(SPKDATL, ARIZONA_OUT5LMIX_INPUT_1_SOURCE);
//ARIZONA_MIXER_ENUMS(SPKDATR, ARIZONA_OUT5RMIX_INPUT_1_SOURCE);

ARIZONA_MIXER_ENUMS(AIF1TX1, ARIZONA_AIF1TX1MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(AIF1TX2, ARIZONA_AIF1TX2MIX_INPUT_1_SOURCE);

ARIZONA_MIXER_ENUMS(AIF2TX1, ARIZONA_AIF2TX1MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(AIF2TX2, ARIZONA_AIF2TX2MIX_INPUT_1_SOURCE);

ARIZONA_MIXER_ENUMS(AIF3TX1, ARIZONA_AIF3TX1MIX_INPUT_1_SOURCE);
ARIZONA_MIXER_ENUMS(AIF3TX2, ARIZONA_AIF3TX2MIX_INPUT_1_SOURCE);

ARIZONA_MUX_ENUMS(ASRC1L, ARIZONA_ASRC1LMIX_INPUT_1_SOURCE);
ARIZONA_MUX_ENUMS(ASRC1R, ARIZONA_ASRC1RMIX_INPUT_1_SOURCE);
ARIZONA_MUX_ENUMS(ASRC2L, ARIZONA_ASRC2LMIX_INPUT_1_SOURCE);
ARIZONA_MUX_ENUMS(ASRC2R, ARIZONA_ASRC2RMIX_INPUT_1_SOURCE);

ARIZONA_MUX_ENUMS(ISRC1INT1, ARIZONA_ISRC1INT1MIX_INPUT_1_SOURCE);
ARIZONA_MUX_ENUMS(ISRC1INT2, ARIZONA_ISRC1INT2MIX_INPUT_1_SOURCE);
ARIZONA_MUX_ENUMS(ISRC1INT3, ARIZONA_ISRC1INT3MIX_INPUT_1_SOURCE);
ARIZONA_MUX_ENUMS(ISRC1INT4, ARIZONA_ISRC1INT4MIX_INPUT_1_SOURCE);

ARIZONA_MUX_ENUMS(ISRC1DEC1, ARIZONA_ISRC1DEC1MIX_INPUT_1_SOURCE);
ARIZONA_MUX_ENUMS(ISRC1DEC2, ARIZONA_ISRC1DEC2MIX_INPUT_1_SOURCE);
ARIZONA_MUX_ENUMS(ISRC1DEC3, ARIZONA_ISRC1DEC3MIX_INPUT_1_SOURCE);
ARIZONA_MUX_ENUMS(ISRC1DEC4, ARIZONA_ISRC1DEC4MIX_INPUT_1_SOURCE);

ARIZONA_MUX_ENUMS(ISRC2INT1, ARIZONA_ISRC2INT1MIX_INPUT_1_SOURCE);
ARIZONA_MUX_ENUMS(ISRC2INT2, ARIZONA_ISRC2INT2MIX_INPUT_1_SOURCE);

ARIZONA_MUX_ENUMS(ISRC2DEC1, ARIZONA_ISRC2DEC1MIX_INPUT_1_SOURCE);
ARIZONA_MUX_ENUMS(ISRC2DEC2, ARIZONA_ISRC2DEC2MIX_INPUT_1_SOURCE);

static const char *wm8998_aec_loopback_texts[] = {
	"HPOUTL", "HPOUTR", "EPOUT",
	"SPKOUTL",
};

static const unsigned int wm8998_aec_loopback_values[] = {
	0, 1, 4, 6,
};

static const SOC_VALUE_ENUM_SINGLE_DECL(wm8998_aec1_loopback,
					ARIZONA_DAC_AEC_CONTROL_1,
					ARIZONA_AEC_LOOPBACK_SRC_SHIFT, 0xf,
					wm8998_aec_loopback_texts,
					wm8998_aec_loopback_values);

static const SOC_VALUE_ENUM_SINGLE_DECL(wm8998_aec2_loopback,
					ARIZONA_DAC_AEC_CONTROL_2,
					ARIZONA_AEC_LOOPBACK_SRC_SHIFT, 0xf,
					wm8998_aec_loopback_texts,
					wm8998_aec_loopback_values);

static const struct snd_kcontrol_new wm8998_aec_loopback_mux[] = {
	SOC_DAPM_VALUE_ENUM("AEC1 Loopback", wm8998_aec1_loopback),
	SOC_DAPM_VALUE_ENUM("AEC2 Loopback", wm8998_aec2_loopback),
};

static const struct snd_soc_dapm_widget wm8998_dapm_widgets[] = {
SND_SOC_DAPM_SUPPLY("SYSCLK", ARIZONA_SYSTEM_CLOCK_1,
		    ARIZONA_SYSCLK_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY("ASYNCCLK", ARIZONA_ASYNC_CLOCK_1,
		    ARIZONA_ASYNC_CLK_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY("OPCLK", ARIZONA_OUTPUT_SYSTEM_CLOCK,
		    ARIZONA_OPCLK_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY("ASYNCOPCLK", ARIZONA_OUTPUT_ASYNC_CLOCK,
		    ARIZONA_OPCLK_ASYNC_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_REGULATOR_SUPPLY("DBVDD2", 0, 0),
SND_SOC_DAPM_REGULATOR_SUPPLY("DBVDD3", 0, 0),
SND_SOC_DAPM_REGULATOR_SUPPLY("CPVDD", 20, 0),
SND_SOC_DAPM_REGULATOR_SUPPLY("MICVDD", 0, 0),
SND_SOC_DAPM_REGULATOR_SUPPLY("SPKVDDL", 0, 0),
SND_SOC_DAPM_REGULATOR_SUPPLY("SPKVDDR", 0, 0),

SND_SOC_DAPM_SIGGEN("TONE"),
SND_SOC_DAPM_SIGGEN("NOISE"),
SND_SOC_DAPM_SIGGEN("HAPTICS"),

SND_SOC_DAPM_INPUT("IN1AL"),
SND_SOC_DAPM_INPUT("IN1AR"),
SND_SOC_DAPM_INPUT("IN1BL"),
SND_SOC_DAPM_INPUT("IN1BR"),
SND_SOC_DAPM_INPUT("IN2A"),
SND_SOC_DAPM_INPUT("IN2B"),

SND_SOC_DAPM_MUX_E("IN1MUXL Input", SND_SOC_NOPM, 0, 0, &wm8998_in1mux[0],
			wm8998_in1mux_ev, SND_SOC_DAPM_PRE_PMU),
SND_SOC_DAPM_MUX_E("IN1MUXR Input", SND_SOC_NOPM, 0, 0, &wm8998_in1mux[1],
			wm8998_in1mux_ev, SND_SOC_DAPM_PRE_PMU),
SND_SOC_DAPM_MUX_E("IN2MUX Input", SND_SOC_NOPM, 0, 0, &wm8998_in2mux,
			wm8998_in2mux_ev, SND_SOC_DAPM_PRE_PMU),

SND_SOC_DAPM_OUTPUT("DRC1 Signal Activity"),

SND_SOC_DAPM_PGA_E("IN1L PGA", ARIZONA_INPUT_ENABLES, ARIZONA_IN1L_ENA_SHIFT,
		   0, NULL, 0, arizona_in_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("IN1R PGA", ARIZONA_INPUT_ENABLES, ARIZONA_IN1R_ENA_SHIFT,
		   0, NULL, 0, arizona_in_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("IN2 PGA", ARIZONA_INPUT_ENABLES, ARIZONA_IN2L_ENA_SHIFT,
		   0, NULL, 0, arizona_in_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),

SND_SOC_DAPM_SUPPLY("MICBIAS1", ARIZONA_MIC_BIAS_CTRL_1,
		    ARIZONA_MICB1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY("MICBIAS2", ARIZONA_MIC_BIAS_CTRL_2,
		    ARIZONA_MICB1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY("MICBIAS3", ARIZONA_MIC_BIAS_CTRL_3,
		    ARIZONA_MICB1_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("Noise Generator", ARIZONA_COMFORT_NOISE_GENERATOR,
		 ARIZONA_NOISE_GEN_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("Tone Generator 1", ARIZONA_TONE_GENERATOR_1,
		 ARIZONA_TONE1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("Tone Generator 2", ARIZONA_TONE_GENERATOR_1,
		 ARIZONA_TONE2_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("EQ1", ARIZONA_EQ1_1, ARIZONA_EQ1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("EQ2", ARIZONA_EQ2_1, ARIZONA_EQ2_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("DRC1L", ARIZONA_DRC1_CTRL1, ARIZONA_DRC1L_ENA_SHIFT, 0,
		 NULL, 0),
SND_SOC_DAPM_PGA("DRC1R", ARIZONA_DRC1_CTRL1, ARIZONA_DRC1R_ENA_SHIFT, 0,
		 NULL, 0),

SND_SOC_DAPM_PGA("LHPF1", ARIZONA_HPLPF1_1, ARIZONA_LHPF1_ENA_SHIFT, 0,
		 NULL, 0),
SND_SOC_DAPM_PGA("LHPF2", ARIZONA_HPLPF2_1, ARIZONA_LHPF2_ENA_SHIFT, 0,
		 NULL, 0),

SND_SOC_DAPM_PGA("PWM1 Driver", ARIZONA_PWM_DRIVE_1, ARIZONA_PWM1_ENA_SHIFT,
		 0, NULL, 0),
SND_SOC_DAPM_PGA("PWM2 Driver", ARIZONA_PWM_DRIVE_1, ARIZONA_PWM2_ENA_SHIFT,
		 0, NULL, 0),

SND_SOC_DAPM_PGA("ASRC1L", ARIZONA_ASRC_ENABLE, ARIZONA_ASRC1L_ENA_SHIFT, 0,
		 NULL, 0),
SND_SOC_DAPM_PGA("ASRC1R", ARIZONA_ASRC_ENABLE, ARIZONA_ASRC1R_ENA_SHIFT, 0,
		 NULL, 0),
SND_SOC_DAPM_PGA("ASRC2L", ARIZONA_ASRC_ENABLE, ARIZONA_ASRC2L_ENA_SHIFT, 0,
		 NULL, 0),
SND_SOC_DAPM_PGA("ASRC2R", ARIZONA_ASRC_ENABLE, ARIZONA_ASRC2R_ENA_SHIFT, 0,
		 NULL, 0),

SND_SOC_DAPM_PGA("ISRC1INT1", ARIZONA_ISRC_1_CTRL_3,
		 ARIZONA_ISRC1_INT0_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC1INT2", ARIZONA_ISRC_1_CTRL_3,
		 ARIZONA_ISRC1_INT1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC1INT3", ARIZONA_ISRC_1_CTRL_3,
		 ARIZONA_ISRC1_INT2_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC1INT4", ARIZONA_ISRC_1_CTRL_3,
		 ARIZONA_ISRC1_INT3_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("ISRC1DEC1", ARIZONA_ISRC_1_CTRL_3,
		 ARIZONA_ISRC1_DEC0_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC1DEC2", ARIZONA_ISRC_1_CTRL_3,
		 ARIZONA_ISRC1_DEC1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC1DEC3", ARIZONA_ISRC_1_CTRL_3,
		 ARIZONA_ISRC1_DEC2_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC1DEC4", ARIZONA_ISRC_1_CTRL_3,
		 ARIZONA_ISRC1_DEC3_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("ISRC2INT1", ARIZONA_ISRC_2_CTRL_3,
		 ARIZONA_ISRC2_INT0_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC2INT2", ARIZONA_ISRC_2_CTRL_3,
		 ARIZONA_ISRC2_INT1_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("ISRC2DEC1", ARIZONA_ISRC_2_CTRL_3,
		 ARIZONA_ISRC2_DEC0_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC2DEC2", ARIZONA_ISRC_2_CTRL_3,
		 ARIZONA_ISRC2_DEC1_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_VALUE_MUX("AEC1 Loopback", ARIZONA_DAC_AEC_CONTROL_1,
		       ARIZONA_AEC_LOOPBACK_ENA_SHIFT, 0,
		       &wm8998_aec_loopback_mux[0]),

SND_SOC_DAPM_VALUE_MUX("AEC2 Loopback", ARIZONA_DAC_AEC_CONTROL_2,
		       ARIZONA_AEC_LOOPBACK_ENA_SHIFT, 0,
		       &wm8998_aec_loopback_mux[1]),

SND_SOC_DAPM_AIF_OUT("AIF1TX1", NULL, 0,
		     ARIZONA_AIF1_TX_ENABLES, ARIZONA_AIF1TX1_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF1TX2", NULL, 0,
		     ARIZONA_AIF1_TX_ENABLES, ARIZONA_AIF1TX2_ENA_SHIFT, 0),

SND_SOC_DAPM_AIF_IN("AIF1RX1", NULL, 0,
		    ARIZONA_AIF1_RX_ENABLES, ARIZONA_AIF1RX1_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF1RX2", NULL, 0,
		    ARIZONA_AIF1_RX_ENABLES, ARIZONA_AIF1RX2_ENA_SHIFT, 0),

SND_SOC_DAPM_AIF_OUT("AIF2TX1", NULL, 0,
		     ARIZONA_AIF2_TX_ENABLES, ARIZONA_AIF2TX1_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF2TX2", NULL, 0,
		     ARIZONA_AIF2_TX_ENABLES, ARIZONA_AIF2TX2_ENA_SHIFT, 0),

SND_SOC_DAPM_AIF_IN("AIF2RX1", NULL, 0,
		    ARIZONA_AIF2_RX_ENABLES, ARIZONA_AIF2RX1_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF2RX2", NULL, 0,
		    ARIZONA_AIF2_RX_ENABLES, ARIZONA_AIF2RX2_ENA_SHIFT, 0),

SND_SOC_DAPM_AIF_OUT("AIF3TX1", NULL, 0,
		     ARIZONA_AIF3_TX_ENABLES, ARIZONA_AIF3TX1_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF3TX2", NULL, 0,
		     ARIZONA_AIF3_TX_ENABLES, ARIZONA_AIF3TX2_ENA_SHIFT, 0),

SND_SOC_DAPM_AIF_IN("AIF3RX1", NULL, 0,
		    ARIZONA_AIF3_RX_ENABLES, ARIZONA_AIF3RX1_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF3RX2", NULL, 0,
		    ARIZONA_AIF3_RX_ENABLES, ARIZONA_AIF3RX2_ENA_SHIFT, 0),

SND_SOC_DAPM_PGA_E("OUT1L", SND_SOC_NOPM,
		   ARIZONA_OUT1L_ENA_SHIFT, 0, NULL, 0, arizona_hp_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("OUT1R", SND_SOC_NOPM,
		   ARIZONA_OUT1R_ENA_SHIFT, 0, NULL, 0, arizona_hp_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
//SND_SOC_DAPM_PGA_E("OUT2L", ARIZONA_OUTPUT_ENABLES_1,
//		   ARIZONA_OUT2L_ENA_SHIFT, 0, NULL, 0, arizona_out_ev,
//		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
//SND_SOC_DAPM_PGA_E("OUT2R", ARIZONA_OUTPUT_ENABLES_1,
//		   ARIZONA_OUT2R_ENA_SHIFT, 0, NULL, 0, arizona_out_ev,
//		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("OUT3", ARIZONA_OUTPUT_ENABLES_1,
		   ARIZONA_OUT3L_ENA_SHIFT, 0, NULL, 0, arizona_out_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
//SND_SOC_DAPM_PGA_E("OUT5L", ARIZONA_OUTPUT_ENABLES_1,
//		   ARIZONA_OUT5L_ENA_SHIFT, 0, NULL, 0, arizona_out_ev,
//		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
//SND_SOC_DAPM_PGA_E("OUT5R", ARIZONA_OUTPUT_ENABLES_1,
//		   ARIZONA_OUT5R_ENA_SHIFT, 0, NULL, 0, arizona_out_ev,
//		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),

ARIZONA_MUX_WIDGETS(EQ1, "EQ1"),
ARIZONA_MUX_WIDGETS(EQ2, "EQ2"),

ARIZONA_MUX_WIDGETS(DRC1L, "DRC1L"),
ARIZONA_MUX_WIDGETS(DRC1R, "DRC1R"),

ARIZONA_MIXER_WIDGETS(LHPF1, "LHPF1"),
ARIZONA_MIXER_WIDGETS(LHPF2, "LHPF2"),

ARIZONA_MIXER_WIDGETS(PWM1, "PWM1"),
ARIZONA_MIXER_WIDGETS(PWM2, "PWM2"),

ARIZONA_MIXER_WIDGETS(OUT1L, "HPOUTL"),
ARIZONA_MIXER_WIDGETS(OUT1R, "HPOUTR"),
//ARIZONA_MIXER_WIDGETS(OUT2L, "LINEOUTL"),
//ARIZONA_MIXER_WIDGETS(OUT2R, "LINEOUTR"),
ARIZONA_MIXER_WIDGETS(OUT3, "EPOUT"),
ARIZONA_MIXER_WIDGETS(SPKOUTL, "SPKOUTL"),
//ARIZONA_MIXER_WIDGETS(SPKOUTR, "SPKOUTR"),
//ARIZONA_MIXER_WIDGETS(SPKDATL, "SPKDATL"),
//ARIZONA_MIXER_WIDGETS(SPKDATR, "SPKDATR"),

ARIZONA_MIXER_WIDGETS(AIF1TX1, "AIF1TX1"),
ARIZONA_MIXER_WIDGETS(AIF1TX2, "AIF1TX2"),

ARIZONA_MIXER_WIDGETS(AIF2TX1, "AIF2TX1"),
ARIZONA_MIXER_WIDGETS(AIF2TX2, "AIF2TX2"),

ARIZONA_MIXER_WIDGETS(AIF3TX1, "AIF3TX1"),
ARIZONA_MIXER_WIDGETS(AIF3TX2, "AIF3TX2"),

ARIZONA_MUX_WIDGETS(ASRC1L, "ASRC1L"),
ARIZONA_MUX_WIDGETS(ASRC1R, "ASRC1R"),
ARIZONA_MUX_WIDGETS(ASRC2L, "ASRC2L"),
ARIZONA_MUX_WIDGETS(ASRC2R, "ASRC2R"),

ARIZONA_MUX_WIDGETS(ISRC1DEC1, "ISRC1DEC1"),
ARIZONA_MUX_WIDGETS(ISRC1DEC2, "ISRC1DEC2"),
ARIZONA_MUX_WIDGETS(ISRC1DEC3, "ISRC1DEC3"),
ARIZONA_MUX_WIDGETS(ISRC1DEC4, "ISRC1DEC4"),

ARIZONA_MUX_WIDGETS(ISRC1INT1, "ISRC1INT1"),
ARIZONA_MUX_WIDGETS(ISRC1INT2, "ISRC1INT2"),
ARIZONA_MUX_WIDGETS(ISRC1INT3, "ISRC1INT3"),
ARIZONA_MUX_WIDGETS(ISRC1INT4, "ISRC1INT4"),

ARIZONA_MUX_WIDGETS(ISRC2DEC1, "ISRC2DEC1"),
ARIZONA_MUX_WIDGETS(ISRC2DEC2, "ISRC2DEC2"),

ARIZONA_MUX_WIDGETS(ISRC2INT1, "ISRC2INT1"),
ARIZONA_MUX_WIDGETS(ISRC2INT2, "ISRC2INT2"),

SND_SOC_DAPM_OUTPUT("HPOUTL"),
SND_SOC_DAPM_OUTPUT("HPOUTR"),
//SND_SOC_DAPM_OUTPUT("LINEOUTL"),
//SND_SOC_DAPM_OUTPUT("LINEOUTR"),
SND_SOC_DAPM_OUTPUT("EPOUT"),
SND_SOC_DAPM_OUTPUT("SPKOUTLN"),
SND_SOC_DAPM_OUTPUT("SPKOUTLP"),
//SND_SOC_DAPM_OUTPUT("SPKOUTRN"),
//SND_SOC_DAPM_OUTPUT("SPKOUTRP"),
//SND_SOC_DAPM_OUTPUT("SPKDATL"),
//SND_SOC_DAPM_OUTPUT("SPKDATR"),

SND_SOC_DAPM_OUTPUT("MICSUPP"),
};

#define ARIZONA_MIXER_INPUT_ROUTES(name)	\
	{ name, "Noise Generator", "Noise Generator" }, \
	{ name, "Tone Generator 1", "Tone Generator 1" }, \
	{ name, "Tone Generator 2", "Tone Generator 2" }, \
	{ name, "Haptics", "HAPTICS" }, \
	{ name, "AEC", "AEC1 Loopback" }, \
	{ name, "AEC2", "AEC2 Loopback" }, \
	{ name, "IN1L", "IN1L PGA" }, \
	{ name, "IN1R", "IN1R PGA" }, \
	{ name, "IN2L", "IN2 PGA" }, \
	{ name, "AIF1RX1", "AIF1RX1" }, \
	{ name, "AIF1RX2", "AIF1RX2" }, \
	{ name, "AIF2RX1", "AIF2RX1" }, \
	{ name, "AIF2RX2", "AIF2RX2" }, \
	{ name, "AIF3RX1", "AIF3RX1" }, \
	{ name, "AIF3RX2", "AIF3RX2" }, \
	{ name, "EQ1", "EQ1" }, \
	{ name, "EQ2", "EQ2" }, \
	{ name, "DRC1L", "DRC1L" }, \
	{ name, "DRC1R", "DRC1R" }, \
	{ name, "LHPF1", "LHPF1" }, \
	{ name, "LHPF2", "LHPF2" }, \
	{ name, "ASRC1L", "ASRC1L" }, \
	{ name, "ASRC1R", "ASRC1R" }, \
	{ name, "ASRC2L", "ASRC2L" }, \
	{ name, "ASRC2R", "ASRC2R" }, \
	{ name, "ISRC1DEC1", "ISRC1DEC1" }, \
	{ name, "ISRC1DEC2", "ISRC1DEC2" }, \
	{ name, "ISRC1DEC3", "ISRC1DEC3" }, \
	{ name, "ISRC1DEC4", "ISRC1DEC4" }, \
	{ name, "ISRC1INT1", "ISRC1INT1" }, \
	{ name, "ISRC1INT2", "ISRC1INT2" }, \
	{ name, "ISRC1INT3", "ISRC1INT3" }, \
	{ name, "ISRC1INT4", "ISRC1INT4" }, \
	{ name, "ISRC2DEC1", "ISRC2DEC1" }, \
	{ name, "ISRC2DEC2", "ISRC2DEC2" }, \
	{ name, "ISRC2INT1", "ISRC2INT1" }, \
	{ name, "ISRC2INT2", "ISRC2INT2" }

static const struct snd_soc_dapm_route wm8998_dapm_routes[] = {
	{ "AIF2 Capture", NULL, "DBVDD2" },
	{ "AIF2 Playback", NULL, "DBVDD2" },

	{ "AIF3 Capture", NULL, "DBVDD3" },
	{ "AIF3 Playback", NULL, "DBVDD3" },

	{ "OUT1L", NULL, "CPVDD" },
	{ "OUT1R", NULL, "CPVDD" },
//	{ "OUT2L", NULL, "CPVDD" },
//	{ "OUT2R", NULL, "CPVDD" },
	{ "OUT3",  NULL, "CPVDD" },

	{ "OUT4L", NULL, "SPKVDDL" },
//	{ "OUT4R", NULL, "SPKVDDR" },

	{ "OUT1L", NULL, "SYSCLK" },
	{ "OUT1R", NULL, "SYSCLK" },
//	{ "OUT2L", NULL, "SYSCLK" },
//	{ "OUT2R", NULL, "SYSCLK" },
	{ "OUT3",  NULL, "SYSCLK" },
	{ "OUT4L", NULL, "SYSCLK" },
//	{ "OUT4R", NULL, "SYSCLK" },
//	{ "OUT5L", NULL, "SYSCLK" },
//	{ "OUT5R", NULL, "SYSCLK" },

	{ "MICBIAS1", NULL, "MICVDD" },
	{ "MICBIAS2", NULL, "MICVDD" },
	{ "MICBIAS3", NULL, "MICVDD" },

	{ "Noise Generator", NULL, "NOISE" },
	{ "Tone Generator 1", NULL, "TONE" },
	{ "Tone Generator 2", NULL, "TONE" },

	{ "AIF1 Capture", NULL, "AIF1TX1" },
	{ "AIF1 Capture", NULL, "AIF1TX2" },

	{ "AIF1RX1", NULL, "AIF1 Playback" },
	{ "AIF1RX2", NULL, "AIF1 Playback" },

	{ "AIF2 Capture", NULL, "AIF2TX1" },
	{ "AIF2 Capture", NULL, "AIF2TX2" },

	{ "AIF2RX1", NULL, "AIF2 Playback" },
	{ "AIF2RX2", NULL, "AIF2 Playback" },

	{ "AIF3 Capture", NULL, "AIF3TX1" },
	{ "AIF3 Capture", NULL, "AIF3TX2" },

	{ "AIF3RX1", NULL, "AIF3 Playback" },
	{ "AIF3RX2", NULL, "AIF3 Playback" },

	{ "AIF1 Playback", NULL, "SYSCLK" },
	{ "AIF2 Playback", NULL, "SYSCLK" },
	{ "AIF3 Playback", NULL, "SYSCLK" },

	{ "AIF1 Capture", NULL, "SYSCLK" },
	{ "AIF2 Capture", NULL, "SYSCLK" },
	{ "AIF3 Capture", NULL, "SYSCLK" },

	{ "IN1MUXL Input", "IN1A", "IN1AL" },
	{ "IN1MUXR Input", "IN1A", "IN1AR" },
	{ "IN1MUXL Input", "IN1B", "IN1BL" },
	{ "IN1MUXR Input", "IN1B", "IN1BR" },

	{ "IN2MUX Input", "IN2A", "IN2A" },
	{ "IN2MUX Input", "IN2B", "IN2B" },

	{ "IN1L PGA", NULL, "IN1MUXL Input" },
	{ "IN1R PGA", NULL, "IN1MUXR Input" },
	{ "IN2 PGA",  NULL, "IN2MUX Input" },

	ARIZONA_MIXER_ROUTES("OUT1L", "HPOUTL"),
	ARIZONA_MIXER_ROUTES("OUT1R", "HPOUTR"),
//	ARIZONA_MIXER_ROUTES("OUT2L", "LINEOUTL"),
//	ARIZONA_MIXER_ROUTES("OUT2R", "LINEOUTR"),
	ARIZONA_MIXER_ROUTES("OUT3",  "EPOUT"),

	ARIZONA_MIXER_ROUTES("OUT4L", "SPKOUTL"),
//	ARIZONA_MIXER_ROUTES("OUT4R", "SPKOUTR"),
//	ARIZONA_MIXER_ROUTES("OUT5L", "SPKDATL"),
//	ARIZONA_MIXER_ROUTES("OUT5R", "SPKDATR"),

	ARIZONA_MIXER_ROUTES("PWM1 Driver", "PWM1"),
	ARIZONA_MIXER_ROUTES("PWM2 Driver", "PWM2"),

	ARIZONA_MIXER_ROUTES("AIF1TX1", "AIF1TX1"),
	ARIZONA_MIXER_ROUTES("AIF1TX2", "AIF1TX2"),

	ARIZONA_MIXER_ROUTES("AIF2TX1", "AIF2TX1"),
	ARIZONA_MIXER_ROUTES("AIF2TX2", "AIF2TX2"),

	ARIZONA_MIXER_ROUTES("AIF3TX1", "AIF3TX1"),
	ARIZONA_MIXER_ROUTES("AIF3TX2", "AIF3TX2"),

	ARIZONA_MUX_ROUTES("EQ1", "EQ1"),
	ARIZONA_MUX_ROUTES("EQ2", "EQ2"),

	ARIZONA_MUX_ROUTES("DRC1L", "DRC1L"),
	ARIZONA_MUX_ROUTES("DRC1R", "DRC1R"),

	ARIZONA_MIXER_ROUTES("LHPF1", "LHPF1"),
	ARIZONA_MIXER_ROUTES("LHPF2", "LHPF2"),

	ARIZONA_MUX_ROUTES("ASRC1L", "ASRC1L"),
	ARIZONA_MUX_ROUTES("ASRC1R", "ASRC1R"),
	ARIZONA_MUX_ROUTES("ASRC2L", "ASRC2L"),
	ARIZONA_MUX_ROUTES("ASRC2R", "ASRC2R"),

	ARIZONA_MUX_ROUTES("ISRC1INT1", "ISRC1INT1"),
	ARIZONA_MUX_ROUTES("ISRC1INT2", "ISRC1INT2"),
	ARIZONA_MUX_ROUTES("ISRC1INT3", "ISRC1INT3"),
	ARIZONA_MUX_ROUTES("ISRC1INT4", "ISRC1INT4"),

	ARIZONA_MUX_ROUTES("ISRC1DEC1", "ISRC1DEC1"),
	ARIZONA_MUX_ROUTES("ISRC1DEC2", "ISRC1DEC2"),
	ARIZONA_MUX_ROUTES("ISRC1DEC3", "ISRC1DEC3"),
	ARIZONA_MUX_ROUTES("ISRC1DEC4", "ISRC1DEC4"),

	ARIZONA_MUX_ROUTES("ISRC2INT1", "ISRC2INT1"),
	ARIZONA_MUX_ROUTES("ISRC2INT2", "ISRC2INT2"),

	ARIZONA_MUX_ROUTES("ISRC2DEC1", "ISRC2DEC1"),
	ARIZONA_MUX_ROUTES("ISRC2DEC2", "ISRC2DEC2"),

	{ "AEC1 Loopback", "HPOUTL", "OUT1L" },
	{ "AEC1 Loopback", "HPOUTR", "OUT1R" },
	{ "AEC2 Loopback", "HPOUTL", "OUT1L" },
	{ "AEC2 Loopback", "HPOUTR", "OUT1R" },
	{ "HPOUTL", NULL, "OUT1L" },
	{ "HPOUTR", NULL, "OUT1R" },

//	{ "AEC1 Loopback", "LINEOUTL", "OUT2L" },
//	{ "AEC1 Loopback", "LINEOUTR", "OUT2R" },
//	{ "AEC2 Loopback", "LINEOUTL", "OUT2L" },
//	{ "AEC2 Loopback", "LINEOUTR", "OUT2R" },
//	{ "LINEOUTL", NULL, "OUT2L" },
//	{ "LINEOUTR", NULL, "OUT2R" },

	{ "AEC1 Loopback", "EPOUT", "OUT3" },
	{ "AEC2 Loopback", "EPOUT", "OUT3" },
	{ "EPOUT", NULL, "OUT3" },

	{ "AEC1 Loopback", "SPKOUTL", "OUT4L" },
	{ "AEC2 Loopback", "SPKOUTL", "OUT4L" },
	{ "SPKOUTLN", NULL, "OUT4L" },
	{ "SPKOUTLP", NULL, "OUT4L" },

//	{ "AEC1 Loopback", "SPKOUTR", "OUT4R" },
//	{ "AEC2 Loopback", "SPKOUTR", "OUT4R" },
//	{ "SPKOUTRN", NULL, "OUT4R" },
//	{ "SPKOUTRP", NULL, "OUT4R" },

//	{ "AEC1 Loopback", "SPKDATL", "OUT5L" },
//	{ "AEC1 Loopback", "SPKDATR", "OUT5R" },
//	{ "AEC2 Loopback", "SPKDATL", "OUT5L" },
//	{ "AEC2 Loopback", "SPKDATR", "OUT5R" },
//	{ "SPKDATL", NULL, "OUT5L" },
//	{ "SPKDATR", NULL, "OUT5R" },

	{ "DRC1 Signal Activity", NULL, "DRC1L" },
	{ "DRC1 Signal Activity", NULL, "DRC1R" },

	{"MICSUPP", NULL, "SYSCLK"},
};

static ssize_t wm8998_show_property(struct device *dev,
                                      struct device_attribute *attr,
                                      char *buf);
static ssize_t wm8998_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count);

#define WM8998_CODEC_ATTR(_name)\
{\
    .attr = { .name = #_name, .mode = S_IRUGO | S_IWUSR},\
    .show = wm8998_show_property,\
    .store = wm8998_store,\
}

static struct device_attribute wm8998_attrs[] = {
    WM8998_CODEC_ATTR(registers),
};
enum {
    WM8998_REG_PROGRAM,
};

#define WM8998_READABLE_REGISTER 0x1443
static ssize_t wm8998_show_property(struct device *dev,
                                      struct device_attribute *attr,
                                      char *buf)
{
	int i = 0, ret;
	unsigned int reg, val;
	size_t off;
	struct arizona *arizona = dev_get_drvdata(dev);

	pm_runtime_get_sync(dev);
	off = attr - wm8998_attrs;
	switch (off) {
	case WM8998_REG_PROGRAM:
		for (reg = 0; reg < WM8998_READABLE_REGISTER; reg++) {
			if (wm8998_i2c_regmap.readable_reg(dev, reg)) {
				ret = regmap_read(arizona->regmap, reg, &val);
				if (ret != 0) {
					dev_err(dev, "Failed to read register %08x, %d\n", reg, ret);
					pm_runtime_put(dev);
					return -EIO;
				}
				printk("Codec Reg 0x%.4X = 0x%.4X\n", reg, val);
			}
		}
		i += scnprintf(buf+i, PAGE_SIZE-i, "End Read\n");
		break;
	default:
		i += scnprintf(buf+i, PAGE_SIZE-i, "Error\n");
		break;
	}
	pm_runtime_put(dev);

	return i;
}

static ssize_t wm8998_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	unsigned int reg, val;
	int ret = 0;
	size_t off;
	struct arizona *arizona = dev_get_drvdata(dev);

	pm_runtime_get_sync(dev);
	off = attr - wm8998_attrs;
	switch(off){
	case WM8998_REG_PROGRAM:
		if (sscanf(buf, "%x=%x", &reg, &val) == 2) {
			regmap_write(arizona->regmap, reg, val);
		} else  if (sscanf(buf, "%x=", &reg) == 1) {
			ret = regmap_read(arizona->regmap, reg, &val);
			if (ret != 0) {
				dev_err(dev, "Failed to read register %08x, %d\n", reg, ret);
				pm_runtime_put(dev);
				return -EIO;
			}
			printk("Codec Reg 0x%.4X = 0x%.4X\n", reg, val);
		}
		ret = count;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	pm_runtime_put(dev);
	return ret;
}

static int wm8998_create_attrs(struct device * dev)
{
	int i, rc;

	for (i = 0; i < ARRAY_SIZE(wm8998_attrs); i++) {
		rc = device_create_file(dev, &wm8998_attrs[i]);
		if (rc)
			goto wm8998_attrs_failed;
	}
	goto succeed;

wm8998_attrs_failed:
	printk(KERN_INFO "%s(): failed!!!\n", __func__);
	while (i--)
		device_remove_file(dev, &wm8998_attrs[i]);
succeed:
	return rc;

}

static void wm8998_destroy_atts(struct device * dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(wm8998_attrs); i++)
		device_remove_file(dev, &wm8998_attrs[i]);
}

#define WM8998_RATES SNDRV_PCM_RATE_8000_192000

#define WM8998_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |\
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver wm8998_dai[] = {
	{
		.name = "wm8998-aif1",
		.id = 1,
		.base = ARIZONA_AIF1_BCLK_CTRL,
		.playback = {
			.stream_name = "AIF1 Playback",
			.channels_min = 1,
			.channels_max = 6,
			.rates = WM8998_RATES,
			.formats = WM8998_FORMATS,
		},
		.capture = {
			 .stream_name = "AIF1 Capture",
			 .channels_min = 1,
			 .channels_max = 6,
			 .rates = WM8998_RATES,
			 .formats = WM8998_FORMATS,
		 },
		.ops = &arizona_dai_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "wm8998-aif2",
		.id = 2,
		.base = ARIZONA_AIF2_BCLK_CTRL,
		.playback = {
			.stream_name = "AIF2 Playback",
			.channels_min = 1,
			.channels_max = 6,
			.rates = WM8998_RATES,
			.formats = WM8998_FORMATS,
		},
		.capture = {
			 .stream_name = "AIF2 Capture",
			 .channels_min = 1,
			 .channels_max = 6,
			 .rates = WM8998_RATES,
			 .formats = WM8998_FORMATS,
		 },
		.ops = &arizona_dai_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "wm8998-aif3",
		.id = 3,
		.base = ARIZONA_AIF3_BCLK_CTRL,
		.playback = {
			.stream_name = "AIF3 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = WM8998_RATES,
			.formats = WM8998_FORMATS,
		},
		.capture = {
			 .stream_name = "AIF3 Capture",
			 .channels_min = 1,
			 .channels_max = 2,
			 .rates = WM8998_RATES,
			 .formats = WM8998_FORMATS,
		 },
		.ops = &arizona_dai_ops,
		.symmetric_rates = 1,
	},
#if 0
	{
		.name = "wm8998-slim1",
		.id = 4,
		.playback = {
			.stream_name = "Slim1 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = WM8998_RATES,
			.formats = WM8998_FORMATS,
		},
		.capture = {
			 .stream_name = "Slim1 Capture",
			 .channels_min = 1,
			 .channels_max = 4,
			 .rates = WM8998_RATES,
			 .formats = WM8998_FORMATS,
		 },
		.ops = &arizona_simple_dai_ops,
	},
	{
		.name = "wm8998-slim2",
		.id = 5,
		.playback = {
			.stream_name = "Slim2 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = WM8998_RATES,
			.formats = WM8998_FORMATS,
		},
		.capture = {
			 .stream_name = "Slim2 Capture",
			 .channels_min = 1,
			 .channels_max = 2,
			 .rates = WM8998_RATES,
			 .formats = WM8998_FORMATS,
		 },
		.ops = &arizona_simple_dai_ops,
	},
#endif
};

static int wm8998_in1mux_ev(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol,
				int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct arizona *arizona = dev_get_drvdata(codec->dev->parent);
	unsigned int left_mux, right_mux, in1mode, old;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		left_mux = snd_soc_read(codec, ARIZONA_ADC_DIGITAL_VOLUME_1L) &
				  ARIZONA_IN1L_SRC_MASK;
		right_mux = snd_soc_read(codec, ARIZONA_ADC_DIGITAL_VOLUME_1R) &
				  ARIZONA_IN1R_SRC_MASK;

		in1mode = (arizona->pdata.inmode[0] & 2)
				<< (ARIZONA_IN1_MODE_SHIFT - 1);

		if (in1mode != 0) {
			/* IN1A is digital, check whether IN1A is selected */

			if (left_mux != right_mux) {
				dev_err(arizona->dev,
					"IN1=DMIC and 'IN1MUXL Input'"
					" != 'IN1MUXR Input'");
				return -EINVAL;
			}

			if (left_mux != 0)
				in1mode = 0; /* IN1B selected, set analogue */
		}

		old = snd_soc_read(codec, ARIZONA_IN1L_CONTROL) &
					ARIZONA_IN1_MODE_MASK;

		if (old != in1mode)
			snd_soc_update_bits(codec, ARIZONA_IN1L_CONTROL,
						ARIZONA_IN1_MODE_MASK, in1mode);
		return 0;

	default:
		return 0;
	}
}

static int wm8998_in2mux_ev(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol,
				int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct arizona *arizona = dev_get_drvdata(codec->dev->parent);
	unsigned int mux, in2mode, old;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		mux = snd_soc_read(codec, ARIZONA_ADC_DIGITAL_VOLUME_2L) &
				  ARIZONA_IN2L_SRC_MASK;

		if (mux == 0)
			in2mode = (arizona->pdata.inmode[1] & 2)
				<< (ARIZONA_IN2_MODE_SHIFT - 1);
		else
			in2mode = 0;	/* IN2B always analogue */

		old = snd_soc_read(codec, ARIZONA_IN2L_CONTROL) &
					ARIZONA_IN2_MODE_MASK;

		if (old != in2mode)
			snd_soc_update_bits(codec, ARIZONA_IN2L_CONTROL,
						ARIZONA_IN2_MODE_MASK, in2mode);
		return 0;

	default:
		return 0;
	}
}

static int wm8998_set_fll(struct snd_soc_codec *codec, int fll_id, int source,
			  unsigned int Fref, unsigned int Fout)
{
	struct wm8998_priv *wm8998 = snd_soc_codec_get_drvdata(codec);

	switch (fll_id) {
	case WM8998_FLL1:
		return arizona_set_fll(&wm8998->fll[0], source, Fref, Fout);
	case WM8998_FLL2:
		return arizona_set_fll(&wm8998->fll[1], source, Fref, Fout);
	case WM8998_FLL1_REFCLK:
		return arizona_set_fll_refclk(&wm8998->fll[0], source, Fref,
					      Fout);
	case WM8998_FLL2_REFCLK:
		return arizona_set_fll_refclk(&wm8998->fll[1], source, Fref,
					      Fout);
	default:
		return -EINVAL;
	}
}

static int wm8998_codec_probe(struct snd_soc_codec *codec)
{
	struct wm8998_priv *priv = snd_soc_codec_get_drvdata(codec);
	int ret;

	codec->control_data = priv->core.arizona->regmap;
	priv->core.arizona->dapm = &codec->dapm;

	ret = snd_soc_codec_set_cache_io(codec, 32, 16, SND_SOC_REGMAP);
	if (ret != 0)
		return ret;

	arizona_init_spk(codec);
	arizona_init_gpio(codec);

	snd_soc_dapm_disable_pin(&codec->dapm, "HAPTICS");

	priv->core.arizona->dapm = &codec->dapm;

	wm8998_create_attrs(priv->core.arizona->dev);

	return 0;
}

static int wm8998_codec_remove(struct snd_soc_codec *codec)
{
	struct wm8998_priv *priv = snd_soc_codec_get_drvdata(codec);

	priv->core.arizona->dapm = NULL;

	wm8998_destroy_atts(priv->core.arizona->dev);

	return 0;
}

#define WM8998_DIG_VU 0x0200

static unsigned int wm8998_digital_vu[] = {
	ARIZONA_DAC_DIGITAL_VOLUME_1L,
	ARIZONA_DAC_DIGITAL_VOLUME_1R,
	ARIZONA_DAC_DIGITAL_VOLUME_2L,
	ARIZONA_DAC_DIGITAL_VOLUME_2R,
	ARIZONA_DAC_DIGITAL_VOLUME_3L,
	ARIZONA_DAC_DIGITAL_VOLUME_4L,
	ARIZONA_DAC_DIGITAL_VOLUME_4R,
	ARIZONA_DAC_DIGITAL_VOLUME_5L,
	ARIZONA_DAC_DIGITAL_VOLUME_5R,
};

static struct snd_soc_codec_driver soc_codec_dev_wm8998 = {
	.probe = wm8998_codec_probe,
	.remove = wm8998_codec_remove,

	.idle_bias_off = true,

	.set_sysclk = arizona_set_sysclk,
	.set_pll = wm8998_set_fll,

	.controls = wm8998_snd_controls,
	.num_controls = ARRAY_SIZE(wm8998_snd_controls),
	.dapm_widgets = wm8998_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(wm8998_dapm_widgets),
	.dapm_routes = wm8998_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(wm8998_dapm_routes),
};

static int wm8998_probe(struct platform_device *pdev)
{
	struct arizona *arizona = dev_get_drvdata(pdev->dev.parent);
	struct wm8998_priv *wm8998;
	int i;

	wm8998 = devm_kzalloc(&pdev->dev, sizeof(struct wm8998_priv),
			      GFP_KERNEL);
	if (!wm8998)
		return -ENOMEM;
	platform_set_drvdata(pdev, wm8998);

	/* Set of_node to parent from the SPI device to allow DAPM to
	 * locate regulator supplies */
	pdev->dev.of_node = arizona->dev->of_node;

	wm8998->core.arizona = arizona;
	wm8998->core.num_inputs = 3;	/* IN1L, IN1R, IN2 */

	for (i = 0; i < ARRAY_SIZE(wm8998->fll); i++)
		wm8998->fll[i].vco_mult = 1;

	arizona->dcvdd_lp_fmax = 24576000;

	arizona_init_fll(arizona, 1, ARIZONA_FLL1_CONTROL_1 - 1,
			 ARIZONA_IRQ_FLL1_LOCK, ARIZONA_IRQ_FLL1_CLOCK_OK,
			 &wm8998->fll[0]);
	arizona_init_fll(arizona, 2, ARIZONA_FLL2_CONTROL_1 - 1,
			 ARIZONA_IRQ_FLL2_LOCK, ARIZONA_IRQ_FLL2_CLOCK_OK,
			 &wm8998->fll[1]);

	for (i = 0; i < ARRAY_SIZE(wm8998_dai); i++)
		arizona_init_dai(&wm8998->core, i);

	/* Latch volume update bits */
	for (i = 0; i < ARRAY_SIZE(wm8998_digital_vu); i++)
		regmap_update_bits(arizona->regmap, wm8998_digital_vu[i],
				   WM8998_DIG_VU, WM8998_DIG_VU);

	pm_runtime_enable(&pdev->dev);
	pm_runtime_idle(&pdev->dev);

	return snd_soc_register_codec(&pdev->dev, &soc_codec_dev_wm8998,
				      wm8998_dai, ARRAY_SIZE(wm8998_dai));
}

static int wm8998_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

	return 0;
}

static struct platform_driver wm8998_codec_driver = {
	.driver = {
		.name = "wm8998-codec",
		.owner = THIS_MODULE,
	},
	.probe = wm8998_probe,
	.remove = wm8998_remove,
};

module_platform_driver(wm8998_codec_driver);

MODULE_DESCRIPTION("ASoC WM8998 driver");
MODULE_AUTHOR("Richard Fitzgerald <rf@opensource.wolfsonmicro.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:wm8998-codec");
