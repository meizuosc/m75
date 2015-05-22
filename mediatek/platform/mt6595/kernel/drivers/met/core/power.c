#include <asm/percpu.h>
#include <linux/cpufreq.h>
#include <trace/events/power.h>

#include "power.h"
//#include "trace.h"
#include "met_drv.h"

extern volatile int do_dvfs;
static DEFINE_PER_CPU(unsigned int, prev_cpufreq);

noinline void cpu_frequency(unsigned int frequency, unsigned int cpu_id)
{
	MET_PRINTK("state=%d cpu_id=%d\n", frequency, cpu_id);
}

void force_power_log(int cpu)
{
	struct cpufreq_policy *p;

	if (cpu == POWER_LOG_ALL) {
		for_each_possible_cpu(cpu) {
			p = cpufreq_cpu_get(cpu);
			if (p != NULL) {
				cpu_frequency(p->cur, cpu);
				cpufreq_cpu_put(p);
				per_cpu(prev_cpufreq, cpu) = p->cur;
			} else {
				cpu_frequency(0, cpu);
			}
		}
	} else {
		p = cpufreq_cpu_get(cpu);
		if (p != NULL) {
			cpu_frequency(p->cur, cpu);
			cpufreq_cpu_put(p);
			per_cpu(prev_cpufreq, cpu) = p->cur;
		} else {
			cpu_frequency(0, cpu);
		}
	}
}


MET_DEFINE_PROBE(cpu_frequency, TP_PROTO(unsigned int frequency, unsigned int cpu))
{
	if (do_dvfs)
		cpu_frequency(per_cpu(prev_cpufreq, cpu), cpu);
	per_cpu(prev_cpufreq, cpu) = frequency;
}


int init_power_log(void)
{
	int cpu;

	// register tracepoints
	if (MET_REGISTER_TRACE(cpu_frequency))
			return -1;

	for_each_present_cpu(cpu) {
		per_cpu(prev_cpufreq, cpu) = 0;
	}

	return 0;
}

int uninit_power_log(void)
{

	MET_UNREGISTER_TRACE(cpu_frequency);
	return 0;
}


