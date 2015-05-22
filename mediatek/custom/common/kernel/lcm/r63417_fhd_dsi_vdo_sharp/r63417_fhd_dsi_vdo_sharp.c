#ifndef BUILD_LK
#include <linux/string.h>
#endif
#include "lcm_drv.h"

#ifdef BUILD_LK
#include <platform/mt_gpio.h>
#include <platform/upmu_common.h>
#include <platform/ddp_dsi.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#include <mach/mt_pm_ldo.h>
#else
#include <mach/mt_gpio.h>
#include <cust_gpio_usage.h>
#include <mach/mt_pm_ldo.h>
#endif

// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  (1080)
#define FRAME_HEIGHT (1800)
#define REGFLAG_DELAY          	0XFE
#define REGFLAG_END_OF_TABLE   	0xFF   // END OF REGISTERS MARKER

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)    (lcm_util.set_reset_pin((v)))

#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))
#ifdef BUILD_LK
#define lcm_print(str, args...) printf(str, ##args);
#else
#define lcm_print(str, args...) pr_debug(str, ##args);
#endif


// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)	lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)	lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)	lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size)   lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)   


#define   LCM_DSI_CMD_MODE 0

struct LCM_setting_table {
	unsigned cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table sharp_video_mode[] = 
{
	{0x01,	1,	{0x0}},
	{REGFLAG_DELAY, 3, {}},

	{0xB0,	1,	{0x0}},
	{REGFLAG_DELAY, 10, {}},

	{0xB3,	6,	{0x14, 0x0, 0x0,
				0x22, 0x0, 0x0	}},
	{REGFLAG_DELAY, 5, {}},

	{0xD6, 	1,	{0x01}},
	{REGFLAG_DELAY, 5, {}},

	{0x3A,	1,	{0x07}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table sharp_command_mode[] = 
{
	{0x01,	1,	{0x0}},
	{REGFLAG_DELAY, 3, {}},

	{0xB0,	1,	{0x0}},
	{REGFLAG_DELAY, 10, {}},

	{0xB3,	6,	{0x04, 0x0, 0x0,
				0x22, 0x0, 0x0	}},
	{REGFLAG_DELAY, 5, {}},

	{0x3A, 	1,	{0x77}},
	{0x2A, 	4,	{0x00,0x00,0x04,0x37}},
	{0x2B, 	4,	{0x00,0x00,0x07,0x07}},
	{REGFLAG_DELAY, 5, {}},
	{0xD6, 	1,	{0x01}},
	{REGFLAG_DELAY, 5, {}},

	{0x35,	1,	{0x0}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table sharp_slp_out[] = {
	// Sleep Out
	{0x11, 1, {0x00}},
	{REGFLAG_DELAY, 120, {}},

	// Display ON
	{0x29, 1, {0x00}},
	{REGFLAG_DELAY, 20, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table sharp_slp_in[] = {
	// Display off sequence
	{0x28, 1, {0x00}},
	{REGFLAG_DELAY, 20, {}},

	// Sleep Mode On
	{0x10, 1, {0x00}},
	{REGFLAG_DELAY, 80, {}},

	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
	unsigned int i;

	for (i = 0; i < count; i++) {
		unsigned cmd;
		cmd = table[i].cmd;

		switch (cmd) {
		case REGFLAG_DELAY :
			if(table[i].count <= 10)
				UDELAY(table[i].count * 1000);
			else
				MDELAY(table[i].count);
			break;

		case REGFLAG_END_OF_TABLE :
			break;

		default:
			dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
		}
	}
}

static void init_lcm_registers(void)
{
#if LCM_DSI_CMD_MODE 
	push_table(sharp_command_mode, 
			sizeof(sharp_command_mode)/sizeof(struct LCM_setting_table), 1);
#else
	push_table(sharp_video_mode, 
			sizeof(sharp_video_mode)/sizeof(struct LCM_setting_table), 1);
#endif
//	ddp_dsi_set_hs_mode(1);
	push_table(sharp_slp_out, 
			sizeof(sharp_slp_out)/sizeof(struct LCM_setting_table), 1);
}

// ---------------------------------------------------------------------------
//  LCM Driver Implementations
// ---------------------------------------------------------------------------

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}

static void lcm_update(unsigned int x, unsigned int y,
                       unsigned int width, unsigned int height)
{
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + width - 1;
	unsigned int y1 = y0 + height - 1;

	unsigned char x0_MSB = ((x0>>8)&0xFF);
	unsigned char x0_LSB = (x0&0xFF);
	unsigned char x1_MSB = ((x1>>8)&0xFF);
	unsigned char x1_LSB = (x1&0xFF);
	unsigned char y0_MSB = ((y0>>8)&0xFF);
	unsigned char y0_LSB = (y0&0xFF);
	unsigned char y1_MSB = ((y1>>8)&0xFF);
	unsigned char y1_LSB = (y1&0xFF);

	unsigned int data_array[16];

	data_array[0]= 0x00053902;
	data_array[1]= (x1_MSB<<24)|(x0_LSB<<16)|(x0_MSB<<8)|0x2a;
	data_array[2]= (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);
	
	data_array[0]= 0x00053902;
	data_array[1]= (y1_MSB<<24)|(y0_LSB<<16)|(y0_MSB<<8)|0x2b;
	data_array[2]= (y1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

//	data_array[0]= 0x00290508; //HW bug, so need send one HS packet
//	dsi_set_cmdq(data_array, 1, 1);
	
	data_array[0]= 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);

}


static void lcm_get_params(LCM_PARAMS *params)
{
	memset(params, 0, sizeof(LCM_PARAMS));

	params->type   = LCM_TYPE_DSI;
	params->lcm_if = LCM_INTERFACE_DSI0;
	params->lcm_cmd_if = LCM_INTERFACE_DSI0;
	params->width  = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;
	params->physical_width = 66;
	params->physical_height = 110;
	// enable tearing-free
	params->dbi.te_mode = LCM_DBI_TE_MODE_VSYNC_ONLY;
	params->dbi.te_edge_polarity = LCM_POLARITY_RISING;
	// DSI
#if LCM_DSI_CMD_MODE 
	params->dsi.mode   = CMD_MODE;//BURST_VDO_MODE;
#else
	params->dsi.mode   = BURST_VDO_MODE;
#endif
	/* Command mode setting */
	//1 Three lane or Four lane
	params->dsi.LANE_NUM = LCM_FOUR_LANE;
	//The following defined the fomat for data coming from LCD engine.
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;
	// Highly depends on LCD driver capability.
	// Not support in MT6573
	params->dsi.packet_size = 256;

	// Video mode setting		
	params->dsi.intermediat_buffer_num = 0;//because DSI/DPI HW design change, this parameters should be 0 when video mode in MT658X; or memory leakage

	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;
	params->dsi.word_count = FRAME_WIDTH * 3;

	params->dsi.vertical_sync_active = 4;
	params->dsi.vertical_backporch = 8;
	params->dsi.vertical_frontporch	= 4;
	params->dsi.vertical_active_line = FRAME_HEIGHT; 

	params->dsi.horizontal_sync_active = 4;
	params->dsi.horizontal_backporch = 100;
	params->dsi.horizontal_frontporch = 42;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;

	// Bit rate calculation
	//1 Every lane speed
	params->dsi.pll_div1 = 0;// div1=0,1,2,3;div1_real=1,2,4,4 ----0: 546Mbps  1:273Mbps
	params->dsi.pll_div2 = 0;// div2=0,1,2,3;div1_real=1,2,4,4	
	params->dsi.fbk_div = 0x12;// fref=26MHz, fvco=fref*(fbk_div+1)*2/(div1_real*div2_real)	
	//or else set PLL_clk to trigger disp_drv calculate the above div automatically.
	//params->dsi.PLL_CLOCK = 450;
}

static void lcm_power_on(bool value)
{
	lcm_print(" %s %s\n", __func__, value ? "on":"false");
	if (value) {
		//reset --> L
		//lcm_reset(0);
		SET_RESET_PIN(0);
		//VDD18
#ifndef BUILD_LK
		hwPowerOn(MT6331_POWER_LDO_VMC,  VOL_1800, "LCD");
#else
		mt6331_upmu_set_rg_vmc_vosel(0);
		mt6331_upmu_set_rg_vmc_en(1);
#endif
		//hwPowerOn(MT6322_POWER_LDO_VMC, VOL_3000, "LCD1");
		MDELAY(5);
		//VSP5V
		mt_set_gpio_out(GPIO_LCD_BIAS_ENP_PIN, GPIO_OUT_ONE);
		//VSN5V
		mt_set_gpio_out(GPIO_LCD_BIAS_ENN_PIN, GPIO_OUT_ONE);
		MDELAY(5);
		//reset ---> H
		//lcm_reset(1);
		SET_RESET_PIN(1);
		MDELAY(10);
	} else {
		//VSN5V
		mt_set_gpio_out(GPIO_LCD_BIAS_ENN_PIN, GPIO_OUT_ZERO);
		//VSP5V
		mt_set_gpio_out(GPIO_LCD_BIAS_ENP_PIN, GPIO_OUT_ZERO);
		MDELAY(10);
		//reset ---> H
		//lcm_reset(0);
		SET_RESET_PIN(0);
#ifndef BUILD_LK
		hwPowerDown(MT6331_POWER_LDO_VMC,"LCD");
#else
		mt6331_upmu_set_rg_vmc_en(0);
#endif
	}
}
static void lcm_init(void)
{
	//pr_err("LCD RESET PIN MODE %d\n",mt_get_gpio_mode(GPIO112));
//	mt_set_gpio_mode(GPIO112, 0);
//	mt_set_gpio_dir(GPIO112, GPIO_DIR_OUT);
	lcm_power_on(true);	
	init_lcm_registers();
}

static void lcm_suspend(void)
{
	push_table(sharp_slp_in,
		sizeof(sharp_slp_in)/sizeof(struct LCM_setting_table), 1);
	lcm_power_on(false);
}

static void lcm_resume(void)
{
	lcm_power_on(true);	
	init_lcm_registers();
}

LCM_DRIVER r63417_fhd_dsi_vdo_sharp_lcm_drv = 
{
	.name			= "r63417_fhd_dsi_vdo_sharp",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
#if LCM_DSI_CMD_MODE 
	.update 		= lcm_update,
#endif
};
