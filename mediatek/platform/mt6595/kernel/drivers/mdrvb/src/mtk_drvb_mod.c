#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/poll.h>
#include "mtk_ac_mod.h"
#include "mtk_drvb_common.h"

/*****************************************************************************
 * MODULE DEFINITION
 *****************************************************************************/
#define MOD "AC_MOD"
#define AC_DEVNAME "drvb"

extern long sec_drv_base_check(uint32 ioctl_num, ulong ioctl_param);
extern unsigned int sec_ac_alloc_mem(void);
extern void sec_ac_free_mem(unsigned int num_pages);

/*****************************************************************************
 * LOCAL VARIABLES
 *****************************************************************************/
static dev_t mtk_ac_devno;
static struct cdev *mtk_ac_cdev;

static DEFINE_MUTEX (drvb_hash_mutex);
static DEFINE_MUTEX (drvb_auth_mutex);

/*****************************************************************************
 * PORTING LAYER
 *****************************************************************************/
void sec_free_page(ulong *addr)
{
    free_page(addr);
}

unsigned long sec_get_free_page()
{
    return __get_free_page (GFP_KERNEL);
}

unsigned long sec_copy_from_user(void * to, void * from, unsigned long size)
{
    return copy_from_user(to, from, size);
}

unsigned long sec_copy_to_user(void * to, void * from, unsigned long size)
{
    return copy_to_user(to, from, size);
}

void sec_mutex_lock()
{
    mutex_lock(&drvb_hash_mutex);
}

void sec_mutex_unlock()
{
    mutex_unlock(&drvb_hash_mutex);
}

static long mtk_drvb_ioctl (struct file *file, uint32 ioctl_num, ulong ioctl_param)
{
    return sec_drv_base_check(ioctl_num, ioctl_param);
}

static int mtk_drvb_open (struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t mtk_drvb_read (struct file *file, char __user * data, size_t len, loff_t * ppos)
{
    return 0;
}

static int mtk_drvb_release (struct inode *inode, struct file *file)
{
    return 0;
}

static int mtk_drvb_flush (struct file *a_pstFile, fl_owner_t a_id)
{
    return 0;
}

/*****************************************************************************
 * MTK SecureDrv Kernel Interface
 *****************************************************************************/
static struct file_operations mtk_ac_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = mtk_drvb_ioctl,
    .open           = mtk_drvb_open,
    .release        = mtk_drvb_release,
    .flush          = mtk_drvb_flush,
    .read           = mtk_drvb_read,
};

/*****************************************************************************
 * MTK SecureDrv Init
 *****************************************************************************/
static int __init mtk_ac_init (void)
{
    int ret, i;
    unsigned int num_pages = 0;
    
    printk("[%s] init :", MOD);
    mtk_ac_devno = MKDEV (MTK_DRVB_MAJOR, 0);

    ret = register_chrdev_region (mtk_ac_devno, 1, AC_DEVNAME);

    if (ret)
    {
        printk("[%s] Register character device failed\n", MOD);
        return ret;
    }

    mtk_ac_cdev = cdev_alloc();
    mtk_ac_cdev->owner = THIS_MODULE;
    mtk_ac_cdev->ops = &mtk_ac_fops;

    ret = cdev_add (mtk_ac_cdev, mtk_ac_devno, 1);
    
    if (ret)
    {
        printk("[%s] Char device add failed\n", MOD);
        unregister_chrdev_region (mtk_ac_devno, 1);
        return ret;
    }
    
    num_pages = sec_ac_alloc_mem();
    
    if(num_pages)
    {
        goto err_free_xbuf;
    }

    printk("Done");
    
    return 0;

err_free_xbuf:
    
    printk("[%s] allocate buffer failed %u \n", MOD, num_pages);
    sec_ac_free_mem(num_pages);   

    return -ENOMEM;
}

/*****************************************************************************
 * MTK SecureDrv De-Init
 *****************************************************************************/
static void __exit mtk_ac_exit (void)
{    
    printk("[%s] exit : ", MOD);
    cdev_del (mtk_ac_cdev);
    unregister_chrdev_region(mtk_ac_devno, 1);    
    sec_ac_free_mem(0);
    printk("Done\n");
}

module_init (mtk_ac_init);
module_exit (mtk_ac_exit);
MODULE_AUTHOR ("Koshi.Chiu <koshi.chiu@mediatek.com>");
MODULE_DESCRIPTION ("MEDIATEK Driver Base");
MODULE_LICENSE ("Proprietary");