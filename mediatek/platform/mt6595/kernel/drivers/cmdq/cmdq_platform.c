#include "cmdq_platform.h"
#include "cmdq_core.h"
#include "cmdq_reg.h"
#include "mach/mt_clkmgr.h"

#include <linux/vmalloc.h>
#include <linux/seq_file.h>
#include <mach/mt_boot.h>

typedef struct RegDef{
    int offset;
    const char* name;
} RegDef;

const bool cmdq_core_support_sync_non_suspendable(void)
{
    return true; 
}

const bool cmdq_core_support_wait_and_receive_event_in_same_tick(void)
{
    CHIP_SW_VER ver = mt_get_chip_sw_ver();
    bool support = false; 
    
    if (CHIP_SW_VER_02 <= ver)
    {
        // SW V2
        support = true;
    }
    else if (CHIP_SW_VER_01 <= ver)
    {
        support = false;
    }

    return support;
}

bool cmdq_core_should_enable_prefetch(CMDQ_SCENARIO_ENUM scenario)
{
#if 0
    switch (scenario)
    {
    case CMDQ_SCENARIO_PRIMARY_DISP:
    case CMDQ_SCENARIO_PRIMARY_ALL:
    case CMDQ_SCENARIO_DEBUG_PREFETCH:  // HACK: force debug into 0/1 thread
        // any path that connects to Primary DISP HW
        // should enable prefetch.
        // MEMOUT scenarios does not.
        // Also, since thread 0/1 shares one prefetch buffer,
        // we allow only PRIMARY path to use prefetch.
        return true;

    default:
        return false;
    }
#endif

    return false;
}

int cmdq_core_disp_thread_index_from_scenario(CMDQ_SCENARIO_ENUM scenario)
{
    if (cmdq_core_should_enable_prefetch(scenario))
    {
        return 0;
    }

    switch (scenario)
    {
    case CMDQ_SCENARIO_PRIMARY_DISP:
    case CMDQ_SCENARIO_PRIMARY_ALL:
    case CMDQ_SCENARIO_DISP_CONFIG_AAL:
    case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_GAMMA:
    case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_DITHER:
    case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_PWM:
    case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_PQ:
    case CMDQ_SCENARIO_DISP_CONFIG_OD:
    case CMDQ_SCENARIO_RDMA0_DISP:
    case CMDQ_SCENARIO_DEBUG_PREFETCH: // HACK: force debug into 0/1 thread
        // primary config: thread 0
        return 0;

    case CMDQ_SCENARIO_SUB_DISP:
    case CMDQ_SCENARIO_SUB_ALL:
    case CMDQ_SCENARIO_MHL_DISP:
    case CMDQ_SCENARIO_RDMA2_DISP:
    case CMDQ_SCENARIO_DISP_CONFIG_SUB_GAMMA:
    case CMDQ_SCENARIO_DISP_CONFIG_SUB_DITHER:
    case CMDQ_SCENARIO_DISP_CONFIG_SUB_PQ:
    case CMDQ_SCENARIO_DISP_CONFIG_SUB_PWM:
        return 1;

    case CMDQ_SCENARIO_DISP_ESD_CHECK:
        return 2;
        
    case CMDQ_SCENARIO_DISP_SCREEN_CAPTURE:
        return 3;

    default:
        // freely dispatch
        return CMDQ_INVALID_THREAD;
    }

    // freely dispatch
    return CMDQ_INVALID_THREAD;
}

CMDQ_HW_THREAD_PRIORITY_ENUM cmdq_core_priority_from_scenario(CMDQ_SCENARIO_ENUM scenario)
{
    switch (scenario)
    {
    case CMDQ_SCENARIO_PRIMARY_DISP:
    case CMDQ_SCENARIO_PRIMARY_ALL:
    case CMDQ_SCENARIO_SUB_DISP:
    case CMDQ_SCENARIO_SUB_ALL:
    case CMDQ_SCENARIO_MHL_DISP:
    case CMDQ_SCENARIO_RDMA0_DISP:
    case CMDQ_SCENARIO_RDMA2_DISP:
    case CMDQ_SCENARIO_DISP_CONFIG_AAL ... CMDQ_SCENARIO_DISP_CONFIG_OD:
        // currently, a prefetch thread is always in high priority.
        return CMDQ_THR_PRIO_DISPLAY_CONFIG;


    // HACK: force debug into 0/1 thread
    case CMDQ_SCENARIO_DEBUG_PREFETCH:
        return CMDQ_THR_PRIO_DISPLAY_CONFIG;


    case CMDQ_SCENARIO_DISP_ESD_CHECK:
    case CMDQ_SCENARIO_DISP_SCREEN_CAPTURE:
        return CMDQ_THR_PRIO_DISPLAY_ESD;

    default:
        // other cases need exta logic, see below.
        break;
    }

    if (CMDQ_SCENARIO_TRIGGER_LOOP == scenario)
    {
        return CMDQ_THR_PRIO_DISPLAY_TRIGGER;
    }
    else
    {
        return CMDQ_THR_PRIO_NORMAL;
    }
}


void cmdq_core_get_reg_id_from_hwflag(uint64_t hwflag, CMDQ_DATA_REGISTER_ENUM *valueRegId, CMDQ_DATA_REGISTER_ENUM *destRegId, CMDQ_EVENT_ENUM *regAccessToken)
{
    *regAccessToken = CMDQ_SYNC_TOKEN_INVALID;

    if (hwflag & (1LL << CMDQ_ENG_JPEG_ENC))
    {
        *valueRegId = CMDQ_DATA_REG_JPEG;
        *destRegId = CMDQ_DATA_REG_JPEG_DST;
        *regAccessToken = CMDQ_SYNC_TOKEN_GPR_SET_0;
    }
    else if (hwflag & (1LL << CMDQ_ENG_MDP_TDSHP0))
    {
        *valueRegId = CMDQ_DATA_REG_2D_SHARPNESS_0;
        *destRegId = CMDQ_DATA_REG_2D_SHARPNESS_0_DST;
        *regAccessToken = CMDQ_SYNC_TOKEN_GPR_SET_1;
    }
    else if (hwflag & (1LL << CMDQ_ENG_MDP_TDSHP1))
    {
        *valueRegId = CMDQ_DATA_REG_2D_SHARPNESS_1;
        *destRegId = CMDQ_DATA_REG_2D_SHARPNESS_1_DST;
        *regAccessToken = CMDQ_SYNC_TOKEN_GPR_SET_2;
    }
    else if (hwflag & ((1LL << CMDQ_ENG_DISP_COLOR0 |
                  (1LL << CMDQ_ENG_DISP_COLOR1))))
    {
        *valueRegId = CMDQ_DATA_REG_PQ_COLOR;
        *destRegId = CMDQ_DATA_REG_PQ_COLOR_DST;
        *regAccessToken = CMDQ_SYNC_TOKEN_GPR_SET_3;
    }
    else
    {
        // assume others are debug cases
        *valueRegId = CMDQ_DATA_REG_DEBUG;
        *destRegId = CMDQ_DATA_REG_DEBUG_DST;
        *regAccessToken = CMDQ_SYNC_TOKEN_GPR_SET_4;
    }

    return;
}


const char* cmdq_core_module_from_event_id(CMDQ_EVENT_ENUM event, uint32_t instA, uint32_t instB)
{
    const char *module = "CMDQ";

    switch(event)
    {
    case CMDQ_EVENT_DISP_RDMA0_SOF:
    case CMDQ_EVENT_DISP_RDMA1_SOF:
    case CMDQ_EVENT_DISP_RDMA2_SOF:
    case CMDQ_EVENT_DISP_RDMA0_EOF:
    case CMDQ_EVENT_DISP_RDMA1_EOF:
    case CMDQ_EVENT_DISP_RDMA2_EOF:
    case CMDQ_EVENT_DISP_RDMA0_UNDERRUN:
    case CMDQ_EVENT_DISP_RDMA1_UNDERRUN:
    case CMDQ_EVENT_DISP_RDMA2_UNDERRUN:
        module = "DISP_RDMA";
        break;

    case CMDQ_EVENT_DISP_WDMA0_SOF:
    case CMDQ_EVENT_DISP_WDMA1_SOF:
    case CMDQ_EVENT_DISP_WDMA0_EOF:
    case CMDQ_EVENT_DISP_WDMA1_EOF:
        module = "DISP_WDMA";
        break;

    case CMDQ_EVENT_DISP_OVL0_SOF:
    case CMDQ_EVENT_DISP_OVL1_SOF:
    case CMDQ_EVENT_DISP_OVL0_EOF:
    case CMDQ_EVENT_DISP_OVL1_EOF:
        module = "DISP_OVL";
        break;

    case CMDQ_EVENT_MDP_DSI0_TE_SOF:
    case CMDQ_EVENT_MDP_DSI1_TE_SOF:
    case CMDQ_EVENT_DISP_COLOR0_SOF ... CMDQ_EVENT_DISP_OD_SOF:
    case CMDQ_EVENT_DISP_COLOR0_EOF ... CMDQ_EVENT_DISP_DPI0_EOF:
    case CMDQ_EVENT_MUTEX0_STREAM_EOF ... CMDQ_EVENT_MUTEX4_STREAM_EOF:
    case CMDQ_SYNC_TOKEN_CONFIG_DIRTY:
    case CMDQ_SYNC_TOKEN_STREAM_EOF:
        module = "DISP";
        break;

    case CMDQ_EVENT_MDP_RDMA0_SOF:
    case CMDQ_EVENT_MDP_RDMA1_SOF:
    case CMDQ_EVENT_MDP_MVW_SOF ... CMDQ_EVENT_MDP_CROP_SOF:
    case CMDQ_EVENT_MDP_RDMA0_EOF ... CMDQ_EVENT_MDP_CROP_EOF:
    case CMDQ_EVENT_MUTEX5_STREAM_EOF ... CMDQ_EVENT_MUTEX9_STREAM_EOF:
        module = "MDP";
        break;

    case CMDQ_EVENT_ISP_PASS2_2_EOF ... CMDQ_EVENT_ISP_PASS1_0_EOF:
    case CMDQ_EVENT_ISP_CAMSV_2_PASS1_DONE ... CMDQ_EVENT_ISP_SENINF_CAM0_FULL:
        module = "ISP";
        break;

    case CMDQ_EVENT_JPEG_ENC_PASS2_EOF ... CMDQ_EVENT_JPEG_DEC_EOF:
        module = "JPGENC";
        break;

    default:
        module = "CMDQ";
        break;
    }

    return module;
}


const char* cmdq_core_parse_module_from_reg_addr(uint32_t reg_addr)
{
    const uint32_t addr_base_and_page = (reg_addr & 0xFFFFF000);
    const uint32_t addr_base_shifted = (reg_addr & 0xFFFF0000) >> 16;
    const char *module = "CMDQ";

    // for well-known base, we check them with 12-bit mask
    // defined in mt_reg_base.h
#define DECLARE_REG_RANGE(base, name) case IO_VIRT_TO_PHYS(base): return #name;
    switch(addr_base_and_page)
    {
    DECLARE_REG_RANGE(MDP_RDMA0_BASE, MDP);
    DECLARE_REG_RANGE(MDP_RDMA1_BASE, MDP);
    DECLARE_REG_RANGE(MDP_RSZ0_BASE, MDP);
    DECLARE_REG_RANGE(MDP_RSZ1_BASE, MDP);
    DECLARE_REG_RANGE(MDP_RSZ2_BASE, MDP);
    DECLARE_REG_RANGE(MDP_WDMA_BASE, MDP);
    DECLARE_REG_RANGE(MDP_WROT0_BASE, MDP);
    DECLARE_REG_RANGE(MDP_WROT1_BASE, MDP);
    DECLARE_REG_RANGE(MDP_TDSHP0_BASE, MDP);
    DECLARE_REG_RANGE(MDP_TDSHP1_BASE, MDP);
    DECLARE_REG_RANGE(MDP_CROP_BASE, MDP);
    DECLARE_REG_RANGE(COLOR0_BASE, COLOR);
    DECLARE_REG_RANGE(COLOR1_BASE, COLOR);
    DECLARE_REG_RANGE(OVL0_BASE, OVL0);
    DECLARE_REG_RANGE(OVL1_BASE, OVL1);
    DECLARE_REG_RANGE(DISP_AAL_BASE, AAL);
    DECLARE_REG_RANGE(DISP_GAMMA_BASE, AAL);
    DECLARE_REG_RANGE(VENC_BASE, VENC);
    DECLARE_REG_RANGE(JPGENC_BASE, JPGENC);
    DECLARE_REG_RANGE(JPGDEC_BASE, JPGDEC);
    }
#undef DECLARE_REG_RANGE

    // for other register address we rely on GCE subsys to group them with
    // 16-bit mask.
#undef DECLARE_CMDQ_SUBSYS
#define DECLARE_CMDQ_SUBSYS(msb, id, grp, base) case msb: return #grp;
    switch(addr_base_shifted)
    {
    #include "cmdq_subsys.h"
    }
#undef DECLARE_CMDQ_SUBSYS
    return module;
}

const int32_t cmdq_core_can_module_entry_suspend(EngineStruct *engineList)
{
    int32_t status = 0;
    int i;
    CMDQ_ENG_ENUM e = 0;

    CMDQ_ENG_ENUM mdpEngines[] = {CMDQ_ENG_ISP_IMGI   
                                  ,CMDQ_ENG_MDP_RDMA0  
                                  ,CMDQ_ENG_MDP_RDMA1  
                                  ,CMDQ_ENG_MDP_RSZ0   
                                  ,CMDQ_ENG_MDP_RSZ1   
                                  ,CMDQ_ENG_MDP_RSZ2   
                                  ,CMDQ_ENG_MDP_TDSHP0 
                                  ,CMDQ_ENG_MDP_TDSHP1 
                                  ,CMDQ_ENG_MDP_WROT0  
                                  ,CMDQ_ENG_MDP_WROT1  
                                  ,CMDQ_ENG_MDP_WDMA};   

    for (i = 0; i < (sizeof(mdpEngines) / sizeof(CMDQ_ENG_ENUM)); ++i)
    {
        e = mdpEngines[i];
        if (0 != engineList[e].userCount)
        {
            CMDQ_ERR("suspend but engine %d has userCount %d, owner=%d\n", 
                      e,
                      engineList[e].userCount, 
                      engineList[e].currOwner);
            status = -EBUSY;
        }
    }
    
    return status;
}

void cmdq_core_dump_mmsys_config(void)
{
    int i = 0;
    uint32_t value = 0;

    const static struct RegDef configRegisters[] = {
            {0x01c, "ISP_MOUT_EN"},
            {0x020, "MDP_RDMA_MOUT_EN"},
            {0x024, "MDP_PRZ0_MOUT_EN"},
            {0x028, "MDP_PRZ1_MOUT_EN"},
            {0x02C, "MDP_TDSHP_MOUT_EN"},
            {0x030, "DISP_OVL0_MOUT_EN"},
            {0x034, "DISP_OVL1_MOUT_EN"},
            {0x038, "DISP_DITHER_MOUT_EN"},
            {0x03C, "DISP_UFOE_MOUT_EN"},
            //{0x040, "MMSYS_MOUT_RST"},
            {0x044, "MDP_PRZ0_SEL_IN"},
            {0x048, "MDP_PRZ1_SEL_IN"},
            {0x04C, "MDP_TDSHP_SEL_IN"},
            {0x050, "MDP_WDMA_SEL_IN"},
            {0x054, "MDP_WROT_SEL_IN"},
            {0x058, "DISP_COLOR_SEL_IN"},
            {0x05C, "DISP_WDMA_SEL_IN"},
            {0x060, "DISP_UFOE_SEL_IN"},
            {0x064, "DSI0_SEL_IN"},
            {0x068, "DPI0_SEL_IN"},
            {0x06C, "DISP_RDMA0_SOUT_SEL_IN"},
            {0x070, "DISP_RDMA1_SOUT_SEL_IN"},
            {0x0F0, "MMSYS_MISC"},
            // ACK and REQ related
            {0x8a0, "DISP_DL_VALID_0"},
            {0x8a4, "DISP_DL_VALID_1"},
            {0x8a8, "DISP_DL_READY_0"},
            {0x8ac, "DISP_DL_READY_1"},
            {0x8b0, "MDP_DL_VALID_0"},
            {0x8b4, "MDP_DL_READY_0"}
    };

    for(i = 0; i < sizeof(configRegisters) / sizeof(configRegisters[0]); ++i)
    {
        value = CMDQ_REG_GET16(MMSYS_CONFIG_BASE + configRegisters[i].offset);
        CMDQ_ERR("%s: 0x%08x\n", configRegisters[i].name, value);
    }

    return;
}

void cmdq_core_dump_clock_gating(void)
{
    uint32_t value[3] = {0};
    
    value[0] = CMDQ_REG_GET32(MMSYS_CONFIG_BASE + 0x100);
    value[1] = CMDQ_REG_GET32(MMSYS_CONFIG_BASE + 0x110);
    value[2] = CMDQ_REG_GET32(MMSYS_CONFIG_BASE + 0x890);
    CMDQ_ERR("MMSYS_CG_CON0(deprecated): 0x%08x, MMSYS_CG_CON1: 0x%08x\n", value[0], value[1]);
    CMDQ_ERR("MMSYS_DUMMY_REG: 0x%08x\n", value[2]);
#ifndef CONFIG_MTK_FPGA
    CMDQ_ERR("ISPSys clock state %d\n", subsys_is_on(SYS_ISP));
    CMDQ_ERR("DisSys clock state %d\n", subsys_is_on(SYS_DIS));
    CMDQ_ERR("VDESys clock state %d\n", subsys_is_on(SYS_VDE));
#endif
}

uint64_t cmdq_rec_flag_from_scenario(CMDQ_SCENARIO_ENUM scn)
{
    uint64_t flag = 0;

    switch(scn)
    {
    case CMDQ_SCENARIO_JPEG_DEC:
        flag = (1LL << CMDQ_ENG_JPEG_DEC);
        break;
    case CMDQ_SCENARIO_PRIMARY_DISP:
        flag = (1LL << CMDQ_ENG_DISP_OVL0)  |
               (1LL << CMDQ_ENG_DISP_COLOR0) |
               (1LL << CMDQ_ENG_DISP_AAL) |
               (1LL << CMDQ_ENG_DISP_RDMA0) |
               (1LL << CMDQ_ENG_DISP_UFOE) |
               (1LL << CMDQ_ENG_DISP_DSI0_CMD);
        break;
    case CMDQ_SCENARIO_PRIMARY_MEMOUT:
        flag = ((1LL << CMDQ_ENG_DISP_OVL0) |
                (1LL << CMDQ_ENG_DISP_WDMA0));
        break;
    case CMDQ_SCENARIO_PRIMARY_ALL:
        flag = ((1LL << CMDQ_ENG_DISP_OVL0) |
                (1LL << CMDQ_ENG_DISP_WDMA0) |
                (1LL << CMDQ_ENG_DISP_COLOR0) |
                (1LL << CMDQ_ENG_DISP_AAL) |
                (1LL << CMDQ_ENG_DISP_RDMA0) |
                (1LL << CMDQ_ENG_DISP_UFOE) |
                (1LL << CMDQ_ENG_DISP_DSI0_CMD));
        break;
    case CMDQ_SCENARIO_SUB_DISP:
        flag = ((1LL << CMDQ_ENG_DISP_OVL1) |
                (1LL << CMDQ_ENG_DISP_COLOR1) |
                (1LL << CMDQ_ENG_DISP_GAMMA) |
                (1LL <<  CMDQ_ENG_DISP_RDMA1) |
                (1LL << CMDQ_ENG_DISP_DSI1_CMD));
        break;
    case CMDQ_SCENARIO_SUB_MEMOUT:
        flag = ((1LL << CMDQ_ENG_DISP_OVL1) |
                (1LL << CMDQ_ENG_DISP_WDMA1));
        break;
    case CMDQ_SCENARIO_SUB_ALL:
         flag = ((1LL << CMDQ_ENG_DISP_OVL1) |
                 (1LL << CMDQ_ENG_DISP_WDMA1) |
                 (1LL << CMDQ_ENG_DISP_COLOR1) |
                 (1LL << CMDQ_ENG_DISP_GAMMA) |
                 (1LL << CMDQ_ENG_DISP_RDMA1) |
                 (1LL << CMDQ_ENG_DISP_DSI1_CMD));
        break;
    case CMDQ_SCENARIO_MHL_DISP:
        flag = ((1LL << CMDQ_ENG_DISP_OVL1) |
                (1LL << CMDQ_ENG_DISP_COLOR1) |
                (1LL << CMDQ_ENG_DISP_GAMMA) |
                (1LL << CMDQ_ENG_DISP_RDMA1) |
                (1LL << CMDQ_ENG_DISP_DPI));
        break;
    case CMDQ_SCENARIO_RDMA0_DISP:
        flag = ((1LL << CMDQ_ENG_DISP_RDMA0) |
                (1LL << CMDQ_ENG_DISP_UFOE) |
                (1LL << CMDQ_ENG_DISP_DSI0_CMD));
        break;
    case CMDQ_SCENARIO_RDMA2_DISP:
        flag = ((1LL << CMDQ_ENG_DISP_RDMA2) |
                (1LL << CMDQ_ENG_DISP_DPI));
        break;
    case CMDQ_SCENARIO_TRIGGER_LOOP:
    case CMDQ_SCENARIO_DISP_CONFIG_AAL:
    case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_GAMMA:
    case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_DITHER:
    case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_PWM:
    case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_PQ:
    case CMDQ_SCENARIO_DISP_CONFIG_SUB_GAMMA:
    case CMDQ_SCENARIO_DISP_CONFIG_SUB_DITHER:
    case CMDQ_SCENARIO_DISP_CONFIG_SUB_PWM:
    case CMDQ_SCENARIO_DISP_CONFIG_SUB_PQ:
    case CMDQ_SCENARIO_DISP_CONFIG_OD:
        // Trigger loop does not related to any HW by itself.
        flag = 0LL;
        break;

    case CMDQ_SCENARIO_USER_SPACE:
        // user space case, engine flag is passed seprately
        flag = 0LL;
        break;

    case CMDQ_SCENARIO_DEBUG:
    case CMDQ_SCENARIO_DEBUG_PREFETCH:
        flag = 0LL;
        break;

    case CMDQ_SCENARIO_DISP_ESD_CHECK:
    case CMDQ_SCENARIO_DISP_SCREEN_CAPTURE:
        // ESD check uses separate thread (not config, not trigger)
        flag = 0LL;
        break;

    default:
        CMDQ_ERR("Unknown scenario type %d\n", scn);
        flag = 0LL;
        break;
    }

    return flag;
}

ssize_t cmdq_core_print_event(char *buf)
{
	char *pBuffer = buf;
	const uint32_t events[] = {CMDQ_SYNC_TOKEN_CABC_EOF, 
				   CMDQ_SYNC_TOKEN_CONFIG_DIRTY,
				   CMDQ_EVENT_MDP_DSI0_TE_SOF,
				   CMDQ_SYNC_TOKEN_STREAM_EOF};
	uint32_t eventValue = 0;
	int i = 0;
	for (i = 0; i < (sizeof(events) / sizeof(uint32_t)); ++i)
	{
		const uint32_t e = 0x3FF & events[i];
		CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_ID, e);
		eventValue = CMDQ_REG_GET32(CMDQ_SYNC_TOKEN_VAL);
		pBuffer += sprintf(pBuffer, "%s = %d\n", 
				cmdq_core_get_event_name(e),
				eventValue);
	}

	return (pBuffer - buf);
}

void cmdq_core_print_event_seq(struct seq_file *m)
{
	const uint32_t events[] = {CMDQ_SYNC_TOKEN_CABC_EOF, 
					   CMDQ_SYNC_TOKEN_CONFIG_DIRTY,
					   CMDQ_EVENT_MDP_DSI0_TE_SOF,
					   CMDQ_SYNC_TOKEN_STREAM_EOF};
	uint32_t eventValue = 0;
	int i = 0;
	seq_puts(m, "====== Events =======\n");
	
	for (i = 0; i < (sizeof(events) / sizeof(uint32_t)); ++i)
	{
		const uint32_t e = 0x3FF & events[i];
		CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_ID, e);
		eventValue = CMDQ_REG_GET32(CMDQ_SYNC_TOKEN_VAL);
		seq_printf(m, 
			"[EVENT]%s = %d\n", 
			cmdq_core_get_event_name(e),
			eventValue);	
	}
}
