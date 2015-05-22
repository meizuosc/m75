
#ifndef _PMU_H_
#define _PMU_H_

#include <linux/device.h>

struct met_pmu {
	unsigned char mode;
	unsigned short event;
	unsigned long freq;
	struct kobject *kobj_cpu_pmu;
};

#define MODE_DISABLED	0
#define MODE_INTERRUPT	1
#define MODE_POLLING	2

struct cpu_pmu_hw {
	const char *name;
	const char *cpu_name;
	int nr_cnt;
	int (*get_event_desc) (int idx, int event, char* event_desc);
	int (*check_event) (struct met_pmu *pmu, int idx, int event);
	void (*start) (struct met_pmu *pmu, int count);
	void (*stop) (int count);
	unsigned int (*polling) (struct met_pmu *pmu, int count, unsigned int *pmu_value);
};

struct cpu_pmu_hw* cpu_pmu_hw_init(void);

struct pmu_desc {
	unsigned int event;
	char name[32];
};

#endif				/* _PMU_H_ */
