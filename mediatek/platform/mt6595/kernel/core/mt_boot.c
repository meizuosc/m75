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
#include <linux/of.h>

#include <mach/mt_boot.h>
#include <mach/mt_reg_base.h>
#include <mach/mt_typedefs.h>
#include <asm/setup.h>
//#include "devinfo.h"
extern u32 get_devinfo_with_index(u32 index);

/* hardware version register */
#define VER_BASE            (DEVINFO_BASE)
#define APHW_CODE           (VER_BASE)
#define APHW_SUBCODE        (VER_BASE + 0x04)
#define APHW_VER            (VER_BASE + 0x08)
#define APSW_VER            (VER_BASE + 0x0C)

/* this vairable will be set by mt_fixup.c */
META_COM_TYPE g_meta_com_type = META_UNKNOWN_COM;
unsigned int g_meta_com_id = 0;

struct meta_driver {
    struct device_driver driver;
    const struct platform_device_id *id_table;
};

static struct meta_driver meta_com_type_info =
{
    .driver  = {
        .name = "meta_com_type_info",
        .bus = &platform_bus_type,
        .owner = THIS_MODULE,
    },
    .id_table = NULL,
};

static struct meta_driver meta_com_id_info =
{
    .driver = {
        .name = "meta_com_id_info",
        .bus = &platform_bus_type,
        .owner = THIS_MODULE,
    },
    .id_table = NULL,
};

static ssize_t boot_show(struct kobject *kobj, struct attribute *a, char *buf)
{
    if (!strncmp(a->name, INFO_SYSFS_ATTR, strlen(INFO_SYSFS_ATTR)))
    {
        return sprintf(buf, "%04X%04X%04X%04X %04X %04X\n", get_chip_code(), get_chip_hw_subcode(),
                            get_chip_hw_ver_code(), get_chip_sw_ver_code(),
                            mt_get_chip_sw_ver(), mt_get_chip_id());
    }
    else
    {
        return sprintf(buf, "%d\n", get_boot_mode());
    }
}

static ssize_t boot_store(struct kobject *kobj, struct attribute *a, const char *buf, size_t count)
{
    return count;
}


/* boot object */
static struct kobject boot_kobj;
static struct sysfs_ops boot_sysfs_ops = {
    .show = boot_show,
    .store = boot_store
};

/* boot attribute */
struct attribute boot_attr = {BOOT_SYSFS_ATTR, 0644};
struct attribute info_attr = {INFO_SYSFS_ATTR, 0644};
static struct attribute *boot_attrs[] = {
    &boot_attr,
    &info_attr,
    NULL
};

/* boot type */
static struct kobj_type boot_ktype = {
    .sysfs_ops = &boot_sysfs_ops,
    .default_attrs = boot_attrs
};

/* boot device node */
static dev_t boot_dev_num;
static struct cdev boot_cdev;
static struct file_operations boot_fops = {
    .owner = THIS_MODULE,
    .open = NULL,
    .release = NULL,
    .write = NULL,
    .read = NULL,
    .unlocked_ioctl = NULL
};

/* boot device class */
static struct class *boot_class;
static struct device *boot_device;

/* return hardware version */
unsigned int get_chip_code(void)
{
    return DRV_Reg32(APHW_CODE);
}

unsigned int get_chip_hw_ver_code(void)
{
    return DRV_Reg32(APHW_VER);
}

unsigned int get_chip_sw_ver_code(void)
{
    return DRV_Reg32(APSW_VER);
}

unsigned int get_chip_hw_subcode(void)
{
    return DRV_Reg32(APHW_SUBCODE);
}

unsigned int mt_get_chip_id(void)
{
    unsigned int chip_id = get_chip_code();
    /*convert id if necessary*/
    return chip_id;
}

CHIP_SW_VER mt_get_chip_sw_ver(void)
{
    return (CHIP_SW_VER)get_chip_sw_ver_code();
}

bool com_is_enable(void)  // usb android will check whether is com port enabled default. in normal boot it is default enabled.
{
    if(get_boot_mode() == NORMAL_BOOT)
	{
        return false;
	}
	else
	{
        return true;
	}
}

void set_meta_com(META_COM_TYPE type, unsigned int id)
{
    g_meta_com_type = type;
    g_meta_com_id = id;
}

META_COM_TYPE get_meta_com_type(void)
{
    return g_meta_com_type;
}

unsigned int get_meta_com_id(void)
{
    return g_meta_com_id;
}

static ssize_t meta_com_type_show(struct device_driver *driver, char *buf)
{
  return sprintf(buf, "%d\n", g_meta_com_type);
}

static ssize_t meta_com_type_store(struct device_driver *driver, const char *buf, size_t count)
{
  /*Do nothing*/
  return count;
}

DRIVER_ATTR(meta_com_type_info, 0644, meta_com_type_show, meta_com_type_store);


static ssize_t meta_com_id_show(struct device_driver *driver, char *buf)
{
  return sprintf(buf, "%d\n", g_meta_com_id);
}

static ssize_t meta_com_id_store(struct device_driver *driver, const char *buf, size_t count)
{
  /*Do nothing*/
  return count;
}

DRIVER_ATTR(meta_com_id_info, 0644, meta_com_id_show, meta_com_id_store);

static int __init create_sysfs(void)
{
#ifdef CONFIG_OF
    const struct tag *tags;
#endif

    int ret;
    BOOTMODE bm = get_boot_mode();

    /* allocate device major number */
    if (alloc_chrdev_region(&boot_dev_num, 0, 1, BOOT_DEV_NAME) < 0) {
        pr_warn("fail to register chrdev\n");
        return -1;
    }

    /* add character driver */
    cdev_init(&boot_cdev, &boot_fops);
    ret = cdev_add(&boot_cdev, boot_dev_num, 1);
    if (ret < 0) {
        pr_warn("fail to add cdev\n");
        return ret;
    }

    /* create class (device model) */
    boot_class = class_create(THIS_MODULE, BOOT_DEV_NAME);
    if (IS_ERR(boot_class)) {
        pr_warn("fail to create class\n");
        return (int)boot_class;
    }

    boot_device = device_create(boot_class, NULL, boot_dev_num, NULL, BOOT_DEV_NAME);
    if (IS_ERR(boot_device)) {
        pr_warn("fail to create device\n");
        return (int)boot_device;
    }

    /* add kobject */
    ret = kobject_init_and_add(&boot_kobj, &boot_ktype, &(boot_device->kobj), BOOT_SYSFS);
    if (ret < 0) {
        pr_warn("fail to add kobject\n");
        return ret;
    }

    pr_notice("CHIP = 0x%04x 0x%04x\n", get_chip_code(), get_chip_hw_subcode());
    pr_notice("CHIP = 0x%04x 0x%04x 0x%04x 0x%04x\n", mt_get_chip_info(CHIP_INFO_FUNCTION_CODE), 
               mt_get_chip_info(CHIP_INFO_PROJECT_CODE), mt_get_chip_info(CHIP_INFO_DATE_CODE), 
               mt_get_chip_info(CHIP_INFO_FAB_CODE));

#ifdef CONFIG_OF
    if (of_chosen) {
        tags = (struct tag *)of_get_property(of_chosen, "atag,meta", NULL);
        if (tags) {
            g_meta_com_type = tags->u.meta_com.meta_com_type;
            g_meta_com_id = tags->u.meta_com.meta_com_id;
            pr_notice("[%s] g_meta_com_type = %d, g_meta_com_id = %d.\n", __func__, g_meta_com_type, g_meta_com_id);
        }
        else
            pr_notice("[%s] No atag,meta found !\n", __func__);
    }
    else
        pr_notice("[%s] of_chosen is NULL !\n", __func__);
#endif

    if(bm == META_BOOT || bm == ADVMETA_BOOT || bm == ATE_FACTORY_BOOT || bm == FACTORY_BOOT)
    {
        /* register driver and create sysfs files */
        ret = driver_register(&meta_com_type_info.driver);
        if (ret)
        {
            pr_warn("fail to register META COM TYPE driver\n");
        }
        ret = driver_create_file(&meta_com_type_info.driver, &driver_attr_meta_com_type_info);
        if (ret)
        {
            pr_warn("fail to create META COM TPYE sysfs file\n");
        }

        ret = driver_register(&meta_com_id_info.driver);
        if (ret)
        {
            pr_warn("fail to register META COM ID driver\n");
        }
        ret = driver_create_file(&meta_com_id_info.driver, &driver_attr_meta_com_id_info);
        if (ret)
        {
            pr_warn("fail to create META COM ID sysfs file\n");
        }
    }    
    
    return 0;
}

static void __exit destroy_sysfs(void)
{
    cdev_del(&boot_cdev);
}

struct chip_inf_entry
{
    const char*     name;
    CHIP_INFO       id;
    int             (*to_str)(char* buf, size_t len, int val);
};

unsigned int mt_get_chip_info(CHIP_INFO id)
{
    
    if (CHIP_INFO_HW_CODE == id)
        return get_chip_code();
    else if (CHIP_INFO_HW_SUBCODE == id)
        return get_chip_hw_subcode();
    else if (CHIP_INFO_HW_VER == id)
        return get_chip_hw_ver_code();
    else if (CHIP_INFO_SW_VER == id)
        return get_chip_sw_ver_code();
    else if (CHIP_INFO_FUNCTION_CODE == id) 
    {       
        unsigned int val = get_devinfo_with_index(24) & 0xFF000000; //[31:24]
        return (val >> 24);
    }
    else if (CHIP_INFO_PROJECT_CODE == id)
    {
        unsigned int val = get_devinfo_with_index(24) & 0x00003FFF; //[13:0]        
        return (val);
    }
    else if (CHIP_INFO_DATE_CODE == id)
    {
        unsigned int val = get_devinfo_with_index(24) & 0x00FFC000; //[23:14]
        return (val >> 14);
    }    
    else if (CHIP_INFO_FAB_CODE == id)
    {
        unsigned int val = get_devinfo_with_index(25) & 0x70000000; //[30:28]    
        return (val >> 28);
    }
    
    return 0x0000ffff;
}

static int hex2str(char* buf, size_t len, int val)
{
    return snprintf(buf, len, "%04X", val);
}

static int dec2str(char* buf, size_t len, int val)
{
    return snprintf(buf, len, "%04d", val);    
}

static int date2str(char* buf, size_t len, int val)
{
    unsigned int year = ((val & 0x3C0) >> 6) + 2012;
    unsigned int week = (val & 0x03F);
    return snprintf(buf, len, "%04d%02d", year, week);
}

static struct proc_dir_entry *chip_proc = NULL;
static struct chip_inf_entry chip_ent[] = 
{
    {"hw_code",     CHIP_INFO_HW_CODE,      hex2str},
    {"hw_subcode",  CHIP_INFO_HW_SUBCODE,   hex2str},
    {"hw_ver",      CHIP_INFO_HW_VER,       hex2str},
    {"sw_ver",      CHIP_INFO_SW_VER,       hex2str},
    {"code_proj",   CHIP_INFO_PROJECT_CODE, dec2str},
    {"code_func",   CHIP_INFO_FUNCTION_CODE,hex2str},
    {"code_fab",    CHIP_INFO_FAB_CODE,     hex2str},
    {"code_date",   CHIP_INFO_DATE_CODE,    date2str},
    {"info",        CHIP_INFO_ALL,          NULL}
};

static int chip_proc_show(struct seq_file* s, void* v)
{
    struct chip_inf_entry *ent = s->private;
    if ((ent->id > CHIP_INFO_NONE) && (ent->id < CHIP_INFO_MAX))
    {
        seq_printf(s, "%04x\n", mt_get_chip_info(ent->id));
    }
    else
    {
        int idx = 0;
        char buf[16];
        for (idx = 0; idx < sizeof(chip_ent)/sizeof(chip_ent[0]); idx++)
        {    
            struct chip_inf_entry *ent = &chip_ent[idx];
            unsigned int val = mt_get_chip_info(ent->id);
        
            if (!ent->to_str)    
                continue;
            if (0 < ent->to_str(buf, sizeof(buf), val))
                seq_printf(s, "%-16s : %6s (%04x)\n", ent->name, buf, val);
            else
                seq_printf(s, "%-16s : %6s (%04x)\n", ent->name, "NULL", val); 
        }
    }      
    return 0;
}

static int chip_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, chip_proc_show, PDE_DATA(file_inode(file)));
}

static const struct file_operations chip_proc_fops = { 
    .open           = chip_proc_open,
    .read           = seq_read,
    .llseek         = seq_lseek,
    .release        = single_release,
};

extern struct proc_dir_entry proc_root;
static void __init create_procfs(void)
{
    int idx;
    
    chip_proc = proc_mkdir_data("chip", S_IRUGO, &proc_root, NULL);
    if (NULL == chip_proc)
    {
        pr_err("create /proc/chip fails\n");
        return;
    }            
    
    for (idx = 0; idx < sizeof(chip_ent)/sizeof(chip_ent[0]); idx++)
    {
        struct chip_inf_entry *ent = &chip_ent[idx];
        if (NULL == proc_create_data(ent->name, S_IRUGO, chip_proc, &chip_proc_fops, ent))
        {
            pr_err("create /proc/chip/%s fail\n", ent->name);
            return;
        }
    }
}

static int __init boot_mod_init(void)
{
    create_sysfs();
    create_procfs();
    return 0;
}

static void __exit boot_mod_exit(void)
{
    destroy_sysfs();
}

module_init(boot_mod_init);
module_exit(boot_mod_exit);
MODULE_DESCRIPTION("MTK Boot Information Querying Driver");
MODULE_LICENSE("GPL");
EXPORT_SYMBOL(mt_get_chip_id);
EXPORT_SYMBOL(mt_get_chip_sw_ver);
EXPORT_SYMBOL(mt_get_chip_info);
