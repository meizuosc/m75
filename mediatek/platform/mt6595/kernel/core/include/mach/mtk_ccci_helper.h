#ifndef __MTK_CCCI_HELPER_H
#define __MTK_CCCI_HELPER_H

// export to other kernel modules, better not let other module include ECCCI header directly (except IPC...)

enum { 
	MD_STATE_INVALID = 0,
	MD_STATE_BOOTING = 1,
	MD_STATE_READY = 2,
	MD_STATE_EXCEPTION = 3
}; // align to MD_BOOT_STAGE

enum {
	ID_GET_MD_WAKEUP_SRC = 0,   // for SPM
	ID_CCCI_DORMANCY = 1,       // abandoned
    ID_LOCK_MD_SLEEP = 2,       // abandoned
	ID_ACK_MD_SLEEP = 3,        // abandoned
	ID_SSW_SWITCH_MODE = 4,     // abandoned
	ID_SET_MD_TX_LEVEL = 5,     // abandoned
	ID_GET_TXPOWER = 6,			// for thermal
	ID_IPO_H_RESTORE_CB = 7,    // abandoned
	ID_FORCE_MD_ASSERT = 8,     // abandoned
	ID_PAUSE_LTE = 9,			// for DVFS
	ID_STORE_SIM_SWITCH_MODE = 10,
	ID_GET_SIM_SWITCH_MODE = 11,
	ID_GET_MD_STATE = 12,		// for DVFS
	//used for throttling feature - start
	ID_THROTTLING_CFG = 13,		// For MD SW throughput throttling
	//used for throttling feature - end
};

enum {
	MODEM_CAP_NAPI = (1<<0),
	MODEM_CAP_TXBUSY_STOP = (1<<1),
};

struct ccci_dev_cfg {
	unsigned int index;
	unsigned int major;
	unsigned int minor_base;
	unsigned int capability;
};

#if 0
typedef int (*ccci_kern_cb_func_t)(int, char *, unsigned int);
int register_ccci_kern_func_by_md_id(int md_id, unsigned int id, ccci_kern_cb_func_t func);
#endif

int exec_ccci_kern_func_by_md_id(int md_id, unsigned int id, char *buf, unsigned int len);
//used for throttling feature - start
unsigned long ccci_get_md_boot_count(int md_id);
//used for throttling feature - end

#endif
