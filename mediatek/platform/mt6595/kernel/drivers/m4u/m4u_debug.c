#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/module.h>

#include "m4u_priv.h"


//global variables
int gM4U_log_to_uart = 0;
int gM4U_log_level = 2;
int gM4U_debug_trigger_translation_fault = 0;


int m4u_test_alloc_dealloc(int id, unsigned int size)
{
    m4u_client_t *client;
    unsigned int va;
    unsigned int mva;
    int ret;
    unsigned long populate;

    if (id == 1)
        va = (unsigned int) kmalloc(size, GFP_KERNEL);
    else if (id == 2)
        va = (unsigned int) vmalloc(size);
    else if (id == 3)
    {
        down_write(&current->mm->mmap_sem);
        va = do_mmap_pgoff(NULL, 0, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_LOCKED, 0, &populate);
        up_write(&current->mm->mmap_sem);
    }

    M4UMSG("test va=0x%x,size=0x%x\n", va, size);

    client = m4u_create_client();
    if (IS_ERR_OR_NULL(client))
    {
        M4UMSG("create client fail!\n");
    }

    ret = m4u_alloc_mva(client, M4U_PORT_DISP_OVL0, va, NULL, size, 
            M4U_PROT_READ | M4U_PROT_CACHE, 0, &mva);
    if (ret)
    {
        M4UMSG("alloc mva fail:va=0x%x,size=0x%x,ret=%d\n", va,size,ret);
        return -1;
    }
    m4u_dump_pgtable(m4u_get_domain_by_port(M4U_PORT_DISP_OVL0), NULL);

    ret = m4u_dealloc_mva(client, M4U_PORT_DISP_OVL0, mva);
    m4u_dump_pgtable(m4u_get_domain_by_port(M4U_PORT_DISP_OVL0), NULL);

    if (id == 1)
        kfree((void *)va);
    else if (id == 2)
        vfree((void *)va);
    else if (id == 3)
    {
        down_read(&current->mm->mmap_sem);
        ret = do_munmap(current->mm, va, size);
        up_read(&current->mm->mmap_sem);
        if (ret)
        {
            M4UMSG("do_munmap failed\n");
        }
    }

//clean
    m4u_destroy_client(client);
    return 0;
}


m4u_callback_ret_t m4u_test_callback(int alloc_port, unsigned int mva, 
    unsigned int size, void* data)
{
    printk("test callback port=%d, mva=0x%x, size=0x%x, data=%d\n", alloc_port, mva,
        size, (int)data);
    return M4U_CALLBACK_HANDLED;
}


int m4u_test_reclaim(unsigned int size)
{
    m4u_client_t *client;
    unsigned int va[10], buf_size;
    unsigned int mva;
    int ret, i;

    //register callback
    m4u_register_reclaim_callback(M4U_PORT_DISP_OVL0, m4u_test_callback, NULL);


    client = m4u_create_client();
    if (IS_ERR_OR_NULL(client))
    {
        M4UMSG("createclientfail!\n");
    }

    buf_size = size;
    for (i = 0; i < 10; i++)
    {
        va[i] = (unsigned int) vmalloc(buf_size);

        ret = m4u_alloc_mva(client, M4U_PORT_DISP_OVL0, va[i], NULL, buf_size, M4U_PROT_READ | M4U_PROT_CACHE, 0, &mva);
        if (ret)
        {
            M4UMSG("alloc using kmalloc fail:va=0x%x,size=0x%x\n", va[i], buf_size);
            return -1;
        }
        M4UMSG("alloc mva:va=0x%x,mva=0x%x,size=0x%x\n", va[i], mva, buf_size);

        buf_size += size;
    }


    for (i = 0; i < 10; i++)
        vfree((void *)va[i]);

    m4u_dump_buf_info(NULL);
    m4u_dump_pgtable(m4u_get_domain_by_port(M4U_PORT_DISP_OVL0), NULL);

    m4u_destroy_client(client);

    m4u_unregister_reclaim_callback(M4U_PORT_DISP_OVL0);
    m4u_unregister_reclaim_callback(M4U_PORT_AAO);
    m4u_unregister_reclaim_callback(M4U_PORT_GCE);

    return 0;
}

static int m4u_test_map_kernel(void)
{
    m4u_client_t *client;
    unsigned int va, size=1024*1024;
    unsigned int mva;
    unsigned int kernel_va, kernel_size;
    int i;
    int ret;
    unsigned long populate;

    down_write(&current->mm->mmap_sem);
    va = do_mmap_pgoff(NULL, 0, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_LOCKED, 0, &populate);
    up_write(&current->mm->mmap_sem);

    M4UMSG("test va=0x%x,size=0x%x\n", va, size);

    for(i=0; i<size; i+=4)
    {
        *(int*)(va+i) = i;
    }

    client = m4u_create_client();
    if (IS_ERR_OR_NULL(client))
    {
        M4UMSG("createclientfail!\n");
    }

    ret = m4u_alloc_mva(client, M4U_PORT_DISP_OVL0, va, NULL, size, M4U_PROT_READ | M4U_PROT_CACHE, 0, &mva);
    if (ret)
    {
        M4UMSG("alloc using kmalloc fail:va=0x%x,size=0x%x\n", va, size);
        return -1;
    }

    ret = m4u_mva_map_kernel(mva, size, &kernel_va, &kernel_size);
    if(ret)
    {
        M4UMSG("map kernel fail!\n");
        return -1;
    }
    for(i=0; i<size; i+=4)
    {
        if(*(int*)(kernel_va+i) != i)
        {
            M4UMSG("wawawa, get map value fail! i=%d, map=%d\n", i, *(int*)(kernel_va+i));
        }
    }

    ret = m4u_mva_unmap_kernel(mva, size, kernel_va);

    ret = m4u_dealloc_mva(client, M4U_PORT_DISP_OVL0, mva);
    down_read(&current->mm->mmap_sem);
    ret = do_munmap(current->mm, va, size);
    up_read(&current->mm->mmap_sem);
    if (ret)
    {
        M4UMSG("do_munmap failed\n");
    }

    m4u_destroy_client(client);
    return 0;
}

__attribute__((weak)) extern int ddp_mem_test(void);
__attribute__((weak)) extern int __ddp_mem_test(unsigned int *pSrc, unsigned char *pSrcPa, 
                            unsigned int* pDst, unsigned char* pDstPa,
                            int need_sync);

int m4u_test_ddp(unsigned int prot)
{
    unsigned int *pSrc, *pDst;
    unsigned int src_pa, dst_pa;
    unsigned int size = 64*64*3;
    M4U_PORT_STRUCT port;
    m4u_client_t * client = m4u_create_client();

    pSrc = vmalloc(size);
    pDst = vmalloc(size);

    m4u_alloc_mva(client, M4U_PORT_DISP_OVL0, (unsigned int)pSrc, NULL, 
        size, prot, 0, &src_pa);

    m4u_alloc_mva(client, M4U_PORT_DISP_OVL0, (unsigned int)pDst, NULL, 
        size, prot, 0, &dst_pa);

    M4UMSG("pSrc=0x%x, pDst=0x%x, src_pa=0x%x, dst_pa=0x%x\n", pSrc, pDst, src_pa, dst_pa);

    port.ePortID = M4U_PORT_DISP_OVL0;
    port.Direction = 0;
    port.Distance = 1;
    port.domain = 3;
    port.Security = 0;
    port.Virtuality = 1;
    m4u_config_port(&port);

    port.ePortID = M4U_PORT_DISP_WDMA0;
    m4u_config_port(&port);
    
    m4u_monitor_start(0);
    __ddp_mem_test(pSrc, (void*)src_pa, pDst, (void*)dst_pa, !(prot & M4U_PROT_CACHE));
    m4u_monitor_stop(0);

    vfree(pSrc);
    vfree(pDst);

    m4u_destroy_client(client);
    return 0;
}

m4u_callback_ret_t test_fault_callback(int port, unsigned int mva, void* data)
{
    printk("fault call port=%d, mva=0x%x, data=%d\n", port, mva, (int)data);
    /* DO NOT print too much logs here !!!! */
    /* Do NOT use any lock hear !!!!*/
    /* DO NOT do any other things except printk !!!*/
    /* DO NOT make any mistake here (or reboot will happen) !!! */
    return M4U_CALLBACK_HANDLED;
}


int m4u_test_tf(unsigned int prot)
{
    unsigned int *pSrc, *pDst;
    unsigned int src_pa, dst_pa;
    unsigned int size = 64*64*3;
    M4U_PORT_STRUCT port;
    m4u_client_t * client = m4u_create_client();


    m4u_register_fault_callback(M4U_PORT_DISP_OVL0, test_fault_callback, (void*)88);
    m4u_register_fault_callback(M4U_PORT_DISP_WDMA0, test_fault_callback, (void*)88);

    pSrc = vmalloc(size);
    pDst = vmalloc(size);

    m4u_alloc_mva(client, M4U_PORT_DISP_OVL0, (unsigned int)pSrc, NULL, 
        size, prot, 0, &src_pa);

    m4u_alloc_mva(client, M4U_PORT_DISP_OVL0, (unsigned int)pDst, NULL, 
        size/2, prot, 0, &dst_pa);

    M4UMSG("pSrc=0x%x, pDst=0x%x, src_pa=0x%x, dst_pa=0x%x\n", pSrc, pDst, src_pa, dst_pa);

    port.ePortID = M4U_PORT_DISP_OVL0;
    port.Direction = 0;
    port.Distance = 1;
    port.domain = 3;
    port.Security = 0;
    port.Virtuality = 1;
    m4u_config_port(&port);

    port.ePortID = M4U_PORT_DISP_WDMA0;
    m4u_config_port(&port);
    
    m4u_monitor_start(0);
    __ddp_mem_test(pSrc, (void*)src_pa, pDst, (void*)dst_pa, !!(prot & M4U_PROT_CACHE));
    m4u_monitor_stop(0);


    m4u_dealloc_mva(client, M4U_PORT_DISP_OVL0, src_pa);
    m4u_dealloc_mva(client, M4U_PORT_DISP_OVL0, dst_pa);
    
    vfree(pSrc);
    vfree(pDst);

    m4u_destroy_client(client);

    return 0;
}


#include <linux/ion_drv.h>
#if 0
int m4u_test_ion()
{
    unsigned int *pSrc, *pDst;
    unsigned int src_pa, dst_pa;
    unsigned int size = 64*64*3, tmp_size;
    M4U_PORT_STRUCT port;
    struct ion_mm_data mm_data;
    struct ion_client *ion_client;
    struct ion_handle *src_handle, *dst_handle;

    //FIX-ME: modified for linux-3.10 early porting
    //ion_client = ion_client_create(g_ion_device, 0xffffffff, "test");
    ion_client = ion_client_create(g_ion_device, "test");

    src_handle = ion_alloc(ion_client, size, 0, ION_HEAP_MULTIMEDIA_MASK, 0);
    dst_handle = ion_alloc(ion_client, size, 0, ION_HEAP_MULTIMEDIA_MASK, 0);

    pSrc = ion_map_kernel(ion_client, src_handle);
    pDst = ion_map_kernel(ion_client, dst_handle);

    mm_data.config_buffer_param.handle = src_handle;
    mm_data.config_buffer_param.m4u_port= M4U_PORT_DISP_OVL0;
    mm_data.config_buffer_param.prot = M4U_PROT_READ|M4U_PROT_WRITE;
    mm_data.config_buffer_param.flags = M4U_FLAGS_SEQ_ACCESS;
    mm_data.mm_cmd = ION_MM_CONFIG_BUFFER;
    if (ion_kernel_ioctl(ion_client, ION_CMD_MULTIMEDIA, (unsigned long)&mm_data) < 0)
    {
        printk("ion_test_drv: Config buffer failed.\n");
    }
    mm_data.config_buffer_param.handle = dst_handle;
    if (ion_kernel_ioctl(ion_client, ION_CMD_MULTIMEDIA, (unsigned long)&mm_data) < 0)
    {
        printk("ion_test_drv: Config buffer failed.\n");
    }
    
    ion_phys(ion_client, src_handle, &src_pa, &tmp_size);
    ion_phys(ion_client, dst_handle, &dst_pa, &tmp_size);
    

    M4UMSG("pSrc=0x%x, pDst=0x%x, src_pa=0x%x, dst_pa=0x%x\n", pSrc, pDst, src_pa, dst_pa);

    port.ePortID = M4U_PORT_DISP_OVL0;
    port.Direction = 0;
    port.Distance = 1;
    port.domain = 3;
    port.Security = 0;
    port.Virtuality = 1;
    m4u_config_port(&port);

    port.ePortID = M4U_PORT_DISP_WDMA0;
    m4u_config_port(&port);
    
    m4u_monitor_start(0);
    __ddp_mem_test(pSrc, (void*)src_pa, pDst, (void*)dst_pa, 0);
    m4u_monitor_stop(0);


    ion_free(ion_client, src_handle);
    ion_free(ion_client, dst_handle);

    ion_client_destroy(ion_client);

}


#endif


static int m4u_debug_set(void *data, u64 val)
{
    m4u_domain_t *domain = data;

    M4UMSG("m4u_debug_set:val=%llu\n", val);

    switch (val)
    {
        case 1:
            {                   //map4kpageonly
                struct sg_table table;
                struct sg_table *sg_table = &table;
                struct scatterlist *sg;
                int i;
                struct page *page;
                int page_num = 512;
                unsigned int mva = 0x4000;

                page = alloc_pages(GFP_KERNEL, get_order(page_num));
                sg_alloc_table(sg_table, page_num, GFP_KERNEL);
                for_each_sg(sg_table->sgl, sg, sg_table->nents, i)
                {
                    sg_set_page(sg, page + i, PAGE_SIZE, 0);
                }
                m4u_map_sgtable(domain, mva, sg_table, page_num * PAGE_SIZE, M4U_PROT_WRITE | M4U_PROT_READ);
                m4u_dump_pgtable(domain, NULL);
                m4u_unmap(domain, mva, page_num * PAGE_SIZE);
                m4u_dump_pgtable(domain, NULL);

                sg_free_table(sg_table);
                __free_pages(page, get_order(page_num));

            }
            break;
        case 2:
            {                   //map64kpageonly
                struct sg_table table;
                struct sg_table *sg_table = &table;
                struct scatterlist *sg;
                int i;
                int page_num = 51;
                unsigned int page_size = SZ_64K;
                unsigned int mva = SZ_64K;

                sg_alloc_table(sg_table, page_num, GFP_KERNEL);
                for_each_sg(sg_table->sgl, sg, sg_table->nents, i)
                {
                    sg_dma_address(sg) = page_size * (i + 1);
                    sg_dma_len(sg) = page_size;
                }

                m4u_map_sgtable(domain, mva, sg_table, page_num * page_size, M4U_PROT_WRITE | M4U_PROT_READ);
                m4u_dump_pgtable(domain, NULL);
                m4u_unmap(domain, mva, page_num * page_size);
                m4u_dump_pgtable(domain, NULL);
                sg_free_table(sg_table);
            }
            break;

        case 3:
            {                   //map1Mpageonly
                struct sg_table table;
                struct sg_table *sg_table = &table;
                struct scatterlist *sg;
                int i;
                int page_num = 37;
                unsigned int page_size = SZ_1M;
                unsigned int mva = SZ_1M;

                sg_alloc_table(sg_table, page_num, GFP_KERNEL);

                for_each_sg(sg_table->sgl, sg, sg_table->nents, i)
                {
                    sg_dma_address(sg) = page_size * (i + 1);
                    sg_dma_len(sg) = page_size;
                }
                m4u_map_sgtable(domain, mva, sg_table, page_num * page_size, M4U_PROT_WRITE | M4U_PROT_READ);
                m4u_dump_pgtable(domain, NULL);
                m4u_unmap(domain, mva, page_num * page_size);
                m4u_dump_pgtable(domain, NULL);

                sg_free_table(sg_table);

            }
            break;

        case 4:
            {                   //map16Mpageonly
                struct sg_table table;
                struct sg_table *sg_table = &table;
                struct scatterlist *sg;
                int i;
                int page_num = 2;
                unsigned int page_size = SZ_16M;
                unsigned int mva = SZ_16M;

                sg_alloc_table(sg_table, page_num, GFP_KERNEL);
                for_each_sg(sg_table->sgl, sg, sg_table->nents, i)
                {
                    sg_dma_address(sg) = page_size * (i + 1);
                    sg_dma_len(sg) = page_size;
                }
                m4u_map_sgtable(domain, mva, sg_table, page_num * page_size, M4U_PROT_WRITE | M4U_PROT_READ);
                m4u_dump_pgtable(domain, NULL);
                m4u_unmap(domain, mva, page_num * page_size);
                m4u_dump_pgtable(domain, NULL);
                sg_free_table(sg_table);
            }
            break;

        case 5:
            {                   //mapmiscpages
                struct sg_table table;
                struct sg_table *sg_table = &table;
                struct scatterlist *sg;
                unsigned int mva = 0x4000;
                unsigned int size = SZ_16M * 2;

                sg_alloc_table(sg_table, 1, GFP_KERNEL);
                sg = sg_table->sgl;
                sg_dma_address(sg) = 0x4000;
                sg_dma_len(sg) = size;

                m4u_map_sgtable(domain, mva, sg_table, size, M4U_PROT_WRITE | M4U_PROT_READ);
                m4u_dump_pgtable(domain, NULL);
                m4u_unmap(domain, mva, size);
                m4u_dump_pgtable(domain, NULL);
                sg_free_table(sg_table);

            }
            break;

        case 6:
            {
                m4u_test_alloc_dealloc(1, SZ_4M);
            }
            break;

        case 7:
            {
                m4u_test_alloc_dealloc(2, SZ_4M);
            }
            break;

        case 8:
            {
                m4u_test_alloc_dealloc(3, SZ_4M);
            }
            break;

        case 9:                //m4u_alloc_mvausingkmallocbuffer
            {
                m4u_test_reclaim(SZ_16K);
            }
            break;

        case 10:
            {
                unsigned int mva;
                mva = m4u_do_mva_alloc_fix(0x90000000, 0x10000000, NULL);
                M4UMSG("mva alloc fix done:mva=0x%x\n", mva);
                mva = m4u_do_mva_alloc_fix(0xb0000000, 0x10000000, NULL);
                M4UMSG("mva alloc fix done:mva=0x%x\n", mva);
                mva = m4u_do_mva_alloc_fix(0xa0000000, 0x10000000, NULL);
                M4UMSG("mva alloc fix done:mva=0x%x\n", mva);
                mva = m4u_do_mva_alloc_fix(0xa4000000, 0x10000000, NULL);
                M4UMSG("mva alloc fix done:mva=0x%x\n", mva);
                m4u_mvaGraph_dump();
                m4u_do_mva_free(0x90000000, 0x10000000);
                m4u_do_mva_free(0xa0000000, 0x10000000);
                m4u_do_mva_free(0xb0000000, 0x10000000);
                m4u_mvaGraph_dump();
            }
            break;

        case 11:    //map unmap kernel
            m4u_test_map_kernel();
        break;

        case 12:
            ddp_mem_test();
            break;

        case 13:
            m4u_test_ddp(M4U_PROT_READ|M4U_PROT_WRITE);
            break;
        case 14:
            m4u_test_tf(M4U_PROT_READ|M4U_PROT_WRITE);
            break;
        case 15:
            //m4u_test_ion();
            break;
        case 16:
            gM4U_debug_trigger_translation_fault = 1;
            break;
        case 17:
            gM4U_debug_trigger_translation_fault = 0;
            break;

        default:
            M4UMSG("m4u_debug_set error,val=%llu\n", val);
    }

    return 0;
}

static int m4u_debug_get(void *data, u64 * val)
{
    *val = 0;
    return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(m4u_debug_fops, m4u_debug_get, m4u_debug_set, "%llu\n");


static int m4u_log_level_set(void *data, u64 val)
{
    if(val & 0xf0)
    {
        gM4U_log_to_uart = 1;
    }
    else
    {
        gM4U_log_to_uart = 0;
    }
    
    gM4U_log_level = val & 0xf;

    return 0;
}

static int m4u_log_level_get(void *data, u64 * val)
{
    *val = gM4U_log_level | (gM4U_log_to_uart<<4);

    return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(m4u_log_level_fops, m4u_log_level_get, m4u_log_level_set, "%llu\n");


int m4u_debug_port_show(struct seq_file *s, void *unused)
{
    m4u_print_port_status(s, 0);
    return 0;
}

int m4u_debug_port_open(struct inode *inode, struct file *file)
{
    return single_open(file, m4u_debug_port_show, inode->i_private);
}

struct file_operations m4u_debug_port_fops = {
    .open = m4u_debug_port_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = seq_release,
};




int m4u_debug_init(struct m4u_device *m4u_dev)
{
    m4u_domain_t *domain = m4u_get_domain_by_id(0);
    debugfs_create_file("debug", 0644, m4u_dev->debug_root, domain, &m4u_debug_fops);
    debugfs_create_file("port", 0644, m4u_dev->debug_root, domain, &m4u_debug_port_fops);
    debugfs_create_file("log_level", 0644, m4u_dev->debug_root, domain, &m4u_log_level_fops);

    return 0;
}



