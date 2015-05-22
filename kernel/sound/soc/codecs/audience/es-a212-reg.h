/*
 * es-a212-reg.h  --  Audience eS515 ALSA SoC Audio driver
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

#ifndef __ES_A212_REG_H__
#define __ES_A212_REG_H__


/*
 * Register values
 */

/*
 * Analog
 */

#define ES_CODEC_ENA				0x00
#define ES_OVERRIDE				0x01
#define ES_MICBIAS_CTRL				0x02
#define ES_MICL_GAIN				0x03
#define ES_MICL_GAIN				0x03
#define ES_MIC_CTRL				0x04
#define ES_MICR_GAIN				0x05
#define ES_AUXL_GAIN				0x06
#define ES_AUXL_CTRL				0x07
#define ES_AUXR_GAIN				0x08
#define ES_AUXR_CTRL				0x09
#define ES_ADC_CTRL				0x0A
#define ES_DAC_CTRL				0x0B
#define ES_DAC_DEBUG				0x0C
#define ES_SPARE_1				0x0D
#define ES_SPARE_2				0x0E
#define ES_EP_GAIN				0x0F
#define ES_EP_CTRL				0x10
#define ES_HPL_GAIN				0x11
#define ES_HPL_CTRL				0x12
#define ES_HPR_GAIN				0x13
#define ES_HPR_CTRL				0x14
#define ES_HFL_GAIN				0x15
#define ES_HFL_CTRL				0x16
#define ES_HFR_GAIN				0x17
#define ES_HFR_CTRL				0x18
#define ES_LO_L_GAIN				0x19
#define ES_LO_L_CTRL				0x1A
#define ES_LO_R_GAIN				0x1B
#define ES_LO_R_CTRL				0x1C
#define ES_SPARE_3				0x1D
#define ES_CP_CTRL				0x1E
#define ES_INT1_STATE				0x1F
#define ES_INT1_MASK				0x20
#define ES_INT2_STATE				0x21
#define ES_INT2_MASK				0x22
#define ES_BTN_CTRL1				0x23
#define ES_BTN_CTRL2				0x24
#define ES_BTN_CTRL3				0x25
#define ES_ACCDET_STAT				0x26
#define ES_CLASSG				0x27
#define ES_HF_DEBUG				0x28
#define ES_HF_TRIM				0x29
#define ES_LDO_TRIM				0x2A
#define ES_LDO_CTRL				0x2B
#define ES_ADC_BIAS_1				0x2C
#define ES_ADC_GAIN_DITH			0x2D

/*
 * Digital
 */

#define ES_DAC_DIG_EN				0x40
#define ES_DAC_DIG_CH				0x41
#define ES_DAC_DIG_CPF				0x42
#define ES_DAC_DIG_I2S1				0x43
#define ES_DAC_DIG_I2S2				0x44
#define ES_DAC_DIG_FS_SEL			0x45
#define ES_DAC_DIG_OS				0x46
#define ES_DAC_DIG_CIC				0x47
#define ES_DAC_DIG_SDM				0x48
#define ES_DAC_DIG_DS				0x49
#define ES_DAC_DIG_DRP_FLT_CFF_0		0x4A
#define ES_DAC_DIG_DRP_FLT_CFF_1		0x4B
#define ES_DAC_DIG_DRP_FLT_CFB_0		0x4C
#define ES_DAC_DIG_DRP_FLT_CFB_1		0x4D
#define ES_DAC_DIG_DRP_FLT_GAIN_0		0x4E
#define ES_DAC_DIG_DRP_FLT_GAIN_1		0x4F
#define ES_DAC_DIG_FILT_ER_SAT_FLAG		0x50


#define ES_MAX_REGISTER				0x50



/*
 * Field Definitions.
 */

/*
 * R2 (0x02) - Microphone Bias Control Register
 */

#define ES_MBIAS1_ENA				0x0001  /* MBIAS1_ENA */
#define ES_MBIAS1_ENA_MASK			0x0001  /* MBIAS1_ENA */
#define ES_MBIAS1_ENA_SHIFT			     0  /* MBIAS1_ENA */
#define ES_MBIAS1_ENA_WIDTH			     1  /* MBIAS1_ENA */
#define ES_MBIAS2_ENA				0x0002  /* MBIAS2_ENA */
#define ES_MBIAS2_ENA_MASK			0x0002  /* MBIAS2_ENA */
#define ES_MBIAS2_ENA_SHIFT			     1  /* MBIAS2_ENA */
#define ES_MBIAS2_ENA_WIDTH			     1  /* MBIAS2_ENA */
#define ES_MBIAS1_LVL_MASK			0x0004  /* 0=2.1V, 1=2.5V*/
#define ES_MBIAS1_LVL_SHIFT			     2  /* MBIAS1_LVL */
#define ES_MBIAS1_LVL_WIDTH			     1  /* MBIAS1_LVL */
#define ES_MBIAS2_LVL_MASK			0x0008  /* 0=2.1V, 1=2.5V*/
#define ES_MBIAS2_LVL_SHIFT			     3  /* MBIAS2_LVL */
#define ES_MBIAS2_LVL_WIDTH			     1  /* MBIAS2_LVL */
#define ES_MBIAS1_PDN_MODE_MASK			0x0010  /* 0=O/p Hi-Z, 1=PD*/
#define ES_MBIAS1_PDN_MODE_SHIFT		     4  /* MBIAS1_PDN_MODE */
#define ES_MBIAS1_PDN_MODE_WIDTH		     1  /* MBIAS1_PDN_MODE */
#define ES_MBIAS2_PDN_MODE_MASK			0x0020  /* 0=O/p Hi-Z, 1=PD*/
#define ES_MBIAS2_PDN_MODE_SHIFT		     5  /* MBIAS2_PDN_MODE */
#define ES_MBIAS2_PDN_MODE_WIDTH		     1  /* MBIAS2_PDN_MODE */
#define ES_CLK_DIV_MASK				0x00C0  /* CLK_DIV*/
#define ES_CLK_DIV_SHIFT			     6  /* CLK_DIV */
#define ES_CLK_DIV_WIDTH			     2  /* CLK_DIV */


/*
 * R3 (0x03) - MICL Gain Register
 */

#define ES_MICL_ATTEN				0x0001  /* 1=6dB atten */
#define ES_MICL_ATTEN_MASK			0x0001  /* MICL_ATTEN */
#define ES_MICL_ATTEN_SHIFT			     0  /* MICL_ATTEN */
#define ES_MICL_ATTEN_WIDTH			     1  /* MICL_ATTEN */
#define ES_MICL_GAIN_MASK			0x003E  /* MICL_GAIN */
#define ES_MICL_GAIN_SHIFT			     1  /* MICL_GAIN */
#define ES_MICL_GAIN_WIDTH			     5  /* MICL_GAIN */
#define ES_MICL_GAIN_MAX			0x0014  /* MICL_GAIN */


/*
 * R4 (0x04) - MIC CTRL Register
 */

#define ES_MICL_ON				0x0001  /* 1=MICL Amp ON */
#define ES_MICL_ON_MASK				0x0001  /* MICL_ON */
#define ES_MICL_ON_SHIFT			     0  /* MICL_ON */
#define ES_MICL_ON_WIDTH			     1  /* MICL_ON */
#define ES_MICL_IN_SEL_MASK			0x0006  /* MICL_IN_SEL */
#define ES_MICL_IN_SEL_SHIFT			     1  /* MICL_IN_SEL */
#define ES_MICL_IN_SEL_WIDTH			     2  /* MICL_IN_SEL */
#define ES_MICR_ON				0x0008  /* 1=MICR Amp ON */
#define ES_MICR_ON_MASK				0x0008  /* MICR_ON */
#define ES_MICR_ON_SHIFT			     3  /* MICR_ON */
#define ES_MICR_ON_WIDTH			     1  /* MICR_ON */
#define ES_MICR_IN_SEL_MASK			0x0030  /* MICR_IN_SEL */
#define ES_MICR_IN_SEL_SHIFT			     4  /* MICR_IN_SEL */
#define ES_MICR_IN_SEL_WIDTH			     2  /* MICR_IN_SEL */
#define ES_IN_BIAS				0x0040  /* IN_BIAS */
#define ES_IN_BIAS_MASK				0x0040  /* IN_BIAS */
#define ES_IN_BIAS_SHIFT			     6  /* IN_BIAS */
#define ES_IN_BIAS_WIDTH			     1  /* IN_BIAS */
#define ES_MIC_TRIM_DIS				0x0080  /* MIC_TRIM_DIS */
#define ES_MIC_TRIM_DIS_MASK			0x0080  /* MIC_TRIM_DIS */
#define ES_MIC_TRIM_DIS_SHIFT			     7  /* MIC_TRIM_DIS */
#define ES_MIC_TRIM_DIS_WIDTH			     1  /* MIC_TRIM_DIS */


/*
 * R5 (0x05) - MICR Gain Register
 */

#define ES_MICR_ATTEN				0x0001  /* 1=6dB atten */
#define ES_MICR_ATTEN_MASK			0x0001  /* MICR_ATTEN */
#define ES_MICR_ATTEN_SHIFT			     0  /* MICR_ATTEN */
#define ES_MICR_ATTEN_WIDTH			     1  /* MICR_ATTEN */
#define ES_MICR_GAIN_MASK			0x003E  /* MICR_GAIN */
#define ES_MICR_GAIN_SHIFT			     1  /* MICR_GAIN */
#define ES_MICR_GAIN_WIDTH			     5  /* MICR_GAIN */
#define ES_MICR_GAIN_MAX			0x0014  /* MICR_GAIN */


/*
 * R6 (0x06) - AUXL Gain Register
 */

#define ES_AUX_P_ATTEN_MASK			0x000F  /* AUX_P_ATTEN */
#define ES_AUX_P_ATTEN_SHIFT			     0  /* AUX_P_ATTEN */
#define ES_AUX_P_ATTEN_WIDTH			     4  /* AUX_P_ATTEN */
#define ES_AUX_P_ATTEN_MAX			0x000A  /* AUX_P_ATTEN */
#define ES_AUX_L_GAIN_MASK			0x0070  /* AUX_L_GAIN */
#define ES_AUX_L_GAIN_SHIFT			     4  /* AUX_L_GAIN */
#define ES_AUX_L_GAIN_WIDTH			     3  /* AUX_L_GAIN */
#define ES_AUX_L_GAIN_MAX			0x0004  /* AUX_L_GAIN */


/*
 * R7 (0x07) - AUXL CTRL Register
 */

#define ES_AUXL_ON				0x0001  /* 1=AUXL PGA ON */
#define ES_AUXL_ON_MASK				0x0001  /* AUXL_ON */
#define ES_AUXL_ON_SHIFT			     0  /* AUXL_ON */
#define ES_AUXL_ON_WIDTH			     1  /* AUXL_ON */
#define ES_APGAL_IN_AUX_MASK			0x0006  /* APGAL_IN_AUX */
#define ES_APGAL_IN_AUX_SHIFT			     1  /* APGAL_IN_AUX */
#define ES_APGAL_IN_AUX_WIDTH			     2  /* APGAL_IN_AUX */
#define ES_AUXL_MONO				0x0008  /* 1=Mono 0=Stereo */
#define ES_AUXL_MONO_MASK			0x0008  /* AUXL_MONO */
#define ES_AUXL_MONO_SHIFT			     3  /* AUXL_MONO */
#define ES_AUXL_MONO_WIDTH			     1  /* AUXL_MONO */
#define ES_AUX_TRIM_DIS				0x0080  /* AUX_TRIM_DIS */
#define ES_AUX_TRIM_DIS_MASK			0x0080  /* AUX_TRIM_DIS */
#define ES_AUX_TRIM_DIS_SHIFT			     7  /* AUX_TRIM_DIS */
#define ES_AUX_TRIM_DIS_WIDTH			     1  /* AUX_TRIM_DIS */


/*
 * R8 (0x08) - AUXR Gain Register
 */

#define ES_AUX_M_ATTEN_MASK			0x000F  /* AUX_P_ATTEN */
#define ES_AUX_M_ATTEN_SHIFT			     0  /* AUX_P_ATTEN */
#define ES_AUX_M_ATTEN_WIDTH			     4  /* AUX_P_ATTEN */
#define ES_AUX_M_ATTEN_MAX			0x000A  /* AUX_P_ATTEN */
#define ES_AUX_R_GAIN_MASK			0x0070  /* AUX_R_GAIN */
#define ES_AUX_R_GAIN_SHIFT			     4  /* AUX_R_GAIN */
#define ES_AUX_R_GAIN_WIDTH			     3  /* AUX_R_GAIN */
#define ES_AUX_R_GAIN_MAX			0x0004  /* AUX_R_GAIN */


/*
 * R9 (0x09) - AUXR CTRL Register
 */

#define ES_AUXR_ON				0x0001  /* 1=AUXR PGA ON */
#define ES_AUXR_ON_MASK				0x0001  /* AUXR_ON */
#define ES_AUXR_ON_SHIFT			     0  /* AUXR_ON */
#define ES_AUXR_ON_WIDTH			     1  /* AUXR_ON */
#define ES_APGAR_IN_AUX_MASK			0x0006  /* APGAR_IN_AUX */
#define ES_APGAR_IN_AUX_SHIFT			     1  /* APGAR_IN_AUX */
#define ES_APGAR_IN_AUX_WIDTH			     2  /* APGAR_IN_AUX */
#define ES_AUXR_MONO				0x0008  /* 1=Mono 0=Stereo */
#define ES_AUXR_MONO_MASK			0x0008  /* AUXR_MONO */
#define ES_AUXR_MONO_SHIFT			     3  /* AUXR_MONO */
#define ES_AUXR_MONO_WIDTH			     1  /* AUXR_MONO */


/*
 * R10 (0x0A) - ADC CTRL Register
 */

#define ES_ADC1_ON				0x0001  /* ADC1_ON */
#define ES_ADC1_ON_MASK				0x0001  /* ADC1_ON */
#define ES_ADC1_ON_SHIFT			     0  /* ADC1_ON */
#define ES_ADC1_ON_WIDTH			     1  /* ADC1_ON */
#define ES_ADC2_ON				0x0002  /* ADC2_ON */
#define ES_ADC2_ON_MASK				0x0002  /* ADC2_ON */
#define ES_ADC2_ON_SHIFT			     1  /* ADC2_ON */
#define ES_ADC2_ON_WIDTH			     1  /* ADC2_ON */
#define ES_ADC3_ON				0x0004  /* ADC3_ON */
#define ES_ADC3_ON_MASK				0x0004  /* ADC3_ON */
#define ES_ADC3_ON_SHIFT			     2  /* ADC3_ON */
#define ES_ADC3_ON_WIDTH			     1  /* ADC3_ON */
#define ES_ADC1_IN_SEL_MASK			0x0008  /* ADC1_IN_SEL */
#define ES_ADC1_IN_SEL_SHIFT			     3  /* ADC1_IN_SEL */
#define ES_ADC1_IN_SEL_WIDTH			     1  /* ADC1_IN_SEL */
#define ES_ADC2_IN_SEL_MASK			0x0010  /* ADC2_IN_SEL */
#define ES_ADC2_IN_SEL_SHIFT			     4  /* ADC2_IN_SEL */
#define ES_ADC2_IN_SEL_WIDTH			     1  /* ADC2_IN_SEL */
#define ES_ADC3_IN_SEL_MASK			0x0020  /* ADC3_IN_SEL */
#define ES_ADC3_IN_SEL_SHIFT			     5  /* ADC3_IN_SEL */
#define ES_ADC3_IN_SEL_WIDTH			     1  /* ADC3_IN_SEL */
#define ES_ADC_MUTE				0x0040  /* ADC_MUTE */
#define ES_ADC_MUTE_MASK			0x0040  /* ADC_MUTE */
#define ES_ADC_MUTE_SHIFT			     6  /* ADC_MUTE */
#define ES_ADC_MUTE_WIDTH			     1  /* ADC_MUTE */
#define ES_ADC_GAIN_H_MASK			0x0080  /* ADC_GAIN_H */
#define ES_ADC_GAIN_H_SHIFT			     7  /* ADC_GAIN_H */
#define ES_ADC_GAIN_H_WIDTH			     1  /* ADC_GAIN_H */


/*
 * R11 (0x0B) - DAC CTRL Register
 */

#define ES_DAC1L_ON				0x0001  /* DAC1L_ON */
#define ES_DAC1L_ON_MASK			0x0001  /* DAC1L_ON */
#define ES_DAC1L_ON_SHIFT			     0  /* DAC1L_ON */
#define ES_DAC1L_ON_WIDTH			     1  /* DAC1L_ON */
#define ES_DAC1R_ON				0x0002  /* DAC1R_ON */
#define ES_DAC1R_ON_MASK			0x0002  /* DAC1R_ON */
#define ES_DAC1R_ON_SHIFT			     1  /* DAC1R_ON */
#define ES_DAC1R_ON_WIDTH			     1  /* DAC1R_ON */
#define ES_DAC2L_ON				0x0004  /* DAC2L_ON */
#define ES_DAC2L_ON_MASK			0x0004  /* DAC2L_ON */
#define ES_DAC2L_ON_SHIFT			     2  /* DAC2L_ON */
#define ES_DAC2L_ON_WIDTH			     1  /* DAC2L_ON */
#define ES_DAC2R_ON				0x0008  /* DAC2R_ON */
#define ES_DAC2R_ON_MASK			0x0008  /* DAC2R_ON */
#define ES_DAC2R_ON_SHIFT			     3  /* DAC2R_ON */
#define ES_DAC2R_ON_WIDTH			     1  /* DAC2R_ON */
#define ES_DAC_TRIM_DIS_MASK			0x0080  /* DAC_TRIM_DIS */
#define ES_DAC_TRIM_DIS_SHIFT			     7  /* DAC_TRIM_DIS */
#define ES_DAC_TRIM_DIS_WIDTH			     1  /* DAC_TRIM_DIS */



/*
 * R15 (0x0F) - Earphone Gain Register
 */

#define ES_EP_GAIN_MASK				0x000F  /* EP_GAIN */
#define ES_EP_GAIN_SHIFT			     0  /* EP_GAIN */
#define ES_EP_GAIN_WIDTH			     4  /* EP_GAIN */
#define ES_EP_GAIN_MAX				0x000F  /* EP_GAIN */
#define ES_EP_MUTE				0x0080  /* EP_MUTE */
#define ES_EP_MUTE_MASK				0x0080  /* EP_MUTE */
#define ES_EP_MUTE_SHIFT			     7  /* EP_MUTE */
#define ES_EP_MUTE_WIDTH			     1  /* EP_MUTE */


/*
 * R16 (0x10) - Earphone Ctrl Register
 */

#define ES_EP_ON				0x0001  /* EP_ON */
#define ES_EP_ON_MASK				0x0001  /* EP_ON */
#define ES_EP_ON_SHIFT				     0  /* EP_ON */
#define ES_EP_ON_WIDTH				     1  /* EP_ON */
#define ES_DAC1L_TO_EP				0x0002  /* DAC1L_TO_EP */
#define ES_DAC1L_TO_EP_MASK			0x0002  /* DAC1L_TO_EP */
#define ES_DAC1L_TO_EP_SHIFT			     1  /* DAC1L_TO_EP */
#define ES_DAC1L_TO_EP_WIDTH			     1  /* DAC1L_TO_EP */
#define ES_AUXL_TO_EP				0x0004  /* AUXL_TO_EP */
#define ES_AUXL_TO_EP_MASK			0x0004  /* AUXL_TO_EP */
#define ES_AUXL_TO_EP_SHIFT			     2  /* AUXL_TO_EP */
#define ES_AUXL_TO_EP_WIDTH			     1  /* AUXL_TO_EP */
#define ES_AUXR_TO_EP				0x0008  /* AUXR_TO_EP */
#define ES_AUXR_TO_EP_MASK			0x0008  /* AUXR_TO_EP */
#define ES_AUXR_TO_EP_SHIFT			     3  /* AUXR_TO_EP */
#define ES_AUXR_TO_EP_WIDTH			     1  /* AUXR_TO_EP */
#define ES_MICL_TO_EP				0x0010  /* MICL_TO_EP */
#define ES_MICL_TO_EP_MASK			0x0010  /* MICL_TO_EP */
#define ES_MICL_TO_EP_SHIFT			     4  /* MICL_TO_EP */
#define ES_MICL_TO_EP_WIDTH			     1  /* MICL_TO_EP */
#define ES_MICR_TO_EP				0x0020  /* MICR_TO_EP */
#define ES_MICR_TO_EP_MASK			0x0020  /* MICR_TO_EP */
#define ES_MICR_TO_EP_SHIFT			     5  /* MICR_TO_EP */
#define ES_MICR_TO_EP_WIDTH			     1  /* MICR_TO_EP */


/*
 * R17 (0x11) - Headphone Left Gain Register
 */

#define ES_HPL_GAIN_MASK			0x000F  /* HPL_GAIN */
#define ES_HPL_GAIN_SHIFT			     0  /* HPL_GAIN */
#define ES_HPL_GAIN_WIDTH			     4  /* HPL_GAIN */
#define ES_HPL_GAIN_MAX				0x000F  /* HPL_GAIN */
#define ES_HPL_MUTE				0x0080  /* HPL_MUTE */
#define ES_HPL_MUTE_MASK			0x0080  /* HPL_MUTE */
#define ES_HPL_MUTE_SHIFT			     7  /* HPL_MUTE */
#define ES_HPL_MUTE_WIDTH			     1  /* HPL_MUTE */


/*
 * R18 (0x12) - Headphone Left Ctrl Register
 */

#define ES_HPL_ON				0x0001  /* HPL_ON */
#define ES_HPL_ON_MASK				0x0001  /* HPL_ON */
#define ES_HPL_ON_SHIFT				     0  /* HPL_ON */
#define ES_HPL_ON_WIDTH				     1  /* HPL_ON */
#define ES_DAC1L_TO_HPL				0x0002  /* DAC1L_TO_HPL */
#define ES_DAC1L_TO_HPL_MASK			0x0002  /* DAC1L_TO_HPL */
#define ES_DAC1L_TO_HPL_SHIFT			     1  /* DAC1L_TO_HPL */
#define ES_DAC1L_TO_HPL_WIDTH			     1  /* DAC1L_TO_HPL */
#define ES_DAC2L_TO_HPL				0x0004  /* DAC2L_TO_HPL */
#define ES_DAC2L_TO_HPL_MASK			0x0004  /* DAC2L_TO_HPL */
#define ES_DAC2L_TO_HPL_SHIFT			     2  /* DAC2L_TO_HPL */
#define ES_DAC2L_TO_HPL_WIDTH			     1  /* DAC2L_TO_HPL */
#define ES_AUXL_TO_HPL				0x0008  /* AUXL_TO_HPL */
#define ES_AUXL_TO_HPL_MASK			0x0008  /* AUXL_TO_HPL */
#define ES_AUXL_TO_HPL_SHIFT			     3  /* AUXL_TO_HPL */
#define ES_AUXL_TO_HPL_WIDTH			     1  /* AUXL_TO_HPL */
#define ES_MICL_TO_HPL				0x0010  /* MICL_TO_HPL */
#define ES_MICL_TO_HPL_MASK			0x0010  /* MICL_TO_HPL */
#define ES_MICL_TO_HPL_SHIFT			     4  /* MICL_TO_HPL */
#define ES_MICL_TO_HPL_WIDTH			     1  /* MICL_TO_HPL */


/*
 * R19 (0x13) - Headphone Right Gain Register
 */

#define ES_HPR_GAIN_MASK			0x000F  /* HPR_GAIN */
#define ES_HPR_GAIN_SHIFT			     0  /* HPR_GAIN */
#define ES_HPR_GAIN_WIDTH			     4  /* HPR_GAIN */
#define ES_HPR_GAIN_MAX				0x000F  /* HPR_GAIN */
#define ES_HPR_MUTE				0x0080  /* HPR_MUTE */
#define ES_HPR_MUTE_MASK			0x0080  /* HPR_MUTE */
#define ES_HPR_MUTE_SHIFT			     7  /* HPR_MUTE */
#define ES_HPR_MUTE_WIDTH			     1  /* HPR_MUTE */


/*
 * R20 (0x14) - Headphone Right Ctrl Register
 */

#define ES_HPR_ON				0x0001  /* HPR_ON */
#define ES_HPR_ON_MASK				0x0001  /* HPR_ON */
#define ES_HPR_ON_SHIFT				     0  /* HPR_ON */
#define ES_HPR_ON_WIDTH				     1  /* HPR_ON */
#define ES_DAC1R_TO_HPR				0x0002  /* DAC1R_TO_HPR */
#define ES_DAC1R_TO_HPR_MASK			0x0002  /* DAC1R_TO_HPR */
#define ES_DAC1R_TO_HPR_SHIFT			     1  /* DAC1R_TO_HPR */
#define ES_DAC1R_TO_HPR_WIDTH			     1  /* DAC1R_TO_HPR */
#define ES_DAC2R_TO_HPR				0x0004  /* DAC2R_TO_HPR */
#define ES_DAC2R_TO_HPR_MASK			0x0004  /* DAC2R_TO_HPR */
#define ES_DAC2R_TO_HPR_SHIFT			     2  /* DAC2R_TO_HPR */
#define ES_DAC2R_TO_HPR_WIDTH			     1  /* DAC2R_TO_HPR */
#define ES_AUXR_TO_HPR				0x0008  /* AUXR_TO_HPR */
#define ES_AUXR_TO_HPR_MASK			0x0008  /* AUXR_TO_HPR */
#define ES_AUXR_TO_HPR_SHIFT			     3  /* AUXR_TO_HPR */
#define ES_AUXR_TO_HPR_WIDTH			     1  /* AUXR_TO_HPR */
#define ES_MICR_TO_HPR				0x0010  /* MICR_TO_HPR */
#define ES_MICR_TO_HPR_MASK			0x0010  /* MICR_TO_HPR */
#define ES_MICR_TO_HPR_SHIFT			     4  /* MICR_TO_HPR */
#define ES_MICR_TO_HPR_WIDTH			     1  /* MICR_TO_HPR */


/*
 * R21 (0x15) - Speaker Left Gain Register
 */

#define ES_HFL_GAIN_MASK			0x001F  /* HFL_GAIN */
#define ES_HFL_GAIN_SHIFT			     0  /* HFL_GAIN */
#define ES_HFL_GAIN_WIDTH			     5  /* HFL_GAIN */
#define ES_HFL_GAIN_MAX				0x001F  /* HFL_GAIN */
#define ES_HFL_MUTE				0x0080  /* HFL_MUTE */
#define ES_HFL_MUTE_MASK			0x0080  /* HFL_MUTE */
#define ES_HFL_MUTE_SHIFT			     7  /* HFL_MUTE */
#define ES_HFL_MUTE_WIDTH			     1  /* HFL_MUTE */


/*
 * R22 (0x16) - Speaker Left Ctrl Register
 */

#define ES_HFL_ON				0x0001  /* HFL_ON */
#define ES_HFL_ON_MASK				0x0001  /* HFL_ON */
#define ES_HFL_ON_SHIFT				     0  /* HFL_ON */
#define ES_HFL_ON_WIDTH				     1  /* HFL_ON */
#define ES_DAC1L_TO_HFL				0x0002  /* DAC1L_TO_HFL */
#define ES_DAC1L_TO_HFL_MASK			0x0002  /* DAC1L_TO_HFL */
#define ES_DAC1L_TO_HFL_SHIFT			     1  /* DAC1L_TO_HFL */
#define ES_DAC1L_TO_HFL_WIDTH			     1  /* DAC1L_TO_HFL */
#define ES_DAC2L_TO_HFL				0x0004  /* DAC2L_TO_HFL */
#define ES_DAC2L_TO_HFL_MASK			0x0004  /* DAC2L_TO_HFL */
#define ES_DAC2L_TO_HFL_SHIFT			     2  /* DAC2L_TO_HFL */
#define ES_DAC2L_TO_HFL_WIDTH			     1  /* DAC2L_TO_HFL */
#define ES_AUXL_TO_HFL				0x0008  /* AUXL_TO_HFL */
#define ES_AUXL_TO_HFL_MASK			0x0008  /* AUXL_TO_HFL */
#define ES_AUXL_TO_HFL_SHIFT			     3  /* AUXL_TO_HFL */
#define ES_AUXL_TO_HFL_WIDTH			     1  /* AUXL_TO_HFL */
#define ES_MICL_TO_HFL				0x0010  /* MICL_TO_HFL */
#define ES_MICL_TO_HFL_MASK			0x0010  /* MICL_TO_HFL */
#define ES_MICL_TO_HFL_SHIFT			     4  /* MICL_TO_HFL */
#define ES_MICL_TO_HFL_WIDTH			     1  /* MICL_TO_HFL */
#define ES_ADIR_TO_HFL				0x0020  /* ADIR_TO_HFL */
#define ES_ADIR_TO_HFL_MASK			0x0020  /* ADIR_TO_HFL */
#define ES_ADIR_TO_HFL_SHIFT			     5  /* ADIR_TO_HFL */
#define ES_ADIR_TO_HFL_WIDTH			     1  /* ADIR_TO_HFL */


/*
 * R23 (0x17) - Speaker Right Gain Register
 */

#define ES_HFR_GAIN_MASK			0x001F  /* HFR_GAIN */
#define ES_HFR_GAIN_SHIFT			     0  /* HFR_GAIN */
#define ES_HFR_GAIN_WIDTH			     5  /* HFR_GAIN */
#define ES_HFR_GAIN_MAX				0x001F  /* HFR_GAIN */
#define ES_HFR_MUTE				0x0080  /* HFR_MUTE */
#define ES_HFR_MUTE_MASK			0x0080  /* HFR_MUTE */
#define ES_HFR_MUTE_SHIFT			     7  /* HFR_MUTE */
#define ES_HFR_MUTE_WIDTH			     1  /* HFR_MUTE */


/*
 * R24 (0x18) - Speaker Right Ctrl Register
 */

#define ES_HFR_ON				0x0001  /* HFR_ON */
#define ES_HFR_ON_MASK				0x0001  /* HFR_ON */
#define ES_HFR_ON_SHIFT				     0  /* HFR_ON */
#define ES_HFR_ON_WIDTH				     1  /* HFR_ON */
#define ES_DAC1R_TO_HFR				0x0002  /* DAC1R_TO_HFR */
#define ES_DAC1R_TO_HFR_MASK			0x0002  /* DAC1R_TO_HFR */
#define ES_DAC1R_TO_HFR_SHIFT			     1  /* DAC1R_TO_HFR */
#define ES_DAC1R_TO_HFR_WIDTH			     1  /* DAC1R_TO_HFR */
#define ES_DAC2R_TO_HFR				0x0004  /* DAC2R_TO_HFR */
#define ES_DAC2R_TO_HFR_MASK			0x0004  /* DAC2R_TO_HFR */
#define ES_DAC2R_TO_HFR_SHIFT			     2  /* DAC2R_TO_HFR */
#define ES_DAC2R_TO_HFR_WIDTH			     1  /* DAC2R_TO_HFR */
#define ES_AUXR_TO_HFR				0x0008  /* AUXR_TO_HFR */
#define ES_AUXR_TO_HFR_MASK			0x0008  /* AUXR_TO_HFR */
#define ES_AUXR_TO_HFR_SHIFT			     3  /* AUXR_TO_HFR */
#define ES_AUXR_TO_HFR_WIDTH			     1  /* AUXR_TO_HFR */
#define ES_MICR_TO_HFR				0x0010  /* MICR_TO_HFR */
#define ES_MICR_TO_HFR_MASK			0x0010  /* MICR_TO_HFR */
#define ES_MICR_TO_HFR_SHIFT			     4  /* MICR_TO_HFR */
#define ES_MICR_TO_HFR_WIDTH			     1  /* MICR_TO_HFR */
#define ES_ADIR_TO_HFR				0x0020  /* ADIR_TO_HFR */
#define ES_ADIR_TO_HFR_MASK			0x0020  /* ADIR_TO_HFR */
#define ES_ADIR_TO_HFR_SHIFT			     5  /* ADIR_TO_HFR */
#define ES_ADIR_TO_HFR_WIDTH			     1  /* ADIR_TO_HFR */


/*
 * R25 (0x19) - Line Output Left Gain Register
 */

#define ES_LO_L_GAIN_MASK			0x000F  /* LO_L_GAIN */
#define ES_LO_L_GAIN_SHIFT			     0  /* LO_L_GAIN */
#define ES_LO_L_GAIN_WIDTH			     4  /* LO_L_GAIN */
#define ES_LO_L_GAIN_MAX			0x000F  /* LO_L_GAIN */
#define ES_LO_L_MUTE				0x0080  /* LO_L_MUTE */
#define ES_LO_L_MUTE_MASK			0x0080  /* LO_L_MUTE */
#define ES_LO_L_MUTE_SHIFT			     7  /* LO_L_MUTE */
#define ES_LO_L_MUTE_WIDTH			     1  /* LO_L_MUTE */

/*
 * R26 (0x1A) - Line Output Left Ctrl Register
 */

#define ES_LO_L_ON				0x0001  /* LO_L_ON */
#define ES_LO_L_ON_MASK				0x0001  /* LO_L_ON */
#define ES_LO_L_ON_SHIFT			     0  /* LO_L_ON */
#define ES_LO_L_ON_WIDTH			     1  /* LO_L_ON */
#define ES_DAC1L_TO_LO_L			0x0002  /* DAC1L_TO_LO_L */
#define ES_DAC1L_TO_LO_L_MASK			0x0002  /* DAC1L_TO_LO_L */
#define ES_DAC1L_TO_LO_L_SHIFT			     1  /* DAC1L_TO_LO_L */
#define ES_DAC1L_TO_LO_L_WIDTH			     1  /* DAC1L_TO_LO_L */
#define ES_DAC2L_TO_LO_L			0x0004  /* DAC2L_TO_LO_L */
#define ES_DAC2L_TO_LO_L_MASK			0x0004  /* DAC2L_TO_LO_L */
#define ES_DAC2L_TO_LO_L_SHIFT			     2  /* DAC2L_TO_LO_L */
#define ES_DAC2L_TO_LO_L_WIDTH			     1  /* DAC2L_TO_LO_L */
#define ES_AUXL_TO_LO_L				0x0008  /* AUXL_TO_LO_L */
#define ES_AUXL_TO_LO_L_MASK			0x0008  /* AUXL_TO_LO_L */
#define ES_AUXL_TO_LO_L_SHIFT			     3  /* AUXL_TO_LO_L */
#define ES_AUXL_TO_LO_L_WIDTH			     1  /* AUXL_TO_LO_L */
#define ES_MICL_TO_LO_L				0x0010  /* MICL_TO_LO_L */
#define ES_MICL_TO_LO_L_MASK			0x0010  /* MICL_TO_LO_L */
#define ES_MICL_TO_LO_L_SHIFT			     4  /* MICL_TO_LO_L */
#define ES_MICL_TO_LO_L_WIDTH			     1  /* MICL_TO_LO_L */


/*
 * R27 (0x1B) - Line Output Right Gain Register
 */

#define ES_LO_R_GAIN_MASK			0x000F  /* LO_R_GAIN */
#define ES_LO_R_GAIN_SHIFT			     0  /* LO_R_GAIN */
#define ES_LO_R_GAIN_WIDTH			     4  /* LO_R_GAIN */
#define ES_LO_R_GAIN_MAX			0x000F  /* LO_R_GAIN */
#define ES_LO_R_MUTE				0x0080  /* LO_R_MUTE */
#define ES_LO_R_MUTE_MASK			0x0080  /* LO_R_MUTE */
#define ES_LO_R_MUTE_SHIFT			     7  /* LO_R_MUTE */
#define ES_LO_R_MUTE_WIDTH			     1  /* LO_R_MUTE */


/*
 * R24 (0x1C) - Line Output Right Ctrl Register
 */

#define ES_LO_R_ON				0x0001  /* LO_R_ON */
#define ES_LO_R_ON_MASK				0x0001  /* LO_R_ON */
#define ES_LO_R_ON_SHIFT			     0  /* LO_R_ON */
#define ES_LO_R_ON_WIDTH			     1  /* LO_R_ON */
#define ES_DAC1R_TO_LO_R			0x0002  /* DAC1R_TO_LO_R */
#define ES_DAC1R_TO_LO_R_MASK			0x0002  /* DAC1R_TO_LO_R */
#define ES_DAC1R_TO_LO_R_SHIFT			     1  /* DAC1R_TO_LO_R */
#define ES_DAC1R_TO_LO_R_WIDTH			     1  /* DAC1R_TO_LO_R */
#define ES_DAC2R_TO_LO_R			0x0004  /* DAC2R_TO_LO_R */
#define ES_DAC2R_TO_LO_R_MASK			0x0004  /* DAC2R_TO_LO_R */
#define ES_DAC2R_TO_LO_R_SHIFT			     2  /* DAC2R_TO_LO_R */
#define ES_DAC2R_TO_LO_R_WIDTH			     1  /* DAC2R_TO_LO_R */
#define ES_AUXR_TO_LO_R				0x0008  /* AUXR_TO_LO_R */
#define ES_AUXR_TO_LO_R_MASK			0x0008  /* AUXR_TO_LO_R */
#define ES_AUXR_TO_LO_R_SHIFT			     3  /* AUXR_TO_LO_R */
#define ES_AUXR_TO_LO_R_WIDTH			     1  /* AUXR_TO_LO_R */
#define ES_MICR_TO_LO_R				0x0010  /* MICR_TO_LO_R */
#define ES_MICR_TO_LO_R_MASK			0x0010  /* MICR_TO_LO_R */
#define ES_MICR_TO_LO_R_SHIFT			     4  /* MICR_TO_LO_R */
#define ES_MICR_TO_LO_R_WIDTH			     1  /* MICR_TO_LO_R */


/*
 * R60 (0x40) - DAC Digital Enable Register
 */

#define ES_DAC1_LEFT_EN				0x0001  /* DAC1_LEFT_EN */
#define ES_DAC1_LEFT_EN_MASK			0x0001  /* DAC1_LEFT_EN */
#define ES_DAC1_LEFT_EN_SHIFT			     0  /* DAC1_LEFT_EN */
#define ES_DAC1_LEFT_EN_WIDTH			     1  /* DAC1_LEFT_EN */
#define ES_DAC1_RIGHT_EN			0x0002  /* DAC1_RIGHT_EN */
#define ES_DAC1_RIGHT_EN_MASK			0x0002  /* DAC1_RIGHT_EN */
#define ES_DAC1_RIGHT_EN_SHIFT			     1  /* DAC1_RIGHT_EN */
#define ES_DAC1_RIGHT_EN_WIDTH			     1  /* DAC1_RIGHT_EN */
#define ES_DAC2_LEFT_EN				0x0004  /* DAC2_LEFT_EN */
#define ES_DAC2_LEFT_EN_MASK			0x0004  /* DAC2_LEFT_EN */
#define ES_DAC2_LEFT_EN_SHIFT			     2  /* DAC2_LEFT_EN */
#define ES_DAC2_LEFT_EN_WIDTH			     1  /* DAC2_LEFT_EN */
#define ES_DAC2_RIGHT_EN			0x0008  /* DAC2_RIGHT_EN */
#define ES_DAC2_RIGHT_EN_MASK			0x0008  /* DAC2_RIGHT_EN */
#define ES_DAC2_RIGHT_EN_SHIFT			     3  /* DAC2_RIGHT_EN */
#define ES_DAC2_RIGHT_EN_WIDTH			     1  /* DAC2_RIGHT_EN */
#define ES_DAC_CLK_EN				0x0010  /* DAC_CLK_EN */
#define ES_DAC_CLK_EN_MASK			0x0010  /* DAC_CLK_EN */
#define ES_DAC_CLK_EN_SHIFT			     4  /* DAC_CLK_EN */
#define ES_DAC_CLK_EN_WIDTH			     1  /* DAC_CLK_EN */
#define ES_DIG_CLK_EN				0x0020  /* DIG_CLK_EN */
#define ES_DIG_CLK_EN_MASK			0x0020  /* DIG_CLK_EN */
#define ES_DIG_CLK_EN_SHIFT			     5  /* DIG_CLK_EN */
#define ES_DIG_CLK_EN_WIDTH			     1  /* DIG_CLK_EN */
#define ES_I2S_MODE				0x0040  /* 1=master 0=slave */
#define ES_I2S_MODE_MASK			0x0040  /* I2S_MODE */
#define ES_I2S_MODE_SHIFT			     6  /* I2S_MODE */
#define ES_I2S_MODE_WIDTH			     1  /* I2S_MODE */
#define ES_I2S_LEFT_JUSTIFIED			0x0080  /* 1=Left justified */
#define ES_I2S_LEFT_JUSTIFIED_MASK		0x0080  /* I2S_LEFT_JUSTIFIED */
#define ES_I2S_LEFT_JUSTIFIED_SHIFT		     7  /* I2S_LEFT_JUSTIFIED */
#define ES_I2S_LEFT_JUSTIFIED_WIDTH		     1  /* I2S_LEFT_JUSTIFIED */


/*
 * R61 (0x41) - DAC Digital Channel Register
 */

#define ES_I2S_CH				0x0001  /* 1=4 chn 0=2 chn */
#define ES_I2S_CH_MASK				0x0001  /* I2S_CH */
#define ES_I2S_CH_SHIFT				     0  /* I2S_CH */
#define ES_I2S_CH_WIDTH				     1  /* I2S_CH */
#define ES_I2S_PIN_EN				0x0002  /* 1=I2s clock 0=mclk */
#define ES_I2S_PIN_EN_MASK			0x0002  /* I2S_PIN_EN */
#define ES_I2S_PIN_EN_SHIFT			     1  /* I2S_PIN_EN */
#define ES_I2S_PIN_EN_WIDTH			     1  /* I2S_PIN_EN */
#define ES_I2S_CLK_SEL				0x0004  /* 1=mclk/2 0=mclk */
#define ES_I2S_CLK_SEL_MASK			0x0004  /* I2S_CLK_SEL */
#define ES_I2S_CLK_SEL_SHIFT			     2  /* I2S_CLK_SEL */
#define ES_I2S_CLK_SEL_WIDTH			     1  /* I2S_CLK_SEL */
#define ES_I2S_CLK_SEL_CLKDIV2			0x0008  /* 1=clk/2 0=clk */
#define ES_I2S_CLK_SEL_CLKDIV2_MASK		0x0008  /* I2S_CLK_SEL_CLKDIV2*/
#define ES_I2S_CLK_SEL_CLKDIV2_SHIFT		     3  /* I2S_CLK_SEL_CLKDIV2*/
#define ES_I2S_CLK_SEL_CLKDIV2_WIDTH		     1  /* I2S_CLK_SEL_CLKDIV2*/
#define ES_CLK_SOURCE				0x0010  /* 1=mclk 0=i2s_clk_in*/
#define ES_CLK_SOURCE_MASK			0x0010  /* CLK_SOURCE */
#define ES_CLK_SOURCE_SHIFT			     4  /* CLK_SOURCE */
#define ES_CLK_SOURCE_WIDTH			     1  /* CLK_SOURCE */


int a212_probe(struct snd_soc_codec *codec);
int es_a212_add_snd_soc_controls(struct snd_soc_codec *codec);
int es_a212_add_snd_soc_dapm_controls(struct snd_soc_codec *codec);
int es_a212_add_snd_soc_route_map(struct snd_soc_codec *codec);


#endif

