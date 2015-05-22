#ifndef AUDIO_RINGTONE_DRC_DEFAULT_H
#define AUDIO_RINGTONE_DRC_DEFAULT_H

#define BES_LOUDNESS_RINGTONEDRC_L_HPF_FC       0
#define BES_LOUDNESS_RINGTONEDRC_L_HPF_ORDER    0
#define BES_LOUDNESS_RINGTONEDRC_L_BPF_FC       0,0,0,0,0,0,0,0
#define BES_LOUDNESS_RINGTONEDRC_L_BPF_BW       0,0,0,0,0,0,0,0
#define BES_LOUDNESS_RINGTONEDRC_L_BPF_GAIN     0,0,0,0,0,0,0,0
#define BES_LOUDNESS_RINGTONEDRC_L_LPF_FC       0
#define BES_LOUDNESS_RINGTONEDRC_L_LPF_ORDER    0
#define BES_LOUDNESS_RINGTONEDRC_R_HPF_FC       0
#define BES_LOUDNESS_RINGTONEDRC_R_HPF_ORDER    0
#define BES_LOUDNESS_RINGTONEDRC_R_BPF_FC       0,0,0,0,0,0,0,0
#define BES_LOUDNESS_RINGTONEDRC_R_BPF_BW       0,0,0,0,0,0,0,0
#define BES_LOUDNESS_RINGTONEDRC_R_BPF_GAIN     0,0,0,0,0,0,0,0
#define BES_LOUDNESS_RINGTONEDRC_R_LPF_FC       0
#define BES_LOUDNESS_RINGTONEDRC_R_LPF_ORDER    0

#define BES_LOUDNESS_RINGTONEDRC_SEP_LR_FILTER  0

#define BES_LOUDNESS_RINGTONEDRC_WS_GAIN_MAX    0
#define BES_LOUDNESS_RINGTONEDRC_WS_GAIN_MIN    0
#define BES_LOUDNESS_RINGTONEDRC_FILTER_FIRST   0

#define BES_LOUDNESS_RINGTONEDRC_NUM_BANDS      5
#define BES_LOUDNESS_RINGTONEDRC_FLT_BANK_ORDER 0
#define BES_LOUDNESS_RINGTONEDRC_DRC_DELAY      0
#define BES_LOUDNESS_RINGTONEDRC_CROSSOVER_FREQ 110,440,1760,8000,0,0,0
#define BES_LOUDNESS_RINGTONEDRC_SB_MODE        1,1,0,0,0,0,0,0
#define BES_LOUDNESS_RINGTONEDRC_SB_GAIN        -10240,-5120,0,1024,0,0,0,0
#define BES_LOUDNESS_RINGTONEDRC_GAIN_MAP_IN    \
        -15360,-12800,-10240,-7680,0,                  \
        -15360,-12800,-10240,-7680,0,                  \
        -15360,-12800,-10240,-7680,0,                  \
        -15360,-12800,-10240,-7680,0,                  \
        -15360,-12800,-10240,-7680,0,                  \
        -15360,-12800,-10240,-7680,0,                  \
        -15360,-12800,-10240,-7680,0,                  \
        -15360,-12800,-10240,-7680,0
#define BES_LOUDNESS_RINGTONEDRC_GAIN_MAP_OUT   \
        6144,6144,6144,6144,0,                  \
        6144,6144,6144,6144,0,                  \
        6144,6144,6144,6144,0,                  \
        6144,6144,6144,6144,0,                  \
        6144,6144,6144,6144,0,                  \
        6144,6144,6144,6144,0,                  \
        6144,6144,6144,6144,0,                  \
        6144,6144,6144,6144,0
#define BES_LOUDNESS_RINGTONEDRC_ATT_TIME       \
        164,164,164,164,164,164,                  \
        164,164,164,164,164,164,                  \
        164,164,164,164,164,164,                  \
        164,164,164,164,164,164,                  \
        164,164,164,164,164,164,                  \
        164,164,164,164,164,164,                  \
        164,164,164,164,164,164,                  \
        164,164,164,164,164,164
#define BES_LOUDNESS_RINGTONEDRC_REL_TIME       \
        16400,16400,16400,16400,16400,16400,                  \
        16400,16400,16400,16400,16400,16400,                  \
        16400,16400,16400,16400,16400,16400,                  \
        16400,16400,16400,16400,16400,16400,                  \
        16400,16400,16400,16400,16400,16400,                  \
        16400,16400,16400,16400,16400,16400,                  \
        16400,16400,16400,16400,16400,16400,                  \
        16400,16400,16400,16400,16400,16400
#define BES_LOUDNESS_RINGTONEDRC_HYST_TH        \
        256,256,256,256,256,256,                  \
        256,256,256,256,256,256,                  \
        256,256,256,256,256,256,                  \
        256,256,256,256,256,256,                  \
        256,256,256,256,256,256,                  \
        256,256,256,256,256,256,                  \
        256,256,256,256,256,256,                  \
        256,256,256,256,256,256

#define BES_LOUDNESS_RINGTONEDRC_LIM_TH     0x7FFF
#define BES_LOUDNESS_RINGTONEDRC_LIM_GN     0x7FFF
#define BES_LOUDNESS_RINGTONEDRC_LIM_CONST  4
#define BES_LOUDNESS_RINGTONEDRC_LIM_DELAY  0

#endif
