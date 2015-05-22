#ifndef __CMDQ_MDP_H__
#define __CMDQ_MDP_H__

#include <linux/types.h>

#ifdef __cplusplus
extern "C" {
#endif

int32_t cmdqMdpClockOn(uint64_t engineFlag);

int32_t cmdqMdpDumpInfo(uint64_t engineFlag,
                        int level);

int32_t cmdqVEncDumpInfo(uint64_t engineFlag,
                        int level);

int32_t cmdqMdpResetEng(uint64_t engineFlag);

int32_t cmdqMdpClockOff(uint64_t engineFlag);

#ifdef __cplusplus
}
#endif

#endif  // __CMDQ_MDP_H__
