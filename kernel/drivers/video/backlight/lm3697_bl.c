/*
 * TI LM3697 Backlight Driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/platform_data/leds-lm3697.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#ifdef BUILD_LK
#include <platform/mt_gpio.h>
#else
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>
#endif

/* Registers */
#define DEFAULT_BL_NAME			"lm3697-led"
#define MAX_BRIGHTNESS			2048

struct lm3697 {
	struct i2c_client *client;
	struct backlight_device *bl;
	struct device *dev;
	struct lm3697_platform_data *pdata;
	int    enabled;
	int    brightness_store;
	int    suspended;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
};

static struct lm3697 * g_chip = NULL;


static int lm3697_write_byte(struct lm3697 *lm, u8 reg, u8 data)
{
	char write_data[2] = {0};
	int ret = 0;

	write_data[0] = reg;
	write_data[1] = data;
	lm->client->ext_flag = ((lm->client->ext_flag) & I2C_MASK_FLAG ) | I2C_DIRECTION_FLAG;
	ret = i2c_master_send(lm->client, write_data, 2);
	lm->client->ext_flag = 0;
	if (ret != 2)
		return ret;
	else
		return 0;
}

static int lm3697_power_on(struct lm3697 *pchip, int onoff)
{
	int gpio = pchip->pdata->en_gpio;

	if (pchip->enabled == onoff)
		return 0;

	mt_set_gpio_mode(gpio, GPIO_MODE_00);
	mt_set_gpio_dir(gpio, GPIO_DIR_OUT);
	if (onoff) {
		mt_set_gpio_out(gpio, GPIO_OUT_ONE);
	} else {
		mt_set_gpio_out(gpio, GPIO_OUT_ZERO);
	}

	pchip->enabled = onoff;

	return 0;
}
#define GET_BYTE(stru)  (unsigned char)((*(int*)stru)&0xff)

static int lm3697_init_device(struct lm3697 *lm)
{
	struct lm3697_platform_data *pdata = lm->pdata;
	u8 val = 0;
	int ret;

	ret = lm3697_power_on(lm, true);
	if (ret)
		return ret;

	/* Update the RAMP RATE register */
	ret = lm3697_write_byte(lm, LM3697_HVLED_CUR_SINK, GET_BYTE(&pdata->string));
	if (ret)
		goto err;
	ret = lm3697_write_byte(lm, LM3697_CTRL_A_SSRT, GET_BYTE(&pdata->ssrt_a));
	if (ret)
		goto err;
	ret = lm3697_write_byte(lm, LM3697_CTRL_B_SSRT, GET_BYTE(&pdata->ssrt_b));
	if (ret)
		goto err;
	ret = lm3697_write_byte(lm, LM3697_CTRL_AB_RTRT, GET_BYTE(&pdata->rtrt));
	if (ret)
		goto err;
	ret = lm3697_write_byte(lm, LM3697_RAMP_CONF, GET_BYTE(&pdata->runtime));
	if (ret)
		goto err;
	ret = lm3697_write_byte(lm, LM3697_BRT_CONF, GET_BYTE(&pdata->brt_en));
	if (ret)
		goto err;
	ret = lm3697_write_byte(lm, LM3697_A_MAX_CUR, GET_BYTE(&pdata->cur_a));
	if (ret)
		goto err;
	ret = lm3697_write_byte(lm, LM3697_B_MAX_CUR, GET_BYTE(&pdata->cur_b));
	if (ret)
		goto err;
	ret = lm3697_write_byte(lm, LM3697_BOOST_CONF, GET_BYTE(&pdata->boost));
	if (ret)
		goto err;
	ret = lm3697_write_byte(lm, LM3697_PWM_CONF, GET_BYTE(&pdata->pwm));
	if (ret)
		goto err;
	ret = lm3697_write_byte(lm, LM3697_CTRL_EN, GET_BYTE(&pdata->bank_en));
	if (ret)
		goto err;

	udelay(600);

	return 0;

err:
	return ret;
}
#ifdef CONFIG_THERMAL
static int lm3697_bl_update_status(struct backlight_device *bl);
static int limit = 0;
static int current_brightness = 0;
static int limit_flag = 0;

void set_max_brightness(int limit_brightness, int enable){
    if(g_chip) {
        if(enable){
            limit_flag = 1;
            limit = limit_brightness;
            if(limit < current_brightness) {
                g_chip->bl->props.brightness = limit;
                printk("the g_chip->bl->props.brightness is %d \n", g_chip->bl->props.brightness);
                lm3697_bl_update_status(g_chip->bl);
            }

        } else {
            limit_flag = 0;
            g_chip->bl->props.brightness = current_brightness;
            lm3697_bl_update_status(g_chip->bl);
        }
    } else {
        pr_err("%s backlight_device not ready \n", __func__);
    }
}
EXPORT_SYMBOL(set_max_brightness);
#endif

static int lm3697_bl_update_status(struct backlight_device *bl)
{
	struct lm3697 *lm = bl_get_data(bl);
	u8 brt_lsb = 0, brt_msb = 0;

	if (bl->props.state & BL_CORE_SUSPENDED)
		bl->props.brightness = 0;

	lm->brightness_store = bl->props.brightness;
#ifdef CONFIG_THERMAL
    if(!limit_flag)
        current_brightness = bl->props.brightness;
#endif
	if (lm->suspended)
		return 0;
	if (!bl->props.brightness) {
		if (lm->pdata->string.hvled1 && lm->pdata->string.hvled2) {
			lm3697_write_byte(lm, LM3697_CTRL_B_BRT_LSB, 0);
			lm3697_write_byte(lm, LM3697_CTRL_B_BRT_MSB, 0);
		} else if (!lm->pdata->string.hvled1 && !lm->pdata->string.hvled2) {
			lm3697_write_byte(lm, LM3697_CTRL_A_BRT_LSB, 0);
			lm3697_write_byte(lm, LM3697_CTRL_A_BRT_MSB, 0);
		} else {
			lm3697_write_byte(lm, LM3697_CTRL_A_BRT_LSB, 0);
			lm3697_write_byte(lm, LM3697_CTRL_A_BRT_MSB, 0);
			lm3697_write_byte(lm, LM3697_CTRL_B_BRT_LSB, 0);
			lm3697_write_byte(lm, LM3697_CTRL_B_BRT_MSB, 0);
		}
		return bl->props.brightness;
	}

	if (!lm->enabled) {
		lm3697_init_device(lm);
	}

#ifdef CONFIG_THERMAL
    if(limit_flag){
       if(bl->props.brightness > limit)
            bl->props.brightness = limit;
    }
#endif

	brt_lsb = bl->props.brightness & 0x7;
	brt_msb = bl->props.brightness >> 3;

	if (lm->pdata->string.hvled1 && lm->pdata->string.hvled2) {
		lm3697_write_byte(lm, LM3697_CTRL_B_BRT_LSB, brt_lsb);
		lm3697_write_byte(lm, LM3697_CTRL_B_BRT_MSB, brt_msb);
	} else if (!lm->pdata->string.hvled1 && !lm->pdata->string.hvled2) {
		lm3697_write_byte(lm, LM3697_CTRL_A_BRT_LSB, brt_lsb);
		lm3697_write_byte(lm, LM3697_CTRL_A_BRT_MSB, brt_msb);
	} else {
		lm3697_write_byte(lm, LM3697_CTRL_A_BRT_LSB, brt_lsb);
		lm3697_write_byte(lm, LM3697_CTRL_A_BRT_MSB, brt_msb);
		lm3697_write_byte(lm, LM3697_CTRL_B_BRT_LSB, brt_lsb);
		lm3697_write_byte(lm, LM3697_CTRL_B_BRT_MSB, brt_msb);
	}

	return bl->props.brightness;
}

static int lm3697_bl_get_brightness(struct backlight_device *bl)
{
	return bl->props.brightness;
}

static const struct backlight_ops lm3697_bl_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.update_status = lm3697_bl_update_status,
	.get_brightness = lm3697_bl_get_brightness,
};

static int lm3697_backlight_register(struct lm3697 *lm)
{
	struct backlight_device *bl;
	struct backlight_properties props;
	struct lm3697_platform_data *pdata = lm->pdata;
	const char *name;
	int brightness;

	if (pdata) {
		name = pdata->name ? : DEFAULT_BL_NAME;
		brightness = pdata->initial_brightness;
	} else {
		name = DEFAULT_BL_NAME;
		brightness = 0;
	}

	props.type = BACKLIGHT_PLATFORM;
	props.max_brightness = MAX_BRIGHTNESS;
	props.brightness = brightness;

	bl = backlight_device_register(name, lm->dev, lm,
				       &lm3697_bl_ops, &props);
	if (IS_ERR(bl))
		return PTR_ERR(bl);

	lm->bl = bl;

	return 0;
}

static void lm3697_backlight_unregister(struct lm3697 *lm)
{
	if (lm->bl)
		backlight_device_unregister(lm->bl);
}

/*
 * lm3697_set_mode
 * */
static int lm3697_mode = 0;
static int lm3697_mode_sel(struct lm3697 *lm, int mode)
{
	switch(mode) {
		case 0:
			lm->pdata->pwm.a_pwm_en = 0;
			lm->pdata->pwm.pwm_polarity = 1;
			lm3697_write_byte(lm, LM3697_PWM_CONF, GET_BYTE(&lm->pdata->pwm));

				break;
		case 1:
			lm->pdata->pwm.a_pwm_en = 1;
			lm->pdata->pwm.pwm_polarity = 1;
			lm3697_write_byte(lm, LM3697_PWM_CONF, GET_BYTE(&lm->pdata->pwm));
				break;
		case 2:
			lm->pdata->pwm.a_pwm_en = 1;
			lm->pdata->pwm.pwm_polarity = 0;
			lm3697_write_byte(lm, LM3697_PWM_CONF, GET_BYTE(&lm->pdata->pwm));
			break;

		default:break;
	}
	lm3697_mode = mode;
	return 0;
}
static ssize_t lm3697_set_mode(struct device *dev, struct device_attribute 
				*attr, char *buf, size_t size)
{
	int ret = 0;
	int mode = 0;
	
	if (g_chip == NULL) {
		pr_err("no device for lm3697\n");
		return -ENOENT;	
	}
	sscanf(buf, "%d", &mode);
	if (mode > 0xff) {
		pr_err("usage:\t0------>normal mode\n");
		pr_err("\t\t1------>pwm mode\n");
		pr_err("\t\t2------>pwm polarity high mode\n");
		pr_err("\t\t3------>pwm polarity low mode\n");
		return size;
	}
	pr_info("%s mode %d\n", __func__, mode);
	lm3697_mode_sel(g_chip, mode);
	return size;
}
static ssize_t lm3697_get_mode(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf,"%d\n",lm3697_mode);
}
static DEVICE_ATTR(mode, 0644, lm3697_get_mode, lm3697_set_mode);
#ifdef CONFIG_HAS_EARLYSUSPEND
static void lm3697_early_suspend(struct early_suspend *handler)
{
        struct lm3697 *pchip = container_of(handler,
                                        struct lm3697, early_suspend);

	pchip->bl->props.brightness = 0;
	backlight_update_status(pchip->bl);
    pchip->suspended = true;
}
static void lm3697_late_resume(struct early_suspend *handler)
{
        struct lm3697 *pchip = container_of(handler,
                                        struct lm3697, early_suspend);

    pchip->suspended = false;
	backlight_update_status(pchip->bl);
}
#endif

static int lm3697_probe(struct i2c_client *cl, const struct i2c_device_id *id)
{
	struct lm3697 *lm;
	struct lm3697_platform_data *pdata = cl->dev.platform_data;
	int ret;

	if (!i2c_check_functionality(cl->adapter, I2C_FUNC_SMBUS_I2C_BLOCK))
		return -EIO;

	lm = devm_kzalloc(&cl->dev, sizeof(struct lm3697), GFP_KERNEL);
	if (!lm)
		return -ENOMEM;

	lm->client = cl;
	lm->dev = &cl->dev;
	lm->pdata = pdata;
	lm->brightness_store = pdata->initial_brightness;

	ret = lm3697_init_device(lm);
	if (ret) {
		dev_err(lm->dev, "failed to init device: %d", ret);
		goto err_dev;
	}
	
	ret = lm3697_backlight_register(lm);
	if (ret) {
		dev_err(lm->dev, "failed to register backlight: %d\n", ret);
		goto err_dev;
	}

	backlight_update_status(lm->bl);
	lm->suspended = false;

#ifdef CONFIG_HAS_EARLYSUSPEND
	lm->early_suspend.suspend = lm3697_early_suspend;
	lm->early_suspend.resume  = lm3697_late_resume;
	lm->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;	
	//register_early_suspend(&lm->early_suspend);
#endif
	g_chip = lm;
	device_create_file(&lm->bl->dev, &dev_attr_mode);
	dev_info(&cl->dev,"TI backlight driver registerred OK!\n");

	return 0;
err_dev:
	devm_kfree(&cl->dev, lm);
	return ret;
}

static int lm3697_remove(struct i2c_client *cl)
{
	struct lm3697 *lm = i2c_get_clientdata(cl);

	lm->bl->props.brightness = 0;
	backlight_update_status(lm->bl);
	lm3697_backlight_unregister(lm);
	lm3697_power_on(lm, false);

	return 0;
}

static const struct i2c_device_id lm3697_ids[] = {
	{"bl-lm3697", 0},
	{ }
};
MODULE_DEVICE_TABLE(i2c, lm3697_ids);

void meizu_set_bl_onoff(int enable)
{
	if (g_chip) {
		if (enable == 0) 
			g_chip->bl->props.brightness = 0;

		lm3697_bl_update_status(g_chip->bl);
	}
}
EXPORT_SYMBOL(meizu_set_bl_onoff);

static struct i2c_driver lm3697_driver = {
	.driver = {
		.name = "bl-lm3697",
	},
	.probe = lm3697_probe,
	.remove = lm3697_remove,
	.id_table = lm3697_ids,
};
static struct lm3697_platform_data bl_lm3697 = {
	.name = "lm3630_bled",
	.initial_brightness = M75_DEFAULT_BRIGHTNESS,
	.en_gpio = GPIO_LCM_BL_EN,
	.string.hvled1 = 0, //control bank a
	.string.hvled2 = 0, // control bank a
	.string.hvled3 = 1, //control bank b
	.ssrt_a.shutdown = 0,
	.ssrt_a.startup = 0,
	.ssrt_b.shutdown = 0,
	.ssrt_b.startup = 0,
	.rtrt.rampdown = 0,
	.rtrt.rampup = 0,
	.runtime.ctrla_sel = 0x3,
	.runtime.ctrlb_sel = 0x3,
	.brt_en.map_mode = 1,//1 for linear, 0 for exponetial
	.brt_en.a_dither_en = 0,
	.brt_en.b_dither_en = 0,
	.cur_a.max_cur = 0x10,
	.cur_b.max_cur = 0x10,
	.boost.freq = 1,// 0 for 500KHz, q for 1MHz
	.boost.ovp = LM3697_OVP_32V,
	.boost.auto_freq = 1,
	.boost.auto_hd = 0,
	.pwm.a_pwm_en = 0,
	.pwm.b_pwm_en = 0,
	.pwm.pwm_polarity = 1,
	.pwm.pwm_zd = 0,
	.bank_en = LM3697_BANK_A_EN,
};
static struct i2c_board_info __initdata i2c_lm3697[] = {
	{
		I2C_BOARD_INFO("bl-lm3697", (0x36)),
		.platform_data = &bl_lm3697,
	},
};
static int __init lm3697_driver_init(void)
{
	int ret = 0;
	
	i2c_register_board_info(3, i2c_lm3697, 1);
	
	ret = i2c_add_driver(&lm3697_driver);
	if (ret != 0) {
		pr_err("@@@@@@@@@@@@@@LM3697 register error!\n");
	}

	return ret;

}
static void __exit lm3697_driver_exit(void)
{
	i2c_del_driver(&lm3697_driver);
}
module_init(lm3697_driver_init);
module_exit(lm3697_driver_exit);

MODULE_DESCRIPTION("Texas Instruments LM3697 Backlight driver");
MODULE_AUTHOR("Mars<caoziqiang@meizu.com>");
MODULE_LICENSE("GPL");
