/*
 * bq24196 charger driver
 *
 * Copyright (C) 2014 xingyan tang <tngxingyan@meizu.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/param.h>
#include <linux/err.h>
#include <linux/workqueue.h>
#include <linux/sysfs.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/idr.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <mach/mt_boot_common.h>
#include <mach/system.h>
#include <mach/mt_sleep.h>
#include <mach/mt_typedefs.h>
#include <mach/charging.h>
#include <mach/eint.h>
#include <mach/mt_gpio.h>
#include <mach/irqs.h>
#include <mach/charging.h>
#include <linux/power/bq24196_charger.h>

kal_bool chargin_hw_init_done = KAL_FALSE; 

static struct bq24196_platform_data bq24196_data_info = 
{
	.ac_current_limit = 1300,
	.usb_current_limit = 500,
	.battery_regulation_voltage = 4520,
	.charge_voltage_limit = 4350,
	.charge_current = 1024,
	.termination_current = 128,
	.fast_charger_timer = BQ24196_FCHGTIME_8H, 
	.pre_current = 128,
	.bat_recharge_threshold = 100,
	.batlow_threshold = BQ24196_BATLOWV3_0,
};

static struct i2c_board_info i2c_devs_info[] =
{
    	{
		I2C_BOARD_INFO("bq24196-charger", 0x6B),
		.platform_data = &bq24196_data_info,
	}
};

extern bool mt_usb_is_device(void);
extern void Charger_Detect_Init(void);
extern void Charger_Detect_Release(void);
extern int hw_charging_get_charger_type(void);
extern void mt_power_off(void);
static struct bq24196_device *bq_data = NULL;
#define STATUS_OK 0
#define STATUS_UNSUPPORTED -1
#define GPIO_BQ_INT_NUM	 6

/* read value from register */
int bq24196_i2c_read(unsigned char cmd, unsigned char *returnData)
{
	char     cmd_buf = 0;
	char     readData = 0;
	int      ret=0;
	cmd_buf = cmd;

	mutex_lock(&bq_data->i2c_mutex);

	bq_data->client->addr = (bq_data->client->addr & I2C_MASK_FLAG) | I2C_WR_FLAG |I2C_RS_FLAG;
	ret = i2c_master_send(bq_data->client, &cmd_buf, (1<<8 | 1));
	if (ret < 0) 
	{
	    pr_err("send command error!!\n");
	    mutex_unlock(&bq_data->i2c_mutex);
	    return ret;
	}
	
	readData = cmd_buf;
	*returnData = readData;

	bq_data->client->addr = bq_data->client->addr & I2C_MASK_FLAG;

	mutex_unlock(&bq_data->i2c_mutex);    
	return 0;
}
EXPORT_SYMBOL_GPL(bq24196_i2c_read);

int bq24196_i2c_write(unsigned char cmd, unsigned char writeData)
{
	char write_data[2] = {0};
	int ret=0;

	mutex_lock(&bq_data->i2c_mutex);
	
	write_data[0] = cmd;
	write_data[1] = writeData;
	ret = i2c_master_send(bq_data->client, write_data, 2);
	if (ret < 0) 
	{
	    pr_err("send command error!!\n");
	    mutex_unlock(&bq_data->i2c_mutex);
	    return ret;
	}
	mutex_unlock(&bq_data->i2c_mutex);

	return 0;
}
EXPORT_SYMBOL_GPL(bq24196_i2c_write);

/* read value from register, apply mask and right shift it */
static int bq24196_i2c_read_mask(struct bq24196_device *bq, u8 reg,
				 u8 mask, u8 shift)
{
	int ret;
	unsigned char value;

	if (shift > 8)
		return -EINVAL;

	ret = bq24196_i2c_read(reg, &value);
	if (ret < 0)
		return ret;
	return (value & mask) >> shift;
}

static int bq24196_i2c_update_reg(struct bq24196_device *bq, u8 reg, u8 val, u8 mask)
{
	int ret;
	unsigned char value;

	ret = bq24196_i2c_read(reg, &value);
	if (ret < 0)
		return ret;

	value &= ~mask;
	value |= (val & mask);

	return bq24196_i2c_write(reg, value);
}

/* set current limit in mA */
static int bq24196_set_input_current(void *data)
{
	int val;
	kal_uint32 curr = *(kal_uint32 *)(data);

	if (curr <= 100)
		val = 0;
	else if (curr <= 500)
		val = 2;
	else if (curr <= 900)
		val = 3;
	else if (curr <= 1200)
	    	val = 4;
	else if (curr <= 1500)
	    	val = 5;
	else if (curr <= 2000)
	    	val = 6;
	else
		val = 7;

	return bq24196_i2c_update_reg(bq_data, BQ24196_REG_INPUT_CONTROL, val,
					BQ24196_INPUT_CURRENT_MASK);
}

/* get current limit in mA */
static int bq24196_get_input_current(struct bq24196_device *bq)
{
	int ret;

	ret = bq24196_i2c_read_mask(bq, BQ24196_REG_POWERON_CONF,
			BQ24196_INPUT_CURRENT_MASK, BQ24196_INPUT_CURRENT_SHIFT);
	if (ret < 0)
		return ret;
	else if (ret == 0)
		return 100;
	else if (ret == 1)
		return 150;
	else if (ret == 2)
		return 500;
	else if (ret == 3)
		return 900;
	else if (ret == 4)
	    	return 1200;
	else if (ret == 5)
	    	return 1500;
	else if (ret == 6)
	    	return 2000;
	else if (ret == 7)
	    	return 3000;
	return -EINVAL;
}

/* get battery regulation voltage in mV */
static int bq24196_get_battery_regulation_voltage(struct bq24196_device *bq)
{
	int ret = bq24196_i2c_read_mask(bq, BQ24196_REG_INPUT_CONTROL,
			BQ24196_INPUT_VOLTAGE_MASK, BQ24196_INPUT_VOLTAGE_SHIFT);

	if (ret < 0)
		return ret;

	return ((ret * 80) + 3880);
}

/* set charge current in mA*/
static void bq24196_set_charger_current(void *data)
{
	u8 reg_data = 0, mask = 0;
	int val;
	kal_uint32 curr = *(kal_uint32 *)(data);

	mask = BQ24196_CHARGE_CURRENT_LIMIT_MASK;
	/*the fast charge current offset is 64mA, adn the range is 512-2048mA*/
	val = (((curr - 64) / 64) + 1);

	bq24196_i2c_update_reg(bq_data, BQ24196_REG_CHARGE_CURRENT, reg_data << 2, mask);
}

/* get charge current in mA */
static int bq24196_get_charger_current(void)
{
	int ret;

	/* the fast charge current offset is 64mA */
	ret = bq24196_i2c_read_mask(bq_data, BQ24196_REG_CHARGE_CURRENT,
					BQ24196_CHARGE_CURRENT_LIMIT_MASK,
					BQ24196_CHARGE_CURRENT_LIMIT_SHIFT);
	if (ret < 0)
		return ret;

	/*return the uA*/
	return (ret * 64000);
}

/* set termination current 128mA */
static int bq24196_set_termination_current(struct bq24196_device *bq, int val)
{
    	u8 reg_data = 0, mask = 0;
	
	/* the offset is 128mA */
	reg_data = (val - 128) / 128;
	mask = BQ24196_TERMINATION_CURRENT_LIMIT_MASK;

	return bq24196_i2c_update_reg(bq, BQ24196_REG_PRE_TERMINATOR_CONTROL,
				reg_data, mask);
}

static void bq24196_get_charger_status(kal_uint32 *data)
{
    	int ret;

	ret = bq24196_i2c_read_mask(bq_data, BQ24196_REG_SYSTEM_STATUS,
			BQ24196_CHRG_STAT_MASK, BQ24196_CHRG_STAT_SHIFT);
	if (ret < 0)
		return ret;
	else if (ret == 0) /* 00: Not Charging */
		*data = POWER_SUPPLY_STATUS_NOT_CHARGING;
	else if ((ret == 1) || (ret == 2)) /* 01: Pre-charger, 10: fast charging*/
		*data = POWER_SUPPLY_STATUS_CHARGING;
	else if (ret == 3) /* 11: Charge termination done */
		*data = POWER_SUPPLY_STATUS_FULL;
	else
		*data = POWER_SUPPLY_STATUS_UNKNOWN;
}

static enum power_supply_property bq24196_charger_props[] = {
	POWER_SUPPLY_PROP_STATUS,
};

static enum power_supply_property bq24196_power_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static int bq24196_charger_get_property(struct power_supply *psy,
					     enum power_supply_property psp,
					     union power_supply_propval *val)
{
	struct bq24196_device *bq = container_of(psy, struct bq24196_device,
						 psy_charger);
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		ret = bq24196_i2c_read_mask(bq, BQ24196_REG_SYSTEM_STATUS,
				BQ24196_CHRG_STAT_MASK, BQ24196_CHRG_STAT_SHIFT);
		if (ret < 0)
			return ret;
		else if (ret == 0) /* 00: Not Charging */
			val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
		else if ((ret == 1) || (ret == 2)) /* 01: Pre-charger, 10: fast charging*/
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		else if (ret == 3) /* 11: Charge termination done */
			val->intval = POWER_SUPPLY_STATUS_FULL;
		else
			val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int bq24196_usb_get_property(struct power_supply *psy,
					     enum power_supply_property psp,
					     union power_supply_propval *val)
{
	struct bq24196_device *bq = container_of(psy, struct bq24196_device,
						 psy_usb);
	if (psp != POWER_SUPPLY_PROP_ONLINE)
	    	return -EINVAL;

	val->intval = bq->usb_on;

	return 0;
}

static int bq24196_ac_get_property(struct power_supply *psy,
					     enum power_supply_property psp,
					     union power_supply_propval *val)
{
	struct bq24196_device *bq = container_of(psy, struct bq24196_device,
						 psy_ac);

	if (psp != POWER_SUPPLY_PROP_ONLINE)
	    	return -EINVAL;

	val->intval = bq->ac_on;

	return 0;
}

static int bq24196_power_supply_init(struct bq24196_device *bq)
{
	int ret;

	bq->psy_charger.name = "battery";
	bq->psy_charger.type = POWER_SUPPLY_TYPE_BATTERY;
	bq->psy_charger.properties = bq24196_charger_props;
	bq->psy_charger.num_properties = ARRAY_SIZE(bq24196_charger_props);
	bq->psy_charger.get_property = bq24196_charger_get_property;
	ret = power_supply_register(bq->dev, &bq->psy_charger);
	if (ret) {
		return ret;
	}

	bq->psy_usb.name = "usb";
	bq->psy_usb.type = POWER_SUPPLY_TYPE_USB;
	bq->psy_usb.properties = bq24196_power_props;
	bq->psy_usb.num_properties = ARRAY_SIZE(bq24196_power_props);
	bq->psy_usb.get_property = bq24196_usb_get_property;
	ret = power_supply_register(bq->dev, &bq->psy_usb);
	if (ret) {
		return ret;
	}

	bq->psy_ac.name = "ac";
	bq->psy_ac.type = POWER_SUPPLY_TYPE_MAINS;
	bq->psy_ac.properties = bq24196_power_props;
	bq->psy_ac.num_properties = ARRAY_SIZE(bq24196_power_props);
	bq->psy_ac.get_property = bq24196_ac_get_property;
	ret = power_supply_register(bq->dev, &bq->psy_ac);
	if (ret) {
		return ret;
	}

	return 0;
}

static void bq24196_power_supply_exit(struct bq24196_device *bq)
{
	cancel_delayed_work_sync(&bq->irq_dwork);
	power_supply_unregister(&bq->psy_charger);
	power_supply_unregister(&bq->psy_ac);
	power_supply_unregister(&bq->psy_usb);
}

static int bq24196_charger_enable(void *data)
{
    	u8 val = 0, mask = 0;
	bool enable = *(int*)(data);
	int ret;

	mask = BQ24196_CHARGER_CONF_MASK;
	val = enable;

	/* Battery charging is enabled when REG01[5:4] = 01 and CE pin = LOW*/
	ret = bq24196_i2c_update_reg(bq_data, BQ24196_REG_POWERON_CONF, val << 4, mask);

	/* if enable charger set the CE pin LOW*/
	  if (enable)
	      mt_set_gpio_out(GPIO_SWCHARGER_EN_PIN, GPIO_OUT_ZERO);
	else
	      mt_set_gpio_out(GPIO_SWCHARGER_EN_PIN, GPIO_OUT_ONE);

	return ret;
}

static int bq24196_reverse_enable(void *data)
{
    	u8 val = 0, mask = 0;
	bool boost = *(int*)(data);
	mask = BQ24196_CHARGER_CONF_MASK;
	int ret;
	
	if (boost) {
		val = 2;
	} else 
		val = 0;	

	ret = bq24196_i2c_update_reg(bq_data, BQ24196_REG_POWERON_CONF, val << 4, mask);

	return ret;
}

static int bq24196_set_cv_voltage(void *data)
{
    	u8 reg = 0, mask = 0;
	int voltage = *(int*)(data);
	int ret;

	reg = ((voltage - 3504) / 16) << 2;
	mask = BQ24196_CHARGER_VOLTAGE_LIMIT_MASK;
	ret = bq24196_i2c_update_reg(bq_data , BQ24196_REG_VOLTAGE_CONTROL, reg, mask);
	
	return ret;
}

static void bq24196_reg_init(struct bq24196_device *bq)
{
    	int ret;
	u8 reg_data = 0, mask = 0;
	u8 reg1 = 0, reg2 = 0, reg3 = 0;

	/* set the input voltage limit(3.88-5.08V)*/
	reg_data = 0x9;
	mask = BQ24196_INPUT_VOLTAGE_MASK;
	bq24196_i2c_update_reg(bq, BQ24196_REG_INPUT_CONTROL, reg_data << 3, mask);

	/* set the minimum system voltage limit */
	reg_data = mask =0;
	reg_data = 0x5; /* 3.5V*/
	mask = 0x7 << 1;
	bq24196_i2c_update_reg(bq, BQ24196_REG_POWERON_CONF, reg_data << 1, mask);

	/* set the boost mode current limit 500mA*/
	reg_data = mask =0;
	bq24196_i2c_update_reg(bq, BQ24196_REG_POWERON_CONF, reg_data, mask);

	/*set the pre-charge current limit 256mA*/
	reg_data = mask = 0;
	reg_data = bq->init_data.pre_current / 128;
	mask = BQ24196_PRECHARGE_CURRENT_LIMIT_MASK;
	bq24196_i2c_update_reg(bq, BQ24196_REG_PRE_TERMINATOR_CONTROL, reg_data << 4, mask);

	/* set the Termination current limit 128mA */
	bq24196_set_termination_current(bq, bq->init_data.termination_current);

	/* set the charger voltage limit 4.35V */
	/* set the battery precharger to fast charger threshold 3.0V*/
	/* set the battery recharger threshold */
	mask = 0;
	reg1 = ((bq->init_data.charge_voltage_limit - 3504) / 16) << 2;
	reg2 = bq->init_data.batlow_threshold << 1;
	reg3 = bq->init_data.bat_recharge_threshold << 0;
	mask = BQ24196_CHARGER_VOLTAGE_LIMIT_MASK | BQ24196_BATLOWV_THRESHOLD_MASK
	    		| BQ24196_RECHARGE_THRESHOLD_MASK;
	bq24196_i2c_update_reg(bq, BQ24196_REG_VOLTAGE_CONTROL, reg1 | reg2 | reg3, mask);

	/* chargeing termination enable */
	reg_data = mask = 0;
	reg_data = 0x1 << 7;
	mask = 0x1 << 7;
	bq24196_i2c_update_reg(bq, BQ24196_REG_TIMER_CONTROL, reg_data, mask);

	/* enable charging safety timer and set the fast charging time is 8Hours*/
	reg1 = 0x1 << 3;
	reg2 =  BQ24196_FCHGTIME_8H << 1;
	mask = BQ24196_CHARGER_SAFETY_TIMER_MASK | BQ24196_FAST_CHARGE_TIMER_MASK;
	bq24196_i2c_update_reg(bq, BQ24196_REG_TIMER_CONTROL, reg1 | reg2, mask);
}

static kal_uint32 charging_hw_init(void *data)
{
	kal_uint32 status = STATUS_OK;

	printk("nothing need todo now\n");
	return status;
}

static kal_uint32 charging_dump_register(void *data)
{
	kal_uint32 status = STATUS_OK;
	int i = 0, ret;
	unsigned char val;

	for (i = 0; i <= 10; i++){
		ret = bq24196_i2c_read(i, &val);
		printk("0x%02x  0x%02x\n", i, val);
   	 }
	return status;
}	

static kal_uint32 charging_enable(void *data)
{
	kal_uint32 status = STATUS_OK;

	status = bq24196_charger_enable(data);

	return status;
}

static kal_uint32 charging_set_cv_voltage(void *data)
{
    	kal_uint32 status = STATUS_OK;

    	bq24196_set_cv_voltage(data);

    	return status;
}

static kal_uint32 charging_get_current(void *data)
{
    	kal_uint32 status = STATUS_OK;
  	
	status = bq24196_get_charger_current();
   
    	return status;
}  

static kal_uint32 charging_set_current(void *data)
{
    	kal_uint32 status = STATUS_OK;
  	
	bq24196_set_charger_current(data);
   
    	return status;
}  

static kal_uint32 charging_set_input_current(void *data)
{
    	kal_uint32 status = STATUS_OK;
  	
	status = bq24196_set_input_current(data);
   
    	return status;
}  

static kal_uint32 charging_get_charging_status(void *data)
{
    	kal_uint32 status = STATUS_OK;
  	
	bq24196_get_charger_status(&status);
   
    	return status;
}

static kal_uint32 charging_reset_watch_dog_timer(void *data)
{
    	kal_uint32 status = STATUS_OK;
  	int ret;
	u8 reg = 0, mask =0;

	reg = 1 << 6;
	mask = BQ24196_WATCHDOG_RESET_MASK;
   	ret = bq24196_i2c_update_reg(bq_data, BQ24196_REG_POWERON_CONF, reg, mask);

    	return ret;
}

static kal_uint32 charging_set_hv_threshold(void *data)
{
	static kal_uint32 status = STATUS_OK;

	printk("%s: nothing todo now\n", __func__);
	return status;
}

static kal_uint32 charging_get_hv_status(void *data)
{
	static kal_uint32 status = STATUS_OK;

	return status;
}

static kal_uint32 charging_get_battery_status(void *data)
{
	static kal_uint32 status = STATUS_OK;

//	bq24196_get_battery_status();
	return status;
}

static kal_uint32 charging_get_charger_det_status(void *data)
{
	static kal_uint32 status = STATUS_OK;

//	bq24196_get_charger_det_status();
	return status;
}

static kal_uint32 charging_get_charger_type(void *data)
{
	static kal_uint32 status = STATUS_OK;
	
	status = hw_charging_get_charger_type();

	return status;
}

static kal_uint32 charging_get_is_pcm_timer_trigger(void *data)
{
	static kal_uint32 status = STATUS_OK;

	if(slp_get_wake_reason() == WR_PCM_TIMER)
	    *(kal_bool*)(data) = KAL_TRUE;
	else
	    *(kal_bool*)(data) = KAL_FALSE;

	return status;
}

static kal_uint32 charging_set_platform_reset(void *data)
{
	kal_uint32 status = STATUS_OK;

	arch_reset(0, NULL);

	return status;
}

static kal_uint32 charging_get_platfrom_boot_mode(void *data)
{
    	kal_uint32 status = STATUS_OK;

	*(kal_uint32*)(data) = get_boot_mode();

	return status;
}

static kal_uint32 charging_set_power_off(void *data)
{
	kal_uint32 status = STATUS_OK;

	mt_power_off();

	return status;
}

static kal_uint32 (* const charging_func[CHARGING_CMD_NUMBER])(void *data)=
 {
 	  charging_hw_init
	,charging_dump_register  	
	,charging_enable
	,charging_set_cv_voltage
	,charging_get_current
	,charging_set_current
	,charging_set_input_current
	,charging_get_charging_status
	,charging_reset_watch_dog_timer
	,charging_set_hv_threshold
	,charging_get_hv_status
	,charging_get_battery_status
	,charging_get_charger_det_status
	,charging_get_charger_type
	,charging_get_is_pcm_timer_trigger
	,charging_set_platform_reset
	,charging_get_platfrom_boot_mode
	,charging_set_power_off
 };
 
kal_int32 chr_control_interface(CHARGING_CTRL_CMD cmd, void *data)
{
	 kal_int32 status;
	 if(cmd < CHARGING_CMD_NUMBER)
		 status = charging_func[cmd](data);
	 else
		 return STATUS_UNSUPPORTED;

	 return status;
}

static void bq24196_dwork(struct work_struct *work)
{
    	struct bq24196_device *bq = container_of(work, struct bq24196_device,
				irq_dwork.work);
	u8 fault_status, chrg_fault, bat_fault, system_status, otg;
	int ret;
	
	ret = bq24196_i2c_read(BQ24196_REG_FAULT_STATUS, &fault_status);
	ret = bq24196_i2c_read(BQ24196_REG_SYSTEM_STATUS, &system_status); 
	chrg_fault = fault_status & 0x30;
	bat_fault = fault_status & 0x08;
	otg = system_status & 0xc0;

	if (otg == 0xc0) {
		bq_data->host_insert = true;
		printk("otg connect\n");
		return;
    	} else if (bq_data->host_insert) {
	    	bq_data->host_insert == false;
		printk("otg disconnect\n");
		return;
    	} 

	switch (chrg_fault) {
	case CHRG_NORMAL:
	    break;
	case INPUT_FAULT:
	    printk("the VBUS OVP or VBUS is less 3.8V\n");
	    break;
	case THERMAL_SHUTDOWN:
	    break;
	case CHRG_TIMER_EXPIRATION:
	    printk("the charger safety timer is expired, should disable the regulation\n");
	    break;
	default:
	    break;
    	}

	if (bat_fault == 0) {
		printk("It is Normal\n");
    	} else if (bat_fault == 1) {
		printk("BATOVP\n");
    	}
}

static void bq24196_irq_handler(void)
{
    printk("Enter %s:********************\n", __func__);
    
    schedule_delayed_work_on(0, &bq_data->irq_dwork, 0);
}

static void bq24196_irq_init(void)
{
	int ret;

	/*unmask CHRG_FAULT and BAT_FAULT */
	ret = bq24196_i2c_update_reg(bq_data, BQ24196_REG_MISC_CONTROL, 0x3, 0x3);

        mt_set_gpio_dir(GPIO_BQ_INT_PIN, GPIO_DIR_IN);
        mt_set_gpio_mode(GPIO_BQ_INT_PIN, GPIO_MODE_00);
        mt_eint_set_sens(GPIO_BQ_INT_NUM, MT_LEVEL_SENSITIVE);

        mt_eint_registration(GPIO_BQ_INT_NUM, EINTF_TRIGGER_LOW, bq24196_irq_handler, 0);
        mt_eint_unmask(GPIO_BQ_INT_NUM);
}

static int bq24196_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	int ret;
	struct bq24196_device *bq;

	if (!client->dev.platform_data) {
		dev_err(&client->dev, "platform data not set\n");
		return -ENODEV;
	}

	bq = devm_kzalloc(&client->dev, sizeof(*bq), GFP_KERNEL);
	if (!bq) {
		dev_err(&client->dev, "failed to allocate device data\n");
		ret = -ENOMEM;
		goto error_1;
	}

	i2c_set_clientdata(client, bq);

	bq->dev = &client->dev;
	bq_data = bq;
	bq->client = client;

	memcpy(&bq->init_data, client->dev.platform_data,
			sizeof(bq->init_data));

	mutex_init(&bq->i2c_mutex);

	ret = bq24196_power_supply_init(bq);
	if (ret) {
		dev_err(bq->dev, "failed to register power supply: %d\n", ret);
		goto error_1;
	}

	bq24196_reg_init(bq);

	INIT_DELAYED_WORK(&bq->irq_dwork, bq24196_dwork);

	bq24196_irq_init();

	dev_info(bq->dev, "driver registered\n");
	return 0;

error_1:
	kfree(bq);

	return ret;
}

static int bq24196_remove(struct i2c_client *client)
{
	struct bq24196_device *bq = i2c_get_clientdata(client);

	bq24196_power_supply_exit(bq);
	kfree(bq);

	return 0;
}

static const struct i2c_device_id bq24196_i2c_id_table[] = {
	{ "bq24196-charger", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, bq24196_i2c_id_table);

static struct i2c_driver bq24196_driver = {
	.driver = {
		.name = "bq24196-charger",
	},
	.probe = bq24196_probe,
	.remove = bq24196_remove,
	.id_table = bq24196_i2c_id_table,
};
module_i2c_driver(bq24196_driver);

static int __init bq24196_init(void)
{
	i2c_register_board_info(1, i2c_devs_info, 1);

	return i2c_add_driver(&bq24196_driver);
}

static void __exit bq24196_exit(void)
{
	i2c_del_driver(&bq24196_driver);
}

module_init(bq24196_init);
module_exit(bq24196_exit);

MODULE_AUTHOR("xingyan tang <tangxingyan@meizu.com>");
MODULE_DESCRIPTION("Ti bq24196 charger driver");
MODULE_LICENSE("GPL");
