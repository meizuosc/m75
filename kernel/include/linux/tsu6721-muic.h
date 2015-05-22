#ifndef __LINUX_TSU6721_H
#define __LINUX_TSU6721_H

#include <mach/charging.h>
#define DEV_NAME	"tsu6721-muic"
#define TSU6721_DEV_ID	0x12

/* define the tsu6721 registers */
#define TSU6721_MUIC_DEVICE_ID		0x01
#define TSU6721_MUIC_CONTROL		0x02
#define TSU6721_MUIC_INT1		0x03
#define TSU6721_MUIC_INT2		0x04
#define TSU6721_MUIC_INTMASK1		0x05
#define TSU6721_MUIC_INTMASK2		0x06
#define TSU6721_MUIC_ADC		0x07
#define TSU6721_MUIC_TIMING_SET1	0x08
#define TSU6721_MUIC_TIMING_SET2	0x09
#define TSU6721_MUIC_DEVICE_TYPE1	0x0a
#define TSU6721_MUIC_DEVICE_TYPE2	0x0b
#define TSU6721_MUIC_MANUAL_SW1		0x13
#define TSU6721_MUIC_MANUAL_SW2		0x14
#define TSU6721_MUIC_DEVICE_TYPE3	0x15
#define TSU6721_MUIC_TIMER_SET		0x20

/* CONTROL 0x02 */
#define INT_MASK_SHIFT   0
#define INT_MASK	1 << 0
#define MANUAL_SWITCH_SHIFT	2
#define MANUAL_SWTICH_MASK	1 << 2
#define SWITCH_OPEN_SHIFT	4
#define SWTICH_OPEN_MASK	1 << 4

/* Interrupt Mask 1*/
#define ATTACH_SHIFT	0
#define ATTACH_MASK	1 << 0
#define DETACH_SHIFT	1
#define DETACH_MASK	1 << 1
#define	OVP_SHIFT	5
#define OVP_MASK	1 << 5
#define	OCP_SHIFT	6
#define	OCP_MASK	1 << 6
#define	OVP_OCP_OTP_DIS_SHIFT	7
#define	OVP_OCP_OTP_DIS_MASK 	1 << 7

/* Interrupt Mask 2*/
#define REVERSE_ATTACH_SHIFT	1
#define REVERSE_ATTACH_MASK	1 << 1
#define ADC_CHANGE_SHIFT	2
#define ADC_CHANGE_MASK		1 << 2
#define VBUS_SHIFT		7
#define VBUS_MASK		1 << 7

#define DP_SWITCH_SHIFT 4
#define DM_SWITCH_SHIFT 7
#define DP_SWITCH_MASK	7 << 4
#define DM_SWITCH_MASK	7 << 7

struct tsu6721_muic_info {
        struct device *dev;
        struct i2c_client *client;
        struct mutex iolock;
        struct delayed_work irq_dwork;
        int adc_type;
        int host_insert;
        int irq;
        int vb_valid;
        int usb_switch;
        struct mutex tsu6721_i2c_access;
	struct regulator *reverse;
	CHARGER_TYPE charger_type;
};

extern void tsu6721_charger_type(void *data);
#endif /* __LINUX_TSU6721_H */
