/*****************************************************************************
 *
 * Filename:
 * ---------
 *   sensor.c
 *
 * Project:
 * --------

 *
 * Description:
 * ------------
 *   Source code of Sensor driver
 *
 *
 * Author:
 * -------

 *
 *============================================================================
 *             HISTORY
 * Below this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *------------------------------------------------------------------------------
 * $Revision:$
 * $Modtime:$
 * $Log:$
 *
 *
 *------------------------------------------------------------------------------
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
#include <linux/xlog.h>

#include <asm/atomic.h>
#include <asm/io.h>
#include <asm/system.h>

#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "kd_camera_feature.h"
   
#include "ov9760mipiraw_Sensor.h"
#include "ov9760mipiraw_Camera_Sensor_para.h"
#include "ov9760mipiraw_CameraCustomized.h"

#define OV9760MIPI_DEBUG
#define MIPI_INTERFACE


#ifdef OV9760MIPI_DEBUG
#define SENSORDB(fmt, arg...) xlog_printk(ANDROID_LOG_DEBUG, "[OV9760MIPI]", fmt, ##arg)

#else
#define SENSORDB(x,...)
#endif

static MSDK_SCENARIO_ID_ENUM CurrentScenarioId_9760 = MSDK_SCENARIO_ID_CAMERA_PREVIEW;

static OV9760_sensor_struct OV9760_sensor =
{
  
  .eng_info =
  {
    .SensorId = 128,
    .SensorType = CMOS_SENSOR,
    .SensorOutputDataFormat = SENSOR_OUTPUT_FORMAT_RAW_B,
  },
  .shutter = 0x20,  
  .gain = 0x20,
  .pv_pclk = OV9760MIPI_PIXEL_CLK,
  .cap_pclk = OV9760MIPI_PIXEL_CLK,
  .pclk = OV9760MIPI_PIXEL_CLK,
  .frame_height = OV9760MIPI_PERIOD_LINE_NUMS,
  .line_length = OV9760MIPI_PERIOD_PIXEL_NUMS,
  .is_zsd = KAL_FALSE, //for zsd
  .dummy_pixels = 0,
  .dummy_lines = 0,  //for zsd
  .is_autofliker = KAL_FALSE,
};


static DEFINE_SPINLOCK(ov9740mipi_raw_drv_lock);
extern int iReadRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u8 * a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u16 i2cId);


static kal_uint16 write_add=0x20;
kal_uint8 OV9760MIPI_read_cmos_sensor(kal_uint32 addr)
{
	kal_uint8 get_byte=0;
	char puSendCmd[2] = {(char)(addr >> 8) , (char)(addr & 0xFF) };
	iReadRegI2C(puSendCmd , 2, (u8*)&get_byte,1,write_add);

	return get_byte;
}

 inline void OV9760MIPI_write_cmos_sensor(u16 addr, u32 para)
{
   char puSendCmd[3] = {(char)(addr >> 8) , (char)(addr & 0xFF) ,(char)(para & 0xFF)};
   iWriteRegI2C(puSendCmd , 3,write_add);
} 
	

/*******************************************************************************
* // Adapter for Winmo typedef 
********************************************************************************/

#define Sleep(ms) mdelay(ms)
#define RETAILMSG(x,...)
/*******************************************************************************
* // End Adapter for Winmo typedef 
********************************************************************************/
/* Global Valuable */

MSDK_SENSOR_CONFIG_STRUCT OV9760MIPISensorConfigData;



/*****************************************************************************
 * FUNCTION
 *  OV9760MIPI_set_dummy
 * DESCRIPTION
 *
 * PARAMETERS
 *  pixels      [IN]
 *  lines       [IN]
 * RETURNS
 *  void
 *****************************************************************************/
UINT32 OV9760MIPI_set_dummy(const kal_uint16 iPixels, const kal_uint16 iLines)
{
	kal_uint16 line_length, frame_height;

	SENSORDB("OV9760MIPI_set_dummy:iPixels:%x; iLines:%x \n",iPixels,iLines);

	OV9760_sensor.dummy_lines = iLines;
	OV9760_sensor.dummy_pixels = iPixels;
	line_length = OV9760MIPI_PERIOD_PIXEL_NUMS + iPixels;
	frame_height = OV9760MIPI_PERIOD_LINE_NUMS + iLines;
	SENSORDB("line_length:%x; frame_height:%x \n",line_length,frame_height);


	if ((line_length >= 0x1FFF)||(frame_height >= 0xFFF))
	{
		return ERROR_NONE;
	}
	
	spin_lock(&ov9740mipi_raw_drv_lock);
	OV9760_sensor.line_length = line_length;
	OV9760_sensor.frame_height = frame_height;
	spin_unlock(&ov9740mipi_raw_drv_lock);

/*
    OV9760MIPI_write_cmos_sensor(0x380c, line_length >> 8);
    OV9760MIPI_write_cmos_sensor(0x380d, line_length & 0xFF);
    OV9760MIPI_write_cmos_sensor(0x380e, frame_height >> 8);
    OV9760MIPI_write_cmos_sensor(0x380f, frame_height & 0xFF);
*/
    OV9760MIPI_write_cmos_sensor(0x0342, line_length >> 8);
    OV9760MIPI_write_cmos_sensor(0x0343, line_length & 0xFF);
    OV9760MIPI_write_cmos_sensor(0x0340, frame_height >> 8);
    OV9760MIPI_write_cmos_sensor(0x0341, frame_height & 0xFF);
	return ERROR_NONE;
}   /* OV9760MIPI_set_dummy */



/*************************************************************************
* FUNCTION
*	OV9760MIPI_GetSensorID
*
* DESCRIPTION
*	This function get the sensor ID
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
static kal_uint32 OV9760MIPI_GetSensorID(kal_uint32 *sensorID)
{
    //volatile signed char i;
	kal_uint16 sensor_id=0;
	int i;
	const kal_uint16 sccb_writeid[] = {OV9760MIPI_WRITE_ID,OV9760MIPI_WRITE_ID_1};

	for(i = 0; i <(sizeof(sccb_writeid)/sizeof(sccb_writeid[0])); i++)
	{
		write_add=sccb_writeid[i];
		sensor_id  =((OV9760MIPI_read_cmos_sensor(0x0000)<<8) | OV9760MIPI_read_cmos_sensor(0x0001));
		
		SENSORDB("------Sensor Read OV9760MIPI ID %x \r\n",(unsigned int)sensor_id);

		if(sensor_id==OV9760MIPI_SENSOR_ID)
			break;
	}
	
	if (sensor_id != OV9760MIPI_SENSOR_ID)
	{
	    *sensorID=0xFFFFFFFF;
	    SENSORDB("Sensor Read ByeBye \r\n");
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	*sensorID=sensor_id;
    return ERROR_NONE;    
}  

void OV9760MIPI_Set720P(void)
{
 	OV9760MIPI_write_cmos_sensor(0x0103, 0x01); //;S/W reset
	Sleep(10); 									//;insert 10ms delay here
	OV9760MIPI_write_cmos_sensor(0x0340, 0x04); //;VTS
	OV9760MIPI_write_cmos_sensor(0x0341, 0x7C); //";VTS, 03/05/2012"
	OV9760MIPI_write_cmos_sensor(0x0342, 0x06); //";HTS, 03/05/2012"  0x06
	OV9760MIPI_write_cmos_sensor(0x0343, 0xc8); //";HTS, 03/05/2012"  //fa
	OV9760MIPI_write_cmos_sensor(0x0344, 0x00); //;x_addr_start
	OV9760MIPI_write_cmos_sensor(0x0345, 0x08); //";x_addr_start, 03/01/2012"
	OV9760MIPI_write_cmos_sensor(0x0346, 0x00); // ;y_addr_start
	OV9760MIPI_write_cmos_sensor(0x0347, 0x02); //;52 ;y_addr_start
	OV9760MIPI_write_cmos_sensor(0x0348, 0x05); //;x_addr_end
	OV9760MIPI_write_cmos_sensor(0x0349, 0xdf); //";x_addr_end, 03/01/2012"
	OV9760MIPI_write_cmos_sensor(0x034a, 0x04); //;y_addr_end
	OV9760MIPI_write_cmos_sensor(0x034b, 0x65); //;15 ;y_addr_end
	OV9760MIPI_write_cmos_sensor(0x3811, 0x04); //;x_offset
	OV9760MIPI_write_cmos_sensor(0x3813, 0x04); //;2 ;y_offset
	OV9760MIPI_write_cmos_sensor(0x034c, 0x05); //;x_output_size
	OV9760MIPI_write_cmos_sensor(0x034d, 0xC0); //;x_output_size
	OV9760MIPI_write_cmos_sensor(0x034e, 0x04); //;y_output_size
	OV9760MIPI_write_cmos_sensor(0x034f, 0x50); //;y_output_size
	OV9760MIPI_write_cmos_sensor(0x0383 , 0x01); //;x_odd_inc
	OV9760MIPI_write_cmos_sensor(0x0387 , 0x01); //;y_odd_inc
	OV9760MIPI_write_cmos_sensor(0x3820, 0x00); //;V bin
	OV9760MIPI_write_cmos_sensor(0x3821, 0x00); //;H bin
	OV9760MIPI_write_cmos_sensor(0x3660, 0x80); //";Analog control, 03/01/2012"
	OV9760MIPI_write_cmos_sensor(0x3680, 0xf4); //";Analog control, 03/01/2012"
	OV9760MIPI_write_cmos_sensor(0x0100 , 0x00); //;Mode select - stop streaming
	OV9760MIPI_write_cmos_sensor(0x3002, 0x80); //;IO control
	OV9760MIPI_write_cmos_sensor(0x3012, 0x08); //;MIPI control
	OV9760MIPI_write_cmos_sensor(0x3014, 0x04); //;MIPI control
	OV9760MIPI_write_cmos_sensor(0x3022, 0x02); //;Analog control
	OV9760MIPI_write_cmos_sensor(0x3023, 0x0f); //;Analog control
	OV9760MIPI_write_cmos_sensor(0x3080, 0x00); //;PLL control
	OV9760MIPI_write_cmos_sensor(0x3090, 0x02); //;PLL control
	OV9760MIPI_write_cmos_sensor(0x3091, 0x28); //;PLL control  //29
	OV9760MIPI_write_cmos_sensor(0x3092, 0x02); //;
	OV9760MIPI_write_cmos_sensor(0x3093, 0x02); //;PLL control
	OV9760MIPI_write_cmos_sensor(0x3094, 0x00); //;PLL control
	OV9760MIPI_write_cmos_sensor(0x3095, 0x00); //;PLL control
	OV9760MIPI_write_cmos_sensor(0x3096, 0x01); //;PLL control
	OV9760MIPI_write_cmos_sensor(0x3097, 0x00); //;PLL control
	OV9760MIPI_write_cmos_sensor(0x3098, 0x04); //;PLL control
	OV9760MIPI_write_cmos_sensor(0x3099, 0x14); //;PLL control
	OV9760MIPI_write_cmos_sensor(0x309a, 0x03); //;PLL control
	OV9760MIPI_write_cmos_sensor(0x309c, 0x00); //;PLL control
	OV9760MIPI_write_cmos_sensor(0x309d, 0x00); //;PLL control
	OV9760MIPI_write_cmos_sensor(0x309e, 0x01); //;PLL control
	OV9760MIPI_write_cmos_sensor(0x309f, 0x00); //;PLL control
	OV9760MIPI_write_cmos_sensor(0x30a2, 0x01); //;PLL control
	OV9760MIPI_write_cmos_sensor(0x30b0, 0x0a); //;v05
	OV9760MIPI_write_cmos_sensor(0x30b3, 0x32); //;PLL control
	OV9760MIPI_write_cmos_sensor(0x30b4, 0x02); //;PLL control
	OV9760MIPI_write_cmos_sensor(0x30b5, 0x00); //;PLL control
	OV9760MIPI_write_cmos_sensor(0x3503, 0x27); //";Auto gain/exposure, //set as 0x17 become manual mode"
	OV9760MIPI_write_cmos_sensor(0x3509, 0x10); //;AEC control  real gain
	OV9760MIPI_write_cmos_sensor(0x3600, 0x7c); //;Analog control
	OV9760MIPI_write_cmos_sensor(0x3621, 0xb8); //;v04
	OV9760MIPI_write_cmos_sensor(0x3622, 0x23); //;Analog control
	OV9760MIPI_write_cmos_sensor(0x3631, 0xe2); //;Analog control
	OV9760MIPI_write_cmos_sensor(0x3634, 0x03); //;Analog control
	OV9760MIPI_write_cmos_sensor(0x3662, 0x14); //;Analog control
	OV9760MIPI_write_cmos_sensor(0x366b, 0x03); //;Analog control
	OV9760MIPI_write_cmos_sensor(0x3682, 0x82); //;Analog control
	OV9760MIPI_write_cmos_sensor(0x3705, 0x20); //;
	OV9760MIPI_write_cmos_sensor(0x3708, 0x64); //;
	OV9760MIPI_write_cmos_sensor(0x371b, 0x60); //;Sensor control
	OV9760MIPI_write_cmos_sensor(0x3732, 0x40); //;Sensor control
	OV9760MIPI_write_cmos_sensor(0x3745, 0x00); //;Sensor control
	OV9760MIPI_write_cmos_sensor(0x3746, 0x18); //;Sensor control
	OV9760MIPI_write_cmos_sensor(0x3780, 0x2a); //;Sensor control
	OV9760MIPI_write_cmos_sensor(0x3781, 0x8c); //;Sensor control
	OV9760MIPI_write_cmos_sensor(0x378f, 0xf5); //;Sensor control
	OV9760MIPI_write_cmos_sensor(0x3823, 0x37); //;Internal timing control
	OV9760MIPI_write_cmos_sensor(0x383d, 0x88); //";Adjust starting black row for BLC calibration to avoid FIFO empty condition, 03/01/2012"
	OV9760MIPI_write_cmos_sensor(0x4000, 0x23); //";BLC control, 03/06/2012, disable DCBLC for production test"
	OV9760MIPI_write_cmos_sensor(0x4001, 0x04); //;BLC control
	OV9760MIPI_write_cmos_sensor(0x4002, 0x45); //;BLC control
	OV9760MIPI_write_cmos_sensor(0x4004, 0x08); //;BLC control
	OV9760MIPI_write_cmos_sensor(0x4005, 0x40); //;BLC for flashing
	OV9760MIPI_write_cmos_sensor(0x4006, 0x40); //;BLC control
	OV9760MIPI_write_cmos_sensor(0x4009, 0x40); //;BLC
	OV9760MIPI_write_cmos_sensor(0x404F, 0x8F); //";BLC control to improve black level fluctuation, 03/01/2012"
	OV9760MIPI_write_cmos_sensor(0x4058, 0x44); //;BLC control
	OV9760MIPI_write_cmos_sensor(0x4101, 0x32); //;BLC control
	OV9760MIPI_write_cmos_sensor(0x4102, 0xa4); //;BLC control
	OV9760MIPI_write_cmos_sensor(0x4520, 0xb0); //;For full res
	OV9760MIPI_write_cmos_sensor(0x4580, 0x08); //";Bypassing HDR gain latch, 03/01/2012"
	OV9760MIPI_write_cmos_sensor(0x4582, 0x00); //";Bypassing HDR gain latch, 03/01/2012"
	OV9760MIPI_write_cmos_sensor(0x4307, 0x30); //;MIPI control
	OV9760MIPI_write_cmos_sensor(0x4605, 0x00); //;VFIFO control v04 updated
	OV9760MIPI_write_cmos_sensor(0x4608, 0x02); //;VFIFO control v04 updated
	OV9760MIPI_write_cmos_sensor(0x4609, 0x00); //;VFIFO control v04 updated
	OV9760MIPI_write_cmos_sensor(0x4801, 0x0f); //;MIPI control
	OV9760MIPI_write_cmos_sensor(0x4819, 0xB6); //;MIPI control v05 updated
	OV9760MIPI_write_cmos_sensor(0x4837, 0x21); //;MIPI control
	OV9760MIPI_write_cmos_sensor(0x4906, 0xff); //;Internal timing control
	OV9760MIPI_write_cmos_sensor(0x4d00, 0x04); //;Temperature sensor
	OV9760MIPI_write_cmos_sensor(0x4d01, 0x4b); //;Temperature sensor
	OV9760MIPI_write_cmos_sensor(0x4d02, 0xfe); //;Temperature sensor
	OV9760MIPI_write_cmos_sensor(0x4d03, 0x09); //;Temperature sensor
	OV9760MIPI_write_cmos_sensor(0x4d04, 0x1e); //;Temperature sensor
	OV9760MIPI_write_cmos_sensor(0x4d05, 0xb7); //;Temperature sensor
	OV9760MIPI_write_cmos_sensor(0x4800, 0x44); // mipi short p. 0x44
	OV9760MIPI_write_cmos_sensor(0x100,  0x01); //
/*
	OV9760MIPI_write_cmos_sensor(0x3501, 0x45); //
	OV9760MIPI_write_cmos_sensor(0x3502, 0x80); //
	OV9760MIPI_write_cmos_sensor(0x350b, 0xf0); //
*/
}

void OV9760MIPI_Setvideosize(void)
{
	OV9760MIPI_write_cmos_sensor(0x0103 , 0x01); //;S/W reset
		Sleep(10);									//;insert 10ms delay here
		OV9760MIPI_write_cmos_sensor(0x0340 , 0x04); //;VTS
		OV9760MIPI_write_cmos_sensor(0x0341 , 0x7C); //";VTS, 03/05/2012"
		OV9760MIPI_write_cmos_sensor(0x0342 , 0x06); //";HTS, 03/05/2012"
		OV9760MIPI_write_cmos_sensor(0x0343 , 0xc8); //";HTS, 03/05/2012"
		OV9760MIPI_write_cmos_sensor(0x0344 , 0x00); //;x_addr_start
		OV9760MIPI_write_cmos_sensor(0x0345 , 0x08); //";x_addr_start, 03/01/2012"
		OV9760MIPI_write_cmos_sensor(0x0346 , 0x00); // ;y_addr_start
		OV9760MIPI_write_cmos_sensor(0x0347 , 0x8a); //;52 ;y_addr_start
		OV9760MIPI_write_cmos_sensor(0x0348 , 0x05); //;x_addr_end
		OV9760MIPI_write_cmos_sensor(0x0349 , 0xdf); //";x_addr_end, 03/01/2012"
		OV9760MIPI_write_cmos_sensor(0x034a, 0x04); //;y_addr_end
		OV9760MIPI_write_cmos_sensor(0x034b, 0x65); //;15 ;y_addr_end
		OV9760MIPI_write_cmos_sensor(0x3811, 0x04); //;x_offset
		OV9760MIPI_write_cmos_sensor(0x3813, 0x04); //;2 ;y_offset
		OV9760MIPI_write_cmos_sensor(0x034c, 0x05); //;x_output_size
		OV9760MIPI_write_cmos_sensor(0x034d, 0xC0); //;x_output_size
		OV9760MIPI_write_cmos_sensor(0x034e, 0x03); //;y_output_size
		OV9760MIPI_write_cmos_sensor(0x034f, 0x40); //;y_output_size
		OV9760MIPI_write_cmos_sensor(0x0383 , 0x01); //;x_odd_inc
		OV9760MIPI_write_cmos_sensor(0x0387 , 0x01); //;y_odd_inc
		OV9760MIPI_write_cmos_sensor(0x3820, 0x00); //;V bin
		OV9760MIPI_write_cmos_sensor(0x3821, 0x00); //;H bin
		OV9760MIPI_write_cmos_sensor(0x3660, 0x80); //";Analog control, 03/01/2012"
		OV9760MIPI_write_cmos_sensor(0x3680, 0xf4); //";Analog control, 03/01/2012"
		OV9760MIPI_write_cmos_sensor(0x0100 , 0x00); //	 ;Mode select - stop streaming
		OV9760MIPI_write_cmos_sensor(0x3002, 0x80); //;IO control
		OV9760MIPI_write_cmos_sensor(0x3012, 0x08); // ;MIPI control
		OV9760MIPI_write_cmos_sensor(0x3014, 0x04); // ;MIPI control
		OV9760MIPI_write_cmos_sensor(0x3022, 0x02); // ;Analog control
		OV9760MIPI_write_cmos_sensor(0x3023, 0x0f); //;Analog control
		OV9760MIPI_write_cmos_sensor(0x3080, 0x00); // ;PLL control
		OV9760MIPI_write_cmos_sensor(0x3090, 0x02); // ;PLL control
		OV9760MIPI_write_cmos_sensor(0x3091, 0x29); //;PLL control
		OV9760MIPI_write_cmos_sensor(0x3092, 0x02); // ;
		OV9760MIPI_write_cmos_sensor(0x3093, 0x02); // ;PLL control
		OV9760MIPI_write_cmos_sensor(0x3094, 0x00); // ;PLL control
		OV9760MIPI_write_cmos_sensor(0x3095, 0x00); // ;PLL control
		OV9760MIPI_write_cmos_sensor(0x3096, 0x01); // ;PLL control
		OV9760MIPI_write_cmos_sensor(0x3097, 0x00); // ;PLL control
		OV9760MIPI_write_cmos_sensor(0x3098, 0x04); // ;PLL control
		OV9760MIPI_write_cmos_sensor(0x3099, 0x14); //;PLL control
		OV9760MIPI_write_cmos_sensor(0x309a, 0x03); // ;PLL control
		OV9760MIPI_write_cmos_sensor(0x309c, 0x00); // ;PLL control
		OV9760MIPI_write_cmos_sensor(0x309d, 0x00); // ;PLL control
		OV9760MIPI_write_cmos_sensor(0x309e, 0x01); // ;PLL control
		OV9760MIPI_write_cmos_sensor(0x309f, 0x00); // ;PLL control
		OV9760MIPI_write_cmos_sensor(0x30a2, 0x01); // ;PLL control
		OV9760MIPI_write_cmos_sensor(0x30b0, 0x0a); //;v05
		OV9760MIPI_write_cmos_sensor(0x30b3, 0x32); //;PLL control
		OV9760MIPI_write_cmos_sensor(0x30b4, 0x02); // ;PLL control
		OV9760MIPI_write_cmos_sensor(0x30b5, 0x00); // ;PLL control
		OV9760MIPI_write_cmos_sensor(0x3503, 0x27); //";Auto gain/exposure, //set as 0x17 become manual mode"
		OV9760MIPI_write_cmos_sensor(0x3509, 0x10); //;AEC control
		OV9760MIPI_write_cmos_sensor(0x3600, 0x7c); //;Analog control
		OV9760MIPI_write_cmos_sensor(0x3621, 0xb8); //;v04
		OV9760MIPI_write_cmos_sensor(0x3622, 0x23); //;Analog control
		OV9760MIPI_write_cmos_sensor(0x3631, 0xe2); //;Analog control
		OV9760MIPI_write_cmos_sensor(0x3634, 0x03); // ;Analog control
		OV9760MIPI_write_cmos_sensor(0x3662, 0x14); //;Analog control
		OV9760MIPI_write_cmos_sensor(0x366b, 0x3 ); //;Analog control
		OV9760MIPI_write_cmos_sensor(0x3682, 0x82); //;Analog control
		OV9760MIPI_write_cmos_sensor(0x3705, 0x20); //;
		OV9760MIPI_write_cmos_sensor(0x3708, 0x64); //;
		OV9760MIPI_write_cmos_sensor(0x371b, 0x60); //;Sensor control
		OV9760MIPI_write_cmos_sensor(0x3732, 0x40); //;Sensor control
		OV9760MIPI_write_cmos_sensor(0x3745, 0x00); //;Sensor control
		OV9760MIPI_write_cmos_sensor(0x3746, 0x18); //;Sensor control
		OV9760MIPI_write_cmos_sensor(0x3780, 0x2a); //;Sensor control
		OV9760MIPI_write_cmos_sensor(0x3781, 0x8c); //;Sensor control
		OV9760MIPI_write_cmos_sensor(0x378f, 0xf5); //;Sensor control
		OV9760MIPI_write_cmos_sensor(0x3823, 0x37); //;Internal timing control
		OV9760MIPI_write_cmos_sensor(0x383d, 0x88); //";Adjust starting black row for BLC calibration to avoid FIFO empty condition, 03/01/2012"
		OV9760MIPI_write_cmos_sensor(0x4000, 0x23); //";BLC control, 03/06/2012, disable DCBLC for production test"
		OV9760MIPI_write_cmos_sensor(0x4001, 0x04); //;BLC control
		OV9760MIPI_write_cmos_sensor(0x4002, 0x45); //;BLC control
		OV9760MIPI_write_cmos_sensor(0x4004, 0x08); //;BLC control
		OV9760MIPI_write_cmos_sensor(0x4005, 0x40); //;BLC for flashing
		OV9760MIPI_write_cmos_sensor(0x4006, 0x40); //;BLC control
		OV9760MIPI_write_cmos_sensor(0x4009, 0x40); //;BLC
		OV9760MIPI_write_cmos_sensor(0x404F, 0x8F); //";BLC control to improve black level fluctuation, 03/01/2012"
		OV9760MIPI_write_cmos_sensor(0x4058, 0x44); //;BLC control
		OV9760MIPI_write_cmos_sensor(0x4101, 0x32); //;BLC control
		OV9760MIPI_write_cmos_sensor(0x4102, 0xa4); //;BLC control
		OV9760MIPI_write_cmos_sensor(0x4520, 0xb0); //;For full res
		OV9760MIPI_write_cmos_sensor(0x4580, 0x08); //";Bypassing HDR gain latch, 03/01/2012"
		OV9760MIPI_write_cmos_sensor(0x4582, 0x00); //";Bypassing HDR gain latch, 03/01/2012"
		OV9760MIPI_write_cmos_sensor(0x4307, 0x30); //;MIPI control
		OV9760MIPI_write_cmos_sensor(0x4605, 0x00); //;VFIFO control v04 updated
		OV9760MIPI_write_cmos_sensor(0x4608, 0x02); //;VFIFO control v04 updated
		OV9760MIPI_write_cmos_sensor(0x4609, 0x00); //;VFIFO control v04 updated
		OV9760MIPI_write_cmos_sensor(0x4801, 0x0f); //;MIPI control
		OV9760MIPI_write_cmos_sensor(0x4819, 0xB6); //;MIPI control v05 updated
		OV9760MIPI_write_cmos_sensor(0x4837, 0x21); //;MIPI control
		OV9760MIPI_write_cmos_sensor(0x4906, 0xff); //;Internal timing control
		OV9760MIPI_write_cmos_sensor(0x4d00, 0x04); //;Temperature sensor
		OV9760MIPI_write_cmos_sensor(0x4d01, 0x4b); //;Temperature sensor
		OV9760MIPI_write_cmos_sensor(0x4d02, 0xfe); //;Temperature sensor
		OV9760MIPI_write_cmos_sensor(0x4d03, 0x09); //;Temperature sensor
		OV9760MIPI_write_cmos_sensor(0x4d04, 0x1e); //;Temperature sensor
		OV9760MIPI_write_cmos_sensor(0x4d05, 0xb7); //;Temperature sensor
		OV9760MIPI_write_cmos_sensor(0x4800, 0x44); // mipi short p. 0x44
		OV9760MIPI_write_cmos_sensor(0x100 , 0x01); //
#if 0
		OV9760MIPI_write_cmos_sensor(0x3501, 0x45); //
		OV9760MIPI_write_cmos_sensor(0x3502, 0x80); //
		OV9760MIPI_write_cmos_sensor(0x350b, 0xf0); //
#endif
		

}



/*****************************************************************************/
/* Windows Mobile Sensor Interface */
/*****************************************************************************/
/*************************************************************************
* FUNCTION
*	OV9760MIPIOpen
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
UINT32 OV9760MIPIOpen(void)
{
	//volatile signed char i;
	kal_uint16 sensor_id=0;
	int i;
	const kal_uint16 sccb_writeid[] = {OV9760MIPI_WRITE_ID,OV9760MIPI_WRITE_ID_1};

	for(i = 0; i <(sizeof(sccb_writeid)/sizeof(sccb_writeid[0])); i++)
	{
		write_add=sccb_writeid[i];
		sensor_id  =((OV9760MIPI_read_cmos_sensor(0x0000)<<8) | OV9760MIPI_read_cmos_sensor(0x0001));
		
		SENSORDB("------Sensor Read OV9760MIPI ID %x \r\n",(unsigned int)sensor_id);

		if(sensor_id==OV9760MIPI_SENSOR_ID)
			break;
	}

	if (sensor_id != OV9760MIPI_SENSOR_ID)
	{
		SENSORDB("Sensor Read ByeBye \r\n");
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	OV9760MIPI_Set720P();
	return ERROR_NONE;
}

/*************************************************************************
* FUNCTION
*	OV9760MIPIClose
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
UINT32 OV9760MIPIClose(void)
{
	return ERROR_NONE;
}	/* OV9760MIPIClose() */


void OV9760MIPI_SetMirrorFlip(kal_uint8 image_mirror)
{

	SENSORDB("OV9760MIPI HVMirror:%d\n",image_mirror);
	
	switch (image_mirror)
    {
    case IMAGE_NORMAL:
        OV9760MIPI_write_cmos_sensor(0x0101 , 0x00); //;Orientation
     break;
    case IMAGE_H_MIRROR:
        OV9760MIPI_write_cmos_sensor(0x0101 , 0x01); //;Orientation
      break;
    case IMAGE_V_MIRROR:
        OV9760MIPI_write_cmos_sensor(0x0101 , 0x02); //;Orientation
         break;
    case IMAGE_HV_MIRROR:
        OV9760MIPI_write_cmos_sensor(0x0101 , 0x03); //;Orientation
      break;
    default:
    ASSERT(0);
    }

}

/*************************************************************************
* FUNCTION
*	OV9760MIPIPreview
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
UINT32 OV9760MIPIPreview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	/* OV9760 1472X1104 FULL 30FPS MIPI_1_LANE 24Mhz input
	Revision history
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	03/05/2012, V13, based on the setting released to ABC on 3/5/2012"
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	input clock: 24MHz; system clock: 60MHz = 24/2*40/2/2/2; DAC clock: 240MHz = 24/2*20;
	MIPI data rate: 600Mbps = 24/2*50
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	Changes:
	1. Update register 0x0344~0x034f, 0x3811 and 0x3813 for the image window size/position"
	2. Update BLC control register, 0x4582, 0x4580, 0x4005 and 0x404f to fix missing BLC trigger
	and improve the BLC fluctuation"
	3. Add register 0x383d to solve the abnormal color after enabling BLC calibration
	4. Add/change register 0x3660, 0x3680, 0x383, and 0x387 to sync with other resolution"
	5. Change register 0x4000 to 0x43 from 0x23 to enable BLC calibration
	6. Increase HTS (because the ADC and system clock is lower than the original value)
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;*/

	SENSORDB("-----------OV9760MIPIPreview------------ \r\n");
	  
//	OV9760MIPI_Set720P();
	OV9760MIPI_SetMirrorFlip(IMAGE_H_MIRROR);
	OV9760MIPI_set_dummy(0,0);
	SENSORDB("-----------OV9760MIPIPreview---end--------- \r\n");
	
  	return ERROR_NONE;
}	/* OV9760MIPIPreview() */

UINT32 OV9760MIPICapture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	

	SENSORDB("-----------OV9760MIPICapture------------ \r\n");
	
//	OV9760MIPI_write_cmos_sensor(0x5a80,0x84);  
//	OV9760MIPI_Set720P();
	OV9760MIPI_SetMirrorFlip(IMAGE_H_MIRROR);
	OV9760MIPI_set_dummy(0,0);
	SENSORDB("-----------OV9760MIPICapture---end--------- \r\n");
	
  	return ERROR_NONE;
}	/* OV9760MIPIPreview() */


UINT32 OV9760MIPIVideo(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                            MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{



	SENSORDB("-----------OV9760MIPIVideo------------ \r\n");
	
	//OV9760MIPI_Set720P();
	OV9760MIPI_SetMirrorFlip(IMAGE_H_MIRROR);
	OV9760MIPI_set_dummy(0,0);
     return ERROR_NONE;
}        /* OV9760MIPICapture() */

UINT32 OV9760MIPIGetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution)
{
	pSensorResolution->SensorFullWidth=OV9760MIPI_IMAGE_SENSOR_FULL_WIDTH;  //modify by yanxu
	pSensorResolution->SensorFullHeight=OV9760MIPI_IMAGE_SENSOR_FULL_HEIGHT;
	pSensorResolution->SensorPreviewWidth=OV9760MIPI_IMAGE_SENSOR_FULL_WIDTH; 
	pSensorResolution->SensorPreviewHeight=OV9760MIPI_IMAGE_SENSOR_FULL_HEIGHT;
	pSensorResolution->SensorVideoWidth=OV9760MIPI_IMAGE_SENSOR_FULL_WIDTH; 
	pSensorResolution->SensorVideoHeight=OV9760MIPI_IMAGE_SENSOR_FULL_HEIGHT;
	pSensorResolution->Sensor3DFullWidth=OV9760MIPI_IMAGE_SENSOR_FULL_WIDTH;
	pSensorResolution->Sensor3DFullHeight=OV9760MIPI_IMAGE_SENSOR_FULL_HEIGHT;	
	pSensorResolution->Sensor3DPreviewWidth=OV9760MIPI_IMAGE_SENSOR_FULL_WIDTH;
	pSensorResolution->Sensor3DPreviewHeight=OV9760MIPI_IMAGE_SENSOR_FULL_HEIGHT;		
	pSensorResolution->Sensor3DVideoWidth=OV9760MIPI_IMAGE_SENSOR_FULL_WIDTH;
	pSensorResolution->Sensor3DVideoHeight=OV9760MIPI_IMAGE_SENSOR_FULL_HEIGHT;
	return ERROR_NONE;
}	/* OV9760MIPIGetResolution() */

UINT32 OV9760MIPIGetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId,
					  MSDK_SENSOR_INFO_STRUCT *pSensorInfo,
					  MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{

	switch(ScenarioId)
    {
	  case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	  case MSDK_SCENARIO_ID_CAMERA_ZSD:
	  case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		   pSensorInfo->SensorPreviewResolutionX=OV9760MIPI_IMAGE_SENSOR_FULL_WIDTH;
		   pSensorInfo->SensorPreviewResolutionY=OV9760MIPI_IMAGE_SENSOR_FULL_HEIGHT;
		   pSensorInfo->SensorFullResolutionX=OV9760MIPI_IMAGE_SENSOR_FULL_WIDTH;
		   pSensorInfo->SensorFullResolutionY=OV9760MIPI_IMAGE_SENSOR_FULL_HEIGHT;				   
		   pSensorInfo->SensorCameraPreviewFrameRate=30;
		   break;
	  case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		   pSensorInfo->SensorPreviewResolutionX=OV9760MIPI_IMAGE_SENSOR_VIDEO_WIDTH;
		   pSensorInfo->SensorPreviewResolutionY=OV9760MIPI_IMAGE_SENSOR_VIDEO_HEIGHT;
		   pSensorInfo->SensorFullResolutionX=OV9760MIPI_IMAGE_SENSOR_FULL_WIDTH;
		   pSensorInfo->SensorFullResolutionY=OV9760MIPI_IMAGE_SENSOR_FULL_HEIGHT;				   
		   pSensorInfo->SensorCameraPreviewFrameRate=30;		  
		  break;
	  default:

		   pSensorInfo->SensorPreviewResolutionX=OV9760MIPI_IMAGE_SENSOR_FULL_WIDTH;
		   pSensorInfo->SensorPreviewResolutionY=OV9760MIPI_IMAGE_SENSOR_FULL_HEIGHT;
		   pSensorInfo->SensorFullResolutionX=OV9760MIPI_IMAGE_SENSOR_FULL_WIDTH;
		   pSensorInfo->SensorFullResolutionY=OV9760MIPI_IMAGE_SENSOR_FULL_HEIGHT;				   
		   pSensorInfo->SensorCameraPreviewFrameRate=30;
		   break;
			   
	}

	pSensorInfo->SensorVideoFrameRate=30;
	pSensorInfo->SensorStillCaptureFrameRate=30;
	pSensorInfo->SensorWebCamCaptureFrameRate=15;
	pSensorInfo->SensorResetActiveHigh=FALSE; //low active
	pSensorInfo->SensorResetDelayCount=5; 

	pSensorInfo->SensorOutputDataFormat=SENSOR_OUTPUT_FORMAT_RAW_B;
	pSensorInfo->SensorClockPolarity=SENSOR_CLOCK_POLARITY_LOW;	
	pSensorInfo->SensorClockFallingPolarity=SENSOR_CLOCK_POLARITY_LOW;
	pSensorInfo->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	pSensorInfo->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;

	pSensorInfo->SensorInterruptDelayLines = 4;
#ifdef MIPI_INTERFACE
	pSensorInfo->SensroInterfaceType        = SENSOR_INTERFACE_TYPE_MIPI;
#else
   	pSensorInfo->SensroInterfaceType		= SENSOR_INTERFACE_TYPE_PARALLEL;
#endif
	pSensorInfo->CaptureDelayFrame = 1; 
	pSensorInfo->PreviewDelayFrame = 3; 
	pSensorInfo->VideoDelayFrame = 1; 	

	pSensorInfo->SensorMasterClockSwitch = 0; 
    pSensorInfo->SensorDrivingCurrent = ISP_DRIVING_2MA;
    pSensorInfo->AEShutDelayFrame = 0;		   /* The frame of setting shutter default 0 for TG int */
	pSensorInfo->AESensorGainDelayFrame = 1;	   /* The frame of setting sensor gain */
	pSensorInfo->AEISPGainDelayFrame = 2;    
	switch (ScenarioId)
	{
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			pSensorInfo->SensorClockFreq=24;
			pSensorInfo->SensorClockDividCount=	3;
			pSensorInfo->SensorClockRisingCount= 0;
			pSensorInfo->SensorClockFallingCount= 2;
			pSensorInfo->SensorPixelClockCount= 3;
			pSensorInfo->SensorDataLatchCount= 2;
			pSensorInfo->SensorGrabStartX = 2; 
			pSensorInfo->SensorGrabStartY = 2; 

			pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_1_LANE;		

            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0; 
	        pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 14; 
	        pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorWidthSampling = 0;  // 0 is default 1x
            pSensorInfo->SensorHightSampling = 0;   // 0 is default 1x 
            pSensorInfo->SensorPacketECCOrder = 1;
		break;
		
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			pSensorInfo->SensorClockFreq=24;
			pSensorInfo->SensorClockDividCount=	3;
			pSensorInfo->SensorClockRisingCount= 0;
			pSensorInfo->SensorClockFallingCount= 2;
			pSensorInfo->SensorPixelClockCount= 3;
			pSensorInfo->SensorDataLatchCount= 2;
			pSensorInfo->SensorGrabStartX = 2; 
			pSensorInfo->SensorGrabStartY = 2; 
			pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_1_LANE;		
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0; 
	        pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 14; 
	        pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorWidthSampling = 0;  // 0 is default 1x
            pSensorInfo->SensorHightSampling = 0;   // 0 is default 1x 
            pSensorInfo->SensorPacketECCOrder = 1;
		break;	
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
			pSensorInfo->SensorClockFreq=24;
			pSensorInfo->SensorClockDividCount= 3;
			pSensorInfo->SensorClockRisingCount=0;
			pSensorInfo->SensorClockFallingCount=2;
			pSensorInfo->SensorPixelClockCount=3;
			pSensorInfo->SensorDataLatchCount=2;
			pSensorInfo->SensorGrabStartX = 2; 
			pSensorInfo->SensorGrabStartY = 2; 

			pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_1_LANE;	
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0; 
	        pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 14; 
	        pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorWidthSampling = 0;  // 0 is default 1x
            pSensorInfo->SensorHightSampling = 0;   // 0 is default 1x 
            pSensorInfo->SensorPacketECCOrder = 1;
		break;
		default:
			pSensorInfo->SensorClockFreq=24;
			pSensorInfo->SensorClockDividCount=3;
			pSensorInfo->SensorClockRisingCount=0;
			pSensorInfo->SensorClockFallingCount=2;		
			pSensorInfo->SensorPixelClockCount=3;
			pSensorInfo->SensorDataLatchCount=2;
			pSensorInfo->SensorGrabStartX = 2; 
			pSensorInfo->SensorGrabStartY = 2; 

			pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_1_LANE;	
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0; 
	        pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 14; 
	        pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorWidthSampling = 0;  // 0 is default 1x
            pSensorInfo->SensorHightSampling = 0;   // 0 is default 1x 
            pSensorInfo->SensorPacketECCOrder = 1;
		break;
	}
	
  	return ERROR_NONE;
}


UINT32 OV9760MIPIControl(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow,
					  MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
	spin_lock(&ov9740mipi_raw_drv_lock);
	CurrentScenarioId_9760 = ScenarioId;
	spin_unlock(&ov9740mipi_raw_drv_lock);
	
	switch (ScenarioId)
	{
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			OV9760MIPIVideo(pImageWindow, pSensorConfigData);
		break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			OV9760MIPIPreview(pImageWindow, pSensorConfigData);
		break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
			OV9760MIPICapture(pImageWindow, pSensorConfigData);
		break;
        default:
            return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}	/* OV9760MIPIControl() */

UINT32 OV9760MIPIRAWSetVideoMode(UINT16 u2FrameRate)
{
	kal_int16 dummy_line;
	/* to fix VSYNC, to fix frame rate */
	SENSORDB("OV9760MIPIRAWSetVideoMode£¬u2FrameRate:%d\n",u2FrameRate);	
	if(0 == u2FrameRate)
	{
		spin_lock(&ov9740mipi_raw_drv_lock);
		OV9760_sensor.video_mode = KAL_FALSE;
		spin_unlock(&ov9740mipi_raw_drv_lock);
		
		SENSORDB("disable video mode\n");
	}
	else 
	{
		dummy_line = OV9760_sensor.pclk / u2FrameRate / OV9760_sensor.line_length - OV9760MIPI_PERIOD_LINE_NUMS;
		if (dummy_line < 0) dummy_line = 0;

		SENSORDB("dummy_line %d\n", dummy_line);

		OV9760MIPI_set_dummy(0, dummy_line); /* modify dummy_pixel must gen AE table again */
		spin_lock(&ov9740mipi_raw_drv_lock);
		OV9760_sensor.video_mode = KAL_TRUE;
		spin_unlock(&ov9740mipi_raw_drv_lock);
	}
	
	return ERROR_NONE;
}

void OV9760_Write_Shutter(kal_int16 ishutter)
{
	kal_uint16 extra_shutter = 0;
	kal_uint16 realtime_fp = 0;
	kal_uint16 frame_height = 0;
	//kal_uint16 line_length = 0;

	unsigned long flags;

	SENSORDB("---OV9760_Write_Shutter:%d \n",ishutter);

	if (ishutter<=0) ishutter = 1; /* avoid 0 */

     frame_height=OV9760MIPI_PERIOD_LINE_NUMS + OV9760_sensor.dummy_lines;;
	if(ishutter > (frame_height - 4)){
		extra_shutter = ishutter - frame_height + 4;
		SENSORDB("[shutter > frame_height] frame_height:%x extra_shutter:%x \n",frame_height, extra_shutter);
	}
	else{
		extra_shutter = 0;
	}
	frame_height += extra_shutter;
	OV9760_sensor.frame_height = frame_height;
	SENSORDB("OV9760_sensor.is_autofliker:%x, OV9760_sensor.frame_height: %x \n",OV9760_sensor.is_autofliker,OV9760_sensor.frame_height);

	if(OV9760_sensor.is_autofliker == KAL_TRUE)
	{
		realtime_fp = OV9760_sensor.pclk *10 / (OV9760_sensor.line_length * OV9760_sensor.frame_height);
		SENSORDB("[OV9760_Write_Shutter]pv_clk:%d\n",OV9760_sensor.pclk);
		SENSORDB("[OV9760_Write_Shutter]line_length:%d\n",OV9760_sensor.line_length);
		SENSORDB("[OV9760_Write_Shutter]frame_height:%d\n",OV9760_sensor.frame_height);
		SENSORDB("[OV9760_Write_Shutter]framerate(10base):%d\n",realtime_fp);

		if((realtime_fp >= 298)&&(realtime_fp <= 302))
		{
			realtime_fp = 297;
			spin_lock_irqsave(&ov9740mipi_raw_drv_lock,flags);
			OV9760_sensor.frame_height = OV9760_sensor.pclk *10 / (OV9760_sensor.line_length * realtime_fp);
			spin_unlock_irqrestore(&ov9740mipi_raw_drv_lock,flags);

			SENSORDB("[autofliker realtime_fp=30,extern heights slowdown to 29.6fps][height:%d]",OV9760_sensor.frame_height);
		}
		else if((realtime_fp >= 147)&&(realtime_fp <= 153))
		{
			realtime_fp = 146;
			spin_lock_irqsave(&ov9740mipi_raw_drv_lock,flags);
			OV9760_sensor.frame_height = OV9760_sensor.pclk *10 / (OV9760_sensor.line_length * realtime_fp);
			spin_unlock_irqrestore(&ov9740mipi_raw_drv_lock,flags);

			SENSORDB("[autofliker realtime_fp=15,extern heights slowdown to 14.6fps][height:%d]",OV9760_sensor.frame_height);
		}
	}
	
	//set vts
//	OV9760MIPI_write_cmos_sensor(0x380f, (OV9760_sensor.frame_height)&0xFF);
//	OV9760MIPI_write_cmos_sensor(0x380e, (OV9760_sensor.frame_height>>8)&0xFF);
	OV9760MIPI_write_cmos_sensor(0x0341, (OV9760_sensor.frame_height)&0xFF);
	OV9760MIPI_write_cmos_sensor(0x0340, (OV9760_sensor.frame_height>>8)&0xFF);


	OV9760MIPI_write_cmos_sensor(0x3502, (ishutter&0xf) << 4);
	OV9760MIPI_write_cmos_sensor(0x3501, (ishutter&0xfff)>> 4);	
	OV9760MIPI_write_cmos_sensor(0x3500, (ishutter >> 12));
}


#if 0
static kal_uint16 OV9760MIPI_Reg2Gain(const kal_uint8 iReg)
{
    kal_uint8 iI;
    kal_uint16 iGain = BASEGAIN;    // 1x-gain base

    // Range: 1x to 32x
    // Gain = (GAIN[7] + 1) * (GAIN[6] + 1) * (GAIN[5] + 1) * (GAIN[4] + 1) * (1 + GAIN[3:0] / 16)
    for (iI = 7; iI >= 4; iI--) {
        iGain *= (((iReg >> iI) & 0x01) + 1);
    }

    return iGain +  iGain * (iReg & 0x0F) / 16;

}
#endif


 kal_uint16  OV9760MIPI_Gain2reg(const kal_uint16 iGain)
{
 	kal_uint16 reg_val=0;
	reg_val= iGain/(BASEGAIN/16);
	return reg_val;

}

void OV9760_Set_Gain(kal_uint16 gain16)
{
	//noitce 0x3509[4]=1 we uss real gain mode.
	kal_uint16 reg=0;
	reg=OV9760MIPI_Gain2reg(gain16);
	SENSORDB("OV9760_SetGain:%d,reg=%0d.\n",gain16,reg);
	if(reg>0x3d0)
		reg=0x3d0;   
	OV9760MIPI_write_cmos_sensor(0x350b,reg&0xff);
	OV9760MIPI_write_cmos_sensor(0x350a,reg>>8);
	return;
}
/* write camera_para to sensor register */
static void OV9760_camera_para_to_sensor(void)
{
	kal_uint32 i;
	
	SENSORDB("OV9760_camera_para_to_sensor\n");

	for (i = 0; 0xFFFFFFFF != OV9760_sensor.eng.reg[i].Addr; i++)
	{
		OV9760MIPI_write_cmos_sensor(OV9760_sensor.eng.reg[i].Addr, OV9760_sensor.eng.reg[i].Para);
	}
	for (i = OV9760_FACTORY_START_ADDR; 0xFFFFFFFF != OV9760_sensor.eng.reg[i].Addr; i++)
	{
		OV9760MIPI_write_cmos_sensor(OV9760_sensor.eng.reg[i].Addr, OV9760_sensor.eng.reg[i].Para);
	}
	OV9760_Set_Gain(OV9760_sensor.gain); /* update gain */
}
/* update camera_para from sensor register */
static void OV9760_sensor_to_camera_para(void)
{
	kal_uint32 i;
	kal_uint32 temp_data;

	SENSORDB("OV9760_sensor_to_camera_para\n");

	for (i = 0; 0xFFFFFFFF != OV9760_sensor.eng.reg[i].Addr; i++)
	{
		temp_data = OV9760MIPI_read_cmos_sensor(OV9760_sensor.eng.reg[i].Addr);

		spin_lock(&ov9740mipi_raw_drv_lock);
		OV9760_sensor.eng.reg[i].Para = temp_data;
		spin_unlock(&ov9740mipi_raw_drv_lock);

	}
	for (i = OV9760_FACTORY_START_ADDR; 0xFFFFFFFF != OV9760_sensor.eng.reg[i].Addr; i++)
	{
		temp_data = OV9760MIPI_read_cmos_sensor(OV9760_sensor.eng.reg[i].Addr);

		spin_lock(&ov9740mipi_raw_drv_lock);
		OV9760_sensor.eng.reg[i].Para = temp_data;
		spin_unlock(&ov9740mipi_raw_drv_lock);
	}
}
inline static void OV9760_get_sensor_item_info(MSDK_SENSOR_ITEM_INFO_STRUCT *para)
{

  const static kal_char *cct_item_name[] = {"SENSOR_BASEGAIN", "Pregain-R", "Pregain-Gr", "Pregain-Gb", "Pregain-B"};
  const static kal_char *editer_item_name[] = {"REG addr", "REG value"};
  

	SENSORDB("OV9760_get_sensor_item_info\n");

  switch (para->GroupIdx)
  {
  case OV9760_PRE_GAIN:
    switch (para->ItemIdx)
    {
    case OV9760_SENSOR_BASEGAIN:
    case OV9760_PRE_GAIN_R_INDEX:
    case OV9760_PRE_GAIN_Gr_INDEX:
    case OV9760_PRE_GAIN_Gb_INDEX:
    case OV9760_PRE_GAIN_B_INDEX:
      break;
    default:
      ASSERT(0);
    }
    sprintf(para->ItemNamePtr, cct_item_name[para->ItemIdx - OV9760_SENSOR_BASEGAIN]);
    para->ItemValue = OV9760_sensor.eng.cct[para->ItemIdx].Para * 1000 / BASEGAIN;
    para->IsTrueFalse = para->IsReadOnly = para->IsNeedRestart = KAL_FALSE;
    para->Min = OV9760_MIN_ANALOG_GAIN * 1000;
    para->Max = OV9760_MAX_ANALOG_GAIN * 1000;
    break;
  case OV9760_CMMCLK_CURRENT:
    switch (para->ItemIdx)
    {
    case 0:
      sprintf(para->ItemNamePtr, "Drv Cur[2,4,6,8]mA");
      switch (OV9760_sensor.eng.reg[OV9760_CMMCLK_CURRENT_INDEX].Para)
      {
      case ISP_DRIVING_2MA:
        para->ItemValue = 2;
        break;
      case ISP_DRIVING_4MA:
        para->ItemValue = 4;
        break;
      case ISP_DRIVING_6MA:
        para->ItemValue = 6;
        break;
      case ISP_DRIVING_8MA:
        para->ItemValue = 8;
        break;
      default:
        ASSERT(0);
      }
      para->IsTrueFalse = para->IsReadOnly = KAL_FALSE;
      para->IsNeedRestart = KAL_TRUE;
      para->Min = 2;
      para->Max = 8;
      break;
    default:
      ASSERT(0);
    }
    break;
  case OV9760_FRAME_RATE_LIMITATION:
    switch (para->ItemIdx)
    {
    case 0:
      sprintf(para->ItemNamePtr, "Max Exposure Lines");
      para->ItemValue = 2998;
      break;
    case 1:
      sprintf(para->ItemNamePtr, "Min Frame Rate");
      para->ItemValue = 5;
      break;
    default:
      ASSERT(0);
    }
    para->IsTrueFalse = para->IsNeedRestart = KAL_FALSE;
    para->IsReadOnly = KAL_TRUE;
    para->Min = para->Max = 0;
    break;
  case OV9760_REGISTER_EDITOR:
    switch (para->ItemIdx)
    {
    case 0:
    case 1:
      sprintf(para->ItemNamePtr, editer_item_name[para->ItemIdx]);
      para->ItemValue = 0;
      para->IsTrueFalse = para->IsReadOnly = para->IsNeedRestart = KAL_FALSE;
      para->Min = 0;
      para->Max = (para->ItemIdx == 0 ? 0xFFFF : 0xFF);
      break;
    default:
      ASSERT(0);
    }
    break;
  default:
    ASSERT(0);
  }
}
inline static kal_bool OV9760_set_sensor_item_info(MSDK_SENSOR_ITEM_INFO_STRUCT *para)
{
  kal_uint16 temp_para;

   SENSORDB("OV9760_set_sensor_item_info\n");

  switch (para->GroupIdx)
  {
  case OV9760_PRE_GAIN:
    switch (para->ItemIdx)
    {
    case OV9760_SENSOR_BASEGAIN:
    case OV9760_PRE_GAIN_R_INDEX:
    case OV9760_PRE_GAIN_Gr_INDEX:
    case OV9760_PRE_GAIN_Gb_INDEX:
    case OV9760_PRE_GAIN_B_INDEX:
	  spin_lock(&ov9740mipi_raw_drv_lock);
      OV9760_sensor.eng.cct[para->ItemIdx].Para = para->ItemValue * BASEGAIN / 1000;
	  spin_unlock(&ov9740mipi_raw_drv_lock);
	  
      OV9760_Set_Gain(OV9760_sensor.gain); /* update gain */
      break;
    default:
      ASSERT(0);
    }
    break;
  case OV9760_CMMCLK_CURRENT:
    switch (para->ItemIdx)
    {
    case 0:
      switch (para->ItemValue)
      {
      case 2:
        temp_para = ISP_DRIVING_2MA;
        break;
      case 3:
      case 4:
        temp_para = ISP_DRIVING_4MA;
        break;
      case 5:
      case 6:
        temp_para = ISP_DRIVING_6MA;
        break;
      default:
        temp_para = ISP_DRIVING_8MA;
        break;
      }
      
	  spin_lock(&ov9740mipi_raw_drv_lock);
      OV9760_sensor.eng.reg[OV9760_CMMCLK_CURRENT_INDEX].Para = temp_para;
	  spin_unlock(&ov9740mipi_raw_drv_lock);
      break;
    default:
      ASSERT(0);
    }
    break;
  case OV9760_FRAME_RATE_LIMITATION:
    ASSERT(0);
    break;
  case OV9760_REGISTER_EDITOR:
    switch (para->ItemIdx)
    {
      static kal_uint32 fac_sensor_reg;
    case 0:
      if (para->ItemValue < 0 || para->ItemValue > 0xFFFF) return KAL_FALSE;
      fac_sensor_reg = para->ItemValue;
      break;
    case 1:
      if (para->ItemValue < 0 || para->ItemValue > 0xFF) return KAL_FALSE;
      OV9760MIPI_write_cmos_sensor(fac_sensor_reg, para->ItemValue);
      break;
    default:
      ASSERT(0);
    }
    break;
  default:
    ASSERT(0);
  }
  return KAL_TRUE;
}

UINT32 OV9760SetAutoFlickerMode(kal_bool bEnable, UINT16 u2FrameRate)
{
	
	
	
	SENSORDB("[OV9760SetAutoFlickerMode] bEnable = d%, frame rate(10base) = %d\n", bEnable, u2FrameRate);

	if(bEnable)
	{
	    
		spin_lock(&ov9740mipi_raw_drv_lock);
		OV9760_sensor.is_autofliker = KAL_TRUE;
		spin_unlock(&ov9740mipi_raw_drv_lock);
	}
	else
	{
		spin_lock(&ov9740mipi_raw_drv_lock);
		OV9760_sensor.is_autofliker = KAL_FALSE;
		spin_unlock(&ov9740mipi_raw_drv_lock);
	}
	SENSORDB("[OV9760SetAutoFlickerMode]bEnable:%x \n",bEnable);
	return ERROR_NONE;
}
UINT32 OV9760SetCalData(PSET_SENSOR_CALIBRATION_DATA_STRUCT pSetSensorCalData){
	UINT32 i;
	SENSORDB("OV9760 Sensor write calibration data num = %d \r\n", pSetSensorCalData->DataSize);
	SENSORDB("OV9760 Sensor write calibration data format = %x \r\n", pSetSensorCalData->DataFormat);
	if(pSetSensorCalData->DataSize <= MAX_SHADING_DATA_TBL)	{
		for (i = 0; i < pSetSensorCalData->DataSize; i++)		{
			if (((pSetSensorCalData->DataFormat & 0xFFFF) == 1) && ((pSetSensorCalData->DataFormat >> 16) == 1))			{
				SENSORDB("OV9760 Sensor write calibration data: address = %x, value = %x \r\n",(pSetSensorCalData->ShadingData[i])>>16,(pSetSensorCalData->ShadingData[i])&0xFFFF);
				OV9760MIPI_write_cmos_sensor((pSetSensorCalData->ShadingData[i])>>16, (pSetSensorCalData->ShadingData[i])&0xFFFF);
			}
		}
	}
	return ERROR_NONE;
}

UINT32 OV9760SetMaxFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 frameRate) {
	kal_uint32 pclk;
	kal_int16 dummyLine;
	kal_uint16 lineLength,frameHeight;
		
	SENSORDB("OV9760SetMaxFramerateByScenario: scenarioId = %d, frame rate = %d\n",scenarioId,frameRate);
	switch (scenarioId) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			pclk = OV9760MIPI_PIXEL_CLK;
			lineLength = OV9760MIPI_PERIOD_PIXEL_NUMS;
			frameHeight = (10 * pclk)/frameRate/lineLength;
			dummyLine = frameHeight - OV9760MIPI_PERIOD_LINE_NUMS;
			if (dummyLine < 0){
				dummyLine = 0;
			}
			OV9760MIPI_set_dummy(0, dummyLine);			
			break;			
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:			
			pclk = OV9760MIPI_PIXEL_CLK;
			lineLength = OV9760MIPI_PERIOD_PIXEL_NUMS;
			frameHeight = (10 * pclk)/frameRate/lineLength;
			if(frameHeight < OV9760MIPI_PERIOD_LINE_NUMS)
				frameHeight = OV9760MIPI_PERIOD_LINE_NUMS;
			dummyLine = frameHeight - OV9760MIPI_PERIOD_LINE_NUMS;
			
			SENSORDB("OV9760SetMaxFramerateByScenario: scenarioId = %d, frame rate calculate = %d\n",((10 * pclk)/frameHeight/lineLength));
			OV9760MIPI_set_dummy(0, dummyLine);			
			break;		
        case MSDK_SCENARIO_ID_CAMERA_3D_PREVIEW: //added
            break;
        case MSDK_SCENARIO_ID_CAMERA_3D_VIDEO:
			break;
        case MSDK_SCENARIO_ID_CAMERA_3D_CAPTURE: //added   
			break;		
		default:
			break;
	}	
	return ERROR_NONE;
}
UINT32 OV9760GetDefaultFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 *pframeRate) {


	switch (scenarioId) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			 *pframeRate = 300;
			 break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
			 *pframeRate = 300;
			break;		
        case MSDK_SCENARIO_ID_CAMERA_3D_PREVIEW: //added
        case MSDK_SCENARIO_ID_CAMERA_3D_VIDEO:
        case MSDK_SCENARIO_ID_CAMERA_3D_CAPTURE: //added   
			 *pframeRate = 300;
			break;		
		default:
			break;
	}

	return ERROR_NONE;
}

UINT32 OV9760SetTestPatternMode(kal_bool bEnable)
{
    SENSORDB("[OV9760SetTestPatternMode] Test pattern enable:%d\n", bEnable);
	if(bEnable)
		OV9760MIPI_write_cmos_sensor(0x5a80,0x84);
	else
		OV9760MIPI_write_cmos_sensor(0x5a80,0x00);

    return ERROR_NONE;
}


UINT32 OV9760MIPIFeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId,
							 UINT8 *pFeaturePara,UINT32 *pFeatureParaLen)
{
	UINT16 *pFeatureReturnPara16=(UINT16 *) pFeaturePara;
	UINT16 *pFeatureData16=(UINT16 *) pFeaturePara;
	UINT32 *pFeatureReturnPara32=(UINT32 *) pFeaturePara;
	UINT32 *pFeatureData32=(UINT32 *) pFeaturePara;
	UINT32 OV976SensorRegNumber;
	int i;
	//PNVRAM_SENSOR_DATA_STRUCT pSensorDefaultData=(PNVRAM_SENSOR_DATA_STRUCT) pFeaturePara;
	//MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData=(MSDK_SENSOR_CONFIG_STRUCT *) pFeaturePara;
	MSDK_SENSOR_REG_INFO_STRUCT *pSensorRegData=(MSDK_SENSOR_REG_INFO_STRUCT *) pFeaturePara;
	
	PSET_SENSOR_CALIBRATION_DATA_STRUCT pSetSensorCalData=(PSET_SENSOR_CALIBRATION_DATA_STRUCT)pFeaturePara;
	switch (FeatureId)
	{
		case SENSOR_FEATURE_GET_RESOLUTION:
			*pFeatureReturnPara16++=OV9760MIPI_IMAGE_SENSOR_FULL_WIDTH;
			*pFeatureReturnPara16=OV9760MIPI_IMAGE_SENSOR_FULL_HEIGHT;
			*pFeatureParaLen=4;
		break;
		case SENSOR_FEATURE_GET_PERIOD:
			*pFeatureReturnPara16++= OV9760_sensor.line_length;
			*pFeatureReturnPara16=OV9760_sensor.frame_height;
			*pFeatureParaLen=4;
		break;
		case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
			*pFeatureReturnPara32 = OV9760MIPI_PIXEL_CLK;
			*pFeatureParaLen=4;
		break;
		case SENSOR_FEATURE_SET_ESHUTTER:
			OV9760_Write_Shutter((UINT16)*pFeatureData16);
			//need to imp!
		break;
		case SENSOR_FEATURE_SET_NIGHTMODE:
			//OV9760MIPI_night_mode((BOOL) *pFeatureData16);
		break;
		case SENSOR_FEATURE_SET_GAIN:
			OV9760_Set_Gain((UINT16)*pFeatureData16);
			//need to imp!
		break;
		case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
		case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
		case SENSOR_FEATURE_SET_REGISTER:
			OV9760MIPI_write_cmos_sensor(pSensorRegData->RegAddr, pSensorRegData->RegData);
		break;
		case SENSOR_FEATURE_GET_REGISTER:
			pSensorRegData->RegData = OV9760MIPI_read_cmos_sensor(pSensorRegData->RegAddr);
		break;
		case SENSOR_FEATURE_GET_CONFIG_PARA:
			memcpy(pFeaturePara, &OV9760_sensor.cfg_data, sizeof(OV9760_sensor.cfg_data));
			*pFeatureParaLen = sizeof(OV9760_sensor.cfg_data);
			break;
		break;
		case SENSOR_FEATURE_SET_CCT_REGISTER:
			OV976SensorRegNumber = OV9760_FACTORY_END_ADDR;
			for (i=0;i<OV976SensorRegNumber;i++)
            {
                spin_lock(&ov9740mipi_raw_drv_lock);
                OV9760_sensor.eng.cct[i].Addr=*pFeatureData32++;
                OV9760_sensor.eng.cct[i].Para=*pFeatureData32++;
			    spin_unlock(&ov9740mipi_raw_drv_lock);
            }
			//need to imp!
		break;
		case SENSOR_FEATURE_GET_CCT_REGISTER:
			if (*pFeatureParaLen >= sizeof(OV9760_sensor.eng.cct) + sizeof(kal_uint32))
			{
			  *((kal_uint32 *)pFeaturePara++) = sizeof(OV9760_sensor.eng.cct);
			  memcpy(pFeaturePara, &OV9760_sensor.eng.cct, sizeof(OV9760_sensor.eng.cct));
			}
			//need to imp!
		break;
		case SENSOR_FEATURE_SET_ENG_REGISTER:
			OV976SensorRegNumber = OV9760_ENGINEER_END;
			for (i=0;i<OV976SensorRegNumber;i++)
            {
                spin_lock(&ov9740mipi_raw_drv_lock);
                OV9760_sensor.eng.reg[i].Addr=*pFeatureData32++;
                OV9760_sensor.eng.reg[i].Para=*pFeatureData32++;
			    spin_unlock(&ov9740mipi_raw_drv_lock);
            }
			//need to imp!
		break;
		case SENSOR_FEATURE_GET_ENG_REGISTER:
			if (*pFeatureParaLen >= sizeof(OV9760_sensor.eng.reg) + sizeof(kal_uint32))
			{
			  *((kal_uint32 *)pFeaturePara++) = sizeof(OV9760_sensor.eng.reg);
			  memcpy(pFeaturePara, &OV9760_sensor.eng.reg, sizeof(OV9760_sensor.eng.reg));
			}
			//need to imp!
		break;
		case SENSOR_FEATURE_GET_REGISTER_DEFAULT:
			((PNVRAM_SENSOR_DATA_STRUCT)pFeaturePara)->Version = NVRAM_CAMERA_SENSOR_FILE_VERSION;
			((PNVRAM_SENSOR_DATA_STRUCT)pFeaturePara)->SensorId = OV9760MIPI_SENSOR_ID;
			memcpy(((PNVRAM_SENSOR_DATA_STRUCT)pFeaturePara)->SensorEngReg, &OV9760_sensor.eng.reg, sizeof(OV9760_sensor.eng.reg));
			memcpy(((PNVRAM_SENSOR_DATA_STRUCT)pFeaturePara)->SensorCCTReg, &OV9760_sensor.eng.cct, sizeof(OV9760_sensor.eng.cct));
			*pFeatureParaLen = sizeof(NVRAM_SENSOR_DATA_STRUCT);
			break;
		//need to imp!
		break;
		case SENSOR_FEATURE_CAMERA_PARA_TO_SENSOR:
			OV9760_camera_para_to_sensor();
			//need to imp!
		break;
		case SENSOR_FEATURE_SENSOR_TO_CAMERA_PARA:
			OV9760_sensor_to_camera_para();
		//need to imp!
		break;
		case SENSOR_FEATURE_GET_GROUP_INFO:
		break;
		case SENSOR_FEATURE_GET_ITEM_INFO:
			OV9760_get_sensor_item_info((MSDK_SENSOR_ITEM_INFO_STRUCT *)pFeaturePara);
		  	*pFeatureParaLen = sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
			//need to imp!
		break;
		case SENSOR_FEATURE_SET_ITEM_INFO:
			OV9760_set_sensor_item_info((MSDK_SENSOR_ITEM_INFO_STRUCT *)pFeaturePara);
		    *pFeatureParaLen = sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
			//need to imp!
		break;
		case SENSOR_FEATURE_GET_ENG_INFO:
			memcpy(pFeaturePara, &OV9760_sensor.eng_info, sizeof(OV9760_sensor.eng_info));
     		*pFeatureParaLen = sizeof(OV9760_sensor.eng_info);
			//need to imp!
		break;
		case SENSOR_FEATURE_GET_GROUP_COUNT:
            *pFeatureReturnPara32++=0;
            *pFeatureParaLen=4;	    
		break;
		case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
			// get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE
			// if EEPROM does not exist in camera module.
			*pFeatureReturnPara32=LENS_DRIVER_ID_DO_NOT_CARE;
			*pFeatureParaLen=4;
		break;
		case SENSOR_FEATURE_SET_VIDEO_MODE:
		    OV9760MIPIRAWSetVideoMode(*pFeatureData16);
		    break; 
		case SENSOR_FEATURE_CHECK_SENSOR_ID:
            OV9760MIPI_GetSensorID(pFeatureData32); 
            break; 	
	    case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
			OV9760SetAutoFlickerMode((BOOL)*pFeatureData16,*(pFeatureData16+1));
			//need to imp!
		break;
		case SENSOR_FEATURE_SET_CALIBRATION_DATA:	
			OV9760SetCalData(pSetSensorCalData);	
			//need to imp!		
			break;
		case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
			OV9760SetMaxFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32, *(pFeatureData32+1));
			//need to imp!
			break;
		case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
			OV9760GetDefaultFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32, (MUINT32 *)(*(pFeatureData32+1)));
			break;
			//need to imp!	
		case SENSOR_FEATURE_SET_TEST_PATTERN:
            OV9760SetTestPatternMode((BOOL)*pFeatureData16);
            break;
		case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:			
			*pFeatureReturnPara32=OV9760_TEST_PATTERN_CHECKSUM;			
			*pFeatureParaLen=4;			
			
			break;
		default:
			break;			
	}
	return ERROR_NONE;
}

SENSOR_FUNCTION_STRUCT	SensorFuncOV9760MIPI=
{
	OV9760MIPIOpen,
	OV9760MIPIGetInfo,
	OV9760MIPIGetResolution,
	OV9760MIPIFeatureControl,
	OV9760MIPIControl,
	OV9760MIPIClose
};

UINT32 OV9760MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc!=NULL)
		*pfFunc=&SensorFuncOV9760MIPI;
	return ERROR_NONE;
}	/* SensorInit() */
