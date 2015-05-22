#ifndef AUDIO_HCF_DEFAULT_H
#define AUDIO_HCF_DEFAULT_H

#define BES_LOUDNESS_HCF_L_HPF_FC       0
#define BES_LOUDNESS_HCF_L_HPF_ORDER    0
#define BES_LOUDNESS_HCF_L_BPF_FC       0,0,0,0,0,0,0,0
#define BES_LOUDNESS_HCF_L_BPF_BW       0,0,0,0,0,0,0,0
#define BES_LOUDNESS_HCF_L_BPF_GAIN     0 << 8,0 << 8,0 << 8,0 << 8,0 << 8,0 << 8,0 << 8,0 << 8
#define BES_LOUDNESS_HCF_L_LPF_FC       0
#define BES_LOUDNESS_HCF_L_LPF_ORDER    0
#define BES_LOUDNESS_HCF_R_HPF_FC       0
#define BES_LOUDNESS_HCF_R_HPF_ORDER    0
#define BES_LOUDNESS_HCF_R_BPF_FC       0,0,0,0,0,0,0,0
#define BES_LOUDNESS_HCF_R_BPF_BW       0,0,0,0,0,0,0,0
#define BES_LOUDNESS_HCF_R_BPF_GAIN     0 << 8,0 << 8,0 << 8,0 << 8,0 << 8,0 << 8,0 << 8,0 << 8
#define BES_LOUDNESS_HCF_R_LPF_FC       0
#define BES_LOUDNESS_HCF_R_LPF_ORDER    0

#define BES_LOUDNESS_HCF_SEP_LR_FILTER  0

#define BES_LOUDNESS_HCF_WS_GAIN_MAX    0
#define BES_LOUDNESS_HCF_WS_GAIN_MIN    0
#define BES_LOUDNESS_HCF_FILTER_FIRST   0

#define BES_LOUDNESS_HCF_NUM_BANDS      0
#define BES_LOUDNESS_HCF_FLT_BANK_ORDER 0
#define BES_LOUDNESS_HCF_DRC_DELAY      0
#define BES_LOUDNESS_HCF_CROSSOVER_FREQ 0,0,0,0,0,0,0
#define BES_LOUDNESS_HCF_SB_MODE        0,0,0,0,0,0,0,0
#define BES_LOUDNESS_HCF_SB_GAIN        0,0,0,0,0,0,0,0
#define BES_LOUDNESS_HCF_GAIN_MAP_IN    \
        0,0,0,0,0,                  \
        0,0,0,0,0,                  \
        0,0,0,0,0,                  \
        0,0,0,0,0,                  \
        0,0,0,0,0,                  \
        0,0,0,0,0,                  \
        0,0,0,0,0,                  \
        0,0,0,0,0
#define BES_LOUDNESS_HCF_GAIN_MAP_OUT   \
        0,0,0,0,0,                  \
        0,0,0,0,0,                  \
        0,0,0,0,0,                  \
        0,0,0,0,0,                  \
        0,0,0,0,0,                  \
        0,0,0,0,0,                  \
        0,0,0,0,0,                  \
        0,0,0,0,0
#define BES_LOUDNESS_HCF_ATT_TIME       \
        0,0,0,0,0,0,                  \
        0,0,0,0,0,0,                  \
        0,0,0,0,0,0,                  \
        0,0,0,0,0,0,                  \
        0,0,0,0,0,0,                  \
        0,0,0,0,0,0,                  \
        0,0,0,0,0,0,                  \
        0,0,0,0,0,0
#define BES_LOUDNESS_HCF_REL_TIME       \
        0,0,0,0,0,0,                  \
        0,0,0,0,0,0,                  \
        0,0,0,0,0,0,                  \
        0,0,0,0,0,0,                  \
        0,0,0,0,0,0,                  \
        0,0,0,0,0,0,                  \
        0,0,0,0,0,0,                  \
        0,0,0,0,0,0
#define BES_LOUDNESS_HCF_HYST_TH        \
        0,0,0,0,0,0,                  \
        0,0,0,0,0,0,                  \
        0,0,0,0,0,0,                  \
        0,0,0,0,0,0,                  \
        0,0,0,0,0,0,                  \
        0,0,0,0,0,0,                  \
        0,0,0,0,0,0,                  \
        0,0,0,0,0,0

#define BES_LOUDNESS_HCF_LIM_TH     0
#define BES_LOUDNESS_HCF_LIM_GN     0
#define BES_LOUDNESS_HCF_LIM_CONST  0
#define BES_LOUDNESS_HCF_LIM_DELAY  0

#endif
