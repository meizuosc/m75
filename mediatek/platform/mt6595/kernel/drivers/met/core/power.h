#ifndef _POWER_H_
#define _POWER_H_

#define POWER_LOG_ALL	-1
void force_power_log(int cpu);
int init_power_log(void);
int uninit_power_log(void);

#endif				/* _POWER_H_ */
