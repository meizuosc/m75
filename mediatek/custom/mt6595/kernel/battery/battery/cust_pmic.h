#ifndef _CUST_PMIC_H_
#define _CUST_PMIC_H_

#define LOW_POWER_LIMIT_LEVEL_1 15

//Define for disable low battery protect feature, default no define for enable low battery protect.
//#define DISABLE_LOW_BATTERY_PROTECT

//Define for disable battery OC protect feature, default no define for enable low battery protect.
//#define DISABLE_BATTERY_OC_PROTECT

//Define for disable battery 15% protect feature, default no define for enable low battery protect.
//#define DISABLE_BATTERY_PERCENT_PROTECT


/* ADC Channel Number */
typedef enum {
	//MT6331
	AUX_TSENSE_31_AP = 	0x004,
	AUX_VACCDET_AP,
	AUX_VISMPS_1_AP,
	AUX_ADCVIN0_AP,
	AUX_HP_AP = 		0x009,
        
	//MT6332
	AUX_BATSNS_AP = 	0x010,
	AUX_ISENSE_AP,
	AUX_VBIF_AP,
	AUX_BATON_AP,
	AUX_TSENSE_32_AP,
	AUX_VCHRIN_AP,
	AUX_VISMPS_2_AP,
	AUX_VUSB_AP,
	AUX_M3_REF_AP,   
	AUX_SPK_ISENSE_AP,
	AUX_SPK_THR_V_AP,
	AUX_SPK_THR_I_AP,

	AUX_VADAPTOR_AP =	0x027,        
	AUX_TSENSE_31_MD = 	0x104,
	AUX_ADCVIN0_MD = 	0x107,
	AUX_TSENSE_32_MD =	0x114,
	AUX_ADCVIN0_GPS = 	0x208
} upmu_adc_chl_list_enum;

typedef enum {
	MT6331_CHIP = 0,
	MT6332_CHIP,
	ADC_CHIP_MAX
} upmu_adc_chip_list_enum;

#endif /* _CUST_PMIC_H_ */ 
