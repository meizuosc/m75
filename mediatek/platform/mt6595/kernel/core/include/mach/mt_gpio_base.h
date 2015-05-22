#ifndef _MT_GPIO_BASE_H_
#define _MT_GPIO_BASE_H_

#include <mach/sync_write.h>

#define GPIO_WR32(addr, data)   mt65xx_reg_sync_writel(data, addr)
#define GPIO_RD32(addr)         __raw_readl(addr)
//#define GPIO_SET_BITS(BIT,REG)   ((*(volatile unsigned long*)(REG)) = (unsigned long)(BIT))
//#define GPIO_CLR_BITS(BIT,REG)   ((*(volatile unsigned long*)(REG)) &= ~((unsigned long)(BIT)))
#define GPIO_SET_BITS(BIT,REG)   GPIO_WR32(REG, (unsigned long)(BIT))
#define GPIO_CLR_BITS(BIT,REG)   GPIO_WR32(REG,GPIO_RD32(REG) & ~((unsigned long)(BIT)))

#define MAX_GPIO_REG_BITS      16
#define MAX_GPIO_MODE_PER_REG  5
#define GPIO_MODE_BITS         3 

#define MT_GPIO_BASE_START 0
typedef enum GPIO_PIN
{    
    GPIO_UNSUPPORTED = -1,    
	GPIO0 = MT_GPIO_BASE_START,
    GPIO1  , GPIO2  , GPIO3  , GPIO4  , GPIO5  , GPIO6  , GPIO7  , 
    GPIO8  , GPIO9  , GPIO10 , GPIO11 , GPIO12 , GPIO13 , GPIO14 , GPIO15 , 
    GPIO16 , GPIO17 , GPIO18 , GPIO19 , GPIO20 , GPIO21 , GPIO22 , GPIO23 , 
    GPIO24 , GPIO25 , GPIO26 , GPIO27 , GPIO28 , GPIO29 , GPIO30 , GPIO31 , 
    GPIO32 , GPIO33 , GPIO34 , GPIO35 , GPIO36 , GPIO37 , GPIO38 , GPIO39 , 
    GPIO40 , GPIO41 , GPIO42 , GPIO43 , GPIO44 , GPIO45 , GPIO46 , GPIO47 , 
    GPIO48 , GPIO49 , GPIO50 , GPIO51 , GPIO52 , GPIO53 , GPIO54 , GPIO55 , 
    GPIO56 , GPIO57 , GPIO58 , GPIO59 , GPIO60 , GPIO61 , GPIO62 , GPIO63 , 
    GPIO64 , GPIO65 , GPIO66 , GPIO67 , GPIO68 , GPIO69 , GPIO70 , GPIO71 , 
    GPIO72 , GPIO73 , GPIO74 , GPIO75 , GPIO76 , GPIO77 , GPIO78 , GPIO79 , 
    GPIO80 , GPIO81 , GPIO82 , GPIO83 , GPIO84 , GPIO85 , GPIO86 , GPIO87 , 
    GPIO88 , GPIO89 , GPIO90 , GPIO91 , GPIO92 , GPIO93 , GPIO94 , GPIO95 , 
    GPIO96 , GPIO97 , GPIO98 , GPIO99 , GPIO100, GPIO101, GPIO102, GPIO103, 
    GPIO104, GPIO105, GPIO106, GPIO107, GPIO108, GPIO109, GPIO110, GPIO111, 
    GPIO112, GPIO113, GPIO114, GPIO115, GPIO116, GPIO117, GPIO118, GPIO119, 
    GPIO120, GPIO121, GPIO122, GPIO123, GPIO124, GPIO125, GPIO126, GPIO127, 
    GPIO128, GPIO129, GPIO130, GPIO131, GPIO132, GPIO133, GPIO134, GPIO135, 
    GPIO136, GPIO137, GPIO138, GPIO139, GPIO140, GPIO141, GPIO142, GPIO143, 
    GPIO144, GPIO145, GPIO146, GPIO147, GPIO148, GPIO149, GPIO150, GPIO151, 
    GPIO152, GPIO153, GPIO154, GPIO155, GPIO156, GPIO157, GPIO158, GPIO159, 
    GPIO160, GPIO161, GPIO162, GPIO163, GPIO164, GPIO165, GPIO166, GPIO167, 
    GPIO168, GPIO169, GPIO170, GPIO171, GPIO172, GPIO173, GPIO174, GPIO175,
	GPIO176, GPIO177, GPIO178, GPIO179, GPIO180, GPIO181, GPIO182, GPIO183,
	GPIO184, GPIO185, GPIO186, GPIO187, GPIO188, GPIO189, GPIO190, GPIO191,
	GPIO192, GPIO193, GPIO194, GPIO195, GPIO196, GPIO197, GPIO198, GPIO199,
	GPIO200, GPIO201, GPIO202, GPIO203, GPIO204, GPIO205, GPIO206, GPIO207,
	GPIO208, GPIO209, GPIO210, GPIO211, GPIO212,
    MT_GPIO_BASE_MAX
}GPIO_PIN;

/*----------------------------------------------------------------------------*/
typedef struct {        
    unsigned short val;        
    unsigned short _align1;
    unsigned short set;
    unsigned short _align2;
    unsigned short rst;
    unsigned short _align3[3];
} VAL_REGS;
/*----------------------------------------------------------------------------*/
typedef struct {
    VAL_REGS    dir[14];            /*0x0000 ~ 0x00DF: 224 bytes*/
    unsigned char       rsv00[32];  /*0x00E0 ~ 0x00FF: 	32 bytes*/
    VAL_REGS    pullen[14];         /*0x0100 ~ 0x01DF: 224 bytes*/
    unsigned char       rsv01[32];  /*0x01E0 ~ 0x01FF: 	32 bytes*/
    VAL_REGS    pullsel[14];        /*0x0200 ~ 0x02DF: 224 bytes*/
    unsigned char       rsv02[32];  /*0x02E0 ~ 0x02FF: 	32 bytes*/
    unsigned char       rsv03[256]; /*0x0300 ~ 0x03FF: 256 bytes*/
    VAL_REGS    dout[14];           /*0x0400 ~ 0x04DF: 224 bytes*/
    unsigned char       rsv04[32];  /*0x04B0 ~ 0x04FF: 	32 bytes*/
    VAL_REGS    din[14];            /*0x0500 ~ 0x05DF: 224 bytes*/
    unsigned char       rsv05[32];	/*0x05E0 ~ 0x05FF: 	32 bytes*/
    VAL_REGS    mode[43];           /*0x0600 ~ 0x08AF: 688 bytes*/  
	unsigned char		rsv06[80];	/*0x08B0 ~ 0x08FF:  80 bytes*/
	VAL_REGS    ies[3];            	/*0x0900 ~ 0x092F: 	48 bytes*/
    VAL_REGS    smt[3];        		/*0x0930 ~ 0x095F: 	48 bytes*/ 
	unsigned char		rsv07[160];	/*0x0960 ~ 0x09FF: 160 bytes*/
	VAL_REGS    tdsel[8];        	/*0x0A00 ~ 0x0A7F: 128 bytes*/ 
	VAL_REGS    rdsel[6];        	/*0x0A80 ~ 0x0ADF:  96 bytes*/ 
	unsigned char		rsv08[32];	/*0x0AE0 ~ 0x0AFF:  32 bytes*/
	VAL_REGS    drv_mode[10];       /*0x0B00 ~ 0x0B9F: 160 bytes*/ 
	unsigned char		rsv09[96];	/*0x0BA0 ~ 0x0BFF:  96 bytes*/
	VAL_REGS    msdc0_ctrl0;        /*0x0C00 ~ 0x0D4F: 336 bytes*/ 
	VAL_REGS    msdc0_ctrl1;        
	VAL_REGS    msdc0_ctrl2;        
	VAL_REGS    msdc0_ctrl5;        
	VAL_REGS    msdc1_ctrl0;        
	VAL_REGS    msdc1_ctrl1;        
	VAL_REGS    msdc1_ctrl2;        
	VAL_REGS    msdc1_ctrl4;        
	VAL_REGS    msdc2_ctrl0;        
	VAL_REGS    msdc2_ctrl1;        
	VAL_REGS    msdc2_ctrl2;        
	VAL_REGS    msdc2_ctrl4;        
	VAL_REGS    msdc3_ctrl0;        
	VAL_REGS    msdc3_ctrl1;        
	VAL_REGS    msdc3_ctrl2;        
	VAL_REGS    msdc3_ctrl4;        
	VAL_REGS    msdc0_ctrl3;        
	VAL_REGS    msdc0_ctrl4;        
	VAL_REGS    msdc1_ctrl3;        
	VAL_REGS    msdc2_ctrl3;        
	VAL_REGS    msdc3_ctrl3;        
	VAL_REGS    dpi_ctrl[4];        /*0x0D50 ~ 0x0D8F: 	64 bytes*/
	unsigned char		rsv10[48];	/*0x0D90 ~ 0x0DBF:  48 bytes*/
	VAL_REGS    exmd_ctrl[1];       /*0x0DC0 ~ 0x0DCF: 	16 bytes*/
	VAL_REGS    bpi_ctrl[1];       	/*0x0DD0 ~ 0x0DDF: 	16 bytes*/
	unsigned char		rsv11[32];	/*0x0D90 ~ 0x0DBF:  48 bytes*/
	VAL_REGS    kpad_ctrl[2];       /*0x0E00 ~ 0x0E1F: 	32 bytes*/
	VAL_REGS    sim_ctrl[4];        /*0x0E20 ~ 0x0E5F: 	64 bytes*/
} GPIO_REGS;

#endif //_MT_GPIO_BASE_H_
