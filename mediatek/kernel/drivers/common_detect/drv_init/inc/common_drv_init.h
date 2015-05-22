#ifndef _COMMON_DRV_INIT_H_
#define _COMMON_DRV_INIT_H_
extern int do_common_drv_init(int chip_id);


/*defined in common part driver*/
#ifdef MTK_WCN_COMBO_CHIP_SUPPORT
extern int mtk_wcn_combo_common_drv_init(void);
extern int mtk_wcn_hif_sdio_drv_init(void);
extern int mtk_wcn_stp_uart_drv_init (void);
extern int mtk_wcn_stp_sdio_drv_init(void);
#endif

#ifdef MTK_WCN_SOC_CHIP_SUPPORT
extern int mtk_wcn_soc_common_drv_init(void);
#endif


#endif

