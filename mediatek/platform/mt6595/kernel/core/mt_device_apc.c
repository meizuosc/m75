#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/xlog.h>
#include "mach/sync_write.h"
#include "mach/mt_device_apc.h"


DEFINE_SPINLOCK(g_mt_devapc_lock);
EXPORT_SYMBOL(g_mt_devapc_lock);

int mt_devapc_check_emi_violation(void)
{
     if ((readl(IOMEM(DEVAPC0_D0_VIO_STA_4)) & ABORT_EMI) == 0) 
    {
        pr_debug(KERN_INFO "Not EMI MPU violation.\n");
        return 1;
    }
    else{
        return 0;
    }

}

void mt_devapc_emi_initial(void)
{
    mt_reg_sync_writel(readl(IOMEM(DEVAPC0_APC_CON)) & (0xFFFFFFFF ^ (1<<2)), DEVAPC0_APC_CON);
    mt_reg_sync_writel(readl(IOMEM(DEVAPC0_PD_APC_CON)) & (0xFFFFFFFF ^ (1<<2)), DEVAPC0_PD_APC_CON);

    mt_reg_sync_writel(ABORT_EMI, DEVAPC0_D0_VIO_STA_4);
    mt_reg_sync_writel(readl(IOMEM(DEVAPC0_D0_VIO_MASK_4)) & (0xFFFFFFFF ^ (ABORT_EMI)), DEVAPC0_D0_VIO_MASK_4);
}


void mt_devapc_clear_emi_violation(void)
{
    if ((readl(IOMEM(DEVAPC0_D0_VIO_STA_4)) & ABORT_EMI) != 0)
    {
        mt_reg_sync_writel(ABORT_EMI, DEVAPC0_D0_VIO_STA_4);
    }
}
 /*
 * mt_devapc_set_permission: set module permission on device apc.
 * @module: the moudle to specify permission
 * @domain_num: domain index number
 * @permission_control: specified permission
 * no return value.
 */
void mt_devapc_set_permission(unsigned int module, E_MASK_DOM domain_num, APC_ATTR permission_control)
{
    unsigned long irq_flag;
    volatile unsigned int* base;
    unsigned int clr_bit = 0x3 << ((module % 16) * 2);
    unsigned int set_bit = permission_control << ((module % 16) * 2);

    if( module >= DEVAPC_DEVICE_NUMBER )
    {
        printk(KERN_WARNING "[DEVAPC] ERROR, device number %d exceeds the max number!\n", module);
        return;
    }

    if (DEVAPC_DOMAIN_AP  == domain_num)
    {
        base = DEVAPC0_D0_APC_0 + (module / 16) * 4;
    }
    else if (DEVAPC_DOMAIN_MD == domain_num)
    {
        base = DEVAPC0_D1_APC_0 + (module / 16) * 4;
    }
    else if (DEVAPC_DOMAIN_CONN == domain_num)
    {
        base = DEVAPC0_D2_APC_0 + (module / 16) * 4;
    }
    else if (DEVAPC_DOMAIN_MM == domain_num)
    {
        base = DEVAPC0_D3_APC_0 + (module / 16) * 4;
    }
    else
    {
        printk(KERN_WARNING "[DEVAPC] ERROR, domain number %d exceeds the max number!\n", domain_num);
        return;
    }
    
    spin_lock_irqsave(&g_mt_devapc_lock, irq_flag);

    mt_reg_sync_writel(readl(base) & ~clr_bit, base);
    mt_reg_sync_writel(readl(base) | set_bit, base);

    spin_unlock_irqrestore(&g_mt_devapc_lock, irq_flag);
    
    return;
}

EXPORT_SYMBOL(mt_devapc_emi_initial);
EXPORT_SYMBOL(mt_devapc_set_permission);
EXPORT_SYMBOL(mt_devapc_check_emi_violation);
EXPORT_SYMBOL(mt_devapc_clear_emi_violation);

