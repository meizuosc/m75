#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/seq_file.h>
#include <linux/aee.h>
#include <asm/uaccess.h>
#include <mach/mtk_memcfg.h>

#define MTK_MEMCFG_SIMPLE_BUFFER_LEN 16
#define MTK_MEMCFG_LARGE_BUFFER_LEN (1200) /* 1200 bytes, it should not be larger than 1 page */

struct mtk_memcfg_info_buf {
    unsigned long max_len;
    unsigned long curr_pos;
    char buf[MTK_MEMCFG_LARGE_BUFFER_LEN];
};

static struct mtk_memcfg_info_buf mtk_memcfg_layout_buf = {
    .buf = { [0 ... (MTK_MEMCFG_LARGE_BUFFER_LEN - 1)] = 0, },
    .max_len = MTK_MEMCFG_LARGE_BUFFER_LEN,
    .curr_pos = 0,
};
static unsigned long force_inode_gfp_lowmem = 0;
#ifdef CONFIG_SLUB_DEBUG
static unsigned long bypass_slub_debug = 0;
#endif 

static int mtk_memcfg_late_warning_flag = 0;

/* inode GFP control */

static int mtk_memcfg_inode_show(struct seq_file *m, void *v)
{
    seq_printf(m, "default inode GFP: %s\n", 
            force_inode_gfp_lowmem? "Low Memory": "High User Movable");

    return 0;
}

static int mtk_memcfg_inode_open(struct inode *inode, struct file *file)
{
    return single_open(file, mtk_memcfg_inode_show, NULL);
}

static ssize_t mtk_memcfg_inode_write(struct file *file, const char __user *buffer,
        size_t count, loff_t *pos)
{
    int flag;
    
    if (count > 0) {
        if (get_user(flag, buffer)) {
            return -EFAULT;
        }
        force_inode_gfp_lowmem = flag;
    }
    return count;
}

unsigned long mtk_memcfg_get_force_inode_gfp_lowmem(void)
{
    return force_inode_gfp_lowmem;
}

unsigned long mtk_memcfg_set_force_inode_gfp_lowmem(unsigned long flag)
{
    force_inode_gfp_lowmem = flag;
    return force_inode_gfp_lowmem;
}

void mtk_memcfg_write_memory_layout_buf(char *fmt, ...)
{
    va_list ap;
    struct mtk_memcfg_info_buf *layout_buf = &mtk_memcfg_layout_buf;
    if (layout_buf->curr_pos <= layout_buf->max_len) {
        va_start(ap, fmt);
        layout_buf->curr_pos += 
            vsnprintf((layout_buf->buf + layout_buf->curr_pos), 
                    (layout_buf->max_len - layout_buf->curr_pos), fmt, ap);
        va_end(ap);
    }
}

void mtk_memcfg_late_warning(void)
{
	mtk_memcfg_late_warning_flag = 1;
}

/* end of inode GFP control */

/* kenerl memory information */

static int mtk_memcfg_memory_layout_show(struct seq_file *m, void *v)
{
    seq_printf(m, "%s", mtk_memcfg_layout_buf.buf);
    seq_printf(m, "buffer usage: %lu/%lu\n",
            (mtk_memcfg_layout_buf.curr_pos <= mtk_memcfg_layout_buf.max_len?
            mtk_memcfg_layout_buf.curr_pos: mtk_memcfg_layout_buf.max_len), 
            mtk_memcfg_layout_buf.max_len);

    return 0;
}

static int mtk_memcfg_memory_layout_open(struct inode *inode, struct file *file)
{
    return single_open(file, mtk_memcfg_memory_layout_show, NULL);
}

/* end of kenerl memory information */

#ifdef CONFIG_SLUB_DEBUG
/* bypass slub debug control */

static int mtk_memcfg_slub_debug_show(struct seq_file *m, void *v)
{
    seq_printf(m, "slub debug mode: %s\n", bypass_slub_debug? "bypass": "normal");

    return 0;
}

static int mtk_memcfg_slub_debug_open(struct inode *inode, struct file *file)
{
    return single_open(file, mtk_memcfg_slub_debug_show, NULL);
}

static ssize_t mtk_memcfg_slub_debug_write(struct file *file, const char __user *buffer,
        size_t count, loff_t *pos)
{
    int flag;
    
    if (count > 0) {
        if (get_user(flag, buffer)) {
            return -EFAULT;
        }
        mtk_memcfg_set_bypass_slub_debug_flag((unsigned long)flag);
    }
    return count;
}

unsigned long mtk_memcfg_get_bypass_slub_debug_flag(void)
{
    return bypass_slub_debug;
}

unsigned long mtk_memcfg_set_bypass_slub_debug_flag(unsigned long flag)
{
    /* 
     * Do not re-enable slub debug after disabling it.
     * We do not trust slub debug data after it is disabled.
     */
    if (bypass_slub_debug && !flag) {
        printk(KERN_ERR"===== slub debug is re-enabled, "
                "ignore this operation =====\n");
        goto out;
    } else if (!bypass_slub_debug && flag) {
        printk(KERN_ALERT"===== bypass slub debug =====\n");
        bypass_slub_debug = flag;
    }
out:
    return bypass_slub_debug;
}

#endif 

/* end of bypass slub debug control */

static int __init mtk_memcfg_init(void)
{
    return 0;
}

static void __exit mtk_memcfg_exit(void)
{
}

static const struct file_operations mtk_memcfg_inode_operations = {
    .open           = mtk_memcfg_inode_open,
    .write          = mtk_memcfg_inode_write,
    .read           = seq_read,
    .llseek         = seq_lseek,
    .release        = single_release,
};

static const struct file_operations mtk_memcfg_memory_layout_operations = {
    .open           = mtk_memcfg_memory_layout_open,
    .read           = seq_read,
    .llseek         = seq_lseek,
    .release        = single_release,
};

#ifdef CONFIG_SLUB_DEBUG
static const struct file_operations mtk_memcfg_slub_debug_operations = {
    .open           = mtk_memcfg_slub_debug_open,
    .write          = mtk_memcfg_slub_debug_write,
    .read           = seq_read,
    .llseek         = seq_lseek,
    .release        = single_release,
};

extern int slabtrace_open(struct inode *inode, struct file *file);
static const struct file_operations proc_slabtrace_operations = {
	.open		= slabtrace_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};
#endif 

static int __init mtk_memcfg_late_init(void)
{
    struct proc_dir_entry *entry = NULL;
    struct proc_dir_entry *mtk_memcfg_dir = NULL;

    printk(KERN_INFO"[%s] start\n", __FUNCTION__);

    mtk_memcfg_dir = proc_mkdir("mtk_memcfg", NULL);

    if (!mtk_memcfg_dir) {

        printk(KERN_ERR"[%s]: mkdir /proc/mtk_memcfg failed\n",
                __FUNCTION__);

    } else {

        /* inode gfp conifg */
        entry = proc_create("force_inode_gfp_lowmem", 
                S_IRUGO | S_IWUSR, mtk_memcfg_dir, &mtk_memcfg_inode_operations);

        if (!entry) {
            printk(KERN_ERR"create force_inode_gfp_lowmem proc entry failed\n");
        }

        /* display kernel memory layout */
        entry = proc_create("memory_layout", 
                S_IRUGO | S_IWUSR, mtk_memcfg_dir, &mtk_memcfg_memory_layout_operations);

        if (!entry) {
            printk(KERN_ERR"create memory_layout proc entry failed\n");
        }

#ifdef CONFIG_SLUB_DEBUG
        /* bypass slub debug control */
        entry = proc_create("bypass_slub_debug", 
                S_IRUGO | S_IWUSR, mtk_memcfg_dir, &mtk_memcfg_slub_debug_operations);

        if (!entry) {
            printk(KERN_ERR"create bypass_slub_debug proc entry failed\n");
        }

	/* slabtrace - full slub object backtrace */
	entry = proc_create("slabtrace", 
		S_IRUSR, mtk_memcfg_dir, &proc_slabtrace_operations);

	if (!entry) {
		printk(KERN_ERR"create slabtrace proc entry failed\n");
	}
#endif 

    }
    
    return 0;
}

module_init(mtk_memcfg_init);
module_exit(mtk_memcfg_exit);

extern unsigned long totalhigh_pages;
static int __init mtk_memcfg_late_sanity_test(void)
{
	/* trigger kernel warning if warning flag is set */
	if (mtk_memcfg_late_warning_flag) {
        	aee_kernel_warning("[memory layout conflict]", mtk_memcfg_layout_buf.buf);
	}

	/* check highmem zone size */
	if (unlikely(totalhigh_pages && (totalhigh_pages << PAGE_SHIFT) < SZ_8M))  {
		aee_kernel_warning("[high zone lt 8MB]", __FUNCTION__);
	}

	return 0;
}

late_initcall(mtk_memcfg_late_init);
late_initcall(mtk_memcfg_late_sanity_test);
