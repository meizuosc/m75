/*****************************************************************************
 *
 * Filename:
 * ---------
 *   CFG_file_info_custom.h
 *
 * Project:
 * --------
 *   YuSu
 *
 * Description:
 * ------------
 *   Configuration File List for Customer
 *
 *
 * Author:
 * -------
 *   Nick Huang (mtk02183)
 *
 ****************************************************************************/

#ifndef __CFG_FILE_INFO_CUSTOM_H__
#define __CFG_FILE_INFO_CUSTOM_H__

#include "CFG_file_public.h"
#include "CFG_file_lid.h"
#include "Custom_NvRam_LID.h"
#include "CFG_AUDIO_File.h"
#include "CFG_Audio_Default.h"
#include "CFG_GPS_File.h"
#include "CFG_GPS_Default.h"
#include "CFG_Wifi_File.h"
#include "CFG_WIFI_Default.h"
#include "CFG_PRODUCT_INFO_File.h"
#include "CFG_PRODUCT_INFO_Default.h"
#include "CFG_Rootsign_File.h"
#include "CFG_Rootsign_Default.h"
#include "CFG_Recoverylock_File.h"
#include "CFG_Recoverylock_Default.h"
#include "CFG_Calibration_File.h"
#include "CFG_Calibration_Default.h"
#include <stdio.h>

int Wifi_ConvertFunction(int, int, char*, char*);

#ifdef __cplusplus

extern "C"
{
#endif

    const TCFG_FILE g_akCFG_File_Custom[]=
    {
        {
            "/data/nvram/APCFG/APRDCL/Audio_Sph",       VER(AP_CFG_RDCL_FILE_AUDIO_LID),         CFG_FILE_SPEECH_REC_SIZE,
            CFG_FILE_SPEECH_REC_TOTAL,                   SIGNLE_DEFUALT_REC,                                   (char *)&speech_custom_default, DataReset , NULL
        },

        {
            "/data/nvram/APCFG/APRDEB/GPS",         VER(AP_CFG_CUSTOM_FILE_GPS_LID),	           CFG_FILE_GPS_CONFIG_SIZE,
            CFG_FILE_GPS_CONFIG_TOTAL,                  SIGNLE_DEFUALT_REC,                (char *)&stGPSConfigDefault, DataReset , NULL
        },

        {
            "/data/nvram/APCFG/APRDCL/Audio_CompFlt",       VER(AP_CFG_RDCL_FILE_AUDIO_COMPFLT_LID),         CFG_FILE_AUDIO_COMPFLT_REC_SIZE,
            CFG_FILE_AUDIO_COMPFLT_REC_TOTAL,                   SIGNLE_DEFUALT_REC,                (char *)&audio_custom_default, DataReset , NULL
        },

        {
            "/data/nvram/APCFG/APRDCL/Audio_Effect",       VER(AP_CFG_RDCL_FILE_AUDIO_EFFECT_LID),         CFG_FILE_AUDIO_EFFECT_REC_SIZE,
            CFG_FILE_AUDIO_EFFECT_REC_TOTAL,                   SIGNLE_DEFUALT_REC,                (char *)&audio_effect_custom_default, DataReset , NULL
        },

        {
            "/data/nvram/APCFG/APRDEB/WIFI",	    	VER(AP_CFG_RDEB_FILE_WIFI_LID),		    CFG_FILE_WIFI_REC_SIZE,
            CFG_FILE_WIFI_REC_TOTAL,		    	SIGNLE_DEFUALT_REC,				    (char *)&stWifiCfgDefault, DataConvert , Wifi_ConvertFunction
        },

        {
            "/data/nvram/APCFG/APRDEB/WIFI_CUSTOM",	VER(AP_CFG_RDEB_WIFI_CUSTOM_LID),	CFG_FILE_WIFI_CUSTOM_REC_SIZE,
            CFG_FILE_WIFI_CUSTOM_REC_TOTAL,		    SIGNLE_DEFUALT_REC,				    (char *)&stWifiCustomDefault, DataReset , NULL
        },

        {
            "/data/nvram/APCFG/APRDCL/Audio_Sph_Med",       VER(AP_CFG_RDCL_FILE_AUDIO_PARAM_MED_LID),         CFG_FILE_AUDIO_PARAM_MED_REC_SIZE,
            CFG_FILE_AUDIO_PARAM_MED_REC_TOTAL,                   SIGNLE_DEFUALT_REC,                (char *)&audio_param_med_default, DataReset , NULL
        },

        {
            "/data/nvram/APCFG/APRDCL/Audio_Vol_custom",       VER(AP_CFG_RDCL_FILE_AUDIO_VOLUME_CUSTOM_LID),         CFG_FILE_AUDIO_VOLUME_CUSTOM_REC_SIZE,
            CFG_FILE_AUDIO_VOLUME_CUSTOM_REC_TOTAL,           SIGNLE_DEFUALT_REC,                (char *)&audio_volume_custom_default, DataReset , NULL
        },

        {
            "/data/nvram/APCFG/APRDCL/Sph_Dual_Mic",       VER(AP_CFG_RDCL_FILE_DUAL_MIC_CUSTOM_LID),         CFG_FILE_SPEECH_DUAL_MIC_SIZE,
            CFG_FILE_SPEECH_DUAL_MIC_TOTAL,           SIGNLE_DEFUALT_REC,                (char *)&dual_mic_custom_default, DataReset , NULL
        },

        {
            "/data/nvram/APCFG/APRDCL/Audio_Wb_Sph",       VER(AP_CFG_RDCL_FILE_AUDIO_WB_PARAM_LID),         CFG_FILE_WB_SPEECH_REC_SIZE,
            CFG_FILE_WB_SPEECH_REC_TOTAL,                   SIGNLE_DEFUALT_REC,                                   (char *)&wb_speech_custom_default, DataReset , NULL
        },

        {
            "/data/nvram/APCFG/APRDEB/PRODUCT_INFO",       VER(AP_CFG_REEB_PRODUCT_INFO_LID),         CFG_FILE_PRODUCT_INFO_SIZE,
            CFG_FILE_PRODUCT_INFO_TOTAL,                   SIGNLE_DEFUALT_REC,                                   (char *)&stPRODUCT_INFOConfigDefault,DataReset, NULL
        },

        {
            "/data/nvram/APCFG/APRDCL/Headphone_CompFlt",       VER(AP_CFG_RDCL_FILE_HEADPHONE_COMPFLT_LID),         CFG_FILE_AUDIO_COMPFLT_REC_SIZE,
            CFG_FILE_HEADPHONE_COMPFLT_REC_TOTAL,                   SIGNLE_DEFUALT_REC,                (char *)&audio_hcf_custom_default, DataReset , NULL
        },

        { "/data/nvram/APCFG/APRDCL/Audio_gain_table",   VER(AP_CFG_RDCL_FILE_AUDIO_GAIN_TABLE_LID), CFG_FILE_AUDIO_GAIN_TABLE_CUSTOM_REC_SIZE,
            CFG_FILE_AUDIO_GAIN_TABLE_CUSTOM_REC_TOTAL, SIGNLE_DEFUALT_REC  ,	 (char *)&Gain_control_table_default, DataReset , NULL
        },

        {
            "/data/nvram/APCFG/APRDCL/Audio_ver1_Vol_custom",       VER(AP_CFG_RDCL_FILE_AUDIO_VER1_VOLUME_CUSTOM_LID),         CFG_FILE_AUDIO_VER1_VOLUME_CUSTOM_REC_SIZE,
            CFG_FILE_AUDIO_VER1_VOLUME_CUSTOM_REC_TOTAL,           SIGNLE_DEFUALT_REC,                (char *)&audio_ver1_custom_default, DataReset , NULL
        },
        
        { "/data/nvram/APCFG/APRDCL/Audio_Hd_Record_Param",   VER(AP_CFG_RDCL_FILE_AUDIO_HD_REC_PAR_LID), CFG_FILE_AUDIO_HD_REC_PAR_SIZE,
            CFG_FILE_AUDIO_HD_REC_PAR_TOTAL, SIGNLE_DEFUALT_REC  ,    (char *)&Hd_Recrod_Par_default, DataReset , NULL
        },
        
        { "/data/nvram/APCFG/APRDCL/Audio_Hd_Record_Scene_Table",   VER(AP_CFG_RDCL_FILE_AUDIO_HD_REC_SCENE_LID), CFG_FILE_AUDIO_HD_REC_SCENE_TABLE_SIZE,
            CFG_FILE_AUDIO_HD_REC_SCENE_TABLE_TOTAL, SIGNLE_DEFUALT_REC  ,    (char *)&Hd_Recrod_Scene_Table_default, DataReset , NULL
        },

		{ "/data/nvram/APCFG/APRDCL/Audio_Hd_Record_48k_Param",   VER(AP_CFG_RDCL_FILE_AUDIO_HD_REC_48K_PAR_LID), CFG_FILE_AUDIO_HD_REC_48K_PAR_SIZE,
            CFG_FILE_AUDIO_HD_REC_48K_PAR_TOTAL, SIGNLE_DEFUALT_REC  ,    (char *)&Hd_Recrod_48k_Par_default, DataReset , NULL
        },

        { "/data/nvram/APCFG/APRDCL/Audio_Buffer_DC_Calibration_Param",   VER(AP_CFG_RDCL_FILE_AUDIO_BUFFER_DC_CALIBRATION_PAR_LID), CFG_FILE_AUDIO_BUFFER_DC_CALIBRATION_PAR_SIZE,
            CFG_FILE_AUDIO_BUFFER_DC_CALIBRATION_PAR_TOTAL, SIGNLE_DEFUALT_REC  ,    (char *)&Audio_Buffer_DC_Calibration_Par_default, DataReset , NULL
        },
        { "/data/nvram/APCFG/APRDCL/VibSpk_CompFlt",   VER(AP_CFG_RDCL_FILE_VIBSPK_COMPFLT_LID), CFG_FILE_AUDIO_COMPFLT_REC_SIZE,
            CFG_FILE_VIBSPK_COMPFLT_REC_TOTAL, SIGNLE_DEFUALT_REC  ,    (char *)&audio_vibspk_custom_default, DataReset , NULL
        },
        { "/data/nvram/APCFG/APRDCL/MusicDRC_CompFlt",   VER(AP_CFG_RDCL_FILE_AUDIO_MUSIC_DRC_LID), CFG_FILE_AUDIO_COMPFLT_REC_SIZE,
            CFG_FILE_MUSICDRC_COMPFLT_REC_TOTAL, SIGNLE_DEFUALT_REC  ,    (char *)&audio_musicdrc_custom_default, DataReset , NULL
        },
        { "/data/nvram/APCFG/APRDCL/RingToneDRC_CompFlt",   VER(AP_CFG_RDCL_FILE_AUDIO_RINGTONE_DRC_LID), CFG_FILE_AUDIO_COMPFLT_REC_SIZE,
            CFG_FILE_RINGTONEDRC_COMPFLT_REC_TOTAL, SIGNLE_DEFUALT_REC  ,    (char *)&audio_ringtonedrc_custom_default, DataReset , NULL
        },
        { "/data/nvram/APCFG/APRDCL/Audio_ANC",   VER(AP_CFG_RDCL_FILE_AUDIO_ANC_LID), CFG_FILE_SPEECH_ANC_SIZE,
            CFG_FILE_SPEECH_ANC_TOTAL, SIGNLE_DEFUALT_REC  ,    (char *)&speech_ANC_custom_default, DataReset , NULL
        },
        { "/data/nvram/APCFG/APRDCL/Spk_Monitor",   VER(AP_CFG_RDCL_FILE_AUDIO_SPEAKER_MONITOR_LID), CFG_FILE_SPEAKER_MONITOR_SIZE,
            CFG_FILE_SPEAK_MONITOR_REC_TOTAL, SIGNLE_DEFUALT_REC  ,    (char *)&speaker_monitor_par_default, DataReset , NULL
        },
	/* custom */
        { "/data/nvram/APCFG/APRDCL/Rootsign",   VER(AP_CFG_CUSTOM_FILE_ROOTSIGN_LID), CFG_FILE_ROOTSIGN_REC_SIZE,
            CFG_FILE_ROOTSIGN_REC_TOTAL, SIGNLE_DEFUALT_REC  ,    (char *)&stRootsignDefault, DataReset , NULL
        },
        { "/data/nvram/APCFG/APRDCL/Recoverylock",   VER(AP_CFG_CUSTOM_FILE_RECOVERYLOCK_LID), CFG_FILE_RECOVERYLOCK_REC_SIZE,
            CFG_FILE_RECOVERYLOCK_REC_TOTAL, SIGNLE_DEFUALT_REC  ,    (char *)&stRecoverylockDefault, DataReset , NULL
        },
        { "/data/nvram/APCFG/APRDCL/Calibration",   VER(AP_CFG_CUSTOM_FILE_CALIBRATION_LID), CFG_FILE_CALIBRATION_REC_SIZE,
            CFG_FILE_CALIBRATION_REC_TOTAL, SIGNLE_DEFUALT_REC  ,    (char *)&stCalibrationDefault, DataReset , NULL
        },
        { "/data/nvram/APCFG/APRDCL/Acceleration",   VER(AP_CFG_CUSTOM_FILE_ACCELERATION_LID), CFG_FILE_ACCELERATION_REC_SIZE,
            CFG_FILE_ACCELERATION_REC_TOTAL, SIGNLE_DEFUALT_REC  ,    (char *)&stAccelerationDefault, DataReset , NULL
        },
        { "/data/nvram/APCFG/APRDCL/Gyroscope",   VER(AP_CFG_CUSTOM_FILE_GYROSCOPE_LID), CFG_FILE_GYROSCOPE_REC_SIZE,
            CFG_FILE_GYROSCOPE_REC_TOTAL, SIGNLE_DEFUALT_REC  ,    (char *)&stGyroscopeDefault, DataReset , NULL
        },
        { "/data/nvram/APCFG/APRDCL/Shopdemo",   VER(AP_CFG_CUSTOM_FILE_SHOPDEMO_TOOL_LID), CFG_FILE_SHOPDEMO_REC_SIZE,
            CFG_FILE_SHOPDEMO_REC_TOTAL, SIGNLE_DEFUALT_REC  ,    (char *)&stShopdemoDefault, DataReset , NULL
        },
    };

    int iNvRamFileMaxLID=AP_CFG_CUSTOM_FILE_MAX_LID;
    extern int iNvRamFileMaxLID;
    const unsigned int g_i4CFG_File_Custom_Count = sizeof(g_akCFG_File_Custom)/sizeof(TCFG_FILE);

    extern const TCFG_FILE g_akCFG_File_Custom[];

    extern const unsigned int g_i4CFG_File_Custom_Count;

    int iFileWIFILID=AP_CFG_RDEB_FILE_WIFI_LID;
    extern int iFileWIFILID;
    int iFileCustomWIFILID=AP_CFG_RDEB_WIFI_CUSTOM_LID;
    extern int iFileCustomWIFILID;
    int iFilePRODUCT_INFOLID=AP_CFG_REEB_PRODUCT_INFO_LID;
    extern int iFilePRODUCT_INFOLID;

   int Wifi_ConvertFunction(int CurrentVerID, int NewVerID , char* pSrcMem, char* pDstMem){
       if(NULL == pSrcMem || NULL == pDstMem){
             return 0;
       }else if((0 == CurrentVerID && 0== NewVerID) || (1 == CurrentVerID && 0== NewVerID)){
              pDstMem = pSrcMem;
              return 1;
           }else{
              return 0;
           }
    }

#ifdef __cplusplus
}
#endif

#endif
