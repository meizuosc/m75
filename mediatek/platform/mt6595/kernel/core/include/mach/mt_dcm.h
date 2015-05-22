#ifndef _MT_DCM_H
#define _MT_DCM_H

#include "mach/mt_reg_base.h"



//#define CAM_BASE                	0xF5004000//0x15004000

// APB Module usb2
#define	USB0_DCM						(USB0_BASE+0x700)//0x11200700


// APB Module msdc
#define MSDC0_PATCH_BIT1				(MSDC0_BASE + 0x00B4)
#define MSDC1_PATCH_BIT1				(MSDC1_BASE + 0x00B4)
#define MSDC2_PATCH_BIT1				(MSDC2_BASE + 0x00B4)
#define MSDC3_PATCH_BIT1				(MSDC3_BASE + 0x00B4)


// APB Module pmic_wrap
#define PMIC_WRAP_DCM_EN			(PWRAP_BASE+0x144)

// APB Module i2c
#define I2C0_I2CREG_HW_CG_EN		(I2C0_BASE+0x054)
#define I2C1_I2CREG_HW_CG_EN		(I2C1_BASE+0x054)
#define I2C2_I2CREG_HW_CG_EN		(I2C2_BASE+0x054)
#define I2C3_I2CREG_HW_CG_EN		(I2C3_BASE+0x054)
#define I2C4_I2CREG_HW_CG_EN		(I2C4_BASE+0x054)


//MJC
// APB Module mjc_config
#define MJC_HW_DCM_DIS        	(MJC_CONFIG_BASE+0x0010)//0xF7000010
#define MJC_HW_DCM_DIS_SET    	(MJC_CONFIG_BASE+0x0014)//0xF7000014
#define MJC_HW_DCM_DIS_CLR    	(MJC_CONFIG_BASE+0x0018)//0xF7000018


//CPUSYS_dcm
#if 1//
#define MCUSYS_CONFIG			(MCUCFG_BASE + 0x001C)//0xF020001C
#define CACHE_CONFIG			(MCUCFG_BASE + 0x0100)//0xF0200100
#define ARMPLL_CTL				(MCUCFG_BASE + 0x0160)//0xF0200160
#endif




//AXI bus dcm
//TOPCKGen_dcm
#define DCM_CFG                 (CKSYS_BASE + 0x0004)//0x10000004


//CA7 DCM
#define CA7_CKDIV1				(INFRACFG_AO_BASE + 0x0008) //0x10001008
#define INFRA_TOPCKGEN_DCMCTL   (INFRACFG_AO_BASE + 0x0010) //0x10001010
#define INFRA_TOPCKGEN_DCMDBC   (INFRACFG_AO_BASE + 0x0014) //0x10001014


//infra dcm
#define INFRA_GLOBALCON_DCMCTL  (INFRACFG_AO_BASE   + 0x0050) //0x10001050
#define INFRA_GLOBALCON_DCMDBC  (INFRACFG_AO_BASE   + 0x0054) //0x10001054
#define INFRA_GLOBALCON_DCMFSEL (INFRACFG_AO_BASE   + 0x0058) //0x10001058
#define MM_MMU_DCM_DIS			(M4U_BASE           + 0x0050) //0x10205050
#define PERISYS_MMU_DCM_DIS		(PERISYS_IOMMU_BASE + 0x0050) //0x10214050

//peri dcm
#define PERI_GLOBALCON_DCMCTL        (PERICFG_BASE + 0x0050) //0x10003050
#define PERI_GLOBALCON_DCMDBC        (PERICFG_BASE + 0x0054) //0x10003054
#define PERI_GLOBALCON_DCMFSEL       (PERICFG_BASE + 0x0058) //0x10003058


#define channel_A_DRAMC_PD_CTRL           (DRAMC0_BASE + 0x01DC)//0x100041dc
#define channel_B_DRAMC_PD_CTRL           (DRAMC1_BASE + 0x01DC)//0x100111dc

//m4u dcm
//#define MMU_DCM					(SMI_MMU_TOP_BASE+0x5f0)

//smi_common dcm
//#define SMI_COMMON_DCM          0x10202300 //HW_DCM API_17

// APB Module smi
//Smi_common dcm
#define SMI_COMMON_SMI_DCM				(SMI_COMMON_BASE+0x300)//0x14022300


// APB Module smi
//Smi_secure dcm


#define SMI_CON						(SMI1_BASE+0x010)//SMI_CON
#define SMI_CON_SET					(SMI1_BASE+0x014)//SMI_CON_SET
#define SMI_CON_CLR					(SMI1_BASE+0x018)//SMI_CON_CLR



// APB Module smi_larb
#define SMI_LARB0_STA        	(SMI_LARB0_BASE + 0x00)//0x14021000
#define SMI_LARB0_CON        	(SMI_LARB0_BASE + 0x10)//0x14021010
#define SMI_LARB0_CON_SET       (SMI_LARB0_BASE + 0x14)//0x14021014
#define SMI_LARB0_CON_CLR       (SMI_LARB0_BASE + 0x18)//0x14021018

#define SMI_LARB1_STAT        	(SMI_LARB1_BASE + 0x00)//0x16010000
#define SMI_LARB1_CON        	(SMI_LARB1_BASE + 0x10)//0x16010010
#define SMI_LARB1_CON_SET       (SMI_LARB1_BASE + 0x14)//0x16010014
#define SMI_LARB1_CON_CLR       (SMI_LARB1_BASE + 0x18)//0x16010018

#define SMI_LARB2_STAT        	(SMI_LARB2_BASE + 0x00)//0x15001000
#define SMI_LARB2_CON        	(SMI_LARB2_BASE + 0x10)//0x15001010
#define SMI_LARB2_CON_SET       (SMI_LARB2_BASE + 0x14)//0x15001014
#define SMI_LARB2_CON_CLR       (SMI_LARB2_BASE + 0x18)//0x15001018

#define SMI_LARB3_STAT        	(SMI_LARB3_BASE + 0x00)//0x18001000
#define SMI_LARB3_CON        	(SMI_LARB3_BASE + 0x10)//0x18001010
#define SMI_LARB3_CON_SET       (SMI_LARB3_BASE + 0x14)//0x18001014
#define SMI_LARB3_CON_CLR       (SMI_LARB3_BASE + 0x18)//0x18001018

#define SMI_LARB4_STAT        	(SMI_LARB4_BASE + 0x00)//0x17002000
#define SMI_LARB4_CON        	(SMI_LARB4_BASE + 0x10)//0x17002010
#define SMI_LARB4_CON_SET       (SMI_LARB4_BASE + 0x14)//0x17002014
#define SMI_LARB4_CON_CLR       (SMI_LARB4_BASE + 0x18)//0x17002018

// APB Module emi
#define EMI_CONM       			(EMI_BASE + 0x60)//0x10203060



#if 0
//MFG
//MFG_DCM
// APB Module mfg_top
#define MFG_DCM_CON_0            (G3D_CONFIG_BASE + 0x10) //MFG_DCM_CON_0
#endif

/*
#define CAM_CTL_RAW_DCM_DIS         (CAM0_BASE + 0x190)//CAM_CTL_RAW_DCM_DIS
#define CAM_CTL_RGB_DCM_DIS         (CAM0_BASE + 0x194)//CAM_CTL_RGB_DCM_DIS
#define CAM_CTL_YUV_DCM_DIS         (CAM0_BASE + 0x198)//CAM_CTL_YUV_DCM_DIS
#define CAM_CTL_CDP_DCM_DIS         (CAM0_BASE + 0x19C)//CAM_CTL_CDP_DCM_DIS
#define CAM_CTL_DMA_DCM_DIS			(CAM0_BASE + 0x1B0)//CAM_CTL_DMA_DCM_DIS

#define CAM_CTL_RAW_DCM_STATUS     (CAM0_BASE + 0x1A0)//CAM_CTL_RAW_DCM_STATUS
#define CAM_CTL_RGB_DCM_STATUS     (CAM0_BASE + 0x1A4)//CAM_CTL_RGB_DCM_STATUS
#define CAM_CTL_YUV_DCM_STATUS     (CAM0_BASE + 0x1A8)//CAM_CTL_YUV_DCM_STATUS
#define CAM_CTL_CDP_DCM_STATUS     (CAM0_BASE + 0x1AC)//CAM_CTL_CDP_DCM_STATUS
#define CAM_CTL_DMA_DCM_STATUS     (CAM0_BASE + 0x1B4)//CAM_CTL_DMA_DCM_STATUS
*/

// APB Module cam1
#define CTL_RAW_DCM_DIS         (CAM1_BASE + 0x188) //0xF5004188
#define CTL_RAW_D_DCM_DIS       (CAM1_BASE + 0x18C) //0xF500418C
#define CTL_DMA_DCM_DIS         (CAM1_BASE + 0x190) //0xF5004190
#define CTL_RGB_DCM_DIS         (CAM1_BASE + 0x194) //0xF5004194
#define CTL_YUV_DCM_DIS         (CAM1_BASE + 0x198) //0xF5004198
#define CTL_TOP_DCM_DIS         (CAM1_BASE + 0x19C) //0xF500419C

// APB Module fdvt
#define FDVT_CTRL         		(FDVT_BASE + 0x19C) //0xF500B19C


#define JPGENC_DCM_CTRL         (JPGENC_BASE + 0x300) //0x18003300
#define JPGDEC_DCM_CTRL         (JPGDEC_BASE + 0x300) //0x18004300

//#define SMI_ISP_COMMON_DCMCON   0x15003010  	//82 N 89 Y
//#define SMI_ISP_COMMON_DCMSET   0x15003014	//82 N 89 Y
//#define SMI_ISP_COMMON_DCMCLR   0x15003018	//82 N 89 Y

//display sys
//mmsys_dcm
// APB Module mmsys_config
#define MMSYS_HW_DCM_DIS0        (MMSYS_CONFIG_BASE + 0x120)//MMSYS_HW_DCM_DIS0
#define MMSYS_HW_DCM_DIS_SET0    (MMSYS_CONFIG_BASE + 0x124)//MMSYS_HW_DCM_DIS_SET0
#define MMSYS_HW_DCM_DIS_CLR0    (MMSYS_CONFIG_BASE + 0x128)//MMSYS_HW_DCM_DIS_CLR0

#define MMSYS_HW_DCM_DIS1        (MMSYS_CONFIG_BASE + 0x130)//MMSYS_HW_DCM_DIS1
#define MMSYS_HW_DCM_DIS_SET1    (MMSYS_CONFIG_BASE + 0x134)//MMSYS_HW_DCM_DIS_SET1
#define MMSYS_HW_DCM_DIS_CLR1    (MMSYS_CONFIG_BASE + 0x138)//MMSYS_HW_DCM_DIS_CLR1

//venc sys

#define VENC_CLK_CG_CTRL       (VENC_BASE + 0xFC)//0x180020FC
#define VENC_CLK_DCM_CTRL      (VENC_BASE + 0xF4)//0x180020F4


// APB Module vdecsys_config
//VDEC_dcm
#define VDEC_DCM_CON            (VDEC_GCON_BASE + 0x18)//0x16000018




#define CPU_DCM                 (1U << 0)
#define IFR_DCM                 (1U << 1)
#define PER_DCM                 (1U << 2)
#define SMI_DCM                 (1U << 3)
#define EMI_DCM                 (1U << 4)
#define DIS_DCM                 (1U << 5)
#define ISP_DCM                 (1U << 6)
#define VDE_DCM                 (1U << 7)
//#define SMILARB_DCM				(1U << 8)
//#define TOPCKGEN_DCM			(1U << 8)
#define MJC_DCM					(1U << 8)
//#define ALL_DCM                 (CPU_DCM|IFR_DCM|PER_DCM|SMI_DCM|MFG_DCM|DIS_DCM|ISP_DCM|VDE_DCM|TOPCKGEN_DCM)
#define ALL_DCM                 (CPU_DCM|IFR_DCM|PER_DCM|SMI_DCM|EMI_DCM|DIS_DCM|ISP_DCM|VDE_DCM|MJC_DCM)
#define NR_DCMS                 (0x9)


//extern void dcm_get_status(unsigned int type);
extern void dcm_enable(unsigned int type);
extern void dcm_disable(unsigned int type);

extern void disable_cpu_dcm(void);
extern void enable_cpu_dcm(void);

extern void bus_dcm_enable(void);
extern void bus_dcm_disable(void);

extern void disable_infra_dcm(void);
extern void restore_infra_dcm(void);

extern void disable_peri_dcm(void);
extern void restore_peri_dcm(void);

extern void mt_dcm_init(void);
extern void dcm_CA7_L2_share_256K_to_external_enable(bool enable);

#endif
