/*
 * Copyright (c) 2014 Meizu Co., Ltd.
 *		http://www.meizu.com
 *
 * Core file for DSI Dynamic Freqency driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/irq.h>
#include <linux/mm.h>
#include <linux/fb.h>
#include <linux/memory.h>
#include <linux/kobject.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/sysfs.h>

#include "mtkfb.h"
#include "ddp_drv.h"
#include "ddp_manager.h"
#include "ddp_dsi.h"
#include "primary_display.h"


#ifdef CONFIG_HAS_WAKELOCK
#include <linux/wakelock.h>
#include <linux/earlysuspend.h>
#include <linux/suspend.h>
#endif

struct ddp_dvfs_info
{
#ifdef CONFIG_HAS_WAKELOCK
	struct early_suspend	early_suspend;
#endif
	spinlock_t dvfs_lock;

	int dsi_dvfs;
	int dsi_nm_freq;
	int dsi_lp_freq;
	int dsi_cur_freq;
	bool suspend;
};
static struct ddp_dvfs_info *g_ddp_dvfs_info;

/**************************************
 * Macro and Inline
 **************************************/
#define DEFINE_ATTR_RO(_name)			\
static struct kobj_attribute _name##_attr = {	\
	.attr	= {				\
		.name = #_name,			\
		.mode = 0444,			\
	},					\
	.show	= _name##_show,			\
}

#define DEFINE_ATTR_RW(_name)			\
static struct kobj_attribute _name##_attr = {	\
	.attr	= {				\
		.name = #_name,			\
		.mode = 0644,			\
	},					\
	.show	= _name##_show,			\
	.store	= _name##_store,		\
}

#define __ATTR_OF(_name)	(&_name##_attr.attr)
/**************************************
 * vcore_debug_xxx Function
 **************************************/
static ssize_t ddp_dvfs_debug_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	char *p = buf;

	p += sprintf(p, "Dvfs state = %d\n" , g_ddp_dvfs_info->dsi_dvfs);
	p += sprintf(p, "Normal freq = %d\n", g_ddp_dvfs_info->dsi_nm_freq);
	p += sprintf(p, "Lowpower freq = %d\n", g_ddp_dvfs_info->dsi_lp_freq);
	p += sprintf(p, "Current freq = %d\n", g_ddp_dvfs_info->dsi_cur_freq);
	p += sprintf(p, "\n Command:\n");
	p += sprintf(p, "Set Freq:\n");
	p += sprintf(p, "  lp_freq x:\n");
	p += sprintf(p, "Enable/Disable:\n");
	p += sprintf(p, "  dsi_dvfs x:\n");

	
	BUG_ON(p - buf >= PAGE_SIZE);
	return p - buf;
}

static ssize_t ddp_dvfs_debug_store(struct kobject *kobj, struct kobj_attribute *attr,
				 const char *buf, size_t count)
{
	int r;
	u32 val;
	char cmd[32];
	struct ddp_dvfs_info *info = g_ddp_dvfs_info;

	if (sscanf(buf, "%31s %d", cmd, &val) != 2)
		return -EPERM;

	pr_debug("ddp_dvfs_debug: cmd = %s, val = 0x%x\n", cmd, val);

	if (!strcmp(cmd, "lp_freq") && (val < 500 && val > 50)) {
		info->dsi_lp_freq = val;
	} else if (!strcmp(cmd, "dsi_dvfs")) {
		info->dsi_dvfs = !!val;
	} else {
		return -EINVAL;
	}

	return count;
}


/**************************************
 * Init Function
 **************************************/
DEFINE_ATTR_RW(ddp_dvfs_debug);

static struct attribute *ddp_dvfs_attrs[] = {
	__ATTR_OF(ddp_dvfs_debug),
	NULL,
};

static struct attribute_group ddp_dvfs_attr_group = {
	.name	= "ddp_dvfs",
	.attrs	= ddp_dvfs_attrs,
};

void ddp_dvfs_set_lcd_freq(bool high)
{
	struct ddp_dvfs_info *info = g_ddp_dvfs_info;
	int dsi_cur_freq;

	if(info && info->dsi_dvfs && !info->suspend)
	{
		spin_lock(&info->dvfs_lock);
		dsi_cur_freq = high ? info->dsi_nm_freq : info->dsi_lp_freq;
		if(dsi_cur_freq != info->dsi_cur_freq){
			info->dsi_cur_freq = dsi_cur_freq;
			DSI_set_VFP_timing(DISP_MODULE_DSI0,  info->dsi_cur_freq);
		}
		spin_unlock(&info->dvfs_lock);
	}
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void ddp_dvfs_early_suspend(struct early_suspend *h)
{
	struct ddp_dvfs_info *info = container_of(h, struct ddp_dvfs_info, early_suspend);

	info->suspend = true;
	pr_debug("%s\n", __func__);
	return;
}

static void ddp_dvfs_late_resume(struct early_suspend *h)
{
	struct ddp_dvfs_info *info = container_of(h, struct ddp_dvfs_info, early_suspend);

	pr_debug("%s\n", __func__);
	info->suspend = false;
	return;
}
#endif /* CONFIG_HAS_EARLYSUSPEND */

static int ddp_dvfs_init(void)
{
	int ret;
	struct ddp_dvfs_info *info;

	info = kzalloc(sizeof(struct ddp_dvfs_info), GFP_KERNEL);
	if (!info) {
		pr_err("failed to allocate for global ddp_dvfs_info structure\n");
		return -1;
	}

	spin_lock_init(&info->dvfs_lock);
	info->dsi_nm_freq = DSI_get_VFP_timing(DISP_MODULE_DSI0);
	info->dsi_lp_freq = 0x8c;
	info->dsi_dvfs = 1;
	info->suspend = false;

	ret = sysfs_create_group(power_kobj, &ddp_dvfs_attr_group);
	if (ret){
		pr_err("FAILED TO CREATE /sys/power/dsi_dvfs (%d)\n", ret);
		kfree(info);
		return -1;
	}
#ifdef CONFIG_HAS_WAKELOCK
#ifdef CONFIG_HAS_EARLYSUSPEND
	info->early_suspend.suspend = ddp_dvfs_early_suspend;
	info->early_suspend.resume = ddp_dvfs_late_resume;
	info->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN-15;

	register_early_suspend(&info->early_suspend);
#endif
#endif

	g_ddp_dvfs_info = info;
	return 0;
}
static void ddp_dvfs_remove(void)
{
#ifdef CONFIG_HAS_WAKELOCK
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&g_ddp_dvfs_info->early_suspend);
#endif
#endif
	kfree(g_ddp_dvfs_info);
}

late_initcall(ddp_dvfs_init);
module_exit(ddp_dvfs_remove);
