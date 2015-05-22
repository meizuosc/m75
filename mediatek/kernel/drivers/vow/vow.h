#ifndef __VOW_H__
#define __VOW_H__



//#include "cust_matv.h"
#define VOW_DEVNAME "vow"
#define VOW_IOC_MAGIC    'a'

//below is control message
#define TEST_VOW_PRINT         _IO(VOW_IOC_MAGIC,  0x00)
#define VOWEINT_GET_BUFSIZE    _IOW(VOW_IOC_MAGIC, 0x01, unsigned int)
#define VOW_GET_STATUS         _IOW(VOW_IOC_MAGIC, 0x02, unsigned int)
#define VOW_SET_STATUS         _IOW(VOW_IOC_MAGIC, 0x03, unsigned int)

struct VOW_EINT_DATA_STRUCT{
   int size;        // size of data section
   int eint_status; // eint status
   char *data;      // reserved for future extension
}VOW_EINT_DATA_STRUCT;

enum VOW_EINT_STATUS{
    VOW_EINT_DISABLE = -2,
    VOW_EINT_FAIL = -1,
    VOW_EINT_PASS = 0,
    VOW_EINT_RETRY = 1,
    NUM_OF_VOW_EINT_STATUS
};

enum VOW_PWR_STATUS{
    VOW_PWR_OFF = 0,
    VOW_PWR_ON = 1,
    VOW_PWR_RESET = 2,
    NUM_OF_VOW_PWR_STATUS
};

#endif //__VOW_H__
