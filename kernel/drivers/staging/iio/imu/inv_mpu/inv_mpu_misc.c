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
#include <linux/crc32.h>

#include "inv_mpu_iio.h"
#include "inv_test/inv_counters.h"

/* DMP defines */
#define FIRMWARE_CRC           0x12f362a6

int inv_get_pedometer_steps(struct inv_mpu_state *st, int *ped)
{
	int r;

	r = read_be32_from_mem(st, ped, PEDSTD_STEPCTR);

	return r;
}
int inv_get_pedometer_time(struct inv_mpu_state *st, int *ped)
{
	int r;

	r = read_be32_from_mem(st, ped, PEDSTD_TIMECTR);

	return r;
}

int inv_read_pedometer_counter(struct inv_mpu_state *st)
{
	int result;
	u32 last_step_counter, curr_counter;
	u64 counter;

	result = read_be32_from_mem(st, &last_step_counter, STPDET_TIMESTAMP);
	if (result)
		return result;
	if (0 != last_step_counter) {
		result = read_be32_from_mem(st, &curr_counter, DMPRATE_CNTR);
		if (result)
			return result;
		counter = inv_get_cntr_diff(curr_counter, last_step_counter);
		st->ped.last_step_time = get_time_ns() - counter *
						st->eng_info[ENGINE_ACCEL].dur;
	}

	return 0;
}

static int inv_load_firmware(struct inv_mpu_state *st)
{
	int bank, write_size;
	int result, size;
	u16 memaddr;
	u8 *data;

	data = st->firmware;
	size = DMP_IMAGE_SIZE - DMP_OFFSET;

	for (bank = 0; size > 0; bank++, size -= write_size) {
		if (size > MPU_MEM_BANK_SIZE)
			write_size = MPU_MEM_BANK_SIZE;
		else
			write_size = size;
		memaddr = (bank << 8);
		if (!bank) {
			memaddr = DMP_OFFSET;
			write_size = MPU_MEM_BANK_SIZE - DMP_OFFSET;
			data += DMP_OFFSET;
		}
		result = mem_w(memaddr, write_size, data);
		if (result) {
			pr_err("error writing firmware:%d\n", bank);
			return result;
		}
		data += write_size;
	}

	return 0;
}

static int inv_verify_firmware(struct inv_mpu_state *st)
{
	int bank, write_size, size;
	int result;
	u16 memaddr;
	u8 firmware[MPU_MEM_BANK_SIZE];
	u8 *data;

	data = st->firmware;
	size = DMP_IMAGE_SIZE - DMP_OFFSET;
	for (bank = 0; size > 0; bank++, size -= write_size) {
		if (size > MPU_MEM_BANK_SIZE)
			write_size = MPU_MEM_BANK_SIZE;
		else
			write_size = size;

		memaddr = (bank << 8);
		if (!bank) {
			memaddr = DMP_OFFSET;
			write_size = MPU_MEM_BANK_SIZE - DMP_OFFSET;
			data += DMP_OFFSET;
		}
		result = mem_r(memaddr, write_size, firmware);

#if 0
		int i;
		printk("%s size:%d\n",__func__,write_size);
		for(i=0; i<write_size; i++) {
			if((i+1)%16==0) printk("\n");
			printk(" 0x%x",firmware[i]);
		}
#endif

		if (result)
			return result;
		if (0 != memcmp(firmware, data, write_size)) {
			pr_err("load data error, bank=%d\n", bank);
			return -EINVAL;
		}
		data += write_size;
	}
	return 0;
}

static int inv_write_compass_matrix(struct inv_mpu_state *st, int *adj)
{
	int addr[] = {CPASS_MTX_00, CPASS_MTX_01, CPASS_MTX_02,
			CPASS_MTX_10, CPASS_MTX_11, CPASS_MTX_12,
			CPASS_MTX_20, CPASS_MTX_21, CPASS_MTX_22};
	int r, i;

	for (i = 0; i < 9; i++) {
		r = write_be32_to_mem(st, adj[i], addr[i]);
		if (r)
			return r;
	}

	return 0;
}
static int inv_compass_dmp_cal(struct inv_mpu_state *st)
{
	s8 *compass_m, *m;
	s8 trans[NINE_ELEM];
	s32 tmp_m[NINE_ELEM];
	int i, j, k, r;
	int sens[THREE_AXES];
	int *adj;
	int scale;
	int shift;

	compass_m = st->plat_data.secondary_orientation;
	m = st->plat_data.orientation;
	for (i = 0; i < THREE_AXES; i++)
		for (j = 0; j < THREE_AXES; j++)
			trans[THREE_AXES * j + i] = m[THREE_AXES * i + j];

	adj = st->current_compass_matrix;
	st->slave_compass->get_scale(st, &scale);

	if ((COMPASS_ID_AK8975 == st->plat_data.sec_slave_id) ||
			(COMPASS_ID_AK8972 == st->plat_data.sec_slave_id) ||
			(COMPASS_ID_AK8963 == st->plat_data.sec_slave_id))
		shift = AK89XX_SHIFT;
	else
		shift = AK99XX_SHIFT;

	for (i = 0; i < THREE_AXES; i++) {
		sens[i] = st->chip_info.compass_sens[i] + 128;
		sens[i] = inv_q30_mult(sens[i] << shift, scale);
	}
	for (i = 0; i < NINE_ELEM; i++) {
		adj[i] = compass_m[i] * sens[i % THREE_AXES];
		tmp_m[i] = 0;
	}
	for (i = 0; i < THREE_AXES; i++)
		for (j = 0; j < THREE_AXES; j++)
			for (k = 0; k < THREE_AXES; k++)
				tmp_m[THREE_AXES * i + j] +=
					trans[THREE_AXES * i + k] *
						adj[THREE_AXES * k + j];

	for (i = 0; i < NINE_ELEM; i++)
		st->final_compass_matrix[i] = adj[i];

	r = inv_write_compass_matrix(st, tmp_m);

	return r;
}

static int inv_write_gyro_sf(struct inv_mpu_state *st)
{
	int result;

	result = write_be32_to_mem(st, st->gyro_sf, GYRO_SF);

	return result;
}

static int inv_setup_dmp_firmware(struct inv_mpu_state *st)
{
	int result;
	u8 v[4] = {0, 0};

	result = mem_w(DATA_OUT_CTL1, 2, v);
	if (result)
		return result;
	result = mem_w(DATA_OUT_CTL2, 2, v);
	if (result)
		return result;
	result = mem_w(DATA_INTR_CTL, 2, v);
	if (result)
		return result;

	result = mem_w(MOTION_EVENT_CTL, 2, v);
	if (result)
		return result;

	result = inv_write_gyro_sf(st);
	if (result) {
		pr_err("dmp loading eror:inv_write_gyro_sf\n");
		return result;
	}
	if (st->chip_config.has_compass) {
		result = inv_compass_dmp_cal(st);
		if (result)
			return result;
	}

	result = write_be32_to_mem(st, 0xf3, 2469);
	if (result)
		return result;

	return result;
}
/*
 * inv_firmware_load() -  calling this function will load the firmware.
 */
static int inv_firmware_load(struct inv_mpu_state *st)
{
	int result;

	result = inv_switch_power_in_lp(st, true);
	if (result) {
		pr_err("load firmware set power error\n");
		goto firmware_write_fail;
	}
	result = inv_plat_single_write(st, REG_USER_CTRL, st->i2c_dis);
	if (result) {
		pr_err("load firmware:stop dmp error\n");
		goto firmware_write_fail;
	}
	result = inv_load_firmware(st);
	if (result) {
		pr_err("load firmware:load firmware eror\n");
		goto firmware_write_fail;
	}
	result = inv_verify_firmware(st);
	if (result) {
		pr_err("load firmware:verify firmware error\n");
		goto firmware_write_fail;
	}
	result = inv_setup_dmp_firmware(st);
	if (result)
		pr_err("load firmware:setup dmp error\n");
firmware_write_fail:
	result |= inv_set_power(st, false);
	if (result) {
		pr_err("load firmware:shuting down power error\n");
		return result;
	}

	st->chip_config.firmware_loaded = 1;

	return 0;
}

/*
 * inv_dmp_firmware_write() -  calling this function will load the firmware.
 */
ssize_t inv_dmp_firmware_write(struct file *fp, struct kobject *kobj,
	struct bin_attribute *attr, char *buf, loff_t pos, size_t size)
{
	int result, offset;
	struct iio_dev *indio_dev;
	struct inv_mpu_state *st;

	indio_dev = dev_get_drvdata(container_of(kobj, struct device, kobj));
	st = iio_priv(indio_dev);

	if (!st->firmware) {
		st->firmware = kmalloc(DMP_IMAGE_SIZE, GFP_KERNEL);
		if (!st->firmware) {
			pr_err("no memory while loading firmware\n");
			return -ENOMEM;
		}
	}
	offset = pos;
	memcpy(st->firmware + pos, buf, size);
	if ((!size) && (DMP_IMAGE_SIZE != pos)) {
		pr_err("wrong size for DMP firmware 0x%08x vs 0x%08x\n",
						offset, DMP_IMAGE_SIZE);
		kfree(st->firmware);
		st->firmware = 0;
		return -EINVAL;
	}
	if (DMP_IMAGE_SIZE == (pos + size)) {
		result = crc32(0, st->firmware, DMP_IMAGE_SIZE);
		if (FIRMWARE_CRC != result) {
			pr_err("firmware CRC error - 0x%08x vs 0x%08x\n",
							result, FIRMWARE_CRC);
			return -EINVAL;
		}
		mutex_lock(&indio_dev->mlock);
		result = inv_firmware_load(st);
		kfree(st->firmware);
		st->firmware = 0;
		mutex_unlock(&indio_dev->mlock);
		if (result) {
			pr_err("firmware load failed\n");
			return result;
		}
	}

	return size;
}

static int inv_dmp_read(struct inv_mpu_state *st, int off, int size, u8 *buf)
{
	int bank, write_size, data, result;
	u16 memaddr;

	data = 0;
	result = inv_switch_power_in_lp(st, true);
	if (result)
		return result;
	inv_stop_dmp(st);
	for (bank = (off >> 8); size > 0; bank++, size -= write_size,
					data += write_size) {
		if (size > MPU_MEM_BANK_SIZE)
			write_size = MPU_MEM_BANK_SIZE;
		else
			write_size = size;
		memaddr = (bank << 8);
		result = mem_r(memaddr, write_size, &buf[data]);

		if (result)
			return result;
	}

	return 0;
}

ssize_t inv_dmp_firmware_read(struct file *filp,
				struct kobject *kobj,
				struct bin_attribute *bin_attr,
				char *buf, loff_t off, size_t count)
{
	int result, offset;
	struct iio_dev *indio_dev;
	struct inv_mpu_state *st;

	indio_dev = dev_get_drvdata(container_of(kobj, struct device, kobj));
	st = iio_priv(indio_dev);

	if (!st->chip_config.firmware_loaded)
		return -EINVAL;

	mutex_lock(&indio_dev->mlock);
	offset = off;
	result = inv_dmp_read(st, offset, count, buf);
	set_inv_enable(indio_dev);
	mutex_unlock(&indio_dev->mlock);
	if (result)
		return result;

	return count;
}
ssize_t inv_soft_iron_matrix_write(struct file *fp, struct kobject *kobj,
	struct bin_attribute *attr, char *buf, loff_t pos, size_t size)
{
	int result;
	struct iio_dev *indio_dev;
	struct inv_mpu_state *st;
	int m[NINE_ELEM], *n, *r;
	int i, j, k;

	indio_dev = dev_get_drvdata(container_of(kobj, struct device, kobj));
	st = iio_priv(indio_dev);

	if (!st->chip_config.firmware_loaded)
		return -EINVAL;
	if (size != SOFT_IRON_MATRIX_SIZE) {
		pr_err("wrong size for soft iron matrix 0x%08x vs 0x%08x\n",
						size, SOFT_IRON_MATRIX_SIZE);
		return -EINVAL;
	}
	n = st->current_compass_matrix;
	r = st->final_compass_matrix;
	for (i = 0; i < NINE_ELEM; i++)
		memcpy((u8 *)&m[i], &buf[i * sizeof(int)], sizeof(int));

	for (i = 0; i < THREE_AXES; i++) {
		for (j = 0; j < THREE_AXES; j++) {
			r[i * THREE_AXES + j] = 0;
			for (k = 0; k < THREE_AXES; k++)
				r[i * THREE_AXES + j] +=
					inv_q30_mult(m[i * THREE_AXES + k],
							n[j + k * THREE_AXES]);
		}
	}
	mutex_lock(&indio_dev->mlock);
	result = inv_switch_power_in_lp(st, true);
	if (result) {
		mutex_unlock(&indio_dev->mlock);
		return result;
	}
	result = inv_write_compass_matrix(st, r);
	if (result) {
		mutex_unlock(&indio_dev->mlock);
		return result;
	}
	mutex_unlock(&indio_dev->mlock);

	return size;
}

ssize_t inv_accel_covariance_write(struct file *fp, struct kobject *kobj,
		struct bin_attribute *attr, char *buf, loff_t pos, size_t size)
{
	int i;
	struct iio_dev *indio_dev;
	struct inv_mpu_state *st;

	indio_dev = dev_get_drvdata(container_of(kobj, struct device, kobj));
	st = iio_priv(indio_dev);

	if (size != ACCEL_COVARIANCE_SIZE)
		return -EINVAL;

	for (i = 0; i < COVARIANCE_SIZE; i++)
		memcpy((u8 *)&st->accel_covariance[i],
					&buf[i * sizeof(int)], sizeof(int));

	return size;
}
ssize_t inv_compass_covariance_write(struct file *fp, struct kobject *kobj,
	struct bin_attribute *attr, char *buf, loff_t pos, size_t size)
{
	int i;
	struct iio_dev *indio_dev;
	struct inv_mpu_state *st;

	indio_dev = dev_get_drvdata(container_of(kobj, struct device, kobj));
	st = iio_priv(indio_dev);

	if (size != COMPASS_COVARIANCE_SIZE)
		return -EINVAL;

	for (i = 0; i < COVARIANCE_SIZE; i++)
		memcpy((u8 *)&st->compass_covariance[i],
					&buf[i * sizeof(int)], sizeof(int));

	return size;

}
ssize_t inv_compass_covariance_cur_write(struct file *fp, struct kobject *kobj,
	struct bin_attribute *attr, char *buf, loff_t pos, size_t size)
{
	int i;
	struct iio_dev *indio_dev;
	struct inv_mpu_state *st;

	indio_dev = dev_get_drvdata(container_of(kobj, struct device, kobj));
	st = iio_priv(indio_dev);

	if (size != COMPASS_COVARIANCE_SIZE)
		return -EINVAL;

	for (i = 0; i < COVARIANCE_SIZE; i++)
		memcpy((u8 *)&st->curr_compass_covariance[i],
					&buf[i * sizeof(int)], sizeof(int));

	return size;
}

static int inv_dmp_covar_read(struct inv_mpu_state *st,
						int off, int size, u8 *buf)
{
	int data, result, i;

	result = inv_switch_power_in_lp(st, true);
	if (result)
		return result;
	inv_stop_dmp(st);
	for (i = 0; i < COVARIANCE_SIZE * sizeof(int); i += sizeof(int)) {
		result = read_be32_from_mem(st, (u32 *)&data, off + i);
		if (result)
			return result;
		memcpy(buf + i, (u8 *)&data, sizeof(int));
	}

	return 0;
}
ssize_t inv_compass_covariance_cur_read(struct file *filp,
				struct kobject *kobj,
				struct bin_attribute *bin_attr,
				char *buf, loff_t off, size_t count)
{
	int result;
	struct iio_dev *indio_dev;
	struct inv_mpu_state *st;

	indio_dev = dev_get_drvdata(container_of(kobj, struct device, kobj));
	st = iio_priv(indio_dev);

	if (!st->chip_config.firmware_loaded)
		return -EINVAL;

	mutex_lock(&indio_dev->mlock);
	result = inv_dmp_covar_read(st, CPASS_COVARIANCE_CUR, count, buf);
	set_inv_enable(indio_dev);
	mutex_unlock(&indio_dev->mlock);
	if (result)
		return result;

	return count;
}

ssize_t inv_compass_covariance_read(struct file *filp,
				struct kobject *kobj,
				struct bin_attribute *bin_attr,
				char *buf, loff_t off, size_t count)
{
	int result;
	struct iio_dev *indio_dev;
	struct inv_mpu_state *st;

	indio_dev = dev_get_drvdata(container_of(kobj, struct device, kobj));
	st = iio_priv(indio_dev);

	if (!st->chip_config.firmware_loaded)
		return -EINVAL;

	mutex_lock(&indio_dev->mlock);
	result = inv_dmp_covar_read(st, CPASS_COVARIANCE, count, buf);
	set_inv_enable(indio_dev);
	mutex_unlock(&indio_dev->mlock);
	if (result)
		return result;

	return count;
}

ssize_t inv_accel_covariance_read(struct file *filp,
				struct kobject *kobj,
				struct bin_attribute *bin_attr,
				char *buf, loff_t off, size_t count)
{
	int result;
	struct iio_dev *indio_dev;
	struct inv_mpu_state *st;

	indio_dev = dev_get_drvdata(container_of(kobj, struct device, kobj));
	st = iio_priv(indio_dev);

	if (!st->chip_config.firmware_loaded)
		return -EINVAL;

	mutex_lock(&indio_dev->mlock);
	result = inv_dmp_covar_read(st, ACCEL_COVARIANCE, count, buf);
	set_inv_enable(indio_dev);
	mutex_unlock(&indio_dev->mlock);
	if (result)
		return result;

	return count;
}

ssize_t inv_activity_read(struct file *filp,
				struct kobject *kobj,
				struct bin_attribute *bin_attr,
				char *buf, loff_t off, size_t count)
{
	int copied;
	struct iio_dev *indio_dev;
	struct inv_mpu_state *st;
	u8 ddd[128];

	indio_dev = dev_get_drvdata(container_of(kobj, struct device, kobj));
	st = iio_priv(indio_dev);

	mutex_lock(&indio_dev->mlock);
	copied = kfifo_out(&st->kf, ddd, count);
	memcpy(buf, ddd, copied);
	mutex_unlock(&indio_dev->mlock);

	return copied;
}

ssize_t inv_load_dmp_bias(struct inv_mpu_state *st, int *out)
{
	inv_switch_power_in_lp(st, true);
	read_be32_from_mem(st, &out[0], ACCEL_BIAS_X);
	read_be32_from_mem(st, &out[1], ACCEL_BIAS_Y);
	read_be32_from_mem(st, &out[2], ACCEL_BIAS_Z);

	read_be32_from_mem(st, &out[3], GYRO_BIAS_X);
	read_be32_from_mem(st, &out[4], GYRO_BIAS_Y);
	read_be32_from_mem(st, &out[5], GYRO_BIAS_Z);

	read_be32_from_mem(st, &out[6], CPASS_BIAS_X);
	read_be32_from_mem(st, &out[7], CPASS_BIAS_Y);
	read_be32_from_mem(st, &out[8], CPASS_BIAS_Z);

	inv_switch_power_in_lp(st, false);
	return 0;
}

ssize_t inv_set_dmp_bias(struct inv_mpu_state *st, int *data)
{
	int result;
	inv_switch_power_in_lp(st, true);
	result = write_be32_to_mem(st, data[0], ACCEL_BIAS_X);
	result += write_be32_to_mem(st, data[1], ACCEL_BIAS_Y);
	result += write_be32_to_mem(st, data[2], ACCEL_BIAS_Z);

	if (result)
		return result;
	result = write_be32_to_mem(st, data[3], GYRO_BIAS_X);
	result += write_be32_to_mem(st, data[4], GYRO_BIAS_Y);
	result += write_be32_to_mem(st, data[5], GYRO_BIAS_Z);

	if (result)
		return result;

	result = write_be32_to_mem(st, data[6], CPASS_BIAS_X);
	result += write_be32_to_mem(st, data[7], CPASS_BIAS_Y);
	result += write_be32_to_mem(st, data[8], CPASS_BIAS_Z);

	if (result)
		return result;

	inv_switch_power_in_lp(st, false);

	return 0;
}
