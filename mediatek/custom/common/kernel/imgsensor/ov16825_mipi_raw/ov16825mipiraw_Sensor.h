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
#ifndef _OV16825_SENSOR_H
#define _OV16825_SENSOR_H

#define OV16825_DEBUG
#define OV16825_DRIVER_TRACE
//#define OV16825_TEST_PATTEM
#ifdef OV16825_DEBUG
//#define SENSORDB printk
#else
//#define SENSORDB(x,...)
#endif

//#define OV16825_2_LANE  // if you use 2 lane setting on MT6589, please define it
#define OV16825_FACTORY_START_ADDR 0
#define OV16825_ENGINEER_START_ADDR 10

//#define MIPI_INTERFACE

 
typedef enum OV16825_group_enum
{
  OV16825_PRE_GAIN = 0,
  OV16825_CMMCLK_CURRENT,
  OV16825_FRAME_RATE_LIMITATION,
  OV16825_REGISTER_EDITOR,
  OV16825_GROUP_TOTAL_NUMS
} OV16825_FACTORY_GROUP_ENUM;

typedef enum OV16825_register_index
{
  OV16825_SENSOR_BASEGAIN = OV16825_FACTORY_START_ADDR,
  OV16825_PRE_GAIN_R_INDEX,
  OV16825_PRE_GAIN_Gr_INDEX,
  OV16825_PRE_GAIN_Gb_INDEX,
  OV16825_PRE_GAIN_B_INDEX,
  OV16825_FACTORY_END_ADDR
} OV16825_FACTORY_REGISTER_INDEX;

typedef enum OV16825_engineer_index
{
  OV16825_CMMCLK_CURRENT_INDEX = OV16825_ENGINEER_START_ADDR,
  OV16825_ENGINEER_END
} OV16825_FACTORY_ENGINEER_INDEX;

typedef struct _sensor_data_struct
{
  SENSOR_REG_STRUCT reg[OV16825_ENGINEER_END];
  SENSOR_REG_STRUCT cct[OV16825_FACTORY_END_ADDR];
} sensor_data_struct;


#define OV16825_COLOR_FORMAT                    SENSOR_OUTPUT_FORMAT_RAW_B

#define OV16825_MIN_ANALOG_GAIN  1   /* 1x */
#define OV16825_MAX_ANALOG_GAIN      32 /* 32x */

 
/* FRAME RATE UNIT */
#define OV16825_FPS(x)                          (10 * (x))


#define OV16825_PREVIEW_CLK   80000000
#define OV16825_CAPTURE_CLK   80000000
#define OV16825_VIDEO_CLK     80000000
#define OV16825_ZSD_PRE_CLK   80000000

/* SENSOR PIXEL/LINE NUMBERS IN ONE PERIOD */
#define OV16825_FULL_PERIOD_PIXEL_NUMS          6112 //(6112+1528)  //  15fps
#define OV16825_FULL_PERIOD_LINE_NUMS           (3490+100)   //

#define OV16825_PV_PERIOD_PIXEL_NUMS            6080 //
#define OV16825_PV_PERIOD_LINE_NUMS             1754 //

#define OV16825_VIDEO_PERIOD_PIXEL_NUMS         6080  //
#define OV16825_VIDEO_PERIOD_LINE_NUMS          1754  //

#define OV16825_3D_FULL_PERIOD_PIXEL_NUMS       6112 /* 15 fps */
#define OV16825_3D_FULL_PERIOD_LINE_NUMS        3490
#define OV16825_3D_PV_PERIOD_PIXEL_NUMS         6080 /* 30 fps */
#define OV16825_3D_PV_PERIOD_LINE_NUMS          1754
#define OV16825_3D_VIDEO_PERIOD_PIXEL_NUMS      6080 /* 30 fps */
#define OV16825_3D_VIDEO_PERIOD_LINE_NUMS       1754
/* SENSOR START/END POSITION */
#define OV16825_FULL_X_START                    0//10
#define OV16825_FULL_Y_START                    0//10
#define OV16825_IMAGE_SENSOR_FULL_WIDTH         4608 //(4608 - 640) /* 2560 */
#define OV16825_IMAGE_SENSOR_FULL_HEIGHT        3456 //(3456 - 480) /* 1920 */

#define OV16825_PV_X_START                      0 // 2
#define OV16825_PV_Y_START                      0 // 2
#define OV16825_IMAGE_SENSOR_PV_WIDTH           2304 //(2304 - 320)
#define OV16825_IMAGE_SENSOR_PV_HEIGHT          1728 //(1728 - 240)

#define OV16825_VIDEO_X_START                   0 //9
#define OV16825_VIDEO_Y_START                   0 //11
#define OV16825_IMAGE_SENSOR_VIDEO_WIDTH        2304 //(2304 - 320) /* 1264 */
#define OV16825_IMAGE_SENSOR_VIDEO_HEIGHT       1728 //(1728 - 240) /* 948 */

#define OV16825_3D_FULL_X_START                 10   //(1+16+6)
#define OV16825_3D_FULL_Y_START                 10  //(1+12+4)
#define OV16825_IMAGE_SENSOR_3D_FULL_WIDTH      (4608 - 640) //(2592 - 16) /* 2560 */
#define OV16825_IMAGE_SENSOR_3D_FULL_HEIGHT     (3456 - 480) //(1944 - 12) /* 1920 */
#define OV16825_3D_PV_X_START                   2
#define OV16825_3D_PV_Y_START                   2
#define OV16825_IMAGE_SENSOR_3D_PV_WIDTH        (2304 - 320) /* 1600 */
#define OV16825_IMAGE_SENSOR_3D_PV_HEIGHT       (1728 - 240) /* 1200 */
#define OV16825_3D_VIDEO_X_START                2
#define OV16825_3D_VIDEO_Y_START                2
#define OV16825_IMAGE_SENSOR_3D_VIDEO_WIDTH     (2304 - 320) /* 1600 */
#define OV16825_IMAGE_SENSOR_3D_VIDEO_HEIGHT    (1728 - 240) /* 1200 */



/* SENSOR READ/WRITE ID */

#define OV16825_SLAVE_WRITE_ID_1   (0x6c)
#define OV16825_SLAVE_WRITE_ID_2   (0x20)
/************OTP Feature*********************/
//#define OV16825_USE_OTP
#define OV16825_USE_WB_OTP

#if defined(OV16825_USE_OTP)

#endif
/************OTP Feature*********************/

/* SENSOR PRIVATE STRUCT */
typedef struct OV16825_sensor_STRUCT
{
  MSDK_SENSOR_CONFIG_STRUCT cfg_data;
  sensor_data_struct eng; /* engineer mode */
  MSDK_SENSOR_ENG_INFO_STRUCT eng_info;
  kal_uint8 mirror;
  kal_bool pv_mode;
  kal_bool video_mode;  
  //kal_bool NightMode;
  kal_bool is_zsd;
  kal_bool is_zsd_cap;
  kal_bool is_autofliker;
  //kal_uint16 normal_fps; /* video normal mode max fps */
  //kal_uint16 night_fps; /* video night mode max fps */  
  kal_uint16 FixedFps;
  kal_uint16 shutter;
  kal_uint16 gain;
  kal_uint32 pv_pclk;
  kal_uint32 cap_pclk;
  kal_uint32 pclk;
  kal_uint16 frame_height;
  kal_uint16 line_length;  
  kal_uint16 write_id;
  kal_uint16 read_id;
  kal_uint16 dummy_pixels;
  kal_uint16 dummy_lines;
} OV16825_sensor_struct;

//export functions
UINT32 OV16825Open(void);
UINT32 OV16825Control(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 OV16825FeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId, UINT8 *pFeaturePara,UINT32 *pFeatureParaLen);
UINT32 OV16825GetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_INFO_STRUCT *pSensorInfo, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 OV16825GetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution);
UINT32 OV16825Close(void);

#define Sleep(ms) mdelay(ms)

#endif 
