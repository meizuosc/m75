/*
 * LM3697 Backlight Driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef _LM3697_H
#define _LM3697_H

/*Register*/
#define LM3697_HVLED_CUR_SINK	0X10
#define LM3697_CTRL_A_SSRT		0X11
#define LM3697_CTRL_B_SSRT		0X12
#define LM3697_CTRL_AB_RTRT		0X13
#define LM3697_RAMP_CONF		0X14
#define LM3697_BRT_CONF			0X16
#define LM3697_A_MAX_CUR		0X17
#define LM3697_B_MAX_CUR		0X18
#define LM3697_BOOST_CONF		0X1A
#define LM3697_PWM_CONF			0X1C
#define LM3697_CTRL_A_BRT_LSB	0X20
#define LM3697_CTRL_A_BRT_MSB	0X21
#define LM3697_CTRL_B_BRT_LSB	0X22
#define LM3697_CTRL_B_BRT_MSB	0X23
#define LM3697_CTRL_EN			0X24

#define LM3697_HVLED1 BIT(0)
#define LM3697_HVLED2 BIT(1)
#define LM3697_HVLED3 BIT(2)

#define LM3697_BANK_A_EN 	0x1
#define LM3697_BANK_B_EN	0x2
#define LM3697_BANK_BOTH_EN	0x3

enum lm3697_cur {
	LM3697_CUR_5MA = 0,
	LM3697_CUR_19_4MA = 0x12,
	LM3697_CUR_20_2MA = 0x13,
	LM3697_CUR_29_8MA = 0x1f,
};
enum lm3697_boost_ovp {
	LM3697_OVP_16V,
	LM3697_OVP_24V,
	LM3697_OVP_32V,
	LM3697_OVP_40V,
};

struct hvled_conf{
	int hvled1 	:1;
	int hvled2 	:1;
	int hvled3	:1;
};
struct ss_rt {
	int shutdown	:4;
	int startup	:4;
};
struct rt_rt {
	int rampdown	:4;
	int rampup	:4;
};
struct rt_conf {
	int ctrla_sel	:2;
	int ctrlb_sel	:2;
};
struct brt_conf {
	int map_mode	:1;
	int rsv		:1;
	int a_dither_en :1;
   	int b_dither_en :1;	
};

struct fscs_conf {
	int max_cur :5;
};

struct boost_conf {
	int freq :1;
	int ovp  :2;
	int auto_freq :1;
	int auto_hd :1;
};

struct pwm_conf {
	int a_pwm_en :1;
	int b_pwm_en :1;
	int pwm_polarity :1;
	int pwm_zd   :1;
};

struct lm3697_platform_data {
	/* Configurable Backlight Driver */
	const char *name;
	int initial_brightness;

	/* GPIO pin number for HWEN */
	int en_gpio;

	/* General Purpose Settings */
	struct hvled_conf string;
	struct ss_rt ssrt_a;
	struct ss_rt ssrt_b;
	struct rt_rt rtrt;
	struct rt_conf runtime;
	struct brt_conf brt_en;
	struct fscs_conf cur_a;
	struct fscs_conf cur_b;
	struct boost_conf boost;
	struct pwm_conf pwm;
	int bank_en;
	/* Ramp Rate Settings */
	bool disable_ramp;
};

#endif
