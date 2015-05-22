#ifndef __MT_EMI_BM_H__
#define __MT_EMI_BW_H__

#define MET_EMI_BASE		(0xF0203000)
#define MET_APDMA_BASE		(0xF0212C00)
//#define MET_DRAMC_CH_A_BASE 	(0xF0004000)
//#define MET_DRAMC_CH_B_BASE 	(0xF0011000)
#define MET_DRAMC_CH_A_BASE 	(0xF020E000)
#define MET_DRAMC_CH_B_BASE 	(0xF0213000)

#define EMI_CONA    (MET_EMI_BASE + 0x000)
#define EMI_CONM    (MET_EMI_BASE + 0x060)
#define EMI_ARBA    (MET_EMI_BASE + 0x100)
//#define EMI_ARBB    (MET_EMI_BASE + 0x108)
#define EMI_ARBC    (MET_EMI_BASE + 0x110)
#define EMI_ARBD    (MET_EMI_BASE + 0x118)
#define EMI_ARBE    (MET_EMI_BASE + 0x120)
#define EMI_ARBF    (MET_EMI_BASE + 0x128)
#define EMI_ARBG    (MET_EMI_BASE + 0x130)
#define EMI_ARBH    (MET_EMI_BASE + 0x138)
#define EMI_BMEN    (MET_EMI_BASE + 0x400)
#define EMI_BCNT    (MET_EMI_BASE + 0x408)
#define EMI_TACT    (MET_EMI_BASE + 0x410)
#define EMI_TSCT    (MET_EMI_BASE + 0x418)
#define EMI_WACT    (MET_EMI_BASE + 0x420)
#define EMI_WSCT    (MET_EMI_BASE + 0x428)
#define EMI_BACT    (MET_EMI_BASE + 0x430)
#define EMI_BSCT    (MET_EMI_BASE + 0x438)
#define EMI_MSEL    (MET_EMI_BASE + 0x440)
#define EMI_TSCT2   (MET_EMI_BASE + 0x448)
#define EMI_TSCT3   (MET_EMI_BASE + 0x450)
#define EMI_WSCT2   (MET_EMI_BASE + 0x458)
#define EMI_WSCT3   (MET_EMI_BASE + 0x460)
#define EMI_WSCT4   (MET_EMI_BASE + 0x464)
#define EMI_MSEL2   (MET_EMI_BASE + 0x468)
#define EMI_MSEL3   (MET_EMI_BASE + 0x470)
#define EMI_MSEL4   (MET_EMI_BASE + 0x478)
#define EMI_MSEL5   (MET_EMI_BASE + 0x480)
#define EMI_MSEL6   (MET_EMI_BASE + 0x488)
#define EMI_MSEL7   (MET_EMI_BASE + 0x490)
#define EMI_MSEL8   (MET_EMI_BASE + 0x498)
#define EMI_MSEL9   (MET_EMI_BASE + 0x4A0)
#define EMI_MSEL10  (MET_EMI_BASE + 0x4A8)
#define EMI_BMID0   (MET_EMI_BASE + 0x4B0)
#define EMI_BMID1   (MET_EMI_BASE + 0x4B4)
#define EMI_BMID2   (MET_EMI_BASE + 0x4B8)
#define EMI_BMID3   (MET_EMI_BASE + 0x4BC)
#define EMI_BMID4   (MET_EMI_BASE + 0x4C0)
#define EMI_BMID5   (MET_EMI_BASE + 0x4C4)
#define EMI_BMID6   (MET_EMI_BASE + 0x4C8)
#define EMI_BMID7   (MET_EMI_BASE + 0x4CC)
#define EMI_BMID8   (MET_EMI_BASE + 0x4D0)
#define EMI_BMID9   (MET_EMI_BASE + 0x4D4)
#define EMI_BMID10   (MET_EMI_BASE + 0x4D8)
#define EMI_BMEN1   (MET_EMI_BASE + 0x4E0)
#define EMI_BMEN2   (MET_EMI_BASE + 0x4E8)
#define EMI_TTYPE1  (MET_EMI_BASE + 0x500)
#define EMI_TTYPE2  (MET_EMI_BASE + 0x508)
#define EMI_TTYPE3  (MET_EMI_BASE + 0x510)
#define EMI_TTYPE4  (MET_EMI_BASE + 0x518)
#define EMI_TTYPE5  (MET_EMI_BASE + 0x520)
#define EMI_TTYPE6  (MET_EMI_BASE + 0x528)
#define EMI_TTYPE7  (MET_EMI_BASE + 0x530)
#define EMI_TTYPE8  (MET_EMI_BASE + 0x538)
#define EMI_TTYPE9  (MET_EMI_BASE + 0x540)
#define EMI_TTYPE10  (MET_EMI_BASE + 0x548)
#define EMI_TTYPE11  (MET_EMI_BASE + 0x550)
#define EMI_TTYPE12  (MET_EMI_BASE + 0x558)
#define EMI_TTYPE13  (MET_EMI_BASE + 0x560)
#define EMI_TTYPE14  (MET_EMI_BASE + 0x568)
#define EMI_TTYPE15  (MET_EMI_BASE + 0x570)
#define EMI_TTYPE16  (MET_EMI_BASE + 0x578)
#define EMI_TTYPE17  (MET_EMI_BASE + 0x580)
#define EMI_TTYPE18  (MET_EMI_BASE + 0x588)
#define EMI_TTYPE19  (MET_EMI_BASE + 0x590)
#define EMI_TTYPE20  (MET_EMI_BASE + 0x598)
#define EMI_TTYPE21  (MET_EMI_BASE + 0x5A0)

#define MET_APDMA_EN 	(MET_APDMA_BASE + 0x008)
#define MET_APDMA_CON 	(MET_APDMA_BASE + 0x018)
#define MET_APDMA_SRC 	(MET_APDMA_BASE + 0x01C)
#define MET_APDMA_DST 	(MET_APDMA_BASE + 0x020)
#define MET_APDMA_LEN 	(MET_APDMA_BASE + 0x024)

#define DRAMC_R2R_PAGE_HIT      (MET_DRAMC_CH_A_BASE + 0x280)
#define DRAMC_R2R_PAGE_MISS     (MET_DRAMC_CH_A_BASE + 0x284)
#define DRAMC_R2R_INTERBANK     (MET_DRAMC_CH_A_BASE + 0x288)
#define DRAMC_R2W_PAGE_HIT      (MET_DRAMC_CH_A_BASE + 0x28C)
#define DRAMC_R2W_PAGE_MISS     (MET_DRAMC_CH_A_BASE + 0x290)
#define DRAMC_R2W_INTERBANK     (MET_DRAMC_CH_A_BASE + 0x294)
#define DRAMC_W2R_PAGE_HIT      (MET_DRAMC_CH_A_BASE + 0x298)
#define DRAMC_W2R_PAGE_MISS     (MET_DRAMC_CH_A_BASE + 0x29C)
#define DRAMC_W2R_INTERBANK     (MET_DRAMC_CH_A_BASE + 0x2A0)
#define DRAMC_W2W_PAGE_HIT      (MET_DRAMC_CH_A_BASE + 0x2A4)
#define DRAMC_W2W_PAGE_MISS     (MET_DRAMC_CH_A_BASE + 0x2A8)
#define DRAMC_W2W_INTERBANK     (MET_DRAMC_CH_A_BASE + 0x2AC)
#define DRAMC_IDLE_COUNT        (MET_DRAMC_CH_A_BASE + 0x2B0)

#define DRAMC_R2R_PAGE_HIT1     (MET_DRAMC_CH_B_BASE + 0x280)
#define DRAMC_R2R_PAGE_MISS1    (MET_DRAMC_CH_B_BASE + 0x284)
#define DRAMC_R2R_INTERBANK1    (MET_DRAMC_CH_B_BASE + 0x288)
#define DRAMC_R2W_PAGE_HIT1     (MET_DRAMC_CH_B_BASE + 0x28C)
#define DRAMC_R2W_PAGE_MISS1    (MET_DRAMC_CH_B_BASE + 0x290)
#define DRAMC_R2W_INTERBANK1    (MET_DRAMC_CH_B_BASE + 0x294)
#define DRAMC_W2R_PAGE_HIT1     (MET_DRAMC_CH_B_BASE + 0x298)
#define DRAMC_W2R_PAGE_MISS1    (MET_DRAMC_CH_B_BASE + 0x29C)
#define DRAMC_W2R_INTERBANK1    (MET_DRAMC_CH_B_BASE + 0x2A0)
#define DRAMC_W2W_PAGE_HIT1     (MET_DRAMC_CH_B_BASE + 0x2A4)
#define DRAMC_W2W_PAGE_MISS1    (MET_DRAMC_CH_B_BASE + 0x2A8)
#define DRAMC_W2W_INTERBANK1    (MET_DRAMC_CH_B_BASE + 0x2AC)
#define DRAMC_IDLE_COUNT1       (MET_DRAMC_CH_B_BASE + 0x2B0)

typedef enum
{
    DRAMC_R2R,
    DRAMC_R2W,
    DRAMC_W2R,
    DRAMC_W2W,
    DRAMC_ALL
} DRAMC_Cnt_Type;

typedef enum
{
    BM_BOTH_READ_WRITE,
    BM_READ_ONLY,
    BM_WRITE_ONLY
} BM_RW_Type;

enum
{
    BM_TRANS_TYPE_1BEAT = 0x0,
    BM_TRANS_TYPE_2BEAT,
    BM_TRANS_TYPE_3BEAT,
    BM_TRANS_TYPE_4BEAT,
    BM_TRANS_TYPE_5BEAT,
    BM_TRANS_TYPE_6BEAT,
    BM_TRANS_TYPE_7BEAT,
    BM_TRANS_TYPE_8BEAT,
    BM_TRANS_TYPE_9BEAT,
    BM_TRANS_TYPE_10BEAT,
    BM_TRANS_TYPE_11BEAT,
    BM_TRANS_TYPE_12BEAT,
    BM_TRANS_TYPE_13BEAT,
    BM_TRANS_TYPE_14BEAT,
    BM_TRANS_TYPE_15BEAT,
    BM_TRANS_TYPE_16BEAT,
    BM_TRANS_TYPE_1Byte = 0 << 4,
    BM_TRANS_TYPE_2Byte = 1 << 4,
    BM_TRANS_TYPE_4Byte = 2 << 4,
    BM_TRANS_TYPE_8Byte = 3 << 4,
    BM_TRANS_TYPE_16Byte = 4 << 4,
    BM_TRANS_TYPE_BURST_WRAP = 0 << 7,
    BM_TRANS_TYPE_BURST_INCR = 1 << 7
};

#define BM_MASTER_APMCU 	(0x01)
#define BM_MASTER_APMCU_1 	(0x02)
#define BM_MASTER_MM_1 		(0x04)
#define BM_MASTER_MDMCU 	(0x08)
#define BM_MASTER_MDHW 		(0x10)
#define BM_MASTER_MM 		(0x20)
#define BM_MASTER_GPU 		(0x40)
#define BM_MASTER_GPU_1 	(0x80)
#define BM_MASTER_ALL 		(0xFF)

#define BUS_MON_EN 	(0x00000001)
#define BUS_MON_PAUSE 	(0x00000002)
#define BC_OVERRUN 	(0x00000100)

#define BM_COUNTER_MAX  (21)

#define BM_REQ_OK           (0)
#define BM_ERR_WRONG_REQ    (-1)
#define BM_ERR_OVERRUN      (-2)

#define BM_TTYPE_ENABLE		(0)
#define BM_TTYPE_DISABLE	(-1)

extern void MET_BM_Init(void);
extern void MET_BM_DeInit(void);
extern void MET_BM_Enable(const unsigned int enable);
//extern void BM_Disable(void);
extern void MET_BM_Pause(void);
extern void MET_BM_Continue(void);
extern unsigned int MET_BM_IsOverrun(void);
extern void MET_BM_SetReadWriteType(const unsigned int ReadWriteType);
extern int MET_BM_GetBusCycCount(void);
extern unsigned int MET_BM_GetTransAllCount(void);
extern int MET_BM_GetTransCount(const unsigned int counter_num);
extern int MET_BM_GetWordAllCount(void);
extern int MET_BM_GetWordCount(const unsigned int counter_num);
extern unsigned int MET_BM_GetBandwidthWordCount(void);
extern unsigned int MET_BM_GetOverheadWordCount(void);
extern int MET_BM_GetTransTypeCount(const unsigned int counter_num);
extern int MET_BM_SetMonitorCounter(const unsigned int counter_num,
		const unsigned int master,
		const unsigned int trans_type);
extern int MET_BM_SetMaster(const unsigned int counter_num,
		const unsigned int master);
extern int MET_BM_SetIDSelect(const unsigned int counter_num,
		const unsigned int id,
		const unsigned int enable);
extern int MET_BM_SetUltraHighFilter(const unsigned int counter_num,
		const unsigned int enable);
extern int MET_BM_SetLatencyCounter(void);
extern int MET_BM_GetLatencyCycle(const unsigned int counter_num);
extern int MET_BM_GetEmiDcm(void);
extern int MET_BM_SetEmiDcm(const unsigned int setting);

// DRAMC Channel A
extern unsigned int MET_DRAMC_GetPageHitCount(DRAMC_Cnt_Type CountType);
extern unsigned int MET_DRAMC_GetPageMissCount(DRAMC_Cnt_Type CountType);
extern unsigned int MET_DRAMC_GetInterbankCount(DRAMC_Cnt_Type CountType);
extern unsigned int MET_DRAMC_GetIdleCount(void);

// DRAMC Channel B
extern unsigned int MET_DRAMC_GetPageHitCount1(DRAMC_Cnt_Type CountType);
extern unsigned int MET_DRAMC_GetPageMissCount1(DRAMC_Cnt_Type CountType);
extern unsigned int MET_DRAMC_GetInterbankCount1(DRAMC_Cnt_Type CountType);
extern unsigned int MET_DRAMC_GetIdleCount1(void);

// Config
unsigned int MET_EMI_GetConA(void);
unsigned int MET_EMI_GetBMEN(void);
unsigned int MET_EMI_GetBMEN2(void);
unsigned int MET_EMI_GetMSEL(void);
unsigned int MET_EMI_GetMSEL2(void);
unsigned int MET_EMI_GetMSEL(void);
unsigned int MET_EMI_GetMSEL2(void);
unsigned int MET_EMI_GetMSEL3(void);
unsigned int MET_EMI_GetMSEL4(void);
unsigned int MET_EMI_GetMSEL5(void);
unsigned int MET_EMI_GetMSEL6(void);
unsigned int MET_EMI_GetMSEL7(void);
unsigned int MET_EMI_GetMSEL8(void);
unsigned int MET_EMI_GetMSEL9(void);
unsigned int MET_EMI_GetMSEL10(void);
unsigned int MET_EMI_GetBMID0(void);
unsigned int MET_EMI_GetBMID1(void);
unsigned int MET_EMI_GetBMID2(void);
unsigned int MET_EMI_GetBMID3(void);
unsigned int MET_EMI_GetBMID4(void);
unsigned int MET_EMI_GetBMID5(void);
unsigned int MET_EMI_GetBMID6(void);
unsigned int MET_EMI_GetBMID7(void);
unsigned int MET_EMI_GetBMID8(void);
unsigned int MET_EMI_GetBMID9(void);
unsigned int MET_EMI_GetBMID10(void);

void MET_APDMA_Enable(const unsigned int enable);
void MET_APDMA_SetSrcAddr(const unsigned int src);
void MET_APDMA_SetDstAddr(const unsigned int dst);
void MET_APDMA_SetLen(const unsigned int len);

unsigned int MET_APDMA_GetEna(void);

#endif  /* !__MT_EMI_BW_H__ */

