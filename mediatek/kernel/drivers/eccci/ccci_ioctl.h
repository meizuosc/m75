#ifndef __CCCI_IOCTL_H__
#define __CCCI_IOCTL_H__

#include <asm/io.h>

// CCCI == EEMCS
#define CCCI_IOC_MAGIC 'C'
#define CCCI_IOC_MD_RESET				_IO(CCCI_IOC_MAGIC, 0) // mdlogger // META // muxreport
#define CCCI_IOC_GET_MD_STATE			_IOR(CCCI_IOC_MAGIC, 1, unsigned int) // audio
#define CCCI_IOC_PCM_BASE_ADDR			_IOR(CCCI_IOC_MAGIC, 2, unsigned int) // audio
#define CCCI_IOC_PCM_LEN				_IOR(CCCI_IOC_MAGIC, 3, unsigned int) // audio
#define CCCI_IOC_FORCE_MD_ASSERT		_IO(CCCI_IOC_MAGIC, 4) // muxreport // mdlogger
#define CCCI_IOC_ALLOC_MD_LOG_MEM		_IO(CCCI_IOC_MAGIC, 5) // mdlogger
#define CCCI_IOC_DO_MD_RST				_IO(CCCI_IOC_MAGIC, 6) // md_init
#define CCCI_IOC_SEND_RUN_TIME_DATA		_IO(CCCI_IOC_MAGIC, 7) // md_init
#define CCCI_IOC_GET_MD_INFO			_IOR(CCCI_IOC_MAGIC, 8, unsigned int) // md_init
#define CCCI_IOC_GET_MD_EX_TYPE			_IOR(CCCI_IOC_MAGIC, 9, unsigned int) // mdlogger
#define CCCI_IOC_SEND_STOP_MD_REQUEST	_IO(CCCI_IOC_MAGIC, 10) // muxreport
#define CCCI_IOC_SEND_START_MD_REQUEST	_IO(CCCI_IOC_MAGIC, 11) // muxreport
#define CCCI_IOC_DO_STOP_MD				_IO(CCCI_IOC_MAGIC, 12) // md_init
#define CCCI_IOC_DO_START_MD			_IO(CCCI_IOC_MAGIC, 13) // md_init
#define CCCI_IOC_ENTER_DEEP_FLIGHT		_IO(CCCI_IOC_MAGIC, 14) // RILD // factory
#define CCCI_IOC_LEAVE_DEEP_FLIGHT		_IO(CCCI_IOC_MAGIC, 15) // RILD // factory
#define CCCI_IOC_POWER_ON_MD			_IO(CCCI_IOC_MAGIC, 16) // md_init
#define CCCI_IOC_POWER_OFF_MD			_IO(CCCI_IOC_MAGIC, 17) // md_init
#define CCCI_IOC_POWER_ON_MD_REQUEST	_IO(CCCI_IOC_MAGIC, 18)
#define CCCI_IOC_POWER_OFF_MD_REQUEST	_IO(CCCI_IOC_MAGIC, 19)
#define CCCI_IOC_SIM_SWITCH				_IOW(CCCI_IOC_MAGIC, 20, unsigned int) // RILD // factory
#define CCCI_IOC_SEND_BATTERY_INFO		_IO(CCCI_IOC_MAGIC, 21) // md_init 
#define CCCI_IOC_SIM_SWITCH_TYPE		_IOR(CCCI_IOC_MAGIC, 22, unsigned int) // RILD
#define CCCI_IOC_STORE_SIM_MODE			_IOW(CCCI_IOC_MAGIC, 23, unsigned int) // RILD
#define CCCI_IOC_GET_SIM_MODE			_IOR(CCCI_IOC_MAGIC, 24, unsigned int) // RILD
#define CCCI_IOC_RELOAD_MD_TYPE			_IO(CCCI_IOC_MAGIC, 25) // META // md_init // muxreport
#define CCCI_IOC_GET_SIM_TYPE			_IOR(CCCI_IOC_MAGIC, 26, unsigned int) // terservice
#define CCCI_IOC_ENABLE_GET_SIM_TYPE	_IOW(CCCI_IOC_MAGIC, 27, unsigned int) // terservice
#define CCCI_IOC_SEND_ICUSB_NOTIFY		_IOW(CCCI_IOC_MAGIC, 28, unsigned int) // icusbd
#define CCCI_IOC_SET_MD_IMG_EXIST		_IOW(CCCI_IOC_MAGIC, 29, unsigned int) // md_init
#define CCCI_IOC_GET_MD_IMG_EXIST		_IOR(CCCI_IOC_MAGIC, 30, unsigned int) // META
#define CCCI_IOC_GET_MD_TYPE			_IOR(CCCI_IOC_MAGIC, 31, unsigned int) // RILD
#define CCCI_IOC_STORE_MD_TYPE			_IOW(CCCI_IOC_MAGIC, 32, unsigned int) // RILD
#define CCCI_IOC_GET_MD_TYPE_SAVING		_IOR(CCCI_IOC_MAGIC, 33, unsigned int) // META
#define CCCI_IOC_GET_EXT_MD_POST_FIX	_IOR(CCCI_IOC_MAGIC, 34, char[32]) // eemcs_fsd // mdlogger
#define CCCI_IOC_FORCE_FD				_IOW(CCCI_IOC_MAGIC, 35, unsigned int) // RILD(6577)
#define CCCI_IOC_AP_ENG_BUILD			_IOW(CCCI_IOC_MAGIC, 36, unsigned int) // md_init(6577)
#define CCCI_IOC_GET_MD_MEM_SIZE		_IOR(CCCI_IOC_MAGIC, 37, unsigned int) // md_init(6577)
#define CCCI_IOC_UPDATE_SIM_SLOT_CFG	_IOW(CCCI_IOC_MAGIC, 38, unsigned int) // RILD
#define CCCI_IOC_GET_CFG_SETTING		_IOW(CCCI_IOC_MAGIC, 39, unsigned int) // md_init

#define CCCI_IOC_SET_MD_SBP_CFG			_IOW(CCCI_IOC_MAGIC, 40, unsigned int) // md_init
#define CCCI_IOC_GET_MD_SBP_CFG			_IOW(CCCI_IOC_MAGIC, 41, unsigned int) // md_init

#define CCCI_IOC_SET_HEADER				_IO(CCCI_IOC_MAGIC,  112)				// emcs_va
#define CCCI_IOC_CLR_HEADER				_IO(CCCI_IOC_MAGIC,  113)				// emcs_va
#define CCCI_IOC_DL_TRAFFIC_CONTROL		_IOW(CCCI_IOC_MAGIC, 119, unsigned int) // mdlogger

#define CCCI_IPC_MAGIC 'P' // only for IPC user
// CCCI == EEMCS
#define CCCI_IPC_RESET_RECV		_IO(CCCI_IPC_MAGIC,0)
#define CCCI_IPC_RESET_SEND		_IO(CCCI_IPC_MAGIC,1)
#define CCCI_IPC_WAIT_MD_READY	_IO(CCCI_IPC_MAGIC,2)
#define CCCI_IPC_KERN_WRITE_TEST    _IO(CCCI_IPC_MAGIC,3)

#endif //__CCCI_IOCTL_H__