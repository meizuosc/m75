/*****************************************************************************
 *
 * Filename:
 * ---------
 *   sensor.h
 *
 * Project:
 * --------
 *   DUMA
 *
 * Description:
 * ------------
 *   CMOS sensor header file
 *
 ****************************************************************************/
#ifndef _OV9726MIPIRAW_SENSOR_H
#define _OV9726MIPIRAW_SENSOR_H

//#define OV9726MIPI_DEBUG
//#define OV9726MIPI_DRIVER_TRACE
//#define OV9726MIPI_TEST_PATTEM
#ifdef OV9726MIPI_DEBUG
//#define SENSORDB printk
#else
//#define SENSORDB(x,...)
#endif

#define OV9726MIPI_FACTORY_START_ADDR 0
#define OV9726MIPI_ENGINEER_START_ADDR 10
 
typedef enum OV9726MIPI_group_enum
{
  OV9726MIPI_PRE_GAIN = 0,
  OV9726MIPI_CMMCLK_CURRENT,
  OV9726MIPI_FRAME_RATE_LIMITATION,
  OV9726MIPI_REGISTER_EDITOR,
  OV9726MIPI_GROUP_TOTAL_NUMS
} OV9726MIPI_FACTORY_GROUP_ENUM;

typedef enum OV9726MIPI_register_index
{
  OV9726MIPI_SENSOR_BASEGAIN = OV9726MIPI_FACTORY_START_ADDR,
  OV9726MIPI_PRE_GAIN_R_INDEX,
  OV9726MIPI_PRE_GAIN_Gr_INDEX,
  OV9726MIPI_PRE_GAIN_Gb_INDEX,
  OV9726MIPI_PRE_GAIN_B_INDEX,
  OV9726MIPI_FACTORY_END_ADDR
} OV9726MIPI_FACTORY_REGISTER_INDEX;

typedef enum OV9726MIPI_engineer_index
{
  OV9726MIPI_CMMCLK_CURRENT_INDEX = OV9726MIPI_ENGINEER_START_ADDR,
  OV9726MIPI_ENGINEER_END
} OV9726MIPI_FACTORY_ENGINEER_INDEX;

typedef struct _sensor_data_struct
{
  SENSOR_REG_STRUCT reg[OV9726MIPI_ENGINEER_END];
  SENSOR_REG_STRUCT cct[OV9726MIPI_FACTORY_END_ADDR];
} sensor_data_struct;

/* SENSOR PREVIEW/CAPTURE VT CLOCK */
#define OV9726MIPI_PREVIEW_CLK                     42000000
#define OV9726MIPI_CAPTURE_CLK                     42000000

#define OV9726MIPI_COLOR_FORMAT                    SENSOR_OUTPUT_FORMAT_RAW_B//SENSOR_OUTPUT_FORMAT_RAW_R

#define OV9726_TEST_PATTERN_CHECKSUM     (0x5b0cfbc0)
#define OV9726MIPI_MIN_ANALOG_GAIN				1	/* 1x */
#define OV9726MIPI_MAX_ANALOG_GAIN				16	/* 32x */


/* FRAME RATE UNIT */
#define OV9726MIPI_FPS(x)                          (10 * (x))

/* SENSOR PIXEL/LINE NUMBERS IN ONE PERIOD */
//#define OV9726MIPI_FULL_PERIOD_PIXEL_NUMS          2700 /* 9 fps */
#define OV9726MIPI_FULL_PERIOD_PIXEL_NUMS          1280 /* 8 fps */
#define OV9726MIPI_FULL_PERIOD_LINE_NUMS           720
#define OV9726MIPI_PV_PERIOD_PIXEL_NUMS            1280 /* 30 fps */
#define OV9726MIPI_PV_PERIOD_LINE_NUMS             720

/* SENSOR START/END POSITION */
#define OV9726MIPI_FULL_X_START                    2
#define OV9726MIPI_FULL_Y_START                    2
#define OV9726MIPI_IMAGE_SENSOR_FULL_WIDTH         (1280) /* 2560 */
#define OV9726MIPI_IMAGE_SENSOR_FULL_HEIGHT        (720) /* 1920 */
#define OV9726MIPI_PV_X_START                      2
#define OV9726MIPI_PV_Y_START                      2
#define OV9726MIPI_IMAGE_SENSOR_PV_WIDTH           (1280) /* 1264 */
#define OV9726MIPI_IMAGE_SENSOR_PV_HEIGHT          (720) /* 948 */

#define OV9726MIPI_DEFAULT_DUMMY_PIXELS			(384)
#define OV9726MIPI_DEFAULT_DUMMY_LINES			(120)

/* SENSOR READ/WRITE ID */
#define OV9726MIPI_WRITE_ID (0x20)


/* SENSOR ID */
//#define OV9726MIPI_SENSOR_ID						(0x9726)

/* SENSOR PRIVATE STRUCT */
typedef struct OV9726MIPI_sensor_STRUCT
{
  MSDK_SENSOR_CONFIG_STRUCT cfg_data;
  sensor_data_struct eng; /* engineer mode */
  MSDK_SENSOR_ENG_INFO_STRUCT eng_info;
  kal_uint8 mirror;
  kal_bool pv_mode;
  kal_bool video_mode;
  kal_uint16 normal_fps; /* video normal mode max fps */
  kal_uint16 night_fps; /* video night mode max fps */
  kal_uint16 shutter;
  kal_uint16 gain;
  kal_uint32 pclk;
  kal_uint16 frame_height;
  kal_uint16 default_height;
  kal_uint16 line_length;  
} OV9726MIPI_sensor_struct;

//export functions
UINT32 OV9726MIPIOpen(void);
UINT32 OV9726MIPIControl(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 OV9726MIPIFeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId, UINT8 *pFeaturePara,UINT32 *pFeatureParaLen);
UINT32 OV9726MIPIGetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_INFO_STRUCT *pSensorInfo, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 OV9726MIPIGetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution);
UINT32 OV9726MIPIClose(void);

#define Sleep(ms) mdelay(ms)

#endif 
