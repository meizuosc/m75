#include <linux/types.h>
#include <linux/genhd.h> 
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <asm/uaccess.h>

#include <linux/mmc/sd_misc.h>

#include "efi.h"

#if 0
#include <linux/kernel.h>	/* printk() */
#include <linux/module.h>
#include <linux/types.h>	/* size_t */
#include <linux/slab.h>		/* kmalloc() */

#include <linux/mmc/sd_misc.h>
#endif

#define USING_XLOG

#ifdef USING_XLOG 
#include <linux/xlog.h>

#define TAG     "PART_KL"

#define part_err(fmt, args...)       \
    xlog_printk(ANDROID_LOG_ERROR, TAG, fmt, ##args)
#define part_info(fmt, args...)      \
    xlog_printk(ANDROID_LOG_INFO, TAG, fmt, ##args)

#else

#define TAG     "[PART_KL]"

#define part_err(fmt, args...)       \
    printk(KERN_ERR TAG);           \
    printk(KERN_CONT fmt, ##args) 
#define part_info(fmt, args...)      \
    printk(KERN_NOTICE TAG);        \
    printk(KERN_CONT fmt, ##args)

#endif


struct part_t {
    u64 start;
    u64 size;
    u32 part_id;
    u8 name[64];
};

struct partition_package {
    u64 signature;
    u32 version;
    u32 nr_parts;
    u32 sizeof_partition;
    //struct part_t *part_info;
};

#define PARTITION_PACKAGE_SIGNATURE
#define PARTITION_PACKAGE_VERSION


struct hd_struct *get_part(char *name)
{
    dev_t devt;
    int partno;
    struct disk_part_iter piter;
    struct gendisk *disk;
    struct hd_struct *part = NULL; 
    
    if (!name)
        return part;

    devt = blk_lookup_devt("mmcblk0", 0);
    disk = get_gendisk(devt, &partno);

    if (!disk || get_capacity(disk) == 0)
        return 0;

	disk_part_iter_init(&piter, disk, 0);
	while ((part = disk_part_iter_next(&piter))) {
        if (part->info && !strcmp(part->info->volname, name)) {
            get_device(part_to_dev(part));
            break;
        }
	}
	disk_part_iter_exit(&piter);
    
    return part;
}
EXPORT_SYMBOL(get_part);


void put_part(struct hd_struct *part)
{
    disk_put_part(part);
}
EXPORT_SYMBOL(put_part);


static int partinfo_show_proc(struct seq_file *m, void *v)
{
    dev_t devt;
    int partno;
    struct disk_part_iter piter;
    struct gendisk *disk;
    struct hd_struct *part; 
    u64 last = 0;

    devt = blk_lookup_devt("mmcblk0", 0);
    disk = get_gendisk(devt, &partno), 

    seq_printf(m, "%-16s %-16s\t%-16s\n", "Name", "Start", "Size");

    if (!disk || get_capacity(disk) == 0)
        return 0;

	disk_part_iter_init(&piter, disk, 0);
    seq_printf(m, "%-16s 0x%016llx\t0x%016llx\n", "pgpt", 0ULL, 512 * 1024ULL);

    while ((part = disk_part_iter_next(&piter))) {
        seq_printf(m, "%-16s 0x%016llx\t0x%016llx\n", 
            part->info ? (char *)(part->info->volname) : "unknown",
            part->start_sect * 512,
            part->nr_sects * 512);
        last = (part->start_sect + part->nr_sects) * 512;
	}

    seq_printf(m, "%-16s 0x%016llx\t0x%016llx\n", "sgpt", last, 512 * 1024ULL);
	disk_part_iter_exit(&piter);

	return 0;
}

static int partinfo_open(struct inode *inode, struct file *file)
{
	return single_open(file, partinfo_show_proc, inode);
}

static const struct file_operations partinfo_proc_fops = { 
    .open       = partinfo_open,
    .read       = seq_read,
    .llseek     = seq_lseek,
    .release    = single_release,
};


static int get_partition_num(struct gendisk *disk)
{
    struct disk_part_tbl *ptbl = disk->part_tbl;

    return ptbl->len - 1;
}

static struct partition_package *alloc_partition_package(struct gendisk *disk, int *len)
{
    struct partition_package *package;

    *len = sizeof(*package) + get_partition_num(disk) * sizeof(struct part_t);
    package = kzalloc(*len, GFP_KERNEL);    
    if (!package) {
        return NULL;
    }

    package->signature = 0x1;
    package->version = 0x1;
    package->nr_parts = get_partition_num(disk);
    package->sizeof_partition = sizeof(struct part_t);

    return package;
}

static int get_partition_package(struct gendisk *disk, struct partition_package *package)
{
    struct part_t *pinfo;
    struct disk_part_iter piter;
    struct hd_struct *part; 

    pinfo = (void*)package + sizeof(*package);

	disk_part_iter_init(&piter, disk, 0);
	while ((part = disk_part_iter_next(&piter))) {
        pinfo->start = part->start_sect * 512;
        pinfo->size = part->nr_sects * 512;
        pinfo->part_id = EMMC_PART_USER;
        if (part->info) {
            memcpy(pinfo->name, part->info->volname, 64);
        }
        part_info("%-16s: 0x%016llx, 0x%016llx\n", pinfo->name, pinfo->start, pinfo->size);
        pinfo++;
	}

	disk_part_iter_exit(&piter);

    return 0;
}

static int valid_package(struct partition_package *package, int size)
{
    int err = 0;
    int num = package->nr_parts;
    if (size != sizeof(*package) + num * sizeof(struct part_t)) {
        part_err("valid_package: num=%d, total_size=%d\n", num, size);
        err = 1;
    }

    return err;
}

extern int update_partition_table(struct partition_package *package);

static int reload_partition_table(void)
{
    struct file *filp;
    mm_segment_t old_fs;
    long ret;

    filp = filp_open("/dev/block/mmcblk0", O_RDWR, 0); 
    if (IS_ERR(filp)) {
        part_err("%s: open error(%d)\n", __func__, PTR_ERR(filp));
        return -EIO;
    }   

    if (!filp->f_op || !filp->f_op->unlocked_ioctl) {
        part_err("%s: operation not supported\n", __func__);
        return -EIO;
    }   

    old_fs = get_fs();
    set_fs(KERNEL_DS);

    ret = filp->f_op->unlocked_ioctl(filp, BLKRRPART, 0);
    if (ret < 0) {
        part_err("%s: rrpart error(%d)\n", __func__, ret);
    }   

    set_fs(old_fs);
    filp_close(filp, NULL);

    return 0;
}

static int upgrade_handler(struct partition_package *package, int size)
{
    int err;

    part_info("upgrade_handler: \n");

    //1. check parameter valid, unpack data
    err = valid_package(package, size);
    if (err) {
        goto out;
    }

    //2. update PGPT/SGPT
    err = update_partition_table(package);
    if (err) {
        goto out;
    }

#if 1
    //3. update disk info
    err = reload_partition_table();
    if (err) {
        goto out;
    }
#endif
out:

//fail_malloc:
    return err;
}


static ssize_t upgrade_proc_read(struct file *file, char __user *buf, 
            size_t count, loff_t *ppos)
{
    dev_t devt;
    int partno;
    struct gendisk *disk;

    struct partition_package *package;
    int len; 
    size_t ret;

    devt = blk_lookup_devt("mmcblk0", 0);
    disk = get_gendisk(devt, &partno);

    if (!disk || get_capacity(disk) == 0)
        return 0;

    package = alloc_partition_package(disk, &len);
    if (!package) {
        ret = -ENOMEM;
        part_err("upgrade_proc_read: fail to malloc package\n");
        goto fail_malloc;
    }

    get_partition_package(disk, package);

    ret = simple_read_from_buffer(buf, count, ppos, package, len);

    kfree(package);

fail_malloc:
    return ret;
}

static ssize_t upgrade_proc_write(struct file *file, const char __user *buf, 
            size_t count, loff_t *ppos)
{
    struct partition_package *package;
    int err;
    size_t size;
    //int len;

    package = kzalloc(count, GFP_KERNEL);
    if (!package) {
        err = -ENOMEM;
        part_err("upgrade_proc_write: fail to malloc package\n");
        goto fail_malloc;
    }

    size = simple_write_to_buffer(package, sizeof(*package), ppos, buf, count);
    if (size < 0) {
        err = size;
        part_err("upgrade_proc_write: fail to receive data(%d)\n", size);
        goto out;
    }

    err = upgrade_handler(package, size);
    
out:
    kfree(package);
fail_malloc:
    if (err)
        return err;
    else
        return count;
}


static const struct file_operations upgrade_proc_fops = { 
    .read = upgrade_proc_read,
    .write = upgrade_proc_write,
};


static int __init partition_init(void)
{
    struct proc_dir_entry *partinfo_proc, *upgrade_proc;

    partinfo_proc = proc_create("partinfo", 0444, NULL, &partinfo_proc_fops);
    if (!partinfo_proc) {
        part_err("[%s]fail to register /proc/partinfo\n", __func__);
    }

    upgrade_proc = proc_create("upgrade", 0600, NULL, &upgrade_proc_fops);
    if (!upgrade_proc) {
        part_err("[%s]fail to register /proc/upgrade\n", __func__);
    }

    return 0;
}

static void __exit partition_exit(void)
{
    remove_proc_entry("partinfo", NULL);
    remove_proc_entry("upgrade", NULL);
}

module_init(partition_init);
module_exit(partition_exit);
