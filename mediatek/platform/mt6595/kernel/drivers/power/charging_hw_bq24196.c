/*****************************************************************************
 *
 * Filename:
 * ---------
 *    charging_pmic.c
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
#include <mach/charging.h>
#include "bq24196.h"
#include <mach/upmu_common.h>
#include <mach/mt_gpio.h>
#include <cust_gpio_usage.h>
#include <mach/upmu_hw.h>
#include <linux/xlog.h>
#include <linux/delay.h>
#include <mach/mt_sleep.h>
#include <mach/mt_boot.h>
#include <mach/system.h>
#include <cust_charging.h>
#include <mach/upmu_common.h>

#define STATUS_OK	0
#define STATUS_UNSUPPORTED	-1
#define GETARRAYNUM(array) (sizeof(array)/sizeof(array[0]))

kal_bool charging_type_det_done = KAL_TRUE;
extern kal_uint32 upmu_get_rgs_chrdet(void);

const kal_uint32 VBAT_CV_VTH[]=
{
	3504000,    3520000,    3536000,    3552000,
	3568000,    3584000,    3600000,    3616000,
	3632000,    3648000,    3664000,    3680000,
	3696000,    3712000,	3728000,    3744000,
	3760000,    3776000,    3792000,    3808000,
	3824000,    3840000,    3856000,    3872000,
	3888000,    3904000,    3920000,    3936000,
	3952000,    3968000,    3984000,    4000000,
	4016000,    4032000,    4048000,    4064000,
	4080000,    4096000,    4112000,    4128000,
	4144000,    4160000,    4176000,    4192000,	
	4208000,    4224000,    4240000,    4256000,
	4272000,    4288000,    4304000,    4320000,
	4336000,    4352000,    4368000,    4384000,
	4400000
};

const kal_uint32 CS_VTH[]=
{
	51200,  57600,  64000,  70400,
	76800,  83200,  89600,  96000,
	102400, 108800, 115200, 121600,
	128000, 134400, 140800, 147200,
	153600, 160000, 166400, 172800,
	179200, 185600, 192000, 198400,
	204800, 211200, 217600, 224000
}; 

 const kal_uint32 INPUT_CS_VTH[]=
 {
	 CHARGE_CURRENT_100_00_MA,  CHARGE_CURRENT_150_00_MA,	CHARGE_CURRENT_500_00_MA,  CHARGE_CURRENT_900_00_MA,
	 CHARGE_CURRENT_1200_00_MA, CHARGE_CURRENT_1500_00_MA,  CHARGE_CURRENT_2000_00_MA,  CHARGE_CURRENT_MAX
 }; 

 const kal_uint32 VCDT_HV_VTH[]=
 {
	  BATTERY_VOLT_04_200000_V, BATTERY_VOLT_04_250000_V,	  BATTERY_VOLT_04_300000_V,   BATTERY_VOLT_04_350000_V,
	  BATTERY_VOLT_04_400000_V, BATTERY_VOLT_04_450000_V,	  BATTERY_VOLT_04_500000_V,   BATTERY_VOLT_04_550000_V,
	  BATTERY_VOLT_04_600000_V, BATTERY_VOLT_06_000000_V,	  BATTERY_VOLT_06_500000_V,   BATTERY_VOLT_07_000000_V,
	  BATTERY_VOLT_07_500000_V, BATTERY_VOLT_08_500000_V,	  BATTERY_VOLT_09_500000_V,   BATTERY_VOLT_10_500000_V		  
 };

 extern bool mt_usb_is_device(void);
 
 // ============================================================ //
 kal_uint32 charging_value_to_parameter(const kal_uint32 *parameter, const kal_uint32 array_size, const kal_uint32 val)
{
	if (val < array_size)
	{
		return parameter[val];
	}
	else
	{
		battery_xlog_printk(BAT_LOG_CRTI, "Can't find the parameter \r\n");	
		return parameter[0];
	}
}

 
 kal_uint32 charging_parameter_to_value(const kal_uint32 *parameter, const kal_uint32 array_size, const kal_uint32 val)
{
	kal_uint32 i;

    battery_xlog_printk(BAT_LOG_CRTI, "array_size = %d \r\n", array_size);
    
	for(i=0;i<array_size;i++)
	{
		if ((val > *(parameter + i)) && (val < *(parameter + i +1)))
		{
			return i + 1;
		} else if (val == *(parameter + i)) {
			return i;
	    	}
	}

    battery_xlog_printk(BAT_LOG_CRTI, "NO register value match. val=%d\r\n", val);
	//TODO: ASSERT(0);	// not find the value
	return 0;
}

 static kal_uint32 bmt_find_closest_level(const kal_uint32 *pList,kal_uint32 number,kal_uint32 level)
 {
	 kal_uint32 i;
	 kal_uint32 max_value_in_last_element;
 
	 if(pList[0] < pList[1])
		 max_value_in_last_element = KAL_TRUE;
	 else
		 max_value_in_last_element = KAL_FALSE;
 
	 if(max_value_in_last_element == KAL_TRUE)
	 {
		 for(i = (number-1); i != 0; i--)	 //max value in the last element
		 {
			 if(pList[i] <= level)
			 {
				 return pList[i];
			 }	  
		 }

 		 battery_xlog_printk(BAT_LOG_CRTI, "Can't find closest level, small value first \r\n");
		 return pList[0];
		 //return CHARGE_CURRENT_0_00_MA;
	 }
	 else
	 {
		 for(i = 0; i< number; i++)  // max value in the first element
		 {
			 if(pList[i] <= level)
			 {
				 return pList[i];
			 }	  
		 }

		 battery_xlog_printk(BAT_LOG_CRTI, "Can't find closest level, large value first \r\n"); 	 
		 return pList[number -1];
  		 //return CHARGE_CURRENT_0_00_MA;
	 }
 }

 static kal_uint32 charging_hw_init(void *data)
 {
 	kal_uint32 status = STATUS_OK;
	
	mt6332_upmu_set_rg_bc12_bb_ctrl(1);    //BC11_BB_CTRL    
    	mt6332_upmu_set_rg_bc12_rst(1);        //BC11_RST
	
#if 1 
    //pull PSEL low.(PSEL is LOW means that the input source is Adapter, HIGH is USB Host)
    mt_set_gpio_mode(GPIO_CHR_PSEL_PIN,GPIO_MODE_GPIO);  
    mt_set_gpio_dir(GPIO_CHR_PSEL_PIN,GPIO_DIR_OUT);
 //LOW means the input source is Adapter, the max input current is 3A
    mt_set_gpio_out(GPIO_CHR_PSEL_PIN,GPIO_OUT_ZERO);
#endif    
    
#if 1
    //pull CE low.(CR LOW meas the charger enable, High is Disable)
    mt_set_gpio_mode(GPIO_SWCHARGER_EN_PIN,GPIO_MODE_GPIO);  
    mt_set_gpio_dir(GPIO_SWCHARGER_EN_PIN,GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_SWCHARGER_EN_PIN,GPIO_OUT_ZERO);    
#endif

#if 1
    //pull OTG low.(CR LOW meas the charger enable, High is Disable)
    mt_set_gpio_mode(GPIO_OTG_DRVVBUS_PIN,GPIO_MODE_GPIO);  
    mt_set_gpio_dir(GPIO_OTG_DRVVBUS_PIN,GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_OTG_DRVVBUS_PIN,GPIO_OUT_ZERO);    
#endif
    //battery_xlog_printk(BAT_LOG_FULL, "gpio_number=0x%x,gpio_on_mode=%d,gpio_off_mode=%d\n", gpio_number, gpio_on_mode, gpio_off_mode);	

    bq24196_set_en_hiz(0x0);
    bq24196_set_vindpm(0xA); //VIN DPM check 4.68V
    bq24196_set_reg_rst(0x0);
    bq24196_set_wdt_rst(0x1); //Kick watchdog	
    bq24196_set_sys_min(0x5); //Minimum system voltage 3.5V	
    bq24196_set_iprechg(0x3); //Precharge current 512mA
    bq24196_set_iterm(0x0); //Termination current 128mA

    bq24196_set_vreg(0x36); //VREG 4.352V
  
    bq24196_set_batlowv(0x1); //BATLOWV 3.0V
    bq24196_set_vrechg(0x0); //VRECHG 0.1V (4.108V)
    bq24196_set_en_term(0x1); //Enable termination
    bq24196_set_term_stat(0x0); //Match ITERM
    bq24196_set_watchdog(0x1); //WDT 40s
    bq24196_set_en_timer(0x0); //Disable charge timer
    bq24196_set_int_mask(0x0); //Disable fault interrupt
    
    return status;
 }

static kal_uint32 charging_dump_register(void *data)
 {
 	kal_uint32 status = STATUS_OK;

    battery_xlog_printk(BAT_LOG_CRTI, "charging_dump_register\r\n");

	bq24196_dump_register();
   	
	return status;
 }

void  tbl_charger_otg_vbus(kal_uint32 mode)
{
	kal_uint32 state = mode&0x1;

	if(state){
		mt_set_gpio_out(GPIO_OTG_DRVVBUS_PIN, GPIO_OUT_ONE);
		bq24196_set_chg_config(0x2);
	}else{
		bq24196_set_chg_config(0x0);
		mt_set_gpio_out(GPIO_OTG_DRVVBUS_PIN, GPIO_OUT_ZERO);  
	}
}
EXPORT_SYMBOL(tbl_charger_otg_vbus);

 static kal_uint32 charging_enable(void *data)
 {
 	kal_uint32 status = STATUS_OK;
	kal_uint32 enable = *(kal_uint32*)(data);

	if(KAL_TRUE == enable)
	{
		bq24196_set_en_hiz(0x0);	            	
		bq24196_set_chg_config(0x1); // charger enable
	}
	else
	{
#if defined(CONFIG_USB_MTK_HDRC_HCD)
		if(mt_usb_is_device())
#endif
		{
			bq24196_set_chg_config(0x0);
		}
	}

	return status;
 }

 static kal_uint32 charging_set_cv_voltage(void *data)
 {
 	kal_uint32 status = STATUS_OK;
	kal_uint16 register_value;
	kal_uint32 cv_value = *(kal_uint32 *)(data);	
	
	if(cv_value == BATTERY_VOLT_04_200000_V)
	{
	    //use nearest value
		cv_value = 4208000;
 	} 

	register_value = charging_parameter_to_value(VBAT_CV_VTH, GETARRAYNUM(VBAT_CV_VTH), cv_value);
	bq24196_set_vreg(register_value); 

	return status;
 } 	

 static kal_uint32 charging_get_current(void *data)
 {
    kal_uint32 status = STATUS_OK;
    //kal_uint32 array_size;
    //kal_uint8 reg_value;
    
    kal_uint8 ret_val=0;    
    kal_uint8 ret_force_20pct=0;
	    
    //Get current level
    bq24196_read_interface(bq24196_CON2, &ret_val, CON2_ICHG_MASK, CON2_ICHG_SHIFT);
    
    //Get Force 20% option
    bq24196_read_interface(bq24196_CON2, &ret_force_20pct, CON2_FORCE_20PCT_MASK, CON2_FORCE_20PCT_SHIFT);
    
    //Parsing
    ret_val = (ret_val*64) + 512;
    
    if (ret_force_20pct == 0)
    {
        //Get current level
        //array_size = GETARRAYNUM(CS_VTH);
        //*(kal_uint32 *)data = charging_value_to_parameter(CS_VTH,array_size,reg_value);
        *(kal_uint32 *)data = ret_val;
    }   
    else
    {
        //Get current level
        //array_size = GETARRAYNUM(CS_VTH_20PCT);
        //*(kal_uint32 *)data = charging_value_to_parameter(CS_VTH,array_size,reg_value);
        //return (int)(ret_val<<1)/10;
        *(kal_uint32 *)data = (int)(ret_val<<1)/10;
    }   
	
    return status;
 }  

 static kal_uint32 charging_set_current(void *data)
 {
 	kal_uint32 status = STATUS_OK;
	kal_uint32 set_chr_current;
	kal_uint32 array_size;
	kal_uint32 register_value;
	kal_uint32 current_value = *(kal_uint32 *)data;

	printk("%s:**********current_value %d***************\n", __func__, current_value);
	
	array_size = GETARRAYNUM(CS_VTH);
	set_chr_current = bmt_find_closest_level(CS_VTH, array_size, current_value);
	register_value = charging_parameter_to_value(CS_VTH, array_size ,set_chr_current);
	bq24196_set_ichg(register_value);
	
	return status;
 } 	

 static kal_uint32 charging_set_input_current(void *data)
 {
 	kal_uint32 status = STATUS_OK;
	kal_uint32 set_chr_current;
	kal_uint32 array_size;
	kal_uint32 register_value;
    
	array_size = GETARRAYNUM(INPUT_CS_VTH);
	set_chr_current = bmt_find_closest_level(INPUT_CS_VTH, array_size, *(kal_uint32 *)data);
	register_value = charging_parameter_to_value(INPUT_CS_VTH, array_size ,set_chr_current);	
        
    bq24196_set_iinlim(register_value);

	return status;
 } 	

 static kal_uint32 charging_get_charging_status(void *data)
 {
 	kal_uint32 status = STATUS_OK;
	kal_uint32 ret_val;

	ret_val = bq24196_get_chrg_stat();
	
	if(ret_val == 0x3)
		*(kal_uint32 *)data = KAL_TRUE;
	else
		*(kal_uint32 *)data = KAL_FALSE;
	
	return status;
 } 	

 static kal_uint32 charging_reset_watch_dog_timer(void *data)
 {
	 kal_uint32 status = STATUS_OK;
 
     battery_xlog_printk(BAT_LOG_CRTI, "charging_reset_watch_dog_timer\r\n");
 
	 bq24196_set_wdt_rst(0x1); //Kick watchdog
	 
	 return status;
 }
 
  static kal_uint32 charging_set_hv_threshold(void *data)
  {
	 kal_uint32 status = STATUS_OK;
 
	 return status;
  }
 
  static kal_uint32 charging_get_hv_status(void *data)
  {
	   kal_uint32 status = STATUS_OK;
 
	   *(kal_bool*)(data) = mt6332_upmu_get_rgs_chr_hv_det();
	   
	   return status;
  }

 static kal_uint32 charging_get_battery_status(void *data)
 {
	   kal_uint32 status = STATUS_OK;
 
 	   mt6332_upmu_set_rg_baton_tdet_en(1);	
	   mt6332_upmu_set_rg_baton_en(1);
	   *(kal_bool*)(data) = mt6332_upmu_get_rg_int_status_vbaton_undet();
	   
	   return status;
 }

 static kal_uint32 charging_get_charger_det_status(void *data)
 {
	   kal_uint32 status = STATUS_OK;
 
	   *(kal_bool*)(data) = upmu_get_rgs_chrdet();
	   return status;
 }

kal_bool charging_type_detection_done(void)
{
	 return charging_type_det_done;
}

 static kal_uint32 charging_get_charger_type(void *data)
 {
	 kal_uint32 status = STATUS_OK;
	 CHARGER_TYPE charger_type = CHARGER_UNKNOWN;
#if defined(CONFIG_POWER_EXT)
	 *(CHARGER_TYPE*)(data) = STANDARD_HOST;
#else
	 charger_type = bq24196_get_charger_type();
	*(CHARGER_TYPE*)(data) = charger_type;
	printk("%s:*********charger_type %d,%d\n", __func__, charger_type, *(CHARGER_TYPE*)(data));
	charging_type_det_done = KAL_TRUE;
#endif    
	 return status;
}

static kal_uint32 charging_get_is_pcm_timer_trigger(void *data)
{
    kal_uint32 status = STATUS_OK;

    if(slp_get_wake_reason() == WR_PCM_TIMER)
        *(kal_bool*)(data) = KAL_TRUE;
    else
        *(kal_bool*)(data) = KAL_FALSE;

    battery_xlog_printk(BAT_LOG_CRTI, "slp_get_wake_reason=%d\n", slp_get_wake_reason());
       
    return status;
}

static kal_uint32 charging_set_platform_reset(void *data)
{
    kal_uint32 status = STATUS_OK;

    battery_xlog_printk(BAT_LOG_CRTI, "charging_set_platform_reset\n");
 
    arch_reset(0,NULL);
        
    return status;
}

static kal_uint32 charging_get_platfrom_boot_mode(void *data)
{
    kal_uint32 status = STATUS_OK;
  
    *(kal_uint32*)(data) = get_boot_mode();

    battery_xlog_printk(BAT_LOG_CRTI, "get_boot_mode=%d\n", get_boot_mode());
         
    return status;
}

 static kal_uint32 (* const charging_func[CHARGING_CMD_NUMBER])(void *data)=
 {
 	  charging_hw_init
	,charging_dump_register  	
	,charging_enable
	,charging_set_cv_voltage
	,charging_get_current
	,charging_set_current
	,charging_set_input_current
	,charging_get_charging_status
	,charging_reset_watch_dog_timer
	,charging_set_hv_threshold
	,charging_get_hv_status
	,charging_get_battery_status
	,charging_get_charger_det_status
	,charging_get_charger_type
	,charging_get_is_pcm_timer_trigger
	,charging_set_platform_reset
	,charging_get_platfrom_boot_mode
 };

 /*
 * FUNCTION
 *		Internal_chr_control_handler
 *
 * DESCRIPTION															 
 *		 This function is called to set the charger hw
 *
 * CALLS  
 *
 * PARAMETERS
 *		None
 *	 
 * RETURNS
 *		
 *
 * GLOBALS AFFECTED
 *	   None
 */
 kal_int32 chr_control_interface(CHARGING_CTRL_CMD cmd, void *data)
 {
	 kal_int32 status;
	 if(cmd < CHARGING_CMD_NUMBER)
		 status = charging_func[cmd](data);
	 else
		 return STATUS_UNSUPPORTED;
 
	 return status;
 }


