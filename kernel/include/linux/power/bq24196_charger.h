#ifndef BQ24196_CHARGER_H
#define BQ24196_CHARGER_H

#define BQ24196_REG_INPUT_CONTROL		0x00
#define BQ24196_REG_POWERON_CONF	0x01
#define BQ24196_REG_CHARGE_CURRENT	0x02
#define BQ24196_REG_PRE_TERMINATOR_CONTROL	0x03
#define BQ24196_REG_VOLTAGE_CONTROL		0x04
#define BQ24196_REG_TIMER_CONTROL	0x05
#define BQ24196_REG_MISC_CONTROL	0x07
#define BQ24196_REG_SYSTEM_STATUS	0x08
#define BQ24196_REG_FAULT_STATUS	0x09

/* REG00: Input Source Control */
#define BQ24196_INPUT_VOLTAGE_SHIFT	3
#define BQ24196_INPUT_VOLTAGE_MASK	(15 << 3)
#define BQ24196_INPUT_CURRENT_SHIFT	0
#define BQ24196_INPUT_CURRENT_MASK	(7 << 0)

/* REG01：Power-On Configuration */
#define BQ24196_CHARGER_CONF_SHIFT	4
#define BQ24196_CHARGER_CONF_MASK	(3 << 4)
#define BQ24196_WATCHDOG_RESET_SHIFT	6
#define BQ24196_WATCHDOG_RESET_MASK	(1 << 6)

/*REG02: Charge Current Control Register */
#define BQ24196_CHARGE_CURRENT_LIMIT_SHIFT	2
#define BQ24196_CHARGE_CURRENT_LIMIT_MASK	(63 << 2)

/* REG 03: Pre-Charge/Terminator Current Control Register */
#define BQ24196_PRECHARGE_CURRENT_LIMIT_SHIFT	4
#define BQ24196_PRECHARGE_CURRENT_LIMIT_MASK	(15 << 4)
#define BQ24196_TERMINATION_CURRENT_LIMIT_SHIFT	0
#define BQ24196_TERMINATION_CURRENT_LIMIT_MASK	(15 << 0)

/* REG 04: Charger voltage congtrol */
#define BQ24196_CHARGER_VOLTAGE_LIMIT_MASK	(0x3f << 2)
#define BQ24196_BATLOWV_THRESHOLD_MASK	(0x1 << 1)
#define BQ24196_RECHARGE_THRESHOLD_MASK	(0x1 << 0)

/* REG 05: Charge termination/timer control */
#define BQ24196_CHARGER_SAFETY_TIMER_MASK	(1 << 3)
#define BQ24196_FAST_CHARGE_TIMER_MASK	(3 << 1)

/* REG 08: System status Register*/
#define BQ24196_CHRG_STAT_SHIFT	4
#define BQ24196_CHRG_STAT_MASK	(3 << 4)

enum bq24196_charger_fchgtime
{
    BQ24196_FCHGTIME_5H,
    BQ24196_FCHGTIME_8H,
    BQ24196_FCHGTIME_12H,
    BQ24196_FCHGTIME_20H,
};

enum bq24196_batlow_threshold
{
	BQ24196_BATLOWV2_8,
	BQ24196_BATLOWV3_0,
};

enum BQ24196_recharge_threshold
{
	BQ24196_VRECHG100,
	BQ24196_VRECHG300,
};

enum CHRG_FAULT_STATUS {
	CHRG_NORMAL,
	INPUT_FAULT,
	THERMAL_SHUTDOWN,
	CHRG_TIMER_EXPIRATION,
};

struct bq24196_platform_data {
	int ac_current_limit;		/* mA */
	int usb_current_limit;
	int battery_regulation_voltage;	/* mV */
	int charge_voltage_limit;
	int charge_current;		/* mA */
	int termination_current;	/* mA */
	int pre_current;
	int fast_charger_timer;
	int bat_recharge_threshold;
	int batlow_threshold;
};

struct bq24196_device {
	struct device *dev;
	struct i2c_client *client;
	struct bq24196_platform_data init_data;
	struct mutex i2c_mutex;
	struct power_supply psy_usb;
	struct power_supply psy_charger;
	struct power_supply psy_ac;
	struct delayed_work irq_dwork;
	int usb_on;
	int ac_on;
	bool host_insert;
};

extern int bq24196_i2c_read(unsigned char cmd, unsigned char *returnData);
extern int bq24196_i2c_write(unsigned char cmd, unsigned char writeData);

#endif
