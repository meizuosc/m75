//#define TPD_HAVE_BUTTON

#include "tpd.h"
#include <linux/interrupt.h>
#include <cust_eint.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/rtpm_prio.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <mach/mt_pm_ldo.h>
//#include <mach/mt6575_pll.h>
#include <linux/dma-mapping.h>

#ifdef TPD_GPT_TIMER_RESUME
#include <mach/hardware.h>
#include <mach/mt_gpt.h>
#include <linux/timer.h>
#endif
#include "tpd_custom_ft5x06.h"

#include "cust_gpio_usage.h"

//#define FTS_APK_DEBUG
#define SYSFS_DEBUG
//#define FTS_AUTO_UPGRADE
#if defined(SYSFS_DEBUG) || defined(FTS_AUTO_UPGRADE)
#include "ft5x06_ex_fun.h"
#endif

#define TPD_INFO(fmt, arg...)  printk("[tpd info:5x06]" "[%s]" fmt "\r\n", __FUNCTION__ ,##arg)
//#define TP_DEBUG
#undef TPD_DEBUG
#undef TPD_DMESG
#if defined(TP_DEBUG)
#define TPD_DEBUG(fmt, arg...)  printk("[tpd debug:5x06]" "[%s]" fmt "\r\n", __FUNCTION__ ,##arg)
#define TPD_DMESG(fmt, arg...)  printk("[tpd dmesg:5x06]" "[%s]" fmt "\r\n", __FUNCTION__ ,##arg)
#else
#define TPD_DEBUG(fmt, arg...)
#define TPD_DMESG(fmt, arg...)
#endif

extern struct tpd_device *tpd;
 
struct i2c_client *i2c_client = NULL;
struct task_struct *thread = NULL;
 
static DECLARE_WAIT_QUEUE_HEAD(waiter);
 
static void tpd_eint_interrupt_handler(void);
   
static int tpd_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tpd_detect(struct i2c_client *client, struct i2c_board_info *info);
static int tpd_remove(struct i2c_client *client);
static int touch_event_handler(void *unused);
 
static int tpd_flag = 0;
static int point_num = 0;
static int p_point_num = 0;

#define TPD_CLOSE_POWER_IN_SLEEP

#define TPD_OK 0
//register define

#define DEVICE_MODE 0x00
#define GEST_ID 0x01
#define TD_STATUS 0x02
#define FW_ID_ADDR	0xA6


//register define

#define FINGER_NUM_MAX	10

struct touch_info {
    int y[FINGER_NUM_MAX];
    int x[FINGER_NUM_MAX];
    int p[FINGER_NUM_MAX];
    int count;
};
 
static const struct i2c_device_id tpd_id[] = {{"mtk-tpd",0},{}};

#if defined(E1910) && !defined(TP_CFG_FOR_E1910_SMT)
unsigned short force[] = {0,0x72,I2C_CLIENT_END,I2C_CLIENT_END}; 
static const unsigned short * const forces[] = { force, NULL };
//static struct i2c_client_address_data addr_data = { .forces = forces, };
static struct i2c_board_info __initdata i2c_tpd = { I2C_BOARD_INFO("mtk-tpd", (0x72>>1))};
#else
unsigned short force[] = {0,0x70,I2C_CLIENT_END,I2C_CLIENT_END}; 
static const unsigned short * const forces[] = { force, NULL };
//static struct i2c_client_address_data addr_data = { .forces = forces, };
static struct i2c_board_info __initdata i2c_tpd = { I2C_BOARD_INFO("mtk-tpd", (0x70>>1))};
#endif

static struct kobject *touchdebug_kobj;
static struct kobject *touchdebug_kobj_info;
static int sensitivity_level = 1;
static int EnableWakeUp = 0;

/* Waiting for deivce resume and write back touch sensitivity level */
static struct timer_list sensitivity_write_timer;

/* Touch panel resume delay */
#define TOUCH_RESUME_INTERVAL 500

/* Workqueue for set touch sensitivity level */
static struct workqueue_struct *sensitivity_wq;
static struct work_struct *sensitivity_work;

struct sensitivity_mapping {
	int symbol;
	int value;
};

enum {
	TOUCH_SENSITIVITY_SYMBOL_HIGH = 0,
	TOUCH_SENSITIVITY_SYMBOL_MEDIUM,
	TOUCH_SENSITIVITY_SYMBOL_LOW,
	TOUCH_SENSITIVITY_SYMBOL_COUNT,
};

static struct sensitivity_mapping sensitivity_table[] = {
	{TOUCH_SENSITIVITY_SYMBOL_HIGH,    14},
	{TOUCH_SENSITIVITY_SYMBOL_MEDIUM,  16},
	{TOUCH_SENSITIVITY_SYMBOL_LOW,     19},
};

#define TOUCH_SENSITIVITY_SYMBOL_DEFAULT TOUCH_SENSITIVITY_SYMBOL_MEDIUM;

#define I2C_DEV_NUMBER		8
#define DRIVER_DEV_NAME	"fts_ts"

struct fts_dev{
	dev_t dev;
	struct cdev cdev;
	struct class *class;
	unsigned char *buf;
	struct fts_info *ts;
};

struct fts_info{
	struct i2c_client *client;
	struct input_dev *input_dev;
};

struct i2c_msg_formal{
	__u16 addr;	/* slave address			*/
	__u16 flags;
#define I2C_M_TEN		0x0010	/* this is a ten bit chip address */
#define I2C_M_RD		0x0001	/* read data, from slave to master */
#define I2C_M_NOSTART		0x4000	/* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_REV_DIR_ADDR	0x2000	/* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_IGNORE_NAK	0x1000	/* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_NO_RD_ACK		0x0800	/* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_RECV_LEN		0x0400	/* length will be first received byte */
	__u16 len;		/* msg length				*/
	__u8 *buf;		/* pointer to msg data			*/
};

static struct fts_dev ts_dev;

#include <mach/mt_boot.h>
static int boot_mode = 0; 

#ifdef TPD_HAVE_BUTTON
extern void tpd_button(unsigned int x, unsigned int y, unsigned int down); 
#if 0
#if 1
#define TPD_KEYS {KEY_HOME,KEY_MENU,KEY_BACK,KEY_SEARCH}
#define TPD_KEYS_DIM {{30,850,60,100},{180,850,60,100},{320,850,60,100},{450,850,60,100}}
#define TPD_KEY_COUNT 4
#else
#define TPD_KEYS {KEY_HOME,KEY_MENU,KEY_BACK}
#define TPD_KEYS_DIM {{80,850,60,100},{240,850,60,100},{400,850,60,100}}
#define TPD_KEY_COUNT 3
#endif
#endif
static int tpd_keys_local[TPD_KEY_COUNT] = TPD_KEYS;
static int tpd_keys_dim_local[TPD_KEY_COUNT][4] = TPD_KEYS_DIM;
#endif

#if (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION))
//static int tpd_calmat_local[8]     = TPD_CALIBRATION_MATRIX;
static int tpd_calmat_driver[8]            = {0};
static int tpd_def_calmat_local_normal[8]  = TPD_CALIBRATION_MATRIX_ROTATION_NORMAL;
static int tpd_def_calmat_local_factory[8] = TPD_CALIBRATION_MATRIX_ROTATION_FACTORY;
#endif
 
static struct i2c_driver tpd_i2c_driver = {
	.driver.name = "mtk-tpd",
	.probe = tpd_probe,
	.remove = tpd_remove,
	.id_table = tpd_id,
	.detect = tpd_detect,
	.address_list = (const unsigned short*) forces,
};
 
#if 0 
static unsigned short i2c_addr[] = {0x72}; 
#endif  

static u8 *gpDMABuf_va = NULL;
static u32 gpDMABuf_pa = 0;

int fts_dma_i2c_read(struct i2c_client *client, u16 addr, int len, u32 rxbuf)
{
	int ret;
	u8 buffer[1];

	struct i2c_msg msg[2] =
	{
		{
			.addr = client->addr,
			.flags = 0,
			.buf = buffer,
			.len = 1,
			.timing = 400
		},
		{
			.addr = client->addr,
			.ext_flag = (client->ext_flag | I2C_ENEXT_FLAG | I2C_DMA_FLAG),
			.flags = I2C_M_RD,
			.buf = (u8 *)rxbuf,
			.len = len,
			.timing = 400
		},
	};

	buffer[0] = addr;

	if ((u8 *)rxbuf == NULL)
		return -1;

	ret = i2c_transfer(client->adapter, &msg[0], 2);
	if (ret < 0)
		dev_err(&client->dev, "%s i2c dma read error.\n", __func__);

	return 0;
}

static ssize_t fts_i2cdev_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{
	char *tmp;
	int ret;
	/* struct fts_info *ts = ts_dev.ts; */ 
	struct i2c_client *client = ts_dev.ts->client;

	TPD_DMESG("called\n");

	if (count > 8192)
		count = 8192;

	tmp = kmalloc(count, GFP_KERNEL);
	if (tmp == NULL)
		return -ENOMEM;

	pr_debug("i2c-dev: i2c-%d reading %zu bytes.\n",
		iminor(file->f_path.dentry->d_inode), count);

	ret = i2c_master_recv(client, tmp, count);
	if (ret >= 0)
		ret = copy_to_user(buf, tmp, count) ? -EFAULT : ret;
	kfree(tmp);
	return ret;
}

static ssize_t fts_i2cdev_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
	int ret;
	char *tmp;
	/* struct fts_info *ts = ts_dev.ts; */
	struct i2c_client *client = ts_dev.ts->client;

	TPD_DMESG("called\n");

	if (count > 8192)
		count = 8192;
/*
	tmp = memdup_user(buf, count);
	if (IS_ERR(tmp))
		return PTR_ERR(tmp);
*/
	tmp = kmalloc(count,GFP_KERNEL);
	if (tmp==NULL)
		return -ENOMEM;
	if (copy_from_user(tmp,buf,count)) {
		kfree(tmp);
		return -EFAULT;
	}

	pr_debug("i2c-dev: i2c-%d writing %zu bytes.\n",
		iminor(file->f_path.dentry->d_inode), count);

	ret = i2c_master_send(client, tmp, count);
	kfree(tmp);
	return ret;
}

static int i2cdev_check(struct device *dev, void *addrp)
{
	struct i2c_client *client = i2c_verify_client(dev);
	/* struct fts_info *ts = ts_dev.ts; */

	TPD_DMESG("called\n");

	if (!client || client->addr != *(unsigned int *)addrp)
		return 0;

	return dev->driver ? -EBUSY : 0;
}

static int i2cdev_check_addr(struct i2c_adapter *adapter, unsigned int addr)
{
	return device_for_each_child(&adapter->dev, &addr, i2cdev_check);
}

static noinline int i2cdev_ioctl_rdrw(struct i2c_client *client,
		unsigned long arg)
{
	struct i2c_rdwr_ioctl_data rdwr_arg;
	struct i2c_msg *rdwr_pa, *rdwr_pa_tmp;
	struct i2c_msg_formal *rdwr_pa_formal;
	u8 __user **data_ptrs;
	int i, res;
	/* struct fts_info *ts = ts_dev.ts; */

	TPD_DMESG("called\n");

	if (copy_from_user(&rdwr_arg,
			   (struct i2c_rdwr_ioctl_data __user *)arg,
			   sizeof(rdwr_arg)))
		return -EFAULT;

	/* Put an arbitrary limit on the number of messages that can
	 * be sent at once */
	if (rdwr_arg.nmsgs > I2C_RDRW_IOCTL_MAX_MSGS)
		return -EINVAL;

	rdwr_pa = kmalloc(rdwr_arg.nmsgs * sizeof(struct i2c_msg), GFP_KERNEL);
	if (!rdwr_pa)
		return -ENOMEM;

	rdwr_pa_formal = kmalloc(rdwr_arg.nmsgs * sizeof(struct i2c_msg_formal), GFP_KERNEL);
	if (!rdwr_pa)
		return -ENOMEM;

	if (copy_from_user(rdwr_pa_formal, rdwr_arg.msgs,
			   rdwr_arg.nmsgs * sizeof(struct i2c_msg_formal))) {
		kfree(rdwr_pa);
		return -EFAULT;
	}

	data_ptrs = kmalloc(rdwr_arg.nmsgs * sizeof(u8 __user *), GFP_KERNEL);
	if (data_ptrs == NULL) {
		kfree(rdwr_pa);
		kfree(rdwr_pa_formal);
		return -ENOMEM;
	}

	res = 0;
	for (i = 0, rdwr_pa_tmp = rdwr_pa; i < rdwr_arg.nmsgs; i++, rdwr_pa_tmp++) {
		rdwr_pa_tmp->addr = rdwr_pa_formal->addr;
		rdwr_pa_tmp->flags = rdwr_pa_formal->flags;
		rdwr_pa_tmp->len = rdwr_pa_formal->len;
		rdwr_pa_tmp->timing = client->timing;
		rdwr_pa_tmp->ext_flag = 0;

		/* Limit the size of the message to a sane amount;
		 * and don't let length change either. */
		if ((rdwr_pa[i].len > 8192) ||
		    (rdwr_pa[i].flags & I2C_M_RECV_LEN)) {
			res = -EINVAL;
			break;
		}
		data_ptrs[i] = (u8 __user *)rdwr_pa_formal[i].buf;

		rdwr_pa[i].buf = kmalloc(rdwr_pa[i].len, GFP_KERNEL);
		if (rdwr_pa[i].buf == NULL) {
			res = -ENOMEM;
			break;
		}
		if (copy_from_user(rdwr_pa[i].buf, data_ptrs[i],
				   rdwr_pa[i].len)) {
				++i; /* Needs to be kfreed too */
				res = -EFAULT;
			break;
		}
/*
		rdwr_pa[i].buf = memdup_user(data_ptrs[i], rdwr_pa[i].len);
		if (IS_ERR(rdwr_pa[i].buf)) {
			res = PTR_ERR(rdwr_pa[i].buf);
			break;
		}
*/
	}
	if (res < 0) {
		int j;
		for (j = 0; j < i; ++j)
			kfree(rdwr_pa[j].buf);
		kfree(data_ptrs);
		kfree(rdwr_pa);
		kfree(rdwr_pa_formal);
		return res;
	}

	res = i2c_transfer(client->adapter, rdwr_pa, rdwr_arg.nmsgs);
	while (i-- > 0) {
		if (res >= 0 && (rdwr_pa[i].flags & I2C_M_RD)) {
			if (copy_to_user(data_ptrs[i], rdwr_pa[i].buf,
					 rdwr_pa[i].len))
				res = -EFAULT;
		}
		kfree(rdwr_pa[i].buf);
	}
	kfree(data_ptrs);
	kfree(rdwr_pa);
	kfree(rdwr_pa_formal);
	return res;
}

static noinline int i2cdev_ioctl_smbus(struct i2c_client *client,
		unsigned long arg)
{
	struct i2c_smbus_ioctl_data data_arg;
	union i2c_smbus_data temp;
	int datasize, res;

	if (copy_from_user(&data_arg,
			   (struct i2c_smbus_ioctl_data __user *) arg,
			   sizeof(struct i2c_smbus_ioctl_data)))
		return -EFAULT;
	if ((data_arg.size != I2C_SMBUS_BYTE) &&
	    (data_arg.size != I2C_SMBUS_QUICK) &&
	    (data_arg.size != I2C_SMBUS_BYTE_DATA) &&
	    (data_arg.size != I2C_SMBUS_WORD_DATA) &&
	    (data_arg.size != I2C_SMBUS_PROC_CALL) &&
	    (data_arg.size != I2C_SMBUS_BLOCK_DATA) &&
	    (data_arg.size != I2C_SMBUS_I2C_BLOCK_BROKEN) &&
	    (data_arg.size != I2C_SMBUS_I2C_BLOCK_DATA) &&
	    (data_arg.size != I2C_SMBUS_BLOCK_PROC_CALL)) {
		dev_dbg(&client->adapter->dev,
			"size out of range (%x) in ioctl I2C_SMBUS.\n",
			data_arg.size);
		return -EINVAL;
	}
	/* Note that I2C_SMBUS_READ and I2C_SMBUS_WRITE are 0 and 1,
	   so the check is valid if size==I2C_SMBUS_QUICK too. */
	if ((data_arg.read_write != I2C_SMBUS_READ) &&
	    (data_arg.read_write != I2C_SMBUS_WRITE)) {
		dev_dbg(&client->adapter->dev,
			"read_write out of range (%x) in ioctl I2C_SMBUS.\n",
			data_arg.read_write);
		return -EINVAL;
	}

	/* Note that command values are always valid! */

	if ((data_arg.size == I2C_SMBUS_QUICK) ||
	    ((data_arg.size == I2C_SMBUS_BYTE) &&
	    (data_arg.read_write == I2C_SMBUS_WRITE)))
		/* These are special: we do not use data */
		return i2c_smbus_xfer(client->adapter, client->addr,
				      client->flags, data_arg.read_write,
				      data_arg.command, data_arg.size, NULL);

	if (data_arg.data == NULL) {
		dev_dbg(&client->adapter->dev,
			"data is NULL pointer in ioctl I2C_SMBUS.\n");
		return -EINVAL;
	}

	if ((data_arg.size == I2C_SMBUS_BYTE_DATA) ||
	    (data_arg.size == I2C_SMBUS_BYTE))
		datasize = sizeof(data_arg.data->byte);
	else if ((data_arg.size == I2C_SMBUS_WORD_DATA) ||
		 (data_arg.size == I2C_SMBUS_PROC_CALL))
		datasize = sizeof(data_arg.data->word);
	else /* size == smbus block, i2c block, or block proc. call */
		datasize = sizeof(data_arg.data->block);

	if ((data_arg.size == I2C_SMBUS_PROC_CALL) ||
	    (data_arg.size == I2C_SMBUS_BLOCK_PROC_CALL) ||
	    (data_arg.size == I2C_SMBUS_I2C_BLOCK_DATA) ||
	    (data_arg.read_write == I2C_SMBUS_WRITE)) {
		if (copy_from_user(&temp, data_arg.data, datasize))
			return -EFAULT;
	}
	if (data_arg.size == I2C_SMBUS_I2C_BLOCK_BROKEN) {
		/* Convert old I2C block commands to the new
		   convention. This preserves binary compatibility. */
		data_arg.size = I2C_SMBUS_I2C_BLOCK_DATA;
		if (data_arg.read_write == I2C_SMBUS_READ)
			temp.block[0] = I2C_SMBUS_BLOCK_MAX;
	}
	res = i2c_smbus_xfer(client->adapter, client->addr, client->flags,
	      data_arg.read_write, data_arg.command, data_arg.size, &temp);
	if (!res && ((data_arg.size == I2C_SMBUS_PROC_CALL) ||
		     (data_arg.size == I2C_SMBUS_BLOCK_PROC_CALL) ||
		     (data_arg.read_write == I2C_SMBUS_READ))) {
		if (copy_to_user(data_arg.data, &temp, datasize))
			return -EFAULT;
	}
	return res;
}

static long fts_i2cdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct i2c_client *client = ts_dev.ts->client;
	unsigned long funcs;
	/* struct fts_info *ts = ts_dev.ts; */

	TPD_DMESG( "ioctl, cmd=0x%02x, arg=0x%02lx\n", cmd, arg);

	switch (cmd) {
	case I2C_SLAVE:
	case I2C_SLAVE_FORCE:
		/* NOTE:  devices set up to work with "new style" drivers
		 * can't use I2C_SLAVE, even when the device node is not
		 * bound to a driver.  Only I2C_SLAVE_FORCE will work.
		 *
		 * Setting the PEC flag here won't affect kernel drivers,
		 * which will be using the i2c_client node registered with
		 * the driver model core.  Likewise, when that client has
		 * the PEC flag already set, the i2c-dev driver won't see
		 * (or use) this setting.
		 */
		if ((arg > 0x3ff) ||
		    (((client->flags & I2C_M_TEN) == 0) && arg > 0x7f))
			return -EINVAL;
		if (cmd == I2C_SLAVE && i2cdev_check_addr(client->adapter, arg))
			return -EBUSY;
		/* REVISIT: address could become busy later */
		client->addr = arg;
		return 0;
	case I2C_TENBIT:
		if (arg)
			client->flags |= I2C_M_TEN;
		else
			client->flags &= ~I2C_M_TEN;
		return 0;
	case I2C_PEC:
		if (arg)
			client->flags |= I2C_CLIENT_PEC;
		else
			client->flags &= ~I2C_CLIENT_PEC;
		return 0;
	case I2C_FUNCS:
		funcs = i2c_get_functionality(client->adapter);
		return put_user(funcs, (unsigned long __user *)arg);

	case I2C_RDWR:
		return i2cdev_ioctl_rdrw(client, arg);

	case I2C_SMBUS:
		return i2cdev_ioctl_smbus(client, arg);

	case I2C_RETRIES:
		client->adapter->retries = arg;
		break;
	case I2C_TIMEOUT:
		/* For historical reasons, user-space sets the timeout
		 * value in units of 10 ms.
		 */
		client->adapter->timeout = msecs_to_jiffies(arg * 10);
		break;
	default:
		/* NOTE:  returning a fault code here could cause trouble
		 * in buggy userspace code.  Some old kernel bugs returned
		 * zero in this case, and userspace code might accidentally
		 * have depended on that bug.
		 */
		return -ENOTTY;
	}
	return 0;
}

static int fts_i2cdev_open(struct inode *inode, struct file *file)
{

	/* struct fts_info *ts = ts_dev.ts; */

	TPD_DMESG("called\n");

	if(ts_dev.ts){
		disable_irq(ts_dev.ts->client->irq);
		printk("[i2c-8] [fts_i2cdev_open][success]\n");
	}
	else{
		printk("[i2c-8] [fts_i2cdev_open][fail]\n");
		return -ENOMEM;
	}

	return 0;
}

static int fts_i2cdev_release(struct inode *inode, struct file *file)
{

	/* struct fts_info *ts = ts_dev.ts; */

/*
	struct i2c_client *client = file->private_data;

	i2c_put_adapter(client->adapter);
	kfree(client);
	file->private_data = NULL;
*/
	TPD_DMESG("called\n");
	if(ts_dev.ts)
		enable_irq(ts_dev.ts->client->irq);
	else
		return -ENOMEM;

	return 0;
}

static const struct file_operations fts_fops = {
	.owner		= THIS_MODULE,
	//.llseek		= no_llseek,
	.read		= fts_i2cdev_read,
	.write		= fts_i2cdev_write,
	.unlocked_ioctl	= fts_i2cdev_ioctl,
	.open		= fts_i2cdev_open,
	.release	= fts_i2cdev_release,
};

int fts_i2cdev_init(struct fts_info *ts)
{

	int ret;
	struct device *dev;

	memset(&ts_dev, 0, sizeof(struct fts_dev));

	ret = alloc_chrdev_region(&ts_dev.dev, 0, 1, DRIVER_DEV_NAME);
	if(ret){
		TPD_DMESG("Unable to get a dynamic major for %s.\n", DRIVER_DEV_NAME);
		return ret;
	}

	cdev_init(&ts_dev.cdev, &fts_fops);
	ts_dev.cdev.owner = THIS_MODULE;
	ret = cdev_add(&ts_dev.cdev, ts_dev.dev, 1);
	if (ret) {
		TPD_DMESG("Unable to register character device !\n");
		goto fail_add;
	}

	ts_dev.class = class_create(THIS_MODULE, DRIVER_DEV_NAME);

	if(IS_ERR(ts_dev.class)){
		TPD_DMESG("Unable to register i2c device class !\n");
		ret = PTR_ERR(ts_dev.class);
		goto err_class;
	}

	dev = device_create(ts_dev.class, NULL, ts_dev.dev, NULL, "i2c-%d", I2C_DEV_NUMBER);

	if(IS_ERR(dev)){
		TPD_DMESG("Failed to create device !\n");
		ret = PTR_ERR(dev);
		goto err_device;
	}

	ts_dev.ts = ts;

	return 0;

err_device:
	class_destroy(ts_dev.class);
err_class:
	cdev_del(&ts_dev.cdev);
fail_add:
	unregister_chrdev_region(ts_dev.dev, 1);

	return ret;
}

void fts_i2cdev_exit(void)
{

	cdev_del(&ts_dev.cdev);
	unregister_chrdev_region(ts_dev.dev, 1);
	device_destroy(ts_dev.class, ts_dev.dev);
	class_destroy(ts_dev.class);
}

int myatoi(const char *a)
{
	int s = 0;

	while(*a >= '0' && *a <= '9')
		s = (s << 3) + (s << 1) + *a++ - '0';
	return s;
}

static void sensitivity_set_func(struct work_struct *work)
{
	uint8_t wdata[1] = {0};
	int ret;

	TPD_INFO("sensitivity_set_func value:%d\n", sensitivity_level);

	wdata[0] = sensitivity_table[sensitivity_level].value;
	ret = ft5x0x_write_reg(i2c_client, 0x80, wdata[0]);
	if(ret < 0)
		TPD_INFO("Can not write sensitivity\n");

	if (EnableWakeUp) {
#ifdef MODIFY_SCANRATE_TO_DEFAULT_VALUE
		/* Modify scan rate to default value */
		wdata[0] = 40;
		ret = ft5x0x_write_reg(i2c_client, 0x89, wdata[0]);
		if(ret < 0)
			TPD_INFO("Can not write scan rate\n");
#endif
	}

	return;
}

static void sensitivity_func(long unsigned unused)
{
	TPD_INFO("sensitivity_func \n");

	queue_work(sensitivity_wq, sensitivity_work);

	return;
}

static ssize_t firmware_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char * buf)
{
	unsigned char reg_version = 0;
	i2c_smbus_read_i2c_block_data(i2c_client, FW_ID_ADDR, 1, &reg_version);
	return sprintf(buf, "FT0-%x0-13032500\n", reg_version);
}

static ssize_t wakeup_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char * buf, size_t n)
{
	int symbol = -1;
	int ret;

	symbol = myatoi(buf);
	if (EnableWakeUp != symbol) {
		EnableWakeUp = symbol;
		/* Modify wake up mode:
			0: disable
			1: only 2 fingers
			2: only 5 fingers
			3: 2 or 5 fingers*/
		ret = ft5x0x_write_reg(i2c_client, 0xac, EnableWakeUp);
		if(ret < 0)
			TPD_INFO("Can not write wake up mode\n");
	}

	TPD_INFO("wakeup_store value:%d\n", EnableWakeUp);

	return n;
}

static ssize_t wakeup_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char * buf)
{
	return sprintf(buf, "%d\n", EnableWakeUp);
}

static ssize_t sensitivity_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char * buf, size_t n)
{
	uint8_t wdata[1] = {0};
	int symbol = -1;
	int ret;

	symbol = myatoi(buf);
	sensitivity_level = symbol;
	TPD_INFO("sensitive_store value:%d\n", symbol);

	wdata[0] = sensitivity_table[symbol].value;

	ret = ft5x0x_write_reg(i2c_client, 0x80, wdata[0]);
	if(ret < 0)
		TPD_INFO("Can not write sensitivity\n");

	return n;
}

static ssize_t sensitivity_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char * buf)
{
	uint8_t rdata[1] = {0};
	int i, symbol = -1;

	if (!i2c_smbus_read_i2c_block_data(i2c_client, 0x80, 1, rdata)) {
		goto i2c_err;
	}

	for (i = 0; i < TOUCH_SENSITIVITY_SYMBOL_COUNT; i++) {
		if (sensitivity_table[i].value == rdata[0]) {
			symbol = sensitivity_table[i].symbol;
			break;
		}
	}

i2c_err:
	if (symbol == -1) {
		TPD_INFO("touch sensitivity default value\n");
		symbol = TOUCH_SENSITIVITY_SYMBOL_DEFAULT;
	}

	return sprintf(buf, "%d\n", symbol);
}

static struct kobj_attribute firmware_attr = { \
	.attr = { \
	.name = __stringify(firmware), \
	.mode = 0644, \
	}, \
	.show = firmware_show, \
};

static struct kobj_attribute wakeup_attr = { \
	.attr = { \
	.name = __stringify(wakeup), \
	.mode = 0644, \
	}, \
	.show = wakeup_show, \
	.store = wakeup_store, \
};

static struct kobj_attribute sensitivity_attr = { \
	.attr = { \
	.name = __stringify(sensitivity), \
	.mode = 0644, \
	}, \
	.show = sensitivity_show, \
	.store = sensitivity_store, \
};

static struct attribute * g[] = {
	&sensitivity_attr.attr,
	&wakeup_attr.attr,
	NULL,
};

static struct attribute * g_info[] = {
	&firmware_attr.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = g,
};

static struct attribute_group attr_group_info = {
	.attrs = g_info,
};

static  void tpd_down(int x, int y, int p) {

#ifdef TPD_HAVE_BUTTON
#if defined(TP_CFG_FOR_E1910_SMT) || defined(ROTATION_FOR_E1910_CQ)
	if(MTK_LCM_PHYSICAL_ROTATION == 270 || MTK_LCM_PHYSICAL_ROTATION == 90)
        {
		#if defined(TP_HEIGHT)
		if(boot_mode!=NORMAL_BOOT && x>=TP_HEIGHT) { 
			tpd_button(x, y, 1);
		#else
		if(boot_mode!=NORMAL_BOOT && x>=TPD_RES_Y) { 
			tpd_button(x, y, 1);
		#endif

			return;
		}
	}
        else
#endif
	{
		#if defined(TP_HEIGHT)
		if(boot_mode!=NORMAL_BOOT && y>=TP_HEIGHT) { 
			tpd_button(x, y, 1);
		#else
		if(boot_mode!=NORMAL_BOOT && y>=TPD_RES_Y) { 
			tpd_button(x, y, 1);
		#endif

			return;
		}
	}

#endif 

	// input_report_abs(tpd->dev, ABS_PRESSURE, p);
	 input_report_key(tpd->dev, BTN_TOUCH, 1);
	 input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 1);

#if defined(TP_CFG_FOR_E1910_SMT) || defined(ROTATION_FOR_E1910_CQ)
	if(boot_mode!=NORMAL_BOOT && (MTK_LCM_PHYSICAL_ROTATION == 270 || MTK_LCM_PHYSICAL_ROTATION == 90) )
	{
		int temp;

		temp = y;
		y = x;
		x = TPD_RES_X-temp;
	}
#endif

	 input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
	 input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
	 TPD_DEBUG("D[%4d %4d %4d] ", x, y, p);
	 input_mt_sync(tpd->dev);
	 TPD_DOWN_DEBUG_TRACK(x,y);
 }
 
 static void tpd_up(int x, int y,int p) {
	 //input_report_abs(tpd->dev, ABS_PRESSURE, 0);
		 input_report_key(tpd->dev, BTN_TOUCH, 0);
		 //input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 0);
		 //input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
		 //input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
		 //printk("U[%4d %4d %4d] ", x, y, 0);
		 input_mt_sync(tpd->dev);
		 TPD_UP_DEBUG_TRACK(x,y);
 }

static int tpd_touchinfo(struct touch_info *cinfo, struct touch_info *pinfo)
{
	int i = 0;
	char *data = gpDMABuf_va;
	u16 high_byte,low_byte;

	p_point_num = point_num;
        memcpy(pinfo, cinfo, sizeof(struct touch_info));
        memset(cinfo, 0, sizeof(struct touch_info));

	fts_dma_i2c_read(i2c_client, 0x00, 64, gpDMABuf_pa);
	//TPD_DEBUG("FW version=%x]\n",data[24]);

	//TPD_DEBUG("received raw data from touch panel as following:\n");
	//TPD_DEBUG("[data[0]=%x,data[1]= %x ,data[2]=%x ,data[3]=%x ,data[4]=%x ,data[5]=%x]\n",data[0],data[1],data[2],data[3],data[4],data[5]);
	//TPD_DEBUG("[data[9]=%x,data[10]= %x ,data[11]=%x ,data[12]=%x]\n",data[9],data[10],data[11],data[12]);
	//TPD_DEBUG("[data[15]=%x,data[16]= %x ,data[17]=%x ,data[18]=%x]\n",data[15],data[16],data[17],data[18]);

	/* Device Mode[2:0] == 0 :Normal operating Mode*/
	if((data[0] & 0x70) != 0) return false; 

	/*get the number of the touch points*/
	point_num= data[2] & 0x0f;
	
	TPD_DEBUG("point_num =%d\n",point_num);

	if(FINGER_NUM_MAX < point_num)
	{
		TPD_DEBUG("point_num is error\n");
		return false;
	}
	
//	if(point_num == 0) return false;

	//TPD_DEBUG("Procss raw data...\n");

		
	for(i = 0; i < point_num; i++)
	{
		cinfo->p[i] = data[3+6*i] >> 6; //event flag 

       		/*get the X coordinate, 2 bytes*/
		high_byte = data[3+6*i];
		high_byte <<= 8;
		high_byte &= 0x0f00;
		low_byte = data[3+6*i + 1];
		cinfo->x[i] = high_byte |low_byte;

		//cinfo->x[i] =  cinfo->x[i] * 480 >> 11; //calibra
		/*get the Y coordinate, 2 bytes*/		
		high_byte = data[3+6*i+2];
		high_byte <<= 8;
		high_byte &= 0x0f00;
		low_byte = data[3+6*i+3];
		cinfo->y[i] = high_byte |low_byte;

		//cinfo->y[i]=  cinfo->y[i] * 800 >> 11;	
		cinfo->count++;
	
#if defined(TPD_RES_X) && defined(TP_WIDTH) 
		cinfo->x[i] = cinfo->x[i]*TP_WIDTH/TPD_RES_X;
#endif
#if defined(TPD_RES_Y) && defined(TP_HEIGHT)
		cinfo->y[i] = cinfo->y[i]*TP_HEIGHT/TPD_RES_Y;
#endif

		TPD_DEBUG(" cinfo->x[i=%d] = %d, cinfo->y[i] = %d, cinfo->p[i] = %d\n", i,cinfo->x[i], cinfo->y[i], cinfo->p[i]);
	}

	//TPD_DEBUG(" cinfo->x[0] = %d, cinfo->y[0] = %d, cinfo->p[0] = %d\n", cinfo->x[0], cinfo->y[0], cinfo->p[0]);	
	//TPD_DEBUG(" cinfo->x[1] = %d, cinfo->y[1] = %d, cinfo->p[1] = %d\n", cinfo->x[1], cinfo->y[1], cinfo->p[1]);		
	//TPD_DEBUG(" cinfo->x[2]= %d, cinfo->y[2]= %d, cinfo->p[2] = %d\n", cinfo->x[2], cinfo->y[2], cinfo->p[2]);	
	  
#if defined(SMT_TP_CONFIG)
	for(i = 0; i < point_num; i++)
	{
		cinfo->x[i] = cinfo->x[i] *36/51;//5.1CM/5.4CM
		cinfo->y[i] = cinfo->y[i] *27/37;//7.4CM/9.0CM
	}
#endif

#if 0/*!defined(TP_CFG_FOR_E1910_SMT) && !defined(ROTATION_FOR_E1910_CQ)*/
	if(MTK_LCM_PHYSICAL_ROTATION == 270 || MTK_LCM_PHYSICAL_ROTATION == 90)
	{
		for(i = 0; i < point_num; i++)
		{
			int temp;

			temp = cinfo->x[i];
			cinfo->x[i] = TPD_RES_X-cinfo->y[i];
			cinfo->y[i] = temp;
		}

		TPD_DEBUG("rot cinfo->x[0] = %d, cinfo->y[0] = %d, cinfo->p[0] = %d\n", cinfo->x[0], cinfo->y[0], cinfo->p[0]);	
		TPD_DEBUG("rot cinfo->x[1] = %d, cinfo->y[1] = %d, cinfo->p[1] = %d\n", cinfo->x[1], cinfo->y[1], cinfo->p[1]);
	}
#endif
	  
	return true;
};

/*Coordination mapping*/
static void tpd_calibrate_driver(int *x, int *y)
{
    int tx;

    TPD_DEBUG("Call tpd_calibrate of this driver ..\n");
    tx = ((tpd_calmat_driver[0] * (*x)) + (tpd_calmat_driver[1] * (*y)) + (tpd_calmat_driver[2])) >> 12;
    *y = ((tpd_calmat_driver[3] * (*x)) + (tpd_calmat_driver[4] * (*y)) + (tpd_calmat_driver[5])) >> 12;
    *x = tx;
}

static int touch_event_handler(void *unused)
{
	struct touch_info cinfo, pinfo;
	int i = 0;
	int input_x = 0;
	int input_y = 0;

	struct sched_param param = { .sched_priority = RTPM_PRIO_TPD };
	sched_setscheduler(current, SCHED_RR, &param);
 
	 do
	 {
		mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM); 
		set_current_state(TASK_INTERRUPTIBLE); 
		wait_event_interruptible(waiter,tpd_flag!=0);
		 
		tpd_flag = 0;

		set_current_state(TASK_RUNNING);

		if (tpd_touchinfo(&cinfo, &pinfo))
		{
			TPD_DEBUG("point_num = %d\n",point_num);
			  
			if(point_num >0) 
			{
				#if 0
				tpd_down(cinfo.x[0], cinfo.y[0], 1);
				if(point_num>1)
				{
					tpd_down(cinfo.x[1], cinfo.y[1], 1);
					if(point_num >2) 
						tpd_down(cinfo.x[2], cinfo.y[2], 1);
				}
				#else
				while(i<point_num)
				{
					//tpd_down(cinfo.x[i], cinfo.y[i], 1);
					input_x = cinfo.x[i];
					input_y = cinfo.y[i];
					tpd_calibrate_driver(&input_x, &input_y);
					tpd_down(input_x, input_y, 1);
					i++;
				}
				i = 0;
				#endif
				TPD_DEBUG("press --->\n");
				
		    	} 
			else  
			{
				TPD_DEBUG("release --->\n"); 

				if(p_point_num >1)
				{
					i = 0;
					while(i<p_point_num){
						tpd_up(pinfo.x[i], pinfo.y[i], 1);
						i++;
					}
				}
				else
				{
					tpd_up(pinfo.x[0], pinfo.y[0], 1);
				}
				i = 0;

			#ifdef TPD_HAVE_BUTTON
				if(boot_mode!=NORMAL_BOOT && tpd->btn_state) 
				{ 
					tpd_button(pinfo.x[0], pinfo.y[0], 0);
				}
			#endif

            		}

			input_sync(tpd->dev);
        	}

 	}while(!kthread_should_stop());

	return 0;
}
 
static int tpd_detect (struct i2c_client *client, struct i2c_board_info *info) 
{
	TPD_DEBUG("tpd_detect\n");
	strcpy(info->type, TPD_DEVICE);	
	return 0;
}

static void tpd_eint_interrupt_handler(void)
{
	TPD_DEBUG("TPD interrupt has been triggered\n");
	tpd_flag = 1;
	mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
	wake_up_interruptible(&waiter);
}

static void tpd_gpio_config(void)
{

	mt_set_gpio_pull_enable(GPIO_CTP_RST_PIN, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(GPIO_CTP_RST_PIN, GPIO_PULL_UP);
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);

	mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
	mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_UP);

	msleep(50);
}

static int  tpd_probe(struct i2c_client *client, const struct i2c_device_id *id)
{	 
	int retval = TPD_OK;
	char data;
	/* int i; */
	struct fts_info *ts;
	int err = 0;

	i2c_client = client;
	TPD_INFO("tpd_probe\n");

	tpd_gpio_config();

	#ifdef TPD_CLOSE_POWER_IN_SLEEP	 

#ifdef TPD_POWER_SOURCE_CUSTOM
	hwPowerOn(TPD_POWER_SOURCE_CUSTOM, VOL_2800, "TP");
#else
#ifndef TPD_LDO_VOL
	hwPowerOn(TPD_POWER_SOURCE, VOL_2800, "TP");
#else
	hwPowerOn(TPD_POWER_SOURCE, TPD_LDO_VOL, "TP");
#endif //TPD_LDO_VOL
#endif
#ifdef TPD_POWER_SOURCE_1800
	hwPowerOn(TPD_POWER_SOURCE_1800, VOL_1800, "TP");
#endif
	msleep(100);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	mdelay(200);

#if 0
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	TPD_INFO("TPD_CLOSE_POWER_IN_SLEEP\n");
        for(i = 0; i < 2; i++) /*Do Power on again to avoid tp bug*/
	{
#ifdef TPD_POWER_SOURCE_CUSTOM
    		hwPowerDown(TPD_POWER_SOURCE_CUSTOM,  "TP");
#else
    		hwPowerDown(TPD_POWER_SOURCE,  "TP");
#endif
#ifdef TPD_POWER_SOURCE_1800
    		hwPowerDown(TPD_POWER_SOURCE_1800,  "TP");
#endif
		mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
		mdelay(10);
		mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
		mdelay(50);
#ifdef TPD_POWER_SOURCE_CUSTOM
    		hwPowerOn(TPD_POWER_SOURCE_CUSTOM, VOL_2800, "TP");
#else
    		hwPowerOn(TPD_POWER_SOURCE, VOL_2800, "TP");
#endif
#ifdef TPD_POWER_SOURCE_1800
    		hwPowerOn(TPD_POWER_SOURCE_1800, VOL_1800, "TP");
#endif  

		msleep(100);
	}
#endif
	#else
#if 1 
#ifdef TPD_POWER_SOURCE_CUSTOM
	hwPowerDown(TPD_POWER_SOURCE_CUSTOM,  "TP");
#else
	hwPowerDown(TPD_POWER_SOURCE,	"TP");
#endif
#ifdef TPD_POWER_SOURCE_1800
	hwPowerDown(TPD_POWER_SOURCE_1800,	"TP");
#endif

	TPD_INFO("tpd power on!\n");
#ifdef TPD_POWER_SOURCE_CUSTOM
	hwPowerOn(TPD_POWER_SOURCE_CUSTOM, VOL_2800, "TP");
#else
#ifndef TPD_LDO_VOL
	hwPowerOn(TPD_POWER_SOURCE, VOL_2800, "TP");
#else
	hwPowerOn(TPD_POWER_SOURCE, TPD_LDO_VOL, "TP");
#endif //TPD_LDO_VOL
#endif
#ifdef TPD_POWER_SOURCE_1800
	hwPowerOn(TPD_POWER_SOURCE_1800, VOL_1800, "TP");
#endif  
	msleep(100);
#endif
	/*
	mt_set_gpio_mode(GPIO_CTP_EN_PIN, GPIO_CTP_EN_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_CTP_EN_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_EN_PIN, GPIO_OUT_ONE);
	msleep(100);
	*/
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	#endif
	
	TPD_INFO("addr:0x%02x",i2c_client->addr);

	if((i2c_smbus_read_i2c_block_data(i2c_client, FW_ID_ADDR, 1, &data))< 0)
	{
#if 0 
		for(i = 0; i < sizeof(i2c_addr)/ sizeof(i2c_addr[0]); i++)
		{
			i2c_client->addr = i2c_addr[i];
			TPD_INFO("addr:0x%02x",i2c_client->addr);
			if((i2c_smbus_read_i2c_block_data(i2c_client, 0x00, 1, &data))>= 0)
			{
				goto i2c_transfer_sucess;
			}
		}
#endif  

		TPD_INFO("I2C transfer error, line: %d\n", __LINE__);
		return -1; 
	}
#if 0 
i2c_transfer_sucess:
#endif
	tpd_load_status = 1;

	touchdebug_kobj = kobject_create_and_add("Touch", NULL);
	if (touchdebug_kobj == NULL)
		TPD_INFO("%s: subsystem_register failed\n", __func__);

	if (sysfs_create_group(touchdebug_kobj, &attr_group))
		TPD_INFO("%s:sysfs_create_group failed\n", __func__);

	touchdebug_kobj_info = kobject_create_and_add("dev-info_touch", NULL);
	if (touchdebug_kobj_info == NULL)
		TPD_INFO("%s: subsystem_register failed\n", __func__);

	if (sysfs_create_group(touchdebug_kobj_info, &attr_group_info))
		TPD_INFO("%s:sysfs_create_group failed\n", __func__);

	sensitivity_work = kzalloc(sizeof(typeof(*sensitivity_work)), GFP_KERNEL);
	if (!sensitivity_work) {
		TPD_INFO("create work queue error, line: %d\n", __LINE__);
		return -1;
	}
	INIT_WORK(sensitivity_work, sensitivity_set_func);

	sensitivity_wq = create_singlethread_workqueue("sensitivity_wq");
	if (!sensitivity_wq) {
		kfree(sensitivity_work);
		TPD_INFO("create thread error, line: %d\n", __LINE__);
		return -1;
	}
	setup_timer(&sensitivity_write_timer, sensitivity_func, 0);

	thread = kthread_run(touch_event_handler, 0, TPD_DEVICE);
	if (IS_ERR(thread))
	{ 
		retval = PTR_ERR(thread);
		TPD_DMESG(TPD_DEVICE " failed to create kernel thread: %d\n", retval);
	}

	//mt_eint_set_sens(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_SENSITIVE);
	
	mt_eint_set_hw_debounce(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN);
	mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_TYPE, tpd_eint_interrupt_handler, 1); 
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
	msleep(50);

	ts = kzalloc (sizeof(struct fts_info), GFP_KERNEL);
	if (!ts)
		return err;

	ts->client = client;
	i2c_set_clientdata(client, ts);

	err = fts_i2cdev_init(ts);
	if(err<0)
		TPD_DMESG("[i2c-8][Fail]\n");
	else
		TPD_DMESG("[i2c-8][Success]\n");

#ifdef SYSFS_DEBUG
	ft5x0x_create_sysfs(i2c_client);
#endif

#ifdef FTS_APK_DEBUG
	ft5x0x_create_apk_debug_channel(i2c_client);
#endif
#ifdef FTS_AUTO_UPGRADE
	fts_ctpm_auto_upgrade(i2c_client);
#endif
	TPD_DMESG("Touch Panel Device Probe %s\n", (retval < TPD_OK) ? "FAIL" : "PASS");
	TPD_DMESG("[i2c-8] i2c_client->timing:%d\n", i2c_client->timing);
	return 0;
}

static int tpd_remove(struct i2c_client *client)
{
	struct fts_info *ts = i2c_get_clientdata(client);
	#ifdef FTS_APK_DEBUG
	ft5x0x_release_apk_debug_channel();
	#endif
	#ifdef SYSFS_DEBUG
	ft5x0x_release_sysfs(client);
	#endif
	TPD_INFO("TPD removed\n");

	del_timer_sync(&sensitivity_write_timer);
	cancel_work_sync(sensitivity_work);
	destroy_workqueue(sensitivity_wq);
	kfree(sensitivity_work);

	fts_i2cdev_exit();
	kfree(ts);

	return 0;
}
 
 
static int tpd_local_init(void)
{
	TPD_DMESG("Focaltech FT5x06 I2C Touchscreen Driver (Built %s @ %s)\n", __DATE__, __TIME__);
	
	gpDMABuf_va = (u8 *)dma_alloc_coherent(NULL, 64, &gpDMABuf_pa, GFP_KERNEL);
	if(!gpDMABuf_va){
        DBG("[Error] Allocate DMA I2C Buffer failed!\n");
	}

	boot_mode = get_boot_mode();
	if(boot_mode==3) boot_mode = NORMAL_BOOT;
#ifdef TPD_HAVE_BUTTON  
	tpd_button_setting(TPD_KEY_COUNT, tpd_keys_local, tpd_keys_dim_local);// initialize tpd button data
#endif

	if(i2c_add_driver(&tpd_i2c_driver)!=0)
	{
		TPD_DMESG("unable to add i2c driver.\n");
		return -1;
	}

#if (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION))

    if (FACTORY_BOOT == get_boot_mode())
    {
        TPD_INFO("Factory mode is detected! \n");
        memcpy(tpd_calmat_driver, tpd_def_calmat_local_factory, sizeof(tpd_calmat_driver));
    }
    else
    {
        TPD_INFO("Normal mode is detected! \n");
        memcpy(tpd_calmat_driver, tpd_def_calmat_local_normal, sizeof(tpd_calmat_driver));
    }
#endif


	input_set_abs_params(tpd->dev, ABS_MT_TRACKING_ID, 0, FINGER_NUM_MAX-1, 0, 0);//for linux3.8

	TPD_DMESG("end %s, %d\n", __FUNCTION__, __LINE__);  
	tpd_type_cap = 1;

	return 0; 
}

#ifdef TPD_GPT_TIMER_RESUME
// GPTimer
void ctp_thread_wakeup(UINT16 i)
{
	//printk("**** ctp_thread_wakeup****\n" );
	GPT_NUM  gpt_num = GPT6;	
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);  
	GPT_Stop(gpt_num); 
}

void CTP_Thread_XGPTConfig(void)
{
	GPT_CONFIG config;	
	GPT_NUM  gpt_num = GPT6;    
	GPT_CLK_SRC clkSrc = GPT_CLK_SRC_RTC;
	//GPT_CLK_DIV clkDiv = GPT_CLK_DIV_128;
	GPT_CLK_DIV clkDiv = GPT_CLK_DIV_64;

	//printk("***CTP_Thread_XGPTConfig***\n" );

    GPT_Init (gpt_num, ctp_thread_wakeup);
    config.num = gpt_num;
    config.mode = GPT_REPEAT;
	config.clkSrc = clkSrc;
    config.clkDiv = clkDiv;
    //config.u4Timeout = 10*128;
    config.u4CompareL = 256; // 10s : 512*64=32768
    config.u4CompareH = 0;
	config.bIrqEnable = TRUE;
    
    if (GPT_Config(config) == FALSE )
        return;                       
        
    GPT_Start(gpt_num);  

    return ;  
}
#endif

static void tpd_resume(struct early_suspend *h)
{
	/* int retval = TPD_OK; */
	char data = 0;
	int ret = 0;/* retry_num = 0, */

	TPD_INFO("TPD wake up\n");

	if (!EnableWakeUp) {
#ifdef TPD_GPT_TIMER_RESUME
		mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);

		mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
		mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);

		msleep(10);
#ifdef TPD_POWER_SOURCE_CUSTOM
		hwPowerDown(TPD_POWER_SOURCE_CUSTOM,  "TP");
#else
		hwPowerDown(TPD_POWER_SOURCE,	"TP");
#endif
#ifdef TPD_POWER_SOURCE_1800
		hwPowerDown(TPD_POWER_SOURCE_1800,	"TP");
#endif
#ifdef TPD_POWER_SOURCE_CUSTOM
		hwPowerOn(TPD_POWER_SOURCE_CUSTOM, VOL_2800, "TP");
#else
#ifndef TPD_LDO_VOL
	hwPowerOn(TPD_POWER_SOURCE, VOL_2800, "TP");
#else
	hwPowerOn(TPD_POWER_SOURCE, TPD_LDO_VOL, "TP");
#endif //TPD_LDO_VOL
#endif
#ifdef TPD_POWER_SOURCE_1800
		hwPowerOn(TPD_POWER_SOURCE_1800, VOL_1800, "TP");
#endif  
		// Run GPT timer
		CTP_Thread_XGPTConfig();
#else
	#ifdef TPD_CLOSE_POWER_IN_SLEEP
		mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
		mdelay(1);
#ifdef TPD_POWER_SOURCE_CUSTOM
		hwPowerOn(TPD_POWER_SOURCE_CUSTOM, VOL_2800, "TP");
#else
#ifndef TPD_LDO_VOL
	hwPowerOn(TPD_POWER_SOURCE, VOL_2800, "TP");
#else
	hwPowerOn(TPD_POWER_SOURCE, TPD_LDO_VOL, "TP");
#endif //TPD_LDO_VOL
#endif
#ifdef TPD_POWER_SOURCE_1800
		hwPowerOn(TPD_POWER_SOURCE_1800, VOL_1800, "TP");
#endif
		msleep(300);
		mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
		msleep(2);
		mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
		msleep(200);
#if 0
		mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
		mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);

		do{
			msleep(10);
#ifdef TPD_POWER_SOURCE_CUSTOM
			hwPowerOn(TPD_POWER_SOURCE_CUSTOM, VOL_2800, "TP");
#else
			hwPowerOn(TPD_POWER_SOURCE, VOL_2800, "TP");
#endif
#ifdef TPD_POWER_SOURCE_1800
			hwPowerOn(TPD_POWER_SOURCE_1800, VOL_1800, "TP");
#endif
			msleep(300);

			if((ret = i2c_smbus_read_i2c_block_data(i2c_client, FW_ID_ADDR, 1, &data))< 0)
			{
				TPD_DEBUG("i2c transf error before reset :ret=%d,retry_num == %d\n",ret,retry_num);

#ifdef TPD_POWER_SOURCE_CUSTOM
				hwPowerDown(TPD_POWER_SOURCE_CUSTOM,  "TP");
#else
				hwPowerDown(TPD_POWER_SOURCE,	"TP");
#endif
#ifdef TPD_POWER_SOURCE_1800
				hwPowerDown(TPD_POWER_SOURCE_1800,	"TP");
#endif
			}
			else
			{
				TPD_DEBUG("i2c transfer success after reset :ret=%d,retry_num == %d\n",ret,retry_num);
				break;
			}
			retry_num++;
		}while(retry_num < 10);

		if((ret = i2c_smbus_read_i2c_block_data(i2c_client, FW_ID_ADDR, 1, &data))< 0)
		{
			TPD_DEBUG("i2c transf error before reset :ret=%d,retry_num == %d\n",ret,retry_num);

			mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
			mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
			mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
			msleep(100);

			mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
			mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
			mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
			msleep(50);
			mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
			mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
			mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
			msleep(400);

			if((ret = i2c_smbus_read_i2c_block_data(i2c_client, FW_ID_ADDR, 1, &data))< 0)
			{
				TPD_DEBUG("i2c transf error after reset :ret = %d,retry_num == %d\n",ret,retry_num);
			}
			else
			{
				TPD_DEBUG("i2c transfer success after reset :ret = %d,retry_num == %d\n",ret,retry_num);
			}
		}
		TPD_DEBUG("retry_num == %d\n",retry_num);
#endif
#else
		mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
		mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
		msleep(100);

		mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
		mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
		msleep(50);  
		mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
		mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
		msleep(400);
#endif
		mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
#endif
	} else {
		data = 0;
		ret = ft5x0x_write_reg(i2c_client, 0xab, data);
		if(ret < 0)
			TPD_INFO("Resume can not write 0xAB\n");
	}

	mod_timer(&sensitivity_write_timer,
		jiffies + msecs_to_jiffies(TOUCH_RESUME_INTERVAL));

	/* return retval; */
}
 
static void tpd_suspend(struct early_suspend *h)
{
	/* int retval = TPD_OK; */
	static char data = 0x3;
	int ret;

	TPD_INFO("TPD enter sleep\n");

	if (!EnableWakeUp) {
		mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
	#ifdef TPD_CLOSE_POWER_IN_SLEEP
		mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
		mdelay(1);
#ifdef TPD_POWER_SOURCE_CUSTOM
		hwPowerDown(TPD_POWER_SOURCE_CUSTOM,  "TP");
#else
		hwPowerDown(TPD_POWER_SOURCE,  "TP");
#endif
#ifdef TPD_POWER_SOURCE_1800
		hwPowerDown(TPD_POWER_SOURCE_1800,  "TP");
#endif
	#else
		ft5x0x_write_reg(i2c_client, 0xA5, data);	//TP enter sleep mode
		/*
		mt_set_gpio_mode(GPIO_CTP_EN_PIN, GPIO_CTP_EN_PIN_M_GPIO);
		mt_set_gpio_dir(GPIO_CTP_EN_PIN, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_CTP_EN_PIN, GPIO_OUT_ZERO);
		*/
	#endif
	} else {
		data = 1;
		ret = ft5x0x_write_reg(i2c_client, 0xab, data);
		if(ret < 0)
			TPD_INFO("Suspend can not write 0xAB\n");
	}
	/* return retval; */
} 


static struct tpd_driver_t tpd_device_driver = {
	.tpd_device_name = "FT5x06",
	.tpd_local_init = tpd_local_init,
	.suspend = tpd_suspend,
	.resume = tpd_resume,
	#ifdef TPD_HAVE_BUTTON
	.tpd_have_button = 1,
	#else
	.tpd_have_button = 0,
	#endif		
};

/* called when loaded into kernel */
static int __init tpd_driver_init(void) {
	TPD_DEBUG("MediaTek FT5x06 touch panel driver init\n");
	i2c_register_board_info(TPD_I2C_NUMBER, &i2c_tpd, 1);
	if(tpd_driver_add(&tpd_device_driver) < 0)
		TPD_DMESG("add FT5x06 driver failed\n");

	return 0;
}
 
/* should never be called */
static void __exit tpd_driver_exit(void) {
	TPD_DMESG("MediaTek FT5x06 touch panel driver exit\n");
	//input_unregister_device(tpd->dev);
	tpd_driver_remove(&tpd_device_driver);
}
 
 module_init(tpd_driver_init);
 module_exit(tpd_driver_exit);
