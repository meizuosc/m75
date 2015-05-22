#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/sysfs.h>
#include <linux/ctype.h>
#include <linux/hwmon-sysfs.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/earlysuspend.h>
#include <linux/wakelock.h>
#include <mach/eint.h>
#include <mach/mt_gpio.h>
#include <mach/irqs.h>
#include <linux/input.h>
#include "cust_eint.h"
#include <linux/interrupt.h>
#include <asm/uaccess.h>
#include "pa12200002.h"
#include <linux/sensors_io.h>

static struct i2c_board_info i2c_txc[] = {
	{
	    I2C_BOARD_INFO("PA122", 0x1e)
	}
};

struct txc_data *txc_info = NULL;

// I2C read one byte data from register 
static int i2c_read_reg(struct i2c_client *client,u8 reg,u8 *data)
{
  	u8 databuf[2]; 
	int res = 0;
	databuf[0]= reg;
	
	res = i2c_master_send(client,databuf,0x1);
	if(res <= 0)
	{
		APS_ERR("i2c_master_send function err\n");
		return res;
	}
	res = i2c_master_recv(client,data,0x1);
	if(res <= 0)
	{
		APS_ERR("i2c_master_recv function err\n");
		return res;
	}
	return 0;
}
// I2C Write one byte data to register
static int i2c_write_reg(struct i2c_client *client,u8 reg,u8 value)
{
	u8 databuf[2];    
	int res = 0;
   
	databuf[0] = reg;   
	databuf[1] = value;
	
	res = i2c_master_send(client,databuf,0x2);

	if (res < 0){
		return res;
		APS_ERR("i2c_master_send function err\n");
	}
		return 0;
}
#if 0
static void set_ps_work_mode(struct i2c_client *client)
{
    int ret = 0;
    u8 data = 0;

    ret = i2c_read_reg(client, REG_CFG2, &data);
    ret = i2c_write_reg(client, REG_CFG2, data | PA12_INT_PS);
}
#endif

static int pa12201001_set_ps_mode(struct i2c_client *client)
{
  u8 sendvalue=0;
  int res = 0;
  
  if (!txc_info->mcu_enable) {
      sendvalue= PA12_LED_CURR100 | PA12_PS_PRST4;
      res=i2c_write_reg(client,REG_CFG1,sendvalue);

      sendvalue= PA12_PS_INT_WINDOW | PA12_PS_PERIOD12;
      res=i2c_write_reg(client,REG_CFG3,sendvalue);
  } else {
      sendvalue= PA12_LED_CURR50 | PA12_PS_PRST4;
      res=i2c_write_reg(client,REG_CFG1,sendvalue);
     /* when mcu enbaled, we should set the irq as edge type*/ 
      sendvalue= PA12_PS_INT_HYSTERESIS | PA12_PS_PERIOD12;
      res=i2c_write_reg(client,REG_CFG3,sendvalue);
  }

  res=i2c_write_reg(client,REG_PS_SET, 0x03); //PSET, Normal Mode

  if (txc_info->mcu_enable) {
     // Set PS threshold
      sendvalue=PA12_PS_FAR_TH_HIGH;	
      res=i2c_write_reg(client,REG_PS_TH,sendvalue); //set TH threshold
	    
      sendvalue=PA12_PS_NEAR_TH_LOW;	
      res=i2c_write_reg(client,REG_PS_TL,sendvalue); //set TL threshold
  } else {
       // Set PS threshold
      sendvalue=PA12_PS_FAR_TH_HIGH;	
      res=i2c_write_reg(client,REG_PS_TH,sendvalue); //set TH threshold
	    
      sendvalue=PA12_PS_FAR_TH_LOW;	
      res=i2c_write_reg(client,REG_PS_TL,sendvalue); //set TL threshold
 }

  // Interrupt Setting	 
  res=i2c_write_reg(client,REG_CFG2, (PA12_INT_PS | PA12_PS_MODE_NORMAL | PA12_PS_INTF_INACTIVE)); //set int mode
  
  res = i2c_write_reg(client, REG_PS_OFFSET, PA12_PS_OFFSET_DEFAULT);
  if(res < 0)
  {	
    APS_ERR("i2c_send function err\n");
    goto EXIT_ERR;
  }
  
    return 0 ;
	
EXIT_ERR:
      APS_ERR("pa12201001 init dev fail!!!!: %d\n", res);
      return res;
}

//PS enable function
int pa12201001_enable_ps(struct i2c_client *client, int enable)
{
  int res;
  u8 regdata=0;
  u8 sendvalue=0;
	
  if(enable == 1) //PS ON
  {
     printk("pa12201001 enable ps sensor\n");
     res=i2c_read_reg(client,REG_CFG0,&regdata); //Read Status
     if(res<0){
		APS_ERR("i2c_read function err\n");
		return res;
     }else{

     	sendvalue=regdata & 0xFD; //clear bit
     	sendvalue=sendvalue | 0x02; //0x02 PS Flag
     	res=i2c_write_reg(client,REG_CFG0,sendvalue); //Write PS enable 
     
    	 if(res<0){
		     APS_ERR("i2c_write function err\n");
		     return res;
          }	  		 	
         res=i2c_read_reg(client,REG_CFG0,&regdata); //Read Status
     	APS_LOG("CFG0 Status: %d\n",regdata);
      }
    }else{       //PS OFF
			
       APS_LOG("pa12201001 disaple ps sensor\n");
       res=i2c_read_reg(client,REG_CFG0,&regdata); //Read Status
       if(res<0){
		  APS_ERR("i2c_read function err\n");
		  return res;
       }else{
          APS_LOG("CFG0 Status: %d\n",regdata);
		
          sendvalue=regdata & 0xFD; //clear bit
		res=i2c_write_reg(client,REG_CFG0,sendvalue); //Write PS disable 
		
          if(res<0){
		      APS_ERR("i2c_write function err\n");
			 return res;
		 }	  	
       }
     }
	
     return 0;
} 

//Read PS Count : 8 bit
int pa12201001_read_ps(struct i2c_client *client, u8 *data)
{
   int res;
   u8 psdata = 0;
	
  // APS_FUN(f);
    res = i2c_read_reg(client,REG_PS_DATA,data); //Read PS Data
    //psdata = i2c_smbus_read_byte_data(client, REG_PS_DATA); 
   if(res < 0){
        APS_ERR("i2c_send function err\n");
   }

   return res;
}

static void txc_set_enable(struct txc_data *txc, int enable)
{
    struct i2c_client *client = txc->client;
    
       // set_ps_work_mode(client);
        pa12201001_enable_ps(client, enable);

        if (enable) {
	    pa12201001_set_ps_mode(client);
            schedule_delayed_work_on(0, &txc->ps_dwork, HZ/100);
        } else {
            txc->ps_data = PS_UNKONW;
            input_report_rel(txc->input_dev, REL_Z, txc->ps_data);
            input_sync(txc->input_dev);
    }
}

static ssize_t txc_ps_enable_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	int enabled;

	enabled = txc_info->ps_enable;

	return sprintf(buf, "%d\n", enabled);
}

static ssize_t txc_ps_enable_store(struct device *dev, struct device_attribute *attr, const char *buf,
	size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	int enable = simple_strtol(buf, NULL, 10);
    
    pa12201001_enable_ps(client, enable);
    if (enable) {
       // set_ps_work_mode(client);
	pa12201001_set_ps_mode(client);
        schedule_delayed_work_on(0, &txc_info->ps_dwork, HZ/100);
    } else {
        txc_info->ps_data = PS_UNKONW;
        input_report_rel(txc_info->input_dev, REL_Z, txc_info->ps_data);
        input_sync(txc_info->input_dev);
    }

	pr_info("%s: %d\n", __func__, count);

	return count;
}

static ssize_t txc_ps_data_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	int ret;
	u8 ps_data;
	    
	ret = i2c_read_reg(client, REG_PS_DATA, &ps_data); //Read PS Data

	printk("ps data is %d \n", ps_data);

	return sprintf(buf, "%d\n", ps_data);
}

static ssize_t pa12200001_show_reg(struct device *dev, struct device_attribute *attr,char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	u8 regdata;
	int res=0;
	int count=0;
	int i=0	;

	for(i;i <17 ;i++)
	{
		res=i2c_read_reg(client,0x00+i,&regdata);
		if(res<0)
		{
		   break;
		}
		else
		count+=sprintf(buf+count,"[%x] = (%x)\n",0x00+i,regdata);
	}
	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t pa12200001_store_send(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	int addr, cmd;


	if(2 != sscanf(buf, "%x %x", &addr, &cmd))
	{
		APS_ERR("invalid format: '%s'\n", buf);
		return 0;
	}

  i2c_write_reg(client, addr, cmd);
	//****************************
	return count;
}

static ssize_t txc_pstype_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", txc_info->pstype);
}

static ssize_t txc_pstype_store(struct device *dev, struct device_attribute *attr, const char *buf,
	size_t count)
{
	int pstype = simple_strtol(buf, NULL, 10);
    
	txc_info->pstype = pstype;
	if (!pstype) {
		txc_info->mcu_enable = false;
    	} else
		txc_info->mcu_enable = true;

	return count;
}

/* sysfs attributes operation function*/
static DEVICE_ATTR(ps_enable, 0664, txc_ps_enable_show, txc_ps_enable_store);
static DEVICE_ATTR(ps_data, S_IRUGO, txc_ps_data_show, NULL);
static DEVICE_ATTR(reg, 0777, pa12200001_show_reg, pa12200001_store_send);
static DEVICE_ATTR(pstype, 0777, txc_pstype_show, txc_pstype_store);

static struct attribute *txc_attributes[] = {
	&dev_attr_ps_enable.attr,
	&dev_attr_ps_data.attr,
	&dev_attr_reg.attr,
	&dev_attr_pstype.attr,
	NULL,
};

static struct attribute_group txc_attribute_group = {
	.attrs = txc_attributes,
};

/*
 * txc misc device file operation functions inplement
 */
static int txc_misc_open(struct inode *inode, struct file *file)
{
	struct txc_data *txc = container_of((struct miscdevice *)file->private_data,
							struct txc_data,
							misc_device);

	if (atomic_xchg(&txc->opened, 1)) {
		pr_err("%s()->%d:request txc private data error!\n",
			__func__, __LINE__);
		return -EBUSY;
	}

	file->private_data = txc;
	atomic_set(&txc->opened, 1);

	return 0;
}

static int txc_misc_close(struct inode *inode, struct file *file)
{
	struct txc_data *txc = file->private_data;

	atomic_set(&txc->opened, 0);

	return 0;
}

static long txc_misc_ioctl_int(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret, enable;
	struct txc_data *txc = (struct txc_data *)file->private_data;
	
	switch (cmd) {

	case TXC_IOCTL_SET_PS_ENABLE:
	    ret = copy_from_user(&txc->ps_enable, (void __user *)arg, sizeof(int));
	    if (ret) {
		    pr_err("%s()->%d:copy enable operation error!\n",
			    __func__, __LINE__);
		    return -EINVAL;
	    }
	    schedule_delayed_work_on(0, &txc->ioctl_enable_work, HZ/100);
	    break;

	case TXC_IOCTL_GET_PS_ENABLE:
	    ret = copy_to_user((void __user *)arg, &txc->ps_enable, sizeof(int));
	    if (ret) {
		    pr_err("%s()->%d:copy enable operation error!\n",
			    __func__, __LINE__);
		    return -EINVAL;
	    }
	    break;
	case ALSPS_SET_PS_MODE:
	case ALSPS_GET_PS_RAW_DATA:
	    break;
	default:
		return -EINVAL;
	}

	return 0;
}

static long txc_misc_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret;
	struct txc_data *txc = (struct txc_data *)file->private_data;
	mutex_lock(&txc->ioctl_lock);
	ret = txc_misc_ioctl_int(file, cmd, arg);
	mutex_unlock(&txc->ioctl_lock);
	return ret;
}
/*
 * txc misc device file operation
 * here just need to define open, release and ioctl functions.
 */
struct file_operations const txc_fops = {
	.owner = THIS_MODULE,
	.open = txc_misc_open,
	.release = txc_misc_close,
	.unlocked_ioctl = txc_misc_ioctl,
};

/*
 *  enable the sensor delayed work function
 */
static void txc_ioctl_enable_handler(struct work_struct *work)
{
	struct txc_data *txc = container_of((struct delayed_work*)work,
			struct txc_data, ioctl_enable_work);

	txc_set_enable(txc, txc->ps_enable);
}

static void txc_ps_handler(struct work_struct *work)
{
    struct txc_data *txc = container_of(work, struct txc_data, ps_dwork.work);
    struct i2c_client *client = txc_info->client;
    u8 psdata=0;
    int ps_data = txc->ps_data;
    u8 sendvalue;
    int res;
    u8 data;
    int ret;

    pa12201001_read_ps(client,&psdata);
    printk("%s, psdata is %d\n", __func__, psdata);

    if (txc->mcu_enable) {
	 if (psdata > PA12_PS_FAR_TH_HIGH) {
		ps_data = PS_NEAR;
	    } else if (psdata < PA12_PS_NEAR_TH_LOW) {
		ps_data = PS_FAR ;
	    }
    } else {
	  if (txc->ps_data == PS_UNKONW || txc->ps_data == PS_FAR) {
	    if(psdata > PA12_PS_FAR_TH_HIGH){
		ps_data = PS_NEAR;
	      // Set PS threshold
	      sendvalue=PA12_PS_NEAR_TH_HIGH;	
	      res=i2c_write_reg(client,REG_PS_TH,sendvalue); //set TH threshold
		
	      sendvalue=PA12_PS_NEAR_TH_LOW;	
	      res=i2c_write_reg(client,REG_PS_TL,sendvalue); //set TL threshold

	    } else if (psdata < PA12_PS_NEAR_TH_LOW) {
		ps_data= PS_FAR;
	    } 
	} else if (txc->ps_data == PS_NEAR) {
	    if(psdata < PA12_PS_NEAR_TH_LOW){
		ps_data = PS_FAR;

	      sendvalue=PA12_PS_FAR_TH_HIGH;	
	      res=i2c_write_reg(client,REG_PS_TH,sendvalue); //set TH threshold
		
	      sendvalue=PA12_PS_FAR_TH_LOW;	
	      res=i2c_write_reg(client,REG_PS_TL,sendvalue); //set TL threshold
	    }
	}

	if (txc->ps_data != ps_data) {
	    txc->ps_data = ps_data;
	    input_report_rel(txc->input_dev, REL_Z, ps_data);
	    input_sync(txc->input_dev);
	    if (ps_data == PS_NEAR) {
		printk("***********near***********\n");	
	    } else if (ps_data == PS_FAR) {
		printk("****************far***************\n");		
	    }
	}
	ret = i2c_read_reg(txc->client, REG_CFG2, &data);
	if (ret < 0) {
	    pr_err("%s: txc_read error\n", __func__);
	}
	data &= 0xfc;
	ret = i2c_write_reg(txc->client, REG_CFG2, data);
	if (ret < 0) {
	    pr_err("%s: txc_write error\n", __func__);
	}
   }
    mt_eint_unmask(CUST_EINT_INTI_INT_NUM);
}

static void txc_irq_handler(void)
{
	schedule_delayed_work(&txc_info->ps_dwork, 0);
}

static int txc_irq_init(struct txc_data *txc)
{
	int ret;
	int irq;

	mt_set_gpio_dir(GPIO_IR_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_mode(GPIO_IR_EINT_PIN, GPIO_MODE_00);
	mt_eint_set_sens(CUST_EINT_INTI_INT_NUM, MT_EDGE_SENSITIVE);
	mt_eint_registration(CUST_EINT_INTI_INT_NUM, EINTF_TRIGGER_FALLING, txc_irq_handler, 0);
	mt_eint_unmask(CUST_EINT_INTI_INT_NUM);

	return ret;
}

static int txc_create_input(struct txc_data *txc)
{
	int ret;
	struct input_dev *dev;

	dev = input_allocate_device();
	if (!dev) {
		pr_err("%s()->%d:can not alloc memory to txc input device!\n",
			__func__, __LINE__);
		return -ENOMEM;
	}

	set_bit(EV_REL, dev->evbit);
	input_set_capability(dev, EV_REL, REL_Z);
	input_set_abs_params(dev, REL_Z, 0, 1, 0, 0);  /*the max value 1bit*/
	dev->name = "pa122";
	dev->dev.parent = &txc->client->dev;

	ret = input_register_device(dev);
	if (ret < 0) {
		pr_err("%s()->%d:can not register txc input device!\n",
			__func__, __LINE__);
		input_free_device(dev);
		return ret;
	}

	txc->input_dev = dev;
	input_set_drvdata(txc->input_dev, txc);

	/* init the ps data is FAR*/
	txc->ps_data = PS_FAR;
	input_report_rel(dev, REL_Z, txc->ps_data);
	input_sync(dev);

	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void txc_early_suspend(struct early_suspend *h)
{
	if (txc_info->ps_enable) {
		enable_irq_wake(EINT_IRQ(CUST_EINT_INTI_INT_NUM));
		txc_info->irq_wake_enabled = 1;
	}
}

static void txc_late_resume(struct early_suspend *h)
{
	if (txc_info->irq_wake_enabled) {
		disable_irq_wake(EINT_IRQ(CUST_EINT_INTI_INT_NUM));
		txc_info->irq_wake_enabled = 0;
	}
}
#endif

static int txc_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret;
	struct txc_data *txc;

	/*request private data*/
	txc = kzalloc(sizeof(struct txc_data), GFP_KERNEL);
	if (!txc) {
		pr_err("%s()->%d:can not alloc memory to private data !\n",
			__func__, __LINE__);
		return -ENOMEM;
	}

	/*set client private data*/
	txc->client = client;
	i2c_set_clientdata(client, txc);
	txc_info = txc;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s()->%d:i2c adapter don't support i2c operation!\n",
			__func__, __LINE__);
		return -ENODEV;
	}
	mutex_init(&txc->ioctl_lock);
	atomic_set(&txc->opened, 0);
	txc->mcu_enable = false;

	/*create input device for reporting data*/
	ret = txc_create_input(txc);
	if (ret < 0) {
		pr_err("%s()->%d:can not create input device!\n",
			__func__, __LINE__);
	}
	txc->misc_device.minor = MISC_DYNAMIC_MINOR;
	txc->misc_device.name = "PA122";
	txc->misc_device.fops = &txc_fops;
	ret = misc_register(&txc->misc_device);
	if (ret < 0) {
		pr_err("%s()->%d:can not create misc device!\n",
			__func__, __LINE__);
	}

	ret = sysfs_create_group(&client->dev.kobj, &txc_attribute_group);
	if (ret < 0) {
		pr_err("%s()->%d:can not create sysfs group attributes!\n",
			__func__, __LINE__);
	}

	INIT_DELAYED_WORK(&txc->ps_dwork, txc_ps_handler);
	INIT_DELAYED_WORK(&txc->ioctl_enable_work, txc_ioctl_enable_handler);
	txc_irq_init(txc);

#ifdef CONFIG_HAS_EARLYSUSPEND
	txc->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	txc->early_suspend.suspend = txc_early_suspend;
	txc->early_suspend.resume = txc_late_resume;
	register_early_suspend(&txc->early_suspend);
#endif

	printk("%s: probe ok!!, client addr is 0x%02x\n", __func__, client->addr);
}

static const struct i2c_device_id txc_id[] = {
	{ "PA122", 0 },
	{},
};

static int txc_remove(struct i2c_client *client)
{
	struct txc_data *txc = i2c_get_clientdata(client);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&txc->early_suspend);
#endif
	sysfs_remove_group(&client->dev.kobj, &txc_attribute_group);
	misc_deregister(&txc->misc_device);
	input_unregister_device(txc->input_dev);
	input_free_device(txc->input_dev);
	kfree(txc);

	return 0;
}

static struct i2c_driver txc_driver = {
	.driver = {
		.name	= TXC_DEV_NAME,
		.owner	= THIS_MODULE,
	},
	.probe	= txc_probe,
	.remove = txc_remove,
	.id_table = txc_id,
};

static int __init txc_init(void)
{
	pr_info("%s:###########\n", __func__);

	i2c_register_board_info(3, i2c_txc, 1);
	return i2c_add_driver(&txc_driver);
}

static void __exit txc_exit(void)
{
	i2c_del_driver(&txc_driver);
}

module_init(txc_init);
module_exit(txc_exit);

