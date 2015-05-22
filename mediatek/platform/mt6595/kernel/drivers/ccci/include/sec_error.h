#ifndef SECLIB_ERROR_H
#define SECLIB_ERROR_H

#define SEC_OK                                  0x0000

/* CRYPTO */
#define ERR_CRYPTO_INIT_FAIL                    0x1000
#define ERR_CRYPTO_DEINIT_FAIL                  0x1001
#define ERR_CRYPTO_MODE_INVALID                 0x1002
#define ERR_CRYPTO_KEY_INVALID                  0x1003
#define ERR_CRYPTO_DATA_UNALIGNED               0x1004
#define ERR_CRYPTO_SEED_LEN_ERROR               0x1005

/* AUTH */
#define ERR_AUTH_IMAGE_VERIFY_FAIL              0x2000

/* LIB */
#define ERR_LIB_SEC_CFG_NOT_EXIST               0x3000
#define ERR_LIB_VER_INVALID                     0x3001
#define ERR_LIB_SEC_CFG_ERASE_FAIL              0x3002
#define ERR_LIB_SEC_CFG_CANNOT_WRITE            0x3003

/* SECURE DOWNLOAD / IMAGE VERIFICATION */
#define ERR_IMG_VERIFY_THIS_IMG_INFO_NOT_EXIST  0x4000
#define ERR_IMG_VERIFY_HASH_COMPARE_FAIL        0x4001
#define ERR_IMG_VERIFY_NO_SPACE_ADD_IMG_INFO    0x4002
#define ERR_SEC_DL_TOKEN_NOT_FOUND_IN_IMG       0x4003
#define ERR_SEC_DL_FLOW_ERROR                   0x4004

/* IMAGE DOWNLOAD LOCK */
#define ERR_IMG_LOCK_TABLE_NOT_EXIST            0x5000
#define ERR_IMG_LOCK_ALL_LOCK                   0x5001
#define ERR_IMG_LOCK_NO_SPACE_ADD_LOCK_INFO     0x5002
#define ERR_IMG_LOCK_THIS_IMG_INFO_NOT_EXIST    0x5003
#define ERR_IMG_LOCK_MAGIC_ERROR                0x5004

/* KERNEL DRIVER */
#define ERR_KERNEL_CRYPTO_INVALID_MODE          0xA000


#endif /* SECLIB_ERROR_H */

