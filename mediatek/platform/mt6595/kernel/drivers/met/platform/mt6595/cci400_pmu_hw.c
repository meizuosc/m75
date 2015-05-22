#include <asm/system.h>
#include <linux/io.h>

#include "cci400_pmu_hw.h"

#define CCR (1 << 2)
#define RST (1 << 1)
#define CEN (1 << 0)

#define CCI400_REG_BASE	0x10390000 // 6592
//#define CCI400_REG_BASE	0x10320000 // 8135
//#define CCI400_REG_BASE	0x2c090000 // TC2

#define CCI400_REG_LEN	0x00010000 // 64K

#ifdef PROBE_CCI400_CYCLES
	#define PROBE_BASE_IDX	0
#else
	#define PROBE_BASE_IDX	1
#endif

static void __iomem *cci400_iobase = NULL;
static unsigned char cci400_rev = 0;

static unsigned int cntr_base[] = {
	REG_CNTR_CYC,
	REG_CNTR0_BASE,
	REG_CNTR1_BASE,
	REG_CNTR2_BASE,
	REG_CNTR3_BASE
};

static inline unsigned int cci400_reg_read(unsigned int reg_offset)
{
	unsigned int value = readl(cci400_iobase + reg_offset);
	mb();
	return value;
}

static inline void cci400_reg_write(unsigned int reg_offset, unsigned int value)
{
	writel(value, cci400_iobase + reg_offset);
	// make sure writel() be completed before outer_sync()
	mb();
	outer_sync();
	// make sure outer_sync() be completed before following readl();
	mb();
}

void cci400_pmu_hw_stop(void)
{
	int i;

	// disable and reset all counter count
	cci400_reg_write(REG_PMCR, CCR | RST);

	// reset all counters (include cycle counter)
	for (i=0; i<=MAX_EVENT_COUNT; i++) {
		cci400_reg_write(cntr_base[i] + COUNTER_CTL, 0); // disable
		cci400_reg_write(cntr_base[i] + OVERFLOW_FLAG, 1); // clear overflow
	}
}

void cci400_pmu_hw_start(struct cci400_pmu_t *ccipmu, int count)
{
	int i;

	cci400_pmu_hw_stop();

	for (i=PROBE_BASE_IDX; i<count; i++) {
		if (i != 0) {
			// select event
			cci400_reg_write(cntr_base[i] + EVENT_SELECT, ccipmu[i].value);
		}
		// enable it
		cci400_reg_write(cntr_base[i] + COUNTER_CTL, 1);
	}

	// enable all counters
	cci400_reg_write(REG_PMCR, CEN);
}

void cci400_pmu_hw_polling(struct cci400_pmu_t *ccipmu, int count, unsigned int *pmu_value)
{
	int i; // NOTE: must be "signed"

	for (i=count-1; i>=PROBE_BASE_IDX; i--) {
		pmu_value[i] = cci400_reg_read(cntr_base[i] + EVENT_COUNT);
		if (cci400_reg_read(cntr_base[i] + OVERFLOW_FLAG) & 1) {
			pmu_value[i] = 0xffffffff;
		}
	}

	// disable and reset all counter counts
	cci400_reg_write(REG_PMCR, CCR | RST);
	// re-enable all counters
	cci400_reg_write(REG_PMCR, CEN);

	return;
}

static int cci400_pmu_hw_probe(void)
{
#if 0 // touch ID registers may crash some systems
	int id[4];

	id[0] = cci400_reg_read(COMP_ID0) & 0xff;
	id[1] = cci400_reg_read(COMP_ID1) & 0xff;
	id[2] = cci400_reg_read(COMP_ID2) & 0xff;
	id[3] = cci400_reg_read(COMP_ID3) & 0xff;

	if ( (id[0] != 0x0d) ||
	     (id[1] != 0xf0) ||
	     (id[2] != 0x05) ||
	     (id[3] != 0xb1) ) {
		return -1;
	}
	cci400_rev = (cci400_reg_read(PERI_ID2) >> 4) & 0xf;
#else
	cci400_rev = 0x6; // r1p1
#endif

	return 0;
}

unsigned char cci400_pmu_hw_rev(void)
{
	return cci400_rev;
}

int cci400_pmu_hw_init(void)
{
	if (cci400_iobase == NULL) {
		cci400_iobase = ioremap(CCI400_REG_BASE, CCI400_REG_LEN);
		if (cci400_iobase == NULL) {
			return -1;
		}
	}

	if (cci400_pmu_hw_probe() < 0) {
		iounmap(cci400_iobase);
		cci400_iobase = NULL;
		return -1;
	}

	cci400_pmu_hw_stop();
	return 0;
}

void cci400_pmu_hw_uninit(void)
{
	if (cci400_iobase != NULL) {
		cci400_pmu_hw_stop();
		iounmap(cci400_iobase);
		cci400_iobase = NULL;
	}
}
