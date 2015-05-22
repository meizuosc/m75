#ifndef _H_DDP_LOG_
#define _H_DDP_LOG_
#include <linux/xlog.h>
#include <linux/aee.h>
#include "display_recorder.h"
#include "ddp_debug.h"
#ifndef LOG_TAG
#define LOG_TAG
#endif

#define DISP_LOG_D( fmt, args...)   pr_debug("[DDP/"LOG_TAG"]"fmt, ##args)
#define DISP_LOG_I( fmt, args...)   pr_debug("[DDP/"LOG_TAG"]"fmt, ##args)
#define DISP_LOG_W( fmt, args...)   pr_warn("[DDP/"LOG_TAG"]"fmt, ##args)
#define DISP_LOG_E( fmt, args...)   do{pr_err("[DDP/"LOG_TAG"]error:"fmt, ##args);dprec_logger_pr(DPREC_LOGGER_ERROR, fmt, ##args);}while(0)

#define DISP_LOG_V(fmt, args...)               \
	do {                                       \
        if(ddp_debug_dbg_log_level()>=2)       \
		    DISP_LOG_I(fmt,##args);            \
	}while (0) 

#define DDPIRQ(fmt, args...)                   \
	do {                                       \
        if(ddp_debug_irq_log_level())          \
		    DISP_LOG_I(fmt,##args);            \
	}while (0)

#define DDPDBG(fmt, args...)                   \
	do {                                       \
        if(ddp_debug_dbg_log_level())          \
		    DISP_LOG_I(fmt,##args);            \
	}while (0)

#define DDPDEBUG_D(fmt, args...)               \
    do {                                       \
        DISP_LOG_D(fmt,##args);                \
    }while (0)

#define DDPMSG(fmt, args...)                   \
	do {                                       \
		DISP_LOG_I(fmt,##args);                \
	}while (0)

#define DDPERR(fmt, args...)                   \
    do {                                       \
        DISP_LOG_E(fmt,##args);                \
    }while (0)

#define DDPDUMP(fmt,...)                        \
    do{                                         \
        if(ddp_debug_analysis_to_buffer())      \
        {                                       \
            char log[512]={'\0'};               \
            sprintf(log, fmt, ##__VA_ARGS__);    \
            dprec_logger_dump(log);             \
        }                                       \
        else                                    \
            pr_notice("[DDP/"LOG_TAG"]"fmt,##__VA_ARGS__);\
    }while(0)

#ifndef ASSERT
#define ASSERT(expr)                             \
    do {                                         \
        if(expr) break;                          \
        printk("DDP ASSERT FAILED %s, %d\n",     \
            __FILE__, __LINE__); BUG();          \
    }while(0)
#endif

#define DDPAEE(args...)                        \
    do {                                       \
        aee_kernel_warning_api(__FILE__,       \
        __LINE__, DB_OPT_DEFAULT,              \
        "DDP/"LOG_TAG,args);                   \
    }while (0)

#endif
