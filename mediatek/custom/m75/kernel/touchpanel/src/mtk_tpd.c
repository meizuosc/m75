
/*< XASP-360 linghai 20120626 begin */
#include <linux/interrupt.h>
#include <cust_eint.h>
#include <linux/i2c.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/rtpm_prio.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include "cust_gpio_usage.h"
#include "tpd.h"



#include <linux/gpio.h>

#include <mach/mt_pm_ldo.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_boot.h>


static int hw_irq_disable(void);
static DECLARE_WAIT_QUEUE_HEAD(waiter);

static int exitThread = 0 ;
/* Function extern */
static int tp_device_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tp_device_detect(struct i2c_client *client,  struct i2c_board_info *info);
static int tp_device_remove(struct i2c_client *client);

static int tpd_flag = 0 ;



static int touch_event_handler(void *arg)
{       
 
	struct sched_param param = { .sched_priority = RTPM_PRIO_TPD };
	struct tp_driver_data * data = (struct tp_driver_data*) arg ;

	if(!data)
		return -ENODEV ;
	
	sched_setscheduler(current, SCHED_RR, &param);

	do
	{	
	
	set_current_state(TASK_INTERRUPTIBLE);
	wait_event_interruptible(waiter, tpd_flag != 0);
	tpd_flag = 0 ;
	set_current_state(TASK_RUNNING);
	if(data->out.report_func&& !exitThread){
		dbg_printk("++++:%pf\n",data->out.report_func);
		data->out.report_func(data->out.driver_data) ;
		dbg_printk("----\n");
	}
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
   }while (!exitThread);		
	
   dbg_printk("!!!!!!!!!!!!!!kthread exit!!!!!!!!!!!!\n"); 
  return 0;
}

static void tpd_eint_handler(void)
{
	tpd_flag = 1 ;
	dbg_printk("+++\n");
	wake_up_interruptible(&waiter);
}


static void hw_register_irq(void)
{
	
	mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, EINTF_TRIGGER_LOW, tpd_eint_handler, 0);
	mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
	
	return ;
}

static int hw_power_onoff(int on)
{
	int retval = -1 ;
	info_printk("power devices %d\n",on);
	
	if(on){
		
     retval = mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	 retval |=mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
	 msleep(20);
	 retval|=hwPowerOn(MT6331_POWER_LDO_VMCH,  VOL_3300, "TPD2.8");
	 retval |= hwPowerOn(MT6331_POWER_LDO_VGP1 , VOL_1800, "TPD1.8");
	 msleep(20);
	 retval = mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	 retval |=mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	}else {
		retval	= hwPowerDown(MT6331_POWER_LDO_VMCH,"TPD2.8") ;
		retval |= hwPowerDown(MT6331_POWER_LDO_VGP1,"TPD1.8") ;
	}
	
	return retval ;
}

static int hw_get_gpio_value(int index)
{
	
	switch(index){
	case SDA_GPIO_INDX :
		return mt_get_gpio_in(GPIO_I2C2_SDA_PIN);
	case SCL_GPIO_INDX :
		return mt_get_gpio_in(GPIO_I2C2_SCA_PIN);
	case IRQ_GPIO_INDX :
		return mt_get_gpio_in(GPIO_CTP_EINT_PIN);
	case RST_GPIO_INDX :
		return mt_get_gpio_out(GPIO_CTP_RST_PIN);
    case IR_GPIO_INDX :
		return mt_get_gpio_in(GPIO_IR_EINT_PIN);
	default:
		return -1 ;
	}
	return -1 ;
}

static int hw_set_gpio_value(int index,int value)
{
	int gpio_value = GPIO_OUT_ZERO ;
	
	switch(index){
	case RST_GPIO_INDX :
		 if(value)
		 	gpio_value = GPIO_OUT_ONE ;
		return mt_set_gpio_out(GPIO_CTP_RST_PIN,gpio_value);
	default:
		return -1 ;
	}
	return -1 ;
}


static void hw_reset_devices(void)
{

	int retval = -1 ;
	info_printk("\n");
	retval  = mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
	retval |= mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	retval |=mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	msleep(20);
	retval |= mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
	msleep(10);
	retval |=mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	msleep(10);
    if(retval<0){
		info_printk("hw reset device error \n");
    }

	return  ;
}

  static int hw_irq_disable(void)
{

	int retval = -1 ;

	info_printk("TPD enter sleep start\n");
	mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
	
	/* Set EINT PIN to Input*/
	retval = mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_GPIO);
	retval |= mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);

	return retval;
}
  static int  hw_irq_enable(void)
 {
	int retval = -1 ;
	/* Recovery EINT Mode */
	retval = mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
	retval |=mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
	retval |=mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_ENABLE);
	retval |=mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_UP);
	
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);	
	
    info_printk("TPD wake up\n"); 	
    return retval;

 }

 static int lancher_thread(struct tp_driver_data *data,int on)
 {
 	int retval = -1 ;
	if(on){
    data->thread = kthread_run(touch_event_handler, (void*)data, TPD_DEVICE);
	if (IS_ERR(data->thread)){
		retval = PTR_ERR(data->thread);
		info_printk("failed to create kernel thread: %d\n", retval);		
	}
  }else{
		exitThread = 1 ;
		tpd_eint_handler();
  }
    
	return 0 ;
}

	
struct tp_driver_data g_data ={
 .in={
	.hw_reset          = hw_reset_devices ,
    .hw_power_onoff    = hw_power_onoff,
    .hw_register_irq   = hw_register_irq,
	.hw_irq_enable	   = hw_irq_enable,
	.hw_irq_disable    = hw_irq_disable ,
	.hw_get_gpio_value = hw_get_gpio_value,
	.hw_set_gpio_value = hw_set_gpio_value,
	.lancher_thread    = lancher_thread ,
  },
 .probed    = 0,
 .bootmode  = 0,      		
};



void* meizu_get_tp_hw_data(void)
{
	return (void*)&g_data ;
}

