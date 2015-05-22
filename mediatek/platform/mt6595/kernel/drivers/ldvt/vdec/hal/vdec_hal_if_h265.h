#ifndef _VDEC_HAL_IF_H265_H_
#define _VDEC_HAL_IF_H265_H_

#include "../include/vdec_info_h265.h"
#include "../include/vdec_info_common.h"

#if CONFIG_DRV_VERIFY_SUPPORT
#include "../verify/vdec_verify_general.h"
#endif

//#include "vdec_hw_common.h"
//#include "vdec_verify_typedef.h"
//#include "vdec_verify_keydef.h"
//#include "typedef.h"

/*! \name Video Decoder HAL H265 Interface
* @{
*/

/// Initialize video decoder hardware
/// \return If return value < 0, it's failed. Please reference hal_vdec_errcode.h.
INT32 i4VDEC_HAL_H265_InitVDecHW(
    UINT32 u4VDecID                                    ///< [IN] Video decoder hardware ID
);


/// Read Barrel Shifter after shifting
/// \return Value of barrel shifter input window after shifting
UINT32 u4VDEC_HAL_H265_ShiftGetBitStream(
    UINT32 u4BSID,                                      ///< [IN] Barrel shifter hardware ID of one video decoder
    UINT32 u4VDecID,                                    ///< [IN] Video decoder hardware ID
    UINT32 u4ShiftBits                                  ///< [IN] Shift bits number
);


/// Read Barrel Shifter before shifting
/// \return Value of barrel shifter input window before shifting
UINT32 u4VDEC_HAL_H265_GetBitStreamShift(
    UINT32 u4BSID,                                      ///< [IN] Barrel shifter hardware ID of one video decoder
    UINT32 u4VDecID,                                    ///< [IN] Video decoder hardware ID
    UINT32 u4ShiftBits                                 ///< [IN] Shift bits number
);


/// Read Barrel Shifter before shifting
/// \return  Most significant (32 - u4ShiftBits) bits of barrel shifter input window before shifting
UINT32 u4VDEC_HAL_H265_GetRealBitStream(
    UINT32 u4BSID,                                      ///< [IN] Barrel shifter hardware ID of one video decoder
    UINT32 u4VDecID,                                    ///< [IN] Video decoder hardware ID
    UINT32 u4ShiftBits                                 ///< [IN] Shift bits number
);


/// Read Barrel Shifter before shifting
/// \return  MSB of barrel shifter input window before shifting
BOOL bVDEC_HAL_H265_GetBitStreamFlg(
    UINT32 u4BSID,                                      ///< [IN] Barrel shifter hardware ID of one video decoder
    UINT32 u4VDecID                                     ///< [IN] Video decoder hardware ID 
);


/// Do UE variable length decoding
/// \return  Input window after UE variable length decoding
UINT32 u4VDEC_HAL_H265_UeCodeNum(
    UINT32 u4BSID,                                      ///< [IN] Barrel shifter hardware ID of one video decoder
    UINT32 u4VDecID                                     ///< [IN] Video decoder hardware ID 
);


/// Do SE variable length decoding
/// \return  Input window after SE variable length decoding
INT32 i4VDEC_HAL_H265_SeCodeNum(
    UINT32 u4BSID,                                      ///< [IN] Barrel shifter hardware ID of one video decoder
    UINT32 u4VDecID                                     ///< [IN] Video decoder hardware ID 
);

/// Get next start code
/// \return Current input window of vld while finding start code 
UINT32 u4VDEC_HAL_H265_GetStartCode_PicStart(
    UINT32 u4BSID,                                     ///< [IN] Barrel shifter hardware ID of one video decoder
    UINT32 u4VDecID                                    ///< [IN] Video decoder hardware ID 
); 

/// Get next start code
/// \return Current input window of vld while finding start code 
UINT32 u4VDEC_HAL_H265_GetStartCode_8530(
    UINT32 u4BSID,                                     ///< [IN] Barrel shifter hardware ID of one video decoder
    UINT32 u4VDecID                                    ///< [IN] Video decoder hardware ID 
); 


/// Initialize barrel shifter with byte alignment
/// \return If return value < 0, it's failed. Please reference hal_vdec_errcode.h.
INT32 i4VDEC_HAL_H265_InitBarrelShifter(
    UINT32 u4BSID,                                      ///< [IN] Barrel shifter hardware ID of one video decoder
    UINT32 u4VDecID,                                     ///< [IN] Video decoder hardware ID
    VDEC_INFO_H265_BS_INIT_PRM_T *prH265BSInitPrm
);


/// Read current read pointer
/// \return Current read pointer with byte alignment
UINT32 u4VDEC_HAL_H265_ReadRdPtr(
    UINT32 u4BSID,                                      ///< [IN] Barrel shifter hardware ID of one video decoder
    UINT32 u4VDecID,                                    ///< [IN] Video decoder hardware ID
    UINT32 u4VFIFOSa,
    UINT32 *pu4Bits                                     ///< [OUT] Read pointer with remained bits
);


/// Align read pointer to byte,word or double word
/// \return None
void vVDEC_HAL_H265_AlignRdPtr(
    UINT32 u4BSID,                                      ///< [IN] Barrel shifter hardware ID of one video decoder
    UINT32 u4VDecID,                                    ///< [IN] Video decoder hardware ID
    UINT32 u4AlignType                                  ///< [IN] Align type
);


/// Read barrel shifter bitcount after initializing 
/// \return Bitcount counted by HAL
UINT32 u4VDEC_HAL_H265_GetBitcount(
    UINT32 u4BSID,                                        ///< [IN] Barrel shifter ID
    UINT32 u4VDecID                                    ///< [IN] Video decoder hardware ID
);

/// Reference list reordering
/// \return None
void vVDEC_HAL_H265_RPL_Modification(
    UINT32 u4VDecID                                    ///< [IN] Video decoder hardware ID
);


/// Decode prediction weighting table
/// \return None
void vVDEC_HAL_H265_PredWeightTable(
    UINT32 u4VDecID                                    ///< [IN] Video decoder hardware ID
);


/// Remove traling bits to byte align
/// \return None
void vVDEC_HAL_H265_TrailingBits(
    UINT32 u4BSID,                                        ///< [IN] Barrel shifter ID
    UINT32 u4VDecID                                    ///< [IN] Video decoder hardware ID
);


/// Check whether there is more rbsp data
/// \return Is morw Rbsp data or not
BOOL bVDEC_HAL_H265_IsMoreRbspData(
    UINT32 u4BSID,                                        ///< [IN] Barrel shifter ID
    UINT32 u4VDecID                                    ///< [IN] Video decoder hardware ID
);


/// Set HW registers to initialize picture info
/// \return None
void vVDEC_HAL_H265_SetPicInfoReg(
    UINT32 u4VDecID                                  ///< [IN] Video decoder hardware ID
);


/// Set HW registers related with P reference list
/// \return None
void vVDEC_HAL_H265_SetRefPicListReg(
    UINT32 u4VDecID                                    ///< [IN] Video decoder hardware ID
);

/// Set SPS data to HW
/// \return None
void vVDEC_HAL_H265_SetSPSHEVLD(
    UINT32 u4VDecID,                                   ///< [IN] Video decoder hardware ID
    H265_SPS_Data *prSPS,          ///< [IN] Pointer to struct of sequence parameter set
    H265_PPS_Data *prPPS
);


/// Set PPS data to HW
/// \return None
void vVDEC_HAL_H265_SetPPSHEVLD(
        UINT32 u4VDecID,                                   ///< [IN] Video decoder hardware ID
        H265_SPS_Data *prSPS,          ///< [IN] Pointer to struct of sequence parameter set
        H265_PPS_Data *prPPS
);


/// Set part of slice header data to HW
/// \return None
void vVDEC_HAL_H265_SetSHDRHEVLD(
    UINT32 u4VDecID,                                              ///< [IN] Video decoder hardware ID
    H265_Slice_Hdr_Data *prSliceHdr,        ///< [IN] Pointer to struct of picutre parameter set
    BOOL bUseSAO,
    H265_PPS_Data *prPPS
);

void vVDEC_HAL_H265_SetSLVLD(
    UINT32 u4VDecID, 
    INT32 *coeff, 
    INT32 width, 
    INT32 height, 
    INT32 invQuantScales
);

void vVDEC_HAL_H265_SetSLPP(
    UINT32 u4VDecID, 
    pH265_SL_Data ScallingList
);

void vVDEC_HAL_H265_SetTilesInfo(
    UINT32 u4VDecID, 
    H265_SPS_Data *prSPS, 
    H265_PPS_Data *prPPS
);

void vVDecWriteHEVCPP(
    UINT32 u4VDecID, 
    UINT32 u4Addr, 
    UINT32 u4Va
);

void vVDecWriteHEVCVLD(
    UINT32 u4VDecID, 
    UINT32 u4Addr, 
    UINT32 u4Va
);


void vVDecWriteHEVCMISC(
    UINT32 u4VDecID, 
    UINT32 u4Addr, 
    UINT32 u4Val
);

UINT32 u4VDecReadHEVCMISC(
    UINT32 u4VDecID, 
    UINT32 u4Addr
);

/// Set video decoder hardware registers to decode
/// \return If return value < 0, it's failed. Please reference hal_vdec_errcode.h.
INT32 i4VDEC_HAL_H265_DecStart(
    UINT32 u4VDecID,
    VDEC_INFO_DEC_PRM_T *prDecPrm              ///< [IN] Pointer to H265 decode Information
);


/// Read current decoded mbx and mby
/// \return None
void vVDEC_HAL_H265_GetMbxMby(
    UINT32 u4VDecID,                                    ///< [IN] Video decoder hardware ID
    UINT32 *pu4Mbx,                                      ///< [OUT] Pointer to current decoded macroblock in x axis
    UINT32 *pu4Mby                                       ///< [OUT] Pointer to current decoded macroblock in y axis
);


/// Check if all video decoder modules are finish
/// \return TRUE: Finish, FALSE: Not yet
BOOL fgVDEC_HAL_H265_DecPicComplete(
    UINT32 u4VDecID                                    ///< [IN] Video decoder hardware ID
);



/// Read h265 error message after decoding end
/// \return h265 decode error message
UINT32 u4VDEC_HAL_H265_GetErrMsg(
    UINT32 u4VDecID                                    ///< [IN] Video decoder hardware ID
);


/// Check h265 error info
/// \return h265 decode error check result
BOOL fgVDEC_HAL_H265_ChkErrInfo(
    UINT32 u4BSID,                                        ///< [IN] Barrel shifter ID
    UINT32 u4VDecID,                                    ///< [IN] Video decoder hardware ID
    UINT32 u4DecErrInfo,                               ///< [IN] Err Info
    UINT32 u4ECLevel                               ///< [IN] Check the EC level   
);

void vVDEC_HAL_H265_VDec_PowerDown(UCHAR u4VDecID);

void vVDEC_HAL_H265_VDec_GetYCbCrCRC(UINT32 u4VDecID, UINT32 *pu4CRC);

#ifdef MPV_DUMP_H265_CHKSUM
/// Compare decode checksum with golden, only for verification
/// \return True for match, false for mismatch
void vVDEC_HAL_H265_VDec_ReadCheckSum1(
    UINT32 u4VDecID                                    ///< [IN] Video decoder hardware ID
);
/// Compare decode checksum with golden, only for verification
/// \return True for match, false for mismatch
void vVDEC_HAL_H265_VDec_ReadCheckSum2(
    UINT32 u4VDecID                                    ///< [IN] Video decoder hardware ID
);
#endif

//
/*! @} */

#if (CONFIG_DRV_VERIFY_SUPPORT)

/// Read H265 video decoder finish register, only for verification
/// \return Register value
UINT32 u4VDEC_HAL_H265_VDec_ReadFinishFlag(
    UINT32 u4VDecID
);

/// H265 video decoder clear interrupt setting, only for verification
UINT32 u4VDEC_HAL_H265_VDec_ClearInt(
    UINT32 u4VDecID
);

UINT32 vVDEC_HAL_H265_VDec_VPmode(
     UINT32 u4VDecID
);

/// Dump H265 video decoder registers, only for verification
/// return None
void vVDEC_HAL_H265_VDec_DumpReg(
     UINT32 u4VDecID,
     BOOL bDecodeDone
);

/// Read H265 video decoder checksum registers, only for verification
/// \return None
void vVDEC_HAL_H265_VDec_ReadCheckSum(
    UINT32 u4VDecID,
    UINT32 *pu4CheckSum
);


/// Compare decode checksum with golden, only for verification
/// \return True for match, false for mismatch
BOOL fgVDEC_HAL_H265_VDec_CompCheckSum(
    UINT32 *pu4DecCheckSum,
    UINT32 *pu4GoldenCheckSum
);

#endif

//
/*! @} */


#endif //#ifndef _HAL_VDEC_H265_IF_H_

