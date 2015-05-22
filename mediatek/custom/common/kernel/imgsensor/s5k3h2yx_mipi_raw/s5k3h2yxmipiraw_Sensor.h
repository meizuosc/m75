/*****************************************************************************
 *
 * Filename:
 * ---------
 *   ov5642_Sensor.h
 *
 * Project:
 * --------
 *   YUSU
 *
 * Description:
 * ------------
 *   Header file of Sensor driver
 *
 *
 * Author:
 * -------
 *   Jackie Su (MTK02380)
 *
 *============================================================================
 *             HISTORY
 * Below this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *------------------------------------------------------------------------------
 * $Revision:$
 * $Modtime:$
 * $Log:$
 *
 * 04 12 2013 guoqing.liu
 * [ALPS00564761] sensor driver check in
 * sensor driver check in.
 *
 * 09 11 2012 chengxue.shen
 * NULL
 * .
 *
 * 08 15 2012 chengxue.shen
 * NULL
 * .
 *
 * 07 31 2012 chengxue.shen
 * NULL
 * 3h2+lens.
 *
 * 05 17 2011 koli.lin
 * [ALPS00048194] [Need Patch] [Volunteer Patch]
 * [Camera]. Chagne the preview size to 1600x1200 for S5K3H2YX sensor.
 *
 * 04 01 2011 koli.lin
 * [ALPS00037670] [MPEG4 recording]the frame rate of fine quality video can not reach 30fps
 * [Camera]Modify the sensor preview output resolution and line time to fix frame rate at 30fps for video mode.
 *
 * 02 11 2011 koli.lin
 * [ALPS00030473] [Camera]
 * Change sensor driver preview size ratio to 4:3.
 *
 * 02 11 2011 koli.lin
 * [ALPS00030473] [Camera]
 * Modify the S5K3H2YX sensor driver for preview mode.
 *
 * 02 11 2011 koli.lin
 * [ALPS00030473] [Camera]
 * Create S5K3H2YX sensor driver to database.
 *
 * 08 19 2010 ronnie.lai
 * [DUMA00032601] [Camera][ISP]
 * Merge dual camera relative settings. Main OV5642, SUB O7675 ready.
 *
 * 08 18 2010 ronnie.lai
 * [DUMA00032601] [Camera][ISP]
 * Mmodify ISP setting and add OV5642 sensor driver.
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/
/* SENSOR FULL SIZE */
#ifndef __SENSOR_H
#define __SENSOR_H

//************************Jun add*************************//
//************************Jun add*************************//
#define Sleep(us) udelay(us)

#define RETAILMSG(x,...)
#define TEXT

#define S5K3H2YXMIPI_SENSOR_ID            S5K3H2YX_SENSOR_ID

#define S5K3H2YXMIPI_WRITE_ID (0x6E)
#define S5K3H2YXMIPI_READ_ID	(0x6F)

#define Using_linestart

#if 1
#define S5K3H2YXMIPI_PV_GRAB_X 						(2)
#define S5K3H2YXMIPI_PV_GRAB_Y					    (3)
#define S5K3H2YXMIPI_FULL_GRAB_X						(2)
#define S5K3H2YXMIPI_FULL_GRAB_Y						(3)

#define S5K3H2YXMIPI_PV_ACTIVE_PIXEL_NUMS 						(1640-40)
#define S5K3H2YXMIPI_PV_ACTIVE_LINE_NUMS							(1232-32)
#define S5K3H2YXMIPI_FULL_ACTIVE_PIXEL_NUMS						(3280-80)
#define S5K3H2YXMIPI_FULL_ACTIVE_LINE_NUMS						(2464-64)

#else
#define S5K3H2YXMIPI_PV_GRAB_X 						(8)
#define S5K3H2YXMIPI_PV_GRAB_Y					    (2)
#define S5K3H2YXMIPI_FULL_GRAB_X						(20)//(16)
#define S5K3H2YXMIPI_FULL_GRAB_Y						(18)//(4)

#define S5K3H2YXMIPI_PV_ACTIVE_PIXEL_NUMS 						(1640)
#define S5K3H2YXMIPI_PV_ACTIVE_LINE_NUMS							(1232)
#define S5K3H2YXMIPI_FULL_ACTIVE_PIXEL_NUMS						(3280)
#define S5K3H2YXMIPI_FULL_ACTIVE_LINE_NUMS						(2464)
#endif

#define S5K3H2YXMIPI_PV_LINE_LENGTH_PIXELS 						(3470)//(3536)
#define S5K3H2YXMIPI_PV_FRAME_LENGTH_LINES					 (1260)	//(1248)//(1244)//(1270)	
#define S5K3H2YXMIPI_FULL_LINE_LENGTH_PIXELS 					(3470)//(3536)
#define S5K3H2YXMIPI_FULL_FRAME_LENGTH_LINES			        (2480)//(2534)

#define S5K3H2YX_MIN_ANALOG_GAIN 1
#define S5K3H2YX_MAX_ANALOG_GAIN 16



#define FACTORY_START_ADDR 	0
#define ENGINEER_START_ADDR	10

typedef enum group_enum {
	   PRE_GAIN=0,
	   CMMCLK_CURRENT,
	   FRAME_RATE_LIMITATION,
	   REGISTER_EDITOR,
	   GROUP_TOTAL_NUMS
} FACTORY_GROUP_ENUM;

typedef enum register_index
{
	SENSOR_BASEGAIN=FACTORY_START_ADDR,
	PRE_GAIN_R_INDEX,
	PRE_GAIN_Gr_INDEX,
	PRE_GAIN_Gb_INDEX,
	PRE_GAIN_B_INDEX,
	FACTORY_END_ADDR
} FACTORY_REGISTER_INDEX;

typedef enum engineer_index
{
	CMMCLK_CURRENT_INDEX=ENGINEER_START_ADDR,
	ENGINEER_END
} FACTORY_ENGINEER_INDEX;

typedef struct
{
	SENSOR_REG_STRUCT	Reg[ENGINEER_END];
   	SENSOR_REG_STRUCT	CCT[FACTORY_END_ADDR];
} SENSOR_DATA_STRUCT, *PSENSOR_DATA_STRUCT;
typedef enum {
    SENSOR_MODE_INIT = 0,
    SENSOR_MODE_PREVIEW,
    SENSOR_MODE_VIDEO,
    SENSOR_MODE_CAPTURE,
    SENSOR_MODE_ZSD_PREVIEW,
    SENSOR_MODE_META
} S5K3H2YX_SENSOR_MODE;


struct S5K3H2YXMIPI_sensor_STRUCT
	{	 
		  kal_uint16 i2c_write_id;
		  kal_uint16 i2c_read_id;
		  kal_bool first_init;
		  kal_bool fix_video_fps;
		  S5K3H2YX_SENSOR_MODE sensor_mode; 				//True: Preview Mode; False: Capture Mode
		  kal_bool night_mode;				//True: Night Mode; False: Auto Mode
		  kal_uint8 mirror_flip;
		  kal_uint32 pv_pclk;				//Preview Pclk
		  kal_uint32 cp_pclk;				//Capture Pclk
		  kal_uint32 pv_shutter;	
		  kal_uint32 vd_shutter;
		  kal_uint32 cp_shutter;
		  kal_uint32 pv_gain;
		  kal_uint32 vd_gain;
		  kal_uint32 cp_gain;
		  kal_uint32 pv_line_length;
		  kal_uint32 pv_frame_length;
		  kal_uint32 cp_line_length;
		  kal_uint32 cp_frame_length;
		  kal_uint16 pv_dummy_pixels;		   //Dummy Pixels:must be 12s
		  kal_uint16 pv_dummy_lines;		   //Dummy Lines
		  kal_uint16 cp_dummy_pixels;		   //Dummy Pixels:must be 12s
		  kal_uint16 cp_dummy_lines;		   //Dummy Lines			
		  kal_uint16 video_current_frame_rate;
		  kal_uint16 ispBaseGain;
		  kal_uint16 sensorBaseGain;
		  kal_uint16 sensorGlobalGain;
		  kal_bool bAutoFlickerMode;
		  kal_bool IsVideoNightMode;
		  kal_bool VideoMode;
	};

//************************Jun add*************************//
//************************Jun add*************************//

//export functions
UINT32 S5K3H2YXMIPIOpen(void);
UINT32 S5K3H2YXMIPIGetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution);
UINT32 S5K3H2YXMIPIGetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_INFO_STRUCT *pSensorInfo, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 S5K3H2YXMIPIControl(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 S5K3H2YXMIPIFeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId, UINT8 *pFeaturePara,UINT32 *pFeatureParaLen);
UINT32 S5K3H2YXMIPIClose(void);



#endif /* __SENSOR_H */

