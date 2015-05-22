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
 * 09 10 2010 jackie.su
 * [ALPS00002279] [Need Patch] [Volunteer Patch] ALPS.Wxx.xx Volunteer patch for
 * .10y dual sensor
 *
 * 09 02 2010 jackie.su
 * [ALPS00002279] [Need Patch] [Volunteer Patch] ALPS.Wxx.xx Volunteer patch for
 * .roll back dual sensor
 *
 * Mar 4 2010 mtk70508
 * [DUMA00154792] Sensor driver
 * 
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


typedef enum {
    SENSOR_MODE_INIT = 0,
    SENSOR_MODE_PREVIEW,
    SENSOR_MODE_CAPTURE,
    SENSOR_MODE_ZSD
} OV2659_SENSOR_MODE;

typedef enum _OV2659_OP_TYPE_ {
        OV2659_MODE_NONE,
        OV2659_MODE_PREVIEW,
        OV2659_MODE_CAPTURE,
        OV2659_MODE_QCIF_VIDEO,
        OV2659_MODE_CIF_VIDEO,
        OV2659_MODE_QVGA_VIDEO
    } OV2659_OP_TYPE;

extern OV2659_OP_TYPE OV2659_g_iOV2659_Mode;

#define OV2659_MAX_GAIN							(0x38)
#define OV2659_MAX_SHUTTER_PREVIEW						(612)
#define OV2659_MAX_SHUTTER_CAPTIRE						(1228)

#define OV2659_MIN_GAIN							(1)
#define OV2659_MIN_SHUTTER						(1)

#define OV2659_ID_REG                          (0x300A)
#define OV2659_INFO_REG                        (0x300B)
 
#define OV2659_IMAGE_SENSOR_SVGA_WIDTH          (800)
#define OV2659_IMAGE_SENSOR_SVGA_HEIGHT         (600)
#define OV2659_IMAGE_SENSOR_UVGA_WITDH        (1600) 
#define OV2659_IMAGE_SENSOR_UVGA_HEIGHT       (1200)

#define OV2659_PV_PERIOD_PIXEL_NUMS    		(1300)  		
#define OV2659_PV_PERIOD_LINE_NUMS     		(616)   	
#define OV2659_FULL_PERIOD_PIXEL_NUMS  		(1951)  	
#define OV2659_FULL_PERIOD_LINE_NUMS   		(1232)  	

#define OV2659_PV_EXPOSURE_LIMITATION      	(616-4)
#define OV2659_FULL_EXPOSURE_LIMITATION    	(1232-4)

#define OV2659_PV_GRAB_START_X 				(5)
#define OV2659_PV_GRAB_START_Y  			(5)
#define OV2659_FULL_GRAB_START_X   			(5)
#define OV2659_FULL_GRAB_START_Y	  		(5)

#define OV2659_WRITE_ID							    0x60
#define OV2659_READ_ID								0x61

UINT32 OV2659Open(void);
UINT32 OV2659GetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution);
UINT32 OV2659GetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_INFO_STRUCT *pSensorInfo, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 OV2659Control(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 OV2659FeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId, UINT8 *pFeaturePara,UINT32 *pFeatureParaLen);
UINT32 OV2659Close(void);
UINT32 OV2659_YUV_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc);

#endif
