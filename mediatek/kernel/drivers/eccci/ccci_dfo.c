#include <linux/device.h>
#include <linux/syscalls.h>
#include <linux/module.h>
#include <linux/memblock.h>
#include <asm/memblock.h>
#include <mach/sec_osal.h>
#include <mach/mt_sec_export.h>
#include <mach/mt_boot.h>
#include <asm/setup.h>
#include <linux/of_fdt.h>
#include "ccci_dfo.h"
#include "ccci_platform.h"
#include "port_kernel.h"

// modem index is not continuous, so there may be gap in this arrays
static unsigned int md_support[MAX_MD_NUM];
static unsigned int meta_md_support[MAX_MD_NUM];
static unsigned int md_usage_case = 0;
static unsigned int modem_num = 0;
static struct ccci_setting ccci_cfg_setting;
static unsigned int md_resv_mem_size[MAX_MD_NUM]; // MD ROM+RAM
static unsigned int md_resv_smem_size[MAX_MD_NUM]; // share memory
static unsigned int modem_size_list[MAX_MD_NUM];

static phys_addr_t md_resv_mem_addr[MAX_MD_NUM]; 
static phys_addr_t md_resv_smem_addr[MAX_MD_NUM]; 
static phys_addr_t md_resv_smem_base;

static struct md_check_header md_img_header[MAX_MD_NUM];
char md_img_info_str[MAX_MD_NUM][256];

static char ap_platform[AP_PLATFORM_LEN]="";
static struct dfo_item ccci_dfo_setting[] =
{
	{"MTK_ENABLE_MD1",	1},
#ifdef MTK_UMTS_TDD128_MODE
	{"MTK_MD1_SUPPORT",	modem_ltg},
#else
	{"MTK_MD1_SUPPORT",	modem_lwg},
#endif
	{"MD1_SMEM_SIZE",	MD1_SMEM_SIZE},
	{"MD1_SIZE",		MD1_MEM_SIZE},
	
	{"MTK_ENABLE_MD2",	0},
	{"MTK_MD2_SUPPORT",	modem_ltg},
	{"MD2_SMEM_SIZE",	MD2_SMEM_SIZE},
	{"MD2_SIZE",		MD2_MEM_SIZE},
	
	{"MTK_ENABLE_MD5",	0},
	{"MTK_MD5_SUPPORT",	modem_lwg},
};
static char * type_str[] = {[md_type_invalid]=VER_INVALID_STR, 
							[modem_2g]=VER_2G_STR, 
							[modem_3g]=VER_3G_STR,
							[modem_wg]=VER_WG_STR,
							[modem_tg]=VER_TG_STR,
							[modem_lwg]=VER_LWG_STR,
							[modem_ltg]=VER_LTG_STR,
							[modem_sglte]=VER_SGLTE_STR};
static char * product_str[] = {[INVALID_VARSION]=INVALID_STR, 
							   [DEBUG_VERSION]=DEBUG_STR, 
							   [RELEASE_VERSION]=RELEASE_STR};

phys_addr_t *__get_modem_start_addr_list(void){return md_resv_mem_addr;}

int scan_image_list(int md_id, char fmt[], int out_img_list[], int img_list_size)
{
	int i;
	int img_num = 0;
	char full_path[64] = {0};
	char img_name[32] = {0};
	struct file *filp = NULL;
	for(i=0; i<(sizeof(type_str)/sizeof(char*)); i++) {
		snprintf(img_name, 32, fmt, md_id+1, type_str[i]);
		// Find at CIP first
		snprintf(full_path, 64, "%s%s", CONFIG_MODEM_FIRMWARE_CIP_PATH, img_name);
		CCCI_INF_MSG(md_id, CORE, "Find:%s\n" ,full_path);
		filp = filp_open(full_path, O_RDONLY, 0644);
		if (IS_ERR(filp)) {
			// Find at default
			snprintf(full_path, 64, "%s%s", CONFIG_MODEM_FIRMWARE_PATH, img_name);
			CCCI_INF_MSG(md_id, CORE, "Find:%s\n" ,full_path);
			filp = filp_open(full_path, O_RDONLY, 0644);
			if (IS_ERR(filp)) {
				CCCI_INF_MSG(md_id, CORE, "%s not found(%d,%d)\n" ,full_path, img_num, i);
				continue;
			}
		}
		// Run here means open image success
		filp_close(filp, NULL);
		CCCI_INF_MSG(md_id, CORE, "Image:%s found\n", full_path);
		if(img_num<img_list_size)
			out_img_list[img_num] = i;
		img_num++;
	}
	return img_num;	
}

static int check_md_header(struct ccci_modem *md, 
							void *parse_addr, 
							struct ccci_image_info *image)
{
	int ret;
	bool md_type_check = false;
	bool md_plat_check = false;
	bool md_sys_match  = false;
	bool md_size_check = false;
	unsigned int md_size = 0;
	struct md_check_header *head = &md_img_header[md->index];

	memcpy(head, (void*)(parse_addr - sizeof(struct md_check_header)), sizeof(struct md_check_header));

	CCCI_INF_MSG(md->index, CORE, "**********************MD image check***************************\n");
	ret = strncmp(head->check_header, MD_HEADER_MAGIC_NO, 12);
	if(ret) {
		CCCI_ERR_MSG(md->index, CORE, "md check header not exist!\n");
		ret = 0;
	}
	else {
		if(head->header_verno != MD_HEADER_VER_NO) {
			CCCI_ERR_MSG(md->index, CORE, "[Error]md check header version mis-match to AP:[%d]!\n", 
				head->header_verno);
		} else {
#ifdef ENABLE_2G_3G_CHECK
			if((head->image_type != 0) && (head->image_type == md->config.load_type)) {
				md_type_check = true;
			}
#else
			md_type_check = true;
#endif

#ifdef ENABLE_CHIP_VER_CHECK
			if(!strncmp(head->platform, ap_platform, AP_PLATFORM_LEN)) {
				md_plat_check = true;
			}
#else
			md_plat_check = true;
#endif

			if(head->bind_sys_id == (md->index+1)) {
				md_sys_match = true;
			}

#ifdef ENABLE_MEM_SIZE_CHECK
			if(head->header_verno >= 2) {
				md_size = md->mem_layout.md_region_size;
				if (head->mem_size == md_size) {
					md_size_check = true;
				} else if(head->mem_size < md_size) {
					md_size_check = true;
					CCCI_INF_MSG(md->index, CORE, "[Warning]md size in md header isn't sync to DFO setting: (%08x, %08x)\n",
						head->mem_size, md_size);
				}
				image->img_info.mem_size = head->mem_size;
				image->ap_info.mem_size = md_size;
			} else {
				md_size_check = true;
			}
#else
			md_size_check = true;
#endif

			image->ap_info.image_type = type_str[md->config.load_type];
			image->ap_info.platform = ap_platform;
			image->img_info.image_type = type_str[head->image_type];
			image->img_info.platform = head->platform;
			image->img_info.build_time = head->build_time;
			image->img_info.build_ver = head->build_ver;
			image->img_info.product_ver = product_str[head->product_ver];
			image->img_info.version = head->product_ver;

			if(md_type_check && md_plat_check && md_sys_match && md_size_check) {
				CCCI_INF_MSG(md->index, CORE, "Modem header check OK!\n");
			} else {
				CCCI_INF_MSG(md->index, CORE, "[Error]Modem header check fail!\n");
				if(!md_type_check)
					CCCI_INF_MSG(md->index, CORE, "[Reason]MD type(2G/3G) mis-match to AP!\n");

				if(!md_plat_check)
					CCCI_INF_MSG(md->index, CORE, "[Reason]MD platform mis-match to AP!\n");

				if(!md_sys_match)
					CCCI_INF_MSG(md->index, CORE, "[Reason]MD image is not for MD SYS%d!\n", md->index+1);

				if(!md_size_check)
					CCCI_INF_MSG(md->index, CORE, "[Reason]MD mem size mis-match to AP setting!\n");

				ret = -CCCI_ERR_LOAD_IMG_MD_CHECK;
			}

			CCCI_INF_MSG(md->index, CORE, "(MD)[type]=%s, (AP)[type]=%s\n", image->img_info.image_type, image->ap_info.image_type);
			CCCI_INF_MSG(md->index, CORE, "(MD)[plat]=%s, (AP)[plat]=%s\n", image->img_info.platform, image->ap_info.platform);
			if(head->header_verno >= 2) {
				CCCI_INF_MSG(md->index, CORE, "(MD)[size]=%x, (AP)[size]=%x\n",image->img_info.mem_size, image->ap_info.mem_size);
				if (head->md_img_size) {
					if (image->size >= head->md_img_size)
						image->size = head->md_img_size;
					else {
						CCCI_INF_MSG(md->index, CORE, "[Reason]MD image size mis-match to AP!\n");
						ret = -CCCI_ERR_LOAD_IMG_MD_CHECK;
					}
					image->ap_info.md_img_size = image->size;
					image->img_info.md_img_size = head->md_img_size;
				}
				// else {image->size -= 0x1A0;} //workaround for md not check in check header
				CCCI_INF_MSG(md->index, CORE, "(MD)[img_size]=%x, (AP)[img_size]=%x\n", head->md_img_size, image->size);
			}
			CCCI_INF_MSG(md->index, CORE, "(MD)[build_ver]=%s, [build_time]=%s\n",image->img_info.build_ver, image->img_info.build_time);
			CCCI_INF_MSG(md->index, CORE, "(MD)[product_ver]=%s\n",image->img_info.product_ver);
		}
	}
	CCCI_INF_MSG(md->index, CORE, "**********************MD image check***************************\n");

	return ret;
}

static int check_dsp_header(struct ccci_modem *md,  
							void *parse_addr, 
							struct ccci_image_info *image)
{
	return 0;
}

#ifdef ENABLE_MD_IMG_SECURITY_FEATURE
static int sec_lib_version_check(void)
{
	int ret = 0;

	int sec_lib_ver = masp_ccci_version_info();
	if(sec_lib_ver != CURR_SEC_CCCI_SYNC_VER){
		CCCI_ERR_COM_MSG("[Error]sec lib for ccci mismatch: sec_ver:%d, ccci_ver:%d\n", sec_lib_ver, CURR_SEC_CCCI_SYNC_VER);
		ret = -1;
	}

	return ret;
}

//--------------------------------------------------------------------------------------------------//
// New signature check version. 2012-2-2. 
// Change to use masp_ccci_signfmt_verify_file(char *file_path, unsigned int *data_offset, unsigned int *data_sec_len)
//  masp_ccci_signfmt_verify_file parameter description
//    @ file_path: such as etc/firmware/modem.img
//    @ data_offset: the offset address that bypass signature header
//    @ data_sec_len: length of signature header + tail
//    @ return value: 0-success;
//---------------------------------------------------------------------------------------------------//
static int signature_check_v2(struct ccci_modem *md, char* file_path, unsigned int *sec_tail_length)
{
	unsigned int bypass_sec_header_offset = 0;
	unsigned int sec_total_len = 0;

	if( masp_ccci_signfmt_verify_file(file_path, &bypass_sec_header_offset, &sec_total_len) == 0 ){
		//signature lib check success
		//-- check return value
		CCCI_INF_MSG(md->index, CORE, "sign check ret value 0x%x, 0x%x!\n", bypass_sec_header_offset, sec_total_len);
		if(bypass_sec_header_offset > sec_total_len){
			CCCI_INF_MSG(md->index, CORE, "sign check fail(0x%x, 0x%x!)!\n", bypass_sec_header_offset, sec_total_len);
			return -CCCI_ERR_LOAD_IMG_SIGN_FAIL;
		} else {
			CCCI_INF_MSG(md->index, CORE, "sign check success(0x%x, 0x%x)!\n", bypass_sec_header_offset, sec_total_len);
			*sec_tail_length = sec_total_len - bypass_sec_header_offset;
			return (int)bypass_sec_header_offset; // Note here, offset is more than 2G is not hoped 
		}
	} else {
		CCCI_INF_MSG(md->index, CORE, "sign check fail!\n");
		return -CCCI_ERR_LOAD_IMG_SIGN_FAIL;
	}
}

static int load_cipher_firmware_v2(struct ccci_modem *md, 
									int fp_id, 
									struct ccci_image_info *img,
									unsigned int cipher_img_offset, 
									unsigned int cipher_img_len)
{
	int ret;
	void *addr = ioremap_nocache(img->address,cipher_img_len);
	void *addr_bak = addr;
	unsigned int data_offset;

	if (addr==NULL) {
		CCCI_INF_MSG(md->index, CORE, "ioremap image fialed!\n");
		ret = -CCCI_ERR_LOAD_IMG_NO_ADDR;
		goto out;
	}

	if(SEC_OK != masp_ccci_decrypt_cipherfmt(fp_id, cipher_img_offset, (char*)addr, cipher_img_len, &data_offset) ) {
		CCCI_INF_MSG(md->index, CORE, "cipher image decrypt fail!\n");
		ret = -CCCI_ERR_LOAD_IMG_CIPHER_FAIL;
		goto unmap_out;
	}

	img->size = cipher_img_len;
	img->offset += data_offset;	
	addr+=cipher_img_len;

	ret = check_md_header(md, addr, img);

unmap_out:
	iounmap(addr_bak);
out:
	return ret;
}
#endif

static void collect_md_mem_setting(void)
{
	phys_addr_t *addr;
	unsigned int md1_en, md2_en;

	addr = __get_modem_start_addr_list(); // MD ROM start address should be 32M align as remap hardware limitation

	if( (md_usage_case&(MD1_EN|MD2_EN))==(MD1_EN|MD2_EN)) { // Both two MD enabled
		md1_en = 1;
		md2_en = 1;
		md_resv_mem_addr[MD_SYS1] = addr[0];
		md_resv_mem_addr[MD_SYS2] = addr[1];
		md_resv_smem_addr[MD_SYS1] = addr[0] + md_resv_mem_size[MD_SYS1];
		md_resv_smem_addr[MD_SYS2] = addr[0] + md_resv_mem_size[MD_SYS1] + md_resv_smem_size[MD_SYS1];
		md_resv_smem_base = addr[0]; // attention, share memory's base is not where share memory actually starts, but the same as MD ROM, check ccci_set_mem_remap()
	} else if( (md_usage_case&(MD1_EN|MD2_EN))==(MD1_EN)) { //Only MD1 enabled
		md1_en = 1;
		md2_en = 0;
		md_resv_mem_addr[MD_SYS1] = addr[0];
		md_resv_mem_addr[MD_SYS2] = 0;
		md_resv_smem_addr[MD_SYS1] = addr[0] + md_resv_mem_size[MD_SYS1];
		md_resv_smem_addr[MD_SYS2] = 0;
		md_resv_smem_base = addr[0];
	} else if( (md_usage_case&(MD1_EN|MD2_EN))==(MD2_EN)) { //Only MD2 enabled
		md1_en = 0;
		md2_en = 1;
		md_resv_mem_addr[MD_SYS1] = 0;
		md_resv_mem_addr[MD_SYS2] = addr[0];
		md_resv_smem_addr[MD_SYS1] = 0;
		md_resv_smem_addr[MD_SYS2] = addr[0] + md_resv_mem_size[MD_SYS2];
		md_resv_smem_base = addr[0];
	} else { // No MD is enabled
		md1_en = 0;
		md2_en = 0;
		md_resv_mem_addr[MD_SYS1] = 0;
		md_resv_mem_addr[MD_SYS2] = 0;
		md_resv_smem_addr[MD_SYS1] = 0;
		md_resv_smem_addr[MD_SYS2] = 0;
		md_resv_smem_base = 0;
	}

	if (md1_en && ((md_resv_mem_addr[MD_SYS1]&(CCCI_MEM_ALIGN - 1)) != 0))
		CCCI_ERR_COM_MSG("md1 memory addr is not 32M align!!!\n");

	if (md2_en && ((md_resv_mem_addr[MD_SYS2]&(CCCI_MEM_ALIGN - 1)) != 0))
		CCCI_ERR_COM_MSG("md2 memory addr is not 32M align!!!\n");

	if (md1_en && ((md_resv_smem_addr[MD_SYS1]&(CCCI_SMEM_ALIGN - 1)) != 0))
		CCCI_ERR_COM_MSG("md1 share memory addr %llX is not %x align!!\n", (unsigned long long)md_resv_smem_addr[MD_SYS1], CCCI_SMEM_ALIGN);

	if (md2_en && ((md_resv_smem_addr[MD_SYS2]&(CCCI_SMEM_ALIGN - 1)) != 0))
		CCCI_ERR_COM_MSG("md2 share memory addr %llX is not %x align!!\n", (unsigned long long)md_resv_smem_addr[MD_SYS2], CCCI_SMEM_ALIGN);

	CCCI_INF_COM_MSG("MD1_EN(%d):MD2_EN(%d):MemBase(0x%llX)\n", md1_en, md2_en, (unsigned long long)md_resv_smem_base);

	CCCI_INF_COM_MSG("MemStart(0x%llX:0x%llX):MemSize(0x%08X:0x%08X)\n", \
		(unsigned long long)md_resv_mem_addr[MD_SYS1], (unsigned long long)md_resv_mem_addr[MD_SYS2], \
		md_resv_mem_size[MD_SYS1], md_resv_mem_size[MD_SYS2]);

	CCCI_INF_COM_MSG("SmemStart(0x%llX:0x%llX):SmemSize(0x%08X:0x%08X)\n", \
		(unsigned long long)md_resv_smem_addr[MD_SYS1], (unsigned long long)md_resv_smem_addr[MD_SYS2], \
		md_resv_smem_size[MD_SYS1], md_resv_smem_size[MD_SYS2]);
}

unsigned int get_modem_is_enabled(int md_id)
{
	return !!(md_usage_case & (1<<md_id));
}

static int get_dfo_setting(char item[], unsigned int *val)
{
	char *ccci_name;
	int  ccci_value;
	int  i;

	for (i=0; i<ARRAY_SIZE(ccci_dfo_setting); i++) {
		ccci_name = ccci_dfo_setting[i].name;
		ccci_value = ccci_dfo_setting[i].value;
		if(!strcmp(ccci_name, item)) {
			CCCI_INF_COM_MSG("Get DFO:%s:0x%08X\n", ccci_name, ccci_value);
			*val = (unsigned int)ccci_value;
			return 0;
		}
	}
	CCCI_ERR_COM_MSG("DFO:%d/%s not found\n", i+1, item);
	return -CCCI_ERR_INVALID_PARAM;
}

static void cal_md_mem_usage(void)
{
	unsigned int tmp;
	unsigned int md1_en = 0;
	unsigned int md2_en = 0;
	unsigned int md5_en = 0;
	md_usage_case = 0;
	
	// MTK_ENABLE_MD*
	if(get_dfo_setting("MTK_ENABLE_MD1", &tmp) == 0) {
		if(tmp > 0)
			md1_en = 1;
	}
	if(get_dfo_setting("MTK_ENABLE_MD2", &tmp) == 0) {
		if(tmp > 0) 
			md2_en = 1;
	}
	if(get_dfo_setting("MTK_ENABLE_MD5", &tmp) == 0) {
		if(tmp > 0) 
			md5_en = 1;
	}
	// MTK_MD*_SUPPORT
	if(get_dfo_setting("MTK_MD1_SUPPORT", &tmp) == 0) {
		md_support[MD_SYS1] = tmp;
	}
	if(get_dfo_setting("MTK_MD2_SUPPORT", &tmp) == 0) {
		md_support[MD_SYS2] = tmp;
	}
	if(get_dfo_setting("MTK_MD5_SUPPORT", &tmp) == 0) {
		md_support[MD_SYS5] = tmp;
	}
	// MD*_SIZE
	if(get_dfo_setting("MD1_SIZE", &tmp) == 0) {
		/*
		 * for legacy CCCI: make share memory start address to be 2MB align, as share 
		 * memory size is 2MB - requested by MD MPU.
		 * for ECCCI: ROM+RAM size will be align to 1M, and share memory is 2K,
		 * 1M alignment is also 2K alignment.
		 */
		tmp = round_up(tmp, CCCI_SMEM_ALIGN);
		md_resv_mem_size[MD_SYS1] = tmp;
	}
	if(get_dfo_setting("MD2_SIZE", &tmp) == 0) {
		tmp = round_up(tmp, CCCI_SMEM_ALIGN);
		md_resv_mem_size[MD_SYS2] = tmp;
	}
	// MD*_SMEM_SIZE
	if(get_dfo_setting("MD1_SMEM_SIZE", &tmp) == 0) {
		md_resv_smem_size[MD_SYS1] = tmp;
	}
	if(get_dfo_setting("MD2_SMEM_SIZE", &tmp) == 0) {
		md_resv_smem_size[MD_SYS2] = tmp;
	}
	
	//meta setting is parsed in mt_fix, before mt_reserve
	if(meta_md_support[MD_SYS1]){
		md_support[MD_SYS1] = meta_md_support[MD_SYS1];
		CCCI_INF_COM_MSG("set md%d type with meta value:%d\n", MD_SYS1, meta_md_support[MD_SYS1]);
		meta_md_support[MD_SYS1]=0;
	}

	// Setting conflict checking
	if(md1_en && (md_resv_smem_size[MD_SYS1]>0) && (md_resv_mem_size[MD_SYS1]>0)) {
		// Setting is OK
	} else if (md1_en && ((md_resv_smem_size[MD_SYS1]<=0) || (md_resv_mem_size[MD_SYS1]<=0))) {
		CCCI_ERR_COM_MSG("DFO Setting for md1 wrong: <%d:0x%08X:0x%08X>\n", 
				md1_en, md_resv_mem_size[MD_SYS1], md_resv_smem_size[MD_SYS1]);
		md_resv_smem_size[MD_SYS1] = MD1_SMEM_SIZE;
		md_resv_mem_size[MD_SYS1] = MD1_MEM_SIZE;
	} else {
		// Has conflict
		CCCI_ERR_COM_MSG("DFO Setting for md1 conflict: <%d:0x%08X:0x%08X>\n", 
				md1_en, md_resv_mem_size[MD_SYS1], md_resv_smem_size[MD_SYS1]);
		md1_en = 0;
		md_resv_smem_size[MD_SYS1]=0;
		md_resv_mem_size[MD_SYS1]=0;
	}

	if(md2_en && (md_resv_smem_size[MD_SYS2]>0) && (md_resv_mem_size[MD_SYS2]>0)) {
		// Setting is OK
	} else if (md2_en && ((md_resv_smem_size[MD_SYS2]<=0) || (md_resv_mem_size[MD_SYS2]<=0))) {
		CCCI_ERR_COM_MSG("DFO Setting for md2 wrong: <%d:0x%08X:0x%08X>\n", 
				md2_en, md_resv_mem_size[MD_SYS2], md_resv_smem_size[MD_SYS2]);
		md_resv_smem_size[MD_SYS2] = MD2_SMEM_SIZE;
		md_resv_mem_size[MD_SYS2] = MD2_MEM_SIZE;
	} else {
		// Has conflict
		CCCI_ERR_COM_MSG("DFO Setting for md2 conflict: <%d:0x%08X:0x%08X>\n", 
				md2_en, md_resv_mem_size[MD_SYS2], md_resv_smem_size[MD_SYS2]);
		md2_en = 0;
		md_resv_smem_size[MD_SYS2]=0;
		md_resv_mem_size[MD_SYS2]=0;
	}

	if(md1_en) {
		md_usage_case |= MD1_EN;
		modem_num++;
	}
	if(md2_en) {
		md_usage_case |= MD2_EN;
		modem_num++;
	}
	if(md5_en) {
		md_usage_case |= MD5_EN;
		modem_num++;
	}

	if( (md_usage_case&(MD1_EN|MD2_EN))==(MD1_EN|MD2_EN)) { // Both two MD enabled
		modem_size_list[0] = md_resv_mem_size[MD_SYS1]+md_resv_smem_size[MD_SYS1]+md_resv_smem_size[MD_SYS2];
		modem_size_list[1] = md_resv_mem_size[MD_SYS2];
	} else if( (md_usage_case&(MD1_EN|MD2_EN))==(MD1_EN)) { //Only MD1 enabled
		modem_size_list[0] = md_resv_mem_size[MD_SYS1]+md_resv_smem_size[MD_SYS1];
		modem_size_list[1] = 0;
	} else if( (md_usage_case&(MD1_EN|MD2_EN))==(MD2_EN)) { //Only MD2 enabled
		modem_size_list[0] = md_resv_mem_size[MD_SYS2]+md_resv_smem_size[MD_SYS2];
		modem_size_list[1] = 0;
	} else { // No MD is enabled
		modem_size_list[0] = 0;
		modem_size_list[1] = 0;
	}
}

unsigned int *ccci_get_modem_size_list(void)
{
    return modem_size_list;
}

unsigned int ccci_get_modem_nr(void)
{
    return modem_num;
}

#ifndef FEATURE_DFO_EN
int ccci_parse_dfo_setting(void *dfo_tbl, int num)
{
	char *ccci_name;
	int  *ccci_value;
	int i;

	for (i=0; i<ARRAY_SIZE(ccci_dfo_setting); i++) {
		ccci_name = ccci_dfo_setting[i].name;
		ccci_value = &(ccci_dfo_setting[i].value);
		CCCI_INF_COM_MSG("No DFO:%s:0x%08X\n", ccci_name, *ccci_value);
	}
	
	cal_md_mem_usage();
	return 0;
}
#else
#ifndef CONFIG_OF
extern int dfo_query(const char *s, unsigned long *v);
int ccci_parse_dfo_setting(void *dfo_tbl, int num)
{
	char *ccci_name;
	int  *ccci_value;
	unsigned long dfo_val = 0;
	int i, found;
	
	for (i=0; i<ARRAY_SIZE(ccci_dfo_setting); i++) {
		ccci_name = ccci_dfo_setting[i].name;
		ccci_value = &(ccci_dfo_setting[i].value);
		found = 0;
		if(dfo_query(ccci_name, &dfo_val)==0 && dfo_val!=0) {
			*ccci_value = (unsigned int)dfo_val;
			found = 1;
		}
		CCCI_INF_COM_MSG("DFO(%d):%s:0x%08X\n", found, ccci_name, *ccci_value);
	}
	
	cal_md_mem_usage();
	return 0;
}
#else
int ccci_parse_dfo_setting(void *dfo_tbl, int num)
{
	char *ccci_name;
	int  *ccci_value;
	int i, j, found;
	tag_dfo_boot *dfo_data;

	if(dfo_tbl == NULL)
		return -CCCI_ERR_INVALID_PARAM;

	dfo_data = (tag_dfo_boot *)dfo_tbl;
	for (i=0; i<ARRAY_SIZE(ccci_dfo_setting); i++) {
		ccci_name = ccci_dfo_setting[i].name;
		ccci_value = &(ccci_dfo_setting[i].value);
		found = 0;
		for (j=0; j<num; j++) {
			if(!strcmp(ccci_name, dfo_data->info[j].name) && dfo_data->info[j].value!=0) {
				*ccci_value = dfo_data->info[j].value;
				found = 1;
			}
		}
		CCCI_INF_COM_MSG("DT(%d):%s:0x%08X\n", found, ccci_name, *ccci_value);
	}
	
	cal_md_mem_usage();
	return 0;
}
#endif
#endif

int ccci_parse_meta_md_setting(unsigned char args[])
{
	unsigned char md_active_setting = args[1];
	unsigned char md_setting_flag = args[0];
	int active_id =  -1;

	if(md_active_setting & MD1_SETTING_ACTIVE)
		active_id = MD_SYS1;
	else if(md_active_setting & MD2_SETTING_ACTIVE)
		active_id = MD_SYS2;
	else if(md_active_setting & MD5_SETTING_ACTIVE)
		active_id = MD_SYS5;
	else
		CCCI_ERR_COM_MSG("META MD setting not found [%d][%d]\n", args[0], args[1]);

	switch(active_id) {
	case MD_SYS1:
	case MD_SYS2:
	case MD_SYS5:
		if(md_setting_flag == MD_2G_FLAG) {
			meta_md_support[active_id] = modem_2g;
		} else if(md_setting_flag == MD_WG_FLAG) {
			meta_md_support[active_id] = modem_wg;
		} else if(md_setting_flag == MD_TG_FLAG) {
			meta_md_support[active_id] = modem_tg;
		} else if(md_setting_flag == MD_LWG_FLAG){
    		meta_md_support[active_id] = modem_lwg;
    	} else if(md_setting_flag == MD_LTG_FLAG){
    		meta_md_support[active_id] = modem_ltg;
    	} else if(md_setting_flag & MD_SGLTE_FLAG){
    		meta_md_support[active_id] = modem_sglte;
    	}
	    CCCI_INF_COM_MSG("META MD type:%d\n", meta_md_support[active_id]);
		break;
	}
	return 0;	
}

#ifdef CONFIG_OF
static int __init early_init_dt_get_chosen(unsigned long node, const char *uname, int depth, void *data)
{
	if (depth != 1 ||
	    (strcmp(uname, "chosen") != 0 && strcmp(uname, "chosen@0") != 0))
		return 0;

  return node;
}
#endif

void __init ccci_md_mem_reserve(void)
{
	int reserved_size = 0;
	phys_addr_t ptr = 0;
#ifdef CONFIG_OF
	struct tag *tags;
	int node, dfo_nr;

	node = of_scan_flat_dt(early_init_dt_get_chosen, NULL);
	tags = of_get_flat_dt_prop(node, "atag,mdinfo", NULL);
	if (tags) {
        CCCI_INF_MSG(-1, CORE, "Get MD info from META\n");
        CCCI_INF_MSG(-1, CORE, "md_inf[0]=%d\n", tags->u.mdinfo_data.md_type[0]);
        CCCI_INF_MSG(-1, CORE, "md_inf[1]=%d\n", tags->u.mdinfo_data.md_type[1]);
        CCCI_INF_MSG(-1, CORE, "md_inf[2]=%d\n", tags->u.mdinfo_data.md_type[2]);
        CCCI_INF_MSG(-1, CORE, "md_inf[3]=%d\n", tags->u.mdinfo_data.md_type[3]);
		if ((get_boot_mode()==META_BOOT) || (get_boot_mode()==ADVMETA_BOOT)) {
			CCCI_INF_MSG(-1, CORE, "(META mode) Load default dfo data...\n");
			ccci_parse_meta_md_setting(tags->u.mdinfo_data.md_type);
		}
    }
	tags = of_get_flat_dt_prop(node, "atag,dfo", NULL);
	if (tags) {
		dfo_nr = ((tags->hdr.size << 2) - sizeof(struct tag_header)) / sizeof(tag_dfo_boot);
		ccci_parse_dfo_setting(&tags->u.dfo_data, dfo_nr);
	}
#else
	ccci_parse_dfo_setting(NULL, 0);
#endif
	
	// currently only for MD1
	reserved_size = ALIGN(modem_size_list[MD_SYS1], SZ_2M);
	memblock_set_current_limit(0xFFFFFFFF);
	ptr = arm_memblock_steal(reserved_size, CCCI_MEM_ALIGN);
	memblock_set_current_limit(MEMBLOCK_ALLOC_ANYWHERE);
	if(ptr) {
		md_resv_mem_addr[MD_SYS1] = ptr;
		CCCI_INF_MSG(-1, CORE, "md1 mem reserve successfully, ptr=0x%llx, size=0x%x\n", (unsigned long long)ptr, reserved_size);
	}else{
		CCCI_INF_MSG(-1, CORE, "md1 mem reserve fail.\n");
	}
}

static void get_md_postfix(int md_id, char buf[], char buf_ex[])
{
	// name format: modem_X_YY_K_Ex.img
	int X, Ex;
	char YY_K[IMG_POSTFIX_LEN];
	unsigned int feature_val = 0;

	if (md_id<0 || md_id>MAX_MD_NUM) {
		CCCI_ERR_COM_MSG("wrong MD ID to get postfix %d\n", md_id);
		return;
	}

	// X
	X = md_id + 1;

	// YY_K
	YY_K[0] = '\0';
	switch(md_id) {
	case MD_SYS1:
		feature_val = md_support[MD_SYS1];
		break;
	case MD_SYS2:
		feature_val = md_support[MD_SYS2];
		break;
  	case MD_SYS5:
  		feature_val = md_support[MD_SYS5];
  		break;
  	default:
		CCCI_ERR_COM_MSG("request MD ID %d not supported\n", md_id);
  		break;
	}
	
	switch(feature_val) {
	case modem_2g:
		snprintf(YY_K, IMG_POSTFIX_LEN, "_2g_n");
		break;		
	case modem_3g:
		snprintf(YY_K, IMG_POSTFIX_LEN, "_3g_n");
		break;	
	case modem_wg:
		snprintf(YY_K, IMG_POSTFIX_LEN, "_wg_n");
		break;		
	case modem_tg:
		snprintf(YY_K, IMG_POSTFIX_LEN, "_tg_n");
		break;					
  	case modem_lwg:
  		snprintf(YY_K, IMG_POSTFIX_LEN, "_lwg_n");
  		break;
  	case modem_ltg:
  		snprintf(YY_K, IMG_POSTFIX_LEN, "_ltg_n");
		break;
  	case modem_sglte:
  		snprintf(YY_K, IMG_POSTFIX_LEN, "_sglte_n");
  		break;
  	default:
  		CCCI_ERR_COM_MSG("request MD type %d not supported\n", feature_val);
  		break;
	}

	// [_Ex] Get chip version
#if 0
	if(get_chip_version() == CHIP_SW_VER_01)
		Ex = 1;
	else if(get_chip_version() == CHIP_SW_VER_02)
		Ex = 2;
#else
	Ex = 1;
#endif

	// Gen post fix
	if(buf) {
		snprintf(buf, IMG_POSTFIX_LEN, "%d%s", X, YY_K);
    	CCCI_INF_COM_MSG("MD%d image postfix=%s\n", md_id, buf);
	}

	if(buf_ex) {
		snprintf(buf_ex, IMG_POSTFIX_LEN, "%d%s_E%d", X, YY_K, Ex);
    	CCCI_INF_COM_MSG("MD%d image postfix=%s\n", md_id, buf_ex);
	}
}

static struct file *open_img_file(char *name, int *sec_fp_id)
{
#ifdef ENABLE_MD_IMG_SECURITY_FEATURE
	int fp_id = OSAL_FILE_NULL;
	fp_id = osal_filp_open_read_only(name);  
	CCCI_DBG_COM_MSG("sec_open fd = (%d)!\n", fp_id); 

	if(sec_fp_id != NULL)
		*sec_fp_id = fp_id;

    CCCI_DBG_COM_MSG("sec_open file ptr = (0x%x)!\n", (unsigned int)osal_get_filp_struct(fp_id)); 

	return (struct file *)osal_get_filp_struct(fp_id);
#else
	CCCI_DBG_COM_MSG("std_open!\n");
	return filp_open(name, O_RDONLY, 0644);
#endif
}

static void close_img_file(struct file *filp_id, int sec_fp_id)
{
#ifdef ENABLE_MD_IMG_SECURITY_FEATURE
	CCCI_DBG_COM_MSG("sec_close (%d)!\n", sec_fp_id);
	osal_filp_close(sec_fp_id);
#else
	CCCI_DBG_COM_MSG("std_close!\n");
	filp_close(filp_id,current->files);
#endif
}

static int find_img_to_open(struct ccci_modem *md, MD_IMG_TYPE img_type)
{
	char img_name[3][IMG_NAME_LEN];
	char full_path[IMG_PATH_LEN];
	int i;
	char post_fix[IMG_POSTFIX_LEN];
	char post_fix_ex[IMG_POSTFIX_LEN];
	struct file *filp = NULL;

	// Gen file name
	get_md_postfix(md->index, post_fix, post_fix_ex);

	if(img_type == IMG_MD){ // Gen MD image name
		snprintf(img_name[0], IMG_NAME_LEN, "modem_%s.img", post_fix); 
		snprintf(img_name[1], IMG_NAME_LEN, "modem_%s.img", post_fix_ex);
		snprintf(img_name[2], IMG_NAME_LEN, "%s", MOEDM_IMAGE_NAME); 
	} else if (img_type == IMG_DSP) { // Gen DSP image name
		snprintf(img_name[0], IMG_NAME_LEN, "DSP_ROM_%s.img", post_fix); 
		snprintf(img_name[1], IMG_NAME_LEN, "DSP_ROM_%s.img", post_fix_ex);
		snprintf(img_name[2], IMG_NAME_LEN, "%s", DSP_IMAGE_NAME);
	} else {
		CCCI_ERR_MSG(md->index, CORE, "Invalid img type%d\n", img_type);
		return -CCCI_ERR_INVALID_PARAM;
	}

	CCCI_INF_MSG(md->index, CORE, "Find img @CIP\n");
	for(i=0; i<3; i++) {
		CCCI_INF_MSG(md->index, CORE, "try to open %s\n", img_name[i]);
		snprintf(full_path, IMG_PATH_LEN, "%s%s", CONFIG_MODEM_FIRMWARE_CIP_PATH, img_name[i]);
		filp = filp_open(full_path, O_RDONLY, 0644);
		if (IS_ERR(filp)) {
			continue;
		} else { // Open image success
			snprintf(md->img_info[img_type].file_name, IMG_PATH_LEN, full_path);
			filp_close(filp, current->files);
			if(i==0) {
				snprintf(md->post_fix, IMG_POSTFIX_LEN, "%s", post_fix);
			} else if(i==1) {
				snprintf(md->post_fix, IMG_POSTFIX_LEN, "%s", post_fix_ex);
			} else {
				md->post_fix[0] = '\0';
			}
			return 0;
		}
	}

	CCCI_INF_MSG(md->index, CORE, "Find img @default\n");
	for(i=0; i<3; i++) {
		CCCI_INF_MSG(md->index, CORE, "try to open %s\n", img_name[i]);
		snprintf(full_path, IMG_PATH_LEN, "%s%s", CONFIG_MODEM_FIRMWARE_PATH, img_name[i]);
		filp = filp_open(full_path, O_RDONLY, 0644);
		if (IS_ERR(filp)) {
			continue;
		} else { // Open image success
			snprintf(md->img_info[img_type].file_name, IMG_PATH_LEN, full_path);
			filp_close(filp, current->files);
			if(i==0) {
				snprintf(md->post_fix, IMG_POSTFIX_LEN, "%s", post_fix);
			} else if(i==1) {
				snprintf(md->post_fix, IMG_POSTFIX_LEN, "%s", post_fix_ex);
			} else {
				md->post_fix[0] = '\0';
			}
			return 0;
		}
	}
	
	md->post_fix[0] = '\0';
	CCCI_ERR_MSG(md->index, CORE,"No Image file found\n");
	return -CCCI_ERR_LOAD_IMG_NOT_FOUND;
}

static int load_std_firmware(struct ccci_modem *md, 
							 struct file *filp, 
							 struct ccci_image_info *img)
{
	void __iomem *	start;
	int				ret = 0;
	int				check_ret = 0;
	int				read_size = 0;
	mm_segment_t	curr_fs;
	phys_addr_t		load_addr;
	void *			end_addr;
	const int		size_per_read = 1024 * 1024;
	const int		size = 1024;

	curr_fs = get_fs();
	set_fs(KERNEL_DS);

	load_addr = img->address;
	filp->f_pos = img->offset;

	while (1) {
		// Map 1M memory
		start = ioremap_nocache((load_addr + read_size), size_per_read);
		CCCI_INF_MSG(md->index, CORE, "map %08x --> %08x\n", (unsigned int)(load_addr+read_size), (unsigned int)start);
		if (start <= 0) {
			CCCI_ERR_MSG(md->index, CORE, "image ioremap fail: %p\n", start);
			set_fs(curr_fs);
			return -CCCI_ERR_LOAD_IMG_NOMEM;
		}

		ret = filp->f_op->read(filp, start, size_per_read, &filp->f_pos);
		if ((ret < 0) || (ret > size_per_read) || ((ret == 0) && (read_size == 0))) { //make sure image size isn't 0
			CCCI_ERR_MSG(md->index, CORE, "image read fail: size=%d\n", ret);
			ret = -CCCI_ERR_LOAD_IMG_FILE_READ;
			goto error;
		} else if(ret == size_per_read) {
			read_size += ret;
			iounmap(start);
		} else {
			read_size += ret;
			img->size = read_size - img->tail_length; /* Note here, signatured image has file tail info. */
			CCCI_INF_MSG(md->index, CORE, "%s, image size=0x%x, read size:%d, tail:%d\n", 
							img->file_name, img->size, read_size, img->tail_length);
			iounmap(start);
			break;
		}
	}

	if(img->type == IMG_MD) {
		start = ioremap_nocache(round_down(load_addr + img->size - 0x4000, 0x4000), 
					round_up(img->size, 0x4000) - round_down(img->size - 0x4000, 0x4000)); // Make sure in one scope
		end_addr = start + img->size - round_down(img->size - 0x4000, 0x4000);
		if((check_ret = check_md_header(md, end_addr, img)) < 0) {
			ret = check_ret;
			goto error;
		}
		iounmap(start);
	} else if(img->type == IMG_DSP) {
		start = ioremap_nocache(load_addr, size);
		if((check_ret = check_dsp_header(md, start, img))<0){
			ret = check_ret;
			goto error;	
		}
		iounmap(start);
	}

	set_fs(curr_fs);
	CCCI_INF_MSG(md->index, CORE, "Load %s (size=0x%x) to 0x%llx\n", img->file_name, read_size, (unsigned long long)load_addr);
	return read_size;

error:
	iounmap(start);
	set_fs(curr_fs);
	return ret;
}


/*
 * this function should be universal to both MD image and DSP image
 */
static int load_image(struct ccci_modem *md, MD_IMG_TYPE img_type)
{
	struct file		*filp = NULL;
	int				fp_id;
	int				ret=0;
	int				offset=0;
	unsigned int	sec_tail_length = 0;
	struct ccci_image_info *img = NULL;
#ifdef ENABLE_MD_IMG_SECURITY_FEATURE
	unsigned int	img_len=0;
#endif

	if (find_img_to_open(md, img_type)<0) {
		ret = -CCCI_ERR_LOAD_IMG_FILE_OPEN;
		filp = NULL;
		goto out;
	}
	img = &(md->img_info[img_type]);
	filp = open_img_file(img->file_name, &fp_id);
	if (IS_ERR(filp)) {
		CCCI_INF_MSG(md->index, CORE, "open %s fail: %ld\n", img->file_name, PTR_ERR(filp));
		ret = -CCCI_ERR_LOAD_IMG_FILE_OPEN;
		filp = NULL;
		goto out;
	} else {
		CCCI_INF_MSG(md->index, CORE, "open %s OK\n", img->file_name);
	}

	//Begin to check header, only modem.img need check signature and cipher header
	sec_tail_length = 0;
	if(img_type == IMG_MD) {
		//step1:check if need to signature
#ifdef ENABLE_MD_IMG_SECURITY_FEATURE
		offset = signature_check_v2(md, img->file_name, &sec_tail_length);
		CCCI_INF_MSG(md->index, CORE, "signature_check offset:%d, tail:%d\n", offset, sec_tail_length);
		if (offset<0) {
			CCCI_INF_MSG(md->index, CORE, "signature_check failed ret=%d\n",offset);
			ret=offset;
			goto out;
		}
#endif
		img->offset=offset;
		img->tail_length = sec_tail_length;

		//step2:check if need to cipher
#ifdef ENABLE_MD_IMG_SECURITY_FEATURE       
		if (masp_ccci_is_cipherfmt(fp_id, offset, &img_len)) {
			CCCI_INF_MSG(md->index, CORE, "cipher image\n");
			ret=load_cipher_firmware_v2(md, fp_id, img, offset, img_len);
			if(ret<0) {
				CCCI_INF_MSG(md->index, CORE, "load_cipher_firmware failed:ret=%d!\n",ret);
				goto out;
			}
			CCCI_INF_MSG(md->index, CORE, "load_cipher_firmware done! (=%d)\n",ret);
		} else {
#endif
			CCCI_INF_MSG(md->index, CORE, "Not cipher image\n");
			ret=load_std_firmware(md, filp, img);
			if(ret<0) {
   				CCCI_INF_MSG(md->index, CORE, "load_firmware failed: ret=%d!\n",ret);
				goto out;
    		}
#ifdef ENABLE_MD_IMG_SECURITY_FEATURE           
		}
#endif        
	} else if(img_type == IMG_DSP) {
		//dsp image check signature during uboot, and ccci not need check for dsp.
		ret=load_std_firmware(md, filp, img);
		if(ret<0) {
   			CCCI_INF_MSG(md->index, CORE, "load_firmware for %s failed:ret=%d!\n",img->file_name,ret);
			goto out;
    	}
	}

out:
	if(filp != NULL){ 
		close_img_file(filp, fp_id);
	}
	return ret;
}

int ccci_load_firmware(struct ccci_modem *md, MD_IMG_TYPE img_type, char img_err_str[IMG_ERR_STR_LEN])
{
	int ret = 0;
	struct ccci_image_info *img_ptr = &md->img_info[img_type];
	char *img_str;

	img_str = md_img_info_str[md->index];

	//step1: clear modem protection when start to load firmware
	ccci_clear_md_region_protection(md);

	//step2: load image
	if((ret=load_image(md, img_type)) < 0){
		// if load_image failed, md->img_info won't have valid file name
		CCCI_INF_MSG(md->index, CORE, "fail to load firmware!\n");
	}

	/* Construct image information string */
	sprintf(img_str, "MD:%s*%s*%s*%s*%s\nAP:%s*%s*%08x (MD)%08x\n",
			img_ptr->img_info.image_type, img_ptr->img_info.platform, 
			img_ptr->img_info.build_ver, img_ptr->img_info.build_time,
			img_ptr->img_info.product_ver, img_ptr->ap_info.image_type,
			img_ptr->ap_info.platform, img_ptr->ap_info.mem_size,
			img_ptr->img_info.mem_size);

	// Prepare error string if needed
	if(img_err_str != NULL) {
		if(ret == -CCCI_ERR_LOAD_IMG_SIGN_FAIL) {
			snprintf(img_err_str, IMG_ERR_STR_LEN, "%s Signature check fail\n", img_ptr->file_name);
			CCCI_INF_MSG(md->index, CORE, "signature check fail!\n");
		} else if(ret == -CCCI_ERR_LOAD_IMG_CIPHER_FAIL) {
			snprintf(img_err_str, IMG_ERR_STR_LEN, "%s Cipher chekc fail\n", img_ptr->file_name);
			CCCI_INF_MSG(md->index, CORE, "cipher check fail!\n");
		} else if(ret == -CCCI_ERR_LOAD_IMG_FILE_OPEN) {
			snprintf(img_err_str, IMG_ERR_STR_LEN, "Modem image not exist\n");
		} else if( ret == -CCCI_ERR_LOAD_IMG_MD_CHECK) {
			snprintf(img_err_str, IMG_ERR_STR_LEN, "Modem mismatch to AP\n");
		}
	}
	if(ret < 0)
		ret = -CCCI_ERR_LOAD_IMG_LOAD_FIRM;
	return ret;
}
EXPORT_SYMBOL(ccci_load_firmware);

/*
 * most of this file is copied from mtk_ccci_helper.c, we use this function to
 * translate legacy data structure into current CCCI core.
 */
void ccci_config_modem(struct ccci_modem *md)
{
	// setup config
	md->config.load_type = md_support[md->index];
	if(md_usage_case & (1<<md->index))
		md->config.setting |= MD_SETTING_ENABLE;
	else
		md->config.setting &= ~MD_SETTING_ENABLE;
	
	// setup memory layout
	// MD image
	md->mem_layout.md_region_phy = md_resv_mem_addr[md->index];
	md->mem_layout.md_region_size = md_resv_mem_size[md->index];
	md->mem_layout.md_region_vir = ioremap_nocache(md->mem_layout.md_region_phy, MD_IMG_DUMP_SIZE); // do not remap whole region, consume too much vmalloc space 
	// DSP image
	md->mem_layout.dsp_region_phy = 0;
	md->mem_layout.dsp_region_size = 0;
	md->mem_layout.dsp_region_vir = 0;
	// Share memory
	md->mem_layout.smem_region_phy = md_resv_smem_addr[md->index];
	md->mem_layout.smem_region_size = md_resv_smem_size[md->index];
	md->mem_layout.smem_region_vir = ioremap_nocache(md->mem_layout.smem_region_phy, md->mem_layout.smem_region_size);
	memset(md->mem_layout.smem_region_vir, 0, md->mem_layout.smem_region_size);

	// exception dump region
	md->smem_layout.ccci_exp_smem_base_phy = md->mem_layout.smem_region_phy;
	md->smem_layout.ccci_exp_smem_base_vir = md->mem_layout.smem_region_vir;
	md->smem_layout.ccci_exp_smem_size = CCCI_SMEM_SIZE_EXCEPTION;
	// exception record start address
	md->smem_layout.ccci_exp_rec_base_vir = md->mem_layout.smem_region_vir+CCCI_SMEM_OFFSET_EXREC;
	
	// updae image info
	md->img_info[IMG_MD].type = IMG_MD;
	md->img_info[IMG_MD].address = md->mem_layout.md_region_phy;
	md->img_info[IMG_DSP].type = IMG_DSP;
	md->img_info[IMG_DSP].address = md->mem_layout.dsp_region_phy;

	if(md->config.setting&MD_SETTING_ENABLE)
		ccci_set_mem_remap(md, md_resv_smem_addr[md->index]-md_resv_smem_base, 
			ALIGN(md_resv_mem_addr[md->index]+modem_size_list[md->index], 0x2000000));
}

void ccci_reload_md_type(struct ccci_modem *md, int type)
{
	if(type != md->config.load_type) {
		md_support[md->index] = type;
		md->config.load_type = type;
		md->config.setting |= MD_SETTING_RELOAD;
	}
}

char *ccci_get_md_info_str(struct ccci_modem *md)
{
	return md_img_info_str[md->index];
}

struct ccci_setting* ccci_get_common_setting(int md_id)
{
#ifdef EVDO_DT_SUPPORT
	unsigned char*	str_slot1_mode = MTK_TELEPHONY_BOOTUP_MODE_SLOT1;
	unsigned char*	str_slot2_mode = MTK_TELEPHONY_BOOTUP_MODE_SLOT2;
	ccci_cfg_setting.slot1_mode = str_slot1_mode[0] - '0';
	ccci_cfg_setting.slot2_mode = str_slot2_mode[0] - '0';
#endif
    return &ccci_cfg_setting;
}

int ccci_store_sim_switch_mode(struct ccci_modem *md, int simmode)
{
    if (ccci_cfg_setting.sim_mode!= simmode) {
        ccci_cfg_setting.sim_mode = simmode;
        ccci_send_virtual_md_msg(md, CCCI_MONITOR_CH, CCCI_MD_MSG_CFG_UPDATE, 1);
    } else {
        CCCI_INF_MSG(md->index, CORE, "same sim mode as last time(0x%x)\n", simmode);
    }
    return 0;
}

int ccci_get_sim_switch_mode(void)
{
   return ccci_cfg_setting.sim_mode;
}

int ccci_init_security(struct ccci_modem *md)
{
	int ret = 0;
#ifdef ENABLE_MD_IMG_SECURITY_FEATURE
	static int security_init = 0; // for multi-modem support
	if(security_init)
		return ret;
	security_init = 1;
	
	if ((ret = masp_boot_init()) !=0) {
		CCCI_ERR_COM_MSG("masp_boot_init fail: %d\n", ret);
		ret= -EIO;
	}

	if(sec_lib_version_check()!= 0) {
		CCCI_ERR_COM_MSG("sec lib version check error\n");
		ret= -EIO;
	}
	CCCI_INF_MSG(md->index, CORE, "security is on!\n");
#else
	CCCI_INF_MSG(md->index, CORE, "security is off!\n");
#endif
	return ret;
}
EXPORT_SYMBOL(ccci_init_security);

int ccci_subsys_dfo_init(void)
{
	CCCI_INF_MSG(-1, CORE, "CCCI version: 0x%X", CCCI_DRIVER_VER);
	memset(&ccci_cfg_setting, 0xFF, sizeof(struct ccci_setting));
	collect_md_mem_setting();
	ccci_get_platform_version(ap_platform);
	return 0;
}
