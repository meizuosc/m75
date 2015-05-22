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
#include <linux/spinlock.h>

#include "inv_mpu_iio.h"
#include "linux/iio/sysfs.h"
#include "inv_test/inv_counters.h"

#ifdef CONFIG_DTS_INV_MPU_IIO
#include "inv_mpu_dts.h"
#endif

#include <linux/dma-mapping.h>

static const struct inv_hw_s hw_info[INV_NUM_PARTS] = {
	{128, "ICM20628"},
	{128, "ICM20728"},
};
static int debug_mem_read_addr = 0x900;
static char debug_reg_addr = 0x6;

static int inv_set_dmp(struct inv_mpu_state *st)
{
	int result;

	result = inv_set_bank(st, BANK_SEL_2);
	if (result)
		return result;
	result = inv_plat_single_write(st, REG_PRGM_START_ADDRH,
							DMP_START_ADDR >> 8);
	if (result)
		return result;
	result = inv_plat_single_write(st, REG_PRGM_START_ADDRH + 1,
							DMP_START_ADDR & 0xff);
	if (result)
		return result;
	result = inv_set_bank(st, BANK_SEL_0);

	return result;
}

static int inv_calc_gyro_sf(s8 pll)
{
	int a, r;
	int value, t;

	t = 102870L + 81L * pll;
	a = (1L << 30) / t;
	r = (1L << 30) - a * t;
	value = a * 797 * DMP_DIVIDER;
	value += (s64)((a * 1011387LL * DMP_DIVIDER) >> 20);
	value += r * 797L * DMP_DIVIDER / t;
	value += (s32)((s64)((r * 1011387LL * DMP_DIVIDER) >> 20)) / t;
	value <<= 1;

	return value;
}

static int inv_read_timebase(struct inv_mpu_state *st)
{
	int result;
	u8 d;
	s8 t;

	result = inv_set_bank(st, BANK_SEL_1);
	if (result)
		return result;
	result = inv_plat_read(st, REG_TIMEBASE_CORRECTION_PLL, 1, &d);
	if (result)
		return result;
	t = abs(d & 0x7f);
	if (d & 0x80)
		t = -t;

	st->eng_info[ENGINE_ACCEL].base_time = NSEC_PER_SEC;
	/* talor expansion to calculate base time unit */
	st->eng_info[ENGINE_GYRO].base_time = NSEC_PER_SEC - t * 769903 +
						((t * 769903) / 1270) * t;
	st->eng_info[ENGINE_PRESSURE].base_time = NSEC_PER_SEC;
	st->eng_info[ENGINE_I2C].base_time  = NSEC_PER_SEC;

	st->eng_info[ENGINE_ACCEL].orig_rate = BASE_SAMPLE_RATE;
	st->eng_info[ENGINE_GYRO].orig_rate = BASE_SAMPLE_RATE;
	st->eng_info[ENGINE_PRESSURE].orig_rate = MAX_PRS_RATE;
	st->eng_info[ENGINE_I2C].orig_rate = BASE_SAMPLE_RATE;

	st->gyro_sf = inv_calc_gyro_sf(t);
	result = inv_set_bank(st, BANK_SEL_0);

	return result;
}

static int inv_set_gyro_sf(struct inv_mpu_state *st)
{
	int result;

	result = inv_set_bank(st, BANK_SEL_2);
	if (result)
		return result;
	result = inv_plat_single_write(st, REG_GYRO_CONFIG_1,
				(st->chip_config.fsr << SHIFT_GYRO_FS_SEL) | 1);
	if (result)
		return result;

	result = inv_set_bank(st, BANK_SEL_0);

	return result;
}

#define ENABLE_ACCEL_LPF
#ifdef ENABLE_ACCEL_LPF
static int inv_set_accel_params(struct inv_mpu_state *st, unsigned char accel_fsr, unsigned char averaging)
{
	int result = 0;
	unsigned char accel_config_1_reg;
    	unsigned char accel_config_2_reg;
    	unsigned char dec3_cfg;
    
	// select reg bank 2
	result = inv_set_bank(st, BANK_SEL_2);
	if (result)
		return result;
    
	result = inv_plat_read(st, REG_ACCEL_CONFIG, 1, &accel_config_1_reg);
	if (result)
		return result;
    
	accel_config_1_reg &= 0xC0;
    if(averaging > 1)
        accel_config_1_reg |= (7 << 3) | (accel_fsr << 1) | 1;   //fchoice = 1, filter = 7.
    else
        accel_config_1_reg |= (accel_fsr << 1);  //fchoice = 0, filter = 0.
    
	result = inv_plat_single_write(st, REG_ACCEL_CONFIG, accel_config_1_reg);
	if (result)
		return result;
    
    switch(averaging) {
        case 1:
            dec3_cfg = 0;
            break;
        case 4:
            dec3_cfg = 0;
            break;
        case 8:
            dec3_cfg = 1;
            break;
        case 16:
            dec3_cfg = 2;
            break;
        case 32:
            dec3_cfg = 3;
            break;
        default:
            dec3_cfg = 0;
            break;
    }
    
    result = inv_plat_read(st, REG_ACCEL_CONFIG_2, 1, &accel_config_2_reg);
	if (result)
		return result;
    
    accel_config_2_reg |=  dec3_cfg;
    result = inv_plat_single_write(st, REG_ACCEL_CONFIG_2, accel_config_2_reg);
   if(result)
   	return result;
   result = inv_set_bank(st, BANK_SEL_0);
   return result;
   
}

#endif


static int inv_set_accel_sf(struct inv_mpu_state *st)
{
	int result;
#ifdef ENABLE_ACCEL_LPF
	result = inv_set_accel_params(st, st->chip_config.accel_fs, 16);
#else
	result = inv_set_bank(st, BANK_SEL_2);
	if (result)
		return result;
	result = inv_plat_single_write(st, REG_ACCEL_CONFIG,
			(st->chip_config.accel_fs << SHIFT_ACCEL_FS) | 0);
	if (result)
		return result;

	result = inv_set_bank(st, BANK_SEL_0);
#endif
	return result;
}

static int inv_set_secondary(struct inv_mpu_state *st)
{
	int r;

	r = inv_set_bank(st, BANK_SEL_3);
	if (r)
		return r;
	r = inv_plat_single_write(st, REG_I2C_MST_CTRL, BIT_I2C_MST_P_NSR);
	if (r)
		return r;

	r = inv_plat_single_write(st, REG_I2C_MST_ODR_CONFIG,
							MIN_MST_ODR_CONFIG);
	if (r)
		return r;

	r = inv_set_bank(st, BANK_SEL_0);

	return r;
}
static int inv_set_odr_sync(struct inv_mpu_state *st)
{
	int r;

	r = inv_set_bank(st, BANK_SEL_2);
	if (r)
		return r;
	r = inv_plat_single_write(st, REG_MOD_CTRL_USR, BIT_ODR_SYNC);
	if (r)
		return r;
	r = inv_set_bank(st, BANK_SEL_0);

	return r;

}
static int inv_init_secondary(struct inv_mpu_state *st)
{
	st->slv_reg[0].addr = REG_I2C_SLV0_ADDR;
	st->slv_reg[0].reg  = REG_I2C_SLV0_REG;
	st->slv_reg[0].ctrl = REG_I2C_SLV0_CTRL;
	st->slv_reg[0].d0   = REG_I2C_SLV0_DO;

	st->slv_reg[1].addr = REG_I2C_SLV1_ADDR;
	st->slv_reg[1].reg  = REG_I2C_SLV1_REG;
	st->slv_reg[1].ctrl = REG_I2C_SLV1_CTRL;
	st->slv_reg[1].d0   = REG_I2C_SLV1_DO;

	st->slv_reg[2].addr = REG_I2C_SLV2_ADDR;
	st->slv_reg[2].reg  = REG_I2C_SLV2_REG;
	st->slv_reg[2].ctrl = REG_I2C_SLV2_CTRL;
	st->slv_reg[2].d0   = REG_I2C_SLV2_DO;

	st->slv_reg[3].addr = REG_I2C_SLV3_ADDR;
	st->slv_reg[3].reg  = REG_I2C_SLV3_REG;
	st->slv_reg[3].ctrl = REG_I2C_SLV3_CTRL;
	st->slv_reg[3].d0   = REG_I2C_SLV3_DO;

	return 0;
}

static int inv_init_config(struct inv_mpu_state *st)
{
	int res;

	st->batch.overflow_on = 0;
	st->chip_config.fsr = MPU_INIT_GYRO_SCALE;
	st->chip_config.accel_fs = MPU_INIT_ACCEL_SCALE;
	st->ped.int_thresh = MPU_INIT_PED_INT_THRESH;
	st->ped.step_thresh = MPU_INIT_PED_STEP_THRESH;
	st->chip_config.low_power_gyro_on = 1;
	st->suspend_state = false;
	st->firmware = 0;
	st->chip_config.debug_data_collection_gyro_freq = BASE_SAMPLE_RATE;
	st->chip_config.debug_data_collection_accel_freq = BASE_SAMPLE_RATE;

	inv_init_secondary(st);

	res = inv_read_timebase(st);
	if (res)
		return res;
	res = inv_set_dmp(st);
	if (res)
		return res;

	res = inv_set_gyro_sf(st);
	if (res)
		return res;
	res = inv_set_accel_sf(st);
	if (res)
		return res;

	res = inv_set_odr_sync(st);
	if (res)
		return res;

	res = inv_set_secondary(st);

	return res;
}

/*
 * inv_firmware_loaded_store() -  calling this function will change
 *                        firmware load
 */
static ssize_t inv_firmware_loaded_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	int result, data;

	result = kstrtoint(buf, 10, &data);
	if (result)
		return -EINVAL;

	if (data)
		return -EINVAL;
	st->chip_config.firmware_loaded = 0;

	return count;

}

static ssize_t _dmp_bias_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int result, data;

	int ret,dmp_bias_x,dmp_bias_y,dmp_bias_z;

	if (!st->chip_config.firmware_loaded)
		return -EINVAL;

	result = inv_switch_power_in_lp(st, true);
	if (result)
		return result;

	result = kstrtoint(buf, 10, &data);
	if (result)
		goto dmp_bias_store_fail;

	pr_debug("%s invn %s\n",__func__, buf);
	pr_debug("%s invn bias_type:%lld, bias:%d\n",__func__,this_attr->address, data);


	switch (this_attr->address) {
	case ATTR_DMP_ACCEL_X_DMP_BIAS:// 4
		if (data)
			st->sensor_acurracy_flag[SENSOR_ACCEL_ACCURACY] = DEFAULT_ACCURACY;
		result = write_be32_to_mem(st, data, ACCEL_BIAS_X);
#if 0
		ret = read_be32_from_mem(st, &dmp_bias_x, ACCEL_BIAS_X);
		if(ret==0) {
			printk("%s invn accel x dmp bias %d, data:%d\n",__func__, dmp_bias_x, data);
		} else {
			printk("%s invn accel x dmp bias %d  failed\n",__func__, dmp_bias_x);
		}
#endif
		if (result)
			goto dmp_bias_store_fail;
		st->input_accel_dmp_bias[0] = data;
		break;
	case ATTR_DMP_ACCEL_Y_DMP_BIAS:// 5
		result = write_be32_to_mem(st, data, ACCEL_BIAS_Y);
#if 0
		ret = read_be32_from_mem(st, &dmp_bias_y, ACCEL_BIAS_Y);
		if(ret==0) {
			printk("%s invn accel y dmp bias %d, data:%d\n",__func__, dmp_bias_y, data);
		} else {
			printk("%s invn accel y dmp bias %d  failed\n",__func__, dmp_bias_y);
		}
#endif
		if (result)
			goto dmp_bias_store_fail;
		st->input_accel_dmp_bias[1] = data;
		break;
	case ATTR_DMP_ACCEL_Z_DMP_BIAS:// 6
		result = write_be32_to_mem(st, data, ACCEL_BIAS_Z);
#if 0
		ret = read_be32_from_mem(st, &dmp_bias_z, ACCEL_BIAS_Z);
		if(ret==0) {
			printk("%s invn accel z dmp bias %d, data:%d\n",__func__, dmp_bias_z, data);
		} else {
			printk("%s invn accel z dmp bias %d  failed\n",__func__, dmp_bias_z);
		}
#endif
		if (result)
			goto dmp_bias_store_fail;
		st->input_accel_dmp_bias[2] = data;
		break;
	case ATTR_DMP_GYRO_X_DMP_BIAS:
		if (data)
			st->sensor_acurracy_flag[SENSOR_GYRO_ACCURACY] = DEFAULT_ACCURACY;
		result = write_be32_to_mem(st, data, GYRO_BIAS_X);
		if (result)
			goto dmp_bias_store_fail;
		st->input_gyro_dmp_bias[0] = data;
		break;
	case ATTR_DMP_GYRO_Y_DMP_BIAS:
		result = write_be32_to_mem(st, data, GYRO_BIAS_Y);
		if (result)
			goto dmp_bias_store_fail;
		st->input_gyro_dmp_bias[1] = data;
		break;
	case ATTR_DMP_GYRO_Z_DMP_BIAS:
		result = write_be32_to_mem(st, data, GYRO_BIAS_Z);
		if (result)
			goto dmp_bias_store_fail;
		st->input_gyro_dmp_bias[2] = data;
		break;
	case ATTR_DMP_MAGN_X_DMP_BIAS:
		if (data)
			st->sensor_acurracy_flag[SENSOR_COMPASS_ACCURACY] = 3;
		result = write_be32_to_mem(st, data, CPASS_BIAS_X);
		if (result)
			goto dmp_bias_store_fail;
		st->input_compass_dmp_bias[0] = data;
		break;
	case ATTR_DMP_MAGN_Y_DMP_BIAS:
		result = write_be32_to_mem(st, data, CPASS_BIAS_Y);
		if (result)
			goto dmp_bias_store_fail;
		st->input_compass_dmp_bias[1] = data;
		break;
	case ATTR_DMP_MAGN_Z_DMP_BIAS:
		result = write_be32_to_mem(st, data, CPASS_BIAS_Z);
		if (result)
			goto dmp_bias_store_fail;
		st->input_compass_dmp_bias[2] = data;
		break;
	case ATTR_DMP_MISC_GYRO_RECALIBRATION:
		result = write_be32_to_mem(st, 0, GYRO_LAST_TEMPR);
		if (result)
			goto dmp_bias_store_fail;
		break;
	case ATTR_DMP_MISC_ACCEL_RECALIBRATION:
	{
		u8 d[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
		int i;
		u32 d1[] = {0, 0, 0, 0,
				0, 0, 0, 0, 0, 0,
				3276800, 3276800, 3276800, 3276800};
		u32 *w_d;

		if (data) {
			result = inv_write_2bytes(st, ACCEL_CAL_RESET, 1);
			if (result)
				goto dmp_bias_store_fail;
			result = mem_w(ACCEL_PRE_SENSOR_DATA, ARRAY_SIZE(d), d);
			w_d = d1;
		} else {
			w_d = st->accel_covariance;
		}
		for (i = 0; i < ARRAY_SIZE(d1); i++) {
			result = write_be32_to_mem(st, w_d[i],
					ACCEL_COVARIANCE + i * sizeof(int));
			if (result)
				goto dmp_bias_store_fail;
		}

		break;
	}
	case ATTR_DMP_MISC_COMPASS_RECALIBRATION:
	{
		u32 d[] = {0, 0, 0, 0,
				14745600, 14745600, 14745600, 14745600,
				0, 0, 0, 0,
				0, 0};
		int i, *w_d;

		if (data) {
			w_d = d;
		} else {
			w_d = st->compass_covariance;
			for (i = 0; i < ARRAY_SIZE(d); i++) {
				result = write_be32_to_mem(st,
						st->curr_compass_covariance[i],
						CPASS_COVARIANCE_CUR +
						i * sizeof(int));
				if (result)
					goto dmp_bias_store_fail;
			}
			result = write_be32_to_mem(st, st->ref_mag_3d,
							CPASS_REF_MAG_3D);
			if (result)
				goto dmp_bias_store_fail;
		}

		for (i = 0; i < ARRAY_SIZE(d); i++) {
			result = write_be32_to_mem(st, w_d[i],
				CPASS_COVARIANCE + i * sizeof(int));
			if (result)
				goto dmp_bias_store_fail;
		}
		break;
	}
	case ATTR_DMP_PARAMS_ACCEL_CALIBRATION_THRESHOLD:
		result = write_be32_to_mem(st, data, ACCEL_VARIANCE_THRESH);
		if (result)
			goto dmp_bias_store_fail;
		st->accel_calib_threshold = data;
		break;
	/* this serves as a divider of calibration rate, 0->225, 3->55 */
	case ATTR_DMP_PARAMS_ACCEL_CALIBRATION_RATE:
		if (data < 0)
			data = 0;
		result = inv_write_2bytes(st, ACCEL_CAL_RATE, data);
		if (result)
			goto dmp_bias_store_fail;
		st->accel_calib_rate = data;
		break;
	case ATTR_DMP_DEBUG_MEM_READ:
		debug_mem_read_addr = data;
		break;
	case ATTR_DMP_DEBUG_MEM_WRITE:
		inv_write_2bytes(st, debug_mem_read_addr, data);
		break;
	default:
		break;
	}

dmp_bias_store_fail:
	if (result)
		return result;
	result = inv_switch_power_in_lp(st, false);
	if (result)
		return result;

	return count;
}

static ssize_t inv_dmp_bias_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	int result;

	mutex_lock(&indio_dev->mlock);
	result = _dmp_bias_store(dev, attr, buf, count);
	mutex_unlock(&indio_dev->mlock);

	return result;
}


#ifdef READ_BIAS_FROM_NVRAM
static ssize_t inv_accel_calibbias_bias_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int result = 0, data;

	//	mutex_lock(&indio_dev->mlock);
	result = kstrtoint(buf, 10, &data);
	if (result)
		goto calibbias_bias_store_fail;

	switch (this_attr->address) {
	case ATTR_ACCEL_X_CALIBBIAS:
		st->accel_bias[0] = data;
		printk("%s accel x calibbias %d\n",__func__, st->accel_bias[0]);
		break;
	case ATTR_ACCEL_Y_CALIBBIAS:
		st->accel_bias[1] = data;
		printk("%s accel y calibbias %d\n",__func__, st->accel_bias[1]);
		break;
	case ATTR_ACCEL_Z_CALIBBIAS:
		st->accel_bias[2] = data;
		printk("%s accel z calibbias %d\n",__func__, st->accel_bias[2]);
		break;
	default:
		printk("%s the store bias case is error!\n",__func__);
	}

//	mutex_unlock(&indio_dev->mlock);
calibbias_bias_store_fail:
	if(result)
		return result;

	return count;
}

#endif



static ssize_t inv_debug_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int result, data;

	result = kstrtoint(buf, 10, &data);
	if (result)
		return result;
	switch (this_attr->address) {
	case ATTR_DMP_LP_EN_OFF:
		st->chip_config.lp_en_mode_off = !!data;
		inv_switch_power_in_lp(st, !!data);
		break;
	case ATTR_DMP_CYCLE_MODE_OFF:
		st->chip_config.cycle_mode_off = !!data;
		inv_turn_off_cycle_mode(st, !!data);
	case ATTR_DMP_CLK_SEL:
		st->chip_config.clk_sel = !!data;
		inv_switch_power_in_lp(st, !!data);
		break;
	case ATTR_DEBUG_REG_ADDR:
		debug_reg_addr = data;
		break;
	case ATTR_DEBUG_REG_WRITE:
		inv_plat_single_write(st, debug_reg_addr, data);
		break;
	case ATTR_DEBUG_WRITE_CFG:
		break;
	}
	return count;
}
static ssize_t _misc_attr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int result, data;

	if (!st->chip_config.firmware_loaded)
		return -EINVAL;
	result = inv_switch_power_in_lp(st, true);
	if (result)
		return result;
	result = kstrtoint(buf, 10, &data);
	if (result)
		return result;
	switch (this_attr->address) {
	case ATTR_DMP_LOW_POWER_GYRO_ON:
		st->chip_config.low_power_gyro_on = !!data;
		break;
	case ATTR_DEBUG_DATA_COLLECTION_MODE:
		st->chip_config.debug_data_collection_mode_on = !!data;
		break;
	case ATTR_DEBUG_DATA_COLLECTION_GYRO_RATE:
		st->chip_config.debug_data_collection_gyro_freq = data;
		break;
	case ATTR_DEBUG_DATA_COLLECTION_ACCEL_RATE:
		st->chip_config.debug_data_collection_accel_freq = data;
		break;
	case ATTR_DMP_REF_MAG_3D:
		st->ref_mag_3d = data;
		return 0;
	case ATTR_DMP_DEBUG_DETERMINE_ENGINE_ON:
		st->debug_determine_engine_on = !!data;
		break;
	case ATTR_GYRO_SCALE:
		if (data > 3)
			return -EINVAL;
		st->chip_config.fsr = data;
		result = inv_set_gyro_sf(st);

		return result;
	case ATTR_ACCEL_SCALE:
		if (data > 3)
			return -EINVAL;
		st->chip_config.accel_fs = data;
		result = inv_set_accel_sf(st);

		return result;
	case ATTR_DMP_PED_INT_ON:
		result = inv_write_cntl(st, PEDOMETER_INT_EN << 8, !!data,
							MOTION_EVENT_CTL);
		if (result)
			return result;
		st->ped.int_on = !!data;

		return 0;
	case ATTR_DMP_PED_STEP_THRESH:
		result = inv_write_2bytes(st, PEDSTD_SB, data);
		if (result)
			return result;
		st->ped.step_thresh = data;

		return 0;
	case ATTR_DMP_PED_INT_THRESH:
		result = inv_write_2bytes(st, PEDSTD_SB2, data);
		if (result)
			return result;
		st->ped.int_thresh = data;

		return 0;
	case ATTR_DMP_SMD_THLD:
		if (data < 0 || data > SHRT_MAX)
			return -EINVAL;
		result = write_be32_to_mem(st, data << 16, SMD_MOT_THLD);
		if (result)
			return result;
		st->smd.threshold = data;

		return 0;
	case ATTR_DMP_SMD_DELAY_THLD:
		if (data < 0 || data > INT_MAX / MPU_DEFAULT_DMP_FREQ)
			return -EINVAL;
		result = write_be32_to_mem(st, data * MPU_DEFAULT_DMP_FREQ,
						SMD_DELAY_THLD);
		if (result)
			return result;
		st->smd.delay = data;

		return 0;
	case ATTR_DMP_SMD_DELAY_THLD2:
		if (data < 0 || data > INT_MAX / MPU_DEFAULT_DMP_FREQ)
			return -EINVAL;
		result = write_be32_to_mem(st, data * MPU_DEFAULT_DMP_FREQ,
						SMD_DELAY2_THLD);
		if (result)
			return result;
		st->smd.delay2 = data;

		return 0;
	default:
		return -EINVAL;
	}
	st->trigger_state = MISC_TRIGGER;
	result = set_inv_enable(indio_dev);

	return result;
}

/*
 * inv_misc_attr_store() -  calling this function will store current
 *                        dmp parameter settings
 */
static ssize_t inv_misc_attr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	int result;

	mutex_lock(&indio_dev->mlock);
	result = _misc_attr_store(dev, attr, buf, count);
	mutex_unlock(&indio_dev->mlock);
	if (result)
		return result;

	return count;
}

static ssize_t _debug_attr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int result, data;

	if (!st->chip_config.firmware_loaded)
		return -EINVAL;
	if (!st->debug_determine_engine_on)
		return -EINVAL;

	result = kstrtoint(buf, 10, &data);
	if (result)
		return result;
	switch (this_attr->address) {
	case ATTR_DMP_IN_ANGLVEL_ACCURACY_ENABLE:
		st->sensor_accuracy[SENSOR_GYRO_ACCURACY].on = !!data;
		break;
	case ATTR_DMP_IN_ACCEL_ACCURACY_ENABLE:
		st->sensor_accuracy[SENSOR_ACCEL_ACCURACY].on = !!data;
		break;
	case ATTR_DMP_IN_MAGN_ACCURACY_ENABLE:
		st->sensor_accuracy[SENSOR_COMPASS_ACCURACY].on = !!data;
		break;
	case ATTR_DMP_ACCEL_CAL_ENABLE:
		st->accel_cal_enable = !!data;
		break;
	case ATTR_DMP_GYRO_CAL_ENABLE:
		st->gyro_cal_enable = !!data;
		break;
	case ATTR_DMP_COMPASS_CAL_ENABLE:
		st->compass_cal_enable = !!data;
		break;
	case ATTR_DMP_EVENT_INT_ON:
		st->chip_config.dmp_event_int_on = !!data;
		break;
	case ATTR_DMP_ON:
		st->chip_config.dmp_on = !!data;
		break;
	case ATTR_GYRO_ENABLE:
		st->chip_config.gyro_enable = !!data;
		break;
	case ATTR_ACCEL_ENABLE:
		st->chip_config.accel_enable = !!data;
		break;
	case ATTR_COMPASS_ENABLE:
		st->chip_config.compass_enable = !!data;
		break;
	default:
		return -EINVAL;
	}
	st->trigger_state = DEBUG_TRIGGER;
	result = set_inv_enable(indio_dev);
	if (result)
		return result;

	return count;
}

/*
 * inv_debug_attr_store() -  calling this function will store current
 *                        dmp parameter settings
 */
static ssize_t inv_debug_attr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	int result;

	mutex_lock(&indio_dev->mlock);
	result = _debug_attr_store(dev, attr, buf, count);
	mutex_unlock(&indio_dev->mlock);

	return result;
}

static int inv_rate_convert(struct inv_mpu_state *st, int ind, int data)
{
	int t, out, out1, out2;

	out = MPU_INIT_SENSOR_RATE;
	if ((SENSOR_COMPASS == ind) || (SENSOR_CALIB_COMPASS == ind)) {
		if ((MSEC_PER_SEC / st->slave_compass->rate_scale) < data)
			out = MSEC_PER_SEC / st->slave_compass->rate_scale;
		else
			out = data;
	} else if (SENSOR_ALS == ind) {
		if (data > MAX_ALS_RATE)
			out = MAX_ALS_RATE;
		else
			out = data;
	} else if (SENSOR_PRESSURE == ind) {
		if (data > MAX_PRESSURE_RATE)
			out = MAX_PRESSURE_RATE;
		else
			out = data;
	} else {
		t = MPU_DEFAULT_DMP_FREQ / data;
		if (!t)
			t = 1;
		out1 = MPU_DEFAULT_DMP_FREQ / (t + 1);
		out2 = MPU_DEFAULT_DMP_FREQ / t;
		if (abs(out1 - data) < abs(out2 - data))
			out = out1;
		else
			out = out2;
	}

	return out;
}

static ssize_t inv_sensor_rate_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);

	return sprintf(buf, "%d\n", st->sensor[this_attr->address].rate);
}
static ssize_t inv_sensor_rate_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int data, rate, ind;
	int result;

	if (!st->chip_config.firmware_loaded) {
		pr_err("sensor_rate_store: firmware not loaded\n");
		return -EINVAL;
	}
	result = kstrtoint(buf, 10, &data);
	if (result)
		return -EINVAL;
	if (data <= 0) {
		pr_err("sensor_rate_store: invalid data=%d\n", data);
		return -EINVAL;
	}
	ind = this_attr->address;
	rate = inv_rate_convert(st, ind, data);
	if (rate == st->sensor[ind].rate)
		return count;
	mutex_lock(&indio_dev->mlock);
	st->sensor[ind].rate = rate;
	if (!st->sensor[ind].on) {
		mutex_unlock(&indio_dev->mlock);
		return count;
	}

	st->trigger_state = DATA_TRIGGER;
	result = set_inv_enable(indio_dev);
	mutex_unlock(&indio_dev->mlock);
	if (result)
		return result;

	return count;
}

static ssize_t inv_sensor_on_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);

	return sprintf(buf, "%d\n", st->sensor[this_attr->address].on);
}
static ssize_t inv_sensor_on_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int data, on, ind;
	int result;

	pr_debug("%s start\n",__func__);

	if (!st->chip_config.firmware_loaded) {
		pr_err("sensor_on store: firmware not loaded\n");
		return -EINVAL;
	}
	result = kstrtoint(buf, 10, &data);
	if (result)
		return -EINVAL;
	if (data < 0) {
		pr_err("sensor_on_store: invalid data=%d\n", data);
		return -EINVAL;
	}
	ind = this_attr->address;
	on = !!data;
	if (on == st->sensor[ind].on)
		return count;


	mutex_lock(&indio_dev->mlock);

	st->sensor[ind].on = on;
	if (on && (!st->sensor[ind].rate)) {
		mutex_unlock(&indio_dev->mlock);
		return count;
	}
	st->trigger_state = RATE_TRIGGER;
	result = set_inv_enable(indio_dev);

	mutex_unlock(&indio_dev->mlock);

	if (result)
		return result;


	pr_debug("%s end\n",__func__);

	return count;
}

static ssize_t _basic_attr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int data;
	int result;

	if (!st->chip_config.firmware_loaded)
		return -EINVAL;
	result = kstrtoint(buf, 10, &data);
	if (result || (data < 0))
		return -EINVAL;

	switch (this_attr->address) {
	case ATTR_DMP_PED_ON:
		if ((!!data) == st->ped.on)
			return count;
		st->ped.on = !!data;
		break;
	case ATTR_DMP_SMD_ENABLE:
		if ((!!data) == st->smd.on)
			return count;
		st->smd.on = !!data;
		break;
	case ATTR_DMP_STEP_DETECTOR_ON:
		if ((!!data) == st->chip_config.step_detector_on)
			return count;
		st->chip_config.step_detector_on = !!data;
		break;
	case ATTR_DMP_STEP_INDICATOR_ON:
		if ((!!data) == st->chip_config.step_indicator_on)
			return count;
		st->chip_config.step_indicator_on = !!data;
		break;
	case ATTR_DMP_BATCHMODE_TIMEOUT:
		if (data == st->batch.timeout)
			return count;
		st->batch.timeout = data;
		break;
	default:
		return -EINVAL;
	};

	st->trigger_state = EVENT_TRIGGER;
	result = set_inv_enable(indio_dev);
	if (result)
		return result;

	return count;
}

/*
 * inv_basic_attr_store() -  calling this function will store current
 *                        non-dmp parameter settings
 */
static ssize_t inv_basic_attr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	int result;

	mutex_lock(&indio_dev->mlock);
	result = _basic_attr_store(dev, attr, buf, count);

	mutex_unlock(&indio_dev->mlock);

	return result;
}

/*
 * inv_attr_bias_show() -  calling this function will show current
 *                        dmp gyro/accel bias.
 */
//#define DEBUG_ACCEL_RAW_DATA
#ifdef DEBUG_ACCEL_RAW_DATA
#define ACCEL_XOUT_H  0x2d
#endif

static ssize_t inv_attr_bias_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int axes, addr, result, dmp_bias;
	int sensor_type;

	switch (this_attr->address) {
	case ATTR_ANGLVEL_X_CALIBBIAS:
		return sprintf(buf, "%d\n", st->gyro_bias[0]);
	case ATTR_ANGLVEL_Y_CALIBBIAS:
		return sprintf(buf, "%d\n", st->gyro_bias[1]);
	case ATTR_ANGLVEL_Z_CALIBBIAS:
		return sprintf(buf, "%d\n", st->gyro_bias[2]);
	case ATTR_ACCEL_X_CALIBBIAS:
		return sprintf(buf, "%d\n", st->accel_bias[0]);
	case ATTR_ACCEL_Y_CALIBBIAS:
		return sprintf(buf, "%d\n", st->accel_bias[1]);
	case ATTR_ACCEL_Z_CALIBBIAS:
		return sprintf(buf, "%d\n", st->accel_bias[2]);
	case ATTR_DMP_ACCEL_X_DMP_BIAS:
		axes = 0;
		addr = ACCEL_BIAS_X;
		sensor_type = SENSOR_ACCEL;
		break;
	case ATTR_DMP_ACCEL_Y_DMP_BIAS:
		axes = 1;
		addr = ACCEL_BIAS_Y;
		sensor_type = SENSOR_ACCEL;
		break;
	case ATTR_DMP_ACCEL_Z_DMP_BIAS:
		axes = 2;
		addr = ACCEL_BIAS_Z;
		sensor_type = SENSOR_ACCEL;
		break;
	case ATTR_DMP_GYRO_X_DMP_BIAS:
		axes = 0;
		addr = GYRO_BIAS_X;
		sensor_type = SENSOR_GYRO;
		break;
	case ATTR_DMP_GYRO_Y_DMP_BIAS:
		axes = 1;
		addr = GYRO_BIAS_Y;
		sensor_type = SENSOR_GYRO;
		break;
	case ATTR_DMP_GYRO_Z_DMP_BIAS:
		axes = 2;
		addr = GYRO_BIAS_Z;
		sensor_type = SENSOR_GYRO;
		break;
	case ATTR_DMP_MAGN_X_DMP_BIAS:
		axes = 0;
		addr = CPASS_BIAS_X;
		sensor_type = SENSOR_COMPASS;
		break;
	case ATTR_DMP_MAGN_Y_DMP_BIAS:
		axes = 1;
		addr = CPASS_BIAS_Y;
		sensor_type = SENSOR_COMPASS;
		break;
	case ATTR_DMP_MAGN_Z_DMP_BIAS:
		axes = 2;
		addr = CPASS_BIAS_Z;
		sensor_type = SENSOR_COMPASS;
		break;
	default:
		return -EINVAL;
	}

	result = inv_switch_power_in_lp(st, true);
	if (result)
		return result;
	result = read_be32_from_mem(st, &dmp_bias, addr);
	if (result)
		return result;
	inv_switch_power_in_lp(st, false);
	if (SENSOR_GYRO == sensor_type)
		st->input_gyro_dmp_bias[axes] = dmp_bias;
	else if (SENSOR_ACCEL == sensor_type)
		st->input_accel_dmp_bias[axes] = dmp_bias;
	else if (SENSOR_COMPASS == sensor_type)
		st->input_compass_dmp_bias[axes] = dmp_bias;
	else
		return -EINVAL;

#ifdef DEBUG_ACCEL_RAW_DATA
         u8 data[6];
         int accel_x = 0;
         int accel_y = 0;
         int accel_z = 0;

    inv_set_bank(st, BANK_SEL_0);
         inv_plat_read(st, ACCEL_XOUT_H, 6, data);

         accel_x = (s16)be16_to_cpup((__be16 *)(&data[0]));
         accel_y = (s16)be16_to_cpup((__be16 *)(&data[2]));
         accel_z = (s16)be16_to_cpup((__be16 *)(&data[4]));

         return sprintf(buf, "accel raw data is %d %d %d,accel %s axis dmp bias is %d\n",
        		 accel_x, accel_y, accel_z,
                   ((addr == ACCEL_BIAS_X) ? "x" : ((addr == ACCEL_BIAS_Y) ? "y" :"z")),
                   dmp_bias);

#endif

	return sprintf(buf, "%d\n", dmp_bias);
}

/*
 * inv_attr_show() -  calling this function will show current
 *                        dmp parameters.
 */
static ssize_t inv_attr_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int result;
	s8 *m;

	switch (this_attr->address) {
	case ATTR_GYRO_SCALE:
	{
		const s16 gyro_scale[] = {250, 500, 1000, 2000};

		return sprintf(buf, "%d\n", gyro_scale[st->chip_config.fsr]);
	}
	case ATTR_ACCEL_SCALE:
	{
		const s16 accel_scale[] = {2, 4, 8, 16};
		return sprintf(buf, "%d\n",
					accel_scale[st->chip_config.accel_fs]);
	}
	case ATTR_COMPASS_SCALE:
		st->slave_compass->get_scale(st, &result);

		return sprintf(buf, "%d\n", result);
	case ATTR_GYRO_ENABLE:
		return sprintf(buf, "%d\n", st->chip_config.gyro_enable);
	case ATTR_ACCEL_ENABLE:
		return sprintf(buf, "%d\n", st->chip_config.accel_enable);
	case ATTR_DMP_ACCEL_CAL_ENABLE:
		return sprintf(buf, "%d\n", st->accel_cal_enable);
	case ATTR_DMP_GYRO_CAL_ENABLE:
		return sprintf(buf, "%d\n", st->gyro_cal_enable);
	case ATTR_DMP_COMPASS_CAL_ENABLE:
		return sprintf(buf, "%d\n", st->compass_cal_enable);
	case ATTR_DMP_DEBUG_DETERMINE_ENGINE_ON:
		return sprintf(buf, "%d\n", st->debug_determine_engine_on);
	case ATTR_DMP_PARAMS_ACCEL_CALIBRATION_THRESHOLD:
		return sprintf(buf, "%d\n", st->accel_calib_threshold);
	case ATTR_DMP_PARAMS_ACCEL_CALIBRATION_RATE:
		return sprintf(buf, "%d\n", st->accel_calib_rate);
	case ATTR_FIRMWARE_LOADED:
		return sprintf(buf, "%d\n", st->chip_config.firmware_loaded);
	case ATTR_DMP_ON:
		return sprintf(buf, "%d\n", st->chip_config.dmp_on);
	case ATTR_DMP_BATCHMODE_TIMEOUT:
		return sprintf(buf, "%d\n", st->batch.timeout);
	case ATTR_DMP_EVENT_INT_ON:
		return sprintf(buf, "%d\n", st->chip_config.dmp_event_int_on);
	case ATTR_DMP_PED_INT_ON:
		return sprintf(buf, "%d\n", st->ped.int_on);
	case ATTR_DMP_PED_ON:
		return sprintf(buf, "%d\n", st->ped.on);
	case ATTR_DMP_PED_STEP_THRESH:
		return sprintf(buf, "%d\n", st->ped.step_thresh);
	case ATTR_DMP_PED_INT_THRESH:
		return sprintf(buf, "%d\n", st->ped.int_thresh);
	case ATTR_DMP_SMD_ENABLE:
		return sprintf(buf, "%d\n", st->smd.on);
	case ATTR_DMP_SMD_THLD:
		return sprintf(buf, "%d\n", st->smd.threshold);
	case ATTR_DMP_SMD_DELAY_THLD:
		return sprintf(buf, "%d\n", st->smd.delay);
	case ATTR_DMP_SMD_DELAY_THLD2:
		return sprintf(buf, "%d\n", st->smd.delay2);
	case ATTR_DMP_LOW_POWER_GYRO_ON:
		return sprintf(buf, "%d\n", st->chip_config.low_power_gyro_on);
	case ATTR_DEBUG_DATA_COLLECTION_MODE:
		return sprintf(buf, "%d\n",
				st->chip_config.debug_data_collection_mode_on);
	case ATTR_DEBUG_DATA_COLLECTION_GYRO_RATE:
		return sprintf(buf, "%d\n",
			st->chip_config.debug_data_collection_gyro_freq);
	case ATTR_DEBUG_DATA_COLLECTION_ACCEL_RATE:
		return sprintf(buf, "%d\n",
			st->chip_config.debug_data_collection_accel_freq);
	case ATTR_DMP_LP_EN_OFF:
		return sprintf(buf, "%d\n", st->chip_config.lp_en_mode_off);
	case ATTR_DMP_CYCLE_MODE_OFF:
		return sprintf(buf, "%d\n", st->chip_config.cycle_mode_off);
	case ATTR_COMPASS_ENABLE:
		return sprintf(buf, "%d\n", st->chip_config.compass_enable);
	case ATTR_DMP_STEP_INDICATOR_ON:
		return sprintf(buf, "%d\n", st->chip_config.step_indicator_on);
	case ATTR_DMP_STEP_DETECTOR_ON:
		return sprintf(buf, "%d\n", st->chip_config.step_detector_on);
	case ATTR_DMP_IN_ANGLVEL_ACCURACY_ENABLE:
		return sprintf(buf, "%d\n",
				st->sensor_accuracy[SENSOR_GYRO_ACCURACY].on);
	case ATTR_DMP_IN_ACCEL_ACCURACY_ENABLE:
		return sprintf(buf, "%d\n",
				st->sensor_accuracy[SENSOR_ACCEL_ACCURACY].on);
	case ATTR_DMP_IN_MAGN_ACCURACY_ENABLE:
		return sprintf(buf, "%d\n",
			st->sensor_accuracy[SENSOR_COMPASS_ACCURACY].on);
	case ATTR_GYRO_MATRIX:
		m = st->plat_data.orientation;
		return sprintf(buf, "%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
			m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]);
	case ATTR_ACCEL_MATRIX:
		m = st->plat_data.orientation;
		return sprintf(buf, "%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
			m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]);
	case ATTR_COMPASS_MATRIX:
		if (st->plat_data.sec_slave_type ==
				SECONDARY_SLAVE_TYPE_COMPASS)
			m =
			st->plat_data.secondary_orientation;
		else
			return -ENODEV;
		return sprintf(buf, "%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
			m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]);
	case ATTR_SECONDARY_NAME:
	{
		const char *n[] = {"NULL", "AK8975", "AK8972", "AK8963",
					"MLX90399", "AK09911", "AK09912"};

		switch (st->plat_data.sec_slave_id) {
		case COMPASS_ID_AK8975:
			return sprintf(buf, "%s\n", n[1]);
		case COMPASS_ID_AK8972:
			return sprintf(buf, "%s\n", n[2]);
		case COMPASS_ID_AK8963:
			return sprintf(buf, "%s\n", n[3]);
		case COMPASS_ID_MLX90399:
			return sprintf(buf, "%s\n", n[4]);
		case COMPASS_ID_AK09911:
			return sprintf(buf, "%s\n", n[5]);
		case COMPASS_ID_AK09912:
			return sprintf(buf, "%s\n", n[6]);
		default:
			return sprintf(buf, "%s\n", n[0]);
		}
	}
	case ATTR_DMP_REF_MAG_3D:
	{
		int out;
		inv_switch_power_in_lp(st, true);
		result = read_be32_from_mem(st, &out, CPASS_REF_MAG_3D);
		if (result)
			return result;
		inv_switch_power_in_lp(st, false);
		return sprintf(buf, "%d\n", out);
	}
	case ATTR_DMP_DEBUG_MEM_READ:
	{
		int out;

		inv_switch_power_in_lp(st, true);
		result = read_be32_from_mem(st, &out, debug_mem_read_addr);
		if (result)
			return result;
		inv_switch_power_in_lp(st, false);
		return sprintf(buf, "addr=%d,0x%x\n", debug_mem_read_addr,out);
	}
	case ATTR_GYRO_SF:
		return sprintf(buf, "%d\n", st->gyro_sf);
	default:
		return -EPERM;
	}
}

static ssize_t inv_attr64_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int result;
	u64 tmp;
	u32 ped;

	mutex_lock(&indio_dev->mlock);
	if (!st->chip_config.dmp_on) {
		mutex_unlock(&indio_dev->mlock);
		return -EINVAL;
	}
	result = 0;
	switch (this_attr->address) {
	case ATTR_DMP_PEDOMETER_STEPS:
		inv_switch_power_in_lp(st, true);
		result = inv_get_pedometer_steps(st, &ped);
		result |= inv_read_pedometer_counter(st);
		tmp = (u64)st->ped.step + (u64)ped;
		inv_switch_power_in_lp(st, false);
		break;
	case ATTR_DMP_PEDOMETER_TIME:
		inv_switch_power_in_lp(st, true);
		result = inv_get_pedometer_time(st, &ped);
		tmp = (u64)st->ped.time + ((u64)ped) * MS_PER_PED_TICKS;
		inv_switch_power_in_lp(st, false);
		break;
	case ATTR_DMP_PEDOMETER_COUNTER:
		tmp = st->ped.last_step_time;
		break;
	default:
		tmp = 0;
		result = -EINVAL;
		break;
	}

	mutex_unlock(&indio_dev->mlock);
	if (result)
		return -EINVAL;
	return sprintf(buf, "%lld\n", tmp);
}

static ssize_t inv_attr64_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	int result;
	u8 d[4] = {0, 0, 0, 0};
	u64 data;

	mutex_lock(&indio_dev->mlock);
	if (!st->chip_config.firmware_loaded) {
		mutex_unlock(&indio_dev->mlock);
		return -EINVAL;
	}
	result = inv_switch_power_in_lp(st, true);
	if (result) {
		mutex_unlock(&indio_dev->mlock);
		return result;
	}
	result = kstrtoull(buf, 10, &data);
	if (result)
		goto attr64_store_fail;
	switch (this_attr->address) {
	case ATTR_DMP_PEDOMETER_STEPS:
		result = mem_w(PEDSTD_STEPCTR, ARRAY_SIZE(d), d);
		if (result)
			goto attr64_store_fail;
		st->ped.step = data;
		break;
	case ATTR_DMP_PEDOMETER_TIME:
		result = mem_w(PEDSTD_TIMECTR, ARRAY_SIZE(d), d);
		if (result)
			goto attr64_store_fail;
		st->ped.time = data;
		break;
	default:
		result = -EINVAL;
		break;
	}
attr64_store_fail:
	mutex_unlock(&indio_dev->mlock);
	result = inv_switch_power_in_lp(st, false);
	if (result)
		return result;

	return count;
}

static ssize_t inv_self_test(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	int res;

	mutex_lock(&indio_dev->mlock);
	res = inv_hw_self_test(st);
	set_inv_enable(indio_dev);
	mutex_unlock(&indio_dev->mlock);

	return sprintf(buf, "%d\n", res);
}


#ifdef TURN_OFF_SENSOR_WHEN_BACKLIGHT_IS_0

#define  YEAH (1)
#define  NO   (0)
extern struct inv_mpu_state *inv_st;

static ssize_t sensor_pwr_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", inv_st->sensor_need_on);
}

static ssize_t sensor_pwr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int sensor_on,result;

	result = kstrtoint(buf, 10, &sensor_on);
	if (result)
		return -EINVAL;
	pr_debug("%s sensor_need_on:%d\n",__func__, sensor_on);
	pr_debug("%s 11 st->need_on:%d\n",__func__, inv_st->sensor_need_on);
	if( sensor_on ) {
		inv_st->sensor_need_on++;
	} else {

		if(inv_st->sensor_need_on>0) {
			inv_st->sensor_need_on--;
		} else if(inv_st->sensor_need_on<0) {
			inv_st->sensor_need_on = 0;
		}

	}
	pr_debug("%s 22 st->need_on:%d\n",__func__, inv_st->sensor_need_on);
	return count;
}

#endif

/*
 *  inv_temperature_show() - Read temperature data directly from registers.
 */
static ssize_t inv_temperature_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{

	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	int result, scale_t;
	short temp;
	u8 data[2];

	mutex_lock(&indio_dev->mlock);
	result = inv_switch_power_in_lp(st, true);
	if (result) {
		mutex_unlock(&indio_dev->mlock);
		return result;
	}

	result = inv_plat_read(st, REG_TEMPERATURE, 2, data);
	mutex_unlock(&indio_dev->mlock);
	if (result) {
		pr_err("Could not read temperature register.\n");
		return result;
	}
	result = inv_switch_power_in_lp(st, false);
	if (result)
		return result;
	temp = (s16)(be16_to_cpup((short *)&data[0]));
	scale_t = TEMPERATURE_OFFSET +
		inv_q30_mult((int)temp << MPU_TEMP_SHIFT, TEMPERATURE_SCALE);

	INV_I2C_INC_TEMPREAD(1);

	return sprintf(buf, "%d %lld\n", scale_t, get_time_ns());
}

/*
 * inv_smd_show() -  calling this function showes smd interrupt.
 *                         This event must use poll.
 */
static ssize_t inv_smd_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "1\n");
}

/*
 * inv_ped_show() -  calling this function showes pedometer interrupt.
 *                         This event must use poll.
 */
static ssize_t inv_ped_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "1\n");
}

/*
 *  inv_reg_dump_show() - Register dump for testing.
 */
static ssize_t inv_reg_dump_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ii;
	char data;
	ssize_t bytes_printed = 0;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mpu_state *st = iio_priv(indio_dev);

	mutex_lock(&indio_dev->mlock);
	inv_set_bank(st, BANK_SEL_0);
	bytes_printed += sprintf(buf + bytes_printed, "bank 0\n");

	for (ii = 0; ii < 0x7F; ii++) {
		/* don't read fifo r/w register */
		if ((ii == REG_MEM_R_W) || (ii == REG_FIFO_R_W))
			data = 0;
		else
			inv_plat_read(st, ii, 1, &data);
		bytes_printed += sprintf(buf + bytes_printed, "%#2x: %#2x\n",
					 ii, data);
	}
	inv_set_bank(st, BANK_SEL_1);
	bytes_printed += sprintf(buf + bytes_printed, "bank 1\n");
	for (ii = 0; ii < 0x2A; ii++) {
		inv_plat_read(st, ii, 1, &data);
		bytes_printed += sprintf(buf + bytes_printed, "%#2x: %#2x\n",
					 ii, data);
	}
	inv_set_bank(st, BANK_SEL_2);
	bytes_printed += sprintf(buf + bytes_printed, "bank 2\n");
	for (ii = 0; ii < 0x55; ii++) {
		inv_plat_read(st, ii, 1, &data);
		bytes_printed += sprintf(buf + bytes_printed, "%#2x: %#2x\n",
					 ii, data);
	}
	inv_set_bank(st, BANK_SEL_3);
	bytes_printed += sprintf(buf + bytes_printed, "bank 3\n");
	for (ii = 0; ii < 0x18; ii++) {
		inv_plat_read(st, ii, 1, &data);
		bytes_printed += sprintf(buf + bytes_printed, "%#2x: %#2x\n",
					 ii, data);
	}
	inv_set_bank(st, BANK_SEL_0);
	set_inv_enable(indio_dev);
	mutex_unlock(&indio_dev->mlock);

	return bytes_printed;
}

static ssize_t inv_flush_batch_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	int result;
	bool has_data;

	mutex_lock(&indio_dev->mlock);
	result = inv_flush_batch_data(indio_dev, &has_data);
	mutex_unlock(&indio_dev->mlock);
	if (result)
		return sprintf(buf, "%d\n", result);
	else
		return sprintf(buf, "%d\n", has_data);
}

static const struct iio_chan_spec inv_mpu_channels[] = {
	IIO_CHAN_SOFT_TIMESTAMP(INV_MPU_SCAN_TIMESTAMP),
};

static DEVICE_ATTR(poll_smd, S_IRUGO, inv_smd_show, NULL);
static DEVICE_ATTR(poll_pedometer, S_IRUGO, inv_ped_show, NULL);

/* special run time sysfs entry, read only */
static DEVICE_ATTR(misc_flush_batch, S_IRUGO, inv_flush_batch_show, NULL);

static DEVICE_ATTR(debug_reg_dump, S_IRUGO | S_IWUGO, inv_reg_dump_show, NULL);
static DEVICE_ATTR(out_temperature, S_IRUGO | S_IWUGO,
						inv_temperature_show, NULL);
static DEVICE_ATTR(misc_self_test, S_IRUGO | S_IWUGO, inv_self_test, NULL);

#ifdef TURN_OFF_SENSOR_WHEN_BACKLIGHT_IS_0
static DEVICE_ATTR(sensor_pwr_state, S_IRUGO | S_IWUGO, sensor_pwr_show, sensor_pwr_store);

#endif

static IIO_DEVICE_ATTR(info_anglvel_matrix, S_IRUGO, inv_attr_show, NULL,
	ATTR_GYRO_MATRIX);
static IIO_DEVICE_ATTR(info_accel_matrix, S_IRUGO, inv_attr_show, NULL,
	ATTR_ACCEL_MATRIX);
static IIO_DEVICE_ATTR(info_magn_matrix, S_IRUGO, inv_attr_show, NULL,
	ATTR_COMPASS_MATRIX);
static IIO_DEVICE_ATTR(info_secondary_name, S_IRUGO, inv_attr_show, NULL,
	ATTR_SECONDARY_NAME);
static IIO_DEVICE_ATTR(info_gyro_sf, S_IRUGO, inv_attr_show, NULL,
						ATTR_GYRO_SF);

/* sensor on/off sysfs control */
static IIO_DEVICE_ATTR(in_accel_enable, S_IRUGO | S_IWUGO,
		inv_sensor_on_show, inv_sensor_on_store, SENSOR_ACCEL);
static IIO_DEVICE_ATTR(in_anglvel_enable, S_IRUGO | S_IWUGO,
		inv_sensor_on_show, inv_sensor_on_store, SENSOR_GYRO);
static IIO_DEVICE_ATTR(in_magn_enable, S_IRUGO | S_IWUGO,
		inv_sensor_on_show, inv_sensor_on_store, SENSOR_COMPASS);
static IIO_DEVICE_ATTR(in_6quat_enable, S_IRUGO | S_IWUGO,
		inv_sensor_on_show, inv_sensor_on_store, SENSOR_SIXQ);
static IIO_DEVICE_ATTR(in_9quat_enable, S_IRUGO | S_IWUGO,
		inv_sensor_on_show, inv_sensor_on_store, SENSOR_NINEQ);
static IIO_DEVICE_ATTR(in_geomag_enable, S_IRUGO | S_IWUGO,
		inv_sensor_on_show, inv_sensor_on_store, SENSOR_GEOMAG);
static IIO_DEVICE_ATTR(in_p6quat_enable, S_IRUGO | S_IWUGO,
		inv_sensor_on_show, inv_sensor_on_store, SENSOR_PEDQ);
static IIO_DEVICE_ATTR(in_pressure_enable, S_IRUGO | S_IWUGO,
		inv_sensor_on_show, inv_sensor_on_store, SENSOR_PRESSURE);
static IIO_DEVICE_ATTR(in_als_px_enable, S_IRUGO | S_IWUGO,
		inv_sensor_on_show, inv_sensor_on_store, SENSOR_ALS);
static IIO_DEVICE_ATTR(in_calib_anglvel_enable, S_IRUGO | S_IWUGO,
		inv_sensor_on_show, inv_sensor_on_store, SENSOR_CALIB_GYRO);
static IIO_DEVICE_ATTR(in_calib_magn_enable, S_IRUGO | S_IWUGO,
		inv_sensor_on_show, inv_sensor_on_store, SENSOR_CALIB_COMPASS);

/* sensor rate sysfs control */
static IIO_DEVICE_ATTR(in_accel_rate, S_IRUGO | S_IWUGO,
	inv_sensor_rate_show, inv_sensor_rate_store, SENSOR_ACCEL);
static IIO_DEVICE_ATTR(in_anglvel_rate, S_IRUGO | S_IWUGO,
	inv_sensor_rate_show, inv_sensor_rate_store, SENSOR_GYRO);
static IIO_DEVICE_ATTR(in_magn_rate, S_IRUGO | S_IWUGO,
	inv_sensor_rate_show, inv_sensor_rate_store, SENSOR_COMPASS);
static IIO_DEVICE_ATTR(in_6quat_rate, S_IRUGO | S_IWUGO,
	inv_sensor_rate_show, inv_sensor_rate_store, SENSOR_SIXQ);
static IIO_DEVICE_ATTR(in_9quat_rate, S_IRUGO | S_IWUGO,
	inv_sensor_rate_show, inv_sensor_rate_store, SENSOR_NINEQ);
static IIO_DEVICE_ATTR(in_geomag_rate, S_IRUGO | S_IWUGO,
	inv_sensor_rate_show, inv_sensor_rate_store, SENSOR_GEOMAG);
static IIO_DEVICE_ATTR(in_p6quat_rate, S_IRUGO | S_IWUGO,
	inv_sensor_rate_show, inv_sensor_rate_store, SENSOR_PEDQ);
static IIO_DEVICE_ATTR(in_pressure_rate, S_IRUGO | S_IWUGO,
	inv_sensor_rate_show, inv_sensor_rate_store, SENSOR_PRESSURE);
static IIO_DEVICE_ATTR(in_als_px_rate, S_IRUGO | S_IWUGO,
	inv_sensor_rate_show, inv_sensor_rate_store, SENSOR_ALS);
static IIO_DEVICE_ATTR(in_calib_anglvel_rate, S_IRUGO | S_IWUGO,
	inv_sensor_rate_show, inv_sensor_rate_store, SENSOR_CALIB_GYRO);
static IIO_DEVICE_ATTR(in_calib_magn_rate, S_IRUGO | S_IWUGO,
	inv_sensor_rate_show, inv_sensor_rate_store, SENSOR_CALIB_COMPASS);

/* debug determine engine related sysfs */
static IIO_DEVICE_ATTR(debug_anglvel_accuracy_enable, S_IRUGO | S_IWUGO,
				inv_attr_show, inv_debug_attr_store,
				ATTR_DMP_IN_ANGLVEL_ACCURACY_ENABLE);
static IIO_DEVICE_ATTR(debug_accel_accuracy_enable, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_debug_attr_store, ATTR_DMP_IN_ACCEL_ACCURACY_ENABLE);
static IIO_DEVICE_ATTR(debug_magn_accuracy_enable, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_debug_attr_store, ATTR_DMP_IN_MAGN_ACCURACY_ENABLE);
static IIO_DEVICE_ATTR(debug_gyro_cal_enable, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_debug_attr_store, ATTR_DMP_GYRO_CAL_ENABLE);
static IIO_DEVICE_ATTR(debug_accel_cal_enable, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_debug_attr_store, ATTR_DMP_ACCEL_CAL_ENABLE);
static IIO_DEVICE_ATTR(debug_compass_cal_enable, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_debug_attr_store, ATTR_DMP_COMPASS_CAL_ENABLE);
static IIO_DEVICE_ATTR(misc_magn_recalibration, S_IRUGO | S_IWUGO, NULL,
	inv_dmp_bias_store, ATTR_DMP_MISC_COMPASS_RECALIBRATION);
static IIO_DEVICE_ATTR(misc_ref_mag_3d, S_IRUGO | S_IWUGO,
		inv_attr_show, inv_misc_attr_store, ATTR_DMP_REF_MAG_3D);

static IIO_DEVICE_ATTR(debug_gyro_enable, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_debug_attr_store, ATTR_GYRO_ENABLE);
static IIO_DEVICE_ATTR(debug_accel_enable, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_debug_attr_store, ATTR_ACCEL_ENABLE);
static IIO_DEVICE_ATTR(debug_compass_enable, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_debug_attr_store, ATTR_COMPASS_ENABLE);
static IIO_DEVICE_ATTR(debug_dmp_on, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_debug_attr_store, ATTR_DMP_ON);
static IIO_DEVICE_ATTR(debug_dmp_event_int_on, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_debug_attr_store, ATTR_DMP_EVENT_INT_ON);
static IIO_DEVICE_ATTR(debug_mem_read, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_dmp_bias_store, ATTR_DMP_DEBUG_MEM_READ);
static IIO_DEVICE_ATTR(debug_mem_write, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_dmp_bias_store, ATTR_DMP_DEBUG_MEM_WRITE);

static IIO_DEVICE_ATTR(misc_batchmode_timeout, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_basic_attr_store, ATTR_DMP_BATCHMODE_TIMEOUT);

static IIO_DEVICE_ATTR(info_firmware_loaded, S_IRUGO | S_IWUGO, inv_attr_show,
	inv_firmware_loaded_store, ATTR_FIRMWARE_LOADED);

/* engine scale */
static IIO_DEVICE_ATTR(in_accel_scale, S_IRUGO | S_IWUGO, inv_attr_show,
	inv_misc_attr_store, ATTR_ACCEL_SCALE);
static IIO_DEVICE_ATTR(in_anglvel_scale, S_IRUGO | S_IWUGO, inv_attr_show,
	inv_misc_attr_store, ATTR_GYRO_SCALE);
static IIO_DEVICE_ATTR(in_magn_scale, S_IRUGO | S_IWUGO, inv_attr_show,
	NULL, ATTR_COMPASS_SCALE);

static IIO_DEVICE_ATTR(debug_low_power_gyro_on, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_misc_attr_store, ATTR_DMP_LOW_POWER_GYRO_ON);
static IIO_DEVICE_ATTR(debug_lp_en_off, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_debug_store, ATTR_DMP_LP_EN_OFF);
static IIO_DEVICE_ATTR(debug_cycle_mode_off, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_debug_store, ATTR_DMP_CYCLE_MODE_OFF);
static IIO_DEVICE_ATTR(debug_clock_sel, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_debug_store, ATTR_DMP_CLK_SEL);
static IIO_DEVICE_ATTR(debug_reg_write, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_debug_store, ATTR_DEBUG_REG_WRITE);
static IIO_DEVICE_ATTR(debug_cfg_write, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_debug_store, ATTR_DEBUG_WRITE_CFG);
static IIO_DEVICE_ATTR(debug_reg_write_addr, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_debug_store, ATTR_DEBUG_REG_ADDR);
static IIO_DEVICE_ATTR(debug_data_collection_mode_on, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_misc_attr_store, ATTR_DEBUG_DATA_COLLECTION_MODE);
static IIO_DEVICE_ATTR(debug_data_collection_mode_gyro_rate, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_misc_attr_store,
	ATTR_DEBUG_DATA_COLLECTION_GYRO_RATE);
static IIO_DEVICE_ATTR(debug_data_collection_mode_accel_rate, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_misc_attr_store,
	ATTR_DEBUG_DATA_COLLECTION_ACCEL_RATE);

#ifndef READ_BIAS_FROM_NVRAM
static IIO_DEVICE_ATTR(in_accel_x_calibbias, S_IRUGO | S_IWUGO,
			inv_attr_bias_show, NULL, ATTR_ACCEL_X_CALIBBIAS);
static IIO_DEVICE_ATTR(in_accel_y_calibbias, S_IRUGO | S_IWUGO,
			inv_attr_bias_show, NULL, ATTR_ACCEL_Y_CALIBBIAS);
static IIO_DEVICE_ATTR(in_accel_z_calibbias, S_IRUGO | S_IWUGO,
			inv_attr_bias_show, NULL, ATTR_ACCEL_Z_CALIBBIAS);
#else
/*
 * add this for store calibbias from nvram
 */
static IIO_DEVICE_ATTR(in_accel_x_calibbias, S_IRUGO | S_IWUGO,
			inv_attr_bias_show, inv_accel_calibbias_bias_store, ATTR_ACCEL_X_CALIBBIAS);
static IIO_DEVICE_ATTR(in_accel_y_calibbias, S_IRUGO | S_IWUGO,
			inv_attr_bias_show, inv_accel_calibbias_bias_store, ATTR_ACCEL_Y_CALIBBIAS);
static IIO_DEVICE_ATTR(in_accel_z_calibbias, S_IRUGO | S_IWUGO,
			inv_attr_bias_show, inv_accel_calibbias_bias_store, ATTR_ACCEL_Z_CALIBBIAS);

#endif


static IIO_DEVICE_ATTR(in_anglvel_x_calibbias, S_IRUGO | S_IWUGO,
			inv_attr_bias_show, NULL, ATTR_ANGLVEL_X_CALIBBIAS);
static IIO_DEVICE_ATTR(in_anglvel_y_calibbias, S_IRUGO | S_IWUGO,
			inv_attr_bias_show, NULL, ATTR_ANGLVEL_Y_CALIBBIAS);
static IIO_DEVICE_ATTR(in_anglvel_z_calibbias, S_IRUGO | S_IWUGO,
			inv_attr_bias_show, NULL, ATTR_ANGLVEL_Z_CALIBBIAS);

static IIO_DEVICE_ATTR(in_accel_x_dmp_bias, S_IRUGO | S_IWUGO,
	inv_attr_bias_show, inv_dmp_bias_store, ATTR_DMP_ACCEL_X_DMP_BIAS);
static IIO_DEVICE_ATTR(in_accel_y_dmp_bias, S_IRUGO | S_IWUGO,
	inv_attr_bias_show, inv_dmp_bias_store, ATTR_DMP_ACCEL_Y_DMP_BIAS);
static IIO_DEVICE_ATTR(in_accel_z_dmp_bias, S_IRUGO | S_IWUGO,
	inv_attr_bias_show, inv_dmp_bias_store, ATTR_DMP_ACCEL_Z_DMP_BIAS);

static IIO_DEVICE_ATTR(in_anglvel_x_dmp_bias, S_IRUGO | S_IWUGO,
	inv_attr_bias_show, inv_dmp_bias_store, ATTR_DMP_GYRO_X_DMP_BIAS);
static IIO_DEVICE_ATTR(in_anglvel_y_dmp_bias, S_IRUGO | S_IWUGO,
	inv_attr_bias_show, inv_dmp_bias_store, ATTR_DMP_GYRO_Y_DMP_BIAS);
static IIO_DEVICE_ATTR(in_anglvel_z_dmp_bias, S_IRUGO | S_IWUGO,
	inv_attr_bias_show, inv_dmp_bias_store, ATTR_DMP_GYRO_Z_DMP_BIAS);

static IIO_DEVICE_ATTR(in_magn_x_dmp_bias, S_IRUGO | S_IWUGO,
	inv_attr_bias_show, inv_dmp_bias_store, ATTR_DMP_MAGN_X_DMP_BIAS);
static IIO_DEVICE_ATTR(in_magn_y_dmp_bias, S_IRUGO | S_IWUGO,
	inv_attr_bias_show, inv_dmp_bias_store, ATTR_DMP_MAGN_Y_DMP_BIAS);
static IIO_DEVICE_ATTR(in_magn_z_dmp_bias, S_IRUGO | S_IWUGO,
	inv_attr_bias_show, inv_dmp_bias_store, ATTR_DMP_MAGN_Z_DMP_BIAS);

static IIO_DEVICE_ATTR(debug_determine_engine_on, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_misc_attr_store, ATTR_DMP_DEBUG_DETERMINE_ENGINE_ON);
static IIO_DEVICE_ATTR(misc_gyro_recalibration, S_IRUGO | S_IWUGO, NULL,
	inv_dmp_bias_store, ATTR_DMP_MISC_GYRO_RECALIBRATION);
static IIO_DEVICE_ATTR(misc_accel_recalibration, S_IRUGO | S_IWUGO, NULL,
	inv_dmp_bias_store, ATTR_DMP_MISC_ACCEL_RECALIBRATION);
static IIO_DEVICE_ATTR(params_accel_calibration_threshold, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_dmp_bias_store,
			ATTR_DMP_PARAMS_ACCEL_CALIBRATION_THRESHOLD);
static IIO_DEVICE_ATTR(params_accel_calibration_rate, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_dmp_bias_store,
			ATTR_DMP_PARAMS_ACCEL_CALIBRATION_RATE);

static IIO_DEVICE_ATTR(in_step_detector_enable, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_basic_attr_store, ATTR_DMP_STEP_DETECTOR_ON);
static IIO_DEVICE_ATTR(in_step_indicator_enable, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_basic_attr_store, ATTR_DMP_STEP_INDICATOR_ON);

static IIO_DEVICE_ATTR(event_smd_enable, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_basic_attr_store, ATTR_DMP_SMD_ENABLE);
static IIO_DEVICE_ATTR(params_smd_threshold, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_misc_attr_store, ATTR_DMP_SMD_THLD);
static IIO_DEVICE_ATTR(params_smd_delay_threshold, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_misc_attr_store, ATTR_DMP_SMD_DELAY_THLD);
static IIO_DEVICE_ATTR(params_smd_delay_threshold2, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_misc_attr_store, ATTR_DMP_SMD_DELAY_THLD2);

static IIO_DEVICE_ATTR(params_pedometer_int_on, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_misc_attr_store, ATTR_DMP_PED_INT_ON);
static IIO_DEVICE_ATTR(event_pedometer_enable, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_basic_attr_store, ATTR_DMP_PED_ON);
static IIO_DEVICE_ATTR(params_pedometer_step_thresh, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_misc_attr_store, ATTR_DMP_PED_STEP_THRESH);
static IIO_DEVICE_ATTR(params_pedometer_int_thresh, S_IRUGO | S_IWUGO,
	inv_attr_show, inv_misc_attr_store, ATTR_DMP_PED_INT_THRESH);

static IIO_DEVICE_ATTR(out_pedometer_steps, S_IRUGO | S_IWUGO, inv_attr64_show,
	inv_attr64_store, ATTR_DMP_PEDOMETER_STEPS);
static IIO_DEVICE_ATTR(out_pedometer_time, S_IRUGO | S_IWUGO, inv_attr64_show,
	inv_attr64_store, ATTR_DMP_PEDOMETER_TIME);
static IIO_DEVICE_ATTR(out_pedometer_counter, S_IRUGO | S_IWUGO,
			inv_attr64_show, NULL, ATTR_DMP_PEDOMETER_COUNTER);

static const struct attribute *inv_raw_attributes[] = {
	&dev_attr_debug_reg_dump.attr,
	&dev_attr_out_temperature.attr,
	&dev_attr_misc_flush_batch.attr,
	&dev_attr_misc_self_test.attr,

#ifdef TURN_OFF_SENSOR_WHEN_BACKLIGHT_IS_0
	&dev_attr_sensor_pwr_state.attr,
#endif

	&iio_dev_attr_info_anglvel_matrix.dev_attr.attr,
	&iio_dev_attr_debug_gyro_enable.dev_attr.attr,
	&iio_dev_attr_in_anglvel_enable.dev_attr.attr,
	&iio_dev_attr_debug_accel_enable.dev_attr.attr,
	&iio_dev_attr_in_accel_enable.dev_attr.attr,
	&iio_dev_attr_info_accel_matrix.dev_attr.attr,
	&iio_dev_attr_in_accel_scale.dev_attr.attr,
	&iio_dev_attr_in_anglvel_scale.dev_attr.attr,
	&iio_dev_attr_info_firmware_loaded.dev_attr.attr,
	&iio_dev_attr_debug_dmp_on.dev_attr.attr,
	&iio_dev_attr_misc_batchmode_timeout.dev_attr.attr,
	&iio_dev_attr_debug_dmp_event_int_on.dev_attr.attr,
	&iio_dev_attr_debug_low_power_gyro_on.dev_attr.attr,
	&iio_dev_attr_debug_data_collection_mode_on.dev_attr.attr,
	&iio_dev_attr_debug_data_collection_mode_gyro_rate.dev_attr.attr,
	&iio_dev_attr_debug_data_collection_mode_accel_rate.dev_attr.attr,
	&iio_dev_attr_debug_lp_en_off.dev_attr.attr,
	&iio_dev_attr_debug_cycle_mode_off.dev_attr.attr,
	&iio_dev_attr_debug_clock_sel.dev_attr.attr,
	&iio_dev_attr_debug_reg_write.dev_attr.attr,
	&iio_dev_attr_debug_reg_write_addr.dev_attr.attr,
	&iio_dev_attr_debug_cfg_write.dev_attr.attr,
	&iio_dev_attr_in_anglvel_rate.dev_attr.attr,
	&iio_dev_attr_in_accel_rate.dev_attr.attr,
	&iio_dev_attr_in_accel_x_dmp_bias.dev_attr.attr,
	&iio_dev_attr_in_accel_y_dmp_bias.dev_attr.attr,
	&iio_dev_attr_in_accel_z_dmp_bias.dev_attr.attr,
	&iio_dev_attr_debug_accel_cal_enable.dev_attr.attr,
	&iio_dev_attr_debug_gyro_cal_enable.dev_attr.attr,
	&iio_dev_attr_in_anglvel_x_dmp_bias.dev_attr.attr,
	&iio_dev_attr_in_anglvel_y_dmp_bias.dev_attr.attr,
	&iio_dev_attr_in_anglvel_z_dmp_bias.dev_attr.attr,
	&iio_dev_attr_in_accel_x_calibbias.dev_attr.attr,
	&iio_dev_attr_in_accel_y_calibbias.dev_attr.attr,
	&iio_dev_attr_in_accel_z_calibbias.dev_attr.attr,
	&iio_dev_attr_in_anglvel_x_calibbias.dev_attr.attr,
	&iio_dev_attr_in_anglvel_y_calibbias.dev_attr.attr,
	&iio_dev_attr_in_anglvel_z_calibbias.dev_attr.attr,
	&iio_dev_attr_in_6quat_enable.dev_attr.attr,
	&iio_dev_attr_in_6quat_rate.dev_attr.attr,
	&iio_dev_attr_in_p6quat_enable.dev_attr.attr,
	&iio_dev_attr_in_p6quat_rate.dev_attr.attr,
	&iio_dev_attr_in_calib_anglvel_enable.dev_attr.attr,
	&iio_dev_attr_in_calib_anglvel_rate.dev_attr.attr,
	&iio_dev_attr_debug_anglvel_accuracy_enable.dev_attr.attr,
	&iio_dev_attr_debug_accel_accuracy_enable.dev_attr.attr,
	&iio_dev_attr_info_secondary_name.dev_attr.attr,
	&iio_dev_attr_info_gyro_sf.dev_attr.attr,
	&iio_dev_attr_debug_determine_engine_on.dev_attr.attr,
	&iio_dev_attr_debug_mem_read.dev_attr.attr,
	&iio_dev_attr_debug_mem_write.dev_attr.attr,
	&iio_dev_attr_misc_gyro_recalibration.dev_attr.attr,
	&iio_dev_attr_misc_accel_recalibration.dev_attr.attr,
	&iio_dev_attr_params_accel_calibration_threshold.dev_attr.attr,
	&iio_dev_attr_params_accel_calibration_rate.dev_attr.attr,
};

static const struct attribute *inv_compass_attributes[] = {
	&iio_dev_attr_in_magn_scale.dev_attr.attr,
	&iio_dev_attr_debug_compass_enable.dev_attr.attr,
	&iio_dev_attr_info_magn_matrix.dev_attr.attr,
	&iio_dev_attr_in_magn_x_dmp_bias.dev_attr.attr,
	&iio_dev_attr_in_magn_y_dmp_bias.dev_attr.attr,
	&iio_dev_attr_in_magn_z_dmp_bias.dev_attr.attr,
	&iio_dev_attr_debug_compass_cal_enable.dev_attr.attr,
	&iio_dev_attr_misc_magn_recalibration.dev_attr.attr,
	&iio_dev_attr_misc_ref_mag_3d.dev_attr.attr,
	&iio_dev_attr_debug_magn_accuracy_enable.dev_attr.attr,
	&iio_dev_attr_in_calib_magn_enable.dev_attr.attr,
	&iio_dev_attr_in_calib_magn_rate.dev_attr.attr,
	&iio_dev_attr_in_geomag_enable.dev_attr.attr,
	&iio_dev_attr_in_geomag_rate.dev_attr.attr,
	&iio_dev_attr_in_magn_enable.dev_attr.attr,
	&iio_dev_attr_in_magn_rate.dev_attr.attr,
	&iio_dev_attr_in_9quat_enable.dev_attr.attr,
	&iio_dev_attr_in_9quat_rate.dev_attr.attr,
};

static const struct attribute *inv_pedometer_attributes[] = {
	&dev_attr_poll_pedometer.attr,
	&iio_dev_attr_params_pedometer_int_on.dev_attr.attr,
	&iio_dev_attr_event_pedometer_enable.dev_attr.attr,
	&iio_dev_attr_in_step_indicator_enable.dev_attr.attr,
	&iio_dev_attr_in_step_detector_enable.dev_attr.attr,
	&iio_dev_attr_out_pedometer_steps.dev_attr.attr,
	&iio_dev_attr_out_pedometer_time.dev_attr.attr,
	&iio_dev_attr_out_pedometer_counter.dev_attr.attr,
	&iio_dev_attr_params_pedometer_step_thresh.dev_attr.attr,
	&iio_dev_attr_params_pedometer_int_thresh.dev_attr.attr,
};
static const struct attribute *inv_smd_attributes[] = {
	&dev_attr_poll_smd.attr,
	&iio_dev_attr_event_smd_enable.dev_attr.attr,
	&iio_dev_attr_params_smd_threshold.dev_attr.attr,
	&iio_dev_attr_params_smd_delay_threshold.dev_attr.attr,
	&iio_dev_attr_params_smd_delay_threshold2.dev_attr.attr,
};

static const struct attribute *inv_pressure_attributes[] = {
	&iio_dev_attr_in_pressure_enable.dev_attr.attr,
	&iio_dev_attr_in_pressure_rate.dev_attr.attr,
};

static const struct attribute *inv_als_attributes[] = {
	&iio_dev_attr_in_als_px_enable.dev_attr.attr,
	&iio_dev_attr_in_als_px_rate.dev_attr.attr,
};

static struct attribute *inv_attributes[
	ARRAY_SIZE(inv_raw_attributes) +
	ARRAY_SIZE(inv_pedometer_attributes) +
	ARRAY_SIZE(inv_smd_attributes) +
	ARRAY_SIZE(inv_compass_attributes) +
	ARRAY_SIZE(inv_pressure_attributes) +
	ARRAY_SIZE(inv_als_attributes) +
	1
];

static const struct attribute_group inv_attribute_group = {
	.name = "mpu",
	.attrs = inv_attributes
};

static const struct iio_info mpu_info = {
	.driver_module = THIS_MODULE,
	.attrs = &inv_attribute_group,
};

/*
 *  inv_check_chip_type() - check and setup chip type.
 */
int inv_check_chip_type(struct iio_dev *indio_dev, const char *name)
{
	u8 v;
	int result;
	int t_ind;
	struct inv_chip_config_s *conf;
	struct mpu_platform_data *plat;
	struct inv_mpu_state *st;

	st = iio_priv(indio_dev);
	conf = &st->chip_config;
	plat = &st->plat_data;
	if (!strcmp(name, "mpu7400"))
		st->chip_type = ICM20628;
	else if (!strcmp(name, "icm20628"))
		st->chip_type = ICM20628;
	else if (!strcmp(name, "icm20728"))
		st->chip_type = ICM20728;
	else
		return -EPERM;
	st->hw  = &hw_info[st->chip_type];
	result = inv_set_bank(st, BANK_SEL_0);
	if (result)
		return result;
	/* reset to make sure previous state are not there */
	result = inv_plat_single_write(st, REG_PWR_MGMT_1, BIT_H_RESET);
	if (result)
		return result;
	usleep_range(REG_UP_TIME_USEC, REG_UP_TIME_USEC);
	msleep(100);
	/* toggle power state */
	result = inv_set_power(st, false);
	if (result)
		return result;

	result = inv_set_power(st, true);
	if (result)
		return result;
	result = inv_plat_read(st, REG_WHO_AM_I, 1, &v);
	if (result)
		return result;

	result = inv_plat_single_write(st, REG_USER_CTRL, st->i2c_dis);
	if (result)
		return result;
	result = inv_init_config(st);
	if (result)
		return result;

	if (SECONDARY_SLAVE_TYPE_COMPASS == plat->sec_slave_type)
		st->chip_config.has_compass = 1;
	else
		st->chip_config.has_compass = 0;
	if (SECONDARY_SLAVE_TYPE_PRESSURE == plat->aux_slave_type)
		st->chip_config.has_pressure = 1;
	else
		st->chip_config.has_pressure = 0;
	if (SECONDARY_SLAVE_TYPE_ALS == plat->read_only_slave_type)
		st->chip_config.has_als = 1;
	else
		st->chip_config.has_als = 0;

	if (st->chip_config.has_compass) {
		result = inv_mpu_setup_compass_slave(st);
		if (result) {
			pr_err("compass setup failed\n");
			inv_set_power(st, false);
			return result;
		}
	}
	if (st->chip_config.has_pressure) {
		result = inv_mpu_setup_pressure_slave(st);
		if (result) {
			pr_err("pressure setup failed\n");
			inv_set_power(st, false);
			return result;
		}
	}
	if (st->chip_config.has_als) {
		result = inv_mpu_setup_als_slave(st);
		if (result) {
			pr_err("als setup failed\n");
			inv_set_power(st, false);
			return result;
		}
	}

	result = mem_r(MPU_SOFT_REV_ADDR, 1, &v);
	if (result)
		return result;
	if (v & MPU_SOFT_REV_MASK) {
		pr_err("incorrect software revision=%x\n", v);
		return -EINVAL;
	}

	t_ind = 0;
	memcpy(&inv_attributes[t_ind], inv_raw_attributes,
					sizeof(inv_raw_attributes));
	t_ind += ARRAY_SIZE(inv_raw_attributes);

	memcpy(&inv_attributes[t_ind], inv_pedometer_attributes,
					sizeof(inv_pedometer_attributes));
	t_ind += ARRAY_SIZE(inv_pedometer_attributes);

	memcpy(&inv_attributes[t_ind], inv_smd_attributes,
					sizeof(inv_smd_attributes));
	t_ind += ARRAY_SIZE(inv_smd_attributes);

	if (st->chip_config.has_compass) {
		memcpy(&inv_attributes[t_ind], inv_compass_attributes,
		       sizeof(inv_compass_attributes));
		t_ind += ARRAY_SIZE(inv_compass_attributes);
	}
	if (ICM20728 == st->chip_type || st->chip_config.has_pressure) {
		memcpy(&inv_attributes[t_ind], inv_pressure_attributes,
		       sizeof(inv_pressure_attributes));
		t_ind += ARRAY_SIZE(inv_pressure_attributes);
	}
	if (st->chip_config.has_als) {
		memcpy(&inv_attributes[t_ind], inv_als_attributes,
		       sizeof(inv_als_attributes));
		t_ind += ARRAY_SIZE(inv_als_attributes);
	}

	inv_attributes[t_ind] = NULL;

	indio_dev->channels = inv_mpu_channels;
	indio_dev->num_channels = ARRAY_SIZE(inv_mpu_channels);

	indio_dev->info = &mpu_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->currentmode = INDIO_DIRECT_MODE;
	INIT_KFIFO(st->kf);

	result = inv_set_power(st, false);

	return result;
}

/*
 *  inv_create_dmp_sysfs() - create binary sysfs dmp entry.
 */
static const struct bin_attribute dmp_firmware = {
	.attr = {
		.name = "misc_bin_dmp_firmware",
		.mode = S_IRUGO | S_IWUGO
	},
	.size = DMP_IMAGE_SIZE,
	.read = inv_dmp_firmware_read,
	.write = inv_dmp_firmware_write,
};

static const struct bin_attribute soft_iron_matrix = {
	.attr = {
		.name = "misc_bin_soft_iron_matrix",
		.mode = S_IRUGO | S_IWUGO
	},
	.size = SOFT_IRON_MATRIX_SIZE,
	.write = inv_soft_iron_matrix_write,
};

static const struct bin_attribute accel_covariance = {
	.attr = {
		.name = "misc_bin_accel_covariance",
		.mode = S_IRUGO | S_IWUGO
	},
	.size = ACCEL_COVARIANCE_SIZE,
	.write = inv_accel_covariance_write,
	.read =  inv_accel_covariance_read,
};

static const struct bin_attribute compass_covariance = {
	.attr = {
		.name = "misc_bin_magn_covariance",
		.mode = S_IRUGO | S_IWUGO
	},
	.size = COMPASS_COVARIANCE_SIZE,
	.write = inv_compass_covariance_write,
	.read =  inv_compass_covariance_read,
};

static const struct bin_attribute compass_cur_covariance = {
	.attr = {
		.name = "misc_bin_cur_magn_covariance",
		.mode = S_IRUGO | S_IWUGO
	},
	.size = COMPASS_COVARIANCE_SIZE,
	.write = inv_compass_covariance_cur_write,
	.read =  inv_compass_covariance_cur_read,
};

int inv_create_dmp_sysfs(struct iio_dev *ind)
{
	int result;

	result = sysfs_create_bin_file(&ind->dev.kobj, &dmp_firmware);
	if (result)
		return result;
	result = sysfs_create_bin_file(&ind->dev.kobj, &soft_iron_matrix);
	if (result)
		return result;

	result = sysfs_create_bin_file(&ind->dev.kobj, &accel_covariance);
	if (result)
		return result;
	result = sysfs_create_bin_file(&ind->dev.kobj, &compass_covariance);
	if (result)
		return result;
	result = sysfs_create_bin_file(&ind->dev.kobj, &compass_cur_covariance);

	return result;
}
