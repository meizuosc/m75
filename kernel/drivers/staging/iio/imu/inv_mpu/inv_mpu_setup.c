/*
* Copyright (C) 2012 Invensense, Inc.
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/
#define pr_fmt(fmt) "inv_mpu: " fmt

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/jiffies.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kfifo.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>

#include "linux/iio/iio.h"
#include "linux/iio/kfifo_buf.h"
#include "linux/iio/trigger_consumer.h"
#include "linux/iio/sysfs.h"

#include "inv_mpu_iio.h"
struct inv_local_store {
	u8 reg_pwr_mgmt_2;
	u8 reg_lp_config;
	u8 reg_fifo_cfg;
	u8 reg_fifo_size_0;
	u8 reg_delay_enable;
	u8 reg_delay_time;
	u8 reg_gyro_smplrt;
	u8 reg_accel_smplrt;
	bool batchmode_en;
	bool d_flag;
	bool step_indicator_on;
	bool geomag_enable;
	bool step_detector_on;
	bool accel_cal_enable;
	bool gyro_cal_enable;
	bool compass_cal_enable;
	bool wom_on;
	int ped_rate;
	int cpass_cal_ind;
	int accel_cal_ind;
};

static struct inv_local_store local;

struct inv_compass_cal_params {
	int ct;
	int alpha_c;
	int a_c;
	int RADIUS_3D;
	int nomot_var_thld;
};

static struct inv_compass_cal_params compass_cal_param[] = {
	{
		.ct = 200,
	},
	{
		.ct = 70,
		.alpha_c = 920350135,
		.a_c  = 153391689,
		.RADIUS_3D = 3584,
		.nomot_var_thld = 12,
	},
	{
		.ct = 35,
		.alpha_c = 766958446,
		.a_c     = 306783378,
		.RADIUS_3D = 3584,
		.nomot_var_thld = 12,
	},
	{
		.ct = 15,
		.alpha_c = 357913941,
		.a_c	= 715827883,
		.RADIUS_3D = 3584,
		.nomot_var_thld = 8,
	},
	{
		.ct = 8,
		.alpha_c = 107374182,
		.a_c     = 966367642,
		.RADIUS_3D = 3584,
		.nomot_var_thld = 1,
	},
	{
		.ct = 4,
		.alpha_c = 107374182,
		.a_c     = 966367642,
		.RADIUS_3D = 3584,
		.nomot_var_thld = 1,
	},
};

struct inv_accel_cal_params {
	int freq;
	int rate;
	int gain;
	int alpha;
	int a;
};

static struct inv_accel_cal_params accel_cal_para[] = {
	{
		.freq = 1000,
	},
	{
		.freq = 225,
		.rate = 3,
		.gain = DEFAULT_ACCEL_GAIN,
		.alpha = 858993459,
		.a     = 214748365,
	},
	{
		.freq = 102,
		.gain = DEFAULT_ACCEL_GAIN,
		.rate = 1,
		.alpha = 858993459,
		.a     = 214748365,
	},
	{
		.freq = PEDOMETER_FREQ,
		.gain = PED_ACCEL_GAIN,
		.rate = 0,
		.alpha = 858993459,
		.a     = 214748365,
	},
	{
		.freq = 15,
		.gain = DEFAULT_ACCEL_GAIN,
		.rate = 0,
		.alpha = 357913941,
		.a     = 715827883,
	},
	{
		.freq = 5,
		.gain = DEFAULT_ACCEL_GAIN,
		.rate = 0,
		.alpha = 107374182,
		.a     = 966367642,
	},
};

static int inv_out_data_cntl(struct inv_mpu_state *st, u16 wd, bool en)
{
	return inv_write_cntl(st, wd, en, DATA_OUT_CTL1);
}

static int inv_out_data_cntl_2(struct inv_mpu_state *st, u16 wd, bool en)
{
	return inv_write_cntl(st, wd, en, DATA_OUT_CTL2);
}

static int inv_set_batchmode(struct inv_mpu_state *st, bool enable)
{
	int result;

	if (local.batchmode_en != enable) {
		result = inv_out_data_cntl_2(st, BATCH_MODE_EN, enable);
		if (result)
			return result;
		local.batchmode_en = enable;
	}

	return 0;
}
static int inv_calc_engine_dur(struct inv_engine_info *ei)
{
	if (!ei->running_rate)
		return -EINVAL;

	ei->dur = ei->base_time / ei->orig_rate;
	ei->dur *= ei->divider;

	return 0;
}
static int inv_batchmode_calc(struct inv_mpu_state *st)
{
	int b, timeout;
	int i, bps, max_rate, max_ind;
	enum INV_ENGINE eng;

	max_rate = 0;
	max_ind = -1;
	bps = 0;
	for (i = 0; i < SENSOR_NUM_MAX; i++) {
		if (st->sensor[i].on) {
			if (max_rate < st->sensor[i].rate) {
				max_rate = st->sensor[i].rate;
				max_ind = i;
			}
			bps += (st->sensor[i].sample_size + 2) *
							st->sensor[i].rate;
		}
	}
	st->batch.max_rate = max_rate;
	if (max_rate  && bps) {
		b = st->batch.timeout * bps;
		if ((b > (FIFO_SIZE * MSEC_PER_SEC)) &&
						(!st->batch.overflow_on))
			timeout = FIFO_SIZE * MSEC_PER_SEC / bps;
		else
			timeout = st->batch.timeout;

		eng = st->sensor[max_ind].engine_base;
	} else {
		if (st->chip_config.step_detector_on ||
					st->chip_config.step_indicator_on) {
			eng = ENGINE_ACCEL;
			timeout = st->batch.timeout;
		} else {
			return -EINVAL;
		}
	}

	b = st->eng_info[eng].dur / USEC_PER_MSEC;
	st->batch.engine_base = eng;
	st->batch.counter = timeout * USEC_PER_MSEC / b;

	if (st->batch.counter)
		st->batch.on = true;

	return 0;
}

static int inv_set_default_batch(struct inv_mpu_state *st)
{
	if (st->batch.max_rate > DEFAULT_BATCH_RATE) {
		st->batch.default_on = true;
		st->batch.counter = DEFAULT_BATCH_TIME * NSEC_PER_MSEC /
					st->eng_info[ENGINE_GYRO].dur;
	}

	return 0;
}
int inv_batchmode_setup(struct inv_mpu_state *st)
{
	int r;
	bool on;
	s16 mask[ENGINE_NUM_MAX] = {1, 2, 8, 8};

	r = write_be32_to_mem(st, 0, BM_BATCH_CNTR);
	if (r)
		return r;
	st->batch.on = false;
	st->batch.default_on = false;
	if (st->batch.timeout > 0) {
		r = inv_batchmode_calc(st);
		if (r)
			return r;
	} else {
		r = inv_set_default_batch(st);
		if (r)
			return r;
	}

	on = (st->batch.on || st->batch.default_on);

	if (on) {
		r = write_be32_to_mem(st, st->batch.counter, BM_BATCH_THLD);
		if (r)
			return r;
		r = inv_write_2bytes(st, BM_BATCH_MASK,
						mask[st->batch.engine_base]);
		if (r)
			return r;
	}
	r = inv_set_batchmode(st, on);

	return r;
}

static int inv_turn_on_fifo(struct inv_mpu_state *st)
{
	u8 w, x;
	int r;

	/* clear FIFO data */
	r = inv_plat_single_write(st, REG_FIFO_RST, MAX_5_BIT_VALUE);
	if (r)
		return r;
	if (!st->chip_config.dmp_on)
		w = 0;
	else
		w = MAX_5_BIT_VALUE - 1;
	r = inv_plat_single_write(st, REG_FIFO_RST, w);
	if (r)
		return r;
	/* turn on FIFO output data  for non-DMP mode */
	w = 0;
	x = 0;
	if (!st->chip_config.dmp_on) {
		if (st->sensor[SENSOR_GYRO].on)
			w |= BITS_GYRO_FIFO_EN;
		if (st->sensor[SENSOR_ACCEL].on)
			w |= BIT_ACCEL_FIFO_EN;
		if (st->sensor[SENSOR_COMPASS].on)
			x |= BIT_SLV_0_FIFO_EN;
	}
	r = inv_plat_single_write(st, REG_FIFO_EN_2, w);
	if (r)
		return r;
	r = inv_plat_single_write(st, REG_FIFO_EN, x);
	if (r)
		return r;
	/* turn on user ctrl register */
	if (st->chip_config.dmp_on) {
		w = BIT_DMP_RST;
		r = inv_plat_single_write(st, REG_USER_CTRL, w | st->i2c_dis);
		if (r)
			return r;
		msleep(DMP_RESET_TIME);
	}
	/* turn on interrupt */
	if (st->chip_config.dmp_on) {
		r = inv_plat_single_write(st, REG_INT_ENABLE, BIT_DMP_INT_EN);
		if (r)
			return r;
		r = inv_plat_single_write(st, REG_INT_ENABLE_2,
							BIT_FIFO_OVERFLOW_EN_0);
	} else {
		w = 0;
		if (st->sensor[SENSOR_GYRO].on && st->sensor[SENSOR_ACCEL].on)
			w = (BIT_DATA_RDY_0_EN | BIT_DATA_RDY_1_EN);
		else
			w = BIT_DATA_RDY_0_EN;
		r = inv_plat_single_write(st, REG_INT_ENABLE_1, w);
	}

	w = BIT_FIFO_EN;
	if (st->chip_config.dmp_on)
		w |= BIT_DMP_EN;
	if (st->chip_config.slave_enable)
			w |= BIT_I2C_MST_EN;
	r = inv_plat_single_write(st, REG_USER_CTRL, w | st->i2c_dis);
	if (r)
		return r;

	return r;
}

static int inv_turn_off_fifo(struct inv_mpu_state *st)
{
	int r;

	if (st->chip_config.dmp_on) {
		r = inv_plat_single_write(st, REG_INT_ENABLE, 0);
		if (r)
			return r;
		r = inv_plat_single_write(st, REG_INT_ENABLE_2, 0);
	} else {
		r = inv_plat_single_write(st, REG_INT_ENABLE_1, 0);
		if (r)
			return r;
		r = inv_plat_single_write(st, REG_FIFO_EN_2, 0);
	}
	if (r)
		return r;

	r = inv_plat_single_write(st, REG_FIFO_RST, MAX_5_BIT_VALUE);
	if (r)
		return r;
	r = inv_plat_single_write(st, REG_FIFO_RST, 0x0);

	return r;
}

/*
 *  inv_reset_fifo() - Reset FIFO related registers.
 */
int inv_reset_fifo(struct iio_dev *indio_dev, bool turn_off)
{
	struct inv_mpu_state *st = iio_priv(indio_dev);
	int r, i;

	if (turn_off) {
		r = inv_turn_off_fifo(st);
		if (r)
			return r;
	}
	r = inv_turn_on_fifo(st);
	if (r)
		return r;
	st->last_run_time = get_time_ns();
	for (i = 0; i < SENSOR_NUM_MAX; i++) {
		if (st->sensor[i].on) {
			st->sensor[i].ts = st->last_run_time;
			st->sensor[i].time_calib = st->last_run_time;
			st->sensor[i].sample_calib = 0;
			st->sensor[i].calib_flag = 0;
		}
	}
	r = read_be32_from_mem(st, &st->start_dmp_counter, DMPRATE_CNTR);
	if (r)
		return r;
	for (i = 0; i < ENGINE_NUM_MAX; i++)
		st->eng_info[i].last_update_time = st->last_run_time;

	st->step_detector_base_ts = st->last_run_time;
	st->ts_for_calib = st->last_run_time;
	st->last_temp_comp_time = st->last_run_time;
	st->left_over_size        = 0;

	return 0;
}

static int inv_turn_on_engine(struct inv_mpu_state *st)
{
	u8 w, v;
	int r;

	r = 0;
	if (ICM20728 == st->chip_type) {
		if (st->chip_config.gyro_enable |
			st->chip_config.accel_enable |
			st->chip_config.pressure_enable) {
			w = 0;
			if (!st->chip_config.gyro_enable)
				w |= BIT_PWR_GYRO_STBY;
			if (!st->chip_config.accel_enable)
				w |= BIT_PWR_ACCEL_STBY;
			if (!st->chip_config.pressure_enable)
				w |= BIT_PWR_PRESSURE_STBY;
		} else {
			w = (BIT_PWR_GYRO_STBY |
					BIT_PWR_ACCEL_STBY |
					BIT_PWR_PRESSURE_STBY);
		}
	} else {
		if (st->chip_config.gyro_enable |
				st->chip_config.accel_enable) {
			w = BIT_PWR_PRESSURE_STBY;
			if (!st->chip_config.gyro_enable)
				w |= BIT_PWR_GYRO_STBY;
			if (!st->chip_config.accel_enable)
				w |= BIT_PWR_ACCEL_STBY;
		} else {
			w = (BIT_PWR_GYRO_STBY |
					BIT_PWR_ACCEL_STBY |
					BIT_PWR_PRESSURE_STBY);
		}
	}
	inv_plat_read(st, REG_PWR_MGMT_2, 1, &v);
	if ((BIT_PWR_ALL_OFF == v) &&
		(BIT_PWR_ALL_OFF != w) &&
		(!st->chip_config.slave_enable)) {
		r = inv_plat_single_write(st, REG_PWR_MGMT_2,
						BIT_PWR_GYRO_STBY |
						BIT_PWR_PRESSURE_STBY);
		if (r)
			return r;
	}
	if (!st->chip_config.dmp_on) {
		r = inv_plat_single_write(st, REG_PWR_MGMT_2,
						BIT_PWR_PRESSURE_STBY);
		if (r)
			return r;
	}
	if (st->chip_config.gyro_enable)
		msleep(GYRO_ENGINE_UP_TIME);

	if (st->chip_config.has_compass) {
		if (st->chip_config.compass_enable)
			r = st->slave_compass->resume(st);
		else
			r = st->slave_compass->suspend(st);
		if (r)
			return r;
	}
	if (st->chip_config.has_als) {
		if (st->chip_config.als_enable)
			r = st->slave_als->resume(st);
		else
			r = st->slave_als->suspend(st);
		if (r)
			return r;
	}
	if (st->chip_config.has_pressure &&  (ICM20628 == st->chip_type)) {
		if (st->chip_config.pressure_enable)
			r = st->slave_pressure->resume(st);
		else
			r = st->slave_pressure->suspend(st);
		if (r)
			return r;
	}

	/* secondary cycle mode should be set all the time */
	w = BIT_I2C_MST_CYCLE;
	if (st->chip_config.low_power_gyro_on)
		w |= BIT_GYRO_CYCLE;
	w |= BIT_ACCEL_CYCLE;
	if (w != local.reg_lp_config) {
		r = inv_plat_single_write(st, REG_LP_CONFIG, w);
		local.reg_lp_config = w;
	}

	return r;
}

static int inv_setup_dmp_rate(struct inv_mpu_state *st)
{
	int i, result, tmp;
	int div[SENSOR_NUM_MAX];
	bool d_flag;

	result = 0;
	for (i = 0; i < SENSOR_NUM_MAX; i++) {
		if (st->sensor[i].on) {
			if (!st->sensor[i].rate) {
				pr_err("sensor %d rate is zero\n", i);
				return -EINVAL;
			}
			div[i] =
			st->eng_info[st->sensor[i].engine_base].running_rate /
							st->sensor[i].rate;
			if (!div[i])
				div[i] = 1;
		}
	}
    /* make geomag and rv consistent */
    if (st->sensor[SENSOR_NINEQ].on &&
					st->sensor[SENSOR_GEOMAG].on) {
		tmp = min(div[SENSOR_GEOMAG], div[SENSOR_NINEQ]);
		div[SENSOR_GEOMAG] = tmp;
		div[SENSOR_NINEQ] = tmp;
	}
	
	/* make compass and calib compass consistent */
	if (st->sensor[SENSOR_COMPASS].on &&
					st->sensor[SENSOR_CALIB_COMPASS].on) {
		tmp = min(div[SENSOR_COMPASS], div[SENSOR_CALIB_COMPASS]);
		div[SENSOR_COMPASS] = tmp;
		div[SENSOR_CALIB_COMPASS] = tmp;
	}
	for (i = 0; i < SENSOR_NUM_MAX; i++) {
		if (st->sensor[i].on) {
			st->sensor[i].dur =
				st->eng_info[st->sensor[i].engine_base].dur *
						div[i];
			if (div[i] != st->sensor[i].div) {
				st->sensor[i].div = div[i];
				result = inv_write_2bytes(st,
					st->sensor[i].odr_addr, div[i] - 1);
				if (result)
					return result;
				result = inv_out_data_cntl(st,
						st->sensor[i].output, true);
				if (result)
					return result;
			}
		} else if (-1 != st->sensor[i].div) {
			result = inv_out_data_cntl(st,
					st->sensor[i].output, false);
			if (result)
				return result;
			st->sensor[i].div = -1;
		}
	}

	d_flag = 0;
	for (i = 0; i < SENSOR_ACCURACY_NUM_MAX; i++) {
		result = inv_out_data_cntl_2(st,
				st->sensor_accuracy[i].output,
					st->sensor_accuracy[i].on);
		if (result)
			return result;
		d_flag |= st->sensor_accuracy[i].on;
	}

	if (d_flag != local.d_flag) {
		result = inv_out_data_cntl(st, HEADER2_SET, d_flag);
		if (result)
			return result;
		local.d_flag = d_flag;
	}
	if (st->chip_config.step_indicator_on != local.step_indicator_on) {
		result = inv_out_data_cntl(st, PED_STEPIND_SET,
					st->chip_config.step_indicator_on);
		if (result)
			return result;
		local.step_indicator_on = st->chip_config.step_indicator_on;
	}
	if (st->chip_config.geomag_enable != local.geomag_enable) {
		result = inv_out_data_cntl_2(st, GEOMAG_EN,
						st->chip_config.geomag_enable);
		if (result)
			return result;
		local.geomag_enable = st->chip_config.geomag_enable;
	}
	if (st->chip_config.step_detector_on != local.step_detector_on) {
		result = inv_out_data_cntl(st, PED_STEPDET_SET,
					st->chip_config.step_detector_on);
		if (result)
			return result;
		local.step_detector_on = st->chip_config.step_detector_on;
	}

	if (!st->chip_config.dmp_event_int_on)
		result = inv_batchmode_setup(st);

	return result;
}

static int inv_set_pressure_rate(struct inv_mpu_state *st)
{
	int r, i;

	if ((!st->chip_config.gyro_enable) && (!st->chip_config.accel_enable))
		r = max(st->sensor[SENSOR_PRESSURE].rate,
					st->chip_config.compass_rate);
	else
		r = st->sensor[SENSOR_PRESSURE].rate;
	if (r > 0) {
		i = -1;
		while (r < MAX_PRS_RATE) {
			r <<= 1;
			i++;
		}
	} else {
		return -EINVAL;
	}
	if (i < 0)
		i = 0;
	st->eng_info[ENGINE_PRESSURE].running_rate = (MAX_PRS_RATE >> i);
	st->eng_info[ENGINE_PRESSURE].divider      = (1 << i);
	r = inv_calc_engine_dur(&st->eng_info[ENGINE_PRESSURE]);
	if (r)
		return r;
	r = inv_plat_single_write(st, REG_PRS_ODR_CONFIG, i);

	return r;
}

static int inv_set_div(struct inv_mpu_state *st, int a_d, int g_d)
{
	int result;

	result = inv_set_bank(st, BANK_SEL_2);
	if (result)
		return result;

	if (local.reg_gyro_smplrt != g_d) {
		result = inv_plat_single_write(st, REG_GYRO_SMPLRT_DIV, g_d);
		if (result)
			return result;
		local.reg_gyro_smplrt = g_d;
	}
	if (local.reg_accel_smplrt != a_d) {
		result = inv_plat_single_write(st, REG_ACCEL_SMPLRT_DIV_2, a_d);
		if (result)
			return result;
		local.reg_accel_smplrt = a_d;
	}
	if (st->chip_config.pressure_enable && (ICM20728 == st->chip_type)) {
		result = inv_set_pressure_rate(st);
		if (result)
			return result;
	}
	result = inv_set_bank(st, BANK_SEL_0);

	return result;
}

static int inv_set_rate(struct inv_mpu_state *st)
{
	int g_d, a_d, result;

	if (st->chip_config.dmp_on) {
		result = inv_setup_dmp_rate(st);
		if (result)
			return result;
	} else {
		st->eng_info[ENGINE_GYRO].running_rate =
						st->sensor[SENSOR_GYRO].rate;
		st->eng_info[ENGINE_ACCEL].running_rate =
						st->sensor[SENSOR_ACCEL].rate;
	}

	g_d = st->eng_info[ENGINE_GYRO].divider - 1;
	a_d = st->eng_info[ENGINE_ACCEL].divider - 1;
	result = inv_set_div(st, a_d, g_d);
	if (result)
		return result;

	return result;
}

static int inv_set_fifo_size(struct inv_mpu_state *st)
{
	int result;
	u8 size, cfg, ind;

	result = 0;
	if (st->chip_config.dmp_on) {
		/* use one FIFO in DMP mode */
		cfg = BIT_MULTI_FIFO_CFG;
		size = BIT_GYRO_FIFO_SIZE_1024;
	} else {
		ind = 0;
		if (st->sensor[SENSOR_GYRO].on)
			ind++;
		if (st->sensor[SENSOR_ACCEL].on)
			ind++;
		if (st->sensor[SENSOR_COMPASS].on)
			ind++;
		if (ind > 1) {
			cfg = BIT_MULTI_FIFO_CFG;
			size = (BIT_GYRO_FIFO_SIZE_1024 |
					BIT_ACCEL_FIFO_SIZE_1024
					| BIT_FIFO_3_SIZE_64);
		} else {
			cfg = BIT_SINGLE_FIFO_CFG;
			size = BIT_FIFO_SIZE_1024;
		}
	}
	if (cfg != local.reg_fifo_cfg) {
		result = inv_plat_single_write(st, REG_FIFO_CFG, cfg);
		if (result)
			return result;
		local.reg_fifo_cfg = cfg;
	}

	if (size != local.reg_fifo_size_0) {
		result = inv_plat_single_write(st, REG_FIFO_SIZE_0, size);
		if (result)
			return result;
		local.reg_fifo_size_0 = size;
	}

	return result;
}

/*
 *  inv_set_fake_secondary() - set fake secondary I2C such that
 *                           I2C data in the same position.
 */
static int inv_set_fake_secondary(struct inv_mpu_state *st)
{
	int r;
	u8 bytes, ind;

	/* may need to saturate the master I2C counter like Scorpion did */
	r = inv_set_bank(st, BANK_SEL_3);
	if (r)
		return r;
	if (st->sec_set.delay_enable != local.reg_delay_enable) {
		r = inv_plat_single_write(st, REG_I2C_MST_DELAY_CTRL,
						st->sec_set.delay_enable);
		if (r)
			return r;
		local.reg_delay_enable = st->sec_set.delay_enable;
	}
	if (st->sec_set.delay_time != local.reg_delay_time) {
		r = inv_plat_single_write(st, REG_I2C_SLV4_CTRL,
					st->sec_set.delay_time);
		if (r)
			return r;
		local.reg_delay_time = st->sec_set.delay_time;
	}
	/* odr config is changed during slave setup */
	r = inv_plat_single_write(st, REG_I2C_MST_ODR_CONFIG,
						st->sec_set.odr_config);
	if (r)
		return r;
	r = inv_set_bank(st, BANK_SEL_0);
	if (r)
		return r;

	if (ICM20728 == st->chip_type)
		return 0;
	/*111, 110 */
	if (st->chip_config.compass_enable && st->chip_config.als_enable)
		return 0;
	/* 100 */
	if (st->chip_config.compass_enable &&
		(!st->chip_config.als_enable) &&
		(!st->chip_config.pressure_enable))
		return 0;
	r = inv_set_bank(st, BANK_SEL_3);
	if (r)
		return r;

	if (st->chip_config.pressure_enable) {
		/* 001 */
		if ((!st->chip_config.compass_enable) &&
					(!st->chip_config.als_enable)) {
			r = inv_read_secondary(st, 0,
						st->plat_data.aux_i2c_addr,
						BMP280_DIG_T1_LSB_REG,
						DATA_AKM_99_BYTES_DMP);
			if (r)
				return r;
			r = inv_read_secondary(st, 2,
						st->plat_data.aux_i2c_addr,
						BMP280_DIG_T1_LSB_REG,
						DATA_ALS_BYTES_DMP);
			r = inv_set_bank(st, BANK_SEL_0);

			return r;
		}

		if (st->chip_config.compass_enable &&
					(!st->chip_config.als_enable)) {
			/* 101 */
			ind = 2;
			if ((COMPASS_ID_AK09911 ==
						st->plat_data.sec_slave_id) ||
					(COMPASS_ID_AK09912 ==
						st->plat_data.sec_slave_id))
				bytes = DATA_ALS_BYTES_DMP;
			else
				bytes = DATA_ALS_BYTES_DMP +
					DATA_AKM_99_BYTES_DMP -
					DATA_AKM_89_BYTES_DMP;
		} else { /* 011 */
			ind = 0;
			bytes = DATA_AKM_99_BYTES_DMP;
		}
		r = inv_read_secondary(st, ind, st->plat_data.aux_i2c_addr,
						BMP280_DIG_T1_LSB_REG, bytes);
	} else { /* compass disabled; als enabled, pressure disabled 010 */
		r = inv_read_secondary(st, 0, st->plat_data.read_only_i2c_addr,
				APDS9900_AILTL_REG, DATA_AKM_99_BYTES_DMP);
	}
	if (r)
		return r;
	r = inv_set_bank(st, BANK_SEL_0);

	return r;
}
static int inv_set_ICM20728_secondary(struct inv_mpu_state *st)
{
	return 0;
}

static int inv_set_ICM20628_secondary(struct inv_mpu_state *st)
{
	int rate, compass_rate, pressure_rate, als_rate, min_rate, base;
	int mst_odr_config, d, delay;

	if (st->chip_config.compass_enable)
		compass_rate = st->chip_config.compass_rate;
	else
		compass_rate = 0;
	if (st->chip_config.pressure_enable)
		pressure_rate = st->sensor[SENSOR_PRESSURE].rate;
	else
		pressure_rate = 0;
	if (st->chip_config.als_enable)
		als_rate = st->sensor[SENSOR_ALS].rate;
	else
		als_rate = 0;
	if (compass_rate)
		rate = compass_rate;
	else
		rate = max(pressure_rate, als_rate);
	mst_odr_config = 0;
	min_rate = BASE_SAMPLE_RATE;
	rate += (rate >> 1);
	while (rate < min_rate) {
		mst_odr_config++;
		min_rate >>= 1;
	}
	if (mst_odr_config < MIN_MST_ODR_CONFIG)
		mst_odr_config = MIN_MST_ODR_CONFIG;

	base = BASE_SAMPLE_RATE / (1 << mst_odr_config);
	st->eng_info[ENGINE_I2C].running_rate = base;
	st->eng_info[ENGINE_I2C].divider = (1 << mst_odr_config);
	inv_calc_engine_dur(&st->eng_info[ENGINE_I2C]);

	d = 0;
	if (d > 0)
		d -= 1;
	if (d > MAX_5_BIT_VALUE)
		d = MAX_5_BIT_VALUE;

	/* I2C_MST_DLY is set to slow down secondary I2C */
	if (d)
		delay = 0x1F;
	else
		delay = 0;

	st->sec_set.delay_enable = delay;
	st->sec_set.delay_time = d;
	st->sec_set.odr_config = mst_odr_config;

	return 0;
}

static int inv_set_master_delay(struct inv_mpu_state *st)
{

	if (!st->chip_config.slave_enable)
		return 0;
	if (ICM20728 == st->chip_type)
		inv_set_ICM20728_secondary(st);
	else
		inv_set_ICM20628_secondary(st);

	return 0;
}

static int inv_enable_accel_cal_V3(struct inv_mpu_state *st, u8 enable)
{
	return inv_write_cntl(st, ACCEL_CAL_EN << 8, enable, MOTION_EVENT_CTL);
}

static int inv_enable_gyro_cal_V3(struct inv_mpu_state *st, u8 enable)
{
	return inv_write_cntl(st, GYRO_CAL_EN << 8, enable, MOTION_EVENT_CTL);
}

static int inv_enable_compass_cal_V3(struct inv_mpu_state *st, u8 enable)
{
	return inv_write_cntl(st, COMPASS_CAL_EN, enable, MOTION_EVENT_CTL);
}

static int inv_enable_9axes_V3(struct inv_mpu_state *st)
{
	bool en;

	if (st->sensor[SENSOR_NINEQ].on ||
		st->sensor[SENSOR_GEOMAG].on ||
			st->sensor_accuracy[SENSOR_COMPASS_ACCURACY].on)
		en = true;
	else
		en = false;

	return inv_write_cntl(st, NINE_AXIS_EN, en, MOTION_EVENT_CTL);
}

static int inv_set_wom(struct inv_mpu_state *st)
{
	int result;
	u8 d[4] = {0, 0, 0, 0};

	if (st->chip_config.wom_on)
		d[3] = 1;

	if (local.wom_on != st->chip_config.wom_on) {
		result = mem_w(WOM_ENABLE, ARRAY_SIZE(d), d);
		if (result)
			return result;
		local.wom_on = st->chip_config.wom_on;
	}

	return 0;
}

static int inv_setup_events(struct inv_mpu_state *st)
{
	int result;

	result = inv_write_cntl(st, PEDOMETER_EN << 8, st->ped.engine_on,
							MOTION_EVENT_CTL);
	if (result)
		return result;

	result = inv_write_cntl(st, SMD_EN << 8, st->smd.on,
							MOTION_EVENT_CTL);

	return result;
}

static int inv_setup_sensor_interrupt(struct inv_mpu_state *st)
{
	int i, ind, rate;
	u16 cntl;

	cntl = 0;
	ind = -1;
	rate = 0;

	for (i = 0; i < SENSOR_NUM_MAX; i++) {
		if (st->sensor[i].on) {
			if (st->batch.on) {
				cntl |= st->sensor[i].output;
			} else {
				if (st->sensor[i].rate > rate) {
					ind = i;
					rate = st->sensor[i].rate;
				}
			}
		}
	}
    // Take appropriate output for sensors sharing same configuration address
	// on DMP memory
	if (ind != -1) {
		for (i = 0; i < SENSOR_NUM_MAX; i++) {
			if ((st->sensor[i].odr_addr == st->sensor[ind].odr_addr) &&
				st->sensor[i].on) {
				if (!st->sensor[ind].output) {
					printk("IWA ind changed from %d to %d\n", ind, i);
					ind = i;
				}
			}
		}
 	}
	if (ind != -1)
		cntl |= st->sensor[ind].output;
	if (st->chip_config.step_detector_on)
		cntl |= PED_STEPDET_SET;

	return inv_write_2bytes(st, DATA_INTR_CTL, cntl);
}

static int inv_setup_dmp(struct inv_mpu_state *st)
{
	int result, i, tmp, min_diff, ind;

	result = inv_setup_sensor_interrupt(st);
	if (result)
		return result;

	i = 0;
	ind = 0;
	min_diff = accel_cal_para[0].freq;
	while (i < ARRAY_SIZE(accel_cal_para)) {
		tmp = abs(accel_cal_para[i].freq -
				st->eng_info[ENGINE_ACCEL].running_rate);
		if (tmp < min_diff) {
			min_diff = tmp;
			ind = i;
		}
		i++;
	}
	i = ind;
	if (i != local.accel_cal_ind) {
		result = inv_write_2bytes(st,
				ACCEL_CAL_RATE, accel_cal_para[i].rate);
		if (result)
			return result;
		result = write_be32_to_mem(st,
				accel_cal_para[i].rate, PED_RATE);
		if (result)
			return result;
		result = write_be32_to_mem(st,
				accel_cal_para[i].alpha, ACCEL_ALPHA_VAR);
		if (result)
			return result;
		result = write_be32_to_mem(st,
				accel_cal_para[i].a, ACCEL_A_VAR);
		if (result)
			return result;
		result = write_be32_to_mem(st,
				accel_cal_para[i].gain, ACCEL_ONLY_GAIN);
		if (result)
			return result;
		local.accel_cal_ind = i;
	}
	i = 0;
	min_diff = compass_cal_param[0].ct;
	while (i < ARRAY_SIZE(compass_cal_param)) {
		tmp = abs(compass_cal_param[i].ct -
				st->eng_info[ENGINE_I2C].running_rate);
		if (tmp < min_diff) {
			min_diff = tmp;
			ind = i;
		}
		i++;
	}
	i = ind;
	if ((i != local.cpass_cal_ind) &&
				(st->eng_info[ENGINE_I2C].running_rate)) {
		result = inv_write_2bytes(st, CPASS_TIME_BUFFER,
						compass_cal_param[i].ct);
		if (result)
			return result;
		result = write_be32_to_mem(st, compass_cal_param[i].alpha_c,
							CPASS_ALPHA_VAR);
		if (result)
			return result;
		result = write_be32_to_mem(st, compass_cal_param[i].a_c,
						CPASS_A_VAR);
		if (result)
			return result;
		result = write_be32_to_mem(st, compass_cal_param[i].RADIUS_3D,
						CPASS_RADIUS_3D_THRESH_ANOMALY);
		if (result)
			return result;
		result = write_be32_to_mem(st,
					compass_cal_param[i].nomot_var_thld,
						CPASS_NOMOT_VAR_THRESH);
		if (result)
			return result;

		local.cpass_cal_ind = i;
	}

	if (local.accel_cal_enable != st->accel_cal_enable) {
		result = inv_enable_accel_cal_V3(st, st->accel_cal_enable);
		if (result)
			return result;
		local.accel_cal_enable = st->accel_cal_enable;
	}
	if (local.gyro_cal_enable != st->gyro_cal_enable) {
		result = inv_enable_gyro_cal_V3(st, st->gyro_cal_enable);
		if (result)
			return result;
		local.gyro_cal_enable = st->gyro_cal_enable;
	}
	if (local.compass_cal_enable != st->compass_cal_enable) {
		result = inv_enable_compass_cal_V3(st, st->compass_cal_enable);
		if (result)
			return result;
		local.compass_cal_enable = st->compass_cal_enable;
	}
	result = inv_enable_9axes_V3(st);
	if (result)
		return result;

	if (EVENT_TRIGGER == st->trigger_state) {
		result = inv_setup_events(st);
		if (result)
			return result;
	}

	result = inv_set_wom(st);

	return result;
}
static int inv_determine_engine(struct inv_mpu_state *st)
{
	int i;
	bool a_en, g_en, c_en, p_en, data_on, ped_on;
	int compass_rate, pressure_rate, nineq_rate, accel_rate, gyro_rate;

#define NINEQ_MIN_COMPASS_RATE 35
#define GEOMAG_MIN_COMPASS_RATE    70
	if (st->chip_config.debug_data_collection_mode_on) {
		st->chip_config.dmp_on = 0;
		st->eng_info[ENGINE_GYRO].divider =
			BASE_SAMPLE_RATE /
			st->chip_config.debug_data_collection_gyro_freq;
		st->eng_info[ENGINE_ACCEL].divider =
			BASE_SAMPLE_RATE /
			st->chip_config.debug_data_collection_accel_freq;
		inv_calc_engine_dur(&st->eng_info[ENGINE_GYRO]);
		inv_calc_engine_dur(&st->eng_info[ENGINE_ACCEL]);
		st->chip_config.gyro_enable = true;
		st->chip_config.accel_enable = true;
		st->sensor[SENSOR_GYRO].on = true;
		st->sensor[SENSOR_ACCEL].on = true;
		st->sensor[SENSOR_GYRO].dur = st->eng_info[ENGINE_GYRO].dur;
		st->sensor[SENSOR_ACCEL].dur = st->eng_info[ENGINE_ACCEL].dur;

		return 0;
	}
	a_en = false;
	g_en = false;
	c_en = false;
	p_en = false;
	ped_on = false;
	data_on = false;
	compass_rate = 0;
	pressure_rate = 0;
	nineq_rate = 0;
	gyro_rate = MPU_INIT_SENSOR_RATE;
	accel_rate = MPU_INIT_SENSOR_RATE;

	st->chip_config.geomag_enable = 0;
	if (st->sensor[SENSOR_NINEQ].on)
		nineq_rate = max(nineq_rate, NINEQ_MIN_COMPASS_RATE);
	else if (st->sensor[SENSOR_GEOMAG].on)
		nineq_rate = max(nineq_rate, GEOMAG_MIN_COMPASS_RATE);

	if (st->sensor[SENSOR_NINEQ].on) {
		st->sensor[SENSOR_GEOMAG].output = 0;
		st->sensor[SENSOR_GEOMAG].sample_size = 0;
		st->sensor[SENSOR_NINEQ].output  = QUAT9_SET;
		st->sensor[SENSOR_NINEQ].sample_size = QUAT9_DATA_SZ;
	} else if (st->sensor[SENSOR_GEOMAG].on) {
		st->sensor[SENSOR_NINEQ].output = 0;
		st->sensor[SENSOR_NINEQ].sample_size = 0;
		st->sensor[SENSOR_GEOMAG].output  = QUAT9_SET;
		st->sensor[SENSOR_GEOMAG].sample_size = QUAT9_DATA_SZ;
		st->chip_config.geomag_enable = 1;
	}

	for (i = 0; i < SENSOR_NUM_MAX; i++) {
		if (st->sensor[i].on) {
			data_on = true;
			a_en |= st->sensor[i].a_en;
			g_en |= st->sensor[i].g_en;
			c_en |= st->sensor[i].c_en;
			p_en |= st->sensor[i].p_en;
			if (st->sensor[i].c_en &&
				(i != SENSOR_NINEQ) &&
				(i != SENSOR_GEOMAG))
				compass_rate =
					max(compass_rate, st->sensor[i].rate);
			if (st->sensor[i].p_en)
				pressure_rate =
					max(pressure_rate, st->sensor[i].rate);
		}
	}
	if (st->chip_config.step_detector_on ||
					st->chip_config.step_indicator_on) {
		ped_on = true;
		data_on = true;
	}
	if (st->ped.on || ped_on || st->smd.on)
		st->ped.engine_on = true;
	else
		st->ped.engine_on = false;
	if (st->sensor[SENSOR_ALS].on)
		st->chip_config.als_enable = true;
	else
		st->chip_config.als_enable = false;
	if (st->ped.engine_on)
		a_en = true;

	if (data_on)
		st->chip_config.dmp_event_int_on = 0;
	else
		st->chip_config.dmp_event_int_on = 1;

	if (st->chip_config.dmp_event_int_on)
		st->chip_config.wom_on = 1;
	else
		st->chip_config.wom_on = 0;

	if (compass_rate < nineq_rate)
		compass_rate = nineq_rate;
	if (compass_rate > MAX_COMPASS_RATE)
		compass_rate = MAX_COMPASS_RATE;
	st->chip_config.compass_rate = compass_rate;
	if (st->sensor[SENSOR_NINEQ].on ||
			st->sensor[SENSOR_GEOMAG].on ||
			st->sensor[SENSOR_SIXQ].on ||
			st->sensor[SENSOR_PEDQ].on) {
		/* if 6 Q or 9 Q is on, set gyro/accel to default rate */
		accel_rate = MPU_DEFAULT_DMP_FREQ;
		gyro_rate  = MPU_DEFAULT_DMP_FREQ;
	} else {
		if (st->ped.engine_on) {
			if (st->sensor[SENSOR_ACCEL].on) {
				if (st->sensor[SENSOR_ACCEL].rate <
							PEDOMETER_FREQ) {
					accel_rate = PEDOMETER_FREQ;
				} else {
					accel_rate =
						st->sensor[SENSOR_ACCEL].rate;
				}
			} else {
				accel_rate = PEDOMETER_FREQ;
			}
		} else {
			accel_rate = st->sensor[SENSOR_ACCEL].rate;
		}
	}
	if (a_en)
		gyro_rate = max(gyro_rate, PEDOMETER_FREQ);
	if (st->sensor[SENSOR_GYRO].on)
		gyro_rate = max(gyro_rate, st->sensor[SENSOR_GYRO].rate);
	if (st->sensor[SENSOR_CALIB_GYRO].on)
		gyro_rate = max(gyro_rate, st->sensor[SENSOR_CALIB_GYRO].rate);

	st->eng_info[ENGINE_GYRO].running_rate = gyro_rate;
	st->eng_info[ENGINE_ACCEL].running_rate = accel_rate;
	st->eng_info[ENGINE_PRESSURE].running_rate = MPU_DEFAULT_DMP_FREQ;
	st->eng_info[ENGINE_I2C].running_rate = compass_rate;
	/* engine divider for pressure and compass is set later */
	st->eng_info[ENGINE_GYRO].divider =
				(BASE_SAMPLE_RATE / MPU_DEFAULT_DMP_FREQ) *
				(MPU_DEFAULT_DMP_FREQ /
				st->eng_info[ENGINE_GYRO].running_rate);
	st->eng_info[ENGINE_ACCEL].divider =
				(BASE_SAMPLE_RATE / MPU_DEFAULT_DMP_FREQ) *
				(MPU_DEFAULT_DMP_FREQ /
				st->eng_info[ENGINE_ACCEL].running_rate);

	inv_calc_engine_dur(&st->eng_info[ENGINE_GYRO]);
	inv_calc_engine_dur(&st->eng_info[ENGINE_ACCEL]);

	if (st->debug_determine_engine_on)
		return 0;

	st->chip_config.gyro_enable = g_en;
	st->chip_config.accel_enable = a_en;
	st->chip_config.compass_enable = c_en;
	st->chip_config.pressure_enable = p_en;
	st->chip_config.dmp_on = 1;
	/* if gyro is enabled, geomag become 9 axes */
	if (g_en)
		st->chip_config.geomag_enable = 0;
	else
		st->chip_config.geomag_enable = 1;
	if (c_en || st->chip_config.als_enable ||
					(p_en && (ICM20628 == st->chip_type)))
		st->chip_config.slave_enable = 1;
	else
		st->chip_config.slave_enable = 0;

	if (a_en) {
		st->gyro_cal_enable = 1;
		st->accel_cal_enable = 1;
	} else {
		st->gyro_cal_enable = 0;
		st->accel_cal_enable = 0;
	}
	if (c_en)
		st->compass_cal_enable = 1;
	else
		st->compass_cal_enable = 0;

	/* setting up accuracy output */
	if (st->sensor[SENSOR_ACCEL].on || st->sensor[SENSOR_NINEQ].on ||
            st->sensor[SENSOR_SIXQ].on)
		st->sensor_accuracy[SENSOR_ACCEL_ACCURACY].on = true;
	else
		st->sensor_accuracy[SENSOR_ACCEL_ACCURACY].on = false;

	if (st->sensor[SENSOR_CALIB_GYRO].on || st->sensor[SENSOR_NINEQ].on ||
            st->sensor[SENSOR_SIXQ].on)
		st->sensor_accuracy[SENSOR_GYRO_ACCURACY].on  = true;
	else
		st->sensor_accuracy[SENSOR_GYRO_ACCURACY].on = false;

	if (st->sensor[SENSOR_CALIB_COMPASS].on || st->sensor[SENSOR_NINEQ].on)
		st->sensor_accuracy[SENSOR_COMPASS_ACCURACY].on = true;
	else
		st->sensor_accuracy[SENSOR_COMPASS_ACCURACY].on = false;

	inv_set_master_delay(st);

	return 0;
}

/*
 *  set_inv_enable() - enable function.
 */
int set_inv_enable(struct iio_dev *indio_dev)
{
	struct inv_mpu_state *st = iio_priv(indio_dev);
	int result;

	inv_determine_engine(st);
	result = inv_switch_power_in_lp(st, true);
	if (result)
		return result;

	result = inv_stop_dmp(st);
	if (result)
		return result;
	result = inv_set_rate(st);
	if (result) {
		pr_err("inv_set_rate error\n");
		return result;
	}
	if (st->chip_config.dmp_on) {
		result = inv_setup_dmp(st);
		if (result) {
			pr_err("setup dmp error\n");
			return result;
		}
	}
	result = inv_turn_on_engine(st);
	if (result) {
		pr_err("inv_turn_on_engine error\n");
		return result;
	}
	result = inv_set_fifo_size(st);
	if (result) {
		pr_err("inv_set_fifo_size error\n");
		return result;
	}
	result = inv_set_fake_secondary(st);
	if (result)
		return result;

	result = inv_reset_fifo(indio_dev, false);
	if (result)
		return result;
	result = inv_switch_power_in_lp(st, false);

	if ((!st->chip_config.gyro_enable) &&
		(!st->chip_config.accel_enable) &&
		(!st->chip_config.slave_enable) &&
		(!st->chip_config.pressure_enable)) {
		inv_set_power(st, false);
		return 0;
	}

	return result;
}

void inv_init_sensor_struct(struct inv_mpu_state *st)
{
	int i;

	for (i = 0; i < SENSOR_NUM_MAX; i++)
		st->sensor[i].rate = MPU_INIT_SENSOR_RATE;

	st->sensor[SENSOR_ACCEL].sample_size         = ACCEL_DATA_SZ;
	st->sensor[SENSOR_GYRO].sample_size          = GYRO_DATA_SZ;
	st->sensor[SENSOR_COMPASS].sample_size       = CPASS_DATA_SZ;
	st->sensor[SENSOR_ALS].sample_size           = ALS_DATA_SZ;
	st->sensor[SENSOR_PRESSURE].sample_size      = PRESSURE_DATA_SZ;
	st->sensor[SENSOR_SIXQ].sample_size          = QUAT6_DATA_SZ;
	st->sensor[SENSOR_PEDQ].sample_size          = PQUAT6_DATA_SZ;
	st->sensor[SENSOR_CALIB_GYRO].sample_size    = GYRO_CALIBR_DATA_SZ;
	st->sensor[SENSOR_CALIB_COMPASS].sample_size = CPASS_CALIBR_DATA_SZ;
	st->sensor[SENSOR_NINEQ].sample_size         = QUAT9_DATA_SZ;
	st->sensor[SENSOR_GEOMAG].sample_size        = QUAT9_DATA_SZ;

	st->sensor[SENSOR_ACCEL].odr_addr         = ODR_ACCEL;
	st->sensor[SENSOR_GYRO].odr_addr          = ODR_GYRO;
	st->sensor[SENSOR_COMPASS].odr_addr       = ODR_CPASS;
	st->sensor[SENSOR_ALS].odr_addr           = ODR_ALS;
	st->sensor[SENSOR_PRESSURE].odr_addr      = ODR_PRESSURE;
	st->sensor[SENSOR_SIXQ].odr_addr          = ODR_QUAT6;
	st->sensor[SENSOR_PEDQ].odr_addr          = ODR_PQUAT6;
	st->sensor[SENSOR_CALIB_GYRO].odr_addr    = ODR_GYRO_CALIBR;
	st->sensor[SENSOR_CALIB_COMPASS].odr_addr = ODR_CPASS_CALIBR;
	st->sensor[SENSOR_NINEQ].odr_addr         = ODR_QUAT9;
	st->sensor[SENSOR_GEOMAG].odr_addr         = ODR_QUAT9;

	st->sensor[SENSOR_ACCEL].counter_addr         = ODR_CNTR_ACCEL;
	st->sensor[SENSOR_GYRO].counter_addr          = ODR_CNTR_GYRO;
	st->sensor[SENSOR_COMPASS].counter_addr       = ODR_CNTR_CPASS;
	st->sensor[SENSOR_ALS].counter_addr           = ODR_CNTR_ALS;
	st->sensor[SENSOR_PRESSURE].counter_addr      = ODR_CNTR_PRESSURE;
	st->sensor[SENSOR_SIXQ].counter_addr          = ODR_CNTR_QUAT6;
	st->sensor[SENSOR_PEDQ].counter_addr          = ODR_CNTR_PQUAT6;
	st->sensor[SENSOR_CALIB_GYRO].counter_addr    = ODR_CNTR_GYRO_CALIBR;
	st->sensor[SENSOR_CALIB_COMPASS].counter_addr = ODR_CNTR_CPASS_CALIBR;
	st->sensor[SENSOR_NINEQ].counter_addr         = ODR_CNTR_QUAT9;
	st->sensor[SENSOR_GEOMAG].counter_addr        = ODR_CNTR_QUAT9;

	st->sensor[SENSOR_ACCEL].output         = ACCEL_SET;
	st->sensor[SENSOR_GYRO].output          = GYRO_SET;
	st->sensor[SENSOR_COMPASS].output       = CPASS_SET;
	st->sensor[SENSOR_ALS].output           = ALS_SET;
	st->sensor[SENSOR_PRESSURE].output      = PRESSURE_SET;
	st->sensor[SENSOR_SIXQ].output          = QUAT6_SET;
	st->sensor[SENSOR_PEDQ].output          = PQUAT6_SET;
	st->sensor[SENSOR_CALIB_GYRO].output    = GYRO_CALIBR_SET;
	st->sensor[SENSOR_CALIB_COMPASS].output = CPASS_CALIBR_SET;
	st->sensor[SENSOR_NINEQ].output         = QUAT9_SET;
	st->sensor[SENSOR_GEOMAG].output        = QUAT9_SET;

	st->sensor[SENSOR_ACCEL].header         = ACCEL_HDR;
	st->sensor[SENSOR_GYRO].header          = GYRO_HDR;
	st->sensor[SENSOR_COMPASS].header       = COMPASS_HDR;
	st->sensor[SENSOR_ALS].header           = ALS_HDR;
	st->sensor[SENSOR_PRESSURE].header      = PRESSURE_HDR;
	st->sensor[SENSOR_SIXQ].header          = SIXQUAT_HDR;
	st->sensor[SENSOR_PEDQ].header          = PEDQUAT_HDR;
	st->sensor[SENSOR_CALIB_GYRO].header    = GYRO_CALIB_HDR;
	st->sensor[SENSOR_CALIB_COMPASS].header = COMPASS_CALIB_HDR;
	st->sensor[SENSOR_NINEQ].header         = NINEQUAT_HDR;
	st->sensor[SENSOR_GEOMAG].header         = NINEQUAT_HDR;

	st->sensor[SENSOR_ACCEL].a_en           = true;
	st->sensor[SENSOR_GYRO].a_en            = false;
	st->sensor[SENSOR_COMPASS].a_en         = false;
	st->sensor[SENSOR_ALS].a_en             = false;
	st->sensor[SENSOR_PRESSURE].a_en        = false;
	st->sensor[SENSOR_SIXQ].a_en            = true;
	st->sensor[SENSOR_PEDQ].a_en            = true;
	st->sensor[SENSOR_CALIB_GYRO].a_en      = false;
	st->sensor[SENSOR_CALIB_COMPASS].a_en   = false;
	st->sensor[SENSOR_NINEQ].a_en           = true;
	st->sensor[SENSOR_GEOMAG].a_en           = true;

	st->sensor[SENSOR_ACCEL].g_en         = false;
	st->sensor[SENSOR_GYRO].g_en          = true;
	st->sensor[SENSOR_COMPASS].g_en       = false;
	st->sensor[SENSOR_ALS].g_en           = false;
	st->sensor[SENSOR_PRESSURE].g_en      = false;
	st->sensor[SENSOR_SIXQ].g_en          = true;
	st->sensor[SENSOR_PEDQ].g_en          = true;
	st->sensor[SENSOR_CALIB_GYRO].g_en    = true;
	st->sensor[SENSOR_CALIB_COMPASS].g_en = false;
	st->sensor[SENSOR_NINEQ].g_en         = true;
	st->sensor[SENSOR_GEOMAG].g_en        = false;

	st->sensor[SENSOR_ACCEL].c_en         = false;
	st->sensor[SENSOR_GYRO].c_en          = false;
	st->sensor[SENSOR_COMPASS].c_en       = true;
	st->sensor[SENSOR_ALS].c_en           = false;
	st->sensor[SENSOR_PRESSURE].c_en      = false;
	st->sensor[SENSOR_SIXQ].c_en          = false;
	st->sensor[SENSOR_PEDQ].c_en          = false;
	st->sensor[SENSOR_CALIB_GYRO].c_en    = false;
	st->sensor[SENSOR_CALIB_COMPASS].c_en = true;
	st->sensor[SENSOR_NINEQ].c_en         = true;
	st->sensor[SENSOR_GEOMAG].c_en        = true;

	st->sensor[SENSOR_ACCEL].p_en         = false;
	st->sensor[SENSOR_GYRO].p_en          = false;
	st->sensor[SENSOR_COMPASS].p_en       = false;
	st->sensor[SENSOR_ALS].p_en           = false;
	st->sensor[SENSOR_PRESSURE].p_en      = true;
	st->sensor[SENSOR_SIXQ].p_en          = false;
	st->sensor[SENSOR_PEDQ].p_en          = false;
	st->sensor[SENSOR_CALIB_GYRO].p_en    = false;
	st->sensor[SENSOR_CALIB_COMPASS].p_en = false;
	st->sensor[SENSOR_NINEQ].p_en         = false;
	st->sensor[SENSOR_GEOMAG].p_en        = false;

	st->sensor[SENSOR_ACCEL].engine_base         = ENGINE_ACCEL;
	st->sensor[SENSOR_GYRO].engine_base          = ENGINE_GYRO;
	st->sensor[SENSOR_COMPASS].engine_base       = ENGINE_I2C;
	st->sensor[SENSOR_ALS].engine_base           = ENGINE_I2C;
	st->sensor[SENSOR_PRESSURE].engine_base      = ENGINE_I2C;
	st->sensor[SENSOR_SIXQ].engine_base          = ENGINE_GYRO;
	st->sensor[SENSOR_PEDQ].engine_base          = ENGINE_GYRO;
	st->sensor[SENSOR_CALIB_GYRO].engine_base    = ENGINE_GYRO;
	st->sensor[SENSOR_CALIB_COMPASS].engine_base = ENGINE_I2C;
	st->sensor[SENSOR_NINEQ].engine_base         = ENGINE_GYRO;
	st->sensor[SENSOR_GEOMAG].engine_base        = ENGINE_ACCEL;

	st->sensor_accuracy[SENSOR_ACCEL_ACCURACY].sample_size =
							ACCEL_ACCURACY_SZ;
	st->sensor_accuracy[SENSOR_GYRO_ACCURACY].sample_size  =
							GYRO_ACCURACY_SZ;
	st->sensor_accuracy[SENSOR_COMPASS_ACCURACY].sample_size =
							CPASS_ACCURACY_SZ;

	st->sensor_accuracy[SENSOR_ACCEL_ACCURACY].output =
							ACCEL_ACCURACY_SET;
	st->sensor_accuracy[SENSOR_GYRO_ACCURACY].output =
							GYRO_ACCURACY_SET;
	st->sensor_accuracy[SENSOR_COMPASS_ACCURACY].output =
							CPASS_ACCURACY_SET;

	st->sensor_accuracy[SENSOR_ACCEL_ACCURACY].header =
							ACCEL_ACCURACY_HDR;
	st->sensor_accuracy[SENSOR_GYRO_ACCURACY].header =
							GYRO_ACCURACY_HDR;
	st->sensor_accuracy[SENSOR_COMPASS_ACCURACY].header =
							CPASS_ACCURACY_HDR;

}
