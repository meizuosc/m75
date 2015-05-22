/*
 * escore-vs.c  --  Audience Voice Sense component ALSA Audio driver
 *
 * Copyright 2013 Audience, Inc.
 *
 * Author: Greg Clemson <gclemson@audience.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "escore.h"
#include "escore-vs.h"

int escore_get_vs_wakeup_keyword(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct escore_voice_sense *voice_sense =
			(struct escore_voice_sense *) escore_priv.voice_sense;
	ucontrol->value.enumerated.item[0] = voice_sense->vs_wakeup_keyword;
	return 0;
}

int escore_put_vs_wakeup_keyword(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct escore_voice_sense *voice_sense =
			(struct escore_voice_sense *) escore_priv.voice_sense;
	voice_sense->vs_wakeup_keyword = ucontrol->value.enumerated.item[0];
	return 0;
}

/* Note: this may only end up being called in a api locked context. In
 * that case the mutex locks need to be removed.
 */
int escore_read_vs_data_block(struct escore_priv *escore)
{
	struct escore_voice_sense *voice_sense =
			(struct escore_voice_sense *) escore->voice_sense;
	/* This function is not re-entrant so avoid stack bloat. */
	u8 block[ES_VS_KEYWORD_PARAM_MAX];

	u32 cmd;
	u32 resp;
	int ret;
	unsigned size;
	unsigned rdcnt;

	mutex_lock(&escore->api_mutex);

	/* Read voice sense keyword data block request. */
	cmd = cpu_to_le32(ES_READ_DATA_BLOCK << 16 | ES_VS_DATA_BLOCK);
	escore->bus.ops.write(escore, (char *)&cmd, 4);

	usleep_range(5000, 5000);

	ret = escore->bus.ops.read(escore, (char *)&resp, 4);
	if (ret < 0) {
		dev_dbg(escore->dev, "%s(): error sending request = %d\n",
			__func__, ret);
		goto out;
	}

	le32_to_cpus(resp);
	size = resp & 0xffff;
	dev_dbg(escore->dev, "%s(): resp=0x%08x size=%d\n",
		__func__, resp, size);
	if ((resp & 0xffff0000) != (ES_READ_DATA_BLOCK << 16)) {
		dev_err(escore->dev,
			"%s(): invalid read vs data block response = 0x%08x\n",
			__func__, resp);
		goto out;
	}

	BUG_ON(size == 0);
	BUG_ON(size > ES_VS_KEYWORD_PARAM_MAX);
	BUG_ON(size % 4 != 0);

	/* This assumes we need to transfer the block in 4 byte
	 * increments. This is true on slimbus, but may not hold true
	 * for other buses.
	 */
	for (rdcnt = 0; rdcnt < size; rdcnt += 4) {
		ret = escore->bus.ops.read(escore, (char *)&resp, 4);
		if (ret < 0) {
			dev_dbg(escore->dev,
				"%s(): error reading data block at %d bytes\n",
				__func__, rdcnt);
			goto out;
		}
		memcpy(&block[rdcnt], &resp, 4);
	}

	memcpy(voice_sense->vs_keyword_param, block, rdcnt);
	voice_sense->vs_keyword_param_size = rdcnt;
	dev_dbg(escore->dev, "%s(): stored v-s keyword block of %d bytes\n",
		__func__, rdcnt);

out:
	mutex_unlock(&escore->api_mutex);
	if (ret)
		dev_err(escore->dev, "%s(): v-s read data block failure=%d\n",
			__func__, ret);
	return ret;
}

int escore_write_vs_data_block(struct escore_priv *escore)
{
	u32 cmd;
	u32 resp;
	int ret;
	u8 *dptr;
	u16 rem;
	u8 wdb[4];
	struct escore_voice_sense *voice_sense =
			(struct escore_voice_sense *) escore->voice_sense;

	if (voice_sense->vs_keyword_param_size == 0) {
		dev_warn(escore->dev,
			"%s(): attempt to write empty keyword data block\n",
			__func__);
		return -ENOENT;
	}

	BUG_ON(voice_sense->vs_keyword_param_size % 4 != 0);

	mutex_lock(&escore->api_mutex);

	cmd = (ES_WRITE_DATA_BLOCK << 16) |
		(voice_sense->vs_keyword_param_size & 0xffff);
	cmd = cpu_to_le32(cmd);
	ret = escore->bus.ops.write(escore, (char *)&cmd, 4);
	if (ret < 0) {
		dev_err(escore->dev,
			"%s(): error writing cmd 0x%08x to device\n",
			__func__, cmd);
		goto EXIT;
	}

	usleep_range(10000, 10000);
	ret = escore->bus.ops.read(escore, (char *)&resp, 4);
	if (ret < 0) {
		dev_dbg(escore->dev, "%s(): error sending request = %d\n",
			__func__, ret);
		goto EXIT;
	}

	le32_to_cpus(resp);
	dev_dbg(escore->dev, "%s(): resp=0x%08x\n", __func__, resp);
	if ((resp & 0xffff0000) != (ES_WRITE_DATA_BLOCK << 16)) {
		dev_err(escore->dev, "%s(): invalid write data block 0x%08x\n",
			__func__, resp);
		goto EXIT;
	}

	dptr = voice_sense->vs_keyword_param;
	for (rem = voice_sense->vs_keyword_param_size; rem > 0;
					rem -= 4, dptr += 4) {
		wdb[0] = dptr[3];
		wdb[1] = dptr[2];
		wdb[2] = dptr[1];
		wdb[3] = dptr[0];
		ret = escore->bus.ops.write(escore, (char *)wdb, 4);
		if (ret < 0) {
			dev_err(escore->dev, "%s(): v-s wdb error offset=%hu\n",
			    __func__, dptr - voice_sense->vs_keyword_param);
			goto EXIT;
		}
	}

	usleep_range(10000, 10000);
	memset(&resp, 0, 4);

	ret = escore->bus.ops.read(escore, (char *)&resp, 4);
	if (ret < 0) {
		dev_dbg(escore->dev, "%s(): error sending request = %d\n",
			__func__, ret);
		goto EXIT;
	}

	le32_to_cpus(resp);
	dev_dbg(escore->dev, "%s(): resp=0x%08x\n", __func__, resp);
	if (resp & 0xffff) {
		dev_err(escore->dev, "%s(): write data block error 0x%08x\n",
			__func__, resp);
		goto EXIT;
	}

	dev_info(escore->dev, "%s(): v-s wdb success\n", __func__);
EXIT:
	mutex_unlock(&escore->api_mutex);
	if (ret != 0)
		dev_err(escore->dev, "%s(): v-s wdb failed ret=%d\n",
			__func__, ret);
	return ret;
}

int escore_put_vs_stored_keyword(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	int op;
	int ret;
	struct escore_voice_sense *voice_sense =
			(struct escore_voice_sense *) escore_priv.voice_sense;

	op = ucontrol->value.enumerated.item[0];
	dev_dbg(escore_priv.dev, "%s(): op=%d\n", __func__, op);

	ret = 0;
	switch (op) {
	case 0:
		dev_dbg(escore_priv.dev, "%s(): keyword params put...\n",
			__func__);
		ret = escore_write_vs_data_block(&escore_priv);
		break;
	case 1:
		dev_dbg(escore_priv.dev, "%s(): keyword params get...\n",
			__func__);
		ret = escore_read_vs_data_block(&escore_priv);
		break;
	case 2:
		dev_dbg(escore_priv.dev, "%s(): keyword params clear...\n",
			__func__);
		voice_sense->vs_keyword_param_size = 0;
		break;
	default:
		BUG_ON(0);
	};

	return ret;
}

int escore_put_cvs_preset_value(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct escore_voice_sense *voice_sense =
			(struct escore_voice_sense *) escore_priv.voice_sense;
	unsigned int value;
	int rc = 0;

	value = ucontrol->value.integer.value[0];

	rc = escore_put_control_value(kcontrol, ucontrol);

	if (!rc)
		voice_sense->cvs_preset = value;

	return rc;
}

int escore_get_cvs_preset_value(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct escore_voice_sense *voice_sense =
			(struct escore_voice_sense *) escore_priv.voice_sense;
	ucontrol->value.integer.value[0] = voice_sense->cvs_preset;

	return 0;
}

static ssize_t escore_vs_status_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	int ret = 0;
	unsigned int value = 0;
	char *status_name = "Voice Sense Status";
	/* Disable vs status read for interrupt to work */
	struct escore_priv *escore = &escore_priv;
	struct escore_voice_sense *voice_sense =
			(struct escore_voice_sense *) escore->voice_sense;

	mutex_lock(&voice_sense->vs_event_mutex);

	value = voice_sense->vs_get_event;
	/* Reset the detection status after read */
	voice_sense->vs_get_event = ES_NO_EVENT;

	mutex_unlock(&voice_sense->vs_event_mutex);

	ret = snprintf(buf, PAGE_SIZE, "%s=0x%04x\n", status_name, value);

	return ret;
}

static DEVICE_ATTR(vs_status, 0444, escore_vs_status_show, NULL);

static struct attribute *vscore_sysfs_attrs[] = {
	&dev_attr_vs_status.attr,
	NULL
};

static struct attribute_group vscore_sysfs = {
	.attrs = vscore_sysfs_attrs
};

int escore_vs_request_firmware(struct escore_priv *escore,
				const char *vs_filename)
{
	struct escore_voice_sense *voice_sense =
			(struct escore_voice_sense *) escore->voice_sense;

	return request_firmware((const struct firmware **)&voice_sense->vs,
			      vs_filename, escore->dev);
}

void escore_vs_release_firmware(struct escore_priv *escore)
{
	struct escore_voice_sense *voice_sense =
			(struct escore_voice_sense *) escore->voice_sense;

	release_firmware(voice_sense->vs);
}

static int escore_vs_isr(struct notifier_block *self, unsigned long action,
		void *dev)
{
	struct escore_priv *escore = (struct escore_priv *)dev;
	struct escore_voice_sense *voice_sense =
		(struct escore_voice_sense *) escore->voice_sense;
	u32 smooth_mute = ES_SET_SMOOTH_MUTE << 16 | ES_SMOOTH_MUTE_ZERO;
	u32 es_set_power_level = ES_SET_POWER_LEVEL << 16 | ES_POWER_LEVEL_6;
	u32 resp;
	int rc = 0;

	dev_dbg(escore->dev, "%s(): Event: 0x%04x\n", __func__, (u32)action);

	if ((action & 0xFF) != ES_VS_INTR_EVENT) {
		dev_err(escore->dev, "%s(): Invalid event callback 0x%04x\n",
				__func__, (u32) action);
		return NOTIFY_DONE;
	}
	dev_info(escore->dev, "%s(): VS event detected 0x%04x\n",
				__func__, (u32) action);

	if (voice_sense->cvs_preset != 0xFFFF && voice_sense->cvs_preset != 0) {
		escore->escore_power_state = ES_SET_POWER_STATE_NORMAL;
		escore->vs_pm_state = ES_PM_NORMAL;
		escore->non_vs_pm_state = ES_PM_NORMAL;
		escore->mode = STANDARD;
	}

	mutex_lock(&voice_sense->vs_event_mutex);
	voice_sense->vs_get_event = action;
	mutex_unlock(&voice_sense->vs_event_mutex);

	/* If CVS preset is set (other than 0xFFFF), earSmart chip is
	 * in CVS mode. To make it switch from internal to external
	 * oscillator, send power level command with highest power
	 * level
	 */
	if (voice_sense->cvs_preset != 0xFFFF &&
			voice_sense->cvs_preset != 0) {

		rc = escore_cmd(escore, smooth_mute, &resp);
		if (rc < 0) {
			pr_err("%s(): Error setting smooth mute\n", __func__);
			goto voiceq_isr_exit;
		}
		usleep_range(2000, 2005);
		rc = escore_cmd(escore, es_set_power_level, &resp);
		if (rc < 0) {
			pr_err("%s(): Error setting power level\n", __func__);
			goto voiceq_isr_exit;
		}
		usleep_range(2000, 2005);

		/* Each time earSmart chip comes in BOSKO mode after
		 * VS detect, CVS mode will be disabled */
		voice_sense->cvs_preset = 0;
	}
	kobject_uevent(&escore->dev->kobj, KOBJ_CHANGE);

	return NOTIFY_OK;

voiceq_isr_exit:
	return NOTIFY_DONE;
}

static struct notifier_block escore_vs_intr_cb = {
	.notifier_call = escore_vs_isr,
	.priority = ES_NORMAL,
};

void escore_vs_init_intr(struct escore_priv *escore)
{
	escore_register_notify(escore_priv.irq_notifier_list,
			&escore_vs_intr_cb);
	((struct escore_voice_sense *)escore->voice_sense)->vs_irq = true;
}

int escore_vs_load(struct escore_priv *escore)
{
	struct escore_voice_sense *voice_sense =
			(struct escore_voice_sense *) escore->voice_sense;
	int rc = 0;
	int write_size = ES_VS_FW_CHUNK;
	int data_remaining = voice_sense->vs->size;
	u32 sync_cmd = (ES_EVENT_RESPONSE_CMD << 16) | ES_SYNC_INTR_RISING_EDGE;
	u32 resp;

	BUG_ON(voice_sense->vs->size == 0);

	if (!escore->boot_ops.setup || !escore->boot_ops.finish) {
		dev_err(escore->dev,
			"%s(): boot setup or finish function undefined\n",
			__func__);
		rc = -EIO;
		goto escore_vs_fw_download_failed;
	}

	rc = escore->boot_ops.setup(escore);
	if (rc) {
		dev_err(escore->dev, "%s(): fw download start error\n",
			__func__);
		goto escore_vs_fw_download_failed;
	}

	dev_dbg(escore->dev, "%s(): write vs firmware image\n", __func__);

	while (data_remaining) {
		rc = escore->bus.ops.high_bw_write(escore,
			((char *)voice_sense->vs->data)
			+ (voice_sense->vs->size - data_remaining), write_size);

		data_remaining -= write_size;

		dev_dbg(escore->dev,
			"data_remaining = %d, write_size = %d\n",
			data_remaining, write_size);

		if (rc) {
			dev_err(escore->dev,
				"%s(): vs firmware image write error\n",
				__func__);
			rc = -EIO;
			goto escore_vs_fw_download_failed;
		} else if (data_remaining < write_size) {
			write_size = data_remaining;
		}

		usleep_range(2000, 2000);
	}

	escore->mode = VOICESENSE;

	if (((struct escore_voice_sense *)escore->voice_sense)->vs_irq != true)
		escore_vs_init_intr(escore);

	rc = escore->boot_ops.finish(escore);
	if (rc) {
		dev_err(escore->dev, "%s() vs fw download finish error\n",
			__func__);
		goto escore_vs_fw_download_failed;
	} else {
		/* Enable Voice Sense Event INTR to Host */
		rc = escore_cmd(escore, sync_cmd, &resp);
		if (rc)
			dev_err(escore->dev, "%s(): escore_cmd fail %d\n",
								__func__, rc);

		if (resp != sync_cmd) {
			dev_err(escore->dev,
				"%s(): Enable VS Event INTR fail\n", __func__);
			goto escore_vs_fw_download_failed;
		}
	}

	dev_dbg(escore->dev, "%s(): fw download done\n", __func__);

escore_vs_fw_download_failed:
	return rc;
}

int escore_vs_init_sysfs(struct escore_priv *escore)
{
	return sysfs_create_group(&escore->dev->kobj, &vscore_sysfs);
}

int escore_vs_init(struct escore_priv *escore)
{
	int rc = 0;

	struct escore_voice_sense *voice_sense;
	voice_sense = (struct escore_voice_sense *)
			kmalloc(sizeof(struct escore_voice_sense), GFP_KERNEL);
	if (!voice_sense) {
		rc = -ENOMEM;
		goto voice_sense_alloc_err;
	}

	escore->voice_sense = (void *)voice_sense;

	/* Initialize variables */
	voice_sense->cvs_preset = 0;

	voice_sense->vs = (struct firmware *)
			kmalloc(sizeof(struct firmware), GFP_KERNEL);
	if (!voice_sense->vs) {
		rc = -ENOMEM;
		goto vs_alloc_err;
	}

	mutex_init(&voice_sense->vs_event_mutex);

	rc = escore_vs_init_sysfs(escore);
	if (rc) {
		dev_err(escore_priv.dev,
			"failed to create core sysfs entries: %d\n", rc);
	}
	return rc;

vs_alloc_err:
	kfree(voice_sense);
voice_sense_alloc_err:
	return rc;
}
