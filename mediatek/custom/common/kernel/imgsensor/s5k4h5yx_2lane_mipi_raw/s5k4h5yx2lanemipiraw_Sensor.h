#ifndef __SENSOR_H 
#define __SENSOR_H

typedef enum group_enum {
    PRE_GAIN=0,
    CMMCLK_CURRENT,
    FRAME_RATE_LIMITATION,
    REGISTER_EDITOR,
    GROUP_TOTAL_NUMS
} FACTORY_GROUP_ENUM; 

#define ENGINEER_START_ADDR 10
#define FACTORY_START_ADDR 0

typedef enum engineer_index
{
    CMMCLK_CURRENT_INDEX=ENGINEER_START_ADDR,
    ENGINEER_END
} FACTORY_ENGINEER_INDEX;

typedef enum register_index
{
	SENSOR_BASEGAIN=FACTORY_START_ADDR,
	PRE_GAIN_R_INDEX,
	PRE_GAIN_Gr_INDEX,
	PRE_GAIN_Gb_INDEX,
	PRE_GAIN_B_INDEX,
	FACTORY_END_ADDR
} FACTORY_REGISTER_INDEX;

typedef struct
{
    SENSOR_REG_STRUCT	Reg[ENGINEER_END];
    SENSOR_REG_STRUCT	CCT[FACTORY_END_ADDR];
} SENSOR_DATA_STRUCT, *PSENSOR_DATA_STRUCT;

typedef enum {
    SENSOR_MODE_INIT = 0,
    SENSOR_MODE_PREVIEW,
    SENSOR_MODE_VIDEO,
    SENSOR_MODE_CAPTURE
} S5K4H5YX_2LANE_SENSOR_MODE;

typedef struct
{
	kal_uint32 DummyPixels;
	kal_uint32 DummyLines;
	kal_uint32 shutter;
	kal_int16 imgMirror;
	S5K4H5YX_2LANE_SENSOR_MODE sensorMode;
	kal_bool S5K4H5YX2LANEAutoFlickerMode;
	kal_bool DynamicVideoSupport;
}S5K4H5YX_2LANE_PARA_STRUCT,*PS5K4H5YX_2LANE_PARA_STRUCT;

#define s5k4h5yx_2lane_master_clock								24   //mhz

#define s5k4h5yx_2lane_max_analog_gain							10
#define s5k4h5yx_2lane_min_analog_gain							1

#define s5k4h5yx_2lane_preview_frame_length   					1264 
#define s5k4h5yx_2lane_preview_line_length  					3688
#define s5k4h5yx_2lane_preview_pixelclock 					 	140000000

#define s5k4h5yx_2lane_capture_frame_length 					2531
#define s5k4h5yx_2lane_capture_line_length 						3688 
#define s5k4h5yx_2lane_capture_pixelclock 					 	140000000

#define s5k4h5yx_2lane_video_frame_length  						s5k4h5yx_2lane_preview_frame_length
#define s5k4h5yx_2lane_video_line_length   						s5k4h5yx_2lane_preview_line_length
#define s5k4h5yx_2lane_video_pixelclock 					 	s5k4h5yx_2lane_preview_pixelclock

#define s5k4h5yx_2lane_sensor_output_preview_width				1632
#define s5k4h5yx_2lane_sensor_output_preview_height				1232
#define s5k4h5yx_2lane_preview_width							s5k4h5yx_2lane_sensor_output_preview_width - 32
#define s5k4h5yx_2lane_preview_height							s5k4h5yx_2lane_sensor_output_preview_height - 32
#define s5k4h5yx_2lane_preview_startx							0
#define s5k4h5yx_2lane_preview_starty							0
#define s5k4h5yx_2lane_preview_max_framerate					300								

#define s5k4h5yx_2lane_sensor_output_capture_width				3264
#define s5k4h5yx_2lane_sensor_output_capture_height				2464
#define s5k4h5yx_2lane_capture_width							s5k4h5yx_2lane_sensor_output_capture_width - 64
#define s5k4h5yx_2lane_capture_height							s5k4h5yx_2lane_sensor_output_capture_height - 64
#define s5k4h5yx_2lane_capture_startx							0
#define s5k4h5yx_2lane_capture_starty							0
#define s5k4h5yx_2lane_capture_max_framerate					150

#define s5k4h5yx_2lane_sensor_output_video_width				s5k4h5yx_2lane_sensor_output_preview_width
#define s5k4h5yx_2lane_sensor_output_video_height				s5k4h5yx_2lane_sensor_output_preview_height
#define s5k4h5yx_2lane_video_width								s5k4h5yx_2lane_preview_width
#define s5k4h5yx_2lane_video_height								s5k4h5yx_2lane_preview_height
#define s5k4h5yx_2lane_video_startx								0
#define s5k4h5yx_2lane_video_starty								0
#define s5k4h5yx_2lane_video_max_framerate						300

#define S5K4H5YX_2LANE_MIPI_WRITE_ID 	(0x20)
#define S5K4H5YX_2LANE_MIPI_READ_ID		(0x21)

// SENSOR CHIP VERSION

#define S5K4H5YX_2LANEMIPI_SENSOR_ID            S5K4H5YX_2LANE_SENSOR_ID

#define S5K4H5YX_2LANEMIPI_PAGE_SETTING_REG    (0xFF)

struct S5K4H5YX_2LANE_MIPI_otp_struct
{
    kal_uint16 customer_id;
	kal_uint16 module_integrator_id;
	kal_uint16 lens_id;
	kal_uint16 rg_ratio;
	kal_uint16 bg_ratio;
	kal_uint16 user_data[5];
	kal_uint16 R_to_G;
	kal_uint16 B_to_G;
	kal_uint16 G_to_G;
	kal_uint16 R_Gain;
	kal_uint16 G_Gain;
	kal_uint16 B_Gain;
};

UINT32 S5K4H5YX_2LANEMIPIOpen(void);
UINT32 S5K4H5YX_2LANEMIPIGetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution);
UINT32 S5K4H5YX_2LANEMIPIGetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_INFO_STRUCT *pSensorInfo, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 S5K4H5YX_2LANEMIPIControl(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 S5K4H5YX_2LANEMIPIFeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId, UINT8 *pFeaturePara,UINT32 *pFeatureParaLen);
UINT32 S5K4H5YX_2LANEMIPIClose(void);

#endif
