#include <generated/autoconf.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/earlysuspend.h>
#include <linux/kthread.h>
#include <linux/rtpm_prio.h>
#include <linux/vmalloc.h>
#include <linux/disp_assert_layer.h>
#include <linux/semaphore.h>
#include <linux/xlog.h>
#include <linux/mutex.h>
#include <linux/leds-mt65xx.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <asm/atomic.h>
#include <asm/mach-types.h>
#include <asm/cacheflush.h>
#include <asm/io.h>

#include <mach/dma.h>
#include <mach/irqs.h>
#include <linux/dma-mapping.h>

#include "mach/mt_boot.h"

#include "debug.h"
#include "disp_drv.h"
#include "ddp_hal.h"
#include "disp_drv_log.h"
#include "disp_hal.h"

#include "mtkfb.h"
#include "mtkfb_console.h"
#include "mtkfb_info.h"
#include "ddp_ovl.h"
#include "disp_drv_platform.h"
#include <linux/aee.h>
// Fence Sync Object
#if defined (MTK_FB_SYNC_SUPPORT)
#include "disp_sync_svp.h"
#endif
#include "disp_mgr.h"
#include "disp_svp.h"

//#define FPGA_DEBUG_PAN
#define ALIGN_TO(x, n)  \
	(((x) + ((n) - 1)) & ~((n) - 1))

unsigned int EnableVSyncLog = 0;

static u32 MTK_FB_XRES  = 0;
static u32 MTK_FB_YRES  = 0;
static u32 MTK_FB_BPP   = 0;
static u32 MTK_FB_PAGES = 0;
static u32 fb_xres_update = 0;
static u32 fb_yres_update = 0;

#define MTK_FB_XRESV (ALIGN_TO(MTK_FB_XRES, disphal_get_fb_alignment()))
#define MTK_FB_YRESV (ALIGN_TO(MTK_FB_YRES, disphal_get_fb_alignment()) * MTK_FB_PAGES) /* For page flipping */
#define MTK_FB_BYPP  ((MTK_FB_BPP + 7) >> 3)
#define MTK_FB_LINE  (ALIGN_TO(MTK_FB_XRES, disphal_get_fb_alignment()) * MTK_FB_BYPP)
#define MTK_FB_SIZE  (MTK_FB_LINE * ALIGN_TO(MTK_FB_YRES, disphal_get_fb_alignment()))

#define MTK_FB_SIZEV (MTK_FB_LINE * ALIGN_TO(MTK_FB_YRES, disphal_get_fb_alignment()) * MTK_FB_PAGES)

#define CHECK_RET(expr)    \
    do {                   \
        int ret = (expr);  \
        ASSERT(0 == ret);  \
    } while (0)


static size_t mtkfb_log_on = false;
#define MTKFB_LOG(fmt, arg...) \
    do { \
        if (mtkfb_log_on) DISP_LOG_PRINT(ANDROID_LOG_WARN, "MTKFB", fmt, ##arg); \
    }while (0)
#define MTKFB_ERR(fmt, arg...) \
    do { \
        DISP_LOG_PRINT(ANDROID_LOG_ERROR, "MTKFB", "error(%d):"fmt, __LINE__, ##arg); \
    }while (0)
#define MTKFB_FUNC()	\
	do { \
		if(mtkfb_log_on) DISP_LOG_PRINT(ANDROID_LOG_INFO, "MTKFB", "[Func]%s\n", __func__); \
	}while (0)
#define MTKFB_LOG_D(fmt, arg...) \
	do { \
		DISP_LOG_PRINT(ANDROID_LOG_DEBUG, "MTKFB", fmt, ##arg); \
	}while (0)
#define MTKFB_LOG_D_IF(con,fmt, arg...) \
	do { \
		if(con) DISP_LOG_PRINT(ANDROID_LOG_DEBUG, "MTKFB", fmt, ##arg); \
	}while (0)

#define PRNERR(fmt, args...)   DISP_LOG_PRINT(ANDROID_LOG_INFO, "MTKFB", fmt, ## args);

void mtkfb_log_enable(int enable)
{
    mtkfb_log_on = enable;
	MTKFB_LOG("mtkfb log %s\n", enable?"enabled":"disabled");
}
void mtkfb_clear_lcm(void);
// ---------------------------------------------------------------------------
//  local variables
// ---------------------------------------------------------------------------
#ifdef MTKFB_FPGA_ONLY
static BOOL mtkfb_enable_mmu = FALSE;
#else
static BOOL mtkfb_enable_mmu = TRUE;
#endif

unsigned int fb_pa = 0;
unsigned int decouple_addr = 0;	// It's PA = MVA after m4u mapping
unsigned int decouple_size = 0;
extern OVL_CONFIG_STRUCT cached_layer_config[DDP_OVL_LAYER_MUN];

static const struct timeval FRAME_INTERVAL = {0, 30000};  // 33ms

atomic_t has_pending_update = ATOMIC_INIT(0);
struct fb_overlay_layer video_layerInfo;
UINT32 dbr_backup = 0;
UINT32 dbg_backup = 0;
UINT32 dbb_backup = 0;
bool fblayer_dither_needed = false;
static unsigned int video_rotation = 0;
//static UINT32 mtkfb_using_layer_type = LAYER_2D;
//static bool	hwc_force_fb_enabled = true;
bool is_ipoh_bootup = false;
struct fb_info         *mtkfb_fbi;
struct fb_overlay_layer fb_layer_context;

/* This mutex is used to prevent tearing due to page flipping when adbd is
   reading the front buffer
*/
DEFINE_SEMAPHORE(sem_flipping);
DEFINE_SEMAPHORE(sem_early_suspend);
DEFINE_SEMAPHORE(sem_overlay_buffer);

extern OVL_CONFIG_STRUCT cached_layer_config[DDP_OVL_LAYER_MUN];
DEFINE_MUTEX(OverlaySettingMutex);
atomic_t OverlaySettingDirtyFlag = ATOMIC_INIT(0);
atomic_t OverlaySettingApplied = ATOMIC_INIT(0);

unsigned int PanDispSettingPending = 0;
unsigned int PanDispSettingDirty = 0;
unsigned int PanDispSettingApplied = 0;

DECLARE_WAIT_QUEUE_HEAD(reg_update_wq);

unsigned int need_esd_check = 0;
DECLARE_WAIT_QUEUE_HEAD(esd_check_wq);

extern unsigned int disp_running;
extern wait_queue_head_t disp_done_wq;

DEFINE_MUTEX(ScreenCaptureMutex);

BOOL is_early_suspended = FALSE;
static int sem_flipping_cnt = 1;
static int sem_early_suspend_cnt = 1;
static int sem_overlay_buffer_cnt = 1;
static int vsync_cnt = 0;

extern BOOL is_engine_in_suspend_mode;
extern BOOL is_lcm_in_suspend_mode;

extern unsigned int is_video_mode_running;
extern unsigned int isAEEEnabled;
extern spinlock_t DispConfigLock;
extern disp_session_config disp_config;
// ---------------------------------------------------------------------------
//  local function declarations
// ---------------------------------------------------------------------------

static int init_framebuffer(struct fb_info *info);
static int mtkfb_set_overlay_layer(struct fb_info *info, struct fb_overlay_layer* layerInfo);
static int mtkfb_get_overlay_layer_info(struct fb_overlay_layer_info* layerInfo);
static int mtkfb_update_screen(struct fb_info *info);
static void mtkfb_update_screen_impl(void);
unsigned int mtkfb_fm_auto_test(void);
#if defined(MTK_HDMI_SUPPORT)
extern void hdmi_setorientation(int orientation);
extern void	MTK_HDMI_Set_Security_Output(int enable);
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void mtkfb_late_resume(struct early_suspend *h);
static void mtkfb_early_suspend(struct early_suspend *h);
#endif
// ---------------------------------------------------------------------------
//  Timer Routines
// ---------------------------------------------------------------------------
static struct task_struct *screen_update_task = NULL;
static struct task_struct *esd_recovery_task = NULL;
unsigned int lcd_fps = 6000;
extern BOOL dal_shown;

void mtkfb_pan_disp_test(void)
{
    MTKFB_FUNC();
    if (down_interruptible(&sem_flipping)) {
        printk("[fb driver] can't get semaphore:%d\n", __LINE__);
        return;
    }
    sem_flipping_cnt--;
    DISP_LOG_PRINT(ANDROID_LOG_WARN, "MTKFB", "wait sem_flipping\n");
    if (down_interruptible(&sem_early_suspend)) {
        printk("[fb driver] can't get semaphore:%d\n", __LINE__);
        sem_flipping_cnt++;
        up(&sem_flipping);
        return;
    }
    sem_early_suspend_cnt--;
    
    DISP_LOG_PRINT(ANDROID_LOG_WARN, "MTKFB", "wait sem_early_suspend\n");
    if (down_interruptible(&sem_overlay_buffer)) {
        printk("[fb driver] can't get semaphore,%d\n", __LINE__);
        sem_early_suspend_cnt++;
        up(&sem_early_suspend);
        
        sem_flipping_cnt++;
        up(&sem_flipping);
        return;
    }
    sem_overlay_buffer_cnt--;
    DISP_LOG_PRINT(ANDROID_LOG_WARN, "MTKFB", "wait sem_overlay_buffer\n");
    if (is_early_suspended) goto end;
    
end:
    sem_overlay_buffer_cnt++;
    sem_early_suspend_cnt++;
    sem_flipping_cnt++;
    up(&sem_overlay_buffer);
    up(&sem_early_suspend);
    up(&sem_flipping);
}

void mtkfb_show_sem_cnt(void)
{
    printk("[FB driver: sem cnt = %d, %d, %d. fps = %d, vsync_cnt = %d\n", sem_overlay_buffer_cnt, sem_early_suspend_cnt, sem_flipping_cnt, lcd_fps, vsync_cnt);
    printk("[FB driver: sem cnt = %d, %d, %d\n", sem_overlay_buffer.count, sem_early_suspend.count, sem_flipping.count);
}

void mtkfb_hang_test(bool en)
{
    MTKFB_FUNC();
    if(en){
        if (down_interruptible(&sem_flipping)) {
            printk("[fb driver] can't get semaphore:%d\n", __LINE__);
            return;
        }
        sem_flipping_cnt--;
    }
    else{
        sem_flipping_cnt++;
        up(&sem_flipping);
    }
}

BOOL esd_kthread_pause = TRUE;

void esd_recovery_pause(BOOL en)
{
    esd_kthread_pause = en;
}

static int esd_recovery_kthread(void *data)
{
    //struct sched_param param = { .sched_priority = RTPM_PRIO_SCRN_UPDATE };
    //sched_setscheduler(current, SCHED_RR, &param);
    MTKFB_LOG("enter esd_recovery_kthread()\n");
    for( ;; ) {

        if (kthread_should_stop())
            break;

        MTKFB_LOG("sleep start in esd_recovery_kthread()\n");
        msleep_interruptible(2000);       //2s
        MTKFB_LOG("sleep ends in esd_recovery_kthread()\n");

        if(!esd_kthread_pause)
        {
            if(is_early_suspended)
            {
                MTKFB_LOG("is_early_suspended in esd_recovery_kthread()\n");
                continue;
            }
            ///execute ESD check and recover flow
            MTKFB_LOG("DISP_EsdCheck starts\n");
            need_esd_check = 1;
            wait_event_interruptible(esd_check_wq, !need_esd_check);
            MTKFB_LOG("DISP_EsdCheck ends\n");
       }
    }


    MTKFB_LOG("exit esd_recovery_kthread()\n");
    return 0;
}


/*
 * ---------------------------------------------------------------------------
 *  mtkfb_set_lcm_inited() will be called in mt6516_board_init()
 * ---------------------------------------------------------------------------
 */
BOOL is_lcm_inited = FALSE;
void mtkfb_set_lcm_inited(BOOL inited)
{
    is_lcm_inited = inited;
}

unsigned long long fb_address_lk = 0;
unsigned long long fb_size_lk    = 0;
void mtkfb_set_fb_lk(unsigned long long address, unsigned long long size)
{
    fb_address_lk = address;
    fb_size_lk    = size;
}

unsigned int mtkfb_fb_lk_copy_size(void)
{
	return DISP_GetFBRamSize()/DISP_GetPages();
}

/*
 * ---------------------------------------------------------------------------
 * fbdev framework callbacks and the ioctl interface
 * ---------------------------------------------------------------------------
 */
/* Called each time the mtkfb device is opened */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0)
static int mtkfb_open(struct file *file, struct fb_info *info, int user)
#else
static int mtkfb_open(struct fb_info *info, int user)
#endif
{
    NOT_REFERENCED(info);
    NOT_REFERENCED(user);

    MSG_FUNC_ENTER();
    MSG_FUNC_LEAVE();
    return 0;
}

/* Called when the mtkfb device is closed. We make sure that any pending
 * gfx DMA operations are ended, before we return. */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0)
static int mtkfb_release(struct file *file, struct fb_info *info, int user)
#else
static int mtkfb_release(struct fb_info *info, int user)
#endif
{
    NOT_REFERENCED(info);
    NOT_REFERENCED(user);

    MSG_FUNC_ENTER();
    MSG_FUNC_LEAVE();
    return 0;
}

/* Store a single color palette entry into a pseudo palette or the hardware
 * palette if one is available. For now we support only 16bpp and thus store
 * the entry only to the pseudo palette.
 */
static int mtkfb_setcolreg(u_int regno, u_int red, u_int green,
                           u_int blue, u_int transp,
                           struct fb_info *info)
{
    int r = 0;
    unsigned bpp, m;

    NOT_REFERENCED(transp);

    MSG_FUNC_ENTER();

    bpp = info->var.bits_per_pixel;
    m = 1 << bpp;
    if (regno >= m)
    {
        r = -EINVAL;
        goto exit;
    }

    switch (bpp)
    {
    case 16:
        /* RGB 565 */
        ((u32 *)(info->pseudo_palette))[regno] =
            ((red & 0xF800) |
            ((green & 0xFC00) >> 5) |
            ((blue & 0xF800) >> 11));
        break;
    case 32:
        /* ARGB8888 */
        ((u32 *)(info->pseudo_palette))[regno] =
             (0xff000000)           |
            ((red   & 0xFF00) << 8) |
            ((green & 0xFF00)     ) |
            ((blue  & 0xFF00) >> 8);
        break;

    // TODO: RGB888, BGR888, ABGR8888

    default:
        ASSERT(0);
    }

exit:
    MSG_FUNC_LEAVE();
    return r;
}

static void mtkfb_update_screen_impl(void)
{
    BOOL down_sem = FALSE;
    MMProfileLog(MTKFB_MMP_Events.UpdateScreenImpl, MMProfileFlagStart);
    if (down_interruptible(&sem_overlay_buffer)) {
        printk("[FB Driver] can't get semaphore in mtkfb_update_screen_impl()\n");
    }
    else{
        down_sem = TRUE;
        sem_overlay_buffer_cnt--;
    }

    DISP_CHECK_RET(DISP_UpdateScreen(0, 0, fb_xres_update, fb_yres_update));
    
    if(down_sem){
        sem_overlay_buffer_cnt++;
        up(&sem_overlay_buffer);
    }
    MMProfileLog(MTKFB_MMP_Events.UpdateScreenImpl, MMProfileFlagEnd);
}


static int mtkfb_update_screen(struct fb_info *info)
{
    if (down_interruptible(&sem_early_suspend)) {
        printk("[FB Driver] can't get semaphore in mtkfb_update_screen()\n");
        return -ERESTARTSYS;
    }
    sem_early_suspend_cnt--;
    if (is_early_suspended) goto End;
    mtkfb_update_screen_impl();

End:
    sem_early_suspend_cnt++;
    up(&sem_early_suspend);
    return 0;
}
static unsigned int BL_level = 0;
static BOOL BL_set_level_resume = FALSE;
int mtkfb_set_backlight_level(unsigned int level)
{
    printk("mtkfb_set_backlight_level:%d\n", level);
    if (down_interruptible(&sem_flipping)) {
        printk("[FB Driver] can't get semaphore:%d\n", __LINE__);
        return -ERESTARTSYS;
    }
    sem_flipping_cnt--;
    if (down_interruptible(&sem_early_suspend)) {
        printk("[FB Driver] can't get semaphore:%d\n", __LINE__);
        sem_flipping_cnt++;
        up(&sem_flipping);
        return -ERESTARTSYS;
    }
    
    sem_early_suspend_cnt--;
    if (is_early_suspended){
        BL_level = level;
        BL_set_level_resume = TRUE;
        printk("[FB driver] set backlight level but FB has been suspended\n");
        goto End;
    }
    DISP_SetBacklight(level);
    BL_set_level_resume = FALSE;
End:
    sem_flipping_cnt++;
    sem_early_suspend_cnt++;
    up(&sem_early_suspend);
    up(&sem_flipping);
    return 0;
}
EXPORT_SYMBOL(mtkfb_set_backlight_level);

int mtkfb_set_backlight_mode(unsigned int mode)
{
    if (down_interruptible(&sem_flipping)) {
        printk("[FB Driver] can't get semaphore:%d\n", __LINE__);
        return -ERESTARTSYS;
    }
    sem_flipping_cnt--;
    if (down_interruptible(&sem_early_suspend)) {
        printk("[FB Driver] can't get semaphore:%d\n", __LINE__);
        sem_flipping_cnt++;
        up(&sem_flipping);
        return -ERESTARTSYS;
    }
    
    sem_early_suspend_cnt--;
    if (is_early_suspended) goto End;
    
    DISP_SetBacklight_mode(mode);
End:
    sem_flipping_cnt++;
    sem_early_suspend_cnt++;
    up(&sem_early_suspend);
    up(&sem_flipping);
    return 0;
}
EXPORT_SYMBOL(mtkfb_set_backlight_mode);


int mtkfb_set_backlight_pwm(int div)
{
    if (down_interruptible(&sem_flipping)) {
        printk("[FB Driver] can't get semaphore:%d\n", __LINE__);
        return -ERESTARTSYS;
    }
    sem_flipping_cnt--;
    if (down_interruptible(&sem_early_suspend)) {
        printk("[FB Driver] can't get semaphore:%d\n", __LINE__);
        sem_flipping_cnt++;
        up(&sem_flipping);
        return -ERESTARTSYS;
    }
    sem_early_suspend_cnt--;
    if (is_early_suspended) goto End;
    DISP_SetPWM(div);
End:
    sem_flipping_cnt++;
    sem_early_suspend_cnt++;
    up(&sem_early_suspend);
    up(&sem_flipping);
    return 0;
}
EXPORT_SYMBOL(mtkfb_set_backlight_pwm);

int mtkfb_get_backlight_pwm(int div, unsigned int *freq)
{
    DISP_GetPWM(div, freq);
    return 0;
}
EXPORT_SYMBOL(mtkfb_get_backlight_pwm);

void mtkfb_waitVsync(void)
{
	if(is_early_suspended){
		printk("[MTKFB_VSYNC]:mtkfb has suspend, return directly\n");
		msleep(20);
		return;
	}
	vsync_cnt++;
#ifndef MTKFB_FPGA_ONLY
	DISP_WaitVSYNC();
#else
	msleep(20);
#endif
	vsync_cnt--;
	return;
}
EXPORT_SYMBOL(mtkfb_waitVsync);
/* Used for HQA test */
/*-------------------------------------------------------------
   Note: The using scenario must be
         1. switch normal mode to factory mode when LCD screen is on
         2. switch factory mode to normal mode(optional)
-------------------------------------------------------------*/
static struct fb_var_screeninfo    fbi_var_backup;
static struct fb_fix_screeninfo    fbi_fix_backup;
static BOOL                         need_restore = FALSE;
static int mtkfb_set_par(struct fb_info *fbi);
void mtkfb_switch_normal_to_factory(void)
{
    if (down_interruptible(&sem_flipping)) {
        printk("[FB Driver] can't get semaphore:%d\n", __LINE__);
        return;
    }
    sem_flipping_cnt--;
    if (down_interruptible(&sem_early_suspend)) {
        printk("[FB Driver] can't get semaphore:%d\n", __LINE__);
        sem_flipping_cnt++;
        up(&sem_flipping);
        return;
    }
    sem_early_suspend_cnt--;
    if (is_early_suspended) {
        goto EXIT;
    }

    if (mtkfb_fbi)
    {
        memcpy(&fbi_var_backup, &mtkfb_fbi->var, sizeof(fbi_var_backup));
        memcpy(&fbi_fix_backup, &mtkfb_fbi->fix, sizeof(fbi_fix_backup));
        need_restore = TRUE;
    }

EXIT:
    sem_early_suspend_cnt++;
    sem_flipping_cnt++;
    up(&sem_early_suspend);
    up(&sem_flipping);
}

/* Used for HQA test */
void mtkfb_switch_factory_to_normal(void)
{
    BOOL need_set_par = FALSE;
    if (down_interruptible(&sem_flipping)) {
        printk("[FB Driver] can't get semaphore in mtkfb_switch_factory_to_normal()\n");
        return;
    }
    sem_flipping_cnt--;
    if (down_interruptible(&sem_early_suspend)) {
        printk("[FB Driver] can't get semaphore in mtkfb_switch_factory_to_normal()\n");
        sem_flipping_cnt++;
        up(&sem_flipping);
        return;
    }

    sem_early_suspend_cnt--;
    if (is_early_suspended) {
        goto EXIT;
    }

    if ((mtkfb_fbi) && (need_restore))
    {
        memcpy(&mtkfb_fbi->var, &fbi_var_backup, sizeof(fbi_var_backup));
        memcpy(&mtkfb_fbi->fix, &fbi_fix_backup, sizeof(fbi_fix_backup));
        need_restore = FALSE;
        need_set_par = TRUE;
    }

EXIT:
    sem_early_suspend_cnt++;
    sem_flipping_cnt++;
    up(&sem_early_suspend);
    up(&sem_flipping);
    if (need_set_par)
    {
        int ret;
        ret = mtkfb_set_par(mtkfb_fbi);
        if (ret != 0)
            PRNERR("failed to mtkfb_set_par\n");
    }
}

static bool first_update = false;
static bool first_enable_esd = true;
static int cnt=3;
static int mtkfb_pan_display_impl(struct fb_var_screeninfo *var, struct fb_info *info)
{
    UINT32 offset;
    UINT32 paStart;
    char *vaStart, *vaEnd;
    int ret = 0;
	int wait_ret = 0;
	disp_job *job;
	if(first_update){
		first_update = false;
		return ret;
	}
	MMProfileLog(MTKFB_MMP_Events.PanDisplay, MMProfileFlagStart);
	if(0!=cnt){
		printk("LCD:%dx%d\n",MTK_FB_XRES,MTK_FB_YRES);
		cnt--;
	}

    MSG(ARGU, "xoffset=%u, yoffset=%u, xres=%u, yres=%u, xresv=%u, yresv=%u\n",
        var->xoffset, var->yoffset,
        info->var.xres, info->var.yres,
        info->var.xres_virtual,
        info->var.yres_virtual);

    if (down_interruptible(&sem_flipping)) {
        printk("[FB Driver] can't get semaphore in mtkfb_pan_display_impl()\n");
        MMProfileLogMetaString(MTKFB_MMP_Events.PanDisplay, MMProfileFlagEnd, "Can't get semaphore in mtkfb_pan_display_impl()");
        return -ERESTARTSYS;
    }
    sem_flipping_cnt--;

    info->var.yoffset = var->yoffset;

    offset = var->yoffset * info->fix.line_length;
    paStart = fb_pa + offset;
    vaStart = info->screen_base + offset;
    vaEnd   = vaStart + info->var.yres * info->fix.line_length;

	job = disp_deque_job(disp_config.session_id);
    mutex_lock(&job->lock);
    job->input[FB_LAYER].layer_id = FB_LAYER;
    job->input[FB_LAYER].address = paStart;
    //job->input[layer].vaddr = (unsigned int)vaStart;
    job->input[FB_LAYER].layer_enable = 1;
    job->input[FB_LAYER].dirty = 1;
    job->input[FB_LAYER].width = var->xres;
    job->input[FB_LAYER].height = var->yres;
    {
        unsigned int layerpitch;
        unsigned int src_pitch = ALIGN_TO(var->xres, disphal_get_fb_alignment());
        switch(var->bits_per_pixel)
        {
        case 16:
        	job->input[FB_LAYER].format = eRGB565;
            layerpitch = 2;
            job->input[FB_LAYER].alpha_enable = FALSE;
            break;
        case 24:
        	job->input[FB_LAYER].format = eRGB888;
            layerpitch = 3;
            job->input[FB_LAYER].alpha_enable = FALSE;
            break;
        case 32:
        	job->input[FB_LAYER].format = ePARGB8888;
            layerpitch = 4;
            job->input[FB_LAYER].alpha_enable = TRUE;
            break;
        default:
            PRNERR("Invalid color format bpp: 0x%d\n", var->bits_per_pixel);
            job->input[FB_LAYER].dirty = 0;
            mutex_unlock(&job->lock);
            MMProfileLogEx(MTKFB_MMP_Events.PanDisplay, MMProfileFlagEnd, 0, var->bits_per_pixel);
            return -1;
        }
        job->input[FB_LAYER].alpha = 0xFF;
        job->input[FB_LAYER].pitch = src_pitch * layerpitch;
    }
    mutex_unlock(&job->lock);

    MMProfileLogStructure(MTKFB_MMP_Events.PanDisplay, MMProfileFlagPulse, &job->input[FB_LAYER], input_config);
    PanDispSettingPending = 1;
    PanDispSettingDirty = 1;
    PanDispSettingApplied = 0;

    ret = disp_enque_job(disp_config.session_id);
    is_ipoh_bootup = false;

    if (DISP_IsDecoupleMode()) {
    	DISP_StartOverlayTransfer();
    } else {
    	ret = mtkfb_update_screen(info);
    }

	// NOTICE: un-interruptible wait here for m4u callback
    wait_ret = wait_event_timeout(reg_update_wq, PanDispSettingApplied, HZ/10);
    MTKFB_LOG("[WaitQ] wait_event_interruptible() ret = %d, %d\n", wait_ret, __LINE__);

    sem_flipping_cnt++;
    up(&sem_flipping);
    if(first_enable_esd)
    {
        esd_recovery_pause(FALSE);
        first_enable_esd = false;
    }
    MMProfileLog(MTKFB_MMP_Events.PanDisplay, MMProfileFlagEnd);

    return ret;
}


static int mtkfb_pan_display_proxy(struct fb_var_screeninfo *var, struct fb_info *info)
{
#ifdef CONFIG_MTPROF_APPLAUNCH  // eng enable, user disable
    LOG_PRINT(ANDROID_LOG_INFO, "AppLaunch", "mtkfb_pan_display_proxy.\n");
#endif
    return mtkfb_pan_display_impl(var, info);
}


/* Set fb_info.fix fields and also updates fbdev.
 * When calling this fb_info.var must be set up already.
 */
static void set_fb_fix(struct mtkfb_device *fbdev)
{
    struct fb_info           *fbi   = fbdev->fb_info;
    struct fb_fix_screeninfo *fix   = &fbi->fix;
    struct fb_var_screeninfo *var   = &fbi->var;
    struct fb_ops            *fbops = fbi->fbops;

    strncpy(fix->id, MTKFB_DRIVER, sizeof(fix->id));
    fix->type = FB_TYPE_PACKED_PIXELS;

    switch (var->bits_per_pixel)
    {
    case 16:
    case 24:
    case 32:
        fix->visual = FB_VISUAL_TRUECOLOR;
        break;
    case 1:
    case 2:
    case 4:
    case 8:
        fix->visual = FB_VISUAL_PSEUDOCOLOR;
        break;
    default:
        ASSERT(0);
    }

    fix->accel       = FB_ACCEL_NONE;
    fix->line_length = ALIGN_TO(var->xres_virtual, disphal_get_fb_alignment()) * var->bits_per_pixel / 8;
    fix->smem_len    = fbdev->fb_size_in_byte;
    fix->smem_start  = fbdev->fb_pa_base;

    fix->xpanstep = 0;
    fix->ypanstep = 1;

    fbops->fb_fillrect  = cfb_fillrect;
    fbops->fb_copyarea  = cfb_copyarea;
    fbops->fb_imageblit = cfb_imageblit;
}


/* Check values in var, try to adjust them in case of out of bound values if
 * possible, or return error.
 */
static int mtkfb_check_var(struct fb_var_screeninfo *var, struct fb_info *fbi)
{
    unsigned int bpp;
    unsigned long max_frame_size;
    unsigned long line_size;

    struct mtkfb_device *fbdev = (struct mtkfb_device *)fbi->par;

    MSG_FUNC_ENTER();

    MSG(ARGU, "xres=%u, yres=%u, xres_virtual=%u, yres_virtual=%u, "
              "xoffset=%u, yoffset=%u, bits_per_pixel=%u)\n",
        var->xres, var->yres, var->xres_virtual, var->yres_virtual,
        var->xoffset, var->yoffset, var->bits_per_pixel);

    bpp = var->bits_per_pixel;

    if (bpp != 16 && bpp != 24 && bpp != 32) {
        MTKFB_LOG("[%s]unsupported bpp: %d", __func__, bpp);
        return -1;
    }

    switch (var->rotate) {
    case 0:
    case 180:
        var->xres = MTK_FB_XRES;
        var->yres = MTK_FB_YRES;
        break;
    case 90:
    case 270:
        var->xres = MTK_FB_YRES;
        var->yres = MTK_FB_XRES;
        break;
    default:
        return -1;
    }

    if (var->xres_virtual < var->xres)
        var->xres_virtual = var->xres;
    if (var->yres_virtual < var->yres)
        var->yres_virtual = var->yres;

    max_frame_size = fbdev->fb_size_in_byte;
    line_size = var->xres_virtual * bpp / 8;

    if (line_size * var->yres_virtual > max_frame_size) {
        /* Try to keep yres_virtual first */
        line_size = max_frame_size / var->yres_virtual;
        var->xres_virtual = line_size * 8 / bpp;
        if (var->xres_virtual < var->xres) {
            /* Still doesn't fit. Shrink yres_virtual too */
            var->xres_virtual = var->xres;
            line_size = var->xres * bpp / 8;
            var->yres_virtual = max_frame_size / line_size;
        }
    }
    if (var->xres + var->xoffset > var->xres_virtual)
        var->xoffset = var->xres_virtual - var->xres;
    if (var->yres + var->yoffset > var->yres_virtual)
        var->yoffset = var->yres_virtual - var->yres;

    if (16 == bpp) {
        var->red.offset    = 11;  var->red.length    = 5;
        var->green.offset  =  5;  var->green.length  = 6;
        var->blue.offset   =  0;  var->blue.length   = 5;
        var->transp.offset =  0;  var->transp.length = 0;
    }
    else if (24 == bpp)
    {
        var->red.length = var->green.length = var->blue.length = 8;
        var->transp.length = 0;

        // Check if format is RGB565 or BGR565

        ASSERT(8 == var->green.offset);
        ASSERT(16 == var->red.offset + var->blue.offset);
        ASSERT(16 == var->red.offset || 0 == var->red.offset);
    }
    else if (32 == bpp)
    {
        var->red.length = var->green.length =
        var->blue.length = var->transp.length = 8;

        // Check if format is ARGB565 or ABGR565

        ASSERT(8 == var->green.offset && 24 == var->transp.offset);
        ASSERT(16 == var->red.offset + var->blue.offset);
        ASSERT(16 == var->red.offset || 0 == var->red.offset);
    }

    var->red.msb_right = var->green.msb_right =
    var->blue.msb_right = var->transp.msb_right = 0;

    var->activate = FB_ACTIVATE_NOW;

    var->height    = UINT_MAX;
    var->width     = UINT_MAX;
    var->grayscale = 0;
    var->nonstd    = 0;

    var->pixclock     = UINT_MAX;
    var->left_margin  = UINT_MAX;
    var->right_margin = UINT_MAX;
    var->upper_margin = UINT_MAX;
    var->lower_margin = UINT_MAX;
    var->hsync_len    = UINT_MAX;
    var->vsync_len    = UINT_MAX;

    var->vmode = FB_VMODE_NONINTERLACED;
    var->sync  = 0;

    MSG_FUNC_LEAVE();
    return 0;
}


/* Switch to a new mode. The parameters for it has been check already by
 * mtkfb_check_var.
 */
static int mtkfb_set_par(struct fb_info *fbi)
{
    struct fb_var_screeninfo *var = &fbi->var;
    struct mtkfb_device *fbdev = (struct mtkfb_device *)fbi->par;
    struct fb_overlay_layer fb_layer;
    u32 bpp = var->bits_per_pixel;
    extern LCM_PARAMS *lcm_params;

    MSG_FUNC_ENTER();
    // No need for IPO-H reboot, or white screen flash will happen
    if(is_ipoh_bootup && (lcm_params->type == LCM_TYPE_DSI && lcm_params->dsi.mode != CMD_MODE))
    {
      printk("mtkfb_set_par return in IPOH!!!\n");
      goto Done;
    }
    memset(&fb_layer, 0, sizeof(struct fb_overlay_layer));
    switch(bpp)
    {
    case 16 :
        fb_layer.src_fmt = MTK_FB_FORMAT_RGB565;
        fb_layer.src_use_color_key = 1;
        fb_layer.src_color_key = 0xFF000000;
        break;

    case 24 :
        fb_layer.src_use_color_key = 1;
        fb_layer.src_fmt = (0 == var->blue.offset) ?
                           MTK_FB_FORMAT_RGB888 :
                           MTK_FB_FORMAT_BGR888;
        fb_layer.src_color_key = 0xFF000000;
        break;

    case 32 :
        fb_layer.src_use_color_key = 0;
        fb_layer.src_fmt = (0 == var->blue.offset) ?
                           MTK_FB_FORMAT_ARGB8888 :
                           MTK_FB_FORMAT_ABGR8888;
        fb_layer.src_color_key = 0;
        break;

    default :
        fb_layer.src_fmt = MTK_FB_FORMAT_UNKNOWN;
        MTKFB_LOG("[%s]unsupported bpp: %d", __func__, bpp);
        return -1;
    }

    // If the framebuffer format is NOT changed, nothing to do
    //
    if (fb_layer.src_fmt == fbdev->layer_format[FB_LAYER]) {
        goto Done;
    }

    // else, begin change display mode
    //
    set_fb_fix(fbdev);

    fb_layer.layer_id = FB_LAYER;
    fb_layer.layer_enable = 1;
    fb_layer.src_base_addr = (void *)((unsigned long)fbdev->fb_va_base + var->yoffset * fbi->fix.line_length);
    fb_layer.src_phy_addr = (void *)(fb_pa + var->yoffset * fbi->fix.line_length);
    fb_layer.src_direct_link = 0;
    fb_layer.src_offset_x = fb_layer.src_offset_y = 0;
//    fb_layer.src_width = fb_layer.tgt_width = fb_layer.src_pitch = var->xres;
#if defined(HWGPU_SUPPORT)
    fb_layer.src_pitch = ALIGN_TO(var->xres, MTK_FB_ALIGNMENT);
#else
    if(get_boot_mode() == META_BOOT || get_boot_mode() == FACTORY_BOOT
       || get_boot_mode() == ADVMETA_BOOT || get_boot_mode() == RECOVERY_BOOT)
        fb_layer.src_pitch = ALIGN_TO(var->xres, MTK_FB_ALIGNMENT);
    else
        fb_layer.src_pitch = var->xres;
#endif
    fb_layer.src_width = fb_layer.tgt_width = var->xres;
    fb_layer.src_height = fb_layer.tgt_height = var->yres;
    fb_layer.tgt_offset_x = fb_layer.tgt_offset_y = 0;

//    fb_layer.src_color_key = 0;
    fb_layer.layer_rotation = MTK_FB_ORIENTATION_0;
    fb_layer.layer_type = LAYER_2D;

    mtkfb_set_overlay_layer(fbi, &fb_layer);

    // backup fb_layer information.
    memcpy(&fb_layer_context, &fb_layer, sizeof(fb_layer));

Done:
    MSG_FUNC_LEAVE();
    return 0;
}


static int mtkfb_soft_cursor(struct fb_info *info, struct fb_cursor *cursor)
{
    NOT_REFERENCED(info);
    NOT_REFERENCED(cursor);

    return 0;
}
extern DAL_STATUS DAL_Dynamic_Change_FB_Layer(unsigned int isAEEEnabled);
static int mtkfb_set_overlay_layer(struct fb_info *info, struct fb_overlay_layer* layerInfo)
{
    struct mtkfb_device *fbdev = (struct mtkfb_device *)info->par;

    unsigned int layerpitch;
    unsigned int layerbpp;
    unsigned int id = layerInfo->layer_id;
    int enable = layerInfo->layer_enable ? 1 : 0;
    int ret = 0;
    disp_job *job = disp_deque_job(disp_config.session_id);
    mutex_lock(&job->lock);
    job->input[id].layer_id = id;

    MMProfileLogEx(MTKFB_MMP_Events.SetOverlayLayer, MMProfileFlagStart, (id<<16)|enable, layerInfo->next_buff_idx);

    MTKFB_LOG_D("L%d set_overlay:%d,%d\n", layerInfo->layer_id, layerInfo->layer_enable, layerInfo->next_buff_idx);

    // Update Layer Enable Bits and Layer Config Dirty Bits
    if ((((fbdev->layer_enable >> id) & 1) ^ enable)) {
        fbdev->layer_enable ^= (1 << id);
        fbdev->layer_config_dirty |= MTKFB_LAYER_ENABLE_DIRTY;
    }

    // Update Layer Format and Layer Config Dirty Bits
    if (fbdev->layer_format[id] != layerInfo->src_fmt) {
        fbdev->layer_format[id] = layerInfo->src_fmt;
        fbdev->layer_config_dirty |= MTKFB_LAYER_FORMAT_DIRTY;
    }

    // Enter Overlay Mode if any layer is enabled except the FB layer

    if(fbdev->layer_enable & ((1 << VIDEO_LAYER_COUNT)-1)){
        if (DISP_STATUS_OK == DISP_EnterOverlayMode()) {
            MTKFB_LOG("mtkfb_ioctl(MTKFB_ENABLE_OVERLAY)\n");
        }
    }

    if (!enable)
    {
    	job->input[id].layer_enable = enable;
    	job->input[id].dirty = true;
        ret = 0;
        goto LeaveOverlayMode;
    }

    switch (layerInfo->src_fmt)
    {
    case MTK_FB_FORMAT_YUV422:
    	job->input[id].format = eYUY2;
        layerpitch = 2;
        layerbpp = 24;
        break;

    case MTK_FB_FORMAT_RGB565:
    	job->input[id].format = eRGB565;
        layerpitch = 2;
        layerbpp = 16;
        break;

    case MTK_FB_FORMAT_RGB888:
    	job->input[id].format = eRGB888;
        layerpitch = 3;
        layerbpp = 24;
        break;
    case MTK_FB_FORMAT_BGR888:
    	job->input[id].format = eBGR888;
        layerpitch = 3;
        layerbpp = 24;
        break;

    case MTK_FB_FORMAT_ARGB8888:
    	job->input[id].format = ePARGB8888;
        layerpitch = 4;
        layerbpp = 32;
        break;
    case MTK_FB_FORMAT_ABGR8888:
    	job->input[id].format = ePABGR8888;
        layerpitch = 4;
        layerbpp = 32;
        break;
    case MTK_FB_FORMAT_XRGB8888:
    	job->input[id].format = eARGB8888;
        layerpitch = 4;
        layerbpp = 32;
        break;
    case MTK_FB_FORMAT_XBGR8888:
    	job->input[id].format = eABGR8888;
        layerpitch = 4;
        layerbpp = 32;
        break;
	case MTK_FB_FORMAT_UYVY:
		job->input[id].format = eUYVY;
        layerpitch = 2;
        layerbpp = 16;
        break;
    default:
        PRNERR("Invalid color format: 0x%x\n", layerInfo->src_fmt);
        ret = -EFAULT;
        goto LeaveOverlayMode;
    }
    job->input[id].security = layerInfo->security;
#if defined (MTK_FB_SYNC_SUPPORT)
    if (layerInfo->src_phy_addr != NULL) {
    	job->input[id].address = (unsigned int)layerInfo->src_phy_addr;
    } else {
    	job->input[id].address = disp_sync_query_buffer_mva(job->group_id, layerInfo->layer_id, (unsigned int)layerInfo->next_buff_idx);
    }
#else
    job->input[id].address = (unsigned int)layerInfo->src_phy_addr;
#endif
    job->input[id].index = layerInfo->next_buff_idx;

{
#if defined(MTK_HDMI_SUPPORT)
    int tl = 0;
    int cnt_security_layer = 0;
    for(tl=0;tl<HW_OVERLAY_COUNT;tl++)
    {
        cnt_security_layer += job->input[tl].security;
    }
    MTKFB_LOG("Totally %d security layer is set now\n", cnt_security_layer);
    MTK_HDMI_Set_Security_Output(!!cnt_security_layer);
#endif
}
	/**
	 * NOT USED now
    job->input[id].tdshp = layerInfo->isTdshp;
	job->input[id].identity = layerInfo->identity;
	job->input[id].connected_type = layerInfo->connected_type;
	job->input[id].sharp = layerInfo->isTdshp;
	*/
    //set Alpha blending
    if (layerInfo->alpha_enable) {
    	job->input[id].alpha_enable = TRUE;
    	job->input[id].alpha = layerInfo->alpha;
    } else {
    	job->input[id].alpha_enable = FALSE;
    }
    if (MTK_FB_FORMAT_ARGB8888 == layerInfo->src_fmt ||
        MTK_FB_FORMAT_ABGR8888 == layerInfo->src_fmt)
    {
    	job->input[id].alpha_enable = TRUE;
        job->input[id].alpha = 0xff;
    }

	// xuecheng, for slt debug
	if(!strcmp(current->comm, "display_slt"))
	{
		job->input[id].alpha_enable = FALSE;
		isAEEEnabled = 1;
		DAL_Dynamic_Change_FB_Layer(isAEEEnabled); // default_ui_ layer coniig to changed_ui_layer
	}

    //set src width, src height
	job->input[id].src_x = layerInfo->src_offset_x;
	job->input[id].src_y = layerInfo->src_offset_y;
	job->input[id].dst_x = layerInfo->tgt_offset_x;
	job->input[id].dst_y = layerInfo->tgt_offset_y;

	if (layerInfo->src_width != layerInfo->tgt_width || layerInfo->src_height != layerInfo->tgt_height) {
		MTKFB_ERR("OVL cannot support clip:src(%d,%d), dst(%d,%d)\n", layerInfo->src_width, layerInfo->src_height, layerInfo->tgt_width, layerInfo->tgt_height);
	}
	job->input[id].width = layerInfo->tgt_width;
	job->input[id].height = layerInfo->tgt_height;
	job->input[id].pitch = layerInfo->src_pitch*layerpitch;

#if defined(DITHERING_SUPPORT)
    {
        bool ditherenabled = false;
        UINT32 ditherbpp = DISP_GetOutputBPPforDithering();
        UINT32 dbr = 0;
        UINT32 dbg = 0;
        UINT32 dbb = 0;
        
        if(ditherbpp < layerbpp)
        {
            if(ditherbpp == 16)
            {
                if(layerbpp == 18)
                {
                    dbr = 1;
                    dbg = 0;
                    dbb = 1;
                    ditherenabled = true;
                }
                else if(layerbpp == 24 || layerbpp == 32)
                {
                    dbr = 2;
                    dbg = 1;
                    dbb = 2;
                    ditherenabled = true;
                }
                else
                {
                    MTKFB_LOG("ERROR, error dithring bpp settings\n");
                }
            }
            else if(ditherbpp == 18)
            {
                if(layerbpp == 24 || layerbpp == 32)
                {
                    dbr = 1;
                    dbg = 1;
                    dbb = 1;
                    ditherenabled = true;
                }
                else
                {
                    MTKFB_LOG("ERROR, error dithring bpp settings\n");
                    ASSERT(0);
                }
            }
            else if(ditherbpp == 24)
            {
                // do nothing here.
            }
            else
            {
                MTKFB_LOG("ERROR, error dithering bpp settings, diterbpp = %d\n",ditherbpp);
                ASSERT(0);
            }
            
            if(ditherenabled)
            {
                //LCD_CHECK_RET(LCD_LayerEnableDither(id, true));
                DISP_ConfigDither(14, 14, 14, dbr, dbg, dbb);
                if(FB_LAYER == id)
                {
                    dbr_backup = dbr;dbg_backup = dbg;dbb_backup = dbb;
                    fblayer_dither_needed = ditherenabled;
                    MTKFB_LOG("[FB driver] dither enabled:%d, dither bit(%d,%d,%d)\n", fblayer_dither_needed, dbr_backup, dbg_backup, dbb_backup);
                }
            }
        }
        else
        {
            // no dithering needed.
        }
    }
#endif

    if(0 == strncmp(MTK_LCM_PHYSICAL_ROTATION, "180", 3))
    {
        layerInfo->layer_rotation = (layerInfo->layer_rotation + MTK_FB_ORIENTATION_180) % 4;
        layerInfo->tgt_offset_x = MTK_FB_XRES - (layerInfo->tgt_offset_x + layerInfo->tgt_width);
        layerInfo->tgt_offset_y = MTK_FB_YRES - (layerInfo->tgt_offset_y + layerInfo->tgt_height);
    }

    video_rotation = layerInfo->video_rotation;

    //set color key
    job->input[id].color_key = layerInfo->src_color_key;
    job->input[id].color_key_enable = layerInfo->src_use_color_key;

    job->input[id].layer_enable= enable;
    job->input[id].dirty = TRUE;

LeaveOverlayMode:
	// Lock/unlock same as RCU to reduce R/W conflicting time
	mutex_unlock(&job->lock);
    // Leave Overlay Mode if only FB layer is enabled
    if ((fbdev->layer_enable & ((1 << VIDEO_LAYER_COUNT)-1)) == 0)
    {
        if (DISP_STATUS_OK == DISP_LeaveOverlayMode())
        {
            MTKFB_LOG("mtkfb_ioctl(MTKFB_DISABLE_OVERLAY)\n");
            if(fblayer_dither_needed)
            {
                DISP_ConfigDither(14, 14, 14, dbr_backup, dbg_backup, dbb_backup);
            }
        }
    }

    MSG_FUNC_LEAVE();
    MMProfileLog(MTKFB_MMP_Events.SetOverlayLayer, MMProfileFlagEnd);

    return ret;
}

static int mtkfb_get_overlay_layer_info(struct fb_overlay_layer_info* layerInfo)
{
    DISP_LAYER_INFO layer;
    if (layerInfo->layer_id >= DDP_OVL_LAYER_MUN)
    {
         return 0;
    }
    layer.id = layerInfo->layer_id;
    DISP_GetLayerInfo(&layer);
    layerInfo->layer_enabled = layer.hw_en;
    layerInfo->curr_en = layer.curr_en;
    layerInfo->next_en = layer.next_en;
    layerInfo->hw_en = layer.hw_en;
    layerInfo->curr_idx = layer.curr_idx;
    layerInfo->next_idx = layer.next_idx;
    layerInfo->hw_idx = layer.hw_idx;
    layerInfo->curr_identity = layer.curr_identity;
    layerInfo->next_identity = layer.next_identity;
    layerInfo->hw_identity = layer.hw_identity;
    layerInfo->curr_conn_type = layer.curr_conn_type;
    layerInfo->next_conn_type = layer.next_conn_type;
    layerInfo->hw_conn_type = layer.hw_conn_type;
#if 0
    MTKFB_LOG("[FB Driver] mtkfb_get_overlay_layer_info():id=%u, layer en=%u, next_en=%u, curr_en=%u, hw_en=%u, next_idx=%u, curr_idx=%u, hw_idx=%u \n",
    		layerInfo->layer_id,
    		layerInfo->layer_enabled,
    		layerInfo->next_en,
    		layerInfo->curr_en,
    		layerInfo->hw_en,
    		layerInfo->next_idx,
    		layerInfo->curr_idx,
    		layerInfo->hw_idx);
#endif
    MMProfileLogEx(MTKFB_MMP_Events.LayerInfo[layerInfo->layer_id], MMProfileFlagPulse, (layerInfo->next_idx<<16)+((layerInfo->curr_idx)&0xFFFF), (layerInfo->hw_idx<<16)+(layerInfo->next_en<<8)+(layerInfo->curr_en<<4)+layerInfo->hw_en);
    return 0;
}


static atomic_t capture_ui_layer_only = ATOMIC_INIT(0); /* when capturing framebuffer ,whether capture ui layer only */
void mtkfb_capture_fb_only(bool enable)
{
    atomic_set(&capture_ui_layer_only, enable);
}

static int mtkfb_auto_capture_framebuffer(struct fb_info *info,struct fb_slt_catpure *config)
{
    int ret = 0;
    unsigned int pvbuf = (unsigned int)config->outputBuffer;
    unsigned int bpp = 32;

    printk("mtkfb_auto_capture_framebuffer\n");

    MMProfileLogEx(MTKFB_MMP_Events.CaptureFramebuffer, MMProfileFlagStart, pvbuf, 0);
    MTKFB_FUNC();

    if (down_interruptible(&sem_flipping)) 
    {
        printk("[FB Driver] can't get semaphore:%d\n", __LINE__);
        MMProfileLogEx(MTKFB_MMP_Events.CaptureFramebuffer, MMProfileFlagEnd, 0, 1);

        return -ERESTARTSYS;
    }
    sem_flipping_cnt--;
    mutex_lock(&ScreenCaptureMutex);
    
    //LCD registers can't be R/W when its clock is gated in early suspend
    //mode; power on/off LCD to modify register values before/after func.
    
    if (is_early_suspended) 
    {
        // Turn on engine clock.
        disp_path_clock_on("mtkfb");
        //DISP_CHECK_RET(DISP_PowerEnable(TRUE));
    }
    //because wdma can't change width and height
    //DISP_Capture_Framebuffer(pvbuf, bpp, is_early_suspended);
    DISP_Auto_Capture_FB(pvbuf, config->format,bpp, is_early_suspended,config->wdma_width,config->wdma_height);
    if (is_early_suspended)
    {
        // Turn off engine clock.
        //DISP_CHECK_RET(DISP_PowerEnable(FALSE));
        disp_path_clock_off("mtkfb");
    }
    
    mutex_unlock(&ScreenCaptureMutex);
    sem_flipping_cnt++;
    up(&sem_flipping);
    MSG_FUNC_LEAVE();
    MMProfileLogEx(MTKFB_MMP_Events.CaptureFramebuffer, MMProfileFlagEnd, 0, 0);
    
    return ret;
}
static int mtkfb_capture_framebuffer(struct fb_info *info, unsigned int pvbuf)
{
    int ret = 0;

    MMProfileLogEx(MTKFB_MMP_Events.CaptureFramebuffer, MMProfileFlagStart, pvbuf, 0);

    if (down_interruptible(&sem_flipping)) 
    {
        printk("[FB Driver] can't get semaphore:%d\n", __LINE__);
        MMProfileLogEx(MTKFB_MMP_Events.CaptureFramebuffer, MMProfileFlagEnd, 0, 1);
        return -ERESTARTSYS;
    }
    sem_flipping_cnt--;
    mutex_lock(&ScreenCaptureMutex);

    /** LCD registers can't be R/W when its clock is gated in early suspend
        mode; power on/off LCD to modify register values before/after func.
    */
    if (is_early_suspended)
    {
        memset((void*)pvbuf, 0, DISP_GetScreenWidth()*DISP_GetScreenHeight()*info->var.bits_per_pixel/8);
        goto EXIT;
    }

    if (atomic_read(&capture_ui_layer_only))
    {
        unsigned int w_xres = (unsigned short)fb_layer_context.src_width;
        unsigned int h_yres = (unsigned short)fb_layer_context.src_height;
        unsigned int pixel_bpp = info->var.bits_per_pixel / 8; // bpp is either 32 or 16, can not be other value
        unsigned int w_fb = (unsigned int)fb_layer_context.src_pitch;
        unsigned int fbsize = w_fb * h_yres * pixel_bpp; // frame buffer size
        unsigned int fbaddress = info->fix.smem_start + info->var.yoffset * info->fix.line_length; //physical address
        unsigned int mem_off_x = (unsigned short)fb_layer_context.src_offset_x;
        unsigned int mem_off_y = (unsigned short)fb_layer_context.src_offset_y;
        unsigned int fbv = 0;
        fbaddress += (mem_off_y * w_fb + mem_off_x) * pixel_bpp;
        fbv = (unsigned int)ioremap_nocache(fbaddress, fbsize);
        MTKFB_LOG("[FB Driver], w_xres = %d, h_yres = %d, w_fb = %d, pixel_bpp = %d, fbsize = %d, fbaddress = 0x%08x\n", w_xres, h_yres, w_fb, pixel_bpp, fbsize, fbaddress);
        if (!fbv)
        {
            MTKFB_LOG("[FB Driver], Unable to allocate memory for frame buffer: address=0x%08x, size=0x%08x\n", \
                    fbaddress, fbsize);
            goto EXIT;
        }
        {
            unsigned int i;
            for(i = 0;i < h_yres; i++)
            {
                memcpy((void *)(pvbuf + i * w_xres * pixel_bpp), (void *)(fbv + i * w_fb * pixel_bpp), w_xres * pixel_bpp);
            }
        }
        iounmap((void *)fbv);
    }
    else
        DISP_Capture_Framebuffer(pvbuf, info->var.bits_per_pixel, is_early_suspended);


EXIT:

    mutex_unlock(&ScreenCaptureMutex);
    sem_flipping_cnt++;
    up(&sem_flipping);
    MSG_FUNC_LEAVE();
    MMProfileLogEx(MTKFB_MMP_Events.CaptureFramebuffer, MMProfileFlagEnd, 0, 0);

    return ret;
}


#include <linux/aee.h>
#define mtkfb_aee_print(string, args...) do{\
    aee_kernel_warning_api(__FILE__, __LINE__, DB_OPT_MMPROFILE_BUFFER, "sf-mtkfb blocked", string, ##args);  \
}while(0)

void mtkfb_dump_layer_info(void)
{
#if 0
	unsigned int i;
	printk("[mtkfb] start dump layer info, early_suspend=%d \n", is_early_suspended);
	printk("[mtkfb] cache(next): \n");
	for(i=0;i<4;i++)
	{
		printk("[mtkfb] layer=%d, layer_en=%d, idx=%d, fmt=%d, addr=0x%x, %d, %d, %d \n ", 
	    cached_layer_config[i].layer,   // layer
	    cached_layer_config[i].layer_en,
	    cached_layer_config[i].buff_idx, 
	    cached_layer_config[i].fmt, 
	    cached_layer_config[i].addr, // addr 	     
	    cached_layer_config[i].identity,  
	    cached_layer_config[i].connected_type, 
	    cached_layer_config[i].security);	
	}
  
  printk("[mtkfb] captured(current): \n");
	for(i=0;i<4;i++)
	{
		printk("[mtkfb] layer=%d, layer_en=%d, idx=%d, fmt=%d, addr=0x%x, %d, %d, %d \n ", 
	    captured_layer_config[i].layer,   // layer
	    captured_layer_config[i].layer_en,
	    captured_layer_config[i].buff_idx,  
	    captured_layer_config[i].fmt, 
	    captured_layer_config[i].addr, // addr 
	    captured_layer_config[i].identity,  
	    captured_layer_config[i].connected_type, 
	    captured_layer_config[i].security);	
	}
  printk("[mtkfb] realtime(hw): \n");
	for(i=0;i<4;i++)
	{
		printk("[mtkfb] layer=%d, layer_en=%d, idx=%d, fmt=%d, addr=0x%x, %d, %d, %d \n ", 
	    realtime_layer_config[i].layer,   // layer
	    realtime_layer_config[i].layer_en,
	    realtime_layer_config[i].buff_idx,  
	    realtime_layer_config[i].fmt, 
	    realtime_layer_config[i].addr, // addr 
	    realtime_layer_config[i].identity,  
	    realtime_layer_config[i].connected_type, 
	    realtime_layer_config[i].security);	
	}
	    
	// dump mmp data
	//mtkfb_aee_print("surfaceflinger-mtkfb blocked");
#endif
}

mtk_dispif_info_t dispif_info[MTKFB_MAX_DISPLAY_COUNT];

#if LINUX_VERSION_CODE > KERNEL_VERSION(3,7,0)
static int mtkfb_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg)
#else
static int mtkfb_ioctl(struct file *file, struct fb_info *info, unsigned int cmd, unsigned long arg)
#endif
{
    void __user *argp = (void __user *)arg;
    DISP_STATUS ret = 0;
    int r = 0;

	/// M: dump debug mmprofile log info
	MMProfileLogEx(MTKFB_MMP_Events.IOCtrl, MMProfileFlagPulse, _IOC_NR(cmd), arg);
	//printk("mtkfb_ioctl, info=0x%08x, cmd=0x%08x, arg=0x%08x\n", (unsigned int)info, (unsigned int)cmd, (unsigned int)arg);

    switch (cmd)
    {
    	case MTKFB_GET_FRAMEBUFFER_MVA:
        	return copy_to_user(argp, &fb_pa,  sizeof(fb_pa)) ? -EFAULT : 0;
	case MTKFB_GET_DISPLAY_IF_INFORMATION:
	{
		int displayid = 0;
		if (copy_from_user(&displayid, (void __user *)arg, sizeof(displayid))) {
			MTKFB_ERR("copy_from_user failed!\n");
			return -EFAULT;
		}
		printk("%s, display_id=%d\n", __func__, displayid);
		if (displayid > MTKFB_MAX_DISPLAY_COUNT) {
			MTKFB_LOG("[FB]: invalid display id:%d \n", displayid);
			return -EFAULT;
		}
		dispif_info[displayid].physicalHeight = DISP_GetPhysicalHeight();
		dispif_info[displayid].physicalWidth = DISP_GetPhysicalWidth() ;
		if (copy_to_user((void __user *)arg, &(dispif_info[displayid]),  sizeof(mtk_dispif_info_t))) {
			MTKFB_ERR("copy_to_user failed!\n");
			r = -EFAULT;
		}
		return (r);
	}
	case MTKFB_POWEROFF:
   	{
#ifdef CONFIG_HAS_EARLYSUSPEND
		if(is_early_suspended) return r;
        mtkfb_early_suspend(0);
#endif
		return r;
	}

	case MTKFB_POWERON:
   	{
#ifdef CONFIG_HAS_EARLYSUSPEND
		if(!is_early_suspended) return r;
		mtkfb_late_resume(0);
		if (!lcd_fps)
		{
            msleep(30);
    	}
        else
    	{
            msleep(2*100000/lcd_fps); // Delay 2 frames.
    	}
#ifndef MTKFB_FPGA_ONLY
		mt65xx_leds_brightness_set(MT65XX_LED_TYPE_LCD,127);
#endif
#endif
		return r;
	}
    case MTKFB_GET_POWERSTATE:
    {
        unsigned long power_state;

        if(is_early_suspended == TRUE)
            power_state = 0;
        else
            power_state = 1;

        return copy_to_user(argp, &power_state,  sizeof(power_state)) ? -EFAULT : 0;
    }

    case MTKFB_CONFIG_IMMEDIATE_UPDATE:
    {
        MTKFB_LOG("[%s] MTKFB_CONFIG_IMMEDIATE_UPDATE, enable = %lu\n",
            __func__, arg);
		if (down_interruptible(&sem_early_suspend)) {
			MTKFB_ERR("can't get semaphore\n");
        		return -ERESTARTSYS;
    	}
		sem_early_suspend_cnt--;
        DISP_WaitForLCDNotBusy();
        ret = DISP_ConfigImmediateUpdate((BOOL)arg);
		sem_early_suspend_cnt++;
		up(&sem_early_suspend);
        return (r);
    }

    case MTKFB_CAPTURE_FRAMEBUFFER:
    {
        unsigned int pbuf = 0;
        if (copy_from_user(&pbuf, (void __user *)arg, sizeof(pbuf)))
        {
        	MTKFB_ERR("copy_from_user failed!\n");
            r = -EFAULT;
        }
        else
        {
            mtkfb_capture_framebuffer(info, pbuf);
        }

        return (r);
    }

    case MTKFB_GET_OVERLAY_LAYER_INFO:
    {
        struct fb_overlay_layer_info layerInfo;
        MTKFB_LOG(" mtkfb_ioctl():MTKFB_GET_OVERLAY_LAYER_INFO\n");

        if (copy_from_user(&layerInfo, (void __user *)arg, sizeof(layerInfo))) {
        	MTKFB_ERR("copy_from_user failed!\n");
            return -EFAULT;
        }
        if (mtkfb_get_overlay_layer_info(&layerInfo) < 0)
        {
            MTKFB_LOG("[FB]: Failed to get overlay layer info\n");
            return -EFAULT;
        }
        if (copy_to_user((void __user *)arg, &layerInfo, sizeof(layerInfo))) {
        	MTKFB_ERR("copy_to_user failed!\n");
            r = -EFAULT;
        }
        return (r);
    }
    case MTKFB_SET_OVERLAY_LAYER:
    {
        struct fb_overlay_layer layerInfo;
        MTKFB_LOG_D(" mtkfb_ioctl():MTKFB_SET_OVERLAY_LAYER\n");

        if (copy_from_user(&layerInfo, (void __user *)arg, sizeof(layerInfo))) {
        	MTKFB_ERR("copy_from_user failed!\n");
            r = -EFAULT;
        } 
        else 
        {
            //in early suspend mode ,will not update buffer index, info SF by return value
            if(is_early_suspended == TRUE)
            {
                printk("[FB] error, set overlay in early suspend ,skip! \n");
                return MTKFB_ERROR_IS_EARLY_SUSPEND;
            }
            
            mtkfb_set_overlay_layer(info, &layerInfo);
            disp_enque_job(disp_config.session_id);
            if (DISP_IsDecoupleMode()) 
            {
                DISP_StartOverlayTransfer();
            }
            if (is_ipoh_bootup)
            {
                is_ipoh_bootup = false;
            }
        }
        return (r);
    }

    case MTKFB_ERROR_INDEX_UPDATE_TIMEOUT:
    {
        printk("[DDP] mtkfb_ioctl():MTKFB_ERROR_INDEX_UPDATE_TIMEOUT  \n");
        // call info dump function here
        mtkfb_dump_layer_info();
        return (r);
    }

    case MTKFB_ERROR_INDEX_UPDATE_TIMEOUT_AEE:
    {
        printk("[DDP] mtkfb_ioctl():MTKFB_ERROR_INDEX_UPDATE_TIMEOUT  \n");
        // call info dump function here
        mtkfb_dump_layer_info();
        mtkfb_aee_print("surfaceflinger-mtkfb blocked");
        return (r);
    }
        
    case MTKFB_SET_VIDEO_LAYERS:
    {
        struct mmp_fb_overlay_layers
        {
            struct fb_overlay_layer Layer0;
            struct fb_overlay_layer Layer1;
            struct fb_overlay_layer Layer2;
            struct fb_overlay_layer Layer3;
        };
        struct fb_overlay_layer layerInfo[VIDEO_LAYER_COUNT];
        MTKFB_LOG(" mtkfb_ioctl():MTKFB_SET_VIDEO_LAYERS\n");
        MMProfileLog(MTKFB_MMP_Events.SetVideoLayers, MMProfileFlagStart);
        if (copy_from_user(&layerInfo, (void __user *)arg, sizeof(layerInfo))) {
            MTKFB_ERR("copy_from_user failed!\n");
            MMProfileLogMetaString(MTKFB_MMP_Events.SetVideoLayers, MMProfileFlagEnd, "Copy_from_user failed!");
            r = -EFAULT;
        } else {
            int32_t i;
            for (i = 0; i < VIDEO_LAYER_COUNT; ++i) {
                mtkfb_set_overlay_layer(info, &layerInfo[i]);
            }
            disp_enque_job(disp_config.session_id);
    		if (DISP_IsDecoupleMode()) {
            	DISP_StartOverlayTransfer();
            }
            is_ipoh_bootup = false;
            MMProfileLogStructure(MTKFB_MMP_Events.SetVideoLayers, MMProfileFlagEnd, layerInfo, struct mmp_fb_overlay_layers);
        }

        return (r);
    }

    case MTKFB_SET_MULTIPLE_LAYERS:
    {
        struct mmp_fb_overlay_layers
        {
            struct fb_overlay_layer Layer0;
            struct fb_overlay_layer Layer1;
            struct fb_overlay_layer Layer2;
            struct fb_overlay_layer Layer3;
        };
        struct fb_overlay_layer layerInfo[HW_OVERLAY_COUNT];

        MTKFB_LOG(" mtkfb_ioctl():MTKFB_SET_MULTIPLE_LAYERS\n");
        MMProfileLog(MTKFB_MMP_Events.SetMultipleLayers, MMProfileFlagStart);

        if (copy_from_user(&layerInfo, (void __user *)arg, sizeof(layerInfo))) 
        {
            MTKFB_ERR("copy_from_user failed!\n");
            MMProfileLogMetaString(MTKFB_MMP_Events.SetMultipleLayers, MMProfileFlagEnd, "Copy_from_user failed!");
            r = -EFAULT;
        } 
        else 
        {
            int32_t i, layerId;

            for (i = 0; i < HW_OVERLAY_COUNT; ++i) 
            {
                layerId = layerInfo[i].layer_id;
                //MTKFB_LOG_D("mslayer(0x%08x) bidx (%d) \n",(layerId<<24 | layerInfo[i].layer_enable), layerInfo[i].next_buff_idx);
                if (layerInfo[i].layer_id >= HW_OVERLAY_COUNT) 
                {
                    continue;
                }
                if(is_early_suspended)
                {
                    MTKFB_ERR("in early suspend layer(0x%x),idx(%d)!\n", layerId<<16|layerInfo[i].layer_enable, layerInfo[i].next_buff_idx);
                    //mtkfb_release_layer_fence(layerInfo[i].layer_id);
                }
                mtkfb_set_overlay_layer(info, &layerInfo[i]);
            }
    		disp_enque_job(disp_config.session_id);
            if (DISP_IsDecoupleMode()) 
            {
                DISP_StartOverlayTransfer();
            }
            is_ipoh_bootup = false;
            MMProfileLogStructure(MTKFB_MMP_Events.SetMultipleLayers, MMProfileFlagEnd, layerInfo, struct mmp_fb_overlay_layers);
        }
        
        return (r);
    }

    case MTKFB_TRIG_OVERLAY_OUT:
    {
        MTKFB_LOG(" mtkfb_ioctl():MTKFB_TRIG_OVERLAY_OUT\n");
        MMProfileLog(MTKFB_MMP_Events.TrigOverlayOut, MMProfileFlagPulse);

        return mtkfb_update_screen(info);
    }

#if defined (MTK_FB_SYNC_SUPPORT)
    case MTKFB_PREPARE_OVERLAY_BUFFER:
    {
        struct fb_overlay_buffer overlay_buffer;

        if (copy_from_user(&overlay_buffer, (void __user *)arg, sizeof(overlay_buffer))) 
        {
            MTKFB_ERR("copy_from_user failed!\n");
            r = -EFAULT;
        } 
        else 
        {
            if (overlay_buffer.layer_en) 
            {
                if (disp_sync_prepare_buffer_deprecated(disp_config.session_id, &overlay_buffer) != SYNC_STATUS_OK)
                {
                    overlay_buffer.fence_fd = DISP_INVALID_FENCE_FD; // invalid fd
                    overlay_buffer.index = 0;
                    r = -EFAULT;
                }
            } 
            else 
            {
                overlay_buffer.fence_fd = DISP_INVALID_FENCE_FD;    // invalid fd
                overlay_buffer.index = 0;
            }
            if (copy_to_user((void __user *)arg, &overlay_buffer, sizeof(overlay_buffer)))
            {
                MTKFB_ERR("copy_to_user failed!\n");
                r = -EFAULT;
            }
        }

        return (r);
    }
#endif //#if defined (MTK_FB_SYNC_SUPPORT)

    case MTKFB_SET_ORIENTATION:
    {
        MTKFB_LOG("[MTKFB] Set Orientation: %lu\n", arg);
        // surface flinger orientation definition of 90 and 270
        // is different than DISP_TV_ROT
        if (arg & 0x1) arg ^= 0x2;
        arg *=90;

#if defined(MTK_HDMI_SUPPORT)
        //for MT6589, the orientation of DDPK is changed from 0123 to 0/90/180/270
        hdmi_setorientation((int)arg);
#endif

        return 0;
    }
    case MTKFB_META_RESTORE_SCREEN:
    {
        struct fb_var_screeninfo var;

        if (copy_from_user(&var, argp, sizeof(var)))
            return -EFAULT;

        info->var.yoffset = var.yoffset;
        init_framebuffer(info);

        return mtkfb_pan_display_impl(&var, info);
    }

	case MTKFB_GET_INTERFACE_TYPE:
	{
        extern LCM_PARAMS *lcm_params;
        unsigned long lcm_type = lcm_params->type;

		MTKFB_LOG("[MTKFB] MTKFB_GET_INTERFACE_TYPE\n");

        printk("[MTKFB EM]MTKFB_GET_INTERFACE_TYPE is %ld\n", lcm_type);

        return copy_to_user(argp, &lcm_type,  sizeof(lcm_type)) ? -EFAULT : 0;
	}
    case MTKFB_GET_DEFAULT_UPDATESPEED:
	{
	    unsigned int speed;
		MTKFB_LOG("[MTKFB] get default update speed\n");
		DISP_Get_Default_UpdateSpeed(&speed);

        printk("[MTKFB EM]MTKFB_GET_DEFAULT_UPDATESPEED is %d\n", speed);
		return copy_to_user(argp, &speed,
                            sizeof(speed)) ? -EFAULT : 0;
    }

    case MTKFB_GET_CURR_UPDATESPEED:
	{
	    unsigned int speed;
		MTKFB_LOG("[MTKFB] get current update speed\n");
		DISP_Get_Current_UpdateSpeed(&speed);

        printk("[MTKFB EM]MTKFB_GET_CURR_UPDATESPEED is %d\n", speed);
		return copy_to_user(argp, &speed,
                            sizeof(speed)) ? -EFAULT : 0;
	}

	case MTKFB_CHANGE_UPDATESPEED:
	{
	    unsigned int speed;
		MTKFB_LOG("[MTKFB] change update speed\n");

		if (copy_from_user(&speed, (void __user *)arg, sizeof(speed))) {
            MTKFB_LOG("[FB]: copy_from_user failed! line:%d \n", __LINE__);
            r = -EFAULT;
        } else {
			DISP_Change_Update(speed);

            printk("[MTKFB EM]MTKFB_CHANGE_UPDATESPEED is %d\n", speed);

        }
        return (r);
	}

	case MTKFB_FACTORY_AUTO_TEST:
	{
		unsigned int result = 0;
		printk("factory mode: lcm auto test\n");
        result = mtkfb_fm_auto_test();
		return copy_to_user(argp, &result, sizeof(result)) ? -EFAULT : 0;
	}
    case MTKFB_AEE_LAYER_EXIST:
    {
		//printk("[MTKFB] isAEEEnabled=%d \n", isAEEEnabled);
		return copy_to_user(argp, &isAEEEnabled,
                            sizeof(isAEEEnabled)) ? -EFAULT : 0;
    }
    case MTKFB_LOCK_FRONT_BUFFER:
        return 0;
    case MTKFB_UNLOCK_FRONT_BUFFER:
        return 0;
	case MTKFB_SLT_AUTO_CAPTURE:
	{		
		struct fb_slt_catpure config;
        printk("MTKFB_SLT_AUTO_CAPTURE\n");
		if (copy_from_user(&config, argp, sizeof(config))) 
		{
			MTKFB_LOG("[FB]: copy_from_user failed! line:%d \n", __LINE__);
			r = -EFAULT;
		} 
		else 
		{
			printk("format bit 0x%x buf 0x%x width 0x%x ,height 0x%x\n",config.format,(unsigned int)config.outputBuffer,config.wdma_width,config.wdma_height);
			mtkfb_auto_capture_framebuffer(info,&config);
		}
		return (r);
	}

///=============================================================================
// Multiple Display Support
///================
	case DISP_IOCTL_CREATE_SESSION:
	{
		struct mmp_session_config {
			unsigned int type;
			unsigned int device;
			unsigned int mode;
			unsigned int session;
		};
		disp_session_config config;
		if (copy_from_user(&config, argp, sizeof(config))) {
			MTKFB_ERR("[FB]: copy_from_user failed!\n");
			return -EFAULT;
		}
		MMProfileLogStructure(MTKFB_MMP_Events.CreateSession, MMProfileFlagStart, &config, disp_session_config);
		if (disp_create_session(&config) != DCP_STATUS_OK) {
			r = -EFAULT;
		}
		if (copy_to_user(argp, &config, sizeof(config))) {
			MTKFB_ERR("[FB]: copy_to_user failed!\n");
			r = -EFAULT;
		}
		MMProfileLogEx(MTKFB_MMP_Events.CreateSession, MMProfileFlagEnd, config.session_id, disp_config.session_id);
		return (r);
	}

	case DISP_IOCTL_DESTROY_SESSION:
	{
		disp_session_config config;
		if (copy_from_user(&config, argp, sizeof(config))) {
			MTKFB_ERR("[FB]: copy_from_user failed!\n");
			return -EFAULT;
		}
		MMProfileLogStructure(MTKFB_MMP_Events.DestroySession, MMProfileFlagStart, &config, disp_session_config);
		if (disp_destroy_session(&config) != DCP_STATUS_OK) {
			r = -EFAULT;
		}
		MMProfileLogEx(MTKFB_MMP_Events.DestroySession, MMProfileFlagEnd, config.session_id, disp_config.session_id);
		return (r);
	}

	case DISP_IOCTL_TRIGGER_SESSION:
	{
		disp_session_config config;
		if (copy_from_user(&config, argp, sizeof(config))) {
			MTKFB_ERR("[FB]: copy_from_user failed!\n");
			return -EFAULT;
		}
		MMProfileLogStructure(MTKFB_MMP_Events.TriggerSession, MMProfileFlagStart, &config, disp_session_config);

		disp_enque_job(config.session_id);
		if (DISP_IsDecoupleMode()) {
        	DISP_StartOverlayTransfer();
        }
		MMProfileLogEx(MTKFB_MMP_Events.TriggerSession, MMProfileFlagEnd, config.session_id, disp_config.session_id);
		return (r);
	}

	case DISP_IOCTL_PREPARE_INPUT_BUFFER:
	case DISP_IOCTL_PREPARE_OUTPUT_BUFFER:
	{
		disp_buffer_info info;
		if (copy_from_user(&info, (void __user *)arg, sizeof(info))) {
			MTKFB_ERR("[FB]: copy_from_user failed!\n");
			return -EFAULT;
		}
		MMProfileLogStructure(MTKFB_MMP_Events.PrepareInput, MMProfileFlagStart, &info, disp_buffer_info);
		if (info.layer_en) {
			if (disp_sync_prepare_buffer(&info) != SYNC_STATUS_OK) {
				info.fence_fd = DISP_INVALID_FENCE_FD; // invalid fd
				info.index = 0;
				r = -EFAULT;
			}
		} else {
			info.fence_fd = DISP_INVALID_FENCE_FD;    // invalid fd
			info.index = 0;
		}
		if (copy_to_user((void __user *)arg, &info, sizeof(info)))
		{
			MTKFB_ERR("[FB]: copy_to_user failed!\n");
			r = -EFAULT;
		}
		MMProfileLogEx(MTKFB_MMP_Events.PrepareInput, MMProfileFlagEnd, disp_config.session_id, r);
		return (r);
	}

	case DISP_IOCTL_SET_INPUT_BUFFER:
	{
		disp_session_input_config layerInfo;
		MTKFB_LOG(" mtkfb_ioctl():DISP_IOCTL_SET_INPUT_BUFFER\n");
		if (copy_from_user(&layerInfo, (void __user *)arg, sizeof(layerInfo))) {
			MTKFB_ERR("[FB]: copy_from_user failed!\n");
			return -EFAULT;
		}
		MMProfileLogStructure(MTKFB_MMP_Events.SetInput, MMProfileFlagStart, &layerInfo, disp_session_input_config);
		if (disp_set_session_input(&layerInfo) != DCP_STATUS_OK) {
			r = -EFAULT;
		}
		is_ipoh_bootup = false;
		MMProfileLogEx(MTKFB_MMP_Events.SetInput, MMProfileFlagEnd, disp_config.session_id, r);
		return r;
	}

	case DISP_IOCTL_SET_OUTPUT_BUFFER:
	{
		disp_session_output_config layerInfo;
		MTKFB_LOG(" mtkfb_ioctl():DISP_IOCTL_SET_OUTPUT_BUFFER\n");
		if (copy_from_user(&layerInfo, (void __user *)arg, sizeof(layerInfo))) {
			MTKFB_ERR("[FB]: copy_from_user failed!\n");
			return -EFAULT;
		}
		MMProfileLogStructure(MTKFB_MMP_Events.SetOutput, MMProfileFlagStart, &layerInfo, disp_session_output_config);
		if (disp_set_session_output(&layerInfo) != DCP_STATUS_OK) {
			r = -EFAULT;
		}
		MMProfileLogEx(MTKFB_MMP_Events.SetOutput, MMProfileFlagEnd, disp_config.session_id, r);
		return r;
	}

	case DISP_IOCTL_GET_SESSION_INFO:
	{
		disp_session_info info;
		int dev = 0;
		if (copy_from_user(&info, argp, sizeof(info))) {
			MTKFB_ERR("[FB]: copy_from_user failed!\n");
			return -EFAULT;
		}
		dev = DISP_SESSION_DEV(info.session_id);
		info.displayFormat = dispif_info[dev].displayFormat;
		info.displayHeight = dispif_info[dev].displayHeight;
		info.displayMode = dispif_info[dev].displayMode;
		info.displayType = dispif_info[dev].displayType;
		info.displayWidth = dispif_info[dev].displayWidth;
		info.isConnected = dispif_info[dev].isConnected;
		info.isHwVsyncAvailable = dispif_info[dev].isHwVsyncAvailable;
		info.maxLayerNum = isAEEEnabled ? (HW_OVERLAY_COUNT - 1):HW_OVERLAY_COUNT;
		info.physicalHeight = dispif_info[dev].physicalHeight;
		info.physicalWidth = dispif_info[dev].physicalWidth;
		info.vsyncFPS = dispif_info[dev].vsyncFPS;

		MMProfileLogStructure(MTKFB_MMP_Events.GetDispInfo, MMProfileFlagPulse, &info, disp_session_info);

		if (copy_to_user(argp, &info, sizeof(info))) {
			MTKFB_ERR("[FB]: copy_to_user failed!\n");
			r = -EFAULT;
		}
		return (r);
	}
////////////////////////////////////////////////
    default:
        printk("mtkfb_ioctl Not support, info=0x%08x, cmd=0x%08x, arg=0x%08x, num=%d\n", (unsigned int)info, (unsigned int)cmd, (unsigned int)arg,  _IOC_NR(cmd));
        return -EINVAL;
    }
}

static int mtkfb_fbinfo_modify(struct fb_info *info)
{
    struct fb_var_screeninfo var;
    int r = 0;
    
    memcpy(&var, &(info->var), sizeof(var));
    var.activate		= FB_ACTIVATE_NOW;
    var.bits_per_pixel  = 32;
    var.transp.offset	= 24;
    var.transp.length	= 8;
    var.red.offset 	    = 16; var.red.length 	= 8;
    var.green.offset    = 8;  var.green.length	= 8;
    var.blue.offset	    = 0;  var.blue.length	= 8;
    var.yoffset         = var.yres;

    r = mtkfb_check_var(&var, info);
    if (r != 0)
        PRNERR("failed to mtkfb_check_var\n");

    info->var = var;

    r = mtkfb_set_par(info);
    if (r != 0)
        PRNERR("failed to mtkfb_set_par\n");

    return r;
}

UINT32 color = 0;
unsigned int mtkfb_fm_auto_test()
{
	unsigned int result = 0;
	unsigned int bls_enable = 0;
	unsigned int i=0;
	UINT32 fbVirAddr;
	UINT32 fbsize;
	int r = 0;
	unsigned int *fb_buffer;
    struct mtkfb_device  *fbdev = (struct mtkfb_device *)mtkfb_fbi->par;
	struct fb_var_screeninfo var;
	fbVirAddr = (UINT32)fbdev->fb_va_base;
	fb_buffer = (unsigned int*)fbVirAddr;

	memcpy(&var, &(mtkfb_fbi->var), sizeof(var));
	var.activate		= FB_ACTIVATE_NOW;
	var.bits_per_pixel	= 32;
	var.transp.offset	= 24;
	var.transp.length	= 8;
	var.red.offset		= 16; var.red.length	= 8;
	var.green.offset	= 8;  var.green.length	= 8;
	var.blue.offset 	= 0;  var.blue.length	= 8;

	r = mtkfb_check_var(&var, mtkfb_fbi);
	if (r != 0)
		PRNERR("failed to mtkfb_check_var\n");

	mtkfb_fbi->var = var;
	r = mtkfb_set_par(mtkfb_fbi);
	if (r != 0)
		PRNERR("failed to mtkfb_set_par\n");
	
	if(color == 0)
		color = 0xFF00FF00;
	fbsize = ALIGN_TO(DISP_GetScreenWidth(),disphal_get_fb_alignment())*DISP_GetScreenHeight()*MTK_FB_PAGES;

	bls_enable = DISP_BLS_Query();
	
	printk("BLS is enable %d\n",bls_enable);
    if(bls_enable == 1)
		DISP_BLS_Enable(false);

	for(i=0;i<fbsize;i++)
		*fb_buffer++ = color;

	for(i=0;i<DDP_OVL_LAYER_MUN;i++)
	{
		cached_layer_config[i].isDirty = 1;
	}

	msleep(100);

    if (down_interruptible(&sem_early_suspend)) {
        printk("[FB Driver] can't get semaphore in mtkfb_fm_auto_test()\n");
        return result;
    }
    DISP_PrepareSuspend();
    // Wait for disp finished.
    if (wait_event_interruptible_timeout(disp_done_wq, !disp_running, HZ/10) == 0)
    {
        printk("[FB Driver] Wait disp finished timeout in early_suspend\n");
    }

	result = DISP_AutoTest();
	
	up(&sem_early_suspend);
	if(result == 0){
		printk("ATA LCM failed\n");
	}else{
		printk("ATA LCM passed\n");
	}
	
	return result;
}

/* Callback table for the frame buffer framework. Some of these pointers
 * will be changed according to the current setting of fb_info->accel_flags.
 */
static struct fb_ops mtkfb_ops = {
    .owner          = THIS_MODULE,
    .fb_open        = mtkfb_open,
    .fb_release     = mtkfb_release,
    .fb_setcolreg   = mtkfb_setcolreg,
    .fb_pan_display = mtkfb_pan_display_proxy,
    .fb_fillrect    = cfb_fillrect,
    .fb_copyarea    = cfb_copyarea,
    .fb_imageblit   = cfb_imageblit,
    .fb_cursor      = mtkfb_soft_cursor,
    .fb_check_var   = mtkfb_check_var,
    .fb_set_par     = mtkfb_set_par,
    .fb_ioctl       = mtkfb_ioctl,
};

/*
 * ---------------------------------------------------------------------------
 * Sysfs interface
 * ---------------------------------------------------------------------------
 */

static int mtkfb_register_sysfs(struct mtkfb_device *fbdev)
{
    NOT_REFERENCED(fbdev);

    return 0;
}

static void mtkfb_unregister_sysfs(struct mtkfb_device *fbdev)
{
    NOT_REFERENCED(fbdev);
}

/*
 * ---------------------------------------------------------------------------
 * LDM callbacks
 * ---------------------------------------------------------------------------
 */
/* Initialize system fb_info object and set the default video mode.
 * The frame buffer memory already allocated by lcddma_init
 */
static int mtkfb_fbinfo_init(struct fb_info *info)
{
    struct mtkfb_device *fbdev = (struct mtkfb_device *)info->par;
    struct fb_var_screeninfo var;
    int r = 0;

    MSG_FUNC_ENTER();

    BUG_ON(!fbdev->fb_va_base);
    info->fbops = &mtkfb_ops;
    info->flags = FBINFO_FLAG_DEFAULT;
    info->screen_base = (char *) fbdev->fb_va_base;
    info->screen_size = fbdev->fb_size_in_byte;
    info->pseudo_palette = fbdev->pseudo_palette;

    r = fb_alloc_cmap(&info->cmap, 16, 0);
    if (r != 0)
        PRNERR("unable to allocate color map memory\n");

    // setup the initial video mode (RGB565)

    memset(&var, 0, sizeof(var));

    var.xres         = MTK_FB_XRES;
    var.yres         = MTK_FB_YRES;
    var.xres_virtual = MTK_FB_XRESV;
    var.yres_virtual = MTK_FB_YRESV;

    var.bits_per_pixel = 16;

    var.red.offset   = 11; var.red.length   = 5;
    var.green.offset =  5; var.green.length = 6;
    var.blue.offset  =  0; var.blue.length  = 5;

    var.width  = DISP_GetPhysicalWidth();
    var.height = DISP_GetPhysicalHeight();

    var.activate = FB_ACTIVATE_NOW;

    r = mtkfb_check_var(&var, info);
    if (r != 0)
        PRNERR("failed to mtkfb_check_var\n");

    info->var = var;

    r = mtkfb_set_par(info);
    if (r != 0)
        PRNERR("failed to mtkfb_set_par\n");

    MSG_FUNC_LEAVE();
    return r;
}

/* Release the fb_info object */
static void mtkfb_fbinfo_cleanup(struct mtkfb_device *fbdev)
{
    MSG_FUNC_ENTER();

    fb_dealloc_cmap(&fbdev->fb_info->cmap);

    MSG_FUNC_LEAVE();
}

void mtkfb_disable_non_fb_layer(void)
{
    int id;
    unsigned int dirty = 0;
    for (id = 0; id < DDP_OVL_LAYER_MUN; id++)
    {
        if (cached_layer_config[id].layer_en == 0)
            continue;

        if (cached_layer_config[id].addr >= fb_pa &&
            cached_layer_config[id].addr < (fb_pa+DISP_GetVRamSize()))
            continue;

        DISP_LOG_PRINT(ANDROID_LOG_INFO, "LCD", "  disable(%d)\n", id);
        cached_layer_config[id].layer_en = 0;
        cached_layer_config[id].isDirty = true;
        dirty = 1;
    }
    if (dirty)
    {
        memset(mtkfb_fbi->screen_base, 0, DISP_GetFBRamSize());
        mtkfb_pan_display_impl(&mtkfb_fbi->var, mtkfb_fbi);
    }
}

int m4u_reclaim_mva_callback_ovl(int moduleID, unsigned int va, unsigned int size, unsigned int mva)
{
    int id;
    unsigned int dirty = 0;
    MMProfileLogEx(MTKFB_MMP_Events.Debug, MMProfileFlagStart, mva, size);
    for (id = 0; id < DDP_OVL_LAYER_MUN; id++)
    {
        if (cached_layer_config[id].layer_en == 0)
            continue;

        if (cached_layer_config[id].addr >= mva &&
            cached_layer_config[id].addr < (mva+size))
        {
            printk("Warning: m4u required to disable layer id=%d\n", id);
            cached_layer_config[id].layer_en = 0;
            cached_layer_config[id].isDirty = 1;
            dirty = 1;
        }
    }
    if (dirty)
    {
        printk(KERN_INFO"Warning: m4u_reclaim_mva_callback_ovl. mva=0x%08X size=0x%X dirty=%d\n", mva, size, dirty);
        memset(mtkfb_fbi->screen_base, 0, DISP_GetVRamSize());
        mtkfb_pan_display_impl(&mtkfb_fbi->var, mtkfb_fbi);
    }
    MMProfileLogEx(MTKFB_MMP_Events.Debug, MMProfileFlagEnd, dirty, 0);
    return 0;
}

#define RGB565_TO_ARGB8888(x)   \
    ((((x) &   0x1F) << 3) |    \
     (((x) &  0x7E0) << 5) |    \
     (((x) & 0xF800) << 8) |    \
     (0xFF << 24)) // opaque

/* Init frame buffer content as 3 R/G/B color bars for debug */
static int init_framebuffer(struct fb_info *info)
{
    void *buffer = info->screen_base +
                   info->var.yoffset * info->fix.line_length;

    // clean whole frame buffer as black
    memset(buffer, 0, info->screen_size);

    return 0;
}


/* Free driver resources. Can be called to rollback an aborted initialization
 * sequence.
 */
static void mtkfb_free_resources(struct mtkfb_device *fbdev, int state)
{
    int r = 0;

    switch (state) {
    case MTKFB_ACTIVE:
        r = unregister_framebuffer(fbdev->fb_info);
        ASSERT(0 == r);
      //lint -fallthrough
    case 5:
        mtkfb_unregister_sysfs(fbdev);
      //lint -fallthrough
    case 4:
        mtkfb_fbinfo_cleanup(fbdev);
      //lint -fallthrough
    case 3:
        DISP_CHECK_RET(DISP_Deinit());
      //lint -fallthrough
    case 2:
        dma_free_coherent(0, fbdev->fb_size_in_byte,
                          fbdev->fb_va_base, fbdev->fb_pa_base);
      //lint -fallthrough
    case 1:
        dev_set_drvdata(fbdev->dev, NULL);
        framebuffer_release(fbdev->fb_info);
      //lint -fallthrough
    case 0:
      /* nothing to free */
        break;
    default:
        BUG();
    }
}

extern char* saved_command_line;
char mtkfb_lcm_name[256] = {0};
BOOL mtkfb_find_lcm_driver(void)
{
	BOOL ret = FALSE;
	char *p, *q;

	p = strstr(saved_command_line, "lcm=");
	if(p == NULL)
	{
		// we can't find lcm string in the command line, the uboot should be old version
		return DISP_SelectDevice(NULL);
	}

	p += 4;
	if((p - saved_command_line) > strlen(saved_command_line+1))
	{
		ret = FALSE;
		goto done;
	}

	printk("%s, %s\n", __func__, p);
	q = p;
	while(*q != ' ' && *q != '\0')
		q++;

	memset((void*)mtkfb_lcm_name, 0, sizeof(mtkfb_lcm_name));
	strncpy((char*)mtkfb_lcm_name, (const char*)p, (int)(q-p));

	printk("%s, %s\n", __func__, mtkfb_lcm_name);
	if(DISP_SelectDevice(mtkfb_lcm_name))
		ret = TRUE;

done:
	return ret;
}

void disp_get_fb_address(UINT32 *fbVirAddr, UINT32 *fbPhysAddr)
{
    struct mtkfb_device  *fbdev = (struct mtkfb_device *)mtkfb_fbi->par;

    *fbVirAddr = (UINT32)fbdev->fb_va_base + mtkfb_fbi->var.yoffset * mtkfb_fbi->fix.line_length;
    *fbPhysAddr =(UINT32)fbdev->fb_pa_base + mtkfb_fbi->var.yoffset * mtkfb_fbi->fix.line_length;
}

static void mtkfb_fb_565_to_8888(struct fb_info *fb_info)
{
    unsigned int xres = fb_info->var.xres;
    unsigned int yres = fb_info->var.yres;
    unsigned int x_virtual = ALIGN_TO(xres,disphal_get_fb_alignment());

    unsigned int fbsize = x_virtual * yres * 2;

    unsigned short *s = (unsigned short*) fb_info->screen_base;
    unsigned int   *d = (unsigned int*) (fb_info->screen_base + fbsize * 2);
    unsigned short src_rgb565 = 0;
    int j = 0;
    int k = 0;
    int wait_ret = 0;

    PRNERR("mtkfb_fb_565_to_8888 xres=%d yres=%d fbsize=0x%X x_virtual=%d s=0x%08X d=0x%08X\n",
                 xres, yres, fbsize, x_virtual, s, d);
    //printf("[boot_logo_updater]normal\n");
    for (j = 0; j < yres; ++ j){
        for(k = 0; k < xres; ++ k)
        {
            src_rgb565 = *s++;
            *d++ = RGB565_TO_ARGB8888(src_rgb565);
        }
		d += (ALIGN_TO(xres, MTK_FB_ALIGNMENT)-xres);
#if 0
        for(k = xres; k < x_virtual; ++ k){
            *d++ = 0xFFFFFFFF;
            *s++;
        }
#endif
        s += (ALIGN_TO(xres, disphal_get_fb_alignment()) - xres);
    }
    //printf("[boot_logo_updater] loop copy color over\n");

    mtkfb_fbinfo_modify(fb_info);
    wait_ret = wait_event_interruptible_timeout(reg_update_wq, atomic_read(&OverlaySettingApplied), HZ/10);
    MTKFB_LOG("[WaitQ] wait_event_interruptible() ret = %d, %d\n", wait_ret, __LINE__);

    s = (unsigned short *)fb_info->screen_base;
    d = (unsigned int *) (fb_info->screen_base + fbsize * 2);
    memcpy(s,d,fbsize*2);
}

 #ifdef FPGA_DEBUG_PAN
 static int update_test_kthread(void *data)
{
	//struct sched_param param = { .sched_priority = RTPM_PRIO_SCRN_UPDATE };
	//sched_setscheduler(current, SCHED_RR, &param);
	unsigned int i = 0;
    for( ;; ) {

        if (kthread_should_stop())
            break;
        msleep(1000);       //2s
		printk("update test thread work\n");
		mtkfb_fbi->var.yoffset =  (i%2)*DISP_GetScreenHeight();
		i++;
		mtkfb_pan_display_impl(&mtkfb_fbi->var,mtkfb_fbi);
    }


    MTKFB_LOG("exit esd_recovery_kthread()\n");
    return 0;
}
#endif

static void mtkfb_fb_565_to_888(void* fb_va)
{
    unsigned int xres = DISP_GetScreenWidth();
    unsigned int yres = DISP_GetScreenHeight();
    //unsigned int x_virtual = ALIGN_TO(xres,disphal_get_fb_alignment());

    unsigned short *s = (unsigned short*)(fb_va);
    unsigned char *d = (unsigned char*) (fb_va + + DISP_GetFBRamSize() + DAL_GetLayerSize());
    unsigned short src_rgb565 = 0;
    int j = 0;
    int k = 0;

    //printk("555_to_888, s = 0x%x, d=0x%x\n", s,d);
    for (j = 0; j < yres; ++ j)
    {
        for(k = 0; k < xres; ++ k)
        {
            src_rgb565 = *s++;
            *d++ = ((src_rgb565 & 0x1F) << 3);
            *d++ = ((src_rgb565 & 0x7E0) >> 3);
            *d++ = ((src_rgb565 & 0xF800) >> 8);
        }
        s += (ALIGN_TO(xres, disphal_get_fb_alignment()) - xres);
    }
}

#if defined(DFO_USE_NEW_API)
#if 1
extern int dfo_query(const char *s, unsigned long *v);
#endif
#else
 #include <mach/dfo_boot.h>
static disp_dfo_item_t disp_dfo_setting[] =
{
	{"LCM_FAKE_WIDTH",	0},
	{"LCM_FAKE_HEIGHT", 0},
	{"DISP_DEBUG_SWITCH",	0}
};
	
#define MT_DISP_DFO_DEBUG 
#ifdef MT_DISP_DFO_DEBUG
#define disp_dfo_printf(string, args...) printk("[DISP/DFO]"string, ##args)
#else
#define disp_dfo_printf(string, args...) ()
#endif

// this function will be called in mt_fixup()@mt_devs.c. which will send DFO information organized as tag_dfo_boot struct.
// because lcm_params isn't inited here, so we will change lcm_params later in mtkfb_probe.
unsigned int mtkfb_parse_dfo_setting(void *dfo_tbl, int num)
{
	char *disp_name = NULL;
	//int  *disp_value;
	char *tag_name;
	int  tag_value;
	int i, j;
	tag_dfo_boot *dfo_data;
	
	disp_dfo_printf("enter mtkfb_parse_dfo_setting\n");

	if(dfo_tbl == NULL)
		return -1;

	dfo_data = (tag_dfo_boot *)dfo_tbl;
	for (i=0; i<(sizeof(disp_dfo_setting)/sizeof(disp_dfo_item_t)); i++) 
	{
		disp_name = disp_dfo_setting[i].name;
		
		for (j=0; j<num; j++) 
		{
			tag_name = dfo_data->name[j];
			tag_value = dfo_data->value[j];
			if(!strcmp(disp_name, tag_name)) 
			{
				disp_dfo_setting[i].value = tag_value;
				disp_dfo_printf("%s = [DEC]%d [HEX]0x%08x\n", disp_dfo_setting[i].name, disp_dfo_setting[i].value, disp_dfo_setting[i].value);
			}
		}
	}	
	
	disp_dfo_printf("leave mtkfb_parse_dfo_setting\n");

	return 0;
}

int mtkfb_get_dfo_setting(const char *string, unsigned int *value)
{	
	char *disp_name;
	int  disp_value;
	int i;

	if(string == NULL)
		return -1;
	
	for (i=0; i<(sizeof(disp_dfo_setting)/sizeof(disp_dfo_item_t)); i++) 
	{
		disp_name = disp_dfo_setting[i].name;
		disp_value = disp_dfo_setting[i].value;
		if(!strcmp(disp_name, string)) 
		{
			*value = disp_value;
			disp_dfo_printf("%s = [DEC]%d [HEX]0x%08x\n", disp_name, disp_value, disp_value);
			return 0;
		}
	}	

	return 0;
}
#endif

/* Called by LDM binding to probe and attach a new device.
 * Initialization sequence:
 *   1. allocate system fb_info structure
 *      select panel type according to machine type
 *   2. init LCD panel
 *   3. init LCD controller and LCD DMA
 *   4. init system fb_info structure
 *   5. init gfx DMA
 *   6. enable LCD panel
 *      start LCD frame transfer
 *   7. register system fb_info structure
 */
static int mtkfb_probe(struct device *dev)
{
    struct platform_device *pdev;
    struct mtkfb_device    *fbdev = NULL;
    struct fb_info         *fbi;
    int                    init_state;
    int                    r = 0;
    unsigned int lcm_fake_width = 0;
    unsigned int lcm_fake_height = 0;
    char *p = NULL;

    MSG_FUNC_ENTER();


    if(get_boot_mode() == META_BOOT || get_boot_mode() == FACTORY_BOOT
       || get_boot_mode() == ADVMETA_BOOT || get_boot_mode() == RECOVERY_BOOT)
        first_update = false;
    
    printk("%s, %s\n", __func__, saved_command_line);
    p = strstr(saved_command_line, "fps=");
    if(p == NULL)
    {
        lcd_fps = 6000;
        printk("[FB driver]can not get fps from uboot\n");
    }
    else
    {
        p += 4;
        lcd_fps = simple_strtol(p, NULL, 10);
        if(0 == lcd_fps) lcd_fps = 6000;
    }
    
    if(DISP_IsContextInited() == FALSE)
    {
        if(mtkfb_find_lcm_driver())
        {
            printk("%s, we have found the lcm - %s\n", __func__, mtkfb_lcm_name);
        }
        else if(DISP_DetectDevice() != DISP_STATUS_OK)
        {
            printk("[mtkfb] detect device fail, maybe caused by the two reasons below:\n");
            printk("\t\t1.no lcm connected\n");
            printk("\t\t2.we can't support this lcm\n");
        }
    }

#ifdef MTKFB_FPGA_ONLY
    is_lcm_inited = FALSE;
#endif

#if defined(DFO_USE_NEW_API)
    #if 1
        if((0 == dfo_query("LCM_FAKE_WIDTH", &lcm_fake_width)) && (0 == dfo_query("LCM_FAKE_HEIGHT", &lcm_fake_height)))
    #endif
#else
    if((0 == mtkfb_get_dfo_setting("LCM_FAKE_WIDTH", &lcm_fake_width)) && (0 == mtkfb_get_dfo_setting("LCM_FAKE_HEIGHT", &lcm_fake_height)))
#endif
    {
        printk("[DFO] LCM_FAKE_WIDTH=%d, LCM_FAKE_HEIGHT=%d\n", lcm_fake_width, lcm_fake_height);
        if(lcm_fake_width && lcm_fake_height)
        {
            if(DISP_STATUS_OK != DISP_Change_LCM_Resolution(lcm_fake_width, lcm_fake_height))
            {
                printk("[DISP/DFO]WARNING!!! Change LCM Resolution FAILED!!!\n");
            }
        }
    }

    MTK_FB_XRES  = DISP_GetScreenWidth();
    MTK_FB_YRES  = DISP_GetScreenHeight();
    fb_xres_update = MTK_FB_XRES;
    fb_yres_update = MTK_FB_YRES;

    printk("[MTKFB] XRES=%d, YRES=%d\n", MTK_FB_XRES, MTK_FB_YRES);

    MTK_FB_BPP   = DISP_GetScreenBpp();
    MTK_FB_PAGES = DISP_GetPages();

    if(DISP_IsLcmFound() && DISP_EsdRecoverCapbility())
    {
        esd_recovery_task = kthread_create(esd_recovery_kthread, NULL, "esd_recovery_kthread");

        if (IS_ERR(esd_recovery_task)) 
        {
            MTKFB_LOG("ESD recovery task create fail\n");
        }
        else 
        {
            wake_up_process(esd_recovery_task);
        }
    }
    init_state = 0;

    pdev = to_platform_device(dev);
    if (pdev->num_resources != 1) 
    {
        PRNERR("probed for an unknown device\n");
        r = -ENODEV;
        goto cleanup;
    }

    fbi = framebuffer_alloc(sizeof(struct mtkfb_device), dev);
    if (!fbi) 
    {
        PRNERR("unable to allocate memory for device info\n");
        r = -ENOMEM;
        goto cleanup;
    }
    mtkfb_fbi = fbi;

    fbdev = (struct mtkfb_device *)fbi->par;
    fbdev->fb_info = fbi;
    fbdev->dev = dev;
    fbdev->layer_format = (MTK_FB_FORMAT*)vmalloc(sizeof(MTK_FB_FORMAT) * HW_OVERLAY_COUNT);
    if(!fbdev->layer_format)
    {
        printk("[mtkfb.c FB driver] vmalloc failed, %d\n", __LINE__);
        r = -ENOMEM;
        goto cleanup;
    }

    dev_set_drvdata(dev, fbdev);

    init_state++;   // 1

    /* Allocate and initialize video frame buffer */

    fbdev->fb_size_in_byte = MTK_FB_SIZEV;
    {
        struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

        // ASSERT(DISP_GetVRamSize() <= (res->end - res->start + 1));
        struct resource res_lk;
        unsigned int pa_lk;
        unsigned int va_lk;
        unsigned int dma_pa_lk;
        unsigned int fbsize_copy = mtkfb_fb_lk_copy_size();

        if(fb_address_lk)
        {
        	res_lk.start = fb_address_lk;
        	res_lk.end   = fb_address_lk + fbsize_copy -1;
        	disphal_enable_mmu(FALSE);
        	disphal_allocate_fb(&res_lk, &pa_lk, &va_lk, &dma_pa_lk);
        }

        disphal_enable_mmu(mtkfb_enable_mmu);
        disphal_allocate_fb(res, &fbdev->fb_pa_base, (unsigned int*)&fbdev->fb_va_base, &fb_pa);

		if(fb_address_lk)
		{
			printk("[wwy]fbsize_copy=%d\n", fbsize_copy);
        	memcpy((void*)fbdev->fb_va_base,(void*)(va_lk),fbsize_copy);
        	iounmap((void*)va_lk);
        }

#if defined(MTK_OVL_DECOUPLE_SUPPORT)
        fbdev->ovl_pa_base = fbdev->fb_pa_base + DISP_GetFBRamSize() + DAL_GetLayerSize();
        fbdev->ovl_va_base = fbdev->fb_va_base + DISP_GetFBRamSize() + DAL_GetLayerSize();
        fbdev->ovl_size_in_byte = DISP_GetOVLRamSize();

        decouple_addr = fbdev->ovl_pa_base;
        decouple_size = fbdev->ovl_size_in_byte;
        // Copy lk logo buffer to decouple buffer
        mtkfb_fb_565_to_888((void*)fbdev->fb_va_base);
#endif
#ifdef MTKFB_FPGA_ONLY
        memset((void*)fbdev->fb_va_base, 0x88, (res->end - res->start + 1));
#endif
    }

    printk("[FB Driver] fb_pa_base=0x%08x, fb_pa=0x%08x, decouple_addr=0x%08x\n", fbdev->fb_pa_base, fb_pa, decouple_addr);

    if (!fbdev->fb_va_base) 
    {
        PRNERR("unable to allocate memory for frame buffer\n");
        r = -ENOMEM;
        goto cleanup;
    }
    init_state++;   // 2

#if defined (MTK_FB_SYNC_SUPPORT)
    disp_sync_init(disp_config.session_id);
#endif
    /* Initialize Display Driver PDD Layer */
    if (DISP_STATUS_OK != DISP_Init((DWORD)fbdev->fb_va_base,
        (DWORD)fb_pa,
         is_lcm_inited))
    {
        r = -1;
        goto cleanup;
    }

    init_state++;   // 3

    /* Register to system */

    r = mtkfb_fbinfo_init(fbi);
    if (r)
        goto cleanup;
    init_state++;   // 4

    r = mtkfb_register_sysfs(fbdev);
    if (r)
        goto cleanup;
    init_state++;   // 5

    r = register_framebuffer(fbi);
    if (r != 0) 
    {
        PRNERR("register_framebuffer failed\n");
        goto cleanup;
    }

    fbdev->state = MTKFB_ACTIVE;

    /********************************************/

    mtkfb_fb_565_to_8888(fbi);

    /********************************************/
    MSG(INFO, "MTK framebuffer initialized vram=%lu\n", fbdev->fb_size_in_byte);
#ifdef FPGA_DEBUG_PAN
    {
        unsigned int cnt = 0;
        unsigned int *fb_va = (unsigned int*)fbdev->fb_va_base;
        unsigned int fbsize = DISP_GetScreenHeight()*DISP_GetScreenWidth();

        for(cnt=0;cnt<fbsize;cnt++)
            *(fb_va++) = 0xFFFF0000;
        for(cnt=0;cnt<fbsize;cnt++)
            *(fb_va++) = 0xFF00FF00;
    }

    printk("memset done\n");
    {
        struct task_struct *update_test_task = NULL;

        update_test_task = kthread_create(update_test_kthread, NULL, "update_test_kthread");
        
        if (IS_ERR(update_test_task)) 
        {
            MTKFB_LOG("update test task create fail\n");
        }
        else 
        {
            wake_up_process(update_test_task);
        }
    }
#endif
    MSG_FUNC_LEAVE();
    return 0;

cleanup:
    mtkfb_free_resources(fbdev, init_state);

    MSG_FUNC_LEAVE();
    return r;
}

/* Called when the device is being detached from the driver */
static int mtkfb_remove(struct device *dev)
{
    struct mtkfb_device *fbdev = dev_get_drvdata(dev);
    enum mtkfb_state saved_state = fbdev->state;

    MSG_FUNC_ENTER();
    /* FIXME: wait till completion of pending events */

    fbdev->state = MTKFB_DISABLED;
    mtkfb_free_resources(fbdev, saved_state);

    MSG_FUNC_LEAVE();
    return 0;
}

/* PM suspend */
static int mtkfb_suspend(struct device *pdev, pm_message_t mesg)
{
    NOT_REFERENCED(pdev);
    MSG_FUNC_ENTER();
    MTKFB_LOG("[FB Driver] mtkfb_suspend(): 0x%x\n", mesg.event);
    MSG_FUNC_LEAVE();
    return 0;
}
bool mtkfb_is_suspend(void)
{
    return is_early_suspended;
}
EXPORT_SYMBOL(mtkfb_is_suspend);

static void mtkfb_shutdown(struct device *pdev)
{
    MTKFB_LOG("[FB Driver] mtkfb_shutdown()\n");
#if defined(CONFIG_MTK_LEDS)
    //mt65xx_leds_brightness_set(MT65XX_LED_TYPE_LCD, LED_OFF);
#endif
    if (!lcd_fps)
        msleep(30);
    else
        msleep(2*100000/lcd_fps); // Delay 2 frames.

	if(is_early_suspended){
		MTKFB_LOG("mtkfb has been power off\n");
		return;
	}

    if (down_interruptible(&sem_early_suspend)) {
        printk("[FB Driver] can't get semaphore in mtkfb_shutdown()\n");
        return;
    }
	sem_early_suspend_cnt--;

	is_early_suspended = TRUE;
	DISP_PrepareSuspend();
    // Wait for disp finished.
    if (wait_event_interruptible_timeout(disp_done_wq, !disp_running, HZ/10) == 0)
    {
        printk("[FB Driver] Wait disp finished timeout in shut_down\n");
    }
	DISP_CHECK_RET(DISP_PanelEnable(FALSE));
 	DISP_CHECK_RET(DISP_PowerEnable(FALSE));

	DISP_CHECK_RET(DISP_PauseVsync(TRUE));
	disp_path_clock_off("mtkfb");
	sem_early_suspend_cnt++;
    up(&sem_early_suspend);

    MTKFB_LOG("[FB Driver] leave mtkfb_shutdown\n");
}

void mtkfb_clear_lcm(void)
{
	int i;
    disp_job *job = disp_deque_job(disp_config.session_id);
    mutex_lock(&job->lock);

    for(i=0;i<DDP_OVL_LAYER_MUN;i++)
    {
        job->input[i].layer_enable = 0;
        job->input[i].dirty = 1;
    }
    mutex_unlock(&job->lock);
    disp_enque_job(disp_config.session_id);
    if (DISP_IsDecoupleMode()) {
    	DISP_StartOverlayTransfer();
    }

    DISP_CHECK_RET(DISP_UpdateScreen(0, 0, fb_xres_update, fb_yres_update));
    DISP_CHECK_RET(DISP_UpdateScreen(0, 0, fb_xres_update, fb_yres_update));
    if (!lcd_fps)
        msleep(30);
    else
        msleep(200000/lcd_fps); // Delay 1 frame.
    DISP_WaitForLCDNotBusy();

}


#ifdef CONFIG_HAS_EARLYSUSPEND
static void mtkfb_early_suspend(struct early_suspend *h)
{
    int i=0;
    MSG_FUNC_ENTER();

    printk("[FB Driver] enter early_suspend\n");

    mutex_lock(&ScreenCaptureMutex);
#if defined(CONFIG_MTK_LEDS)
#ifndef MTKFB_FPGA_ONLY
    mt65xx_leds_brightness_set(MT65XX_LED_TYPE_LCD, LED_OFF);
#endif
#endif
    if (!lcd_fps)
        msleep(30);
    else
        msleep(2*100000/lcd_fps); // Delay 2 frames.

    if (down_interruptible(&sem_early_suspend)) {
        printk("[FB Driver] can't get semaphore in mtkfb_early_suspend()\n");
        mutex_unlock(&ScreenCaptureMutex);
        return;
    }

    sem_early_suspend_cnt--;
    //MMProfileLogEx(MTKFB_MMP_Events.EarlySuspend, MMProfileFlagStart, 0, 0);

    if(is_early_suspended){
        is_early_suspended = TRUE;
        sem_early_suspend_cnt++;
        up(&sem_early_suspend);
        MTKFB_LOG("[FB driver] has been suspended\n");
        mutex_unlock(&ScreenCaptureMutex);
        return;
    }

    MMProfileLog(MTKFB_MMP_Events.EarlySuspend, MMProfileFlagStart);
    is_early_suspended = TRUE;

    DISP_PrepareSuspend();
    // Wait for disp finished.
    if (wait_event_interruptible_timeout(disp_done_wq, !disp_running, HZ/10) == 0)
    {
        printk("[FB Driver] Wait disp finished timeout in early_suspend\n");
    }
#if defined (MTK_FB_SYNC_SUPPORT)
    for(i=0;i<HW_OVERLAY_COUNT;i++)
    {
        disp_sync_release(disp_config.session_id, i);
        MTKFB_LOG("[FB driver] layer%d release fences\n",i);		
    }
#endif
    DISP_CHECK_RET(DISP_PanelEnable(FALSE));
    DISP_CHECK_RET(DISP_PowerEnable(FALSE));

    DISP_CHECK_RET(DISP_PauseVsync(TRUE));
    disp_path_clock_off("mtkfb");
#if defined (MTK_FB_SYNC_SUPPORT)
	//disp_sync_deinit();
#endif
    //MMProfileLogEx(MTKFB_MMP_Events.EarlySuspend, MMProfileFlagEnd, 0, 0);
    sem_early_suspend_cnt++;
    up(&sem_early_suspend);
    mutex_unlock(&ScreenCaptureMutex);

    printk("[FB Driver] leave early_suspend\n");

    MSG_FUNC_LEAVE();
	aee_kernel_wdt_kick_Powkey_api("mtkfb_early_suspend",WDT_SETBY_Display); 
}
#endif

/* PM resume */
static int mtkfb_resume(struct device *pdev)
{
    NOT_REFERENCED(pdev);
    MSG_FUNC_ENTER();
    MTKFB_LOG("[FB Driver] mtkfb_resume()\n");
    MSG_FUNC_LEAVE();
    return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void mtkfb_late_resume(struct early_suspend *h)
{
    MSG_FUNC_ENTER();

    printk("[FB Driver] enter late_resume\n");
    mutex_lock(&ScreenCaptureMutex);
    if (down_interruptible(&sem_early_suspend)) {
        printk("[FB Driver] can't get semaphore in mtkfb_late_resume()\n");
        mutex_unlock(&ScreenCaptureMutex);
        return;
    }
	sem_early_suspend_cnt--;
    //MMProfileLogEx(MTKFB_MMP_Events.EarlySuspend, MMProfileFlagStart, 0, 0);

    MMProfileLog(MTKFB_MMP_Events.EarlySuspend, MMProfileFlagEnd);
    if (is_ipoh_bootup)
    {
        //atomic_set(&OverlaySettingDirtyFlag, 0);
        is_video_mode_running = true;
    }
    else
    {
        disp_path_clock_on("mtkfb");
    }
    printk("[FB LR] 1\n");
    DISP_CHECK_RET(DISP_PauseVsync(FALSE));
    printk("[FB LR] 2\n");
    DISP_CHECK_RET(DISP_PowerEnable(TRUE));
    printk("[FB LR] 3\n");
    DISP_CHECK_RET(DISP_PanelEnable(TRUE));
    printk("[FB LR] 4\n");

    is_early_suspended = FALSE;

    if (is_ipoh_bootup)
    {
        DISP_StartConfigUpdate();
    }
    else
    {
        mtkfb_clear_lcm();
    }

	sem_early_suspend_cnt++;
    up(&sem_early_suspend);
    mutex_unlock(&ScreenCaptureMutex);

	if(BL_set_level_resume){
		mtkfb_set_backlight_level(BL_level);
		BL_set_level_resume = FALSE;
		}

    printk("[FB Driver] leave late_resume\n");

    MSG_FUNC_LEAVE();
	aee_kernel_wdt_kick_Powkey_api("mtkfb_late_resume",WDT_SETBY_Display); 
}
#endif

/*---------------------------------------------------------------------------*/
#ifdef CONFIG_PM
/*---------------------------------------------------------------------------*/
int mtkfb_pm_suspend(struct device *device)
{
    //pr_debug("calling %s()\n", __func__);

    struct platform_device *pdev = to_platform_device(device);
    BUG_ON(pdev == NULL);

    return mtkfb_suspend((struct device *)pdev, PMSG_SUSPEND);
}

int mtkfb_pm_resume(struct device *device)
{
    //pr_debug("calling %s()\n", __func__);

    struct platform_device *pdev = to_platform_device(device);
    BUG_ON(pdev == NULL);

    return mtkfb_resume((struct device *)pdev);
}

#ifdef DEFAULT_MMP_ENABLE
void MMProfileStart(int start);
#endif
int mtkfb_pm_restore_noirq(struct device *device)
{

#ifdef DEFAULT_MMP_ENABLE
    MMProfileStart(0);
    MMProfileStart(1);
#endif

    disphal_pm_restore_noirq(device);
    is_ipoh_bootup = true;
    return 0;

}

int mtkfb_pm_restore_early(struct device *device)
{
    // sometime disp_path_clock  will control i2c, when IPOH,
    // there is no irq in mtkfb_pm_restore_noirq , i2c will timeout.
    // so move this to  resore early.
    disp_path_clock_on("ipoh_mtkfb");
    return 0;
}

/*---------------------------------------------------------------------------*/
#else /*CONFIG_PM*/
/*---------------------------------------------------------------------------*/
#define mtkfb_pm_suspend NULL
#define mtkfb_pm_resume  NULL
#define mtkfb_pm_restore_noirq NULL
#define mtkfb_pm_restore_early NULL
/*---------------------------------------------------------------------------*/
#endif /*CONFIG_PM*/
/*---------------------------------------------------------------------------*/
struct dev_pm_ops mtkfb_pm_ops = {
    .suspend = mtkfb_pm_suspend,
    .resume = mtkfb_pm_resume,
    .freeze = mtkfb_pm_suspend,
    .thaw = mtkfb_pm_resume,
    .poweroff = mtkfb_pm_suspend,
    .restore = mtkfb_pm_resume,
    .restore_noirq = mtkfb_pm_restore_noirq,
    .restore_early = mtkfb_pm_restore_early,
};

static struct platform_driver mtkfb_driver =
{
    .driver = {
        .name    = MTKFB_DRIVER,
#ifdef CONFIG_PM
        .pm     = &mtkfb_pm_ops,
#endif
        .bus     = &platform_bus_type,
        .probe   = mtkfb_probe,
        .remove  = mtkfb_remove,
        .suspend = mtkfb_suspend,
        .resume  = mtkfb_resume,
		.shutdown = mtkfb_shutdown,
    },
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend mtkfb_early_suspend_handler =
{
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB,
	.suspend = mtkfb_early_suspend,
	.resume = mtkfb_late_resume,
};
#endif

#ifdef DEFAULT_MMP_ENABLE
void MMProfileEnable(int enable);
void MMProfileStart(int start);
void init_mtkfb_mmp_events(void);
void init_ddp_mmp_events(void);
#endif

/* Register both the driver and the device */
int __init mtkfb_init(void)
{
    int r = 0;

    MSG_FUNC_ENTER();

#ifdef DEFAULT_MMP_ENABLE
    MMProfileEnable(1);
	init_mtkfb_mmp_events();
    init_ddp_mmp_events();
    MMProfileStart(1);
#endif

    /* Register the driver with LDM */

    if (platform_driver_register(&mtkfb_driver)) {
        PRNERR("failed to register mtkfb driver\n");
        r = -ENODEV;
        goto exit;
    }

#ifdef CONFIG_HAS_EARLYSUSPEND
   	register_early_suspend(&mtkfb_early_suspend_handler);
#endif

    DBG_Init();
//#ifdef MTK_DISP_CONFIG_SUPPORT
	ConfigPara_Init();//In order to Trigger Display Customization Tool..
//#endif
exit:
    MSG_FUNC_LEAVE();
    return r;
}


static void __exit mtkfb_cleanup(void)
{
    MSG_FUNC_ENTER();

    platform_driver_unregister(&mtkfb_driver);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&mtkfb_early_suspend_handler);
#endif

    kthread_stop(screen_update_task);
    if(esd_recovery_task)
         kthread_stop(esd_recovery_task);

    DBG_Deinit();
	//#ifdef MTK_DISP_CONFIG_SUPPORT
	ConfigPara_Deinit();//clean up Display Customization Tool...
	//#endif
    MSG_FUNC_LEAVE();
}


module_init(mtkfb_init);
module_exit(mtkfb_cleanup);

MODULE_DESCRIPTION("MEDIATEK framebuffer driver");
MODULE_AUTHOR("Zaikuo Wang <zaikuo.wang@mediatek.com>");
MODULE_LICENSE("GPL");
