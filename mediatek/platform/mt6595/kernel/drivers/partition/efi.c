#include <linux/types.h>
#include <linux/genhd.h> 
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/crc32.h>
#include <asm/uaccess.h>

#include <linux/mmc/sd_misc.h>

#include "efi.h"

#if 0
#include <linux/kernel.h>	/* printk() */
#include <linux/module.h>
#include <linux/types.h>	/* size_t */
#include <linux/slab.h>		/* kmalloc() */
#endif

#define USING_XLOG

#ifdef USING_XLOG 
#include <linux/xlog.h>

#define TAG     "PART_KL"

#define part_err(fmt, args...)       \
    xlog_printk(ANDROID_LOG_ERROR, TAG, fmt, ##args)
#define part_info(fmt, args...)      \
    xlog_printk(ANDROID_LOG_INFO, TAG, fmt, ##args)

#else

#define TAG     "[PART_KL]"

#define part_err(fmt, args...)       \
    printk(KERN_ERR TAG);           \
    printk(KERN_CONT fmt, ##args) 
#define part_info(fmt, args...)      \
    printk(KERN_NOTICE TAG);        \
    printk(KERN_CONT fmt, ##args)

#endif


#if 1
struct part_t {
    u64 start;
    u64 size;
    u32 part_id;
    u8 name[64];
};

struct partition_package {
    u64 signature;
    u32 version;
    u32 nr_parts;
    u32 sizeof_partition;
    //struct partition *part_info;
};

#define PARTITION_PACKAGE_SIGNATURE
#define PARTITION_PACKAGE_VERSION

static u32 efi_crc32(u8 *p, u32 len)
{
    return (crc32(~0L, p, len) ^ ~0L);
}

static efi_guid_raw_data partition_basic_data_guid = {
    0xEBD0A0A2, 0xB9E5, 0x4433, 0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7};

static efi_guid_raw_data partition_guid_tables[] = {
    {0xF57AD330, 0x39C2, 0x4488, 0x9B, 0xB0, 0x00, 0xCB, 0x43, 0xC9, 0xCC, 0xD4},
    {0xFE686D97, 0x3544, 0x4A41, 0xBE, 0x21, 0x16, 0x7E, 0x25, 0xB6, 0x1B, 0x6F},
    {0x1CB143A8, 0xB1A8, 0x4B57, 0xB2, 0x51, 0x94, 0x5C, 0x51, 0x19, 0xE8, 0xFE},
    {0x3B9E343B, 0xCDC8, 0x4D7F, 0x9F, 0xA6, 0xB6, 0x81, 0x2E, 0x50, 0xAB, 0x62},
    {0x5F6A2C79, 0x6617, 0x4B85, 0xAC, 0x02, 0xC2, 0x97, 0x5A, 0x14, 0xD2, 0xD7},
    {0x4AE2050B, 0x5DB5, 0x4FF7, 0xAA, 0xD3, 0x57, 0x30, 0x53, 0x4B, 0xE6, 0x3D},
    {0x1F9B0939, 0xE16B, 0x4BC9, 0xA5, 0xBC, 0xDC, 0x2E, 0xE9, 0x69, 0xD8, 0x01},
    {0xD722C721, 0x0DEE, 0x4CB8, 0x8A, 0x83, 0x2C, 0x63, 0xCD, 0x13, 0x93, 0xC7},
    {0xE02179A8, 0xCEB5, 0x48A9, 0x88, 0x31, 0x4F, 0x1C, 0x9C, 0x5A, 0x86, 0x95},
    {0x84B09A81, 0xFAD2, 0x41AC, 0x89, 0x0E, 0x40, 0x7C, 0x24, 0x97, 0x5E, 0x74},
    {0xE8F0A5EF, 0x8D1B, 0x42EA, 0x9C, 0x2A, 0x83, 0x5C, 0xD7, 0x7D, 0xE3, 0x63},
    {0xD5F0E175, 0xA6E1, 0x4DB7, 0x94, 0xC0, 0xF8, 0x2A, 0xD0, 0x32, 0x95, 0x0B},
    {0x1D9056E1, 0xE139, 0x4FCA, 0x8C, 0x0B, 0xB7, 0x5F, 0xD7, 0x4D, 0x81, 0xC6},
    {0x7792210B, 0xB6A8, 0x45D5, 0xAD, 0x91, 0x33, 0x61, 0xED, 0x14, 0xC6, 0x08},
    {0x138A6DB9, 0x1032, 0x451D, 0x91, 0xE9, 0x0F, 0xA3, 0x8F, 0xF9, 0x4F, 0xBB},
    {0x756D934C, 0x50E3, 0x4C91, 0xAF, 0x46, 0x02, 0xD8, 0x24, 0x16, 0x9C, 0xA7},
    {0xA3F3C267, 0x5521, 0x42DD, 0xA7, 0x24, 0x3B, 0xDE, 0xC2, 0x0C, 0x7C, 0x6F},
    {0x8C68CD2A, 0xCCC9, 0x4C5D, 0x8B, 0x57, 0x34, 0xAE, 0x9B, 0x2D, 0xD4, 0x81},
    {0x6A5CEBF8, 0x54A7, 0x4B89, 0x8D, 0x1D, 0xC5, 0xEB, 0x14, 0x0B, 0x09, 0x5B},
    {0xA0D65BF8, 0xE8DE, 0x4107, 0x94, 0x34, 0x1D, 0x31, 0x8C, 0x84, 0x3D, 0x37},
    {0x46F0C0BB, 0xF227, 0x4EB6, 0xB8, 0x2F, 0x66, 0x40, 0x8E, 0x13, 0xE3, 0x6D},
    {0xFBC2C131, 0x6392, 0x4217, 0xB5, 0x1E, 0x54, 0x8A, 0x6E, 0xDB, 0x03, 0xD0},
    {0xE195A981, 0xE285, 0x4734, 0x80, 0x25, 0xEC, 0x32, 0x3E, 0x95, 0x89, 0xD9},
    {0xE29052F8, 0x5D3A, 0x4E97, 0xAD, 0xB5, 0x5F, 0x31, 0x2C, 0xE6, 0x61, 0x0A},
    {0x9C3CABD7, 0xA35D, 0x4B45, 0x8C, 0x57, 0xB8, 0x07, 0x75, 0x42, 0x6B, 0x35},
    {0xE7099731, 0x95A6, 0x45A6, 0xA1, 0xE5, 0x1B, 0x6A, 0xBA, 0x03, 0x2C, 0xF1},
    {0x8273E1AB, 0x846F, 0x4468, 0xB9, 0x99, 0xEE, 0x2E, 0xA8, 0xE5, 0x0A, 0x16},
    {0xD26472F1, 0x9EBC, 0x421D, 0xBA, 0x14, 0x31, 0x12, 0x96, 0x45, 0x7C, 0x90},
    {0xB72CCBE9, 0x2055, 0x46F4, 0xA1, 0x67, 0x4A, 0x06, 0x9C, 0x20, 0x17, 0x38},
    {0x9C1520F3, 0xC2C5, 0x4B89, 0x82, 0x42, 0xFE, 0x4C, 0x61, 0x20, 0x8A, 0x9E},
    {0x902D5F3F, 0x434A, 0x4DE7, 0x89, 0x88, 0x32, 0x1E, 0x88, 0xC9, 0xB8, 0xAA},
    {0xBECE74C8, 0xD8E2, 0x4863, 0x9B, 0xFE, 0x5B, 0x0B, 0x66, 0xBB, 0x92, 0x0F},
    {0xFF1342CF, 0xB7BE, 0x44D5, 0xA2, 0x5E, 0xA4, 0x35, 0xAD, 0xDD, 0x27, 0x02},
    {0xA4DA8F1B, 0xFE07, 0x433B, 0x95, 0xCB, 0x84, 0xA5, 0xF2, 0x3E, 0x47, 0x7B},
    {0xC2635E15, 0x61AA, 0x454E, 0x9C, 0x40, 0xEB, 0xE1, 0xBD, 0xF1, 0x9B, 0x9B},
    {0x4D2D1290, 0x36A3, 0x4F5D, 0xAF, 0xB4, 0x31, 0x9F, 0x8A, 0xB6, 0xDC, 0xD8},
    {0xFDCE12F0, 0xA7EB, 0x40F7, 0x83, 0x50, 0x96, 0x09, 0x72, 0xE6, 0xCB, 0x57},
    {0x0FBBAFA2, 0x4AA9, 0x4490, 0x89, 0x83, 0x53, 0x29, 0x32, 0x85, 0x05, 0xFD},
    {0xA76E4B2F, 0x31CB, 0x40BA, 0x82, 0x6A, 0xC0, 0xCB, 0x0B, 0x73, 0xC8, 0x56},
    {0xF54AC030, 0x7004, 0x4D02, 0x94, 0x81, 0xBB, 0xF9, 0x82, 0x03, 0x68, 0x07},  
}; 



//#define min(a, b)   ((a) < (b) ? (a) : (b))

#define GPT_HEADER_SIGNATURE 0x5452415020494645ULL
#define GPT_HEADER_VERSION 0x00010000  

static u64 g_sdmmc_user_size;

static u64 first_usable_lba;
static u64 last_usable_lba;
static void set_first_usable_lba(u64 lba)
{
    first_usable_lba = lba;
}
static u64 get_first_usable_lba(void)
{
    return first_usable_lba;
}

static void set_last_usable_lba(u64 lba)
{
    last_usable_lba = lba;
}
static u64 get_last_usable_lba(void)
{
    return last_usable_lba;
}

static u64 last_lba(void)
{
    return g_sdmmc_user_size / 512 - 1;
}



#if 0
static int read_data(u32 *buf, u64 lba, u64 size)
{
    int err;
    
    err = SDMMC_Read_Part(lba, buf, size, EMMC_PART_USER);
    return err;  
}

static int parse_gpt_header(u64 header_lba, struct partition *parts, int *nr_entries)
{
    int err; 
    int i;
    
    u32 calc_crc, orig_header_crc;
    u64 entries_real_size, entries_read_size;

    gpt_header *header = (gpt_header *)header_buf;
    gpt_entry *entries = (gpt_entry *)entries_buf;

    err = read_data((u32 *)header, header_lba, 512);
    if (err) {
        part_err("read header(lba=%lx), err(%d)\n", header_lba, err);
        return err;
    }
        
    if (header->signature != GPT_HEADER_SIGNATURE) {
        part_err("check header, err(signature 0x%lx!=0x%lx)\n", header->signature, GPT_HEADER_SIGNATURE);
        return 1;
    }

    orig_header_crc = header->header_crc32;
    header->header_crc32 = 0;
    calc_crc = efi_crc32((u8 *)header, header->header_size);

    if (orig_header_crc != calc_crc) {
        part_err("check header, err(crc 0x%x!=0x%x(calc))\n", orig_header_crc, calc_crc);
        return 1;
    }

    header->header_crc32 = orig_header_crc;

    if (header->my_lba != header_lba) {
        part_err("check header, err(my_lba 0x%x!=0x%x)\n", header->my_lba, header_lba);
        return 1;
    }

    entries_real_size = header->num_partition_entries * header->sizeof_partition_entry;
    entries_read_size = ((header->num_partition_entries + 3) / 4) * 512;
    err = read_data((u32 *)entries, header->partition_entry_lba, entries_read_size);
    if (err) {
        part_err("read entries(lba=%lu), err(%d)\n", header->partition_entry_lba, err);
        return err;
    }

    calc_crc = efi_crc32((u8 *)entries, entries_real_size);
    if (header->partition_entry_array_crc32 != calc_crc) {
        part_err("check header, err(entries crc 0x%x!=0x%x(calc)\n", header->partition_entry_array_crc32, calc_crc);
        return 1;
    }

    for (i = 0; i < header->num_partition_entries; i++) {
        w2s(parts[i].name, MAX_PARTITION_NAME_LEN, (u16 *)entries[i].partition_name, GPT_ENTRY_NAME_LEN);
        parts[i].size = (entries[i].ending_lba - entries[i].starting_lba + 1) * 512;
        parts[i].region = EMMC_PART_USER;
        parts[i].start = entries[i].starting_lba * 512;
    } 
    
    *nr_entries = header->num_partition_entries;

    return 0;
}


int read_gpt(struct partition *parts, int *nr_entries)
{
    int err;

    part_info("Parsing Primary GPT now...\n");
    err = parse_gpt_header(1, parts, nr_entries);
    if (!err) {
        part_info("Parsing Primary GPT Done...\n");
        return S_DONE;
    }

    part_info("Parsing Secondary GPT now...\n");
    err = parse_gpt_header(last_lba(), parts, nr_entries);
    if (!err) {
        part_info("Parsing Secondary GPT done...\n");
        return S_DONE;
    }

    part_err("No Valid GPT...\n");
    return S_PART_NO_VALID_TABLE;
}
#endif

static void s2w(u16 *dst, int dst_max, u8 *src, int src_max)
{
    int i = 0;
    int len = min(src_max, dst_max - 1);

    while (i < len) {
        if (!src[i]) {
            break;
        }
        dst[i] = (u16)src[i]; 
        i++;
    }

    dst[i] = 0;
}
#if 0
static void w2s(u8 *dst, int dst_max, u16 *src, int src_max)
{
    int i = 0;
    int len = min(src_max, dst_max - 1);

    while (i < len) {
        if (!src[i]) {
            break;
        }
        dst[i] = src[i] & 0xFF; 
        i++;
    }

    dst[i] = 0;
}
#endif

static inline void efi_guidcpy(efi_guid_t *dst, efi_guid_raw_data *src)
{
    dst->b[0] = (src->a) & 0xff;
    dst->b[1] = ((src->a) >> 8) & 0xff;
    dst->b[2] = ((src->a) >> 16) & 0xff;
    dst->b[3] = ((src->a) >> 24) & 0xff;

    dst->b[4] = (src->b) & 0xff;
    dst->b[5] = ((src->b) >> 8) & 0xff;
            
    dst->b[6] = (src->c)  & 0xff;
    dst->b[7] = ((src->c) >> 8) & 0xff;

    dst->b[8] = src->d0;
    dst->b[9] = src->d1;
    dst->b[10] = src->d2;
    dst->b[11] = src->d3;
    dst->b[12] = src->d4;
    dst->b[13] = src->d5;
    dst->b[14] = src->d6;
    dst->b[15] = src->d7;
}


#endif

#if 0
static int read_data(loff_t offset, int origin, u8 *buf, int size)
{
    struct file *filp;
    mm_segment_t old_fs; 
    loff_t cur_pos; 
    size_t bytes_read;

    filp = filp_open("/dev/block/mmcblk0", O_RDONLY|O_SYNC, 0); 
    if (IS_ERR(filp)) {
        part_err("%s: open error(%d)\n", __func__, PTR_ERR(filp));
        return -EIO;
    }   

    if (!filp->f_op || !filp->f_op->llseek || !filp->f_op->read) {
        part_err("%s: operation not supported\n", __func__);
        return -EIO;
    }

    old_fs = get_fs();
    set_fs(KERNEL_DS);

    cur_pos = filp->f_op->llseek(filp, offset, origin);
    if (cur_pos < 0) {
        part_err("%s: llseek error(%d)\n", __func__, cur_pos);
        goto out;
    }

    bytes_read = filp->f_op->read(filp, buf, size, &(filp->f_pos));
    if (bytes_read < 0) {
        part_err("%s: read error(%d)\n", __func__, bytes_read);
    }

out:
    set_fs(old_fs);
    filp_close(filp, NULL);

    return size;
#if 0    
    ptr = (u32 *)buf;
    for (i = 0; i < 512 / 4; i++) {
        part_info("0x%x ", ptr[i]);
        if ((i+1) % 16 == 0) {
            part_info("\n");
        }   
    }   
#endif
}
#endif

static int write_data(loff_t offset, int origin, u8 *buf, int size)
{
    //part_info("write_data entry, but no operation\n");
#if 1
    struct file *filp;
    mm_segment_t old_fs; 
    loff_t cur_pos; 
    size_t bytes_write;

    filp = filp_open("/dev/block/mmcblk0", O_WRONLY, 0); 
    if (IS_ERR(filp)) {
        part_err("%s: open error(%d)\n", __func__, PTR_ERR(filp));
        return -EIO;
    }   

    if (!filp->f_op || !filp->f_op->llseek || !filp->f_op->write) {
        part_err("%s: operation not supported\n", __func__);
        return -EIO;
    }

    old_fs = get_fs();
    set_fs(KERNEL_DS);

    cur_pos = filp->f_op->llseek(filp, offset, origin);
    if (cur_pos < 0) {
        part_err("%s: llseek error(%d)\n", __func__, cur_pos);
        goto out;
    }

    bytes_write = filp->f_op->write(filp, buf, size, &filp->f_pos);
    if (bytes_write < 0) {
        part_err("%s: write error(%d)\n", __func__, bytes_write);
    }

out:
    set_fs(old_fs);
    filp_close(filp, NULL);
#endif
    return size;
}

static int erase_data(u64 start, u64 len)
{
    struct file *filp;
    mm_segment_t old_fs;
    u64 range[2];
    long ret;

    part_info("%s: start=0x%llx, len=0x%llx\n", __func__, start, len);

    filp = filp_open("/dev/block/mmcblk0", O_RDWR, 0); 
    if (IS_ERR(filp)) {
        part_err("%s: open error(%d)\n", __func__, PTR_ERR(filp));
        return -EIO;
    }   

    if (!filp->f_op || !filp->f_op->unlocked_ioctl) {
        part_err("%s: operation not supported\n", __func__);
        return -EIO;
    }

    old_fs = get_fs();
    set_fs(KERNEL_DS);

    range[0] = start;
    range[1] = len;

    ret = filp->f_op->unlocked_ioctl(filp, BLKDISCARD, (unsigned long)range);
    if (ret < 0) {
        part_err("%s: discard error(%d)\n", __func__, ret);
    }

    set_fs(old_fs);
    filp_close(filp, NULL);

    return ret;
}

static int get_total_size(void)
{
    struct file *filp;
    mm_segment_t old_fs;
    long ret;

    filp = filp_open("/dev/block/mmcblk0", O_RDWR, 0); 
    if (IS_ERR(filp)) {
        part_err("%s: open error(%d)\n", __func__, PTR_ERR(filp));
        return -EIO;
    }   

    if (!filp->f_op || !filp->f_op->unlocked_ioctl) {
        part_err("%s: operation not supported\n", __func__);
        return -EIO;
    }

    old_fs = get_fs();
    set_fs(KERNEL_DS);

    ret = filp->f_op->unlocked_ioctl(filp, BLKGETSIZE64, (unsigned long)&g_sdmmc_user_size);
    if (ret < 0) {
        part_err("%s: getsize error(%d)\n", __func__, ret);
    }

    set_fs(old_fs);
    filp_close(filp, NULL);

    part_info("%s: g_sdmmc_user_size=0x%llx\n", __func__, g_sdmmc_user_size);

    return 0;
}

static int param_valid(struct part_t *parts, int nr_parts)
{
    //int i;

    //int pgpt_index = 0;
    //int sgpt_index = 0;
    
    u64 pgpt_start, pgpt_size;
    u64 sgpt_start, sgpt_size;

    pgpt_start = 0;
    pgpt_size = parts[0].start;

    sgpt_start = parts[nr_parts-1].start + parts[nr_parts-1].size;
    sgpt_size = g_sdmmc_user_size - sgpt_start;

    if (pgpt_start != 0 || pgpt_size < 34 * 512 || pgpt_size % 512) {
        part_err("check param, err(pgpt_start = 0x%lx, pgpt_size = 0x%lx)\n", 
                        pgpt_start, pgpt_size);
        return 1;
    }

    if (sgpt_size < 33 * 512 || sgpt_size % 512) {
        part_err("check param, err(sgpt_size = 0x%lx)\n", sgpt_size);
        return 1;
    }

    if (nr_parts > 128) {
        part_err("check param, err(nr_parts = %d)\n", nr_parts);
        return 1;
    }

    set_first_usable_lba(pgpt_size / 512);
    set_last_usable_lba((sgpt_start - 1) / 512);

    part_info("pgpt_start=0x%llx, pgpt_size=0x%llx\n", pgpt_start, pgpt_size);
    part_info("sgpt_start=0x%llx, sgpt_size=0x%llx\n", sgpt_start, sgpt_size);
    
    return 0;
}

static gpt_entry *pack_entries_data(struct part_t *parts, int nr_parts)
{
    int i;
    
    gpt_entry *entries = kzalloc(nr_parts * sizeof(gpt_entry), GFP_KERNEL);
    
    for (i = 0; i < nr_parts; i++) {
        efi_guidcpy(&entries[i].partition_type_guid, &partition_basic_data_guid);
        efi_guidcpy(&entries[i].unique_partition_guid, &partition_guid_tables[i]);      
        entries[i].starting_lba = parts[i].start / 512;
        entries[i].ending_lba = (parts[i].start + parts[i].size - 1) / 512;
        s2w((u16 *)entries[i].partition_name, GPT_ENTRY_NAME_LEN, parts[i].name, 64);
    }

    return entries;
}

static gpt_header *pack_pheader_data(gpt_entry *entries, int nr_parts)
{
    gpt_header *header = kzalloc(sizeof(gpt_header), GFP_KERNEL);
    
    header->signature = GPT_HEADER_SIGNATURE;
    header->revision = GPT_HEADER_VERSION;
    header->header_size = sizeof(gpt_header);
    header->header_crc32 = 0;
    header->my_lba = 1;
    header->alternate_lba = last_lba();
    header->first_usable_lba = get_first_usable_lba();
    header->last_usable_lba = get_last_usable_lba();
    header->partition_entry_lba = 2;    
    header->num_partition_entries = nr_parts;
    header->sizeof_partition_entry = sizeof(gpt_entry);
    header->partition_entry_array_crc32 = efi_crc32((u8 *)entries, nr_parts * sizeof(gpt_entry));
    header->header_crc32 = efi_crc32((u8 *)header, sizeof(gpt_header));

    return header;
}

static gpt_header *pack_sheader_data(gpt_entry *entries, int nr_parts)
{
    gpt_header *header = kzalloc(sizeof(gpt_header), GFP_KERNEL);
    
    header->signature = GPT_HEADER_SIGNATURE;
    header->revision = GPT_HEADER_VERSION;
    header->header_size = sizeof(gpt_header);
    header->header_crc32 = 0;
    header->my_lba = last_lba();
    header->alternate_lba = 1;
    header->first_usable_lba = get_first_usable_lba();
    header->last_usable_lba = get_last_usable_lba();
    header->partition_entry_lba = 2;    
    header->num_partition_entries = nr_parts;
    header->sizeof_partition_entry = sizeof(gpt_entry);
    header->partition_entry_array_crc32 = efi_crc32((u8 *)entries, nr_parts * sizeof(gpt_entry));
    header->header_crc32 = efi_crc32((u8 *)header, sizeof(gpt_header));

    return header;
}

static int write_pheader_data(gpt_header *header)
{
    int err;
    
    err = erase_data(512, 512);
    if (err) {
        part_err("erase pheader, err(%d)\n", err);
        return err;
    }

    err = write_data(512, SEEK_SET, (u8 *)header, sizeof(gpt_header));
    if (err) {
        part_err("write pheader, err(%d)\n", err);
        return err;
    }

    return err;
}

static int write_sheader_data(gpt_header *header)
{
    int err;

    err = erase_data(g_sdmmc_user_size - 512, 512);
    if (err) {
        part_err("%s: erase sheader, err(%d)\n", __func__, err);
        return err;
    }

    err = write_data(g_sdmmc_user_size - 512, SEEK_SET, (u8 *)header, sizeof(gpt_header));
    if (err) {
        part_err("write sheader, err(%d)\n", err);
        return err;
    }

    return err;
}


static int write_pentries_data(gpt_entry *entries, int nr_parts)
{
    int err;

    err = erase_data(512 * 2, (entries[0].starting_lba - 2) * 512);
    if (err) {
        part_err("%s: erase pentries, err(%d)\n", __func__, err);
        return err;
    }

    err = write_data(512 * 2, SEEK_SET, (u8 *)entries, sizeof(*entries) * nr_parts);
    if (err) {
        part_err("%s: write pentries, err(%d)\n", __func__, err);
        return err;
    }

    return err;
}

static int write_sentries_data(gpt_entry *entries, int nr_parts)
{
    int err;

    u64 sentries_start_lba;
    u64 sentries_erase_size;
    //u64 sentries_write_size;

    sentries_start_lba = get_last_usable_lba() + 1;
    sentries_erase_size = (last_lba() - get_last_usable_lba() - 1) * 512;
    //sentries_write_size = ((get_nr_entries() + 3) / 4) * 512; 

    err = erase_data(sentries_start_lba * 512, sentries_erase_size);
    if (err) {
        part_err("%s: erase pentries, err(%d)\n", __func__, err);
        return err;
    }

    err = write_data(sentries_start_lba * 512, SEEK_SET, (u8 *)entries, sizeof(*entries) * nr_parts);
    if (err) {
        part_err("%s: write sentries, err(%d)\n", __func__, err);
        return err;
    }

    return err;
}

int update_partition_table(struct partition_package *package)
{
    gpt_header *pheader, *sheader;
    gpt_entry *entries;
    struct part_t *part_info;
    int err;

    get_total_size();

    part_info = (void *)package + sizeof(*package);

    err = param_valid(part_info, package->nr_parts);
    if (err) {
        return 1;
    }      

    part_info("%s: pack entries\n", __func__);
    entries = pack_entries_data(part_info, package->nr_parts);
    part_info("%s: pack primiary header\n", __func__);
    pheader = pack_pheader_data(entries, package->nr_parts);
    part_info("%s: pack secondary header\n", __func__);
    sheader = pack_sheader_data(entries, package->nr_parts);

    err = write_sheader_data(sheader);
    err = write_sentries_data(entries, package->nr_parts);

    err = write_pheader_data(pheader);
    err = write_pentries_data(entries, package->nr_parts);

    kfree(entries);
    kfree(pheader);
    kfree(sheader);

    return 0;
}

