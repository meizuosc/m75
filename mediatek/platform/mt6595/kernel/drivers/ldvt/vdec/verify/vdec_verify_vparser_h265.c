#include "vdec_verify_mpv_prov.h"
#include "../hal/vdec_hal_if_common.h"
#include "../hal/vdec_hal_if_mpeg.h"
#include "../hal/vdec_hal_if_wmv.h"
#include "../hal/vdec_hal_if_h265.h"
#include "vdec_verify_file_common.h"
#include "vdec_verify_filesetting.h"
#include "vdec_verify_vparser_h265.h"
#include "vdec_verify_common.h"
//#include "x_debug.h"

#include <linux/string.h>
#include <linux/vmalloc.h>
#if (!CONFIG_DRV_LINUX)
#include <string.h>
#include <stdio.h>
#include <math.h>
#endif

// Morris Yang 20131128, prevent kernel stack overflow
#define STACK_FRAME_LIMIT
#ifdef STACK_FRAME_LIMIT
static UINT32 u4NalType[500];
static UINT32 u4NalStartOffset[500];
#endif

UINT32 vHEVCParseSliceHeader(UINT32 u4InstID);  
UINT32 vHEVCVerifySPS_Rbsp(UINT32 u4InstID);
UINT32 vHEVCVerifyPPS_Rbsp(UINT32 u4InstID);
void vHEVCVerifySEI_Rbsp(UINT32 u4InstID);
void vHEVCInterpretFilmGrainCharacteristicsInfo(UINT32 u4InstID, VDEC_INFO_DEC_PRM_T *ptVerMpvDecPrm);
void vHEVCVerifyRef_Pic_List_Modification(UINT32 u4InstID, H265_Slice_Hdr_Data *prSliceHdr);
void vHEVCVerifyInitPicInfo(UINT32 u4InstID);
void vHEVCVerifyInitSPS(H265_SPS_Data *prSPS);
void vHEVCVerifyInitTilesInfo(UINT32 u4VDecID, H265_PPS_Data *prPPS);
void vHEVCVerifyInitSliceHdr(UINT32 u4InstID, VDEC_INFO_DEC_PRM_T *tVerMpvDecPrm);
UINT32 vHEVCVDecSetRefPicList(UINT32 u4InstID, VDEC_INFO_DEC_PRM_T *tVerMpvDecPrm);
void vHEVCVDecApplyRefPicList(UINT32 u4InstID, VDEC_INFO_DEC_PRM_T *tVerMpvDecPrm);

INT32 * vHEVCVerifyGetSLDefaultAddress(INT32 sizeId, INT32 listId);
BOOL fgHEVCChkRefInfo(UINT32 u4InstID, UINT32 u4FBufIdx, UINT32 u4RefType);



const UINT32 H265_scalingListSize   [4] = {16,64,256,1024}; 
const UINT32 H265_scalingListSizeX  [4] = { 4, 8, 16,  32};
const int  H265_eTTable[4] = {0,3,1,2};
const UINT32 H265_scalingListNum[SCALING_LIST_SIZE_NUM]={6,6,6,2};
const int  H265_invQuantScales[6] =
{
  40,45,51,57,64,72
};

INT32 H265_quantTSDefault4x4[16] =
{
  16,16,16,16,
  16,16,16,16,
  16,16,16,16,
  16,16,16,16
};

INT32 H265_quantIntraDefault8x8[64] =
{
  16,16,16,16,17,18,21,24,
  16,16,16,16,17,19,22,25,
  16,16,17,18,20,22,25,29,
  16,16,18,21,24,27,31,36,
  17,17,20,24,30,35,41,47,
  18,19,22,27,35,44,54,65,
  21,22,25,31,41,54,70,88,
  24,25,29,36,47,65,88,115
};

INT32 H265_quantInterDefault8x8[64] =
{
  16,16,16,16,17,18,20,24,
  16,16,16,17,18,20,24,25,
  16,16,17,18,20,24,25,28,
  16,17,18,20,24,25,28,33,
  17,18,20,24,25,28,33,41,
  18,20,24,25,28,33,41,54,
  20,24,25,28,33,41,54,71,
  24,25,28,33,41,54,71,91
};


/// coefficient scanning type used in ACS
enum COEFF_SCAN_TYPE
{
  SCAN_DIAG = 0,         ///< up-right diagonal scan
  SCAN_HOR,              ///< horizontal first scan
  SCAN_VER               ///< vertical first scan
};

// scanning order table
#define     MAX_CU_DEPTH            7                           // log2(LCUSize)
#define     MAX_CU_SIZE             (1<<(MAX_CU_DEPTH))         // maximum allowable size of CU

INT32  H265_aucConvertToBit  [ MAX_CU_SIZE+1 ];
UINT32* H265_auiSigLastScan[ 3 ][ MAX_CU_DEPTH ] = {NULL};
const UINT32 H265_sigLastScan8x8[ 3 ][ 4 ] =
{
  {0, 2, 1, 3},
  {0, 1, 2, 3},
  {0, 2, 1, 3}
};

UINT32 H265_sigLastScanCG32x32[ 64 ];


// *********************************************************************
// Function    : void vHEVCErrInfo(UINT32 u4Type)
// Description : error handler
// Parameter   : None
// Return      : None
// *********************************************************************
void vHEVCErrInfo(UINT32 u4Type)
{
  switch(u4Type)
  {
    case OUT_OF_FILE:
      break;
    case VER_FORBIDEN_ERR:
      break;
    case DEC_INIT_FAILED:
      break;
    default:
      break;
  }
}



// *********************************************************************
// Function    :void vHEVCCreateLostPicture(UINT32 u4InstID, INT32 i4LostPoc)
// Description :  Function for Create Lost Picture.
// Parameter   : None
// Return      : None
// *********************************************************************
void vHEVCCreateLostPicture(UINT32 u4InstID, INT32 i4LostPoc){

    int j, closestPoc;
    H265_Slice_Hdr_Data* pCurrSliceHdr;
    pCurrSliceHdr = _tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.prSliceHdr;

    closestPoc = MAX_INT;
    for( j=0; j<_tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.ucMaxFBufNum; j++) {
        if ( abs(_ptH265FBufInfo[u4InstID][j].i4POC-i4LostPoc)<closestPoc && abs(_ptH265FBufInfo[u4InstID][j].i4POC-i4LostPoc)!=0 &&  _ptH265FBufInfo[u4InstID][j].i4POC!=pCurrSliceHdr->i4POC )
            closestPoc = abs(_ptH265FBufInfo[u4InstID][j].i4POC-i4LostPoc);
    }
    
    for( j=0; j<_tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.ucMaxFBufNum; j++) {
        if ( abs(_ptH265FBufInfo[u4InstID][j].i4POC-i4LostPoc)==closestPoc &&  _ptH265FBufInfo[u4InstID][j].i4POC!=pCurrSliceHdr->i4POC ){
            UINT32 u4LostBFindex = 0;

            printk ("[INFO] Copying picture %d to losted %d (%d).\n", _ptH265FBufInfo[u4InstID][j].i4POC, i4LostPoc, pCurrSliceHdr->i4POC);
            u4LostBFindex = vHEVCAllocateFBuf(u4InstID, &_tVerMpvDecPrm[u4InstID], 0);
            //memcpy(_ptH265FBufInfo[u4InstID][u4LostBFindex].u4Addr, _ptH265FBufInfo[u4InstID][j].u4Addr,  _ptH265FBufInfo[u4InstID][j].u4DramPicArea);
            _ptH265FBufInfo[u4InstID][u4LostBFindex] = _ptH265FBufInfo[u4InstID][j];

            _ptH265FBufInfo[u4InstID][u4LostBFindex].i4POC = i4LostPoc;
            _ptH265FBufInfo[u4InstID][u4LostBFindex].ucFBufRefType = SREF_PIC;
            _ptH265FBufInfo[u4InstID][u4LostBFindex].ucFBufStatus = FRAME;
            _ptH265FBufInfo[u4InstID][u4LostBFindex].bFirstSliceReferenced = 1;
            _ptH265FBufInfo[u4InstID][u4LostBFindex].bUsedAsLongTerm = 0;
            if (_tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.i4RAPOC==MAX_INT){
                _tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.i4RAPOC = u4LostBFindex;
            }
            break;
        }
    }

}

// *********************************************************************
// Function    : INT32 vHEVCCheckAllRefPicsAreAvailable(UINT32 u4InstID, VDEC_INFO_DEC_PRM_T *tVerMpvDecPrm)
// Description :  Function for check the pictures in the Reference Picture Set are available.
// Parameter   : None
// Return      : INT32
// *********************************************************************
INT32 vHEVCCheckAllRefPicsAreAvailable(UINT32 u4InstID, VDEC_INFO_DEC_PRM_T *tVerMpvDecPrm)
{
    int i, isAvailable, j;
    int atLeastOneLost = 0;
    int atLeastOneRemoved = 0;
    INT32 iPocLost = 0;
    H265_Slice_Hdr_Data* pCurrSliceHdr;
    pH265_RPS_Data pCurrRPS;
    
#if DEBUG_LEVEL >= DBG_LEVEL_INFO
              printk ("[INFO] vHEVCCheckAllRefPicsAreAvailable() start!!\n");
#endif
    
    pCurrSliceHdr = tVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSliceHdr;
    pCurrRPS = pCurrSliceHdr->pShortTermRefPicSets;

    // loop through all long-term pictures in the Reference Picture Set
    // to see if the picture should be kept as reference picture
    i = 0;
    for( i = pCurrRPS->u4NumberOfNegativePictures+pCurrRPS->u4NumberOfPositivePictures; i< pCurrRPS->u4NumberOfPictures; i++)
    {
        if (i>=MAX_NUM_REF_PICS){ break; }
        j = 0;
        isAvailable = 0;
        // loop through all pictures in the reference picture buffer
        for( j=0; j<_tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.ucMaxFBufNum; j++) {
/*
#if DEBUG_LEVEL >= DBG_LEVEL_INFO
            printk ("[INFO]     FB index:%d  pCurrRPS->i4POC[%d] = %d\n", j, i ,pCurrRPS->i4POC[i]);
            printk ("[INFO]         ucFBufStatus %d; FB i4POC: %d;\n", _ptH265FBufInfo[u4InstID][j].ucFBufStatus, _ptH265FBufInfo[u4InstID][j].i4POC);
            printk ("[INFO]         FB bFirstSliceReferenced: %d\n", _ptH265FBufInfo[u4InstID][j].bFirstSliceReferenced);
#endif
*/           
            if (_ptH265FBufInfo[u4InstID][j].ucFBufStatus == NO_PIC){
                continue;
            }
            if( 1 == pCurrRPS->bCheckLTMSB[i] ) {
                if(( _ptH265FBufInfo[u4InstID][j].bUsedAsLongTerm) && _ptH265FBufInfo[u4InstID][j].i4POC == pCurrRPS->i4POC[i] )
                    isAvailable = 1;
            } else {
                if(( _ptH265FBufInfo[u4InstID][j].bUsedAsLongTerm) && (_ptH265FBufInfo[u4InstID][j].i4POC%(1<<_ptH265FBufInfo[u4InstID][j].u4POCBits)) == pCurrRPS->i4POC[i]%(1<<_ptH265FBufInfo[u4InstID][j].u4POCBits) )
                    isAvailable = 1;
            }
        } 
        // if there was no such long-term check the short terms
        if(!isAvailable) {
            for( j=0; j<_tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.ucMaxFBufNum; j++) {
                if (_ptH265FBufInfo[u4InstID][j].ucFBufStatus == NO_PIC){
                    continue;
                }     
                if( 1 == pCurrRPS->bCheckLTMSB[i] ) {
                    if ( (_ptH265FBufInfo[u4InstID][j].i4POC == pCurrRPS->i4POC[i]) && _ptH265FBufInfo[u4InstID][j].bFirstSliceReferenced ){
                        isAvailable = 1;
                        _ptH265FBufInfo[u4InstID][j].ucFBufRefType = LREF_PIC;
                        _ptH265FBufInfo[u4InstID][j].bUsedAsLongTerm = 1;
                        //printk("[DEBUG] FB index %d pCurrRPS->i4POC[%d] = %d mark as LT\n", j, i,pCurrRPS->i4POC[i]);
                        break;
                    }   
                } else {
                    if ( ((_ptH265FBufInfo[u4InstID][j].i4POC%(1<<_ptH265FBufInfo[u4InstID][j].u4POCBits)) == (pCurrRPS->i4POC[i]%(1<<_ptH265FBufInfo[u4InstID][j].u4POCBits))) && _ptH265FBufInfo[u4InstID][j].bFirstSliceReferenced){
                        isAvailable = 1;
                        _ptH265FBufInfo[u4InstID][j].ucFBufRefType = LREF_PIC;
                        _ptH265FBufInfo[u4InstID][j].bUsedAsLongTerm = 1;        
                        //printk("[DEBUG] FB index %d pCurrRPS->i4POC[%d] = %d mark as LT\n", j, i,pCurrRPS->i4POC[i]);
                        break;
                    }
                }
            }
        }

        // report that a picture is lost if it is in the Reference Picture Set
        // but not available as reference picture
        if(isAvailable == 0) {
            if ( pCurrSliceHdr->i4POC + pCurrRPS->i4DeltaPOC[i] >=_tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.i4RAPOC ){
                if ( !pCurrRPS->bUsed[i] ) {
                    printk ("\n[WARNING] Long-term reference picture with POC = %3d seems to have been removed or not correctly decoded!!\n\n",  pCurrSliceHdr->i4POC + pCurrRPS->i4DeltaPOC[i] );
                    atLeastOneRemoved = 1;
                } else {
                    printk ("\n[WARNING] Long-term reference picture with POC = %3d is lost or not correctly decoded!!\n\n",  pCurrSliceHdr->i4POC + pCurrRPS->i4DeltaPOC[i] );
                    atLeastOneLost = 1;
                    iPocLost=pCurrSliceHdr->i4POC + pCurrRPS->i4DeltaPOC[i];
                }
            }
        }
    }

    // loop through all short-term pictures in the Reference Picture Set
    // to see if the picture should be kept as reference picture
    for( i = 0; i < pCurrRPS->u4NumberOfNegativePictures+pCurrRPS->u4NumberOfPositivePictures ; i++)
    {
        if (i>=MAX_NUM_REF_PICS){ break; }
        isAvailable = 0;
        // loop through all pictures in the reference picture buffer
        for( j=0; j<_tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.ucMaxFBufNum; j++) {
            if (_ptH265FBufInfo[u4InstID][j].ucFBufStatus == NO_PIC){
                continue;
            }
            if ( (!(_ptH265FBufInfo[u4InstID][j].bUsedAsLongTerm)) && _ptH265FBufInfo[u4InstID][j].i4POC==(pCurrSliceHdr->i4POC+pCurrRPS->i4DeltaPOC[i]) && _ptH265FBufInfo[u4InstID][j].bFirstSliceReferenced){
                isAvailable = 1;
        }
        }
        // report that a picture is lost if it is in the Reference Picture Set
        // but not available as reference picture
        if(isAvailable == 0){
            if ( pCurrSliceHdr->i4POC + pCurrRPS->i4DeltaPOC[i] >=_tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.i4RAPOC ){
                if ( !pCurrRPS->bUsed[i] ) {
                    printk ("\n[WARNING] Short-term reference picture with POC = %3d seems to have been removed or not correctly decoded!!\n\n",  pCurrSliceHdr->i4POC + pCurrRPS->i4DeltaPOC[i] );
                    atLeastOneRemoved = 1;
                } else {
                    printk ("\n[WARNING] Short-term reference picture with POC = %3d is lost or not correctly decoded!!\n\n",  pCurrSliceHdr->i4POC + pCurrRPS->i4DeltaPOC[i] );
                    atLeastOneRemoved = 1;
                    atLeastOneLost = 1;
                    iPocLost=pCurrSliceHdr->i4POC + pCurrRPS->i4DeltaPOC[i];
                }
            }
        }

    }

#if DEBUG_LEVEL >= DBG_LEVEL_INFO
              printk ("[INFO] vHEVCCheckAllRefPicsAreAvailable() Done!!\n");
#endif
    if(atLeastOneLost) {
        printk ("[INFO] vHEVCCheckAllRefPicsAreAvailable() return lost iPocLost %d!!\n", iPocLost);
        return (iPocLost+1);
    }
    
    if(atLeastOneRemoved) {
        return -2;
    } else {
        return 0;
    }

}


// *********************************************************************
// Function    : void vHEVCRefreshMarking(UINT32 u4Type)
// Description : error handler
// Parameter   : None
// Return      : None
// *********************************************************************
void vHEVCRefreshMarking(UINT32 u4InstID, UINT32 u4NalType, INT32 i4CurrPOC)
{
    int i;

    if ( u4NalType == NAL_UNIT_CODED_SLICE_BLA
    || u4NalType == NAL_UNIT_CODED_SLICE_BLANT
    || u4NalType == NAL_UNIT_CODED_SLICE_BLA_N_LP
    || u4NalType == NAL_UNIT_CODED_SLICE_IDR
    || u4NalType == NAL_UNIT_CODED_SLICE_IDR_N_LP )  // IDR or BLA picture
    {
        // mark all pictures as not used for reference
        for(i=0; i<_tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.ucMaxFBufNum; i++)
        {
            vVerifyClrFBufInfo(u4InstID, i );
        }     
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            printk ("[INFO] IDR or BLA clear all DPB !!\n", i , _ptH265FBufInfo[u4InstID][i].i4POC);
        #endif        
    }
    
}


// *********************************************************************
// Function    : UINT32   vHEVCPrepareRefPiclist(UINT32 u4InstID, VDEC_INFO_DEC_PRM_T *tVerMpvDecPrm)
// Description : check pic type to send P_0, B_0, B_1
// Parameter   : None
// Return      : UINT32
// *********************************************************************
UINT32 vHEVCPrepareRefPiclist(UINT32 u4InstID, VDEC_INFO_DEC_PRM_T *tVerMpvDecPrm)
{
    INT32 i4LostPoc;
    INT32 i=0;
    H265_Slice_Hdr_Data* pCurrSliceHdr;
    
    pCurrSliceHdr = _tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.prSliceHdr;

    if ( _u4PicCnt[u4InstID] == 0) // start of random access point, m_pocRandomAccess has not been set yet.
    {
        if (   _ucNalUnitType[u4InstID] == NAL_UNIT_CODED_SLICE_CRA
        || _ucNalUnitType[u4InstID] == NAL_UNIT_CODED_SLICE_BLANT
        || _ucNalUnitType[u4InstID] == NAL_UNIT_CODED_SLICE_BLA_N_LP
        || _ucNalUnitType[u4InstID] == NAL_UNIT_CODED_SLICE_BLA )
        {
            // set the POC random access since we need to skip the reordered pictures in the case of CRA/CRANT/BLA/BLANT.
            _tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.i4RAPOC = _tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.prSliceHdr->i4POC;
        }
        else if ( _ucNalUnitType[u4InstID] == NAL_UNIT_CODED_SLICE_IDR || _ucNalUnitType[u4InstID] == NAL_UNIT_CODED_SLICE_IDR_N_LP )
        {
            _tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.i4RAPOC = -MAX_INT; // no need to skip the reordered pictures in IDR, they are decodable.
        }
        printk ("[INFO] i4RAPOC %d  _ucNalUnitType %d!!\n",  _tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.i4RAPOC, _ucNalUnitType[u4InstID] );
        
    }
    while ((i4LostPoc = vHEVCCheckAllRefPicsAreAvailable(u4InstID, tVerMpvDecPrm))>0){
        printk ("[INFO] vHEVCCreateLostPicture lost i4LostPoc %d!!\n", i4LostPoc-1);
        vHEVCCreateLostPicture(u4InstID, i4LostPoc-1);
        if ( i4LostPoc-1 == pCurrSliceHdr->i4POC ){
             printk ("[ERROR] i4LostPoc(%d) equal to CurrSlicePOC(%d)!!\n", i4LostPoc-1, pCurrSliceHdr->i4POC);
             return SET_REG_SYNTAX_ERROR;
        }
        i++;
        if(i>=32)
            break;
    }

    vHEVCVDecApplyRefPicList(u4InstID, tVerMpvDecPrm);
    if ( PARSE_OK != vHEVCVDecSetRefPicList(u4InstID, tVerMpvDecPrm) ){
        return SET_REG_SYNTAX_ERROR;
    }
    vHEVCRefreshMarking(u4InstID,  _ucNalUnitType[u4InstID], _tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.prSliceHdr->i4POC);

    return PARSE_OK;

}

// *********************************************************************
// Function : void AssignQuantParam(void)
// Description : 
// Parameter : 
// Return    : 
// *********************************************************************
void vHEVCAssignQuantParam(UINT32 u4InstID, VDEC_INFO_DEC_PRM_T *ptVerMpvDecPrm)
{
    int  i,size, list, qp;
    H265_SPS_Data * prSPS;
    H265_PPS_Data * prPPS;
    H265_Slice_Hdr_Data * prSliceHdr;
    H265_SL_Data * pScalingList;

#if DEBUG_LEVEL >= DBG_LEVEL_INFO
    printk ("[INFO] vHEVCAssignQuantParam() start!!\n");
    printk ("[INFO] SQT settings\n"); 
#endif
  
    prSPS = ptVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSPS;
    prPPS = ptVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prPPS;
    prSliceHdr = ptVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSliceHdr;

    // SQT part
    vVDecWriteHEVCPP(u4InstID, HEVC_IQ_SL_CTRL, prSPS->bScalingListFlag );
    
    if ( prSPS->bScalingListFlag ){

        pScalingList = &(prSPS->rSPS_ScalingList);
        if (prPPS->bPPSScalingListPresentFlag) {
            pScalingList = &(prPPS->rPPS_ScalingList);
        }
        pScalingList->bUseTransformSkip = prPPS->bTransformSkipEnabledFlag;
        
        if (!prPPS->bPPSScalingListPresentFlag && !prSPS->bScalingListPresentFlag ){
            UINT32 sizeId,listId;
            for( sizeId = 0; sizeId < SCALING_LIST_SIZE_NUM; sizeId++)
            {
                for( listId=0;listId<H265_scalingListNum[sizeId];listId++)
                {
                    int listSize = (MAX_MATRIX_COEF_NUM < H265_scalingListSize[sizeId])? MAX_MATRIX_COEF_NUM : H265_scalingListSize[sizeId];
                    memcpy(pScalingList->pScalingListDeltaCoef[sizeId][listId], vHEVCVerifyGetSLDefaultAddress(sizeId, listId), sizeof(INT32)*listSize );
                    pScalingList->i4ScalingListDC[sizeId][listId] = (INT32)SCALING_LIST_DC;
                }
            }
        }
        
        vVDEC_HAL_H265_SetSLPP( u4InstID, pScalingList );
        
        if (_rH265PicInfo[u4InstID].u4SliceCnt==0){
            vVDecWriteHEVCPP(u4InstID, HEVC_IQ_SRAM_32BITS_CTRL_REG, 0x1 );
        }
        for(size=0;size<SCALING_LIST_SIZE_NUM;size++)
        {
            for(list = 0; list < H265_scalingListNum[size]; list++)
            {
                for(qp=0;qp<SCALING_LIST_REM_NUM;qp++)
                {
                    UINT32 width = H265_scalingListSizeX[size];
                    UINT32 height = H265_scalingListSizeX[size];
                    int *coeff = pScalingList->pScalingListDeltaCoef[size][list];
                    vVDEC_HAL_H265_SetSLVLD(u4InstID,coeff , width, height, H265_invQuantScales[qp]);
                }
            }
        }
        if (_rH265PicInfo[u4InstID].u4SliceCnt==0){
            vVDecWriteHEVCPP(u4InstID, HEVC_IQ_SRAM_32BITS_CTRL_REG, 0 );
        }
        
    }
    
#if DEBUG_LEVEL >= DBG_LEVEL_INFO
    printk ("[INFO] vHEVCAssignQuantParam() done!!\n");
#endif

}


// *********************************************************************
// Function    : UINT32 vHEVCVerifyVDecSetPicInfo(UINT32 u4InstID, VDEC_INFO_DEC_PRM_T *ptVerMpvDecPrm)
// Description : Set Pic related info before reordering
// Parameter   : None
// Return      : UINT32
// *********************************************************************
UINT32 vHEVCVerifyVDecSetPicInfo(UINT32 u4InstID, VDEC_INFO_DEC_PRM_T *ptVerMpvDecPrm)
{

  vHEVCVerifyPrepareFBufInfo(u4InstID, ptVerMpvDecPrm);    
  if (PARSE_OK!=vHEVCPrepareRefPiclist(u4InstID, ptVerMpvDecPrm)){
      return SET_REG_SYNTAX_ERROR;
  }
  
  // Find a empty fbuf 
  vHEVCAllocateFBuf(u4InstID, &_tVerMpvDecPrm[u4InstID], 1);   
  vVDEC_HAL_H265_SetRefPicListReg(u4InstID);
 
  vVDEC_HAL_H265_SetPicInfoReg(u4InstID);
  vHEVCAssignQuantParam(u4InstID, ptVerMpvDecPrm);

  vVDEC_HAL_H265_SetSHDRHEVLD(u4InstID, ptVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSliceHdr,
                                        ptVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSPS->bUseSAO,
                                        ptVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prPPS);
  vVDEC_HAL_H265_SetSPSHEVLD(u4InstID, ptVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSPS, ptVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prPPS);
  vVDEC_HAL_H265_SetPPSHEVLD(u4InstID, ptVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSPS, ptVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prPPS);

  vHEVCVerifyInitTilesInfo( u4InstID, ptVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prPPS);
  vVDEC_HAL_H265_SetTilesInfo(u4InstID, ptVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSPS, ptVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prPPS);

  return PARSE_OK;

}

// *********************************************************************
// Function    : void   vHEVCVerifyPrepareFBufInfo(UINT32 u4InstID, VDEC_INFO_DEC_PRM_T *tVerMpvDecPrm)
// Description : check pic type to send P_0, B_0, B_1
// Parameter   : None
// Return      : None
// *********************************************************************
void vHEVCVerifyPrepareFBufInfo(UINT32 u4InstID, VDEC_INFO_DEC_PRM_T *tVerMpvDecPrm)
{
#if DEBUG_LEVEL >= DBG_LEVEL_INFO
      printk ("[INFO] vHEVCVerifyPrepareFBufInfo() start!!\n");
#endif

  
  tVerMpvDecPrm->u4PicW = ((tVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSPS->u4PicWidthInLumaSamples + 63) >> 6 ) << 6; //64x
  tVerMpvDecPrm->u4PicH = ((tVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSPS->u4PicHeightInLumaSamples + 63) >> 6 ) << 6; //64x
  tVerMpvDecPrm->u4PicBW = tVerMpvDecPrm->u4PicW;

  if((tVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.rLastInfo.u4LastPicW != tVerMpvDecPrm->u4PicW) 
      || (tVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.rLastInfo.u4LastPicH != tVerMpvDecPrm->u4PicH))
  {
    vHEVCPartitionDPB(u4InstID);      
  }
  
#if DEBUG_LEVEL >= DBG_LEVEL_INFO
    printk ("[INFO] vHEVCVerifyPrepareFBufInfo() done!!\n");
#endif

}


// *********************************************************************
// Function    : vHEVCVDecGetRefPicbyPOC(UINT32 u4InstID, INT32 i4POC)
// Description : get frame buffer by poc
// Parameter   : None
// Return      : frame buffe  index;
// *********************************************************************
UINT32 vHEVCVDecGetRefPicbyPOC(UINT32 u4InstID, INT32 i4POC)
{
    INT32 i ;

    for(i=0; i<_tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.ucMaxFBufNum; i++)
    {
    /*
#if DEBUG_LEVEL >= DBG_LEVEL_INFO
            printk ("[INFO]     FB index:%d\n",i );
            printk ("[INFO]         ucFBufStatus %d; FB i4POC: %d;\n", _ptH265FBufInfo[u4InstID][i].ucFBufStatus, _ptH265FBufInfo[u4InstID][i].i4POC);
            printk ("[INFO]         input i4POC: %d; FB bFirstSliceReferenced: %d\n", i4POC, _ptH265FBufInfo[u4InstID][i].bFirstSliceReferenced);
#endif
   */
      if (_ptH265FBufInfo[u4InstID][i].i4POC == i4POC && _ptH265FBufInfo[u4InstID][i].ucFBufStatus != NO_PIC){
          return i;
      }
    } 

    printk("\n[ERROR] GetRef Error!! no such POC(%d) picture in Frame buffer!!\n\n", i4POC);
    return 0xFFFFFFFF;
}


// *********************************************************************
// Function    :  vHEVCVDecGetLTRefPicbyPOC(UINT32 u4InstID, INT32 i4POC, BOOL LTMSBPresent, VDEC_INFO_DEC_PRM_T *tVerMpvDecPrm)
// Description : get frame buffer by poc
// Parameter   : None
// Return      :  frame buffe  index;
// *********************************************************************
UINT32 vHEVCVDecGetLTRefPicbyPOC(UINT32 u4InstID, INT32 i4POC, BOOL bMSBpresentFlag )
{
    INT32 i ;
    INT32 i4LtFBIndex = 0;
    INT32 i4POCCycle ;
    H265_Slice_Hdr_Data* pCurrSliceHdr;


    pCurrSliceHdr = _tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.prSliceHdr;

    
    i4POCCycle =1<< (_tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.prSPS->u4Log2MaxPicOrderCntLsbMinus4+4);

    if ( !bMSBpresentFlag ){
        i4POC = i4POC%i4POCCycle;
    }
    
    for(i=0; i<_tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.ucMaxFBufNum; i++)
    {
/*
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            printk ("[INFO]     FB index:%d\n",i );
            printk ("[INFO]         ucFBufStatus %d; FB i4POC: %d; FB bFirstSliceReferenced: %d \n", _ptH265FBufInfo[u4InstID][i].ucFBufStatus, _ptH265FBufInfo[u4InstID][i].i4POC, _ptH265FBufInfo[u4InstID][i].bFirstSliceReferenced);
            printk ("[INFO]         input i4POC: %d; i4POCCycle: %d \n", i4POC,i4POCCycle);
        #endif
        */
        if (_ptH265FBufInfo[u4InstID][i].ucFBufStatus != NO_PIC && _ptH265FBufInfo[u4InstID][i].i4POC !=pCurrSliceHdr->i4POC ){ 
            int pic_poc =  _ptH265FBufInfo[u4InstID][i].i4POC;
            if ( !bMSBpresentFlag ){
                pic_poc = pic_poc%i4POCCycle;
            }

            if (pic_poc == i4POC){
                if( _ptH265FBufInfo[u4InstID][i].ucFBufRefType == LREF_PIC) {
                    return i;
                } else {
                    i4LtFBIndex = i;
                }
                break;
            }
        }
    } 
    if ( i4LtFBIndex < _tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.ucMaxFBufNum ){
        return i4LtFBIndex;
    } else {
        printk("\n[ERROR] GetRef Error!! no such LT POC(%d) picture in Frame buffer!!\n\n", i4POC);
        return 0xFFFFFFFF;
    }
    
}


// *********************************************************************
// Function    : void vHEVCVDecApplyRefPicList(UINT32 u4InstID, VDEC_INFO_DEC_PRM_T *tVerMpvDecPrm)
// Description :  Function for applying picture marking based on the Reference Picture Set.
// Parameter   : None
// Return      : None
// *********************************************************************
void vHEVCVDecApplyRefPicList(UINT32 u4InstID, VDEC_INFO_DEC_PRM_T *tVerMpvDecPrm)
{

    int i, isReference;
    int j = 0;
    H265_Slice_Hdr_Data* pCurrSliceHdr;
    pH265_RPS_Data pCurrRPS;

#if DEBUG_LEVEL >= DBG_LEVEL_INFO
          printk ("[INFO] vHEVCVDecApplyRefPicList() start!!\n");
#endif

    pCurrSliceHdr = tVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSliceHdr;
    pCurrRPS = pCurrSliceHdr->pShortTermRefPicSets;

    // loop through all pictures in the reference picture buffer
    for(j=0; j<_tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.ucMaxFBufNum; j++){

        if( (!_ptH265FBufInfo[u4InstID][j].bFirstSliceReferenced) ||_ptH265FBufInfo[u4InstID][j].ucFBufStatus == NO_PIC )
        {
          continue;
        }
   
        isReference = 0;
        // loop through all pictures in the Reference Picture Set
        // to see if the picture should be kept as reference picture
        for(i=0;i<pCurrRPS->u4NumberOfNegativePictures+pCurrRPS->u4NumberOfPositivePictures;i++)
        {
            if (i>=MAX_NUM_REF_PICS){ break; }
            if( (!(_ptH265FBufInfo[u4InstID][j].ucFBufRefType==LREF_PIC)) && _ptH265FBufInfo[u4InstID][j].i4POC == (pCurrSliceHdr->i4POC + pCurrRPS->i4DeltaPOC[i]) )
            {
                isReference = 1;
                _ptH265FBufInfo[u4InstID][j].bUsedByCurr = pCurrRPS->bUsed[i];
                _ptH265FBufInfo[u4InstID][j].bUsedAsLongTerm = 0;
            }
        }
        
        for(;i<pCurrRPS->u4NumberOfPictures;i++)
        {
            if (i>=MAX_NUM_REF_PICS){ break; }
            if( 1 == pCurrRPS->bCheckLTMSB[i] )
            {
                if((_ptH265FBufInfo[u4InstID][j].ucFBufRefType==LREF_PIC) && _ptH265FBufInfo[u4InstID][j].i4POC == pCurrRPS->i4POC[i] )
                {
                    isReference = 1;
                    _ptH265FBufInfo[u4InstID][j].bUsedByCurr = pCurrRPS->bUsed[i];
                    _rH265PicInfo[u4InstID].i4DpbLTBuffId[_rH265PicInfo[u4InstID].i4DpbLTBuffCnt] = j;
                    _rH265PicInfo[u4InstID].i4DpbLTBuffCnt ++;
                }
            }
            else 
            {
                if((_ptH265FBufInfo[u4InstID][j].ucFBufRefType==LREF_PIC) && (_ptH265FBufInfo[u4InstID][j].i4POC%(1<<_ptH265FBufInfo[u4InstID][j].u4POCBits) == pCurrRPS->i4POC[i]%(1<<_ptH265FBufInfo[u4InstID][j].u4POCBits)))
                {
                    isReference = 1;
                    _ptH265FBufInfo[u4InstID][j].bUsedByCurr = pCurrRPS->bUsed[i];
                    _rH265PicInfo[u4InstID].i4DpbLTBuffId[_rH265PicInfo[u4InstID].i4DpbLTBuffCnt] = j;
                    _rH265PicInfo[u4InstID].i4DpbLTBuffCnt ++;

                }
            }
        }

        // mark the picture as "unused for reference" if it is not in
        // the Reference Picture Set
        if(_ptH265FBufInfo[u4InstID][j].i4POC != pCurrSliceHdr->i4POC &&  _ptH265FBufInfo[u4InstID][j].ucFBufStatus != NO_PIC && isReference == 0)    
        { 

            if(_ptH265FBufInfo[u4InstID][j].bUsedAsLongTerm)
            {
              _rH265PicInfo[u4InstID].i4DpbLTBuffId[_rH265PicInfo[u4InstID].i4DpbLTBuffCnt] = j;
              _rH265PicInfo[u4InstID].i4DpbLTBuffCnt ++;
            }

            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                printk ("[INFO] _ptH265FBufInfo index = %d (POC %d) marked as NREF_PIC (cleared) !!\n", j, _ptH265FBufInfo[u4InstID][j].i4POC);
            #endif
            vVerifyClrFBufInfo(u4InstID, j );

        }

    }

#if DEBUG_LEVEL >= DBG_LEVEL_INFO
    printk ("[INFO] vHEVCVDecApplyRefPicList() done!!\n");
#endif

}



// *********************************************************************
// Function    : UINT32 vHEVCVDecSetRefPicList(UINT32 u4InstID, VDEC_INFO_DEC_PRM_T *tVerMpvDecPrm)
// Description : Set P ref pic list by Pic Num
// Parameter   : None
// Return      : UINT32
// *********************************************************************
UINT32 vHEVCVDecSetRefPicList(UINT32 u4InstID, VDEC_INFO_DEC_PRM_T *tVerMpvDecPrm)
{
    int iNumRefIdx[3];
    int i;

    UINT32  pcRefPic= 0;
    UINT32  RefPicSetStCurr0[16];
    UINT32  RefPicSetStCurr1[16];
    UINT32  RefPicSetLtCurr[16];
    UINT32 NumPocStCurr0 = 0;
    UINT32 NumPocStCurr1 = 0;
    UINT32 NumPocLtCurr = 0;
    H265_Slice_Hdr_Data* pCurrSliceHdr;
    pH265_RPS_Data pCurrRPS;

    pCurrSliceHdr = tVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSliceHdr;
    pCurrRPS = pCurrSliceHdr->pShortTermRefPicSets;
       
    iNumRefIdx[0] = pCurrSliceHdr->i4NumRefIdx[REF_PIC_LIST_0];
    iNumRefIdx[1] = pCurrSliceHdr->i4NumRefIdx[REF_PIC_LIST_1];

    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
    //dump pCurrRPS
    printk("[INFO] pCurrRPS u4NumberOfNegativePictures: %d u4NumberOfPositivePictures: %d u4NumberOfLongtermPictures: %d \n"
                , pCurrRPS->u4NumberOfNegativePictures, pCurrRPS->u4NumberOfPositivePictures, pCurrRPS->u4NumberOfLongtermPictures);
    for (i=0; i < pCurrRPS->u4NumberOfNegativePictures+pCurrRPS->u4NumberOfPositivePictures+pCurrRPS->u4NumberOfLongtermPictures; i++){
        if (i>=MAX_NUM_REF_PICS){ break; }
        if (i <pCurrRPS->u4NumberOfNegativePictures+pCurrRPS->u4NumberOfPositivePictures){
            printk("[INFO] Dump Short term RPS: \n");
        }else {
            printk("[INFO] Dump Long term RPS: \n");
        }
        printk("[INFO]     bUsed[%d] = %d \n", i, pCurrRPS->bUsed[i]);
        printk("[INFO]     i4DeltaPOC[%d] = %d \n", i, pCurrRPS->i4DeltaPOC[i]);
        printk("[INFO]     i4POC[%d] = %d (For LT)\n", i, pCurrRPS->i4POC[i]);
        printk("[INFO]     bCheckLTMSB[%d] = %d \n", i, pCurrRPS->bCheckLTMSB[i]);
    }
    #endif

    if (pCurrRPS->u4NumberOfNegativePictures+pCurrRPS->u4NumberOfPositivePictures+pCurrRPS->u4NumberOfLongtermPictures>_tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.ucMaxFBufNum){
        printk ("[ERROR] SetRefPicList: Number of ref pic(%d) > ucMaxFBufNum(%d)!!\n",pCurrRPS->u4NumberOfNegativePictures+pCurrRPS->u4NumberOfPositivePictures+pCurrRPS->u4NumberOfLongtermPictures, _tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.ucMaxFBufNum);
        return SET_REG_SYNTAX_ERROR;
    }

    for(i=0; i < pCurrRPS->u4NumberOfNegativePictures; i++)
    {
        if(pCurrSliceHdr->pShortTermRefPicSets->bUsed[i])
        {
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                printk("[INFO] Neg vHEVCVDecGetRefPicbyPOC: CurrSlice.POC: %d;  CurrRPS.DeltaPOC: %d\n", pCurrSliceHdr->i4POC, pCurrRPS->i4DeltaPOC[i]);
            #endif
            pcRefPic = vHEVCVDecGetRefPicbyPOC(u4InstID, pCurrSliceHdr->i4POC+pCurrRPS->i4DeltaPOC[i]);
            if ( pcRefPic == 0xFFFFFFFF) {
                continue;
            }
            _ptH265FBufInfo[u4InstID][pcRefPic].ucFBufRefType = SREF_PIC;
            _ptH265FBufInfo[u4InstID][pcRefPic].bUsedAsLongTerm = 0;
            RefPicSetStCurr0[NumPocStCurr0] = pcRefPic;
            _ptH265FBufInfo[u4InstID][pcRefPic].bLtMsbPresentFlag = 0;  
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                printk("[INFO]     FBindex: %d RefPic.POC: %d RefPic.PicCnt: %d FBufRefType: %d\n", pcRefPic, _ptH265FBufInfo[u4InstID][pcRefPic].i4POC, _ptH265FBufInfo[u4InstID][pcRefPic].u4PicCnt, _ptH265FBufInfo[u4InstID][pcRefPic].ucFBufRefType);
            #endif
            NumPocStCurr0++;
        }
    }

    for(; i < pCurrRPS->u4NumberOfNegativePictures+pCurrRPS->u4NumberOfPositivePictures; i++)
    {
        if(pCurrSliceHdr->pShortTermRefPicSets->bUsed[i])
        {
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                printk("[INFO] Pos vHEVCVDecGetRefPicbyPOC: CurrSlice.POC: %d;  CurrRPS.DeltaPOC: %d\n", pCurrSliceHdr->i4POC, pCurrRPS->i4DeltaPOC[i]);
            #endif
            pcRefPic =  vHEVCVDecGetRefPicbyPOC(u4InstID, pCurrSliceHdr->i4POC+pCurrRPS->i4DeltaPOC[i]);
            if ( pcRefPic == 0xFFFFFFFF) {
                continue;
            }
            _ptH265FBufInfo[u4InstID][pcRefPic].ucFBufRefType = SREF_PIC;
            _ptH265FBufInfo[u4InstID][pcRefPic].bUsedAsLongTerm = 0;
            RefPicSetStCurr1[NumPocStCurr1] = pcRefPic;
            _ptH265FBufInfo[u4InstID][pcRefPic].bLtMsbPresentFlag = 0;  
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                printk("[INFO]     FBindex: %d RefPic.POC: %d RefPic.PicCnt: %d FBufRefType: %d\n", pcRefPic, _ptH265FBufInfo[u4InstID][pcRefPic].i4POC, _ptH265FBufInfo[u4InstID][pcRefPic].u4PicCnt, _ptH265FBufInfo[u4InstID][pcRefPic].ucFBufRefType);
            #endif
            NumPocStCurr1++;
        }
    }

    if ( pCurrRPS->u4NumberOfNegativePictures+pCurrRPS->u4NumberOfPositivePictures+pCurrRPS->u4NumberOfLongtermPictures != 0) {
        for( i = (int)(pCurrRPS->u4NumberOfNegativePictures+pCurrRPS->u4NumberOfPositivePictures+pCurrRPS->u4NumberOfLongtermPictures)-1; 
                i >= (int)(pCurrRPS->u4NumberOfNegativePictures+pCurrRPS->u4NumberOfPositivePictures) ; i--)
        {
            if(pCurrSliceHdr->pShortTermRefPicSets->bUsed[i])
            {
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                printk("[INFO] LT vHEVCVDecGetLTRefPicbyPOC: CurrRPS.u4POC: %d; bCheckLTMSB: %d\n", pCurrRPS->i4POC[i], pCurrRPS->bCheckLTMSB[i]);
            #endif
                
                pcRefPic = vHEVCVDecGetLTRefPicbyPOC(u4InstID, pCurrRPS->i4POC[i], pCurrRPS->bCheckLTMSB[i]);
                _ptH265FBufInfo[u4InstID][pcRefPic].ucFBufRefType = LREF_PIC;
                _ptH265FBufInfo[u4InstID][pcRefPic].bUsedAsLongTerm = 1;
                RefPicSetLtCurr[NumPocLtCurr] = pcRefPic;
                NumPocLtCurr++;
                 _ptH265FBufInfo[u4InstID][pcRefPic].bLtMsbPresentFlag = pCurrRPS->bCheckLTMSB[i];
                #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                    printk("[INFO]     FBindex: %d RefPic.POC: %d RefPic.PicCnt: %d FBufRefType: %d\n", pcRefPic, _ptH265FBufInfo[u4InstID][pcRefPic].i4POC, _ptH265FBufInfo[u4InstID][pcRefPic].u4PicCnt, _ptH265FBufInfo[u4InstID][pcRefPic].ucFBufRefType);
                #endif
            }
            if(pcRefPic == 0xFFFFFFFF) {
                pcRefPic = vHEVCVDecGetLTRefPicbyPOC(u4InstID, pCurrRPS->i4POC[i], pCurrRPS->bCheckLTMSB[i]);
            }
        }
    }
    // ref_pic_list_init
    UINT32   rpsCurrList0[H265_MAX_PIC_LIST_NUM+1];
    UINT32   rpsCurrList1[H265_MAX_PIC_LIST_NUM+1];
    INT32     numPocTotalCurr = NumPocStCurr0 + NumPocStCurr1 + NumPocLtCurr;

    // ----------------------------------------------
    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
        printk("[INFO] Construct rpsCurrList0, rpsCurrList1 \n");
    #endif

    UINT32 cIdx;
    cIdx = 0;
    while ((cIdx < 16) && (numPocTotalCurr > 0))
    { 
        if(cIdx < 16)
        for ( i=0; (i<NumPocStCurr0) && (cIdx < 16); cIdx++,i++)
        {
          rpsCurrList0[cIdx] = RefPicSetStCurr0[ i ];
        }
        if(cIdx < 16)
        for ( i=0; (i<NumPocStCurr1) && (cIdx < 16); cIdx++,i++)
        {
          rpsCurrList0[cIdx] = RefPicSetStCurr1[ i ];
        }
        if(cIdx < 16)
        for ( i=0; (i<NumPocLtCurr) && (cIdx < 16); cIdx++,i++)
        {
          rpsCurrList0[cIdx] = RefPicSetLtCurr[ i ];
        }
    }

    cIdx = 0;
    while ((cIdx < 16) && (numPocTotalCurr > 0))
    {
        if(cIdx < 16)
        for ( i=0; (i<NumPocStCurr1) && (cIdx < 16); cIdx++,i++)
        {
          rpsCurrList1[cIdx] = RefPicSetStCurr1[ i ];
        }

        if(cIdx < 16)
        for ( i=0; (i<NumPocStCurr0) && (cIdx < 16); cIdx++,i++)
        {
          rpsCurrList1[cIdx] = RefPicSetStCurr0[ i ];
        }

        if(cIdx < 16)
        for ( i=0; (i<NumPocLtCurr) && (cIdx < 16); cIdx++,i++)
        {
          rpsCurrList1[cIdx] = RefPicSetLtCurr[ i ];
        }
    }
    // ----------------------------------------------
      
#if DEBUG_LEVEL >= DBG_LEVEL_INFO
      printk("[INFO] Set PicInfo: NumPocStCurr0 %d; NumPocStCurr1 %d; NumPocLtCurr %d; \n", NumPocStCurr0, NumPocStCurr1, NumPocLtCurr);
#endif
      _rH265PicInfo[u4InstID].i4RefListNum = numPocTotalCurr;       //number of tatol ref pics
      
    for (cIdx = 0; (cIdx < numPocTotalCurr) & (numPocTotalCurr > 0); cIdx ++)
    {
        int poc_diff;
        poc_diff = pCurrSliceHdr->i4POC - _ptH265FBufInfo[u4InstID][rpsCurrList0[cIdx]].i4POC;
        poc_diff = (poc_diff < -128) ? -128 :
                   (poc_diff >= 127) ? 127 : poc_diff;

        _rH265PicInfo[u4InstID].i4PocDiffList0[cIdx] = poc_diff;
        _rH265PicInfo[u4InstID].i4LongTermList0[cIdx] = (_ptH265FBufInfo[u4InstID][rpsCurrList0[cIdx]].ucFBufRefType==LREF_PIC);
        _rH265PicInfo[u4InstID].i4List0DecOrderNo[cIdx] = _ptH265FBufInfo[u4InstID][rpsCurrList0[cIdx]].u4PicCnt;
        _rH265PicInfo[u4InstID].i4BuffIdList0[cIdx] = rpsCurrList0[cIdx];
    }
      

    for (cIdx = 0; (cIdx < numPocTotalCurr) & (numPocTotalCurr > 0); cIdx ++)
    {
        int poc_diff;
        poc_diff = pCurrSliceHdr->i4POC -_ptH265FBufInfo[u4InstID][rpsCurrList1[cIdx]].i4POC;
        poc_diff = (poc_diff < -128) ? -128 :
                (poc_diff >= 127) ? 127 : poc_diff;

        _rH265PicInfo[u4InstID].i4PocDiffList1[cIdx] = poc_diff;
        _rH265PicInfo[u4InstID].i4LongTermList1[cIdx] =  (_ptH265FBufInfo[u4InstID][rpsCurrList1[cIdx]].ucFBufRefType==LREF_PIC);
        _rH265PicInfo[u4InstID].i4List1DecOrderNo[cIdx] = _ptH265FBufInfo[u4InstID][rpsCurrList1[cIdx]].u4PicCnt;
        _rH265PicInfo[u4InstID].i4BuffIdList1[cIdx] = rpsCurrList1[cIdx];
    }

    // ----------------------------------------------


    cIdx = 0;
    for ( i=0; i<NumPocStCurr0; i++, cIdx++)
    {
      rpsCurrList0[cIdx] = RefPicSetStCurr0[i];
    }
    for ( i=0; i<NumPocStCurr1; i++, cIdx++)
    {
      rpsCurrList0[cIdx] = RefPicSetStCurr1[i];
    }
    for ( i=0; i<NumPocLtCurr;  i++, cIdx++)
    {
      rpsCurrList0[cIdx] = RefPicSetLtCurr[i];
    }
      

    cIdx = 0;
    for ( i=0; i<NumPocStCurr1; i++, cIdx++)
    {
      rpsCurrList1[cIdx] = RefPicSetStCurr1[i];
    }
    for ( i=0; i<NumPocStCurr0; i++, cIdx++)
    {
      rpsCurrList1[cIdx] = RefPicSetStCurr0[i];
    }
    for ( i=0; i<NumPocLtCurr;  i++, cIdx++)
    {
      rpsCurrList1[cIdx] = RefPicSetLtCurr[i];
    }

    // set low delay flag

    if ( numPocTotalCurr > 0 ){
        BOOL bLowDelay = true;
        int  iCurrPOC  = pCurrSliceHdr->i4POC;
        int iRefIdx = 0;

        for (iRefIdx = 0; iRefIdx < iNumRefIdx[0]  && bLowDelay; iRefIdx++)
        {
            if ( _ptH265FBufInfo[u4InstID][rpsCurrList0[iRefIdx%numPocTotalCurr]].i4POC > iCurrPOC )
            {
                bLowDelay = false;
            }
        }

        for (iRefIdx = 0; iRefIdx < iNumRefIdx[1]  && bLowDelay; iRefIdx++)
        {
            if ( _ptH265FBufInfo[u4InstID][rpsCurrList1[iRefIdx%numPocTotalCurr]].i4POC > iCurrPOC )
            {
              bLowDelay = false;
            }
        }        
        _rH265PicInfo[u4InstID].bLowDelayFlag = bLowDelay;
    }

    return PARSE_OK;
}



// *********************************************************************
// Function    : void vHEVCPartitionDPB(UINT32 u4InstID)
// Description : Set VDec related parameters
// Parameter   : None
// Return      : None
// *********************************************************************
void vHEVCPartitionDPB(UINT32 u4InstID)
{
  INT32 i;
  BOOL bIsUFO;
  UINT32 u4DramPicSize;
  UINT32 u4DramPicArea;
  UINT32 u4DramMvSize;
  UINT32 u4DramPicYCSize;
  UINT32 u4UFOLenYsize, u4UFOLenCsize;
  UINT32 u4PicSizeBS, u4PicSizeYBS;

  bIsUFO = _tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.bIsUFOMode;

  u4DramPicSize = (_tVerMpvDecPrm[u4InstID].u4PicW *_tVerMpvDecPrm[u4InstID].u4PicH);
  u4DramMvSize = ( u4DramPicSize >> 8)<<4;

  if (bIsUFO){
    u4UFOLenYsize = ((((u4DramPicSize+255) >>8) + 63 + (16*8)) >>6) << 6;
    u4UFOLenCsize = (((u4UFOLenYsize>>1) + 15 + (16*8)) >>4) <<4;
    u4PicSizeYBS = ((u4DramPicSize + 4095) >>12) <<12;
    u4PicSizeBS = ((u4PicSizeYBS + (u4DramPicSize >>1) + 511) >>9) <<9;
    u4DramPicYCSize  = (((u4PicSizeBS + (u4UFOLenYsize << 1)) + 511) >> 9) <<9;     //512 align
    u4DramPicArea = ((u4DramPicYCSize +  u4DramMvSize + 4095) >> 12) <<12;         //4096 align
  } else {
    u4DramPicYCSize = ((u4DramPicSize* 3/2 + 511) >>9 )<<9;   //512 align
    u4DramPicArea =  (( u4DramPicYCSize +  u4DramMvSize + 2047)>>11)<<11;    //2048 align
  }
 
  _tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.ucMaxFBufNum = (DPB_SZ /u4DramPicArea>255)? 255 : DPB_SZ /u4DramPicArea;
  printk("[INFO] [VDEC_VER] PicW = %d, PicH = %d, u4DramPicArea = %d. MaxFBufNum = %d\n", _tVerMpvDecPrm[u4InstID].u4PicW, _tVerMpvDecPrm[u4InstID].u4PicH,  u4DramPicArea, _tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.ucMaxFBufNum);
  if(_tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.prSPS->u4NumRefFrames > _tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.ucMaxFBufNum)
  {
    _tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.ucMaxFBufNum = _tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.prSPS->u4NumRefFrames;
    printk("//==============================================================================\n");
    printk("[WARNING] [VDEC-H265-VFY] DPB Buffer smaller than file needed,should remalloc DPB buffer!\n" );
    printk("[WARNING] [VDEC_H265_VFY] DPB Size need about 0x%x Bytes, SPS NumRefFrames:%d\n",(_tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.prSPS->u4NumRefFrames + 1)*u4DramPicArea, _tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.prSPS->u4NumRefFrames);
    printk("//==============================================================================\n");
  }
  
  if(_tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.ucMaxFBufNum > 25 )
  {
    _tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.ucMaxFBufNum = 25;
  }

  if (_tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.ucMaxFBufNum * u4DramPicArea > DPB_SZ)
  {
       printk("[ERROR] [VDEC_VER] H265 DPB Size Not Enough!!!!!\n");
       printk("[ERROR] [VDEC_VER] H265 DPB Size Not Enough!!!!!\n");
       printk("[ERROR] [VDEC_VER] H265 DPB Size Not Enough!!!!!\n");
       printk("[ERROR] [VDEC_VER] H265 DPB Size Not Enough!!!!!\n");
       printk("[ERROR] [VDEC_VER] H265 DPB Size Not Enough!!!!!\n");
       VDEC_ASSERT(0);
  }

  printk("[INFO] [H265] Inst%d, u4DramPicSize:0x%x, u4DramPicArea:0x%x\n", u4InstID, u4DramPicSize, u4DramPicArea);
  if (bIsUFO){
      printk("[INFO] [H265]    u4UFOLenYsize:0x%x, u4UFOLenCsize:0x%x, u4PicSizeYBS:0x%x, u4PicSizeBS:0x%x\n", u4UFOLenYsize, u4UFOLenCsize, u4PicSizeYBS, u4PicSizeBS );
  }

  for(i=0; i<_tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.ucMaxFBufNum; i++)
  {
    _ptH265FBufInfo[u4InstID][i].u4W = _tVerMpvDecPrm[u4InstID].u4PicW;
    _ptH265FBufInfo[u4InstID][i].u4H = _tVerMpvDecPrm[u4InstID].u4PicH;    
    _ptH265FBufInfo[u4InstID][i].u4DramPicSize = u4DramPicSize;
    _ptH265FBufInfo[u4InstID][i].u4DramPicArea = u4DramPicArea;
    _ptH265FBufInfo[u4InstID][i].u4DramMvSize = u4DramMvSize;
   
    _ptH265FBufInfo[u4InstID][i].u4Addr = ((UINT32)_pucDPB[u4InstID]) + (i * u4DramPicArea);
    _ptH265FBufInfo[u4InstID][i].u4YStartAddr = _ptH265FBufInfo[u4InstID][i].u4Addr;
    _ptH265FBufInfo[u4InstID][i].u4CAddrOffset = u4DramPicSize;

    printk("[INFO] [H265] Inst%d, buf %d use frame Buffer 0x%x (0x%x)\n", u4InstID, i, _ptH265FBufInfo[u4InstID][i].u4Addr, PHYSICAL(_ptH265FBufInfo[u4InstID][i].u4Addr));

    if (bIsUFO){
        _ptH265FBufInfo[u4InstID][i].u4CAddrOffset = u4PicSizeYBS;
        _ptH265FBufInfo[u4InstID][i].u4PicSizeBS = u4PicSizeBS;
        _ptH265FBufInfo[u4InstID][i].u4UFOLenYsize = u4UFOLenYsize;
        _ptH265FBufInfo[u4InstID][i].u4PicSizeYBS = u4PicSizeYBS;
        _ptH265FBufInfo[u4InstID][i].u4UFOLenCsize  = u4UFOLenCsize;
        _ptH265FBufInfo[u4InstID][i].u4YLenStartAddr = _ptH265FBufInfo[u4InstID][i].u4YStartAddr + u4PicSizeBS;
        _ptH265FBufInfo[u4InstID][i].u4CLenStartAddr = _ptH265FBufInfo[u4InstID][i].u4YLenStartAddr + u4UFOLenYsize;
        
        printk("[INFO] [H265] Inst%d, buf %d use UFO Buffer 0x%x (0x%x)\n", u4InstID, i, _ptH265FBufInfo[u4InstID][i].u4YLenStartAddr, PHYSICAL(_ptH265FBufInfo[u4InstID][i].u4YLenStartAddr));
    }

    #if (CONFIG_CHIP_VER_CURR < CONFIG_CHIP_VER_MT8560)//14/9/2010 mtk40343
    if (_tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.ucMaxFBufNum <= 8)
     {
         _tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.fgIsReduceMVBuffer = TRUE;
     }
    #endif

    _ptH265FBufInfo[u4InstID][i].u4MvStartAddr = _ptH265FBufInfo[u4InstID][i].u4Addr + u4DramPicYCSize;
    printk("[INFO] [H265] Inst%d, buf %d use large B MV Buffer 0x%x (0x%x)\n", u4InstID, i, _ptH265FBufInfo[u4InstID][i].u4MvStartAddr, PHYSICAL(_ptH265FBufInfo[u4InstID][i].u4MvStartAddr));

  }
  // current reset to 0 when DPB partition.
  _ptH265CurrFBufInfo[u4InstID] = &_ptH265FBufInfo[u4InstID][0];
  _tVerMpvDecPrm[u4InstID].ucDecFBufIdx = 0;
  
}


// *********************************************************************
// Function    : void vHEVCAllocateFBuf(UINT32 u4InstID, VDEC_INFO_DEC_PRM_T *tVerMpvDecPrm, BOOL fgFillCurrFBuf)
// Description : Allocate decoding frm buff in DPB
// Parameter   : 
// Return      : UINT32
// *********************************************************************
UINT32 vHEVCAllocateFBuf(UINT32 u4InstID, VDEC_INFO_DEC_PRM_T *tVerMpvDecPrm, BOOL fgFillCurrFBuf)
{
  INT32 i;
  INT32 iMinPOC;
  UINT32 u4MinPOCFBufIdx = 0;
  
  // Check if DPB full
  iMinPOC = 0x7fffffff;
  for(i=0; i<_tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.ucMaxFBufNum; i++)
  {
    if(_ptH265FBufInfo[u4InstID][i].ucFBufStatus == NO_PIC)
    {
      iMinPOC = 0x7fffffff;
      u4MinPOCFBufIdx = i;        
      break;
    }

    else if((iMinPOC > _ptH265FBufInfo[u4InstID][i].i4POC)&& fgIsNonRefFBuf(u4InstID, i))
    {
      iMinPOC = _ptH265FBufInfo[u4InstID][i].i4POC;
      u4MinPOCFBufIdx = i;
    }

  }  
  // No empty DPB, 1 FBuf output
  if(_ptH265FBufInfo[u4InstID][u4MinPOCFBufIdx].ucFBufStatus != NO_PIC)
  {
    vVerifyClrFBufInfo(u4InstID, u4MinPOCFBufIdx);
  }

  #if DEBUG_LEVEL >= DBG_LEVEL_INFO
        printk("[INFO] vHEVCAllocateFBuf() MaxFB IndexNum = %d; get frame buffer index = %d\n" , _tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.ucMaxFBufNum ,u4MinPOCFBufIdx );
  #endif
  
  if(fgFillCurrFBuf)
  {
      _tVerMpvDecPrm[u4InstID].ucDecFBufIdx = u4MinPOCFBufIdx;
      vHEVCSetCurrFBufIdx(u4InstID, _tVerMpvDecPrm[u4InstID].ucDecFBufIdx);    
  }
  return u4MinPOCFBufIdx;
}


// *********************************************************************
// Function    : BOOL fgHEVCChkRefInfo(UINT32 u4InstID, UINT32 u4FBufIdx, UINT32 u4RefType)
// Description : Check if reference picture should be insered to ref pic list
// Parameter   : None
// Return      : None
// *********************************************************************
BOOL fgHEVCChkRefInfo(UINT32 u4InstID, UINT32 u4FBufIdx, UINT32 u4RefType)
{
    // NOTE: A non-pared reference fiedl is not used for inter prediction for decoding a frame.
    if((_ptH265FBufInfo[u4InstID][u4FBufIdx].ucFBufRefType == u4RefType))
    {
      return TRUE;
    }
    else
    {
      return FALSE;
    }
}

// *********************************************************************
// Function    : void vHEVCSetCurrFBufIdx(UINT32 u4InstID, UINT32 u4DecFBufIdx)
// Description : Set Curr FBuf index
// Parameter   : None
// Return      : None
// *********************************************************************
void vHEVCSetCurrFBufIdx(UINT32 u4InstID, UINT32 u4DecFBufIdx)
{
   _ptH265CurrFBufInfo[u4InstID] = &_ptH265FBufInfo[u4InstID][u4DecFBufIdx];  
   _pucDecWorkBuf[u4InstID] = (UCHAR *)(_ptH265CurrFBufInfo[u4InstID]->u4Addr);
   if (_tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.bIsUFOMode){                     //UFO mode clear Dram for golden compare
        memset(_ptH265CurrFBufInfo[u4InstID]->u4YStartAddr , 0, _ptH265CurrFBufInfo[u4InstID]->u4DramPicArea);
   }
}

UINT32 vHEVCParseNALs(UINT32 u4InstID)
{
#ifndef STACK_FRAME_LIMIT
    UINT32 u4NalType[500];
    UINT32 u4NalStartOffset[500];
#endif
    UINT32 u4OriginalStartAddr, u4Temp, u4Bits;
    VDEC_INFO_H265_INIT_PRM_T rH265VDecInitPrm;
    VDEC_INFO_H265_BS_INIT_PRM_T rH265BSInitPrm;
    int i;

    u4OriginalStartAddr = u4VDEC_HAL_H265_ReadRdPtr(_u4BSID[u4InstID], u4InstID, (UINT32)_pucVFifo[u4InstID], &u4Bits);

    printk("Nal Info ===================================\n");

    for (i=0; i<500; i++){
        u4Temp = u4VDEC_HAL_H265_GetStartCode_8530(_u4BSID[u4InstID], u4InstID) & 0x00ffff;
        u4NalType[i] = ((u4Temp>>9) & 0x3f); // bit 9 10 11 12 13 14
        u4NalStartOffset[i] = u4VDEC_HAL_H265_ReadRdPtr(_u4BSID[u4InstID], u4InstID, (UINT32)_pucVFifo[u4InstID], &u4Bits);
        u4NalStartOffset[i] +=  u4Bits>>3;  //precise to byte
        printk("u4NalType[%d] = %d; u4NalStartOffset[%d] = 0x%08X\n", i, u4NalType[i], i, u4NalStartOffset[i] );
        if ( i>0  ){
            if( u4NalStartOffset[i] < u4NalStartOffset[i-1] )
                break;
        }
    }

    // reset HW 
    i4VDEC_HAL_H265_InitVDecHW(u4InstID);
    rH265BSInitPrm.u4VFifoSa = (UINT32)_pucVFifo[u4InstID];
    rH265BSInitPrm.u4VFifoEa = (UINT32)_pucVFifo[u4InstID] + V_FIFO_SZ;
    rH265BSInitPrm.u4VLDRdPtr = (UINT32)_pucVFifo[u4InstID];

#ifndef  RING_VFIFO_SUPPORT
    rH265BSInitPrm.u4VLDWrPtr = (UINT32)_pucVFifo[u4InstID] + V_FIFO_SZ;
#else
    rH265BSInitPrm.u4VLDWrPtr = (UINT32)_pucVFifo[u4InstID] + ((_u4LoadBitstreamCnt[u4InstID]%2)?(V_FIFO_SZ):(V_FIFO_SZ>>1));
#endif
    rH265BSInitPrm.u4PredSa = /*PHYSICAL*/((UINT32)_pucPredSa[u4InstID]);
    i4VDEC_HAL_H265_InitBarrelShifter(_u4BSID[u4InstID], u4InstID, &rH265BSInitPrm);

}


// *********************************************************************
// Function    : UINT32 vHEVCSearchRealPic(UINT32 u4InstID)
// Description : Search for the real pic then to dec
// Parameter   : None
// Return      : UINT32
// *********************************************************************
UINT32 vHEVCSearchRealPic(UINT32 u4InstID)
{
  UINT32 u4RetVal = PARSE_OK;
  UINT32 u4PreIDRPicCnt = 0;
  UINT32 u4RollBackBitCnt,u4RollBackPrePtr, u4RollBackBC;
  UINT32 u4RollBackBitCnt_temp, u4RollBackPrePtr_temp, u4RollBackBC_temp;
  VDEC_INFO_H265_BS_INIT_PRM_T rH265PreBSInitPrm, rH265PreBSInitPrm_temp;
  UINT32 u4Temp;
  BOOL fgForbidenZeroBits;
  VDEC_INFO_H265_BS_INIT_PRM_T rH265BSInitPrm = {0,0,0,0,0};
  UINT32 u4Bits = 0;    
  BOOL bSkipToDecodeFlag = 0;
  
  do
  {
    
    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
        printk("\n[INFO] vHEVCSearchRealPic Parsing start!!!============================= \n\n" );
    #endif
/*
    printk("[ERROR] &u4Bits:  0x%08x==========!!\n", &u4Bits);

    if (_u4PicCnt[u4InstID]==9){
        vVDEC_HAL_H265_VDec_DumpReg(u4InstID, 0);
    }

    printk("[ERROR] u4Bits:  0x%08x==========!!\n", u4Bits);
    printk("[ERROR] Alloc  rH265BSInitPrm.u4VLDRdPtr:  0x%08x; &rH265BSInitPrm:  0x%08x==========!!\n", rH265BSInitPrm.u4VLDRdPtr, &rH265BSInitPrm);
 */
    
    if (_u4CurrPicStartAddr[1] != 0){
        _u4CurrPicStartAddr[u4InstID]  = _u4CurrPicStartAddr[1];
        _u4CurrPicStartAddr[1] = 0;  //reset for looping until real picture
    } else {
        _u4CurrPicStartAddr[u4InstID] = u4VDEC_HAL_H265_ReadRdPtr(_u4BSID[u4InstID], u4InstID, (UINT32)_pucVFifo[u4InstID], &u4Bits);
    }
    rH265BSInitPrm.u4VFifoSa = (UINT32)_pucVFifo[u4InstID];
    rH265BSInitPrm.u4VFifoEa = (UINT32)_pucVFifo[u4InstID] + V_FIFO_SZ;
    rH265BSInitPrm.u4VLDRdPtr = (UINT32)_pucVFifo[u4InstID] + _u4CurrPicStartAddr[u4InstID];

  #ifndef  RING_VFIFO_SUPPORT
    rH265BSInitPrm.u4VLDWrPtr = (UINT32)_pucVFifo[u4InstID] + V_FIFO_SZ;
  #else
    rH265BSInitPrm.u4VLDWrPtr = (UINT32)_pucVFifo[u4InstID] + ((_u4LoadBitstreamCnt[u4InstID]%2)?(V_FIFO_SZ):(V_FIFO_SZ/21));
  #endif
    rH265BSInitPrm.u4PredSa = /*PHYSICAL*/((UINT32)_pucPredSa[u4InstID]);


    i4VDEC_HAL_H265_InitVDecHW(u4InstID);

    if (bSkipToDecodeFlag == 1){
        i4VDEC_HAL_H265_InitBarrelShifter(_u4BSID[u4InstID], u4InstID, &rH265PreBSInitPrm);
       _u4PicCnt[u4InstID] = u4PreIDRPicCnt;
       _u4VerBitCount[u4InstID] = u4RollBackBitCnt;
       _u4PrevPtr[u4InstID] = u4RollBackPrePtr;
       _u4LoadBitstreamCnt[u4InstID] = u4RollBackBC;
       bSkipToDecodeFlag = 0;
    } else {
        i4VDEC_HAL_H265_InitBarrelShifter(_u4BSID[u4InstID], u4InstID, &rH265BSInitPrm);
    }

    u4Temp = u4VDEC_HAL_H265_GetStartCode_PicStart(_u4BSID[u4InstID], u4InstID) & 0xffff;
    
    // Check read pointer after get start code 
    //u4VDEC_HAL_H265_ReadRdPtr(_u4BSID[u4InstID], u4InstID, (UINT32)_pucVFifo[u4InstID], &u4Bits);
    
    //printk("\n[ERROR] ER1!!!&fgForbidenZeroBits 0x%08x============================= \n", &fgForbidenZeroBits );
    fgForbidenZeroBits = ((u4Temp >> 15) & 0x01); // bit 15
    if(fgForbidenZeroBits != 0)
    {
        vHEVCErrInfo(VER_FORBIDEN_ERR);
    }

    _ucNalUnitType[u4InstID] = ((u4Temp>>9) & 0x3f); // bit 9 10 11 12 13 14
    _u4NuhLayerId[u4InstID] =  ((u4Temp>>3) & 0x3f); // bit 3 4 5 6 7 8
    _u4NuhTemporalId[u4InstID] = (u4Temp & 0x07)-1; // bit 0 1 2
    //printk("\n[ERROR] ER2!!!_ucNalUnitType[u4InstID] 0x%08x============================= \n", _ucNalUnitType[u4InstID] );

    printk("//================================ pic# %d ================================\n", _u4PicCnt[u4InstID]);
    printk("//[INFO] NalHdr: 0x%04X  _ucNalUnitType=%d  _u4NuhLayerId=%d  _u4NuhTemporalId=%d\n",u4Temp,_ucNalUnitType[u4InstID], _u4NuhLayerId[u4InstID], _u4NuhTemporalId[u4InstID] );
    printk("//==========================================================================\n");

    u4Temp = u4VDEC_HAL_H265_ShiftGetBitStream(_u4BSID[u4InstID], u4InstID, 0);
    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
        printk("[INFO] NAL Current input window: 0x%08x \n",u4Temp );
    #endif      
    
    switch(_ucNalUnitType[u4InstID])
    {
        case NAL_UNIT_CODED_SLICE_BLA:
        case NAL_UNIT_CODED_SLICE_BLANT:
        case NAL_UNIT_CODED_SLICE_BLA_N_LP:
        case NAL_UNIT_CODED_SLICE_IDR:
        case NAL_UNIT_CODED_SLICE_IDR_N_LP:
      
            u4PreIDRPicCnt = _u4SkipPicNum[u4InstID];       
            //printk("\n[ERROR] ER4!!!&u4PreIDRPicCnt 0x%08x============================= \n", &u4PreIDRPicCnt );

            rH265PreBSInitPrm = rH265PreBSInitPrm_temp;     // get IDR set roll back point
            //printk("\n[ERROR] ER5!!!&rH265PreBSInitPrm 0x%08x============================= \n", &rH265PreBSInitPrm );
            u4RollBackBitCnt = u4RollBackBitCnt_temp;
            u4RollBackPrePtr = u4RollBackPrePtr_temp;
            u4RollBackBC = u4RollBackBC_temp;
            //IDR update initBS info & Fall through
        case NAL_UNIT_CODED_SLICE_CRA:
        case NAL_UNIT_CODED_SLICE_TRAIL_R:
        case NAL_UNIT_CODED_SLICE_TRAIL_N:
        case NAL_UNIT_CODED_SLICE_TLA:
        case NAL_UNIT_CODED_SLICE_TSA_N:
        case NAL_UNIT_CODED_SLICE_STSA_R:
        case NAL_UNIT_CODED_SLICE_STSA_N:
        case NAL_UNIT_CODED_SLICE_RADL_N:
        case NAL_UNIT_CODED_SLICE_DLP:
        case NAL_UNIT_CODED_SLICE_RASL_N:
        case NAL_UNIT_CODED_SLICE_TFD:
            if ( _u4SkipPicNum[u4InstID] >= _u4StartCompPicNum[u4InstID] ){
                u4RetVal = vHEVCParseSliceHeader(u4InstID);
            } else {
                printk("[INFO] Skip frame: %d, Start frame: %d, PeIDRPicCnt: %d\n",_u4SkipPicNum[u4InstID], _u4StartCompPicNum[u4InstID], u4PreIDRPicCnt );
                _u4SkipPicNum[u4InstID]++;
                if (_u4SkipPicNum[u4InstID] == _u4StartCompPicNum[u4InstID] ){
                   bSkipToDecodeFlag = 1;
                }
                UINT32 u4FIFOMidpoint = (UINT32)_pucVFifo[u4InstID] + V_FIFO_SZ/2;
                if ( (rH265PreBSInitPrm.u4VLDRdPtr < u4FIFOMidpoint && rH265BSInitPrm.u4VLDRdPtr > u4FIFOMidpoint)
                    ||(rH265PreBSInitPrm.u4VLDRdPtr > u4FIFOMidpoint && rH265BSInitPrm.u4VLDRdPtr < u4FIFOMidpoint) ){
                    vH265RingFIFO_read( u4InstID, 0 );
                } else {
                    vH265RingFIFO_read( u4InstID, 1 );
                }
                _ucNalUnitType[u4InstID] = NAL_UNIT_SKIPP;
            }
            break;
        case NAL_UNIT_SEI:
            vHEVCVerifySEI_Rbsp(u4InstID);
            break;
        case NAL_UNIT_SPS:
            rH265PreBSInitPrm_temp = rH265BSInitPrm;        // Skip mode roll back point
            u4RollBackBitCnt_temp = _u4VerBitCount[u4InstID];
            u4RollBackPrePtr_temp = _u4PrevPtr[u4InstID];
            u4RollBackBC_temp =_u4LoadBitstreamCnt[u4InstID];
            u4RetVal = vHEVCVerifySPS_Rbsp(u4InstID);
            break;      
        case NAL_UNIT_PPS:
            u4RetVal = vHEVCVerifyPPS_Rbsp(u4InstID);
            break;         
        default:
            break;
    }

    if (u4RetVal != PARSE_OK){
        break;
    }
  }while(!((0<=_ucNalUnitType[u4InstID] && _ucNalUnitType[u4InstID] <=9)||(16<=_ucNalUnitType[u4InstID] && _ucNalUnitType[u4InstID] <=21) ));

  return u4RetVal;
}


// *********************************************************************
// Function    : void vHEVCVerifyPTL_Rbsp(UINT32 u4InstID)
// Description : Handle SPS PTL header
// Parameter   : None
// Return      : None
// *********************************************************************

void vHEVCVerifyPTL_Rbsp( UINT32 u4InstID, pH265_PTL_Data pPTL, BOOL profilePresentFlag, INT32 maxNumSubLayersMinus1 )
{    

    int i4tmp = 0;
    int i4tmp2 = 0;
    int i = 0;
    int u4Idx  = 0;
    int pfLogFile;

    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"================== PTL parameters ==================\n" );
    #endif

    // general level
    if (pPTL->bProfilePresentFlag){
        pPTL->generalPTL.u4ProfileSpace = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 2);//u(2)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"general_profile_space[] %d\n", pPTL->generalPTL.u4ProfileSpace);
        #endif
        pPTL->generalPTL.bTierFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"general_tier_flag[] %d\n", pPTL->generalPTL.bTierFlag);
        #endif
        pPTL->generalPTL.u4ProfileIdc= u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 5);//u(5)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"general_profile_idc[] %d\n", pPTL->generalPTL.u4ProfileIdc);
        #endif
        for( i = 0; i < 32; i++)
        {
            pPTL->generalPTL.bProfileCompatibilityFlag[i] = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"general_profile_compatibility_flag[][%d] %d\n", i, pPTL->generalPTL.bProfileCompatibilityFlag[i] );
            #endif
        }

        pPTL->generalPTL.bProgressiveSourceFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                    DBG_H265_PRINTF(pfLogFile,"general_progressive_source_flag %d\n", pPTL->generalPTL.bProgressiveSourceFlag);
        #endif
        pPTL->generalPTL.bInterlacedSourceFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                    DBG_H265_PRINTF(pfLogFile,"general_interlaced_source_flag %d\n", pPTL->generalPTL.bInterlacedSourceFlag);
        #endif
        pPTL->generalPTL.bNonPackedConstraintFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                    DBG_H265_PRINTF(pfLogFile,"general_non_packed_constraint_flag %d\n", pPTL->generalPTL.bNonPackedConstraintFlag);
        #endif
        pPTL->generalPTL.bFrameOnlyConstraintFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                    DBG_H265_PRINTF(pfLogFile,"general_frame_only_constraint_flag %d\n", pPTL->generalPTL.bFrameOnlyConstraintFlag);
        #endif

        i4tmp = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 32);   //u(44)
        i4tmp2 = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 12);
        
        if ( i4tmp ||i4tmp2 ){                    
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"reserved_zero_44bits[] %08x%08x\n", i4tmp2, i4tmp  );
            #endif
            H265_DRV_PARSE_SET_ERR_RET(-13);
        }
        
    }
    
    pPTL->generalPTL.u4LevelIdc= u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 8);//u(8)
    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"general_level_idc %d\n", pPTL->generalPTL.u4LevelIdc);
    #endif

    
    for(u4Idx = 0; u4Idx < maxNumSubLayersMinus1; u4Idx++)
    {
        pPTL->bSubLayerProfilePresentFlag[u4Idx] = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"sub_layer_profile_present_flag[%d] %d\n", u4Idx, pPTL->bSubLayerProfilePresentFlag[u4Idx]);
        #endif
        pPTL->bSubLayerLevelPresentFlag[u4Idx] = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"sub_layer_level_present_flag[%d] %d\n", u4Idx, pPTL->bSubLayerLevelPresentFlag[u4Idx]);
        #endif
    }

    if ( 0 < maxNumSubLayersMinus1 ){
        for(u4Idx = maxNumSubLayersMinus1; u4Idx < 8; u4Idx++)
        {
            if (i4tmp = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 2) ){  //u(2)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"reserved_zero_2bits[] %d\n", i4tmp );
            #endif
                H265_DRV_PARSE_SET_ERR_RET(-14);
            }
        }
    }

    // sub level 
    for(u4Idx = 0; u4Idx < maxNumSubLayersMinus1; u4Idx++)
    {
        if( pPTL->bProfilePresentFlag && pPTL->bSubLayerProfilePresentFlag[u4Idx] )
        {
            pPTL->subLayerPTL[u4Idx].u4ProfileSpace = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 2);//u(2)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"sub_layer_profile_space[] %d\n", pPTL->subLayerPTL[u4Idx].u4ProfileSpace);
            #endif
            pPTL->subLayerPTL[u4Idx].bTierFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"sub_layer_tier_flag[] %d\n", pPTL->subLayerPTL[u4Idx].bTierFlag);
            #endif
            pPTL->subLayerPTL[u4Idx].u4ProfileIdc= u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 5);//u(5)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"sub_layer_profile_idc[] %d\n", pPTL->subLayerPTL[u4Idx].u4ProfileIdc);
            #endif
            for( i = 0; i < 32; i++)
            {
                pPTL->subLayerPTL[u4Idx].bProfileCompatibilityFlag[i] = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                    DBG_H265_PRINTF(pfLogFile,"sub_layer_profile_compatibility_flag[][%d] %d\n", i,pPTL->subLayerPTL[u4Idx].bProfileCompatibilityFlag[i] );
            #endif
            }

            pPTL->subLayerPTL[u4Idx].bProgressiveSourceFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                        DBG_H265_PRINTF(pfLogFile,"sub_layer_progressive_source_flag %d\n", pPTL->subLayerPTL[u4Idx].bProgressiveSourceFlag);
            #endif
            pPTL->subLayerPTL[u4Idx].bInterlacedSourceFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                        DBG_H265_PRINTF(pfLogFile,"sub_layer_interlaced_source_flag %d\n", pPTL->subLayerPTL[u4Idx].bInterlacedSourceFlag);
            #endif
            pPTL->subLayerPTL[u4Idx].bNonPackedConstraintFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                        DBG_H265_PRINTF(pfLogFile,"sub_layer_non_packed_constraint_flag %d\n", pPTL->subLayerPTL[u4Idx].bNonPackedConstraintFlag);
            #endif
            pPTL->subLayerPTL[u4Idx].bFrameOnlyConstraintFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                        DBG_H265_PRINTF(pfLogFile,"sub_layer_frame_only_constraint_flag %d\n", pPTL->subLayerPTL[u4Idx].bFrameOnlyConstraintFlag);
            #endif
            
            i4tmp = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 32);   //u(44)
            i4tmp2 = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 12);
            
            if ( i4tmp ||i4tmp2 ){                    
                #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                    DBG_H265_PRINTF(pfLogFile,"reserved_zero_44bits[] %08x%08x\n", i4tmp2, i4tmp  );
                #endif
                H265_DRV_PARSE_SET_ERR_RET(-13);
            }

       }
       if( pPTL->bSubLayerLevelPresentFlag[u4Idx] )
       {
            pPTL->subLayerPTL[u4Idx].u4LevelIdc= u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 8);//u(8)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"sub_layer_level_idc[%d] %d\n", u4Idx, pPTL->subLayerPTL[u4Idx].u4LevelIdc);
        #endif
       }
    }

    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"==================== PTL end =====================\n" );
    #endif

}


// *********************************************************************
// Function    :void vHEVCInitSigLastScan(UINT32* pBuffD, UINT32* pBuffH, UINT32* pBuffV, INT32 iWidth, INT32 iHeight)
// Description : Init scan list
// Parameter   : None
// Return      : None
// *********************************************************************

void vHEVCInitSigLastScan(UINT32* pBuffD, UINT32* pBuffH, UINT32* pBuffV, INT32 iWidth, INT32 iHeight)
{
      const UINT32  uiNumScanPos  = (UINT32) iWidth * iWidth;
      UINT32 uiNextScanPos = 0;
      UINT32 uiScanLine;

      if( iWidth < 16 )
      {
        UINT32* pBuffTemp = pBuffD;
        if( iWidth == 8 )
        {
          pBuffTemp = H265_sigLastScanCG32x32;
        }

        for(uiScanLine = 0; uiNextScanPos < uiNumScanPos; uiScanLine++ )
        {
          INT32    iPrimDim  = (INT32) uiScanLine;
          INT32    iScndDim  = 0;
          while( iPrimDim >= iWidth )
          {
            iScndDim++;
            iPrimDim--;
          }
          while( iPrimDim >= 0 && iScndDim < iWidth )
          {
            pBuffTemp[ uiNextScanPos ] = iPrimDim * iWidth + iScndDim ;
            uiNextScanPos++;
            iScndDim++;
            iPrimDim--;
          }
        }
      }


      if( iWidth > 4 )
      {
        UINT32 uiNumBlkSide = iWidth >> 2;
        UINT32 uiNumBlks    = uiNumBlkSide * uiNumBlkSide;
        UINT32 log2Blk      = (UINT32)((INT32)H265_aucConvertToBit[ uiNumBlkSide ] + 1);
        UINT32 uiBlk;

        for( uiBlk = 0; uiBlk < uiNumBlks; uiBlk++ )
        {
          uiNextScanPos   = 0;
          UINT32 initBlkPos = H265_auiSigLastScan[ SCAN_DIAG ][ log2Blk ][ uiBlk ];

          if( iWidth == 32 )
          {
            initBlkPos = H265_sigLastScanCG32x32[ uiBlk ];
          }
          UINT32 offsetY    = initBlkPos / uiNumBlkSide;
          UINT32 offsetX    = initBlkPos - offsetY * uiNumBlkSide;
          UINT32 offsetD    = 4 * ( offsetX + offsetY * iWidth );
          UINT32 offsetScan = 16 * uiBlk;
          for( uiScanLine = 0; uiNextScanPos < 16; uiScanLine++ )
          {
            INT32    iPrimDim  = (INT32)uiScanLine;
            INT32    iScndDim  = 0;
            while( iPrimDim >= 4 )
            {
              iScndDim++;
              iPrimDim--;
            }
            while( iPrimDim >= 0 && iScndDim < 4 )
            {
              pBuffD[ uiNextScanPos + offsetScan ] = iPrimDim * iWidth + iScndDim + offsetD;
              uiNextScanPos++;
              iScndDim++;
              iPrimDim--;
            }
          }
        }
      }

      UINT32 uiCnt = 0;
      INT32 blkX, blkY;
      INT32 iX, iY, x, y;
      
      if( iWidth > 2 )
      {
        UINT32 numBlkSide = iWidth >> 2;
        for(blkY=0; blkY < numBlkSide; blkY++)
        {
          for(blkX=0; blkX < numBlkSide; blkX++)
          {
            UINT32 offset = blkY * 4 * iWidth + blkX * 4;
            for(y=0; y < 4; y++)
            {
              for(x=0; x < 4; x++)
              {
                pBuffH[uiCnt] = y*iWidth + x + offset;
                uiCnt ++;
              }
            }
          }
        }

        uiCnt = 0;
        for(blkX=0; blkX < numBlkSide; blkX++)
        {
          for(blkY=0; blkY < numBlkSide; blkY++)
          {
            UINT32 offset = blkY * 4 * iWidth + blkX * 4;
            for(x=0; x < 4; x++)
            {
              for(y=0; y < 4; y++)
              {
                pBuffV[uiCnt] = y*iWidth + x + offset;
                uiCnt ++;
              }
            }
          }
        }
      }
      else
      {
        for(iY=0; iY < iHeight; iY++)
        {
          for(iX=0; iX < iWidth; iX++)
          {
            pBuffH[uiCnt] = iY*iWidth + iX;
            uiCnt ++;
          }
        }

        uiCnt = 0;
        for(iX=0; iX < iWidth; iX++)
        {
          for(iY=0; iY < iHeight; iY++)
          {
            pBuffV[uiCnt] = iY*iWidth + iX;
            uiCnt ++;
          }
        }    
      }
}



// *********************************************************************
// Function    :void vHEVCInitROM(UINT32 u4InstID)
// Description : Set ROM for SL
// Parameter   : None
// Return      : None
// *********************************************************************

void vHEVCInitROM(UINT32 u4InstID)
{
    INT32 i, c;

    // H265_aucConvertToBit[ x ]: log2(x/4), if x=4 -> 0, x=8 -> 1, x=16 -> 2, ...

    for(i=0; i<MAX_CU_SIZE+1 ; i++)
    {
        H265_aucConvertToBit[i] = -1;
    }
    
    c=0;
    for ( i=4; i<MAX_CU_SIZE; i*=2 )
    {
        H265_aucConvertToBit[i] = c;
        c++;
    }
    H265_aucConvertToBit[i] = c;

    c=2;
    for ( i=0; i<MAX_CU_DEPTH; i++ )
    {
        if (H265_auiSigLastScan[0][i] == NULL){ H265_auiSigLastScan[0][i] = (UINT32*)vmalloc(c*c*sizeof(UINT32)); } 
        if (H265_auiSigLastScan[1][i] == NULL){ H265_auiSigLastScan[1][i] = (UINT32*)vmalloc(c*c*sizeof(UINT32)); }
        if (H265_auiSigLastScan[2][i] == NULL){ H265_auiSigLastScan[2][i] = (UINT32*)vmalloc(c*c*sizeof(UINT32)); } 

        vHEVCInitSigLastScan( H265_auiSigLastScan[0][i], H265_auiSigLastScan[1][i], H265_auiSigLastScan[2][i], c, c);
        c <<= 1;
    }  
}




// *********************************************************************
// Function    :INT32 * vHEVCVerifyGetSLDefaultAddress( INT32 sizeId, INT32 listId)
// Description : Handle SL default address
// Parameter   : None
// Return      : (INT32 *) default SL address
// *********************************************************************

INT32 * vHEVCVerifyGetSLDefaultAddress(INT32 sizeId, INT32 listId)
{
  INT32 *src = 0;
  switch(sizeId)
  {
    case SCALING_LIST_4x4:
      src = H265_quantTSDefault4x4;
      break;
    case SCALING_LIST_8x8:
      src = (3>listId) ? H265_quantIntraDefault8x8 : H265_quantInterDefault8x8;
      break;
    case SCALING_LIST_16x16:
      src = (3>listId) ? H265_quantIntraDefault8x8 : H265_quantInterDefault8x8;
      break;
    case SCALING_LIST_32x32:
      src = (1>listId) ? H265_quantIntraDefault8x8 : H265_quantInterDefault8x8;
      break;
    default:
      src = NULL;
      break;
  }
  return src;
}


// *********************************************************************
// Function    : UINT32 vHEVCVerifySL_Rbsp( UINT32 u4InstID, pH265_SL_Data scalingList )
// Description : Handle SPS SL header
// Parameter   : None
// Return      : UINT32
// *********************************************************************

UINT32  vHEVCVerifySL_Rbsp( UINT32 u4InstID, pH265_SL_Data scalingList )
{
            
    UINT32  code, sizeId, listId;
    BOOL scalingListPredModeFlag;
    int i, i4tmp, nextCoef ,refListId, coefNum, listSize, pfLogFile;

    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"================== SL parameters ==================\n" );
    #endif

    //for each size
    for(sizeId = 0; sizeId < SCALING_LIST_SIZE_NUM; sizeId++)
    {
        for(listId = 0; listId <  H265_scalingListNum[sizeId]; listId++)
        {
            scalingList->bScalingListPredModeFlag[sizeId][listId] = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"scaling_list_pred_mode_flag %d\n", scalingList->bScalingListPredModeFlag[sizeId][listId]  );
            #endif
            scalingListPredModeFlag = ( scalingList->bScalingListPredModeFlag[sizeId][listId] ) ? 1 : 0;
            if( 1 != scalingListPredModeFlag ) //Copy Mode
            {
            
                i4tmp  = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
                #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                    DBG_H265_PRINTF(pfLogFile,"scaling_list_pred_matrix_id_delta %d\n", i4tmp );
                #endif
                if ((int)listId-i4tmp >= 0 && (int)listId-i4tmp < H265_scalingListNum[sizeId] ){
                    scalingList->u4RefMatrixId[sizeId][listId] = (UINT32)((int)listId-i4tmp);
                } else if ( (int)listId-i4tmp >= H265_scalingListNum[sizeId] ){
                    printk ("[ERROR] u4RefMatrixId(%d) out of ramge 0 to H265_scalingListNum[%d]-1 (%d)!!!!!\n",  (int)listId-i4tmp, sizeId, H265_scalingListNum[sizeId]-1);
                    return SL_SYNTAX_ERROR;
                } else {
                    printk ("[ERROR] u4RefMatrixId(%d) < 0 !!!!!\n",  (int)listId-i4tmp);
                    return SL_SYNTAX_ERROR;
                }

                if( sizeId > SCALING_LIST_8x8 )
                {
                    scalingList->i4ScalingListDC[sizeId][listId] = ((listId == scalingList->u4RefMatrixId[sizeId][listId])? 16 : scalingList->i4ScalingListDC[sizeId][scalingList->u4RefMatrixId[sizeId][listId]] );
                }

                refListId = scalingList->u4RefMatrixId[sizeId][listId]; 
                listSize = (MAX_MATRIX_COEF_NUM < H265_scalingListSize[sizeId])? MAX_MATRIX_COEF_NUM : H265_scalingListSize[sizeId];
                memcpy(scalingList->pScalingListDeltaCoef[sizeId][listId],( (listId == refListId)? 
                    vHEVCVerifyGetSLDefaultAddress(sizeId, refListId): scalingList->pScalingListDeltaCoef[sizeId][refListId]) , sizeof(INT32)*listSize);

           } else{   //DPCM Mode
                int i,coefNum = min(MAX_MATRIX_COEF_NUM,(int)H265_scalingListSize[sizeId]);
                int data;
                int scalingListDcCoefMinus8 = 0;
                int nextCoef = SCALING_LIST_START_VALUE;
                UINT32* scan  = (sizeId == 0) ? H265_auiSigLastScan [SCAN_DIAG] [1] :  H265_sigLastScanCG32x32;
                INT32* dst = scalingList->pScalingListDeltaCoef[sizeId][listId];

                if( sizeId > SCALING_LIST_8x8 )
                {
                    i4tmp  = i4VDEC_HAL_H265_SeCodeNum(_u4BSID[u4InstID], u4InstID);//se(v)
                    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                        DBG_H265_PRINTF(pfLogFile,"scaling_list_dc_coef_minus8 %d\n",i4tmp );
                    #endif
                    scalingList->i4ScalingListDC[sizeId][listId] = i4tmp+8;
                    nextCoef = scalingList->i4ScalingListDC[sizeId][listId];
                }

                for(i = 0; i < coefNum; i++)
                {
                    i4tmp = i4VDEC_HAL_H265_SeCodeNum(_u4BSID[u4InstID], u4InstID);//se(v)
                    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                        DBG_H265_PRINTF(pfLogFile,"scaling_list_delta_coef %d\n", i4tmp);
                    #endif
                    nextCoef = (nextCoef + i4tmp + 256 ) % 256;
                    dst[scan[i]]= nextCoef;
                }
           }
        }
    }

    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"==================== SL end =====================\n" );
    #endif
    return PARSE_OK;
}

// *********************************************************************
// Function    : UINT32  vHEVCVerifyRPS_Rbsp( UINT32 u4InstID, pH265_SPS_Data SPS, pH265_RPS_Data pRPS ,UINT32 index )
// Description : Handle SPS RPS header
// Parameter   : None
// Return      : UINT32
// *********************************************************************

UINT32  vHEVCVerifyRPS_Rbsp( UINT32 u4InstID, H265_SPS_Data * pSPS, pH265_RPS_Data pRPS ,UINT32 index )
{
 
    UINT32 code, bit, refIndex, refIdc;
    BOOL interRPSPred, used;
    pH265_RPS_Data refRPS = NULL;
    
    int k = 0;
    int k0 = 0;
    int k1 = 0;
    int deltaRPS, deltaPOC, i, j;
    int temp, numNegPics;
    int prev, poc,pfLogFile;

    //reset pRPS
    pRPS->u4NumberOfNegativePictures = 0;
    pRPS->u4NumberOfPositivePictures = 0;
    pRPS->u4NumberOfPictures = 0;
    pRPS->u4NumberOfLongtermPictures = 0;
    pRPS->u4DeltaRIdxMinus1 = 0;
    pRPS->u4DeltaRPS = 0;
    pRPS->bInterRPSPrediction = 0;
    pRPS->u4NumRefIdc = 0;

    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
    if ( index == 0 )
        DBG_H265_PRINTF(pfLogFile,"================== RPS parameters ==================\n" );
    #endif

    if ( 0 < index ){
        pRPS->bInterRPSPrediction = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"inter_ref_pic_set_prediction_flag[%d] %d\n", index, pRPS->bInterRPSPrediction );
        #endif
    } else {
        pRPS->bInterRPSPrediction = 0;
    }
    interRPSPred = pRPS->bInterRPSPrediction;
    
    if ( interRPSPred ){
        if( index ==  pSPS->u4NumShortTermRefPicSets ){
            code = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"delta_idx_minus1[%d] %d\n", index, code );
            #endif
        } else {
            code = 0;
        }
           
        if (code > index-1){
            DBG_H265_PRINTF(pfLogFile, "[ERROR] delta_idx_minus1[%d](%d) shall not be larger than index(%d) -1", index, code,index );
            H265_DRV_PARSE_SET_ERR_RET(-1);
            return RPS_SYNTAX_ERROR;
        }
        refIndex = index -1 - code;
        if ( refIndex > index -1 || refIndex < 0 ){
            DBG_H265_PRINTF(pfLogFile, "[ERROR] refIndex[%d](%d) must belong to range 0, index-1", index, refIndex );
            H265_DRV_PARSE_SET_ERR_RET(-2);
            return RPS_SYNTAX_ERROR;
        }
        refRPS = pSPS->pShortTermRefPicSets[refIndex];

        if ( refRPS==NULL ){
            DBG_H265_PRINTF(pfLogFile,"[ERROR] refRPS is NULL !!!\n" );
            return RPS_SYNTAX_ERROR;
        }


        bit = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"delta_rps_sign[%d] %d\n", index, bit );
        #endif
        code = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"abs_delta_rps_minus1[%d] %d\n", index, code );
        #endif
        deltaRPS = (1 - (bit<<1)) * (code + 1); // delta_RPS

        _rH265PicInfo[u4InstID].i4StrNumDeltaPocs = refRPS->u4NumberOfNegativePictures+refRPS->u4NumberOfPositivePictures;

        for ( i = 0; i <= (refRPS->u4NumberOfNegativePictures+refRPS->u4NumberOfPositivePictures); i++ ){
            bit = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"used_by_curr_pic_flag[%d][%d] %d\n", index, i, bit );
            #endif
            refIdc = bit;
            if (refIdc == 0) {
                bit = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
                #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                    DBG_H265_PRINTF(pfLogFile,"use_delta_flag[%d][%d] %d\n", index, i , bit );     //second bit is "1" if Idc is 2, "0" otherwise.
                #endif
                refIdc = bit<<1; //second bit is "1" if refIdc is 2, "0" if refIdc = 0.
            }
            if (refIdc == 1 || refIdc == 2){
                deltaPOC = deltaRPS + (( i < (refRPS->u4NumberOfNegativePictures+refRPS->u4NumberOfPositivePictures) )? refRPS->i4DeltaPOC[i]  : 0);
                pRPS->i4DeltaPOC[k] = deltaPOC;
                pRPS->bUsed[k] = ( refIdc == 1 );
                
                if (deltaPOC < 0) {
                    k0++;
                } else {
                    k1++;
                }
                k++;
            }
            pRPS->u4RefIdc[i] = refIdc;
        }

        pRPS->u4NumRefIdc = refRPS->u4NumberOfPictures + 1;
        pRPS->u4NumberOfPictures = k;
        pRPS->u4NumberOfNegativePictures = k0;
        pRPS->u4NumberOfPositivePictures = k1;

        // sort in increasing order (smallest first)
        for( j=1; j < pRPS->u4NumberOfPictures; j++)
        { 
            deltaPOC = pRPS->i4DeltaPOC[j];
            used = pRPS->bUsed[j];
             for ( k=j-1; k >= 0; k--)
            {
                temp = pRPS->i4DeltaPOC[k];
                if (deltaPOC < temp){
                    pRPS->i4DeltaPOC[k+1] = temp;
                    pRPS->bUsed[k+1] = pRPS->bUsed[k];
                    pRPS->i4DeltaPOC[k] = deltaPOC;
                    pRPS->bUsed[k] = used;
                }
            }
        }
        // flip the negative values to largest first
        numNegPics = pRPS->u4NumberOfNegativePictures;
        for( j=0, k=numNegPics-1; j < numNegPics>>1; j++, k--)
        { 
            deltaPOC = pRPS->i4DeltaPOC[j];
            used = pRPS->bUsed[j];
            pRPS->i4DeltaPOC[j] = pRPS->i4DeltaPOC[k];
            pRPS->bUsed[j] = pRPS->bUsed[k] ;
            pRPS->i4DeltaPOC[k] = deltaPOC;
            pRPS->bUsed[k] = used;
        }

    }else{

        pRPS->u4NumberOfNegativePictures = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"num_negative_pics[%d] %d\n", index, pRPS->u4NumberOfNegativePictures );
        #endif
        pRPS->u4NumberOfPositivePictures = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"num_positive_pics[%d] %d\n", index, pRPS->u4NumberOfPositivePictures );
        #endif
        
        _rH265PicInfo[u4InstID].i4StrNumNegPosPics = pRPS->u4NumberOfNegativePictures + pRPS->u4NumberOfPositivePictures;
        _rH265PicInfo[u4InstID].i4MaxStrNumNegPosPics = (_rH265PicInfo[u4InstID].i4MaxStrNumNegPosPics<_rH265PicInfo[u4InstID].i4StrNumNegPosPics)?
                                                                _rH265PicInfo[u4InstID].i4StrNumNegPosPics:_rH265PicInfo[u4InstID].i4MaxStrNumNegPosPics;
        if ( _rH265PicInfo[u4InstID].i4StrNumNegPosPics > 32 ){
            #if DEBUG_LEVEL >= DBG_LEVEL_BUG1
                DBG_H265_PRINTF(pfLogFile,"[ERROR] Number of shorterm reference pic > 32 !!!\n" );
            #endif
            return RPS_SYNTAX_ERROR;
        }
        
        prev = 0;
        for( j=0 ; j < pRPS->u4NumberOfNegativePictures; j++)
        {
            code = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"delta_poc_s0_minus1[%d][%d] %d\n", index, j, code );
            #endif
            poc = prev-code-1;
            prev = poc;
            pRPS->i4DeltaPOC[j] = poc;
            
            pRPS->bUsed[j] = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"used_by_curr_pic_s0_flag[%d][%d] %d\n", index, j, pRPS->bUsed[j] );
            #endif
        }
        prev = 0;
        for( j=pRPS->u4NumberOfNegativePictures; j < pRPS->u4NumberOfNegativePictures + pRPS->u4NumberOfPositivePictures; j++)
        {
            code = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"delta_poc_s1_minus1[%d][%d] %d\n", index, j, code );
            #endif
            poc = prev+code+1;
            prev = poc;
            pRPS->i4DeltaPOC[j] = poc;
            
            pRPS->bUsed[j] = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"used_by_curr_pic_s1_flag[%d][%d] %d\n", index, j, pRPS->bUsed[j] );
            #endif
        }

        pRPS->u4NumberOfPictures = pRPS->u4NumberOfNegativePictures + pRPS->u4NumberOfPositivePictures;
    }

    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
    if ( index == (pSPS->u4NumShortTermRefPicSets -1)  )
        DBG_H265_PRINTF(pfLogFile,"==================== RPS end =====================\n" );
    #endif
    return PARSE_OK;
    
}


// *********************************************************************
// Function    : void vHEVCVerifyHRD_Rbsp( UINT32 u4InstID, H265_HRD_Data* HRD, VAL_BOOL_T bCommonInfPresentFlag , UINT32 MaxTLayersMinus1 )
// Description : Handle VUI HRD header
// Parameter   : None
// Return      : None
// *********************************************************************
void vHEVCVerifyHRD_Rbsp( UINT32 u4InstID, H265_HRD_Data* HRD, BOOL bCommonInfPresentFlag , UINT32 MaxTLayersMinus1 )
{
    int i, j, nalOrVcl;
    
    if( bCommonInfPresentFlag )
    {
    HRD->bNalHrdParametersPresentFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"nal_hrd_parameters_present_flag %d\n", HRD->bNalHrdParametersPresentFlag  );
    #endif
    HRD->bVclHrdParametersPresentFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"vcl_hrd_parameters_present_flag %d\n", HRD->bVclHrdParametersPresentFlag  );
    #endif

        if( HRD->bNalHrdParametersPresentFlag ||HRD->bVclHrdParametersPresentFlag )
        {
            HRD->bSubPicCpbParamsPresentFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"sub_pic_cpb_params_present_flag %d\n", HRD->bSubPicCpbParamsPresentFlag);
            #endif
            if( HRD->bSubPicCpbParamsPresentFlag )
            {
                HRD->u4TickDivisorMinus2 = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 8);//u(8)
                #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                    DBG_H265_PRINTF(pfLogFile,"tick_divisor_minus2 %d\n", HRD->u4TickDivisorMinus2 );
                #endif
                HRD->u4DuCpbRemovalDelayLengthMinus1 = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 5);//u(5)
                #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                    DBG_H265_PRINTF(pfLogFile,"du_cpb_removal_delay_length_minus1 %d\n", HRD->u4DuCpbRemovalDelayLengthMinus1);
                #endif
                HRD->bSubPicCpbParamsInPicTimingSEIFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
                #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                    DBG_H265_PRINTF(pfLogFile,"sub_pic_cpb_params_in_pic_timing_sei_flag %d\n", HRD->bSubPicCpbParamsInPicTimingSEIFlag);
                #endif
                HRD->u4DpbOutputDelayDuLengthMinus1 = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 5);//u(5)
                #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                    DBG_H265_PRINTF(pfLogFile,"dpb_output_delay_du_length_minus1 %d\n", HRD->u4DpbOutputDelayDuLengthMinus1);
                #endif
            }
            HRD->u4BitRateScale = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 4);//u(4)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"bit_rate_scale %d\n", HRD->u4BitRateScale);
            #endif
            HRD->u4CpbSizeScale = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 4);//u(4)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"cpb_size_scale %d\n", HRD->u4CpbSizeScale);
            #endif
            if( HRD->bSubPicCpbParamsPresentFlag )
            {
                HRD->u4DucpbSizeScale = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 4);//u(4)
                #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                    DBG_H265_PRINTF(pfLogFile,"cpb_size_du_scale %d\n", HRD->u4DucpbSizeScale);
                #endif
            }

            HRD->u4InitialCpbRemovalDelayLengthMinus1 = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 5);//u(5)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"initial_cpb_removal_delay_length_minus1 %d\n", HRD->u4InitialCpbRemovalDelayLengthMinus1);
            #endif
            HRD->u4AuCpbRemovalDelayLengthMinus1 = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 5);//u(5)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"au_cpb_removal_delay_length_minus1 %d\n", HRD->u4AuCpbRemovalDelayLengthMinus1);
            #endif
            HRD->u4DpbOutputDelayLengthMinus1 = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 5);//u(5)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"dpb_output_delay_length_minus1 %d\n", HRD->u4DpbOutputDelayLengthMinus1);
            #endif
            
           }
      }


      for( i = 0; i <= MaxTLayersMinus1; i ++ )
      {
            HRD->rSubLayerHRD[i].bFixedPicRateFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"fixed_pic_rate_general_flag[%d] %d\n", i, HRD->rSubLayerHRD[i].bFixedPicRateFlag );
            #endif
            if( 1 != HRD->rSubLayerHRD[i].bFixedPicRateFlag )
            {
                HRD->rSubLayerHRD[i].bFixedPicRateWithinCvsFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
                #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                    DBG_H265_PRINTF(pfLogFile,"fixed_pic_rate_within_cvs_flag[%d] %d\n", i, HRD->rSubLayerHRD[i].bFixedPicRateWithinCvsFlag);
                #endif
            } else {
                HRD->rSubLayerHRD[i].bFixedPicRateWithinCvsFlag = 1;
            }
            HRD->rSubLayerHRD[i].bLowDelayHrdFlag = 0;
            HRD->rSubLayerHRD[i].u4CpbCntMinus1 = 0;

            if( HRD->rSubLayerHRD[i].bFixedPicRateWithinCvsFlag )
            {
                HRD->rSubLayerHRD[i].u4ElementalDurationInTcMinus1 = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
                #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                    DBG_H265_PRINTF(pfLogFile,"elemental_duration_in_tc_minus1[%d] %d\n", i, HRD->rSubLayerHRD[i].u4ElementalDurationInTcMinus1 );
                #endif
            } else { 
                HRD->rSubLayerHRD[i].bLowDelayHrdFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
                #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                    DBG_H265_PRINTF(pfLogFile,"low_delay_hrd_flag[%d] %d\n", i, HRD->rSubLayerHRD[i].bLowDelayHrdFlag);
                #endif
            }
                
            if ( 1 != HRD->rSubLayerHRD[i].bLowDelayHrdFlag )
            {
                HRD->rSubLayerHRD[i].u4CpbCntMinus1 = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
                #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                    DBG_H265_PRINTF(pfLogFile,"cpb_cnt_minus1[%d] %d\n", i, HRD->rSubLayerHRD[i].u4CpbCntMinus1);
                #endif
            }

            for( nalOrVcl = 0; nalOrVcl < 2; nalOrVcl ++ )
            {
                if( ( ( nalOrVcl == 0 ) && ( HRD->bNalHrdParametersPresentFlag ) ) ||
                    ( ( nalOrVcl == 1 ) && ( HRD->bVclHrdParametersPresentFlag ) ) )
                {
                      for( j = 0; j <= (HRD->rSubLayerHRD[i].u4CpbCntMinus1); j ++ )
                      {
                          UINT32 u4Bits;
                          HRD->rSubLayerHRD[i].u4BitRateValueMinus1[j][nalOrVcl] = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
                          #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                              DBG_H265_PRINTF(pfLogFile,"bit_rate_value_minus1[%d] %d\n", i, HRD->rSubLayerHRD[i].u4BitRateValueMinus1[j][nalOrVcl] );
                          #endif
                          HRD->rSubLayerHRD[i].u4CpbSizeValueMinus1[j][nalOrVcl] = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
                          #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                              DBG_H265_PRINTF(pfLogFile,"cpb_size_value_minus1[%d] %d\n", i, HRD->rSubLayerHRD[i].u4CpbSizeValueMinus1[j][nalOrVcl] );
                          #endif

                          if( HRD->bSubPicCpbParamsPresentFlag )
                          {
                              //[notice] spec code confilction !?
                              HRD->rSubLayerHRD[i].u4DuBitRateValueMinus1[j][nalOrVcl] = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
                              #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                                  DBG_H265_PRINTF(pfLogFile,"bit_rate_du_value_minus1[%d] %d\n", i, HRD->rSubLayerHRD[i].u4DuBitRateValueMinus1[j][nalOrVcl] );
                              #endif
                              HRD->rSubLayerHRD[i].u4DucpbSizeValueMinus1[j][nalOrVcl] = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
                              #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                                  DBG_H265_PRINTF(pfLogFile,"cpb_size_du_value_minus1[%d] %d\n", i, HRD->rSubLayerHRD[i].u4DucpbSizeValueMinus1[j][nalOrVcl] );
                              #endif
                          }
                          HRD->rSubLayerHRD[i].bCbrFlag[j][nalOrVcl]  = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
                          #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                              DBG_H265_PRINTF(pfLogFile,"cbr_flag[%d] %d\n", i, HRD->rSubLayerHRD[i].bCbrFlag[j][nalOrVcl] );
                          #endif
                     }
              }
          }
     }
    
}


// *********************************************************************
// Function    : void vHEVCVerifyVUI_Rbsp( UINT32 u4InstID, H265_VUI_Data* VUI, UINT32 MaxTLayers )
// Description : Handle SPS VUI header
// Parameter   : None
// Return      : None
// *********************************************************************

void vHEVCVerifyVUI_Rbsp( UINT32 u4InstID, H265_VUI_Data* VUI, UINT32 MaxTLayers )
{

    VUI->bAspectRatioInfoPresentFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"================== VUI parameters ==================\n" );
        DBG_H265_PRINTF(pfLogFile,"aspect_ratio_info_present_flag %d\n", VUI->bAspectRatioInfoPresentFlag  );
    #endif
    if (VUI->bAspectRatioInfoPresentFlag){
        VUI->i4AspectRatioIdc = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 8);//u(8)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"aspect_ratio_idc %d\n", VUI->i4AspectRatioIdc );
        #endif
        if (VUI->i4AspectRatioIdc  == 255){
            VUI->i4SarWidth= u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 16);//u(16)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"sar_width %d\n", VUI->i4SarWidth );
            #endif
            VUI->i4SarHeight = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 16);//u(16)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"sar_height %d\n", VUI->i4SarHeight );
            #endif
        }
    }

    VUI->bOverscanInfoPresentFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"overscan_info_present_flag %d\n", VUI->bOverscanInfoPresentFlag  );
    #endif
    if (VUI->bOverscanInfoPresentFlag){
        VUI->bOverscanAppropriateFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"overscan_appropriate_flag %d\n", VUI->bOverscanAppropriateFlag  );
        #endif
    }

    VUI->bVideoSignalTypePresentFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"video_signal_type_present_flag %d\n", VUI->bVideoSignalTypePresentFlag  );
    #endif
    if (VUI->bVideoSignalTypePresentFlag ){
        VUI->i4VideoFormat = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 3);//u(3)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"video_format %d\n", VUI->i4VideoFormat  );
        #endif
        VUI->bVideoFullRangeFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"video_full_range_flag %d\n", VUI->bVideoFullRangeFlag  );
        #endif
        VUI->bColourDescriptionPresentFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"colour_description_present_flag %d\n", VUI->bColourDescriptionPresentFlag  );
        #endif
        if (VUI->bColourDescriptionPresentFlag ){
            VUI->i4ColourPrimaries = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 8);//u(8)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"colour_primaries %d\n", VUI->i4ColourPrimaries );
            #endif
            VUI->i4TransferCharacteristics = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 8);//u(8)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"transfer_characteristics %d\n", VUI->i4TransferCharacteristics );
            #endif
            VUI->i4MatrixCoefficients = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 8);//u(8)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"matrix_coefficients %d\n", VUI->i4MatrixCoefficients );
            #endif
        }
    }

    VUI->bChromaLocInfoPresentFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"chroma_loc_info_present_flag %d\n", VUI->bChromaLocInfoPresentFlag  );
    #endif
    if (VUI->bChromaLocInfoPresentFlag ){
        VUI->i4ChromaSampleLocTypeTopField = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"chroma_sample_loc_type_top_field %d\n", VUI->i4ChromaSampleLocTypeTopField );
        #endif
        VUI->i4ChromaSampleLocTypeBottomField = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"chroma_sample_loc_type_bottom_field %d\n", VUI->i4ChromaSampleLocTypeBottomField );
        #endif
    }

    VUI->bNeutralChromaIndicationFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"neutral_chroma_indication_flag %d\n", VUI->bNeutralChromaIndicationFlag  );
    #endif

    VUI->bFieldSeqFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"field_seq_flag %d\n", VUI->bFieldSeqFlag  );
    #endif
    #if DEBUG_LEVEL >= DBG_LEVEL_BUG1
    if (VUI->bFieldSeqFlag)
        DBG_H265_PRINTF(pfLogFile,"[ERROR] VUI field_seq_flag not supported !!!\n" );
    #endif
   
    VUI->bFrameFieldInfoPresentFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"frame_field_info_present_flag %d\n", VUI->bFrameFieldInfoPresentFlag);
    #endif
    VUI->bDefaultDisplayWindowEnabledFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"default_display_window_flag %d\n", VUI->bDefaultDisplayWindowEnabledFlag);
    #endif

    if (VUI->bDefaultDisplayWindowEnabledFlag ){
        VUI->i4DefaultDisplayWinLeftOffset = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"def_disp_win_left_offset %d\n", VUI->i4DefaultDisplayWinLeftOffset);
        #endif
        VUI->i4DefaultDisplayWinRightOffset = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"def_disp_win_right_offset %d\n", VUI->i4DefaultDisplayWinRightOffset);
        #endif
        VUI->i4DefaultDisplayWinTopOffset = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"def_disp_win_top_offset %d\n", VUI->i4DefaultDisplayWinTopOffset);
        #endif
        VUI->i4DefaultDisplayWinBottomOffset = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"def_disp_win_bottom_offset %d\n", VUI->i4DefaultDisplayWinBottomOffset);
        #endif
    }

    VUI->bTimingInfoPresentFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"vui_timing_info_present_flag %d\n", VUI->bTimingInfoPresentFlag);
    #endif
    if( VUI->bTimingInfoPresentFlag )
    {
        VUI->u4NumUnitsInTick = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 32);//u(32)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"vui_num_units_in_tick %d\n", VUI->u4NumUnitsInTick);
        #endif
        VUI->u4TimeScale = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 32);//u(32)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"vui_time_scale %d\n", VUI->u4TimeScale);
        #endif
        VUI->bPocProportionalToTimingFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"vui_poc_proportional_to_timing_flag %d\n", VUI->bPocProportionalToTimingFlag);
        #endif
        if(VUI->bPocProportionalToTimingFlag)
        {
            VUI->i4NumTicksPocDiffOneMinus1 = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"vui_num_ticks_poc_diff_one_minus1 %d\n", VUI->i4NumTicksPocDiffOneMinus1);
            #endif
        }
        VUI->bHrdParametersPresentFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"hrd_parameters_present_flag %d\n", VUI->bHrdParametersPresentFlag);
        #endif
        if( VUI->bHrdParametersPresentFlag )
        {
          vHEVCVerifyHRD_Rbsp( _u4BSID[u4InstID], &VUI->rHdrParameters, 1, MaxTLayers -1 );
        }
    }

     VUI->bBitstreamRestrictionFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
         DBG_H265_PRINTF(pfLogFile,"bitstream_restriction_flag %d\n", VUI->bBitstreamRestrictionFlag);
    #endif
    if ( VUI->bBitstreamRestrictionFlag )
    {
        VUI->bTilesFixedStructureFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"tiles_fixed_structure_flag %d\n", VUI->bTilesFixedStructureFlag);
        #endif
        VUI->bMotionVectorsOverPicBoundariesFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"motion_vectors_over_pic_boundaries_flag %d\n", VUI->bMotionVectorsOverPicBoundariesFlag);
        #endif
        VUI->bRestrictedRefPicListsFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"restricted_ref_pic_lists_flag %d\n", VUI->bRestrictedRefPicListsFlag);
        #endif
        
        VUI->i4MinSpatialSegmentationIdc = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"min_spatial_segmentation_idc %d\n", VUI->i4MinSpatialSegmentationIdc);
        #endif
        VUI->i4MaxBytesPerPicDenom = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"max_bytes_per_pic_denom %d\n", VUI->i4MaxBytesPerPicDenom);
        #endif
        VUI->i4MaxBitsPerMinCuDenom = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"max_bits_per_mincu_denom %d\n", VUI->i4MaxBitsPerMinCuDenom);
        #endif
        VUI->i4Log2MaxMvLengthHorizontal = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"log2_max_mv_length_horizontal %d\n", VUI->i4Log2MaxMvLengthHorizontal);
        #endif
        VUI->i4Log2MaxMvLengthVertical = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"log2_max_mv_length_vertical %d\n", VUI->i4Log2MaxMvLengthVertical);
        #endif

    }
    
    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
    DBG_H265_PRINTF(pfLogFile,"==================== VUI end =====================\n" );
    #endif

}



// *********************************************************************
// Function    : UINT32 vHEVCVerifySPS_Rbsp(UINT32 u4InstID)
// Description : Handle picture parameter set header
// Parameter   : None
// Return      : UINT32
// *********************************************************************
UINT32 vHEVCVerifySPS_Rbsp(UINT32 u4InstID)
{
  UINT32 u4Temp;
  UINT32 u4SeqParameterSetId;
  UINT32 pfLogFile;
  UINT32 u4TmpVPSId, u4TmpMaxTLayersMinus1;
  BOOL bTmpTemporalIdNestingFlag;
  H265_SPS_Data *pH265_SPS_DataInst = NULL;
  H265_PTL_Data rTmpPTL;
  int i4tmp = 0;
  int u4AddCUDepth = 0;
  int iIndexer = 0;
  int i,j;
  int u4Idx = 0;
  char byteIs = 0;
  
#if DEBUG_LEVEL >= DBG_LEVEL_INFO
  printk ("[INFO] vHEVCVerifySPS_Rbsp() start!!\n");
#endif

    //start parsing
    u4TmpVPSId = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 4);//u(4)
    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"video_parameter_set_id %d\n", u4TmpVPSId);
    #endif
    u4TmpMaxTLayersMinus1 = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 3);//u(3)
    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"sps_max_sub_layers_minus1 %d\n", u4TmpMaxTLayersMinus1 );
    #endif
    bTmpTemporalIdNestingFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"temporal_id_nesting_flag %d\n", bTmpTemporalIdNestingFlag );
    #endif
    if ( u4TmpMaxTLayersMinus1==0 ){
        if ( 1 !=bTmpTemporalIdNestingFlag ){
            // sps_temporal_id_nesting_flag must be 1 when sps_max_sub_layers_minus1 is 0
            H265_DRV_PARSE_SET_ERR_RET(-12);
        }
    }

     rTmpPTL.bProfilePresentFlag = 1;
     vHEVCVerifyPTL_Rbsp(_u4BSID[u4InstID], &(rTmpPTL), rTmpPTL.bProfilePresentFlag, u4TmpMaxTLayersMinus1 );

     u4SeqParameterSetId = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
     if(u4SeqParameterSetId < 32){
         #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                 DBG_H265_PRINTF(pfLogFile,"SPS_id %d\n", u4SeqParameterSetId );
         #endif
         pH265_SPS_DataInst = &_rH265SPS[u4InstID][u4SeqParameterSetId];
         pH265_SPS_DataInst->bSPSValid = FALSE;// FALSE until set completely
         pH265_SPS_DataInst->u4VPSId = u4TmpVPSId;
         pH265_SPS_DataInst->u4MaxTLayersMinus1 = u4TmpMaxTLayersMinus1;
         pH265_SPS_DataInst->bTemporalIdNestingFlag = bTmpTemporalIdNestingFlag;
         pH265_SPS_DataInst->u4SeqParameterSetId = u4SeqParameterSetId;
         pH265_SPS_DataInst->rSPS_PTL = rTmpPTL;
         _tVerMpvDecPrm[u4InstID].SpecDecPrm.rVDecH265DecPrm.rLastInfo.ucLastSPSId = pH265_SPS_DataInst->u4SeqParameterSetId;

     } else {
     #if DEBUG_LEVEL >= DBG_LEVEL_BUG1
             DBG_H265_PRINTF(pfLogFile,"[ERROR] invalid SPS ID %d\n", u4SeqParameterSetId);
     #endif
         sprintf(_bTempStr1[u4InstID], "%s", "err in SPS Num\\n\\0");
         vErrMessage(u4InstID, (CHAR *)_bTempStr1[u4InstID]);    
         //TODO: if the parsing error then force to use sps id == 0 's SPS data?
         H265_DRV_PARSE_SET_ERR_RET(-16);
         return SPS_SYNTAX_ERROR;
     }
    
     vHEVCVerifyInitSPS(pH265_SPS_DataInst);
     vHEVCVerifyInitPicInfo(u4InstID);
     
     pH265_SPS_DataInst->u4ChromaFormatIdc = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
     #if DEBUG_LEVEL >= DBG_LEVEL_INFO
         DBG_H265_PRINTF(pfLogFile,"chroma_format_idc %d\n", pH265_SPS_DataInst->u4ChromaFormatIdc );
     #endif
     if( pH265_SPS_DataInst->u4ChromaFormatIdc == 3 )
     {
        if ( pH265_SPS_DataInst->bSeparateColourPlaneFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1) ){
             #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                 DBG_H265_PRINTF(pfLogFile,"separate_colour_plane_flag %d chroma_format_idc %d\n", pH265_SPS_DataInst->bSeparateColourPlaneFlag ,pH265_SPS_DataInst->u4ChromaFormatIdc );
             #endif
                 H265_DRV_PARSE_SET_ERR_RET(-17);
        }
     }

     if ( 0 == pH265_SPS_DataInst->u4ChromaFormatIdc ){
         pH265_SPS_DataInst->u4SubWidthC = 1;
         pH265_SPS_DataInst->u4SubHeightC = 1;
     } else if ( 1 == pH265_SPS_DataInst->u4ChromaFormatIdc ) {
         pH265_SPS_DataInst->u4SubWidthC = 2;
         pH265_SPS_DataInst->u4SubHeightC = 2;
     } else if ( 2 == pH265_SPS_DataInst->u4ChromaFormatIdc ) {
         pH265_SPS_DataInst->u4SubWidthC = 2;
         pH265_SPS_DataInst->u4SubHeightC = 1;
     } else if ( 3 == pH265_SPS_DataInst->u4ChromaFormatIdc ) {
         pH265_SPS_DataInst->u4SubWidthC = 1;
         pH265_SPS_DataInst->u4SubHeightC = 1;
     } else {
         #if DEBUG_LEVEL >= DBG_LEVEL_INFO
             DBG_H265_PRINTF(pfLogFile,"[ERROR] invalid chroma_format_idc %d\n", i4tmp ,pH265_SPS_DataInst->u4ChromaFormatIdc );
         #endif
         H265_DRV_PARSE_SET_ERR_RET(-18);
     }
     
     pH265_SPS_DataInst->u4PicWidthInLumaSamples = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
     #if DEBUG_LEVEL >= DBG_LEVEL_INFO
         DBG_H265_PRINTF(pfLogFile,"pic_width_in_luma_samples %d\n", pH265_SPS_DataInst->u4PicWidthInLumaSamples );
     #endif
     pH265_SPS_DataInst->u4PicHeightInLumaSamples = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);    //ue(v)
     #if DEBUG_LEVEL >= DBG_LEVEL_INFO
         DBG_H265_PRINTF(pfLogFile,"pic_height_in_luma_samples %d\n", pH265_SPS_DataInst->u4PicHeightInLumaSamples );
     #endif    
     pH265_SPS_DataInst->bConformanceWindowFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
     #if DEBUG_LEVEL >= DBG_LEVEL_INFO
         DBG_H265_PRINTF(pfLogFile,"conformance_window_flag %d\n", pH265_SPS_DataInst->bConformanceWindowFlag );
     #endif
     if ( pH265_SPS_DataInst->bConformanceWindowFlag ){
         pH265_SPS_DataInst->u4ConfWinLeftOffset = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
         #if DEBUG_LEVEL >= DBG_LEVEL_INFO
             DBG_H265_PRINTF(pfLogFile,"conf_win_left_offset %d\n", pH265_SPS_DataInst->u4ConfWinLeftOffset );
         #endif
          pH265_SPS_DataInst->u4ConfWinRightOffset = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
         #if DEBUG_LEVEL >= DBG_LEVEL_INFO
             DBG_H265_PRINTF(pfLogFile,"conf_win_right_offset %d\n", pH265_SPS_DataInst->u4ConfWinRightOffset );
         #endif
         pH265_SPS_DataInst->u4ConfWinTopOffset = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
         #if DEBUG_LEVEL >= DBG_LEVEL_INFO
             DBG_H265_PRINTF(pfLogFile,"conf_win_top_offset %d\n", pH265_SPS_DataInst->u4ConfWinTopOffset );
         #endif
         pH265_SPS_DataInst->u4ConfWinBottomOffset = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
         #if DEBUG_LEVEL >= DBG_LEVEL_INFO
             DBG_H265_PRINTF(pfLogFile,"conf_win_bottom_offset %d\n", pH265_SPS_DataInst->u4ConfWinBottomOffset );
         #endif
     }

     pH265_SPS_DataInst->u4FrameCropLeftOffset = pH265_SPS_DataInst->u4ConfWinLeftOffset * pH265_SPS_DataInst->u4SubWidthC;
     pH265_SPS_DataInst->u4FrameCropRightOffset = pH265_SPS_DataInst->u4ConfWinRightOffset * pH265_SPS_DataInst->u4SubWidthC;
     pH265_SPS_DataInst->u4FrameCropTopOffset = pH265_SPS_DataInst->u4ConfWinTopOffset * pH265_SPS_DataInst->u4SubHeightC;
     pH265_SPS_DataInst->u4FrameCropBottomOffset = pH265_SPS_DataInst->u4ConfWinBottomOffset * pH265_SPS_DataInst->u4SubHeightC;
     
     pH265_SPS_DataInst->u4BitDepthLumaMinus8 = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
     #if DEBUG_LEVEL >= DBG_LEVEL_INFO
         DBG_H265_PRINTF(pfLogFile,"bit_depth_luma_minus8 %d\n", pH265_SPS_DataInst->u4BitDepthLumaMinus8 );
     #endif
     pH265_SPS_DataInst->u4QpBDOffsetY = (UINT32) 6*(pH265_SPS_DataInst->u4BitDepthLumaMinus8+8);

     pH265_SPS_DataInst->u4BitDepthChromaMinus8 = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
     #if DEBUG_LEVEL >= DBG_LEVEL_INFO
         DBG_H265_PRINTF(pfLogFile,"bit_depth_chroma_minus8 %d\n", pH265_SPS_DataInst->u4BitDepthChromaMinus8  );
     #endif
     pH265_SPS_DataInst->u4QpBDOffsetC = (UINT32) 6*(pH265_SPS_DataInst->u4BitDepthChromaMinus8+8);

     if ( pH265_SPS_DataInst->u4BitDepthLumaMinus8!=0 || pH265_SPS_DataInst->u4BitDepthChromaMinus8!=0 ){ 
        //10 bits currently not support
         #if DEBUG_LEVEL >= DBG_LEVEL_BUG1
             DBG_H265_PRINTF(pfLogFile,"[ERROR] 10-bits not support!! \n" );
         #endif
//         return NOT_SUPPORT;
         H265_DRV_PARSE_SET_ERR_RET(-19);
         return SPS_SYNTAX_ERROR; 
     }

     pH265_SPS_DataInst->u4Log2MaxPicOrderCntLsbMinus4 = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
     #if DEBUG_LEVEL >= DBG_LEVEL_INFO
         DBG_H265_PRINTF(pfLogFile,"log2_max_pic_order_cnt_lsb_minus4 %d\n", pH265_SPS_DataInst->u4Log2MaxPicOrderCntLsbMinus4 );
     #endif

     i4tmp  = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
     #if DEBUG_LEVEL >= DBG_LEVEL_INFO
         DBG_H265_PRINTF(pfLogFile,"sps_sub_layer_ordering_info_present_flag %d\n", i4tmp );
     #endif

     for( i=0; i <= pH265_SPS_DataInst->u4MaxTLayersMinus1; i++)
     {
         pH265_SPS_DataInst->u4MaxDecPicBuffering[i] = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID)+1;//ue(v)
         #if DEBUG_LEVEL >= DBG_LEVEL_INFO
             DBG_H265_PRINTF(pfLogFile,"sps_max_dec_pic_buffering[%d] %d\n", i,pH265_SPS_DataInst->u4MaxDecPicBuffering[i] );
         #endif

         pH265_SPS_DataInst->u4NumReorderPics[i] = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
         #if DEBUG_LEVEL >= DBG_LEVEL_INFO
             DBG_H265_PRINTF(pfLogFile,"sps_num_reorder_pics[%d] %d\n", i,pH265_SPS_DataInst->u4NumReorderPics[i] );
         #endif

        pH265_SPS_DataInst->u4MaxLatencyIncrease[i] = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
         #if DEBUG_LEVEL >= DBG_LEVEL_INFO
             DBG_H265_PRINTF(pfLogFile,"sps_max_latency_increase[%d] %d\n", i,pH265_SPS_DataInst->u4MaxLatencyIncrease[i] );
         #endif
         
        if  ( 1 != i4tmp )
        {
            for ( i++; i <= pH265_SPS_DataInst->u4MaxTLayersMinus1; i++)
            {
                 pH265_SPS_DataInst->u4MaxDecPicBuffering[i] = pH265_SPS_DataInst->u4MaxDecPicBuffering[0];
                 pH265_SPS_DataInst->u4NumReorderPics[i] = pH265_SPS_DataInst->u4NumReorderPics[0];
                 pH265_SPS_DataInst->u4MaxLatencyIncrease[i] = pH265_SPS_DataInst->u4MaxLatencyIncrease[0];
            }
         break;
        }
    }

     pH265_SPS_DataInst->u4Log2MinCodingBlockSizeMinus3 = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
     #if DEBUG_LEVEL >= DBG_LEVEL_INFO
         DBG_H265_PRINTF(pfLogFile,"log2_min_coding_block_size_minus3 %d\n", pH265_SPS_DataInst->u4Log2MinCodingBlockSizeMinus3 );
     #endif
     pH265_SPS_DataInst->u4Log2DiffMaxMinCodingBlockSize = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
     #if DEBUG_LEVEL >= DBG_LEVEL_INFO
         DBG_H265_PRINTF(pfLogFile,"log2_diff_max_min_coding_block_size %d\n", pH265_SPS_DataInst->u4Log2DiffMaxMinCodingBlockSize);
     #endif
     pH265_SPS_DataInst->u4MaxCUWidth = 1<<(pH265_SPS_DataInst->u4Log2MinCodingBlockSizeMinus3 + 3 + pH265_SPS_DataInst->u4Log2DiffMaxMinCodingBlockSize);
     pH265_SPS_DataInst->u4MaxCUHeight =  1<<(pH265_SPS_DataInst->u4Log2MinCodingBlockSizeMinus3 + 3 + pH265_SPS_DataInst->u4Log2DiffMaxMinCodingBlockSize);

     pH265_SPS_DataInst->u4Log2MinTransformBlockSizeMinus2 = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);    //ue(v)
     #if DEBUG_LEVEL >= DBG_LEVEL_INFO
         DBG_H265_PRINTF(pfLogFile,"log2_min_transform_block_size_minus2 %d\n", pH265_SPS_DataInst->u4Log2MinTransformBlockSizeMinus2);
     #endif
     pH265_SPS_DataInst->u4Log2DiffMaxMinTtransformBlockSize = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);  //ue(v)
     #if DEBUG_LEVEL >= DBG_LEVEL_INFO
         DBG_H265_PRINTF(pfLogFile,"log2_diff_max_min_transform_block_size %d\n", pH265_SPS_DataInst->u4Log2DiffMaxMinTtransformBlockSize);
     #endif
     pH265_SPS_DataInst->u4MaxTrSize = (1<<(pH265_SPS_DataInst->u4Log2MinTransformBlockSizeMinus2+2 + pH265_SPS_DataInst->u4Log2DiffMaxMinTtransformBlockSize  ) );

     pH265_SPS_DataInst->u4QuadtreeTUMaxDepthInter = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID) + 1;  //ue(v)
     #if DEBUG_LEVEL >= DBG_LEVEL_INFO
         DBG_H265_PRINTF(pfLogFile,"max_transform_hierarchy_depth_inter %d\n", pH265_SPS_DataInst->u4QuadtreeTUMaxDepthInter);
     #endif
     pH265_SPS_DataInst->u4QuadtreeTUMaxDepthIntra = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID) + 1;  //ue(v)
     #if DEBUG_LEVEL >= DBG_LEVEL_INFO
         DBG_H265_PRINTF(pfLogFile,"max_transform_hierarchy_depth_intra %d\n", pH265_SPS_DataInst->u4QuadtreeTUMaxDepthIntra);
     #endif

      while( ( pH265_SPS_DataInst->u4MaxCUWidth >> pH265_SPS_DataInst->u4Log2DiffMaxMinCodingBlockSize ) > ( 1 << ( pH265_SPS_DataInst->u4Log2MinTransformBlockSizeMinus2+2 + u4AddCUDepth )  ) )
     {
         u4AddCUDepth++;
     }
      pH265_SPS_DataInst->u4MaxCUDepth = u4AddCUDepth + pH265_SPS_DataInst->u4Log2DiffMaxMinCodingBlockSize;

     pH265_SPS_DataInst->bScalingListFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
     #if DEBUG_LEVEL >= DBG_LEVEL_INFO
             DBG_H265_PRINTF(pfLogFile,"scaling_list_enabled_flag %d\n", pH265_SPS_DataInst->bScalingListFlag);
     #endif
     
     if( pH265_SPS_DataInst->bScalingListFlag ){
         pH265_SPS_DataInst->bScalingListPresentFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
     #if DEBUG_LEVEL >= DBG_LEVEL_INFO
             DBG_H265_PRINTF(pfLogFile,"sps_scaling_list_data_present_flag %d\n", pH265_SPS_DataInst->bScalingListPresentFlag);
     #endif
         if (pH265_SPS_DataInst->bScalingListPresentFlag ){
             //parse scaling list
            if ( PARSE_OK != vHEVCVerifySL_Rbsp( _u4BSID[u4InstID], &(pH265_SPS_DataInst->rSPS_ScalingList) ) ){
                H265_DRV_PARSE_SET_ERR_RET(-20);
                return SPS_SYNTAX_ERROR;
            }
             
         }
     }

    pH265_SPS_DataInst->bUseAMP = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"asymmetric_motion_partitions_enabled_flag %d\n", pH265_SPS_DataInst->bUseAMP);
    #endif
    pH265_SPS_DataInst->bUseSAO = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"sample_adaptive_offset_enabled_flag %d\n", pH265_SPS_DataInst->bUseSAO);
    #endif
    pH265_SPS_DataInst->bUsePCM = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"pcm_enabled_flag %d\n", pH265_SPS_DataInst->bUsePCM );
    #endif
    if ( pH265_SPS_DataInst->bUsePCM ){
        pH265_SPS_DataInst->u4PCMBitDepthLumaMinus1 = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 4);//u(4)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"pcm_sample_bit_depth_luma_minus1 %d\n", pH265_SPS_DataInst->u4PCMBitDepthLumaMinus1 );
        #endif
        pH265_SPS_DataInst->u4PCMBitDepthChromaMinus1 = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 4);//u(4)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"pcm_sample_bit_depth_chroma_minus1 %d\n", pH265_SPS_DataInst->u4PCMBitDepthChromaMinus1 );
        #endif

        pH265_SPS_DataInst->u4PCMLog2LumaMinSizeMinus3 = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"log2_min_pcm_luma_coding_block_size_minus3 %d\n", pH265_SPS_DataInst->u4PCMLog2LumaMinSizeMinus3);
        #endif
        pH265_SPS_DataInst->u4PCMLog2LumaMaxSize = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"log2_diff_max_min_pcm_luma_coding_block_size %d\n", pH265_SPS_DataInst->u4PCMLog2LumaMaxSize);
        #endif
        pH265_SPS_DataInst->u4PCMLog2LumaMaxSize += pH265_SPS_DataInst->u4PCMLog2LumaMinSizeMinus3+3;

        pH265_SPS_DataInst->bPCMFilterDisableFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
         #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"pcm_loop_filter_disable_flag %d\n", pH265_SPS_DataInst->bPCMFilterDisableFlag);
        #endif
    }


    //parse RPS
    pH265_SPS_DataInst->u4NumShortTermRefPicSets = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"num_short_term_ref_pic_sets %d\n", pH265_SPS_DataInst->u4NumShortTermRefPicSets);
    #endif

    if (pH265_SPS_DataInst->u4NumShortTermRefPicSets>64){
        pH265_SPS_DataInst->u4NumShortTermRefPicSets = 64;
         #if DEBUG_LEVEL >= DBG_LEVEL_BUG1
            DBG_H265_PRINTF(pfLogFile,"[ERROR] num_short_term_ref_pic_sets > 64 (set to 64)!! \n" );
         #endif
    }
    
    for(u4Idx = 0; u4Idx < pH265_SPS_DataInst->u4NumShortTermRefPicSets; u4Idx++)
    {   
        if ( pH265_SPS_DataInst->pShortTermRefPicSets[u4Idx] == NULL )
            pH265_SPS_DataInst->pShortTermRefPicSets[u4Idx] = (pH265_RPS_Data) vmalloc(sizeof(H265_RPS_Data));
            if ( PARSE_OK != vHEVCVerifyRPS_Rbsp(_u4BSID[u4InstID], pH265_SPS_DataInst, pH265_SPS_DataInst->pShortTermRefPicSets[u4Idx], u4Idx) ){
                #if DEBUG_LEVEL >= DBG_LEVEL_BUG1
                    DBG_H265_PRINTF(pfLogFile,"[ERROR] RPS syntex Error!! \n" );
                #endif
                H265_DRV_PARSE_SET_ERR_RET(-21);
                return SPS_SYNTAX_ERROR; 
        }
    }
    
    pH265_SPS_DataInst->bLongTermRefsPresent = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"long_term_ref_pics_present_flag %d\n", pH265_SPS_DataInst->bLongTermRefsPresent);
    #endif
    if (pH265_SPS_DataInst->bLongTermRefsPresent){
        pH265_SPS_DataInst->u4NumLongTermRefPicSPS = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"num_long_term_ref_pic_sps %d\n", pH265_SPS_DataInst->u4NumLongTermRefPicSPS);
        #endif
        for(u4Idx = 0; u4Idx < pH265_SPS_DataInst->u4NumLongTermRefPicSPS; u4Idx++)
        {
            pH265_SPS_DataInst->u4LtRefPicPocLsbSps[u4Idx] = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, pH265_SPS_DataInst->u4Log2MaxPicOrderCntLsbMinus4 + 4);//u(BitsForPOC)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"lt_ref_pic_poc_lsb_sps[%d] %d\n", u4Idx, pH265_SPS_DataInst->u4LtRefPicPocLsbSps[u4Idx] );
            #endif
            pH265_SPS_DataInst->bUsedByCurrPicLtSPSFlag[u4Idx] = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"used_by_curr_pic_lt_sps_flag[%d] %d\n", u4Idx, pH265_SPS_DataInst->bUsedByCurrPicLtSPSFlag[u4Idx] );
            #endif
        }
    }
    pH265_SPS_DataInst->u4NumRefFrames = pH265_SPS_DataInst->u4NumLongTermRefPicSPS + _rH265PicInfo[u4InstID].i4MaxStrNumNegPosPics;
    
    pH265_SPS_DataInst->bTMVPFlagsPresent = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"sps_temporal_mvp_enable_flag %d\n", pH265_SPS_DataInst->bTMVPFlagsPresent );
    #endif
    
    pH265_SPS_DataInst->bUseStrongIntraSmoothing = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"sps_strong_intra_smoothing_enable_flag %d\n", pH265_SPS_DataInst->bUseStrongIntraSmoothing );
    #endif

    pH265_SPS_DataInst->bVuiParametersPresentFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"vui_parameters_present_flag %d\n", pH265_SPS_DataInst->bVuiParametersPresentFlag );
    #endif

    if ( pH265_SPS_DataInst->bVuiParametersPresentFlag )
    {
         vHEVCVerifyVUI_Rbsp(_u4BSID[u4InstID],  &(pH265_SPS_DataInst->rVUI), pH265_SPS_DataInst->u4MaxTLayersMinus1+1);
    }

    pH265_SPS_DataInst->bSPSExtensionFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"sps_extension_flag %d\n", pH265_SPS_DataInst->bSPSExtensionFlag );
    #endif

    vVDEC_HAL_H265_TrailingBits(_u4BSID[u4InstID], u4InstID);
    
    pH265_SPS_DataInst->bSPSValid = TRUE;

#if DEBUG_LEVEL >= DBG_LEVEL_INFO
    printk ("[INFO] vHEVCVerifySPS_Rbsp() done!!\n");
#endif

    return PARSE_OK;
}


// *********************************************************************
// Function    : UINT32 vHEVCVerifyPPS_Rbsp(UINT32 u4InstID)
// Description : Handle picture parameter set header
// Parameter   : None
// Return      : UINT32
// *********************************************************************
UINT32 vHEVCVerifyPPS_Rbsp(UINT32 u4InstID)
{
  UINT32 u4PicParameterSetId;
  H265_SPS_Data *pH265_SPS_DataInst;
  H265_PPS_Data *pH265_PPS_DataInst;

  int i4tmp = 0;
  int iIndexer = 0,i;
  int iDataSize = 0;
  
#if DEBUG_LEVEL >= DBG_LEVEL_INFO
  printk ("[INFO] vHEVCVerifyPPS_Rbsp() start!!\n");
#endif

    // parse PPS
    u4PicParameterSetId = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
    if(u4PicParameterSetId < 256){
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"pps_PPS_id %d\n", u4PicParameterSetId);
        #endif
        pH265_PPS_DataInst = &_rH265PPS[u4InstID][u4PicParameterSetId];
        pH265_PPS_DataInst->bPPSValid = FALSE;// FALSE until set completely
    }
    else{
#if DEBUG_LEVEL >= DBG_LEVEL_BUG1
            DBG_H265_PRINTF(pfLogFile,"[ERROR] invalid PicParameterSetId %d\n", pH265_PPS_DataInst->u4PicParameterSetId);
#endif
        sprintf(_bTempStr1[u4InstID], "%s", "err in PPS Num err\\n\\0");
        vErrMessage(u4InstID, (CHAR *)_bTempStr1[u4InstID]);    
        H265_DRV_PARSE_SET_ERR_RET(-15);
        return PPS_SYNTAX_ERROR;
    }
    pH265_PPS_DataInst->u4SeqParameterSetId = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
#if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"pps_SPS_id %d\n", pH265_PPS_DataInst->u4SeqParameterSetId);
#endif
    if ( pH265_PPS_DataInst->u4SeqParameterSetId < 32 ){
        pH265_SPS_DataInst = &_rH265SPS[u4InstID][pH265_PPS_DataInst->u4SeqParameterSetId];
    } else {
        pH265_SPS_DataInst = &_rH265SPS[u4InstID][0];
        #if DEBUG_LEVEL >= DBG_LEVEL_BUG1
            DBG_H265_PRINTF(pfLogFile,"[ERROR] SPS ID out of range 0-32 [%d]\n",pH265_PPS_DataInst->u4SeqParameterSetId);
        #endif
    }

    if (!pH265_SPS_DataInst->bSPSValid){
#if DEBUG_LEVEL >= DBG_LEVEL_BUG1
        DBG_H265_PRINTF(pfLogFile,"[ERROR] Cannot find SPS ID exist in array [%d]\n",pH265_PPS_DataInst->u4SeqParameterSetId);
#endif
        if (_rH265SPS[u4InstID][0].bSPSValid){
            pH265_SPS_DataInst = &_rH265SPS[u4InstID][0];
#if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"[ERROR] PPS reference SPS id force set 0!!\n");
#endif
        } else {
            H265_DRV_PARSE_SET_ERR_RET(-16);
            return PPS_SYNTAX_ERROR;
        }
    }

    //init PPS
    //init scaling list
    int sizeId;
    int listId;
    for( sizeId = 0; sizeId < SCALING_LIST_SIZE_NUM; sizeId++)
    {
        for( listId = 0; listId < H265_scalingListNum[sizeId]; listId++)
        {
            int size = (MAX_MATRIX_COEF_NUM>H265_scalingListSize[sizeId])? H265_scalingListSize[sizeId] : MAX_MATRIX_COEF_NUM;
            if ( pH265_PPS_DataInst->bSL_Init ){
                 vfree(pH265_PPS_DataInst->rPPS_ScalingList.pScalingListDeltaCoef[sizeId][listId]);
            }
            pH265_PPS_DataInst->rPPS_ScalingList.pScalingListDeltaCoef[sizeId][listId] = (INT32*)vmalloc( size*sizeof(INT32) );
        }
    }
    pH265_PPS_DataInst->rPPS_ScalingList.pScalingListDeltaCoef[SCALING_LIST_32x32][3] = pH265_PPS_DataInst->rPPS_ScalingList.pScalingListDeltaCoef[SCALING_LIST_32x32][1];
    pH265_PPS_DataInst->bSL_Init = 1; 
    pH265_PPS_DataInst->u4NumColumnsMinus1 = 0;
    pH265_PPS_DataInst->u4NumRowsMinus1 = 0;
    pH265_PPS_DataInst->bLoopFilterAcrossTilesEnabledFlag = 1;
    // init end

    pH265_PPS_DataInst->bDependentSliceSegmentsEnabledFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
#if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"dependent_slice_segments_enabled_flag %d\n", pH265_PPS_DataInst->bDependentSliceSegmentsEnabledFlag);
#endif
    pH265_PPS_DataInst->bOutputFlagPresentFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
#if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"output_flag_present_flag %d\n", pH265_PPS_DataInst->bOutputFlagPresentFlag);
#endif
    pH265_PPS_DataInst->u4NumExtraSliceHeaderBits = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 3);//u(3)
#if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"num_extra_slice_header_bits %d\n", pH265_PPS_DataInst->u4NumExtraSliceHeaderBits);
#endif

    pH265_PPS_DataInst->bSignHideFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
#if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"sign_data_hiding_flag %d\n", pH265_PPS_DataInst->bSignHideFlag);
#endif
    pH265_PPS_DataInst->bCabacInitPresentFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
#if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"cabac_init_present_flag %d\n", pH265_PPS_DataInst->bCabacInitPresentFlag);
#endif

    pH265_PPS_DataInst->u4NumRefIdxL0DefaultActiveMinus1 = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
#if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"num_ref_idx_l0_default_active_minus1 %d\n", pH265_PPS_DataInst->u4NumRefIdxL0DefaultActiveMinus1);
#endif
    if ( pH265_PPS_DataInst->u4NumRefIdxL0DefaultActiveMinus1 > 14 ){
        #if DEBUG_LEVEL >= DBG_LEVEL_BUG1
            DBG_H265_PRINTF(pfLogFile,"[ERROR] u4NumRefIdxL0DefaultActiveMinus1:%d > 14\n", pH265_PPS_DataInst->u4NumRefIdxL0DefaultActiveMinus1);
        #endif
        H265_DRV_PARSE_SET_ERR_RET(-17);
        return PPS_SYNTAX_ERROR;
    }
    pH265_PPS_DataInst->u4NumRefIdxL1DefaultActiveMinus1 = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
#if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"num_ref_idx_l1_default_active_minus1 %d\n", pH265_PPS_DataInst->u4NumRefIdxL1DefaultActiveMinus1);
#endif
    if ( pH265_PPS_DataInst->u4NumRefIdxL1DefaultActiveMinus1 > 14 ){
#if DEBUG_LEVEL >= DBG_LEVEL_BUG1
            DBG_H265_PRINTF(pfLogFile,"[ERROR] u4NumRefIdxL1DefaultActiveMinus1:%d > 14\n", pH265_PPS_DataInst->u4NumRefIdxL1DefaultActiveMinus1);
#endif
        H265_DRV_PARSE_SET_ERR_RET(-18);
        return PPS_SYNTAX_ERROR;
    }

    pH265_PPS_DataInst->i4PicInitQPMinus26 = i4VDEC_HAL_H265_SeCodeNum(_u4BSID[u4InstID], u4InstID);//se(v)
#if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"init_qp_minus26 %d\n", pH265_PPS_DataInst->i4PicInitQPMinus26);
#endif

    pH265_PPS_DataInst->bConstrainedIntraPredFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
#if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"constrained_intra_pred_flag %d\n", pH265_PPS_DataInst->bConstrainedIntraPredFlag);
#endif
    pH265_PPS_DataInst->bTransformSkipEnabledFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
#if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"transform_skip_enabled_flag %d\n", pH265_PPS_DataInst->bTransformSkipEnabledFlag);
#endif
    pH265_PPS_DataInst->bCuQPDeltaEnabledFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
#if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"cu_qp_delta_enabled_flag %d\n", pH265_PPS_DataInst->bCuQPDeltaEnabledFlag);
#endif
    if( pH265_PPS_DataInst->bCuQPDeltaEnabledFlag )
    {
        pH265_PPS_DataInst->u4DiffCuQPDeltaDepth = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"diff_cu_qp_delta_depth  %d\n", pH265_PPS_DataInst->u4DiffCuQPDeltaDepth);
        #endif
    } else {
        pH265_PPS_DataInst->u4DiffCuQPDeltaDepth = 0;
    }

    pH265_PPS_DataInst->i4PPSCbQPOffset = i4VDEC_HAL_H265_SeCodeNum(_u4BSID[u4InstID], u4InstID);//se(v)
#if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"pps_cb_qp_offset %d\n", pH265_PPS_DataInst->i4PPSCbQPOffset);
#endif
    if ( pH265_PPS_DataInst->i4PPSCbQPOffset < -12 || pH265_PPS_DataInst->i4PPSCbQPOffset > 12 ){
        #if DEBUG_LEVEL >= DBG_LEVEL_BUG1
            DBG_H265_PRINTF(pfLogFile,"[ERROR] i4PPSCbQPOffset:%d is not in between -12 ~ 12\n", pH265_PPS_DataInst->i4PPSCbQPOffset);
        #endif
        H265_DRV_PARSE_SET_ERR_RET(-19);
        return PPS_SYNTAX_ERROR;
    }
    pH265_PPS_DataInst->i4PPSCrQPOffset = i4VDEC_HAL_H265_SeCodeNum(_u4BSID[u4InstID], u4InstID);//se(v)
#if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"pps_cr_qp_offset %d\n", pH265_PPS_DataInst->i4PPSCrQPOffset);
#endif
    if ( pH265_PPS_DataInst->i4PPSCrQPOffset < -12 || pH265_PPS_DataInst->i4PPSCrQPOffset > 12 ){
#if DEBUG_LEVEL >= DBG_LEVEL_BUG1
            DBG_H265_PRINTF(pfLogFile,"[ERROR] i4PPSCrQPOffset:%d is not in between -12 ~ 12\n", pH265_PPS_DataInst->i4PPSCrQPOffset);
#endif
        H265_DRV_PARSE_SET_ERR_RET(-20);
        return PPS_SYNTAX_ERROR;
    }

    pH265_PPS_DataInst->bPPSSliceChromaQpFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
#if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"pps_slice_chroma_qp_offsets_present_flag %d\n", pH265_PPS_DataInst->bPPSSliceChromaQpFlag);
#endif
    pH265_PPS_DataInst->bWPPredFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
#if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"weighted_pred_flag %d\n", pH265_PPS_DataInst->bWPPredFlag);
#endif
    pH265_PPS_DataInst->bWPBiPredFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
#if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"weighted_bipred_flag %d\n", pH265_PPS_DataInst->bWPBiPredFlag);
#endif
    pH265_PPS_DataInst->bTransquantBypassEnableFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
#if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"transquant_bypass_enable_flag %d\n", pH265_PPS_DataInst->bTransquantBypassEnableFlag);
#endif
    pH265_PPS_DataInst->bTilesEnabledFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
#if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"tiles_enabled_flag %d\n", pH265_PPS_DataInst->bTilesEnabledFlag);
#endif
    pH265_PPS_DataInst->bEntropyCodingSyncEnabledFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
#if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"entropy_coding_sync_enabled_flag %d\n", pH265_PPS_DataInst->bEntropyCodingSyncEnabledFlag);
#endif

    if( pH265_PPS_DataInst->bTilesEnabledFlag )
    {
        pH265_PPS_DataInst->u4NumColumnsMinus1 = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"num_tile_columns_minus1  %d\n", pH265_PPS_DataInst->u4NumColumnsMinus1);
        #endif
        pH265_PPS_DataInst->u4NumRowsMinus1 = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
              DBG_H265_PRINTF(pfLogFile,"num_tile_rows_minus1  %d\n", pH265_PPS_DataInst->u4NumRowsMinus1);
        #endif
        pH265_PPS_DataInst->bUniformSpacingFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"uniform_spacing_flag %d\n", pH265_PPS_DataInst->bUniformSpacingFlag);
        #endif

        if( 1 != pH265_PPS_DataInst->bUniformSpacingFlag )
        {   
            for( iIndexer = 0; iIndexer<pH265_PPS_DataInst->u4NumColumnsMinus1 ; iIndexer++)
            { 
                pH265_PPS_DataInst->u4ColumnWidthMinus1[iIndexer] = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
                #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                      DBG_H265_PRINTF(pfLogFile,"column_width_minus1[%d]  %d\n", iIndexer, pH265_PPS_DataInst->u4ColumnWidthMinus1[iIndexer]);
                #endif
            }

            for( iIndexer = 0; iIndexer<pH265_PPS_DataInst->u4NumRowsMinus1 ; iIndexer++)
            { 
                pH265_PPS_DataInst->u4RowHeightMinus1[iIndexer] = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
                #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                      DBG_H265_PRINTF(pfLogFile,"row_height_minus1[%d]  %d\n", iIndexer, pH265_PPS_DataInst->u4RowHeightMinus1[iIndexer]);
                #endif
            }
            
        }
        if( 0 != pH265_PPS_DataInst->u4NumColumnsMinus1 || 0 != pH265_PPS_DataInst->u4NumRowsMinus1 )
        {
            pH265_PPS_DataInst->bLoopFilterAcrossTilesEnabledFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"loop_filter_across_tiles_enabled_flag %d\n", pH265_PPS_DataInst->bLoopFilterAcrossTilesEnabledFlag);
            #endif
        }
    }

    pH265_PPS_DataInst->bLoopFilterAcrossSlicesEnabledFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
#if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"loop_filter_across_slices_enabled_flag %d\n", pH265_PPS_DataInst->bLoopFilterAcrossSlicesEnabledFlag);
#endif
    pH265_PPS_DataInst->bDeblockingFilterControlPresentFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
#if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"deblocking_filter_control_present_flag %d\n", pH265_PPS_DataInst->bDeblockingFilterControlPresentFlag);
#endif
    if( pH265_PPS_DataInst->bDeblockingFilterControlPresentFlag )
    {
        pH265_PPS_DataInst->bDeblockingFilterOverrideEnabledFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"deblocking_filter_override_enabled_flag %d\n", pH265_PPS_DataInst->bDeblockingFilterOverrideEnabledFlag);
        #endif
        pH265_PPS_DataInst->bPicDisableDeblockingFilterFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"pps_disable_deblocking_filter_flag %d\n", pH265_PPS_DataInst->bPicDisableDeblockingFilterFlag);
        #endif
        if( 1 != pH265_PPS_DataInst->bPicDisableDeblockingFilterFlag )
        {
            pH265_PPS_DataInst->i4DeblockingFilterBetaOffsetDiv2 = i4VDEC_HAL_H265_SeCodeNum(_u4BSID[u4InstID], u4InstID);//se(v)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"pps_beta_offset_div2 %d\n", pH265_PPS_DataInst->i4DeblockingFilterBetaOffsetDiv2);
            #endif
            pH265_PPS_DataInst->i4DeblockingFilterTcOffsetDiv2 = i4VDEC_HAL_H265_SeCodeNum(_u4BSID[u4InstID], u4InstID);//se(v)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"pps_tc_offset_div2 %d\n", pH265_PPS_DataInst->i4DeblockingFilterTcOffsetDiv2);
            #endif
        }
    }

    pH265_PPS_DataInst->bPPSScalingListPresentFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
#if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"pps_scaling_list_data_present_flag %d\n", pH265_PPS_DataInst->bPPSScalingListPresentFlag);
#endif
    if( pH265_PPS_DataInst->bPPSScalingListPresentFlag )
    {
        if ( PARSE_OK != vHEVCVerifySL_Rbsp( _u4BSID[u4InstID], &(pH265_PPS_DataInst->rPPS_ScalingList)) ){
            H265_DRV_PARSE_SET_ERR_RET(-21);
            return PPS_SYNTAX_ERROR;
        }
    }

    pH265_PPS_DataInst->bListsModificationPresentFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
#if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"lists_modification_present_flag %d\n", pH265_PPS_DataInst->bListsModificationPresentFlag);
#endif
    pH265_PPS_DataInst->u4Log2ParallelMergeLevelMinus2 = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
#if DEBUG_LEVEL >= DBG_LEVEL_INFO
          DBG_H265_PRINTF(pfLogFile,"log2_parallel_merge_level_minus2  %d\n", pH265_PPS_DataInst->u4Log2ParallelMergeLevelMinus2);
#endif
    pH265_PPS_DataInst->bSliceHeaderExtensionPresentFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
#if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"slice_segment_header_extension_present_flag %d\n", pH265_PPS_DataInst->bSliceHeaderExtensionPresentFlag);
#endif

    pH265_PPS_DataInst->bPPSExtensionFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
#if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"pps_extension_flag %d\n", pH265_PPS_DataInst->bPPSExtensionFlag);
#endif

    vVDEC_HAL_H265_TrailingBits(_u4BSID[u4InstID], u4InstID);

    pH265_PPS_DataInst->bPPSValid = TRUE;

#if DEBUG_LEVEL >= DBG_LEVEL_INFO
    printk ("[INFO] vHEVCVerifyPPS_Rbsp() done!!\n");
#endif
    return PARSE_OK;
}

// *********************************************************************
// Function    : void vHEVCVerifyInitTilesInfo(UINT32 u4VDecID, H265_PPS_Data *prPPS)
// Description : Set Tiles Info
// Parameter   : None
// Return      : None
// *********************************************************************

void vHEVCVerifyInitTilesInfo(UINT32 u4VDecID, H265_PPS_Data *prPPS)
{
      UINT32  uiTileIdx;
      UINT32  uiColumnIdx = 0;
      UINT32  uiRowIdx = 0;
      UINT32  uiRightEdgePosInCU;
      UINT32  uiBottomEdgePosInCU;
      UINT32  uiCummulativeTileWidth, uiCummulativeTileHeight;
      int   i, j, p;

      // set tiles width and height
      if( prPPS->bUniformSpacingFlag )
      {
            //set the width for each tile
            for(j=0; j < prPPS->u4NumRowsMinus1+1; j++)
            {
                for(p=0; p < prPPS->u4NumColumnsMinus1+1; p++)
                {
                    _rH265PicInfo[u4VDecID].rTileInfo[j * (prPPS->u4NumColumnsMinus1+1) + p].u4TileWidth = 
                        (p+1)*_rH265PicInfo[u4VDecID].u4PicWidthInCU/(prPPS->u4NumColumnsMinus1+1) 
                        - (p*_rH265PicInfo[u4VDecID].u4PicWidthInCU)/(prPPS->u4NumColumnsMinus1+1);
                }
            }

            //set the height for each tile
            for(j=0; j < prPPS->u4NumColumnsMinus1+1; j++)
            {
                for(p=0; p < prPPS->u4NumRowsMinus1+1; p++)
                {
                 _rH265PicInfo[u4VDecID].rTileInfo[p * (prPPS->u4NumColumnsMinus1+1) + j ].u4TileHeight =
                       (p+1)*_rH265PicInfo[u4VDecID].u4PicHeightInCU/(prPPS->u4NumRowsMinus1+1) 
                      - (p*_rH265PicInfo[u4VDecID].u4PicHeightInCU)/(prPPS->u4NumRowsMinus1+1) ;   
                }
            }
      }
      else
      {
            //set the width for each tile
            for(j=0; j < prPPS->u4NumRowsMinus1+1; j++)
            {
                uiCummulativeTileWidth = 0;
                for(i=0; i < prPPS->u4NumColumnsMinus1; i++)
                {
                    _rH265PicInfo[u4VDecID].rTileInfo[j * (prPPS->u4NumColumnsMinus1+1) + i].u4TileWidth =  prPPS->u4ColumnWidthMinus1[i]+1;
                    uiCummulativeTileWidth += prPPS->u4ColumnWidthMinus1[i]+1;
                }
                _rH265PicInfo[u4VDecID].rTileInfo[j * (prPPS->u4NumColumnsMinus1+1) + i].u4TileWidth = ( _rH265PicInfo[u4VDecID].u4PicWidthInCU-uiCummulativeTileWidth );
            }

            //set the height for each tile
            for(j=0; j < prPPS->u4NumColumnsMinus1+1; j++)
            {
                uiCummulativeTileHeight = 0;
                for(i=0; i < prPPS->u4NumRowsMinus1; i++)
                { 
                    _rH265PicInfo[u4VDecID].rTileInfo[i * (prPPS->u4NumColumnsMinus1+1) + j].u4TileHeight = prPPS->u4RowHeightMinus1[i]+1;
                    uiCummulativeTileHeight += prPPS->u4RowHeightMinus1[i]+1;
                }
                _rH265PicInfo[u4VDecID].rTileInfo[i * (prPPS->u4NumColumnsMinus1+1) + j].u4TileHeight = ( _rH265PicInfo[u4VDecID].u4PicHeightInCU-uiCummulativeTileHeight );
            }
      }

   
      //initialize each tile of the current picture
      for( uiRowIdx=0; uiRowIdx < prPPS->u4NumRowsMinus1+1; uiRowIdx++ )
      {
            for( uiColumnIdx=0; uiColumnIdx < prPPS->u4NumColumnsMinus1+1; uiColumnIdx++ )
            {
                uiTileIdx = uiRowIdx * (prPPS->u4NumColumnsMinus1+1) + uiColumnIdx;

                //initialize the RightEdgePosInCU for each tile
                uiRightEdgePosInCU = 0;
                for( i=0; i <= uiColumnIdx; i++ )
                {
                    uiRightEdgePosInCU += _rH265PicInfo[u4VDecID].rTileInfo[uiRowIdx * (prPPS->u4NumColumnsMinus1+1) + i].u4TileWidth;
                }
                _rH265PicInfo[u4VDecID].rTileInfo[uiTileIdx].u4RightEdgePosInCU = (uiRightEdgePosInCU-1);

                //initialize the BottomEdgePosInCU for each tile
                uiBottomEdgePosInCU = 0;
                for( i=0; i <= uiRowIdx; i++ )
                {
                    uiBottomEdgePosInCU += _rH265PicInfo[u4VDecID].rTileInfo[i * (prPPS->u4NumColumnsMinus1+1) + uiColumnIdx].u4TileHeight;
                }
                _rH265PicInfo[u4VDecID].rTileInfo[uiTileIdx].u4BottomEdgePosInCU = (uiBottomEdgePosInCU-1);

                //initialize the FirstCUAddr for each tile
                _rH265PicInfo[u4VDecID].rTileInfo[uiTileIdx].u4FirstCUAddr = ( (_rH265PicInfo[u4VDecID].rTileInfo[uiTileIdx].u4BottomEdgePosInCU - 
                    _rH265PicInfo[u4VDecID].rTileInfo[uiTileIdx].u4TileHeight +1)*_rH265PicInfo[u4VDecID].u4PicWidthInCU + 
                    _rH265PicInfo[u4VDecID].rTileInfo[uiTileIdx].u4RightEdgePosInCU - _rH265PicInfo[u4VDecID].rTileInfo[uiTileIdx].u4TileWidth + 1);
                
            }
      }

      
}


// *********************************************************************
// Function    : void vHEVCVerifyInitPicInfo(UINT32 u4InstID)
// Description : init pic info
// Parameter   : None
// Return      : None
// *********************************************************************

void vHEVCVerifyInitPicInfo(UINT32 u4InstID)
{
    int i;
    _rH265PicInfo[u4InstID].u4SliceCnt = 0;
    _rH265PicInfo[u4InstID].u4IqSramAddrAccCnt = 0;
    _rH265PicInfo[u4InstID].u4PicWidthInCU = 0;
    _rH265PicInfo[u4InstID].u4PicHeightInCU = 0;
    
    //_rH265PicInfo[u4InstID].i4StrNumDeltaPocs = 0;
    //_rH265PicInfo[u4InstID].i4StrNumNegPosPics = 0;
    _rH265PicInfo[u4InstID].i4MaxStrNumNegPosPics = 0;
    _rH265PicInfo[u4InstID].i4NumLongTerm = 0;
    _rH265PicInfo[u4InstID].i4NumLongTermSps = 0;

    _rH265PicInfo[u4InstID].i4DpbLTBuffCnt = 0;
    _rH265PicInfo[u4InstID].i4RefListNum = 0;
    _rH265PicInfo[u4InstID].bLowDelayFlag = 0;

    for(i = 0; i<16;i++)
    {
    	_rH265PicInfo[u4InstID].i4PocDiffList0[i] = 0;
    	_rH265PicInfo[u4InstID].i4PocDiffList1[i] = 0;
    	_rH265PicInfo[u4InstID].i4LongTermList0[i] = 0;
    	_rH265PicInfo[u4InstID].i4LongTermList1[i] = 0;
       _rH265PicInfo[u4InstID].i4BuffIdList0[i] = 0;
       _rH265PicInfo[u4InstID].i4BuffIdList1[i] = 0;
       _rH265PicInfo[u4InstID].i4DpbLTBuffId[i] = 0;
       _rH265PicInfo[u4InstID].i4List0DecOrderNo[i] = 0;
       _rH265PicInfo[u4InstID].i4List1DecOrderNo[i] = 0;
    }
}


// *********************************************************************
// Function    : void vHEVCVerifyInitSPS(H265_SPS_Data *prSPS)
// Description : Init SPS related fields
// Parameter   : None
// Return      : None
// *********************************************************************
void vHEVCVerifyInitSPS(H265_SPS_Data *prSPS)
{
    INT32 i;
    int sizeId,listId;
        
    prSPS->u4MaxCUWidth = 32;
    prSPS->u4MaxCUHeight = 32;
    prSPS->u4MaxCUDepth = 3;
    prSPS->u4PCMLog2LumaMaxSize = 5;
    prSPS->u4PCMLog2LumaMinSizeMinus3 = 4;
    prSPS->u4ChromaFormatIdc = 1;
    prSPS->u4BitDepthLumaMinus8 = 0;
    prSPS->u4BitDepthChromaMinus8 = 0;
    prSPS->u4PCMBitDepthLumaMinus1 = 7;
    prSPS->u4PCMBitDepthChromaMinus1= 7;
    prSPS->u4BitDepthLumaMinus8 = 0;
    prSPS->u4BitDepthChromaMinus8 = 0;
    prSPS->u4Log2MaxPicOrderCntLsbMinus4 = 4;
    prSPS->u4MaxTrSize = 32;
    prSPS->u4NumLongTermRefPicSPS = 0;
    prSPS->u4NumShortTermRefPicSets = 0;
    prSPS->u4NumRefFrames = 0;

    for ( i = 0; i < MAX_TLAYER; i++ ) {
        prSPS->u4MaxLatencyIncrease[i] = 0;
        prSPS->u4MaxDecPicBuffering[i] = 1;
        prSPS->u4NumReorderPics[i] = 0;
    }

    for ( i = 0; i < 32; i++ ) {
        prSPS->pShortTermRefPicSets[i] = NULL;
    }

    // init scaling list
    for( sizeId = 0; sizeId < SCALING_LIST_SIZE_NUM; sizeId++)
    {
        for( listId = 0; listId < H265_scalingListNum[sizeId]; listId++)
        {
            int size = (MAX_MATRIX_COEF_NUM < H265_scalingListSize[sizeId])? MAX_MATRIX_COEF_NUM : H265_scalingListSize[sizeId];
            if (prSPS->bSL_Init){
                vfree(prSPS->rSPS_ScalingList.pScalingListDeltaCoef[sizeId][listId]);
            }
            prSPS->rSPS_ScalingList.pScalingListDeltaCoef[sizeId][listId] = (INT32*)vmalloc( size*sizeof(INT32) );
        }
    }
    prSPS->rSPS_ScalingList.pScalingListDeltaCoef[SCALING_LIST_32x32][3] = prSPS->rSPS_ScalingList.pScalingListDeltaCoef[SCALING_LIST_32x32][1];
    prSPS->bSL_Init = 1;

}


// *********************************************************************
// Function    : void vHEVCVerifyInitSliceHdr()
// Description : 
// Parameter   : None
// Return      : None
// *********************************************************************
void vHEVCVerifyInitSliceHdr(UINT32 u4InstID, VDEC_INFO_DEC_PRM_T *tVerMpvDecPrm)
{

    if((_ucNalUnitType[u4InstID] == NAL_UNIT_CODED_SLICE_BLA)   ||
       (_ucNalUnitType[u4InstID] == NAL_UNIT_CODED_SLICE_BLANT) |
       (_ucNalUnitType[u4InstID] == NAL_UNIT_CODED_SLICE_BLA_N_LP) ||
       (_ucNalUnitType[u4InstID] == NAL_UNIT_CODED_SLICE_IDR) ||
       (_ucNalUnitType[u4InstID] == NAL_UNIT_CODED_SLICE_IDR_N_LP) )
    {
        tVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSliceHdr->bNoRaslOutputFlag = 1;
    }
    else if(_ucNalUnitType[u4InstID]==NAL_UNIT_CODED_SLICE_CRA)
    {
        if( tVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.bFirstSliceInSequence)
            tVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSliceHdr->bNoRaslOutputFlag = 1;
        else if(tVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSliceHdr->bFirstSliceSegmentInPic )
            tVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSliceHdr->bNoRaslOutputFlag = 0;
    } else{
        tVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSliceHdr->bNoRaslOutputFlag = 0;
    }

    tVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSliceHdr->u4NumOfLongTermPics = 0;
    tVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSliceHdr->u4NumOfLongTermSPS = 0;
    tVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSliceHdr->i4NumRefIdx[REF_PIC_LIST_0] = 0;
    tVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSliceHdr->i4NumRefIdx[REF_PIC_LIST_1] = 0;
    tVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSliceHdr->i4NumRefIdx[REF_PIC_LIST_C] = 0;
    tVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSliceHdr->u4ColRefIdx = 0;
    tVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSliceHdr->bMvdL1ZeroFlag = 0;

    tVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSliceHdr->bColFromL0Flag = 1;
    tVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSliceHdr->bTMVPFlagsPresent = 1;

    tVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSliceHdr->i4SliceQpDeltaCb = 0;
    tVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSliceHdr->i4SliceQpDeltaCr = 0;

    tVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSliceHdr->i4POC = 0;
    tVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSliceHdr->i4POCLsb = 0;
    tVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSliceHdr->i4POCMsb = 0;
    tVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSliceHdr->bCabacInitFlag = 0;
    tVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSliceHdr->u4FiveMinusMaxNumMergeCand = MRG_MAX_NUM_CANDS;
    tVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.bFirstSliceInSequence = 0;

    tVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSliceHdr->rLocalRPS.u4NumberOfNegativePictures = 0;
    tVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSliceHdr->rLocalRPS.u4NumberOfPositivePictures = 0;
    tVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSliceHdr->rLocalRPS.u4NumberOfPictures = 0;
    tVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSliceHdr->rLocalRPS.u4NumberOfLongtermPictures = 0;
    tVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSliceHdr->rLocalRPS.u4DeltaRIdxMinus1 = 0;
    tVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSliceHdr->rLocalRPS.u4DeltaRPS = 0;
    tVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSliceHdr->rLocalRPS.bInterRPSPrediction = 0;
    tVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSliceHdr->rLocalRPS.u4NumRefIdc = 0;


}


// *********************************************************************
// Function    : UINT32 vHEVCParseSliceHeader(UINT32 u4InstID)
// Description : Parse Slice Header then to dec
// Parameter   : None
// Return      : UINT32
// *********************************************************************
UINT32 vHEVCParseSliceHeader(UINT32 u4InstID)
{

    H265_Slice_Hdr_Data *pH265_Slice_Hdr_DataInst;
    H265_SPS_Data *pH265_SPS_DataInst = NULL;
    H265_PPS_Data *pH265_PPS_DataInst = NULL;
    VDEC_INFO_DEC_PRM_T  *ptVerMpvDecPrm;
    pH265_RPS_Data pSliceRPS;
    INT32 i4tmp = 0;
    INT32 i4Indexer = 0;
    INT32 i4Indexer2 = 0;
    INT32 i4Indexer3 = 0;
    INT32 i4BitsSliceSegmentAddress, i4NumCTUs, i4MaxParts, i4NumBits;
    INT32 i4NumOfLtrp, i4NumRpsCurrTempList = 0;
    BOOL bDeltaFlag = 0;
    BOOL bSAOEnabled, bDBFEnabled;
    int iPOClsb, iPrevPOC, iMaxPOClsb, iPrevPOClsb, iPrevPOCmsb, iPOCmsb;
    int iOffset, iNumLtrpInSPS, iPrevDeltaMSB, iDeltaPocMSBCycleLT, iPocLsbLt, iPocLTCurr;

    unsigned int u4NalTemporalID = 0;
    unsigned int u4NalType = 0;
#if DEBUG_LEVEL >= DBG_LEVEL_INFO
    printk ("[INFO] vHEVCParseSliceHeader() start!!\n");
#endif


    u4NalTemporalID = _u4NuhTemporalId[u4InstID];
    u4NalType = _ucNalUnitType[u4InstID];

    pH265_Slice_Hdr_DataInst = &_rH265SliceHdr[u4InstID];
    pH265_Slice_Hdr_DataInst->u4NalType = u4NalType;

    ptVerMpvDecPrm = &_tVerMpvDecPrm[u4InstID];
    ptVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSliceHdr = pH265_Slice_Hdr_DataInst;
    ptVerMpvDecPrm->ucPicStruct = FRAME;

    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"[INFO] parsing [%d] slice data\n", u4NalType );
    #endif

    pH265_Slice_Hdr_DataInst->bFirstSliceSegmentInPic = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"first_slice_segment_in_pic_flag  %d\n", pH265_Slice_Hdr_DataInst->bFirstSliceSegmentInPic);
    #endif

    if( pH265_Slice_Hdr_DataInst->bFirstSliceSegmentInPic )
    {
        pH265_Slice_Hdr_DataInst->bDeblockingFilterDisableFlag = 0;
        pH265_Slice_Hdr_DataInst->bDeblockingFilterOverrideFlag = 1;
    }

    if ( 16 <= u4NalType && 23>= u4NalType  ){      //IRAP picture
        i4tmp = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"no_output_of_prior_pics_flag  %d\n", i4tmp );
        #endif
    }
    pH265_Slice_Hdr_DataInst->u4PPSID = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
        DBG_H265_PRINTF(pfLogFile,"slice_pic_parameter_set_id  %d\n", pH265_Slice_Hdr_DataInst->u4PPSID);
    #endif

#if DEBUG_LEVEL >= DBG_LEVEL_BUG1
            DBG_H265_PRINTF(pfLogFile,"[INFO] Slice PPS[%d] SPS[%d] \n", pH265_Slice_Hdr_DataInst->u4PPSID,_rH265PPS[u4InstID][pH265_Slice_Hdr_DataInst->u4PPSID].u4SeqParameterSetId );
#endif

    if((pH265_Slice_Hdr_DataInst->u4PPSID < 256)
        && (_rH265PPS[u4InstID][pH265_Slice_Hdr_DataInst->u4PPSID].bPPSValid)
        && (_rH265SPS[u4InstID][_rH265PPS[u4InstID][pH265_Slice_Hdr_DataInst->u4PPSID].u4SeqParameterSetId].bSPSValid))
    {
        ptVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prPPS = &_rH265PPS[u4InstID][pH265_Slice_Hdr_DataInst->u4PPSID];
        pH265_PPS_DataInst = ptVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prPPS;
        ptVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSPS = &_rH265SPS[u4InstID][pH265_PPS_DataInst->u4SeqParameterSetId];
        pH265_SPS_DataInst = ptVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSPS;
  
    }
    else
    {
        #if DEBUG_LEVEL >= DBG_LEVEL_BUG1
            DBG_H265_PRINTF(pfLogFile,"[ERROR] Slice Hdr PPS Num err !! \n");
        #endif
        sprintf(_bTempStr1[u4InstID], "%s", "err in Slice Hdr PPS Num err\\n\\0");
        vErrMessage(u4InstID, (CHAR *)_bTempStr1[u4InstID]);
        if ( _rH265PPS[u4InstID][pH265_Slice_Hdr_DataInst->u4PPSID].bPPSValid ){       // given PPS id is valid
            ptVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prPPS = &_rH265PPS[u4InstID][pH265_Slice_Hdr_DataInst->u4PPSID];
            pH265_PPS_DataInst = ptVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prPPS;
        } else if ( _rH265PPS[u4InstID][0].bPPSValid ){                                                 // PPS id = 0 is valid
            ptVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prPPS = &_rH265PPS[u4InstID][0];
            pH265_PPS_DataInst = ptVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prPPS;
            #if DEBUG_LEVEL >= DBG_LEVEL_BUG1
                DBG_H265_PRINTF(pfLogFile,"[ERROR] USE PPS[0]\n");
            #endif
        } else {
            #if DEBUG_LEVEL >= DBG_LEVEL_BUG1
                DBG_H265_PRINTF(pfLogFile,"[ERROR] no valid PPS\n");
            #endif
            return SLICE_SYNTAX_ERROR;
        }

        if ( _rH265SPS[u4InstID][_rH265PPS[u4InstID][pH265_Slice_Hdr_DataInst->u4PPSID].u4SeqParameterSetId].bSPSValid ){       // given SPS id is valid
            ptVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSPS = &_rH265SPS[u4InstID][pH265_PPS_DataInst->u4SeqParameterSetId];
            pH265_SPS_DataInst = ptVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSPS;

        } else if ( _rH265SPS[u4InstID][0].bSPSValid ){                                                   // SPS id = 0 is valid
            ptVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSPS = &_rH265SPS[u4InstID][0];
            pH265_SPS_DataInst = ptVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.prSPS;
            #if DEBUG_LEVEL >= DBG_LEVEL_BUG1
                DBG_H265_PRINTF(pfLogFile,"[ERROR] USE SPS[0]\n");
            #endif
        } else {
            #if DEBUG_LEVEL >= DBG_LEVEL_BUG1
                DBG_H265_PRINTF(pfLogFile,"[ERROR] no valid SPS\n");
            #endif
            return SLICE_SYNTAX_ERROR;
        }
    }

    vHEVCVerifyInitSliceHdr(u4InstID, ptVerMpvDecPrm);

    if(1 != pH265_Slice_Hdr_DataInst->bFirstSliceSegmentInPic && pH265_PPS_DataInst->bDependentSliceSegmentsEnabledFlag){
        pH265_Slice_Hdr_DataInst->bDependentSliceSegmentFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"dependent_slice_segment_flag  %d\n", pH265_Slice_Hdr_DataInst->bDependentSliceSegmentFlag);
        #endif
    } else {
        pH265_Slice_Hdr_DataInst->bDependentSliceSegmentFlag = 0;
    }
    
    i4NumCTUs = ((pH265_SPS_DataInst->u4PicWidthInLumaSamples+pH265_SPS_DataInst->u4MaxCUWidth-1)/pH265_SPS_DataInst->u4MaxCUWidth)*
                       ((pH265_SPS_DataInst->u4PicHeightInLumaSamples+pH265_SPS_DataInst->u4MaxCUHeight-1)/pH265_SPS_DataInst->u4MaxCUHeight);
    i4MaxParts = (1<<(pH265_SPS_DataInst->u4MaxCUDepth<<1));

    //calculation of SliceSegmentAddress
    i4BitsSliceSegmentAddress = 0;
    while(i4NumCTUs>(1<<i4BitsSliceSegmentAddress)){
        i4BitsSliceSegmentAddress++;
    }
    
    if(1 != pH265_Slice_Hdr_DataInst->bFirstSliceSegmentInPic){
        pH265_Slice_Hdr_DataInst->u4SliceSegmentAddress  = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID,i4BitsSliceSegmentAddress);//u(v)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"slice_segment_address %d\n", pH265_Slice_Hdr_DataInst->u4SliceSegmentAddress );
        #endif

    }

    if ( 0==pH265_Slice_Hdr_DataInst->bDependentSliceSegmentFlag ){
        for ( i4Indexer=0; i4Indexer<pH265_PPS_DataInst->u4NumExtraSliceHeaderBits; i4Indexer++){
            i4tmp = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"slice_reserved_undetermined_flag  %d\n", i4tmp);
            #endif
        }
        pH265_Slice_Hdr_DataInst->u4SliceType = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"slice_type  %d\n", pH265_Slice_Hdr_DataInst->u4SliceType);
        #endif

        if (pH265_Slice_Hdr_DataInst->u4SliceType >= H265_Slice_Type_MAX) {
            if ( 16 <= u4NalType && 23>= u4NalType  ){
                DBG_H265_PRINTF(pfLogFile,"[Err] Unsupport slice type [%d] naltype:%d set I_SLICE\n", pH265_Slice_Hdr_DataInst->u4SliceType, u4NalType);
                pH265_Slice_Hdr_DataInst->u4SliceType = HEVC_I_SLICE;
            } else {
                DBG_H265_PRINTF(pfLogFile,"[Err] Unsupport slice type [%d] naltype:%d set B_SLICE\n", pH265_Slice_Hdr_DataInst->u4SliceType, u4NalType);
                pH265_Slice_Hdr_DataInst->u4SliceType = HEVC_B_SLICE;
            }       
            H265_DRV_PARSE_SET_ERR_RET(-12);
        }
        if ( pH265_PPS_DataInst->bOutputFlagPresentFlag ){
            pH265_Slice_Hdr_DataInst->bPicOutputFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"pic_output_flag  %d\n", pH265_Slice_Hdr_DataInst->bPicOutputFlag);
            #endif
        }

        ////////RW_HEVC_SLICE_0 register setting
        if ( pH265_Slice_Hdr_DataInst->bFirstSliceSegmentInPic ){
            UINT32 hevc_slice_0_reg;
            BOOL isIDRflag;
            isIDRflag = (u4NalType ==NAL_UNIT_CODED_SLICE_IDR) || (u4NalType ==NAL_UNIT_CODED_SLICE_IDR_N_LP);
            hevc_slice_0_reg = (( 0 & 0xfffff) << 0) |
                                            ((i4BitsSliceSegmentAddress & 0x1f) << 20) |
                                            ((pH265_Slice_Hdr_DataInst->u4SliceType & 0x3) << 28) |
                                            ((pH265_Slice_Hdr_DataInst->bDependentSliceSegmentFlag & 0x1) << 30) |
                                            ((isIDRflag & 0x1) << 31); 
            vVDecWriteHEVCVLD(u4InstID, RW_HEVC_SLICE_0, hevc_slice_0_reg);
            vHEVCVerifyInitPicInfo(u4InstID);
        }
        
        if ( pH265_SPS_DataInst->bSeparateColourPlaneFlag ){
            pH265_Slice_Hdr_DataInst->u4ColourPlaneID = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID,2);//u(2)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"colour_plane_id  %d\n", pH265_Slice_Hdr_DataInst->u4ColourPlaneID);
            #endif
        }

        if  ( u4NalType == NAL_UNIT_CODED_SLICE_IDR || u4NalType == NAL_UNIT_CODED_SLICE_IDR_N_LP ){ // IDR picture
        
            pH265_Slice_Hdr_DataInst->i4POC = 0;
            pH265_Slice_Hdr_DataInst->pShortTermRefPicSets = &(pH265_Slice_Hdr_DataInst->rLocalRPS);
            pH265_Slice_Hdr_DataInst->pShortTermRefPicSets->u4NumberOfNegativePictures = 0;
            pH265_Slice_Hdr_DataInst->pShortTermRefPicSets->u4NumberOfPositivePictures = 0;
            pH265_Slice_Hdr_DataInst->pShortTermRefPicSets->u4NumberOfLongtermPictures = 0;
            pH265_Slice_Hdr_DataInst->pShortTermRefPicSets->u4NumberOfPictures = 0;
            pH265_Slice_Hdr_DataInst->i4POCLsb = 0;
            pH265_Slice_Hdr_DataInst->i4POCMsb = 0;

            if ( 0==u4NalTemporalID )
            {
                ptVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.i4PrePOC = 0;
                ptVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.i4PrevT0POCLsb = 0;
                ptVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.i4PrevT0POCMsb = 0;     
            }
    
        } else {
        
            pH265_Slice_Hdr_DataInst->i4POCLsb = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID,pH265_SPS_DataInst->u4Log2MaxPicOrderCntLsbMinus4 + 4);//u(v)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"pic_order_cnt_lsb  %d; SPS.u4Log2MaxPicOrderCntLsbMinus4 = %d\n", pH265_Slice_Hdr_DataInst->i4POCLsb, pH265_SPS_DataInst->u4Log2MaxPicOrderCntLsbMinus4);
            #endif

            // slice POC calculation
            iPOClsb = pH265_Slice_Hdr_DataInst->i4POCLsb;
            iPrevPOC = ptVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.i4PrePOC;
            iMaxPOClsb = 1<< (pH265_SPS_DataInst->u4Log2MaxPicOrderCntLsbMinus4+4);
           
            iPrevPOClsb = ptVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.i4PrevT0POCLsb;
            iPrevPOCmsb = ptVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.i4PrevT0POCMsb;

            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"[INFO] iPrevPOC = %d; iPrevPOClsb = %d; iPrevPOCmsb = %d\n", iPrevPOC, iPrevPOClsb, iPrevPOCmsb);
            #endif          

            if( ( iPOClsb  <  iPrevPOClsb ) && ( ( iPrevPOClsb - iPOClsb )  >=  ( iMaxPOClsb / 2 ) ) ) {
                iPOCmsb = iPrevPOCmsb + iMaxPOClsb;
            } else if ( (iPOClsb  >  iPrevPOClsb )  && ( (iPOClsb - iPrevPOClsb )  >  ( iMaxPOClsb / 2 ) ) ) {
                iPOCmsb = iPrevPOCmsb - iMaxPOClsb;
            } else {
                iPOCmsb = iPrevPOCmsb;
            }
            
            if ( u4NalType == NAL_UNIT_CODED_SLICE_BLA
              || u4NalType == NAL_UNIT_CODED_SLICE_BLANT
              || u4NalType == NAL_UNIT_CODED_SLICE_BLA_N_LP ) {
              // For BLA picture types, POCmsb is set to 0.
                  iPOCmsb = 0;
            }
            if ( u4NalType == NAL_UNIT_CODED_SLICE_CRA && pH265_Slice_Hdr_DataInst->bNoRaslOutputFlag )
            {
                iPOCmsb = 0;
            }
            pH265_Slice_Hdr_DataInst->i4POCLsb = iPOClsb;
            pH265_Slice_Hdr_DataInst->i4POCMsb = iPOCmsb;
            
            // get POC
            pH265_Slice_Hdr_DataInst->i4POC = (iPOCmsb+iPOClsb);
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"[INFO] Calculated POC %d; \n", pH265_Slice_Hdr_DataInst->i4POC);
            #endif
            
            BOOL bTemporalLayerNonReferenceFlag = 0;
            if((u4NalType == NAL_UNIT_CODED_SLICE_TRAIL_N) ||
               (u4NalType == NAL_UNIT_CODED_SLICE_TSA_N) ||
               (u4NalType == NAL_UNIT_CODED_SLICE_STSA_N) ||
               (u4NalType == NAL_UNIT_CODED_SLICE_RADL_N) ||
               (u4NalType == NAL_UNIT_CODED_SLICE_RASL_N))
            {
               bTemporalLayerNonReferenceFlag = 1;
            }
        
            if ( 0==u4NalTemporalID && !bTemporalLayerNonReferenceFlag && 
                u4NalType!=NAL_UNIT_CODED_SLICE_DLP &&
                u4NalType!=NAL_UNIT_CODED_SLICE_TFD)
            {
                ptVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.i4PrePOC = pH265_Slice_Hdr_DataInst->i4POC;
                ptVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.i4PrevT0POCLsb = iPOClsb;
                ptVerMpvDecPrm->SpecDecPrm.rVDecH265DecPrm.i4PrevT0POCMsb = iPOCmsb;
            }
            
            i4tmp = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"short_term_ref_pic_set_sps_flag  %d\n", i4tmp );
            #endif
            if ( 0 == i4tmp ){     //! short_term_ref_pic_set_sps_flag
                pSliceRPS = &(pH265_Slice_Hdr_DataInst->rLocalRPS);
                
                /////////////////////////////////////////////////////////////////////
                 if ( PARSE_OK != vHEVCVerifyRPS_Rbsp(_u4BSID[u4InstID], pH265_SPS_DataInst, pSliceRPS, pH265_SPS_DataInst->u4NumShortTermRefPicSets )){
                     #if DEBUG_LEVEL >= DBG_LEVEL_BUG1
                        DBG_H265_PRINTF(pfLogFile,"[ERROR] RPS syntex Error!! \n" );
                     #endif
                     return SLICE_SYNTAX_ERROR; 
                 }
                /////////////////////////////////////////////////////////////////////
         
                pH265_Slice_Hdr_DataInst->pShortTermRefPicSets = pSliceRPS;

            } else  {
                //calculation of BitsForShortermRPSidx
                i4NumBits = 0;
                while ((1 << i4NumBits) < pH265_SPS_DataInst->u4NumShortTermRefPicSets){
                    i4NumBits++;
                }
                if ( i4NumBits > 0 ){
                    i4tmp = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID,i4NumBits);//u(v)
                    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                        DBG_H265_PRINTF(pfLogFile,"short_term_ref_pic_set_idx %d\n", i4tmp );
                    #endif
                    if ( i4tmp>= pH265_SPS_DataInst->u4NumShortTermRefPicSets ){
                         #if DEBUG_LEVEL >= DBG_LEVEL_BUG1
                            DBG_H265_PRINTF(pfLogFile,"[ERROR] short_term_ref_pic_set_idx(%d) > SPS.NumShortTermRefPicSets(%d) -1 !! \n", i4tmp, pH265_SPS_DataInst->u4NumShortTermRefPicSets );
                         #endif
                         i4tmp = 0;
                    }
                } else {
                    i4tmp = 0;
                }
                pH265_Slice_Hdr_DataInst->pShortTermRefPicSets = pH265_SPS_DataInst->pShortTermRefPicSets[i4tmp];
                pSliceRPS = pH265_Slice_Hdr_DataInst->pShortTermRefPicSets;
            }

            if ( 1 == pH265_SPS_DataInst->bLongTermRefsPresent ){
                iOffset = pSliceRPS->u4NumberOfNegativePictures + pSliceRPS->u4NumberOfPositivePictures;
                iNumLtrpInSPS = 0;
                
                i4NumOfLtrp = 0;
                if ( 0 <  pH265_SPS_DataInst->u4NumLongTermRefPicSPS ){
                    pH265_Slice_Hdr_DataInst->u4NumOfLongTermSPS = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
                    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                        DBG_H265_PRINTF(pfLogFile,"num_long_term_sps  %d\n", pH265_Slice_Hdr_DataInst->u4NumOfLongTermSPS  );
                    #endif
                    iNumLtrpInSPS = pH265_Slice_Hdr_DataInst->u4NumOfLongTermSPS;
                    i4NumOfLtrp += iNumLtrpInSPS;
                    pSliceRPS->u4NumberOfLongtermPictures = i4NumOfLtrp;
                }
                
                pH265_Slice_Hdr_DataInst->u4NumOfLongTermPics = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
                #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                    DBG_H265_PRINTF(pfLogFile,"num_long_term_pics  %d\n", pH265_Slice_Hdr_DataInst->u4NumOfLongTermPics );
                #endif
                i4NumOfLtrp += pH265_Slice_Hdr_DataInst->u4NumOfLongTermPics;
                pSliceRPS->u4NumberOfLongtermPictures = i4NumOfLtrp;

                //calculation of BitsForLtrpInSPS
                i4NumBits = 0;
                while (pH265_SPS_DataInst->u4NumLongTermRefPicSPS > (1 << i4NumBits)) {
                    i4NumBits++;
                }
                
                 _rH265PicInfo[u4InstID].i4NumLongTerm = i4NumOfLtrp;
                 _rH265PicInfo[u4InstID].i4NumLongTermSps = iNumLtrpInSPS;

                iPrevDeltaMSB = 0, iDeltaPocMSBCycleLT = 0;
                for ( i4Indexer2 = iOffset+pSliceRPS->u4NumberOfLongtermPictures-1, i4Indexer3 = 0; i4Indexer3 < i4NumOfLtrp; i4Indexer2--, i4Indexer3++){

                    iPocLsbLt = 0;   
                    if ( i4Indexer3<pH265_Slice_Hdr_DataInst->u4NumOfLongTermSPS ){
                        i4tmp = 0;
                        if ( 0< i4NumBits ){
                            i4tmp = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID,i4NumBits);//u(v)
                            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                                DBG_H265_PRINTF(pfLogFile,"lt_idx_sps[i] %d\n", i4tmp );
                            #endif
                        }    
                        iPocLsbLt = pH265_SPS_DataInst->u4LtRefPicPocLsbSps[i4tmp];
                        pSliceRPS->bUsed[i4Indexer2] = pH265_SPS_DataInst->bUsedByCurrPicLtSPSFlag[i4tmp];
                    }else{
                        i4tmp = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID,pH265_SPS_DataInst->u4Log2MaxPicOrderCntLsbMinus4+4);//u(v)
                        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                            DBG_H265_PRINTF(pfLogFile,"poc_lsb_lt[i] %d\n", i4tmp );
                        #endif
                        iPocLsbLt = i4tmp;
                        i4tmp = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
                        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                            DBG_H265_PRINTF(pfLogFile,"used_by_curr_pic_lt_flag[i] %d\n", i4tmp);
                        #endif
                        pSliceRPS->bUsed[i4Indexer2] = i4tmp;
                    }
                    i4tmp = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
                    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                        DBG_H265_PRINTF(pfLogFile,"delta_poc_msb_present_flag[i] %d\n", i4tmp);
                    #endif
                    if ( 1 == i4tmp ){       //SBPresentFlag
                        i4tmp = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
                        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                            DBG_H265_PRINTF(pfLogFile,"delta_poc_msb_cycle_lt[i] %d\n", i4tmp );
                        #endif

                        bDeltaFlag = 0;
                        //            First LTRP || First LTRP from SH  
                        if( (  i4Indexer2 == iOffset+pSliceRPS->u4NumberOfLongtermPictures-1) || ( i4Indexer2 == iOffset+(i4NumOfLtrp-iNumLtrpInSPS)-1) ){
                            bDeltaFlag = 1;
                        }
                        if( 1==bDeltaFlag )
                        {
                          iDeltaPocMSBCycleLT = i4tmp;
                        } else {
                          iDeltaPocMSBCycleLT = i4tmp + iPrevDeltaMSB;              
                        }
                        iPocLTCurr = pH265_Slice_Hdr_DataInst->i4POC -iDeltaPocMSBCycleLT * (1 << (pH265_SPS_DataInst->u4Log2MaxPicOrderCntLsbMinus4+4)) 
                                                    - iPOClsb + iPocLsbLt;
                        pSliceRPS->i4POC[i4Indexer2] = iPocLTCurr;
                        pSliceRPS->i4DeltaPOC[i4Indexer2] = iPocLTCurr-pH265_Slice_Hdr_DataInst->i4POC;
                        pSliceRPS->bCheckLTMSB[i4Indexer2] = 1;
                       
                    } else {
                        pSliceRPS->i4POC[i4Indexer2] = iPocLsbLt;
                        pSliceRPS->i4DeltaPOC[i4Indexer2] = iPocLsbLt-pH265_Slice_Hdr_DataInst->i4POC;
                        pSliceRPS->bCheckLTMSB[i4Indexer2] = 0;
                         if(i4Indexer2 == iOffset+(i4NumOfLtrp-iNumLtrpInSPS)-1) {
                             iDeltaPocMSBCycleLT = 0;
                         }
                    }
                    iPrevDeltaMSB = iDeltaPocMSBCycleLT;
                }

                iOffset += pSliceRPS->u4NumberOfLongtermPictures;
                pSliceRPS->u4NumberOfPictures = iOffset;
            }
            
            if ( u4NalType == NAL_UNIT_CODED_SLICE_BLA
                || u4NalType == NAL_UNIT_CODED_SLICE_BLANT
                || u4NalType == NAL_UNIT_CODED_SLICE_BLA_N_LP ) {
                // In the case of BLA picture types, rps data is read from slice header but ignored
                pSliceRPS = &(pH265_Slice_Hdr_DataInst->rLocalRPS);
                pSliceRPS->u4NumberOfNegativePictures = 0;
                pSliceRPS->u4NumberOfPositivePictures = 0;
                pSliceRPS->u4NumberOfLongtermPictures = 0;
                pSliceRPS->u4NumberOfPictures = 0;
                pH265_Slice_Hdr_DataInst->pShortTermRefPicSets = pSliceRPS;
            }
            
            if (1==pH265_SPS_DataInst->bTMVPFlagsPresent){
                pH265_Slice_Hdr_DataInst->bTMVPFlagsPresent = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
                #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                    DBG_H265_PRINTF(pfLogFile,"slice_temporal_mvp_enable_flag %d\n", pH265_Slice_Hdr_DataInst->bTMVPFlagsPresent);
                #endif
            }else{
                pH265_Slice_Hdr_DataInst->bTMVPFlagsPresent = 0;
            }
        } 
     
        if (1==pH265_SPS_DataInst->bUseSAO){
            pH265_Slice_Hdr_DataInst->bSaoEnabledFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"slice_sao_luma_flag %d\n", pH265_Slice_Hdr_DataInst->bSaoEnabledFlag);
            #endif
            pH265_Slice_Hdr_DataInst->bSaoEnabledFlagChroma = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"slice_sao_chroma_flag %d\n", pH265_Slice_Hdr_DataInst->bSaoEnabledFlagChroma);
            #endif
        }
        
        if ( u4NalType == NAL_UNIT_CODED_SLICE_IDR ||u4NalType == NAL_UNIT_CODED_SLICE_IDR_N_LP ){
            pH265_Slice_Hdr_DataInst->bTMVPFlagsPresent = 0;
        }
        if ( pH265_Slice_Hdr_DataInst->u4SliceType != HEVC_I_SLICE ){  // not I slice
            i4tmp = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"num_ref_idx_active_override_flag %d\n", i4tmp);
            #endif
            if (1==i4tmp){
                i4tmp = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
                #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                    DBG_H265_PRINTF(pfLogFile,"num_ref_idx_l0_active_minus1[i] %d\n", i4tmp );
                #endif
                pH265_Slice_Hdr_DataInst->i4NumRefIdx[REF_PIC_LIST_0] = i4tmp + 1;
                if ( pH265_Slice_Hdr_DataInst->u4SliceType == HEVC_B_SLICE ){
                    i4tmp = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
                    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                        DBG_H265_PRINTF(pfLogFile,"num_ref_idx_l1_active_minus1[i] %d\n", i4tmp );
                    #endif
                    pH265_Slice_Hdr_DataInst->i4NumRefIdx[REF_PIC_LIST_1] = i4tmp + 1;
                } else {
                    pH265_Slice_Hdr_DataInst->i4NumRefIdx[REF_PIC_LIST_1] = 0;
                }
            } else {
                pH265_Slice_Hdr_DataInst->i4NumRefIdx[REF_PIC_LIST_0] = pH265_PPS_DataInst->u4NumRefIdxL0DefaultActiveMinus1+1;
                if ( pH265_Slice_Hdr_DataInst->u4SliceType == HEVC_B_SLICE ){
                    pH265_Slice_Hdr_DataInst->i4NumRefIdx[REF_PIC_LIST_1] = pH265_PPS_DataInst->u4NumRefIdxL1DefaultActiveMinus1+1;
                } else {
                    pH265_Slice_Hdr_DataInst->i4NumRefIdx[REF_PIC_LIST_1] = 0;
                }
            }
        }

        // calculate i4NumRpsCurrTempList
        if (pH265_Slice_Hdr_DataInst->pShortTermRefPicSets ==NULL){
            #if DEBUG_LEVEL >= DBG_LEVEL_BUG1
                DBG_H265_PRINTF(pfLogFile,"[ERROR] pShortTermRefPicSets is NULL\n");
            #endif
            return SLICE_SYNTAX_ERROR;
        }

        i4NumRpsCurrTempList = 0;
        for( i4Indexer=0; i4Indexer < pH265_Slice_Hdr_DataInst->pShortTermRefPicSets->u4NumberOfNegativePictures 
                +pH265_Slice_Hdr_DataInst->pShortTermRefPicSets->u4NumberOfPositivePictures
                +pH265_Slice_Hdr_DataInst->pShortTermRefPicSets->u4NumberOfLongtermPictures; i4Indexer++) {
            if(pH265_Slice_Hdr_DataInst->pShortTermRefPicSets->bUsed[i4Indexer]) {
                i4NumRpsCurrTempList++;
            }
        }
        
        if ( pH265_Slice_Hdr_DataInst->u4SliceType != HEVC_I_SLICE ){  // not I slice

            ////////RW_HEVC_SLICE_2 register setting
            if ( pH265_Slice_Hdr_DataInst->bFirstSliceSegmentInPic ){
                    UINT32 hevc_slice_2_reg;
                    hevc_slice_2_reg = (((pH265_Slice_Hdr_DataInst->i4NumRefIdx[REF_PIC_LIST_0]-1) & 0xf) << 0) |
                                                 (((pH265_Slice_Hdr_DataInst->i4NumRefIdx[REF_PIC_LIST_1]-1) & 0xf) << 4);
                    vVDecWriteHEVCVLD(u4InstID, RW_HEVC_SLICE_2, hevc_slice_2_reg);  
            }
            
            if ( pH265_PPS_DataInst->bListsModificationPresentFlag && i4NumRpsCurrTempList > 1 ){

                ////////RW_HEVC_REF_PIC_LIST_MOD register setting
                if ( pH265_Slice_Hdr_DataInst->bFirstSliceSegmentInPic ){
                    UINT32 hevc_ref_pic_list_mod_reg;
                    int length = 0;
                    int numRpsCurrTempList0 = i4NumRpsCurrTempList;
                    if ( numRpsCurrTempList0 > 1 ){
                            length = 1;
                            numRpsCurrTempList0 --;
                    }
                    while ( numRpsCurrTempList0 >>= 1) {
                        length ++;
                    }
                    hevc_ref_pic_list_mod_reg = ((i4NumRpsCurrTempList & 0xf) << 0) |
								      ((length & 0x7) << 4);
                    vVDecWriteHEVCVLD(u4InstID, RW_HEVC_REF_PIC_LIST_MOD, hevc_ref_pic_list_mod_reg);  

                    /////////////////////////////////////////////////////////////////////
                    vHEVCVerifyRef_Pic_List_Modification(u4InstID, pH265_Slice_Hdr_DataInst);
                    /////////////////////////////////////////////////////////////////////
                
                }

            }

            if ( pH265_Slice_Hdr_DataInst->u4SliceType == HEVC_B_SLICE ){
                pH265_Slice_Hdr_DataInst->bMvdL1ZeroFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
                #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                    DBG_H265_PRINTF(pfLogFile,"mvd_l1_zero_flag %d\n", pH265_Slice_Hdr_DataInst->bMvdL1ZeroFlag );
                #endif
            }
        
            pH265_Slice_Hdr_DataInst->bCabacInitFlag = 0; //default
            if ( 1==pH265_PPS_DataInst->bCabacInitPresentFlag ){
                pH265_Slice_Hdr_DataInst->bCabacInitFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
                #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                    DBG_H265_PRINTF(pfLogFile,"cabac_init_flag %d\n", pH265_Slice_Hdr_DataInst->bCabacInitFlag );
                #endif
            }
            if ( 1==pH265_Slice_Hdr_DataInst->bTMVPFlagsPresent ){
                if ( pH265_Slice_Hdr_DataInst->u4SliceType == HEVC_B_SLICE ){
                    pH265_Slice_Hdr_DataInst->bColFromL0Flag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
                    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                        DBG_H265_PRINTF(pfLogFile,"collocated_from_l0_flag %d\n", pH265_Slice_Hdr_DataInst->bColFromL0Flag );
                    #endif
                } else {
                    pH265_Slice_Hdr_DataInst->bColFromL0Flag = 1;
                }
                if ( (1==pH265_Slice_Hdr_DataInst->bColFromL0Flag && 1<pH265_Slice_Hdr_DataInst->i4NumRefIdx[REF_PIC_LIST_0]) ||
                     (0==pH265_Slice_Hdr_DataInst->bColFromL0Flag && 1<pH265_Slice_Hdr_DataInst->i4NumRefIdx[REF_PIC_LIST_1]) ){
                    pH265_Slice_Hdr_DataInst->u4ColRefIdx = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
                    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                        DBG_H265_PRINTF(pfLogFile,"collocated_ref_idx %d\n", pH265_Slice_Hdr_DataInst->u4ColRefIdx );
                    #endif
                } else {
                    pH265_Slice_Hdr_DataInst->u4ColRefIdx  = 0;
                }
            }
            if ( (1==pH265_PPS_DataInst->bWPPredFlag && pH265_Slice_Hdr_DataInst->u4SliceType == HEVC_P_SLICE) ||
                 (1==pH265_PPS_DataInst->bWPBiPredFlag && pH265_Slice_Hdr_DataInst->u4SliceType == HEVC_B_SLICE) ){
                 
                /////////////////////////////////////////////////////////////////////
                vVDEC_HAL_H265_PredWeightTable(u4InstID);
                /////////////////////////////////////////////////////////////////////
            }
            pH265_Slice_Hdr_DataInst->u4FiveMinusMaxNumMergeCand = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"five_minus_max_num_merge_cand %d\n", pH265_Slice_Hdr_DataInst->u4FiveMinusMaxNumMergeCand );
            #endif
        }

        i4tmp= i4VDEC_HAL_H265_SeCodeNum(_u4BSID[u4InstID], u4InstID);//se(v)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"slice_qp_delta %d\n", i4tmp );
        #endif
        pH265_Slice_Hdr_DataInst->i4SliceQp = i4tmp+pH265_PPS_DataInst->i4PicInitQPMinus26+26;

        if ( 1==pH265_PPS_DataInst->bPPSSliceChromaQpFlag ){
            pH265_Slice_Hdr_DataInst->i4SliceQpDeltaCb = i4VDEC_HAL_H265_SeCodeNum(_u4BSID[u4InstID], u4InstID);//se(v)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"slice_qp_delta_cb %d\n", pH265_Slice_Hdr_DataInst->i4SliceQpDeltaCb );
            #endif
            pH265_Slice_Hdr_DataInst->i4SliceQpDeltaCr = i4VDEC_HAL_H265_SeCodeNum(_u4BSID[u4InstID], u4InstID);//se(v)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"slice_qp_delta_cr %d\n", pH265_Slice_Hdr_DataInst->i4SliceQpDeltaCr );
            #endif
        }
        if (1==pH265_PPS_DataInst->bDeblockingFilterControlPresentFlag){
            if (1==pH265_PPS_DataInst->bDeblockingFilterOverrideEnabledFlag){
                pH265_Slice_Hdr_DataInst->bDeblockingFilterOverrideFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
                #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                    DBG_H265_PRINTF(pfLogFile,"deblocking_filter_override_flag %d\n", pH265_Slice_Hdr_DataInst->bDeblockingFilterOverrideFlag );
                #endif
            } else {
                pH265_Slice_Hdr_DataInst->bDeblockingFilterOverrideFlag = 0;
            }
            if (1==pH265_Slice_Hdr_DataInst->bDeblockingFilterOverrideFlag){
                pH265_Slice_Hdr_DataInst->bDeblockingFilterDisableFlag = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
                #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                    DBG_H265_PRINTF(pfLogFile,"slice_disable_deblocking_filter_flag %d\n", pH265_Slice_Hdr_DataInst->bDeblockingFilterDisableFlag );
                #endif
                if (0==pH265_Slice_Hdr_DataInst->bDeblockingFilterDisableFlag){
                    pH265_Slice_Hdr_DataInst->i4DeblockingFilterBetaOffsetDiv2 = i4VDEC_HAL_H265_SeCodeNum(_u4BSID[u4InstID], u4InstID);//se(v)
                    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                        DBG_H265_PRINTF(pfLogFile,"beta_offset_div2 %d\n", pH265_Slice_Hdr_DataInst->i4DeblockingFilterBetaOffsetDiv2 );
                    #endif
                    pH265_Slice_Hdr_DataInst->i4DeblockingFilterTcOffsetDiv2 = i4VDEC_HAL_H265_SeCodeNum(_u4BSID[u4InstID], u4InstID);//se(v)
                    #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                        DBG_H265_PRINTF(pfLogFile,"tc_offset_div2 %d\n", pH265_Slice_Hdr_DataInst->i4DeblockingFilterTcOffsetDiv2 );
                    #endif
                }
            }else{
                pH265_Slice_Hdr_DataInst->bDeblockingFilterDisableFlag = pH265_PPS_DataInst->bPicDisableDeblockingFilterFlag;
                pH265_Slice_Hdr_DataInst->i4DeblockingFilterBetaOffsetDiv2 = pH265_PPS_DataInst->i4DeblockingFilterBetaOffsetDiv2;
                pH265_Slice_Hdr_DataInst->i4DeblockingFilterTcOffsetDiv2 = pH265_PPS_DataInst->i4DeblockingFilterTcOffsetDiv2;
            }
        }else{
            pH265_Slice_Hdr_DataInst->bDeblockingFilterDisableFlag = 0;
            pH265_Slice_Hdr_DataInst->i4DeblockingFilterBetaOffsetDiv2 = 0;
            pH265_Slice_Hdr_DataInst->i4DeblockingFilterTcOffsetDiv2 = 0;
        }

        bSAOEnabled = ( 0==pH265_SPS_DataInst->bUseSAO )?(0):(pH265_Slice_Hdr_DataInst->bSaoEnabledFlag ||pH265_Slice_Hdr_DataInst->bSaoEnabledFlagChroma);
        bDBFEnabled = ( 0==pH265_Slice_Hdr_DataInst->bDeblockingFilterDisableFlag );
        if ( 1==pH265_PPS_DataInst->bLoopFilterAcrossSlicesEnabledFlag && (bSAOEnabled ||bDBFEnabled )){
            i4tmp= u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID, 1);//u(1)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"slice_loop_filter_across_slices_enabled_flag %d\n", i4tmp );
            #endif
        }else{
            i4tmp = pH265_PPS_DataInst->bLoopFilterAcrossSlicesEnabledFlag;
        }
        pH265_Slice_Hdr_DataInst->bLoopFilterAcrossSlicesEnabledFlag = i4tmp;
    }

    if ( 1==pH265_PPS_DataInst->bTilesEnabledFlag || 1==pH265_PPS_DataInst->bEntropyCodingSyncEnabledFlag ){
        pH265_Slice_Hdr_DataInst->u4NumEntryPointOffsets = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"num_entry_point_offsets %d\n", pH265_Slice_Hdr_DataInst->u4NumEntryPointOffsets );
        #endif
        if ( 0<pH265_Slice_Hdr_DataInst->u4NumEntryPointOffsets ){
            pH265_Slice_Hdr_DataInst->u4OffsetLenMinus1 = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"offset_len_minus1 %d\n", pH265_Slice_Hdr_DataInst->u4OffsetLenMinus1 );
            #endif
        }
        for ( i4Indexer=0; i4Indexer<pH265_Slice_Hdr_DataInst->u4NumEntryPointOffsets ; i4Indexer++ ){
            i4tmp = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID,pH265_Slice_Hdr_DataInst->u4OffsetLenMinus1+1);//u(v)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"entry_point_offset_minus1  %d\n", i4tmp);
            #endif
        }
    }

     ////////////////////////////////set Pic INFO ////////////////////////////////
     if ( PARSE_OK != vHEVCVerifyVDecSetPicInfo(u4InstID, ptVerMpvDecPrm) ){
     #if DEBUG_LEVEL >= DBG_LEVEL_BUG1
         DBG_H265_PRINTF(pfLogFile,"[ERROR] SetPicInfo syntex Error!! \n" );
     #endif
         return SLICE_SYNTAX_ERROR; 
     }
     /////////////////////////////////////////////////////////////////////////
    
    if ( 1==pH265_PPS_DataInst->bSliceHeaderExtensionPresentFlag ){
        pH265_Slice_Hdr_DataInst->u4SliceHeaderExtensionLength = u4VDEC_HAL_H265_UeCodeNum(_u4BSID[u4InstID], u4InstID);//ue(v)
        #if DEBUG_LEVEL >= DBG_LEVEL_INFO
            DBG_H265_PRINTF(pfLogFile,"slice_header_extension_length %d\n", pH265_Slice_Hdr_DataInst->u4SliceHeaderExtensionLength );
        #endif
        for ( i4Indexer=0; i4Indexer<pH265_Slice_Hdr_DataInst->u4SliceHeaderExtensionLength ; i4Indexer++ ){
            i4tmp = u4VDEC_HAL_H265_GetRealBitStream(_u4BSID[u4InstID], u4InstID,8);//u(8)
            #if DEBUG_LEVEL >= DBG_LEVEL_INFO
                DBG_H265_PRINTF(pfLogFile,"slice_header_extension_data_byte %d\n", i4tmp );
            #endif
        }
    }
    _rH265PicInfo[u4InstID].u4SliceCnt++;

    vVDEC_HAL_H265_TrailingBits(_u4BSID[u4InstID], u4InstID);
#if DEBUG_LEVEL >= DBG_LEVEL_INFO
    printk ("[INFO] vHEVCParseSliceHeader() done!!\n");
#endif

    return PARSE_OK;
}

// *********************************************************************
// Function    : void vHEVCVerifyRef_Pic_List_Modification(UINT32 u4InstID, H265_Slice_Hdr_Data *prSliceHdr)
// Description : ref pic List0 & List1 reordering
// Parameter   : None
// Return      : None
// *********************************************************************
void vHEVCVerifyRef_Pic_List_Modification(UINT32 u4InstID, H265_Slice_Hdr_Data *prSliceHdr)
{
   vVDEC_HAL_H265_RPL_Modification(u4InstID);
}


// *********************************************************************
// Function    : void vHEVCVerifySEI_Rbsp(UINT32 u4InstID)
// Description : SEI parameter set header
// Parameter   : None
// Return      : None
// *********************************************************************
void vHEVCVerifySEI_Rbsp(UINT32 u4InstID)
{

}


// *********************************************************************
// Function    : void vHEVCInterpretFilmGrainCharacteristicsInfo(UINT32 u4InstID, VDEC_INFO_DEC_PRM_T *ptVerMpvDecPrm)
// Description : SEI Film Grain parameter set header
// Parameter   : VDEC_INFO_DEC_PRM_T *ptVerMpvDecPrm
// Return      : None
// *********************************************************************
void vHEVCInterpretFilmGrainCharacteristicsInfo(UINT32 u4InstID, VDEC_INFO_DEC_PRM_T *ptVerMpvDecPrm)
{

}


