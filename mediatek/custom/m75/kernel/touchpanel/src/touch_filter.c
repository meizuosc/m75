#include <linux/slab.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <asm/uaccess.h>

#include "tpd.h"

#define TOUCH_FILTER 1

#if TOUCH_FILTER
#define TPD_FILTER_PARA {1, 146} //{enable, pixel density}
#else 
#define TPD_FILTER_PARA {0,0}
#endif
static struct tpd_filter_t tpd_filter = TPD_FILTER_PARA;



static  int tpd_v_magnify_x = 10 ;
static  int tpd_v_magnify_y = 12 ;

// for magnify velocity********************************************
#define TOUCH_IOC_MAGIC 'A'

#define TPD_GET_VELOCITY_CUSTOM_X _IO(TOUCH_IOC_MAGIC,0)
#define TPD_GET_VELOCITY_CUSTOM_Y _IO(TOUCH_IOC_MAGIC,1)
#define TPD_GET_FILTER_PARA _IOWR(TOUCH_IOC_MAGIC,2,struct tpd_filter_t) 


static int tpd_misc_open(struct inode *inode, struct file *file)
{
    return nonseekable_open(inode, file);
}

static int tpd_misc_release(struct inode *inode, struct file *file)
{
    return 0;
}

static long tpd_unlocked_ioctl(struct file *file, unsigned int cmd,
                               unsigned long arg)
{
    //char strbuf[256];
    void __user *data;

    long err = 0;

    if (_IOC_DIR(cmd) & _IOC_READ)
    {
        err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
    }
    else if (_IOC_DIR(cmd) & _IOC_WRITE)
    {
        err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
    }

    if (err)
    {
        printk("tpd: access error: %08X, (%2d, %2d)\n", cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
        return -EFAULT;
    }


    switch (cmd)
    {
        case TPD_GET_VELOCITY_CUSTOM_X:
            data = (void __user *) arg;

            if (data == NULL)
            {
                err = -EINVAL;
                break;
            }

            if (copy_to_user(data, &tpd_v_magnify_x, sizeof(tpd_v_magnify_x)))
            {
                err = -EFAULT;
                break;
            }

            break;

        case TPD_GET_VELOCITY_CUSTOM_Y:
            data = (void __user *) arg;

            if (data == NULL)
            {
                err = -EINVAL;
                break;
            }

            if (copy_to_user(data, &tpd_v_magnify_y, sizeof(tpd_v_magnify_y)))
            {
                err = -EFAULT;
                break;
            }

            break;
        case TPD_GET_FILTER_PARA:
            data = (void __user *) arg;

            if (data == NULL)
            {
                err = -EINVAL;
                printk("tpd: TPD_GET_FILTER_PARA IOCTL CMD: data is null\n");
                break;
            }

            if(copy_to_user(data, &tpd_filter, sizeof(struct tpd_filter_t)))
            {
                printk("tpd: TPD_GET_FILTER_PARA IOCTL CMD: copy data error\n");
                err = -EFAULT;
                break;
            }
            break;
            
        default:
            printk("tpd: unknown IOCTL: 0x%08x\n", cmd);
            err = -ENOIOCTLCMD;
            break;

    }

    return err;
}


static struct file_operations tpd_fops =
{
//	.owner = THIS_MODULE,
    .open = tpd_misc_open,
    .release = tpd_misc_release,
    .unlocked_ioctl = tpd_unlocked_ioctl,
};
/*----------------------------------------------------------------------------*/
static struct miscdevice tpd_misc_device =
{
    .minor = MISC_DYNAMIC_MINOR,
    .name = "touch",
    .fops = &tpd_fops,
};

int mtk_tpd_register_misc(void)
{
	 misc_register(&tpd_misc_device);
	 printk("mtk_tpd: tpd_misc_device register failed\n");
    return 0;
}

