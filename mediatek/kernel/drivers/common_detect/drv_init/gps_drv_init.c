#ifdef DFT_TAG
#undef DFT_TAG
#endif
#define DFT_TAG         "[GPS-MOD-INIT]"

#include "wmt_detect.h"
#include "gps_drv_init.h"


int do_gps_drv_init(int chip_id)
{
	int i_ret = -1;
	WMT_DETECT_INFO_FUNC("start to do gps driver init \n");
	i_ret = mtk_wcn_stpgps_drv_init();
	WMT_DETECT_INFO_FUNC("finish gps driver init, i_ret:%d\n", i_ret);
	return i_ret;

}


