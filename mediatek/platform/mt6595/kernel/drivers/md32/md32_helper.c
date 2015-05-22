#include <linux/module.h>       /* needed by all modules */
#include <linux/init.h>         /* needed by module macros */
#include <linux/fs.h>		/* needed by file_operations* */
#include <linux/miscdevice.h>	/* needed by miscdevice* */
#include <linux/device.h>	/* needed by device_* */
#include <mach/hardware.h>	/* needed by __io_address */
#include <asm/io.h>		/* needed by ioremap * */
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/fs.h>		/* needed by file_operations* */
#include <linux/slab.h>
#include <linux/poll.h> /* needed by poll */
#include <linux/sched.h>
#include <linux/suspend.h>

#include <mach/mt_reg_base.h>
#include <mach/sync_write.h>
#include <mach/mt_spm_sleep.h>

#include "md32_ipi.h"
#include "md32_helper.h"


#define TIMEOUT 100

static void memcpy_md32 (void __iomem *trg, const void *src, int size)
{
	int i;
	u32 __iomem *t = trg;
	const u32 *s = src;

	for (i = 0; i < (size >> 2); i++)
		*t++ = *s++;
}


int get_md32_semaphore(int flag)
{
    int read_back;
    int count = 0;
    int ret = -1;

    flag = (flag*2)+1;

    writel((0x1<<flag), (void __iomem *)MD32_SEMAPHORE);

    while(count != TIMEOUT)
    {
        /* repeat test if we get semaphore */
        read_back = (readl((void __iomem *)MD32_SEMAPHORE) >> flag) & 0x1;
        if(read_back == 1)
        {
            ret = 1;
            break;
        }
        writel((0x1<<flag), (void __iomem *)MD32_SEMAPHORE);
        count++;
    }

    if(ret < 0)        
        pr_err("get md32 semaphore TIMEOUT...!\n");

    return ret;
}

int release_md32_semaphore(int flag)
{
    int read_back;
    int ret = -1;

    flag = (flag*2)+1;

    /* Write 1 clear */
    writel((1<<flag), (void __iomem *)MD32_SEMAPHORE);
    read_back = (readl((void __iomem *)MD32_SEMAPHORE) >> flag) & 0x1;
    if(read_back == 0)
    {
        ret = 1;
    }
    else
        pr_err( "release md32 semaphore failed!\n");

    return -1;
}

unsigned int md32_log_buf_addr;
unsigned int md32_log_start_idx_addr;
unsigned int md32_log_end_idx_addr;
unsigned int md32_log_lock_addr;
unsigned int md32_log_buf_len_addr;
unsigned int enable_md32_mobile_log_addr;

static BLOCKING_NOTIFIER_HEAD(md32_notifier_list);



void md32_register_notify (struct notifier_block *nb)
{
    blocking_notifier_chain_register (&md32_notifier_list, nb);
}

void md32_unregister_notify(struct notifier_block *nb)
{
    blocking_notifier_chain_unregister(&md32_notifier_list, nb);
}

EXPORT_SYMBOL_GPL(md32_unregister_notify);


EXPORT_SYMBOL_GPL(md32_register_notify);

static int md32_helper_notify(struct notifier_block *self, unsigned long action, void *dev)
{
    MD32_REQUEST_SWAP *request_swap = (MD32_REQUEST_SWAP*)dev;
    int ret;

	switch (action) {
#ifdef DYNAMIC_TCM_SWAP
	case MD32_SELF_TRIGGER_TCM_SWAP:
        ret = md32_tcm_swap(request_swap->group_start);
        if(ret < 0)
            pr_err("md32_helper_notify swap tcm failed\n");
        break;
	case APP_TRIGGER_TCM_SWAP_START:
        /* do nothing, the following is only for demo and testing */
        if(request_swap->prepare_result < 0)
        {
            pr_err( "MD32 dynamic swap prepare failed\n");
            break;
        }
        //if(request_swap->current_group == GROUP_BASIC && request_swap->group_start == GROUP_A)
        //{
        /* Group leader need to call swap funtion */
        ret = md32_tcm_swap(request_swap->group_start);
        if(ret < 0)
            blocking_notifier_call_chain(&md32_notifier_list, APP_TRIGGER_TCM_SWAP_FAIL, request_swap);
        else
            blocking_notifier_call_chain(&md32_notifier_list, APP_TRIGGER_TCM_SWAP_DONE, request_swap);
        //}
		break;
	case APP_TRIGGER_TCM_SWAP_DONE:
        /* do nothing */
		break;
    case APP_TRIGGER_TCM_SWAP_FAIL:
        /* do nothing */
        break;
    case APP_TRIGGER_APP_FINISHED:
        /* do nothing */
        break;
#endif
    default:
        pr_err("md32 helper get unkown action %d, request_swap=%p\n", (int)action, request_swap);
        break;
	}
	return NOTIFY_OK;
}

static struct notifier_block md32_helper_nb = {
    .notifier_call =        md32_helper_notify,
};

int load_md32(const char *IMAGE_PATH, unsigned int dst);

MD32_GROUP_INFO g_group_info;
int g_current_group = GROUP_BASIC;
unsigned char *swap_buf[MAX_GROUP_NUM];
unsigned char *md32_data_image;
unsigned char *md32_program_image;

struct mutex md32_log_mutex;
static wait_queue_head_t logwait;

ssize_t md32_get_log_buf(unsigned char *md32_log_buf, size_t b_len)
{
    ssize_t i = 0;
    unsigned int log_start_idx;
    unsigned int log_end_idx;
    unsigned int log_buf_max_len;
    unsigned char *__log_buf = (unsigned char *)(MD32_DTCM + md32_log_buf_addr);

    //mutex_lock(&md32_log_mutex); //mark for get log buf in irq

    log_start_idx = readl((void __iomem *)(MD32_DTCM + md32_log_start_idx_addr));
    log_end_idx = readl((void __iomem *)(MD32_DTCM + md32_log_end_idx_addr));
    log_buf_max_len = readl((void __iomem *)(MD32_DTCM + md32_log_buf_len_addr));

    if(!md32_log_buf)
    {
        pr_err("input null buffer\n");
        goto out;
    }

    if(b_len > log_buf_max_len)
    {
        b_len = log_buf_max_len;
    }

#define LOG_BUF_MASK (log_buf_max_len-1)
#define LOG_BUF(idx) (__log_buf[(idx) & LOG_BUF_MASK])

    /* Read MD32 log */
    i = 0;
    /* Lock the log buffer */
    mt65xx_reg_sync_writel(0x1, (MD32_DTCM + md32_log_lock_addr));
    while( (log_start_idx != log_end_idx) && i < b_len)
    {
        md32_log_buf[i] = LOG_BUF(log_start_idx);
        log_start_idx++;
        i++;
    }
    mt65xx_reg_sync_writel(log_start_idx, (MD32_DTCM + md32_log_start_idx_addr));
    /* Unlock the log buffer */
    mt65xx_reg_sync_writel(0x0, (MD32_DTCM + md32_log_lock_addr));

out:
    //mutex_unlock(&md32_log_mutex);

    return i;
}


int get_current_group(void)
{
    return g_current_group;
}

static int md32_log_open(struct inode *inode, struct file *file)
{
	return nonseekable_open(inode, file);
}

static ssize_t md32_log_read(struct file *file, char __user *data,
                             size_t len, loff_t *ppos)
{
    int i;
    int ret_len;
    unsigned char *md32_log_buf;
    unsigned long copy_size;
    unsigned int log_buf_max_len = readl((void __iomem *)(MD32_DTCM + md32_log_buf_len_addr));
    md32_log_buf = (unsigned char*)kmalloc(log_buf_max_len, GFP_KERNEL);

    if(!md32_log_buf)
    {
        pr_err("%s, kmalloc fail, len=0x%08x\n", __func__, log_buf_max_len);
        return 0;
    }

    ret_len = md32_get_log_buf(md32_log_buf, len);

    for(i = 0; i < ret_len; i++)
    {
        pr_err("%c", md32_log_buf[i]);
    }

    copy_size = copy_to_user((unsigned char*)data, (unsigned char *)md32_log_buf, ret_len);

    kfree(md32_log_buf);
    return ret_len;
}

static unsigned int md32_poll(struct file *file, poll_table *wait)
{
    unsigned int ret = POLLOUT | POLLWRNORM;

  if (!(file->f_mode & FMODE_READ))
		return ret;

  poll_wait(file, &logwait, wait);


  ret |= POLLIN | POLLRDNORM;

  return ret;
}

static struct file_operations md32_file_ops = {
    .owner = THIS_MODULE,
    .read = md32_log_read,
    .open = md32_log_open,
    .poll = md32_poll,
};

static struct miscdevice md32_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "md32",
	.fops = &md32_file_ops
};


void logger_ipi_handler(int id, void *data, unsigned int len)
{
    MD32_LOG_INFO *log_info = (MD32_LOG_INFO*)data;

    md32_log_buf_addr = log_info->md32_log_buf_addr;
    md32_log_start_idx_addr = log_info->md32_log_start_idx_addr;
    md32_log_end_idx_addr = log_info->md32_log_end_idx_addr;
    md32_log_lock_addr = log_info->md32_log_lock_addr;
    md32_log_buf_len_addr = log_info->md32_log_buf_len_addr;
    md32_log_buf_len_addr = log_info->md32_log_buf_len_addr;
    enable_md32_mobile_log_addr =log_info->enable_md32_mobile_log_addr;

    pr_crit("[MD32] md32_log_buf_addr = %x\n", md32_log_buf_addr);
    pr_crit("[MD32] md32_log_start_idx_addr = %x\n", md32_log_start_idx_addr);
    pr_crit("[MD32] md32_log_end_idx_addr = %x\n", md32_log_end_idx_addr);
    pr_crit("[MD32] md32_log_lock_addr = %x\n", md32_log_lock_addr);
    pr_crit("[MD32] md32_log_buf_len_addr = %x\n", md32_log_buf_len_addr);
    pr_crit("[MD32] enable_md32_mobile_log_addr = %x\n", enable_md32_mobile_log_addr);
}

void buf_full_ipi_handler(int id, void *data, unsigned int len)
{
    wake_up(&logwait);
}

int get_md32_img_sz(const char *IMAGE_PATH)
{
    struct file *filp = NULL;
    struct inode *inode;
    off_t fsize = 0;

    filp = filp_open(IMAGE_PATH, O_RDONLY, 0644);

    if(IS_ERR(filp))
    {
        pr_err( "Open MD32 image %s FAIL!\n", IMAGE_PATH);
        return -1;
    }
    else
    {
        inode=filp->f_dentry->d_inode;
        fsize=inode->i_size;
    }

    filp_close(filp, NULL);
    return fsize;
}

#ifdef DYNAMIC_TCM_SWAP
/* @group_id: the group want to swap in tcm and run. */
void md32_prepare_swap(int group_id)
{
    MD32_PREPARE_SWAP prepare_swap;

    /* Prepare object for MD32 */
    prepare_swap.info_type = INFO_PREPARE_SWAP;
    prepare_swap.group_start = group_id;

    md32_ipi_send(IPI_SWAP, (void *)&prepare_swap, sizeof(MD32_PREPARE_SWAP), 0);
}

/* @group_id: the group want to swap in tcm and run. */
int md32_tcm_swap(int group_id)
{
    int ret = 0;
    unsigned int swap_lock;
    unsigned int app_program_start_addr;
    unsigned int app_program_end_addr;
    unsigned int app_program_sz;
    //unsigned char *p_buf;
    //int p_sz;

    unsigned int app_data_start_addr;
    unsigned int app_data_end_addr;
    unsigned int app_data_sz;
    //unsigned char *d_buf;
    //int d_sz;

    unsigned int swap_out_data_start_addr;
    unsigned int swap_out_data_end_addr;
    unsigned int swap_out_data_sz;

#if 0
    int i;
    unsigned int *tmp_ptr;
    unsigned int *ptcm_ptr;
#endif

    /* Check swap_lock is set by md32 */
    swap_lock = readl((void __iomem *)(MD32_DTCM + g_group_info.swap_lock_addr));

    if(swap_lock == 1)
    {
        app_program_start_addr = readl((void __iomem *)(MD32_DTCM + g_group_info.app_ptr_p + sizeof(unsigned int)*group_id*2));
        app_program_end_addr = readl((void __iomem *)(MD32_DTCM + g_group_info.app_ptr_p + (sizeof(unsigned int)*group_id*2) + sizeof(unsigned int)));
        app_program_sz = app_program_end_addr - app_program_start_addr + 1;

        app_data_start_addr = readl((void __iomem *)(MD32_DTCM + g_group_info.app_ptr_d + sizeof(unsigned int)*group_id*2));
        app_data_end_addr = readl((void __iomem *)(MD32_DTCM + g_group_info.app_ptr_d + (sizeof(unsigned int)*group_id*2) + sizeof(unsigned int)));
        app_data_sz = app_data_end_addr - app_data_start_addr + 1;

        swap_out_data_start_addr = readl((void __iomem *)(MD32_DTCM + g_group_info.app_ptr_d + sizeof(unsigned int)*g_current_group*2));
        swap_out_data_end_addr = readl((void __iomem *)(MD32_DTCM + g_group_info.app_ptr_d + sizeof(unsigned int)*g_current_group*2 + sizeof(unsigned int)));
        swap_out_data_sz = swap_out_data_end_addr - swap_out_data_start_addr + 1;

        /* swap out stop-app data into swap_buf */
        memcpy_md32((void *)swap_buf[g_current_group], (const void *)(MD32_DTCM + g_group_info.app_d_area), swap_out_data_sz);

        //p_sz = get_md32_img_sz(MD32_PROGRAM_IMAGE_PATH);
        //d_sz = get_md32_img_sz(MD32_DATA_IMAGE_PATH);

        //p_buf = (unsigned char*)kmalloc((size_t)p_sz+1 , GFP_KERNEL);
        //d_buf = (unsigned char*)kmalloc((size_t)d_sz+1 , GFP_KERNEL);

        /* swap in start-app program/data into tcm */
        //ret = load_md32(MD32_PROGRAM_IMAGE_PATH, p_buf);
        //ret = load_md32(MD32_DATA_IMAGE_PATH, d_buf);

#if 0
        pr_crit("[MD32] swap out id is = %d, swap in id is %d\n", g_current_group, group_id);
        for(i=0; i<app_program_sz; i++)
        {
          pr_debug("============= md32_program_image[%d] = 0x%x \n", i, md32_program_image[app_program_start_addr+i]);
        }
        for(i=0; i<app_data_sz; i++)
        {
          pr_debug("============= md32_data_image[%d] = 0x%x \n", i, swap_buf[group_id][i]);
        }
        pr_debug("[MD32] MD32_PTCM swap area = %x\n", MD32_PTCM + g_group_info.app_p_area);
        pr_debug("[MD32] MD32_DCM swap area = %x\n", MD32_DTCM + g_group_info.app_d_area );

        pr_debug("[MD32]A: image check...\n");
        tmp_ptr = (unsigned int*)md32_program_image;
        ptcm_ptr = (unsigned int*) MD32_PTCM;
        for(i=0; i<(0x2000>>2); i++)
        {
          if(ptcm_ptr[i] != tmp_ptr[i])
          {
              pr_err("ERROR!!!ERROR!!!ERROR!!! --> PTCM[%x] = %x, md32_program_image[%x] = %x\n", i, ptcm_ptr[i], i, tmp_ptr[i]);
          }
        }
#endif
        memcpy_md32((void *)(MD32_PTCM + g_group_info.app_p_area), (void *)(md32_program_image + app_program_start_addr), app_program_sz);
        memcpy_md32((void *)(MD32_DTCM + g_group_info.app_d_area), (void *)swap_buf[group_id], app_data_sz);

        pr_debug("[MD32]B: image check...\n");

#if 0
        for(i=0; i<(0x2000>>2); i++)
        {
          if(ptcm_ptr[i] != tmp_ptr[i])
          {
              pr_err("ERROR!!!ERROR!!!ERROR!!! --> PTCM[%d] = %x, md32_program_image[%d] = %x\n", i, ptcm_ptr[i], i, tmp_ptr[i]);
          }
        }
#endif

        /* swap done. clear the swap_lock */
        mt65xx_reg_sync_writel(0x0, (MD32_DTCM + g_group_info.swap_lock_addr));
        g_current_group = group_id;
    }
    else
    {
        ret = -1;
    }

    return ret;
}

void swap_ipi_handler(int id, void *data, unsigned int len)
{
    int info_type;
    int i;
    MD32_GROUP_INFO *group_info;
    MD32_REQUEST_SWAP request_swap;
    MD32_REQUEST_SWAP self_trigger_swap;
    unsigned int app_data_start_addr;
    unsigned int app_data_end_addr;
    unsigned int app_data_sz;

    unsigned int app_program_end_addr;
    //unsigned char *d_buf;
    //int d_sz;
    //int ret;

    pr_debug("[MD32] Enter swap ipi...\n");

    info_type = ((int *)data)[0];

    pr_debug("swap info_type = %d\n", info_type);

    /* by info_type to select structure we receive */
    if(info_type == INFO_GROUP)
    {
        group_info = (MD32_GROUP_INFO*)data;
        g_group_info.info_type = group_info->info_type;
        g_group_info.app_ptr_d = group_info->app_ptr_d;
        g_group_info.app_ptr_p = group_info->app_ptr_p;
        g_group_info.swap_lock_addr = group_info->swap_lock_addr;
        g_group_info.group_num = group_info->group_num;
        g_group_info.app_d_area = group_info->app_d_area;
        g_group_info.app_p_area = group_info->app_p_area;

        /* Clear other apps in TCM, only keep GROUP_BASIC in TCM */
        app_program_end_addr = readl((void __iomem *)(MD32_DTCM + g_group_info.app_ptr_p + (sizeof(unsigned int)*GROUP_BASIC*2) + sizeof(unsigned int)));
        memset((void *)(MD32_PTCM + app_program_end_addr), 0, MD32_PTCM_SIZE - app_program_end_addr);
        app_data_end_addr = readl((void __iomem *)(MD32_DTCM + g_group_info.app_ptr_d + (sizeof(unsigned int)*GROUP_BASIC*2) + sizeof(unsigned int)));
        memset((void *)(MD32_DTCM + app_data_end_addr), 0, MD32_DTCM_SIZE - app_data_end_addr);

        /* Prepare Swap buffer for Data restore */
        //swap_buf = (unsigned char**)krealloc(swap_buf, (g_group_info.group_num)*sizeof(unsigned char*), GFP_ATOMIC);
        for(i=0; i<g_group_info.group_num; i++)
        {
            app_data_start_addr = readl((void __iomem *)(MD32_DTCM + g_group_info.app_ptr_d + sizeof(unsigned int)*i*2));
            app_data_end_addr = readl((void __iomem *)(MD32_DTCM + g_group_info.app_ptr_d + (sizeof(unsigned int)*i*2) + sizeof(unsigned int)));
            app_data_sz = app_data_end_addr - app_data_start_addr + 1;
            swap_buf[i] = (unsigned char*)krealloc(swap_buf[i], app_data_sz, GFP_ATOMIC);
            memcpy(swap_buf[i], md32_data_image+app_data_start_addr, app_data_sz);
            pr_debug("[swap_ipi_handler] group %d, app_data_start_addr = 0x%x\n", i, app_data_start_addr);
            pr_debug("[swap_ipi_handler] group %d, app_data_stop_addr = 0x%x\n", i, app_data_end_addr);
        }

        pr_debug("info_type = %d\n", g_group_info.info_type);
        pr_debug("group_num = %d\n", g_group_info.group_num);
        pr_debug("app_ptr_d = 0x%x\n", g_group_info.app_ptr_d);
        pr_debug("app_ptr_p = 0x%x\n", g_group_info.app_ptr_p);
        pr_debug("swap_lock_addr = 0x%x\n", g_group_info.swap_lock_addr);
    }
    else if(info_type == INFO_REQUEST_SWAP)
    {
        //request_swap = (MD32_REQUEST_SWAP*)data;
        memcpy((void *)&request_swap, (const void *)data, sizeof(MD32_REQUEST_SWAP));
        if(request_swap.prepare_result == 0) //prepare ready.
        {
            g_current_group = request_swap.current_group;
        }
        /* trigger notifier */
        blocking_notifier_call_chain(&md32_notifier_list, APP_TRIGGER_TCM_SWAP_START, &request_swap);
    }
    else if(info_type == INFO_SELF_TRIGGER_SWAP)
    {
        //self_trigger_swap = (MD32_SELF_TRIGGER_SWAP *)data;
        memcpy((void *)&self_trigger_swap, (const void *)data, sizeof(MD32_REQUEST_SWAP));
        if(self_trigger_swap.prepare_result == 0) //prepare ready.
        {
            g_current_group = self_trigger_swap.current_group;
        }
        blocking_notifier_call_chain(&md32_notifier_list, MD32_SELF_TRIGGER_TCM_SWAP, &self_trigger_swap);
    }
    else
    {
        pr_err("[MD32 swap IPI] unknown info type %d !\n", info_type);
    }
}
#endif

int load_md32(const char *IMAGE_PATH, unsigned int dst)
{
    struct file *filp = NULL;
    unsigned char *buf=NULL;
    struct inode *inode;
    off_t fsize;
    mm_segment_t fs;

    filp = filp_open(IMAGE_PATH, O_RDONLY, 0644);

    if(IS_ERR(filp))
    {
        pr_err( "Open MD32 image %s FAIL!\n", IMAGE_PATH);
        goto error;
    }
    else
    {
        inode=filp->f_dentry->d_inode;
        fsize=inode->i_size;
        pr_debug("MD32 file %s size: %i \n", IMAGE_PATH, (int)fsize);
        buf = (unsigned char*)kmalloc((size_t)fsize+1 , GFP_KERNEL);
        fs=get_fs();
        set_fs(KERNEL_DS);
        filp->f_op->read(filp,buf,fsize,&(filp->f_pos));
        set_fs(fs);
        buf[fsize]='\0';
        //pr_debug("<1>The File Content is:\n");
        //pr_debug("<1>%s",buf);
        memcpy((void *)dst, (const void *)buf, fsize);
    }

    filp_close(filp, NULL);
    kfree(buf);
    return fsize;

error:
    if(filp != NULL)
        filp_close(filp, NULL);

    if(buf)
        kfree(buf);
    return -1;

}

void boot_up_md32(void)
{
    mt65xx_reg_sync_writel(0x1, MD32_BASE);
}

void reset_md32(void)
{
    unsigned int sw_rstn;

    sw_rstn = readl((void __iomem *)
                    MD32_BASE);
    if(sw_rstn == 0x0)
    {
        pr_debug("[MD32] MD32 has already been reseted!\n");
    }
    else
        mt65xx_reg_sync_writel(0x0, MD32_BASE);
}



static inline ssize_t md32_log_len_show(struct device *kobj, struct device_attribute *attr, char *buf)
{
    int log_legnth;
    //unsigned int log_start_idx;
    //unsigned int log_end_idx;
    unsigned int log_buf_max_len;

    //log_start_idx = readl(MD32_DTCM + md32_log_start_idx_addr);
    //log_end_idx = readl(MD32_DTCM + md32_log_end_idx_addr);
    log_buf_max_len = readl((void __iomem *)(MD32_DTCM + md32_log_buf_len_addr));

    log_legnth = log_buf_max_len;

    return sprintf(buf, "%08x\n", log_legnth);
}

static ssize_t md32_log_len_store(struct device *kobj, struct device_attribute *attr,	const char *buf, size_t n)
{
    /*do nothing*/
    return n;
}

DEVICE_ATTR(md32_log_len, 0644, md32_log_len_show, md32_log_len_store);


static inline ssize_t md32_boot_show(struct device *kobj, struct device_attribute *attr, char *buf)
{
    unsigned int sw_rstn;
    sw_rstn = readl((void __iomem *)MD32_BASE);

    if(sw_rstn == 0x0)
    {
        return sprintf(buf, "MD32 not enabled\n");
    }
    else
    {
        return sprintf(buf, "MD32 is running...\n");
    }
}


int reboot_load_md32(void)
{

    unsigned int sw_rstn;
    int ret = 0;
    int d_sz;
    int p_sz;

    sw_rstn = readl((void __iomem *)MD32_BASE);

    if(sw_rstn == 0x1)
    {
        pr_crit("MD32 is already running, reboot now...\n");
    }

    /* reset MD32 */
    mt65xx_reg_sync_writel(0x0, MD32_BASE);

    d_sz = get_md32_img_sz(MD32_DATA_IMAGE_PATH);
    if(d_sz <= 0)
    {
        pr_err("MD32 boot up failed --> can not get data image size\n");
        ret = -1;
        goto error;
    }

    md32_data_image = (unsigned char*)krealloc(md32_data_image, (size_t)d_sz+1 , GFP_KERNEL);
    if(!md32_data_image)
    {
        pr_err("MD32 boot up failed --> can't allocate memory\n");
        ret = -1;
        goto error;
    }

    ret = load_md32((const char *)MD32_DATA_IMAGE_PATH, (unsigned int)md32_data_image);
    if(ret < 0)
    {
        pr_err("MD32 boot up failed --> load data image failed!\n");
        ret = -1;
        goto error;
    }

    if(d_sz > MD32_DTCM_SIZE)
    {
        d_sz = MD32_DTCM_SIZE;
    }
    //memcpy((void *)MD32_DTCM, (const void *)md32_data_image, d_sz);
    memcpy_md32((void *)MD32_DTCM, (const void *)md32_data_image, d_sz);

    p_sz = get_md32_img_sz(MD32_PROGRAM_IMAGE_PATH);
    if(p_sz <= 0)
    {
        pr_err("MD32 boot up failed --> can not get program image size\n");
        ret = -1;
        goto error;
    }

    md32_program_image = (unsigned char*)krealloc(md32_program_image, (size_t)p_sz+1 , GFP_KERNEL);
    if(!md32_program_image)
    {
        pr_err("MD32 boot up failed --> can't allocate memory\n");
        ret = -1;
        goto error;
    }

    ret = load_md32((const char *)MD32_PROGRAM_IMAGE_PATH, (unsigned int)md32_program_image);
    if(ret < 0)
    {
        pr_err("MD32 boot up failed --> load program image failed!\n");
        ret = -1;
        goto error;
    }
    if(p_sz > MD32_PTCM_SIZE)
    {
        p_sz = MD32_PTCM_SIZE;
    }
    //memcpy((void *)MD32_PTCM, (const void *)md32_program_image, p_sz);
    memcpy_md32((void *)MD32_PTCM, (const void *)md32_program_image, p_sz);

#if 0
#ifdef DYNAMIC_TCM_SWAP
    unsigned int app_program_start_addr;
    unsigned int app_program_end_addr;
    unsigned int app_program_sz;

    unsigned int app_data_start_addr;
    unsigned int app_data_end_addr;
    unsigned int app_data_sz;

    int d_sz;
    int p_sz;

    g_current_group = GROUP_BASIC;

    /* Only load Basic Group into TCM at beging */
    app_program_start_addr = readl(MD32_DTCM + g_group_info.app_ptr_p + sizeof(unsigned int)*GROUP_BASIC*2);
    app_program_end_addr = readl(MD32_DTCM + g_group_info.app_ptr_p + (sizeof(unsigned int)*GROUP_BASIC)*2 + sizeof(unsigned int));
    app_program_sz = app_program_end_addr - app_program_start_addr + 1;

    app_data_start_addr = readl(MD32_DTCM + g_group_info.app_ptr_d + sizeof(unsigned int)*GROUP_BASIC*2);
    app_data_end_addr = readl(MD32_DTCM + g_group_info.app_ptr_d + (sizeof(unsigned int)*GROUP_BASIC*2) + sizeof(unsigned int));
    app_data_sz = app_data_end_addr - app_data_start_addr + 1;

    /* Initialize swap buffer & copy each app data into it*/
    d_sz = get_md32_img_sz(MD32_DATA_IMAGE_PATH);
    md32_data_image = (unsigned char*)kmalloc((size_t)d_sz+1 , GFP_KERNEL);
    ret = load_md32((const char *)MD32_DATA_IMAGE_PATH, (unsigned int)md32_data_image);
    if(ret < 0)
    {
        pr_err("MD32 boot up failed --> load data image failed!\n");
        return;
    }
    memcpy_md32((void *)MD32_DTCM, (const void *)md32_data_image, (app_data_sz + g_group_info.app_d_area));

    p_sz = get_md32_img_sz(MD32_PROGRAM_IMAGE_PATH);
    md32_program_image = (unsigned char*)kmalloc((size_t)p_sz+1 , GFP_KERNEL);
    ret = load_md32((const char *)MD32_PROGRAM_IMAGE_PATH, (unsigned int)md32_program_image);
    if(ret < 0)
    {
        pr_err("MD32 boot up failed --> load program image failed!\n");
        return;
    }
    memcpy_md32((void *)MD32_PTCM, (const void *)md32_program_image, (app_program_sz + g_group_info.app_p_area));
#else
    d_sz = load_md32((const char *)MD32_DATA_IMAGE_PATH, (unsigned int)MD32_DTCM);
    if(d_sz < 0)
    {
        pr_err("MD32 boot up failed --> load data image failed!\n");
        return n;
    }
    ret = load_md32((const char *)MD32_PROGRAM_IMAGE_PATH, (unsigned int)MD32_PTCM);
    if(ret < 0)
    {
        pr_err("MD32 boot up failed --> load program image failed!\n");
        return n;
    }
#endif
#endif

    boot_up_md32();
    return ret;

error:
    pr_err("[MD32] boot up failed!!! free images\n");
    kfree(md32_data_image);
    kfree(md32_program_image);

    return ret;


}

static ssize_t md32_boot_store(struct device *kobj, struct device_attribute *attr,	const char *buf, size_t n)
{
    unsigned int enable = 0;
    if (sscanf(buf, "%d", &enable) != 1)
        return -EINVAL;

    if(reboot_load_md32() < 0)
        return -EINVAL;

    return n;
}

DEVICE_ATTR(md32_boot, 0644, md32_boot_show, md32_boot_store);

#ifdef DYNAMIC_TCM_SWAP
static inline ssize_t md32_swap_show(struct device *kobj, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "Current running group is %d\n", g_current_group);
}

static ssize_t md32_swap_store(struct device *kobj, struct device_attribute *attr,	const char *buf, size_t n)
{
    //MD32_PREPARE_SWAP prepare_swap;
    int group_start;

    if (sscanf(buf, "%d", &group_start) != 1)
        return -EINVAL;

    /* Prepare object for MD32 */
    md32_prepare_swap(group_start);
    //prepare_swap.info_type = INFO_PREPARE_SWAP;
    //prepare_swap.group_start = group_start;

    //md32_ipi_send(IPI_SWAP, (void *)&prepare_swap, sizeof(MD32_PREPARE_SWAP), 0);

    return n;
}


DEVICE_ATTR(md32_swap, 0644, md32_swap_show, md32_swap_store);
#endif


static inline ssize_t md32_mobile_log_show(struct device *kobj, struct device_attribute *attr, char *buf)
{
    unsigned int enable_md32_mobile_log;

    enable_md32_mobile_log = readl((void __iomem *)(MD32_DTCM + enable_md32_mobile_log_addr));

    if(enable_md32_mobile_log == 0x0)
        return sprintf(buf, "MD32 mobile log is disabled\n");
    else if(enable_md32_mobile_log == 0x1)
        return sprintf(buf, "MD32 mobile log is enabled\n");
    else
        return sprintf(buf, "MD32 mobile log is in unknown state...\n");
}

static ssize_t md32_mobile_log_store(struct device *kobj, struct device_attribute *attr,	const char *buf, size_t n)
{
    unsigned int enable;
    if (sscanf(buf, "%d", &enable) != 1)
        return -EINVAL;

    mt65xx_reg_sync_writel(enable, (MD32_DTCM + enable_md32_mobile_log_addr));

    return n;
}


DEVICE_ATTR(md32_mobile_log, 0644, md32_mobile_log_show, md32_mobile_log_store);


static int create_files(void)
{
    int ret = device_create_file(md32_device.this_device, &dev_attr_md32_log_len);
    if (unlikely(ret != 0))
        return ret;

    ret = device_create_file(md32_device.this_device, &dev_attr_md32_boot);
    if (unlikely(ret != 0))
        return ret;

    ret = device_create_file(md32_device.this_device, &dev_attr_md32_ocd);
    if (unlikely(ret != 0))
        return ret;

    ret = device_create_file(md32_device.this_device, &dev_attr_md32_mobile_log);
    if (unlikely(ret != 0))
        return ret;

#if MD32_JTAG_GPIO_DVT
    ret = device_create_file(md32_device.this_device, &dev_attr_md32_jtag_switch);
    if (unlikely(ret != 0))
        return ret;

    ret = device_create_file(md32_device.this_device, &dev_attr_md32_jtag_gpio_dvt);
    if (unlikely(ret != 0))
        return ret;
#endif


#ifdef DYNAMIC_TCM_SWAP
    ret = device_create_file(md32_device.this_device, &dev_attr_md32_swap);
    if (unlikely(ret != 0))
        return ret;
#endif

    return 0;
}

static int md32_pm_event(struct notifier_block *notifier, unsigned long pm_event, void *unused)
{
	int retval;

	switch(pm_event) {
	case PM_POST_HIBERNATION:
		pr_crit("[MD32] MD32 reboot\n");
		retval = reboot_load_md32();
		if(retval < 0){
			retval = -EINVAL;
			pr_err("[MD32] MD32 reboot Fail\n");
		}
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static struct notifier_block md32_pm_notifier_block = {
	.notifier_call = md32_pm_event,
	.priority = 0,
};

/**
 * driver initialization entry point
 */
static int __init md32_init(void)
{
    int ret = 0;

    mutex_init(&md32_log_mutex);
    init_waitqueue_head(&logwait);

    /*FIXME: need to call SPM API. hard code temply*/
    //mt65xx_reg_sync_writel(0x0b160001, SLEEP_BASE);
    //mt65xx_reg_sync_writel(0xfffffff0, (SLEEP_BASE + 0x2c8));
    spm_poweron_config_set();
    spm_md32_sram_con(0xfffffff0);

#if 0  //FIXME: At FPGA stage, we boot up annually by sysfs; at EVB stage, we boot up it by insmod
    /* Load MD32 image & boot up MD32 */
    /* reset MD32 */
    mt65xx_reg_sync_writel(0x0, MD32_BASE);

    ret = load_md32((const char *)MD32_DATA_IMAGE_PATH, (unsigned int)MD32_DTCM);
    if(ret < 0)
    {
        pr_err( "MD32 boot up failed --> load data image failed!\n");
        return ret;
    }
    ret = load_md32((const char *)MD32_PROGRAM_IMAGE_PATH, (unsigned int)MD32_PTCM);
    if(ret < 0)
    {
        pr_err( "MD32 boot up failed --> load program image failed!\n");
        return ret;
    }
    boot_up_md32();
#endif

    md32_irq_init();
    md32_ipi_init();
    md32_ocd_init();

    /* register logger IPI */
    md32_ipi_registration(IPI_LOGGER, logger_ipi_handler, "logger");

    /* register log buf full IPI */
    md32_ipi_registration(IPI_BUF_FULL, buf_full_ipi_handler, "buf_full");

#ifdef  DYNAMIC_TCM_SWAP
    /* register swap IPI */
    md32_ipi_registration(IPI_SWAP, swap_ipi_handler, "swap");
#endif

    pr_debug("MD32 helper init\n");

    if (unlikely((ret = misc_register(&md32_device)) != 0))
    {
        pr_err( "misc register failed\n");
        ret = -1;
	}

	if (unlikely((ret = create_files()) != 0)) {
		pr_err( "create files failed\n");
	}

    /*
     * Hook to the MD32 chain to get notification
     */

	md32_register_notify(&md32_helper_nb);
	ret = register_pm_notifier(&md32_pm_notifier_block);
	if (ret)
		pr_err(KERN_WARNING "[%s] failed to register PM notifier %d\n", __func__, ret);

	return ret;
}

/**
 * driver exit point
 */
static void __exit md32_exit(void)
{
    kfree(swap_buf);
    misc_deregister(&md32_device);
    md32_unregister_notify(&md32_helper_nb);
}

module_init(md32_init);
module_exit(md32_exit);

