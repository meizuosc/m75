LINUXINCLUDE += -include $(TO_ROOT)/kernel/kernel/sched/sched.h

obj-$(CONFIG_MT_PRIO_TRACER) += prio_tracer.o

#
# RQ stats for TLP estimation
#
# For user space operation
obj-$(CONFIG_MTK_SCHED_RQAVG_US) += rq_stats.o
# For kernel space operation
obj-$(CONFIG_MTK_SCHED_RQAVG_KS) += sched_avg.o
# For query cpu topology
obj-$(CONFIG_MTK_CPU_TOPOLOGY) += cputopo.o
