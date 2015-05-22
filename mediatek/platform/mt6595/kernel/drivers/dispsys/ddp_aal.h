#ifndef __DDP_AAL_H__
#define __DDP_AAL_H__

#define AAL_HIST_BIN        33 /* [0..32] */
#define AAL_DRE_POINT_NUM   29

typedef struct {
    // DRE
    int dre_map_bypass;
    // CABC
    int cabc_gainlmt[33];
} DISP_AAL_INITREG;

typedef struct {
    unsigned long maxHist[AAL_HIST_BIN];
} DISP_AAL_HIST;

typedef struct {
    int DREGainFltStatus[AAL_DRE_POINT_NUM];
    int cabc_fltgain_force;   // 10-bit ; [0,1023]
    int cabc_gainlmt[33];
    int FinalBacklight;       // 10-bit ; [0,1023]
} DISP_AAL_PARAM;


void disp_aal_on_end_of_frame(void);

extern int aal_dbg_en;

#endif

