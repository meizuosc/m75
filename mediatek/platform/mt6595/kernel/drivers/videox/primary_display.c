#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/rtpm_prio.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_irq.h>

#include "debug.h"

#include "disp_drv_log.h"

#include "disp_lcm.h"
#include "disp_utils.h"
#include "mtkfb.h"

#include "ddp_hal.h"
#include "ddp_dump.h"
#include "ddp_path.h"
#include "ddp_drv.h"

#include "disp_session.h"

#include <mach/m4u.h>
#include <mach/m4u_port.h>
#include "primary_display.h"
#include "cmdq_def.h"
#include "cmdq_record.h"
#include "cmdq_reg.h"
#include "cmdq_core.h"

#include "ddp_manager.h"
#include "mtkfb_fence.h"
#include "disp_drv_platform.h"
#include "display_recorder.h"
#include "fbconfig_kdebug_rome.h"
#include "ddp_mmp.h"
// for sodi rdma callback
#include "ddp_irq.h"
#include <mach/mt_spm_idle.h>
#include "mach/eint.h"
#include <cust_eint.h>
#include <mach/mt_gpio.h>
#include <mach/mt_boot_common.h>
#include <cust_gpio_usage.h>
#include "disp_session.h"
extern 	int is_DAL_Enabled(void);
extern int dprec_mmp_dump_ovl_layer(OVL_CONFIG_STRUCT *ovl_layer,unsigned int l,unsigned int session/*1:primary, 2:external, 3:memory*/);
extern bool is_ipoh_bootup;
extern unsigned int isAEEEnabled;
int primary_display_use_cmdq = CMDQ_DISABLE;
int primary_display_use_m4u = 1;
DISP_PRIMARY_PATH_MODE primary_display_mode = DIRECT_LINK_MODE;

static unsigned long dim_layer_mva = 0;
//DDP_SCENARIO_ENUM ddp_scenario = DDP_SCENARIO_SUB_RDMA1_DISP;
#ifdef DISP_SWITCH_DST_MODE
int primary_display_def_dst_mode = 0;
int primary_display_cur_dst_mode = 0;
#endif
#ifdef CONFIG_OF
extern unsigned int islcmconnected;
#endif
extern unsigned int _need_wait_esd_eof(void);
extern unsigned int _need_register_eint(void);
extern unsigned int _need_do_esd_check(void);
int primary_trigger_cnt = 0;
#define PRIMARY_DISPLAY_TRIGGER_CNT (1)
typedef struct
{
	DISP_POWER_STATE			state;
	unsigned int				lcm_fps;
	int					max_layer;
	int					need_trigger_overlay;
	int					need_trigger_ovl1to2;
	DISP_PRIMARY_PATH_MODE 	                mode;
	int                                     ovl1to2_mode;
	unsigned int				last_vsync_tick;
    unsigned long               framebuffer_mva;
    unsigned long               framebuffer_va;
	struct mutex 				lock;
	struct mutex 				capture_lock;
#ifdef DISP_SWITCH_DST_MODE
	struct mutex 				switch_dst_lock;
#endif
	disp_lcm_handle *			plcm;
	cmdqRecHandle 				cmdq_handle_config_esd;
	cmdqRecHandle 				cmdq_handle_config;
	cmdqRecHandle 				cmdq_handle_ovl1to2_config;
	cmdqRecHandle 				cmdq_handle_trigger;
	disp_path_handle 			dpmgr_handle;
	disp_path_handle 			ovl2mem_path_handle;
	char *			                mutex_locker;
	int				        vsync_drop;
}display_primary_path_context;

#define pgc	_get_context()

static display_primary_path_context* _get_context(void)
{
	static int is_context_inited = 0;
	static display_primary_path_context g_context;
	if(!is_context_inited)
	{
		memset((void*)&g_context, 0, sizeof(display_primary_path_context));
		is_context_inited = 1;
	}

	return &g_context;
}

static void _primary_path_lock(const char* caller)
{
	dprec_logger_start(DPREC_LOGGER_PRIMARY_MUTEX, 0, 0);
	disp_sw_mutex_lock(&(pgc->lock));
	pgc->mutex_locker = caller;
}

static void _primary_path_unlock(const char* caller)
{
	pgc->mutex_locker = NULL;
	disp_sw_mutex_unlock(&(pgc->lock));
	dprec_logger_done(DPREC_LOGGER_PRIMARY_MUTEX, 0, 0);
}

unsigned long long last_primary_trigger_time = 0xffffffffffffffff;
static struct task_struct *primary_display_idle_detect_task = NULL;

int primary_display_save_power_for_idle(int enter)
{
	unsigned int vfp_for_low_power = 0;
	unsigned int vfp_original = 0;
	static int vfp_state = 0;

	if (vfp_state == enter)
		return 0;

	vfp_state = enter;

	if(pgc->state == DISP_SLEEPED)
		return 0;
	
	if(pgc->plcm == NULL)
	{
		DISPERR("lcm handle is null\n");
		return 0;
	}
	
	if(pgc->plcm->params)
	{
		vfp_for_low_power = pgc->plcm->params->dsi.vertical_frontporch_for_low_power;
		vfp_original = pgc->plcm->params->dsi.vertical_frontporch;
	}
	
	if(enter)
	{
		primary_display_cmdq_set_reg(0xF401B028, vfp_for_low_power);
	}
	else
	{
		primary_display_cmdq_set_reg(0xF401B028, vfp_original);
	}

	return 0;
}

static int _disp_primary_path_idle_detect_thread(void *data)
{
	int ret = 0;
	//msleep(1000*30);
	while(1)
	{   
		msleep_interruptible(2000);
		
		if(((sched_clock() - last_primary_trigger_time)/1000) > 1000000) //2s not trigger disp
		{
			//primary_display_switch_dst_mode(0);//switch to cmd mode
			primary_display_save_power_for_idle(1);
		}
		
		if (kthread_should_stop())
			break;
	}
	return 0;
}


#ifdef DISP_SWITCH_DST_MODE

bool is_switched_dst_mode = false;

static struct task_struct *primary_display_switch_dst_mode_task = NULL;
static void _primary_path_switch_dst_lock(void)
{
	mutex_lock(&(pgc->switch_dst_lock));
}
static void _primary_path_switch_dst_unlock(void)
{
	mutex_unlock(&(pgc->switch_dst_lock));
}

static int _disp_primary_path_switch_dst_mode_thread(void *data)
{
	int ret = 0;
	while(1)
	{   
		msleep(1000);
		
		if(((sched_clock() - last_primary_trigger_time)/1000) > 500000)//500ms not trigger disp
		{
			primary_display_switch_dst_mode(0);//switch to cmd mode
			is_switched_dst_mode = true;
		}
		if (kthread_should_stop())
			break;
	}
	return 0;
}
#endif
extern int disp_od_is_enabled(void);
int primary_display_get_debug_state(char* stringbuf, int buf_len)
{	
	int len = 0;
	LCM_PARAMS *lcm_param = disp_lcm_get_params(pgc->plcm);
	LCM_DRIVER *lcm_drv = pgc->plcm->drv;
	len += scnprintf(stringbuf+len, buf_len - len, "|--------------------------------------------------------------------------------------|\n");	
	len += scnprintf(stringbuf+len, buf_len - len, "|********Primary Display Path General Information********\n");
	len += scnprintf(stringbuf+len, buf_len - len, "|Primary Display is %s\n", dpmgr_path_is_idle(pgc->dpmgr_handle)?"idle":"busy");
	
	if(mutex_trylock(&(pgc->lock)))
	{
		mutex_unlock(&(pgc->lock));
		len += scnprintf(stringbuf+len, buf_len - len, "|primary path global mutex is free\n");
	}
	else
	{
		len += scnprintf(stringbuf+len, buf_len - len, "|primary path global mutex is hold by [%s]\n", pgc->mutex_locker);
	}
	
	if(lcm_param && lcm_drv)
		len += scnprintf(stringbuf+len, buf_len - len, "|LCM Driver=[%s]\tResolution=%dx%d,Interface:%s\n", lcm_drv->name, lcm_param->width, lcm_param->height, (lcm_param->type==LCM_TYPE_DSI)?"DSI":"Other");

	len += scnprintf(stringbuf+len, buf_len - len, "|OD is %s\n", disp_od_is_enabled()?"enabled":"disabled");	

	len += scnprintf(stringbuf+len, buf_len - len, "|State=%s\tlcm_fps=%d\tmax_layer=%d\tmode:%d\tvsync_drop=%d\n", pgc->state==DISP_ALIVE?"Alive":"Sleep", pgc->lcm_fps, pgc->max_layer, pgc->mode, pgc->vsync_drop);
	len += scnprintf(stringbuf+len, buf_len - len, "|cmdq_handle_config=0x%08x\tcmdq_handle_trigger=0x%08x\tdpmgr_handle=0x%08x\tovl2mem_path_handle=0x%08x\n", pgc->cmdq_handle_config, pgc->cmdq_handle_trigger, pgc->dpmgr_handle, pgc->ovl2mem_path_handle);
	len += scnprintf(stringbuf+len, buf_len - len, "|Current display driver status=%s + %s\n", primary_display_is_video_mode()?"video mode":"cmd mode", primary_display_cmdq_enabled()?"CMDQ Enabled":"CMDQ Disabled");

	return len;
}

static DISP_MODULE_ENUM _get_dst_module_by_lcm(disp_lcm_handle *plcm)
{
	if(plcm == NULL)
	{
		DISPERR("plcm is null\n");
		return DISP_MODULE_UNKNOWN;
	}
	
	if(plcm->params->type == LCM_TYPE_DSI)
	{
		if(plcm->lcm_if_id == LCM_INTERFACE_DSI0)
		{
			return DISP_MODULE_DSI0;
		}
		else if(plcm->lcm_if_id == LCM_INTERFACE_DSI1)
		{
			return DISP_MODULE_DSI1;
		}
		else if(plcm->lcm_if_id == LCM_INTERFACE_DSI_DUAL)
		{
			return DISP_MODULE_DSIDUAL;
		}
		else
		{
			return DISP_MODULE_DSI0;
		}
	}
	else if(plcm->params->type == LCM_TYPE_DPI)
	{
		return DISP_MODULE_DPI;
	}
	else
	{
		DISPERR("can't find primary path dst module\n");
		return DISP_MODULE_UNKNOWN;
	}
}

#define AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA

/***************************************************************
***trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU  
*** 1.wait idle:           N         N       Y        Y                 
*** 2.lcm update:          N         Y       N        Y
*** 3.path start:      	idle->Y      Y    idle->Y     Y                  
*** 4.path trigger:     idle->Y      Y    idle->Y     Y  
*** 5.mutex enable:        N         N    idle->Y     Y        
*** 6.set cmdq dirty:      N         Y       N        N        
*** 7.flush cmdq:          Y         Y       N        N        
****************************************************************/

int _should_wait_path_idle(void)
{
	/***trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU	
	*** 1.wait idle:	          N         N        Y        Y 	 			*/
	if(primary_display_cmdq_enabled())
	{
		if(primary_display_is_video_mode())
		{
			return 0;
		}
		else
		{
			return 0;
		}
	}
	else
	{
		if(primary_display_is_video_mode())
		{
			return dpmgr_path_is_busy(pgc->dpmgr_handle);
		}
		else
		{
			return dpmgr_path_is_busy(pgc->dpmgr_handle);
		}
	}
}

int _should_update_lcm(void)
{
/***trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU  
*** 2.lcm update:          N         Y       N        Y        **/
	if(primary_display_cmdq_enabled())
	{
		if(primary_display_is_video_mode())
		{
			return 0;
		}
		else
		{
			// TODO: lcm_update can't use cmdq now
			return 0;
		}
	}
	else
	{
		if(primary_display_is_video_mode())
		{
			return 0;
		}
		else
		{
			return 1;
		}
	}
}

int _should_start_path(void)
{
/***trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU  
*** 3.path start:      	idle->Y      Y    idle->Y     Y        ***/
	if(primary_display_cmdq_enabled())
	{
		if(primary_display_is_video_mode())
		{
			return 0;
			//return dpmgr_path_is_idle(pgc->dpmgr_handle);
		}
		else
		{
			return 0;
		}
	}
	else
	{
		if(primary_display_is_video_mode())
		{
			return dpmgr_path_is_idle(pgc->dpmgr_handle);
		}
		else
		{
			return 1;
		}
	}
}

int _should_trigger_path(void)
{
/***trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU              
*** 4.path trigger:     idle->Y      Y    idle->Y     Y     
*** 5.mutex enable:        N         N    idle->Y     Y        ***/

	// this is not a perfect design, we can't decide path trigger(ovl/rdma/dsi..) seperately with mutex enable
	// but it's lucky because path trigger and mutex enable is the same w/o cmdq, and it's correct w/ CMDQ(Y+N).
	if(primary_display_cmdq_enabled())
	{
		if(primary_display_is_video_mode())
		{
			return 0;
			//return dpmgr_path_is_idle(pgc->dpmgr_handle);
		}
		else
		{
			return 0;
		}
	}
	else
	{
		if(primary_display_is_video_mode())
		{
			return dpmgr_path_is_idle(pgc->dpmgr_handle);
		}
		else
		{
			return 1;
		}
	}
}

int _should_set_cmdq_dirty(void)
{
/***trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU  
*** 6.set cmdq dirty:	    N         Y       N        N     ***/
	if(primary_display_cmdq_enabled())
	{
		if(primary_display_is_video_mode())
		{
			return 0;
		}
		else
		{
			return 1;
		}
	}
	else
	{
		if(primary_display_is_video_mode())
		{
			return 0;
		}
		else
		{
			return 0;
		}
	}
}

int _should_flush_cmdq_config_handle(void)
{
/***trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU        
*** 7.flush cmdq:          Y         Y       N        N        ***/
	if(primary_display_cmdq_enabled())
	{
		if(primary_display_is_video_mode())
		{
			return 1;
		}
		else
		{
			return 1;
		}
	}
	else
	{
		if(primary_display_is_video_mode())
		{
			return 0;
		}
		else
		{
			return 0;
		}
	}
}

int _should_reset_cmdq_config_handle(void)
{
	if(primary_display_cmdq_enabled())
	{
		if(primary_display_is_video_mode())
		{
			return 1;
		}
		else
		{
			return 1;
		}
	}
	else
	{
		if(primary_display_is_video_mode())
		{
			return 0;
		}
		else
		{
			return 0;
		}
	}
}

int _should_insert_wait_frame_done_token(void)
{
/***trigger operation:  VDO+CMDQ  CMD+CMDQ VDO+CPU  CMD+CPU    
*** 7.flush cmdq:          Y         Y       N        N      */  
	if(primary_display_cmdq_enabled())
	{
		if(primary_display_is_video_mode())
		{
			return 1;
		}
		else
		{
			return 1;
		}
	}
	else
	{
		if(primary_display_is_video_mode())
		{
			return 0;
		}
		else
		{
			return 0;
		}
	}
}

int _should_trigger_interface(void)
{
	if(pgc->mode == DECOUPLE_MODE)
	{
		return 0;
	}
	else
	{
		return 1;
	}
}

int _should_config_ovl_input(void)
{
	// should extend this when display path dynamic switch is ready
	if(pgc->mode == SINGLE_LAYER_MODE ||pgc->mode == DEBUG_RDMA1_DSI0_MODE)
		return 0;
	else
		return 1;

}

#define OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO
static long int get_current_time_us(void)
{
    struct timeval t;
    do_gettimeofday(&t);
    return (t.tv_sec & 0xFFF) * 1000000 + t.tv_usec;
}

static enum hrtimer_restart _DISP_CmdModeTimer_handler(struct hrtimer *timer)
{
	DISPMSG("fake timer, wake up\n");
	dpmgr_signal_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);		
#if 0
	if((get_current_time_us() - pgc->last_vsync_tick) > 16666)
	{
		dpmgr_signal_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);		
		pgc->last_vsync_tick = get_current_time_us();
	}
#endif
		return HRTIMER_RESTART;
}

int _init_vsync_fake_monitor(int fps)
{
	static struct hrtimer cmd_mode_update_timer;
	static ktime_t cmd_mode_update_timer_period;

	if(fps == 0) 
		fps = 60;
	
       cmd_mode_update_timer_period = ktime_set(0 , 1000/fps*1000);
        DISPMSG("[MTKFB] vsync timer_period=%d \n", 1000/fps);
        hrtimer_init(&cmd_mode_update_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
        cmd_mode_update_timer.function = _DISP_CmdModeTimer_handler;

	return 0;
}

static int _build_path_direct_link(void)
{
	int ret = 0;

	DISP_MODULE_ENUM dst_module = 0;
	DISPFUNC(); 
	pgc->mode = DIRECT_LINK_MODE;
	
	pgc->dpmgr_handle = dpmgr_create_path(DDP_SCENARIO_PRIMARY_DISP, pgc->cmdq_handle_config);
	if(pgc->dpmgr_handle)
	{
		DISPCHECK("dpmgr create path SUCCESS(0x%08x)\n", pgc->dpmgr_handle);
	}
	else
	{
		DISPCHECK("dpmgr create path FAIL\n");
		return -1;
	}
	
	dst_module = _get_dst_module_by_lcm(pgc->plcm);
	dpmgr_path_set_dst_module(pgc->dpmgr_handle, dst_module);
	DISPCHECK("dpmgr set dst module FINISHED(%s)\n", ddp_get_module_name(dst_module));
	{
		M4U_PORT_STRUCT sPort;
		sPort.ePortID = M4U_PORT_DISP_OVL0;
		sPort.Virtuality = primary_display_use_m4u;					   
		sPort.Security = 0;
		sPort.Distance = 1;
		sPort.Direction = 0;
		ret = m4u_config_port(&sPort);
		if(ret == 0)
		{
			DISPCHECK("config M4U Port %s to %s SUCCESS\n",ddp_get_module_name(DISP_MODULE_OVL0), primary_display_use_m4u?"virtual":"physical");
		}
		else
		{
			DISPCHECK("config M4U Port %s to %s FAIL(ret=%d)\n",ddp_get_module_name(DISP_MODULE_OVL0), primary_display_use_m4u?"virtual":"physical", ret);
			return -1;
		}
	}
	
	
	dpmgr_set_lcm_utils(pgc->dpmgr_handle, pgc->plcm->drv);

	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE);
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_START);

	return ret;
}


static int _build_path_decouple(void)
{}

static int _build_path_single_layer(void)
{}

static int _build_path_debug_rdma1_dsi0(void)
{
	int ret = 0;

	DISP_MODULE_ENUM dst_module = 0;
	
	pgc->mode = DEBUG_RDMA1_DSI0_MODE;
	
	pgc->dpmgr_handle = dpmgr_create_path(DDP_SCENARIO_SUB_RDMA1_DISP, pgc->cmdq_handle_config);
	if(pgc->dpmgr_handle)
	{
		DISPCHECK("dpmgr create path SUCCESS(0x%08x)\n", pgc->dpmgr_handle);
	}
	else
	{
		DISPCHECK("dpmgr create path FAIL\n");
		return -1;
	}
	
	dst_module = _get_dst_module_by_lcm(pgc->plcm);
	dpmgr_path_set_dst_module(pgc->dpmgr_handle, dst_module);
	DISPCHECK("dpmgr set dst module FINISHED(%s)\n", ddp_get_module_name(dst_module));
	
	{
		M4U_PORT_STRUCT sPort;
		sPort.ePortID = M4U_PORT_DISP_RDMA1;
		sPort.Virtuality = primary_display_use_m4u;					   
		sPort.Security = 0;
		sPort.Distance = 1;
		sPort.Direction = 0;
		ret = m4u_config_port(&sPort);
		if(ret == 0)
		{
			DISPCHECK("config M4U Port %s to %s SUCCESS\n",ddp_get_module_name(DISP_MODULE_RDMA1), primary_display_use_m4u?"virtual":"physical");
		}
		else
		{
			DISPCHECK("config M4U Port %s to %s FAIL(ret=%d)\n",ddp_get_module_name(DISP_MODULE_RDMA1), primary_display_use_m4u?"virtual":"physical", ret);
			return -1;
		}
	}
	
	dpmgr_set_lcm_utils(pgc->dpmgr_handle, pgc->plcm->drv);

	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE);

	return ret;
}

static void _cmdq_build_trigger_loop(void)
{
	int ret = 0;
	if(pgc->cmdq_handle_trigger == NULL)
	{
		cmdqRecCreate(CMDQ_SCENARIO_TRIGGER_LOOP, &(pgc->cmdq_handle_trigger));
		DISPMSG("primary path trigger thread cmd handle=0x%08x\n", pgc->cmdq_handle_trigger);
	}
	cmdqRecReset(pgc->cmdq_handle_trigger);  

	if(primary_display_is_video_mode())
	{
		// wait and clear stream_done, HW will assert mutex enable automatically in frame done reset.
		// todo: should let dpmanager to decide wait which mutex's eof.
		ret = cmdqRecWait(pgc->cmdq_handle_trigger, CMDQ_EVENT_MUTEX0_STREAM_EOF);

		// for some module(like COLOR) to read hw register to GPR after frame done
		dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_trigger,CMDQ_AFTER_STREAM_EOF);
	}
	else
	{
		ret = cmdqRecWaitNoClear(pgc->cmdq_handle_trigger, CMDQ_SYNC_TOKEN_CABC_EOF);
		// DSI command mode doesn't have mutex_stream_eof, need use CMDQ token instead
		ret = cmdqRecWait(pgc->cmdq_handle_trigger, CMDQ_SYNC_TOKEN_CONFIG_DIRTY);

		if(_need_wait_esd_eof())
		{
			// Wait esd config thread done.
			ret = cmdqRecWaitNoClear(pgc->cmdq_handle_trigger, CMDQ_SYNC_TOKEN_ESD_EOF);
		}
		//ret = cmdqRecWait(pgc->cmdq_handle_trigger, CMDQ_EVENT_MDP_DSI0_TE_SOF);
		// for operations before frame transfer, such as waiting for DSI TE
		if(islcmconnected)
		{
			dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_trigger,CMDQ_BEFORE_STREAM_SOF);
		}
		
		// cleat frame done token, now the config thread will not allowed to config registers.
		// remember that config thread's priority is higher than trigger thread, so all the config queued before will be applied then STREAM_EOF token be cleared
		// this is what CMDQ did as "Merge"
		ret = cmdqRecClearEventToken(pgc->cmdq_handle_trigger, CMDQ_SYNC_TOKEN_STREAM_EOF);
		
		ret = cmdqRecClearEventToken(pgc->cmdq_handle_trigger, CMDQ_SYNC_TOKEN_CONFIG_DIRTY);

		// enable mutex, only cmd mode need this
		// this is what CMDQ did as "Trigger"
		dpmgr_path_trigger(pgc->dpmgr_handle, pgc->cmdq_handle_trigger, CMDQ_ENABLE);
		//ret = cmdqRecWrite(pgc->cmdq_handle_trigger, (unsigned int)(DISP_REG_CONFIG_MUTEX_EN(0))&0x1fffffff, 1, ~0);

		// waiting for frame done, because we can't use mutex stream eof here, so need to let dpmanager help to decide which event to wait
		// most time we wait rdmax frame done event.
		ret = cmdqRecWait(pgc->cmdq_handle_trigger, CMDQ_EVENT_DISP_RDMA0_EOF);  
		dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_trigger,CMDQ_WAIT_STREAM_EOF_EVENT);

		// dsi is not idle rightly after rdma frame done, so we need to polling about 1us for dsi returns to idle
		// do not polling dsi idle directly which will decrease CMDQ performance
		dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_trigger,CMDQ_CHECK_IDLE_AFTER_STREAM_EOF);
		
		// for some module(like COLOR) to read hw register to GPR after frame done
		dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_trigger,CMDQ_AFTER_STREAM_EOF);

		// polling DSI idle
		//ret = cmdqRecPoll(pgc->cmdq_handle_trigger, 0x1401b00c, 0, 0x80000000);
		// polling wdma frame done
		//ret = cmdqRecPoll(pgc->cmdq_handle_trigger, 0x140060A0, 1, 0x1);

		// now frame done, config thread is allowed to config register now
		ret = cmdqRecSetEventToken(pgc->cmdq_handle_trigger, CMDQ_SYNC_TOKEN_STREAM_EOF);

		// RUN forever!!!!
		BUG_ON(ret < 0);
	}

	// dump trigger loop instructions to check whether dpmgr_path_build_cmdq works correctly
	DISPCHECK("primary display BUILD cmdq trigger loop finished\n");

	return;
}

static void _cmdq_start_trigger_loop(void)
{
	int ret = 0;
	
	cmdqRecDumpCommand(pgc->cmdq_handle_trigger);
	// this should be called only once because trigger loop will nevet stop
	ret = cmdqRecStartLoop(pgc->cmdq_handle_trigger);
	if(!primary_display_is_video_mode())
	{
		if(_need_wait_esd_eof())
	        {
		        // Need set esd check eof synctoken to let trigger loop go.
		        cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_ESD_EOF);
	        }
		// need to set STREAM_EOF for the first time, otherwise we will stuck in dead loop
		cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_STREAM_EOF);
		cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_CABC_EOF);
		dprec_event_op(DPREC_EVENT_CMDQ_SET_EVENT_ALLOW);
	}
	else
	{
		#if 0
		if(dpmgr_path_is_idle(pgc->dpmgr_handle))
		{
			cmdqCoreSetEvent(CMDQ_EVENT_MUTEX0_STREAM_EOF);
		}
		#endif
	}
	
	DISPCHECK("primary display START cmdq trigger loop finished\n");
}

static void _cmdq_stop_trigger_loop(void)
{
	int ret = 0;
	
	// this should be called only once because trigger loop will nevet stop
	ret = cmdqRecStopLoop(pgc->cmdq_handle_trigger);
	
	DISPCHECK("primary display STOP cmdq trigger loop finished\n");
}


static void _cmdq_set_config_handle_dirty(void)
{
	if(!primary_display_is_video_mode())
	{
		dprec_logger_start(DPREC_LOGGER_PRIMARY_CMDQ_SET_DIRTY, 0, 0);
		// only command mode need to set dirty
		cmdqRecSetEventToken(pgc->cmdq_handle_config, CMDQ_SYNC_TOKEN_CONFIG_DIRTY);
		dprec_event_op(DPREC_EVENT_CMDQ_SET_DIRTY);
		dprec_logger_done(DPREC_LOGGER_PRIMARY_CMDQ_SET_DIRTY, 0, 0);
	}
}

static void _cmdq_set_config_handle_dirty_mira(void *handle)
{
	if(!primary_display_is_video_mode())
	{
		dprec_logger_start(DPREC_LOGGER_PRIMARY_CMDQ_SET_DIRTY, 0, 0);
		// only command mode need to set dirty
		cmdqRecSetEventToken(handle, CMDQ_SYNC_TOKEN_CONFIG_DIRTY);
		dprec_event_op(DPREC_EVENT_CMDQ_SET_DIRTY);
		dprec_logger_done(DPREC_LOGGER_PRIMARY_CMDQ_SET_DIRTY, 0, 0);
	}
}

static void _cmdq_reset_config_handle(void)
{
	cmdqRecReset(pgc->cmdq_handle_config);
	dprec_event_op(DPREC_EVENT_CMDQ_RESET);
}

static void _cmdq_flush_config_handle(int blocking, void *callback, unsigned int userdata)
{
	dprec_logger_start(DPREC_LOGGER_PRIMARY_CMDQ_FLUSH, blocking, callback);
	if(blocking)
	{
		//DISPERR("Should not use blocking cmdq flush, may block primary display path for 1 frame period\n");
		cmdqRecFlush(pgc->cmdq_handle_config);
	}
	else
	{
		if(callback)
			cmdqRecFlushAsyncCallback(pgc->cmdq_handle_config, callback, userdata);
		else
			cmdqRecFlushAsync(pgc->cmdq_handle_config);
	}
	
	dprec_event_op(DPREC_EVENT_CMDQ_FLUSH);
	dprec_logger_done(DPREC_LOGGER_PRIMARY_CMDQ_FLUSH, userdata, 0);	
}

static void _cmdq_flush_config_handle_mira(void* handle, int blocking)
{
	dprec_logger_start(DPREC_LOGGER_PRIMARY_CMDQ_FLUSH, 0, 0);
	if(blocking)
	{
		cmdqRecFlush(handle);
	}
	else
	{
		cmdqRecFlushAsync(handle);
	}
	dprec_event_op(DPREC_EVENT_CMDQ_FLUSH);
	dprec_logger_done(DPREC_LOGGER_PRIMARY_CMDQ_FLUSH, 0, 0);	
}

static void _cmdq_insert_wait_frame_done_token(void)
{
	if(primary_display_is_video_mode())
	{
		cmdqRecWaitNoClear(pgc->cmdq_handle_config, CMDQ_EVENT_MUTEX0_STREAM_EOF);
	}
	else
	{
		cmdqRecWaitNoClear(pgc->cmdq_handle_config, CMDQ_SYNC_TOKEN_STREAM_EOF);
	}
	
	dprec_event_op(DPREC_EVENT_CMDQ_WAIT_STREAM_EOF);
}

static void _cmdq_insert_wait_frame_done_token_mira(void* handle)
{
	if(primary_display_is_video_mode())
	{
		cmdqRecWaitNoClear(handle, CMDQ_EVENT_MUTEX0_STREAM_EOF);
	}
	else
	{
		cmdqRecWaitNoClear(handle, CMDQ_SYNC_TOKEN_STREAM_EOF);
	}
	
	dprec_event_op(DPREC_EVENT_CMDQ_WAIT_STREAM_EOF);
}

static int _convert_disp_input_to_rdma(RDMA_CONFIG_STRUCT *dst, primary_disp_input_config* src)
{
	if(src && dst)
	{    		
		dst->inputFormat = src->fmt;		
		dst->address = src->addr;  
		dst->width = src->src_w;
		dst->height = src->src_h;
		dst->pitch = src->src_pitch;

		return 0;
	}
	else
	{
		DISPERR("src(0x%08x) or dst(0x%08x) is null\n", src, dst);
		return -1;
	}
}

static int _convert_disp_input_to_ovl(OVL_CONFIG_STRUCT *dst, primary_disp_input_config* src)
{
	if(src && dst)
	{
		dst->layer = src->layer;
		dst->layer_en = src->layer_en;
		dst->source = src->buffer_source;        
		dst->fmt = src->fmt;
		dst->addr = src->addr;  
		dst->vaddr = src->vaddr;
		dst->src_x = src->src_x;
		dst->src_y = src->src_y;
		dst->src_w = src->src_w;
		dst->src_h = src->src_h;
		dst->src_pitch = src->src_pitch;
		dst->dst_x = src->dst_x;
		dst->dst_y = src->dst_y;
		dst->dst_w = src->dst_w;
		dst->dst_h = src->dst_h;
		dst->keyEn = src->keyEn;
		dst->key = src->key; 
		dst->aen = src->aen; 
		dst->alpha = src->alpha;
        dst->sur_aen = src->sur_aen;
        dst->src_alpha = src->src_alpha;
        dst->dst_alpha = src->dst_alpha;

		dst->isDirty = src->isDirty;

		dst->buff_idx = src->buff_idx;
		dst->identity = src->identity;
		dst->connected_type = src->connected_type;
		dst->security = src->security;
   	    dst->yuv_range = src->yuv_range;     

		return 0;
	}
	else
	{
		DISPERR("src(0x%08x) or dst(0x%08x) is null\n", src, dst);
		return -1;
	}
}

int _trigger_display_interface(int blocking, void *callback, unsigned int userdata)
{
	if(_should_wait_path_idle())
	{
		dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE, HZ*1);
	}
	
	if(_should_update_lcm())
	{
		disp_lcm_update(pgc->plcm, 0, 0, pgc->plcm->params->width, pgc->plcm->params->height, 0);
	}
	
	if(_should_start_path())
	{
		dpmgr_path_start(pgc->dpmgr_handle, primary_display_cmdq_enabled());
	}

	if(_should_trigger_path())
	{	
		// trigger_loop_handle is used only for build trigger loop, which should always be NULL for config thread
		dpmgr_path_trigger(pgc->dpmgr_handle, NULL, primary_display_cmdq_enabled());
	}
	
	if(_should_set_cmdq_dirty())
	{
		_cmdq_set_config_handle_dirty();
	}
	
	if(_should_flush_cmdq_config_handle())
	{
		_cmdq_flush_config_handle(blocking, callback, userdata);
	}
	
	if(_should_reset_cmdq_config_handle())
	{
		_cmdq_reset_config_handle();
	}
	
	if(_should_insert_wait_frame_done_token())
	{
		_cmdq_insert_wait_frame_done_token();
	}

	return 0;
}

int _trigger_overlay_engine(void)
{
	// maybe we need a simple merge mechanism for CPU config.
	dpmgr_path_trigger(pgc->ovl2mem_path_handle, NULL, primary_display_use_cmdq);
}

#define EEEEEEEEEEEEEEE
/******************************************************************************/
/* ESD CHECK / RECOVERY ---- BEGIN                                            */
/******************************************************************************/
static struct task_struct *primary_display_esd_check_task = NULL;

static wait_queue_head_t  esd_check_task_wq; // For Esd Check Task
static atomic_t esd_check_task_wakeup = ATOMIC_INIT(0); // For Esd Check Task
static wait_queue_head_t  esd_ext_te_wq; // For Vdo Mode EXT TE Check
static atomic_t esd_ext_te_event = ATOMIC_INIT(0); // For Vdo Mode EXT TE Check

static int eint_flag = 0; // For DCT Setting

unsigned int _need_do_esd_check(void)
{
	int ret = 0;
#ifdef CONFIG_OF
        if((pgc->plcm->params->dsi.esd_check_enable == 1)&&(islcmconnected == 1))
        {
                ret = 1;
        }
#else
	if(pgc->plcm->params->dsi.esd_check_enable == 1)
	{
	        ret = 1;
	}
#endif
        return ret;
}


unsigned int _need_register_eint(void)
{

	int ret = 1;

	// 1.need do esd check
	// 2.dsi vdo mode
	// 3.customization_esd_check_enable = 0
        if(_need_do_esd_check() == 0)
        {
                ret = 0;
        }
	else if(primary_display_is_video_mode() == 0)
	{
		ret = 0;
	}
	else if(pgc->plcm->params->dsi.customization_esd_check_enable == 1)
	{
		ret = 0;
	}

	return ret;

}
unsigned int _need_wait_esd_eof(void)
{
	int ret = 1;

	// 1.need do esd check
	// 2.customization_esd_check_enable = 1
	// 3.dsi cmd mode
        if(_need_do_esd_check() == 0)
        {
                ret = 0;
        }
        else if(pgc->plcm->params->dsi.customization_esd_check_enable == 0)
	{
		ret = 0;
	}
	else if(primary_display_is_video_mode())
	{
		ret = 0;
	}

	return ret;
}
// For Cmd Mode Read LCM Check
// Config cmdq_handle_config_esd
int _esd_check_config_handle_cmd(void)
{
	int ret = 0; // 0:success

	// 1.reset
	cmdqRecReset(pgc->cmdq_handle_config_esd);

	// 2.write first instruction
	// cmd mode: wait CMDQ_SYNC_TOKEN_STREAM_EOF(wait trigger thread done)
	cmdqRecWaitNoClear(pgc->cmdq_handle_config_esd, CMDQ_SYNC_TOKEN_STREAM_EOF);

	// 3.clear CMDQ_SYNC_TOKEN_ESD_EOF(trigger thread need wait this sync token)
	cmdqRecClearEventToken(pgc->cmdq_handle_config_esd, CMDQ_SYNC_TOKEN_ESD_EOF);

	// 4.write instruction(read from lcm)
	dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_config_esd,CMDQ_ESD_CHECK_READ);

	// 5.set CMDQ_SYNC_TOKE_ESD_EOF(trigger thread can work now)
	cmdqRecSetEventToken(pgc->cmdq_handle_config_esd, CMDQ_SYNC_TOKEN_ESD_EOF);

	// 6.flush instruction
	dprec_logger_start(DPREC_LOGGER_ESD_CMDQ, 0, 0);
	ret = cmdqRecFlush(pgc->cmdq_handle_config_esd);
	dprec_logger_done(DPREC_LOGGER_ESD_CMDQ, 0, 0);

	
	DISPCHECK("[ESD]_esd_check_config_handle_cmd ret=%d\n",ret);

done:
        if(ret) ret=1;
	return ret;
}

// For Vdo Mode Read LCM Check
// Config cmdq_handle_config_esd
int _esd_check_config_handle_vdo(void)
{
	int ret = 0; // 0:success , 1:fail

	// 1.reset
	cmdqRecReset(pgc->cmdq_handle_config_esd);

	// 2.stop dsi vdo mode
	dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_config_esd,CMDQ_STOP_VDO_MODE);

	// 3.write instruction(read from lcm)
	dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_config_esd,CMDQ_ESD_CHECK_READ);

	// 4.start dsi vdo mode
	dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_config_esd,CMDQ_START_VDO_MODE);

	// 5. trigger path
	dpmgr_path_trigger(pgc->dpmgr_handle, pgc->cmdq_handle_config_esd, CMDQ_ENABLE);

	// 6.flush instruction
	dprec_logger_start(DPREC_LOGGER_ESD_CMDQ, 0, 0);
	ret = cmdqRecFlush(pgc->cmdq_handle_config_esd);
	dprec_logger_done(DPREC_LOGGER_ESD_CMDQ, 0, 0);

	DISPCHECK("[ESD]_esd_check_config_handle_vdo ret=%d\n",ret);

done:
	if(ret) ret=1;
	return ret;
}


// ESD CHECK FUNCTION
// return 1: esd check fail
// return 0: esd check pass
int primary_display_esd_check(void)
{
	int ret = 0;

	dprec_logger_start(DPREC_LOGGER_ESD_CHECK, 0, 0);
	MMProfileLogEx(ddp_mmp_get_events()->esd_check_t, MMProfileFlagStart, 0, 0);
	DISPCHECK("[ESD]ESD check begin\n");

        _primary_path_lock(__func__);
	if(pgc->state == DISP_SLEEPED)
	{
		MMProfileLogEx(ddp_mmp_get_events()->esd_check_t, MMProfileFlagPulse, 1, 0);
		DISPCHECK("[ESD]primary display path is sleeped?? -- skip esd check\n");
                _primary_path_unlock(__func__);
		goto done;
	}
        _primary_path_unlock(__func__);
	
        /// Esd Check : EXT TE
        if(pgc->plcm->params->dsi.customization_esd_check_enable==0)
        {
                MMProfileLogEx(ddp_mmp_get_events()->esd_extte, MMProfileFlagStart, 0, 0);
                if(primary_display_is_video_mode())
    	        {
                        if(_need_register_eint())
                        {
                                MMProfileLogEx(ddp_mmp_get_events()->esd_extte, MMProfileFlagPulse, 1, 1);

    		                if(wait_event_interruptible_timeout(esd_ext_te_wq,atomic_read(&esd_ext_te_event),HZ/2)>0)
    		                {
    			                ret = 0; // esd check pass
    		                }
    		                else
    		                {
    			                ret = 1; // esd check fail
    		                }
    		                atomic_set(&esd_ext_te_event, 0);
                        }
    	        }
	        else
    	        {
			MMProfileLogEx(ddp_mmp_get_events()->esd_extte, MMProfileFlagPulse, 0, 1);
    		        if(dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC, HZ/2)>0)
    		        {
    			        ret = 0; // esd check pass
    		        }
    		        else
    		        {
    			        ret = 1; // esd check fail
    		        }
    	        }
    	        MMProfileLogEx(ddp_mmp_get_events()->esd_extte, MMProfileFlagEnd, 0, ret);
    	        goto done;
        }

	/// Esd Check : Read from lcm
	MMProfileLogEx(ddp_mmp_get_events()->esd_rdlcm, MMProfileFlagStart, 0, primary_display_cmdq_enabled());
	if(primary_display_cmdq_enabled())
	{
		MMProfileLogEx(ddp_mmp_get_events()->esd_rdlcm, MMProfileFlagPulse, 0, 1);
		// 0.create esd check cmdq
		cmdqRecCreate(CMDQ_SCENARIO_DISP_ESD_CHECK,&(pgc->cmdq_handle_config_esd));	
		dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_config_esd,CMDQ_ESD_ALLC_SLOT);
                MMProfileLogEx(ddp_mmp_get_events()->esd_rdlcm, MMProfileFlagPulse, 0, 2);
		DISPCHECK("[ESD]ESD config thread=0x%x\n",pgc->cmdq_handle_config_esd);
		
		// 1.use cmdq to read from lcm
		if(primary_display_is_video_mode())
		{
			ret = _esd_check_config_handle_vdo();
		}
		else
		{
			ret = _esd_check_config_handle_cmd();
		}
		MMProfileLogEx(ddp_mmp_get_events()->esd_rdlcm, MMProfileFlagPulse, primary_display_is_video_mode(), 3);
		if(ret == 1) // cmdq fail
		{	
			if(_need_wait_esd_eof())
	                {
		                // Need set esd check eof synctoken to let trigger loop go.
		                cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_ESD_EOF);
	                }
			// do dsi reset
			dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_config_esd,CMDQ_DSI_RESET);
			goto destory_cmdq;
		}
		
		DISPCHECK("[ESD]ESD config thread done~\n");

		// 2.check data(*cpu check now)
		ret = dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_config_esd,CMDQ_ESD_CHECK_CMP);
		MMProfileLogEx(ddp_mmp_get_events()->esd_rdlcm, MMProfileFlagPulse, 0, 4);
		if(ret)
		{
			ret = 1; // esd check fail
		}

destory_cmdq:
		dpmgr_path_build_cmdq(pgc->dpmgr_handle, pgc->cmdq_handle_config_esd,CMDQ_ESD_FREE_SLOT);
		// 3.destory esd config thread
		cmdqRecDestroy(pgc->cmdq_handle_config_esd);
		pgc->cmdq_handle_config_esd = NULL;

	}
	else
	{
    	        /// 0: lock path
    	        /// 1: stop path
    	        /// 2: do esd check (!!!)
    	        /// 3: start path
    	        /// 4: unlock path

		MMProfileLogEx(ddp_mmp_get_events()->esd_rdlcm, MMProfileFlagPulse, 0, 1);
    	        _primary_path_lock(__func__);

    	        /// 1: stop path
    	        DISPCHECK("[ESD]display cmdq trigger loop stop[begin]\n");
    	        _cmdq_stop_trigger_loop();
    	        DISPCHECK("[ESD]display cmdq trigger loop stop[end]\n");

    	        if(dpmgr_path_is_busy(pgc->dpmgr_handle))
    	        {
    		        DISPCHECK("[ESD]primary display path is busy\n");
    		        ret = dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE, HZ*1);
    		        DISPCHECK("[ESD]wait frame done ret:%d\n", ret);
    	        }

    	        DISPCHECK("[ESD]stop dpmgr path[begin]\n");
    	        dpmgr_path_stop(pgc->dpmgr_handle, CMDQ_DISABLE);
    	        DISPCHECK("[ESD]stop dpmgr path[end]\n");

    	        if(dpmgr_path_is_busy(pgc->dpmgr_handle))
    	        {
    		        DISPCHECK("[ESD]primary display path is busy after stop\n");
    		        dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE, HZ*1);
    		        DISPCHECK("[ESD]wait frame done ret:%d\n", ret);
    	        }

    	        DISPCHECK("[ESD]reset display path[begin]\n");
    	        dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);
    	        DISPCHECK("[ESD]reset display path[end]\n");

    	        /// 2: do esd check (!!!)
    	        MMProfileLogEx(ddp_mmp_get_events()->esd_rdlcm, MMProfileFlagPulse, 0, 2);

    	        if(primary_display_is_video_mode())
    	        {
    		        //ret = 0;
    		        ret = disp_lcm_esd_check(pgc->plcm);
    	        }
    	        else
    	        {
    		        ret = disp_lcm_esd_check(pgc->plcm);
    	        }

    	        /// 3: start path
    	        MMProfileLogEx(ddp_mmp_get_events()->esd_rdlcm, MMProfileFlagPulse, primary_display_is_video_mode(), 3);

    	        DISPCHECK("[ESD]start dpmgr path[begin]\n");
    	        dpmgr_path_start(pgc->dpmgr_handle, CMDQ_DISABLE);
    	        DISPCHECK("[ESD]start dpmgr path[end]\n");

    	        DISPCHECK("[ESD]start cmdq trigger loop[begin]\n");
    	        _cmdq_start_trigger_loop();
    	        DISPCHECK("[ESD]start cmdq trigger loop[end]\n");

    	        _primary_path_unlock(__func__);
        }
	MMProfileLogEx(ddp_mmp_get_events()->esd_rdlcm, MMProfileFlagEnd, 0, ret);

done:
	DISPCHECK("[ESD]ESD check end\n");
	MMProfileLogEx(ddp_mmp_get_events()->esd_check_t, MMProfileFlagEnd, 0, ret);	
	dprec_logger_done(DPREC_LOGGER_ESD_CHECK, 0, 0);
	return ret;

}

// For Vdo Mode EXT TE Check
static irqreturn_t _esd_check_ext_te_irq_handler(int irq, void *data)
{
	MMProfileLogEx(ddp_mmp_get_events()->esd_vdo_eint, MMProfileFlagPulse, 0, 0);
 	atomic_set(&esd_ext_te_event, 1);
        wake_up_interruptible(&esd_ext_te_wq);
	return IRQ_HANDLED;	
}

static int primary_display_esd_check_worker_kthread(void *data)
{
	struct sched_param param = { .sched_priority = RTPM_PRIO_FB_THREAD };
    	sched_setscheduler(current, SCHED_RR, &param);
	long int ttt = 0;
	int ret = 0;
	int i = 0;
	int esd_try_cnt = 5;  // 20;
	
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE);
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_START);

	while(1)
	{
		#if 0
		dpmgr_wait_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_START);
		ret = dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE, HZ);
		if(ret <= 0)
		{
			DISPERR("wait frame done timeout, reset whole path now\n");
			primary_display_diagnose();
			dprec_logger_trigger(DPREC_LOGGER_ESD_RECOVERY);
			dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);
		}
		#else
		wait_event_interruptible(esd_check_task_wq,atomic_read(&esd_check_task_wakeup));
		msleep_interruptible(2000); // esd check every 2s
#ifdef DISP_SWITCH_DST_MODE		
		_primary_path_switch_dst_lock();
#endif
		#if 0
		{
			// let's do a mutex holder check here
			unsigned long long period = 0;
			period = dprec_logger_get_current_hold_period(DPREC_LOGGER_PRIMARY_MUTEX);
			if(period > 2000*1000*1000)
			{
				DISPERR("primary display mutex is hold by %s for %dns\n", pgc->mutex_locker, period);				
			}
		}
		#endif	
		ret = primary_display_esd_check();
		if(ret == 1)
		{
			DISPCHECK("[ESD]esd check fail, will do esd recovery\n", ret);
			i = esd_try_cnt;
			while(i--)
			{
				DISPCHECK("[ESD]esd recovery try:%d\n", i);
				primary_display_esd_recovery();
				ret = primary_display_esd_check();
				if(ret == 0)
				{
					DISPCHECK("[ESD]esd recovery success\n");
					break;
				}
				else
				{
					DISPCHECK("[ESD]after esd recovery, esd check still fail\n");
					if(i==0)
					{
						DISPCHECK("[ESD]after esd recovery %d times, esd check still fail, disable esd check\n",esd_try_cnt);
						primary_display_esd_check_enable(0);
					}
				}
			}
		}
#ifdef DISP_SWITCH_DST_MODE
		_primary_path_switch_dst_unlock();
#endif
		#endif
		
		if (kthread_should_stop())
			break;
	}
	return 0;
}

// ESD RECOVERY
int primary_display_esd_recovery(void)
{
	DISP_STATUS ret = DISP_STATUS_OK;

	DISPFUNC();
	dprec_logger_start(DPREC_LOGGER_ESD_RECOVERY, 0, 0);
	MMProfileLogEx(ddp_mmp_get_events()->esd_recovery_t, MMProfileFlagStart, 0, 0);
	DISPCHECK("[ESD]ESD recovery begin\n");
	_primary_path_lock(__func__);

	LCM_PARAMS *lcm_param = NULL;

	lcm_param = disp_lcm_get_params(pgc->plcm);
	if(pgc->state == DISP_SLEEPED)
	{
		DISPCHECK("[ESD]esd recovery but primary display path is sleeped??\n");
		goto done;
	}

	DISPCHECK("[ESD]display cmdq trigger loop stop[begin]\n");
	_cmdq_stop_trigger_loop();
	DISPCHECK("[ESD]display cmdq trigger loop stop[end]\n");

	if(dpmgr_path_is_busy(pgc->dpmgr_handle))
	{
		DISPCHECK("[ESD]primary display path is busy\n");
		ret = dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE, HZ*1);
		DISPCHECK("[ESD]wait frame done ret:%d\n", ret);
	}

	DISPCHECK("[ESD]stop dpmgr path[begin]\n");
	dpmgr_path_stop(pgc->dpmgr_handle, CMDQ_DISABLE);
	DISPCHECK("[ESD]stop dpmgr path[end]\n");

	if(dpmgr_path_is_busy(pgc->dpmgr_handle))
	{
		DISPCHECK("[ESD]primary display path is busy after stop\n");
		dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE, HZ*1);
		DISPCHECK("[ESD]wait frame done ret:%d\n", ret);
	}

	disp_lcm_suspend(pgc->plcm);
	DISPCHECK("[ESD]reset display path[begin]\n");
	dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);
	DISPCHECK("[ESD]reset display path[end]\n");

	DISPCHECK("[ESD]lcm force init[begin]\n");
	//disp_lcm_init(pgc->plcm, 1);
	msleep(250);
	disp_lcm_resume(pgc->plcm);
	DISPCHECK("[ESD]lcm force init[end]\n");

	DISPCHECK("[ESD]start dpmgr path[begin]\n");
	dpmgr_path_start(pgc->dpmgr_handle, CMDQ_DISABLE);
	DISPCHECK("[ESD]start dpmgr path[end]\n");
	if(dpmgr_path_is_busy(pgc->dpmgr_handle))
	{
		DISPERR("[ESD]Fatal error, we didn't trigger display path but it's already busy\n");
		ret = -1;
		//goto done;
	}
	
	if(primary_display_is_video_mode())
	{
		// for video mode, we need to force trigger here
		// for cmd mode, just set DPREC_EVENT_CMDQ_SET_EVENT_ALLOW when trigger loop start
		dpmgr_path_trigger(pgc->dpmgr_handle, NULL, CMDQ_DISABLE);
	}

	DISPCHECK("[ESD]start cmdq trigger loop[begin]\n");
	_cmdq_start_trigger_loop();
	DISPCHECK("[ESD]start cmdq trigger loop[end]\n");

done:
	_primary_path_unlock(__func__);
	DISPCHECK("[ESD]ESD recovery end\n");
	MMProfileLogEx(ddp_mmp_get_events()->esd_recovery_t, MMProfileFlagEnd, 0, 0);
	dprec_logger_done(DPREC_LOGGER_ESD_RECOVERY, 0, 0);
	return ret;
}

void primary_display_esd_check_enable(int enable)
{
    if(_need_do_esd_check())
    {
    	if(_need_register_eint() && eint_flag != 2)
    	{
    		DISPCHECK("[ESD]Please check DCT setting about GPIO107/EINT107 \n");
    	        return;
    	}
    	
    	if(enable)
    	{
    		DISPCHECK("[ESD]esd check thread wakeup\n");
    	        atomic_set(&esd_check_task_wakeup, 1);
    	        wake_up_interruptible(&esd_check_task_wq);
    	}
    	else
    	{
    		DISPCHECK("[ESD]esd check thread stop\n");
    	        atomic_set(&esd_check_task_wakeup, 0); 
    	}
    }
}

/******************************************************************************/
/* ESD CHECK / RECOVERY ---- End                                              */
/******************************************************************************/
#define EEEEEEEEEEEEEEEEEEEEEEEEEE

static struct task_struct *primary_path_aal_task = NULL;

static int _disp_primary_path_check_trigger(void *data)
{
	int ret = 0;

	cmdqRecHandle handle = NULL;
	ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP,&handle);
	
	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_TRIGGER);
	while(1)
	{	
		dpmgr_wait_event(pgc->dpmgr_handle, DISP_PATH_EVENT_TRIGGER);
		MMProfileLogEx(ddp_mmp_get_events()->primary_display_aalod_trigger, MMProfileFlagPulse, 0, 0);
		
		_primary_path_lock(__func__);
		
		if(pgc->state != DISP_SLEEPED)
		{
			cmdqRecReset(handle);
			_cmdq_insert_wait_frame_done_token_mira(handle);		        
			_cmdq_set_config_handle_dirty_mira(handle);
			_cmdq_flush_config_handle_mira(handle, 0);
		}

		_primary_path_unlock(__func__);
		
		if (kthread_should_stop())
			break;
	}

	cmdqRecDestroy(handle);

	return 0;
}
#define OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO

//need remove
unsigned int cmdqDdpClockOn(uint64_t engineFlag)
{
    //DISP_LOG_I("cmdqDdpClockOff\n");
    return 0;
}

unsigned int cmdqDdpClockOff(uint64_t engineFlag)
{
    //DISP_LOG_I("cmdqDdpClockOff\n");
    return 0;
}

unsigned int cmdqDdpDumpInfo(uint64_t engineFlag,
                        char     *pOutBuf,
                        unsigned int bufSize)
{
	DISPERR("cmdq timeout:%llu\n", engineFlag);
	primary_display_diagnose();
    	//DISP_LOG_I("cmdqDdpDumpInfo\n");
    	return 0;
}

unsigned int cmdqDdpResetEng(uint64_t engineFlag)
{
    //DISP_LOG_I("cmdqDdpResetEng\n");
    return 0;
}

// TODO: these 2 functions should be splited into another file
static void _RDMA0_INTERNAL_IRQ_Handler(DISP_MODULE_ENUM module, unsigned int param)
{
	if(param & 0x2)
	{
		// RDMA Start
		spm_sodi_mempll_pwr_mode(1);
	}
	else if(param & 0x4)
	{
		// RDMA Done
		spm_sodi_mempll_pwr_mode(0);
	}
}

void primary_display_sodi_rule_init(void)
{
	if(disp_od_is_enabled() && (primary_display_mode == DECOUPLE_MODE) && primary_display_is_video_mode())
	{
		spm_enable_sodi(0);
		return;
	}
	
	spm_enable_sodi(1);
	if(primary_display_is_video_mode())
	{
		disp_unregister_module_irq_callback(DISP_MODULE_RDMA0, _RDMA0_INTERNAL_IRQ_Handler);//if switch to video mode, should de-register callback
		spm_sodi_mempll_pwr_mode(0);
	}
	else
	{
		disp_register_module_irq_callback(DISP_MODULE_RDMA0, _RDMA0_INTERNAL_IRQ_Handler);
	}
}


extern int dfo_query(const char *s, unsigned long *v);

int primary_display_change_lcm_resolution(unsigned int width, unsigned int height)
{
	if(pgc->plcm)
	{
		DISPMSG("LCM Resolution will be changed, original: %dx%d, now: %dx%d\n", pgc->plcm->params->width, pgc->plcm->params->height, width, height);
		// align with 4 is the minimal check, to ensure we can boot up into kernel, and could modify dfo setting again using meta tool
		// otherwise we will have a panic in lk(root cause unknown).
		if(width >pgc->plcm->params->width || height > pgc->plcm->params->height || width == 0 || height == 0 || width %4 || height %4)
		{
			DISPERR("Invalid resolution: %dx%d\n", width, height);
			return -1;
		}

		if(primary_display_is_video_mode())
		{
			DISPERR("Warning!!!Video Mode can't support multiple resolution!\n");
			return -1;
		}

		pgc->plcm->params->width = width;
		pgc->plcm->params->height = height;

		return 0;
	}
	else
	{
		return -1;
	}
}
static struct task_struct *fence_release_worker_task = NULL;

extern unsigned int ddp_ovl_get_cur_addr(bool rdma_mode, int layerid );

static int _fence_release_worker_thread(void *data)
{
	int ret = 0;
	int i = 0;
	unsigned int addr = 0;
	int fence_idx = -1;
	unsigned int session_id = MAKE_DISP_SESSION(DISP_SESSION_PRIMARY,0);
	struct sched_param param = { .sched_priority = RTPM_PRIO_SCRN_UPDATE };
	sched_setscheduler(current, SCHED_RR, &param);

	dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_START);
	while(1)
	{   
	   	dpmgr_wait_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_START);
		if(!primary_display_is_video_mode())
		{
			DISPPR_FENCE("P0/Frame Start\n");
		}
		
		for(i=0;i<PRIMARY_DISPLAY_SESSION_LAYER_COUNT;i++)
		{
			addr = ddp_ovl_get_cur_addr(!_should_config_ovl_input(), i);
            if(is_dim_layer(addr))
            {
                addr = 0;
            }
			if(i == primary_display_get_option("ASSERT_LAYER") && is_DAL_Enabled())
			{
				mtkfb_release_layer_fence(session_id, 3);
			}
			else
			{
				fence_idx = disp_sync_find_fence_idx_by_addr(session_id, i, addr);
				if(fence_idx <0) 
				{
					if(fence_idx == -1)
					{
						DISPPR_ERROR("find fence idx for layer %d,addr 0x%08x fail, unregistered addr%d\n", i, addr, fence_idx);
					}
					else if(fence_idx == -2)
					{

					}
					else
					{
						DISPPR_ERROR("find fence idx for layer %d,addr 0x%08x fail,reason unknown%d\n", i, addr, fence_idx);
					}
				}
				else
				{
					mtkfb_release_fence(session_id, i, fence_idx);
				}
			}
		}

		MMProfileLogEx(ddp_mmp_get_events()->session_release, MMProfileFlagEnd, 0, 0);
	   	
	   	if(kthread_should_stop())
		   	break;
   	}	

   	return 0;
}

static struct task_struct *present_fence_release_worker_task = NULL;

static int _present_fence_release_worker_thread(void *data)
{
   int ret = 0;

   cmdqRecHandle handle = NULL;
   ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP,&handle);
   
   dpmgr_enable_event(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE);
   while(1)
   {   
   	msleep_interruptible(10000000);
	   
	   if (kthread_should_stop())
		   break;
   }

   cmdqRecDestroy(handle);

   return 0;
}

#define xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx

int primary_display_set_frame_buffer_address(unsigned long va,unsigned long mva)
{

    DISPMSG("framebuffer va %lu, mva %lu\n", va, mva);
    pgc->framebuffer_va = va;
    pgc->framebuffer_mva = mva;
/*
    int frame_buffer_size = ALIGN_TO(DISP_GetScreenWidth(), MTK_FB_ALIGNMENT) * 
                      ALIGN_TO(DISP_GetScreenHeight(), MTK_FB_ALIGNMENT) * 4;
    unsigned long dim_layer_va = va + 2*frame_buffer_size;
    dim_layer_mva = mva + 2*frame_buffer_size;
    memset(dim_layer_va, 0, frame_buffer_size);
*/
    return 0;
}

unsigned long primary_display_get_frame_buffer_mva_address(void)
{
    return pgc->framebuffer_mva;
}

unsigned long primary_display_get_frame_buffer_va_address(void)
{
    return pgc->framebuffer_va;
}

int is_dim_layer(unsigned int long mva)
{
	if(mva == dim_layer_mva)
		return 1;
	return 0;
}

unsigned long get_dim_layer_mva_addr(void)
{
    if(dim_layer_mva == 0){
        int frame_buffer_size = ALIGN_TO(DISP_GetScreenWidth(), MTK_FB_ALIGNMENT) * 
                  ALIGN_TO(DISP_GetScreenHeight(), MTK_FB_ALIGNMENT) * 4;
        unsigned long dim_layer_va =  pgc->framebuffer_va + 3*frame_buffer_size;
        memset(dim_layer_va, 0, frame_buffer_size);
        dim_layer_mva =  pgc->framebuffer_mva + 3*frame_buffer_size;
		DISPMSG("init dim layer mva %lu, size %d", dim_layer_mva, frame_buffer_size);
    }
    return dim_layer_mva;
}

int primary_display_init(char *lcm_name, unsigned int lcm_fps)
{
	DISPFUNC();
	DISP_STATUS ret = DISP_STATUS_OK;
	DISP_MODULE_ENUM dst_module = 0;
	
	unsigned int lcm_fake_width = 0;
	unsigned int lcm_fake_height = 0;
	
	LCM_PARAMS *lcm_param = NULL;
	LCM_INTERFACE_ID lcm_id = LCM_INTERFACE_NOTDEFINED;
	dprec_init();
	//xuecheng, for debug
	//dprec_handle_option(0x3);

	dpmgr_init();

	mutex_init(&(pgc->capture_lock));
	mutex_init(&(pgc->lock));
#ifdef DISP_SWITCH_DST_MODE
	mutex_init(&(pgc->switch_dst_lock));
#endif
	_primary_path_lock(__func__);
	
	pgc->plcm = disp_lcm_probe( lcm_name, LCM_INTERFACE_NOTDEFINED);

	if(pgc->plcm == NULL)
	{
		DISPCHECK("disp_lcm_probe returns null\n");
		ret = DISP_STATUS_ERROR;
		goto done;
	}
	else
	{
		DISPCHECK("disp_lcm_probe SUCCESS\n");
	}
	
	if((0 == dfo_query("LCM_FAKE_WIDTH", &lcm_fake_width)) && (0 == dfo_query("LCM_FAKE_HEIGHT", &lcm_fake_height)))
	{
		printk("[DFO] LCM_FAKE_WIDTH=%d, LCM_FAKE_HEIGHT=%d\n", lcm_fake_width, lcm_fake_height);
		if(lcm_fake_width && lcm_fake_height)
		{
			if(DISP_STATUS_OK != primary_display_change_lcm_resolution(lcm_fake_width, lcm_fake_height))
			{
				DISPMSG("[DISP\DFO]WARNING!!! Change LCM Resolution FAILED!!!\n");
			}
		}
	}

	lcm_param = disp_lcm_get_params(pgc->plcm);
	 
	if(lcm_param == NULL)
	{
		DISPERR("get lcm params FAILED\n");
		ret = DISP_STATUS_ERROR;
		goto done;
	}
	 
	ret = cmdqCoreRegisterCB(CMDQ_GROUP_DISP,	cmdqDdpClockOn,cmdqDdpDumpInfo,cmdqDdpResetEng,cmdqDdpClockOff);
	if(ret)
	{
		DISPERR("cmdqCoreRegisterCB failed, ret=%d \n", ret);
		ret = DISP_STATUS_ERROR;
		goto done;
	}					 
	
	ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP,&(pgc->cmdq_handle_config));
	if(ret)
	{
		DISPCHECK("cmdqRecCreate FAIL, ret=%d \n", ret);
		ret = DISP_STATUS_ERROR;
		goto done;
	}
	else
	{
		DISPCHECK("cmdqRecCreate SUCCESS, g_cmdq_handle=0x%x \n", pgc->cmdq_handle_config);
	}

	if(primary_display_mode == DIRECT_LINK_MODE)
	{
		_build_path_direct_link();
		
		DISPCHECK("primary display is DIRECT LINK MODE\n");
	}
	else if(primary_display_mode == DECOUPLE_MODE)
	{
		_build_path_decouple();
		
		DISPCHECK("primary display is DECOUPLE MODE\n");
	}
	else if(primary_display_mode == SINGLE_LAYER_MODE)
	{
		_build_path_single_layer();
		
		DISPCHECK("primary display is SINGLE LAYER MODE\n");
	}
	else if(primary_display_mode == DEBUG_RDMA1_DSI0_MODE)
	{
		_build_path_debug_rdma1_dsi0();
		
		DISPCHECK("primary display is DEBUG RDMA1 DSI0 MODE\n");
	}
	else
	{
		DISPCHECK("primary display mode is WRONG\n");
	}

	_cmdq_build_trigger_loop();
	
	_cmdq_start_trigger_loop();

	dpmgr_path_set_video_mode(pgc->dpmgr_handle, primary_display_is_video_mode());

	primary_display_use_cmdq = CMDQ_ENABLE;

	if(primary_display_use_cmdq == CMDQ_ENABLE)
	{
		_cmdq_reset_config_handle();
		_cmdq_insert_wait_frame_done_token();
	}
	
	dpmgr_path_init(pgc->dpmgr_handle, CMDQ_ENABLE);

	disp_ddp_path_config *data_config = NULL;

	data_config = dpmgr_path_get_last_config(pgc->dpmgr_handle);

	memcpy(&(data_config->dispif_config), lcm_param, sizeof(LCM_PARAMS));

	data_config->dst_w = lcm_param->width;
	data_config->dst_h = lcm_param->height;
	if(lcm_param->type == LCM_TYPE_DSI)
	{
		if(lcm_param->dsi.data_format.format == LCM_DSI_FORMAT_RGB888)
			data_config->lcm_bpp = 24;
		else if(lcm_param->dsi.data_format.format == LCM_DSI_FORMAT_RGB565)
			data_config->lcm_bpp = 16;
		else if(lcm_param->dsi.data_format.format == LCM_DSI_FORMAT_RGB666)
			data_config->lcm_bpp = 18;
	}
	else if(lcm_param->type == LCM_TYPE_DPI)
	{
		if( lcm_param->dpi.format == LCM_DPI_FORMAT_RGB888)
			data_config->lcm_bpp = 24;
		else if( lcm_param->dpi.format == LCM_DPI_FORMAT_RGB565)
			data_config->lcm_bpp = 16;
		if( lcm_param->dpi.format == LCM_DPI_FORMAT_RGB666)
			data_config->lcm_bpp = 18;
	}
	
	data_config->fps = lcm_fps;
	data_config->dst_dirty = 1;
	
	ret = dpmgr_path_config(pgc->dpmgr_handle, data_config, pgc->cmdq_handle_config);
	
	// should we set dirty here????????
	_cmdq_flush_config_handle(0, NULL, 0);

	_cmdq_reset_config_handle();
	_cmdq_insert_wait_frame_done_token();
	{
		ret = disp_lcm_init(pgc->plcm, 0);
	}
	
	init_waitqueue_head(&esd_ext_te_wq);
	init_waitqueue_head(&esd_check_task_wq);
	if(_need_do_esd_check())
        {
            primary_display_esd_check_task = kthread_create(primary_display_esd_check_worker_kthread, NULL, "display_esd_check");
	        wake_up_process(primary_display_esd_check_task);	
                //primary_display_esd_check_enable(1);
        }
		
	if(_need_register_eint())
	{
		int node;
		int irq;
		u32 ints[2]={0,0};
#ifdef GPIO_DSI_TE_PIN
		// 1.set GPIO107 eint mode	
		mt_set_gpio_mode(GPIO_DSI_TE_PIN, GPIO_DSI_TE_PIN_M_GPIO);
		eint_flag++;
#endif

		// 2.register eint
		node = of_find_compatible_node(NULL,NULL,"mediatek, DSI_TE_1-eint");
                if(node)
                {
                        of_property_read_u32_array(node,"debounce",ints,ARRAY_SIZE(ints));
			mt_gpio_set_debounce(ints[0],ints[1]);

			irq = irq_of_parse_and_map(node,0);
			if(request_irq(irq, _esd_check_ext_te_irq_handler, IRQF_TRIGGER_NONE, "DSI_TE_1-eint", NULL))
			{
				DISPCHECK("[ESD]EINT IRQ LINE NOT AVAILABLE!!\n");
			}
			else
			{
				eint_flag++;
			}
                }
		else
		{
		    DISPCHECK("[ESD][%s] can't find DSI_TE_1 eint compatible node\n",__func__);
		}
	}

		
	if(_need_do_esd_check())
	{
	    primary_display_esd_check_enable(1);
	}
	if (g_boot_mode == NORMAL_BOOT) {
		primary_display_idle_detect_task = kthread_create(_disp_primary_path_idle_detect_thread, NULL, "display_idle_detect");
		wake_up_process(primary_display_idle_detect_task);
	}

#ifdef DISP_SWITCH_DST_MODE
	primary_display_switch_dst_mode_task = kthread_create(_disp_primary_path_switch_dst_mode_thread, NULL, "display_switch_dst_mode");
	wake_up_process(primary_display_switch_dst_mode_task);
#endif

	primary_path_aal_task = kthread_create(_disp_primary_path_check_trigger, NULL, "display_check_aal");
	wake_up_process(primary_path_aal_task);

	fence_release_worker_task = kthread_create(_fence_release_worker_thread, NULL, "fence_worker");
	wake_up_process(fence_release_worker_task);
	
	present_fence_release_worker_task = kthread_create(_present_fence_release_worker_thread, NULL, "present_fence_worker");
	wake_up_process(present_fence_release_worker_task);


	primary_display_use_cmdq = CMDQ_ENABLE;

	// this will be set to always enable cmdq later
	if(primary_display_is_video_mode())
	{
#ifdef DISP_SWITCH_DST_MODE
		primary_display_cur_dst_mode = 1;//video mode
		primary_display_def_dst_mode = 1;//default mode is video mode
#endif
		dpmgr_map_event_to_irq(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC, DDP_IRQ_RDMA0_DONE);
	}
	else
	{

	}
	
	
	{
		M4U_PORT_STRUCT sPort;
		sPort.ePortID = M4U_PORT_DISP_WDMA0;
		sPort.Virtuality = primary_display_use_m4u; 				   
		sPort.Security = 0;
		sPort.Distance = 1;
		sPort.Direction = 0;
		ret = m4u_config_port(&sPort);
		if(ret != 0)
		{
			DISPCHECK("config M4U Port %s to %s FAIL(ret=%d)\n",ddp_get_module_name(DISP_MODULE_WDMA0), primary_display_use_m4u?"virtual":"physical", ret);
			return -1;
		}
	}

	pgc->lcm_fps = lcm_fps;
	if(lcm_fps > 6000)
	{
		pgc->max_layer = 4;
	}
	else
	{
		pgc->max_layer = 4;
	}
	pgc->state = DISP_ALIVE;

	primary_display_sodi_rule_init();
done:

	_primary_path_unlock(__func__);
	return ret;
}

int primary_display_deinit(void)
{
	_primary_path_lock(__func__);

	_cmdq_stop_trigger_loop();
	dpmgr_path_deinit(pgc->dpmgr_handle, CMDQ_DISABLE);
	_primary_path_unlock(__func__);
	return 0;
}

// register rdma done event
int primary_display_wait_for_idle(void)
{	
	DISP_STATUS ret = DISP_STATUS_OK;

	DISPFUNC();

	_primary_path_lock(__func__);
	
done:
	_primary_path_unlock(__func__);
	return ret;
}

int primary_display_wait_for_dump(void)
{
	
}

int primary_display_wait_for_vsync(void *config)
{
	disp_session_vsync_config *c = (disp_session_vsync_config *)config;
	int ret = 0;
	if(!islcmconnected)
	{
		DISPCHECK("lcm not connect, use fake vsync\n");
		msleep(16);
		return 0;
	}
	
	ret = dpmgr_wait_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);	
	if(ret == -2)
	{
		DISPCHECK("vsync for primary display path not enabled yet\n");
		return -1;
	}
	
	if(pgc->vsync_drop)
	{
		ret = dpmgr_wait_event(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC);	
	}
		
	//DISPMSG("vsync signaled\n");
	c->vsync_ts = get_current_time_us();
	c->vsync_cnt ++;

	return ret;
}

unsigned int primary_display_get_ticket(void)
{
	return dprec_get_vsync_count();
}

int primary_suspend_release_fence(void)
{
    unsigned int session = (unsigned int)( (DISP_SESSION_PRIMARY)<<16 | (0));
    unsigned int i=0; 
	for (i = 0; i < HW_OVERLAY_COUNT; i++)
	{
		DISPMSG("mtkfb_release_layer_fence  session=0x%x,layerid=%d \n", session,i);
		mtkfb_release_layer_fence(session, i);
	}
	return 0;
}
int primary_display_suspend(void)
{
	DISP_STATUS ret = DISP_STATUS_OK;

	DISPFUNC();
	
	MMProfileLogEx(ddp_mmp_get_events()->primary_suspend, MMProfileFlagStart, 0, 0);
#ifdef DISP_SWITCH_DST_MODE
	primary_display_switch_dst_mode(primary_display_def_dst_mode);
#endif
	disp_sw_mutex_lock(&(pgc->capture_lock));
	_primary_path_lock(__func__);
	if(pgc->state == DISP_SLEEPED)
	{
		DISPCHECK("primary display path is already sleep, skip\n");
		goto done;
	}

	MMProfileLogEx(ddp_mmp_get_events()->primary_suspend, MMProfileFlagPulse, 0, 1);
	if(dpmgr_path_is_busy(pgc->dpmgr_handle))
	{
		MMProfileLogEx(ddp_mmp_get_events()->primary_suspend, MMProfileFlagPulse, 1, 2);
		int event_ret = dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE, HZ*1);
		MMProfileLogEx(ddp_mmp_get_events()->primary_suspend, MMProfileFlagPulse, 2, 2);
		DISPCHECK("[POWER]primary display path is busy now, wait frame done, event_ret=%d\n", event_ret);
		if(event_ret<=0)
		{
			DISPERR("wait frame done in suspend timeout\n");
			MMProfileLogEx(ddp_mmp_get_events()->primary_suspend, MMProfileFlagPulse, 3, 2);
			primary_display_diagnose();
			ret = -1;	
		}
	}	
	
	MMProfileLogEx(ddp_mmp_get_events()->primary_suspend, MMProfileFlagPulse, 0, 2);

	DISPCHECK("[POWER]display cmdq trigger loop stop[begin]\n");
	_cmdq_stop_trigger_loop();
	DISPCHECK("[POWER]display cmdq trigger loop stop[end]\n");
	MMProfileLogEx(ddp_mmp_get_events()->primary_suspend, MMProfileFlagPulse, 0, 3);

	DISPCHECK("[POWER]primary display path stop[begin]\n");
	dpmgr_path_stop(pgc->dpmgr_handle, CMDQ_DISABLE);
	DISPCHECK("[POWER]primary display path stop[end]\n");
	MMProfileLogEx(ddp_mmp_get_events()->primary_suspend, MMProfileFlagPulse, 0, 4);

	if(dpmgr_path_is_busy(pgc->dpmgr_handle))
	{
		MMProfileLogEx(ddp_mmp_get_events()->primary_suspend, MMProfileFlagPulse,1, 4);
		DISPERR("[POWER]stop display path failed, still busy\n");
		dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);
		ret = -1;
		// even path is busy(stop fail), we still need to continue power off other module/devices
		//goto done;
	}
	MMProfileLogEx(ddp_mmp_get_events()->primary_suspend, MMProfileFlagPulse, 0, 5);

	DISPCHECK("[POWER]lcm suspend[begin]\n");
	disp_lcm_suspend(pgc->plcm);
	DISPCHECK("[POWER]lcm suspend[end]\n");
	MMProfileLogEx(ddp_mmp_get_events()->primary_suspend, MMProfileFlagPulse, 0, 6);
	DISPCHECK("[POWER]primary display path Release Fence[begin]\n");
	primary_suspend_release_fence();
	DISPCHECK("[POWER]primary display path Release Fence[end]\n");
	MMProfileLogEx(ddp_mmp_get_events()->primary_suspend, MMProfileFlagPulse, 0, 7);

	DISPCHECK("[POWER]dpmanager path power off[begin]\n");
	dpmgr_path_power_off(pgc->dpmgr_handle, CMDQ_DISABLE);
	DISPCHECK("[POWER]dpmanager path power off[end]\n");
	MMProfileLogEx(ddp_mmp_get_events()->primary_suspend, MMProfileFlagPulse, 0, 8);

	pgc->state = DISP_SLEEPED;
done:
	_primary_path_unlock(__func__);
	disp_sw_mutex_unlock(&(pgc->capture_lock));	
	aee_kernel_wdt_kick_Powkey_api("mtkfb_early_suspend", WDT_SETBY_Display);
	primary_trigger_cnt = 0;
    /* clear dim layer buffer */
	dim_layer_mva = 0;
	MMProfileLogEx(ddp_mmp_get_events()->primary_suspend, MMProfileFlagEnd, 0, 0);
	return ret;
}

int primary_display_get_lcm_index(void)
{
	int index = 0;
	DISPFUNC();
	
	if(pgc->plcm == NULL)
	{
		DISPERR("lcm handle is null\n");
		return 0;
	}

	index = pgc->plcm->index; 
	DISPMSG("lcm index = %d\n", index);
	return index;
}

int primary_display_resume(void)
{
	DISP_STATUS ret = DISP_STATUS_OK;

	DISPFUNC();
	MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagStart, 0, 0);

	_primary_path_lock(__func__);
	if(pgc->state == DISP_ALIVE)
	{
		DISPCHECK("primary display path is already resume, skip\n");
		goto done;
	}
	MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 0, 1);
	
	DISPCHECK("dpmanager path power on[begin]\n");
	dpmgr_path_power_on(pgc->dpmgr_handle, CMDQ_DISABLE);
	DISPCHECK("dpmanager path power on[end]\n");
	
	MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 0, 2);

    /* clear dim layer buffer */
	dim_layer_mva = 0;
	if(is_ipoh_bootup)
	{
	  DISPCHECK("[primary display path] leave primary_display_resume -- IPOH\n");
      DISPCHECK("ESD check start[begin]\n");
      primary_display_esd_check_enable(1);
      DISPCHECK("ESD check start[end]\n");
	  is_ipoh_bootup = false;
	  DISPCHECK("[POWER]start cmdq[begin]--IPOH\n");
	  _cmdq_start_trigger_loop();
	  DISPCHECK("[POWER]start cmdq[end]--IPOH\n");	  
	  pgc->state = DISP_ALIVE;	  
	  goto done;
	}
	DISPCHECK("[POWER]dpmanager re-init[begin]\n");

	{
		dpmgr_path_connect(pgc->dpmgr_handle, CMDQ_DISABLE);
		MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 1, 2);
		LCM_PARAMS *lcm_param = disp_lcm_get_params(pgc->plcm);

		disp_ddp_path_config * data_config = dpmgr_path_get_last_config(pgc->dpmgr_handle); 

		data_config->dst_w = lcm_param->width;
		data_config->dst_h = lcm_param->height;
		if(lcm_param->type == LCM_TYPE_DSI)
		{
		    if(lcm_param->dsi.data_format.format == LCM_DSI_FORMAT_RGB888)
		        data_config->lcm_bpp = 24;
		    else if(lcm_param->dsi.data_format.format == LCM_DSI_FORMAT_RGB565)
		        data_config->lcm_bpp = 16;
		    else if(lcm_param->dsi.data_format.format == LCM_DSI_FORMAT_RGB666)
		        data_config->lcm_bpp = 18;
		}
		else if(lcm_param->type == LCM_TYPE_DPI)
		{
		    if( lcm_param->dpi.format == LCM_DPI_FORMAT_RGB888)
		        data_config->lcm_bpp = 24;
		    else if( lcm_param->dpi.format == LCM_DPI_FORMAT_RGB565)
		        data_config->lcm_bpp = 16;
		    if( lcm_param->dpi.format == LCM_DPI_FORMAT_RGB666)
		        data_config->lcm_bpp = 18;
		}

		data_config->fps = pgc->lcm_fps;
		data_config->dst_dirty = 1;

		ret = dpmgr_path_config(pgc->dpmgr_handle, data_config, NULL);
		MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 2, 2);
		data_config->dst_dirty = 0;
	}
	DISPCHECK("[POWER]dpmanager re-init[end]\n");
	MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 0, 3);

	DISPCHECK("[POWER]lcm resume[begin]\n");
	disp_lcm_resume(pgc->plcm);
	DISPCHECK("[POWER]lcm resume[end]\n");
	
	MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 0, 4);
	if(dpmgr_path_is_busy(pgc->dpmgr_handle))
	{
		MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 1, 4);
		DISPERR("[POWER]Fatal error, we didn't start display path but it's already busy\n");
		ret = -1;
		//goto done;
	}
	
	MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 0, 5);
	DISPCHECK("[POWER]dpmgr path start[begin]\n");
	dpmgr_path_start(pgc->dpmgr_handle, CMDQ_DISABLE);
	DISPCHECK("[POWER]dpmgr path start[end]\n");
	
	MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 0, 6);
	if(dpmgr_path_is_busy(pgc->dpmgr_handle))
	{
		MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 1, 6);
		DISPERR("[POWER]Fatal error, we didn't trigger display path but it's already busy\n");
		ret = -1;
		//goto done;
	}
	
	MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 0, 7);
	if(primary_display_is_video_mode())
	{
		MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 1, 7);
		// for video mode, we need to force trigger here
		// for cmd mode, just set DPREC_EVENT_CMDQ_SET_EVENT_ALLOW when trigger loop start
		dpmgr_path_trigger(pgc->dpmgr_handle, NULL, CMDQ_DISABLE);
	}
	MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 0, 8);

	DISPCHECK("[POWER]start cmdq[begin]\n");
	_cmdq_start_trigger_loop();
	DISPCHECK("[POWER]start cmdq[end]\n");
	MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagPulse, 0, 9);

    if(!primary_display_is_video_mode())
    {
        /*refresh black picture of ovl bg*/
        DISPCHECK("[POWER]triggger cmdq[begin]\n");
        _trigger_display_interface(0, NULL, 0);
        DISPCHECK("[POWER]triggger cmdq[end]\n");
    }
#if 0
	DISPCHECK("[POWER]wakeup aal/od trigger process[begin]\n");
	wake_up_process(primary_path_aal_task);
	DISPCHECK("[POWER]wakeup aal/od trigger process[end]\n");
#endif
	pgc->state = DISP_ALIVE;

done:
	_primary_path_unlock(__func__);
	//primary_display_diagnose();
	aee_kernel_wdt_kick_Powkey_api("mtkfb_late_resume", WDT_SETBY_Display);
	MMProfileLogEx(ddp_mmp_get_events()->primary_resume, MMProfileFlagEnd, 0, 0);
	return 0;
}
int primary_display_ipoh_restore(void)
{
    DISPMSG("primary_display_ipoh_restore In\n");
    DISPCHECK("ESD check stop[begin]\n");
    primary_display_esd_check_enable(0);
    DISPCHECK("ESD check stop[end]\n");
	if(NULL!=pgc->cmdq_handle_trigger)
	{
		struct TaskStruct *pTask = pgc->cmdq_handle_trigger->pRunningTask;
		if(NULL != pTask)
		{
			DISPCHECK("[Primary_display]display cmdq trigger loop stop[begin]\n");
			_cmdq_stop_trigger_loop();
			DISPCHECK("[Primary_display]display cmdq trigger loop stop[end]\n");
		}
	}
	DISPMSG("primary_display_ipoh_restore Out\n");
	return 0;
}

int primary_display_start(void)
{
	DISP_STATUS ret = DISP_STATUS_OK;

	DISPFUNC();

	_primary_path_lock(__func__);
	dpmgr_path_start(pgc->dpmgr_handle, CMDQ_DISABLE);

	if(dpmgr_path_is_busy(pgc->dpmgr_handle))
	{
		DISPCHECK("Fatal error, we didn't trigger display path but it's already busy\n");
		ret = -1;
		goto done;
	}

done:
	_primary_path_unlock(__func__);
	return ret;
}

int primary_display_stop(void)
{
	DISP_STATUS ret = DISP_STATUS_OK;

	DISPFUNC();
	_primary_path_lock(__func__);

	if(dpmgr_path_is_busy(pgc->dpmgr_handle))
	{
		dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE, HZ*1);
	}
	
	dpmgr_path_stop(pgc->dpmgr_handle, CMDQ_DISABLE);

	if(dpmgr_path_is_busy(pgc->dpmgr_handle))
	{
		DISPCHECK("stop display path failed, still busy\n");
		ret = -1;
		goto done;
	}

done:
	_primary_path_unlock(__func__);
	return ret;
}

int primary_display_trigger(int blocking, void *callback, unsigned int userdata)
{
	int ret = 0;

	last_primary_trigger_time = sched_clock();
#ifdef DISP_SWITCH_DST_MODE
	if(is_switched_dst_mode)
	{
		primary_display_switch_dst_mode(1);//swith to vdo mode if trigger disp
		is_switched_dst_mode = false;
	}
#endif	
	primary_display_save_power_for_idle(0);

	primary_trigger_cnt++;
	_primary_path_lock(__func__);

	if(pgc->state == DISP_SLEEPED)
	{
		DISPMSG("%s, skip because primary dipslay is sleep\n", __func__);
		goto done;
	}

	dprec_logger_start(DPREC_LOGGER_PRIMARY_TRIGGER, 0, 0);

	if(_should_trigger_interface())
	{	
		_trigger_display_interface(blocking, callback, userdata);
	}
	else
	{
		_trigger_overlay_engine();
	}

	dprec_logger_done(DPREC_LOGGER_PRIMARY_TRIGGER, 0, 0);

done:
	_primary_path_unlock(__func__);
	if((primary_trigger_cnt > PRIMARY_DISPLAY_TRIGGER_CNT) && aee_kernel_Powerkey_is_press())
	{
		aee_kernel_wdt_kick_Powkey_api("primary_display_trigger", WDT_SETBY_Display);
		primary_trigger_cnt = 0;
	}
	return ret;
}

static int primary_display_ovl2mem_callback(unsigned int userdata)
{
    int i = 0;
    unsigned int session_id = MAKE_DISP_SESSION(DISP_SESSION_PRIMARY,0);
    int fence_idx = userdata;

    disp_ddp_path_config *data_config= dpmgr_path_get_last_config(pgc->dpmgr_handle);

    if(data_config)
    {
        WDMA_CONFIG_STRUCT wdma_layer;
    
        wdma_layer.dstAddress = mtkfb_query_buf_mva(session_id, 4, fence_idx);
        wdma_layer.outputFormat = data_config->wdma_config.outputFormat;
        wdma_layer.srcWidth = primary_display_get_width();
        wdma_layer.srcHeight = primary_display_get_height();
        wdma_layer.dstPitch = data_config->wdma_config.dstPitch;
        dprec_mmp_dump_wdma_layer(&wdma_layer, 0);
    }    
                    
    if(fence_idx > 0)
        mtkfb_release_fence(session_id, EXTERNAL_DISPLAY_SESSION_LAYER_COUNT, fence_idx);

    DISPMSG("mem_out release fence idx:0x%x\n", fence_idx);
     
    return 0;
}
int primary_display_mem_out_trigger(int blocking, void *callback, unsigned int userdata)
{
	int ret = 0;
	//DISPFUNC();
	if(pgc->state == DISP_SLEEPED || pgc->ovl1to2_mode < DISP_SESSION_DIRECT_LINK_MIRROR_MODE)
	{
		DISPMSG("mem out trigger is already sleeped or mode wrong(%d)\n", pgc->ovl1to2_mode);		
		return 0;
	}
	
    ///dprec_logger_start(DPREC_LOGGER_PRIMARY_TRIGGER, 0, 0);

    //if(blocking)
    {
        _primary_path_lock(__func__);
    }

    if(pgc->need_trigger_ovl1to2 == 0)
    {
        goto done;
    }
    
    if(_should_wait_path_idle())
    {
        dpmgr_wait_event_timeout(pgc->ovl2mem_path_handle, DISP_PATH_EVENT_FRAME_DONE, HZ*1);
    }
        
    if(_should_trigger_path())
    {   
        ///dpmgr_path_trigger(pgc->dpmgr_handle, NULL, primary_display_cmdq_enabled());
    }
    if(_should_set_cmdq_dirty())
    {
        _cmdq_set_config_handle_dirty_mira(pgc->cmdq_handle_ovl1to2_config);
    }
    
    if(_should_flush_cmdq_config_handle())
        _cmdq_flush_config_handle_mira(pgc->cmdq_handle_ovl1to2_config, 0);  

    if(_should_reset_cmdq_config_handle())
        cmdqRecReset(pgc->cmdq_handle_ovl1to2_config);

    cmdqRecWait(pgc->cmdq_handle_ovl1to2_config, CMDQ_EVENT_DISP_WDMA0_SOF);
    
    _cmdq_insert_wait_frame_done_token_mira(pgc->cmdq_handle_ovl1to2_config);
    dpmgr_path_remove_memout(pgc->ovl2mem_path_handle, pgc->cmdq_handle_ovl1to2_config);

    if(_should_set_cmdq_dirty())
        _cmdq_set_config_handle_dirty_mira(pgc->cmdq_handle_ovl1to2_config);
    

    if(_should_flush_cmdq_config_handle())
        ///_cmdq_flush_config_handle_mira(pgc->cmdq_handle_ovl1to2_config, 0);  
        cmdqRecFlushAsyncCallback(pgc->cmdq_handle_ovl1to2_config, primary_display_ovl2mem_callback, userdata);

    if(_should_reset_cmdq_config_handle())
        cmdqRecReset(pgc->cmdq_handle_ovl1to2_config);      

    ///_cmdq_insert_wait_frame_done_token_mira(pgc->cmdq_handle_ovl1to2_config);
    
done:

    pgc->need_trigger_ovl1to2 = 0;
    
    _primary_path_unlock(__func__); 
    
    ///dprec_logger_done(DPREC_LOGGER_PRIMARY_TRIGGER, 0, 0);

	return ret;
}


int primary_display_config_output(disp_mem_output_config* output)
{
	int ret = 0;
	int i=0;
	int layer =0;
	int need_lock = !primary_display_cmdq_enabled();
	disp_ddp_path_config *pconfig =NULL;
	unsigned int mva = output->addr;
	unsigned int w_xres = primary_display_get_width();
	unsigned int h_yres = primary_display_get_height();
	unsigned int pixel_byte =primary_display_get_bpp() / 8; // bpp is either 32 or 16, can not be other value

	///DISPMSG("primary_display_config_output, state %d, m_mode %d\n", pgc->mira_state ,  pgc->mira_mode);
	
	if(pgc->state == DISP_SLEEPED || pgc->ovl1to2_mode < DISP_SESSION_DIRECT_LINK_MIRROR_MODE)
	{
		DISPMSG("mem out is already sleeped or mode wrong(%d)\n", pgc->ovl1to2_mode);		
		return 0;
	}
	///dprec_logger_start(DPREC_LOGGER_PRIMARY_CONFIG, output->layer|(output->layer_en<<16), output->addr);

	_primary_path_lock(__func__);

	///_cmdq_insert_wait_frame_done_token_mira(pgc->cmdq_handle_ovl1to2_config);
    pconfig = dpmgr_path_get_last_config(pgc->ovl2mem_path_handle);
    pconfig->wdma_dirty                     = 1;
    pconfig->wdma_config.dstAddress         = output->addr;
    pconfig->wdma_config.srcHeight          = h_yres;
    pconfig->wdma_config.srcWidth           = w_xres;
    pconfig->wdma_config.clipX              = 0;
    pconfig->wdma_config.clipY              = 0;
    pconfig->wdma_config.clipHeight         = h_yres;
    pconfig->wdma_config.clipWidth          = w_xres;
    pconfig->wdma_config.outputFormat       = output->fmt; 
    pconfig->wdma_config.useSpecifiedAlpha  = 1;
    pconfig->wdma_config.alpha              = 0xFF;
    pconfig->wdma_config.dstPitch           = output->pitch;///w_xres * DP_COLOR_BITS_PER_PIXEL(output->fmt)/8;

	// hope we can use only 1 input struct for input config, just set layer number
	if(pgc->need_trigger_ovl1to2 ==0 && pgc->ovl1to2_mode > 0)
	{
	    _cmdq_insert_wait_frame_done_token_mira(primary_display_cmdq_enabled()?pgc->cmdq_handle_ovl1to2_config:NULL);
		dpmgr_path_add_memout(pgc->ovl2mem_path_handle, ENGINE_OVL0, primary_display_cmdq_enabled()?pgc->cmdq_handle_ovl1to2_config:NULL);
		pgc->need_trigger_ovl1to2 = 1;
	}

	ret = dpmgr_path_config(pgc->ovl2mem_path_handle, pconfig, primary_display_cmdq_enabled()?pgc->cmdq_handle_ovl1to2_config:NULL);
	
	_primary_path_unlock(__func__);
	
	///dprec_logger_done(DPREC_LOGGER_PRIMARY_CONFIG, output->src_x, output->src_y);
	return ret;

}

int primary_display_config_input_multiple(primary_disp_input_config* input)
{
	int ret = 0;
	int i=0;
	int layer =0;
	
	disp_ddp_path_config *data_config;	

	_primary_path_lock(__func__);

	// all dirty should be cleared in dpmgr_path_get_last_config()
	data_config = dpmgr_path_get_last_config(pgc->dpmgr_handle);
	data_config->dst_dirty = 0;
	data_config->ovl_dirty = 0;
	data_config->rdma_dirty = 0;
	data_config->wdma_dirty = 0;
	
	if(pgc->state == DISP_SLEEPED)
	{
		DISPMSG("%s, skip because primary dipslay is sleep\n", __func__);
		goto done;
	}
	
	// hope we can use only 1 input struct for input config, just set layer number
	if(_should_config_ovl_input())
	{
		for(i = 0;i<HW_OVERLAY_COUNT;i++)
		{	
			if(input[i].layer_en)
			{
				if(input[i].vaddr)
				{
					_debug_pattern(0x00000000, input[i].vaddr, input[i].dst_w, input[i].dst_h, input[i].src_pitch, 0x00000000, input[i].layer, input[i].buff_idx);
				}	
				else
				{
					_debug_pattern(input[i].addr,0x00000000,  input[i].dst_w, input[i].dst_h, input[i].src_pitch, 0x00000000, input[i].layer, input[i].buff_idx);
				}
			}

			//DISPMSG("[primary], i:%d, layer:%d, layer_en:%d, dirty:%d\n", i, input[i].layer, input[i].layer_en, input[i].dirty);
			if(input[i].dirty)
			{
				dprec_logger_start(DPREC_LOGGER_PRIMARY_CONFIG, input[i].layer|(input[i].layer_en<<16), input[i].addr);
				dprec_mmp_dump_ovl_layer(&(data_config->ovl_config[input[i].layer]),input[i].layer, 1);
				ret = _convert_disp_input_to_ovl(&(data_config->ovl_config[input[i].layer]), &input[i]);
				dprec_logger_done(DPREC_LOGGER_PRIMARY_CONFIG, input[i].src_x, input[i].src_y);
			}
			data_config->ovl_dirty = 1;	

		}
	}
	else
	{
		ret = _convert_disp_input_to_rdma(&(data_config->rdma_config), input);
		data_config->rdma_dirty= 1;
	}
	
	if(_should_wait_path_idle())
	{
		dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE, HZ*1);
	}
	
	ret = dpmgr_path_config(pgc->dpmgr_handle, data_config, primary_display_cmdq_enabled()?pgc->cmdq_handle_config:NULL);

	// this is used for decouple mode, to indicate whether we need to trigger ovl
	pgc->need_trigger_overlay = 1;

done:
	_primary_path_unlock(__func__);
    	return ret;
}

int primary_display_config_input(primary_disp_input_config* input)
{
	int ret = 0;
	int i=0;
	int layer =0;
	
	disp_ddp_path_config *data_config;	

	// all dirty should be cleared in dpmgr_path_get_last_config()
	data_config = dpmgr_path_get_last_config(pgc->dpmgr_handle);
	data_config->dst_dirty = 0;
	data_config->ovl_dirty = 0;
	data_config->rdma_dirty = 0;
	data_config->wdma_dirty = 0;
	
	_primary_path_lock(__func__);
	
	if(pgc->state == DISP_SLEEPED)
	{
		if(isAEEEnabled && input->layer == primary_display_get_option("ASSERT_LAYER"))
		{
			// hope we can use only 1 input struct for input config, just set layer number
			if(_should_config_ovl_input())
			{
				ret = _convert_disp_input_to_ovl(&(data_config->ovl_config[input->layer]), input);
				data_config->ovl_dirty = 1;
			}
			else
			{
				ret = _convert_disp_input_to_rdma(&(data_config->rdma_config), input);
				data_config->rdma_dirty= 1;
			}
			DISPCHECK("%s save temp asset layer ,because primary dipslay is sleep\n", __func__);
		}
		DISPMSG("%s, skip because primary dipslay is sleep\n", __func__);
		goto done;
	}

	dprec_logger_start(DPREC_LOGGER_PRIMARY_CONFIG, input->layer|(input->layer_en<<16), input->addr);
	
	if(input->layer_en)
	{
		if(input->vaddr)
		{
			_debug_pattern(0x00000000, input->vaddr, input->dst_w, input->dst_h, input->src_pitch, 0x00000000, input->layer, input->buff_idx);
		}	
		else
		{
			_debug_pattern(input->addr,0x00000000,  input->dst_w, input->dst_h, input->src_pitch, 0x00000000, input->layer, input->buff_idx);
		}
	}

	// hope we can use only 1 input struct for input config, just set layer number
	if(_should_config_ovl_input())
	{
		ret = _convert_disp_input_to_ovl(&(data_config->ovl_config[input->layer]), input);
		data_config->ovl_dirty = 1;
	}
	else
	{
		ret = _convert_disp_input_to_rdma(&(data_config->rdma_config), input);
		data_config->rdma_dirty= 1;
	}
	
	if(_should_wait_path_idle())
	{
		dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE, HZ*1);
	}
	
	ret = dpmgr_path_config(pgc->dpmgr_handle, data_config, primary_display_cmdq_enabled()?pgc->cmdq_handle_config:NULL);

	// this is used for decouple mode, to indicate whether we need to trigger ovl
	pgc->need_trigger_overlay = 1;

	dprec_logger_done(DPREC_LOGGER_PRIMARY_CONFIG, input->src_x, input->src_y);


done:	

	_primary_path_unlock(__func__);
	return ret;
}

static int Panel_Master_primary_display_config_dsi(const char * name,UINT32  config_value)

{
	int ret = 0;	
	disp_ddp_path_config *data_config;	
	// all dirty should be cleared in dpmgr_path_get_last_config()
	data_config = dpmgr_path_get_last_config(pgc->dpmgr_handle);
	//modify below for config dsi
	if(!strcmp(name, "PM_CLK"))
	{
		printk("Pmaster_config_dsi: PM_CLK:%d\n", config_value);
		data_config->dispif_config.dsi.PLL_CLOCK= config_value;	
	}
	else if(!strcmp(name, "PM_SSC"))
	{	
		data_config->dispif_config.dsi.ssc_range=config_value;
	}
	printk("Pmaster_config_dsi: will Run path_config()\n");
	ret = dpmgr_path_config(pgc->dpmgr_handle, data_config, NULL);

	return ret;
}

int primary_display_cmdq_set_reg(unsigned int addr, unsigned int val)
{
	int ret =0;
	cmdqRecHandle handle = NULL;
	
	ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP,&handle);
	cmdqRecReset(handle);
	_cmdq_insert_wait_frame_done_token_mira(handle);		        

	cmdqRecWrite(handle, addr&0x1fffffff, val, ~0);
	cmdqRecFlushAsync(handle);
	
	cmdqRecDestroy(handle);

	return 0;
}

int primary_display_user_cmd(unsigned int cmd, unsigned long arg)
{
	int ret =0;
	cmdqRecHandle handle = NULL;
	int cmdqsize = 0;

	MMProfileLogEx(ddp_mmp_get_events()->primary_display_cmd, MMProfileFlagStart, handle, 0);    	

	ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP,&handle);
	cmdqRecReset(handle);
	_cmdq_insert_wait_frame_done_token_mira(handle);		        
	cmdqsize = cmdqRecGetInstructionCount(handle);
		
	_primary_path_lock(__func__);
	if(pgc->state == DISP_SLEEPED)
	{
		cmdqRecDestroy(handle);
		handle = NULL;
	}
	_primary_path_unlock(__func__); 		

	ret = dpmgr_path_user_cmd(pgc->dpmgr_handle,cmd,arg, handle);

	if(handle)
	{
		if(cmdqRecGetInstructionCount(handle) > cmdqsize)
		{
			_primary_path_lock(__func__);
			if(pgc->state == DISP_ALIVE)
			{
				_cmdq_set_config_handle_dirty_mira(handle);
				// use non-blocking flush here to avoid primary path is locked for too long
				_cmdq_flush_config_handle_mira(handle, 0);
			}
			_primary_path_unlock(__func__);			
		}
		
		cmdqRecDestroy(handle);
	}
	
	MMProfileLogEx(ddp_mmp_get_events()->primary_display_cmd, MMProfileFlagEnd, handle, cmdqsize);    		
	
	return ret;
}

int primary_display_switch_mode(int sess_mode)
{
    int ret = 0;
    DISPMSG("primary_display_switch_mode to %d\n", sess_mode);  
    _primary_path_lock(__func__);

    if(sess_mode == DISP_SESSION_DIRECT_LINK_MIRROR_MODE || sess_mode == DISP_SESSION_DECOUPLE_MIRROR_MODE)
    {
        if(pgc->ovl1to2_mode == sess_mode)
            goto done;
            
        #if 1
        if(primary_display_cmdq_enabled())
	    {
		    cmdqRecCreate(CMDQ_SCENARIO_DISP_SCREEN_CAPTURE, &pgc->cmdq_handle_ovl1to2_config); 
		    
    		if(ret!=0)
    		{
    			_primary_path_unlock(__func__);
    			return -1;
    		}
	
		    cmdqRecReset(pgc->cmdq_handle_ovl1to2_config);
		    ///cmdqRecReset(pgc->cmdq_handle_ovl1to2_eof_config);
        }
 
		pgc->ovl2mem_path_handle = pgc->dpmgr_handle ;//dpmgr_create_path(CMDQ_SCENARIO_PRIMARY_DISP, pgc->cmdq_handle_ovl1to2_config); // pgc->dpmgr_handle ;
    	if(pgc->ovl2mem_path_handle)
    	{
    		DISPMSG("dpmgr create path SUCCESS(0x%08x ), dphandle (0x%08x)\n", pgc->cmdq_handle_ovl1to2_config, pgc->ovl2mem_path_handle);
    	}
    	else
    	{
    		DISPCHECK("dpmgr create path FAIL\n");
    		return -1;
    	}
    	#else
	    pgc->cmdq_handle_ovl1to2_config = pgc->cmdq_handle_config;
	    pgc->ovl2mem_path_handle = pgc->dpmgr_handle; 
    	#endif
    	
        M4U_PORT_STRUCT sPort;
		sPort.ePortID = M4U_PORT_DISP_WDMA0;
		sPort.Virtuality = primary_display_use_m4u;					   
		sPort.Security = 0;
		sPort.Distance = 1;
		sPort.Direction = 0;
		ret = m4u_config_port(&sPort);
		 
        dpmgr_path_memout_clock(pgc->ovl2mem_path_handle, 1);
        pgc->ovl1to2_mode = sess_mode;
    }
    else
    {
        if(pgc->ovl1to2_mode == 0)
            goto done;
            
        pgc->need_trigger_ovl1to2 = 0;
        pgc->ovl1to2_mode = 0;

        if(pgc->cmdq_handle_ovl1to2_config)
            cmdqRecDestroy(pgc->cmdq_handle_ovl1to2_config); 

        if(pgc->ovl2mem_path_handle)
            dpmgr_path_memout_clock(pgc->ovl2mem_path_handle, 0);
        
        pgc->cmdq_handle_ovl1to2_config = NULL;
        pgc->ovl2mem_path_handle = 0;
    }

done:	
	_primary_path_unlock(__func__);

	DISPMSG("primary_display_switch_mode done %d\n", sess_mode); 
}

int primary_display_is_alive(void)
{
	unsigned int temp = 0;
	//DISPFUNC();
	_primary_path_lock(__func__);
	
	if(pgc->state == DISP_ALIVE)
	{
		temp = 1;
	}

	_primary_path_unlock(__func__);
	
	return temp;
}

int primary_display_is_sleepd(void)
{
	unsigned int temp = 0;
	//DISPFUNC();
	_primary_path_lock(__func__);
	
	if(pgc->state == DISP_SLEEPED)
	{
		temp = 1;
	}

	_primary_path_unlock(__func__);
	
	return temp;
}



int primary_display_get_width(void)
{
	if(pgc->plcm == NULL)
	{
		DISPERR("lcm handle is null\n");
		return 0;
	}
	
	if(pgc->plcm->params)
	{
		return pgc->plcm->params->width;
	}
	else
	{
		DISPERR("lcm_params is null!\n");
		return 0;
	}
}

int primary_display_get_height(void)
{
	if(pgc->plcm == NULL)
	{
		DISPERR("lcm handle is null\n");
		return 0;
	}
	
	if(pgc->plcm->params)
	{
		return pgc->plcm->params->height;
	}
	else
	{
		DISPERR("lcm_params is null!\n");
		return 0;
	}
}


int primary_display_get_original_width(void)
{
	if(pgc->plcm == NULL)
	{
		DISPERR("lcm handle is null\n");
		return 0;
	}
	
	if(pgc->plcm->params)
	{
		return pgc->plcm->lcm_original_width;
	}
	else
	{
		DISPERR("lcm_params is null!\n");
		return 0;
	}
}

int primary_display_get_original_height(void)
{
	if(pgc->plcm == NULL)
	{
		DISPERR("lcm handle is null\n");
		return 0;
	}
	
	if(pgc->plcm->params)
	{
		return pgc->plcm->lcm_original_height;
	}
	else
	{
		DISPERR("lcm_params is null!\n");
		return 0;
	}
}

int primary_display_get_bpp(void)
{
	return 32;
}

void primary_display_set_max_layer(int maxlayer)
{
	pgc->max_layer = maxlayer;
}

int primary_display_get_info(void *info)
{
#if 1
	//DISPFUNC();
	disp_session_info* dispif_info = (disp_session_info*)info;

	LCM_PARAMS *lcm_param = disp_lcm_get_params(pgc->plcm);
	if(lcm_param == NULL)
	{
		DISPCHECK("lcm_param is null\n");
		return -1;
	}

	memset((void*)dispif_info, 0, sizeof(disp_session_info));

	// TODO: modify later
	if(is_DAL_Enabled() && pgc->max_layer == 4)
		dispif_info->maxLayerNum = pgc->max_layer -1;		
	else
		dispif_info->maxLayerNum = pgc->max_layer;
	
	switch(lcm_param->type)
	{
		case LCM_TYPE_DBI:
		{
			dispif_info->displayType = DISP_IF_TYPE_DBI;
			dispif_info->displayMode = DISP_IF_MODE_COMMAND;
			dispif_info->isHwVsyncAvailable = 1;
			//DISPMSG("DISP Info: DBI, CMD Mode, HW Vsync enable\n");
			break;
		}
		case LCM_TYPE_DPI:
		{
			dispif_info->displayType = DISP_IF_TYPE_DPI;
			dispif_info->displayMode = DISP_IF_MODE_VIDEO;
			dispif_info->isHwVsyncAvailable = 1;				
			//DISPMSG("DISP Info: DPI, VDO Mode, HW Vsync enable\n");
			break;
		}
		case LCM_TYPE_DSI:
		{
			dispif_info->displayType = DISP_IF_TYPE_DSI0;
			if(lcm_param->dsi.mode == CMD_MODE)
			{
				dispif_info->displayMode = DISP_IF_MODE_COMMAND;
				dispif_info->isHwVsyncAvailable = 1;
				//DISPMSG("DISP Info: DSI, CMD Mode, HW Vsync enable\n");
			}
			else
			{
				dispif_info->displayMode = DISP_IF_MODE_VIDEO;
				dispif_info->isHwVsyncAvailable = 1;
				//DISPMSG("DISP Info: DSI, VDO Mode, HW Vsync enable\n");
			}
			
			break;
		}
		default:
		break;
	}
	

	dispif_info->displayFormat = DISP_IF_FORMAT_RGB888;

	dispif_info->displayWidth = primary_display_get_width();
	dispif_info->displayHeight = primary_display_get_height();
	
	dispif_info->vsyncFPS = pgc->lcm_fps;

	if(dispif_info->displayWidth * dispif_info->displayHeight <= 240*432)
	{
		dispif_info->physicalHeight= dispif_info->physicalWidth= 0;
	}
	else if(dispif_info->displayWidth * dispif_info->displayHeight <= 320*480)
	{
		dispif_info->physicalHeight= dispif_info->physicalWidth= 0;
	}
	else if(dispif_info->displayWidth * dispif_info->displayHeight <= 480*854)
	{
		dispif_info->physicalHeight= dispif_info->physicalWidth= 0;
	}
	else
	{
		dispif_info->physicalHeight= dispif_info->physicalWidth= 0;
	}
	
	dispif_info->isConnected = 1;

#ifdef ROME_TODO
#error
	{
		LCM_PARAMS lcm_params_temp;
		memset((void*)&lcm_params_temp, 0, sizeof(lcm_params_temp));
		if(lcm_drv)
		{
			lcm_drv->get_params(&lcm_params_temp);
			dispif_info->lcmOriginalWidth = lcm_params_temp.width;
			dispif_info->lcmOriginalHeight = lcm_params_temp.height;			
			DISPMSG("DISP Info: LCM Panel Original Resolution(For DFO Only): %d x %d\n", dispif_info->lcmOriginalWidth, dispif_info->lcmOriginalHeight);
		}
		else
		{
			DISPMSG("DISP Info: Fatal Error!!, lcm_drv is null\n");
		}
	}
#endif

#endif
}

int primary_display_get_pages(void)
{
	return 3;
}


int primary_display_is_video_mode(void)
{
	// TODO: we should store the video/cmd mode in runtime, because ROME will support cmd/vdo dynamic switch
	return disp_lcm_is_video_mode(pgc->plcm);
}

int primary_display_diagnose(void)
{
	int ret = 0;
	dpmgr_check_status(pgc->dpmgr_handle);
	primary_display_check_path(NULL, 0);
	
	return ret;
}

CMDQ_SWITCH primary_display_cmdq_enabled(void)
{
	return primary_display_use_cmdq;
}

int primary_display_switch_cmdq_cpu(CMDQ_SWITCH use_cmdq)
{
	_primary_path_lock(__func__);

	primary_display_use_cmdq = use_cmdq;
	DISPCHECK("display driver use %s to config register now\n", (use_cmdq==CMDQ_ENABLE)?"CMDQ":"CPU");

	_primary_path_unlock(__func__);
	return primary_display_use_cmdq;
}

int primary_display_manual_lock(void)
{
	_primary_path_lock(__func__);
}

int primary_display_manual_unlock(void)
{
	_primary_path_unlock(__func__);
}

void primary_display_reset(void)
{
	dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);
}

unsigned int primary_display_get_fps(void)
{
	unsigned int fps = 0;
	_primary_path_lock(__func__);
	fps = pgc->lcm_fps;
	_primary_path_unlock(__func__);

	return fps;
}
int primary_display_force_set_vsync_fps(unsigned int fps)
{
	int ret = 0;	
	DISPMSG("force set fps to %d\n", fps);
	_primary_path_lock(__func__);

	if(fps == pgc->lcm_fps)
	{
		pgc->vsync_drop = 0;
		ret = 0;
	}
	else if(fps == 30)
	{	
		pgc->vsync_drop = 1;
		ret = 0;
	}
	else
	{
		ret = -1;
	}

	_primary_path_unlock(__func__);

	return ret;
}

int primary_display_enable_path_cg(int enable)
{
	int ret = 0;	
	DISPMSG("%s primary display's path cg\n", enable?"enable":"disable");
	_primary_path_lock(__func__);

	if(enable)
	{
		ret += disable_clock(MT_CG_DISP1_DSI0_ENGINE, "DSI0");
		ret += disable_clock(MT_CG_DISP1_DSI0_DIGITAL, "DSI0");
		ret += disable_clock(MT_CG_DISP0_DISP_RDMA0 , "DDP");
		ret += disable_clock(MT_CG_DISP0_DISP_OVL0 , "DDP");
		ret += disable_clock(MT_CG_DISP0_DISP_COLOR0, "DDP");
		ret += disable_clock(MT_CG_DISP0_DISP_OD , "DDP");
		ret += disable_clock(MT_CG_DISP0_DISP_UFOE , "DDP");
	    	ret += disable_clock(MT_CG_DISP0_DISP_AAL, "DDP");
       		//ret += disable_clock(MT_CG_DISP1_DISP_PWM0_26M , "PWM");
       		//ret += disable_clock(MT_CG_DISP1_DISP_PWM0_MM , "PWM");
		
		ret += disable_clock(MT_CG_DISP0_MUTEX_32K	 , "Debug");
		ret += disable_clock(MT_CG_DISP0_SMI_LARB0	 , "Debug");
		ret += disable_clock(MT_CG_DISP0_SMI_COMMON  , "Debug");
		
		ret += disable_clock(MT_CG_DISP0_MUTEX_32K   , "Debug2");
		ret += disable_clock(MT_CG_DISP0_SMI_LARB0   , "Debug2");
		ret += disable_clock(MT_CG_DISP0_SMI_COMMON  , "Debug2");

	}		
	else
	{
		ret += enable_clock(MT_CG_DISP1_DSI0_ENGINE, "DSI0");
		ret += enable_clock(MT_CG_DISP1_DSI0_DIGITAL, "DSI0");
		ret += enable_clock(MT_CG_DISP0_DISP_RDMA0 , "DDP");
		ret += enable_clock(MT_CG_DISP0_DISP_OVL0 , "DDP");
		ret += enable_clock(MT_CG_DISP0_DISP_COLOR0, "DDP");
		ret += enable_clock(MT_CG_DISP0_DISP_OD , "DDP");
		ret += enable_clock(MT_CG_DISP0_DISP_UFOE , "DDP");
	    	ret += enable_clock(MT_CG_DISP0_DISP_AAL, "DDP");
        	//ret += enable_clock(MT_CG_DISP1_DISP_PWM0_26M, "PWM");
        	//ret += enable_clock(MT_CG_DISP1_DISP_PWM0_MM, "PWM");
			
		ret += enable_clock(MT_CG_DISP0_MUTEX_32K	 , "Debug");
		ret += enable_clock(MT_CG_DISP0_SMI_LARB0	 , "Debug");
		ret += enable_clock(MT_CG_DISP0_SMI_COMMON  , "Debug");
		
		ret += enable_clock(MT_CG_DISP0_MUTEX_32K   , "Debug2");
		ret += enable_clock(MT_CG_DISP0_SMI_LARB0   , "Debug2");
		ret += enable_clock(MT_CG_DISP0_SMI_COMMON  , "Debug2");
	}
	
	_primary_path_unlock(__func__);

	return ret;
}

int _set_backlight_by_cmdq(unsigned int level)
{
	MMProfileLogEx(ddp_mmp_get_events()->primary_set_bl, MMProfileFlagPulse, 1, 1);
	int ret=0;
	cmdqRecHandle cmdq_handle_backlight = NULL;
	ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP,&cmdq_handle_backlight);
	DISPCHECK("primary backlight, handle=0x%08x\n", cmdq_handle_backlight);
	if(ret!=0)
	{
		DISPCHECK("fail to create primary cmdq handle for backlight\n");
		return -1;
	}

    if(primary_display_is_video_mode())
   	{	
   	    MMProfileLogEx(ddp_mmp_get_events()->primary_set_bl, MMProfileFlagPulse, 1, 2);
		cmdqRecReset(cmdq_handle_backlight);	
		dpmgr_path_ioctl(pgc->dpmgr_handle, cmdq_handle_backlight, DDP_BACK_LIGHT, &level);
		_cmdq_flush_config_handle_mira(cmdq_handle_backlight, 1);		
		DISPCHECK("[BL]_set_backlight_by_cmdq ret=%d\n",ret);
   	}
	else
	{	
	    MMProfileLogEx(ddp_mmp_get_events()->primary_set_bl, MMProfileFlagPulse, 1, 3);
		cmdqRecReset(cmdq_handle_backlight);
		_cmdq_insert_wait_frame_done_token_mira(cmdq_handle_backlight);
		cmdqRecClearEventToken(cmdq_handle_backlight, CMDQ_SYNC_TOKEN_CABC_EOF);
		dpmgr_path_ioctl(pgc->dpmgr_handle, cmdq_handle_backlight, DDP_BACK_LIGHT, &level);
		cmdqRecSetEventToken(cmdq_handle_backlight, CMDQ_SYNC_TOKEN_CABC_EOF);
		MMProfileLogEx(ddp_mmp_get_events()->primary_set_bl, MMProfileFlagPulse, 1, 4);
		_cmdq_flush_config_handle_mira(cmdq_handle_backlight, 1);
		MMProfileLogEx(ddp_mmp_get_events()->primary_set_bl, MMProfileFlagPulse, 1, 6);
		DISPCHECK("[BL]_set_backlight_by_cmdq ret=%d\n",ret);
	}	
	cmdqRecDestroy(cmdq_handle_backlight);		
	cmdq_handle_backlight = NULL;
	MMProfileLogEx(ddp_mmp_get_events()->primary_set_bl, MMProfileFlagPulse, 1, 5);
	return ret;
}
int _set_backlight_by_cpu(unsigned int level)
{
	int ret=0;
	MMProfileLogEx(ddp_mmp_get_events()->primary_set_bl, MMProfileFlagPulse, 0, 1);
    if(primary_display_is_video_mode())
   	{
		disp_lcm_set_backlight(pgc->plcm,level);
   	}
	else
	{		
		DISPCHECK("[BL]display cmdq trigger loop stop[begin]\n");
		_cmdq_stop_trigger_loop();
		DISPCHECK("[BL]display cmdq trigger loop stop[end]\n");
		
		if(dpmgr_path_is_busy(pgc->dpmgr_handle))
		{
			DISPCHECK("[BL]primary display path is busy\n");
			ret = dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE, HZ*1);
			DISPCHECK("[BL]wait frame done ret:%d\n", ret);
		}

		DISPCHECK("[BL]stop dpmgr path[begin]\n");
		dpmgr_path_stop(pgc->dpmgr_handle, CMDQ_DISABLE);
		DISPCHECK("[BL]stop dpmgr path[end]\n");
		if(dpmgr_path_is_busy(pgc->dpmgr_handle))
		{
			DISPCHECK("[BL]primary display path is busy after stop\n");
			dpmgr_wait_event_timeout(pgc->dpmgr_handle, DISP_PATH_EVENT_FRAME_DONE, HZ*1);
			DISPCHECK("[BL]wait frame done ret:%d\n", ret);
		}
		DISPCHECK("[BL]reset display path[begin]\n");
		dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);
		DISPCHECK("[BL]reset display path[end]\n");
		
		MMProfileLogEx(ddp_mmp_get_events()->primary_set_bl, MMProfileFlagPulse, 0, 2);
		
		disp_lcm_set_backlight(pgc->plcm,level);
		
		MMProfileLogEx(ddp_mmp_get_events()->primary_set_bl, MMProfileFlagPulse, 0, 3);

		DISPCHECK("[BL]start dpmgr path[begin]\n"); 			
		dpmgr_path_start(pgc->dpmgr_handle, CMDQ_DISABLE);
		DISPCHECK("[BL]start dpmgr path[end]\n");

		DISPCHECK("[BL]start cmdq trigger loop[begin]\n");
		_cmdq_start_trigger_loop();
		DISPCHECK("[BL]start cmdq trigger loop[end]\n");
	}		
	MMProfileLogEx(ddp_mmp_get_events()->primary_set_bl, MMProfileFlagPulse, 0, 7);
	return ret;
}
int primary_display_setbacklight(unsigned int level)
{
	DISPFUNC();
	int ret=0;
	MMProfileLogEx(ddp_mmp_get_events()->primary_set_bl, MMProfileFlagStart, 0, 0);
#ifdef DISP_SWITCH_DST_MODE
	_primary_path_switch_dst_lock();
#endif
	_primary_path_lock(__func__);
	if(pgc->state == DISP_SLEEPED)
	{
		DISPCHECK("Sleep State set backlight invald\n");
	}
	else
	{
		if(primary_display_cmdq_enabled())	
		{	
			if(primary_display_is_video_mode())
			{
				MMProfileLogEx(ddp_mmp_get_events()->primary_set_bl, MMProfileFlagPulse, 0, 7);
				disp_lcm_set_backlight(pgc->plcm,level);
			}
			else
			{
				_set_backlight_by_cmdq(level);
			}
		}
		else
		{
			_set_backlight_by_cpu(level);
		}
	}
	_primary_path_unlock(__func__);
#ifdef DISP_SWITCH_DST_MODE
	_primary_path_switch_dst_lock();
#endif
	MMProfileLogEx(ddp_mmp_get_events()->primary_set_bl, MMProfileFlagEnd, 0, 0);
	return ret;
}

#define LLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLL

/***********************/ 
/*****Legacy DISP API*****/
/***********************/
UINT32 DISP_GetScreenWidth(void)
{
	 return primary_display_get_width();
}

UINT32 DISP_GetScreenHeight(void)
{
	return primary_display_get_height();
}
UINT32 DISP_GetActiveHeight(void)
{
	if(pgc->plcm == NULL)
	{
		DISPERR("lcm handle is null\n");
		return 0;
	}
	
	if(pgc->plcm->params)
	{
		return pgc->plcm->params->physical_height;
	}
	else
	{
		DISPERR("lcm_params is null!\n");
		return 0;
	}
}


UINT32 DISP_GetActiveWidth(void)
{
	if(pgc->plcm == NULL)
	{
		DISPERR("lcm handle is null\n");
		return 0;
	}
	
	if(pgc->plcm->params)
	{
		return pgc->plcm->params->physical_width;
	}
	else
	{
		DISPERR("lcm_params is null!\n");
		return 0;
	}
}

LCM_PARAMS * DISP_GetLcmPara(void)
{
	if(pgc->plcm == NULL)
		{
			DISPERR("lcm handle is null\n");
			return NULL;
		}
		
		if(pgc->plcm->params)
		{
			return pgc->plcm->params;
		}
		else
		return NULL;
}

LCM_DRIVER * DISP_GetLcmDrv(void)
{

if(pgc->plcm == NULL)
		{
			DISPERR("lcm handle is null\n");
			return NULL;
		}
		
		if(pgc->plcm->drv)
		{
			return pgc->plcm->drv ;
		}
		else
		return NULL;
}

int primary_display_capture_framebuffer_ovl(unsigned int pbuf, unsigned int format)
{
	unsigned int i =0;
	int ret =0;
	cmdqRecHandle cmdq_handle = NULL;
    cmdqRecHandle cmdq_wait_handle = NULL;
	disp_ddp_path_config *pconfig =NULL;
    m4u_client_t * m4uClient = NULL;
	unsigned int mva = 0;
	unsigned int w_xres = primary_display_get_width();
	unsigned int h_yres = primary_display_get_height();
	unsigned int pixel_byte =primary_display_get_bpp() / 8; // bpp is either 32 or 16, can not be other value
	int buffer_size = h_yres*w_xres*pixel_byte;
	DISPMSG("primary capture: begin\n");

	disp_sw_mutex_lock(&(pgc->capture_lock));
    
    if (primary_display_is_sleepd()|| !primary_display_cmdq_enabled())
    {
        memset(pbuf, 0,buffer_size);
		DISPMSG("primary capture: Fail black End\n");
        goto out;
    }
    
	m4uClient = m4u_create_client();
	if(m4uClient == NULL)
	{
		DISPCHECK("primary capture:Fail to alloc  m4uClient\n",m4uClient);
        ret = -1;
        goto out;
	}
	
	ret = m4u_alloc_mva(m4uClient,M4U_PORT_DISP_WDMA0, pbuf, NULL,buffer_size,M4U_PROT_READ | M4U_PROT_WRITE,0,&mva);
	if(ret !=0 )
	{
		 DISPCHECK("primary capture:Fail to allocate mva\n");
         ret = -1;
         goto out;
	}
	
	ret = m4u_cache_sync(m4uClient, M4U_PORT_DISP_WDMA0, pbuf, buffer_size, mva, M4U_CACHE_FLUSH_BY_RANGE);
	if(ret !=0 )
	{
		DISPCHECK("primary capture:Fail to cach sync\n");
        ret = -1;
        goto out;
	}
	
    if(primary_display_cmdq_enabled())
	{
	    /*create config thread*/
		ret = cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP,&cmdq_handle);
		if(ret!=0)
		{
		    DISPCHECK("primary capture:Fail to create primary cmdq handle for capture\n");
            ret = -1;
            goto out;
		}
		cmdqRecReset(cmdq_handle);
        
        /*create wait thread*/
        ret = cmdqRecCreate(CMDQ_SCENARIO_DISP_SCREEN_CAPTURE,&cmdq_wait_handle);
		if(ret!=0)
		{
		    DISPCHECK("primary capture:Fail to create primary cmdq wait handle for capture\n");
            ret = -1;
            goto out;
		}        
        cmdqRecReset(cmdq_wait_handle);
        
        /*configure  config thread*/
		_cmdq_insert_wait_frame_done_token_mira(cmdq_handle);
		dpmgr_path_memout_clock(pgc->dpmgr_handle, 1);
		
		_primary_path_lock(__func__);		
		pconfig = dpmgr_path_get_last_config(pgc->dpmgr_handle);
		pconfig->wdma_dirty 					= 1;
		pconfig->wdma_config.dstAddress 		= mva;
		pconfig->wdma_config.srcHeight			= h_yres;
		pconfig->wdma_config.srcWidth			= w_xres;
		pconfig->wdma_config.clipX				= 0;
		pconfig->wdma_config.clipY				= 0;
		pconfig->wdma_config.clipHeight 		= h_yres;
		pconfig->wdma_config.clipWidth			= w_xres;
		pconfig->wdma_config.outputFormat		= format; 
		pconfig->wdma_config.useSpecifiedAlpha	= 1;
		pconfig->wdma_config.alpha				= 0xFF;
		pconfig->wdma_config.dstPitch			= w_xres * DP_COLOR_BITS_PER_PIXEL(format)/8;
		dpmgr_path_add_memout(pgc->dpmgr_handle, ENGINE_OVL0, cmdq_handle);
		ret = dpmgr_path_config(pgc->dpmgr_handle, pconfig, cmdq_handle);
		pconfig->wdma_dirty  = 0;
		_primary_path_unlock(__func__);
		_cmdq_set_config_handle_dirty_mira(cmdq_handle);
		_cmdq_flush_config_handle_mira(cmdq_handle, 0);
		DISPMSG("primary capture:Flush add memout mva(0x%x)\n",mva);
        
		/*wait wdma0 sof*/
        cmdqRecWait(cmdq_wait_handle,CMDQ_EVENT_DISP_WDMA0_SOF);
        cmdqRecFlush(cmdq_wait_handle);
        DISPMSG("primary capture:Flush wait wdma sof\n");

		cmdqRecReset(cmdq_handle);
		_cmdq_insert_wait_frame_done_token_mira(cmdq_handle);
		_primary_path_lock(__func__);
		dpmgr_path_remove_memout(pgc->dpmgr_handle, cmdq_handle);
		_primary_path_unlock(__func__);
		cmdqRecClearEventToken(cmdq_handle,CMDQ_EVENT_DISP_WDMA0_SOF);
		_cmdq_set_config_handle_dirty_mira(cmdq_handle);
		//flush remove memory to cmdq
		_cmdq_flush_config_handle_mira(cmdq_handle, 1);
		DISPMSG("primary capture: Flush remove memout\n");

		dpmgr_path_memout_clock(pgc->dpmgr_handle, 0);
	}
    
out:
    cmdqRecDestroy(cmdq_handle);
    cmdqRecDestroy(cmdq_wait_handle);
    if(mva > 0) 
	    m4u_dealloc_mva(m4uClient,M4U_PORT_DISP_WDMA0,mva);
    
    if(m4uClient != 0)
	    m4u_destroy_client(m4uClient);

	disp_sw_mutex_unlock(&(pgc->capture_lock));
	DISPMSG("primary capture: end\n");

	return ret;
}

int primary_display_capture_framebuffer(unsigned int pbuf)
{
#if 1
	unsigned int fb_layer_id = primary_display_get_option("FB_LAYER");
	unsigned int w_xres = primary_display_get_width();
	unsigned int h_yres = primary_display_get_height();
	unsigned int pixel_bpp = primary_display_get_bpp() / 8; // bpp is either 32 or 16, can not be other value
	unsigned int w_fb = ALIGN_TO(w_xres, MTK_FB_ALIGNMENT);
	unsigned int fbsize = w_fb * h_yres * pixel_bpp; // frame buffer size
	unsigned int fbaddress = dpmgr_path_get_last_config(pgc->dpmgr_handle)->ovl_config[fb_layer_id].addr;
	unsigned int mem_off_x = 0;
	unsigned int mem_off_y = 0;
	unsigned int fbv = 0;
	DISPMSG("w_res=%d, h_yres=%d, pixel_bpp=%d, w_fb=%d, fbsize=%d, fbaddress=0x%08x\n", w_xres, h_yres, pixel_bpp, w_fb, fbsize, fbaddress);
	fbv = (unsigned int)ioremap(fbaddress, fbsize);
	DISPMSG("w_xres = %d, h_yres = %d, w_fb = %d, pixel_bpp = %d, fbsize = %d, fbaddress = 0x%08x\n", w_xres, h_yres, w_fb, pixel_bpp, fbsize, fbaddress);
	if (!fbv)
	{
		DISPMSG("[FB Driver], Unable to allocate memory for frame buffer: address=0x%08x, size=0x%08x\n", \
				fbaddress, fbsize);
		return -1;
	}

	unsigned int i;
	unsigned long ttt = get_current_time_us();
	for(i = 0;i < h_yres; i++)
	{
		//DISPMSG("i=%d, dst=0x%08x,src=%08x\n", i, (pbuf + i * w_xres * pixel_bpp), (fbv + i * w_fb * pixel_bpp));
		memcpy((void *)(pbuf + i * w_xres * pixel_bpp), (void *)(fbv + i * w_fb * pixel_bpp), w_xres * pixel_bpp);
	}
	DISPMSG("capture framebuffer cost %dus\n", get_current_time_us() - ttt);
	iounmap((void *)fbv);
#endif
    return -1;
}


#define ALIGN_TO(x, n)  \
    (((x) + ((n) - 1)) & ~((n) - 1))
UINT32 DISP_GetPanelBPP(void)
{
#if 0
	PANEL_COLOR_FORMAT fmt;
	disp_drv_init_context();
	
	if(disp_if_drv->get_panel_color_format == NULL) 
	{
		return DISP_STATUS_NOT_IMPLEMENTED;
	}

	fmt = disp_if_drv->get_panel_color_format();
	switch(fmt)
	{
		case PANEL_COLOR_FORMAT_RGB332:
			return 8;
		case PANEL_COLOR_FORMAT_RGB444:
			return 12;
		case PANEL_COLOR_FORMAT_RGB565:
			return 16;
		case PANEL_COLOR_FORMAT_RGB666:
			return 18;
		case PANEL_COLOR_FORMAT_RGB888:
			return 24;
		default:
			return 0;
	}
#endif
}

static UINT32 disp_fb_bpp = 32;
static UINT32 disp_fb_pages = 4;    //three for framebuffer, one for dim layer

UINT32 DISP_GetScreenBpp(void)
{
    return disp_fb_bpp; 
}

UINT32 DISP_GetPages(void)
{
    return disp_fb_pages;
}

UINT32 DISP_GetFBRamSize(void)
{
    return ALIGN_TO(DISP_GetScreenWidth(), MTK_FB_ALIGNMENT) * 
           ALIGN_TO(DISP_GetScreenHeight(), MTK_FB_ALIGNMENT) * 
           ((DISP_GetScreenBpp() + 7) >> 3) * 
           DISP_GetPages();
}


UINT32 DISP_GetVRamSize(void)
{
#if 0
    // Use a local static variable to cache the calculated vram size
    //    
    static UINT32 vramSize = 0;
    
    if (0 == vramSize)
    {
        disp_drv_init_context();
        
        ///get framebuffer size
        vramSize = DISP_GetFBRamSize();
        
        ///get DXI working buffer size
        vramSize += disp_if_drv->get_working_buffer_size();
        
        // get assertion layer buffer size
        vramSize += DAL_GetLayerSize();
        
        // Align vramSize to 1MB
        //
        vramSize = ALIGN_TO_POW_OF_2(vramSize, 0x100000);
        
        DISP_LOG("DISP_GetVRamSize: %u bytes\n", vramSize);
    }

    return vramSize;
	    #endif
}
extern char* saved_command_line;

UINT32 __init DISP_GetVRamSizeBoot(char *cmdline)
{
#ifdef CONFIG_OF
	extern vramsize;
	extern void _parse_tag_videolfb(void);
	_parse_tag_videolfb();
	if(vramsize == 0) vramsize = 0x3000000;
	DISPCHECK("[DT]display vram size = 0x%08x|%d\n", vramsize, vramsize);
	return vramsize;
#else
	char *p = NULL;
	UINT32 vramSize = 0;	
	DISPMSG("%s, cmdline=%s\n", __func__, cmdline);
	p = strstr(cmdline, "vram=");
	if(p == NULL)
	{
		vramSize = 0x3000000;
		DISPERR("[FB driver]can not get vram size from lk\n");
	}
	else
	{
		p += 5;
		vramSize = simple_strtol(p, NULL, 10);
		if(0 == vramSize) 
			vramSize = 0x3000000;
	}
	
	DISPCHECK("display vram size = 0x%08x|%d\n", vramSize, vramSize);
    	return vramSize;
#endif
}


struct sg_table table;

int disp_hal_allocate_framebuffer(phys_addr_t pa_start, phys_addr_t pa_end, unsigned int* va, unsigned int* mva)
{
	int ret = 0;
	*va = (unsigned int) ioremap_nocache(pa_start, pa_end - pa_start + 1);
	printk("disphal_allocate_fb, pa=%pa, va=0x%08x\n", &pa_start, *va);

//	if (_get_init_setting("M4U"))
	//xuecheng, m4u not enabled now
	if(1)
	{		
		m4u_client_t *client;
		
                struct sg_table *sg_table = &table;		
		sg_alloc_table(sg_table, 1, GFP_KERNEL);
		
		sg_dma_address(sg_table->sgl) = pa_start;
		sg_dma_len(sg_table->sgl) = (pa_end - pa_start + 1);
		client = m4u_create_client();
		if (IS_ERR_OR_NULL(client))
		{
			DISPMSG("create client fail!\n");
		}
		
		*mva = pa_start & 0xffffffffULL;
		ret = m4u_alloc_mva(client, M4U_PORT_DISP_OVL0, 0, sg_table, (pa_end - pa_start + 1), M4U_PROT_READ |M4U_PROT_WRITE, M4U_FLAGS_FIX_MVA, mva);
		//m4u_alloc_mva(M4U_PORT_DISP_OVL0, pa_start, (pa_end - pa_start + 1), 0, 0, mva);
		if(ret)
		{
			DISPMSG("m4u_alloc_mva returns fail: %d\n", ret);
		}
		printk("[DISPHAL] FB MVA is 0x%08X PA is %pa\n", *mva, &pa_start);
		
	}
	else
	{
		*mva = pa_start & 0xffffffffULL;
	}

	return 0;
}

int primary_display_remap_irq_event_map(void)
{

}

unsigned int primary_display_get_option(const char* option)
{
	if(!strcmp(option, "FB_LAYER"))
		return 0;
	if(!strcmp(option, "ASSERT_LAYER"))
		return 3;
	if(!strcmp(option, "M4U_ENABLE"))
		return 1;
	ASSERT(0);
}

int primary_display_get_debug_info(char *buf)
{
	// resolution
	// cmd/video mode
	// display path
	// dsi data rate/lane number/state
	// primary path trigger count
	// frame done count
	// suspend/resume count
	// current fps 10s/5s/1s
	// error count and message
	// current state of each module on the path
}

#include "ddp_reg.h"

#define IS_READY(x)	((x)?"READY\t":"Not READY")
#define IS_VALID(x)	((x)?"VALID\t":"Not VALID")

#define READY_BIT0(x) ((DISP_REG_GET(DISPSYS_CONFIG_BASE + 0x8b8) & (1 << x)))
#define VALID_BIT0(x) ((DISP_REG_GET(DISPSYS_CONFIG_BASE + 0x8b0) & (1 << x)))

#define READY_BIT1(x) ((DISP_REG_GET(DISPSYS_CONFIG_BASE + 0x8bc) & (1 << x)))
#define VALID_BIT1(x) ((DISP_REG_GET(DISPSYS_CONFIG_BASE + 0x8b4) & (1 << x)))

int primary_display_check_path(char* stringbuf, int buf_len)
{
	int len = 0;
	if(stringbuf)
	{
		len += scnprintf(stringbuf+len, buf_len - len, "|--------------------------------------------------------------------------------------|\n");	

		len += scnprintf(stringbuf+len, buf_len - len, "READY0=0x%08x, READY1=0x%08x, VALID0=0x%08x, VALID1=0x%08x\n",DISP_REG_GET(DISPSYS_CONFIG_BASE + 0x8b8),DISP_REG_GET(DISPSYS_CONFIG_BASE + 0x8bC),DISP_REG_GET(DISPSYS_CONFIG_BASE + 0x8b0),DISP_REG_GET(DISPSYS_CONFIG_BASE + 0x8b4));		
		len += scnprintf(stringbuf+len, buf_len - len, "OVL0\t\t\t%s\t%s\n", 			IS_READY(READY_BIT0(DDP_SIGNAL_OVL0__OVL0_MOUT)), 			IS_VALID(VALID_BIT0(DDP_SIGNAL_OVL0__OVL0_MOUT)));
		len += scnprintf(stringbuf+len, buf_len - len, "OVL0_MOUT:\t\t%s\t%s\n", 	IS_READY(READY_BIT1(DDP_SIGNAL_OVL0_MOUT0__COLOR0_SEL1)), 	IS_VALID(VALID_BIT1(DDP_SIGNAL_OVL0_MOUT0__COLOR0_SEL1)));
		len += scnprintf(stringbuf+len, buf_len - len, "COLOR0_SEL:\t\t%s\t%s\n", 	IS_READY(READY_BIT0(DDP_SIGNAL_COLOR0_SEL__COLOR0)), 		IS_VALID(VALID_BIT0(DDP_SIGNAL_COLOR0_SEL__COLOR0)));
		len += scnprintf(stringbuf+len, buf_len - len, "COLOR0:\t\t\t%s\t%s\n", 		IS_READY(READY_BIT0(DDP_SIGNAL_COLOR0__COLOR0_SOUT)), 		IS_VALID(VALID_BIT0(DDP_SIGNAL_COLOR0__COLOR0_SOUT)));
		len += scnprintf(stringbuf+len, buf_len - len, "COLOR0_SOUT:\t\t%s\t%s\n", 	IS_READY(READY_BIT0(DDP_SIGNAL_COLOR0_SOUT0__AAL_SEL0)), 	IS_VALID(VALID_BIT0(DDP_SIGNAL_COLOR0_SOUT0__AAL_SEL0)));
		len += scnprintf(stringbuf+len, buf_len - len, "AAL_SEL:\t\t%s\t%s\n", 		IS_READY(READY_BIT0(DDP_SIGNAL_AAL_SEL__AAL0)), 				IS_VALID(VALID_BIT0(DDP_SIGNAL_AAL_SEL__AAL0)));
		len += scnprintf(stringbuf+len, buf_len - len, "AAL0:\t\t\t%s\t%s\n", 			IS_READY(READY_BIT0(DDP_SIGNAL_AAL0__OD)), 					IS_VALID(VALID_BIT0(DDP_SIGNAL_AAL0__OD)));
		len += scnprintf(stringbuf+len, buf_len - len, "OD:\t\t\t%s\t%s\n", 			IS_READY(READY_BIT1(DDP_SIGNAL_OD__OD_MOUT)), 				IS_VALID(VALID_BIT1(DDP_SIGNAL_OD__OD_MOUT)));
		len += scnprintf(stringbuf+len, buf_len - len, "OD_MOUT:\t\t%s\t%s\n", 		IS_READY(READY_BIT1(DDP_SIGNAL_OD_MOUT0__RDMA0)), 			IS_VALID(VALID_BIT1(DDP_SIGNAL_OD_MOUT0__RDMA0)));
		len += scnprintf(stringbuf+len, buf_len - len, "RDMA0:\t\t\t%s\t%s\n", 		IS_READY(READY_BIT1(DDP_SIGNAL_RDMA0__RDMA0_SOUT)), 		IS_VALID(VALID_BIT1(DDP_SIGNAL_RDMA0__RDMA0_SOUT)));
		len += scnprintf(stringbuf+len, buf_len - len, "RDMA0_SOUT:\t\t%s\t%s\n", 	IS_READY(READY_BIT1(DDP_SIGNAL_RDMA0_SOUT0__PATH0_SEL0)), 	IS_VALID(VALID_BIT1(DDP_SIGNAL_RDMA0_SOUT0__PATH0_SEL0)));
		len += scnprintf(stringbuf+len, buf_len - len, "PATH0_SEL:\t\t%s\t%s\n", 		IS_READY(READY_BIT0(DDP_SIGNAL_PATH0_SEL__PATH0_SOUT)), 		IS_VALID(VALID_BIT0(DDP_SIGNAL_PATH0_SEL__PATH0_SOUT)));
		len += scnprintf(stringbuf+len, buf_len - len, "PATH0_SOUT:\t\t%s\t%s\n", 	IS_READY(READY_BIT0(DDP_SIGNAL_PATH0_SOUT0__UFOE_SEL0)), 	IS_VALID(VALID_BIT0(DDP_SIGNAL_PATH0_SOUT0__UFOE_SEL0)));
		len += scnprintf(stringbuf+len, buf_len - len, "UFOE:\t\t\t%s\t%s\n", 		IS_READY(READY_BIT0(DDP_SIGNAL_UFOE_SEL__UFOE0)), 			IS_VALID(VALID_BIT0(DDP_SIGNAL_UFOE_SEL__UFOE0)));
		len += scnprintf(stringbuf+len, buf_len - len, "UFOE_MOUT:\t\t%s\t%s\n", 	IS_READY(READY_BIT0(DDP_SIGNAL_UFOE0__UFOE_MOUT)), 			IS_VALID(VALID_BIT0(DDP_SIGNAL_UFOE0__UFOE_MOUT)));
		len += scnprintf(stringbuf+len, buf_len - len, "DSI0_SEL:\t\t%s\t%s\n", 		IS_READY(READY_BIT1(DDP_SIGNAL_UFOE_MOUT0__DSI0_SEL0)), 		IS_VALID(VALID_BIT1(DDP_SIGNAL_UFOE_MOUT0__DSI0_SEL0)));
		len += scnprintf(stringbuf+len, buf_len - len, "DSI0:\t\t\t%s\t%s\n", 			IS_READY(READY_BIT0(DDP_SIGNAL_DSI0_SEL__DSI0)), 				IS_VALID(VALID_BIT0(DDP_SIGNAL_DSI0_SEL__DSI0)));
	}
	else
	{
		DISPMSG("|--------------------------------------------------------------------------------------|\n");	
		
		DISPMSG("READY0=0x%08x, READY1=0x%08x, VALID0=0x%08x, VALID1=0x%08x\n",DISP_REG_GET(DISPSYS_CONFIG_BASE + 0x8b8),DISP_REG_GET(DISPSYS_CONFIG_BASE + 0x8bC),DISP_REG_GET(DISPSYS_CONFIG_BASE + 0x8b0),DISP_REG_GET(DISPSYS_CONFIG_BASE + 0x8b4)); 	
		DISPMSG("OVL0\t\t\t%s\t%s\n",			IS_READY(READY_BIT0(DDP_SIGNAL_OVL0__OVL0_MOUT)),			IS_VALID(VALID_BIT0(DDP_SIGNAL_OVL0__OVL0_MOUT)));
		DISPMSG("OVL0_MOUT:\t\t%s\t%s\n",	IS_READY(READY_BIT1(DDP_SIGNAL_OVL0_MOUT0__COLOR0_SEL1)),	IS_VALID(VALID_BIT1(DDP_SIGNAL_OVL0_MOUT0__COLOR0_SEL1)));
		DISPMSG("COLOR0_SEL:\t\t%s\t%s\n",	IS_READY(READY_BIT0(DDP_SIGNAL_COLOR0_SEL__COLOR0)),		IS_VALID(VALID_BIT0(DDP_SIGNAL_COLOR0_SEL__COLOR0)));
		DISPMSG("COLOR0:\t\t\t%s\t%s\n", 		IS_READY(READY_BIT0(DDP_SIGNAL_COLOR0__COLOR0_SOUT)),		IS_VALID(VALID_BIT0(DDP_SIGNAL_COLOR0__COLOR0_SOUT)));
		DISPMSG("COLOR0_SOUT:\t\t%s\t%s\n",	IS_READY(READY_BIT0(DDP_SIGNAL_COLOR0_SOUT0__AAL_SEL0)),	IS_VALID(VALID_BIT0(DDP_SIGNAL_COLOR0_SOUT0__AAL_SEL0)));
		DISPMSG("AAL_SEL:\t\t%s\t%s\n",		IS_READY(READY_BIT0(DDP_SIGNAL_AAL_SEL__AAL0)), 				IS_VALID(VALID_BIT0(DDP_SIGNAL_AAL_SEL__AAL0)));
		DISPMSG("AAL0:\t\t\t%s\t%s\n",			IS_READY(READY_BIT0(DDP_SIGNAL_AAL0__OD)),					IS_VALID(VALID_BIT0(DDP_SIGNAL_AAL0__OD)));
		DISPMSG("OD:\t\t\t%s\t%s\n", 			IS_READY(READY_BIT1(DDP_SIGNAL_OD__OD_MOUT)),				IS_VALID(VALID_BIT1(DDP_SIGNAL_OD__OD_MOUT)));
		DISPMSG("OD_MOUT:\t\t%s\t%s\n",		IS_READY(READY_BIT1(DDP_SIGNAL_OD_MOUT0__RDMA0)),			IS_VALID(VALID_BIT1(DDP_SIGNAL_OD_MOUT0__RDMA0)));
		DISPMSG("RDMA0:\t\t\t%s\t%s\n",		IS_READY(READY_BIT1(DDP_SIGNAL_RDMA0__RDMA0_SOUT)), 		IS_VALID(VALID_BIT1(DDP_SIGNAL_RDMA0__RDMA0_SOUT)));
		DISPMSG("RDMA0_SOUT:\t\t%s\t%s\n",	IS_READY(READY_BIT1(DDP_SIGNAL_RDMA0_SOUT0__PATH0_SEL0)),	IS_VALID(VALID_BIT1(DDP_SIGNAL_RDMA0_SOUT0__PATH0_SEL0)));
		DISPMSG("PATH0_SEL:\t\t%s\t%s\n",		IS_READY(READY_BIT0(DDP_SIGNAL_PATH0_SEL__PATH0_SOUT)), 		IS_VALID(VALID_BIT0(DDP_SIGNAL_PATH0_SEL__PATH0_SOUT)));
		DISPMSG("PATH0_SOUT:\t\t%s\t%s\n",	IS_READY(READY_BIT0(DDP_SIGNAL_PATH0_SOUT0__UFOE_SEL0)),	IS_VALID(VALID_BIT0(DDP_SIGNAL_PATH0_SOUT0__UFOE_SEL0)));
		DISPMSG("UFOE:\t\t\t%s\t%s\n",		IS_READY(READY_BIT0(DDP_SIGNAL_UFOE_SEL__UFOE0)),			IS_VALID(VALID_BIT0(DDP_SIGNAL_UFOE_SEL__UFOE0)));
		DISPMSG("UFOE_MOUT:\t\t%s\t%s\n",	IS_READY(READY_BIT0(DDP_SIGNAL_UFOE0__UFOE_MOUT)),			IS_VALID(VALID_BIT0(DDP_SIGNAL_UFOE0__UFOE_MOUT)));
		DISPMSG("DSI0_SEL:\t\t%s\t%s\n", 		IS_READY(READY_BIT1(DDP_SIGNAL_UFOE_MOUT0__DSI0_SEL0)), 		IS_VALID(VALID_BIT1(DDP_SIGNAL_UFOE_MOUT0__DSI0_SEL0)));
		DISPMSG("DSI0:\t\t\t%s\t%s\n",			IS_READY(READY_BIT0(DDP_SIGNAL_DSI0_SEL__DSI0)),				IS_VALID(VALID_BIT0(DDP_SIGNAL_DSI0_SEL__DSI0)));
	}

	return len;
}

int primary_display_lcm_ATA()
{
	DISP_STATUS ret = DISP_STATUS_OK;

	DISPFUNC();
	_primary_path_lock(__func__);
	if(pgc->state == 0)
	{
		DISPCHECK("ATA_LCM, primary display path is already sleep, skip\n");
		goto done;
	}
	
	DISPCHECK("[ATA_LCM]primary display path stop[begin]\n");
	if(primary_display_is_video_mode())
	{
		dpmgr_path_ioctl(pgc->dpmgr_handle, NULL, DDP_STOP_VIDEO_MODE, NULL);
	}
	DISPCHECK("[ATA_LCM]primary display path stop[end]\n");
	ret = disp_lcm_ATA(pgc->plcm);
	dpmgr_path_start(pgc->dpmgr_handle, CMDQ_DISABLE);
	if(primary_display_is_video_mode()){
		// for video mode, we need to force trigger here
		// for cmd mode, just set DPREC_EVENT_CMDQ_SET_EVENT_ALLOW when trigger loop start
		dpmgr_path_trigger(pgc->dpmgr_handle, NULL, CMDQ_DISABLE);
	}
done:
	_primary_path_unlock(__func__);
	return ret;
}
int fbconfig_get_esd_check_test(UINT32 dsi_id,UINT32 cmd,UINT8*buffer,UINT32 num)
{
	int ret = 0;
        _primary_path_lock(__func__);

	if(pgc->state == DISP_SLEEPED)
	{
		DISPCHECK("[ESD]primary display path is sleeped?? -- skip esd check\n");
                _primary_path_unlock(__func__);
		goto done;
	}
	/// 1: stop path
	_cmdq_stop_trigger_loop();
	if(dpmgr_path_is_busy(pgc->dpmgr_handle))
	{
		DISPCHECK("[ESD]wait frame done ret:%d\n", ret);
	}
	dpmgr_path_stop(pgc->dpmgr_handle, CMDQ_DISABLE);
	DISPCHECK("[ESD]stop dpmgr path[end]\n");
	if(dpmgr_path_is_busy(pgc->dpmgr_handle))
	{
		DISPCHECK("[ESD]wait frame done ret:%d\n", ret);
	}
	dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);
extern int fbconfig_get_esd_check(DSI_INDEX dsi_id,UINT32 cmd,UINT8*buffer,UINT32 num);
	ret=fbconfig_get_esd_check(dsi_id,cmd,buffer,num);
	dpmgr_path_start(pgc->dpmgr_handle, CMDQ_DISABLE);
	DISPCHECK("[ESD]start dpmgr path[end]\n");
	if(primary_display_is_video_mode())
	{
		// for video mode, we need to force trigger here
		// for cmd mode, just set DPREC_EVENT_CMDQ_SET_EVENT_ALLOW when trigger loop start
		dpmgr_path_trigger(pgc->dpmgr_handle, NULL, CMDQ_DISABLE);
	}
	_cmdq_start_trigger_loop();
	DISPCHECK("[ESD]start cmdq trigger loop[end]\n");
	_primary_path_unlock(__func__);

done:
	return ret;
}
int Panel_Master_dsi_config_entry(const char * name,void *config_value)
{
	int ret = 0;
	int force_trigger_path=0;
    UINT32* config_dsi  = (UINT32 *)config_value;
	DISPFUNC();
	LCM_PARAMS *lcm_param = NULL;
	LCM_DRIVER *pLcm_drv=DISP_GetLcmDrv();
	int	esd_check_backup=atomic_read(&esd_check_task_wakeup);
	if(!strcmp(name, "DRIVER_IC_RESET") || !strcmp(name, "PM_DDIC_CONFIG"))	
	{	
		primary_display_esd_check_enable(0);
		msleep(2500);
	}
	_primary_path_lock(__func__);		

	lcm_param = disp_lcm_get_params(pgc->plcm);
	if(pgc->state == DISP_SLEEPED)
	{
		DISPERR("[Pmaster]Panel_Master: primary display path is sleeped??\n");
		goto done;
	}
	/// Esd Check : Read from lcm
	/// the following code is to
	/// 0: lock path
	/// 1: stop path
	/// 2: do esd check (!!!)
	/// 3: start path
	/// 4: unlock path
	/// 1: stop path
	_cmdq_stop_trigger_loop();

	if(dpmgr_path_is_busy(pgc->dpmgr_handle))
	{
		DISPCHECK("[ESD]wait frame done ret:%d\n", ret);
	}

	dpmgr_path_stop(pgc->dpmgr_handle, CMDQ_DISABLE);
	DISPCHECK("[ESD]stop dpmgr path[end]\n");

	if(dpmgr_path_is_busy(pgc->dpmgr_handle))
	{
		DISPCHECK("[ESD]wait frame done ret:%d\n", ret);
	}
	dpmgr_path_reset(pgc->dpmgr_handle, CMDQ_DISABLE);
	if((!strcmp(name, "PM_CLK"))||(!strcmp(name, "PM_SSC")))
		Panel_Master_primary_display_config_dsi(name,*config_dsi);
	else if(!strcmp(name, "PM_DDIC_CONFIG"))
	{
		Panel_Master_DDIC_config();	
		force_trigger_path=1;
	}else if(!strcmp(name, "DRIVER_IC_RESET"))
	{
		if(pLcm_drv&&pLcm_drv->init_power)
			pLcm_drv->init_power();	
		if(pLcm_drv)
			pLcm_drv->init();		
		else
			ret=-1;	
		force_trigger_path=1;
	}		
	dpmgr_path_start(pgc->dpmgr_handle, CMDQ_DISABLE);	
	if(primary_display_is_video_mode())
	{
		// for video mode, we need to force trigger here
		// for cmd mode, just set DPREC_EVENT_CMDQ_SET_EVENT_ALLOW when trigger loop start
		dpmgr_path_trigger(pgc->dpmgr_handle, NULL, CMDQ_DISABLE);
		force_trigger_path=0;	
	}
	_cmdq_start_trigger_loop();
	DISPCHECK("[Pmaster]start cmdq trigger loop\n");	
done:
	_primary_path_unlock(__func__);

	if(force_trigger_path)		//command mode only
	{
		primary_display_trigger(0,NULL,0);
		DISPCHECK("[Pmaster]force trigger display path\r\n");	
	}
	atomic_set(&esd_check_task_wakeup, esd_check_backup);

	return ret;
}

/*
mode: 0, switch to cmd mode; 1, switch to vdo mode
*/
int primary_display_switch_dst_mode(int mode)
{
	DISP_STATUS ret = DISP_STATUS_ERROR;
#ifdef DISP_SWITCH_DST_MODE
	void* lcm_cmd = NULL;
	DISPFUNC();
	_primary_path_switch_dst_lock();
	disp_sw_mutex_lock(&(pgc->capture_lock));
	if(pgc->plcm->params->type != LCM_TYPE_DSI)
	{
		printk("[primary_display_switch_dst_mode] Error, only support DSI IF\n");
		goto done;
	}
	if(pgc->state == DISP_SLEEPED)
	{
		DISPCHECK("[primary_display_switch_dst_mode], primary display path is already sleep, skip\n");
		goto done;
	}

	if(mode == primary_display_cur_dst_mode){
		DISPCHECK("[primary_display_switch_dst_mode]not need switch,cur_mode:%d, switch_mode:%d\n",primary_display_cur_dst_mode,mode);
		goto done;
	}
//	DISPCHECK("[primary_display_switch_mode]need switch,cur_mode:%d, switch_mode:%d\n",primary_display_cur_dst_mode,mode);
	lcm_cmd = disp_lcm_switch_mode(pgc->plcm,mode);
	if(lcm_cmd == NULL) 
	{
		DISPCHECK("[primary_display_switch_dst_mode]get lcm cmd fail\n",primary_display_cur_dst_mode,mode);
		goto done;
	}
	else
	{
		int temp_mode = 0;
		if(0 != dpmgr_path_ioctl(pgc->dpmgr_handle, pgc->cmdq_handle_config, DDP_SWITCH_LCM_MODE, lcm_cmd))
		{
			printk("switch lcm mode fail, return directly\n");
			goto done;
		}
		_primary_path_lock(__func__);
		temp_mode = (int)(pgc->plcm->params->dsi.mode);
		pgc->plcm->params->dsi.mode = pgc->plcm->params->dsi.switch_mode;
		pgc->plcm->params->dsi.switch_mode = temp_mode;
		dpmgr_path_set_video_mode(pgc->dpmgr_handle, primary_display_is_video_mode());
		if(0 != dpmgr_path_ioctl(pgc->dpmgr_handle, pgc->cmdq_handle_config, DDP_SWITCH_DSI_MODE, lcm_cmd))
		{
			printk("switch dsi mode fail, return directly\n");
			_primary_path_unlock(__func__);
			goto done;
		}
	}
	primary_display_sodi_rule_init();
	_cmdq_stop_trigger_loop();
	_cmdq_build_trigger_loop();
	_cmdq_start_trigger_loop();
	_cmdq_reset_config_handle();//must do this
	_cmdq_insert_wait_frame_done_token();

	primary_display_cur_dst_mode = mode;

	if(primary_display_is_video_mode())
	{
		dpmgr_map_event_to_irq(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC, DDP_IRQ_RDMA0_DONE);
	}
	else
	{
		dpmgr_map_event_to_irq(pgc->dpmgr_handle, DISP_PATH_EVENT_IF_VSYNC, DDP_IRQ_DSI0_EXT_TE);
	}
	_primary_path_unlock(__func__);
	ret = DISP_STATUS_OK;
done:
//	dprec_handle_option(0x0);
	disp_sw_mutex_unlock(&(pgc->capture_lock)); 
	_primary_path_switch_dst_unlock();
#else
	printk("[ERROR: primary_display_switch_dst_mode]this function not enable in disp driver\n");
#endif
	return ret;
}

int primary_display_switch_esd_mode(int mode)
{
    DISPFUNC();
	int ret=0;
	int gpio_mode=0;
	
	if(pgc->plcm->params->dsi.customization_esd_check_enable!=0)
		return;

	DISPMSG("switch esd mode to %d\n", mode);
	
#ifdef GPIO_DSI_TE_PIN
    gpio_mode = mt_get_gpio_mode(GPIO_DSI_TE_PIN);
	//DISPMSG("[ESD]gpio_mode=%d\n",gpio_mode);
#endif
    if(mode==1)
    {
    	//switch to vdo mode
		if(gpio_mode==GPIO_DSI_TE_PIN_M_DSI_TE)
		{
			//if(_need_register_eint())
			{   
				//DISPMSG("[ESD]switch video mode\n");
				int node;
				int irq;
				u32 ints[2]={0, 0};
#ifdef GPIO_DSI_TE_PIN
				// 1.set GPIO107 eint mode	
				mt_set_gpio_mode(GPIO_DSI_TE_PIN, GPIO_DSI_TE_PIN_M_GPIO);
#endif
				// 2.register eint
				node = of_find_compatible_node(NULL, NULL, "mediatek, DSI_TE_1-eint");
				if(node)
				{   
					//DISPMSG("node 0x%x\n", node);
					of_property_read_u32_array(node, "debounce", ints, ARRAY_SIZE(ints));
					mt_gpio_set_debounce(ints[0], ints[1]);
					irq = irq_of_parse_and_map(node, 0);
					if(request_irq(irq, _esd_check_ext_te_irq_handler, IRQF_TRIGGER_NONE, "DSI_TE_1-eint", NULL))
					{
						DISPERR("[ESD]EINT IRQ LINE NOT AVAILABLE!!\n");
					}
				}
				else
				{
					DISPERR("[ESD][%s] can't find DSI_TE_1 eint compatible node\n",__func__);
				}
			}
		}
    }
	else if(mode==0)
	{
		//switch to cmd mode
		if(gpio_mode==GPIO_DSI_TE_PIN_M_GPIO)
		{
		    int node;
			int irq;
			//DISPMSG("[ESD]switch cmd mode\n");
			
			//unregister eint 
			node = of_find_compatible_node(NULL, NULL, "mediatek, DSI_TE_1-eint");
			//DISPMSG("node 0x%x\n", node);
			if(node)
			{	
				irq = irq_of_parse_and_map(node, 0);
		    	free_irq(irq, NULL);
			}	
			// set GPIO107 DSI TE mode	
			mt_set_gpio_mode(GPIO_DSI_TE_PIN, GPIO_DSI_TE_PIN_M_DSI_TE);
		}
	}

	//DISPMSG("primary_display_switch_esd_mode end\n");
	return ret;
}
