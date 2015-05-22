#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <generated/autoconf.h>
#include <linux/platform_device.h>

#include <linux/types.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/atomic.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/smp.h>

#include <mach/mt_reg_base.h>
#include <mach/mt_typedefs.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/completion.h>
#include <linux/random.h>
#include <asm/system.h>

//extern int rand(void);
#define RAND_MAX 20000000

//#include <processor.h>
//#include <BusMonitor.h> //for high pri test
//#include "ts_pmic_wrap.h"
//#include "../../pmic_wrap/reg_pmic.h"
#include "../../pmic_wrap/reg_pmic_wrap.h"
//#include "../../pmic_wrap/mt_pmic_wrap.h"
#include <mach/mt_pmic_wrap.h>

//#include "reg_pmic.h"
//#include "reg_pmic_wrap.h"
//#include "mt_pmic_wrap.h"
#include "tc_pwrap_ldvt.h"
#include "register_read_write_test.h"


S32 tc_wrap_init_test(void  );
S32 tc_wrap_access_test( void );
S32 tc_status_update_test(void  );
//S32 tc_event_test( void );
S32 tc_dual_io_test(void  );
S32 tc_reg_rw_test(  void);
S32 tc_mux_switch_test( void );
S32 tc_reset_pattern_test( void );
S32 tc_soft_reset_test( void );
S32 tc_high_pri_test( void );
S32 tc_spi_encryption_test( void );
S32 tc_wdt_test(void  );
S32 tc_int_test(void  );
S32 tc_concurrence_test( void );

static void _concurrence_eint_test_callback1(int eint_idx);

S32 pwrap_lisr_normal_test(void);
S32 pwrap_interrupt_on_ldvt(void);
S32 pwrap_lisr_normal_test(void);
S32 pwrap_lisr_for_wdt_test(void);
S32 pwrap_lisr_for_int_test(void);
DECLARE_COMPLETION(pwrap_done);


#define WRAP_ACCESS_TEST_REG DEW_WRITE_TEST
#define ldvt_follow_up
#define DEBUG_LDVT

U32 eint_in_cpu0=0;
U32 eint_in_cpu1=2;
static  void pwrap_delay_us(U32 us)
  {
    volatile UINT32 delay=100*1000;
    while(delay--);
  }
static inline void pwrap_complete(void *arg)
{
  complete(arg);
}

//--------------wrap test API--------------------------------------------------------------------

//--------------------------------------------------------
//    Function : _pwrap_status_update_test()
// Description :
//   Parameter :
//      Return :
//--------------------------------------------------------
S32 _pwrap_status_update_test( void )
{
  U32 i, j;
  U32 rdata;
  PWRAPFUC();
  //disable signature interrupt
  WRAP_WR32(PMIC_WRAP_INT_EN,0x0);
  pwrap_write(DEW_WRITE_TEST, WRITE_TEST_VALUE);
  WRAP_WR32(PMIC_WRAP_SIG_ADR,DEW_WRITE_TEST);
  WRAP_WR32(PMIC_WRAP_SIG_VALUE,0xAA55);
  WRAP_WR32(PMIC_WRAP_SIG_MODE, 0x1);

  pwrap_delay_us(5000);//delay 5 seconds
  rdata=WRAP_RD32(PMIC_WRAP_SIG_ERRVAL);
  if( rdata != WRITE_TEST_VALUE )
  {
    PWRAPERR("_pwrap_status_update_test error,error code=%x, rdata=%x\n", 1, rdata);
    //return 1;
  }
  WRAP_WR32(PMIC_WRAP_SIG_VALUE,WRITE_TEST_VALUE);//tha same as write test
  //clear sig_error interrupt flag bit
  WRAP_WR32(PMIC_WRAP_INT_CLR,1<<1);

  //enable signature interrupt
  WRAP_WR32(PMIC_WRAP_INT_EN,0x7ffffffd);
  WRAP_WR32(PMIC_WRAP_SIG_MODE, 0x0);
  WRAP_WR32(PMIC_WRAP_SIG_ADR , DEW_CRC_VAL);
  return 0;
}
//--------------------------------------------------------
//    Function : _pwrap_wrap_access_test()
// Description :
//   Parameter :
//      Return :
//--------------------------------------------------------
S32 _pwrap_wrap_access_test( void )
{
  U32 rdata=0;
  U32 res=0;
  U32 reg_value_backup=0;
  U32 return_value=0;
  PWRAPFUC();
  //###############################//###############################
  // Read/Write test using WACS0
  //###############################
  //clear sig_error interrupt test
  reg_value_backup=WRAP_RD32(PMIC_WRAP_INT_EN);
  WRAP_CLR_BIT(1<<1,PMIC_WRAP_INT_EN);
  PWRAPLOG("start test WACS0\n");
  return_value=pwrap_wacs0(0, DEW_READ_TEST, 0, &rdata);
  if( rdata != DEFAULT_VALUE_READ_TEST )
  {
    PWRAPERR("read test error(using WACS0),return_value=%x, rdata=%x\n", return_value, rdata);
    res+=1;
  }
  rdata=0;
  pwrap_wacs0(1, WRAP_ACCESS_TEST_REG, 0x1234, &rdata);
  return_value=pwrap_wacs0(0, WRAP_ACCESS_TEST_REG, 0, &rdata);
  if( rdata != 0x1234 )
  {
    PWRAPERR("write test error(using WACS0),return_value=%x, rdata=%x\n", return_value, rdata);
    res+=1;
  }
  //###############################//###############################
  // Read/Write test using WACS1
  //###############################
  PWRAPLOG("start test WACS1\n");
  return_value=pwrap_wacs1(0, DEW_READ_TEST, 0, &rdata);
  if( rdata != 0x5aa5 )
  {
    PWRAPERR("read test error(using WACS1),return_value=%x, rdata=%x\n", return_value, rdata);
    res+=1;
  }
  rdata=0;
  pwrap_wacs1(1, WRAP_ACCESS_TEST_REG, 0x1234, &rdata);
  return_value=pwrap_wacs1(0, WRAP_ACCESS_TEST_REG, 0, &rdata);
  if( rdata != 0x1234 )
  {
    PWRAPERR("write test error(using WACS1),return_value=%x, rdata=%x\n", return_value, rdata);
    res+=1;
  }

  rdata=0;
  //###############################//###############################
  // Read/Write test using WACS2
  //###############################
  PWRAPLOG("start test WACS2\n");
  return_value=pwrap_read(DEW_READ_TEST,&rdata);
  if( rdata != DEFAULT_VALUE_READ_TEST )
  {
    PWRAPERR("read test error(using WACS2),return_value=%x, rdata=%x\n", return_value, rdata);
    res+=1;
  }

  rdata=0;
  pwrap_write(WRAP_ACCESS_TEST_REG, 0x1234);
  return_value=pwrap_wacs2(0, WRAP_ACCESS_TEST_REG, 0, &rdata);
  if( rdata != 0x1234 )
  {
    PWRAPERR("write test error(using WACS2),return_value=%x, rdata=%x\n", return_value, rdata);
    res+=1;
  }

  WRAP_WR32(PMIC_WRAP_INT_EN,reg_value_backup);
  return res;
}
//--------------------------------------------------------
//    Function : _pwrap_man_access_test()
// Description :
//   Parameter :
//      Return :
//--------------------------------------------------------
S32 _pwrap_man_access_test( void )
{
  U32 rdata=0;
  U32 res=0;
  U32 return_value=0;
  U32 reg_value_backup;
  PWRAPFUC();
  //###############################//###############################
  // Read/Write test using manual mode
  //###############################
  reg_value_backup=WRAP_RD32(PMIC_WRAP_STAUPD_GRPEN);
  WRAP_WR32(PMIC_WRAP_STAUPD_GRPEN,reg_value_backup & (~(0x1<<6)));

  return_value=_pwrap_manual_modeAccess(0, DEW_READ_TEST, 0, &rdata);
  if( rdata != DEFAULT_VALUE_READ_TEST )
  {
    /* TERR="Error: [ReadTest] fail, rdata=%x, exp=0x5aa5", rdata */
    PWRAPERR("read test error(using manual mode),return_value=%x, rdata=%x", return_value, rdata);
    res+=1;
  }

  rdata=0;
  _pwrap_manual_modeAccess(1, WRAP_ACCESS_TEST_REG, 0x1234, &rdata);
  return_value=_pwrap_manual_modeAccess(0, WRAP_ACCESS_TEST_REG, 0, &rdata);
  if( rdata != 0x1234 )
  {
    /* TERR="Error: [WriteTest] fail, rdata=%x, exp=0x1234", rdata*/
    PWRAPERR("write test error(using manual mode),return_value=%x, rdata=%x", return_value, rdata);
    res+=1;
  }
  _pwrap_switch_mux(0);//wrap mode

  rdata=0;
  //MAN
  _pwrap_AlignCRC();
  WRAP_WR32(PMIC_WRAP_STAUPD_GRPEN,reg_value_backup );

  return res;
}


S32 tc_wrap_init_test(  )
{
  UINT32 ret=0;
  UINT32 res=0;
  U32 regValue=0;
  struct pmic_wrap_obj *pwrap_obj = g_pmic_wrap_obj;
  pwrap_obj->complete = pwrap_complete;
  pwrap_obj->context = &pwrap_done;
  //CTP_GetRandom(&u4Random, 200);
  ret=pwrap_init();
  if(ret==0)
  {
    PWRAPLOG("wrap_init test pass.\n");
  }
  else
  {
    PWRAPLOG("error:wrap_init test fail.return_value=%d.\n",ret);
    res+=1;
  }
#ifdef DEBUG_LDVT
  regValue=WRAP_RD32(PMIC_WRAP_INT_FLG);
    PWRAPLOG("PMIC_WRAP_INT_FLG =%x\n",regValue);
  //regValue=WRAP_RD32(PERI_PWRAP_BRIDGE_INT_FLG);
   // PWRAPLOG("PERI_PWRAP_BRIDGE_INT_FLG =%x\n",regValue);
  regValue=WRAP_RD32(PMIC_WRAP_WDT_FLG);
    PWRAPLOG("PMIC_WRAP_WDT_FLG =%x\n",regValue);
  //regValue=WRAP_RD32(PERI_PWRAP_BRIDGE_WDT_FLG);
    //PWRAPLOG("PERI_PWRAP_BRIDGE_WDT_FLG =%x\n",regValue);
#endif

  ret=_pwrap_status_update_test();
  if(ret==0)
  {
    PWRAPLOG("_pwrap_status_update_test pass.\n");
  }
  else
  {
    PWRAPLOG("error:_pwrap_status_update_test fail.\n");
    res+=1;
  }
#ifdef DEBUG_CTP
  regValue=WRAP_RD32(PMIC_WRAP_INT_FLG);
    PWRAPLOG("PMIC_WRAP_INT_FLG =%x\n",regValue);
  //regValue=WRAP_RD32(PERI_PWRAP_BRIDGE_INT_FLG);
  //  PWRAPLOG("PERI_PWRAP_BRIDGE_INT_FLG =%x\n",regValue);
  regValue=WRAP_RD32(PMIC_WRAP_WDT_FLG);
    PWRAPLOG("PMIC_WRAP_WDT_FLG =%x\n",regValue);
  //regValue=WRAP_RD32(PERI_PWRAP_BRIDGE_WDT_FLG);
  //  PWRAPLOG("PERI_PWRAP_BRIDGE_WDT_FLG =%x\n",regValue);
#endif
  //pwrap_interrupt_on_ldvt();
  if (res != 0)
  {
    PWRAPLOG("error:tc_wrap_init_test fail.\n");

  }
  else
  {
    PWRAPLOG("tc_wrap_init_test pass.\n");

  }
  return res;
}
S32 tc_wrap_access_test(  )
{
    UINT32 res=0;
    res=_pwrap_wrap_access_test();
    if(res==0)
    {
      PWRAPLOG("WRAP_UVVF_WACS_TEST pass.\n");
    }
    else
    {
      PWRAPLOG("WRAP_UVVF_WACS_TEST fail.res=%d\n",res);
    }
    return res;
}


S32 tc_status_update_test(  )
{
    UINT32 res=0;

    res=0;
    res=_pwrap_status_update_test();
    if(res==0)
    {
      PWRAPLOG("_pwrap_status_update_test pass.\n");
    }
    else
    {
      PWRAPLOG("_pwrap_status_update_test fail.res=%d\n",res);
    }
    return res;
}

S32 tc_dual_io_test(  )
{
    UINT32 res=0;
    U32 rdata=0;

    res=0;
    //###############################//###############################
    PWRAPLOG("enable dual io mode.\n");
    //enable dual io mode
    _pwrap_switch_dio(1);
    res=_pwrap_wrap_access_test();
    if(res==0)
      PWRAPLOG("_pwrap_wrap_access_test pass.\n");
    else
      PWRAPLOG("_pwrap_wrap_access_test fail.res=%d\n",res);

    //###############################//###############################
    //disable dual io mode
    _pwrap_switch_dio(0);
    PWRAPLOG("disable dual io mode.\n");
    res=_pwrap_wrap_access_test();
    if(res==0)
      PWRAPLOG("_pwrap_wrap_access_test pass.\n");
    else
      PWRAPLOG("_pwrap_wrap_access_test fail.res=%d\n",res);

    if(res==0)
    {
      PWRAPLOG("tc_dual_io_test pass.\n");

    }
    else
    {
      PWRAPLOG("tc_dual_io_test fail.res=%d\n",res);

    }
    return res;
}

U32 RegWriteValue[4] = {0, 0xFFFFFFFF, 0x55555555, 0xAAAAAAAA};

S32 tc_reg_rw_test(  )
{
  U32 res=0;
  U32 i,j;
  U32 pmic_wrap_reg_size=0;
  U32 PERI_PWRAP_BRIDGE_reg_size=0;
  U32 DEW_reg_tbl_size=0;
  U32 regValue=0;
  U32 reg_data=0;

  U32 test_result=0;
  PWRAPFUC();

  pmic_wrap_reg_size=sizeof(pmic_wrap_reg_tbl)/sizeof(pmic_wrap_reg_tbl[0]);
  //PERI_PWRAP_BRIDGE_reg_size=sizeof(PERI_PWRAP_BRIDGE_reg_tbl)/sizeof(PERI_PWRAP_BRIDGE_reg_tbl[0]);
  DEW_reg_tbl_size=sizeof(DEW_reg_tbl)/sizeof(DEW_reg_tbl[0]);

  PWRAPLOG("pmic_wrap_reg_size=%d\n",pmic_wrap_reg_size);
  //PWRAPLOG("PERI_PWRAP_BRIDGE_reg_size=%d\n",PERI_PWRAP_BRIDGE_reg_size);
  PWRAPLOG("DEW_reg_tbl_size=%d\n",DEW_reg_tbl_size);

  PWRAPLOG("start test pmic_wrap_reg_tbl:default value test\n");
  for(i=0; i<pmic_wrap_reg_size; i++)
  {
    //Only R/W or RO should do default value test
    if(REG_TYP_WO!=pmic_wrap_reg_tbl[i][3])
    {
        PWRAPLOG("Register %x Default Value test DEF %x,i=%d\n", pmic_wrap_reg_tbl[i][0], pmic_wrap_reg_tbl[i][1],i);

      if((*((volatile unsigned int *)(pmic_wrap_reg_tbl[i][0]+PMIC_WRAP_BASE))!=pmic_wrap_reg_tbl[i][1]))
      {
        PWRAPLOG("Register %x Default Value test fail. DEF %x, read %x \r\n", pmic_wrap_reg_tbl[i][0]+PMIC_WRAP_BASE,  pmic_wrap_reg_tbl[i][1],  (*((volatile unsigned int *)(pmic_wrap_reg_tbl[i][0]+PMIC_WRAP_BASE))));
        test_result++;
      }
    }
  }
//#if 0
  PWRAPLOG("start test pmic_wrap_reg_tbl:R/W test\n");
  //sixian:test value:RegWriteValue[LCD_RegWriteValueSize] = {0, 0xFFFFFFFF, 0x55555555, 0xAAAAAAAA};
  for(i=0; i<pmic_wrap_reg_size; i++)
  {
    if(REG_TYP_RW==pmic_wrap_reg_tbl[i][3])
    {
      for(j=0; j<pmic_wrap_reg_size; j++)
      {
        *((volatile unsigned int *)(pmic_wrap_reg_tbl[i][0]+PMIC_WRAP_BASE)) = (RegWriteValue[j]&pmic_wrap_reg_tbl[i][2]);

        if(((*((volatile unsigned int *)(pmic_wrap_reg_tbl[i][0]+PMIC_WRAP_BASE))&pmic_wrap_reg_tbl[i][2])!=(RegWriteValue[j]&pmic_wrap_reg_tbl[i][2])))
        {
          PWRAPLOG("Register %x R/W test fail. write %x, read %x \r\n",(pmic_wrap_reg_tbl[i][0]+PMIC_WRAP_BASE),
            (RegWriteValue[j]&pmic_wrap_reg_tbl[i][2]),
            (*((volatile unsigned int *)(pmic_wrap_reg_tbl[i][0]+PMIC_WRAP_BASE)))&pmic_wrap_reg_tbl[i][2]);
           test_result++;
        }
      }
    }
  }
//#endif

#if 0
//init wrap
   //init wrap
	  WRAP_SET_BIT(0x80,INFRA_GLOBALCON_RST0);
	  WRAP_CLR_BIT(0x80,INFRA_GLOBALCON_RST0);


	 //###############################
	 //Enable DCM
	 //###############################
	  WRAP_WR32(PMIC_WRAP_DCM_EN , 1);
	  WRAP_WR32(PMIC_WRAP_DCM_DBC_PRD ,0);


	   //###############################
	   //Reset SPISLV
	   //###############################
	   sub_return=_pwrap_reset_spislv();
	   if( sub_return != 0 )
	   {
	   PWRAPERR("error,_pwrap_reset_spislv fail,sub_return=%x\n",sub_return);
	   return E_PWR_INIT_RESET_SPI;
	   }

	   //###############################
	   // SPI Waveform Configuration
	   //###############################
	   sub_return = _pwrap_init_reg_clock(2); //0:safe mode, 1:18MHz, 2:36MHz //2
	   if( sub_return != 0)  {
	   PWRAPERR("error,_pwrap_init_reg_clock fail,sub_return=%x\n",sub_return);
	   return E_PWR_INIT_REG_CLOCK;
	   }

	 //###############################
	 // Enable WACS0
	 //###############################
	 WRAP_WR32(PMIC_WRAP_WRAP_EN,1);//enable wrap
	 //WRAP_WR32(PMIC_WRAP_CSHEXT,0xf);
	 WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN,2); //Only WACS0
	 WRAP_WR32(PMIC_WRAP_WACS0_EN,1);


  for(i=0; i<DEW_reg_tbl_size; i++)
  {
    //Only R/W or RO should do default value test
    if(REG_TYP_WO!=DEW_reg_tbl[i][3])
    {
     _pwrap_wacs2_nochk(0, DEW_reg_tbl[i][0], 0, &reg_data);

      if((reg_data !=DEW_reg_tbl[i][1]))
      {
        PWRAPLOG("Register %x Default Value test fail. DEF %x, read %x \r\n",
        DEW_reg_tbl[i][0],  DEW_reg_tbl[i][1],  (*((volatile unsigned int *)DEW_reg_tbl[i][0])));
        test_result++;
      }
    }
  }
#if 0
  //sixian:test value:RegWriteValue[LCD_RegWriteValueSize] = {0, 0xFFFFFFFF, 0x55555555, 0xAAAAAAAA};
  for(i=0; i<DEW_reg_tbl_size; i++)
  {
    if(REG_TYP_RW==DEW_reg_tbl[i][3])
    {
      for(j=0; j<DEW_reg_tbl_size; j++)
      {
        *((volatile unsigned int *)DEW_reg_tbl[i][0]) = (RegWriteValue[j]&DEW_reg_tbl[i][2]);

        if(((*((volatile unsigned int *)DEW_reg_tbl[i][0])&DEW_reg_tbl[i][2])!=(RegWriteValue[j]&DEW_reg_tbl[i][2])))
        {
          PWRAPLOG("Register %x R/W test fail. write %x, read %x \r\n",DEW_reg_tbl[i][0],
            (RegWriteValue[j]&DEW_reg_tbl[i][2]),
            (*((volatile unsigned int *)PMIC_WRDEW_reg_tblAP_reg_tbl[i][0]))&DEW_reg_tbl[i][2]);
           test_result++;
        }
      }
    }
  }
#endif
  PWRAP_SOFT_RESET;
  regValue=WRAP_RD32(INFRA_GLOBALCON_RST0);
  PWRAPLOG("the reset register =%x\n",regValue);
  regValue=WRAP_RD32(PMIC_WRAP_STAUPD_GRPEN);
  PWRAPLOG("PMIC_WRAP_STAUPD_GRPEN =%x,it should be equal to 0x0\n",regValue);
  //clear reset bit
  PWRAP_CLEAR_SOFT_RESET_BIT;
#endif
  if(test_result==0)
  {
    PWRAPLOG("tc_reg_rw_test pass.\n");
  }
  else
  {
    PWRAPLOG("tc_reg_rw_test fail.res=%d\n",res);
  }
  return test_result;
}

S32 tc_mux_switch_test(  )
{
    UINT32 res=0;

    res=0;
    res=_pwrap_man_access_test();
    if(res==0)
    {
      PWRAPLOG("tc_mux_switch_test pass.\n");

    }
    else
    {
      PWRAPLOG("tc_mux_switch_test fail.res=%d\n",res);

    }
    return res;
}

S32 tc_reset_pattern_test(  )
{
    UINT32 res=0;

    res=pwrap_init();
    res=_pwrap_wrap_access_test();
    res=pwrap_init();
    res=_pwrap_wrap_access_test();
    if(res==0)
    {
      PWRAPLOG("tc_reset_pattern_test pass.\n");

    }
    else
    {
      PWRAPLOG("tc_reset_pattern_test fail.res=%d\n",res);

    }
    return res;
}


S32 tc_soft_reset_test(  )
{
    UINT32 res=0;
    UINT32 regValue=0;

    res=0;
    //---do wrap init and wrap access test-----------------------------------
    res=pwrap_init();
    res=_pwrap_wrap_access_test();
    //---reset wrap-------------------------------------------------------------
    PWRAPLOG("start reset wrapper\n");
    PWRAP_SOFT_RESET;
    regValue=WRAP_RD32(INFRA_GLOBALCON_RST0);
    PWRAPLOG("the reset register =%x\n",regValue);
    regValue=WRAP_RD32(PMIC_WRAP_STAUPD_GRPEN);
    PWRAPLOG("PMIC_WRAP_STAUPD_GRPEN =%x,it should be equal to 0x1\n",regValue);
    //clear reset bit
    PWRAP_CLEAR_SOFT_RESET_BIT;
    PWRAPLOG("the wrap access test should be fail after reset,before init\n");
    res=_pwrap_wrap_access_test();
    if(res==0)
      PWRAPLOG("_pwrap_wrap_access_test pass.\n");
    else
      PWRAPLOG("_pwrap_wrap_access_test fail.res=%d\n",res);
    PWRAPLOG("the wrap access test should be pass after reset and wrap init\n");

    //---do wrap init and wrap access test-----------------------------------
    res=pwrap_init();
    res=_pwrap_wrap_access_test();
    if(res==0)
      PWRAPLOG("_pwrap_wrap_access_test pass.\n");
    else
      PWRAPLOG("_pwrap_wrap_access_test fail.res=%d\n",res);
    if(res==0)
    {
      PWRAPLOG("tc_soft_reset_test pass.\n");

    }
    else
    {
      PWRAPLOG("tc_soft_reset_test fail.res=%d\n",res);

    }
    return res;
}

S32 tc_high_pri_test(  )
{
    U32 res=0;
    U32 rdata=0;
    U64 pre_time=0;
    U64 post_timer=0;
    U64 enable_staupd_time=0;
    U64 disable_staupd_time=0;
    U64 GPT2_COUNT_value=0;

    res=0;
    //----enable status updata and do wacs0-------------------------------------
    PWRAPLOG("enable status updata and do wacs0,record the cycle\n");
    WRAP_WR32(PMIC_WRAP_STAUPD_PRD, 0x1);  //0x1:20us,for concurrence test,MP:0x5;  //100us
    WRAP_WR32(PMIC_WRAP_STAUPD_GRPEN,0xff);
    //###############################
    // Read/Write test using WACS0
    //###############################
    //perfmon_start();//record time start,ldvt_follow_up
    GPT2_COUNT_value=WRAP_RD32(APXGPT_BASE + 0x0028);
    pre_time=sched_clock();
	PWRAPLOG("GPT2_COUNT_value=%lld pre_time=%lld\n", GPT2_COUNT_value,pre_time);
    pwrap_wacs0(0, DEW_READ_TEST, 0, &rdata);
    if( rdata != DEFAULT_VALUE_READ_TEST )
    {
      PWRAPERR("read test error(using WACS0),error code=%x, rdata=%x\n", 1, rdata);
      res+=1;
    }
    //perfmon_end();
    post_timer=sched_clock();
    enable_staupd_time=post_timer-pre_time;
	PWRAPLOG("pre_time=%lld post_timer=%lld\n", pre_time,post_timer);
	PWRAPLOG("pwrap_wacs0 enable_staupd_time=%lld\n", enable_staupd_time);

    //----disable status updata and do wacs0-------------------------------------
    PWRAPLOG("disable status updata and do wacs0,record the cycle\n");
    WRAP_WR32(PMIC_WRAP_STAUPD_PRD, 0xF);  //0x1:20us,for concurrence test,MP:0x5;  //100us
    WRAP_WR32(PMIC_WRAP_STAUPD_GRPEN,0x00);
    //###############################
    // Read/Write test using WACS0
    //###############################
    //perfmon_start();
    pre_time=sched_clock();
    pwrap_wacs0(0, DEW_READ_TEST, 0, &rdata);
    if( rdata != DEFAULT_VALUE_READ_TEST )
    {
      PWRAPERR("read test error(using WACS0),error code=%x, rdata=%x\n", 1, rdata);
      res+=1;
    }
    //perfmon_end();
    post_timer=sched_clock();
    disable_staupd_time=post_timer-pre_time;
	PWRAPLOG("pre_time=%lld post_timer=%lld\n", pre_time,post_timer);
	PWRAPLOG("pwrap_wacs0 disable_staupd_time=%lld\n", disable_staupd_time);
    if(disable_staupd_time<=enable_staupd_time)
    {
      PWRAPLOG("tc_high_pri_test pass.\n");

    }
    else
    {
      PWRAPLOG("tc_high_pri_test fail.res=%d\n",res);

    }
    return res;
}


S32 tc_spi_encryption_test(  )
{

    U32 res=0;
    U32 reg_value_backup=0;
    res=0;
    // disable status update,to check the waveform on oscilloscope
    WRAP_WR32(PMIC_WRAP_STAUPD_PRD, 0x0);  //0x0:disable
    //disable wdt int bit
    reg_value_backup=WRAP_RD32(PMIC_WRAP_INT_EN);
    WRAP_CLR_BIT(1<<0,PMIC_WRAP_INT_EN);
    //disable dio mode,single io wave
    _pwrap_switch_dio(0);
    //###############################
    // disable Encryption
    //###############################
    res = _pwrap_disable_cipher();//FPGA:set breakpoint here
    if( res != 0 )
    {
      PWRAPERR("disable Encryption error,error code=%x, rdata=%x", 0x21, res);
      return -EINVAL;
    }
    res=_pwrap_wrap_access_test();
    //###############################
    // enable Encryption
    //###############################
    res = _pwrap_enable_cipher();
    if( res != 0 )
    {
      PWRAPERR("Enable Encryption error,error code=%x, res=%x", 0x21, res);
      return -EINVAL;
    }
    res=_pwrap_wrap_access_test();
    if(res==0)
    {
      PWRAPLOG("tc_spi_encryption_test pass.\n");

    }
    else
    {
      PWRAPLOG("tc_spi_encryption_test fail.res=%d\n",res);

    }
    WRAP_WR32(PMIC_WRAP_STAUPD_PRD, reg_value_backup);  //0x0:disable
    return res;
}
//-------------------irq init start-------------------------------------
//CHOOSE_LISR=0:normal test;CHOOSE_LISR=1:watch dog test;
//CHOOSE_LISR=2:interrupt test
#define CHOOSE_LISR     1
#define NORMAL_TEST     1
#define WDT_TEST        2
//#define PERI_WDT_TEST   3
#define INT_TEST        4
//#define PERI_INT_TEST   5

U32 wrapper_lisr_count_cpu0=0;
U32 wrapper_lisr_count_cpu1=0;

static U32 int_test_bit=0;
static U32 wait_int_flag=0;
static U32 int_test_fail_count=0;

//global value for watch dog
static U32 wdt_test_bit=0;
static U32 wait_for_wdt_flag=0;
static U32 wdt_test_fail_count=0;

//global value for peri watch dog


S32 pwrap_interrupt_on_ldvt(void)
{
    struct pmic_wrap_obj *pwrap_obj = g_pmic_wrap_obj;


  //PWRAPFUC();
  switch(CHOOSE_LISR)
  {
    case NORMAL_TEST:
      pwrap_lisr_normal_test();
      break;
    case WDT_TEST:
      pwrap_lisr_for_wdt_test();
	  pwrap_obj->complete(pwrap_obj->context);
      break;
    case INT_TEST:
      pwrap_lisr_for_int_test();
	  pwrap_obj->complete(pwrap_obj->context);
      break;
  }
  //PWRAPLOG("before complete\n");
  //PWRAPLOG("after complete\n");
}

S32 pwrap_lisr_normal_test(void)
{
  U32 reg_int_flg=0;
  U32 reg_wdt_flg=0;
  //IRQMask(MT6583_PMIC_WRAP_IRQ_ID);
  PWRAPFUC();
//#ifndef ldvt_follow_up
    if (raw_smp_processor_id() == 0)
    {
      wrapper_lisr_count_cpu0++;
    } else if (raw_smp_processor_id() == 1)
    {
      wrapper_lisr_count_cpu1++;
    }
//#endif
  reg_int_flg=WRAP_RD32(PMIC_WRAP_INT_FLG);
  PWRAPLOG("PMIC_WRAP_INT_FLG=0x%x.\n",reg_int_flg);
  reg_wdt_flg=WRAP_RD32(PMIC_WRAP_WDT_FLG);
  PWRAPLOG("PMIC_WRAP_WDT_FLG=0x%x.\n",reg_wdt_flg);
  WRAP_WR32(PMIC_WRAP_WDT_SRC_EN, 0);//clear watch dog
  WRAP_WR32(PMIC_WRAP_WDT_SRC_EN,0xffffff);
  WRAP_WR32(PMIC_WRAP_INT_CLR, reg_int_flg);
  //IRQClearInt(MT6583_PMIC_WRAP_IRQ_ID);
  //IRQUnmask(MT6583_PMIC_WRAP_IRQ_ID);

}

S32 pwrap_lisr_for_wdt_test(void)
{
  U32 reg_int_flg=0;
  U32 reg_wdt_flg=0;
  //IRQMask(MT6583_PMIC_WRAP_IRQ_ID);
  PWRAPFUC();
  reg_int_flg=WRAP_RD32(PMIC_WRAP_INT_FLG);
  PWRAPLOG("PMIC_WRAP_INT_FLG=0x%x.\n",reg_int_flg);
  reg_wdt_flg=WRAP_RD32(PMIC_WRAP_WDT_FLG);
  PWRAPLOG("PMIC_WRAP_WDT_FLG=0x%x.\n",reg_wdt_flg);

  if((reg_int_flg & 0x1) !=0)
  {
    //dispatch_WDT();
    if((reg_wdt_flg & (1<<wdt_test_bit) )!= 0)
    {
      PWRAPLOG("watch dog test:recieve the right wdt.\n");
      wait_for_wdt_flag=1;
      //clear watch dog and interrupt
      WRAP_WR32(PMIC_WRAP_WDT_SRC_EN, 0);
      //WRAP_WR32(PMIC_WRAP_WDT_SRC_EN,0xffffff);
    }
    else
    {
      PWRAPLOG("fail watch dog test:recieve the wrong wdt.\n");
      wdt_test_fail_count++;
      //clear the unexpected watch dog and interrupt
      WRAP_WR32(PMIC_WRAP_WDT_SRC_EN,0 );
      WRAP_WR32(PMIC_WRAP_WDT_SRC_EN,1<<wdt_test_bit);
    }
  }

  WRAP_WR32(PMIC_WRAP_INT_CLR, reg_int_flg);
  //IRQClearInt(MT6583_PMIC_WRAP_IRQ_ID);
  //IRQUnmask(MT6583_PMIC_WRAP_IRQ_ID);
  reg_int_flg=WRAP_RD32(PMIC_WRAP_INT_FLG);
  PWRAPLOG("PMIC_WRAP_INT_FLG=0x%x.\n",reg_int_flg);
  reg_wdt_flg=WRAP_RD32(PMIC_WRAP_WDT_FLG);
  PWRAPLOG("PMIC_WRAP_WDT_FLG=0x%x.\n",reg_wdt_flg);
}

S32 pwrap_lisr_for_int_test(void)
{
  U32 reg_int_flg=0;
  U32 reg_wdt_flg=0;
  //IRQMask(MT6583_PMIC_WRAP_IRQ_ID);
  PWRAPFUC();
  reg_int_flg=WRAP_RD32(PMIC_WRAP_INT_FLG);
  PWRAPLOG("PMIC_WRAP_INT_FLG=0x%x.\n",reg_int_flg);
  reg_wdt_flg=WRAP_RD32(PMIC_WRAP_WDT_FLG);
  PWRAPLOG("PMIC_WRAP_WDT_FLG=0x%x.\n",reg_wdt_flg);

  //reg_peri_int_flg=WRAP_RD32(PERI_PWRAP_BRIDGE_INT_FLG);
  //PWRAPLOG("PERI_PWRAP_BRIDGE_INT_FLG=0x%x.\n",reg_peri_int_flg);
  //reg_peri_wdt_flg=WRAP_RD32(PERI_PWRAP_BRIDGE_WDT_FLG);
  //PWRAPLOG("PERI_PWRAP_BRIDGE_WDT_FLG=0x%x.\n",reg_peri_wdt_flg);
  //-------------------------interrupt test---------------
  PWRAPLOG("int_test_bit=0x%x.\n",int_test_bit);
  if((reg_int_flg & (1<<int_test_bit)) != 0)
  {
    PWRAPLOG(" int test:recieve the right pwrap interrupt.\n");
    wait_int_flag=1;
  }
  else
  {
    PWRAPLOG(" int test fail:recieve the wrong pwrap interrupt.\n");
    int_test_fail_count++;
  }
  WRAP_WR32(PMIC_WRAP_INT_CLR, reg_int_flg);
  reg_int_flg=WRAP_RD32(PMIC_WRAP_INT_FLG);
  PWRAPLOG("PMIC_WRAP_INT_FLG=0x%x.\n",reg_int_flg);
  reg_wdt_flg=WRAP_RD32(PMIC_WRAP_WDT_FLG);
  PWRAPLOG("PMIC_WRAP_WDT_FLG=0x%x.\n",reg_wdt_flg);

  //for int test bit[1]
  WRAP_WR32(PMIC_WRAP_STAUPD_GRPEN, 0);
  //WRAP_WR32(PERI_PWRAP_BRIDGE_INT_FLG, reg_peri_int_flg);
  //IRQClearInt(MT6583_PMIC_WRAP_IRQ_ID);
  //IRQUnmask(MT6583_PMIC_WRAP_IRQ_ID);

}

//-------------------irq init end-------------------------------------------

//-------------------watch dog test start-------------------------------------
U32 watch_dog_test_reg=DEW_WRITE_TEST;

//static U32 wrap_WDT_flg=0;


//#define ENABLE_INT_ON_CTP

S32 _wdt_test_disable_other_int(void)
{
  //disable watch dog
  WRAP_WR32(PMIC_WRAP_INT_EN,0x1);
   return 0;
}

//[1]: HARB_WACS0_ALE: HARB to WACS0 ALE timeout monitor
//disable the corresponding bit in HIPRIO_ARB_EN,and send a WACS0 write command
S32 _wdt_test_bit1( )
{
  U32 rdata=0;
  U32 reg_rdata=0;
  U32 res=0;
  PWRAPFUC();
  wait_for_wdt_flag=0;
  wdt_test_bit=1;
  //disable corresponding bit in PMIC_WRAP_HIPRIO_ARB_EN
  WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN,0x1ff);
  WRAP_CLR_BIT(1<<wdt_test_bit,PMIC_WRAP_HIPRIO_ARB_EN);
  pwrap_wacs0(1, watch_dog_test_reg, 0x1234, &rdata);

  wait_for_completion(&pwrap_done);
  while(wait_for_wdt_flag!=1);
  PWRAPLOG("_wdt_test_bit1 pass.\n");
  return 0;

}

//[2]: HARB_WACS1_ALE: HARB to WACS1 ALE timeout monitor
//disable the corresponding bit in HIPRIO_ARB_EN,and send a WACS1 write command
S32 _wdt_test_bit2( )
{
  U32 rdata=0;
  U32 reg_rdata=0;
  U32 res=0;
  PWRAPFUC();
  wdt_test_bit=2;
  wait_for_wdt_flag=0;
  //disable corresponding bit in PMIC_WRAP_HIPRIO_ARB_EN
  WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN,0x1ff);
  WRAP_CLR_BIT(1<<wdt_test_bit,PMIC_WRAP_HIPRIO_ARB_EN);
  pwrap_wacs1(1, watch_dog_test_reg, 0x1234, &rdata);
  wait_for_completion(&pwrap_done);
  while(wait_for_wdt_flag!=1);
  PWRAPLOG("_wdt_test_bit2 pass.\n");
  return 0;

  return res;
}

//[3]: HARB_WACS2_ALE: HARB to WACS2 ALE timeout monitor
//disable the corresponding bit in HIPRIO_ARB_EN,and send a WACS3 write command
S32 _wdt_test_bit3( )
{
  U32 rdata=0;
  U32 reg_rdata=0;
  U32 res=0;
  PWRAPFUC();
  wdt_test_bit=3;
  wait_for_wdt_flag=0;
  //disable corresponding bit in PMIC_WRAP_HIPRIO_ARB_EN
  WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN,0x1ff);
  WRAP_CLR_BIT(1<<wdt_test_bit,PMIC_WRAP_HIPRIO_ARB_EN);
  pwrap_write(watch_dog_test_reg, 0x1234);

  wait_for_completion(&pwrap_done);
  while(wait_for_wdt_flag!=1);
  PWRAPLOG("_wdt_test_bit3 pass.\n");
  return 0;
}

//[5]: HARB_ERC_ALE: HARB to ERC ALE timeout monitor
//disable the corresponding bit in HIPRIO_ARB_EN,do event test
S32 _wdt_test_bit5( )
{
  U32 rdata=0;
  U32 reg_rdata=0;
  U32 res=0;
  PWRAPFUC();
  wdt_test_bit=6;
  wait_for_wdt_flag=0;
  //disable corresponding bit in PMIC_WRAP_HIPRIO_ARB_EN
  WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN,0x1ff);
  WRAP_CLR_BIT(1<<wdt_test_bit,PMIC_WRAP_HIPRIO_ARB_EN);
  //similar to event  test case
  //WRAP_WR32(PMIC_WRAP_EVENT_STACLR , 0xffff);

  //res=pwrap_wacs0(1, DEW_EVENT_TEST, 0x1, &rdata);

  wait_for_completion(&pwrap_done);
  while(wait_for_wdt_flag!=1);
  PWRAPLOG("_wdt_test_bit5 pass.\n");
  return 0;
}

//[6]: HARB_STAUPD_ALE: HARB to STAUPD ALE timeout monitor
//disable the corresponding bit in HIPRIO_ARB_EN,and send a WACS1 write command
S32 _wdt_test_bit6( )
{
  U32 rdata=0;
  U32 reg_rdata=0;
  U32 res=0;
  PWRAPFUC();
  wdt_test_bit=6;
  wait_for_wdt_flag=0;
  //disable corresponding bit in PMIC_WRAP_HIPRIO_ARB_EN
  WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN,0x1ff);
  WRAP_CLR_BIT(1<<5,PMIC_WRAP_HIPRIO_ARB_EN);
  //similar to status updata test case
  //pwrap_wacs0(1, DEW_WRITE_TEST, 0x55AA, &rdata);
  //WRAP_WR32(PMIC_WRAP_SIG_ADR,DEW_WRITE_TEST);
  //WRAP_WR32(PMIC_WRAP_SIG_VALUE,0xAA55);
  WRAP_WR32(PMIC_WRAP_STAUPD_MAN_TRIG,0x1);
  wait_for_completion(&pwrap_done);
  while(wait_for_wdt_flag!=1);
  PWRAPLOG("_wdt_test_bit6 pass.\n");
  WRAP_WR32(PMIC_WRAP_SIG_VALUE,0x55AA);//tha same as write test
  return 0;
}

//[7]: PWRAP_PERI_ALE: HARB to PWRAP_PERI_BRIDGE ALE timeout monitor
//disable the corresponding bit in HIPRIO_ARB_EN,and send a WACS3 write command
S32 _wdt_test_bit7( )
{
  U32 rdata=0;
  U32 reg_rdata=0;
  U32 res=0;
  PWRAPFUC();
  wdt_test_bit=7;
  wait_for_wdt_flag=0;
  //disable corresponding bit in PMIC_WRAP_HIPRIO_ARB_EN
  WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN,0x1ff);
  WRAP_CLR_BIT(1<<wdt_test_bit,PMIC_WRAP_HIPRIO_ARB_EN);
  //do wacs3
  //pwrap_wacs3(1, watch_dog_test_reg, 0x55AA, &rdata);
  wait_for_completion(&pwrap_done);

  while(wait_for_wdt_flag!=1);
  PWRAPLOG("_wdt_test_bit7 pass.\n");
  return 0;
}

//[8]: HARB_EINTBUF_ALE: HARB to EINTBUF ALE timeout monitor
//disable the corresponding bit in HIPRIO_ARB_EN,and send a eint interrupt
S32 _wdt_test_bit8( )
{
  U32 rdata=0;
  U32 reg_rdata=0;
  U32 res=0;
  PWRAPFUC();
  wdt_test_bit=8;
  wait_for_wdt_flag=0;
  #ifdef ENABLE_KEYPAD_ON_LDVT
    //kepadcommand
    *((volatile kal_uint16 *)(KP_BASE + 0x1c)) = 0x1;
    kpd_init();
    initKpdTest();
  #endif
  //disable corresponding bit in PMIC_WRAP_HIPRIO_ARB_EN
  WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN,0x1ff);
  WRAP_CLR_BIT(1<<wdt_test_bit,PMIC_WRAP_HIPRIO_ARB_EN);
  wait_for_completion(&pwrap_done);
  while(wait_for_wdt_flag!=1);
  PWRAPLOG("_wdt_test_bit8 pass.\n");
  return 0;
}

//[9]: WRAP_HARB_ALE: WRAP to HARB ALE timeout monitor
//disable RRARB_EN[0],and do eint test
S32 _wdt_test_bit9( )
{
  U32 rdata=0;
  U32 reg_rdata=0;
  U32 res=0,i=0;
  PWRAPFUC();
  wdt_test_bit=9;
  wait_for_wdt_flag=0;



#ifdef ENABLE_EINT_ON_LDVT
  //eint_init();
  //_concurrence_eint_test_code(eint_in_cpu0);
  //eint_unmask(eint_in_cpu0);
  //Delay(500);
#endif
  //disable wrap_en
  //WRITE_REG(0xFFFFFFFF, EINT_MASK_CLR);



  //Delay(1000);
   for (i=0;i<(300*20);i++);
   wait_for_completion(&pwrap_done);
  while(wait_for_wdt_flag!=1);
  //WRAP_WR32(PMIC_WRAP_WRAP_EN ,1);//recover
  PWRAPLOG("_wdt_test_bit9 pass.\n");
  return 0;
}

//[10]: PWRAP_AG_ALE#1: PWRAP to AG#1 ALE timeout monitor
//disable RRARB_EN[1],and do keypad test
S32 _wdt_test_bit10( )
{
  U32 rdata=0;
  U32 reg_rdata=0;
  U32 res=0;
  PWRAPFUC();
  wdt_test_bit=10;
  wait_for_wdt_flag=0;
#ifdef ENABLE_KEYPAD_ON_LDVT
  //kepad command
  *((volatile kal_uint16 *)(KP_BASE + 0x1c)) = 0x1;
  kpd_init();
  initKpdTest();
#endif
  //disable wrap_en
  //WRAP_CLR_BIT(1<<1 ,PMIC_WRAP_RRARB_EN);
  wait_for_completion(&pwrap_done);
  //push keypad key
  while(wait_for_wdt_flag!=1);
  PWRAPLOG("_wdt_test_bit10 pass.\n");
  return 0;
}

//[11]: PWRAP_AG_ALE#2: PWRAP to AG#2 ALE timeout monitor
//disable RRARB_EN[0],and do eint test
S32 _wdt_test_bit11( )
{
  U32 rdata=0;
  U32 reg_rdata=0;
  U32 res=0;
  PWRAPFUC();
  wdt_test_bit=11;
  wait_for_wdt_flag=0;
  //kepadcommand
#ifdef ENABLE_EINT_ON_LDVT
  //eint_init();
  //_concurrence_eint_test_code(eint_in_cpu0);
  //eint_unmask(eint_in_cpu0);
#endif

  //disable wrap_en
  //WRAP_CLR_BIT(1<<1 ,PMIC_WRAP_RRARB_EN);
  wait_for_completion(&pwrap_done);
  //push keypad key
  while(wait_for_wdt_flag!=1);
  PWRAPLOG("_wdt_test_bit11 pass.\n");
  return 0;
}

//[12]: wrap_HARB_ALE: WRAP to harb ALE timeout monitor
//  ,disable wrap_en and set a WACS0 read command
S32 _wdt_test_bit12( )
{
  U32 rdata=0;
  U32 reg_rdata=0;
  U32 res=0;
  PWRAPFUC();
  wdt_test_bit=12;
  wait_for_wdt_flag=0;

  //_pwrap_switch_mux(1);//manual mode
  //WRAP_WR32(PMIC_WRAP_MUX_SEL , 0);
  WRAP_WR32(PMIC_WRAP_WRAP_EN , 0);//disble wrap
  //WRAP_WR32(PMIC_WRAP_MAN_EN , 1);//enable manual
  pwrap_wacs0(0, watch_dog_test_reg, 0, &rdata);
  wait_for_completion(&pwrap_done);

  while(wait_for_wdt_flag!=1);
  //_pwrap_switch_mux(0);// recover

  PWRAPLOG("_wdt_test_bit12 pass.\n");
  return 0;

}

//[13]: MUX_WRAP_ALE: MUX to WRAP ALE timeout monitor
// set MUX to manual mode ,enable wrap_en and set a WACS0 read command
S32 _wdt_test_bit13( )
{
  U32 rdata=0;
  U32 reg_rdata=0;
  U32 res=0;
  PWRAPFUC();
  wdt_test_bit=13;
  wait_for_wdt_flag=0;

  _pwrap_switch_mux(1);//manual mode
  //WRAP_WR32(PMIC_WRAP_MUX_SEL , 0);
  WRAP_WR32(PMIC_WRAP_WRAP_EN , 1);//enable wrap
  //WRAP_WR32(PMIC_WRAP_MAN_EN , 1);//enable manual
  pwrap_wacs0(0, watch_dog_test_reg, 0, &rdata);
  wait_for_completion(&pwrap_done);

  while(wait_for_wdt_flag!=1);
  _pwrap_switch_mux(0);// recover

  PWRAPLOG("_wdt_test_bit13 pass.\n");
  return 0;

}

//[14]: MUX_MAN_ALE: MUX to MAN ALE timeout monitor
//MUX to MAN ALE:set MUX to wrap mode and set manual command
S32 _wdt_test_bit14( )
{
  U32 rdata=0;
  U32 reg_rdata=0;
  U32 res=0;
  PWRAPFUC();
  wdt_test_bit=14;
  wait_for_wdt_flag=0;
  _pwrap_switch_mux(0);//wrap mode
  //WRAP_WR32(PMIC_WRAP_WRAP_EN , 0);//enable wrap
  WRAP_WR32(PMIC_WRAP_MAN_EN , 1);//enable manual

  _pwrap_manual_mode(0,  OP_IND, 0, &rdata);
  wait_for_completion(&pwrap_done);

  while(wait_for_wdt_flag!=1);
  PWRAPLOG("_wdt_test_bit14 pass.\n");
  _pwrap_switch_mux(1);//
  return 0;
}

//[16]: HARB_WACS0_DLE: HARB to WACS0 DLE timeout monitor
//HARB to WACS0 DLE:disable MUX,and send a read commnad with WACS0
S32 _wdt_test_bit16( )
{
  U32 rdata=0;
  U32 reg_rdata=0;
  U32 res=0;
  PWRAPFUC();
  wdt_test_bit=16;
  wait_for_wdt_flag=0;

  //disable other wdt bit
  //WRAP_WR32(PMIC_WRAP_WDT_SRC_EN,0);
  //WRAP_WR32(PMIC_WRAP_WDT_SRC_EN,1<<wdt_test_bit);
  reg_rdata=WRAP_RD32(PMIC_WRAP_WDT_SRC_EN);
  PWRAPLOG("PMIC_WRAP_WDT_SRC_EN=%x.\n",reg_rdata);

  WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN , 1<<1);//enable wrap
  reg_rdata=WRAP_RD32(PMIC_WRAP_HIPRIO_ARB_EN);
  PWRAPLOG("PMIC_WRAP_WDT_SRC_EN=%x.\n",reg_rdata);
  //set status update period to the max value,or disable status update
  WRAP_WR32(PMIC_WRAP_STAUPD_PRD, 0xF);

  _pwrap_switch_mux(1);//manual mode
  WRAP_WR32(PMIC_WRAP_WRAP_EN , 1);//enable wrap
  //read command
  pwrap_wacs0(0, watch_dog_test_reg, 0, &rdata);
  wait_for_completion(&pwrap_done);

  while(wait_for_wdt_flag!=1);
  PWRAPLOG("_wdt_test_bit16 pass.\n");
  _pwrap_switch_mux(0);//recover
  return 0;
}


//[17]: HARB_WACS1_DLE: HARB to WACS1 DLE timeout monitor
//HARB to WACS1 DLE:disable MUX,and send a read commnad with WACS1
S32 _wdt_test_bit17( )
{
  U32 rdata=0;
  U32 reg_rdata=0;
  U32 res=0;
  PWRAPFUC();
  wdt_test_bit=17;
  wait_for_wdt_flag=0;

  WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN , 1<<2);//enable wrap
  reg_rdata=WRAP_RD32(PMIC_WRAP_HIPRIO_ARB_EN);
  PWRAPLOG("PMIC_WRAP_HIPRIO_ARB_EN=%x.\n",reg_rdata);
  //set status update period to the max value,or disable status update
  WRAP_WR32(PMIC_WRAP_STAUPD_PRD, 0x0);

  _pwrap_switch_mux(1);//manual mode
  WRAP_WR32(PMIC_WRAP_WRAP_EN , 1);//enable wrap
  //read command
  pwrap_wacs1(0, watch_dog_test_reg, 0, &rdata);
  wait_for_completion(&pwrap_done);
  while(wait_for_wdt_flag!=1);
  PWRAPLOG("_wdt_test_bit17 pass.\n");
  _pwrap_switch_mux(0);//recover
  return 0;
}
//[18]: HARB_WACS2_DLE: HARB to WACS1 DLE timeout monitor
//HARB to WACS2 DLE:disable MUX,and send a read commnad with WACS2
S32 _wdt_test_bit18( )
{
  U32 rdata=0;
  U32 reg_rdata=0;
  U32 res=0;
  PWRAPFUC();
  wdt_test_bit=18;
  wait_for_wdt_flag=0;

  WRAP_WR32(PMIC_WRAP_WDT_SRC_EN,0);
  WRAP_WR32(PMIC_WRAP_WDT_SRC_EN,1<<wdt_test_bit);
  reg_rdata=WRAP_RD32(PMIC_WRAP_WDT_SRC_EN);
  PWRAPLOG("PMIC_WRAP_HIPRIO_ARB_EN=%x.\n",reg_rdata);

  WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN , 1<<3);
  reg_rdata=WRAP_RD32(PMIC_WRAP_HIPRIO_ARB_EN);
  PWRAPLOG("PMIC_WRAP_HIPRIO_ARB_EN=%x.\n",reg_rdata);
  //set status update period to the max value,or disable status update
  WRAP_WR32(PMIC_WRAP_STAUPD_PRD, 0xF);

  reg_rdata=WRAP_RD32(PMIC_WRAP_STAUPD_PRD);
  PWRAPLOG("PMIC_WRAP_STAUPD_PRD=%x.\n",reg_rdata);

  _pwrap_switch_mux(1);//manual mode
  WRAP_WR32(PMIC_WRAP_WRAP_EN , 1);//enable wrap
  //clear INT
  WRAP_WR32(PMIC_WRAP_INT_CLR, 0xFFFFFFFF);

  //read command
  pwrap_read(watch_dog_test_reg,&rdata);
  wait_for_completion(&pwrap_done);
  while(wait_for_wdt_flag!=1);
  PWRAPLOG("_wdt_test_bit18 pass.\n");
  _pwrap_switch_mux(0);//recover
  return 0;
}

//[19]: HARB_ERC_DLE: HARB to ERC DLE timeout monitor
//HARB to staupda DLE:disable event,write de_wrap event test,then swith mux to manual mode ,enable wrap_en enable event
//similar to bit5
S32 _wdt_test_bit19( )
{
  U32 rdata=0;
  U32 reg_rdata=0;
  U32 res=0;
  PWRAPFUC();
  wdt_test_bit=19;
  wait_for_wdt_flag=0;
  //disable event
  //WRAP_WR32(PMIC_WRAP_EVENT_IN_EN , 0);

  //do event test
  //WRAP_WR32(PMIC_WRAP_EVENT_STACLR , 0xffff);
  //res=pwrap_wacs0(1, DEW_EVENT_TEST, 0x1, &rdata);
  //disable mux
  _pwrap_switch_mux(1);//manual mode
  WRAP_WR32(PMIC_WRAP_WRAP_EN , 1);//enable wrap
  //enable event
  //WRAP_WR32(PMIC_WRAP_EVENT_IN_EN , 1);
  wait_for_completion(&pwrap_done);

  while(wait_for_wdt_flag!=1);
  PWRAPLOG("_wdt_test_bit19 pass.\n");
  _pwrap_switch_mux(0);//recover
  return 0;
}

//[20]: HARB_STAUPD_DLE: HARB to STAUPD DLE timeout monitor
//  HARB to staupda DLE:disable MUX,then send a read commnad ,and do status update test
//similar to bit6
S32 _wdt_test_bit20( )
{
  U32 rdata=0;
  U32 reg_rdata=0;
  U32 res=0;
  PWRAPFUC();
  wdt_test_bit=20;
  wait_for_wdt_flag=0;
  _pwrap_switch_mux(1);//manual mode
  WRAP_WR32(PMIC_WRAP_WRAP_EN , 1);//enable wrap
  //similar to status updata test case
  pwrap_wacs0(1, DEW_WRITE_TEST, 0x55AA, &rdata);
  WRAP_WR32(PMIC_WRAP_SIG_ADR,DEW_WRITE_TEST);
  WRAP_WR32(PMIC_WRAP_SIG_VALUE,0xAA55);
  wait_for_completion(&pwrap_done);

  while(wait_for_wdt_flag!=1);
  PWRAPLOG("_wdt_test_bit20 pass.\n");
  _pwrap_switch_mux(0);//recover
  WRAP_WR32(PMIC_WRAP_SIG_VALUE,0x55AA);//tha same as write test

  return 0;
}
//[21]: HARB_RRARB_DLE: HARB to RRARB DLE timeout monitor HARB to RRARB DLE
//:disable MUX,do keypad test
S32 _wdt_test_bit21( )
{
  U32 rdata=0;
  U32 reg_rdata=0;
  U32 res=0;
  U32 reg_backup;
  PWRAPFUC();
  wdt_test_bit=21;
  wait_for_wdt_flag=0;
#ifdef ENABLE_KEYPAD_ON_LDVT
  //kepad command
  *((volatile kal_uint16 *)(KP_BASE + 0x1c)) = 0x1;
  kpd_init();
  initKpdTest();
#endif
  //WRAP_WR32(PMIC_WRAP_WDT_SRC_EN,0 );
  //WRAP_WR32(PMIC_WRAP_WDT_SRC_EN,1<<wdt_test_bit);
  reg_backup=WRAP_RD32(PMIC_WRAP_HIPRIO_ARB_EN);
  WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN , 0x80);//only enable keypad
  _pwrap_switch_mux(1);//manual mode
  WRAP_WR32(PMIC_WRAP_WRAP_EN , 1);//enable wrap
  WRAP_WR32(0x10016020 , 0);//write keypad register,to send a keypad read request
  wait_for_completion(&pwrap_done);

  while(wait_for_wdt_flag!=1);
  PWRAPLOG("_wdt_test_bit21 pass.\n");
  WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN , reg_backup);

  _pwrap_switch_mux(0);//recover
  return 0;
}


//[22]: MUX_WRAP_DLE: MUX to WRAP DLE timeout monitor
//MUX to WRAP DLE:disable MUX,then send a read commnad ,and do WACS0
S32 _wdt_test_bit22( )
{
  U32 rdata=0;
  U32 reg_rdata=0;
  U32 res=0;
  PWRAPFUC();
  wdt_test_bit=22;
  wait_for_wdt_flag=0;
  //WRAP_WR32(PMIC_WRAP_WRAP_EN , 1);//enable wrap
  pwrap_wacs1(0, watch_dog_test_reg, 0, &rdata);
  _pwrap_switch_mux(1);//manual mode
  WRAP_WR32(PMIC_WRAP_WRAP_EN , 1);//enable wrap
  wait_for_completion(&pwrap_done);

  while(wait_for_wdt_flag!=1);
  PWRAPLOG("_wdt_test_bit22 pass.\n");
  _pwrap_switch_mux(0);//recover
  return 0;
}

//[23]: MUX_MAN_DLE: MUX to MAN DLE timeout monitor
//MUX to MAN DLE:disable MUX,then send a read commnad in manual mode
S32 _wdt_test_bit23( )
{
  U32 rdata=0;
  U32 reg_rdata=0;
  U32 res=0;
  U32 return_value=0;
  PWRAPFUC();
  wdt_test_bit=23;
  wait_for_wdt_flag=0;

  WRAP_WR32(PMIC_WRAP_WDT_SRC_EN,0);
  WRAP_WR32(PMIC_WRAP_WDT_SRC_EN,1<<wdt_test_bit);
  reg_rdata=WRAP_RD32(PMIC_WRAP_WDT_SRC_EN);
  PWRAPLOG("PMIC_WRAP_WDT_SRC_EN=%x.\n",reg_rdata);



  return_value=_pwrap_switch_mux(1);//manual mode
  PWRAPLOG("_pwrap_switch_mux return_value=%x.\n",return_value);

    WRAP_WR32(PMIC_WRAP_SI_CK_CON,0x6);
  reg_rdata=WRAP_RD32(PMIC_WRAP_SI_CK_CON);
  PWRAPLOG("PMIC_WRAP_SI_CK_CON=%x.\n",reg_rdata);

  return_value=_pwrap_manual_mode(0,  OP_IND, 0, &rdata);
  PWRAPLOG("_pwrap_manual_mode return_value=%x.\n",return_value);

  wait_for_completion(&pwrap_done);

  while(wait_for_wdt_flag!=1);
  PWRAPLOG("_wdt_test_bit23 pass.\n");
  _pwrap_switch_mux(0);//recover
  return 0;
}

//[24]: MSTCTL_SYNC_DLE: MSTCTL to SYNC DLE timeout monitor
//MSTCTL to SYNC  DLE:disable sync,then send a read commnad with wacs0
S32 _wdt_test_bit24( )
{
  U32 rdata=0;
  U32 reg_rdata=0;
  U32 res=0;
  PWRAPFUC();
  wdt_test_bit=24;
  wait_for_wdt_flag=0;
  _pwrap_switch_mux(1);//manual mode
  wait_for_completion(&pwrap_done);
  while(wait_for_wdt_flag!=1);
  PWRAPLOG("_wdt_test_bit24 pass.\n");
  _pwrap_switch_mux(0);//recover
  return 0;
}

//[25]: STAUPD_TRIG:
//set period=0
S32 _wdt_test_bit25( )
{
  U32 rdata=0;
  U32 reg_rdata=0;
  U32 res=0;
  PWRAPFUC();
  wdt_test_bit=25;
  wait_for_wdt_flag=0;
  WRAP_WR32(PMIC_WRAP_STAUPD_PRD, 0x0);  //0x1:20us,for concurrence test,MP:0x5;  //100us
  wait_for_completion(&pwrap_done);
  while(wait_for_wdt_flag!=1);
  PWRAPLOG("_wdt_test_bit25 pass.\n");
  return 0;
}

//[26]: PREADY: APB PREADY timeout monitor
//disable wrap_en and write wacs0 6 times
S32 _wdt_test_bit26( )
{
  U32 rdata=0;
  U32 wdata=0;
  U32 reg_rdata=0;
  U32 i=0;
  U32 wacs_write=0;
  U32 wacs_adr=0;
  U32 wacs_cmd=0;
  U32 return_value=0;
  PWRAPFUC();
  wdt_test_bit=26;
  wait_for_wdt_flag=0;
  WRAP_WR32(PMIC_WRAP_WDT_SRC_EN, 0);
  WRAP_WR32(PMIC_WRAP_WDT_SRC_EN,1<<wdt_test_bit);

  WRAP_WR32(PMIC_WRAP_WRAP_EN , 0);//disable wrap
  for(i=0;i<10;i++)
  {
    wdata+=0x20;
    //pwrap_wacs0(1, watch_dog_test_reg, wdata, &rdata);
    WRAP_WR32(PMIC_WRAP_WACS0_CMD,0x10000000);
    PWRAPLOG("send %d command .\n",i);
  }
  wait_for_completion(&pwrap_done);

  while(wait_for_wdt_flag!=1);
  PWRAPLOG("_wdt_test_bit26 pass.\n");
  return 0;
}

#define test_fail
S32 tc_wdt_test(  )
{
    UINT32 return_value=0;
    UINT32 result=0;
    UINT32 reg_data=0;
    //struct pmic_wrap_obj *pwrap_obj = g_pmic_wrap_obj;
  //pwrap_obj->complete = pwrap_complete;
  //pwrap_obj->context = &pwrap_done;

    //enable watch dog
    WRAP_WR32(PMIC_WRAP_WDT_SRC_EN,0xffffff);
    //WRAP_WR32(PERI_PWRAP_BRIDGE_WDT_SRC_EN , 0xffff);
#if 0
    return_value=_wdt_test_bit1();
    return_value=pwrap_init();
    _wdt_test_disable_other_int();

    return_value=_wdt_test_bit2();
    return_value=pwrap_init();
    _wdt_test_disable_other_int();

    return_value=_wdt_test_bit3();
    return_value=pwrap_init();
    _wdt_test_disable_other_int();

    //return_value=_wdt_test_bit5();
    return_value=pwrap_init();
    _wdt_test_disable_other_int();

    return_value=_wdt_test_bit6();//fail
    return_value=pwrap_init();
    _wdt_test_disable_other_int();

    //return_value=_wdt_test_bit7();
    //return_value=pwrap_init();
    //_wdt_test_disable_other_int();
#if 0
    return_value=_wdt_test_bit8();
    return_value=pwrap_init();
    _wdt_test_disable_other_int();

    return_value=_wdt_test_bit9();//eint
    return_value=pwrap_init();
    _wdt_test_disable_other_int();



    return_value=_wdt_test_bit10();//eint
    return_value=pwrap_init();
    _wdt_test_disable_other_int();

    //return_value=_wdt_test_bit11();//eint
    return_value=pwrap_init();
    _wdt_test_disable_other_int();

    reg_data=WRAP_RD32(PMIC_WRAP_WDT_SRC_EN);
  PWRAPLOG("PMIC_WRAP_WDT_SRC_EN=%x.\n",reg_data);
#endif
#ifdef followup
    return_value=_wdt_test_bit12(); //need to add timeout
    return_value=pwrap_init();
    _wdt_test_disable_other_int();

    return_value=_wdt_test_bit13();
    return_value=pwrap_init();
    _wdt_test_disable_other_int();


  reg_data=WRAP_RD32(PMIC_WRAP_INT_FLG);
  PWRAPLOG("wrap_int_flg=%x.\n",reg_data);
  reg_data=WRAP_RD32(PMIC_WRAP_WDT_FLG);
  PWRAPLOG("PMIC_WRAP_WDT_FLG=%x.\n",reg_data);

    return_value=_wdt_test_bit14();
    return_value=pwrap_init();
    _wdt_test_disable_other_int();



    //return_value=_wdt_test_bit15();
    //return_value=pwrap_init();
    //_wdt_test_disable_other_int();
#endif

    //return_value=_wdt_test_bit16();//pass
    return_value=pwrap_init();
    _wdt_test_disable_other_int();


#if 0
    //return_value=_wdt_test_bit17();
    return_value=pwrap_init();
    _wdt_test_disable_other_int();
#endif
#endif

    return_value=_wdt_test_bit18();
    return_value=pwrap_init();
    _wdt_test_disable_other_int();

    //return_value=_wdt_test_bit19();
    //return_value=pwrap_init();
    //_wdt_test_disable_other_int();

    return_value=_wdt_test_bit20();
    return_value=pwrap_init();
    _wdt_test_disable_other_int();
#if 0

    return_value=_wdt_test_bit21();
    return_value=pwrap_init();
    _wdt_test_disable_other_int();
#endif
    //return_value=_wdt_test_bit23(); //pass
    return_value=pwrap_init();
    _wdt_test_disable_other_int();

    //return_value=_wdt_test_bit24();
    //return_value=pwrap_init();
    //_wdt_test_disable_other_int();

    return_value=_wdt_test_bit25();
    return_value=pwrap_init();
    _wdt_test_disable_other_int();

    return_value=_wdt_test_bit26();//cann't test
    return_value=pwrap_init();
    _wdt_test_disable_other_int();
    PWRAPLOG("wdt_test_fail_count=%d.\n",wdt_test_fail_count);
    if(result==0)
    {
      PWRAPLOG("tc_wdt_test pass.\n");
    }
    else
    {
      PWRAPLOG("tc_wdt_test fail.res=%d\n",result);
    }
    return result;
}
//-------------------watch dog test end-------------------------------------

//start----------------interrupt test ------------------------------------
U32 interrupt_test_reg=DEW_WRITE_TEST;
//[1]:  SIG_ERR: Signature Checking failed.	set bit[0]=1 in cmd
S32 _int_test_bit1( )
{
  U32 rdata=0;
  U32 reg_rdata=0;
  U32 res=0;
  U32 return_value=0;
  U32 wacs_write=0;
  U32 wacs_adr=0;
  U32 wacs_cmd=0;
  U32 addr=WRAP_ACCESS_TEST_REG;
  PWRAPFUC();
  int_test_bit=1;
  wait_int_flag=0;
#if 1 //CRC mode
  WRAP_WR32(PMIC_WRAP_ADC_RDY_ADDR,0x1);
#endif

#if 0 //sig_value mode
  pwrap_write(DEW_WRITE_TEST, 0x55AA);
  WRAP_WR32(PMIC_WRAP_SIG_ADR,DEW_WRITE_TEST);
  WRAP_WR32(PMIC_WRAP_SIG_VALUE,0xAA55);
  WRAP_WR32(PMIC_WRAP_SIG_MODE, 0x1);

  pwrap_delay_us(5000);//delay 5 seconds
  rdata=WRAP_RD32(PMIC_WRAP_SIG_ERRVAL);
  if( rdata != 0x55AA )
  {
    PWRAPERR("_pwrap_status_update_test error,error code=%x, rdata=%x", 1, rdata);
    //return 1;
  }
  //WRAP_WR32(PMIC_WRAP_SIG_VALUE,0x55AA);//tha same as write test
  //clear sig_error interrupt flag bit
  //WRAP_WR32(PMIC_WRAP_INT_CLR,1<<1);
#endif
  wait_for_completion(&pwrap_done);
  if(wait_int_flag==1)
    PWRAPLOG("_int_test_bit1 pass.\n");
  else
    PWRAPLOG("_int_test_bit1 fail.\n");
  return 0;
}


//[5]:  MAN_CMD_MISS: A MAN CMD is written while MAN is disabled.
//    disable man,send a manual command
S32 _int_test_bit5( )
{
  U32 rdata=0;
  U32 reg_rdata=0;
  U32 res=0;
  U32 return_value=0;
  PWRAPFUC();
  int_test_bit=5;
  wait_int_flag=0;
  WRAP_WR32(PMIC_WRAP_MAN_EN , 0);// disable man

  return_value=_pwrap_manual_mode(OP_WR,  OP_CSH,  0, &rdata);
  PWRAPLOG("return_value of _pwrap_manual_mode=%x.\n",return_value);

  wait_for_completion(&pwrap_done);
  if(wait_int_flag==1)
    PWRAPLOG("_int_test_bit5 pass.\n");
  else
    PWRAPLOG("_int_test_bit5 fail.\n");
  return 0;
}

//[14]: WACS0_CMD_MISS: A WACS0 CMD is written while WACS0 is disabled.
//    disable man,send a wacs0 command
S32 _int_test_bit14( )
{
  U32 rdata=0;
  U32 reg_rdata=0;
  U32 res=0;
  PWRAPFUC();
  int_test_bit=14;
  wait_int_flag=0;
  WRAP_WR32(PMIC_WRAP_WACS0_EN , 0);// disable man

  pwrap_wacs0(1, WRAP_ACCESS_TEST_REG, 0x55AA, &rdata);
  wait_for_completion(&pwrap_done);
  if(wait_int_flag==1)
    PWRAPLOG("_int_test_bit14 pass.\n");
  else
    PWRAPLOG("_int_test_bit14 fail.\n");
  return 0;
}

//[17]: WACS1_CMD_MISS: A WACS1 CMD is written while WACS1 is disabled.
//    disable man,send a wacs0 command
S32 _int_test_bit17( )
{
  U32 rdata=0;
  U32 reg_rdata=0;
  U32 res=0;
  PWRAPFUC();
  int_test_bit=17;
  wait_int_flag=0;
  WRAP_WR32(PMIC_WRAP_WACS1_EN , 0);// disable man

  pwrap_wacs1(1, WRAP_ACCESS_TEST_REG, 0x55AA, &rdata);
  wait_for_completion(&pwrap_done);
  if(wait_int_flag==1)
    PWRAPLOG("_int_test_bit17 pass.\n");
  else
    PWRAPLOG("_int_test_bit17 fail.\n");
  return 0;
}

//[20]: WACS2_CMD_MISS: A WACS2 CMD is written while WACS2 is disabled.
//    disable man,send a wacs2 command
S32 _int_test_bit20( )
{
  U32 rdata=0;
  U32 reg_rdata=0;
  U32 res=0;
  PWRAPFUC();
  int_test_bit=20;
  wait_int_flag=0;
  WRAP_WR32(PMIC_WRAP_WACS2_EN , 0);// disable man

  pwrap_write(WRAP_ACCESS_TEST_REG, 0x55AA);
  wait_for_completion(&pwrap_done);
  if(wait_int_flag==1)
    PWRAPLOG("_int_test_bit20 pass.\n");
  else
    PWRAPLOG("_int_test_bit20 fail.\n");
  return 0;
}

//[4]:  MAN_UNEXP_VLDCLR: MAN unexpected VLDCLR
//    send a manual write command,and clear valid big
S32 _int_test_bit3( )
{
  U32 rdata=0;
  U32 reg_rdata=0;
  U32 res=0;
  U32 return_value;
  PWRAPFUC();
  int_test_bit=3;
  wait_int_flag=0;
  _pwrap_switch_mux(1);
  return_value=_pwrap_manual_mode(OP_WR,  OP_CSH,  0, &rdata);
  PWRAPLOG("return_value of _pwrap_manual_mode=%x.\n",return_value);
  WRAP_WR32(PMIC_WRAP_MAN_VLDCLR , 1);
  wait_for_completion(&pwrap_done);
  if(wait_int_flag==1)
    PWRAPLOG("_int_test_bit3 pass.\n");
  else
    PWRAPLOG("_int_test_bit3 fail.\n");
  return 0;
}//[12]: WACS0_UNEXP_VLDCLR: WACS0 unexpected VLDCLR
//    send a wacs0 write command,and clear valid big
S32 _int_test_bit12( )
{
  U32 rdata=0;
  U32 reg_rdata=0;
  U32 res=0;
  PWRAPFUC();
  int_test_bit=12;
  wait_int_flag=0;
  pwrap_wacs0(1, WRAP_ACCESS_TEST_REG, 0x55AA, &rdata);
  WRAP_WR32(PMIC_WRAP_WACS0_VLDCLR , 1);
  wait_for_completion(&pwrap_done);
  if(wait_int_flag==1)
    PWRAPLOG("_int_test_bit12 pass.\n");
  else
    PWRAPLOG("_int_test_bit12 fail.\n");
  return 0;
}//[15]: WACS1_UNEXP_VLDCLR: WACS1 unexpected VLDCLR
//    send a wacs1 write command,and clear valid big
S32 _int_test_bit15( )
{
  U32 rdata=0;
  U32 reg_rdata=0;
  U32 res=0;
  PWRAPFUC();
  int_test_bit=15;
  wait_int_flag=0;
  pwrap_wacs1(1, WRAP_ACCESS_TEST_REG, 0x55AA, &rdata);
  WRAP_WR32(PMIC_WRAP_WACS1_VLDCLR , 1);

  wait_for_completion(&pwrap_done);
  if(wait_int_flag==1)
    PWRAPLOG("_int_test_bit15 pass.\n");
  else
    PWRAPLOG("_int_test_bit15 fail.\n");
  return 0;
}//[18]: WACS2_UNEXP_VLDCLR: WACS2 unexpected VLDCLR
//    send a wacs2 write command,and clear valid big
S32 _int_test_bit18( )
{
  U32 rdata=0;
  U32 reg_rdata=0;
  U32 res=0;
  PWRAPFUC();
  int_test_bit=18;
  wait_int_flag=0;
  pwrap_write(WRAP_ACCESS_TEST_REG, 0x55AA);
  WRAP_WR32(PMIC_WRAP_WACS2_VLDCLR , 1);

  wait_for_completion(&pwrap_done);
  if(wait_int_flag==1)
    PWRAPLOG("_int_test_bit18 pass.\n");
  else
    PWRAPLOG("_int_test_bit18 fail.\n");
  return 0;
}//[21]: PERI_WRAP_INT: PERI_PWRAP_BRIDGE interrupt is asserted.
//    send a wacs3 write command,and clear valid big
S32 _int_test_bit21( )
{
  U32 rdata=0;
  U32 reg_rdata=0;
  U32 res=0;
  PWRAPFUC();
  int_test_bit=21;

  wait_int_flag=0;
  //pwrap_wacs3(1, WRAP_ACCESS_TEST_REG, 0x55AA, &rdata);
  //WRAP_WR32(PERI_PWRAP_BRIDGE_WACS3_VLDCLR , 1);

  wait_for_completion(&pwrap_done);
  if(wait_int_flag==1)
    PWRAPLOG("_int_test_bit21 pass.\n");
  else
    PWRAPLOG("_int_test_bit21 fail.\n");
  return 0;
}

S32 _int_test_disable_watch_dog(void)
{
  //disable watch dog
  WRAP_WR32(PMIC_WRAP_WDT_SRC_EN,0);
  //WRAP_WR32(PERI_PWRAP_BRIDGE_WDT_SRC_EN , 0);
   return 0;
}


S32 tc_int_test(  )
{
    UINT32 return_value=0;
    UINT32 test_this_case=0;
    struct pmic_wrap_obj *pwrap_obj = g_pmic_wrap_obj;
  pwrap_obj->complete = pwrap_complete;
  pwrap_obj->context = &pwrap_done;

    pwrap_init();
    _int_test_disable_watch_dog();
    return_value=1;
    _int_test_bit1();
    pwrap_init();
    _int_test_disable_watch_dog();

    return_value=_int_test_bit5();
    pwrap_init();
    _int_test_disable_watch_dog();

    return_value=_int_test_bit14();
    pwrap_init();
    _int_test_disable_watch_dog();

    return_value=_int_test_bit17();
    pwrap_init();
    _int_test_disable_watch_dog();

    return_value=_int_test_bit20();
    pwrap_init();
    _int_test_disable_watch_dog();

    return_value=_int_test_bit3();
    pwrap_init();
    _int_test_disable_watch_dog();


    return_value=_int_test_bit12();
    pwrap_init();
    _int_test_disable_watch_dog();

    return_value=_int_test_bit15();
    pwrap_init();
    _int_test_disable_watch_dog();

    return_value=_int_test_bit18();
    pwrap_init();
    _int_test_disable_watch_dog();

    //return_value=_int_test_bit21();
    //pwrap_init();
    if(return_value==0)
    {
      PWRAPLOG("tc_int_test pass.\n");

    }
    else
    {
      PWRAPLOG("tc_int_test fail.res=%d\n",return_value);

    }
    return return_value;
}
#define CLK_CFG_4_SET 0xF0000000
void pwrap_power_off(void)
{
  WRAP_SET_BIT(0xE0,CLK_CFG_4_SET);//SPI clock

}

S32 tc_clock_gating_test(  )
{
  UINT32 return_value=0;
  UINT32 test_this_case=0;
  PWRAPFUC();
  pwrap_power_off();//need to follow up
  return_value=_pwrap_wrap_access_test();
  if(return_value==0)
  {
    PWRAPLOG("tc_clock_gating_test pass.\n");
    PWRAPLOG("tc_clock_gating_test fail.res=%d\n",return_value);
  }
  return return_value;

}
volatile U32 index_wacs0=0;
volatile U32 index_wacs1=0;
volatile U32 index_wacs2=0;
U64 start_time_wacs0=0;
U64 start_time_wacs1=0;
U64 start_time_wacs2=0;
U64 end_time_wacs0=0;
U64 end_time_wacs1=0;
U64 end_time_wacs2=0;
U32 WACS0_TEST_REG=DEW_WRITE_TEST;
U32 WACS1_TEST_REG=DEW_WRITE_TEST;
U32 WACS2_TEST_REG=DEW_WRITE_TEST;
U32 WACS3_TEST_REG=DEW_WRITE_TEST;
U32 WACS4_TEST_REG=DEW_WRITE_TEST;

void _throughput_wacs0_test( void )
{
  U32 i=0;
  U32 rdata=0;
  PWRAPFUC();
  start_time_wacs0=sched_clock();
  for(index_wacs0=0;index_wacs0<10000;index_wacs0++)
  {
      pwrap_wacs0(0, WACS0_TEST_REG, 0, &rdata);
  }
  end_time_wacs0=sched_clock();
  PWRAPLOG("_throughput_wacs0_test send 10000 read command:average time(ns)=%llx.\n",(end_time_wacs0-start_time_wacs0));
  PWRAPLOG("index_wacs0=%d index_wacs1=%d index_wacs2=%d\n",index_wacs0,index_wacs1,index_wacs2);
  PWRAPLOG("start_time_wacs0=%llx start_time_wacs1=%llx start_time_wacs2=%llx\n",start_time_wacs0,start_time_wacs1,start_time_wacs2);
  PWRAPLOG("end_time_wacs0=%llx end_time_wacs1=%llx end_time_wacs2=%llx\n",end_time_wacs0,end_time_wacs1,end_time_wacs2);
}
void _throughput_wacs1_test( void )
{
  //U32 i=0;
  U32 rdata=0;
  PWRAPFUC();
  start_time_wacs1=sched_clock();
  for(index_wacs1=0;index_wacs1<10000;index_wacs1++)
  {
      pwrap_wacs1(0, WACS1_TEST_REG, 0, &rdata);
  }
  end_time_wacs1=sched_clock();
  PWRAPLOG("_throughput_wacs1_test send 10000 read command:average time(ns)=%llx.\n",(end_time_wacs1-start_time_wacs1));
  PWRAPLOG("index_wacs0=%d index_wacs1=%d index_wacs2=%d\n",index_wacs0,index_wacs1,index_wacs2);
  PWRAPLOG("start_time_wacs0=%llx start_time_wacs1=%llx start_time_wacs2=%llx\n",start_time_wacs0,start_time_wacs1,start_time_wacs2);
  PWRAPLOG("end_time_wacs0=%llx end_time_wacs1=%llx end_time_wacs2=%llx\n",end_time_wacs0,end_time_wacs1,end_time_wacs2);
}

void _throughput_wacs2_test( void )
{
  U32 i=0;
  U32 rdata=0;
  U32 return_value=0;
  PWRAPFUC();
  start_time_wacs2=sched_clock();
  for(index_wacs2=0;index_wacs2<10000;index_wacs2++)
  {
      return_value=pwrap_wacs2(0, WACS2_TEST_REG, 0, &rdata);
//      if(return_value!=0)
//        PWRAPLOG("return_value=%d.index_wacs2=%d\n",return_value,index_wacs2);
  }
  end_time_wacs2=sched_clock();
  PWRAPLOG("_throughput_wacs2_test send 10000 read command:average time(ns)=%llx.\n",(end_time_wacs2-start_time_wacs2));
  PWRAPLOG("index_wacs0=%d index_wacs1=%d index_wacs2=%d\n",index_wacs0,index_wacs1,index_wacs2);
  PWRAPLOG("start_time_wacs0=%llx start_time_wacs1=%llx start_time_wacs2=%llx\n",start_time_wacs0,start_time_wacs1,start_time_wacs2);
  PWRAPLOG("end_time_wacs0=%llx end_time_wacs1=%llx end_time_wacs2=%llx\n",end_time_wacs0,end_time_wacs1,end_time_wacs2);
}

S32 tc_throughput_test(  )
{
  U32 return_value=0;
  U32 test_this_case=0;
  U32 i=0;
  U64 start_time=0;
  U64 end_time=0;

  U32 wacs0_throughput_task=0;
  U32 wacs1_throughput_task=0;
  U32 wacs2_throughput_task=0;

  U32 wacs0_throughput_cpu_id=0;
  U32 wacs1_throughput_cpu_id=1;
  U32 wacs2_throughput_cpu_id=2;

  PWRAPFUC();

  //disable INT
  WRAP_WR32(PMIC_WRAP_WDT_SRC_EN,0);
  WRAP_WR32(PMIC_WRAP_INT_EN,0x7ffffffc); //except for [31] debug_int
#if 0
  //-----------------------------------------------------------------------------------
  PWRAPLOG("write throughput,start.\n");
  WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN,8); //Only WACS2
  start_time=sched_clock();
  for(i=0;i<10000;i++)
  {
    pwrap_write(WACS2_TEST_REG, 0x30);
  }
  end_time=sched_clock();
  PWRAPLOG("send 100 write command:average time(ns)=%llx.\n",(end_time-start_time));//100000=100*1000
  PWRAPLOG("write throughput,end.\n");
  //-----------------------------------------------------------------------------------
#endif
#if 0
  dsb();
  PWRAPLOG("1-core read throughput,start.\n");
  WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN,1<<1); //Only WACS0
  wacs0_throughput_task = kthread_create(_throughput_wacs0_test,0,"wacs0_concurrence");
  kthread_bind(wacs0_throughput_task, wacs0_throughput_cpu_id);
  wake_up_process(wacs0_throughput_task);
  pwrap_delay_us(5000);
  //kthread_stop(wacs0_throughput_task);
  PWRAPLOG("stop wacs0_throughput_task.\n");
  PWRAPLOG("1-core read throughput,end.\n");
  //-----------------------------------------------------------------------------------
#endif
#if 1
  dsb();
  PWRAPLOG("2-core read throughput,start.\n");
  WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN,6); //Only WACS0 and WACS1
  wacs0_throughput_task = kthread_create(_throughput_wacs0_test,0,"wacs0_concurrence");
  kthread_bind(wacs0_throughput_task, wacs0_throughput_cpu_id);
  wake_up_process(wacs0_throughput_task);

  wacs1_throughput_task = kthread_create(_throughput_wacs1_test,0,"wacs1_concurrence");
  kthread_bind(wacs1_throughput_task, wacs1_throughput_cpu_id);
  wake_up_process(wacs1_throughput_task);

  pwrap_delay_us(50000);
  //kthread_stop(wacs0_throughput_task);
  //kthread_stop(wacs1_throughput_task);
  PWRAPLOG("stop wacs0_throughput_task and wacs1_throughput_task.\n");
  PWRAPLOG("2-core read throughput,end.\n");
  //-----------------------------------------------------------------------------------
#endif
#if 0
  dsb();
  PWRAPLOG("3-core read throughput,start.\n");
  WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN,0xE); //Only WACS0 and WACS1
  wacs0_throughput_task = kthread_create(_throughput_wacs0_test,0,"wacs0_concurrence");
  kthread_bind(wacs0_throughput_task, wacs0_throughput_cpu_id);
  wake_up_process(wacs0_throughput_task);

  wacs1_throughput_task = kthread_create(_throughput_wacs1_test,0,"wacs1_concurrence");
  kthread_bind(wacs1_throughput_task, wacs1_throughput_cpu_id);
  wake_up_process(wacs1_throughput_task);

  wacs2_throughput_task = kthread_create(_throughput_wacs2_test,0,"wacs2_concurrence");
  kthread_bind(wacs2_throughput_task, wacs2_throughput_cpu_id);
  wake_up_process(wacs2_throughput_task);
  pwrap_delay_us(50000);
  //kthread_stop(wacs0_throughput_task);
  //kthread_stop(wacs1_throughput_task);
  //kthread_stop(wacs2_throughput_task);
  //PWRAPLOG("stop wacs0_throughput_task /wacs1_throughput_task/wacs2_throughput_task.\n");
  PWRAPLOG("3-core read throughput,end.\n");
#endif
  if(return_value==0)
  {
    PWRAPLOG("tc_throughput_test pass.\n");
  }
  else
  {
    PWRAPLOG("tc_throughput_test fail.res=%d\n",return_value);
  }
}

//#ifdef PWRAP_CONCURRENCE_TEST

//###############################concurrence_test start#########################
//---define wacs direction flag:  read:WACS0_READ_WRITE_FLAG=0;write:WACS0_READ_WRITE_FLAG=0;
//#define RANDOM_TEST
//#define NORMAL_TEST
//#define stress_test_on_concurrence

static U8 wacs0_send_write_cmd_done=0;
static U8 wacs0_send_read_cmd_done=0;
static U8 wacs0_read_write_flag=0;


static U8 wacs1_send_write_cmd_done=0;
static U8 wacs1_send_read_cmd_done=0;
static U8 wacs1_read_write_flag=0;

static U8 wacs2_send_write_cmd_done=0;
static U8 wacs2_send_read_cmd_done=0;
static U8 wacs2_read_write_flag=0;


static U16 wacs0_test_value=0x10;
static U16 wacs1_test_value=0x20;
static U16 wacs2_test_value=0x30;


U32 wacs_read_cmd_done=0;
//U32 test_count0=100000000;
//U32 test_count1=100000000;
U32 test_count0=0;
U32 test_count1=0;




static U16 concurrence_fail_count_cpu0=0;
static U16 concurrence_fail_count_cpu1=0;
static U16 concurrence_pass_count_cpu0=0;
static U16 concurrence_pass_count_cpu1=0;


U32 g_spm_pass_count0=0;
U32 g_spm_fail_count0=0;
U32 g_spm_pass_count1=0;
U32 g_spm_fail_count1=0;

U32 g_pwm_pass_count0=0;
U32 g_pwm_fail_count0=0;
U32 g_pwm_pass_count1=0;
U32 g_pwm_fail_count1=0;

U32 g_wacs0_pass_count0=0;
U32 g_wacs0_fail_count0=0;
U32 g_wacs0_pass_count1=0;
U32 g_wacs0_fail_count1=0;

U32 g_wacs1_pass_count0=0;
U32 g_wacs1_fail_count0=0;
U32 g_wacs1_pass_count1=0;
U32 g_wacs1_fail_count1=0;

U32 g_wacs2_pass_count0=0;
U32 g_wacs2_fail_count0=0;
U32 g_wacs2_pass_count1=0;
U32 g_wacs2_fail_count1=0;


U32 g_stress0_cpu0_count=0;
U32 g_stress1_cpu0_count=0;
U32 g_stress2_cpu0_count=0;
U32 g_stress3_cpu0_count=0;
U32 g_stress4_cpu0_count=0;
//U32 g_stress5_cpu0_count=0;
U32 g_stress0_cpu1_count=0;
U32 g_stress1_cpu1_count=0;
U32 g_stress2_cpu1_count=0;
U32 g_stress3_cpu1_count=0;
U32 g_stress4_cpu1_count=0;
U32 g_stress5_cpu1_count=0;

U32 g_stress0_cpu0_count0=0;
U32 g_stress1_cpu0_count0=0;
U32 g_stress0_cpu1_count0=0;

U32 g_stress0_cpu0_count1=0;
U32 g_stress1_cpu0_count1=0;
U32 g_stress0_cpu1_count1=0;

U32 g_stress2_cpu0_count1=0;
U32 g_stress3_cpu0_count1=0;

U32 g_random_count0=0;
U32 g_random_count1=0;




//--------------------------------------------------------
//    Function : pwrap_wacs0()
// Description :
//   Parameter :
//      Return :
//--------------------------------------------------------
S32 _concurrence_pwrap_wacs0( U32 write, U32 adr, U32 wdata, U32 *rdata,U32 read_cmd_done )
{
  U32 reg_rdata=0;
  U32 wacs_write=0;
  U32 wacs_adr=0;
  U32 wacs_cmd=0;
  //PWRAPFUC();
  if(read_cmd_done==0)
  {
    reg_rdata = WRAP_RD32(PMIC_WRAP_WACS0_RDATA);
    if( GET_INIT_DONE0( reg_rdata ) != 1)
    {
      PWRAPERR("initialization isn't finished when write data\n");
      return 1;
    }
    if( GET_WACS0_FSM( reg_rdata ) != WACS_FSM_IDLE) //IDLE State
    {
      PWRAPERR("WACS0 is not in IDLE state\n");
      return 2;
    }
    // check argument validation
    if( (write & ~(0x1))    != 0)  return 3;
    if( (adr   & ~(0xffff)) != 0)  return 4;
    if( (wdata & ~(0xffff)) != 0)  return 5;

    wacs_write  = write << 31;
    wacs_adr    = (adr >> 1) << 16;
    wacs_cmd= wacs_write | wacs_adr | wdata;
    WRAP_WR32(PMIC_WRAP_WACS0_CMD,wacs_cmd);
  }
  else
  {
    if( write == 0 )
    {
      do
      {
        reg_rdata = WRAP_RD32(PMIC_WRAP_WACS0_RDATA);
        if( GET_INIT_DONE0( reg_rdata ) != 1)
        {
          //wrapper may be reset when error happen,so need to check if init is done
          PWRAPERR("initialization isn't finished when read data\n");
          return 6;
        }
      } while( GET_WACS0_FSM( reg_rdata ) != WACS_FSM_WFVLDCLR ); //WFVLDCLR

      *rdata = GET_WACS0_RDATA( reg_rdata );
      WRAP_WR32(PMIC_WRAP_WACS0_VLDCLR , 1);
    }
  }
  return 0;
}
//--------------------------------------------------------
//    Function : pwrap_wacs1()
// Description :
//   Parameter :
//      Return :
//--------------------------------------------------------
S32 _concurrence_pwrap_wacs1( U32  write, U32  adr, U32  wdata, U32 *rdata ,U32 read_cmd_done)
{
  U32 reg_rdata=0;
  U32 wacs_write=0;
  U32 wacs_adr=0;
  U32 wacs_cmd=0;
  if(read_cmd_done==0)
  {
    //PWRAPFUC();
    reg_rdata = WRAP_RD32(PMIC_WRAP_WACS1_RDATA);
    if( GET_INIT_DONE0( reg_rdata ) != 1)
    {
      PWRAPERR("initialization isn't finished when write data\n");
      return 1;
    }
    if( GET_WACS0_FSM( reg_rdata ) != WACS_FSM_IDLE) //IDLE State
    {
      PWRAPERR("WACS1 is not in IDLE state\n");
      return 2;
    }
    // check argument validation
    if( (write & ~(0x1))    != 0)  return 3;
    if( (adr   & ~(0xffff)) != 0)  return 4;
    if( (wdata & ~(0xffff)) != 0)  return 5;

    wacs_write  = write << 31;
    wacs_adr    = (adr >> 1) << 16;
    wacs_cmd= wacs_write | wacs_adr | wdata;

    WRAP_WR32(PMIC_WRAP_WACS1_CMD,wacs_cmd);
  }
  else
  {
    if( write == 0 )
    {
      do
      {
        reg_rdata = WRAP_RD32(PMIC_WRAP_WACS1_RDATA);
        if( GET_INIT_DONE0( reg_rdata ) != 1)
        {
          //wrapper may be reset when error happen,so need to check if init is done
          PWRAPERR("initialization isn't finished when read data\n");
          return 6;
        }
      } while( GET_WACS0_FSM( reg_rdata ) != WACS_FSM_WFVLDCLR ); //WFVLDCLR State

      *rdata = GET_WACS0_RDATA( reg_rdata );
      WRAP_WR32(PMIC_WRAP_WACS1_VLDCLR , 1);
    }
  }
  return 0;
}
//----wacs API implement for concurrence test-----------------------
S32 _concurrence_pwrap_wacs2( U32  write, U32  adr, U32  wdata, U32 *rdata, U32 read_cmd_done )
{
  U32 reg_rdata=0;
  U32 wacs_write=0;
  U32 wacs_adr=0;
  U32 wacs_cmd=0;
  if(read_cmd_done==0)
  {
    //PWRAPFUC();
    reg_rdata = WRAP_RD32(PMIC_WRAP_WACS2_RDATA);
    if( GET_INIT_DONE0( reg_rdata ) != 1)
      return 1;
    if( GET_WACS0_FSM( reg_rdata ) != WACS_FSM_IDLE) //IDLE State
    {
      PWRAPERR("WACS2 is not in IDLE state\n");
      return 2;
    }

    // check argument validation
    if( (write & ~(0x1))    != 0)  return 3;
    if( (adr   & ~(0xffff)) != 0)  return 4;
    if( (wdata & ~(0xffff)) != 0)  return 5;

    wacs_write  = write << 31;
    wacs_adr    = (adr >> 1) << 16;
    wacs_cmd= wacs_write | wacs_adr | wdata;

    WRAP_WR32(PMIC_WRAP_WACS2_CMD,wacs_cmd);
  }
  else
  {
    if( write == 0 )
    {
      do
      {
        reg_rdata = WRAP_RD32(PMIC_WRAP_WACS2_RDATA);
        //if( GET_INIT_DONE0( reg_rdata ) != 1)
        //  return 3;
      } while( GET_WACS0_FSM( reg_rdata ) != WACS_FSM_WFVLDCLR ); //WFVLDCLR

      *rdata = GET_WACS0_RDATA( reg_rdata );
      WRAP_WR32(PMIC_WRAP_WACS2_VLDCLR , 1);
    }
  }
  return 0;
}

void _concurrence_wacs0_test( void )
{
  U32 rdata=0;
  U32 reg_rdata=0;
  U32 rand_number=0;
  PWRAPFUC();
  while(1)
  {
  #ifdef RANDOM_TEST
    rand_number=(U32)random32();
    if((rand_number%2)==1)
      msleep(10);
    else
  #endif
  {
    //write wacs
    if((wacs0_read_write_flag==0)&&(wacs0_send_write_cmd_done==0))
    {
      pwrap_wacs0(1, WACS0_TEST_REG, wacs0_test_value, &rdata);
      wacs0_send_write_cmd_done=1;
      wacs0_read_write_flag=1;//read
    }
    else if(wacs0_read_write_flag==1)
    {
      if(wacs0_send_read_cmd_done==0)
      {
        //send read cmd
        _concurrence_pwrap_wacs0(0, WACS0_TEST_REG, 0, &rdata,wacs_read_cmd_done);
        wacs0_send_read_cmd_done=1;
      }
      //wait for read data
      reg_rdata = WRAP_RD32(PMIC_WRAP_WACS0_RDATA);//WFVLDCLR
      if( GET_WACS0_FSM( reg_rdata ) == WACS_FSM_WFVLDCLR ) //read_data is ready
      {
        rdata=0;
        _concurrence_pwrap_wacs0(0, WACS0_TEST_REG, 0, &rdata,!wacs_read_cmd_done);
        if( rdata != wacs0_test_value )
        {
          PWRAPERR("read test error(using WACS0),wacs0_test_value=%x, rdata=%x", wacs0_test_value, rdata);
        if (raw_smp_processor_id() == 0)
        {
          g_wacs0_fail_count0++;
        } else if (raw_smp_processor_id() == 1)
        {
          g_wacs0_fail_count1++;
        }
        //PWRAPERR("concurrence_fail_count_cpu2=%d", ++concurrence_fail_count_cpu0);
      }
      else
      {
          //PWRAPLOG("WACS0 concurrence_test pass,rdata=%x.\n",rdata);
          //PWRAPLOG("WACS0 concurrence_test pass,concurrence_pass_count_cpu0=%d\n",++concurrence_pass_count_cpu0);
        if (raw_smp_processor_id() == 0)
        {
          g_wacs0_pass_count0++;
        } else if (raw_smp_processor_id() == 1)
        {
          g_wacs0_pass_count1++;
        }
      }
      wacs0_read_write_flag=0;
      wacs0_send_read_cmd_done=0;
      wacs0_send_write_cmd_done=0;
      wacs0_test_value+=0x1;
      }
    }
  }
  }//end of while(1)
}
void _concurrence_wacs1_test( void )
{
  U32 rdata=0;
  U32 reg_rdata=0;
  U32 rand_number=0;
  PWRAPFUC();
  while(1)
  {
  #ifdef RANDOM_TEST
    rand_number=(U32)random32();
    if((rand_number%2)==1)
      msleep(10);
    else
  #endif
    {
      if((wacs1_read_write_flag==0)&&(wacs1_send_write_cmd_done==0))
      {
        pwrap_wacs1(1, WACS1_TEST_REG, wacs1_test_value, &rdata);
        wacs1_send_write_cmd_done=1;
        wacs1_read_write_flag=1;//read
      }
      else if(wacs1_read_write_flag==1)
      {
        if(wacs1_send_read_cmd_done==0)
        {
          //send read cmd
          _concurrence_pwrap_wacs1(0, WACS1_TEST_REG, 0, &rdata,wacs_read_cmd_done);
          wacs1_send_read_cmd_done=1;
        }
        //wait for read data
        reg_rdata = WRAP_RD32(PMIC_WRAP_WACS1_RDATA);//WFVLDCLR
        if( GET_WACS0_FSM( reg_rdata ) == WACS_FSM_WFVLDCLR )
        {
          rdata=0;
          _concurrence_pwrap_wacs1(0, WACS1_TEST_REG, 0, &rdata,!wacs_read_cmd_done);
          if( rdata != wacs1_test_value )
          {
            PWRAPERR("read test error(using WACS1),wacs1_test_value=%x, rdata=%x", wacs1_test_value, rdata);
            if (raw_smp_processor_id() == 0)
            {
              g_wacs1_fail_count0++;
            } else if (raw_smp_processor_id() == 1)
            {
              g_wacs1_fail_count1++;
            }
            // PWRAPERR("concurrence_fail_count_cpu1=%d", ++concurrence_fail_count_cpu1);
          }
          else
          {
            if (raw_smp_processor_id() == 0)
            {
              g_wacs1_pass_count0++;
            } else if (raw_smp_processor_id() == 1)
            {
              g_wacs1_pass_count1++;
            }
            //PWRAPLOG("WACS1 concurrence_test pass,rdata=%x.\n",rdata);
            //PWRAPLOG("WACS1 concurrence_test pass,concurrence_pass_count_cpu1=%d\n",++concurrence_pass_count_cpu1);
          }
          wacs1_read_write_flag=0;
          wacs1_send_read_cmd_done=0;
          wacs1_send_write_cmd_done=0;
          wacs1_test_value+=0x3;
        }
      }
   }
  }//end of while(1)
}
void _concurrence_wacs2_test( void )
{
  U32 rdata=0;
  U32 reg_rdata=0;
  U32 rand_number=0;
  PWRAPFUC();
  while(1)
  {
  #ifdef RANDOM_TEST
    rand_number=(U32)random32();
    if((rand_number%2)==1)
      msleep(10);
    else
  #endif
    {

      if((wacs2_read_write_flag==0)&&(wacs2_send_write_cmd_done==0))
      {
        pwrap_write(WACS2_TEST_REG, wacs2_test_value);
        wacs2_send_write_cmd_done=1;
        wacs2_read_write_flag=1;//read
      }
      else if(wacs2_read_write_flag==1)
      {
        if(wacs2_send_read_cmd_done==0)
        {
          //send read cmd
          _concurrence_pwrap_wacs2(0, WACS2_TEST_REG, 0, &rdata,wacs_read_cmd_done);
          wacs2_send_read_cmd_done=1;
        }
        //wait for read data
        reg_rdata = WRAP_RD32(PMIC_WRAP_WACS2_RDATA);//WFVLDCLR
        if( GET_WACS0_FSM( reg_rdata ) == WACS_FSM_WFVLDCLR )
        {
          rdata=0;
          _concurrence_pwrap_wacs2(0, WACS2_TEST_REG, 0, &rdata,!wacs_read_cmd_done);
          if( rdata != wacs2_test_value )
          {
            /* TERR="Error: [WriteTest] fail, rdata=%x, exp=0x1234", rdata*/
            PWRAPERR("read test error(using WACS2),wacs2_test_value=%x, rdata=%x", wacs2_test_value, rdata);
            if (raw_smp_processor_id() == 0)
            {
              g_wacs2_fail_count0++;
            } else if (raw_smp_processor_id() == 1)
            {
              g_wacs2_fail_count1++;
            }
           //PWRAPERR("concurrence_fail_count_cpu1=%d", ++concurrence_fail_count_cpu1);
          }
          else
          {
            if (raw_smp_processor_id() == 0)
            {
              g_wacs2_pass_count0++;
            } else if (raw_smp_processor_id() == 1)
            {
              g_wacs2_pass_count1++;
            }
            //PWRAPLOG("WACS2 concurrence_test pass,rdata=%x.\n",rdata);
            //PWRAPLOG("WACS2 concurrence_test pass,concurrence_pass_count_cpu1=%d\n",++concurrence_pass_count_cpu1);
          }
          wacs2_read_write_flag=0;
          wacs2_send_read_cmd_done=0;
          wacs2_send_write_cmd_done=0;
          wacs2_test_value+=0x4;
        }
      }
    }
  }//end of while(1)
}


U32 spm_task=0;
U32 spm_cpu_id=1;

U32 wacs0_task=0;
U32 wacs0_cpu_id=1;
U32 wacs1_task=0;
U32 wacs1_cpu_id=1;
U32 wacs2_task=0;
U32 wacs2_cpu_id=1;


U32 log0_task=0;
U32 log0_cpu_id=0;

U32 log1_task=0;
U32 log1_cpu_id=1;

U32 kthread_stress0_cpu0=0;
U32 stress0_cpu_id=0;

U32 kthread_stress1_cpu0=0;
U32 stress1_cpu_id=0;

U32 kthread_stress2_cpu0=0;
U32 stress2_cpu_id=0;

U32 kthread_stress3_cpu0=0;
U32 stress3_cpu_id=0;

U32 kthread_stress4_cpu0=0;
U32 stress4_cpu_id=0;

U32 kthread_stress0_cpu1=0;
U32 stress01_cpu_id=0;

U32 kthread_stress1_cpu1=0;
U32 kthread_stress2_cpu1=0;
U32 kthread_stress3_cpu1=0;
U32 kthread_stress4_cpu1=0;
U32 kthread_stress5_cpu1=0;

S32 _concurrence_spm_test_code(unsigned int spm)
{
  PWRAPFUC();
#ifdef ENABLE_SPM_ON_LDVT
  U32 i=0;
  //while(i<20)
  while(1)
  {
    //mtk_pmic_dvfs_wrapper_test(10);
    //i--;
  }
#endif
}

S32 _concurrence_log0(unsigned int spm)
{
  PWRAPFUC();
  U32 i=1;
  U32 index=0;
  U32 cpu_id=0;
  U32 rand_number=0;
  U32 reg_value=0;
  //while(i<20)
  while(1)
  {
    //log---------------------------------------------------------------
    //if((test_count0%10000)==0)
    if((i%1000000)==0)
    {
      PWRAPLOG("spm,pass count=%d,fail count=%d\n",g_spm_pass_count0,g_spm_fail_count0);

      PWRAPLOG("wacs0,pass count=%d,fail count=%d\n",g_wacs0_pass_count0,g_wacs0_fail_count0);
      PWRAPLOG("wacs1,pass count=%d,fail count=%d\n",g_wacs1_pass_count0,g_wacs1_fail_count0);
      PWRAPLOG("wacs2,pass count=%d,fail count=%d\n",g_wacs2_pass_count0,g_wacs2_fail_count0);
      //PWRAPLOG("wacs4,pass count=%d,fail count=%d\n",g_wacs4_pass_count0,g_wacs4_fail_count0);
      PWRAPLOG("g_stress0_cpu0_count0=%d\n",g_stress0_cpu0_count0);
      PWRAPLOG("g_stress1_cpu0_count0=%d\n",g_stress1_cpu0_count0);
      PWRAPLOG("g_stress0_cpu1_count0=%d\n",g_stress0_cpu1_count0);
      PWRAPLOG("g_random_count0=%d\n",g_random_count0);
      PWRAPLOG("g_random_count1=%d\n",g_random_count1);
	  reg_value = WRAP_RD32(PMIC_WRAP_HARB_STA1);
      PWRAPLOG("PMIC_WRAP_HARB_STA1=%d\n",reg_value);
	  //reg_value = WRAP_RD32(PMIC_WRAP_RRARB_STA1);
      //PWRAPLOG("PMIC_WRAP_RRARB_STA1=%d\n",reg_value);
	  //reg_value = WRAP_RD32(PERI_PWRAP_BRIDGE_IARB_STA1);
      //PWRAPLOG("PERI_PWRAP_BRIDGE_IARB_STA1=%d\n",reg_value);

    //}
    //if((i%1000000)==0)
    //if(0)
    //{
      rand_number=(U32)random32();
      if((rand_number%2)==1)
      {
        cpu_id=((spm_cpu_id++)%2);
        if (wait_task_inactive(spm_task, TASK_UNINTERRUPTIBLE))
        {
          PWRAPLOG("spm_cpu_id=%d\n",cpu_id);
          kthread_bind(spm_task, cpu_id);
        }
        else
         spm_cpu_id--;
      }


      rand_number=(U32)random32();
      if((rand_number%2)==1)
      {
        cpu_id=(wacs0_cpu_id++)%2;
        if (wait_task_inactive(wacs0_task, TASK_UNINTERRUPTIBLE))
        {
          PWRAPLOG("wacs0_cpu_id=%d\n",cpu_id);
          kthread_bind(wacs0_task, cpu_id);
        }
        else
         wacs0_cpu_id--;
      }

      rand_number=(U32)random32();
      if((rand_number%2)==1)
      {
        cpu_id=(wacs1_cpu_id++)%2;
        if (wait_task_inactive(wacs1_task, TASK_UNINTERRUPTIBLE))
        {
          PWRAPLOG("wacs1_cpu_id=%d\n",cpu_id);
          kthread_bind(wacs1_task, cpu_id);
        }
        else
         wacs1_cpu_id--;
      }

      rand_number=(U32)random32();
      if((rand_number%2)==1)
      {
        cpu_id=(wacs2_cpu_id++)%2;
        if (wait_task_inactive(wacs2_task, TASK_UNINTERRUPTIBLE))
        {
          PWRAPLOG("wacs2_cpu_id=%d\n",cpu_id);
          kthread_bind(wacs2_task, cpu_id);
        }
        else
         wacs2_cpu_id--;
      }


      rand_number=(U32)random32();
      if((rand_number%2)==1)
      {
        cpu_id=(stress0_cpu_id++)%2;
        //kthread_bind(kthread_stress0_cpu0, cpu_id);
      }

      rand_number=(U32)random32();
      if((rand_number%2)==1)
      {
        cpu_id=(stress1_cpu_id++)%2;
        //kthread_bind(kthread_stress1_cpu0, cpu_id);
      }
      rand_number=(U32)random32();
      if((rand_number%2)==1)
      {
        cpu_id=(stress2_cpu_id++)%2;
        //kthread_bind(kthread_stress2_cpu0, cpu_id);
      }

      rand_number=(U32)random32();
      if((rand_number%2)==1)
      {
        cpu_id=(stress3_cpu_id++)%2;
        //kthread_bind(kthread_stress3_cpu0, cpu_id);
      }

      rand_number=(U32)random32();
      if((rand_number%2)==1)
      {
        cpu_id=(stress4_cpu_id++)%2;
        //kthread_bind(kthread_stress4_cpu0, cpu_id);
      }

      rand_number=(U32)random32();
      if((rand_number%2)==1)
      {
        cpu_id=(stress01_cpu_id++)%2;
        //kthread_bind(kthread_stress0_cpu1, cpu_id);
      }

    }
    i++;
   }

}

S32 _concurrence_log1(unsigned int spm)
{
  PWRAPFUC();
  U32 i=0;
  //while(i<20)
  while(1)
  {
    //log---------------------------------------------------------------
    //if((test_count0%10000)==0)
    if((i%1000000)==0)
    {
      PWRAPLOG("spm,pass count=%d,fail count=%d\n", g_spm_pass_count1, g_spm_fail_count1);
      PWRAPLOG("wacs0,pass count=%d,fail count=%d\n",g_wacs0_pass_count1,g_wacs0_fail_count1);
      PWRAPLOG("wacs1,pass count=%d,fail count=%d\n",g_wacs1_pass_count1,g_wacs1_fail_count1);
      PWRAPLOG("wacs2,pass count=%d,fail count=%d\n",g_wacs2_pass_count1,g_wacs2_fail_count1);
      //PWRAPLOG("g_stress0_cpu1_count=%d\n",g_stress0_cpu1_count);
      PWRAPLOG("g_stress0_cpu0_count1=%d\n",g_stress0_cpu0_count1);
      PWRAPLOG("g_stress1_cpu0_count1=%d\n",g_stress1_cpu0_count1);
      PWRAPLOG("g_stress0_cpu1_count1=%d\n",g_stress0_cpu1_count1);

    }
    i++;
   }

}

S32 _concurrence_stress0_cpu0(unsigned int stress)
{
  PWRAPFUC();
  U32 i=0;
  U32 rand_number=0;

  while(1)
  {
      g_random_count0++;
      rand_number=(U32)random32();
      if((rand_number%2)==1)
      {
        g_random_count1++;
        for(i=0;i<100000;i++)
        {
          //g_stress0_cpu0_count++;
          if (raw_smp_processor_id() == 0)
          {
            g_stress0_cpu0_count0++;
          } else if (raw_smp_processor_id() == 1)
          {
            g_stress0_cpu0_count1++;
          }
        }
      }
  }
}
S32 _concurrence_stress1_cpu0(unsigned int stress)
{
  PWRAPFUC();
  U32 rand_number=0;
  U32 i=0;
  //while(i<20)
  for(;;)
  {
    for(i=0;i<100000;i++)
    {
      //g_stress1_cpu0_count++;
      if (raw_smp_processor_id() == 0)
      {
        g_stress1_cpu0_count0++;
      } else if (raw_smp_processor_id() == 1)
      {
        g_stress1_cpu0_count1++;
      }
    }
  }
}

S32 _concurrence_stress2_cpu0(unsigned int stress)
{
  PWRAPFUC();
  U32 i=0;
  U32 rand_number=0;
  for(;;)
  {
    rand_number=(U32)random32();
    if((rand_number%2)==1)
    {
      for(i=0;i<100000;i++)
        g_stress1_cpu0_count++;
    }
  }
}

S32 _concurrence_stress3_cpu0(unsigned int stress)
{
  PWRAPFUC();
  U32 i=0;
  U32 rand_number=0;
  for(;;)
  {
    rand_number=(U32)random32();
    if((rand_number%2)==1)
    {
      for(i=0;i<100000;i++)
        g_stress3_cpu0_count++;
    }
  }
}


S32 _concurrence_stress4_cpu0(unsigned int stress)
{
  PWRAPFUC();
  U32 i=0;
  U32 rand_number=0;
  for(;;)
  {
    rand_number=(U32)random32();
    if((rand_number%2)==1)
    {
      for(i=0;i<100000;i++)
        g_stress4_cpu0_count++;
    }
  }
}

S32 _concurrence_stress0_cpu1(unsigned int stress)
{
  PWRAPFUC();
  U32 i=0;
  U32 rand_number=0;
  for(;;)
  {
    rand_number=(U32)random32();
    if((rand_number%2)==1)
    {
      for(i=0;i<100000;i++)
      {
        if (raw_smp_processor_id() == 0)
        {
          g_stress0_cpu1_count0++;
        } else if (raw_smp_processor_id() == 1)
        {
          g_stress0_cpu1_count1++;
        }
      }
    }
  }
}

S32 _concurrence_stress1_cpu1(unsigned int stress)
{
  PWRAPFUC();
  U32 i=0;
  U32 rand_number=0;
  for(;;)
  {
    rand_number=(U32)random32();
    if((rand_number%2)==1)
    {
      for(i=0;i<100000;i++)
        g_stress1_cpu1_count++;
    }
  }
}

S32 _concurrence_stress2_cpu1(unsigned int stress)
{
  PWRAPFUC();
  U32 i=0;
  U32 rand_number=0;
    //while(i<20)
  for(;;)
  {
    rand_number=(U32)random32();
    if((rand_number%2)==1)
    {
     for(i=0;i<100000;i++)
        g_stress2_cpu0_count++;
    }
  }
}

S32 _concurrence_stress3_cpu1(unsigned int stress)
{
  PWRAPFUC();
  U32 i=0;
  U32 rand_number=0;
    //while(i<20)
  for(;;)
  {
    rand_number=(U32)random32();
    if((rand_number%2)==1)
    {
      for(i=0;i<100000;i++)
        g_stress3_cpu0_count++;
    }
  }
}

S32 _concurrence_stress4_cpu1(unsigned int stress)
{
  PWRAPFUC();
  U32 i=0;
  U32 rand_number=0;

  while(1)
  {
    rand_number=(U32)random32();
    if((rand_number%2)==1)
    {
      for(i=0;i<100000;i++)
      g_stress4_cpu1_count++;
    }
  }
}

S32 _concurrence_stress5_cpu1(unsigned int stress)
{
  PWRAPFUC();
  U32 i=0;
  U32 rand_number=0;

  while(1)
  {
    rand_number=(U32)random32();
    if((rand_number%2)==1)
    {
      for(i=0;i<100000;i++)
        g_stress5_cpu1_count++;
    }
  }
}

//----wacs concurrence test start ------------------------------------------

S32 tc_concurrence_test(  )
{
  UINT32 res=0;
  U32 rdata=0;
  U32 i=0;
  res=0;
  struct sched_param param = { .sched_priority = MAX_RT_PRIO-1 };

  PWRAPFUC();
  spm_task = kthread_create(_concurrence_spm_test_code,0,"spm_concurrence");
  if(IS_ERR(spm_task)){
    PWRAPERR("Unable to start kernelthread \n");
    res = -5;
  }
  //kthread_bind(spm_task, spm_cpu_id);
  wake_up_process(spm_task);

  wacs0_task = kthread_create(_concurrence_wacs0_test,0,"wacs0_concurrence");
  if(IS_ERR(wacs0_task)){
    PWRAPERR("Unable to start kernelthread \n");
    res = -5;
  }
  //kthread_bind(wacs0_task, wacs0_cpu_id);
  wake_up_process(wacs0_task);

  wacs1_task = kthread_create(_concurrence_wacs1_test,0,"wacs1_concurrence");
  if(IS_ERR(wacs1_task)){
    PWRAPERR("Unable to start kernelthread \n");
    res = -5;
  }
  //kthread_bind(wacs1_task, wacs1_cpu_id);
  wake_up_process(wacs1_task);

  wacs2_task = kthread_create(_concurrence_wacs2_test,0,"wacs2_concurrence");
  if(IS_ERR(wacs2_task)){
    PWRAPERR("Unable to start kernelthread \n");
    res = -5;
  }
  //kthread_bind(wacs2_task, wacs2_cpu_id);
  wake_up_process(wacs2_task);


  log0_task = kthread_create(_concurrence_log0,0,"log0_concurrence");
  if(IS_ERR(log0_task)){
    PWRAPERR("Unable to start kernelthread \n");
    res = -5;
  }
  //sched_setscheduler(log0_task, SCHED_FIFO, &param);
  kthread_bind(log0_task, log0_cpu_id);
  wake_up_process(log0_task);

  log1_task = kthread_create(_concurrence_log1,0,"log1_concurrence");
  if(IS_ERR(log1_task)){
    PWRAPERR("Unable to start kernelthread \n");
    res = -5;
  }
  //sched_setscheduler(log1_task, SCHED_FIFO, &param);
  kthread_bind(log1_task, log1_cpu_id);
  wake_up_process(log1_task);
#ifdef stress_test_on_concurrence
  //increase cpu load
  kthread_stress0_cpu0 = kthread_create(_concurrence_stress0_cpu0,0,"stress0_cpu0_concurrence");
  if(IS_ERR(kthread_stress0_cpu0)){
    PWRAPERR("Unable to start kernelthread \n");
    res = -5;
  }
  kthread_bind(kthread_stress0_cpu0, 0);
  wake_up_process(kthread_stress0_cpu0);

  kthread_stress1_cpu0 = kthread_create(_concurrence_stress1_cpu0,0,"stress0_cpu1_concurrence");
  if(IS_ERR(kthread_stress1_cpu0)){
    PWRAPERR("Unable to start kernelthread \n");
    res = -5;
  }
  kthread_bind(kthread_stress1_cpu0, 0);
  wake_up_process(kthread_stress1_cpu0);

  kthread_stress2_cpu0 = kthread_create(_concurrence_stress2_cpu0,0,"stress0_cpu1_concurrence");
  if(IS_ERR(kthread_stress2_cpu0)){
    PWRAPERR("Unable to start kernelthread \n");
    res = -5;
  }
  //kthread_bind(kthread_stress2_cpu0, 0);
  wake_up_process(kthread_stress2_cpu0);

  kthread_stress3_cpu0 = kthread_create(_concurrence_stress3_cpu0,0,"stress0_cpu1_concurrence");
  if(IS_ERR(kthread_stress3_cpu0)){
    PWRAPERR("Unable to start kernelthread \n");
    res = -5;
  }
  //kthread_bind(kthread_stress3_cpu0, 0);
  wake_up_process(kthread_stress3_cpu0);

  //kthread_stress4_cpu0 = kthread_create(_concurrence_stress4_cpu0,0,"stress0_cpu1_concurrence");
  if(IS_ERR(kthread_stress4_cpu0)){
    PWRAPERR("Unable to start kernelthread \n");
    res = -5;
  }
  //kthread_bind(kthread_stress4_cpu0, 1);
  //wake_up_process(kthread_stress4_cpu0);

  kthread_stress0_cpu1 = kthread_create(_concurrence_stress0_cpu1,0,"stress0_cpu1_concurrence");
  if(IS_ERR(kthread_stress0_cpu1)){
    PWRAPERR("Unable to start kernelthread \n");
    res = -5;
  }
  kthread_bind(kthread_stress0_cpu1, 1);
  wake_up_process(kthread_stress0_cpu1);

  kthread_stress1_cpu1 = kthread_create(_concurrence_stress1_cpu1,0,"stress0_cpu1_concurrence");
  if(IS_ERR(kthread_stress1_cpu1)){
    PWRAPERR("Unable to start kernelthread \n");
    res = -5;
  }
  //kthread_bind(kthread_stress1_cpu1, 1);
  wake_up_process(kthread_stress1_cpu1);

  kthread_stress2_cpu1 = kthread_create(_concurrence_stress2_cpu1,0,"stress0_cpu1_concurrence");
  if(IS_ERR(kthread_stress2_cpu1)){
    PWRAPERR("Unable to start kernelthread \n");
    res = -5;
  }
  //kthread_bind(kthread_stress2_cpu1, 0);
  wake_up_process(kthread_stress2_cpu1);

  kthread_stress3_cpu1 = kthread_create(_concurrence_stress3_cpu1,0,"stress0_cpu1_concurrence");
  if(IS_ERR(kthread_stress3_cpu1)){
    PWRAPERR("Unable to start kernelthread \n");
    res = -5;
  }
  //kthread_bind(kthread_stress3_cpu1, 1);
  wake_up_process(kthread_stress3_cpu1);

  kthread_stress4_cpu1 = kthread_create(_concurrence_stress4_cpu1,0,"stress0_cpu1_concurrence");
  if(IS_ERR(kthread_stress3_cpu1)){
    PWRAPERR("Unable to start kernelthread \n");
    res = -5;
  }
  //kthread_bind(kthread_stress4_cpu1, 1);
  wake_up_process(kthread_stress4_cpu1);

  kthread_stress5_cpu1 = kthread_create(_concurrence_stress5_cpu1,0,"stress0_cpu1_concurrence");
  if(IS_ERR(kthread_stress3_cpu1)){
    PWRAPERR("Unable to start kernelthread \n");
    res = -5;
  }
  //kthread_bind(kthread_stress5_cpu1, 1);
  wake_up_process(kthread_stress5_cpu1);

#endif //stress test


  if(res==0)
  {
    PWRAPLOG("tc_concurrence_test pass.\n");
  }
  else
  {
    PWRAPLOG("tc_concurrence_test fail.res=%d\n",res);
  }
  return res;
}
//-------------------concurrence_test end-------------------------------------



