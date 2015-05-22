/*******************************************************************************************/

 
/*******************************************************************************************/

/* SENSOR FULL SIZE */
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
    SENSOR_MODE_ZSD_PREVIEW,
    SENSOR_MODE_CAPTURE,
} S5K3H7Y_SENSOR_MODE;

typedef struct
{	
	kal_uint32 pvPclk;  
	kal_uint32 capPclk; 
	kal_uint32 m_vidPclk;
	kal_int16 imgMirror;
	S5K3H7Y_SENSOR_MODE sensorMode;
	kal_bool S5K3H7YAutoFlickerMode;	
}S5K3H7Y_PARA_STRUCT,*PS5K3H7Y_PARA_STRUCT;

	//*************** +Sensor Framelength & Linelength ***************//	
	//Preview
	#define S5K3H7Y_PV_PERIOD_PIXEL_NUMS					(3688)
	#define S5K3H7Y_PV_PERIOD_LINE_NUMS 					(2530)
	
	//Video
	#define S5K3H7Y_VIDEO_PERIOD_PIXEL_NUMS					(S5K3H7Y_PV_PERIOD_PIXEL_NUMS)
	#define S5K3H7Y_VIDEO_PERIOD_LINE_NUMS					(2530)
	
	//Capture
	#define S5K3H7Y_FULL_PERIOD_PIXEL_NUMS					(S5K3H7Y_PV_PERIOD_PIXEL_NUMS)
	#define S5K3H7Y_FULL_PERIOD_LINE_NUMS					(2530)

	//*************** -Sensor Framelength & Linelength ***************//	

	//*************** +Sensor Output Size ***************//	
	#define S5K3H7Y_IMAGE_SENSOR_PV_WIDTH					(1600)
	#define S5K3H7Y_IMAGE_SENSOR_PV_HEIGHT					(1200)

	#define S5K3H7Y_IMAGE_SENSOR_VIDEO_WIDTH_SETTING 		(3264)
	#define S5K3H7Y_IMAGE_SENSOR_VIDEO_HEIGHT_SETTING		(1836)
	#define S5K3H7Y_IMAGE_SENSOR_VIDEO_WIDTH 				(S5K3H7Y_IMAGE_SENSOR_VIDEO_WIDTH_SETTING-64) //(2176-64)
	#define S5K3H7Y_IMAGE_SENSOR_VIDEO_HEIGHT 				(S5K3H7Y_IMAGE_SENSOR_VIDEO_HEIGHT_SETTING-36) //(1224-36)	
	#define S5K3H7Y_IMAGE_SENSOR_FULL_WIDTH 				(3264-64)	
	#define S5K3H7Y_IMAGE_SENSOR_FULL_HEIGHT				(2448-48)

	/* SENSOR START/EDE POSITION */         	
	#define S5K3H7Y_FULL_X_START						    		(2)
	#define S5K3H7Y_FULL_Y_START						    		(2+1)
	#define S5K3H7Y_ZSD_X_START								S5K3H7Y_FULL_X_START
	#define S5K3H7Y_ZSD_Y_START								S5K3H7Y_FULL_Y_START		
	#define S5K3H7Y_PV_X_START								(2)
	#define S5K3H7Y_PV_Y_START								(2+1)	
	#define S5K3H7Y_VIDEO_X_START								(2)
	#define S5K3H7Y_VIDEO_Y_START								(2+1)	


	#define S5K3H7YMIPI_WRITE_ID 	(0x20)
	#define S5K3H7YMIPI_READ_ID	    (0x21)

	#define S5K3H7YMIPI_WRITE_ID2 	(0x5A)
	#define S5K3H7YMIPI_READ_ID2	(0x5B)

	#define S5K3H7YMIPI_SENSOR_ID            S5K3H7Y_SENSOR_ID

//export functions
UINT32 S5K3H7YMIPIOpen(void);
UINT32 S5K3H7YMIPIGetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution);
UINT32 S5K3H7YMIPIGetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_INFO_STRUCT *pSensorInfo, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 S5K3H7YMIPIControl(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 S5K3H7YMIPIFeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId, UINT8 *pFeaturePara,UINT32 *pFeatureParaLen);
UINT32 S5K3H7YMIPIClose(void);


#endif /* __SENSOR_H */

