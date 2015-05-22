#include <linux/slab.h>
#include <linux/aee.h>
#include <linux/atomic.h>
#include <linux/console.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/kthread.h>
#include <asm/io.h>


#define RC_CPU_COUNT NR_CPUS
#define RAM_CONSOLE_HEADER_STR_LEN 1024

static int mtk_cpu_num;

#ifdef MTK_EMMC_SUPPORT
#ifdef CONFIG_MTK_AEE_IPANIC
#include <linux/mmc/sd_misc.h>

extern int card_dump_func_write(unsigned char *buf, unsigned int len, unsigned long long offset,
				int dev);
extern int card_dump_func_read(unsigned char *buf, unsigned int len, unsigned long long offset,
			       int dev);
#define EMMC_BLOCK_SIZE 512
#endif
#endif

struct ram_console_buffer {
	uint32_t sig;
	uint32_t start;
	uint32_t size;

	uint8_t hw_status;
	uint8_t fiq_step;
	uint8_t reboot_mode;
	uint8_t __pad2;
	uint8_t __pad3;

	uint32_t bin_log_count;

	uint32_t last_irq_enter[RC_CPU_COUNT];
	uint64_t jiffies_last_irq_enter[RC_CPU_COUNT];

	uint32_t last_irq_exit[RC_CPU_COUNT];
	uint64_t jiffies_last_irq_exit[RC_CPU_COUNT];

	uint64_t jiffies_last_sched[RC_CPU_COUNT];
	char last_sched_comm[RC_CPU_COUNT][TASK_COMM_LEN];

	uint8_t hotplug_data1[RC_CPU_COUNT];
	uint8_t hotplug_data2;
	uint64_t hotplug_data3;

	uint32_t mcdi_wfi;

	void *kparams;

	uint8_t data[0];
};

#define RAM_CONSOLE_SIG (0x43474244)	/* DBGC */
static int FIQ_log_size = sizeof(struct ram_console_buffer);


static char *ram_console_old_log_init_buffer;

static struct ram_console_buffer ram_console_old_header;
static char *ram_console_old_log;
static size_t ram_console_old_log_size;

static struct ram_console_buffer *ram_console_buffer;
static size_t ram_console_buffer_size;

static DEFINE_SPINLOCK(ram_console_lock);

static atomic_t rc_in_fiq = ATOMIC_INIT(0);

#ifdef MTK_EMMC_SUPPORT
#ifdef CONFIG_MTK_AEE_IPANIC
#define EMMC_ADDR 0X700000
static char *ram_console2_log;

void last_kmsg_store_to_emmc(void)
{
	int buff_size;
	/* save log to emmc */
	buff_size = ram_console_buffer_size + sizeof(struct ram_console_buffer);
	buff_size = buff_size / EMMC_BLOCK_SIZE;
	buff_size *= EMMC_BLOCK_SIZE;
	card_dump_func_write((unsigned char *)ram_console_buffer, buff_size, EMMC_ADDR,
			     DUMP_INTO_BOOT_CARD_IPANIC);

	pr_err("ram_console: save kernel log to emmc!\n");
}

static int ram_console2_show(struct seq_file *m, void *v)
{
	struct ram_console_buffer *bufp = NULL;

	bufp = (struct ram_console_buffer *)ram_console2_log;
	/* seq_printf(m, ram_console2_log); */
	seq_printf(m, "show last_kmsg2 sig %d, size %d, hw_status %u, reboot_mode %u!\n",
		   bufp->sig, bufp->size, bufp->hw_status, bufp->reboot_mode);
	seq_printf(m, &bufp->data[0]);
	return 0;
}


static int ram_console2_file_open(struct inode *inode, struct file *file)
{
	return single_open(file, ram_console2_show, inode->i_private);
}

static const struct file_operations ram_console2_file_ops = {
	.owner = THIS_MODULE,
	.open = ram_console2_file_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

#ifdef MTK_GPT_SCHEME_SUPPORT
extern int get_emmc_dump_status(void);

#endif
static int emmc_read_last_kmsg(void)
{
	int size;

	struct proc_dir_entry *entry;
	struct ram_console_buffer *bufp = NULL;

#ifdef MTK_GPT_SCHEME_SUPPORT
	int emmc_ready_flag = 0;
	int count = 0;

	emmc_ready_flag = get_emmc_dump_status();
	while (emmc_ready_flag != 1) {
		msleep(2000);
		if (emmc_ready_flag == -1) {
			pr_err("emmc have no expd partition!\n");
			return 1;
		}
		pr_err("emmc expd not ready!\n");
		emmc_ready_flag = get_emmc_dump_status();
		count++;
		if (count > 100) {
			pr_err("emmc mount expd partition error!\n");
			return 1;
		}

	}
#endif

	size = ram_console_buffer_size + sizeof(struct ram_console_buffer);
	size = size / EMMC_BLOCK_SIZE;
	size *= EMMC_BLOCK_SIZE;

	ram_console2_log = kzalloc(size, GFP_KERNEL);
	if (ram_console2_log == NULL) {
		pr_err("ram_console: malloc size 2 error!\n");
		return 1;
	}

	if (card_dump_func_read(ram_console2_log, size, EMMC_ADDR, DUMP_INTO_BOOT_CARD_IPANIC) != 0) {
		kfree(ram_console2_log);
		ram_console2_log = NULL;
		pr_err("ram_console: read emmc data 2 error!\n");
		return 1;
	}

	bufp = (struct ram_console_buffer *)ram_console2_log;
	if (bufp->sig != RAM_CONSOLE_SIG) {
		kfree(ram_console2_log);
		ram_console2_log = NULL;
		pr_err("ram_console: emmc read data sig is not match!\n");
		return 1;
	}

	entry = proc_create("last_kmsg2", 0444, NULL, &ram_console2_file_ops);
	if (!entry) {
		pr_err("ram_console: failed to create proc entry\n");
		kfree(ram_console2_log);
		ram_console2_log = NULL;
		return 1;
	}
	pr_err("ram_console: create last_kmsg2 ok.\n");
	return 0;

}
#else
void last_kmsg_store_to_emmc(void)
{
}
#endif
#endif


void aee_rr_rec_reboot_mode(u8 mode)
{
	if (ram_console_buffer) {
		ram_console_buffer->reboot_mode = mode;
	}
}

extern void aee_rr_rec_kdump_params(void *params)
{
	if (ram_console_buffer) {
		ram_console_buffer->kparams = params;
	}
}

void aee_rr_rec_fiq_step(u8 i)
{
	if (ram_console_buffer) {
		ram_console_buffer->fiq_step = i;
	}
}

int aee_rr_curr_fiq_step(void)
{
	if (ram_console_buffer)
		return ram_console_buffer->fiq_step;
	else
		return 0;
}

void aee_rr_rec_last_irq_enter(int cpu, int irq, u64 j)
{
	if ((ram_console_buffer != NULL) && (cpu >= 0) && (cpu < RC_CPU_COUNT)) {
		ram_console_buffer->last_irq_enter[cpu] = irq;
		ram_console_buffer->jiffies_last_irq_enter[cpu] = j;
	}
	mb();
}

void aee_rr_rec_last_irq_exit(int cpu, int irq, u64 j)
{
	if ((ram_console_buffer != NULL) && (cpu >= 0) && (cpu < RC_CPU_COUNT)) {
		ram_console_buffer->last_irq_exit[cpu] = irq;
		ram_console_buffer->jiffies_last_irq_exit[cpu] = j;
	}
	mb();
}

void aee_rr_rec_last_sched_jiffies(int cpu, u64 j, const char *comm)
{
	if ((ram_console_buffer != NULL) && (cpu >= 0) && (cpu < RC_CPU_COUNT)) {
		ram_console_buffer->jiffies_last_sched[cpu] = j;
		strlcpy(ram_console_buffer->last_sched_comm[cpu], comm, TASK_COMM_LEN);
	}
	mb();
}

void aee_rr_rec_hoplug(int cpu, u8 data1, u8 data2)
{
	if ((ram_console_buffer != NULL) && (cpu >= 0) && (cpu < RC_CPU_COUNT)) {
		ram_console_buffer->hotplug_data1[cpu] = data1;
		if (cpu == 0)
			ram_console_buffer->hotplug_data2 = data2;
	}
}

void aee_rr_rec_hotplug(int cpu, u8 data1, u8 data2, unsigned long data3)
{
	if ((ram_console_buffer != NULL) && (cpu >= 0) && (cpu < RC_CPU_COUNT)) {
		ram_console_buffer->hotplug_data1[cpu] = data1;
		if (cpu == 0) {
			ram_console_buffer->hotplug_data2 = data2;
			ram_console_buffer->hotplug_data3 = (uint64_t)data3;
		}
	}
}

unsigned int *aee_rr_rec_mcdi_wfi(void)
{
	if (ram_console_buffer != NULL)
		return &ram_console_buffer->mcdi_wfi;
	else
		return NULL;
}

void sram_log_save(const char *msg, int count)
{
	struct ram_console_buffer *buffer;
	int rem;

	if (ram_console_buffer == NULL) {
		pr_err("ram console buffer is NULL!\n");
		return;
	}

	buffer = ram_console_buffer;

	/* count >= buffer_size, full the buffer */
	if (count >= ram_console_buffer_size) {
		memcpy(buffer->data, msg + (count - ram_console_buffer_size),
		       ram_console_buffer_size);
		buffer->start = 0;
		buffer->size = ram_console_buffer_size;
	} else if (count > (ram_console_buffer_size - buffer->start))	/* count > last buffer, full them and fill the head buffer */
	{
		rem = ram_console_buffer_size - buffer->start;
		memcpy(buffer->data + buffer->start, msg, rem);
		memcpy(buffer->data, msg + rem, count - rem);
		buffer->start = count - rem;
		buffer->size = ram_console_buffer_size;
	} else			/* count <=  last buffer, fill in free buffer */
	{
		memcpy(buffer->data + buffer->start, msg, count);	/* count <= last buffer, fill them */
		buffer->start += count;
		buffer->size += count;
		if (buffer->start >= ram_console_buffer_size) {
			buffer->start = 0;
		}
		if (buffer->size > ram_console_buffer_size) {
			buffer->size = ram_console_buffer_size;
		}
	}

}

void aee_sram_fiq_save_bin(const char *msg, size_t len)
{
	int delay = 100;
	char bin_buffer[4];
	struct ram_console_buffer *buffer = ram_console_buffer;

	if (FIQ_log_size + len > ram_console_buffer_size) {
		return;
	}

	if (len > 0xffff) {
		return;
	}

	if (len % 4 != 0) {
		len -= len % 4;
	}

	atomic_set(&rc_in_fiq, 1);

	while ((delay > 0) && (spin_is_locked(&ram_console_lock))) {
		udelay(1);
		delay--;
	}

	/* bin buffer flag 00ff */
	bin_buffer[0] = 0x00;
	bin_buffer[1] = 0xff;
	/* bin buffer size */
	bin_buffer[2] = len / 255;
	bin_buffer[3] = len % 255;

	sram_log_save(bin_buffer, 4);
	sram_log_save(msg, len);
	FIQ_log_size = FIQ_log_size + len + 4;
	buffer->bin_log_count += len;
}


void aee_disable_ram_console_write(void)
{
	atomic_set(&rc_in_fiq, 1);
	return;
}

void aee_sram_fiq_log(const char *msg)
{
	unsigned int count = strlen(msg);
	int delay = 100;

	if (FIQ_log_size + count > ram_console_buffer_size) {
		return;
	}

	atomic_set(&rc_in_fiq, 1);

	while ((delay > 0) && (spin_is_locked(&ram_console_lock))) {
		udelay(1);
		delay--;
	}

	sram_log_save(msg, count);
	FIQ_log_size += count;
}

void ram_console_write(struct console *console, const char *s, unsigned int count)
{
	unsigned long flags;

	if (atomic_read(&rc_in_fiq))
		return;

	spin_lock_irqsave(&ram_console_lock, flags);

	sram_log_save(s, count);

	spin_unlock_irqrestore(&ram_console_lock, flags);
}

static struct console ram_console = {
	.name = "ram",
	.write = ram_console_write,
	.flags = CON_PRINTBUFFER | CON_ENABLED | CON_ANYTIME,
	.index = -1,
};

void ram_console_enable_console(int enabled)
{
	if (enabled)
		ram_console.flags |= CON_ENABLED;
	else
		ram_console.flags &= ~CON_ENABLED;
}

static inline void bin_to_asc(char *buff, uint8_t num)
{
	if (num > 9) {
		*buff = num - 10 + 'a';
	} else {
		*buff = num + '0';
	}
	//pr_err("buff %c, num %d.\n", *buff, num);
}

static void __init ram_console_save_old(struct ram_console_buffer *buffer)
{
	size_t old_log_size = buffer->size;
	size_t total_size = old_log_size;
	size_t bin_log_size = 0;

	char *tmp;
	int i, n;
	int length;
	int point = 0;

	if (buffer->bin_log_count == 0) {
		ram_console_old_log_init_buffer = kmalloc(total_size, GFP_KERNEL);
		if (ram_console_old_log_init_buffer == NULL) {
			pr_err("ram_console: failed to allocate old buffer\n");
			return;
		}


		memcpy(&ram_console_old_header, buffer, sizeof(struct ram_console_buffer));

		ram_console_old_log = ram_console_old_log_init_buffer;
		ram_console_old_log_size = total_size;

		memcpy(ram_console_old_log_init_buffer,
		       &buffer->data[buffer->start], buffer->size - buffer->start);
		memcpy(ram_console_old_log_init_buffer + buffer->size - buffer->start,
		       &buffer->data[0], buffer->start);
	} else {
		bin_log_size = buffer->bin_log_count * 5 / 4;	/* bin: 12 34 56 78-->ascill: 78654321z */

		ram_console_old_log_init_buffer = kmalloc(total_size + bin_log_size, GFP_KERNEL);
		if (ram_console_old_log_init_buffer == NULL) {
			pr_err("ram_console: failed to allocate buffer\n");
			return;
		}

		tmp = kmalloc(total_size, GFP_KERNEL);
		if (tmp == NULL) {
			pr_err("ram_console: failed to allocate tmp buffer\n");
			return;
		}

		memcpy(&ram_console_old_header, buffer, sizeof(struct ram_console_buffer));

		ram_console_old_log = ram_console_old_log_init_buffer;
		memcpy(tmp, &buffer->data[buffer->start], buffer->size - buffer->start);
		memcpy(tmp + buffer->size - buffer->start, &buffer->data[0], buffer->start);

		for (i = 0; i < total_size;) {
			if ((tmp[i] == 0x00) && (tmp[i + 1] == 0xff)) {
				length = tmp[i + 2] * 0xff + tmp[i + 3];
				i = i + 4;
				for (n = 0; n < length / 4; n++) {
					bin_to_asc(&ram_console_old_log_init_buffer[point],
						   (uint8_t) (tmp[i + 3] / 16));
					point++;
					bin_to_asc(&ram_console_old_log_init_buffer[point],
						   (uint8_t) (tmp[i + 3] % 16));
					point++;
					bin_to_asc(&ram_console_old_log_init_buffer[point],
						   (uint8_t) (tmp[i + 2] / 16));
					point++;
					bin_to_asc(&ram_console_old_log_init_buffer[point],
						   (uint8_t) (tmp[i + 2] % 16));
					point++;
					bin_to_asc(&ram_console_old_log_init_buffer[point],
						   (uint8_t) (tmp[i + 1] / 16));
					point++;
					bin_to_asc(&ram_console_old_log_init_buffer[point],
						   (uint8_t) (tmp[i + 1] % 16));
					point++;
					bin_to_asc(&ram_console_old_log_init_buffer[point],
						   (uint8_t) (tmp[i] / 16));
					point++;
					bin_to_asc(&ram_console_old_log_init_buffer[point],
						   (uint8_t) (tmp[i] % 16));
					point++;
					ram_console_old_log_init_buffer[point++] = 32;
					i = i + 4;
				}
			} else {
				ram_console_old_log_init_buffer[point++] = tmp[i++];
			}
		}
		ram_console_old_log_size = point;
		kfree(tmp);
	}
}

static int __init ram_console_init(struct ram_console_buffer *buffer, size_t buffer_size)
{
	ram_console_buffer = buffer;
	ram_console_buffer_size = buffer_size - sizeof(struct ram_console_buffer);

	if (buffer->sig == RAM_CONSOLE_SIG) {
		if (buffer->size > ram_console_buffer_size || buffer->start > buffer->size)
			pr_err("ram_console: found existing invalid buffer, size %d, start %d\n",
			       buffer->size, buffer->start);
		else {
			pr_err("ram_console: found existing buffer,size %d, start %d\n",
			       buffer->size, buffer->start);
			ram_console_save_old(buffer);
		}
	} else {
		pr_err("ram_console: no valid data in buffer(sig = 0x%08x)\n", buffer->sig);
	}
	memset(buffer, 0, buffer_size);
	buffer->sig = RAM_CONSOLE_SIG;

	register_console(&ram_console);

	return 0;
}

#if defined(CONFIG_MTK_RAM_CONSOLE_USING_DRAM)
static void *remap_lowmem(phys_addr_t start, phys_addr_t size)
{
	struct page **pages;
	phys_addr_t page_start;
	unsigned int page_count;
	pgprot_t prot;
	unsigned int i;
	void *vaddr;

	page_start = start - offset_in_page(start);
	page_count = DIV_ROUND_UP(size + offset_in_page(start), PAGE_SIZE);

	prot = pgprot_noncached(PAGE_KERNEL);

	pages = kmalloc(sizeof(struct page *) * page_count, GFP_KERNEL);
	if (!pages) {
		pr_err("%s: Failed to allocate array for %u pages\n", __func__, page_count);
		return NULL;
	}

	for (i = 0; i < page_count; i++) {
		phys_addr_t addr = page_start + i * PAGE_SIZE;
		pages[i] = pfn_to_page(addr >> PAGE_SHIFT);
	}
	vaddr = vmap(pages, page_count, VM_MAP, prot);
	kfree(pages);
	if (!vaddr) {
		pr_err("%s: Failed to map %u pages\n", __func__, page_count);
		return NULL;
	}

	return vaddr + offset_in_page(start);
}
#endif

static int __init ram_console_early_init(void)
{
	struct ram_console_buffer *bufp = NULL;
	size_t buffer_size = 0;

#if defined(CONFIG_MTK_RAM_CONSOLE_USING_SRAM)
	bufp = (struct ram_console_buffer *)CONFIG_MTK_RAM_CONSOLE_ADDR;
	buffer_size = CONFIG_MTK_RAM_CONSOLE_SIZE;
#elif defined(CONFIG_MTK_RAM_CONSOLE_USING_DRAM)

	bufp = remap_lowmem(CONFIG_MTK_RAM_CONSOLE_DRAM_ADDR, CONFIG_MTK_RAM_CONSOLE_DRAM_SIZE);
	if (bufp == NULL) {
		pr_err("ioremap failed, no ram console available\n");
		return 0;
	}
	buffer_size = CONFIG_MTK_RAM_CONSOLE_DRAM_SIZE;
#else
	return 0;
#endif

	pr_err("%s: start: 0x%p, size: %d\n", __func__, bufp, buffer_size);
	mtk_cpu_num = num_present_cpus();
	return ram_console_init(bufp, buffer_size);
}

static int ram_console_show(struct seq_file *m, void *v)
{
	seq_write(m, ram_console_old_log, ram_console_old_log_size);
	return 0;
}

static int ram_console_file_open(struct inode *inode, struct file *file)
{
	return single_open(file, ram_console_show, inode->i_private);
}

static const struct file_operations ram_console_file_ops = {
	.owner = THIS_MODULE,
	.open = ram_console_file_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init ram_console_late_init(void)
{
	struct proc_dir_entry *entry;
	struct last_reboot_reason lrr;
	char *ram_console_header_buffer;
	int str_real_len = 0;
	int i = 0;

#ifdef MTK_EMMC_SUPPORT
#ifdef CONFIG_MTK_AEE_IPANIC
#ifdef MTK_GPT_SCHEME_SUPPORT
	int err;
	static struct task_struct *thread;
	thread = kthread_run(emmc_read_last_kmsg, 0, "read_poweroff_log");
	if (IS_ERR(thread)) {
		err = PTR_ERR(thread);
		pr_err("failed to create kernel thread: %d\n", err);
	}
#else
	emmc_read_last_kmsg();
#endif
#endif
#endif
	if (ram_console_old_log == NULL) {
		pr_err("ram console old log is null!\n");
		return 0;
	}

	memset(&lrr, 0, sizeof(struct last_reboot_reason));
	lrr.wdt_status = ram_console_old_header.hw_status;
	lrr.fiq_step = ram_console_old_header.fiq_step;
	lrr.reboot_mode = ram_console_old_header.reboot_mode;

	for (i = 0; i < NR_CPUS; i++) {
		lrr.last_irq_enter[i] = ram_console_old_header.last_irq_enter[i];
		lrr.jiffies_last_irq_enter[i] = ram_console_old_header.jiffies_last_irq_enter[i];

		lrr.last_irq_exit[i] = ram_console_old_header.last_irq_exit[i];
		lrr.jiffies_last_irq_exit[i] = ram_console_old_header.jiffies_last_irq_exit[i];

		lrr.jiffies_last_sched[i] = ram_console_old_header.jiffies_last_sched[i];
		strlcpy(lrr.last_sched_comm[i], ram_console_old_header.last_sched_comm[i],
			TASK_COMM_LEN);

		lrr.hotplug_data1[i] = ram_console_old_header.hotplug_data1[i];
		lrr.hotplug_data2[i] = i ? 0 : ram_console_old_header.hotplug_data2;
		lrr.hotplug_data3[i] = i ? 0 : ram_console_old_header.hotplug_data3;
	}

	lrr.mcdi_wfi = ram_console_old_header.mcdi_wfi;

	aee_rr_last(&lrr);

	ram_console_header_buffer = kmalloc(RAM_CONSOLE_HEADER_STR_LEN, GFP_KERNEL);
	if (ram_console_header_buffer == NULL) {
		pr_err("ram_console: failed to allocate buffer for header buffer.\n");
		return 0;
	}


	str_real_len =
	    sprintf(ram_console_header_buffer, "ram console header, hw_status: %u, fiq step %u.\n",
		    ram_console_old_header.hw_status, ram_console_old_header.fiq_step);

	str_real_len +=
	    sprintf(ram_console_header_buffer + str_real_len, "bin log %d.\n",
		    ram_console_old_header.bin_log_count);

	ram_console_old_log = kmalloc(ram_console_old_log_size + str_real_len, GFP_KERNEL);
	if (ram_console_old_log == NULL) {
		pr_err("ram_console: failed to allocate buffer for old log\n");
		ram_console_old_log_size = 0;
		kfree(ram_console_header_buffer);
		return 0;
	}
	memcpy(ram_console_old_log, ram_console_header_buffer, str_real_len);
	memcpy(ram_console_old_log + str_real_len,
	       ram_console_old_log_init_buffer, ram_console_old_log_size);

	kfree(ram_console_header_buffer);
	kfree(ram_console_old_log_init_buffer);
	entry = proc_create("last_kmsg", 0444, NULL, &ram_console_file_ops);
	if (!entry) {
		pr_err("ram_console: failed to create proc entry\n");
		kfree(ram_console_old_log);
		ram_console_old_log = NULL;
		return 0;
	}

	ram_console_old_log_size += str_real_len;
	return 0;
}
console_initcall(ram_console_early_init);
late_initcall(ram_console_late_init);
