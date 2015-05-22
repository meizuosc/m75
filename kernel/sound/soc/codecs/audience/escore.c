#include "escore.h"
#include "escore-i2c.h"
#include "escore-slim.h"
#include "escore-spi.h"
#include "escore-uart.h"
#include "escore-i2s.h"

struct escore_macro cmd_hist[ES_MAX_ROUTE_MACRO_CMD] = { {0} };
int cmd_hist_index;
/* History struture, log route commands to debug */
/* Send a single command to the chip.
 *
 * If the SR (suppress response bit) is NOT set, will read the
 * response and cache it the driver object retrieve with escore_resp().
 *
 * Returns:
 * 0 - on success.
 * EITIMEDOUT - if the chip did not respond in within the expected time.
 * E* - any value that can be returned by the underlying HAL.
 */

static int _escore_cmd(struct escore_priv *escore, u32 cmd, u32 *resp)
{
	int sr;
	int err;

	sr = cmd & BIT(28);
	cmd_hist[cmd_hist_index].cmd = cmd;
	cmd_hist[cmd_hist_index].timestamp = jiffies;
	if (cmd_hist_index == ES_MAX_ROUTE_MACRO_CMD-1)
		cmd_hist_index = 0;
	else
		cmd_hist_index++;
	err = escore->bus.ops.cmd(escore, cmd, resp);
	if (err || sr)
		goto cmd_err;

	if (resp == 0) {
		err = -ETIMEDOUT;
		dev_err(escore->dev, "no response to command 0x%08x\n", cmd);
	} else {
		escore->bus.last_response = *resp;
		get_monotonic_boottime(&escore->last_resp_time);
	}
cmd_err:
	return err;
}

int escore_cmd(struct escore_priv *escore, u32 cmd, u32 *resp)
{
	int ret;
	ret = escore_pm_get_sync();
	if (ret > -1) {
		mutex_lock(&escore_priv.api_mutex);
		ret = _escore_cmd(escore, cmd, resp);
		mutex_unlock(&escore_priv.api_mutex);
		escore_pm_put_autosuspend();
	}
	return ret;
}
int escore_write_block(struct escore_priv *escore, const u32 *cmd_block)
{
	int ret = 0;
	u32 resp;
	ret = escore_pm_get_sync();
	if (ret > -1) {
		mutex_lock(&escore_priv.api_mutex);
		while (*cmd_block != 0xffffffff) {
			_escore_cmd(escore, *cmd_block, &resp);
			usleep_range(1000, 1000);
			cmd_block++;
		}
		mutex_unlock(&escore->api_mutex);
		escore_pm_put_autosuspend();
	}
	return ret;
}

int escore_prepare_msg(struct escore_priv *escore, unsigned int reg,
		       unsigned int value, char *msg, int *len, int msg_type)
{
	struct escore_api_access *api_access;
	u32 api_word[2] = {0};
	unsigned int val_mask;
	int msg_len;

	if (reg > escore->api_addr_max) {
		pr_err("%s(): invalid address = 0x%04x\n", __func__, reg);
		return -EINVAL;
	}

	pr_debug("%s(): reg=%08x val=%d\n", __func__, reg, value);

	api_access = &escore->api_access[reg];
	val_mask = (1 << get_bitmask_order(api_access->val_max)) - 1;

	if (msg_type == ES_MSG_WRITE) {
		msg_len = api_access->write_msg_len;
		memcpy((char *)api_word, (char *)api_access->write_msg,
				msg_len);

		switch (msg_len) {
		case 8:
			api_word[1] |= (val_mask & value);
			break;
		case 4:
			api_word[0] |= (val_mask & value);
			break;
		}
	} else {
		msg_len = api_access->read_msg_len;
		memcpy((char *)api_word, (char *)api_access->read_msg,
				msg_len);
	}

	*len = msg_len;
	memcpy(msg, (char *)api_word, *len);

	return 0;

}

static unsigned int _escore_read(struct snd_soc_codec *codec, unsigned int reg)
{
	struct escore_priv *escore = &escore_priv;
	u32 api_word[2] = {0};
	unsigned int msg_len;
	unsigned int value = 0;
	u32 resp;
	int rc;

	rc = escore_prepare_msg(escore, reg, value, (char *) api_word,
			&msg_len, ES_MSG_READ);
	if (rc) {
		pr_err("%s(): Failed to prepare read message\n", __func__);
		goto out;
	}

	rc = _escore_cmd(escore, api_word[0], &resp);
	if (rc < 0) {
		pr_err("%s(): escore_cmd()", __func__);
		return rc;
	}
	api_word[0] = escore->bus.last_response;

	value = api_word[0] & 0xffff;
out:
	return value;
}

unsigned int escore_read(struct snd_soc_codec *codec, unsigned int reg)
{
	unsigned int ret = 0;
	int rc;
	rc = escore_pm_get_sync();
	if (rc > -1) {
		mutex_lock(&escore_priv.api_mutex);
		ret = _escore_read(codec, reg);
		mutex_unlock(&escore_priv.api_mutex);
		escore_pm_put_autosuspend();
	}
	return ret;
}

static int _escore_write(struct snd_soc_codec *codec, unsigned int reg,
		       unsigned int value)
{
	struct escore_priv *escore = &escore_priv;
	u32 api_word[2] = {0};
	int msg_len;
	u32 resp;
	int rc;
	int i;

	rc = escore_prepare_msg(escore, reg, value, (char *) api_word,
			&msg_len, ES_MSG_WRITE);
	if (rc) {
		pr_err("%s(): Failed to prepare write message\n", __func__);
		goto out;
	}

	for (i = 0; i < msg_len / 4; i++) {
		rc = _escore_cmd(escore, api_word[i], &resp);
		if (rc < 0) {
			pr_err("%s(): escore_cmd()", __func__);
			return rc;
		}
	}
	pr_debug("%s(): mutex unlock\n", __func__);
out:
	return rc;
}

int escore_datablock_open(struct escore_priv *escore)
{
	int rc = 0;
	if (escore->bus.ops.high_bw_open)
		rc = escore->bus.ops.high_bw_open(escore);
	return rc;
}

int escore_datablock_close(struct escore_priv *escore)
{
	int rc = 0;
	if (escore->bus.ops.high_bw_close)
		rc = escore->bus.ops.high_bw_close(escore);
	return rc;
}

int escore_datablock_wait(struct escore_priv *escore)
{
	int rc = 0;
	if (escore->bus.ops.high_bw_wait)
		rc = escore->bus.ops.high_bw_wait(escore);
	return rc;
}

int escore_datablock_read(struct escore_priv *escore, void *buf,
		size_t len, int id)
{
	int rc;
	int size;
	u32 cmd;
	u32 resp;
	u8 flush_extra_blk = 0;
	u32 flush_buf;

	/* Reset read data block size */
	escore->datablock_dev.rdb_read_count = 0;

	mutex_lock(&escore->api_mutex);
	rc = escore_pm_get_sync();
	if (rc < 0) {
		pr_err("%s() escore_pm_get_sync() failed rc = %x\n",
			__func__, rc);
		goto pm_get_sync_err;
	}

	cmd = (ES_READ_DATA_BLOCK << 16) | (id & 0xFFFF);

	rc = escore->bus.ops.high_bw_cmd(escore, cmd, &resp);
	if (rc < 0) {
		pr_err("%s(): escore_cmd() failed rc = %d\n", __func__, rc);
		goto out;
	}
	if ((resp >> 16) != ES_READ_DATA_BLOCK) {
		pr_err("%s(): Invalid response received: 0x%08x\n",
				__func__, resp);
		rc = -EINVAL;
		goto out;
	}

	size = resp & 0xFFFF;
	pr_debug("%s(): RDB size = %d\n", __func__, size);
	if (size == 0 || size % 4 != 0) {
		pr_err("%s(): Read Data Block with invalid size:%d\n",
				__func__, size);
		rc = -EINVAL;
		goto out;
	}

	if (len != size) {
		pr_debug("%s(): Requested:%d Received:%d\n", __func__,
				len, size);
		if (len < size)
			flush_extra_blk = (size - len) % 4;
		else
			len = size;
	}

	rc = escore->bus.ops.high_bw_read(escore, buf, len);
	if (rc < 0) {
		pr_err("%s(): Read Data Block error %d\n",
				__func__, rc);
		goto out;
	}

	/* Store read data block size */
	escore->datablock_dev.rdb_read_count = size;

	/* No need to read in case of no extra bytes */
	if (flush_extra_blk) {
		/* Discard the extra bytes */
		rc = escore->bus.ops.high_bw_read(escore, &flush_buf,
							flush_extra_blk);
		if (rc < 0) {
			pr_err("%s(): Read Data Block error in flushing %d\n",
					__func__, rc);
			goto out;
		}
	}
	escore_pm_put_autosuspend();
	mutex_unlock(&escore->api_mutex);
	return len;
out:
	escore_pm_put_autosuspend();
pm_get_sync_err:
	mutex_unlock(&escore->api_mutex);
	return rc;
}

int escore_datablock_write(struct escore_priv *escore, void *buf,
		size_t len)
{
	int rc;
	u32 resp;
	u32 cmd = ES_WRITE_DATA_BLOCK << 16;

	mutex_lock(&escore->api_mutex);
	rc = escore_pm_get_sync();
	if (rc < 0) {
		pr_err("%s() escore_pm_get_sync() failed rc = %x\n",
			__func__, rc);
		goto pm_get_sync_err;
	}

	cmd = cmd | (len & 0xFFFF);
	rc = escore->bus.ops.high_bw_cmd(escore, cmd, &resp);
	if (rc < 0) {
		pr_err("%s(): escore_cmd() failed rc = %d\n", __func__, rc);
		goto out;
	}
	if ((resp >> 16) != ES_WRITE_DATA_BLOCK) {
		pr_err("%s(): Invalid response received: 0x%08x\n",
				__func__, resp);
		rc = -EIO;
		goto out;
	}

	rc = escore->bus.ops.high_bw_write(escore, buf, len);
	if (rc < 0) {
		pr_err("%s(): WDB error:%d\n", __func__, rc);
		goto out;
	}
	usleep_range(10000, 10000);
	rc = escore->bus.ops.high_bw_read(escore, &resp, sizeof(resp));
	if (rc < 0) {
		pr_err("%s(): WDB last ACK read error:%d\n", __func__, rc);
		goto out;
	}

	if (resp & 0xff000000) {
		pr_err("%s(): write data block error 0x%0x\n",
				__func__, resp);
		rc = -EIO;
		goto out;
	}

	escore_pm_put_autosuspend();
	mutex_unlock(&escore->api_mutex);
	return len;

out:
	escore_pm_put_autosuspend();
pm_get_sync_err:
	mutex_unlock(&escore->api_mutex);
	return rc;
}

int escore_write(struct snd_soc_codec *codec, unsigned int reg,
		       unsigned int value)
{
	int ret;
	ret = escore_pm_get_sync();
	if (ret > -1) {
		mutex_lock(&escore_priv.api_mutex);
		ret = _escore_write(codec, reg, value);
		mutex_unlock(&escore_priv.api_mutex);
		escore_pm_put_autosuspend();
	}
	return ret;

}
int escore_read_and_clear_intr(struct escore_priv *escore)
{
	int value;
	int ret;
	struct snd_soc_codec *codec = escore->codec;

	pr_debug("%s()\n", __func__);
	ret = escore_pm_get_sync();
	if (ret > -1) {
		mutex_lock(&escore_priv.api_mutex);
		value = _escore_read(codec, escore->regs->get_intr_status);
		if (value < 0) {
			pr_err("%s(): Reading System Interrupt Status failed\n",
				__func__);
			ret = value;
			goto read_error;
		}
		ret = _escore_write(codec, escore->regs->clear_intr_status,
				value);
		if (ret < 0)
			pr_err("%s(): Clearing interrupt status failed\n",
				__func__);
read_error:
		mutex_unlock(&escore_priv.api_mutex);
		escore_pm_put_autosuspend();
	}
	return ret;

}

int escore_accdet_config(struct escore_priv *escore, int enable)
{
	int ret;
	struct snd_soc_codec *codec = escore->codec;

	pr_debug("%s()\n", __func__);
	ret = escore_pm_get_sync();
	if (ret > -1) {
		mutex_lock(&escore_priv.api_mutex);
		ret = _escore_write(codec, escore->regs->accdet_config, enable);
		if (ret < 0)
			pr_err("Accdet detection enabling failed\n");
		mutex_unlock(&escore_priv.api_mutex);
		escore_pm_put_autosuspend();
	}
	return ret;
}
EXPORT_SYMBOL_GPL(escore_accdet_config);

static int _escore_btndet_config(struct escore_priv *escore, int enable)
{
	int rc;
	struct snd_soc_codec *codec = escore->codec;
	struct esxxx_accdet_config *accdet_cfg = &escore->pdata->accdet_cfg;

	pr_debug("%s()\n", __func__);
	rc = _escore_write(codec, escore->regs->enable_btndet, enable);
	if (rc < 0) {
		pr_err("Button detection enabling failed\n");
		goto btndet_config_error;
	}

	if (enable) {
		/* Enable serial button config */
		if (accdet_cfg->btn_serial_cfg != -1) {
			rc = _escore_write(codec,
					escore->regs->btn_serial_cfg,
					accdet_cfg->btn_serial_cfg);
			if (rc < 0) {
				pr_err("Serial button config failed\n");
				goto btndet_config_error;
			}
		}

		/* Enable parallel button config */
		if (accdet_cfg->btn_parallel_cfg != -1) {
			rc = _escore_write(codec,
					escore->regs->btn_parallel_cfg,
					accdet_cfg->btn_parallel_cfg);
			if (rc < 0) {
				pr_err("Parallel button config failed\n");
				goto btndet_config_error;
			}
		}

		/* Set button detection rate */
		if (accdet_cfg->btn_detection_rate != -1) {
			rc = _escore_write(codec,
					escore->regs->btn_detection_rate,
					accdet_cfg->btn_detection_rate);
			if (rc < 0) {
				pr_err("Set button detection rate failed\n");
				goto btndet_config_error;
			}
		}

		/* Set settling time config for button press */
		if (accdet_cfg->btn_press_settling_time != -1) {
			rc = _escore_write(codec,
					escore->regs->btn_press_settling_time,
					accdet_cfg->btn_press_settling_time);
			if (rc < 0) {
				pr_err("Set button settling time failed\n");
				goto btndet_config_error;
			}
		}

		/* Set bounce time config for button press */
		if (accdet_cfg->btn_bounce_time != -1) {
			rc = _escore_write(codec,
					escore->regs->btn_bounce_time,
					accdet_cfg->btn_bounce_time);
			if (rc < 0) {
				pr_err("Set button bounce time failed\n");
				goto btndet_config_error;
			}
		}

		/*
		 * Sets the time duration for a button press necessary
		 * to classify a press detection event as a LONG button
		 * press
		 */

		if (accdet_cfg->btn_long_press_time != -1) {
			rc = _escore_write(codec,
					escore->regs->btn_long_press_time,
					accdet_cfg->btn_long_press_time);
		if (rc < 0) {
			pr_err("Long Button Press config failed\n");
			goto btndet_config_error;
			}
		}
	}

btndet_config_error:
	return rc;
}

int escore_btndet_config(struct escore_priv *escore, int enable)
{
	int ret;
	pr_debug("%s()\n", __func__);
	ret = escore_pm_get_sync();
	if (ret > -1) {
		mutex_lock(&escore_priv.api_mutex);
		ret =  _escore_btndet_config(escore, enable);
		mutex_unlock(&escore_priv.api_mutex);
		escore_pm_put_autosuspend();
	}
	return ret;
}
EXPORT_SYMBOL_GPL(escore_btndet_config);

int escore_process_accdet(struct escore_priv *escore)
{
	int value;
	struct snd_soc_codec *codec = escore->codec;

	pr_debug("%s()\n", __func__);
	value = escore_pm_get_sync();
	if (value > -1) {
		mutex_lock(&escore_priv.api_mutex);
		/* Find out type of accessory using Get Accessory Detect
		 * Status Command */
		value = _escore_read(codec, escore->regs->accdet_status);
		if (value < 0) {
			pr_err("%s(): Enable button detect failed\n", __func__);
			goto accdet_error;
		} else if (ES_IS_LRG_HEADPHONE(value))
			pr_info("%s(): LRG Headphone\n", __func__);
		else
			pr_info("Unknown Accessory detected\n");
accdet_error:
		mutex_unlock(&escore_priv.api_mutex);
		escore_pm_put_autosuspend();
	}
	return value;
}
EXPORT_SYMBOL_GPL(escore_process_accdet);

/*
 * Placeholder for digital chip related interrupts
 */
void escore_process_digital_intr(struct escore_priv *escore)
{
	if (!escore->process_digital)
		return;

	/* TODO: Add the generic digital interrupt handling */
}

/*
 * Processes the various analog interrupts. Detects the type of
 * accessory plugged in (either headphone or headset) and configures
 * the accessory if required.
 *
 * TODO: Report the jack plug/unplug event to userspace
 */
void escore_process_analog_intr(struct escore_priv *escore)
{
	int rc = 0;
	int value;

	pr_debug("%s()\n", __func__);
	if (!escore->process_analog)
		goto process_analog_intr_error;

	value = escore_read_and_clear_intr(escore);
	if (value < 0)
		goto process_analog_intr_error;

	if (ES_IS_PLUG_EVENT(value)) {

		pr_info("%s(): Plug event\n", __func__);
		/* Enable accessory detection */
		rc = escore_accdet_config(escore, ES_ACCDET_ENABLE);
		if (rc < 0) {
			pr_err("%s(): Enabling accessory detection failed\n",
					__func__);
			goto process_analog_intr_error;
		}
	} else if (ES_IS_UNPLUG_EVENT(value)) {

		pr_info("%s(): Unplug event\n", __func__);
		/* Disable button detection */
		rc = escore_btndet_config(escore, ES_BTNDET_DISABLE);
		if (rc < 0) {
			pr_err("%s(): Disabling button detection failed\n",
					__func__);
			goto process_analog_intr_error;
		}

		/* Disable accessory detection */
		rc = escore_accdet_config(escore, ES_ACCDET_DISABLE);
		if (rc < 0) {
			pr_err("%s(): Disabling accessory detection failed\n",
					__func__);
			goto process_analog_intr_error;
		}
	} else if (ES_IS_ACCDET_EVENT(value)) {

		pr_info("%s(): Accdet event\n", __func__);
		/* Process accessory detection */
		rc = escore_process_accdet(escore);
		if (rc < 0) {
			pr_err("%s(): Processing accessory detection failed\n",
					__func__);
			goto process_analog_intr_error;
		}

	} else if (ES_IS_BTN_PRESS_EVENT(value)) {

		if (ES_IS_SHORT_BTN_PARALLEL_PRESS(value))
			pr_info("%s(): Short button parallel press event\n",
					__func__);
		else if (ES_IS_SHORT_BTN_SERIAL_PRESS(value))
			pr_info("%s(): Short button serial press event\n",
					__func__);
		else if (ES_IS_LONG_BTN_PARALLEL_PRESS(value))
			pr_info("%s(): Long button parallel press event\n",
					__func__);
		else if (ES_IS_LONG_BTN_SERIAL_PRESS(value))
			pr_info("%s(): Long button serial press event\n",
					__func__);
	} else
		pr_info("%s(): Unknown Interrupt %x\n", __func__, value);

process_analog_intr_error:
	return;
}


/*
 * Generic ISR for Audience chips. It is divided mainly into two parts to
 * process interrupts for:
 * 1) chips containing codec
 * 2) chips only having digital component
 */

irqreturn_t escore_irq_work(int irq, void *data)
{
	struct escore_priv *escore = data;

	escore_process_digital_intr(escore);

	escore_process_analog_intr(escore);

	return IRQ_HANDLED;
}

int escore_put_control_enum(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	unsigned int value;
	int rc = 0;
	rc = escore_pm_get_sync();
	if (rc > -1) {
		mutex_lock(&escore_priv.api_mutex);
		value = ucontrol->value.enumerated.item[0];
		rc = _escore_write(NULL, reg, value);
		mutex_unlock(&escore_priv.api_mutex);
		escore_pm_put_autosuspend();
	}
	return 0;
}

int escore_get_control_enum(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	int ret;
	struct soc_enum *e =
		(struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	unsigned int value;

	ret = escore_pm_get_sync();
	if (ret > -1) {
		mutex_lock(&escore_priv.api_mutex);
		value = _escore_read(NULL, reg);
		ucontrol->value.enumerated.item[0] = value;
		mutex_unlock(&escore_priv.api_mutex);
		escore_pm_put_autosuspend();
	}
	return 0;
}

int escore_put_control_value(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	int ret = 0;

	ret = escore_pm_get_sync();
	if (ret  > -1) {
		mutex_lock(&escore_priv.api_mutex);
		value = ucontrol->value.integer.value[0];
		ret = _escore_write(NULL, reg, value);
		mutex_unlock(&escore_priv.api_mutex);
		escore_pm_put_autosuspend();
	}
	return ret;
}

int escore_get_control_value(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int ret;
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value;
	ret = escore_pm_get_sync();
	if (ret  > -1) {
		mutex_lock(&escore_priv.api_mutex);
		value = _escore_read(NULL, reg);
		ucontrol->value.integer.value[0] = value;
		mutex_unlock(&escore_priv.api_mutex);
		escore_pm_put_autosuspend();
		ret = 0;
	}
	return ret;
}

void escore_register_notify(struct blocking_notifier_head *list,
		struct notifier_block *nb)
{
	blocking_notifier_chain_register(list, nb);
}

void escore_gpio_reset(struct escore_priv *escore)
{
	if (escore->pdata->reset_gpio == -1) {
		pr_warn("%s(): Reset GPIO not initialized\n", __func__);
		return;
	}
#ifdef CONFIG_ARCH_MT6595
	mt_set_gpio_out(escore->pdata->reset_gpio, 0);
	usleep_range(1000, 1000);
	mt_set_gpio_out(escore->pdata->reset_gpio, 1);
	usleep_range(10000, 10000);
#else
	gpio_set_value(escore->pdata->reset_gpio, 0);
	/* Wait 1 ms then pull Reset signal in High */
	usleep_range(1000, 1000);
	gpio_set_value(escore->pdata->reset_gpio, 1);
	/* Wait 10 ms then */
	usleep_range(10000, 10000);
	/* eSxxx is READY */
#endif
}

int escore_probe(struct escore_priv *escore, struct device *dev, int curr_intf)
{
	int rc = 0;

	mutex_lock(&escore->api_mutex);

	escore->intf_probed |= curr_intf;

	if (escore->high_bw_intf == ES_UART_INTF && !escore->uart_ready) {
		pr_err("add uart codec name\n");
		rc = escore_uart_add_dummy_dev(escore);
		if (rc)
			pr_err("%s(): Adding UART dummy dev failed\n",
					__func__);
	}

	if (curr_intf == escore->pri_intf)
		escore->dev = dev;

	if (escore->intf_probed != (escore->pri_intf | escore->high_bw_intf)) {
		pr_debug("%s(): Both interfaces are not probed %d\n",
				__func__, escore->intf_probed);
		mutex_unlock(&escore->api_mutex);
		return 0;
	}
	mutex_unlock(&escore->api_mutex);

	if (escore->wakeup_intf == ES_UART_INTF && !escore->uart_ready) {
		pr_err("%s(): Wakeup mechanism not initialized\n", __func__);
		return 0;
	}

	escore->bus.setup_prim_intf(escore);

	rc = escore->bus.setup_high_bw_intf(escore);
	if (rc) {
		pr_err("%s(): Error while setting up high bw interface %d\n",
				__func__, rc);
		goto out;
	}

	if (escore->flag.is_codec) {
		rc = snd_soc_register_codec(escore->dev,
				escore->soc_codec_dev_escore,
				escore->dai,
				escore->dai_nr);

		if (rc) {
			pr_err("%s(): Codec registration failed %d\n",
					__func__, rc);
			goto out;
		}
	}

	/* Enable the gpiob IRQ */
//	if (escore_priv.pdata->gpiob_gpio != -1)
//		enable_irq(gpio_to_irq(escore_priv.pdata->gpiob_gpio));

out:
	return rc;
}
