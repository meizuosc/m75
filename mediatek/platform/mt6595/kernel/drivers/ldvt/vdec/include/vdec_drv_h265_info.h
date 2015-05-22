#ifndef _VDEC_DRV_H265_INFO_H_
#define _VDEC_DRV_H265_INFO_H_

//#include "x_os.h"
//#include "x_bim.h"
//#include "x_assert.h"
//#include "x_timer.h"

#include "drv_common.h"

#include "vdec_common_if.h"
#include "vdec_usage.h"

#include "vdec_info_h265.h"
#include "vdec_info_common.h"

#include "vdec_hal_if_h265.h"

#define OUT_OF_FILE 0x000001
//#define FORBIDEN_ERR 0x000002
#define DEC_INIT_FAILED 0x000003

#define NON_IDR_SLICE 0x01
#define IDR_SLICE 0x05
#define H265_SEI 0x06
#define H265_SPS 0x07
#define H265_PPS 0x08
#define H265_END_SEQ 0x0A
#define H265_PREFIX_NAL 0x0E
#define H265_SUB_SPS 0x0F
#define H265_SLICE_EXT 0x14

// Slice_type
#define H265_P_Slice 0
#define H265_B_Slice 1
#define H265_I_Slice 2
#define H265_SP_Slice 3
#define H265_SI_Slice 4
#define H265_P_Slice_ALL 5
#define H265_B_Slice_ALL 6
#define H265_I_Slice_ALL 7
#define H265_SP_Slice_ALL 8
#define H265_SI_Slice_ALL 9

#define NREF_PIC 0
#define SREF_PIC 1
#define LREF_PIC 2

//HEVC Profile IDC definitions
#define BASELINE_PROFILE					66      //!< YUV 4:2:0/8  "Baseline"
#define MAIN_PROFILE						77      //!< YUV 4:2:0/8  "Main"
#define EXTENDED_PROFILE					88      //!< YUV 4:2:0/8  "Extended"
#define FREXT_HP_PROFILE					100		//!< YUV 4:2:0/8 "High"
#define FREXT_Hi10P_PROFILE					110		//!< YUV 4:2:0/10 "High 10"
#define FREXT_Hi422_PROFILE					122		//!< YUV 4:2:2/10 "High 4:2:2"
#define FREXT_Hi444_PROFILE					244		//!< YUV 4:4:4/14 "High 4:4:4"
#define FREXT_CAVLC444_PROFILE				44      //!< YUV 4:4:4/14 "CAVLC 4:4:4"
#define SCALABLE_BASELINE_PROFILE	83		//!< Scalable Baseline profile
#define SCALABLE_HIGH_PROFILE		86		//!< Scalable High profile
#define MULTIVIEW_HIGH_PROFILE		118		//!< Multiview High profile
#define STEREO_HIGH_PROFILE		128		//!< Stereo High profile

#define YUV400 0
#define YUV420 1
#define YUV422 2
#define YUV444 3

#define H265_MAX_FB_NUM 17
#define H265_MAX_REF_PIC_LIST_NUM 3
#define H265_DPB_FBUF_UNKNOWN 0xFF
#define H265_FRM_IDX_UNKNOWN 0xFFFFFFFF
#define H265_FRM_NUM_UNKNOWN 0xFFFFFFFF
#define H265_FRM_NUM_WRAP_UNKNOWN 0x7FFFFFFF
#define H265_MAX_POC 0x7FFFFFFF
#define H265_MIN_POC 0x80000001
#define H265_MAX_PIC_NUM 0xEFFFFFFF

#define SubWidthC  [4]= { 1, 2, 2, 1};
#define SubHeightC [4]= { 1, 2, 1, 1};

#define H265_MAX_SPS_NUM 32
#define H265_MAX_PPS_NUM 256

#define H265_MAX_REF_LIST_NUM 6

#define fgIsH265SeqEnd(arg)   (arg & SEQ_END)

typedef enum _H265_DPB_SIZE_T
{
    H265_LEVEL_1_0 = 10,
    H265_LEVEL_1_b = 9,
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
    H265_LEVEL_5_0 = 50,
    H265_LEVEL_5_1 = 51,
}H265_DPB_SIZE;

typedef enum _H265_RPIC_LIST
{
    H265_P_SREF_TFLD = 0,
    H265_P_SREF_BFLD = 1,
    H265_B0_SREF_TFLD = 2,
    H265_B0_SREF_BFLD = 3,
    H265_B1_SREF_TFLD = 4,
    H265_B1_SREF_BFLD = 5,
    H265_P_LREF_TFLD = 6,
    H265_P_LREF_BFLD = 7,    
    H265_B_LREF_TFLD = 8,
    H265_B_LREF_BFLD = 9,   
}H265_RPIC_LIST;

typedef enum _H265_DPB_SEARCH_T
{
    H265_DPB_S_EMPTY = 0,
    H265_DPB_S_DECODED = 1,
    H265_DPB_S_DECODED_NO_CURR = 2,
    H265_DPB_S_DECODED_NO_CURR_IP_ONLY = 3,
#if VDEC_SKYPE_SUPPORT    
    H265_DPB_S_SKYPE_ORDER = 4,
#endif
}H265_DPB_SEARCH_T;

typedef struct VDEC_INFO_H265_AU_T_
{
    UCHAR  ucPicStruct;
    UCHAR  ucDpbId;         // Idx in DPB                
    UINT32 u4PicCdType;
    INT32   i4POC;  
    UCHAR ucH265OwnFBUFListIdx;
    UCHAR ucH265RefFBUFListIdx[H265_MAX_FB_NUM];    
    UCHAR ucH265OutputListWrIdx;    
    UCHAR ucH265OutputListRdIdx;    
    UCHAR arH265OutputList[H265_MAX_FB_NUM];   
    UINT32 u4SlicePPSID;       
    H265_PPS_Data rH265AUPPSInfo;
    H265_SPS_Data rH265AUSPSInfo;
    VDEC_INFO_H265_LAST_INFO_T rH265AULastInfo;
}VDEC_INFO_H265_AU_T;

#ifdef VDEC_SR_SUPPORT
#define H265_MAX_DFB_NUM MPEG_DFB_NUM
typedef struct VDEC_INFO_H265_DFB_LSIT_INFO_T_
{
    UCHAR ucH265DFBListWrIdx;
    UCHAR ucH265DFBListRdIdx;
    UCHAR ucH265MaxBLPtsDFBIdx;
    UCHAR ucH265EDPBLastRemoveIdx;    
    UCHAR ucH265LastReconstructDFBIdx;
    UINT64 u8H265MaxBLPts;
    VDEC_INFO_H265_AU_T arH265DFBList[2][H265_MAX_DFB_NUM]; 
    VDEC_INFO_H265_FBUF_INFO_T arH265DFBFbInfo[2][H265_MAX_DFB_NUM];
    FBM_FRAMEINFO arH265DFBFrameInfo[H265_MAX_DFB_NUM];
}VDEC_INFO_H265_DFB_LSIT_INFO_T;
#endif

typedef struct _H265_DRV_INFO_T
{
    UCHAR ucH265DpbOutputFbId;
    UCHAR ucPredFbId;
    UINT32 u4PredSa;
    UINT32 u4CurrStartCodeAddr;
    UINT32 u4BitCount;
    UINT32 u4DecErrInfo;
    INT32 i4LatestIPOC;
    INT64 i8BasePTS;
    INT64 i8LatestRealPTS;
    INT32 i4PreFrmPOC;
    INT32 i4LatestRealPOC;
    UINT32 u4LatestSPSId;
    H265_SPS_Data arH265SPS[H265_MAX_SPS_NUM];
    H265_PPS_Data arH265PPS[H265_MAX_PPS_NUM];
    H265_Slice_Hdr_Data rH265SliceHdr;
    H265_SEI_Data rH265SEI;
    VDEC_INFO_H265_FBUF_INFO_T arH265FbInfo[H265_MAX_FB_NUM];
#if VDEC_MVC_SUPPORT
    VDEC_INFO_H265_FBUF_INFO_T rH265PrevFbInfo;
#endif
#ifdef VDEC_SR_SUPPORT
    VDEC_INFO_H265_FBUF_INFO_T arH265EDpbInfo[MAX_EDPB_NUM];     // 0: FRef, 1: BRef, 2:Working
    //VDEC_INFO_H265_DFB_LSIT_INFO_T rH265DFBInfo;
    VDEC_INFO_H265_DFB_LSIT_INFO_T *ptH265DFBInfo;
#endif
    VDEC_INFO_H265_REF_PIC_LIST_T arH265RefPicList[H265_MAX_REF_LIST_NUM];
    VDEC_INFO_H265_P_REF_PRM_T rPRefPicListInfo;
    VDEC_INFO_H265_B_REF_PRM_T rBRefPicListInfo;
    VDEC_INFO_H265_BS_INIT_PRM_T rBsInitPrm;
    VDEC_NORM_INFO_T *prVDecNormInfo;
    VDEC_FBM_INFO_T *prVDecFbmInfo;
    VDEC_PIC_INFO_T *prVDecPicInfo;
    VDEC_INFO_DEC_PRM_T rVDecNormHalPrm;    
    void *prH265MvStartAddr;
#ifdef VDEC_SR_SUPPORT
    VDEC_SR_INFO_T *prVDecSRInfo;
#endif    
#if  (defined(DRV_VDEC_VDP_RACING) || defined(VDEC_PIP_WITH_ONE_HW))
    BOOL   fgIsRacing;
    UCHAR ucDecFrmBufCnt;
    BOOL   fgIsReadyPreOutput;
    BOOL   fgPreInsOutFBuf;
    UINT32 u4OutIdx;
    VDEC_INFO_H265_FBUF_INFO_T  rH265OutFbInfo[2];
    
    INT32   i4BaseOutPOC;
    UCHAR   ucWaitClrPicRefIno_FbId;
#endif

   //Patch for .mp4 file
   BOOL    fgHeaderDefineByCFA;
   UINT32 u4SPSHeaderCnt;
   UINT32 u4PPSHeaderCnt;

#if VDEC_MVC_SUPPORT
   VDEC_OFFSET_METADATA_INFO_T* prVDecOMTData;
#endif  
}H265_DRV_INFO_T;

#if 0
static H265_SPS_Data _arH265SPS[MPV_MAX_VLD][MAX_SPS_NUM];
static H265_PPS_Data _arH265PPS[MPV_MAX_VLD][MAX_PPS_NUM];
static H265_Slice_Hdr_Data _rH265SliceHdr[MPV_MAX_VLD];
static VDEC_INFO_H265_DEC_PRM_T _rH265DecPrm[MPV_MAX_VLD];
static VDEC_INFO_H265_FBUF_INFO_T _arH265FbInfo[MPV_MAX_VLD][H265_MAX_FB_NUM];
static VDEC_INFO_H265_FBUF_INFO_T *_prH265CurrFbInfo[MPV_MAX_VLD];
static VDEC_INFO_H265_REF_PIC_LIST_T _arH265RefPicList[MPV_MAX_VLD][6];
static VDEC_INFO_H265_P_REF_PRM_T _arH265PRefPicListInfo[MPV_MAX_VLD];
static VDEC_INFO_H265_B_REF_PRM_T _arH265BRefPicListInfo[MPV_MAX_VLD];
#endif
    
#define fgIsRefPic(arg) ((arg > 0))
#define fgIsIDRPic(arg) ((arg == IDR_SLICE))
//#define fgIsFrmPic(arg) ((_rH265DecPrm[arg].bPicStruct == FRAME))  prVDecH265DecPrm->fgIsFrmPic

#define fgIsISlice(bType) ((bType == H265_I_Slice) ||(bType == H265_SI_Slice) || (bType == H265_I_Slice_ALL))
#define fgIsPSlice(bType) ((bType == H265_P_Slice) ||(bType == H265_SP_Slice) || (bType == H265_P_Slice_ALL))
#define fgIsBSlice(bType) ((bType == H265_B_Slice) || (bType == H265_B_Slice_ALL))

#if VDEC_MVC_SUPPORT
#define fgIsMVCBaseView(arg) (arg == VDEC_MVC_BASE)
#define fgIsMVCDepView(arg) (arg == VDEC_MVC_DEP)
#define fgIsMVCType(arg)  (fgIsMVCBaseView(arg)  || fgIsMVCDepView(arg))
#endif

extern void vH265InitProc(UCHAR ucEsId);
extern INT32 i4H265VParseProc(UCHAR ucEsId, UINT32 u4VParseType);
extern BOOL fgH265VParseChkProc(UCHAR ucEsId);
extern INT32 i4H265UpdInfoToFbg(UCHAR ucEsId);
extern void vH265StartToDecProc(UCHAR ucEsId);
extern void vH265ISR(UCHAR ucEsId);
extern BOOL fgIsH265DecEnd(UCHAR ucEsId);
extern BOOL fgIsH265DecErr(UCHAR ucEsId);
extern BOOL fgH265ResultChk(UCHAR ucEsId);
extern BOOL fgIsH265InsToDispQ(UCHAR ucEsId);
extern BOOL fgIsH265GetFrmToDispQ(UCHAR ucEsId);
extern void vH265EndProc(UCHAR ucEsId);
extern BOOL fgH265FlushDPB(UCHAR ucEsId, BOOL fgWithOutput);
extern void vH265ReleaseProc(UCHAR ucEsId, BOOL fgResetHW);

#ifdef VDEC_SR_SUPPORT
extern BOOL fgH265GenEDPB(UCHAR ucEsId);
extern BOOL fgH265RestoreEDPB(UCHAR ucEsId, BOOL fgRestore);
extern BOOL fgIsH265GetSRFrmToDispQ(UCHAR ucEsId, BOOL fgSeqEnd, BOOL fgRefPic);
extern void vH265GetSeqFirstTarget(UCHAR ucEsId);
extern void vH265ReleaseSRDrvInfo(UCHAR ucEsId);
extern BOOL fgH265GetDFBInfo(UCHAR ucEsId, void **prDFBInfo);
extern void vH265RestoreSeqInfo(UCHAR ucEsId);
extern BOOL fgH265RvsDone(UCHAR ucEsId);
extern void vH265ReleaseEDPB(UCHAR ucEsId);
#endif

#if  (defined(DRV_VDEC_VDP_RACING) || defined(VDEC_PIP_WITH_ONE_HW))
extern BOOL fgIsH265PreInsToDispQ(UCHAR ucEsId);
extern BOOL fgIsH265PreGetFrmToDispQ(UCHAR ucEsId);
extern BOOL fgIsH265FrmBufReadyForDisp(UCHAR ucEsId);
extern BOOL fgIsH265PreInsToDispQ(UCHAR ucEsId);
extern BOOL fgIsH265SetFBufInfo(UCHAR ucEsId);
#endif

// vdec_drv_h265_parse.c
extern void vSeq_Par_Set_Rbsp(UINT32 u4BSID,UINT32 u4VDecID, H265_DRV_INFO_T *prH265DrvDecInfo);
extern INT32 vPic_Par_Set_Rbsp(UINT32 u4BSID, UINT32 u4VDecID, H265_DRV_INFO_T *prH265DrvDecInfo);
extern void vSEI_Rbsp(UINT32 u4BSID, UINT32 u4VDecID, H265_DRV_INFO_T  *prH265DrvDecInfo);
extern INT32 i4SlimParseSliceHeader(UINT32 u4VDecID, H265_DRV_INFO_T  *prH265DrvDecInfo);
extern INT32 i4ParseSliceHeader(UINT32 u4VDecID, H265_DRV_INFO_T  *prH265DrvDecInfo);

// vdec_drv_h265_decode.c
extern void vFlushBufInfo(H265_DRV_INFO_T *prH265DrvInfo);
extern void vFlushAllSetData(H265_DRV_INFO_T *prH265DrvInfo);
extern UCHAR ucH265GetMaxFBufNum(VDEC_INFO_H265_DEC_PRM_T *prVDecH265DecPrm, UINT32 u4PicWidth, UINT32 u4PicHeight, BOOL fgIsBDDisc);
extern void vPrepareFBufInfo(H265_DRV_INFO_T *prH265DrvInfo);
extern BOOL fgIsNonRefFbuf(VDEC_INFO_H265_FBUF_INFO_T *tFBufInfo);
extern void vSetPicRefType(UCHAR ucPicStruct, UCHAR ucRefType, VDEC_INFO_H265_FBUF_INFO_T *tFBufInfo);
extern void vAdapRefPicmarkingProce(UINT32 u4VDecID, H265_DRV_INFO_T *prH265DrvInfo);
extern void vSlidingWindowProce(H265_DRV_INFO_T *prH265DrvInfo);
extern UCHAR ucGetPicRefType(UCHAR ucPicStruct, VDEC_INFO_H265_FBUF_INFO_T *tFBufInfo);
extern UINT32 u4VDecGetMinPOCFBuf(VDEC_ES_INFO_T *prVDecEsInfo, H265_DPB_SEARCH_T eDPBSearchType);
extern void vFlushBufRefInfo(H265_DRV_INFO_T *prH265DrvInfo, BOOL fgIsForceFree);
#if (!defined(DRV_VDEC_VDP_RACING) && !defined(VDEC_PIP_WITH_ONE_HW))
extern UINT32 u4VDecOutputMinPOCFBuf(VDEC_ES_INFO_T *prVDecEsInfo, UINT32 u4MinPOCFBufIdx);
#else
extern UINT32 u4VDecOutputMinPOCFBuf(VDEC_ES_INFO_T *prVDecEsInfo, UINT32 u4MinPOCFBufIdx, BOOL fgFreeBuf, BOOL fgOutBase);
#endif
#ifdef FBM_ALLOC_SUPPORT
extern void vFreeH265WorkingArea(VDEC_ES_INFO_T *prVDecEsInfo);
#endif
extern void vVDecSetPicInfo(UINT32 u4VDecID, H265_DRV_INFO_T *prH265DrvInfo);
extern void vH265SetDownScaleParam(H265_DRV_INFO_T *prH265DrvInfo, BOOL fgEnable);
#if ((CONFIG_CHIP_VER_CURR >= CONFIG_CHIP_VER_MT8560) && CONFIG_DRV_FTS_SUPPORT)
extern void vH265SetLetterBoxParam(H265_DRV_INFO_T *prH265DrvInfo);
#endif
extern void vClrPicRefInfo(UCHAR ucPicStruct, H265_DRV_INFO_T *prH265DrvInfo, UINT32 u4FBufIdx);
extern void vH265ChkWhileLines(UCHAR ucEsId);
extern void vClrFBufInfo(H265_DRV_INFO_T *prH265DrvInfo, UINT32 u4FBufIdx);


void vHrdParameters(UINT32 u4BSID, UINT32 u4VDecID, H265_HRD_Data *tHrdPara);
void vInitSPS(H265_SPS_Data *prSPS);
void vInitSliceHdr(VDEC_INFO_H265_DEC_PRM_T *prVDecH265DecPrm);
void vRef_Pic_List_Reordering(UINT32 u4VDecID, H265_Slice_Hdr_Data *prSliceHdr);
void vDec_Ref_Pic_Marking(UINT32 u4VDecID, H265_Slice_Hdr_Data *prSliceHdr, BOOL fgIsIDRPic);
//void vHEVCInterpretBufferingPeriodInfo(UINT32 u4BSID,UINT32 u4VDecID, H265_DRV_INFO_T *prH265DrvDecInfo);
void vRecoveryPoint(UINT32 u4BSID,UINT32 u4VDecID, H265_DRV_INFO_T *prH265DrvDecInfo);
void vPicTiming(UINT32 u4BSID,UINT32 u4VDecID, H265_DRV_INFO_T *prH265DrvDecInfo);
//void vHEVCInterpretFilmGrainCharacteristicsInfo(UINT32 u4BSID,UINT32 u4VDecID, H265_DRV_INFO_T *prH265DrvDecInfo);
void vH265SetColorPrimaries(H265_DRV_INFO_T *prH265DrvDecInfo, UINT32 u4ColorPrimaries);
void vH265SetSampleAsp(H265_DRV_INFO_T *prH265DrvDecInfo, UINT32 u4H265Asp, UINT32 u4SarWidth, UINT32 u4SarHeight);
void vH265SetFrameTimingInfo(H265_DRV_INFO_T *prH265DrvDecInfo, UINT32 u4NumUnitsInTick, UINT32 u4TimeScale, BOOL fgIsFixFrm);
void vH265CCData(UINT32 u4BSID,UINT32 u4VDecID, H265_DRV_INFO_T *prH265DrvDecInfo);
void vH265xvYCCData(UINT32 u4BSID,UINT32 u4VDecID, H265_DRV_INFO_T *prH265DrvDecInfo);

extern BOOL fgVDecH265FreeFBuf(H265_DRV_INFO_T *prH265DrvInfo, UINT32 u4FBufIdx);

#ifdef VDEC_SR_SUPPORT
extern void vH265InitDFBList(H265_DRV_INFO_T *prH265DrvInfo);
extern BOOL fgH265UpdDFBList(H265_DRV_INFO_T * prH265DrvInfo);
extern void vH265UpdDFBListIdxInfo(H265_DRV_INFO_T *prH265DrvInfo, UCHAR ucDPBFbId, UCHAR ucH265FBUFListIdx);
extern UINT32 u4H265GetMaxPOCBFBuf(H265_DRV_INFO_T *prH265DrvInfo);
extern void vH265EDpbPutBuf(H265_DRV_INFO_T *prH265DrvInfo,  UCHAR ucSrcDpbBuf);
extern UINT32 u4H265SROutputMaxPOCFBuf(VDEC_ES_INFO_T *prVDecEsInfo, UINT32 u4MinPOCFBufIdx);
extern void vH265InitFBufInfo(VDEC_INFO_H265_FBUF_INFO_T *prH265FbufInfo);
extern UINT32 i4H265OutputProcSR(VDEC_ES_INFO_T *prVDecEsInfo, UINT32 u4MinPOCFBufIdx);
extern UINT32 u4VDecGetMaxPOCFBuf(VDEC_ES_INFO_T *prVDecEsInfo, H265_DPB_SEARCH_T eDPBSearchType);
extern void vH265UpdWorkingAreaInfo(H265_DRV_INFO_T *prH265TarDrvInfo, UCHAR ucEDPBSize);

#endif

#ifdef DRV_VDEC_VDP_RACING
extern void vH265SetDpbBufferInfo(UCHAR ucEsId);
extern void vH265SetFBufInfoOnly(UCHAR ucEsId);
extern void vVDecClearExternalBufInfo(H265_DRV_INFO_T *prH265DrvInfo, UINT32 u4FbIdx);
#endif

extern void vH265SetCurrFrameBufferInfo(UCHAR ucEsId);

#ifdef MPV_DUMP_FBUF
extern void VDec_Dump_Data(UINT32 u4StartAddr, UINT32 u4FileSize, UINT32 u4FileCnt, UCHAR* pucAddStr);
#endif

#if VDEC_MVC_SUPPORT
extern BOOL fgIsMVCIDR(H265_DRV_INFO_T *prH265DrvInfo);
extern void vPrefix_Nal_Unit_Rbsp(UINT32 u4BSID, UINT32 u4VDecID, H265_DRV_INFO_T *prH265DrvDecInfo);
extern void vSubset_Seq_Parameter_Set_Rbsp(UINT32 u4BSID, UINT32 u4VDecID, H265_DRV_INFO_T *prH265DrvDecInfo);
void vH265OffsetMetadata(UINT32 u4BSID,UINT32 u4VDecID, H265_DRV_INFO_T *prH265DrvDecInfo);
void vH265MVCScalableNesting(UINT32 u4BSID,UINT32 u4VDecID, H265_DRV_INFO_T *prH265DrvDecInfo);
#endif
#if MVC_PATCH_1
INT32 i4MVCHWPatch1(UINT32 u4BSID, UINT32 u4VDecID, VDEC_INFO_H265_BS_INIT_PRM_T *prH265BSInitPrm);
#endif

#ifdef DRV_VDEC_SUPPORT_FBM_OVERLAY
extern BOOL fgHEVCNeedDoDscl(UCHAR ucEsId);
#endif

//Get mw frame rate for h265-ts file. Jie Zhang@20100806
extern UINT32 u4H265CheckFrameRate(UCHAR ucEsId);
extern BOOL fgH265IsSlideShow(UCHAR ucEsId);
BOOL fgH265CheckIsDivXPlus( UINT32 u4BSID, UINT32 u4VDecID, UCHAR ucEsId);
#endif
