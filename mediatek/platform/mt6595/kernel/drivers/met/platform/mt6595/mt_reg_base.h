/*
 * This file is generated automatically according to the design of silicon.
 * Don't modify it directly.
 */

#ifndef __MT_REG_BASE
#define __MT_REG_BASE

#if !defined(CONFIG_MT6582_FPGA)

// APB Module cksys
#define INFRA_BASE (0xF0000000)

// APB Module infracfg_ao
#define INFRACFG_AO_BASE (0xF0001000)

// APB Module fhctl
#define FHCTL_BASE (0xF0002000)

// APB Module pericfg
#define PERICFG_BASE (0xF0003000)

// APB Module dramc
#define DRAMC0_BASE (0xF0004000)

// APB Module gpio
#define GPIO_BASE (0xF0005000)

// APB Module sleep
#define SPM_BASE (0xF0006000)

// APB Module toprgu
#define TOPRGU_BASE (0xF0007000)
#define AP_RGU_BASE TOPRGU_BASE

// APB Module apxgpt
#define APMCU_GPTIMER_BASE (0xF0008000)

// APB Module rsvd
#define RSVD_BASE (0xF0009000)

// APB Module sej
#define SEJ_BASE (0xF000A000)

// APB Module ap_cirq_eint
#define AP_CIRQ_EINT (0xF000B000)

// APB Module ap_cirq_eint
#define EINT_BASE (0xF000B000)

// APB Module smi
#define SMI1_BASE (0xF000C000)

// APB Module pmic_wrap
#define PWRAP_BASE (0xF000D000)

// APB Module device_apc_ao
#define DEVAPC_AO_BASE (0xF000E000)

// APB Module ddrphy
#define DDRPHY_BASE (0xF000F000)

// APB Module vencpll
#define VENCPLL_BASE (0xF000F000)

// APB Module mipi_tx_config
#define MIPI_CONFIG_BASE (0xF0010000)

// APB Module mipi_rx_ana
#define MIPI_RX_ANA_BASE (0xF0010800)

// APB Module kp
#define KP_BASE (0xF0011000)

// APB Module dbgapb
#define DEBUGTOP_BASE (0xF0100000)

// APB Module mcucfg
#define MCUSYS_CFGREG_BASE (0xF0200000)

// APB Module infracfg
#define INFRACFG_BASE (0xF0201000)

// APB Module sramrom
#define SRAMROM_BASE (0xF0202000)

// APB Module emi
#define EMI_BASE (0xF0203000)

// APB Module sys_cirq
#define SYS_CIRQ_BASE (0xF0204000)

// APB Module m4u
#define SMI_MMU_TOP_BASE (0xF0205000)

// APB Module nb_mmu
#define NB_MMU0_BASE (0xF0205200)

// APB Module nb_mmu
#define NB_MMU1_BASE (0xF0205800)

// APB Module efusec
#define EFUSEC_BASE (0xF0206000)

// APB Module device_apc
#define DEVAPC_BASE (0xF0207000)

// APB Module mcu_biu_cfg
#define MCU_BIU_BASE (0xF0208000)

// APB Module apmixed
#define APMIXEDSYS_BASE (0xF0209000)

// APB Module ccif
#define AP_CCIF_BASE (0xF020A000)

// APB Module ccif
#define MD_CCIF_BASE (0xF020B000)

// APB Module gpio1
#define GPIO1_BASE (0xF020C000)

// APB Module infra_mbist
#define INFRA_TOP_MBIST_CTRL_BASE (0xF020D000)

// APB Module dramc_conf_nao
#define DRAMC_NAO_BASE (0xF020E000)

// APB Module trng
#define TRNG_BASE (0xF020F000)

// APB Module ca9
#define CORTEXA7MP_BASE (0xF0210000)

// APB Module ap_dma
#define AP_DMA_BASE (0xF1000000)

// APB Module auxadc
#define AUXADC_BASE (0xF1001000)

// APB Module uart
#define UART1_BASE (0xF1002000)

// APB Module uart
#define UART2_BASE (0xF1003000)

// APB Module uart
#define UART3_BASE (0xF1004000)

// APB Module uart
#define UART4_BASE (0xF1005000)

// APB Module pwm
#define PWM_BASE (0xF1006000)

// APB Module i2c
#define I2C0_BASE (0xF1007000)

// APB Module i2c
#define I2C1_BASE (0xF1008000)

// APB Module i2c
#define I2C2_BASE (0xF1009000)

// APB Module spi
#define SPI0_BASE (0xF100A000)
#define SPI1_BASE (0xF100A000)

// APB Module therm_ctrl
#define THERMAL_BASE (0xF100B000)

// APB Module btif
#define BTIF_BASE (0xF100C000)

// APB Module nfi
#define NFI_BASE (0xF100D000)

// APB Module nfiecc_16bit
#define NFIECC_BASE (0xF100E000)

// APB Module nli_arb
#define NLI_ARB_BASE (0xF100F000)

// APB Module peri_pwrap_bridge
#define PERI_PWRAP_BRIDGE_BASE (0xF1017000)

// APB Module usb2
#define USB_BASE (0xF1200000)

// APB Module usb_sif
#define USB_SIF_BASE (0xF1210000)

// APB Module msdc
#define MSDC_0_BASE (0xF1230000)

// APB Module msdc
#define MSDC_1_BASE (0xF1240000)

// APB Module msdc
#define MSDC_2_BASE (0xF1250000)

// APB Module wcn_ahb
#define WCN_AHB_BASE (0xF1260000)

// APB Module mfg_top
#define G3D_CONFIG_BASE (0xF3000000)

// APB Module mali
#define MALI_BASE (0xF3010000)

// APB Module mali_tb_cmd
#define MALI_TB_BASE (0xF301f000)

// APB Module mmsys_config
#define DISPSYS_BASE (0xF4000000)

// APB Module mdp_rdma
#define MDP_RDMA_BASE (0xF4001000)

// APB Module mdp_rsz
#define MDP_RSZ0_BASE (0xF4002000)

// APB Module mdp_rsz
#define MDP_RSZ1_BASE (0xF4003000)

// APB Module disp_wdma
#define MDP_WDMA_BASE (0xF4004000)

// APB Module disp_wdma
#define WDMA1_BASE (0xF4004000)

// APB Module mdp_wrot
#define MDP_WROT_BASE (0xF4005000)

// APB Module mdp_tdshp
#define MDP_TDSHP_BASE (0xF4006000)

// APB Module ovl
#define DISP_OVL_BASE (0xF4007000)

// APB Module ovl
#define OVL0_BASE (0xF4007000)

// APB Module ovl
#define OVL1_BASE (0xF4007000)

// APB Module disp_rdma
#define DISP_RDMA_BASE (0xF4008000)

// APB Module disp_rdma
#define R_DMA1_BASE (0xF4008000)

// APB Module disp_rdma
#define R_DMA0_BASE (0xF4008000)

// APB Module disp_wdma
#define DISP_WDMA_BASE (0xF4009000)

// APB Module disp_wdma
#define WDMA0_BASE (0xF4009000)

// APB Module disp_bls
#define DISP_BLS_BASE (0xF400A000)

// APB Module disp_color_config
#define DISP_COLOR_BASE (0xF400B000)

// APB Module dsi
#define DSI_BASE (0xF400C000)

// APB Module disp_dpi
#define DPI_BASE (0xF400D000)

// APB Module disp_mutex
#define MMSYS_MUTEX_BASE (0xF400E000)

// APB Module mm_cmdq
#define MMSYS_CMDQ_BASE (0xF400F000)

// APB Module smi_larb
#define SMI_LARB0_BASE (0xF4010000)

// APB Module smi
#define SMI_BASE (0xF4011000)

// APB Module smi_larb
#define SMILARB2_BASE (0xF5001000)

// APB Module smi_larb
#define SMI_LARB3_BASE (0xF5001000)

// APB Module mmu
#define SMI_LARB3_MMU_BASE (0xF5001800)

// APB Module smi_larb
#define SMI_LARB4_BASE (0xF5002000)

// APB Module fake_eng
#define FAKE_ENG_BASE (0xF5002000)

// APB Module mmu
#define SMI_LARB4_MMU_BASE (0xF5002800)

// APB Module smi
#define VENC_BASE (0xF5009000)

// APB Module jpgenc
#define JPGENC_BASE (0xF500A000)

// APB Module vdecsys_config
#define VDEC_GCON_BASE (0xF6000000)

// APB Module smi_larb
#define SMI_LARB1_BASE (0xF6010000)

// APB Module mmu
#define SMI_LARB1_MMU_BASE (0xF6010800)

// APB Module vdtop
#define VDEC_BASE (0xF6020000)

// APB Module vdtop
#define VDTOP_BASE (0xF6020000)

// APB Module vld
#define VLD_BASE (0xF6021000)

// APB Module vld_top
#define VLD_TOP_BASE (0xF6021800)

// APB Module mc
#define MC_BASE (0xF6022000)

// APB Module avc_vld
#define AVC_VLD_BASE (0xF6023000)

// APB Module avc_mv
#define AVC_MV_BASE (0xF6024000)

// APB Module vdec_pp
#define VDEC_PP_BASE (0xF6025000)

// APB Module vp8_vld
#define VP8_VLD_BASE (0xF6026800)

// APB Module vp6
#define VP6_BASE (0xF6027000)

// APB Module vld2
#define VLD2_BASE (0xF6027800)

// APB Module mc_vmmu
#define MC_VMMU_BASE (0xF6028000)

// APB Module pp_vmmu
#define PP_VMMU_BASE (0xF6029000)

// APB Module imgsys
#define IMGSYS_CONFG_BASE (0xF5000000)

// APB Module cam
#define CAMINF_BASE (0xF5000000)

// APB Module csi2
#define CSI2_BASE (0xF5000000)

// APB Module seninf
#define SENINF_BASE (0xF5000000)

// APB Module seninf_tg
#define SENINF_TG_BASE (0xF5000000)

// APB Module seninf_top
#define SENINF_TOP_BASE (0xF5000000)

// APB Module mipi_rx_config
#define MIPI_RX_CONFIG_BASE (0xF500C000)

// APB Module scam
#define SCAM_BASE (0xF5008000)

// APB Module ncsi2
#define NCSI2_BASE (0xF5008000)

// APB Module ccir656
#define CCIR656_BASE (0xF5000000)

// APB Module n3d_ctl
#define N3D_CTL_BASE (0xF5000000)

// APB Module fdvt
#define FDVT_BASE (0xF500B000)

// APB Module audiosys
#define AUDIO_BASE (0xF1221000)
#define AUDIO_REG_BASE (0xF1220000)

// CONNSYS
#define CONN_BTSYS_PKV_BASE (0xF8000000)
#define CONN_BTSYS_TIMCON_BASE (0xF8010000)
#define CONN_BTSYS_RF_CONTROL_BASE (0xF8020000)
#define CONN_BTSYS_MODEM_BASE (0xF8030000)
#define CONN_BTSYS_BT_CONFIG_BASE (0xF8040000)
#define CONN_MCU_CONFIG_BASE (0xF8070000)
#define CONN_TOP_CR_BASE (0xF80B0000)
#define CONN_HIF_CR_BASE (0xF80F0000)

/*
 * Addresses below are added manually.
 * They cannot be mapped via IO_VIRT_TO_PHYS().
 */

#define GIC_CPU_BASE (CORTEXA7MP_BASE + 0x2000)
#define GIC_DIST_BASE (CORTEXA7MP_BASE + 0x1000)
#define SYSRAM_BASE 0xF2000000  /* L2 cache shared RAM */
#define DEVINFO_BASE 0xF7000000
#define INTER_SRAM 0xF9000000

#else 

#define SMI_MMU_TOP_BASE            0xF0205000
#define SMILARB2_BASE               0xF5001000

/* on-chip SRAM */
#define INTER_SRAM                  0xF9000000

/* infrasys */
//#define TOPRGU_BASE                 0xF0000000
#define INFRA_BASE                  0xF0000000
#define INFRACFG_BASE               0xF0001000
#define INFRACFG_AO_BASE            0xF0001000
#define FHCTL_BASE                  0xF0002000
#define PERICFG_BASE                0xF0003000
#define DRAMC0_BASE                 0xF0004000
#define DDRPHY_BASE                 0xF000F000
#define DRAMC_NAO_BASE              0xF020E000
#define GPIO_BASE                   0xF0005000
#define GPIO1_BASE                  0xF020C000
#define TOPSM_BASE                  0xF0006000
#define SPM_BASE                    0xF0006000
#define TOPRGU_BASE                 0xF0007000
#define AP_RGU_BASE                 TOPRGU_BASE
#define APMCU_GPTIMER_BASE          0xF0008000
#define SEJ_BASE                    0xF000A000
#define AP_CIRQ_EINT                0xF000B000
#define SMI1_BASE                   0xF000C000
#define MIPI_CONFIG_BASE            0xF0010000
#define KP_BASE                     0xF0011000
#if 0
#define DEVICE_APC_0_BASE           0xF0010000
#define DEVICE_APC_1_BASE           0xF0011000
#define DEVICE_APC_2_BASE           0xF0012000
#define DEVICE_APC_3_BASE           0xF0013000
#define DEVICE_APC_4_BASE           0xF0014000
#define SMI0_BASE                   0xF0208000
#endif
#define EINT_BASE                   0xF000B000


#define DEBUGTOP_BASE               0xF0100000
#define MCUSYS_CFGREG_BASE          0xF0200000
#define SRAMROM_BASE                0xF0202000
#define EMI_BASE                    0xF0203000
#define EFUSEC_BASE                 0xF0206000
#define MCU_BIU_BASE                0xF0208000
#define APMIXED_BASE                0xF0209000
#define APMIXEDSYS_BASE             0xF0209000
#define AP_CCIF_BASE                0xF020A000
#define MD_CCIF_BASE                0xF020B000
#define INFRA_TOP_MBIST_CTRL_BASE   0xF020D000
#define DRAMC_NAO_BASE              0xF020E000
#define CORTEXA7MP_BASE             0xF0210000
#define GIC_CPU_BASE    (CORTEXA7MP_BASE + 0x2000)
#define GIC_DIST_BASE   (CORTEXA7MP_BASE + 0x1000)
//#define SMI_LARB_BASE             0xF0211000
//#define MCUSYS_AVS_BASE           0xF0212000

/* perisys */
/*avalaible*/
#define AP_DMA_BASE                 0xF1000000
#define AUXADC_BASE                 0xF1001000
#define UART1_BASE                  0xF1002000
#define UART2_BASE                  0xF1003000
#define UART3_BASE                  0xF1004000
#define UART4_BASE                  0xF1005000
#define PWM_BASE                    0xF1006000
#define I2C0_BASE                   0xF1007000
#define I2C1_BASE                   0xF1008000
#define I2C2_BASE                   0xF1009000
#define SPI0_BASE                   0xF100A000
#define BTIF_BASE                   (0xF100C000)
#define NFI_BASE                    0xF100D000
#define NFIECC_BASE                 0xF100E000
#define NLI_ARB_BASE                0xF100F000
#define I2C3_BASE                   0xF1010000 //FIXME 6582 take off
#define SPI1_BASE                   0xF100A000 
#define THERMAL_BASE                0xF100B000

// APB Module pmic_wrap
#define PWRAP_BASE (0xF000D000)

#if 0
//#define IRDA_BASE                 0xF1007000
#define I2C4_BASE                   0xF1014000
#define I2CDUAL_BASE                0xF1015000
#define ACCDET_BASE                 0xF1016000
#define AP_HIF_BASE                 0xF1017000
#define MD_HIF_BASE                 0xF1018000
#define GCPU_BASE                   0xF101B000
#define GCPU_NS_BASE                0xF01C000
#define GCPU_MMU_BASE               0xF01D000
#define SATA_BASE                   0xF01E000
#define CEC_BASE                    0xF01F000
//#define SPI1_BASE                 0xF1022000
#endif

#define USB1_BASE                   0xF1200000
#define USB2_BASE                   0xF1200000
#define USB_BASE                    0xF1200000
#define USB_SIF_BASE                0xF1210000
//#define USB3_BASE                 0xF1220000
#define MSDC_0_BASE                 0xF1230000
#define MSDC_1_BASE                 0xF1240000
#define MSDC_2_BASE                 0xF1250000
#define MSDC_3_BASE                 0xF1260000
#define MSDC_4_BASE                 0xF1270000
//#define ETHERNET_BASE             0xF1290000

//#define ETB_BASE                  0xF0111000
//#define ETM_BASE                  0xF017C000


/* SMI common subsystem */
#define SYSRAM_BASE                 0xF2000000
#define AUDIO_REG_BASE              0xF2030000
#define MFG_AXI_BASE                0xF2060000
#define AUDIO_BASE                  0xF1200000 //0xF2071000
#define MMSYS1_CONFIG_BASE          0xF2080000
#define SMI_LARB0_BASE              0xF2081000
#define SMI_LARB1_BASE              0xF2082000
#define SMI_LARB2_BASE              0xF2083000
#define VDEC_GCON_BASE              0xF6000000 //0xF4000000
#define VDEC_BASE                   0xF4020000
#define VENC_TOP_BASE               0xF7000000
#define VENC_BASE                   0xF7002000
#define R_DMA0_BASE                 0xF2086000
#define R_DMA1_BASE                 0xF2087000
#define VDO_ROT0_BASE               0xF2088000
#define RGB_ROT0_BASE               0xF2089000
#define VDO_ROT1_BASE               0xF208A000
#define RGB_ROT1_BASE               0xF208B000
//#define DPI_BASE                    0xF208C000
#define BRZ_BASE                    0xF208D000
#define JPG_DMA_BASE                0xF208E000
#define OVL_DMA_BASE                0xF208F000
#define CSI2_BASE                   0xF2092000
#define CRZ_BASE                    0xF2093000
#define VRZ0_BASE                   0xF2094000
#define IMGPROC_BASE                0xF2095000
#define EIS_BASE                    0xF2096000
#define SPI_BASE                    0xF2097000
#define SCAM_BASE                   0xF2098000
#define PRZ0_BASE                   0xF2099000
#define PRZ1_BASE                   0xF209A000
#define JPG_CODEC_BASE              0xF209B000
//#define DSI_BASE                    0xF209C000
#define TVC_BASE                    0xF209D000
#define TVE_BASE                    0xF209E000
#define TV_ROT_BASE                 0xF209F000
#define RGB_ROT2_BASE               0xF20A0000
//#define LCD_BASE                    0xF20A1000
#define FD_BASE                     0xF20A2000
#define MIPI_CONFG_BASE             0xF20A3000
#define VRZ1_BASE                   0xF20A4000
#define MMSYS2_CONFG_BASE           0xF20C0000
#define SMI_LARB3_BASE              0xF20C1000
#define MFG_APB_BASE                0xF20C4000
#define G2D_BASE                    0xF20C6000

#define DISPSYS_BASE				0xF4000000
#define ROT_BASE					0xF4001000
#define SCL_BASE					0xF4002000
#define OVL_BASE					0xF4007000
#define WDMA0_BASE					0xF4009000
#define WDMA1_BASE					0xF4005000
#define RDMA0_BASE					0xF4008000
//#define RDMA1_BASE					0xF4007000
#define BLS_BASE					0xF400A000
//#define GAMMA_BASE					0xF400000
#define COLOR_BASE					0xF400B000
#define TDSHP_BASE					0xF4006000
#define LCD_BASE					0xF4012000// only exist on FPGA
#define DSI_BASE					0xF400C000
#define DPI_BASE					0xF400D000
#define SMILARB1_BASE				0xF4010000
#define DISP_MUTEX_BASE				0xF400E000
#define DISP_CMDQ_BASE				0xF400F000

/* imgsys */
#define IMGSYS_CONFG_BASE           0xF5000000
#define CAMINF_BASE                 IMGSYS_CONFG_BASE

/* G3DSYS */
#define G3D_CONFIG_BASE             0xF3000000
#define MALI_BASE                   0xF3010000

#define DEVINFO_BASE                0xF8000000

#endif

#endif
