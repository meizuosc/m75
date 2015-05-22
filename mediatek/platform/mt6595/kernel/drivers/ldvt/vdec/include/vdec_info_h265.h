#ifndef _VDEC_INFO_H265_H_
#define _VDEC_INFO_H265_H_

//#include "drv_config.h"
//#include "drv_vdec.h"

#include "vdec_info_common.h"
#include "vdec_usage.h"
#define VDEC_MVC_SUPPORT 0

typedef struct _H265_PTLFlag_Data_{

  UINT32     u4ProfileSpace;
  BOOL       bTierFlag;
  UINT32     u4ProfileIdc;
  BOOL       bProfileCompatibilityFlag[32];
  UINT32     u4LevelIdc;

  BOOL     bProgressiveSourceFlag;
  BOOL     bInterlacedSourceFlag;
  BOOL     bNonPackedConstraintFlag;
  BOOL     bFrameOnlyConstraintFlag;
  
}H265_PTLFlag_Data, *pH265_PTLFlag_Data;

typedef struct _H265_PTL_Data_{

  BOOL     bProfilePresentFlag;
  H265_PTLFlag_Data   generalPTL;
  H265_PTLFlag_Data   subLayerPTL[6];    
  BOOL     bSubLayerProfilePresentFlag[6];
  BOOL     bSubLayerLevelPresentFlag[6];
  
}H265_PTL_Data, *pH265_PTL_Data;

#define SCALING_LIST_NUM 6         ///< list number for quantization matrix
#define SCALING_LIST_NUM_32x32 2   ///< list number for quantization matrix 32x32
#define SCALING_LIST_REM_NUM 6     ///< remainder of QP/6
#define SCALING_LIST_START_VALUE 8 ///< start value for dpcm mode
#define MAX_MATRIX_COEF_NUM 64     ///< max coefficient number for quantization matrix
#define MAX_MATRIX_SIZE_NUM 8      ///< max size number for quantization matrix
#define SCALING_LIST_DC 16         ///< default DC value

enum ScalingListDIR
{
  SCALING_LIST_SQT = 0,
  SCALING_LIST_VER,
  SCALING_LIST_HOR,
  SCALING_LIST_DIR_NUM
};
enum ScalingListSize
{
  SCALING_LIST_4x4 = 0,
  SCALING_LIST_8x8,
  SCALING_LIST_16x16,
  SCALING_LIST_32x32,
  SCALING_LIST_SIZE_NUM
};

typedef struct _H265_ScalingList_Data_{
    INT32      i4ScalingListDC[SCALING_LIST_SIZE_NUM][SCALING_LIST_NUM]; //!< the DC value of the matrix coefficient for 16x16
    BOOL     bScalingListPredModeFlag [SCALING_LIST_SIZE_NUM][SCALING_LIST_NUM];     
    UINT32    u4RefMatrixId[SCALING_LIST_SIZE_NUM][SCALING_LIST_NUM];   //!< RefMatrixID 
    INT32*     pScalingListDeltaCoef [SCALING_LIST_SIZE_NUM][SCALING_LIST_NUM]; //!< quantization matrix
    BOOL     bUseTransformSkip;


}H265_SL_Data, *pH265_SL_Data;


#define MAX_NUM_REF_PICS 32
typedef struct _H265_RPS_Data_{

  UINT32     u4NumberOfPictures;
  UINT32     u4NumberOfNegativePictures;
  UINT32     u4NumberOfPositivePictures;
  UINT32     u4NumberOfLongtermPictures;
  INT32       i4DeltaPOC[MAX_NUM_REF_PICS];
  UINT32     i4POC[MAX_NUM_REF_PICS];
  BOOL       bUsed[MAX_NUM_REF_PICS];
  BOOL       bInterRPSPrediction;
  UINT32     u4DeltaRIdxMinus1;   
  UINT32     u4DeltaRPS; 
  UINT32     u4NumRefIdc; 
  UINT32     u4RefIdc[17];
  BOOL       bCheckLTMSB[MAX_NUM_REF_PICS];
  UINT32     u4PocLSBLT[MAX_NUM_REF_PICS];
  UINT32     i4DeltaPOCMSBCycleLT[MAX_NUM_REF_PICS];
  BOOL       bDeltaPocMSBPresentFlag[MAX_NUM_REF_PICS];

}H265_RPS_Data, *pH265_RPS_Data;


#define MAX_CPB_CNT                     32  ///< Upper bound of (cpb_cnt_minus1 + 1)
#define MAX_TLAYER                  8           ///< max number of temporal layer

typedef struct  _H265_SUB_HRD_Data_   //HRD Sub Layer Info
{

    BOOL bFixedPicRateFlag;
    BOOL bFixedPicRateWithinCvsFlag;
    UINT32 u4ElementalDurationInTcMinus1;
    BOOL bLowDelayHrdFlag;
    UINT32 u4CpbCntMinus1;
    UINT32 u4BitRateValueMinus1[MAX_CPB_CNT][2];
    UINT32 u4CpbSizeValueMinus1[MAX_CPB_CNT][2];
    UINT32 u4DucpbSizeValueMinus1[MAX_CPB_CNT][2];
    UINT32 u4DuBitRateValueMinus1[MAX_CPB_CNT][2];
    BOOL bCbrFlag[MAX_CPB_CNT][2];

} H265_SUB_HRD_Data;


typedef struct  _H265_HRD_Data_   //HRD Sub Layer Info
{

    BOOL bNalHrdParametersPresentFlag;
    BOOL bVclHrdParametersPresentFlag;
    BOOL bSubPicCpbParamsPresentFlag;
    UINT32 u4TickDivisorMinus2;
    UINT32 u4DuCpbRemovalDelayLengthMinus1;
    BOOL bSubPicCpbParamsInPicTimingSEIFlag;
    UINT32 u4DpbOutputDelayDuLengthMinus1;

    UINT32 u4BitRateScale;
    UINT32 u4CpbSizeScale;
    UINT32 u4DucpbSizeScale;
    UINT32 u4InitialCpbRemovalDelayLengthMinus1;
    UINT32 u4AuCpbRemovalDelayLengthMinus1;
    UINT32 u4DpbOutputDelayLengthMinus1;
    H265_SUB_HRD_Data rSubLayerHRD[MAX_TLAYER];
    
} H265_HRD_Data;


typedef struct _H265_VUI_Data_
{
    BOOL bAspectRatioInfoPresentFlag;
    INT32  i4AspectRatioIdc;
    INT32  i4SarWidth;
    INT32  i4SarHeight;
    BOOL bOverscanInfoPresentFlag;
    BOOL bOverscanAppropriateFlag;
    BOOL bVideoSignalTypePresentFlag;
    INT32  i4VideoFormat;
    BOOL bVideoFullRangeFlag;
    BOOL bColourDescriptionPresentFlag;
    INT32  i4ColourPrimaries;
    INT32  i4TransferCharacteristics;
    INT32  i4MatrixCoefficients;
    BOOL bChromaLocInfoPresentFlag;
    INT32  i4ChromaSampleLocTypeTopField;
    INT32  i4ChromaSampleLocTypeBottomField;
    BOOL bNeutralChromaIndicationFlag;
    BOOL bFieldSeqFlag;

    //DefaultDisplayWindow
    BOOL bDefaultDisplayWindowEnabledFlag;
    INT32  i4DefaultDisplayWinLeftOffset;
    INT32  i4DefaultDisplayWinRightOffset;
    INT32  i4DefaultDisplayWinTopOffset;
    INT32  i4DefaultDisplayWinBottomOffset;

    BOOL bFrameFieldInfoPresentFlag;
    BOOL bHrdParametersPresentFlag;
    H265_HRD_Data rHdrParameters;
    
    BOOL bBitstreamRestrictionFlag;
    BOOL bTilesFixedStructureFlag;
    BOOL bMotionVectorsOverPicBoundariesFlag;
    BOOL bRestrictedRefPicListsFlag;
    INT32  i4MinSpatialSegmentationIdc;
    INT32  i4MaxBytesPerPicDenom;
    INT32  i4MaxBitsPerMinCuDenom;
    INT32  i4Log2MaxMvLengthHorizontal;
    INT32  i4Log2MaxMvLengthVertical;

     //TimingInfo
    BOOL bTimingInfoPresentFlag;
    UINT32 u4NumUnitsInTick;
    UINT32 u4TimeScale;
    BOOL bPocProportionalToTimingFlag;
    INT32  i4NumTicksPocDiffOneMinus1;

}H265_VUI_Data;


#define MAXnum_ref_frames_in_pic_order_cnt_cycle  256
typedef struct _H265_SPS_Data_ 
{

         BOOL     bSPSValid;                  // indicates the parameter set is valid
    
         UINT32    u4VPSId;                         // u(4)
         UINT32    u4MaxTLayersMinus1;  // u(3)
         BOOL      bTemporalIdNestingFlag;                   // u(1)
         H265_PTL_Data   rSPS_PTL;                    // ProfileTierLevel
        
         UINT32    u4SeqParameterSetId; // ue(v)
         UINT32    u4ChromaFormatIdc;    // ue(v)
         BOOL      bSeparateColourPlaneFlag;
         UINT32    u4PicWidthInLumaSamples;    // ue(v)
         UINT32    u4PicHeightInLumaSamples;    // ue(v)
        
         BOOL      bConformanceWindowFlag;              // u(1)
         UINT32    u4ConfWinLeftOffset;           // ue(v)
         UINT32    u4ConfWinRightOffset;         // ue(v)
         UINT32    u4ConfWinTopOffset;           // ue(v)
         UINT32    u4ConfWinBottomOffset;     // ue(v)
        
         UINT32    u4SubWidthC;
         UINT32    u4SubHeightC;
         UINT32    u4FrameCropLeftOffset;
         UINT32    u4FrameCropRightOffset;
         UINT32    u4FrameCropTopOffset;
         UINT32    u4FrameCropBottomOffset;
        
         UINT32    u4BitDepthLumaMinus8;             // ue(v)
         UINT32    u4BitDepthChromaMinus8;        // ue(v)
         UINT32    u4QpBDOffsetY;
         UINT32    u4QpBDOffsetC;                   // ue(v)   
        
         UINT32    u4Log2MaxPicOrderCntLsbMinus4;              // ue(v)
        
        //[notice] spec conflict u4MaxDecPicBufferingMinux1? u4MaxLatencyIncreasePlus1?
         UINT32    u4MaxDecPicBuffering[8];        // ue(v)   
         UINT32    u4NumReorderPics[8];             // ue(v)   
         UINT32    u4MaxLatencyIncrease[8];       // ue(v)   
        
        
         UINT32    u4Log2MinCodingBlockSizeMinus3;     // ue(v)   
         UINT32    u4Log2DiffMaxMinCodingBlockSize;    // ue(v)   
         UINT32    u4MaxCUWidth;
         UINT32    u4MaxCUHeight;
        
         UINT32    u4Log2MinTransformBlockSizeMinus2;      // ue(v)   
         UINT32    u4Log2DiffMaxMinTtransformBlockSize;     // ue(v)   
         UINT32    u4QuadtreeTUMaxDepthInter;      // ue(v)   
         UINT32    u4QuadtreeTUMaxDepthIntra;     // ue(v)   
         UINT32    u4MaxTrSize;
         UINT32    u4MaxCUDepth;
        
         BOOL      bScalingListFlag;                                 // u(1)
         BOOL      bScalingListPresentFlag;                   // u(1)
         BOOL      bSL_Init;
         H265_SL_Data     rSPS_ScalingList;                               //ScalingList
         
         BOOL      bUseAMP;                                           // u(1)
         BOOL      bUseSAO;                                           // u(1)
         BOOL      bUsePCM;                                            // u(1)
         
         UINT32    u4PCMBitDepthLumaMinus1;          // u(4)
         UINT32    u4PCMBitDepthChromaMinus1;     // u(4)
         UINT32    u4PCMLog2LumaMinSizeMinus3;      // ue(v)   
         UINT32    u4PCMLog2LumaMaxSize;                  // ue(v)   
         BOOL      bPCMFilterDisableFlag;                      // u(1)
         
         UINT32    u4NumShortTermRefPicSets;            // ue(v)   
         pH265_RPS_Data  pShortTermRefPicSets[64];            //RPS pointer array
         
         BOOL      bLongTermRefsPresent;                // u(1)
         UINT32    u4NumLongTermRefPicSPS;        // ue(v)   
         UINT32    u4LtRefPicPocLsbSps[33];                // u(BitsForPOC)   
         BOOL      bUsedByCurrPicLtSPSFlag[33];       // u(1)
         UINT32     u4NumRefFrames;

         BOOL      bTMVPFlagsPresent;                      // u(1)
         BOOL      bUseStrongIntraSmoothing;            // u(1)
         BOOL      bVuiParametersPresentFlag;         // u(1)
         H265_VUI_Data rVUI;                  // vui_seq_parameters_t
         BOOL      bSPSExtensionFlag;                      // u(1)
         // read sps_extension_data_flag
          
}H265_SPS_Data, *pH265_SPS_Data;


#define MAX_TILES_WITTH_HEIGHT 64

typedef struct _H265_PPS_Data_
{

    BOOL    bPPSValid;                  // indicates the parameter set is valid
    UINT32    u4PicParameterSetId;                                     // ue(v)
    UINT32    u4SeqParameterSetId;                                  // ue(v)
  

    BOOL      bDependentSliceSegmentsEnabledFlag;   // u(1)
    BOOL      bOutputFlagPresentFlag;                             // u(1)
    UINT32    u4NumExtraSliceHeaderBits;                       // u(3)
    BOOL      bSignHideFlag;                                           // u(1)
    BOOL      bCabacInitPresentFlag;                             // u(1)
    UINT32    u4NumRefIdxL0DefaultActiveMinus1;         // ue(v)
    UINT32    u4NumRefIdxL1DefaultActiveMinus1;         // ue(v)
    INT32      i4PicInitQPMinus26;                                    //se(v)
    BOOL      bConstrainedIntraPredFlag;                        // u(1)
    BOOL      bTransformSkipEnabledFlag;                     // u(1)
    BOOL      bCuQPDeltaEnabledFlag;                          // u(1)

    UINT32    u4DiffCuQPDeltaDepth;                  // ue(v)
    INT32      i4PPSCbQPOffset;                          //se(v)    
    INT32      i4PPSCrQPOffset;                            //se(v)

    BOOL      bPPSSliceChromaQpFlag;               // u(1)
    BOOL      bWPPredFlag;                                     // u(1)
    BOOL      bWPBiPredFlag;                                 // u(1)
    BOOL      bTransquantBypassEnableFlag;        // u(1)
    BOOL      bTilesEnabledFlag;                              // u(1)
    BOOL      bEntropyCodingSyncEnabledFlag;    // u(1)

    // if bTilesEnabledFlag
    UINT32    u4NumColumnsMinus1;                       // ue(v)
    UINT32    u4NumRowsMinus1;                             // ue(v)
    BOOL      bUniformSpacingFlag;                         // u(1)
    UINT32    u4ColumnWidthMinus1[MAX_TILES_WITTH_HEIGHT];   // ue(v)
    UINT32    u4RowHeightMinus1[MAX_TILES_WITTH_HEIGHT];       // ue(v)
    BOOL      bLoopFilterAcrossTilesEnabledFlag;              // u(1)

    BOOL      bLoopFilterAcrossSlicesEnabledFlag;             // u(1)
    BOOL      bDeblockingFilterControlPresentFlag;              // u(1)

    BOOL      bDeblockingFilterOverrideEnabledFlag;           // u(1)
    BOOL      bPicDisableDeblockingFilterFlag;                     // u(1)
    
    INT32      i4DeblockingFilterBetaOffsetDiv2;                    //se(v)    
    INT32      i4DeblockingFilterTcOffsetDiv2;                        //se(v)

    BOOL      bPPSScalingListPresentFlag;                   // u(1)
    BOOL      bSL_Init;
    H265_SL_Data     rPPS_ScalingList;                           //ScalingList
    BOOL      bListsModificationPresentFlag;                 // u(1)
    UINT32     u4Log2ParallelMergeLevelMinus2;           // ue(v)
    BOOL      bSliceHeaderExtensionPresentFlag;         // u(1)
    BOOL      bPPSExtensionFlag;                      // u(1)

} H265_PPS_Data, *pH265_PPS_Data;


typedef struct _H265_Slice_Hdr_Data_
{
    BOOL     bFirstSliceSegmentInPic;
    UINT32    u4PPSID;   
    BOOL     bDependentSliceSegmentFlag;
    UINT32    u4SliceSegmentAddress;
    UINT32    u4SliceType;
    UINT32    u4NalType;
    BOOL     bPicOutputFlag;
    UINT32    u4ColourPlaneID;

    INT32      i4POCMsb; 
    INT32      i4POCLsb;
    INT32      i4POC;
    H265_RPS_Data  rLocalRPS;
    pH265_RPS_Data  pShortTermRefPicSets;            

    UINT32     u4NumOfLongTermSPS;
    UINT32     u4NumOfLongTermPics;

    BOOL     bNumRefIdxActiveOverrideFlag;
    BOOL     bTMVPFlagsPresent;
    BOOL     bSaoEnabledFlag;
    BOOL     bSaoEnabledFlagChroma;

    INT32      i4NumRefIdx[3];    //  for multiple reference of current slice
    BOOL     bRefPicListModificationFlagL0;
    BOOL     bMvdL1ZeroFlag;
    BOOL     bCabacInitFlag;
    BOOL     bColFromL0Flag;
    UINT32    u4ColRefIdx;
    UINT32    u4FiveMinusMaxNumMergeCand;

    INT32     i4SliceQp;
    INT32     i4SliceQpDeltaCb;
    INT32     i4SliceQpDeltaCr;

    BOOL     bDeblockingFilterOverrideFlag;
    BOOL     bDeblockingFilterDisableFlag;
    INT32     i4DeblockingFilterBetaOffsetDiv2;
    INT32     i4DeblockingFilterTcOffsetDiv2;
    BOOL     bLoopFilterAcrossSlicesEnabledFlag;
    UINT32   u4SliceHeaderExtensionLength;

    UINT32    u4NumEntryPointOffsets;
    UINT32    u4OffsetLenMinus1;
    BOOL      bNoRaslOutputFlag;

}H265_Slice_Hdr_Data, *pH265_Slice_Hdr_Data;


typedef struct _H265_SEI_Data_
{

}H265_SEI_Data;


typedef struct _VDEC_INFO_H265_TILE_INFO_T
{
    UINT32      u4TileWidth;
    UINT32      u4TileHeight;
    UINT32      u4RightEdgePosInCU;
    UINT32      u4BottomEdgePosInCU;
    UINT32      u4FirstCUAddr;
    
} VDEC_INFO_H265_TILE_INFO_T;

#define TILE_MAX_NUM  256
typedef struct _VDEC_INFO_H265_PIC_INFO_T_
{
    INT32    bLowDelayFlag; 
    INT32    i4PocDiffList0[16]; 
    INT32    i4PocDiffList1[16]; 
    INT32    i4LongTermList0[16]; 
    INT32    i4LongTermList1[16]; 
    INT32    i4BuffIdList0[16];
    INT32    i4BuffIdList1[16];
    INT32    i4RefListNum;
    INT32    i4List0DecOrderNo[16];
    INT32    i4List1DecOrderNo[16];

    INT32    i4DpbLTBuffCnt;
    INT32    i4DpbLTBuffId[16];

    //RPS info
    INT32   i4StrNumDeltaPocs;
    INT32   i4MaxStrNumNegPosPics;
    INT32   i4StrNumNegPosPics;
    INT32   i4NumLongTerm;
    INT32   i4NumLongTermSps;

    UINT32  u4PicWidthInCU;
    UINT32  u4PicHeightInCU;
    VDEC_INFO_H265_TILE_INFO_T rTileInfo[TILE_MAX_NUM];

    UINT32   u4SliceCnt;
    UINT32   u4IqSramAddrAccCnt;
    
} VDEC_INFO_H265_PIC_INFO_T;


typedef struct _VDEC_INFO_H265_LAST_INFO_T_
{
    BOOL    fgLastMmco5;
    UCHAR ucLastNalUnitType;
    UCHAR  ucLastPicStruct;
    UCHAR  ucLastSPSId;
    UCHAR  ucLastSPSLevel;
    INT32   i4LastPOC;  
    INT32   i4LastRefPOC;  
    INT32   i4LastRefPOCMsb;
    INT32   i4LastRefPOCLsb;  
    INT32   i4LastFrameNumOffset;  
    UINT32 u4LastFrameNum; 
    UINT32 u4LastPicW;
    UINT32 u4LastPicH;  

}VDEC_INFO_H265_LAST_INFO_T;

typedef enum
{
    H265_DPB_STATUS_EMPTY = 0,   // Free
    H265_DPB_STATUS_READY,         // After Get          
    H265_DPB_STATUS_DECODING,   // After Lock                
    H265_DPB_STATUS_DECODED,     // After UnLock
    H265_DPB_STATUS_OUTPUTTED,     // After Output
    H265_DPB_STATUS_FLD_DECODED,   // After 1fld UnLock
    H265_DPB_STATUS_DEC_REF,     // LOCK for decoded but ref needed
    H265_DPB_STATUS_FLD_DEC_REF,     // LOCK for decoded but ref needed
    H265_DPB_STATUS_OUT_REF,     // LOCK for outputted but ref needed
#ifdef DRV_VDEC_VDP_RACING
    H265_DPB_STATUS_OUT_DECODING,   // After Lock
    H265_DPB_STATUS_OUT_FLD_DEC,
#endif
}H265_DPB_COND_T;

typedef struct _VDEC_INFO_H265_FBUF_INFO_T_
{ 
    H265_DPB_COND_T eH265DpbStatus;
    
    UCHAR   ucFBufStatus;
    UCHAR   ucFBufRefType;

    INT32    i4POC;
    UINT32  u4PicCnt;
    UINT32  u4POCBits;
    BOOL   bLtMsbPresentFlag;
    BOOL   bFirstSliceReferenced;
     
    UINT32  u4YStartAddr;
    UINT32  u4CAddrOffset;
    UINT32  u4MvStartAddr;
    
    // for UFO Mode
    UINT32  u4YLenStartAddr;
    UINT32  u4CLenStartAddr;
    UINT32  u4UFOLenYsize;
    UINT32  u4UFOLenCsize;
    UINT32  u4PicSizeBS;
    UINT32  u4PicSizeYBS;
    BOOL  bIsUFOEncoded;

    #if 1//(CONFIG_DRV_VERIFY_SUPPORT)
    UINT32  u4W;
    UINT32  u4H;  
    UINT32  u4DecOrder;
    UINT32  u4DramPicSize;  
    UINT32  u4DramPicArea; 
    UINT32  u4DramMvSize;
    UINT32  u4Addr;  // change name to u4YStartAddr   TODO:071021
    #endif
    BOOL  bUsedByCurr;
    BOOL  bUsedAsLongTerm;


}VDEC_INFO_H265_FBUF_INFO_T;

#define H265_MAX_PIC_LIST_NUM 32
typedef struct _VDEC_INFO_H265_REF_PIC_LIST_T_
{  
    UINT32  u4RefPicCnt;
    UINT32  u4FBufIdx[H265_MAX_PIC_LIST_NUM];
    
}VDEC_INFO_H265_REF_PIC_LIST_T;


typedef struct _VDEC_INFO_H265_BS_INIT_PRM_T_
{
    UINT32  u4VLDRdPtr;
    UINT32  u4VLDWrPtr;
    UINT32  u4VFifoSa;                 ///< Video Fifo memory start address    
    UINT32  u4VFifoEa;                 ///< Video Fifo memory end address
    UINT32  u4PredSa;
}VDEC_INFO_H265_BS_INIT_PRM_T;


typedef struct _VDEC_INFO_H265_INIT_PRM_T_
{
    UINT32  u4FGSeedbase;
    UINT32  u4CompModelValue; 
    UINT32  u4FGDatabase;
}VDEC_INFO_H265_INIT_PRM_T;


typedef struct _VDEC_INFO_H265_DEC_PRM_T_
{    

    UINT32    u4NuhTemporalId;
    UCHAR   ucMaxFBufNum;
    // Decode picture setting

    INT32    i4RAPOC;
    INT32    i4PrePOC;
    INT32     i4PrevT0POCLsb;
    INT32     i4PrevT0POCMsb;
    BOOL     fgUserScalingMatrixPresentFlag;                   // u(1)
    BOOL     fgUserScalingListPresentFlag[8];                   // u(1)
    BOOL     bFirstSliceInSequence;

    H265_SPS_Data *prSPS;
    H265_PPS_Data *prPPS;
    H265_Slice_Hdr_Data *prSliceHdr;  
    H265_SEI_Data *prSEI;
    VDEC_INFO_H265_LAST_INFO_T rLastInfo;
    VDEC_INFO_H265_FBUF_INFO_T *prCurrFBufInfo;

    // For UFO mode verification
    BOOL   bIsUFOMode;
    UINT32   u4RefUFOEncoded;

    // For Error concealment
    UCHAR  ucPreFBIndex;
    BOOL bNoDecode;

    BOOL  fgIsReduceMVBuffer;

}VDEC_INFO_H265_DEC_PRM_T;



#endif //#ifndef _HAL_VDEC_H265_IF_H_

