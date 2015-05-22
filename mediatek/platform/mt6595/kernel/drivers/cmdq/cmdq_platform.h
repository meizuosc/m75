#ifndef __CMDQ_PLATFORM_H__
#define __CMDQ_PLATFORM_H__

// platform dependent utilities, format: cmdq_{util_type}_{name}

#include "cmdq_def.h"
#include "cmdq_core.h"

/*
 * GCE capability
 */
const bool cmdq_core_support_sync_non_suspendable(void);
const bool cmdq_core_support_wait_and_receive_event_in_same_tick(void);

/**
 * Scenario related
 *
 */
bool cmdq_core_should_enable_prefetch(CMDQ_SCENARIO_ENUM scenario);
int cmdq_core_disp_thread_index_from_scenario(CMDQ_SCENARIO_ENUM scenario);
CMDQ_HW_THREAD_PRIORITY_ENUM cmdq_core_priority_from_scenario(CMDQ_SCENARIO_ENUM scenario);

/**
 * Module dependent
 *
 */
void cmdq_core_get_reg_id_from_hwflag(uint64_t hwflag, CMDQ_DATA_REGISTER_ENUM *valueRegId, CMDQ_DATA_REGISTER_ENUM *destRegId, CMDQ_EVENT_ENUM *regAccessToken);
const char* cmdq_core_module_from_event_id(CMDQ_EVENT_ENUM event, uint32_t instA, uint32_t instB);
const char* cmdq_core_parse_module_from_reg_addr(uint32_t reg_addr);
const int32_t cmdq_core_can_module_entry_suspend(EngineStruct *engineList);

/**
 * Debug
 *
 */
 
// use to generate [CMDQ_ENGINE_ENUM_id and name] mapping for status print 
#define CMDQ_FOREACH_STATUS_MODULE_PRINT(ACTION)\
    ACTION(CMDQ_ENG_ISP_IMGI,   ISP_IMGI) \
    ACTION(CMDQ_ENG_MDP_RDMA0,  MDP_RDMA0) \
    ACTION(CMDQ_ENG_MDP_RDMA1,  MDP_RDMA1) \
    ACTION(CMDQ_ENG_MDP_RSZ0,   MDP_RSZ0) \
    ACTION(CMDQ_ENG_MDP_RSZ1,   MDP_RSZ1) \
    ACTION(CMDQ_ENG_MDP_RSZ2,   MDP_RSZ2) \
    ACTION(CMDQ_ENG_MDP_TDSHP0, MDP_TDSHP0) \
    ACTION(CMDQ_ENG_MDP_TDSHP1, MDP_TDSHP1) \
    ACTION(CMDQ_ENG_MDP_WROT0,  MDP_WROT0) \
    ACTION(CMDQ_ENG_MDP_WROT1,  MDP_WROT1) \
    ACTION(CMDQ_ENG_MDP_WDMA,   MDP_WDMA)


void cmdq_core_dump_mmsys_config(void);
void cmdq_core_dump_clock_gating(void);

ssize_t cmdq_core_print_event(char *buf);
void cmdq_core_print_event_seq(struct seq_file *m);

/**
 * Record usage
 *
 */
uint64_t cmdq_rec_flag_from_scenario(CMDQ_SCENARIO_ENUM scn);

#endif // #ifndef __CMDQ_PLATFORM_H__
