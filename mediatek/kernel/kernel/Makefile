ccflags-y += -I$(srctree)
obj-$(CONFIG_KGDB_KDB) += kdb_enhance.o
obj-y += mt_storage_logger.o
obj-$(CONFIG_MT65XX_TRACER) += trace_mt65xx_mon.o
obj-y += sched/
obj-$(CONFIG_MTK_MEMCFG) += mtk_memcfg.o
obj-$(CONFIG_TRACING) += mtk_trace.o
obj-y += mtk_meminfo.o
obj-y += system.o

# Hibernation
obj-$(CONFIG_MTK_HIBERNATION) += hibcore/
obj-y += mt_cache_v7.o
obj-y += fiq_smp_call.o
obj-y += sec_osal.o
