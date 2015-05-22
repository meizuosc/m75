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
    SENSOR_MODE_CAPTURE
} MN34130_SENSOR_MODE;


typedef struct
{
	kal_uint32 DummyPixels;
	kal_uint32 DummyLines;
	
	kal_uint32 pvShutter;
	kal_uint32 pvGain;
	
	kal_uint32 pvPclk;  // x10 480 for 48MHZ
	kal_uint32 videoPclk;
	kal_uint32 capPclk; // x10
	
	kal_uint32 shutter;
	kal_uint32 maxExposureLines;

	kal_uint16 sensorGlobalGain;//sensor gain read from 0x350a 0x350b;
	kal_uint16 ispBaseGain;//64
	kal_uint16 realGain;//ispBaseGain as 1x

	kal_int16 imgMirror;

	MN34130_SENSOR_MODE sensorMode;

	kal_bool MN34130AutoFlickerMode;
	kal_bool MN34130VideoMode;
	
}MN34130_PARA_STRUCT,*PMN34130_PARA_STRUCT;




#define MN34130_IMAGE_SENSOR_FULL_WIDTH			       (4224) // Effective_H:4224, HTR1:8, HTR2:8
#define MN34130_IMAGE_SENSOR_FULL_HEIGHT		       (3168) // Effective_V:3168, Dummy_V1:2, Dummy_V2:2, Dummy_V3:0, VOB:36, VTR1:8, VTR2:8

/* SENSOR START POSITION */         	

#define MN34130_FULL_X_START						    (0) 
#define MN34130_FULL_Y_START						    (0) 



/* SENSOR PV SIZE */


#define MN34130_IMAGE_SENSOR_PV_WIDTH					(2112)
#define MN34130_IMAGE_SENSOR_PV_HEIGHT					(1584)




#define MN34130_PV_X_START							(0)
#define MN34130_PV_Y_START							(0)




#define MN34130_IMAGE_SENSOR_VIDEO_WIDTH				(2112)
#define MN34130_IMAGE_SENSOR_VIDEO_HEIGHT				(1584)



#define MN34130_VIDEO_X_START							(0)
#define MN34130_VIDEO_Y_START							(0)



#define MN34130_MAX_ANALOG_GAIN					            (250)
#define MN34130_MIN_ANALOG_GAIN					            (1)
#define MN34130_ANALOG_GAIN_1X					            (0x0100)

#define MN34130_MAX_DIGITAL_GAIN					        (8)
#define MN34130_MIN_DIGITAL_GAIN					        (1)
#define MN34130_DIGITAL_GAIN_1X					            (0x0100)

/* SENSOR PIXEL/LINE NUMBERS IN ONE PERIOD */
#define MN34130_FULL_PERIOD_PIXEL_NUMS				        (5208) 
#define MN34130_FULL_PERIOD_LINE_NUMS				        (3316) 


#define MN34130_PV_PERIOD_PIXEL_NUMS			    (2604)
#define MN34130_PV_PERIOD_LINE_NUMS				    (1658) //(1658) //(1632)



#define MN34130_VIDEO_PERIOD_PIXEL_NUMS 			    (2604)
#define MN34130_VIDEO_PERIOD_LINE_NUMS				    (1658) //(1658) //(1632)

	
#define MN34130_MIN_LINE_LENGTH						        (2484)
#define MN34130_MIN_FRAME_LENGTH					        (1739)

	
#define MN34130_MAX_LINE_LENGTH						        0xCCCC
#define MN34130_MAX_FRAME_LENGTH						    0xFFFF


/* SENSOR SCALER FACTOR */
#define MN34130_PV_SCALER_FACTOR					        1
#define MN34130_FULL_SCALER_FACTOR					        1


/* DUMMY NEEDS TO BE INSERTED */
/* SETUP TIME NEED TO BE INSERTED */
#define MN34130_IMAGE_SENSOR_PV_INSERTED_PIXELS		    0
#define MN34130_IMAGE_SENSOR_PV_INSERTED_LINES			4

#define MN34130_IMAGE_SENSOR_FULL_INSERTED_PIXELS		0
#define MN34130_IMAGE_SENSOR_FULL_INSERTED_LINES		4

/* I2C Slave ID */
#define MN34130MIPI_WRITE_ID 	(0x20)
#define MN34130MIPI_READ_ID	    (0x21)


// SENSOR CHIP VERSION
#define MN34130MIPI_SENSOR_ID            MN34130_SENSOR_ID

#define MN34130MIPI_PAGE_SETTING_REG    (0xFF)

//s_add for porting
//s_add for porting
//s_add for porting

//export functions
UINT32 MN34130MIPIOpen(void);
UINT32 MN34130MIPIGetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution);
UINT32 MN34130MIPIGetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_INFO_STRUCT *pSensorInfo, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 MN34130MIPIControl(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 MN34130MIPIFeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId, UINT8 *pFeaturePara,UINT32 *pFeatureParaLen);
UINT32 MN34130MIPIClose(void);

//#define Sleep(ms) mdelay(ms)
//#define RETAILMSG(x,...)
//#define TEXT

//e_add for porting
//e_add for porting
//e_add for porting

#endif /* __SENSOR_H */

