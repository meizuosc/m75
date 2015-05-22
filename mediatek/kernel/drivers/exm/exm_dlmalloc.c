#include <mach/memory.h>

/* EXM */
#include <linux/module.h> 
#include <asm/io.h> 
#include <linux/slab.h>
#include <linux/exm_driver.h> 
#include <linux/platform_device.h>
#include <linux/mm.h>

#include "exm_dlmalloc.h"

// Ugly inclusion of C file so that bionic specific #defines configure dlmalloc.
#include "malloc.c"

#define DEV_DRV_NAME "mt-extmem"

//#define EXTMEM_DEBUG
#ifdef EXTMEM_DEBUG
#define extmem_printk(fmt, args...) printk(fmt, ## args)
#else
#define extmem_printk(...) 
#endif

static mspace extmem_mspace = NULL;
static void * extmem_mspace_base = NULL;
static size_t extmem_mspace_size = 0;

extern unsigned int get_max_phys_addr(void);
extern unsigned int get_actual_DRAM_size(void);

static void extmem_init(void) {
    if (extmem_mspace == NULL) {
        if (extmem_mspace_size == 0) {
			// 0x9E000000 is the spare address
            extmem_mspace_size = get_actual_DRAM_size() - CONFIG_MAX_DRAM_SIZE_SUPPORT - 0x02000000;
		}
        //extmem_mspace_base = (void*) ioremap(get_max_phys_addr(), extmem_mspace_size);
        extmem_mspace_base = (void*) ioremap_cached(get_max_phys_addr(), extmem_mspace_size);
        extmem_mspace = create_mspace_with_base(extmem_mspace_base, extmem_mspace_size, 1);
        //printk(KERN_ERR "[LCH_DEBUG]get_actual_DRAM_size:0x%x, CONFIG_MAX_DRAM_SIZE_SUPPORT:0x%x, get_max_phys_addr:0x%x, extmem_mspace:0x%x\n", 
        //           get_actual_DRAM_size(), CONFIG_MAX_DRAM_SIZE_SUPPORT, get_max_phys_addr(), extmem_mspace);
    }
}

void* extmem_malloc(size_t bytes) {
    extmem_init();
    extmem_printk("[EXT_MEM] %s size: 0x%x\n", __FUNCTION__, bytes);
    return mspace_malloc(extmem_mspace, bytes);
}
EXPORT_SYMBOL(extmem_malloc);

void* extmem_malloc_page_align(size_t bytes) {
    extmem_init();
    extmem_printk("[EXT_MEM] %s size: 0x%x\n", __FUNCTION__, bytes);
    return mspace_memalign(extmem_mspace, 1<<PAGE_SHIFT, bytes);
}
EXPORT_SYMBOL(extmem_malloc_page_align);

void extmem_free(void* mem) {
    extmem_printk("[EXT_MEM] %s addr:0x%x\n", __FUNCTION__, mem);
    if (extmem_mspace != NULL)
        mspace_free(extmem_mspace, mem);
}
EXPORT_SYMBOL(extmem_free);

static unsigned long get_phys_from_mspace(unsigned long va)
{
    return ( va - (unsigned long)extmem_mspace_base + get_max_phys_addr());
}

unsigned long get_virt_from_mspace(unsigned long pa)
{
    return ( pa - get_max_phys_addr() + (unsigned long)extmem_mspace_base );
}
EXPORT_SYMBOL(get_virt_from_mspace);

static void extmem_vma_close(struct vm_area_struct *vma)
{
	//if (extmem_in_mspace(vma))
	extmem_free((void *)get_virt_from_mspace((vma->vm_pgoff << PAGE_SHIFT)));
}

static const struct vm_operations_struct exm_vm_ops = {
	.close = extmem_vma_close,
};

bool extmem_in_mspace(struct vm_area_struct *vma)
{
    return (vma->vm_ops == &exm_vm_ops);
}
EXPORT_SYMBOL(extmem_in_mspace);

size_t extmem_get_mem_size(unsigned long pgoff)
{
    void * va = (void *)get_virt_from_mspace(pgoff << PAGE_SHIFT);
    mchunkptr p  = mem2chunk(va);
    size_t psize = chunksize(p) - TWO_SIZE_T_SIZES;

    extmem_printk("[EXT_MEM] %s size: 0x%x\n", __FUNCTION__, psize);
    return psize;
}
EXPORT_SYMBOL(extmem_get_mem_size);


static int mtk_mspace_mmap_physical(struct exm_info *info, struct vm_area_struct *vma)
{
    unsigned long size = vma->vm_end - vma->vm_start;
    void * va = NULL;
    unsigned long pa;
    int ret = -EINVAL;

    if ((vma->vm_flags & VM_SHARED) == 0) {
        return -EINVAL;
    }

    vma->vm_ops = &exm_vm_ops;
    va = extmem_malloc_page_align(size);

    if (!va) {
        printk("[EXT_MEM] %s[%d] malloc failed...", __FILE__, __LINE__);
        return -ENOMEM;
    }

    memset(va, 0, size);

    vma->vm_flags |= (VM_DONTCOPY | VM_DONTEXPAND);

	vma->vm_page_prot = __pgprot_modify(vma->vm_page_prot, L_PTE_MT_MASK, L_PTE_MT_WRITEBACK);

    pa = get_phys_from_mspace((unsigned long) va);
    ret = remap_pfn_range(vma,
    		       vma->vm_start,
    		       (pa >> PAGE_SHIFT),
    		       size,
    		       vma->vm_page_prot);
    extmem_printk("[EXT_MEM] pa:0x%x, va:0x%x, vma->vm_pgoff:0x%x, vm_start:0x%x, vm_end:0x%x\n", pa, va, vma->vm_pgoff, vma->vm_start, vma->vm_end);

    return ret;
}

static int __init mt_mspace_probe(struct platform_device *dev) 
{ 
    //struct resource *regs;
    struct exm_info *info;

    extmem_printk("[EXT_MEM] probing mt_mspace\n");
    info = kzalloc(sizeof(struct exm_info), GFP_KERNEL);
    if (!info) 
        return -ENOMEM;

    extmem_init();

    info->mem[0].addr = get_max_phys_addr();
    info->mem[0].size = extmem_mspace_size;
    info->mmap = mtk_mspace_mmap_physical;

    if (!info->mem[0].addr) {
        dev_err(&dev->dev, "Invalid memory resource\n");
        return -ENODEV;
    }

    info->version = "0.0.2";
    info->name= DEV_DRV_NAME;

    if (exm_register_device(&dev->dev, info)) {
        iounmap(info->mem[0].internal_addr);
        printk("[EXT_MEM] exm_register failed\n");
        return -ENODEV;
    }
    platform_set_drvdata(dev, info);
    printk("[EXT_MEM] probing mt_mspace success\n");

    return 0;
} 

static int __exit mt_mspace_remove(struct platform_device *dev) 
{ 
    struct exm_info *info = platform_get_drvdata(dev);

    exm_unregister_device(info);
    platform_set_drvdata(dev, NULL);
    iounmap(info->mem[0].internal_addr);
    kfree(info);
    return 0;
}

static struct platform_driver mt_mspace_driver = {
    .probe = mt_mspace_probe,
    .remove = mt_mspace_remove,
    .driver = {
        .name = DEV_DRV_NAME,
    },
};

static int __init mt_mspace_init(void)
{
    return platform_driver_register(&mt_mspace_driver);
}

static void __exit mt_mspace_exit(void)
{
    platform_driver_unregister(&mt_mspace_driver);
}

module_init(mt_mspace_init);
module_exit(mt_mspace_exit);

MODULE_LICENSE("GPL");

