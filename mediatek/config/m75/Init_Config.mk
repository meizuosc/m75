#Android forbiden developer use product level variables(PRODUCT_XXX, TARGET_XXX, BOARD_XXX) in Android.mk
#Because AndroidBoard.mk include by build/target/board/Android.mk
#split from AndroidBoard.mk for PRODUCT level variables definition.
#use MTK_ROOT_CONFIG_OUT instead of LOCAL_PATH

TARGET_PROVIDES_INIT_RC := true

PRODUCT_COPY_FILES += $(MTK_ROOT_CONFIG_OUT)/mtk-kpd.kl:system/usr/keylayout/mtk-kpd.kl \
                      $(MTK_ROOT_CONFIG_OUT)/atmel_mxt_ts.kl:system/usr/keylayout/atmel_mxt_ts.kl \
                      $(MTK_ROOT_CONFIG_OUT)/mtk-tpd.kl:system/usr/keylayout/mtk-tpd.kl \
                      $(MTK_ROOT_CONFIG_OUT)/init.rc:root/init.rc \
                      $(MTK_ROOT_CONFIG_OUT)/init.usb.rc:root/init.usb.rc \
                      $(MTK_ROOT_CONFIG_OUT)/player.cfg:system/etc/player.cfg \
                      $(MTK_ROOT_CONFIG_OUT)/media_codecs.xml:system/etc/media_codecs.xml \
                      $(MTK_ROOT_CONFIG_OUT)/mtk_omx_core.cfg:system/etc/mtk_omx_core.cfg \
                      $(MTK_ROOT_CONFIG_OUT)/audio_policy.conf:system/etc/audio_policy.conf \
                      $(MTK_ROOT_CONFIG_OUT)/init.modem.rc:root/init.modem.rc \
                      $(MTK_ROOT_CONFIG_OUT)/meta_init.rc:root/meta_init.rc \
                      $(MTK_ROOT_CONFIG_OUT)/meta_init.modem.rc:root/meta_init.modem.rc \
                      $(MTK_ROOT_CONFIG_OUT)/factory_init.rc:root/factory_init.rc \
                      $(MTK_ROOT_CONFIG_OUT)/init.protect.rc:root/init.protect.rc \
                      $(MTK_ROOT_CONFIG_OUT)/ACCDET.kl:system/usr/keylayout/ACCDET.kl \
                      $(MTK_ROOT_CONFIG_OUT)/fstab:root/fstab \
                      $(MTK_ROOT_CONFIG_OUT)/fstab:system/etc/fstab \
                      $(MTK_ROOT_CONFIG_OUT)/fstab:root/fstab.nand \
                      $(MTK_ROOT_CONFIG_OUT)/fstab:root/fstab.fat.nand \
		      $(MTK_ROOT_CONFIG_OUT)/enableswap.sh:root/enableswap.sh \
                      $(MTK_ROOT_CONFIG_OUT)/gpio-keys.kl:system/usr/keylayout/gpio-keys.kl \
                      $(MTK_ROOT_CONFIG_OUT)/ubuntu/device-hacks.conf:system/ubuntu/etc/init/device-hacks.conf \

ifeq ($(MTK_SMARTBOOK_SUPPORT),yes)
PRODUCT_COPY_FILES += $(MTK_ROOT_CONFIG_OUT)/sbk-kpd.kl:system/usr/keylayout/sbk-kpd.kl \
                      $(MTK_ROOT_CONFIG_OUT)/sbk-kpd.kcm:system/usr/keychars/sbk-kpd.kcm
endif

ifeq ($(MTK_CLEARMOTION_SUPPORT),yes)
PRODUCT_COPY_FILES += $(MTK_ROOT_CONFIG_OUT)/mtk_clear_motion.cfg:system/etc/mtk_clear_motion.cfg
endif
ifeq ($(MTK_KERNEL_POWER_OFF_CHARGING),yes)
PRODUCT_COPY_FILES += $(MTK_ROOT_CONFIG_OUT)/init.charging.rc:root/init.charging.rc 
endif

ifeq ($(MTK_FAT_ON_NAND),yes)
PRODUCT_COPY_FILES += $(MTK_ROOT_CONFIG_OUT)/init.fon.rc:root/init.fon.rc
endif

ifeq ($(strip $(MTK_DOLBY_DAP_SUPPORT)), yes)
PRODUCT_COPY_FILES += $(MTK_ROOT_CONFIG_OUT)/dolby/ds1-default.xml:system/etc/ds1-default.xml
endif

_init_project_rc := $(MTK_ROOT_CONFIG_OUT)/init.project.rc
ifneq ($(wildcard $(_init_project_rc)),)
PRODUCT_COPY_FILES += $(_init_project_rc):root/init.project.rc
endif

_meta_init_project_rc := $(MTK_ROOT_CONFIG_OUT)/meta_init.project.rc
ifneq ($(wildcard $(_meta_init_project_rc)),)
PRODUCT_COPY_FILES += $(_meta_init_project_rc):root/meta_init.project.rc
endif

_factory_init_project_rc := $(MTK_ROOT_CONFIG_OUT)/factory_init.project.rc
ifneq ($(wildcard $(_factory_init_project_rc)),)
PRODUCT_COPY_FILES += $(_factory_init_project_rc):root/factory_init.project.rc
endif

PRODUCT_COPY_FILES += $(strip \
                        $(foreach file,$(wildcard $(MTK_ROOT_CONFIG_OUT)/*.xml), \
                          $(addprefix $(MTK_ROOT_CONFIG_OUT)/$(notdir $(file)):system/etc/permissions/,$(notdir $(file))) \
                         ) \
                       )

ifeq ($(strip $(HAVE_SRSAUDIOEFFECT_FEATURE)),yes)
  PRODUCT_COPY_FILES += $(MTK_ROOT_CONFIG_OUT)/srs_processing.cfg:system/data/srs_processing.cfg
endif

ifeq ($(MTK_SHARED_SDCARD),yes)
ifeq ($(MTK_2SDCARD_SWAP),yes)
  PRODUCT_COPY_FILES += $(MTK_ROOT_CONFIG_OUT)/init.ssd_nomuser.rc:root/init.ssd_nomuser.rc
else
  PRODUCT_COPY_FILES += $(MTK_ROOT_CONFIG_OUT)/init.ssd.rc:root/init.ssd.rc
endif
else
  PRODUCT_COPY_FILES += $(MTK_ROOT_CONFIG_OUT)/init.no_ssd.rc:root/init.no_ssd.rc
endif
#### INSTALL ht120.mtc ##########

_ht120_mtc := $(MTK_ROOT_CONFIG_OUT)/configs/ht120.mtc
ifneq ($(wildcard $(_ht120_mtc)),)
PRODUCT_COPY_FILES += $(_ht120_mtc):system/etc/.tp/.ht120.mtc
endif

##################################

##### INSTALL thermal.conf ##########

_thermal_conf := $(MTK_ROOT_CONFIG_OUT)/configs/thermal.conf
ifneq ($(wildcard $(_thermal_conf)),)
PRODUCT_COPY_FILES += $(_thermal_conf):system/etc/.tp/thermal.conf
endif

##################################

##### INSTALL thermal.off.conf ##########

_thermal_off_conf := $(MTK_ROOT_CONFIG_OUT)/configs/thermal.off.conf
ifneq ($(wildcard $(_thermal_off_conf)),)
PRODUCT_COPY_FILES += $(_thermal_off_conf):system/etc/.tp/thermal.off.conf
endif

##################################

##### INSTALL thermal.high.conf ##########

_thermal_high_conf := $(MTK_ROOT_CONFIG_OUT)/configs/thermal.high.conf
ifneq ($(wildcard $(_thermal_high_conf)),)
PRODUCT_COPY_FILES += $(_thermal_high_conf):system/etc/.tp/thermal.high.conf
endif

##################################

##### INSTALL thermal.mid.conf ##########

_thermal_mid_conf := $(MTK_ROOT_CONFIG_OUT)/configs/thermal.mid.conf
ifneq ($(wildcard $(_thermal_mid_conf)),)
PRODUCT_COPY_FILES += $(_thermal_mid_conf):system/etc/.tp/thermal.mid.conf
endif

##################################

##### INSTALL thermal.off.conf ##########

_thermal_low_conf := $(MTK_ROOT_CONFIG_OUT)/configs/thermal.low.conf
ifneq ($(wildcard $(_thermal_low_conf)),)
PRODUCT_COPY_FILES += $(_thermal_low_conf):system/etc/.tp/thermal.low.conf
endif

##################################

##### INSTALL throttle.sh ##########

_throttle_sh := $(MTK_ROOT_CONFIG_OUT)/configs/throttle.sh
ifneq ($(wildcard $(_throttle_sh)),)
PRODUCT_COPY_FILES += $(_throttle_sh):system/etc/throttle.sh
endif

########INSTALL Speaker and SmartPA parameters ########
ifeq ($(strip $(NXP_SMARTPA_SUPPORT)),yes)
PRODUCT_COPY_FILES += $(MTK_ROOT_CONFIG_OUT)/paparams/coldboot.patch:system/etc/smartpa_params/coldboot.patch
PRODUCT_COPY_FILES += $(MTK_ROOT_CONFIG_OUT)/paparams/devkit_Release.parms:system/etc/smartpa_params/devkit_Release.parms
PRODUCT_COPY_FILES += $(MTK_ROOT_CONFIG_OUT)/paparams/HQ_KS_13X18_DUMBO.eq:system/etc/smartpa_params/HQ_KS_13X18_DUMBO.eq
PRODUCT_COPY_FILES += $(MTK_ROOT_CONFIG_OUT)/paparams/HQ_KS_13X18_DUMBO.preset:system/etc/smartpa_params/HQ_KS_13X18_DUMBO.preset
PRODUCT_COPY_FILES += $(MTK_ROOT_CONFIG_OUT)/paparams/KS_13X18_DUMBO_tCoef.speaker:system/etc/smartpa_params/KS_13X18_DUMBO_tCoef.speaker
PRODUCT_COPY_FILES += $(MTK_ROOT_CONFIG_OUT)/paparams/TFA9887_N1D2_2_4_1.patch:system/etc/smartpa_params/TFA9887_N1D2_2_4_1.patch
PRODUCT_COPY_FILES += $(MTK_ROOT_CONFIG_OUT)/paparams/TFA9887_N1D2.config:system/etc/smartpa_params/TFA9887_N1D2.config
endif

ifeq ($(strip $(NXP_SMARTPA_TFA9890_SUPPORT)),yes)
PRODUCT_COPY_FILES += $(MTK_ROOT_CONFIG_OUT)/tfa9890/coldboot.patch:system/etc/tfa98xx/coldboot.patch
PRODUCT_COPY_FILES += $(MTK_ROOT_CONFIG_OUT)/tfa9890/HQ1_1128_0_0_s3_M75_0709.preset:system/etc/tfa98xx/HQ1_1128_0_0_s3_M75_0709.preset
PRODUCT_COPY_FILES += $(MTK_ROOT_CONFIG_OUT)/tfa9890/HQ1_1128_0_0_s3_M75_0709.eq:system/etc/tfa98xx/HQ1_1128_0_0_s3_M75_0709.eq
PRODUCT_COPY_FILES += $(MTK_ROOT_CONFIG_OUT)/tfa9890/HQ1_26dB_Bass_M75_1105_2_1_0_Meizu_M75_0709.preset:system/etc/tfa98xx/HQ1_26dB_Bass_M75_1105_2_1_0_Meizu_M75_0709.preset
PRODUCT_COPY_FILES += $(MTK_ROOT_CONFIG_OUT)/tfa9890/HQ1_26dB_Bass_M75_1105_2_1_0_Meizu_M75_0709.eq:system/etc/tfa98xx/HQ1_26dB_Bass_M75_1105_2_1_0_Meizu_M75_0709.eq
PRODUCT_COPY_FILES += $(MTK_ROOT_CONFIG_OUT)/tfa9890/HQ1_26dB_Bass_M75_1105_2_2_0_Meizu_M75_0709.preset:system/etc/tfa98xx/HQ1_26dB_Bass_M75_1105_2_2_0_Meizu_M75_0709.preset
PRODUCT_COPY_FILES += $(MTK_ROOT_CONFIG_OUT)/tfa9890/HQ1_26dB_Bass_M75_1105_2_2_0_Meizu_M75_0709.eq:system/etc/tfa98xx/HQ1_26dB_Bass_M75_1105_2_2_0_Meizu_M75_0709.eq
PRODUCT_COPY_FILES += $(MTK_ROOT_CONFIG_OUT)/tfa9890/Meizu_M75_0709.speaker:system/etc/tfa98xx/Meizu_M75_0709.speaker
PRODUCT_COPY_FILES += $(MTK_ROOT_CONFIG_OUT)/tfa9890/Speech_bypass_AEC_0_0_s3_M75_0711.eq:system/etc/tfa98xx/Speech_bypass_AEC_0_0_s3_M75_0711.eq
PRODUCT_COPY_FILES += $(MTK_ROOT_CONFIG_OUT)/tfa9890/Speech_bypass_AEC_0_0_s3_M75_0711.preset:system/etc/tfa98xx/Speech_bypass_AEC_0_0_s3_M75_0711.preset
PRODUCT_COPY_FILES += $(MTK_ROOT_CONFIG_OUT)/tfa9890/SpeechNB_Sloud3_0_0_s3_M75_0711.eq:system/etc/tfa98xx/SpeechNB_Sloud3_0_0_s3_M75_0711.eq
PRODUCT_COPY_FILES += $(MTK_ROOT_CONFIG_OUT)/tfa9890/SpeechNB_Sloud3_0_0_s3_M75_0711.preset:system/etc/tfa98xx/SpeechNB_Sloud3_0_0_s3_M75_0711.preset
PRODUCT_COPY_FILES += $(MTK_ROOT_CONFIG_OUT)/tfa9890/SpeechWB_Sloud3_0_0_s3_M75_0711.eq:system/etc/tfa98xx/SpeechWB_Sloud3_0_0_s3_M75_0711.eq
PRODUCT_COPY_FILES += $(MTK_ROOT_CONFIG_OUT)/tfa9890/SpeechWB_Sloud3_0_0_s3_M75_0711.preset:system/etc/tfa98xx/SpeechWB_Sloud3_0_0_s3_M75_0711.preset
PRODUCT_COPY_FILES += $(MTK_ROOT_CONFIG_OUT)/tfa9890/TFA9890_N1B12_N1C3_v3.config:system/etc/tfa98xx/TFA9890_N1B12_N1C3_v3.config
PRODUCT_COPY_FILES += $(MTK_ROOT_CONFIG_OUT)/tfa9890/TFA9890_N1C3_1_7_1.patch:system/etc/tfa98xx/TFA9890_N1C3_1_7_1.patch
PRODUCT_COPY_FILES += $(MTK_ROOT_CONFIG_OUT)/tfa9890/s1_speaker_redo.speaker:system/etc/tfa98xx/s1_speaker_redo.speaker
PRODUCT_COPY_FILES += $(MTK_ROOT_CONFIG_OUT)/tfa9890/HQ2_1128_0_0_m76_v1_1126.eq:system/etc/tfa98xx/HQ2_1128_0_0_m76_v1_1126.eq
PRODUCT_COPY_FILES += $(MTK_ROOT_CONFIG_OUT)/tfa9890/HQ2_1128_0_0_m76_v1_1126.preset:system/etc/tfa98xx/HQ2_1128_0_0_m76_v1_1126.preset
PRODUCT_COPY_FILES += $(MTK_ROOT_CONFIG_OUT)/tfa9890/s1_preset_redo_3Vstep_4_1_0_s1_redo.eq:system/etc/tfa98xx/s1_preset_redo_3Vstep_4_1_0_s1_redo.eq
PRODUCT_COPY_FILES += $(MTK_ROOT_CONFIG_OUT)/tfa9890/s1_preset_redo_3Vstep_4_1_0_s1_redo.preset:system/etc/tfa98xx/s1_preset_redo_3Vstep_4_1_0_s1_redo.preset
PRODUCT_COPY_FILES += $(MTK_ROOT_CONFIG_OUT)/tfa9890/s1_preset_redo_3Vstep_4_2_0_s1_redo.eq:system/etc/tfa98xx/s1_preset_redo_3Vstep_4_2_0_s1_redo.eq
PRODUCT_COPY_FILES += $(MTK_ROOT_CONFIG_OUT)/tfa9890/s1_preset_redo_3Vstep_4_2_0_s1_redo.preset:system/etc/tfa98xx/s1_preset_redo_3Vstep_4_2_0_s1_redo.preset
endif

# Ubuntu Overlay Files
PRODUCT_COPY_FILES += \
    $(MTK_ROOT_CONFIG_OUT)/ubuntu/fstab:ubuntu/ramdisk/fstab \
    $(MTK_ROOT_CONFIG_OUT)/ubuntu/post-update.sh:post-update.sh \
    $(MTK_ROOT_CONFIG_OUT)/ubuntu/touch:ubuntu/ramdisk/scripts/touch \
    $(MTK_ROOT_CONFIG_OUT)/ubuntu/touch-custom:ubuntu/ramdisk/scripts/touch-custom\
    $(MTK_ROOT_CONFIG_OUT)/ubuntu/udev.rules:system/ubuntu/lib/udev/rules.d/70-android.rules \
    $(MTK_ROOT_CONFIG_OUT)/ubuntu/powerd-config.xml:system/ubuntu/usr/share/powerd/device_configs/config-default.xml \
    $(MTK_ROOT_CONFIG_OUT)/ubuntu/android.conf:system/ubuntu/etc/ubuntu-touch-session.d/android.conf \
    $(MTK_ROOT_CONFIG_OUT)/ubuntu/ubuntu-location-service.conf:system/ubuntu/etc/init/ubuntu-location-service.conf \
    $(MTK_ROOT_CONFIG_OUT)/ubuntu/gps.conf:system/ubuntu/etc/gps.conf
