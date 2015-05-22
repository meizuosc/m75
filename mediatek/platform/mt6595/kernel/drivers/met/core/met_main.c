#include <linux/module.h>
#include <linux/hrtimer.h>
#include <linux/cpu.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/profile.h>
#include <linux/dcache.h>
#include <linux/types.h>
#include <linux/dcookies.h>
#include <linux/sched.h>
#include <linux/fs.h>

#include <asm/irq_regs.h>

#include "met_struct.h"
#include "met_drv.h"

DEFINE_PER_CPU(struct met_cpu_struct, met_cpu);

//FIXME : it is a temperary solution , remove it after 
extern const char *__start___trace_bprintk_fmt[];
extern const char *__stop___trace_bprintk_fmt[];
extern void trace_printk_init_buffers(void);

static int __init met_drv_init(void)
{
	int cpu;
	struct met_cpu_struct *met_cpu_ptr;

	for_each_possible_cpu(cpu) {
		met_cpu_ptr = &per_cpu(met_cpu, cpu);
		/* snprintf(&(met_cpu_ptr->name[0]), sizeof(met_cpu_ptr->name), "met%02d", cpu); */
		met_cpu_ptr->cpu = cpu;
	}

	fs_reg();
	return 0;
}

static void __exit met_drv_exit(void)
{
	fs_unreg();
}
module_init(met_drv_init);
module_exit(met_drv_exit);

MODULE_AUTHOR("DT_DM5");
MODULE_DESCRIPTION("MET_CORE");
MODULE_LICENSE("GPL");
