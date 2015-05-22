#ifndef SEC_AUTH_H
#define SEC_AUTH_H

/**************************************************************************
 * AUTH DATA STRUCTURE
**************************************************************************/
#define RSA_KEY_SIZE                        (128)

/**************************************************************************
*  EXPORT FUNCTION
**************************************************************************/
extern int lib_verify (unsigned char* data_buf,  unsigned int data_len, unsigned char* sig_buf, unsigned int sig_len);
extern void * _memcpy(void *dest, const void *src, int  count);

#endif /* SEC_AUTH_H */                   

