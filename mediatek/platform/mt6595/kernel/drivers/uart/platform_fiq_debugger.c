#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <asm/thread_info.h>
#include <asm/fiq.h>
#include <asm/fiq_glue.h>
#include <asm/fiq_debugger.h>
#include <mach/irqs.h>
#include <mach/mt_reg_base.h>
#include <mach/mt_clkmgr.h>
#include <linux/uart/mtk_uart.h>
#include <linux/uart/mtk_uart_intf.h>
#include <cust_gpio_usage.h>
#include <mach/mt_gpio.h>

#define THREAD_INFO(sp) ((struct thread_info *) \
                ((unsigned long)(sp) & ~(THREAD_SIZE - 1)))
#define REG_UART_BASE   *((volatile unsigned int*)(console_base_addr + 0x00))
#define REG_UART_STATUS *((volatile unsigned int*)(console_base_addr + 0x14))
#define REG_UART_IIR 	*((volatile unsigned int*)(console_base_addr + 0x08))
#ifdef CONFIG_MEIZU_FIQ_DEBUGGER_CONSOLE
#define REG_UART_DLL 	*((volatile unsigned int*)(console_base_addr + 0x00))
#define REG_UART_IER 	*((volatile unsigned int*)(console_base_addr + 0x04))
#define REG_UART_DLH 	*((volatile unsigned int*)(console_base_addr + 0x04))
#define REG_UART_EFR 	*((volatile unsigned int*)(console_base_addr + 0x08))
#define REG_UART_FCR 	*((volatile unsigned int*)(console_base_addr + 0x08))
#define REG_UART_LCR 	*((volatile unsigned int*)(console_base_addr + 0x0c))
#define REG_UART_MCR 	*((volatile unsigned int*)(console_base_addr + 0x10))
#define REG_UART_HS 	*((volatile unsigned int*)(console_base_addr + 0x24))
#define REG_UART_SAMPLE_COUNT 	*((volatile unsigned int*)(console_base_addr + 0x28))
#define REG_UART_SAMPLE_POINT 	*((volatile unsigned int*)(console_base_addr + 0x2c))
#define REG_UART_GUARD 	*((volatile unsigned int*)(console_base_addr + 0x3c))
#define REG_UART_FDIV_L 	*((volatile unsigned int*)(console_base_addr + 0x54))
#define REG_UART_FDIV_M 	*((volatile unsigned int*)(console_base_addr + 0x58))
#define REG_UART_FCR_RD 	*((volatile unsigned int*)(console_base_addr + 0x5c))
#define REG_UART_ESCAPE_EN 	*((volatile unsigned int*)(console_base_addr + 0x44))
#define REG_UART_SEL 	*((volatile unsigned int*)(console_base_addr + 0x90))
#endif

#define FIQ_DEBUGGER_BREAK_CH 6 /* CTRL + F */
#define MAX_FIQ_DBG_EVENT 1024

#ifndef CONFIG_MEIZU_FIQ_DEBUGGER_CONSOLE
static struct fiq_dbg_event fiq_dbg_events[MAX_FIQ_DBG_EVENT];
static int fiq_dbg_event_rd, fiq_dbg_event_wr;
static unsigned int fiq_dbg_event_ov;
#endif
static int console_base_addr = AP_UART1_BASE;
static int ret_FIQ_DEBUGGER_BREAK;
static int uart_irq_number = -1;
#ifdef CONFIG_MEIZU_FIQ_DEBUGGER_CONSOLE
static int UART_PDN_ID = PDN_FOR_UART2;
static int uart_rx_gpio_pin = -1;
static int uart_tx_gpio_pin = -1;
static int uart_rx_gpio_mode = 0;
static int uart_tx_gpio_mode = 0;
static int uart_rx_func_mode = 0;
static int uart_tx_func_mode = 0;

#define UART_IER_HW_FIQINTS (UART_IER_ERBFI | UART_IER_ELSI)

struct fiq_uart_register {  
    unsigned int dll;
    unsigned int dlh;
    unsigned int ier;
    unsigned int lcr;
    unsigned int mcr;
    unsigned int fcr;
    unsigned int lsr;
    unsigned int efr;
    unsigned int highspeed;
    unsigned int sample_count;
    unsigned int sample_point;
    unsigned int fracdiv_l;
    unsigned int fracdiv_m;
    unsigned int escape_en;
    unsigned int guard;
    unsigned int rx_sel;
};
static struct fiq_uart_register fiq_uart_reg;
#endif

extern struct mtk_uart *mt_console_uart;
extern int is_fiq_debug_console_enable(void *argv);
extern bool debug_handle_uart_interrupt(void *state, int this_cpu, void *regs, void *svc_sp);
extern void mtk_uart_tx_handler(struct mtk_uart *uart);
extern void mtk_uart_get_modem_status(struct mtk_uart *uart);
extern void debug_handle_irq_context(void *arg);

#ifdef CONFIG_MEIZU_FIQ_DEBUGGER_CONSOLE
/*---------------------------------------------------------------------------*/
void fiq_uart_power_up(void)
{
	if (0 != enable_clock(UART_PDN_ID, "UART"))
            printk("power on fail!!\n");
}
void fiq_uart_power_down(void)
{
	if (0 != disable_clock(UART_PDN_ID, "UART"))
            printk("power off fail!!\n");
}
void fiq_uart_save(void)
{
#ifdef CONFIG_PM
    //DLL may be changed by console write. To avoid this, use spinlock
    fiq_uart_reg.lcr = REG_UART_LCR;

    REG_UART_LCR = 0xbf;
    fiq_uart_reg.efr = REG_UART_EFR;
    REG_UART_LCR = fiq_uart_reg.lcr;

    fiq_uart_reg.fcr = REG_UART_FCR_RD;

    //baudrate
    fiq_uart_reg.highspeed = REG_UART_HS;
    fiq_uart_reg.fracdiv_l = REG_UART_FDIV_L;
    fiq_uart_reg.fracdiv_m = REG_UART_FDIV_M;

    REG_UART_LCR = fiq_uart_reg.lcr | UART_LCR_DLAB;
    fiq_uart_reg.dll = REG_UART_DLL;
    fiq_uart_reg.dlh = REG_UART_DLH;
    REG_UART_LCR =fiq_uart_reg.lcr;

    fiq_uart_reg.sample_count = REG_UART_SAMPLE_COUNT;
    fiq_uart_reg.sample_point = REG_UART_SAMPLE_POINT;
    fiq_uart_reg.guard = REG_UART_GUARD;
	    
    //flow control
    fiq_uart_reg.escape_en = REG_UART_ESCAPE_EN;
    fiq_uart_reg.mcr = REG_UART_MCR;
    fiq_uart_reg.ier = REG_UART_IER; 
  
    fiq_uart_reg.rx_sel = REG_UART_SEL; 
#endif
}

void fiq_uart_restore(void)
{
#ifdef CONFIG_PM
    REG_UART_LCR = 0xbf;
    REG_UART_EFR = fiq_uart_reg.efr;
    REG_UART_LCR =  fiq_uart_reg.lcr;
    REG_UART_FCR = fiq_uart_reg.fcr;

    //baudrate
    REG_UART_HS = fiq_uart_reg.highspeed;
    REG_UART_FDIV_L = fiq_uart_reg.fracdiv_l;
    REG_UART_FDIV_M = fiq_uart_reg.fracdiv_m;

    REG_UART_LCR = fiq_uart_reg.lcr | UART_LCR_DLAB;
    REG_UART_DLL = fiq_uart_reg.dll;
    REG_UART_DLH = fiq_uart_reg.dlh;
    REG_UART_LCR = fiq_uart_reg.lcr;

    REG_UART_SAMPLE_COUNT = fiq_uart_reg.sample_count;
    REG_UART_SAMPLE_POINT = fiq_uart_reg.sample_point;
    REG_UART_GUARD = fiq_uart_reg.guard;
    
    //flow control
    REG_UART_ESCAPE_EN = fiq_uart_reg.escape_en;
    REG_UART_MCR = fiq_uart_reg.mcr;
    REG_UART_IER = fiq_uart_reg.ier; 

    REG_UART_SEL = fiq_uart_reg.rx_sel; 
#endif
}

int fiq_uart_init(struct platform_device *pdev)
{
    fiq_uart_power_up();
    /* enable rx/br interrupts */
    REG_UART_IER = UART_IER_HW_FIQINTS;
    return 0;
}

int fiq_uart_suspend(struct platform_device *pdev)
{
    fiq_uart_save();
    /* disable interrupts */
    REG_UART_IER = 0;
    mt_set_gpio_mode(uart_rx_gpio_pin, uart_rx_gpio_mode);
    //mt_set_gpio_out(uart_tx_gpio_pin, GPIO_OUT_ONE);
    //mt_set_gpio_mode(uart_tx_gpio_pin, uart_tx_gpio_mode);
    //fiq_uart_power_down();
	return 0;
}

int fiq_uart_resume(struct platform_device *pdev)
{
    //fiq_uart_power_up();
    fiq_uart_restore();
    mt_set_gpio_mode(uart_rx_gpio_pin, uart_rx_func_mode);
    //mt_set_gpio_mode(uart_tx_gpio_pin, uart_tx_func_mode);
	return 0;
}
#endif

int fiq_uart_getc(struct platform_device *pdev)
{
    int ch;

    if (ret_FIQ_DEBUGGER_BREAK) {
        ret_FIQ_DEBUGGER_BREAK = 0;
        return FIQ_DEBUGGER_BREAK;
    }

    if (!(REG_UART_STATUS & 0x01))
        return FIQ_DEBUGGER_NO_CHAR;

    ch = REG_UART_BASE & 0xFF;

    if (ch == FIQ_DEBUGGER_BREAK_CH)
        return FIQ_DEBUGGER_BREAK;

    return ch;
}

void fiq_uart_putc(struct platform_device *pdev, unsigned int c)
{
    while (! (REG_UART_STATUS & 0x20));

    REG_UART_BASE = c & 0xFF;
}

void fiq_uart_fixup(int uart_port)
{
    switch (uart_port) {
    case 0:
        console_base_addr = AP_UART0_BASE;
        fiq_resource[1].start = UART0_IRQ_BIT_ID;
        fiq_resource[1].end = UART0_IRQ_BIT_ID;
        uart_irq_number = UART0_IRQ_BIT_ID;
#ifdef CONFIG_MEIZU_FIQ_DEBUGGER_CONSOLE
        mt_fiq_debugger.id = 0;
        UART_PDN_ID = PDN_FOR_UART0;
        uart_rx_gpio_pin = GPIO_UART_URXD0_PIN;
        uart_tx_gpio_pin = GPIO_UART_UTXD0_PIN;
        uart_rx_gpio_mode = GPIO_UART_URXD0_PIN_M_GPIO;
        uart_tx_gpio_mode = GPIO_UART_UTXD0_PIN_M_GPIO;
        uart_rx_func_mode = GPIO_UART_URXD0_PIN_M_URXD;
        uart_tx_func_mode = GPIO_UART_UTXD0_PIN_M_UTXD;
#endif
        break;
    case 1:
        console_base_addr = AP_UART1_BASE;
        fiq_resource[1].start = UART1_IRQ_BIT_ID;
        fiq_resource[1].end = UART1_IRQ_BIT_ID;
        uart_irq_number = UART1_IRQ_BIT_ID;
#ifdef CONFIG_MEIZU_FIQ_DEBUGGER_CONSOLE
        mt_fiq_debugger.id = 1;
        UART_PDN_ID = PDN_FOR_UART1;
//        uart_rx_gpio_pin = GPIO_UART_URXD1_PIN;
//        uart_tx_gpio_pin = GPIO_UART_UTXD1_PIN;
//        uart_rx_gpio_mode = GPIO_UART_URXD1_PIN_M_GPIO;
//        uart_tx_gpio_mode = GPIO_UART_UTXD1_PIN_M_GPIO;
//        uart_rx_func_mode = GPIO_UART_URXD1_PIN_M_URXD;
//        uart_tx_func_mode = GPIO_UART_UTXD1_PIN_M_UTXD;
#endif
        break;
    case 2:
        console_base_addr = AP_UART2_BASE;
        fiq_resource[1].start = UART2_IRQ_BIT_ID;
        fiq_resource[1].end = UART2_IRQ_BIT_ID;
        uart_irq_number = UART2_IRQ_BIT_ID;
#ifdef CONFIG_MEIZU_FIQ_DEBUGGER_CONSOLE
        mt_fiq_debugger.id = 2;
        UART_PDN_ID = PDN_FOR_UART2;
        uart_rx_gpio_pin = GPIO_UART_URXD2_PIN;
        uart_tx_gpio_pin = GPIO_UART_UTXD2_PIN;
        uart_rx_gpio_mode = GPIO_UART_URXD2_PIN_M_GPIO;
        uart_tx_gpio_mode = GPIO_UART_UTXD2_PIN_M_GPIO;
        uart_rx_func_mode = GPIO_UART_URXD2_PIN_M_URXD;
        uart_tx_func_mode = GPIO_UART_UTXD2_PIN_M_UTXD;
#endif
        break;
    case 3:
        console_base_addr = AP_UART3_BASE;
        fiq_resource[1].start = UART3_IRQ_BIT_ID;
        fiq_resource[1].end = UART3_IRQ_BIT_ID;
        uart_irq_number = UART3_IRQ_BIT_ID;
#ifdef CONFIG_MEIZU_FIQ_DEBUGGER_CONSOLE
        mt_fiq_debugger.id = 3;
        UART_PDN_ID = PDN_FOR_UART3;
//        uart_rx_gpio_pin = GPIO_UART_URXD3_PIN;
//        uart_tx_gpio_pin = GPIO_UART_UTXD3_PIN;
//        uart_rx_gpio_mode = GPIO_UART_URXD3_PIN_M_GPIO;
//        uart_tx_gpio_mode = GPIO_UART_UTXD3_PIN_M_GPIO;
//        uart_rx_func_mode = GPIO_UART_URXD3_PIN_M_URXD;
//        uart_tx_func_mode = GPIO_UART_UTXD3_PIN_M_UTXD;
#endif
        break;
    default:
        break;
    }
}

#ifndef CONFIG_MEIZU_FIQ_DEBUGGER_CONSOLE
static void __push_event(u32 iir, int data)
{
    if (((fiq_dbg_event_wr + 1) % MAX_FIQ_DBG_EVENT) == fiq_dbg_event_rd) {
        /* full */
        fiq_dbg_event_ov++;
    } else {
        fiq_dbg_events[fiq_dbg_event_wr].iir = iir;
        fiq_dbg_events[fiq_dbg_event_wr].data = data;
        fiq_dbg_event_wr++;
        fiq_dbg_event_wr %= MAX_FIQ_DBG_EVENT;
    }
}

static int __pop_event(u32 *iir, int *data)
{
    if (fiq_dbg_event_rd == fiq_dbg_event_wr) {
        /* empty */
        return -1;
    } else {
        *iir = fiq_dbg_events[fiq_dbg_event_rd].iir;
        *data = fiq_dbg_events[fiq_dbg_event_rd].data;
        fiq_dbg_event_rd++;
        fiq_dbg_event_rd %= MAX_FIQ_DBG_EVENT;
        return 0;
    }
}
#endif

static void mt_debug_fiq(void *arg, void *regs, void *svc_sp)
{
    u32 iir;
    int data = -1;
    int max_count = UART_FIFO_SIZE;
    unsigned int this_cpu;
    int need_irq = 1;

    iir = REG_UART_IIR;
    iir &= UART_IIR_INT_MASK;
    if (iir == UART_IIR_NO_INT_PENDING)
        return ;
    if (iir == UART_IIR_THRE) {
    }
#ifndef CONFIG_MEIZU_FIQ_DEBUGGER_CONSOLE
    __push_event(iir, data);
#endif
    while (max_count-- > 0) {
        if (!(REG_UART_STATUS & 0x01)) {
            break;
        }
#ifdef CONFIG_MEIZU_FIQ_DEBUGGER_CONSOLE
        this_cpu = THREAD_INFO(svc_sp)->cpu;
        need_irq = debug_handle_uart_interrupt(arg, this_cpu, regs, svc_sp);
#else
        if (is_fiq_debug_console_enable(arg)) {
            data = mt_console_uart->read_byte(mt_console_uart);
            if (data == FIQ_DEBUGGER_BREAK_CH) {
                /* enter FIQ debugger mode */
                ret_FIQ_DEBUGGER_BREAK = 1;
                this_cpu = THREAD_INFO(svc_sp)->cpu;
                debug_handle_uart_interrupt(arg, this_cpu, regs, svc_sp);
                return ;
            }
            __push_event(UART_IIR_NO_INT_PENDING, data);
            /*why need_irq?*/
            need_irq = 1;
        } else {
            this_cpu = THREAD_INFO(svc_sp)->cpu;
            need_irq = debug_handle_uart_interrupt(arg, this_cpu, regs, svc_sp);
        }
#endif
    }

    if (need_irq) {
        mt_disable_fiq(uart_irq_number);
        trigger_sw_irq(FIQ_DBG_SGI);
    }
}

irqreturn_t mt_debug_signal_irq(int irq, void *dev_id)
{
#ifndef CONFIG_MEIZU_FIQ_DEBUGGER_CONSOLE
   struct tty_struct *tty = mt_console_uart->port.state->port.tty;
    u32 iir;
    int data;

    while (__pop_event(&iir, &data) >= 0) {
        if (iir == UART_IIR_MS) {
            mtk_uart_get_modem_status(mt_console_uart);
        } else if (iir == UART_IIR_THRE) {
            mtk_uart_tx_handler(mt_console_uart);
        }
        if (data != -1) {
            if (!tty_insert_flip_char(tty->port, data, TTY_NORMAL)) {
            }
        }
    }
    tty_flip_buffer_push(tty->port);
#endif
    /* handle commands which can only be handled in the IRQ context */
    debug_handle_irq_context(dev_id);

    mt_enable_fiq(uart_irq_number);

    return IRQ_HANDLED;
}

int mt_fiq_init(void *arg)
{
    return request_fiq(uart_irq_number, (fiq_isr_handler)mt_debug_fiq, IRQF_TRIGGER_LOW, arg);
}
