#include "cmdq_device.h"
#include "cmdq_core.h"
#include <mach/mt_irq.h>

#if 0
// device tree
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/io.h>
#endif 

typedef struct CmdqDeviceStruct
{
    struct device *pDev;
#if 0
    long regBaseVA;    // considering 64 bit kernel, use long
    long regBasePA;
    uint32_t irqId;
    uint32_t irqSecId;
#endif
} CmdqDeviceStruct;

static CmdqDeviceStruct gCmdqDev;

struct device* cmdq_dev_get(void)
{
    return gCmdqDev.pDev;
}

const uint32_t cmdq_dev_get_irq_id(void)
{
    return CQ_DMA_IRQ_BIT_ID;
}

const uint32_t cmdq_dev_get_irq_secure_id(void)
{
    return CQ_DMA_SEC_IRQ_BIT_ID;
}

void cmdq_dev_init_module_base_VA(void)
{
    return;
}

void cmdq_dev_deinit_module_base_VA(void)
{
    return;
}

void cmdq_dev_init(struct platform_device *pDevice)
{
    memset(&gCmdqDev, 0x0, sizeof(CmdqDeviceStruct));

    if(pDevice)
    {
        gCmdqDev.pDev = &pDevice->dev;
    }
    
    return;
}

void cmdq_dev_deinit(void)
{
    return;
}

