#ifndef _VDEC_HW_H265_H_
#define _VDEC_HW_H265_H_
//#include "typedef.h"
//#include "vdec_info_common.h"



// *********************************************************************
//  Video Decoder HW Functions
// *********************************************************************
extern void vVDecWriteHEVCVLD(UINT32 u4VDecID, UINT32 u4Addr, UINT32 u4Val);
extern UINT32 u4VDecReadHEVCVLD(UINT32 u4VDecID, UINT32 u4Addr);
extern void vVDecWriteHEVCMV(UINT32 u4VDecID, UINT32 u4Addr, UINT32 u4Val);
extern UINT32 u4VDecReadHEVCMV(UINT32 u4VDecID, UINT32 u4Addr);
extern void vVDecWriteHEVCFG(UINT32 u4VDecID, UINT32 u4Addr, UINT32 u4Val);
extern UINT32 u4VDecReadHEVCFG(UINT32 u4VDecID, UINT32 u4Addr);
extern UINT32 u4VDecHEVCVLDShiftBits(UINT32 u4BSID, UINT32 u4VDecID);
extern UINT32 u4VDecHEVCVLDGetBitS(UINT32 u4BSID,UINT32 u4VDecID,UINT32 dShiftBit);
extern UINT32 u4VDECHEVCInitSearchStartCode(UINT32 u4BSID, UINT32 u4VDecID, UINT32 u4ShiftBit); 
extern BOOL fgInitH265BarrelShift(UINT32 u4VDecID, VDEC_INFO_H265_BS_INIT_PRM_T *prH265BSInitPrm);
extern void vInitHEVCFgtHWSetting(UINT32 u4VDecID, VDEC_INFO_H265_INIT_PRM_T *prH265VDecInitPrm);
extern UINT32 u4VDecReadH265VldRPtr(UINT32 u4BSID, UINT32 u4VDecID, UINT32 *pu4Bits, UINT32 u4VFIFOSa);
extern UINT32 u4VDecReadHEVCPP(UINT32 u4VDecID, UINT32 u4Addr);

#endif

