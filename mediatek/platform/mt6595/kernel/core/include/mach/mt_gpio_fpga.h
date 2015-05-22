#ifndef _MT_GPIO_FPGA_H_
#define _MT_GPIO_FPGA_H_

#include <asm/io.h>
#include <mach/sync_write.h>

/* FPGA support code */	
#define INFRA_AO_CFG	0xF0001000
#define MT_GPIO_IN_REG  (INFRA_AO_CFG + 0xE80)
#define MT_GPIO_OUT_REG (INFRA_AO_CFG + 0xE84)
#define MT_GPIO_DIR_REG (INFRA_AO_CFG + 0xE88)

#define  MT_GPIO_BASE_START 0
typedef enum GPIO_PIN
{    
    GPIO_UNSUPPORTED = -1,    
	GPIO0 = MT_GPIO_BASE_START,
    GPIO1  , GPIO2  , GPIO3  , GPIO4  , GPIO5  , GPIO6  , GPIO7  , 
    MT_GPIO_BASE_MAX
}GPIO_PIN;    

/* This REG structure is FAKE, just for compile pass with mt_gpio_debug.c */
typedef struct {        
    unsigned short val;        
    unsigned short _align1;
    unsigned short set;
    unsigned short _align2;
    unsigned short rst;
    unsigned short _align3[3];
} VAL_REGS;
typedef struct {
    VAL_REGS    dir[11];            /*0x0000 ~ 0x00AF: 176 bytes*/
    VAL_REGS    pullen[11];         /*0x0100 ~ 0x01AF: 176 bytes*/
    VAL_REGS    pullsel[11];        /*0x0200 ~ 0x02AF: 176 bytes*/
    VAL_REGS    dout[11];           /*0x0400 ~ 0x04AF: 176 bytes*/
    VAL_REGS    din[11];            /*0x0500 ~ 0x05AF: 176 bytes*/
    VAL_REGS    mode[36];           /*0x0600 ~ 0x083F: 576 bytes*/  
	VAL_REGS    ies[2];            	/*0x0900 ~ 0x091F: 	32 bytes*/
} GPIO_REGS;
/*----------------------------------------------------------------------------*/

#endif
