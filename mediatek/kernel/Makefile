obj-y := drivers/ kernel/

# Libs for MTK internal tests
CHECK_MTK_LIBS := $(shell ls -d $(srctree)/mediatek/kernel/lib/)
$(info Hello "$(CHECK_MTK_LIBS)")
ifneq ($(CHECK_MTK_LIBS),)
obj-y += lib/
endif
