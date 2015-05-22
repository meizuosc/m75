#ifndef _SAMPLER_H_
#define _SAMPLER_H_

/*
 * sampling rate: 1ms
 * log generating rate: 10ms
 */
#if 0
#define DEFAULT_TIMER_EXPIRE (HZ / 100)
#define DEFAULT_HRTIMER_EXPIRE (TICK_NSEC / 10)
#else
extern int timer_expire;	/* in jiffies */
extern int hrtimer_expire;	/* in us */
#define DEFAULT_TIMER_EXPIRE (timer_expire)
#define DEFAULT_HRTIMER_EXPIRE (hrtimer_expire)
#endif
/*
 * sampling rate: 10ms
 * log generating rate: 100ms
 */
/* #define DEFAULT_TIMER_EXPIRE (HZ / 10) */
/* #define DEFAULT_HRTIMER_EXPIRE (TICK_NSEC / 1) */

int met_hrtimer_start(void);
void met_hrtimer_stop(void);
int sampler_start(void);
void sampler_stop(void);

extern struct list_head met_list;
extern void add_cookie(struct pt_regs *regs, int cpu);

#ifdef CONFIG_CPU_FREQ
#include "power.h"

extern volatile int do_dvfs;
#endif

#endif				/* _SAMPLER_H_ */
