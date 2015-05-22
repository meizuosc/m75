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

#include "s5k5e2yamipiraw_Sensor.h"
#include "s5k5e2yamipiraw_Camera_Sensor_para.h"
#include "s5k5e2yamipiraw_CameraCustomized.h"
static DEFINE_SPINLOCK(s5k5e2yamipiraw_drv_lock);


//#define S5K5E2YA_DEBUG_SOFIA

#define mDELAY(ms)  mdelay(ms)
#define Sleep(ms) mdelay(ms)

#define S5K5E2YA_DEBUG
#ifdef S5K5E2YA_DEBUG
#define LOG_TAG (__FUNCTION__)
#define SENSORDB(fmt,arg...) xlog_printk(ANDROID_LOG_DEBUG , LOG_TAG, fmt, ##arg)  							//printk(LOG_TAG "%s: " fmt "\n", __FUNCTION__ ,##arg)
#else
#define SENSORDB(fmt,arg...)  
#endif


#if 0
#define S5K5E2YA_DEBUG
#ifdef S5K5E2YA_DEBUG
	//#define S5K5E2YADB(fmt, arg...) printk( "[S5K5E2YARaw] "  fmt, ##arg)
	#define S5K5E2YADB(fmt, arg...) xlog_printk(ANDROID_LOG_DEBUG, "[S5K5E2YARaw]" fmt, #arg)
#else
	#define S5K5E2YADB(x,...)
#endif

#ifdef S5K5E2YA_DEBUG_SOFIA
	#define S5K5E2YADBSOFIA(fmt, arg...) printk( "[S5K5E2YARaw] "  fmt, ##arg)
#else
	#define S5K5E2YADBSOFIA(x,...)
#endif
#endif

#define S5K5E2YA_TEST_PATTERN_CHECKSUM (0x87e356d9)


MSDK_SENSOR_CONFIG_STRUCT S5K5E2YASensorConfigData;

kal_uint32 S5K5E2YA_FAC_SENSOR_REG;
static MSDK_SCENARIO_ID_ENUM s_S5K5E2YACurrentScenarioId = MSDK_SCENARIO_ID_CAMERA_PREVIEW;

/* FIXME: old factors and DIDNOT use now. s*/
SENSOR_REG_STRUCT S5K5E2YASensorCCT[]=CAMERA_SENSOR_CCT_DEFAULT_VALUE;
SENSOR_REG_STRUCT S5K5E2YASensorReg[ENGINEER_END]=CAMERA_SENSOR_REG_DEFAULT_VALUE;
/* FIXME: old factors and DIDNOT use now. e*/

static S5K5E2YA_PARA_STRUCT s5k5e2ya;

extern int iWriteRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u16 i2cId);
extern int iReadRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u8 * a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
UINT32 S5K5E2YAMIPISetMaxFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 frameRate);

inline kal_uint16 S5K5E2YA_read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte=0;
	char puSendCmd[2] = {(char)(addr >> 8) , (char)(addr & 0xFF) };
	iReadRegI2C(puSendCmd , 2, (u8*)&get_byte,1,S5K5E2YAMIPI_WRITE_ID);
	return get_byte;
}

inline void S5K5E2YA_wordwrite_cmos_sensor(u16 addr, u32 para)
{
	char puSendCmd[4] = {(char)(addr >> 8) , (char)(addr & 0xFF) ,  (char)(para >> 8),	(char)(para & 0xFF) };
	iWriteRegI2C(puSendCmd , 4,S5K5E2YAMIPI_WRITE_ID);
}

inline void S5K5E2YA_bytewrite_cmos_sensor(u16 addr, u32 para)
{
	char puSendCmd[4] = {(char)(addr >> 8) , (char)(addr & 0xFF)  ,	(char)(para & 0xFF) };
	iWriteRegI2C(puSendCmd , 3,S5K5E2YAMIPI_WRITE_ID);
}



//#define S5K5E2YA_USE_AWB_OTP
#if defined(S5K5E2YA_USE_AWB_OTP)

#define RG_TYPICAL 0x2a1
#define BG_TYPICAL 0x23f


kal_uint32 tRG_Ratio_typical = RG_TYPICAL;
kal_uint32 tBG_Ratio_typical = BG_TYPICAL;


void S5K5E2YA_MIPI_read_otp_wb(struct S5K5E2YA_MIPI_otp_struct *otp)
{
	kal_uint32 R_to_G, B_to_G, G_to_G;
	kal_uint16 PageCount;
	for(PageCount = 4; PageCount>=1; PageCount--)
	{
		SENSORDB("[S5K5E2YA] [S5K5E2YA_MIPI_read_otp_wb] PageCount=%d\n", PageCount);	
		S5K5E2YA_bytewrite_cmos_sensor(0x3a02, PageCount); //page set
		S5K5E2YA_bytewrite_cmos_sensor(0x3a00, 0x01); //otp enable read
		R_to_G = (S5K5E2YA_read_cmos_sensor(0x3a09)<<8)+S5K5E2YA_read_cmos_sensor(0x3a0a);
		B_to_G = (S5K5E2YA_read_cmos_sensor(0x3a0b)<<8)+S5K5E2YA_read_cmos_sensor(0x3a0c);
		G_to_G = (S5K5E2YA_read_cmos_sensor(0x3a0d)<<8)+S5K5E2YA_read_cmos_sensor(0x3a0e);
		S5K5E2YA_bytewrite_cmos_sensor(0x3a00, 0x00); //otp disable read

		if((R_to_G != 0) && (B_to_G != 0) && (G_to_G != 0))
			break;	

		if(PageCount == 1)
			SENSORDB("[S5K5E2YA] [S5K5E2YA_MIPI_read_otp_wb] otp all value is zero");
	}

	otp->R_to_G = R_to_G;
	otp->B_to_G = B_to_G;
	otp->G_to_G = 0x400;
	SENSORDB("[S5K5E2YA] [S5K5E2YA_MIPI_read_otp_wb] otp->R_to_G=0x%x\n", otp->R_to_G);	
	SENSORDB("[S5K5E2YA] [S5K5E2YA_MIPI_read_otp_wb] otp->B_to_G=0x%x\n", otp->B_to_G);	
	SENSORDB("[S5K5E2YA] [S5K5E2YA_MIPI_read_otp_wb] otp->G_to_G=0x%x\n", otp->G_to_G);	
}

void S5K5E2YA_MIPI_algorithm_otp_wb1(struct S5K5E2YA_MIPI_otp_struct *otp)
{
	kal_uint32 R_to_G, B_to_G, G_to_G;
	kal_uint32 R_Gain, B_Gain, G_Gain;
	kal_uint32 G_gain_R, G_gain_B;
	
	R_to_G = otp->R_to_G;
	B_to_G = otp->B_to_G;
	G_to_G = otp->G_to_G;

	SENSORDB("[S5K5E2YA] [S5K5E2YA_MIPI_read_otp_wb] R_to_G=%d\n", R_to_G);	
	SENSORDB("[S5K5E2YA] [S5K5E2YA_MIPI_read_otp_wb] B_to_G=%d\n", B_to_G);	
	SENSORDB("[S5K5E2YA] [S5K5E2YA_MIPI_read_otp_wb] G_to_G=%d\n", G_to_G);

	if(B_to_G < tBG_Ratio_typical)
		{
			if(R_to_G < tRG_Ratio_typical)
				{
					G_Gain = 0x100;
					B_Gain = 0x100 * tBG_Ratio_typical / B_to_G;
					R_Gain = 0x100 * tRG_Ratio_typical / R_to_G;
				}
			else
				{
			        R_Gain = 0x100;
					G_Gain = 0x100 * R_to_G / tRG_Ratio_typical;
					B_Gain = G_Gain * tBG_Ratio_typical / B_to_G;	        
				}
		}
	else
		{
			if(R_to_G < tRG_Ratio_typical)
				{
			        B_Gain = 0x100;
					G_Gain = 0x100 * B_to_G / tBG_Ratio_typical;
					R_Gain = G_Gain * tRG_Ratio_typical / R_to_G;
				}
			else
				{
			        G_gain_B = 0x100*B_to_G / tBG_Ratio_typical;
				    G_gain_R = 0x100*R_to_G / tRG_Ratio_typical;
					
					if(G_gain_B > G_gain_R)
						{
							B_Gain = 0x100;
							G_Gain = G_gain_B;
							R_Gain = G_Gain * tRG_Ratio_typical / R_to_G;
						}
					else
						{
							R_Gain = 0x100;
							G_Gain = G_gain_R;
							B_Gain = G_Gain * tBG_Ratio_typical / B_to_G;
						}        
				}	
		}

	otp->R_Gain = R_Gain;
	otp->B_Gain = B_Gain;
	otp->G_Gain = G_Gain;

	SENSORDB("[S5K5E2YA] [S5K5E2YA_MIPI_read_otp_wb] R_gain=0x%x\n", otp->R_Gain);	
	SENSORDB("[S5K5E2YA] [S5K5E2YA_MIPI_read_otp_wb] B_gain=0x%x\n", otp->B_Gain);	
	SENSORDB("[S5K5E2YA] [S5K5E2YA_MIPI_read_otp_wb] G_gain=0x%x\n", otp->G_Gain);
}



void S5K5E2YA_MIPI_write_otp_wb(struct S5K5E2YA_MIPI_otp_struct *otp)
{
	kal_uint16 R_GainH, B_GainH, G_GainH;
	kal_uint16 R_GainL, B_GainL, G_GainL;
	kal_uint32 temp;

	temp = otp->R_Gain;
	R_GainH = (temp & 0xff00)>>8;
	temp = otp->R_Gain;
	R_GainL = (temp & 0x00ff);

	temp = otp->B_Gain;
	B_GainH = (temp & 0xff00)>>8;
	temp = otp->B_Gain;
	B_GainL = (temp & 0x00ff);

	temp = otp->G_Gain;
	G_GainH = (temp & 0xff00)>>8;
	temp = otp->G_Gain;
	G_GainL = (temp & 0x00ff);

	SENSORDB("[S5K5E2YA] [S5K5E2YA_MIPI_read_otp_wb] R_GainH=0x%x\n", R_GainH);	
	SENSORDB("[S5K5E2YA] [S5K5E2YA_MIPI_read_otp_wb] R_GainL=0x%x\n", R_GainL);	
	SENSORDB("[S5K5E2YA] [S5K5E2YA_MIPI_read_otp_wb] B_GainH=0x%x\n", B_GainH);
	SENSORDB("[S5K5E2YA] [S5K5E2YA_MIPI_read_otp_wb] B_GainL=0x%x\n", B_GainL);	
	SENSORDB("[S5K5E2YA] [S5K5E2YA_MIPI_read_otp_wb] G_GainH=0x%x\n", G_GainH);	
	SENSORDB("[S5K5E2YA] [S5K5E2YA_MIPI_read_otp_wb] G_GainL=0x%x\n", G_GainL);

	S5K5E2YA_bytewrite_cmos_sensor(0x020e, G_GainH);
	S5K5E2YA_bytewrite_cmos_sensor(0x020f, G_GainL);
	S5K5E2YA_bytewrite_cmos_sensor(0x0210, R_GainH);
	S5K5E2YA_bytewrite_cmos_sensor(0x0211, R_GainL);
	S5K5E2YA_bytewrite_cmos_sensor(0x0212, B_GainH);
	S5K5E2YA_bytewrite_cmos_sensor(0x0213, B_GainL);
	S5K5E2YA_bytewrite_cmos_sensor(0x0214, G_GainH);
	S5K5E2YA_bytewrite_cmos_sensor(0x0215, G_GainL);

	SENSORDB("[S5K5E2YA] [S5K5E2YA_MIPI_read_otp_wb] [0x020e,0x%x]\n", S5K5E2YA_read_cmos_sensor(0x020e));	
	SENSORDB("[S5K5E2YA] [S5K5E2YA_MIPI_read_otp_wb] [0x020f,0x%x]\n", S5K5E2YA_read_cmos_sensor(0x020f));	
	SENSORDB("[S5K5E2YA] [S5K5E2YA_MIPI_read_otp_wb] [0x0210,0x%x]\n", S5K5E2YA_read_cmos_sensor(0x0210));
	SENSORDB("[S5K5E2YA] [S5K5E2YA_MIPI_read_otp_wb] [0x0211,0x%x]\n", S5K5E2YA_read_cmos_sensor(0x0211));	
	SENSORDB("[S5K5E2YA] [S5K5E2YA_MIPI_read_otp_wb] [0x0212,0x%x]\n", S5K5E2YA_read_cmos_sensor(0x0212));	
	SENSORDB("[S5K5E2YA] [S5K5E2YA_MIPI_read_otp_wb] [0x0213,0x%x]\n", S5K5E2YA_read_cmos_sensor(0x0213));
	SENSORDB("[S5K5E2YA] [S5K5E2YA_MIPI_read_otp_wb] [0x0214,0x%x]\n", S5K5E2YA_read_cmos_sensor(0x0214));	
	SENSORDB("[S5K5E2YA] [S5K5E2YA_MIPI_read_otp_wb] [0x0215,0x%x]\n", S5K5E2YA_read_cmos_sensor(0x0215));
}

void S5K5E2YA_MIPI_update_wb_register_from_otp(void)
{
	struct S5K5E2YA_MIPI_otp_struct current_otp;
	S5K5E2YA_MIPI_read_otp_wb(&current_otp);
	S5K5E2YA_MIPI_algorithm_otp_wb1(&current_otp);
	S5K5E2YA_MIPI_write_otp_wb(&current_otp);
}
#endif





static inline kal_uint32 GetScenarioLinelength(void)
{
	kal_uint32 u4Linelength=S5K5E2YA_PV_PERIOD_PIXEL_NUMS; //+s5k5e2ya.DummyPixels;
	switch(s_S5K5E2YACurrentScenarioId)
	{
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			u4Linelength=S5K5E2YA_PV_PERIOD_PIXEL_NUMS; //+s5k5e2ya.DummyPixels;
		break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			u4Linelength=S5K5E2YA_VIDEO_PERIOD_PIXEL_NUMS; //+s5k5e2ya.DummyPixels;
		break;
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
			u4Linelength=S5K5E2YA_ZSD_PERIOD_PIXEL_NUMS; //+s5k5e2ya.DummyPixels;
		break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			u4Linelength=S5K5E2YA_FULL_PERIOD_PIXEL_NUMS; //+s5k5e2ya.DummyPixels;
		break;
		default:
		break;
	}
	SENSORDB("u4Linelength=%d\n",u4Linelength);
	return u4Linelength;		
}

static inline kal_uint32 GetScenarioPixelClock(void)
{
	SENSORDB("enter getscenariopixelclock\n");
	kal_uint32 pclk = s5k5e2ya.pvPclk;
	switch(s_S5K5E2YACurrentScenarioId)
	{
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			pclk = s5k5e2ya.pvPclk;
		break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			pclk = s5k5e2ya.m_vidPclk;
		break;
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			pclk = s5k5e2ya.capPclk;
		break;
		default:
		break;
	}
	SENSORDB("pixel clock=%d\n",pclk);
	return pclk;		
}


static inline kal_uint32 GetScenarioFramelength(void)
{
	SENSORDB("enter getscenarioframelength\n");
	kal_uint32 u4Framelength=S5K5E2YA_PV_PERIOD_LINE_NUMS; //+s5k5e2ya.DummyLines ;
	switch(s_S5K5E2YACurrentScenarioId)
	{
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			u4Framelength=S5K5E2YA_PV_PERIOD_LINE_NUMS; //+s5k5e2ya.DummyLines ;
		break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			u4Framelength=S5K5E2YA_VIDEO_PERIOD_LINE_NUMS; //+s5k5e2ya.DummyLines ;
		break;
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
			u4Framelength=S5K5E2YA_ZSD_PERIOD_LINE_NUMS; //+s5k5e2ya.DummyLines ;
		break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			u4Framelength=S5K5E2YA_FULL_PERIOD_LINE_NUMS; //+s5k5e2ya.DummyLines ;
		break;
		default:
		break;
	}
	SENSORDB("Framelength=%d\n",u4Framelength);
	return u4Framelength;		
}

static inline void SetLinelength(kal_uint16 u2Linelength)
{
	SENSORDB("u4Linelength=%d\n",u2Linelength);
	S5K5E2YA_bytewrite_cmos_sensor(0x0104, 0x01);	 //Grouped parameter hold	 
	S5K5E2YA_wordwrite_cmos_sensor(0x0342,u2Linelength);		
	S5K5E2YA_bytewrite_cmos_sensor(0x0104, 0x00);	 //Grouped parameter release	
}

static inline void SetFramelength(kal_uint16 u2Framelength)
{
	SENSORDB("u2Framelength=%d\n",u2Framelength);
	
	spin_lock(&s5k5e2yamipiraw_drv_lock);
	s5k5e2ya.maxExposureLines = u2Framelength;
	spin_unlock(&s5k5e2yamipiraw_drv_lock);
	S5K5E2YA_bytewrite_cmos_sensor(0x0104, 0x01);	 //Grouped parameter hold	 
	S5K5E2YA_wordwrite_cmos_sensor(0x0340,u2Framelength);		
	S5K5E2YA_bytewrite_cmos_sensor(0x0104, 0x00);	 //Grouped parameter release	
}



void S5K5E2YA_write_shutter(kal_uint32 shutter)
{
	SENSORDB("enter s5k5 write shutter");
	kal_uint32 frame_length = 0, line_length = 0, framerate = 0 , pixelclock = 0;	
	unsigned long flags;

	#define SHUTTER_FRAMELENGTH_MARGIN 16
	
	frame_length = GetScenarioFramelength();

	frame_length = (s5k5e2ya.FixedFrameLength>frame_length)?s5k5e2ya.FixedFrameLength:frame_length;
	
	if (shutter < 3)
		shutter = 3;

	if (shutter+SHUTTER_FRAMELENGTH_MARGIN > frame_length)
		frame_length = shutter + SHUTTER_FRAMELENGTH_MARGIN; //extend framelength

	if(s5k5e2ya.S5K5E2YAAutoFlickerMode == KAL_TRUE)
	{
		line_length = GetScenarioLinelength();
		pixelclock = GetScenarioPixelClock();
		framerate = (10 * pixelclock) / (frame_length * line_length);
		if(framerate == 300)
		  	framerate = 296;
		else if(framerate == 150)
		  	framerate = 148;
	}

	spin_lock_irqsave(&s5k5e2yamipiraw_drv_lock,flags);
	s5k5e2ya.maxExposureLines = frame_length;
	s5k5e2ya.shutter = shutter;
	spin_unlock_irqrestore(&s5k5e2yamipiraw_drv_lock,flags);

	//S5K5E2YA_bytewrite_cmos_sensor(0x0104, 0x01);    //Grouped parameter hold    
 	S5K5E2YA_wordwrite_cmos_sensor(0x0202, shutter);
	S5K5E2YA_wordwrite_cmos_sensor(0x0340,frame_length);	
 	//S5K5E2YA_bytewrite_cmos_sensor(0x0104, 0x00);    //Grouped parameter release
 	
	SENSORDB("shutter=%d,framerate=%d\n",shutter,framerate);
}   /* write_S5K5E2YA_shutter */




/*************************************************************************
* FUNCTION
*    S5K5E2YA_SetGain
*
* DESCRIPTION
*    This function is to set global gain to sensor.
*
* PARAMETERS
*    gain : sensor global gain(base: 0x40)
*
* RETURNS
*    the actually gain set to sensor.
*
* GLOBALS AFFECTED
*
*************************************************************************/
void S5K5E2YA_SetGain(UINT16 gain)
{

	unsigned long flags;
	SENSORDB("gain=%d\n",gain);
	
	spin_lock_irqsave(&s5k5e2yamipiraw_drv_lock,flags);
	s5k5e2ya.sensorGain = gain/2;
	spin_unlock_irqrestore(&s5k5e2yamipiraw_drv_lock,flags);
	
	//S5K5E2YA_bytewrite_cmos_sensor(0x0104, 0x01);    //Grouped parameter hold    
	S5K5E2YA_wordwrite_cmos_sensor(0x0204,s5k5e2ya.sensorGain);   //sensor base gain is 32
	//S5K5E2YA_bytewrite_cmos_sensor(0x0104, 0x00);    //Grouped parameter release 
	
	SENSORDB("enter s5k5 setgain");
}


void S5K5E2YA_camera_para_to_sensor(void)
{
  /*  kal_uint32    i;
    for(i=0; 0xFFFFFFFF!=S5K5E2YASensorReg[i].Addr; i++)
    {
        S5K5E2YA_wordwrite_cmos_sensor(S5K5E2YASensorReg[i].Addr, S5K5E2YASensorReg[i].Para);
    }
    for(i=ENGINEER_START_ADDR; 0xFFFFFFFF!=S5K5E2YASensorReg[i].Addr; i++)
    {
        S5K5E2YA_wordwrite_cmos_sensor(S5K5E2YASensorReg[i].Addr, S5K5E2YASensorReg[i].Para);
    }
    for(i=FACTORY_START_ADDR; i<FACTORY_END_ADDR; i++)
    {
        S5K5E2YA_wordwrite_cmos_sensor(S5K5E2YASensorCCT[i].Addr, S5K5E2YASensorCCT[i].Para);
    }*/
}


/*************************************************************************
* FUNCTION
*    S5K5E2YA_sensor_to_camera_para
*
* DESCRIPTION
*    // update camera_para from sensor register
*
* PARAMETERS
*    None
*
* RETURNS
*    gain : sensor global gain(base: 0x40)
*
* GLOBALS AFFECTED
*
*************************************************************************/
void S5K5E2YA_sensor_to_camera_para(void)
{
/*    kal_uint32    i, temp_data;
    for(i=0; 0xFFFFFFFF!=S5K5E2YASensorReg[i].Addr; i++)
    {
         temp_data = S5K5E2YA_read_cmos_sensor(S5K5E2YASensorReg[i].Addr);
		 spin_lock(&s5k5e2yamipiraw_drv_lock);
		 S5K5E2YASensorReg[i].Para =temp_data;
		 spin_unlock(&s5k5e2yamipiraw_drv_lock);
    }
    for(i=ENGINEER_START_ADDR; 0xFFFFFFFF!=S5K5E2YASensorReg[i].Addr; i++)
    {
        temp_data = S5K5E2YA_read_cmos_sensor(S5K5E2YASensorReg[i].Addr);
		spin_lock(&s5k5e2yamipiraw_drv_lock);
		S5K5E2YASensorReg[i].Para = temp_data;
		spin_unlock(&s5k5e2yamipiraw_drv_lock);
    }*/
}

/*************************************************************************
* FUNCTION
*    S5K5E2YA_get_sensor_group_count
*
* DESCRIPTION
*    //
*
* PARAMETERS
*    None
*
* RETURNS
*    gain : sensor global gain(base: 0x40)
*
* GLOBALS AFFECTED
*
*************************************************************************/
kal_int32  S5K5E2YA_get_sensor_group_count(void)
{
    return GROUP_TOTAL_NUMS;
}

void S5K5E2YA_get_sensor_group_info(kal_uint16 group_idx, kal_int8* group_name_ptr, kal_int32* item_count_ptr)
{
 /*  switch (group_idx)
   {
        case PRE_GAIN:
            sprintf((char *)group_name_ptr, "CCT");
            *item_count_ptr = 2;
            break;
        case CMMCLK_CURRENT:
            sprintf((char *)group_name_ptr, "CMMCLK Current");
            *item_count_ptr = 1;
            break;
        case FRAME_RATE_LIMITATION:
            sprintf((char *)group_name_ptr, "Frame Rate Limitation");
            *item_count_ptr = 2;
            break;
        case REGISTER_EDITOR:
            sprintf((char *)group_name_ptr, "Register Editor");
            *item_count_ptr = 2;
            break;
        default:
            ASSERT(0);
	}*/
}

void S5K5E2YA_get_sensor_item_info(kal_uint16 group_idx,kal_uint16 item_idx, MSDK_SENSOR_ITEM_INFO_STRUCT* info_ptr)
{
/*    kal_int16 temp_reg=0;
    kal_uint16 temp_gain=0, temp_addr=0, temp_para=0;

    switch (group_idx)
    {
        case PRE_GAIN:
           switch (item_idx)
          {
              case 0:
                sprintf((char *)info_ptr->ItemNamePtr,"Pregain-R");
                  temp_addr = PRE_GAIN_R_INDEX;
              break;
              case 1:
                sprintf((char *)info_ptr->ItemNamePtr,"Pregain-Gr");
                  temp_addr = PRE_GAIN_Gr_INDEX;
              break;
              case 2:
                sprintf((char *)info_ptr->ItemNamePtr,"Pregain-Gb");
                  temp_addr = PRE_GAIN_Gb_INDEX;
              break;
              case 3:
                sprintf((char *)info_ptr->ItemNamePtr,"Pregain-B");
                  temp_addr = PRE_GAIN_B_INDEX;
              break;
              case 4:
                 sprintf((char *)info_ptr->ItemNamePtr,"SENSOR_BASEGAIN");
                 temp_addr = SENSOR_BASEGAIN;
              break;
              default:
                 ASSERT(0);
          }

            temp_para= S5K5E2YASensorCCT[temp_addr].Para;
			temp_gain= (temp_para*1000/s5k5e2ya.sensorBaseGain) ;

            info_ptr->ItemValue=temp_gain;
            info_ptr->IsTrueFalse=KAL_FALSE;
            info_ptr->IsReadOnly=KAL_FALSE;
            info_ptr->IsNeedRestart=KAL_FALSE;
            info_ptr->Min= S5K5E2YA_MIN_ANALOG_GAIN * 1000;
            info_ptr->Max= S5K5E2YA_MAX_ANALOG_GAIN * 1000;
            break;
        case CMMCLK_CURRENT:
            switch (item_idx)
            {
                case 0:
                    sprintf((char *)info_ptr->ItemNamePtr,"Drv Cur[2,4,6,8]mA");

                    //temp_reg=MT9P017SensorReg[CMMCLK_CURRENT_INDEX].Para;
                    temp_reg = ISP_DRIVING_2MA;
                    if(temp_reg==ISP_DRIVING_2MA)
                    {
                        info_ptr->ItemValue=2;
                    }
                    else if(temp_reg==ISP_DRIVING_4MA)
                    {
                        info_ptr->ItemValue=4;
                    }
                    else if(temp_reg==ISP_DRIVING_6MA)
                    {
                        info_ptr->ItemValue=6;
                    }
                    else if(temp_reg==ISP_DRIVING_8MA)
                    {
                        info_ptr->ItemValue=8;
                    }

                    info_ptr->IsTrueFalse=KAL_FALSE;
                    info_ptr->IsReadOnly=KAL_FALSE;
                    info_ptr->IsNeedRestart=KAL_TRUE;
                    info_ptr->Min=2;
                    info_ptr->Max=8;
                    break;
                default:
                    ASSERT(0);
            }
            break;
        case FRAME_RATE_LIMITATION:
            switch (item_idx)
            {
                case 0:
                    sprintf((char *)info_ptr->ItemNamePtr,"Max Exposure Lines");
                    info_ptr->ItemValue=    111;  //MT9P017_MAX_EXPOSURE_LINES;
                    info_ptr->IsTrueFalse=KAL_FALSE;
                    info_ptr->IsReadOnly=KAL_TRUE;
                    info_ptr->IsNeedRestart=KAL_FALSE;
                    info_ptr->Min=0;
                    info_ptr->Max=0;
                    break;
                case 1:
                    sprintf((char *)info_ptr->ItemNamePtr,"Min Frame Rate");
                    info_ptr->ItemValue=12;
                    info_ptr->IsTrueFalse=KAL_FALSE;
                    info_ptr->IsReadOnly=KAL_TRUE;
                    info_ptr->IsNeedRestart=KAL_FALSE;
                    info_ptr->Min=0;
                    info_ptr->Max=0;
                    break;
                default:
                    ASSERT(0);
            }
            break;
        case REGISTER_EDITOR:
            switch (item_idx)
            {
                case 0:
                    sprintf((char *)info_ptr->ItemNamePtr,"REG Addr.");
                    info_ptr->ItemValue=0;
                    info_ptr->IsTrueFalse=KAL_FALSE;
                    info_ptr->IsReadOnly=KAL_FALSE;
                    info_ptr->IsNeedRestart=KAL_FALSE;
                    info_ptr->Min=0;
                    info_ptr->Max=0xFFFF;
                    break;
                case 1:
                    sprintf((char *)info_ptr->ItemNamePtr,"REG Value");
                    info_ptr->ItemValue=0;
                    info_ptr->IsTrueFalse=KAL_FALSE;
                    info_ptr->IsReadOnly=KAL_FALSE;
                    info_ptr->IsNeedRestart=KAL_FALSE;
                    info_ptr->Min=0;
                    info_ptr->Max=0xFFFF;
                    break;
                default:
                ASSERT(0);
            }
            break;
        default:
            ASSERT(0);
    }*/
}



kal_bool S5K5E2YA_set_sensor_item_info(kal_uint16 group_idx, kal_uint16 item_idx, kal_int32 ItemValue)
{
/*
   kal_uint16  temp_gain=0,temp_addr=0, temp_para=0;
   switch (group_idx)
    {
        case PRE_GAIN:
            switch (item_idx)
            {
				case 0:	temp_addr = PRE_GAIN_R_INDEX;		break;	
				case 1:	temp_addr = PRE_GAIN_Gr_INDEX;		break;
				case 2: temp_addr = PRE_GAIN_Gb_INDEX;		break;
				case 3: temp_addr = PRE_GAIN_B_INDEX;		break;
				case 4:	temp_addr = SENSOR_BASEGAIN;		break;
				default: ASSERT(0);
          }

			temp_gain=((ItemValue*s5k5e2ya.sensorBaseGain+500)/1000);			//+500:get closed integer value

		  spin_lock(&s5k5e2yamipiraw_drv_lock);
          S5K5E2YASensorCCT[temp_addr].Para = temp_para;
		  spin_unlock(&s5k5e2yamipiraw_drv_lock);
          S5K5E2YA_wordwrite_cmos_sensor(S5K5E2YASensorCCT[temp_addr].Addr,temp_para);
          break;

        case CMMCLK_CURRENT:
            switch (item_idx)
            {
                case 0:
                    //no need to apply this item for driving current
                    break;
                default:
                    ASSERT(0);
            }
            break;
        case FRAME_RATE_LIMITATION:
            ASSERT(0);
            break;
        case REGISTER_EDITOR:
            switch (item_idx)
            {
                case 0:
					spin_lock(&s5k5e2yamipiraw_drv_lock);
                    S5K5E2YA_FAC_SENSOR_REG=ItemValue;
					spin_unlock(&s5k5e2yamipiraw_drv_lock);
                    break;
                case 1:
                    S5K5E2YA_wordwrite_cmos_sensor(S5K5E2YA_FAC_SENSOR_REG,ItemValue);
                    break;
                default:
                    ASSERT(0);
            }
            break;
        default:
            ASSERT(0);
    }*/
    return KAL_TRUE; 
}


static void S5K5E2YA_SetDummy( const kal_uint32 iPixels, const kal_uint32 iLines )
{
	kal_uint16 u2Linelength = 0,u2Framelength = 0;
	SENSORDB("iPixels=%d,iLines=%d\n",iPixels,iLines);
	
	switch (s_S5K5E2YACurrentScenarioId) 
	{
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			u2Linelength = S5K5E2YA_PV_PERIOD_PIXEL_NUMS+iPixels;
			u2Framelength = S5K5E2YA_PV_PERIOD_LINE_NUMS+iLines;
		break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			u2Linelength = S5K5E2YA_VIDEO_PERIOD_PIXEL_NUMS+iPixels;
			u2Framelength = S5K5E2YA_VIDEO_PERIOD_LINE_NUMS+iLines;
		break;
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
			u2Linelength = S5K5E2YA_ZSD_PERIOD_PIXEL_NUMS+iPixels;
			u2Framelength = S5K5E2YA_ZSD_PERIOD_LINE_NUMS+iLines;
		break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			u2Linelength = S5K5E2YA_FULL_PERIOD_PIXEL_NUMS+iPixels;
			u2Framelength = S5K5E2YA_FULL_PERIOD_LINE_NUMS+iLines;
		break;
		default:
			u2Linelength = S5K5E2YA_PV_PERIOD_PIXEL_NUMS+iPixels;
			u2Framelength = S5K5E2YA_PV_PERIOD_LINE_NUMS+iLines;
		break;
	}

	spin_lock(&s5k5e2yamipiraw_drv_lock);
	s5k5e2ya.maxExposureLines = u2Framelength;
	s5k5e2ya.DummyPixels=iPixels;
	s5k5e2ya.DummyLines=iLines;
	spin_unlock(&s5k5e2yamipiraw_drv_lock);

	//S5K5E2YA_bytewrite_cmos_sensor(0x0104, 0x01);    //Grouped parameter hold    
	S5K5E2YA_wordwrite_cmos_sensor(0x0340,u2Framelength);
	S5K5E2YA_wordwrite_cmos_sensor(0x0342,u2Linelength);
	//S5K5E2YA_bytewrite_cmos_sensor(0x0104, 0x00);    //Grouped parameter hold    
}  



void S5K5E2YAPreviewSetting(void)
{

	SENSORDB("s5k5e2yaPreview Setting Start\n");                                                            
   
   // Reset for operation																		  
   S5K5E2YA_bytewrite_cmos_sensor(0x0100,0x00); //stream off
   
   // Clock Setting
   S5K5E2YA_bytewrite_cmos_sensor(0x0305,0x06); //PLLP (def:5)
   S5K5E2YA_bytewrite_cmos_sensor(0x0306,0x00);
   S5K5E2YA_bytewrite_cmos_sensor(0x0307,0xE0); //PLLM (def:CCh 204d --> B3h 179d)
   S5K5E2YA_bytewrite_cmos_sensor(0x3C1F,0x00); //PLLS 
   
   S5K5E2YA_bytewrite_cmos_sensor(0x0820,0x03); // requested link bit rate mbps : (def:3D3h 979d --> 35Bh 859d)
   S5K5E2YA_bytewrite_cmos_sensor(0x0821,0x80); 
   S5K5E2YA_bytewrite_cmos_sensor(0x3C1C,0x58); //dbr_div
	
   
   // Size Setting
   S5K5E2YA_bytewrite_cmos_sensor(0x0340,0x07); // frame_length_lines : def. 990d (--> 3C8 Mimnimum 22 lines)
   S5K5E2YA_bytewrite_cmos_sensor(0x0341,0xD0);
   S5K5E2YA_bytewrite_cmos_sensor(0x0342,0x0B); // line_length_pck : def. 2950d 
   S5K5E2YA_bytewrite_cmos_sensor(0x0343,0x86);
   
   S5K5E2YA_bytewrite_cmos_sensor(0x0344,0x00); // x_addr_start
   S5K5E2YA_bytewrite_cmos_sensor(0x0345,0x08); 
   S5K5E2YA_bytewrite_cmos_sensor(0x0346,0x00); // y_addr_start
   S5K5E2YA_bytewrite_cmos_sensor(0x0347,0x08); 
   S5K5E2YA_bytewrite_cmos_sensor(0x0348,0x0A); // x_addr_end : def. 2575d	
   S5K5E2YA_bytewrite_cmos_sensor(0x0349,0x07); 
   S5K5E2YA_bytewrite_cmos_sensor(0x034A,0x07); // y_addr_end : def. 1936d
   S5K5E2YA_bytewrite_cmos_sensor(0x034B,0x87); 
   S5K5E2YA_bytewrite_cmos_sensor(0x034C,0x05); // x_output size : def. 1280d
   S5K5E2YA_bytewrite_cmos_sensor(0x034D,0x00); 
   S5K5E2YA_bytewrite_cmos_sensor(0x034E,0x03); // y_output size : def. 960d
   S5K5E2YA_bytewrite_cmos_sensor(0x034F,0xC0); 
   
   
   //Digital Binning
   S5K5E2YA_bytewrite_cmos_sensor(0x0900,0x01);    //2x2 Binning
   S5K5E2YA_bytewrite_cmos_sensor(0x0901,0x22);
   S5K5E2YA_bytewrite_cmos_sensor(0x0387,0x03);
   
   //Integration time  
   S5K5E2YA_bytewrite_cmos_sensor(0x0202,0x02);  // coarse integration
   S5K5E2YA_bytewrite_cmos_sensor(0x0203,0x00);
   
   
   //Analog Timing Tuning (140117)
   S5K5E2YA_bytewrite_cmos_sensor(0x3000,0x04);    // ct_ld_start
   S5K5E2YA_bytewrite_cmos_sensor(0x3002,0x03);    // ct_sl_start
   S5K5E2YA_bytewrite_cmos_sensor(0x3003,0x04);    // ct_sl_margin
   S5K5E2YA_bytewrite_cmos_sensor(0x3004,0x02);    // ct_rx_start
   S5K5E2YA_bytewrite_cmos_sensor(0x3005,0x00);    // ct_rx_margin (MSB)
   S5K5E2YA_bytewrite_cmos_sensor(0x3006,0x10);    // ct_rx_margin (LSB)
   S5K5E2YA_bytewrite_cmos_sensor(0x3007,0x03);    // ct_tx_start
   S5K5E2YA_bytewrite_cmos_sensor(0x3008,0x46);    // ct_tx_width
   S5K5E2YA_bytewrite_cmos_sensor(0x0200,0x04);    // (fine_integ_time) (MSB)
   S5K5E2YA_bytewrite_cmos_sensor(0x0201,0x98);    // (fine_integ_time) (LSB)
   S5K5E2YA_bytewrite_cmos_sensor(0x3039,0x05);    // cintc1_margin
   S5K5E2YA_bytewrite_cmos_sensor(0x303A,0x05);    // cintc2_margin
   S5K5E2YA_bytewrite_cmos_sensor(0x303B,0x00);    // offs_sh
   S5K5E2YA_bytewrite_cmos_sensor(0x3009,0x05);    // ct_srx_margin
   S5K5E2YA_bytewrite_cmos_sensor(0x300A,0x46);    // ct_stx_width
   S5K5E2YA_bytewrite_cmos_sensor(0x300B,0x2E);    // ct_dstx_width
   S5K5E2YA_bytewrite_cmos_sensor(0x300C,0x10);    // ct_stx2dstx
   S5K5E2YA_bytewrite_cmos_sensor(0x3012,0x05);    // ct_cds_start
   S5K5E2YA_bytewrite_cmos_sensor(0x3013,0x00);    // ct_s1s_start
   S5K5E2YA_bytewrite_cmos_sensor(0x3014,0x1C);    // ct_s1s_end
   S5K5E2YA_bytewrite_cmos_sensor(0x300E,0x64);    // ct_S5K5E2YA_bytewrite_cmos_sensor(0x3_width
   S5K5E2YA_bytewrite_cmos_sensor(0x3010,0x56);    // ct_s4_width
   S5K5E2YA_bytewrite_cmos_sensor(0x3019,0x03);    // ct_s4d_start
   S5K5E2YA_bytewrite_cmos_sensor(0x301A,0x00);    // ct_pbr_start
   S5K5E2YA_bytewrite_cmos_sensor(0x301B,0x05);    // ct_pbr_width
   S5K5E2YA_bytewrite_cmos_sensor(0x301C,0x00);    // ct_pbs_start
   S5K5E2YA_bytewrite_cmos_sensor(0x301D,0x1C);    // ct_pbs_width
   S5K5E2YA_bytewrite_cmos_sensor(0x301E,0x00);    // ct_pbr_ob_start
   S5K5E2YA_bytewrite_cmos_sensor(0x301F,0x0E);    // ct_pbr_ob_width
   S5K5E2YA_bytewrite_cmos_sensor(0x3020,0x00);    // ct_pbs_ob_start
   S5K5E2YA_bytewrite_cmos_sensor(0x3021,0x00);    // ct_pbs_ob_width
   S5K5E2YA_bytewrite_cmos_sensor(0x3022,0x0A);    // ct_cds_lim_start
   S5K5E2YA_bytewrite_cmos_sensor(0x3023,0x19);    // ct_crs_start
   S5K5E2YA_bytewrite_cmos_sensor(0x3024,0x00);    // ct_lp_hblk_cds_start (MSB)
   S5K5E2YA_bytewrite_cmos_sensor(0x3025,0x00);    // ct_lp_hblk_cds_start (LSB)
   S5K5E2YA_bytewrite_cmos_sensor(0x3026,0x00);    // ct_lp_hblk_cds_end (MSB)
   S5K5E2YA_bytewrite_cmos_sensor(0x3027,0x00);    // ct_lp_hblk_cds_end (LSB)
   S5K5E2YA_bytewrite_cmos_sensor(0x3028,0x16);    // ct_rmp_off_start
   S5K5E2YA_bytewrite_cmos_sensor(0x3015,0x00);    // ct_rmp_rst_start (MSB)
   S5K5E2YA_bytewrite_cmos_sensor(0x3016,0x6D);    // ct_rmp_rst_start (LSB)
   S5K5E2YA_bytewrite_cmos_sensor(0x3017,0x00);    // ct_rmp_sig_start (MSB)
   S5K5E2YA_bytewrite_cmos_sensor(0x3018,0x84);    // ct_rmp_sig_start (LSB)
   S5K5E2YA_bytewrite_cmos_sensor(0x302B,0x10);    // ct_cnt_margin
   S5K5E2YA_bytewrite_cmos_sensor(0x302C,0x0A);    // ct_rmp_per
   S5K5E2YA_bytewrite_cmos_sensor(0x302D,0x06);    // ct_cnt_ms_margin1
   S5K5E2YA_bytewrite_cmos_sensor(0x302E,0x05);    // ct_cnt_ms_margin2
   S5K5E2YA_bytewrite_cmos_sensor(0x302F,0x0E);    // rst_mx
   S5K5E2YA_bytewrite_cmos_sensor(0x3030,0x2F);    // sig_mx
   S5K5E2YA_bytewrite_cmos_sensor(0x3031,0x08);    // ct_latch_start
   S5K5E2YA_bytewrite_cmos_sensor(0x3032,0x05);    // ct_latch_width
   S5K5E2YA_bytewrite_cmos_sensor(0x3033,0x09);    // ct_hold_start
   S5K5E2YA_bytewrite_cmos_sensor(0x3034,0x05);    // ct_hold_width
   S5K5E2YA_bytewrite_cmos_sensor(0x3035,0x00);    // ct_lp_hblk_dbs_start (MSB)
   S5K5E2YA_bytewrite_cmos_sensor(0x3036,0x00);    // ct_lp_hblk_dbs_start (LSB)
   S5K5E2YA_bytewrite_cmos_sensor(0x3037,0x00);    // ct_lp_hblk_dbs_end (MSB)
   S5K5E2YA_bytewrite_cmos_sensor(0x3038,0x00);    // ct_lp_hblk_dbs_end (LSB)
   S5K5E2YA_bytewrite_cmos_sensor(0x3088,0x06);    // ct_lat_lsb_offset_start1
   S5K5E2YA_bytewrite_cmos_sensor(0x308A,0x08);    // ct_lat_lsb_offset_end1
   S5K5E2YA_bytewrite_cmos_sensor(0x308C,0x05);    // ct_lat_lsb_offset_start2
   S5K5E2YA_bytewrite_cmos_sensor(0x308E,0x07);    // ct_lat_lsb_offset_end2
   S5K5E2YA_bytewrite_cmos_sensor(0x3090,0x06);    // ct_conv_en_offset_start1
   S5K5E2YA_bytewrite_cmos_sensor(0x3092,0x08);    // ct_conv_en_offset_end1
   S5K5E2YA_bytewrite_cmos_sensor(0x3094,0x05);    // ct_conv_en_offset_start2
   S5K5E2YA_bytewrite_cmos_sensor(0x3096,0x21);    // ct_conv_en_offset_end2
   
   //CDS
   S5K5E2YA_bytewrite_cmos_sensor(0x3099,0x0E);  // cds_option ([3]:crs switch disable, S5K5E2YA_bytewrite_cmos_sensor(0x3,s4 strengthx16)
   S5K5E2YA_bytewrite_cmos_sensor(0x3070,0x10);  // comp1_bias (default:77)
   S5K5E2YA_bytewrite_cmos_sensor(0x3085,0x11);  // comp1_bias (gain1~4)
   S5K5E2YA_bytewrite_cmos_sensor(0x3086,0x01);  // comp1_bias (gain4~8) 
   
   
   
   //RMP
   S5K5E2YA_bytewrite_cmos_sensor(0x3064,0x00); // Multiple sampling(gainx8,x16)
   S5K5E2YA_bytewrite_cmos_sensor(0x3062,0x08); // off_rst
   
   //DBR
   S5K5E2YA_bytewrite_cmos_sensor(0x3061,0x11);  // dbr_tune_rd (default :08, 0E 3.02V)    3.1V
   S5K5E2YA_bytewrite_cmos_sensor(0x307B,0x20);  // dbr_tune_rgsl (default :08)
   
   //Bias sampling
   S5K5E2YA_bytewrite_cmos_sensor(0x3068,0x00); // RMP BP bias sampling [0]: Disable
   S5K5E2YA_bytewrite_cmos_sensor(0x3074,0x00); // Pixel bias sampling [2]:Default L
   S5K5E2YA_bytewrite_cmos_sensor(0x307D,0x00); // VREF sampling [0] : Disable
   S5K5E2YA_bytewrite_cmos_sensor(0x3045,0x01); // ct_opt_l1_start
   S5K5E2YA_bytewrite_cmos_sensor(0x3046,0x05); // ct_opt_l1_width
   S5K5E2YA_bytewrite_cmos_sensor(0x3047,0x78);
   
   //Smart PLA
   S5K5E2YA_bytewrite_cmos_sensor(0x307F,0xB1); //RDV_OPTION[5:4], RG default high
   S5K5E2YA_bytewrite_cmos_sensor(0x3098,0x01); //CDS_OPTION[16] SPLA-II enable
   S5K5E2YA_bytewrite_cmos_sensor(0x305C,0xF6); //lob_extension[6]
   
   S5K5E2YA_bytewrite_cmos_sensor(0x306B,0x10); // [3]bls_stx_en disable
   S5K5E2YA_bytewrite_cmos_sensor(0x3063,0x27); // ADC_SAT 
   S5K5E2YA_bytewrite_cmos_sensor(0x320C,0x07); // ADC_MAX (def:076Ch --> 0700h)
   S5K5E2YA_bytewrite_cmos_sensor(0x320D,0x00); 																	  
   S5K5E2YA_bytewrite_cmos_sensor(0x3400,0x01); // GAS bypass
   S5K5E2YA_bytewrite_cmos_sensor(0x3235,0x49); // L/F-ADLC on
   S5K5E2YA_bytewrite_cmos_sensor(0x3233,0x00); // D-pedestal L/F ADLC off (1FC0h)
   S5K5E2YA_bytewrite_cmos_sensor(0x3234,0x00);
   S5K5E2YA_bytewrite_cmos_sensor(0x3300,0x0C); //BPC On
   S5K5E2YA_bytewrite_cmos_sensor(0x0204,0x00); //Analog gain x1
   S5K5E2YA_bytewrite_cmos_sensor(0x0205,0x20);
   
   S5K5E2YA_bytewrite_cmos_sensor(0x3203,0x45);
   S5K5E2YA_bytewrite_cmos_sensor(0x3205,0x4D);
   S5K5E2YA_bytewrite_cmos_sensor(0x320B,0x40);
   S5K5E2YA_bytewrite_cmos_sensor(0x320C,0x06);
   S5K5E2YA_bytewrite_cmos_sensor(0x320D,0xC0);
   
   //LSC settting for 2560x1920
   
   S5K5E2YA_bytewrite_cmos_sensor(0x340B,0x00);
   S5K5E2YA_bytewrite_cmos_sensor(0x340C,0x00);
   S5K5E2YA_bytewrite_cmos_sensor(0x340D,0x00);
   S5K5E2YA_bytewrite_cmos_sensor(0x340E,0x00);
   
   S5K5E2YA_bytewrite_cmos_sensor(0x3401,0x50);
   S5K5E2YA_bytewrite_cmos_sensor(0x3402,0x3C);
   S5K5E2YA_bytewrite_cmos_sensor(0x3403,0x03);
   S5K5E2YA_bytewrite_cmos_sensor(0x3404,0x33);
   S5K5E2YA_bytewrite_cmos_sensor(0x3405,0x04);
   S5K5E2YA_bytewrite_cmos_sensor(0x3406,0x44); 
   S5K5E2YA_bytewrite_cmos_sensor(0x3458,0x03);
   S5K5E2YA_bytewrite_cmos_sensor(0x3459,0x33);
   S5K5E2YA_bytewrite_cmos_sensor(0x345A,0x04);
   S5K5E2YA_bytewrite_cmos_sensor(0x345B,0x44);

  // S5K5E2YA_bytewrite_cmos_sensor(0x3200,0x00);
   
   // streaming ON
   S5K5E2YA_bytewrite_cmos_sensor(0x0100,0x01); 
   
	mDELAY(50);
                                                                                                         
	SENSORDB("preview setting end\n");
}

void S5K5E2YAVideoSetting(void)
{
	SENSORDB("enter s5k5e2ya video setting\n");

	
	// Reset for operation																		   
	S5K5E2YA_bytewrite_cmos_sensor(0x0100,0x00); //stream off
	
	// Clock Setting
	S5K5E2YA_bytewrite_cmos_sensor(0x0305,0x06); //PLLP (def:5)
	S5K5E2YA_bytewrite_cmos_sensor(0x0306,0x00);
	S5K5E2YA_bytewrite_cmos_sensor(0x0307,0xE0); //PLLM (def:CCh 204d --> B3h 179d)
	S5K5E2YA_bytewrite_cmos_sensor(0x3C1F,0x00); //PLLS 
	
	
	S5K5E2YA_bytewrite_cmos_sensor(0x0820,0x03); // requested link bit rate mbps : (def:3D3h 979d --> 35Bh 859d)
	S5K5E2YA_bytewrite_cmos_sensor(0x0821,0x80); 
	S5K5E2YA_bytewrite_cmos_sensor(0x3C1C,0x58); //dbr_div
	 
	
	// Size Setting
	S5K5E2YA_bytewrite_cmos_sensor(0x0340,0x07); // frame_length_lines : def. 1962d (7C2 --> 7A6 Mimnimum 23 lines)
	S5K5E2YA_bytewrite_cmos_sensor(0x0341,0xD0);
	S5K5E2YA_bytewrite_cmos_sensor(0x0342,0x0B); // line_length_pck : def. 2950d 
	S5K5E2YA_bytewrite_cmos_sensor(0x0343,0x86);
	
	S5K5E2YA_bytewrite_cmos_sensor(0x0344,0x00); // x_addr_start
	S5K5E2YA_bytewrite_cmos_sensor(0x0345,0x08); 
	S5K5E2YA_bytewrite_cmos_sensor(0x0346,0x00); // y_addr_start
	S5K5E2YA_bytewrite_cmos_sensor(0x0347,0xF8); 
	S5K5E2YA_bytewrite_cmos_sensor(0x0348,0x0A); // x_addr_end : def. 2575d  
	S5K5E2YA_bytewrite_cmos_sensor(0x0349,0x07); 
	S5K5E2YA_bytewrite_cmos_sensor(0x034A,0x06); // y_addr_end : def. 1936d
	S5K5E2YA_bytewrite_cmos_sensor(0x034B,0x97); 
	S5K5E2YA_bytewrite_cmos_sensor(0x034C,0x0A); // x_output size : def. 2560d 
	S5K5E2YA_bytewrite_cmos_sensor(0x034D,0x00); 
	S5K5E2YA_bytewrite_cmos_sensor(0x034E,0x05); // y_output size : def. 1440d
	S5K5E2YA_bytewrite_cmos_sensor(0x034F,0xA0); 
	
	//Digital Binning(default)
	S5K5E2YA_bytewrite_cmos_sensor(0x0900,0x00);	//0x0 Binning
	S5K5E2YA_bytewrite_cmos_sensor(0x0901,0x20);
	S5K5E2YA_bytewrite_cmos_sensor(0x0387,0x01);
	
	
	//Integration time	
	S5K5E2YA_bytewrite_cmos_sensor(0x0202,0x02);  // coarse integration
	S5K5E2YA_bytewrite_cmos_sensor(0x0203,0x00);
	
	//Analog Timing Tuning (140117)
	S5K5E2YA_bytewrite_cmos_sensor(0x3000,0x04);	// ct_ld_start
	S5K5E2YA_bytewrite_cmos_sensor(0x3002,0x03);	// ct_sl_start
	S5K5E2YA_bytewrite_cmos_sensor(0x3003,0x04);	// ct_sl_margin
	S5K5E2YA_bytewrite_cmos_sensor(0x3004,0x02);	// ct_rx_start
	S5K5E2YA_bytewrite_cmos_sensor(0x3005,0x00);	// ct_rx_margin (MSB)
	S5K5E2YA_bytewrite_cmos_sensor(0x3006,0x10);	// ct_rx_margin (LSB)
	S5K5E2YA_bytewrite_cmos_sensor(0x3007,0x03);	// ct_tx_start
	S5K5E2YA_bytewrite_cmos_sensor(0x3008,0x46);	// ct_tx_width
	S5K5E2YA_bytewrite_cmos_sensor(0x0200,0x04);	// (fine_integ_time) (MSB)
	S5K5E2YA_bytewrite_cmos_sensor(0x0201,0x98);	// (fine_integ_time) (LSB)
	S5K5E2YA_bytewrite_cmos_sensor(0x3039,0x05);	// cintc1_margin
	S5K5E2YA_bytewrite_cmos_sensor(0x303A,0x05);	// cintc2_margin
	S5K5E2YA_bytewrite_cmos_sensor(0x303B,0x00);	// offs_sh
	S5K5E2YA_bytewrite_cmos_sensor(0x3009,0x05);	// ct_srx_margin
	S5K5E2YA_bytewrite_cmos_sensor(0x300A,0x46);	// ct_stx_width
	S5K5E2YA_bytewrite_cmos_sensor(0x300B,0x2E);	// ct_dstx_width
	S5K5E2YA_bytewrite_cmos_sensor(0x300C,0x10);	// ct_stx2dstx
	S5K5E2YA_bytewrite_cmos_sensor(0x3012,0x05);	// ct_cds_start
	S5K5E2YA_bytewrite_cmos_sensor(0x3013,0x00);	// ct_s1s_start
	S5K5E2YA_bytewrite_cmos_sensor(0x3014,0x1C);	// ct_s1s_end
	S5K5E2YA_bytewrite_cmos_sensor(0x300E,0x64);	// ct_S5K5E2YA_bytewrite_cmos_sensor(0x3_width
	S5K5E2YA_bytewrite_cmos_sensor(0x3010,0x56);	// ct_s4_width
	S5K5E2YA_bytewrite_cmos_sensor(0x3019,0x03);	// ct_s4d_start
	S5K5E2YA_bytewrite_cmos_sensor(0x301A,0x00);	// ct_pbr_start
	S5K5E2YA_bytewrite_cmos_sensor(0x301B,0x05);	// ct_pbr_width
	S5K5E2YA_bytewrite_cmos_sensor(0x301C,0x00);	// ct_pbs_start
	S5K5E2YA_bytewrite_cmos_sensor(0x301D,0x1C);	// ct_pbs_width
	S5K5E2YA_bytewrite_cmos_sensor(0x301E,0x00);	// ct_pbr_ob_start
	S5K5E2YA_bytewrite_cmos_sensor(0x301F,0x0E);	// ct_pbr_ob_width
	S5K5E2YA_bytewrite_cmos_sensor(0x3020,0x00);	// ct_pbs_ob_start
	S5K5E2YA_bytewrite_cmos_sensor(0x3021,0x00);	// ct_pbs_ob_width
	S5K5E2YA_bytewrite_cmos_sensor(0x3022,0x0A);	// ct_cds_lim_start
	S5K5E2YA_bytewrite_cmos_sensor(0x3023,0x19);	// ct_crs_start
	S5K5E2YA_bytewrite_cmos_sensor(0x3024,0x00);	// ct_lp_hblk_cds_start (MSB)
	S5K5E2YA_bytewrite_cmos_sensor(0x3025,0x00);	// ct_lp_hblk_cds_start (LSB)
	S5K5E2YA_bytewrite_cmos_sensor(0x3026,0x00);	// ct_lp_hblk_cds_end (MSB)
	S5K5E2YA_bytewrite_cmos_sensor(0x3027,0x00);	// ct_lp_hblk_cds_end (LSB)
	S5K5E2YA_bytewrite_cmos_sensor(0x3028,0x16);	// ct_rmp_off_start
	S5K5E2YA_bytewrite_cmos_sensor(0x3015,0x00);	// ct_rmp_rst_start (MSB)
	S5K5E2YA_bytewrite_cmos_sensor(0x3016,0x6D);	// ct_rmp_rst_start (LSB)
	S5K5E2YA_bytewrite_cmos_sensor(0x3017,0x00);	// ct_rmp_sig_start (MSB)
	S5K5E2YA_bytewrite_cmos_sensor(0x3018,0x84);	// ct_rmp_sig_start (LSB)
	S5K5E2YA_bytewrite_cmos_sensor(0x302B,0x10);	// ct_cnt_margin
	S5K5E2YA_bytewrite_cmos_sensor(0x302C,0x0A);	// ct_rmp_per
	S5K5E2YA_bytewrite_cmos_sensor(0x302D,0x06);	// ct_cnt_ms_margin1
	S5K5E2YA_bytewrite_cmos_sensor(0x302E,0x05);	// ct_cnt_ms_margin2
	S5K5E2YA_bytewrite_cmos_sensor(0x302F,0x0E);	// rst_mx
	S5K5E2YA_bytewrite_cmos_sensor(0x3030,0x2F);	// sig_mx
	S5K5E2YA_bytewrite_cmos_sensor(0x3031,0x08);	// ct_latch_start
	S5K5E2YA_bytewrite_cmos_sensor(0x3032,0x05);	// ct_latch_width
	S5K5E2YA_bytewrite_cmos_sensor(0x3033,0x09);	// ct_hold_start
	S5K5E2YA_bytewrite_cmos_sensor(0x3034,0x05);	// ct_hold_width
	S5K5E2YA_bytewrite_cmos_sensor(0x3035,0x00);	// ct_lp_hblk_dbs_start (MSB)
	S5K5E2YA_bytewrite_cmos_sensor(0x3036,0x00);	// ct_lp_hblk_dbs_start (LSB)
	S5K5E2YA_bytewrite_cmos_sensor(0x3037,0x00);	// ct_lp_hblk_dbs_end (MSB)
	S5K5E2YA_bytewrite_cmos_sensor(0x3038,0x00);	// ct_lp_hblk_dbs_end (LSB)
	S5K5E2YA_bytewrite_cmos_sensor(0x3088,0x06);	// ct_lat_lsb_offset_start1
	S5K5E2YA_bytewrite_cmos_sensor(0x308A,0x08);	// ct_lat_lsb_offset_end1
	S5K5E2YA_bytewrite_cmos_sensor(0x308C,0x05);	// ct_lat_lsb_offset_start2
	S5K5E2YA_bytewrite_cmos_sensor(0x308E,0x07);	// ct_lat_lsb_offset_end2
	S5K5E2YA_bytewrite_cmos_sensor(0x3090,0x06);	// ct_conv_en_offset_start1
	S5K5E2YA_bytewrite_cmos_sensor(0x3092,0x08);	// ct_conv_en_offset_end1
	S5K5E2YA_bytewrite_cmos_sensor(0x3094,0x05);	// ct_conv_en_offset_start2
	S5K5E2YA_bytewrite_cmos_sensor(0x3096,0x21);	// ct_conv_en_offset_end2
	
	//CDS
	S5K5E2YA_bytewrite_cmos_sensor(0x3099,0x0E);  // cds_option ([3]:crs switch disable, S5K5E2YA_bytewrite_cmos_sensor(0x3,s4 strengthx16)
	S5K5E2YA_bytewrite_cmos_sensor(0x3070,0x10);  // comp1_bias (default:77)
	S5K5E2YA_bytewrite_cmos_sensor(0x3085,0x11);  // comp1_bias (gain1~4)
	S5K5E2YA_bytewrite_cmos_sensor(0x3086,0x01);  // comp1_bias (gain4~8) 
	
	
	
	//RMP
	S5K5E2YA_bytewrite_cmos_sensor(0x3064,0x00); // Multiple sampling(gainx8,x16)
	S5K5E2YA_bytewrite_cmos_sensor(0x3062,0x08); // off_rst
	
	//DBR
	S5K5E2YA_bytewrite_cmos_sensor(0x3061,0x11);  // dbr_tune_rd (default :08, 0E 3.02V)	3.1V
	S5K5E2YA_bytewrite_cmos_sensor(0x307B,0x20);  // dbr_tune_rgsl (default :08)
	
	//Bias sampling
	S5K5E2YA_bytewrite_cmos_sensor(0x3068,0x00); // RMP BP bias sampling [0]: Disable
	S5K5E2YA_bytewrite_cmos_sensor(0x3074,0x00); // Pixel bias sampling [2]:Default L
	S5K5E2YA_bytewrite_cmos_sensor(0x307D,0x00); // VREF sampling [0] : Disable
	S5K5E2YA_bytewrite_cmos_sensor(0x3045,0x01); // ct_opt_l1_start
	S5K5E2YA_bytewrite_cmos_sensor(0x3046,0x05); // ct_opt_l1_width
	S5K5E2YA_bytewrite_cmos_sensor(0x3047,0x78);
	
	//Smart PLA
	S5K5E2YA_bytewrite_cmos_sensor(0x307F,0xB1); //RDV_OPTION[5:4], RG default high
	S5K5E2YA_bytewrite_cmos_sensor(0x3098,0x01); //CDS_OPTION[16] SPLA-II enable
	S5K5E2YA_bytewrite_cmos_sensor(0x305C,0xF6); //lob_extension[6]
	
	S5K5E2YA_bytewrite_cmos_sensor(0x306B,0x10); // [3]bls_stx_en disable
	S5K5E2YA_bytewrite_cmos_sensor(0x3063,0x27); // ADC_SAT
	S5K5E2YA_bytewrite_cmos_sensor(0x320C,0x07); // ADC_MAX (def:076Ch --> 0700h)
	S5K5E2YA_bytewrite_cmos_sensor(0x320D,0x00);																	   
	S5K5E2YA_bytewrite_cmos_sensor(0x3400,0x01); // GAS bypass
	S5K5E2YA_bytewrite_cmos_sensor(0x3235,0x49); // L/F-ADLC on
	S5K5E2YA_bytewrite_cmos_sensor(0x3233,0x00); // D-pedestal L/F ADLC off (1FC0h)
	S5K5E2YA_bytewrite_cmos_sensor(0x3234,0x00);
	S5K5E2YA_bytewrite_cmos_sensor(0x3300,0x0C); //BPC On
	S5K5E2YA_bytewrite_cmos_sensor(0x0204,0x00); //Analog gain x1
	S5K5E2YA_bytewrite_cmos_sensor(0x0205,0x20);
	
	S5K5E2YA_bytewrite_cmos_sensor(0x3203,0x45);
	S5K5E2YA_bytewrite_cmos_sensor(0x3205,0x4D);
	S5K5E2YA_bytewrite_cmos_sensor(0x320B,0x40);
	S5K5E2YA_bytewrite_cmos_sensor(0x320C,0x06);
	S5K5E2YA_bytewrite_cmos_sensor(0x320D,0xC0);
	
	//LSC setting
	S5K5E2YA_bytewrite_cmos_sensor(0x340B,0x27);
	S5K5E2YA_bytewrite_cmos_sensor(0x340C,0x01);
	S5K5E2YA_bytewrite_cmos_sensor(0x340D,0xA3);
	S5K5E2YA_bytewrite_cmos_sensor(0x340E,0x9E);
	
	S5K5E2YA_bytewrite_cmos_sensor(0x3401,0x50);
	S5K5E2YA_bytewrite_cmos_sensor(0x3402,0x3C);
	S5K5E2YA_bytewrite_cmos_sensor(0x3403,0x03);
	S5K5E2YA_bytewrite_cmos_sensor(0x3404,0x33);
	S5K5E2YA_bytewrite_cmos_sensor(0x3405,0x04);
	S5K5E2YA_bytewrite_cmos_sensor(0x3406,0x44); 
	S5K5E2YA_bytewrite_cmos_sensor(0x3458,0x03);
	S5K5E2YA_bytewrite_cmos_sensor(0x3459,0x33);
	S5K5E2YA_bytewrite_cmos_sensor(0x345A,0x04);
	S5K5E2YA_bytewrite_cmos_sensor(0x345B,0x44);

//	S5K5E2YA_bytewrite_cmos_sensor(0x3200,0x00);
	
	// streaming ON
	S5K5E2YA_bytewrite_cmos_sensor(0x0100,0x01); 
	
	mDELAY(50);
}

void S5K5E2YACaptureSetting(void)
{
	SENSORDB("enter s5k5e2ya capture setting\n");
	
	// Reset for operation																		   
	S5K5E2YA_bytewrite_cmos_sensor(0x0100,0x00); //stream off
	
	// Clock Setting
	S5K5E2YA_bytewrite_cmos_sensor(0x0305,0x06); //PLLP (def:5)
	S5K5E2YA_bytewrite_cmos_sensor(0x0306,0x00);
	S5K5E2YA_bytewrite_cmos_sensor(0x0307,0xE0); //PLLM (def:CCh 204d --> B3h 179d)
	S5K5E2YA_bytewrite_cmos_sensor(0x3C1F,0x00); //PLLS 
	
	S5K5E2YA_bytewrite_cmos_sensor(0x0820,0x03); // requested link bit rate mbps : (def:3D3h 979d --> 35Bh 859d)
	S5K5E2YA_bytewrite_cmos_sensor(0x0821,0x80); 
	S5K5E2YA_bytewrite_cmos_sensor(0x3C1C,0x58); //dbr_div
	 
	
	// Size Setting
	S5K5E2YA_bytewrite_cmos_sensor(0x0340,0x07); // frame_length_lines : def. 1962d (7C2 --> 7A6 Mimnimum 23 lines)
	S5K5E2YA_bytewrite_cmos_sensor(0x0341,0xE9);
	S5K5E2YA_bytewrite_cmos_sensor(0x0342,0x0B); // line_length_pck : def. 2950d 
	S5K5E2YA_bytewrite_cmos_sensor(0x0343,0x86);
	
	S5K5E2YA_bytewrite_cmos_sensor(0x0344,0x00); // x_addr_start
	S5K5E2YA_bytewrite_cmos_sensor(0x0345,0x08); 
	S5K5E2YA_bytewrite_cmos_sensor(0x0346,0x00); // y_addr_start
	S5K5E2YA_bytewrite_cmos_sensor(0x0347,0x08); 
	S5K5E2YA_bytewrite_cmos_sensor(0x0348,0x0A); // x_addr_end : def. 2575d  
	S5K5E2YA_bytewrite_cmos_sensor(0x0349,0x07); 
	S5K5E2YA_bytewrite_cmos_sensor(0x034A,0x07); // y_addr_end : def. 1936d
	S5K5E2YA_bytewrite_cmos_sensor(0x034B,0x87); 
	S5K5E2YA_bytewrite_cmos_sensor(0x034C,0x0A); // x_output size : def. 2560d 
	S5K5E2YA_bytewrite_cmos_sensor(0x034D,0x00); 
	S5K5E2YA_bytewrite_cmos_sensor(0x034E,0x07); // y_output size : def. 1920d
	S5K5E2YA_bytewrite_cmos_sensor(0x034F,0x80); 
	
	//Digital Binning(default)
	S5K5E2YA_bytewrite_cmos_sensor(0x0900,0x00);	//0x0 Binning
	S5K5E2YA_bytewrite_cmos_sensor(0x0901,0x20);
	S5K5E2YA_bytewrite_cmos_sensor(0x0387,0x01);
	
	
	//Integration time	
	S5K5E2YA_bytewrite_cmos_sensor(0x0202,0x02);  // coarse integration
	S5K5E2YA_bytewrite_cmos_sensor(0x0203,0x00);
	
	//Analog Timing Tuning (140117)
	S5K5E2YA_bytewrite_cmos_sensor(0x3000,0x04);	// ct_ld_start
	S5K5E2YA_bytewrite_cmos_sensor(0x3002,0x03);	// ct_sl_start
	S5K5E2YA_bytewrite_cmos_sensor(0x3003,0x04);	// ct_sl_margin
	S5K5E2YA_bytewrite_cmos_sensor(0x3004,0x02);	// ct_rx_start
	S5K5E2YA_bytewrite_cmos_sensor(0x3005,0x00);	// ct_rx_margin (MSB)
	S5K5E2YA_bytewrite_cmos_sensor(0x3006,0x10);	// ct_rx_margin (LSB)
	S5K5E2YA_bytewrite_cmos_sensor(0x3007,0x03);	// ct_tx_start
	S5K5E2YA_bytewrite_cmos_sensor(0x3008,0x46);	// ct_tx_width
	S5K5E2YA_bytewrite_cmos_sensor(0x0200,0x04);	// (fine_integ_time) (MSB)
	S5K5E2YA_bytewrite_cmos_sensor(0x0201,0x98);	// (fine_integ_time) (LSB)
	S5K5E2YA_bytewrite_cmos_sensor(0x3039,0x05);	// cintc1_margin
	S5K5E2YA_bytewrite_cmos_sensor(0x303A,0x05);	// cintc2_margin
	S5K5E2YA_bytewrite_cmos_sensor(0x303B,0x00);	// offs_sh
	S5K5E2YA_bytewrite_cmos_sensor(0x3009,0x05);	// ct_srx_margin
	S5K5E2YA_bytewrite_cmos_sensor(0x300A,0x46);	// ct_stx_width
	S5K5E2YA_bytewrite_cmos_sensor(0x300B,0x2E);	// ct_dstx_width
	S5K5E2YA_bytewrite_cmos_sensor(0x300C,0x10);	// ct_stx2dstx
	S5K5E2YA_bytewrite_cmos_sensor(0x3012,0x05);	// ct_cds_start
	S5K5E2YA_bytewrite_cmos_sensor(0x3013,0x00);	// ct_s1s_start
	S5K5E2YA_bytewrite_cmos_sensor(0x3014,0x1C);	// ct_s1s_end
	S5K5E2YA_bytewrite_cmos_sensor(0x300E,0x64);	// ct_S5K5E2YA_bytewrite_cmos_sensor(0x3_width
	S5K5E2YA_bytewrite_cmos_sensor(0x3010,0x56);	// ct_s4_width
	S5K5E2YA_bytewrite_cmos_sensor(0x3019,0x03);	// ct_s4d_start
	S5K5E2YA_bytewrite_cmos_sensor(0x301A,0x00);	// ct_pbr_start
	S5K5E2YA_bytewrite_cmos_sensor(0x301B,0x05);	// ct_pbr_width
	S5K5E2YA_bytewrite_cmos_sensor(0x301C,0x00);	// ct_pbs_start
	S5K5E2YA_bytewrite_cmos_sensor(0x301D,0x1C);	// ct_pbs_width
	S5K5E2YA_bytewrite_cmos_sensor(0x301E,0x00);	// ct_pbr_ob_start
	S5K5E2YA_bytewrite_cmos_sensor(0x301F,0x0E);	// ct_pbr_ob_width
	S5K5E2YA_bytewrite_cmos_sensor(0x3020,0x00);	// ct_pbs_ob_start
	S5K5E2YA_bytewrite_cmos_sensor(0x3021,0x00);	// ct_pbs_ob_width
	S5K5E2YA_bytewrite_cmos_sensor(0x3022,0x0A);	// ct_cds_lim_start
	S5K5E2YA_bytewrite_cmos_sensor(0x3023,0x19);	// ct_crs_start
	S5K5E2YA_bytewrite_cmos_sensor(0x3024,0x00);	// ct_lp_hblk_cds_start (MSB)
	S5K5E2YA_bytewrite_cmos_sensor(0x3025,0x00);	// ct_lp_hblk_cds_start (LSB)
	S5K5E2YA_bytewrite_cmos_sensor(0x3026,0x00);	// ct_lp_hblk_cds_end (MSB)
	S5K5E2YA_bytewrite_cmos_sensor(0x3027,0x00);	// ct_lp_hblk_cds_end (LSB)
	S5K5E2YA_bytewrite_cmos_sensor(0x3028,0x16);	// ct_rmp_off_start
	S5K5E2YA_bytewrite_cmos_sensor(0x3015,0x00);	// ct_rmp_rst_start (MSB)
	S5K5E2YA_bytewrite_cmos_sensor(0x3016,0x6D);	// ct_rmp_rst_start (LSB)
	S5K5E2YA_bytewrite_cmos_sensor(0x3017,0x00);	// ct_rmp_sig_start (MSB)
	S5K5E2YA_bytewrite_cmos_sensor(0x3018,0x84);	// ct_rmp_sig_start (LSB)
	S5K5E2YA_bytewrite_cmos_sensor(0x302B,0x10);	// ct_cnt_margin
	S5K5E2YA_bytewrite_cmos_sensor(0x302C,0x0A);	// ct_rmp_per
	S5K5E2YA_bytewrite_cmos_sensor(0x302D,0x06);	// ct_cnt_ms_margin1
	S5K5E2YA_bytewrite_cmos_sensor(0x302E,0x05);	// ct_cnt_ms_margin2
	S5K5E2YA_bytewrite_cmos_sensor(0x302F,0x0E);	// rst_mx
	S5K5E2YA_bytewrite_cmos_sensor(0x3030,0x2F);	// sig_mx
	S5K5E2YA_bytewrite_cmos_sensor(0x3031,0x08);	// ct_latch_start
	S5K5E2YA_bytewrite_cmos_sensor(0x3032,0x05);	// ct_latch_width
	S5K5E2YA_bytewrite_cmos_sensor(0x3033,0x09);	// ct_hold_start
	S5K5E2YA_bytewrite_cmos_sensor(0x3034,0x05);	// ct_hold_width
	S5K5E2YA_bytewrite_cmos_sensor(0x3035,0x00);	// ct_lp_hblk_dbs_start (MSB)
	S5K5E2YA_bytewrite_cmos_sensor(0x3036,0x00);	// ct_lp_hblk_dbs_start (LSB)
	S5K5E2YA_bytewrite_cmos_sensor(0x3037,0x00);	// ct_lp_hblk_dbs_end (MSB)
	S5K5E2YA_bytewrite_cmos_sensor(0x3038,0x00);	// ct_lp_hblk_dbs_end (LSB)
	S5K5E2YA_bytewrite_cmos_sensor(0x3088,0x06);	// ct_lat_lsb_offset_start1
	S5K5E2YA_bytewrite_cmos_sensor(0x308A,0x08);	// ct_lat_lsb_offset_end1
	S5K5E2YA_bytewrite_cmos_sensor(0x308C,0x05);	// ct_lat_lsb_offset_start2
	S5K5E2YA_bytewrite_cmos_sensor(0x308E,0x07);	// ct_lat_lsb_offset_end2
	S5K5E2YA_bytewrite_cmos_sensor(0x3090,0x06);	// ct_conv_en_offset_start1
	S5K5E2YA_bytewrite_cmos_sensor(0x3092,0x08);	// ct_conv_en_offset_end1
	S5K5E2YA_bytewrite_cmos_sensor(0x3094,0x05);	// ct_conv_en_offset_start2
	S5K5E2YA_bytewrite_cmos_sensor(0x3096,0x21);	// ct_conv_en_offset_end2
	
	//CDS
	S5K5E2YA_bytewrite_cmos_sensor(0x3099,0x0E);  // cds_option ([3]:crs switch disable, S5K5E2YA_bytewrite_cmos_sensor(0x3,s4 strengthx16)
	S5K5E2YA_bytewrite_cmos_sensor(0x3070,0x10);  // comp1_bias (default:77)
	S5K5E2YA_bytewrite_cmos_sensor(0x3085,0x11);  // comp1_bias (gain1~4)
	S5K5E2YA_bytewrite_cmos_sensor(0x3086,0x01);  // comp1_bias (gain4~8) 
	
	
	
	//RMP
	S5K5E2YA_bytewrite_cmos_sensor(0x3064,0x00); // Multiple sampling(gainx8,x16)
	S5K5E2YA_bytewrite_cmos_sensor(0x3062,0x08); // off_rst
	
	//DBR
	S5K5E2YA_bytewrite_cmos_sensor(0x3061,0x11);  // dbr_tune_rd (default :08, 0E 3.02V)	3.1V
	S5K5E2YA_bytewrite_cmos_sensor(0x307B,0x20);  // dbr_tune_rgsl (default :08)
	
	//Bias sampling
	S5K5E2YA_bytewrite_cmos_sensor(0x3068,0x00); // RMP BP bias sampling [0]: Disable
	S5K5E2YA_bytewrite_cmos_sensor(0x3074,0x00); // Pixel bias sampling [2]:Default L
	S5K5E2YA_bytewrite_cmos_sensor(0x307D,0x00); // VREF sampling [0] : Disable
	S5K5E2YA_bytewrite_cmos_sensor(0x3045,0x01); // ct_opt_l1_start
	S5K5E2YA_bytewrite_cmos_sensor(0x3046,0x05); // ct_opt_l1_width
	S5K5E2YA_bytewrite_cmos_sensor(0x3047,0x78);
	
	//Smart PLA
	S5K5E2YA_bytewrite_cmos_sensor(0x307F,0xB1); //RDV_OPTION[5:4], RG default high
	S5K5E2YA_bytewrite_cmos_sensor(0x3098,0x01); //CDS_OPTION[16] SPLA-II enable
	S5K5E2YA_bytewrite_cmos_sensor(0x305C,0xF6); //lob_extension[6]
	
	S5K5E2YA_bytewrite_cmos_sensor(0x306B,0x10); // [3]bls_stx_en disable
	S5K5E2YA_bytewrite_cmos_sensor(0x3063,0x27); // ADC_SAT
	S5K5E2YA_bytewrite_cmos_sensor(0x320C,0x07); // ADC_MAX (def:076Ch --> 0700h)
	S5K5E2YA_bytewrite_cmos_sensor(0x320D,0x00);																	   
	S5K5E2YA_bytewrite_cmos_sensor(0x3400,0x01); // GAS bypass
	S5K5E2YA_bytewrite_cmos_sensor(0x3235,0x49); // L/F-ADLC on
	S5K5E2YA_bytewrite_cmos_sensor(0x3233,0x00); // D-pedestal L/F ADLC off (1FC0h)
	S5K5E2YA_bytewrite_cmos_sensor(0x3234,0x00);
	S5K5E2YA_bytewrite_cmos_sensor(0x3300,0x0C); //BPC On
	S5K5E2YA_bytewrite_cmos_sensor(0x0204,0x00); //Analog gain x1
	S5K5E2YA_bytewrite_cmos_sensor(0x0205,0x20);
	
	S5K5E2YA_bytewrite_cmos_sensor(0x3203,0x45);
	S5K5E2YA_bytewrite_cmos_sensor(0x3205,0x4D);
	S5K5E2YA_bytewrite_cmos_sensor(0x320B,0x40);
	S5K5E2YA_bytewrite_cmos_sensor(0x320C,0x06);
	S5K5E2YA_bytewrite_cmos_sensor(0x320D,0xC0);
	
	//LSC settting for 2560x1920
	S5K5E2YA_bytewrite_cmos_sensor(0x340B,0x00);
	S5K5E2YA_bytewrite_cmos_sensor(0x340C,0x00);
	S5K5E2YA_bytewrite_cmos_sensor(0x340D,0x00);
	S5K5E2YA_bytewrite_cmos_sensor(0x340E,0x00);
	
	S5K5E2YA_bytewrite_cmos_sensor(0x3401,0x50);
	S5K5E2YA_bytewrite_cmos_sensor(0x3402,0x3C);
	S5K5E2YA_bytewrite_cmos_sensor(0x3403,0x03);
	S5K5E2YA_bytewrite_cmos_sensor(0x3404,0x33);
	S5K5E2YA_bytewrite_cmos_sensor(0x3405,0x04);
	S5K5E2YA_bytewrite_cmos_sensor(0x3406,0x44); 
	S5K5E2YA_bytewrite_cmos_sensor(0x3458,0x03);
	S5K5E2YA_bytewrite_cmos_sensor(0x3459,0x33);
	S5K5E2YA_bytewrite_cmos_sensor(0x345A,0x04);
	S5K5E2YA_bytewrite_cmos_sensor(0x345B,0x44);

	//S5K5E2YA_bytewrite_cmos_sensor(0x3200,0x00);
	
	// streaming ON
	S5K5E2YA_bytewrite_cmos_sensor(0x0100,0x01); 
	
		mDELAY(50);
	
	
}

   /*  S5K5E2YAInitSetting  */

/*************************************************************************
* FUNCTION
*   S5K5E2YAOpen
*
* DESCRIPTION
*   This function initialize the registers of CMOS sensor
*
* PARAMETERS
*   None
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/

UINT32 S5K5E2YAOpen(void)
{

	volatile signed int i;
	kal_uint32 sensor_id = 0;

	SENSORDB("enter s5k5e2ya open\n");

	sensor_id = (S5K5E2YA_read_cmos_sensor(0x0000)<<8)|(S5K5E2YA_read_cmos_sensor(0x0001));
	SENSORDB("Read sensor ID=0x%x\n",sensor_id);
	if(S5K5E2YA_SENSOR_ID != sensor_id)
	{
		return ERROR_SENSOR_CONNECT_FAIL;
	}


	spin_lock(&s5k5e2yamipiraw_drv_lock);
	s5k5e2ya.sensorMode = SENSOR_MODE_INIT;
	s5k5e2ya.S5K5E2YAAutoFlickerMode = KAL_FALSE;
	s5k5e2ya.S5K5E2YAVideoMode = KAL_FALSE;	
	s5k5e2ya.DummyLines= 0;
	s5k5e2ya.DummyPixels= 0;
	s5k5e2ya.pvPclk = SENSOR_PCLK_PREVIEW; //171.8MHz 
	s5k5e2ya.m_vidPclk= SENSOR_PCLK_VIDEO;
	s5k5e2ya.capPclk= SENSOR_PCLK_CAPTURE;
	s5k5e2ya.maxExposureLines = S5K5E2YA_PV_PERIOD_LINE_NUMS;
	s5k5e2ya.FixedFrameLength = S5K5E2YA_PV_PERIOD_LINE_NUMS;
	s5k5e2ya.sensorGain = 0x10; //base gain
	s5k5e2ya.pvGain = 0x10;
	s_S5K5E2YACurrentScenarioId = MSDK_SCENARIO_ID_CAMERA_PREVIEW;
	spin_unlock(&s5k5e2yamipiraw_drv_lock);
	
    return ERROR_NONE;
}

/*************************************************************************
* FUNCTION
*   S5K5E2YAGetSensorID
*
* DESCRIPTION
*   This function get the sensor ID
*
* PARAMETERS
*   *sensorID : return the sensor ID
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 S5K5E2YAGetSensorID(UINT32 *sensorID)
{
	SENSORDB("enter s5k5e2ya getsensorid\n");
    *sensorID = (S5K5E2YA_read_cmos_sensor(0x0000)<<8) | (S5K5E2YA_read_cmos_sensor(0x0001));
	SENSORDB("Read sensor ID=0x%x\n",*sensorID);
	if (*sensorID != S5K5E2YA_SENSOR_ID)
	{
        *sensorID = 0xFFFFFFFF;
        return ERROR_SENSOR_CONNECT_FAIL;
   	 }
	
	SENSORDB("leave getsensorid\n");
    return ERROR_NONE;
}



/*************************************************************************
* FUNCTION
*   S5K5E2YA_read_shutter
*
* DESCRIPTION
*   This function to  Get exposure time.
*
* PARAMETERS
*   None
*
* RETURNS
*   shutter : exposured lines
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 S5K5E2YA_read_shutter(void)
{
	return ((S5K5E2YA_read_cmos_sensor(0x0202)<<8)|S5K5E2YA_read_cmos_sensor(0x0203));   // smiaRegs_rw_integration_time_coarse_integration_time 
	
}

/*************************************************************************
* FUNCTION
*   S5K5E2YA_night_mode
*
* DESCRIPTION
*   This function night mode of S5K5E2YA.
*
* PARAMETERS
*   none
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void S5K5E2YA_NightMode(kal_bool bEnable)
{
}/*	S5K5E2YA_NightMode */



/*************************************************************************
* FUNCTION
*   S5K5E2YAClose
*
* DESCRIPTION
*   This function is to turn off sensor module power.
*
* PARAMETERS
*   None
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 S5K5E2YAClose(void)
{
    return ERROR_NONE;
}	/* S5K5E2YAClose() */

void S5K5E2YASetFlipMirror(kal_int32 imgMirror)
{
	SENSORDB("enter s5k5e2ya setflip mirror\n");
	SENSORDB("imgMirror=%d\n",imgMirror);
	spin_lock(&s5k5e2yamipiraw_drv_lock);
	s5k5e2ya.imgMirror = imgMirror; //(imgMirror+IMAGE_HV_MIRROR)%(IMAGE_HV_MIRROR+1);
	spin_unlock(&s5k5e2yamipiraw_drv_lock);
	
    switch (imgMirror)
    {
        case IMAGE_H_MIRROR://IMAGE_NORMAL:  bit0 mirror,   bit1 flip.
			S5K5E2YA_bytewrite_cmos_sensor(0x0101,0x01  ); //morror
            break;
        case IMAGE_NORMAL://IMAGE_V_MIRROR:
			S5K5E2YA_bytewrite_cmos_sensor(0x0101,0x00  ); 
            break;
        case IMAGE_HV_MIRROR://IMAGE_H_MIRROR:
			S5K5E2YA_bytewrite_cmos_sensor(0x0101,0x03  );   //morror +flip
            break;
        case IMAGE_V_MIRROR://IMAGE_HV_MIRROR:
			S5K5E2YA_bytewrite_cmos_sensor(0x0101,0x02  ); //flip
            break;
    }
}


/*************************************************************************
* FUNCTION
*   S5K5E2YAPreview
*
* DESCRIPTION
*   This function start the sensor preview.
*
* PARAMETERS
*   *image_window : address pointer of pixel numbers in one period of HSYNC
*  *sensor_config_data : address pointer of line numbers in one period of VSYNC
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 S5K5E2YAPreview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                                                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	SENSORDB("enter s5k5e2ya preview\n");

	spin_lock(&s5k5e2yamipiraw_drv_lock);
	s5k5e2ya.sensorMode = SENSOR_MODE_PREVIEW; // Need set preview setting after capture mode
	//S5K5E2YA_FeatureControl_PERIOD_PixelNum=S5K5E2YA_PV_PERIOD_PIXEL_NUMS+ s5k5e2ya.DummyPixels;
	//S5K5E2YA_FeatureControl_PERIOD_LineNum=S5K5E2YA_PV_PERIOD_LINE_NUMS+s5k5e2ya.DummyLines;
	spin_unlock(&s5k5e2yamipiraw_drv_lock);

	S5K5E2YAPreviewSetting();

	S5K5E2YA_SetDummy(0,0);
	//set mirror & flip
	S5K5E2YASetFlipMirror(sensor_config_data->SensorImageMirror);
	SENSORDB("leave preview\n");
	
    return ERROR_NONE;
}	/* S5K5E2YAPreview() */

UINT32 S5K5E2YAVideo(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                                                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	SENSORDB("enter s5k5e2ya video\n");

	spin_lock(&s5k5e2yamipiraw_drv_lock);
	s5k5e2ya.sensorMode = SENSOR_MODE_VIDEO; // Need set preview setting after capture mode
	//S5K5E2YA_FeatureControl_PERIOD_PixelNum=S5K5E2YA_PV_PERIOD_PIXEL_NUMS+ s5k5e2ya.DummyPixels;
	//S5K5E2YA_FeatureControl_PERIOD_LineNum=S5K5E2YA_PV_PERIOD_LINE_NUMS+s5k5e2ya.DummyLines;
	spin_unlock(&s5k5e2yamipiraw_drv_lock);

	S5K5E2YAVideoSetting();
	
	S5K5E2YA_write_shutter(s5k5e2ya.shutter);
	S5K5E2YA_SetGain(s5k5e2ya.pvGain);

	S5K5E2YA_SetDummy(0,0);
	//set mirror & flip
	S5K5E2YASetFlipMirror(sensor_config_data->SensorImageMirror);
	SENSORDB("leave video\n");
	
    return ERROR_NONE;
}	/* S5K5E2YAPreview() */



UINT32 S5K5E2YACapture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                                                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	SENSORDB("enter s5k5e2ya capture\n");
	SENSORDB("sensorMode=%d\n",s5k5e2ya.sensorMode);
		
	// Full size setting	
	S5K5E2YACaptureSetting();

	S5K5E2YA_SetDummy(0,0);
	spin_lock(&s5k5e2yamipiraw_drv_lock);
	s5k5e2ya.sensorMode = SENSOR_MODE_CAPTURE;	
	spin_unlock(&s5k5e2yamipiraw_drv_lock);
	
	S5K5E2YASetFlipMirror(sensor_config_data->SensorImageMirror);
	
    return ERROR_NONE;
}	/* S5K5E2YACapture() */

UINT32 S5K5E2YAZSDPreview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                                                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	SENSORDB("enter s5k5e2yxa zsd preview\n");	
	// Full size setting
	S5K5E2YACaptureSetting();

	spin_lock(&s5k5e2yamipiraw_drv_lock);
	s5k5e2ya.sensorMode = SENSOR_MODE_ZSD_PREVIEW;	
	//S5K5E2YXA_FeatureControl_PERIOD_PixelNum = S5K5E2YXA_FULL_PERIOD_PIXEL_NUMS + s5k5e2yxa.DummyPixels;
	//S5K5E2YXA_FeatureControl_PERIOD_LineNum = S5K5E2YXA_FULL_PERIOD_LINE_NUMS + s5k5e2yxa.DummyLines;
	spin_unlock(&s5k5e2yamipiraw_drv_lock);
	S5K5E2YA_SetDummy(0,0);
	
	S5K5E2YASetFlipMirror(sensor_config_data->SensorImageMirror);
	
    return ERROR_NONE;
}


UINT32 S5K5E2YAGetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution)
{
	
    SENSORDB("enter s5k5e2ya getresolution\n");
	pSensorResolution->SensorPreviewWidth	= 	S5K5E2YA_IMAGE_SENSOR_PV_WIDTH;
	pSensorResolution->SensorPreviewHeight	= 	S5K5E2YA_IMAGE_SENSOR_PV_HEIGHT;
	pSensorResolution->SensorVideoWidth		=	S5K5E2YA_IMAGE_SENSOR_VIDEO_WIDTH;
	pSensorResolution->SensorVideoHeight 	=	S5K5E2YA_IMAGE_SENSOR_VIDEO_HEIGHT;
	pSensorResolution->SensorFullWidth		= 	S5K5E2YA_IMAGE_SENSOR_FULL_WIDTH;
	pSensorResolution->SensorFullHeight		= 	S5K5E2YA_IMAGE_SENSOR_FULL_HEIGHT;
    return ERROR_NONE;
}   /* S5K5E2YAGetResolution() */

UINT32 S5K5E2YAGetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId,
                                                MSDK_SENSOR_INFO_STRUCT *pSensorInfo,
                                                MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
	SENSORDB("enter s5k5e2ya getinfo\n");
	SENSORDB("SCENARIO id=%d\n", ScenarioId);
	switch(s_S5K5E2YACurrentScenarioId)
	{
    	case MSDK_SCENARIO_ID_CAMERA_ZSD:
			pSensorInfo->SensorPreviewResolutionX= S5K5E2YA_IMAGE_SENSOR_FULL_WIDTH;
			pSensorInfo->SensorPreviewResolutionY= S5K5E2YA_IMAGE_SENSOR_FULL_HEIGHT;
			break;
		default:
			pSensorInfo->SensorPreviewResolutionX= S5K5E2YA_IMAGE_SENSOR_PV_WIDTH;
			pSensorInfo->SensorPreviewResolutionY= S5K5E2YA_IMAGE_SENSOR_PV_HEIGHT;
			break;
	}

	pSensorInfo->SensorFullResolutionX= S5K5E2YA_IMAGE_SENSOR_FULL_WIDTH;
    pSensorInfo->SensorFullResolutionY= S5K5E2YA_IMAGE_SENSOR_FULL_HEIGHT;

	pSensorInfo->SensorOutputDataFormat= SENSOR_OUTPUT_FORMAT_RAW_Gr;

    pSensorInfo->SensorClockPolarity =SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorClockFallingPolarity=SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;

    pSensorInfo->SensroInterfaceType=SENSOR_INTERFACE_TYPE_MIPI;

    pSensorInfo->CaptureDelayFrame = 1;
    pSensorInfo->PreviewDelayFrame = 1;
    pSensorInfo->VideoDelayFrame = 2;

    pSensorInfo->SensorDrivingCurrent = ISP_DRIVING_8MA;
    pSensorInfo->AEShutDelayFrame = 0;//0;		    /* The frame of setting shutter default 0 for TG int */
    pSensorInfo->AESensorGainDelayFrame = 1 ;//0;     /* The frame of setting sensor gain */
    pSensorInfo->AEISPGainDelayFrame = 2;

	pSensorInfo->SensorClockFreq=24;  //26
	pSensorInfo->SensorClockRisingCount= 0;
	pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_2_LANE;
	pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 14;
	pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	pSensorInfo->SensorPacketECCOrder = 1;
	
    switch (ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:  
            pSensorInfo->SensorGrabStartX = S5K5E2YA_PV_X_START;
            pSensorInfo->SensorGrabStartY = S5K5E2YA_PV_Y_START;
		break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:         
			pSensorInfo->SensorGrabStartX = S5K5E2YA_VIDEO_X_START;
			pSensorInfo->SensorGrabStartY = S5K5E2YA_VIDEO_Y_START;     
        break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
            pSensorInfo->SensorGrabStartX = S5K5E2YA_FULL_X_START;	//2*S5K5E2YA_IMAGE_SENSOR_PV_STARTX;
            pSensorInfo->SensorGrabStartY = S5K5E2YA_FULL_Y_START;	//2*S5K5E2YA_IMAGE_SENSOR_PV_STARTY;           
        break;
        default:
            pSensorInfo->SensorGrabStartX = S5K5E2YA_PV_X_START;
            pSensorInfo->SensorGrabStartY = S5K5E2YA_PV_Y_START;
            break;
    }

    memcpy(pSensorConfigData, &S5K5E2YASensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));

    return ERROR_NONE;
}   /* S5K5E2YAGetInfo() */


UINT32 S5K5E2YAControl(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow,
                                                MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
	SENSORDB("enter s5k5e2ya control\n");
	spin_lock(&s5k5e2yamipiraw_drv_lock);
	s_S5K5E2YACurrentScenarioId = ScenarioId;
	s5k5e2ya.FixedFrameLength = GetScenarioFramelength();
	spin_unlock(&s5k5e2yamipiraw_drv_lock);

	SENSORDB("s_S5K5E2YACurrentScenarioId=%d\n",s_S5K5E2YACurrentScenarioId);
	
    switch (ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            S5K5E2YAPreview(pImageWindow, pSensorConfigData);
        break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			S5K5E2YAVideo(pImageWindow, pSensorConfigData);
		break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			S5K5E2YACapture(pImageWindow, pSensorConfigData);
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
			S5K5E2YAZSDPreview(pImageWindow,pSensorConfigData);
        break;
        default:
            return ERROR_INVALID_SCENARIO_ID;

    }	
    return ERROR_NONE;
} /* S5K5E2YAControl() */

UINT32 S5K5E2YASetVideoMode(UINT16 u2FrameRate)
{
	s5k5e2ya.sensorMode=MSDK_SCENARIO_ID_VIDEO_PREVIEW;
	SENSORDB("enter s5k5 setvideomode");
    SENSORDB("u2FrameRate=%d,sensorMode=%d\n", u2FrameRate,s5k5e2ya.sensorMode);
	
	if(0==u2FrameRate) //do not fix frame rate 
	{
		spin_lock(&s5k5e2yamipiraw_drv_lock);
		s5k5e2ya.FixedFrameLength = GetScenarioFramelength();
		spin_unlock(&s5k5e2yamipiraw_drv_lock);
		SENSORDB("s5k5e2ya.FixedFrameLength=%d\n",s5k5e2ya.FixedFrameLength);
		return ERROR_NONE;
	}
	
	S5K5E2YAMIPISetMaxFramerateByScenario(MSDK_SCENARIO_ID_VIDEO_PREVIEW,u2FrameRate*10);
	return ERROR_NONE;
}

static void S5K5E2YASetMaxFrameRate(UINT16 u2FrameRate)
{
	kal_uint16 FrameHeight;
		
	SENSORDB("[S5K4H5YX] [S5K4H5YXMIPISetMaxFrameRate] u2FrameRate=%d\n",u2FrameRate);

	if(SENSOR_MODE_PREVIEW == s5k5e2ya.sensorMode)
	{
		FrameHeight= (10 * s5k5e2ya.pvPclk) / u2FrameRate / S5K5E2YA_PV_PERIOD_PIXEL_NUMS;
		FrameHeight = (FrameHeight > S5K5E2YA_PV_PERIOD_LINE_NUMS) ? FrameHeight : S5K5E2YA_PV_PERIOD_LINE_NUMS;
	}
	else if(SENSOR_MODE_CAPTURE== s5k5e2ya.sensorMode || SENSOR_MODE_ZSD_PREVIEW == s5k5e2ya.sensorMode)
	{
		FrameHeight= (10 * s5k5e2ya.capPclk) / u2FrameRate / S5K5E2YA_FULL_PERIOD_PIXEL_NUMS;
		FrameHeight = (FrameHeight > S5K5E2YA_FULL_PERIOD_LINE_NUMS) ? FrameHeight : S5K5E2YA_FULL_PERIOD_LINE_NUMS;
	}
	else
	{
		FrameHeight = (10 * s5k5e2ya.m_vidPclk) / u2FrameRate / S5K5E2YA_VIDEO_PERIOD_PIXEL_NUMS;
		FrameHeight = (FrameHeight > S5K5E2YA_VIDEO_PERIOD_LINE_NUMS) ? FrameHeight : S5K5E2YA_VIDEO_PERIOD_LINE_NUMS;
	}
	SENSORDB("[S5K4H5YX] [S5K4H5YXMIPISetMaxFrameRate] FrameHeight=%d",FrameHeight);
	SetFramelength(FrameHeight); /* modify dummy_pixel must gen AE table again */	
}


UINT32 S5K5E2YASetAutoFlickerMode(kal_bool bEnable, UINT16 u2FrameRate)
{
	if(bEnable) 
	{
		SENSORDB("[S5K4H5YX] [S5K4H5YXSetAutoFlickerMode] enable\n");
		spin_lock(&s5k5e2yamipiraw_drv_lock);
		s5k5e2ya.S5K5E2YAAutoFlickerMode = KAL_TRUE;
		spin_unlock(&s5k5e2yamipiraw_drv_lock);

		if(u2FrameRate == 300)
			S5K5E2YASetMaxFrameRate(296);
		else if(u2FrameRate == 150)
			S5K5E2YASetMaxFrameRate(148);
    } 
	else 
	{
    	SENSORDB("[S5K4H5YX] [S5K4H5YXSetAutoFlickerMode] disable\n");
    	spin_lock(&s5k5e2yamipiraw_drv_lock);
        s5k5e2ya.S5K5E2YAAutoFlickerMode = KAL_FALSE;
		spin_unlock(&s5k5e2yamipiraw_drv_lock);
    }
    return ERROR_NONE;
}


UINT32 S5K5E2YASetTestPatternMode(kal_bool bEnable)
{
    SENSORDB("bEnable=%d\n", bEnable);
	if(bEnable) 
	{
		S5K5E2YA_bytewrite_cmos_sensor(0x0601,0x01);
	}
	else        
	{
		S5K5E2YA_bytewrite_cmos_sensor(0x0601,0x00);	
	}

	S5K5E2YA_bytewrite_cmos_sensor(0x3200,0x00);	

    return ERROR_NONE;
}

UINT32 S5K5E2YAMIPISetMaxFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 frameRate) 
{
	kal_uint16 frameLength = 0;
		
	SENSORDB("scenarioId=%d,frameRate=%d\n",scenarioId,frameRate);
	switch (scenarioId) 
	{
		//SetDummy() has to switch scenarioId again, so we do not use it here
		//when SetDummy() is ok, we'll switch to using SetDummy()
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			frameLength = (s5k5e2ya.pvPclk)/frameRate*10/S5K5E2YA_PV_PERIOD_PIXEL_NUMS;
			frameLength = (frameLength>S5K5E2YA_PV_PERIOD_LINE_NUMS)?(frameLength):(S5K5E2YA_PV_PERIOD_LINE_NUMS);				
		break;			
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			frameLength = (s5k5e2ya.m_vidPclk)/frameRate*10/S5K5E2YA_VIDEO_PERIOD_PIXEL_NUMS;
			frameLength = (frameLength>S5K5E2YA_VIDEO_PERIOD_LINE_NUMS)?(frameLength):(S5K5E2YA_VIDEO_PERIOD_LINE_NUMS);	
		break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:	
			frameLength = (s5k5e2ya.m_vidPclk)/frameRate*10/S5K5E2YA_FULL_PERIOD_PIXEL_NUMS;
			frameLength = (frameLength>S5K5E2YA_FULL_PERIOD_LINE_NUMS)?(frameLength):(S5K5E2YA_FULL_PERIOD_LINE_NUMS);	
		break;	
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
			frameLength = (s5k5e2ya.m_vidPclk)/frameRate*10/S5K5E2YA_ZSD_PERIOD_PIXEL_NUMS;
			frameLength = (frameLength>S5K5E2YA_ZSD_PERIOD_LINE_NUMS)?(frameLength):(S5K5E2YA_ZSD_PERIOD_LINE_NUMS);
		break;
        case MSDK_SCENARIO_ID_CAMERA_3D_PREVIEW: //added
            break;
        case MSDK_SCENARIO_ID_CAMERA_3D_VIDEO:
		break;
        case MSDK_SCENARIO_ID_CAMERA_3D_CAPTURE: //added   
		break;		
		default:
			frameLength = S5K5E2YA_PV_PERIOD_LINE_NUMS;
		break;
	}
	spin_lock(&s5k5e2yamipiraw_drv_lock);
	s5k5e2ya.FixedFrameLength = frameLength;
	spin_unlock(&s5k5e2yamipiraw_drv_lock);
	
	SetFramelength(frameLength); //direct set frameLength
	return ERROR_NONE;
}


UINT32 S5K5E2YAMIPIGetDefaultFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 *pframeRate) 
{

	switch (scenarioId) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			 *pframeRate = 300;
			 break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
		#ifdef FULL_SIZE_30_FPS
			 *pframeRate = 300;
		#else
			*pframeRate = 250; 
		#endif	
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

UINT32 S5K5E2YAFeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId,
                                                                UINT8 *pFeaturePara,UINT32 *pFeatureParaLen)
{
	SENSORDB("enter s5k5e2ya featurecontrol\n");
    UINT16 *pFeatureReturnPara16=(UINT16 *) pFeaturePara;
    UINT16 *pFeatureData16=(UINT16 *) pFeaturePara;
    UINT32 *pFeatureReturnPara32=(UINT32 *) pFeaturePara;
    UINT32 *pFeatureData32=(UINT32 *) pFeaturePara;
    UINT32 SensorRegNumber;
    UINT32 i;
    PNVRAM_SENSOR_DATA_STRUCT pSensorDefaultData=(PNVRAM_SENSOR_DATA_STRUCT) pFeaturePara;
    MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData=(MSDK_SENSOR_CONFIG_STRUCT *) pFeaturePara;
    MSDK_SENSOR_REG_INFO_STRUCT *pSensorRegData=(MSDK_SENSOR_REG_INFO_STRUCT *) pFeaturePara;
    MSDK_SENSOR_GROUP_INFO_STRUCT *pSensorGroupInfo=(MSDK_SENSOR_GROUP_INFO_STRUCT *) pFeaturePara;
    MSDK_SENSOR_ITEM_INFO_STRUCT *pSensorItemInfo=(MSDK_SENSOR_ITEM_INFO_STRUCT *) pFeaturePara;
    MSDK_SENSOR_ENG_INFO_STRUCT	*pSensorEngInfo=(MSDK_SENSOR_ENG_INFO_STRUCT *) pFeaturePara;
	
	SENSORDB("s5k5FeatureId=%d\n",FeatureId);
    switch (FeatureId)
    {
        case SENSOR_FEATURE_GET_RESOLUTION:
            *pFeatureReturnPara16++= S5K5E2YA_IMAGE_SENSOR_FULL_WIDTH;
            *pFeatureReturnPara16= S5K5E2YA_IMAGE_SENSOR_FULL_HEIGHT;
            *pFeatureParaLen=4;
			SENSORDB("s5k5enter SENSOR_FEATURE_GET_RESOLUTION\n");
            break;
        case SENSOR_FEATURE_GET_PERIOD:
				*pFeatureReturnPara16++= GetScenarioLinelength();
				*pFeatureReturnPara16= GetScenarioFramelength();
				*pFeatureParaLen=4;
				SENSORDB("s5k5enter SENSOR_FEATURE_GET_PERIOD\n");
				break;
        case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
			//same pclk for preview/capture
    	 	*pFeatureReturnPara32 = GetScenarioPixelClock();
			SENSORDB("s5k5sensor clock=%d\n",*pFeatureReturnPara32);
    	 	*pFeatureParaLen=4;
 			 break;
        case SENSOR_FEATURE_SET_ESHUTTER:
            S5K5E2YA_write_shutter(*pFeatureData16);
			SENSORDB("s5k5enter setshutter\n");
            break;
        case SENSOR_FEATURE_SET_NIGHTMODE:
            S5K5E2YA_NightMode((BOOL) *pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_GAIN:
            S5K5E2YA_SetGain((UINT16) *pFeatureData16);
			SENSORDB("s5k5enter setgain\n");
            break;
        case SENSOR_FEATURE_SET_FLASHLIGHT:
            break;
        case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
            //S5K5E2YA_isp_master_clock=*pFeatureData32;
            SENSORDB("s5k5enter setispmasterclockfreq\n");
            break;
        case SENSOR_FEATURE_SET_REGISTER:
            S5K5E2YA_wordwrite_cmos_sensor(pSensorRegData->RegAddr, pSensorRegData->RegData);
			SENSORDB("s5k5enter setregister\n");
			break;
        case SENSOR_FEATURE_GET_REGISTER:
            pSensorRegData->RegData = S5K5E2YA_read_cmos_sensor(pSensorRegData->RegAddr);
			SENSORDB("s5k5enter getregister\n");
			break;
        case SENSOR_FEATURE_SET_CCT_REGISTER:
            SensorRegNumber=FACTORY_END_ADDR;
            for (i=0;i<SensorRegNumber;i++)
            {
            	spin_lock(&s5k5e2yamipiraw_drv_lock);
                S5K5E2YASensorCCT[i].Addr=*pFeatureData32++;
                S5K5E2YASensorCCT[i].Para=*pFeatureData32++;
				spin_unlock(&s5k5e2yamipiraw_drv_lock);
            }
            break;
        case SENSOR_FEATURE_GET_CCT_REGISTER:
            SensorRegNumber=FACTORY_END_ADDR;
            if (*pFeatureParaLen<(SensorRegNumber*sizeof(SENSOR_REG_STRUCT)+4))
                return ERROR_INVALID_PARA;
            *pFeatureData32++=SensorRegNumber;
            for (i=0;i<SensorRegNumber;i++)
            {
                *pFeatureData32++=S5K5E2YASensorCCT[i].Addr;
                *pFeatureData32++=S5K5E2YASensorCCT[i].Para;
            }
            break;
        case SENSOR_FEATURE_SET_ENG_REGISTER:
            SensorRegNumber=ENGINEER_END;
            for (i=0;i<SensorRegNumber;i++)
            {
            	spin_lock(&s5k5e2yamipiraw_drv_lock);
                S5K5E2YASensorReg[i].Addr=*pFeatureData32++;
                S5K5E2YASensorReg[i].Para=*pFeatureData32++;
				spin_unlock(&s5k5e2yamipiraw_drv_lock);
            }
			SENSORDB("s5k5enter set eng register\n");
            break;
        case SENSOR_FEATURE_GET_ENG_REGISTER:
            SensorRegNumber=ENGINEER_END;
            if (*pFeatureParaLen<(SensorRegNumber*sizeof(SENSOR_REG_STRUCT)+4))
                return ERROR_INVALID_PARA;
            *pFeatureData32++=SensorRegNumber;
            for (i=0;i<SensorRegNumber;i++)
            {
                *pFeatureData32++=S5K5E2YASensorReg[i].Addr;
                *pFeatureData32++=S5K5E2YASensorReg[i].Para;
            }
			SENSORDB("s5k5enter get eng register\n");
            break;
        case SENSOR_FEATURE_GET_REGISTER_DEFAULT:
            if (*pFeatureParaLen>=sizeof(NVRAM_SENSOR_DATA_STRUCT))
            {
                pSensorDefaultData->Version=NVRAM_CAMERA_SENSOR_FILE_VERSION;
                pSensorDefaultData->SensorId=S5K5E2YA_SENSOR_ID;
                memcpy(pSensorDefaultData->SensorEngReg, S5K5E2YASensorReg, sizeof(SENSOR_REG_STRUCT)*ENGINEER_END);
                memcpy(pSensorDefaultData->SensorCCTReg, S5K5E2YASensorCCT, sizeof(SENSOR_REG_STRUCT)*FACTORY_END_ADDR);
            }
            else
                return ERROR_INVALID_PARA;
            *pFeatureParaLen=sizeof(NVRAM_SENSOR_DATA_STRUCT);
			SENSORDB("s5k5enter get register default\n");
			break;
        case SENSOR_FEATURE_GET_CONFIG_PARA:
            memcpy(pSensorConfigData, &S5K5E2YASensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));
            *pFeatureParaLen=sizeof(MSDK_SENSOR_CONFIG_STRUCT);
            break;
        case SENSOR_FEATURE_CAMERA_PARA_TO_SENSOR:
            S5K5E2YA_camera_para_to_sensor();
            break;

        case SENSOR_FEATURE_SENSOR_TO_CAMERA_PARA:
            S5K5E2YA_sensor_to_camera_para();
            break;
        case SENSOR_FEATURE_GET_GROUP_COUNT:
            *pFeatureReturnPara32++=S5K5E2YA_get_sensor_group_count();
            *pFeatureParaLen=4;
            break;
        case SENSOR_FEATURE_GET_GROUP_INFO:
            S5K5E2YA_get_sensor_group_info(pSensorGroupInfo->GroupIdx, pSensorGroupInfo->GroupNamePtr, &pSensorGroupInfo->ItemCount);
            *pFeatureParaLen=sizeof(MSDK_SENSOR_GROUP_INFO_STRUCT);
            break;
        case SENSOR_FEATURE_GET_ITEM_INFO:
            S5K5E2YA_get_sensor_item_info(pSensorItemInfo->GroupIdx,pSensorItemInfo->ItemIdx, pSensorItemInfo);
            *pFeatureParaLen=sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
            break;

        case SENSOR_FEATURE_SET_ITEM_INFO:
            S5K5E2YA_set_sensor_item_info(pSensorItemInfo->GroupIdx, pSensorItemInfo->ItemIdx, pSensorItemInfo->ItemValue);
            *pFeatureParaLen=sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
            break;

        case SENSOR_FEATURE_GET_ENG_INFO:
            pSensorEngInfo->SensorId = 129;
            pSensorEngInfo->SensorType = CMOS_SENSOR;
            pSensorEngInfo->SensorOutputDataFormat=SENSOR_OUTPUT_FORMAT_RAW_B;
            *pFeatureParaLen=sizeof(MSDK_SENSOR_ENG_INFO_STRUCT);
            break;
        case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
            // get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE
            // if EEPROM does not exist in camera module.
            *pFeatureReturnPara32=LENS_DRIVER_ID_DO_NOT_CARE;
            *pFeatureParaLen=4;
            break;

        case SENSOR_FEATURE_INITIALIZE_AF:
            break;
        case SENSOR_FEATURE_CONSTANT_AF:
            break;
        case SENSOR_FEATURE_MOVE_FOCUS_LENS:
            break;
        case SENSOR_FEATURE_SET_VIDEO_MODE:
            S5K5E2YASetVideoMode(*pFeatureData16);
            break;
        case SENSOR_FEATURE_CHECK_SENSOR_ID:
			SENSORDB("s5k5enter check_sensor_id\n");
            S5K5E2YAGetSensorID(pFeatureReturnPara32);
            break;
        case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
            S5K5E2YASetAutoFlickerMode((BOOL)*pFeatureData16, *(pFeatureData16+1));
	        break;
        case SENSOR_FEATURE_SET_TEST_PATTERN:
            S5K5E2YASetTestPatternMode((BOOL)*pFeatureData16);
            break;
		case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE://for factory mode auto testing             
            *pFeatureReturnPara32= S5K5E2YA_TEST_PATTERN_CHECKSUM;
			SENSORDB("checksum = 0x%x",*pFeatureReturnPara32);
           *pFeatureParaLen=4;                             
            break;
		case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
			S5K5E2YAMIPISetMaxFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32, *(pFeatureData32+1));
			break;
		case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
			S5K5E2YAMIPIGetDefaultFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32, (MUINT32 *)(*(pFeatureData32+1)));
			break;

        default:
            break;
    }
    return ERROR_NONE;
}	/* S5K5E2YAFeatureControl() */


SENSOR_FUNCTION_STRUCT	SensorFuncS5K5E2YA=
{
    S5K5E2YAOpen,
    S5K5E2YAGetInfo,
    S5K5E2YAGetResolution,
    S5K5E2YAFeatureControl,
    S5K5E2YAControl,
    S5K5E2YAClose
};

UINT32 S5K5E2YA_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
    /* To Do : Check Sensor status here */
    if (pfFunc!=NULL)
        *pfFunc=&SensorFuncS5K5E2YA;

    return ERROR_NONE;
}   /* SensorInit() */


