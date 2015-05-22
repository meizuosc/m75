#ifndef AUDIO_VOIP_CUSTOM_H
#define AUDIO_VOIP_CUSTOM_H

/****************************************************
* Define default AP side VOIP parameters
*****************************************************/

//VoIP speech parameters common part
#define DEFAULT_VOIP_SPEECH_COMMON_PARAM  \
    0,  55997,  31000,    10752,      32769,      0,      0,      0, \
    0,      0,      0,      0


#ifdef MTK_DUAL_MIC_SUPPORT
#ifdef MTK_VOIP_NORMAL_DMNR
#define VOIP_SPEECH_MODE_PARA13 (371)
#define VOIP_SPEECH_MODE_PARA14 (23)
#define VOIP_SPEECH_MODE_PARA03 (29)
#define VOIP_SPEECH_MODE_PARA08 (400)
#else
#define VOIP_SPEECH_MODE_PARA13 (0)
#define VOIP_SPEECH_MODE_PARA14 (0)
#define VOIP_SPEECH_MODE_PARA03 (31)
#define VOIP_SPEECH_MODE_PARA08 (80)
#endif
#else
#define VOIP_SPEECH_MODE_PARA13 (0)
#define VOIP_SPEECH_MODE_PARA14 (0)
#define VOIP_SPEECH_MODE_PARA03 (31)
#define VOIP_SPEECH_MODE_PARA08 (80)
#endif

#ifdef MTK_DUAL_MIC_SUPPORT
#ifdef MTK_VOIP_HANDSFREE_DMNR
#define VOIP_SPEECH_SPK_MODE_PARA13 (307)
#define VOIP_SPEECH_SPK_MODE_PARA14 (31)
#define VOIP_SPEECH_SPK_MODE_PARA03 (29)
#define VOIP_SPEECH_SPK_MODE_PARA08 (400)      
#else
#define VOIP_SPEECH_SPK_MODE_PARA13 (0)
#define VOIP_SPEECH_SPK_MODE_PARA14 (0)
#define VOIP_SPEECH_SPK_MODE_PARA03 (29)
#define VOIP_SPEECH_SPK_MODE_PARA08 (276)  
#endif    
#else
#define VOIP_SPEECH_SPK_MODE_PARA13 (0)
#define VOIP_SPEECH_SPK_MODE_PARA14 (0)
#define VOIP_SPEECH_SPK_MODE_PARA03 (29)
#define VOIP_SPEECH_SPK_MODE_PARA08 (276)
#endif


//VoIP speech parameter different part
//1. normal mode
//2.loud speaker mode
//3. headset mode
//4.BT mode
#define DEFAULT_VOIP_SPEECH_MODE_PARAM \
    128,   253, 6212,    VOIP_SPEECH_MODE_PARA03, 57351,   31,    0,    32, \
    VOIP_SPEECH_MODE_PARA08,  197,  611,     0, 13066,   0|VOIP_SPEECH_MODE_PARA13,    0|VOIP_SPEECH_MODE_PARA14,   0, \
    \
    192,   224,  2218,    VOIP_SPEECH_SPK_MODE_PARA03, 57351, 24607,    0,     4, \
    VOIP_SPEECH_SPK_MODE_PARA08,  197,   611,     0, 13066,     VOIP_SPEECH_SPK_MODE_PARA13,     VOIP_SPEECH_SPK_MODE_PARA14,     0, \
    \
    0,   189,  8196,    31, 57351,    31,      0,    64, \
    80,  197,   611,     0, 13066,     0,     0,     0, \
    \
    0,   253,  8196,    31, 53255,    31,      0,     0, \
    80,  197,   611,     0, 13066,    0,     0,      0

//VoIP FIR parameter
//1. normal mode
//2.loud speaker mode
//3. headset mode
//4.BT mode
#define DEFAULT_VOIP_IN_FIR_PARAM \
    32767,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0,  \
    \
    32767,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0,  \
    \
    32767,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0,  \
    \
    32767,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0

//VoIP FIR parameter
//1. normal mode
//2.loud speaker mode
//3. headset mode
//4.BT mode
#define DEFAULT_VOIP_OUT_FIR_PARAM \
    32767,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0,  \
    \
    32767,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0,  \
    \
    32767,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0,  \
    \
    32767,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0, \
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0

#endif
