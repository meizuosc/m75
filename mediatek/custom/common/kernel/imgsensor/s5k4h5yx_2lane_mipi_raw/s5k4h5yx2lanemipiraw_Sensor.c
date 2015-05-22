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

#include "s5k4h5yx2lanemipiraw_Sensor.h"
#include "s5k4h5yx2lanemipiraw_Camera_Sensor_para.h"
#include "s5k4h5yx2lanemipiraw_CameraCustomized.h"

static DEFINE_SPINLOCK(s5k4h5yx2lanemipiraw_drv_lock);

#define S5K4H5YX_2LANE_DEBUG
//#define BURST_WRITE_SUPPORT
#define S5K4H5YX_2LANE_TEST_PATTERN_CHECKSUM (0x82256eb5)
#define SHUTTER_FRAMELENGTH_MARGIN 16

#ifdef S5K4H5YX_2LANE_DEBUG
	#define LOG_TAG (__FUNCTION__)
	#define S5K4H5YX2LANEDB(fmt, arg...) xlog_printk(ANDROID_LOG_DEBUG, LOG_TAG, fmt, ##arg)
#else
	#define S5K4H5YX2LANEDB(x,...)
#endif

kal_uint32 S5K4H5YX_2LANE_FeatureControl_PERIOD_PixelNum;
kal_uint32 S5K4H5YX_2LANE_FeatureControl_PERIOD_LineNum;
kal_uint32 S5K4H5YX_2LANE_FAC_SENSOR_REG;

MSDK_SENSOR_CONFIG_STRUCT S5K4H5YX2LANESensorConfigData;
MSDK_SCENARIO_ID_ENUM S5K4H5YX2LANECurrentScenarioId = MSDK_SCENARIO_ID_CAMERA_PREVIEW;

SENSOR_REG_STRUCT S5K4H5YX2LANESensorCCT[]=CAMERA_SENSOR_CCT_DEFAULT_VALUE;
SENSOR_REG_STRUCT S5K4H5YX2LANESensorReg[ENGINEER_END]=CAMERA_SENSOR_REG_DEFAULT_VALUE;

static S5K4H5YX_2LANE_PARA_STRUCT s5k4h5yx_2lane;

extern int iReadReg(u16 a_u2Addr , u8 * a_puBuff , u16 i2cId);
extern int iWriteReg(u16 a_u2Addr , u32 a_u4Data , u32 a_u4Bytes , u16 i2cId);

#define Sleep(ms) mdelay(ms)
#define mDELAY(ms)  mdelay(ms)

#define S5K4H5YX_2LANE_write_cmos_sensor(addr, para) iWriteReg((u16) addr , (u32) para , 1, S5K4H5YX_2LANE_MIPI_WRITE_ID)



kal_uint16 S5K4H5YX_2LANE_read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte=0;
    iReadReg((u16) addr ,(u8*)&get_byte,S5K4H5YX_2LANE_MIPI_WRITE_ID);
    return get_byte;
}


#ifdef BURST_WRITE_SUPPORT
extern int iBurstWriteReg(u8 *pData, u32 bytes, u16 i2cId);

#define I2C_BUFFER_LEN 254 



static kal_uint16 S5K4H5YX_2LANE_burst_write_cmos_sensor(kal_uint16* para, kal_uint32 len)
{   
	char puSendCmd[I2C_BUFFER_LEN];
	kal_uint32 tosend = 0, IDX = 0;   
	kal_uint16 addr, addr_last, data;   

	while(IDX < len)   
	{       
		addr = para[IDX];       

		if(tosend == 0)    
		{           
			puSendCmd[tosend++] = (char)(addr >> 8);           
			puSendCmd[tosend++] = (char)(addr & 0xFF);           
			data = para[IDX+1];                
			puSendCmd[tosend++] = (char)(data & 0xFF);           
			IDX += 2;           
			addr_last = addr;       
		}       
		else if(addr == addr_last)     
		{           
			data = para[IDX+1];                   
			puSendCmd[tosend++] = (char)(data & 0xFF);           
			IDX += 2;       
		}
		
		if (tosend == I2C_BUFFER_LEN || IDX == len || addr != addr_last)       
		{           
			iBurstWriteReg(puSendCmd , tosend, S5K4H5YX2LANEMIPI_WRITE_ID);           
			tosend = 0;       
		}   
	}   

	return 0;
}

#endif




//#define S5K4H5YX_2LANE_USE_AWB_OTP
#if defined(S5K4H5YX_2LANE_USE_AWB_OTP)

#define RG_TYPICAL 0x2a1
#define BG_TYPICAL 0x23f

kal_uint32 tRG_Ratio_typical = RG_TYPICAL;
kal_uint32 tBG_Ratio_typical = BG_TYPICAL;



void S5K4H5YX_2LANE_MIPI_read_otp_wb(struct S5K4H5YX_2LANE_MIPI_otp_struct *otp)
{
	kal_uint32 R_to_G, B_to_G, G_to_G;
	kal_uint16 PageCount;
	
	for(PageCount = 4; PageCount>=1; PageCount--)
	{
		S5K4H5YX2LANEDB("PageCount=%d\n", PageCount);	
		
		S5K4H5YX_2LANE_write_cmos_sensor(0x3a02, PageCount); 
		S5K4H5YX_2LANE_write_cmos_sensor(0x3a00, 0x01); 
		R_to_G = (S5K4H5YX_2LANE_read_cmos_sensor(0x3a09)<<8)+S5K4H5YX_2LANE_read_cmos_sensor(0x3a0a);
		B_to_G = (S5K4H5YX_2LANE_read_cmos_sensor(0x3a0b)<<8)+S5K4H5YX_2LANE_read_cmos_sensor(0x3a0c);
		G_to_G = (S5K4H5YX_2LANE_read_cmos_sensor(0x3a0d)<<8)+S5K4H5YX_2LANE_read_cmos_sensor(0x3a0e);
		S5K4H5YX_2LANE_write_cmos_sensor(0x3a00, 0x00); 

		if((R_to_G != 0) && (B_to_G != 0) && (G_to_G != 0))
			break;	
	}

	otp->R_to_G = R_to_G;
	otp->B_to_G = B_to_G;
	otp->G_to_G = 0x400;

	S5K4H5YX2LANEDB("otp->R_to_G=0x%x, otp->B_to_G=0x%x, otp->G_to_G=0x%x\n", otp->R_to_G, otp->B_to_G, otp->G_to_G);	
}



void S5K4H5YX_2LANE_MIPI_algorithm_otp_wb(struct S5K4H5YX_2LANE_MIPI_otp_struct *otp)
{
	kal_uint32 R_to_G, B_to_G, G_to_G, R_Gain, B_Gain, G_Gain, G_gain_R, G_gain_B;
	
	R_to_G = otp->R_to_G;
	B_to_G = otp->B_to_G;
	G_to_G = otp->G_to_G;

	S5K4H5YX2LANEDB("R_to_G=%d, B_to_G=%d, G_to_G=%d\n", R_to_G, B_to_G, G_to_G);	

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

	S5K4H5YX2LANEDB("R_gain=0x%x, B_gain=0x%, G_gain=0x%x\n", otp->R_Gain, otp->B_Gain, otp->G_Gain);
}



void S5K4H5YX_2LANE_MIPI_write_otp_wb(struct S5K4H5YX_2LANE_MIPI_otp_struct *otp)
{
	kal_uint16 R_GainH, B_GainH, G_GainH, R_GainL, B_GainL, G_GainL;
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

	S5K4H5YX2LANEDB("R_GainH=0x%x, R_GainL=0x%x, B_GainH=0x%x, B_GainL=0x%x, G_GainH=0x%x, G_GainL=0x%x\n", R_GainH, R_GainL, B_GainH, B_GainL, G_GainH, G_GainL);	

	S5K4H5YX_2LANE_write_cmos_sensor(0x020e, G_GainH);
	S5K4H5YX_2LANE_write_cmos_sensor(0x020f, G_GainL);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0210, R_GainH);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0211, R_GainL);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0212, B_GainH);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0213, B_GainL);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0214, G_GainH);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0215, G_GainL);
}



void S5K4H5YX_2LANE_MIPI_update_wb_register_from_otp(void)
{
	struct S5K4H5YX_2LANE_MIPI_otp_struct current_otp;
	
	S5K4H5YX_2LANE_MIPI_read_otp_wb(&current_otp);
	S5K4H5YX_2LANE_MIPI_algorithm_otp_wb(&current_otp);
	S5K4H5YX_2LANE_MIPI_write_otp_wb(&current_otp);
}
#endif



UINT32 S5K4H5YX_2LANE_read_shutter(void)
{
	UINT32 shutter =0;
	
	shutter = (S5K4H5YX_2LANE_read_cmos_sensor(0x0202) << 8) | S5K4H5YX_2LANE_read_cmos_sensor(0x0203);

	S5K4H5YX2LANEDB("shutter = %d\n", shutter);

	return shutter;
}



///////////////////////////////////////////////
//
//20121212:	add value "DynamicVideoSupport" in video mode
//			if this value is true, dynamic framerate, count min frame length limit sensor setting
//			if this value is false, fix frameratee, count min frame length limit in S5K4H5YX2LANESetVideoMode function call S5K4H5YX_2LANE_SetDummy function
//
//////////////////////////////////////////////
kal_uint16 S5K4H5YX_2LANE_get_framelength(void)
{
	kal_uint16 Framelength = s5k4h5yx_2lane_preview_frame_length;

	if(s5k4h5yx_2lane.sensorMode == SENSOR_MODE_PREVIEW)
		Framelength = s5k4h5yx_2lane_preview_frame_length;
	else if(s5k4h5yx_2lane.sensorMode == SENSOR_MODE_CAPTURE)
		Framelength = s5k4h5yx_2lane_capture_frame_length;
	else if(s5k4h5yx_2lane.sensorMode == SENSOR_MODE_VIDEO)
		{
		if(s5k4h5yx_2lane.DynamicVideoSupport == KAL_TRUE)
			Framelength = s5k4h5yx_2lane_video_frame_length;
		else
			Framelength = (S5K4H5YX_2LANE_read_cmos_sensor(0x0340)<<8)|S5K4H5YX_2LANE_read_cmos_sensor(0x0341);
		}
	return Framelength;
}



kal_uint16 S5K4H5YX_2LANE_get_linelength(void)
{
	kal_uint16 Linelength = s5k4h5yx_2lane_preview_line_length;

	if(s5k4h5yx_2lane.sensorMode == SENSOR_MODE_PREVIEW)
		Linelength = s5k4h5yx_2lane_preview_line_length;
	else if(s5k4h5yx_2lane.sensorMode == SENSOR_MODE_CAPTURE)
		Linelength = s5k4h5yx_2lane_capture_line_length;
	else if(s5k4h5yx_2lane.sensorMode == SENSOR_MODE_VIDEO)
		Linelength = s5k4h5yx_2lane_video_line_length;
	
	return Linelength;
}



kal_uint32 S5K4H5YX_2LANE_get_pixelclock(void)
{
	kal_uint32 PixelClock = s5k4h5yx_2lane_preview_pixelclock;

	if(s5k4h5yx_2lane.sensorMode == SENSOR_MODE_PREVIEW)
		PixelClock = s5k4h5yx_2lane_preview_pixelclock;
	else if(s5k4h5yx_2lane.sensorMode == SENSOR_MODE_CAPTURE)
		PixelClock = s5k4h5yx_2lane_capture_pixelclock;
	else if(s5k4h5yx_2lane.sensorMode == SENSOR_MODE_VIDEO)
		PixelClock = s5k4h5yx_2lane_video_pixelclock;
	
	return PixelClock;
}



///////////////////////////////////////////////////
//
//20121212:	add value "DynamicVideosupport" in video mode
//			if this value is false. fix framerate, fix frame_length, dynamic shutter
//			if this value is true or in other mode, dynamic framerate, dynamic frame_length, fix shutter
//
//////////////////////////////////////////////////
void S5K4H5YX_2LANE_write_shutter(kal_uint32 shutter)
{
	kal_uint16 frame_length = 0, line_length = 0, framerate = 0 , frame_length_min = 0, shutter_range = 0, shutter_count = 0;
	kal_uint32 pixelclock = 0;
	unsigned long flags;

	if (shutter < 3)
		shutter = 3;

	shutter_range = shutter + SHUTTER_FRAMELENGTH_MARGIN; 
	frame_length_min = S5K4H5YX_2LANE_get_framelength();

	if(s5k4h5yx_2lane.sensorMode == SENSOR_MODE_VIDEO && s5k4h5yx_2lane.DynamicVideoSupport == KAL_FALSE)
	{
		frame_length = frame_length_min;
		
		if(shutter_range > frame_length)
			shutter = frame_length - SHUTTER_FRAMELENGTH_MARGIN;
	}
	else
	{
		frame_length = shutter_range;
		
		if(frame_length < frame_length_min)
			frame_length = frame_length_min;

		if(s5k4h5yx_2lane.S5K4H5YX2LANEAutoFlickerMode == KAL_TRUE)
		{
			line_length = S5K4H5YX_2LANE_get_linelength();
			pixelclock = S5K4H5YX_2LANE_get_pixelclock();
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
	}

	shutter_count = S5K4H5YX_2LANE_read_shutter();
	
 	//S5K4H5YX_2LANE_write_cmos_sensor(0x0104, 0x01);
	if(shutter_count < frame_length - SHUTTER_FRAMELENGTH_MARGIN)
	{
		S5K4H5YX_2LANE_write_cmos_sensor(0x0340, (frame_length >>8) & 0xFF);
 		S5K4H5YX_2LANE_write_cmos_sensor(0x0341, frame_length & 0xFF);	 
 		S5K4H5YX_2LANE_write_cmos_sensor(0x0202, (shutter >> 8) & 0xFF);
 		S5K4H5YX_2LANE_write_cmos_sensor(0x0203, shutter  & 0xFF);
	}
	else
	{
		S5K4H5YX_2LANE_write_cmos_sensor(0x0202, (shutter >> 8) & 0xFF);
 		S5K4H5YX_2LANE_write_cmos_sensor(0x0203, shutter  & 0xFF);
		S5K4H5YX_2LANE_write_cmos_sensor(0x0340, (frame_length >>8) & 0xFF);
 		S5K4H5YX_2LANE_write_cmos_sensor(0x0341, frame_length & 0xFF);
	}
 	//S5K4H5YX_2LANE_write_cmos_sensor(0x0104, 0x00);

	S5K4H5YX2LANEDB("shutter=%d, frame_length=%d, frame_length_min=%d, framerate=%d\n", shutter,frame_length, frame_length_min, framerate);
} 



void write_S5K4H5YX_2LANE_gain(kal_uint16 gain)
{
	S5K4H5YX2LANEDB("gain=%d\n", gain);
	
	gain = gain / 2;
	
	//S5K4H5YX_2LANE_write_cmos_sensor(0x0104, 0x01);	
	S5K4H5YX_2LANE_write_cmos_sensor(0x0204,(gain>>8));
	S5K4H5YX_2LANE_write_cmos_sensor(0x0205,(gain&0xff));
	//S5K4H5YX_2LANE_write_cmos_sensor(0x0104, 0x00);
}



void S5K4H5YX_2LANE_SetGain(UINT16 iGain)
{
	S5K4H5YX2LANEDB("gain=%d\n", iGain);

	write_S5K4H5YX_2LANE_gain(iGain);
} 



kal_uint16 read_S5K4H5YX_2LANE_gain(void)
{
	kal_uint16 read_gain = 0;

	read_gain=((S5K4H5YX_2LANE_read_cmos_sensor(0x0204) << 8) | S5K4H5YX_2LANE_read_cmos_sensor(0x0205));
	
	S5K4H5YX2LANEDB("gain=%d\n", read_gain);

	return read_gain;
}  



void S5K4H5YX_camera_para_to_sensor(void)
{
    kal_uint32 i;

	for(i=0; 0xFFFFFFFF!=S5K4H5YX2LANESensorReg[i].Addr; i++)
        S5K4H5YX_2LANE_write_cmos_sensor(S5K4H5YX2LANESensorReg[i].Addr, S5K4H5YX2LANESensorReg[i].Para);
    
	for(i=ENGINEER_START_ADDR; 0xFFFFFFFF!=S5K4H5YX2LANESensorReg[i].Addr; i++)
        S5K4H5YX_2LANE_write_cmos_sensor(S5K4H5YX2LANESensorReg[i].Addr, S5K4H5YX2LANESensorReg[i].Para);

	for(i=FACTORY_START_ADDR; i<FACTORY_END_ADDR; i++)
        S5K4H5YX_2LANE_write_cmos_sensor(S5K4H5YX2LANESensorCCT[i].Addr, S5K4H5YX2LANESensorCCT[i].Para);
}



void S5K4H5YX_2LANE_sensor_to_camera_para(void)
{
    kal_uint32 i, temp_data;

	for(i=0; 0xFFFFFFFF!=S5K4H5YX2LANESensorReg[i].Addr; i++)
    {
         temp_data = S5K4H5YX_2LANE_read_cmos_sensor(S5K4H5YX2LANESensorReg[i].Addr);

		 spin_lock(&s5k4h5yx2lanemipiraw_drv_lock);
		 S5K4H5YX2LANESensorReg[i].Para =temp_data;
		 spin_unlock(&s5k4h5yx2lanemipiraw_drv_lock);
    }

	for(i=ENGINEER_START_ADDR; 0xFFFFFFFF!=S5K4H5YX2LANESensorReg[i].Addr; i++)
    {
        temp_data = S5K4H5YX_2LANE_read_cmos_sensor(S5K4H5YX2LANESensorReg[i].Addr);

		spin_lock(&s5k4h5yx2lanemipiraw_drv_lock);
		S5K4H5YX2LANESensorReg[i].Para = temp_data;
		spin_unlock(&s5k4h5yx2lanemipiraw_drv_lock);
    }
}



kal_int32  S5K4H5YX_2LANE_get_sensor_group_count(void)
{
    return GROUP_TOTAL_NUMS;
}



void S5K4H5YX_2LANE_get_sensor_group_info(kal_uint16 group_idx, kal_int8* group_name_ptr, kal_int32* item_count_ptr)
{
   switch (group_idx)
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
			break;
	}
}



void S5K4H5YX_2LANE_get_sensor_item_info(kal_uint16 group_idx,kal_uint16 item_idx, MSDK_SENSOR_ITEM_INFO_STRUCT* info_ptr)
{
    kal_int16 temp_reg=0, temp_gain=0, temp_addr=0, temp_para=0;

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
			  	break;
          }

       		temp_para= S5K4H5YX2LANESensorCCT[temp_addr].Para;
            info_ptr->ItemValue=temp_gain;
            info_ptr->IsTrueFalse=KAL_FALSE;
            info_ptr->IsReadOnly=KAL_FALSE;
            info_ptr->IsNeedRestart=KAL_FALSE;
            info_ptr->Min= s5k4h5yx_2lane_min_analog_gain * 1000;
            info_ptr->Max= s5k4h5yx_2lane_max_analog_gain * 1000;
            break;
        case CMMCLK_CURRENT:
            switch (item_idx)
            {
                case 0:
                    sprintf((char *)info_ptr->ItemNamePtr,"Drv Cur[2,4,6,8]mA");

                    temp_reg = ISP_DRIVING_2MA;
                    if(temp_reg==ISP_DRIVING_2MA)
                        info_ptr->ItemValue=2;
                    else if(temp_reg==ISP_DRIVING_4MA)
                        info_ptr->ItemValue=4;
                    else if(temp_reg==ISP_DRIVING_6MA)
                        info_ptr->ItemValue=6;
                    else if(temp_reg==ISP_DRIVING_8MA)
                        info_ptr->ItemValue=8;

                    info_ptr->IsTrueFalse=KAL_FALSE;
                    info_ptr->IsReadOnly=KAL_FALSE;
                    info_ptr->IsNeedRestart=KAL_TRUE;
                    info_ptr->Min=2;
                    info_ptr->Max=8;
                    break;
                default:
					break;
            }
            break;
        case FRAME_RATE_LIMITATION:
            switch (item_idx)
            {
                case 0:
                    sprintf((char *)info_ptr->ItemNamePtr,"Max Exposure Lines");
                    info_ptr->ItemValue=    111; 
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
					break;
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
					break;
            }
            break;
        default:
			break;
    }
}



kal_bool S5K4H5YX_2LANE_set_sensor_item_info(kal_uint16 group_idx, kal_uint16 item_idx, kal_int32 ItemValue)
{
   kal_uint16  temp_gain=0, temp_addr=0, temp_para=0;

   switch (group_idx)
    {
        case PRE_GAIN:
            switch (item_idx)
            {
              case 0:
                temp_addr = PRE_GAIN_R_INDEX;
              	break;
              case 1:
                temp_addr = PRE_GAIN_Gr_INDEX;
              	break;
              case 2:
                temp_addr = PRE_GAIN_Gb_INDEX;
              	break;
              case 3:
                temp_addr = PRE_GAIN_B_INDEX;
              	break;
              case 4:
                temp_addr = SENSOR_BASEGAIN;
              	break;
              default:
              	break;
          }

		  temp_gain=((ItemValue*BASEGAIN+500)/1000);			

		  spin_lock(&s5k4h5yx2lanemipiraw_drv_lock);
          S5K4H5YX2LANESensorCCT[temp_addr].Para = temp_para;
		  spin_unlock(&s5k4h5yx2lanemipiraw_drv_lock);
		  
          S5K4H5YX_2LANE_write_cmos_sensor(S5K4H5YX2LANESensorCCT[temp_addr].Addr,temp_para);
        	break;
        case CMMCLK_CURRENT:
            switch (item_idx)
            {
                case 0:
                    break;
                default:
					break;
            }
            break;
        case FRAME_RATE_LIMITATION:
            break;
        case REGISTER_EDITOR:
            switch (item_idx)
            {
                case 0:
					spin_lock(&s5k4h5yx2lanemipiraw_drv_lock);
                    S5K4H5YX_2LANE_FAC_SENSOR_REG=ItemValue;
					spin_unlock(&s5k4h5yx2lanemipiraw_drv_lock);
                    break;
                case 1:
                    S5K4H5YX_2LANE_write_cmos_sensor(S5K4H5YX_2LANE_FAC_SENSOR_REG,ItemValue);
                    break;
                default:
					break;
            }
            break;
        default:
			break;
    }
    return KAL_TRUE;
}



/////////////////////////////////////////////////////////////////////////////////////////
//
//20121212:	add value "shutter", fix bad frame, when shutter > frame_length -16;
//
////////////////////////////////////////////////////////////////////////////////////////
static void S5K4H5YX_2LANE_SetDummy( const kal_uint32 iPixels, const kal_uint32 iLines )
{
	kal_uint16 line_length = 0, frame_length = 0, shutter = 0, shutter_count = 0;

	frame_length = S5K4H5YX_2LANE_get_framelength() + iLines;
	line_length = S5K4H5YX_2LANE_get_linelength() + iPixels;
	shutter = frame_length - SHUTTER_FRAMELENGTH_MARGIN;

	shutter_count = S5K4H5YX_2LANE_read_shutter();
	
	//S5K4H5YX_2LANE_write_cmos_sensor(0x0104, 0x01);
	if(shutter_count < frame_length - SHUTTER_FRAMELENGTH_MARGIN)
	{
		shutter = shutter_count;
		
		S5K4H5YX_2LANE_write_cmos_sensor(0x0340, (frame_length >> 8) & 0xFF);
		S5K4H5YX_2LANE_write_cmos_sensor(0x0341, frame_length & 0xFF);
		S5K4H5YX_2LANE_write_cmos_sensor(0x0342, (line_length >> 8) & 0xFF);
		S5K4H5YX_2LANE_write_cmos_sensor(0x0343, line_length & 0xFF);
		S5K4H5YX_2LANE_write_cmos_sensor(0x0202, (shutter >> 8) & 0xFF); 
 		S5K4H5YX_2LANE_write_cmos_sensor(0x0203, shutter  & 0xFF);
	}
	else
	{
		S5K4H5YX_2LANE_write_cmos_sensor(0x0202, (shutter >> 8) & 0xFF); 
 		S5K4H5YX_2LANE_write_cmos_sensor(0x0203, shutter  & 0xFF);
		S5K4H5YX_2LANE_write_cmos_sensor(0x0340, (frame_length >> 8) & 0xFF);
		S5K4H5YX_2LANE_write_cmos_sensor(0x0341, frame_length & 0xFF);
		S5K4H5YX_2LANE_write_cmos_sensor(0x0342, (line_length >> 8) & 0xFF);
		S5K4H5YX_2LANE_write_cmos_sensor(0x0343, line_length & 0xFF);
	}
	//S5K4H5YX_2LANE_write_cmos_sensor(0x0104, 0x00);
	
	S5K4H5YX2LANEDB("frame_length=%d, line_length=%d\n", frame_length, line_length);
}



void S5K4H5YX2LANEPreviewSetting(void)
{
	kal_uint16 chip_id = 0;
	
	chip_id = S5K4H5YX_2LANE_read_cmos_sensor(0x0002);

	S5K4H5YX2LANEDB("chip_id=%x\n",chip_id);

	if(chip_id == 0x01)
	{
	#ifdef BURST_WRITE_SUPPORT
		static const kal_uint16 addr_data_pair[] =
		{	
			0x0100, 0x00,
			0x0101, 0x00,
			0x0204, 0x00,
			0x0205, 0x20,
			0x0200, 0x0D,
			0x0201, 0x78,
			0x0202, 0x00,
			0x0203, 0x20,
			0x0340, 0x09,
			0x0341, 0xD0,
			0x0342, 0x0E,
			0x0343, 0x68,
			0x0344, 0x00,
			0x0345, 0x00,
			0x0346, 0x00,
			0x0347, 0x00,
			0x0348, 0x0C,
			0x0349, 0xCF,
			0x034A, 0x09,
			0x034B, 0x9F,
			0x034C, 0x06,
			0x034D, 0x68,
			0x034E, 0x04,
			0x034F, 0xD0,
			0x0390, 0x01,
			0x0391, 0x22,
			0x0381, 0x01,
			0x0383, 0x03,
			0x0385, 0x01,
			0x0387, 0x03,
			0x0301, 0x02,
			0x0303, 0x01,
			0x0305, 0x06,
			0x0306, 0x00,
			0x0307, 0x8C,
			0x0309, 0x02,
			0x030B, 0x01,
			0x3C59, 0x00,
			0x030D, 0x06,
			0x030E, 0x00,
			0x030F, 0xAF,
			0x3C5A, 0x00,
			0x0310, 0x01,
			0x3C50, 0x53,
			0x3C62, 0x02,
			0x3C63, 0xBC,
			0x3C64, 0x00,
			0x3C65, 0x00,
			0x3C1E, 0x00,
			0x302A, 0x0A,
			0x304B, 0x2A,
			0x3205, 0x84,
			0x3207, 0x85,
			0x3214, 0x94,
			0x3216, 0x95,
			0x303A, 0x9F,
			0x3201, 0x07,
			0x3051, 0xFF,
			0x3052, 0xFF,
			0x3054, 0xF0,
			0x302D, 0x7F,
			0x3002, 0x0D,
			0x300A, 0x0D,
			0x3037, 0x12,
			0x3045, 0x04,
			0x300C, 0x78,
			0x300D, 0x80,
			0x305C, 0x82,
			0x3010, 0x0A,
			0x305E, 0x11,
			0x305F, 0x11,
			0x3060, 0x10,
			0x3091, 0x04,
			0x3092, 0x07,
			0x303D, 0x05,
			0x3038, 0x99,
			0x3B29, 0x01,
			0x0100, 0x01,
		};
	    S5K4H5YX_2LANE_burst_write_cmos_sensor(addr_data_pair, sizeof(addr_data_pair)/sizeof(kal_uint16));
	#else
S5K4H5YX_2LANE_write_cmos_sensor(0x0100, 0x00);
S5K4H5YX_2LANE_write_cmos_sensor(0x0101, 0x00);
S5K4H5YX_2LANE_write_cmos_sensor(0x0204, 0x00);
S5K4H5YX_2LANE_write_cmos_sensor(0x0205, 0x20);
S5K4H5YX_2LANE_write_cmos_sensor(0x0200, 0x0D);
S5K4H5YX_2LANE_write_cmos_sensor(0x0201, 0x78);
S5K4H5YX_2LANE_write_cmos_sensor(0x0202, 0x00);//04
S5K4H5YX_2LANE_write_cmos_sensor(0x0203, 0x20);//c0
S5K4H5YX_2LANE_write_cmos_sensor(0x0340, 0x04);
S5K4H5YX_2LANE_write_cmos_sensor(0x0341, 0xF0);
S5K4H5YX_2LANE_write_cmos_sensor(0x0342, 0x0E);
S5K4H5YX_2LANE_write_cmos_sensor(0x0343, 0x68);
S5K4H5YX_2LANE_write_cmos_sensor(0x0344, 0x00);
S5K4H5YX_2LANE_write_cmos_sensor(0x0345, 0x08);
S5K4H5YX_2LANE_write_cmos_sensor(0x0346, 0x00);
S5K4H5YX_2LANE_write_cmos_sensor(0x0347, 0x00);
S5K4H5YX_2LANE_write_cmos_sensor(0x0348, 0x0C);
S5K4H5YX_2LANE_write_cmos_sensor(0x0349, 0xC9);
S5K4H5YX_2LANE_write_cmos_sensor(0x034A, 0x09);
S5K4H5YX_2LANE_write_cmos_sensor(0x034B, 0x9F);
S5K4H5YX_2LANE_write_cmos_sensor(0x034C, 0x06);
S5K4H5YX_2LANE_write_cmos_sensor(0x034D, 0x60);
S5K4H5YX_2LANE_write_cmos_sensor(0x034E, 0x04);
S5K4H5YX_2LANE_write_cmos_sensor(0x034F, 0xD0);
S5K4H5YX_2LANE_write_cmos_sensor(0x0390, 0x01);
S5K4H5YX_2LANE_write_cmos_sensor(0x0391, 0x22);
S5K4H5YX_2LANE_write_cmos_sensor(0x0940, 0x00);
S5K4H5YX_2LANE_write_cmos_sensor(0x0381, 0x01);
S5K4H5YX_2LANE_write_cmos_sensor(0x0383, 0x03);
S5K4H5YX_2LANE_write_cmos_sensor(0x0385, 0x01);
S5K4H5YX_2LANE_write_cmos_sensor(0x0387, 0x03);
S5K4H5YX_2LANE_write_cmos_sensor(0x0114, 0x01);
S5K4H5YX_2LANE_write_cmos_sensor(0x0301, 0x02);
S5K4H5YX_2LANE_write_cmos_sensor(0x0303, 0x01);
S5K4H5YX_2LANE_write_cmos_sensor(0x0305, 0x06);
S5K4H5YX_2LANE_write_cmos_sensor(0x0306, 0x00);
S5K4H5YX_2LANE_write_cmos_sensor(0x0307, 0x46);
S5K4H5YX_2LANE_write_cmos_sensor(0x0309, 0x02);
S5K4H5YX_2LANE_write_cmos_sensor(0x030B, 0x01);
S5K4H5YX_2LANE_write_cmos_sensor(0x3C59, 0x00);
S5K4H5YX_2LANE_write_cmos_sensor(0x030D, 0x06);
S5K4H5YX_2LANE_write_cmos_sensor(0x030E, 0x00);
S5K4H5YX_2LANE_write_cmos_sensor(0x030F, 0xA2);
S5K4H5YX_2LANE_write_cmos_sensor(0x3C5A, 0x00);
S5K4H5YX_2LANE_write_cmos_sensor(0x0310, 0x01);
S5K4H5YX_2LANE_write_cmos_sensor(0x3C50, 0x53);
S5K4H5YX_2LANE_write_cmos_sensor(0x3C62, 0x02);
S5K4H5YX_2LANE_write_cmos_sensor(0x3C63, 0x88);
S5K4H5YX_2LANE_write_cmos_sensor(0x3C64, 0x00);
S5K4H5YX_2LANE_write_cmos_sensor(0x3C65, 0x00);
S5K4H5YX_2LANE_write_cmos_sensor(0x3C1E, 0x00);
S5K4H5YX_2LANE_write_cmos_sensor(0x3500, 0x0C);
S5K4H5YX_2LANE_write_cmos_sensor(0x3C1A, 0xA8);
S5K4H5YX_2LANE_write_cmos_sensor(0x3B29, 0x01);
S5K4H5YX_2LANE_write_cmos_sensor(0x3000, 0x07);
S5K4H5YX_2LANE_write_cmos_sensor(0x3001, 0x05);
S5K4H5YX_2LANE_write_cmos_sensor(0x3002, 0x03);
S5K4H5YX_2LANE_write_cmos_sensor(0x0200, 0x0D);
S5K4H5YX_2LANE_write_cmos_sensor(0x0201, 0x78);
S5K4H5YX_2LANE_write_cmos_sensor(0x300A, 0x03);
S5K4H5YX_2LANE_write_cmos_sensor(0x300C, 0x65);
S5K4H5YX_2LANE_write_cmos_sensor(0x300D, 0x54);
S5K4H5YX_2LANE_write_cmos_sensor(0x3010, 0x00);
S5K4H5YX_2LANE_write_cmos_sensor(0x3012, 0x14);
S5K4H5YX_2LANE_write_cmos_sensor(0x3014, 0x19);
S5K4H5YX_2LANE_write_cmos_sensor(0x3017, 0x0F);
S5K4H5YX_2LANE_write_cmos_sensor(0x3018, 0x1A);
S5K4H5YX_2LANE_write_cmos_sensor(0x3019, 0x6C);
S5K4H5YX_2LANE_write_cmos_sensor(0x301A, 0x78);
S5K4H5YX_2LANE_write_cmos_sensor(0x306F, 0x00);
S5K4H5YX_2LANE_write_cmos_sensor(0x3070, 0x00);
S5K4H5YX_2LANE_write_cmos_sensor(0x3071, 0x00);
S5K4H5YX_2LANE_write_cmos_sensor(0x3072, 0x00);
S5K4H5YX_2LANE_write_cmos_sensor(0x3073, 0x00);
S5K4H5YX_2LANE_write_cmos_sensor(0x3074, 0x00);
S5K4H5YX_2LANE_write_cmos_sensor(0x3075, 0x00);
S5K4H5YX_2LANE_write_cmos_sensor(0x3076, 0x0A);
S5K4H5YX_2LANE_write_cmos_sensor(0x3077, 0x03);
S5K4H5YX_2LANE_write_cmos_sensor(0x3078, 0x84);
S5K4H5YX_2LANE_write_cmos_sensor(0x3079, 0x00);
S5K4H5YX_2LANE_write_cmos_sensor(0x307A, 0x00);
S5K4H5YX_2LANE_write_cmos_sensor(0x307B, 0x00);
S5K4H5YX_2LANE_write_cmos_sensor(0x307C, 0x00);
S5K4H5YX_2LANE_write_cmos_sensor(0x3085, 0x00);
S5K4H5YX_2LANE_write_cmos_sensor(0x3086, 0x72);
S5K4H5YX_2LANE_write_cmos_sensor(0x30A6, 0x01);
S5K4H5YX_2LANE_write_cmos_sensor(0x30A7, 0x0E);
S5K4H5YX_2LANE_write_cmos_sensor(0x3032, 0x01);
S5K4H5YX_2LANE_write_cmos_sensor(0x3037, 0x02);
S5K4H5YX_2LANE_write_cmos_sensor(0x304A, 0x01);
S5K4H5YX_2LANE_write_cmos_sensor(0x3054, 0xF0);
S5K4H5YX_2LANE_write_cmos_sensor(0x3044, 0x20);
S5K4H5YX_2LANE_write_cmos_sensor(0x3045, 0x20);
S5K4H5YX_2LANE_write_cmos_sensor(0x3047, 0x04);
S5K4H5YX_2LANE_write_cmos_sensor(0x3048, 0x11);
S5K4H5YX_2LANE_write_cmos_sensor(0x303D, 0x08);
S5K4H5YX_2LANE_write_cmos_sensor(0x304B, 0x31);
S5K4H5YX_2LANE_write_cmos_sensor(0x3063, 0x00);
S5K4H5YX_2LANE_write_cmos_sensor(0x303A, 0xA0);
S5K4H5YX_2LANE_write_cmos_sensor(0x302D, 0x7F);
S5K4H5YX_2LANE_write_cmos_sensor(0x3039, 0x45);
S5K4H5YX_2LANE_write_cmos_sensor(0x3038, 0x10);
S5K4H5YX_2LANE_write_cmos_sensor(0x3097, 0x11);
S5K4H5YX_2LANE_write_cmos_sensor(0x3096, 0x02);
S5K4H5YX_2LANE_write_cmos_sensor(0x3042, 0x01);
S5K4H5YX_2LANE_write_cmos_sensor(0x3053, 0x01);
S5K4H5YX_2LANE_write_cmos_sensor(0x320D, 0x00);
S5K4H5YX_2LANE_write_cmos_sensor(0x3204, 0x00);
S5K4H5YX_2LANE_write_cmos_sensor(0x3205, 0x81);
S5K4H5YX_2LANE_write_cmos_sensor(0x3206, 0x00);
S5K4H5YX_2LANE_write_cmos_sensor(0x3207, 0x81);
S5K4H5YX_2LANE_write_cmos_sensor(0x3208, 0x00);
S5K4H5YX_2LANE_write_cmos_sensor(0x3209, 0x81);
S5K4H5YX_2LANE_write_cmos_sensor(0x3213, 0x02);
S5K4H5YX_2LANE_write_cmos_sensor(0x3214, 0x91);
S5K4H5YX_2LANE_write_cmos_sensor(0x3215, 0x02);
S5K4H5YX_2LANE_write_cmos_sensor(0x3216, 0x91);
S5K4H5YX_2LANE_write_cmos_sensor(0x3217, 0x02);
S5K4H5YX_2LANE_write_cmos_sensor(0x3218, 0x91);
S5K4H5YX_2LANE_write_cmos_sensor(0x0100, 0x01);
	#endif
	}
	else if(chip_id == 0x03)
	{ 
	#ifdef BURST_WRITE_SUPPORT
		static const kal_uint16 addr_data_pair[] =
{	
			0x0100, 0x00,
			0x0101, 0x00,
			0x0204, 0x00,
			0x0205, 0x20,
			0x0200, 0x0D,
			0x0201, 0x78,
			0x0202, 0x00,
			0x0203, 0x20,
			0x0340, 0x09,
			0x0341, 0xD0,
			0x0342, 0x0E,
			0x0343, 0x68,
			0x0344, 0x00,
			0x0345, 0x00,
			0x0346, 0x00,
			0x0347, 0x00,
			0x0348, 0x0C,
			0x0349, 0xCF,
			0x034A, 0x09,
			0x034B, 0x9F,
			0x034C, 0x06,
			0x034D, 0x68,
			0x034E, 0x04,
			0x034F, 0xD0,
			0x0390, 0x01,
			0x0391, 0x22,
			0x0381, 0x01,
			0x0383, 0x03,
			0x0385, 0x01,
			0x0387, 0x03,
			0x0301, 0x02,
			0x0303, 0x01,
			0x0305, 0x06,
			0x0306, 0x00,
			0x0307, 0x8C,
			0x0309, 0x02,
			0x030B, 0x01,
			0x3C59, 0x00,
			0x030D, 0x06,
			0x030E, 0x00,
			0x030F, 0xAF,
			0x3C5A, 0x00,
			0x0310, 0x01,
			0x3C50, 0x53,
			0x3C62, 0x02,
			0x3C63, 0xBC,
			0x3C64, 0x00,
			0x3C65, 0x00,
			0x3C1E, 0x00,
			0x302A, 0x0A,
			0x304B, 0x2A,
			0x3205, 0x84,
			0x3207, 0x85,
			0x3214, 0x94,
			0x3216, 0x95,
			0x303A, 0x9F,
			0x3201, 0x07,
			0x3051, 0xFF,
			0x3052, 0xFF,
			0x3054, 0xF0,
			0x302D, 0x7F,
			0x3002, 0x0D,
			0x300A, 0x0D,
			0x3037, 0x12,
			0x3045, 0x04,
			0x300C, 0x78,
			0x300D, 0x80,
			0x305C, 0x82,
			0x3010, 0x0A,
			0x305E, 0x11,
			0x305F, 0x11,
			0x3060, 0x10,
			0x3091, 0x04,
			0x3092, 0x07,
			0x303D, 0x05,
			0x3038, 0x99,
			0x3B29, 0x01,
			0x0100, 0x01,
		};
		S5K4H5YX_2LANE_burst_write_cmos_sensor(addr_data_pair, sizeof(addr_data_pair)/sizeof(kal_uint16));
	#else
	S5K4H5YX_2LANE_write_cmos_sensor(0x0100, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0101, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0204, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0205, 0x20);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0200, 0x0D);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0201, 0x78);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0202, 0x00);//04
	S5K4H5YX_2LANE_write_cmos_sensor(0x0203, 0x20);//c0
	S5K4H5YX_2LANE_write_cmos_sensor(0x0340, 0x04);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0341, 0xF0);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0342, 0x0E);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0343, 0x68);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0344, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0345, 0x08);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0346, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0347, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0348, 0x0C);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0349, 0xC9);
	S5K4H5YX_2LANE_write_cmos_sensor(0x034A, 0x09);
	S5K4H5YX_2LANE_write_cmos_sensor(0x034B, 0x9F);
	S5K4H5YX_2LANE_write_cmos_sensor(0x034C, 0x06);
	S5K4H5YX_2LANE_write_cmos_sensor(0x034D, 0x60);
	S5K4H5YX_2LANE_write_cmos_sensor(0x034E, 0x04);
	S5K4H5YX_2LANE_write_cmos_sensor(0x034F, 0xD0);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0390, 0x01);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0391, 0x22);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0940, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0381, 0x01);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0383, 0x03);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0385, 0x01);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0387, 0x03);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0114, 0x01);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0301, 0x02);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0303, 0x01);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0305, 0x06);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0306, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0307, 0x46);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0309, 0x02);
	S5K4H5YX_2LANE_write_cmos_sensor(0x030B, 0x01);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3C59, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x030D, 0x06);
	S5K4H5YX_2LANE_write_cmos_sensor(0x030E, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x030F, 0xA2);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3C5A, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0310, 0x01);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3C50, 0x53);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3C62, 0x02);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3C63, 0x88);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3C64, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3C65, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3C1E, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3500, 0x0C);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3C1A, 0xA8);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3B29, 0x01);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3000, 0x07);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3001, 0x05);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3002, 0x03);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0200, 0x0D);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0201, 0x78);
	S5K4H5YX_2LANE_write_cmos_sensor(0x300A, 0x03);
	S5K4H5YX_2LANE_write_cmos_sensor(0x300C, 0x65);
	S5K4H5YX_2LANE_write_cmos_sensor(0x300D, 0x54);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3010, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3012, 0x14);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3014, 0x19);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3017, 0x0F);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3018, 0x1A);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3019, 0x6C);
	S5K4H5YX_2LANE_write_cmos_sensor(0x301A, 0x78);
	S5K4H5YX_2LANE_write_cmos_sensor(0x306F, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3070, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3071, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3072, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3073, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3074, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3075, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3076, 0x0A);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3077, 0x03);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3078, 0x84);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3079, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x307A, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x307B, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x307C, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3085, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3086, 0x72);
	S5K4H5YX_2LANE_write_cmos_sensor(0x30A6, 0x01);
	S5K4H5YX_2LANE_write_cmos_sensor(0x30A7, 0x0E);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3032, 0x01);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3037, 0x02);
	S5K4H5YX_2LANE_write_cmos_sensor(0x304A, 0x01);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3054, 0xF0);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3044, 0x20);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3045, 0x20);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3047, 0x04);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3048, 0x11);
	S5K4H5YX_2LANE_write_cmos_sensor(0x303D, 0x08);
	S5K4H5YX_2LANE_write_cmos_sensor(0x304B, 0x31);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3063, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x303A, 0xA0);
	S5K4H5YX_2LANE_write_cmos_sensor(0x302D, 0x7F);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3039, 0x45);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3038, 0x10);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3097, 0x11);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3096, 0x02);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3042, 0x01);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3053, 0x01);
	S5K4H5YX_2LANE_write_cmos_sensor(0x320D, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3204, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3205, 0x81);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3206, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3207, 0x81);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3208, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3209, 0x81);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3213, 0x02);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3214, 0x91);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3215, 0x02);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3216, 0x91);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3217, 0x02);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3218, 0x91);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0100, 0x01);
	#endif
	}	

	mDELAY(15);
}



void S5K4H5YX2LANECaptureSetting(void)
{
	kal_uint16 chip_id = 0;
				
	chip_id = S5K4H5YX_2LANE_read_cmos_sensor(0x0002);

	S5K4H5YX2LANEDB("chip_id=%x\n",chip_id);

	if(chip_id == 0x01)
	{
	#ifdef BURST_WRITE_SUPPORT
		static const kal_uint16 addr_data_pair[] =
		{
			0x0100, 0x00,
			0x0101, 0x00,
			0x0204, 0x00,
			0x0205, 0x20,
			0x0200, 0x0D,
			0x0201, 0x78,
			//0x0202, 0x04,
			//0x0203, 0xE2,
			//0x0340, 0x09,
			//0x0341, 0xD0,
			0x0342, 0x0E,
			0x0343, 0x68,
			0x0344, 0x00,
			0x0345, 0x00,
			0x0346, 0x00,
			0x0347, 0x00,
			0x0348, 0x0C,
			0x0349, 0xCF,
			0x034A, 0x09,
			0x034B, 0x9F,
			0x034C, 0x0C,
			0x034D, 0xD0,
			0x034E, 0x09,
			0x034F, 0xA0,
			0x0390, 0x00,
			0x0391, 0x00,
			0x0381, 0x01,
			0x0383, 0x01,
			0x0385, 0x01,
			0x0387, 0x01,
			0x0301, 0x02,
			0x0303, 0x01,
			0x0305, 0x06,
			0x0306, 0x00,
			0x0307, 0x8C,
			0x0309, 0x02,
			0x030B, 0x01,
			0x3C59, 0x00,
			0x030D, 0x06,
			0x030E, 0x00,
			0x030F, 0xAF,
			0x3C5A, 0x00,
			0x0310, 0x01,
			0x3C50, 0x53,
			0x3C62, 0x02,
			0x3C63, 0xBC,
			0x3C64, 0x00,
			0x3C65, 0x00,
			0x3C1E, 0x00,
			0x302A, 0x0A,
			0x304B, 0x2A,
			0x3205, 0x84,
			0x3207, 0x85,
			0x3214, 0x94,
			0x3216, 0x95,
			0x303A, 0x9F,
			0x3201, 0x07,
			0x3051, 0xFF,
			0x3052, 0xFF,
			0x3054, 0xF0,
			0x302D, 0x7F,
			0x3002, 0x0D,
			0x300A, 0x0D,
			0x3037, 0x12,
			0x3045, 0x04,
			0x300C, 0x78,
			0x300D, 0x80,
			0x305C, 0x82,
			0x3010, 0x0A,
			0x305E, 0x11,
			0x305F, 0x11,
			0x3060, 0x10,
			0x3091, 0x04,
			0x3092, 0x07,
			0x303D, 0x05,
			0x3038, 0x99,
			0x3B29, 0x01,
			0x0100, 0x01
		};
		S5K4H5YX_2LANE_burst_write_cmos_sensor(addr_data_pair, sizeof(addr_data_pair)/sizeof(kal_uint16));
	#else
	S5K4H5YX_2LANE_write_cmos_sensor(0x0100, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0101, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0204, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0205, 0x20);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0200, 0x0D);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0201, 0x78);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0202, 0x09);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0203, 0xC8);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0340, 0x09);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0341, 0xE3);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0342, 0x0E);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0343, 0x68);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0344, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0345, 0x08);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0346, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0347, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0348, 0x0C);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0349, 0xC7);
	S5K4H5YX_2LANE_write_cmos_sensor(0x034A, 0x09);
	S5K4H5YX_2LANE_write_cmos_sensor(0x034B, 0x9F);
	S5K4H5YX_2LANE_write_cmos_sensor(0x034C, 0x0C);
	S5K4H5YX_2LANE_write_cmos_sensor(0x034D, 0xC0);
	S5K4H5YX_2LANE_write_cmos_sensor(0x034E, 0x09);
	S5K4H5YX_2LANE_write_cmos_sensor(0x034F, 0xA0);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0390, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0391, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0940, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0381, 0x01);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0383, 0x01);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0385, 0x01);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0387, 0x01);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0114, 0x01);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0301, 0x02);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0303, 0x01);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0305, 0x06);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0306, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0307, 0x46);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0309, 0x02);
	S5K4H5YX_2LANE_write_cmos_sensor(0x030B, 0x01);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3C59, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x030D, 0x06);
	S5K4H5YX_2LANE_write_cmos_sensor(0x030E, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x030F, 0xA2);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3C5A, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0310, 0x01);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3C50, 0x53);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3C62, 0x02);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3C63, 0x88);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3C64, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3C65, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3C1E, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3500, 0x0C);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3C1A, 0xA8);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3B29, 0x01);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3000, 0x07);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3001, 0x05);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3002, 0x03);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0200, 0x0D);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0201, 0x78);
	S5K4H5YX_2LANE_write_cmos_sensor(0x300A, 0x03);
	S5K4H5YX_2LANE_write_cmos_sensor(0x300C, 0x65);
	S5K4H5YX_2LANE_write_cmos_sensor(0x300D, 0x54);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3010, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3012, 0x14);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3014, 0x19);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3017, 0x0F);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3018, 0x1A);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3019, 0x6C);
	S5K4H5YX_2LANE_write_cmos_sensor(0x301A, 0x78);
	S5K4H5YX_2LANE_write_cmos_sensor(0x306F, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3070, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3071, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3072, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3073, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3074, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3075, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3076, 0x0A);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3077, 0x03);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3078, 0x84);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3079, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x307A, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x307B, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x307C, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3085, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3086, 0x72);
	S5K4H5YX_2LANE_write_cmos_sensor(0x30A6, 0x01);
	S5K4H5YX_2LANE_write_cmos_sensor(0x30A7, 0x0E);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3032, 0x01);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3037, 0x02);
	S5K4H5YX_2LANE_write_cmos_sensor(0x304A, 0x01);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3054, 0xF0);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3044, 0x20);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3045, 0x20);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3047, 0x04);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3048, 0x11);
	S5K4H5YX_2LANE_write_cmos_sensor(0x303D, 0x08);
	S5K4H5YX_2LANE_write_cmos_sensor(0x304B, 0x31);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3063, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x303A, 0xA0);
	S5K4H5YX_2LANE_write_cmos_sensor(0x302D, 0x7F);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3039, 0x45);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3038, 0x10);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3097, 0x11);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3096, 0x02);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3042, 0x01);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3053, 0x01);
	S5K4H5YX_2LANE_write_cmos_sensor(0x320D, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3204, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3205, 0x81);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3206, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3207, 0x81);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3208, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3209, 0x81);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3213, 0x02);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3214, 0x91);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3215, 0x02);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3216, 0x91);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3217, 0x02);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3218, 0x91);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0100, 0x01);

	#endif
	}
	else if(chip_id == 0x03)
	{ 
	#ifdef BURST_WRITE_SUPPORT
		static const kal_uint16 addr_data_pair[] =
		{
			0x0100, 0x00,
			0x0101, 0x00,
			0x0204, 0x00,
			0x0205, 0x20,
			0x0200, 0x0D,
			0x0201, 0x78,
			//0x0202, 0x04,
			//0x0203, 0xE2,
			//0x0340, 0x09,
			//0x0341, 0xD0,
			0x0342, 0x0E,
			0x0343, 0x68,
			0x0344, 0x00,
			0x0345, 0x00,
			0x0346, 0x00,
			0x0347, 0x00,
			0x0348, 0x0C,
			0x0349, 0xCF,
			0x034A, 0x09,
			0x034B, 0x9F,
			0x034C, 0x0C,
			0x034D, 0xD0,
			0x034E, 0x09,
			0x034F, 0xA0,
			0x0390, 0x00,
			0x0391, 0x00,
			0x0940, 0x00,
			0x0381, 0x01,
			0x0383, 0x01,
			0x0385, 0x01,
			0x0387, 0x01,
			0x0301, 0x02,
			0x0303, 0x01,
			0x0305, 0x06,
			0x0306, 0x00,
			0x0307, 0x8C,
			0x0309, 0x02,
			0x030B, 0x01,
			0x3C59, 0x00,
			0x030D, 0x06,
			0x030E, 0x00,
			0x030F, 0xAF,
			0x3C5A, 0x00,
			0x0310, 0x01,
			0x3C50, 0x53,
			0x3C62, 0x02,
			0x3C63, 0xBC,
			0x3C64, 0x00,
			0x3C65, 0x00,
			0x0114, 0x03,
			0x3C1E, 0x00,
			0x3500, 0x0C,
			0x3C1A, 0xA8,
			0x3002, 0x03,
			0x0200, 0x0D,
			0x0201, 0x78,
			0x300A, 0x03,
			0x300C, 0x54,
			0x300D, 0x65,
			0x3010, 0x00,
			0x3012, 0x1D,
			0x3014, 0x19,
			0x3017, 0x0F,
			0x3018, 0x09,
			0x3019, 0x5B,
			0x301A, 0x78,
			0x3071, 0x00,
			0x3072, 0x00,
			0x3073, 0x00,
			0x3074, 0x00,
			0x3075, 0x00,
			0x3076, 0x0A,
			0x3077, 0x03,
			0x3078, 0x84,
			0x3079, 0x00,
			0x307A, 0x00,
			0x307B, 0x00,
			0x307C, 0x00,
			0x3085, 0x00,
			0x3086, 0x7A,
			0x30A6, 0x01,
			0x30A7, 0x06,
			0x3032, 0x01,
			0x304A, 0x02,
			0x3054, 0xF0,
			0x3044, 0x20,
			0x3045, 0x20,
			0x3047, 0x04,
			0x3048, 0x11,
			0x303D, 0x06,
			0x304B, 0x39,
			0x303A, 0xA0,
			0x302D, 0x7F,
			0x305E, 0x11,
			0x3038, 0x50,
			0x3097, 0x55,
			0x3096, 0x05,
			0x308F, 0x00,
			0x3230, 0x00,
			0x3042, 0x01,
			0x3053, 0x01,
			0x320D, 0x00,
			0x0100, 0x01
		};
		S5K4H5YX_2LANE_burst_write_cmos_sensor(addr_data_pair, sizeof(addr_data_pair)/sizeof(kal_uint16));
	#else
	S5K4H5YX_2LANE_write_cmos_sensor(0x0100, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0101, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0204, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0205, 0x20);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0200, 0x0D);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0201, 0x78);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0202, 0x09);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0203, 0xC8);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0340, 0x09);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0341, 0xE3);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0342, 0x0E);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0343, 0x68);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0344, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0345, 0x08);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0346, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0347, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0348, 0x0C);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0349, 0xC7);
	S5K4H5YX_2LANE_write_cmos_sensor(0x034A, 0x09);
	S5K4H5YX_2LANE_write_cmos_sensor(0x034B, 0x9F);
	S5K4H5YX_2LANE_write_cmos_sensor(0x034C, 0x0C);
	S5K4H5YX_2LANE_write_cmos_sensor(0x034D, 0xC0);
	S5K4H5YX_2LANE_write_cmos_sensor(0x034E, 0x09);
	S5K4H5YX_2LANE_write_cmos_sensor(0x034F, 0xA0);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0390, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0391, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0940, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0381, 0x01);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0383, 0x01);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0385, 0x01);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0387, 0x01);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0114, 0x01);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0301, 0x02);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0303, 0x01);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0305, 0x06);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0306, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0307, 0x46);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0309, 0x02);
	S5K4H5YX_2LANE_write_cmos_sensor(0x030B, 0x01);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3C59, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x030D, 0x06);
	S5K4H5YX_2LANE_write_cmos_sensor(0x030E, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x030F, 0xA2);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3C5A, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0310, 0x01);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3C50, 0x53);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3C62, 0x02);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3C63, 0x88);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3C64, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3C65, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3C1E, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3500, 0x0C);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3C1A, 0xA8);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3B29, 0x01);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3000, 0x07);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3001, 0x05);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3002, 0x03);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0200, 0x0D);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0201, 0x78);
	S5K4H5YX_2LANE_write_cmos_sensor(0x300A, 0x03);
	S5K4H5YX_2LANE_write_cmos_sensor(0x300C, 0x65);
	S5K4H5YX_2LANE_write_cmos_sensor(0x300D, 0x54);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3010, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3012, 0x14);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3014, 0x19);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3017, 0x0F);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3018, 0x1A);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3019, 0x6C);
	S5K4H5YX_2LANE_write_cmos_sensor(0x301A, 0x78);
	S5K4H5YX_2LANE_write_cmos_sensor(0x306F, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3070, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3071, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3072, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3073, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3074, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3075, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3076, 0x0A);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3077, 0x03);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3078, 0x84);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3079, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x307A, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x307B, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x307C, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3085, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3086, 0x72);
	S5K4H5YX_2LANE_write_cmos_sensor(0x30A6, 0x01);
	S5K4H5YX_2LANE_write_cmos_sensor(0x30A7, 0x0E);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3032, 0x01);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3037, 0x02);
	S5K4H5YX_2LANE_write_cmos_sensor(0x304A, 0x01);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3054, 0xF0);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3044, 0x20);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3045, 0x20);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3047, 0x04);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3048, 0x11);
	S5K4H5YX_2LANE_write_cmos_sensor(0x303D, 0x08);
	S5K4H5YX_2LANE_write_cmos_sensor(0x304B, 0x31);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3063, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x303A, 0xA0);
	S5K4H5YX_2LANE_write_cmos_sensor(0x302D, 0x7F);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3039, 0x45);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3038, 0x10);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3097, 0x11);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3096, 0x02);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3042, 0x01);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3053, 0x01);
	S5K4H5YX_2LANE_write_cmos_sensor(0x320D, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3204, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3205, 0x81);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3206, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3207, 0x81);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3208, 0x00);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3209, 0x81);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3213, 0x02);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3214, 0x91);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3215, 0x02);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3216, 0x91);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3217, 0x02);
	S5K4H5YX_2LANE_write_cmos_sensor(0x3218, 0x91);
	S5K4H5YX_2LANE_write_cmos_sensor(0x0100, 0x01);
	#endif
	}	
	
	mDELAY(100);
}



UINT32 S5K4H5YX2LANEOpen(void)
{
	volatile signed int i;
	kal_uint16 sensor_id = 0;

	for(i=0;i<3;i++)
	{
		sensor_id = (S5K4H5YX_2LANE_read_cmos_sensor(0x0000)<<8)|S5K4H5YX_2LANE_read_cmos_sensor(0x0001);

		S5K4H5YX2LANEDB("sensor_id=%x\n",sensor_id);

		if(sensor_id != S5K4H5YX_2LANE_SENSOR_ID)
			return ERROR_SENSOR_CONNECT_FAIL;
		else
			break;
	}
	
	spin_lock(&s5k4h5yx2lanemipiraw_drv_lock);
	s5k4h5yx_2lane.sensorMode = SENSOR_MODE_INIT;
	s5k4h5yx_2lane.S5K4H5YX2LANEAutoFlickerMode = KAL_FALSE;
	s5k4h5yx_2lane.DummyLines= 0;
	s5k4h5yx_2lane.DummyPixels= 0;
	spin_unlock(&s5k4h5yx2lanemipiraw_drv_lock);
	
#if defined(S5K4H5YX_2LANE_USE_AWB_OTP)
	S5K4H5YX_2LANE_MIPI_update_wb_register_from_otp();
#endif

    return ERROR_NONE;
}



UINT32 S5K4H5YX2LANEGetSensorID(UINT32 *sensorID)
{
    int  retry = 1;
	
    do {
        *sensorID = (S5K4H5YX_2LANE_read_cmos_sensor(0x0000)<<8)|S5K4H5YX_2LANE_read_cmos_sensor(0x0001);
		
        if (*sensorID == S5K4H5YX_2LANE_SENSOR_ID)
        {
        	S5K4H5YX2LANEDB("Sensor ID = 0x%04x\n", *sensorID);
			break;
        }
		
        S5K4H5YX2LANEDB("Read Sensor ID Fail = 0x%04x\n", *sensorID);

		retry--;
    } while (retry > 0);

    if (*sensorID != S5K4H5YX_2LANE_SENSOR_ID) 
	{
        *sensorID = 0xFFFFFFFF;
        return ERROR_SENSOR_CONNECT_FAIL;
    }
	
    return ERROR_NONE;
}



void S5K4H5YX_2LANE_SetShutter(kal_uint32 iShutter)
{
	S5K4H5YX2LANEDB("shutter = %d\n", iShutter);

	S5K4H5YX_2LANE_write_shutter(iShutter);
} 




UINT32 S5K4H5YX2LANEClose(void)
{
    return ERROR_NONE;
}



void S5K4H5YX2LANESetFlipMirror(kal_int32 imgMirror)
{
	S5K4H5YX2LANEDB("imgMirror = %d\n", imgMirror);

	switch (imgMirror)
    {
        case IMAGE_NORMAL: //B
            S5K4H5YX_2LANE_write_cmos_sensor(0x0101, 0x03);	//Set normal
            break;
        case IMAGE_V_MIRROR: //Gr X
            S5K4H5YX_2LANE_write_cmos_sensor(0x0101, 0x01);	//Set flip
            break;
        case IMAGE_H_MIRROR: //Gb
            S5K4H5YX_2LANE_write_cmos_sensor(0x0101, 0x02);	//Set mirror
            break;
        case IMAGE_HV_MIRROR: //R
            S5K4H5YX_2LANE_write_cmos_sensor(0x0101, 0x00);	//Set mirror and flip
            break;
    }

}



UINT32 S5K4H5YX2LANEPreview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window, MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	S5K4H5YX2LANEDB("Enter\n");

	if((s5k4h5yx_2lane.sensorMode != SENSOR_MODE_VIDEO) && (s5k4h5yx_2lane.sensorMode != SENSOR_MODE_PREVIEW))
		S5K4H5YX2LANEPreviewSetting();
		
	spin_lock(&s5k4h5yx2lanemipiraw_drv_lock);
	s5k4h5yx_2lane.sensorMode = SENSOR_MODE_PREVIEW; 
	s5k4h5yx_2lane.DummyPixels = 0;
	s5k4h5yx_2lane.DummyLines = 0 ;
	S5K4H5YX_2LANE_FeatureControl_PERIOD_PixelNum = s5k4h5yx_2lane_preview_line_length;
	S5K4H5YX_2LANE_FeatureControl_PERIOD_LineNum = s5k4h5yx_2lane_preview_frame_length;
	s5k4h5yx_2lane.imgMirror = sensor_config_data->SensorImageMirror;
	spin_unlock(&s5k4h5yx2lanemipiraw_drv_lock);
	
	S5K4H5YX2LANESetFlipMirror(IMAGE_HV_MIRROR);

	return ERROR_NONE;
}	



UINT32 S5K4H5YX2LANEVideo(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window, MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	S5K4H5YX2LANEDB("Enter\n");

	if((s5k4h5yx_2lane.sensorMode != SENSOR_MODE_VIDEO) && (s5k4h5yx_2lane.sensorMode != SENSOR_MODE_PREVIEW))
		S5K4H5YX2LANEPreviewSetting();

	spin_lock(&s5k4h5yx2lanemipiraw_drv_lock);
	s5k4h5yx_2lane.sensorMode = SENSOR_MODE_VIDEO;
	S5K4H5YX_2LANE_FeatureControl_PERIOD_PixelNum = s5k4h5yx_2lane_video_line_length;
	S5K4H5YX_2LANE_FeatureControl_PERIOD_LineNum = s5k4h5yx_2lane_video_frame_length;
	s5k4h5yx_2lane.imgMirror = sensor_config_data->SensorImageMirror;
	s5k4h5yx_2lane.DynamicVideoSupport = KAL_TRUE;
	spin_unlock(&s5k4h5yx2lanemipiraw_drv_lock);

	S5K4H5YX2LANESetFlipMirror(IMAGE_HV_MIRROR);

    return ERROR_NONE;
}



UINT32 S5K4H5YX2LANECapture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window, MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	S5K4H5YX2LANEDB("Enter\n");

	if( SENSOR_MODE_CAPTURE != s5k4h5yx_2lane.sensorMode)
	{
		S5K4H5YX2LANECaptureSetting();

		spin_lock(&s5k4h5yx2lanemipiraw_drv_lock);
		s5k4h5yx_2lane.sensorMode = SENSOR_MODE_CAPTURE;
		s5k4h5yx_2lane.imgMirror = sensor_config_data->SensorImageMirror;
		s5k4h5yx_2lane.DummyPixels = 0;
		s5k4h5yx_2lane.DummyLines = 0 ;
		S5K4H5YX_2LANE_FeatureControl_PERIOD_PixelNum = s5k4h5yx_2lane_capture_line_length;
		S5K4H5YX_2LANE_FeatureControl_PERIOD_LineNum = s5k4h5yx_2lane_capture_frame_length;
		spin_unlock(&s5k4h5yx2lanemipiraw_drv_lock);

		S5K4H5YX2LANESetFlipMirror(IMAGE_HV_MIRROR);
	}

    return ERROR_NONE;
}	



UINT32 S5K4H5YX2LANEGetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution)
{
    S5K4H5YX2LANEDB("Enter\n");

	pSensorResolution->SensorPreviewWidth	= s5k4h5yx_2lane_preview_width;
    pSensorResolution->SensorPreviewHeight	= s5k4h5yx_2lane_preview_height;
	pSensorResolution->SensorFullWidth		= s5k4h5yx_2lane_capture_width;
    pSensorResolution->SensorFullHeight		= s5k4h5yx_2lane_capture_height;
    pSensorResolution->SensorVideoWidth		= s5k4h5yx_2lane_video_width; 
    pSensorResolution->SensorVideoHeight    = s5k4h5yx_2lane_video_height;

	return ERROR_NONE;
} 



UINT32 S5K4H5YX2LANEGetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_INFO_STRUCT *pSensorInfo, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
	S5K4H5YX2LANEDB("Enter\n");

	pSensorInfo->SensorPreviewResolutionX= s5k4h5yx_2lane_preview_width;
	pSensorInfo->SensorPreviewResolutionY= s5k4h5yx_2lane_preview_height;
	pSensorInfo->SensorFullResolutionX= s5k4h5yx_2lane_capture_width;
    pSensorInfo->SensorFullResolutionY= s5k4h5yx_2lane_capture_height;
	
	spin_lock(&s5k4h5yx2lanemipiraw_drv_lock);
	s5k4h5yx_2lane.imgMirror = pSensorConfigData->SensorImageMirror ;
	spin_unlock(&s5k4h5yx2lanemipiraw_drv_lock);

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
    pSensorInfo->AEShutDelayFrame = 0;
    pSensorInfo->AESensorGainDelayFrame = 1;
    pSensorInfo->AEISPGainDelayFrame = 2;

    switch (ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            pSensorInfo->SensorClockFreq=s5k4h5yx_2lane_master_clock;
            pSensorInfo->SensorClockRisingCount= 0;
			pSensorInfo->SensorGrabStartX = s5k4h5yx_2lane_preview_startx;
            pSensorInfo->SensorGrabStartY = s5k4h5yx_2lane_preview_starty;
            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_2_LANE;
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	     	pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 6;
	    	pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorPacketECCOrder = 1;
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            pSensorInfo->SensorClockFreq=s5k4h5yx_2lane_master_clock;
            pSensorInfo->SensorClockRisingCount= 0;
            pSensorInfo->SensorGrabStartX = s5k4h5yx_2lane_video_startx;
            pSensorInfo->SensorGrabStartY = s5k4h5yx_2lane_video_starty;
            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_2_LANE;
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	     	pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 6;
	    	pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorPacketECCOrder = 1;
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
            pSensorInfo->SensorClockFreq=s5k4h5yx_2lane_master_clock;
            pSensorInfo->SensorClockRisingCount= 0;
			pSensorInfo->SensorGrabStartX = s5k4h5yx_2lane_capture_startx;	
            pSensorInfo->SensorGrabStartY = s5k4h5yx_2lane_capture_starty;	
            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_2_LANE;
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 6;
            pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorPacketECCOrder = 1;
            break;
        default:
			pSensorInfo->SensorClockFreq=s5k4h5yx_2lane_master_clock;
            pSensorInfo->SensorClockRisingCount= 0;
            pSensorInfo->SensorGrabStartX = s5k4h5yx_2lane_preview_startx;
            pSensorInfo->SensorGrabStartY = s5k4h5yx_2lane_preview_starty;
            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_2_LANE;
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	     	pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 6;
	    	pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorPacketECCOrder = 1;
            break;
    }

    memcpy(pSensorConfigData, &S5K4H5YX2LANESensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));

    return ERROR_NONE;
} 



UINT32 S5K4H5YX2LANEControl(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
	spin_lock(&s5k4h5yx2lanemipiraw_drv_lock);
	S5K4H5YX2LANECurrentScenarioId = ScenarioId;
	spin_unlock(&s5k4h5yx2lanemipiraw_drv_lock);

	S5K4H5YX2LANEDB("ScenarioId=%d\n",S5K4H5YX2LANECurrentScenarioId);

    switch (ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            S5K4H5YX2LANEPreview(pImageWindow, pSensorConfigData);
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			S5K4H5YX2LANEVideo(pImageWindow, pSensorConfigData);
			break;   
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
            S5K4H5YX2LANECapture(pImageWindow, pSensorConfigData);
            break;
        default:
            return ERROR_INVALID_SCENARIO_ID;
    }
	
    return ERROR_NONE;
}



UINT32 S5K4H5YX2LANESetVideoMode(UINT16 u2FrameRate)
{
    kal_uint32 MIN_Frame_length =0, frameRate=0, extralines=0;
	
    S5K4H5YX2LANEDB("frame rate = %d\n", u2FrameRate);
	
	if(u2FrameRate==0)
	{
		S5K4H5YX2LANEDB("Disable Video Mode or dynimac fps\n");
		
		spin_lock(&s5k4h5yx2lanemipiraw_drv_lock);
		s5k4h5yx_2lane.DynamicVideoSupport = KAL_TRUE;
		spin_unlock(&s5k4h5yx2lanemipiraw_drv_lock);
		
		return KAL_TRUE;
	}
	
	if(u2FrameRate >30 || u2FrameRate <5)
	    S5K4H5YX2LANEDB("error frame rate seting\n");

	if(s5k4h5yx_2lane.S5K4H5YX2LANEAutoFlickerMode == KAL_TRUE)
    {
    	if (u2FrameRate==30)
			frameRate= 302;
		else if(u2FrameRate==15)
			frameRate= 148;
		else
			frameRate=u2FrameRate*10;

		MIN_Frame_length = (s5k4h5yx_2lane_video_pixelclock)/(s5k4h5yx_2lane_video_line_length+ s5k4h5yx_2lane.DummyPixels)/frameRate*10;
    }
	else
		MIN_Frame_length = (s5k4h5yx_2lane_video_pixelclock) /(s5k4h5yx_2lane_video_line_length + s5k4h5yx_2lane.DummyPixels)/u2FrameRate;

	if((MIN_Frame_length <= s5k4h5yx_2lane_video_frame_length))
		MIN_Frame_length = s5k4h5yx_2lane_video_frame_length;
		
	extralines = MIN_Frame_length - s5k4h5yx_2lane_video_frame_length;

	S5K4H5YX_2LANE_SetDummy(s5k4h5yx_2lane.DummyPixels,extralines);
	spin_lock(&s5k4h5yx2lanemipiraw_drv_lock);
	s5k4h5yx_2lane.DummyPixels = 0;
	s5k4h5yx_2lane.DummyLines = extralines ;
	s5k4h5yx_2lane.DynamicVideoSupport = KAL_FALSE;
	spin_unlock(&s5k4h5yx2lanemipiraw_drv_lock);
	
	S5K4H5YX2LANEDB("MIN_Frame_length=%d,s5k4h5yx_2lane.DummyLines=%d\n",MIN_Frame_length,s5k4h5yx_2lane.DummyLines);

    return KAL_TRUE;
}



UINT32 S5K4H5YX2LANESetAutoFlickerMode(kal_bool bEnable, UINT16 u2FrameRate)
{
	S5K4H5YX2LANEDB(":%d\n", bEnable);
	if(bEnable) 
	{  
		spin_lock(&s5k4h5yx2lanemipiraw_drv_lock);
		s5k4h5yx_2lane.S5K4H5YX2LANEAutoFlickerMode = KAL_TRUE;
		spin_unlock(&s5k4h5yx2lanemipiraw_drv_lock);
    } 
	else 
	{
    	spin_lock(&s5k4h5yx2lanemipiraw_drv_lock);
        s5k4h5yx_2lane.S5K4H5YX2LANEAutoFlickerMode = KAL_FALSE;
		spin_unlock(&s5k4h5yx2lanemipiraw_drv_lock);
    }

    return ERROR_NONE;
}



UINT32 S5K4H5YX2LANESetTestPatternMode(kal_bool bEnable)
{
	S5K4H5YX2LANEDB(":%d\n", bEnable);	 
	
	if(bEnable) 	 
		S5K4H5YX_2LANE_write_cmos_sensor(0x0601, 0x02);
	else		  
		S5K4H5YX_2LANE_write_cmos_sensor(0x0601, 0x00);  

	return TRUE;
}



UINT32 S5K4H5YX2LANEMIPISetMaxFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 frameRate) 
{
	kal_int32 dummyLine, lineLength, frameHeight;

	if(frameRate == 0)
		return FALSE;
		
	switch (scenarioId) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			S5K4H5YX2LANEDB("MSDK_SCENARIO_ID_CAMERA_PREVIEW\n");
			lineLength = s5k4h5yx_2lane_preview_line_length;
			frameHeight = (10 * s5k4h5yx_2lane_preview_pixelclock) / frameRate / lineLength;
			dummyLine = frameHeight - s5k4h5yx_2lane_preview_frame_length;
			S5K4H5YX_2LANE_SetDummy(0, dummyLine);	

			spin_lock(&s5k4h5yx2lanemipiraw_drv_lock);
			s5k4h5yx_2lane.sensorMode = SENSOR_MODE_PREVIEW;
			spin_unlock(&s5k4h5yx2lanemipiraw_drv_lock);
			break;			
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			S5K4H5YX2LANEDB("MSDK_SCENARIO_ID_VIDEO_PREVIEW\n");
			lineLength = s5k4h5yx_2lane_video_line_length;
			frameHeight = (10*s5k4h5yx_2lane_video_pixelclock) / frameRate / lineLength;
			dummyLine = frameHeight - s5k4h5yx_2lane_video_frame_length;
			S5K4H5YX_2LANE_SetDummy(0, dummyLine);

			spin_lock(&s5k4h5yx2lanemipiraw_drv_lock);
			s5k4h5yx_2lane.sensorMode = SENSOR_MODE_VIDEO;
			spin_unlock(&s5k4h5yx2lanemipiraw_drv_lock);
			break;			
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:	
			S5K4H5YX2LANEDB("MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG or MSDK_SCENARIO_ID_CAMERA_ZSD\n");
			lineLength = s5k4h5yx_2lane_capture_line_length;
			frameHeight =  (10*s5k4h5yx_2lane_capture_pixelclock) / frameRate / lineLength;
			dummyLine = frameHeight - s5k4h5yx_2lane_capture_frame_length;
			S5K4H5YX_2LANE_SetDummy(0, dummyLine);

			spin_lock(&s5k4h5yx2lanemipiraw_drv_lock);
			s5k4h5yx_2lane.sensorMode = SENSOR_MODE_CAPTURE;
			spin_unlock(&s5k4h5yx2lanemipiraw_drv_lock);
			break;		
        case MSDK_SCENARIO_ID_CAMERA_3D_PREVIEW:
        case MSDK_SCENARIO_ID_CAMERA_3D_VIDEO:
        case MSDK_SCENARIO_ID_CAMERA_3D_CAPTURE:   
			break;		
		default:
			break;
	}	
	
	return ERROR_NONE;
}



UINT32 S5K4H5YX2LANEMIPIGetDefaultFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 *pframeRate) 
{
	switch (scenarioId) 
	{
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			 *pframeRate = s5k4h5yx_2lane_video_max_framerate;
			 break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
			 *pframeRate = s5k4h5yx_2lane_capture_max_framerate;
			break;		
        case MSDK_SCENARIO_ID_CAMERA_3D_PREVIEW: 
        case MSDK_SCENARIO_ID_CAMERA_3D_VIDEO:
        case MSDK_SCENARIO_ID_CAMERA_3D_CAPTURE:    
			 *pframeRate = s5k4h5yx_2lane_preview_max_framerate;
			break;		
		default:
			break;
	}

	return ERROR_NONE;
}



UINT32 S5K4H5YX2LANEFeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId, UINT8 *pFeaturePara, UINT32 *pFeatureParaLen)
{
    UINT16 *pFeatureReturnPara16=(UINT16 *) pFeaturePara, *pFeatureData16=(UINT16 *) pFeaturePara;
    UINT32 *pFeatureReturnPara32=(UINT32 *) pFeaturePara, *pFeatureData32=(UINT32 *) pFeaturePara, SensorRegNumber, i;
    PNVRAM_SENSOR_DATA_STRUCT pSensorDefaultData=(PNVRAM_SENSOR_DATA_STRUCT) pFeaturePara;
    MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData=(MSDK_SENSOR_CONFIG_STRUCT *) pFeaturePara;
    MSDK_SENSOR_REG_INFO_STRUCT *pSensorRegData=(MSDK_SENSOR_REG_INFO_STRUCT *) pFeaturePara;
    MSDK_SENSOR_GROUP_INFO_STRUCT *pSensorGroupInfo=(MSDK_SENSOR_GROUP_INFO_STRUCT *) pFeaturePara;
    MSDK_SENSOR_ITEM_INFO_STRUCT *pSensorItemInfo=(MSDK_SENSOR_ITEM_INFO_STRUCT *) pFeaturePara;
    MSDK_SENSOR_ENG_INFO_STRUCT	*pSensorEngInfo=(MSDK_SENSOR_ENG_INFO_STRUCT *) pFeaturePara;

    switch (FeatureId)
    {
        case SENSOR_FEATURE_GET_RESOLUTION:
            *pFeatureReturnPara16++= s5k4h5yx_2lane_capture_width;
            *pFeatureReturnPara16= s5k4h5yx_2lane_capture_height;
            *pFeatureParaLen=4;
            break;
        case SENSOR_FEATURE_GET_PERIOD:
			*pFeatureReturnPara16++= S5K4H5YX_2LANE_FeatureControl_PERIOD_PixelNum;
			*pFeatureReturnPara16= S5K4H5YX_2LANE_FeatureControl_PERIOD_LineNum;
			*pFeatureParaLen=4;
			break;
        case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
			switch(S5K4H5YX2LANECurrentScenarioId)
			{
				case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
					*pFeatureReturnPara32 = s5k4h5yx_2lane_preview_pixelclock;
					*pFeatureParaLen=4;
					break;
				case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
					*pFeatureReturnPara32 = s5k4h5yx_2lane_video_pixelclock;
					*pFeatureParaLen=4;
					break;	 
				case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				case MSDK_SCENARIO_ID_CAMERA_ZSD:
					*pFeatureReturnPara32 = s5k4h5yx_2lane_capture_pixelclock;
					*pFeatureParaLen=4;
					break;
				default:
					*pFeatureReturnPara32 = s5k4h5yx_2lane_preview_pixelclock;
					*pFeatureParaLen=4;
					break;
			}
		    break;
        case SENSOR_FEATURE_SET_ESHUTTER:
            S5K4H5YX_2LANE_SetShutter(*pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_GAIN:
            S5K4H5YX_2LANE_SetGain((UINT16) *pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_REGISTER:
            S5K4H5YX_2LANE_write_cmos_sensor(pSensorRegData->RegAddr, pSensorRegData->RegData);
            break;
        case SENSOR_FEATURE_GET_REGISTER:
            pSensorRegData->RegData = S5K4H5YX_2LANE_read_cmos_sensor(pSensorRegData->RegAddr);
            break;
        case SENSOR_FEATURE_SET_CCT_REGISTER:
            SensorRegNumber=FACTORY_END_ADDR;

			for (i=0;i<SensorRegNumber;i++)
            {
            	spin_lock(&s5k4h5yx2lanemipiraw_drv_lock);
                S5K4H5YX2LANESensorCCT[i].Addr=*pFeatureData32++;
                S5K4H5YX2LANESensorCCT[i].Para=*pFeatureData32++;
				spin_unlock(&s5k4h5yx2lanemipiraw_drv_lock);
            }
            break;
        case SENSOR_FEATURE_GET_CCT_REGISTER:
            SensorRegNumber=FACTORY_END_ADDR;
			
            if (*pFeatureParaLen<(SensorRegNumber*sizeof(SENSOR_REG_STRUCT)+4))
                return FALSE;

			*pFeatureData32++=SensorRegNumber;

			for (i=0;i<SensorRegNumber;i++)
            {
                *pFeatureData32++=S5K4H5YX2LANESensorCCT[i].Addr;
                *pFeatureData32++=S5K4H5YX2LANESensorCCT[i].Para;
            }
            break;
        case SENSOR_FEATURE_SET_ENG_REGISTER:
            SensorRegNumber=ENGINEER_END;

			for (i=0;i<SensorRegNumber;i++)
            {
            	spin_lock(&s5k4h5yx2lanemipiraw_drv_lock);
                S5K4H5YX2LANESensorReg[i].Addr=*pFeatureData32++;
                S5K4H5YX2LANESensorReg[i].Para=*pFeatureData32++;
				spin_unlock(&s5k4h5yx2lanemipiraw_drv_lock);
            }
            break;
        case SENSOR_FEATURE_GET_ENG_REGISTER:
            SensorRegNumber=ENGINEER_END;

			if (*pFeatureParaLen<(SensorRegNumber*sizeof(SENSOR_REG_STRUCT)+4))
                return FALSE;

			*pFeatureData32++=SensorRegNumber;

			for (i=0;i<SensorRegNumber;i++)
            {
                *pFeatureData32++=S5K4H5YX2LANESensorReg[i].Addr;
                *pFeatureData32++=S5K4H5YX2LANESensorReg[i].Para;
            }

			break;
        case SENSOR_FEATURE_GET_REGISTER_DEFAULT:
            if (*pFeatureParaLen>=sizeof(NVRAM_SENSOR_DATA_STRUCT))
            {
                pSensorDefaultData->Version=NVRAM_CAMERA_SENSOR_FILE_VERSION;
                pSensorDefaultData->SensorId=S5K4H5YX_2LANE_SENSOR_ID;
                memcpy(pSensorDefaultData->SensorEngReg, S5K4H5YX2LANESensorReg, sizeof(SENSOR_REG_STRUCT)*ENGINEER_END);
                memcpy(pSensorDefaultData->SensorCCTReg, S5K4H5YX2LANESensorCCT, sizeof(SENSOR_REG_STRUCT)*FACTORY_END_ADDR);
            }
            else
                return FALSE;

			*pFeatureParaLen=sizeof(NVRAM_SENSOR_DATA_STRUCT);
            break;
        case SENSOR_FEATURE_GET_CONFIG_PARA:
            memcpy(pSensorConfigData, &S5K4H5YX2LANESensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));
            *pFeatureParaLen=sizeof(MSDK_SENSOR_CONFIG_STRUCT);
            break;
        case SENSOR_FEATURE_CAMERA_PARA_TO_SENSOR:
            S5K4H5YX_camera_para_to_sensor();
            break;
        case SENSOR_FEATURE_SENSOR_TO_CAMERA_PARA:
            S5K4H5YX_2LANE_sensor_to_camera_para();
            break;
        case SENSOR_FEATURE_GET_GROUP_COUNT:
            *pFeatureReturnPara32++=S5K4H5YX_2LANE_get_sensor_group_count();
            *pFeatureParaLen=4;
            break;
        case SENSOR_FEATURE_GET_GROUP_INFO:
            S5K4H5YX_2LANE_get_sensor_group_info(pSensorGroupInfo->GroupIdx, pSensorGroupInfo->GroupNamePtr, &pSensorGroupInfo->ItemCount);
            *pFeatureParaLen=sizeof(MSDK_SENSOR_GROUP_INFO_STRUCT);
            break;
        case SENSOR_FEATURE_GET_ITEM_INFO:
            S5K4H5YX_2LANE_get_sensor_item_info(pSensorItemInfo->GroupIdx,pSensorItemInfo->ItemIdx, pSensorItemInfo);
            *pFeatureParaLen=sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
            break;
        case SENSOR_FEATURE_SET_ITEM_INFO:
            S5K4H5YX_2LANE_set_sensor_item_info(pSensorItemInfo->GroupIdx, pSensorItemInfo->ItemIdx, pSensorItemInfo->ItemValue);
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
            S5K4H5YX2LANESetVideoMode(*pFeatureData16);
            break;
        case SENSOR_FEATURE_CHECK_SENSOR_ID:
            S5K4H5YX2LANEGetSensorID(pFeatureReturnPara32);
            break;
        case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
            S5K4H5YX2LANESetAutoFlickerMode((BOOL)*pFeatureData16, *(pFeatureData16+1));
	        break;
        case SENSOR_FEATURE_SET_TEST_PATTERN:
            S5K4H5YX2LANESetTestPatternMode((BOOL)*pFeatureData16);
            break;
		case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
			S5K4H5YX2LANEMIPISetMaxFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32, *(pFeatureData32+1));
			break;
		case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
			S5K4H5YX2LANEMIPIGetDefaultFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32, (MUINT32 *)(*(pFeatureData32+1)));
			break;
		case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:		           
			*pFeatureReturnPara32= S5K4H5YX_2LANE_TEST_PATTERN_CHECKSUM;			
			*pFeatureParaLen=4; 										
			break;
		case SENSOR_FEATURE_SET_NIGHTMODE:
		case SENSOR_FEATURE_SET_FLASHLIGHT:
		case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		case SENSOR_FEATURE_INITIALIZE_AF:
		case SENSOR_FEATURE_CONSTANT_AF:
		case SENSOR_FEATURE_MOVE_FOCUS_LENS:
			break;
        default:
            break;
    }
    return ERROR_NONE;
}



SENSOR_FUNCTION_STRUCT	SensorFuncS5K4H5YX2LANE=
{
    S5K4H5YX2LANEOpen,
    S5K4H5YX2LANEGetInfo,
    S5K4H5YX2LANEGetResolution,
    S5K4H5YX2LANEFeatureControl,
    S5K4H5YX2LANEControl,
    S5K4H5YX2LANEClose
};

UINT32 S5K4H5YX_2LANE_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
    if (pfFunc!=NULL)
        *pfFunc=&SensorFuncS5K4H5YX2LANE;

    return ERROR_NONE;
} 

