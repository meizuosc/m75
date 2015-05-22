#ifdef DFT_TAG
#undef DFT_TAG
#endif
#define DFT_TAG         "[FM-MOD-INIT]"

#include "wmt_detect.h"
#include "fm_drv_init.h"


int do_fm_drv_init(int chip_id)
{
	WMT_DETECT_INFO_FUNC("start to do fm module init \n");
	
#ifdef MTK_FM_SUPPORT
	mtk_wcn_fm_init();
#endif
	
	WMT_DETECT_INFO_FUNC("finish fm module init\n");
	return 0;
}


