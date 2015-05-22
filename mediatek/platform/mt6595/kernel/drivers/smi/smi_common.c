#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/aee.h>
#include <linux/xlog.h>
#include <mach/mt_clkmgr.h>
#include <asm/io.h>

#include <linux/ioctl.h>
#include <linux/fs.h>

#include <mach/mt_smi.h>
#include "smi_reg.h"
#include "smi_common.h"
#include "smi_debug.h"

#define SMI_LOG_TAG "SMI"

#define LARB_BACKUP_REG_SIZE 128
#define SMI_COMMON_BACKUP_REG_NUM   9

#define SMIDBG(level, x...)            \
    do{                        \
    if (smi_debug_level >= (level))    \
    SMIMSG(x);            \
    } while (0)

typedef struct
{
    spinlock_t SMI_lock;
    unsigned int pu4ConcurrencyTable[SMI_BWC_SCEN_CNT];  //one bit represent one module
} SMI_struct;

static SMI_struct g_SMIInfo;

/* LARB BASE ADDRESS */
static unsigned int gLarbBaseAddr[SMI_LARB_NR] = 
    {LARB0_BASE, LARB1_BASE, LARB2_BASE, LARB3_BASE, LARB4_BASE}; 

static const unsigned int larb_port_num[SMI_LARB_NR]=
{SMI_LARB0_PORT_NUM, SMI_LARB1_PORT_NUM, SMI_LARB2_PORT_NUM, SMI_LARB3_PORT_NUM, SMI_LARB4_PORT_NUM};

static unsigned short int larb0_port_backup[SMI_LARB0_PORT_NUM];
static unsigned short int larb1_port_backup[SMI_LARB1_PORT_NUM];
static unsigned short int larb2_port_backup[SMI_LARB2_PORT_NUM];
static unsigned short int larb3_port_backup[SMI_LARB3_PORT_NUM];
static unsigned short int larb4_port_backup[SMI_LARB4_PORT_NUM];

/* SMI COMMON register list to be backuped */
static unsigned short g_smi_common_backup_reg_offset[SMI_COMMON_BACKUP_REG_NUM] = {0x200, 0x204, 0x208, 0x20c, 0x210, 0x214, 0x230, 0x234, 0x238};
static unsigned int g_smi_common_backup[SMI_COMMON_BACKUP_REG_NUM];

static unsigned char larb_vc_setting[SMI_LARB_NR] = {0, 2, 0, 1, 2};    

static unsigned short int * larb_port_backup[SMI_LARB_NR] = 
{ larb0_port_backup, larb1_port_backup, larb2_port_backup, larb3_port_backup, larb4_port_backup};

// To keep the HW's init value
static int is_default_value_saved = 0;
static unsigned int default_val_smi_l1arb[SMI_LARB_NR] = {0};

static unsigned int wifi_disp_transaction = 0;


/* debug level */
static unsigned int smi_debug_level = 0;

/* tuning mode, 1 for register ioctl */
static unsigned int smi_tuning_mode = 0;

static unsigned int smi_profile = SMI_BWC_SCEN_NORMAL;

static unsigned int* pLarbRegBackUp[SMI_LARB_NR];
static int g_bInited = 0;

char *smi_port_name[][21] = 
{
    {   /* 0 MMSYS */
        "disp_ovl0",
        "disp_rdma0",
        "disp_rdma1",
        "disp_wdma0",
        "disp_ovl1",
        "disp_rdma2",
        "disp_wdma1",
        "disp_od_r",
        "disp_od_w",
        "mdp_rdma0",
        "mdp_rdma1",
        "mdp_wdma",
        "mdp_wrot0",
        "mdp_wrot1"
    },
    {   /* 1 VDEC */
        "hw_vdec_mc_ext",
        "hw_vdec_pp_ext",
        "hw_vdec_ufo_ext",
        "hw_vdec_vld_ext",
        "hw_vdec_vld2_ext",
        "hw_vdec_avc_mv_ext",
        "hw_vdec_pred_rd_ext",
        "hw_vdec_pred_wr_ext",
        "hw_vdec_ppwrap_ext"
    },
    {   /* 2 ISP */
        "imgo",
        "rrzo",
        "aao",
        "lcso",
        "esfko",
        "imgo_d",
        "lsci",
        "lsci_d",
        "bpci",
        "bpci_d",
        "ufdi",
        "imgi",
        "img2o",
        "img3o",
        "vipi",
        "vip2i",
        "vip3i",
        "lcei",
        "rb",
        "rp",
        "wr"
    },
    {   /* 3 VENC */
        "venc_rcpu",
        "venc_rec",
        "venc_bsdma",
        "venc_sv_comv",
        "venc_rd_comv",
        "jpgenc_bsdma",
        "remdc_sdma",
        "remdc_bsdma",
        "jpgenc_rdma",
        "jpgenc_sdma",
        "jpgdec_wdma",
        "jpgdec_bsdma",
        "venc_cur_luma",
        "venc_cur_chroma",
        "venc_ref_luma",
        "venc_ref_chroma",
        "remdc_wdma",
        "venc_nbm_rdma",
        "venc_nbm_wdma"
    },    
    {   /* 4 MJC */
        "mjc_mv_rd",
        "mjc_mv_wr",
        "mjc_dma_rd",
        "mjc_dma_wr"
    }    
};

static void initSetting(void);
static void vpSetting(void);
static void vrSetting(void);

static void smi_dumpLarb(unsigned int index);
static void smi_dumpCommon(void);

// for slow motion force 30 fps
extern int primary_display_force_set_vsync_fps(unsigned int fps);
extern unsigned int primary_display_get_fps(void);

// Use this function to get base address of Larb resgister
// to support error checking
int get_larb_base_addr(int larb_id){
    if(larb_id > SMI_LARB_NR || larb_id < 0)
    {
        return SMI_ERROR_ADDR;
    }else{
        return gLarbBaseAddr[larb_id];
    }
}

static int larb_clock_on(int larb_id) 
{

#ifndef CONFIG_MTK_FPGA
    char name[30];
    sprintf(name, "smi+%d", larb_id);  

    switch(larb_id)
    {
    case 0: 
        enable_clock(MT_CG_DISP0_SMI_COMMON, name);
        enable_clock(MT_CG_DISP0_SMI_LARB0, name);
        break;
    case 1:
        enable_clock(MT_CG_DISP0_SMI_COMMON, name);
        enable_clock(MT_CG_VDEC1_LARB, name);
        break;
    case 2: 
        enable_clock(MT_CG_DISP0_SMI_COMMON, name);
        enable_clock(MT_CG_IMAGE_LARB2_SMI, name);
        break;
    case 3: 
        enable_clock(MT_CG_DISP0_SMI_COMMON, name);
        enable_clock(MT_CG_VENC_LARB, name);
        break;
    case 4: 
        enable_clock(MT_CG_DISP0_SMI_COMMON, name);
        enable_clock(MT_CG_MJC_SMI_LARB, name);
        break;
    default: 
        break;
    }
#endif /* CONFIG_MTK_FPGA */

  return 0;
}

static int larb_clock_off(int larb_id) 
{

#ifndef CONFIG_MTK_FPGA
    char name[30];
    sprintf(name, "smi+%d", larb_id);


    switch(larb_id)
    {
    case 0: 
        disable_clock(MT_CG_DISP0_SMI_LARB0, name);
        disable_clock(MT_CG_DISP0_SMI_COMMON, name);
        break;
    case 1:
        disable_clock(MT_CG_VDEC1_LARB, name);
        disable_clock(MT_CG_DISP0_SMI_COMMON, name);
        break;
    case 2: 
        disable_clock(MT_CG_IMAGE_LARB2_SMI, name);
        disable_clock(MT_CG_DISP0_SMI_COMMON, name);
        break;
    case 3: 
        disable_clock(MT_CG_VENC_LARB, name);
        disable_clock(MT_CG_DISP0_SMI_COMMON, name);
        break;
    case 4: 
        disable_clock(MT_CG_MJC_SMI_LARB, name);
        disable_clock(MT_CG_DISP0_SMI_COMMON, name);
        break;
    default: 
        break;
    }
#endif /* CONFIG_MTK_FPGA */

    return 0;
}

static void backup_smi_common(void)
{
    int i;
    
    for (i = 0; i < SMI_COMMON_BACKUP_REG_NUM; i++)
    {
        g_smi_common_backup[i] = M4U_ReadReg32(SMI_COMMON_EXT_BASE, (unsigned int)g_smi_common_backup_reg_offset[i]);
    }
}

static void restore_smi_common(void)
{
    int i;
    
    for (i = 0; i < SMI_COMMON_BACKUP_REG_NUM; i++)
    {
        M4U_WriteReg32(SMI_COMMON_EXT_BASE, (unsigned int)g_smi_common_backup_reg_offset[i], g_smi_common_backup[i]);
    }    
}


static void backup_larb_smi(int index)
{
    int port_index = 0;
    unsigned short int *backup_ptr = NULL;
    unsigned int larb_base =  gLarbBaseAddr[index];
    unsigned int larb_offset = 0x200;
    int total_port_num = 0; 

    // boundary check for larb_port_num and larb_port_backup access
    if (index < 0 || index >= SMI_LARB_NR) {
        return;
    }

    total_port_num = larb_port_num[index];
    backup_ptr = larb_port_backup[index];

    // boundary check for port value access
    if (total_port_num <= 0 || backup_ptr == NULL) {
        return;
    }

    for (port_index = 0; port_index < total_port_num; port_index++) {
        *backup_ptr = (unsigned short int)(M4U_ReadReg32(larb_base , larb_offset));
        backup_ptr++;
        larb_offset += 4;
    }
    
    /* backup smi common along with larb0, smi common clk is guaranteed to be on when processing larbs */
    if (index == 0)
    {
        backup_smi_common();    
    }
    
    return;
}   

static void restore_larb_smi(int index)
{
    int port_index = 0;
    unsigned short int *backup_ptr = NULL;
    unsigned int larb_base =  gLarbBaseAddr[index];
    unsigned int larb_offset = 0x200;
    unsigned int backup_value = 0;
    int total_port_num = 0; 

    // boundary check for larb_port_num and larb_port_backup access
    if (index < 0 || index >= SMI_LARB_NR) {
        return;
    }
    total_port_num = larb_port_num[index];
    backup_ptr = larb_port_backup[index];

    // boundary check for port value access
    if ( total_port_num <= 0 || backup_ptr == NULL) {
        return;
    }
    
    /* restore smi common along with larb0, smi common clk is guaranteed to be on when processing larbs */
    if (index == 0)
    {
        restore_smi_common();
    }
    
    for (port_index = 0; port_index < total_port_num; port_index++) {
        backup_value = *backup_ptr;
        M4U_WriteReg32(larb_base , larb_offset, backup_value);
        backup_ptr++;
        larb_offset += 4;
    }
    
    /* we do not backup 0x20 because it is a fixed setting */
    M4U_WriteReg32(larb_base, 0x20, larb_vc_setting[index]);
    
    /* turn off EMI empty OSTD dobule, fixed setting */
    M4U_WriteReg32(larb_base, 0x2c, 4);
    
    return;
}


static int larb_reg_backup(int larb)
{
    unsigned int* pReg = pLarbRegBackUp[larb];
    unsigned int larb_base = gLarbBaseAddr[larb];
    
    *(pReg++) = M4U_ReadReg32(larb_base, SMI_LARB_CON);

    // *(pReg++) = M4U_ReadReg32(larb_base, SMI_SHARE_EN);
    // *(pReg++) = M4U_ReadReg32(larb_base, SMI_ROUTE_SEL);

    backup_larb_smi(larb);

    if(0 == larb)
    {
        g_bInited = 0;
    }

    return 0;
}

static int smi_larb_init(unsigned int larb)
{
    unsigned int regval = 0;
    unsigned int regval1 = 0;
    unsigned int regval2 = 0;
    unsigned int larb_base = get_larb_base_addr(larb);

    // Clock manager enable LARB clock before call back restore already, it will be disabled after restore call back returns
    // Got to enable OSTD before engine starts
    regval = M4U_ReadReg32(larb_base , SMI_LARB_STAT);

    // TODO: FIX ME
    // regval1 = M4U_ReadReg32(larb_base , SMI_LARB_MON_BUS_REQ0);
    // regval2 = M4U_ReadReg32(larb_base , SMI_LARB_MON_BUS_REQ1);

    if(0 == regval)
    {
        SMIMSG("Init OSTD for larb_base: 0x%x\n" , larb_base);
        M4U_WriteReg32(larb_base , SMI_LARB_OSTDL_SOFT_EN , 0xffffffff);
    }
    else
    {
        SMIMSG("Larb: 0x%x is busy : 0x%x , port:0x%x,0x%x ,fail to set OSTD\n" , larb_base , regval , regval1 , regval2);
        smi_dumpDebugMsg();
        if (smi_debug_level >= 1) {
            SMIERR("DISP_MDP LARB  0x%x OSTD cannot be set:0x%x,port:0x%x,0x%x\n" , larb_base , regval , regval1 , regval2);
        } else {
            dump_stack();
        }
    }

    restore_larb_smi(larb);

    return 0;
}

int larb_reg_restore(int larb)
{
    unsigned int larb_base = SMI_ERROR_ADDR;
    unsigned int regval = 0;
    unsigned int* pReg = NULL;

    larb_base = get_larb_base_addr(larb);

    // The larb assign doesn't exist
    if(larb_base == SMI_ERROR_ADDR ){
        SMIMSG("Can't find the base address for Larb%d\n", larb);
        return 0;
    }

    pReg = pLarbRegBackUp[larb];

    SMIDBG(1, "+larb_reg_restore(), larb_idx=%d \n", larb); 
    SMIDBG(1, "m4u part restore, larb_idx=%d \n", larb);
    //warning: larb_con is controlled by set/clr
    regval = *(pReg++);
    M4U_WriteReg32(larb_base, SMI_LARB_CON_CLR, ~(regval));
    M4U_WriteReg32(larb_base, SMI_LARB_CON_SET, (regval));

    //M4U_WriteReg32(larb_base, SMI_SHARE_EN, *(pReg++) );
    //M4U_WriteReg32(larb_base, SMI_ROUTE_SEL, *(pReg++) );
    
    smi_larb_init(larb);

    return 0;
}

// callback after larb clock is enabled
void on_larb_power_on(struct larb_monitor *h, int larb_idx)
{
    //M4ULOG("on_larb_power_on(), larb_idx=%d \n", larb_idx);
    larb_reg_restore(larb_idx);
    
    return;
}
// callback before larb clock is disabled
void on_larb_power_off(struct larb_monitor *h, int larb_idx)
{
    //M4ULOG("on_larb_power_off(), larb_idx=%d \n", larb_idx);
    larb_reg_backup(larb_idx);
}

//Make sure clock is on
static void initSetting(void)
{   
    /* save default larb regs */
    if (!is_default_value_saved) {
        SMIMSG("Save default config:\n");
        default_val_smi_l1arb[0] = M4U_ReadReg32(REG_SMI_L1ARB0, 0);
        default_val_smi_l1arb[1] = M4U_ReadReg32(REG_SMI_L1ARB1, 0);
        default_val_smi_l1arb[2] = M4U_ReadReg32(REG_SMI_L1ARB2, 0);
        default_val_smi_l1arb[3] = M4U_ReadReg32(REG_SMI_L1ARB3, 0);
        default_val_smi_l1arb[4] = M4U_ReadReg32(REG_SMI_L1ARB4, 0);
        SMIMSG("l1arb[0-2]= 0x%x,  0x%x, 0x%x\n" , default_val_smi_l1arb[0], default_val_smi_l1arb[1], default_val_smi_l1arb[2]);
        SMIMSG("l1arb[3-4]= 0x%x,  0x%x\n" , default_val_smi_l1arb[3], default_val_smi_l1arb[4]);

        is_default_value_saved = 1;
    }

    // Keep the HW's init setting in REG_SMI_L1ARB0 ~ REG_SMI_L1ARB4
    M4U_WriteReg32(REG_SMI_L1ARB0 , 0 , default_val_smi_l1arb[0]);
    M4U_WriteReg32(REG_SMI_L1ARB1 , 0 , default_val_smi_l1arb[1]);
    M4U_WriteReg32(REG_SMI_L1ARB2 , 0 , default_val_smi_l1arb[2]);
    M4U_WriteReg32(REG_SMI_L1ARB3 , 0 , default_val_smi_l1arb[3]);
    M4U_WriteReg32(REG_SMI_L1ARB4 , 0 , default_val_smi_l1arb[4]);

    M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x200, 0x1b);
    // 0x220 is controlled by M4U
    // M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x220, 0x1); //disp: emi0, other:emi1    
    M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x234, (0x1 << 31) + (0x13 << 26) + (0x14 << 21) + (0x0 << 20) + (0x2 << 15) + (0x3 << 10) + (0x4 << 5) + 0x5);
    M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x238, (0x2 << 25) + (0x3 << 20) + (0x4 << 15) + (0x5 << 10) + (0x6 << 5) + 0x8);
    M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x230, 0x1f + (0x8 << 5) + (0x6 << 10));
    
    // Set VC priority: MMSYS = ISP > VENC > VDEC = MJC
    M4U_WriteReg32(LARB0_BASE, 0x20, 0x0); // MMSYS
    M4U_WriteReg32(LARB1_BASE, 0x20, 0x2); // VDEC
    M4U_WriteReg32(LARB2_BASE, 0x20, 0x0); // ISP   
    M4U_WriteReg32(LARB3_BASE, 0x20, 0x1); // VENC
    M4U_WriteReg32(LARB4_BASE, 0x20, 0x2); // MJC
    
	// turn off EMI empty double OSTD
	M4U_WriteReg32(LARB0_BASE, 0x2c, M4U_ReadReg32(LARB0_BASE, 0x2c) | (1 << 2));
	M4U_WriteReg32(LARB1_BASE, 0x2c, M4U_ReadReg32(LARB1_BASE, 0x2c) | (1 << 2));
	M4U_WriteReg32(LARB2_BASE, 0x2c, M4U_ReadReg32(LARB2_BASE, 0x2c) | (1 << 2));
	M4U_WriteReg32(LARB3_BASE, 0x2c, M4U_ReadReg32(LARB3_BASE, 0x2c) | (1 << 2));
	M4U_WriteReg32(LARB4_BASE, 0x2c, M4U_ReadReg32(LARB4_BASE, 0x2c) | (1 << 2));
}

static void vrSetting(void)
{
    // VR 4K
    M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x204, 0x1614);  //LARB0, DISP+MDP
    M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x208, 0x1000);  //LARB1, VDEC
    M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x20C, 0x11F7);  //LARB2, ISP
    M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x210, 0x1584);  //LARB3, VENC+JPG
    M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x214, 0x1000);  //LARB4, MJC

    M4U_WriteReg32(LARB0_BASE, 0x200, 0x1f); //ovl_ch0_0+ovl_ch0_1
    M4U_WriteReg32(LARB0_BASE, 0x224, 0x2); //mdp_wdma
    M4U_WriteReg32(LARB0_BASE, 0x228, 0x5); //mdp_wrot0; min(9,5)
    M4U_WriteReg32(LARB2_BASE, 0x200, 0x8); //imgo
    M4U_WriteReg32(LARB2_BASE, 0x208, 0x1); //aao
    M4U_WriteReg32(LARB2_BASE, 0x20c, 0x1); //lsco
    M4U_WriteReg32(LARB2_BASE, 0x210, 0x1); //esfko
    M4U_WriteReg32(LARB2_BASE, 0x218, 0x1); //lsci
    M4U_WriteReg32(LARB2_BASE, 0x220, 0x1); //bpci
    M4U_WriteReg32(LARB2_BASE, 0x22c, 0x4); //imgi
    M4U_WriteReg32(LARB2_BASE, 0x230, 0x1); //img2o
    M4U_WriteReg32(LARB2_BASE, 0x244, 0x1); //lcei
    M4U_WriteReg32(LARB3_BASE, 0x200, 0x1); //venc_rcpu
    M4U_WriteReg32(LARB3_BASE, 0x204, 0x4); //venc_rec_frm
    M4U_WriteReg32(LARB3_BASE, 0x208, 0x1); //venc_bsdma
    M4U_WriteReg32(LARB3_BASE, 0x20c, 0x1); //venc_sv_comv
    M4U_WriteReg32(LARB3_BASE, 0x210, 0x1); //venc_rd_comv
    M4U_WriteReg32(LARB3_BASE, 0x230, 0x8); //venc_cur_luma
    M4U_WriteReg32(LARB3_BASE, 0x234, 0x4); //venc_cur_chroma
    M4U_WriteReg32(LARB3_BASE, 0x23c, 0x10); //venc_ref_chroma

    // MJC
    M4U_WriteReg32(LARB4_BASE, 0x200, 0x1); // MJC_MVR
    M4U_WriteReg32(LARB4_BASE, 0x204, 0x1); // MJC_MVW
    M4U_WriteReg32(LARB4_BASE, 0x208, 0x1c); // MJC_RDMA
    M4U_WriteReg32(LARB4_BASE, 0x20c, 0xa); //MJC_WDMA

    // VP concurrent settings
    // LARB0
    M4U_WriteReg32(LARB0_BASE, 0x210, 0x8);  //port 4:ovl_ch1_0/1
    M4U_WriteReg32(LARB0_BASE, 0x21C, 0x4);  //port 7:mdp_rdma0; min(4,5)
    M4U_WriteReg32(LARB0_BASE, 0x22C, 0x3);  //port11:mdp_wrot1

    // VDEC
    M4U_WriteReg32(LARB1_BASE, 0x200, 0x1f); //port#0, mc
    M4U_WriteReg32(LARB1_BASE, 0x204, 0x06); //port#1, pp
    M4U_WriteReg32(LARB1_BASE, 0x208, 0x1); //port#2, ufo
    M4U_WriteReg32(LARB1_BASE, 0x20c, 0x1); //port#3, vld
    M4U_WriteReg32(LARB1_BASE, 0x210, 0x1); //port#4, vld2
    M4U_WriteReg32(LARB1_BASE, 0x214, 0x2); //port#5, mv
    M4U_WriteReg32(LARB1_BASE, 0x218, 0x1); //port#6, pred rd
    M4U_WriteReg32(LARB1_BASE, 0x21c, 0x1); //port#7, pred wr
    M4U_WriteReg32(LARB1_BASE, 0x220, 0x1); //port#8, ppwrap    
}

static void vpSetting(void)
{
    // VP 4K
    M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x204, 0x17C0);  //LARB0, DISP+MDP
    M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x208, 0x161B);  //LARB1, VDEC
    M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x20C, 0x1000);  //LARB2, ISP
    M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x210, 0x1000);  //LARB3, VENC+JPG
    M4U_WriteReg32(SMI_COMMON_EXT_BASE, 0x214, 0x1000);  //LARB4, MJC
    
    M4U_WriteReg32(LARB0_BASE, 0x200, 0x18); //port 0:ovl_ch0_0/1
    M4U_WriteReg32(LARB0_BASE, 0x210, 0x8);  //port 4:ovl_ch1_0/1
    M4U_WriteReg32(LARB0_BASE, 0x21C, 0x4);  //port 7:mdp_rdma0; min(4,5)
    M4U_WriteReg32(LARB0_BASE, 0x220, 0x4);  //port 8:mdp_rdma1; min(4,5)
    M4U_WriteReg32(LARB0_BASE, 0x228, 0x5);  //port10:mdp_wrot0
    M4U_WriteReg32(LARB0_BASE, 0x22C, 0x3);  //port11:mdp_wrot1
    
    M4U_WriteReg32(LARB1_BASE, 0x200, 0x1f); //port#0, mc
    M4U_WriteReg32(LARB1_BASE, 0x204, 0x06); //port#1, pp
    M4U_WriteReg32(LARB1_BASE, 0x208, 0x1); //port#2, ufo
    M4U_WriteReg32(LARB1_BASE, 0x20c, 0x1); //port#3, vld
    M4U_WriteReg32(LARB1_BASE, 0x210, 0x1); //port#4, vld2
    M4U_WriteReg32(LARB1_BASE, 0x214, 0x2); //port#5, mv
    M4U_WriteReg32(LARB1_BASE, 0x218, 0x1); //port#6, pred rd
    M4U_WriteReg32(LARB1_BASE, 0x21c, 0x1); //port#7, pred wr
    M4U_WriteReg32(LARB1_BASE, 0x220, 0x1); //port#8, ppwrap

    // MJC
    M4U_WriteReg32(LARB4_BASE, 0x200, 0x1); // MJC_MVR
    M4U_WriteReg32(LARB4_BASE, 0x204, 0x1); // MJC_MVW
    M4U_WriteReg32(LARB4_BASE, 0x208, 0x1c); // MJC_RDMA
    M4U_WriteReg32(LARB4_BASE, 0x20c, 0xa); //MJC_WDMA
}

// Fake mode check, e.g. WFD
static int fake_mode_handling(MTK_SMI_BWC_CONFIG* p_conf , unsigned int *pu4LocalCnt)
{
    if (p_conf->scenario == SMI_BWC_SCEN_WFD) {
        if (p_conf->b_on_off) {
            wifi_disp_transaction = 1;
            SMIMSG("Enable WFD in profile: %d\n" , smi_profile);
        } else {
            wifi_disp_transaction = 0;
            SMIMSG("Disable WFD in profile: %d\n" , smi_profile);
        }
        return 1;
    } else {
        return 0;
    }
}

static int smi_bwc_config( MTK_SMI_BWC_CONFIG* p_conf , unsigned int *pu4LocalCnt)
{
    int i;
    int result = 0;
    unsigned int u4Concurrency = 0;
    MTK_SMI_BWC_SCEN eFinalScen;
    static MTK_SMI_BWC_SCEN ePreviousFinalScen = SMI_BWC_SCEN_CNT;

    if (smi_tuning_mode == 1) {
        SMIMSG("Doesn't change profile in tunning mode");
        return 0;
    }

    spin_lock(&g_SMIInfo.SMI_lock);
    result = fake_mode_handling(p_conf, pu4LocalCnt);
    spin_unlock(&g_SMIInfo.SMI_lock);

    // Fake mode is not a real SMI profile, so we need to return here
    if (result == 1) {
        return 0;
    }

    if((SMI_BWC_SCEN_CNT <= p_conf->scenario) || (0 > p_conf->scenario)) {
        SMIERR("Incorrect SMI BWC config : 0x%x, how could this be...\n" , p_conf->scenario);
        return -1;
    }

//Debug - S
//SMIMSG("SMI setTo%d,%s,%d\n" , p_conf->scenario , (p_conf->b_on_off ? "on" : "off") , ePreviousFinalScen);
//Debug - E

    spin_lock(&g_SMIInfo.SMI_lock);

    if (p_conf->b_on_off) {
        //turn on certain scenario
        g_SMIInfo.pu4ConcurrencyTable[p_conf->scenario] += 1;

        if(NULL != pu4LocalCnt) {
            pu4LocalCnt[p_conf->scenario] += 1;
        }
    } else {
        //turn off certain scenario
        if (0 == g_SMIInfo.pu4ConcurrencyTable[p_conf->scenario]) {
            SMIMSG("Too many turning off for global SMI profile:%d,%d\n" , p_conf->scenario , g_SMIInfo.pu4ConcurrencyTable[p_conf->scenario]);
        } else {
            g_SMIInfo.pu4ConcurrencyTable[p_conf->scenario] -= 1;
        }

        if (NULL != pu4LocalCnt)
        {
            if (0 == pu4LocalCnt[p_conf->scenario]) {
                SMIMSG("Process : %s did too many turning off for local SMI profile:%d,%d\n" , current->comm ,p_conf->scenario , pu4LocalCnt[p_conf->scenario]);
            } else {
                pu4LocalCnt[p_conf->scenario] -= 1;
            }
        }
    }

    for (i = 0; i < SMI_BWC_SCEN_CNT; i++) {
        if(g_SMIInfo.pu4ConcurrencyTable[i]) {
            u4Concurrency |= (1 << i);
        }
    }

    if ((1 << SMI_BWC_SCEN_MM_GPU) & u4Concurrency) {
        eFinalScen = SMI_BWC_SCEN_MM_GPU;
    } else if ((1 << SMI_BWC_SCEN_VR_SLOW) & u4Concurrency) {
        eFinalScen = SMI_BWC_SCEN_VR_SLOW;
    } else if ((1 << SMI_BWC_SCEN_VR) & u4Concurrency) {
        eFinalScen = SMI_BWC_SCEN_VR;
    } else if ((1 << SMI_BWC_SCEN_VP) & u4Concurrency) {
        eFinalScen = SMI_BWC_SCEN_VP;
    } else if ((1 << SMI_BWC_SCEN_SWDEC_VP) & u4Concurrency) {
        eFinalScen = SMI_BWC_SCEN_SWDEC_VP;
    } else if ((1<< SMI_BWC_SCEN_VENC) & u4Concurrency) {
        eFinalScen = SMI_BWC_SCEN_VENC;
    } else {
        eFinalScen = SMI_BWC_SCEN_NORMAL;
    }

    if (ePreviousFinalScen == eFinalScen) {
        SMIMSG("Scen equal%d,don't change\n" , eFinalScen);
        spin_unlock(&g_SMIInfo.SMI_lock);
        return 0;
    } else {
        ePreviousFinalScen = eFinalScen;
    }

    /* turn on larb clock */
    for (i = 0; i < SMI_LARB_NR; i++) {
        larb_clock_on(i);
    }

    smi_profile = eFinalScen;

    /* Bandwidth Limiter */
    switch (eFinalScen)
    {
    case SMI_BWC_SCEN_VP:
        SMIMSG( "[SMI_PROFILE] : %s\n", "SMI_BWC_VP");
        vpSetting();
        break;

    case SMI_BWC_SCEN_SWDEC_VP:
        SMIMSG( "[SMI_PROFILE] : %s\n", "SMI_BWC_SCEN_SWDEC_VP");
        vpSetting();
        break;
   
    case SMI_BWC_SCEN_VR:
        SMIMSG( "[SMI_PROFILE] : %s\n", "SMI_BWC_VR");
        vrSetting();
        break;
        
    case SMI_BWC_SCEN_VR_SLOW:
        SMIMSG( "[SMI_PROFILE] : %s\n", "SMI_BWC_VR");
        smi_profile = SMI_BWC_SCEN_VR_SLOW;
        vrSetting();
        break;

    case SMI_BWC_SCEN_VENC:
        SMIMSG( "[SMI_PROFILE] : %s\n", "SMI_BWC_SCEN_VENC");
        vrSetting();
        break;

    case SMI_BWC_SCEN_NORMAL:
        SMIMSG( "[SMI_PROFILE] : %s\n", "SMI_BWC_SCEN_NORMAL");
        initSetting();
        break;

    case SMI_BWC_SCEN_MM_GPU:
        SMIMSG( "[SMI_PROFILE] : %s\n", "SMI_BWC_SCEN_MM_GPU");
        initSetting();
        break;
 
    default:
        SMIMSG( "[SMI_PROFILE] : %s %d\n", "initSetting", eFinalScen);
        initSetting();        
        break;
    }

    /*turn off larb clock*/    
    for(i = 0; i < SMI_LARB_NR; i++) {
        larb_clock_off(i);
    }

    spin_unlock(&g_SMIInfo.SMI_lock);

    /* force 30 fps in VR slow motion, because disp driver set fps apis got mutex, call these APIs only when necessary */
    {
        static unsigned int current_fps = 0;
        
        if ((eFinalScen == SMI_BWC_SCEN_VR_SLOW) && (current_fps != 30))
        {   /* force 30 fps in VR slow motion profile */
            primary_display_force_set_vsync_fps(30);
            current_fps = 30;
            SMIMSG( "[SMI_PROFILE] set 30 fps\n");           
        }
        else if ((eFinalScen != SMI_BWC_SCEN_VR_SLOW) && (current_fps == 30))
        {   /* back to normal fps */
            current_fps = primary_display_get_fps();
            primary_display_force_set_vsync_fps(current_fps);
            SMIMSG( "[SMI_PROFILE] back to %u fps\n", current_fps);
        }        
    }        

    SMIMSG("SMI_PROFILE to:%d %s,cur:%d,%d,%d,%d\n" , p_conf->scenario , (p_conf->b_on_off ? "on" : "off") , eFinalScen , 
        g_SMIInfo.pu4ConcurrencyTable[SMI_BWC_SCEN_NORMAL] , g_SMIInfo.pu4ConcurrencyTable[SMI_BWC_SCEN_VR] , g_SMIInfo.pu4ConcurrencyTable[SMI_BWC_SCEN_VP]);

//Debug usage - S
//smi_dumpDebugMsg();
//SMIMSG("Config:%d,%d,%d\n" , eFinalScen , g_SMIInfo.pu4ConcurrencyTable[SMI_BWC_SCEN_NORMAL] , (NULL == pu4LocalCnt ? (-1) : pu4LocalCnt[p_conf->scenario]));
//Debug usage - E

    return 0;
}

struct larb_monitor larb_monitor_handler =
{
    .level = LARB_MONITOR_LEVEL_HIGH,
    .backup = on_larb_power_off,
    .restore = on_larb_power_on	
};

int smi_common_init(void)
{
    int i;

    for (i = 0; i < SMI_LARB_NR; i++) {
        pLarbRegBackUp[i] = (unsigned int*)kmalloc(LARB_BACKUP_REG_SIZE, GFP_KERNEL | __GFP_ZERO);
        if (pLarbRegBackUp[i] == NULL) {
        	  SMIERR("pLarbRegBackUp kmalloc fail %d \n", i);
        }  
    }

    /* 
     * make sure all larb power is on before we register callback func.
     * then, when larb power is first off, default register value will be backed up.
    */     

    for (i = 0; i < SMI_LARB_NR; i++) {
        larb_clock_on(i);
    }

    /* apply init setting after kernel boot */
    initSetting();

    register_larb_monitor(&larb_monitor_handler);
    
    for (i = 0; i < SMI_LARB_NR; i++) {
        larb_clock_off(i);
    }
    
    return 0;
}

static int smi_open(struct inode *inode, struct file *file)
{
    file->private_data = kmalloc(SMI_BWC_SCEN_CNT * sizeof(unsigned int), GFP_ATOMIC);

    if (NULL == file->private_data) {
        SMIMSG("Not enough entry for DDP open operation\n");
        return -ENOMEM;
    }

    memset(file->private_data , 0 , SMI_BWC_SCEN_CNT * sizeof(unsigned int));

    return 0;
}

static int smi_release(struct inode *inode, struct file *file)
{

#if 0
    unsigned long u4Index = 0 ;
    unsigned long u4AssignCnt = 0;
    unsigned long * pu4Cnt = (unsigned long *)file->private_data;
    MTK_SMI_BWC_CONFIG config;

    for(; u4Index < SMI_BWC_SCEN_CNT ; u4Index += 1)
    {
        if(pu4Cnt[u4Index])
        {
            SMIMSG("Process:%s does not turn off BWC properly , force turn off %d\n" , current->comm , u4Index);
            u4AssignCnt = pu4Cnt[u4Index];
            config.b_on_off = 0;
            config.scenario = (MTK_SMI_BWC_SCEN)u4Index;
            do
            {
                smi_bwc_config( &config , pu4Cnt);
            }
            while(0 < u4AssignCnt);
        }
    }
#endif

    if (NULL != file->private_data) {
        kfree(file->private_data);
        file->private_data = NULL;
    }

    return 0;
}

static long smi_ioctl( struct file * pFile,
                      unsigned int cmd,
                      unsigned long param)
{
    int ret = 0;

//  unsigned long * pu4Cnt = (unsigned long *)pFile->private_data;

    switch (cmd)
    {

    /* disable reg access ioctl by default for possible security holes */
    // TBD: check valid SMI register range
#if 0 
    case MTK_IOC_SMI_BWC_REGISTER_SET:
        {
            MTK_SMI_BWC_REGISTER_SET cfg;
            if( smi_tuning_mode != 1) {
                SMIMSG("Only support MTK_IOC_SMI_BWC_REGISTER_SET in tuning mode");
                return 0;
            }
            ret = copy_from_user(&cfg, (void*)param , sizeof(MTK_SMI_BWC_REGISTER_SET));
            if (ret) {
                SMIMSG(" MTK_IOC_SMI_BWC_REGISTER_SET, copy_to_user failed: %d\n", ret);
                return -EFAULT;
            }  
            // Set the address to the value assigned by user space program
            if(((unsigned int *)cfg.address) != NULL){
                M4U_WriteReg32(cfg.address, 0, cfg.value);
                SMIMSG("[Tunning] ADDR = 0x%x, VALUE = 0x%x\n", cfg.address, cfg.value);
            }
            break;  
        }
    case MTK_IOC_SMI_BWC_REGISTER_GET:
        {
            MTK_SMI_BWC_REGISTER_GET cfg;
            unsigned int value_read = 0;

            if (smi_tuning_mode != 1) {
                SMIMSG("Only support MTK_IOC_SMI_BWC_REGISTER_SET in tuning mode");
                return 0;
            }
            ret = copy_from_user(&cfg, (void*)param, sizeof(MTK_SMI_BWC_REGISTER_GET));

            if (ret)
            {
                SMIMSG(" MTK_IOC_SMI_BWC_REGISTER_GET, copy_to_user failed: %d\n", ret);
                return -EFAULT;
            }  

            value_read = M4U_ReadReg32(cfg.address, 0);

            if (((unsigned int *)cfg.return_address) != NULL){
                ret = copy_to_user((void*)cfg.return_address, (void*)&value_read, sizeof(unsigned int));

                if (ret) {
                    SMIMSG(" MTK_IOC_SMI_REGISTER_GET, copy_to_user failed: %d\n", ret);
                    return -EFAULT;
                }
            }
            SMIMSG("[Tunning] ADDR = 0x%x, VALUE = 0x%x\n", cfg.address, value_read);
            break;
        }
#endif

    case MTK_IOC_SMI_BWC_CONFIG:
        {
            MTK_SMI_BWC_CONFIG cfg;
            ret = copy_from_user(&cfg, (void*)param, sizeof(MTK_SMI_BWC_CONFIG));
            if (ret) {
                SMIMSG(" SMI_BWC_CONFIG, copy_from_user failed: %d\n", ret);
                return -EFAULT;
            }  

            ret = smi_bwc_config(&cfg, NULL);
        }
        break;

    case MTK_IOC_SMI_DUMP_LARB:
        {
            unsigned int larb_index;
            
            ret = copy_from_user(&larb_index, (void*)param, sizeof(unsigned int));
            if (ret) {
                return -EFAULT;
            }
            
            smi_dumpLarb(larb_index);
        }
        break;        

    case MTK_IOC_SMI_DUMP_COMMON:
        {
            unsigned int arg;
            
            ret = copy_from_user(&arg, (void*)param, sizeof(unsigned int));
            if (ret) {
                return -EFAULT;
            }

            smi_dumpCommon();
        }            
        break;        

    default:
        return -1;
    }

	return ret;
}

static const struct file_operations smiFops =
{
	.owner = THIS_MODULE,
	.open = smi_open,
	.release = smi_release,
	.unlocked_ioctl = smi_ioctl
};

static struct cdev * pSmiDev = NULL;
static dev_t smiDevNo = MKDEV(MTK_SMI_MAJOR_NUMBER, 0);
static inline int smi_register(void)
{
    if (alloc_chrdev_region(&smiDevNo, 0, 1,"MTK_SMI")) {
        SMIERR("Allocate device No. failed");
        return -EAGAIN;
    }
    //Allocate driver
    pSmiDev = cdev_alloc();

    if (NULL == pSmiDev) {
        unregister_chrdev_region(smiDevNo, 1);
        SMIERR("Allocate mem for kobject failed");
        return -ENOMEM;
    }

    //Attatch file operation.
    cdev_init(pSmiDev, &smiFops);
    pSmiDev->owner = THIS_MODULE;

    //Add to system
    if (cdev_add(pSmiDev, smiDevNo, 1)) {
        SMIERR("Attatch file operation failed");
        unregister_chrdev_region(smiDevNo, 1);
        return -EAGAIN;
    }

    return 0;
}

static struct class *pSmiClass = NULL;
static int smi_probe(struct platform_device *pdev)
{
    struct device* smiDevice = NULL;

    if (NULL == pdev) {
        SMIERR("platform data missed");
        return -ENXIO;
    }

    if (smi_register()) {
        dev_err(&pdev->dev,"register char failed\n");
        return -EAGAIN;
    }

    pSmiClass = class_create(THIS_MODULE, "MTK_SMI");
    if (IS_ERR(pSmiClass)) {
        int ret = PTR_ERR(pSmiClass);
        SMIERR("Unable to create class, err = %d", ret);
        return ret;
    }
    
    smiDevice = device_create(pSmiClass, NULL, smiDevNo, NULL, "MTK_SMI");

    smi_common_init();

    SMI_DBG_Init();

    return 0;
}

static int smi_remove(struct platform_device *pdev)
{
    cdev_del(pSmiDev);
    unregister_chrdev_region(smiDevNo, 1);
    device_destroy(pSmiClass, smiDevNo);
    class_destroy(pSmiClass);
    return 0;
}

static int smi_suspend(struct platform_device *pdev, pm_message_t mesg)
{
    return 0;
}

static int smi_resume(struct platform_device *pdev)
{
    return 0;
}

static struct platform_driver smiDrv = {
    .probe	= smi_probe,
    .remove	= smi_remove,
    .suspend= smi_suspend,
    .resume	= smi_resume,
    .driver	= {
    .name	= "MTK_SMI",
    .owner	= THIS_MODULE,
    }
};

static int __init smi_init(void)
{
    spin_lock_init(&g_SMIInfo.SMI_lock);

    memset(g_SMIInfo.pu4ConcurrencyTable , 0 , SMI_BWC_SCEN_CNT * sizeof(unsigned int));

    if (platform_driver_register(&smiDrv)) {
        SMIERR("failed to register MAU driver");
        return -ENODEV;
    }

    return 0;
}

static void __exit smi_exit(void)
{
    platform_driver_unregister(&smiDrv);

}

static void smi_dumpCommonDebugMsg(void)
{
    unsigned int u4Base;

    //SMI COMMON dump
    SMIMSG("===SMI common reg dump===\n");

    u4Base = SMI_COMMON_EXT_BASE;
    SMIMSG("[0x200,0x204,0x208]=[0x%x,0x%x,0x%x]\n", M4U_ReadReg32(u4Base, 0x200), M4U_ReadReg32(u4Base, 0x204), M4U_ReadReg32(u4Base, 0x208)); 
    SMIMSG("[0x20C,0x210,0x214]=[0x%x,0x%x,0x%x]\n", M4U_ReadReg32(u4Base, 0x20C), M4U_ReadReg32(u4Base, 0x210), M4U_ReadReg32(u4Base, 0x214));
    SMIMSG("[0x220,0x230,0x234,0x238]=[0x%x,0x%x,0x%x,0x%x]\n", M4U_ReadReg32(u4Base, 0x220), M4U_ReadReg32(u4Base, 0x230), M4U_ReadReg32(u4Base, 0x234), M4U_ReadReg32(u4Base, 0x238));
    SMIMSG("[0x400,0x404,0x408]=[0x%x,0x%x,0x%x]\n", M4U_ReadReg32(u4Base, 0x400), M4U_ReadReg32(u4Base, 0x404), M4U_ReadReg32(u4Base, 0x408));

    // TBD: M4U should dump these
/*
    // For VA and PA check:
    // 0x1000C5C0 , 0x1000C5C4, 0x1000C5C8, 0x1000C5CC, 0x1000C5D0
    u4Base = SMI_COMMON_AO_BASE;
    SMIMSG("===SMI always on reg dump===\n");
    SMIMSG("[0x5C0,0x5C4,0x5C8]=[0x%x,0x%x,0x%x]\n" ,M4U_ReadReg32(u4Base , 0x5C0),M4U_ReadReg32(u4Base , 0x5C4),M4U_ReadReg32(u4Base , 0x5C8));
    SMIMSG("[0x5CC,0x5D0]=[0x%x,0x%x]\n" ,M4U_ReadReg32(u4Base , 0x5CC),M4U_ReadReg32(u4Base , 0x5D0));
*/    
}

static void smi_dumpLarbDebugMsg(unsigned int u4Index)
{
    unsigned int u4Base;

    u4Base = get_larb_base_addr(u4Index);

    if (u4Base == SMI_ERROR_ADDR)
    {
        SMIMSG("Doesn't support reg dump for Larb%d\n", u4Index);
        
        return;
    } else {
        SMIMSG("===SMI LARB%d reg dump===\n" , u4Index);

        SMIMSG("[0x0,0x8,0x10]=[0x%x,0x%x,0x%x]\n", M4U_ReadReg32(u4Base, 0x0), M4U_ReadReg32(u4Base, 0x8), M4U_ReadReg32(u4Base, 0x10));
        SMIMSG("[0x24,0x50,0x60]=[0x%x,0x%x,0x%x]\n", M4U_ReadReg32(u4Base, 0x24), M4U_ReadReg32(u4Base, 0x50), M4U_ReadReg32(u4Base, 0x60));
        SMIMSG("[0xa0,0xa4,0xa8]=[0x%x,0x%x,0x%x]\n", M4U_ReadReg32(u4Base, 0xa0), M4U_ReadReg32(u4Base, 0xa4), M4U_ReadReg32(u4Base, 0xa8));
        SMIMSG("[0xac,0xb0,0xb4]=[0x%x,0x%x,0x%x]\n", M4U_ReadReg32(u4Base, 0xac), M4U_ReadReg32(u4Base, 0xb0), M4U_ReadReg32(u4Base, 0xb4));
        SMIMSG("[0xb8,0xbc,0xc0]=[0x%x,0x%x,0x%x]\n", M4U_ReadReg32(u4Base, 0xb8), M4U_ReadReg32(u4Base, 0xbc), M4U_ReadReg32(u4Base, 0xc0));
        SMIMSG("[0xc8,0xcc]=[0x%x,0x%x]\n", M4U_ReadReg32(u4Base, 0xc8), M4U_ReadReg32(u4Base, 0xcc));                
    }
}

static void smi_dump_format(unsigned int base, unsigned int from, unsigned int to)
{
    int i, j, left;    
    unsigned int value[8];
    
    for (i = from; i <= to; i += 32)
    {        
        for (j = 0; j < 8; j++)
        {
            value[j] = M4U_ReadReg32(base, i + j * 4);
        }
        
        SMIMSG2("%8x %x %x %x %x %x %x %x %x\n", i, value[0], value[1], value[2], value[3], value[4], value[5], value[6], value[7]);
    }
        
    left = ((from - to) / 4 + 1) % 8;
    
    if (left)
    {        
        memset(value, 0, 8 * sizeof(unsigned int));
        
        for (j = 0; j < left; j++)
        {
            value[j] = M4U_ReadReg32(base, i - 32 + j * 4);
        }
        
        SMIMSG2("%8x %x %x %x %x %x %x %x %x\n", i - 32 + j * 4, value[0], value[1], value[2], value[3], value[4], value[5], value[6], value[7]);
    }            
}

static void smi_dumpLarb(unsigned int index)
{
    unsigned int u4Base;

    u4Base = get_larb_base_addr(index);

    if (u4Base == SMI_ERROR_ADDR)
    {
        SMIMSG2("Doesn't support reg dump for Larb%d\n", index);
        
        return;
    } else {        
        SMIMSG2("===SMI LARB%d reg dump base 0x%x===\n", index, u4Base);

        smi_dump_format(u4Base, 0, 0x434);        
        smi_dump_format(u4Base, 0xF00, 0xF0C);        
    }            
}

static void smi_dumpCommon(void)
{
    SMIMSG2("===SMI COMMON reg dump base 0x%x===\n", SMI_COMMON_EXT_BASE);

    smi_dump_format(SMI_COMMON_EXT_BASE, 0x1A0, 0x418);   
}

void smi_dumpDebugMsg(void)
{
    unsigned int u4Index;
   
    // SMI COMMON dump
    smi_dumpCommonDebugMsg();

    // dump all SMI LARB 
    for (u4Index = 0; u4Index < SMI_LARB_NR; u4Index++) {
        smi_dumpLarbDebugMsg(u4Index);
    }
}

int smi_debug_bus_hanging_detect(unsigned int larbs, int show_dump){

    int i = 0;
    int dump_time = 0;
    int is_smi_issue = 0;
    int status_code = 0;
    // Keep the dump result
    unsigned char smi_common_busy_count = 0;
    volatile unsigned int reg_temp = 0;
    unsigned char smi_larb_busy_count[SMI_LARB_NR] = {0};
    unsigned char smi_larb_mmu_status[SMI_LARB_NR] = {0};

    // dump resister and save resgister status
    for (dump_time = 0; dump_time < 5; dump_time++) {
        unsigned int u4Index = 0;
        reg_temp = M4U_ReadReg32(SMI_COMMON_EXT_BASE, 0x400);
        if ((reg_temp & (1<<30)) == 0) {
            // smi common is busy
            smi_common_busy_count++;
        }		    
        // Dump smi common regs
        if (show_dump != 0) {
            smi_dumpCommonDebugMsg();
        }
        for (u4Index = 0; u4Index < SMI_LARB_NR; u4Index++) {
            unsigned int u4Base = get_larb_base_addr(u4Index);
            if (u4Base != SMI_ERROR_ADDR) {
                reg_temp = M4U_ReadReg32(u4Base, 0x0);
                if (reg_temp != 0) {
                    // Larb is busy
                    smi_larb_busy_count[u4Index]++;
                }
                smi_larb_mmu_status[u4Index] = M4U_ReadReg32(u4Base, 0xa0);
                if (show_dump != 0) {
                    smi_dumpLarbDebugMsg(u4Index);
                }
            }
        }

    }

    // Show the checked result    
    for (i = 0; i < SMI_LARB_NR; i++) { // Check each larb
        if (SMI_DGB_LARB_SELECT(larbs, i)) {
            // larb i has been selected
            // Get status code

            if (smi_larb_busy_count[i] == 5) {  // The larb is always busy
                if (smi_common_busy_count == 5) {  // smi common is always busy
                    status_code = 1;
                } else if (smi_common_busy_count == 0) { // smi common is always idle
                    status_code = 2;
                } else {
                    status_code = 5; // smi common is sometimes busy and idle
                }
            } else if (smi_larb_busy_count[i] == 0) { // The larb is always idle
                if (smi_common_busy_count == 5) {  // smi common is always busy
                    status_code = 3;
                } else if (smi_common_busy_count == 0) { // smi common is always idle
                    status_code = 4;
                } else {
                    status_code = 6; // smi common is sometimes busy and idle
                }                     
            } else { //sometime the larb is busy
                if (smi_common_busy_count == 5) {  // smi common is always busy
                    status_code = 7;
                } else if (smi_common_busy_count == 0) { // smi common is always idle
                    status_code = 8;
                } else {
                    status_code = 9; // smi common is sometimes busy and idle
                }            
            }

            // Send the debug message according to the final result
            switch (status_code) {
                    case 1:
                    case 3:
                    case 5:
                    case 7:
                    case 8:
                        SMIMSG("Larb%d Busy=%d/5, SMI Common Busy=%d/5, status=%d ==> Check engine's state first", i, smi_larb_busy_count[i], smi_common_busy_count, status_code);
                        SMIMSG("If the engine is waiting for Larb%ds' response, it needs SMI HW's check", i);
                        break;
                    case 2:
                        if (smi_larb_mmu_status[i] == 0) {
                            SMIMSG("Larb%d Busy=%d/5, SMI Common Busy=%d/5, status=%d ==> Check engine state first", i, smi_larb_busy_count[i], smi_common_busy_count, status_code);
                            SMIMSG("If the engine is waiting for Larb%ds' response, it needs SMI HW's check", i);
                        } else {
                            SMIMSG("Larb%d Busy=%d/5, SMI Common Busy=%d/5, status=%d ==> MMU port config error", i, smi_larb_busy_count[i], smi_common_busy_count, status_code);
                            is_smi_issue = 1;
                        }
                        break;
                    case 4:
                    case 6:
                    case 9:
                        SMIMSG("Larb%d Busy=%d/5, SMI Common Busy=%d/5, status=%d ==> not SMI issue", i, smi_larb_busy_count[i], smi_common_busy_count, status_code);
                        break;
                    default:
                        SMIMSG("Larb%d Busy=%d/5, SMI Common Busy=%d/5, status=%d ==> status unknown", i, smi_larb_busy_count[i], smi_common_busy_count, status_code);     
                        break;
            }    
        }
    }

    return is_smi_issue;
}

module_init(smi_init);
module_exit(smi_exit);

module_param_named(tuning_mode, smi_tuning_mode, uint, S_IRUGO | S_IWUSR);
module_param_named(wifi_disp_transaction, wifi_disp_transaction, uint, S_IRUGO | S_IWUSR);

MODULE_DESCRIPTION("MTK SMI driver");
MODULE_AUTHOR("Glory Hung<glory.hung@mediatek.com>");
MODULE_LICENSE("GPL");


