#include <mach/mt_typedefs.h>
#include <linux/types.h>
#include "disp_drv.h"
#include "ddp_hal.h"
#include "disp_drv_log.h"
#include <linux/disp_assert_layer.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>

///common part
#define DAL_BPP             (2)
#define DAL_WIDTH           (DISP_GetScreenWidth())
#define DAL_HEIGHT          (DISP_GetScreenHeight())

#define PITCH_ALIGN  (128)

#define ALIGN_TO(x, n)  \
	(((x) + ((n) - 1)) & ~((n) - 1))

#define DAL_PITCH           ( ALIGN_TO((DAL_WIDTH*DAL_BPP),PITCH_ALIGN)/DAL_BPP)


#ifdef CONFIG_MTK_FB_SUPPORT_ASSERTION_LAYER

#include <linux/string.h>
#include <linux/semaphore.h>
#include <asm/cacheflush.h>
#include <linux/module.h>

#include "mtkfb_console.h"
#include "disp_mgr.h"
#include "disp_svp.h"
extern disp_session_config disp_config;
// ---------------------------------------------------------------------------
#define DAL_FORMAT          (eRGB565)
#define DAL_BG_COLOR        (dal_bg_color)
#define DAL_FG_COLOR        (dal_fg_color)

#define RGB888_To_RGB565(x) ((((x) & 0xF80000) >> 8) |                      \
                             (((x) & 0x00FC00) >> 5) |                      \
                             (((x) & 0x0000F8) >> 3))

#define MAKE_TWO_RGB565_COLOR(high, low)  (((low) << 16) | (high))

#define DAL_LOCK()                                                          \
    do {                                                                    \
        if (down_interruptible(&dal_sem)) {                                 \
            DISP_LOG_PRINT(ANDROID_LOG_WARN, "DAL", "Can't get semaphore in %s()\n",          \
                   __FUNCTION__);                                           \
            return DAL_STATUS_LOCK_FAIL;                                    \
        }                                                                   \
    } while (0)
    
#define DAL_UNLOCK()                                                        \
    do {                                                                    \
        up(&dal_sem);                                                       \
    } while (0)


#define DAL_CHECK_MFC_RET(expr)                                             \
    do {                                                                    \
        MFC_STATUS ret = (expr);                                            \
        if (MFC_STATUS_OK != ret) {                                         \
            DISP_LOG_PRINT(ANDROID_LOG_WARN, "DAL", "Warning: call MFC_XXX function failed "           \
                   "in %s(), line: %d, ret: %x\n",                          \
                   __FUNCTION__, __LINE__, ret);                            \
            return ret;                                                     \
        }                                                                   \
    } while (0)


#define DAL_CHECK_DISP_RET(expr)                                            \
    do {                                                                    \
        DISP_STATUS ret = (expr);                                           \
        if (DISP_STATUS_OK != ret) {                                        \
            DISP_LOG_PRINT(ANDROID_LOG_WARN, "DAL", "Warning: call DISP_XXX function failed "          \
                   "in %s(), line: %d, ret: %x\n",                          \
                   __FUNCTION__, __LINE__, ret);                            \
            return ret;                                                     \
        }                                                                   \
    } while (0)

#define DAL_LOG(fmt, arg...) \
    do { \
        DISP_LOG_PRINT(ANDROID_LOG_INFO, "DAL", fmt, ##arg); \
    }while (0)
// ---------------------------------------------------------------------------

static MFC_HANDLE mfc_handle = NULL;

static void *dal_fb_addr = NULL;  
unsigned int dal_fb_pa = 0;
unsigned int isAEEEnabled = 0;
extern BOOL is_early_suspended;

//static BOOL  dal_shown   = FALSE;
BOOL  dal_shown   = FALSE;
static BOOL  dal_enable_when_resume = FALSE;
static BOOL  dal_disable_when_resume = FALSE;
static unsigned int dal_fg_color = RGB888_To_RGB565(DAL_COLOR_WHITE);
static unsigned int dal_bg_color = RGB888_To_RGB565(DAL_COLOR_RED);

#define DAL_LOWMEMORY_ASSERT

#ifdef DAL_LOWMEMORY_ASSERT

static unsigned int dal_lowmemory_fg_color = RGB888_To_RGB565(DAL_COLOR_PINK);
static unsigned int dal_lowmemory_bg_color = RGB888_To_RGB565(DAL_COLOR_YELLOW);
static BOOL  dal_enable_when_resume_lowmemory = FALSE;
static BOOL  dal_disable_when_resume_lowmemory = FALSE;
static BOOL  dal_lowMemory_shown = FALSE;
static const char low_memory_msg[] = {"Low Memory!"};
#define DAL_LOWMEMORY_FG_COLOR (dal_lowmemory_fg_color)
#define DAL_LOWMEMORY_BG_COLOR (dal_lowmemory_bg_color)
#endif
//DECLARE_MUTEX(dal_sem);
DEFINE_SEMAPHORE(dal_sem);

static char dal_print_buffer[1024];

static void DAL_UpdateScreen (BOOL enable, BOOL aen, UINT32 width, UINT32 height) {
	disp_job *job = disp_deque_job(disp_config.session_id);
	mutex_lock(&job->lock);
	job->input[ASSERT_LAYER].layer_id = ASSERT_LAYER;
	job->input[ASSERT_LAYER].layer_enable = enable;
	job->input[ASSERT_LAYER].dirty = TRUE;
	if (enable) {
		job->input[ASSERT_LAYER].src_x = DAL_WIDTH - width;
		job->input[ASSERT_LAYER].src_y = 0;
		job->input[ASSERT_LAYER].dst_x = DAL_WIDTH - width;
		job->input[ASSERT_LAYER].dst_y = 0;
		job->input[ASSERT_LAYER].width = width;
		job->input[ASSERT_LAYER].height = height;
		job->input[ASSERT_LAYER].pitch = DAL_PITCH * DAL_BPP;
		job->input[ASSERT_LAYER].format = DAL_FORMAT;
		job->input[ASSERT_LAYER].address = dal_fb_pa;
		job->input[ASSERT_LAYER].alpha_enable = aen;
		if (aen) {
			job->input[ASSERT_LAYER].alpha = 0x80;
		}
	}
	mutex_unlock(&job->lock);
	printk("[FB Driver] DAL_UpdateScreen(): id=%d,en=%d,aen=%d,fmt=%d,pitch=%d,w=%d,h=%d\n",
	        ASSERT_LAYER, enable, aen, DP_COLOR_GET_UNIQUE_ID(DAL_FORMAT), (int)(DAL_PITCH * DAL_BPP), width, height);
	disp_enque_job(disp_config.session_id);
	if (DISP_IsDecoupleMode()) {
		DISP_StartOverlayTransfer();
	}
}

#ifdef DAL_LOWMEMORY_ASSERT
static DAL_STATUS Show_LowMemory(void)
{
	UINT32 update_width, update_height;
	MFC_CONTEXT *ctxt = (MFC_CONTEXT *)mfc_handle;

	if(!dal_shown){//only need show lowmemory assert
		update_width = ctxt->font_width * strlen(low_memory_msg);
		update_height = ctxt->font_height;
		DISP_LOG_PRINT(ANDROID_LOG_INFO, "DAL", "update size:%d,%d",update_width, update_height);
		DAL_UpdateScreen(TRUE, FALSE, update_width, update_height);
	}
/*
	if(!dal_lowMemory_shown)
		DAL_CHECK_DISP_RET(DISP_UpdateScreen(0, 0, 
                                         DAL_WIDTH,
                                         DAL_HEIGHT));
*/
    return DAL_STATUS_OK;
}
#endif

UINT32 DAL_GetLayerSize(void)
{
	// xuecheng, avoid lcdc read buffersize+1 issue
    return DAL_PITCH* DAL_HEIGHT * DAL_BPP + 4096;
}

int DAL_address_burst_align(void)
{
    int align = 0;
    int burst_mod = DAL_WIDTH*DAL_BPP % (8*DAL_BPP);
    int burst = DAL_WIDTH*DAL_BPP/(8*DAL_BPP) + (burst_mod ?1:0);

    int burst_8mod = burst%8;
    if(burst_8mod < 4)
    {
         align = 0x40;
    }
    printk("dal address align 0x%x\n",align);
    return align;
}

DAL_STATUS DAL_SetScreenColor(DAL_COLOR color)
{
	UINT32 i;
	UINT32 size,offset;
	UINT32 BG_COLOR;
	UINT32 *addr;
	MFC_CONTEXT *ctxt;
	color=RGB888_To_RGB565(color);
	BG_COLOR = MAKE_TWO_RGB565_COLOR(color, color);
	
	ctxt = (MFC_CONTEXT *)mfc_handle;
	if(!ctxt)
		return DAL_STATUS_FATAL_ERROR; 
	if(ctxt->screen_color==color)
		return DAL_STATUS_OK;
	offset =  MFC_Get_Cursor_Offset(mfc_handle);
	addr=(UINT32*)(ctxt->fb_addr+offset);
	
	size= DAL_GetLayerSize()-offset - DAL_address_burst_align();
	for(i = 0; i < size/ sizeof(UINT32); ++ i) 
	{
	    *addr ++ = BG_COLOR;
	}
	ctxt->screen_color=color;

    return DAL_STATUS_OK;
}

DAL_STATUS DAL_Init(UINT32 layerVA, UINT32 layerPA)
{
    int align;
	align = DAL_address_burst_align();
    dal_fb_addr = (void *)(layerVA+align);
    dal_fb_pa = layerPA+align;
    DAL_CHECK_MFC_RET(MFC_Open_Ex(&mfc_handle, dal_fb_addr,
                               DAL_WIDTH, DAL_HEIGHT, DAL_PITCH, DAL_BPP,
                               DAL_FG_COLOR, DAL_BG_COLOR));

    //DAL_Clean();
    DAL_SetScreenColor(DAL_COLOR_RED);

    return DAL_STATUS_OK;
}


DAL_STATUS DAL_SetColor(unsigned int fgColor, unsigned int bgColor)
{
    if (NULL == mfc_handle) 
        return DAL_STATUS_NOT_READY;

    DAL_LOCK();
    dal_fg_color = RGB888_To_RGB565(fgColor);
    dal_bg_color = RGB888_To_RGB565(bgColor);
    DAL_CHECK_MFC_RET(MFC_SetColor(mfc_handle, dal_fg_color, dal_bg_color));
    DAL_UNLOCK();

    return DAL_STATUS_OK;
}

DAL_STATUS DAL_Dynamic_Change_FB_Layer(unsigned int isAEEEnabled)
{
    //static int ui_layer_tdshp = 0;

    printk("[DDP] DAL_Dynamic_Change_FB_Layer, isAEEEnabled=%d \n", isAEEEnabled);
    
    if(DISP_DEFAULT_UI_LAYER_ID==DISP_CHANGED_UI_LAYER_ID)
    {
    	 printk("[DDP] DAL_Dynamic_Change_FB_Layer, no dynamic switch \n");
    	 return DAL_STATUS_OK;
    }
    
    if(isAEEEnabled==1)
    {
        // change ui layer from DISP_DEFAULT_UI_LAYER_ID to DISP_CHANGED_UI_LAYER_ID
        //ui_layer_tdshp = cached_layer_config[DISP_DEFAULT_UI_LAYER_ID].isTdshp;
        //cached_layer_config[DISP_DEFAULT_UI_LAYER_ID].isTdshp = 0;
 	//disp_path_change_tdshp_status(DISP_DEFAULT_UI_LAYER_ID, 0); // change global variable value, else error-check will find layer 2, 3 enable tdshp together
        FB_LAYER = DISP_CHANGED_UI_LAYER_ID;
    }
    else
    {
        //cached_layer_config[DISP_DEFAULT_UI_LAYER_ID].isTdshp = ui_layer_tdshp;
        FB_LAYER = DISP_DEFAULT_UI_LAYER_ID;
    }

    return DAL_STATUS_OK;
}

DAL_STATUS DAL_Clean(void)
{
    DAL_STATUS ret = DAL_STATUS_OK;
    MFC_CONTEXT *ctxt;

    static int dal_clean_cnt = 0;
    
    printk("[MTKFB_DAL] DAL_Clean\n");
    if (NULL == mfc_handle) 
        return DAL_STATUS_NOT_READY;

//    if (LCD_STATE_POWER_OFF == LCD_GetState())
//        return DAL_STATUS_LCD_IN_SUSPEND;

    DAL_LOCK();

    DAL_CHECK_MFC_RET(MFC_ResetCursor(mfc_handle));

    ctxt = (MFC_CONTEXT *)mfc_handle;
    ctxt->screen_color=0;
    DAL_SetScreenColor(DAL_COLOR_RED);

    //TODO: if dal_shown=false, and 3D enabled, mtkfb may disable UI layer, please modify 3D driver
    if(isAEEEnabled==1)
    {
        // DAL disable, switch UI layer to default layer 3
        printk("[DDP]* isAEEEnabled from 1 to 0, %d \n", dal_clean_cnt++);
        isAEEEnabled = 0;
        DAL_Dynamic_Change_FB_Layer(isAEEEnabled);  // restore UI layer to DEFAULT_UI_LAYER
    }
    
    dal_shown = FALSE;
#ifdef DAL_LOWMEMORY_ASSERT
   	if (dal_lowMemory_shown) {//only need show lowmemory assert
		UINT32 LOWMEMORY_FG_COLOR = MAKE_TWO_RGB565_COLOR(DAL_LOWMEMORY_FG_COLOR, DAL_LOWMEMORY_FG_COLOR);
        UINT32 LOWMEMORY_BG_COLOR = MAKE_TWO_RGB565_COLOR(DAL_LOWMEMORY_BG_COLOR, DAL_LOWMEMORY_BG_COLOR);
        
        DAL_CHECK_MFC_RET(MFC_LowMemory_Printf(mfc_handle, low_memory_msg, LOWMEMORY_FG_COLOR, LOWMEMORY_BG_COLOR));
        Show_LowMemory();
    }
    dal_enable_when_resume_lowmemory = FALSE;
    dal_disable_when_resume_lowmemory = FALSE;
#endif
    dal_disable_when_resume = FALSE;
    DAL_UpdateScreen(FALSE, FALSE, 0, 0);
    DAL_CHECK_DISP_RET(DISP_UpdateScreen(0, 0, DAL_WIDTH, DAL_HEIGHT));

    DAL_UNLOCK();
    return ret;
}


DAL_STATUS DAL_Printf(const char *fmt, ...)
{
    va_list args;
    uint i;
    DAL_STATUS ret = DAL_STATUS_OK;
    printk("%s", __func__);
    //printk("[MTKFB_DAL] DAL_Printf mfc_handle=0x%08X, fmt=0x%08X\n", mfc_handle, fmt);
    if (NULL == mfc_handle) 
        return DAL_STATUS_NOT_READY;
    
    if (NULL == fmt)
        return DAL_STATUS_INVALID_ARGUMENT;
    
    DAL_LOCK();
     if(isAEEEnabled==0)
    {
        printk("[DDP] isAEEEnabled from 0 to 1, ASSERT_LAYER=%d, dal_fb_pa %x\n", 
            ASSERT_LAYER, dal_fb_pa);
            
        isAEEEnabled = 1;
        DAL_Dynamic_Change_FB_Layer(isAEEEnabled); // default_ui_ layer coniig to changed_ui_layer
        
        DAL_CHECK_MFC_RET(MFC_Open_Ex(&mfc_handle, dal_fb_addr,
                               DAL_WIDTH, DAL_HEIGHT,DAL_PITCH, DAL_BPP,
                               DAL_FG_COLOR, DAL_BG_COLOR));        
    }
    va_start (args, fmt);
    i = vsprintf(dal_print_buffer, fmt, args);
    BUG_ON(i>=ARRAY_SIZE(dal_print_buffer));
    va_end (args);
    DAL_CHECK_MFC_RET(MFC_Print(mfc_handle, dal_print_buffer));

    flush_cache_all();

    if (!dal_shown) {
        dal_shown = TRUE;
    }
    DAL_UpdateScreen(TRUE, TRUE, DAL_WIDTH, DAL_HEIGHT);
    DAL_CHECK_DISP_RET(DISP_UpdateScreen(0, 0, DAL_WIDTH, DAL_HEIGHT));

    DAL_UNLOCK();
    return ret;
}

DAL_STATUS DAL_OnDispPowerOn(void)
{
    DAL_LOCK();

    /* Re-enable assertion layer when display resumes */
    
    if (is_early_suspended)
    {
		if(dal_enable_when_resume) 
		{
        	dal_enable_when_resume = FALSE;
        	if (!dal_shown) 
        	{
        		DAL_UpdateScreen(TRUE, FALSE, DAL_WIDTH, DAL_HEIGHT);
            	dal_shown = TRUE;
        	}
#ifdef DAL_LOWMEMORY_ASSERT
			dal_enable_when_resume_lowmemory = FALSE;
			dal_disable_when_resume_lowmemory = FALSE;
#endif
			goto End;
    	}
		else if(dal_disable_when_resume)
		{
			dal_disable_when_resume = FALSE;
			DAL_UpdateScreen(FALSE, FALSE, 0, 0);
			dal_shown = FALSE;
#ifdef DAL_LOWMEMORY_ASSERT
        	if (dal_lowMemory_shown) 
        	{//only need show lowmemory assert	
				UINT32 LOWMEMORY_FG_COLOR = MAKE_TWO_RGB565_COLOR(DAL_LOWMEMORY_FG_COLOR, DAL_LOWMEMORY_FG_COLOR);
				UINT32 LOWMEMORY_BG_COLOR = MAKE_TWO_RGB565_COLOR(DAL_LOWMEMORY_BG_COLOR, DAL_LOWMEMORY_BG_COLOR);

				DAL_CHECK_MFC_RET(MFC_LowMemory_Printf(mfc_handle, low_memory_msg, LOWMEMORY_FG_COLOR, LOWMEMORY_BG_COLOR));
				Show_LowMemory();			
        	}
			dal_enable_when_resume_lowmemory = FALSE;
			dal_disable_when_resume_lowmemory = FALSE;
#endif
			goto End;

		}
#ifdef DAL_LOWMEMORY_ASSERT
		if(dal_enable_when_resume_lowmemory)
		{
			dal_enable_when_resume_lowmemory = FALSE;
			if(!dal_shown)
			{//only need show lowmemory assert
				Show_LowMemory();
			}
		}
		else if(dal_disable_when_resume_lowmemory)
		{
			if(!dal_shown)
			{// only low memory assert shown on screen
				DAL_UpdateScreen(FALSE, FALSE, 0, 0);
			}
		}
		else{}
#endif
	}

End:
    DAL_UNLOCK();

    return DAL_STATUS_OK;
}

#ifdef DAL_LOWMEMORY_ASSERT
DAL_STATUS DAL_LowMemoryOn(void)
{
	UINT32 LOWMEMORY_FG_COLOR = MAKE_TWO_RGB565_COLOR(DAL_LOWMEMORY_FG_COLOR, DAL_LOWMEMORY_FG_COLOR);
	UINT32 LOWMEMORY_BG_COLOR = MAKE_TWO_RGB565_COLOR(DAL_LOWMEMORY_BG_COLOR, DAL_LOWMEMORY_BG_COLOR);
	DAL_LOG("Enter DAL_LowMemoryOn()\n");

	DAL_CHECK_MFC_RET(MFC_LowMemory_Printf(mfc_handle, low_memory_msg, LOWMEMORY_FG_COLOR, LOWMEMORY_BG_COLOR));
	
	Show_LowMemory();

	dal_lowMemory_shown = TRUE;
	DAL_LOG("Leave DAL_LowMemoryOn()\n");
	return DAL_STATUS_OK;
}

DAL_STATUS DAL_LowMemoryOff(void)
{
	UINT32 BG_COLOR = MAKE_TWO_RGB565_COLOR(DAL_BG_COLOR, DAL_BG_COLOR);
	DAL_LOG("Enter DAL_LowMemoryOff()\n");

	DAL_CHECK_MFC_RET(MFC_SetMem(mfc_handle, low_memory_msg, BG_COLOR));
	
//what about LCM_PHYSICAL_ROTATION = 180
	if(!dal_shown)
	{// only low memory assert shown on screen
		DAL_UpdateScreen(FALSE, FALSE, 0, 0);
	}
	dal_lowMemory_shown = FALSE;
	DAL_LOG("Leave DAL_LowMemoryOff()\n");
    return DAL_STATUS_OK;
}
#endif
// ##########################################################################
//  !CONFIG_MTK_FB_SUPPORT_ASSERTION_LAYER
// ##########################################################################
#else

UINT32 DAL_GetLayerSize(void)
{
	// xuecheng, avoid lcdc read buffersize+1 issue
    //return DAL_WIDTH * DAL_HEIGHT * DAL_BPP + 4096;
    return DAL_PITCH * DAL_HEIGHT * DAL_BPP + 4096;
}

DAL_STATUS DAL_Init(UINT32 layerVA, UINT32 layerPA)
{
    NOT_REFERENCED(layerVA);
    NOT_REFERENCED(layerPA);
    
    return DAL_STATUS_OK;
}
DAL_STATUS DAL_SetColor(unsigned int fgColor, unsigned int bgColor)
{
    NOT_REFERENCED(fgColor);
    NOT_REFERENCED(bgColor);

    return DAL_STATUS_OK;
}
DAL_STATUS DAL_Clean(void)
{
    printk("[MTKFB_DAL] DAL_Clean is not implemented\n");
    return DAL_STATUS_OK;
}
DAL_STATUS DAL_Printf(const char *fmt, ...)
{
    NOT_REFERENCED(fmt);
    printk("[MTKFB_DAL] DAL_Printf is not implemented\n");
    return DAL_STATUS_OK;
}

DAL_STATUS DAL_OnDispPowerOn(void)
{
    return DAL_STATUS_OK;
}
#ifdef DAL_LOWMEMORY_ASSERT
DAL_STATUS DAL_LowMemoryOn(void)
{
    return DAL_STATUS_OK;
}
DAL_STATUS DAL_LowMemoryOff(void)
{
    return DAL_STATUS_OK;
}
#endif
#endif  // CONFIG_MTK_FB_SUPPORT_ASSERTION_LAYER
EXPORT_SYMBOL(DAL_SetScreenColor);
EXPORT_SYMBOL(DAL_SetColor);
EXPORT_SYMBOL(DAL_Printf);
EXPORT_SYMBOL(DAL_Clean);
#ifdef DAL_LOWMEMORY_ASSERT
EXPORT_SYMBOL(DAL_LowMemoryOn);
EXPORT_SYMBOL(DAL_LowMemoryOff);
#endif

