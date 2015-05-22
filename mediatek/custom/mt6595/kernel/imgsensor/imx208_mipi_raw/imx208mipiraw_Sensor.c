/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 IMX208mipi_Sensor.c
 *
 * Project:
 * --------
 *	 ALPS
 *
 * Description:
 * ------------
 *	 Source code of Sensor driver
 *
 *
 *---------------------------------------------------------------------------
---
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include <asm/system.h>
#include <linux/xlog.h>

#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h" 
#include "kd_imgsensor_errcode.h"

#include "imx208mipiraw_Sensor.h"

#define PFX "IMX208_qtech_camera_sensor"
//#define LOG_WRN(format, args...) xlog_printk(ANDROID_LOG_WARN ,PFX, "[%S] " format, __FUNCTION__, ##args)
//#defineLOG_INF(format, args...) xlog_printk(ANDROID_LOG_INFO ,PFX, "[%s] " format, __FUNCTION__, ##args)
//#define LOG_DBG(format, args...) xlog_printk(ANDROID_LOG_DEBUG ,PFX, "[%S] " format, __FUNCTION__, ##args)
#define LOG_INF(format, args...)	xlog_printk(ANDROID_LOG_INFO   , PFX, "[%s] " format, __FUNCTION__, ##args)

#define QT_E2PROM_WRITE_ID	0x6E

int qtech_status = 0; 
extern int sunny_status; 

static DEFINE_SPINLOCK(imgsensor_drv_lock);


static imgsensor_info_struct imgsensor_info = { 
	.sensor_id = IMX208_SENSOR_ID & 0x0FFF,
	
	.checksum_value = 0xaafdcfaf,
	
	.pre = {
		.pclk = 81000000,									//record different mode's pclk
		.linelength = 1124,  /*[actually 1124*2]	*/			//record different mode's linelength
		.framelength = 2400,								//record different mode's framelength
		.startx = 0,										//record different mode's startx of grabwindow
		.starty = 0,										//record different mode's starty of grabwindow
		.grabwindow_width = 1920,							//record different mode's width of grabwindow
		.grabwindow_height = 1080,							//record different mode's height of grabwindow
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 85,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,	
	},
	.cap = {
		.pclk = 81000000,
		.linelength = 1124,
		.framelength = 2400,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1920,
		.grabwindow_height = 1080,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
	},
	.cap1 = {
		.pclk = 81000000,
		.linelength = 1124,
		.framelength = 3000,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1920,
		.grabwindow_height = 1080,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 240,	
	},
	.normal_video = {
		.pclk = 81000000,
		.linelength = 1124,
		.framelength = 2400,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1920,
		.grabwindow_height = 1080,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
	},
	.hs_video = {
		.pclk = 81000000,
		.linelength = 1124,
		.framelength = 1200,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1920,
		.grabwindow_height = 1080,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 600,
	},
	.slim_video = {
		.pclk = 81000000,
		.linelength = 1124,
		.framelength = 2400,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1920,
		.grabwindow_height = 1080,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
	},
	.margin = 6,
	.min_shutter = 1,
	.max_frame_length = 0xfffe,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,	  //1, support; 0,not support
	.ihdr_le_firstline = 0,  //1,le first ; 0, se first
	.sensor_mode_num = 5,	  //support sensor mode num
	
	.cap_delay_frame = 2, 
	.pre_delay_frame = 2, 
	.video_delay_frame = 2,
	.hs_video_delay_frame = 2,
	.slim_video_delay_frame = 2,
	
	.isp_driving_current = ISP_DRIVING_6MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_2_LANE,
};


static imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,				//mirrorflip information
	.sensor_mode = IMGSENSOR_MODE_INIT, //IMGSENSOR_MODE enum value,record current sensor mode,such as: INIT, Preview, Capture, Video,High Speed Video, Slim Video
	.shutter = 0x3D0,					//current shutter
	.gain = 0x100,						//current gain
	.dummy_pixel = 0,					//current dummypixel
	.dummy_line = 0,					//current dummyline
	.current_fps = 0,  //full size current fps : 24fps for PIP, 30fps for Normal or ZSD
	.autoflicker_en = KAL_FALSE,  //auto flicker enable: KAL_FALSE for disable auto flicker, KAL_TRUE for enable auto flicker
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,//current scenario id
	.ihdr_en = 0, //sensor need support LE, SE with HDR feature
	.i2c_write_id = 0x6E,
};


/* Sensor output window information */
static SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] =	 
{{ 1920, 1080, 0, 0, 1920, 1080,    1920,1080,0,0,1920,1080,	  0,0,1920,1080}, // Preview 
 { 1920, 1080, 0, 0, 1920, 1080, 	1920,1080,0,0,1920,1080,	  0,0,1920,1080}, // capture
 { 1920, 1080, 0, 0, 1920, 1080, 	1920,1080,0,0,1920,1080,	  0,0,1920,1080},// video 
 { 1920, 1080, 0, 0, 1920, 1080, 	1920,1080,0,0,1920,1080,	  0,0,1920,1080},//hight speed video 
 { 1920, 1080, 0, 0, 1920, 1080, 	1920,1080,0,0,1920,1080,	  0,0,1920,1080}};// slim video 


#define MaxGainIndex (97)
static kal_uint16 SensorGainMapping[MaxGainIndex][2] ={
{ 64 ,0  },   
{ 68 ,12 },   
{ 71 ,23 },   
{ 74 ,33 },   
{ 77 ,42 },   
{ 81 ,52 },   
{ 84 ,59 },   
{ 87 ,66 },   
{ 90 ,73 },   
{ 93 ,79 },   
{ 96 ,85 },   
{ 100,91 },   
{ 103,96 },   
{ 106,101},   
{ 109,105},   
{ 113,110},   
{ 116,114},   
{ 120,118},   
{ 122,121},   
{ 125,125},   
{ 128,128},   
{ 132,131},   
{ 135,134},   
{ 138,137},
{ 141,139},
{ 144,142},   
{ 148,145},   
{ 151,147},   
{ 153,149}, 
{ 157,151},
{ 160,153},      
{ 164,156},   
{ 168,158},   
{ 169,159},   
{ 173,161},   
{ 176,163},   
{ 180,165}, 
{ 182,166},   
{ 187,168},
{ 189,169},
{ 193,171},
{ 196,172},
{ 200,174},
{ 203,175}, 
{ 205,176},
{ 208,177}, 
{ 213,179}, 
{ 216,180},  
{ 219,181},   
{ 222,182},
{ 225,183},  
{ 228,184},   
{ 232,185},
{ 235,186},
{ 238,187},
{ 241,188},
{ 245,189},
{ 249,190},
{ 253,191},
{ 256,192}, 
{ 260,193},
{ 265,194},
{ 269,195},
{ 274,196},   
{ 278,197},
{ 283,198},
{ 288,199},
{ 293,200},
{ 298,201},   
{ 304,202},   
{ 310,203},
{ 315,204},
{ 322,205},   
{ 328,206},   
{ 335,207},   
{ 342,208},   
{ 349,209},   
{ 357,210},   
{ 365,211},   
{ 373,212}, 
{ 381,213},
{ 400,215},      
{ 420,217},   
{ 432,218},   
{ 443,219},      
{ 468,221},   
{ 482,222},   
{ 497,223},   
{ 512,224},
{ 529,225}, 	 
{ 546,226},   
{ 566,227},   
{ 585,228}, 	 
{ 607,229},   
{ 631,230},   
{ 656,231},   
{ 683,232}
};

static kal_bool imx208_During_testpattern = KAL_FALSE;


static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte=0;

	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };
	iReadRegI2C(pu_send_cmd, 2, (u8*)&get_byte, 1, imgsensor.i2c_write_id);

	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[3] = {(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF)};
	iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}

static int imx208_qt_read_otp(u16 addr, u8 *buf)
{
	int ret = 0;
	u8 pu_send_cmd[2] = {(u8)(addr >> 8), (u8)(addr & 0xFF)};

	ret = iReadRegI2C(pu_send_cmd, 2, (u8*)buf, 1, QT_E2PROM_WRITE_ID);
	if (ret < 0)
		LOG_INF("read data from qtech otp e2prom failed!\n");

	return ret;
}

static void set_dummy()
{
	LOG_INF("dummyline = %d, dummypixels = %d \n", imgsensor.dummy_line, imgsensor.dummy_pixel);

	write_cmos_sensor(0x0104,0x01);//group hold
	write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
	write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);	  
	write_cmos_sensor(0x0342, imgsensor.line_length >> 8);
	write_cmos_sensor(0x0343, imgsensor.line_length & 0xFF);
	write_cmos_sensor(0x0104,0x00);
	
}	/*	set_dummy  */


static void set_max_framerate(UINT16 framerate)
{
	kal_int16 dummy_line;
	kal_uint32 frame_length = imgsensor.frame_length;

	LOG_INF("framerate = %d\n ", framerate);
   
	frame_length = (10 * imgsensor.pclk) / framerate / imgsensor.line_length;
	frame_length = (frame_length + 1) / 2 * 2;
	spin_lock(&imgsensor_drv_lock);
	dummy_line = frame_length - imgsensor.frame_length;
	if (dummy_line < 0)
		imgsensor.dummy_line = 0;
	else
		imgsensor.dummy_line = dummy_line;
	imgsensor.frame_length += imgsensor.dummy_line;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
	{
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	}
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}	/*	set_max_framerate  */


static void write_shutter(kal_uint16 shutter)
{
	kal_uint16 realtime_fps = 0;
	kal_uint32 line_length = 0;
	//kal_uint32 frame_length = 0;
	LOG_INF("enter shutter =%d\n", shutter);
	   
	
	// if shutter bigger than frame_length, should extend frame length first
	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)		
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;

	imgsensor.frame_length = (imgsensor.frame_length + 1) / 2 * 2;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);

	
	if (shutter < imgsensor_info.min_shutter)
		shutter = imgsensor_info.min_shutter;

	/* called by capture with long exposure case */
        if (shutter == 6) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.line_length = imgsensor_info.cap.linelength;
		spin_unlock(&imgsensor_drv_lock);

		write_cmos_sensor(0x0104,0x01);//group hold
		write_cmos_sensor(0x0342, imgsensor.line_length >> 8);
		write_cmos_sensor(0x0343, imgsensor.line_length & 0xFF);
		write_cmos_sensor(0x0104,0x00);
	} else if (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) {
		line_length = shutter * imgsensor_info.cap.linelength / (imgsensor_info.max_frame_length - imgsensor_info.margin);
		line_length = (line_length + 1) / 2 * 2;
		if (line_length > 0xFFF0)
			line_length = 0xFFF0;

		write_cmos_sensor(0x0104,0x01);//group hold
		write_cmos_sensor(0x0342, line_length >> 8);
		write_cmos_sensor(0x0343, line_length & 0xFF);
		write_cmos_sensor(0x0104,0x00);

		imgsensor.line_length = line_length;
	}

	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

	if (imgsensor.autoflicker_en) { 
		realtime_fps = imgsensor.pclk * 10 / (imgsensor.line_length * imgsensor.frame_length);
		if (realtime_fps >= 297 && realtime_fps <= 305) {
			set_max_framerate(296);
			write_cmos_sensor(0x0104, 0x01);//group hold
		} else if (realtime_fps >= 147 && realtime_fps <= 150) {
			set_max_framerate(146);
			write_cmos_sensor(0x0104, 0x01);//group hold
		} else {
			// Extend frame length
			write_cmos_sensor(0x0104, 0x01);//group hold
			write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
			write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
		}
	} else {
		// Extend frame length
		write_cmos_sensor(0x0104, 0x01);//group hold
		write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
		write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
	}

	// Update Shutter
	write_cmos_sensor(0x0202, (shutter>>8) & 0xFF);
	write_cmos_sensor(0x0203,  shutter & 0xFF);
	write_cmos_sensor(0x0104, 0x00);
	
	LOG_INF("shutter =%d, framelength =%d, linelength=%d\n", shutter,imgsensor.frame_length, imgsensor.line_length);

	//LOG_INF("frame_length = %d ", frame_length);
	
}	/*	write_shutter  */



/*************************************************************************
* FUNCTION
*	set_shutter
*
* DESCRIPTION
*	This function set e-shutter of sensor to change exposure time.
*
* PARAMETERS
*	iShutter : exposured lines
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static void set_shutter(kal_uint16 shutter)
{
	unsigned long flags;
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
	
	write_shutter(shutter);
}	/*	set_shutter */



static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint8 GainTCheckIndex;
	
	for (GainTCheckIndex = 0; GainTCheckIndex < (MaxGainIndex-1); GainTCheckIndex++) 
	{
		if(gain <SensorGainMapping[GainTCheckIndex][0])
		{	 
			break;
		}
		
		if(gain < SensorGainMapping[GainTCheckIndex][0])
		{				 
			return SensorGainMapping[GainTCheckIndex][1];	   
		}
			
	}
	return SensorGainMapping[GainTCheckIndex-1][1];
}



/*************************************************************************
* FUNCTION
*	set_gain
*
* DESCRIPTION
*	This function is to set global gain to sensor.
*
* PARAMETERS
*	iGain : sensor global gain(base: 0x40)
*
* RETURNS
*	the actually gain set to sensor.
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain;


	if (gain < BASEGAIN || gain > 32 * BASEGAIN) {
		LOG_INF("Error gain setting");

		if (gain < BASEGAIN)
			gain = BASEGAIN;
		else if (gain > 32 * BASEGAIN)
			gain = 32 * BASEGAIN;		 
	}
 
	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain; 
	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

	write_cmos_sensor(0x0104,0x01);//group hold
	write_cmos_sensor(0x0205,reg_gain);
	write_cmos_sensor(0x0104,0x00);
	
	return gain;
}	/*	set_gain  */


static void ihdr_write_shutter_gain(kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{
  //Don't support
}


static void set_mirror_flip(kal_uint8 image_mirror)
{
	LOG_INF("image_mirror = %d\n", image_mirror);

	switch (image_mirror)
	{
		case IMAGE_NORMAL:
			write_cmos_sensor(0x0101, 0x00);
			break;
		case IMAGE_H_MIRROR:
			write_cmos_sensor(0x0101, 0x01);
			break;
		case IMAGE_V_MIRROR:
			write_cmos_sensor(0x0101, 0x02);
			break;
		case IMAGE_HV_MIRROR:
			write_cmos_sensor(0x0101, 0x03);
			break;
		default:
			LOG_INF("Error image_mirror setting\n");
	}
}


/*************************************************************************
* FUNCTION
*	night_mode
*
* DESCRIPTION
*	This function night mode of sensor.
*
* PARAMETERS
*	bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static void night_mode(kal_bool enable)
{
/*No Need to implement this function*/ 
}	/*	night_mode	*/


static void sensor_init(void)
{
	LOG_INF("E\n");
}

	/*	sensor_init  */


static void preview_setting(void)
{

	write_cmos_sensor(0x0100,0x00);//stream off

	write_cmos_sensor(0x0305,0x04);
	write_cmos_sensor(0x0307,0x87);
	write_cmos_sensor(0x303C,0x4B);
	write_cmos_sensor(0x30A4,0x02);
	write_cmos_sensor(0x0112,0x0A);
	write_cmos_sensor(0x0113,0x0A);
	
	write_cmos_sensor(0x0340, ((imgsensor_info.pre.framelength >> 8) & 0xFF)); // framelength 2400
	write_cmos_sensor(0x0341, (imgsensor_info.pre.framelength & 0xFF));	   
	write_cmos_sensor(0x0342, ((imgsensor_info.pre.linelength >> 8) & 0xFF)); //linelength:1124   [actually 1124*2]
	write_cmos_sensor(0x0343, (imgsensor_info.pre.linelength & 0xFF));
	
	write_cmos_sensor(0x0344,0x00);
	write_cmos_sensor(0x0345,0x08);
	write_cmos_sensor(0x0346,0x00);
	write_cmos_sensor(0x0347,0x08);
	write_cmos_sensor(0x0348,0x07);
	write_cmos_sensor(0x0349,0x87);
	write_cmos_sensor(0x034A,0x04);
	write_cmos_sensor(0x034B,0x3F);
	
	write_cmos_sensor(0x034C,0x07);//x output size 1920
	write_cmos_sensor(0x034D,0x80);
	write_cmos_sensor(0x034E,0x04);//y output size 1080
	write_cmos_sensor(0x034F,0x38);
	
	write_cmos_sensor(0x0381,0x01);
	write_cmos_sensor(0x0383,0x01);
	write_cmos_sensor(0x0385,0x01);
	write_cmos_sensor(0x0387,0x01);
	write_cmos_sensor(0x3048,0x00);
	write_cmos_sensor(0x304E,0x0A);
	write_cmos_sensor(0x3050,0x01);
	write_cmos_sensor(0x309B,0x00);
	write_cmos_sensor(0x30D5,0x00);
	write_cmos_sensor(0x3301,0x00);
	write_cmos_sensor(0x3318,0x61);

	write_cmos_sensor(0x0100,0x01);//stream on

}	/*	preview_setting  */

static void capture_setting(kal_uint16 currefps)
{
	LOG_INF("E! currefps:%d\n",currefps);

	write_cmos_sensor(0x0100,0x00);//stream off

	write_cmos_sensor(0x0305,0x04);
	write_cmos_sensor(0x0307,0x87);
	write_cmos_sensor(0x303C,0x4B);
	write_cmos_sensor(0x30A4,0x02);
	write_cmos_sensor(0x0112,0x0A);
	write_cmos_sensor(0x0113,0x0A);

	if (currefps == 240)
	{ //24fps for PIP
		write_cmos_sensor(0x0340, ((imgsensor_info.cap1.framelength >> 8) & 0xFF)); // framelength 3000
		write_cmos_sensor(0x0341, (imgsensor_info.cap1.framelength & 0xFF)); 	   
		write_cmos_sensor(0x0342, ((imgsensor_info.cap1.linelength >> 8) & 0xFF)); //linelength:1124   [actually 1124*2]
		write_cmos_sensor(0x0343, (imgsensor_info.cap1.linelength & 0xFF));
	}
	else
	{
		write_cmos_sensor(0x0340, ((imgsensor_info.cap.framelength >> 8) & 0xFF)); // framelength 2400
		write_cmos_sensor(0x0341, (imgsensor_info.cap.framelength & 0xFF));
		write_cmos_sensor(0x0342, ((imgsensor_info.cap.linelength >> 8) & 0xFF)); //linelength:1124
		write_cmos_sensor(0x0343, (imgsensor_info.cap.linelength & 0xFF));
	}

	write_cmos_sensor(0x0342,0x04);//linelength:1124
	write_cmos_sensor(0x0343,0x64);

	write_cmos_sensor(0x0344,0x00);
	write_cmos_sensor(0x0345,0x08);
	write_cmos_sensor(0x0346,0x00);
	write_cmos_sensor(0x0347,0x08);
	write_cmos_sensor(0x0348,0x07);
	write_cmos_sensor(0x0349,0x87);
	write_cmos_sensor(0x034A,0x04);
	write_cmos_sensor(0x034B,0x3F);

	write_cmos_sensor(0x034C,0x07);//x output size 1920
	write_cmos_sensor(0x034D,0x80);
	write_cmos_sensor(0x034E,0x04);//y output size 1080
	write_cmos_sensor(0x034F,0x38);

	write_cmos_sensor(0x0381,0x01);
	write_cmos_sensor(0x0383,0x01);
	write_cmos_sensor(0x0385,0x01);
	write_cmos_sensor(0x0387,0x01);
	write_cmos_sensor(0x3048,0x00);
	write_cmos_sensor(0x304E,0x0A);
	write_cmos_sensor(0x3050,0x01);
	write_cmos_sensor(0x309B,0x00);
	write_cmos_sensor(0x30D5,0x00);
	write_cmos_sensor(0x3301,0x00);
	write_cmos_sensor(0x3318,0x61);

	write_cmos_sensor(0x0100,0x01);//stream on
}	
	
		

static void normal_video_setting(kal_uint16 currefps)
{
	LOG_INF("E! currefps:%d\n",currefps);
	
	write_cmos_sensor(0x0100,0x00);//stream off
	
	write_cmos_sensor(0x0305,0x04);
	write_cmos_sensor(0x0307,0x87);
	write_cmos_sensor(0x303C,0x4B);
	write_cmos_sensor(0x30A4,0x02);
	write_cmos_sensor(0x0112,0x0A);
	write_cmos_sensor(0x0113,0x0A);
	
	write_cmos_sensor(0x0340, ((imgsensor_info.normal_video.framelength >> 8) & 0xFF));  // 2400
	write_cmos_sensor(0x0341, (imgsensor_info.normal_video.framelength & 0xFF));	
	write_cmos_sensor(0x0342, ((imgsensor_info.normal_video.linelength >> 8) & 0xFF)); // 1124   [actually 1124*2]
	write_cmos_sensor(0x0343, (imgsensor_info.normal_video.linelength & 0xFF));
	
	write_cmos_sensor(0x0344,0x00);
	write_cmos_sensor(0x0345,0x08);
	write_cmos_sensor(0x0346,0x00);
	write_cmos_sensor(0x0347,0x08);
	write_cmos_sensor(0x0348,0x07);
	write_cmos_sensor(0x0349,0x87);
	write_cmos_sensor(0x034A,0x04);
	write_cmos_sensor(0x034B,0x3F);
	
	write_cmos_sensor(0x034C,0x07);//x output size 1920
	write_cmos_sensor(0x034D,0x80);
	write_cmos_sensor(0x034E,0x04);//y output size 1080
	write_cmos_sensor(0x034F,0x38);
	
	write_cmos_sensor(0x0381,0x01);
	write_cmos_sensor(0x0383,0x01);
	write_cmos_sensor(0x0385,0x01);
	write_cmos_sensor(0x0387,0x01);
	write_cmos_sensor(0x3048,0x00);
	write_cmos_sensor(0x304E,0x0A);
	write_cmos_sensor(0x3050,0x01);
	write_cmos_sensor(0x309B,0x00);
	write_cmos_sensor(0x30D5,0x00);
	write_cmos_sensor(0x3301,0x00);
	write_cmos_sensor(0x3318,0x61);

	write_cmos_sensor(0x0100,0x01);//stream on
		
}
static void hs_video_setting()
{
	LOG_INF("E\n");

	write_cmos_sensor(0x0100,0x00);//stream off
	
	write_cmos_sensor(0x0305,0x04);
	write_cmos_sensor(0x0307,0x87);
	write_cmos_sensor(0x303C,0x4B);
	write_cmos_sensor(0x30A4,0x02);
	write_cmos_sensor(0x0112,0x0A);
	write_cmos_sensor(0x0113,0x0A);
	
	write_cmos_sensor(0x0340, ((imgsensor_info.hs_video.framelength >> 8) & 0xFF));  // 1200
	write_cmos_sensor(0x0341, (imgsensor_info.hs_video.framelength & 0xFF));	
	write_cmos_sensor(0x0342, ((imgsensor_info.hs_video.linelength >> 8) & 0xFF)); // 1124	 [actually 1124*2]
	write_cmos_sensor(0x0343, (imgsensor_info.hs_video.linelength & 0xFF));
	
	write_cmos_sensor(0x0344,0x00);
	write_cmos_sensor(0x0345,0x08);
	write_cmos_sensor(0x0346,0x00);
	write_cmos_sensor(0x0347,0x08);
	write_cmos_sensor(0x0348,0x07);
	write_cmos_sensor(0x0349,0x87);
	write_cmos_sensor(0x034A,0x04);
	write_cmos_sensor(0x034B,0x3F);
	
	write_cmos_sensor(0x034C,0x07);//x output size 1920
	write_cmos_sensor(0x034D,0x80);
	write_cmos_sensor(0x034E,0x04);//y output size 1080
	write_cmos_sensor(0x034F,0x38);
	
	write_cmos_sensor(0x0381,0x01);
	write_cmos_sensor(0x0383,0x01);
	write_cmos_sensor(0x0385,0x01);
	write_cmos_sensor(0x0387,0x01);
	write_cmos_sensor(0x3048,0x00);
	write_cmos_sensor(0x304E,0x0A);
	write_cmos_sensor(0x3050,0x01);
	write_cmos_sensor(0x309B,0x00);
	write_cmos_sensor(0x30D5,0x00);
	write_cmos_sensor(0x3301,0x00);
	write_cmos_sensor(0x3318,0x61);

	write_cmos_sensor(0x0100,0x01);//stream on

}

static void slim_video_setting()
{
	LOG_INF("E\n");

	write_cmos_sensor(0x0100,0x00);//stream off

	write_cmos_sensor(0x0305,0x04);
	write_cmos_sensor(0x0307,0x87);
	write_cmos_sensor(0x303C,0x4B);
	write_cmos_sensor(0x30A4,0x02);
	write_cmos_sensor(0x0112,0x0A);
	write_cmos_sensor(0x0113,0x0A);
	
	write_cmos_sensor(0x0340, ((imgsensor_info.slim_video.framelength >> 8) & 0xFF));  // 2400
	write_cmos_sensor(0x0341, (imgsensor_info.slim_video.framelength & 0xFF));	
	write_cmos_sensor(0x0342, ((imgsensor_info.slim_video.linelength >> 8) & 0xFF)); // 1124	 [actually 1124*2]
	write_cmos_sensor(0x0343, (imgsensor_info.slim_video.linelength & 0xFF));
	
	write_cmos_sensor(0x0344,0x00);
	write_cmos_sensor(0x0345,0x08);
	write_cmos_sensor(0x0346,0x00);
	write_cmos_sensor(0x0347,0x08);
	write_cmos_sensor(0x0348,0x07);
	write_cmos_sensor(0x0349,0x87);
	write_cmos_sensor(0x034A,0x04);
	write_cmos_sensor(0x034B,0x3F);
	
	write_cmos_sensor(0x034C,0x07);//x output size 1920
	write_cmos_sensor(0x034D,0x80);
	write_cmos_sensor(0x034E,0x04);//y output size 1080
	write_cmos_sensor(0x034F,0x38);
	
	write_cmos_sensor(0x0381,0x01);
	write_cmos_sensor(0x0383,0x01);
	write_cmos_sensor(0x0385,0x01);
	write_cmos_sensor(0x0387,0x01);
	write_cmos_sensor(0x3048,0x00);
	write_cmos_sensor(0x304E,0x0A);
	write_cmos_sensor(0x3050,0x01);
	write_cmos_sensor(0x309B,0x00);
	write_cmos_sensor(0x30D5,0x00);
	write_cmos_sensor(0x3301,0x00);
	write_cmos_sensor(0x3318,0x61);

	write_cmos_sensor(0x0100,0x01);//stream on
}


/*************************************************************************
* FUNCTION
*	get_imgsensor_id
*
* DESCRIPTION
*	This function get the sensor ID 
*
* PARAMETERS
*	*sensorID : return the sensor ID 
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id) 
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	u8 module_id = 0;
	u16 addr[3] = {0x3508, 0x3510, 0x3518};
	int ret = 0;
	
	//module have defferent  i2c address;
	for (i = 0; i < 3; i++) {
		retry = 2;
	do {
		*sensor_id = ((read_cmos_sensor(0x0000) << 8) | read_cmos_sensor(0x0001));
		ret = imx208_qt_read_otp(addr[i], &module_id);
		if ((ret >= 0) && (*sensor_id == imgsensor_info.sensor_id) && (module_id == 0x06)) {				
			LOG_INF("i2c write id: 0x%x, sensor id: 0x%x, module_id:0x%x\n", imgsensor.i2c_write_id,*sensor_id, module_id);	  
			qtech_status = 0;
			*sensor_id = IMX208_SENSOR_ID;
			return ERROR_NONE;
		}	
		LOG_INF("Read sensor id fail, write id: 0x%x, sensor id:0x%x, module_id:0x%x\n", imgsensor.i2c_write_id,*sensor_id, module_id);
		retry--;
	} while(retry > 0);
	}

	if (*sensor_id == imgsensor_info.sensor_id && sunny_status == 1) {
		LOG_INF("no front camera, return sensor id: 0x%x\n", *sensor_id);
		qtech_status = 0;
		*sensor_id = IMX208_SENSOR_ID;
		return ERROR_NONE;
	} else if (*sensor_id == imgsensor_info.sensor_id) {
		qtech_status = 1;
	}

	// if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF 
	*sensor_id = 0xFFFFFFFF;
	return ERROR_SENSOR_CONNECT_FAIL;
}


/*************************************************************************
* FUNCTION
*	open
*
* DESCRIPTION
*	This function initialize the registers of CMOS sensor
*
* PARAMETERS
*	None
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 open(void)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint16 sensor_id = 0; 
	u8 module_id = 0;
	u16 addr[3] = {0x3508, 0x3510, 0x3518};
	int ret = 0;
	LOG_INF("PLATFORM:MT6595,MIPI 2LANE\n");
	LOG_INF("preview/video/slim_video/capture sync 30fps,hs_video 60fps\n");
	
	for (i = 0; i < 3; i++) {
		retry = 2;
	do {
		sensor_id = ((read_cmos_sensor(0x0000) << 8) | read_cmos_sensor(0x0001));
		ret = imx208_qt_read_otp(addr[i], &module_id);
		if ((ret >= 0) && (sensor_id == imgsensor_info.sensor_id) && (module_id == 0x06)) {				
			LOG_INF("i2c write id: 0x%x, sensor id: 0x%x, module_id:0x%x\n", imgsensor.i2c_write_id,sensor_id, module_id);	  
			break;
		}	
		LOG_INF("Read sensor id fail, write id: 0x%x, sensor id: 0x%x, module_id:0x%x\n", imgsensor.i2c_write_id,sensor_id, module_id);
		retry--;
	} while(retry > 0);
		if ((ret >= 0) && (sensor_id == imgsensor_info.sensor_id) && (module_id == 0x06))
			break;
	}

	if ((imgsensor_info.sensor_id != sensor_id) || (qtech_status == 1 && sunny_status == 0))
		return ERROR_SENSOR_CONNECT_FAIL;
	
	LOG_INF("imx208qtech open successfully.\n");
	/* initail sequence write in  */
	sensor_init();

	spin_lock(&imgsensor_drv_lock);
	imgsensor.autoflicker_en= KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.shutter = 0x3D0;
	imgsensor.gain = 0x100;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_en = 0;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}	/*	open  */



/*************************************************************************
* FUNCTION
*	close
*
* DESCRIPTION
*	
*
* PARAMETERS
*	None
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 close(void)
{
	LOG_INF("E\n");
	qtech_status = 0;

	/*No Need to implement this function*/ 
	
	return ERROR_NONE;
}	/*	close  */


/*************************************************************************
* FUNCTION
* preview
*
* DESCRIPTION
*	This function start the sensor preview.
*
* PARAMETERS
*	*image_window : address pointer of pixel numbers in one period of HSYNC
*  *sensor_config_data : address pointer of line numbers in one period of VSYNC
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength; 
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	return ERROR_NONE;
}	/*	preview   */

/*************************************************************************
* FUNCTION
*	capture
*
* DESCRIPTION
*	This function setup the CMOS sensor in capture MY_OUTPUT mode
*
* PARAMETERS
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
						  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	imgsensor.current_fps = 300;
	if (imgsensor.current_fps == 240) {
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;  
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else {
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;  
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.current_fps = imgsensor_info.cap.max_framerate;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);

	capture_setting(imgsensor.current_fps); 
	
	if(imx208_During_testpattern == KAL_TRUE)
	{
		write_cmos_sensor(0x3282,0x01);//DPU OFF
		
		write_cmos_sensor(0x0600,0x00);
		write_cmos_sensor(0x0601,0x02);
		
	}

	return ERROR_NONE;
}	/* capture() */
static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;  
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.current_fps = imgsensor_info.normal_video.max_framerate;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting(imgsensor.current_fps);
	
	return ERROR_NONE;
}	/*	normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength; 
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.current_fps = imgsensor_info.hs_video.max_framerate;;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	
	return ERROR_NONE;
}	/*	hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength; 
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.current_fps = imgsensor_info.slim_video.max_framerate;;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();
	
	return ERROR_NONE;
}	/*	slim_video	 */



static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *
sensor_resolution)
{
	LOG_INF("E\n");
	sensor_resolution->SensorFullWidth = imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight = imgsensor_info.cap.grabwindow_height;
	
	sensor_resolution->SensorPreviewWidth = imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight = imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorVideoWidth = imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight = imgsensor_info.normal_video.grabwindow_height;		

	
	sensor_resolution->SensorHighSpeedVideoWidth	 = imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight	 = imgsensor_info.hs_video.grabwindow_height;
	
	sensor_resolution->SensorSlimVideoWidth	 = imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight	 = imgsensor_info.slim_video.grabwindow_height;
	return ERROR_NONE;
}	/*	get_resolution	*/

static kal_uint32 get_info(MSDK_SCENARIO_ID_ENUM scenario_id,
					  MSDK_SENSOR_INFO_STRUCT *sensor_info,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);


	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW; /* not use */
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW; // inverse with datasheet
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4; /* not use */
	sensor_info->SensorResetActiveHigh = FALSE; /* not use */
	sensor_info->SensorResetDelayCount = 5; /* not use */

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;

	sensor_info->SensorOutputDataFormat = imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame; 
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame; 
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame = imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;
	
	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame; 		 /* The frame of setting shutter default 0 for TG int */
	sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;	/* The frame of setting sensor gain */
	sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;	
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
    sensor_info->MIPIsensorType = MIPI_OPHY_NCSI2;
    sensor_info->SettleDelayMode = MIPI_SETTLEDELAY_AUTO;

	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num; 
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3; /* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2; /* not use */
	sensor_info->SensorPixelClockCount = 3; /* not use */
	sensor_info->SensorDataLatchCount = 2; /* not use */
	
	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0; 
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;  // 0 is default 1x
	sensor_info->SensorHightSampling = 0;	// 0 is default 1x 
	sensor_info->SensorPacketECCOrder = 1;

	switch (scenario_id) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			sensor_info->SensorGrabStartX = imgsensor_info.pre.startx; 
			sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;		
			
			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
			
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			sensor_info->SensorGrabStartX = imgsensor_info.cap.startx; 
			sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;
				  
			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.cap.mipi_data_lp2hs_settle_dc; 

			break;	 
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			
			sensor_info->SensorGrabStartX = imgsensor_info.normal_video.startx; 
			sensor_info->SensorGrabStartY = imgsensor_info.normal_video.starty;
	   
			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc; 

			break;	  
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:			
			sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx; 
			sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;
				  
			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc; 

			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			sensor_info->SensorGrabStartX = imgsensor_info.slim_video.startx; 
			sensor_info->SensorGrabStartY = imgsensor_info.slim_video.starty;
				  
			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc; 

			break;
		default:			
			sensor_info->SensorGrabStartX = imgsensor_info.pre.startx; 
			sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;		
			
			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
			break;
	}
	
	return ERROR_NONE;
}	/*	get_info  */


static kal_uint32 control(MSDK_SCENARIO_ID_ENUM scenario_id, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);
	switch (scenario_id) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			preview(image_window, sensor_config_data);
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			capture(image_window, sensor_config_data);
			break;	
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			normal_video(image_window, sensor_config_data);
			break;	  
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			hs_video(image_window, sensor_config_data);
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			slim_video(image_window, sensor_config_data);
			break;	  
		default:
			LOG_INF("Error ScenarioId setting");
			preview(image_window, sensor_config_data);
			return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}	/* control() */



static kal_uint32 set_video_mode(UINT16 framerate)
{
	LOG_INF("framerate = %d\n ", framerate);
	// SetVideoMode Function should fix framerate
	if (framerate == 0)
		// Dynamic frame rate
		return ERROR_NONE;
	
	spin_lock(&imgsensor_drv_lock);
	if ((framerate == 30) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 296;
	else if ((framerate == 15) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 146;
	else
		imgsensor.current_fps = 10 * framerate;
	spin_unlock(&imgsensor_drv_lock);
	set_max_framerate(imgsensor.current_fps);

	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
	LOG_INF("enable = %d, framerate = %d \n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable) 	  
		imgsensor.autoflicker_en = KAL_TRUE;
	else //Cancel Auto flick
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate) 
{
	kal_int16 dummyLine;
	kal_uint32 lineLength,frameHeight;
  
	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			frameHeight = (10 * imgsensor_info.pre.pclk) / framerate / imgsensor_info.pre.linelength;
			frameHeight = (frameHeight + 1) / 2 * 2;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = frameHeight - imgsensor_info.pre.framelength;
			if (imgsensor.dummy_line < 0)
				imgsensor.dummy_line = 0;
			imgsensor.frame_length += imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			set_dummy();			
			break;			
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			frameHeight = (10 * imgsensor_info.normal_video.pclk) / framerate / imgsensor_info.normal_video.linelength;
			frameHeight = (frameHeight + 1) / 2 * 2;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = frameHeight - imgsensor_info.normal_video.framelength;
			if (imgsensor.dummy_line < 0)
				imgsensor.dummy_line = 0;			
			imgsensor.frame_length += imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			set_dummy();			
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		//case MSDK_SCENARIO_ID_CAMERA_ZSD:			
			frameHeight = (10 * imgsensor_info.cap.pclk) / framerate / imgsensor_info.cap.linelength;
			frameHeight = (frameHeight + 1) / 2 * 2;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = frameHeight - imgsensor_info.cap.framelength;
			if (imgsensor.dummy_line < 0)
				imgsensor.dummy_line = 0;
			imgsensor.frame_length += imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			set_dummy();			
			break;	
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			frameHeight = (10 * imgsensor_info.hs_video.pclk) / framerate / imgsensor_info.hs_video.linelength;
			frameHeight = (frameHeight + 1) / 2 * 2;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = frameHeight - imgsensor_info.hs_video.framelength;
			if (imgsensor.dummy_line < 0)
				imgsensor.dummy_line = 0;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			set_dummy();			
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			frameHeight = (10 * imgsensor_info.slim_video.pclk) / framerate / imgsensor_info.slim_video.linelength;
			frameHeight = (frameHeight + 1) / 2 * 2;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = frameHeight - imgsensor_info.slim_video.framelength;
			if (imgsensor.dummy_line < 0)
				imgsensor.dummy_line = 0;			
			imgsensor.frame_length += imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			set_dummy();			
		default:			
			LOG_INF("error scenario_id = %d\n", scenario_id);
			break;
	}	
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate) 
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	switch (scenario_id) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			*framerate = imgsensor_info.pre.max_framerate;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*framerate = imgsensor_info.normal_video.max_framerate;
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*framerate = imgsensor_info.cap.max_framerate;
			break;		
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*framerate = imgsensor_info.hs_video.max_framerate;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO: 
			*framerate = imgsensor_info.slim_video.max_framerate;
			break;
		default:
			break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("enable: %d\n", enable);

    if(enable)
    {
        imx208_During_testpattern = KAL_TRUE;

		write_cmos_sensor(0x3282,0x01);//DPU OFF
		write_cmos_sensor(0x0600,0x00);//test pattern
		write_cmos_sensor(0x0601,0x02);
		
    }
	else
	{
        imx208_During_testpattern = KAL_FALSE;
		write_cmos_sensor(0x3282,0x00);//DPU ON
		write_cmos_sensor(0x0600,0x00);//test pattern stop
		write_cmos_sensor(0x0601,0x00);
	}

	return ERROR_NONE;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
							 UINT8 *feature_para,UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16=(UINT16 *) feature_para;
	UINT16 *feature_data_16=(UINT16 *) feature_para;
	UINT32 *feature_return_para_32=(UINT32 *) feature_para;
	UINT32 *feature_data_32=(UINT32 *) feature_para;
	
	SENSOR_WINSIZE_INFO_STRUCT *wininfo;	
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data=(MSDK_SENSOR_REG_INFO_STRUCT *) 
feature_para;
 
	LOG_INF("feature_id = %d\n", feature_id);
	switch (feature_id) {
		case SENSOR_FEATURE_GET_PERIOD:
			*feature_return_para_16++ = imgsensor.line_length;
			*feature_return_para_16 = imgsensor.frame_length;
			*feature_para_len=4;
			break;
		case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:	 
			*feature_return_para_32 = imgsensor.pclk;
			*feature_para_len=4;
			break;		   
		case SENSOR_FEATURE_SET_ESHUTTER:
			set_shutter(*feature_data_16);
			break;
		case SENSOR_FEATURE_SET_NIGHTMODE:
			night_mode((BOOL) *feature_data_16);
			break;
		case SENSOR_FEATURE_SET_GAIN:		
			set_gain((UINT16) *feature_data_16);
			break;
		case SENSOR_FEATURE_SET_FLASHLIGHT:
			break;
		case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
			break;
		case SENSOR_FEATURE_SET_REGISTER:
			write_cmos_sensor(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
			break;
		case SENSOR_FEATURE_GET_REGISTER:
			sensor_reg_data->RegData = read_cmos_sensor(sensor_reg_data->RegAddr);
			break;
		case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
			// get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE
			// if EEPROM does not exist in camera module.
			*feature_return_para_32=LENS_DRIVER_ID_DO_NOT_CARE;
			*feature_para_len=4;
			break;
		case SENSOR_FEATURE_SET_VIDEO_MODE:
			set_video_mode(*feature_data_16);
			break; 
		case SENSOR_FEATURE_CHECK_SENSOR_ID:
			get_imgsensor_id(feature_return_para_32); 
			break; 
		case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
			set_auto_flicker_mode((BOOL)*feature_data_16,*(feature_data_16+1));
			break;
		case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
			set_max_framerate_by_scenario((MSDK_SCENARIO_ID_ENUM)*feature_data_32, *(feature_data_32+1));
			break;
		case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
			get_default_framerate_by_scenario((MSDK_SCENARIO_ID_ENUM)*feature_data_32, (MUINT32 *)(*(feature_data_32+1)));
			break;
		case SENSOR_FEATURE_SET_TEST_PATTERN:
			set_test_pattern_mode((BOOL)*feature_data_16);
			break;
		case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE: //for factory mode auto testing			 
			*feature_return_para_32 = imgsensor_info.checksum_value;
			*feature_para_len=4;							 
			break;				
		case SENSOR_FEATURE_SET_FRAMERATE:
			LOG_INF("current fps :%d\n", *feature_data_16);
			spin_lock(&imgsensor_drv_lock);
			imgsensor.current_fps = *feature_data_16;
			spin_unlock(&imgsensor_drv_lock);		
			break;
		case SENSOR_FEATURE_SET_HDR:
			LOG_INF("ihdr enable :%d\n", *feature_data_16);
			spin_lock(&imgsensor_drv_lock);
			imgsensor.ihdr_en = *feature_data_16;
			spin_unlock(&imgsensor_drv_lock);		
			break;
		case SENSOR_FEATURE_GET_CROP_INFO:
			LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n", *feature_data_32);
			wininfo = (SENSOR_WINSIZE_INFO_STRUCT *)(*(feature_data_32+1));
		
			switch (*feature_data_32) {
				case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
					memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[1],sizeof(SENSOR_WINSIZE_INFO_STRUCT));
					break;	  
				case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
					memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[2],sizeof(SENSOR_WINSIZE_INFO_STRUCT));
					break;
				case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
					memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[3],sizeof(SENSOR_WINSIZE_INFO_STRUCT));
					break;
				case MSDK_SCENARIO_ID_SLIM_VIDEO:
					memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[4],sizeof(SENSOR_WINSIZE_INFO_STRUCT));
					break;
				case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
				default:
					memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[0],sizeof(SENSOR_WINSIZE_INFO_STRUCT));
					break;
			}
		case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
			break;
			LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",*feature_data_32,*(feature_data_32+1),*(feature_data_32+2)); 
			ihdr_write_shutter_gain(*feature_data_32,*(feature_data_32+1),*(feature_data_32+2));	
			break;
		default:
			break;
	}
  
	return ERROR_NONE;
}	/*	feature_control()  */

static SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 IMX208_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc!=NULL)
		*pfFunc=&sensor_func;
	return ERROR_NONE;
}	/*	IMX208_MIPI_RAW_SensorInit	*/
