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

#include "inv_mpu_iio.h"

/* AVAGO APDS-9930 */
#define APDS9930_ENABLE_REG     0x00
#define APDS9930_ATIME_REG      0x01
#define APDS9930_PTIME_REG      0x02
#define APDS9930_WTIME_REG      0x03
#define APDS9930_AILTL_REG      0x04
#define APDS9930_AILTH_REG      0x05
#define APDS9930_AIHTL_REG      0x06
#define APDS9930_AIHTH_REG      0x07
#define APDS9930_PILTL_REG      0x08
#define APDS9930_PILTH_REG      0x09
#define APDS9930_PIHTL_REG      0x0A
#define APDS9930_PIHTH_REG      0x0B
#define APDS9930_PERS_REG       0x0C
#define APDS9930_CONFIG_REG     0x0D
#define APDS9930_PPCOUNT_REG    0x0E
#define APDS9930_CONTROL_REG    0x0F
#define APDS9930_REV_REG        0x11
#define APDS9930_ID_REG         0x12
#define APDS9930_STATUS_REG     0x13
#define APDS9930_CDATAL_REG     0x14
#define APDS9930_CDATAH_REG     0x15
#define APDS9930_IRDATAL_REG    0x16
#define APDS9930_IRDATAH_REG    0x17
#define APDS9930_PDATAL_REG     0x18
#define APDS9930_PDATAH_REG     0x19

#define LIGHT_SENSOR_RATE_SCALE  100

#define CMD_BYTE                0x80
#define CMD_WORD                0xA0
#define CMD_SPECIAL             0xE0
#define DATA_ATIME              0xDB
#define DATA_PPCOUNT            0x08
#define DATA_CONFIG             0x00
#define DATA_CONTROL            0x20
#define DATA_ENABLE             0x03

#define GA                      49
#define B                       186
#define C                       74
#define D                       129
#define CALC_RESOL              100
#define DF                      52
#define AGAIN                   1
#define MIN_ATIME               273
#define PON                     1
#define AEN                     0x2
#define PEN                     0x4
#define WEN                     0x8

#define ALS_89XX_BYTES          9
#define ALS_99XX_BYTES          8

static bool secondary_resume_state;

static int setup_apds9930_avago(struct inv_mpu_state *st)
{
	int result;
	u8 addr;

	addr = st->plat_data.read_only_i2c_addr;
	/* power off the chip to setup */
	result = inv_execute_write_secondary(st, 2, addr,
				CMD_BYTE | APDS9930_ENABLE_REG, 0x0);
	if (result)
		return result;

	/* Write Atime, multiples of 50ms */
	result = inv_execute_write_secondary(st, 2, addr,
				CMD_BYTE | APDS9930_ATIME_REG, DATA_ATIME);
	if (result)
		return result;

	/* Leave PTIME register at reset value of 0xFF */
	/* Leave WTIME register at reset value of 0xFF */

	/* write  config register register */
	result = inv_execute_write_secondary(st, 2, addr,
				CMD_BYTE | APDS9930_CONFIG_REG, DATA_CONFIG);
	if (result)
		return result;

	/* write pp count register */
	result = inv_execute_write_secondary(st, 2, addr,
				CMD_BYTE | APDS9930_PPCOUNT_REG, DATA_PPCOUNT);
	if (result)
		return result;
	/* write control register */
	result = inv_execute_write_secondary(st, 2, addr,
				CMD_BYTE | APDS9930_CONTROL_REG, DATA_CONTROL);

	return result;
}

static int resume_apds9930_avago(struct inv_mpu_state *st)
{
	int result, start, bytes;
	u8 addr;
	u8 d[1];
	
	result = inv_plat_single_write(st, REG_USER_CTRL, st->i2c_dis);
	if (result)
		return result;
	addr = st->plat_data.read_only_i2c_addr;

	/* setup the secondary bus to default speed */
	result = inv_set_bank(st, BANK_SEL_3);
	if (result)
		return result;
	result = inv_plat_single_write(st, REG_I2C_MST_ODR_CONFIG,
							MIN_MST_ODR_CONFIG);
	if (result)
		return result;

	/* clear up the ctrl register to avoid interference of setup */
	if (!st->chip_config.compass_enable) {
		result = inv_plat_single_write(st, st->slv_reg[0].ctrl, 0);
		if (result)
			return result;
	}

	/* enable ALS only and power on */
	result = inv_execute_write_secondary(st, 2, addr,
				CMD_BYTE | APDS9930_ENABLE_REG,
				PON | AEN | PEN | WEN);
	if (result)
		return result;
	if (st->chip_config.compass_enable &&
			((st->plat_data.sec_slave_id == COMPASS_ID_AK8975) ||
			(st->plat_data.sec_slave_id == COMPASS_ID_AK8963))) {
		start =  APDS9930_ID_REG;
		bytes =  ALS_89XX_BYTES;
	} else {
		start =  APDS9930_STATUS_REG;
		bytes =  ALS_99XX_BYTES;
	}
	/* dummy read */
	result = inv_execute_read_secondary(st, 2, addr,
						CMD_WORD | start, 1, d);

	result = inv_set_bank(st, BANK_SEL_3);
	if (result)
		return result;
	result = inv_read_secondary(st, 2, addr, CMD_WORD | start, bytes);
	if (result)
		return result;
	result = inv_set_bank(st, BANK_SEL_0);

	secondary_resume_state = true;

	return result;
}

static int suspend_apds9930_avago(struct inv_mpu_state *st)
{
	int result;
	u8 addr;

	if (!secondary_resume_state)
		return 0;
	result = inv_plat_single_write(st, REG_USER_CTRL, st->i2c_dis);
	if (result)
		return result;

	addr = st->plat_data.read_only_i2c_addr;
	result = inv_set_bank(st, BANK_SEL_3);
	if (result)
		return result;
	result = inv_plat_single_write(st, REG_I2C_MST_ODR_CONFIG,
							MIN_MST_ODR_CONFIG);
	if (result)
		return result;
	if (!st->chip_config.compass_enable) {
		result = inv_plat_single_write(st, st->slv_reg[0].ctrl, 0);
		if (result)
			return result;
	}
	/* disable 9930 */
	result = inv_execute_write_secondary(st, 2, addr,
					CMD_BYTE | APDS9930_ENABLE_REG, 0);
	if (result)
		return result;

	/* slave 2 is disabled */
	result = inv_set_bank(st, BANK_SEL_3);
	if (result)
		return result;
	result = inv_plat_single_write(st, REG_I2C_SLV2_CTRL, 0);
	if (result)
		return result;

	result = inv_set_bank(st, BANK_SEL_0);

	secondary_resume_state = false;

	return result;
}

static int read_data_apds9930_avago(struct inv_mpu_state *st, s16 *o)
{
	u8 *d;
	u16 ch0, ch1;
	int ALSIT, IAC1, IAC2, IAC, LPC;

	d = st->fifo_data;

	ch0 = ((d[2] << 8) | d[1]);
	ch1 = ((d[4] << 8) | d[3]);
	ALSIT = MIN_ATIME * (0x100 - DATA_ATIME);
	IAC1 = ch0 * CALC_RESOL - B * ch1;
	IAC2 = C * ch0 - D * ch1;
	IAC = max(IAC1, IAC2);
	IAC = max(IAC, 0) / CALC_RESOL;
	LPC = GA * DF * CALC_RESOL / (ALSIT * AGAIN);
	o[0] = IAC * LPC / CALC_RESOL;
	o[1] = (short)((d[6] << 8) | d[5]);
	o[2] = 0;

	return 0;
}

static struct inv_mpu_slave slave_apds9930 = {
	.suspend   = suspend_apds9930_avago,
	.resume    = resume_apds9930_avago,
	.setup     = setup_apds9930_avago,
	.read_data = read_data_apds9930_avago,
	.rate_scale = LIGHT_SENSOR_RATE_SCALE,
};

int inv_mpu_setup_als_slave(struct inv_mpu_state *st)
{
	switch (st->plat_data.read_only_slave_id) {
	case ALS_ID_APDS_9930:
		st->slave_als = &slave_apds9930;
		break;
	default:
		return -EINVAL;
	}

	return st->slave_als->setup(st);
}

