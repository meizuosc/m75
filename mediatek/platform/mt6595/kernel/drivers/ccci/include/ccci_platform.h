#ifndef __CCCI_PLATFORM_H
#define __CCCI_PLATFORM_H

#include <mach/irqs.h>
#include <mach/mt6575_irq.h>
#include <mach/mt6575_reg_base.h>
#include <mach/mt6575_typedefs.h>
//#include <mach/mt6575_pll.h>
#include <mach/mt6575_boot.h>
#include <mach/sync_write.h>
#include <mach/mt6575_sc.h>
#include <linux/sched.h>
#include <asm/memory.h>
#include <ccci_rpc.h>
#include <cipher_header.h>
#include <sec_auth.h>
#include <SignHeader.h>
#include <sec_boot.h>
#include <sec_error.h>
#include <linux/uaccess.h>
#include <asm/string.h>

#include <linux/fs.h>



//*******************external Function definition*****************//
extern void start_emi_mpu_protect(void);
extern int  ccci_load_firmware(struct image_info *img_info);
extern int  platform_init(void);
extern void platform_deinit(void);
extern void dump_firmware_info(void);

//*****************external variable definition*******************//
extern 	int 			first;
extern  int *dsp_img_vir;
extern  int *md_img_vir;
extern  unsigned long 	mdconfig_base;
extern  unsigned long 	mdif_base;
extern  unsigned long	ccif_base;
extern  unsigned int	modem_infra_sys;
extern  unsigned int	md_rgu_base;
extern	unsigned int 	ap_infra_sys_base;
extern	unsigned int	md_ccif_base;

//******************** macro definition**************************//
#define CCCI_DEV_MAJOR 184

#define KERNEL_REGION_BASE (PHYS_OFFSET)
#define MODEM_REGION_BASE  (UL(0))

#define DSP_REGION_BASE_2G (UL(0x700000))
#define DSP_REGION_BASE_3G (UL(0x1300000))
#define DSP_REGION_LEN  (UL(0x00300000))

#define MDIF_REGION_BASE (UL(0x60110000))
#define MDIF_REGION_LEN  (UL(0x1d0))

#define CCIF_REGION_BASE (UL(0x60130000))
#define CCIF_REGION_LEN  (UL(0x200))

#define MDCFG_REGION_BASE (UL(0x60140000))
#define MDCFG_REGION_LEN  (UL(0x504))

#if defined(CCCI_STATIC_SHARED_MEM)
#define CCCI_SHARED_MEM_BASE UL(0x2000000) 
// started by 32MB
#define CCCI_SHARED_MEM_SIZE UL(0x200000)  
// 2MB shared memory because kernel physical memory must be 2MB alignment
#endif

/*modem&dsp name define*/
#define MD_INDEX 0
#define DSP_INDEX 1
#define IMG_CNT 2

#define MOEDM_IMAGE_NAME "modem.img"
#define DSP_IMAGE_NAME "DSP_ROM"

#define MOEDM_IMAGE_E1_NAME "modem_E1.img"
#define DSP_IMAGE_E1_NAME "DSP_ROM_E1"

#define MOEDM_IMAGE_E2_NAME "modem_E2.img"
#define DSP_IMAGE_E2_NAME "DSP_ROM_E2"


#ifndef CONFIG_MODEM_FIRMWARE_PATH
#define CONFIG_MODEM_FIRMWARE_PATH "/system/etc/firmware/"

#define MOEDM_IMAGE_PATH "/system/etc/firmware/modem.img"
#define DSP_IMAGE_PATH "/system/etc/firmware/DSP_ROM"

#define MOEDM_IMAGE_E1_PATH "/system/etc/firmware/modem_E1.img"
#define DSP_IMAGE_E1_PATH "/system/etc/firmware/DSP_ROM_E1"

#define MOEDM_IMAGE_E2_PATH "/system/etc/firmware/modem_E2.img"
#define DSP_IMAGE_E2_PATH "/system/etc/firmware/DSP_ROM_E2"

#endif


/*modem&dsp check header macro define*/
#define DSP_VER_3G  0x0
#define DSP_VER_2G  0x1
#define DSP_VER_INVALID  0x2

#define VER_3G_STR  "3G"
#define VER_2G_STR  "2G"
#define VER_INVALID_STR  "VER_INVALID"

#define DEBUG_STR   "Debug"
#define RELEASE_STR  "Release"
#define INVALID_STR "VER_INVALID"

#define DSP_ROM_TYPE 0x0104
#define DSP_BL_TYPE  0x0003

#define DSP_ROM_STR  "DSP_ROM"
#define DSP_BL_STR   "DSP_BL"

#define MD_HEADER_MAGIC_NO "CHECK_HEADER"
#define MD_HEADER_VER_NO 0x1

#define GFH_HEADER_MAGIC_NO 0x4D4D4D
#define GFH_HEADER_VER_NO 0x1
#define GFH_FILE_INFO_TYPE 0x0
#define GFH_CHECK_HEADER_TYPE 0x104

#define DSP_2G_BIT 16
#define DSP_DEBUG_BIT 17

#define PLATFORM_VER_STR "MT6573_S"

//*********************HW register definitions*********************//
#define MDSYS_RST(base)	      	((volatile unsigned int*)(base+0x010))
#define MD_SLPPRT_BUS(base)   	((volatile unsigned int*)(base+0x0430))
#define MD_SLPPRT_EMI(base)   	((volatile unsigned int*)(base+0x0434))
#define MD_WAY_CON0(base)     	((volatile unsigned int*)(base+0x0420))
#define MD_BUS_BUSY(base)     	((volatile unsigned int*)(base+0x042c))
#define MDMCU_CG_CLR1(base)		((volatile unsigned int*)(base+0x0318))

//FIXME: mask me for MT6575 porting
//#define DBG_PDN 	((volatile unsigned int*)(APCONFIG_BASE+0x0320))

#define MDIF_SH_DCFG(base)	((volatile unsigned short*)(base+0x00))
#define MDIF_SH_DSTA(base)	((volatile unsigned short*)(base+0x04))
#define MDIF_SH_DRSVCTH(base)	((volatile unsigned short*)(base+0x14))	

#if 0
#define SW_DSP_RSTN 	((volatile unsigned int*)(AP_RGU_BASE+0x014))	
#define DSP_WDT_CTL	((volatile unsigned short*)(AP_RGU_BASE+0x020))	

#define MD_WDT_CTL		((volatile unsigned short*)(AP_RGU_BASE+0x030))
#define MD_WDT_SWRST	((volatile unsigned short*)(AP_RGU_BASE+0x044))
#define MD_WDT_STA		((volatile unsigned short*)(AP_RGU_BASE+0x03c))
#define MD_WDT_TIMEOUT	((volatile unsigned short*)(AP_RGU_BASE+0x034))
#define MD_WDT_RESTART	((volatile unsigned short*)(AP_RGU_BASE+0x038))
#define MD_WDT_INTERNAL	((volatile unsigned short*)(AP_RGU_BASE+0x040))
#endif

/* Register offset definition */
/*-- Modem --*/
#define WDT_MD_MODE(base)		( *((volatile unsigned int*)(base + 0x00)) )
#define WDT_MD_LENGTH(base)		( *((volatile unsigned int*)(base + 0x04)) )
#define WDT_MD_RESTART(base)		( *((volatile unsigned int*)(base + 0x08)) )
#define WDT_MD_STA(base)		( *((volatile unsigned int*)(base + 0x0C)) )
/*-- DSP --*/
#define WDT_DSP_MODE(base)		( *((volatile unsigned int*)(base + 0x20)) )
#define WDT_DSP_LENGTH(base)		( *((volatile unsigned int*)(base + 0x24)) )
#define WDT_DSP_RESTART(base)		( *((volatile unsigned int*)(base + 0x28)) )
#define WDT_DSP_STA(base)		( *((volatile unsigned int*)(base + 0x2C)) )
/*-- AP_MD_RGU_SW_MDMCU_RSTN --*/
#define WDT_MD_MCU_RSTN(base)		( *((volatile unsigned int*)(base + 0x40)) )
#define WDT_DSP_MCU_RSTN(base)		( *((volatile unsigned int*)(base + 0x44)) )

/* MD & DSP WDT Default value and KEY */
/*-- Modem --*/
#define WDT_MD_MODE_DEFAULT		(0x3)
#define WDT_MD_MODE_KEY			(0x22<<8)
#define WDT_MD_LENGTH_DEFAULT	(0x7FF<<5)
#define WDT_MD_LENGTH_KEY		(0x8)
#define WDT_MD_RESTART_KEY		(0x1971)
/*-- DSP --*/
#define WDT_DSP_MODE_DEFAULT	(0x3)
#define WDT_DSP_MODE_KEY		(0x22<<8)
#define WDT_DSP_LENGTH_DEFAULT	(0x7FF<<5)
#define WDT_DSP_LENGTH_KEY		(0x8)
#define WDT_DSP_RESTART_KEY		(0x1971)

/* Infra top AXI */
#define INFRA_TOP_AXI_SI4_CTL(base)	( *((volatile unsigned int*)(base + 0x0210)) )
#define INFRA_TOP_AXI_PROTECT_EN(base)	( *((volatile unsigned int*)(base + 0x0220)) )
#define INFRA_TOP_AXI_PROTECT_STA(base)	( *((volatile unsigned int*)(base + 0x0224)) )

/* MD CCIF */
#define MD_CCIF_ACK(base)		( *((volatile unsigned int*)(base + 0x14)) )

/*define IRQ code for Modem WDT and DSP WDT*/
//#define MT6575_MDWDT_IRQ_LINE 	MT6575_MD_WDT_DSP_IRQ_ID
//#define MT6575_DSPWDT_IRQ_LINE  MT6575_MD_WDT_IRQ_ID
#define MT6575_MDWDT_IRQ_LINE 	MT6575_MD_WDT_IRQ_ID
#define MT6575_DSPWDT_IRQ_LINE  MT6575_MD_WDT_DSP_IRQ_ID

/*define CCIF register*/
#define CCIF_BASE AP_CCIF_BASE
#define CCIF_CON(addr) ((volatile unsigned int *)((addr) + 0x0000))
#define CCIF_BUSY(addr) ((volatile unsigned int *)((addr) + 0x0004))
#define CCIF_START(addr) ((volatile unsigned int *)((addr) + 0x0008))
#define CCIF_TCHNUM(addr) ((volatile unsigned int *)((addr)+ 0x000C))
#define CCIF_RCHNUM(addr) ((volatile unsigned int *)((addr) + 0x0010))
#define CCIF_ACK(addr) ((volatile unsigned int *)((addr) + 0x0014))

/* for CHDATA, the first half space belongs to AP and the remaining space belongs to MD */
#define CCIF_TXCHDATA(addr) ((volatile unsigned int *)((addr) + 0x0100))
#define CCIF_RXCHDATA(addr) ((volatile unsigned int *)((addr) + 0x0100 + 128))

/* define constant */
#define CCIF_CON_SEQ 0x00 /* sequencial */
#define CCIF_CON_ARB 0x01 /* arbitrate */
#define CCIF_MAX_PHY 8
#define CCIF_IRQ_CODE MT6575_AP_CCIF_IRQ_ID

/* define macro */
#define CCIF_MASK(irq) \
        do {    \
            disable_irq(irq); \
        } while (0)
#define CCIF_UNMASK(irq) \
        do {    \
            enable_irq(irq); \
        } while (0)

#define CCIF_CLEAR_PHY(pc)   do {	\
            *CCIF_ACK(pc) = 0xFFFFFFFF; \
        } while (0)


#define CCCI_WRITEL(addr,val) mt65xx_reg_sync_writel((val), (addr))
#define CCCI_WRITEW(addr,val) mt65xx_reg_sync_writew((val), (addr))
#define CCCI_WRITEB(addr,val) mt65xx_reg_sync_writeb((val), (addr))


//*****************structure definition*****************************//

// DSP check image header
typedef struct {
    U32 m_magic_ver;          /* bit23-bit0="MMM", not31-bit24:header version number=1*/
    U16 m_size;               /* the size of GFH structure*/
    U16 m_type;               /* m_type=0, GFH_FILE_INFO_v1; m_type=0x104, GFH_CHECK_CFG_v1*/
} GFH_HEADER;

typedef struct {
    GFH_HEADER m_gfh_hdr;     /* m_type=0*/
    U8 m_identifier[12];      /* "FILE_INFO" */
    U32 m_file_ver;           /* bit16=0:3G, bit16=1:2G;  bit17=0:release, bit17=1:debug*/
    U16 m_file_type;          /* DSP_ROM:0x0104; DSP_BL:0x0003*/
    U8 dummy1; 
    U8 dummy2;
    U32 dummy3[7];
} GFH_FILE_INFO_v1;

typedef struct {
    GFH_HEADER m_gfh_hdr;      /* m_type=0x104, m_size=0xc8*/
    U32 m_product_ver;        /* 0x0:invalid; 0x1:debug version; 0x2:release version */
    U32 m_image_type;          /* 0x0:invalid; 0x1:2G modem; 0x2: 3G modem */
    U8 m_platform_id[16];	   /* chip version, ex:MT6573_S01 */
    U8 m_project_id[64];	   /* build version, ex: MAUI.11A_MD.W11.31 */
    U8 m_build_time[64];	   /* build time, ex: 2011/8/4 04:19:30 */
    U8 reserved[64];
}GFH_CHECK_CFG_v1;


typedef enum{
	INVALID_VARSION = 0,
	DEBUG_VERSION,
	RELEASE_VERSION
}PRODUCT_VER_TYPE;


typedef struct{
    U8 check_header[12];	    /* magic number is "CHECK_HEADER"*/
    U32 header_verno;	        /* header structure version number */
    U32 product_ver;	        /* 0x0:invalid; 0x1:debug version; 0x2:release version */
    U32 image_type;	            /* 0x0:invalid; 0x1:2G modem; 0x2: 3G modem */
    U8 platform[16];	        /* MT6573_S01 or MT6573_S02 */
    U8 build_time[64];	        /* build time string */
    U8 build_ver[64];	        /* project version, ex:11A_MD.W11.28 */
    U32 reserved_info[4];       /* for reserved */
    U32 size;	                /* the size of this structure */
}MD_CHECK_HEADER;


typedef struct{
	unsigned long modem_region_base;
	unsigned long modem_region_len;
	unsigned long dsp_region_base;
	unsigned long dsp_region_len;
	unsigned long mdif_region_base;
	unsigned long mdif_region_len;
	unsigned long ccif_region_base;
	unsigned long ccif_region_len;
	unsigned long mdcfg_region_base;
	unsigned long mdcfg_region_len;
}CCCI_REGION_LAYOUT;

typedef enum {
	E_NOMEM=1,
	E_FILE_OPEN, 	//=2
	E_FILE_READ, 	//=3
	E_KERN_READ,	//=4
	E_NO_ADDR,	//=5
	E_NO_FIRST_BOOT,//=6
	E_LOAD_FIRM,	//=7
	E_FIRM_NULL,	//=8
	E_CHECK_HEAD,	//=9
	E_SIGN_FAIL,	//=10
	E_CIPHER_FAIL,	//=11
	E_MD_CHECK,	//=12
	E_DSP_CHECK,	//=13
}ERROR_CODE;


//******************function definitions*****************************//

static inline void ccci_get_region_layout(CCCI_REGION_LAYOUT *layout)
{
	if(get_ap_img_ver() == AP_IMG_2G)
		layout->dsp_region_base = DSP_REGION_BASE_2G;
	else if(get_ap_img_ver() == AP_IMG_3G)
		layout->dsp_region_base = DSP_REGION_BASE_3G;
	else
		layout->dsp_region_base = DSP_REGION_BASE_3G;	
	layout->dsp_region_len = DSP_REGION_LEN;
	
	layout->modem_region_base = MODEM_REGION_BASE;
	layout->modem_region_len = layout->dsp_region_base;
	
	layout->mdif_region_base = MDIF_REGION_BASE;
	layout->mdif_region_len = MDIF_REGION_LEN;
	
	layout->ccif_region_base = CCIF_REGION_BASE;
	layout->ccif_region_len = CCIF_REGION_LEN;
	
	layout->mdcfg_region_base = MDCFG_REGION_BASE;
	layout->mdcfg_region_len = MDCFG_REGION_LEN;

}


//get chip version
static inline unsigned int get_chip_version(void)
{
	//return get_chip_sw_ver();
	return get_chip_eco_ver();
}

static inline char *ccci_get_platform_ver(void)
{
	char *platform_ver = "";

	if(get_chip_version() == CHIP_E1)
		platform_ver = "MT6575_S00";
	else if(get_chip_version() == CHIP_E2)
		platform_ver = "MT6575_S01";
	else if(get_chip_version() == CHIP_E3)
		platform_ver = "MT6575_S02";

	return platform_ver;

}

static inline void ccci_before_modem_start_boot(void)
{

}

static inline void ccci_after_modem_finish_boot(void)
{

}


static inline void ccif_platform_irq_init(int irq)
{
    mt65xx_irq_set_sens(MT6575_AP_CCIF_IRQ_ID, MT65xx_LEVEL_SENSITIVE);	
	mt65xx_irq_set_polarity(MT6575_AP_CCIF_IRQ_ID, MT65xx_POLARITY_LOW);
}

static inline int platform_ready2boot(void)
{
	return 0;
}

#define MD_INFRA_BASE  			0xD10D0000 // Modem side: 0x810D0000
#define MD_RGU_BASE  			0xD10C0000 // Modem side: 0x810C0000

#define CLK_SW_CON0 		(0x00000910)
#define CLK_SW_CON1 		(0x00000914)
#define CLK_SW_CON2 		(0x00000918)
#define BOOT_JUMP_ADDR 		(0x00000980)


void ungate_md(void);

void gate_md(void);

static inline void platform_set_runtime_data(struct modem_runtime_t *runtime)
{
	#define PLATFORM_VER "MT6575E1" 
	#define DRIVER_VER	 0x20110118

	CCCI_WRITEL(&runtime->Platform_L, 0x3536544D); // "MT65"
    CCCI_WRITEL(&runtime->Platform_H, 0x31453537); // "75E1"
	CCCI_WRITEL(&runtime->DriverVersion, 0x20110118);
}


/*
 * md_wdt_reset: reset modem by WDT SW reset (BUG BUG: tempoary implementation)
 */
static inline int md_wdt_reset(void)
{
	return 0;  //To DO
}


#endif

