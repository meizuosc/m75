
//#include <asm/page.h>
#include <linux/slab.h>
#include <linux/version.h>

#include "interface.h"
#include "trace.h"
#include "cpu_pmu.h"
#include "met_drv.h"

struct metdevice met_cpupmu;
struct cpu_pmu_hw *cpu_pmu;
static int nr_counters;

static struct kobject *kobj_cpu;
static struct met_pmu *pmu;
static int nr_arg;

static inline struct met_pmu *lookup_pmu(struct kobject *kobj)
{
	int i;
	for (i = 0; i < nr_counters; i++) {
		if (pmu[i].kobj_cpu_pmu == kobj)
			return &pmu[i];
	}
	return NULL;
}

static ssize_t count_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", nr_counters - 1);
}

static ssize_t count_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t n)
{
	return -EINVAL;
}

static ssize_t event_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct met_pmu *p = lookup_pmu(kobj);
	if (p != NULL)
		return snprintf(buf, PAGE_SIZE, "0x%hx\n", p->event);

	return -EINVAL;
}

static ssize_t event_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t n)
{
	struct met_pmu *p = lookup_pmu(kobj);
	unsigned short event;

	if (p != NULL) {
		if (sscanf(buf, "0x%hx", &event) != 1)
			return -EINVAL;

		if (p == &(pmu[nr_counters - 1])) {	/* cycle counter */
			if (event != 0xff)
				return -EINVAL;
		} else {
			if (cpu_pmu->check_event(pmu, nr_arg, event) < 0)
				return -EINVAL;
		}

		p->event = event;
		return n;
	}
	return -EINVAL;
}

static ssize_t mode_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct met_pmu *p = lookup_pmu(kobj);
	if (p != NULL) {
		switch (p->mode) {
		case 0:
			return snprintf(buf, PAGE_SIZE, "%hhd (disabled)\n", p->mode);
		case 1:
			return snprintf(buf, PAGE_SIZE, "%hhd (interrupt)\n", p->mode);
		case 2:
			return snprintf(buf, PAGE_SIZE, "%hhd (polling)\n", p->mode);
		}
	}
	return -EINVAL;
}

static ssize_t mode_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t n)
{
	unsigned char mode;
	struct met_pmu *p = lookup_pmu(kobj);
	if (p != NULL) {
		if (sscanf(buf, "%hhd", &mode) != 1)
			return -EINVAL;

		if (mode <= 2) {
			p->mode = mode;
			if (mode > 0)
				met_cpupmu.mode = 1;
			return n;
		}
	}
	return -EINVAL;
}

static ssize_t freq_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct met_pmu *p = lookup_pmu(kobj);
	if (p != NULL)
		return snprintf(buf, PAGE_SIZE, "%ld\n", p->freq);

	return -EINVAL;
}

static ssize_t freq_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t n)
{
	struct met_pmu *p = lookup_pmu(kobj);
	if (p != NULL) {
		if (sscanf(buf, "%ld", &(p->freq)) != 1)
			return -EINVAL;

		return n;
	}
	return -EINVAL;
}

static struct kobj_attribute count_attr = __ATTR(count, 0644, count_show, count_store);
static struct kobj_attribute event_attr = __ATTR(event, 0644, event_show, event_store);
static struct kobj_attribute mode_attr = __ATTR(mode, 0644, mode_show, mode_store);
static struct kobj_attribute freq_attr = __ATTR(freq, 0644, freq_show, freq_store);

static int cpupmu_create_subfs(struct kobject *parent)
{
	int ret = 0;
	int i;
	char buf[16];

	cpu_pmu = cpu_pmu_hw_init();
	if (cpu_pmu == NULL) {
		pr_err("Failed to init CPU PMU HW!!\n");
		return -ENODEV;
	}
	nr_counters = cpu_pmu->nr_cnt;

	pmu = (struct met_pmu *)kzalloc(sizeof(struct met_pmu) * nr_counters, GFP_KERNEL);
	if (pmu == NULL) {
		pr_err("can not create kobject: kobj_cpu\n");
		return -ENOMEM;
	}

	kobj_cpu = parent;

	ret = sysfs_create_file(kobj_cpu, &count_attr.attr);
	if (ret != 0) {
		pr_err("Failed to create count in sysfs\n");
		goto out;
	}

	for (i = 0; i < nr_counters; i++) {
		snprintf(buf, sizeof(buf), "%d", i);
		pmu[i].kobj_cpu_pmu = kobject_create_and_add(buf, kobj_cpu);

		ret = sysfs_create_file(pmu[i].kobj_cpu_pmu, &event_attr.attr);
		if (ret != 0) {
			pr_err("Failed to create event in sysfs\n");
			goto out;
		}

		ret = sysfs_create_file(pmu[i].kobj_cpu_pmu, &mode_attr.attr);
		if (ret != 0) {
			pr_err("Failed to create mode in sysfs\n");
			goto out;
		}

		ret = sysfs_create_file(pmu[i].kobj_cpu_pmu, &freq_attr.attr);
		if (ret != 0) {
			pr_err("Failed to create freq in sysfs\n");
			goto out;
		}
	}

 out:
	if (ret != 0) {
		if (pmu != NULL) {
			kfree(pmu);
			pmu = NULL;
		}
	}
	return ret;
}

static void cpupmu_delete_subfs(void)
{
	int i;

	if (kobj_cpu != NULL) {
		for (i = 0; i < nr_counters; i++) {
			sysfs_remove_file(pmu[i].kobj_cpu_pmu, &event_attr.attr);
			sysfs_remove_file(pmu[i].kobj_cpu_pmu, &mode_attr.attr);
			sysfs_remove_file(pmu[i].kobj_cpu_pmu, &freq_attr.attr);
			kobject_del(pmu[i].kobj_cpu_pmu);
			kobject_put(pmu[i].kobj_cpu_pmu);
			pmu[i].kobj_cpu_pmu = NULL;
		}
		sysfs_remove_file(kobj_cpu, &count_attr.attr);
		kobj_cpu = NULL;
	}

	if (pmu != NULL) {
		kfree(pmu);
		pmu = NULL;
	}

	cpu_pmu  = NULL;
}

void mp_cpu(unsigned char cnt, unsigned int *value)
{
	switch (cnt) {
	case 1:
		MET_PRINTK(MP_FMT1, MP_VAL1);
		break;
	case 2:
		MET_PRINTK(MP_FMT2, MP_VAL2);
		break;
	case 3:
		MET_PRINTK(MP_FMT3, MP_VAL3);
		break;
	case 4:
		MET_PRINTK(MP_FMT4, MP_VAL4);
		break;
	case 5:
		MET_PRINTK(MP_FMT5, MP_VAL5);
		break;
	case 6:
		MET_PRINTK(MP_FMT6, MP_VAL6);
		break;
	case 7:
		MET_PRINTK(MP_FMT7, MP_VAL7);
		break;
	case 8:
		MET_PRINTK(MP_FMT8, MP_VAL8);
		break;
	case 9:
		MET_PRINTK(MP_FMT9, MP_VAL9);
		break;
	}
}

/* #define USE_KERNEL_PMUPERF */
#ifdef USE_KERNEL_PMUPERF
#include <linux/perf_event.h>
#define CNTMAX 8

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 1, 0)
static void dummy_handler(struct perf_event *event, int unused, struct perf_sample_data *data,
			  struct pt_regs *regs)
#else
static void dummy_handler(struct perf_event *event, struct perf_sample_data *data,
			  struct pt_regs *regs)
#endif
{
/* Required as perf_event_create_kernel_counter() requires an overflow handler, even though all we do is poll */
}

static DEFINE_PER_CPU(int[CNTMAX], perfCurr);
static DEFINE_PER_CPU(int[CNTMAX], perfPrev);
static DEFINE_PER_CPU(int[CNTMAX], perfPrevDelta);
static DEFINE_PER_CPU(struct perf_event * [CNTMAX], pevent);
static DEFINE_PER_CPU(struct perf_event_attr * [CNTMAX], pevent_attr);

static void cpupmu_polling(unsigned long long stamp, int cpu)
{
	int i, count, delta;
	struct perf_event *ev;
	unsigned int pmu_value[8];

	memset(pmu_value, 0, sizeof(pmu_value));
	count = 0;
	for (i = 0; i < nr_counters; i++) {

		if (pmu[i].mode == 0)
			continue;

		ev = per_cpu(pevent, cpu)[i];

		if (ev != NULL && ev->state == PERF_EVENT_STATE_ACTIVE) {
			ev->pmu->read(ev);
			per_cpu(perfCurr, cpu)[i] = local64_read(&ev->count);
			delta = per_cpu(perfCurr, cpu)[i] - per_cpu(perfPrev, cpu)[i];
			if (delta != 0 || delta != per_cpu(perfPrevDelta, cpu)[i]) {
				per_cpu(perfPrevDelta, cpu)[i] = delta;
				per_cpu(perfPrev, cpu)[i] = per_cpu(perfCurr, cpu)[i];
				if (delta < 0)
					delta *= -1;
				pmu_value[count] = delta;
				count++;
			}
		}
	}

	mp_cpu(count, pmu_value);
	return;
}

static void cpupmu_stop(void)
{
	unsigned int i, cpu;
	for_each_present_cpu(cpu) {
		for (i = 0; i < nr_counters; i++) {
			if (per_cpu(pevent_attr, cpu)[i]) {
				kfree(per_cpu(pevent_attr, cpu)[i]);
				per_cpu(pevent_attr, cpu)[i] = NULL;
			}
		}
	}
}

static void cpupmu_start(void)
{
	int i, cpu, size;
	struct perf_event *ev;

	nr_arg = 0;
	size = sizeof(struct perf_event_attr);

	for_each_online_cpu(cpu) {
		for (i = 0; i < nr_counters; i++) {
			per_cpu(pevent, cpu)[i] = NULL;
			if (!pmu[i].mode)	/* Skip disabled counters */
				continue;

			per_cpu(perfPrev, cpu)[i] = 0;
			per_cpu(perfCurr, cpu)[i] = 0;
			per_cpu(perfPrevDelta, cpu)[i] = 0;
			per_cpu(pevent_attr, cpu)[i] = kmalloc(size, GFP_KERNEL);
			if (!per_cpu(pevent_attr, cpu)[i]) {
				cpupmu_stop();
				return;
			}

			memset(per_cpu(pevent_attr, cpu)[i], 0, size);
			per_cpu(pevent_attr, cpu)[i]->type = PERF_TYPE_RAW;
			per_cpu(pevent_attr, cpu)[i]->size = size;
			per_cpu(pevent_attr, cpu)[i]->config = pmu[i].event;
			per_cpu(pevent_attr, cpu)[i]->sample_period = 0;
			per_cpu(pevent_attr, cpu)[i]->pinned = 1;

			/* handle special case for cycle count */
			if (i == (nr_counters - 1)) {
				per_cpu(pevent_attr, cpu)[i]->type = PERF_TYPE_HARDWARE;
				per_cpu(pevent_attr, cpu)[i]->config = PERF_COUNT_HW_CPU_CYCLES;
			}
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 1, 0)
			per_cpu(pevent, cpu)[i] =
			    perf_event_create_kernel_counter(per_cpu(pevent_attr, cpu)[i], cpu, 0,
							     dummy_handler);
#else
			per_cpu(pevent, cpu)[i] =
			    perf_event_create_kernel_counter(per_cpu(pevent_attr, cpu)[i], cpu, 0,
							     dummy_handler, 0);
#endif
			if (IS_ERR(per_cpu(pevent, cpu)[i])) {
				printk(KERNEL_INFO"+++gator: unable to online a counter on cpu %d\n", cpu);
				per_cpu(pevent, cpu)[i] = NULL;
				continue;
			}

			if (per_cpu(pevent, cpu)[i]->state != PERF_EVENT_STATE_ACTIVE) {
				printk(KERNEL_INFO"+++gator: inactive counter on cpu %d\n", cpu);
				perf_event_release_kernel(per_cpu(pevent, cpu)[i]);
				per_cpu(pevent, cpu)[i] = NULL;
				continue;
			}

			ev = per_cpu(pevent, cpu)[i];
			ev->pmu->read(ev);
			per_cpu(perfPrev, cpu)[i] = per_cpu(perfCurr, cpu)[i] =
			    local64_read(&ev->count);

		}		/* for all PMU counter */
	}			/* for each present cpu */
}

#else
static void cpupmu_polling(unsigned long long stamp, int cpu)
{
	int count;
	unsigned int pmu_value[8];
	count = cpu_pmu->polling(pmu, nr_counters, pmu_value);
	mp_cpu(count, pmu_value);
	return;
}

static void cpupmu_start(void)
{
	nr_arg = 0;
	cpu_pmu->start(pmu, nr_counters);
}
/*
static void cpupmu_start(void)
{
	nr_arg = 0;
	on_each_cpu(_cpupmu_start, NULL, 1);
}
*/
static void cpupmu_stop(void)
{
	cpu_pmu->stop(nr_counters);
}
/*
static void cpupmu_stop(void)
{
	on_each_cpu(_cpupmu_stop, NULL, 1);
}
*/
#endif

static const char cache_line_header[] = "met-info [000] 0.0: met_cpu_cache_line_size: %d\n";
static const char header_n[] = "# mp_cpu: pmu_value1, ...\n" "met-info [000] 0.0: met_cpu_header: 0x%x:%s";
static const char header[] = "# mp_cpu: pmu_value1, ...\n" "met-info [000] 0.0: met_cpu_header: 0x%x";

static const char help[] =
	"  --pmu-cpu-evt=EVENT                   select CPU-PMU events. in %s,\n"
	"                                        you can enable at most \"%d general purpose events\"\n"
	"                                        plus \"one special 0xff (CPU_CYCLE) event\"\n";

static int cpupmu_print_help(char *buf, int len)
{
	return snprintf(buf, PAGE_SIZE, help, cpu_pmu->cpu_name, nr_counters - 1);
}

static int cpupmu_print_header(char *buf, int len)
{
	int i, ret, first;
	char name[32];

	first = 1;
	ret = 0;

    /*append cache line size*/
    ret += snprintf(buf + ret, PAGE_SIZE, cache_line_header, cache_line_size());

	for (i = 0; i < nr_counters; i++) {
		if (pmu[i].mode == 0)
			continue;
		if (cpu_pmu->get_event_desc && 0 == cpu_pmu->get_event_desc(i, pmu[i].event, name)) {
			if (first) {
				ret += snprintf(buf + ret, PAGE_SIZE, header_n, pmu[i].event, name);
				first = 0;
			} else {
				ret += snprintf(buf + ret, PAGE_SIZE, ",0x%x:%s", pmu[i].event, name);
			}
		}
		else {
			if (first) {
				ret += snprintf(buf + ret, PAGE_SIZE, header, pmu[i].event);
				first = 0;
			} else {
				ret += snprintf(buf + ret, PAGE_SIZE, ",0x%x", pmu[i].event);
			}
		}
		pmu[i].mode = 0;

	}

	ret += snprintf(buf + ret, PAGE_SIZE, "\n");
	met_cpupmu.mode = 0;
	nr_arg = 0;
	return ret;
}

/*
 * "met-cmd --start --pmu_cpu_evt=0x3"
 */
static int cpupmu_process_argument(const char *arg, int len)
{
	int i;
	unsigned int value;

	if (met_parse_num(arg, &value, len) < 0)
		return -EINVAL;

	if (cpu_pmu->check_event(pmu, nr_arg, value) < 0) {
		goto arg_out;
	}

	if (value == 0xff) {
		pmu[nr_counters - 1].mode = MODE_POLLING;
		pmu[nr_counters - 1].event = 0xff;
		pmu[nr_counters - 1].freq = 0;
	} else {

		if (nr_arg >= (nr_counters - 1))
			goto arg_out;

		pmu[nr_arg].mode = MODE_POLLING;
		pmu[nr_arg].event = value;
		pmu[nr_arg].freq = 0;
		nr_arg++;
	}

	met_cpupmu.mode = 1;
	return 0;

arg_out:
	met_cpupmu.mode = 0;
	nr_arg = 0;
	for (i = 0; i < nr_counters; i++) {
		pmu[nr_arg].mode = MODE_POLLING;
		pmu[nr_arg].event = value;
		pmu[nr_arg].freq = 0;
	}
	return -EINVAL;
}

struct metdevice met_cpupmu = {
	.name = "cpu",
	.type = MET_TYPE_PMU,
	.cpu_related = 1,
	.create_subfs = cpupmu_create_subfs,
	.delete_subfs = cpupmu_delete_subfs,
	.start = cpupmu_start,
	.stop = cpupmu_stop,
	.polling_interval = 1,
	.timed_polling = cpupmu_polling,
	.tagged_polling = cpupmu_polling,
	.print_help = cpupmu_print_help,
	.print_header = cpupmu_print_header,
	.process_argument = cpupmu_process_argument
};
