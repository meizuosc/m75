#ifndef POWER_MODE
#define POWER_MODE
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pm_qos.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/cpu.h>
#include <linux/delay.h>
#include <linux/kobject.h>
#include <linux/cpufreq.h>
#include <linux/notifier.h>
#endif

#define POWER_MODE_LEN	(8)

struct power_mode_info {
	char		*mode_name;
	unsigned int	cpu_freq_big;
	unsigned int	cpu_freq_little;
	unsigned int	gpu_freq_lock;
	unsigned int	little_num;
	unsigned int	big_num;
	unsigned int	little_turbo;
	unsigned int	big_turbo;
	unsigned int	turbo_temp;
};

enum power_mode_idx {
	POWER_MODE_0,
	POWER_MODE_1,
	POWER_MODE_2,
	POWER_MODE_END,
};

int power_mode_init(void);
void show_power_mode_list(void);
ssize_t power_mode_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
ssize_t power_mode_store(struct kobject *kobj, struct attribute *attr,const char *buf, size_t count);

#ifdef CONFIG_POWER_MODE
int power_mode_register_notifier(struct notifier_block *nb);
int power_mode_unregister_notifier(struct notifier_block *nb);
int power_mode_notifier_call_chain(unsigned long val, void *v);
extern unsigned int cpu_power_mode;
#else
#define power_mode_register_notifier(nb)	do { } while (0)
#define power_mode_unregister_notifier(nb)	do { } while (0)
#define power_mode_notifier_call_chain(val, v)	do { } while (0)
#endif


