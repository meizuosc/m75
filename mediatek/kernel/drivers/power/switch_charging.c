/*****************************************************************************
 *
 * Filename:
 * ---------
 *    linear_charging.c
 *
 * Project:
 * --------
 *   ALPS_Software
 *
 * Description:
 * ------------
 *   This file implements the interface between BMT and ADC scheduler.
 *
 * Author:
 * -------
 *  Oscar Liu
 *
 *============================================================================
  * $Revision:   1.0  $
 * $Modtime:   11 Aug 2005 10:28:16  $
 * $Log:   //mtkvs01/vmdata/Maui_sw/archives/mcu/hal/peripheral/inc/bmt_chr_setting.h-arc  $
 *             HISTORY
 * Below this line, this part is controlled by PVCS VM. DO NOT MODIFY!!
 *------------------------------------------------------------------------------
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by PVCS VM. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/
#include <linux/xlog.h>
#include <linux/kernel.h>
#include <mach/battery_common.h>
#include <mach/charging.h>
#include "cust_charging.h"
#include <mach/mt_boot.h>
#include <mach/battery_meter.h>
//modify by willcai 2014-5-29 begin
#ifdef X2_CHARGING_STANDARD_SUPPORT
#include "x2_charging.h"
#endif
#include <linux/delay.h>
//end

 // ============================================================ //
 //define
 // ============================================================ //
 //cut off to full
#define POST_CHARGING_TIME	 30 * 60 // 30mins
#define FULL_CHECK_TIMES		6
#define MTK_PUMP_EXPRESS_SUPPORT_INCREASE//ADD by willcai 

 // ============================================================ //
 //global variable
 // ============================================================ //
 kal_uint32 g_bcct_flag=0;
 kal_uint32 g_bcct_value=0;
 kal_uint32 g_full_check_count=0;
 CHR_CURRENT_ENUM g_temp_CC_value = CHARGE_CURRENT_0_00_MA;
 CHR_CURRENT_ENUM g_temp_input_CC_value = CHARGE_CURRENT_0_00_MA;
 kal_uint32 g_usb_state = USB_UNCONFIGURED;
 static bool usb_unlimited=false;

/* USB working mode */
typedef enum
{
    CABLE_MODE_CHRG_ONLY = 0,
    CABLE_MODE_NORMAL,
    CABLE_MODE_HOST_ONLY,
    CABLE_MODE_MAX
} CABLE_MODE;

static CABLE_MODE musb_mode = CABLE_MODE_NORMAL;
 extern void check_musb_mode(void *data);

  ///////////////////////////////////////////////////////////////////////////////////////////
  //// JEITA
  ///////////////////////////////////////////////////////////////////////////////////////////
#if defined(MTK_JEITA_STANDARD_SUPPORT)
  int g_temp_status=TEMP_POS_10_TO_POS_45;
  kal_bool temp_error_recovery_chr_flag =KAL_TRUE;
#endif

#if defined(MTK_PUMP_EXPRESS_SUPPORT)
   struct wake_lock TA_charger_suspend_lock;
   kal_bool ta_check_chr_type = KAL_TRUE;
   kal_bool ta_cable_out_occur = KAL_FALSE;
   kal_bool is_ta_connect = KAL_FALSE;
   kal_bool ta_vchr_tuning = KAL_TRUE;
   int ta_v_chr_org = 0;
   kal_bool is_ta_check_done = KAL_FALSE;
#endif

static kal_uint32 batt_temp_status = TEMP_POS_NORMAL;
extern void get_batt_temp_status(void *data);
 
 // ============================================================ //
 // function prototype
 // ============================================================ //
 
 
 // ============================================================ //
 //extern variable
 // ============================================================ //
 extern int g_platform_boot_mode;
 // ============================================================ //
 //extern function
 // ============================================================ //

void BATTERY_SetUSBState(int usb_state_value)
{
#if defined(CONFIG_POWER_EXT)
	battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY_SetUSBState] in FPGA/EVB, no service\r\n");
#else
    if ( (usb_state_value < USB_SUSPEND) || ((usb_state_value > USB_CONFIGURED))){
        battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY] BAT_SetUSBState Fail! Restore to default value\r\n");    
        usb_state_value = USB_UNCONFIGURED;
    } else {
        battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY] BAT_SetUSBState Success! Set %d\r\n", usb_state_value);    
        g_usb_state = usb_state_value;    
    }
#endif	
}


kal_uint32 get_charging_setting_current()
{
    return g_temp_CC_value;	
}

#ifdef MTK_PUMP_EXPRESS_SUPPORT
static DEFINE_MUTEX(ta_mutex);

//add by willcai for configing the current to vchg using the Vchg 
#if 0
static void set_vcharging_current(void)
{
	int real_vchg = 0;
	real_vchg =battery_meter_get_charger_voltage();
	battery_xlog_printk(BAT_LOG_CRTI, "willcai set_vcharging_current, real_vchg=%d ,BMT_status.charger_vol= %d\n", real_vchg,BMT_status.charger_vol);
	if(BMT_status.charger_vol<6600)
		{
		g_temp_input_CC_value = CHARGE_CURRENT_1700_00_MA;

	    }
	else if( BMT_status.charger_vol<8600)
		{
		g_temp_input_CC_value = CHARGE_CURRENT_1100_00_MA;

	    }
	else if(BMT_status.charger_vol<11400)
		{
		g_temp_input_CC_value = CHARGE_CURRENT_900_00_MA;

	    }
	else {
		g_temp_input_CC_value = CHARGE_CURRENT_700_00_MA;

	     }
	battery_xlog_printk(BAT_LOG_CRTI, "willcai set_vcharging_current, g_temp_input_CC_value=%d\n", g_temp_input_CC_value);

}
#endif
static void set_ta_charging_current(void)
{
	int real_v_chrA = 0;
	real_v_chrA = battery_meter_get_charger_voltage();
	battery_xlog_printk(BAT_LOG_CRTI, "set_ta_charging_current, chrA=%d, chrB=%d\n", 
		ta_v_chr_org, real_v_chrA);

	if((real_v_chrA - ta_v_chr_org) > 3000)
	{
		g_temp_input_CC_value = CHARGE_CURRENT_2000_00_MA;  //TA = 9V
		g_temp_CC_value = CHARGE_CURRENT_1400_00_MA;
	}
	else if((real_v_chrA - ta_v_chr_org) > 1000)
	{
		g_temp_input_CC_value = CHARGE_CURRENT_2000_00_MA;  //TA = 7V
		g_temp_CC_value = CHARGE_CURRENT_2000_00_MA;
	}
}

static void mtk_ta_decrease(void)
{
	kal_bool ta_current_pattern = KAL_FALSE;  // FALSE = decrease

	if(ta_cable_out_occur == KAL_FALSE)
	{
		battery_charging_control(CHARGING_CMD_SET_TA_CURRENT_PATTERN, &ta_current_pattern);
	}
	else
	{
		ta_check_chr_type = KAL_TRUE;
		battery_xlog_printk(BAT_LOG_CRTI, "mtk_ta_decrease() Cable out \n");
	}
}

static void mtk_ta_increase(void)
{
	kal_bool ta_current_pattern = KAL_TRUE;  // TRUE = increase

	if(ta_cable_out_occur == KAL_FALSE)
	{
		battery_charging_control(CHARGING_CMD_SET_TA_CURRENT_PATTERN, &ta_current_pattern);
	}
	else
	{
		ta_check_chr_type = KAL_TRUE;
		battery_xlog_printk(BAT_LOG_CRTI, "mtk_ta_increase() Cable out \n");
	}
}

static kal_bool mtk_ta_retry_increase(void)
{
	int real_v_chrA;
	int real_v_chrB;
	kal_bool retransmit = KAL_TRUE;
	kal_uint32 retransmit_count=0;
	kal_bool vbat = KAL_TRUE;
	
	ta_v_chr_org = battery_meter_get_charger_voltage();
	do
	{
		real_v_chrA = battery_meter_get_charger_voltage();
		mtk_ta_increase();  //increase TA voltage to 7V
		real_v_chrB = battery_meter_get_charger_voltage();

		if(real_v_chrB - real_v_chrA >= 1000)	// 1.0V
		{
			retransmit = KAL_FALSE;
		}
		else
		{
			retransmit_count++;
			battery_xlog_printk(BAT_LOG_CRTI, "mtk_ta_detector(): retransmit_count =%d, chrA=%d, chrB=%d\n", 
				retransmit_count, real_v_chrA, real_v_chrB);
		}

		if((retransmit_count == 3) || (BMT_status.charger_exist == KAL_FALSE))
		{
			retransmit = KAL_FALSE;
		}

        #if defined(PE_PLUS_KEEP_IC_ALIVE)
        //keep charging alive--------------
        is_ta_check_done = KAL_TRUE;
        battery_charging_control(CHARGING_CMD_RESET_WATCH_DOG_TIMER,&vbat);
        is_ta_check_done = KAL_FALSE;
        //--------------
        #endif

	}while((retransmit == KAL_TRUE) && (ta_cable_out_occur == KAL_FALSE));

	battery_xlog_printk(BAT_LOG_CRTI, "mtk_ta_retry_increase() real_v_chrA=%d, real_v_chrB=%d, retry=%d\n", 
	real_v_chrA, real_v_chrB,retransmit_count);

	if(retransmit_count == 3)
		return KAL_FALSE;	
	else
		return KAL_TRUE;	
}

static void mtk_ta_detector(void)
{
	battery_xlog_printk(BAT_LOG_CRTI, "mtk_ta_detector() start\n");

	if(mtk_ta_retry_increase() == KAL_TRUE)
		is_ta_connect = KAL_TRUE;
	else
		is_ta_connect = KAL_FALSE;


		
	battery_xlog_printk(BAT_LOG_CRTI, "mtk_ta_detector() end, is_ta_connect=%d\n",is_ta_connect);
}
//add by willcai for decince the vchg to 5V  begin 
static void mtk_ta_reset_vchr(void)
{
   CHR_CURRENT_ENUM    chr_current = CHARGE_CURRENT_70_00_MA;
   
   battery_charging_control(TA_SET_CURRENT,NULL);
   msleep(300);    // reset Vchr to 5V 

  

   battery_xlog_printk(BAT_LOG_CRTI, "mtk_ta_reset_vchr(): reset Vchr to 5V \n");
}
//end
static void mtk_ta_init(void)
{
	battery_xlog_printk(BAT_LOG_CRTI, "mtk_ta_init() start\n");

	is_ta_connect = KAL_FALSE;
	ta_cable_out_occur = KAL_FALSE;

    #if 0
	#ifdef TA_9V_SUPPORT
	ta_vchr_tuning = KAL_FALSE;
	#endif
	#endif
  
	battery_charging_control(CHARGING_CMD_INIT,NULL);
	
	#if 0 
	//modify by will for conifging the current  Vchg  begin  7-28
	battery_charging_control(CHARGING_CMD_SET_CURRENT_VCHG,NULL);
	
	battery_charging_control(VCHG_RET_TO_5V,NULL);//reset the vchg to 5v
    #endif

	battery_xlog_printk(BAT_LOG_CRTI, "mtk_ta_init() End\n");
}

static void battery_pump_express_charger_check(void)
{
	int real_v_chr;

	if (KAL_TRUE == ta_check_chr_type &&
	  STANDARD_CHARGER == BMT_status.charger_type &&
	  BMT_status.SOC > TA_START_BATTERY_SOC &&
	  BMT_status.SOC < TA_STOP_BATTERY_SOC)
	{
		mutex_lock(&ta_mutex);
		wake_lock(&TA_charger_suspend_lock);
		
		
		
		mtk_ta_init();
		
		mtk_ta_detector();

		/* need to re-check if the charger plug out during ta detector */
		if(KAL_TRUE == ta_cable_out_occur)
			ta_check_chr_type = KAL_TRUE;
		else
			ta_check_chr_type = KAL_FALSE;

		wake_unlock(&TA_charger_suspend_lock);
		mutex_unlock(&ta_mutex);
	}
	else
	{       
		battery_xlog_printk(BAT_LOG_CRTI, 
		"Stop battery_pump_express_charger_check, SOC=%d, ta_check_chr_type = %d, charger_type = %d \n", 
		BMT_status.SOC, ta_check_chr_type, BMT_status.charger_type);
	}
}

static void battery_pump_express_algorithm_start(void)
{
	kal_int32 charger_vol;

	mutex_lock(&ta_mutex);
	wake_lock(&TA_charger_suspend_lock);

	if(KAL_TRUE == is_ta_connect)
	{
		/* check cable impedance */
		charger_vol = battery_meter_get_charger_voltage();
		if(KAL_FALSE == ta_vchr_tuning)
		{
			mtk_ta_retry_increase();	//increase TA voltage to 9V
			charger_vol = battery_meter_get_charger_voltage();
			ta_vchr_tuning = KAL_TRUE;
		}
		else if(BMT_status.SOC > TA_STOP_BATTERY_SOC)
		{
			mtk_ta_decrease();	//decrease TA voltage to 5V
			charger_vol = battery_meter_get_charger_voltage();
			if(abs(charger_vol - ta_v_chr_org) <= 1000)	// 1.0V
				is_ta_connect = KAL_FALSE;

			battery_xlog_printk(BAT_LOG_CRTI, "Stop battery_pump_express_algorithm, SOC=%d is_ta_connect =%d\n",
				BMT_status.SOC, is_ta_connect);
		}
		battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY] check cable impedance, VA(%d) VB(%d) delta(%d).\n", 
		ta_v_chr_org, charger_vol, charger_vol - ta_v_chr_org);

		battery_xlog_printk(BAT_LOG_CRTI, "mtk_ta_algorithm() end\n");
	}
	else
	{
		battery_xlog_printk(BAT_LOG_CRTI, "It's not a TA charger, bypass TA algorithm\n");
	}

	if(ta_cable_out_occur == KAL_FALSE)
		is_ta_check_done = KAL_TRUE;

	battery_xlog_printk(BAT_LOG_CRTI, "mtk_ta_algorithm() is_ta_check_done=%d\n", is_ta_check_done);

	wake_unlock(&TA_charger_suspend_lock);
	mutex_unlock(&ta_mutex);
}
#endif

#if defined(MTK_JEITA_STANDARD_SUPPORT)

static BATTERY_VOLTAGE_ENUM select_jeita_cv(void)
{
    BATTERY_VOLTAGE_ENUM cv_voltage;

    if(g_temp_status == TEMP_ABOVE_POS_60)
    {
        cv_voltage = JEITA_TEMP_ABOVE_POS_60_CV_VOLTAGE;
    }
    else if(g_temp_status == TEMP_POS_45_TO_POS_60)
    {
        cv_voltage = JEITA_TEMP_POS_45_TO_POS_60_CV_VOLTAGE;
    }
    else if(g_temp_status == TEMP_POS_10_TO_POS_45)
    {
    	#ifdef HIGH_BATTERY_VOLTAGE_SUPPORT
        cv_voltage = BATTERY_VOLT_04_340000_V;
      #else
        cv_voltage = JEITA_TEMP_POS_10_TO_POS_45_CV_VOLTAGE;
      #endif
    }
    else if(g_temp_status == TEMP_POS_0_TO_POS_10)
    {
        cv_voltage = JEITA_TEMP_POS_0_TO_POS_10_CV_VOLTAGE;
    }
    else if(g_temp_status == TEMP_NEG_10_TO_POS_0)
    {
        cv_voltage = JEITA_TEMP_NEG_10_TO_POS_0_CV_VOLTAGE;
    }
    else if(g_temp_status == TEMP_BELOW_NEG_10)
    {
        cv_voltage = JEITA_TEMP_BELOW_NEG_10_CV_VOLTAGE;
    }
    else
    {
        cv_voltage = BATTERY_VOLT_04_200000_V;
    }            

    return cv_voltage;
}

PMU_STATUS do_jeita_state_machine(void)
{
	BATTERY_VOLTAGE_ENUM cv_voltage;
	
    //JEITA battery temp Standard 
	
    if (BMT_status.temperature >= TEMP_POS_60_THRESHOLD) 
    {
        battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY] Battery Over high Temperature(%d) !!\n\r", TEMP_POS_60_THRESHOLD);  
        
        g_temp_status = TEMP_ABOVE_POS_60;
        
        return PMU_STATUS_FAIL; 
    }
    else if(BMT_status.temperature > TEMP_POS_45_THRESHOLD)  //control 45c to normal behavior
    {
        if((g_temp_status == TEMP_ABOVE_POS_60) && (BMT_status.temperature >= TEMP_POS_60_THRES_MINUS_X_DEGREE))
        {
            battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY] Battery Temperature between %d and %d,not allow charging yet!!\n\r", TEMP_POS_60_THRES_MINUS_X_DEGREE,TEMP_POS_60_THRESHOLD); 
            
            return PMU_STATUS_FAIL; 
        }
        else
        {
             battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY] Battery Temperature between %d and %d !!\n\r", TEMP_POS_45_THRESHOLD,TEMP_POS_60_THRESHOLD); 
            
            g_temp_status = TEMP_POS_45_TO_POS_60;
        }
    }
    else if(BMT_status.temperature >= TEMP_POS_10_THRESHOLD)
    {
        if( ((g_temp_status == TEMP_POS_45_TO_POS_60) && (BMT_status.temperature >= TEMP_POS_45_THRES_MINUS_X_DEGREE)) ||
            ((g_temp_status == TEMP_POS_0_TO_POS_10 ) && (BMT_status.temperature <= TEMP_POS_10_THRES_PLUS_X_DEGREE ))) 
        {
            battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY] Battery Temperature not recovery to normal temperature charging mode yet!!\n\r");     
        }
        else
        {
            battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY] Battery Normal Temperature between %d and %d !!\n\r", TEMP_POS_10_THRESHOLD,TEMP_POS_45_THRESHOLD); 
            g_temp_status = TEMP_POS_10_TO_POS_45;
        }
    }
    else if(BMT_status.temperature >= TEMP_POS_0_THRESHOLD)
    {
        if((g_temp_status == TEMP_NEG_10_TO_POS_0 || g_temp_status == TEMP_BELOW_NEG_10) && (BMT_status.temperature <= TEMP_POS_0_THRES_PLUS_X_DEGREE))
        {
			if (g_temp_status == TEMP_NEG_10_TO_POS_0) {
				battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY] Battery Temperature between %d and %d !!\n\r", TEMP_POS_0_THRES_PLUS_X_DEGREE,TEMP_POS_10_THRESHOLD); 
			}
			if (g_temp_status == TEMP_BELOW_NEG_10) {
				battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY] Battery Temperature between %d and %d,not allow charging yet!!\n\r",	TEMP_POS_0_THRESHOLD,TEMP_POS_0_THRES_PLUS_X_DEGREE); 
				return PMU_STATUS_FAIL; 
			}
        }
        else
        {
            battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY] Battery Temperature between %d and %d !!\n\r", TEMP_POS_0_THRESHOLD,TEMP_POS_10_THRESHOLD); 
            
            g_temp_status = TEMP_POS_0_TO_POS_10;
        }
    }
    else if(BMT_status.temperature >= TEMP_NEG_10_THRESHOLD)
    {
        if((g_temp_status == TEMP_BELOW_NEG_10) && (BMT_status.temperature <= TEMP_NEG_10_THRES_PLUS_X_DEGREE))
        {
            battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY] Battery Temperature between %d and %d,not allow charging yet!!\n\r", TEMP_NEG_10_THRESHOLD,TEMP_NEG_10_THRES_PLUS_X_DEGREE); 
            
            return PMU_STATUS_FAIL; 
        }
        else
        {
            battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY] Battery Temperature between %d and %d !!\n\r",  TEMP_NEG_10_THRESHOLD,TEMP_POS_0_THRESHOLD); 
            
            g_temp_status = TEMP_NEG_10_TO_POS_0;
        }
    }
    else
    {
        battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY] Battery below low Temperature(%d) !!\n\r", TEMP_NEG_10_THRESHOLD);  
        g_temp_status = TEMP_BELOW_NEG_10;
        
        return PMU_STATUS_FAIL; 
    }

	//set CV after temperature changed

	cv_voltage = select_jeita_cv();
	battery_charging_control(CHARGING_CMD_SET_CV_VOLTAGE,&cv_voltage);
	
	return PMU_STATUS_OK;
}


static void set_jeita_charging_current(void)
{
#ifdef CONFIG_USB_IF
	if(BMT_status.charger_type == STANDARD_HOST)
		return;
#endif	

	if(g_temp_status == TEMP_NEG_10_TO_POS_0)
    {
        g_temp_CC_value = CHARGE_CURRENT_350_00_MA;
		g_temp_input_CC_value = CHARGE_CURRENT_500_00_MA;	
        battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY] JEITA set charging current : %d\r\n", g_temp_CC_value);
    }
}

#endif

bool get_usb_current_unlimited(void)
{
	if (BMT_status.charger_type == STANDARD_HOST || BMT_status.charger_type == CHARGING_HOST)
		return usb_unlimited;
	else
		return false;
}

void set_usb_current_unlimited(bool enable)
{
	usb_unlimited = enable;
}

static void pchr_turn_on_charging (void);
kal_uint32 set_bat_charging_current_limit(int current_limit)
{
    battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY] set_bat_charging_current_limit (%d)\r\n", current_limit);

    if(current_limit != -1)
    {
        g_bcct_value = current_limit;
        
        if(current_limit < 70)         g_temp_CC_value=CHARGE_CURRENT_0_00_MA;
        else if(current_limit < 200)   g_temp_CC_value=CHARGE_CURRENT_70_00_MA;
        else if(current_limit < 300)   g_temp_CC_value=CHARGE_CURRENT_200_00_MA;
        else if(current_limit < 400)   g_temp_CC_value=CHARGE_CURRENT_300_00_MA;
        else if(current_limit < 450)   g_temp_CC_value=CHARGE_CURRENT_400_00_MA;
        else if(current_limit < 550)   g_temp_CC_value=CHARGE_CURRENT_450_00_MA;
        else if(current_limit < 650)   g_temp_CC_value=CHARGE_CURRENT_550_00_MA;
        else if(current_limit < 700)   g_temp_CC_value=CHARGE_CURRENT_650_00_MA;
        else if(current_limit < 800)   g_temp_CC_value=CHARGE_CURRENT_700_00_MA;
        else if(current_limit < 900)   g_temp_CC_value=CHARGE_CURRENT_800_00_MA;
        else if(current_limit < 1000)  g_temp_CC_value=CHARGE_CURRENT_900_00_MA;
        else if(current_limit < 1100)  g_temp_CC_value=CHARGE_CURRENT_1000_00_MA;
        else if(current_limit < 1200)  g_temp_CC_value=CHARGE_CURRENT_1100_00_MA;
        else if(current_limit < 1300)  g_temp_CC_value=CHARGE_CURRENT_1200_00_MA;
        else if(current_limit < 1400)  g_temp_CC_value=CHARGE_CURRENT_1300_00_MA;
        else if(current_limit < 1500)  g_temp_CC_value=CHARGE_CURRENT_1400_00_MA;
        else if(current_limit < 1600)  g_temp_CC_value=CHARGE_CURRENT_1500_00_MA;
        else if(current_limit == 1600) g_temp_CC_value=CHARGE_CURRENT_1600_00_MA;
        else                           g_temp_CC_value=CHARGE_CURRENT_450_00_MA;
    }
    else
    {
        //change to default current setting
        g_bcct_flag=0;
    }
    
    return g_bcct_flag;
}    

void select_charging_curret_bcct(void)
{
	set_bat_charging_current_limit(MED12C_CHARGER_CURRENT / 100);
	if (BMT_status.charger_type == STANDARD_HOST) {
		if ((musb_mode == CABLE_MODE_CHRG_ONLY) ||
			(musb_mode == CABLE_MODE_HOST_ONLY)) {
			g_temp_input_CC_value = CHARGE_CURRENT_1000_00_MA;
		} else {
			g_temp_input_CC_value = USB_CHARGER_CURRENT;
			g_temp_CC_value = USB_CHARGER_CURRENT;
		}
	}else if( (BMT_status.charger_type == STANDARD_CHARGER) ||
             (BMT_status.charger_type == CHARGING_HOST) ) {
		g_temp_input_CC_value = STANDARD_CHARGER_CURRENT;
	 } else if (BMT_status.charger_type == MEIZU_2_0A_CHARGER) {
		g_temp_input_CC_value = MEIZU_2_0A_CHARGER_CURRENT;
	 } else {
		g_temp_input_CC_value = CHARGE_CURRENT_500_00_MA;
	 }

        /*------mask it by tangxingyan2014.6.28------------------------
        -------because set the CC in the set_bat_charging_current_limit---
	*/
#if 0
     if ( (BMT_status.charger_type == STANDARD_HOST) || 
         (BMT_status.charger_type == NONSTANDARD_CHARGER) )
    {
        if(g_bcct_value < 100)        g_temp_input_CC_value = CHARGE_CURRENT_0_00_MA ;
        else if(g_bcct_value < 500)   g_temp_input_CC_value = CHARGE_CURRENT_100_00_MA;
        else if(g_bcct_value < 800)   g_temp_input_CC_value = CHARGE_CURRENT_500_00_MA;
        else if(g_bcct_value == 800)  g_temp_input_CC_value = CHARGE_CURRENT_800_00_MA;
        else                          g_temp_input_CC_value = CHARGE_CURRENT_500_00_MA;     
    }
    else if( (BMT_status.charger_type == STANDARD_CHARGER) ||
             (BMT_status.charger_type == CHARGING_HOST) )
    {
    	g_temp_input_CC_value = CHARGE_CURRENT_MAX;

        //set IOCHARGE
        if(g_bcct_value < 550)        g_temp_CC_value = CHARGE_CURRENT_0_00_MA;
        else if(g_bcct_value < 650)   g_temp_CC_value = CHARGE_CURRENT_550_00_MA;
        else if(g_bcct_value < 750)   g_temp_CC_value = CHARGE_CURRENT_650_00_MA;
        else if(g_bcct_value < 850)   g_temp_CC_value = CHARGE_CURRENT_750_00_MA;
        else if(g_bcct_value < 950)   g_temp_CC_value = CHARGE_CURRENT_850_00_MA;
        else if(g_bcct_value < 1050)  g_temp_CC_value = CHARGE_CURRENT_950_00_MA;
        else if(g_bcct_value < 1150)  g_temp_CC_value = CHARGE_CURRENT_1050_00_MA;
        else if(g_bcct_value < 1250)  g_temp_CC_value = CHARGE_CURRENT_1150_00_MA;
        else if(g_bcct_value == 1250) g_temp_CC_value = CHARGE_CURRENT_1250_00_MA;
        else                          g_temp_CC_value = CHARGE_CURRENT_650_00_MA;
    }
    else
    {
        g_temp_input_CC_value = CHARGE_CURRENT_500_00_MA;           
    } 
#endif
}

#define CHARGER_CURRENT_1A_THRESHOLD 4550
#define CHARGER_CURRENT_1_2A_THRESHOLD 4730
static kal_uint32 bmt_find_closest_current_level(kal_uint32 current_level);
void select_charging_curret(void)
{
	kal_uint32 charger_voltage = 0, sum_voltage = 0;
	kal_uint32 current_level = 0;
	int i;
    if (g_ftm_battery_flag) 
    {
        battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY] FTM charging : %d\r\n", charging_level_data[0]);    
        g_temp_CC_value = charging_level_data[0];

        if(g_temp_CC_value == CHARGE_CURRENT_450_00_MA)
        { 
			g_temp_input_CC_value = CHARGE_CURRENT_500_00_MA;	
        }
        else
        {            
			g_temp_input_CC_value = CHARGE_CURRENT_MAX;
			g_temp_CC_value = STANDARD_CHARGER_CURRENT;

            battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY] set_ac_current \r\n");    
        }        
    }
    else 
    {    
        if ( BMT_status.charger_type == STANDARD_HOST ) 
        {
           #ifdef CONFIG_USB_IF
            {
            	g_temp_input_CC_value = CHARGE_CURRENT_MAX;
                if (g_usb_state == USB_SUSPEND)
                {
                    g_temp_CC_value = USB_CHARGER_CURRENT_SUSPEND;
                }
                else if (g_usb_state == USB_UNCONFIGURED)
                {
                    g_temp_CC_value = USB_CHARGER_CURRENT_UNCONFIGURED;
                }
                else if (g_usb_state == USB_CONFIGURED)
                {
                    g_temp_CC_value = USB_CHARGER_CURRENT_CONFIGURED;
                }
                else
                {
                    g_temp_CC_value = USB_CHARGER_CURRENT_UNCONFIGURED;
                }
                
                 battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY] STANDARD_HOST CC mode charging : %d on %d state\r\n", g_temp_CC_value, g_usb_state);
            }
			#else
            {    
		if ((musb_mode == CABLE_MODE_CHRG_ONLY) ||
			(musb_mode == CABLE_MODE_HOST_ONLY)) {
		    g_temp_input_CC_value = USB_CHG_ONLY_CURRENT;
		    g_temp_CC_value = USB_CHG_ONLY_CURRENT;
		} else {
		    g_temp_input_CC_value = USB_CHARGER_CURRENT;
		    g_temp_CC_value = USB_CHARGER_CURRENT;
		}
            }
			#endif
        } 
        else if (BMT_status.charger_type == NONSTANDARD_CHARGER) 
        {
	    g_temp_input_CC_value = NON_STD_AC_CHARGER_CURRENT;	
	    g_temp_CC_value = NON_STD_AC_CHARGER_CURRENT;

        } 
       else if (BMT_status.charger_type == MEIZU_2_0A_CHARGER) 
        {
	    g_temp_input_CC_value = MEIZU_2_0A_CHARGER_CURRENT;	
	    g_temp_CC_value = MEIZU_2_0A_CHARGER_CURRENT;
        } 
        else if (BMT_status.charger_type == STANDARD_CHARGER) 
        {
	    g_temp_input_CC_value = MEIZU_2_0A_CHARGER_CURRENT;
	    g_temp_CC_value = MEIZU_2_0A_CHARGER_CURRENT;
	    current_level=bmt_find_closest_current_level(g_temp_CC_value);
	    battery_charging_control(CHARGING_CMD_SET_CURRENT,&current_level);
	    msleep(100);
	    for (i=0; i<5; i++) {
		    charger_voltage = battery_meter_get_charger_voltage();
		    sum_voltage += charger_voltage;
		    msleep(10);
	    }
	    charger_voltage = sum_voltage / 5;
	    if (charger_voltage <= CHARGER_CURRENT_1A_THRESHOLD) {
		    g_temp_CC_value = STANDARD_CHARGER_CURRENT; 
	    } else if (charger_voltage <= CHARGER_CURRENT_1_2A_THRESHOLD){
		    g_temp_CC_value = MEIZU_1_5A_CHARGER_CURRENT; 
	    } else {
		    g_temp_CC_value = MEIZU_2_0A_CHARGER_CURRENT; 
	    }
        }
        else if (BMT_status.charger_type == CHARGING_HOST) 
        {
	    g_temp_input_CC_value = CHARGING_HOST_CHARGER_CURRENT;
            g_temp_CC_value = CHARGING_HOST_CHARGER_CURRENT;
        }
	else if (BMT_status.charger_type == APPLE_CHARGER) 
        {
            g_temp_input_CC_value = APPLE_2_1A_CHARGER_CURRENT;
            g_temp_CC_value = APPLE_2_1A_CHARGER_CURRENT;
	    battery_charging_control(CHARGING_CMD_SET_CURRENT,&g_temp_CC_value);
	    msleep(100);
	    for (i=0; i<5; i++) {
		    charger_voltage = battery_meter_get_charger_voltage();
		    sum_voltage += charger_voltage;
		    msleep(10);
	    }
	    charger_voltage = sum_voltage / 5;
	    if (charger_voltage < CHARGER_CURRENT_1A_THRESHOLD) {
		    g_temp_CC_value = APPLE_1_0A_CHARGER_CURRENT;
	    } else {
		    g_temp_CC_value = APPLE_2_1A_CHARGER_CURRENT;
	    }
        }
	else if (BMT_status.charger_type == U200_CHARGER) 
        {
            g_temp_input_CC_value = U200_1A_CHARGER_CURRENT;
            g_temp_CC_value = U200_1A_CHARGER_CURRENT;
	}
        else 
        {
	    g_temp_input_CC_value = USB_CHARGER_CURRENT;	
            g_temp_CC_value = USB_CHARGER_CURRENT;
        }     

#if defined(MTK_JEITA_STANDARD_SUPPORT)
	set_jeita_charging_current();
#endif		
    }
}

static kal_uint32 charging_full_check(void)
{
	kal_uint32 status;

	battery_charging_control(CHARGING_CMD_GET_CHARGING_STATUS,&status);
	if ( status == KAL_TRUE) {
		g_full_check_count++;
		if (g_full_check_count >= FULL_CHECK_TIMES) {
			return KAL_TRUE;
		}
		else
			return KAL_FALSE;
	} else {
		g_full_check_count = 0;
	return status;
}
}

//add by willcai for configing the current by vchg  begin
/*
This function  config the vbat  current from vchg 
ICHG=IBAT*VBAT/(VCHG*0.78)  VCHG<10v
ICHG=IBAT*VBAT/(VCHG*0.73)  VCHG>10v

*/
static kal_uint32 bmt_find_closest_current_level(kal_uint32 current_level)
{
    //add by willcai  8-13
	kal_uint32 vchg_level  ;
	//vchg_level =(260*current_level)/(5*BMT_status.charger_vol);//14-8-12
	battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY] bmt_find_closest_current_level  BMT_status.bat_vol=%d BMT_status.charger_vol=%d!\r\n",BMT_status.bat_vol ,BMT_status.charger_vol);
	current_level= current_level/100;
	if(BMT_status.charger_vol <10000)//2014-8-16 willcai modify 
		{
		vchg_level =(50*BMT_status.bat_vol *current_level)/(BMT_status.charger_vol*39);
	   }
	else
		{
		vchg_level =(100*BMT_status.bat_vol *current_level)/(BMT_status.charger_vol*73);

	   }
	vchg_level=100*vchg_level;
	battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY] bmt_find_closest_current_level  vchg_level=%d !\r\n",vchg_level);
	
		if(vchg_level>CHARGE_CURRENT_1650_00_MA)
		{
			current_level=CHARGE_CURRENT_1700_00_MA;
		}
		else if(vchg_level>CHARGE_CURRENT_1525_00_MA)
		{
			current_level=CHARGE_CURRENT_1600_00_MA;

		}
		else if(vchg_level>CHARGE_CURRENT_1450_00_MA)
		{
			current_level=CHARGE_CURRENT_1500_00_MA;

		}
		else if(vchg_level>CHARGE_CURRENT_1350_00_MA)
		{
			current_level=CHARGE_CURRENT_1400_00_MA;

		}
		else if(vchg_level>CHARGE_CURRENT_1250_00_MA)
		{
			current_level=CHARGE_CURRENT_1300_00_MA;

		}
		else if(vchg_level>CHARGE_CURRENT_1150_00_MA)
		{
			current_level=CHARGE_CURRENT_1200_00_MA;

		}
		else if(vchg_level>CHARGE_CURRENT_1050_00_MA)
		{
			current_level=CHARGE_CURRENT_1100_00_MA;

		}
		else if(vchg_level>CHARGE_CURRENT_950_00_MA)
		{
			current_level=CHARGE_CURRENT_1000_00_MA;

		}
		else if(vchg_level>CHARGE_CURRENT_850_00_MA)
		{
			current_level=CHARGE_CURRENT_900_00_MA;

		}
		else if(vchg_level>CHARGE_CURRENT_750_00_MA)
		{
			current_level=CHARGE_CURRENT_800_00_MA;

		}
		else if(vchg_level>CHARGE_CURRENT_650_00_MA)
		{
			current_level=CHARGE_CURRENT_700_00_MA;

		}
		else if(vchg_level>CHARGE_CURRENT_550_00_MA)
		{
			current_level=CHARGE_CURRENT_600_00_MA;

		}
		else if(vchg_level>CHARGE_CURRENT_450_00_MA)
		{
			current_level=CHARGE_CURRENT_500_00_MA;

		}
		else if(vchg_level>CHARGE_CURRENT_350_00_MA)
		{
			current_level=CHARGE_CURRENT_400_00_MA;

		}
		else 
		{
			current_level=CHARGE_CURRENT_300_00_MA;
		}
       return current_level ;	
}
//end

static void pchr_turn_on_charging (void)
{
#if !defined(MTK_JEITA_STANDARD_SUPPORT) 
	BATTERY_VOLTAGE_ENUM cv_voltage;
#endif	
	kal_uint32 charging_enable = KAL_TRUE;
// add by willcai begin 
	kal_uint32 current_level = 0;
//end

    if ( BMT_status.bat_charging_state == CHR_ERROR ) 
    {
        battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY] Charger Error, turn OFF charging !\n");

		charging_enable = KAL_FALSE;
    }
    else if( (g_platform_boot_mode==META_BOOT) || (g_platform_boot_mode==ADVMETA_BOOT) )
    {   
        battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY] In meta or advanced meta mode, disable charging.\n");    
        charging_enable = KAL_FALSE;
    }
    else
    {
        /*HW initialization*/
        battery_charging_control(CHARGING_CMD_INIT,NULL);
	battery_xlog_printk(BAT_LOG_FULL, "charging_hw_init\n" );

#if defined(MTK_PUMP_EXPRESS_SUPPORT)
		battery_pump_express_algorithm_start();
#endif
	/* enable/disable charging */
  	battery_charging_control(CHARGING_CMD_ENABLE,&charging_enable);

	/* get the usb charging mode*/
	check_musb_mode(&musb_mode);

        /* Set Charging Current */			
        if (get_usb_current_unlimited()) {
            g_temp_input_CC_value = USB_CHG_ONLY_CURRENT;
            g_temp_CC_value = USB_CHG_ONLY_CURRENT;
            battery_xlog_printk(BAT_LOG_FULL, "USB_CURRENT_UNLIMITED, use AC_CHARGER_CURRENT\n" );
        }			
        else {
	        if (g_bcct_flag == 1)
	        {
	            select_charging_curret_bcct();
		    g_bcct_flag = 0;
	
		    battery_xlog_printk(BAT_LOG_FULL, "[BATTERY] select_charging_curret_bcct !\n");
	        }
	        else
	        {
	            select_charging_curret();

		//add by willcai begin  for PE+  8-14
		current_level=bmt_find_closest_current_level(g_temp_CC_value);
		g_temp_CC_value = current_level;
		//end
	             battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY] bmt_find_closest_current_level  current_level=%d !\r\n",current_level);
		    battery_xlog_printk(BAT_LOG_FULL, "[BATTERY] select_charging_curret !\n");
	        }
        	}
//modify by willcai 2014-5-29 begin
	 #ifdef X2_CHARGING_STANDARD_SUPPORT
	    lenovo_get_charging_curret(&g_temp_CC_value);
	 #endif
//end
#ifdef CONFIG_MT_ENG_BUILD
        printk("[BATTERY] Default CC mode charging : %d, input current = %d\r\n", g_temp_CC_value,g_temp_input_CC_value);
#endif
        if( g_temp_CC_value == CHARGE_CURRENT_0_00_MA || g_temp_input_CC_value == CHARGE_CURRENT_0_00_MA)
        {
        
			charging_enable = KAL_FALSE;
						
            battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY] charging current is set 0mA, turn off charging !\r\n");
        }
        else
        {    
      		battery_charging_control(CHARGING_CMD_SET_INPUT_CURRENT,&g_temp_input_CC_value);
		battery_charging_control(CHARGING_CMD_SET_CURRENT,&g_temp_CC_value);

		/*Set CV Voltage*/
		#if !defined(MTK_JEITA_STANDARD_SUPPORT)            
                #ifdef HIGH_BATTERY_VOLTAGE_SUPPORT
                    cv_voltage = BATTERY_VOLT_04_350000_V;
                #else
	                cv_voltage = BATTERY_VOLT_04_350000_V;
                #endif            
		    battery_charging_control(CHARGING_CMD_SET_CV_VOLTAGE,&cv_voltage);
		#endif
        }
    }
	battery_xlog_printk(BAT_LOG_FULL, "[BATTERY] pchr_turn_on_charging(), enable =%d !\r\n", charging_enable);
}
 
PMU_STATUS BAT_PreChargeModeAction(void)
{
    battery_xlog_printk(BAT_LOG_FULL, "[BATTERY] Pre-CC mode charge, timer=%ld on %ld !!\n\r", BMT_status.PRE_charging_time, BMT_status.total_charging_time);    
    
    BMT_status.PRE_charging_time += BAT_TASK_PERIOD;
    BMT_status.CC_charging_time = 0;
    BMT_status.TOPOFF_charging_time = 0;
    BMT_status.total_charging_time += BAT_TASK_PERIOD;

    /*  Enable charger */
    pchr_turn_on_charging();            

	if (BMT_status.UI_SOC == 100)
	{
		BMT_status.bat_charging_state = CHR_BATFULL;
		BMT_status.bat_full = KAL_TRUE;
		g_charging_full_reset_bat_meter = KAL_TRUE;
	}
	else if ( BMT_status.bat_vol > V_PRE2CC_THRES )
	{
		BMT_status.bat_charging_state = CHR_CC;
	}

    return PMU_STATUS_OK;        
} 


PMU_STATUS BAT_ConstantCurrentModeAction(void)
{
    battery_xlog_printk(BAT_LOG_FULL, "[BATTERY] CC mode charge, timer=%ld on %ld !!\n\r", BMT_status.CC_charging_time, BMT_status.total_charging_time);    

    BMT_status.PRE_charging_time = 0;
    BMT_status.CC_charging_time += BAT_TASK_PERIOD;
    BMT_status.TOPOFF_charging_time = 0;
    BMT_status.total_charging_time += BAT_TASK_PERIOD;

    /*  Enable charger */
    pchr_turn_on_charging();     

	if(charging_full_check() == KAL_TRUE)
	{
	    BMT_status.bat_charging_state = CHR_BATFULL;
		BMT_status.bat_full = KAL_TRUE;
		g_charging_full_reset_bat_meter = KAL_TRUE;
	}	
    	
    return PMU_STATUS_OK;        
}    

PMU_STATUS BAT_BatteryFullAction(void)
{
    battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY] Battery full !!\n\r");            
    
    BMT_status.bat_full = KAL_TRUE;
    BMT_status.total_charging_time = 0;
    BMT_status.PRE_charging_time = 0;
    BMT_status.CC_charging_time = 0;
    BMT_status.TOPOFF_charging_time = 0;
    BMT_status.POSTFULL_charging_time = 0;
	BMT_status.bat_in_recharging_state = KAL_FALSE;

    if(charging_full_check() == KAL_FALSE)
    {
        battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY] Battery Re-charging !!\n\r");                

	BMT_status.bat_in_recharging_state = KAL_TRUE;
        BMT_status.bat_charging_state = CHR_CC;
	battery_meter_reset();
    }        
    return PMU_STATUS_OK;
}


PMU_STATUS BAT_BatteryHoldAction(void)
{
	kal_uint32 charging_enable;
	
	battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY] Hold mode !!\n\r");
	 
	if(BMT_status.bat_vol < TALKING_RECHARGE_VOLTAGE || g_call_state == CALL_IDLE)
	{
		BMT_status.bat_charging_state = CHR_CC;
		battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY] Exit Hold mode and Enter CC mode !!\n\r");
	}	
		
	/*  Disable charger */
	charging_enable = KAL_FALSE;
	battery_charging_control(CHARGING_CMD_ENABLE,&charging_enable);	

    return PMU_STATUS_OK;
}


PMU_STATUS BAT_BatteryStatusFailAction(void)
{
	kal_uint32 charging_enable;
	
    battery_xlog_printk(BAT_LOG_CRTI, "[BATTERY] BAD Battery status... Charging Stop !!\n\r");            

#if defined(MTK_JEITA_STANDARD_SUPPORT)
    if((g_temp_status == TEMP_ABOVE_POS_60) ||(g_temp_status == TEMP_BELOW_NEG_10))
    {
        temp_error_recovery_chr_flag=KAL_FALSE;
    }	
    if((temp_error_recovery_chr_flag==KAL_FALSE) && (g_temp_status != TEMP_ABOVE_POS_60) && (g_temp_status != TEMP_BELOW_NEG_10))
    {
        temp_error_recovery_chr_flag=KAL_TRUE;
        BMT_status.bat_charging_state=CHR_PRE;
    }
#endif

    BMT_status.total_charging_time = 0;
    BMT_status.PRE_charging_time = 0;
    BMT_status.CC_charging_time = 0;
    BMT_status.TOPOFF_charging_time = 0;
    BMT_status.POSTFULL_charging_time = 0;

    /*  Disable charger */
	charging_enable = KAL_FALSE;
	battery_charging_control(CHARGING_CMD_ENABLE,&charging_enable);

    return PMU_STATUS_OK;
}


void mt_battery_charging_algorithm()
{
	battery_charging_control(CHARGING_CMD_RESET_WATCH_DOG_TIMER,NULL);
	/* set the temperature charging control */
	get_batt_temp_status(&batt_temp_status);
	if (batt_temp_status == TEMP_POS_MEDIUM)
	    g_bcct_flag = 1;

	switch(BMT_status.bat_charging_state)
    {            
        case CHR_PRE :
            BAT_PreChargeModeAction();
            break;    

	case CHR_CC :
	    BAT_ConstantCurrentModeAction();
	    break;	 
			 
        case CHR_BATFULL:
            BAT_BatteryFullAction();
            break;
            
        case CHR_HOLD:
	    BAT_BatteryHoldAction();
	    break;		    
			
        case CHR_ERROR:
            BAT_BatteryStatusFailAction();
            break;                
    }    
    battery_charging_control(CHARGING_CMD_DUMP_REGISTER,NULL);
}
 

