
#ifndef _WMT_IDC_H_
#define _WMT_IDC_H_


#include "osal_typedef.h"
#include "osal.h"

#if WMT_PLAT_ALPS
#include "conn_md_exp.h"


#define LTE_IDC_BUFFER_MAX_SIZE 1024
/*comment from firmware owner,max pckage num is 5,but should not happened*/
#define WMT_IDC_RX_MAX_LEN 384 

typedef enum {
	WMT_IDC_TX_OPCODE_MIN = 0,
	WMT_IDC_TX_OPCODE_LTE_PARA = 0x0a,
	WMT_IDC_TX_OPCODE_LTE_FREQ = 0x0b,
	WMT_IDC_TX_OPCODE_WIFI_MAX_POWER = 0x0c,
	WMT_IDC_TX_OPCODE_DEBUG_MONITOR = 0x0e,
	WMT_IDC_TX_OPCODE_SPLIT_FILTER = 0x0f,
	WMT_IDC_TX_OPCODE_LTE_CONNECTION_STAS = 0x16,
	WMT_IDC_TX_OPCODE_LTE_HW_IF_INDICATION = 0x17,
	WMT_IDC_TX_OPCODE_LTE_INDICATION = 0x20,
	
	WMT_IDC_TX_OPCODE_MAX
}WMT_IDC_TX_OPCODE;

typedef enum {
	WMT_IDC_RX_OPCODE_BTWF_DEF_PARA = 0x0,
	WMT_IDC_RX_OPCODE_BTWF_CHAN_RAN = 0x1,
	//WMT_IDC_RX_OPCODE_TDM_REQ = 0x10,
	WMT_IDC_RX_OPCODE_DEBUG_MONITOR = 0x02,
	WMT_IDC_RX_OPCODE_LTE_FREQ_IDX_TABLE = 0x03,
	WMT_IDC_RX_OPCODE_BTWF_PROFILE_IND = 0x04,
	WMT_IDC_RX_OPCODE_MAX
}WMT_IDC_RX_OPCODE;

typedef struct _MTK_WCN_WMT_IDC_INFO_{
	ipc_ilm_t iit;
	CONN_MD_BRIDGE_OPS ops;
	UINT8 buffer[LTE_IDC_BUFFER_MAX_SIZE];
	
}MTK_WCN_WMT_IDC_INFO,*P_MTK_WCN_WMT_IDC_INFO;

extern INT32 wmt_idc_init(VOID);
extern INT32 wmt_idc_deinit(VOID);
extern INT32 wmt_idc_msg_to_lte_handing(VOID);
extern UINT32 wmt_idc_msg_to_lte_handing_for_test(UINT8 *p_buf,UINT32 len);

#else

INT32 wmt_idc_init(VOID)
{
    return 0;
}
INT32 wmt_idc_deinit(VOID)
{
    return 0;
}

INT32 wmt_idc_msg_to_lte_handing(VOID)
{
    return 0;
}

UINT32 wmt_idc_msg_to_lte_handing_for_test(UINT8 *p_buf,UINT32 len)
{
    return 0;
}

#endif


#endif
