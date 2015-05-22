#ifndef AUDIO_ACF_DEFAULT_H
#define AUDIO_ACF_DEFAULT_H

#define BES_LOUDNESS_ACF_L_HPF_FC       200
#define BES_LOUDNESS_ACF_L_HPF_ORDER    4
#define BES_LOUDNESS_ACF_L_BPF_FC       0,0,0,0,0,0,0,0
#define BES_LOUDNESS_ACF_L_BPF_BW       0,0,0,0,0,0,0,0
#define BES_LOUDNESS_ACF_L_BPF_GAIN     0,0,0,0,0,0,0,0
#define BES_LOUDNESS_ACF_L_LPF_FC       12000
#define BES_LOUDNESS_ACF_L_LPF_ORDER    1
#define BES_LOUDNESS_ACF_R_HPF_FC       0
#define BES_LOUDNESS_ACF_R_HPF_ORDER    0
#define BES_LOUDNESS_ACF_R_BPF_FC       0,0,0,0,0,0,0,0
#define BES_LOUDNESS_ACF_R_BPF_BW       0,0,0,0,0,0,0,0
#define BES_LOUDNESS_ACF_R_BPF_GAIN     0,0,0,0,0,0,0,0
#define BES_LOUDNESS_ACF_R_LPF_FC       0
#define BES_LOUDNESS_ACF_R_LPF_ORDER    0

#define BES_LOUDNESS_ACF_SEP_LR_FILTER  0

#define BES_LOUDNESS_ACF_WS_GAIN_MAX    0
#define BES_LOUDNESS_ACF_WS_GAIN_MIN    0
#define BES_LOUDNESS_ACF_FILTER_FIRST   0

#define BES_LOUDNESS_ACF_NUM_BANDS      0
#define BES_LOUDNESS_ACF_FLT_BANK_ORDER 0
#define BES_LOUDNESS_ACF_DRC_DELAY      0
#define BES_LOUDNESS_ACF_CROSSOVER_FREQ 0,0,0,0,0,0,0
#define BES_LOUDNESS_ACF_SB_MODE        0,1,0,0,0,0,0,0
#define BES_LOUDNESS_ACF_SB_GAIN        0,0,0,0,0,0,0,0
#define BES_LOUDNESS_ACF_GAIN_MAP_IN    \
        0,0,0,0,0,                  \
        0,0,0,0,0,                  \
        0,0,0,0,0,                  \
        0,0,0,0,0,                  \
        0,0,0,0,0,                  \
        0,0,0,0,0,                  \
        0,0,0,0,0,                  \
        0,0,0,0,0
#define BES_LOUDNESS_ACF_GAIN_MAP_OUT   \
        0,0,0,0,0,                  \
        0,0,0,0,0,                  \
        0,0,0,0,0,                  \
        0,0,0,0,0,                  \
        0,0,0,0,0,                  \
        0,0,0,0,0,                  \
        0,0,0,0,0,                  \
        0,0,0,0,0
#define BES_LOUDNESS_ACF_ATT_TIME       \
        0,0,0,0,0,0,                  \
        0,0,0,0,0,0,                  \
        0,0,0,0,0,0,                  \
        0,0,0,0,0,0,                  \
        0,0,0,0,0,0,                  \
        0,0,0,0,0,0,                  \
        0,0,0,0,0,0,                  \
        0,0,0,0,0,0
#define BES_LOUDNESS_ACF_REL_TIME       \
        0,0,0,0,0,0,                  \
        0,0,0,0,0,0,                  \
        0,0,0,0,0,0,                  \
        0,0,0,0,0,0,                  \
        0,0,0,0,0,0,                  \
        0,0,0,0,0,0,                  \
        0,0,0,0,0,0,                  \
        0,0,0,0,0,0
#define BES_LOUDNESS_ACF_HYST_TH        \
        0,0,0,0,0,0,                  \
        0,0,0,0,0,0,                  \
        0,0,0,0,0,0,                  \
        0,0,0,0,0,0,                  \
        0,0,0,0,0,0,                  \
        0,0,0,0,0,0,                  \
        0,0,0,0,0,0,                  \
        0,0,0,0,0,0

#define BES_LOUDNESS_ACF_LIM_TH     0
#define BES_LOUDNESS_ACF_LIM_GN     0
#define BES_LOUDNESS_ACF_LIM_CONST  0
#define BES_LOUDNESS_ACF_LIM_DELAY  0

#endif
