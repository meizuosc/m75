#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/kallsyms.h>
#include <linux/utsname.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <asm/uaccess.h>

#include <linux/timer.h>
#include <linux/jiffies.h>

#define SEQ_printf(m, x...)	    \
 do {			    \
    if (m)		    \
	seq_printf(m, x);	\
    else		    \
	printk(x);	    \
 } while (0)

#define X_ms 200
#define Y_steps (2000/X_ms)
#define BW_THRESHOLD 1300
static void enable_fliper(void);
static void disable_fliper(void);
extern unsigned int get_ddr_type(void)__attribute__((weak));

/* define supported DRAM types */
enum
{
  LPDDR2 = 0,
  DDR3_16,
  DDR3_32,
  LPDDR3,
  mDDR,
};
static int fliper_debug = 0;
static ssize_t mt_fliper_write(struct file *filp, const char *ubuf,
	   size_t cnt, loff_t *data)
{
    char buf[64];
    int val;
    int ret;
    if (cnt >= sizeof(buf))
        return -EINVAL;

    if (copy_from_user(&buf, ubuf, cnt))
        return -EFAULT;

    buf[cnt] = 0;

    ret = strict_strtoul(buf, 10, (unsigned long*)&val);
    if (ret < 0)
        return ret;
    if(val == 1){
        enable_fliper();
    }else if(val == 0){
        disable_fliper();
    }else if(val == 3){
        fliper_debug = 1;
    }
    printk(" fliper option: %d\n", val);
    return cnt;

}

static int mt_fliper_show(struct seq_file *m, void *v)
{
    SEQ_printf(m, "----------------------------------------\n");
    return 0;
}
/*** Seq operation of mtprof ****/
static int mt_fliper_open(struct inode *inode, struct file *file) 
{ 
    return single_open(file, mt_fliper_show, inode->i_private); 
} 

static const struct file_operations mt_fliper_fops = { 
    .open = mt_fliper_open, 
    .write = mt_fliper_write,
    .read = seq_read, 
    .llseek = seq_lseek, 
    .release = single_release, 
};
/******* POWER PERF TRANSFORMER *********/
#include <asm/div64.h>
//Cache info
#ifdef CACHE_REFILL_INFO
extern unsigned int get_cache_refill(unsigned int cpu);
extern unsigned int get_cache_access(unsigned int cpu);
extern void fliper_pmu_reset(void);
#endif
#include <mach/mt_cpufreq.h>

static void mt_power_pef_transfer(void);
static DEFINE_TIMER(mt_pp_transfer_timer, (void *)mt_power_pef_transfer, 0, 0);
static int pp_index;

static void mt_power_pef_transfer_work(void);
static DECLARE_WORK(mt_pp_work,(void *) mt_power_pef_transfer_work);

//EMI
extern unsigned long long get_mem_bw(void);

static void mt_power_pef_transfer_work()
{
    unsigned long long emi_bw;
    int perf_mode = 0;

#ifdef CACHE_REFILL_INFO
    int cpu;
    int cpu_num=0;
    unsigned long long refill, access, per, t_per, t_refill;
    int high_count = 0; int low_count = 0;
    t_refill = 0;
    t_per = 0;
    for_each_online_cpu(cpu){
        cpu_num++;
        refill = (unsigned long long)get_cache_refill(cpu);
        access = (unsigned long long)get_cache_access(cpu);
        if(refill!=0){
            per = refill*100;
            do_div(per, access);
            t_per += per;
        }
        t_refill += refill;

        per > 10 ? high_count++:low_count++;
        //printk(KERN_EMERG"CPU %2d: %3llu%% %10llu/%10llu\n", cpu, per, refill, access);
       // fliper_pmu_reset();
    }
    if(t_per!=0)
        do_div(t_per, cpu_num);
#endif
    
    /*  Get EMI*/ 
    emi_bw = get_mem_bw();
    if(emi_bw > BW_THRESHOLD)
        perf_mode = 1;

    if(perf_mode == 1){
        if(pp_index == 0){
            mt_soc_dvfs(SOC_DVFS_TYPE_FLIPER, 1);
            printk("\n<<SOC DVFS FLIPER>> flip to S, %llu\n", emi_bw); 
        }
        pp_index = 1 << Y_steps;
    }else{
        if(pp_index == 1){
            mt_soc_dvfs(SOC_DVFS_TYPE_FLIPER, 0);
            printk("\n<<SOC DVFS FLIPER>> flip to E, %llu\n", emi_bw); 
        }
        pp_index = pp_index >> 1;
    }

#ifdef CACHE_REFILL_INFO
    //printk(KERN_EMERG"Rate %10llu%%, %4d\n", t_per, pp_index);
    if(t_refill!=0)
        do_div(t_refill, cpu_num);
    //printk(KERN_EMERG"Miss %10llu%%, %4d, %llu\n", t_refill, pp_index, t_per);
#endif
    if(fliper_debug == 1)
        printk(KERN_EMERG"EMI:Rate:count:mode %6llu:%4d\n", emi_bw, pp_index); 

    //printk(KERN_EMERG"======\n");

}
static void enable_fliper()
{
    printk("fliper enable +++\n");
    mod_timer(&mt_pp_transfer_timer, jiffies + msecs_to_jiffies(X_ms));
}
static void disable_fliper()
{
    printk("fliper disable ---\n");
    del_timer(&mt_pp_transfer_timer);
}
static void mt_power_pef_transfer()
{
    mod_timer(&mt_pp_transfer_timer, jiffies + msecs_to_jiffies(X_ms));
    schedule_work(&mt_pp_work);
}
#define TIME_5SEC_IN_MS 5000
static int __init init_fliper(void)
{
    struct proc_dir_entry *pe;
    //int DRAM_Type=get_ddr_type();

    pe = proc_create("fliper", 0644, NULL, &mt_fliper_fops);
    if (!pe)
        return -ENOMEM;
    printk("prepare mt pp transfer: jiffies:%lu-->%lu\n",jiffies, jiffies + msecs_to_jiffies(TIME_5SEC_IN_MS));
    printk("-  next jiffies:%lu >>> %lu\n",jiffies, jiffies + msecs_to_jiffies(X_ms));
    mod_timer(&mt_pp_transfer_timer, jiffies + msecs_to_jiffies(TIME_5SEC_IN_MS));
    return 0;
}
__initcall(init_fliper);
