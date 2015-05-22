#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/random.h>
#include <linux/fs.h>

#include <mach/mt_spm.h>
#include "core/met_drv.h"
#include "core/trace.h"
#include "spm_twam.h"

extern struct metdevice met_spmtwam;

static char help[] = "  --spmtwam=clock:[speed|normal]        default is normal, normal mode monitors 4 channels, speed mode 2 channels\n"
"  --spmtwam=signal:selx                 selx= 0 ~ 31\n";

// 2 or 4 event counters
#define MAX_EVENT_COUNT 4
static struct twam_sig twamsig;
static unsigned int twamsig_sel[MAX_EVENT_COUNT];
static int used_count = 0;
static int start = 0;
static bool twam_clock_mode = true; //true:speed mode, false:normal mode

#define SPM_TWAM_FMT1	"%x\n"
#define SPM_TWAM_FMT2	"%x,%x\n"
#define SPM_TWAM_FMT3	"%x,%x,%x\n"
#define SPM_TWAM_FMT4	"%x,%x,%x,%x\n"

#define SPM_TWAM_VAL1	value[0]
#define SPM_TWAM_VAL2	value[0],value[1]
#define SPM_TWAM_VAL3	value[0],value[1],value[2]
#define SPM_TWAM_VAL4	value[0],value[1],value[2],value[3]

noinline static void spmtwam(struct twam_sig *ts)
{

	switch (used_count) {
	case 1:
		MET_PRINTK(SPM_TWAM_FMT1,get_high_percent(ts->sig0));
		break;
	case 2:
		MET_PRINTK(SPM_TWAM_FMT2,get_high_percent(ts->sig0),get_high_percent(ts->sig1));
		break;
	case 3:
		MET_PRINTK(SPM_TWAM_FMT3,get_high_percent(ts->sig0),get_high_percent(ts->sig1),get_high_percent(ts->sig2));
		break;
	case 4:
		MET_PRINTK(SPM_TWAM_FMT4,get_high_percent(ts->sig0),get_high_percent(ts->sig1),get_high_percent(ts->sig2),get_high_percent(ts->sig3));
		break;
	default :
		MET_PRINTK("No assign\n");
		break;
	}
}

/*
 * Called from "met-cmd --start"
 */
static void spmtwam_start(void)
{
	if (twam_clock_mode && used_count > 2) {
		used_count = 2;
	}

       spm_twam_register_handler(spmtwam);
	spm_twam_enable_monitor(&twamsig, twam_clock_mode);
	start = 1;
	return;
}

/*
 * Called from "met-cmd --stop"
 */
static void spmtwam_stop(void)
{
       spm_twam_register_handler(NULL);
	spm_twam_disable_monitor();
	start = 0;
	return;
}

/*
 * Called from "met-cmd --help"
 */
static int spmtwam_print_help(char *buf, int len)
{
	return snprintf(buf, PAGE_SIZE, help);
	return 0;
}

static char header[] = "met-info [000] 0.0: ms_ud_sys_header: spmtwam";

static inline void reset_driver_stat(void)
{

//	spm_twam_register_handler(0);
	met_spmtwam.mode = 0;
	used_count = 0;
	start = 0;
}

/*
 * It will be called back when run "met-cmd --extract" and mode is 1
 */
static int spmtwam_print_header(char *buf, int len)
{
	int i, size, total_size;

	size = snprintf(buf, PAGE_SIZE, header);
	total_size = size;
	buf += size;

	for (i=0; i<used_count; i++) {
		size = snprintf(buf, PAGE_SIZE, ",0x%02X:%s(%%)",
				twamsig_sel[i],
				twam_sig_list[twamsig_sel[i]].name);
		total_size += size;
		buf += size;
	}
	for (i=1; i<=used_count; i++) {
		size = snprintf(buf, PAGE_SIZE, ",x");
		total_size += size;
		buf += size;
	}
	size = snprintf(buf, PAGE_SIZE, "\n");
	total_size += size;
	buf += size;

	size = snprintf(buf, PAGE_SIZE, "met-info [000] 0.0:spmtwam_clock_mode=%s\n",twam_clock_mode == true? "speed":"normal");
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

static int assign_slot(unsigned int event)
{
	int i;

	if (used_count == MAX_EVENT_COUNT) {
		return -1;
	}

	//check duplicated
	for (i=0; i<used_count; i++) {
		if  (twamsig_sel[i] == event) {
			return -2;
		}
	}

	twamsig_sel[used_count] = event;
	switch (used_count) {
	case 0:
		twamsig.sig0 = event; break;
	case 1:
		twamsig.sig1 = event; break;
	case 2:
		twamsig.sig2 = event; break;
	case 3:
		twamsig.sig3 = event; break;
	}
	used_count++;

	return 0;
}

/*
 * "  --spmtwam=clock:[speed|normal]          default is normal, normal mode monitors 4 channels, speed mode 2 channels\n"
 * "  --spmtwam=signal:signal_selx            signal_selx= 0 ~ 15\n"
*/

static int spmtwam_process_argument(const char *arg, int len)
{
	unsigned int event;

	if (start == 1) {
		reset_driver_stat();
	}

	if (strncmp(arg, "clock:", 6) == 0) {
		if (strncmp(&(arg[6]), "speed", 5) == 0) {
			twam_clock_mode = true;
		} else if (strncmp(&(arg[6]), "normal", 6) == 0){
			twam_clock_mode = false;
		} else {
			return -1;
		}
	} else if (strncmp(arg, "signal:", 7) == 0) {
		if (parse_num(&(arg[7]), &event, len-7) < 0) {
			return -1;
		}
		if (assign_slot(event) < 0) {
			return -1;
		}
	} else if (strncmp(arg, "scpsys:", 7) == 0) { //TODO:
		event = 0;
		if (arg[7] == '0') {//infrasys

		} else if (arg[7] == '1') {//perisys

		} else {
			return -1;
		}
	} else {
		return -1;
	}

	met_spmtwam.mode = 1;
	return 0;
}

static int spmtwam_create_subfs(struct kobject *parent)
{
//	spm_twam_register_handler(spmtwam);
	return 0;
}

static void spmtwam_delete_subfs(void)
{
//	spm_twam_register_handler(0);
}

struct metdevice met_spmtwam = {
	.name = "spmtwam",
	.owner = THIS_MODULE,
	.type = MET_TYPE_BUS,
	.cpu_related = 0,
	.create_subfs = spmtwam_create_subfs,
	.delete_subfs = spmtwam_delete_subfs,
	.start = spmtwam_start,
	.stop = spmtwam_stop,
	.print_help = spmtwam_print_help,
	.print_header = spmtwam_print_header,
	.process_argument = spmtwam_process_argument
};

EXPORT_SYMBOL(met_spmtwam);
