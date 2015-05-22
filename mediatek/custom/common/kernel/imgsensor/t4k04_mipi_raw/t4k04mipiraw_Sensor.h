/*******************************************************************************************/
  

/*******************************************************************************************/

/* SENSOR FULL SIZE */
#ifndef __SENSOR_H_T4K04
#define __SENSOR_H_T4K04
  
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
} T4K04_SENSOR_MODE;


typedef struct
{
	kal_uint32 DummyPixels;
	kal_uint32 DummyLines;
	
	kal_uint32 pvShutter;
	kal_uint32 pvGain;
	
	kal_uint32 pvPclk;  // x10 480 for 48MHZ
	kal_uint32 capPclk; // x10
	kal_uint32 videoPclk;
	
	kal_uint32 shutter;
	kal_uint32 maxExposureLines;

	kal_uint16 sensorGlobalGain;//sensor gain read from 0x350a 0x350b;
	kal_uint16 ispBaseGain;//64
	kal_uint16 realGain;//ispBaseGain as 1x

	kal_int16 imgMirror;

	T4K04_SENSOR_MODE sensorMode;

	kal_bool T4K04AutoFlickerMode;
	kal_bool T4K04VideoMode;
	
}T4K04_PARA_STRUCT,*PT4K04_PARA_STRUCT;


    //grab windows
	#define T4K04_IMAGE_SENSOR_PV_WIDTH					1632   
	#define T4K04_IMAGE_SENSOR_PV_HEIGHT				1224  
	
	#define T4K04_IMAGE_SENSOR_VDO_WIDTH 				3264  
	#define T4K04_IMAGE_SENSOR_VDO_HEIGHT				1836
	
	#define T4K04_IMAGE_SENSOR_FULL_WIDTH				3264
	#define T4K04_IMAGE_SENSOR_FULL_HEIGHT				2448

    //total nums
	#define T4K04_PV_PERIOD_PIXEL_NUMS					0x0D54	//3412
	#define T4K04_PV_PERIOD_LINE_NUMS					0x0932	//2354	
	
	#define T4K04_VIDEO_PERIOD_PIXEL_NUMS				0x0D98	//3480
	#define T4K04_VIDEO_PERIOD_LINE_NUMS				0x09C0	//2496
	//#define T4K04_VIDEO_PERIOD_LINE_NUMS				0x0973	//

	
	
	#define T4K04_FULL_PERIOD_PIXEL_NUMS				0x0D98	//3480
	#define T4K04_FULL_PERIOD_LINE_NUMS 				0x09C0	//2496

	                                        	
	/* SENSOR START/EDE POSITION */         	
	#define T4K04_PV_X_START						    (2)
	#define T4K04_PV_Y_START						    (2)
	#define T4K04_VIDEO_X_START                         (2)
	#define T4K04_VIDEO_Y_START 						(2)
	#define T4K04_FULL_X_START							(2)
	#define T4K04_FULL_Y_START							(2)

	#define T4K04_MAX_ANALOG_GAIN						(10)
	#define T4K04_MIN_ANALOG_GAIN						(1)
	#define T4K04_ANALOG_GAIN_1X						(0x41)

	#define T4K04_PREVIEW_PCLK 							(242000000)
	#define T4K04_VIDEO_PCLK							(261000000)
	#define T4K04_CAPTURE_PCLK							(242000000)
	

	#define T4K04MIPI_WRITE_ID 	(0x6E)
	#define T4K04MIPI_READ_ID	(0x6F)


	#define T4K04MIPI_SENSOR_ID            T4K04_SENSOR_ID

	#define T4K04MIPI_PAGE_SETTING_REG    (0xFF)


//export functions
UINT32 T4K04MIPIOpen(void);
UINT32 T4K04MIPIGetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution);
UINT32 T4K04MIPIGetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_INFO_STRUCT *pSensorInfo, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 T4K04MIPIControl(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 T4K04MIPIFeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId, UINT8 *pFeaturePara,UINT32 *pFeatureParaLen);
UINT32 T4K04MIPIClose(void);


#endif /* __SENSOR_H_T4K04 */

