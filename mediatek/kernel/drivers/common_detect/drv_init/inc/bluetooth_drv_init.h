#ifndef _BLUETOOTH_DRIVER_INIT_H_
#define _BLUETOOTH_DRIVER_INIT_H_

extern int do_bluetooth_drv_init(int chip_id);
extern int mtk_wcn_stpbt_drv_init(void);
#ifdef MTK_WCN_REMOVE_KERNEL_MODULE
extern int mtk_hci_init(void);
#endif
#endif

