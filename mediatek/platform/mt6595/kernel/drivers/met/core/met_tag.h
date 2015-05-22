#ifndef __MET_TAG_EX_H__
#define __MET_TAG_EX_H__

#ifdef BUILD_WITH_MET
void force_sample(void *unused);
#else
#include <linux/string.h>
#endif

/* Black List Table */
typedef struct _bltable_t {
	struct mutex mlock;
	/* flag - Bit31: Global ON/OFF; Bit0~30: ON/OF slot map of class_id */
	unsigned int flag;
	int class_id[MAX_EVENT_CLASS];
} bltable_t;

extern struct met_api_tbl met_ext_api;

#endif	/* __MET_TAG_EX_H__ */

