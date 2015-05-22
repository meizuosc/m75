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
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/wakelock.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <mach/eint.h>
#include <mach/mt_gpio.h>
#include <cust_eint.h>
#include "cust_gpio_usage.h"

#include <linux/types.h>
#include <linux/stat.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

#define inv_printk  printk

static DEFINE_MUTEX(inv_mutex_sensor);
struct wake_lock invThread_lock_sensor;

void wake_up_inv_sensor(void);
static DECLARE_WAIT_QUEUE_HEAD(inv_thread_wq);
static kal_bool inv_thread_timeout = KAL_FALSE;


static int inv_process_dmp_data(struct inv_mpu_state *st);
static char iden[] = {1, 0, 0, 0, 1, 0, 0, 0, 1};

static int inv_push_marker_to_buffer(struct inv_mpu_state *st, u16 hdr)
{
	struct iio_dev *indio_dev = iio_priv_to_dev(st);
	u8 buf[IIO_BUFFER_BYTES];

	memcpy(buf, &hdr, sizeof(hdr));
	iio_push_to_buffers(indio_dev, buf);

	return 0;
}

/*
 *  inv_irq_handler() - Cache a timestamp at each data ready interrupt.
 */
#ifdef QUEUE_WORK
extern struct inv_mpu_state *inv_st;
#endif
static void inv_irq_handler(void)
{

#ifdef QUEUE_WORK
	 queue_work(inv_st->fifo_queue, &inv_st->fifo_work);
#else
	 wake_up_inv_sensor();
#endif

}

static int inv_update_dmp_ts(struct inv_mpu_state *st, int ind)
{
	int i;
	u32 counter;
	u64 ts;
	enum INV_ENGINE en_ind;

	ts = st->last_isr_time - st->sensor[ind].time_calib;
	counter = st->sensor[ind].sample_calib;
	en_ind = st->sensor[ind].engine_base;

	if (ts < 2 * NSEC_PER_SEC)
		return 0;

	if (!st->sensor[ind].calib_flag) {
		st->sensor[ind].sample_calib = 0;
		st->sensor[ind].time_calib = st->last_isr_time;
		st->sensor[ind].calib_flag = 1;
		return 0;
	}

	if (ts > 4 * ((u64)NSEC_PER_SEC)) {
		while (ts > (4 * (u64)NSEC_PER_SEC)) {
			ts >>= 1;
			counter >>= 1;
		}
	}
	if ((counter > 0) &&
		(st->last_isr_time - st->eng_info[en_ind].last_update_time >
		2 * NSEC_PER_SEC)) {
		st->sensor[ind].dur = ((u32)ts) / counter;
		st->eng_info[en_ind].dur = st->sensor[ind].dur /
							st->sensor[ind].div;
		st->eng_info[en_ind].base_time = (st->eng_info[en_ind].dur /
						st->eng_info[en_ind].divider) *
						st->eng_info[en_ind].orig_rate;
		st->eng_info[en_ind].last_update_time = st->last_isr_time;
		for (i = 0; i < SENSOR_NUM_MAX; i++) {
			if (st->sensor[i].on &&
					(st->sensor[i].engine_base == en_ind))
				st->sensor[i].dur = st->sensor[i].div *
						st->eng_info[en_ind].dur;
		}

	}
	st->sensor[ind].sample_calib = 0;
	st->sensor[ind].time_calib = st->last_isr_time;

	return 0;
}

static int be32_to_int(u8 *d)
{
	return (d[0] << 24) | (d[1] << 16) | (d[2] << 8) | d[3];
}

static int inv_push_16bytes_buffer(struct inv_mpu_state *st, u16 hdr,
							u64 t, int *q)
{
	struct iio_dev *indio_dev = iio_priv_to_dev(st);
	u8 buf[IIO_BUFFER_BYTES];
	int i;

	memcpy(buf, &hdr, sizeof(hdr));
	memcpy(buf + 4, &q[0], sizeof(q[0]));
	iio_push_to_buffers(indio_dev, buf);
	for (i = 0; i < 2; i++)
		memcpy(buf + 4 * i, &q[i + 1], sizeof(q[i]));
	iio_push_to_buffers(indio_dev, buf);
	memcpy(buf, &t, sizeof(t));
	iio_push_to_buffers(indio_dev, buf);

	return 0;
}

static int inv_push_16bytes_buffer_accuracy(struct inv_mpu_state *st, u16 hdr,
						u64 t, int *q, s16 accur)
{
	struct iio_dev *indio_dev = iio_priv_to_dev(st);
	u8 buf[IIO_BUFFER_BYTES];
	int i;

	memcpy(buf, &hdr, sizeof(hdr));
	memcpy(buf + 2, &accur, sizeof(accur));
	memcpy(buf + 4, &q[0], sizeof(q[0]));
	iio_push_to_buffers(indio_dev, buf);
	for (i = 0; i < 2; i++)
		memcpy(buf + 4 * i, &q[i + 1], sizeof(q[i]));
	iio_push_to_buffers(indio_dev, buf);
	memcpy(buf, &t, sizeof(t));
	iio_push_to_buffers(indio_dev, buf);

	return 0;
}

static int inv_push_8bytes_buffer(struct inv_mpu_state *st, u16 hdr,
							u64 t, s16 *d)
{
	struct iio_dev *indio_dev = iio_priv_to_dev(st);
	u8 buf[IIO_BUFFER_BYTES];
	int i;

	memcpy(buf, &hdr, sizeof(hdr));
	for (i = 0; i < 3; i++)
		memcpy(&buf[2 + i * 2], &d[i], sizeof(d[i]));
	iio_push_to_buffers(indio_dev, buf);
	memcpy(buf, &t, sizeof(t));
	iio_push_to_buffers(indio_dev, buf);

	return 0;
}

static void inv_push_step_indicator(struct inv_mpu_state *st, u64 t)
{
	s16 sen[3];
#define STEP_INDICATOR_HEADER 0x0001

	sen[0] = 0;
	sen[1] = 0;
	sen[2] = 0;
	inv_push_8bytes_buffer(st, STEP_INDICATOR_HEADER, t, sen);
}
static int inv_apply_soft_iron(struct inv_mpu_state *st, s16 *out_1, s32 *out_2)
{
	int *r, i, j;
	s64 tmp;

	r = st->final_compass_matrix;
	for (i = 0; i < THREE_AXES; i++) {
		tmp = 0;
		for (j = 0; j < THREE_AXES; j++)
			tmp  +=
			(s64)r[i * THREE_AXES + j] * (((int)out_1[j]) << 16);
		out_2[i] = (int)(tmp >> 30);
	}

	return 0;
}

static void inv_convert_and_push_16bytes(struct inv_mpu_state *st, u16 hdr,
							u8 *d, u64 t, s8 *m)
{
	int i, j;
	s32 in[3], out[3];

	for (i = 0; i < 3; i++)
		in[i] = be32_to_int(d + i * 4);
	/* multiply with orientation matrix can be optimized like this */
	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++)
			if (m[i * 3 + j])
				out[i] = in[j] * m[i * 3 + j];

	inv_push_16bytes_buffer(st, hdr, t, out);
}

static void inv_convert_and_push_8bytes(struct inv_mpu_state *st, u16 hdr,
							u8 *d, u64 t, s8 *m)
{
	int i, j;
	s16 in[3], out[3];

	for (i = 0; i < 3; i++)
		in[i] = be16_to_cpup((__be16 *)(d + i * 2));

	/* multiply with orientation matrix can be optimized like this */
	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++)
			if (m[i * 3 + j])
				out[i] = in[j] * m[i * 3 + j];

	inv_push_8bytes_buffer(st, hdr, t, out);
}

static int inv_push_sensor(struct inv_mpu_state *st, int ind, u64 t, u8 *d)
{
	int i, res;
	s16 out_1[3];
	s32 out_2[3];
	u16 hdr, accur;

	hdr = st->sensor[ind].header;
	res = 0;

	switch (ind) {
	case SENSOR_ACCEL:
		inv_convert_and_push_16bytes(st, hdr, d, t, iden);
		break;
	case SENSOR_GYRO:
		inv_convert_and_push_8bytes(st, hdr, d, t, iden);
		break;
	case SENSOR_COMPASS:
		for (i = 0; i < 6; i++)
			st->fifo_data[i] = d[i];
		res = st->slave_compass->read_data(st, out_1);
		/* bad compass handling */
		if ((out_1[0] == BAD_COMPASS_DATA) &&
				(out_1[1] == BAD_COMPASS_DATA) &&
				(out_1[2] == BAD_COMPASS_DATA))
			hdr = COMPASS_HDR_2;
		inv_apply_soft_iron(st, out_1, out_2);
		inv_push_16bytes_buffer(st, hdr, t, out_2);
		break;
	case SENSOR_ALS:
		for (i = 0; i < 8; i++)
			st->fifo_data[i] = d[i];
		if (st->chip_config.has_als) {
			res = st->slave_als->read_data(st, out_1);
			inv_push_8bytes_buffer(st, hdr, t, out_1);

			return res;
		}
		break;
	case SENSOR_SIXQ:
		inv_convert_and_push_16bytes(st, hdr, d, t, iden);
		break;
	case SENSOR_NINEQ:
	case SENSOR_GEOMAG:
		for (i = 0; i < 3; i++)
			out_2[i] = be32_to_int(d + i * 4);
		accur = be16_to_cpup((__be16 *)(d + 3 * 4));
		inv_push_16bytes_buffer_accuracy(st, hdr, t, out_2, accur);
		break;
	case SENSOR_PEDQ:
		inv_convert_and_push_8bytes(st, hdr, d, t, iden);
		break;
	case SENSOR_PRESSURE:
		for (i = 0; i < 6; i++)
			st->fifo_data[i] = d[i];
		if (st->chip_config.has_pressure &&
					(ICM20628 == st->chip_type)) {
			res = st->slave_pressure->read_data(st, out_1);
			inv_push_8bytes_buffer(st, hdr, t, out_1);
		} else {
			for (i = 0; i < 3; i++)
				out_1[i] = be16_to_cpup((__be16 *)(d + i * 2));

			inv_push_8bytes_buffer(st, hdr, t, out_1);
		}
		break;
	case SENSOR_CALIB_GYRO:
		inv_convert_and_push_16bytes(st, hdr, d, t, iden);
		break;
	case SENSOR_CALIB_COMPASS:
		inv_convert_and_push_16bytes(st, hdr, d, t,
						st->plat_data.orientation);
		break;
	}

	return res;
}

static int inv_get_packet_size(struct inv_mpu_state *st, u16 hdr,
							u32 *pk_size, u8 *dptr)
{
	int i, size;
	u16 hdr2;

	size = HEADER_SZ;
	for (i = 0; i < SENSOR_NUM_MAX; i++) {
		if (hdr & st->sensor[i].output) {
			if (st->sensor[i].on)
				size += st->sensor[i].sample_size;
			else
				return -EINVAL;
		}
	}
	if (hdr & HEADER2_SET) {
		size += HEADER2_SZ;
		hdr2 = be16_to_cpup((__be16 *)(dptr + 2));
		for (i = 0; i < SENSOR_ACCURACY_NUM_MAX; i++) {
			if (hdr2 & st->sensor_accuracy[i].output) {
				if (st->sensor_accuracy[i].on)
					size +=
					st->sensor_accuracy[i].sample_size;
				else
					return -EINVAL;
			}
		}
	}
	if ((!st->chip_config.step_indicator_on) && (hdr & PED_STEPIND_SET)) {
		pr_err("ERROR: step inditor should not be here=%x\n", hdr);
		return -EINVAL;
	}
	if (hdr & PED_STEPDET_SET) {
		if (st->chip_config.step_detector_on) {
			size += PED_STEPDET_TIMESTAMP_SZ;
		} else {
			pr_err("ERROR: step detector should not be here\n");
			return -EINVAL;
		}
	}
	*pk_size = size;

	return 0;
}

static int inv_push_accuracy(struct inv_mpu_state *st, int ind, u8 *d)
{
	struct iio_dev *indio_dev = iio_priv_to_dev(st);
	u8 buf[IIO_BUFFER_BYTES];
	u16 hdr, accur;

	hdr = st->sensor_accuracy[ind].header;
	accur = be16_to_cpup((__be16 *)(d));
	if (st->sensor_acurracy_flag[ind]) {
		if (!accur)
			accur = DEFAULT_ACCURACY;
		else
			st->sensor_acurracy_flag[ind] = 0;
	}
	memcpy(buf, &hdr, sizeof(hdr));
	memcpy(buf + sizeof(hdr), &accur, sizeof(accur));
	iio_push_to_buffers(indio_dev, buf);

	return 0;
}

static int inv_get_dmp_ts(struct inv_mpu_state *st, int i)
{
	bool ts_set;
	u64 ts;
	int threshold, dur;

	ts_set = false;
	ts = st->sensor[i].ts + st->sensor[i].dur;
	threshold = (st->sensor[i].dur >> 6);
	if ((st->header_count == 1) && (!st->batch.on)) {
		if ((st->last_isr_time >= st->sensor[i].ts +
					st->sensor[i].dur - NSEC_PER_MSEC) ||
			(st->last_isr_time >=
					st->last_run_time - NSEC_PER_MSEC)) {
			ts = st->last_isr_time;
			ts_set = true;
			inv_update_dmp_ts(st, i);
		}
	}
	if (!ts_set) {
		if (ts > st->last_run_time)
			ts = st->last_run_time - NSEC_PER_MSEC;
	}
	if (st->sensor[i].calib_flag) {
		dur = ts - st->sensor[i].ts;
		if (dur > st->sensor[i].dur + threshold)
			st->sensor[i].ts += (st->sensor[i].dur + threshold);
		else if (dur < st->sensor[i].dur - threshold)
			st->sensor[i].ts += (st->sensor[i].dur - threshold);
		else
			st->sensor[i].ts = ts;
	} else {
		st->sensor[i].ts = ts;
	}
	return 0;
}
static int inv_parse_packet(struct inv_mpu_state *st, u16 hdr, u8 *dptr)
{
	int i;
	u32 tmp;
	s16 s[3];
	u64 t;
	u16 hdr2 = 0;
	bool data_header;

	t = 0;
	if (hdr & HEADER2_SET) {
		hdr2 = be16_to_cpup((__be16 *)(dptr));
		dptr += HEADER2_SZ;
	}

	data_header = false;
	for (i = 0; i < SENSOR_NUM_MAX; i++) {
		if (hdr & st->sensor[i].output) {
			inv_get_dmp_ts(st, i);
			st->sensor[i].sample_calib++;
			inv_push_sensor(st, i, st->sensor[i].ts, dptr);
			dptr += st->sensor[i].sample_size;
			t = st->sensor[i].ts;
			data_header = true;
		}
	}
	if (data_header)
		st->header_count--;
	if (hdr & PED_STEPDET_SET) {
		tmp = be32_to_int(dptr);
		tmp = inv_get_cntr_diff(tmp, st->start_dmp_counter);

		t = st->step_detector_base_ts + (u64)tmp *
						st->eng_info[ENGINE_ACCEL].dur;
		s[0] = 0;
		s[1] = 0;
		s[2] = 0;
		inv_push_8bytes_buffer(st, STEP_DETECTOR_HDR, t, s);
		dptr += PED_STEPDET_TIMESTAMP_SZ;
	}
	if (hdr & PED_STEPIND_MASK)
		inv_push_step_indicator(st, t);
	if (hdr2) {
		for (i = 0; i < SENSOR_ACCURACY_NUM_MAX; i++) {
			if (hdr2 & st->sensor_accuracy[i].output) {
				inv_push_accuracy(st, i, dptr);
				dptr += st->sensor_accuracy[i].sample_size;
			}
		}
	}
	return 0;
}

static int inv_pre_parse_packet(struct inv_mpu_state *st, u16 hdr, u8 *dptr)
{
	int i;
	u16 hdr2 = 0;
	bool data_header;

	if (hdr & HEADER2_SET) {
		hdr2 = be16_to_cpup((__be16 *)(dptr));
		dptr += HEADER2_SZ;
	}

	data_header = false;
	for (i = 0; i < SENSOR_NUM_MAX; i++) {
		if (hdr & st->sensor[i].output) {
			dptr += st->sensor[i].sample_size;
			data_header = true;
		}
	}
	if (data_header)
		st->header_count++;
	if (hdr & PED_STEPDET_SET)
		dptr += PED_STEPDET_TIMESTAMP_SZ;
	if (hdr2) {
		if (hdr2 & ACT_RECOG_SET)
			dptr += ACT_RECOG_SZ;

		for (i = 0; i < SENSOR_ACCURACY_NUM_MAX; i++) {
			if (hdr2 & st->sensor_accuracy[i].output)
				dptr += st->sensor_accuracy[i].sample_size;
		}
	}
	return 0;
}

static int inv_prescan_data(struct inv_mpu_state *st, u8 *dptr, int len)
{
	int res, pk_size;
	bool done_flag;
	u16 hdr;

	done_flag = false;
	st->header_count = 0;
	while (!done_flag) {
		if (len > HEADER_SZ) {
			hdr = (u16)be16_to_cpup((__be16 *)(dptr));
			if (!hdr) {
				pr_err("error header zero\n");
				st->left_over_size = 0;
				return -EINVAL;
			}
			res = inv_get_packet_size(st, hdr, &pk_size, dptr);
			if (res) {
				pr_err("error in header parsing=%x\n", hdr);
				st->left_over_size = 0;

				return -EINVAL;
			}
			if (len >= pk_size) {
				inv_pre_parse_packet(st, hdr, dptr + HEADER_SZ);
				len -= pk_size;
				dptr += pk_size;
			} else {
				done_flag = true;
			}
		} else {
			done_flag = true;
		}
	}

	return 0;
}

static u8 fifo_data[HARDWARE_FIFO_SIZE];

static int inv_process_dmp_data(struct inv_mpu_state *st)
{
	int total_bytes, tmp, res, fifo_count, pk_size;
	u8 *dptr, *d;
	u16 hdr;
	u8 data[2];
	bool done_flag;

	if (st->chip_config.dmp_event_int_on)
		return 0;
	res = inv_plat_read(st, REG_FIFO_COUNT_H, FIFO_COUNT_BYTE, data);
	if (res)
		return res;
	fifo_count = be16_to_cpup((__be16 *)(data));
	if (!fifo_count)
		return 0;

    if (fifo_count > HARDWARE_FIFO_SIZE){//--yd
             struct iio_dev *indio_dev = iio_priv_to_dev(st);
             inv_reset_fifo(indio_dev, true);
               return 0;
    }

	st->fifo_count = fifo_count;
	d = fifo_data;

	if (st->left_over_size > LEFT_OVER_BYTES) {
		st->left_over_size = 0;
		return -EINVAL;
	}

	if (st->left_over_size > 0)
		memcpy(d, st->left_over, st->left_over_size);

	dptr = d + st->left_over_size;
	total_bytes = fifo_count;





	while (total_bytes > 0) {
		if (total_bytes < MAX_READ_SIZE)
			tmp = total_bytes;
		else
			tmp = MAX_READ_SIZE;
		res = inv_plat_read(st, REG_FIFO_R_W, tmp, dptr);
		if (res < 0)
			return res;
		dptr += tmp;
		total_bytes -= tmp;
	}
	dptr = d;
	total_bytes = fifo_count + st->left_over_size;

	res = inv_prescan_data(st, dptr, total_bytes);
	if (res)
		return -EINVAL;
	dptr = d;
	done_flag = false;
	while (!done_flag) {
		if (total_bytes > HEADER_SZ) {
			hdr = (u16)be16_to_cpup((__be16 *)(dptr));
			res = inv_get_packet_size(st, hdr, &pk_size, dptr);
			if (res) {
				printk("%s  @@@@@@@@@@@@@@@@@@@@@@@@ error in header parsing=%x\n",__func__, hdr);
				st->left_over_size = 0;

				return -EINVAL;
			}
			if (total_bytes >= pk_size) {
				inv_parse_packet(st, hdr, dptr + HEADER_SZ);
				total_bytes -= pk_size;
				dptr += pk_size;
			} else {
				done_flag = true;
			}
		} else {
			done_flag = true;
		}
	}
	st->left_over_size = total_bytes;
	if (st->left_over_size > LEFT_OVER_BYTES) {
		st->left_over_size = 0;
		return -EINVAL;
	}

	if (st->left_over_size)
		memcpy(st->left_over, dptr, st->left_over_size);

	return 0;
}

static int inv_process_update_ts(struct inv_mpu_state *st, enum INV_SENSORS t)
{
	u32 diff, dur, adj;
	u64 ts;

	ts = st->ts_for_calib - st->sensor[t].time_calib;

	if (ts < 2 * NSEC_PER_SEC)
		return 0;
	if (ts > 4 * ((u64)NSEC_PER_SEC)) {
		while (ts > (4 * (u64)NSEC_PER_SEC)) {
			ts >>= 1;
			st->sensor[t].sample_calib >>= 1;
		}
	}
	if (!st->sensor[t].sample_calib) {
		st->sensor[t].time_calib = st->ts_for_calib;
		return 0;
	}

	diff = (u32)ts;
	dur = diff  / st->sensor[t].sample_calib;
	adj = abs(dur - st->sensor[t].dur);
	adj >>= 3;
	if (adj > (st->sensor[t].dur >> 5))
		adj = (st->sensor[t].dur >> 5);
	if (dur > st->sensor[t].dur)
		st->sensor[t].dur += adj;
	else
		st->sensor[t].dur -= adj;
	st->sensor[t].sample_calib = 0;
	st->sensor[t].time_calib = st->ts_for_calib;

	return 0;

}

static int inv_push_data(struct inv_mpu_state *st, enum INV_SENSORS type)
{
	int result;
	int i, fifo_count, bpm, read_size, f;
	u8 *dptr;
	u8 data[2];
	s16 out[3];
	s32 out_2[3];

	st->ts_for_calib = get_time_ns();
	result = inv_plat_read(st, REG_FIFO_COUNT_H, FIFO_COUNT_BYTE, data);
	if (result)
		return result;
	fifo_count = be16_to_cpup((__be16 *)(data));
	if (!fifo_count)
		return 0;

	dptr = fifo_data;
	if (type == SENSOR_COMPASS)
		bpm = 8;
	else
		bpm = 6;
	f = fifo_count;
	while (f >= bpm) {
		if (f < MAX_READ_SIZE)
			read_size = f;
		else
			read_size = MAX_READ_SIZE;
		read_size = read_size / bpm;
		read_size *= bpm;
		result = inv_plat_read(st, REG_FIFO_R_W, read_size, dptr);
		if (result < 0)
			return result;
		dptr += read_size;
		f -= read_size;
	}
	dptr = fifo_data;
	while (fifo_count >= bpm) {
		st->sensor[type].ts += st->sensor[type].dur;
		if (type == SENSOR_ACCEL) {
			for (i = 0; i < 3; i++) {
				out[i] = be16_to_cpup((__be16 *)(dptr + i * 2));
				out_2[i] = (out[i] << 15);
			}
			inv_push_16bytes_buffer(st, st->sensor[type].header,
						st->sensor[type].ts, out_2);
		} else if (type == SENSOR_COMPASS) {
			for (i = 0; i < 3; i++)
				out[i] = (short)((dptr[i * 2 + 1] << 8) |
							dptr[i * 2 + 2]);
			inv_push_8bytes_buffer(st, st->sensor[type].header,
						st->sensor[type].ts, out);
		} else {
			for (i = 0; i < 3; i++)
				out[i] = be16_to_cpup((__be16 *)(dptr + i * 2));
			inv_push_8bytes_buffer(st, st->sensor[type].header,
						st->sensor[type].ts, out);
		}
		fifo_count -= bpm;
		dptr += bpm;
		st->sensor[type].sample_calib++;
	}
	inv_process_update_ts(st, type);

	return 0;
}

static int inv_set_fifo_read_data(struct inv_mpu_state *st,
					enum INV_SENSORS type, u8 cfg)
{
	int res;

	res = inv_plat_single_write(st, REG_FIFO_CFG, cfg);
	if (res)
		return res;
	res = inv_push_data(st, type);

	return res;
}

static int inv_process_non_dmp_data(struct inv_mpu_state *st)
{
	int res;
	u8 cfg;

	res = 0;
	if (st->sensor[SENSOR_GYRO].on && st->sensor[SENSOR_ACCEL].on) {
		cfg = (BIT_MULTI_FIFO_CFG | BIT_GYRO_FIFO_NUM);
		res = inv_set_fifo_read_data(st, SENSOR_GYRO, cfg);
		if (res)
			return res;
		cfg = (BIT_MULTI_FIFO_CFG | BIT_ACCEL_FIFO_NUM);
		res = inv_set_fifo_read_data(st, SENSOR_ACCEL, cfg);
	} else {
		if (st->sensor[SENSOR_GYRO].on)
			res = inv_push_data(st, SENSOR_GYRO);
		if (st->sensor[SENSOR_ACCEL].on)
			res = inv_push_data(st, SENSOR_ACCEL);
		if (st->sensor[SENSOR_COMPASS].on)
			res = inv_push_data(st, SENSOR_COMPASS);
	}

	return res;
}

static int inv_get_gyro_bias(struct inv_mpu_state *st, int *bias)
{
	int b_addr[] = {GYRO_BIAS_X, GYRO_BIAS_Y, GYRO_BIAS_Z};
	int i, r;

	for (i = 0; i < THREE_AXES; i++) {
		r = read_be32_from_mem(st, &bias[i], b_addr[i]);
		if (r)
			return r;
	}

	return 0;
}
static int inv_process_temp_comp(struct inv_mpu_state *st)
{
	u8 d[2];
	int r, l1, scale_t, curr_temp, i;
	s16 temp;
	s64 tmp, recp;
	bool update_slope;
	struct inv_temp_comp *t_c;
	int s_addr[] = {GYRO_SLOPE_X, GYRO_SLOPE_Y, GYRO_SLOPE_Z};

#define TEMP_COMP_WIDTH  4
#define TEMP_COMP_MID_L  (12 + TEMP_COMP_WIDTH)
#define TEMP_COMP_MID_H  (32 + TEMP_COMP_WIDTH)

	if (st->last_run_time - st->last_temp_comp_time < (NSEC_PER_SEC >> 1))
		return 0;
	st->last_temp_comp_time = st->last_run_time;
	if ((!st->gyro_cal_enable) ||
		(!st->chip_config.gyro_enable) ||
		(!st->chip_config.accel_enable))
		return 0;
	r = inv_plat_read(st, REG_TEMPERATURE, 2, d);
	if (r)
		return r;
	temp = (s16)(be16_to_cpup((short *)d));
	scale_t = TEMPERATURE_OFFSET +
		inv_q30_mult((int)temp << MPU_TEMP_SHIFT, TEMPERATURE_SCALE);
	curr_temp = (scale_t >> MPU_TEMP_SHIFT);

	update_slope = false;
	/* check the lower part of the temperature */
	l1 = abs(curr_temp - TEMP_COMP_MID_L);
	l1 = l1 - TEMP_COMP_WIDTH;
	l1 = l1 - TEMP_COMP_WIDTH;
	t_c = &st->temp_comp;
	if (l1 < 0) {
		t_c->t_lo = temp;
		r = inv_get_gyro_bias(st, t_c->b_lo);
		if (r)
			return r;
		t_c->has_low = true;
		update_slope = true;
	}

	l1 = abs(curr_temp - TEMP_COMP_MID_H);
	l1 = l1 - TEMP_COMP_WIDTH;
	l1 = l1 - TEMP_COMP_WIDTH;
	if (l1 < 0) {
		t_c->t_hi = temp;
		r = inv_get_gyro_bias(st, t_c->b_hi);
		if (r)
			return r;
		t_c->has_high = true;
		update_slope = true;
	}
	if (t_c->has_high && t_c->has_low && update_slope) {
		if (t_c->t_hi != t_c->t_lo) {
			recp = (1 << 30) / (t_c->t_hi - t_c->t_lo);
			for (i = 0; i < THREE_AXES; i++) {
				tmp = recp * (t_c->b_hi[i] - t_c->b_lo[i]);
				t_c->slope[i] = (tmp >> 15);
				r = write_be32_to_mem(st,
						t_c->slope[i], s_addr[i]);
				if (r)
					return r;
			}
		}
	}

	return 0;
}
static int inv_process_dmp_interrupt(struct inv_mpu_state *st)
{
	struct iio_dev *indio_dev = iio_priv_to_dev(st);
	u8 d[1];
	int result;

#define DMP_INT_SMD             0x04
#define DMP_INT_PED             0x08

	if ((!st->smd.on) && (!st->ped.int_on))
		return 0;

	result = inv_plat_read(st, REG_DMP_INT_STATUS, 1, d);
	if (result)
		return result;

	if (d[0] & DMP_INT_SMD) {
		sysfs_notify(&indio_dev->dev.kobj, NULL, "poll_smd");
		st->smd.on = false;
		st->trigger_state = EVENT_TRIGGER;
		set_inv_enable(indio_dev);
	}
	if (d[0] & DMP_INT_PED)
		sysfs_notify(&indio_dev->dev.kobj, NULL, "poll_pedometer");

	return 0;
}
/*
 *  inv_read_fifo() - Transfer data from FIFO to ring buffer.
 */
static unsigned char sensor_read_byte(struct i2c_client *client, unsigned char cmd)
{
	unsigned char     cmd_buf = 0;
    unsigned char     readData = 0;
    int      ret=0;

    cmd_buf = cmd;

    if( client==NULL ) {
    	printk("[sesnor][%s] i2c_client is NULL \n",__func__);
        return -1;
    } else {
    	client->addr = (client->addr & I2C_MASK_FLAG) | I2C_WR_FLAG |I2C_RS_FLAG;
    	ret = i2c_master_send(client, &cmd_buf, (1<<8 | 1));
    }
    if (ret < 0)
    {
		printk("[sensor] %s read data error!!\n",__func__);
        return ret;
    }

    readData = cmd_buf;
    client->addr = client->addr & I2C_MASK_FLAG;

    return readData;
}

//#define DEBUG_ACCEL_BIAS

#ifdef QUEUE_WORK
static void  inv_read_fifo(struct work_struct *work)
{
	struct inv_mpu_state *st = container_of(work, struct inv_mpu_state, fifo_work);
#else
static void  inv_read_fifo(struct inv_mpu_state *st)
{
#endif

	struct iio_dev *indio_dev = iio_priv_to_dev(st);
	int result, min_run_time;
	u64 pts1;
	int ret = 0;

#define NON_DMP_MIN_RUN_TIME (10 * NSEC_PER_MSEC)

	if (st->suspend_state) {
		mt_eint_unmask(st->irq);
		return ;//IRQ_HANDLED;
	}
	mutex_lock(&st->suspend_resume_lock);
	mutex_lock(&indio_dev->mlock);

#ifdef DEBUG_ACCEL_BIAS
	u8 v = 0xff;
	u8 reg = REG_WHO_AM_I;
	st->client->timing = 400;
	inv_set_bank(st, BANK_SEL_0);
	ret = inv_plat_read(st, REG_WHO_AM_I, 1, &v);
	if (ret!=0) {
		printk("%s read REG_WHO_AM_I failed 11\n",__func__);
	} else {
		if( v!=0xa2 ) {
			printk("%s read REG_WHO_AM_I==0x%x ##############################################################\n",__func__, v);
		}
//		else {
//			printk("%s read REG_WHO_AM_I==0x%x\n",__func__, v);
//		}
	}

	int addr,dmp_bias_x=0xffff,dmp_bias_y=0xffff,dmp_bias_z=0xffff;
	addr = ACCEL_BIAS_X;
	result = read_be32_from_mem(st, &dmp_bias_x, addr);
	if( result ) {
		printk("invn: read accel X axis dmp bias is %d\n");
	}

	addr = ACCEL_BIAS_Y;
	result = read_be32_from_mem(st, &dmp_bias_y, addr);
	if( result ) {
		printk("invn: read accel Y axis dmp bias is %d\n");
	}

	addr = ACCEL_BIAS_Z;
	result = read_be32_from_mem(st, &dmp_bias_z, addr);
	if( result ) {
		printk("invn: read accel Z axis dmp bias is %d\n");
	}
	if( dmp_bias_x!=-991232||dmp_bias_y!=-92160||dmp_bias_z!=1544192 ) {
		printk("accel dmp bias is X:%d, Y:%d, Z:%d\n",dmp_bias_x,dmp_bias_y,dmp_bias_z);
		printk("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
	}
#endif


	if (st->chip_config.dmp_on) {
		st->last_run_time = get_time_ns();
		inv_switch_power_in_lp(st, true);
		result = inv_process_dmp_interrupt(st);
		if (result)
			goto end_read_fifo;
		result = inv_process_temp_comp(st);
		if (result)
			goto end_read_fifo;
		result = inv_process_dmp_data(st);
	} else {
		pts1 = get_time_ns();
		if ((!st->sensor[SENSOR_COMPASS].on))
			min_run_time = NON_DMP_MIN_RUN_TIME;
		else
			min_run_time = min((int)NON_DMP_MIN_RUN_TIME,
						st->sensor[SENSOR_COMPASS].dur);

		if (pts1 - st->last_run_time < min_run_time)
			goto end_read_fifo;
		else
			st->last_run_time = pts1;
		result = inv_process_non_dmp_data(st);
	}
	if (result)
		goto err_reset_fifo;

end_read_fifo:
	inv_switch_power_in_lp(st, false);
	mutex_unlock(&indio_dev->mlock);
	mutex_unlock(&st->suspend_resume_lock);

	mt_eint_unmask(st->irq);
	return ;

err_reset_fifo:
	pr_err("error to reset fifo\n");
	inv_reset_fifo(indio_dev, true);
	inv_switch_power_in_lp(st, false);
	mutex_unlock(&indio_dev->mlock);
	mutex_unlock(&st->suspend_resume_lock);

	mt_eint_unmask(st->irq);
	return ;

}

void inv_mpu_unconfigure_ring(struct iio_dev *indio_dev)
{
	struct inv_mpu_state *st = iio_priv(indio_dev);
	free_irq(st->irq, st);
	iio_kfifo_free(indio_dev->buffer);
};

static int inv_predisable(struct iio_dev *indio_dev)
{
	return 0;
}

static int inv_preenable(struct iio_dev *indio_dev)
{
	int result;

	result = iio_sw_buffer_preenable(indio_dev);

	return result;
}

int inv_flush_batch_data(struct iio_dev *indio_dev, bool *has_data)
{
	struct inv_mpu_state *st = iio_priv(indio_dev);
	int result;
	u8 w;

	if (!(iio_buffer_enabled(indio_dev)))
		return -EINVAL;

	if (st->batch.on) {
		st->last_run_time = get_time_ns();
		result = inv_plat_read(st, REG_USER_CTRL, 1, &w);
		w &= ~BIT_DMP_EN;
		result = inv_plat_single_write(st, REG_USER_CTRL, w);
		result = write_be32_to_mem(st, 0, BM_BATCH_CNTR);
		w |= BIT_DMP_EN;
		result = inv_plat_single_write(st, REG_USER_CTRL, w);

		inv_process_dmp_data(st);
		*has_data = !!st->fifo_count;
		inv_push_marker_to_buffer(st, END_MARKER);
		return result;
	}
	inv_push_marker_to_buffer(st, EMPTY_MARKER);

	return 0;
}

static const struct iio_buffer_setup_ops inv_mpu_ring_setup_ops = {
	.preenable = &inv_preenable,
	.predisable = &inv_predisable,
};

#ifndef QUEUE_WORK
void wake_up_inv_sensor(void)
{
    inv_thread_timeout = KAL_TRUE;

    wake_up_interruptible(&inv_thread_wq);
}

int sensor_thread_kthread_mpu7400(void *x)
{

	struct inv_mpu_state *st = (struct inv_mpu_state *)x;

    inv_printk("[sensor_thread_kthread_mpu7400] enter\n");

    /* Run on a process content */
    while (1) {
		wait_event_interruptible(inv_thread_wq, (inv_thread_timeout == KAL_TRUE));
		inv_thread_timeout = KAL_FALSE;

        mutex_lock(&inv_mutex_sensor);
        inv_read_fifo(st);
        mutex_unlock(&inv_mutex_sensor);

        mt_eint_unmask(st->irq);
    }

    return 0;
}
#endif

int inv_mpu_configure_ring(struct iio_dev *indio_dev)
{
	int ret;
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_buffer *ring;

	ring = iio_kfifo_allocate(indio_dev);
	if (!ring)
		return -ENOMEM;
	indio_dev->buffer = ring;
	/* setup ring buffer */
	ring->scan_timestamp = true;
	indio_dev->setup_ops = &inv_mpu_ring_setup_ops;

#ifdef QUEUE_WORK
	st->fifo_queue = create_singlethread_workqueue("mpu7400_fifo_queue");
	INIT_WORK(&st->fifo_work, inv_read_fifo);
	inv_st = st;
#else
    kthread_run(sensor_thread_kthread_mpu7400, st, "inv_sensor_thread");
#endif
	extern void mt_eint_registration(unsigned int eint_num, unsigned int flag,
			void (EINT_FUNC_PTR) (void), unsigned int is_auto_umask);
	// set INT mode
	mt_set_gpio_mode(GPIO_SENSORHUB_EINT_PIN, GPIO_SENSORHUB_EINT_PIN_M_EINT);
	mt_set_gpio_dir(GPIO_SENSORHUB_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_SENSORHUB_EINT_PIN, GPIO_PULL_DISABLE);

	mt_eint_set_sens(st->irq,MT_EDGE_SENSITIVE);
	mt_eint_registration(st->irq, EINTF_TRIGGER_RISING, inv_irq_handler, 0);
	mt_eint_unmask(st->irq);

#if 0
	ret = request_threaded_irq(st->irq, inv_irq_handler,
			inv_read_fifo,
			IRQF_TRIGGER_RISING | IRQF_SHARED, "inv_irq", st);
	if (ret)
		goto error_iio_sw_rb_free;
#endif

	indio_dev->modes |= INDIO_BUFFER_TRIGGERED;

	return 0;
error_iio_sw_rb_free:
	iio_kfifo_free(indio_dev->buffer);

	return ret;
}

