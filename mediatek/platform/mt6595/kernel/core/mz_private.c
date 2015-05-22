#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <mach/mz_private.h>

struct tag_lk_info g_lk_info = {0,};

static int lk_version_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "0x%x\n", g_lk_info.lk_version);
	return 0;
}
static int lk_mode_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "0x%x\n", g_lk_info.lk_mode);
	return 0;
}

static int hw_version_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "0x%x\n", g_lk_info.hw_version);
	return 0;
}

static int sw_version_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "0x%X\n", g_lk_info.sw_version);
	return 0;
}
static int sn_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", g_lk_info.sn);
	return 0;
}

static int psn_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", g_lk_info.psn);
	return 0;
}

static int rtx_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", g_lk_info.rsv[1]);
	return 0;
}
extern int sec_schip_enabled(void);
static int sec_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", sec_schip_enabled());
	return 0;
}
static int lcd_id_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "0x%x\n", g_lk_info.rsv[2]);
	return 0;
}
#define PROC_FOPS_RO(name)	\
	static int name##_proc_open(struct inode *inode, struct file *file)	\
	{									\
		return single_open(file, name##_proc_show, PDE_DATA(inode));	\
	}									\
	static const struct file_operations name##_proc_fops = {		\
		.owner          = THIS_MODULE,					\
		.open           = name##_proc_open,				\
		.read           = seq_read,					\
		.llseek         = seq_lseek,					\
		.release        = single_release,				\
	}

#define PROC_ENTRY(name) {__stringify(name), &name##_proc_fops}
PROC_FOPS_RO(lk_version);
PROC_FOPS_RO(lk_mode);
PROC_FOPS_RO(hw_version);
PROC_FOPS_RO(sw_version);
PROC_FOPS_RO(sn);
PROC_FOPS_RO(psn);
PROC_FOPS_RO(rtx);
PROC_FOPS_RO(sec);
PROC_FOPS_RO(lcd_id);

struct pentry {
	const char *name;
	const struct file_operations *fops;
};
const struct pentry lk_info_entries[] = {
	PROC_ENTRY(lk_version),
	PROC_ENTRY(lk_mode),
	PROC_ENTRY(hw_version),
	PROC_ENTRY(sw_version),
	PROC_ENTRY(sn),
	PROC_ENTRY(psn),
	PROC_ENTRY(rtx),
	PROC_ENTRY(sec),
	PROC_ENTRY(lcd_id),
};
unsigned int mz_system_root_flag = 0;
EXPORT_SYMBOL_GPL(mz_system_root_flag);

static int __init proc_lk_info_init(void)
{
	struct proc_dir_entry *dir_entry = NULL;
	int i = 0;
	
	mz_system_root_flag = g_lk_info.rsv[0]; //root area enable.

	dir_entry = proc_mkdir("lk_info", NULL);
	if (!dir_entry) {
		pr_err("LK_INFO: Failed to create /proc/ entry\n");
		return -ENOMEM;
	}
	
	for (i = 0; i < ARRAY_SIZE(lk_info_entries); i++) {
		if (! proc_create(lk_info_entries[i].name, S_IRUGO, dir_entry, lk_info_entries[i].fops))
			pr_err("LK_INFO: Failed to create /proc/lk_info entry nodes\n");
	}
    return 0;
}
module_init(proc_lk_info_init);
