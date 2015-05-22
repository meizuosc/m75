
//#include <asm/system.h>
#include <linux/smp.h>
#include "cpu_pmu.h"
#include "v8_pmu_name.h"

/*
 * Per-CPU PMCR: config reg
 */
#define ARMV8_PMCR_E		(1 << 0) /* Enable all counters */
#define ARMV8_PMCR_P		(1 << 1) /* Reset all counters */
#define ARMV8_PMCR_C		(1 << 2) /* Cycle counter reset */
#define ARMV8_PMCR_D		(1 << 3) /* CCNT counts every 64th cpu cycle */
#define ARMV8_PMCR_X		(1 << 4) /* Export to ETM */
#define ARMV8_PMCR_DP		(1 << 5) /* Disable CCNT if non-invasive debug*/
#define	ARMV8_PMCR_N_SHIFT	11		/* Number of counters supported */
#define	ARMV8_PMCR_N_MASK	0x1f
#define	ARMV8_PMCR_MASK		0x3f		/* Mask for writable bits */

/*
 * PMOVSR: counters overflow flag status reg
 */
#define	ARMV8_OVSR_MASK		0xffffffff	/* Mask for writable bits */
#define	ARMV8_OVERFLOWED_MASK	ARMV8_OVSR_MASK


enum ARM_TYPE {
	CORTEX_A53 = 0xD03,
	CHIP_UNKNOWN = 0xFFF
};

struct chip_pmu {
	enum ARM_TYPE type;
	struct pmu_desc *desc;
	unsigned int count;
	const char *cpu_name;
};

static struct chip_pmu chips[] = {
	{CORTEX_A53, a53_pmu_desc, A53_PMU_DESC_COUNT, "Cortex-A7L"},
};
static struct chip_pmu chip_unknown = { CHIP_UNKNOWN, NULL, 0, "Unkown CPU" };

#define CHIP_PMU_COUNT (sizeof(chips) / sizeof(struct chip_pmu))

static struct chip_pmu *chip;

static enum ARM_TYPE armv8_get_ic(void)
{
	unsigned int value;
	/* Read Main ID Register */
	asm("mrs %0, midr_el1":"=r" (value));

	value = (value & 0xffff) >> 4;	/* primary part number */
	return value;
}

static inline void armv8_pmu_counter_select(unsigned int idx)
{
	asm volatile("msr pmselr_el0, %0" :  : "r" (idx));
	isb();
}

static inline void armv8_pmu_type_select(unsigned int idx, unsigned int type)
{
	armv8_pmu_counter_select(idx);
	asm volatile("msr pmxevtyper_el0, %0" :  : "r" (type));
}

static inline unsigned int armv8_pmu_read_count(unsigned int idx)
{
	unsigned int value;

	if (idx == 31) {
		asm volatile("mrs %0, pmccntr_el0" : "=r" (value));
	} else {
		armv8_pmu_counter_select(idx);
		asm volatile("mrs %0, pmxevcntr_el0" : "=r" (value));
	}
	return value;
}

static inline void armv8_pmu_write_count(int idx, u32 value)
{
	if (idx == 31) {
		asm volatile("msr pmccntr_el0, %0" :  : "r" (value));
	} else {
		armv8_pmu_counter_select(idx);
		asm volatile("msr pmxevcntr_el0, %0" :  : "r" (value));
	}
}

static inline void armv8_pmu_enable_count(unsigned int idx)
{
	asm volatile ("msr pmcntenset_el0, %0" :  : "r" (1 << idx));
}

static inline void armv8_pmu_disable_count(unsigned int idx)
{
	asm volatile ("msr pmcntenclr_el0, %0" :  : "r" (1 << idx));
}

static inline void armv8_pmu_enable_intr(unsigned int idx)
{
	asm volatile ("msr pmintenset_el1, %0" :  : "r" (1 << idx));
}

static inline void armv8_pmu_disable_intr(unsigned int idx)
{
	asm volatile("msr pmintenclr_el1, %0" :: "r" (1 << idx));
	isb();
	asm volatile ("msr pmovsclr_el0, %0" :  : "r" (1 << idx));
	isb();
}

static inline unsigned int armv8_pmu_overflow(void)
{
	unsigned int val;
	asm volatile ("mrs %0, pmovsclr_el0":"=r" (val));	/* read */
	val &= ARMV8_OVSR_MASK;
	asm volatile ("mrs %0, pmovsclr_el0" :  : "r" (val));
	return val;
}

static inline unsigned int armv8_pmu_control_read(void)
{
	unsigned int val;
	asm volatile("mrs %0, pmcr_el0" : "=r" (val));
	return val;
}

static inline void armv8_pmu_control_write(u32 val)
{
	val &= ARMV8_PMCR_MASK;
	isb();
	asm volatile("msr pmcr_el0, %0" :: "r" (val));
}

static int armv8_pmu_hw_get_counters(void)
{
	int count = armv8_pmu_control_read();
	/* N, bits[15:11] */
	count = ((count >> ARMV8_PMCR_N_SHIFT) & ARMV8_PMCR_N_MASK);
	return count;
}

static void armv8_pmu_hw_reset_all(int generic_counters)
{
	int i;
	armv8_pmu_control_write(ARMV8_PMCR_C | ARMV8_PMCR_P);
	/* generic counter */
	for (i = 0; i < generic_counters; i++) {
		armv8_pmu_disable_intr(i);
		armv8_pmu_disable_count(i);
	}
	/* cycle counter */
	armv8_pmu_disable_intr(31);
	armv8_pmu_disable_count(31);
	armv8_pmu_overflow();	/* clear overflow */
}

static int armv8_pmu_hw_get_event_desc(int i, int event, char* event_desc)
{
	if (NULL == event_desc) {
		return -1;
	}

	for (i = 0; i < chip->count; i++) {
		if (chip->desc[i].event == event) {
			strcpy(event_desc, chip->desc[i].name);
			break;
		}
	}
	if (i == chip->count)
		return -1;

	return 0;
}

static int armv8_pmu_hw_check_event(struct met_pmu *pmu, int idx, int event)
{
	int i;

	/* Check if event is duplicate */
	for (i = 0; i < idx; i++) {
		if (pmu[i].event == event)
			break;
	}
	if (i < idx) {
		/* printk("++++++ found duplicate event 0x%02x i=%d\n", event, i); */
		return -1;
	}

	for (i = 0; i < chip->count; i++) {
		if (chip->desc[i].event == event)
			break;
	}

	if (i == chip->count)
		return -1;

	return 0;
}

static void armv8_pmu_hw_start(struct met_pmu *pmu, int count)
{
	int i;
	int generic = count - 1;

	armv8_pmu_hw_reset_all(generic);
	for (i = 0; i < generic; i++) {
		if (pmu[i].mode == MODE_POLLING) {
			armv8_pmu_type_select(i, pmu[i].event);
			armv8_pmu_enable_count(i);
		}
	}
	if (pmu[count - 1].mode == MODE_POLLING) {	/* cycle counter */
		armv8_pmu_enable_count(31);
	}
	armv8_pmu_control_write(ARMV8_PMCR_E);
}

static void armv8_pmu_hw_stop(int count)
{
	int generic = count - 1;
	armv8_pmu_hw_reset_all(generic);
}

static unsigned int armv8_pmu_hw_polling(struct met_pmu *pmu, int count, unsigned int *pmu_value)
{
	int i, cnt = 0;
	int generic = count - 1;

	for (i = 0; i < generic; i++) {
		if (pmu[i].mode == MODE_POLLING) {
			pmu_value[cnt] = armv8_pmu_read_count(i);
			cnt++;
		}
	}
	if (pmu[count - 1].mode == MODE_POLLING) {
		pmu_value[cnt] = armv8_pmu_read_count(31);
		cnt++;
	}
	armv8_pmu_control_write(ARMV8_PMCR_C | ARMV8_PMCR_P | ARMV8_PMCR_E);

	return cnt;
}


struct cpu_pmu_hw armv8_pmu = {
	.name = "armv8_pmu",
	.get_event_desc = armv8_pmu_hw_get_event_desc,
	.check_event = armv8_pmu_hw_check_event,
	.start = armv8_pmu_hw_start,
	.stop = armv8_pmu_hw_stop,
	.polling = armv8_pmu_hw_polling,
};

struct cpu_pmu_hw* cpu_pmu_hw_init(void)
{
	int i;
	enum ARM_TYPE type;

	type = armv8_get_ic();
	for (i = 0; i < CHIP_PMU_COUNT; i++) {
		if (chips[i].type == type) {
			chip = &(chips[i]);
			break;
		}
	}
	if (i == CHIP_PMU_COUNT) {
		chip = &chip_unknown;
		return NULL;
	}

	armv8_pmu.nr_cnt = armv8_pmu_hw_get_counters() + 1;
	armv8_pmu.cpu_name = chip->cpu_name;

	return &armv8_pmu;
}

