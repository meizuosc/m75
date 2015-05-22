#ifndef _MT_CLKMGR_H
#define _MT_CLKMGR_H

#include <linux/list.h>
#include "mach/mt_reg_base.h"
#include "mach/mt_typedefs.h"

#define CONFIG_CLKMGR_STAT

//#define APMIXED_BASE      (0xF0209000)
//#define CKSYS_BASE        (0xF0000000)
//#define INFRACFG_AO_BASE  (0xF0001000)
//#define PERICFG_BASE      (0xF0003000)
//#define AUDIO_BASE        (0xF1220000)
//#define MFGCFG_BASE       (0xF3FFF000)
//#define MMSYS_CONFIG_BASE (0xF4000000)
//#define IMGSYS_BASE       (0xF5000000)
//#define VDEC_GCON_BASE    (0xF6000000)
//#define MJC_CONFIG_BASE   (0xF7000000)
//#define VENC_GCON_BASE    (0xF8000000)
//#define MCUCFG_BASE       (0xF0200000)
//#define CA15L_CONFIG_BASE (0xF0200200)

/* APMIXEDSYS Register */
#define AP_PLL_CON0             (APMIXED_BASE + 0x00)
#define AP_PLL_CON1             (APMIXED_BASE + 0x04)
#define AP_PLL_CON2             (APMIXED_BASE + 0x08)
#define AP_PLL_CON7             (APMIXED_BASE + 0x1C)

#define ARMCA15PLL_CON0         (APMIXED_BASE + 0x200)
#define ARMCA15PLL_CON1         (APMIXED_BASE + 0x204)
#define ARMCA15PLL_CON2         (APMIXED_BASE + 0x208)
#define ARMCA15PLL_PWR_CON0     (APMIXED_BASE + 0x20C)

#define ARMCA7PLL_CON0          (APMIXED_BASE + 0x210)
#define ARMCA7PLL_CON1          (APMIXED_BASE + 0x214)
#define ARMCA7PLL_CON2          (APMIXED_BASE + 0x218)
#define ARMCA7PLL_PWR_CON0      (APMIXED_BASE + 0x21C)

#define MAINPLL_CON0            (APMIXED_BASE + 0x220)
#define MAINPLL_CON1            (APMIXED_BASE + 0x224)
#define MAINPLL_PWR_CON0        (APMIXED_BASE + 0x22C)

#define UNIVPLL_CON0            (APMIXED_BASE + 0x230)
#define UNIVPLL_CON1            (APMIXED_BASE + 0x234)
#define UNIVPLL_PWR_CON0        (APMIXED_BASE + 0x23C)

#define MMPLL_CON0              (APMIXED_BASE + 0x240)
#define MMPLL_CON1              (APMIXED_BASE + 0x244)
#define MMPLL_CON2              (APMIXED_BASE + 0x248)
#define MMPLL_PWR_CON0          (APMIXED_BASE + 0x24C)

#define MSDCPLL_CON0            (APMIXED_BASE + 0x250)
#define MSDCPLL_CON1            (APMIXED_BASE + 0x254)
#define MSDCPLL_PWR_CON0        (APMIXED_BASE + 0x25C)

#define VENCPLL_CON0            (APMIXED_BASE + 0x260)
#define VENCPLL_CON1            (APMIXED_BASE + 0x264)
#define VENCPLL_PWR_CON0        (APMIXED_BASE + 0x26C)

#define TVDPLL_CON0             (APMIXED_BASE + 0x270)
#define TVDPLL_CON1             (APMIXED_BASE + 0x274)
#define TVDPLL_PWR_CON0         (APMIXED_BASE + 0x27C)

#define MPLL_CON0               (APMIXED_BASE + 0x280)
#define MPLL_CON1               (APMIXED_BASE + 0x284)
#define MPLL_PWR_CON0           (APMIXED_BASE + 0x28C)

#define VCODECPLL_CON0          (APMIXED_BASE + 0x290)
#define VCODECPLL_CON1          (APMIXED_BASE + 0x294)
#define VCODECPLL_PWR_CON0      (APMIXED_BASE + 0x29C)

#define APLL1_CON0              (APMIXED_BASE + 0x2A0)
#define APLL1_CON1              (APMIXED_BASE + 0x2A4)
#define APLL1_CON2              (APMIXED_BASE + 0x2A8)
#define APLL1_CON3              (APMIXED_BASE + 0x2AC)
#define APLL1_PWR_CON0          (APMIXED_BASE + 0x2B0)

#define APLL2_CON0              (APMIXED_BASE + 0x2B4)
#define APLL2_CON1              (APMIXED_BASE + 0x2B8)
#define APLL2_CON2              (APMIXED_BASE + 0x2BC)
#define APLL2_CON3              (APMIXED_BASE + 0x2C0)
#define APLL2_PWR_CON0          (APMIXED_BASE + 0x2C4)

/* TOPCKGEN Register */
#define CLK_CFG_0               (CKSYS_BASE + 0x040)
#define CLK_CFG_1               (CKSYS_BASE + 0x050)
#define CLK_CFG_2               (CKSYS_BASE + 0x060)
#define CLK_CFG_3               (CKSYS_BASE + 0x070)
#define CLK_CFG_4               (CKSYS_BASE + 0x080)
#define CLK_CFG_5               (CKSYS_BASE + 0x090)
#define CLK_CFG_6               (CKSYS_BASE + 0x0A0) 
#define CLK_CFG_7               (CKSYS_BASE + 0x0B0)
#define CLK_CFG_8               (CKSYS_BASE + 0x100)
#define CLK_CFG_9               (CKSYS_BASE + 0x104)
#define CLK_CFG_10              (CKSYS_BASE + 0x108)
#define CLK_CFG_11              (CKSYS_BASE + 0x10C)
#define CLK_SCP_CFG_0           (CKSYS_BASE + 0x200)
#define CLK_SCP_CFG_1           (CKSYS_BASE + 0x204)
#define CLK_MISC_CFG_0          (CKSYS_BASE + 0x210)
#define CLK_MISC_CFG_1          (CKSYS_BASE + 0x214)
#define CLK_MISC_CFG_2          (CKSYS_BASE + 0x218)
#define CLK26CALI_0             (CKSYS_BASE + 0x220)
#define CLK26CALI_1             (CKSYS_BASE + 0x224)
#define CLK26CALI_2             (CKSYS_BASE + 0x228)
#define CKSTA_REG               (CKSYS_BASE + 0x22C)
#define TEST_MODE_CFG           (CKSYS_BASE + 0x230)
#define MBIST_CFG_0             (CKSYS_BASE + 0x308)
#define MBIST_CFG_1             (CKSYS_BASE + 0x30C)
#define RESET_DEGLITCH_KEY      (CKSYS_BASE + 0x310)
#define MBIST_CFG_3             (CKSYS_BASE + 0x314)

/* INFRASYS Register */
#define TOP_CKMUXSEL            (INFRACFG_AO_BASE + 0x00)
#define TOP_CKDIV1              (INFRACFG_AO_BASE + 0x08)

#define INFRA_PDN_SET           (INFRACFG_AO_BASE + 0x0040)
#define INFRA_PDN_CLR           (INFRACFG_AO_BASE + 0x0044)
#define INFRA_PDN_STA           (INFRACFG_AO_BASE + 0x0048)

#define TOPAXI_PROT_EN          (INFRACFG_AO_BASE + 0x0220)
#define TOPAXI_PROT_STA1        (INFRACFG_AO_BASE + 0x0228)


/* PERIFCG_Register */
#define PERI_PDN0_SET           (PERICFG_BASE + 0x0008)
#define PERI_PDN0_CLR           (PERICFG_BASE + 0x0010)
#define PERI_PDN0_STA           (PERICFG_BASE + 0x0018)

/* Audio Register*/
#define AUDIO_TOP_CON0          (AUDIO_BASE + 0x0000)
                                
/* MFGCFG Register*/            
#define MFG_CG_CON              (MFGCFG_BASE + 0)
#define MFG_CG_SET              (MFGCFG_BASE + 4)
#define MFG_CG_CLR              (MFGCFG_BASE + 8)

/* MMSYS Register*/             
#define DISP_CG_CON0            (MMSYS_CONFIG_BASE + 0x100)
#define DISP_CG_SET0            (MMSYS_CONFIG_BASE + 0x104)
#define DISP_CG_CLR0            (MMSYS_CONFIG_BASE + 0x108)
#define DISP_CG_CON1            (MMSYS_CONFIG_BASE + 0x110)
#define DISP_CG_SET1            (MMSYS_CONFIG_BASE + 0x114)
#define DISP_CG_CLR1            (MMSYS_CONFIG_BASE + 0x118)

#define MMSYS_DUMMY             (MMSYS_CONFIG_BASE + 0x890)
#define	SMI_LARB_BWL_EN_REG     (MMSYS_CONFIG_BASE + 0x21050)

/* IMGSYS Register */
#define IMG_CG_CON              (IMGSYS_BASE + 0x0000)
#define IMG_CG_SET              (IMGSYS_BASE + 0x0004)
#define IMG_CG_CLR              (IMGSYS_BASE + 0x0008)

/* VDEC Register */                                
#define VDEC_CKEN_SET           (VDEC_GCON_BASE + 0x0000)
#define VDEC_CKEN_CLR           (VDEC_GCON_BASE + 0x0004)
#define LARB_CKEN_SET           (VDEC_GCON_BASE + 0x0008)
#define LARB_CKEN_CLR           (VDEC_GCON_BASE + 0x000C)

/* MJC Register*/
#define MJC_CG_CON              (MJC_CONFIG_BASE + 0x0000)
#define MJC_CG_SET              (MJC_CONFIG_BASE + 0x0004)
#define MJC_CG_CLR              (MJC_CONFIG_BASE + 0x0008)

/* VENC Register*/
#define VENC_CG_CON             (VENC_GCON_BASE + 0x0)
#define VENC_CG_SET             (VENC_GCON_BASE + 0x4)
#define VENC_CG_CLR             (VENC_GCON_BASE + 0x8)

/* MCUSYS Register */           
#define IR_ROSC_CTL             (MCUCFG_BASE + 0x030)
//#define CA15L_MON_SEL           (CA15L_CONFIG_BASE + 0x01C)


enum {
    CG_PERI  = 0,
    CG_INFRA = 1,
//    CG_TOPCK = 2,
    CG_DISP0 = 2,
    CG_DISP1 = 3,
    CG_IMAGE = 4,
    CG_MFG   = 5,
    CG_AUDIO = 6,
    CG_VDEC0 = 7,
    CG_VDEC1 = 8,
    CG_MJC   = 9,
    CG_VENC  = 10,
    NR_GRPS  = 11,
};

enum cg_clk_id{                                 //The following is CODA name
    MT_CG_PERI_NFI                  = 0,        //NFI_PDN_SET
    MT_CG_PERI_THERM                = 1,        //THERM_PDN
    MT_CG_PERI_PWM1                 = 2,        //PWM1_PDN
    MT_CG_PERI_PWM2                 = 3,        //PWM2_PDN
    MT_CG_PERI_PWM3                 = 4,        //PWM3_PDN
    MT_CG_PERI_PWM4                 = 5,        //PWM4_PDN
    MT_CG_PERI_PWM5                 = 6,        //PWM5_PDN
    MT_CG_PERI_PWM6                 = 7,        //PWM6_PDN
    MT_CG_PERI_PWM7                 = 8,        //PWM7_PDN
    MT_CG_PERI_PWM                  = 9,        //PWM_PDN
    MT_CG_PERI_USB0                 = 10,       //USB0_PDN
    MT_CG_PERI_USB1                 = 11,       //USB1_PDN
    MT_CG_PERI_AP_DMA               = 12,       //AP_DMA_PDN
    MT_CG_PERI_MSDC30_0             = 13,       //MSDC30_0_PDN
    MT_CG_PERI_MSDC30_1             = 14,       //MSDC30_1_PDN
    MT_CG_PERI_MSDC30_2             = 15,       //MSDC30_2_PDN
    MT_CG_PERI_MSDC30_3             = 16,       //MSDC30_3_PDN
    MT_CG_PERI_NLI                  = 17,       //NLI_PDN
    MT_CG_PERI_IRDA                 = 18,       //IRDA_PDN
    MT_CG_PERI_UART0                = 19,       //UART0_PDN
    MT_CG_PERI_UART1                = 20,       //UART1_PDN
    MT_CG_PERI_UART2                = 21,       //UART2_PDN
    MT_CG_PERI_UART3                = 22,       //UART3_PDN
    MT_CG_PERI_I2C0                 = 23,       //I2C0_PDN
    MT_CG_PERI_I2C1                 = 24,       //I2C1_PDN
    MT_CG_PERI_I2C2                 = 25,       //I2C2_PDN
    MT_CG_PERI_I2C3                 = 26,       //I2C3_PDN
    MT_CG_PERI_I2C4                 = 27,       //I2C4_PDN
    MT_CG_PERI_AUXADC               = 28,       //AUXADC_PDN
    MT_CG_PERI_SPI0                 = 29,       //SPI0_PDN
    
    MT_CG_INFRA_DBGCLK              = 32,       //dbgclk_pdn
    MT_CG_INFRA_SMI                 = 33,       //smi_pdn
    MT_CG_INFRA_AUDIO               = 37,       //audio_pdn, shoule be removed
    MT_CG_INFRA_GCE                 = 38,       //gce_pdn
    MT_CG_INFRA_L2C_SRAM            = 39,       //l2c_sram_pdn
    MT_CG_INFRA_M4U                 = 40,       //m4u_pdn
    MT_CG_INFRA_MD1MCU              = 41,       //md1mcu_bus_pdn
    MT_CG_INFRA_MD1BUS              = 42,       //md1bus_bus_pdn
    MT_CG_INFRA_MD1DBB              = 43,       //md1dbb_bus_pdn
    MT_CG_INFRA_DEVICE_APC          = 44,       //device_apc_pd_pdn
    MT_CG_INFRA_TRNG				= 45,       //trng_pdn
    MT_CG_INFRA_MD1LTE              = 46,       //md1lte_bus_pdn
    MT_CG_INFRA_CPUM                = 47,       //cpum_pdn
    MT_CG_INFRA_KP                  = 48,       //kp_pdn
    MT_CG_INFRA_PMICSPI             = 54,       //pmicspi_pdn
    MT_CG_INFRA_PMICWRAP            = 55,       //pmic_wrap_pdn
    
//    MT_CG_TOPCK_PMICSPI             = 69,
    
    MT_CG_DISP0_SMI_COMMON          = 64,       //SMI_COMMON 
    MT_CG_DISP0_SMI_LARB0           = 65,       //SMI_LARB0  
    MT_CG_DISP0_CAM_MDP             = 66,       //CAM_MDP    
    MT_CG_DISP0_MDP_RDMA0           = 67,       //MDP_RDMA0  
    MT_CG_DISP0_MDP_RDMA1           = 68,       //MDP_RDMA1  
    MT_CG_DISP0_MDP_RSZ0            = 69,       //MDP_RSZ0   
    MT_CG_DISP0_MDP_RSZ1            = 70,       //MDP_RSZ1   
    MT_CG_DISP0_MDP_RSZ2            = 71,       //MDP_RSZ2   
    MT_CG_DISP0_MDP_TDSHP0          = 72,       //MDP_TDSHP0 
    MT_CG_DISP0_MDP_TDSHP1          = 73,       //MDP_TDSHP1 
    MT_CG_DISP0_MDP_CROP            = 74,       //MDP_CROP   
    MT_CG_DISP0_MDP_WDMA            = 75,       //MDP_WDMA   
    MT_CG_DISP0_MDP_WROT0           = 76,       //MDP_WROT0  
    MT_CG_DISP0_MDP_WROT1           = 77,       //MDP_WROT1  
    MT_CG_DISP0_FAKE_ENG            = 78,       //FAKE_ENG   
    MT_CG_DISP0_MUTEX_32K           = 79,       //MUTEX_32K  
    MT_CG_DISP0_DISP_OVL0           = 80,       //DISP_OVL0  
    MT_CG_DISP0_DISP_OVL1           = 81,       //DISP_OVL1  
    MT_CG_DISP0_DISP_RDMA0          = 82,       //DISP_RDMA0 
    MT_CG_DISP0_DISP_RDMA1          = 83,       //DISP_RDMA1 
    MT_CG_DISP0_DISP_RDMA2          = 84,       //DISP_RDMA2 
    MT_CG_DISP0_DISP_WDMA0          = 85,       //DISP_WDMA0 
    MT_CG_DISP0_DISP_WDMA1          = 86,       //DISP_WDMA1 
    MT_CG_DISP0_DISP_COLOR0         = 87,       //DISP_COLOR0
    MT_CG_DISP0_DISP_COLOR1         = 88,       //DISP_COLOR1
    MT_CG_DISP0_DISP_AAL            = 89,       //DISP_AAL   
    MT_CG_DISP0_DISP_GAMMA          = 90,       //DISP_GAMMA 
    MT_CG_DISP0_DISP_UFOE           = 91,       //DISP_UFOE  
    MT_CG_DISP0_DISP_SPLIT0         = 92,       //DISP_SPLIT0
    MT_CG_DISP0_DISP_SPLIT1         = 93,       //DISP_SPLIT1
    MT_CG_DISP0_DISP_MERGE          = 94,       //DISP_MERGE 
    MT_CG_DISP0_DISP_OD             = 95,       //DISP_OD    
    
    MT_CG_DISP1_DISP_PWM0_MM        = 96 ,      //DISP_PWM0_MM_clock 
    MT_CG_DISP1_DISP_PWM0_26M       = 97 ,      //DISP_PWM0_26M_clock
    MT_CG_DISP1_DISP_PWM1_MM        = 98 ,      //DISP_PWM1_MM_clock 
    MT_CG_DISP1_DISP_PWM1_26M       = 99 ,      //DISP_PWM1_26M_clock
    MT_CG_DISP1_DSI0_ENGINE         = 100,      //DSI0_engine        
    MT_CG_DISP1_DSI0_DIGITAL        = 101,      //DSI0_digital       
    MT_CG_DISP1_DSI1_ENGINE         = 102,      //DSI1_engine        
    MT_CG_DISP1_DSI1_DIGITAL        = 103,      //DSI1_digital       
    MT_CG_DISP1_DPI_PIXEL           = 104,      //DPI_pixel_clock    
    MT_CG_DISP1_DPI_ENGINE          = 105,      //DPI_engine_clock   
    
    MT_CG_IMAGE_LARB2_SMI           = 128,      //LARB2_SMI_CKPDN
    MT_CG_IMAGE_CAM_SMI             = 133,      //CAM_SMI_CKPDN
    MT_CG_IMAGE_CAM_CAM             = 134,      //CAM_CAM_CKPDN
    MT_CG_IMAGE_SEN_TG              = 135,      //SEN_TG_CKPDN
    MT_CG_IMAGE_SEN_CAM             = 136,      //SEN_CAM_CKPDN
    MT_CG_IMAGE_CAM_SV              = 137,      //CAM_SV_CKPDN
    MT_CG_IMAGE_FD                  = 139,      //FD_CKPDN
    
    MT_CG_MFG_AXI					= 160,      //BAXI_PDN
    MT_CG_MFG_MEM					= 161,      //BMEM_PDN
    MT_CG_MFG_G3D					= 162,      //BG3D_PDN
    MT_CG_MFG_26M					= 163,      //B26M_PDN
    
    MT_CG_AUDIO_AFE                 = 194,      //PDN_AFE
    MT_CG_AUDIO_I2S                 = 198,      //PDN_I2S
    MT_CG_AUDIO_22M                 = 200,      //PDN_22M
    MT_CG_AUDIO_24M                 = 201,      //PDN_24M
    MT_CG_AUDIO_APLL2_TUNER         = 210,      //PDN_APLL2_TUNER
    MT_CG_AUDIO_APLL_TUNER          = 211,      //PDN_APLL_TUNER
    MT_CG_AUDIO_HDMI                = 212,      //PDN_HDMI_CK
    MT_CG_AUDIO_ADDA3               = 214,      //PDN_ADDA3
    MT_CG_AUDIO_ADDA2               = 215,      //PDN_ADDA2
    
    MT_CG_VDEC0_VDEC				= 224,      //VDEC_CKEN
                                                
    MT_CG_VDEC1_LARB				= 256,      //LARB_CKEN
                                                
    MT_CG_MJC_SMI_LARB              = 288,      //SMI LARB
    MT_CG_MJC_TOP_GROUP0            = 289,      //MJC_TOP clock group 0
    MT_CG_MJC_TOP_GROUP1            = 290,      //MJC_TOP clock group 1
    MT_CG_MJC_TOP_GROUP2            = 291,      //MJC_TOP clock group 2
    MT_CG_MJC_LARB4_AXI_ASIF        = 293,      //MJC larb4 axi asif
                                                
    MT_CG_VENC_LARB                 = 320,      //LARB_CKE
    MT_CG_VENC_VENC                 = 324,      //VENC_CKE
    MT_CG_VENC_JPGENC               = 328,      //JPGENC_CKE
    MT_CG_VENC_JPGDEC               = 332,      //JPGDEC_CKE

    CG_PERI_FROM					= MT_CG_PERI_NFI,
    CG_PERI_TO						= MT_CG_PERI_SPI0,
    NR_PERI_CLKS					= 30,
    
    CG_INFRA_FROM                   = MT_CG_INFRA_DBGCLK,
    CG_INFRA_TO                     = MT_CG_INFRA_PMICWRAP,
    NR_INFRA_CLKS                   = 16,
    
    CG_DISP0_FROM                   = MT_CG_DISP0_SMI_COMMON,
    CG_DISP0_TO                     = MT_CG_DISP0_DISP_OD,
    NR_DISP0_CLKS                   = 32,
    
    CG_DISP1_FROM                   = MT_CG_DISP1_DISP_PWM0_MM,
    CG_DISP1_TO                     = MT_CG_DISP1_DPI_ENGINE,
    NR_DISP1_CLKS                   = 10,
    
    CG_IMAGE_FROM                   = MT_CG_IMAGE_LARB2_SMI,
    CG_IMAGE_TO                     = MT_CG_IMAGE_FD,
    NR_IMAGE_CLKS                   = 7,
    
    CG_MFG_FROM                     = MT_CG_MFG_AXI,
    CG_MFG_TO                       = MT_CG_MFG_26M,
    NR_MFG_CLKS                     = 4,
    
    CG_AUDIO_FROM                   = MT_CG_AUDIO_AFE,
    CG_AUDIO_TO                     = MT_CG_AUDIO_ADDA2,
    NR_AUDIO_CLKS                   = 9,
    
    CG_VDEC0_FROM                   = MT_CG_VDEC0_VDEC,
    CG_VDEC0_TO                     = MT_CG_VDEC0_VDEC,
    NR_VDEC0_CLKS                   = 1,
    
    CG_VDEC1_FROM                   = MT_CG_VDEC1_LARB,
    CG_VDEC1_TO                     = MT_CG_VDEC1_LARB,
    NR_VDEC1_CLKS                   = 1,
    
    CG_MJC_FROM                     = MT_CG_MJC_SMI_LARB,
    CG_MJC_TO                       = MT_CG_MJC_LARB4_AXI_ASIF,
    NR_MJC_CLKS                     = 5,
    
    CG_VENC_FROM                    = MT_CG_VENC_LARB,
    CG_VENC_TO                      = MT_CG_VENC_JPGDEC,
    NR_VENC_CLKS                    = 4,
    
    NR_CLKS                         = 333,
};

enum {
	//CLK_CFG_0
    MT_MUX_MM           = 0,
//    MT_MUX_DDRPHY     = 1,
//    MT_MUX_MEM		= 2,
//    MT_MUX_AXI		= 3,

    //CLK_CFG_1
    MT_MUX_MFG          = 1,
    MT_MUX_VENC         = 2,
    MT_MUX_VDEC         = 3,
    MT_MUX_PWM          = 4,

    //CLK_CFG_2
    MT_MUX_USB20        = 5,
    MT_MUX_SPI          = 6,
    MT_MUX_UART         = 7,
    MT_MUX_CAMTG        = 8,

    //CLK_CFG_3
    MT_MUX_MSDC30_1     = 9,
    MT_MUX_MSDC50_0     = 10,
    MT_MUX_MSDC50_0_hclk = 11,
    MT_MUX_USB30        = 12,

    //CLK_CFG_4
    MT_MUX_AUDINTBUS    = 13,
    MT_MUX_AUDIO        = 14,
    MT_MUX_MSDC30_3     = 15,
    MT_MUX_MSDC30_2     = 16,
    
    //CLK_CFG_5
    MT_MUX_MJC          = 17,
    MT_MUX_SCP          = 18,
    MT_MUX_PMICSPI      = 19,
    
    //CLK_CFG_6
    MT_MUX_AUD1         = 20,
    MT_MUX_CCI400       = 21,
    MT_MUX_IRDA         = 22,
    MT_MUX_DPI0         = 23,

    //CLK_CFG_7
    MT_MUX_SCAM          = 24,
    MT_MUX_AXI_MFG_IN_AS = 25,
    MT_MUX_MEM_MFG_IN_AS = 26,
    MT_MUX_AUD2          = 27,

    NR_MUXS             = 28,
};

enum {
    ARMCA15PLL = 0,
    ARMCA7PLL  = 1,
    MAINPLL    = 2,
    MSDCPLL    = 3,
    UNIVPLL    = 4,
    MMPLL      = 5,
    VENCPLL    = 6,
    TVDPLL     = 7,
    MPLL       = 8,
    VCODECPLL  = 9,
    APLL1      = 10,
    APLL2      = 11,
    NR_PLLS    = 12,
};

enum {
    SYS_MD1       = 0,
    SYS_DIS       = 1,
//    SYS_MFG_ASYNC = 2,
//    SYS_MFG_2D    = 3,
    SYS_MFG       = 2,
    SYS_ISP       = 3,
    SYS_VDE       = 4,
    SYS_MJC       = 5,
    SYS_VEN       = 6,
    SYS_AUD       = 7,
    NR_SYSS       = 8,
};

enum {
    MT_LARB_DISP = 0,
    MT_LARB_VDEC = 1,
    MT_LARB_IMG  = 2,
    MT_LARB_VENC = 3,
    MT_LARB_MJC  = 4,
};

/* larb monitor mechanism definition*/
enum {
    LARB_MONITOR_LEVEL_HIGH     = 10,
    LARB_MONITOR_LEVEL_MEDIUM   = 20,
    LARB_MONITOR_LEVEL_LOW      = 30,
};

struct larb_monitor {
    struct list_head link;
    int level;
    void (*backup)(struct larb_monitor *h, int larb_idx);       /* called before disable larb clock */
    void (*restore)(struct larb_monitor *h, int larb_idx);      /* called after enable larb clock */
};

enum monitor_clk_sel_0{
    no_clk_0             = 0,
    AD_UNIV_624M_CK      = 5,
    AD_UNIV_416M_CK      = 6,
    AD_UNIV_249P6M_CK    = 7,
    AD_UNIV_178P3M_CK_0  = 8,
    AD_UNIV_48M_CK       = 9,
    AD_USB_48M_CK        = 10,
    rtc32k_ck_i_0        = 20,
    AD_SYS_26M_CK_0      = 21,
};
enum monitor_clk_sel{
    no_clk               = 0,
    AD_SYS_26M_CK        = 1,
    rtc32k_ck_i          = 2,
    clkph_MCLK_o         = 7,
    AD_DPICLK            = 8,
    AD_MSDCPLL_CK        = 9,
    AD_MMPLL_CK          = 10,
    AD_UNIV_178P3M_CK    = 11,
    AD_MAIN_H156M_CK     = 12,
    AD_VENCPLL_CK        = 13,
};

enum ckmon_sel{
    clk_ckmon0           = 0,
    clk_ckmon1           = 1,
    clk_ckmon2           = 2,
    clk_ckmon3           = 3,
};

extern void register_larb_monitor(struct larb_monitor *handler);
extern void unregister_larb_monitor(struct larb_monitor *handler);

/* clock API */
extern int enable_clock(enum cg_clk_id id, char *mod_name);
extern int disable_clock(enum cg_clk_id id, char *mod_name);
extern int mt_enable_clock(enum cg_clk_id id, char *mod_name);
extern int mt_disable_clock(enum cg_clk_id id, char *mod_name);

extern int enable_clock_ext_locked(int id, char *mod_name);
extern int disable_clock_ext_locked(int id, char *mod_name);

extern int clock_is_on(int id);

extern int clkmux_sel(int id, unsigned int clksrc, char *name);
extern void enable_mux(int id, char *name);
extern void disable_mux(int id, char *name);

extern void clk_set_force_on(int id);
extern void clk_clr_force_on(int id);
extern int clk_is_force_on(int id);

/* pll API */
extern int enable_pll(int id, char *mod_name);
extern int disable_pll(int id, char *mod_name);

extern int pll_hp_switch_on(int id, int hp_on);
extern int pll_hp_switch_off(int id, int hp_off);

extern int pll_fsel(int id, unsigned int value);
extern int pll_is_on(int id);

/* subsys API */
extern int enable_subsys(int id, char *mod_name);
extern int disable_subsys(int id, char *mod_name);

extern int subsys_is_on(int id);
extern int md_power_on(int id);
extern int md_power_off(int id, unsigned int timeout);

/* other API */

extern void set_mipi26m(int en);
extern void set_ada_ssusb_xtal_ck(int en);

const char* grp_get_name(int id);
extern int clkmgr_is_locked(void);

/* init */
//extern void mt_clkmgr_init(void);

extern int clk_monitor_0(enum ckmon_sel ckmon, enum monitor_clk_sel_0 sel, int div);
extern int clk_monitor(enum ckmon_sel ckmon, enum monitor_clk_sel sel, int div);

extern void cci400_sel_for_ddp(void);
extern void clk_stat_check(int id);

#if 0
extern void all_force_off(void);

#define AP_PLL_CON7	/*((UINT32P)*/(APMIXED_BASE+0x001C)//)

#define MD_PLL_MIXEDSYS_BASE 			(0x20120000)
#define PLL_DFS_CON7					/*((UINT16P)*/(MD_PLL_MIXEDSYS_BASE+0x00AC)//)
#define PLL_PLL_CON4					/*((UINT16P)*/(MD_PLL_MIXEDSYS_BASE+0x0050)//)
#define PLL_CLKSW_CKSEL4				/*((UINT16P)*/(MD_PLL_MIXEDSYS_BASE+0x0094)//)
#define PLL_CLKSW_CKSEL6				/*((UINT16P)*/(MD_PLL_MIXEDSYS_BASE+0x009C)//)
#define PLL_MDPLL_CON0					/*((UINT16P)*/(MD_PLL_MIXEDSYS_BASE+0x0100)//)
#define PLL_ARM7PLL_CON0				/*((UINT16P)*/(MD_PLL_MIXEDSYS_BASE+0x0150)//)
#define PLL_ARM7PLL_CON1	   			/*((UINT16P)*/(MD_PLL_MIXEDSYS_BASE+0x0154)//)

#define Reg_2012045C					0x2012045C

#define Reg_200308B0					0x200308B0
#define Reg_266030C8					0x266030C8					
#define Reg_2660306C					0x2660306C					
#define Reg_26603070					0x26603070
#define Reg_26603074					0x26603074
#define Reg_26603078					0x26603078
#define Reg_26602000					0x26602000
#define Reg_26602004					0x26602004
#define Reg_26602008					0x26602008
#define Reg_2660200C					0x2660200C
#define Reg_26632000					0x26632000
#define Reg_26632004					0x26632004
#define Reg_26622000					0x26622000
#define Reg_26622004					0x26622004
#define Reg_26622008					0x26622008
#define Reg_2662200C					0x2662200C
#define Reg_26622010					0x26622010
#define Reg_26622014					0x26622014
#define Reg_26622018					0x26622018
#define Reg_2662201C					0x2662201C
#define Reg_26642000					0x26642000
#define Reg_26642004					0x26642004
#define Reg_26642008					0x26642008
#define Reg_26652000					0x26652000
#define Reg_26652004					0x26652004
#define Reg_26652008					0x26652008
#define Reg_2665200C					0x2665200C
#define Reg_26612000					0x26612000
#define Reg_26612004					0x26612004
#define Reg_26612008					0x26612008
#define Reg_2661200C					0x2661200C
#define Reg_266031B4					0x266031B4
#define Reg_266031C4					0x266031C4
#define Reg_26602030					0x26602030
#define Reg_26602034					0x26602034
#define Reg_26602038					0x26602038
#define Reg_2660203C					0x2660203C
#define Reg_26602040					0x26602040
#define Reg_26632028					0x26632028
#define Reg_2663202C					0x2663202C
#define Reg_26632030					0x26632030
#define Reg_26632034					0x26632034
#define Reg_26632038					0x26632038
#define Reg_26622044					0x26622044
#define Reg_26622048					0x26622048
#define Reg_2662204C					0x2662204C
#define Reg_26622050					0x26622050
#define Reg_26622054					0x26622054
#define Reg_2664202C					0x2664202C
#define Reg_26642030					0x26642030
#define Reg_26642034					0x26642034
#define Reg_26642038					0x26642038     
#define Reg_2664203C					0x2664203C
#define Reg_2665202C					0x2665202C
#define Reg_26652030					0x26652030
#define Reg_26652034					0x26652034
#define Reg_26652038					0x26652038
#define Reg_2665203C					0x2665203C
#define Reg_2661202C					0x2661202C
#define Reg_26612030					0x26612030
#define Reg_26612034					0x26612034
#define Reg_26612038					0x26612038
#define Reg_2661203C					0x2661203C
#define Reg_266030A0					0x266030A0
#define Reg_266030A4					0x266030A4
#define Reg_26603118					0x26603118
#define Reg_26603104					0x26603104
#define Reg_26603100					0x26603100
#define Reg_26603004					0x26603004
#define Reg_26603110					0x26603110            
#define Reg_266030F0					0x266030F0        
    
#define Reg_266030d4            		0x266030d4
#define Reg_266030B8                    0x266030B8
#define Reg_266030BC                    0x266030BC

#define Reg_26604014					0x26604014
#define Reg_26604018            		0x26604018
#define Reg_2660401C            		0x2660401C
#define Reg_26604028					0x26604028

#define Reg_26604058					0x26604058
#define Reg_26603120					0x26603120
#define Reg_26604000					0x26604000


            
#define TDD_REG 						0x24000000
            
#define MD_TOPSM_BASE 		  			0x20030000
#define MD_TOPSM_RM_TMR_PWR0 		    /*((UINT32P)*/(MD_TOPSM_BASE+0x0018)//)
#define MD_TOPSM_RM_PWR_CON0 		    /*((UINT32P)*/(MD_TOPSM_BASE+0x0800)//)
#define MD_TOPSM_RM_PWR_CON1			/*((UINT32P)*/(MD_TOPSM_BASE+0x0804)//)
#define MD_TOPSM_RM_PWR_CON2			/*((UINT32P)*/(MD_TOPSM_BASE+0x0808)//)
#define MD_TOPSM_RM_PWR_CON3			/*((UINT32P)*/(MD_TOPSM_BASE+0x080c)//)
#define MD_TOPSM_RM_PLL_MASK0			/*((UINT32P)*/(MD_TOPSM_BASE+0x0830)//)
#define MD_TOPSM_RM_PLL_MASK1			/*((UINT32P)*/(MD_TOPSM_BASE+0x0834)//)
#define MD_TOPSM_SM_REQ_MASK			/*((UINT32P)*/(MD_TOPSM_BASE+0x08b0)//)
#define MD_TOPSM_TOPSM_DBG_FLAG_SEL		/*((UINT32P)*/(MD_TOPSM_BASE+0x0a1c)//)
            
#define MODEM_LITE_TOPSM_BASE 			0x23010000
#define MODEM_LITE_TOPSM_RM_TMR_PWR0	/*((UINT32P)*/(MODEM_LITE_TOPSM_BASE+0x0018)//)
#define MODEM_LITE_TOPSM_RM_TMR_PWR1	/*((UINT32P)*/(MODEM_LITE_TOPSM_BASE+0x001c)//)
#define MODEM_LITE_TOPSM_RM_PWR_CON0	/*((UINT32P)*/(MODEM_LITE_TOPSM_BASE+0x0800)//)
#define MODEM_LITE_TOPSM_RM_PWR_CON1	/*((UINT32P)*/(MODEM_LITE_TOPSM_BASE+0x0804)//)
#define MODEM_LITE_TOPSM_RM_PLL_MASK0 	/*((UINT32P)*/(MODEM_LITE_TOPSM_BASE+0x0830)//)
#define MODEM_LITE_TOPSM_RM_PLL_MASK1 	/*((UINT32P)*/(MODEM_LITE_TOPSM_BASE+0x0834)//)
#define MODEM_LITE_TOPSM_SM_REQ_MASK  	/*((UINT32P)*/(MODEM_LITE_TOPSM_BASE+0x08B0)//)
            
#define MODEM_TOPSM_BASE 				0x27010000
#define MODEM_TOPSM_RM_TMR_PWR0			/*((UINT32P)*/(MODEM_TOPSM_BASE+0x0018)//)
#define MODEM_TOPSM_RM_TMR_PWR1			/*((UINT32P)*/(MODEM_TOPSM_BASE+0x001c)//)
#define MODEM_TOPSM_RM_PWR_CON1			/*((UINT32P)*/(MODEM_TOPSM_BASE+0x0804)//)
#define MODEM_TOPSM_RM_PWR_CON2			/*((UINT32P)*/(MODEM_TOPSM_BASE+0x0808)//)
#define MODEM_TOPSM_RM_PWR_CON3			/*((UINT32P)*/(MODEM_TOPSM_BASE+0x080C)//)
#define MODEM_TOPSM_RM_PWR_CON4			/*((UINT32P)*/(MODEM_TOPSM_BASE+0x0810)//)
#define MODEM_TOPSM_RM_PLL_MASK0 	    /*((UINT32P)*/(MODEM_TOPSM_BASE+0x0830)//)
#define MODEM_TOPSM_RM_PLL_MASK1 	    /*((UINT32P)*/(MODEM_TOPSM_BASE+0x0834)//)
#define MODEM_TOPSM_SM_REQ_MASK  	    /*((UINT32P)*/(MODEM_TOPSM_BASE+0x08B0)//)

//#define REG1 0x2660306C
//#define REG2 0x26603070
//#define REG3 0x26603074
//#define REG4 0x26603078

#endif

#endif
