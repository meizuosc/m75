#ifndef _VDEC_VERIFY_VPARSER_H265_H_
#define _VDEC_VERIFY_VPARSER_H265_H_

#include <mach/mt_typedefs.h>

#define MAX_H265_PPS_COUNT  256
#define MAX_H265_SPS_COUNT  32
#define H265_Slice_Type_MAX 3

#define DBG_H265_PRINTF(output,arg, ...)      if(1){printk("[PARSE] " );printk( arg, ##__VA_ARGS__ );}
#define DEBUG_LEVEL 0
#define DBG_LEVEL_INFO   5
#define DBG_LEVEL_BUG1 2
#define H265_DRV_PARSE_SET_ERR_RET(x) printk("[ERROR] Error %d!!!!\n",x )
#define MRG_MAX_NUM_CANDS           5
#define MAX_INT                     0x7FFFFFFF  ///< max. value of signed 32-bit integer

enum RefPicList
{
  REF_PIC_LIST_0 = 0,   ///< reference list 0
  REF_PIC_LIST_1 = 1,   ///< reference list 1
  REF_PIC_LIST_C = 2,   ///< combined reference list for uni-prediction in B-Slices
  REF_PIC_LIST_X = 100  ///< special mark
};

enum SliceType
{
  HEVC_B_SLICE,
  HEVC_P_SLICE,
  HEVC_I_SLICE
};

typedef enum _H265_DPB_SIZE_T
{
    H265_LEVEL_1_0 = 10,
    H265_LEVEL_1_1 = 11,
    H265_LEVEL_1_2 = 12,    
    H265_LEVEL_1_3 = 13,
    H265_LEVEL_2_0 = 20,
    H265_LEVEL_2_1 = 21,
    H265_LEVEL_2_2 = 22,
    H265_LEVEL_3_0 = 30,
    H265_LEVEL_3_1 = 31,
    H265_LEVEL_3_2 = 32,
    H265_LEVEL_4_0 = 40,
    H265_LEVEL_4_1 = 41,
}H265_DPB_SIZE;

enum ErrorTypes
{
    PARSE_OK,
    PPS_SYNTAX_ERROR,
    SPS_SYNTAX_ERROR,
    SLICE_SYNTAX_ERROR,
    RPS_SYNTAX_ERROR,
    SL_SYNTAX_ERROR,
    SET_REG_SYNTAX_ERROR,
    NOT_SUPPORT
};


void vHEVCInitROM(UINT32 u4InstID);
void vHEVCAssignQuantParam(UINT32 u4InstID, VDEC_INFO_DEC_PRM_T *ptVerMpvDecPrm);
void vHEVCVerifyFlushBufRefInfo(UINT32 u4InstID);
void vHEVCVerifyPrepareFBufInfo(UINT32 u4InstID, VDEC_INFO_DEC_PRM_T *tVerMpvDecPrm);
void vHEVCVDecSetBRefPicList(UINT32 u4InstID);
void vHEVCSetupBRefPicList(UINT32 u4InstID, UINT32 *pu4RefIdx, UINT32 u4TFldListIdx, UINT32 u4BFldListIdx, BOOL *fgDiff);
void vHEVCSetupRefPicList(UINT32 u4InstID);
void vHEVCPartitionDPB(UINT32 u4InstID);
void vHEVCSetCurrFBufIdx(UINT32 u4InstID, UINT32 u4DecFBufIdx);
UINT32 vHEVCParseNALs(UINT32 u4InstID);
UINT32 vHEVCVDecSetRefPicList(UINT32 u4InstID, VDEC_INFO_DEC_PRM_T *tVerMpvDecPrm);
UINT32 vHEVCVerifyVDecSetPicInfo(UINT32 u4InstID, VDEC_INFO_DEC_PRM_T *ptVerMpvDecPrm);
UINT32 vHEVCPrepareRefPiclist(UINT32 u4InstID, VDEC_INFO_DEC_PRM_T *tVerMpvDecPrm);
UINT32 vHEVCSearchRealPic(UINT32 u4InstID);
UINT32 vHEVCAllocateFBuf(UINT32 u4InstID, VDEC_INFO_DEC_PRM_T *tVerMpvDecPrm, BOOL fgFillCurrFBuf);

#endif // _PR_EMU_H_

