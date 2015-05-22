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
 *   Header file of Sensor driver
 *
 *
 * Author:
 * -------
 *   PC Huang (MTK02204)
 *
 *============================================================================
 *             HISTORY
 * Below this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *------------------------------------------------------------------------------
 * $Revision:$
 * $Modtime:$
 * $Log:$
 *
 * 03 31 2010 jianhua.tang
 * [DUMA00158728]S5K8AAYX YUV sensor driver 
 * .
 *
 * Feb 24 2010 mtk01118
 * [DUMA00025869] [Camera][YUV I/F & Query feature] check in camera code
 * 
 *
 * Aug 5 2009 mtk01051
 * [DUMA00009217] [Camera Driver] CCAP First Check In
 * 
 *
 * Apr 7 2009 mtk02204
 * [DUMA00004012] [Camera] Restructure and rename camera related custom folders and folder name of came
 * 
 *
 * Mar 26 2009 mtk02204
 * [DUMA00003515] [PC_Lint] Remove PC_Lint check warnings of camera related drivers.
 * 
 *
 * Mar 2 2009 mtk02204
 * [DUMA00001084] First Check in of MT6516 multimedia drivers
 * 
 *
 * Feb 24 2009 mtk02204
 * [DUMA00001084] First Check in of MT6516 multimedia drivers
 * 
 *
 * Dec 27 2008 MTK01813
 * DUMA_MBJ CheckIn Files
 * created by clearfsimport
 *
 * Dec 10 2008 mtk02204
 * [DUMA00001084] First Check in of MT6516 multimedia drivers
 * 
 *
 * Oct 27 2008 mtk01051
 * [DUMA00000851] Camera related drivers check in
 * Modify Copyright Header
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/
/* SENSOR FULL SIZE */
#ifndef __SENSOR_H
#define __SENSOR_H


#define OV9760_FACTORY_START_ADDR 0
#define OV9760_ENGINEER_START_ADDR 10


typedef enum OV9760_group_enum
{
  OV9760_PRE_GAIN = 0,
  OV9760_CMMCLK_CURRENT,
  OV9760_FRAME_RATE_LIMITATION,
  OV9760_REGISTER_EDITOR,
  OV9760_GROUP_TOTAL_NUMS
} OV9760_FACTORY_GROUP_ENUM;

typedef enum OV9760_register_index
{
  OV9760_SENSOR_BASEGAIN = OV9760_FACTORY_START_ADDR,
  OV9760_PRE_GAIN_R_INDEX,
  OV9760_PRE_GAIN_Gr_INDEX,
  OV9760_PRE_GAIN_Gb_INDEX,
  OV9760_PRE_GAIN_B_INDEX,
  OV9760_FACTORY_END_ADDR
} OV9760_FACTORY_REGISTER_INDEX;

typedef enum OV9760_engineer_index
{
  OV9760_CMMCLK_CURRENT_INDEX = OV9760_ENGINEER_START_ADDR,
  OV9760_ENGINEER_END
} OV9760_FACTORY_ENGINEER_INDEX;

typedef struct _sensor_data_struct
{
  SENSOR_REG_STRUCT reg[OV9760_ENGINEER_END];
  SENSOR_REG_STRUCT cct[OV9760_FACTORY_END_ADDR];
} sensor_data_struct;


typedef struct OV9760_sensor_STRUCT
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
} OV9760_sensor_struct;

#define OV9760_MIN_ANALOG_GAIN				1	/* 1x */
#define OV9760_MAX_ANALOG_GAIN				32	/* 32x */



// Grab Window Setting for preview mode.
#define OV9760MIPI_IMAGE_SENSOR_VIDEO_WIDTH	 				1280
#define OV9760MIPI_IMAGE_SENSOR_VIDEO_HEIGHT					720
	
#define OV9760MIPI_IMAGE_SENSOR_FULL_WIDTH			1470
#define OV9760MIPI_IMAGE_SENSOR_FULL_HEIGHT			1100

#define OV9760MIPI_PERIOD_PIXEL_NUMS         1736   
#define OV9760MIPI_PERIOD_LINE_NUMS          1148 

#define OV9760MIPI_PIXEL_CLK 60000000

#define OV9760_TEST_PATTERN_CHECKSUM     (0x451979de)


/* DUMMY NEEDS TO BE INSERTED */
/* SETUP TIME NEED TO BE INSERTED */


/* SENSOR READ/WRITE ID */
#define OV9760MIPI_WRITE_ID		0x6c		
#define OV9760MIPI_WRITE_ID_1	0x20



//customize
#define CAM_SIZE_QVGA_WIDTH 	320
#define CAM_SIZE_QVGA_HEIGHT 	240
#define CAM_SIZE_VGA_WIDTH 		640
#define CAM_SIZE_VGA_HEIGHT 	480
#define CAM_SIZE_05M_WIDTH 		800
#define CAM_SIZE_05M_HEIGHT 	600
#define CAM_SIZE_1M_WIDTH 		1280
#define CAM_SIZE_1M_HEIGHT 		960
#define CAM_SIZE_2M_WIDTH 		1600
#define CAM_SIZE_2M_HEIGHT 		1200
#define CAM_SIZE_3M_WIDTH 		2048
#define CAM_SIZE_3M_HEIGHT 		1536
#define CAM_SIZE_5M_WIDTH 		2592
#define CAM_SIZE_5M_HEIGHT 		1944



//export functions
UINT32 OV9760MIPIOpen(void);
UINT32 OV9760MIPIControl(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 OV9760MIPIFeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId, UINT8 *pFeaturePara,UINT32 *pFeatureParaLen);
UINT32 OV9760MIPIGetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_INFO_STRUCT *pSensorInfo, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 OV9760MIPIGetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution);
UINT32 OV9760MIPIClose(void);

#define Sleep(ms) mdelay(ms)
#define RETAILMSG(x,...)
#define TEXT

#endif /* __SENSOR_H */
