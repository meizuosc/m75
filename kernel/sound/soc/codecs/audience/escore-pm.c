/*
 * Copyright 2014 Audience, Inc.
 *
 * Author: Steven Tarr  <starr@audience.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * For the time being, the default actions are those required for the
 * ES755 as decibed in "ES755 ENgineering API Guide" version 0.31
 */
#include <linux/kernel.h>
#include <linux/jiffies.h>
#include "escore.h"
#include "escore-uart-common.h"

#define ES_PM_SLEEP_DELAY		30 /* 30 ms */
#define ES_PM_CLOCK_STABILIZATION	1 /* 1ms */
#define ES_PM_RESUME_TIME		30 /* 30ms */
#define ES_PM_AUTOSUSPEND_DELAY		1000 /* 1 sec */

static int escore_vs_suspend(struct device *dev)
{
	dev_dbg(dev, "%s()\n", __func__);
	return 0;
}

static int escore_vs_resume(struct device *dev)
{
	dev_dbg(dev, "%s()\n", __func__);
	return 0;
}

static int escore_non_vs_suspend(struct device *dev)
{
	int ret = 0;
	int rsp;
	dev_dbg(dev, "%s()\n", __func__);
	if (escore_priv.non_vs_pm_state == ES_PM_ASLEEP) {
		dev_dbg(dev, "%s() - leaving sleeping dogs alone\n", __func__);
		goto suspend_out;
	}
	/* It is assumed that a channels have already been muted */
	/* Send a SetPowerState command - no respnse */
#if defined(CONFIG_SND_SOC_ES_WAKEUP_GPIO) || \
	defined(CONFIG_SND_SOC_ES_WAKEUP_UART)
	{
		u32 cmd = (ES_SET_POWER_STATE << 16) | ES_SET_POWER_STATE_SLEEP;
		ret = escore_priv.bus.ops.cmd(&escore_priv, cmd,
				&rsp);
		if (ret < 0) {
			dev_err(dev, "%s() - Chip dead.....\n", __func__);
			goto suspend_out;
		}
		/* Set delay time time */
		msleep(ES_PM_SLEEP_DELAY);

		/* Disable the clocks */
		if (escore_priv.pdata->esxxx_clk_cb)
			escore_priv.pdata->esxxx_clk_cb(0);
	}
#endif
suspend_out:
	escore_priv.non_vs_pm_state = ES_PM_ASLEEP;
	return ret;
}

static int escore_non_vs_resume(struct device *dev)
{
	u32 cmd = ES_SYNC_CMD << 16;
	u32 rsp;
	int ret = 0;
	dev_dbg(dev, "%s()\n", __func__);
	if (escore_priv.non_vs_pm_state == ES_PM_NORMAL) {
		dev_dbg(dev, "%s() - already awake\n", __func__);
		return 0;
	}

#if defined(CONFIG_SND_SOC_ES_WAKEUP_GPIO) || \
	defined(CONFIG_SND_SOC_ES_WAKEUP_UART)

	/* Enable the clocks */
	if (escore_priv.pdata->esxxx_clk_cb)
		escore_priv.pdata->esxxx_clk_cb(1);

	/* Setup for clock stablization delay */
	msleep(ES_PM_CLOCK_STABILIZATION);

	/* Toggle the wakeup pin H->L the L->H */
	if (escore_priv.pdata->wakeup_gpio != -1) {
#ifdef CONFIG_ARCH_MT6595
		mt_set_gpio_out(escore_priv.pdata->wakeup_gpio, 0);
		usleep_range(1000, 1000);
		mt_set_gpio_out(escore_priv.pdata->wakeup_gpio, 1);
#else
		gpio_set_value(escore_priv.pdata->wakeup_gpio, 0);
		usleep_range(1000, 1000);
		gpio_set_value(escore_priv.pdata->wakeup_gpio, 1);
#endif
	} else {
#ifdef CONFIG_SND_SOC_ES_WAKEUP_UART
		char wakeup_byte = 'A';
		ret = escore_uart_write(&escore_priv, &wakeup_byte, 1);
		if (ret < 0) {
			dev_err(dev, "%s() UART wakeup failed:%d\n",
				__func__, ret);
		/* Probably should do somethting like reset, but... */
		}

		/* Read an extra byte to flush UART read buffer. If this byte
		 * is not read, an extra byte is received in next UART read
		 * because of above write.
		 */
		ret = escore_uart_read(&escore_priv, &wakeup_byte, 1);
#else
		dev_err(dev, "%s() No wakeup mechanism\n", __func__);
#endif
	}
	/* Give the device time to "wakeup" */
	msleep(ES_PM_RESUME_TIME);
	/* Send a Sync command - not sure about the response */
	if (escore_priv.cmd_compl_mode == ES_CMD_COMP_INTR)
		cmd |= ES_RISING_EDGE;
	ret = escore_priv.bus.ops.cmd(&escore_priv, cmd, &rsp);
	if (ret < 0)
		dev_err(dev, "%s() - failed sync cmd resume\n", __func__);
	if (cmd != rsp)
		dev_err(dev, "%s() - failed sync rsp resume\n", __func__);
	escore_priv.non_vs_pm_state = ES_PM_NORMAL;
	dev_dbg(dev, "%s() - out rc =%d\n", __func__, ret);
#endif
	return ret;
}

static int escore_runtime_suspend(struct device *dev)
{
	int ret = 0;
	unsigned long time_left;
	struct escore_priv *escore = &escore_priv;

	dev_dbg(dev, "%s()\n", __func__);
	time_left = pm_runtime_autosuspend_expiration(escore->dev);
	if (time_left) {
		pm_runtime_mark_last_busy(escore->dev);
		dev_dbg(escore->dev, "%s() - delayed\n", __func__);
		return -EBUSY;
	}

	ret = escore_non_vs_suspend(dev);

	return ret;
}

static int escore_runtime_resume(struct device *dev)
{
	int ret = 0;
	struct escore_priv *escore = &escore_priv;
	struct device *p = dev->parent;

	dev_dbg(dev, "%s()\n", __func__);
	if (p && pm_runtime_status_suspended(p)) {
		dev_err(dev, "%s() - parent is suspended\n", __func__);
		return -EIO;
	}
	if (escore->pm_state == ES_PM_NORMAL) {
		dev_dbg(dev, "%s() - already awake\n", __func__);
		return 0;
	}
	ret = escore_non_vs_resume(dev);
	pm_runtime_mark_last_busy(escore->dev);
	dev_dbg(dev, "%s() complete %d\n", __func__, ret);

	return ret;
}

int escore_suspend(struct device *dev)
{
	int ret = 0;
	unsigned long time_left;
	struct escore_priv *escore = &escore_priv;

	dev_dbg(dev, "%s()\n", __func__);
	time_left = pm_runtime_autosuspend_expiration(escore->dev);
	if (time_left) {
		pm_runtime_mark_last_busy(escore->dev);
		dev_dbg(escore->dev, "%s() - delayed\n", __func__);
		return -EBUSY;
	}

	ret = escore_vs_suspend(dev);
	return ret;
}

int escore_resume(struct device *dev)
{
	int ret = 0;
	struct escore_priv *escore = &escore_priv;
	struct device *p = dev->parent;

	dev_dbg(dev, "%s()\n", __func__);
	if (p && pm_runtime_status_suspended(p)) {
		dev_err(dev, "%s() - parent is suspended\n", __func__);
		return -EIO;
	}
	if (escore->pm_state == ES_PM_NORMAL) {
		dev_dbg(dev, "%s() - already awake\n", __func__);
		return 0;
	}
	ret = escore_vs_resume(dev);
	pm_runtime_mark_last_busy(escore->dev);
	dev_dbg(dev, "%s() complete %d\n", __func__, ret);
	return ret;
}

const struct dev_pm_ops escore_pm_ops = {
	.suspend = escore_suspend,
	.resume = escore_resume,
	.runtime_suspend = escore_runtime_suspend,
	.runtime_resume = escore_runtime_resume,
};

void escore_pm_init(void)
{
	escore_priv.vs_pm_ops.suspend = escore_vs_suspend;
	escore_priv.vs_pm_ops.resume = escore_vs_resume;
	escore_priv.non_vs_pm_ops.suspend = escore_non_vs_suspend;
	escore_priv.non_vs_pm_ops.resume = escore_non_vs_resume;

	return;
}

void escore_pm_enable(void)
{
	dev_dbg(escore_priv.dev, "%s()\n", __func__);
	escore_priv.pm_enable = 1;
	pm_runtime_set_active(escore_priv.dev);
	pm_runtime_enable(escore_priv.dev);
	pm_runtime_set_autosuspend_delay(escore_priv.dev,
					ES_PM_AUTOSUSPEND_DELAY);
	pm_runtime_use_autosuspend(escore_priv.dev);
	return;
}

void escore_pm_disable(void)
{
	escore_priv.pm_enable = 0;
	pm_runtime_disable(escore_priv.dev);
	return;
}

void escore_pm_vs_enable(struct escore_priv *escore, bool value)
{
	struct device *xdev = escore->dev;
	dev_dbg(escore->dev, "%s()\n", __func__);
	if (xdev) {
		if (value == true)
			device_wakeup_enable(xdev);
		else
			device_wakeup_disable(xdev);
	}
	return;
}

int escore_pm_get_sync(void)
{
	dev_dbg(escore_priv.dev, "%s()\n", __func__);
	if (escore_priv.pm_enable == 1)
		return pm_runtime_get_sync(escore_priv.dev);
	else
		return 0;
}

void escore_pm_put_autosuspend(void)
{
	int ret = 0;
	dev_dbg(escore_priv.dev, "%s()\n", __func__);
	if (escore_priv.pm_enable == 1) {
		pm_runtime_mark_last_busy(escore_priv.dev);
		ret = pm_runtime_put_sync_autosuspend(escore_priv.dev);
		if (ret)
			dev_err(escore_priv.dev, "%s() - failed\n", __func__);
	}
	return;
}
