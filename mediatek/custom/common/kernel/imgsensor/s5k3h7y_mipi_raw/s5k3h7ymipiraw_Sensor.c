/*******************************************************************************************/

 
/*******************************************************************************************/

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/xlog.h>
#include <asm/atomic.h>
#include <asm/system.h>

#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "s5k3h7ymipiraw_Sensor.h"
#include "s5k3h7ymipiraw_Camera_Sensor_para.h"
#include "s5k3h7ymipiraw_CameraCustomized.h"

#define S5K3H7Y_DEBUG
#ifdef S5K3H7Y_DEBUG
#define LOG_TAG (__FUNCTION__)
#define SENSORDB(fmt,arg...) xlog_printk(ANDROID_LOG_DEBUG , LOG_TAG, fmt, ##arg) 
#else
#define SENSORDB(fmt,arg...)
#endif

#define SENSOR_PCLK_PREVIEW  	28000*10000
#define SENSOR_PCLK_VIDEO  		SENSOR_PCLK_PREVIEW
#define SENSOR_PCLK_CAPTURE  	SENSOR_PCLK_PREVIEW
#define SENSOR_PCLK_ZSD  		SENSOR_PCLK_CAPTURE
#define S5K3H7_TEST_PATTERN_CHECKSUM (0xadc56499)

MSDK_SENSOR_CONFIG_STRUCT S5K3H7YSensorConfigData;

SENSOR_REG_STRUCT S5K3H7YSensorCCT[]=CAMERA_SENSOR_CCT_DEFAULT_VALUE;
SENSOR_REG_STRUCT S5K3H7YSensorReg[ENGINEER_END]=CAMERA_SENSOR_REG_DEFAULT_VALUE;

static DEFINE_SPINLOCK(s5k3h7ymipiraw_drv_lock);
static MSDK_SCENARIO_ID_ENUM s_S5K3H7YCurrentScenarioId = MSDK_SCENARIO_ID_CAMERA_PREVIEW;
static S5K3H7Y_PARA_STRUCT s5k3h7y;
static kal_uint16 s5k3h7y_slave_addr = S5K3H7YMIPI_WRITE_ID;

extern int iWriteRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u16 i2cId);
extern int iReadRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u8 * a_pRecvData, u16 a_sizeRecvData, u16 i2cId);

UINT32 S5K3H7YMIPISetMaxFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 frameRate);

kal_uint32 S5K3H7Y_FAC_SENSOR_REG;



inline kal_uint16 S5K3H7Y_read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte=0;
	
	char puSendCmd[2] = {(char)(addr >> 8) , (char)(addr & 0xFF) };
	iReadRegI2C(puSendCmd , 2, (u8*)&get_byte,2,s5k3h7y_slave_addr);
	
	return ((get_byte<<8)&0xff00)|((get_byte>>8)&0x00ff);
}



inline void S5K3H7Y_wordwrite_cmos_sensor(u16 addr, u32 para)
{
	char puSendCmd[4] = {(char)(addr >> 8) , (char)(addr & 0xFF) ,  (char)(para >> 8),	(char)(para & 0xFF) };
	iWriteRegI2C(puSendCmd , 4,s5k3h7y_slave_addr);
}



inline void S5K3H7Y_bytewrite_cmos_sensor(u16 addr, u32 para)
{
	char puSendCmd[4] = {(char)(addr >> 8) , (char)(addr & 0xFF)  ,	(char)(para & 0xFF) };
	iWriteRegI2C(puSendCmd , 3,s5k3h7y_slave_addr);
}



static inline kal_uint32 GetScenarioLinelength(void)
{
	kal_uint32 u4Linelength=S5K3H7Y_PV_PERIOD_PIXEL_NUMS;
	
	switch(s_S5K3H7YCurrentScenarioId)
	{
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			u4Linelength=S5K3H7Y_PV_PERIOD_PIXEL_NUMS;
		break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			u4Linelength=S5K3H7Y_VIDEO_PERIOD_PIXEL_NUMS;
		break;
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			u4Linelength=S5K3H7Y_FULL_PERIOD_PIXEL_NUMS;
		break;
		default:
		break;
	}
	
	return u4Linelength;
}



static inline kal_uint32 GetScenarioPixelClock(void)
{
	kal_uint32 Pixelcloclk = s5k3h7y.pvPclk;
	
	switch(s_S5K3H7YCurrentScenarioId)
	{
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			Pixelcloclk = s5k3h7y.pvPclk;
		break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			Pixelcloclk = s5k3h7y.m_vidPclk;
		break;
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			Pixelcloclk = s5k3h7y.capPclk;
		break;
		default:
		break;
	}
	
	return Pixelcloclk;		
}



static inline kal_uint32 GetScenarioFramelength(void)
{
	kal_uint32 u4Framelength=S5K3H7Y_PV_PERIOD_LINE_NUMS;
	
	switch(s_S5K3H7YCurrentScenarioId)
	{
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			u4Framelength=S5K3H7Y_PV_PERIOD_LINE_NUMS;
		break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			u4Framelength=S5K3H7Y_VIDEO_PERIOD_LINE_NUMS;
		break;
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			u4Framelength=S5K3H7Y_FULL_PERIOD_LINE_NUMS; 
		break;
		default:
		break;
	}
	
	return u4Framelength;
}



static inline void SetFramelength(kal_uint16 a_u2FrameLen)
{	
	S5K3H7Y_bytewrite_cmos_sensor(0x0104, 0x01);	
	S5K3H7Y_wordwrite_cmos_sensor(0x0340,a_u2FrameLen);
	S5K3H7Y_bytewrite_cmos_sensor(0x0104, 0x00);
}



void S5K3H7Y_write_shutter(kal_uint32 shutter)
{
	kal_uint16 frame_length = 0, line_length = 0, framerate = 0 , frame_length_min = 0;	
	kal_uint32 pixelclock = 0;

	#define SHUTTER_FRAMELENGTH_MARGIN 16
	
	frame_length_min = GetScenarioFramelength();

	if (shutter < 3)
		shutter = 3;

	frame_length = shutter+SHUTTER_FRAMELENGTH_MARGIN;

	if(frame_length < frame_length_min)
		frame_length = frame_length_min;	

	if(s5k3h7y.S5K3H7YAutoFlickerMode == KAL_TRUE)
	{
		line_length = GetScenarioLinelength();
		pixelclock = GetScenarioPixelClock();
		framerate = (10 * pixelclock) / (frame_length * line_length);
		  
		if(framerate > 290)
		{
		  	framerate = 290;
		  	frame_length = (10 * pixelclock) / (framerate * line_length);
		}
		else if(framerate > 147 && framerate < 152)
		{
		  	framerate = 147;
			frame_length = (10 * pixelclock) / (framerate * line_length);
		}
	}

	S5K3H7Y_bytewrite_cmos_sensor(0x0104, 0x01);
	S5K3H7Y_wordwrite_cmos_sensor(0x0340, frame_length);
 	S5K3H7Y_wordwrite_cmos_sensor(0x0202, shutter);
 	S5K3H7Y_bytewrite_cmos_sensor(0x0104, 0x00);

	SENSORDB("shutter=%d,frame_length=%d,framerate=%d\n",shutter,frame_length, framerate);
}  



void write_S5K3H7Y_gain(kal_uint16 gain)
{
	SENSORDB("gain=%d\n",gain);
	
	S5K3H7Y_bytewrite_cmos_sensor(0x0104, 0x01);
	S5K3H7Y_wordwrite_cmos_sensor(0x0204, gain);
	S5K3H7Y_bytewrite_cmos_sensor(0x0104, 0x00);
}



void S5K3H7Y_SetGain(UINT16 iGain)
{

	write_S5K3H7Y_gain(iGain);
}



kal_uint16 read_S5K3H7Y_gain(void)
{
	kal_uint16 read_gain=S5K3H7Y_read_cmos_sensor(0x0204);
	
	return read_gain;
}



kal_int32  S5K3H7Y_get_sensor_group_count(void)
{
    return GROUP_TOTAL_NUMS;
}



static void S5K3H7YInitSetting(void)
{
	SENSORDB("enter\n");

	S5K3H7Y_wordwrite_cmos_sensor(0x6010, 0x0001);	// Reset
	mdelay(1);
	S5K3H7Y_wordwrite_cmos_sensor(0x38FA, 0x0030);	
	S5K3H7Y_wordwrite_cmos_sensor(0x38FC, 0x0030);	
	S5K3H7Y_wordwrite_cmos_sensor(0x0086, 0x01FF);	
	S5K3H7Y_wordwrite_cmos_sensor(0x6218, 0xF1D0);	
	S5K3H7Y_wordwrite_cmos_sensor(0x6214, 0xF9F0);	
	S5K3H7Y_wordwrite_cmos_sensor(0x6226, 0x0001);	
	S5K3H7Y_wordwrite_cmos_sensor(0xB0C0, 0x000C);
	S5K3H7Y_wordwrite_cmos_sensor(0xF400, 0x0BBC);	
	S5K3H7Y_wordwrite_cmos_sensor(0xF616, 0x0004);	
	S5K3H7Y_wordwrite_cmos_sensor(0x6226, 0x0000);	
	S5K3H7Y_wordwrite_cmos_sensor(0x6218, 0xF9F0);	
	S5K3H7Y_wordwrite_cmos_sensor(0x6218, 0xF1D0);	
	S5K3H7Y_wordwrite_cmos_sensor(0x6214, 0xF9F0);	
	S5K3H7Y_wordwrite_cmos_sensor(0x6226, 0x0001);	
	S5K3H7Y_wordwrite_cmos_sensor(0xB0C0, 0x000C);
	S5K3H7Y_wordwrite_cmos_sensor(0x6226, 0x0000);	
	S5K3H7Y_wordwrite_cmos_sensor(0x6218, 0xF9F0);	
	S5K3H7Y_wordwrite_cmos_sensor(0x38FA, 0x0030);	
	S5K3H7Y_wordwrite_cmos_sensor(0x38FC, 0x0030);	
	S5K3H7Y_wordwrite_cmos_sensor(0x32CE, 0x0060);	
	S5K3H7Y_wordwrite_cmos_sensor(0x32D0, 0x0024);	
	S5K3H7Y_bytewrite_cmos_sensor(0x0114, 0x03);
	S5K3H7Y_wordwrite_cmos_sensor(0x030E, 0x00A5);	
	S5K3H7Y_wordwrite_cmos_sensor(0x0342, 0x0E68);	
	S5K3H7Y_wordwrite_cmos_sensor(0x0340, 0x09E2);
	S5K3H7Y_wordwrite_cmos_sensor(0x0200, 0x0618);	
	S5K3H7Y_wordwrite_cmos_sensor(0x0202, 0x09C2);	
	S5K3H7Y_wordwrite_cmos_sensor(0x0204, 0x0020);	
	S5K3H7Y_bytewrite_cmos_sensor(0x3011, 0x02);
	S5K3H7Y_bytewrite_cmos_sensor(0x0900, 0x01);
	S5K3H7Y_bytewrite_cmos_sensor(0x0901, 0x12);
	S5K3H7Y_wordwrite_cmos_sensor(0x034C, 0x0662);
	S5K3H7Y_wordwrite_cmos_sensor(0x034E, 0x04C8);
	S5K3H7Y_wordwrite_cmos_sensor(0x6028, 0xD000);
	S5K3H7Y_wordwrite_cmos_sensor(0x602A, 0x012A);
	S5K3H7Y_wordwrite_cmos_sensor(0x6F12, 0x0040);
	S5K3H7Y_wordwrite_cmos_sensor(0x6F12, 0x7077);
	S5K3H7Y_wordwrite_cmos_sensor(0x6F12, 0x7777);	
	S5K3H7Y_wordwrite_cmos_sensor(0x0100, 0x0100);	
	mdelay(2);
}



void S5K3H7YPreviewSetting(void)
{
	SENSORDB("enter\n");
	
	S5K3H7Y_bytewrite_cmos_sensor(0x0105, 0x04);
	S5K3H7Y_bytewrite_cmos_sensor(0x0104, 0x01);
	S5K3H7Y_bytewrite_cmos_sensor(0x0114, 0x03);
	S5K3H7Y_wordwrite_cmos_sensor(0x030E, 0x00A5);
	S5K3H7Y_wordwrite_cmos_sensor(0x0342, 0x0E68);
	S5K3H7Y_wordwrite_cmos_sensor(0x0340, 0x09E2);
	S5K3H7Y_wordwrite_cmos_sensor(0x0200, 0x0618);
	S5K3H7Y_wordwrite_cmos_sensor(0x0202, 0x09C2);
	S5K3H7Y_wordwrite_cmos_sensor(0x0204, 0x0020);
	S5K3H7Y_bytewrite_cmos_sensor(0x3011, 0x02);
	S5K3H7Y_bytewrite_cmos_sensor(0x0900, 0x01);
	S5K3H7Y_bytewrite_cmos_sensor(0x0901, 0x12);
	S5K3H7Y_wordwrite_cmos_sensor(0x0346, 0x0004);
	S5K3H7Y_wordwrite_cmos_sensor(0x034A, 0x0993);
	S5K3H7Y_wordwrite_cmos_sensor(0x034C, 0x0662);
	S5K3H7Y_wordwrite_cmos_sensor(0x034E, 0x04C8);
	S5K3H7Y_wordwrite_cmos_sensor(0x6004, 0x0000);
	S5K3H7Y_wordwrite_cmos_sensor(0x6028, 0xD000);
	S5K3H7Y_wordwrite_cmos_sensor(0x602A, 0x012A);
	S5K3H7Y_wordwrite_cmos_sensor(0x6F12, 0x0040);
	S5K3H7Y_wordwrite_cmos_sensor(0x6F12, 0x7077);
	S5K3H7Y_wordwrite_cmos_sensor(0x6F12, 0x7777);
	S5K3H7Y_bytewrite_cmos_sensor(0x0104, 0x00);
    mdelay(2);
}



void S5K3H7YCaptureSetting(void)
{
	SENSORDB("enter\n");

	S5K3H7Y_bytewrite_cmos_sensor(0x0105, 0x04);
	S5K3H7Y_bytewrite_cmos_sensor(0x0104, 0x01);
	S5K3H7Y_bytewrite_cmos_sensor(0x0114, 0x03);
	S5K3H7Y_wordwrite_cmos_sensor(0x030E, 0x00A5);
	S5K3H7Y_wordwrite_cmos_sensor(0x0342, 0x0E68);
	S5K3H7Y_wordwrite_cmos_sensor(0x0340, 0x09E2);
	S5K3H7Y_wordwrite_cmos_sensor(0x0200, 0x0618);
	S5K3H7Y_wordwrite_cmos_sensor(0x0202, 0x09C2);
	S5K3H7Y_wordwrite_cmos_sensor(0x0204, 0x0020);
	S5K3H7Y_wordwrite_cmos_sensor(0x0346, 0x0004);
	S5K3H7Y_wordwrite_cmos_sensor(0x034A, 0x0993);
	S5K3H7Y_wordwrite_cmos_sensor(0x034C, 0x0CC0);
	S5K3H7Y_wordwrite_cmos_sensor(0x034E, 0x0990);
	S5K3H7Y_wordwrite_cmos_sensor(0x0900, 0x0011);
	S5K3H7Y_wordwrite_cmos_sensor(0x0901, 0x0011);
	S5K3H7Y_wordwrite_cmos_sensor(0x3011, 0x0001);
	S5K3H7Y_wordwrite_cmos_sensor(0x6004, 0x0000);
	S5K3H7Y_wordwrite_cmos_sensor(0x6028, 0xD000);
	S5K3H7Y_wordwrite_cmos_sensor(0x602A, 0x012A);
	S5K3H7Y_wordwrite_cmos_sensor(0x6F12, 0x0040);
	S5K3H7Y_wordwrite_cmos_sensor(0x6F12, 0x7077);
	S5K3H7Y_wordwrite_cmos_sensor(0x6F12, 0x7777);
	S5K3H7Y_bytewrite_cmos_sensor(0x0104, 0x00);
	mdelay(2);
}



void S5K3H7YVideoSetting(void)
{
	SENSORDB("enter\n");

	S5K3H7Y_bytewrite_cmos_sensor(0x0105, 0x04);	 
	S5K3H7Y_bytewrite_cmos_sensor(0x0104, 0x01);	 
	S5K3H7Y_bytewrite_cmos_sensor(0x0114, 0x03);	 
	S5K3H7Y_wordwrite_cmos_sensor(0x030E, 0x00A5);	 
	S5K3H7Y_wordwrite_cmos_sensor(0x0342, 0x0E68);	 
	S5K3H7Y_wordwrite_cmos_sensor(0x0340, 0x09E2);	 
	S5K3H7Y_wordwrite_cmos_sensor(0x0200, 0x0618);	 
	S5K3H7Y_wordwrite_cmos_sensor(0x0202, 0x09C2);	 
	S5K3H7Y_wordwrite_cmos_sensor(0x0204, 0x0020);	 
	S5K3H7Y_wordwrite_cmos_sensor(0x0346, 0x0136);	 
	S5K3H7Y_wordwrite_cmos_sensor(0x034A, 0x0861);	 
	S5K3H7Y_wordwrite_cmos_sensor(0x034C, 0x0CC0);	 
	S5K3H7Y_wordwrite_cmos_sensor(0x034E, 0x072C);	 
	S5K3H7Y_wordwrite_cmos_sensor(0x0900, 0x0011);   
	S5K3H7Y_wordwrite_cmos_sensor(0x0901, 0x0011);   
	S5K3H7Y_wordwrite_cmos_sensor(0x3011, 0x0001);   
	S5K3H7Y_wordwrite_cmos_sensor(0x6004, 0x0000);   
	S5K3H7Y_wordwrite_cmos_sensor(0x6028, 0xD000);   
	S5K3H7Y_wordwrite_cmos_sensor(0x602A, 0x012A);   
	S5K3H7Y_wordwrite_cmos_sensor(0x6F12, 0x0040);	 
	S5K3H7Y_wordwrite_cmos_sensor(0x6F12, 0x7077);	 
	S5K3H7Y_wordwrite_cmos_sensor(0x6F12, 0x7777);	 
	S5K3H7Y_bytewrite_cmos_sensor(0x0104, 0x00);	 
	mdelay(2);
}



UINT32 S5K3H7YOpen(void)
{
	volatile signed int i,j;
	kal_uint16 sensor_id = 0;

	SENSORDB("enter\n");

	for(j=0;j<2;j++)
	{
		SENSORDB("Read sensor ID=0x%x\n",sensor_id);

		if(S5K3H7Y_SENSOR_ID == sensor_id)
			break;
		
		switch(j)
		{
			case 0:
				s5k3h7y_slave_addr = S5K3H7YMIPI_WRITE_ID2;
				break;
			case 1:
				s5k3h7y_slave_addr = S5K3H7YMIPI_WRITE_ID;
				break;
			default:
				break;
		}
		
		SENSORDB("s5k3h7y_slave_addr =0x%x\n", s5k3h7y_slave_addr);
		
		for(i=3;i>0;i--)
		{
			sensor_id = S5K3H7Y_read_cmos_sensor(0x0000);

			SENSORDB("Read sensor ID=0x%x\n",sensor_id);

			if(S5K3H7Y_SENSOR_ID == sensor_id)
				break;
		}
	}
	
	if(S5K3H7Y_SENSOR_ID != sensor_id)
		return ERROR_SENSOR_CONNECT_FAIL;

	S5K3H7YInitSetting();

    return ERROR_NONE;
}



UINT32 S5K3H7YGetSensorID(UINT32 *sensorID)
{
	int i=0,j =0;

	SENSORDB("enter\n");

	for(j=0;j<2;j++)
	{
		SENSORDB("Read sensor ID=0x%x\n",*sensorID);

		if(S5K3H7Y_SENSOR_ID == *sensorID)
			break;
		
		switch(j) 
		{
			case 0:
				s5k3h7y_slave_addr = S5K3H7YMIPI_WRITE_ID2;
				break;
			case 1:
				s5k3h7y_slave_addr = S5K3H7YMIPI_WRITE_ID;
				break;
			default:
				break;
		}
		
		SENSORDB("s5k3h7y_slave_addr =0x%x\n",s5k3h7y_slave_addr);

		for(i=3;i>0;i--)
		{
			S5K3H7Y_wordwrite_cmos_sensor(0x6010,0x0001);
			
			mdelay(1);

			*sensorID = S5K3H7Y_read_cmos_sensor(0x0000);

			SENSORDB("Read sensor ID=0x%x\n",*sensorID);

			if(S5K3H7Y_SENSOR_ID == *sensorID)
				break;
		}
	}


	if (*sensorID != S5K3H7Y_SENSOR_ID)
	{
        *sensorID = 0xFFFFFFFF;
		
        return ERROR_SENSOR_CONNECT_FAIL;
    }

	spin_lock(&s5k3h7ymipiraw_drv_lock);
	s5k3h7y.sensorMode = SENSOR_MODE_INIT;
	s5k3h7y.S5K3H7YAutoFlickerMode = KAL_FALSE;
	s5k3h7y.pvPclk = SENSOR_PCLK_PREVIEW; 
	s5k3h7y.m_vidPclk= SENSOR_PCLK_VIDEO;
	s5k3h7y.capPclk= SENSOR_PCLK_CAPTURE;
	s_S5K3H7YCurrentScenarioId = MSDK_SCENARIO_ID_CAMERA_PREVIEW;
	spin_unlock(&s5k3h7ymipiraw_drv_lock);

    return ERROR_NONE;
}



void S5K3H7Y_SetShutter(kal_uint32 iShutter)
{
   S5K3H7Y_write_shutter(iShutter);
}



UINT32 S5K3H7Y_read_shutter(void)
{
	return S5K3H7Y_read_cmos_sensor(0x0202);
}



UINT32 S5K3H7YClose(void)
{
    return ERROR_NONE;
}



void S5K3H7YSetFlipMirror(kal_int32 imgMirror)
{
	SENSORDB("imgMirror=%d\n",imgMirror);
	
	spin_lock(&s5k3h7ymipiraw_drv_lock);
	s5k3h7y.imgMirror = imgMirror; 
	spin_unlock(&s5k3h7ymipiraw_drv_lock);

    switch (imgMirror)
    {
		case IMAGE_H_MIRROR:
			S5K3H7Y_bytewrite_cmos_sensor(0x0101, 0x02); 
            break;
        case IMAGE_NORMAL:
			S5K3H7Y_bytewrite_cmos_sensor(0x0101, 0x03); 
            break;
        case IMAGE_HV_MIRROR:
			S5K3H7Y_bytewrite_cmos_sensor(0x0101, 0x00); 
            break;
        case IMAGE_V_MIRROR:
			S5K3H7Y_bytewrite_cmos_sensor(0x0101, 0x01);
            break;
    }
}



UINT32 S5K3H7YPreview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window, MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	SENSORDB("enter\n");

	spin_lock(&s5k3h7ymipiraw_drv_lock);
	s5k3h7y.sensorMode = SENSOR_MODE_PREVIEW; 
	spin_unlock(&s5k3h7ymipiraw_drv_lock);

	S5K3H7YPreviewSetting();
	S5K3H7YSetFlipMirror(sensor_config_data->SensorImageMirror);

    return ERROR_NONE;
}



UINT32 S5K3H7YVideo(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window, MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	SENSORDB("enter\n");

	spin_lock(&s5k3h7ymipiraw_drv_lock);
	s5k3h7y.sensorMode = SENSOR_MODE_VIDEO; 
	spin_unlock(&s5k3h7ymipiraw_drv_lock);

	S5K3H7YVideoSetting();
	S5K3H7YSetFlipMirror(sensor_config_data->SensorImageMirror);

    return ERROR_NONE;
}



UINT32 S5K3H7YZSDPreview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window, MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	SENSORDB("enter\n");

	if((s5k3h7y.sensorMode = SENSOR_MODE_INIT) ||(s5k3h7y.sensorMode = SENSOR_MODE_PREVIEW) || (s5k3h7y.sensorMode = SENSOR_MODE_VIDEO))
		S5K3H7YCaptureSetting();

	spin_lock(&s5k3h7ymipiraw_drv_lock);
	s5k3h7y.sensorMode = SENSOR_MODE_ZSD_PREVIEW;
	spin_unlock(&s5k3h7ymipiraw_drv_lock);
	
	S5K3H7YSetFlipMirror(sensor_config_data->SensorImageMirror);

    return ERROR_NONE;
}



UINT32 S5K3H7YCapture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window, MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	SENSORDB("sensorMode=%d\n",s5k3h7y.sensorMode);

	if((s5k3h7y.sensorMode = SENSOR_MODE_INIT) ||(s5k3h7y.sensorMode = SENSOR_MODE_PREVIEW) || (s5k3h7y.sensorMode = SENSOR_MODE_VIDEO))
		S5K3H7YCaptureSetting();

	spin_lock(&s5k3h7ymipiraw_drv_lock);
	s5k3h7y.sensorMode = SENSOR_MODE_CAPTURE;
	spin_unlock(&s5k3h7ymipiraw_drv_lock);

	S5K3H7YSetFlipMirror(sensor_config_data->SensorImageMirror);

    return ERROR_NONE;
}	



UINT32 S5K3H7YGetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution)
{
    SENSORDB("enter\n");
	
	pSensorResolution->SensorPreviewWidth	= 	S5K3H7Y_IMAGE_SENSOR_PV_WIDTH;
	pSensorResolution->SensorPreviewHeight	= 	S5K3H7Y_IMAGE_SENSOR_PV_HEIGHT;
	pSensorResolution->SensorVideoWidth		=	S5K3H7Y_IMAGE_SENSOR_VIDEO_WIDTH;
	pSensorResolution->SensorVideoHeight 	=	S5K3H7Y_IMAGE_SENSOR_VIDEO_HEIGHT;
	pSensorResolution->SensorFullWidth		= 	S5K3H7Y_IMAGE_SENSOR_FULL_WIDTH;
	pSensorResolution->SensorFullHeight		= 	S5K3H7Y_IMAGE_SENSOR_FULL_HEIGHT;
	
    return ERROR_NONE;
}



UINT32 S5K3H7YGetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_INFO_STRUCT *pSensorInfo, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
	switch(s_S5K3H7YCurrentScenarioId)
	{
    	case MSDK_SCENARIO_ID_CAMERA_ZSD:
			pSensorInfo->SensorPreviewResolutionX= S5K3H7Y_IMAGE_SENSOR_FULL_WIDTH;
			pSensorInfo->SensorPreviewResolutionY= S5K3H7Y_IMAGE_SENSOR_FULL_HEIGHT;
			break;
		default:
			pSensorInfo->SensorPreviewResolutionX= S5K3H7Y_IMAGE_SENSOR_PV_WIDTH;
			pSensorInfo->SensorPreviewResolutionY= S5K3H7Y_IMAGE_SENSOR_PV_HEIGHT;
			break;
	}

	pSensorInfo->SensorFullResolutionX= S5K3H7Y_IMAGE_SENSOR_FULL_WIDTH;
    pSensorInfo->SensorFullResolutionY= S5K3H7Y_IMAGE_SENSOR_FULL_HEIGHT;

	SENSORDB("SensorImageMirror=%d\n", pSensorConfigData->SensorImageMirror);

	switch(s5k3h7y.imgMirror)
	{
		case IMAGE_NORMAL:
   			pSensorInfo->SensorOutputDataFormat= SENSOR_OUTPUT_FORMAT_RAW_R;
		break;
		case IMAGE_H_MIRROR:
   			pSensorInfo->SensorOutputDataFormat= SENSOR_OUTPUT_FORMAT_RAW_Gb;
		break;
		case IMAGE_V_MIRROR:
   			pSensorInfo->SensorOutputDataFormat= SENSOR_OUTPUT_FORMAT_RAW_Gr;
		break;
		case IMAGE_HV_MIRROR:
   			pSensorInfo->SensorOutputDataFormat= SENSOR_OUTPUT_FORMAT_RAW_B;
		break;
		default:
			pSensorInfo->SensorOutputDataFormat= SENSOR_OUTPUT_FORMAT_RAW_R;
	}
	
    pSensorInfo->SensorClockPolarity =SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorClockFallingPolarity=SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;

    pSensorInfo->SensroInterfaceType=SENSOR_INTERFACE_TYPE_MIPI;

    pSensorInfo->CaptureDelayFrame = 3;
    pSensorInfo->PreviewDelayFrame = 3;
    pSensorInfo->VideoDelayFrame = 2;

    pSensorInfo->SensorDrivingCurrent = ISP_DRIVING_8MA;
    pSensorInfo->AEShutDelayFrame = 0;
    pSensorInfo->AESensorGainDelayFrame = 0;
    pSensorInfo->AEISPGainDelayFrame = 2;

	pSensorInfo->SensorClockFreq=24;
	pSensorInfo->SensorClockRisingCount= 0;
	
	pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;
	
	pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 0x20;
	pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	pSensorInfo->SensorPacketECCOrder = 1;

    switch (ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            pSensorInfo->SensorGrabStartX = S5K3H7Y_PV_X_START;
            pSensorInfo->SensorGrabStartY = S5K3H7Y_PV_Y_START;
		break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			pSensorInfo->SensorGrabStartX = S5K3H7Y_VIDEO_X_START;
			pSensorInfo->SensorGrabStartY = S5K3H7Y_VIDEO_Y_START;
        break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
            pSensorInfo->SensorGrabStartX = S5K3H7Y_FULL_X_START;	
            pSensorInfo->SensorGrabStartY = S5K3H7Y_FULL_Y_START;	
        break;
        default:
            pSensorInfo->SensorGrabStartX = S5K3H7Y_PV_X_START;
            pSensorInfo->SensorGrabStartY = S5K3H7Y_PV_Y_START;
            break;
    }

    memcpy(pSensorConfigData, &S5K3H7YSensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));

    return ERROR_NONE;
} 



UINT32 S5K3H7YControl(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
	spin_lock(&s5k3h7ymipiraw_drv_lock);
	s_S5K3H7YCurrentScenarioId = ScenarioId;
	spin_unlock(&s5k3h7ymipiraw_drv_lock);

	SENSORDB("s_S5K3H7YCurrentScenarioId=%d\n",s_S5K3H7YCurrentScenarioId);

    switch (ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            S5K3H7YPreview(pImageWindow, pSensorConfigData);
        break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			S5K3H7YVideo(pImageWindow, pSensorConfigData);
		break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			S5K3H7YCapture(pImageWindow, pSensorConfigData);
        break;
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
			S5K3H7YZSDPreview(pImageWindow, pSensorConfigData);
		break;
        default:
            return ERROR_INVALID_SCENARIO_ID;
    }
	
    return ERROR_NONE;
} 



UINT32 S5K3H7YSetVideoMode(UINT16 u2FrameRate)
{	
    SENSORDB("u2FrameRate=%d,sensorMode=%d\n", u2FrameRate,s5k3h7y.sensorMode);

	if(0==u2FrameRate || u2FrameRate >30 || u2FrameRate <5)
	{
	    return ERROR_NONE;
	}

	S5K3H7YMIPISetMaxFramerateByScenario(MSDK_SCENARIO_ID_VIDEO_PREVIEW,u2FrameRate*10);

	return ERROR_NONE;
}



static void S5K3H7YSetMaxFrameRate(UINT16 u2FrameRate)
{
	kal_uint16 FrameHeight;
		
	SENSORDB("[S5K4H5YX] [S5K4H5YXMIPISetMaxFrameRate] u2FrameRate=%d\n",u2FrameRate);

	if(SENSOR_MODE_PREVIEW == s5k3h7y.sensorMode)
	{
		FrameHeight= (10 * s5k3h7y.pvPclk) / u2FrameRate / S5K3H7Y_PV_PERIOD_PIXEL_NUMS;
		FrameHeight = (FrameHeight > S5K3H7Y_PV_PERIOD_LINE_NUMS) ? FrameHeight : S5K3H7Y_PV_PERIOD_LINE_NUMS;
	}
	else if(SENSOR_MODE_CAPTURE== s5k3h7y.sensorMode || SENSOR_MODE_ZSD_PREVIEW == s5k3h7y.sensorMode)
	{
		FrameHeight= (10 * s5k3h7y.capPclk) / u2FrameRate / S5K3H7Y_FULL_PERIOD_PIXEL_NUMS;
		FrameHeight = (FrameHeight > S5K3H7Y_FULL_PERIOD_LINE_NUMS) ? FrameHeight : S5K3H7Y_FULL_PERIOD_LINE_NUMS;
	}
	else
	{
		FrameHeight = (10 * s5k3h7y.m_vidPclk) / u2FrameRate / S5K3H7Y_VIDEO_PERIOD_PIXEL_NUMS;
		FrameHeight = (FrameHeight > S5K3H7Y_VIDEO_PERIOD_LINE_NUMS) ? FrameHeight : S5K3H7Y_VIDEO_PERIOD_LINE_NUMS;
	}
	
	SENSORDB("[S5K4H5YX] [S5K4H5YXMIPISetMaxFrameRate] FrameHeight=%d",FrameHeight);

	SetFramelength(FrameHeight);
}



UINT32 S5K3H7YSetAutoFlickerMode(kal_bool bEnable, UINT16 u2FrameRate)
{
	if(bEnable) 
	{
		SENSORDB("[S5K4H5YX] [S5K4H5YXSetAutoFlickerMode] enable\n");
		
		spin_lock(&s5k3h7ymipiraw_drv_lock);
		s5k3h7y.S5K3H7YAutoFlickerMode = KAL_TRUE;
		spin_unlock(&s5k3h7ymipiraw_drv_lock);

		if(u2FrameRate == 300)
			S5K3H7YSetMaxFrameRate(296);
		else if(u2FrameRate == 150)
			S5K3H7YSetMaxFrameRate(148);
    } 
	else 
	{
    	SENSORDB("[S5K4H5YX] [S5K4H5YXSetAutoFlickerMode] disable\n");
		
    	spin_lock(&s5k3h7ymipiraw_drv_lock);
        s5k3h7y.S5K3H7YAutoFlickerMode = KAL_FALSE;
		spin_unlock(&s5k3h7ymipiraw_drv_lock);
    }
    return ERROR_NONE;
}



UINT32 S5K3H7YSetTestPatternMode(kal_bool bEnable)
{
    SENSORDB("bEnable=%d\n", bEnable);
	
    if(bEnable)
        S5K3H7Y_wordwrite_cmos_sensor(0x0600,0x0002);
    else
        S5K3H7Y_wordwrite_cmos_sensor(0x0600,0x0000);
    
    return ERROR_NONE;
}



UINT32 S5K3H7YMIPISetMaxFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 frameRate)
{
	kal_uint16 frameLength=0;

	SENSORDB("scenarioId=%d,frameRate=%d\n",scenarioId,frameRate);
	
	switch (scenarioId)
	{
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			frameLength = (s5k3h7y.pvPclk)/frameRate*10/S5K3H7Y_PV_PERIOD_PIXEL_NUMS;
			frameLength = (frameLength>S5K3H7Y_PV_PERIOD_LINE_NUMS)?(frameLength):(S5K3H7Y_PV_PERIOD_LINE_NUMS);
		break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			frameLength = (s5k3h7y.m_vidPclk)/frameRate*10/S5K3H7Y_VIDEO_PERIOD_PIXEL_NUMS;
			frameLength = (frameLength>S5K3H7Y_VIDEO_PERIOD_LINE_NUMS)?(frameLength):(S5K3H7Y_VIDEO_PERIOD_LINE_NUMS);
		break;
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			frameLength = (s5k3h7y.capPclk)/frameRate*10/S5K3H7Y_FULL_PERIOD_PIXEL_NUMS;
			frameLength = (frameLength>S5K3H7Y_FULL_PERIOD_LINE_NUMS)?(frameLength):(S5K3H7Y_FULL_PERIOD_LINE_NUMS);
		break;
        case MSDK_SCENARIO_ID_CAMERA_3D_PREVIEW:
        case MSDK_SCENARIO_ID_CAMERA_3D_VIDEO:
        case MSDK_SCENARIO_ID_CAMERA_3D_CAPTURE:
		break;
		default:
			frameLength = S5K3H7Y_PV_PERIOD_LINE_NUMS;
		break;
	}
	
	S5K3H7Y_write_shutter(frameLength - 16);
	SetFramelength(frameLength);
	
	return ERROR_NONE;
}



UINT32 S5K3H7YMIPIGetDefaultFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 *pframeRate)
{

	switch (scenarioId) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			 *pframeRate = 300;
			 break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
			 *pframeRate = 300;
			break;
        case MSDK_SCENARIO_ID_CAMERA_3D_PREVIEW: 
        case MSDK_SCENARIO_ID_CAMERA_3D_VIDEO:
        case MSDK_SCENARIO_ID_CAMERA_3D_CAPTURE:
			 *pframeRate = 300;
			break;
		default:
			break;
	}

	return ERROR_NONE;
}



UINT32 S5K3H7YMIPIGetTemperature(UINT32 *temperature)
{
	*temperature = 0;
    return ERROR_NONE;
}



UINT32 S5K3H7YFeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId, UINT8 *pFeaturePara, UINT32 *pFeatureParaLen)
{
    UINT16 *pFeatureReturnPara16=(UINT16 *) pFeaturePara, *pFeatureData16=(UINT16 *) pFeaturePara;
    UINT32 *pFeatureReturnPara32=(UINT32 *) pFeaturePara, *pFeatureData32=(UINT32 *) pFeaturePara, SensorRegNumber, i;
    PNVRAM_SENSOR_DATA_STRUCT pSensorDefaultData=(PNVRAM_SENSOR_DATA_STRUCT) pFeaturePara;
    MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData=(MSDK_SENSOR_CONFIG_STRUCT *) pFeaturePara;
    MSDK_SENSOR_REG_INFO_STRUCT *pSensorRegData=(MSDK_SENSOR_REG_INFO_STRUCT *) pFeaturePara;
    //MSDK_SENSOR_GROUP_INFO_STRUCT *pSensorGroupInfo=(MSDK_SENSOR_GROUP_INFO_STRUCT *) pFeaturePara;
    //MSDK_SENSOR_ITEM_INFO_STRUCT *pSensorItemInfo=(MSDK_SENSOR_ITEM_INFO_STRUCT *) pFeaturePara;
    MSDK_SENSOR_ENG_INFO_STRUCT	*pSensorEngInfo=(MSDK_SENSOR_ENG_INFO_STRUCT *) pFeaturePara;

	SENSORDB("FeatureId=%d\n",FeatureId);
	
    switch (FeatureId)
    {
        case SENSOR_FEATURE_GET_RESOLUTION:
            *pFeatureReturnPara16++= S5K3H7Y_IMAGE_SENSOR_FULL_WIDTH;
            *pFeatureReturnPara16= S5K3H7Y_IMAGE_SENSOR_FULL_HEIGHT;
            *pFeatureParaLen=4;
            break;
        case SENSOR_FEATURE_GET_PERIOD:
			*pFeatureReturnPara16++= GetScenarioLinelength();
			*pFeatureReturnPara16= GetScenarioFramelength();
			*pFeatureParaLen=4;
			break;
        case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
    	 	*pFeatureReturnPara32 = s5k3h7y.pvPclk;
			SENSORDB("sensor clock=%d\n",*pFeatureReturnPara32);
    	 	*pFeatureParaLen=4;
 			 break;
        case SENSOR_FEATURE_SET_ESHUTTER:
            S5K3H7Y_SetShutter(*pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_GAIN:
            S5K3H7Y_SetGain((UINT16) *pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_REGISTER:
            S5K3H7Y_wordwrite_cmos_sensor(pSensorRegData->RegAddr, pSensorRegData->RegData);
            break;
        case SENSOR_FEATURE_GET_REGISTER:
            pSensorRegData->RegData = S5K3H7Y_read_cmos_sensor(pSensorRegData->RegAddr);
            break;
        case SENSOR_FEATURE_SET_CCT_REGISTER:
            SensorRegNumber=FACTORY_END_ADDR;
            for (i=0;i<SensorRegNumber;i++)
            {
            	spin_lock(&s5k3h7ymipiraw_drv_lock);
                S5K3H7YSensorCCT[i].Addr=*pFeatureData32++;
                S5K3H7YSensorCCT[i].Para=*pFeatureData32++;
				spin_unlock(&s5k3h7ymipiraw_drv_lock);
            }
            break;
        case SENSOR_FEATURE_GET_CCT_REGISTER:
            SensorRegNumber=FACTORY_END_ADDR;
            if (*pFeatureParaLen<(SensorRegNumber*sizeof(SENSOR_REG_STRUCT)+4))
                return ERROR_INVALID_PARA;
            *pFeatureData32++=SensorRegNumber;
            for (i=0;i<SensorRegNumber;i++)
            {
                *pFeatureData32++=S5K3H7YSensorCCT[i].Addr;
                *pFeatureData32++=S5K3H7YSensorCCT[i].Para;
            }
            break;
        case SENSOR_FEATURE_SET_ENG_REGISTER:
            SensorRegNumber=ENGINEER_END;
            for (i=0;i<SensorRegNumber;i++)
            {
            	spin_lock(&s5k3h7ymipiraw_drv_lock);
                S5K3H7YSensorReg[i].Addr=*pFeatureData32++;
                S5K3H7YSensorReg[i].Para=*pFeatureData32++;
				spin_unlock(&s5k3h7ymipiraw_drv_lock);
            }
            break;
        case SENSOR_FEATURE_GET_ENG_REGISTER:
            SensorRegNumber=ENGINEER_END;
            if (*pFeatureParaLen<(SensorRegNumber*sizeof(SENSOR_REG_STRUCT)+4))
                return ERROR_INVALID_PARA;
            *pFeatureData32++=SensorRegNumber;
            for (i=0;i<SensorRegNumber;i++)
            {
                *pFeatureData32++=S5K3H7YSensorReg[i].Addr;
                *pFeatureData32++=S5K3H7YSensorReg[i].Para;
            }
            break;
        case SENSOR_FEATURE_GET_REGISTER_DEFAULT:
            if (*pFeatureParaLen>=sizeof(NVRAM_SENSOR_DATA_STRUCT))
            {
                pSensorDefaultData->Version=NVRAM_CAMERA_SENSOR_FILE_VERSION;
                pSensorDefaultData->SensorId=S5K3H7Y_SENSOR_ID;
                memcpy(pSensorDefaultData->SensorEngReg, S5K3H7YSensorReg, sizeof(SENSOR_REG_STRUCT)*ENGINEER_END);
                memcpy(pSensorDefaultData->SensorCCTReg, S5K3H7YSensorCCT, sizeof(SENSOR_REG_STRUCT)*FACTORY_END_ADDR);
            }
            else
                return ERROR_INVALID_PARA;
            *pFeatureParaLen=sizeof(NVRAM_SENSOR_DATA_STRUCT);
            break;
        case SENSOR_FEATURE_GET_CONFIG_PARA:
            memcpy(pSensorConfigData, &S5K3H7YSensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));
            *pFeatureParaLen=sizeof(MSDK_SENSOR_CONFIG_STRUCT);
            break;
        case SENSOR_FEATURE_GET_GROUP_COUNT:
            *pFeatureReturnPara32++=S5K3H7Y_get_sensor_group_count();
            *pFeatureParaLen=4;
            break;
        case SENSOR_FEATURE_GET_GROUP_INFO:
            *pFeatureParaLen=sizeof(MSDK_SENSOR_GROUP_INFO_STRUCT);
            break;
        case SENSOR_FEATURE_GET_ITEM_INFO:
            *pFeatureParaLen=sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
            break;
        case SENSOR_FEATURE_SET_ITEM_INFO:
            *pFeatureParaLen=sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
            break;
        case SENSOR_FEATURE_GET_ENG_INFO:
            pSensorEngInfo->SensorId = 129;
            pSensorEngInfo->SensorType = CMOS_SENSOR;
            pSensorEngInfo->SensorOutputDataFormat=SENSOR_OUTPUT_FORMAT_RAW_B;
            *pFeatureParaLen=sizeof(MSDK_SENSOR_ENG_INFO_STRUCT);
            break;
        case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
            *pFeatureReturnPara32=LENS_DRIVER_ID_DO_NOT_CARE;
            *pFeatureParaLen=4;
            break;
        case SENSOR_FEATURE_SET_VIDEO_MODE:
            S5K3H7YSetVideoMode(*pFeatureData16);
            break;
        case SENSOR_FEATURE_CHECK_SENSOR_ID:
            S5K3H7YGetSensorID(pFeatureReturnPara32);
            break;
        case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
            S5K3H7YSetAutoFlickerMode((BOOL)*pFeatureData16, *(pFeatureData16+1));
	        break;
        case SENSOR_FEATURE_SET_TEST_PATTERN:
            S5K3H7YSetTestPatternMode((BOOL)*pFeatureData16);
            break;
		case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
			S5K3H7YMIPISetMaxFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32, *(pFeatureData32+1));
			break;
		case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
			S5K3H7YMIPIGetDefaultFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32, (MUINT32 *)(*(pFeatureData32+1)));
			break;
		case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
            *pFeatureReturnPara32= S5K3H7_TEST_PATTERN_CHECKSUM;
			*pFeatureParaLen=4;
			break;
		case SENSOR_FEATURE_GET_SENSOR_CURRENT_TEMPERATURE:
			S5K3H7YMIPIGetTemperature(pFeatureReturnPara32);
			*pFeatureParaLen=4;
			break;
		case SENSOR_FEATURE_SET_NIGHTMODE:
	 	case SENSOR_FEATURE_SET_FLASHLIGHT:
		case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		case SENSOR_FEATURE_CAMERA_PARA_TO_SENSOR:
        case SENSOR_FEATURE_SENSOR_TO_CAMERA_PARA:
		case SENSOR_FEATURE_INITIALIZE_AF:
        case SENSOR_FEATURE_CONSTANT_AF:
        case SENSOR_FEATURE_MOVE_FOCUS_LENS:
            break;
        default:
            break;
    }
    return ERROR_NONE;
}



SENSOR_FUNCTION_STRUCT	SensorFuncS5K3H7Y=
{
    S5K3H7YOpen,
    S5K3H7YGetInfo,
    S5K3H7YGetResolution,
    S5K3H7YFeatureControl,
    S5K3H7YControl,
    S5K3H7YClose
};



UINT32 S5K3H7Y_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
    if (pfFunc!=NULL)
        *pfFunc=&SensorFuncS5K3H7Y;

    return ERROR_NONE;
}



