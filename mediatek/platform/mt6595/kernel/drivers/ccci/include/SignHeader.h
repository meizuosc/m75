#ifndef _SECIMGHEADER_H
#define _SECIMGHEADER_H

/**************************************************************************
 *  SEC IMAGE HEADER FORMAT
 **************************************************************************/

#define CUSTOM_NAME                  "CUSTOM_NAME"
#define IMAGE_VERSION                "IMAGE_VERSION"
 
#define SEC_IMG_MAGIC                (0x53535353)

//#define SEC_IMG_HEADER_SIZE          (128)
#define SEC_IMG_HEADER_SIZE          (64)

#define HASH_SIG_LEN                 (20+128) /* hash + signature */


/* in order to speedup verification, you can customize the image size 
   which should be signed and checked at boot time */
#define VERIFY_OFFSET                "VERIFY_OFFSET"
#define VERIFY_LENGTH                "VERIFY_LENGTH"

 
typedef struct _SEC_IMG_HEADER
{
    unsigned int magic_number;
    
    //unsigned char cust_name [32];
    char cust_name [16];
    unsigned int image_version;
    unsigned int image_length;  
    unsigned int image_offset;
    
    unsigned int sign_offset;
    unsigned int sign_length;
    
    unsigned int signature_offset;
    unsigned int signature_length;

    //unsigned char dummy[64];
    unsigned char dummy[16];

}SEC_IMG_HEADER;

#endif   /*_SECIMGHEADER_H*/
