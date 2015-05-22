#ifndef __MT_SMI_H__
#define __MT_SMI_H__

#define MET_SMI_LARB0_BASE 	0xF4021000
#define MET_SMI_LARB1_BASE 	0xF6010000
#define MET_SMI_LARB2_BASE 	0xF5001000
#define MET_SMI_LARB3_BASE 	0xF8001000
#define MET_SMI_LARB4_BASE 	0xF7002000

#define MET_SMI_COMM_BASE 	0xF4022000

#define SMI_LARB_NUMBER 5
#define SMI_COMM_NUMBER 1

#define SMI_REQ_OK           (0)
#define SMI_ERR_WRONG_REQ    (-1)
#define SMI_ERR_OVERRUN      (-2)

#define SMI_LARB_MON_ENA(i)		(void*)(i + 0x400)
#define SMI_LARB_MON_CLR(i)		(void*)(i + 0x404)
#define SMI_LARB_MON_PORT(i)		(void*)(i + 0x408)
#define SMI_LARB_MON_CON(i)		(void*)(i + 0x40C)

#define SMI_LARB_MON_ACT_CNT(i)		(void*)(i + 0x410)
#define SMI_LARB_MON_REQ_CNT(i)		(void*)(i + 0x414)
#define SMI_LARB_MON_BEA_CNT(i)		(void*)(i + 0x418)
#define SMI_LARB_MON_BYT_CNT(i)		(void*)(i + 0x41C)
#define SMI_LARB_MON_CP_CNT(i)		(void*)(i + 0x420)
#define SMI_LARB_MON_DP_CNT(i)		(void*)(i + 0x424)
#define SMI_LARB_MON_OSTD_CNT(i)	(void*)(i + 0x428)
#define SMI_LARB_MON_CP_MAX(i) 		(void*)(i + 0x430)
#define SMI_LARB_MON_OSTD_MAX(i) 	(void*)(i + 0x434)

#define SMI_COMM_MON_ENA(i)		(void*)(i + 0x1A0)
#define SMI_COMM_MON_CLR(i)		(void*)(i + 0x1A4)
#define SMI_COMM_MON_TYPE(i)		(void*)(i + 0x1AC)
#define SMI_COMM_MON_CON(i)		(void*)(i + 0x1B0)
#define SMI_COMM_MON_ACT_CNT(i)		(void*)(i + 0x1C0)
#define SMI_COMM_MON_REQ_CNT(i)		(void*)(i + 0x1C4)
#define SMI_COMM_MON_OSTD_CNT(i) 	(void*)(i + 0x1C8)
#define SMI_COMM_MON_BEA_CNT(i) 	(void*)(i + 0x1CC)
#define SMI_COMM_MON_BYT_CNT(i) 	(void*)(i + 0x1D0)
#define SMI_COMM_MON_CP_CNT(i) 		(void*)(i + 0x1D4)
#define SMI_COMM_MON_DP_CNT(i) 		(void*)(i + 0x1D8)
#define SMI_COMM_MON_CP_MAX(i) 		(void*)(i + 0x1DC)
#define SMI_COMM_MON_OSTD_MAX(i) 	(void*)(i + 0x1E0)

void MET_SMI_Init(void);
void MET_SMI_Enable(unsigned long larbno);
void MET_SMI_Disable(unsigned long larbno);
void MET_SMI_Pause(int larbno);
void MET_SMI_Clear(int larbno);
void MET_SMI_PowerOn(void);
void MET_SMI_PowerOff(void);
int MET_SMI_LARB_SetCfg(unsigned int larbno,
			unsigned int pm,
			unsigned int reqtype,
			unsigned int rwtype,
			unsigned int dsttype);
int MET_SMI_LARB_SetPortNo(unsigned int larbno,
				unsigned int idx,
				unsigned int port);
int MET_SMI_COMM_SetCfg(unsigned int commonno,
			unsigned int pm,
			unsigned int reqtype);
int MET_SMI_COMM_SetPortNo(unsigned int commonno,
				unsigned int idx,
				unsigned int port);
int MET_SMI_COMM_SetRWType(unsigned int commonno,
				unsigned int idx,
				unsigned int rw);

// config
int MET_SMI_GetEna(int larbno);
int MET_SMI_GetClr(int larbno);
int MET_SMI_GetPortNo(int larbno);
int MET_SMI_GetCon(int larbno);
// cnt
int MET_SMI_GetActiveCnt(int larbno);
int MET_SMI_GetRequestCnt(int larbno);
int MET_SMI_GetBeatCnt(int larbno);
int MET_SMI_GetByteCnt(int larbno);
int MET_SMI_GetCPCnt(int larbno);
int MET_SMI_GetDPCnt(int larbno);
int MET_SMI_GetOSTDCnt(int larbno);
int MET_SMI_GetCP_MAX(int larbno);
int MET_SMI_GetOSTD_MAX(int larbno);

//common
void MET_SMI_Comm_Init(void);
void MET_SMI_Comm_Enable(unsigned long commonno);
void MET_SMI_Comm_Disable(unsigned long commonno);
void MET_SMI_Pause(int commonno);
void MET_SMI_Comm_Clear(int commonno);

//common config
int MET_SMI_Comm_GetEna(int commonno);
int MET_SMI_Comm_GetClr(int commonno);
int MET_SMI_Comm_GetType(int commonno);
int MET_SMI_Comm_GetCon(int commonno);
//cnt
int MET_SMI_Comm_GetPortNo(int commonno);
int MET_SMI_Comm_GetActiveCnt(int commonno);
int MET_SMI_Comm_GetRequestCnt(int commonno);
int MET_SMI_Comm_GetBeatCnt(int commonno);
int MET_SMI_Comm_GetByteCnt(int commonno);
int MET_SMI_Comm_GetCPCnt(int commonno);
int MET_SMI_Comm_GetDPCnt(int commonno);
int MET_SMI_Comm_GetOSTDCnt(int commonno);
int MET_SMI_Comm_GetCP_MAX(int commonno);
int MET_SMI_Comm_GetOSTD_MAX(int commonno);

#endif  /* !__MT_SMI_H__ */
