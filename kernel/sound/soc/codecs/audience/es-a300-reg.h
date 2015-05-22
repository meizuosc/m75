/*
 * es-a300-reg.h  --  Audience eS755 ALSA SoC Audio driver
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

#ifndef __ES_A300_REG_H__
#define __ES_A300_REG_H__


/*
 * Register values
 */

/*
 * Power
 */

#define ES_CHIP_CTRL				0x00
#define ES_OVERRIDE				0x01
#define ES_LDO_CTRL				0x2B
#define ES_LDO_ILIM				0x36
#define ES_LDO_TRIM				0x2A
#define ES_MICBIAS_CTRL				0x02
#define ES_MB_TRIM1				0x34
#define ES_MB_TRIM2				0x35

/*
 * Accessory Detection
 */

#define ES_PLUGDET_CTRL				0x33
#define ES_BTN_CTRL1				0x23
#define ES_BTN_CTRL2				0x24
#define ES_BTN_CTRL3				0x25
#define ES_BTN_CTRL4				0x32
#define ES_ACC_INTERCEPT			0x0E
#define ES_PLUG_STAT				0x3B
#define ES_BTNDET_STAT				0x26
#define ES_INT1_STATE				0x1F
#define ES_INT1_MASK				0x20
#define ES_INT2_STATE				0x21
#define ES_INT2_MASK				0x22

/*
 * Analog Inputs
 */

#define ES_MIC0_CTRL				0x03
#define ES_MIC1_CTRL				0x05
#define ES_MIC2_CTRL				0x08
#define ES_MICHS_CTRL				0x0D
#define ES_AUX_L_CTRL				0x07
#define ES_AUX_R_CTRL				0x09
#define ES_MIC_TUNE				0x04
#define ES_REC_TRIM				0x06

/*
 * ADC
 */

#define ES_ADC_CTRL				0x0A
#define ES_VS_ADC_CTRL				0x2E
#define ES_ADC_BIAS_I				0x2C
#define ES_ADC_GAIN_DITH			0x2D

/*
 * Analog Ground Referenced Output
 */

#define ES_HP_L_GAIN				0x11
#define ES_HP_L_CTRL				0x12
#define ES_HP_R_GAIN				0x13
#define ES_HP_R_CTRL				0x14
#define ES_HP_CHAIN_TRIM			0x1D
#define ES_EP_GAIN				0x0F
#define ES_EP_CTRL				0x10
#define ES_LO_L_GAIN				0x19
#define ES_LO_L_CTRL				0x1A
#define ES_LO_R_GAIN				0x1B
#define ES_LO_R_CTRL				0x1C
#define ES_CLASSG				0x27
#define ES_CP_CTRL				0x1E

/*
 * Loudspeaker Output
 */

#define ES_SPKR_L_GAIN				0x15
#define ES_SPKR_L_CTRL				0x16
#define ES_SPKR_R_GAIN				0x17
#define ES_SPKR_R_CTRL				0x18
#define ES_SPKR_DEBUG				0x28
#define ES_SPKR_TRIM				0x29
#define ES_SPKR_ALC1				0x30
#define ES_SPKR_ALC2				0x31

/*
 * DAC
 */

#define ES_DAC_CTRL				0x0B
#define ES_DAC_DEBUG				0x0C


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
#define ES_SQUELCH_CONTROL			0x51
#define ES_SQUELCH_THRESHOLD			0x52
#define ES_SQUELCH_TERM_CNT			0x53
#define ES_DEM_TERM_CNT				0x54
#define ES_DERM_ROT_LVL				0x55

#define ES_MAX_REGISTER				0x55



/*
 * Field Definitions.
 */

/*
 * R0 (0x00) - Chip Control Register
 */

#define ES_CHIP_EN_MASK			0x01  /* CHIP_EN 1=Enable chip */
#define ES_CHIP_EN_SHIFT			   0  /* CHIP_EN */
#define ES_CHIP_EN_WIDTH			   1  /* CHIP_EN */
#define ES_STANDBY_MASK				0x02  /* STANDBY 1=Standby*/
#define ES_STANDBY_SHIFT			   1  /* STANDBY */
#define ES_STANDBY_WIDTH			   1  /* STANDBY */
#define ES_LDO1_DIS_MASK			0x04  /* LDO1_DIS 1=LDO1 OFF*/
#define ES_LDO1_DIS_SHIFT			   2  /* LDO1_DIS */
#define ES_LDO1_DIS_WIDTH			   1  /* LDO1_DIS */
#define ES_LDO3_DIS_MASK			0x08  /* LDO3_DIS 1=LDO3 OFF*/
#define ES_LDO3_DIS_SHIFT			   3  /* LDO3_DIS */
#define ES_LDO3_DIS_WIDTH			   1  /* LDO3_DIS */
#define ES_LDO2_DIS_MASK			0x10  /* LDO2_DIS 1=LDO2 OFF*/
#define ES_LDO2_DIS_SHIFT			   4  /* LDO2_DIS */
#define ES_LDO2_DIS_WIDTH			   1  /* LDO2_DIS */
#define ES_DIV_FOR_5_6MHZ_MASK		0x20  /* 1=44.1KHz 0=48KHz */
#define ES_DIV_FOR_5_6MHZ_SHIFT		   5  /* DIV_FOR_5_6MHZ */
#define ES_DIV_FOR_5_6MHZ_WIDTH		   1  /* DIV_FOR_5_6MHZ */
#define ES_SKIP_OUT_CAL_MASK		0x40  /* SKIP_OUT_CAL */
#define ES_SKIP_OUT_CAL_SHIFT		   6  /* SKIP_OUT_CAL */
#define ES_SKIP_OUT_CAL_WIDTH		   1  /* SKIP_OUT_CAL */
#define ES_RGLTR28_DIS_MASK			0x80  /* RGLTR28_DIS */
#define ES_RGLTR28_DIS_SHIFT		   7  /* RGLTR28_DIS */
#define ES_RGLTR28_DIS_WIDTH		   1  /* RGLTR28_DIS */


/*
 * R1 (0x01) - Override
 */

#define ES_AUTO_TSD_MASK			0x01  /* AUTO_TSD */
#define ES_AUTO_TSD_SHIFT			   0  /* AUTO_TSD */
#define ES_AUTO_TSD_WIDTH			   1  /* AUTO_TSD */
#define ES_FORCE_VCM_MASK			0x02  /* FORCE_VCM */
#define ES_FORCE_VCM_SHIFT			   1  /* FORCE_VCM */
#define ES_FORCE_VCM_WIDTH			   1  /* FORCE_VCM */
#define ES_FORCE_LDO1_MASK			0x04  /* FORCE_LDO1 */
#define ES_FORCE_LDO1_SHIFT			   2  /* FORCE_LDO1 */
#define ES_FORCE_LDO1_WIDTH			   1  /* FORCE_LDO1 */
#define ES_FORCE_LDO3_MASK			0x08  /* FORCE_LDO3 */
#define ES_FORCE_LDO3_SHIFT			   3  /* FORCE_LDO3 */
#define ES_FORCE_LDO3_WIDTH			   1  /* FORCE_LDO3 */
#define ES_FORCE_LDO2_MASK			0x10  /* FORCE_LDO2 */
#define ES_FORCE_LDO2_SHIFT			   4  /* FORCE_LDO2 */
#define ES_FORCE_LDO2_WIDTH			   1  /* FORCE_LDO2 */
#define ES_TSD_IGNORE_MASK			0x20  /* TSD_IGNORE */
#define ES_TSD_IGNORE_SHIFT			   5  /* TSD_IGNORE */
#define ES_TSD_IGNORE_WIDTH			   1  /* TSD_IGNORE */
#define ES_LVLO_IGNORE_MASK			0x40  /* LVLO_IGNORE */
#define ES_LVLO_IGNORE_SHIFT		   6  /* LVLO_IGNORE */
#define ES_LVLO_IGNORE_WIDTH		   1  /* LVLO_IGNORE */
#define ES_SD_RING_CLK_MASK			0x80  /* SD_RING_CLK */
#define ES_SD_RING_CLK_SHIFT		   7  /* SD_RING_CLK */
#define ES_SD_RING_CLK_WIDTH		   1  /* SD_RING_CLK */


/*
 * R2 (0x02) - Microphone Bias Control Register
 * Mode 0 => Pulldown
 * Mode 1 => Enable
 * Mode 2 => Hi-Z
 * Mode 3 => Bypass to 1.8V
 */

#define ES_MBIAS0_MODE_MASK			0x03  /* MBIAS0_MODE */
#define ES_MBIAS0_MODE_SHIFT			   0  /* MBIAS0_MODE */
#define ES_MBIAS0_MODE_WIDTH			   2  /* MBIAS0_MODE */
#define ES_MBIAS1_MODE_MASK			0x0C  /* MBIAS1_MODE */
#define ES_MBIAS1_MODE_SHIFT			   2  /* MBIAS1_MODE */
#define ES_MBIAS1_MODE_WIDTH			   2  /* MBIAS1_MODE */
#define ES_MBIAS2_MODE_MASK			0x30  /* MBIAS2_MODE */
#define ES_MBIAS2_MODE_SHIFT			   4  /* MBIAS2_MODE */
#define ES_MBIAS2_MODE_WIDTH			   2  /* MBIAS2_MODE */
#define ES_MBIASHS_MODE_MASK			0xC0  /* MBIASHS_MODE */
#define ES_MBIASHS_MODE_SHIFT			   6  /* MBIASHS_MODE */
#define ES_MBIASHS_MODE_WIDTH			   2  /* MBIASHS_MODE */


/*
 * R3 (0x03) - MIC0 CTRL Register
 */

#define ES_MIC0_ON_MASK				0x01  /* MIC0_ON */
#define ES_MIC0_ON_SHIFT			   0  /* MIC0_ON */
#define ES_MIC0_ON_WIDTH			   1  /* MIC0_ON */
#define ES_MIC0_GAIN_MASK			0x3E  /* MIC0_GAIN */
#define ES_MIC0_GAIN_SHIFT			   1  /* MIC0_GAIN */
#define ES_MIC0_GAIN_WIDTH			   5  /* MIC0_GAIN */
#define ES_MIC0_GAIN_MAX			0x14  /* MIC0_GAIN */
#define ES_MIC0_SE_MASK				0x40  /* MIC0_SE */
#define ES_MIC0_SE_SHIFT			   6  /* MIC0_SE */
#define ES_MIC0_SE_WIDTH			   1  /* MIC0_SE */


/*
 * R4 (0x04) - MIC Tune
 * Mode 0 => 100 kOhm input impedance
 * Mode 1 => 50 kOhm input impedance
 * Mode 2 => 25 kOhm input impedance
 * Mode 3 => Attenuate input by 3dB
 */

#define ES_MIC0_ZIN_MODE_MASK			0x03  /* MIC0_ZIN_MODE */
#define ES_MIC0_ZIN_MODE_SHIFT			   0  /* MIC0_ZIN_MODE */
#define ES_MIC0_ZIN_MODE_WIDTH			   2  /* MIC0_ZIN_MODE */
#define ES_MIC1_ZIN_MODE_MASK			0x0C  /* MIC1_ZIN_MODE */
#define ES_MIC1_ZIN_MODE_SHIFT			   2  /* MIC1_ZIN_MODE */
#define ES_MIC1_ZIN_MODE_WIDTH			   2  /* MIC1_ZIN_MODE */
#define ES_MIC2_ZIN_MODE_MASK			0x30  /* MIC2_ZIN_MODE */
#define ES_MIC2_ZIN_MODE_SHIFT			   4  /* MIC2_ZIN_MODE */
#define ES_MIC2_ZIN_MODE_WIDTH			   2  /* MIC2_ZIN_MODE */
#define ES_MICHS_ZIN_MODE_MASK			0xC0  /* MICHS_ZIN_MODE */
#define ES_MICHS_ZIN_MODE_SHIFT			   6  /* MICHS_ZIN_MODE */
#define ES_MICHS_ZIN_MODE_WIDTH			   2  /* MICHS_ZIN_MODE */


/*
 * R5 (0x05) - MIC1 CTRL Register
 */

#define ES_MIC1_ON_MASK				0x01  /* MIC1_ON */
#define ES_MIC1_ON_SHIFT			   0  /* MIC1_ON */
#define ES_MIC1_ON_WIDTH			   1  /* MIC1_ON */
#define ES_MIC1_GAIN_MASK			0x3E  /* MIC1_GAIN */
#define ES_MIC1_GAIN_SHIFT			   1  /* MIC1_GAIN */
#define ES_MIC1_GAIN_WIDTH			   5  /* MIC1_GAIN */
#define ES_MIC1_GAIN_MAX			0x14  /* MIC1_GAIN */
#define ES_MIC1_SE_MASK				0x40  /* MIC1_SE */
#define ES_MIC1_SE_SHIFT			   6  /* MIC1_SE */
#define ES_MIC1_SE_WIDTH			   1  /* MIC1_SE */


/*
 * R6 (0x06) - REC TRIM Register
 *
 * Zin Mode 0 => 100 kOhm input impedance
 * Zin Mode 1 => 50 kOhm input impedance
 * Zin Mode 2 => 25 kOhm input impedance
 * Zin Mode 3 => Attenuate input by 3dB
 *
 *  BIAS TRIM 0 => 2.5 uA
 *  BIAS TRIM 1 => 5.0 uA
 *  BIAS TRIM 2 => 0.63 uA
 *  BIAS TRIM 3 => 0.31 uA
 */

#define ES_AUX_ZIN_MODE_MASK		0x03  /* AUX_ZIN_MODE */
#define ES_AUX_ZIN_MODE_SHIFT		   0  /* AUX_ZIN_MODE */
#define ES_AUX_ZIN_MODE_WIDTH		   2  /* AUX_ZIN_MODE */
#define ES_REC_MUTE_MASK			0x04  /* REC_MUTE */
#define ES_REC_MUTE_SHIFT			   2  /* REC_MUTE */
#define ES_REC_MUTE_WIDTH			   1  /* REC_MUTE */
#define ES_TRIMDISABLE_MASK			0x08  /* TRIMDISABLE */
#define ES_TRIMDISABLE_SHIFT		   3  /* TRIMDISABLE */
#define ES_TRIMDISABLE_WIDTH		   1  /* TRIMDISABLE */
#define ES_FORCE_VCMBUF_MASK		0x10  /* FORCE_VCMBUF */
#define ES_FORCE_VCMBUF_SHIFT		   4  /* FORCE_VCMBUF */
#define ES_FORCE_VCMBUF_WIDTH		   1  /* FORCE_VCMBUF */
#define ES_HI_PERF_MASK				0x20  /* HI_PERF */
#define ES_HI_PERF_SHIFT			   5  /* HI_PERF */
#define ES_HI_PERF_WIDTH			   1  /* HI_PERF */
#define ES_I_BIAS_TRIM_MASK			0xC0  /* I_BIAS_TRIM */
#define ES_I_BIAS_TRIM_SHIFT		   6  /* I_BIAS_TRIM */
#define ES_I_BIAS_TRIM_WIDTH		   2  /* I_BIAS_TRIM */


/*
 * R7 (0x07) - AUXL CTRL Register
 */

#define ES_AUXL_ON_MASK				0x01  /* AUXL_ON */
#define ES_AUXL_ON_SHIFT			   0  /* AUXL_ON */
#define ES_AUXL_ON_WIDTH			   1  /* AUXL_ON */
#define ES_AUXL_GAIN_MASK			0x3E  /* AUXL_GAIN */
#define ES_AUXL_GAIN_SHIFT			   1  /* AUXL_GAIN */
#define ES_AUXL_GAIN_WIDTH			   5  /* AUXL_GAIN */
#define ES_AUXL_GAIN_MAX			0x14  /* AUXL_GAIN */
#define ES_AUX_STEREO_MASK			0x40  /* AUX_STEREO */
#define ES_AUX_STEREO_SHIFT			   6  /* AUX_STEREO */
#define ES_AUX_STEREO_WIDTH			   1  /* AUX_STEREO */
#define ES_AUX_SWAP_L_R_MASK		0x80  /* AUX_SWAP_L_R */
#define ES_AUX_SWAP_L_R_SHIFT		   7  /* AUX_SWAP_L_R */
#define ES_AUX_SWAP_L_R_WIDTH		   1  /* AUX_SWAP_L_R */


/*
 * R8 (0x08) - MIC2 CTRL Register
 */

#define ES_MIC2_ON_MASK				0x01  /* MIC2_ON */
#define ES_MIC2_ON_SHIFT			   0  /* MIC2_ON */
#define ES_MIC2_ON_WIDTH			   1  /* MIC2_ON */
#define ES_MIC2_GAIN_MASK			0x3E  /* MIC2_GAIN */
#define ES_MIC2_GAIN_SHIFT			   1  /* MIC2_GAIN */
#define ES_MIC2_GAIN_WIDTH			   5  /* MIC2_GAIN */
#define ES_MIC2_GAIN_MAX			0x14  /* MIC2_GAIN */
#define ES_MIC2_SE_MASK				0x40  /* MIC2_SE */
#define ES_MIC2_SE_SHIFT			   6  /* MIC2_SE */
#define ES_MIC2_SE_WIDTH			   1  /* MIC2_SE */


/*
 * R9 (0x09) - AUXR CTRL Register
 */

#define ES_AUXR_ON_MASK				0x01  /* AUXR_ON */
#define ES_AUXR_ON_SHIFT			   0  /* AUXR_ON */
#define ES_AUXR_ON_WIDTH			   1  /* AUXR_ON */
#define ES_AUXR_GAIN_MASK			0x3E  /* AUXR_GAIN */
#define ES_AUXR_GAIN_SHIFT			   1  /* AUXR_GAIN */
#define ES_AUXR_GAIN_WIDTH			   5  /* AUXR_GAIN */
#define ES_AUXR_GAIN_MAX			0x14  /* AUXR_GAIN */
#define ES_AUXR_SE_MASK				0x40  /* AUXR_SE */
#define ES_AUXR_SE_SHIFT			   6  /* AUXR_SE */
#define ES_AUXR_SE_WIDTH			   1  /* AUXR_SE */


/*
 * R10 (0x0A) - ADC CTRL Register
 */

#define ES_ADC0_ON_MASK				0x01  /* ADC0_ON */
#define ES_ADC0_ON_SHIFT			     0  /* ADC0_ON */
#define ES_ADC0_ON_WIDTH			     1  /* ADC0_ON */
#define ES_ADC1_ON_MASK				0x02  /* ADC1_ON */
#define ES_ADC1_ON_SHIFT			     1  /* ADC1_ON */
#define ES_ADC1_ON_WIDTH			     1  /* ADC1_ON */
#define ES_ADC2_ON_MASK				0x04  /* ADC2_ON */
#define ES_ADC2_ON_SHIFT			     2  /* ADC2_ON */
#define ES_ADC2_ON_WIDTH			     1  /* ADC2_ON */
#define ES_ADC3_ON_MASK				0x08  /* ADC3_ON */
#define ES_ADC3_ON_SHIFT			     3  /* ADC3_ON */
#define ES_ADC3_ON_WIDTH			     1  /* ADC3_ON */
#define ES_ADC1_IN_SEL_MASK			0x10  /* ADC1_IN_SEL */
#define ES_ADC1_IN_SEL_SHIFT			     4  /* ADC1_IN_SEL */
#define ES_ADC1_IN_SEL_WIDTH			     1  /* ADC1_IN_SEL */
#define ES_ADC2_IN_SEL_MASK			0x20  /* ADC2_IN_SEL */
#define ES_ADC2_IN_SEL_SHIFT			     5  /* ADC2_IN_SEL */
#define ES_ADC2_IN_SEL_WIDTH			     1  /* ADC2_IN_SEL */
#define ES_ADC_MUTE_MASK			0x40  /* ADC_MUTE */
#define ES_ADC_MUTE_SHIFT			     6  /* ADC_MUTE */
#define ES_ADC_MUTE_WIDTH			     1  /* ADC_MUTE */
#define ES_ADC_GAIN_H_MASK			0x80  /* ADC_GAIN_H */
#define ES_ADC_GAIN_H_SHIFT			     7  /* ADC_GAIN_H */
#define ES_ADC_GAIN_H_WIDTH			     1  /* ADC_GAIN_H */


/*
 * R11 (0x0B) - DAC CTRL Register
 */

#define ES_DAC0L_ON				0x01  /* DAC0L_ON */
#define ES_DAC0L_ON_MASK			0x01  /* DAC0L_ON */
#define ES_DAC0L_ON_SHIFT			     0  /* DAC0L_ON */
#define ES_DAC0L_ON_WIDTH			     1  /* DAC0L_ON */
#define ES_DAC0R_ON				0x02  /* DAC0R_ON */
#define ES_DAC0R_ON_MASK			0x02  /* DAC0R_ON */
#define ES_DAC0R_ON_SHIFT			     1  /* DAC0R_ON */
#define ES_DAC0R_ON_WIDTH			     1  /* DAC0R_ON */
#define ES_DAC1L_ON				0x04  /* DAC1L_ON */
#define ES_DAC1L_ON_MASK			0x04  /* DAC1L_ON */
#define ES_DAC1L_ON_SHIFT			     2  /* DAC1L_ON */
#define ES_DAC1L_ON_WIDTH			     1  /* DAC1L_ON */
#define ES_DAC1R_ON				0x08  /* DAC1R_ON */
#define ES_DAC1R_ON_MASK			0x08  /* DAC1R_ON */
#define ES_DAC1R_ON_SHIFT			     3  /* DAC1R_ON */
#define ES_DAC1R_ON_WIDTH			     1  /* DAC1R_ON */
#define ES_SQUELCH_DIS_MASK			0x40  /* SQUELCH_DIS */
#define ES_SQUELCH_DIS_SHIFT			     6  /* SQUELCH_DIS */
#define ES_SQUELCH_DIS_WIDTH			     1  /* SQUELCH_DIS */
#define ES_DAC_TRIM_DIS_MASK			0x80  /* DAC_TRIM_DIS */
#define ES_DAC_TRIM_DIS_SHIFT			     7  /* DAC_TRIM_DIS */
#define ES_DAC_TRIM_DIS_WIDTH			     1  /* DAC_TRIM_DIS */


/*
 * R13 (0x0D) - MICHS CTRL Register
 */

#define ES_MICHS_ON_MASK				0x01  /* MICHS_ON */
#define ES_MICHS_ON_SHIFT			   0  /* MICHS_ON */
#define ES_MICHS_ON_WIDTH			   1  /* MICHS_ON */
#define ES_MICHS_GAIN_MASK			0x3E  /* MICHS_GAIN */
#define ES_MICHS_GAIN_SHIFT			   1  /* MICHS_GAIN */
#define ES_MICHS_GAIN_WIDTH			   5  /* MICHS_GAIN */
#define ES_MICHS_GAIN_MAX			0x14  /* MICHS_GAIN */
#define ES_MICHS_SE_MASK				0x40  /* MICHS_SE */
#define ES_MICHS_SE_SHIFT			   6  /* MICHS_SE */
#define ES_MICHS_SE_WIDTH			   1  /* MICHS_SE */
#define ES_MICHS_IN_SEL_MASK				0x80  /* MICHS_IN_SEL */
#define ES_MICHS_IN_SEL_SHIFT			   7  /* MICHS_IN_SEL */
#define ES_MICHS_IN_SEL_WIDTH			   1  /* MICHS_IN_SEL */


/*
 * R15 (0x0F) - Earphone Gain Register
 */

#define ES_EP_GAIN_MASK				0x0F  /* EP_GAIN */
#define ES_EP_GAIN_SHIFT			     0  /* EP_GAIN */
#define ES_EP_GAIN_WIDTH			     4  /* EP_GAIN */
#define ES_EP_GAIN_MAX				0x0F  /* EP_GAIN */
#define ES_EP_MUTE				0x80  /* EP_MUTE */
#define ES_EP_MUTE_MASK				0x80  /* EP_MUTE */
#define ES_EP_MUTE_SHIFT			     7  /* EP_MUTE */
#define ES_EP_MUTE_WIDTH			     1  /* EP_MUTE */


/*
 * R16 (0x10) - Earphone Ctrl Register
 */

#define ES_EP_ON				0x01  /* EP_ON */
#define ES_EP_ON_MASK				0x01  /* EP_ON */
#define ES_EP_ON_SHIFT				     0  /* EP_ON */
#define ES_EP_ON_WIDTH				     1  /* EP_ON */
#define ES_DAC0L_TO_EP				0x02  /* DAC0L_TO_EP */
#define ES_DAC0L_TO_EP_MASK			0x02  /* DAC0L_TO_EP */
#define ES_DAC0L_TO_EP_SHIFT			     1  /* DAC0L_TO_EP */
#define ES_DAC0L_TO_EP_WIDTH			     1  /* DAC0L_TO_EP */
#define ES_AUXL_TO_EP				0x04  /* AUXL_TO_EP */
#define ES_AUXL_TO_EP_MASK			0x04  /* AUXL_TO_EP */
#define ES_AUXL_TO_EP_SHIFT			     2  /* AUXL_TO_EP */
#define ES_AUXL_TO_EP_WIDTH			     1  /* AUXL_TO_EP */
#define ES_EP_M_PD_MASK				0x10  /* EP_M_PD */
#define ES_EP_M_PD_SHIFT				     4  /* EP_M_PD */
#define ES_EP_M_PD_WIDTH				     1  /* EP_M_PD */
#define ES_EP_P_PD_MASK				0x20  /* EP_P_PD */
#define ES_EP_P_PD_SHIFT				     5  /* EP_P_PD */
#define ES_EP_P_PD_WIDTH				     1  /* EP_P_PD */
#define ES_EP_BIAS_CUR_MASK			0xC0  /* EP_BIAS_CUR */
#define ES_EP_BIAS_CUR_SHIFT			     6  /* EP_BIAS_CUR */
#define ES_EP_BIAS_CUR_WIDTH			     2  /* EP_BIAS_CUR */


/*
 * R17 (0x11) - Headphone Left Gain Register
 */

#define ES_HPL_GAIN_MASK			0x0F  /* HPL_GAIN */
#define ES_HPL_GAIN_SHIFT			   0  /* HPL_GAIN */
#define ES_HPL_GAIN_WIDTH			   4  /* HPL_GAIN */
#define ES_HPL_GAIN_MAX				0x0F  /* HPL_GAIN */
#define ES_HPL_PD_MASK				0x10  /* HPL_PD */
#define ES_HPL_PD_SHIFT				   4  /* HPL_PD */
#define ES_HPL_PD_WIDTH				   1  /* HPL_PD */
#define ES_HPL_BIAS_CUR_MASK		0x60  /* HPL_BIAS_CUR */
#define ES_HPL_BIAS_CUR_SHIFT		   5  /* HPL_BIAS_CUR */
#define ES_HPL_BIAS_CUR_WIDTH		   2  /* HPL_BIAS_CUR */
#define ES_HPL_MUTE				0x80  /* HPL_MUTE */
#define ES_HPL_MUTE_MASK			0x80  /* HPL_MUTE */
#define ES_HPL_MUTE_SHIFT			     7  /* HPL_MUTE */
#define ES_HPL_MUTE_WIDTH			     1  /* HPL_MUTE */


/*
 * R18 (0x12) - Headphone Left Ctrl Register
 */

#define ES_HPL_ON				0x01  /* HPL_ON */
#define ES_HPL_ON_MASK				0x01  /* HPL_ON */
#define ES_HPL_ON_SHIFT				     0  /* HPL_ON */
#define ES_HPL_ON_WIDTH				     1  /* HPL_ON */
#define ES_DAC0L_TO_HPL				0x02  /* DAC0L_TO_HPL */
#define ES_DAC0L_TO_HPL_MASK			0x02  /* DAC0L_TO_HPL */
#define ES_DAC0L_TO_HPL_SHIFT			     1  /* DAC0L_TO_HPL */
#define ES_DAC0L_TO_HPL_WIDTH			     1  /* DAC0L_TO_HPL */
#define ES_AUXL_TO_HPL				0x04  /* AUXL_TO_HPL */
#define ES_AUXL_TO_HPL_MASK			0x04  /* AUXL_TO_HPL */
#define ES_AUXL_TO_HPL_SHIFT			     3  /* AUXL_TO_HPL */
#define ES_AUXL_TO_HPL_WIDTH			     1  /* AUXL_TO_HPL */


/*
 * R19 (0x13) - Headphone Right Gain Register
 */

#define ES_HPR_GAIN_MASK			0x0F  /* HPR_GAIN */
#define ES_HPR_GAIN_SHIFT			     0  /* HPR_GAIN */
#define ES_HPR_GAIN_WIDTH			     4  /* HPR_GAIN */
#define ES_HPR_GAIN_MAX				0x0F  /* HPR_GAIN */
#define ES_HPR_PD_MASK				0x10  /* HPR_PD */
#define ES_HPR_PD_SHIFT				     4  /* HPR_PD */
#define ES_HPR_PD_WIDTH				     1  /* HPR_PD */
#define ES_HPR_BIAS_CUR_MASK			0x60  /* HPR_BIAS_CUR */
#define ES_HPR_BIAS_CUR_SHIFT			     5  /* HPR_BIAS_CUR */
#define ES_HPR_BIAS_CUR_WIDTH			     2  /* HPR_BIAS_CUR */
#define ES_HPR_MUTE				0x80  /* HPR_MUTE */
#define ES_HPR_MUTE_MASK			0x80  /* HPR_MUTE */
#define ES_HPR_MUTE_SHIFT			     7  /* HPR_MUTE */
#define ES_HPR_MUTE_WIDTH			     1  /* HPR_MUTE */


/*
 * R20 (0x14) - Headphone Right Ctrl Register
 */

#define ES_HPR_ON				0x01  /* HPR_ON */
#define ES_HPR_ON_MASK				0x01  /* HPR_ON */
#define ES_HPR_ON_SHIFT				     0  /* HPR_ON */
#define ES_HPR_ON_WIDTH				     1  /* HPR_ON */
#define ES_DAC0R_TO_HPR				0x02  /* DAC0R_TO_HPR */
#define ES_DAC0R_TO_HPR_MASK			0x02  /* DAC0R_TO_HPR */
#define ES_DAC0R_TO_HPR_SHIFT			     1  /* DAC0R_TO_HPR */
#define ES_DAC0R_TO_HPR_WIDTH			     1  /* DAC0R_TO_HPR */
#define ES_AUXR_TO_HPR				0x04  /* AUXR_TO_HPR */
#define ES_AUXR_TO_HPR_MASK			0x04  /* AUXR_TO_HPR */
#define ES_AUXR_TO_HPR_SHIFT			     3  /* AUXR_TO_HPR */
#define ES_AUXR_TO_HPR_WIDTH			     1  /* AUXR_TO_HPR */



/*
 * R21 (0x15) - Speaker Left Gain Register
 */

#define ES_SPKRL_GAIN_MASK			0x1F  /* SPKRL_GAIN */
#define ES_SPKRL_GAIN_SHIFT			     0  /* SPKRL_GAIN */
#define ES_SPKRL_GAIN_WIDTH			     5  /* SPKRL_GAIN */
#define ES_SPKRL_GAIN_MAX				0x1F  /* SPKRL_GAIN */
#define ES_MONO_EN_MASK				0x40  /* MONO_EN */
#define ES_MONO_EN_SHIFT			     6  /* MONO_EN */
#define ES_MONO_EN_WIDTH			     1  /* MONO_EN */
#define ES_SPKRL_MUTE				0x80  /* SPKRL_MUTE */
#define ES_SPKRL_MUTE_MASK			0x80  /* SPKRL_MUTE */
#define ES_SPKRL_MUTE_SHIFT			     7  /* SPKRL_MUTE */
#define ES_SPKRL_MUTE_WIDTH			     1  /* SPKRL_MUTE */


/*
 * R22 (0x16) - Speaker Left Ctrl Register
 */

#define ES_SPKRL_ON				0x01  /* SPKRL_ON */
#define ES_SPKRL_ON_MASK				0x01  /* SPKRL_ON */
#define ES_SPKRL_ON_SHIFT				     0  /* SPKRL_ON */
#define ES_SPKRL_ON_WIDTH				     1  /* SPKRL_ON */
#define ES_DAC0L_TO_SPKRL			0x02  /* DAC0L_TO_SPKRL */
#define ES_DAC0L_TO_SPKRL_MASK		0x02  /* DAC0L_TO_SPKRL */
#define ES_DAC0L_TO_SPKRL_SHIFT		     1  /* DAC0L_TO_SPKRL */
#define ES_DAC0L_TO_SPKRL_WIDTH		     1  /* DAC0L_TO_SPKRL */
#define ES_DAC1L_TO_SPKRL			0x04  /* DAC1L_TO_SPKRL */
#define ES_DAC1L_TO_SPKRL_MASK		0x04  /* DAC1L_TO_SPKRL */
#define ES_DAC1L_TO_SPKRL_SHIFT		     2  /* DAC1L_TO_SPKRL */
#define ES_DAC1L_TO_SPKRL_WIDTH		     1  /* DAC1L_TO_SPKRL */
#define ES_AUXL_TO_SPKRL			0x08  /* AUXL_TO_SPKRL */
#define ES_AUXL_TO_SPKRL_MASK		0x08  /* AUXL_TO_SPKRL */
#define ES_AUXL_TO_SPKRL_SHIFT		     3  /* AUXL_TO_SPKRL */
#define ES_AUXL_TO_SPKRL_WIDTH		     1  /* AUXL_TO_SPKRL */


/*
 * R23 (0x17) - Speaker Right Gain Register
 */

#define ES_SPKRR_GAIN_MASK			0x1F  /* SPKRR_GAIN */
#define ES_SPKRR_GAIN_SHIFT			     0  /* SPKRR_GAIN */
#define ES_SPKRR_GAIN_WIDTH			     5  /* SPKRR_GAIN */
#define ES_SPKRR_GAIN_MAX				0x1F  /* SPKRR_GAIN */
#define ES_SPKRR_MUTE				0x80  /* SPKRR_MUTE */
#define ES_SPKRR_MUTE_MASK			0x80  /* SPKRR_MUTE */
#define ES_SPKRR_MUTE_SHIFT			     7  /* SPKRR_MUTE */
#define ES_SPKRR_MUTE_WIDTH			     1  /* SPKRR_MUTE */


/*
 * R24 (0x18) - Speaker Right Ctrl Register
 */

#define ES_SPKRR_ON				0x01  /* SPKRR_ON */
#define ES_SPKRR_ON_MASK			0x01  /* SPKRR_ON */
#define ES_SPKRR_ON_SHIFT			     0  /* SPKRR_ON */
#define ES_SPKRR_ON_WIDTH			     1  /* SPKRR_ON */
#define ES_DAC0R_TO_SPKRR			0x02  /* DAC0R_TO_SPKRR */
#define ES_DAC0R_TO_SPKRR_MASK		0x02  /* DAC0R_TO_SPKRR */
#define ES_DAC0R_TO_SPKRR_SHIFT		     1  /* DAC0R_TO_SPKRR */
#define ES_DAC0R_TO_SPKRR_WIDTH		     1  /* DAC0R_TO_SPKRR */
#define ES_DAC1R_TO_SPKRR			0x04  /* DAC1R_TO_SPKRR */
#define ES_DAC1R_TO_SPKRR_MASK		0x04  /* DAC1R_TO_SPKRR */
#define ES_DAC1R_TO_SPKRR_SHIFT		     2  /* DAC1R_TO_SPKRR */
#define ES_DAC1R_TO_SPKRR_WIDTH		     1  /* DAC1R_TO_SPKRR */
#define ES_AUXR_TO_SPKRR			0x08  /* AUXR_TO_SPKRR */
#define ES_AUXR_TO_SPKRR_MASK		0x08  /* AUXR_TO_SPKRR */
#define ES_AUXR_TO_SPKRR_SHIFT		     3  /* AUXR_TO_SPKRR */
#define ES_AUXR_TO_SPKRR_WIDTH		     1  /* AUXR_TO_SPKRR */


/*
 * R25 (0x19) - Line Output Left Gain Register
 */

#define ES_LO_L_GAIN_MASK			0x0F  /* LO_L_GAIN */
#define ES_LO_L_GAIN_SHIFT			     0  /* LO_L_GAIN */
#define ES_LO_L_GAIN_WIDTH			     4  /* LO_L_GAIN */
#define ES_LO_L_GAIN_MAX			0x0F  /* LO_L_GAIN */
#define ES_LO_L_MUTE				0x80  /* LO_L_MUTE */
#define ES_LO_L_MUTE_MASK			0x80  /* LO_L_MUTE */
#define ES_LO_L_MUTE_SHIFT			     7  /* LO_L_MUTE */
#define ES_LO_L_MUTE_WIDTH			     1  /* LO_L_MUTE */

/*
 * R26 (0x1A) - Line Output Left Ctrl Register
 */

#define ES_LO_L_ON				0x01  /* LO_L_ON */
#define ES_LO_L_ON_MASK				0x01  /* LO_L_ON */
#define ES_LO_L_ON_SHIFT			     0  /* LO_L_ON */
#define ES_LO_L_ON_WIDTH			     1  /* LO_L_ON */
#define ES_DAC0L_TO_LO_L			0x02  /* DAC0L_TO_LO_L */
#define ES_DAC0L_TO_LO_L_MASK			0x02  /* DAC0L_TO_LO_L */
#define ES_DAC0L_TO_LO_L_SHIFT			     1  /* DAC0L_TO_LO_L */
#define ES_DAC0L_TO_LO_L_WIDTH			     1  /* DAC0L_TO_LO_L */
#define ES_DAC1L_TO_LO_L			0x04  /* DAC1L_TO_LO_L */
#define ES_DAC1L_TO_LO_L_MASK			0x04  /* DAC1L_TO_LO_L */
#define ES_DAC1L_TO_LO_L_SHIFT			     2  /* DAC1L_TO_LO_L */
#define ES_DAC1L_TO_LO_L_WIDTH			     1  /* DAC1L_TO_LO_L */
#define ES_AUXL_TO_LO_L				0x08  /* AUXL_TO_LO_L */
#define ES_AUXL_TO_LO_L_MASK			0x08  /* AUXL_TO_LO_L */
#define ES_AUXL_TO_LO_L_SHIFT			     3  /* AUXL_TO_LO_L */
#define ES_AUXL_TO_LO_L_WIDTH			     1  /* AUXL_TO_LO_L */
#define ES_LO_L_STARTUP_PULLDN_MASK		0x20  /* LO_L_STARTUP_PULLDN */
#define ES_LO_L_STARTUP_PULLDN_SHIFT		5  /* LO_L_STARTUP_PULLDN */
#define ES_LO_L_STARTUP_PULLDN_WIDTH		1  /* LO_L_STARTUP_PULLDN */
#define ES_LO_TRIM_DIS				0x80  /* LO_TRIM_DIS */
#define ES_LO_TRIM_DIS_MASK			0x80  /* LO_TRIM_DIS */
#define ES_LO_TRIM_DIS_SHIFT			     7  /* LO_TRIM_DIS */
#define ES_LO_TRIM_DIS_WIDTH			     1  /* LO_TRIM_DIS */


/*
 * R27 (0x1B) - Line Output Right Gain Register
 */

#define ES_LO_R_GAIN_MASK			0x0F  /* LO_R_GAIN */
#define ES_LO_R_GAIN_SHIFT			     0  /* LO_R_GAIN */
#define ES_LO_R_GAIN_WIDTH			     4  /* LO_R_GAIN */
#define ES_LO_R_GAIN_MAX			0x0F  /* LO_R_GAIN */
#define ES_LO_R_MUTE				0x80  /* LO_R_MUTE */
#define ES_LO_R_MUTE_MASK			0x80  /* LO_R_MUTE */
#define ES_LO_R_MUTE_SHIFT			     7  /* LO_R_MUTE */
#define ES_LO_R_MUTE_WIDTH			     1  /* LO_R_MUTE */


/*
 * R24 (0x1C) - Line Output Right Ctrl Register
 */

#define ES_LO_R_ON				0x01  /* LO_R_ON */
#define ES_LO_R_ON_MASK				0x01  /* LO_R_ON */
#define ES_LO_R_ON_SHIFT			     0  /* LO_R_ON */
#define ES_LO_R_ON_WIDTH			     1  /* LO_R_ON */
#define ES_DAC0R_TO_LO_R			0x02  /* DAC0R_TO_LO_R */
#define ES_DAC0R_TO_LO_R_MASK			0x02  /* DAC0R_TO_LO_R */
#define ES_DAC0R_TO_LO_R_SHIFT			     1  /* DAC0R_TO_LO_R */
#define ES_DAC0R_TO_LO_R_WIDTH			     1  /* DAC0R_TO_LO_R */
#define ES_DAC1R_TO_LO_R			0x04  /* DAC1R_TO_LO_R */
#define ES_DAC1R_TO_LO_R_MASK			0x04  /* DAC1R_TO_LO_R */
#define ES_DAC1R_TO_LO_R_SHIFT			     2  /* DAC1R_TO_LO_R */
#define ES_DAC1R_TO_LO_R_WIDTH			     1  /* DAC1R_TO_LO_R */
#define ES_AUXR_TO_LO_R				0x08  /* AUXR_TO_LO_R */
#define ES_AUXR_TO_LO_R_MASK			0x08  /* AUXR_TO_LO_R */
#define ES_AUXR_TO_LO_R_SHIFT			     3  /* AUXR_TO_LO_R */
#define ES_AUXR_TO_LO_R_WIDTH			     1  /* AUXR_TO_LO_R */


/*
 * R33 (0x21) - INT1 STATE
 */

#define ES_THM_SDN_INT_MASK			0x01  /* THM_SDN_INT */
#define ES_THM_SDN_INT_SHIFT		   0  /* THM_SDN_INT */
#define ES_THM_SDN_INT_WIDTH		   1  /* THM_SDN_INT */
#define ES_LINEOUT_SC_INT_MASK		0x02  /* LINEOUT_SC_INT */
#define ES_LINEOUT_SC_INT_SHIFT		   1  /* LINEOUT_SC_INT */
#define ES_LINEOUT_SC_INT_WIDTH		   1  /* LINEOUT_SC_INT */
#define ES_SPKR_L_SC_INT_MASK		0x04  /* SPKR_L_SC_INT */
#define ES_SPKR_L_SC_INT_SHIFT		   2  /* SPKR_L_SC_INT */
#define ES_SPKR_L_SC_INT_WIDTH		   1  /* SPKR_L_SC_INT */
#define ES_SPKR_R_SC_INT_MASK		0x08  /* SPKR_R_SC_INT */
#define ES_SPKR_R_SC_INT_SHIFT		   3  /* SPKR_R_SC_INT */
#define ES_SPKR_R_SC_INT_WIDTH		   1  /* SPKR_R_SC_INT */
#define ES_HP_SC_INT_MASK			0x10  /* HP_SC_INT */
#define ES_HP_SC_INT_SHIFT		   4  /* HP_SC_INT */
#define ES_HP_SC_INT_WIDTH		   1  /* HP_SC_INT */
#define ES_EP_SC_INT_MASK			0x20  /* EP_SC_INT */
#define ES_EP_SC_INT_SHIFT		   5  /* EP_SC_INT */
#define ES_EP_SC_INT_WIDTH		   1  /* EP_SC_INT */


/*
 * R34 (0x22) - INT1 MASK
 */

#define ES_THM_SDN_MASK_MASK			0x01  /* THM_SDN_MASK */
#define ES_THM_SDN_MASK_SHIFT		   0  /* THM_SDN_MASK */
#define ES_THM_SDN_MASK_WIDTH		   1  /* THM_SDN_MASK */
#define ES_LINEOUT_SC_MASK_MASK		0x02  /* LINEOUT_SC_MASK */
#define ES_LINEOUT_SC_MASK_SHIFT		   1  /* LINEOUT_SC_MASK */
#define ES_LINEOUT_SC_MASK_WIDTH		   1  /* LINEOUT_SC_MASK */
#define ES_SPKR_L_SC_MASK_MASK		0x04  /* SPKR_L_SC_MASK */
#define ES_SPKR_L_SC_MASK_SHIFT		   2  /* SPKR_L_SC_MASK */
#define ES_SPKR_L_SC_MASK_WIDTH		   1  /* SPKR_L_SC_MASK */
#define ES_SPKR_R_SC_MASK_MASK		0x08  /* SPKR_R_SC_MASK */
#define ES_SPKR_R_SC_MASK_SHIFT		   3  /* SPKR_R_SC_MASK */
#define ES_SPKR_R_SC_MASK_WIDTH		   1  /* SPKR_R_SC_MASK */
#define ES_HP_SC_MASK_MASK			0x10  /* HP_SC_MASK */
#define ES_HP_SC_MASK_SHIFT		   4  /* HP_SC_MASK */
#define ES_HP_SC_MASK_WIDTH		   1  /* HP_SC_MASK */
#define ES_EP_SC_MASK_MASK			0x20  /* EP_SC_MASK */
#define ES_EP_SC_MASK_SHIFT		   5  /* EP_SC_MASK */
#define ES_EP_SC_MASK_WIDTH		   1  /* EP_SC_MASK */


/*
 * R52 (0x34) - MB TRIM1
 */

#define ES_LD02_TRIM_MASK		0x3
#define ES_LD02_TRIM_SHIFT		6
#define ES_LD02_TRIM_WIDTH		2
#define ES_MBHS_TRIM_MASK		0x7
#define ES_MBHS_TRIM_SHIFT		3
#define ES_MBHS_TRIM_WIDTH		3
#define ES_MB2_TRIM_MASK		0x7
#define ES_MB2_TRIM_SHIFT		0
#define ES_MB2_TRIM_WIDTH		3

/*
 * R53 (0x35) - MB TRIM2
 */

#define ES_LD02_PD_MODE_MASK		0x3
#define ES_LD02_PD_MODE_SHIFT		6
#define ES_LD02_PD_MODE_WIDTH		2
#define ES_MB1_TRIM_MASK		0x7
#define ES_MB1_TRIM_SHIFT		3
#define ES_MB1_TRIM_WIDTH		3
#define ES_MB0_TRIM_MASK		0x7
#define ES_MB0_TRIM_SHIFT		0
#define ES_MB0_TRIM_WIDTH		3

/*
 * R60 (0x40) - DAC Digital Enable Register
 */

#define ES_DAC0_LEFT_EN				0x01  /* DAC0_LEFT_EN */
#define ES_DAC0_LEFT_EN_MASK			0x01  /* DAC0_LEFT_EN */
#define ES_DAC0_LEFT_EN_SHIFT			     0  /* DAC0_LEFT_EN */
#define ES_DAC0_LEFT_EN_WIDTH			     1  /* DAC0_LEFT_EN */
#define ES_DAC0_RIGHT_EN			0x02  /* DAC0_RIGHT_EN */
#define ES_DAC0_RIGHT_EN_MASK			0x02  /* DAC0_RIGHT_EN */
#define ES_DAC0_RIGHT_EN_SHIFT			     1  /* DAC0_RIGHT_EN */
#define ES_DAC0_RIGHT_EN_WIDTH			     1  /* DAC0_RIGHT_EN */
#define ES_DAC1_LEFT_EN				0x04  /* DAC1_LEFT_EN */
#define ES_DAC1_LEFT_EN_MASK			0x04  /* DAC1_LEFT_EN */
#define ES_DAC1_LEFT_EN_SHIFT			     2  /* DAC1_LEFT_EN */
#define ES_DAC1_LEFT_EN_WIDTH			     1  /* DAC1_LEFT_EN */
#define ES_DAC1_RIGHT_EN			0x08  /* DAC1_RIGHT_EN */
#define ES_DAC1_RIGHT_EN_MASK			0x08  /* DAC1_RIGHT_EN */
#define ES_DAC1_RIGHT_EN_SHIFT			     3  /* DAC1_RIGHT_EN */
#define ES_DAC1_RIGHT_EN_WIDTH			     1  /* DAC1_RIGHT_EN */
#define ES_DAC_CLK_EN				0x10  /* DAC_CLK_EN */
#define ES_DAC_CLK_EN_MASK			0x10  /* DAC_CLK_EN */
#define ES_DAC_CLK_EN_SHIFT			     4  /* DAC_CLK_EN */
#define ES_DAC_CLK_EN_WIDTH			     1  /* DAC_CLK_EN */
#define ES_DIG_CLK_EN				0x20  /* DIG_CLK_EN */
#define ES_DIG_CLK_EN_MASK			0x20  /* DIG_CLK_EN */
#define ES_DIG_CLK_EN_SHIFT			     5  /* DIG_CLK_EN */
#define ES_DIG_CLK_EN_WIDTH			     1  /* DIG_CLK_EN */
#define ES_I2S_MODE				0x40  /* 1=master 0=slave */
#define ES_I2S_MODE_MASK			0x40  /* I2S_MODE */
#define ES_I2S_MODE_SHIFT			     6  /* I2S_MODE */
#define ES_I2S_MODE_WIDTH			     1  /* I2S_MODE */
#define ES_I2S_LEFT_JUSTIFIED			0x80  /* 1=Left justified */
#define ES_I2S_LEFT_JUSTIFIED_MASK		0x80  /* I2S_LEFT_JUSTIFIED */
#define ES_I2S_LEFT_JUSTIFIED_SHIFT		     7  /* I2S_LEFT_JUSTIFIED */
#define ES_I2S_LEFT_JUSTIFIED_WIDTH		     1  /* I2S_LEFT_JUSTIFIED */


/*
 * R61 (0x41) - DAC Digital Channel Register
 */

#define ES_I2S_CH				0x01  /* 1=4 chn 0=2 chn */
#define ES_I2S_CH_MASK				0x01  /* I2S_CH */
#define ES_I2S_CH_SHIFT				     0  /* I2S_CH */
#define ES_I2S_CH_WIDTH				     1  /* I2S_CH */
#define ES_I2S_PIN_EN				0x02  /* 1=I2s clock 0=mclk */
#define ES_I2S_PIN_EN_MASK			0x02  /* I2S_PIN_EN */
#define ES_I2S_PIN_EN_SHIFT			     1  /* I2S_PIN_EN */
#define ES_I2S_PIN_EN_WIDTH			     1  /* I2S_PIN_EN */
#define ES_I2S_CLK_SEL				0x04  /* 1=mclk/2 0=mclk */
#define ES_I2S_CLK_SEL_MASK			0x04  /* I2S_CLK_SEL */
#define ES_I2S_CLK_SEL_SHIFT			     2  /* I2S_CLK_SEL */
#define ES_I2S_CLK_SEL_WIDTH			     1  /* I2S_CLK_SEL */
#define ES_I2S_CLK_SEL_CLKDIV2			0x08  /* 1=clk/2 0=clk */
#define ES_I2S_CLK_SEL_CLKDIV2_MASK		0x08  /* I2S_CLK_SEL_CLKDIV2*/
#define ES_I2S_CLK_SEL_CLKDIV2_SHIFT		     3  /* I2S_CLK_SEL_CLKDIV2*/
#define ES_I2S_CLK_SEL_CLKDIV2_WIDTH		     1  /* I2S_CLK_SEL_CLKDIV2*/
#define ES_CLK_SOURCE				0x10  /* 1=mclk 0=i2s_clk_in*/
#define ES_CLK_SOURCE_MASK			0x10  /* CLK_SOURCE */
#define ES_CLK_SOURCE_SHIFT			     4  /* CLK_SOURCE */
#define ES_CLK_SOURCE_WIDTH			     1  /* CLK_SOURCE */


/*
 * R63 (0x43) - DAC Digital I2S1 Register
 */

#define ES_BITS_PER_SAMPLE_MASK			0x1F  /* BITS_PER_SAMPLE */
#define ES_BITS_PER_SAMPLE_SHIFT		   0  /* BITS_PER_SAMPLE */
#define ES_BITS_PER_SAMPLE_WIDTH		   5  /* BITS_PER_SAMPLE */
#define ES_FS_POL_MASK				0x60  /* FS_POL */
#define ES_FS_POL_SHIFT			     5  /* FS_POL */
#define ES_FS_POL_WIDTH			     2  /* FS_POL */
#define ES_CLK_POL_MASK				0x80  /* CLK_POL */
#define ES_CLK_POL_SHIFT			     7  /* CLK_POL */
#define ES_CLK_POL_WIDTH			     1  /* CLK_POL */


/*
 * R65 (0x45) - Frame Sync Selection Register
 */

#define ES_FS_SEL_MASK			0x03  /* FS_SEL */
#define ES_FS_SEL_SHIFT		   0  /* FS_SEL */
#define ES_FS_SEL_WIDTH		   2  /* FS_SEL */
#define ES_FS_OVERRIDE_MASK		0x04  /* FS_OVERRIDE */
#define ES_FS_OVERRIDE_SHIFT		   2  /* FS_OVERRIDE */
#define ES_FS_OVERRIDE_WIDTH		   1  /* FS_OVERRIDE */

int es_analog_add_snd_soc_controls(struct snd_soc_codec *codec);
int es_analog_add_snd_soc_dapm_controls(struct snd_soc_codec *codec);
int es_analog_add_snd_soc_route_map(struct snd_soc_codec *codec);


#endif
