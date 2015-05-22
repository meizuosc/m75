#include <linux/slab.h>
#include "ccci_sysfs.h"

static struct ccci_info *ccci_sys_info = NULL;

extern ssize_t boot_md_show(char *buf);
extern ssize_t boot_md_store(const char *buf, size_t count);

static void ccci_obj_release(struct kobject *kobj)
{
	struct ccci_info *ccci_info_temp = container_of(kobj, struct ccci_info, kobj);
	kfree(ccci_info_temp);
	ccci_sys_info = NULL; // as ccci_info_temp==ccci_sys_info
}

static ssize_t ccci_attr_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
	ssize_t len = 0;
	struct ccci_attribute *a = container_of(attr, struct ccci_attribute, attr);

	if (a->show)
		len = a->show(buf);

	return len;
}

static ssize_t ccci_attr_store(struct kobject *kobj, struct attribute *attr, const char *buf, size_t count)
{
	ssize_t len = 0;
	struct ccci_attribute *a = container_of(attr, struct ccci_attribute, attr);

	if (a->store)
		len = a->store(buf, count);

	return len;
}

static struct sysfs_ops ccci_sysfs_ops = {
	.show  = ccci_attr_show,
	.store = ccci_attr_store
};

CCCI_ATTR(boot, 0660, &boot_md_show, &boot_md_store);

ssize_t ccci_version_show(char *buf)
{
#ifdef ENABLE_EXT_MD_DSDA	
	return snprintf(buf, 16, "%d\n", 5); // DSDA
#else
	return snprintf(buf, 16, "%d\n", 3); // ECCCI
#endif
}

CCCI_ATTR(version, 0644, &ccci_version_show, NULL);

unsigned int ccci_debug_enable = 0; // 0 to disable; 1 for print to ram; 2 for print to uart
static ssize_t debug_enable_show(char *buf)
{
	int curr = 0;

	curr = snprintf(buf, 16, "%d\n", ccci_debug_enable);
    return curr;
}

static ssize_t debug_enable_store(const char *buf, size_t count)
{
	ccci_debug_enable = buf[0] - '0';
	return count;
}

CCCI_ATTR(debug, 0660, &debug_enable_show, &debug_enable_store);

static struct attribute *ccci_default_attrs[] = {
	&ccci_attr_boot.attr,
	&ccci_attr_debug.attr,
	&ccci_attr_version.attr,
	NULL
};

static struct kobj_type ccci_ktype = {
	.release		= ccci_obj_release,
    .sysfs_ops 		= &ccci_sysfs_ops,
    .default_attrs 	= ccci_default_attrs
};

static void ccci_md_obj_release(struct kobject *kobj)
{
	struct ccci_modem *md = container_of(kobj, struct ccci_modem, kobj);
	CCCI_DBG_MSG(md->index, SYSFS, "md kobject release\n");
}

static ssize_t ccci_md_attr_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
	ssize_t len = 0;
	struct ccci_md_attribute *a = container_of(attr, struct ccci_md_attribute, attr);

	if (a->show)
		len = a->show(a->modem, buf);

	return len;
}

static ssize_t ccci_md_attr_store(struct kobject *kobj, struct attribute *attr, const char *buf, size_t count)
{
	ssize_t len = 0;
	struct ccci_md_attribute *a = container_of(attr, struct ccci_md_attribute, attr);

	if (a->store)
		len = a->store(a->modem, buf, count);

	return len;
}

static struct sysfs_ops ccci_md_sysfs_ops = {
	.show  = ccci_md_attr_show,
	.store = ccci_md_attr_store
};

static struct attribute *ccci_md_default_attrs[] = {
	NULL
};

static struct kobj_type ccci_md_ktype = {
	.release		= ccci_md_obj_release,
    .sysfs_ops 		= &ccci_md_sysfs_ops,
    .default_attrs 	= ccci_md_default_attrs
};

int ccci_sysfs_add_modem(struct ccci_modem *md)
{
	int ret;
	
	if(!ccci_sys_info)
		return -CCCI_ERR_SYSFS_NOT_READY;

	memset(&md->kobj, 0, sizeof(struct kobject));
	ret = kobject_init_and_add(&md->kobj, &ccci_md_ktype, &ccci_sys_info->kobj, "MDSYS%d", md->index+1);
	if (ret < 0) {
		kobject_put(&md->kobj);
        CCCI_ERR_MSG(md->index, SYSFS, "fail to add md kobject\n");
    }
	return ret;
}

int ccci_subsys_sysfs_init(void)
{
	int ret = 0;

	ccci_sys_info = kmalloc(sizeof(struct ccci_info), GFP_KERNEL);
	if (!ccci_sys_info)
		return -ENOMEM;

	memset(ccci_sys_info, 0, sizeof(struct ccci_info));

	ret = kobject_init_and_add(&ccci_sys_info->kobj, &ccci_ktype, kernel_kobj, CCCI_KOBJ_NAME);
	if (ret < 0) {
		kobject_put(&ccci_sys_info->kobj);
        CCCI_ERR_MSG(-1, SYSFS, "fail to add ccci kobject\n");
        return ret;
    }

	ccci_sys_info->ccci_attr_count = ARRAY_SIZE(ccci_default_attrs)-1;
	CCCI_DBG_MSG(-1, SYSFS, "ccci attr cnt %d\n", ccci_sys_info->ccci_attr_count);
	return ret;
}
