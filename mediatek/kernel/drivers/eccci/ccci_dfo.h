#ifndef __CCCI_DFO_H__
#define __CCCI_DFO_H__

#include "ccci_core.h"
#include "ccci_debug.h"

#define MD1_EN (1<<0)
#define MD2_EN (1<<1)
#define MD5_EN (1<<4)

#define MD1_SETTING_ACTIVE	(1<<0)
#define MD2_SETTING_ACTIVE	(1<<1)

#define MD5_SETTING_ACTIVE	(1<<4)

#define MD_2G_FLAG (1<<0)
#define MD_FDD_FLAG (1<<1)
#define MD_TDD_FLAG (1<<2)
#define MD_LTE_FLAG (1<<3)
#define MD_SGLTE_FLAG (1<<4)

#define MD_WG_FLAG (MD_FDD_FLAG|MD_2G_FLAG)
#define MD_TG_FLAG (MD_TDD_FLAG|MD_2G_FLAG)
#define MD_LWG_FLAG (MD_LTE_FLAG|MD_FDD_FLAG|MD_2G_FLAG)
#define MD_LTG_FLAG (MD_LTE_FLAG|MD_TDD_FLAG|MD_2G_FLAG)

#define MD1_MEM_SIZE (80*1024*1024) // MD ROM+RAM size
#define MD1_SMEM_SIZE (2*1024*1024) // share memory size
#define MD2_MEM_SIZE (22*1024*1024)
#define MD2_SMEM_SIZE (2*1024*1024)

#define CCCI_SMEM_SIZE_EXCEPTION 4096 // smem size we dump when EE
#define CCCI_SMEM_OFFSET_EXREC 2048 // where the exception record begain in smem
#define CCCI_SMEM_OFFSET_EPON 0xC64
#define CCCI_SMEM_OFFSET_SEQERR 0x34

typedef enum {
	md_type_invalid = 0,
	modem_2g = 1,
	modem_3g,
	modem_wg,
	modem_tg,
    modem_lwg,
    modem_ltg,
    modem_sglte,
    MAX_IMG_NUM = modem_sglte // this enum starts from 1
} MD_LOAD_TYPE;

struct dfo_item
{
	char name[32];
	int  value;
};

// image name/path
#define MOEDM_IMAGE_NAME "modem.img"
#define DSP_IMAGE_NAME "DSP_ROM"
#define CONFIG_MODEM_FIRMWARE_PATH "/system/etc/firmware/"
#define CONFIG_MODEM_FIRMWARE_CIP_PATH "/custom/etc/firmware/"
#define IMG_ERR_STR_LEN 64

// image header constants
#define MD_HEADER_MAGIC_NO "CHECK_HEADER"
#define MD_HEADER_VER_NO    (2)
#define VER_2G_STR  "2g"
#define VER_3G_STR  "3g"
#define VER_WG_STR   "wg"
#define VER_TG_STR   "tg"
#define VER_LWG_STR   "lwg"
#define VER_SGLTE_STR   "sglte"
#define VER_LTG_STR   "ltg"
#define VER_INVALID_STR  "invalid"
#define DEBUG_STR   "Debug"
#define RELEASE_STR  "Release"
#define INVALID_STR  "INVALID"
#define AP_PLATFORM_LEN 16

struct ccci_setting {
    int sim_mode;    
    int slot1_mode; // 0:CDMA 1:GSM 2:WCDMA 3:TDCDMA
    int slot2_mode; // 0:CDMA 1:GSM 2:WCDMA 3:TDCDMA
};

struct md_check_header {
	u8 check_header[12];	    /* magic number is "CHECK_HEADER"*/
	u32 header_verno;	        /* header structure version number */
	u32 product_ver;	        /* 0x0:invalid; 0x1:debug version; 0x2:release version */
	u32 image_type;	            /* 0x0:invalid; 0x1:2G modem; 0x2: 3G modem */
	u8 platform[16];	        /* MT6573_S01 or MT6573_S02 */
	u8 build_time[64];	        /* build time string */
	u8 build_ver[64];	        /* project version, ex:11A_MD.W11.28 */
	u8 bind_sys_id;	            /* bind to md sys id, MD SYS1: 1, MD SYS2: 2 */
	u8 ext_attr;                /* no shrink: 0, shrink: 1*/
	u8 reserved[2];             /* for reserved */
	u32 mem_size;       		/* md ROM/RAM image size requested by md */
	u32 md_img_size;            /* md image size, exclude head size*/
	u32 reserved_info;          /* for reserved */
	u32 size;	                /* the size of this structure */
} __attribute__ ((packed));

void ccci_config_modem(struct ccci_modem *md);
int ccci_load_firmware(struct ccci_modem *md, MD_IMG_TYPE img_type, char img_err_str[IMG_ERR_STR_LEN]);
char *ccci_get_md_info_str(struct ccci_modem *md);
int ccci_init_security(struct ccci_modem *md);
void ccci_reload_md_type(struct ccci_modem *md, int type);
int ccci_get_sim_switch_mode(void);
int ccci_store_sim_switch_mode(struct ccci_modem *md, int simmode);
struct ccci_setting* ccci_get_common_setting(int md_id);

#endif //__CCCI_DFO_H__
