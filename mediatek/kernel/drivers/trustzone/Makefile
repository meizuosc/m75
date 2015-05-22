# TZ KREE driver makefile.

ccflags-y += -I$(srctree)/../mediatek/kernel/include/trustzone/ -Werror

# Build as module
#obj-m += tz_module.o
obj-y += tz_module.o
tz_module-y += tz_mod.o tz_system.o test.o kree_mem.o tz_sys_ipc.o \
               tz_irq.o tz_pm.o tz_clock.o tz_ndbg.o tz_counter.o tz_mem.o

