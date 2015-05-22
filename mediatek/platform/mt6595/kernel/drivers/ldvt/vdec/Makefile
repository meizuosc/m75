# Makefile for the linux kernel.


#obj-m := vdec.o
#obj-m   += hal/
#obj-m   += verify/
#obj-$(CONFIG_MTK_LDVT_VDEC)   += hal/
#obj-$(CONFIG_MTK_LDVT_VDEC)   += verify/


vdec_verify-objs :=        \
    vdec.o      \
    hal/vdec_hal_if_common.o    \
    hal/vdec_hw_common.o    \
    hal/vdec_hal_if_avs.o    \
    hal/vdec_hal_if_h264.o    \
    hal/vdec_hal_if_h265.o    \
    hal/vdec_hal_if_mpeg.o    \
    hal/vdec_hal_if_rm.o    \
    hal/vdec_hal_if_vp6.o    \
    hal/vdec_hal_if_vp8.o    \
    hal/vdec_hal_if_wmv.o    \
    hal/vdec_hw_avs.o    \
    hal/vdec_hw_h264.o    \
    hal/vdec_hw_h265.o    \
    hal/vdec_hw_mpeg.o    \
    hal/vdec_hw_rm.o    \
    hal/vdec_hw_vp6.o    \
    hal/vdec_hw_vp8.o    \
    verify/vdec_verify_main.o   \
    verify/vdec_verify_common.o   \
    verify/vdec_verify_vdecode.o   \
    verify/vdec_verify_filesetting.o   \
    verify/vdec_verify_mm_map.o   \
    verify/vdec_verify_irq_fiq_proc.o   \
    verify/vdec_verify_mpv_data.o   \
    verify/vdec_verify_file_common.o   \
    verify/vdec_verify_imgresz_rm.o   \
    verify/vdec_verify_vparser_avs.o   \
    verify/vdec_verify_vparser_h264.o   \
    verify/vdec_verify_vparser_h265.o   \
    verify/vdec_verify_vparser_mpeg4.o   \
    verify/vdec_verify_vparser_mpeg.o   \
    verify/vdec_verify_vparser_rm.o   \
    verify/vdec_verify_vparser_vp6.o   \
    verify/vdec_verify_vparser_vp8.o   \
    verify/vdec_verify_vparser_wmv.o   \
    verify/sata_fs_io.o       

obj-m += vdec_verify.o


