#include <linux/batch.h>

static DEFINE_MUTEX(batch_data_mutex);
static struct batch_context *batch_context_obj = NULL;


static void batch_early_suspend(struct early_suspend *h);
static void batch_late_resume(struct early_suspend *h);

static int register_eint_unmask(void)
{
	return 0;
}
static int batch_update_polling_rate(void)
{
	struct batch_context *obj = batch_context_obj; 
	int idx, mindelay;
	
	mindelay = obj->dev_list.data_dev[0].delay;
	
	for(idx = 0; idx < MAX_ANDROID_SENSOR_NUM; idx++)//choose first MAX_ANDROID_SENSOR_NUM sensors for different sensor type 
	{
		if((obj->dev_list.data_dev[idx].delay < mindelay) && (obj->dev_list.data_dev[idx].delay!=0))
		{
			mindelay = obj->dev_list.data_dev[idx].delay;	
		}
	}
	BATCH_LOG("get polling rate min value (%d) !\n", mindelay);
	return mindelay;
}


static void batch_work_func(struct work_struct *work)
{
	struct batch_context *obj = batch_context_obj;
	hwm_sensor_data *sensor_data = NULL;	//
	int idx, err;

	if((obj->dev_list.ctl_dev[MAX_ANDROID_SENSOR_NUM].flush != NULL) && (obj->dev_list.data_dev[MAX_ANDROID_SENSOR_NUM].get_data != NULL))
	{
		for(idx = 0; idx < MAX_ANDROID_SENSOR_NUM; idx++)
		{
			if((obj->active_sensor & (0x01<< idx))){
				do{
					err = obj->dev_list.data_dev[MAX_ANDROID_SENSOR_NUM].get_data(idx, sensor_data);
					if(err == 0)
						report_batch_data(obj->idev, sensor_data);
				}while(err == 0);//0:read one data success, not 0 data finsih
			}
		}
	}
	
	for(idx = 0; idx < MAX_ANDROID_SENSOR_NUM; idx++)
	{
		BATCH_LOG("get data from sensor (%d) !\n", idx);
		if((obj->dev_list.ctl_dev[idx].flush == NULL) || (obj->dev_list.data_dev[idx].get_data == NULL))
		{
			continue;
		}
		
		if((obj->active_sensor & (0x01<< idx))){
			do{
				err = obj->dev_list.data_dev[idx].get_data(idx, sensor_data);
				if(err == 0)
					report_batch_data(obj->idev, sensor_data);
			}while(err == 0);
		}
	}

	if(obj->is_polling_run){
		mod_timer(&obj->timer, jiffies + atomic_read(&obj->delay)/(1000/HZ));
	}
}

static void batch_poll(unsigned long data)
{
	struct batch_context *obj = (struct batch_context *)data;
	if(obj != NULL)
	{
		schedule_work(&obj->report);
	}
}

static void report_data_once(int handle)
{
	struct batch_context *obj = batch_context_obj;
	hwm_sensor_data *sensor_data = NULL;	
	int err;

	if((obj->dev_list.ctl_dev[MAX_ANDROID_SENSOR_NUM].flush != NULL) && (obj->dev_list.data_dev[MAX_ANDROID_SENSOR_NUM].get_data != NULL))
	{
		if((obj->active_sensor & (0x01<< handle))){
			do{//need mutex against store active and report data once
				err = obj->dev_list.data_dev[MAX_ANDROID_SENSOR_NUM].get_data(handle, sensor_data);
				if(err == 0)
					report_batch_data(obj->idev, sensor_data);
			}while(err == 0);
			report_batch_finish(obj->idev, handle);
			obj->flush_result = obj->dev_list.ctl_dev[MAX_ANDROID_SENSOR_NUM].flush(handle);
			return;
		}else{
			BATCH_LOG("batch mode is not enabled for this sensor!\n");
			obj->flush_result= -1;
			return;
		}
	}
	
	if((obj->active_sensor & (0x01<< handle))){
		if((obj->dev_list.ctl_dev[handle].flush != NULL) && (obj->dev_list.data_dev[handle].get_data != NULL))
		{
			do{
				err = obj->dev_list.data_dev[handle].get_data(handle, sensor_data);
				if(err == 0)
					report_batch_data(obj->idev, sensor_data);
			}while(err == 0);
			report_batch_finish(obj->idev, handle);
			obj->flush_result = obj->dev_list.ctl_dev[handle].flush(handle);
			return;
		}else{
			BATCH_LOG("batch mode is not enabled for this sensor!\n");
			obj->flush_result = -1;
			return;
		}
	}
	obj->flush_result = 0;
}

static struct batch_context *batch_context_alloc_object(void)
{
	
	struct batch_context *obj = kzalloc(sizeof(*obj), GFP_KERNEL); 
    	BATCH_LOG("batch_context_alloc_object++++\n");
	if(!obj)
	{
		BATCH_ERR("Alloc batch object error!\n");
		return NULL;
	}	
	atomic_set(&obj->delay, 200); /*5Hz*/// set work queue delay time 200ms
	atomic_set(&obj->wake, 0);
	INIT_WORK(&obj->report, batch_work_func);
	init_timer(&obj->timer);
	obj->timer.expires	= jiffies + atomic_read(&obj->delay)/(1000/HZ);
	obj->timer.function	= batch_poll;
	obj->timer.data = (unsigned long)obj;
	obj->is_first_data_after_enable = false;
	obj->is_polling_run = false;
	obj->active_sensor = 0;
	mutex_init(&obj->batch_op_mutex);
	BATCH_LOG("batch_context_alloc_object----\n");
	return obj;
}

static ssize_t batch_store_active(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
	BATCH_LOG(" batch_store_active not support now\n");
    	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t batch_show_active(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	int len = 0;
	struct batch_context *cxt = NULL;
	cxt = batch_context_obj;
	//display now enabling sensors of batch mode
	BATCH_LOG("batch vender_div value: %d\n", len);
	return snprintf(buf, PAGE_SIZE, "%d\n", len); 
}

static ssize_t batch_store_delay(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
	BATCH_LOG(" batch_store_delay not support now\n");
    	return count;
}

static ssize_t batch_show_delay(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
    	int len = 0;
	BATCH_LOG(" batch_show_delay not support now\n");
	return len;
}

static ssize_t batch_store_batch(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
	int handle, flags, samplingPeriodNs, maxBatchReportLatencyNs;
	int res, delay;
	struct batch_context *cxt = NULL;
	cxt = batch_context_obj;
	BATCH_LOG("write value: buf = %s\n", buf);
	if((res = sscanf(buf, "%d,%d,%d,%d", &handle, &flags, &samplingPeriodNs, &maxBatchReportLatencyNs))!=4)
	{
		BATCH_ERR(" batch_store_delay param error: res = %d\n", res);
	}
	BATCH_LOG(" batch_store_delay param: handle %d, flag:%d samplingPeriodNs:%d, maxBatchReportLatencyNs: %d\n",handle, flags, samplingPeriodNs, maxBatchReportLatencyNs);	

	if(flags & SENSORS_BATCH_DRY_RUN )
	{
		if(cxt->dev_list.data_dev[handle].is_batch_supported != 0){
			cxt->batch_result = 0;
		}else{
			cxt->batch_result = -1;
		}	
		//return count;
	}else if(flags & SENSORS_BATCH_WAKE_UPON_FIFO_FULL){
		register_eint_unmask();
	}
	
	if(maxBatchReportLatencyNs == 0){
		cxt->active_sensor = cxt->active_sensor & (~(0x01 << handle));//every active_sensor bit stands for a sensor type, bit = 0 stands for batch disabled
		if(cxt->active_sensor == 0){
			cxt->is_polling_run = false;
			del_timer_sync(&cxt->timer);
			cancel_work_sync(&cxt->report);
		}else{
			cxt->dev_list.data_dev[handle].delay = 0;
			delay = batch_update_polling_rate();
			atomic_set(&cxt->delay, delay);
			mod_timer(&cxt->timer, jiffies + atomic_read(&cxt->delay)/(1000/HZ));
			cxt->is_polling_run = true;
		}
	}else if(maxBatchReportLatencyNs != 0){
		cxt->active_sensor = cxt->active_sensor |(0x01 << handle);
		cxt->dev_list.data_dev[handle].delay = (int)maxBatchReportLatencyNs/1000/1000;
		delay = batch_update_polling_rate();
		atomic_set(&cxt->delay, delay);
		mod_timer(&cxt->timer, jiffies + atomic_read(&cxt->delay)/(1000/HZ));
		cxt->is_polling_run = true;
	}
	
	BATCH_ERR(" active_sensor param 0x%x\n", cxt->active_sensor);
	
	cxt->batch_result = 0;
	
	return count;
}

static ssize_t batch_show_batch(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	struct batch_context *cxt = NULL;
	int res = 0;
	cxt = batch_context_obj;
	res = cxt->batch_result;
	BATCH_LOG(" batch_show_delay batch result: %d\n", res);
	return snprintf(buf, PAGE_SIZE, "%d\n", res);
}

static ssize_t batch_store_flush(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
	struct batch_context *cxt = NULL;
	int handle;
	cxt = batch_context_obj;
	
    	if (1 != sscanf(buf, "%d", &handle)) {
        	BATCH_ERR("invalid format!!\n");
        	return count;
    	}

	report_data_once(handle);//handle need to use of this function 
	
	BATCH_LOG(" batch_store_delay sucess\n");
    	return count;
}

static ssize_t batch_show_flush(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	struct batch_context *cxt = NULL;
	int res = 0;
	cxt = batch_context_obj;
	res = cxt->flush_result;
	BATCH_LOG(" batch_show_flush flush result: %d\n", res);
	return snprintf(buf, PAGE_SIZE, "%d\n", res);

}

static ssize_t batch_show_devnum(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	struct batch_context *cxt = NULL;
	const char *devname = NULL;
	cxt = batch_context_obj;
	devname = dev_name(&cxt->idev->dev);
	return snprintf(buf, PAGE_SIZE, "%s\n", devname+5);
}

static int batch_misc_init(struct batch_context *cxt)
{

    int err=0;
    cxt->mdev.minor = MISC_DYNAMIC_MINOR;
	cxt->mdev.name  = BATCH_MISC_DEV_NAME;
	if((err = misc_register(&cxt->mdev)))
	{
		BATCH_ERR("unable to register batch misc device!!\n");
	}
	return err;
}

static void batch_input_destroy(struct batch_context *cxt)
{
	struct input_dev *dev = cxt->idev;

	input_unregister_device(dev);
	input_free_device(dev);
}

static int batch_input_init(struct batch_context *cxt)
{
	struct input_dev *dev;
	int err = 0;

	dev = input_allocate_device();
	if (NULL == dev)
		return -ENOMEM;

	dev->name = BATCH_INPUTDEV_NAME;

	input_set_capability(dev, EV_ABS, EVENT_TYPE_BATCH_X);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_BATCH_Y);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_BATCH_Z);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_SENSORTYPE);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_BATCH_VALUE);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_BATCH_STATUS);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_END_FLAG);
	
	input_set_abs_params(dev, EVENT_TYPE_BATCH_X, BATCH_VALUE_MIN, BATCH_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_BATCH_Y, BATCH_VALUE_MIN, BATCH_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_BATCH_Z, BATCH_VALUE_MIN, BATCH_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_BATCH_STATUS, BATCH_STATUS_MIN, BATCH_STATUS_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_BATCH_VALUE, BATCH_VALUE_MIN, BATCH_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_SENSORTYPE, BATCH_TYPE_MIN, BATCH_TYPE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_END_FLAG, BATCH_STATUS_MIN, BATCH_STATUS_MAX, 0, 0);
	input_set_drvdata(dev, cxt);

	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		return err;
	}
	cxt->idev= dev;

	return 0;
}


DEVICE_ATTR(batchactive,     S_IWUSR | S_IRUGO, batch_show_active, batch_store_active);
DEVICE_ATTR(batchdelay,      S_IWUSR | S_IRUGO, batch_show_delay,  batch_store_delay);
DEVICE_ATTR(batchbatch,      S_IWUSR | S_IRUGO, batch_show_batch,  batch_store_batch);
DEVICE_ATTR(batchflush,      S_IWUSR | S_IRUGO, batch_show_flush,  batch_store_flush);
DEVICE_ATTR(batchdevnum,      S_IWUSR | S_IRUGO, batch_show_devnum,  NULL);


static struct attribute *batch_attributes[] = {
	&dev_attr_batchactive.attr,
	&dev_attr_batchdelay.attr,
	&dev_attr_batchbatch.attr,
	&dev_attr_batchflush.attr,
	&dev_attr_batchdevnum.attr,
	NULL
};

static struct attribute_group batch_attribute_group = {
	.attrs = batch_attributes
};

int batch_register_data_path(int handle, struct batch_data_path *data)
{
	struct batch_context *cxt = NULL;
	//int err =0;
	cxt = batch_context_obj;
	if(data == NULL){
		BATCH_ERR("data pointer is null!\n");
		return -1;
		}
	if(handle >= 0 && handle <=(MAX_ANDROID_SENSOR_NUM)){
		cxt ->dev_list.data_dev[handle].get_data = data->get_data;
		cxt ->dev_list.data_dev[handle].flags = data->flags;
		cxt ->dev_list.data_dev[handle].is_batch_supported = data->is_batch_supported;
		return 0;
		}
	return -1;
}

int batch_register_control_path(int handle, struct batch_control_path *ctl)
{
	struct batch_context *cxt = NULL;
	cxt = batch_context_obj;
	if(ctl == NULL){
		BATCH_ERR("ctl pointer is null!\n");
		return -1;
		}
	if(handle >= 0 && handle <=(MAX_ANDROID_SENSOR_NUM)){
		cxt ->dev_list.ctl_dev[handle].flush= ctl->flush;
		return 0;	
		}
	return -1;
}

int batch_register_support_info(int handle, int support)
{
	struct batch_context *cxt = NULL;
	//int err =0;
	cxt = batch_context_obj;
	if(cxt == NULL){
		BATCH_ERR("cxt pointer is null!\n");
		return -1;
		}
	if(handle >= 0 && handle <=(MAX_ANDROID_SENSOR_NUM)){
		cxt ->dev_list.data_dev[handle].is_batch_supported = support;
		return 0;
	}
	return -1;
}

void report_batch_data(struct input_dev *dev, hwm_sensor_data *data)
{	
	hwm_sensor_data report_data;

	memcpy(&report_data, data, sizeof(hwm_sensor_data)); 

	if(report_data.sensor == SENSOR_TYPE_ACCELEROMETER
	||report_data.sensor == SENSOR_TYPE_MAGNETIC_FIELD
	||report_data.sensor == SENSOR_TYPE_ORIENTATION
	||report_data.sensor == SENSOR_TYPE_GYROSCOPE){
		input_report_abs(dev, EVENT_TYPE_SENSORTYPE, report_data.sensor );
		input_report_abs(dev, EVENT_TYPE_BATCH_X, report_data.values[0]);
		input_report_abs(dev, EVENT_TYPE_BATCH_Y, report_data.values[1]);
		input_report_abs(dev, EVENT_TYPE_BATCH_Z, report_data.values[2]);
		input_report_abs(dev, EVENT_TYPE_BATCH_STATUS, report_data.status);
		input_sync(dev); 
	}else{
		input_report_abs(dev, EVENT_TYPE_SENSORTYPE, report_data.sensor );
		input_report_abs(dev, EVENT_TYPE_BATCH_VALUE, report_data.values[0]);
		input_report_abs(dev, EVENT_TYPE_BATCH_STATUS, report_data.status);
		input_sync(dev); 
	}
}

void report_batch_finish(struct input_dev *dev, int sensor)
{
	input_report_abs(dev, EVENT_TYPE_END_FLAG, sensor);
	input_sync(dev); 
}


static int batch_probe(struct platform_device *pdev) 
{

	int err;
	BATCH_LOG("+++++++++++++batch_probe!!\n");

	batch_context_obj = batch_context_alloc_object();
	if (!batch_context_obj)
	{
		err = -ENOMEM;
		BATCH_ERR("unable to allocate devobj!\n");
		goto exit_alloc_data_failed;
	}

	//init input dev
	err = batch_input_init(batch_context_obj);
	if(err)
	{
		BATCH_ERR("unable to register batch input device!\n");
		goto exit_alloc_input_dev_failed;
	}

    	atomic_set(&(batch_context_obj->early_suspend), 0);
	batch_context_obj->early_drv.level    = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 1,
	batch_context_obj->early_drv.suspend  = batch_early_suspend,
	batch_context_obj->early_drv.resume   = batch_late_resume,    
	register_early_suspend(&batch_context_obj->early_drv);

	wake_lock_init(&(batch_context_obj->read_data_wake_lock),WAKE_LOCK_SUSPEND,"read_data_wake_lock");

	//add misc dev for sensor hal control cmd
	err = batch_misc_init(batch_context_obj);
	if(err)
	{
	   BATCH_ERR("unable to register batch misc device!!\n");
	   goto exit_err_sysfs;
	}
	err = sysfs_create_group(&batch_context_obj->mdev.this_device->kobj,
			&batch_attribute_group);
	if (err < 0)
	{
	   BATCH_ERR("unable to create batch attribute file\n");
	   goto exit_misc_register_failed;
	}
	
	kobject_uevent(&batch_context_obj->mdev.this_device->kobj, KOBJ_ADD);

	BATCH_LOG("----batch_probe OK !!\n");
	return 0;

	//exit_hwmsen_create_attr_failed:
	exit_misc_register_failed:    
	if((err = misc_deregister(&batch_context_obj->mdev)))
	{
		BATCH_ERR("misc_deregister fail: %d\n", err);
	}
	
	exit_err_sysfs:
	
	if (err)
	{
	   BATCH_ERR("sysfs node creation error \n");
	   batch_input_destroy(batch_context_obj);
	}
	
	//real_driver_init_fail:
	exit_alloc_input_dev_failed:    
	kfree(batch_context_obj);
	
	exit_alloc_data_failed:
	

	BATCH_LOG("----batch_probe fail !!!\n");
	return err;
}



static int batch_remove(struct platform_device *pdev)
{
	int err=0;
	BATCH_FUN(f);
	input_unregister_device(batch_context_obj->idev);        
	sysfs_remove_group(&batch_context_obj->idev->dev.kobj,
				&batch_attribute_group);
	
	if((err = misc_deregister(&batch_context_obj->mdev)))
	{
		BATCH_ERR("misc_deregister fail: %d\n", err);
	}
	kfree(batch_context_obj);

	return 0;
}

static void batch_early_suspend(struct early_suspend *h) 
{
   atomic_set(&(batch_context_obj->early_suspend), 1);
   BATCH_LOG(" batch_early_suspend ok------->hwm_obj->early_suspend=%d \n",atomic_read(&(batch_context_obj->early_suspend)));
   return ;
}
/*----------------------------------------------------------------------------*/
static void batch_late_resume(struct early_suspend *h)
{
   atomic_set(&(batch_context_obj->early_suspend), 0);
   BATCH_LOG(" batch_late_resume ok------->hwm_obj->early_suspend=%d \n",atomic_read(&(batch_context_obj->early_suspend)));
   return ;
}

static int batch_suspend(struct platform_device *dev, pm_message_t state) 
{
	return 0;
}
/*----------------------------------------------------------------------------*/
static int batch_resume(struct platform_device *dev)
{
	return 0;
}

static struct platform_driver batch_driver =
{
	.probe      = batch_probe,
	.remove     = batch_remove,    
	.suspend    = batch_suspend,
	.resume     = batch_resume,
	.driver     = 
	{
		.name = BATCH_PL_DEV_NAME,
	}
};

static int __init batch_init(void) 
{
	BATCH_FUN();

	if(platform_driver_register(&batch_driver))
	{
		BATCH_ERR("failed to register batch driver\n");
		return -ENODEV;
	}
	
	return 0;
}

static void __exit batch_exit(void)
{
	platform_driver_unregister(&batch_driver);      
}

module_init(batch_init);
module_exit(batch_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("batch device driver");
MODULE_AUTHOR("Mediatek");

