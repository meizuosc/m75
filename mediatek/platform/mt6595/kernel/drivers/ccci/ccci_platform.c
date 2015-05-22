#include <ccci.h>
#include <mach/mt6575_emi_mpu.h>
#include <linux/delay.h>


int *dsp_img_vir;
int *md_img_vir;
unsigned long mdconfig_base;
unsigned long mdif_base;
unsigned long ccif_base;
int first;
char image_buf[256] = ""; 
int  valid_image_info = 0;

struct image_info *pImg_info;
static CCCI_REGION_LAYOUT ccci_layout;
static MD_CHECK_HEADER head;
static GFH_CHECK_CFG_v1 gfh_check_head;

static char * product_str[] = {[INVALID_VARSION]=INVALID_STR, 
							   [DEBUG_VERSION]=DEBUG_STR, 
							   [RELEASE_VERSION]=RELEASE_STR};

static char * type_str[] = {[AP_IMG_INVALID]=VER_INVALID_STR, 
							[AP_IMG_2G]=VER_2G_STR, 
							[AP_IMG_3G]=VER_3G_STR};


// For 75 reset
unsigned int modem_infra_sys;
unsigned int md_rgu_base;
unsigned int ap_infra_sys_base;
unsigned int md_ccif_base;

void start_emi_mpu_protect(void)
{
  /*Start protect MD/DSP region*/
  CCCI_MSG_INF("ctl", "MPU Start protect MD(%d)/DSP(%d) region...\n",
  														pImg_info[MD_INDEX].size,pImg_info[DSP_INDEX].size);

  emi_mpu_set_region_protection(ccci_layout.modem_region_base, ccci_layout.modem_region_base + /*MD_IMG_REGION_LEN*/pImg_info[MD_INDEX].size, 
                                                          0, 
                                                          SET_ACCESS_PERMISSON(NO_PRETECTION, NO_PRETECTION, SEC_R_NSEC_R));
                                                          
#if 0 
  /*Start protect MD region*/
  emi_mpu_set_region_protection(ccci_layout.dsp_region_base, ccci_layout.dsp_region_base + 0x100000, 
                                                          1, 
                                                          SET_ACCESS_PERMISSON(SEC_R_NSEC_R, SEC_R_NSEC_R, SEC_R_NSEC_R));
  emi_mpu_set_region_protection(ccci_layout.dsp_region_base+0x100000, ccci_layout.dsp_region_base + ccci_layout.dsp_region_len, 
                                                          2, 
                                                          SET_ACCESS_PERMISSON(SEC_R_NSEC_R, NO_PRETECTION, SEC_R_NSEC_R));
#endif

  /*Start protect AP region*/
  CCCI_MSG_INF("ctl", "MPU Start protect AP region...\n");

  emi_mpu_set_region_protection(KERNEL_REGION_BASE, ccci_smem_phy, 
                                                          3,
                                                          SET_ACCESS_PERMISSON(SEC_R_NSEC_R, SEC_R_NSEC_R, NO_PRETECTION));

//#ifndef MTK_DSPIRDBG
//  emi_mpu_set_region_protection(ccci_smem_phy + 0x4000-1, ccci_smem_phy + ccci_smem_size -1, 
//                                                          CCCI_MPU_REGION,
//                                                          SET_ACCESS_PERMISSON(NO_PRETECTION, SEC_R_NSEC_R, NO_PRETECTION));
//#endif                                                          
// 
  emi_mpu_set_region_protection(ccci_smem_phy + ccci_smem_size + EMI_MPU_ALIGNMENT,  CONFIG_MAX_DRAM_SIZE_SUPPORT,
                                                          5,
                                                          SET_ACCESS_PERMISSON(SEC_R_NSEC_R, SEC_R_NSEC_R, NO_PRETECTION));
  CCCI_MSG_INF("ctl", "MPU Start MM MAU...\n");

  start_mm_mau_protect(ccci_layout.modem_region_base, 
  					   (ccci_layout.modem_region_base + ccci_layout.modem_region_len + ccci_layout.dsp_region_len), 
  					    0);
  start_mm_mau_protect(ccci_smem_phy + MAU_NO_M4U_ALIGNMENT, ccci_smem_phy + ccci_smem_size, 1);

  //add MAU protect for multi-media
  start_mm_mau_protect(CONFIG_MAX_DRAM_SIZE_SUPPORT, 0xFFFFFFFF, 2);
}


void dump_firmware_info(void)
{
	
	CCCI_MSG_INF("ctl", "\n");
	CCCI_MSG_INF("ctl", "****************dump dsp&modem firmware info***************\n");
	
	CCCI_MSG_INF("ctl", "(AP)[Type]%s, [Plat]%s\n", pImg_info[MD_INDEX].ap_info.image_type, 
			pImg_info[MD_INDEX].ap_info.platform);

	CCCI_MSG_INF("ctl", "(MD)[Type]%s, [Plat]%s\n", pImg_info[MD_INDEX].img_info.image_type, 
		pImg_info[MD_INDEX].img_info.platform);

	CCCI_MSG_INF("ctl", "(DSP)[Type]%s, [Plat]%s\n", pImg_info[DSP_INDEX].img_info.image_type, 
		pImg_info[DSP_INDEX].img_info.platform);
	
	CCCI_MSG_INF("ctl", "(MD)[Build_Ver]%s, [Build_time]%s, [Product_ver]%s\n",
		pImg_info[MD_INDEX].img_info.build_ver, pImg_info[MD_INDEX].img_info.build_time,
		pImg_info[MD_INDEX].img_info.product_ver);
	
	CCCI_MSG_INF("ctl", "(DSP)[Build_Ver]%s, [Build_time]%s, [Product_ver]%s\n",
		pImg_info[DSP_INDEX].img_info.build_ver, pImg_info[DSP_INDEX].img_info.build_time,
		pImg_info[DSP_INDEX].img_info.product_ver);
	
	CCCI_MSG_INF("ctl", "-----------------dump dsp&modem firmware info---------------\r\n");

        return;
}


//check dsp header structure
//return value: 0, no dsp header; >0, dsp header check ok; <0, dsp header check fail
static int check_dsp_header(struct file *filp,struct image_info *image, unsigned int addr)
{
	int ret = 0;
	char *start_addr = (char *)addr;
	GFH_HEADER *gfh_head = (GFH_HEADER *)addr;
	GFH_FILE_INFO_v1 *gfh_file_head;
	//GFH_CHECK_CFG_v1 gfh_check_head;
	unsigned int dsp_ver = DSP_VER_INVALID;
	unsigned int ap_ver = DSP_VER_INVALID;
	char *ap_platform = "";

	bool file_info_check = false;
	bool header_check = false;
	bool ver_check = false;
	bool image_check = false;
	bool platform_check = false;
	
	if (gfh_head == NULL) {
		CCCI_MSG_INF("ctl", "ioremap DSP image failed!\n");
		ret = -ENOMEM;
		goto out;
	}

	while((gfh_head->m_magic_ver & 0xFFFFFF) == GFH_HEADER_MAGIC_NO) {
		if(gfh_head->m_type == GFH_FILE_INFO_TYPE) {
			gfh_file_head = (GFH_FILE_INFO_v1 *)gfh_head;
			file_info_check = true;

			//check image type: DSP_ROM or DSP_BL
			if (gfh_file_head->m_file_type == DSP_ROM_TYPE) {
				image_check = true;
			}
		}
		else if(gfh_head->m_type == GFH_CHECK_HEADER_TYPE) {
			gfh_check_head = *(GFH_CHECK_CFG_v1 *)gfh_head;
			header_check = true;

			//check image version: 2G or 3G
			if(gfh_check_head.m_image_type == get_ap_img_ver())
				ver_check = true;
			image->ap_info.image_type = type_str[get_ap_img_ver()];
        	image->img_info.image_type = type_str[gfh_check_head.m_image_type];

			//get dsp product version: debug or release
			image->img_info.product_ver = product_str[gfh_check_head.m_product_ver];
		
			//check chip version: MT6573_S02 or MT6575_S01
			ap_platform = ccci_get_platform_ver();
			if(!strcmp(gfh_check_head.m_platform_id, ap_platform)) {
				platform_check = true;
			}	
			image->img_info.platform = gfh_check_head.m_platform_id;
			image->ap_info.platform = ap_platform;

			//get build version and build time
			image->img_info.build_ver = gfh_check_head.m_project_id;	
			image->img_info.build_time = gfh_check_head.m_build_time;
		}

		start_addr += gfh_head->m_size;
		gfh_head = (GFH_HEADER *)start_addr;
	}

	CCCI_MSG_INF("ctl", "\n");
	CCCI_MSG_INF("ctl", "**********************DSP image check****************************\n");
	if(!file_info_check && !header_check) {
		CCCI_MSG_INF("ctl", "GFH_FILE_INFO header and GFH_CHECK_HEADER not exist!\n");
		CCCI_MSG_INF("ctl", "[Reason]No DSP_ROM, please check this image!\n");
		ret = -E_DSP_CHECK;
	}
	else if(file_info_check && !header_check) {
		CCCI_MSG_INF("ctl", "GFH_CHECK_HEADER not exist!\n");

		//check the image version from file_info structure
		dsp_ver = (gfh_file_head->m_file_ver >> DSP_2G_BIT)& 0x1;
		dsp_ver = dsp_ver? AP_IMG_2G:AP_IMG_3G;
		ap_ver = get_ap_img_ver();

		if(dsp_ver == ap_ver)
			ver_check = true;

		image->ap_info.image_type = type_str[ap_ver];        	
		image->img_info.image_type = type_str[dsp_ver];
			
		if(image_check && ver_check) {	
			CCCI_MSG_INF("ctl", "GFH_FILE_INFO header check OK!\n");
		}
		else {
			CCCI_MSG_INF("ctl", "[Error]GFH_FILE_INFO check fail!\n");
			if(!image_check)
				CCCI_MSG_INF("ctl", "[Reason]not DSP_ROM image, please check this image!\n");

			if(!ver_check)
				CCCI_MSG_INF("ctl", "[Reason]DSP type(2G/3G) mis-match to AP!\n");	
				
			ret = -E_DSP_CHECK;
		}
		
		CCCI_MSG_INF("ctl", "(DSP)[type]=%s\n",(image_check?DSP_ROM_STR:DSP_BL_STR));
		CCCI_MSG_INF("ctl", "(DSP)[ver]=%s, (AP)[ver]=%s\n",image->img_info.image_type,
				image->ap_info.image_type);
	}
	else if(!file_info_check && header_check) {		
		CCCI_MSG_INF("ctl", "GFH_FILE_INFO header not exist!\n");
		
		if(ver_check && platform_check) {
			CCCI_MSG_INF("ctl", "GFH_CHECK_HEADER header check OK!\n");	
		}
		else {
			CCCI_MSG_INF("ctl", "[Error]GFH_CHECK_HEADER check fail!\n");
			
			if(!ver_check)
				CCCI_MSG_INF("ctl", "[Reason]DSP type(2G/3G) mis-match to AP!\n");	

			if(!platform_check)
				CCCI_MSG_INF("ctl", "[Reason]DSP platform version mis-match to AP!\n");
			
			ret = -E_DSP_CHECK;
		}
		CCCI_MSG_INF("ctl", "(DSP)[ver]=%s, (AP)[ver]=%s\n",image->img_info.image_type, 
			image->ap_info.image_type);
		CCCI_MSG_INF("ctl", "(DSP)[plat]=%s, (AP)[plat]=%s\n",image->img_info.platform, 
			image->ap_info.platform);
		CCCI_MSG_INF("ctl", "(DSP)[build_Ver]=%s, [build_time]=%s\n", image->img_info.build_ver , 
			image->img_info.build_time);	
		CCCI_MSG_INF("ctl", "(DSP)[product_ver]=%s\n", product_str[gfh_check_head.m_product_ver]);
		
	}
	else {
		if(image_check && ver_check && platform_check) {
			CCCI_MSG_INF("ctl", "GFH_FILE_INFO header and GFH_CHECK_HEADER check OK!\n");	
		}
		else {
			CCCI_MSG_INF("ctl", "[Error]DSP header check fail!\n");
			if(!image_check)
				CCCI_MSG_INF("ctl", "[Reason]No DSP_ROM, please check this image!\n");

			if(!ver_check)
				CCCI_MSG_INF("ctl", "[Reason]DSP type(2G/3G) mis-match to AP!\n");

			if(!platform_check)
				CCCI_MSG_INF("ctl", "[Reason]DSP platform version mis-match to AP!\n");
	
			ret = -E_DSP_CHECK;
		}
		CCCI_MSG_INF("ctl", "(DSP)[type]=%s\n",(image_check?DSP_ROM_STR:DSP_BL_STR));
		CCCI_MSG_INF("ctl", "(DSP)[ver]=%s, (AP)[ver]=%s\n",image->img_info.image_type, 
			image->ap_info.image_type);
		CCCI_MSG_INF("ctl", "(DSP)[plat]=%s, (AP)[plat]=%s\n",image->img_info.platform, 
			image->ap_info.platform);
		CCCI_MSG_INF("ctl", "(DSP)[build_Ver]=%s, [build_time]=%s\n", image->img_info.build_ver , 
			image->img_info.build_time);	
		CCCI_MSG_INF("ctl", "(DSP)[product_ver]=%s\n", product_str[gfh_check_head.m_product_ver]);
		
	}
	CCCI_MSG_INF("ctl", "**********************DSP image check****************************\r\n");

out:
	return ret;
	
}


static int check_md_header(struct file *filp, struct image_info *image, unsigned int addr)
{
	int ret;
	//MD_CHECK_HEADER head;
	bool md_type_check = false;
	bool md_plat_check = false;
	char *ap_platform;

	head = *(MD_CHECK_HEADER *)(addr - sizeof(MD_CHECK_HEADER));

	CCCI_MSG_INF("ctl", "\n");
	CCCI_MSG_INF("ctl", "**********************MD image check***************************\n");
	ret = strncmp(head.check_header, MD_HEADER_MAGIC_NO, 12);
	if(ret) {
		CCCI_MSG_INF("ctl", "md check header not exist!\n");
		ret = 0;
	}
	else {
		if(head.header_verno != MD_HEADER_VER_NO) {
			CCCI_MSG_INF("ctl", "[Error]md check header version mis-match to AP:[%d]!\n", 
				head.header_verno);
		}
		else {
			if((head.image_type != AP_IMG_INVALID) && (head.image_type == get_ap_img_ver())) {
				md_type_check = true;
			}

			ap_platform = ccci_get_platform_ver();
			if(!strcmp(head.platform, ap_platform)) {
				md_plat_check = true;
			}

			image->ap_info.image_type = type_str[get_ap_img_ver()];
			image->img_info.image_type = type_str[head.image_type];
			image->ap_info.platform = ap_platform;
			image->img_info.platform = head.platform;
			image->img_info.build_time = head.build_time;
			image->img_info.build_ver = head.build_ver;
			image->img_info.product_ver = product_str[head.product_ver];
			
			if(md_type_check && md_plat_check) {
			
				CCCI_MSG_INF("ctl", "Modem header check OK!\n");
			}
			else {
				CCCI_MSG_INF("ctl", "[Error]Modem header check fail!\n");
				if(!md_type_check)
					CCCI_MSG_INF("ctl", "[Reason]MD type(2G/3G) mis-match to AP!\n");
		
				if(!md_plat_check)
					CCCI_MSG_INF("ctl", "[Reason]MD platform mis-match to AP!\n");
		
				ret = -E_MD_CHECK;
			}
			
			CCCI_MSG_INF("ctl", "(MD)[type]=%s, (AP)[type]=%s\n",image->img_info.image_type, image->ap_info.image_type);
			CCCI_MSG_INF("ctl", "(MD)[plat]=%s, (AP)[plat]=%s\n",image->img_info.platform, image->ap_info.platform);
			CCCI_MSG_INF("ctl", "(MD)[build_ver]=%s, [build_time]=%s\n",image->img_info.build_ver, image->img_info.build_time);
			CCCI_MSG_INF("ctl", "(MD)[product_ver]=%s\n",image->img_info.product_ver);

		}
	}
	CCCI_MSG_INF("ctl", "**********************MD image check***************************\r\n");

out:
	return ret;
}


static int load_cipher_firmware(struct file *filp,struct image_info *img,CIPHER_HEADER *header)
{
	int ret;
	void *addr = ioremap_nocache(img->address,header->image_length);
	void *o_addr = addr;
	int len = header->image_length;
	loff_t offset = header->image_offset;
	char buff[CIPHER_BLOCK_SIZE];
	unsigned long start;

	if (addr==NULL) {
		CCCI_MSG_INF("ctl", "ioremap image fialed!\n");
		ret = -E_NO_ADDR;
		goto out;
	}
	
	if (header->cipher_offset) {
		ret = kernel_read(filp,offset,addr,header->cipher_offset);
		if (ret != header->cipher_offset) {
			CCCI_MSG_INF("ctl", "kernel_read cipher_offset failed:ret=%d!\n",ret);
			ret = -E_KERN_READ;
			goto unmap_out;
		}
		offset +=header->cipher_offset;
		addr +=header->cipher_offset;
		len -=header->cipher_offset;
	}
	
	if (header->cipher_length) {
		loff_t end=offset+((header->cipher_length+CIPHER_BLOCK_SIZE-1)/CIPHER_BLOCK_SIZE)*CIPHER_BLOCK_SIZE;	
		ret = sec_aes_init();	
		if (ret) {
			CCCI_MSG_INF("ctl", "sec_aes_init fialed ret=%d\n",ret);
			ret = -E_CIPHER_FAIL;
			goto unmap_out;
		}
		
		start=jiffies;
		while (offset < end) { 
			ret=kernel_read(filp,offset,buff,CIPHER_BLOCK_SIZE);
			if (ret<0) {
				CCCI_MSG_INF("ctl", "kernel_read failed:ret=%d!\n",ret);
				ret = -E_KERN_READ;
				goto unmap_out;
			}
			
			if (ret!=CIPHER_BLOCK_SIZE)  
				CCCI_MSG_INF("ctl", "ret=%d offset=%lld\n",ret,offset);
			offset +=ret;
			
			ret=lib_aes_dec(buff,CIPHER_BLOCK_SIZE,addr,CIPHER_BLOCK_SIZE);	
			if (ret) {
				CCCI_MSG("<ctl> lib_aes_dec failed:ret=%d!\n",ret);
				ret = -E_CIPHER_FAIL;
				goto unmap_out;
			}
			
			addr += CIPHER_BLOCK_SIZE;
			len -= CIPHER_BLOCK_SIZE;
		}
		CCCI_MSG_INF("ctl", "decrypt consumed time: %dms tail_len:%d offset:%lld CIPHER_BLOCK_SIZE:%d\n",
			jiffies_to_msecs(jiffies-start),len<0?0:len,offset,CIPHER_BLOCK_SIZE);
	}
	
	if (len>0) {
		ret=kernel_read(filp,offset,addr,len);
		if (ret!=len) {
			CCCI_MSG_INF("ctl", "read tail len(%d) failed:ret=%d!\n",len,ret);
			ret = -E_KERN_READ;
			goto unmap_out;
		}
		offset +=len;
		addr +=len;
		len -=len;
	}
	
	ret = header->image_length - len;
	img->size = header->image_length;
	img->offset += sizeof(CIPHER_HEADER);	

	//if(!strcmp(img->file_name, MOEDM_IMAGE_PATH))
	//{
		ret=check_md_header(filp, img, ((unsigned int)addr));
	//}
	//else if(!strcmp(img->file_name, DSP_IMAGE_PATH))
	//{
	//	ret=check_dsp_header(filp, img, ((unsigned int)o_addr));
	//}
	
unmap_out:
	iounmap(o_addr);
out:
	return ret;
}


static int signature_check(struct file *filp)
{
	int ret=0;
	SEC_IMG_HEADER head;
	int sec_check;
	unsigned char key[RSA_KEY_SIZE];
        int size;
	SEC_IMG_HEADER *head_p;

	ret=kernel_read(filp,0,(char*)&head,sizeof(SEC_IMG_HEADER));
	if (ret != sizeof(SEC_IMG_HEADER)) {
		CCCI_MSG_INF("ctl", "read sec header failed: ret=%d\n",ret);
		ret = -E_KERN_READ;
		goto out;
	}
	
	sec_check=(head.magic_number==SEC_IMG_MAGIC);
	if(!sec_check) {
		if((sec_modem_auth_enabled() == 0) && (sec_schip_enabled() == 0)) {
			CCCI_MSG_INF("ctl", "mage has no sec header!\n");
			ret = 0;
			goto out;
		}
		else {
			CCCI_MSG_INF("ctl", "sec_modem_auth_enabled()=%d,sec_schip_enabled()=%d,sec_check=0\n",
				sec_modem_auth_enabled(), sec_schip_enabled());
			ret = -E_SIGN_FAIL;
			goto out;
		}
	}

		size=head.sign_length+sizeof(SEC_IMG_HEADER);
		head_p=kmalloc(size,GFP_KERNEL);	
		if (head_p==NULL) {
			CCCI_MSG_INF("ctl", "kmalloc for SEC_IMG_HEADER failed!\n");
			ret = -E_NOMEM;
			goto out;
		}

		*head_p=head;
		CCCI_MSG_INF("ctl", "custom_name:%s signature header:sign_offset(%d) sign_length(%d)\n",
			head_p->cust_name[0]?head_p->cust_name:"Unknown",head_p->sign_offset,head_p->sign_length);

		ret=kernel_read(filp,sizeof(SEC_IMG_HEADER)+head_p->sign_offset,
			(char*)(head_p+1),head_p->sign_length);
		if (ret!=head_p->sign_length) {
			CCCI_MSG_INF("ctl", "kernel_read failed ret=%d\n",ret);
			ret = -E_KERN_READ;
			goto out_free;
		}
		
		ret=kernel_read(filp,head_p->signature_offset,(char*)key,sizeof(key));
		if (ret!=sizeof(key)) {
			CCCI_MSG_INF("ctl", "kernel_read for key failed ret=%d\n",ret);
			ret = -E_KERN_READ;
			goto out_free;
		}
		
		ret=lib_verify((unsigned char*)head_p,size,key,sizeof(key));
		if (ret) {
			CCCI_MSG_INF("ctl", "lib_verify failed ret=%d\n",ret);
			ret = -E_SIGN_FAIL;
			goto out_free;
		}
		
		ret=sizeof(SEC_IMG_HEADER);
		
	out_free:
		kfree(head_p);
		
out:
	return ret;
}



/*
 * load_firmware
 */
static int load_firmware(struct file *filp, struct image_info *img)
{
    void *start;
    int   ret = 0;
    int check_ret = 0;
    int   read_size = 0;
    mm_segment_t curr_fs;
    unsigned long load_addr;
    unsigned int end_addr;
    const int size_per_read = 1024 * 1024;
    const int size = 1024;

    curr_fs = get_fs();
    set_fs(KERNEL_DS);

    load_addr = (img->address + img->offset);
    filp->f_pos = img->offset;
	
    while (1) {
        start = ioremap_nocache((load_addr + read_size), size_per_read);
        if (start <= 0) {
		CCCI_MSG_INF("ctl", "CCCI_MD: Firmware ioremap failed:%d\n", (unsigned int)start);
		set_fs(curr_fs);
		return -E_NOMEM;
        }
		
        ret = filp->f_op->read(filp, start, size_per_read, &filp->f_pos);
        if ((ret < 0) || (ret > size_per_read)) {
		CCCI_MSG_INF("ctl", "modem image read failed=%d\n", ret);
		ret = -E_FILE_READ;
		goto error;
        }
	else if(ret == size_per_read) {
	    if(read_size == 0)
		CCCI_MSG_INF("ctl", "start VM address of %s is:0x%x\n", img->file_name, (unsigned int)start);
			
		read_size += ret;
        	iounmap(start);
	    }	
	 else {
		read_size += ret;
	    	img->size = read_size;	
		CCCI_MSG_INF("ctl", "%s image size=0x%x\n", img->file_name, read_size);
                iounmap(start);
                break;
        }
        
    }

	if(img->idx == MD_INDEX) {
		start = ioremap_nocache((read_size - size), size);
		end_addr = ((unsigned int)start + size);
		CCCI_MSG_INF("ctl", "VM address of %s is:0x%x - 0x%x\n", img->file_name, (unsigned int)start,end_addr);
		
		if((check_ret = check_md_header(filp, img, end_addr)) < 0) {
			ret = check_ret;
			goto error;
		}
		iounmap(start);
	}
	else if(img->idx == DSP_INDEX) {
		start = ioremap_nocache(load_addr, size);
		CCCI_MSG_INF("ctl", "start VM address of %s is:0x%x\n", img->file_name, (unsigned int)start);
		if((check_ret = check_dsp_header(filp, img, (unsigned int)start))<0){
			ret = check_ret;
			goto error;	
		}
		iounmap(start);
	}

	set_fs(curr_fs);
	CCCI_MSG_INF("ctl", "Load %s (size=%d) to 0x%lx\n", img->file_name, read_size, load_addr);

	return read_size;
	
error:
	iounmap(start);
	set_fs(curr_fs);
	return ret;
	
}


static int load_firmware_func(struct image_info *img)
{
    struct file *filp = NULL;
    CIPHER_HEADER cipher_header;
    int ret=0;
    int offset=0;
	
    //get modem&dsp image name with E1&E2 
    if(get_chip_version() == CHIP_E1) {
    	snprintf(img->file_name,sizeof(img->file_name),
		CONFIG_MODEM_FIRMWARE_PATH "%s",(img->idx?DSP_IMAGE_E1_NAME:MOEDM_IMAGE_E1_NAME));
		CCCI_MSG_INF("ctl", "open %s \n",img->file_name);
    }
    else if(get_chip_version() == CHIP_E2) {		
    	snprintf(img->file_name,sizeof(img->file_name),
		CONFIG_MODEM_FIRMWARE_PATH "%s",(img->idx?DSP_IMAGE_E2_NAME:MOEDM_IMAGE_E2_NAME));
		CCCI_MSG_INF("ctl", "open %s \n",img->file_name);
    }
   
    filp = filp_open(img->file_name, O_RDONLY, 0777);
    if (IS_ERR(filp)) {
        CCCI_MSG_INF("ctl","open %s fail(%ld), try to open modem.img/DSP_ROM\n",
			img->file_name, PTR_ERR(filp));
		goto open_file;
    }
    else
		goto check_head;

open_file:
    //get default modem&dsp image name (modem.img & DSP_ROM)
    snprintf(img->file_name,sizeof(img->file_name),
			CONFIG_MODEM_FIRMWARE_PATH "%s",(img->idx?DSP_IMAGE_NAME:MOEDM_IMAGE_NAME));	
	
    CCCI_MSG_INF("ctl", "open %s \n",img->file_name);
    filp = filp_open(img->file_name, O_RDONLY, 0777);
    if (IS_ERR(filp)) {
        CCCI_MSG_INF("ctl", "open %s failed:%ld\n",img->file_name, PTR_ERR(filp));
        ret = -E_FILE_OPEN;
		filp = NULL;
		goto out;
    }

check_head:
    //only modem.img need check signature and cipher header
    if(img->idx == MD_INDEX) {
	//step1:check if need to signature
	offset=signature_check(filp);
        if (offset<0) {
		CCCI_MSG_INF("ctl", "signature_check failed ret=%d\n",offset);
		ret=offset;
		goto out;
        }
	
        img->offset=offset;

	//step2:check if need to cipher
	ret=kernel_read(filp,offset,(char*)&cipher_header,sizeof(CIPHER_HEADER));	
        if (ret!=sizeof(CIPHER_HEADER)) {
		CCCI_MSG_INF("ctl", "read cipher header failed:ret=%d!\n",ret);
		ret = -E_KERN_READ;
		goto out;
    }
	
    if (cipher_header.magic_number==CIPHER_IMG_MAGIC) {
		cipher_header.image_offset +=offset;
		CCCI_MSG_INF("ctl", "img_len:%d img_offset:%d cipher_len:%d cipher_offset:%d cust_name:%s.\n",
		cipher_header.image_length,cipher_header.image_offset,cipher_header.cipher_length,
		cipher_header.cipher_offset,cipher_header.cust_name[0]?(char*)cipher_header.cust_name:"Unknow");
	
		ret=load_cipher_firmware(filp,img,&cipher_header);
		if(ret<0) {
   			CCCI_MSG_INF("ctl", "load_cipher_firmware failed:ret=%d!\n",ret);
			goto out;
    	}

    }
   	else {
		ret=load_firmware(filp,img);
		if(ret<0) {
   			CCCI_MSG_INF("ctl", "load_firmware failed:ret=%d!\n",ret);
			goto out;
    	}
   	}
	}
	//dsp image check signature during uboot, and ccci not need check for dsp.
	else if(img->idx == DSP_INDEX) {
		ret=load_firmware(filp,img);
		if(ret<0) {
   			CCCI_MSG_INF("ctl", "load_firmware for %s failed:ret=%d!\n",img->file_name,ret);
			goto out;
    	}
	}

out:
	if(filp != NULL) 
	    filp_close(filp,current->files);

	return ret;
}

enum {
	MD_DEBUG_REL_INFO_NOT_READY = 0,
	MD_IS_DEBUG_VERSION,
	MD_IS_RELEASE_VERSION
};
static int modem_is_debug_ver = MD_DEBUG_REL_INFO_NOT_READY;
static void store_modem_dbg_rel_info(char *str)
{
	if( NULL == str)
		modem_is_debug_ver = MD_DEBUG_REL_INFO_NOT_READY;
	else if(strcmp(str, "Debug") == 0)
		modem_is_debug_ver = MD_IS_DEBUG_VERSION;
	else
		modem_is_debug_ver = MD_IS_RELEASE_VERSION;
}
int is_modem_debug_ver(void)
{
	return modem_is_debug_ver;
}

int  ccci_load_firmware(struct image_info *img_info)
{
    int i;
    int ret = 0;
    int ret_a[IMG_CNT] = {0,0};
    char err_buf[128] = "";
	
    //step1:get modem&dsp image info
    pImg_info = kzalloc((2*sizeof(struct image_info)),GFP_KERNEL);
    if (pImg_info == NULL) {
	CCCI_MSG_INF("ctl", "kmalloc for image_info structure failed!\n");
	ret = -E_NOMEM;
	goto out;
    }	

    pImg_info[MD_INDEX].idx = MD_INDEX;
    pImg_info[DSP_INDEX].idx = DSP_INDEX;
    pImg_info[MD_INDEX].address = ccci_layout.modem_region_base;
    pImg_info[MD_INDEX].offset = pImg_info[DSP_INDEX].offset = 0;
    pImg_info[DSP_INDEX].address = ccci_layout.dsp_region_base;
    pImg_info[MD_INDEX].load_firmware = load_firmware_func;
    pImg_info[DSP_INDEX].load_firmware = load_firmware_func;
	
    	
 	//step2: load image
	//clear modem protection when start to load firmware
	CCCI_CTL_MSG("Clear region protect...\n");
    emi_mpu_set_region_protection(0, 0, 0, SET_ACCESS_PERMISSON(NO_PRETECTION, NO_PRETECTION, NO_PRETECTION));
    emi_mpu_set_region_protection(0, 0, 1, SET_ACCESS_PERMISSON(NO_PRETECTION, NO_PRETECTION, NO_PRETECTION));
    emi_mpu_set_region_protection(0, 0, 2, SET_ACCESS_PERMISSON(NO_PRETECTION, NO_PRETECTION, NO_PRETECTION));
    emi_mpu_set_region_protection(0, 0, 3, SET_ACCESS_PERMISSON(NO_PRETECTION, NO_PRETECTION, NO_PRETECTION));
    emi_mpu_set_region_protection(0, 0, 5, SET_ACCESS_PERMISSON(NO_PRETECTION, NO_PRETECTION, NO_PRETECTION));
    

	for(i = 0; i < IMG_CNT; i++) {
		if (pImg_info[i].load_firmware) {
			if((ret_a[i]= pImg_info[i].load_firmware(&pImg_info[i])) < 0){
				CCCI_MSG_INF("ctl", "load firmware fail for %s!\n", pImg_info[i].file_name);
			}
		}
		else {
			CCCI_MSG_INF("ctl", "load null firmware for %s!\n", pImg_info[i].file_name);
			ret_a[i] = -E_FIRM_NULL; 
    	}
	}

	//need protect after load firmeware is completed
	//start_emi_mpu_protect();

out:
	/* Construct image information string */
	if(ret_a[DSP_INDEX] == -E_FILE_OPEN)	
		sprintf(image_buf, "DSP:%s*%s*%s*%s\nMD:%s*%s*%s*%s\nAP:%s*%s (DSP)%s (MD)%s\n",	
			pImg_info[DSP_INDEX].img_info.image_type,pImg_info[DSP_INDEX].img_info.platform,
			pImg_info[DSP_INDEX].img_info.build_ver,pImg_info[DSP_INDEX].img_info.build_time,
			pImg_info[MD_INDEX].img_info.image_type,pImg_info[MD_INDEX].img_info.platform, 
			pImg_info[MD_INDEX].img_info.build_ver,pImg_info[MD_INDEX].img_info.build_time,
			pImg_info[MD_INDEX].ap_info.image_type,pImg_info[MD_INDEX].ap_info.platform,
			pImg_info[DSP_INDEX].img_info.product_ver,pImg_info[MD_INDEX].img_info.product_ver);
	else
		sprintf(image_buf, "DSP:%s*%s*%s*%s\nMD:%s*%s*%s*%s\nAP:%s*%s (DSP)%s (MD)%s\n",	
			pImg_info[DSP_INDEX].img_info.image_type,pImg_info[DSP_INDEX].img_info.platform,
			pImg_info[DSP_INDEX].img_info.build_ver,pImg_info[DSP_INDEX].img_info.build_time,
			pImg_info[MD_INDEX].img_info.image_type,pImg_info[MD_INDEX].img_info.platform, 
			pImg_info[MD_INDEX].img_info.build_ver,pImg_info[MD_INDEX].img_info.build_time,
			pImg_info[DSP_INDEX].ap_info.image_type,pImg_info[DSP_INDEX].ap_info.platform,
			pImg_info[DSP_INDEX].img_info.product_ver,pImg_info[MD_INDEX].img_info.product_ver);

	valid_image_info = 1;
	store_modem_dbg_rel_info(pImg_info[MD_INDEX].img_info.product_ver);

	if(ret_a[MD_INDEX] == -E_SIGN_FAIL) {
		sprintf(err_buf, "%s Signature check fail\n", pImg_info[i].file_name);
		CCCI_MSG_INF("ctl", "signature check fail!\n");
		ccci_aed(0, err_buf);
	}
	else if(ret_a[MD_INDEX] == -E_CIPHER_FAIL) {
		sprintf(err_buf, "%s Cipher chekc fail\n", pImg_info[i].file_name);
		CCCI_MSG_INF("ctl", "cipher check fail!\n");
		ccci_aed(0, err_buf);
	}
	else if((ret_a[DSP_INDEX] == -E_FILE_OPEN) || (ret_a[MD_INDEX] == -E_FILE_OPEN)) {
		ccci_aed(0, "[ASSERT] Modem/DSP image not exist\n");
	}
	else if((ret_a[DSP_INDEX] == -E_DSP_CHECK)&&(ret_a[MD_INDEX] != -E_MD_CHECK)) {
		ccci_aed(0, "[ASSERT] DSP mismatch to AP\n");
	}
	else if((ret_a[DSP_INDEX] != -E_DSP_CHECK)&&(ret_a[MD_INDEX] == -E_MD_CHECK)) {
		ccci_aed(0, "[ASSERT] Modem mismatch to AP\n\n");
	}
	else if((ret_a[DSP_INDEX] == -E_DSP_CHECK)&&(ret_a[MD_INDEX] == -E_MD_CHECK)) {
		ccci_aed(0, "[ASSERT] Modem&DSP mismatch to AP\n\n");
	}
	
	if((ret_a[MD_INDEX] < 0) || (ret_a[DSP_INDEX] < 0))
		ret = -E_LOAD_FIRM;
	
	return ret;	
}


static void dsp_debug(void *data __always_unused)
{
	my_mem_dump(dsp_img_vir,DSP_IMG_DUMP_SIZE,my_default_end,
		"\n\n[CCCI_MD_IMG][%p][%dBytes]:",dsp_img_vir,DSP_IMG_DUMP_SIZE);
	
}

static irqreturn_t md_dsp_wdt_isr(int irq,void *data __always_unused)
{

	DEBUG_INFO_T debug_info;
        memset(&debug_info,0,sizeof(DEBUG_INFO_T));
	debug_info.type=-1;
	switch (irq)
	{
		case MT6575_MDWDT_IRQ_LINE:
			if(md_rgu_base) {
				CCCI_MSG_INF("ctl", "MD_WDT_STA=%04x.\n",WDT_MD_STA(md_rgu_base));
				//*MD_WDT_CTL =(0x22<<8);
				WDT_MD_STA(md_rgu_base) = WDT_MD_MODE_KEY;
			}
			debug_info.name="MD wdt timeout";
			break;
		case MT6575_DSPWDT_IRQ_LINE:
			if(md_rgu_base) {
				CCCI_MSG_INF("ctl", "DSP_WDT_STA=%04x.\n",WDT_DSP_STA(md_rgu_base));
				//*DSP_WDT_CTL =(0x22<<8);
				WDT_DSP_STA(md_rgu_base) = WDT_DSP_MODE_KEY;
			}
			debug_info.name="DSP wdt timeout";
			debug_info.platform_call=dsp_debug;
			break;
	}
	
	CCCI_MSG("<ctl> irq=%d\n",irq);
//	ccci_dump_debug_info(&debug_info);
	reset_md();

	return IRQ_HANDLED;
}
 
static int __init md_dsp_irq_init(void)
{
	int ret;
	
	mt65xx_irq_set_sens(MT6575_MDWDT_IRQ_LINE, MT65xx_EDGE_SENSITIVE);
	ret=request_irq(MT6575_MDWDT_IRQ_LINE,md_dsp_wdt_isr,0,"MD-WDT",NULL);
	if (ret) {
		CCCI_MSG_INF("ctl", "Failed for MT6575_MDWDT_IRQ_LINE(%d).\n",ret);
		return ret;
	}
	//mt65xx_irq_unmask(MT6575_MDWDT_IRQ_LINE);
	enable_irq(MT6575_MDWDT_IRQ_LINE);
	mt65xx_irq_set_sens(MT6575_DSPWDT_IRQ_LINE, MT65xx_EDGE_SENSITIVE);
	ret=request_irq(MT6575_DSPWDT_IRQ_LINE,md_dsp_wdt_isr,0,"DSP-WDT",NULL);
	if (ret) {
		CCCI_MSG_INF("ctl", "Failed for MT6575_DSPWDT_IRQ_LINE(%d).\n",ret);
		free_irq(MT6575_MDWDT_IRQ_LINE,NULL);
		return ret;
	}
	//mt65xx_irq_unmask(MT6575_DSPWDT_IRQ_LINE);
	enable_irq(MT6575_DSPWDT_IRQ_LINE);
	return 0;
}

static void md_dsp_irq_deinit(void)
{
	free_irq(MT6575_MDWDT_IRQ_LINE,NULL);
	//mt65xx_irq_mask(MT6575_MDWDT_IRQ_LINE);
	disable_irq(MT6575_MDWDT_IRQ_LINE);

	free_irq(MT6575_DSPWDT_IRQ_LINE,NULL);
	//mt65xx_irq_mask(MT6575_DSPWDT_IRQ_LINE);
	disable_irq(MT6575_DSPWDT_IRQ_LINE);

}


static void map_md_side_register(void)
{
	modem_infra_sys = ioremap_nocache(MD_INFRA_BASE, 0x1000);
	md_rgu_base = ioremap_nocache(0xD10C0000, 0x1000);
	ap_infra_sys_base = INFRA_SYS_CFG_BASE;
	md_ccif_base = 0xF1020000; // MD CCIF Bas;
}

void ungate_md(void)
{
	CCCI_MSG_INF("rst", "ungate_md\n");		
	
	if ( (!modem_infra_sys)||(!md_rgu_base) ) {		
		CCCI_MSG_INF("rst", "fail map md ifrasys and md rgu base!\n");		
		//return -ENOMEM;	
		return;
	}

	/* AP MCU release MD_MCU reset via AP_MD_RGU_SW_MDMCU_RSTN */
	if(WDT_MD_MCU_RSTN(md_rgu_base) & 0x8000) {
		CCCI_MSG("<rst> AP MCU release MD_MCU reset via AP_MD_RGU_SW_MDMCU_RSTN\n");
		WDT_MD_MCU_RSTN(md_rgu_base) = 0x37;
	}

	/* Setting MD & DSP to its default status */
	WDT_MD_LENGTH(md_rgu_base) = WDT_MD_LENGTH_DEFAULT|WDT_MD_LENGTH_KEY;
	WDT_MD_RESTART(md_rgu_base) = WDT_MD_RESTART_KEY;
	WDT_MD_MODE(md_rgu_base) = WDT_MD_MODE_DEFAULT|WDT_MD_MODE_KEY;

	CCCI_CTL_MSG("md_infra_base <%d>, jumpaddr_val <%x>\n",
		modem_infra_sys, *((unsigned int*)(modem_infra_sys + BOOT_JUMP_ADDR)));	
	 
	*((unsigned int*)(modem_infra_sys + BOOT_JUMP_ADDR)) = 0x00000000;  

	//*(unsigned int*)(modem_infra_sys + CLK_SW_CON2) = 0x0155;  
	//*(unsigned int*)(modem_infra_sys + CLK_SW_CON0) = 0x1557;  
	//*(unsigned int*)(modem_infra_sys + CLK_SW_CON2) = 0x0555;  
}

void gate_md(void)
{
	void *p=ioremap(0xd2000800,4);
	if(0 == md_rgu_base){
		CCCI_MSG_INF("ctl", "<rst> md_rgu_base map fail\n");
		return;
	}
	if(0 == ap_infra_sys_base){
		CCCI_MSG_INF("ctl", "<rst> ap_infra_sys_base map fail\n");
		return;
	}
	if(0 == md_ccif_base){
		CCCI_MSG_INF("ctl", "<rst> md_ccif_base map fail\n");
		return;
	}
	/* Disable MD & DSP WDT */
	WDT_MD_MODE(md_rgu_base) = WDT_MD_MODE_KEY;
	WDT_DSP_MODE(md_rgu_base) = WDT_DSP_MODE_KEY;

	/* AP MCU block MDSYS's AXI masters in APCONFIG */
	CCCI_CTL_MSG("<rst> AP MCU block MDSYS's AXI masters in APCONFIG\n");
	INFRA_TOP_AXI_PROTECT_EN(ap_infra_sys_base) |= (0xf<<5);
	dsb();

	/* AP MCU polling MDSYS AXI master IDLE */
	CCCI_CTL_MSG("<rst> AP MCU polling MDSYS AXI master IDLE\n");
	while( (INFRA_TOP_AXI_PROTECT_STA(ap_infra_sys_base)&(0xf<<5)) != (0xf<<5) )
		yield();

	/* Block every bus access form AP to MD */
	CCCI_CTL_MSG("<rst> Block every bus access form AP to MD\n");
	INFRA_TOP_AXI_SI4_CTL(ap_infra_sys_base) &= ~(0x1<<7);
	dsb();

	/* AP MCU assert MD SYS watchdog SW reset in AP_RGU */
	CCCI_CTL_MSG("<rst> AP MCU assert MD SYS watchdog SW reset in AP_RGU\n");
	*(volatile unsigned int*)(AP_RGU_BASE+0x18) = (1<<2)|(0x15<<8);
	dsb();

	/* AP MCU release MD SYS's AXI masters */
	CCCI_CTL_MSG("<rst> AP MCU release MD SYS's AXI masters\n");
	INFRA_TOP_AXI_PROTECT_EN(ap_infra_sys_base) &= ~(0xf<<5);
	dsb();

	CCCI_CTL_MSG("<rst> Wait 0.5s.\n");	
	schedule_timeout_interruptible(HZ/2);

	/* Release MDSYS SW reset in AP RGU */
	CCCI_CTL_MSG("<rst> AP MCU release MD SYS SW reset\n");
	*(volatile unsigned int*)(AP_RGU_BASE+0x18) = (0x15<<8);

	/* AP MCU release AXI bus slave way enable in AP Config */
	CCCI_CTL_MSG("<rst> AP MCU release AXI bus slave way enable in AP Config\n");
	INFRA_TOP_AXI_SI4_CTL(ap_infra_sys_base) |= (0x1<<7);
	dsb();

	/* AP MCU release MD_MCU reset via AP_MD_RGU_SW_MDMCU_RSTN */
	CCCI_CTL_MSG("<rst> AP MCU release MD_MCU reset via AP_MD_RGU_SW_MDMCU_RSTN\n");
	WDT_MD_MCU_RSTN(md_rgu_base) = 0x37;
	dsb();

	/* Tell DSP Reset occour */
	CCCI_CTL_MSG("<rst> AP MCU write FRST to notify DSP\n");
	if (p){
		*((volatile unsigned int *)p)= 0x46525354;   //*((unsigned int*)"FRST"); 
		iounmap(p);
	}else
		CCCI_MSG_INF("ctl", "<rst> remap failed for 0x85000800(0xd2000800)\n");

	/* AP MCU release DSP_MCU reset via AP_MD_RGU_SW_MDMCU_RSTN */
	CCCI_CTL_MSG("<rst> AP MCU release DSP_MCU reset via AP_MD_RGU_SW_DSPMCU_RSTN\n");
	WDT_DSP_MCU_RSTN(md_rgu_base) = 0x48;
	dsb();

	/* Write MD CCIF Ack to clear AP CCIF busy register */
	CCCI_CTL_MSG("<rst> Write MD CCIF Ack to clear AP CCIF busy register\n");
	MD_CCIF_ACK(md_ccif_base)=~0U;
	first=0;
}

int __init platform_init(void)
{
	int ret=0;

	ccci_get_region_layout(&ccci_layout);
	CCCI_CTL_MSG("mdconfig=0x%lx, ccif=0x%lx, mdif=0x%lx,dsp=0x%lx\n", ccci_layout.mdcfg_region_base,
		ccci_layout.ccif_region_base, ccci_layout.mdif_region_base,ccci_layout.dsp_region_base);

	CCCI_CTL_MSG("mdconfig_len=0x%lx, ccif_len=0x%lx, mdif_len=0x%lx,dsp_len=0x%lx\n", 
		ccci_layout.mdcfg_region_len,ccci_layout.ccif_region_len, ccci_layout.mdif_region_len,ccci_layout.dsp_region_len);

	md_img_vir = (int *)ioremap_nocache(ccci_layout.modem_region_base, MD_IMG_DUMP_SIZE);
	if (!md_img_vir)
	{
		CCCI_MSG_INF("ctl", "MD region ioremap fail!\n");
		return -ENOMEM;
	}

	dsp_img_vir=(int *)ioremap_nocache(ccci_layout.dsp_region_base, DSP_IMG_DUMP_SIZE);
	if (!dsp_img_vir) {
		CCCI_MSG_INF("ctl", "DSP region ioremap fail!\n");
		ret= -ENOMEM;
		goto err_out4;
	}
	CCCI_MSG_INF("ctl", "md_img_vir=0x%lx, dsp_img_vir=0x%lx\n",md_img_vir, dsp_img_vir);

	// FIXME: just a temp solution
	map_md_side_register();

	mdconfig_base=(unsigned long)ioremap_nocache(ccci_layout.mdcfg_region_base, ccci_layout.mdcfg_region_len);
	if (!mdconfig_base) {
		CCCI_MSG_INF("ctl", "mdconfig region ioremap fail!\n");
		ret=-ENOMEM;
		goto err_out3;
	}

	ccif_base=(unsigned long)ioremap_nocache(ccci_layout.ccif_region_base, ccci_layout.ccif_region_len);
	if (!ccif_base) {
        	CCCI_MSG_INF("ctl", "ccif region ioremap fail!\n");
		ret=-ENOMEM;
		goto err_out2;
	}
	
	mdif_base=(unsigned long)ioremap_nocache(ccci_layout.mdif_region_base, ccci_layout.mdif_region_len);
	if (!mdif_base) {
		CCCI_MSG_INF("ctl", "mdif region ioremap fail!\n");
		ret=-ENOMEM;
		goto err_out1;
	}
	CCCI_CTL_MSG("mdconfig_vir=0x%lx, ccif_vir=0x%lx, mdif_vir=0x%lx\n", 
		mdconfig_base, ccif_base, mdif_base);
	
	if ((ret=ccci_rpc_init()) !=0)
		goto err_out0;
	
	if ((ret=md_dsp_irq_init()) != 0) 
		goto err_out;

	if ((ret=sec_boot_init()) !=0) {
		CCCI_MSG_INF("ctl", "sec_boot_init failed ret=%d\n",ret);
		ret= -EIO;
		goto err_out_isr;
	}	
	
	goto out;
	
err_out_isr:
	md_dsp_irq_deinit();
err_out:
	ccci_rpc_exit();
err_out0:
	iounmap((void*)mdif_base);
err_out1:	
	iounmap((void*)ccif_base);	
err_out2:
	iounmap((void*)mdconfig_base);
err_out3:
	iounmap((void*)dsp_img_vir);
err_out4:
	iounmap((void*)md_img_vir);
out:
	return ret;
}

void __exit platform_deinit(void)
{
	iounmap((void*)ccif_base);	
	iounmap((void*)mdconfig_base);
	iounmap((void*)dsp_img_vir);
	iounmap((void*)md_img_vir);
	md_dsp_irq_deinit();
	ccci_rpc_exit();	

}
