#ifndef __CMDQ_DEVICE_H__
#define __CMDQ_DEVICE_H__

#include <linux/platform_device.h>
#include <linux/device.h>

struct device* cmdq_dev_get(void);
const uint32_t cmdq_dev_get_irq_id(void);
const uint32_t cmdq_dev_get_irq_secure_id(void);
const long cmdq_dev_get_module_base_VA_GCE(void);
const long cmdq_dev_get_module_base_PA_GCE(void);

const long cmdq_dev_get_module_base_VA_MMSYS_CONFIG(void);
const long cmdq_dev_get_module_base_VA_MDP_RDMA(void);
const long cmdq_dev_get_module_base_VA_MDP_RSZ0(void);
const long cmdq_dev_get_module_base_VA_MDP_RSZ1(void);
const long cmdq_dev_get_module_base_VA_MDP_WDMA(void);
const long cmdq_dev_get_module_base_VA_MDP_WROT(void);
const long cmdq_dev_get_module_base_VA_MDP_TDSHP(void);
const long cmdq_dev_get_module_base_VA_MM_MUTEX(void);
const long cmdq_dev_get_module_base_VA_VENC(void);

const long cmdq_dev_alloc_module_base_VA_by_name(const char* name);
void cmdq_dev_free_module_base_VA(const long VA);

void cmdq_dev_init(struct platform_device *pDevice);
void cmdq_dev_deinit(void);

#endif // __CMDQ_DEVICE_H__
