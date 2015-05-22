#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/stacktrace.h>
#include <linux/sched.h>
#include <asm/stacktrace.h>
#include <asm/memory.h>
#include <asm/traps.h>
#include <mach/wd_api.h>
#include <mach/smp.h>
#include <linux/aee.h>
#include "aee-common.h"

#define RR_PROC_NAME "reboot-reason"

/* Some chip do not have reg dump, define a weak to avoid build error */
int __weak mt_reg_dump(char *buf)
{
	return 1;
}
EXPORT_SYMBOL(mt_reg_dump);

static struct proc_dir_entry *aee_rr_file;
struct last_reboot_reason aee_rr_last_rec;

#define WDT_NORMAL_BOOT 0
#define WDT_HW_REBOOT 1
#define WDT_SW_REBOOT 2

typedef enum {
	BR_POWER_KEY = 0,
	BR_USB,
	BR_RTC,
	BR_WDT,
	BR_WDT_BY_PASS_PWK,
	BR_TOOL_BY_PASS_PWK,
	BR_2SEC_REBOOT,
	BR_UNKNOWN,
	BR_KE_REBOOT
} boot_reason_t;

char boot_reason[][16] =
    { "keypad", "usb_chg", "rtc", "wdt", "reboot", "tool reboot", "smpl", "others", "kpanic" };

void aee_rr_last(struct last_reboot_reason *lrr)
{
	memcpy(&aee_rr_last_rec, lrr, sizeof(struct last_reboot_reason));
}

static int aee_rr_reboot_reason_proc_show(struct seq_file *m, void *v)
{
	int i;
	static char reg_buf[1024];

	seq_printf(m, "WDT status: %d\n fiq step: %u\n",
		   aee_rr_last_rec.wdt_status, aee_rr_last_rec.fiq_step);
	for (i = 0; i < NR_CPUS; i++) {
		seq_printf(m, "CPU %d\n"
			   "  irq: enter(%d, %llu) quit(%d, %llu)\n"
			   "  sched: %llu, \"%s\"\n"
			   "  hotplug: %d, %d, 0x%llx\n", i,
			   aee_rr_last_rec.last_irq_enter[i],
			   aee_rr_last_rec.jiffies_last_irq_enter[i],
			   aee_rr_last_rec.last_irq_exit[i],
			   aee_rr_last_rec.jiffies_last_irq_exit[i],
			   aee_rr_last_rec.jiffies_last_sched[i],
			   aee_rr_last_rec.last_sched_comm[i], aee_rr_last_rec.hotplug_data1[i],
			   aee_rr_last_rec.hotplug_data2[i], aee_rr_last_rec.hotplug_data3[i]);
	}
	seq_printf(m, "mcdi_wfi: 0x%x\n", aee_rr_last_rec.mcdi_wfi);
	if (mt_reg_dump(reg_buf) == 0)
		seq_printf(m, "%s\n", reg_buf);

	return 0;
}

static int aee_rr_reboot_reason_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, aee_rr_reboot_reason_proc_show, NULL);
}

static const struct file_operations aee_rr_reboot_reason_proc_fops = {
	.open = aee_rr_reboot_reason_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};


void aee_rr_proc_init(struct proc_dir_entry *aed_proc_dir)
{
	aee_rr_file = proc_create(RR_PROC_NAME,
				  0444, aed_proc_dir, &aee_rr_reboot_reason_proc_fops);
	if (aee_rr_file == NULL) {
		LOGE("%s: Can't create rr proc entry\n", __func__);
	}
}
EXPORT_SYMBOL(aee_rr_proc_init);

void aee_rr_proc_done(struct proc_dir_entry *aed_proc_dir)
{
	remove_proc_entry(RR_PROC_NAME, aed_proc_dir);
}
EXPORT_SYMBOL(aee_rr_proc_done);

/* define /sys/bootinfo/powerup_reason */
static ssize_t powerup_reason_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int g_boot_reason = 0;
	char *br_ptr;
	if ((br_ptr = strstr(saved_command_line, "boot_reason=")) != 0) {
		/* get boot reason */
		g_boot_reason = br_ptr[12] - '0';
		LOGE("g_boot_reason=%d\n", g_boot_reason);
		if (aee_rr_last_rec.fiq_step == 0)
			g_boot_reason = BR_KE_REBOOT;

		return sprintf(buf, "%s\n", boot_reason[g_boot_reason]);
	} else
		return 0;

}

static struct kobj_attribute powerup_reason_attr = __ATTR_RO(powerup_reason);

struct kobject *bootinfo_kobj;
EXPORT_SYMBOL(bootinfo_kobj);

static struct attribute *bootinfo_attrs[] = {
	&powerup_reason_attr.attr,
	NULL
};

static struct attribute_group bootinfo_attr_group = {
	.attrs = bootinfo_attrs,
};

int ksysfs_bootinfo_init(void)
{
	int error;

	bootinfo_kobj = kobject_create_and_add("bootinfo", NULL);
	if (!bootinfo_kobj) {
		return -ENOMEM;
	}

	error = sysfs_create_group(bootinfo_kobj, &bootinfo_attr_group);
	if (error)
		kobject_put(bootinfo_kobj);

	return error;
}

void ksysfs_bootinfo_exit(void)
{
	kobject_put(bootinfo_kobj);
}

/* end sysfs bootinfo */

static inline unsigned int get_linear_memory_size(void)
{
	return (unsigned long)high_memory - PAGE_OFFSET;
}

static char nested_panic_buf[1024];
int aee_nested_printf(const char *fmt, ...)
{
	va_list args;
	static int total_len;
	va_start(args, fmt);
	total_len += vsnprintf(nested_panic_buf, sizeof(nested_panic_buf), fmt, args);
	va_end(args);

	aee_sram_fiq_log(nested_panic_buf);

	return total_len;
}

static void print_error_msg(int len)
{
	static char error_msg[][50] = { "Bottom unaligned", "Bottom out of kernel addr",
		"Top out of kernel addr", "Buf len not enough"
	};
	int tmp = (-len) - 1;
	aee_sram_fiq_log(error_msg[tmp]);
}

/*save stack as binary into buf,
 *return value

    -1: bottom unaligned
    -2: bottom out of kernel addr space
    -3 top out of kernel addr addr
    -4: buff len not enough
    >0: used length of the buf
 */
int aee_dump_stack_top_binary(char *buf, int buf_len, unsigned long bottom, unsigned long top)
{
	/*should check stack address in kernel range */
	if (bottom & 3) {
		return -1;
	}
	if (!((bottom >= (PAGE_OFFSET + THREAD_SIZE)) &&
	      (bottom <= (PAGE_OFFSET + get_linear_memory_size())))) {
		return -2;
	}

	if (!((top >= (PAGE_OFFSET + THREAD_SIZE)) &&
	      (top <= (PAGE_OFFSET + get_linear_memory_size())))) {
		return -3;
	}

	if (buf_len < top - bottom) {
		return -4;
	}

	memcpy((void *)buf, (void *)bottom, top - bottom);

	return top - bottom;
}

/* extern void mt_fiq_printf(const char *fmt, ...); */
void *aee_excp_regs;
static atomic_t nested_panic_time = ATOMIC_INIT(0);

inline void aee_print_regs(struct pt_regs *regs)
{
	aee_nested_printf("[r0-r15,cpsr]%08lx %08lx %08lx %08lx %08lx %08lx %08lx "
			  "%08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
			  regs->ARM_r0, regs->ARM_r1, regs->ARM_r2,
			  regs->ARM_r3, regs->ARM_r4, regs->ARM_r5,
			  regs->ARM_r6, regs->ARM_r7, regs->ARM_r8,
			  regs->ARM_r9, regs->ARM_r10,
			  regs->ARM_fp, regs->ARM_ip, regs->ARM_sp,
			  regs->ARM_lr, regs->ARM_pc, regs->ARM_cpsr);
}

#define AEE_MAX_EXCP_FRAME	32
inline void aee_print_bt(struct pt_regs *regs)
{
	int i;
	unsigned long high, bottom, fp;
	struct stackframe cur_frame;
	struct pt_regs *exp_regs;
	bottom = regs->ARM_sp;
	if (!virt_addr_valid(bottom)) {
		aee_nested_printf("invalid sp[%x]\n", regs);
		return;
	}
	high = ALIGN(bottom, THREAD_SIZE);
	cur_frame.lr = regs->ARM_lr;
	cur_frame.fp = regs->ARM_fp;
	cur_frame.pc = regs->ARM_pc;
	for (i = 0; i < AEE_MAX_EXCP_FRAME; i++) {
		fp = cur_frame.fp;
		if ((fp < (bottom + 12)) || ((fp + 4) >= (high + 8192))) {
			if (fp != 0)
				aee_nested_printf("fp(%x)", fp);
			break;
		}
		cur_frame.fp = *(unsigned long *)(fp - 12);
		cur_frame.lr = *(unsigned long *)(fp - 4);
		cur_frame.pc = *(unsigned long *)fp;
		if (!
		    ((cur_frame.lr >= (PAGE_OFFSET + THREAD_SIZE))
		     && virt_addr_valid(cur_frame.lr)))
			break;
		if (in_exception_text(cur_frame.pc)) {
			exp_regs = (struct pt_regs *)(fp + 4);
			cur_frame.lr = exp_regs->ARM_pc;
		}
		aee_nested_printf("%08lx, ", cur_frame.lr);
	}
	aee_nested_printf("\n");
	return;
}

inline int aee_nested_save_stack(struct pt_regs *regs)
{
	int len = 0;
	if (!virt_addr_valid(regs->ARM_sp))
		return -1;
	aee_nested_printf("[%08lx %08lx]\n", regs->ARM_sp, regs->ARM_sp + 256);

	len = aee_dump_stack_top_binary(nested_panic_buf, sizeof(nested_panic_buf),
					regs->ARM_sp, regs->ARM_sp + 256);
	if (len > 0)
		aee_sram_fiq_save_bin(nested_panic_buf, len);
	else
		print_error_msg(len);
	return len;
}

int aee_in_nested_panic(void)
{
	return (atomic_read(&nested_panic_time) &&
		((aee_rr_curr_fiq_step() & ~(AEE_FIQ_STEP_KE_NESTED_PANIC - 1)) ==
		 AEE_FIQ_STEP_KE_NESTED_PANIC));
}

static inline void aee_rec_step_nested_panic(int step)
{
	if (step < 64)
		aee_rr_rec_fiq_step(AEE_FIQ_STEP_KE_NESTED_PANIC + step);
}

void aee_stop_nested_panic(struct pt_regs *regs)
{
	struct thread_info *thread = current_thread_info();
	int len = 0;
	int timeout = 1000000;
	int res = 0, cpu = 0;
	struct wd_api *wd_api = NULL;
	struct pt_regs *excp_regs = NULL;
	int prev_fiq_step = aee_rr_curr_fiq_step();
	/* everytime enter nested_panic flow, add 8 */
	static int step_base = -8;
	step_base = step_base < 48 ? step_base + 8 : 56;

	aee_rec_step_nested_panic(step_base);
	local_irq_disable();
	preempt_disable();

	aee_rec_step_nested_panic(step_base + 1);
	cpu = get_HW_cpuid();
	aee_rec_step_nested_panic(step_base + 2);
	/*nested panic may happens more than once on many/single cpus */
	if (atomic_read(&nested_panic_time) < 3)
		aee_nested_printf("\nCPU%dpanic%d@%d\n", cpu, nested_panic_time, prev_fiq_step);
	atomic_inc(&nested_panic_time);

	switch (atomic_read(&nested_panic_time)) {
	case 2:
		aee_print_regs(regs);
		aee_nested_printf("backtrace:");
		aee_print_bt(regs);
		break;

		/* must guarantee Only one cpu can run here */
		/* first check if thread valid */
	case 1:
		if (virt_addr_valid(thread) && virt_addr_valid(thread->regs_on_excp)) {
			excp_regs = thread->regs_on_excp;
		} else {
			/* if thread invalid, which means wrong sp or thread_info corrupted,
			   check global aee_excp_regs instead */
			aee_nested_printf("invalid thread [%x], excp_regs [%x]\n", thread,
					  aee_excp_regs);
			excp_regs = aee_excp_regs;
		}

		aee_nested_printf("Nested panic\n");
		if (excp_regs) {
			aee_nested_printf("Previous\n");
			aee_print_regs(excp_regs);
		}
		aee_nested_printf("Current\n");
		aee_print_regs(regs);

		/*should not print stack info. this may overwhelms ram console used by fiq */
		if (0 != in_fiq_handler()) {
			aee_nested_printf("in fiq hander\n");
		} else {
			/*Dump first panic stack */
			aee_nested_printf("Previous\n");
			if (excp_regs) {
				len = aee_nested_save_stack(excp_regs);
				aee_nested_printf("\nbacktrace:");
				aee_print_bt(excp_regs);
			}

			/*Dump second panic stack */
			aee_nested_printf("Current\n");
			if (virt_addr_valid(regs)) {
				len = aee_nested_save_stack(regs);
				aee_nested_printf("\nbacktrace:");
				aee_print_bt(regs);
			}
		}
		aee_rec_step_nested_panic(step_base + 5);
		ipanic_recursive_ke(regs, excp_regs, cpu);

		aee_rec_step_nested_panic(step_base + 6);
		/* we donot want a FIQ after this, so disable hwt */
		res = get_wd_api(&wd_api);
		if (res) {
			aee_nested_printf("get_wd_api error\n");
		} else {
			wd_api->wd_aee_confirm_hwreboot();
		}
		aee_rec_step_nested_panic(step_base + 7);
		break;
	default:
		break;
	}
	/* waiting for the WDT timeout */
	while (1) {
		/* output to UART directly to avoid printk nested panic */
		/* mt_fiq_printf("%s hang here%d\t", __func__, i++); */
		while (timeout--) {
			udelay(1);
		}
		timeout = 1000000;
	}
}
