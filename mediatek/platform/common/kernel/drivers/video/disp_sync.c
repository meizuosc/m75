/**
 * Sync Object support in Display Driver
 */
#include <linux/ion_drv.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/spinlock.h>

#define LOG_TAG "SYNC"

#include "disp_sync.h"
#include "debug.h"
#include "mtk_sync.h"
// Client interface definition
#include "mtkfb.h"
#include "disp_drv.h"

// how many counters prior to current timeline real-time counter
#define FENCE_STEP_COUNTER          1
#define DDP_OVL_LAYER_COUNT     	4

///=============================================================================
// external variables declarations
///==========================
extern BOOL isAEEEnabled;
static BOOL log_on = 0;
void disp_sync_log_on(BOOL on) {
	log_on = on;
}

///=============================================================================
// structure declarations
///===========================
typedef struct buffer_info_t {
	struct list_head list;
	UINT idx;
	int fence;
	struct ion_handle *hnd;
	UINT mva;
	BOOL cache_sync;
}buffer_info;

typedef struct sync_info_t {
	struct mutex lock;
	UINT layer_id;
	UINT fence_idx;
	UINT timeline_idx;
	UINT inc;
	UINT cur_idx;
	struct list_head buf_list;
}sync_info;

///=============================================================================
// local variables
///==========================
static struct ion_client *ion_client;
static struct sw_sync_timeline* sync_timelines[DDP_OVL_LAYER_COUNT];
static sync_info layer_infos[DDP_OVL_LAYER_COUNT] =
{
		{.layer_id = DDP_OVL_LAYER_COUNT},
		{.layer_id = DDP_OVL_LAYER_COUNT},
		{.layer_id = DDP_OVL_LAYER_COUNT},
		{.layer_id = DDP_OVL_LAYER_COUNT}
};
static LIST_HEAD(info_pool_head);
static DEFINE_MUTEX(fence_buffer_mutex);

///=============================================================================
// local function forward declarations
///=============================
static struct ion_client* disp_sync_ion_init (void);
static void disp_sync_ion_deinit (void);
static SYNC_STATUS disp_sync_ion_import_handle(struct ion_client *client, int fd, struct ion_handle **hnd);
static void disp_sync_ion_free_handle(struct ion_client *client, struct ion_handle *handle);
static void disp_sync_ion_config_buffer (struct ion_client *client, struct ion_handle *handle);
static size_t disp_sync_ion_phys_mmu_addr(struct ion_client *client, struct ion_handle *handle, UINT *mva);

static UINT disp_sync_get_fence_index (UINT layer);
static UINT disp_sync_inc_fence_index (UINT layer) ;
static sync_info* disp_sync_init_layer_info(UINT layer);
static buffer_info* disp_sync_init_buf_info(buffer_info *buf);
static buffer_info* disp_sync_get_buf_info (void);
static void disp_sync_free_buf_pool (void);
static struct sw_sync_timeline* disp_sync_create_timeline (UINT layer);
static struct fence_data disp_sync_create_fence(UINT layer);

///=============================================================================
// global function definitions
///=============================
SYNC_STATUS disp_sync_ion_alloc_buffer(struct ion_client *client, int ion_fd, UINT *mva, struct ion_handle **hnd) {

	if (disp_sync_ion_import_handle(ion_client, ion_fd, hnd) != SYNC_STATUS_OK) {
		return SYNC_STATUS_ERROR;
	}
	disp_sync_ion_config_buffer(client, *hnd);
	disp_sync_ion_phys_mmu_addr(client, *hnd, mva);

	return SYNC_STATUS_OK;
}

void disp_sync_ion_cache_flush(struct ion_client *client, struct ion_handle *handle) {
	struct ion_sys_data sys_data;
	if (IS_ERR_OR_NULL(ion_client) || IS_ERR_OR_NULL(handle)) {
		return ;
	}
	sys_data.sys_cmd = ION_SYS_CACHE_SYNC;
	sys_data.cache_sync_param.handle = handle;
	sys_data.cache_sync_param.sync_type = ION_CACHE_FLUSH_BY_RANGE;

	if (ion_kernel_ioctl(client, ION_CMD_SYSTEM, (int)&sys_data)) {
		XLOG_ERR("ion cache flush failed!\n");
	}
}

UINT disp_sync_query_buffer_mva(UINT layer, UINT idx) {
	sync_info *info;
	buffer_info *buf;
	UINT mva = 0x0;
	ASSERT(layer < DDP_OVL_LAYER_COUNT, "layer_id(%d) overflow!", layer);
	info = &layer_infos[layer];

	mutex_lock(&info->lock);
	list_for_each_entry(buf, &info->buf_list, list) {
		if (buf->idx == idx) {
			mva = buf->mva;
			break;
		}
	}
	mutex_unlock(&info->lock);
	if (mva != 0x0) {
		if (buf->cache_sync) {
			disp_sync_ion_cache_flush(ion_client, buf->hnd);
		}
	} else {
		//FIXME: non-ion buffer need cache sync here?
		XLOG_ERR("L%d query mva failed idx=%d!\n", layer, idx);
	}
	return mva;
}

void disp_sync_release_buffer(UINT layer) {
	sync_info *info;
	buffer_info *pos, *n;
	UINT index;
	ASSERT(layer < DDP_OVL_LAYER_COUNT, "layer_id(%d) overflow!", layer);
	info = &layer_infos[layer];

	mutex_lock(&info->lock);
	index = info->timeline_idx;
	list_for_each_entry_safe(pos, n, &info->buf_list, list) {
		if (pos->idx <= index) {
			//XLOG_DBG("L%d release buffer idx=%d\n", layer, pos->idx);
			list_del_init(&pos->list);

			disp_sync_ion_free_handle(ion_client, pos->hnd);

			mutex_lock(&fence_buffer_mutex);
			list_add_tail(&pos->list, &info_pool_head);
			mutex_unlock(&fence_buffer_mutex);
		}
	}
	mutex_unlock(&info->lock);
}

/**
 * Release fence
 * timeline_idx will step forward and present current hw used buff index
  * Covered case:
 * case 1: prepare layer1-bufA, set_overlay layer1-bufA, prepare layer1-bufB, set_overlay layer1-bufB
 * case 2: prepare layer1-bufA, prepare layer1-bufB, set_overlay layer1-bufA, set_overlay layer1-bufB
 * case 3: prepare layer1-bufA, prepare layer1-bufB, set_multiple_overlay layer1-bufA & layer1-bufB
 * case 4: prepare layer1-bufA, prepare layer1-bufB, disable layer1-bufA, set_overlay layer1-bufB
 * NOTICE:
 *     Frame dropping maybe happen, we has no cache FIFO now!
 *     When a new buffer is coming, all prior to it will be released
 *     Buf will be released immediately if ovl_layer is disabled
 *     If just other property changes but not addr, do nothing, because
 *     fence_idx not changed.
 */
void disp_sync_signal_fence(UINT layer, UINT cur_idx) {
	sync_info *info;
	struct sw_sync_timeline *timeline;
	UINT num_fence = 0;
	ASSERT(layer < DDP_OVL_LAYER_COUNT, "layer_id(%d) overflow!", layer);
	info = &layer_infos[layer];
	timeline = sync_timelines[layer];

	if (timeline != NULL) {
		mutex_lock(&info->lock);
    	num_fence = cur_idx - info->timeline_idx;
		if (num_fence > 0) {
			MMProfileLogEx(MTKFB_MMP_Events.SignalSyncFence, MMProfileFlagStart, layer, info->timeline_idx);
			timeline_inc(timeline, num_fence);
			info->timeline_idx = cur_idx;
			XLOG_DBG("L%d signal idx=%d,cnt=%d\n", layer, info->timeline_idx, num_fence);
			MMProfileLogEx(MTKFB_MMP_Events.SignalSyncFence, MMProfileFlagEnd, layer, timeline->value);
		}
		mutex_unlock(&info->lock);
		if(cur_idx != timeline->value) {
		    XLOG_WARN("L%d idx=%d, value=%d\n", layer, cur_idx, timeline->value);
		}
	}
}

void disp_sync_release(UINT layer) {
	sync_info *info;
	int index = 0;
	ASSERT(layer < DDP_OVL_LAYER_COUNT, "layer_id(%d) overflow!", layer);
	info = &layer_infos[layer];
	mutex_lock(&info->lock);
	index = info->fence_idx;
	mutex_unlock(&info->lock);
	XLOG_DBG("L%d release all %d\n", layer, index);
	disp_sync_signal_fence(layer, index);
	disp_sync_release_buffer(layer);
}

SYNC_STATUS disp_sync_prepare_buffer(struct fb_overlay_buffer *buf) {
	sync_info *info;
	struct sw_sync_timeline *timeline;
	buffer_info *buf_info;
	struct fence_data data;
	struct ion_handle * handle = NULL;
	UINT mva = 0x0;
	const int layer = buf->layer_id;
	ASSERT(layer < DDP_OVL_LAYER_COUNT, "layer_id(%d) overflow!", layer);

	//XLOG_DBG("prepare 0x%08x\n", (buf->layer_id<<24)|(buf->layer_en<<16)|(buf->ion_fd==MTK_FB_NO_ION_FD?0:buf->ion_fd));
	info = &layer_infos[layer];
	timeline = sync_timelines[layer];
	if (isAEEEnabled && (layer == DISP_DEFAULT_UI_LAYER_ID)) {
		XLOG_ERR("Maybe hang: AEE is enabled, HWC cannot use FB_LAYER again!\n");
		return SYNC_STATUS_ERROR;
	}
	if (layer != info->layer_id) {
		// Reset sync_fence info
		disp_sync_init_layer_info(layer);
	}
	// The first time create fence, need to initialize timeline and sync_info
	if (timeline == NULL) {
		timeline = disp_sync_create_timeline(layer);
	}
	// ION MVA
	if (ion_client == NULL) {
		disp_sync_ion_init();
	}
	// If no need Ion support, do nothing!
	if (buf->ion_fd == MTK_FB_NO_ION_FD) {
		//XLOG_DBG("NO NEED ion support\n");
	} else if (disp_sync_ion_alloc_buffer(ion_client, buf->ion_fd, &mva, &handle) != SYNC_STATUS_OK) {
		return SYNC_STATUS_ERROR;
	}
	// Fence Object
	data = disp_sync_create_fence(layer);
	buf->fence_fd = data.fence;
	buf->index = data.value;

	buf_info = disp_sync_get_buf_info();
	buf_info->fence = data.fence;
	buf_info->idx = data.value;
	buf_info->hnd = handle;
	buf_info->mva = mva;
	buf_info->cache_sync = buf->cache_sync;
	mutex_lock(&info->lock);
	list_add_tail(&buf_info->list, &info->buf_list);
	mutex_unlock(&info->lock);

	return SYNC_STATUS_OK;
}

void disp_sync_init(void) {
	sync_info *info;
    int i =0;
	for(i = 0; i < DDP_OVL_LAYER_COUNT; i++) {
	 	info = &layer_infos[i];
	 	mutex_init(&info->lock);
	 	info->layer_id = i;
	 	info->fence_idx = 0;
	 	info->timeline_idx = 0;
	 	info->inc = 0;
	 	info->cur_idx = 0;
	    INIT_LIST_HEAD(&info->buf_list);
	}
}

void disp_sync_deinit(void) {
	struct sw_sync_timeline *timeline;
	UINT layer = 0;
    for (layer = 0; layer < DDP_OVL_LAYER_COUNT; layer++) {
    	timeline = sync_timelines[layer];
    	if (timeline != NULL) {
    		XLOG_DBG("destroy timeline %s(%d)\n", timeline->obj.name, timeline->value);
    		timeline_destroy(timeline);
    		sync_timelines[layer] = NULL;
    	}
    	disp_sync_release_buffer(layer);
    	disp_sync_init_layer_info(layer);
    }
    disp_sync_free_buf_pool();
    disp_sync_ion_deinit();
}


///=============================================================================
// local function definitions
///==========================
static struct ion_client* disp_sync_ion_init (void) {
	if (IS_ERR_OR_NULL(ion_client) && (g_ion_device != NULL)) {
		ion_client = ion_client_create(g_ion_device, "mtkfb");
		if (IS_ERR_OR_NULL(ion_client)) {
			XLOG_ERR("create ion client failed!\n");
			return NULL;
		}
		XLOG_DBG("create ion client 0x%p\n", ion_client);
	}
	return ion_client;
}

static void disp_sync_ion_deinit (void) {
	if (IS_ERR_OR_NULL(ion_client)) {
		return;
	}
	ion_client_destroy(ion_client);
	ion_client = NULL;
	XLOG_DBG("destroy ion client 0x%p\n", ion_client);
}

/**
 * Import ion handle and configure this buffer
 * @client
 * @fd ion shared fd
 * @return ion handle
 */
static SYNC_STATUS disp_sync_ion_import_handle(struct ion_client *client, int fd, struct ion_handle **handle) {
	struct ion_handle *hnd;
	if (fd == DISP_INVALID_ION_FD) {
		XLOG_ERR("invalid ion fd!\n");
		return SYNC_STATUS_INVALID_PARAM;
	}
	hnd = ion_import_dma_buf(client, fd);
	if (IS_ERR_OR_NULL(hnd)) {
		XLOG_ERR("import ion handle failed!\n");
		return SYNC_STATUS_ERROR;
	}
	//XLOG_DBG("import ion handle fd=%d,hnd=0x%p\n", fd, hnd);

	*handle = hnd;
	return SYNC_STATUS_OK;
}

static void disp_sync_ion_free_handle(struct ion_client *client, struct ion_handle *handle) {
	if (IS_ERR_OR_NULL(client) || IS_ERR_OR_NULL(handle)) {
		//XLOG_ERR("invalid ion_client(%p),ion_hnd(%p)!\n", client, handle);
		return;
	}
	ion_free(client, handle);
	XLOG_DBG("free hnd=0x%p\n",  handle);
}

static void disp_sync_ion_config_buffer (struct ion_client *client, struct ion_handle *handle) {
	struct ion_mm_data mm_data;
	if (IS_ERR_OR_NULL(client) || IS_ERR_OR_NULL(handle)) {
		XLOG_ERR("invalid ion_client(%p),ion_hnd(%p)!\n", client, handle);
		return;
	}

	mm_data.mm_cmd = ION_MM_CONFIG_BUFFER;
	mm_data.config_buffer_param.handle = handle;
	mm_data.config_buffer_param.eModuleID = 0;
	mm_data.config_buffer_param.security = 0;
	mm_data.config_buffer_param.coherent = 0;

	if(ion_kernel_ioctl(client, ION_CMD_MULTIMEDIA, (int)&mm_data)) {
		XLOG_ERR("configure ion buffer failed hnd=0x%p!\n", handle);
	}
}

static size_t disp_sync_ion_phys_mmu_addr(struct ion_client *client, struct ion_handle *handle, UINT *mva) {
	size_t size;
	if (IS_ERR_OR_NULL(client) || IS_ERR_OR_NULL(handle)) {
		XLOG_ERR("invalid ion_client(%p),ion_hnd(%p)!\n", client, handle);
		return 0;
	}
	ion_phys(client, handle, (ion_phys_addr_t*)mva, &size);
	XLOG_DBG("alloc hnd=0x%p,mva=0x%08x\n",  handle, (UINT)*mva);
	return size;
}

/**
 * return the next index used to create fence
 */
static UINT disp_sync_get_fence_index (UINT layer) {
	sync_info *info;
	UINT index = 0;
	ASSERT(layer < DDP_OVL_LAYER_COUNT, "layer_id(%d) overflow!", layer);

	info = &layer_infos[layer];
	mutex_lock(&info->lock);
	index = info->fence_idx + FENCE_STEP_COUNTER;
	mutex_unlock(&info->lock);

	return index;
}

static UINT disp_sync_inc_fence_index (UINT layer) {
	sync_info *info;
	UINT index = 0;
	ASSERT(layer < DDP_OVL_LAYER_COUNT, "layer_id(%d) overflow!", layer);

	info = &layer_infos[layer];
	mutex_lock(&info->lock);
	info->fence_idx += FENCE_STEP_COUNTER;
	index = info->fence_idx;
	mutex_unlock(&info->lock);

	return index;
}

static sync_info* disp_sync_init_layer_info(UINT layer) {
	sync_info *info;
	ASSERT(layer < DDP_OVL_LAYER_COUNT, "layer_id(%d) overflow!", layer);

	info = &layer_infos[layer];
	mutex_init(&info->lock);
	info->layer_id = layer;
	info->fence_idx = 0;
	info->timeline_idx = 0;
	info->inc = 0;
	info->cur_idx = 0;
	INIT_LIST_HEAD(&info->buf_list);

	return info;
}

static buffer_info* disp_sync_init_buf_info(buffer_info *buf) {
	INIT_LIST_HEAD(&buf->list);
	buf->fence = DISP_INVALID_FENCE_FD;
	buf->hnd = NULL;
	buf->idx = 0;
	buf->mva = 0;
	buf->cache_sync = 0;
	return buf;
}

/**
 * Query a @buf_info node from @info_pool_head, if empty create a new one
 */
static buffer_info* disp_sync_get_buf_info (void) {
	buffer_info *buf;
	mutex_lock(&fence_buffer_mutex);
	if (!list_empty(&info_pool_head)) {
		buf = list_first_entry(&info_pool_head, buffer_info, list);
		list_del_init(&buf->list);
		disp_sync_init_buf_info(buf);
	} else {
		buf = kzalloc(sizeof(buffer_info), GFP_KERNEL);
		disp_sync_init_buf_info(buf);
		XLOG_DBG("create new buf_info node 0x%p\n", buf);
	}
	mutex_unlock(&fence_buffer_mutex);

	return buf;
}

static void disp_sync_free_buf_pool (void) {
	buffer_info *pos, *n;
	mutex_lock(&fence_buffer_mutex);
	list_for_each_entry_safe(pos, n, &info_pool_head, list) {
		kzfree(pos);
	}
	mutex_unlock(&fence_buffer_mutex);
}

static struct sw_sync_timeline* disp_sync_create_timeline (UINT layer) {
	struct sw_sync_timeline *timeline;
	char name[32];
	const char *prefix = "ovl_timeline";
	sprintf(name, "%s-%d", prefix, layer);

	ASSERT(layer < DDP_OVL_LAYER_COUNT, "layer_id(%d) overflow!", layer);
	MMProfileLogEx(MTKFB_MMP_Events.CreateSyncTimeline, MMProfileFlagPulse, layer, 0);

	timeline = timeline_create(name);
	sync_timelines[layer] = timeline;
	if (timeline == NULL) {
		XLOG_ERR("L%d cannot create timeline! \n");
		return timeline;
	}
	XLOG_DBG("L%d create timeline: %s\n", layer, name);
	return timeline;
}

static struct fence_data disp_sync_create_fence(UINT layer) {
	sync_info *info;
	struct sw_sync_timeline *timeline;
	int fenceFd = DISP_INVALID_FENCE_FD;
	struct fence_data data;
	const char *prefix = "ovl_fence";
	ASSERT(layer < DDP_OVL_LAYER_COUNT, "layer_id(%d) overflow!", layer);
	// Init invalid fd
	data.fence = fenceFd;
	data.value = disp_sync_get_fence_index(layer);
	sprintf(data.name, "%s-%d-%d", prefix, layer, data.value);

	info = &layer_infos[layer];
	timeline = sync_timelines[layer];

	// The first time create fence, need to initialize timeline and sync_info
	if (timeline == NULL) {
		sync_timelines[layer] = timeline = disp_sync_create_timeline(layer);
		disp_sync_init_layer_info(layer);
	}

	MMProfileLogEx(MTKFB_MMP_Events.CreateSyncFence, MMProfileFlagStart, layer, 0);

	if (timeline != NULL) {
		if (fence_create(timeline, &data)) {
			XLOG_ERR("L%d create Fence Object failed!\n", layer);
		} else {
			disp_sync_inc_fence_index(layer);
			fenceFd = data.fence;
		}
		XLOG_DBG("L%d fence %s(%d)\n", layer, data.name, fenceFd);
	} else {
		XLOG_ERR("L%d has no Timeline!\n", layer);
	}

	MMProfileLogEx(MTKFB_MMP_Events.CreateSyncFence, MMProfileFlagEnd, data.value, fenceFd);

	return data;
}




