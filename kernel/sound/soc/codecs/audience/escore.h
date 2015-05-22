/*
 * escore.h  --  Audicnece earSmart Soc Audio driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ESCORE_H
#define _ESCORE_H

#include <linux/cdev.h>
#include <linux/mutex.h>
#include <linux/time.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/completion.h>
#include <linux/i2c.h>
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
#include <sound/jack.h>
#include <linux/kthread.h>
#include <linux/device.h>
#include <linux/pm_runtime.h>
#include <linux/pm_wakeup.h>
#include <linux/mutex.h>
#include <linux/spi/spi.h>
#include <linux/esxxx.h>
#include <linux/of_gpio.h>
#include <linux/wait.h>
#if defined(CONFIG_SND_SOC_ES_SLIM)
#include <linux/slimbus/slimbus.h>
#endif
#include <linux/version.h>
#include <linux/pm.h>
#include "escore-uart.h"

#include <mach/mt_gpio.h>

#define ES_READ_VE_OFFSET		0x0804
#define ES_READ_VE_WIDTH		4
#define ES_WRITE_VE_OFFSET		0x0800
#define ES_WRITE_VE_WIDTH		4

#define ES_CMD_ACCESS_WR_MAX 8
#define ES_CMD_ACCESS_RD_MAX 8

/* Standard commands used by all chips */


#define ES_SYNC_CMD			0x8000
#define ES_SYNC_POLLING			0x0000
#define ES_SYNC_ACK			0x80000000
#define ES_SUPRESS_RESPONSE		0x1000

#define ES_GET_POWER_STATE		0x800f
#define ES_SET_POWER_STATE		0x9010
#define ES_SET_POWER_STATE_SLEEP	0x0001
#define ES_SET_POWER_STATE_MP_SLEEP	0x0002
#define ES_SET_POWER_STATE_MP_CMD	0x0003
#define ES_SET_POWER_STATE_NORMAL	0x0004
#define ES_SET_POWER_STATE_VS_OVERLAY	0x0005
#define ES_SET_POWER_STATE_VS_LOWPWR	0x0006

#define ES_SYNC_MAX_RETRY 5

#define ES_NOT_READY		0x00000000
#define ES_ILLEGAL_CMD		0xFFFF0000

#define ES_EVENT_RESPONSE_CMD	0x801a

/*
 * Codec Interrupt event type
 */
#define ES_NO_EVENT			0x0000
#define ES_CODEC_INTR_EVENT		0x0001
#define ES_VS_INTR_EVENT		0x0002
#define ES_MASK_INTR_EVENT		0x0000FFFF
/*
 * Interrupt status bits
 */

/* Specific to A212 Codec */
#define ES_IS_CODEC_READY(x)		(x & 0x80)
#define ES_IS_THERMAL_SHUTDOWN(x)	(x & 0x100)
#define ES_IS_LO_SHORT_CKT(x)		(x & 0x200)
#define ES_IS_HFL_SHORT_CKT(x)		(x & 0x400)
#define ES_IS_HFR_SHORT_CKT(x)		(x & 0x800)
#define ES_IS_HP_SHORT_CKT(x)		(x & 0x1000)
#define ES_IS_EP_SHORT_CKT(x)		(x & 0x2000)
#define ES_IS_PLUG_EVENT(x)		(x & 0x40)
#define ES_IS_UNPLUG_EVENT(x)		(x & 0x20)

/*
 * Accessory status bits
 */
#define ES_IS_ACCDET_EVENT(x)			(x & 0x10)
#define ES_IS_SHORT_BTN_PARALLEL_PRESS(x)	(x & 0x01)
#define ES_IS_LONG_BTN_PARALLEL_PRESS(x)	(x & 0x02)
#define ES_IS_SHORT_BTN_SERIAL_PRESS(x)		(x & 0x04)
#define ES_IS_LONG_BTN_SERIAL_PRESS(x)		(x & 0x08)
#define ES_IS_BTN_PRESS_EVENT(x)		(x & 0x0f)

#define ES_IS_LRGM_HEADSET(x)			(x == 1)
#define ES_IS_LRMG_HEADSET(x)			(x == 3)
#define ES_IS_LRG_HEADPHONE(x)			(x == 2)
#define ES_IS_HEADSET(x) (ES_IS_LRGM_HEADSET(x) || ES_IS_LRMG_HEADSET(x))

#define ES_ACCDET_ENABLE	1
#define ES_ACCDET_DISABLE	0

#define ES_BTNDET_ENABLE	1
#define ES_BTNDET_DISABLE	0

#define ES_SYNC_POLLING                0x0000
#define ES_SYNC_INTR_ACITVE_LOW        0x0001
#define ES_SYNC_INTR_ACITVE_HIGH       0x0002
#define ES_SYNC_INTR_FALLING_EDGE      0x0003
#define ES_SYNC_INTR_RISING_EDGE       0x0004

#define ES_WDB_CMD			0x802f
#define ES_RDB_CMD			0x802e
#define ES_WDB_MAX_SIZE			512

#define ES_READ_DATA_BLOCK				0x802E
#define ES_WRITE_DATA_BLOCK				0x802F
#define ES_SET_SMOOTH_MUTE				0x804E
#define ES_SMOOTH_MUTE_ZERO				0x0000

#define ES_SET_POWER_LEVEL				0x8011
#define ES_POWER_LEVEL_6				0x0006

#define ES_RESP_TOUT_MSEC			20 /* 20ms */
#define ES_RESP_TOUT				20000 /* 20ms */
#define ES_RESP_POLL_TOUT			4000  /*  4ms */
#define ES_FIN_TOUT				10000 /* 10ms */
#define ES_FIN_POLL_TOUT			2000  /* 2ms */
#define ES_MAX_RETRIES		\
			(ES_RESP_TOUT / ES_RESP_POLL_TOUT)
#define ES_MAX_FIN_RETRIES		\
			(ES_FIN_TOUT / ES_FIN_POLL_TOUT)

#define ES_UART_WAKE_CMD	0xa

enum {
	SBL,
	STANDARD,
	VOICESENSE,
};

struct escore_reg_cache {
	int value;
	int is_volatile;
};

struct escore_api_access {
	u32 read_msg[ES_CMD_ACCESS_RD_MAX];
	unsigned int read_msg_len;
	u32 write_msg[ES_CMD_ACCESS_WR_MAX];
	unsigned int write_msg_len;
	unsigned int val_shift;
	unsigned int val_max;
};

enum {
	ES_INVAL_INTF,
	ES_SLIM_INTF = 0x1,
	ES_I2C_INTF,
	ES_SPI_INTF,
	ES_UART_INTF,
};

enum {
	ES_MSG_READ,
	ES_MSG_WRITE,
};

enum {
	ES_SLIM_CH_TX,
	ES_SLIM_CH_RX,
	ES_SLIM_CH_UND,
};

enum {
	ES_PM_NORMAL,
	ES_PM_ASLEEP,
};

/* Notifier chain priority */
enum {
	ES_LOW,
	ES_NORMAL,
	ES_HIGH,
};

struct escore_slim_dai_data {
	unsigned int rate;
	unsigned int *ch_num;
	unsigned int ch_act;
	unsigned int ch_tot;
};

struct escore_slim_ch {
	u32	sph;
	u32	ch_num;
	u16	ch_h;
	u16	grph;
};
#define DAI_INDEX(xid)		(xid - 1)

/* Maximum size of keyword parameter block in bytes. */
#define ESCORE_VS_KEYWORD_PARAM_MAX 512

/* Base name used by character devices. */
#define ESCORE_CDEV_NAME "adnc"

/* device ops table for streaming operations */
struct es_stream_device {
	int (*open)(struct escore_priv *escore);
	int (*read)(struct escore_priv *escore, void *buf, int len);
	int (*close)(struct escore_priv *escore);
	int (*wait)(struct escore_priv *escore);
	int intf;
	int no_more_bit;
};

/* device ops table for datablock operations */
struct es_datablock_device {
	int (*open)(struct escore_priv *escore);
	int (*read)(struct escore_priv *escore, void *buf, int len);
	int (*close)(struct escore_priv *escore);
	int (*wait)(struct escore_priv *escore);
	char *rdb_read_buffer;
	int rdb_read_count;
	struct mutex datablock_read_mutex;
	struct mutex datablock_mutex;
};

struct escore_intr_regs {
	u32 get_intr_status;
	u32 clear_intr_status;
	u32 set_intr_mask;
	u32 accdet_config;
	u32 accdet_status;
	u32 enable_btndet;

	u32 btn_serial_cfg;
	u32 btn_parallel_cfg;
	u32 btn_detection_rate;
	u32 btn_press_settling_time;
	u32 btn_bounce_time;
	u32 btn_long_press_time;
};

struct escore_flags {
	u32 is_codec:1;
	u32 is_fw_ready:1;
	u32 local_slim_ch_cfg:1;
	u32 rx1_route_enable:1;
	u32 tx1_route_enable:1;
	u32 rx2_route_enable:1;
/* 705 */
	u32 vs_enable:1;
	u32 sleep_enable:1;
	u32 sleep_abort:1;
	u32 ns:1;
	u32 zoom:2;	/* Value can be in range 0-3 */
};

/*Generic boot ops*/
struct escore_boot_ops {
	int (*setup)(struct escore_priv *escore);
	int (*finish)(struct escore_priv *escore);
	int (*bootup)(struct escore_priv *escore);
};

/*Generic Bus ops*/
struct escore_bus_ops {
	int (*read)(struct escore_priv *escore, void *buf, int len);
	int (*write)(struct escore_priv *escore,
			const void *buf, int len);
	int (*high_bw_write)(struct escore_priv *escore,
			const void *buf, int len);
	int (*high_bw_read)(struct escore_priv *escore,
			void *buf, int len);
	int (*cmd)(struct escore_priv *escore, u32 cmd, u32 *resp);
	int (*high_bw_cmd)(struct escore_priv *escore, u32 cmd, u32 *resp);
	int (*high_bw_wait)(struct escore_priv *escore);
	int (*high_bw_open)(struct escore_priv *escore);
	int (*high_bw_close)(struct escore_priv *escore);
};

/*Generic bus function*/
struct escore_bus {
	struct escore_bus_ops ops;
	void (*setup_prim_intf)(struct escore_priv *escore);
	int (*setup_high_bw_intf)(struct escore_priv *escore);
	u32 last_response;
};

/* escore device pm_ops */
struct escore_pm_ops {
	int (*prepare)(struct device *dev);
	void (*complete)(struct device *dev);
	int (*suspend)(struct device *dev);
	int (*resume)(struct device *dev);
	int (*suspend_noirq)(struct device *dev);
	int (*resume_noirq)(struct device *dev);
	int (*runtime_suspend)(struct device *dev);
	int (*runtime_resume)(struct device *dev);
	int (*runtime_idle)(struct device *dev);
};

struct escore_macro {
	u32 cmd;
	unsigned long timestamp;
};
#define	ES_MAX_ROUTE_MACRO_CMD		100
extern struct escore_macro cmd_hist[ES_MAX_ROUTE_MACRO_CMD];
extern int cmd_hist_index;

struct escore_priv {
	struct device *dev;
	struct snd_soc_codec *codec;
	struct snd_soc_dai_ops i2s_dai_ops;
	struct snd_soc_dai_ops slim_dai_ops;
	struct firmware *standard;
	int mode;
	struct escore_flags flag;
	unsigned int pri_intf;
	unsigned int high_bw_intf;
	unsigned int wakeup_intf;
	struct completion cmd_compl;
	void *voice_sense;

	struct esxxx_platform_data *pdata;
	struct es_stream_device streamdev;
	struct es_datablock_device datablock_dev;

	struct escore_boot_ops boot_ops;
	struct escore_bus bus;
	int (*probe)(struct device *dev);

	int (*sleep)(struct escore_priv *escore);
	int (*wakeup)(struct escore_priv *escore);

	int pm_use_vs;
	int pm_enable;
	struct escore_pm_ops non_vs_pm_ops;
	struct escore_pm_ops vs_pm_ops;
	unsigned int vs_pm_state;
	unsigned int non_vs_pm_state;
	unsigned long pm_time;

	void (*slim_setup)(struct escore_priv *escore);
	void (*init_slim_slave)(struct escore_priv *escore);
	int (*remote_cfg_slim_rx)(int dai_id);
	int (*remote_cfg_slim_tx)(int dai_id);
	int (*remote_close_slim_rx)(int dai_id);
	int (*remote_close_slim_tx)(int dai_id);
	int (*remote_route_enable)(struct snd_soc_dai *dai);
	int (*channel_dir)(int dir);

	int (*set_streaming)(struct escore_priv *escore, int val);
	int (*set_datalogging)(struct escore_priv *escore, int val);

	struct escore_slim_dai_data *slim_dai_data;
	struct escore_slim_ch *slim_rx;
	struct escore_slim_ch *slim_tx;

	u16 codec_slim_dais;
	u16 slim_tx_ports;
	u16 slim_rx_ports;

	int *slim_tx_port_to_ch_map;
	int *slim_rx_port_to_ch_map;
	int *slim_be_id;

	unsigned int ap_tx1_ch_cnt;

	struct timespec last_resp_time;

	struct slim_device *intf_client;
	struct slim_device *gen0_client;

	struct mutex api_mutex;
	struct mutex streaming_mutex;
/* if 705 */
	struct mutex abort_mutex;
	struct mutex wake_mutex;
	struct mutex atablock_read_mutex;
	struct delayed_work vs_fw_load;

	int sleep_delay;
	int vs_abort_kw;
	int es705_power_state;
	int wake_count;
	int auto_power_preset;
	int fw_requested;
	u16 preset;
	u16 cvs_preset;
/* endif 705 */

	struct mutex pm_mutex;
	int pm_state;

	struct mutex msg_list_mutex;
	struct list_head msg_list;

	long internal_route_num;
	long internal_rate;

	struct cdev cdev_command;
	struct cdev cdev_streaming;
	struct cdev cdev_firmware;
	struct cdev cdev_datablock;
	struct cdev cdev_datalogging;

	struct task_struct *stream_thread;
	wait_queue_head_t stream_in_q;

	struct snd_soc_codec_driver *soc_codec_dev_escore;
	struct snd_soc_dai_driver *dai;
	u32 dai_nr;
	u32 api_addr_max;
	atomic_t active_streams;
	u8 process_analog;
	u8 process_digital;
	struct escore_intr_regs *regs;

	struct escore_api_access *api_access;
	struct escore_reg_cache *reg_cache;
	void *priv;
	struct delayed_work sleep_work;

	struct blocking_notifier_head *irq_notifier_list;
	struct snd_soc_jack *jack;
	u8 algo_type;
	u8 algo_rate;
	u8 VP_Asr;
	u8 pcm_port;
	u8 cmd_compl_mode;
	u8 uart_ready;
	int intf_probed;
	char *device_name;
	int escore_power_state;
	u32 escore_event_type;
};

#define escore_resp(obj) ((obj)->bus.last_response)
extern struct escore_priv escore_priv;
extern int escore_read_and_clear_intr(struct escore_priv *escore);
extern int escore_accdet_config(struct escore_priv *escore, int enable);
extern int escore_btndet_config(struct escore_priv *escore, int enable);
extern int escore_process_accdet(struct escore_priv *escore);

extern void escore_process_analog_intr(struct escore_priv *escore);
extern void escore_process_digital_intr(struct escore_priv *escore);

extern void escore_gpio_reset(struct escore_priv *escore);

extern irqreturn_t escore_irq_work(int irq, void *data);
extern int escore_cmd(struct escore_priv *escore, u32 cmd, u32 *resp);
extern int escore_write_block(struct escore_priv *escore,
		const u32 *cmd_block);
extern unsigned int escore_read(struct snd_soc_codec *codec,
			       unsigned int reg);
extern int escore_write(struct snd_soc_codec *codec, unsigned int reg,
		       unsigned int value);
extern int escore_prepare_msg(struct escore_priv *escore, unsigned int reg,
		unsigned int value, char *msg, int *len, int msg_type);
extern int escore_put_control_enum(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol);
extern int escore_get_control_enum(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol);
extern int escore_put_control_value(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol);
extern int escore_get_control_value(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol);
void escore_register_notify(struct blocking_notifier_head *list,
		struct notifier_block *nb);
int escore_probe(struct escore_priv *escore, struct device *dev, int curr_intf);
extern int escore_datablock_read(struct escore_priv *escore, void *buf,
					size_t len, int id);
extern int escore_datablock_write(struct escore_priv *escore, void *buf,
					size_t len);
extern int escore_datablock_open(struct escore_priv *escore);
extern int escore_datablock_close(struct escore_priv *escore);
extern int escore_datablock_wait(struct escore_priv *escore);
extern int escore_datablock_read(struct escore_priv *escore, void *buf,
					size_t len, int id);
extern int escore_datablock_write(struct escore_priv *escore, void *buf,
					size_t len);
extern int escore_datablock_open(struct escore_priv *escore);
extern int escore_datablock_close(struct escore_priv *escore);
extern int escore_datablock_wait(struct escore_priv *escore);
void escore_pm_init(void);
void escore_pm_enable(void);
void escore_pm_disable(void);
void escore_pm_vs_enable(struct escore_priv *escore, bool value);
int escore_suspend(struct device *dev);
int escore_resume(struct device *dev);
void escore_pm_put_autosuspend(void);
int escore_pm_get_sync(void);
extern const struct dev_pm_ops escore_pm_ops;

#define ESCORE_STREAM_DISABLE	0
#define ESCORE_STREAM_ENABLE	1
#define ESCORE_DATALOGGING_CMD_ENABLE    0x803f0001
#define ESCORE_DATALOGGING_CMD_DISABLE   0x803f0000
#endif /* _ESCORE_H */
