#include <linux/kernel.h>
#include <linux/module.h>

#include "core/met_drv.h"
#include "core/trace.h"


#define MS_EMI_FMT	"%5lu.%06lu"
#define MS_EMI_VAL	(unsigned long)(timestamp), nano_rem/1000
void ms_emi(unsigned long long timestamp, unsigned char cnt, unsigned int *value)
{
	unsigned long nano_rem = do_div(timestamp, 1000000000);
	switch (cnt) {
	case 18: MET_PRINTK(MS_EMI_FMT FMT18, MS_EMI_VAL VAL18); break;
	case 22: MET_PRINTK(MS_EMI_FMT FMT22, MS_EMI_VAL VAL22); break;
	case 32: MET_PRINTK(MS_EMI_FMT FMT32, MS_EMI_VAL VAL32); break;
	}
}
void ms_ttype(unsigned long long timestamp, unsigned char cnt, unsigned int *value)
{
	unsigned long nano_rem = do_div(timestamp, 1000000000);
	switch (cnt) {
	case 5: MET_PRINTK(MS_EMI_FMT FMT5, MS_EMI_VAL VAL5); break;
	}
}

#define MS_SMI_FMT	"%5lu.%06lu"
#define MS_SMI_VAL	(unsigned long)(timestamp), nano_rem/1000
void ms_smi(unsigned long long timestamp, unsigned char cnt, unsigned int *value)
{
	unsigned long nano_rem = do_div(timestamp, 1000000000);
	switch (cnt) {
	case  1: MET_PRINTK(MS_SMI_FMT FMT1, MS_SMI_VAL VAL1); break;
	case  2: MET_PRINTK(MS_SMI_FMT FMT2, MS_SMI_VAL VAL2); break;
	case  3: MET_PRINTK(MS_SMI_FMT FMT3, MS_SMI_VAL VAL3); break;
	case  4: MET_PRINTK(MS_SMI_FMT FMT4, MS_SMI_VAL VAL4); break;
	case  5: MET_PRINTK(MS_SMI_FMT FMT5, MS_SMI_VAL VAL5); break;
	case  6: MET_PRINTK(MS_SMI_FMT FMT6, MS_SMI_VAL VAL6); break;
	case  7: MET_PRINTK(MS_SMI_FMT FMT7, MS_SMI_VAL VAL7); break;
	case  8: MET_PRINTK(MS_SMI_FMT FMT8, MS_SMI_VAL VAL8); break;
	case  9: MET_PRINTK(MS_SMI_FMT FMT9, MS_SMI_VAL VAL9); break;
	case 10: MET_PRINTK(MS_SMI_FMT FMT10, MS_SMI_VAL VAL10); break;
	case 11: MET_PRINTK(MS_SMI_FMT FMT11, MS_SMI_VAL VAL11); break;
	case 12: MET_PRINTK(MS_SMI_FMT FMT12, MS_SMI_VAL VAL12); break;
	case 13: MET_PRINTK(MS_SMI_FMT FMT13, MS_SMI_VAL VAL13); break;
	case 14: MET_PRINTK(MS_SMI_FMT FMT14, MS_SMI_VAL VAL14); break;
	case 15: MET_PRINTK(MS_SMI_FMT FMT15, MS_SMI_VAL VAL15); break;
	case 16: MET_PRINTK(MS_SMI_FMT FMT16, MS_SMI_VAL VAL16); break;
	case 17: MET_PRINTK(MS_SMI_FMT FMT17, MS_SMI_VAL VAL17); break;
	case 18: MET_PRINTK(MS_SMI_FMT FMT18, MS_SMI_VAL VAL18); break;
	case 19: MET_PRINTK(MS_SMI_FMT FMT19, MS_SMI_VAL VAL19); break;
	case 20: MET_PRINTK(MS_SMI_FMT FMT20, MS_SMI_VAL VAL20); break;
	case 21: MET_PRINTK(MS_SMI_FMT FMT21, MS_SMI_VAL VAL21); break;
	case 22: MET_PRINTK(MS_SMI_FMT FMT22, MS_SMI_VAL VAL22); break;
	case 23: MET_PRINTK(MS_SMI_FMT FMT23, MS_SMI_VAL VAL23); break;
	case 24: MET_PRINTK(MS_SMI_FMT FMT24, MS_SMI_VAL VAL24); break;
	case 25: MET_PRINTK(MS_SMI_FMT FMT25, MS_SMI_VAL VAL25); break;
	case 26: MET_PRINTK(MS_SMI_FMT FMT26, MS_SMI_VAL VAL26); break;
	case 27: MET_PRINTK(MS_SMI_FMT FMT27, MS_SMI_VAL VAL27); break;
	case 28: MET_PRINTK(MS_SMI_FMT FMT28, MS_SMI_VAL VAL28); break;
	case 29: MET_PRINTK(MS_SMI_FMT FMT29, MS_SMI_VAL VAL29); break;
	case 30: MET_PRINTK(MS_SMI_FMT FMT30, MS_SMI_VAL VAL30); break;
	case 31: MET_PRINTK(MS_SMI_FMT FMT31, MS_SMI_VAL VAL31); break;
	case 32: MET_PRINTK(MS_SMI_FMT FMT32, MS_SMI_VAL VAL32); break;
	case 33: MET_PRINTK(MS_SMI_FMT FMT33, MS_SMI_VAL VAL33); break;
	case 34: MET_PRINTK(MS_SMI_FMT FMT34, MS_SMI_VAL VAL34); break;
	case 35: MET_PRINTK(MS_SMI_FMT FMT35, MS_SMI_VAL VAL35); break;
	case 36: MET_PRINTK(MS_SMI_FMT FMT36, MS_SMI_VAL VAL36); break;
	case 37: MET_PRINTK(MS_SMI_FMT FMT37, MS_SMI_VAL VAL37); break;
	case 38: MET_PRINTK(MS_SMI_FMT FMT38, MS_SMI_VAL VAL38); break;
	case 39: MET_PRINTK(MS_SMI_FMT FMT39, MS_SMI_VAL VAL39); break;
	case 40: MET_PRINTK(MS_SMI_FMT FMT40, MS_SMI_VAL VAL40); break;
	case 41: MET_PRINTK(MS_SMI_FMT FMT41, MS_SMI_VAL VAL41); break;
	case 42: MET_PRINTK(MS_SMI_FMT FMT42, MS_SMI_VAL VAL42); break;
	case 43: MET_PRINTK(MS_SMI_FMT FMT43, MS_SMI_VAL VAL43); break;
	case 44: MET_PRINTK(MS_SMI_FMT FMT44, MS_SMI_VAL VAL44); break;
	case 45: MET_PRINTK(MS_SMI_FMT FMT45, MS_SMI_VAL VAL45); break;
	case 46: MET_PRINTK(MS_SMI_FMT FMT46, MS_SMI_VAL VAL46); break;
	case 47: MET_PRINTK(MS_SMI_FMT FMT47, MS_SMI_VAL VAL47); break;
	case 48: MET_PRINTK(MS_SMI_FMT FMT48, MS_SMI_VAL VAL48); break;
	case 49: MET_PRINTK(MS_SMI_FMT FMT49, MS_SMI_VAL VAL49); break;
	case 50: MET_PRINTK(MS_SMI_FMT FMT50, MS_SMI_VAL VAL50); break;
	case 51: MET_PRINTK(MS_SMI_FMT FMT51, MS_SMI_VAL VAL51); break;
	case 52: MET_PRINTK(MS_SMI_FMT FMT52, MS_SMI_VAL VAL52); break;
	case 53: MET_PRINTK(MS_SMI_FMT FMT53, MS_SMI_VAL VAL53); break;
	case 54: MET_PRINTK(MS_SMI_FMT FMT54, MS_SMI_VAL VAL54); break;
	case 55: MET_PRINTK(MS_SMI_FMT FMT55, MS_SMI_VAL VAL55); break;
	case 56: MET_PRINTK(MS_SMI_FMT FMT56, MS_SMI_VAL VAL56); break;
	case 57: MET_PRINTK(MS_SMI_FMT FMT57, MS_SMI_VAL VAL57); break;
	case 58: MET_PRINTK(MS_SMI_FMT FMT58, MS_SMI_VAL VAL58); break;
	case 59: MET_PRINTK(MS_SMI_FMT FMT59, MS_SMI_VAL VAL59); break;
	case 60: MET_PRINTK(MS_SMI_FMT FMT60, MS_SMI_VAL VAL60); break;
	}
}

void ms_smit(unsigned long long timestamp, unsigned char cnt, unsigned int *value)
{
	unsigned long nano_rem = do_div(timestamp, 1000000000);
	switch (cnt) {
	case 10: MET_PRINTK(MS_SMI_FMT FMT10, MS_SMI_VAL VAL10); break;
	case 19: MET_PRINTK(MS_SMI_FMT FMT19, MS_SMI_VAL VAL19); break;
	case 14: MET_PRINTK(MS_SMI_FMT FMT14, MS_SMI_VAL VAL14); break;
	}
}

#define MS_TH_FMT	"%5lu.%06lu"
#define MS_TH_VAL	(unsigned long)(timestamp), nano_rem/1000
#define MS_TH_UD_FMT1	",%d\n"
#define MS_TH_UD_FMT2	",%d,%d\n"
#define MS_TH_UD_FMT3	",%d,%d,%d\n"
#define MS_TH_UD_FMT4	",%d,%d,%d,%d\n"
#define MS_TH_UD_FMT5	",%d,%d,%d,%d,%d\n"
#define MS_TH_UD_FMT6	",%d,%d,%d,%d,%d,%d\n"
#define MS_TH_UD_FMT7	",%d,%d,%d,%d,%d,%d,%d\n"
#define MS_TH_UD_FMT8	",%d,%d,%d,%d,%d,%d,%d,%d\n"
#define MS_TH_UD_FMT9	",%d,%d,%d,%d,%d,%d,%d,%d,%d\n"
#define MS_TH_UD_FMT10 ",%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n"
#define MS_TH_UD_FMT11	 ",%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n"
#define MS_TH_UD_FMT12	 ",%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n"
#define MS_TH_UD_FMT13	 ",%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n"
#define MS_TH_UD_FMT14	 ",%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n"

#define MS_TH_UD_VAL1	,value[0]
#define MS_TH_UD_VAL2	,value[0],value[1]
#define MS_TH_UD_VAL3	,value[0],value[1],value[2]
#define MS_TH_UD_VAL4	,value[0],value[1],value[2],value[3]
#define MS_TH_UD_VAL5	,value[0],value[1],value[2],value[3],value[4]
#define MS_TH_UD_VAL6	,value[0],value[1],value[2],value[3],value[4],value[5]
#define MS_TH_UD_VAL7	,value[0],value[1],value[2],value[3],value[4],value[5],value[6]
#define MS_TH_UD_VAL8	,value[0],value[1],value[2],value[3],value[4],value[5],value[6],value[7]
#define MS_TH_UD_VAL9	,value[0],value[1],value[2],value[3],value[4],value[5],value[6],value[7],value[8]
#define MS_TH_UD_VAL10 ,value[0],value[1],value[2],value[3],value[4],value[5],value[6],value[7],value[8],value[9]
#define MS_TH_UD_VAL11	,value[0],value[1],value[2],value[3],value[4],value[5],value[6],value[7],value[8],value[9],value[10]
#define MS_TH_UD_VAL12	,value[0],value[1],value[2],value[3],value[4],value[5],value[6],value[7],value[8],value[9],value[10],value[11]
#define MS_TH_UD_VAL13	,value[0],value[1],value[2],value[3],value[4],value[5],value[6],value[7],value[8],value[9],value[10],value[11],value[12]
#define MS_TH_UD_VAL14	,value[0],value[1],value[2],value[3],value[4],value[5],value[6],value[7],value[8],value[9],value[10],value[11],value[12],value[13]

void ms_th(unsigned long long timestamp, unsigned char cnt, unsigned int *value)
{
	unsigned long nano_rem = do_div(timestamp, 1000000000);
	switch (cnt) {
	case 1: MET_PRINTK(MS_TH_FMT MS_TH_UD_FMT1, MS_TH_VAL MS_TH_UD_VAL1); break;
	case 2: MET_PRINTK(MS_TH_FMT MS_TH_UD_FMT2, MS_TH_VAL MS_TH_UD_VAL2); break;
	case 3: MET_PRINTK(MS_TH_FMT MS_TH_UD_FMT3, MS_TH_VAL MS_TH_UD_VAL3); break;
	case 4: MET_PRINTK(MS_TH_FMT MS_TH_UD_FMT4, MS_TH_VAL MS_TH_UD_VAL4); break;
	case 5: MET_PRINTK(MS_TH_FMT MS_TH_UD_FMT5, MS_TH_VAL MS_TH_UD_VAL5); break;
	case 6: MET_PRINTK(MS_TH_FMT MS_TH_UD_FMT6, MS_TH_VAL MS_TH_UD_VAL6); break;
	case 7: MET_PRINTK(MS_TH_FMT MS_TH_UD_FMT7, MS_TH_VAL MS_TH_UD_VAL7); break;
	case 8: MET_PRINTK(MS_TH_FMT MS_TH_UD_FMT8, MS_TH_VAL MS_TH_UD_VAL8); break;
	case 9: MET_PRINTK(MS_TH_FMT MS_TH_UD_FMT9, MS_TH_VAL MS_TH_UD_VAL9); break;
	case 10: MET_PRINTK(MS_TH_FMT MS_TH_UD_FMT10, MS_TH_VAL MS_TH_UD_VAL10); break;
	case 11: MET_PRINTK(MS_TH_FMT MS_TH_UD_FMT11, MS_TH_VAL MS_TH_UD_VAL11); break;
	case 12: MET_PRINTK(MS_TH_FMT MS_TH_UD_FMT12, MS_TH_VAL MS_TH_UD_VAL12); break;
	case 13: MET_PRINTK(MS_TH_FMT MS_TH_UD_FMT13, MS_TH_VAL MS_TH_UD_VAL13); break;
	case 14: MET_PRINTK(MS_TH_FMT MS_TH_UD_FMT14, MS_TH_VAL MS_TH_UD_VAL14); break;
	default : printk("Warnning!MET thermal Cnt Not support: %d\n" , cnt); break;
	}
}

#define MS_DRAMC_UD_FMT	"%x,%x,%x,%x\n"
#define MS_DRAMC_UD_VAL	value[0],value[1],value[2],value[3]
void ms_dramc(unsigned long long timestamp, unsigned char cnt, unsigned int *value)
{
	switch (cnt) {
	case 4: MET_PRINTK(MS_DRAMC_UD_FMT, MS_DRAMC_UD_VAL); break;
	}
}
