#ifndef _MET_STRUCT_H_
#define _MET_STRUCT_H_

#include <linux/hrtimer.h>

struct met_cpu_struct {
	struct hrtimer hrtimer;
	struct delayed_work dwork;
/* struct kmem_cache *cachep; */
/* struct list_head sample_head; */
/* spinlock_t list_lock; */
/* struct mutex list_sync_lock; */
	int work_enabled;
	int cpu;
/* char name[16]; */
};

DECLARE_PER_CPU(struct met_cpu_struct, met_cpu);

#endif				/* _MET_STRUCT_H_ */
