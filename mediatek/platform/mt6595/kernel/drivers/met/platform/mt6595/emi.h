#ifndef _EMI_H_
#define _EMI_H_

#include <linux/device.h>

#if 0
int emi_reg(struct device *this_device);
void emi_unreg(struct device *this_device);

void emi_init(void);
void emi_uninit(void);

void emi_start(void);
void emi_stop(void);

int do_emi(void);
unsigned int emi_polling(unsigned int *emi_value);
#endif
#endif // _EMI_H_
