
#ifndef _V6_PMU_NAME_H_
#define _V6_PMU_NAME_H_

/* ARM11 */
struct pmu_desc arm11_pmu_desc[] = {
        { 0x00, "ICACHE_MISS" },
        { 0x01, "IBUF_STALL" },
        { 0x02, "DDEP_STALL" },
        { 0x03, "ITLB_MISS" },
        { 0x04, "DTLB_MISS" },
        { 0x05, "BR_EXEC" },
        { 0x06, "BR_MISPREDICT" },
        { 0x07, "CPU_INST" },
        { 0x09, "DCACHE_HIT" },
        { 0x0A, "L1D_CACHE" },
        { 0x0B, "L1D_CACHE_REFILL" },
        { 0x0C, "DCACHE_WBACK" },
        { 0x0D, "SW_PC_CHANGE" },
        { 0x0F, "MAIN_TLB_MISS" },
        { 0x10, "EXPL_D_ACCESS" },
        { 0x11, "LSU_FULL_STALL" },
        { 0x12, "WBUF_DRAINED" },
        { 0xFF, "CPU_CYCLES" },
        { 0x20, "NOP" },
};

#define ARM11_PMU_DESC_COUNT (sizeof(arm11_pmu_desc) / sizeof(struct pmu_desc))

#endif // _V6_PMU_NAME_H_
