/*
 * es705.h  --  ES705 Soc Audio driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ES705_H
#define _ES705_H

#include <linux/cdev.h>
#include <linux/mutex.h>
#include <sound/soc.h>
#include <linux/time.h>

#include "es705-uart.h"

#ifdef CONFIG_SND_SOC_ES_SPI_SENSOR_HUB
#include "es705-sensorhub-demo.h"
#endif

#define PARSE_BUFFER_SIZE (PAGE_SIZE * 2)

/* TODO: condition of kernel version or commit code to specific kernels */
#ifdef CONFIG_SLIMBUS_MSM_NGD
#define ES705_DAI_ID_BASE	0
#define DAI_INDEX(xid)		(xid)

#else
#define ES705_DAI_ID_BASE	1
#define DAI_INDEX(xid)		(xid - 1)
#endif

#define ES705_READ_VE_OFFSET		0x0804
#define ES705_READ_VE_WIDTH		4
#define ES705_WRITE_VE_OFFSET		0x0800
#define ES705_WRITE_VE_WIDTH		4

#define ES705_MCLK_DIV			0x0000
#define ES705_CLASSD_CLK_DIV		0x0001
#define ES705_CP_CLK_DIV		0x0002

#define ES705_BOOT_CMD			0x0001
#define ES705_BOOT_ACK			0x01010101

#define ES705_WDB_CMD			0x802f
#define ES705_RDB_CMD			0x802e
#define ES705_WDB_MAX_SIZE		512

#define ES705_SYNC_CMD			0x8000
#define ES705_SYNC_POLLING		0x0000
#define ES705_SYNC_INTR_ACITVE_LOW	0x0001
#define ES705_SYNC_INTR_ACITVE_HIGH	0x0002
#define ES705_SYNC_INTR_FALLING_EDGE	0x0003
#define ES705_SYNC_INTR_RISING_EDGE	0x0004
#define ES705_SYNC_ACK			0x80000000
#define ES705_SBL_ACK			0x8000FFFF

#define ES705_RESET_CMD			0x8002
#define ES705_RESET_IMMED		0x0000
#define ES705_RESET_DELAYED		0x0001

#define ES705_GET_POWER_STATE		0x800F
#define ES705_SET_POWER_STATE		0x9010
#define ES705_SET_POWER_STATE_SLEEP	0x0001
#define ES705_SET_POWER_STATE_MP_SLEEP	0x0002
#define ES705_SET_POWER_STATE_MP_CMD	0x0003
#define ES705_SET_POWER_STATE_NORMAL	0x0004
#define ES705_SET_POWER_STATE_VS_OVERLAY	0x0005
#define ES705_SET_POWER_STATE_VS_LOWPWR	0x0006

#define ES705_STREAM_UART_ON		0x90250101
#define ES705_STREAM_UART_OFF		0x90250100

#define ES705_SET_SMOOTH			0x904E
#define ES705_SET_SMOOTH_RATE		0x0000

#define ES705_SET_SMOOTH_MUTE_ZERO	0x804E0000
#define ES705_SET_PRESET		0x8031
#define ES705_SET_POWER_LEVEL		0x8011
#define ES705_POWER_LEVEL_6			0x0006
#define ES705_GET_POWER_LEVEL		0x8012
#define ES705_DMIC_PRESET		0x0F08
/*
 * bit15 - reserved
 * bit[14:12] - access type
 * bit11 - commit = 0, staged = 1
 * bit[10:0] - psuedo address
 */
#define ES705_ACCESS_MASK	(7 << 12)
#define ES705_ALGO_ACCESS	(0 << 12)
#define ES705_DEV_ACCESS	(1 << 12)
#define ES705_CMD_ACCESS	(2 << 12)
#define ES705_OTHER_ACCESS	(3 << 12)

#define ES705_CMD_MASK		(1 << 11)
#define ES705_STAGED_CMD	(1 << 11)
#define ES705_COMMIT_CMD	(0 << 11)

#define ES705_ADDR_MASK		0x7ff

#define ES705_STAGED_MSG_BIT	(1 << 13)
/*
 * Device parameter command codes
 */
#define ES705_DEV_PARAM_OFFSET		0x2000
#define ES705_GET_DEV_PARAM		0x800b
#define ES705_SET_DEV_PARAM_ID		0x900c
#define ES705_SET_DEV_PARAM		0x900d

/*
 * Algoithm parameter command codes
 */
#define ES705_ALGO_PARAM_OFFSET		0x0000
#define ES705_GET_ALGO_PARAM		0x8016
#define ES705_SET_ALGO_PARAM_ID		0x9017
#define ES705_SET_ALGO_PARAM		0x9018

/*
 * addresses
 */
enum {
	ES705_MIC_CONFIG,
	ES705_AEC_MODE,
	ES705_VEQ_ENABLE,
	ES705_DEREVERB_ENABLE,
	ES705_DEREVERB_GAIN,
	ES705_BWE_ENABLE,
	ES705_BWE_HIGH_BAND_GAIN,
	ES705_BWE_MAX_SNR,
	ES705_BWE_POST_EQ_ENABLE,
	ES705_SLIMBUS_LINK_MULTI_CHANNEL,
	ES705_POWER_STATE,
	ES705_FE_STREAMING,
	ES705_PRESET,
	ES705_ALGO_PROCESSING,
	ES705_ALGO_SAMPLE_RATE,
	ES705_CHANGE_STATUS,
	ES705_MIX_SAMPLE_RATE,
	ES705_FW_FIRST_CHAR,
	ES705_FW_NEXT_CHAR,
	ES705_EVENT_RESPONSE,
	ES705_VOICE_SENSE_ENABLE,
	ES705_VOICE_SENSE_SET_KEYWORD,
	ES705_VOICE_SENSE_EVENT,
	ES705_VOICE_SENSE_TRAINING_MODE,
	ES705_VOICE_SENSE_DETECTION_SENSITIVITY,
	ES705_VOICE_ACTIVITY_DETECTION_SENSITIVITY,
	ES705_VOICE_SENSE_TRAINING_RECORD,
	ES705_VOICE_SENSE_TRAINING_STATUS,
	ES705_VOICE_SENSE_DEMO_ENABLE,
	ES705_VS_STORED_KEYWORD,
	ES705_VS_INT_OSC_MEASURE_START,
	ES705_VS_INT_OSC_MEASURE_STATUS,
	ES705_CVS_PRESET,
	ES705_RX_ENABLE,
	ES705_API_ADDR_MAX,
};

#define ES705_SLIM_CH_RX_OFFSET		152
#define ES705_SLIM_CH_TX_OFFSET		156
/* #define ES705_SLIM_RX_PORTS		10 */
#define ES705_SLIM_RX_PORTS		6
#define ES705_SLIM_TX_PORTS		6

#define ES705_NUM_CODEC_SLIM_DAIS	6

#define ES705_NUM_CODEC_I2S_DAIS	4

#define ES705_I2S_PORTA		7
#define ES705_I2S_PORTB		8
#define ES705_I2S_PORTC		9
#define ES705_I2S_PORTD		10

#define ES705_NS_ON_PRESET		969
#define ES705_NS_OFF_PRESET		624
#define ES705_SW_ON_PRESET		702
#define ES705_SW_OFF_PRESET		703
#define ES705_STS_ON_PRESET		984
#define ES705_STS_OFF_PRESET	985
#define ES705_RX_NS_ON_PRESET	996
#define ES705_RX_NS_OFF_PRESET	997
#define ES705_WNF_ON_PRESET		994
#define ES705_WNF_OFF_PRESET	995
#define ES705_BWE_ON_PRESET		622
#define ES705_BWE_OFF_PRESET	623
#define ES705_AVALON_WN_ON_PRESET	704
#define ES705_AVALON_WN_OFF_PRESET	705
#define ES705_VBB_ON_PRESET		706
#define ES705_VBB_OFF_PRESET	707

#define ES705_VS_PRESET		1382

#define ES705_AUD_ZOOM_PRESET			1355
#define ES705_AUD_ZOOM_NARRATOR_PRESET	756
#define ES705_AUD_ZOOM_SCENE_PRESET		757
#define ES705_AUD_ZOOM_NARRATION_PRESET	758


#define ES705_NUM_CODEC_DAIS	(ES705_NUM_CODEC_SLIM_DAIS + ES705_NUM_CODEC_I2S_DAIS)

enum {
	ES705_SLIM_1_PB = ES705_DAI_ID_BASE,
	ES705_SLIM_1_CAP,
	ES705_SLIM_2_PB,
	ES705_SLIM_2_CAP,
	ES705_SLIM_3_PB,
	ES705_SLIM_3_CAP,
};

enum {
	SBL,
	STANDARD,
	VOICESENSE,
};

enum {
	NO_EVENT,
	CODEC_EVENT,
	KW_DETECTED,
};

enum {
	POWER_LEVEL_0 = 0,
	POWER_LEVEL_1,
	POWER_LEVEL_2,
	POWER_LEVEL_3,
	POWER_LEVEL_4,
	POWER_LEVEL_5,
	POWER_LEVEL_6,
	POWER_LEVEL_MAX
};

enum {
	UART_RATE_4608,
	UART_RATE_9216,
	UART_RATE_1kk,
	UART_RATE_1M,
	UART_RATE_1152k,
	UART_RATE_2kk,
	UART_RATE_2M,
	UART_RATE_3kk,
	UART_RATE_3M,
	UART_RATE_MAX
};

#define ES705_SLIM_INTF		0
#define ES705_I2C_INTF		1
#define ES705_SPI_INTF         2
#define ES705_UART_INTF		3

struct es705_slim_dai_data {
	unsigned int rate;
	unsigned int *ch_num;
	unsigned int ch_act;
	unsigned int ch_tot;
};

struct es705_slim_ch {
	u32	sph;
	u32	ch_num;
	u16	ch_h;
	u16	grph;
};

enum {
	ES705_PM_ACTIVE,
	ES705_PM_SUSPENDING,
	ES705_PM_SUSPENDED,
	ES705_PM_SUSPENDING_TO_VS,
	ES705_PM_SUSPENDED_VS
};

enum {
	ES705_AUD_ZOOM_DISABLED,
	ES705_AUD_ZOOM_NARRATOR,
	ES705_AUD_ZOOM_SCENE,
	ES705_AUD_ZOOM_NARRATION
};

/* Maximum size of keyword parameter block in bytes. */
#define ES705_VS_KEYWORD_PARAM_MAX 512

/* Base name used by character devices. */
#define ES705_CDEV_NAME "adnc"

/* device ops table for streaming operations */
struct es_stream_device {
	int (*open)(struct es705_priv *es705);
	int (*read)(struct es705_priv *es705, void *buf, int len);
	int (*close)(struct es705_priv *es705);
	int (*wait)(struct es705_priv *es705);
	int intf;
};

struct es_datablock_device {
	int (*open)(struct es705_priv *es705);
	int (*read)(struct es705_priv *es705, void *buf, int len);
	int (*close)(struct es705_priv *es705);
	int (*wait)(struct es705_priv *es705);
	int (*rdb) (struct es705_priv *es705, void *buf, int id);
	int (*wdb) (struct es705_priv *es705, const void *buf, int len);
	int intf;
};

struct es705_priv {
	struct device *dev;
	struct snd_soc_codec *codec;
	struct firmware *standard;
	struct firmware *vs;

	unsigned int intf;

	struct esxxx_platform_data *pdata;
	struct es_stream_device streamdev;
	struct es_datablock_device datablockdev;

	int (*dev_read)(struct es705_priv *es705, void *buf, int len);
	int (*dev_write)(struct es705_priv *es705, const void *buf, int len);
	int (*dev_write_then_read)(struct es705_priv *es705, const void *buf,
					int len, u32 *rspn, int match);

	int (*boot_setup)(struct es705_priv *es705);
	int (*boot_finish)(struct es705_priv *es705);
	int (*uart_fw_download) (struct es705_priv *es705, int fw_type);

	int (*dev_rdb)(struct es705_priv *es705, void *buf, int id);
	int (*dev_wdb)(struct es705_priv *es705, const void *buf, int len);

	struct timespec last_resp_time;
	u32 last_response;
	int (*cmd)(struct es705_priv *es705, u32 cmd, int sr, u32 *resp);

	struct i2c_client *i2c_client;
	struct slim_device *intf_client;
	struct slim_device *gen0_client;
	struct spi_device *spi_client;
	struct es705_uart_device uart_dev;

	struct mutex api_mutex;
	struct mutex datablock_read_mutex;
	struct mutex streaming_mutex;
#if defined(CONFIG_MQ100_SENSOR_HUB)
	struct mutex uart_transaction_mutex;
#endif
	struct delayed_work sleep_work;

	struct es705_slim_dai_data dai[ES705_NUM_CODEC_SLIM_DAIS];
	struct es705_slim_ch slim_rx[ES705_SLIM_RX_PORTS];
	struct es705_slim_ch slim_tx[ES705_SLIM_TX_PORTS];

	struct mutex pm_mutex;
	struct mutex wake_mutex;
	struct mutex abort_mutex;
	int vs_abort_kw;
	int pm_state;
	int ns;/*Noise suppression flag, used for read status*/
	int zoom;/*Audio Zoom status*/
	int mode;
	int wake_count;
	int sleep_enable;
	int sleep_delay;
	int sleep_abort;
	int fw_requested;
	int vs_get_event;
	int vs_enable;
	int vs_wakeup_keyword;
	int uart_state;
	bool no_more_bit;
	u16 vs_keyword_param_size;
	u8 vs_keyword_param[ES705_VS_KEYWORD_PARAM_MAX];
	char *rdb_read_buffer;
	int rdb_read_count;
	u16 cvs_preset;
	u16 preset;

	long internal_route_num;
	long internal_rate;
	unsigned int rx1_route_enable;
	unsigned int tx1_route_enable;
	unsigned int rx2_route_enable;

	unsigned int ap_tx1_ch_cnt;

	unsigned int es705_power_state;
	unsigned int auto_power_preset;
	unsigned int uart_app_rate;

	struct cdev cdev_command;
	struct cdev cdev_streaming;
	struct cdev cdev_firmware;
	struct cdev cdev_datablock;
	struct cdev cdev_datalogging;
	unsigned int datablock_intf;

	struct task_struct *stream_thread;
	wait_queue_head_t stream_in_q;

#ifdef CONFIG_SND_SOC_ES_SPI_SENSOR_HUB
	struct spi_data *sensData;
	struct workqueue_struct *spi_workq;
	struct work_struct sensor_event_work;
#endif
};

extern struct es705_priv es705_priv;
extern struct snd_soc_codec_driver soc_codec_dev_es705;
extern struct snd_soc_dai_driver es705_dai[];

extern int es705_core_probe(struct device *dev);

extern int es705_bootup(struct es705_priv *es705);

extern int remote_esxxx_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai);

extern int remote_esxxx_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai);
extern void es705_gpio_reset(struct es705_priv *es705);
#if defined(CONFIG_MQ100_SENSOR_HUB)
extern void es705_indicate_state_change(u8 val);
#endif
#define es705_resp(obj) ((obj)->last_response)
int es705_cmd(struct es705_priv *es705, u32 cmd);
int es705_cmd_without_sleep(struct es705_priv *es705, u32 cmd);

int es705_bus_init(struct es705_priv *es705);
extern int fw_download(void *arg);

extern u32 es705_streaming_cmds[];

#define ES705_STREAM_DISABLE	0
#define ES705_STREAM_ENABLE	1
#define ES705_DATALOGGING_CMD_ENABLE	0x803f0001
#define ES705_DATALOGGING_CMD_DISABLE	0x803f0000
#define ES705_VS_FW_CHUNK	512
/* #define ES705_WAIT_TIMEOUT	2*/
#define MAX_RETRY_TO_SWITCH_TO_LOW_POWER_MODE	5

#endif /* _ES705_H */
