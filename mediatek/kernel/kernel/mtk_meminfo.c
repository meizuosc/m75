#include <asm/page.h>
#include <linux/module.h> 

// return the size of memory from start pfn to max pfn,
// _NOTE_
// the memory area may be discontinuous
extern unsigned long max_pfn;
phys_addr_t get_memory_size (void)
{
    return (phys_addr_t)(max_pfn << PAGE_SHIFT);
}
EXPORT_SYMBOL(get_memory_size);

// return the actual physical DRAM size
// wrapper function of mtk_get_max_DRAM_size
extern phys_addr_t mtk_get_max_DRAM_size(void);
phys_addr_t get_max_DRAM_size(void)
{
        return mtk_get_max_DRAM_size();
}
EXPORT_SYMBOL(get_max_DRAM_size);
