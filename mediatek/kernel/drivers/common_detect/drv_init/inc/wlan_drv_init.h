#ifndef _WLAN_DRV_INIT_H_
#define _WLAN_DRV_INIT_H_
extern int do_wlan_drv_init(int chip_id);


#ifdef MTK_WCN_COMBO_CHIP_SUPPORT
extern int mtk_wcn_wmt_wifi_init(void);
#ifdef MT6620
extern int mtk_wcn_wlan_6620_init(void);
#endif
#ifdef MT6628
extern int mtk_wcn_wlan_6628_init(void);
#endif
#ifdef MT6630
extern int mtk_wcn_wlan_6630_init(void);
#endif
#endif

#ifdef MTK_WCN_SOC_CHIP_SUPPORT
extern int mtk_wcn_wmt_wifi_soc_init(void);
extern int mtk_wcn_wlan_soc_init(void);
#endif
#endif
