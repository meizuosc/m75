#include <linux/delay.h>
#include <linux/fb.h>
#include "mtkfb.h"
#include <asm/uaccess.h>

#include "disp_drv.h"
#include "ddp_hal.h"
#include "disp_drv_platform.h"
#include "disp_drv_log.h"
#include "lcm_drv.h"
#include "debug.h"
//#ifdef MTK_DISP_CONFIG_SUPPORT
#include "fbconfig_kdebug.h"
//#endif

// Fence Sync Object
#if defined (MTK_FB_SYNC_SUPPORT)
#include "disp_sync_svp.h"
#endif
#include "disp_mgr.h"
#include "disp_svp.h"

#include <linux/disp_assert_layer.h>

#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include "mach/mt_clkmgr.h"
#include <linux/vmalloc.h>
#include "mtkfb_info.h"
#include <linux/dma-mapping.h>
#include <linux/rtpm_prio.h>
extern unsigned int lcd_fps;
extern BOOL is_early_suspended;
extern struct semaphore sem_early_suspend;

extern unsigned int EnableVSyncLog;

#define LCM_ESD_CHECK_MAX_COUNT 5

#define ALIGN_TO(x, n)  \
	(((x) + ((n) - 1)) & ~((n) - 1))

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------
unsigned int FB_LAYER = DISP_DEFAULT_UI_LAYER_ID;
static const DISP_IF_DRIVER *disp_if_drv = NULL;
FBCONFIG_DISP_IF * fbconfig_if_drv = NULL;
const LCM_DRIVER  *lcm_drv  = NULL;
extern LCM_PARAMS *lcm_params;

static volatile int direct_link_layer = -1;
static UINT32 disp_fb_bpp = 32;     ///ARGB8888
static UINT32 disp_fb_pages = 3;     ///double buffer

BOOL is_engine_in_suspend_mode = FALSE;
BOOL is_lcm_in_suspend_mode    = FALSE;

static unsigned long u4IndexOfLCMList = 0;

#ifdef MTK_LCD_HW_3D_SUPPORT
// S3D mode
extern wait_queue_head_t gS3DModeWaitQueue;
extern int gS3DModeIsOn;
extern struct mutex gS3DModeMutex;
#endif

static unsigned int dispMergeTriggerMode = NONE_MODE;
static wait_queue_head_t config_update_wq;
static struct task_struct *config_update_task = NULL;
static int config_update_task_wakeup = 0;

extern unsigned int PanDispSettingPending;
extern unsigned int PanDispSettingDirty;
extern unsigned int PanDispSettingApplied;

extern bool is_ipoh_bootup;

extern wait_queue_head_t reg_update_wq;
static wait_queue_head_t vsync_wq;
static bool vsync_wq_flag = false;

static struct hrtimer cmd_mode_update_timer;
static ktime_t cmd_mode_update_timer_period;

static bool needStartEngine = true;

extern unsigned int need_esd_check;
extern wait_queue_head_t esd_check_wq;
extern BOOL esd_kthread_pause;

DEFINE_MUTEX(MemOutSettingMutex);
static struct disp_path_config_mem_out_struct MemOutConfig;
static BOOL is_immediateupdate = false;

DEFINE_MUTEX(LcmCmdMutex);
DEFINE_MUTEX(Fbconfig_Switch_Mode_Mutex);
DEFINE_MUTEX(UpdateRegMutex);
DEFINE_MUTEX(SwitchModeMutex);
DEFINE_SPINLOCK(DispConfigLock);	// To protect @disp_config
unsigned int disp_running = 0;
disp_session_config disp_config = {
		.mode = DISP_SESSION_DECOUPLE_MODE,
		.type = DISP_SESSION_PRIMARY,
		.device_id = 0,
		.session_id = MAKE_DISP_SESSION(DISP_SESSION_DECOUPLE_MODE, DISP_SESSION_PRIMARY, 0)
};

DECLARE_WAIT_QUEUE_HEAD(disp_done_wq);
/**
 * NOTICE:
 * DO NOT use this for Multiple Display(SVP), just reserved for compatibility
 */
OVL_CONFIG_STRUCT cached_layer_config[DDP_OVL_LAYER_MUN];

static OVL_CONFIG_STRUCT _layer_config[2][DDP_OVL_LAYER_MUN];
OVL_CONFIG_STRUCT* captured_layer_config = _layer_config[0];
OVL_CONFIG_STRUCT* realtime_layer_config = _layer_config[0];

unsigned int gCaptureLayerRawData = 0;
unsigned int gCaptureLayerEnable = 0;
unsigned int gCaptureLayerDownX = 10;
unsigned int gCaptureLayerDownY = 10;

unsigned int gCaptureOvlRawData = 0;
unsigned int gCaptureOvlEnable = 0;
unsigned int gCaptureOvlDownX = 10;
unsigned int gCaptureOvlDownY = 10;

unsigned int gCaptureFBRawData = 0;
unsigned int gCaptureFBEnable = 0;
unsigned int gCaptureFBDownX = 10;
unsigned int gCaptureFBDownY = 10;

extern struct fb_info *mtkfb_fbi;

unsigned int is_video_mode_running = 0;

DEFINE_SEMAPHORE(sem_update_screen);//linux 3.0 porting
// whether LCM Driver/Param is found
static BOOL isLCMFound 					= FALSE;
// whether lcm is connected
static BOOL isLCMConnected 				= FALSE;

/// Some utilities
#define ALIGN_TO_POW_OF_2(x, n)  \
                (((x) + ((n) - 1)) & ~((n) - 1))

#define ALIGN_TO(x, n)  \
                (((x) + ((n) - 1)) & ~((n) - 1))



static size_t disp_log_on = false;
#define DISP_LOG(fmt, arg...) \
    do { \
        if (disp_log_on) DISP_LOG_PRINT(ANDROID_LOG_WARN, "COMMON", fmt, ##arg); \
    }while (0)

#define DISP_FUNC()	\
    do { \
        if(disp_log_on) DISP_LOG_PRINT(ANDROID_LOG_INFO, "COMMON", "[Func]%s\n", __func__); \
    }while (0)

#define DISP_LOG_D(fmt, arg...) \
	do { \
	   DISP_LOG_PRINT(ANDROID_LOG_DEBUG, "COMMON", fmt, ##arg); \
	}while (0)

void disp_log_enable(int enable)
{
    disp_log_on = enable;
    DISP_LOG("disp common log %s\n", enable?"enabled":"disabled");
}
// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

extern LCM_DRIVER* lcm_driver_list[];
extern unsigned int lcm_count;

typedef struct disp_path_config_dirty_t {
	unsigned int ovl_dirty;
	unsigned int aal_dirty;
	unsigned int mem_out_dirty;
}disp_path_config_dirty;

#define DISP_CB_MAXCNT 2
typedef struct {
    DISP_EXTRA_CHECKUPDATE_PTR checkupdate_cb[DISP_CB_MAXCNT];
    DISP_EXTRA_CONFIG_PTR config_cb[DISP_CB_MAXCNT];
} CONFIG_CB_ARRAY;
CONFIG_CB_ARRAY g_CB_Array = { {NULL , NULL},{NULL , NULL}};//if DISP_CB_MAXCNT == 2


void GetUpdateMutex(void)
{
    mutex_lock(&UpdateRegMutex);
}
int TryGetUpdateMutex(void)
{
    return mutex_trylock(&UpdateRegMutex);
}
void ReleaseUpdateMutex(void)
{
    mutex_unlock(&UpdateRegMutex);
}

// Fence Sync Object
#if defined (MTK_FB_SYNC_SUPPORT)
static DECLARE_WAIT_QUEUE_HEAD(clean_up_wq);
static int clean_up_task_wakeup = 0;
struct task_struct *clean_up_task = NULL;
extern BOOL isAEEEnabled;

static int _DISP_CleanUpKThread(void *data) {
	int i = 0;
	disp_job *job;
	//struct sched_param param = { .sched_priority = RTPM_PRIO_SCRN_UPDATE };
	//sched_setscheduler(current, SCHED_RR, &param);
	while(1) {
		wait_event_interruptible(clean_up_wq, clean_up_task_wakeup);
		clean_up_task_wakeup = 0;

		while ((job = disp_recycle_job(false)) != NULL) {
			for (i = 0; i < MAX_INPUT_CONFIG; i++) {
				if (isAEEEnabled && job->input[i].layer_id == DISP_DEFAULT_UI_LAYER_ID) {
					disp_sync_release(job->group_id, DISP_DEFAULT_UI_LAYER_ID);
				}
				if (job->input[i].dirty) {
					disp_sync_signal_fence(job->group_id, job->input[i].layer_id);
					disp_sync_release_buffer(job->group_id, job->input[i].layer_id);
				}
			}
			//output layerId = MAX_INPUT_CONFIG
			if (job->output.dirty) {
				disp_sync_signal_fence(job->group_id, job->output.layer_id);
				disp_sync_release_buffer(job->group_id, job->output.layer_id);
			}
		}
	}
	return 0;
}
#endif

#if defined(MTK_OVL_DECOUPLE_SUPPORT)
#define MAX_BUFFER_COUNT 		3
#define BPP				 		3
// Main display buffer queue: normal/secure
static disp_buffer_queue normal_buffer_queue;
static disp_buffer_queue* buffer_queue = &normal_buffer_queue;

static BOOL write_mem_running = 0;
static DECLARE_WAIT_QUEUE_HEAD(write_mem_idle_wq);
static BOOL read_mem_running = 0;
static DECLARE_WAIT_QUEUE_HEAD(read_mem_idle_wq);

static DECLARE_WAIT_QUEUE_HEAD(decouple_transfer_wq);
static int decouple_write_task_wakeup = 0;
static struct task_struct *decouple_worker_task = NULL;

extern unsigned int decouple_addr;
DISP_STATUS _DISP_ConfigUpdateScreen(UINT32 x, UINT32 y, UINT32 width, UINT32 height);
static disp_job* _DISP_MergeOVLDirty(disp_path_config_dirty *dirty_flag);
static void _DISP_DumpLayer(OVL_CONFIG_STRUCT* pLayer);
static void _DISP_DumpFrameBuffer(void);
static void _DISP_DumpOverlay(void);
/**
 * eRGB888 triple buffer for 0VL->WDMA->MEM, MEM->RDMA->LCM
 */
UINT32 DISP_GetOVLRamSize(void) {
	return ALIGN_TO(DISP_GetScreenWidth(), disphal_get_fb_alignment()) *
	           ALIGN_TO(DISP_GetScreenHeight(), disphal_get_fb_alignment()) *
	           MAX_BUFFER_COUNT * BPP;
}

static void _DISP_InitHWConfig (disp_job *job, OVL_CONFIG_STRUCT *pOVLConfig,
		WDMA_CONFIG_STRUCT *pWDMAConfig) {
	int idx;
	if (job == NULL) {
		printk("[FB Driver] error: _DISP_InitHWConfig() invalid param!\n");
		return;
	}
	mutex_lock(&job->lock);
	if (pOVLConfig != NULL && job != NULL) {
		idx = pOVLConfig->layer;
		pOVLConfig->layer = job->input[idx].layer_id;
		pOVLConfig->layer_en = job->input[idx].layer_enable;
		pOVLConfig->addr = job->input[idx].address;
		pOVLConfig->buff_idx = job->input[idx].index;
		pOVLConfig->isDirty = job->input[idx].dirty;

		pOVLConfig->src_x = job->input[idx].src_x;
		pOVLConfig->src_y = job->input[idx].src_y;
		pOVLConfig->dst_x = job->input[idx].dst_x;
		pOVLConfig->dst_y = job->input[idx].dst_y;
		pOVLConfig->src_w = job->input[idx].width;
		pOVLConfig->src_h = job->input[idx].height;
		pOVLConfig->src_pitch = job->input[idx].pitch;
		pOVLConfig->dst_h = job->input[idx].height;
		pOVLConfig->dst_w = job->input[idx].width;
		pOVLConfig->fmt = job->input[idx].format;
		pOVLConfig->aen = job->input[idx].alpha_enable;
		pOVLConfig->alpha = job->input[idx].alpha;
		pOVLConfig->key = job->input[idx].color_key;
		pOVLConfig->keyEn = job->input[idx].color_key_enable;
		pOVLConfig->security = job->input[idx].security;
		pOVLConfig->source = OVL_LAYER_SOURCE_MEM;
		pOVLConfig->isTdshp = 0;			// NO USE now
		pOVLConfig->identity = 0;			// NO USE now
		pOVLConfig->vaddr = 0;				// NO USE now
		pOVLConfig->connected_type = 0;		// NO USE now
	}
	if (pWDMAConfig != NULL) {
		pWDMAConfig->idx = 0;
		pWDMAConfig->clipHeight = job->output.height;
		pWDMAConfig->clipWidth = job->output.width;
		pWDMAConfig->clipX = job->output.x;
		pWDMAConfig->clipY = job->output.y;
		pWDMAConfig->dstAddress = job->output.address;
		pWDMAConfig->dstWidth = job->output.width;
		//pWDMAConfig->dstHeight = job->output.height;	// NO USE now
		pWDMAConfig->dstPitch = job->output.pitch;
		pWDMAConfig->dstPitchUV = job->output.pitchUV;
		pWDMAConfig->inputFormat = WDMA_INPUT_FORMAT_ARGB;
		pWDMAConfig->outputFormat = job->output.format;
		pWDMAConfig->srcHeight = job->output.height;
		pWDMAConfig->srcWidth = job->output.width;
		pWDMAConfig->security = job->output.security;
	}
	mutex_unlock(&job->lock);
}

extern unsigned int handles[3];
extern unsigned int decouple_buffer_handles[3];
static void _DISP_ConfigMemWriteDatapath (disp_job *job) {
	UINT i;
	UINT roiW = DISP_GetScreenWidth();
	UINT roiH = DISP_GetScreenHeight();
	OVL_CONFIG_STRUCT ovl_config;
	WDMA_CONFIG_STRUCT wdma_config;
	BOOL secure_buffer = 0;
#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT) && defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
    static unsigned int executed = 0;
    unsigned int handles[MAX_BUFFER_COUNT];
#endif
	// Apply configuration here.
	disp_path_get_mem_write_mutex();

	memset(&ovl_config, 0, sizeof(ovl_config));
	memset(&wdma_config, 0, sizeof(wdma_config));

	MMProfileLog(MTKFB_MMP_Events.ConfigOVL, MMProfileFlagStart);
	for (i=0; i<DDP_OVL_LAYER_MUN; i++) {
		if (job->input[i].dirty) {
			ovl_config.layer = i;	// this is array index, maybe not the real layer id now
			_DISP_InitHWConfig(job, &ovl_config, NULL);
			secure_buffer |= ovl_config.security;
			MMProfileLogStructure(MTKFB_MMP_Events.ConfigOVL, MMProfileFlagPulse, &ovl_config, OVL_CONFIG_STRUCT);
			if (ovl_config.layer_en && ovl_config.addr == 0) {
				MMProfileLogEx(MTKFB_MMP_Events.Debug, MMProfileFlagPulse, ovl_config.layer, ovl_config.buff_idx);
				AEE_WARNING("[DISP/Common]", "L%d,%d of 0x%08x invalid addr!\n",
						ovl_config.layer, ovl_config.buff_idx, job->group_id);
			} else {
				_DISP_DumpLayer(&ovl_config);
				disp_path_config_layer(&ovl_config);
			}
		}
	}
	MMProfileLog(MTKFB_MMP_Events.ConfigOVL, MMProfileFlagEnd);
#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT) && defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
    if (secure_buffer && executed == 0)
    {
        printk("[FB Driver] query secure handles for primary display \n");
	    disp_path_query_primary_handle(DISP_GetScreenWidth(), DISP_GetScreenHeight(), BPP, handles, MAX_BUFFER_COUNT);
	    executed = 1;
    }
	/*if (secure_buffer) {
	    printk("[FB Driver] with secure buffer! \n");
		buffer_queue = &secure_buffer_queue;
		if (buffer_queue->buffer_count == 0) {
		    //unsigned int handles[MAX_BUFFER_COUNT];
		    //disp_path_query_primary_handle(DISP_GetScreenWidth(), DISP_GetScreenHeight(), BPP, &handles, MAX_BUFFER_COUNT);

		    //disp_buffer_queue_init(&secure_buffer_queue, handles, MAX_BUFFER_COUNT);
		}
	} else {*/
		buffer_queue = &normal_buffer_queue;
	//}
#endif
	MMProfileLog(MTKFB_MMP_Events.ConfigWDMA, MMProfileFlagStart);
	if (job->output.dirty) {
		_DISP_InitHWConfig(job, NULL, &wdma_config);
		roiW = wdma_config.srcWidth;
		roiH = wdma_config.srcHeight;
	} else {
		// No Fence here
		wdma_config.idx = 0;
		wdma_config.srcWidth = wdma_config.dstWidth = wdma_config.clipWidth = DISP_GetScreenWidth();
		wdma_config.srcHeight = wdma_config.clipHeight = DISP_GetScreenHeight();
		wdma_config.dstPitch = wdma_config.dstWidth * BPP;
		wdma_config.inputFormat = WDMA_INPUT_FORMAT_ARGB;
		wdma_config.outputFormat = eRGB888;
		wdma_config.dstAddress = disp_deque_buffer(buffer_queue);
		wdma_config.useSpecifiedAlpha = 1;
		wdma_config.alpha = 0;
	}
	disp_path_config_wdma2(&wdma_config);
	MMProfileLogStructure(MTKFB_MMP_Events.ConfigWDMA, MMProfileFlagEnd, &wdma_config, WDMA_CONFIG_STRUCT);
	// refine OVL ROI if needed (for multiple display)
	disp_path_config_ovl_roi(roiW, roiH);

	disp_path_release_mem_write_mutex();
}

static void _DISP_WaitMemWriteDone (void) {
	int i;
	disp_job *job = NULL;
	//1. waiting for HW frame done
	disp_path_wait_frame_done();
    //Must before calling Swap Buffer
    _DISP_DumpOverlay();
    _DISP_DumpFrameBuffer();
	//2. swap buffer queue
	if (disp_config.type == DISP_SESSION_PRIMARY) {
		disp_enque_buffer(buffer_queue);
	}

	wake_up_interruptible(&write_mem_idle_wq);

	//3. release job queue
	disp_release_job();

	//4. update Fence info
	while ((job = disp_query_job()) != NULL) {
		for (i = 0; i < MAX_INPUT_CONFIG; i++) {
			if (job->input[i].dirty) {
				disp_sync_inc_timeline(job->group_id, job->input[i].layer_id, job->input[i].index);
			}
		}
		//output layerId = MAX_INPUT_CONFIG
		if (job->output.dirty) {
			disp_sync_inc_timeline(job->group_id, job->output.layer_id, job->output.index);
		}
	}
	//5. misc
	clean_up_task_wakeup = 1;
	wake_up_interruptible(&clean_up_wq);
	if ((PanDispSettingPending==1) && (PanDispSettingDirty==0)) {
		PanDispSettingApplied = 1;
		PanDispSettingPending = 0;
	}
	wake_up(&reg_update_wq);
	MMProfileLog(MTKFB_MMP_Events.TrigOverlayOut, MMProfileFlagEnd);
}

static int _DISP_DecoupleWorkerKThread(void *data) {
    //int i;
    //int dirty;
    //unsigned long long flag;
    disp_job *job = NULL;
    int suspended = 0;
    unsigned int trigger_cnt = 0;
    struct sched_param param = { .sched_priority = RTPM_PRIO_SCRN_UPDATE };
    sched_setscheduler(current, SCHED_RR, &param);
    disp_buffer_queue_init_continous(&normal_buffer_queue, decouple_addr, DISP_GetOVLRamSize(), MAX_BUFFER_COUNT);

	while(1) {
		wait_event_interruptible(decouple_transfer_wq, decouple_write_task_wakeup);
		decouple_write_task_wakeup = 0;
#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT) && defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
        disp_path_update_decouple_secure_port();
#endif
        while ((job = disp_acquire_job()) != NULL) {
        	MMProfileLogEx(MTKFB_MMP_Events.TrigOverlayOut, MMProfileFlagStart, job->group_id, (unsigned int)job);
        	//1. update display configuration settings
        	spin_lock(&DispConfigLock);
        	disp_config.session_id = job->group_id;
        	disp_config.mode = DISP_SESSION_MODE(job->group_id);
        	disp_config.type = DISP_SESSION_TYPE(job->group_id);
        	disp_config.device_id = DISP_SESSION_DEV(job->group_id);
        	spin_unlock(&DispConfigLock);
        	//2. trigger HW
        	write_mem_running = 1;
        	PanDispSettingDirty = 0;
        	if (is_early_suspended) {
        	    if (down_interruptible(&sem_early_suspend)) {
        	        printk("[FB Driver] can't get semaphore:line%d\n", __LINE__);
        	        continue;
        	    }
        	    if (is_early_suspended) {
        	    	suspended = 1;
        	    	trigger_cnt++;
        	    	DISP_LOG_D("In early_suspend, trigger OVL-WDMA to work cnt=%d\n", trigger_cnt);
        	    	// Turn on engine clock.
        	    	disp_path_clock_on("mtkfb"); // need to restore registers
        	    } else {
        	    	up(&sem_early_suspend);
        	    }
        	} else {
        		trigger_cnt = 0;
        	}
        	_DISP_ConfigMemWriteDatapath(job);
        	//3. waiting for HW frame done
        	_DISP_WaitMemWriteDone();
        	if (suspended) {
        		suspended = 0;
        		// Turn off engine clock.
        		disp_path_clock_off("no-backup");// Do not backup registers
        		up(&sem_early_suspend);
        	}
        	write_mem_running = 0;
        }
	}
	return 0;
}
#endif //#if defined(MTK_OVL_DECOUPLE_SUPPORT)

static void _DISP_ConfigMemReadDatapath (disp_path_config_dirty *dirty_flag) {
#if defined(MTK_OVL_DECOUPLE_SUPPORT)
	int i;
	RDMA_CONFIG_STRUCT config;
	memset(&config, 0, sizeof(config));

	config.idx = 0;
	config.mode = RDMA_MODE_MEMORY;
	config.inputFormat = eRGB888;
	config.width = DISP_GetScreenWidth();
	config.height = DISP_GetScreenHeight();
	config.pitch = BPP * DISP_GetScreenWidth();
	config.outputFormat = RDMA_OUTPUT_FORMAT_ARGB;
	config.address = disp_request_buffer(buffer_queue);
	
	disp_path_get_mutex();
	disp_path_config_rdma(&config);

	if (dirty_flag->aal_dirty) {
		MMProfileLog(MTKFB_MMP_Events.ConfigAAL, MMProfileFlagStart);
		//GetUpdateMutex();
		if(1 == TryGetUpdateMutex()) {
			for(i = 0 ; i < DISP_CB_MAXCNT ; i += 1) {
				if((NULL != g_CB_Array.checkupdate_cb[i]) && g_CB_Array.checkupdate_cb[i](dirty_flag->aal_dirty)) {
					g_CB_Array.config_cb[i](dirty_flag->aal_dirty);
				}
			}
			ReleaseUpdateMutex();
		}
		MMProfileLog(MTKFB_MMP_Events.ConfigAAL, MMProfileFlagEnd);
	}
	if ((lcm_params->type == LCM_TYPE_DBI) ||
			((lcm_params->type == LCM_TYPE_DSI) && (lcm_params->dsi.mode == CMD_MODE))){
		DISP_STATUS ret = _DISP_ConfigUpdateScreen(0, 0, DISP_GetScreenWidth(), DISP_GetScreenHeight());
		if ((ret != DISP_STATUS_OK) && (is_early_suspended == 0))
			hrtimer_start(&cmd_mode_update_timer, cmd_mode_update_timer_period, HRTIMER_MODE_REL);
#ifndef MTK_FB_START_DSI_ISR
		disp_path_release_mutex();
#endif
	} else {
		disp_path_release_mutex();
	}

#endif //#if defined(MTK_OVL_DECOUPLE_SUPPORT)
}

///=================================================================
// OVL De-couple interface @{
///===========================
BOOL DISP_IsDecoupleMode(void) {
	BOOL ret = FALSE;
#if defined(MTK_OVL_DECOUPLE_SUPPORT)
	//spin_lock(&DispConfigLock);
	//ret = (disp_config.mode == DISP_DECOUPLE_MODE);
	ret = TRUE;
	//spin_unlock(&DispConfigLock);
#endif
	return ret;
}

void DISP_StartOverlayTransfer (void) {
#if defined(MTK_OVL_DECOUPLE_SUPPORT)
	decouple_write_task_wakeup = 1;
	wake_up_interruptible(&decouple_transfer_wq);
#endif
}

void DISP_WaitMemWriteDoneIfNeeded (void) {
#if defined(MTK_OVL_DECOUPLE_SUPPORT)
	if (DISP_IsDecoupleMode()) {
		if (wait_event_interruptible_timeout(write_mem_idle_wq, !write_mem_running, HZ/10) == 0) {
			printk("[FB Driver] wait write mem done timeout!\n");
		}
	}
#endif
}

DISP_STATUS DISP_SwitchDisplayMode (struct fb_overlay_mode *pConfig) {
	int ret = DISP_STATUS_ERROR;
#if 0//defined(MTK_OVL_DECOUPLE_SUPPORT)
	unsigned long long flag;
	if (mode == disp_mode) {
		return DISP_STATUS_OK;
	}
    if (!is_early_suspended) {
    	printk("[FB Driver] Switch to display mode: %s\n", (pConfig->mode == DISP_DECOUPLE_MODE) ? "de-couple" : "direct-link");
    	MMProfileLogEx(MTKFB_MMP_Events.SwitchMode, MMProfileFlagStart, disp_mode, pConfig->mode);
    	/**
    	 * 1. If direct-link --> decouple, push the last frame buffer to be read in decouple
    	 *    If de-couple --> direct-link, waiting for write last frame done
    	 */
    	if (disp_mode == DISP_DIRECT_LINK_MODE) {
    		printk("[FB Driver] Switch display mode-0_1!\n");
    		MMProfileLogEx(MTKFB_MMP_Events.SwitchMode, MMProfileFlagPulse, 0, 1);

    		disp_path_clear_mem_out_done_flag();  // clear last time mem_out_done flag
    		mutex_lock(&MemOutSettingMutex);
    		MemOutConfig.dirty = 1;
    		MemOutConfig.outFormat = eRGB888;
    		MemOutConfig.enable = 1;
    		MemOutConfig.dstAddr = disp_deque_buffer(buffer_queue);
    		MemOutConfig.srcROI.src_x = 0;
    		MemOutConfig.srcROI.src_y = 0;
    		MemOutConfig.srcROI.height= DISP_GetScreenHeight();
    		MemOutConfig.srcROI.width= DISP_GetScreenWidth();
    		mutex_unlock(&MemOutSettingMutex);

    		// Wait for mem out done.
    		disp_path_wait_mem_out_done();

    		printk("[FB Driver] Switch display mode-0_2!\n");
    		MMProfileLogEx(MTKFB_MMP_Events.SwitchMode, MMProfileFlagPulse, 0, 2);

    		mutex_lock(&MemOutSettingMutex);
    		MemOutConfig.dirty = 1;
    		MemOutConfig.enable = 0;
    		mutex_unlock(&MemOutSettingMutex);
    		wait_event_interruptible(reg_update_wq, !MemOutConfig.dirty);

    		printk("[FB Driver] Switch display mode-0_3!\n");
    		MMProfileLogEx(MTKFB_MMP_Events.SwitchMode, MMProfileFlagPulse, 0, 3);

    	} else {
    		wait_event_interruptible_timeout(write_mem_idle_wq, !write_mem_running, HZ/10);
    		printk("[FB Driver] Switch display mode-1_0!\n");
    		MMProfileLogEx(MTKFB_MMP_Events.SwitchMode, MMProfileFlagPulse, 1, 0);
    	}
    	/**
    	 * 2. Waiting for last frame screen update done
    	 */
    	mutex_lock(&SwitchModeMutex);
    	printk("[FB Driver] Switch display mode-2_0!\n");
    	MMProfileLogEx(MTKFB_MMP_Events.SwitchMode, MMProfileFlagPulse, 2, 0);
    	/*if (!read_mem_running) {
    		msleep(10);
    	}*/
    	wait_event_interruptible_timeout(read_mem_idle_wq, !read_mem_running, HZ/10);

    	printk("[FB Driver] Switch display mode-2_1!\n");
    	MMProfileLogEx(MTKFB_MMP_Events.SwitchMode, MMProfileFlagPulse, 2, 1);
    	/**
    	 * 3. Switch display mode
    	 */
    	disp_mode = mode;

    	//disp_path_get_mutex();

    	struct disp_path_config_ovl_mode_t config;
    	memset(&config, 0, sizeof(config));
    	config.mode = pConfig->mode;
    	config.address = (pConfig->mode == DISP_DECOUPLE_MODE) ? disp_request_buffer(buffer_queue) : 0;
    	config.format = eRGB888;
    	config.roi.width = DISP_GetScreenWidth();
    	config.roi.height = DISP_GetScreenHeight();
    	config.pitch = DISP_GetScreenWidth() * BPP;
    	disp_path_switch_ovl_mode(&config);

    	//disp_path_release_mutex();

    	mutex_unlock(&SwitchModeMutex);
    	// trigger ovl transfer
    	if (disp_mode == DISP_DECOUPLE_MODE) {
    		DISP_StartOverlayTransfer();
    	}
    	printk("[FB Driver] Switch display mode done!\n");
    	MMProfileLogEx(MTKFB_MMP_Events.SwitchMode, MMProfileFlagEnd, disp_mode, pConfig->mode);
    }
	ret = DISP_STATUS_OK;
#endif
	return ret;
}
///OVL de-couple @}

static void disp_dump_lcm_parameters(LCM_PARAMS *lcm_params)
{
    unsigned char *LCM_TYPE_NAME[] = {"DBI", "DPI", "DSI"};
    unsigned char *LCM_CTRL_NAME[] = {"NONE", "SERIAL", "PARALLEL", "GPIO"};
    
    if(lcm_params == NULL)
        return;
    
    printk("[mtkfb] LCM TYPE: %s\n", LCM_TYPE_NAME[lcm_params->type]);
    printk("[mtkfb] LCM INTERFACE: %s\n", LCM_CTRL_NAME[lcm_params->ctrl]);
    printk("[mtkfb] LCM resolution: %d x %d\n", lcm_params->width, lcm_params->height);
    
    return;
}

char disp_lcm_name[256] = {0};
BOOL disp_get_lcm_name_boot(char *cmdline)
{
    BOOL ret = FALSE;
    char *p, *q;
    
    p = strstr(cmdline, "lcm=");
    if(p == NULL)
    {
        // we can't find lcm string in the command line, 
        // the uboot should be old version, or the kernel is loaded by ICE debugger
        return DISP_SelectDeviceBoot(NULL);
    }
    
    p += 4;
    if((p - cmdline) > strlen(cmdline+1))
    {
        ret = FALSE;
        goto done;
    }
    
    isLCMConnected  = strncmp(p, "0", 1);
    printk("[mtkfb] LCM is %sconnected\n", ((isLCMConnected)?"":"not "));
    p += 2;
    q = p;
    while(*q != ' ' && *q != '\0')
        q++;
    
    if(q != p) isLCMFound = 1;
    memset((void*)disp_lcm_name, 0, sizeof(disp_lcm_name));
    strncpy((char*)disp_lcm_name, (const char*)p, (int)(q-p));
    
    if(DISP_SelectDeviceBoot(disp_lcm_name))
        ret = TRUE;
    
done:
    return ret;
}
    
static BOOL disp_drv_init_context(void)
{
    if (disp_if_drv != NULL && lcm_drv != NULL){
        return TRUE;
    }
    
    if(!isLCMFound)
        DISP_DetectDevice();
    
    disphal_init_ctrl_if();

    disp_if_drv = disphal_get_if_driver();

    if (!disp_if_drv) return FALSE;
    
    return TRUE;
}

BOOL DISP_IsLcmFound(void)
{
    return isLCMConnected;
}

BOOL DISP_IsContextInited(void)
{
    if(lcm_params && disp_if_drv && lcm_drv)
        return TRUE;
    else
        return FALSE;
}

BOOL DISP_SelectDeviceBoot(const char* lcm_name)
{
	//LCM_DRIVER *lcm = NULL;
    
    printk("%s\n", __func__);

    if(lcm_name == NULL)
    {
        // we can't do anything in boot stage if lcm_name is NULL
        return false;
    }
    lcm_drv = disphal_get_lcm_driver(lcm_name, (unsigned int*)&u4IndexOfLCMList);

    if (NULL == lcm_drv)
    {
        printk("%s, get_lcm_driver returns NULL\n", __func__);
        return FALSE;
    }
    isLCMFound = TRUE;

    disp_if_drv = disphal_get_if_driver();
    
    disp_dump_lcm_parameters(lcm_params);
    return TRUE;
}

BOOL DISP_SelectDevice(const char* lcm_name)
{
    lcm_drv = disphal_get_lcm_driver(lcm_name, (unsigned int *)&u4IndexOfLCMList);
    if (NULL == lcm_drv)
    {
        printk("%s, disphal_get_lcm_driver() returns NULL\n", __func__);
        return FALSE;
    }
    isLCMFound = TRUE;

    disp_dump_lcm_parameters(lcm_params);
    return disp_drv_init_context();
}

BOOL DISP_DetectDevice(void)
{
    lcm_drv = disphal_get_lcm_driver(NULL, (unsigned int *)&u4IndexOfLCMList);
    if (NULL == lcm_drv)
    {
        printk("%s, disphal_get_lcm_driver() returns NULL\n", __func__);
        return FALSE;
    }
    isLCMFound = TRUE;
    
    disp_dump_lcm_parameters(lcm_params);
    
    return TRUE;
}

// ---------------------------------------------------------------------------
//  DISP Driver Implementations
// ---------------------------------------------------------------------------
extern mtk_dispif_info_t dispif_info[MTKFB_MAX_DISPLAY_COUNT];

DISP_STATUS DISP_Init(UINT32 fbVA, UINT32 fbPA, BOOL isLcmInited)
{
    DISP_STATUS r = DISP_STATUS_OK;
    if (!disp_drv_init_context()) {
        return DISP_STATUS_NOT_IMPLEMENTED;
    }

    disphal_init_ctrl_if();
	#ifndef MTKFB_FPGA_ONLY
    disp_path_clock_on("mtkfb");
    #endif

    //tmp solution, just for KK.p49 sanity fail
    #if defined(MTK_OVL_DECOUPLE_SUPPORT)
    if (lcm_params->type == LCM_TYPE_DPI)
        disp_config.mode = DISP_DIRECT_LINK_MODE;
    #endif
    //End

	//This is for Display Customizaiton Tool to get driver interface.....	  
	fbconfig_if_drv =(FBCONFIG_DISP_IF *) disphal_fbconfig_get_def_if();
 
    r = (disp_if_drv->init) ?
        (disp_if_drv->init(fbVA, fbPA, isLcmInited)) :
          DISP_STATUS_NOT_IMPLEMENTED;
	msleep(50);
#ifndef MTKFB_FPGA_ONLY
   DISP_InitVSYNC((100000000/lcd_fps) + 1);//us
#endif

    {
        DAL_STATUS ret;

        /// DAL init here
        fbVA += DISP_GetFBRamSize();
        fbPA += DISP_GetFBRamSize();
        ret = DAL_Init(fbVA, fbPA);
        ASSERT(DAL_STATUS_OK == ret);
    }

    memset((void*)(&dispif_info[MTKFB_DISPIF_PRIMARY_LCD]), 0, sizeof(mtk_dispif_info_t));
    
    switch(lcm_params->type)
    {
        case LCM_TYPE_DBI:
        {
            dispif_info[MTKFB_DISPIF_PRIMARY_LCD].displayType = DISPIF_TYPE_DBI;
            dispif_info[MTKFB_DISPIF_PRIMARY_LCD].displayMode = DISPIF_MODE_COMMAND;
            dispif_info[MTKFB_DISPIF_PRIMARY_LCD].isHwVsyncAvailable = 1;
            printk("DISP Info: DBI, CMD Mode, HW Vsync enable\n");
            break;
        }
        case LCM_TYPE_DPI:
        {
            dispif_info[MTKFB_DISPIF_PRIMARY_LCD].displayType = DISPIF_TYPE_DPI;
            dispif_info[MTKFB_DISPIF_PRIMARY_LCD].displayMode = DISPIF_MODE_VIDEO;
            dispif_info[MTKFB_DISPIF_PRIMARY_LCD].isHwVsyncAvailable = 1;				
            printk("DISP Info: DPI, VDO Mode, HW Vsync enable\n");
            break;
        }
        case LCM_TYPE_DSI:
        {
            dispif_info[MTKFB_DISPIF_PRIMARY_LCD].displayType = DISPIF_TYPE_DSI;
            if(lcm_params->dsi.mode == CMD_MODE)
            {
                dispif_info[MTKFB_DISPIF_PRIMARY_LCD].displayMode = DISPIF_MODE_COMMAND;
                dispif_info[MTKFB_DISPIF_PRIMARY_LCD].isHwVsyncAvailable = 1;
                printk("DISP Info: DSI, CMD Mode, HW Vsync enable\n");
            }
            else
            {
                dispif_info[MTKFB_DISPIF_PRIMARY_LCD].displayMode = DISPIF_MODE_VIDEO;
                dispif_info[MTKFB_DISPIF_PRIMARY_LCD].isHwVsyncAvailable = 1;
                printk("DISP Info: DSI, VDO Mode, HW Vsync enable\n");
            }
            
            break;
        }
        default:
        break;
    }
    

    if(disp_if_drv->get_panel_color_format())
    {
        switch(disp_if_drv->get_panel_color_format())
        {
        case PANEL_COLOR_FORMAT_RGB565:
            dispif_info[MTKFB_DISPIF_PRIMARY_LCD].displayFormat = DISPIF_FORMAT_RGB565;
        case PANEL_COLOR_FORMAT_RGB666:
            dispif_info[MTKFB_DISPIF_PRIMARY_LCD].displayFormat = DISPIF_FORMAT_RGB666;
        case PANEL_COLOR_FORMAT_RGB888:
            dispif_info[MTKFB_DISPIF_PRIMARY_LCD].displayFormat = DISPIF_FORMAT_RGB888;
            default:
                break;
        }
    }
    
    dispif_info[MTKFB_DISPIF_PRIMARY_LCD].displayWidth = DISP_GetScreenWidth();
    dispif_info[MTKFB_DISPIF_PRIMARY_LCD].displayHeight = DISP_GetScreenHeight();
    dispif_info[MTKFB_DISPIF_PRIMARY_LCD].vsyncFPS = lcd_fps;
    
    if(dispif_info[MTKFB_DISPIF_PRIMARY_LCD].displayWidth * dispif_info[MTKFB_DISPIF_PRIMARY_LCD].displayHeight <= 240*432)
    {
        dispif_info[MTKFB_DISPIF_PRIMARY_LCD].physicalHeight= dispif_info[MTKFB_DISPIF_PRIMARY_LCD].physicalWidth= 0;
    }
    else if(dispif_info[MTKFB_DISPIF_PRIMARY_LCD].displayWidth * dispif_info[MTKFB_DISPIF_PRIMARY_LCD].displayHeight <= 320*480)
    {
        dispif_info[MTKFB_DISPIF_PRIMARY_LCD].physicalHeight= dispif_info[MTKFB_DISPIF_PRIMARY_LCD].physicalWidth= 0;
    }
    else if(dispif_info[MTKFB_DISPIF_PRIMARY_LCD].displayWidth * dispif_info[MTKFB_DISPIF_PRIMARY_LCD].displayHeight <= 480*854)
    {
        dispif_info[MTKFB_DISPIF_PRIMARY_LCD].physicalHeight= dispif_info[MTKFB_DISPIF_PRIMARY_LCD].physicalWidth= 0;
    }
    else
    {
        dispif_info[MTKFB_DISPIF_PRIMARY_LCD].physicalHeight= dispif_info[MTKFB_DISPIF_PRIMARY_LCD].physicalWidth= 0;
    }
    
    dispif_info[MTKFB_DISPIF_PRIMARY_LCD].isConnected = 1;
    
	{
		LCM_PARAMS lcm_params_temp;
		memset((void*)&lcm_params_temp, 0, sizeof(lcm_params_temp));
		if(lcm_drv)
		{
			lcm_drv->get_params(&lcm_params_temp);
			dispif_info[MTKFB_DISPIF_PRIMARY_LCD].lcmOriginalWidth = lcm_params_temp.width;
			dispif_info[MTKFB_DISPIF_PRIMARY_LCD].lcmOriginalHeight = lcm_params_temp.height;			
			printk("DISP Info: LCM Panel Original Resolution(For DFO Only): %d x %d\n", dispif_info[MTKFB_DISPIF_PRIMARY_LCD].lcmOriginalWidth, dispif_info[MTKFB_DISPIF_PRIMARY_LCD].lcmOriginalHeight);
		}
		else
		{
			printk("DISP Info: Fatal Error!!, lcm_drv is null\n");
		}
	}

#if 0
	{
		unsigned int i = 0;
		unsigned int width, height;
		for(i=0;i<DFO_BOOT_COUNT;i++)	
		{
			if(!strcmp(&dfo_boot_default.name[i], "LCM_FAKE_WIDTH"))
				width = dfo_boot_default.value[i];
			
			if(!strcmp(&dfo_boot_default.name[i], "LCM_FAKE_HEIGHT"))
				height = dfo_boot_default.value[i];
		}
		printk("DISP Info: from DFO, width/height=%d x %d\n", width, height);
	}
#endif
    
    return r;
}


DISP_STATUS DISP_Deinit(void)
{
    DISP_CHECK_RET(DISP_PanelEnable(FALSE));
    DISP_CHECK_RET(DISP_PowerEnable(FALSE));

    return DISP_STATUS_OK;
}

// -----
DISP_STATUS DISP_PowerEnable(BOOL enable)
{
    DISP_STATUS ret = DISP_STATUS_OK;

    static BOOL s_enabled = TRUE;

    if (enable != s_enabled)
        s_enabled = enable;
    else
        return ret;
    
    if (down_interruptible(&sem_update_screen)) {
        printk("ERROR: Can't get sem_update_screen in DISP_PowerEnable()\n");
        return DISP_STATUS_ERROR;
    }
    
    disp_drv_init_context();
    
    is_engine_in_suspend_mode = enable ? FALSE : TRUE;
    
    if (!is_ipoh_bootup)
        needStartEngine = true;

    if (enable && lcm_drv && lcm_drv->resume_power)
    {
		lcm_drv->resume_power();
    }
    // No need for IPO-H reboot, or white screen flash will happen
    if((!is_ipoh_bootup) || (is_ipoh_bootup && (lcm_params->type==LCM_TYPE_DSI && lcm_params->dsi.mode == CMD_MODE)))
    {
    	ret = (disp_if_drv->enable_power) ?
    			(disp_if_drv->enable_power(enable)) :
    			DISP_STATUS_NOT_IMPLEMENTED;
    }
    
    if (enable) {
        DAL_OnDispPowerOn();
    }
    else if (lcm_drv && lcm_drv->suspend_power)
    {
        lcm_drv->suspend_power();
    }
        
    up(&sem_update_screen);
    
    return ret;
}


DISP_STATUS DISP_PanelEnable(BOOL enable)
{
    static BOOL s_enabled = TRUE;
    DISP_STATUS ret = DISP_STATUS_OK;

    DISP_LOG("panel is %s\n", enable?"enabled":"disabled");

    if (down_interruptible(&sem_update_screen)) 
    {
        printk("ERROR: Can't get sem_update_screen in DISP_PanelEnable()\n");
        return DISP_STATUS_ERROR;
    }
    
    disp_drv_init_context();
    
    is_lcm_in_suspend_mode = enable ? FALSE : TRUE;
    
    if (is_ipoh_bootup)
        s_enabled = TRUE;

    if (!lcm_drv->suspend || !lcm_drv->resume) 
    {
        ret = DISP_STATUS_NOT_IMPLEMENTED;
        goto End;
    }

    if (enable && !s_enabled) 
    {
        s_enabled = TRUE;
        disphal_panel_enable(lcm_drv, &LcmCmdMutex, TRUE);
    }
    else if (!enable && s_enabled)
    {
        s_enabled = FALSE;
        disphal_panel_enable(lcm_drv, &LcmCmdMutex, FALSE);
    }

End:
    up(&sem_update_screen);
    
    return ret;
}

DISP_STATUS DISP_SetBacklight(UINT32 level)
{
    DISP_STATUS ret = DISP_STATUS_OK;
    
    if (down_interruptible(&sem_update_screen)) {
        printk("ERROR: Can't get sem_update_screen in DISP_SetBacklight()\n");
        return DISP_STATUS_ERROR;
    }
    
    disp_drv_init_context();
    
    disphal_wait_not_busy();
    
    if (!lcm_drv->set_backlight) {
        ret = DISP_STATUS_NOT_IMPLEMENTED;
        goto End;
    }
    
    disphal_set_backlight(lcm_drv, &LcmCmdMutex, level);
End:
    
    up(&sem_update_screen);
    
    return ret;
}

DISP_STATUS DISP_SetBacklight_mode(UINT32 mode)
{
    DISP_STATUS ret = DISP_STATUS_OK;
    
    if (down_interruptible(&sem_update_screen)) {
        printk("ERROR: Can't get sem_update_screen in DISP_SetBacklight_mode()\n");
        return DISP_STATUS_ERROR;
    }
    
    disp_drv_init_context();
    
    disphal_wait_not_busy();
    
    if (!lcm_drv->set_backlight) {
        ret = DISP_STATUS_NOT_IMPLEMENTED;
        goto End;
    }
    
    disphal_set_backlight_mode(lcm_drv, &LcmCmdMutex, mode);
End:
    
    up(&sem_update_screen);
    
    return ret;

}

DISP_STATUS DISP_SetPWM(UINT32 divider)
{
    DISP_STATUS ret = DISP_STATUS_OK;
    
    if (down_interruptible(&sem_update_screen)) {
        printk("ERROR: Can't get sem_update_screen in DISP_SetPWM()\n");
        return DISP_STATUS_ERROR;
    }
    
    disp_drv_init_context();
    
    disphal_wait_not_busy();
    
    if (!lcm_drv->set_pwm) {
        ret = DISP_STATUS_NOT_IMPLEMENTED;
        goto End;
    }
    
    disphal_set_pwm(lcm_drv, &LcmCmdMutex, divider);
End:
    
    up(&sem_update_screen);
    
    return ret;
}

DISP_STATUS DISP_GetPWM(UINT32 divider, unsigned int *freq)
{
    DISP_STATUS ret = DISP_STATUS_OK;
    
    disp_drv_init_context();
    
    if (!lcm_drv->get_pwm) {
        ret = DISP_STATUS_NOT_IMPLEMENTED;
        goto End;
    }
    
    disphal_get_pwm(lcm_drv, &LcmCmdMutex, divider, freq);
End:
    return ret;
}


#ifndef BUILD_UBOOT
#if defined(MTK_LCD_HW_3D_SUPPORT)
static BOOL is3denabled = FALSE;
static BOOL ispwmenabled = FALSE;
static BOOL ismodechanged = FALSE;

static BOOL gCurrentMode = FALSE;
static BOOL gUsingMode = FALSE;


DISP_STATUS DISP_Set3DPWM(BOOL enable, BOOL landscape)
{
	unsigned int temp_reg;

	if (enable && (!ispwmenabled || ismodechanged))
	{
        struct pwm_spec_config pwm_spec_config = {
            .pwm_no = PWM1,
            .mode = PWM_MODE_FIFO,
            .clk_div = CLK_DIV128,
            .clk_src = PWM_CLK_NEW_MODE_BLOCK_DIV_BY_1625,
            .pmic_pad = false,
            .PWM_MODE_FIFO_REGS.IDLE_VALUE = IDLE_FALSE,
            .PWM_MODE_FIFO_REGS.GUARD_VALUE = GUARD_FALSE,
            .PWM_MODE_FIFO_REGS.STOP_BITPOS_VALUE = 1,      //1+1=2 bit valid
            .PWM_MODE_FIFO_REGS.HDURATION = 0,
            .PWM_MODE_FIFO_REGS.LDURATION = 0,
            .PWM_MODE_FIFO_REGS.GDURATION = 0,
            .PWM_MODE_FIFO_REGS.SEND_DATA0 = 1,             //reverse phase should fill 0x2(b'10)
            .PWM_MODE_FIFO_REGS.SEND_DATA1 = 0,
        	.PWM_MODE_FIFO_REGS.WAVE_NUM = 0,
        };       
		
		pwm_spec_config.pwm_no = PWM1;
#ifdef EVB
		if (landscape)
			pwm_spec_config.PWM_MODE_FIFO_REGS.SEND_DATA0 = 2;
		else
			pwm_spec_config.PWM_MODE_FIFO_REGS.SEND_DATA0 = 1;
#else
        pwm_spec_config.PWM_MODE_FIFO_REGS.SEND_DATA0 = 1;
#endif
        pwm_set_spec_config(&pwm_spec_config);

		pwm_spec_config.pwm_no = PWM2;
#ifdef EVB
        pwm_spec_config.PWM_MODE_FIFO_REGS.SEND_DATA0 = 1;
#else
        pwm_spec_config.PWM_MODE_FIFO_REGS.SEND_DATA0 = 1;
#endif
        pwm_set_spec_config(&pwm_spec_config);

		pwm_spec_config.pwm_no = PWM3;
#ifdef EVB
        pwm_spec_config.PWM_MODE_FIFO_REGS.SEND_DATA0 = 1;
#else
        if (landscape)
            pwm_spec_config.PWM_MODE_FIFO_REGS.SEND_DATA0 = 2;
        else
            pwm_spec_config.PWM_MODE_FIFO_REGS.SEND_DATA0 = 1;
#endif
        pwm_set_spec_config(&pwm_spec_config);

		pwm_spec_config.pwm_no = PWM4;
#ifdef EVB
		if (landscape)
			pwm_spec_config.PWM_MODE_FIFO_REGS.SEND_DATA0 = 1;
		else
			pwm_spec_config.PWM_MODE_FIFO_REGS.SEND_DATA0 = 2;
#else
		if (landscape)
			pwm_spec_config.PWM_MODE_FIFO_REGS.SEND_DATA0 = 1;
		else
			pwm_spec_config.PWM_MODE_FIFO_REGS.SEND_DATA0 = 2;	
#endif

        pwm_set_spec_config(&pwm_spec_config);

		mt_set_gpio_mode(GPIO73, GPIO_MODE_01);	
		mt_set_gpio_mode(GPIO74, GPIO_MODE_01);	
		mt_set_gpio_mode(GPIO75, GPIO_MODE_01);   
		mt_set_gpio_mode(GPIO76, GPIO_MODE_01); 

#ifdef EVB
        DISP_LOG("3D GPIO PWR NO CONFIG! \n");
#else
        mt_set_gpio_mode(GPIOEXT21, GPIO_MODE_GPIO);
        mt_set_gpio_dir(GPIOEXT21,GPIO_DIR_OUT);
        mt_set_gpio_out(GPIOEXT21, GPIO_OUT_ONE);
#endif

		ispwmenabled = TRUE;

        mutex_lock(&gS3DModeMutex);
        gS3DModeIsOn = DISP_S3D_3D_MODE;
        mutex_unlock(&gS3DModeMutex);
        wake_up_interruptible(&gS3DModeWaitQueue);
        DISP_LOG("set to 3D Mode and wake up gS3DModeWaitQueue in DISP_Set3DPWM()\n");
		
		DISP_LOG("3D PWM is enabled. landscape:%d ! \n", landscape);
	}
	else if (!enable && ispwmenabled)
	{	
#ifdef EVB
		DISP_LOG("3D GPIO PWR NO CONFIG! \n");
#else
		mt_set_gpio_mode(GPIOEXT21, GPIO_MODE_GPIO);
        mt_set_gpio_dir(GPIOEXT21,GPIO_DIR_OUT);
		mt_set_gpio_out(GPIOEXT21, GPIO_OUT_ZERO);
#endif		
        mt_set_gpio_mode(GPIO73, GPIO_MODE_GPIO);	
		mt_set_gpio_mode(GPIO74, GPIO_MODE_GPIO);	
		mt_set_gpio_mode(GPIO75, GPIO_MODE_GPIO);   
		mt_set_gpio_mode(GPIO76, GPIO_MODE_GPIO); 
		ispwmenabled = FALSE;


        
        mutex_lock(&gS3DModeMutex);
        gS3DModeIsOn = DISP_S3D_2D_MODE;
        mutex_unlock(&gS3DModeMutex);
        wake_up_interruptible(&gS3DModeWaitQueue);
        DISP_LOG("set to 2D Mode and wake up gS3DModeWaitQueue in DISP_Set3DPWM()\n");

		DISP_LOG("3D PWM is disabled ! \n");
	}

	return DISP_STATUS_OK;
}


BOOL DISP_Is3DEnabled(void)
{
	is3denabled = LCD_Is3DEnabled();

	return is3denabled;
}

BOOL DISP_is3DLandscapeMode(void)
{
	gCurrentMode = LCD_Is3DLandscapeMode();

	if (gCurrentMode != gUsingMode)
		ismodechanged = TRUE;
	else
		ismodechanged = FALSE;
	
	gUsingMode = gCurrentMode;

	return LCD_Is3DLandscapeMode();
}
#endif
#endif

// -----
DISP_STATUS DISP_SetFrameBufferAddr(UINT32 fbPhysAddr)
{
    cached_layer_config[FB_LAYER].addr = fbPhysAddr;
    cached_layer_config[FB_LAYER].isDirty = true;
    return DISP_STATUS_OK;
}

// -----
static BOOL is_overlaying = FALSE;

DISP_STATUS DISP_EnterOverlayMode(void)
{
    DISP_FUNC();
    if (is_overlaying) {
        return DISP_STATUS_ALREADY_SET;
    } else {
        is_overlaying = TRUE;
    }
    
    return DISP_STATUS_OK;
}


DISP_STATUS DISP_LeaveOverlayMode(void)
{
    DISP_FUNC();
    if (!is_overlaying) {
        return DISP_STATUS_ALREADY_SET;
    } else {
        is_overlaying = FALSE;
    }
    
    return DISP_STATUS_OK;
}

DISP_STATUS DISP_UpdateScreen(UINT32 x, UINT32 y, UINT32 width, UINT32 height)
{
    unsigned int is_video_mode;

//	return DISP_STATUS_OK;
    DISP_LOG("update screen, (%d,%d),(%d,%d)\n", x, y, width, height);

    if ((lcm_params->type==LCM_TYPE_DPI) ||
        ((lcm_params->type==LCM_TYPE_DSI) && (lcm_params->dsi.mode != CMD_MODE)))
        is_video_mode = 1;
    else
        is_video_mode = 0;

    if (down_interruptible(&sem_update_screen)) {
        printk("ERROR: Can't get sem_update_screen in DISP_UpdateScreen()\n");
        return DISP_STATUS_ERROR;
    }
    // if LCM is powered down, LCD would never recieve the TE signal
    //
    if (is_lcm_in_suspend_mode || is_engine_in_suspend_mode) goto End;
    if (is_video_mode && is_video_mode_running)
        needStartEngine = false;
    if (needStartEngine)
    {
        disphal_update_screen(lcm_drv, &LcmCmdMutex, x, y, width, height);
    }
    if (-1 != direct_link_layer) {
    } 
    else 
    {
        if (needStartEngine)
            disp_if_drv->update_screen(FALSE);
    }
    needStartEngine = false;
End:
    up(&sem_update_screen);

    return DISP_STATUS_OK;
}

DISP_STATUS DISP_WaitForLCDNotBusy(void)
{
    disphal_wait_not_busy();
    return DISP_STATUS_OK;
}

DISP_STATUS _DISP_ConfigUpdateScreen(UINT32 x, UINT32 y, UINT32 width, UINT32 height)
{
    // if LCM is powered down, LCD would never recieve the TE signal
    //
    if (is_lcm_in_suspend_mode || is_engine_in_suspend_mode) return DISP_STATUS_ERROR;
    disphal_update_screen(lcm_drv, &LcmCmdMutex, x, y, width, height);
    disp_if_drv->update_screen(TRUE);
    return DISP_STATUS_OK;
}

int DISP_RegisterExTriggerSource(DISP_EXTRA_CHECKUPDATE_PTR pCheckUpdateFunc , DISP_EXTRA_CONFIG_PTR pConfFunc)
{
    int index = 0;
    int hit = 0;
    if((NULL == pCheckUpdateFunc) || (NULL == pConfFunc))
    {
        printk("Warnning! [Func]%s register NULL function : %p,%p\n", __func__ , pCheckUpdateFunc , pConfFunc);
        return -1;
    }

    GetUpdateMutex();

    for(index = 0 ; index < DISP_CB_MAXCNT ; index += 1)
    {
        if(NULL == g_CB_Array.checkupdate_cb[index])
        {
            g_CB_Array.checkupdate_cb[index] = pCheckUpdateFunc;
            g_CB_Array.config_cb[index] = pConfFunc;
            hit = 1;
            break;
        }
    }

    ReleaseUpdateMutex();

    return (hit ? index : (-1));
}

void DISP_UnRegisterExTriggerSource(int u4ID)
{
    if(DISP_CB_MAXCNT < (u4ID+1))
    {
        printk("Warnning! [Func]%s unregister a never registered function : %d\n", __func__ , u4ID);
        return;
    }

    GetUpdateMutex();

    g_CB_Array.checkupdate_cb[u4ID] = NULL;
    g_CB_Array.config_cb[u4ID] = NULL;

    ReleaseUpdateMutex();
}
#if defined (MTK_HDMI_SUPPORT)
extern 	bool is_hdmi_active(void);
#endif

static void _DISP_DumpFrameBuffer(void)
{
	struct fb_info* pInfo = mtkfb_fbi;
	MMP_MetaData_t meta;
	MMP_MetaDataBitmap_t Bitmap;
	if (gCaptureFBEnable)
	{
		if (gCaptureFBRawData) {
			meta.data_type = MMProfileMetaRaw;
			meta.size = DISP_GetScreenHeight() * pInfo->fix.line_length;
			meta.pData = ((struct mtkfb_device*)pInfo->par)->fb_va_base;
			MMProfileLogMeta(MTKFB_MMP_Events.FBDump, MMProfileFlagPulse, &meta);

		} else {
			Bitmap.data1 = pInfo->var.yoffset;
			Bitmap.width = DISP_GetScreenWidth();
			Bitmap.height = DISP_GetScreenHeight();
			Bitmap.bpp = pInfo->var.bits_per_pixel;

			switch (pInfo->var.bits_per_pixel)
			{
			case 16:
				Bitmap.format = MMProfileBitmapRGB565;
				break;
			case 32:
				Bitmap.format = MMProfileBitmapBGRA8888;
				break;
			default:
				Bitmap.format = MMProfileBitmapRGB565;
				Bitmap.bpp = 16;
				break;
			}

			Bitmap.start_pos = 0;
			Bitmap.pitch = pInfo->fix.line_length;
			if (Bitmap.pitch == 0)
			{
				Bitmap.pitch = ALIGN_TO(Bitmap.width, disphal_get_fb_alignment()) * Bitmap.bpp / 8;
			}
			Bitmap.data_size = Bitmap.pitch * Bitmap.height;
			Bitmap.down_sample_x = gCaptureFBDownX;
			Bitmap.down_sample_y = gCaptureFBDownY;
			Bitmap.pData = ((struct mtkfb_device*)pInfo->par)->fb_va_base;
			MMProfileLogMetaBitmap(MTKFB_MMP_Events.FBDump, MMProfileFlagPulse, &Bitmap);
		}
	}
}

static void _DISP_DumpOverlay(void)
{
	static unsigned int index = 0;
	static unsigned int mva[2];
	static void* va[2];
	static unsigned int init = 0;
	static unsigned int enabled = 0;

	unsigned int buf_size = DISP_GetScreenWidth() * DISP_GetScreenHeight() * 3;
	unsigned int paddr = 0;

	MMP_MetaData_t meta;
	MMP_MetaDataBitmap_t Bitmap;

	// Prepare settings and retrieve the buffer address
	if (DISP_IsDecoupleMode())
	{
		paddr = disp_request_buffer(buffer_queue);	//mem_write buffer
	} else {
		if (init == 0)
		{
			va[0] = vmalloc(buf_size);
			va[1] = vmalloc(buf_size);
			memset(va[0], 0, buf_size);
			memset(va[1], 0, buf_size);
			disphal_map_overlay_out_buffer((unsigned int)va[0], buf_size, &(mva[0]));
			disphal_map_overlay_out_buffer((unsigned int)va[1], buf_size, &(mva[1]));
			disphal_init_overlay_to_memory();
			init = 1;
		}
		if (!gCaptureOvlEnable)
		{
			if (enabled)
			{
				DISP_Config_Overlay_to_Memory(mva[index], false);
				enabled = 0;
			}
			return;
		}
		DISP_Config_Overlay_to_Memory(mva[index], true);
		enabled = 1;
		disphal_sync_overlay_out_buffer((unsigned int)va[index], DISP_GetScreenHeight()*DISP_GetScreenWidth()*24/8);
        // config front buffer to hw for next frame, back buffer to mmp
        index = 1 - index;
        paddr = mva[index];
	}
	// Dump buffer to mmprofile
	if (gCaptureOvlEnable)
	{
		// Dump buffer to mmp
		if (gCaptureOvlRawData) {
			meta.data_type = MMProfileMetaRaw;
			meta.size = buf_size;

			disphal_dma_map_kernel(paddr, meta.size, (unsigned int*)&meta.pData, &meta.size);
			MMProfileLogMeta(MTKFB_MMP_Events.OvlDump, MMProfileFlagPulse, &meta);
			disphal_dma_unmap_kernel(paddr, meta.size, (unsigned int)meta.pData);

		} else {
			Bitmap.data1 = 0;
			Bitmap.width = DISP_GetScreenWidth();
			Bitmap.height = DISP_GetScreenHeight();
			Bitmap.format = MMProfileBitmapBGR888;
			Bitmap.bpp = 24;
			Bitmap.start_pos = 0;
			Bitmap.pitch = DISP_GetScreenWidth() * 3;
			Bitmap.data_size = buf_size;
			Bitmap.down_sample_x = gCaptureLayerDownX;
			Bitmap.down_sample_y = gCaptureLayerDownY;

			disphal_dma_map_kernel(paddr, Bitmap.data_size, (unsigned int*)&Bitmap.pData, &Bitmap.data_size);
			MMProfileLogMetaBitmap(MTKFB_MMP_Events.OvlDump, MMProfileFlagPulse, &Bitmap);
			disphal_dma_unmap_kernel(paddr, Bitmap.data_size, (unsigned int)Bitmap.pData);
		}
	}
}

static void _DISP_DumpLayer(OVL_CONFIG_STRUCT* pLayer)
{
	MMP_MetaData_t meta;
	MMP_MetaDataBitmap_t Bitmap;
	if (gCaptureLayerEnable && pLayer->layer_en)
	{
		if (gCaptureLayerRawData) {
			meta.data_type = MMProfileMetaRaw;
			meta.size = pLayer->src_pitch * pLayer->src_h;

			disphal_dma_map_kernel(pLayer->addr, meta.size, (unsigned int*)&meta.pData, &meta.size);
			MMProfileLogMeta(MTKFB_MMP_Events.Layer[pLayer->layer], MMProfileFlagPulse, &meta);
			disphal_dma_unmap_kernel(pLayer->addr, meta.size, (unsigned int)meta.pData);

		} else {
			Bitmap.data1 = pLayer->vaddr;
			Bitmap.width = pLayer->dst_w;
			Bitmap.height = pLayer->dst_h;
			switch (pLayer->fmt)
			{
			case eRGB565: Bitmap.format = MMProfileBitmapRGB565; Bitmap.bpp = 16; break;
			case eRGB888: Bitmap.format = MMProfileBitmapBGR888; Bitmap.bpp = 24; break;
			case eARGB8888:
			case ePARGB8888:
				Bitmap.format = MMProfileBitmapBGRA8888; Bitmap.bpp = 32; break;
			case eBGR888: Bitmap.format = MMProfileBitmapRGB888; Bitmap.bpp = 24; break;
			case eABGR8888:
			case ePABGR8888:
				Bitmap.format = MMProfileBitmapRGBA8888; Bitmap.bpp = 32; break;
			default:
				printk("error: _DISP_DumpLayer(), unknow format=%d \n", pLayer->fmt);
				// enhancement: for those YUV or unknown format, we dump it to raw data so
				//   that we can check if the buffer data is correct.
				//   user should config a larger MMProfile Metadata buffer size.
				meta.data_type = MMProfileMetaRaw;
				meta.size = pLayer->src_pitch * pLayer->src_h;
				disphal_dma_map_kernel(pLayer->addr, meta.size, (unsigned int*)&meta.pData, &meta.size);
				MMProfileLogMeta(MTKFB_MMP_Events.Layer[pLayer->layer], MMProfileFlagPulse, &meta);
				disphal_dma_unmap_kernel(pLayer->addr, meta.size, (unsigned int)meta.pData);
				return;
			}
			Bitmap.start_pos = 0;
			Bitmap.pitch = pLayer->src_pitch;
			Bitmap.data_size = Bitmap.pitch * Bitmap.height;
			Bitmap.down_sample_x = gCaptureLayerDownX;
			Bitmap.down_sample_y = gCaptureLayerDownY;
			disphal_dma_map_kernel(pLayer->addr, Bitmap.data_size, (unsigned int*)&Bitmap.pData, &Bitmap.data_size);
			MMProfileLogMetaBitmap(MTKFB_MMP_Events.Layer[pLayer->layer], MMProfileFlagPulse, &Bitmap);
			disphal_dma_unmap_kernel(pLayer->addr, Bitmap.data_size, (unsigned int)Bitmap.pData);
		}
	}
}

static void _DISP_VSyncCallback(void* pParam);

void DISP_StartConfigUpdate(void)
{
    if(((LCM_TYPE_DSI == lcm_params->type) && (CMD_MODE == lcm_params->dsi.mode)) || (LCM_TYPE_DBI == lcm_params->type))
    {
        config_update_task_wakeup = 1;
        wake_up_interruptible(&config_update_wq);
    }
}

static int _DISP_ESD_Check(int* dirty)
{
    unsigned int esd_check_count;
    if (need_esd_check && (!is_early_suspended))
    {
        esd_check_count = 0;
        disp_running = 1;
        MMProfileLog(MTKFB_MMP_Events.EsdCheck, MMProfileFlagStart);
        while (esd_check_count < LCM_ESD_CHECK_MAX_COUNT)
        {
            if(DISP_EsdCheck())
            {
                MMProfileLogEx(MTKFB_MMP_Events.EsdCheck, MMProfileFlagPulse, 0, 0);
                DISP_EsdRecover();
            }
            else
            {
                break;
            }
            esd_check_count++;
        }
        if (esd_check_count >= LCM_ESD_CHECK_MAX_COUNT)
        {
            MMProfileLogEx(MTKFB_MMP_Events.EsdCheck, MMProfileFlagPulse, 2, 0);
            esd_kthread_pause = TRUE;
        }
        if (esd_check_count)
        {
            disphal_update_screen(lcm_drv, &LcmCmdMutex, 0, 0, DISP_GetScreenWidth(), DISP_GetScreenHeight());
            *dirty = 1;
        }
        need_esd_check = 0;
        wake_up_interruptible(&esd_check_wq);
        MMProfileLog(MTKFB_MMP_Events.EsdCheck, MMProfileFlagEnd);
        return 1;
    }
    return 0;
}

static int _DISP_MergeAALDirty (disp_path_config_dirty *dirty_flag) {
	int i;
	int dirty = 0;
	dirty_flag->aal_dirty = dirty_flag->ovl_dirty; //overlay refresh means AAL also needs refresh
	//GetUpdateMutex();
	if (mutex_trylock(&UpdateRegMutex)) {
		for (i = 0; i < DISP_CB_MAXCNT; i++) {
			if ((NULL != g_CB_Array.checkupdate_cb[i])
					&& g_CB_Array.checkupdate_cb[i](dirty_flag->aal_dirty)) {
				dirty = 1;
				dirty_flag->aal_dirty = 1;
				break;
			}
		}
		mutex_unlock(&UpdateRegMutex);
	}
	return dirty;
}

#if 0
static BOOL _DISP_IsYUVFormat (DpColorFormat fmt) {
	BOOL yuv = false;
	switch(fmt) {
	case eYUY2:
	case eUYVY:
		yuv = true;
		break;
	default:
		yuv = false;
	}
	return yuv;
}
#endif

static disp_job* _DISP_MergeOVLDirty(disp_path_config_dirty *dirty_flag) {
	disp_job *job = NULL;
	if ( (job=disp_acquire_job()) != NULL ) {
		dirty_flag->ovl_dirty = TRUE;
	}
	return job;
}

static void _DISP_StartSoftTimer (void) {
	if ((lcm_params->type == LCM_TYPE_DBI) ||
			((lcm_params->type == LCM_TYPE_DSI) && (lcm_params->dsi.mode == CMD_MODE)))  {
		// Start update timer.
		if (!is_early_suspended) {
			if (is_immediateupdate) {
				hrtimer_start(&cmd_mode_update_timer, ktime_set(0 , 5000000), HRTIMER_MODE_REL);
			} else {
				hrtimer_start(&cmd_mode_update_timer, cmd_mode_update_timer_period, HRTIMER_MODE_REL);
			}
		}
	}
}

static int _DISP_ConfigDlinkDatapath (disp_job *job, disp_path_config_dirty *dirty_flag) {
	int i;
	OVL_CONFIG_STRUCT ovl_config;
	disp_path_get_mutex();
	if (dirty_flag->ovl_dirty) {
		MMProfileLog(MTKFB_MMP_Events.ConfigOVL, MMProfileFlagStart);
		for (i=0; i<DDP_OVL_LAYER_MUN; i++) {
			ovl_config.layer = i;
			_DISP_InitHWConfig(job, &ovl_config, NULL);
			if (ovl_config.isDirty) {
				_DISP_DumpLayer(&ovl_config);
				disp_path_config_layer(&ovl_config);
				if (ovl_config.layer_en) {
					disp_sync_inc_timeline(disp_config.session_id, ovl_config.layer, ovl_config.buff_idx-1);
				} else {
					disp_sync_inc_timeline(disp_config.session_id, ovl_config.layer, ovl_config.buff_idx);
				}
			}
		}
        _DISP_DumpOverlay();
        _DISP_DumpFrameBuffer();

		if ((lcm_params->type == LCM_TYPE_DBI) ||
				((lcm_params->type == LCM_TYPE_DSI) && (lcm_params->dsi.mode == CMD_MODE))) {
			if ((PanDispSettingPending==1) && (PanDispSettingDirty==0)) {
				PanDispSettingApplied = 1;
				PanDispSettingPending = 0;
			}
			wake_up(&reg_update_wq);
		}
		MMProfileLog(MTKFB_MMP_Events.ConfigOVL, MMProfileFlagEnd);
	}

	// Apply AAL config here.
	if (dirty_flag->aal_dirty) {
		MMProfileLog(MTKFB_MMP_Events.ConfigAAL, MMProfileFlagStart);
		//GetUpdateMutex();
		if(mutex_trylock(&UpdateRegMutex)) {
			for(i = 0 ; i < DISP_CB_MAXCNT ; i += 1) {
				if((NULL != g_CB_Array.checkupdate_cb[i]) && g_CB_Array.checkupdate_cb[i](dirty_flag->ovl_dirty)) {
					g_CB_Array.config_cb[i](dirty_flag->aal_dirty);
				}
			}
			mutex_unlock(&UpdateRegMutex);
		}
		MMProfileLog(MTKFB_MMP_Events.ConfigAAL, MMProfileFlagEnd);
	}
	// Trigger interface engine for cmd mode.
	if ((lcm_params->type == LCM_TYPE_DBI) ||
			((lcm_params->type == LCM_TYPE_DSI) && (lcm_params->dsi.mode == CMD_MODE))) {
		DISP_STATUS ret = _DISP_ConfigUpdateScreen(0, 0, DISP_GetScreenWidth(), DISP_GetScreenHeight());
		if ((ret != DISP_STATUS_OK) && (is_early_suspended == 0)) {
			hrtimer_start(&cmd_mode_update_timer, cmd_mode_update_timer_period, HRTIMER_MODE_REL);
		}
		//				if(false == g_dsi_start_in_interrupt_cmd)
#ifndef MTK_FB_START_DSI_ISR
		{
			disp_path_release_mutex();
		}
#endif
	} else {
		disp_path_release_mutex();
	}
	return 0;
}

static int _DISP_ConfigUpdateKThread(void *data)
{
    int dirty = 0;
    //int index = 0;
    disp_path_config_dirty dirty_flag;
    struct disp_path_config_mem_out_struct mem_out_config;
    disp_job *job = NULL;

    struct sched_param param = { .sched_priority = RTPM_PRIO_SCRN_UPDATE };
    sched_setscheduler(current, SCHED_RR, &param);
    while(1)
    {
        wait_event_interruptible(config_update_wq, config_update_task_wakeup);
        config_update_task_wakeup = 0;
        //MMProfileLog(MTKFB_MMP_Events.UpdateConfig, MMProfileFlagStart);
#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT) && defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
        // the config thread is wake up by frame done irqs
        // we need to check if we should switch secure port to non-secure
        disp_path_update_secure_port();
#endif
        dirty = 0;
        memset(&dirty_flag, 0, sizeof(dirty_flag));

        if (down_interruptible(&sem_early_suspend)) {
        	printk("[FB Driver] can't get semaphore:%d\n", __LINE__);
            continue;
        }
        //MMProfileLogEx(MTKFB_MMP_Events.EarlySuspend, MMProfileFlagStart, 1, 0);
        if(((LCM_TYPE_DSI == lcm_params->type) && (CMD_MODE == lcm_params->dsi.mode)) || (LCM_TYPE_DBI == lcm_params->type))
        {
            _DISP_VSyncCallback(NULL);
        }
		if (!((lcm_params->type == LCM_TYPE_DSI) && (lcm_params->dsi.mode != CMD_MODE)))
		{
			if (_DISP_ESD_Check(&dirty))
			{
				disp_running = 0;
			}
        }
		if (!is_early_suspended) {
			// 1. Merge dirty flags
			dirty = _DISP_MergeAALDirty(&dirty_flag);

			if (!DISP_IsDecoupleMode()) {
				if (mutex_trylock(&SwitchModeMutex)) {
					job = _DISP_MergeOVLDirty(&dirty_flag);
					dirty |= dirty_flag.ovl_dirty;
					if (mutex_trylock(&MemOutSettingMutex)) {
						if (MemOutConfig.dirty) {
							memcpy(&mem_out_config, &MemOutConfig, sizeof(MemOutConfig));
							MemOutConfig.dirty = 0;
						} else {
							mem_out_config.dirty = 0;
						}
						mutex_unlock(&MemOutSettingMutex);
					}
					mutex_unlock(&SwitchModeMutex);
				}
			}
			// 2. Configure hw if dirty
			if (DISP_IsDecoupleMode()) {
				if (disp_acquire_buffer(buffer_queue) || dirty) {
					disp_running = 1;
					dirty_flag.aal_dirty = dirty;
					_DISP_ConfigMemReadDatapath(&dirty_flag);
				} else {
					_DISP_StartSoftTimer();
				}
			} else {
				if (dirty) {
					disp_running = 1;
					dirty_flag.aal_dirty = dirty;
					_DISP_ConfigDlinkDatapath(job, &dirty_flag);
				} else {
					_DISP_StartSoftTimer();
				}
				if (mem_out_config.dirty) {
					MMProfileLogEx(MTKFB_MMP_Events.ConfigMemOut, MMProfileFlagStart, mem_out_config.enable, mem_out_config.dstAddr);
					disp_path_config_mem_out(&mem_out_config);
					MMProfileLogEx(MTKFB_MMP_Events.ConfigMemOut, MMProfileFlagEnd, 0, 0);
				}
			}
		}
		if ((lcm_params->type == LCM_TYPE_DSI) && (lcm_params->dsi.mode != CMD_MODE))
		{
			if (_DISP_ESD_Check(&dirty))
			{
				disp_running = 1;
			}
		}

        //MMProfileLog(MTKFB_MMP_Events.UpdateConfig, MMProfileFlagEnd);
        //MMProfileLogEx(MTKFB_MMP_Events.EarlySuspend, MMProfileFlagEnd, 1, 0);
        up(&sem_early_suspend);
        if (kthread_should_stop())
            break;
    }

    return 0;
}

static void _DISP_HWDoneCallback(void* pParam)
{
    MMProfileLogEx(MTKFB_MMP_Events.DispDone, MMProfileFlagPulse, is_early_suspended, 0);
    disp_running = 0;
    wake_up_interruptible(&disp_done_wq);
}

static void _DISP_VSyncCallback(void* pParam)
{
    MMProfileLog(MTKFB_MMP_Events.VSync, MMProfileFlagPulse);
    vsync_wq_flag = 1;
    wake_up_interruptible(&vsync_wq);
}

static void _DISP_RegUpdateCallback(void* pParam)
{
    MMProfileLog(MTKFB_MMP_Events.RegUpdate, MMProfileFlagPulse);
// Fence Sync Object
#if defined (MTK_FB_SYNC_SUPPORT)
    if (!DISP_IsDecoupleMode()) {
    	clean_up_task_wakeup = 1;
    	wake_up_interruptible(&clean_up_wq);
    }
#endif
	if (!DISP_IsDecoupleMode()) {
		if ((lcm_params->type == LCM_TYPE_DPI) ||
				((lcm_params->type == LCM_TYPE_DSI) && (lcm_params->dsi.mode != CMD_MODE)))
		{
			if ((PanDispSettingPending==1) && (PanDispSettingDirty==0))
			{
				PanDispSettingApplied = 1;
				PanDispSettingPending = 0;
			}
		}
	}
}

static void _DISP_TargetLineCallback(void* pParam)
{
	if ( LCM_TYPE_DPI == lcm_params->type
	|| (LCM_TYPE_DSI == lcm_params->type && lcm_params->dsi.mode != CMD_MODE)
	|| (LCM_TYPE_DSI == lcm_params->type && lcm_params->dsi.mode == CMD_MODE && TARGET_LINE == DISP_Get_MergeTrigger_Mode()))
	{
		//hrtimer_cancel(&cmd_mode_update_timer);
	    //tasklet_hi_schedule(&ConfigUpdateTask);
	    MMProfileLogEx(MTKFB_MMP_Events.UpdateConfig, MMProfileFlagPulse, 0, 0);
	    config_update_task_wakeup = 1;
	    wake_up_interruptible(&config_update_wq);
    }
}

static void _DISP_ScrUpdStartCallback(void* pParam)
{
#if defined(MTK_OVL_DECOUPLE_SUPPORT)
	read_mem_running = 1;
#endif
}

static void _DISP_ScrUpdEndCallback(void* pParam)
{
#if defined(MTK_OVL_DECOUPLE_SUPPORT)
	if (DISP_IsDecoupleMode()) {
		disp_release_buffer(buffer_queue);
	}
	read_mem_running = 0;
	wake_up_interruptible(&read_mem_idle_wq);
#endif
}

static void _DISP_CmdDoneCallback(void* pParam)
{
    //tasklet_hi_schedule(&ConfigUpdateTask);
    if(COMMAND_DONE == DISP_Get_MergeTrigger_Mode())
    {
    	MMProfileLogEx(MTKFB_MMP_Events.UpdateConfig, MMProfileFlagPulse, 1, 0);
    	config_update_task_wakeup = 1;
    	wake_up_interruptible(&config_update_wq);
    }

    _DISP_HWDoneCallback(NULL);
}

static enum hrtimer_restart _DISP_CmdModeTimer_handler(struct hrtimer *timer)
{
	if(NONE_MODE == DISP_Get_MergeTrigger_Mode())
	{
	   /***************!!!TEMP***************/
		DISP_Set_MergeTrigger_Mode(COMMAND_DONE);
		//DISP_Set_MergeTrigger_Mode(TARGET_LINE);
	}

    MMProfileLogEx(MTKFB_MMP_Events.UpdateConfig, MMProfileFlagPulse, 2, 0);
    config_update_task_wakeup = 1;
    wake_up_interruptible(&config_update_wq);
    return HRTIMER_NORESTART;
}

void DISP_Set_MergeTrigger_Mode(unsigned int mode)
{
	dispMergeTriggerMode = mode;
}

DISP_MERGE_TRIGGER_MODE DISP_Get_MergeTrigger_Mode(void)
{
	return dispMergeTriggerMode;
}

void DISP_InitVSYNC(unsigned int vsync_interval)
{
    init_waitqueue_head(&config_update_wq);
    init_waitqueue_head(&vsync_wq);
    config_update_task = kthread_create(
        _DISP_ConfigUpdateKThread, NULL, "disp_config_update_kthread");

    if (IS_ERR(config_update_task)) 
    {
        DISP_LOG("DISP_InitVSYNC(): Cannot create config update kthread\n");
        return;
    }
    wake_up_process(config_update_task);
    // Fence Sync Object
#if defined (MTK_FB_SYNC_SUPPORT)
    clean_up_task = kthread_create(_DISP_CleanUpKThread, NULL, "disp_clean_up_kthread");
    if (IS_ERR(clean_up_task))
    {
    	DISP_LOG("DISP_InitVSYNC(): Cannot create clean up kthread\n");
    }
    wake_up_process(clean_up_task);
#endif
#if defined(MTK_OVL_DECOUPLE_SUPPORT)
    decouple_worker_task = kthread_create(_DISP_DecoupleWorkerKThread, NULL, "disp_worker_kthread");
    if (IS_ERR(decouple_worker_task))
    {
    	DISP_LOG("DISP_InitVSYNC(): Cannot create disp_worker kthread\n");
    }
    wake_up_process(decouple_worker_task);
#endif
    disphal_register_event("DISP_CmdDone", _DISP_CmdDoneCallback);
    disphal_register_event("DISP_RegUpdate", _DISP_RegUpdateCallback);
    disphal_register_event("DISP_VSync", _DISP_VSyncCallback);
    disphal_register_event("DISP_TargetLine", _DISP_TargetLineCallback);

    disphal_register_event("DISP_ScrUpdStart", _DISP_ScrUpdStartCallback);
    disphal_register_event("DISP_ScrUpdEnd", _DISP_ScrUpdEndCallback);

    disphal_register_event("DISP_HWDone", _DISP_HWDoneCallback);
    if ((LCM_TYPE_DBI == lcm_params->type) ||
        ((LCM_TYPE_DSI == lcm_params->type) && (CMD_MODE == lcm_params->dsi.mode)))
    {
        cmd_mode_update_timer_period = ktime_set(0 , vsync_interval*1000);
        printk("[MTKFB] vsync timer_period=%d \n", vsync_interval);
        hrtimer_init(&cmd_mode_update_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
        cmd_mode_update_timer.function = _DISP_CmdModeTimer_handler;
        config_update_task_wakeup = 1;
        wake_up_interruptible(&config_update_wq);
    }

}

void DISP_WaitVSYNC(void)
{
	MMProfileLog(MTKFB_MMP_Events.WaitVSync, MMProfileFlagStart);
	vsync_wq_flag = 0;
	if (wait_event_interruptible_timeout(vsync_wq, vsync_wq_flag, HZ/10) == 0)
	{
		printk("[DISP] Wait VSync timeout. early_suspend=%d\n", is_early_suspended);
	}
	MMProfileLog(MTKFB_MMP_Events.WaitVSync, MMProfileFlagEnd);
}

DISP_STATUS DISP_PauseVsync(BOOL enable)
{
    if((LCM_TYPE_DBI == lcm_params->type) || 
        (LCM_TYPE_DSI == lcm_params->type && lcm_params->dsi.mode == CMD_MODE))
    {
        if (enable)
            hrtimer_cancel(&cmd_mode_update_timer);
    }
    else if((LCM_TYPE_DPI == lcm_params->type) || 
        (LCM_TYPE_DSI == lcm_params->type && lcm_params->dsi.mode != CMD_MODE))
    {
    }
    else
    {
        DISP_LOG("DISP_PauseVSYNC():unknown interface\n");
    }
    return DISP_STATUS_OK;
}

DISP_STATUS DISP_ConfigDither(int lrs, int lgs, int lbs, int dbr, int dbg, int dbb)
{
    DISP_LOG("DISP_ConfigDither lrs:0x%x, lgs:0x%x, lbs:0x%x, dbr:0x%x, dbg:0x%x, dbb:0x%x \n", lrs, lgs, lbs, dbr, dbg, dbb);

    return DISP_STATUS_OK;
}


// ---------------------------------------------------------------------------
//  Retrieve Information
// ---------------------------------------------------------------------------

BOOL DISP_IsVideoMode(void)
{
    disp_drv_init_context();
    if(lcm_params)
        return lcm_params->type==LCM_TYPE_DPI || (lcm_params->type==LCM_TYPE_DSI && lcm_params->dsi.mode != CMD_MODE);
    else
    {
        printk("WARNING!! DISP_IsVideoMode is called before display driver inited!\n");
        return 0;
    }
}

UINT32 DISP_GetScreenWidth(void)
{
    disp_drv_init_context();
    if(lcm_params)
        return lcm_params->width;
    else
    {
        printk("WARNING!! get screen width before display driver inited!\n");
        return 0;
    }
}
EXPORT_SYMBOL(DISP_GetScreenWidth);

UINT32 DISP_GetScreenHeight(void)
{
    disp_drv_init_context();
    if(lcm_params)
        return lcm_params->height;
    else
    {
        printk("WARNING!! get screen height before display driver inited!\n");
        return 0;
    }
}

UINT32 DISP_GetPhysicalHeight(void)
{
    disp_drv_init_context();
    if(lcm_params)
    {
        printk("[wwy]lcm_parms->physical_height = %d\n",lcm_params->physical_height);
        return lcm_params->physical_height;
    }
    else
    {
        printk("WARNING!! get physical_height before display driver inited!\n");
        return 0;
    }
}

UINT32 DISP_GetPhysicalWidth(void)
{
    disp_drv_init_context();
    if(lcm_params)
    {
        printk("[wwy]lcm_parms->physical_width = %d\n",lcm_params->physical_width);
        return lcm_params->physical_width;
    }
    else
    {
        printk("WARNING!! get physical_width before display driver inited!\n");
        return 0;
    }
}

DISP_STATUS DISP_SetScreenBpp(UINT32 bpp)
{
    ASSERT(bpp != 0);

    if( bpp != 16   &&          \
        bpp != 24   &&          \
        bpp != 32   &&          \
        1 )
    {
        DISP_LOG("DISP_SetScreenBpp error, not support %d bpp\n", bpp);
        return DISP_STATUS_ERROR;
    }

    disp_fb_bpp = bpp;
    DISP_LOG("DISP_SetScreenBpp %d bpp\n", bpp);

    return DISP_STATUS_OK; 
}

UINT32 DISP_GetScreenBpp(void)
{
    return disp_fb_bpp; 
}

DISP_STATUS DISP_SetPages(UINT32 pages)
{
    ASSERT(pages != 0);

    disp_fb_pages = pages;
    DISP_LOG("DISP_SetPages %d pages\n", pages);

    return DISP_STATUS_OK; 
}

UINT32 DISP_GetPages(void)
{
    return disp_fb_pages;   // Double Buffers
}


BOOL DISP_IsDirectLinkMode(void)
{
    return (-1 != direct_link_layer) ? TRUE : FALSE;
}


BOOL DISP_IsInOverlayMode(void)
{
    return is_overlaying;
}

UINT32 DISP_GetFBRamSize(void)
{
    return ALIGN_TO(DISP_GetScreenWidth(), disphal_get_fb_alignment()) * 
           ALIGN_TO(DISP_GetScreenHeight(), disphal_get_fb_alignment()) * 
           ((DISP_GetScreenBpp() + 7) >> 3) * 
           DISP_GetPages();
}


UINT32 DISP_GetVRamSize(void)
{
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
#if defined(MTK_OVL_DECOUPLE_SUPPORT)
        // get ovl-wdma buffer size
        vramSize += DISP_GetOVLRamSize();
#endif
        // Align vramSize to 1MB
        //
        vramSize = ALIGN_TO_POW_OF_2(vramSize, 0x100000);
        
        DISP_LOG("DISP_GetVRamSize: %u bytes\n", vramSize);
    }
    
    return vramSize;
}

UINT32 DISP_GetVRamSizeBoot(char *cmdline)
{
    static UINT32 vramSize = 0;
    
    if(vramSize)
    {
        return vramSize;
    }
    
    disp_get_lcm_name_boot(cmdline);
    
    // if can't get the lcm type from uboot, we will return 0x800000 for a safe value
    if(disp_if_drv)
        vramSize = DISP_GetVRamSize();
    else
    {
        printk("%s, can't get lcm type, reserved memory size will be set as 0x800000\n", __func__);
        return 0x1400000;
    }   
    // Align vramSize to 1MB
    //
    vramSize = ALIGN_TO_POW_OF_2(vramSize, 0x100000);

    printk("DISP_GetVRamSizeBoot: %u bytes[%dMB]\n", vramSize, (vramSize>>20));

    return vramSize;
}

PANEL_COLOR_FORMAT DISP_GetPanelColorFormat(void)
{
    disp_drv_init_context();
    
    return (disp_if_drv->get_panel_color_format) ?
        (disp_if_drv->get_panel_color_format()) :
                DISP_STATUS_NOT_IMPLEMENTED;
}

UINT32 DISP_GetPanelBPP(void)
{
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
}

UINT32 DISP_GetOutputBPPforDithering(void)
{
    disp_drv_init_context();
    
    return (disp_if_drv->get_dithering_bpp) ?
        (disp_if_drv->get_dithering_bpp()) :
                DISP_STATUS_NOT_IMPLEMENTED;
}

DISP_STATUS DISP_Config_Overlay_to_Memory(unsigned int mva, int enable)
{
    int wait_ret = 0;
    
    //	struct disp_path_config_mem_out_struct mem_out = {0};
    
    if(enable)
    {
        MemOutConfig.outFormat = eRGB888;

        MemOutConfig.enable = 1;
        MemOutConfig.dstAddr = mva;
        MemOutConfig.srcROI.x = 0;
        MemOutConfig.srcROI.y = 0;
        MemOutConfig.srcROI.height= DISP_GetScreenHeight();
        MemOutConfig.srcROI.width= DISP_GetScreenWidth();
        mutex_lock(&MemOutSettingMutex);
        MemOutConfig.dirty = 1;
        mutex_unlock(&MemOutSettingMutex);
    }
    else
    {
        MemOutConfig.outFormat = eRGB888;
        MemOutConfig.enable = 0;
        MemOutConfig.dstAddr = mva;
        MemOutConfig.srcROI.x = 0;
        MemOutConfig.srcROI.y = 0;
        MemOutConfig.srcROI.height= DISP_GetScreenHeight();
        MemOutConfig.srcROI.width= DISP_GetScreenWidth();

        mutex_lock(&MemOutSettingMutex);
        MemOutConfig.dirty = 1;		
        mutex_unlock(&MemOutSettingMutex);

        // Wait for reg update.
        wait_ret = wait_event_interruptible(reg_update_wq, !MemOutConfig.dirty);
        DISP_LOG("[WaitQ] wait_event_interruptible() ret = %d, %d\n", wait_ret, __LINE__);
    }

    return DISP_STATUS_OK;
}


DISP_STATUS DISP_Capture_Framebuffer( unsigned int pvbuf, unsigned int bpp, unsigned int is_early_suspended )
{
	unsigned int mva;
	unsigned int ret = 0;
	int wait_ret = 0;
	int i;
	BOOL deinit_o2m = TRUE;
	UINT32 ovl_buffer_size,w,h;
	UINT8 *fbv;
	if (DISP_IsDecoupleMode()) {
		w = DISP_GetScreenWidth();
		h = DISP_GetScreenHeight();
		ovl_buffer_size = w*h;
		//FIXME: Just return black buffer if it's security buffer?
		fbv = (UINT8*)ioremap_cached((UINT32)disp_request_buffer(&normal_buffer_queue), ovl_buffer_size*3);

		if(bpp == 32){
			for(i = 0;i < ovl_buffer_size; i++) {
				*(unsigned int*)(pvbuf+i*4) = 0xff000000|fbv[i*3]|(fbv[i*3+1]<<8)|(fbv[i*3+2]<<16);
			}
		}
		if(bpp == 16){
			for(i = 0;i < ovl_buffer_size; i++)
			{
				*(unsigned short*)(pvbuf+i*2) = ((fbv[i*3+0]&0xF8)>>3)|
						((fbv[i*3+1]&0xFC)<<3)|
						((fbv[i*3+2]&0xF8)<<8);
			}
		}
		iounmap(fbv);
		return DISP_STATUS_OK;
	}

	DISP_FUNC();
	for (i=0; i<DDP_OVL_LAYER_MUN; i++)
	{
		//if (cached_layer_config[i].layer_en && cached_layer_config[i].security)
			break;
	}
	if (i < DDP_OVL_LAYER_MUN)
	{
		// There is security layer.
		memset((void*)pvbuf, 0, DISP_GetScreenHeight()*DISP_GetScreenWidth()*bpp/8);
		return DISP_STATUS_OK;
	}
	disp_drv_init_context();

	MMProfileLogEx(MTKFB_MMP_Events.CaptureFramebuffer, MMProfileFlagPulse, 0, pvbuf);
	MMProfileLogEx(MTKFB_MMP_Events.CaptureFramebuffer, MMProfileFlagPulse, 1, bpp);
	ret = disphal_map_overlay_out_buffer(pvbuf, DISP_GetScreenHeight()*DISP_GetScreenWidth()*bpp/8, &mva);
	if(ret!=0)
	{
		printk("disphal_map_overlay_out_buffer fail! \n");
		return DISP_STATUS_OK;
	}
	disphal_init_overlay_to_memory();

	mutex_lock(&MemOutSettingMutex);
	if(bpp == 32)
		MemOutConfig.outFormat = eARGB8888;
	else if(bpp == 16)
		MemOutConfig.outFormat = eRGB565;
	else if(bpp == 24)
		MemOutConfig.outFormat = eRGB888;
	else
	{
		printk("DSI_Capture_FB, fb color format not support\n");
		MemOutConfig.outFormat = eRGB888;
	}

	MemOutConfig.enable = 1;
	MemOutConfig.dstAddr = mva;
	MemOutConfig.srcROI.x = 0;
	MemOutConfig.srcROI.y = 0;
	MemOutConfig.srcROI.height= DISP_GetScreenHeight();
	MemOutConfig.srcROI.width= DISP_GetScreenWidth();

	if (is_early_suspended == 0)
	{
		disp_path_clear_mem_out_done_flag();  // clear last time mem_out_done flag
		MemOutConfig.dirty = 1;
	}

	mutex_unlock(&MemOutSettingMutex);
	MMProfileLogEx(MTKFB_MMP_Events.CaptureFramebuffer, MMProfileFlagPulse, 2, mva);
	if (is_early_suspended)
	{
		disp_path_get_mutex();
		disp_path_config_mem_out_without_lcd(&MemOutConfig);
		disp_path_release_mutex();
		// Wait for mem out done.
		disp_path_wait_mem_out_done();
		MMProfileLogEx(MTKFB_MMP_Events.CaptureFramebuffer, MMProfileFlagPulse, 3, 0);
		MemOutConfig.enable = 0;
		disp_path_get_mutex();
		disp_path_config_mem_out_without_lcd(&MemOutConfig);
		disp_path_release_mutex();
	}
	else
	{
		// Wait for mem out done.
		disp_path_wait_mem_out_done();
		MMProfileLogEx(MTKFB_MMP_Events.CaptureFramebuffer, MMProfileFlagPulse, 3, 0);

		mutex_lock(&MemOutSettingMutex);
		MemOutConfig.enable = 0;
		MemOutConfig.dirty = 1;
		mutex_unlock(&MemOutSettingMutex);
		// Wait for reg update.
		wait_ret = wait_event_interruptible(reg_update_wq, !MemOutConfig.dirty);
		DISP_LOG("[WaitQ] wait_event_interruptible() ret = %d, %d\n", wait_ret, __LINE__);
		MMProfileLogEx(MTKFB_MMP_Events.CaptureFramebuffer, MMProfileFlagPulse, 4, 0);
	}

#if defined(MTK_HDMI_SUPPORT)
	deinit_o2m &= !is_hdmi_active();
#endif

	if(deinit_o2m)
	{
		disphal_deinit_overlay_to_memory();
	}

	disphal_unmap_overlay_out_buffer(pvbuf, DISP_GetScreenHeight()*DISP_GetScreenWidth()*bpp/8, mva);

    return DISP_STATUS_OK;
}

// xuecheng, 2010-09-19
// this api is for mATV signal interfere workaround.
// immediate update == (TE disabled + delay update in overlay mode disabled)
DISP_STATUS DISP_ConfigImmediateUpdate(BOOL enable)
{
    disp_drv_init_context();
    if(enable == TRUE)
    {
        disphal_enable_te(FALSE);
    }
    else
    {
        if(disp_if_drv->init_te_control)
            disp_if_drv->init_te_control();
        else
            return DISP_STATUS_NOT_IMPLEMENTED;
    }
    
    is_immediateupdate = enable;
    
    return DISP_STATUS_OK;
}

BOOL DISP_IsImmediateUpdate(void)
{
    return is_immediateupdate;
}


DISP_STATUS DISP_Get_Default_UpdateSpeed(unsigned int *speed)
{
    return disphal_get_default_updatespeed(speed);
}

DISP_STATUS DISP_Get_Current_UpdateSpeed(unsigned int *speed)
{
    return disphal_get_current_updatespeed(speed);
}

DISP_STATUS DISP_Change_Update(unsigned int speed)
{
    DISP_STATUS ret = DISP_STATUS_OK;

    if (down_interruptible(&sem_update_screen)) {
        DISP_LOG("ERROR: Can't get sem_update_screen in DISP_Change_Update()\n");
        return DISP_STATUS_ERROR;
    }

    ret = disphal_change_updatespeed(speed);
    
    up(&sem_update_screen);

    return ret;
}

extern UINT32 color;
unsigned int DISP_AutoTest()
{
    unsigned int ret = 0;
    if (down_interruptible(&sem_update_screen)) {
        printk("ERROR: Can't get sem_update_screen in DISP_Change_Update()\n");
        return DISP_STATUS_ERROR;
    }
    ret = disphal_check_lcm(color);
    if(LCM_TYPE_DSI == lcm_params->type && lcm_params->dsi.mode != CMD_MODE){
        is_video_mode_running = FALSE;
        needStartEngine = true;
    }
    up(&sem_update_screen);
    return ret;
}

unsigned int DISP_BLS_Query(void)
{
    return disphal_bls_query();
}

void DISP_BLS_Enable(BOOL enable)
{
    disphal_bls_enable(enable);
}

const char* DISP_GetLCMId(void)
{
    if(lcm_drv)
        return lcm_drv->name;
    else
        return NULL;
}


BOOL DISP_EsdCheck(void)
{
    BOOL result = FALSE;

    disp_drv_init_context();
    MMProfileLogEx(MTKFB_MMP_Events.EsdCheck, MMProfileFlagPulse, 0x10, 0);

    if(lcm_drv->esd_check == NULL && disp_if_drv->esd_check == NULL)
    {
        return FALSE;
    }

    if (down_interruptible(&sem_update_screen)) {
        printk("ERROR: Can't get sem_update_screen in DISP_EsdCheck()\n");
        return FALSE;
    }
    MMProfileLogEx(MTKFB_MMP_Events.EsdCheck, MMProfileFlagPulse, 0x11, 0);

    if(is_lcm_in_suspend_mode)
    {
        up(&sem_update_screen);
        return FALSE;
    }

    if(disp_if_drv->esd_check)
        result |= disp_if_drv->esd_check();
    MMProfileLogEx(MTKFB_MMP_Events.EsdCheck, MMProfileFlagPulse, 0x12, 0);

    up(&sem_update_screen);

    return result;
}


BOOL DISP_EsdRecoverCapbility(void)
{
    if(!disp_drv_init_context())
        return FALSE;

    if((lcm_drv->esd_check && lcm_drv->esd_recover) || (lcm_params->dsi.lcm_ext_te_monitor) || (lcm_params->dsi.lcm_int_te_monitor))
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

BOOL DISP_EsdRecover(void)
{
    BOOL result = FALSE;
    DISP_LOG("DISP_EsdRecover enter");

    if(lcm_drv->esd_recover == NULL)
    {
        return FALSE;
    }

    if (down_interruptible(&sem_update_screen)) {
        printk("ERROR: Can't get sem_update_screen in DISP_EsdRecover()\n");
        return FALSE;
    }

    if(is_lcm_in_suspend_mode)
    {
        up(&sem_update_screen);
        return FALSE;
    }

    disphal_wait_not_busy();
    
     DISP_LOG("DISP_EsdRecover do LCM recover");

    // do necessary configuration reset for LCM re-init
    if(disp_if_drv->esd_reset)
        disp_if_drv->esd_reset();

    /// LCM recover
    mutex_lock(&LcmCmdMutex);
    result = lcm_drv->esd_recover();
    mutex_unlock(&LcmCmdMutex);

	if ((lcm_params->type == LCM_TYPE_DSI) && (lcm_params->dsi.mode != CMD_MODE))
    {
        is_video_mode_running = false;
        needStartEngine = true;
    }
    up(&sem_update_screen);
	DISP_UpdateScreen(0, 0, DISP_GetScreenWidth(), DISP_GetScreenHeight());
    return result;
}

unsigned long DISP_GetLCMIndex(void)
{
    return u4IndexOfLCMList;
}

DISP_STATUS DISP_PrepareSuspend(void)
{
    disphal_prepare_suspend();
    // Waiting for de-couple mem_write done, then we can power off
    DISP_WaitMemWriteDoneIfNeeded();
    return DISP_STATUS_OK;
}

DISP_STATUS DISP_GetLayerInfo(DISP_LAYER_INFO *pLayer)
{
    int id = pLayer->id;
    //mutex_lock(&OverlaySettingMutex);
    pLayer->curr_en = captured_layer_config[id].layer_en;
    pLayer->next_en = cached_layer_config[id].layer_en;
    pLayer->hw_en = realtime_layer_config[id].layer_en;
    pLayer->curr_idx = captured_layer_config[id].buff_idx;
    pLayer->next_idx = cached_layer_config[id].buff_idx;
    pLayer->hw_idx = realtime_layer_config[id].buff_idx;
    pLayer->curr_identity = captured_layer_config[id].identity;
    pLayer->next_identity = cached_layer_config[id].identity;
    pLayer->hw_identity = realtime_layer_config[id].identity;
    pLayer->curr_conn_type = captured_layer_config[id].connected_type;
    pLayer->next_conn_type = cached_layer_config[id].connected_type;
    pLayer->hw_conn_type = realtime_layer_config[id].connected_type;
    //mutex_unlock(&OverlaySettingMutex);

    return DISP_STATUS_OK;
}

// This part is for Display Customization Tool Implementation****************/
//#ifdef MTK_DISP_CONFIG_SUPPORT

BOOL fbconfig_dsi_vdo_prepare(void)
{
    if ( !is_early_suspended)
    {
        disp_drv_init_context();
        
        if (down_interruptible(&sem_update_screen)) {
            printk("ERROR: Can't get sem_update_screen in fbconfig_dsi_vdo_prepare()\n");
            return FALSE;
        }
        
        if(is_lcm_in_suspend_mode)
        {
            up(&sem_update_screen);
            return FALSE;
        }
        //to do: set to cmd mode and other preparation....
	    printk("fbconfig=>02will set cmd mode in disp_drv.c!!\n");
        //disphal_fbconfig_dsi_late_prepare();
	    mutex_lock(&Fbconfig_Switch_Mode_Mutex);
        if(fbconfig_if_drv->set_cmd_mode)
            fbconfig_if_drv->set_cmd_mode();
	    mutex_unlock(&Fbconfig_Switch_Mode_Mutex);
	    printk("fbconfig=>exec cmd !!\n");	
        fb_config_execute_cmd();//execute my cmds from config file....
        printk("sxk=>restore to vdo mode !!\n");
        //disphal_fbconfig_dsi_post();
        if(fbconfig_if_drv->set_dsi_post)
            fbconfig_if_drv->set_dsi_post();
        
        up(&sem_update_screen);
        printk("sxk=>will call disphal_update_scn !!\n");
        //disphal_update_screen(lcm_drv, &LcmCmdMutex, 0, 0, DISP_GetScreenWidth(), DISP_GetScreenHeight());
        
        return TRUE;
    }
    else 
        return FALSE;
}

DISP_STATUS DISP_Change_LCM_Resolution(unsigned int width, unsigned int height)
{
	if(lcm_params)
	{
		printk("LCM Resolution will be changed, original: %dx%d, now: %dx%d\n", lcm_params->width, lcm_params->height, width, height);
		if(width >lcm_params->width || height > lcm_params->height || width == 0 || height == 0)
		{
			printk("Invalid resolution: %dx%d\n", width, height);
			return DISP_STATUS_ERROR;
		}

		if(DISP_IsVideoMode())
		{
			printk("Warning!!!Video Mode can't support multiple resolution!\n");
			return DISP_STATUS_ERROR;
		}
			
		lcm_params->width = width;
		lcm_params->height = height;

		return DISP_STATUS_OK;
	}
	else
	{
		return DISP_STATUS_ERROR;
	}
}

//for slt 
DISP_STATUS DISP_Auto_Capture_FB( unsigned int pvbuf, unsigned int wdma_out_fmt,unsigned int bpp, unsigned int is_early_suspended,int wdma_width,int wdma_height)
{
	printk("DISP_Auto_Capture_FB width %d height %d\n",wdma_width,wdma_height);
	if (DISP_IsDecoupleMode()) {
		UINT32 ovl_buffer_size,i,w,h;
	    UINT8 *fbv;
		w = DISP_GetScreenWidth();
	    h = DISP_GetScreenHeight();
		ovl_buffer_size = w*h;
		//FIXME: Just return black buffer if it's security buffer?
		fbv = (UINT8*)ioremap_cached((UINT32)disp_request_buffer(&normal_buffer_queue), ovl_buffer_size*3);

		for(i = 0;i < ovl_buffer_size; i++)
		{
			*(unsigned int*)(pvbuf+i*4) = 0xff000000|fbv[i*3]|(fbv[i*3+1]<<8)|(fbv[i*3+2]<<16);
		}
	    iounmap(fbv);

	} else {
		unsigned int mva;
		unsigned int ret = 0;
		int wait_ret = 0;
		int i;
		BOOL deinit_o2m = TRUE;

		DISP_FUNC();
		for (i=0; i<DDP_OVL_LAYER_MUN; i++)
		{
			//if (cached_layer_config[i].layer_en && cached_layer_config[i].security)
				break;
		}
		if (i < DDP_OVL_LAYER_MUN)
		{
			// There is security layer.
			memset((void*)pvbuf, 0, DISP_GetScreenHeight()*DISP_GetScreenWidth()*bpp/8);
			return DISP_STATUS_OK;
		}
		disp_drv_init_context();

		MMProfileLogEx(MTKFB_MMP_Events.CaptureFramebuffer, MMProfileFlagPulse, 0, pvbuf);
		MMProfileLogEx(MTKFB_MMP_Events.CaptureFramebuffer, MMProfileFlagPulse, 1, bpp);\

		ret = disphal_map_overlay_out_buffer(pvbuf, DISP_GetScreenHeight()*DISP_GetScreenWidth()*bpp/8, &mva);
		if(ret!=0)
		{
			printk("disphal_map_overlay_out_buffer fail! \n");
			return DISP_STATUS_OK;
		}
		disphal_init_overlay_to_memory();

		mutex_lock(&MemOutSettingMutex);
		if(wdma_out_fmt== MTK_FB_FORMAT_ARGB8888)
		{
			MemOutConfig.outFormat=eARGB8888;
		}
		else if(wdma_out_fmt== MTK_FB_FORMAT_RGB888)
		{
			MemOutConfig.outFormat=eRGB888;

		}
		else if(wdma_out_fmt== MTK_FB_FORMAT_RGB565)
		{
			MemOutConfig.outFormat=eRGB565;
		}
		else if(MTK_FB_FORMAT_YUV420_P)
		{
			MemOutConfig.outFormat=eYUV_420_3P;
		}
		else
		{
			printk("DSI_Capture_FB, fb color format not support\n");
			MemOutConfig.outFormat = eRGB888;
		}

		MemOutConfig.enable = 1;
		MemOutConfig.dstAddr = mva;
		MemOutConfig.srcROI.x = 0;
		MemOutConfig.srcROI.y = 0;
		MemOutConfig.srcROI.height= DISP_GetScreenHeight();
		MemOutConfig.srcROI.width= DISP_GetScreenWidth();

		if (is_early_suspended == 0)
		{
			disp_path_clear_mem_out_done_flag();  // clear last time mem_out_done flag
			MemOutConfig.dirty = 1;
		}

		mutex_unlock(&MemOutSettingMutex);
		MMProfileLogEx(MTKFB_MMP_Events.CaptureFramebuffer, MMProfileFlagPulse, 2, mva);
		if (is_early_suspended)
		{
			disp_path_get_mutex();
			disp_path_config_mem_out_without_lcd(&MemOutConfig);
			disp_path_release_mutex();
			// Wait for mem out done.
			disp_path_wait_mem_out_done();
			MMProfileLogEx(MTKFB_MMP_Events.CaptureFramebuffer, MMProfileFlagPulse, 3, 0);
			MemOutConfig.enable = 0;
			disp_path_get_mutex();
			disp_path_config_mem_out_without_lcd(&MemOutConfig);
			disp_path_release_mutex();
		}
		else
		{
			// Wait for mem out done.
			disp_path_wait_mem_out_done();
			MMProfileLogEx(MTKFB_MMP_Events.CaptureFramebuffer, MMProfileFlagPulse, 3, 0);
			mutex_lock(&MemOutSettingMutex);
			MemOutConfig.enable = 0;
			MemOutConfig.dirty = 1;
			mutex_unlock(&MemOutSettingMutex);
			// Wait for reg update.
			wait_ret = wait_event_interruptible(reg_update_wq, !MemOutConfig.dirty);
			DISP_LOG("[WaitQ] wait_event_interruptible() ret = %d, %d\n", wait_ret, __LINE__);
			MMProfileLogEx(MTKFB_MMP_Events.CaptureFramebuffer, MMProfileFlagPulse, 4, 0);
		}

#if defined(MTK_HDMI_SUPPORT)
		deinit_o2m &= !is_hdmi_active();
#endif


		if(deinit_o2m)
		{
			disphal_deinit_overlay_to_memory();
		}

		disphal_unmap_overlay_out_buffer(pvbuf, DISP_GetScreenHeight()*DISP_GetScreenWidth()*bpp/8, mva);
	}

    return DISP_STATUS_OK;
}

/*
called from SET_MIPI_CLK ioctl in debug.c 
Purpose : set mipi clk ,cover cmd mode and video mode
*/
static BOOL fbconfig_disp_set_clk_prepare(unsigned int clk)
{
    if ( !is_early_suspended)
    {
        disp_drv_init_context();
        
        if (down_interruptible(&sem_update_screen)) {
            printk("ERROR: Can't get sem_update_screen in fbconfig_disp_set_clk_prepare()\n");
            return FALSE;
        }
        
        if(is_lcm_in_suspend_mode)
        {
            up(&sem_update_screen);
            return FALSE;
        }
        //to do: set to cmd mode and other preparation....		
        if(fbconfig_if_drv->set_cmd_mode)
            fbconfig_if_drv->set_cmd_mode();
        printk("sxk==>in disp_drv 2124:%d\n",clk);
        if(fbconfig_if_drv->set_mipi_clk)
            fbconfig_if_drv->set_mipi_clk(clk);//execute :clk setting .....	
        if(fbconfig_if_drv->set_dsi_post)
            fbconfig_if_drv->set_dsi_post();
        up(&sem_update_screen);		
        return TRUE;
    }
    else 
        return FALSE;
}

void fbconfig_disp_set_mipi_clk(unsigned int clk)
{
    if(lcm_params->dsi.mode != CMD_MODE)
    {
        if (down_interruptible(&sem_early_suspend)) {
            printk("sxk=>can't get semaphore in fbconfig_disp_set_mipi_clk()\n");
            return ;
        }
        
        fbconfig_disp_set_clk_prepare(clk);	
        up(&sem_early_suspend);
    }
    else{//cmd mode
        if (down_interruptible(&sem_early_suspend)) {
            printk("sxk=>can't get semaphore in fbconfig_disp_set_mipi_clk()\n");
            return ;
        }
        if(fbconfig_if_drv->set_mipi_clk)
            fbconfig_if_drv->set_mipi_clk(clk);//execute :clk setting .....	
        up(&sem_early_suspend);
    }
}

static BOOL fbconfig_disp_set_ssc_prepare(unsigned int ssc)
{
    if ( !is_early_suspended)
	{
		disp_drv_init_context();		
		
		if (down_interruptible(&sem_update_screen)) {
			printk("ERROR: Can't get sem_update_screen in fbconfig_disp_set_ssc_prepare()\n");
			return FALSE;
		}	
		if(is_lcm_in_suspend_mode)
		{
			up(&sem_update_screen);
			return FALSE;
		}
		//to do: set to cmd mode and other preparation....		
		if(fbconfig_if_drv->set_cmd_mode)
			fbconfig_if_drv->set_cmd_mode();
		printk("sxk==>in disp_drv 2124:%d\n",ssc);
		if(fbconfig_if_drv->set_spread_frequency)
			fbconfig_if_drv->set_spread_frequency(ssc);//execute :clk setting .....	
				
		if(fbconfig_if_drv->set_dsi_post)
			fbconfig_if_drv->set_dsi_post();		
			up(&sem_update_screen);		
			return TRUE;
		}
		else 
		return FALSE;
}

void fbconfig_disp_set_mipi_ssc(unsigned int ssc)
{
	if(lcm_params->dsi.mode != CMD_MODE)
	{	
	    if (down_interruptible(&sem_early_suspend)) {
					printk("sxk=>can't get semaphore in fbconfig_disp_set_mipi_ssc()\n");
					return ;
		}

	    fbconfig_disp_set_ssc_prepare(ssc);	
	    up(&sem_early_suspend);
	}
	else{//cmd mode
    	if (down_interruptible(&sem_early_suspend)) {
    				printk("sxk=>can't get semaphore in fbconfig_disp_set_mipi_ssc()\n");
    				return ;
    	}
    	if(fbconfig_if_drv->set_spread_frequency)
    			fbconfig_if_drv->set_spread_frequency(ssc);//execute :ssc setting .....	
    	up(&sem_early_suspend);
	}
}


void fbconfig_disp_set_mipi_lane_num(unsigned int lane_num)
{
    if(fbconfig_if_drv->set_lane_num)
        fbconfig_if_drv->set_lane_num(lane_num);
}

void fbconfig_disp_set_mipi_timing(MIPI_TIMING timing)
{
    if(fbconfig_if_drv->set_mipi_timing)
        fbconfig_if_drv->set_mipi_timing(timing);
}

unsigned int fbconfig_get_layer_info(FBCONFIG_LAYER_INFO *layers)
{
    int i =0 ;
    for(i=0;i<4;i++)
    {
        layers->layer_enable[i]= captured_layer_config[i].layer_en ;
        //layers->layer_size[i] = captured_layer_config[i].src_pitch * captured_layer_config[i].dst_h ;
    }
    return 0 ;
}


unsigned int fbconfig_get_layer_vaddr(int layer_id,int * layer_size,int * enable)
{
    *enable = captured_layer_config[layer_id].layer_en ;
    if(*enable ==0)
 	{
 	    *layer_size =0;
	    return 0 ;
 	}
    else{
 	    *layer_size = captured_layer_config[layer_id].src_pitch * captured_layer_config[layer_id].dst_h;
	    return captured_layer_config[layer_id].addr;
 	}
    return 0;
}

unsigned int fbconfig_get_layer_height(int layer_id,int * layer_size,int * enable,int* height ,int * fmt)
{
    *enable = captured_layer_config[layer_id].layer_en ;
    if(*enable ==0)
    {
      *layer_size =0;
      *height =0 ;
      return 0 ;
    }else{
      *height = captured_layer_config[layer_id].dst_h;
      *layer_size = captured_layer_config[layer_id].src_pitch * captured_layer_config[layer_id].dst_h;
      *fmt = captured_layer_config[layer_id].fmt ;
      return 0;
    }
    return 0;
}

static int fbconfig_disp_get_esd_prepare(void)
{
    int ret =0;
    if ( !is_early_suspended)
    {
        disp_drv_init_context();
        
        if (down_interruptible(&sem_update_screen)) {
            printk("ERROR: Can't get sem_update_screen in fbconfig_disp_get_esd_prepare()\n");
            ret = -2;
        }
        
        if(is_lcm_in_suspend_mode)
        {
            up(&sem_update_screen);
            ret = -2;
        }
        //to do: set to cmd mode and other preparation....		
        if(fbconfig_if_drv->set_cmd_mode)
            fbconfig_if_drv->set_cmd_mode();
        ret = fbconfig_get_esd_check_exec();//execute :esd check...	
        if(fbconfig_if_drv->set_dsi_post)
            fbconfig_if_drv->set_dsi_post();
        up(&sem_update_screen);
        return ret;
    }
    else 
        return -2;
}


int fbconfig_get_esd_check(void)
{
	int ret=0 ;
	if(lcm_params->dsi.mode != CMD_MODE)
	{	
	if (down_interruptible(&sem_early_suspend)) {
					printk("sxk=>can't get semaphore in fbconfig_get_esd_check()\n");
					ret= -2;
			   }		
	ret = fbconfig_disp_get_esd_prepare();
	up(&sem_early_suspend);
	}
	else{//cmd mode
	if (down_interruptible(&sem_early_suspend)) {
				printk("sxk=>can't get semaphore in fbconfig_get_esd_check()\n");
				ret= -2;
		   }
	ret =fbconfig_get_esd_check_exec();//execute :esd check...	
	up(&sem_early_suspend);		 
	}
	return ret ;
}


BOOL fbconfig_rest_lcm_setting_prepare(void)
{
    if ( !is_early_suspended)
    {
        disp_drv_init_context();
        
        if (down_interruptible(&sem_update_screen)) {
            printk("ERROR: Can't get sem_update_screen in fbconfig_rest_lcm_setting_prepare()\n");
            return FALSE;
        }
        
        if(is_lcm_in_suspend_mode)
        {
            up(&sem_update_screen);
            return FALSE;
        }
        //to do: set to cmd mode and other preparation....	
        if(fbconfig_if_drv->set_cmd_mode)
            fbconfig_if_drv->set_cmd_mode();
        printk("sxk=>exec cmd !!\n");	
        lcm_drv->init();//execute my cmds from config file....
        printk("sxk=>restore to vdo mode !!\n");
        if(fbconfig_if_drv->set_dsi_post)
            fbconfig_if_drv->set_dsi_post();
        up(&sem_update_screen); 
        return TRUE;
    }
    else 
        return FALSE;
}

//#endif
