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

#ifndef _INV_MPU_IIO_H_
#define _INV_MPU_IIO_H_

#include <linux/i2c.h>
#include <linux/kfifo.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/mpu.h>

#include "linux/iio/iio.h"
#include "linux/iio/buffer.h"
#include "dmp3Default.h"

#include <linux/earlysuspend.h>
//#define BIAS_CONFIDENCE_HIGH 1

/*
 * when update flyme, inv_cal_data.bin file will be deleted.
 */
#define READ_BIAS_FROM_NVRAM
/*
 * some apps wont turn off sensor even if they dont use it
 */
#define TURN_OFF_SENSOR_WHEN_BACKLIGHT_IS_0

/*register and associated bit definition*/
/* bank 0 register map */
#define REG_WHO_AM_I            0x00

#define REG_USER_CTRL           0x03
#define BIT_DMP_EN                      0x80
#define BIT_FIFO_EN                     0x40
#define BIT_I2C_MST_EN                  0x20
#define BIT_I2C_IF_DIS                  0x10
#define BIT_DMP_RST                     0x08

#define REG_LP_CONFIG           0x05
#define BIT_I2C_MST_CYCLE               0x40
#define BIT_ACCEL_CYCLE                 0x20
#define BIT_GYRO_CYCLE                  0x10

#define REG_PWR_MGMT_1          0x06
#define BIT_H_RESET                     0x80
#define BIT_SLEEP                       0x40
#define BIT_LP_EN                       0x20
#define BIT_CLK_PLL                     0x01

#define REG_PWR_MGMT_2          0x07
#define BIT_PWR_PRESSURE_STBY           0x40
#define BIT_PWR_ACCEL_STBY              0x38
#define BIT_PWR_GYRO_STBY               0x07
#define BIT_PWR_ALL_OFF                 0x7f

#define REG_INT_PIN_CFG         0x0F
#define BIT_BYPASS_EN                   0x2

#define REG_INT_ENABLE          0x10
#define BIT_DMP_INT_EN                  0x02

#define REG_INT_ENABLE_1        0x11
#define BIT_DATA_RDY_3_EN               0x08
#define BIT_DATA_RDY_2_EN               0x04
#define BIT_DATA_RDY_1_EN               0x02
#define BIT_DATA_RDY_0_EN               0x01

#define REG_INT_ENABLE_2        0x12
#define BIT_FIFO_OVERFLOW_EN_0          0x1

#define REG_INT_ENABLE_3        0x13
#define REG_DMP_INT_STATUS      0x18
#define REG_INT_STATUS          0x19
#define REG_INT_STATUS_1        0x1A

#define REG_TEMPERATURE         0x39

#define REG_EXT_SLV_SENS_DATA_00 0x3B
#define REG_EXT_SLV_SENS_DATA_08 0x43
#define REG_EXT_SLV_SENS_DATA_09 0x44
#define REG_EXT_SLV_SENS_DATA_10 0x45

#define REG_FIFO_EN             0x66
#define BIT_SLV_0_FIFO_EN               1

#define REG_FIFO_EN_2           0x67
#define BIT_PRS_FIFO_EN                 0x20
#define BIT_ACCEL_FIFO_EN               0x10
#define BITS_GYRO_FIFO_EN               0x0E

#define REG_FIFO_RST            0x68

#define REG_FIFO_SIZE_0         0x6E
#define BIT_ACCEL_FIFO_SIZE_128         0x00
#define BIT_ACCEL_FIFO_SIZE_256         0x04
#define BIT_ACCEL_FIFO_SIZE_512         0x08
#define BIT_ACCEL_FIFO_SIZE_1024        0x0C
#define BIT_GYRO_FIFO_SIZE_128          0x00
#define BIT_GYRO_FIFO_SIZE_256          0x01
#define BIT_GYRO_FIFO_SIZE_512          0x02
#define BIT_GYRO_FIFO_SIZE_1024         0x03
#define BIT_FIFO_SIZE_1024              0x01
#define BIT_FIFO_SIZE_512               0x00
#define BIT_FIFO_3_SIZE_256             0x40
#define BIT_FIFO_3_SIZE_64              0x00

#define REG_FIFO_COUNT_H        0x70
#define REG_FIFO_R_W            0x72

#define REG_FIFO_CFG            0x76
#define BIT_MULTI_FIFO_CFG              0x01
#define BIT_SINGLE_FIFO_CFG             0x00
#define BIT_GYRO_FIFO_NUM               (0 << 2)
#define BIT_ACCEL_FIFO_NUM              (1 << 2)
#define BIT_PRS_FIFO_NUM                2
#define BIT_EXT_FIFO_NUM                3

#define REG_MEM_START_ADDR      0x7C
#define REG_MEM_R_W             0x7D
#define REG_MEM_BANK_SEL        0x7E

/* bank 1 register map */
#define REG_TIMEBASE_CORRECTION_PLL 0x28
#define REG_TIMEBASE_CORRECTION_RCOSC 0x29
#define REG_SELF_TEST1                0x02
#define REG_SELF_TEST2                0x03
#define REG_SELF_TEST3                0x04
#define REG_SELF_TEST4                0x0E
#define REG_SELF_TEST5                0x0F
#define REG_SELF_TEST6                0x10

/* bank 2 register map */
#define REG_GYRO_SMPLRT_DIV     0x00

#define REG_GYRO_CONFIG_1       0x01
#define SHIFT_GYRO_FS_SEL               1
#define SHIFT_GYRO_DLPCFG               3

#define REG_GYRO_CONFIG_2       0x02
#define BIT_GYRO_CTEN                   0x38

#define REG_ACCEL_SMPLRT_DIV_1  0x10
#define REG_ACCEL_SMPLRT_DIV_2  0x11

#define REG_ACCEL_CONFIG        0x14
#define SHIFT_ACCEL_FS                  1

#define REG_ACCEL_CONFIG_2       0x15
#define BIT_ACCEL_CTEN                  0x1C

#define REG_PRS_ODR_CONFIG      0x20
#define REG_PRGM_START_ADDRH    0x50

#define REG_MOD_CTRL_USR        0x54
#define BIT_ODR_SYNC                    0x7

/* bank 3 register map */
#define REG_I2C_MST_ODR_CONFIG  0

#define REG_I2C_MST_CTRL         1
#define BIT_I2C_MST_P_NSR       0x10

#define REG_I2C_MST_DELAY_CTRL  0x02
#define BIT_SLV0_DLY_EN                 0x01
#define BIT_SLV1_DLY_EN                 0x02
#define BIT_SLV2_DLY_EN                 0x04
#define BIT_SLV3_DLY_EN                 0x08

#define REG_I2C_SLV0_ADDR       0x03
#define REG_I2C_SLV0_REG        0x04
#define REG_I2C_SLV0_CTRL       0x05
#define REG_I2C_SLV0_DO         0x06

#define REG_I2C_SLV1_ADDR       0x07
#define REG_I2C_SLV1_REG        0x08
#define REG_I2C_SLV1_CTRL       0x09
#define REG_I2C_SLV1_DO         0x0A

#define REG_I2C_SLV2_ADDR       0x0B
#define REG_I2C_SLV2_REG        0x0C
#define REG_I2C_SLV2_CTRL       0x0D
#define REG_I2C_SLV2_DO         0x0E

#define REG_I2C_SLV3_ADDR       0x0F
#define REG_I2C_SLV3_REG        0x10
#define REG_I2C_SLV3_CTRL       0x11
#define REG_I2C_SLV3_DO         0x12

#define REG_I2C_SLV4_CTRL       0x15

#define INV_MPU_BIT_SLV_EN      0x80
#define INV_MPU_BIT_BYTE_SW     0x40
#define INV_MPU_BIT_REG_DIS     0x20
#define INV_MPU_BIT_GRP         0x10
#define INV_MPU_BIT_I2C_READ    0x80

/* register for all banks */
#define REG_BANK_SEL            0x7F
#define BANK_SEL_0                      0x00
#define BANK_SEL_1                      0x10
#define BANK_SEL_2                      0x20
#define BANK_SEL_3                      0x30

/* data definitions */
#define BYTES_PER_SENSOR         6
#define FIFO_COUNT_BYTE          2
#define HARDWARE_FIFO_SIZE       1024
#define FIFO_SIZE                (HARDWARE_FIFO_SIZE * 4 / 5)
#define POWER_UP_TIME            100
#define REG_UP_TIME_USEC         100
#define DMP_RESET_TIME           20
#define GYRO_ENGINE_UP_TIME      50
#define MPU_MEM_BANK_SIZE        256
#define IIO_BUFFER_BYTES         8
#define HEADERED_NORMAL_BYTES    8
#define HEADERED_Q_BYTES         16
#define LEFT_OVER_BYTES          128
#define BASE_SAMPLE_RATE         1125

#ifdef BIAS_CONFIDENCE_HIGH
#define DEFAULT_ACCURACY         3
#else
#define DEFAULT_ACCURACY         1
#endif

#ifdef FREQ_225
#define MPU_DEFAULT_DMP_FREQ     225
#define PEDOMETER_FREQ           (MPU_DEFAULT_DMP_FREQ >> 2)
#define DEFAULT_ACCEL_GAIN       (33554432L * 5 / 11)
#else
#define MPU_DEFAULT_DMP_FREQ     102
#define PEDOMETER_FREQ           (MPU_DEFAULT_DMP_FREQ >> 1)
#define DEFAULT_ACCEL_GAIN       33554432L
#endif
#define PED_ACCEL_GAIN           67108864L
#define ALPHA_FILL_PED           858993459
#define A_FILL_PED               214748365

#define DMP_OFFSET               0x90
#define DMP_IMAGE_SIZE           (7012+ DMP_OFFSET)
#define MIN_MST_ODR_CONFIG       4
#define THREE_AXES               3
#define NINE_ELEM                (THREE_AXES * THREE_AXES)
#define MPU_TEMP_SHIFT           16
#define SOFT_IRON_MATRIX_SIZE    (4 * 9)
#define DMP_DIVIDER              (BASE_SAMPLE_RATE / MPU_DEFAULT_DMP_FREQ)
#define MAX_5_BIT_VALUE          0x1F
#define BAD_COMPASS_DATA         0x7FFF
#define DEFAULT_BATCH_RATE       400
#define DEFAULT_BATCH_TIME    (MSEC_PER_SEC / DEFAULT_BATCH_RATE)
#define MAX_COMPASS_RATE         115
#define MAX_PRESSURE_RATE        30
#define MAX_ALS_RATE             5
#define DATA_AKM_99_BYTES_DMP  10
#define DATA_AKM_89_BYTES_DMP  9
#define DATA_ALS_BYTES_DMP     8
#define APDS9900_AILTL_REG      0x04
#define BMP280_DIG_T1_LSB_REG                0x88
#define COVARIANCE_SIZE          14
#define ACCEL_COVARIANCE_SIZE  (COVARIANCE_SIZE * sizeof(int))
#define COMPASS_COVARIANCE_SIZE  (COVARIANCE_SIZE * sizeof(int))
#define TEMPERATURE_SCALE  3340827L
#define TEMPERATURE_OFFSET 1376256L
#define SECONDARY_INIT_WAIT 60
#define MPU_SOFT_REV_ADDR               0x86
#define MPU_SOFT_REV_MASK               0xf0
#define AK99XX_SHIFT                    23
#define AK89XX_SHIFT                    22

/* this is derived from 1000 divided by 55, which is the pedometer
   running frequency */
#define MS_PER_PED_TICKS         18

/* data limit definitions */
#define MIN_FIFO_RATE            4
#define MAX_FIFO_RATE            MPU_DEFAULT_DMP_FREQ
#define MAX_DMP_OUTPUT_RATE      MPU_DEFAULT_DMP_FREQ
#define MAX_READ_SIZE            128
#define MAX_MPU_MEM              8192
#define MAX_PRS_RATE             281

/* data header defines */
#define PRESSURE_HDR             0x8000
#define ACCEL_HDR                0x4000
#define ACCEL_ACCURACY_HDR       0x4080
#define GYRO_HDR                 0x2000
#define GYRO_ACCURACY_HDR        0x2080
#define COMPASS_HDR              0x1000
#define COMPASS_HDR_2            0x1800
#define CPASS_ACCURACY_HDR       0x1080
#define ALS_HDR                  0x0800
#define SIXQUAT_HDR              0x0400
#define PEDQUAT_HDR              0x0200
#define STEP_DETECTOR_HDR        0x0100
#define ACTIVITY_HDR             0x0180
#define COMPASS_CALIB_HDR        0x0080
#define GYRO_CALIB_HDR           0x0040
#define EMPTY_MARKER             0x0020
#define END_MARKER               0x0010
#define NINEQUAT_HDR             0x0008
#define STEP_INDICATOR_MASK      0x000f

/* init parameters */
#define MPU_INIT_SMD_THLD        1500
#define MPU_INIT_SENSOR_RATE     5
#define MPU_INIT_GYRO_SCALE      3
#define MPU_INIT_ACCEL_SCALE     0
#define MPU_INIT_PED_INT_THRESH  2
#define MPU_INIT_PED_STEP_THRESH 6
#define DMP_START_ADDR           0x8D0

struct inv_mpu_state;

/* device enum */
enum inv_devices {
	ICM20628,
	ICM20728,
	INV_NUM_PARTS
};

enum INV_ENGINE {
	ENGINE_GYRO = 0,
	ENGINE_ACCEL,
	ENGINE_PRESSURE,
	ENGINE_I2C,
	ENGINE_NUM_MAX,
};

/**
 *  struct inv_hw_s - Other important hardware information.
 *  @num_reg:	Number of registers on device.
 *  @name:      name of the chip
 */
struct inv_hw_s {
	u8 num_reg;
	u8 *name;
};

/* enum for sensor
   The sequence is important.
   It represents the order of apperance from DMP */
enum INV_SENSORS {
	SENSOR_ACCEL = 0,
	SENSOR_GYRO,
	SENSOR_COMPASS,
	SENSOR_ALS,
	SENSOR_SIXQ,
	SENSOR_NINEQ,
	SENSOR_GEOMAG,
	SENSOR_PEDQ,
	SENSOR_PRESSURE,
	SENSOR_CALIB_GYRO,
	SENSOR_CALIB_COMPASS,
	SENSOR_NUM_MAX,
	SENSOR_INVALID,
};

/**
 *  struct inv_sensor - information for each sensor.
 *  @ts: this sensors timestamp.
 *  @dur: duration between samples in ns.
 *  @rate:  sensor data rate.
 *  @sample_size: number of bytes for the sensor.
 *  @odr_addr: output data rate address in DMP.
 *  @counter_addr: output counter address in DMP.
 *  @output: output on/off control word.
 *  @header: data header to the user space.
 *  @time_calib: calibrate timestamp.
 *  @sample_calib: calibrate bytes accumulated.
 *  @div:         divider in DMP mode.
 *  @calib_flag:  calibrate flag used to improve the accuracy of estimation.
 *  @on:    sensor on/off.
 *  @a_en:  accel engine requirement.
 *  @g_en:  gyro engine requirement.
 *  @c_en:  compass_engine requirement.
 *  @p_en:  pressure engine requirement.
 *  @engine_base: engine base for this sensor.
 */
struct inv_sensor {
	u64 ts;
	int dur;
	int rate;
	u8 sample_size;
	int odr_addr;
	int counter_addr;
	u16 output;
	u16 header;
	u64 time_calib;
	u32 sample_calib;
	int div;
	bool calib_flag;
	bool on;
	bool a_en;
	bool g_en;
	bool c_en;
	bool p_en;
	enum INV_ENGINE engine_base;
};

enum SENSOR_ACCURACY {
	SENSOR_ACCEL_ACCURACY = 0,
	SENSOR_GYRO_ACCURACY,
	SENSOR_COMPASS_ACCURACY,
	SENSOR_ACCURACY_NUM_MAX,
};

/**
 *  struct inv_batch - information for batchmode.
 *  @on: normal batch mode on.
 *  @default_on: default batch on. This is optimization option.
 *  @overflow_on: overflow mode for batchmode.
 *  @wake_fifo_on: overflow for suspend mode.
 *  @step_only: mean only step detector data is batched.
 *  @post_isr_run: mean post isr has runned once.
 *  @counter: counter for batch mode.
 *  @timeout: nominal timeout value for batchmode in milliseconds.
 *  @max_rate: max rate for all batched sensors.
 *  @engine_base: engine base batch mode should stick to.
 */
struct inv_batch {
	bool on;
	bool default_on;
	bool overflow_on;
	bool wake_fifo_on;
	bool step_only;
	bool post_isr_run;
	u32 counter;
	u32 timeout;
	u32 max_rate;
	enum INV_ENGINE engine_base;
};

/**
 *  struct inv_chip_config_s - Cached chip configuration data.
 *  @fsr:		Full scale range.
 *  @lpf:		Digital low pass filter frequency.
 *  @accel_fs:		accel full scale range.
 *  @accel_enable:	enable accel functionality
 *  @gyro_enable:	enable gyro functionality
 *  @compass_enable:    enable compass functinality.
 *  @als_enable:        enable ALS functionality.
 *  @geomag_enable:     set geomag flag or not.
 *  @pressure_enable:   eanble pressure functionality.
 *  @secondary_enable:  secondary I2C bus enabled or not.
 *  @has_compass:	has secondary I2C compass or not.
 *  @has_pressure:      has secondary I2C pressure or not.
 *  @has_als:           has secondary I2C als or not.
 *  @slave_enable:      secondary I2C interface enabled or not.
 *  @normal_compass_measure: discard first compass data after reset.
 *  @is_asleep:		1 if chip is powered down.
 *  @lp_en_set:         1 if LP_EN bit is set;
 *  @lp_en_mode_off:    debug mode that turns off LP_EN mode off.
 *  @cycle_mode_off:    debug mode that turns off cycle mode.
 *  @clk_sel:           debug_mode that turns on/off clock selection.
 *  @dmp_on:		dmp is on/off.
 *  @dmp_event_int_on:  dmp event interrupt on/off.
 *  @wom_on:        WOM interrupt on. This is an internal variable.
 *  @step_indicator_on: step indicate bit added to the sensor or not.
 *  @step_detector_on:  step detector on or not.
 *  @firmware_loaded:	flag indicate firmware loaded or not.
 *  @low_power_gyro_on: flag indicating low power gyro on/off.
 *  @debug_data_collection_mode_on: debug mode for data collection.
 *  @debug_data_collection_gyro_freq: data collection mode for gyro rate.
 *  @debug_data_collection_accel_freq: data collection mode for accel rate.
 *  @compass_rate:    compass engine rate. Determined by underlying data.
 */
struct inv_chip_config_s {
	u32 fsr:2;
	u32 lpf:3;
	u32 accel_fs:2;
	u32 accel_enable:1;
	u32 gyro_enable:1;
	u32 compass_enable:1;
	u32 als_enable:1;
	u32 geomag_enable:1;
	u32 pressure_enable:1;
	u32 has_compass:1;
	u32 has_pressure:1;
	u32 has_als:1;
	u32 slave_enable:1;
	u32 normal_compass_measure:1;
	u32 is_asleep:1;
	u32 lp_en_set:1;
	u32 lp_en_mode_off:1;
	u32 cycle_mode_off:1;
	u32 clk_sel:1;
	u32 dmp_on:1;
	u32 dmp_event_int_on:1;
	u32 wom_on:1;
	u32 step_indicator_on:1;
	u32 step_detector_on:1;
	u32 firmware_loaded:1;
	u32 low_power_gyro_on:1;
	u32 debug_data_collection_mode_on:1;
	u32 debug_data_collection_gyro_freq;
	u32 debug_data_collection_accel_freq;
	int compass_rate;
};

/**
 *  struct inv_temp_comp - temperature compensation structure.
 *  @t_lo:    raw temperature in low temperature.
 *  @t_hi:    raw temperature in high temperature.
 *  @b_lo:    gyro bias in low temperature.
 *  @b_hi:    gyro bias in high temperature.
 *  @has_low:    flag indicate low temperature parameters is updated.
 *  @has_high:   flag indicates high temperature parameters is updated.
 *  @slope:      slope for temperature compensation.
 */
struct inv_temp_comp {
	int t_lo;
	int t_hi;
	int b_lo[3];
	int b_hi[3];
	bool has_low;
	bool has_high;
	int slope[3];
};

/**
 *  struct inv_chip_info_s - Chip related information.
 *  @product_id:	Product id.
 *  @product_revision:	Product revision.
 *  @silicon_revision:	Silicon revision.
 *  @software_revision:	software revision.
 *  @compass_sens:	compass sensitivity.
 *  @gyro_sens_trim:	Gyro sensitivity trim factor.
 *  @accel_sens_trim:    accel sensitivity trim factor.
 */
struct inv_chip_info_s {
	u8 product_id;
	u8 product_revision;
	u8 silicon_revision;
	u8 software_revision;
	u8 compass_sens[3];
	u32 gyro_sens_trim;
	u32 accel_sens_trim;
};

/**
 * struct inv_smd significant motion detection structure.
 * @threshold: accel threshold for motion detection.
 * @delay: delay time to confirm 2nd motion.
 * @delay2: delay window parameter.
 * @on: smd on/off.
 */
struct inv_smd {
	u32 threshold;
	u32 delay;
	u32 delay2;
	bool on;
};

/**
 * struct inv_ped pedometer related data structure.
 * @step: steps taken.
 * @time: time taken during the period.
 * @last_step_time: last time the step is taken.
 * @step_thresh: step threshold to show steps.
 * @int_thresh: step threshold to generate interrupt.
 * @int_on:   pedometer interrupt enable/disable.
 * @on:  pedometer on/off.
 * @engine_on: pedometer engine on/off.
 */
struct inv_ped {
	u64 step;
	u64 time;
	u64 last_step_time;
	u16 step_thresh;
	u16 int_thresh;
	bool int_on;
	bool on;
	bool engine_on;
};

enum TRIGGER_STATE {
	DATA_TRIGGER = 0,
	RATE_TRIGGER,
	EVENT_TRIGGER,
	MISC_TRIGGER,
	DEBUG_TRIGGER,
};

/**
 *  struct inv_secondary_reg - secondary registers data structure.
 *  @addr:       address of the slave.
 *  @reg: register address of slave.
 *  @ctrl: control register.
 *  @d0: data out register.
 */
struct inv_secondary_reg {
	u8 addr;
	u8 reg;
	u8 ctrl;
	u8 d0;
};

struct inv_secondary_set {
	u8 delay_enable;
	u8 delay_time;
	u8 odr_config;
};
/**
 *  struct inv_engine_info - data structure for engines.
 *  @base_time: base time for each engine.
 *  @divider: divider used to downsample engine rate from original rate.
 *  @running_rate: the actually running rate of engine.
 *  @orig_rate: original rate for each engine before downsample.
 *  @dur: duration for one tick.
 *  @last_update_time: last update time.
 */
struct inv_engine_info {
	u32 base_time;
	u32 divider;
	u32 running_rate;
	u32 orig_rate;
	u32 dur;
	u64 last_update_time;
};

struct inv_mpu_slave;
/**
 *  struct inv_mpu_state - Driver state variables.
 *  @dev:               device address of the current bus, i2c or spi.
 *  @chip_config:	Cached attribute information.
 *  @chip_info:		Chip information from read-only registers.
 *  @trig;              iio trigger.
 *  @smd:               SMD data structure.
 *  @ped:               pedometer data structure.
 *  @batch:             batchmode data structure.
 *  @temp_comp:         gyro temperature compensation structure.
 *  @slave_compass:     slave compass.
 *  @slave_pressure:    slave pressure.
 *  @slave_als:         slave als.
 *  @slv_reg: slave register data structure.
 *  @sec_set: slave register odr config.
 *  @eng_info: information for each engine.
 *  @hw:		Other hardware-specific information.
 *  @chip_type:		chip type.
 *  @suspend_resume_lock: mutex lock for suspend/resume.
 *  @client:		i2c client handle.
 *  @plat_data:		platform data.
 *  @sl_handle:         Handle to I2C port.
 *  @sensor{SENSOR_NUM_MAX]: sensor individual properties.
 *  @sensor_accuracy[SENSOR_ACCURACY_NUM_MAX]: sensor accuracy.
 *  @sensor_acurracy_flag: flag indiciate whether to check output accuracy.
 *  @irq:               irq number store.
 *  @accel_bias:        accel bias store.
 *  @gyro_bias:         gyro bias store.
 *  @input_accel_dmp_bias[3]: accel bias for dmp.
 *  @input_gyro_dmp_bias[3]: gyro bias for dmp.
 *  @input_compass_dmp_bias[3]: compass bias for dmp.
 *  @fifo_data[8]: fifo data storage.
 *  @i2c_addr:          i2c address.
 *  @header_count:      header count in current FIFO.
 *  @gyro_sf: gyro scale factor.
 *  @left_over[LEFT_OVER_BYTES]: left over bytes storage.
 *  @left_over_size: left over size.
 *  @fifo_count: current fifo_count;
 *  @accel_cal_enable:  accel calibration on/off
 *  @gyro_cal_enable:   gyro calibration on/off
 *  @debug_determine_engine_on: determine engine on/off.
 *  @compass_cal_enable: compass calibration on/off.
 *  @suspend_state:     state indicator suspend.
 *  @step_detector_base_ts: base time stamp for step detector calculation.
 *  @last_run_time: last time the post ISR runs.
 *  @last_isr_time: last time the ISR runs.
 *  @last_temp_comp_time: last time temperature compensation is done.
 *  @ts_for_calib: ts storage for calibration.
 *  @dmp_ticks: dmp ticks storage for calibration.
 *  @start_dmp_counter: dmp counter when start a new session.
 *  @i2c_dis: disable I2C interface or not.
 *  @name: name for the chip.
 *  @gyro_st_data: gyro self test data.
 *  @accel_st_data: accel self test data.
 *  @secondary_name: name for the slave device in the secondary I2C.
 *  @current_compass_matrix: matrix compass data multiplied to before soft iron.
 *  @final_compass_matrix: matrix compass data multiplied to before soft iron.
 *  @trigger_state: information that which part triggers set_inv_enable.
 *  @firmware: firmware data pointer.
 *  @accel_calib_threshold: accel calibration threshold;
 *  @accel_calib_rate: divider for accel calibration rate.
 *  @accel_covariance[COVARIANCE_SIZE]: accel covariance data;
 *  @compass_covariance[COVARIANCE_SIZE]: compass covariance data;
 *  @curr_compass_covariance[COVARIANCE_SIZE]: current compass covariance data;
 *  @ref_mag_3d: refence compass value for compass calibration.
 *  @kf: kfifo for activity store.
 *  @activity_size: size for activity.
 */
#define QUEUE_WORK

struct inv_mpu_state {
	struct device *dev;
	struct inv_chip_config_s chip_config;
	struct inv_chip_info_s chip_info;
	struct iio_trigger  *trig;
	struct inv_smd smd;
	struct inv_ped ped;
	struct inv_batch batch;
	struct inv_temp_comp temp_comp;
	struct inv_mpu_slave *slave_compass;
	struct inv_mpu_slave *slave_pressure;
	struct inv_mpu_slave *slave_als;
	struct inv_secondary_reg slv_reg[4];
	struct inv_secondary_set sec_set;
	struct inv_engine_info eng_info[ENGINE_NUM_MAX];
	const struct inv_hw_s *hw;
	enum   inv_devices chip_type;
	struct mutex suspend_resume_lock;
	struct i2c_client *client;
	struct mpu_platform_data plat_data;
	void *sl_handle;
	struct inv_sensor sensor[SENSOR_NUM_MAX];
	struct inv_sensor sensor_accuracy[SENSOR_ACCURACY_NUM_MAX];
	bool sensor_acurracy_flag[SENSOR_ACCURACY_NUM_MAX];
	short irq;
	int accel_bias[3];
	int gyro_bias[3];
	int input_accel_dmp_bias[3];
	int input_gyro_dmp_bias[3];
	int input_compass_dmp_bias[3];
	u8 fifo_data[8];
	u8 i2c_addr;
	u8 header_count;
	s32 gyro_sf;
	u8 left_over[LEFT_OVER_BYTES];
	u32 left_over_size;
	u32 fifo_count;
	bool accel_cal_enable;
	bool gyro_cal_enable;
	bool compass_cal_enable;
	bool debug_determine_engine_on;
	bool suspend_state;
	u64 step_detector_base_ts;
	u64 last_run_time;
	u64 last_isr_time;
	u64 last_temp_comp_time;
	u64 ts_for_calib;
	u32 dmp_ticks;
	u32 start_dmp_counter;
	u8 i2c_dis;
	u8 name[20];
	u8 gyro_st_data[3];
	u8 accel_st_data[3];
	u8 secondary_name[20];
	int current_compass_matrix[9];
	int final_compass_matrix[9];
	enum TRIGGER_STATE trigger_state;
	u8 *firmware;
	int accel_calib_threshold;
	int accel_calib_rate;
	u32 accel_covariance[COVARIANCE_SIZE];
	u32 compass_covariance[COVARIANCE_SIZE];
	u32 curr_compass_covariance[COVARIANCE_SIZE];
	u32 ref_mag_3d;
	DECLARE_KFIFO(kf, u8, 128);

#ifdef QUEUE_WORK
	struct work_struct fifo_work;
	struct workqueue_struct *fifo_queue;
#endif

#ifdef TURN_OFF_SENSOR_WHEN_BACKLIGHT_IS_0
	char sensor_need_on;
#endif

#ifdef	CONFIG_HAS_EARLYSUSPEND
	struct early_suspend sensor_early_suspend;
#endif

	struct mutex inv_rw_mutex;
};

/**
 *  struct inv_mpu_slave - MPU slave structure.
 *  @st_upper:  compass self test upper limit.
 *  @st_lower:  compass self test lower limit.
 *  @scale: compass scale.
 *  @rate_scale: decide how fast a compass can read.
 *  @min_read_time: minimum time between each reading.
 *  @self_test: self test method of the slave.
 *  @set_scale: set scale of slave
 *  @get_scale: read scale back of the slave.
 *  @suspend:		suspend operation.
 *  @resume:		resume operation.
 *  @setup:		setup chip. initialization.
 *  @combine_data:	combine raw data into meaningful data.
 *  @read_data:        read external sensor and output
 *  @get_mode:		get current chip mode.
 *  @set_lpf:            set low pass filter.
 *  @set_fs:             set full scale
 *  @prev_ts: last time it is read.
 */
struct inv_mpu_slave {
	const short *st_upper;
	const short *st_lower;
	int scale;
	int rate_scale;
	int min_read_time;
	int (*self_test)(struct inv_mpu_state *);
	int (*set_scale)(struct inv_mpu_state *, int scale);
	int (*get_scale)(struct inv_mpu_state *, int *val);
	int (*suspend)(struct inv_mpu_state *);
	int (*resume)(struct inv_mpu_state *);
	int (*setup)(struct inv_mpu_state *);
	int (*combine_data)(u8 *in, short *out);
	int (*read_data)(struct inv_mpu_state *, short *out);
	int (*get_mode)(void);
	int (*set_lpf)(struct inv_mpu_state *, int rate);
	int (*set_fs)(struct inv_mpu_state *, int fs);
	u64 prev_ts;
};

/* scan element definition */
enum inv_mpu_scan {
	INV_MPU_SCAN_TIMESTAMP,
};

/* IIO attribute address */
enum MPU_IIO_ATTR_ADDR {
	ATTR_DMP_GYRO_X_DMP_BIAS,
	ATTR_DMP_GYRO_Y_DMP_BIAS,
	ATTR_DMP_GYRO_Z_DMP_BIAS,
	ATTR_DMP_GYRO_CAL_ENABLE,
	ATTR_DMP_ACCEL_X_DMP_BIAS,
	ATTR_DMP_ACCEL_Y_DMP_BIAS,
	ATTR_DMP_ACCEL_Z_DMP_BIAS,
	ATTR_DMP_MAGN_X_DMP_BIAS,
	ATTR_DMP_MAGN_Y_DMP_BIAS,
	ATTR_DMP_MAGN_Z_DMP_BIAS,
	ATTR_DMP_COMPASS_CAL_ENABLE,
	ATTR_DMP_ACCEL_CAL_ENABLE,
	ATTR_DMP_PED_INT_ON,
	ATTR_DMP_PED_STEP_THRESH,
	ATTR_DMP_PED_INT_THRESH,
	ATTR_DMP_PED_ON,
	ATTR_DMP_SMD_ENABLE,
	ATTR_DMP_SMD_THLD,
	ATTR_DMP_SMD_DELAY_THLD,
	ATTR_DMP_SMD_DELAY_THLD2,
	ATTR_DMP_PEDOMETER_STEPS,
	ATTR_DMP_PEDOMETER_TIME,
	ATTR_DMP_PEDOMETER_COUNTER,
	ATTR_DMP_LOW_POWER_GYRO_ON,
	ATTR_DMP_LP_EN_OFF,
	ATTR_DMP_CYCLE_MODE_OFF,
	ATTR_DMP_CLK_SEL,
	ATTR_DMP_DEBUG_MEM_READ,
	ATTR_DMP_DEBUG_MEM_WRITE,
	ATTR_DEBUG_REG_WRITE,
	ATTR_DEBUG_WRITE_CFG,
	ATTR_DEBUG_REG_ADDR,
	ATTR_DEBUG_DATA_COLLECTION_MODE,
	ATTR_DEBUG_DATA_COLLECTION_GYRO_RATE,
	ATTR_DEBUG_DATA_COLLECTION_ACCEL_RATE,
/* *****above this line, are DMP features, power needs on/off */
/* *****below this line, are DMP features, no power needed */
	ATTR_DMP_ON,
	ATTR_DMP_EVENT_INT_ON,
	ATTR_DMP_STEP_INDICATOR_ON,
	ATTR_DMP_BATCHMODE_TIMEOUT,
	ATTR_DMP_BATCHMODE_WAKE_FIFO_FULL,
	ATTR_DMP_STEP_DETECTOR_ON,
	ATTR_DMP_ACTIVITY_ON,
	ATTR_DMP_IN_ANGLVEL_ACCURACY_ENABLE,
	ATTR_DMP_IN_ACCEL_ACCURACY_ENABLE,
	ATTR_DMP_IN_MAGN_ACCURACY_ENABLE,
	ATTR_DMP_DEBUG_DETERMINE_ENGINE_ON,
	ATTR_DMP_MISC_GYRO_RECALIBRATION,
	ATTR_DMP_MISC_ACCEL_RECALIBRATION,
	ATTR_DMP_MISC_COMPASS_RECALIBRATION,
	ATTR_DMP_REF_MAG_3D,
	ATTR_DMP_PARAMS_ACCEL_CALIBRATION_THRESHOLD,
	ATTR_DMP_PARAMS_ACCEL_CALIBRATION_RATE,
	ATTR_GYRO_SCALE,
	ATTR_ACCEL_SCALE,
	ATTR_COMPASS_SCALE,
	ATTR_GYRO_ENABLE,
	ATTR_ACCEL_ENABLE,
	ATTR_COMPASS_ENABLE,
	ATTR_FIRMWARE_LOADED,
	ATTR_ANGLVEL_X_CALIBBIAS,
	ATTR_ANGLVEL_Y_CALIBBIAS,
	ATTR_ANGLVEL_Z_CALIBBIAS,
	ATTR_ACCEL_X_CALIBBIAS,
	ATTR_ACCEL_Y_CALIBBIAS,
	ATTR_ACCEL_Z_CALIBBIAS,
	ATTR_GYRO_MATRIX,
	ATTR_ACCEL_MATRIX,
	ATTR_COMPASS_MATRIX,
	ATTR_SECONDARY_NAME,
	ATTR_GYRO_SF,
};

int inv_mpu_configure_ring(struct iio_dev *indio_dev);
int inv_mpu_probe_trigger(struct iio_dev *indio_dev);
void inv_mpu_unconfigure_ring(struct iio_dev *indio_dev);
void inv_mpu_remove_trigger(struct iio_dev *indio_dev);

int inv_get_pedometer_steps(struct inv_mpu_state *st, int *ped);
int inv_get_pedometer_time(struct inv_mpu_state *st, int *ped);
int inv_read_pedometer_counter(struct inv_mpu_state *st);

ssize_t inv_dmp_firmware_write(struct file *fp, struct kobject *kobj,
	struct bin_attribute *attr, char *buf, loff_t pos, size_t size);
ssize_t inv_dmp_firmware_read(struct file *filp,
				struct kobject *kobj,
				struct bin_attribute *bin_attr,
				char *buf, loff_t off, size_t count);
ssize_t inv_soft_iron_matrix_write(struct file *fp, struct kobject *kobj,
	struct bin_attribute *attr, char *buf, loff_t pos, size_t size);
ssize_t inv_compass_covariance_cur_read(struct file *filp,
				struct kobject *kobj,
				struct bin_attribute *bin_attr,
				char *buf, loff_t off, size_t count);
ssize_t inv_compass_covariance_read(struct file *filp,
				struct kobject *kobj,
				struct bin_attribute *bin_attr,
				char *buf, loff_t off, size_t count);
ssize_t inv_accel_covariance_read(struct file *filp,
				struct kobject *kobj,
				struct bin_attribute *bin_attr,
				char *buf, loff_t off, size_t count);
ssize_t inv_accel_covariance_write(struct file *fp, struct kobject *kobj,
	struct bin_attribute *attr, char *buf, loff_t pos, size_t size);
ssize_t inv_compass_covariance_write(struct file *fp, struct kobject *kobj,
	struct bin_attribute *attr, char *buf, loff_t pos, size_t size);
ssize_t inv_compass_covariance_cur_write(struct file *fp, struct kobject *kobj,
	struct bin_attribute *attr, char *buf, loff_t pos, size_t size);
ssize_t inv_activity_read(struct file *filp, struct kobject *kobj,
				struct bin_attribute *bin_attr,
				char *buf, loff_t off, size_t count);
ssize_t inv_load_dmp_bias(struct inv_mpu_state *st, int *out);
ssize_t inv_set_dmp_bias(struct inv_mpu_state *st, int *data);

int set_inv_enable(struct iio_dev *indio_dev);

int inv_mpu_setup_compass_slave(struct inv_mpu_state *st);
int inv_mpu_setup_pressure_slave(struct inv_mpu_state *st);
int inv_mpu_setup_als_slave(struct inv_mpu_state *st);

void inv_init_sensor_struct(struct inv_mpu_state *st);
s64 get_time_ns(void);
int inv_i2c_read_base(struct inv_mpu_state *st, u16 i, u8 r, u16 l, u8 *d);
int inv_i2c_single_write_base(struct inv_mpu_state *st, u16 i, u8 r, u8 d);
int write_be32_to_mem(struct inv_mpu_state *st, u32 data, int addr);
int read_be32_from_mem(struct inv_mpu_state *st, u32 *o, int addr);
u32 inv_get_cntr_diff(u32 curr_counter, u32 prev);
int inv_write_2bytes(struct inv_mpu_state *st, int k, int data);
int inv_set_bank(struct inv_mpu_state *st, u8 bank);
int inv_set_power(struct inv_mpu_state *st, bool power_on);
int inv_switch_power_in_lp(struct inv_mpu_state *st, bool on);
int inv_turn_off_cycle_mode(struct inv_mpu_state *st, bool on);
int inv_stop_dmp(struct inv_mpu_state *st);
int inv_reset_fifo(struct iio_dev *indio_dev, bool turn_off);
int inv_create_dmp_sysfs(struct iio_dev *ind);
int inv_check_chip_type(struct iio_dev *indio_dev, const char *name);

int inv_flush_batch_data(struct iio_dev *indio_dev, bool *has_data);
int mpu_memory_write(struct inv_mpu_state *st, u8 mpu_addr, u16 mem_addr,
						u32 len, u8 const *data);
int mpu_memory_read(struct inv_mpu_state *st, u8 mpu_addr, u16 mem_addr,
							u32 len, u8 *data);
int inv_read_secondary(struct inv_mpu_state *st, int ind, int addr,
							int reg, int len);
int inv_write_secondary(struct inv_mpu_state *st, int ind, int addr,
							int reg, int v);
int inv_execute_write_secondary(struct inv_mpu_state *st, int ind, int addr,
							int reg, int v);
int inv_execute_read_secondary(struct inv_mpu_state *st, int ind, int addr,
						int reg, int len, u8 *d);

int inv_write_cntl(struct inv_mpu_state *st, u16 wd, bool en, int cntl);
/* used to print i2c data using pr_debug */
char *wr_pr_debug_begin(u8 const *data, u32 len, char *string);
char *wr_pr_debug_end(char *string);

int inv_hw_self_test(struct inv_mpu_state *st);
int inv_q30_mult(int a, int b);
int inv_plat_single_write(struct inv_mpu_state *st, u8 reg, u8 data);
int inv_plat_read(struct inv_mpu_state *st, u8 reg, int len, u8 *data);

#define mem_w(a, b, c) \
	mpu_memory_write(st, st->i2c_addr, a, b, c)
#define mem_r(a, b, c) \
	mpu_memory_read(st, st->i2c_addr, a, b, c)

#endif  /* #ifndef _INV_MPU_IIO_H_ */

