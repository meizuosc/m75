/*
 * volum_up.c
 *
 *  Created on: 2013年12月3日
 *      Author: xiaopeng
 *
 *  Created for volume up key
 *  This file include in mediatek/platform/mt6592/kernel/drivers/keypad/kpd.c
 *  GPIO75 -- external interrupt source
 *  Trigger by edge, both rising and falling
 */

#include "keys_m75.h"

static struct gpio_button_data *gpio_bdata[GPIO_MAX] = {NULL,};

struct gpio_button_data **get_gpio_bdata(void)
{
	return gpio_bdata;
}

struct gpio_button_data *get_gpio_sbdata(GPIO_INDEX which_gpio)
{
	return gpio_bdata[which_gpio];
}

void set_gpio_bdata(struct gpio_button_data *bdata, GPIO_INDEX which_gpio)
{
	struct gpio_button_data **gbdata = get_gpio_bdata();
	/*
	 * related to enum GPIO_INDEX
	 *
	 */
	pr_debug("[%s] key:%s,gpio:%d\n",__func__,bdata->button->desc,which_gpio);

	if( bdata==NULL )
		printk("[%s] bdata==NULL,something wrong\n",__func__);
	switch( which_gpio ) {
	case GPIO_VOLDOWN:
	case GPIO_VOLUP:
	case GPIO_HALL:
		gbdata[which_gpio] = bdata;
		break;
	default:
		printk("[set_gpio_bdata] index:%d is not within the scope of [0,%d]\n", which_gpio, GPIO_MAX);
		break;
	}
	if( gbdata[which_gpio]==NULL ) {
		printk("[set_gpio_bdata] point==NULL init failed!\n");
	}
}

static void kpd_voldown_eint_interrupt_handler(void)
{
	struct gpio_button_data *bdata = get_gpio_sbdata(GPIO_VOLDOWN);
	int press = RELEASED;

	if( bdata==NULL ) {
		mt_eint_unmask(CUST_EINT_KEYDOWN_EINT_NUM);
		printk("[%s] voldown key point is not init ==NULL!\n",__func__);
	} else {
#ifdef DELAYED_WORK
		press = atomic_read(&bdata->key_pressed);
		if(  (press==RELEASED)&&(mt_get_gpio_in(bdata->button->gpio)==1)  ) {
				pr_debug("%s &&&&&&&&&&&&&&&&&& key bounce\n",__func__);
				mt_eint_unmask(CUST_EINT_KEYDOWN_EINT_NUM);
				return;
		}
		mt_eint_set_polarity(bdata->button->irq, press==PRESSED?FALLING:RISING);
		schedule_work(&bdata->work);
#else
		mod_timer(&bdata->timer,
					jiffies + msecs_to_jiffies(bdata->timer_debounce));
#endif
	}

}

static void kpd_volup_eint_interrupt_handler(void)
{
	struct gpio_button_data *bdata = get_gpio_sbdata(GPIO_VOLUP);
	int press = RELEASED;

	if( bdata==NULL ) {
		mt_eint_unmask(CUST_EINT_KEYUP_EINT_NUM);
		printk("[%s] volup key point is not init ==NULL!\n",__func__);
	} else {
#ifdef DELAYED_WORK
		press = atomic_read(&bdata->key_pressed);
		if(  (press==RELEASED)&&(mt_get_gpio_in(bdata->button->gpio)==1)  ) {
				pr_debug("%s &&&&&&&&&&&&&&&&&& key bounce\n",__func__);
				mt_eint_unmask(CUST_EINT_KEYUP_EINT_NUM);
				return;
		}
		mt_eint_set_polarity(bdata->button->irq, press==PRESSED?FALLING:RISING);
		schedule_work(&bdata->work);

#else
		mod_timer(&bdata->timer,
					jiffies + msecs_to_jiffies(bdata->timer_debounce));
#endif
	}

}

static void kpd_hall_eint_interrupt_handler(void)
{
	struct gpio_button_data *bdata = get_gpio_sbdata(GPIO_HALL);
	if( bdata==NULL ) {
		mt_eint_unmask(CUST_EINT_HALL_INT_NUM);
		printk("[%s] hall key point is not init ==NULL!\n",__func__);
	} else {
#ifdef DELAYED_WORK
		mt_eint_set_polarity(bdata->button->irq, atomic_read(&bdata->key_pressed)==CLOSED?LOW:HIGH);
		schedule_work(&bdata->work);
#else
		mod_timer(&bdata->timer,
					jiffies + msecs_to_jiffies(bdata->timer_debounce));
#endif
	}
}

#if 0
static void kpd_pwrkey_eint_interrupt_handler(unsigned long pressed)
{
	struct gpio_button_data **bdata = get_gpio_bdata();
	if( bdata[GPIO_PWR]==NULL ) {
		printk("[%s] pwrkey point==NULL! maybe too early\n",__func__);
	} else {
		//kpd_eint_interrupt_handler(bdata[GPIO_POWER]);
		/*
		 * the PWRKEY is stable now,theres no need to set de-bounce time
		 */
		input_report_key(bdata[GPIO_PWR]->input, bdata[GPIO_PWR]->button->code, pressed);
		input_sync(bdata[GPIO_PWR]->input);
	}
}

/*
 * the pwrkey driver have been located in mediatek/kernel/driver/keypad/kpd.c
 */
void kpd_pwrkey_pmic_handler(unsigned long pressed)
{
	printk("[kpd] Power Key generate, pressed=%ld\n", pressed);
	kpd_pwrkey_eint_interrupt_handler(pressed);
}
#endif

/*
 * Init de-bounce timer
 * Set GPIO direction...
 * Register interrupt function
 * Enable interrupt
 */
void  external_kpd_eint_init(void)
{
    mt_set_gpio_mode(GPIO_KEY_VOLDOWN_EINT_PIN, GPIO_KEY_VOLDOWN_EINT_PIN_M_EINT);
    mt_set_gpio_dir(GPIO_KEY_VOLDOWN_EINT_PIN, GPIO_DIR_IN);
    mt_set_gpio_pull_enable(GPIO_KEY_VOLDOWN_EINT_PIN, GPIO_PULL_ENABLE);
    mt_set_gpio_pull_select(GPIO_KEY_VOLDOWN_EINT_PIN, GPIO_PULL_UP);
//	mt_eint_set_sens(CUST_EINT_KEYDOWN_EINT_NUM, MT_LEVEL_SENSITIVE);
//	mt_eint_set_hw_debounce(CUST_EINT_KEYDOWN_EINT_NUM, 100);
    mt_eint_registration(CUST_EINT_KEYDOWN_EINT_NUM, CUST_EINT_KEYDOWN_EINT_TYPE, kpd_voldown_eint_interrupt_handler, 0);
    mt_eint_unmask(CUST_EINT_KEYDOWN_EINT_NUM);

    mt_set_gpio_mode(GPIO_KEY_VOLUP_EINT_PIN, GPIO_KEY_VOLUP_EINT_PIN_M_EINT);
    mt_set_gpio_dir(GPIO_KEY_VOLUP_EINT_PIN, GPIO_DIR_IN);
    mt_set_gpio_pull_enable(GPIO_KEY_VOLUP_EINT_PIN, GPIO_PULL_ENABLE);
    mt_set_gpio_pull_select(GPIO_KEY_VOLUP_EINT_PIN, GPIO_PULL_UP);
//	mt_eint_set_sens(CUST_EINT_KEYUP_EINT_NUM, MT_LEVEL_SENSITIVE);
//	mt_eint_set_hw_debounce(CUST_EINT_KEYUP_EINT_NUM, 100);
    mt_eint_registration(CUST_EINT_KEYUP_EINT_NUM, CUST_EINT_KEYUP_EINT_TYPE, kpd_volup_eint_interrupt_handler, 0);
    mt_eint_unmask(CUST_EINT_KEYUP_EINT_NUM);
	
	mt_set_gpio_mode(GPIO_HALL_SWITH_EINT_PIN, GPIO_HALL_SWITH_EINT_PIN_M_EINT);
    mt_set_gpio_dir(GPIO_HALL_SWITH_EINT_PIN, GPIO_DIR_IN);
    mt_set_gpio_pull_enable(GPIO_HALL_SWITH_EINT_PIN, GPIO_PULL_ENABLE);
    mt_set_gpio_pull_select(GPIO_HALL_SWITH_EINT_PIN, GPIO_PULL_UP);
	mt_eint_set_sens(CUST_EINT_HALL_INT_NUM, MT_LEVEL_SENSITIVE);
	mt_eint_set_hw_debounce(CUST_EINT_HALL_INT_NUM, CUST_EINT_HALL_INT_DEBOUNCE_CN);
    mt_eint_registration(CUST_EINT_HALL_INT_NUM, CUST_EINT_HALL_INT_TYPE, kpd_hall_eint_interrupt_handler, 0);
    mt_eint_unmask(CUST_EINT_HALL_INT_NUM);
	
    pr_debug(" external_kpd_eint_init: end here!\n");
}

#if 0
/*
 * This module will be loaded after module gpio_keys_init,
 * and it's More reasonable.
 */
static void __exit external_kpd_eint_exit(void)
{
	printk("[%s]\n",__func__);
	mt_eint_mask(CUST_EINT_KEYDOWN_EINT_NUM);
	mt_eint_mask(CUST_EINT_KEYUP_EINT_NUM);
	mt_eint_mask(CUST_EINT_HALL_INT_NUM);
}

late_initcall(external_kpd_eint_init);
module_exit(external_kpd_eint_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("bsp@meizu.com");
MODULE_DESCRIPTION("GPIO KEYS");
MODULE_ALIAS("platform:gpio");

#endif
