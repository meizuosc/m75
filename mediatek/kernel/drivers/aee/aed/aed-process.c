#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/ptrace.h>
#include <linux/ratelimit.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/stacktrace.h>
#include <linux/linkage.h>
#include <linux/atomic.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <asm/memory.h>
#include <asm/stacktrace.h>
#include <asm/traps.h>
#include <linux/semaphore.h>
#include <linux/delay.h>

#include "aed.h"

struct bt_sync {
	atomic_t cpus_report;
	atomic_t cpus_lock;
};

static void per_cpu_get_bt(void *info)
{
	int timeout_max = 500000;
	struct bt_sync *s = (struct bt_sync *)info;

	if (atomic_read(&s->cpus_lock) == 0)
		return;

	atomic_dec(&s->cpus_report);
	while (atomic_read(&s->cpus_lock) == 1) {
		if (timeout_max-- > 0) {
			udelay(1);
		} else {
			break;
		}
	}
	atomic_dec(&s->cpus_report);
}

int notrace aed_unwind_frame(struct stackframe *frame, unsigned long stack_address)
{
	unsigned long high, low;
	unsigned long fp = frame->fp;

	unsigned long thread_info = stack_address - THREAD_SIZE;

	/* only go to a higher address on the stack */
	low = frame->sp;
	high = ALIGN(low, THREAD_SIZE);
	if (high != stack_address) {
		LOGD("%s: sp base(%lx) not equal to process stack base(%lx)\n", __func__, low,
		     stack_address);
		return -EINVAL;
	}
	/* check current frame pointer is within bounds */
	if ((fp < (low + 12)) || ((fp + 4) >= high))
		return -EINVAL;

	if ((fp < thread_info) || (fp >= (stack_address - 4))) {
		LOGD("%s: fp(%lx) out of process stack base(%lx)\n", __func__, fp, stack_address);
		return -EINVAL;
	}

	/* restore the registers from the stack frame */
	frame->fp = *(unsigned long *)(fp - 12);
	frame->sp = *(unsigned long *)(fp - 8);
	frame->lr = *(unsigned long *)(fp - 4);
	frame->pc = *(unsigned long *)(fp);

	return 0;
}

#define FUNCTION_OFFSET 12
asmlinkage void __sched preempt_schedule_irq(void);
static struct aee_bt_frame aed_backtrace_buffer[AEE_NR_FRAME];

static int aed_walk_stackframe(struct stackframe *frame, struct aee_process_bt *bt,
			       unsigned int stack_address)
{
	int count;
	struct stackframe current_stk;
	bt->entries = aed_backtrace_buffer;

	memcpy(&current_stk, frame, sizeof(struct stackframe));
	for (count = 0; count < AEE_NR_FRAME; count++) {
		unsigned long prev_fp = current_stk.fp;
		int ret;

		bt->entries[bt->nr_entries].pc = current_stk.pc;
		bt->entries[bt->nr_entries].lr = current_stk.lr;
		snprintf(bt->entries[bt->nr_entries].pc_symbol, AEE_SZ_SYMBOL_S, "%pS",
			 (void *)current_stk.pc);
		snprintf(bt->entries[bt->nr_entries].lr_symbol, AEE_SZ_SYMBOL_L, "%pS",
			 (void *)current_stk.lr);

		bt->nr_entries++;
		if (bt->nr_entries >= AEE_NR_FRAME) {
			break;
		}

		ret = aed_unwind_frame(&current_stk, stack_address);
		/* oops, reached end without exception. return original info */
		if (ret < 0)
			break;

		if (in_exception_text(current_stk.pc)
		    || ((current_stk.pc - FUNCTION_OFFSET) ==
			(unsigned long)preempt_schedule_irq)) {
			struct pt_regs *regs = (struct pt_regs *)(prev_fp + 4);

			/* passed exception point, return this if unwinding is sucessful */
			current_stk.pc = regs->ARM_pc;
			current_stk.lr = regs->ARM_lr;
			current_stk.fp = regs->ARM_fp;
			current_stk.sp = regs->ARM_sp;
		}

	}

	if (bt->nr_entries < AEE_NR_FRAME) {
		bt->entries[bt->nr_entries].pc = ULONG_MAX;
		bt->entries[bt->nr_entries].pc_symbol[0] = '\0';
		bt->entries[bt->nr_entries].lr = ULONG_MAX;
		bt->entries[bt->nr_entries++].lr_symbol[0] = '\0';
	}
	return 0;
}

static void aed_get_bt(struct task_struct *tsk, struct aee_process_bt *bt)
{
	struct stackframe frame;
	unsigned int stack_address;

	bt->nr_entries = 0;

	memset(&frame, 0, sizeof(struct stackframe));
	if (tsk != current) {
		frame.fp = thread_saved_fp(tsk);
		frame.sp = thread_saved_sp(tsk);
		frame.lr = thread_saved_pc(tsk);
		frame.pc = 0xffffffff;
	} else {
		register unsigned long current_sp asm("sp");

		frame.fp = (unsigned long)__builtin_frame_address(0);
		frame.sp = current_sp;
		frame.lr = (unsigned long)__builtin_return_address(0);
		frame.pc = (unsigned long)aed_get_bt;
	}
	stack_address = ALIGN(frame.sp, THREAD_SIZE);
	if ((stack_address >= (PAGE_OFFSET + THREAD_SIZE)) && virt_addr_valid(stack_address)) {
		aed_walk_stackframe(&frame, bt, stack_address);
	} else {
		LOGD("%s: Invalid sp value %lx\n", __func__, frame.sp);
	}
}

static DEFINE_SEMAPHORE(process_bt_sem);

int aed_get_process_bt(struct aee_process_bt *bt)
{
	int nr_cpus, err;
	struct bt_sync s;
	struct task_struct *task;
	int timeout_max = 500000;

	if (down_interruptible(&process_bt_sem) < 0) {
		return -ERESTARTSYS;
	}

	err = 0;
	if (bt->pid > 0) {
		task = find_task_by_vpid(bt->pid);
		if (task == NULL) {
			err = -EINVAL;
			goto exit;
		}
	} else {
		err = -EINVAL;
		goto exit;
	}

	err = mutex_lock_killable(&task->signal->cred_guard_mutex);
	if (err)
		goto exit;
	if (!ptrace_may_access(task, PTRACE_MODE_ATTACH)) {
		mutex_unlock(&task->signal->cred_guard_mutex);
		err = -EPERM;
		goto exit;
	}

	get_online_cpus();
	preempt_disable();

	nr_cpus = num_online_cpus();
	atomic_set(&s.cpus_report, nr_cpus - 1);
	atomic_set(&s.cpus_lock, 1);

	smp_call_function(per_cpu_get_bt, &s, 0);

	while (atomic_read(&s.cpus_report) != 0) {
		if (timeout_max-- > 0) {
			udelay(1);
		} else {
			break;
		}
	}

	aed_get_bt(task, bt);

	atomic_set(&s.cpus_report, nr_cpus - 1);
	atomic_set(&s.cpus_lock, 0);
	timeout_max = 500000;
	while (atomic_read(&s.cpus_report) != 0) {
		if (timeout_max-- > 0) {
			udelay(1);
		} else {
			break;
		}
	}

	preempt_enable();
	put_online_cpus();

	mutex_unlock(&task->signal->cred_guard_mutex);

 exit:
	up(&process_bt_sem);
	return err;

}
