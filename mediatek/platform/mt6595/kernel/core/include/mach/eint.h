#ifndef __EINT_H
#define __EINT_H

#include "mt_reg_base.h"


/*
 * Define hardware registers.
 */
#define EINT_STA_BASE         ((APIRQ_BASE + 0x000))
#define EINT_INTACK_BASE      ((APIRQ_BASE + 0x040))
#define EINT_MASK_BASE        ((APIRQ_BASE + 0x080))
#define EINT_MASK_SET_BASE    ((APIRQ_BASE + 0x0c0))
#define EINT_MASK_CLR_BASE    ((APIRQ_BASE + 0x100))
#define EINT_SENS_BASE        ((APIRQ_BASE + 0x140))
#define EINT_SENS_SET_BASE    ((APIRQ_BASE + 0x180))
#define EINT_SENS_CLR_BASE    ((APIRQ_BASE + 0x1c0))
#define EINT_SOFT_BASE        ((APIRQ_BASE + 0x200))
#define EINT_SOFT_SET_BASE    ((APIRQ_BASE + 0x240))
#define EINT_SOFT_CLR_BASE    ((APIRQ_BASE + 0x280))
#define EINT_POL_BASE         ((APIRQ_BASE + 0x300))
#define EINT_POL_SET_BASE     ((APIRQ_BASE + 0x340))
#define EINT_POL_CLR_BASE     ((APIRQ_BASE + 0x380))
#define EINT_D0_EN_BASE       ((APIRQ_BASE + 0x400))
#define EINT_DBNC_BASE        ((APIRQ_BASE + 0x500))
#define EINT_DBNC_SET_BASE    ((APIRQ_BASE + 0x600))
#define EINT_DBNC_CLR_BASE    ((APIRQ_BASE + 0x700))
#define DEINT_CON_BASE        ((APIRQ_BASE + 0x800))
#define DEINT_SEL_BASE        ((APIRQ_BASE + 0x840))
#define DEINT_SEL_SET_BASE    ((APIRQ_BASE + 0x880))
#define DEINT_SEL_CLR_BASE    ((APIRQ_BASE + 0x8c0))
#define EINT_EEVT_BASE	      ((APIRQ_BASE + 0x900))
#define EINT_EMUL_BASE        ((APIRQ_BASE + 0xF00))
#define EINT_RAW_STA_BASE     ((APIRQ_BASE + 0xA00))

/*
 * Define control bits.
 */
#define EINT_DBNC_SET_DBNC_BITS    (4)
#define EINT_DBNC_CLR_DBNC_BITS    (4)
#define EINT_DBNC_SET_EN_BITS      (0)
#define EINT_DBNC_CLR_EN_BITS      (0)
#define EINT_DBNC_SET_RST_BITS     (1)

#define EINT_DBNC_SET_EN           (0x1)
#define EINT_DBNC_CLR_EN           (0x1)

#define EINT_DBNC_EN_BIT           (0x1)
#define EINT_DBNC_RST_BIT          (0x1)

#define EINT_DBNC_0_MS             (0x7)
#define EINT_DBNC                  (0x7)


#define EINT_STA_DEFAULT	0x00000000
#define EINT_INTACK_DEFAULT	0x00000000
#define EINT_EEVT_DEFAULT	0x00000001
#define EINT_MASK_DEFAULT	0x00000000
#define EINT_MASK_SET_DEFAULT	0x00000000
#define EINT_MASK_CLR_DEFAULT	0x00000000
#define EINT_SENS_DEFAULT	0x0000FFFF
#define EINT_SENS_SET_DEFAULT	0x00000000
#define EINT_SENS_CLR_DEFAULT	0x00000000
#define EINT_D0EN_DEFAULT	0x00000000
#define EINT_D1EN_DEFAULT	0x00000000
#define EINT_D2EN_DEFAULT	0x00000000
#define EINT_DBNC_DEFAULT(n)	0x00000000
#define DEINT_MASK_DEFAULT      0x00000000
#define DEINT_MASK_SET_DEFAULT  0x00000000
#define DEINT_MASK_CLR_DEFAULT  0x00000000


/*
 * Define constants.
 */
#define EINT_IRQ_BASE            NR_MT_IRQ_LINE
#define EINT_AP_MAXNUMBER 192
#define EINT_MAX_CHANNEL 192
#define MAX_HW_DEBOUNCE_CNT 32
#define DEINT_MAX_CHANNEL 8

#define EINT_CTRL_REG_NUMBER    ((EINT_MAX_CHANNEL + 31) / 32)

#define MT_EINT_POL_NEG (0)
#define MT_EINT_POL_POS (1)

#define EINTF_TRIGGER_RISING     0x00000001
#define EINTF_TRIGGER_FALLING    0x00000002
#define EINTF_TRIGGER_HIGH       0x00000004
#define EINTF_TRIGGER_LOW        0x00000008

#define EINT_IRQ(eint)           (eint+EINT_IRQ_BASE)
#define DEMUX_EINT_IRQ(irq)       (irq-EINT_IRQ_BASE)
#define EINT_GPIO(eint)             eint
/*
 * Define function prototypes.
 */
extern void mt_eint_mask(unsigned int eint_num);
extern void mt_eint_unmask(unsigned int eint_num);
extern void mt_eint_set_hw_debounce(unsigned int eint_num, unsigned int ms);
extern void mt_eint_set_polarity(unsigned int eint_num, unsigned int pol);
extern unsigned int mt_eint_set_sens(unsigned int eint_num, unsigned int sens);
extern void mt_eint_registration(unsigned int eint_num, unsigned int flow, void (EINT_FUNC_PTR)(void), unsigned int is_auto_umask);
extern void mt_eint_print_status(void);
extern int mt_gpio_set_debounce(unsigned gpio, unsigned debounce);
extern int mt_gpio_to_irq(unsigned gpio);
#endif
