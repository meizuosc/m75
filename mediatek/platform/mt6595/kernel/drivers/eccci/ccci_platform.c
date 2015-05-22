#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/module.h>
#include <mach/emi_mpu.h>
#include <mach/sync_write.h>
#include <mach/memory.h>
#include <mach/upmu_sw.h>
#include "ccci_core.h"
#include "ccci_debug.h"
#include "ccci_platform.h"

#ifdef ENABLE_DRAM_API
extern unsigned int get_max_DRAM_size (void);
extern unsigned int get_phys_offset (void);
#endif

#define MPU_REGION_ID_MD0_ROM 4
#define MPU_REGION_ID_MD0_RAM 5
#define MPU_REGION_ID_MD0_SRM 6
#define MPU_REGION_ID_AP 7

void ccci_clear_md_region_protection(struct ccci_modem *md)
{
#ifdef ENABLE_EMI_PROTECTION
	unsigned int rom_mem_mpu_id, rw_mem_mpu_id;

	CCCI_INF_MSG(md->index, CORE, "Clear MD region protect...\n");
	switch(md->index) {
	case MD_SYS1:
		rom_mem_mpu_id = MPU_REGION_ID_MD0_ROM;
		rw_mem_mpu_id = MPU_REGION_ID_MD0_RAM;
		break;
		
	default:
		CCCI_INF_MSG(md->index, CORE, "[error]MD ID invalid when clear MPU protect\n");
		return;
	}
	
	CCCI_INF_MSG(md->index, CORE, "Clear MPU protect MD ROM region<%d>\n", rom_mem_mpu_id);
	emi_mpu_set_region_protection(0,	  				/*START_ADDR*/
								  0,      				/*END_ADDR*/
								  rom_mem_mpu_id,       /*region*/
								  SET_ACCESS_PERMISSON(NO_PROTECTION, NO_PROTECTION, NO_PROTECTION, NO_PROTECTION));

	CCCI_INF_MSG(md->index, CORE, "Clear MPU protect MD R/W region<%d>\n", rw_mem_mpu_id);
	emi_mpu_set_region_protection(0,		  			/*START_ADDR*/
								  0,       				/*END_ADDR*/
								  rw_mem_mpu_id,        /*region*/
								  SET_ACCESS_PERMISSON(NO_PROTECTION, NO_PROTECTION, NO_PROTECTION, NO_PROTECTION));
#endif
}

/*
 * for some unkonw reason on 6582 and 6572, MD will read AP's memory during boot up, so we
 * set AP region as MD read-only at first, and re-set it to portected after MD boot up.
 * this function should be called right before sending runtime data.
 */
void ccci_set_ap_region_protection(struct ccci_modem *md)
{
#ifdef ENABLE_EMI_PROTECTION
	unsigned int ap_mem_mpu_id, ap_mem_mpu_attr;
	unsigned int kernel_base;
	unsigned int dram_size;

	if(enable_4G())
		kernel_base = 0;
	else
		kernel_base = get_phys_offset();
#ifdef ENABLE_DRAM_API
	dram_size = get_max_DRAM_size();
#else
	dram_size = 256*1024*1024;
#endif
	ap_mem_mpu_id = MPU_REGION_ID_AP;
	ap_mem_mpu_attr = SET_ACCESS_PERMISSON(NO_PROTECTION, FORBIDDEN, FORBIDDEN, NO_PROTECTION);

	CCCI_INF_MSG(md->index, CORE, "MPU Start protect AP region<%d:%08x:%08x> %x\n",
								ap_mem_mpu_id, kernel_base, (kernel_base+dram_size-1), ap_mem_mpu_attr); 
	emi_mpu_set_region_protection(kernel_base,
									(kernel_base+dram_size-1),
									 ap_mem_mpu_id,
									 ap_mem_mpu_attr);
#endif
}
EXPORT_SYMBOL(ccci_set_ap_region_protection);

void ccci_set_mem_access_protection(struct ccci_modem *md)
{
#ifdef ENABLE_EMI_PROTECTION
	unsigned int shr_mem_phy_start, shr_mem_phy_end, shr_mem_mpu_id, shr_mem_mpu_attr;
	unsigned int rom_mem_phy_start, rom_mem_phy_end, rom_mem_mpu_id, rom_mem_mpu_attr;
	unsigned int rw_mem_phy_start, rw_mem_phy_end, rw_mem_mpu_id, rw_mem_mpu_attr;
	unsigned int ap_mem_mpu_id, ap_mem_mpu_attr;
	struct ccci_image_info *img_info;
	struct ccci_mem_layout *md_layout;
	unsigned int kernel_base;
	unsigned int dram_size;

	// For MT6595
	//===================================================================
	//            | Region |  D0(AP)    |  D1(MD0)   |  D2(MD32)  |  D3(MM)
	//------------+------------------------------------------------------
	// Secure OS  |    0   |RW(S)       |Forbidden   |Forbidden   |Forbidden
	//------------+------------------------------------------------------
	// MD32 Code  |    1   |Forbidden   |Forbidden   |RO(S/NS)    |Forbidden
	//------------+------------------------------------------------------
	// MD32 Share |    2   |No protect  |Forbidden   |No protect  |Forbidden
	//------------+------------------------------------------------------
	// MD0 s-secure |   3   |secure R/W  |No protect  |Forbidden  |Forbidden
	//------------+------------------------------------------------------
	// MD0 ROM    |    4   |RO(S/NS)    |RO(S/NS)    |Forbidden   |Forbidden
	//------------+------------------------------------------------------
	// MD0 R/W+   |    5   |Forbidden   |No protect  |Forbidden   |Forbidden
	//------------+------------------------------------------------------
	// MD0 Share  |    6   |No protect  |No protect  |Forbidden   |Forbidden
	//------------+------------------------------------------------------
	// AP         |    7  |No protect  |Forbidden   |Forbidden   |No protect
	//===================================================================

	switch(md->index) {
	case MD_SYS1:
		img_info = &md->img_info[IMG_MD];
		md_layout = &md->mem_layout;
		rom_mem_mpu_id = MPU_REGION_ID_MD0_ROM;
		rw_mem_mpu_id = MPU_REGION_ID_MD0_RAM;
		shr_mem_mpu_id = MPU_REGION_ID_MD0_SRM;
		rom_mem_mpu_attr = SET_ACCESS_PERMISSON(FORBIDDEN, FORBIDDEN, SEC_R_NSEC_R, SEC_R_NSEC_R);
		rw_mem_mpu_attr = SET_ACCESS_PERMISSON(FORBIDDEN, FORBIDDEN, NO_PROTECTION, FORBIDDEN);
		shr_mem_mpu_attr = SET_ACCESS_PERMISSON(FORBIDDEN, FORBIDDEN, NO_PROTECTION, NO_PROTECTION);			
		break;

	default:
		CCCI_ERR_MSG(md->index, CORE, "[error]invalid when MPU protect\n");
		return;
	}

	if(enable_4G())
		kernel_base = 0;
	else
		kernel_base = get_phys_offset();
#ifdef ENABLE_DRAM_API
	dram_size = get_max_DRAM_size();
#else
	dram_size = 256*1024*1024;
#endif
	ap_mem_mpu_id = MPU_REGION_ID_AP;
	ap_mem_mpu_attr = SET_ACCESS_PERMISSON(NO_PROTECTION, FORBIDDEN, SEC_R_NSEC_R, NO_PROTECTION);

	/*
	 * if set start=0x0, end=0x10000, the actural protected area will be 0x0-0x1FFFF,
	 * here we use 64KB align, MPU actually request 32KB align since MT6582, but this works...
	 * we assume emi_mpu_set_region_protection will round end address down to 64KB align.
	 */
	rom_mem_phy_start = (unsigned int)md_layout->md_region_phy;
	rom_mem_phy_end   = ((rom_mem_phy_start + img_info->size + 0xFFFF)&(~0xFFFF)) - 0x1;
	rw_mem_phy_start  = rom_mem_phy_end + 0x1;
	rw_mem_phy_end	  = rom_mem_phy_start + md_layout->md_region_size - 0x1;
	shr_mem_phy_start = (unsigned int)md_layout->smem_region_phy;
	shr_mem_phy_end   = ((shr_mem_phy_start + md_layout->smem_region_size + 0xFFFF)&(~0xFFFF)) - 0x1;
	
	CCCI_INF_MSG(md->index, CORE, "MPU Start protect MD ROM region<%d:%08x:%08x> %x\n", 
                              	rom_mem_mpu_id, rom_mem_phy_start, rom_mem_phy_end, rom_mem_mpu_attr);
	emi_mpu_set_region_protection(rom_mem_phy_start,	  /*START_ADDR*/
									rom_mem_phy_end,      /*END_ADDR*/
									rom_mem_mpu_id,       /*region*/
									rom_mem_mpu_attr);

	CCCI_INF_MSG(md->index, CORE, "MPU Start protect MD R/W region<%d:%08x:%08x> %x\n", 
                              	rw_mem_mpu_id, rw_mem_phy_start, rw_mem_phy_end, rw_mem_mpu_attr);
	emi_mpu_set_region_protection(rw_mem_phy_start,		  /*START_ADDR*/
									rw_mem_phy_end,       /*END_ADDR*/
									rw_mem_mpu_id,        /*region*/
									rw_mem_mpu_attr);

	CCCI_INF_MSG(md->index, CORE, "MPU Start protect MD Share region<%d:%08x:%08x> %x\n", 
                              	shr_mem_mpu_id, shr_mem_phy_start, shr_mem_phy_end, shr_mem_mpu_attr);
	emi_mpu_set_region_protection(shr_mem_phy_start,	  /*START_ADDR*/
									shr_mem_phy_end,      /*END_ADDR*/
									shr_mem_mpu_id,       /*region*/
									shr_mem_mpu_attr);

	CCCI_INF_MSG(md->index, CORE, "MPU Start protect AP region<%d:%08x:%08x> %x\n",
								ap_mem_mpu_id, kernel_base, (kernel_base+dram_size-1), ap_mem_mpu_attr); 
	emi_mpu_set_region_protection(kernel_base,
								  (kernel_base+dram_size-1),
								  ap_mem_mpu_id,
								  ap_mem_mpu_attr);
#endif
}
EXPORT_SYMBOL(ccci_set_mem_access_protection);

int set_ap_smem_remap(struct ccci_modem *md, phys_addr_t src, phys_addr_t des)
{
	unsigned int remap1_val = 0;
	unsigned int remap2_val = 0;
	static int	smem_remapped = 0;
	
	if(!smem_remapped) {
		smem_remapped = 1;
		remap1_val =(((des>>24)|0x1)&0xFF)
				  + (((INVALID_ADDR>>16)|1<<8)&0xFF00)
				  + (((INVALID_ADDR>>8)|1<<16)&0xFF0000)
				  + (((INVALID_ADDR>>0)|1<<24)&0xFF000000);
		
		remap2_val =(((INVALID_ADDR>>24)|0x1)&0xFF)
				  + (((INVALID_ADDR>>16)|1<<8)&0xFF00)
				  + (((INVALID_ADDR>>8)|1<<16)&0xFF0000)
				  + (((INVALID_ADDR>>0)|1<<24)&0xFF000000);
		
		CCCI_INF_MSG(md->index, CORE, "AP Smem remap: [%llx]->[%llx](%08x:%08x)\n", (unsigned long long)des, (unsigned long long)src, remap1_val, remap2_val);

#ifdef 	ENABLE_MEM_REMAP_HW
		mt_reg_sync_writel(remap1_val, AP_BANK4_MAP0);
		mt_reg_sync_writel(remap2_val, AP_BANK4_MAP1);
		mt_reg_sync_writel(remap2_val, AP_BANK4_MAP1); // HW bug, write twice to activate setting
#endif					
	}
	return 0;
}


int set_md_smem_remap(struct ccci_modem *md, phys_addr_t src, phys_addr_t des, phys_addr_t invalid)
{
	unsigned int remap1_val = 0;
	unsigned int remap2_val = 0;
	if(enable_4G()) {
		des &= 0xFFFFFFFF;
	} else {
		des -= KERN_EMI_BASE;
	}
	
	switch(md->index) {
	case MD_SYS1:
		remap1_val =(((des>>24)|0x1)&0xFF)
				  + ((((invalid+0x2000000*0)>>16)|1<<8)&0xFF00)
				  + ((((invalid+0x2000000*1)>>8)|1<<16)&0xFF0000)
				  + ((((invalid+0x2000000*2)>>0)|1<<24)&0xFF000000);
		remap2_val =((((invalid+0x2000000*3)>>24)|0x1)&0xFF)
				  + ((((invalid+0x2000000*4)>>16)|1<<8)&0xFF00)
				  + ((((invalid+0x2000000*5)>>8)|1<<16)&0xFF0000)
				  + ((((invalid+0x2000000*6)>>0)|1<<24)&0xFF000000);
		
#ifdef 	ENABLE_MEM_REMAP_HW
        mt_reg_sync_writel(remap1_val, MD1_BANK4_MAP0);
        mt_reg_sync_writel(remap2_val, MD1_BANK4_MAP1);
#endif
		break;

	default:
		break;
	}

	CCCI_INF_MSG(md->index, CORE, "MD Smem remap:[%llx]->[%llx](%08x:%08x)\n", (unsigned long long)des, (unsigned long long)src, remap1_val, remap2_val);
	return 0;
}


int set_md_rom_rw_mem_remap(struct ccci_modem *md, phys_addr_t src, phys_addr_t des, phys_addr_t invalid)
{
	unsigned int remap1_val = 0;
	unsigned int remap2_val = 0;
	if(enable_4G()) {
		des &= 0xFFFFFFFF;
	} else {
		des -= KERN_EMI_BASE;
	}
	
	switch(md->index) {
	case MD_SYS1:
		remap1_val =(((des>>24)|0x1)&0xFF)
				  + ((((des+0x2000000*1)>>16)|1<<8)&0xFF00)
				  + ((((des+0x2000000*2)>>8)|1<<16)&0xFF0000)
				  + ((((invalid+0x02000000*7)>>0)|1<<24)&0xFF000000);
		remap2_val =((((invalid+0x02000000*8)>>24)|0x1)&0xFF)
				  + ((((invalid+0x02000000*9)>>16)|1<<8)&0xFF00)
				  + ((((invalid+0x02000000*10)>>8)|1<<16)&0xFF0000)
				  + ((((invalid+0x02000000*11)>>0)|1<<24)&0xFF000000);
		
#ifdef 	ENABLE_MEM_REMAP_HW
        mt_reg_sync_writel(remap1_val, MD1_BANK0_MAP0);
        mt_reg_sync_writel(remap2_val, MD1_BANK0_MAP1);
#endif
		break;
		
	default:
		break;
	}

	CCCI_INF_MSG(md->index, CORE, "MD ROM mem remap:[%llx]->[%llx](%08x:%08x)\n", (unsigned long long)des, (unsigned long long)src, remap1_val, remap2_val);
	return 0;
}

void ccci_set_mem_remap(struct ccci_modem *md, unsigned long smem_offset, phys_addr_t invalid)
{
	unsigned long remainder;
	if(enable_4G()) {
		invalid &= 0xFFFFFFFF;
		CCCI_INF_MSG(md->index, CORE, "4GB mode enabled, invalid_map=%llx\n", (unsigned long long)invalid);
	} else {
		invalid -= KERN_EMI_BASE;
		CCCI_INF_MSG(md->index, CORE, "4GB mode disabled, invalid_map=%llx\n", (unsigned long long)invalid);
	}
	
	// Set share memory remapping
#if 0 // no hardware AP remap after MT6592
	set_ap_smem_remap(md, 0x40000000, md->mem_layout.smem_region_phy_before_map);
	md->mem_layout.smem_region_phy = smem_offset + 0x40000000;
#endif
	/*
	 * always remap only the 1 slot where share memory locates. smem_offset is the offset between
	 * ROM start address(32M align) and share memory start address.
	 * (AP view smem address) - [(smem_region_phy) - (bank4 start address) - (un-32M-align space)]
	 * = (MD view smem address)
	 */
	remainder = smem_offset % 0x02000000;
	md->mem_layout.smem_offset_AP_to_MD = md->mem_layout.smem_region_phy - (remainder + 0x40000000);
	set_md_smem_remap(md, 0x40000000, md->mem_layout.md_region_phy + (smem_offset-remainder), invalid); 
	CCCI_INF_MSG(md->index, CORE, "AP to MD share memory offset 0x%X", md->mem_layout.smem_offset_AP_to_MD);

	// Set md image and rw runtime memory remapping
	set_md_rom_rw_mem_remap(md, 0x00000000, md->mem_layout.md_region_phy, invalid);
}

/*
 * when MD attached its codeviser for debuging, this bit will be set. so CCCI should disable some 
 * checkings and operations as MD may not respond to us.
 */
unsigned int ccci_get_md_debug_mode(struct ccci_modem *md)
{
	unsigned int dbg_spare;
	static unsigned int debug_setting_flag = 0;
	
	// this function does NOT distinguish modem ID, may be a risk point
	if((debug_setting_flag&DBG_FLAG_JTAG) == 0) {
		dbg_spare = ioread32((void __iomem *)MD_DEBUG_MODE);
		if(dbg_spare & MD_DBG_JTAG_BIT) {
			CCCI_INF_MSG(md->index, CORE, "Jtag Debug mode(%08x)\n", dbg_spare);
			debug_setting_flag |= DBG_FLAG_JTAG;
			mt_reg_sync_writel(dbg_spare & (~MD_DBG_JTAG_BIT), MD_DEBUG_MODE);
		}
	}
	return debug_setting_flag;
}
EXPORT_SYMBOL(ccci_get_md_debug_mode);

void ccci_get_platform_version(char * ver)
{
#ifdef ENABLE_CHIP_VER_CHECK
	sprintf(ver, "MT%04x_S%02x", get_chip_hw_ver_code(), (get_chip_hw_subcode()&0xFF));
#else
	sprintf(ver, "MT6595_S00");
#endif
}

#ifdef MTK_MD_LOW_BAT_SUPPORT
static void ccci_md1_low_battery_cb(LOW_BATTERY_LEVEL level)
{
	struct ccci_modem *md = ccci_get_modem_by_id(MD_SYS1);
	if(md && md->ops->low_power_notify)
		md->ops->low_power_notify(md, LOW_BATTERY, level);
}

static void ccci_md1_battery_percent_cb(BATTERY_PERCENT_LEVEL level)
{
	struct ccci_modem *md = ccci_get_modem_by_id(MD_SYS1);
	if(md && md->ops->low_power_notify)
		md->ops->low_power_notify(md, BATTERY_PERCENT, level);
}
#endif

int ccci_platform_init(struct ccci_modem *md)
{
	switch(md->index) {
	case MD_SYS1:
#ifdef MTK_MD_LOW_BAT_SUPPORT
		register_low_battery_notify(&ccci_md1_low_battery_cb, LOW_BATTERY_PRIO_MD);
		register_battery_percent_notify(&ccci_md1_battery_percent_cb, BATTERY_PERCENT_PRIO_MD);
#endif
		break;
	default:
		break;
	};
	return 0;
}
