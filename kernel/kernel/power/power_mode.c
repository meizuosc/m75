/*
 * kernel/power/power_mode.c
 *
 * add /sys/power/power_mode for set save mode mode .
 *
 * Copyright (C) 2014 chenshb. chenshb <chenshb@meizu.com>
 */

#include <linux/device.h>
#include <linux/mutex.h>
#include "power.h"
#include <linux/power_mode.h>

struct power_mode_info *cur_power_mode;
unsigned int cpu_power_mode;
struct power_mode_info power_mode[POWER_MODE_END] = {
	/* name, big_freq, little_freq, gpu_freq, little_num, big_num, little_turbo, big_turbo, turbo_temp */
	{ "low",   1690000, 1690000, 396500, 4, 0, 0, 0, 47},
	{ "normal",1937000, 1690000, 396500, 4, 2, 1, 0, 50},
	{ "high",  2210000, 1690000, 598000, 4, 4, 1, 1, 57},
};

/*add power mode notify service*/
static BLOCKING_NOTIFIER_HEAD(power_mode_notifier_list);

int power_mode_register_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&power_mode_notifier_list, nb);
}
EXPORT_SYMBOL(power_mode_register_notifier);

int power_mode_unregister_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&power_mode_notifier_list, nb);
}
EXPORT_SYMBOL(power_mode_unregister_notifier);

int power_mode_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&power_mode_notifier_list, val, v);
}
EXPORT_SYMBOL_GPL(power_mode_notifier_call_chain);



void show_power_mode_list(void)
{
	int i;

	/* Show Exynos Power mode list */
	pr_err("M75 Power mode List\n");
	pr_info("name: big_freq, little_freq, gpu_freq, little_num, big_num, little_turbo, big_turbo, turbo_temp\n");
	for (i = 0; i < POWER_MODE_END; i++ ) {
		pr_err("%s: %d, %d, %d, %d, %d, %d, %d, %d\n", power_mode[i].mode_name,
			power_mode[i].cpu_freq_big, power_mode[i].cpu_freq_little,
			power_mode[i].gpu_freq_lock, power_mode[i].little_num, power_mode[i].big_num,
			power_mode[i].little_turbo, power_mode[i].big_turbo, power_mode[i].turbo_temp);
	}
}

ssize_t power_mode_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int ret;
	ret = snprintf(buf, POWER_MODE_LEN, "%s\n", cur_power_mode->mode_name);
	show_power_mode_list();
	return ret;
}

ssize_t power_mode_store(struct kobject *kobj, struct attribute *attr,const char *buf, size_t count)
{
	char str_power_mode[POWER_MODE_LEN];
	int ret , i;

	ret = sscanf(buf, "%8s", str_power_mode);
	if (ret != 1)
		return -EINVAL;

	for (i = 0; i < POWER_MODE_END; i++) {
		if (!strnicmp(power_mode[i].mode_name, str_power_mode, POWER_MODE_LEN)) {
			break;
		}
	}

	if ( i <  POWER_MODE_END) {
		cpu_power_mode = i;
		cur_power_mode = &power_mode[i];
		power_mode_notifier_call_chain(cpu_power_mode, cur_power_mode);
		pr_err("store power_mode to %s \n", cur_power_mode->mode_name);
	}

	return count;
}

int power_mode_init(void)
{
	pr_err("M75 Power mode init, default POWER_MODE_1\n");
	cpu_power_mode = POWER_MODE_1;
	cur_power_mode = &power_mode[cpu_power_mode];
	show_power_mode_list();
	return 0;
}

