#include <xhci.h>
#include <linux/xhci/xhci-mtk.h>
#include <linux/xhci/xhci-mtk-power.h>
#include <linux/xhci/xhci-mtk-scheduler.h>
#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/wakelock.h>
#ifdef CONFIG_USB_MTK_DUALMODE
#include <mach/eint.h>
#include <linux/irq.h>
#include <linux/switch.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <mach/mt_boot.h>
#undef DRV_Reg32
#undef DRV_WriteReg32
#include <mach/battery_meter.h>
#ifdef CONFIG_USBIF_COMPLIANCE
#define MTK_OTG_PMIC_BOOST_5V
#endif
#ifdef MTK_OTG_PMIC_BOOST_5V
#undef DRV_Reg32
#undef DRV_WriteReg32
#include <mach/upmu_common.h>
#include <mach/mt_gpio.h>
#include <cust_gpio_usage.h>
#endif
#include <linux/of.h>
#include <linux/of_address.h>
#ifdef CONFIG_PROJECT_PHY
#include <linux/mu3phy/mtk-phy-asic.h>
#endif
#endif
#ifdef CONFIG_MTK_LDVT
#undef DRV_Reg32
#undef DRV_WriteReg32
#include <mach/mt_gpio.h>
#endif
#include <hub.h>
#include <linux/jiffies.h>
#include <linux/list.h>
#include <asm/atomic.h>
#include <linux/usb/hcd.h>

#define mtk_xhci_mtk_log(fmt, args...) \
    pr_debug("%s(%d): " fmt, __func__, __LINE__, ##args)

#define RET_SUCCESS 0
#define RET_FAIL 1

struct xhci_hcd *mtk_xhci;
static spinlock_t *mtk_hub_event_lock;
static struct list_head* mtk_hub_event_list;
static int mtk_ep_count;

static struct wake_lock mtk_xhci_wakelock;

#ifdef CONFIG_USB_MTK_DUALMODE
enum idpin_state {
	IDPIN_OUT,
	IDPIN_IN_HOST,
	IDPIN_IN_DEVICE,
};

static int mtk_idpin_irqnum;
static enum idpin_state mtk_idpin_cur_stat = IDPIN_OUT;
static struct switch_dev mtk_otg_state;

static struct delayed_work mtk_xhci_delaywork;
int mtk_iddig_debounce = 10;
module_param(mtk_iddig_debounce, int, 0644);

static void mtk_set_iddig_out_detect(void)
{
	irq_set_irq_type(mtk_idpin_irqnum, IRQF_TRIGGER_HIGH);
	enable_irq(mtk_idpin_irqnum);
}

static void mtk_set_iddig_in_detect(void)
{
	irq_set_irq_type(mtk_idpin_irqnum, IRQF_TRIGGER_LOW);
	enable_irq(mtk_idpin_irqnum);
}

static bool mtk_is_charger_4_vol(void)
{
	int vol = battery_meter_get_charger_voltage();
	//mtk_xhci_mtk_log("voltage(%d)\n", vol);

#ifdef CONFIG_USBIF_COMPLIANCE
	return false ;
#else
	return (vol > 4000) ? true : false;
#endif
}

bool mtk_is_usb_id_pin_short_gnd(void)
{
	return (mtk_idpin_cur_stat != IDPIN_OUT) ? true : false;
}

#ifdef MTK_OTG_PMIC_BOOST_5V
#define PMIC_REG_BAK_NUM (10)

extern U32 pmic_read_interface(U32 RegNum, U32 * val, U32 MASK, U32 SHIFT);
extern U32 pmic_config_interface(U32 RegNum, U32 val, U32 MASK, U32 SHIFT);

U32 pmic_bak_regs[PMIC_REG_BAK_NUM][2] = {
		{0x8D22, 0}, {0x8D14, 0}, {0x803C, 0}, {0x8036, 0}, {0x8D24, 0},
		{0x8D16, 0}, {0x803A, 0}, {0x8046, 0}, {0x803E, 0}, {0x8044, 0}
	};

static void pmic_save_regs(void){
	int i;

	for(i = 0; i < PMIC_REG_BAK_NUM; i++){
		pmic_read_interface(pmic_bak_regs[i][0], &pmic_bak_regs[i][1], 0xffffffff, 0);
	}
}

static void pmic_restore_regs(void){
	int i;

	for(i = 0; i < PMIC_REG_BAK_NUM; i++){
		pmic_config_interface(pmic_bak_regs[i][0], pmic_bak_regs[i][1], 0xffffffff, 0);
	}
}

static void mtk_enable_pmic_otg_mode(void)
{
	int val;

	mt_set_gpio_mode(GPIO_OTG_DRVVBUS_PIN, GPIO_MODE_GPIO);
	mt_set_gpio_pull_select(GPIO_OTG_DRVVBUS_PIN, GPIO_PULL_DOWN);
	mt_set_gpio_pull_enable(GPIO_OTG_DRVVBUS_PIN, GPIO_PULL_ENABLE);

	/* save PMIC related registers */
	pmic_save_regs();

	pmic_config_interface(0x8D22, 0x1, 0x1, 12);
	pmic_config_interface(0x8D14, 0x1, 0x1, 12);
	pmic_config_interface(0x803C, 0x3, 0x3, 0);
	pmic_config_interface(0x803C, 0x2, 0x3, 2);
	pmic_config_interface(0x803C, 0x1, 0x1, 14);
	pmic_config_interface(0x8036, 0x0, 0x0, 0);
	pmic_config_interface(0x8D24, 0xf, 0xf, 12);
	pmic_config_interface(0x8D16, 0x1, 0x1, 15);
	pmic_config_interface(0x803A, 0x1, 0x1, 6);
	pmic_config_interface(0x8046, 0x00A0, 0xffff, 0);
	pmic_config_interface(0x803E, 0x1, 0x1, 2);
	pmic_config_interface(0x803E, 0x1, 0x1, 3);
	pmic_config_interface(0x803E, 0x3, 0x3, 8);
	pmic_config_interface(0x803E, 0x0, 0x1, 10);
	pmic_config_interface(0x8044, 0x3, 0x3, 0);
	pmic_config_interface(0x8044, 0x3, 0x7, 8);
	pmic_config_interface(0x8044, 0x1, 0x1, 11);

	pmic_config_interface(0x809C, 0x8000, 0xFFFF, 0);

	val = 0;
	while (val == 0) {
		pmic_read_interface(0x809A, &val, 0x1, 15);
	}

	pmic_config_interface(0x8084, 0x1, 0x1, 0);
	mdelay(50);

	val = 0;
	while (val == 0) {
		pmic_read_interface(0x8060, &val, 0x1, 14);
	}

	mtk_xhci_mtk_log("set pmic power on, done\n");
}

static void mtk_disable_pmic_otg_mode(void)
{
	int val;

	pmic_config_interface(0x8068, 0x0, 0x1, 0);
	pmic_config_interface(0x8084, 0x0, 0x1, 0);
	mdelay(50);
	pmic_config_interface(0x8068, 0x0, 0x1, 1);

	val = 1;
	while (val == 1) {
		pmic_read_interface(0x805E, &val, 0x1, 4);
	}

	#if 0
	pmic_config_interface(0x809E, 0x8000, 0xFFFF, 0);

	val = 1;
	while (val == 1) {
		pmic_read_interface(0x809A, &val, 0x1, 15);
	}
	#endif

	/* restore PMIC registers */
	pmic_restore_regs();

	mtk_xhci_mtk_log("set pimc power off, done\n");
}

#endif

void mtk_hub_event_steal(spinlock_t *lock, struct list_head* list){
	mtk_hub_event_lock = lock;
	mtk_hub_event_list = list;
}

void mtk_ep_count_inc(){
	mtk_ep_count++;
}

void mtk_ep_count_dec(){
	mtk_ep_count--;
}

int mtk_is_hub_active(void){
	struct usb_hcd *hcd = xhci_to_hcd(mtk_xhci);
	struct usb_device *rhdev = hcd->self.root_hub;
	struct usb_hub *hub = usb_hub_to_struct_hub(rhdev);

	bool ret = true;

	spin_lock_irq(mtk_hub_event_lock);
	if((mtk_ep_count == 0) && (list_empty(&hub->event_list) == 1) && (atomic_read(&(hub->kref.refcount)) == 1)){
		ret = false;
	}
	spin_unlock_irq(mtk_hub_event_lock);

	return ret;
}


static int mtk_xhci_hcd_init(void)
{
	int retval;

	mtk_xhci_ip_init();

	retval = xhci_register_plat();
	if (retval < 0) {
		printk(KERN_DEBUG "Problem registering platform driver.");
		return retval;
	}
	retval = xhci_attrs_init();
	if (retval < 0) {
		printk(KERN_DEBUG "Problem creating xhci attributes.");
		goto unreg_plat;
	}
	/*
	 * Check the compiler generated sizes of structures that must be laid
	 * out in specific ways for hardware access.
	 */
	BUILD_BUG_ON(sizeof(struct xhci_doorbell_array) != 256 * 32 / 8);
	BUILD_BUG_ON(sizeof(struct xhci_slot_ctx) != 8 * 32 / 8);
	BUILD_BUG_ON(sizeof(struct xhci_ep_ctx) != 8 * 32 / 8);
	/* xhci_device_control has eight fields, and also
	 * embeds one xhci_slot_ctx and 31 xhci_ep_ctx
	 */
	BUILD_BUG_ON(sizeof(struct xhci_stream_ctx) != 4 * 32 / 8);
	BUILD_BUG_ON(sizeof(union xhci_trb) != 4 * 32 / 8);
	BUILD_BUG_ON(sizeof(struct xhci_erst_entry) != 4 * 32 / 8);
	BUILD_BUG_ON(sizeof(struct xhci_cap_regs) != 7 * 32 / 8);
	BUILD_BUG_ON(sizeof(struct xhci_intr_reg) != 8 * 32 / 8);
	/* xhci_run_regs has eight fields and embeds 128 xhci_intr_regs */
	BUILD_BUG_ON(sizeof(struct xhci_run_regs) != (8 + 8 * 128) * 32 / 8);
	return 0;

unreg_plat:
	xhci_unregister_plat();
	return retval;
}

static void mtk_xhci_hcd_cleanup(void)
{
	xhci_unregister_plat();
	xhci_attrs_exit();
}

static void mtk_xhci_imod_set(u32 imod)
{
	u32 temp;

	temp = xhci_readl(mtk_xhci, &mtk_xhci->ir_set->irq_control);
	temp &= ~0xFFFF;
	temp |= imod;
	xhci_writel(mtk_xhci, temp, &mtk_xhci->ir_set->irq_control);
}

static int mtk_xhci_driver_load(void)
{
	int ret = 0;

	/* recover clock/power setting and deassert reset bit of mac */
	usb_phy_recover(0);
	writel(readl((void __iomem *)SSUSB_IP_PW_CTRL) & (~SSUSB_IP_SW_RST),
	       (void __iomem *)SSUSB_IP_PW_CTRL);
	ret = mtk_xhci_hcd_init();
	if (ret || !mtk_xhci) {
		ret = -EFAULT;
		goto _err;
	}

	/* for performance, fixed the interrupt moderation from 0xA0(default) to 0x30 */
	mtk_xhci_imod_set(0x30);

#ifdef CONFIG_USBIF_COMPLIANCE
	mtk_enable_pmic_otg_mode();
	enableXhciAllPortPower(mtk_xhci);
#else
#ifdef MTK_OTG_PMIC_BOOST_5V
	mtk_enable_pmic_otg_mode();
#else
	enableXhciAllPortPower(mtk_xhci);
#endif
#endif
	return 0;

_err:
	mtk_xhci_mtk_log("ret(%d), mtk_xhci(0x%p)\n", ret, mtk_xhci);
	writel(readl((void __iomem *)SSUSB_IP_PW_CTRL) & (SSUSB_IP_SW_RST),
	       (void __iomem *)SSUSB_IP_PW_CTRL);
	usb_phy_savecurrent(1);
	return ret;
}

static void mtk_xhci_disPortPower(void)
{
#ifdef CONFIG_USBIF_COMPLIANCE
	mtk_disable_pmic_otg_mode();
	disableXhciAllPortPower(mtk_xhci);
#else
	#ifdef MTK_OTG_PMIC_BOOST_5V
	mtk_disable_pmic_otg_mode();
	#else
	disableXhciAllPortPower(mtk_xhci);
	#endif
#endif
}

static void mtk_xhci_driver_unload(void)
{
	mtk_xhci_hcd_cleanup();

	mtk_xhci = NULL;
	writel(readl((void __iomem *)SSUSB_IP_PW_CTRL) | (SSUSB_IP_SW_RST),
	       (void __iomem *)SSUSB_IP_PW_CTRL);
	/* close clock/power setting and assert reset bit of mac */
	usb_phy_savecurrent(1);
}

void mtk_xhci_switch_init(void)
{
	mtk_otg_state.name = "otg_state";
	mtk_otg_state.index = 0;
	mtk_otg_state.state = 0;

#ifndef CONFIG_USBIF_COMPLIANCE
	if (switch_dev_register(&mtk_otg_state))
		printk("switch_dev_register fail\n");
	else
		mtk_xhci_mtk_log("switch_dev register success\n");
#endif
}

void mtk_xhci_mode_switch(struct work_struct *work)
{
	static bool is_load = false;
	static bool is_pwoff = false;
	int ret = 0;

	if (mtk_idpin_cur_stat == IDPIN_OUT) {
		is_load = false;

		/* expect next isr is for id-pin out action */
		mtk_idpin_cur_stat = (mtk_is_charger_4_vol())? IDPIN_IN_DEVICE : IDPIN_IN_HOST;
		/* make id pin to detect the plug-out */
		mtk_set_iddig_out_detect();

		if (mtk_idpin_cur_stat == IDPIN_IN_DEVICE)
			goto done;

		ret = mtk_xhci_driver_load();
		if (!ret) {
			is_load = true;
			mtk_xhci_wakelock_lock();
#ifndef CONFIG_USBIF_COMPLIANCE
			switch_set_state(&mtk_otg_state, 1);
#endif
		}

	} else {		/* IDPIN_OUT */
		if (is_load) {
			if(!is_pwoff)
				mtk_xhci_disPortPower();

			if(mtk_is_hub_active()){
				is_pwoff = true;
				schedule_delayed_work_on(0, &mtk_xhci_delaywork, msecs_to_jiffies(mtk_iddig_debounce));
				mtk_xhci_mtk_log("wait, hub is still active, ep cnt %d !!!\n", mtk_ep_count);
				return;
			}

			mtk_xhci_driver_unload();
			is_pwoff = false;
			is_load = false;
#ifndef CONFIG_USBIF_COMPLIANCE
			switch_set_state(&mtk_otg_state, 0);
#endif
			mtk_xhci_wakelock_unlock();
		}

		/* expect next isr is for id-pin in action */
		mtk_idpin_cur_stat = IDPIN_OUT;
		/* make id pin to detect the plug-in */
		mtk_set_iddig_in_detect();
	}

done:
	mtk_xhci_mtk_log("current mode is %s, ret(%d), switch(%d)\n",
			 (mtk_idpin_cur_stat == IDPIN_IN_HOST) ? "host" :
			 (mtk_idpin_cur_stat == IDPIN_IN_DEVICE) ? "id_device" : "device",
			 ret, mtk_otg_state.state);
}

static irqreturn_t xhci_eint_iddig_isr(int irqnum, void *data)
{
	int ret;
	//schedule_delayed_work(&mtk_xhci_delay_work, mtk_iddig_debounce*HZ/1000);
	/* microseconds */
	ret = schedule_delayed_work_on(0, &mtk_xhci_delaywork, msecs_to_jiffies(mtk_iddig_debounce));
	mtk_xhci_mtk_log("schedule to delayed work, ret(%d)\n", ret);

	disable_irq_nosync(irqnum);
	return IRQ_HANDLED;
}

int mtk_xhci_eint_iddig_init(void)
{
	int retval;

	INIT_DELAYED_WORK(&mtk_xhci_delaywork, mtk_xhci_mode_switch);

	mtk_idpin_irqnum = mt_gpio_to_irq(IDDIG_EINT_PIN);

	/* 50 microseconds < the hub stable time is 100 ms */
	mt_gpio_set_debounce(IDDIG_EINT_PIN, 50);

	retval =
	    request_irq(mtk_idpin_irqnum, xhci_eint_iddig_isr, IRQF_TRIGGER_LOW, "iddig_eint",
			NULL);
	if (retval != 0) {
		printk("%s(%d): request_irq fail, ret %d, irqnum %d!!!\n", __func__,
				 __LINE__, retval, mtk_idpin_irqnum);
		return retval;
	}
	mtk_xhci_mtk_log("external iddig register done, irqnum = %d\n", mtk_idpin_irqnum);

	/* set in-detect and umask the iddig interrupt */
	//enable_irq(mtk_idpin_irqnum);
	return retval;
}

void mtk_xhci_eint_iddig_deinit(void)
{
	//mt_eint_registration(IDDIG_EINT_PIN, EINTF_TRIGGER_LOW, NULL, false);
	disable_irq_nosync(mtk_idpin_irqnum);

	free_irq(mtk_idpin_irqnum, NULL);

	cancel_delayed_work(&mtk_xhci_delaywork);

	mtk_idpin_cur_stat = IDPIN_OUT;

	mtk_xhci_mtk_log("external iddig unregister done.\n");
}

#endif

void mtk_xhci_wakelock_init(void)
{
	wake_lock_init(&mtk_xhci_wakelock, WAKE_LOCK_SUSPEND, "xhci.wakelock");
}

void mtk_xhci_wakelock_lock(void)
{
	if (!wake_lock_active(&mtk_xhci_wakelock))
		wake_lock(&mtk_xhci_wakelock);
	mtk_xhci_mtk_log("done\n");
}

void mtk_xhci_wakelock_unlock(void)
{
	if (wake_lock_active(&mtk_xhci_wakelock))
		wake_unlock(&mtk_xhci_wakelock);
	mtk_xhci_mtk_log("done\n");
}

void mtk_xhci_set(struct xhci_hcd *xhci)
{
	mtk_xhci_mtk_log("mtk_xhci = 0x%x\n", (unsigned int)xhci);
	mtk_xhci = xhci;
}

void mtk_set_host_mode_in_host()
{
	mtk_idpin_cur_stat = IDPIN_IN_HOST;
}

void mtk_set_host_mode_out()
{
	mtk_idpin_cur_stat = IDPIN_OUT;
}


bool mtk_is_host_mode(void)
{
	return (mtk_idpin_cur_stat == IDPIN_IN_HOST) ? true : false;
}

void mtk_xhci_ck_timer_init(void)
{
	__u32 __iomem *addr;
	u32 temp = 0;
	int num_u3_port;

	num_u3_port = SSUSB_U3_PORT_NUM(readl((void __iomem *)SSUSB_IP_CAP));
	if (num_u3_port) {
		//set MAC reference clock speed
		addr = (__u32 __iomem *) (SSUSB_U3_MAC_BASE + U3_UX_EXIT_LFPS_TIMING_PAR);
		temp = readl(addr);
		temp &= ~(0xff << U3_RX_UX_EXIT_LFPS_REF_OFFSET);
		temp |= (U3_RX_UX_EXIT_LFPS_REF << U3_RX_UX_EXIT_LFPS_REF_OFFSET);
		writel(temp, addr);
		addr = (__u32 __iomem *) (SSUSB_U3_MAC_BASE + U3_REF_CK_PAR);
		temp = readl(addr);
		temp &= ~(0xff);
		temp |= U3_REF_CK_VAL;
		writel(temp, addr);

		//set SYS_CK
		addr = (__u32 __iomem *) (SSUSB_U3_SYS_BASE + U3_TIMING_PULSE_CTRL);
		temp = readl(addr);
		temp &= ~(0xff);
		temp |= MTK_CNT_1US_VALUE;
		writel(temp, addr);
	}

	addr = (__u32 __iomem *) (SSUSB_U2_SYS_BASE + USB20_TIMING_PARAMETER);
	temp &= ~(0xff);
	temp |= MTK_TIME_VALUE_1US;
	writel(temp, addr);

	if (num_u3_port) {
		//set LINK_PM_TIMER=3
		addr = (__u32 __iomem *) (SSUSB_U3_SYS_BASE + LINK_PM_TIMER);
		temp = readl(addr);
		temp &= ~(0xf);
		temp |= MTK_PM_LC_TIMEOUT_VALUE;
		writel(temp, addr);
	}
}

static void setLatchSel(void)
{
	__u32 __iomem *latch_sel_addr;
	u32 latch_sel_value;
	int num_u3_port;

	num_u3_port = SSUSB_U3_PORT_NUM(readl((void __iomem *)SSUSB_IP_CAP));
	if (num_u3_port <= 0)
		return;

	latch_sel_addr = (__u32 __iomem *) U3_PIPE_LATCH_SEL_ADD;
	latch_sel_value = ((U3_PIPE_LATCH_TX) << 2) | (U3_PIPE_LATCH_RX);
	writel(latch_sel_value, latch_sel_addr);
}

#ifndef CONFIG_USB_MTK_DUALMODE
static int mtk_xhci_phy_init(int argc, char **argv)
{
	/* initialize PHY related data structure */
	if (!u3phy_ops)
		u3phy_init();

	/* USB 2.0 slew rate calibration */
	if (u3phy_ops->u2_slew_rate_calibration)
		u3phy_ops->u2_slew_rate_calibration(u3phy);
	else
		printk(KERN_ERR "WARN: PHY doesn't implement u2 slew rate calibration function\n");

	/* phy initialization */
	if (u3phy_ops->init(u3phy) != PHY_TRUE)
		return RET_FAIL;

	printk(KERN_ERR "phy registers and operations initial done\n");
	return RET_SUCCESS;
}
#endif

void mtk_xhci_ip_init(void)
{
#ifdef CONFIG_MTK_LDVT
	mt_set_gpio_mode(121, 4);
#endif

	/* phy initialization is done by device, if target runs on dual mode */
#ifndef CONFIG_USB_MTK_DUALMODE
	mtk_xhci_phy_init(0, NULL);
#endif

	/* reset ip, power on host and power on/enable ports */
#if 0
#ifndef CONFIG_USB_MTK_DUALMODE
	enableAllClockPower(1);	/* host do reset ip */
#else
	enableAllClockPower(0);	/* device do reset ip */
#endif
#endif

	enableAllClockPower(0);	/* host do reset ip */

	setLatchSel();
	mtk_xhci_ck_timer_init();
	mtk_xhci_scheduler_init();
}

int mtk_xhci_get_port_num(void)
{
	return SSUSB_U3_PORT_NUM(readl((void __iomem *)SSUSB_IP_CAP))
	    + SSUSB_U2_PORT_NUM(readl((void __iomem *)SSUSB_IP_CAP));
}
