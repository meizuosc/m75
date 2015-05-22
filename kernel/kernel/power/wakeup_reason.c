/*
 * kernel/power/wakeup_reason.c
 *
 * Logs the reasons which caused the kernel to resume from
 * the suspend mode.
 *
 * Copyright (C) 2014 Google, Inc.
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/wakeup_reason.h>
#include <linux/kernel.h>
#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/suspend.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include "power.h"

extern int wakeup_reason_stats_show(struct seq_file *m, void *unused);

static int proc_wakeup_reason_show(struct seq_file *p, void *unused)
{
	return wakeup_reason_stats_show(p, NULL);
}

static int proc_wakeup_reason_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_wakeup_reason_show, NULL);
}

static const struct file_operations proc_wakeup_reason_operations = {
	.open		= proc_wakeup_reason_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

int __init wakeup_reason_init(void)
{
	int error;
	error = proc_create("wakeup_reason", S_IFREG | S_IRUGO, NULL, &proc_wakeup_reason_operations);
	return error;
}

late_initcall(wakeup_reason_init);
