#ifndef __INTERFACE_H__
#define __INTERFACE_H__

#include <linux/fs.h>

#ifdef MET_USER_EVENT_SUPPORT
extern int tag_reg(struct file_operations * const fops, struct kobject *kobj);
extern int tag_unreg(void);
#endif

extern struct metdevice met_stat;
extern struct metdevice met_cpupmu;
extern struct metdevice met_cookie;
extern struct metdevice met_memstat;

extern int met_parse_num(const char *str, unsigned int *value, int len);

extern struct metdevice met_cookie;

#endif	/* __INTERFACE_H__ */
