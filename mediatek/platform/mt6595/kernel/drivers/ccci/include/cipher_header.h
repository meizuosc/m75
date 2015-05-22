#ifndef _CIPHERHEADER_H
#define _CIPHERHEADER_H

/**************************************************************************
 *  CIPHER HEADER FORMAT
 **************************************************************************/

#define CUSTOM_NAME                  "CUSTOM_NAME"
#define IMAGE_VERSION                "IMAGE_VERSION"
#define CIPHER_IMG_MAGIC             (0x63636363U)
#define CIPHER_IMG_HEADER_SIZE       (128)
#define CIPHER_BLOCK_SIZE          (16)
typedef struct _SEC_IMG_HEADER_CIPHER
{
    unsigned int magic_number;
    
    unsigned char cust_name [32];
    unsigned int image_version;
    unsigned int image_length;  
    unsigned int image_offset;
    
    unsigned int cipher_offset;
    unsigned int cipher_length;    

    unsigned char dummy[72];

}CIPHER_HEADER;

/**************************************************************************
 *  EXPORTED FUNCTIONS
 **************************************************************************/
extern int sec_aes_test(void);
extern int sec_aes_init(void);
extern int lib_aes_enc(unsigned char* input_buf,  unsigned int input_len, unsigned char* output_buf, unsigned int output_len);
extern int lib_aes_dec(unsigned char* input_buf,  unsigned int input_len, unsigned char* output_buf, unsigned int output_len);

#endif   /*_CIPHERHEADER_H*/



