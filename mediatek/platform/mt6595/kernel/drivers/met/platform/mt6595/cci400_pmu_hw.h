#ifndef _CCI400_PMU_HW_H_
#define _CCI400_PMU_HW_H_

#define MAX_EVENT_COUNT 4
//#define PROBE_CCI400_CYCLES

#define REG_PMCR	0x0100

#define PERI_ID2	0x0FE8	// for r?p?

#define COMP_ID0	0x0FF0	// 0x0d
#define COMP_ID1	0x0FF4	// 0xf0
#define COMP_ID2	0x0FF8	// 0x05
#define COMP_ID3	0x0FFC	// 0xb1

#define REG_CNTR_CYC	0x9000
#define REG_CNTR0_BASE	0xA000
#define REG_CNTR1_BASE	0xB000
#define REG_CNTR2_BASE	0xC000
#define REG_CNTR3_BASE	0xD000

#define EVENT_SELECT	0x0
#define EVENT_COUNT	0x4
#define COUNTER_CTL	0x8
#define OVERFLOW_FLAG	0xC

struct event_select {
	unsigned int event:5;
	unsigned int interface:3;
	unsigned int unused:24;
};

struct cci400_pmu_t {
	union {
		struct event_select evt_sel;
		unsigned int value;
	};
};

int cci400_pmu_hw_init(void);
unsigned char cci400_pmu_hw_rev(void);
void cci400_pmu_hw_uninit(void);

void cci400_pmu_hw_start(struct cci400_pmu_t *ccipmu, int count);
void cci400_pmu_hw_stop(void);
void cci400_pmu_hw_polling(struct cci400_pmu_t *ccipmu, int count, unsigned int *pmu_value);

#endif // _CCI400_PMU_HW_H_
