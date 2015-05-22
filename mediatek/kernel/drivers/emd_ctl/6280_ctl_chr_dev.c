#include <linux/sched.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/uaccess.h>
#include <asm/io.h>
#include <linux/fs.h>
#include <linux/semaphore.h>
#include <linux/cdev.h>
#include <linux/kfifo.h>
#include <linux/wakelock.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <cust_eint.h>
#include <mach/eint.h>
#include <mtk_kpd.h>
#include <mach/mt_gpio.h>
#include <linux/aee.h>
#include <linux/slab.h>
#include <asm/siginfo.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/debugfs.h>
/******************************************************************************************
 *   Macro definition region
 ******************************************************************************************/

#define DEVICE0_ID			(0x0)
#define USER0_NAME			"USB-Host"
#define DEVICE1_ID			(0x1)
#define USER1_NAME			"UART"

#define EMD_CHR_DEV_MAJOR		(167) // Note, may need to change
#define EMD_DEV_NAME			"ext-md-ctl"
#define EMD_DEV_NODE_NAME		"ext_md_ctl"

#define EXT_MD_IOC_MAGIC		'E'
#define EXT_MD_IOCTL_LET_MD_GO		_IO(EXT_MD_IOC_MAGIC, 1)
#define EXT_MD_IOCTL_REQUEST_RESET	_IO(EXT_MD_IOC_MAGIC, 2)
#define EXT_MD_IOCTL_POWER_ON_HOLD	_IO(EXT_MD_IOC_MAGIC, 3)
#define EXT_MD_IOCTL_POWER_ON   	_IO(EXT_MD_IOC_MAGIC, 100)
#define EXT_MD_IOCTL_POWER_OFF   	_IO(EXT_MD_IOC_MAGIC, 102)
#define EXT_MD_IOCTL_RESET      	_IO(EXT_MD_IOC_MAGIC, 103)
#define EXT_MD_IOCTL_R8_TO_PC   	_IO(EXT_MD_IOC_MAGIC, 104)
#define EXT_MD_IOCTL_R8_TO_AP   	_IO(EXT_MD_IOC_MAGIC, 105)
#define EXT_MD_IOCTL_R8_DOWNLOAD   	_IO(EXT_MD_IOC_MAGIC, 106)
#define EXT_MD_IOCTL_R8_ASSERTLOG    _IO(EXT_MD_IOC_MAGIC, 107)
#define EXT_MD_IOCTL_R8_ASSERTLOG_STATUS    _IO(EXT_MD_IOC_MAGIC, 108)
#define EXT_MD_IOCTL_R8_WAKEUPEN    _IO(EXT_MD_IOC_MAGIC, 109)

#define EMD_ERR_ACCESS_DENY		(1)
#define EMD_ERR_UN_DEF_CMD		(2)

#define EMD_CLIENT_NUM			(3)
#define EMD_MAX_MESSAGE_NUM		(16)
#define WAKEUP_EMD_USER_NUM		(2)

#define EMD_PWR_KEY_LOW_TIME		(5000)
#define EMD_RST_LOW_TIME		(300)
#define MAX_CHK_RDY_TIMES		(50)

/* For ext MD Message, this is for user space deamon use */
enum {
	EXT_MD_MSG_READY = 0xF0A50000,
	EXT_MD_MSG_REQUEST_RST,
	EXT_MD_MSG_WAIT_DONE,
};

/******************************************************************************************
 *   Data structure definition region
 ******************************************************************************************/
typedef struct _emd_dev_client{
	struct kfifo		fifo;
	int			major_dev_id;
	int			sub_dev_id;
	int			user_num;
	spinlock_t		lock;
	wait_queue_head_t 	wait_q;
	struct wake_lock	emd_wake_lock;
	struct mutex		emd_mutex;
}emd_dev_client_t;

typedef struct _wakeup_ctl_info{
	int	dev_major_id;
	int	dev_sub_id;
	int	time_out_req_count;
	int	manual_req_count;
	char	*name;
}wakeup_ctl_info_t;

/******************************************************************************************
 *   Gloabal variables region
 ******************************************************************************************/ 
static struct cdev		*emd_chr_dev;
static emd_dev_client_t		drv_client[EMD_CLIENT_NUM];
static atomic_t			rst_on_going = ATOMIC_INIT(0);
static wakeup_ctl_info_t	wakeup_user[WAKEUP_EMD_USER_NUM]; // 0-USB, 1-UART
static unsigned int		ext_modem_is_ready=0;

static void emd_wakeup_timer_cb(unsigned long);
static DEFINE_TIMER(emd_wakeup_timer,emd_wakeup_timer_cb,0,0);
static unsigned int		timeout_pending_count = 0;
static spinlock_t		wakeup_ctl_lock;

static struct wake_lock		emd_wdt_wake_lock;	// Make sure WDT reset not to suspend
static void emd_aseert_log_work_func(struct work_struct *data);
static void emd_wakeup_work_func(struct work_struct *data);
static DECLARE_WAIT_QUEUE_HEAD(emd_aseert_log_wait);
static DEFINE_MUTEX(emd_aseert_log_lock);
static DECLARE_WORK(emd_aseert_log_work, emd_aseert_log_work_func);
static DECLARE_WORK(emd_wakeup_work, emd_wakeup_work_func);
static struct wake_lock emd_wakeup_wakelock;
static int emd_aseert_log_wait_timeout = 0;
static int power_on_md(void);
static int power_off_md(void);
static int ext_md_power_state = 0;
/*****************************************************************************************
 *   Log control section
 *****************************************************************************************/
#define EMD_MSG(fmt, args...)	printk("[ext-md-ctl]" fmt, ##args)
#define EMD_RAW			printk
/* Debug message switch */
#define EMD_DBG_NONE		(0x00000000)    /* No debug log */
#define EMD_DBG_FUNC		(0x00000001)    /* Function entry log */
#define EMD_DBG_PM		(0x00000002)
#define EMD_DBG_MISC		(0x00000004)
#define CCCI_DBG_ALL		(0xffffffff)
static unsigned int		emd_msg_mask = CCCI_DBG_ALL;
/*---------------------------------------------------------------------------*/
#define EMD_DBG_MSG(mask, fmt, args...) \
do {	\
	if ((EMD_DBG_##mask) & emd_msg_mask ) \
            printk("[ext-md-ctl]" fmt , ##args); \
} while(0)
/*---------------------------------------------------------------------------*/
#define EMD_FUNC_ENTER(f)		EMD_DBG_MSG(FUNC, "%s ++\n", __FUNCTION__)
#define EMD_PM_MSG(fmt, args...)	EMD_DBG_MSG(MISC, fmt, ##args)
#define EMD_FUNC_EXIT(f)		EMD_DBG_MSG(FUNC, "%s -- %d\n", __FUNCTION__, f)
#define EMD_MISC_MSG(fmt, args...)	EMD_DBG_MSG(MISC, fmt, ##args)

/******************************************************************************************
 *   External customization functions region
 ******************************************************************************************/
extern void cm_ext_md_rst(void);
extern void cm_gpio_setup(void);
extern void cm_hold_wakeup_md_signal(void);
extern void cm_release_wakeup_md_signal(void);
extern void cm_enable_ext_md_wdt_irq(void);
extern void cm_disable_ext_md_wdt_irq(void);
extern void cm_enable_ext_md_wakeup_irq(void);
extern void cm_disable_ext_md_wakeup_irq(void);
extern int  cm_do_md_power_on(void);
extern int  cm_do_md_power_off(void);
extern int  cm_do_md_reset(void);
extern int  cm_do_md_switch_r8_to_pc(void);
extern int  cm_do_md_switch_r8_to_ap(void);
extern int  cm_do_md_download_r8(void);
extern int  cm_do_md_go(void);
extern void cm_do_md_rst_and_hold(void);

/*=======================================================================*/
/* MTK External MD Ctl                                                   */
/*=======================================================================*/
#if (MTK_EXTERNAL_MODEM_SLOT_NUM==6280)
static struct platform_device emd_chr_devs[] = {

	{
		.name	= "ext-md-ctl",
		.id	= 0,
	},
	{
		.name	= "ext-md-ctl",
		.id	= 1,
	},
	{
		.name	= "ext-md-ctl",
		.id	= 2,
	},
};
#endif

/******************************************************************************************
 *   Helper functions region
 ******************************************************************************************/ 
static int request_ext_md_reset(void); // Function declaration
static int send_message_to_user(emd_dev_client_t *client, int msg);

static void ext_md_rst(void)
{
	EMD_FUNC_ENTER();
	cm_ext_md_rst();
	ext_modem_is_ready = 0;
}

void request_wakeup_md_timeout(unsigned int dev_id, unsigned int dev_sub_id)
{
	int i;
	unsigned long flags;

	for(i=0; i<WAKEUP_EMD_USER_NUM; i++){
		if( (wakeup_user[i].dev_major_id == dev_id)
		  &&(wakeup_user[i].dev_sub_id == dev_sub_id) ){
		 	spin_lock_irqsave(&wakeup_ctl_lock, flags);
			wakeup_user[i].time_out_req_count++;
			if(0 == timeout_pending_count){
				cm_hold_wakeup_md_signal();
				mod_timer(&emd_wakeup_timer, jiffies+2*HZ);
			}
			timeout_pending_count++;
			if(timeout_pending_count > 2)
				timeout_pending_count = 2;
			spin_unlock_irqrestore(&wakeup_ctl_lock, flags);
			break;
		}
	}
}

void request_wakeup_md(unsigned int dev_id, unsigned int dev_sub_id)
{
	int i;
	unsigned long flags;

	for(i=0; i<WAKEUP_EMD_USER_NUM; i++){
		if( (wakeup_user[i].dev_major_id == dev_id)
		  &&(wakeup_user[i].dev_sub_id == dev_sub_id) ){
		 	spin_lock_irqsave(&wakeup_ctl_lock, flags);
		 	if(wakeup_user[i].manual_req_count == 0)
		 		cm_hold_wakeup_md_signal();
			wakeup_user[i].manual_req_count++;
			spin_unlock_irqrestore(&wakeup_ctl_lock, flags);
			break;
		}
	}
}

void release_wakeup_md(unsigned int dev_id, unsigned int dev_sub_id)
{
	int i;
	unsigned long flags;

	for(i=0; i<WAKEUP_EMD_USER_NUM; i++){
		if( (wakeup_user[i].dev_major_id == dev_id)
		  &&(wakeup_user[i].dev_sub_id == dev_sub_id) ){
		 	if(wakeup_user[i].manual_req_count == 0){
		 		EMD_MSG("E: %s%d mis-match!\n", 
		 			wakeup_user[i].name, wakeup_user[i].dev_sub_id);
		 	}else{
		 		spin_lock_irqsave(&wakeup_ctl_lock, flags);
				wakeup_user[i].manual_req_count--;
				if(0 == timeout_pending_count){
					timeout_pending_count++;
					mod_timer(&emd_wakeup_timer, jiffies+HZ); // Let time to release
				}
				spin_unlock_irqrestore(&wakeup_ctl_lock, flags);
			}
			break;
		}
	}
}

void emd_wakeup_timer_cb(unsigned long data  __always_unused)
{
	int release_wakeup_confirm = 1;
	int i;
	unsigned long flags;

	EMD_FUNC_ENTER();
	spin_lock_irqsave(&wakeup_ctl_lock, flags);

	// 1. Clear timeout counter
	for(i=0; i<WAKEUP_EMD_USER_NUM; i++){
		if(wakeup_user[i].time_out_req_count > 0){
			wakeup_user[i].time_out_req_count=0;
		}
	}

	// 2. Update timeout pending counter
	if(timeout_pending_count == 0){
		EMD_MISC_MSG("timeout_pending_count == 0\n");
	}else{
		timeout_pending_count--;
	}

	// 3. Check whether need to release wakeup signal
	if(timeout_pending_count == 0){ // Need check whether to release
		for(i=0; i<WAKEUP_EMD_USER_NUM; i++){
			if(wakeup_user[i].manual_req_count > 0){
				release_wakeup_confirm = 0;
				break;
			}
		}
		if(release_wakeup_confirm){
			cm_release_wakeup_md_signal();
			EMD_FUNC_EXIT(1);
		}
	}else{ // Need to run once more
		mod_timer(&emd_wakeup_timer, jiffies+2*HZ);
		EMD_FUNC_EXIT(2);
	}

	EMD_FUNC_EXIT(0);
	spin_unlock_irqrestore(&wakeup_ctl_lock, flags);
}

static void reset_wakeup_control_logic(void)
{
	int i;
	unsigned long flags;

	spin_lock_irqsave(&wakeup_ctl_lock, flags);
	for(i=0; i<WAKEUP_EMD_USER_NUM; i++){
		wakeup_user[i].time_out_req_count=0;
		wakeup_user[i].manual_req_count = 0;
	}
	timeout_pending_count = 0;
	spin_unlock_irqrestore(&wakeup_ctl_lock, flags);
}

static void enable_ext_md_wakeup_irq(void)
{
	EMD_MISC_MSG("Enable Ext MD Wakeup!\n");
	cm_enable_ext_md_wakeup_irq();
}

static void disable_ext_md_wakeup_irq(void)
{
	EMD_MISC_MSG("Disable Ext MD Wakeup!\n");
	cm_disable_ext_md_wakeup_irq();
}
static void emd_aseert_log_work_func(struct work_struct *data)
{
#if defined (CONFIG_MTK_AEE_FEATURE) && defined (CONFIG_MT_ENG_BUILD)
#if 1
#define TEST_PHY_SIZE 0x10000

	char log[16]="6280 assert";
	int i;
	char *ptr;

    EMD_MSG("Ext MD ASSERT\n");
	emd_aseert_log_wait_timeout = 1;
    wake_up_interruptible(&emd_aseert_log_wait);
	memset(log, 0, sizeof(log));
	ptr = kmalloc(TEST_PHY_SIZE, GFP_KERNEL);
	if (ptr == NULL) {
        printk("ee kmalloc fail\n");
	}
	for (i = 0; i < TEST_PHY_SIZE; i++) {
		ptr[i] = (i % 26) + 'A';
	}
	aed_md_exception((int *)log, sizeof(log), (int *)ptr, TEST_PHY_SIZE, __FILE__);
	kfree(ptr);
#else
    EMD_MSG("Ext MD ASSERT\n");
	emd_aseert_log_wait_timeout = 1;
    wake_up_interruptible(&emd_aseert_log_wait);
    aee_kernel_exception("ext-md-ctl", "ext modem assert!");    
#endif    
#else
    EMD_MSG("Ext MD ASSERT -> RESET\n");
	emd_aseert_log_wait_timeout = 1;
    wake_up_interruptible(&emd_aseert_log_wait);
	power_off_md();
	msleep(1000);
	power_on_md();	
	mt_eint_unmask(CUST_EINT_MT6280_WD_NUM);	
#endif    
}
static void ext_md_wdt_irq_cb(void)
{
	EMD_MSG("Ext MD WDT rst gotten!\n");
    schedule_work(&emd_aseert_log_work);
}
static void emd_wakeup_work_func(struct work_struct *data)
{
    EMD_MSG("Ext MD wake up\n");
#if 1
    if(ext_md_power_state)
    {
      EMD_MSG("wake up\n");
      wake_lock(&emd_wakeup_wakelock);
      //kpd_pwrkey_pmic_handler(0x1);
      msleep(200); 
      //kpd_pwrkey_pmic_handler(0x0);
      msleep(2000); 
      wake_unlock(&emd_wakeup_wakelock);
    }
   	//mt_eint_unmask(CUST_EINT_MT6280_USB_WAKEUP_NUM);
#endif   	
}
static void ext_md_wakeup_irq_cb(void)
{
	EMD_MISC_MSG("Ext MD wake up request!\n");
	schedule_work(&emd_wakeup_work);
	//wake_lock_timeout(&drv_client[0].emd_wake_lock, 2*HZ);
	//disable_ext_md_wakeup_irq();
}

static int register_dev_node(const char *name, int major, int sub_id)
{
	char name_str[64];
	dev_t dev;
	struct class *emd_class = NULL;

	EMD_FUNC_ENTER();
	memset(name_str, 0, sizeof(name_str));
	dev = MKDEV(major,0) + sub_id;
	snprintf(name_str, 64, "%s%d", name, sub_id);

	emd_class = class_create(THIS_MODULE, name_str);
	device_create(emd_class, NULL, dev, NULL, name_str);

	EMD_FUNC_EXIT(0);

	return 0;
}

static int let_ext_md_go(void) 
{
	int ret;
	int retry;
	EMD_FUNC_ENTER();

	ret = send_message_to_user(&drv_client[0], EXT_MD_MSG_READY);
	atomic_set(&rst_on_going, 0);
	if( ret==0 ){
		//cm_enable_ext_md_wdt_irq();
		retry = cm_do_md_go();
		EMD_MSG("let_ext_md_go, retry:%d\n", retry);
		cm_enable_ext_md_wdt_irq();
		ext_modem_is_ready = 1;
	}else{
		EMD_MSG("let_ext_md_go fail, msg does not send\n");
		ret = -1;
	}

	EMD_FUNC_EXIT(0);

	return ret;
}

int request_ext_md_reset()
{
	int ret = 0;

	EMD_FUNC_ENTER();

	//disable_ext_md_wdt_irq(); //------------------------------------------------------
	if(atomic_inc_and_test(&rst_on_going) == 0){
		//ext_md_rst();
		ret = send_message_to_user(&drv_client[0], EXT_MD_MSG_REQUEST_RST);
		if(ret!=0){
			EMD_MSG("request_ext_md_reset fail, msg does not send\n");
			atomic_dec(&rst_on_going);
		}
	}else{
		EMD_MSG("reset is on-going\n");
	}

	EMD_FUNC_EXIT(0);

	return ret;
}

static int power_on_md_and_hold_it(void)
{
	EMD_FUNC_ENTER();
	cm_do_md_power_on();
	cm_do_md_rst_and_hold();
	EMD_FUNC_EXIT(0);

	return 0;
}

#define SIG_TEST 44
struct task_struct *t = NULL;
struct siginfo info;
static int power_on_md(void)
{
	EMD_FUNC_ENTER();
	EMD_MSG("%s, EXT_MD_IOCTL_POWER_ON, t=%s\n", __func__, (t==NULL)?"NULL":"T");
	if (t) {
		info.si_int = 1111;
		send_sig_info(SIG_TEST, &info, t);
	}
	cm_do_md_power_on();
	EMD_FUNC_EXIT(0);
	return 0;
}
static int power_off_md(void)
{
	EMD_FUNC_ENTER();
	EMD_MSG("%s, EXT_MD_IOCTL_POWER_OFF, t=%s\n", __func__, (t==NULL)?"NULL":"T");
	if (t) {
		info.si_int = 2222;
		send_sig_info(SIG_TEST, &info, t);
	}
	cm_do_md_power_off();
	EMD_FUNC_EXIT(0);
	return 0;
}
static int switch_r8_to_pc(void)
{
	EMD_FUNC_ENTER();
	cm_do_md_switch_r8_to_pc();
	EMD_FUNC_EXIT(0);
	return 0;
}
static int switch_r8_to_ap(void)
{
	EMD_FUNC_ENTER();
	cm_do_md_switch_r8_to_ap();
	EMD_FUNC_EXIT(0);
	return 0;
}
static int download_r8(void)
{
	EMD_FUNC_ENTER();
	cm_do_md_download_r8();
	EMD_FUNC_EXIT(0);
	return 0;
}

static int push_data(emd_dev_client_t *client, int data)
{
	int size, ret;

	EMD_FUNC_ENTER();

	if(kfifo_is_full(&client->fifo)){
		ret=-ENOMEM;
		EMD_MSG("sub_dev%d kfifo full\n",client->sub_dev_id);
	}else{
		size=kfifo_in(&client->fifo,&data,sizeof(int));
		WARN_ON(size!=sizeof(int));
		ret=sizeof(int);
	}
	EMD_FUNC_EXIT(0);

	return ret;
}

static int pop_data(emd_dev_client_t *client, int *buf)
{
	int ret = 0;

	EMD_FUNC_ENTER();

	if(!kfifo_is_empty(&client->fifo))
		ret = kfifo_out(&client->fifo, buf, sizeof(int));

	EMD_FUNC_EXIT(0);

	return ret;
}

int send_message_to_user(emd_dev_client_t *client, int msg)
{
	int ret = 0;
	unsigned long flags = 0;

	EMD_FUNC_ENTER();
	spin_lock_irqsave(&client->lock, flags);

	// 1. Push data to fifo
	ret = push_data(client, msg);

	// 2. Wake up read function
	if( sizeof(int) == ret ){
		wake_up_interruptible(&client->wait_q);
		ret = 0;
	}

	spin_unlock_irqrestore(&client->lock, flags);
	EMD_FUNC_EXIT(0);

	return ret;
}

static int client_init(emd_dev_client_t *client, int sub_id)
{
	int ret = 0;
	EMD_FUNC_ENTER();
	if( (sub_id >= EMD_CLIENT_NUM) || (sub_id < 0) ){
		EMD_FUNC_EXIT(1);
		return -1;
	}

	// 1. Clear client
	memset(client, 0, sizeof(emd_dev_client_t));

	// 2. Setting device id
	client->major_dev_id = EMD_CHR_DEV_MAJOR;
	client->sub_dev_id = sub_id;

	// 3. Init wait queue head, wake lock, spin loc and semaphore
	init_waitqueue_head(&client->wait_q);
	wake_lock_init(&client->emd_wake_lock, WAKE_LOCK_SUSPEND, EMD_DEV_NAME);
	spin_lock_init(&client->lock);
	mutex_init(&client->emd_mutex);

	// 4. Set user_num to zero
	client->user_num = 0;

	// 5. Alloc and init kfifo
	ret=kfifo_alloc(&client->fifo, EMD_MAX_MESSAGE_NUM*sizeof(int),GFP_ATOMIC);
	if (ret){
		EMD_MSG("kfifo alloc failed(ret=%d).\n",ret);
		wake_lock_destroy(&client->emd_wake_lock);
		EMD_FUNC_EXIT(2);
		return ret;
	}
	EMD_FUNC_EXIT(0);

	return 0;
}

static int client_deinit(emd_dev_client_t *client)
{
	EMD_FUNC_ENTER();
	kfifo_free(&client->fifo);
	wake_lock_destroy(&client->emd_wake_lock);
	EMD_FUNC_EXIT(0);
	return 0;
}

static void eint_setup(void)
{
	EMD_MSG("eint_setup\n");
	//--- Ext MD wdt irq -------
        mt_eint_registration(CUST_EINT_MT6280_WD_NUM,
          CUST_EINT_MT6280_WD_TYPE,
          ext_md_wdt_irq_cb,
          0);
	mt_eint_unmask(CUST_EINT_MT6280_WD_NUM);

        mt_eint_registration(CUST_EINT_MT6280_USB_WAKEUP_NUM,
          CUST_EINT_MT6280_USB_WAKEUP_TYPE,
          ext_md_wakeup_irq_cb,
          0);
#if 0
	//--- Ext MD wdt irq -------
	mt65xx_eint_mask(CUST_EINT_DT_EXT_MD_WDT_NUM);
	mt65xx_eint_set_sens(CUST_EINT_DT_EXT_MD_WDT_NUM, CUST_EINT_DT_EXT_MD_WDT_SENSITIVE);
	mt65xx_eint_set_polarity(CUST_EINT_DT_EXT_MD_WDT_NUM, CUST_EINT_DT_EXT_MD_WDT_POLARITY);
	mt65xx_eint_set_hw_debounce(CUST_EINT_DT_EXT_MD_WDT_NUM, CUST_EINT_DT_EXT_MD_WDT_DEBOUNCE_CN);
	mt65xx_eint_registration(CUST_EINT_DT_EXT_MD_WDT_NUM, 
				 CUST_EINT_DT_EXT_MD_WDT_DEBOUNCE_EN, 
				 CUST_EINT_DT_EXT_MD_WDT_POLARITY, 
				 ext_md_wdt_irq_cb, 
				 0);

	//--- Ext MD wake up irq ------------
	mt65xx_eint_set_sens(CUST_EINT_DT_EXT_MD_WK_UP_NUM, CUST_EINT_DT_EXT_MD_WK_UP_SENSITIVE);
	mt65xx_eint_set_polarity(CUST_EINT_DT_EXT_MD_WK_UP_NUM, CUST_EINT_DT_EXT_MD_WK_UP_POLARITY);
	mt65xx_eint_set_hw_debounce(CUST_EINT_DT_EXT_MD_WK_UP_NUM, CUST_EINT_DT_EXT_MD_WK_UP_DEBOUNCE_CN);
	mt65xx_eint_registration(CUST_EINT_DT_EXT_MD_WK_UP_NUM, 
				 CUST_EINT_DT_EXT_MD_WK_UP_DEBOUNCE_EN, 
				 CUST_EINT_DT_EXT_MD_WK_UP_POLARITY, 
				 ext_md_wakeup_irq_cb, 
				 0);
#endif	
}

extern char usb_h_acm_all_clear(void);
void check_drv_rdy_to_rst(void)
{
	int check_count = 0; // Max 10 seconds
	while(check_count<MAX_CHK_RDY_TIMES){
//		if(usb_h_acm_all_clear())
//			return;
		msleep(200);
		check_count++;
	}
	EMD_MSG("Wait drv rdy to rst timeout!!\n");
}

/******************************************************************************************
 *   Driver functions region
 ******************************************************************************************/ 
static int emd_dev_open(struct inode *inode, struct file *file)
{
	int index=iminor(inode);
	int ret=0;

	EMD_MSG("Open by %s sub_id:%d\n",current->comm,index);

	if( (index >= EMD_CLIENT_NUM) || (index < 0) ){
		EMD_MSG("Open func get invalid dev sub id\n");
		EMD_FUNC_EXIT(1);
		return -1;
	}

	mutex_lock(&drv_client[index].emd_mutex);
#if 0	
	if(drv_client[index].user_num > 0){
		EMD_MSG("Multi-Open not support!\n");
		mutex_unlock(&drv_client[index].emd_mutex);
		EMD_FUNC_EXIT(2);
		return -1;
	}
#endif	
	drv_client[index].user_num++;
	mutex_unlock(&drv_client[index].emd_mutex);

	file->private_data=&drv_client[index];	
	nonseekable_open(inode,file);

	EMD_FUNC_EXIT(0);
	return ret;
}

static int emd_dev_release(struct inode *inode, struct file *file)
{
	int ret=0;
	emd_dev_client_t *client=(emd_dev_client_t *)file->private_data;

	EMD_MSG("clint %d call release\n", client->sub_dev_id);

	// Release resource and check wether need trigger reset
	if(1 == client->sub_dev_id){ // Only ext_md_ctl1 has the ability of  trigger reset
		if( 0!=atomic_read(&rst_on_going) ){
			ext_md_rst();
			msleep(EMD_RST_LOW_TIME);
			check_drv_rdy_to_rst();
			EMD_MSG("send wait done message\n");
			ret = send_message_to_user(&drv_client[0], EXT_MD_MSG_WAIT_DONE);
			if( ret!=0 )
				EMD_MSG("send wait done message fail\n");
		}
	}

	mutex_lock(&client->emd_mutex);
	client->user_num--;
	mutex_unlock(&client->emd_mutex);
	EMD_FUNC_EXIT(0);

	return 0;
}

static ssize_t emd_dev_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	emd_dev_client_t *client=(emd_dev_client_t *)file->private_data;
	int ret=0,data;

	DEFINE_WAIT(wait);

	EMD_FUNC_ENTER();
	WARN_ON(client==NULL);

	ret=pop_data(client,&data);

	if( sizeof(int) == ret){
		EMD_MISC_MSG("client%d get data:%08x, ret=%d\n", client->sub_dev_id, data, ret);
		if( copy_to_user(buf,&data,sizeof(int)) ){
			EMD_MSG("copy_to_user fialed\n");
			ret= -EFAULT;
		}else{
			ret = sizeof(int);
		}
	}else{
		if (file->f_flags & O_NONBLOCK){
			ret=-EAGAIN;
		}else{
			prepare_to_wait(&client->wait_q,&wait,TASK_INTERRUPTIBLE);
			schedule();

			if (signal_pending(current)){
				EMD_MSG("Interrupted syscall.signal_pend=0x%llx\n",
					*(long long *)current->pending.signal.sig);
				ret=-EINTR;
			}
		}
	}

	finish_wait(&client->wait_q,&wait);
	EMD_FUNC_EXIT(0);

	return ret;
}

static long emd_dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0, val;
	emd_dev_client_t *client=(emd_dev_client_t *)file->private_data;

	EMD_FUNC_ENTER();
	mutex_lock(&client->emd_mutex);
	switch (cmd) {
	case EXT_MD_IOCTL_LET_MD_GO:
		if(1 == client->sub_dev_id){ // Only /dev/ext_md_ctl1 has the ablility of let modem go
			ret = let_ext_md_go();
		}else{
			EMD_MSG("sub dev%d call EXT_MD_IOCTL_LET_MD_GO is denied\n", client->sub_dev_id);
			ret = -EMD_ERR_ACCESS_DENY;
		}
		break;

	case EXT_MD_IOCTL_REQUEST_RESET:
		if(2 == client->sub_dev_id){ // Only /dev/ext_md_ctl1 has the ablility of request modem reset
			ret = request_ext_md_reset();
		}else{
			EMD_MSG("sub dev%d call EXT_MD_IOCTL_REQUEST_RESET is denied\n", client->sub_dev_id);
			ret = -EMD_ERR_ACCESS_DENY;
		}
		break;
	
	case EXT_MD_IOCTL_POWER_ON_HOLD:
		if(0 == client->sub_dev_id){ // Only /dev/ext_md_ctl0 has the ablility of power on modem
			ret = power_on_md_and_hold_it();
		}else{
			EMD_MSG("sub dev%d call EXT_MD_IOCTL_POWER_ON_HOLD is denied\n", client->sub_dev_id);
			ret = -EMD_ERR_ACCESS_DENY;
		}
		break;
	case EXT_MD_IOCTL_POWER_ON:
		if(0 == client->sub_dev_id){ // Only /dev/ext_md_ctl0 has the ablility of power on modem
			ret = power_on_md();
		}else{
			EMD_MSG("sub dev%d call EXT_MD_IOCTL_POWER_ON is denied\n", client->sub_dev_id);
			ret = -EMD_ERR_ACCESS_DENY;
		}
		break;
	case EXT_MD_IOCTL_POWER_OFF:
		if(0 == client->sub_dev_id){ // Only /dev/ext_md_ctl0 has the ablility of power on modem
			ret = power_off_md();
		}else{
			EMD_MSG("sub dev%d call EXT_MD_IOCTL_POWER_OFF is denied\n", client->sub_dev_id);
			ret = -EMD_ERR_ACCESS_DENY;
		}
		break;
	case EXT_MD_IOCTL_RESET:
		//if(0 == client->sub_dev_id){ // Only /dev/ext_md_ctl0 has the ablility of power on modem
			ret = power_off_md();
			msleep(1000);
			ret = power_on_md();
		//}else{
		//	EMD_MSG("sub dev%d call EXT_MD_IOCTL_RESET is denied\n", client->sub_dev_id);
		//	ret = -EMD_ERR_ACCESS_DENY;
		//}
		break;
	case EXT_MD_IOCTL_R8_TO_PC:
		//if(0 == client->sub_dev_id){ // Only /dev/ext_md_ctl0 has the ablility of power on modem
			ret = switch_r8_to_pc();
		//}else{
		//	EMD_MSG("sub dev%d call EXT_MD_IOCTL_R8_TO_PC is denied\n", client->sub_dev_id);
		//	ret = -EMD_ERR_ACCESS_DENY;
		//}
		break;
	case EXT_MD_IOCTL_R8_TO_AP:
		if(0 == client->sub_dev_id){ // Only /dev/ext_md_ctl0 has the ablility of power on modem
			ret = switch_r8_to_ap();
		}else{
			EMD_MSG("sub dev%d call EXT_MD_IOCTL_R8_TO_AP is denied\n", client->sub_dev_id);
			ret = -EMD_ERR_ACCESS_DENY;
		}
		break;
	case EXT_MD_IOCTL_R8_DOWNLOAD:
		if(0 == client->sub_dev_id){ // Only /dev/ext_md_ctl0 has the ablility of power on modem
			ret = power_off_md();
			msleep(1000);
			ret = download_r8();
		}else{
			EMD_MSG("sub dev%d call EXT_MD_IOCTL_R8_DOWNLOAD is denied\n", client->sub_dev_id);
			ret = -EMD_ERR_ACCESS_DENY;
		}
		break;
	case EXT_MD_IOCTL_R8_ASSERTLOG:
		mutex_unlock(&client->emd_mutex);
		mutex_lock(&emd_aseert_log_lock);
	    mt_set_gpio_mode(GPIO_6280_WD, GPIO_6280_WD_M_EINT);		
        mt_eint_unmask(CUST_EINT_MT6280_WD_NUM);	
		if(0 == client->sub_dev_id){ // Only /dev/ext_md_ctl0 has the ablility of power on modem
			EMD_MSG("sub dev%d call EXT_MD_IOCTL_R8_ASSERTLOG is enter\n", client->sub_dev_id);
            wait_event_interruptible(emd_aseert_log_wait, emd_aseert_log_wait_timeout);
			EMD_MSG("sub dev%d call EXT_MD_IOCTL_R8_ASSERTLOG is exit\n", client->sub_dev_id);
		}else{
			EMD_MSG("sub dev%d call EXT_MD_IOCTL_R8_ASSERTLOG is denied\n", client->sub_dev_id);
			ret = -EMD_ERR_ACCESS_DENY;
		}
		emd_aseert_log_wait_timeout = 0;
		mutex_unlock(&emd_aseert_log_lock);		
    	mutex_lock(&client->emd_mutex);	
		break;
	case EXT_MD_IOCTL_R8_ASSERTLOG_STATUS:
		if(0 == client->sub_dev_id){ // Only /dev/ext_md_ctl0 has the ablility of power on modem
		    mt_set_gpio_mode(GPIO_6280_WD, GPIO_6280_WD_M_GPIO);
		    //access_ok		    
		    val = mt_get_gpio_in(GPIO_6280_WD);
            if (copy_to_user((int *)arg, &val, sizeof(int)))
            {                 
              return -EACCES;             
            }		    
		    EMD_MSG("EXT_MD_IOCTL_R8_ASSERTLOG_STATUS %d\n", val);
		}else{
			EMD_MSG("EXT_MD_IOCTL_R8_ASSERTLOG_STATUS is denied\n");
			ret = -EMD_ERR_ACCESS_DENY;
		}
		break;
	case EXT_MD_IOCTL_R8_WAKEUPEN:
		if(0 == client->sub_dev_id){ // Only /dev/ext_md_ctl0 has the ablility of power on modem
            if (copy_from_user(&val, (int *)arg , sizeof(int)))
            {                 
              return -EACCES;             
            }	
            ext_md_power_state = val;
		    EMD_MSG("EXT_MD_IOCTL_R8_WAKEUPEN %d\n", val);
		}else{
			EMD_MSG("EXT_MD_IOCTL_R8_WAKEUPEN is denied\n");
			ret = -EMD_ERR_ACCESS_DENY;
		}
		break;
	default:
		ret = -EMD_ERR_UN_DEF_CMD;
		break;
	}
	mutex_unlock(&client->emd_mutex);
	EMD_FUNC_EXIT(0);

	return ret;
}

static int emd_ctl_drv_probe(struct platform_device *device)
{
	EMD_FUNC_ENTER();
	EMD_MISC_MSG("clint %d probe!!\n", device->id);

	if( (device->id < 0)||(device->id >= EMD_CLIENT_NUM) )
		EMD_MSG("invalid id%d!!\n", device->id);
	else{
		register_dev_node(EMD_DEV_NODE_NAME, EMD_CHR_DEV_MAJOR, device->id);
		if(0 == device->id){
			cm_gpio_setup();
			eint_setup();
		}
	}
	EMD_FUNC_EXIT(0);

	return 0;
}

static int emd_ctl_drv_remove(struct platform_device *dev)
{
	EMD_FUNC_ENTER();
	EMD_MISC_MSG("remove!!\n" );
	EMD_FUNC_EXIT(0);
	return 0;
}

static void emd_ctl_drv_shutdown(struct platform_device *dev)
{
	EMD_FUNC_ENTER();
	EMD_MISC_MSG("shutdown!!\n" );
	EMD_FUNC_EXIT(0);
}

static int emd_ctl_drv_suspend(struct platform_device *dev, pm_message_t state)
{
	EMD_FUNC_ENTER();
	EMD_PM_MSG("client:%d suspend!!\n", dev->id);
	if( (dev->id < 0)||(dev->id >= EMD_CLIENT_NUM) )
		EMD_MSG("invalid id%d!!\n", dev->id);
	else{
		if(0 == dev->id){
    	      EMD_PM_MSG("ext md enable wake up\n");
			  cm_release_wakeup_md_signal();
			  if(ext_md_power_state)
			  {
			    enable_ext_md_wakeup_irq();
			  }
			  del_timer(&emd_wakeup_timer);
			  reset_wakeup_control_logic();
		}
	}
	EMD_FUNC_EXIT(0);

	return 0;
}

static int emd_ctl_drv_resume(struct platform_device *dev)
{
	EMD_FUNC_ENTER();
	EMD_PM_MSG("client:%d resume!!\n", dev->id);
	if( (dev->id < 0)||(dev->id >= EMD_CLIENT_NUM) )
		EMD_MSG("invalid id%d!!\n", dev->id);
	else{
		if(0 == dev->id){
			disable_ext_md_wakeup_irq();
		}
	}
	EMD_FUNC_EXIT(0);

	return 0;
}


static struct file_operations emd_chr_dev_fops=
{
	.owner		= THIS_MODULE,
	.open		= emd_dev_open,
	.read		= emd_dev_read,
	.release	= emd_dev_release,
	.unlocked_ioctl	= emd_dev_ioctl,

};

struct dentry *file;
static ssize_t write_pid(struct file *file, const char __user *buf,
                                size_t count, loff_t *ppos)
{
	char mybuf[10];
	int pid = 0;
	int ret;
	//struct siginfo info;
	// struct task_struct *t;
	/* read the value from user space */
	if(count > 10)
		return -EINVAL;
	if (copy_from_user(mybuf, buf, count))
		return -EACCES;
	sscanf(mybuf, "%d", &pid);
	printk("pid = %d\n", pid);

	/* send the signal */
	memset(&info, 0, sizeof(struct siginfo));
	info.si_signo = SIG_TEST;
	info.si_code = SI_QUEUE;	// this is bit of a trickery: SI_QUEUE is normally used by sigqueue from user space,
					// and kernel space should use SI_KERNEL. But if SI_KERNEL is used the real_time data 
					// is not delivered to the user space signal handler function. 
	info.si_int = 1234;  		//real time signals may have 32 bits of data.

	rcu_read_lock();
	t = find_task_by_vpid(pid);
	//t = find_task_by_pid_type(PIDTYPE_PID, pid);  //find the task_struct associated with this pid
	if(t == NULL){
		printk("no such pid\n");
		rcu_read_unlock();
		return -ENODEV;
	}
	rcu_read_unlock();
	ret = send_sig_info(SIG_TEST, &info, t);    //send the signal
	if (ret < 0) {
		printk("error sending signal\n");
		return ret;
	}
	return count;
}

static const struct file_operations u3dg_fops = {
	.write = write_pid,
};

static struct platform_driver emd_ctl_driver =
{
	.driver     = {
	.name		= EMD_DEV_NAME,
		},
	.probe		= emd_ctl_drv_probe,
	.remove		= emd_ctl_drv_remove,
	.shutdown	= emd_ctl_drv_shutdown,
	.suspend	= emd_ctl_drv_suspend,
	.resume		= emd_ctl_drv_resume,
};

int __init emd_chrdev_init(void)
{
	int ret=0, i;

	EMD_FUNC_ENTER();
	EMD_MSG("ver: 20111111\n");

	/* 0. Init global ext-md-wdt wake lock */
	wake_lock_init(&emd_wdt_wake_lock, WAKE_LOCK_SUSPEND, "ext_md_wdt_wake_lock");
	wake_lock_init(&emd_wakeup_wakelock, WAKE_LOCK_SUSPEND, "emd_wakeup_wakelock");	

#if (MTK_EXTERNAL_MODEM_SLOT_NUM==6280)
    // Note: MUST init before USB and UART driver if this driver enable
    for (i = 0; i < ARRAY_SIZE(emd_chr_devs); i++){
        ret = platform_device_register(&emd_chr_devs[i]);
        if (ret != 0){
            printk("[emd] Regsiter emd_char_device %d failed\n", i);
            return ret;
        }
    }
#endif

	/* 1. Register device region. 0~1 */
	if (register_chrdev_region(MKDEV(EMD_CHR_DEV_MAJOR,0),EMD_CLIENT_NUM,EMD_DEV_NAME) != 0)
	{
		EMD_MSG("Regsiter EMD_CHR_DEV failed\n");
		ret=-1;
		goto register_region_fail;
	}

	/* 2. Alloc charactor devices */
	emd_chr_dev=cdev_alloc();
	if (NULL == emd_chr_dev)
	{
		EMD_MSG("cdev_alloc failed\n");
		ret=-1;
		goto cdev_alloc_fail;
	}

	/* 3. Initialize and add devices */
	cdev_init(emd_chr_dev,&emd_chr_dev_fops);
	emd_chr_dev->owner=THIS_MODULE;
	ret = cdev_add(emd_chr_dev,MKDEV(EMD_CHR_DEV_MAJOR,0),EMD_CLIENT_NUM);
	if (ret){
		EMD_MSG("cdev_add failed\n");
		goto cdev_add_fail;
	}

	/* 4. Register platform driver */
	ret = platform_driver_register(&emd_ctl_driver);
	if(ret){	
		EMD_MSG("platform_driver_register failed\n");
		goto platform_driver_register_fail;
	}

	/* 5. Init driver client */
	for(i=0; i<EMD_CLIENT_NUM; i++){
		if( 0!=client_init(&drv_client[i], i) ){
			EMD_MSG("driver client init failed\n");
			goto driver_client_init_fail;
		}
	}

	/* 6. Init wake up ctl structure */
	memset(wakeup_user, 0, sizeof(wakeup_user));
	wakeup_user[0].dev_major_id = DEVICE0_ID;
	wakeup_user[0].dev_sub_id = 0;
	wakeup_user[0].name = USER0_NAME;
	wakeup_user[1].dev_major_id = DEVICE1_ID;
	wakeup_user[1].dev_sub_id = 1;
	wakeup_user[1].name = USER1_NAME;
	spin_lock_init(&wakeup_ctl_lock);

	EMD_FUNC_EXIT(0);
	return 0;

driver_client_init_fail:
platform_driver_register_fail:

cdev_add_fail:
	cdev_del(emd_chr_dev);

cdev_alloc_fail:
	unregister_chrdev_region(MKDEV(EMD_CHR_DEV_MAJOR,0),EMD_CLIENT_NUM);

register_region_fail:
	EMD_FUNC_EXIT(1);
	return ret;
}

void __exit emd_chrdev_exit(void)
{
	EMD_FUNC_ENTER();
	unregister_chrdev_region(MKDEV(EMD_CHR_DEV_MAJOR,0),EMD_CLIENT_NUM);
	cdev_del(emd_chr_dev);
	client_deinit(&drv_client[0]);
	client_deinit(&drv_client[1]);
	wake_lock_destroy(&emd_wdt_wake_lock);
	wake_lock_destroy(&emd_wakeup_wakelock);	
	EMD_FUNC_EXIT(0);
}

module_init(emd_chrdev_init);
module_exit(emd_chrdev_exit);

static int __init signalexample_module_init(void)
{
	/* we need to know the pid of the user space process
 	 * -> we use debugfs for this. As soon as a pid is written to 
 	 * this file, a signal is sent to that pid
 	 */
	/* only root can write to this file (no read) */
	file = debugfs_create_file("signalconfpid", 0664, NULL, NULL, &u3dg_fops);
	return 0;
}
static void __exit signalexample_module_exit(void)
{
	debugfs_remove(file);

}

module_init(signalexample_module_init);
module_exit(signalexample_module_exit);
MODULE_AUTHOR("Chao Song");
MODULE_DESCRIPTION("Ext Modem Control Device Driver");
MODULE_LICENSE("GPL");
