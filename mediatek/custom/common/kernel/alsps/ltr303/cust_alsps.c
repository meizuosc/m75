#include <linux/types.h>
#include <cust_alsps.h>
#include <linux/platform_device.h>
#include <mach/mt_pm_ldo.h>

static struct alsps_hw cust_alsps_hw = {
    .i2c_num    = 2,
    .polling_mode =1,
    .power_id   = MT65XX_POWER_NONE,    /*LDO is not used*/
    .power_vol  = VOL_DEFAULT,          /*LDO is not used*/
    //.i2c_addr   = {0x20, 0x00, 0x00, 0x00},
    .als_level  = { 0,  1,  1,   5,  11,  11,  70, 700, 1400,  2100,  4200, 7000, 9800, 12600, 14000},
    .als_value  = {40, 40, 90,  90, 160, 160,  225,  320,  640,  1280,  1280,  2600,  2600, 2600,  10240, 10240},
};
struct alsps_hw *get_cust_alsps_hw(void) {
    return &cust_alsps_hw;
}
