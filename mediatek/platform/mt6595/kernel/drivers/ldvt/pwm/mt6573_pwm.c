/*******************************************************************************
* mt6573_pwm.c PWM Drvier                                                     
*                                                                                             
* Copyright (c) 2010, Media Teck.inc                                           
*                                                                             
* This program is free software; you can redistribute it and/or modify it     
* under the terms and conditions of the GNU General Public Licence,            
* version 2, as publish by the Free Software Foundation.                       
*                                                                              
* This program is distributed and in hope it will be useful, but WITHOUT       
* ANY WARRNTY; without even the implied warranty of MERCHANTABITLITY or        
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for     
* more details.                                                                
*                                                                              
*                                                                              
********************************************************************************
* Author : cindy zhang (cindy.zhang@mediatek.com)                              
********************************************************************************
*/
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <generated/autoconf.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/delay.h>

#include <linux/types.h>
#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>
#include <asm/atomic.h>
#include <asm/uaccess.h>
#include <asm/io.h>

//#include <mach/mt6573_pwm.h>  
#include "mt6573_pwm.h"
#include <mach/mt6573_typedefs.h>
#include <mach/mt6573_pll.h>
#include <mach/mt6573_gpio.h>

#ifdef PWM_DEBUG
	#define PWMDBG(fmt, args ...) printk(KERN_INFO "pwm %5d: " fmt, __LINE__,##args)
#else
	#define PWMDBG(fmt, args ...)
#endif

#define PWMMSG(fmt, args ...)  printk(KERN_INFO fmt, ##args)

#define PWM_DEVICE "mt6573-pwm"

static U32 pwm_status=0; 
static U32 camer_on =0;

enum {                                                                                     
	PWM_CON,                                                                              
	PWM_HDURATION,                                                                        
	PWM_LDURATION,                                                                        
	PWM_GDURATION,                                                                        
	PWM_BUF0_BASE_ADDR,                                                                   
	PWM_BUF0_SIZE,                                                                        
	PWM_BUF1_BASE_ADDR,                                                                   
	PWM_BUF1_SIZE,                                                                        
	PWM_SEND_DATA0,                                                                       
	PWM_SEND_DATA1,                                                                       
	PWM_WAVE_NUM,                                                                         
	PWM_DATA_WIDTH,      //only PWM1, PWM2, PWM3, PWM7 have such a register for old mode  
	PWM_THRESH,          //only PWM1, PWM2, PWM3, PWM7 have such a register for old mode  
	PWM_SEND_WAVENUM,                                                                     
	PWM_VALID                                                                             
}PWM_REG_OFF;

U32 PWM_register[PWM_NUM]={                                                                                                                                                     
	(PWM_BASE+0x0010),     //PWM1 REGISTER BASE,   15 registers  
	(PWM_BASE+0x0050),     //PWM2 register base    15 registers                
	(PWM_BASE+0x0090),     //PWM3 register base    15 registers                
	(PWM_BASE+0x00d0),     //PWM4 register base    13 registers                
	(PWM_BASE+0x0110),     //PWM5 register base    13 registers                
	(PWM_BASE+0x0150),     //PWM6 register base    13 registers                
	(PWM_BASE+0x0190)      //PWM7 register base    15 registers                
}; 

struct pwm_device {
	const char      *name;
	atomic_t        ref;
	dev_t           devno;
	spinlock_t      lock;
	struct device   dev;
	struct miscdevice *miscdev;
};	

static struct pwm_device pwm_dat = {
	.name = PWM_DEVICE,
	.ref = ATOMIC_INIT(0),
	.lock = SPIN_LOCK_UNLOCKED
};

static struct pwm_device *pwm_dev = &pwm_dat;

void mt_power_on(U32 pwm_no)
{
	if ((pwm_status & 0x7f) == 0) {
		hwEnableClock ( MT65XX_PDN_PERI_PWM, "PWM");  //enable clock 
		PWMDBG("hwEnableClock PWM\n");
	}
	
	if ( pwm_no == PWM2) {
		if ( ( camer_on & ( 1 << PWM2 ) ) == 0 ) {
			hwPowerOn(MT65XX_POWER_LDO_VCAMA, VOL_2800, "_PWM2");
			camer_on |= ( 1 << PWM2 );
		}
	}

	pwm_status |= (1 << pwm_no);
}

void mt_power_off (U32 pwm_no)
{

	pwm_status &= ~(1 << pwm_no );
	
	if ( (pwm_status & 0x7f) ==0 ) {
		hwDisableClock ( MT65XX_PDN_PERI_PWM, "PWM" );  //disable clock to save power
		PWMDBG("hwDisableClock PWM\n");
	}

	if ( pwm_no == PWM2) {
		if ( (camer_on & (1<<PWM2)) == 1 ) {
			hwPowerDown(MT65XX_POWER_LDO_VCAMA, "_PWM2");
			camer_on &= ~(1 << PWM2);
		}
		PWMDBG("hwPowerDown PWM2 \n");
	}
}

/*******************************************************
*   Set PWM_ENABLE register bit to enable pwm1~pwm7
*
********************************************************/
S32 mt_set_pwm_enable(U32 pwm_no) 
{
	unsigned long flags;

	struct pwm_device *dev = pwm_dev;

	if ( !dev ) {
		PWMDBG("dev is not valid!\n");
		return -EINVALID;
	}

	if ( pwm_no >= PWM_MAX ) {
		PWMDBG ( "pwm number is not between PWM1~PWM7(0~6)\n" );
		return -EEXCESSPWMNO;
	} 

	spin_lock_irqsave ( &dev->lock,flags );
	SETREG32(PWM_ENABLE, 1 << pwm_no);
	spin_unlock_irqrestore ( &dev->lock, flags );

	return RSUCCESS;
}


/*******************************************************/
S32 mt_set_pwm_disable ( U32 pwm_no )  
{
	unsigned long flags;
	struct pwm_device *dev = pwm_dev;

	if ( !dev ) {
		PWMDBG ( "dev is not valid\n" );
		return -EINVALID;
	}

	if ( pwm_no >= PWM_MAX ) {
		PWMDBG ( "pwm number is not between PWM1~PWM7 (0~6)\n" );
		return -EEXCESSPWMNO;
	}

	spin_lock_irqsave ( &dev->lock, flags );	
	CLRREG32 ( PWM_ENABLE, 1 << pwm_no );
	spin_unlock_irqrestore ( &dev->lock, flags );

	mdelay(1);

	return RSUCCESS;
}

/********************************************************/
S32 mt_get_pwm_enable(U32 pwm_no)
{
	unsigned long flags;

	int en;

	struct pwm_device *dev = pwm_dev;

	if (!dev ) {
		PWMDBG("dev is not valid.\n");
		return -EINVALID;
	}

	if ( pwm_no >= PWM_MAX ) {
		PWMDBG("pwm number is not between PWM1~PWM7.\n");
		return -EEXCESSPWMNO;
	}

	spin_lock_irqsave( &dev->lock, flags );
	en = INREG32(PWM_ENABLE );
	en &= 1 << pwm_no;
	en >>= pwm_no;
	spin_unlock_irqrestore( &dev->lock, flags );

	return en;
}

int mt_get_pwm_mode(U32 pwm_no)
{
	unsigned long flags;
	U32 reg_val, reg_con;
	int mode;
	u32 con_mode, con_src;
	
	struct pwm_device *dev = pwm_dev;

	if (!dev) {
		PWMDBG("dev is not valid.\n");
		return -EINVAL;
	}

	reg_con = PWM_register[pwm_no] + 4*PWM_CON;
	spin_lock_irqsave( &dev->lock,flags);
	reg_val = INREG32(reg_con);
	if (reg_val & PWM_CON_OLD_MODE_MASK) {
		mode = 0;
	}else {
		reg_val = INREG32(PWM_ENABLE);
		if (reg_val & (1 << PWM_ENABLE_SEQ_OFFSET)) {
			mode = 4;
		}else {
			reg_val = INREG32(reg_con);
			con_mode = reg_val & PWM_CON_MODE_MASK;
			con_src = reg_val & PWM_CON_SRCSEL_MASK;
			if ((con_mode == PERIOD)&& (con_src == FIFO)) {
				mode = 1;
			}else if ((con_mode == RAND) && (con_src == MEMORY)) {
				mode = 3;
			}else if ((con_mode == PERIOD) && (con_src == MEMORY) )  {
				mode = 2;
			}else {
				PWMDBG("mode is invalid.\n");
				PWMDBG("PWM_CON_MODE is :0x%x, PWM_CON_SRCSEL is: 0x%x\n", 
					con_mode, con_src);
			}
		}
	}

	spin_unlock_irqrestore( &dev->lock, flags);

	return mode;
		
}


void mt_set_pwm_enable_seqmode(void)
{
	unsigned long flags;
	struct pwm_device *dev = pwm_dev;

	if ( !dev ) {
		PWMDBG ( "dev is not valid \n" );
		return ;
    }

	spin_lock_irqsave ( &dev->lock,flags );
	SETREG32 ( PWM_ENABLE, 1 << PWM_ENABLE_SEQ_OFFSET );
	spin_unlock_irqrestore ( &dev->lock, flags );
}

void mt_set_pwm_disable_seqmode(void)
{
	unsigned long flags;
	struct pwm_device *dev = pwm_dev;

	if ( !dev ) {
		PWMDBG ( "dev is not valid\n" );
		return ;
	}

	spin_lock_irqsave ( &dev->lock, flags );
	CLRREG32 ( PWM_ENABLE,1 << PWM_ENABLE_SEQ_OFFSET );
	spin_unlock_irqrestore ( &dev->lock, flags );
}

S32 mt_set_pwm_test_sel(U32 val)  //val as 0 or 1
{
	unsigned long flags;
	struct pwm_device *dev = pwm_dev;

	if ( !dev ) {
		PWMDBG ( "dev is not pwm_dev \n" );
		return -EINVALID;
	}

	spin_lock_irqsave ( &dev->lock, flags );
	if (val == TEST_SEL_TRUE)
		SETREG32 ( PWM_ENABLE, 1 << PWM_ENABLE_TEST_SEL_OFFSET );
	else if ( val == TEST_SEL_FALSE )
		CLRREG32 ( PWM_ENABLE, 1 << PWM_ENABLE_TEST_SEL_OFFSET );
	else
		goto err;	
	spin_unlock_irqrestore ( &dev->lock, flags );
	return RSUCCESS;

err:
	spin_unlock_irqrestore ( &dev->lock, flags );
	return -EPARMNOSUPPORT;
}

S32 mt_set_pwm_clk ( U32 pwm_no, U32 clksrc, U32 div )
{
	unsigned long flags;
	U32 reg_con;

	struct pwm_device *dev = pwm_dev;
	if ( !dev ) {
		PWMDBG ( "dev is not valid\n" );
		return -EINVALID;
	}

	if ( pwm_no >= PWM_MAX ) {
		PWMDBG ( "pwm number excesses PWM_MAX \n" );
		return -EEXCESSPWMNO;
	}

	if ( div >= CLK_DIV_MAX ) {
		PWMDBG ("division excesses CLK_DIV_MAX\n");
		return -EPARMNOSUPPORT;
	}

	if (clksrc > CLK_BLOCK_BY_1625_OR_32K) {
		PWMDBG("clksrc excesses CLK_BLOCK_BY_1625_OR_32K\n");
		return -EPARMNOSUPPORT;
	}

	reg_con = PWM_register [pwm_no] + 4* PWM_CON;

	spin_lock_irqsave ( &dev->lock, flags );
	MASKREG32 ( reg_con, PWM_CON_CLKDIV_MASK, div );
	if (clksrc == CLK_BLOCK)
		CLRREG32 ( reg_con, 1 << PWM_CON_CLKSEL_OFFSET );
	else if (clksrc == CLK_BLOCK_BY_1625_OR_32K)
		SETREG32 ( reg_con, 1 << PWM_CON_CLKSEL_OFFSET );
	spin_unlock_irqrestore ( &dev->lock, flags );

	return RSUCCESS;
}

/****************************************************/
S32 mt_get_pwm_clk ( U32 pwm_no )
{
	unsigned long flags;
	S32 clk;
	U32 reg_con, reg_val;

	struct pwm_device *dev = pwm_dev;
	if ( !dev ) {
		PWMDBG ("dev is not valid \n");
		return -EINVALID;
	}

	if ( pwm_no >= PWM_MAX) {
		PWMDBG ( "pwm number excesses PWM_MAX \n" );
		return -EEXCESSPWMNO;
	}

	reg_con = PWM_register[pwm_no] + 4*PWM_CON;

	spin_lock_irqsave ( &dev->lock, flags);
	reg_val = INREG32 (reg_con);
	spin_unlock_irqrestore ( &dev->lock, flags );

	clk = (reg_val & PWM_CON_CLKSEL_MASK) >> PWM_CON_CLKSEL_OFFSET;
	return clk;
}

/*********************************************************/
S32 mt_get_pwm_datawidth (U32 pwm_no)
{
	unsigned long flags;
	U32 reg_data, reg_val;

	struct pwm_device *dev = pwm_dev;
	if (!dev) {
		PWMDBG("dev is not valid.\n");
		return -EINVALID;
	}

	if (pwm_no >= PWM_MAX) {
		PWMDBG("pwm number excesses PWM_MAX.\n");
		return -EEXCESSPWMNO;
	}

	reg_data = PWM_register[pwm_no] + 4*PWM_DATA_WIDTH;
	spin_lock_irqsave ( &dev->lock, flags);
	reg_val = INREG32 (reg_data);
	reg_val &= 0x1fff;
	spin_unlock_irqrestore ( &dev->lock, flags );
	
	return reg_val;
	
}

/***********************************************************/
S32 mt_get_pwm_thresh(U32 pwm_no)
{
	unsigned long flags;
	U32 reg_thresh, reg_val;

	struct pwm_device *dev = pwm_dev;
	if (!dev) {
		PWMDBG("dev is not valid.\n");
		return -EINVALID;
	}

	if (pwm_no >= PWM_MAX) {
		PWMDBG("pwm number excesses PWM_MAX.\n");
		return -EEXCESSPWMNO;
	}

	reg_thresh = PWM_register[pwm_no] + 4*PWM_THRESH;
	spin_lock_irqsave ( &dev->lock, flags);
	reg_val = INREG32 (reg_thresh);
	reg_val &= 0x1fff;
	spin_unlock_irqrestore ( &dev->lock, flags );
	
	return reg_val;
}

/****************************************************/
S32 mt_get_pwm_div ( U32 pwm_no )
{
	unsigned long flags;
	S32 div;
	U32 reg_con, reg_val;

	struct pwm_device *dev = pwm_dev;
	if ( !dev ) {
		PWMDBG ("dev is not valid \n");
		return -EINVALID;
	}

	if ( pwm_no >= PWM_MAX) {
		PWMDBG ( "pwm number excesses PWM_MAX \n" );
		return -EEXCESSPWMNO;
	}

	reg_con = PWM_register[pwm_no] + 4*PWM_CON;

	spin_lock_irqsave ( &dev->lock, flags);
	reg_val = INREG32 (reg_con);
	spin_unlock_irqrestore ( &dev->lock, flags );

	div = (reg_val & PWM_CON_CLKDIV_MASK) >> PWM_CON_CLKDIV_OFFSET;
	return div;
}

/****************************************************/
S32 mt_get_pwm_stpbit ( U32 pwm_no )
{
	unsigned long flags;
	S32 stpbit;
	U32 reg_con, reg_val;

	struct pwm_device *dev = pwm_dev;
	if ( !dev ) {
		PWMDBG ("dev is not valid \n");
		return -EINVALID;
	}

	if ( pwm_no >= PWM_MAX) {
		PWMDBG ( "pwm number excesses PWM_MAX \n" );
		return -EEXCESSPWMNO;
	}

	reg_con = PWM_register[pwm_no] + 4*PWM_CON;

	spin_lock_irqsave ( &dev->lock, flags);
	reg_val = INREG32 (reg_con);
	spin_unlock_irqrestore ( &dev->lock, flags );

	stpbit = (reg_val & PWM_CON_STOP_BITS_MASK) >> PWM_CON_STOP_BITS_OFFSET;
	return stpbit;
}

/****************************************************/
S32 mt_get_pwm_high ( U32 pwm_no )
{
	unsigned long flags;
	S32 high;
	U32 reg_high, reg_val;

	struct pwm_device *dev = pwm_dev;
	if ( !dev ) {
		PWMDBG ("dev is not valid \n");
		return -EINVALID;
	}

	if ( pwm_no >= PWM_MAX) {
		PWMDBG ( "pwm number excesses PWM_MAX \n" );
		return -EEXCESSPWMNO;
	}

	reg_high = PWM_register[pwm_no] + 4*PWM_HDURATION;

	spin_lock_irqsave ( &dev->lock, flags);
	reg_val = INREG32 (reg_high);
	spin_unlock_irqrestore ( &dev->lock, flags );

	high = (reg_val & PWM_HDURATION) ;
	return high;
}

/****************************************************/
S32 mt_get_pwm_low ( U32 pwm_no )
{
	unsigned long flags;
	S32 low;
	U32 reg_low, reg_val;

	struct pwm_device *dev = pwm_dev;
	if ( !dev ) {
		PWMDBG ("dev is not valid \n");
		return -EINVALID;
	}

	if ( pwm_no >= PWM_MAX) {
		PWMDBG ( "pwm number excesses PWM_MAX \n" );
		return -EEXCESSPWMNO;
	}

	reg_low = PWM_register[pwm_no] + 4*PWM_LDURATION;

	spin_lock_irqsave ( &dev->lock, flags);
	reg_val = INREG32 (reg_low);
	spin_unlock_irqrestore ( &dev->lock, flags );

	low = (reg_val & PWM_LDURATION) ;
	return low;
}

/****************************************************/
S32 mt_get_pwm_grd ( U32 pwm_no )
{
	unsigned long flags;
	S32 grd;
	U32 reg_grd, reg_val;

	struct pwm_device *dev = pwm_dev;
	if ( !dev ) {
		PWMDBG ("dev is not valid \n");
		return -EINVALID;
	}

	if ( pwm_no >= PWM_MAX) {
		PWMDBG ( "pwm number excesses PWM_MAX \n" );
		return -EEXCESSPWMNO;
	}

	reg_grd = PWM_register[pwm_no] + 4*PWM_GDURATION;

	spin_lock_irqsave ( &dev->lock, flags);
	reg_val = INREG32 (reg_grd);
	spin_unlock_irqrestore ( &dev->lock, flags );

	grd = (reg_val & PWM_LDURATION) ;
	return grd;
}

/****************************************************/
S32 mt_get_pwm_grdval ( U32 pwm_no )
{
	unsigned long flags;
	S32 grdval;
	U32 reg_con, reg_val;

	struct pwm_device *dev = pwm_dev;
	if ( !dev ) {
		PWMDBG ("dev is not valid \n");
		return -EINVALID;
	}

	if ( pwm_no >= PWM_MAX) {
		PWMDBG ( "pwm number excesses PWM_MAX \n" );
		return -EEXCESSPWMNO;
	}

	reg_con = PWM_register[pwm_no] + 4*PWM_CON;

	spin_lock_irqsave ( &dev->lock, flags);
	reg_val = INREG32 (reg_con);
	spin_unlock_irqrestore ( &dev->lock, flags );

	grdval = (reg_val & PWM_CON_GUARD_VALUE_MASK) >>PWM_CON_GUARD_VALUE_OFFSET;
	return grdval;
}


/****************************************************/
S32 mt_get_pwm_idlval ( U32 pwm_no )
{
	unsigned long flags;
	S32 idlval;
	U32 reg_con, reg_val;

	struct pwm_device *dev = pwm_dev;
	if ( !dev ) {
		PWMDBG ("dev is not valid \n");
		return -EINVALID;
	}

	if ( pwm_no >= PWM_MAX) {
		PWMDBG ( "pwm number excesses PWM_MAX \n" );
		return -EEXCESSPWMNO;
	}

	reg_con = PWM_register[pwm_no] + 4*PWM_CON;

	spin_lock_irqsave ( &dev->lock, flags);
	reg_val = INREG32 (reg_con);
	spin_unlock_irqrestore ( &dev->lock, flags );

	idlval = (reg_val & PWM_CON_IDLE_VALUE_MASK) >>PWM_CON_IDLE_VALUE_OFFSET;
	return idlval;
}

/******************************************
* Set PWM_CON register data source
* pwm_no: pwm1~pwm7(0~6)
*val: 0 is fifo mode
*       1 is memory mode
*******************************************/

S32 mt_set_pwm_con_datasrc ( U32 pwm_no, U32 val )
{
	unsigned long flags;
	U32 reg_con;

	struct pwm_device *dev = pwm_dev;
	if ( !dev ) {
		PWMDBG (" pwm deivce doesn't exist\n");
		return -EINVALID;
	}
		
	if ( pwm_no >= PWM_MAX ) {
		PWMDBG ("pwm number excesses PWM_MAX \n");
		return -EEXCESSPWMNO;
	}
	
	reg_con = PWM_register[pwm_no] + 4*PWM_CON;

	spin_lock_irqsave ( &dev->lock, flags );
	if ( val == FIFO )
		CLRREG32 ( reg_con, 1 << PWM_CON_SRCSEL_OFFSET );
	else if ( val == MEMORY )
		SETREG32 ( reg_con, 1 << PWM_CON_SRCSEL_OFFSET );
	else 
		goto err;
	spin_unlock_irqrestore ( &dev->lock, flags );
	return RSUCCESS;

err:
	spin_unlock_irqrestore ( &dev->lock, flags );
	return -EPARMNOSUPPORT;
}


/************************************************
*  set the PWM_CON register
* pwm_no : pwm1~pwm7 (0~6)
* val: 0 is period mode
*        1 is random mode
*
***************************************************/
S32 mt_set_pwm_con_mode( U32 pwm_no, U32 val )
{
	unsigned long flags;
	U32 reg_con;

	struct pwm_device *dev = pwm_dev;
	if ( !dev ) {
		PWMDBG ("dev is not valid \n");
		return -EINVALID;
	}
	
	if ( pwm_no >= PWM_MAX ) {
		PWMDBG ("pwm number excesses PWM_MAX\n");
		return -EEXCESSPWMNO;
	}
	
	reg_con = PWM_register[pwm_no] + 4*PWM_CON;

	spin_lock_irqsave ( &dev->lock, flags );
	if ( val == PERIOD )
		CLRREG32 ( reg_con, 1 << PWM_CON_MODE_OFFSET );
	else if (val == RAND)
		SETREG32 ( reg_con, 1 << PWM_CON_MODE_OFFSET );
	else
		goto err;
	spin_unlock_irqrestore ( &dev->lock, flags );
	return RSUCCESS;

err:
	spin_unlock_irqrestore ( &dev->lock, flags );
	return -EPARMNOSUPPORT;
}

/***********************************************
*Set PWM_CON register, idle value bit 
* val: 0 means that  idle state is not put out.
*       1 means that idle state is put out
*
*      IDLE_FALSE: 0
*      IDLE_TRUE: 1
***********************************************/
S32 mt_set_pwm_con_idleval(U32 pwm_no, U16 val)
{
	unsigned long flags;
	U32 reg_con;

	struct pwm_device *dev = pwm_dev;
	if ( ! dev ) {
		PWMDBG ( "dev is not valid \n" );
		return -EINVALID;
	}

	if ( pwm_no >= PWM_MAX ) {
		PWMDBG ( "pwm number excesses PWM_MAX \n" );
		return -EEXCESSPWMNO;
	}
	
	reg_con = PWM_register[pwm_no] + 4*PWM_CON;

	spin_lock_irqsave ( &dev->lock, flags );
	if ( val == IDLE_TRUE )
		SETREG32 ( reg_con,1 << PWM_CON_IDLE_VALUE_OFFSET );
	else if ( val == IDLE_FALSE )
		CLRREG32 ( reg_con, 1 << PWM_CON_IDLE_VALUE_OFFSET );
	else 
		goto err;
	
	spin_unlock_irqrestore ( &dev->lock, flags );
	return RSUCCESS;

err:
	spin_unlock_irqrestore ( &dev->lock, flags );
	return -EPARMNOSUPPORT;
}

/*********************************************
* Set PWM_CON register guardvalue bit
*  val: 0 means guard state is not put out.
*        1 mens guard state is put out.
*
*    GUARD_FALSE: 0
*    GUARD_TRUE: 1
**********************************************/
S32 mt_set_pwm_con_guardval(U32 pwm_no, U16 val)
{
	unsigned long flags;
	U32 reg_con;

	struct pwm_device *dev = pwm_dev;
	if ( !dev ) {
		PWMDBG ( "dev is not valid\n" );
		return -EINVALID;
	}

	if ( pwm_no >= PWM_MAX ) {
		PWMDBG ("pwm number excesses PWM_MAX \n");
		return -EEXCESSPWMNO;
	}
	
	reg_con = PWM_register[pwm_no] + 4*PWM_CON;

	spin_lock_irqsave ( &dev->lock, flags );
	if ( val == GUARD_TRUE )
		SETREG32 ( reg_con, 1 << PWM_CON_GUARD_VALUE_OFFSET );
	else if ( val == GUARD_FALSE )
		CLRREG32 ( reg_con, 1 << PWM_CON_GUARD_VALUE_OFFSET );
	else 
		goto err;
	
	spin_unlock_irqrestore ( &dev->lock, flags );
	return RSUCCESS;

err:
	spin_unlock_irqrestore ( &dev->lock, flags );
	return -EPARMNOSUPPORT;
}


/*************************************************
* Set PWM_CON register stopbits
*stop bits should be less then 0x3f
*
**************************************************/
S32 mt_set_pwm_con_stpbit(U32 pwm_no, U32 stpbit, U32 srcsel )
{
	unsigned long flags;
	U32 reg_con;
	
	struct pwm_device *dev = pwm_dev;
	if ( !dev ) {
		PWMDBG ( "dev is not valid \n" );
		return -EINVALID;
	}

	if ( pwm_no >= PWM_MAX ) {
		PWMDBG ( "pwm number excesses PWM_MAX \n" );
		return -EEXCESSPWMNO;
	}

	reg_con = PWM_register[pwm_no] + 4*PWM_CON;

	if (srcsel == FIFO) {
		if ( stpbit > 0x3f ) {
			PWMDBG ( "stpbit execesses the most of 0x3f in fifo mode\n" );
			return -EPARMNOSUPPORT;
		}
	}else if (srcsel == MEMORY){
		if ( stpbit > 0x1f) {
			PWMDBG ("stpbit excesses the most of 0x1f in memory mode\n");
			return -EPARMNOSUPPORT;
		}
	}

	spin_lock_irqsave ( &dev->lock, flags );
	if ( srcsel == FIFO )
		MASKREG32 ( reg_con, PWM_CON_STOP_BITS_MASK, stpbit << PWM_CON_STOP_BITS_OFFSET);
	if ( srcsel == MEMORY )
		MASKREG32 ( reg_con, PWM_CON_STOP_BITS_MASK & (0x1f << PWM_CON_STOP_BITS_OFFSET), stpbit << PWM_CON_STOP_BITS_OFFSET);
	spin_unlock_irqrestore ( &dev->lock, flags );

	return RSUCCESS;
}

/*****************************************************
*Set PWM_CON register oldmode bit
* val: 0 means disable oldmode
*        1 means enable oldmode
*
*      OLDMODE_DISABLE: 0
*      OLDMODE_ENABLE: 1
******************************************************/

S32 mt_set_pwm_con_oldmode ( U32 pwm_no, U32 val )
{
	unsigned long flags;
	U32 reg_con;

	struct pwm_device *dev = pwm_dev;
	if ( !dev ) {
		PWMDBG ( "dev is not valid \n" );
		return -EINVALID;
	}

	if ( pwm_no >= PWM_MAX ) {
		PWMDBG ("pwm number excesses PWM_MAX \n");
		return -EEXCESSPWMNO;
	}

	reg_con = PWM_register[pwm_no] + 4*PWM_CON;

	spin_lock_irqsave ( &dev->lock, flags );
	if ( val == OLDMODE_DISABLE )
		CLRREG32 ( reg_con, 1 << PWM_CON_OLD_MODE_OFFSET );
	else if ( val == OLDMODE_ENABLE )
		SETREG32 ( reg_con, 1 << PWM_CON_OLD_MODE_OFFSET );
	else 
		goto err;
	
	spin_unlock_irqrestore ( &dev->lock, flags );
	return RSUCCESS;

err:
	spin_unlock_irqrestore ( &dev->lock, flags );
	return -EPARMNOSUPPORT;
}

/***********************************************************
* Set PWM_HIDURATION register
*
*************************************************************/

S32 mt_set_pwm_HiDur(U32 pwm_no, U16 DurVal)  //only low 16 bits are valid
{
	unsigned long flags;
	U32 reg_HiDur;

	struct pwm_device *dev = pwm_dev;
	if ( !dev ) {
		PWMDBG ( "dev is not valid \n" );
		return -EINVALID;
	}

	if ( pwm_no >= PWM_MAX ) {
		PWMDBG ( "pwm number excesses PWM_MAX\n" );
		return -EEXCESSPWMNO;
	}
	
	reg_HiDur = PWM_register[pwm_no]+4*PWM_HDURATION;

	spin_lock_irqsave ( &dev->lock, flags );
	OUTREG32 ( reg_HiDur, DurVal);	
	spin_unlock_irqrestore ( &dev->lock, flags );

	return RSUCCESS;
}

/************************************************
* Set PWM Low Duration register
*************************************************/
S32 mt_set_pwm_LowDur (U32 pwm_no, U16 DurVal)
{
	unsigned long flags;
	U32 reg_LowDur;
	
	struct pwm_device *dev = pwm_dev;
	if ( !dev ) {
		PWMDBG ( "dev is not valid \n" );
		return -EINVALID;
	}

	if ( pwm_no >= PWM_MAX ) {
		PWMDBG ("pwm number excesses PWM_MAX\n");
		return -EEXCESSPWMNO;
	}

	reg_LowDur = PWM_register[pwm_no] + 4*PWM_LDURATION;

	spin_lock_irqsave ( &dev->lock, flags );
	OUTREG32 ( reg_LowDur, DurVal );
	spin_unlock_irqrestore ( &dev->lock, flags );

	return RSUCCESS;
}

/***************************************************
* Set PWM_GUARDDURATION register
* pwm_no: PWM1~PWM7(0~6)
* DurVal:   the value of guard duration
****************************************************/
S32 mt_set_pwm_GuardDur ( U32 pwm_no, U16 DurVal )
{
	unsigned long flags;
	U32 reg_GuardDur;

	struct pwm_device *dev = pwm_dev;
	if ( !dev ) {
		PWMDBG ( "dev is not valid\n" );
		return -EINVALID;
	}

	if ( pwm_no >= PWM_MAX ) {
		PWMDBG ("pwm number excesses PWM_MAX\n");
		return -EEXCESSPWMNO;
	}

	reg_GuardDur = PWM_register[pwm_no] + 4*PWM_GDURATION;

	spin_lock_irqsave ( &dev->lock, flags );
	OUTREG32 ( reg_GuardDur, DurVal );
	spin_unlock_irqrestore ( &dev->lock, flags );

	return RSUCCESS;
}

/*****************************************************
* Set pwm_buf0_addr register
* pwm_no: pwm1~pwm7 (0~6)
* addr: data address
******************************************************/
S32 mt_set_pwm_buf0_addr (U32 pwm_no, U32 addr )
{
	unsigned long flags;
	U32 reg_buff0_addr;

	struct pwm_device *dev = pwm_dev;
	if ( !dev ) {
		PWMDBG ( "dev is not valid\n" );
		return -EINVALID;
	}

	if ( pwm_no >= PWM_MAX ) {
		PWMDBG ( "pwm number excesses PWM_MAX \n" );
		return -EEXCESSPWMNO;
	}

	reg_buff0_addr = PWM_register[pwm_no] + 4 * PWM_BUF0_BASE_ADDR;

	spin_lock_irqsave ( &dev->lock, flags );
	OUTREG32 ( reg_buff0_addr, addr );
	spin_unlock_irqrestore ( &dev->lock, flags );

	return RSUCCESS;
}

/*****************************************************
* Set pwm_buf0_size register
* pwm_no: pwm1~pwm7 (0~6)
* size: size of data
******************************************************/
S32 mt_set_pwm_buf0_size ( U32 pwm_no, U16 size)
{
	unsigned long flags;
	U32 reg_buff0_size;
	
	struct pwm_device *dev = pwm_dev;
	if ( !dev ) {
		PWMDBG ( "dev is not valid\n" );
		return -EINVALID;
	}

	if ( pwm_no >= PWM_MAX ) {
		PWMDBG ( "pwm number excesses PWM_MAX \n" );
		return -EEXCESSPWMNO;
	}

	reg_buff0_size = PWM_register[pwm_no] + 4* PWM_BUF0_SIZE;

	spin_lock_irqsave ( &dev->lock, flags );
	OUTREG32 ( reg_buff0_size, size );
	spin_unlock_irqrestore ( &dev->lock, flags );

	return RSUCCESS;

}

/*****************************************************
* Set pwm_buf1_addr register
* pwm_no: pwm1~pwm7 (0~6)
* addr: data address
******************************************************/
S32 mt_set_pwm_buf1_addr (U32 pwm_no, U32 addr )
{
	unsigned long flags;
	U32 reg_buff1_addr;
	
	struct pwm_device *dev = pwm_dev;
	if ( !dev ) {
		PWMDBG ( "dev is not valid \n" );
		return -EINVALID;
	}

	if ( pwm_no >= PWM_MAX ) {
		PWMDBG ( "pwm number excesses PWM_MAX \n" );
		return -EEXCESSPWMNO;
	}

	reg_buff1_addr = PWM_register[pwm_no] + 4 * PWM_BUF1_BASE_ADDR;

	spin_lock_irqsave ( &dev->lock, flags );
	OUTREG32 ( reg_buff1_addr, addr );
	spin_unlock_irqrestore ( &dev->lock, flags );

	return RSUCCESS;
}

/*****************************************************
* Set pwm_buf1_size register
* pwm_no: pwm1~pwm7 (0~6)
* size: size of data
******************************************************/
S32 mt_set_pwm_buf1_size ( U32 pwm_no, U16 size)
{
	unsigned long flags;
	U32 reg_buff1_size;

	struct pwm_device *dev = pwm_dev;
	if ( !dev ) {
		PWMDBG ( "dev is not valid\n" );
		return -EINVALID;
	}

	if ( pwm_no >= PWM_MAX ) {
		PWMDBG ( "pwm number excesses PWM_MAX \n" );
		return -EEXCESSPWMNO;
	}

	reg_buff1_size = PWM_register[pwm_no] + 4* PWM_BUF1_SIZE;

	spin_lock_irqsave ( &dev->lock, flags );
	OUTREG32 ( reg_buff1_size, size );
	spin_unlock_irqrestore ( &dev->lock, flags );

	return RSUCCESS;

}

/*****************************************************
* Set pwm_send_data0 register
* pwm_no: pwm1~pwm7 (0~6)
* data: the data in the register
******************************************************/
S32 mt_set_pwm_send_data0 ( U32 pwm_no, U32 data )
{
	unsigned long flags;
	U32 reg_data0;

	struct pwm_device *dev = pwm_dev;
	if ( !dev ) {
		PWMDBG ( "dev is not valid\n" );
		return -EINVALID;
	}

	if ( pwm_no >= PWM_MAX ) {
		PWMDBG ( "pwm number excesses PWM_MAX \n" );
		return -EEXCESSPWMNO;
	}

	reg_data0 = PWM_register[pwm_no] + 4 * PWM_SEND_DATA0;

	spin_lock_irqsave ( &dev->lock, flags );
	OUTREG32 ( reg_data0, data );
	spin_unlock_irqrestore ( &dev->lock, flags );

	return RSUCCESS;
}

/*****************************************************
* Set pwm_send_data1 register
* pwm_no: pwm1~pwm7 (0~6)
* data: the data in the register
******************************************************/
S32 mt_set_pwm_send_data1 ( U32 pwm_no, U32 data )
{
	unsigned long flags;
	U32 reg_data1;

	struct pwm_device *dev = pwm_dev; 
	if ( !dev ) {
		PWMDBG ( "dev is not valid \n" );
		return -EINVALID;
	}

	if ( pwm_no >= PWM_MAX ) {
		PWMDBG ( "pwm number excesses PWM_MAX \n" );
		return -EEXCESSPWMNO;
	}

	reg_data1 = PWM_register[pwm_no] + 4 * PWM_SEND_DATA1;

	spin_lock_irqsave ( &dev->lock, flags );
	OUTREG32 ( reg_data1, data );
	spin_unlock_irqrestore ( &dev->lock, flags );

	return RSUCCESS;
}

/*****************************************************
* Set pwm_wave_num register
* pwm_no: pwm1~pwm7 (0~6)
* num:the wave number
******************************************************/
S32 mt_set_pwm_wave_num ( U32 pwm_no, U16 num )
{
	unsigned long flags;
	U32 reg_wave_num;

	struct pwm_device *dev = pwm_dev;
	if ( !dev ) {
		PWMDBG ("dev is not valid\n");
		return -EINVALID;
	}

	if ( pwm_no >= PWM_MAX ) {
		PWMDBG ( "pwm number excesses PWM_MAX\n" );
		return -EEXCESSPWMNO;
	}

	reg_wave_num = PWM_register[pwm_no] + 4 * PWM_WAVE_NUM;

	spin_lock_irqsave ( &dev->lock, flags );
	OUTREG32 ( reg_wave_num, num );
	spin_unlock_irqrestore ( &dev->lock, flags );

	return RSUCCESS;
}

/*****************************************************
* Set pwm_data_width register. 
* This is only for old mode
* pwm_no: pwm1~pwm7 (0~6)
* width: set the guard value in the old mode
******************************************************/
S32 mt_set_pwm_data_width ( U32 pwm_no, U16 width )
{
	unsigned long flags;
	U32 reg_data_width;

	struct pwm_device *dev = pwm_dev;
	if ( !dev ) {
		PWMDBG ( "dev is not valid\n" );
		return -EINVALID;
	}

	if ( pwm_no >= PWM_MAX ) {
		PWMDBG ( "pwm number excesses PWM_MAX\n" );
		return -EEXCESSPWMNO;
	}

	reg_data_width = PWM_register[pwm_no] + 4 * PWM_DATA_WIDTH;

	spin_lock_irqsave ( &dev->lock, flags );
	OUTREG32 ( reg_data_width, width );
	spin_unlock_irqrestore ( &dev->lock, flags );

	return RSUCCESS;
}

/*****************************************************
* Set pwm_thresh register
* pwm_no: pwm1~pwm7 (0~6)
* thresh:  the thresh of the wave
******************************************************/
S32 mt_set_pwm_thresh ( U32 pwm_no, U16 thresh )
{
	unsigned long flags;
	U32 reg_thresh;

	struct pwm_device *dev = pwm_dev; 
	if ( !dev ) {
		PWMDBG ( "dev is not valid \n" );
		return -EINVALID;
	}

	if ( pwm_no >= PWM_MAX ) {
		PWMDBG ( " pwm number excesses PWM_MAX \n");
		return -EEXCESSPWMNO;
	}

	reg_thresh = PWM_register[pwm_no] + 4 * PWM_THRESH;

	spin_lock_irqsave ( &dev->lock, flags );
	OUTREG32 ( reg_thresh, thresh );
	spin_unlock_irqrestore ( &dev->lock, flags );

	return RSUCCESS;
}

/*****************************************************
* Set pwm_send_wavenum register
* pwm_no: pwm1~pwm7 (0~6)
*
******************************************************/
S32 mt_get_pwm_send_wavenum ( U32 pwm_no )
{
	unsigned long flags;
	U32 reg_send_wavenum;
	S32 wave_num;

	struct pwm_device *dev = pwm_dev;
	if ( ! dev ) {
		PWMDBG ( "dev is not valid\n" );
		return -EBADADDR;
	}
	
	if ( pwm_no >= PWM_MAX ) {
		PWMDBG ( "pwm number excesses PWM_MAX\n" );
		return -EEXCESSPWMNO;
	}

	if ( (pwm_no <=PWM3)||(pwm_no == PWM7) )
		reg_send_wavenum = PWM_register[pwm_no] + 4 * PWM_SEND_WAVENUM;
	else
		reg_send_wavenum = PWM_register[pwm_no] + 4 * (PWM_SEND_WAVENUM - 2);  //pwm4,pwm5,pwm6 has no data width and thresh register

	spin_lock_irqsave ( &dev->lock, flags );
	wave_num = INREG32 ( reg_send_wavenum );
	spin_unlock_irqrestore ( &dev->lock, flags );

	return wave_num;
}

/*****************************************************
* Set pwm_send_data1 register
* pwm_no: pwm1~pwm7 (0~6)
* buf_valid_bit: 
* for buf0: bit0 and bit1 should be set 1. 
* for buf1: bit2 and bit3 should be set 1.
******************************************************/
S32 mt_set_pwm_valid ( U32 pwm_no, U32 buf_valid_bit )   //set 0  for BUF0 bit or set 1 for BUF1 bit
{
	unsigned long flags;
	U32 reg_valid;

	struct pwm_device *dev = pwm_dev;
	if ( !dev ) {
		PWMDBG ("dev is not valid\n");
		return -EINVALID;
	}

	if ( pwm_no >= PWM_MAX ) {
		PWMDBG ( "pwm number excesses PWM_MAX\n" );
		return -EEXCESSPWMNO;
	}

	if ( !buf_valid_bit>= BUF_EN_MAX) {
		PWMDBG ( "inavlid bit \n" );
		return -EPARMNOSUPPORT;
	}

	if ( (pwm_no <= PWM3)||(pwm_no == PWM7))
		reg_valid = PWM_register[pwm_no] + 4 * PWM_VALID;
	else
		reg_valid = PWM_register[pwm_no] + 4* (PWM_VALID -2);

	spin_lock_irqsave ( &dev->lock, flags );
	SETREG32 ( reg_valid, 1 << buf_valid_bit );
	spin_unlock_irqrestore ( &dev->lock, flags );

	return RSUCCESS;
}

/*************************************************
*  set PWM4_delay when using SEQ mode
*
**************************************************/
S32 mt_set_pwm_delay_duration(U32 pwm_delay_reg, U16 val)
{
	unsigned long flags;
	struct pwm_device *pwmdev = pwm_dev;

	if (!pwmdev) {
		PWMDBG( "device doesn't exist\n" );
		return -EBADADDR;
	}

	spin_lock_irqsave ( &pwmdev->lock, flags );	
	MASKREG32 ( pwm_delay_reg, PWM_DELAY_DURATION_MASK, val );
	spin_unlock_irqrestore ( &pwmdev->lock, flags );

	return RSUCCESS;
}

/*******************************************************
* Set pwm delay clock
* 
*
********************************************************/
S32 mt_set_pwm_delay_clock (U32 pwm_delay_reg, U32 clksrc)
{
	unsigned long flags;
	struct pwm_device *pwmdev = pwm_dev;
	if ( ! pwmdev ) {
		PWMDBG ( "device doesn't exist\n" );
		return -EBADADDR;
	}

	spin_lock_irqsave ( &pwmdev->lock, flags );
	MASKREG32 (pwm_delay_reg, PWM_DELAY_CLK_MASK, clksrc );
	spin_unlock_irqrestore (&pwmdev->lock, flags);

	return RSUCCESS;
}

/*******************************************
* Set intr enable register
* pwm_intr_enable_bit: the intr bit, 
*
*********************************************/
S32 mt_set_intr_enable(U32 pwm_intr_enable_bit)
{
	unsigned long flags;

	struct pwm_device *dev = pwm_dev;
	if ( !dev ) {
		PWMDBG ( "dev is not valid\n" );
		return -EINVALID;
	}
	
	if (pwm_intr_enable_bit >= PWM_INT_ENABLE_BITS_MAX) {
		PWMDBG (" pwm inter enable bit is not right.\n"); 
		return -EEXCESSBITS; 
	}
	
	spin_lock_irqsave ( &dev->lock, flags );
	SETREG32 ( PWM_INT_ENABLE, 1 << pwm_intr_enable_bit );
	spin_lock_irqsave (&dev->lock, flags );

	return RSUCCESS;
}

S32 mt_set_intr_disable(U32 pwm_intr_enable_bit)
{
	unsigned long flags;
	
	struct pwm_device *dev = pwm_dev;
	if ( !dev ) {
		PWMDBG ( "dev is not valid\n" );
		return -EINVALID;
	}
	
	if (pwm_intr_enable_bit >= PWM_INT_ENABLE_BITS_MAX) {
		PWMDBG (" pwm inter enable bit is not right.\n"); 
		return -EEXCESSBITS; 
	}
	
	spin_lock_irqsave ( &dev->lock, flags );
	CLRREG32 ( PWM_INT_ENABLE, 1 << pwm_intr_enable_bit );
	spin_lock_irqsave (&dev->lock, flags );

	return RSUCCESS;
}

/*****************************************************
* Set intr status register
* pwm_no: pwm1~pwm7 (0~6)
* pwm_intr_status_bit
******************************************************/
S32 mt_get_intr_status(U32 pwm_intr_status_bit)
{
	unsigned long flags;
	int ret;

	struct pwm_device *dev = pwm_dev;
	if ( !dev ) {
		PWMDBG ( "dev is not valid\n" );
		return -EINVALID;
	}

	if ( pwm_intr_status_bit >= PWM_INT_STATUS_BITS_MAX ) {
		PWMDBG ( "status bit excesses PWM_INT_STATUS_BITS_MAX\n" );
		return -EEXCESSBITS;
	}
	
	spin_lock_irqsave ( &dev->lock, flags );
	ret = INREG32 ( PWM_INT_STATUS );
	ret = ( ret >> pwm_intr_status_bit ) & 0x01;
	spin_unlock_irqrestore ( &dev->lock, flags );

	return ret;
}

/*****************************************************
* Set intr ack register
* pwm_no: pwm1~pwm7 (0~6)
* pwm_intr_ack_bit
******************************************************/
S32 mt_set_intr_ack ( U32 pwm_intr_ack_bit )
{
	unsigned long flags;
	struct pwm_device *dev = pwm_dev;
	if ( !dev ) {
		PWMDBG ("dev is not valid\n");
		return -EINVALID;
	}

	if ( pwm_intr_ack_bit >= PWM_INT_ACK_BITS_MAX ) {
		PWMDBG ( "ack bit excesses PWM_INT_ACK_BITS_MAX\n" ); 
		return -EEXCESSBITS;
	}

	spin_lock_irqsave ( &dev->lock, flags );
	SETREG32 ( PWM_INT_ACK, 1 << pwm_intr_ack_bit );
	spin_unlock_irqrestore ( &dev->lock, flags );

	return RSUCCESS;
}

/*****************************************************
* Clear intr ack register
* pwm_no: pwm1~pwm7 (0~6)
* pwm_intr_ack_bit
******************************************************/
S32 mt_clr_intr_ack ( U32 pwm_intr_ack_bit )
{
	unsigned long flags;
	struct pwm_device *dev = pwm_dev;
	if ( !dev ) {
		PWMDBG ("dev is not valid\n");
		return -EINVALID;
	}

	if ( pwm_intr_ack_bit >= PWM_INT_ACK_BITS_MAX ) {
		PWMDBG ( "ack bit excesses PWM_INT_ACK_BITS_MAX\n" ); 
		return -EEXCESSBITS;
	}

	spin_lock_irqsave ( &dev->lock, flags );
	CLRREG32 ( PWM_INT_ACK, 1 << pwm_intr_ack_bit );
	spin_unlock_irqrestore ( &dev->lock, flags );

	return RSUCCESS;
}


S32 pwm_set_easy_config ( struct pwm_easy_config *conf)
{

	U32 duty = 0;
	U16 duration = 0;
	U32 data_AllH=0xffffffff;
	U32 data0 = 0;
	U32 data1 = 0;
	
	if ( conf->pwm_no >= PWM_MAX || conf->pwm_no < PWM_MIN ) {
		PWMDBG("pwm number excess PWM_MAX\n");
		return -EEXCESSPWMNO;
	}

	if ((conf->clk_div >= CLK_DIV_MAX) || (conf->clk_div < CLK_DIV_MIN )) {
		PWMDBG ( "PWM clock division invalid\n" );
		return -EINVALID;
	}
	
	if ( ( conf ->clk_src >= PWM_CLK_SRC_INVALID) || (conf->clk_src < PWM_CLK_SRC_MIN) ) {
		PWMDBG ("PWM clock source invalid\n");
		return -EINVALID;
	}

	if  ( conf->duty < 0 ) {
		PWMDBG("duty parameter is invalid\n");
		return -EINVALID;
	}

	PWMDBG("pwm_set_easy_config\n");

	if ( conf->duty == 0 ) {
		mt_set_pwm_disable (conf->pwm_no);
		mt_power_off(conf->pwm_no);
		return RSUCCESS;
	}

	mt_set_gpio_mode(99,1);   //pwm1
	mt_set_gpio_mode(100,1);  //pwm2
	mt_set_gpio_mode(101,1);  //pwm3

	mt_set_gpio_mode(0,4);    //pwm4
	mt_set_gpio_mode(1,4);   //pwm5
	mt_set_gpio_mode(2,4);    //pwm6
	
	duty = conf->duty;
	duration = conf->duration;
	
	switch ( conf->clk_src ) {
		case PWM_CLK_OLD_MODE_BLOCK:
		case PWM_CLK_OLD_MODE_32K:
			if ( duration > 8191 || duration < 0 ) {
				PWMDBG ( "duration invalid parameter\n" );
				return -EPARMNOSUPPORT;
			}
			if ( conf->pwm_no == PWM4 || conf->pwm_no == PWM5 || conf->pwm_no == PWM6 ){
				PWMDBG ( "invalid parameters\n" );
				return -EPARMNOSUPPORT;
			}
			if ( duration < 10 ) 
				duration = 10;
			break;
			
		case PWM_CLK_NEW_MODE_BLOCK:
		case PWM_CLK_NEW_MODE_BLOCK_DIV_BY_1625:
			if ( duration > 65535 || duration < 0 ){
				PWMDBG ("invalid paramters\n");
				return -EPARMNOSUPPORT;
			}
			break;
		default:
			PWMDBG("invalid clock source\n");
			return -EPARMNOSUPPORT;
	}
	
	if ( duty > 100 ) 
		duty = 100;

	if ( duty > 50 ){
		data0 = data_AllH;
		data1 = data_AllH >> ((PWM_NEW_MODE_DUTY_TOTAL_BITS * (100 - duty ))/100 );
	}else {
		data0 = data_AllH >> ((PWM_NEW_MODE_DUTY_TOTAL_BITS * (50 - duty))/100);
		PWMDBG("DATA0 :0x%x\n",data0);
		data1 = 0;
	}

	mt_power_on(conf->pwm_no);
	mt_set_pwm_con_guardval(conf->pwm_no, GUARD_TRUE);

	switch ( conf->clk_src ) {
		case PWM_CLK_OLD_MODE_32K:
			mt_set_pwm_con_oldmode(conf->pwm_no, OLDMODE_ENABLE);
			mt_set_pwm_clk ( conf->pwm_no, CLK_BLOCK_BY_1625_OR_32K, conf->clk_div);
			break;

		case PWM_CLK_OLD_MODE_BLOCK:
			mt_set_pwm_con_oldmode (conf->pwm_no, OLDMODE_ENABLE );
			mt_set_pwm_clk ( conf->pwm_no, CLK_BLOCK, conf->clk_div );
			break;

		case PWM_CLK_NEW_MODE_BLOCK:
			mt_set_pwm_con_oldmode (conf->pwm_no, OLDMODE_DISABLE );
			mt_set_pwm_clk ( conf->pwm_no, CLK_BLOCK , conf->clk_div );
			mt_set_pwm_con_datasrc( conf->pwm_no, FIFO);
			mt_set_pwm_con_stpbit ( conf->pwm_no, 0x3f, FIFO );
			break;

		case PWM_CLK_NEW_MODE_BLOCK_DIV_BY_1625:
			mt_set_pwm_con_oldmode (conf->pwm_no,  OLDMODE_DISABLE );
			mt_set_pwm_clk ( conf->pwm_no, CLK_BLOCK_BY_1625_OR_32K, conf->clk_div );
			mt_set_pwm_con_datasrc( conf->pwm_no, FIFO);
			mt_set_pwm_con_stpbit ( conf->pwm_no, 0x3f, FIFO );
			break;

		default:
			break;
		}
	PWMDBG("The duration is:%x\n", duration);
	PWMDBG("The data0 is:%x\n",data0);
	PWMDBG("The data1 is:%x\n",data1);
	mt_set_pwm_HiDur ( conf->pwm_no, duration );
	mt_set_pwm_LowDur (conf->pwm_no, duration );
	mt_set_pwm_GuardDur (conf->pwm_no, 0 );
	mt_set_pwm_buf0_addr (conf->pwm_no, 0 );
	mt_set_pwm_buf0_size( conf->pwm_no, 0 );
	mt_set_pwm_buf1_addr (conf->pwm_no, 0 );
	mt_set_pwm_buf1_size (conf->pwm_no, 0 );
	mt_set_pwm_send_data0 (conf->pwm_no, data0 );
	mt_set_pwm_send_data1 (conf->pwm_no, data1 );
	mt_set_pwm_wave_num (conf->pwm_no, 0 );

	if ( conf->pwm_no <= PWM3 || conf->pwm_no == PWM7)
	{
		mt_set_pwm_data_width (conf->pwm_no, duration );
		mt_set_pwm_thresh ( conf->pwm_no, (( duration * conf->duty)/100));
		mt_set_pwm_valid (conf->pwm_no, BUF0_EN_VALID );
		mt_set_pwm_valid ( conf->pwm_no, BUF1_EN_VALID );
		
	}

	mt_set_pwm_enable ( conf->pwm_no );
	PWMDBG("mt_set_pwm_enable\n");

	return RSUCCESS;
	
}

EXPORT_SYMBOL(pwm_set_easy_config);
	
S32 pwm_set_spec_config(struct pwm_spec_config *conf)
{

	if ( conf->pwm_no >= PWM_MAX ) {
		PWMDBG("pwm number excess PWM_MAX\n");
		return -EEXCESSPWMNO;
	}

       if ( ( conf->mode >= PWM_MODE_INVALID )||(conf->mode < PWM_MODE_MIN )) {
	   	PWMDBG ( "PWM mode invalid \n" );
		return -EINVALID;
       }

	if ( ( conf ->clk_src >= PWM_CLK_SRC_INVALID) || (conf->clk_src < PWM_CLK_SRC_MIN) ) {
		PWMDBG ("PWM clock source invalid\n");
		return -EINVALID;
	}

	if ((conf->clk_div >= CLK_DIV_MAX) || (conf->clk_div < CLK_DIV_MIN )) {
		PWMDBG ( "PWM clock division invalid\n" );
		return -EINVALID;
	}

	if ( (( conf->pwm_no == PWM4 || conf->pwm_no == PWM5||conf->pwm_no == PWM6 ) 
		&& conf->mode == PWM_MODE_OLD)  
		|| (conf->mode == PWM_MODE_OLD &&
			(conf->clk_src == PWM_CLK_NEW_MODE_BLOCK|| conf->clk_src == PWM_CLK_NEW_MODE_BLOCK_DIV_BY_1625)) 
		||(conf->mode != PWM_MODE_OLD &&
			(conf->clk_src == PWM_CLK_OLD_MODE_32K || conf->clk_src == PWM_CLK_OLD_MODE_BLOCK)) ) {

		PWMDBG ( "parameters match error\n" );
		return -ERROR;
	}
	mt_set_gpio_mode(99,1);   //pwm1
	mt_set_gpio_mode(100,1);  //pwm2
	mt_set_gpio_mode(101,1);  //pwm3

	mt_set_gpio_mode(0,4);    //pwm4
	mt_set_gpio_mode(1,4);   //pwm5
	mt_set_gpio_mode(2,4);    //pwm6

	mt_power_on(conf->pwm_no);

	switch (conf->mode ) {
		case PWM_MODE_OLD:
			PWMDBG("PWM_MODE_OLD\n");
			mt_set_pwm_con_oldmode(conf->pwm_no, OLDMODE_ENABLE);
			mt_set_pwm_con_idleval(conf->pwm_no, conf->PWM_MODE_OLD_REGS.IDLE_VALUE);
			mt_set_pwm_con_guardval (conf->pwm_no, conf->PWM_MODE_OLD_REGS.GUARD_VALUE);
			mt_set_pwm_GuardDur (conf->pwm_no, conf->PWM_MODE_OLD_REGS.GDURATION);
			mt_set_pwm_wave_num(conf->pwm_no, conf->PWM_MODE_OLD_REGS.WAVE_NUM);
			mt_set_pwm_data_width(conf->pwm_no, conf->PWM_MODE_OLD_REGS.DATA_WIDTH);
			mt_set_pwm_thresh(conf->pwm_no, conf->PWM_MODE_OLD_REGS.THRESH);
			PWMDBG ("PWM set old mode finish\n");
			break;
		case PWM_MODE_FIFO:
			PWMDBG("PWM_MODE_FIFO\n");
			mt_set_pwm_con_oldmode(conf->pwm_no, OLDMODE_DISABLE);
			mt_set_pwm_con_datasrc(conf->pwm_no, FIFO);
			mt_set_pwm_con_mode (conf->pwm_no, PERIOD);
			mt_set_pwm_con_idleval(conf->pwm_no, conf->PWM_MODE_FIFO_REGS.IDLE_VALUE);
			mt_set_pwm_con_guardval (conf->pwm_no, conf->PWM_MODE_FIFO_REGS.GUARD_VALUE);
			mt_set_pwm_HiDur (conf->pwm_no, conf->PWM_MODE_FIFO_REGS.HDURATION);
			mt_set_pwm_LowDur (conf->pwm_no, conf->PWM_MODE_FIFO_REGS.LDURATION);
			mt_set_pwm_GuardDur (conf->pwm_no, conf->PWM_MODE_FIFO_REGS.GDURATION);
			mt_set_pwm_send_data0 (conf->pwm_no, conf->PWM_MODE_FIFO_REGS.SEND_DATA0);
			mt_set_pwm_send_data1 (conf->pwm_no, conf->PWM_MODE_FIFO_REGS.SEND_DATA1);
			mt_set_pwm_wave_num(conf->pwm_no, conf->PWM_MODE_FIFO_REGS.WAVE_NUM);
			mt_set_pwm_con_stpbit(conf->pwm_no, conf->PWM_MODE_FIFO_REGS.STOP_BITPOS_VALUE,FIFO);
			break;
		case PWM_MODE_MEMORY:
			PWMDBG("PWM_MODE_MEMORY\n");
			mt_set_pwm_con_oldmode(conf->pwm_no, OLDMODE_DISABLE);
			mt_set_pwm_con_datasrc(conf->pwm_no, MEMORY);
			mt_set_pwm_con_mode (conf->pwm_no, PERIOD);
			mt_set_pwm_con_idleval(conf->pwm_no, conf->PWM_MODE_MEMORY_REGS.IDLE_VALUE);
			mt_set_pwm_con_guardval (conf->pwm_no, conf->PWM_MODE_MEMORY_REGS.GUARD_VALUE);
			mt_set_pwm_HiDur (conf->pwm_no, conf->PWM_MODE_MEMORY_REGS.HDURATION);
			mt_set_pwm_LowDur (conf->pwm_no, conf->PWM_MODE_MEMORY_REGS.LDURATION);
			mt_set_pwm_GuardDur (conf->pwm_no, conf->PWM_MODE_MEMORY_REGS.GDURATION);
			mt_set_pwm_buf0_addr(conf->pwm_no, (U32)conf->PWM_MODE_MEMORY_REGS.BUF0_BASE_ADDR);
			mt_set_pwm_buf0_size (conf->pwm_no, conf->PWM_MODE_MEMORY_REGS.BUF0_SIZE);
			mt_set_pwm_wave_num(conf->pwm_no, conf->PWM_MODE_MEMORY_REGS.WAVE_NUM);
			mt_set_pwm_con_stpbit(conf->pwm_no, conf->PWM_MODE_MEMORY_REGS.STOP_BITPOS_VALUE,MEMORY);
			
			break;
		case PWM_MODE_RANDOM:
			PWMDBG("PWM_MODE_RANDOM\n");
			mt_set_pwm_disable(conf->pwm_no);
			mt_set_pwm_con_oldmode(conf->pwm_no, OLDMODE_DISABLE);
			mt_set_pwm_con_datasrc(conf->pwm_no, MEMORY);
			mt_set_pwm_con_mode (conf->pwm_no, RAND);
			mt_set_pwm_con_idleval(conf->pwm_no, conf->PWM_MODE_RANDOM_REGS.IDLE_VALUE);
			mt_set_pwm_con_guardval (conf->pwm_no, conf->PWM_MODE_RANDOM_REGS.GUARD_VALUE);
			mt_set_pwm_HiDur (conf->pwm_no, conf->PWM_MODE_RANDOM_REGS.HDURATION);
			mt_set_pwm_LowDur (conf->pwm_no, conf->PWM_MODE_RANDOM_REGS.LDURATION);
			mt_set_pwm_GuardDur (conf->pwm_no, conf->PWM_MODE_RANDOM_REGS.GDURATION);
			mt_set_pwm_buf0_addr(conf->pwm_no, (U32 )conf->PWM_MODE_RANDOM_REGS.BUF0_BASE_ADDR);
			mt_set_pwm_buf0_size (conf->pwm_no, conf->PWM_MODE_RANDOM_REGS.BUF0_SIZE);
			mt_set_pwm_buf1_addr(conf->pwm_no, (U32 )conf->PWM_MODE_RANDOM_REGS.BUF1_BASE_ADDR);
			mt_set_pwm_buf1_size (conf->pwm_no, conf->PWM_MODE_RANDOM_REGS.BUF1_SIZE);
			mt_set_pwm_wave_num(conf->pwm_no, conf->PWM_MODE_RANDOM_REGS.WAVE_NUM);
			mt_set_pwm_con_stpbit(conf->pwm_no, conf->PWM_MODE_RANDOM_REGS.STOP_BITPOS_VALUE, MEMORY);
			mt_set_pwm_valid(conf->pwm_no, BUF0_EN_VALID);
			mt_set_pwm_valid(conf->pwm_no, BUF1_EN_VALID);
			break;

		case PWM_MODE_DELAY:
			PWMDBG("PWM_MODE_DELAY\n");
			mt_set_pwm_con_oldmode(conf->pwm_no, OLDMODE_DISABLE);
			mt_set_pwm_enable_seqmode();
			mt_set_pwm_disable(PWM3);
			mt_set_pwm_disable(PWM4);
			mt_set_pwm_disable(PWM5);
			mt_set_pwm_disable(PWM6);
			if ( conf->PWM_MODE_DELAY_REGS.PWM4_DELAY_DUR <0 ||conf->PWM_MODE_DELAY_REGS.PWM4_DELAY_DUR >= (1<<17) ||
				conf->PWM_MODE_DELAY_REGS.PWM5_DELAY_DUR < 0|| conf->PWM_MODE_DELAY_REGS.PWM5_DELAY_DUR >= (1<<17) ||
				conf->PWM_MODE_DELAY_REGS.PWM6_DELAY_DUR <0 || conf->PWM_MODE_DELAY_REGS.PWM6_DELAY_DUR >=(1<<17) ) {
				PWMDBG("Delay value invalid\n");
				return -EINVALID;
			}
			mt_set_pwm_delay_duration(PWM4_DELAY, conf->PWM_MODE_DELAY_REGS.PWM4_DELAY_DUR );
			mt_set_pwm_delay_clock(PWM4_DELAY, conf->PWM_MODE_DELAY_REGS.PWM4_DELAY_CLK);
			mt_set_pwm_delay_duration(PWM5_DELAY, conf->PWM_MODE_DELAY_REGS.PWM5_DELAY_DUR);
			mt_set_pwm_delay_clock(PWM5_DELAY, conf->PWM_MODE_DELAY_REGS.PWM5_DELAY_CLK);
			mt_set_pwm_delay_duration(PWM6_DELAY, conf->PWM_MODE_DELAY_REGS.PWM6_DELAY_DUR);
			mt_set_pwm_delay_clock(PWM6_DELAY, conf->PWM_MODE_DELAY_REGS.PWM6_DELAY_CLK);
			
			mt_set_pwm_enable(PWM3);
			mt_set_pwm_enable(PWM4);
			mt_set_pwm_enable(PWM5);
			mt_set_pwm_enable(PWM6);
			break;
			
		default:
			break;
		}

	switch (conf->clk_src) {
		case PWM_CLK_OLD_MODE_BLOCK:
			mt_set_pwm_clk (conf->pwm_no, CLK_BLOCK, conf->clk_div);
			PWMDBG("Enable oldmode and set clock block\n");
			break;
		case PWM_CLK_OLD_MODE_32K:
			mt_set_pwm_clk (conf->pwm_no, CLK_BLOCK_BY_1625_OR_32K, conf->clk_div);
			PWMDBG("Enable oldmode and set clock 32K\n");
			break;
		case PWM_CLK_NEW_MODE_BLOCK:
			mt_set_pwm_clk (conf->pwm_no, CLK_BLOCK, conf->clk_div);
			PWMDBG("Enable newmode and set clock block\n");
			break;
		case PWM_CLK_NEW_MODE_BLOCK_DIV_BY_1625:
			mt_set_pwm_clk (conf->pwm_no, CLK_BLOCK_BY_1625_OR_32K, conf->clk_div);
			PWMDBG("Enable newmode and set clock 32K\n");
			break;
		default:
			break;
		} 

	mt_set_pwm_enable(conf->pwm_no); 
	PWMDBG("mt_set_pwm_enable\n");

	return RSUCCESS;
	
}	

EXPORT_SYMBOL(pwm_set_spec_config);

void mt_pwm_dump_regs(void)
{
	int i;
	U32 reg_val;
	reg_val = INREG32(PWM_ENABLE);
	PWMMSG("\r\n[PWM_ENABLE is:%x]\n\r ", reg_val );

	for ( i = PWM1;  i <= PWM7; i ++ ) 
	{
		reg_val = INREG32(PWM_register[i] + 4* PWM_CON);
		PWMMSG("\r\n[PWM%d_CON is:%x]\r\n", i+1, reg_val);
		reg_val=INREG32(PWM_register[i]+4* PWM_HDURATION);
		PWMMSG("\r\n[PWM%d_HDURATION is:%x]\r\n",i+1,reg_val);
		reg_val = INREG32(PWM_register[i] + 4* PWM_LDURATION);
		PWMMSG("\r\n[PWM%d_LDURATION is:%x]\r\n", i+1, reg_val);
		reg_val=INREG32(PWM_register[i]+4* PWM_GDURATION);
		PWMMSG("\r\n[PWM%d_HDURATION is:%x]\r\n",i+1,reg_val);
		reg_val = INREG32(PWM_register[i] + 4* PWM_BUF0_BASE_ADDR);
		PWMMSG("\r\n[PWM%d_BUF0_BASE_ADDR is:%x]\r\n", i+1, reg_val);
		reg_val=INREG32(PWM_register[i]+4* PWM_BUF0_SIZE);
		PWMMSG("\r\n[PWM%d_BUF0_SIZE is:%x]\r\n",i+1,reg_val);
		reg_val = INREG32(PWM_register[i] + 4* PWM_BUF1_BASE_ADDR);
		PWMMSG("\r\n[PWM%d_BUF1_BASE_ADDR is:%x]\r\n", i+1, reg_val);
		reg_val=INREG32(PWM_register[i]+4* PWM_BUF1_SIZE);
		PWMMSG("\r\n[PWM%d_BUF1_SIZE is:%x]\r\n",i+1,reg_val);
		reg_val = INREG32(PWM_register[i] + 4* PWM_SEND_DATA0);
		PWMMSG("\r\n[PWM%d_SEND_DATA0 is:%x]\r\n", i+1, reg_val);
		reg_val=INREG32(PWM_register[i]+4* PWM_SEND_DATA1);
		PWMMSG("\r\n[PWM%d_PWM_SEND_DATA1 is:%x]\r\n",i+1,reg_val);
		reg_val = INREG32(PWM_register[i] + 4* PWM_WAVE_NUM);
		PWMMSG("\r\n[PWM%d_WAVE_NUM is:%x]\r\n", i+1, reg_val);
		if ( i <= PWM3||i==PWM7) {
			reg_val=INREG32(PWM_register[i]+4* PWM_DATA_WIDTH);
			PWMMSG("\r\n[PWM%d_WIDTH is:%x]\r\n",i+1,reg_val);
			reg_val=INREG32(PWM_register[i]+4* PWM_THRESH);
			PWMMSG("\r\n[PWM%d_THRESH is:%x]\r\n",i+1,reg_val);
			reg_val=INREG32(PWM_register[i]+4* PWM_SEND_WAVENUM);
			PWMMSG("\r\n[PWM%d_SEND_WAVENUM is:%x]\r\n",i+1,reg_val);
						reg_val=INREG32(PWM_register[i]+4* PWM_VALID);
			PWMMSG("\r\n[PWM%d_SEND_VALID is:%x]\r\n",i+1,reg_val);
		}else {
			reg_val=INREG32(PWM_register[i]+4* (PWM_SEND_WAVENUM-2));
			PWMMSG("\r\n[PWM%d_SEND_WAVENUM is:%x]\r\n",i+1,reg_val);
			reg_val=INREG32(PWM_register[i]+4* (PWM_VALID-2));
			PWMMSG("\r\n[PWM%d_SEND_VALID is:%x]\r\n",i+1,reg_val);
		}
	}		
}

EXPORT_SYMBOL(mt_pwm_dump_regs);

static ssize_t mt_pwm_dump_infos(char *buf, ssize_t len)
{
	int idx = 0, length = 0;
	int en, mode, clk, div, stpbit, high,low, grd, grdval, idlval, datawidth, thresh, intr_en;
	int intr_overflow, intr_fin;

	/*pwm1~pwm7*/
	for (idx=0; idx < 7; idx ++) {
		en = mt_get_pwm_enable(idx);
		mode = mt_get_pwm_mode(idx);
		if ( mode ==0 && ( idx == 0 || idx==1 || idx == 2 ||idx==6 ) ) {
			datawidth = mt_get_pwm_datawidth(idx);
			thresh = mt_get_pwm_thresh(idx);
		}else {
			datawidth = 0;
			thresh = 0;
		}
		
		clk = mt_get_pwm_clk(idx);
		div = mt_get_pwm_div(idx);
		stpbit = mt_get_pwm_stpbit(idx);
		high = mt_get_pwm_high(idx);
		low = mt_get_pwm_low(idx);
		grd = mt_get_pwm_grd(idx);
		grdval = mt_get_pwm_grdval(idx);
		idlval = mt_get_pwm_idlval(idx);
		intr_fin = mt_get_intr_status(idx*2);
		intr_overflow=mt_get_intr_status(idx*2+1);
		intr_en = (intr_overflow << 1 ) + intr_fin;
		

		length += snprintf(buf+length, len-length, "%d:%d %d %d %d %d %d %d %d %d %d %d %d %d\n", 
			idx+1, en, mode, clk, div, datawidth, thresh, stpbit, high, low, grd, grdval, idlval, intr_en);	
	}

	return length;
	
}

static ssize_t mt_pwm_show(struct device *dev, struct device_attribute *attr, const char *buf)
{
	struct pwm_device *pwmdev;
	char *name;
	int ret;

	pwmdev = (struct pwm_device *)dev_get_drvdata(dev);
	if (!pwmdev) {
		PWMDBG("The device is invalid.\n");
		return -EINVAL;
	}

	ret = mt_pwm_dump_infos(buf, PAGE_SIZE);
	return ret;
}

/* en, mode, clk, div, stpbit, high, low, grd, grdval, idlval are integers. 
*  en:0~1, mode:0 (old mode), 1(fifo), 2 (memory mode), 3 (random mode), 4 (seq mode) 
*   clk:0~1, div:0~7, 
*  stpbit: 0~63, high, low, grd: integers
*  grdval:0~1, idlval:0~1.
*/
static ssize_t mt_pwm_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct pwm_device *pwmdev;
	char *name;
	u32 value;
	u32 *buffer0;
	u32 *buffer1;
	u32 pwm_no, en, mode, clk, div, datawidth, thresh, stpbit, high, low, grd, grdval, idlval, intr_en;

	u32 data0=0x55555555;
	u32 data1=0x55555555;

	pwmdev = (struct pwm_device *)dev_get_drvdata(dev);
	if (!pwmdev) {
		PWMDBG("The device is invalid.\n");
		return -EINVAL;
	}

	if (!strncmp(buf, "-h", 2)) {
		PWMMSG("PWM_NO: ENABLE MODE CLK DIV DATAWIDTH THRESH STPBIT HIGH LOW GURD GRDVAL IDLVAL INTR_EN (0~3)\n");
	}

	if (!strncmp(buf, "-w", 2)) {
		buf += 2;
		if (13 == sscanf(buf, "=%d:%d %d %d %d %d %d %d %d %d %d %d %d ", &pwm_no, &en, &mode, 
			&clk, &div, &datawidth, &thresh, &stpbit, &high, &low, &grd, &grdval, &idlval,&intr_en)) {
			/*write the registers of the pwm */
			if (!en) {
				mt_set_pwm_disable(pwm_no-1);
				mt_power_off(pwm_no-1);
				return count;
				
			}else 
				mt_power_on(pwm_no-1);

			if  ( intr_en == 0 ) {
				mt_set_intr_disable( ( pwm_no - 1) *2 );
				mt_set_intr_disable( ( pwm_no -1) *2 + 1 );
				mt_clr_intr_ack( ( pwm_no-1)*2 );
				mt_clr_intr_ack( ( pwm_no-1 ) * 2 + 1);
			}else if ( intr_en == 1 ) {
				mt_set_intr_enable( ( pwm_no-1)*2 );
				mt_set_intr_disable( ( pwm_no -1) *2 + 1 );
				mt_set_intr_ack( (pwm_no-1 ) * 2);
				mt_clr_intr_ack( ( pwm_no-1 ) * 2 + 1);
			}else if (intr_en == 2) {
				mt_set_intr_disable( ( pwm_no - 1) *2);
				mt_set_intr_enable( ( pwm_no-1 )*2 + 1);
				mt_clr_intr_ack( ( pwm_no-1)*2 );
				mt_set_intr_ack( ( pwm_no-1 )*2 + 1);
			}else if (intr_en == 3) {
				mt_set_intr_enable( ( pwm_no-1 ) * 2);
				mt_set_intr_disable( ( pwm_no - 1 ) *2 + 1);
				mt_set_intr_ack( ( pwm_no-1 ) * 2 );
				mt_set_intr_ack( ( pwm_no-1 ) * 2 + 1);
			}
				

			/*old mode*/
			if (mode==0) {  
				mt_set_pwm_con_oldmode(pwm_no-1, OLDMODE_ENABLE);
				mt_set_pwm_data_width(pwm_no-1, datawidth);
				mt_set_pwm_thresh(pwm_no-1,thresh);
			}

			/*fifo mode*/
			if (mode == 1)  {
				mt_set_pwm_con_oldmode(pwm_no-1, OLDMODE_DISABLE);
				mt_set_pwm_con_mode(pwm_no-1, PERIOD);
				mt_set_pwm_con_datasrc(pwm_no-1, FIFO);
				mt_set_pwm_con_stpbit(pwm_no-1, stpbit, FIFO);
				mt_set_pwm_send_data0(pwm_no-1, data0);
				mt_set_pwm_send_data0(pwm_no-1, data1);
			}

			/*memory mode*/
			if (mode == 2) {
				buffer0 = (u32 *)kzalloc(32,GFP_KERNEL);
				memcpy(buffer0, (u32 *)&data0, 32);
				mt_set_pwm_con_oldmode(pwm_no-1, OLDMODE_DISABLE);
				mt_set_pwm_con_mode(pwm_no-1, PERIOD);
				mt_set_pwm_con_datasrc(pwm_no-1, MEMORY);
				mt_set_pwm_con_stpbit(pwm_no-1, stpbit, MEMORY);
				mt_set_pwm_buf0_addr(pwm_no-1, (u32)buffer1);
				mt_set_pwm_buf0_size(pwm_no-1, 32);
				mt_set_intr_enable((pwm_no-1)*2);
				mt_set_intr_enable((pwm_no-1)*2+1);
			}

			/*random mode*/
			if (mode == 3) {
				buffer0 = (u32 *)kzalloc(32, GFP_KERNEL);
				buffer1 = (u32 *)kzalloc(32, GFP_KERNEL);
				memcpy(buffer0, (u32 *)&data0, 32);
				memcpy(buffer1, (u32 *)&data1, 32);
				mt_set_pwm_con_oldmode(pwm_no-1, OLDMODE_DISABLE);
				mt_set_pwm_con_mode(pwm_no-1, RAND);
				mt_set_pwm_con_datasrc(pwm_no-1, MEMORY);
				mt_set_pwm_con_stpbit(pwm_no-1, stpbit, MEMORY);
				mt_set_pwm_buf0_addr(pwm_no-1, (u32)buffer0);
				mt_set_pwm_buf0_size(pwm_no-1, 32);
				mt_set_pwm_buf1_addr(pwm_no-1, (u32)buffer1);
				mt_set_pwm_buf1_size(pwm_no-1, 32);
				mt_set_pwm_valid(pwm_no-1, BUF0_VALID);
				mt_set_pwm_valid(pwm_no-1, BUF0_EN_VALID);
				mt_set_pwm_valid(pwm_no-1, BUF1_VALID);
				mt_set_pwm_valid(pwm_no-1, BUF1_EN_VALID);
			}

			/*seq mode*/
			if (mode == 4) {   
				mt_set_pwm_con_oldmode(PWM3, OLDMODE_DISABLE);
				mt_set_pwm_enable_seqmode();
				mt_set_pwm_con_mode(PWM3, PERIOD);
				mt_set_pwm_con_mode(PWM4, PERIOD);
				mt_set_pwm_con_mode(PWM5, PERIOD);
				mt_set_pwm_con_mode(PWM6, PERIOD);
				
				mt_set_pwm_con_datasrc(PWM3, FIFO);
				mt_set_pwm_con_datasrc(PWM4, FIFO);
				mt_set_pwm_con_datasrc(PWM5, FIFO);
				mt_set_pwm_con_datasrc(PWM6, FIFO);

				mt_set_pwm_send_data0(PWM3, data0);
				mt_set_pwm_send_data0(PWM3, data1);

				mt_set_pwm_send_data0(PWM4, data0);
				mt_set_pwm_send_data0(PWM4, data1);

				mt_set_pwm_send_data0(PWM5, data0);
				mt_set_pwm_send_data0(PWM5, data1);

				mt_set_pwm_send_data0(PWM6, data0);
				mt_set_pwm_send_data0(PWM6, data1);

				mt_set_pwm_con_stpbit(PWM3, stpbit, FIFO);
				mt_set_pwm_con_stpbit(PWM4, stpbit, FIFO);
				mt_set_pwm_con_stpbit(PWM5, stpbit, FIFO);
				mt_set_pwm_con_stpbit(PWM6, stpbit, FIFO);
				
				mt_set_pwm_enable(PWM3);
				mt_set_pwm_enable(PWM4);
				mt_set_pwm_enable(PWM5);
				mt_set_pwm_enable(PWM6);
			}

			mt_set_pwm_clk(pwm_no-1, clk, div);
			mt_set_pwm_HiDur(pwm_no-1, high);
			mt_set_pwm_LowDur(pwm_no-1, low);
			mt_set_pwm_GuardDur(pwm_no-1, grd);
			mt_set_pwm_con_guardval(pwm_no-1, grdval);
			mt_set_pwm_con_idleval(pwm_no-1, grdval);

			mt_set_pwm_enable(pwm_no-1);
					
		}else 
			PWMDBG("parameters less than expected.\n");
	}else 
		PWMDBG("Don't you want to store ?\n");

	return count;
}

static DEVICE_ATTR(pwm, S_IWUGO|S_IRUGO, mt_pwm_show, mt_pwm_store);

static struct device_attribute *pwm_attr_list[] = {
	&dev_attr_pwm,
};

static int mt_pwm_create_attr(struct device *dev)
{
	int idx, err = 0;
	int num = (int)(sizeof(pwm_attr_list)/sizeof(pwm_attr_list[0]));
	if (!dev)
		return -EINVAL;

	for (idx = 0; idx < num; idx ++) {
		if ((err = device_create_file(dev, pwm_attr_list[idx])))
			break;
	}

	return err;
}

static int mt_pwm_delete_attr(struct device *dev)
{
	int idx;
	int num = (int)(sizeof(pwm_attr_list)/sizeof(pwm_attr_list[0]));
	if (!dev)
		return -EINVAL;

	for (idx=0; idx < num; idx ++) {
		device_remove_file(dev, pwm_attr_list[idx]);
	}

	return 0;
}

int mt6573_pwm_open(struct inode *node, struct file *file)
{
	if (!pwm_dev) {
		PWMDBG("device is invalid.\n");
		return -EBADADDR;
	}

	atomic_inc(&pwm_dev->ref);
	file->private_data = pwm_dev;
	return nonseekable_open(node, file);
}

int mt6573_pwm_release(struct inode *node, struct file *file)
{
	if (!pwm_dev) {
		PWMDBG("device is invalid.\n");
		return -EBADADDR;
	}

	atomic_dec(&pwm_dev->ref);
	return RSUCCESS;
}

struct file_operations mt6573_pwm_fops={
	.owner = THIS_MODULE,
	.open=mt6573_pwm_open,
	.release=mt6573_pwm_release,
};

static struct miscdevice mt6573_pwm_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "mt6573_pwm",
	.fops = &mt6573_pwm_fops,
};

struct platform_device pwm_plat_dev={
	.name = "mt6573-pwm",
};

int mt6573_pwm_probe ( struct platform_device *pdev)
{
	platform_set_drvdata (pdev, pwm_dev);
	return RSUCCESS;
}

int  mt6573_pwm_remove(struct platform_device *pdev)
{
	if ( ! pdev ) {
		PWMDBG ("The plaform device is not exist\n");
		return -EBADADDR;
	}

	PWMDBG ( "mt6573_pwm_remove\n" );
	return RSUCCESS;
}

void mt6573_pwm_shutdown(struct platform_device *pdev)
{
	printk("mt6573_pwm_shutdown\n");
	return;
}

struct platform_driver pwm_plat_driver={
	.probe = mt6573_pwm_probe,
	.remove = mt6573_pwm_remove,
	.shutdown = mt6573_pwm_shutdown,
	.driver = {
		.name="mt6573-pwm"
	},
};

static int __init mt6573_pwm_init(void)
{
	int ret;
	ret = platform_device_register ( &pwm_plat_dev );
	if (ret < 0 ){
		PWMDBG ("platform_device_register error\n");
		goto out;
	}
	ret = platform_driver_register ( &pwm_plat_driver );
	if ( ret < 0 ) {
		PWMDBG ("platform_driver_register error\n");
		goto out;
	}

out:
	return ret;
}

static void __exit mt6573_pwm_exit(void)
{
	platform_device_unregister ( &pwm_plat_dev );
	platform_driver_unregister ( &pwm_plat_driver );
}

module_init(mt6573_pwm_init);
module_exit(mt6573_pwm_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Cindy Zhang <cindy.zhang@mediatek.com>");
MODULE_DESCRIPTION(" This module is for mt6573 chip of mediatek");

