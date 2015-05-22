#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/time.h>
#include "kd_flashlight.h"
#include <asm/io.h>
#include <asm/uaccess.h>
#include "kd_camera_hw.h"
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/xlog.h>
#include <mach/upmu_common.h>

#include <mach/mt_gpio.h>		// For gpio control

/*
0  1  2  3   4   5   6   7   8   9   10  11  12  13
25 50 75 100 125 150 300 400 500 600 700 800 900 1000
*/

/******************************************************************************
 * Debug configuration
******************************************************************************/
// availible parameter
// ANDROID_LOG_ASSERT
// ANDROID_LOG_ERROR
// ANDROID_LOG_WARNING
// ANDROID_LOG_INFO
// ANDROID_LOG_DEBUG
// ANDROID_LOG_VERBOSE
#define TAG_NAME "leds_strobe.c"
#define PK_DBG_NONE(fmt, arg...)    do {} while (0)
#define PK_DBG_FUNC(fmt, arg...)    xlog_printk(ANDROID_LOG_DEBUG  , TAG_NAME, KERN_INFO  "%s: " fmt, __FUNCTION__ ,##arg)
#define PK_WARN(fmt, arg...)        xlog_printk(ANDROID_LOG_WARNING, TAG_NAME, KERN_WARNING  "%s: " fmt, __FUNCTION__ ,##arg)
#define PK_NOTICE(fmt, arg...)      xlog_printk(ANDROID_LOG_DEBUG  , TAG_NAME, KERN_NOTICE  "%s: " fmt, __FUNCTION__ ,##arg)
#define PK_INFO(fmt, arg...)        xlog_printk(ANDROID_LOG_INFO   , TAG_NAME, KERN_INFO  "%s: " fmt, __FUNCTION__ ,##arg)
#define PK_TRC_FUNC(f)              xlog_printk(ANDROID_LOG_DEBUG  , TAG_NAME,  "<%s>\n", __FUNCTION__);
#define PK_TRC_VERBOSE(fmt, arg...) xlog_printk(ANDROID_LOG_VERBOSE, TAG_NAME,  fmt, ##arg)
#define PK_ERROR(fmt, arg...)       xlog_printk(ANDROID_LOG_ERROR  , TAG_NAME, KERN_ERR "%s: " fmt, __FUNCTION__ ,##arg)


#define DEBUG_LEDS_STROBE
#ifdef  DEBUG_LEDS_STROBE
	#define PK_DBG PK_DBG_FUNC
	#define PK_VER PK_TRC_VERBOSE
	#define PK_ERR PK_ERROR
	#define logI PK_DBG_FUNC
	#define logE(fmt, arg...)         printk(KERN_ERR PFX "%s: " fmt, __FUNCTION__ ,##arg)
#else
	#define PK_DBG(a,...)
	#define PK_VER(a,...)
	#define PK_ERR(a,...)
#endif

/******************************************************************************
 * local variables
******************************************************************************/
static DEFINE_SPINLOCK(g_strobeSMPLock);
static u32 strobe_Res = 0;
static BOOL g_strobe_On = 0;
static int g_duty=0;
static int g_duty2=0;
//static int g_step=-1;
static int g_timeOutTimeMs=0;
static int g_bOpen=1;

static struct work_struct workTimeOut;
static struct work_struct workWDReset;
/*****************************************************************************
Functions
*****************************************************************************/
static void work_timeOutFunc(struct work_struct *data);


extern U32 pmic_read_interface (U32 RegNum, U32 *val, U32 MASK, U32 SHIFT);
extern U32 pmic_config_interface (U32 RegNum, U32 val, U32 MASK, U32 SHIFT);
extern kal_uint32 upmu_get_reg_value(kal_uint32 reg);





static int setReg6332(int reg, int gRegV, int gRegM, int gRegSh)
{
    pmic_config_interface(reg,gRegV,gRegM,gRegSh);
    logI("\nQQQS pmic_config_interface %d %d %d %d (0x%x 0x%x 0x%x 0x%x)\n",reg, gRegV, gRegM, gRegSh, reg, gRegV, gRegM, gRegSh);
    return 0;
}
/*
static int getReg6332(void)
{
    //int regV;
    //pmic_read_interface(gReg, &regV, 0xFFFF, 0x0);
    //logI("\nQQQR pmic_read_interface %d %d (0x%x 0x%x)\n",gReg, regV, gReg, regV);
    //return regV;
    return 0;
}*/

static int getReg6332(int reg)
{
    int regV;
    pmic_read_interface(reg, &regV, 0xFFFF, 0x0);
    logI("\nQQQR pmic_read_interface %d %d (0x%x 0x%x)\n",reg, regV, reg, regV);
    return regV;
}
//===============================
static int fl_get6332Efuse(int row)
{
    U32 ret=0;
    U32 reg_val=0;
	int efusevalue;

    printk("[tsbuck_read_6332_efuse] start\n");

    //1. enable efuse ctrl engine clock
    ret=pmic_config_interface(0x80B6, 0x0010, 0xFFFF, 0);
    ret=pmic_config_interface(0x80A4, 0x0004, 0xFFFF, 0);

    //2.
    ret=pmic_config_interface(0x8C6C, 0x1, 0x1, 0);
/*
    //dump
    xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n",
        0x80B2,upmu_get_reg_value(0x80B2),
        0x80A0,upmu_get_reg_value(0x80A0),
        0x8C6C,upmu_get_reg_value(0x8C6C)
        );
*/
//    for(i=0;i<=0x1F;i++)

        //3. set row to read
        ret=pmic_config_interface(0x8C56, row, 0x1F, 1);

        //4. Toggle
        ret=pmic_read_interface(0x8C66, &reg_val, 0x1, 0);
        if(reg_val==0)
            ret=pmic_config_interface(0x8C66, 1, 0x1, 0);
        else
            ret=pmic_config_interface(0x8C66, 0, 0x1, 0);



        reg_val=1;
        while(reg_val == 1)
        {
            ret=pmic_read_interface(0x8C70, &reg_val, 0x1, 0);
            printk("5. polling Reg[0x61A][0]=0x%x\n", reg_val);
        }

		udelay(1000);//Need to delay at least 1ms for 0x8C70 and than can read 0x8C6E
        printk("5. 6332 delay 1 ms\n");

        //6. read data
        efusevalue = upmu_get_reg_value(0x8C6E);
		printk("6332_efuse : efusevalue=0x%x\n", efusevalue);
/*
        xlog_printk(ANDROID_LOG_INFO, "Power/PMIC", "i=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x,Reg[0x%x]=0x%x\n",
            i,
            0x8C56,upmu_get_reg_value(0x8C56),
            0x8C70,upmu_get_reg_value(0x8C70),
            0x8C6E,upmu_get_reg_value(0x8C6E)
            );
*/

    //}

    //7. Disable efuse ctrl engine clock
    ret=pmic_config_interface(0x80A2, 0x0004, 0xFFFF, 0);
    ret=pmic_config_interface(0x80B4, 0x0010, 0xFFFF, 0); // new add
    return efusevalue;
}
static int fl_getEfuseBit(int bit)
{
	int r;
	int id;
	int sh;
	id = bit/16;

	sh = bit-id*16;
	r = (fl_get6332Efuse(id) >> sh) & 1;
	return r;
}

static void fl_setEfuse(int bit)
{
    int r;
    int bit8030;
    int bit8030_base385[]={0,1,7,8,9,10,7,8,9,10,2,3,4,5,2,3,4,5,7,8,9,10,2,3,4,5,7,8,9,10,2,3,4,5,0,1,0,1,0,1,0,0,7,8,9,10,0,1,7,8,9,10,0,1};
    r  = fl_getEfuseBit(bit);
    bit8030 = bit8030_base385[bit-385];
    setReg6332(0x8030, r, 1, bit8030);
}

//torch
int fl_adjTorch6332_1(int lev)
{
    PK_DBG("qq fl_adjTorch6332_1\n");
    //25,50,75,100,125,150 mA
    if(lev<0)
        lev=0;
    if(lev>5)
        lev=5;
    setReg6332(0x8088, lev, 7, 0);
    return 0;
}

int fl_adjTorch6332_2(int lev)
{
    //25,50,75,100,125,150 mA
    if(lev<0)
        lev=0;
    if(lev>5)
        lev=5;
    setReg6332(0x8088, lev, 7, 4);
    return 0;
}

static int user1 = 0;
static int user2 = 0;
int mt6332_OpenBoost4Flash(void);
int mt6332_CloseBoost4Flash(void);
int fl_setTorchOnOff6332_1(int onOff)
{
    PK_DBG("qq fl_setTorchOnOff6332_1 %d\n",onOff);
    if(onOff==1)
	{
	user1 = 1;
        setReg6332(0x80b4,	0x1,	0x1,	6);
        //setReg6332(0x809e,	0x2,	0xffff,	0);
        mt6332_OpenBoost4Flash();
        setReg6332(0x8086,	0x1,	0x1,	0);
        mdelay(1);
        setReg6332(0x8554,	0x12,	0xffff,	0);
        setReg6332(0x853a,	0x0,	0xffff,	0);
        setReg6332(0x8536,	0x0,	0xffff,	0);
        setReg6332(0x8532,	0x1c0,	0xffff,	0);
        setReg6332(0x80b2,	0x3ff,	0xffff,	0);
        setReg6332(0x854a,	0x0,	0xffff,	0);
        setReg6332(0x854a,	0x1,	0xffff,	0);
        setReg6332(0x854c,	0x1,	0xffff,	0);
        setReg6332(0x8542,	0x40,	0xffff,	0);
        setReg6332(0x854e,	0x8,	0xffff,	0);
        setReg6332(0x853e,	0x21,	0xffff,	0);
        setReg6332(0x854e,	0x0,	0xffff,	0);
        setReg6332(0x8086,	0x1,	0x1,	1);
    }
    else
    {
	user1 = 0;
        setReg6332(0x8086,	0x0,	0x1,	1);
	if (user2 == 0) {
        setReg6332(0x8086,	0x0,	0x1,	0);
        //setReg6332(0x809c,	0x2,	0xffff,	0);
        mt6332_CloseBoost4Flash();
	}
    }
    return 0;
}
int fl_setTorchOnOff6332_2(int onOff)
{
    if(onOff==1)
    {
	user2 = 1;
        setReg6332(0x80b4,	0x1,	0x1,	6);
        //setReg6332(0x809e,	0x2,	0xffff,	0);
        mt6332_OpenBoost4Flash();
        setReg6332(0x8086,	0x1,	0x1,	0);
        mdelay(1);
        setReg6332(0x8554,	0x12,	0xffff,	0);
        setReg6332(0x853a,	0x0,	0xffff,	0);
        setReg6332(0x8536,	0x0,	0xffff,	0);
        setReg6332(0x8532,	0x1c0,	0xffff,	0);
        setReg6332(0x80b2,	0x3ff,	0xffff,	0);
        setReg6332(0x854a,	0x0,	0xffff,	0);
        setReg6332(0x854a,	0x1,	0xffff,	0);
        setReg6332(0x854c,	0x1,	0xffff,	0);
        setReg6332(0x8542,	0x40,	0xffff,	0);
        setReg6332(0x854e,	0x8,	0xffff,	0);
        setReg6332(0x853e,	0x21,	0xffff,	0);
        setReg6332(0x854e,	0x0,	0xffff,	0);
        setReg6332(0x8086,	0x1,	0x1,	2);
    }
    else
    {
	user2 = 0;
        setReg6332(0x8086,	0x0,	0x1,	2);
	if (user1 == 0) {
        setReg6332(0x8086,	0x0,	0x1,	0);
        //setReg6332(0x809c,	0x2,	0xffff,	0);
        mt6332_CloseBoost4Flash();
	}
    }
    return 0;
}

//flash
int gFlashLev6332_1=1;
int gFlashLev6332_2=1;
//@@
int fl_trimFlash6332_1(int lev)
{
    int e425;
    int e426;
    e425 = fl_getEfuseBit(425);
    e426 = fl_getEfuseBit(426);
    if(e425==0 && e426==0) //method 1
    {
        fl_setEfuse(385);
        fl_setEfuse(386);
        if(gFlashLev6332_1==0)
        {
            fl_setEfuse(387);
            fl_setEfuse(388);
            fl_setEfuse(389);
            fl_setEfuse(390);
            fl_setEfuse(395);
            fl_setEfuse(396);
            fl_setEfuse(397);
            fl_setEfuse(398);
        }
        else if(gFlashLev6332_1>=1 && gFlashLev6332_1<=4)
        {
            fl_setEfuse(403);
            fl_setEfuse(404);
            fl_setEfuse(405);
            fl_setEfuse(406);
            fl_setEfuse(407);
            fl_setEfuse(408);
            fl_setEfuse(409);
            fl_setEfuse(410);
        }
        else
        {
            fl_setEfuse(391);
            fl_setEfuse(392);
            fl_setEfuse(393);
            fl_setEfuse(394);
            fl_setEfuse(399);
            fl_setEfuse(400);
            fl_setEfuse(401);
            fl_setEfuse(402);
        }
    }
    else if(e425==1 && e426==0) //method 2
    {
        if(gFlashLev6332_1==0)
        {
            fl_setEfuse(385);
            fl_setEfuse(386);
            fl_setEfuse(387);
            fl_setEfuse(388);
            fl_setEfuse(389);
            fl_setEfuse(390);
            fl_setEfuse(395);
            fl_setEfuse(396);
            fl_setEfuse(397);
            fl_setEfuse(398);
        }
        else if(gFlashLev6332_1==1)
        {
            fl_setEfuse(403);
            fl_setEfuse(404);
            fl_setEfuse(405);
            fl_setEfuse(406);
            fl_setEfuse(407);
            fl_setEfuse(408);
            fl_setEfuse(409);
            fl_setEfuse(410);
            fl_setEfuse(419);
            fl_setEfuse(420);
        }
        else if(gFlashLev6332_1>=2 && gFlashLev6332_1<=4)
        {
            fl_setEfuse(411);
            fl_setEfuse(412);
            fl_setEfuse(413);
            fl_setEfuse(414);
            fl_setEfuse(415);
            fl_setEfuse(416);
            fl_setEfuse(417);
            fl_setEfuse(418);
            fl_setEfuse(421);
            fl_setEfuse(422);
        }
        else
        {
            fl_setEfuse(391);
            fl_setEfuse(392);
            fl_setEfuse(393);
            fl_setEfuse(394);
            fl_setEfuse(399);
            fl_setEfuse(400);
            fl_setEfuse(401);
            fl_setEfuse(402);
            fl_setEfuse(423);
            fl_setEfuse(424);
        }

    }
    else if(e425==1 && e426==1) //method 3
    {
        if(gFlashLev6332_1==0)
        {
            fl_setEfuse(385);
            fl_setEfuse(387);
            fl_setEfuse(388);
            fl_setEfuse(415);
            fl_setEfuse(416);
            fl_setEfuse(417);
            fl_setEfuse(418);
        }
        else if(gFlashLev6332_1==1)
        {
            fl_setEfuse(403);
            fl_setEfuse(404);
            fl_setEfuse(419);
            fl_setEfuse(415);
            fl_setEfuse(416);
            fl_setEfuse(417);
            fl_setEfuse(418);

        }
        else if(gFlashLev6332_1>=2 && gFlashLev6332_1<=4)
        {
            fl_setEfuse(411);
            fl_setEfuse(412);
            fl_setEfuse(421);
            fl_setEfuse(415);
            fl_setEfuse(416);
            fl_setEfuse(417);
            fl_setEfuse(418);

        }
        else
        {
            fl_setEfuse(423);
            fl_setEfuse(391);
            fl_setEfuse(392);
            fl_setEfuse(415);
            fl_setEfuse(416);
            fl_setEfuse(417);
            fl_setEfuse(418);
        }

    }
    else //if(e425==0 && e426==1) //method 4
    {
        if(gFlashLev6332_1==0)
        {
            fl_setEfuse(385);
            fl_setEfuse(387);
            fl_setEfuse(388);
            fl_setEfuse(415);
            fl_setEfuse(416);
            fl_setEfuse(417);
            fl_setEfuse(418);
        }
        else if(gFlashLev6332_1==1)
        {
            fl_setEfuse(403);
            fl_setEfuse(404);
            fl_setEfuse(419);
            fl_setEfuse(415);
            fl_setEfuse(416);
            fl_setEfuse(417);
            fl_setEfuse(418);

        }
        else if(gFlashLev6332_1==2 || gFlashLev6332_1==3)
        {
            fl_setEfuse(411);
            fl_setEfuse(412);
            fl_setEfuse(421);
            fl_setEfuse(415);
            fl_setEfuse(416);
            fl_setEfuse(417);
            fl_setEfuse(418);

        }
        else if(gFlashLev6332_1==4 || gFlashLev6332_1==5)
        {
            fl_setEfuse(427);
            fl_setEfuse(428);
            fl_setEfuse(431);
            fl_setEfuse(415);
            fl_setEfuse(416);
            fl_setEfuse(417);
            fl_setEfuse(418);

        }
        else if(gFlashLev6332_1==6)
        {
            fl_setEfuse(433);
            fl_setEfuse(434);
            fl_setEfuse(437);
            fl_setEfuse(415);
            fl_setEfuse(416);
            fl_setEfuse(417);
            fl_setEfuse(418);

        }
        else //7, 8
        {
            fl_setEfuse(423);
            fl_setEfuse(391);
            fl_setEfuse(392);
            fl_setEfuse(415);
            fl_setEfuse(416);
            fl_setEfuse(417);
            fl_setEfuse(418);
        }

    }
    return 0;
}
//@@
int fl_trimFlash6332_2(int lev)
{
    int e425;
    int e426;
    e425 = fl_getEfuseBit(425);
    e426 = fl_getEfuseBit(426);
    if(e425==0 && e426==0) //method 1
    {
        fl_setEfuse(385);
        fl_setEfuse(386);
        if(gFlashLev6332_2==0)
        {
            fl_setEfuse(387);
            fl_setEfuse(388);
            fl_setEfuse(389);
            fl_setEfuse(390);
            fl_setEfuse(395);
            fl_setEfuse(396);
            fl_setEfuse(397);
            fl_setEfuse(398);
        }
        else if(gFlashLev6332_2>=1 && gFlashLev6332_2<=4)
        {
            fl_setEfuse(403);
            fl_setEfuse(404);
            fl_setEfuse(405);
            fl_setEfuse(406);
            fl_setEfuse(407);
            fl_setEfuse(408);
            fl_setEfuse(409);
            fl_setEfuse(410);
        }
        else
        {
            fl_setEfuse(391);
            fl_setEfuse(392);
            fl_setEfuse(393);
            fl_setEfuse(394);
            fl_setEfuse(399);
            fl_setEfuse(400);
            fl_setEfuse(401);
            fl_setEfuse(402);
        }
    }
    else if(e425==1 && e426==0) //method 2
    {
        if(gFlashLev6332_2==0)
        {
            fl_setEfuse(385);
            fl_setEfuse(386);
            fl_setEfuse(387);
            fl_setEfuse(388);
            fl_setEfuse(389);
            fl_setEfuse(390);
            fl_setEfuse(395);
            fl_setEfuse(396);
            fl_setEfuse(397);
            fl_setEfuse(398);
        }
        else if(gFlashLev6332_2==1)
        {
            fl_setEfuse(403);
            fl_setEfuse(404);
            fl_setEfuse(405);
            fl_setEfuse(406);
            fl_setEfuse(407);
            fl_setEfuse(408);
            fl_setEfuse(409);
            fl_setEfuse(410);
            fl_setEfuse(419);
            fl_setEfuse(420);
        }
        else if(gFlashLev6332_2>=2 && gFlashLev6332_2<=4)
        {
            fl_setEfuse(411);
            fl_setEfuse(412);
            fl_setEfuse(413);
            fl_setEfuse(414);
            fl_setEfuse(415);
            fl_setEfuse(416);
            fl_setEfuse(417);
            fl_setEfuse(418);
            fl_setEfuse(421);
            fl_setEfuse(422);
        }
        else
        {
            fl_setEfuse(391);
            fl_setEfuse(392);
            fl_setEfuse(393);
            fl_setEfuse(394);
            fl_setEfuse(399);
            fl_setEfuse(400);
            fl_setEfuse(401);
            fl_setEfuse(402);
            fl_setEfuse(423);
            fl_setEfuse(424);
        }

    }
    else if(e425==1 && e426==1) //method 3
    {
        if(gFlashLev6332_1==0)
        {
            fl_setEfuse(386);
            fl_setEfuse(389);
            fl_setEfuse(390);
            fl_setEfuse(415);
            fl_setEfuse(416);
            fl_setEfuse(417);
            fl_setEfuse(418);
        }
        else if(gFlashLev6332_1==1)
        {
            fl_setEfuse(420);
            fl_setEfuse(405);
            fl_setEfuse(406);
            fl_setEfuse(415);
            fl_setEfuse(416);
            fl_setEfuse(417);
            fl_setEfuse(418);

        }
        else if(gFlashLev6332_1>=2 && gFlashLev6332_1<=4)
        {
            fl_setEfuse(413);
            fl_setEfuse(414);
            fl_setEfuse(422);
            fl_setEfuse(415);
            fl_setEfuse(416);
            fl_setEfuse(417);
            fl_setEfuse(418);

        }
        else
        {
            fl_setEfuse(424);
            fl_setEfuse(393);
            fl_setEfuse(394);
            fl_setEfuse(415);
            fl_setEfuse(416);
            fl_setEfuse(417);
            fl_setEfuse(418);
        }

    }
    else //if(e425==0 && e426==1) //method 4
    {
        if(gFlashLev6332_1==0)
        {
            fl_setEfuse(386);
            fl_setEfuse(389);
            fl_setEfuse(390);
            fl_setEfuse(415);
            fl_setEfuse(416);
            fl_setEfuse(417);
            fl_setEfuse(418);
        }
        else if(gFlashLev6332_1==1)
        {
            fl_setEfuse(420);
            fl_setEfuse(405);
            fl_setEfuse(406);
            fl_setEfuse(415);
            fl_setEfuse(416);
            fl_setEfuse(417);
            fl_setEfuse(418);

        }
        else if(gFlashLev6332_1==2 || gFlashLev6332_1==3)
        {
            fl_setEfuse(413);
            fl_setEfuse(414);
            fl_setEfuse(422);
            fl_setEfuse(415);
            fl_setEfuse(416);
            fl_setEfuse(417);
            fl_setEfuse(418);

        }
        else if(gFlashLev6332_1==4 || gFlashLev6332_1==5)
        {
            fl_setEfuse(429);
            fl_setEfuse(430);
            fl_setEfuse(432);
            fl_setEfuse(415);
            fl_setEfuse(416);
            fl_setEfuse(417);
            fl_setEfuse(418);

        }
        else if(gFlashLev6332_1==6)
        {
            fl_setEfuse(435);
            fl_setEfuse(436);
            fl_setEfuse(438);
            fl_setEfuse(415);
            fl_setEfuse(416);
            fl_setEfuse(417);
            fl_setEfuse(418);

        }
        else
        {
            fl_setEfuse(424);
            fl_setEfuse(393);
            fl_setEfuse(394);
            fl_setEfuse(415);
            fl_setEfuse(416);
            fl_setEfuse(417);
            fl_setEfuse(418);
        }

    }
    return 0;
}

int fl_adjFlash6332_1(int lev)
{
    PK_DBG("qq fl_adjFlash6332_1\n");
    //150,300,400,500,600,700,800,900,1000
    if(lev<0)
        lev=0;
    if(lev>8)
        lev=8;
    gFlashLev6332_1=lev;
    fl_trimFlash6332_1(lev);
    if(lev==0)
        setReg6332(0x808a, 8, 0xf, 0);
    else
        setReg6332(0x808a, lev-1, 0xf, 0);
    return 0;
}
int fl_adjFlash6332_2(int lev)
{
    //150,300,400,500,600,700,800,900,1000
    if(lev<0)
        lev=0;
    if(lev>8)
        lev=8;
    gFlashLev6332_2=lev;
    fl_trimFlash6332_2(lev);
    if(lev==0)
        setReg6332(0x808a, 8, 0xf, 8);
    else
        setReg6332(0x808a, lev-1, 0xf, 8);
    return 0;
}

int fl_fashPreOn6332(void)
{
    PK_DBG("qq fl_fashPreOn6332\n");
        setReg6332(0x8d22,	0x1,	0x1,	12);
        setReg6332(0x8d14,	0x1,	0x1,	12);
        setReg6332(0x803c,	0x3,	0x3,	0);
        setReg6332(0x803c,	0x2,	0x3,	2);
        setReg6332(0x803c,	0x1,	0x1,	14);
        setReg6332(0x8036,	0x0,	0x1,	0);
        setReg6332(0x8d24,	0xf,	0xf,	12);
        setReg6332(0x8d16,	0x1,	0x1,	15);
        setReg6332(0x803a,	0x1,	0x1,	6);
        setReg6332(0x8046,	0xa0,	0xffff,	0);
        setReg6332(0x803e,	0x1,	0x1,	2);
        setReg6332(0x803e,	0x1,	0x1,	3);
        setReg6332(0x803e,	0x3,	0x3,	8);
        setReg6332(0x803e,	0x1,	0x1,	10);
        setReg6332(0x8044,	0x3,	0x3,	0);
        setReg6332(0x8044,	0x3,	0x3,	8);
        setReg6332(0x8044,	0x1,	0x1,	11);
        setReg6332(0x809c,	0x8000,	0xffff,	0);
        //read reg(0x809a);
        setReg6332(0x8084,	0x1,	0x1,	2);
    //mdelay(50);
    return 0;
}
int fl_setFlashOnOff6332_1(int onOff)
{
    PK_DBG("qq fl_setFlashOnOff6332_1\n");
    if(onOff==1)
    {
        setReg6332(0x8086,	0x1,	0x1,	4);
    }
    else
    {
        setReg6332(0x8086,	0x0,	0x1,	4);
        //setReg6332(0x8086,	0x0,	0x1,	6);
        setReg6332(0x8084,	0x0,	0x1,	2);
        mdelay(2);
        setReg6332(0x8d22,	0x0,	0x1,	12);
        setReg6332(0x8d14,	0x0,	0x1,	12);
        setReg6332(0x8036,	0x1,	0x1,	0);
        setReg6332(0x8024,	0x0,	0xf,	12);
        setReg6332(0x8d16,	0x0,	0x1,	15);
        setReg6332(0x803a,	0x0,	0x1,	6);
        setReg6332(0x8046,	0x0,	0xffff,	0);
    }
    return 0;
}
int fl_setFlashOnOff6332_2(int onOff)
{
    if(onOff==1)
    {
        setReg6332(0x8086,	0x1,	0x1,	6);
    }
    else
    {
        //setReg6332(0x8086,	0x0,	0x1,	4);
        setReg6332(0x8086,	0x0,	0x1,	6);
        setReg6332(0x8084,	0x0,	0x1,	2);
        mdelay(2);
        setReg6332(0x8d22,	0x0,	0x1,	12);
        setReg6332(0x8d14,	0x0,	0x1,	12);
        setReg6332(0x8036,	0x1,	0x1,	0);
        setReg6332(0x8024,	0x0,	0xf,	12);
        setReg6332(0x8d16,	0x0,	0x1,	15);
        setReg6332(0x803a,	0x0,	0x1,	6);
        setReg6332(0x8046,	0x0,	0xffff,	0);

    }
    return 0;
}

int fl_init6332(void)
{
    //time out 800 1
    setReg6332(0x808c,	0x3,	0x3,	6);
    //time out 800 2
    setReg6332(0x808c,	0x3,	0x3,	9);
    return 0;
}

int fl_kickWdt6332(void)
{
    setReg6332(0x80e0,	0x1,	0x1,	7);
    return 0;

}


int fl_torchInd6332[39]={ 0, 1, 2, 3, 4, 5,0,1,2,3,4,-1,0,1,2,-1,4,5,2,-1,4,5,2,-1,4,5,2,-1,4,5,2,-1,5,-1,5,-1,5,3,5,};
int fl_flashInd6332[39]={-1,-1,-1,-1,-1,-1,0,0,0,0,0, 1,1,1,1,2,1,1,2,3,2,2,3,4,3,3,4,5,4,4,5,6,5,7,6,8,7,8,8};


int FL_enable_6332_2(void)
{
    int torchInd;
    int flashInd;
	PK_DBG("FL_enable2");
	if(g_duty2<0)
	    g_duty2=0;
	else if(g_duty2>38)
	    g_duty2=38;

	torchInd = fl_torchInd6332[g_duty2];
	flashInd = fl_flashInd6332[g_duty2];
	if(torchInd>=0)
	{
        fl_setTorchOnOff6332_2(1);
    }
    if(flashInd>=0)
    {
        fl_setFlashOnOff6332_2(1);
    }
    return 0;
}

int FL_disable_6332_2(void)
{
	PK_DBG("FL_disable2");
	fl_setTorchOnOff6332_2(0);
	fl_setFlashOnOff6332_2(0);
    return 0;
}

int FL_dim_duty_6332_2(kal_uint32 duty)
{
    int torchInd;
    int flashInd;
    g_duty2 = duty;
    if(g_duty2<0)
	    g_duty2=0;
	else if(g_duty2>38)
	    g_duty2=38;
    torchInd = fl_torchInd6332[g_duty2];
	flashInd = fl_flashInd6332[g_duty2];
	if(torchInd>=0)
	{
        fl_adjTorch6332_2(torchInd);
    }
    if(flashInd>=0)
    {
        fl_adjFlash6332_2(flashInd);
	/* WORKAROUND: preon call */
	fl_fashPreOn6332();
    }
    return 0;
}

int FL_getPreOnTime_6332_2(int duty)
{
    if(g_duty2>5)
        return 50;
    else
        return -1;
}
int FL_preOn_6332_2(void)
{
    fl_fashPreOn6332();
    return 0;
}

//===============================
//duty 0  1  2   3   4   5   6   7   8   9  10  11  12  13  14  15  16  17  18  19  20  21  22  23  24  25  26  27  28  29  30  31  32  33  34   35   36   37   38
//mA  25 50 75 100 125 150 175 200 225 250 275 300 325 350 375 400 425 450 475 500 525 550 575 600 625 650 675 700 725 750 775 800 850 900 950 1000 1050 1100 1150

static int FL_enable(void)
{
    int torchInd;
    int flashInd;
	PK_DBG("FL_enable");
	if(g_duty<0)
	    g_duty=0;
	else if(g_duty>38)
	    g_duty=38;

	torchInd = fl_torchInd6332[g_duty];
	flashInd = fl_flashInd6332[g_duty];
	if(torchInd>=0)
	{
        fl_setTorchOnOff6332_1(1);
    }
    if(flashInd>=0)
    {
        fl_setFlashOnOff6332_1(1);
    }
    return 0;
}

static int FL_disable(void)
{
	PK_DBG("FL_disable");
	fl_setTorchOnOff6332_1(0);
	fl_setFlashOnOff6332_1(0);
    return 0;
}

static int FL_dim_duty(kal_uint32 duty)
{
    int torchInd;
    int flashInd;
    g_duty = duty;
    if(g_duty<0)
	    g_duty=0;
	else if(g_duty>38)
	    g_duty=38;
    torchInd = fl_torchInd6332[g_duty];
	flashInd = fl_flashInd6332[g_duty];
	if(torchInd>=0)
{
        fl_adjTorch6332_1(torchInd);
    }
    if(flashInd>=0)
    {
        fl_adjFlash6332_1(flashInd);
	/*WA: preon call */
	fl_fashPreOn6332();
    }


    return 0;
}
static int FL_getPreOnTime(int duty)
{
    if(duty>5)
        return 50;
    else
        return -1;
}
static int FL_preOn(void)
{
    fl_fashPreOn6332();
    return 0;
}

static int FL_init(void)
{
    fl_init6332();
    g_duty=0;


	FL_disable();
    return 0;
}

static int FL_uninit(void)
{
	PK_DBG("FL_uninit");

	FL_disable();
    return 0;
}

/*****************************************************************************
User interface
*****************************************************************************/
static struct hrtimer g_timeOutTimer;
static struct hrtimer g_WDResetTimer;
static void work_timeOutFunc(struct work_struct *data)
{
	FL_disable();
    PK_DBG("ledTimeOut_callback\n");
    //printk(KERN_ALERT "work handler function./n");
}

static void work_WDResetFunc(struct work_struct *data)
{
	ktime_t ktime;
	//mt6333_set_rg_chrwdt_wr(1); // write 1 to kick chr wdt
	if(g_bOpen==1)
	{
		ktime = ktime_set( 0, 1000*1000000 );//1s
		hrtimer_start( &g_WDResetTimer, ktime, HRTIMER_MODE_REL );
	}
}

static enum hrtimer_restart ledTimeOutCallback(struct hrtimer *timer)
{
	PK_DBG("ledTimeOut_callback\n");
	schedule_work(&workTimeOut);

    return HRTIMER_NORESTART;
}


static enum hrtimer_restart ledWDResetCallback(struct hrtimer *timer)
{
    schedule_work(&workWDReset);
    return HRTIMER_NORESTART;
}


static void timerInit(void)
{
	ktime_t ktime;
	static int g_b1stInit=1;
    fl_kickWdt6332();
	if(g_b1stInit==1)
    {
		g_b1stInit=0;
	    INIT_WORK(&workWDReset, work_WDResetFunc);
	    hrtimer_init( &g_WDResetTimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL );
	    g_WDResetTimer.function=ledWDResetCallback;

	  	INIT_WORK(&workTimeOut, work_timeOutFunc);
	g_timeOutTimeMs=1000; //1s
	hrtimer_init( &g_timeOutTimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL );
	g_timeOutTimer.function=ledTimeOutCallback;
	}
	ktime = ktime_set( 0, 1000*1000000 );//1s
	hrtimer_start( &g_WDResetTimer, ktime, HRTIMER_MODE_REL );

}
static int gGetPreOnDuty=0;
static int constant_flashlight_ioctl(MUINT32 cmd, MUINT32 arg)
{
    int temp;
	int i4RetValue = 0;
	int iFlashType = (int)FLASHLIGHT_NONE;
	int ior;
	int iow;
	int iowr;
	ior = _IOR(FLASHLIGHT_MAGIC,0, int);
	iow = _IOW(FLASHLIGHT_MAGIC,0, int);
	iowr = _IOWR(FLASHLIGHT_MAGIC,0, int);
	PK_DBG("constant_flashlight_ioctl() line=%d cmd=%d, ior=%d, iow=%d iowr=%d arg=%d\n",__LINE__, cmd, ior, iow, iowr, arg);
	PK_DBG("constant_flashlight_ioctl() line=%d cmd-ior=%d, cmd-iow=%d cmd-iowr=%d arg=%d\n",__LINE__, cmd-ior, cmd-iow, cmd-iowr, arg);
    switch(cmd)
    {

        case FLASH_IOC_GET_REG:
            logI("FLASH_IOC_GET_REG %d 0x%x ##", arg, arg);
            return getReg6332(arg);
        break;

		case FLASH_IOC_SET_TIME_OUT_TIME_MS:
			PK_DBG("FLASH_IOC_SET_TIME_OUT_TIME_MS: %d\n",arg);
			g_timeOutTimeMs=arg;
			break;

    	case FLASH_IOC_SET_DUTY :
    		PK_DBG("FLASHLIGHT_DUTY: %d\n",arg);
    		g_duty=arg;
    		FL_dim_duty(arg);
    		break;
        case FLASH_IOC_PRE_ON:
    		PK_DBG("FLASH_IOC_PRE_ON\n");
			FL_preOn();
    		break;

        case FLASH_IOC_GET_PRE_ON_TIME_MS_DUTY:
            PK_DBG("FLASH_IOC_GET_PRE_ON_TIME_MS_DUTY: %d\n",arg);
            gGetPreOnDuty = arg;
            break;

        case FLASH_IOC_GET_PRE_ON_TIME_MS:
    		PK_DBG("FLASH_IOC_GET_PRE_ON_TIME_MS: %d\n",arg);
    		temp = FL_getPreOnTime(gGetPreOnDuty);
    		if(copy_to_user((void __user *) arg , (void*)&temp , 4))
            {
                PK_DBG(" ioctl copy to user failed\n");
                return -1;
            }
    		break;
        case FLASH_IOC_GET_FLASH_DRIVER_NAME_ID:
            PK_DBG("FLASH_IOC_GET_FLASH_DRIVER_NAME_ID: %d\n",arg);
            temp = e_FLASH_DRIVER_6332;
    		if(copy_to_user((void __user *) arg , (void*)&temp , 4))
            {
                PK_DBG(" ioctl copy to user failed\n");
                return -1;
            }
    		break;

    	case FLASH_IOC_SET_ONOFF :
    		PK_DBG("FLASHLIGHT_ONOFF: %d\n",arg);
    		if(arg==1)
    		{
				if(g_timeOutTimeMs!=0)
	            {
	            	ktime_t ktime;
					ktime = ktime_set( 0, g_timeOutTimeMs*1000000 );
					hrtimer_start( &g_timeOutTimer, ktime, HRTIMER_MODE_REL );
	            }
    			FL_enable();
    			g_strobe_On=1;
    		}
    		else
    		{
    			FL_disable();
				hrtimer_cancel( &g_timeOutTimer );
				g_strobe_On=0;
    		}
    		break;
        case FLASHLIGHTIOC_G_FLASHTYPE:
            iFlashType = FLASHLIGHT_LED_CONSTANT;
            if(copy_to_user((void __user *) arg , (void*)&iFlashType , _IOC_SIZE(cmd)))
            {
                PK_DBG("[strobe_ioctl] ioctl copy to user failed\n");
                return -EFAULT;
            }
            break;
		default :
    		PK_DBG(" No such command \n");
    		i4RetValue = -EPERM;
    		break;
    }
    return i4RetValue;
}

static int constant_flashlight_open(void *pArg)
{
    int i4RetValue = 0;
    PK_DBG("constant_flashlight_open line=%d\n", __LINE__);

	if (0 == strobe_Res)
	{
	    FL_init();
		timerInit();
	}
	spin_lock_irq(&g_strobeSMPLock);

    if(strobe_Res)
    {
        PK_ERR(" busy!\n");
        i4RetValue = -EBUSY;
    }
    else
    {
        strobe_Res += 1;
    }

    g_bOpen=1;
    spin_unlock_irq(&g_strobeSMPLock);

    return i4RetValue;

}

static int constant_flashlight_release(void *pArg)
{
    PK_DBG(" constant_flashlight_release\n");

    if (strobe_Res)
    {
        spin_lock_irq(&g_strobeSMPLock);
        g_bOpen=0;

        strobe_Res = 0;

        /* LED On Status */
        g_strobe_On = FALSE;

        spin_unlock_irq(&g_strobeSMPLock);

    	FL_uninit();
    }

    PK_DBG(" Done\n");

    return 0;

}

static FLASHLIGHT_FUNCTION_STRUCT	constantFlashlightFunc=
{
	constant_flashlight_open,
	constant_flashlight_release,
	constant_flashlight_ioctl
};

MUINT32 constantFlashlightInit(PFLASHLIGHT_FUNCTION_STRUCT *pfFunc)
{
    if (pfFunc != NULL)
    {
        *pfFunc = &constantFlashlightFunc;
    }
    return 0;
}

/* LED flash control for high current capture mode*/
ssize_t strobe_VDIrq(void)
{

    return 0;
}

EXPORT_SYMBOL(strobe_VDIrq);
