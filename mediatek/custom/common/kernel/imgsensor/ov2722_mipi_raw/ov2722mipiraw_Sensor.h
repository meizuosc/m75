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
#ifndef _OV2722MIPIRAW_SENSOR_H
#define _OV2722MIPIRAW_SENSOR_H
	 
	 //#define OV2722MIPI_DEBUG
	 //#define OV2722MIPI_DRIVER_TRACE
	 //#define OV2722MIPI_TEST_PATTEM
#ifdef OV2722MIPI_DEBUG
	 //#define SENSORDB printk
#else
	 //#define SENSORDB(x,...)
#endif
	 
#define OV2722MIPI_FACTORY_START_ADDR 0
#define OV2722MIPI_ENGINEER_START_ADDR 10
	  
	 typedef enum OV2722MIPI_group_enum
	 {
	   OV2722MIPI_PRE_GAIN = 0,
	   OV2722MIPI_CMMCLK_CURRENT,
	   OV2722MIPI_FRAME_RATE_LIMITATION,
	   OV2722MIPI_REGISTER_EDITOR,
	   OV2722MIPI_GROUP_TOTAL_NUMS
	 } OV2722MIPI_FACTORY_GROUP_ENUM;
	 
	 typedef enum OV2722MIPI_register_index
	 {
	   OV2722MIPI_SENSOR_BASEGAIN = OV2722MIPI_FACTORY_START_ADDR,
	   OV2722MIPI_PRE_GAIN_R_INDEX,
	   OV2722MIPI_PRE_GAIN_Gr_INDEX,
	   OV2722MIPI_PRE_GAIN_Gb_INDEX,
	   OV2722MIPI_PRE_GAIN_B_INDEX,
	   OV2722MIPI_FACTORY_END_ADDR
	 } OV2722MIPI_FACTORY_REGISTER_INDEX;
	 
	 typedef enum OV2722MIPI_engineer_index
	 {
	   OV2722MIPI_CMMCLK_CURRENT_INDEX = OV2722MIPI_ENGINEER_START_ADDR,
	   OV2722MIPI_ENGINEER_END
	 } OV2722MIPI_FACTORY_ENGINEER_INDEX;
	 
	 typedef struct _sensor_data_struct
	 {
	   SENSOR_REG_STRUCT reg[OV2722MIPI_ENGINEER_END];
	   SENSOR_REG_STRUCT cct[OV2722MIPI_FACTORY_END_ADDR];
	 } sensor_data_struct;
	 
	 /* SENSOR PREVIEW/CAPTURE VT CLOCK */
#define OV2722MIPI_PREVIEW_CLK                     72000000   //42000000
#define OV2722MIPI_CAPTURE_CLK                     72000000   //42000000
	 
#define OV2722MIPI_COLOR_FORMAT                    SENSOR_OUTPUT_FORMAT_RAW_B
	 
#define OV2722MIPI_MIN_ANALOG_GAIN				1	/* 1x */
#define OV2722MIPI_MAX_ANALOG_GAIN				16	/* 32x */
	 
	 
	 /* FRAME RATE UNIT */
#define OV2722MIPI_FPS(x)                          (10 * (x))
	 
	 /* SENSOR PIXEL/LINE NUMBERS IN ONE PERIOD */
	 //#define OV2722MIPI_FULL_PERIOD_PIXEL_NUMS		  2700 /* 9 fps */
#define OV2722MIPI_FULL_PERIOD_PIXEL_NUMS          1920 /* 8 fps */
#define OV2722MIPI_FULL_PERIOD_LINE_NUMS           1080
#define OV2722MIPI_PV_PERIOD_PIXEL_NUMS            1920 /* 30 fps */
#define OV2722MIPI_PV_PERIOD_LINE_NUMS             1080
	 
	 /* SENSOR START/END POSITION */
#define OV2722MIPI_FULL_X_START                    1
#define OV2722MIPI_FULL_Y_START                    1
#define OV2722MIPI_IMAGE_SENSOR_FULL_WIDTH         (1920)
#define OV2722MIPI_IMAGE_SENSOR_FULL_HEIGHT        (1080)
#define OV2722MIPI_PV_X_START                      1
#define OV2722MIPI_PV_Y_START                      1
#define OV2722MIPI_IMAGE_SENSOR_PV_WIDTH           (1920)
#define OV2722MIPI_IMAGE_SENSOR_PV_HEIGHT          (1080)
	 
#define OV2722MIPI_DEFAULT_DUMMY_PIXELS			(220)
#define OV2722MIPI_DEFAULT_DUMMY_LINES			(40)
	 
	 /* SENSOR READ/WRITE ID */
#define OV2722MIPI_WRITE_ID (0x6c)
#define OV2722MIPI_READ_ID  (0x6d)

	 
	 
	 /* SENSOR ID */
	 //#define OV2722MIPI_SENSOR_ID 					 (0x2720)
	 
	 /* SENSOR PRIVATE STRUCT */
	 typedef struct OV2722MIPI_sensor_STRUCT
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
	 } OV2722MIPI_sensor_struct;
	 
	 //export functions
	 UINT32 OV2722MIPIOpen(void);
	 UINT32 OV2722MIPIControl(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
	 UINT32 OV2722MIPIFeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId, UINT8 *pFeaturePara,UINT32 *pFeatureParaLen);
	 UINT32 OV2722MIPIGetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_INFO_STRUCT *pSensorInfo, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
	 UINT32 OV2722MIPIGetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution);
	 UINT32 OV2722MIPIClose(void);
	 
#define Sleep(ms) mdelay(ms)
	 
#endif 

