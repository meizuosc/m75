#include <linux/cpumask.h>
/* #include <linux/fs.h> */
#include <linux/init.h>
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>
/* #include <linux/proc_fs.h> */
#include <linux/sched.h>
/* #include <linux/seq_file.h> */
/* #include <linux/slab.h> */
#include <linux/time.h>
#include <linux/irqnr.h>
#include <linux/vmalloc.h>
#include <asm/cputime.h>
/* #include <asm-generic/cputime.h> */
#include <linux/tick.h>
/* #include <linux/jiffies.h> */

#include <asm/page.h>
#include <linux/slab.h>

#include "stat.h"
#include "met_drv.h"
#include "trace.h"

#define MS_STAT_FMT	"%5lu.%06lu"
#define MS_STAT_VAL	(unsigned long)(timestamp), nano_rem/1000
#define FMTLX7		",%llx,%llx,%llx,%llx,%llx,%llx,%llx\n"
#define FMTLX10		",%llx,%llx,%llx,%llx,%llx,%llx,%llx,%llx,%llx,%llx\n"
/* void ms_st(unsigned long long timestamp, unsigned char cnt, unsigned int *value) */
void ms_st(unsigned long long timestamp, unsigned char cnt, u64 *value)
{
	unsigned long nano_rem = do_div(timestamp, 1000000000);
	switch (cnt) {
	case 10:
		MET_PRINTK(MS_STAT_FMT FMTLX10, MS_STAT_VAL VAL10);
		break;
	case 7:
		MET_PRINTK(MS_STAT_FMT FMTLX7, MS_STAT_VAL VAL7);
		break;
	}
}

static void met_stat_start(void)
{
	if (get_ctrl_flags() & 1)
		met_stat.mode = 0;

	return;
}

static void met_stat_stop(void)
{
	return;
}

static int do_stat(void)
{
	return met_stat.mode;
}

u64 met_usecs_to_cputime64(u64 n)
{
#if (NSEC_PER_SEC % HZ) == 0
	/* Common case, HZ = 100, 128, 200, 250, 256, 500, 512, 1000 etc. */
	return div_u64(n, NSEC_PER_SEC / HZ);
#elif (HZ % 512) == 0
	/* overflow after 292 years if HZ = 1024 */
	return div_u64(n * HZ / 512, NSEC_PER_SEC / 512);
#else
	/*
	 * Generic case - optimized for cases where HZ is a multiple of 3.
	 * overflow after 64.99 years, exact for HZ = 60, 72, 90, 120 etc.
	 */
	return div_u64(n * 9, (9ull * NSEC_PER_SEC + HZ / 2) / HZ);
#endif
}

static u64 get_idle_time(int cpu)
{
	u64 idle, idle_time = get_cpu_idle_time_us(cpu, NULL);

	if (idle_time == -1ULL)
		/* !NO_HZ so we can rely on cpustat.idle */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
		idle = kcpustat_cpu(cpu).cpustat[CPUTIME_IDLE];
#else
		idle = kstat_cpu(cpu).cpustat.idle;
#endif
	else
		idle = met_usecs_to_cputime64(idle_time);

	return idle;
}

static u64 get_iowait_time(int cpu)
{
	u64 iowait, iowait_time = get_cpu_iowait_time_us(cpu, NULL);

	if (iowait_time == -1ULL)
		/* !NO_HZ so we can rely on cpustat.iowait */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
		iowait = kcpustat_cpu(cpu).cpustat[CPUTIME_IOWAIT];
#else
		iowait = kstat_cpu(cpu).cpustat.iowait;
#endif
	else
		iowait = met_usecs_to_cputime64(iowait_time);

	return iowait;
}


static unsigned int stat_os_polling(u64 *value, int i)
{
	int j = -1;

	/* return 0; */
	/* Copy values here to work around gcc-2.95.3, gcc-2.96 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
	value[++j] = cputime64_to_clock_t(kcpustat_cpu(i).cpustat[CPUTIME_USER]);	/* user */
	value[++j] = cputime64_to_clock_t(kcpustat_cpu(i).cpustat[CPUTIME_NICE]);	/* nice */
	value[++j] = cputime64_to_clock_t(kcpustat_cpu(i).cpustat[CPUTIME_SYSTEM]);	/* system */
	value[++j] = cputime64_to_clock_t(get_idle_time(i));	/* idle */
	value[++j] = cputime64_to_clock_t(get_iowait_time(i));	/* iowait */
	value[++j] = cputime64_to_clock_t(kcpustat_cpu(i).cpustat[CPUTIME_IRQ]);	/* irq */
	value[++j] = cputime64_to_clock_t(kcpustat_cpu(i).cpustat[CPUTIME_SOFTIRQ]);	/* softirq */
	value[++j] = cputime64_to_clock_t(kcpustat_cpu(i).cpustat[CPUTIME_STEAL]);	/* steal */
	value[++j] = cputime64_to_clock_t(kcpustat_cpu(i).cpustat[CPUTIME_GUEST]);	/* guest */
	value[++j] = cputime64_to_clock_t(kcpustat_cpu(i).cpustat[CPUTIME_GUEST_NICE]);	/* guest_nice */
#else
	value[++j] = cputime64_to_clock_t(kstat_cpu(i).cpustat.user);	/* user */
	value[++j] = cputime64_to_clock_t(kstat_cpu(i).cpustat.nice);	/* nice */
	value[++j] = cputime64_to_clock_t(kstat_cpu(i).cpustat.system);	/* system */
	value[++j] = cputime64_to_clock_t(get_idle_time(i));	/* idle */
	value[++j] = cputime64_to_clock_t(get_iowait_time(i));	/* iowait */
	value[++j] = cputime64_to_clock_t(kstat_cpu(i).cpustat.irq);	/* irq */
	value[++j] = cputime64_to_clock_t(kstat_cpu(i).cpustat.softirq);	/* softirq */
	value[++j] = cputime64_to_clock_t(kstat_cpu(i).cpustat.steal);	/* steal */
	value[++j] = cputime64_to_clock_t(kstat_cpu(i).cpustat.guest);	/* guest */
	value[++j] = cputime64_to_clock_t(kstat_cpu(i).cpustat.guest_nice);	/* guest_nice */
#endif

	return j + 1;
}

static void met_stat_polling(unsigned long long stamp, int cpu)
{
	unsigned char count;
	u64 value[10];
	/* return; */
	if (do_stat()) {
		count = stat_os_polling(value, cpu);
		if (count) {
			/* printk("stat_polling..cpu=%d, count=%d\n", cpu, count); */
			ms_st(stamp, count, value);
		}
	}
}

static const char header[] =
	"met-info [000] 0.0: met_st_header: user,nice,system,idle,iowait,irq,softirq,steal,guest,guest_nice\n";

static const char help[] = "  --stat                                monitor stat\n";


static int met_stat_print_help(char *buf, int len)
{
	return snprintf(buf, PAGE_SIZE, help);
}

static int met_stat_print_header(char *buf, int len)
{
	return snprintf(buf, PAGE_SIZE, header);
}

struct metdevice met_stat = {
	.name = "stat",
	.type = MET_TYPE_PMU,
	.cpu_related = 1,
	.start = met_stat_start,
	.stop = met_stat_stop,
	.polling_interval = 30,
	.timed_polling = met_stat_polling,
	.print_help = met_stat_print_help,
	.print_header = met_stat_print_header,
};
