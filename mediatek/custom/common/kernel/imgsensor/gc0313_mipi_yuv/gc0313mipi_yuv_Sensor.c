/*****************************************************************************
 *
 * Filename:
 * ---------
 *   gc0313yuv_Sensor.c
 *
 * Project:
 * --------
 *   MAUI
 *
 * Description:
 * ------------
 *   Image sensor driver function
 *   V1.2.3
 *
 * Author:
 * -------
 *   Leo
 *
 *=============================================================
 *             HISTORY
 * Below this line, this part is controlled by GCoreinc. DO NOT MODIFY!!
 *------------------------------------------------------------------------------
 * $Log$
 * 2012.02.29  kill bugs
 *   
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by GCoreinc. DO NOT MODIFY!!
 *=============================================================
 ******************************************************************************/
#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>

#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "kd_camera_feature.h"

#include "gc0313mipi_yuv_Sensor.h"
#include "gc0313mipi_yuv_Camera_Sensor_para.h"
#include "gc0313mipi_yuv_CameraCustomized.h"

#define GC0313MIPIYUV_DEBUG

#ifdef GC0313MIPIYUV_DEBUG
#define SENSORDB(fmt, arg...) printk("%s: " fmt "\n", __FUNCTION__ ,##arg)
#else
#define SENSORDB(x,...)
#endif

extern int iReadRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u8 * a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u16 i2cId);

kal_uint16 GC0313MIPI_write_cmos_sensor(kal_uint8 addr, kal_uint8 para)
{
    char puSendCmd[2] = {(char)(addr & 0xFF) , (char)(para & 0xFF)};
	
	return iWriteRegI2C(puSendCmd , 2, GC0313MIPI_WRITE_ID);

}
kal_uint16 GC0313MIPI_read_cmos_sensor(kal_uint8 addr)
{
	kal_uint16 get_byte=0;
    char puSendCmd = { (char)(addr & 0xFF) };
	iReadRegI2C(&puSendCmd , 1, (u8*)&get_byte, 1, GC0313MIPI_READ_ID);
	
    return get_byte;
}


/*******************************************************************************
 * // Adapter for Winmo typedef
 ********************************************************************************/
#define WINMO_USE 0

#define Sleep(ms) mdelay(ms)
#define RETAILMSG(x,...)
#define TEXT

kal_bool   GC0313MIPI_MPEG4_encode_mode = KAL_FALSE;
kal_uint16 GC0313MIPI_dummy_pixels = 0, GC0313MIPI_dummy_lines = 0;
kal_bool   GC0313MIPI_MODE_CAPTURE = KAL_FALSE;
kal_bool   GC0313MIPI_NIGHT_MODE = KAL_FALSE;

kal_uint32 GC0313MIPI_isp_master_clock;
static kal_uint32 GC0313MIPI_g_fPV_PCLK = 26;

kal_uint8 GC0313MIPI_sensor_write_I2C_address = GC0313MIPI_WRITE_ID;
kal_uint8 GC0313MIPI_sensor_read_I2C_address = GC0313MIPI_READ_ID;

UINT8 GC0313MIPIPixelClockDivider=0;

MSDK_SENSOR_CONFIG_STRUCT GC0313MIPISensorConfigData;
#define GC0313_TEST_PATTERN_CHECKSUM 0x17c06485

#define GC0313MIPI_SET_PAGE0 	GC0313MIPI_write_cmos_sensor(0xfe, 0x00)
#define GC0313MIPI_SET_PAGE1 	GC0313MIPI_write_cmos_sensor(0xfe, 0x01)
#define GC0313MIPI_SET_PAGE2 	GC0313MIPI_write_cmos_sensor(0xfe, 0x02)

/*************************************************************************
 * FUNCTION
 *	GC0313MIPI_SetShutter
 *
 * DESCRIPTION
 *	This function set e-shutter of GC0313MIPI to change exposure time.
 *
 * PARAMETERS
 *   iShutter : exposured lines
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
void GC0313MIPI_Set_Shutter(kal_uint16 iShutter)
{
} /* Set_GC0313MIPI_Shutter */


/*************************************************************************
 * FUNCTION
 *	GC0313MIPI_read_Shutter
 *
 * DESCRIPTION
 *	This function read e-shutter of GC0313MIPI .
 *
 * PARAMETERS
 *  None
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
kal_uint16 GC0313MIPI_Read_Shutter(void)
{
    	kal_uint8 temp_reg1, temp_reg2;
	kal_uint16 shutter;

	temp_reg1 = GC0313MIPI_read_cmos_sensor(0x04);
	temp_reg2 = GC0313MIPI_read_cmos_sensor(0x03);

	shutter = (temp_reg1 & 0xFF) | (temp_reg2 << 8);

	return shutter;
} /* GC0313MIPI_read_shutter */


/*************************************************************************
 * FUNCTION
 *	GC0313MIPI_write_reg
 *
 * DESCRIPTION
 *	This function set the register of GC0313MIPI.
 *
 * PARAMETERS
 *	addr : the register index of GC0313MIPI
 *  para : setting parameter of the specified register of GC0313MIPI
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
void GC0313MIPI_write_reg(kal_uint32 addr, kal_uint32 para)
{
	GC0313MIPI_write_cmos_sensor(addr, para);
} /* GC0313MIPI_write_reg() */


/*************************************************************************
 * FUNCTION
 *	GC0313MIPI_read_cmos_sensor
 *
 * DESCRIPTION
 *	This function read parameter of specified register from GC0313MIPI.
 *
 * PARAMETERS
 *	addr : the register index of GC0313MIPI
 *
 * RETURNS
 *	the data that read from GC0313MIPI
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
kal_uint32 GC0313MIPI_read_reg(kal_uint32 addr)
{
	return GC0313MIPI_read_cmos_sensor(addr);
} /* OV7670_read_reg() */


/*************************************************************************
* FUNCTION
*	GC0313MIPI_awb_enable
*
* DESCRIPTION
*	This function enable or disable the awb (Auto White Balance).
*
* PARAMETERS
*	1. kal_bool : KAL_TRUE - enable awb, KAL_FALSE - disable awb.
*
* RETURNS
*	kal_bool : It means set awb right or not.
*
*************************************************************************/
static void GC0313MIPI_awb_enable(kal_bool enalbe)
{	 
   kal_uint16 temp_AWB_reg = 0;

   SENSORDB("enalbe = %d", enalbe);
	
	temp_AWB_reg = GC0313MIPI_read_cmos_sensor(0x42);
	
	if (enalbe)
	{
		GC0313MIPI_write_cmos_sensor(0x42, (temp_AWB_reg |0x02));
	}
	else
	{
		GC0313MIPI_write_cmos_sensor(0x42, (temp_AWB_reg & (~0x02)));
	}

}


/*************************************************************************
* FUNCTION
*	GC0313MIPI_GAMMA_Select
*
* DESCRIPTION
*	This function is served for FAE to select the appropriate GAMMA curve.
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
void GC0313MIPIGammaSelect(kal_uint32 GammaLvl)
{
    SENSORDB("GammaLvl = %d", GammaLvl);
	switch(GammaLvl)
	{
		case GC0313MIPI_RGB_Gamma_m1:						//smallest gamma curve
			GC0313MIPI_write_cmos_sensor(0xfe, 0x00);
			GC0313MIPI_write_cmos_sensor(0xbf, 0x06);
			GC0313MIPI_write_cmos_sensor(0xc0, 0x12);
			GC0313MIPI_write_cmos_sensor(0xc1, 0x22);
			GC0313MIPI_write_cmos_sensor(0xc2, 0x35);
			GC0313MIPI_write_cmos_sensor(0xc3, 0x4b);
			GC0313MIPI_write_cmos_sensor(0xc4, 0x5f);
			GC0313MIPI_write_cmos_sensor(0xc5, 0x72);
			GC0313MIPI_write_cmos_sensor(0xc6, 0x8d);
			GC0313MIPI_write_cmos_sensor(0xc7, 0xa4);
			GC0313MIPI_write_cmos_sensor(0xc8, 0xb8);
			GC0313MIPI_write_cmos_sensor(0xc9, 0xc8);
			GC0313MIPI_write_cmos_sensor(0xca, 0xd4);
			GC0313MIPI_write_cmos_sensor(0xcb, 0xde);
			GC0313MIPI_write_cmos_sensor(0xcc, 0xe6);
			GC0313MIPI_write_cmos_sensor(0xcd, 0xf1);
			GC0313MIPI_write_cmos_sensor(0xce, 0xf8);
			GC0313MIPI_write_cmos_sensor(0xcf, 0xfd);
			break;
		case GC0313MIPI_RGB_Gamma_m2:
			GC0313MIPI_write_cmos_sensor(0xBF, 0x08);
			GC0313MIPI_write_cmos_sensor(0xc0, 0x0F);
			GC0313MIPI_write_cmos_sensor(0xc1, 0x21);
			GC0313MIPI_write_cmos_sensor(0xc2, 0x32);
			GC0313MIPI_write_cmos_sensor(0xc3, 0x43);
			GC0313MIPI_write_cmos_sensor(0xc4, 0x50);
			GC0313MIPI_write_cmos_sensor(0xc5, 0x5E);
			GC0313MIPI_write_cmos_sensor(0xc6, 0x78);
			GC0313MIPI_write_cmos_sensor(0xc7, 0x90);
			GC0313MIPI_write_cmos_sensor(0xc8, 0xA6);
			GC0313MIPI_write_cmos_sensor(0xc9, 0xB9);
			GC0313MIPI_write_cmos_sensor(0xcA, 0xC9);
			GC0313MIPI_write_cmos_sensor(0xcB, 0xD6);
			GC0313MIPI_write_cmos_sensor(0xcC, 0xE0);
			GC0313MIPI_write_cmos_sensor(0xcD, 0xEE);
			GC0313MIPI_write_cmos_sensor(0xcE, 0xF8);
			GC0313MIPI_write_cmos_sensor(0xcF, 0xFF);
			break;
			
		case GC0313MIPI_RGB_Gamma_m3:			
			GC0313MIPI_write_cmos_sensor(0xBF, 0x0B);
			GC0313MIPI_write_cmos_sensor(0xc0, 0x16);
			GC0313MIPI_write_cmos_sensor(0xc1, 0x29);
			GC0313MIPI_write_cmos_sensor(0xc2, 0x3C);
			GC0313MIPI_write_cmos_sensor(0xc3, 0x4F);
			GC0313MIPI_write_cmos_sensor(0xc4, 0x5F);
			GC0313MIPI_write_cmos_sensor(0xc5, 0x6F);
			GC0313MIPI_write_cmos_sensor(0xc6, 0x8A);
			GC0313MIPI_write_cmos_sensor(0xc7, 0x9F);
			GC0313MIPI_write_cmos_sensor(0xc8, 0xB4);
			GC0313MIPI_write_cmos_sensor(0xc9, 0xC6);
			GC0313MIPI_write_cmos_sensor(0xcA, 0xD3);
			GC0313MIPI_write_cmos_sensor(0xcB, 0xDD);
			GC0313MIPI_write_cmos_sensor(0xcC, 0xE5);
			GC0313MIPI_write_cmos_sensor(0xcD, 0xF1);
			GC0313MIPI_write_cmos_sensor(0xcE, 0xFA);
			GC0313MIPI_write_cmos_sensor(0xcF, 0xFF);
			break;
			
		case GC0313MIPI_RGB_Gamma_m4:
			GC0313MIPI_write_cmos_sensor(0xBF, 0x0E);
			GC0313MIPI_write_cmos_sensor(0xc0, 0x1C);
			GC0313MIPI_write_cmos_sensor(0xc1, 0x34);
			GC0313MIPI_write_cmos_sensor(0xc2, 0x48);
			GC0313MIPI_write_cmos_sensor(0xc3, 0x5A);
			GC0313MIPI_write_cmos_sensor(0xc4, 0x6B);
			GC0313MIPI_write_cmos_sensor(0xc5, 0x7B);
			GC0313MIPI_write_cmos_sensor(0xc6, 0x95);
			GC0313MIPI_write_cmos_sensor(0xc7, 0xAB);
			GC0313MIPI_write_cmos_sensor(0xc8, 0xBF);
			GC0313MIPI_write_cmos_sensor(0xc9, 0xCE);
			GC0313MIPI_write_cmos_sensor(0xcA, 0xD9);
			GC0313MIPI_write_cmos_sensor(0xcB, 0xE4);
			GC0313MIPI_write_cmos_sensor(0xcC, 0xEC);
			GC0313MIPI_write_cmos_sensor(0xcD, 0xF7);
			GC0313MIPI_write_cmos_sensor(0xcE, 0xFD);
			GC0313MIPI_write_cmos_sensor(0xcF, 0xFF);
			break;
			
		case GC0313MIPI_RGB_Gamma_m5:
			GC0313MIPI_write_cmos_sensor(0xBF, 0x10);
			GC0313MIPI_write_cmos_sensor(0xc0, 0x20);
			GC0313MIPI_write_cmos_sensor(0xc1, 0x38);
			GC0313MIPI_write_cmos_sensor(0xc2, 0x4E);
			GC0313MIPI_write_cmos_sensor(0xc3, 0x63);
			GC0313MIPI_write_cmos_sensor(0xc4, 0x76);
			GC0313MIPI_write_cmos_sensor(0xc5, 0x87);
			GC0313MIPI_write_cmos_sensor(0xc6, 0xA2);
			GC0313MIPI_write_cmos_sensor(0xc7, 0xB8);
			GC0313MIPI_write_cmos_sensor(0xc8, 0xCA);
			GC0313MIPI_write_cmos_sensor(0xc9, 0xD8);
			GC0313MIPI_write_cmos_sensor(0xcA, 0xE3);
			GC0313MIPI_write_cmos_sensor(0xcB, 0xEB);
			GC0313MIPI_write_cmos_sensor(0xcC, 0xF0);
			GC0313MIPI_write_cmos_sensor(0xcD, 0xF8);
			GC0313MIPI_write_cmos_sensor(0xcE, 0xFD);
			GC0313MIPI_write_cmos_sensor(0xcF, 0xFF);
			break;
			
		case GC0313MIPI_RGB_Gamma_m6:										// largest gamma curve
			GC0313MIPI_write_cmos_sensor(0xBF, 0x14);
			GC0313MIPI_write_cmos_sensor(0xc0, 0x28);
			GC0313MIPI_write_cmos_sensor(0xc1, 0x44);
			GC0313MIPI_write_cmos_sensor(0xc2, 0x5D);
			GC0313MIPI_write_cmos_sensor(0xc3, 0x72);
			GC0313MIPI_write_cmos_sensor(0xc4, 0x86);
			GC0313MIPI_write_cmos_sensor(0xc5, 0x95);
			GC0313MIPI_write_cmos_sensor(0xc6, 0xB1);
			GC0313MIPI_write_cmos_sensor(0xc7, 0xC6);
			GC0313MIPI_write_cmos_sensor(0xc8, 0xD5);
			GC0313MIPI_write_cmos_sensor(0xc9, 0xE1);
			GC0313MIPI_write_cmos_sensor(0xcA, 0xEA);
			GC0313MIPI_write_cmos_sensor(0xcB, 0xF1);
			GC0313MIPI_write_cmos_sensor(0xcC, 0xF5);
			GC0313MIPI_write_cmos_sensor(0xcD, 0xFB);
			GC0313MIPI_write_cmos_sensor(0xcE, 0xFE);
			GC0313MIPI_write_cmos_sensor(0xcF, 0xFF);
			break;
		case GC0313MIPI_RGB_Gamma_night:									//Gamma for night mode
			GC0313MIPI_write_cmos_sensor(0xBF, 0x0B);
			GC0313MIPI_write_cmos_sensor(0xc0, 0x16);
			GC0313MIPI_write_cmos_sensor(0xc1, 0x29);
			GC0313MIPI_write_cmos_sensor(0xc2, 0x3C);
			GC0313MIPI_write_cmos_sensor(0xc3, 0x4F);
			GC0313MIPI_write_cmos_sensor(0xc4, 0x5F);
			GC0313MIPI_write_cmos_sensor(0xc5, 0x6F);
			GC0313MIPI_write_cmos_sensor(0xc6, 0x8A);
			GC0313MIPI_write_cmos_sensor(0xc7, 0x9F);
			GC0313MIPI_write_cmos_sensor(0xc8, 0xB4);
			GC0313MIPI_write_cmos_sensor(0xc9, 0xC6);
			GC0313MIPI_write_cmos_sensor(0xcA, 0xD3);
			GC0313MIPI_write_cmos_sensor(0xcB, 0xDD);
			GC0313MIPI_write_cmos_sensor(0xcC, 0xE5);
			GC0313MIPI_write_cmos_sensor(0xcD, 0xF1);
			GC0313MIPI_write_cmos_sensor(0xcE, 0xFA);
			GC0313MIPI_write_cmos_sensor(0xcF, 0xFF);
			break;
		default:
			//GC0313MIPI_RGB_Gamma_m1
			GC0313MIPI_write_cmos_sensor(0xfe, 0x00);
			GC0313MIPI_write_cmos_sensor(0xbf, 0x06);
			GC0313MIPI_write_cmos_sensor(0xc0, 0x12);
			GC0313MIPI_write_cmos_sensor(0xc1, 0x22);
			GC0313MIPI_write_cmos_sensor(0xc2, 0x35);
			GC0313MIPI_write_cmos_sensor(0xc3, 0x4b);
			GC0313MIPI_write_cmos_sensor(0xc4, 0x5f);
			GC0313MIPI_write_cmos_sensor(0xc5, 0x72);
			GC0313MIPI_write_cmos_sensor(0xc6, 0x8d);
			GC0313MIPI_write_cmos_sensor(0xc7, 0xa4);
			GC0313MIPI_write_cmos_sensor(0xc8, 0xb8);
			GC0313MIPI_write_cmos_sensor(0xc9, 0xc8);
			GC0313MIPI_write_cmos_sensor(0xca, 0xd4);
			GC0313MIPI_write_cmos_sensor(0xcb, 0xde);
			GC0313MIPI_write_cmos_sensor(0xcc, 0xe6);
			GC0313MIPI_write_cmos_sensor(0xcd, 0xf1);
			GC0313MIPI_write_cmos_sensor(0xce, 0xf8);
			GC0313MIPI_write_cmos_sensor(0xcf, 0xfd);
			break;
	}
}


/*************************************************************************
 * FUNCTION
 *	GC0313MIPI_config_window
 *
 * DESCRIPTION
 *	This function config the hardware window of GC0313MIPI for getting specified
 *  data of that window.
 *
 * PARAMETERS
 *	start_x : start column of the interested window
 *  start_y : start row of the interested window
 *  width  : column widht of the itnerested window
 *  height : row depth of the itnerested window
 *
 * RETURNS
 *	the data that read from GC0313MIPI
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
void GC0313MIPI_config_window(kal_uint16 startx, kal_uint16 starty, kal_uint16 width, kal_uint16 height)
{
} /* GC0313MIPI_config_window */


/*************************************************************************
 * FUNCTION
 *	GC0313MIPI_SetGain
 *
 * DESCRIPTION
 *	This function is to set global gain to sensor.
 *
 * PARAMETERS
 *   iGain : sensor global gain(base: 0x40)
 *
 * RETURNS
 *	the actually gain set to sensor.
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
kal_uint16 GC0313MIPI_SetGain(kal_uint16 iGain)
{
	return iGain;
}


/*************************************************************************
 * FUNCTION
 *	GC0313MIPI_NightMode
 *
 * DESCRIPTION
 *	This function night mode of GC0313MIPI.
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
void GC0313MIPINightMode(kal_bool bEnable)
{
    SENSORDB("bEnable = %d", bEnable);
	if (bEnable)
	{	
			GC0313MIPI_write_cmos_sensor(0xfe, 0x01);
		if(GC0313MIPI_MPEG4_encode_mode == KAL_TRUE)
			GC0313MIPI_write_cmos_sensor(0x33, 0x00);
		else
			GC0313MIPI_write_cmos_sensor(0x33, 0x30);
             		GC0313MIPI_write_cmos_sensor(0xfe, 0x00);
			GC0313MIPIGammaSelect(GC0313MIPI_RGB_Gamma_night);		
			GC0313MIPI_NIGHT_MODE = KAL_TRUE;
	}
	else 
	{
			GC0313MIPI_write_cmos_sensor(0xfe, 0x01);
		if(GC0313MIPI_MPEG4_encode_mode == KAL_TRUE)
			GC0313MIPI_write_cmos_sensor(0x33, 0x00);
		else
			GC0313MIPI_write_cmos_sensor(0x33, 0x20);
           	       GC0313MIPI_write_cmos_sensor(0xfe, 0x00);
			GC0313MIPIGammaSelect(GC0313MIPI_RGB_Gamma_m2);				   
			GC0313MIPI_NIGHT_MODE = KAL_FALSE;
	}
} /* GC0313MIPI_NightMode */

/*************************************************************************
* FUNCTION
*	GC0313MIPI_Sensor_Init
*
* DESCRIPTION
*	This function apply all of the initial setting to sensor.
*
* PARAMETERS
*	None
*
* RETURNS
*	None
*
*************************************************************************/
void GC0313MIPI_Sensor_Init(void)
{
	GC0313MIPI_write_cmos_sensor(0xfe , 0x80); 
	GC0313MIPI_write_cmos_sensor(0xfe , 0x80); 
	GC0313MIPI_write_cmos_sensor(0xfe , 0x80); 
	GC0313MIPI_write_cmos_sensor(0xf1 , 0xf0); 
	GC0313MIPI_write_cmos_sensor(0xf2 , 0x00); 
	GC0313MIPI_write_cmos_sensor(0xf6 , 0x03); 
	GC0313MIPI_write_cmos_sensor(0xf7 , 0x03); 
	GC0313MIPI_write_cmos_sensor(0xfc , 0x1e); 
	GC0313MIPI_write_cmos_sensor(0xfe , 0x00); 
	GC0313MIPI_write_cmos_sensor(0x42 , 0xfd); 
	GC0313MIPI_write_cmos_sensor(0x77 , 0x6a); //6f
	GC0313MIPI_write_cmos_sensor(0x78 , 0x40); 
	GC0313MIPI_write_cmos_sensor(0x79 , 0x60); //54
	GC0313MIPI_write_cmos_sensor(0x42 , 0xff); 

	/////////////////////////////////////////////////////
	////////////////// Window Setting ///////////////////
	/////////////////////////////////////////////////////
	GC0313MIPI_write_cmos_sensor(0x0d , 0x01);
	GC0313MIPI_write_cmos_sensor(0x0e , 0xe8);
	GC0313MIPI_write_cmos_sensor(0x0f , 0x02);
	GC0313MIPI_write_cmos_sensor(0x10 , 0x88);
	GC0313MIPI_write_cmos_sensor(0x05 , 0x03);
	GC0313MIPI_write_cmos_sensor(0x06 , 0xa3);
	GC0313MIPI_write_cmos_sensor(0x07 , 0x00);
	GC0313MIPI_write_cmos_sensor(0x08 , 0x48);
	GC0313MIPI_write_cmos_sensor(0x09 , 0x00);
	GC0313MIPI_write_cmos_sensor(0x0a , 0x00);
	GC0313MIPI_write_cmos_sensor(0x0b , 0x00);
	GC0313MIPI_write_cmos_sensor(0x0c , 0x04); 

	/////////////////////////////////////////////////////
	////////////////// Analog & CISCTL //////////////////
	/////////////////////////////////////////////////////
	GC0313MIPI_write_cmos_sensor(0x17 , 0x14); 
	GC0313MIPI_write_cmos_sensor(0x19 , 0x04); 
	GC0313MIPI_write_cmos_sensor(0x1b , 0x48); 
	GC0313MIPI_write_cmos_sensor(0x1f , 0x08); 
	GC0313MIPI_write_cmos_sensor(0x20 , 0x01); 
	GC0313MIPI_write_cmos_sensor(0x21 , 0x48); 
	GC0313MIPI_write_cmos_sensor(0x22 , 0x9a); 
	GC0313MIPI_write_cmos_sensor(0x23 , 0x07); 
	GC0313MIPI_write_cmos_sensor(0x24 , 0x16); 

	/////////////////////////////////////////////////////
	/////////////////// ISP Realated ////////////////////
	/////////////////////////////////////////////////////
	GC0313MIPI_write_cmos_sensor(0x40 , 0xdf);
	GC0313MIPI_write_cmos_sensor(0x41 , 0x60);
	GC0313MIPI_write_cmos_sensor(0x42 , 0x7f);
	GC0313MIPI_write_cmos_sensor(0x44 , 0x22);
	GC0313MIPI_write_cmos_sensor(0x45 , 0x00);
	GC0313MIPI_write_cmos_sensor(0x46 , 0x02);
	GC0313MIPI_write_cmos_sensor(0x4d , 0x01);
	GC0313MIPI_write_cmos_sensor(0x4f , 0x01);
	GC0313MIPI_write_cmos_sensor(0x50 , 0x01);
	GC0313MIPI_write_cmos_sensor(0x70 , 0x70);

	/////////////////////////////////////////////////////
	/////////////////////// BLK /////////////////////////
	/////////////////////////////////////////////////////
	GC0313MIPI_write_cmos_sensor(0x26 , 0xf7);
	GC0313MIPI_write_cmos_sensor(0x27 , 0x01);
	GC0313MIPI_write_cmos_sensor(0x28 , 0x7f);
	GC0313MIPI_write_cmos_sensor(0x29 , 0x38);
	GC0313MIPI_write_cmos_sensor(0x33 , 0x1a);
	GC0313MIPI_write_cmos_sensor(0x34 , 0x1a);
	GC0313MIPI_write_cmos_sensor(0x35 , 0x1a);
	GC0313MIPI_write_cmos_sensor(0x36 , 0x1a);
	GC0313MIPI_write_cmos_sensor(0x37 , 0x1a);
	GC0313MIPI_write_cmos_sensor(0x38 , 0x1a);
	GC0313MIPI_write_cmos_sensor(0x39 , 0x1a);
	GC0313MIPI_write_cmos_sensor(0x3a , 0x1a);
	////////////////////////////////////////////////////
	//////////////////// Y Gamma ///////////////////////
	////////////////////////////////////////////////////
	GC0313MIPI_write_cmos_sensor(0xfe , 0x00);
	GC0313MIPI_write_cmos_sensor(0x63 , 0x00);
	GC0313MIPI_write_cmos_sensor(0x64 , 0x06);
	GC0313MIPI_write_cmos_sensor(0x65 , 0x0f);
	GC0313MIPI_write_cmos_sensor(0x66 , 0x21);
	GC0313MIPI_write_cmos_sensor(0x67 , 0x34);
	GC0313MIPI_write_cmos_sensor(0x68 , 0x47);
	GC0313MIPI_write_cmos_sensor(0x69 , 0x59);
	GC0313MIPI_write_cmos_sensor(0x6a , 0x6c);
	GC0313MIPI_write_cmos_sensor(0x6b , 0x8e);
	GC0313MIPI_write_cmos_sensor(0x6c , 0xab);
	GC0313MIPI_write_cmos_sensor(0x6d , 0xc5);
	GC0313MIPI_write_cmos_sensor(0x6e , 0xe0);
	GC0313MIPI_write_cmos_sensor(0x6f , 0xfa);

	////////////////////////////////////////////////////
	////////////////// YUV to RGB //////////////////////
	////////////////////////////////////////////////////
	GC0313MIPI_write_cmos_sensor(0xb0 , 0x13);
	GC0313MIPI_write_cmos_sensor(0xb1 , 0x27);
	GC0313MIPI_write_cmos_sensor(0xb2 , 0x07);
	GC0313MIPI_write_cmos_sensor(0xb3 , 0xf5);
	GC0313MIPI_write_cmos_sensor(0xb4 , 0xe9);
	GC0313MIPI_write_cmos_sensor(0xb5 , 0x21);
	GC0313MIPI_write_cmos_sensor(0xb6 , 0x21);
	GC0313MIPI_write_cmos_sensor(0xb7 , 0xe3);
	GC0313MIPI_write_cmos_sensor(0xb8 , 0xfb);

	////////////////////////////////////////////////////
	/////////////////////// DNDD ///////////////////////
	////////////////////////////////////////////////////
	GC0313MIPI_write_cmos_sensor(0x7e , 0x14);
	GC0313MIPI_write_cmos_sensor(0x7f , 0xc1);
	GC0313MIPI_write_cmos_sensor(0x82 , 0x7f);
	GC0313MIPI_write_cmos_sensor(0x84 , 0x02);
	GC0313MIPI_write_cmos_sensor(0x89 , 0xe4);

	////////////////////////////////////////////////////
	////////////////////// INTPEE //////////////////////
	////////////////////////////////////////////////////
	GC0313MIPI_write_cmos_sensor(0x90 , 0xbc); 
	GC0313MIPI_write_cmos_sensor(0x92 , 0x08);
	GC0313MIPI_write_cmos_sensor(0x94 , 0x08);
	GC0313MIPI_write_cmos_sensor(0x95 , 0x56); 

	////////////////////////////////////////////////////
	/////////////////////// ASDE ///////////////////////
	////////////////////////////////////////////////////
	GC0313MIPI_write_cmos_sensor(0xfe , 0x01);
	GC0313MIPI_write_cmos_sensor(0x18 , 0x01);
	GC0313MIPI_write_cmos_sensor(0xfe , 0x00);
	GC0313MIPI_write_cmos_sensor(0x9a , 0x20);
	GC0313MIPI_write_cmos_sensor(0x9c , 0x98);
	GC0313MIPI_write_cmos_sensor(0x9e , 0x08);
	GC0313MIPI_write_cmos_sensor(0xa2 , 0x32);
	GC0313MIPI_write_cmos_sensor(0xa4 , 0x40);
	GC0313MIPI_write_cmos_sensor(0xaa , 0x60);

	////////////////////////////////////////////////////
	//////////////////// RGB Gamma /////////////////////
	////////////////////////////////////////////////////
	GC0313MIPI_write_cmos_sensor(0xbf , 0x0b); 
	GC0313MIPI_write_cmos_sensor(0xc0 , 0x16); 
	GC0313MIPI_write_cmos_sensor(0xc1 , 0x29); 
	GC0313MIPI_write_cmos_sensor(0xc2 , 0x3c); 
	GC0313MIPI_write_cmos_sensor(0xc3 , 0x4f); 
	GC0313MIPI_write_cmos_sensor(0xc4 , 0x5f); 
	GC0313MIPI_write_cmos_sensor(0xc5 , 0x6f); 
	GC0313MIPI_write_cmos_sensor(0xc6 , 0x8a); 
	GC0313MIPI_write_cmos_sensor(0xc7 , 0x9f); 
	GC0313MIPI_write_cmos_sensor(0xc8 , 0xb4); 
	GC0313MIPI_write_cmos_sensor(0xc9 , 0xc6); 
	GC0313MIPI_write_cmos_sensor(0xca , 0xd3); 
	GC0313MIPI_write_cmos_sensor(0xcb , 0xdd); 
	GC0313MIPI_write_cmos_sensor(0xcc , 0xe5); 
	GC0313MIPI_write_cmos_sensor(0xcd , 0xf1); 
	GC0313MIPI_write_cmos_sensor(0xce , 0xfa); 
	GC0313MIPI_write_cmos_sensor(0xcf , 0xff); 

	////////////////////////////////////////////////////
	/////////////////////// AEC ////////////////////////
	////////////////////////////////////////////////////
	GC0313MIPI_write_cmos_sensor(0xfe , 0x01); 
	GC0313MIPI_write_cmos_sensor(0x10 , 0x00);
	GC0313MIPI_write_cmos_sensor(0x11 , 0x11);
	GC0313MIPI_write_cmos_sensor(0x12 , 0x10);
	GC0313MIPI_write_cmos_sensor(0x13 , 0x8c);
	GC0313MIPI_write_cmos_sensor(0x16 , 0x18);
	GC0313MIPI_write_cmos_sensor(0x17 , 0x88);
	GC0313MIPI_write_cmos_sensor(0x21 , 0xf0);
	GC0313MIPI_write_cmos_sensor(0x22 , 0x80);
	GC0313MIPI_write_cmos_sensor(0x29 , 0x00);
	GC0313MIPI_write_cmos_sensor(0x2a , 0x50);
	GC0313MIPI_write_cmos_sensor(0x2b , 0x02);
	GC0313MIPI_write_cmos_sensor(0x2c , 0x30);
	GC0313MIPI_write_cmos_sensor(0x2d , 0x02);
	GC0313MIPI_write_cmos_sensor(0x2e , 0xd0);
	GC0313MIPI_write_cmos_sensor(0x2f , 0x03);
	GC0313MIPI_write_cmos_sensor(0x30 , 0xc0);
	GC0313MIPI_write_cmos_sensor(0x31 , 0x06);
	GC0313MIPI_write_cmos_sensor(0x32 , 0x40);
	GC0313MIPI_write_cmos_sensor(0x33 , 0x30);
	GC0313MIPI_write_cmos_sensor(0x3c , 0x60);
	GC0313MIPI_write_cmos_sensor(0x3e , 0x40);

	////////////////////////////////////////////////////
	/////////////////////// YCP ////////////////////////
	////////////////////////////////////////////////////
	GC0313MIPI_write_cmos_sensor(0xfe , 0x00);
	GC0313MIPI_write_cmos_sensor(0xd0 , 0x40);
	GC0313MIPI_write_cmos_sensor(0xd1 , 0x34);
	GC0313MIPI_write_cmos_sensor(0xd2 , 0x34);
	GC0313MIPI_write_cmos_sensor(0xd3 , 0x3c);
	GC0313MIPI_write_cmos_sensor(0xde , 0x38);
	GC0313MIPI_write_cmos_sensor(0xd6 , 0xed);
	GC0313MIPI_write_cmos_sensor(0xd7 , 0x19);
	GC0313MIPI_write_cmos_sensor(0xd8 , 0x16);
	GC0313MIPI_write_cmos_sensor(0xdd , 0x00);
	GC0313MIPI_write_cmos_sensor(0xe4 , 0x8f);
	GC0313MIPI_write_cmos_sensor(0xe5 , 0x50);

	////////////////////////////////////////////////////
	//////////////////// DARK & RC /////////////////////
	////////////////////////////////////////////////////
	GC0313MIPI_write_cmos_sensor(0xfe , 0x01);
	GC0313MIPI_write_cmos_sensor(0x40 , 0x8f);
	GC0313MIPI_write_cmos_sensor(0x41 , 0x83);
	GC0313MIPI_write_cmos_sensor(0x42 , 0xff);
	GC0313MIPI_write_cmos_sensor(0x43 , 0x06);
	GC0313MIPI_write_cmos_sensor(0x44 , 0x1f);
	GC0313MIPI_write_cmos_sensor(0x45 , 0xff);
	GC0313MIPI_write_cmos_sensor(0x46 , 0xff);
	GC0313MIPI_write_cmos_sensor(0x47 , 0x04);

	////////////////////////////////////////////////////
	////////////////////// AWB /////////////////////////
	//////////////////////////////////////////////////// 
	GC0313MIPI_write_cmos_sensor(0x06 , 0x0d);
	GC0313MIPI_write_cmos_sensor(0x07 , 0x06); 
	GC0313MIPI_write_cmos_sensor(0x08 , 0xa4);
	GC0313MIPI_write_cmos_sensor(0x09 , 0xf2); 
	GC0313MIPI_write_cmos_sensor(0x50 , 0xfd); 
	GC0313MIPI_write_cmos_sensor(0x51 , 0x20); 
	GC0313MIPI_write_cmos_sensor(0x52 , 0x20); 
	GC0313MIPI_write_cmos_sensor(0x53 , 0x08); 
	GC0313MIPI_write_cmos_sensor(0x54 , 0x08); //10
	GC0313MIPI_write_cmos_sensor(0x55 , 0x18); //20
	GC0313MIPI_write_cmos_sensor(0x56 , 0x1b); 
	GC0313MIPI_write_cmos_sensor(0x57 , 0x20); 
	GC0313MIPI_write_cmos_sensor(0x58 , 0xfd); 
	GC0313MIPI_write_cmos_sensor(0x59 , 0x08); 
	GC0313MIPI_write_cmos_sensor(0x5a , 0x11); 
	GC0313MIPI_write_cmos_sensor(0x5b , 0xf0); 
	GC0313MIPI_write_cmos_sensor(0x5c , 0xe8); 
	GC0313MIPI_write_cmos_sensor(0x5d , 0x10); 
	GC0313MIPI_write_cmos_sensor(0x5e , 0x20); 
	GC0313MIPI_write_cmos_sensor(0x5f , 0xe0); 
	GC0313MIPI_write_cmos_sensor(0x60 , 0x00); 
	GC0313MIPI_write_cmos_sensor(0x67 , 0x03); 
	GC0313MIPI_write_cmos_sensor(0x69 , 0xb0);
	GC0313MIPI_write_cmos_sensor(0x6d , 0x32); 
	GC0313MIPI_write_cmos_sensor(0x6e , 0x08); 
	GC0313MIPI_write_cmos_sensor(0x6f , 0x08); 
	GC0313MIPI_write_cmos_sensor(0x70 , 0x40); 
	GC0313MIPI_write_cmos_sensor(0x71 , 0x82); 
	GC0313MIPI_write_cmos_sensor(0x72 , 0x25); 
	GC0313MIPI_write_cmos_sensor(0x73 , 0x62); 
	GC0313MIPI_write_cmos_sensor(0x74 , 0x10); //1b                                      
	GC0313MIPI_write_cmos_sensor(0x75 , 0x48); 
	GC0313MIPI_write_cmos_sensor(0x76 , 0x40); 
	GC0313MIPI_write_cmos_sensor(0x77 , 0xc2); 
	GC0313MIPI_write_cmos_sensor(0x78 , 0xa5); 
	GC0313MIPI_write_cmos_sensor(0x79 , 0x18); 
	GC0313MIPI_write_cmos_sensor(0x7a , 0x40); 
	GC0313MIPI_write_cmos_sensor(0x7b , 0xb0); 
	GC0313MIPI_write_cmos_sensor(0x7c , 0xf5); 
	GC0313MIPI_write_cmos_sensor(0x81 , 0x80); 
	GC0313MIPI_write_cmos_sensor(0x82 , 0x60); 
	GC0313MIPI_write_cmos_sensor(0x83 , 0xd0); //b0
	GC0313MIPI_write_cmos_sensor(0x84 , 0x80); 
	GC0313MIPI_write_cmos_sensor(0x85 , 0x58); 
	GC0313MIPI_write_cmos_sensor(0x86 , 0x4a); 
	GC0313MIPI_write_cmos_sensor(0x92 , 0x00); 
	GC0313MIPI_write_cmos_sensor(0xd5 , 0x0C); 
	GC0313MIPI_write_cmos_sensor(0xd6 , 0x02); 
	GC0313MIPI_write_cmos_sensor(0xd7 , 0x06); 
	GC0313MIPI_write_cmos_sensor(0xd8 , 0x05); 
	GC0313MIPI_write_cmos_sensor(0xdd , 0x00); 

	////////////////////////////////////////////////////
	////////////////////// LSC /////////////////////////
	////////////////////////////////////////////////////
	GC0313MIPI_write_cmos_sensor(0xfe , 0x01); 
	GC0313MIPI_write_cmos_sensor(0xa0 , 0x00); 
	GC0313MIPI_write_cmos_sensor(0xa1 , 0x3c); 
	GC0313MIPI_write_cmos_sensor(0xa2 , 0x50); 
	GC0313MIPI_write_cmos_sensor(0xa3 , 0x00); 
	GC0313MIPI_write_cmos_sensor(0xa4 , 0x00); 
	GC0313MIPI_write_cmos_sensor(0xa5 , 0x00); 
	GC0313MIPI_write_cmos_sensor(0xa6 , 0x00); 
	GC0313MIPI_write_cmos_sensor(0xa7 , 0x00); 
	GC0313MIPI_write_cmos_sensor(0xa8 , 0x12); 
	GC0313MIPI_write_cmos_sensor(0xa9 , 0x0b); 
	GC0313MIPI_write_cmos_sensor(0xaa , 0x0c); 
	GC0313MIPI_write_cmos_sensor(0xab , 0x0c); 
	GC0313MIPI_write_cmos_sensor(0xac , 0x04); 
	GC0313MIPI_write_cmos_sensor(0xad , 0x00); 
	GC0313MIPI_write_cmos_sensor(0xae , 0x0a); 
	GC0313MIPI_write_cmos_sensor(0xaf , 0x04); 
	GC0313MIPI_write_cmos_sensor(0xb0 , 0x00); 
	GC0313MIPI_write_cmos_sensor(0xb1 , 0x06); 
	GC0313MIPI_write_cmos_sensor(0xb2 , 0x00); 
	GC0313MIPI_write_cmos_sensor(0xb3 , 0x00); 
	GC0313MIPI_write_cmos_sensor(0xb4 , 0x3c); 
	GC0313MIPI_write_cmos_sensor(0xb5 , 0x40); 
	GC0313MIPI_write_cmos_sensor(0xb6 , 0x3a); 
	GC0313MIPI_write_cmos_sensor(0xba , 0x2d); 
	GC0313MIPI_write_cmos_sensor(0xbb , 0x1e); 
	GC0313MIPI_write_cmos_sensor(0xbc , 0x1c); 
	GC0313MIPI_write_cmos_sensor(0xc0 , 0x1a); 
	GC0313MIPI_write_cmos_sensor(0xc1 , 0x17); 
	GC0313MIPI_write_cmos_sensor(0xc2 , 0x18); 
	GC0313MIPI_write_cmos_sensor(0xc6 , 0x0b); 
	GC0313MIPI_write_cmos_sensor(0xc7 , 0x09); 
	GC0313MIPI_write_cmos_sensor(0xc8 , 0x09); 
	GC0313MIPI_write_cmos_sensor(0xb7 , 0x35); 
	GC0313MIPI_write_cmos_sensor(0xb8 , 0x20); 
	GC0313MIPI_write_cmos_sensor(0xb9 , 0x20); 
	GC0313MIPI_write_cmos_sensor(0xbd , 0x20); 
	GC0313MIPI_write_cmos_sensor(0xbe , 0x20); 
	GC0313MIPI_write_cmos_sensor(0xbf , 0x20); 
	GC0313MIPI_write_cmos_sensor(0xc3 , 0x00); 
	GC0313MIPI_write_cmos_sensor(0xc4 , 0x00); 
	GC0313MIPI_write_cmos_sensor(0xc5 , 0x00); 
	GC0313MIPI_write_cmos_sensor(0xc9 , 0x00); 
	GC0313MIPI_write_cmos_sensor(0xca , 0x00); 
	GC0313MIPI_write_cmos_sensor(0xcb , 0x00); 

	//////////////////////////////////////////////////
	////////////////////// MIPI //////////////////////
	//////////////////////////////////////////////////
	GC0313MIPI_write_cmos_sensor(0xfe , 0x03);
	GC0313MIPI_write_cmos_sensor(0x01 , 0x03);
	GC0313MIPI_write_cmos_sensor(0x02 , 0x21);
	GC0313MIPI_write_cmos_sensor(0x03 , 0x10);
	GC0313MIPI_write_cmos_sensor(0x04 , 0x80);
	GC0313MIPI_write_cmos_sensor(0x05 , 0x02);
	GC0313MIPI_write_cmos_sensor(0x06 , 0x80);
	GC0313MIPI_write_cmos_sensor(0x11 , 0x1e);
	GC0313MIPI_write_cmos_sensor(0x12 , 0x00);
	GC0313MIPI_write_cmos_sensor(0x13 , 0x05);
	GC0313MIPI_write_cmos_sensor(0x15 , 0x10);
	GC0313MIPI_write_cmos_sensor(0x17 , 0x00);
	GC0313MIPI_write_cmos_sensor(0x21 , 0x01);
	GC0313MIPI_write_cmos_sensor(0x22 , 0x02);
	GC0313MIPI_write_cmos_sensor(0x23 , 0x01);
	GC0313MIPI_write_cmos_sensor(0x29 , 0x02);
	GC0313MIPI_write_cmos_sensor(0x2a , 0x01);
	GC0313MIPI_write_cmos_sensor(0x10 , 0x94);

	GC0313MIPI_write_cmos_sensor(0xfe , 0x00);


}


UINT32 GC0313MIPIGetSensorID(UINT32 *sensorID)
{
    kal_uint16 sensor_id=0;
    int i;

    Sleep(20);

    do
    {
        	// check if sensor ID correct
        	for(i = 0; i < 3; i++)
		{
	            	sensor_id = GC0313MIPI_read_cmos_sensor(0xf0);
	            	printk("GC0313MIPI Sensor id = %x\n", sensor_id);
	            	if (sensor_id == GC0313MIPI_YUV_SENSOR_ID)             
			{
	               	break;
	            	}
        	}
        	mdelay(50);
    }while(0);

    if(sensor_id != GC0313MIPI_YUV_SENSOR_ID)
    {
        SENSORDB("GC0313MIPI Sensor id read failed, ID = %x\n", sensor_id);
        return ERROR_SENSOR_CONNECT_FAIL;
    }

    *sensorID = sensor_id;

    RETAILMSG(1, (TEXT("Sensor Read ID OK \r\n")));
	
    return ERROR_NONE;
}




/*************************************************************************
* FUNCTION
*	GC0313MIPI_Write_More_Registers
*
* DESCRIPTION
*	This function is served for FAE to modify the necessary Init Regs. Do not modify the regs
*     in init_GC0313MIPI() directly.
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
void GC0313MIPI_Write_More_Registers(void)
{
    	GC0313MIPIGammaSelect(2);//0:use default
}


/*************************************************************************
 * FUNCTION
 *	GC0313MIPIOpen
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
UINT32 GC0313MIPIOpen(void)
{
    kal_uint16 sensor_id=0;
    int i;


    do
    {
        	// check if sensor ID correct
        	for(i = 0; i < 3; i++)
		{
	            	sensor_id = GC0313MIPI_read_cmos_sensor(0xf0);
	            	printk("GC0313MIPI Sensor id = %x\n", sensor_id);
	            	if (sensor_id == GC0313MIPI_YUV_SENSOR_ID)
			{
	               	break;
	            	}
        	}
        	mdelay(50);
    }while(0);

    if(sensor_id != GC0313MIPI_YUV_SENSOR_ID)
    {
        SENSORDB("GC0313MIPI Sensor id read failed, ID = %x\n", sensor_id);
        return ERROR_SENSOR_CONNECT_FAIL;
    }
	
    GC0313MIPI_MPEG4_encode_mode = KAL_FALSE;
    RETAILMSG(1, (TEXT("Sensor Read ID OK \r\n")));
    // initail sequence write in
    GC0313MIPI_Sensor_Init();
    GC0313MIPI_Write_More_Registers();
	
    return ERROR_NONE;
} /* GC0313MIPIOpen */


/*************************************************************************
 * FUNCTION
 *	GC0313MIPIClose
 *
 * DESCRIPTION
 *	This function is to turn off sensor module power.
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
UINT32 GC0313MIPIClose(void)
{
    return ERROR_NONE;
} /* GC0313MIPIClose */


/*************************************************************************
 * FUNCTION
 * GC0313MIPIPreview
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
UINT32 GC0313MIPIPreview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
        MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)

{
    SENSORDB("Start");
    //kal_uint32 iTemp;
    //kal_uint16 iStartX = 0, iStartY = 1;

    if(sensor_config_data->SensorOperationMode == MSDK_SENSOR_OPERATION_MODE_VIDEO)		// MPEG4 Encode Mode
    {
        RETAILMSG(1, (TEXT("Camera Video preview\r\n")));
        GC0313MIPI_MPEG4_encode_mode = KAL_TRUE;
       
    }
    else
    {
        RETAILMSG(1, (TEXT("Camera preview\r\n")));
        GC0313MIPI_MPEG4_encode_mode = KAL_FALSE;
    }

    image_window->GrabStartX= IMAGE_SENSOR_VGA_GRAB_PIXELS;
    image_window->GrabStartY= IMAGE_SENSOR_VGA_GRAB_LINES;
    image_window->ExposureWindowWidth = IMAGE_SENSOR_PV_WIDTH;
    image_window->ExposureWindowHeight =IMAGE_SENSOR_PV_HEIGHT;

    // copy sensor_config_data
    memcpy(&GC0313MIPISensorConfigData, sensor_config_data, sizeof(MSDK_SENSOR_CONFIG_STRUCT));
    SENSORDB("End");
    return ERROR_NONE;
} /* GC0313MIPIPreview */


/*************************************************************************
 * FUNCTION
 *	GC0313MIPICapture
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
UINT32 GC0313MIPICapture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
        MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)

{
    SENSORDB("Start");
    GC0313MIPI_MODE_CAPTURE=KAL_TRUE;

    image_window->GrabStartX = IMAGE_SENSOR_VGA_GRAB_PIXELS;
    image_window->GrabStartY = IMAGE_SENSOR_VGA_GRAB_LINES;
    image_window->ExposureWindowWidth= IMAGE_SENSOR_FULL_WIDTH;
    image_window->ExposureWindowHeight = IMAGE_SENSOR_FULL_HEIGHT;

    // copy sensor_config_data
    memcpy(&GC0313MIPISensorConfigData, sensor_config_data, sizeof(MSDK_SENSOR_CONFIG_STRUCT));
    SENSORDB("End");
    return ERROR_NONE;
} /* GC0313MIPI_Capture() */



UINT32 GC0313MIPIGetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution)
{
    pSensorResolution->SensorFullWidth=IMAGE_SENSOR_FULL_WIDTH;
    pSensorResolution->SensorFullHeight=IMAGE_SENSOR_FULL_HEIGHT;
    pSensorResolution->SensorPreviewWidth=IMAGE_SENSOR_PV_WIDTH;
    pSensorResolution->SensorPreviewHeight=IMAGE_SENSOR_PV_HEIGHT;
    pSensorResolution->SensorVideoWidth=IMAGE_SENSOR_PV_WIDTH;
    pSensorResolution->SensorVideoHeight=IMAGE_SENSOR_PV_HEIGHT;
    return ERROR_NONE;
} /* GC0313MIPIGetResolution() */


UINT32 GC0313MIPIGetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId,
        MSDK_SENSOR_INFO_STRUCT *pSensorInfo,
        MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
    pSensorInfo->SensorPreviewResolutionX=IMAGE_SENSOR_PV_WIDTH;
    pSensorInfo->SensorPreviewResolutionY=IMAGE_SENSOR_PV_HEIGHT;
    pSensorInfo->SensorFullResolutionX=IMAGE_SENSOR_FULL_WIDTH;
    pSensorInfo->SensorFullResolutionY=IMAGE_SENSOR_FULL_HEIGHT;

    pSensorInfo->SensorCameraPreviewFrameRate=30;
    pSensorInfo->SensorVideoFrameRate=30;
    pSensorInfo->SensorStillCaptureFrameRate=10;
    pSensorInfo->SensorWebCamCaptureFrameRate=15;
    pSensorInfo->SensorResetActiveHigh=FALSE;
    pSensorInfo->SensorResetDelayCount=1;
    pSensorInfo->SensorOutputDataFormat=SENSOR_OUTPUT_FORMAT_YUYV;
    pSensorInfo->SensorClockPolarity=SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorClockFallingPolarity=SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorInterruptDelayLines = 1;
    pSensorInfo->SensroInterfaceType=SENSOR_INTERFACE_TYPE_MIPI;//MIPI setting
    pSensorInfo->CaptureDelayFrame = 2;
    pSensorInfo->PreviewDelayFrame = 6;//0
    pSensorInfo->VideoDelayFrame = 4;
    pSensorInfo->SensorMasterClockSwitch = 0;
    pSensorInfo->SensorDrivingCurrent = ISP_DRIVING_2MA;

    switch (ScenarioId)
    {
    case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
    case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
    case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
    default:
        pSensorInfo->SensorClockFreq=26;
        pSensorInfo->SensorClockDividCount= 3;
        pSensorInfo->SensorClockRisingCount=0;
        pSensorInfo->SensorClockFallingCount=2;
        pSensorInfo->SensorPixelClockCount=3;
        pSensorInfo->SensorDataLatchCount=2;
        pSensorInfo->SensorGrabStartX = IMAGE_SENSOR_VGA_GRAB_PIXELS;
        pSensorInfo->SensorGrabStartY = IMAGE_SENSOR_VGA_GRAB_LINES;
		//MIPI setting
		pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_1_LANE; 	
		pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0; 
		pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 14;
		pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
		pSensorInfo->SensorWidthSampling = 0;  // 0 is default 1x
		pSensorInfo->SensorHightSampling = 0;	// 0 is default 1x 
		pSensorInfo->SensorPacketECCOrder = 1;

        break;
    }
    GC0313MIPIPixelClockDivider=pSensorInfo->SensorPixelClockCount;
    memcpy(pSensorConfigData, &GC0313MIPISensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));
    return ERROR_NONE;
} /* GC0313MIPIGetInfo() */


UINT32 GC0313MIPIControl(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow,
        MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
    SENSORDB("Start");
    switch (ScenarioId)
    {
    case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
    case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
    case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
       // GC0313MIPICapture(pImageWindow, pSensorConfigData);
	 GC0313MIPIPreview(pImageWindow, pSensorConfigData);
        break;
        default:
            SENSORDB("Error ScenarioId setting");
            return ERROR_INVALID_SCENARIO_ID;        
    }

    SENSORDB("End");

    return TRUE;
}	/* GC0313MIPIControl() */

BOOL GC0313MIPI_set_param_wb(UINT16 para)
{
    SENSORDB("Para = %d", para);

	switch (para)
	{
		case AWB_MODE_OFF:

		break;
		
		case AWB_MODE_AUTO:
			GC0313MIPI_awb_enable(KAL_TRUE);
		break;
		
		case AWB_MODE_CLOUDY_DAYLIGHT: //cloudy
			GC0313MIPI_awb_enable(KAL_FALSE);
			GC0313MIPI_write_cmos_sensor(0x77, 0x8c); //WB_manual_gain 
			GC0313MIPI_write_cmos_sensor(0x78, 0x50);
			GC0313MIPI_write_cmos_sensor(0x79, 0x40);
		break;
		
		case AWB_MODE_DAYLIGHT: //sunny
			GC0313MIPI_awb_enable(KAL_FALSE);
			GC0313MIPI_write_cmos_sensor(0x77, 0x74); 
			GC0313MIPI_write_cmos_sensor(0x78, 0x52);
			GC0313MIPI_write_cmos_sensor(0x79, 0x40);			
		break;
		
		case AWB_MODE_INCANDESCENT: //office
			GC0313MIPI_awb_enable(KAL_FALSE);
			GC0313MIPI_write_cmos_sensor(0x77, 0x48);
			GC0313MIPI_write_cmos_sensor(0x78, 0x40);
			GC0313MIPI_write_cmos_sensor(0x79, 0x5c);
		break;
		
		case AWB_MODE_TUNGSTEN: //home
			GC0313MIPI_awb_enable(KAL_FALSE);
			GC0313MIPI_write_cmos_sensor(0x77, 0x40);
			GC0313MIPI_write_cmos_sensor(0x78, 0x54);
			GC0313MIPI_write_cmos_sensor(0x79, 0x70);
		break;
		
		case AWB_MODE_FLUORESCENT:
			GC0313MIPI_awb_enable(KAL_FALSE);
			GC0313MIPI_write_cmos_sensor(0x77, 0x40);
			GC0313MIPI_write_cmos_sensor(0x78, 0x42);
			GC0313MIPI_write_cmos_sensor(0x79, 0x50);
		break;
		
		default:
		return FALSE;
	}

	return TRUE;
} /* GC0313MIPI_set_param_wb */


BOOL GC0313MIPI_set_param_effect(UINT16 para)
{
	kal_uint32  ret = KAL_TRUE;
	
	SENSORDB("Para = %d", para);

	switch (para)
	{
		case MEFFECT_OFF:
			GC0313MIPI_write_cmos_sensor(0x43 , 0x00);
		break;
		
		case MEFFECT_SEPIA:
			GC0313MIPI_write_cmos_sensor(0x43 , 0x02);
			GC0313MIPI_write_cmos_sensor(0xda , 0xd0);
			GC0313MIPI_write_cmos_sensor(0xdb , 0x28);
		break;
		
		case MEFFECT_NEGATIVE:
			GC0313MIPI_write_cmos_sensor(0x43 , 0x01);
		break;
		
		case MEFFECT_SEPIAGREEN:
			GC0313MIPI_write_cmos_sensor(0x43 , 0x02);
			GC0313MIPI_write_cmos_sensor(0xda , 0xc0);
			GC0313MIPI_write_cmos_sensor(0xdb , 0xc0);
		break;
		
		case MEFFECT_SEPIABLUE:
			GC0313MIPI_write_cmos_sensor(0x43 , 0x02);
			GC0313MIPI_write_cmos_sensor(0xda , 0x38);//50 hyw
			GC0313MIPI_write_cmos_sensor(0xdb , 0xa8);//e0
		break;

		case MEFFECT_MONO:
			GC0313MIPI_write_cmos_sensor(0x43 , 0x02);
			GC0313MIPI_write_cmos_sensor(0xda , 0x00);
			GC0313MIPI_write_cmos_sensor(0xdb , 0x00);
		break;
		default:
			ret = FALSE;
	}

	return ret;

} /* GC0313MIPI_set_param_effect */


BOOL GC0313MIPI_set_param_banding(UINT16 para)
{
    SENSORDB("Para = %d", para);
	switch (para)
	{
		case AE_FLICKER_MODE_50HZ:
			GC0313MIPI_write_cmos_sensor(0x05 , 0x03); //HB
			GC0313MIPI_write_cmos_sensor(0x06 , 0xa3); 
			GC0313MIPI_write_cmos_sensor(0x07 , 0x00); //VB
			GC0313MIPI_write_cmos_sensor(0x08 , 0x48); 
			
			GC0313MIPI_SET_PAGE1;
			GC0313MIPI_write_cmos_sensor(0x29, 0x00);
			GC0313MIPI_write_cmos_sensor(0x2a , 0x50); //step
			GC0313MIPI_write_cmos_sensor(0x2b , 0x02); //14fps
			GC0313MIPI_write_cmos_sensor(0x2c , 0x30);
			GC0313MIPI_write_cmos_sensor(0x2d , 0x02); //11fps
			GC0313MIPI_write_cmos_sensor(0x2e , 0xd0);
			GC0313MIPI_write_cmos_sensor(0x2f , 0x03); //8fps
			GC0313MIPI_write_cmos_sensor(0x30 , 0xc0);
			GC0313MIPI_write_cmos_sensor(0x31 , 0x07); //4fps
			GC0313MIPI_write_cmos_sensor(0x32 , 0xd0); 
			GC0313MIPI_SET_PAGE0;
			break;

		case AE_FLICKER_MODE_60HZ:
			GC0313MIPI_write_cmos_sensor(0x05 , 0x03); //HB
			GC0313MIPI_write_cmos_sensor(0x06 , 0x26); 
			GC0313MIPI_write_cmos_sensor(0x07 , 0x00); //VB
			GC0313MIPI_write_cmos_sensor(0x08 , 0x48); 
			
			GC0313MIPI_SET_PAGE1;
			GC0313MIPI_write_cmos_sensor(0x29, 0x00);
			GC0313MIPI_write_cmos_sensor(0x2a , 0x60); //step
			GC0313MIPI_write_cmos_sensor(0x2b , 0x02); //14fps
			GC0313MIPI_write_cmos_sensor(0x2c , 0x30);
			GC0313MIPI_write_cmos_sensor(0x2d , 0x02); //11fps
			GC0313MIPI_write_cmos_sensor(0x2e , 0xd0);
			GC0313MIPI_write_cmos_sensor(0x2f , 0x03); //8fps
			GC0313MIPI_write_cmos_sensor(0x30 , 0xc0);
			GC0313MIPI_write_cmos_sensor(0x31 , 0x07); //4fps
			GC0313MIPI_write_cmos_sensor(0x32 , 0xd0); 
			GC0313MIPI_SET_PAGE0;
		break;
		default:
		return FALSE;
	}

	return TRUE;
} /* GC0313MIPI_set_param_banding */


BOOL GC0313MIPI_set_param_exposure(UINT16 para)
{
    SENSORDB("Para = %d", para);

	switch (para)
	{
		case AE_EV_COMP_n13:
			GC0313MIPI_write_cmos_sensor(0xfe, 0x01);
			GC0313MIPI_write_cmos_sensor(0x13, 0x30);
			GC0313MIPI_write_cmos_sensor(0xfe, 0x00);
			GC0313MIPI_write_cmos_sensor(0xd5, 0xc0);
		break;
		
		case AE_EV_COMP_n10:
			GC0313MIPI_write_cmos_sensor(0xfe, 0x01);
			GC0313MIPI_write_cmos_sensor(0x13, 0x38);
			GC0313MIPI_write_cmos_sensor(0xfe, 0x00);
			GC0313MIPI_write_cmos_sensor(0xd5, 0xd0);
		break;
		
		case AE_EV_COMP_n07:
			GC0313MIPI_write_cmos_sensor(0xfe, 0x01);
			GC0313MIPI_write_cmos_sensor(0x13, 0x40);
			GC0313MIPI_write_cmos_sensor(0xfe, 0x00);
			GC0313MIPI_write_cmos_sensor(0xd5, 0xe0);
		break;
		
		case AE_EV_COMP_n03:
			GC0313MIPI_write_cmos_sensor(0xfe, 0x01);
			GC0313MIPI_write_cmos_sensor(0x13, 0x48);
			GC0313MIPI_write_cmos_sensor(0xfe, 0x00);
			GC0313MIPI_write_cmos_sensor(0xd5, 0xf0);
		break;				
		
		case AE_EV_COMP_00:		
			GC0313MIPI_write_cmos_sensor(0xfe, 0x01);
			GC0313MIPI_write_cmos_sensor(0x13, 0x4c);
			GC0313MIPI_write_cmos_sensor(0xfe, 0x00);
			GC0313MIPI_write_cmos_sensor(0xd5, 0x00);
		break;

		case AE_EV_COMP_03:
			GC0313MIPI_write_cmos_sensor(0xfe, 0x01);
			GC0313MIPI_write_cmos_sensor(0x13, 0x58);
			GC0313MIPI_write_cmos_sensor(0xfe, 0x00);
			GC0313MIPI_write_cmos_sensor(0xd5, 0x10);
		break;
		
		case AE_EV_COMP_07:
			GC0313MIPI_write_cmos_sensor(0xfe, 0x01);
			GC0313MIPI_write_cmos_sensor(0x13, 0x60);
			GC0313MIPI_write_cmos_sensor(0xfe, 0x00);
			GC0313MIPI_write_cmos_sensor(0xd5, 0x20);
		break;
		
		case AE_EV_COMP_10:			
			GC0313MIPI_write_cmos_sensor(0xfe, 0x01);
			GC0313MIPI_write_cmos_sensor(0x13, 0x68);
			GC0313MIPI_write_cmos_sensor(0xfe, 0x00);
			GC0313MIPI_write_cmos_sensor(0xd5, 0x30);
		break;
		
		case AE_EV_COMP_13:
			GC0313MIPI_write_cmos_sensor(0xfe, 0x01);
			GC0313MIPI_write_cmos_sensor(0x13, 0x70);
			GC0313MIPI_write_cmos_sensor(0xfe, 0x00);
			GC0313MIPI_write_cmos_sensor(0xd5, 0x38);
		break;
		default:
		return FALSE;
	}

	return TRUE;
} /* GC0313MIPI_set_param_exposure */



UINT32 GC0313MIPIYUVSetVideoMode(UINT16 u2FrameRate)    // lanking add
{
    SENSORDB("u2FrameRate = %d", u2FrameRate);
  
        GC0313MIPI_MPEG4_encode_mode = KAL_TRUE;
     if (u2FrameRate == 30)
   	{
   	
   	    /*********video frame ************/
		
   	}
    else if (u2FrameRate == 15)       
    	{
    	
   	    /*********video frame ************/
		
    	}
    else
   	{
   	
            SENSORDB("Wrong Frame Rate"); 
			
   	}

      return TRUE;

}


UINT32 GC0313MIPIYUVSensorSetting(FEATURE_ID iCmd, UINT16 iPara)
{
    SENSORDB("iCmd = %d, iPara = %d", iCmd, iPara);
    switch (iCmd) {
    case FID_AWB_MODE:
        GC0313MIPI_set_param_wb(iPara);
        break;
    case FID_COLOR_EFFECT:
        GC0313MIPI_set_param_effect(iPara);
        break;
    case FID_AE_EV:
        GC0313MIPI_set_param_exposure(iPara);
        break;
    case FID_AE_FLICKER:
        GC0313MIPI_set_param_banding(iPara);
		break;
    case FID_SCENE_MODE:
	 GC0313MIPINightMode(iPara);
        break;
    default:
        break;
    }
    return TRUE;
} /* GC0313MIPIYUVSensorSetting */

UINT32 GC0313SetTestPatternMode(kal_bool bEnable)
{
	SENSORDB("test pattern bEnable:=%d\n",bEnable);
	//GC0313MIPI_write_cmos_sensor(0xfe, 0x00);	
	//GC0313MIPI_write_cmos_sensor(0x4c, 0x01);
	//GC0313MIPI_write_cmos_sensor(0xfe, 0x00);

	GC0313MIPI_write_cmos_sensor(0xfe, 0x00); // page 0
	GC0313MIPI_write_cmos_sensor(0x18, 0x06); //sdark
	GC0313MIPI_write_cmos_sensor(0x26, 0xf0);
	GC0313MIPI_write_cmos_sensor(0x2a, 0x00);
	GC0313MIPI_write_cmos_sensor(0x2b, 0x00);
	GC0313MIPI_write_cmos_sensor(0x2c, 0x00);
	GC0313MIPI_write_cmos_sensor(0x2d, 0x00);
	GC0313MIPI_write_cmos_sensor(0x2e, 0x00);
	GC0313MIPI_write_cmos_sensor(0x2f, 0x00);
	GC0313MIPI_write_cmos_sensor(0x30, 0x00);
	GC0313MIPI_write_cmos_sensor(0x31, 0x00);

	GC0313MIPI_write_cmos_sensor(0x40, 0x08); 
	GC0313MIPI_write_cmos_sensor(0x41, 0x00);
	GC0313MIPI_write_cmos_sensor(0x42, 0x00); // AWB disable
	GC0313MIPI_write_cmos_sensor(0x4f, 0x00); // AEC disable
	GC0313MIPI_write_cmos_sensor(0x03, 0x02); //exposure [11:8]
	GC0313MIPI_write_cmos_sensor(0x04, 0xe6); //exposure [7:0]
	GC0313MIPI_write_cmos_sensor(0x70, 0x40); // global gain
	GC0313MIPI_write_cmos_sensor(0x71, 0x40); // pregain
	GC0313MIPI_write_cmos_sensor(0x72, 0x40); // postgain
	GC0313MIPI_write_cmos_sensor(0x77, 0x40); // fixed R 1X gain
	GC0313MIPI_write_cmos_sensor(0x78, 0x40); // fixed G 1X gain
	GC0313MIPI_write_cmos_sensor(0x79, 0x40); // fixed B 1X gain
	GC0313MIPI_write_cmos_sensor(0xd0, 0x40); // global saturation
	GC0313MIPI_write_cmos_sensor(0xdd, 0x00);

	GC0313MIPI_write_cmos_sensor(0xfe, 0x01); //ABS
	GC0313MIPI_write_cmos_sensor(0x9e, 0xc0);
	GC0313MIPI_write_cmos_sensor(0x9f, 0x40);

	GC0313MIPI_write_cmos_sensor(0xfe, 0x00);
	GC0313MIPI_write_cmos_sensor(0x4c, 0x04); //test pattern:input test image

	if(bEnable)
	{
	}
	else
	{
	}
	return ERROR_NONE;
}

UINT32 GC0313MIPIFeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId,
        UINT8 *pFeaturePara,UINT32 *pFeatureParaLen)
{
    UINT16 *pFeatureReturnPara16=(UINT16 *) pFeaturePara;
    UINT16 *pFeatureData16=(UINT16 *) pFeaturePara;
    UINT32 *pFeatureReturnPara32=(UINT32 *) pFeaturePara;
    UINT32 *pFeatureData32=(UINT32 *) pFeaturePara;
    //UINT32 GC0313MIPISensorRegNumber;
    //UINT32 i;
    MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData=(MSDK_SENSOR_CONFIG_STRUCT *) pFeaturePara;
    MSDK_SENSOR_REG_INFO_STRUCT *pSensorRegData=(MSDK_SENSOR_REG_INFO_STRUCT *) pFeaturePara;

    RETAILMSG(1, (_T("gaiyang GC0313MIPIFeatureControl FeatureId=%d\r\n"), FeatureId));

    switch (FeatureId)
    {
    case SENSOR_FEATURE_GET_RESOLUTION:
        *pFeatureReturnPara16++=IMAGE_SENSOR_FULL_WIDTH;
        *pFeatureReturnPara16=IMAGE_SENSOR_FULL_HEIGHT;
        *pFeatureParaLen=4;
        break;
    case SENSOR_FEATURE_GET_PERIOD:
        *pFeatureReturnPara16++=(VGA_PERIOD_PIXEL_NUMS)+GC0313MIPI_dummy_pixels;
        *pFeatureReturnPara16=(VGA_PERIOD_LINE_NUMS)+GC0313MIPI_dummy_lines;
        *pFeatureParaLen=4;
        break;
    case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
        *pFeatureReturnPara32 = GC0313MIPI_g_fPV_PCLK;
        *pFeatureParaLen=4;
        break;
    case SENSOR_FEATURE_SET_ESHUTTER:
        break;
    case SENSOR_FEATURE_SET_NIGHTMODE:
        //GC0313MIPINightMode((BOOL) *pFeatureData16);
        break;
    case SENSOR_FEATURE_SET_GAIN:
    case SENSOR_FEATURE_SET_FLASHLIGHT:
        break;
    case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
        GC0313MIPI_isp_master_clock=*pFeatureData32;
        break;
    case SENSOR_FEATURE_SET_REGISTER:
        GC0313MIPI_write_cmos_sensor(pSensorRegData->RegAddr, pSensorRegData->RegData);
        break;
    case SENSOR_FEATURE_GET_REGISTER:
        pSensorRegData->RegData = GC0313MIPI_read_cmos_sensor(pSensorRegData->RegAddr);
        break;
    case SENSOR_FEATURE_GET_CONFIG_PARA:
        memcpy(pSensorConfigData, &GC0313MIPISensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));
        *pFeatureParaLen=sizeof(MSDK_SENSOR_CONFIG_STRUCT);
        break;
    case SENSOR_FEATURE_SET_CCT_REGISTER:
    case SENSOR_FEATURE_GET_CCT_REGISTER:
    case SENSOR_FEATURE_SET_ENG_REGISTER:
    case SENSOR_FEATURE_GET_ENG_REGISTER:
    case SENSOR_FEATURE_GET_REGISTER_DEFAULT:
    case SENSOR_FEATURE_CAMERA_PARA_TO_SENSOR:
    case SENSOR_FEATURE_SENSOR_TO_CAMERA_PARA:
    case SENSOR_FEATURE_GET_GROUP_COUNT:
    case SENSOR_FEATURE_GET_GROUP_INFO:
    case SENSOR_FEATURE_GET_ITEM_INFO:
    case SENSOR_FEATURE_SET_ITEM_INFO:
    case SENSOR_FEATURE_GET_ENG_INFO:
        break;
    case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
        // get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE
        // if EEPROM does not exist in camera module.
        *pFeatureReturnPara32=LENS_DRIVER_ID_DO_NOT_CARE;
        *pFeatureParaLen=4;
        break;
    case SENSOR_FEATURE_SET_YUV_CMD:
        GC0313MIPIYUVSensorSetting((FEATURE_ID)*pFeatureData32, *(pFeatureData32+1));
        break;
    case SENSOR_FEATURE_SET_VIDEO_MODE:    //  lanking
	 GC0313MIPIYUVSetVideoMode(*pFeatureData16);
	 break;
    case SENSOR_FEATURE_CHECK_SENSOR_ID:
	GC0313MIPIGetSensorID(pFeatureData32);
	break;
    case SENSOR_FEATURE_SET_TEST_PATTERN:			 
		GC0313SetTestPatternMode((BOOL)*pFeatureData16);			
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		*pFeatureReturnPara32 = GC0313_TEST_PATTERN_CHECKSUM;
		*pFeatureParaLen=4;
		break;    
    default:
        break;
	}
return ERROR_NONE;
}	/* GC0313MIPIFeatureControl() */


SENSOR_FUNCTION_STRUCT	SensorFuncGC0313MIPIYUV=
{
	GC0313MIPIOpen,
	GC0313MIPIGetInfo,
	GC0313MIPIGetResolution,
	GC0313MIPIFeatureControl,
	GC0313MIPIControl,
	GC0313MIPIClose
};


UINT32 GC0313MIPI_YUV_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc!=NULL)
		*pfFunc=&SensorFuncGC0313MIPIYUV;
	return ERROR_NONE;
} /* SensorInit() */
