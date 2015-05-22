#ifndef __CMDQ_REG_H__
#define __CMDQ_REG_H__

#include <mach/mt_reg_base.h>
#include <mach/sync_write.h>
#include "cmdq_core.h"

#define GCE_BASE_VA                  (GCE_BASE)
#define GCE_BASE_PA                  (0x10212000)

#define CMDQ_CORE_WARM_RESET         (GCE_BASE + 0x000)
#define CMDQ_CURR_IRQ_STATUS         (GCE_BASE + 0x010)
#define CMDQ_SECURE_IRQ_STATUS       (GCE_BASE + 0x014)
#define CMDQ_CURR_LOADED_THR         (GCE_BASE + 0x018)
#define CMDQ_THR_SLOT_CYCLES         (GCE_BASE + 0x030)
#define CMDQ_THR_EXEC_CYCLES         (GCE_BASE + 0x034)
#define CMDQ_THR_TIMEOUT_TIMER       (GCE_BASE + 0x038)
#define CMDQ_BUS_CONTROL_TYPE        (GCE_BASE + 0x040)
#define CMDQ_CURR_INST_ABORT         (GCE_BASE + 0x020)
#define CMDQ_CURR_REG_ABORT          (GCE_BASE + 0x050)

#define CMDQ_SECURITY_STA(id)        (GCE_BASE + (0x030 * id) + 0x024)
#define CMDQ_SECURITY_SET(id)        (GCE_BASE + (0x030 * id) + 0x028)
#define CMDQ_SECURITY_CLR(id)        (GCE_BASE + (0x030 * id) + 0x02C)

#define CMDQ_SYNC_TOKEN_ID           (GCE_BASE + 0x060)
#define CMDQ_SYNC_TOKEN_VAL          (GCE_BASE + 0x064)
#define CMDQ_SYNC_TOKEN_UPD          (GCE_BASE + 0x068)

#define CMDQ_GPR_R32(id)             (GCE_BASE + (0x004 * id) + 0x80)
#define CMDQ_GPR_R32_PA(id)          (GCE_BASE_PA + (0x004 * id) + 0x80)

#define CMDQ_THR_WARM_RESET(id)      (GCE_BASE + (0x080 * id) + 0x100)
#define CMDQ_THR_ENABLE_TASK(id)     (GCE_BASE + (0x080 * id) + 0x104)
#define CMDQ_THR_SUSPEND_TASK(id)    (GCE_BASE + (0x080 * id) + 0x108)
#define CMDQ_THR_CURR_STATUS(id)     (GCE_BASE + (0x080 * id) + 0x10C)
#define CMDQ_THR_IRQ_STATUS(id)      (GCE_BASE + (0x080 * id) + 0x110)
#define CMDQ_THR_IRQ_ENABLE(id)      (GCE_BASE + (0x080 * id) + 0x114)
#define CMDQ_THR_SECURITY(id)        (GCE_BASE + (0x080 * id) + 0x118)
#define CMDQ_THR_CURR_ADDR(id)       (GCE_BASE + (0x080 * id) + 0x120)
#define CMDQ_THR_END_ADDR(id)        (GCE_BASE + (0x080 * id) + 0x124)
#define CMDQ_THR_EXEC_CNT(id)        (GCE_BASE + (0x080 * id) + 0x128)
#define CMDQ_THR_WAIT_TOKEN(id)      (GCE_BASE + (0x080 * id) + 0x130)
#define CMDQ_THR_CFG(id)             (GCE_BASE + (0x080 * id) + 0x140)
#define CMDQ_THR_INST_CYCLES(id)     (GCE_BASE + (0x080 * id) + 0x150)
#define CMDQ_THR_INST_THRESX(id)     (GCE_BASE + (0x080 * id) + 0x154)

#define CMDQ_TEST_MMSYS_DUMMY_OFFSET (0x890)
#define CMDQ_APXGPT2_COUNT           (0x10008028)   // each count is 76ns

#define CMDQ_REG_GET32(addr)         ((*((volatile uint32_t*)(addr))) & 0xFFFFFFFF)
#define CMDQ_REG_GET16(addr)         ((*((volatile uint32_t*)(addr))) & 0x0000FFFF)

#define CMDQ_REG_GET64_GPR_PX(id)    cmdq_core_get_GPR64(id)

#define CMDQ_REG_SET32(addr, val)    mt65xx_reg_sync_writel(val, (addr) & 0xFFFFFFFF)


#endif  // __CMDQ_REG_H__
