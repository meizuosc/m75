#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/xlog.h>
#include <linux/types.h>
#include <linux/kobject.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <asm/uaccess.h>
#include <linux/err.h>
#include <linux/syscalls.h>
#include <asm/system.h>

#include "mach/mtk_thermal_monitor.h"
#include <mach/system.h>


// Extern two API functions from battery driver to limit max charging current. 
#if 1
/**
 *  return value means charging current in mA
 *  -1 means error
 *  Implementation in mt_battery.c and mt_battery_fan5405.c
 */
extern int get_bat_charging_current_level(void);

/**
 *  current_limit means limit of charging current in mA
 *  -1 means no limit
 *  Implementation in mt_battery.c and mt_battery_fan5405.c
 */
extern int set_bat_charging_current_limit(int current_limit);
#endif

#define mtk_cooler_bcct_dprintk_always(fmt, args...) \
  do { xlog_printk(ANDROID_LOG_INFO, "thermal/cooler/bcct", fmt, ##args); } while(0)

#define mtk_cooler_bcct_dprintk(fmt, args...) \
  do { \
    if (1 == cl_bcct_klog_on) { \
      xlog_printk(ANDROID_LOG_INFO, "thermal/cooler/bcct", fmt, ##args); \
    } \
  } while(0)

#define MAX_NUM_INSTANCE_MTK_COOLER_BCCT  4

#define MTK_CL_BCCT_GET_LIMIT(limit, state) \
    do { (limit) = (short) (((unsigned long) (state))>>16); } while(0)

#define MTK_CL_BCCT_SET_LIMIT(limit, state) \
    do { state = ((((unsigned long) (state))&0xFFFF) | ((short) limit<<16)); } while(0)

#define MTK_CL_BCCT_GET_CURR_STATE(curr_state, state) \
    do { curr_state = (((unsigned long) (state))&0xFFFF); } while(0)

#define MTK_CL_BCCT_SET_CURR_STATE(curr_state, state) \
    do { if (0 == curr_state) \
           state &= ~0x1; \
         else \
           state |= 0x1; \
    } while(0)

static int cl_bcct_klog_on = 0;
static struct thermal_cooling_device *cl_bcct_dev[MAX_NUM_INSTANCE_MTK_COOLER_BCCT] = {0};
static unsigned long cl_bcct_state[MAX_NUM_INSTANCE_MTK_COOLER_BCCT] = {0};
static int cl_bcct_cur_limit = 65535;

#include "linux/../../drivers/thermal/thermal_core.h"
static void 
mtk_cl_bcct_set_bcct_limit(struct thermal_cooling_device *cdev,
        unsigned long state)
{
	/*
	 * the trip numbers of mtktsbattery is 5,but the index of bcct begin with "1",and end with "4"
	 * Temperature
	 * <= 0, change current == 0
	 * 0 < t <=10, charge current == 900mA
	 * 10 < t <=45, charge current == 1500mA
	 * > 45, charge current == 0
	 *
	 * limit == 1,     current == 0mA
	 * limit == 1599,  current == 1500mA
	 * limit == 999,   current == 900mA
	 */
	static int trip = 4, trip_temp = -40, limit = 1;
	int trip_now = 0, curr_state = 0;
	long trip_temp_now = 0;
	struct thermal_instance *instance = NULL;

	mutex_lock(&cdev->lock);
	list_for_each_entry(instance, &cdev->thermal_instances, cdev_node) {
		if (instance->target == THERMAL_NO_TARGET)
			continue;
		trip_now = instance->trip;

		//mtk_cooler_bcct_dprintk_always("BCCT instance->trip:%d,target:%lu,tz_type:%s,line:%d\n",instance->trip,instance->target,instance->tz->type,__LINE__);
		break;
	}
	mutex_unlock(&cdev->lock);
	if( instance==NULL || instance->tz==NULL ) {
		printk("%s error line:%d\n",__func__,__LINE__);
		return;
	}
	if( instance->trip<0 || instance->trip>5 ) {
		printk("%s error line:%d\n",__func__,__LINE__);
		return;
	}
	if( instance->tz->ops==NULL || instance->tz->ops->get_trip_temp==NULL ) {
		printk("%s error line:%d\n",__func__,__LINE__);
		return;
	}
	instance->tz->ops->get_trip_temp(instance->tz, instance->trip, &trip_temp_now);

	mtk_cooler_bcct_dprintk_always("BCCT trip:%d,get_temp:%d,trip_temp_now:%ld,trip_temp:%d,state:%d,cl_type:%s\n",instance->trip,instance->tz->temperature,trip_temp_now,trip_temp,state,cdev->type);

	MTK_CL_BCCT_GET_CURR_STATE(curr_state, cl_bcct_state[trip_now-1]);

	if( 1 == curr_state ) {
		if( trip_temp_now>trip_temp ) {
			trip = trip_now;
			trip_temp = trip_temp_now;
			MTK_CL_BCCT_GET_LIMIT(limit, cl_bcct_state[trip_now-1]);

			mtk_cooler_bcct_dprintk_always("BCCT trip_now:%d,limit:%d,cur_limit:%d\n",trip_now,limit,cl_bcct_cur_limit);
			if( limit!=cl_bcct_cur_limit ) {
				cl_bcct_cur_limit = limit;
				set_bat_charging_current_limit(cl_bcct_cur_limit);
				mtk_cooler_bcct_dprintk_always("mtk_cl_bcct_set_bcct_limit() set limit=%d\n", cl_bcct_cur_limit);
			} else {
				mtk_cooler_bcct_dprintk_always("BCCT limit is the same as before\n");
			}
		} else {
			mtk_cooler_bcct_dprintk_always("BCCT have set higher temp\n");
		}
	} else {
		  cl_bcct_cur_limit = 599;
	      set_bat_charging_current_limit(-1);// == -1,current == 500mA
	      mtk_cooler_bcct_dprintk_always("mtk_cl_bcct_set_bcct_limit() limit=-1, state==0\n");
	}
    mtk_cooler_bcct_dprintk_always("mtk_cl_bcct_set_bcct_limit() real limit=%d\n", get_bat_charging_current_level()/100);
    mtk_cooler_bcct_dprintk_always("\n");
	if( trip_now==instance->tz->trips-1 ) {
		trip = 4;
		trip_temp = -40;
		limit = 1;
	}

	/*
	 * cant work with mtk code,so delete it
	 */
#if 0
  // TODO: optimize
  int i = 0;
  int min_limit = 65535;
  for (; i < MAX_NUM_INSTANCE_MTK_COOLER_BCCT; i++)
  {
    unsigned long curr_state;

    MTK_CL_BCCT_GET_CURR_STATE(curr_state, cl_bcct_state[i]);
    if (1 == curr_state)
    {
      int limit;
      MTK_CL_BCCT_GET_LIMIT(limit, cl_bcct_state[i]);
      if ((min_limit > limit) && (limit > 0))
        min_limit = limit;
      mtk_cooler_bcct_dprintk("%s limit:%d\n",__func__,limit);
    }
    mtk_cooler_bcct_dprintk("%s i:%d,curr_state:%d,min_limit:%d,cur_limit:%d\n",__func__,i,curr_state,min_limit,cl_bcct_cur_limit);
  }

  if (min_limit != cl_bcct_cur_limit)
  {
    cl_bcct_cur_limit = min_limit;
#if 1
    if (65535 <= cl_bcct_cur_limit)
    {
      set_bat_charging_current_limit(-1);
      mtk_cooler_bcct_dprintk_always("mtk_cl_bcct_set_bcct_limit() limit=-1\n");
    }
    else
    {
      set_bat_charging_current_limit(cl_bcct_cur_limit);
      mtk_cooler_bcct_dprintk_always("mtk_cl_bcct_set_bcct_limit() limit=%d\n", cl_bcct_cur_limit);
    }
    
    mtk_cooler_bcct_dprintk_always("mtk_cl_bcct_set_bcct_limit() real limit=%d\n", get_bat_charging_current_level()/100);
#endif
  }
#endif
}

static int 
mtk_cl_bcct_get_max_state(struct thermal_cooling_device *cdev,
                          unsigned long *state)
{        
  *state = 1;
  mtk_cooler_bcct_dprintk("mtk_cl_bcct_get_max_state() %s %d\n", cdev->type, *state);
  return 0;
}

static int 
mtk_cl_bcct_get_cur_state(struct thermal_cooling_device *cdev,
                          unsigned long *state)
{
  MTK_CL_BCCT_GET_CURR_STATE(*state, *((unsigned long*) cdev->devdata));
  mtk_cooler_bcct_dprintk("mtk_cl_bcct_get_cur_state() %s %d\n", cdev->type, *state);
  mtk_cooler_bcct_dprintk("mtk_cl_bcct_get_cur_state() %s limit=%d\n", cdev->type, get_bat_charging_current_level()/100);
  return 0;
}

static int 
mtk_cl_bcct_set_cur_state(struct thermal_cooling_device *cdev,
                          unsigned long state)
{
  mtk_cooler_bcct_dprintk("mtk_cl_bcct_set_cur_state() %s %d\n", cdev->type, state);
  MTK_CL_BCCT_SET_CURR_STATE(state, *((unsigned long*) cdev->devdata));
  mtk_cl_bcct_set_bcct_limit(cdev, state);
  mtk_cooler_bcct_dprintk("mtk_cl_bcct_set_cur_state() %s limit=%d\n", cdev->type, get_bat_charging_current_level()/100);
  
  return 0;
}

/* bind fan callbacks to fan device */
static struct thermal_cooling_device_ops mtk_cl_bcct_ops = {
  .get_max_state = mtk_cl_bcct_get_max_state,
  .get_cur_state = mtk_cl_bcct_get_cur_state,
  .set_cur_state = mtk_cl_bcct_set_cur_state,
};

static int mtk_cooler_bcct_register_ltf(void)
{
  int i;
  mtk_cooler_bcct_dprintk("register ltf\n");

  for (i = MAX_NUM_INSTANCE_MTK_COOLER_BCCT; i-- > 0; )
  {
    char temp[20] = {0};
    sprintf(temp, "mtk-cl-bcct%02d", i);
    cl_bcct_dev[i] = mtk_thermal_cooling_device_register(temp, 
                                                         (void*) &cl_bcct_state[i], // put bcct state to cooler devdata
                                                         &mtk_cl_bcct_ops);
  }

  return 0;
}

static void mtk_cooler_bcct_unregister_ltf(void)
{
  int i;
  mtk_cooler_bcct_dprintk("unregister ltf\n");

  for (i = MAX_NUM_INSTANCE_MTK_COOLER_BCCT; i-- > 0; )
  {
    if (cl_bcct_dev[i])
    {
      mtk_thermal_cooling_device_unregister(cl_bcct_dev[i]);
      cl_bcct_dev[i] = NULL;
      cl_bcct_state[i] = 0;
    }
  }
}

#if 0
static int _mtk_cl_bcct_proc_read(char *buf, char **start, off_t off, int count, int *eof, void *data)
{
    int len = 0;
    char *p = buf;

    mtk_cooler_bcct_dprintk("[_mtk_cl_bcct_proc_read] invoked.\n");

    /**
     * The format to print out: 
     *  kernel_log <0 or 1>
     *  <mtk-cl-bcct<ID>> <bcc limit>
     *  ..
     */
    if (NULL == data)
    {
        mtk_cooler_bcct_dprintk("[_mtk_cl_bcct_proc_read] null data\n");
    }
    else
    {
        int i = 0;

        p += sprintf(p, "klog %d\n", cl_bcct_klog_on);
        p += sprintf(p, "curr_limit %d\n", cl_bcct_cur_limit);

        for (; i < MAX_NUM_INSTANCE_MTK_COOLER_BCCT; i++)
        {
            int limit;
            unsigned int curr_state;
            
            MTK_CL_BCCT_GET_LIMIT(limit, cl_bcct_state[i]);
            MTK_CL_BCCT_GET_CURR_STATE(curr_state, cl_bcct_state[i]);
        
            p += sprintf(p, "mtk-cl-bcct%02d %d mA, state %d\n", i, limit, curr_state);
        }
    }

    *start = buf + off;

    len = p - buf;
    if (len > off)
        len -= off;
    else
        len = 0;

    return len < count ? len  : count;
}

static ssize_t _mtk_cl_bcct_proc_write(struct file *file, const char *buffer, unsigned long count, void *data)
{
    int len = 0;
    char desc[128];
    int klog_on, limit0, limit1, limit2;

    len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
    if (copy_from_user(desc, buffer, len))
    {
        return 0;
    }
    desc[len] = '\0';

    /**
     * sscanf format <klog_on> <mtk-cl-bcct00 limit> <mtk-cl-bcct01 limit> ...
     * <klog_on> can only be 0 or 1
     * <mtk-cl-bcct00 limit> can only be positive integer or -1 to denote no limit
     */

    if (NULL == data)
    {
        mtk_cooler_bcct_dprintk("[_mtk_cl_bcct_proc_write] null data\n");
        return -EINVAL;
    }

    // WARNING: Modify here if MTK_THERMAL_MONITOR_COOLER_MAX_EXTRA_CONDITIONS is changed to other than 3
#if (3 == MAX_NUM_INSTANCE_MTK_COOLER_BCCT)
    MTK_CL_BCCT_SET_LIMIT(-1, cl_bcct_state[0]);
    MTK_CL_BCCT_SET_LIMIT(-1, cl_bcct_state[1]);
    MTK_CL_BCCT_SET_LIMIT(-1, cl_bcct_state[2]);

    if (1 <= sscanf(desc, "%d %d %d %d",
                    &klog_on, &limit0, &limit1, &limit2))
    {
        if (klog_on == 0 || klog_on == 1)
        {
          cl_bcct_klog_on = klog_on;
        }

        if (limit0 >= -1) MTK_CL_BCCT_SET_LIMIT(limit0, cl_bcct_state[0]);
        if (limit1 >= -1) MTK_CL_BCCT_SET_LIMIT(limit1, cl_bcct_state[1]);
        if (limit2 >= -1) MTK_CL_BCCT_SET_LIMIT(limit2, cl_bcct_state[2]);
        
        return count;
    }
    else
#else
#error "Change correspondent part when changing MAX_NUM_INSTANCE_MTK_COOLER_BCCT!"
#endif
    {
        mtk_cooler_bcct_dprintk("[_mtk_cl_bcct_proc_write] bad argument\n");
    }

    return -EINVAL;
}
#endif

static ssize_t _cl_bcct_write(struct file *filp, const char __user *buf, size_t len, loff_t *data)
{
    //int ret = 0;
	char tmp[128] = {0};
	int klog_on, limit0, limit1, limit2,limit3;

	/* write data to the buffer */
	if ( copy_from_user(tmp, buf, len) ) {
		return -EFAULT;
	}

    /**
     * sscanf format <klog_on> <mtk-cl-bcct00 limit> <mtk-cl-bcct01 limit> ...
     * <klog_on> can only be 0 or 1
     * <mtk-cl-bcct00 limit> can only be positive integer or -1 to denote no limit
     */

    if (NULL == data)
    {
        mtk_cooler_bcct_dprintk("[_mtk_cl_bcct_proc_write] null data\n");
        return -EINVAL;
    }

    // WARNING: Modify here if MTK_THERMAL_MONITOR_COOLER_MAX_EXTRA_CONDITIONS is changed to other than 3
#if (4 == MAX_NUM_INSTANCE_MTK_COOLER_BCCT)
    MTK_CL_BCCT_SET_LIMIT(-1, cl_bcct_state[0]);
    MTK_CL_BCCT_SET_LIMIT(-1, cl_bcct_state[1]);
    MTK_CL_BCCT_SET_LIMIT(-1, cl_bcct_state[2]);
    MTK_CL_BCCT_SET_LIMIT(-1, cl_bcct_state[3]);

    if (1 <= sscanf(tmp, "%d %d %d %d %d",
                    &klog_on, &limit0, &limit1, &limit2, &limit3))
    {
        if (klog_on == 0 || klog_on == 1)
        {
          cl_bcct_klog_on = klog_on;
        }

        if (limit0 >= -1) MTK_CL_BCCT_SET_LIMIT(limit0, cl_bcct_state[0]);
        if (limit1 >= -1) MTK_CL_BCCT_SET_LIMIT(limit1, cl_bcct_state[1]);
        if (limit2 >= -1) MTK_CL_BCCT_SET_LIMIT(limit2, cl_bcct_state[2]);
        if (limit3 >= -1) MTK_CL_BCCT_SET_LIMIT(limit3, cl_bcct_state[3]);
        
        mtk_cooler_bcct_dprintk("[_mtk_cl_bcct_proc_write] limit0:%ld,limit1:%ld,limit2:%ld,limit3%ld\n",limit0,limit1,limit2,limit3);

        return len;
    }
    else
#else
#error "Change correspondent part when changing MAX_NUM_INSTANCE_MTK_COOLER_BCCT!"
#endif
    {
        mtk_cooler_bcct_dprintk("[_mtk_cl_bcct_proc_write] bad argument\n");
    }

    return -EINVAL;
}

static int _cl_bcct_read(struct seq_file *m, void *v)
{
    /**
     * The format to print out: 
     *  kernel_log <0 or 1>
     *  <mtk-cl-bcct<ID>> <bcc limit>
     *  ..
     */

    mtk_cooler_bcct_dprintk("_cl_bcct_read invoked.\n");

    {
        int i = 0;

        seq_printf(m, "klog %d\n", cl_bcct_klog_on);
        seq_printf(m, "curr_limit %d\n", cl_bcct_cur_limit);

        for (; i < MAX_NUM_INSTANCE_MTK_COOLER_BCCT; i++)
        {
            int limit;
            unsigned int curr_state;
            
            MTK_CL_BCCT_GET_LIMIT(limit, cl_bcct_state[i]);
            MTK_CL_BCCT_GET_CURR_STATE(curr_state, cl_bcct_state[i]);
        
            seq_printf(m, "mtk-cl-bcct%02d %d mA, state %d\n", i, limit, curr_state);
        }
    }

	return 0;
}

static int _cl_bcct_open(struct inode *inode, struct file *file)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
    return single_open(file, _cl_bcct_read, PDE_DATA(inode));
#else
    return single_open(file, _cl_bcct_read, PDE(inode)->data);
#endif
}

static const struct file_operations _cl_bcct_fops = {
    .owner = THIS_MODULE,
    .open = _cl_bcct_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .write = _cl_bcct_write,
    .release = single_release,
};


static int __init mtk_cooler_bcct_init(void)
{
  int err = 0;
  int i;

  for (i = MAX_NUM_INSTANCE_MTK_COOLER_BCCT; i-- > 0; )
  {
    cl_bcct_dev[i] = NULL;
    cl_bcct_state[i] = 0;
  }

  //cl_bcct_dev = NULL;

  mtk_cooler_bcct_dprintk("init\n");

  err = mtk_cooler_bcct_register_ltf();
  if (err)
    goto err_unreg;

  /* create a proc file */
  {
    struct proc_dir_entry *entry = NULL;

#if 0
    entry = create_proc_entry("driver/mtk-cl-bcct", S_IRUGO | S_IWUSR | S_IWGRP, NULL);
    
    if (NULL != entry)
    {
      entry->read_proc = _mtk_cl_bcct_proc_read;
      entry->write_proc = _mtk_cl_bcct_proc_write;
      entry->data = cl_bcct_state;
      entry->gid = 1000; // allow system process to write this proc
    }
#else
    entry = proc_create("driver/mtk-cl-bcct", S_IRUGO | S_IWUSR | S_IWGRP, NULL, &_cl_bcct_fops);
    if (!entry)
    {
        mtk_cooler_bcct_dprintk_always("mtk_cooler_bcct_init driver/mtk-cl-bcct creation failed\n");
    }
#endif
  }

  return 0;

err_unreg:
  mtk_cooler_bcct_unregister_ltf();
  return err;
}

static void __exit mtk_cooler_bcct_exit(void)
{
  mtk_cooler_bcct_dprintk("exit\n");

  /* remove the proc file */
  remove_proc_entry("driver/mtk-cl-bcct", NULL);
    
  mtk_cooler_bcct_unregister_ltf();
}

module_init(mtk_cooler_bcct_init);
module_exit(mtk_cooler_bcct_exit);



