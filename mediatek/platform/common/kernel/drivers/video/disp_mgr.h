#ifndef __DISP_MGR_H
#define __DISP_MGR_H

#define MAX_INPUT_CONFIG 		4
#define MAX_QUEUE_BUFFER_COUNT 	3
#define UNKNOWN_GROUP_ID		0xff

typedef unsigned int UINT;
typedef unsigned char BOOL;

#define MAKE_DISP_SESSION(mode, type, dev) (UINT)((mode) << 24 | (type)<<16 | (dev))
#define DISP_SESSION_MODE(id) (((id)>>24)&0xff)
#define DISP_SESSION_TYPE(id) (((id)>>16)&0xff)
#define DISP_SESSION_DEV(id) ((id)&0xff)

///=============================================================================
// forward declarations of external structures
// NOTICE: this is the INPUT PARAMETERS directly from its CLIENT
///===========================
struct disp_session_config_t;
struct disp_session_input_config_t;
struct disp_session_output_config_t;

///=============================================================================
// structure declarations
///===========================
typedef enum {
   DCP_STATUS_OK = 0,

   DCP_STATUS_NOT_IMPLEMENTED,
   DCP_STATUS_ALREADY_EXIST,
   DCP_STATUS_DONT_EXIST,
   DCP_STATUS_INVALID_PARAM,
   DCP_STATUS_ERROR,
} DCP_STATUS;

typedef enum {
	FREE,
	ACTIVE,
	QUEUED,
	ACQUIRED,
	DONE,
}JOB_STATUS;

typedef struct disp_buffer_queue_t {
	spinlock_t lock;
	UINT write_slot;
	UINT read_slot;
	UINT reserved;
	UINT buffer_count;
	UINT buffer_queue[MAX_QUEUE_BUFFER_COUNT];
}disp_buffer_queue;

typedef struct input_config_t {
	UINT layer_id;
	UINT layer_enable;
	UINT index;
	UINT format;
	UINT src_x;
	UINT src_y;
	UINT dst_x;
	UINT dst_y;
	UINT width;
	UINT height;
	UINT pitch;
	UINT address;
	UINT alpha_enable;
	UINT alpha;
	UINT security;
	UINT color_key_enable;
	UINT color_key;
	UINT dirty;
}input_config;

typedef struct output_config_t {
	UINT layer_id;
	UINT index;
	UINT format;
	UINT x;
	UINT y;
	UINT width;
	UINT height;
	UINT pitch;
	UINT pitchUV;
	UINT address;
	UINT security;
	UINT dirty;
}output_config;

typedef struct disp_job_t {
	struct list_head list;
	struct mutex lock;
	JOB_STATUS status;
	// belongs to which session
	UINT group_id;
	input_config  input[MAX_INPUT_CONFIG];
	output_config output;
}disp_job;

///=============================================================================
// function declarations
///===========================

///-----------------------------------------------------------------------------
///implementation has dependency of Interface file(Ex. mtkfb.h) @{
/**
 * create a new display session if it does not exist
 */
DCP_STATUS disp_create_session (struct disp_session_config_t *config);
/**
 * destroy a display session if it does exist
 */
DCP_STATUS disp_destroy_session (struct disp_session_config_t *config);
/**
 * set OVL input buffer for a specified display session
 */
DCP_STATUS disp_set_session_input (struct disp_session_input_config_t *input);
/**
 * set OVL-WDMA output buffer for a specified display session
 */
DCP_STATUS disp_set_session_output (struct disp_session_output_config_t *input);
///@}

/**
 * Dequeue an active job for given session, return existing or create a new one
 * Each Session only has one ACTIVE JOB
 * @gid: specify which session
 *
 * Note, return the existing Active Job or create a new one
 */
disp_job* disp_deque_job (UINT gid);

/**
 * Configuring is completed, add this job to JOB QUEUE
 * @gid: specify which session
 *
 * Note, return DCP_STATUS_OK if success, or DCP_STATUS_DONT_EXIST
 */
DCP_STATUS disp_enque_job(UINT gid);


/**
 * Get a jobs from Job Queue list to be processed.
 *
 * Note, return Job Node or NULL if has no jobs in Job Queue
 */
disp_job* disp_acquire_job (void);

/**
 * Move all frame done job from job queue into done list.
 *
 * Note, return how many jobs removed from job queue
 */
UINT disp_release_job (void);

/**
 * Query a frame done job from done list
 * @gid: 	 specify which session
 * @reverse: if true get the latest, or get the oldest
 *
 * Note, you could query the same job before it's recycled
 */
disp_job* disp_query_job (void);

/**
 * Processing is completed, recycle this job
 *
 * Note, return how many jobs recycled
 */
disp_job* disp_recycle_job(BOOL all);

/**
 * Discard the active job for given session
 * Each Session only has one ACTIVE JOB
 * @gid: specify which session
 *
 * Note, return DCP_STATUS_OK if success, or DCP_STATUS_DONT_EXIST
 */
DCP_STATUS disp_cancel_job(UINT gid);

/**
 * Initialize buffer queue for a given block buffers.
 * @que:	 uninitialized
 * @address: start address of this block buffer
 * @size:	 total size of this block buffer
 * @cnt:	 buffer queue size
 *
 * Note, return DCP_STATUS_OK or DCP_STATUS_NOT_IMPLEMENTED
 * MUST lock before modify it, because more than one can get this job
 * at the same time
 */
DCP_STATUS disp_buffer_queue_init (disp_buffer_queue *que, UINT address[], UINT cnt);

/**
 * Initialize buffer queue for a given continuous block buffer.
 * @que: 	 uninitialized
 * @address: start address array of block buffers
 * @size:	 total size of this block buffer
 * @cnt:	 buffer queue size
 *
 * Note, return DCP_STATUS_OK or DCP_STATUS_NOT_IMPLEMENTED
 * MUST lock before modify it, because more than one can get this job
 * at the same time
 */
DCP_STATUS disp_buffer_queue_init_continous (disp_buffer_queue *que, UINT address, UINT size, UINT cnt);

/**
 * Dequeue buffer from buffer queue to use (mem write)
 */
UINT disp_deque_buffer (disp_buffer_queue *que);

/**
 * Queue buffer to buffer queue
 */
void disp_enque_buffer (disp_buffer_queue *que);

/**
 * Acquire buffer from buffer queue (mem read)
 */
BOOL disp_acquire_buffer (disp_buffer_queue *que);

/**
 * Recycle buffer into buffer queue
 */
void disp_release_buffer (disp_buffer_queue *que);

/**
 * Request for get buffer from buffer queue
 * The next buffer to be updated will be returned
 */
UINT disp_request_buffer (disp_buffer_queue *que);



#endif //__DISP_MGR_H
