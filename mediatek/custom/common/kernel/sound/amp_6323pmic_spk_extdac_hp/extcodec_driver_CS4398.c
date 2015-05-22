/*****************************************************************************
*                E X T E R N A L      R E F E R E N C E S
******************************************************************************
*/
#include <asm/uaccess.h>
#include <linux/xlog.h>
#include <linux/i2c.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>
//#include <platform/mt_reg_base.h>
#include <linux/delay.h>

#include "external_codec_driver.h"


#define MT6592_I2SOUT_GPIO
#define CS4398_GPIO_CONTROL
#define TPA6141_GPIO_CONTROL
#define NB3N501_GPIO_CONTROL

#define GPIO_BASE (0x10005000)

/*****************************************************************************
*                          DEBUG INFO
******************************************************************************
*/

static bool ecodec_log_on = true;

#define ECODEC_PRINTK(fmt, arg...) \
    do { \
        if (ecodec_log_on) xlog_printk(ANDROID_LOG_INFO,"ECODEC", "%s() "fmt"\n", __func__,##arg); \
    }while (0)

/*****************************************************************************
*               For I2C definition
******************************************************************************
*/

#define ECODEC_I2C_CHANNEL     (2)        //I2CL: SDA2,SCL2

// device address,  write: 10011000, read: 10011001
#define ECODEC_SLAVE_ADDR_WRITE 0x98
#define ECODEC_SLAVE_ADDR_READ  0x99

#define ECODEC_I2C_DEVNAME "CS4398"

//define registers

#define CS4398_DIF_MASK 0x70
#define CS4398_DIF_LJ 0x00
#define CS4398_DIF_I2S 0x10
#define CS4398_DIF_RJ_16 0x20
#define CS4398_DIF_RJ_24 0x30
#define CS4398_DIF_RJ_20 0x40
#define CS4398_DIF_RJ_18 0x50

#define CS4398_FUNCMODE_MASK 0x03

#define CS4398_MCLKDIV_MASK 0x18

#define CS4398_PDN_MASK 0x80

#define CS4398_CPEN_MASK 0x40

#define CS4398_MUTEAB_MASK 0x18
#define CS4398_PAMUTE_MASK 0x80

#define CS4398_VOLBEQA_MASK 0x80
#define CS4398_VOLUME_MASK 0xff

#define CS4398_CHIPID_MASK   0x70

// I2C variable
static struct i2c_client *new_client = NULL;

// new I2C register method
static const struct i2c_device_id ecodec_i2c_id[] = {{ECODEC_I2C_DEVNAME, 0}, {}};
static struct i2c_board_info __initdata  ecodec_dev = {I2C_BOARD_INFO(ECODEC_I2C_DEVNAME, (ECODEC_SLAVE_ADDR_WRITE >> 1))};

//function declration
static int ecodec_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int ecodec_i2c_remove(struct i2c_client *client);
//i2c driver
struct i2c_driver ecodec_i2c_driver =
{
    .probe = ecodec_i2c_probe,
    .remove = ecodec_i2c_remove,
    .driver = {
        .name = ECODEC_I2C_DEVNAME,
    },
    .id_table = ecodec_i2c_id,
};

// function implementation
//read one register
ssize_t static ecodec_read_byte(u8 addr, u8 *returnData)
{
    char        cmd_buf[1] = {0x00};
    char        readData = 0;
    int   ret = 0;

    if (!new_client)
    {
        ECODEC_PRINTK("ecodec_read_byte I2C client not initialized!!");
        return -1;
    }
    cmd_buf[0] = addr;
    ret = i2c_master_send(new_client, &cmd_buf[0], 1);
    if (ret < 0)
    {
        ECODEC_PRINTK("ecodec_read_byte read sends command error!!");
        return -1;
    }
    ret = i2c_master_recv(new_client, &readData, 1);
    if (ret < 0)
    {
        ECODEC_PRINTK("ecodec_read_byte reads recv data error!!");
        return -1;
    }
    *returnData = readData;
    ECODEC_PRINTK("addr 0x%x data 0x%x", addr, readData);
    return 0;
}

//read one register
static u8   I2CRead(u8 addr)
{
    u8 regvalue = 0;
    ecodec_read_byte(addr, &regvalue);
    return regvalue;
}

// write register
static ssize_t  I2CWrite(u8 addr, u8 writeData)
{
    char      write_data[2] = {0};
    int  ret = 0;

    if (!new_client)
    {
        ECODEC_PRINTK("I2C client not initialized!!");
        return -1;
    }
    write_data[0] = addr;         // ex. 0x01
    write_data[1] = writeData;
    ret = i2c_master_send(new_client, write_data, 2);
    if (ret < 0)
    {
        ECODEC_PRINTK("write sends command error!!");
        return -1;
    }
    ECODEC_PRINTK("addr 0x%x data 0x%x", addr, writeData);
    return 0;
}

static void  ecodec_set_hw_parameters(DIGITAL_INTERFACE_FORMAT dif)
{
    //MCLKDIV, SpeedMode, format
    volatile u8 reg;
    ECODEC_PRINTK("ecodec_set_hw_parameters (+) dif=%d", dif);

    reg = I2CRead(CS4398_MODE);
    reg &= ~CS4398_FUNCMODE_MASK;
    reg |= (0x00 & CS4398_FUNCMODE_MASK);
    I2CWrite(CS4398_MODE, reg); //functional mode (1:0), 00: Single-speed mode

    reg = I2CRead(CS4398_MISC1);
    reg &= ~CS4398_MCLKDIV_MASK;
    reg |= (0x00 & CS4398_MCLKDIV_MASK);
    I2CWrite(CS4398_MISC1, reg); //MCLKDIV2/MCLKDIV3 (4:3), 00: MCLKDIV2 disable, MCLKDIV3 disable

    reg = I2CRead(CS4398_MODE);
    reg &= ~CS4398_DIF_MASK;
    switch (dif)
    {
        case DIF_LEFT_JUSTIFIED:
            reg |= (CS4398_DIF_LJ & CS4398_DIF_MASK);
            I2CWrite(CS4398_MODE, reg);
            break;
        case DIF_I2S:
            reg |= (CS4398_DIF_I2S & CS4398_DIF_MASK);
            I2CWrite(CS4398_MODE, reg); //Digital interface format (6:4), 001: I2S mode(up to 24bit)
            break;
        case DIF_RIGHT_JUSTIFIED_16BIT:
            reg |= (CS4398_DIF_RJ_16 & CS4398_DIF_MASK);
            I2CWrite(CS4398_MODE, reg);
            break;
        case DIF_RIGHT_JUSTIFIED_24BIT:
            reg |= (CS4398_DIF_RJ_24 & CS4398_DIF_MASK);
            I2CWrite(CS4398_MODE, reg);
            break;
        case DIF_RIGHT_JUSTIFIED_20BIT:
            reg |= (CS4398_DIF_RJ_20 & CS4398_DIF_MASK);
            I2CWrite(CS4398_MODE, reg);
            break;
        case DIF_RIGHT_JUSTIFIED_18BIT:
            reg |= (CS4398_DIF_RJ_18 & CS4398_DIF_MASK);
            I2CWrite(CS4398_MODE, reg);
            break;
        default:
            ECODEC_PRINTK("digital interface format=%d error!", dif);
            break;
    }

    reg = I2CRead(CS4398_MUTE);
    reg &= ~CS4398_PAMUTE_MASK;
    reg |= (0x00 & CS4398_PAMUTE_MASK);
    I2CWrite(CS4398_MUTE, reg); //PAMUTE (7),  0: disable PCM AutoMute

#if 0
    reg = I2CRead(CS4398_MIXING);
    reg &= ~CS4398_VOLBEQA_MASK;
    reg |= (0x80 & CS4398_VOLBEQA_MASK);
    I2CWrite(CS4398_MIXING, reg); //VOLB=A (7),  1: set Vol B = A
#endif
    //ECODEC_PRINTK("ecodec_set_hw_parameters (-)");
}

static void  ecodec_set_control_port(u8 enable)
{
    //CPEN
    volatile u8 reg;
    ECODEC_PRINTK("ecodec_set_control_port (+), enable=%d ", enable);

    reg = I2CRead(CS4398_MISC1);
    reg &= ~CS4398_CPEN_MASK;

    if (enable == 1)
    {
        reg |= (0x40 & CS4398_CPEN_MASK);
        I2CWrite(CS4398_MISC1, reg); //CPEN (6), 1: Control port enable
    }
    else
    {
        reg |= (0x00 & CS4398_CPEN_MASK);
        I2CWrite(CS4398_MISC1, reg); //CPEN (6), 0: Control port disable
    }
    //ECODEC_PRINTK("ecodec_set_control_port(-)");
}

static void  ecodec_set_power(u8 enable)
{
    //PDN
    volatile u8 reg;
    ECODEC_PRINTK("ecodec_set_power(+) enable=%d ", enable);

    reg = I2CRead(CS4398_MISC1);

    reg &= ~CS4398_PDN_MASK;

    if (enable == 1)
    {
        reg |= (0x00 & CS4398_PDN_MASK);
        I2CWrite(CS4398_MISC1, reg); //PDN (7), 0: Power down disable
    }
    else
    {
        reg |= (0x80 & CS4398_PDN_MASK);
        I2CWrite(CS4398_MISC1, reg); //PDN (7), 1: Power down enable
    }
    //ECODEC_PRINTK("ecodec_set_power(-)");
}

static void  ecodec_mute(u8 enable)
{
    //MUTE
    volatile u8 reg;
    ECODEC_PRINTK("ecodec_mute CS4398_MUTE(+), enable=%d ", enable);

    reg = I2CRead(CS4398_MUTE);
    reg &= ~CS4398_MUTEAB_MASK;

    if (enable == 1)
    {
        reg |= (0x18 & CS4398_MUTEAB_MASK);
        I2CWrite(CS4398_MUTE, reg); //MUTE_A/MUTEB (4:3), 1: Mute
    }
    else
    {
        reg |= (0x00 & CS4398_MUTEAB_MASK);
        I2CWrite(CS4398_MUTE, reg); //MUTE_A/MUTEB (4:3), 0: Unmute
    }
    //ECODEC_PRINTK("ecodec_mute CS4398_MUTE(-)");
}

static void  ecodec_set_volume(u8 leftright, u8 gain)
{
    //Volume
    // 0: 0dB, 6: -3dB, ..., 255: -127.5dB
    ECODEC_PRINTK("ecodec_set_volume leftright=%d, gain=%d ", leftright, gain);

    if (leftright == 0) //left
    {
        I2CWrite(CS4398_VOLA, gain & CS4398_VOLUME_MASK);
    }
    else if (leftright == 1) //right
    {
        I2CWrite(CS4398_VOLB, gain & CS4398_VOLUME_MASK);
    }
}

void ecodec_dump_register(void)
{
    volatile u8 reg;

    ECODEC_PRINTK("ecodec_dump_register");

    reg = I2CRead(CS4398_MODE);
    ECODEC_PRINTK("CS4398_MODE = 0x%x ", reg);
    reg = I2CRead(CS4398_MIXING);
    ECODEC_PRINTK("CS4398_MIXING = 0x%x ", reg);
    reg = I2CRead(CS4398_MUTE);
    ECODEC_PRINTK("CS4398_MUTE = 0x%x ", reg);
    reg = I2CRead(CS4398_VOLA);
    ECODEC_PRINTK("CS4398_VOLA = 0x%x ", reg);
    reg = I2CRead(CS4398_VOLB);
    ECODEC_PRINTK("CS4398_VOLB = 0x%x ", reg);
    reg = I2CRead(CS4398_RAMP);
    ECODEC_PRINTK("CS4398_RAMP = 0x%x ", reg);
    reg = I2CRead(CS4398_MISC1);
    ECODEC_PRINTK("CS4398_MISC1 = 0x%x ", reg);
    reg = I2CRead(CS4398_MISC2);
    ECODEC_PRINTK("CS4398_MISC2 = 0x%x ", reg);
}

u8 ExtCodec_ReadReg(u8 addr)
{
    u8 val = 0;

    switch (addr)
    {
        case CS4398_MODE:
            val = I2CRead(CS4398_MODE);
            break;
        case CS4398_MIXING:
            val = I2CRead(CS4398_MIXING);
            break;
        case CS4398_MUTE:
            val = I2CRead(CS4398_MUTE);
            break;
        case CS4398_VOLA:
            val = I2CRead(CS4398_VOLA);
            break;
        case CS4398_VOLB:
            val = I2CRead(CS4398_VOLB);
            break;
        case CS4398_RAMP:
            val = I2CRead(CS4398_RAMP);
            break;
        case CS4398_MISC1:
            val = I2CRead(CS4398_MISC1);
            break;
        case CS4398_MISC2:
            val = I2CRead(CS4398_MISC2);
            break;
        default:
            ECODEC_PRINTK("ecodec_read_register wrong reg addr!!!");
            break;
    }
    return val;
}



static void ecodec_suspend(void)
{
    ECODEC_PRINTK("ecodec_suspend");
    //ecodec_resetRegister();
}

static void ecodec_resume(void)
{
    ECODEC_PRINTK("ecodec_resume");
}

static int ecodec_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    volatile u8 reg;
    new_client = client;
    //new_client->timing = 400;
    ECODEC_PRINTK("client timing=%dK ", new_client->timing);

    reg = I2CRead(CS4398_CHIPID) & CS4398_CHIPID_MASK ;
    if (reg == 0x70)
    {
        ECODEC_PRINTK("Find ChipID 0x%x  !!", reg);
    }
    else
    {
        ECODEC_PRINTK("Can not find ChipID, reg=0x%x  !!", reg);
    }
    //ecodec_resetRegister();
    return 0;
}

static int ecodec_i2c_remove(struct i2c_client *client)
{
    ECODEC_PRINTK("ecodec_i2c_remove");
    new_client = NULL;
    i2c_unregister_device(client);
    i2c_del_driver(&ecodec_i2c_driver);
    return 0;
}

static int ecodec_register(void)
{
    ECODEC_PRINTK("ecodec_register");
    i2c_register_board_info(ECODEC_I2C_CHANNEL, &ecodec_dev, 1);
    if (i2c_add_driver(&ecodec_i2c_driver))
    {
        ECODEC_PRINTK("fail to add device into i2c");
        return -1;
    }
    return 0;
}

#if 0
static ssize_t ecodec_resetRegister(void)
{
    // set registers to default value
    ECODEC_PRINTK("ecodec_resetRegister");
    I2CWrite(CS4398_MODE, 0x00);
    I2CWrite(CS4398_MIXING, 0x09);
    I2CWrite(CS4398_MUTE, 0xC0);
    I2CWrite(CS4398_VOLA, 0x18); //default -12dB
    I2CWrite(CS4398_VOLB, 0x18); //default -12dB
    I2CWrite(CS4398_RAMP, 0xb0);
    I2CWrite(CS4398_MISC1, 0x80);
    I2CWrite(CS4398_MISC2, 0x08);
    return 0;
}
#endif

/*****************************************************************************
*                  F U N C T I O N        D E F I N I T I O N
******************************************************************************
*/

void ExtCodec_Init()
{
    ECODEC_PRINTK("ExtCodec_Init ");
}

void ExtCodec_PowerOn(void)
{
    ecodec_set_control_port(1);
    ecodec_set_hw_parameters(DIF_I2S); //Need not power enable(PDN) when R/W register, onlt need turn on CPEN
    ecodec_set_volume(0, 14); //default -7dB
    ecodec_set_volume(1, 14);
    ecodec_set_power(1);
    ExtCodec_DumpReg();
}

void ExtCodec_PowerOff(void)
{
    ecodec_set_power(0);
}

bool ExtCodec_Register(void)
{
    ECODEC_PRINTK("ExtCodec_Register ");
    ecodec_register();
    return true;
}

void ExtCodec_Mute(void)
{
    ecodec_mute(1);
}

void ExtCodec_SetGain(u8 leftright, u8 gain)
{
    ecodec_set_volume(leftright, gain);
}

void ExtCodec_DumpReg(void)
{
    ecodec_dump_register();
}

int ExternalCodec(void)
{
    return 1;
}

void ExtCodecDevice_Suspend(void)
{
    ecodec_suspend();
}

void ExtCodecDevice_Resume(void)
{
    ecodec_resume();
}

/*
GPIO12: GPIO_AUD_EXTHP_EN_PIN
GPIO13: GPIO_AUD_EXTHP_GAIN_PIN
GPIO43: GPIO_AUD_EXTDAC_PWREN_PIN
GPIO92: GPIO_AUD_EXTDAC_RST_PIN
GPIO105: GPIO_I2S0_DAT_PIN
GPIO106: GPIO_I2S0_WS_PIN
GPIO107: GPIO_I2S0_CK_PIN
GPIO167: GPIO_AUD_EXTPLL_S0_PIN
GPIO168: GPIO_AUD_EXTPLL_S1_PIN
*/
void cust_extcodec_gpio_on()
{
    //external DAC, DAC_PWREN = 1, RST set to HIGH,
#ifdef CS4398_GPIO_CONTROL // 6592 GPIO setting
    ECODEC_PRINTK("Set GPIO for CS4398 ON ");
    mt_set_gpio_mode(GPIO_AUD_EXTDAC_RST_PIN, GPIO_MODE_00);
    mt_set_gpio_out(GPIO_AUD_EXTDAC_RST_PIN, GPIO_OUT_ZERO); // RST tied lo
#ifdef MT6592_I2SOUT_GPIO // 6592 GPIO setting for AFE I2S output 
    // Need to turn on I2S before CS4398 RST is released (by GPIO)
    ECODEC_PRINTK("6592 Set GPIO for AFE I2S output to external DAC ");
    mt_set_gpio_mode(GPIO_I2S0_DAT_PIN, GPIO_MODE_04);
    mt_set_gpio_mode(GPIO_I2S0_WS_PIN, GPIO_MODE_04);
    mt_set_gpio_mode(GPIO_I2S0_CK_PIN, GPIO_MODE_04);
#endif

    msleep(5); //avoid no sound when set I2S GPIO mode to 0 when turn off extDAC each time!!!

    mt_set_gpio_mode(GPIO_AUD_EXTDAC_PWREN_PIN, GPIO_MODE_00);
    mt_set_gpio_out(GPIO_AUD_EXTDAC_PWREN_PIN, GPIO_OUT_ONE);  // power on

    hwPowerOn(MT6323_POWER_LDO_VGP1, VOL_3300, "Audio");

    mt_set_gpio_mode(GPIO_AUD_EXTDAC_RST_PIN, GPIO_MODE_00);
    mt_set_gpio_out(GPIO_AUD_EXTDAC_RST_PIN, GPIO_OUT_ONE); // RST tied high
#endif
}

void cust_extcodec_gpio_off()
{
    ecodec_mute(1);

    //external DAC, RST set to LOW, DAC_PWREN = 0
#ifdef CS4398_GPIO_CONTROL // 6592 GPIO setting
    ECODEC_PRINTK("Set GPIO for CS4398 OFF ");
    mt_set_gpio_mode(GPIO_AUD_EXTDAC_PWREN_PIN, GPIO_MODE_00);
    mt_set_gpio_out(GPIO_AUD_EXTDAC_PWREN_PIN, GPIO_OUT_ZERO);   // power off
#endif

    mt_set_gpio_mode(GPIO_AUD_EXTDAC_RST_PIN, GPIO_MODE_00);
    mt_set_gpio_out(GPIO_AUD_EXTDAC_RST_PIN, GPIO_OUT_ZERO); // RST tied lo


    //ExtCodec_PowerOff();

//#ifdef MT6592_I2SOUT_GPIO // 6592 GPIO setting for AFE I2S output
#if 0
    ECODEC_PRINTK("6592 Set GPIO for AFE I2S output to mode0 ");
    mt_set_gpio_mode(GPIO_I2S0_DAT_PIN, GPIO_MODE_00);
    mt_set_gpio_mode(GPIO_I2S0_WS_PIN, GPIO_MODE_00);
    mt_set_gpio_mode(GPIO_I2S0_CK_PIN, GPIO_MODE_00);
#endif

    hwPowerDown(MT6323_POWER_LDO_VGP1, "Audio");
}

void cust_extHPAmp_gpio_on()
{
    //external HPAmp, gain set to 0dB, AMP set to enable
#ifdef TPA6141_GPIO_CONTROL // 6592 GPIO setting
    ECODEC_PRINTK("Set GPIO for TPA6141 ON  ");
    mt_set_gpio_mode(GPIO_AUD_EXTHP_GAIN_PIN, GPIO_MODE_00);
    mt_set_gpio_out(GPIO_AUD_EXTHP_GAIN_PIN, GPIO_OUT_ZERO); // fixed at 0dB
    mt_set_gpio_mode(GPIO_AUD_EXTHP_EN_PIN, GPIO_MODE_00);
    mt_set_gpio_out(GPIO_AUD_EXTHP_EN_PIN, GPIO_OUT_ONE); // enable HP amp
#endif
}

void cust_extHPAmp_gpio_off()
{
    //external HPAmp, AMP set to disable, gain set to 0dB
#ifdef TPA6141_GPIO_CONTROL // 6592 GPIO setting
    ECODEC_PRINTK("Set GPIO for TPA6141 OFF ");
    mt_set_gpio_mode(GPIO_AUD_EXTHP_EN_PIN, GPIO_MODE_00);
    mt_set_gpio_out(GPIO_AUD_EXTHP_EN_PIN, GPIO_OUT_ZERO); // disable HP amp
#endif
}

void cust_extPLL_gpio_config()
{
    //external PLL, set S0/S1
#ifdef NB3N501_GPIO_CONTROL // 6592 GPIO setting
    ECODEC_PRINTK("Set GPIO for NB3N501 ");
    mt_set_gpio_mode(GPIO_AUD_EXTPLL_S0_PIN, GPIO_MODE_00);
    mt_set_gpio_out(GPIO_AUD_EXTPLL_S0_PIN, GPIO_OUT_ZERO); // set low means S0 = 1
    mt_set_gpio_mode(GPIO_AUD_EXTPLL_S1_PIN, GPIO_MODE_00);
    mt_set_gpio_out(GPIO_AUD_EXTPLL_S1_PIN, GPIO_OUT_ZERO); // set low means S1 = 1
#endif

}
