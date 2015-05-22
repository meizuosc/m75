#ifndef __DISP_SYNC_H
#define __DISP_SYNC_H

#include <linux/xlog.h>
#include <linux/aee.h>

#define DISP_INVALID_FENCE_FD    	-1
#define DISP_INVALID_ION_FD	   		-1

typedef unsigned int UINT;
typedef unsigned char BOOL;
//#define NULL 					  0x0


#ifndef ASSERT
#define ASSERT(expr, fmt, arg...)	\
	do {							\
		if (!(expr)) aee_kernel_warning("[DISP DRV] ASSERT FAILED(%s:%d):"fmt, __func__, __LINE__, ##arg); \
	} while (0)
#endif

#define XLOG_INFO(fmt, arg...) 												   \
	do {                                                    				   \
		if(log_on)xlog_printk(ANDROID_LOG_INFO, "DISP/"LOG_TAG, fmt, ##arg);   \
	}while(0)
#define XLOG_DBG(fmt, arg...)  												   \
	do {                                                    				   \
		if(log_on)xlog_printk(ANDROID_LOG_DEBUG, "DISP/"LOG_TAG, fmt, ##arg);  \
	}while(0)
#define XLOG_WARN(fmt, arg...) 												   \
    do { 																	   \
    	xlog_printk(ANDROID_LOG_WARN, "DISP/"LOG_TAG, fmt, ##arg);  		   \
    }while (0)
#define XLOG_ERR(fmt, arg...) 												   \
    do { 																	   \
    	xlog_printk(ANDROID_LOG_WARN, "DISP/"LOG_TAG, fmt, ##arg);  		   \
    }while (0)

///=============================================================================
// forward declare external structures
///===========================
struct ion_client;
struct ion_handle;
struct fb_overlay_buffer;


///=============================================================================
// structure declarations
///===========================
typedef enum {
   SYNC_STATUS_OK = 0,

   SYNC_STATUS_NOT_IMPLEMENTED,
   SYNC_STATUS_ALREADY_SET,
   SYNC_STATUS_INVALID_PARAM,
   SYNC_STATUS_ERROR,
} SYNC_STATUS;


///=============================================================================
// function declarations
///===========================
SYNC_STATUS disp_sync_ion_alloc_buffer (struct ion_client *client, int ion_fd, UINT *mva, struct ion_handle **hnd);
void disp_sync_ion_cache_flush(struct ion_client *client, struct ion_handle *handle);

/**
 * Release all buffer mva of whose fence is signaled.
 * @param layer, specify which timeline (OVL layer)
 */
void disp_sync_release_buffer(UINT layer);

/**
 * Signal all fences of this timeline to given index
 * @param layer, specify which timeline
 * @param idx, all behind this will be signaled
 */
void disp_sync_signal_fence(UINT layer, UINT idx);
SYNC_STATUS disp_sync_prepare_buffer(struct fb_overlay_buffer *buf);
UINT disp_sync_query_buffer_mva(UINT layer, UINT idx);

/**
 * Release all resource(buffer,fence) on this timeline
 * @param layer, specify which timeline
 */
void disp_sync_release(UINT layer);
void disp_sync_init(void);
void disp_sync_deinit(void);

#endif //__DISP_SYNC_H
