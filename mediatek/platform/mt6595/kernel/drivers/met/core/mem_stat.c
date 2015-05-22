#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/cpu.h>
#include <linux/module.h>
#include <linux/fs.h>

#include "met_drv.h"
#include "mem_stat.h"
#include "trace.h"


//#define MEMSTAT_DEBUG
#ifdef MEMSTAT_DEBUG
#define debug_memstat(fmt, arg...) printk(fmt, ##arg)
#else
#define debug_memstat(fmt, arg...) do {} while (0)
#endif

struct metdevice met_memstat;

unsigned int phy_memstat_mask;
unsigned int vir_memstat_mask;

#define MAX_PHY_MEMSTAT_EVENT_AMOUNT 6
#define MAX_VIR_MEMSTAT_EVENT_AMOUNT 6

struct mem_event phy_memstat_table[] = {
	{FREE_MEM, "free_mem"}
};

#define PHY_MEMSTAT_TABLE_SIZE (sizeof(phy_memstat_table) / sizeof(struct mem_event))

struct mem_event vir_memstat_table[] = {
	{FILE_PAGES, "file_pages"},
	{FILE_DIRTY, "file_dirty"},
	{NUM_DIRTIED, "num_dirtied"},
	{WRITE_BACK, "write_back"},
	{NUM_WRITTEN, "num_written"},
	{PG_FAULT_CNT, "pg_fault_cnt"}
};

#define VIR_MEMSTAT_TABLE_SIZE (sizeof(vir_memstat_table) / sizeof(struct mem_event))

int vm_event_counters_enable;
unsigned long *vm_status;
static struct delayed_work dwork;

void memstat(unsigned int cnt, unsigned int *value)
{
	switch (cnt) {
	case 1:
		MET_PRINTK(MP_FMT1, MP_VAL1);
		break;
	case 2:
		MET_PRINTK(MP_FMT2, MP_VAL2);
		break;
	case 3:
		MET_PRINTK(MP_FMT3, MP_VAL3);
		break;
	case 4:
		MET_PRINTK(MP_FMT4, MP_VAL4);
		break;
	case 5:
		MET_PRINTK(MP_FMT5, MP_VAL5);
		break;
	case 6:
		MET_PRINTK(MP_FMT6, MP_VAL6);
		break;
	case 7:
		MET_PRINTK(MP_FMT7, MP_VAL7);
		break;
	case 8:
		MET_PRINTK(MP_FMT8, MP_VAL8);
		break;
	case 9:
		MET_PRINTK(MP_FMT9, MP_VAL9);
		break;
	case 10:
		MET_PRINTK(MP_FMT10, MP_VAL10);
		break;
	case 11:
		MET_PRINTK(MP_FMT11, MP_VAL11);
		break;
	case 12:
		MET_PRINTK(MP_FMT12, MP_VAL12);
		break;
	}
}

static int get_phy_memstat(unsigned int *value)
{
	int i, cnt = 0;
	struct sysinfo info;

#define K(x) ((x) << (PAGE_SHIFT - 10))

	si_meminfo(&info);

	for (i = 0; i < MAX_PHY_MEMSTAT_EVENT_AMOUNT; i++) {
		if (phy_memstat_mask & (1 << i)) {
			switch (i) {
			case FREE_MEM:
				value[cnt] = K(info.freeram);
				break;
			}

			cnt++;
		}
	}

	return cnt;
}

static int get_vir_memstat(unsigned int *value)
{
	int i, cnt = 0;

	for (i = 0; i < NR_VM_ZONE_STAT_ITEMS; i++)
		vm_status[i] = global_page_state(i);

	all_vm_events(vm_status + NR_VM_ZONE_STAT_ITEMS);

	for (i = 0; i < MAX_VIR_MEMSTAT_EVENT_AMOUNT; i++) {
		if (vir_memstat_mask & (1 << i)) {
			switch (i) {
			case FILE_PAGES:
				value[cnt] = vm_status[NR_FILE_PAGES] << (PAGE_SHIFT - 10);
				break;
			case FILE_DIRTY:
				value[cnt] = vm_status[NR_FILE_DIRTY] << (PAGE_SHIFT - 10);
				break;
			case NUM_DIRTIED:
				value[cnt] = vm_status[NR_DIRTIED] << (PAGE_SHIFT - 10);
				break;
			case WRITE_BACK:
				value[cnt] = vm_status[NR_WRITEBACK] << (PAGE_SHIFT - 10);
				break;
			case NUM_WRITTEN:
				value[cnt] = vm_status[NR_WRITTEN] << (PAGE_SHIFT - 10);
				break;
			case PG_FAULT_CNT:
				value[cnt] = vm_status[NR_VM_ZONE_STAT_ITEMS + PGFAULT];
				break;
			}

			cnt++;
		}
	}

	return cnt;
}

static void wq_get_memstat(struct work_struct *work)
{
	int total_event_amount = 0, phy_event_amount = 0;
	unsigned int stat_val[MAX_PHY_MEMSTAT_EVENT_AMOUNT + MAX_VIR_MEMSTAT_EVENT_AMOUNT];

	memset(stat_val, 0, sizeof(stat_val));
	total_event_amount = phy_event_amount = get_phy_memstat(stat_val);

	if (vm_event_counters_enable) {
		total_event_amount += get_vir_memstat(&stat_val[phy_event_amount]);
	}

	memstat(total_event_amount, stat_val);
}

void met_memstat_polling(unsigned long long stamp, int cpu)
{
	schedule_delayed_work(&dwork, 0);
}

static void met_memstat_start(void)
{
	int stat_items_size = 0;

	stat_items_size = NR_VM_ZONE_STAT_ITEMS * sizeof(unsigned long);

#ifdef CONFIG_VM_EVENT_COUNTERS
	stat_items_size += sizeof(struct vm_event_state);
#endif

	vm_status = kmalloc(stat_items_size, GFP_KERNEL);
	if (!vm_status)
		printk("Error: [%s] Line %d alloc virtual memory status table fail ...\n", __func__, __LINE__);

	INIT_DELAYED_WORK(&dwork, wq_get_memstat);

	return;
}

static void met_memstat_stop(void)
{
	if (vm_status)
		kfree(vm_status);

	cancel_delayed_work_sync(&dwork);

	return;
}

static const char help[] = "--memstat=[phy_mem_stat|vir_mem_stat]:event_name		enable sampling physical & virtual memory status\n";

static int met_memstat_print_help(char *buf, int len)
{
	int i, l;

	l = snprintf(buf, PAGE_SIZE, help);

	for (i = 0; i < PHY_MEMSTAT_TABLE_SIZE; i++) {
		l += snprintf(buf + l, PAGE_SIZE - l, "--memstat=phy_mem_stat:%s\n", phy_memstat_table[i].name);
	}

#ifdef CONFIG_VM_EVENT_COUNTERS
	for (i = 0; i < VIR_MEMSTAT_TABLE_SIZE; i++) {
		l += snprintf(buf + l, PAGE_SIZE - l, "--memstat=vir_mem_stat:%s\n", vir_memstat_table[i].name);
	}
#endif

	return l;
}

static const char header[] =
	"# memstat: free_memory,page_fault_count\n"
	"met-info [000] 0.0: ms_ud_sys_header: memstat,";


static int met_memstat_print_header(char *buf, int len)
{
	int i, l;
	int event_amount = 0;

	l = snprintf(buf, PAGE_SIZE, header);

	for (i = 0; i < MAX_PHY_MEMSTAT_EVENT_AMOUNT; i++) {
		if (phy_memstat_mask & (1 << i) && (i < PHY_MEMSTAT_TABLE_SIZE)) {
			l += snprintf(buf + l, PAGE_SIZE - l, phy_memstat_table[i].name);
			l += snprintf(buf + l, PAGE_SIZE - l, ",");
			event_amount++;
		}
	}

#ifdef CONFIG_VM_EVENT_COUNTERS
	for (i = 0; i < MAX_VIR_MEMSTAT_EVENT_AMOUNT; i++) {
		if (vir_memstat_mask & (1 << i) && (i < PHY_MEMSTAT_TABLE_SIZE)) {
			l += snprintf(buf + l, PAGE_SIZE - l, vir_memstat_table[i].name);
			l += snprintf(buf + l, PAGE_SIZE - l, ",");
			event_amount++;
		}
	}
#endif

	for (i = 0; i < event_amount; i++) {
		l += snprintf(buf + l, PAGE_SIZE - l, "x");
		l += snprintf(buf + l, PAGE_SIZE - l, ",");
	}

	l += snprintf(buf + l, PAGE_SIZE - l, "\n");

	return l;
}

static int met_memstat_process_argument(const char *arg, int len)
{
	int i, found_event = 0;
	char choice[16], event[32];
	char *pch;
	int str_len;

#ifdef CONFIG_VM_EVENT_COUNTERS
	vm_event_counters_enable = 1;
#endif

	pch = strchr(arg, ':');
	if (pch == NULL)
		goto error;

	memset(choice, 0, sizeof(choice));
	memset(event, 0, sizeof(event));

	str_len = (int) (pch - arg);
	memcpy(choice, arg, str_len);
	memcpy(event, arg + str_len + 1, len - (str_len + 1));

	if (strncmp(choice, "phy_mem_stat", 12) == 0) {
		for (i = 0; i < PHY_MEMSTAT_TABLE_SIZE; i++) {
			if (strncmp(event, phy_memstat_table[i].name, MAX_EVENT_NAME_LEN) == 0) {
				phy_memstat_mask |= (1 << phy_memstat_table[i].id);
				found_event = 1;

				break;
			}
		}
	} else if (strncmp(choice, "vir_mem_stat", 12) == 0) {
		if (!vm_event_counters_enable) {
			printk("[%s] %d: CONFIG_VM_EVENT_COUNTERS is not configured\n", __func__, __LINE__);
			goto error;
		}

		for (i = 0; i < VIR_MEMSTAT_TABLE_SIZE; i++) {
			if (strncmp(event, vir_memstat_table[i].name, MAX_EVENT_NAME_LEN) == 0) {
				vir_memstat_mask |= (1 << vir_memstat_table[i].id);
				found_event = 1;

				break;
			}
		}
	} else {
		printk("[%s] %d: only support phy_mem_stat & vir_mem_stat keyword\n", __func__, __LINE__);
		goto error;
	}

	if (!found_event) {
		printk("[%s] %d: input event name error\n", __func__, __LINE__);
		goto error;
	}

	met_memstat.mode = 1;
	return 0;

error:
	met_memstat.mode = 0;
	return -EINVAL;
}

struct metdevice met_memstat = {
	.name = "memstat",
	.type = MET_TYPE_PMU,
	.cpu_related = 0,
	.start = met_memstat_start,
	.stop = met_memstat_stop,
	.polling_interval = 1,
	.timed_polling = met_memstat_polling,
	.tagged_polling = met_memstat_polling,
	.print_help = met_memstat_print_help,
	.print_header = met_memstat_print_header,
	.process_argument = met_memstat_process_argument
};
