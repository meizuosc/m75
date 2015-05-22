#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/random.h>
#include <linux/fs.h>

#include "core/met_drv.h"
#include "core/trace.h"

extern int pmic_get_buck_current(int avg_times);

extern struct metdevice met_pmic;
static struct delayed_work dwork;

static char help[] = "  --pmic                                Meature PMIC buck currents\n";

/*
 * Called from "met-cmd --start"
 */
static void pmic_start(void)
{
	cancel_delayed_work_sync(&dwork); // jobs are triggered by pmic_polling
	return;
}

/*
 * Called from "met-cmd --stop"
 */
static void pmic_stop(void)
{
	cancel_delayed_work_sync(&dwork);
	return;
}

void pmic(struct work_struct *work)
{
	int currents;
	currents = pmic_get_buck_current(1);
	MET_PRINTK("%x\n", currents);
}

static void pmic_polling(unsigned long long stamp, int cpu)
{
	schedule_delayed_work(&dwork, 0);
}

/*
 * Called from "met-cmd --help"
 */
static int pmic_print_help(char *buf, int len)
{
	return snprintf(buf, PAGE_SIZE, help);
	return 0;
}

static char header[] = "met-info [000] 0.0: ms_ud_sys_header: pmic,BUCK_CURRENT (mA),x\n";

/*
 * It will be called back when run "met-cmd --extract" and mode is 1
 */
static int pmic_print_header(char *buf, int len)
{
	cancel_delayed_work_sync(&dwork);
	met_pmic.mode = 0;
	return snprintf(buf, PAGE_SIZE, header);
}

/*
 * "met-cmd --start --pmic"
 */
static int pmic_process_argument(const char *arg, int len)
{
	cancel_delayed_work_sync(&dwork);
	met_pmic.mode = 1;
	return 0;
}

static int pmic_create_subfs(struct kobject *parent)
{
	INIT_DELAYED_WORK(&dwork, pmic);
	return 0;
}

static void pmic_delete_subfs(void)
{
	cancel_delayed_work_sync(&dwork);
}

struct metdevice met_pmic = {
	.name = "pmic",
	.owner = THIS_MODULE,
	.type = MET_TYPE_BUS,
	.cpu_related = 0,
	.create_subfs = pmic_create_subfs,
	.delete_subfs = pmic_delete_subfs,
	.start = pmic_start,
	.stop = pmic_stop,
	.polling_interval = 100, // In "ms", NOTE: low level API delays more than 5ms
	.timed_polling = pmic_polling,
	.tagged_polling = pmic_polling,
	.print_help = pmic_print_help,
	.print_header = pmic_print_header,
	.process_argument = pmic_process_argument
};

EXPORT_SYMBOL(met_pmic);

