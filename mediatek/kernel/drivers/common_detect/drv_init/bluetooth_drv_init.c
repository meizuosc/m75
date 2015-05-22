#ifdef DFT_TAG
#undef DFT_TAG
#endif
#define DFT_TAG         "[BT-MOD-INIT]"

#include "wmt_detect.h"
#include "bluetooth_drv_init.h"


int do_bluetooth_drv_init(int chip_id)
{
	int i_ret = -1;
#ifdef MTK_WCN_REMOVE_KERNEL_MODULE
	WMT_DETECT_INFO_FUNC("start to do bluetooth driver init \n");
	i_ret = mtk_hci_init();
	WMT_DETECT_INFO_FUNC("finish bluetooth driver init, i_ret:%d\n", i_ret);
#else
	WMT_DETECT_INFO_FUNC("start to do bluetooth driver init \n");
	i_ret = mtk_wcn_stpbt_drv_init();
	WMT_DETECT_INFO_FUNC("finish bluetooth driver init, i_ret:%d\n", i_ret);
#endif
	return i_ret;
}


