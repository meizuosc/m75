#include <linux/kernel.h>
#include <asm/atomic.h>
#include <cust_leds.h>
#include <cust_leds_def.h>
#include <mach/mt_reg_base.h>
#include <mach/mt_clkmgr.h>
#include <mach/mt_gpio.h>
#include <cust_gpio_usage.h>
#include "ddp_reg.h"
#include "ddp_pwm.h"
#include "ddp_path.h"


#define PWM_DEFAULT_DIV_VALUE 0x0

#define PWM_ERR(fmt, arg...) printk(KERN_ERR "[PWM] " fmt "\n", ##arg)
#define PWM_MSG(fmt, arg...) printk(KERN_DEBUG "[PWM] " fmt "\n", ##arg)

#define pwm_get_reg_base(id) ((id) == DISP_PWM0 ? DISPSYS_PWM0_BASE : DISPSYS_PWM1_BASE)
    
#define index_of_pwm(id) ((id == DISP_PWM0) ? 0 : 1)


static disp_pwm_id_t g_pwm_main_id = DISP_PWM0;
static atomic_t g_pwm_backlight[2] = { ATOMIC_INIT(-1), ATOMIC_INIT(-1) };
static volatile int g_pwm_max_backlight[2] = { 1023, 1023 };
static ddp_module_notify g_ddp_notify = NULL;

static int disp_pwm_config_init(DISP_MODULE_ENUM module, disp_ddp_path_config* pConfig, void* cmdq)
{
    struct cust_mt65xx_led *cust_led_list;
    struct cust_mt65xx_led *cust;
    struct PWM_config *config_data;
    unsigned int pwm_div;
    disp_pwm_id_t id = (module == DISP_MODULE_PWM0 ? DISP_PWM0 : DISP_PWM1);
    unsigned int reg_base = pwm_get_reg_base(id);
    int index = index_of_pwm(id);

    pwm_div = PWM_DEFAULT_DIV_VALUE;
    cust_led_list = get_cust_led_list();
    if (cust_led_list)
    {
        /* WARNING: may overflow if MT65XX_LED_TYPE_LCD not configured properly */
        cust = &cust_led_list[MT65XX_LED_TYPE_LCD];
        if ((strcmp(cust->name,"lcd-backlight") == 0) && (cust->mode == MT65XX_LED_MODE_CUST_BLS_PWM))
        {
            config_data = &cust->config_data;
            if (config_data->clock_source >= 0 && config_data->clock_source <= 3)
            {
                unsigned int regVal = DISP_REG_GET(CLK_CFG_1);
                clkmux_sel(MT_MUX_PWM, config_data->clock_source, "DISP_PWM");
                PWM_MSG("disp_pwm_init : CLK_CFG_1 0x%x => 0x%x", regVal, DISP_REG_GET(CLK_CFG_1));
            }
            /* Some backlight chip/PMIC(e.g. MT6332) only accept slower clock */
            pwm_div = (config_data->div == 0) ? PWM_DEFAULT_DIV_VALUE : config_data->div;
            pwm_div &= 0x3FF;
            PWM_MSG("disp_pwm_init : PWM config data (%d,%d)", config_data->clock_source, config_data->div);
        }
    }

    atomic_set(&g_pwm_backlight[index], -1);

    /* We don't enable PWM until we really need */
    DISP_REG_MASK(cmdq, reg_base + DISP_PWM_CON_0_OFF, pwm_div << 16, (0x3ff << 16));

    DISP_REG_MASK(cmdq, reg_base + DISP_PWM_CON_1_OFF, 1023, 0x3ff); /* 1024 levels */
    /* We don't init the backlight here until AAL/Android give */

    return 0;
}


static int disp_pwm_config(DISP_MODULE_ENUM module, disp_ddp_path_config* pConfig, void* cmdq)
{
    int ret = 0;

    if (pConfig->dst_dirty)
        ret |= disp_pwm_config_init(module, pConfig, cmdq);

    return ret;
}


static void disp_pwm_trigger_refresh(disp_pwm_id_t id)
{
    DISP_MODULE_ENUM mod = DISP_MODULE_PWM0;

    if (id == DISP_PWM1)
        mod = DISP_MODULE_PWM1;
    
    g_ddp_notify(mod, DISP_PATH_EVENT_TRIGGER);
}


/* Set the PWM which acts by default (e.g. ddp_bls_set_backlight) */
void disp_pwm_set_main(disp_pwm_id_t main)
{
    g_pwm_main_id = main;
}


disp_pwm_id_t disp_pwm_get_main(void)
{
    return g_pwm_main_id;
}


int disp_pwm_is_enabled(disp_pwm_id_t id)
{
    unsigned int reg_base = pwm_get_reg_base(id);
    return (DISP_REG_GET(reg_base + DISP_PWM_EN_OFF) & 0x1);
}


static void disp_pwm_set_drverIC_en(disp_pwm_id_t id, int enabled)
{
#ifdef GPIO_LCM_LED_EN
    if (id == DISP_PWM0) {
        mt_set_gpio_mode(GPIO_LCM_LED_EN, GPIO_MODE_00);
        mt_set_gpio_dir(GPIO_LCM_LED_EN, GPIO_DIR_OUT);
        
        if (enabled)
            mt_set_gpio_out(GPIO_LCM_LED_EN, GPIO_OUT_ONE);
        else
            mt_set_gpio_out(GPIO_LCM_LED_EN, GPIO_OUT_ZERO);
    }
#endif
}


static void disp_pwm_set_enabled(cmdqRecHandle cmdq, disp_pwm_id_t id, int enabled)
{
    unsigned int reg_base = pwm_get_reg_base(id);
    if (enabled) {
        if (!disp_pwm_is_enabled(id)) {
            DISP_REG_MASK(cmdq, reg_base + DISP_PWM_EN_OFF, 0x1, 0x1);
            PWM_MSG("disp_pwm_set_enabled: PWN_EN = 0x1");

            disp_pwm_set_drverIC_en(id, enabled);
        }
    } else {
        DISP_REG_MASK(cmdq, reg_base + DISP_PWM_EN_OFF, 0x0, 0x1);
        disp_pwm_set_drverIC_en(id, enabled);
    }
}


int disp_bls_set_max_backlight(unsigned int level_1024)
{
    return disp_pwm_set_max_backlight(disp_pwm_get_main(), level_1024);
}


int disp_pwm_set_max_backlight(disp_pwm_id_t id, unsigned int level_1024)
{
    int index;

    if ((DISP_PWM_ALL & id) == 0) {
        PWM_ERR("[ERROR] disp_pwm_set_backlight: invalid PWM ID = 0x%x", id);
        return -EFAULT;
    }
    
    index = index_of_pwm(id);
    g_pwm_max_backlight[index] = level_1024;

    PWM_MSG("disp_pwm_set_max_backlight(id = 0x%x, level = %u)", id, level_1024);

    if (level_1024 < atomic_read(&g_pwm_backlight[index]))
        disp_pwm_set_backlight(id, level_1024);

    return 0;
}


/* For backward compatible */
int disp_bls_set_backlight(int level_1024)
{
    return disp_pwm_set_backlight(disp_pwm_get_main(), level_1024);
}


/*
 * If you want to re-map the backlight level from user space to
 * the real level of hardware output, please modify here.
 *
 * Inputs:
 *  id          - DISP_PWM0 / DISP_PWM1
 *  level_1024  - Backlight value in [0, 1023]
 * Returns:
 *  PWM duty in [0, 1023]
 */
static int disp_pwm_level_remap(disp_pwm_id_t id, int level_1024)
{
    return level_1024;
}


int disp_pwm_set_backlight(disp_pwm_id_t id, int level_1024)
{
    int ret;

    /* Always write registers by CPU */
    ret = disp_pwm_set_backlight_cmdq(id, level_1024, NULL);

    if (ret >= 0)
        disp_pwm_trigger_refresh(id);

    return 0;
}


static volatile int g_pwm_duplicate_count = 0;

int disp_pwm_set_backlight_cmdq(disp_pwm_id_t id, int level_1024, void *cmdq)
{
    unsigned int reg_base;
    int old_pwm;
    int index;

    if ((DISP_PWM_ALL & id) == 0) {
        PWM_ERR("[ERROR] disp_pwm_set_backlight_cmdq: invalid PWM ID = 0x%x", id);
        return -EFAULT;
    }

    index = index_of_pwm(id);

    old_pwm = atomic_xchg(&g_pwm_backlight[index], level_1024);
    if (old_pwm != level_1024) {
        PWM_MSG("disp_pwm_set_backlight_cmdq(id = 0x%x, level_1024 = %d), old = %d", id, level_1024, old_pwm);

        if (level_1024 > g_pwm_max_backlight[index]) {
            level_1024 = g_pwm_max_backlight[index];
        } else if (level_1024 < 0) {
            level_1024 = 0;
        }

        level_1024 = disp_pwm_level_remap(id, level_1024);

        reg_base = pwm_get_reg_base(id);
        DISP_REG_MASK(cmdq, reg_base + DISP_PWM_CON_1_OFF, level_1024 << 16, 0x1fff << 16);

        if (level_1024 > 0) {
            disp_pwm_set_enabled(cmdq, id, 1);
        } else {
            disp_pwm_set_enabled(cmdq, id, 0); /* To save power */
        }

        DISP_REG_MASK(cmdq, reg_base + DISP_PWM_COMMIT_OFF, 1, ~0);
        DISP_REG_MASK(cmdq, reg_base + DISP_PWM_COMMIT_OFF, 0, ~0);

        g_pwm_duplicate_count = 0;
    } else {
        g_pwm_duplicate_count = (g_pwm_duplicate_count + 1) & 63;
        if (g_pwm_duplicate_count == 2) {
            PWM_MSG("disp_pwm_set_backlight_cmdq(id = 0x%x, level_1024 = %d), old = %d (dup)",
                id, level_1024, old_pwm);
        }
    }

    return 0;
}


int ddp_pwm_power_on(DISP_MODULE_ENUM module, void *handle)
{
    PWM_MSG("ddp_pwm_power_on: %d\n", module);
    
    if (module == DISP_MODULE_PWM0) {
        enable_clock(MT_CG_DISP1_DISP_PWM0_26M, "PWM");
        enable_clock(MT_CG_DISP1_DISP_PWM0_MM, "PWM");
    } else if(module == DISP_MODULE_PWM1) {
        enable_clock(MT_CG_DISP1_DISP_PWM1_26M , "PWM");
        enable_clock(MT_CG_DISP1_DISP_PWM1_MM , "PWM");
    }
    
    return 0;
}

int ddp_pwm_power_off(DISP_MODULE_ENUM module, void *handle)
{
    PWM_MSG("ddp_pwm_power_off: %d\n", module);
    
    if (module == DISP_MODULE_PWM0) {
        atomic_set(&g_pwm_backlight[0], 0);
        disable_clock(MT_CG_DISP1_DISP_PWM0_26M , "PWM");
        disable_clock(MT_CG_DISP1_DISP_PWM0_MM , "PWM");
    } else if(module == DISP_MODULE_PWM1) {
        atomic_set(&g_pwm_backlight[1], 0);
        disable_clock(MT_CG_DISP1_DISP_PWM1_26M , "PWM");
        disable_clock(MT_CG_DISP1_DISP_PWM1_MM , "PWM");
    }

    return 0;
}


static int ddp_pwm_init(DISP_MODULE_ENUM module, void *cmq_handle)
{
    ddp_pwm_power_on(module, cmq_handle);
    return 0; 
}

static int ddp_pwm_set_listener(DISP_MODULE_ENUM module, ddp_module_notify notify)
{
    g_ddp_notify = notify;
    return 0;
}



DDP_MODULE_DRIVER ddp_driver_pwm =
{
    .init           = ddp_pwm_init,
    .config         = disp_pwm_config,
    .power_on       = ddp_pwm_power_on,
    .power_off      = ddp_pwm_power_off,
    .set_listener   = ddp_pwm_set_listener,
}; 


