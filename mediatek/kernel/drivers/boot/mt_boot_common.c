#define pr_fmt(fmt) "["KBUILD_MODNAME"] " fmt
#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/kfifo.h>

#include <linux/firmware.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/of.h>
#ifdef CONFIG_OF
#include <linux/of_fdt.h>
#endif
#include <asm/setup.h>
#include <asm/atomic.h>
#include <mach/mt_reg_base.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_boot_common.h>

typedef enum {
    BOOT_UNINIT         = 0,
    BOOT_INITIALIZING   = 1,
    BOOT_INITIALIZED    = 2,
} BOOT_INIT_STATE;

/* this vairable will be set by mt_fixup.c */
BOOTMODE g_boot_mode __nosavedata = UNKNOWN_BOOT;
boot_reason_t g_boot_reason __nosavedata = BR_UNKNOWN;

#ifdef CONFIG_OF
static atomic_t g_boot_init = ATOMIC_INIT(BOOT_UNINIT);
static int __init dt_get_boot_common(unsigned long node, const char *uname, int depth, void *data)
{
    struct tag *tags = NULL;
    char *ptr = NULL, *br_ptr = NULL;

    if (depth != 1 ||(strcmp(uname, "chosen") != 0 && strcmp(uname, "chosen@0") != 0))
        return 0;
        
    tags = (struct tag*)of_get_flat_dt_prop(node, "atag,boot", NULL);
    
    if (tags)
        g_boot_mode = tags->u.boot.bootmode;
    else
        pr_warn("'atag,boot' is not found\n");
        
    ptr = (char *)of_get_flat_dt_prop(node, "bootargs", NULL);
    if (ptr)
    {
 		if ((br_ptr = strstr(ptr, "boot_reason=")) != 0) 			
			g_boot_reason = br_ptr[12] - '0'; /* get boot reason */
		else
		    pr_warn("'boot_reason=' is not found\n");	
		pr_debug("%s\n", ptr);    
    } else 
        pr_warn("'bootargs' is not found\n");
    
    /* break now */          
    return 1;	
}
#endif


void __init init_boot_common(unsigned int line)
{
#ifdef CONFIG_OF
    int rc;
    
    if (BOOT_INITIALIZING == atomic_read(&g_boot_init))
    {
        pr_warn("%s (%d) state(%d)\n", __func__, line, atomic_read(&g_boot_init));
        return;
    }

    if (BOOT_UNINIT == atomic_read(&g_boot_init))
        atomic_set(&g_boot_init, BOOT_INITIALIZING);
    else 
        return;

    if ((UNKNOWN_BOOT != g_boot_mode) && (BR_UNKNOWN != g_boot_reason))
        return;

    pr_info("%s %d [%d %d] [%d]\n", __func__, line, g_boot_mode, g_boot_reason, atomic_read(&g_boot_init));
    rc = of_scan_flat_dt(dt_get_boot_common, NULL);
    if (0 != rc)
        atomic_set(&g_boot_init, BOOT_INITIALIZED);    
    else 
        atomic_set(&g_boot_init, BOOT_UNINIT);            
    pr_info("%s %d [%d %d] [%d]\n", __func__, line, g_boot_mode, g_boot_reason, atomic_read(&g_boot_init));
#endif        
}

/* return boot reason */
boot_reason_t get_boot_reason(void)
{
    return g_boot_reason;
}

/* set boot reason */
void set_boot_reason (boot_reason_t br)
{
    g_boot_reason = br;
}

/* return boot mode */
BOOTMODE get_boot_mode(void)
{
    return g_boot_mode;
}

/* set boot mode */
void set_boot_mode (BOOTMODE bm)
{
    g_boot_mode = bm;
}

/* for convenience, simply check is meta mode or not */
bool is_meta_mode(void)
{   
    if(g_boot_mode == META_BOOT)
    {   
        return true;
    }
    else
    {   
        return false;
    }
}

bool is_advanced_meta_mode(void)
{
    if (g_boot_mode == ADVMETA_BOOT)
    {
        return true;
    }
    else
    {
        return false;
    }
}

static int boot_mode_proc_show(struct seq_file* p, void* v)
{
    seq_printf(p, "\n\rMTK BOOT MODE : " );
    switch(g_boot_mode)
    {
        case NORMAL_BOOT :
            seq_printf(p, "NORMAL BOOT\n");
            break;
        case META_BOOT :
            seq_printf(p, "META BOOT\n");
            break;
        case ADVMETA_BOOT :
            seq_printf(p, "Advanced META BOOT\n");
            break;   
        case ATE_FACTORY_BOOT :
            seq_printf(p, "ATE_FACTORY BOOT\n");
            break;
        case ALARM_BOOT :
            seq_printf(p, "ALARM BOOT\n");
            break;
        default :
            seq_printf(p, "UNKNOWN BOOT\n");
            break;
    }  

    return 0;
}

static int boot_mode_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, boot_mode_proc_show, NULL);
}

static const struct file_operations boot_mode_proc_fops = { 
    .open           = boot_mode_proc_open,
    .read           = seq_read,
    .llseek         = seq_lseek,
    .release        = single_release,
};

extern struct proc_dir_entry proc_root;

static int __init boot_common_init(void)
{   
    /* create proc entry at /proc/boot_mode */
    proc_create_data("boot_mode", S_IRUGO, &proc_root, &boot_mode_proc_fops, NULL);

    return 0;
}

static void __exit boot_common_exit(void)
{
    
}
module_init(boot_common_init);
module_exit(boot_common_exit);
MODULE_DESCRIPTION("MTK Boot Information Common Driver");
MODULE_LICENSE("GPL");
EXPORT_SYMBOL(is_meta_mode);
EXPORT_SYMBOL(is_advanced_meta_mode);
EXPORT_SYMBOL(get_boot_mode);
