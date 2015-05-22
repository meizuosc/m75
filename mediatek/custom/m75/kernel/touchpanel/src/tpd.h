#ifndef __TPD__
#define __TPD__

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/types.h>
#include <linux/seq_file.h>
#include <linux/list.h>
#include <linux/proc_fs.h> 

#include <mach/mt_gpio.h>
#include <mach/mt_reg_base.h>
#include <mach/mt_typedefs.h>

#include <mach/board.h>
#include <mach/irqs.h>
#include <mach/eint.h>

#include <asm/io.h>
#include <linux/platform_device.h>
#include <generated/autoconf.h>
#include <linux/kobject.h>
#include <linux/earlysuspend.h>

#define TPD_DEBUG_CODE
//#define TPD_DEBUG_TRACK
#define TPD_DMESG(a,arg...) printk(TPD_DEVICE ": " a,##arg)
#if defined(TPD_DEBUG)
#undef TPD_DEBUG
#define TPD_DEBUG(a,arg...) printk(TPD_DEVICE ": " a,##arg)
#else
#define TPD_DEBUG(arg...) 
#endif

//#define __TPD_DEBUG__
#ifdef __TPD_DEBUG__
#define dbg_printk(fmt,args...) \
	  printk("mtk-tpd->%s_%d:"fmt,__func__,__LINE__,##args)
#else 
#define dbg_printk(fmt,args...)
#endif

#define info_printk(fmt,args...) \
	  printk("mtk-tpd->%s_%d:"fmt,__func__,__LINE__,##args)

#define err_printk(fmt,args...) \
		  printk("mtk-tpd->%s_%d:"fmt,__func__,__LINE__,##args)


void* meizu_get_tp_hw_data(void);
int mtk_tpd_register_misc(void);

struct tpd_filter_t
{
	int enable; //0: disable, 1: enable
	int pixel_density; //XXX pixel/cm
};


struct input_param {
	void (*hw_reset)(void) ;
	int (*hw_power_onoff)(int);
	void(*hw_register_irq)(void);
	int  (*hw_irq_enable)(void);
	int  (*hw_irq_disable)(void);
	int  (*hw_get_gpio_value)(int );
	int  (*hw_set_gpio_value)(int,int);
	int  (*lancher_thread)(struct tp_driver_data *,int);
};
struct output_param {
	void * driver_data ;	
	void (*report_func)(void*) ;
	int  (*remove)(struct i2c_client *);
};

struct tp_driver_data {
	struct task_struct * thread ;
	int bootmode ;
	int probed ;/*1=mtk-tpd,2=tp-ic  */
	struct input_param  in ;
	struct output_param out ;
    
};

typedef int (*probe_func_t)(struct i2c_client *,
			 const struct i2c_device_id *,struct tp_driver_data *);

int register_probe_func(probe_func_t  probe_func);

#define GPIO_MIN_INDX (0x0)
#define SDA_GPIO_INDX (0x1)
#define SCL_GPIO_INDX (0x2)
#define IRQ_GPIO_INDX (0x3)
#define RST_GPIO_INDX (0x4)
#define IR_GPIO_INDX  (0x5)
#define GPIO_MAX_INDX (0x6)


/* register, address, configurations */
#define TPD_DEVICE            "mtk-tpd"
#define TPD_X                  0
#define TPD_Y                  1
#define TPD_Z1                 2
#define TPD_Z2                 3
#define TP_DELAY              (2*HZ/100)
#define TP_DRV_MAX_COUNT          (20)
#define TPD_WARP_CNT          (4)
#define TPD_VIRTUAL_KEY_MAX   (10)

/* various mode */
#define TPD_MODE_NORMAL        0
#define TPD_MODE_KEYPAD        1
#define TPD_MODE_SW 2
#define TPD_MODE_FAV_SW 3
#define TPD_MODE_FAV_HW 4
#define TPD_MODE_RAW_DATA 5
#undef TPD_RES_X
#undef TPD_RES_Y
extern int TPD_RES_X;
extern int TPD_RES_Y;
extern int tpd_load_status ; //0: failed, 1: sucess
extern int tpd_mode;
extern int tpd_mode_axis;
extern int tpd_mode_min;
extern int tpd_mode_max;
extern int tpd_mode_keypad_tolerance;
extern int tpd_em_debounce_time;
extern int tpd_em_debounce_time0;
extern int tpd_em_debounce_time1;
extern int tpd_em_asamp;
extern int tpd_em_auto_time_interval;
extern int tpd_em_sample_cnt;
extern int tpd_calmat[];
extern int tpd_def_calmat[];
extern int tpd_calmat[];
extern int tpd_def_calmat[];
extern int TPD_DO_WARP;
extern int tpd_wb_start[];
extern int tpd_wb_end[];

struct tpd_device
{
    struct input_dev *dev;
    struct input_dev *kpd;
    struct timer_list timer;
    struct tasklet_struct tasklet;
    int btn_state;
};

struct tpd_attrs
{
	struct device_attribute **attr;
	int num;
};
struct tpd_driver_t
{
		char *tpd_device_name;
		int (*tpd_local_init)(void);
 		void (*suspend)(struct early_suspend *h);
 		void (*resume)(struct early_suspend *h);
 		int tpd_have_button;
		struct tpd_attrs attrs;
};

#if 1 //#ifdef TPD_HAVE_BUTTON
void tpd_button(unsigned int x, unsigned int y, unsigned int down);
void tpd_button_init(void);
ssize_t tpd_virtual_key(char *buf);
//#ifndef TPD_BUTTON_HEIGHT
//#define TPD_BUTTON_HEIGHT TPD_RES_Y
//#endif
#endif

#if 0
void tpd_adc_init(void);
void tpd_set_debounce_time(int debounce_time);
void tpd_set_spl_number(int spl_num);
u16 tpd_read(int position);
u16 tpd_read_adc(u16 pos);
u16 tpd_read_status(void);
#endif

extern int tpd_driver_add(struct tpd_driver_t *tpd_drv);
extern int tpd_driver_remove(struct tpd_driver_t *tpd_drv);
void tpd_button_setting(int keycnt, void *keys, void *keys_dim);
extern int tpd_em_spl_num;
extern int tpd_em_pressure_threshold;

#ifdef TPD_DEBUG_CODE
#include "tpd_debug.h"
#endif

#ifdef TPD_HAVE_CALIBRATION
#include "tpd_calibrate.h"
#endif

#include "tpd_default.h"

/* switch touch panel into different mode */
void _tpd_switch_single_mode(void);
void _tpd_switch_multiple_mode(void);
void _tpd_switch_sleep_mode(void);
void _tpd_switch_normal_mode(void);   


#endif
