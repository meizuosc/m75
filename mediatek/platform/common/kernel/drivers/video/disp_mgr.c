/**
 * Multiple Display support in Display Driver
 */
#include <linux/slab.h>
#include <linux/list.h>

#define LOG_TAG "MGR"	// multiple display

#include "DpDataType.h"
#include "disp_mgr.h"
#include "disp_sync_svp.h"
// Client interface definition
#include "disp_svp.h"
#include "debug.h"
///=============================================================================
// external variables declarations
///==========================

static BOOL log_on = 0;
void disp_mgr_log_on(BOOL on) {
	log_on = on;
}

///=============================================================================
// structure declarations
///===========================
#define MAX_SESSION_COUNT		5
static UINT session_config[MAX_SESSION_COUNT];
static DEFINE_MUTEX(disp_session_lock);

///=============================================================================
// local variables
///==========================
static LIST_HEAD(todo_queue_head);			// ONE or MORE job for each session
static DEFINE_MUTEX(todo_queue_lock);
static LIST_HEAD(active_queue_head);		// ONLY ONE job for each session
static DEFINE_MUTEX(active_queue_lock);
static LIST_HEAD(done_queue_head);			// ONLY ONE job for each session
static DEFINE_MUTEX(done_queue_lock);
static LIST_HEAD(job_pool_head);
static DEFINE_MUTEX(job_pool_lock);

///=============================================================================
// local function forward declarations
///=============================
static disp_job* disp_create_job (void);
static void disp_init_job(disp_job *job);

///=============================================================================
// global function definitions
///=============================

//------------------------------------------------------------------------------
// Buffer queue management
//-------------------------
DCP_STATUS disp_create_session (struct disp_session_config_t *config) {
	DCP_STATUS ret = DCP_STATUS_OK;
	UINT session = MAKE_DISP_SESSION(config->mode, config->type, config->device_id);
	int i, idx = -1;
	//1.To check if this session exists already
	mutex_lock(&disp_session_lock);
	for (i = 0; i < MAX_SESSION_COUNT; i++) {
		if (session_config[i] == 0 && idx == -1) {
			idx = i;
		}
		if (session_config[i] == session) {
			config->session_id = session;
			ret = DCP_STATUS_ALREADY_EXIST;
			XLOG_WARN("session(0x%x) already exists\n", session);
			break;
		}
	}
	//1.To check if support this session (mode,type,dev)
	//TODO:

	//2. Create this session
	if (ret != DCP_STATUS_ALREADY_EXIST) {
		config->session_id = session;
		session_config[idx] = session;
		disp_sync_init(session);
		XLOG_DBG("New session(0x%x)\n", session);
	}
	mutex_unlock(&disp_session_lock);

	return ret;
}

DCP_STATUS disp_destroy_session (struct disp_session_config_t *config) {
	DCP_STATUS ret = DCP_STATUS_DONT_EXIST;
	UINT session = config->session_id;
	int i;
	//1.To check if this session exists already, and remove it
	mutex_lock(&disp_session_lock);
	for (i = 0; i < MAX_SESSION_COUNT; i++) {
		if (session_config[i] == session) {
			session_config[i] = 0;
			disp_sync_deinit(session);
			ret = DCP_STATUS_OK;
			break;
		}
	}
	mutex_unlock(&disp_session_lock);
	//2. Destroy this session
	if (ret == DCP_STATUS_OK) {
		XLOG_DBG("Destroy session(0x%x)\n", session);
	} else {
		XLOG_WARN("session(0x%x) does not exists\n", session);
	}

	return ret;
}


DCP_STATUS disp_set_session_input (struct disp_session_input_config_t *input) {
	DCP_STATUS ret = DCP_STATUS_DONT_EXIST;
	int i;
	int layerpitch = 2;
	int layerbpp = 16;
	int unknown_fmt = 0;
	disp_input_config *config;
	disp_job *job = NULL;
	//1. check if this session exits
	mutex_lock(&disp_session_lock);
	for (i = 0; i < MAX_SESSION_COUNT; i++) {
		if (input->session_id == session_config[i]) {
			ret = DCP_STATUS_OK;
			break;
		}
	}
	mutex_unlock(&disp_session_lock);
	if (ret != DCP_STATUS_OK) {
		return ret;
	}
	//2. Reset active job of this session
	job = disp_deque_job(input->session_id);
	mutex_lock(&job->lock);
	for (i = 0; i < MAX_INPUT_CONFIG; i++) {
		config = &input->config[i];
		job->input[i].layer_id = config->layer_id;
		if (config->layer_id >= MAX_INPUT_CONFIG) {
			//layer_dirty is false, will be ignored
			continue;
		}
		if (config->layer_enable) {
			if (config->next_buff_idx <= disp_sync_get_last_signaled(
					input->session_id, config->layer_id)) {
				MMProfileLogEx(MTKFB_MMP_Events.Debug, MMProfileFlagPulse, config->layer_id, config->next_buff_idx);
				AEE_WARNING("[DISP/Mgr]", "L%d of 0x%08x is enabled, but HWC set an old buffer(%d)!\n",
						config->layer_id, input->session_id, config->next_buff_idx);
				job->input[i].dirty = 0;
				continue;
			}
			switch (config->src_fmt)
			{
			case DISP_FORMAT_RGB565:
				job->input[i].format = eRGB565;
				layerpitch = 2;
				layerbpp = 16;
				break;

			case DISP_FORMAT_RGB888:
				job->input[i].format = eRGB888;
				layerpitch = 3;
				layerbpp = 24;
				break;
			case DISP_FORMAT_BGR888:
				job->input[i].format = eBGR888;
				layerpitch = 3;
				layerbpp = 24;
				break;

			case DISP_FORMAT_ARGB8888:
				job->input[i].format = ePARGB8888;
				layerpitch = 4;
				layerbpp = 32;
				break;
			case DISP_FORMAT_ABGR8888:
				job->input[i].format = ePABGR8888;
				layerpitch = 4;
				layerbpp = 32;
				break;
			case DISP_FORMAT_XRGB8888:
				job->input[i].format = eARGB8888;
				layerpitch = 4;
				layerbpp = 32;
				break;
			case DISP_FORMAT_XBGR8888:
				job->input[i].format = eABGR8888;
				layerpitch = 4;
				layerbpp = 32;
				break;

			case DISP_FORMAT_YUV444:
				job->input[i].format = eYUV_444_1P;
				layerpitch = 3;
				layerbpp = 24;
				break;
			case DISP_FORMAT_YVYU:
				job->input[i].format = eYVYU;
				layerpitch = 2;
				layerbpp = 16;
				break;
			case DISP_FORMAT_VYUY:
				job->input[i].format = eVYUY;
				layerpitch = 2;
				layerbpp = 16;
				break;
			case DISP_FORMAT_UYVY:
			//case DISP_FORMAT_Y422:
				job->input[i].format = eUYVY;
				layerpitch = 2;
				layerbpp = 16;
				break;
			case DISP_FORMAT_YUYV:
			//case DISP_FORMAT_YUY2:
			//case DISP_FORMAT_YUV422:
				job->input[i].format = eYUYV;
				layerpitch = 2;
				layerbpp = 16;
				break;

			default:
				MMProfileLogEx(MTKFB_MMP_Events.Debug, MMProfileFlagPulse, config->layer_id, config->src_fmt);
				AEE_WARNING("[DISP/Mgr]", "L%d of 0x%08x unknown format(0x%x)!\n",
						config->layer_id, input->session_id, config->src_fmt);
				unknown_fmt = 1;
				break;
			}
			if (unknown_fmt) {
				unknown_fmt = 0;
				job->input[i].dirty = 0;
				continue;
			}
		    job->input[i].security = config->security;
		    if (config->src_phy_addr != NULL) {
		    	job->input[i].address = (unsigned int)config->src_phy_addr;
		    } else {
		    	job->input[i].address = disp_sync_query_buffer_mva(job->group_id, config->layer_id, (unsigned int)config->next_buff_idx);
		    }
		    job->input[i].index = config->next_buff_idx;

		    /**
		     * NOT USED now
		        job->input[i].tdshp = layerInfo->isTdshp;
		    	job->input[i].identity = layerInfo->identity;
		    	job->input[i].connected_type = layerInfo->connected_type;
		    	job->input[i].sharp = layerInfo->isTdshp;
		     */
		    //set Alpha blending
		    if (config->alpha_enable) {
		    	job->input[i].alpha_enable = 1;
		    	job->input[i].alpha = config->alpha;
		    } else if (DISP_FORMAT_ARGB8888 == config->src_fmt || DISP_FORMAT_ABGR8888 == config->src_fmt) {
		    	job->input[i].alpha_enable = 1;
		    	job->input[i].alpha = 0xff;
		    } else {
		    	job->input[i].alpha_enable = 0;
		    }
		    //set src width, src height
			job->input[i].src_x = config->src_offset_x;
			job->input[i].src_y = config->src_offset_y;
			job->input[i].dst_x = config->tgt_offset_x;
			job->input[i].dst_y = config->tgt_offset_y;
			if (config->src_width != config->tgt_width || config->src_height != config->tgt_height) {
				XLOG_ERR("OVL cannot support clip:src(%d,%d), dst(%d,%d)\n", config->src_width, config->src_height, config->tgt_width, config->tgt_height);
			}
			job->input[i].width = config->tgt_width;
			job->input[i].height = config->tgt_height;

			job->input[i].pitch = config->src_pitch*layerpitch;

		    //set color key
		    job->input[i].color_key = config->src_color_key;
		    job->input[i].color_key_enable = config->src_use_color_key;

		    //data transferring is triggerred in MTKFB_TRIG_OVERLAY_OUT
		    job->input[i].layer_enable = config->layer_enable;
		    job->input[i].dirty = 1;

		} else {
			job->input[i].index = DISP_INVALID_FENCE_INDEX;
			job->input[i].dirty = 1;
		}
		XLOG_DBG("L%d input:%d,%d\n", job->input[i].layer_id, job->input[i].layer_enable, job->input[i].index);
	}
	mutex_unlock(&job->lock);

	return ret;
}

DCP_STATUS disp_set_session_output (struct disp_session_output_config_t *output) {
	DCP_STATUS ret = DCP_STATUS_DONT_EXIST;
	disp_job *job = NULL;
	disp_output_config* config;
	int i, bpp = 2, yuv = 0;
	//1. check if this session exits
	mutex_lock(&disp_session_lock);
	for (i = 0; i < MAX_SESSION_COUNT; i++) {
		if (output->session_id == session_config[i]) {
			ret = DCP_STATUS_OK;
			break;
		}
	}
	mutex_unlock(&disp_session_lock);
	if (ret != DCP_STATUS_OK) {
		XLOG_ERR("NO such session 0x%x\n", output->session_id);
		return ret;
	}
	//2. Reset active job of this session
	job = disp_deque_job(output->session_id);
	mutex_lock(&job->lock);

	config = &output->config;
	switch (config->fmt)
	{
	case DISP_FORMAT_RGB565:
		job->output.format = eRGB565;
		bpp = 2;
		yuv = 0;
		break;
	case DISP_FORMAT_RGB888:
		job->output.format = eRGB888;
		bpp = 3;
		yuv = 0;
		break;
	case DISP_FORMAT_BGR888:
		job->output.format = eBGR888;
		bpp = 3;
		yuv = 0;
		break;
	case DISP_FORMAT_ARGB8888:
		job->output.format = eABGR8888;
		bpp = 4;
		yuv = 0;
		break;
	case DISP_FORMAT_ABGR8888:
		job->output.format = eABGR8888;
		bpp = 4;
		yuv = 0;
		break;
	case DISP_FORMAT_XRGB8888:
		job->output.format = eXARGB8888;
		bpp = 4;
		yuv = 0;
		break;
	case DISP_FORMAT_XBGR8888:
		job->output.format = eABGR8888;
		bpp = 4;
		yuv = 0;
		break;
	case DISP_FORMAT_UYVY:
	//case DISP_FORMAT_Y422:
		job->output.format = eUYVY;
		bpp = 2;
		yuv = 1;
		break;
	case DISP_FORMAT_YUYV:
	//case DISP_FORMAT_YUY2:
	//case DISP_FORMAT_YUV422:
		job->output.format = eYUYV;
		bpp = 2;
		yuv = 1;
		break;
	case DISP_FORMAT_GREY:
	//case DISP_FORMAT_Y800:
	//case DISP_FORMAT_Y8:
		job->output.format = eGREY;
		bpp = 1;
		yuv = 1;
		break;
	case DISP_FORMAT_YV12:
		job->output.format = eYV12;
		bpp = 1;
		yuv = 1;
		break;
	case DISP_FORMAT_I420:
	//case DISP_FORMAT_IYUV:
		job->output.format = eI420;
		bpp = 1;
		yuv = 1;
		break;
	case DISP_FORMAT_NV12:
		job->output.format = eNV12;
		bpp = 1;
		yuv = 1;
		break;
	case DISP_FORMAT_NV21:
		job->output.format = eNV21;
		bpp = 1;
		yuv = 1;
		break;

	default:
		job->output.format = eRGB888;
		XLOG_ERR("Invalid color format: 0x%x\n", config->fmt);
		MMProfileLogEx(MTKFB_MMP_Events.Debug, MMProfileFlagPulse, output->session_id, config->fmt);
		AEE_WARNING("[DISP/Mgr]", "0x%08x unknown format(0x%x)!\n",
				output->session_id, config->fmt);
		break;
	}
	if (config->pa != 0) {
		job->output.address = config->pa;
	} else {
		job->output.address = disp_sync_query_buffer_mva(job->group_id, MAX_INPUT_CONFIG, config->buff_idx);
	}
	job->output.layer_id = MAX_INPUT_CONFIG;
	job->output.x = config->x;
	job->output.y = config->y;
	job->output.width = config->width;
	job->output.height = config->height;
	job->output.pitch = config->pitch * bpp;
	job->output.pitchUV = config->pitchUV * bpp * yuv;
	job->output.security = config->security;
	job->output.dirty = 1;
	job->output.index = config->buff_idx;

	mutex_unlock(&job->lock);
	XLOG_DBG("L%d output:%d,0x%x\n", config->buff_idx, config->pa);
	return ret;
}

//------------------------------------------------------------------------------
// job managements
// Job's lifetime:
// 1. [FREE] Add a active job for the new session
// 2. [ACTIVE] Push configuration into the active job for given session
// 3. [QUEUED] Queue this active job into JOB Queue
// 4. [ACQUIRED] Handling jobs in job queue one by one until empty
// 5. [FREE] Recycle finished job
// 6. Step for 1~5
//
// Job's status:
// 1. FREE, be created if needed, as a freed job
// 2. ACTIVE, set as the current active job for a certain Session
//    all information of this Session will be fill into this active job
// 3. QUEUED, Add to Job Queue, waiting for processed by hw
// 4. ACQUIRED, Removed from Job Queue, be processing now
//----------------------------------------------------------

disp_job* disp_deque_job(UINT gid) {
	disp_job *pos, *job = NULL;
	mutex_lock(&active_queue_lock);
	//1. Reture the current Active Job if it exists already
	if (!list_empty(&active_queue_head)) {
		list_for_each_entry(pos, &active_queue_head, list) {
			if (pos->group_id == gid) {
				job = pos;
				break;
			}
		}
	}
	//2. Create a new Active Job for this @gid
	if (job == NULL) {
		job = disp_create_job();
		job->status = ACTIVE;
		job->group_id = gid;
		list_add_tail(&job->list, &active_queue_head);
	}
	mutex_unlock(&active_queue_lock);
	XLOG_DBG("S%x deque: 0x%p\n", gid, job);
	MMProfileLogEx(MTKFB_MMP_Events.deque, MMProfileFlagPulse, gid, (UINT)job);

	return job;
}

//FIXME: if deque job thread is not same as enque job thread, then we need to
// sync to make only one active job exists!!
DCP_STATUS disp_enque_job (UINT gid) {
	DCP_STATUS ret = DCP_STATUS_DONT_EXIST;
	disp_job *pos, *n;
	mutex_lock(&active_queue_lock);
	if (!list_empty(&active_queue_head)) {
		list_for_each_entry_safe(pos, n, &active_queue_head, list) {
			if (gid == pos->group_id) {
				//1. remove from active job list
				mutex_lock(&pos->lock);
				list_del_init(&pos->list);
				pos->status = QUEUED;
				mutex_unlock(&pos->lock);

				//2. add to job queue list
				mutex_lock(&todo_queue_lock);
				list_add_tail(&pos->list, &todo_queue_head);
				mutex_unlock(&todo_queue_lock);
				ret = DCP_STATUS_OK;

				XLOG_DBG("S%x enque: 0x%p\n", gid, pos);
				MMProfileLogEx(MTKFB_MMP_Events.enque, MMProfileFlagPulse, gid, (UINT)pos);
				break;
			}
		}
	}
	mutex_unlock(&active_queue_lock);
	return ret;
}

disp_job* disp_acquire_job (void) {
	disp_job *pos, *job = NULL;
	mutex_lock(&todo_queue_lock);
	if (!list_empty(&todo_queue_head)) {
		list_for_each_entry(pos, &todo_queue_head, list) {
			if (pos->status == QUEUED) {
				job = pos;
				mutex_lock(&job->lock);
				job->status = ACQUIRED;
				mutex_unlock(&job->lock);
				MMProfileLogEx(MTKFB_MMP_Events.acquire, MMProfileFlagPulse, job->group_id, (UINT)job);
				break;
			}
		}
	}
	mutex_unlock(&todo_queue_lock);
	return job;
}

UINT disp_release_job (void) {
	UINT cnt = 0;
	disp_job *pos, *n;
	mutex_lock(&todo_queue_lock);
	if (!list_empty(&todo_queue_head)) {
		list_for_each_entry_safe(pos, n, &todo_queue_head, list) {
			if (pos->status == ACQUIRED) {
				cnt++;
				//1. remove from todo job list
				mutex_lock(&pos->lock);
				list_del_init(&pos->list);
				pos->status = DONE;
				mutex_unlock(&pos->lock);
				//2. add to done job list
				mutex_lock(&done_queue_lock);
				list_add_tail(&pos->list, &done_queue_head);
				mutex_unlock(&done_queue_lock);
				XLOG_DBG("release:0x%p,%d\n", pos, cnt);
				MMProfileLogEx(MTKFB_MMP_Events.release, MMProfileFlagPulse, cnt, (UINT)pos);
			}
		}
	}
	mutex_unlock(&todo_queue_lock);

	return cnt;
}

disp_job* disp_query_job (void) {
	disp_job *pos, *job = NULL;
	mutex_lock(&done_queue_lock);
	if (!list_empty(&done_queue_head)) {
		list_for_each_entry(pos, &done_queue_head, list) {
			if (pos->status == DONE) {
				//1. remove from processing job list
				mutex_lock(&pos->lock);
				pos->status = FREE;
				mutex_unlock(&pos->lock);
				job = pos;
				XLOG_DBG("query:0x%p\n", pos);
				MMProfileLogEx(MTKFB_MMP_Events.query, MMProfileFlagPulse, job->group_id, (UINT)job);
				break;
			}
		}
	}
	mutex_unlock(&done_queue_lock);
	return job;
}

disp_job* disp_recycle_job (BOOL all) {
	disp_job *pos, *n, *job = NULL;
	mutex_lock(&done_queue_lock);
	if (!list_empty(&done_queue_head)) {
		list_for_each_entry_safe(pos, n, &done_queue_head, list) {
			if (pos->status == FREE) {
				//1. remove from processing job list
				list_del_init(&pos->list);
				job = pos;
				//2. add to job pool list
				mutex_lock(&job_pool_lock);
				list_add_tail(&pos->list, &job_pool_head);
				mutex_unlock(&job_pool_lock);
				XLOG_DBG("recycle:0x%p\n", pos);
				MMProfileLogEx(MTKFB_MMP_Events.recycle, MMProfileFlagPulse, pos->group_id, (UINT)pos);
				if (!all) {
					break;
				}
			}
		}
	}
	mutex_unlock(&done_queue_lock);
	return job;
}

DCP_STATUS disp_cancel_job(UINT gid) {
	DCP_STATUS ret = DCP_STATUS_DONT_EXIST;
	disp_job *pos, *n;
	mutex_lock(&active_queue_lock);
	if (!list_empty(&active_queue_head)) {
		list_for_each_entry_safe(pos, n, &active_queue_head, list) {
			if (pos->group_id == gid) {
				//1. remove from active job list
				list_del_init(&pos->list);
				//2. re-initialize this job before use it
				disp_init_job(pos);
				//3. add to job pool list
				mutex_lock(&job_pool_lock);
				list_add_tail(&pos->list, &job_pool_head);
				mutex_unlock(&job_pool_lock);
				XLOG_DBG("cancel:%d,0x%p\n", gid, pos);
				ret = DCP_STATUS_OK;
			}
		}
	}
	mutex_unlock(&active_queue_lock);
	return ret;
}

//------------------------------------------------------------------------------
// Buffer queue management
//----------------------------
DCP_STATUS disp_buffer_queue_init (disp_buffer_queue *que, UINT address[], UINT cnt) {
	DCP_STATUS ret = DCP_STATUS_NOT_IMPLEMENTED;
	UINT index;
	if (cnt <= MAX_QUEUE_BUFFER_COUNT) {
		spin_lock_init(&que->lock);
		que->buffer_count = cnt;
		que->read_slot = 0;
		que->write_slot = 1;
		for (index = 0; index < cnt; index++) {
			que->buffer_queue[index] = address[index];
		}
		ret = DCP_STATUS_OK;
	}

	return ret;
}

DCP_STATUS disp_buffer_queue_init_continous (disp_buffer_queue *que, UINT address, UINT size, UINT cnt) {
	DCP_STATUS ret = DCP_STATUS_NOT_IMPLEMENTED;
	UINT index, offset;
	if (cnt <= MAX_QUEUE_BUFFER_COUNT) {
		spin_lock_init(&que->lock);
		que->buffer_count = cnt;
		que->read_slot = 0;
		que->write_slot = 1;
		offset = size / cnt;
		for (index = 0; index < cnt; index++) {
			que->buffer_queue[index] = address + index * offset;
		}
		ret = DCP_STATUS_OK;
	}

	return ret;
}

UINT disp_deque_buffer (disp_buffer_queue *que) {
	UINT slot = 0;
	unsigned long flag;
	spin_lock_irqsave(&que->lock, flag);
	slot = que->write_slot;
	spin_unlock_irqrestore(&que->lock, flag);
	MMProfileLogEx(MTKFB_MMP_Events.deque_buf, MMProfileFlagPulse, que->read_slot, que->write_slot);
	return que->buffer_queue[slot];
}

void disp_enque_buffer (disp_buffer_queue *que) {
	UINT slot = 0;
	unsigned long flag;
	spin_lock_irqsave(&que->lock, flag);
	que->reserved++;
	if (que->reserved == 1) {
		que->read_slot = que->write_slot;
	}
	slot = (que->write_slot + 1) % que->buffer_count;
	if (slot != que->read_slot) {
		que->write_slot = slot;
	}
	spin_unlock_irqrestore(&que->lock, flag);
	MMProfileLogEx(MTKFB_MMP_Events.enque_buf, MMProfileFlagPulse, que->read_slot, que->write_slot);
}

UINT disp_request_buffer (disp_buffer_queue *que) {
	UINT slot;
	unsigned long flag;
	spin_lock_irqsave(&que->lock, flag);
	slot = que->read_slot;
	spin_unlock_irqrestore(&que->lock, flag);
	MMProfileLogEx(MTKFB_MMP_Events.request_buf, MMProfileFlagPulse, que->read_slot, que->write_slot);
	return que->buffer_queue[slot];
}

void disp_release_buffer (disp_buffer_queue *que) {
	UINT slot;
	unsigned long flag;
	spin_lock_irqsave(&que->lock, flag);
	slot = (que->read_slot + 1) % que->buffer_count;
	if (slot != que->write_slot) {
		que->read_slot = slot;
	}
	spin_unlock_irqrestore(&que->lock, flag);
	MMProfileLogEx(MTKFB_MMP_Events.release_buf, MMProfileFlagPulse, que->read_slot, que->write_slot);
}

BOOL disp_acquire_buffer (disp_buffer_queue *que) {
	BOOL ret = 0;
	unsigned long flag;
	spin_lock_irqsave(&que->lock, flag);
	if (que->reserved > 0) {
		que->reserved--;
		ret = 1;
	}
	spin_unlock_irqrestore(&que->lock, flag);
	MMProfileLogEx(MTKFB_MMP_Events.acquire_buf, MMProfileFlagPulse, ret, que->reserved);
	return ret;
}


///=============================================================================
// local function definitions
///==========================
static void disp_init_job(disp_job *job) {
	mutex_init(&job->lock);
	INIT_LIST_HEAD(&job->list);
	job->status = FREE;

	job->group_id = UNKNOWN_GROUP_ID;
	memset(&job->input, 0, sizeof(job->input));
	memset(&job->output, 0, sizeof(job->output));
}

/**
 * Get a freed job from Job Pool, if empty create a new one
 * @return job is initialized
 */
static disp_job* disp_create_job (void) {
	disp_job *job = NULL;
	mutex_lock(&job_pool_lock);
	if (!list_empty(&job_pool_head)) {
		job = list_first_entry(&job_pool_head, disp_job, list);
		list_del_init(&job->list);
	}
	mutex_unlock(&job_pool_lock);
	if (job == NULL) {
		job = kzalloc(sizeof(disp_job), GFP_KERNEL);
		XLOG_DBG("create new job node 0x%p\n", job);
	}
	disp_init_job(job);
	return job;
}



