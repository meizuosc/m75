#ifndef __MZ_PRIVATE_H
#define __MZ_PRIVATE_H

/*mz private information data*/
#define ATAG_LK_INFO 0xCA02191A

struct tag_lk_info {
    unsigned int lk_version;
    unsigned int lk_mode;
    unsigned int hw_version;
    unsigned int sw_version;
    unsigned char  sn[64];
    unsigned char  psn[64];
    unsigned int rsv[4];
};

#endif
