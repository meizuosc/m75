#ifndef __CCCI_PLATFORM_CFG_H__
#define __CCCI_PLATFORM_CFG_H__
#include <linux/version.h>
#include <mach/irqs.h>
#include <mach/mt_irq.h>
#include <mach/mt_reg_base.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_boot.h>
#include <mach/sync_write.h>
//-------------ccci driver configure------------------------//
#define MD1_DEV_MAJOR		(184)
#define MD2_DEV_MAJOR		(169)

//#define CCCI_PLATFORM_L 		0x3536544D
//#define CCCI_PLATFORM_H 		0x31453537
#define CCCI_PLATFORM 			"MT6582E1"
#define CCCI1_DRIVER_VER 		0x20121001
#define CCCI2_DRIVER_VER 		0x20121001

#define CURR_SEC_CCCI_SYNC_VER			(1)	// Note: must sync with sec lib, if ccci and sec has dependency change

#define CCMNI_V1            (1)
#define CCMNI_V2            (2)

#define MD_HEADER_VER_NO    (2)
#define GFH_HEADER_VER_NO   (1)


//-------------md share memory configure----------------//
//Common configuration
#define MD_EX_LOG_SIZE					(2*1024)
#define CCCI_MISC_INFO_SMEM_SIZE		(1*1024)
#define CCCI_SHARED_MEM_SIZE 			UL(0x200000) // 2M align for share memory

#define MD_IMG_DUMP_SIZE				(1<<8)
#define DSP_IMG_DUMP_SIZE				(1<<9)

#define CCMNI_V1_PORT_NUM               (3)          //For V1 CCMNI
#define CCMNI_V2_PORT_NUM               (3) 		 // For V2 CCMNI


// MD SYS1 configuration
#define CCCI1_PCM_SMEM_SIZE				(16 * 2 * 1024)				// PCM
#define CCCI1_MD_LOG_SIZE				(137*1024*4+64*1024+112*1024)	// MD Log

#define RPC1_MAX_BUF_SIZE				2048 //(6*1024)
#define RPC1_REQ_BUF_NUM				2 			 //support 2 concurrently request	

#define CCCI1_TTY_BUF_SIZE			    (16 * 1024)
#define CCCI1_CCMNI_BUF_SIZE			(16*1024)
#define CCCI1_TTY_PORT_NUM    			(3)
//#define CCCI1_CCMNI_V1_PORT_NUM			(3) 		 // For V1 CCMNI


// MD SYS2 configuration
#define CCCI2_PCM_SMEM_SIZE				(16 * 2 * 1024)					// PCM 
#define CCCI2_MD_LOG_SIZE				(137*1024*4+64*1024+112*1024)	// MD Log

#define RPC2_MAX_BUF_SIZE				2048 //(6*1024)
#define RPC2_REQ_BUF_NUM				2 			 //support 2 concurrently request	

#define CCCI2_TTY_BUF_SIZE			    (16 * 1024)
#define CCCI2_CCMNI_BUF_SIZE			(16*1024)
#define CCCI2_TTY_PORT_NUM  			(3)
//#define CCCI2_CCMNI_V1_PORT_NUM			(3) 		 // For V1 CCMNI




//-------------feature enable/disable configure----------------//
/******security feature configure******/
//#define  ENCRYPT_DEBUG                            	//enable debug log for SECURE_ALGO_OP, always disable
//#define  ENABLE_MD_IMG_SECURITY_FEATURE 	//disable for bring up, need enable by security owner after security feature ready

/******share memory configure******/
#define CCCI_STATIC_SHARED_MEM           //using ioremap to allocate share memory, not dma_alloc_coherent
//#define  MD_IMG_SIZE_ADJUST_BY_VER        //md region can be adjusted by 2G/3G, ex, 2G: 10MB for md+dsp, 3G: 22MB for md+dsp

/******md header check configure******/
//#define  ENABLE_CHIP_VER_CHECK
//#define  ENABLE_2G_3G_CHECK
#define  ENABLE_MEM_SIZE_CHECK


/******EMI MPU protect configure******/
#define  ENABLE_EMI_PROTECTION  			//disable for bring up           

/******md memory remap configure******/
#define  ENABLE_MEM_REMAP_HW

/******md wake up workaround******/
//#define  ENABLE_MD_WAKE_UP        			//only enable for mt6589 platform         

//******other feature configure******//
//#define  ENABLE_LOCK_MD_SLP_FEATURE
#define ENABLE_32K_CLK_LESS					//disable for bring up
#define  ENABLE_MD_WDT_PROCESS				//disable for bring up for md not enable wdt at bring up
//#define ENABLE_MD_WDT_DBG					//disable on official branch, only for local debug
#define ENABLE_AEE_MD_EE						//disable for bring up
#define  ENABLE_DRAM_API						//awlays enable for bring up



/*******************AP CCIF register define**********************/
#define CCIF_BASE				(AP_CCIF_BASE)
#define CCIF_CON(addr)			((addr) + 0x0000)
#define CCIF_BUSY(addr)			((addr) + 0x0004)
#define CCIF_START(addr)		((addr) + 0x0008)
#define CCIF_TCHNUM(addr)		((addr) + 0x000C)
#define CCIF_RCHNUM(addr)		((addr) + 0x0010)
#define CCIF_ACK(addr)			((addr) + 0x0014)

/* for CHDATA, the first half space belongs to AP and the remaining space belongs to MD */
#define CCIF_TXCHDATA(addr) 	((addr) + 0x0100)
#define CCIF_RXCHDATA(addr) 	((addr) + 0x0100 + 128)

/* Modem CCIF */
#define MD_CCIF_CON(base)		((base) + 0x0000)
#define MD_CCIF_BUSY(base)		((base) + 0x0004)
#define MD_CCIF_START(base)		((base) + 0x0008)
#define MD_CCIF_TCHNUM(base)	((base) + 0x000C)
#define MD_CCIF_RCHNUM(base)	((base) + 0x0010)
#define MD_CCIF_ACK(base)		((base) + 0x0014)

/* define constant */
#define CCIF_CON_SEQ 0x00 /* sequencial */
#define CCIF_CON_ARB 0x01 /* arbitration */
//#define CCIF_IRQ_CODE MT_AP_CCIF_IRQ_ID

// CCIF HW specific macro definition
#define CCIF_STD_V1_MAX_CH_NUM				(8)
#define CCIF_STD_V1_RUN_TIME_DATA_OFFSET	(0x140)		//need confirm
#define CCIF_STD_V1_RUM_TIME_MEM_MAX_LEN	(256-64)	//need confirm


/*******************other register define**********************/
//modem debug register and bit
#define MD_DEBUG_MODE			(DEBUGTOP_BASE+0x1A010)
#define MD_DBG_JTAG_BIT			1<<0


/************************* define funtion macro **************/
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36))
#define CCIF_MASK(irq) \
        do {    \
            mt65xx_irq_mask(irq);  \
        } while (0)
#define CCIF_UNMASK(irq) \
        do {    \
            mt65xx_irq_unmask(irq);  \
        } while (0)

#else
#define CCIF_MASK(irq) \
        do {    \
            disable_irq(irq); \
        } while (0)
#define CCIF_UNMASK(irq) \
        do {    \
            enable_irq(irq); \
        } while (0)
#endif

#define CCIF_CLEAR_PHY(pc)   do {	\
            *CCIF_ACK(pc) = 0xFFFFFFFF; \
        } while (0)


//#define CCCI_WRITEL(addr,val) mt65xx_reg_sync_writel((val), (addr))
//#define CCCI_WRITEW(addr,val) mt65xx_reg_sync_writew((val), (addr))
//#define CCCI_WRITEB(addr,val) mt65xx_reg_sync_writeb((val), (addr))

#define ccci_write32(a, v)			mt65xx_reg_sync_writel(v, a)
#define ccci_write16(a, v)			mt65xx_reg_sync_writew(v, a)
#define ccci_write8(a, v)			mt65xx_reg_sync_writeb(v, a)


#define ccci_read32(a)				(*((volatile unsigned int*)a))
#define ccci_read16(a)				(*((volatile unsigned short*)a))
#define ccci_read8(a)				(*((volatile unsigned char*)a))

#endif // __CCCI_PLATFORM_CFG_H__

