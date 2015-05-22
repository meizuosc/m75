#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/random.h>
#include <linux/fs.h>

#include "core/met_drv.h"
#include "core/trace.h"
#include "cci400_pmu_hw.h"

extern struct metdevice met_cci400;

static char help[] = "  --cci400=src:evt                      src: S0~S4 or M0~M2, evt:0x00~0x14 for S*, 0x00~0x11 for M*\n";

// 1 cycle + 4 event counters
static struct cci400_pmu_t ccipmu_all[MAX_EVENT_COUNT+1]; // ccipmu_all[0] is cycle counter
static struct cci400_pmu_t *ccipmu = &(ccipmu_all[1]);	// normal counters
static int used_count = 0;
static int start = 0;

#define CCI_FMT1	"%x\n"
#define CCI_FMT2	"%x,%x\n"
#define CCI_FMT3	"%x,%x,%x\n"
#define CCI_FMT4	"%x,%x,%x,%x\n"
#define CCI_FMT5	"%x,%x,%x,%x,%x\n"

#define CCI_VAL1	value[0]
#define CCI_VAL2	value[0],value[1]
#define CCI_VAL3	value[0],value[1],value[2]
#define CCI_VAL4	value[0],value[1],value[2],value[3]
#define CCI_VAL5	value[0],value[1],value[2],value[3],value[4]

static void sort_slot(void)
{
	int i, j;
	unsigned int tmp;

	for (i=0; i<used_count; i++) {
		for (j=i+1; j<used_count; j++) {
			if (ccipmu[i].value > ccipmu[j].value) {
				tmp = ccipmu[i].value;
				ccipmu[i].value = ccipmu[j].value;
				ccipmu[j].value = tmp;
			}
		}
	}
}

/*
 * Called from "met-cmd --start"
 */
static void cci400_start(void)
{
	sort_slot();
	cci400_pmu_hw_start(ccipmu_all, used_count + 1);
	start = 1;
	return;
}

/*
 * Called from "met-cmd --stop"
 */
static void cci400_stop(void)
{
	cci400_pmu_hw_stop();
	return;
}

static void cci400(unsigned long long stamp, int cpu)
{
	unsigned int value_all[MAX_EVENT_COUNT+1];
	unsigned int *value;
	int all_count = used_count + 1;

	cci400_pmu_hw_polling(ccipmu_all, all_count, value_all);
#ifdef PROBE_CCI400_CYCLES
	value = value_all;
#else
	value = &(value_all[1]);
	all_count--;
#endif

	switch (all_count) {
#ifndef PROBE_CCI400_CYCLES
	case 1: MET_PRINTK(CCI_FMT1, CCI_VAL1); break;
#endif
	case 2: MET_PRINTK(CCI_FMT2, CCI_VAL2); break;
	case 3: MET_PRINTK(CCI_FMT3, CCI_VAL3); break;
	case 4: MET_PRINTK(CCI_FMT4, CCI_VAL4); break;
#ifdef PROBE_CCI400_CYCLES
	case 5: MET_PRINTK(CCI_FMT5, CCI_VAL5); break;
#endif
	}
}

/*
 * Called from "met-cmd --help"
 */
static int cci400_print_help(char *buf, int len)
{
	return snprintf(buf, PAGE_SIZE, help);
	return 0;
}

#ifdef PROBE_CCI400_CYCLES
static char header[] = "met-info [000] 0.0: ms_ud_sys_header: cci400,CYCLES";
#else
static char header[] = "met-info [000] 0.0: ms_ud_sys_header: cci400";
#endif

static const char *src_name[] = { "S0", "S1", "S2", "S3", "S4", "M0", "M1", "M2" };

static inline void reset_driver_stat(void)
{
	met_cci400.mode = 0;
	used_count = 0;
	start = 0;
}

/*
 * It will be called back when run "met-cmd --extract" and mode is 1
 */
static int cci400_print_header(char *buf, int len)
{
	int i, size, total_size;

	size = snprintf(buf, PAGE_SIZE, header);
	total_size = size;
	buf += size;

	for (i=0; i<used_count; i++) {
		size = snprintf(buf, PAGE_SIZE, ",%s:0x%02X",
				src_name[ccipmu[i].evt_sel.interface],
				ccipmu[i].evt_sel.event);
		total_size += size;
		buf += size;
	}
#ifdef PROBE_CCI400_CYCLES
	for (i=0; i<=used_count; i++) { // includes cycles
#else
	for (i=1; i<=used_count; i++) { // not includes cycles
#endif
		size = snprintf(buf, PAGE_SIZE, ",x");
		total_size += size;
		buf += size;
	}
	size = snprintf(buf, PAGE_SIZE, "\n");
	total_size += size;

	reset_driver_stat();
	return total_size;
}

static int parse_num(const char *str, unsigned int *value, int len)
{
	unsigned int i;

	if (len <= 0) {
		return -1;
	}

	if ((len > 2) &&
		((str[0]=='0') &&
		((str[1]=='x') || (str[1]=='X')))) {
		for (i=2; i<len; i++) {
			if (! (((str[i] >= '0') && (str[i] <= '9'))
			   || ((str[i] >= 'a') && (str[i] <= 'f'))
			   || ((str[i] >= 'A') && (str[i] <= 'F')))) {
				return -1;
			}
		}
		sscanf(str, "%x", value);
	} else {
		for (i=0; i<len; i++) {
			if (! ((str[i] >= '0') && (str[i] <= '9'))) {
				return -1;
			}
		}
		sscanf(str, "%d", value);
	}

	return 0;
}

static int assign_slot(unsigned int interface, unsigned int event)
{
	int i;

	if (used_count == MAX_EVENT_COUNT) {
		return -1;
	}

	for (i=0; i<used_count; i++) {
		if ((ccipmu[i].evt_sel.interface == interface) &&
		    (ccipmu[i].evt_sel.event == event)) {
			return -2;
		}
	}

	ccipmu[used_count].evt_sel.unused = 0;
	ccipmu[used_count].evt_sel.interface = interface;
	ccipmu[used_count].evt_sel.event = event;
	used_count++;

	return 0;
}

/*
 * "met-cmd --start --cci400=m0:3 --cci400=s0:0x12"
 */
static int cci400_process_argument(const char *arg, int len)
{
	unsigned int interface, event;

	if (start == 1) {
		reset_driver_stat();
	}

	// for example s0:3
	if ((len < 4) || (arg[2] != ':')) {
		return -1;
	}

	if ((arg[0] == 's') || (arg[0] == 'S')) { // S0 ~ S4
		if ((arg[1] >= '0') && (arg[1] <= '4')) {
			interface = 0x0 + (arg[1] - '0');
		} else {
			return -1;
		}
	} else if ((arg[0] == 'm') || (arg[0] == 'M')) { // M0 ~ M2
		if ((arg[1] >= '0') && (arg[1] <= '2')) {
			interface = 0x5 + (arg[1] - '0');
		} else {
			return -1;
		}
	} else {
		return -1;
	}

	// the string "s0:" contains 3 chars
	if (parse_num(&(arg[3]), &event, len-3) < 0) {
		return -1;
	}

	if (cci400_pmu_hw_rev() <= 4) { // <= r0p4
		if (interface <= 0x4) { // S0 ~ S4
			if (event > 0x13) {
				return -1;
			}
		} else { // M0 ~ M2
			if ((event < 0x14) || (event > 0x1a)) {
				return -1;
			}
		}
	} else {
		if (interface <= 0x4) { // S0 ~ S4
			if (event > 0x14) {
				return -1;
			}
		} else { // M0 ~ M2
			if (event > 0x11) {
				return -1;
			}
		}
	}

	if (assign_slot(interface, event) < 0) {
		return -1;
	}

	met_cci400.mode = 1;
	return 0;
}

static int cci400_create_subfs(struct kobject *parent)
{
	return cci400_pmu_hw_init();
}

static void cci400_delete_subfs(void)
{
	cci400_pmu_hw_uninit();
}

struct metdevice met_cci400 = {
	.name = "cci400",
	.owner = THIS_MODULE,
	.type = MET_TYPE_BUS,
	.cpu_related = 0,
	.create_subfs = cci400_create_subfs,
	.delete_subfs = cci400_delete_subfs,
	.start = cci400_start,
	.stop = cci400_stop,
	.polling_interval = 1, // ms
	.timed_polling = cci400,
	.tagged_polling = cci400,
	.print_help = cci400_print_help,
	.print_header = cci400_print_header,
	.process_argument = cci400_process_argument
};

EXPORT_SYMBOL(met_cci400);
