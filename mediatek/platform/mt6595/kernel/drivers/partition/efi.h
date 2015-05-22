#ifndef _EFI_H
#define _EFI_H

#include <linux/types.h>

/* GuidPT */

typedef struct {
    u8 boot_ind;
    u8 head;
    u8 sector;
    u8 cyl;
    u8 sys_ind;
    u8 end_head;
    u8 end_sector;
    u8 end_cyl;
    u32 start_sector;
    u32 nr_sects;
} __attribute__((packed)) partition;

typedef struct {
    u8 boot_code[440];
    u32 unique_mbr_signature;
    u16 unknown;
    partition partition_record[4];
    u16 signature;
} __attribute__((packed)) pmbr;

typedef struct {
    u8 b[16];
} __attribute__((packed)) efi_guid_t;

#define GPT_ENTRY_NAME_LEN  (72 / sizeof(u16))

typedef struct {
    efi_guid_t partition_type_guid;
    efi_guid_t unique_partition_guid;
    u64 starting_lba;
    u64 ending_lba;
    u64 attributes;
    u16 partition_name[GPT_ENTRY_NAME_LEN];    
} __attribute__((packed)) gpt_entry;

typedef struct {
    u64 signature;
    u32 revision;
    u32 header_size; 
    u32 header_crc32;
    u32 reserved;
    u64 my_lba;
    u64 alternate_lba;
    u64 first_usable_lba;
    u64 last_usable_lba;
    efi_guid_t disk_guid;
    u64 partition_entry_lba;
    u32 num_partition_entries;
    u32 sizeof_partition_entry;
    u32 partition_entry_array_crc32;
} __attribute__((packed)) gpt_header;


typedef struct {
    u32 a;
    u16 b;
    u16 c;
    u8  d0;
    u8  d1;
    u8  d2;
    u8  d3;
    u8  d4;
    u8  d5;
    u8  d6;
    u8  d7;
} __attribute__((packed)) efi_guid_raw_data;

#define EFI_GUID(a,b,c,d0,d1,d2,d3,d4,d5,d6,d7) \
((efi_guid_t)   \
{(a) & 0xff, ((a) >> 8) & 0xff, ((a) >> 16) & 0xff, ((a) >> 24) & 0xff, \
    (b) & 0xff, ((b) >> 8) & 0xff, \
    (c) & 0xff, ((c) >> 8) & 0xff, \
    (d0), (d1), (d2), (d3), (d4), (d5), (d6), (d7)})

#define PARTITION_BASIC_DATA_GUID   \
    EFI_GUID(0xEBD0A0A2, 0xB9E5, 0x4433, 0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7)


#endif
