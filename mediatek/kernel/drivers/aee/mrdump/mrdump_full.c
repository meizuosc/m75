#include <stdarg.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/aee.h>
#include <linux/elf.h>
#include <linux/elfcore.h>
#include <linux/kallsyms.h>
#include <linux/memblock.h>
#include <linux/miscdevice.h>
#include <linux/mtk_ram_console.h>
#include <linux/reboot.h>
#include <linux/stacktrace.h>
#include <linux/vmalloc.h>
#include <asm/pgtable.h>
#include <asm/processor.h>
#include <mach/fiq_smp_call.h>
#include <mach/smp.h>
#include <linux/mrdump.h>
#include <linux/kdebug.h>

#if defined(CONFIG_SMP) && !defined(CONFIG_FIQ_GLUE)
#error "MRDUMP cannot run without CONFIG_FIQ_GLUE enabled"
#endif

extern void __inner_flush_dcache_all(void);
extern void __inner_flush_dcache_L1(void);

static bool mrdump_enable;
static int mrdump_output_device;

static const struct mrdump_platform *mrdump_plat;

static struct mrdump_control_block *mrdump_cb;
static struct mrdump_cblock_result *mrdump_result;

static void save_current_task(void)
{
	int i;
	struct stack_trace trace;
	unsigned long stack_entries[16];
	struct task_struct *tsk;
	struct mrdump_crash_record *crash_record = &mrdump_cb->crash_record;

	tsk = current_thread_info()->task;

	/* Grab kernel task stack trace */
	trace.nr_entries = 0;
	trace.max_entries = sizeof(stack_entries) / sizeof(stack_entries[0]);
	trace.entries = stack_entries;
	trace.skip = 1;
	save_stack_trace_tsk(tsk, &trace);

	for (i = 0; i < trace.nr_entries; i++) {
		int off = strlen(crash_record->backtrace);
		int plen = sizeof(crash_record->backtrace) - off;
		if (plen > 16) {
			snprintf(crash_record->backtrace + off, plen, "[<%p>] %pS\n",
				 (void *)stack_entries[i], (void *)stack_entries[i]);
		}
	}
}

static void aee_kdump_cpu_stop(void *arg, void *regs, void *svc_sp)
{
	struct mrdump_crash_record *crash_record = &mrdump_cb->crash_record;
	int cpu = 0;
	register int sp asm("sp");
	struct pt_regs *ptregs = (struct pt_regs *)regs;

	asm volatile("mov %0, %1\n\t"
		     "mov fp, %2\n\t"
		     : "=r" (sp)
		     : "r" (svc_sp), "r" (ptregs->ARM_fp)
		);
	cpu = get_HW_cpuid();

	elf_core_copy_regs(&crash_record->cpu_regs[cpu], ptregs);

	set_cpu_online(cpu, false);
	local_fiq_disable();
	local_irq_disable();

	__inner_flush_dcache_L1();
	while (1)
		cpu_relax();
}

void mrdump_platform_init(struct mrdump_control_block *cblock, const struct mrdump_platform *plat)
{
	mrdump_cb = cblock;
	mrdump_plat = plat;
}

static void __mrdump_reboot_va(AEE_REBOOT_MODE reboot_mode, struct pt_regs *regs, const char *msg,
			       va_list ap)
{
	struct mrdump_crash_record *crash_record = NULL;
	struct cpumask mask;
	int timeout, cpu;

	if (mrdump_cb == NULL) {
		return;
	}

	crash_record = &mrdump_cb->crash_record;
	local_irq_disable();
	local_fiq_disable();

	cpu = get_HW_cpuid();
	elf_core_copy_regs(&crash_record->cpu_regs[cpu], regs);

	cpumask_copy(&mask, cpu_online_mask);

	cpumask_clear_cpu(cpu, &mask);

#if defined(CONFIG_SMP) && (defined(CONFIG_FIQ_GLUE))
	fiq_smp_call_function(aee_kdump_cpu_stop, NULL, 0);
#endif

	crash_record->fault_cpu = cpu;

	/* Wait up to one second for other CPUs to stop */
	timeout = USEC_PER_SEC;
	while (num_online_cpus() > 1 && timeout--)
		udelay(1);

	vsnprintf(crash_record->msg, sizeof(crash_record->msg), msg, ap);

	save_current_task();

	/* FIXME: Check reboot_mode is valid */
	crash_record->reboot_mode = reboot_mode;
	__inner_flush_dcache_all();

	if (reboot_mode == AEE_REBOOT_MODE_NESTED_EXCEPTION) {
		while (1) {
			cpu_relax();
		}
	}

	mrdump_plat->reboot();
}

void aee_kdump_reboot(AEE_REBOOT_MODE reboot_mode, const char *msg, ...)
{
	va_list ap;
	struct pt_regs regs;

	va_start(ap, msg);
	asm volatile("stmia %1, {r0 - r15}\n\t"
		     "mrs %0, cpsr\n"
		     : "=r"(regs.uregs[16])
		     : "r" (&regs)
		     : "memory");
	__mrdump_reboot_va(reboot_mode, &regs, msg, ap);
	/* No return anymore */
	va_end(ap);
}

static int mrdump_create_dump(struct notifier_block *this, unsigned long event, void *ptr)
{
	if (mrdump_enable) {
		aee_kdump_reboot(AEE_REBOOT_MODE_KERNEL_PANIC, "kernel panic");
	} else {
		pr_err("MT-RAMDUMP no enable");
	}
	return NOTIFY_DONE;
}

void __mrdump_create_oops_dump(AEE_REBOOT_MODE reboot_mode, struct pt_regs *regs, const char *msg,
			       ...)
{
	va_list ap;

	va_start(ap, msg);
	__mrdump_reboot_va(reboot_mode, regs, msg, ap);
	va_end(ap);
}

#if 0
static int mrdump_create_oops_dump(struct notifier_block *self, unsigned long cmd, void *ptr)
{
	if (mrdump_enable) {
		struct die_args *dargs = (struct die_args *)ptr;
		__mrdump_create_oops_dump(AEE_REBOOT_MODE_KERNEL_PANIC, dargs->regs, "kernel Oops");
	} else {
		pr_err("MT-RAMDUMP no enable");
	}
	return NOTIFY_DONE;
}

static struct notifier_block mrdump_oops_blk = {
	.notifier_call = mrdump_create_oops_dump,
};
#endif

static struct notifier_block mrdump_panic_blk = {
	.notifier_call = mrdump_create_dump,
};

#if CONFIG_SYSFS

static ssize_t dump_status_show(struct kobject *kobj, struct kobj_attribute *attr, char *page)
{
	return snprintf(page, PAGE_SIZE, "%s", mrdump_result->status);
}

static ssize_t dump_log_show(struct kobject *kobj, struct kobj_attribute *attr, char *page)
{
	int index;
	char *p = page;
	memset(page, 0, PAGE_SIZE);

	for (index = 0; (index < mrdump_result->log_size) && (index < PAGE_SIZE - 1); index++, p++) {
		*p = mrdump_result->log_buf[index];
	}
	*p = 0;
	return p - page;
}

static ssize_t dump_log_size_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", mrdump_result->log_size);
}

static ssize_t manual_dump_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "Trigger manual dump with message, format \"manualdump:HelloWorld\"\n");
}

static ssize_t manual_dump_store(struct kobject *kobj, struct kobj_attribute *attr,
				 const char *buf, size_t count)
{
	if (strncmp(buf, "manualdump:", 11) == 0) {
		aee_kdump_reboot(AEE_REBOOT_MODE_MANUAL_KDUMP, buf + 11);
	}
	return count;
}

static struct kobj_attribute dump_status_attribute =
__ATTR(dump_status, 0400, dump_status_show, NULL);

static struct kobj_attribute dump_log_attribute = __ATTR(dump_log, 0440, dump_log_show, NULL);

static struct kobj_attribute dump_log_size_attribute =
__ATTR(dump_log_size, 0440, dump_log_size_show, NULL);

static struct kobj_attribute manual_dump_attribute =
__ATTR(manualdump, 0600, manual_dump_show, manual_dump_store);

static struct attribute *attrs[] = {
	&dump_status_attribute.attr,
	&dump_log_attribute.attr,
	&dump_log_size_attribute.attr,
	&manual_dump_attribute.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

static int __init mrdump_init_log(void)
{
	struct kobject *kobj;
	/* Preserved last result */
	mrdump_result = vmalloc(PAGE_ALIGN(sizeof(struct mrdump_cblock_result)));
	if (mrdump_result == NULL) {
		pr_err("%s: cannot allocate result memory\n", __func__);
		return -EINVAL;
	}
	memset(mrdump_result, 0, sizeof(struct mrdump_cblock_result));

	if (memcmp(mrdump_cb->sig, "XRDUMP01", 8) == 0) {
		memcpy(mrdump_result, &mrdump_cb->result, sizeof(struct mrdump_cblock_result));
	} else {
		snprintf(mrdump_result->status, sizeof(mrdump_result->status),
			 "NONE\nNo LK signature found\n");
	}
	kobj = kset_find_obj(module_kset, KBUILD_MODNAME);
	if (kobj) {
		if (sysfs_create_group(kobj, &attr_group)) {
			pr_err("%s: sysfs  create sysfs failed\n", __func__);
			goto error;
		}
	} else {
		pr_err("Cannot find module %s object\n", KBUILD_MODNAME);
		goto error;
	}
	return 0;

 error:
	vfree(mrdump_result);
	mrdump_result = NULL;
	return -EINVAL;
}

#else
static int __init mrdump_init_log(void)
{
	return 0;
}
#endif

static int __init mrdump_init(void)
{
	struct mrdump_machdesc *machdesc_p;

	pr_err("MT-RAMDUMP init control block %p\n", mrdump_cb);
	if (mrdump_cb == NULL) {
		pr_err("%s: No control block memory found\n", __func__);
		return -EINVAL;
	}
	if (mrdump_init_log()) {
		return -EINVAL;
	}

	memset(mrdump_cb, 0, sizeof(struct mrdump_control_block));
	memcpy(&mrdump_cb->sig, "MRDUMP01", 8);

	machdesc_p = &mrdump_cb->machdesc;
	machdesc_p->output_device = MRDUMP_DEV_EMMC;
	machdesc_p->nr_cpus = NR_CPUS;
	machdesc_p->page_offset = (void *)PAGE_OFFSET;
	machdesc_p->high_memory = (void *)high_memory;

	machdesc_p->vmalloc_start = (void *)VMALLOC_START;
	machdesc_p->vmalloc_end = (void *)VMALLOC_END;

	machdesc_p->modules_start = (void *)MODULES_VADDR;
	machdesc_p->modules_end = (void *)MODULES_END;

	machdesc_p->phys_offset = (void *)PHYS_OFFSET;
	machdesc_p->master_page_table = (void *)&swapper_pg_dir;

#if 0
	register_die_notifier(&mrdump_oops_blk);
#endif

	atomic_notifier_chain_register(&panic_notifier_list, &mrdump_panic_blk);

	return 0;
}

static int param_set_mrdump_device(const char *val, const struct kernel_param *kp)
{
	char strval[16], *strp;
	int eval;

	strlcpy(strval, val, sizeof(strval));
	strp = strstrip(strval);

	if (strcmp(strp, "null") == 0) {
		eval = MRDUMP_DEV_NULL;
	} else if (strcmp(strp, "sdcard") == 0) {
		eval = MRDUMP_DEV_SDCARD;
	} else if (strcmp(strp, "emmc") == 0) {
		eval = MRDUMP_DEV_EMMC;
	} else {
		eval = MRDUMP_DEV_NULL;
	}
	*(int *)kp->arg = eval;
	if (mrdump_cb != NULL) {
		mrdump_cb->machdesc.output_device = eval;
	}
	return 0;
}

static int param_get_mrdump_device(char *buffer, const struct kernel_param *kp)
{
	char *dev;
	if (mrdump_cb == NULL) {
		return -EIO;
	}
	switch (mrdump_cb->machdesc.output_device) {
	case MRDUMP_DEV_NULL:
		dev = "null";
		break;
	case MRDUMP_DEV_SDCARD:
		dev = "sdcard";
		break;
	case MRDUMP_DEV_EMMC:
		dev = "emmc";
		break;
	default:
		dev = "none(unknown)";
		break;
	}
	strcpy(buffer, dev);
	return strlen(dev);
}

static int param_set_mrdump_enable(const char *val, const struct kernel_param *kp)
{
	int retval = param_set_bool(val, kp);
	if ((retval == 0) && (mrdump_plat != NULL)) {
		mrdump_plat->hw_enable(mrdump_enable);
	}
	return retval;
}

struct kernel_param_ops param_ops_mrdump_enable = {
	.set = param_set_mrdump_enable,
	.get = param_get_bool,
};

param_check_bool(enable, &mrdump_enable);
module_param_cb(enable, &param_ops_mrdump_enable, &mrdump_enable, S_IRUGO | S_IWUSR);
__MODULE_PARM_TYPE(device, bool);

struct kernel_param_ops param_ops_mrdump_device = {
	.set = param_set_mrdump_device,
	.get = param_get_mrdump_device,
};

param_check_int(device, &mrdump_output_device);
module_param_cb(device, &param_ops_mrdump_device, &mrdump_output_device, S_IRUGO | S_IWUSR);
__MODULE_PARM_TYPE(device, int);

late_initcall(mrdump_init);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek MRDUMP module");
MODULE_AUTHOR("MediaTek Inc.");
