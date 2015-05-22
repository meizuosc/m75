/*******************************************************************************************/


/*******************************************************************************************/

/* SENSOR FULL SIZE */
#ifndef __SENSOR_H
#define __SENSOR_H

//#define CAPTURE_USE_VIDEO_SETTING
#define FULL_SIZE_30_FPS
//#define USE_MIPI_2_LANES  //undefine this macro will use mipi 4 lanes

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
    SENSOR_MODE_VIDEO_NIGHT,
    SENSOR_MODE_SMALL_SIZE_END=SENSOR_MODE_VIDEO_NIGHT,
    SENSOR_MODE_ZSD_PREVIEW,
    SENSOR_MODE_CAPTURE,
    SENSOR_MODE_FULL_SIZE_END=SENSOR_MODE_CAPTURE
} S5K5E2YA_SENSOR_MODE;

typedef struct
{
	kal_uint32 DummyPixels;
	kal_uint32 DummyLines;
	
	kal_uint32 pvShutter;
	kal_uint32 pvGain;
	
	kal_uint32 pvPclk;  // x10 480 for 48MHZ
	kal_uint32 capPclk; // x10
	kal_uint32 m_vidPclk;
	
	kal_uint32 shutter;
	kal_uint32 maxExposureLines;
	kal_uint16 sensorGain; 
	kal_uint16 sensorBaseGain; 
	kal_int16 imgMirror;

	S5K5E2YA_SENSOR_MODE sensorMode;

	kal_bool S5K5E2YAAutoFlickerMode;
	kal_bool S5K5E2YAVideoMode;
	kal_uint32 FixedFrameLength;
	
}S5K5E2YA_PARA_STRUCT,*PS5K5E2YA_PARA_STRUCT;

	//*************** +Sensor Framelength & Linelength ***************//	
	//Preview
	#define S5K5E2YA_PV_PERIOD_PIXEL_NUMS					(2950) 	
	#define S5K5E2YA_PV_PERIOD_LINE_NUMS 					(2000) 
	
	//Video
	#define S5K5E2YA_VIDEO_PERIOD_PIXEL_NUMS				(2950) 	
	#define S5K5E2YA_VIDEO_PERIOD_LINE_NUMS					(2000) 
	
	//Capture
	#define S5K5E2YA_FULL_PERIOD_PIXEL_NUMS					(2950)	
	#define S5K5E2YA_FULL_PERIOD_LINE_NUMS					(2025)	
	
	//ZSD
	#define S5K5E2YA_ZSD_PERIOD_PIXEL_NUMS					S5K5E2YA_FULL_PERIOD_PIXEL_NUMS
	#define S5K5E2YA_ZSD_PERIOD_LINE_NUMS					S5K5E2YA_FULL_PERIOD_LINE_NUMS
	//*************** -Sensor Framelength & Linelength ***************//	

	//*************** +Sensor Output Size ***************//	
	#define S5K5E2YA_IMAGE_SENSOR_PV_WIDTH					(1280)
	#define S5K5E2YA_IMAGE_SENSOR_PV_HEIGHT					(960)

	#define S5K5E2YA_IMAGE_SENSOR_VIDEO_WIDTH       		(2560)
	#define S5K5E2YA_IMAGE_SENSOR_VIDEO_HEIGHT	        	(1440)
	
	
	#define S5K5E2YA_IMAGE_SENSOR_FULL_WIDTH 				(2560)	
	#define S5K5E2YA_IMAGE_SENSOR_FULL_HEIGHT				(1920)


	#define S5K5E2YA_IMAGE_SENSOR_ZSD_WIDTH				S5K5E2YA_IMAGE_SENSOR_FULL_WIDTH	
	#define S5K5E2YA_IMAGE_SENSOR_ZSD_HEIGHT				S5K5E2YA_IMAGE_SENSOR_FULL_HEIGHT
	//*************** -Sensor Output Size ***************//	   


	/* SENSOR START/EDE POSITION */         	
	#define S5K5E2YA_FULL_X_START						    		(0)
	#define S5K5E2YA_FULL_Y_START						    		(0)

	
	#define S5K5E2YA_PV_X_START								(0)
	#define S5K5E2YA_PV_Y_START								(0)	


	#define S5K5E2YA_VIDEO_X_START								(0)
	#define S5K5E2YA_VIDEO_Y_START								(0)	



	#define S5K5E2YA_MAX_ANALOG_GAIN					(16)
	#define S5K5E2YA_MIN_ANALOG_GAIN					(1)

	#define S5K5E2YA_MIN_LINE_LENGTH						(0x0E68)	//2724
	#define S5K5E2YA_MIN_FRAME_LENGTH					(0x0A0F)	//532
	
	#define S5K5E2YA_MAX_LINE_LENGTH						0xCCCC
	#define S5K5E2YA_MAX_FRAME_LENGTH						0xFFFF

	/* DUMMY NEEDS TO BE INSERTED */
	/* SETUP TIME NEED TO BE INSERTED */
	#define S5K5E2YA_IMAGE_SENSOR_PV_INSERTED_PIXELS			2	// Sync, Nosync=2
	#define S5K5E2YA_IMAGE_SENSOR_PV_INSERTED_LINES			2

	#define S5K5E2YA_IMAGE_SENSOR_FULL_INSERTED_PIXELS		4
	#define S5K5E2YA_IMAGE_SENSOR_FULL_INSERTED_LINES		4


#define S5K5E2YAMIPI_WRITE_ID 	(0x20)
#define S5K5E2YAMIPI_READ_ID	(0x21)

#define SENSOR_PCLK_PREVIEW  	(179200000) 
#define SENSOR_PCLK_VIDEO  		SENSOR_PCLK_PREVIEW 
#define SENSOR_PCLK_CAPTURE  	SENSOR_PCLK_PREVIEW 
#define SENSOR_PCLK_ZSD  		SENSOR_PCLK_CAPTURE


#define S5K5E2YAMIPI_SENSOR_ID            S5K5E2YA_SENSOR_ID


struct S5K5E2YA_MIPI_otp_struct
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



//export functions
UINT32 S5K5E2YAMIPIOpen(void);
UINT32 S5K5E2YAMIPIGetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution);
UINT32 S5K5E2YAMIPIGetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_INFO_STRUCT *pSensorInfo, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 S5K5E2YAMIPIControl(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 S5K5E2YAMIPIFeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId, UINT8 *pFeaturePara,UINT32 *pFeatureParaLen);
UINT32 S5K5E2YAMIPIClose(void);


#endif /* __SENSOR_H */

