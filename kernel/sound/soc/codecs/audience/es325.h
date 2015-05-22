/*
 * es325.h  --  ES325 Soc Audio driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ES325_H
#define _ES325_H

#include <linux/cdev.h>
#include <linux/mutex.h>
#include <sound/soc.h>
#include <linux/time.h>
#include "escore.h"

#if defined(CONFIG_SND_SOC_ES_UART) || \
	defined(CONFIG_SND_SOC_ES_HIGH_BW_BUS_UART)
#include "escore-uart-common.h"
#endif

#define ES_DAI_ID_BASE	1

#define ES_MCLK_DIV			0x0000
#define ES_CLASSD_CLK_DIV		0x0001
#define ES_CP_CLK_DIV			0x0002

#define ES_SYNC_INTR_ACITVE_LOW		0x0001
#define ES_SYNC_INTR_ACITVE_HIGH	0x0002
#define ES_SYNC_INTR_FALLING_EDGE	0x0003
#define ES_SYNC_INTR_RISING_EDGE	0x0004

#define ES_RESET_CMD			0x8002
#define ES_RESET_IMMED			0x0000
#define ES_RESET_DELAYED		0x0001

#define ES_POWER_STATE_SLEEP		0x0001
#define ES_POWER_STATE_NORMAL		0x0004
#define ES_POWER_STATE_VS_OVERLAY	0x0005
#define ES_POWER_STATE_VS_LOWPWR	0x0006
#define ES_POWER_STATE_SLEEP_PENDING	0x0007
#define ES_POWER_STATE_SLEEP_REQUESTED	0x0008

#define ES_SMOOTH_MUTE			0x904E0000
/*
 * bit15 - reserved
 * bit[14:12] - access type
 * bit11 - commit = 0, staged = 1
 * bit[10:0] - psuedo address
 */
#define ES_ACCESS_MASK		(7 << 12)
#define ES_ALGO_ACCESS		(0 << 12)
#define ES_DEV_ACCESS		(1 << 12)
#define ES_CMD_ACCESS		(2 << 12)
#define ES_OTHER_ACCESS		(3 << 12)

#define ES_CMD_MASK		(1 << 11)
#define ES_STAGED_CMD		(1 << 11)
#define ES_COMMIT_CMD		(0 << 11)

#define ES_ADDR_MASK		0x7ff

#define ES_STAGED_MSG_BIT	(1 << 13)
/*
 * Device parameter command codes
 */
#define ES_DEV_PARAM_OFFSET		0x2000
#define ES_GET_DEV_PARAM		0x800b
#define ES_SET_DEV_PARAM_ID		0x900c
#define ES_SET_DEV_PARAM		0x900d

/*
 * Algoithm parameter command codes
 */
#define ES_ALGO_PARAM_OFFSET		0x0000
#define ES_GET_ALGO_PARAM		0x8016
#define ES_SET_ALGO_PARAM_ID		0x9017
#define ES_SET_ALGO_PARAM		0x9018

/* Speculative sleep delay in msecs */
#define ES_SLEEP_DELAY	300

#define ES_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |\
			SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 |\
			SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |\
			SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000)
#define ES_SLIMBUS_RATES (SNDRV_PCM_RATE_48000)

#define ES_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S16_BE |\
			SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_S20_3BE |\
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S24_BE |\
			SNDRV_PCM_FMTBIT_S32_LE | SNDRV_PCM_FMTBIT_S32_BE)
#define ES_SLIMBUS_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
			SNDRV_PCM_FMTBIT_S16_BE)

/*
 * addresses
 */
enum {
	ES_MIC_CONFIG,
	ES_POWER_STATE,
	ES_ALGO_PROCESSING,
	ES_ALGO_SAMPLE_RATE,
	ES_CHANGE_STATUS,
	ES_FW_FIRST_CHAR,
	ES_FW_NEXT_CHAR,
	ES_API_ADDR_MAX,
};

#define ES_SLIM_1_PB_MAX_CHANS		2
#define ES_SLIM_1_CAP_MAX_CHANS		2
#define ES_SLIM_2_PB_MAX_CHANS		2
#define ES_SLIM_2_CAP_MAX_CHANS		2
#define ES_SLIM_3_PB_MAX_CHANS		2
#define ES_SLIM_3_CAP_MAX_CHANS		2

#define ES_SLIM_1_PB_OFFSET		0
#define ES_SLIM_2_PB_OFFSET		2
#define ES_SLIM_3_PB_OFFSET		4
#define ES_SLIM_1_CAP_OFFSET		0
#define ES_SLIM_2_CAP_OFFSET		2
#define ES_SLIM_3_CAP_OFFSET		4


#define ES_SLIM_CH_RX_OFFSET		152
#define ES_SLIM_CH_TX_OFFSET		156
#define ES_SLIM_RX_PORTS		6
#define ES_SLIM_TX_PORTS		6

#define ES_I2S_PORTA			7
#define ES_I2S_PORTB			8
#define ES_I2S_PORTC			9
#define ES_I2S_PORTD			10

#if defined(CONFIG_SND_SOC_ES_SLIM)
#define ES_NUM_CODEC_SLIM_DAIS		6
#define ES_NUM_CODEC_I2S_DAIS		0
#else
#define ES_NUM_CODEC_SLIM_DAIS		0
#define ES_NUM_CODEC_I2S_DAIS		4
#endif

#define ES_NUM_CODEC_DAIS	(ES_NUM_CODEC_SLIM_DAIS + \
				 ES_NUM_CODEC_I2S_DAIS)


enum {
	ES_SLIM_1_PB = ES_DAI_ID_BASE,
	ES_SLIM_1_CAP,
	ES_SLIM_2_PB,
	ES_SLIM_2_CAP,
	ES_SLIM_3_PB,
	ES_SLIM_3_CAP,
};

enum {
	ES_PM_ACTIVE,
	ES_PM_SUSPENDING,
	ES_PM_SUSPENDED,
};

/* Maximum size of keyword parameter block in bytes. */
#define ES_VS_KEYWORD_PARAM_MAX 512

/* Base name used by character devices. */
#define ES_CDEV_NAME "adnc"

#if defined(CONFIG_SND_SOC_ES_UART) || \
	defined(CONFIG_SND_SOC_ES_HIGH_BW_BUS_UART)
#define UART_FW_IMAGE_1_SIZE    80

/* Stage 1 bootloaders to configure baudrate in eS325 */
static u8 UARTFirstStageBoot_InputClk_19_200_Baud_3M[UART_FW_IMAGE_1_SIZE] = {
	0x41, 0x55, 0x44, 0x49, 0x45, 0x4E, 0x43, 0x45, 0x00, 0x00, 0x00, 0x00,
	0x05, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x84, 0x00, 0x01, 0x20,
	0x01, 0x00, 0x00, 0x00,
	0x01, 0x00, 0x00, 0x00, 0xC0, 0x00, 0x01, 0x20, 0x01, 0x00, 0x00, 0x00,
	0x01, 0x00, 0x00, 0x00,
	0xC4, 0x00, 0x01, 0x20, 0x12, 0x00, 0x08, 0x00, 0x01, 0x00, 0x00, 0x00,
	0xC8, 0x00, 0x01, 0x20,
	0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xD0, 0x00, 0x01, 0x20,
	0x05, 0x2F, 0x00, 0x00
};

#endif

extern struct snd_soc_codec_driver soc_codec_dev_ess325;
extern struct snd_soc_dai_driver ess325_dai[];

extern int es325_core_probe(struct device *dev);

extern int es325_bootup(struct escore_priv *ess325);

#define ess325_resp(obj) ((obj)->last_response)
int es325_cmd(struct escore_priv *ess325, u32 cmd);
int es325_bus_init(struct escore_priv *ess325);
int es325_set_streaming(struct escore_priv *escore, int value);
void es325_slim_setup(struct escore_priv *escore_priv);

#endif /* _ES325_H */
