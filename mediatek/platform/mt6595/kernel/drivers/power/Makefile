obj-$(CONFIG_MTK_PMIC)          += pmic.o upmu_common.o pmic_auxadc.o pmic_chr_type_det.o da9210.o tps6128x.o

ifeq ($(MTK_PMIC_DVT_SUPPORT),yes)
    obj-$(CONFIG_MTK_PMIC)      += pmic_dvt.o
endif

ifeq ($(X2_BQ27531_SUPPORT),yes)
    obj-$(CONFIG_MTK_SMART_BATTERY) += bq27531.o bq27531_firmware.o
endif
obj-$(CONFIG_MTK_SMART_BATTERY) += battery_meter_hal.o 
ifeq ($(MTK_BQ24250_SUPPORT),yes)
     obj-$(CONFIG_MTK_SMART_BATTERY) += bq24250.o charging_hw_bq24250.o
else
     ifeq ($(MTK_BQ24196_SUPPORT),yes)
          obj-$(CONFIG_MTK_SMART_BATTERY) += bq24196.o charging_hw_bq24196.o
     else
          ifeq ($(MTK_BQ24160_SUPPORT),yes)
               obj-$(CONFIG_MTK_SMART_BATTERY) += bq24160.o charging_hw_bq24160.o
          else
               ifeq ($(MTK_SWCHR_SUPPORT),yes)
                    obj-$(CONFIG_MTK_SMART_BATTERY) += charging_hw_mt_sw_charger.o                
               else
                    obj-$(CONFIG_MTK_PMIC_CHARGER) += charging_hw_pmic.o 
               endif
	  endif
     endif
endif
