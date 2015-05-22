/*******************************************************************************************/


/*******************************************************************************************/
     
#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include <asm/system.h>
#include <linux/kernel.h>//for printk


#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "t4k04mipiraw_Sensor.h"
#include "t4k04mipiraw_Camera_Sensor_para.h"
#include "t4k04mipiraw_CameraCustomized.h"
static DEFINE_SPINLOCK(t4k04mipiraw_drv_lock);

#define T4K04_DEBUG

#ifdef T4K04_DEBUG
	#define T4K04DB(fmt, arg...) printk( "[T4K04Raw] "  fmt, ##arg)
#else
	#define T4K04DB(x,...)
#endif


#define mDELAY(ms)  mdelay(ms)

MSDK_SENSOR_CONFIG_STRUCT T4K04SensorConfigData;

kal_uint32 T4K04_FAC_SENSOR_REG;

MSDK_SCENARIO_ID_ENUM T4K04CurrentScenarioId = MSDK_SCENARIO_ID_CAMERA_PREVIEW;

/* FIXME: old factors and DIDNOT use now. s*/
SENSOR_REG_STRUCT T4K04SensorCCT[]=CAMERA_SENSOR_CCT_DEFAULT_VALUE;
SENSOR_REG_STRUCT T4K04SensorReg[ENGINEER_END]=CAMERA_SENSOR_REG_DEFAULT_VALUE;
/* FIXME: old factors and DIDNOT use now. e*/

static T4K04_PARA_STRUCT t4k04;


extern int iReadReg(u16 a_u2Addr , u8 * a_puBuff , u16 i2cId);
extern int iWriteReg(u16 a_u2Addr , u32 a_u4Data , u32 a_u4Bytes , u16 i2cId);

#define T4K04_write_cmos_sensor(addr, para) iWriteReg((u16) addr , (u32) para , 1, T4K04MIPI_WRITE_ID)

kal_uint16 T4K04_read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte=0;
    iReadReg((u16) addr ,(u8*)&get_byte,T4K04MIPI_WRITE_ID);
    return get_byte;
}

#define Sleep(ms) mdelay(ms)

void T4K04_write_shutter(kal_uint32 shutter)
{
	kal_uint32 min_framelength = T4K04_PV_PERIOD_PIXEL_NUMS, max_shutter=0;
	kal_uint32 extra_lines = 0;
	kal_uint32 line_length = 0;
	kal_uint32 frame_length = 0;
	unsigned long flags;

	if(t4k04.T4K04AutoFlickerMode == KAL_TRUE)
	{
		if ( SENSOR_MODE_PREVIEW == t4k04.sensorMode )
		{
			line_length = T4K04_PV_PERIOD_PIXEL_NUMS + t4k04.DummyPixels;
			max_shutter = T4K04_PV_PERIOD_LINE_NUMS + t4k04.DummyLines ; 
		}
		else if(SENSOR_MODE_VIDEO == t4k04.sensorMode)
		{
			line_length = T4K04_VIDEO_PERIOD_PIXEL_NUMS + t4k04.DummyPixels;
			max_shutter = T4K04_VIDEO_PERIOD_LINE_NUMS + t4k04.DummyLines ; 
		}
		else	
		{
			line_length = T4K04_FULL_PERIOD_PIXEL_NUMS + t4k04.DummyPixels;
			max_shutter = T4K04_FULL_PERIOD_LINE_NUMS + t4k04.DummyLines ; 
		}

		switch(T4K04CurrentScenarioId)
		{
        	case MSDK_SCENARIO_ID_CAMERA_ZSD:
				min_framelength = (t4k04.capPclk*10000) /(T4K04_FULL_PERIOD_PIXEL_NUMS + t4k04.DummyPixels)/296*10 ;
				break;

			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				min_framelength = (t4k04.videoPclk*10000) /(T4K04_VIDEO_PERIOD_PIXEL_NUMS + t4k04.DummyPixels)/296*10 ;
				break;
			default:
				min_framelength = (t4k04.pvPclk*10000) /(T4K04_PV_PERIOD_PIXEL_NUMS + t4k04.DummyPixels)/296*10 ; 
    			break;
		}

		if (shutter < 1)
			shutter = 1;

		if (shutter > max_shutter)
			extra_lines = shutter - max_shutter;
		else
			extra_lines = 0;

		if ( SENSOR_MODE_PREVIEW == t4k04.sensorMode )
		{
			frame_length = T4K04_PV_PERIOD_LINE_NUMS+ t4k04.DummyLines + extra_lines ; 
		}
		else if(SENSOR_MODE_VIDEO == t4k04.sensorMode)
		{
			frame_length = T4K04_VIDEO_PERIOD_LINE_NUMS+ t4k04.DummyLines + extra_lines ; 
		}
		else			
		{ 
			frame_length = T4K04_FULL_PERIOD_LINE_NUMS + t4k04.DummyLines + extra_lines ; 
		}
		
		if (frame_length < min_framelength)
		{
			switch(T4K04CurrentScenarioId)
			{
        	case MSDK_SCENARIO_ID_CAMERA_ZSD:
				extra_lines = min_framelength- (T4K04_FULL_PERIOD_LINE_NUMS+ t4k04.DummyLines);
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				extra_lines = min_framelength- (T4K04_VIDEO_PERIOD_LINE_NUMS+ t4k04.DummyLines);
			default:
				extra_lines = min_framelength- (T4K04_PV_PERIOD_LINE_NUMS+ t4k04.DummyLines); 
    			break;
			}
			
			frame_length = min_framelength;//redefine frame_length
		}	
		
		//Set total frame length
		T4K04_write_cmos_sensor(0x0340, (frame_length >> 8) & 0xFF);
		T4K04_write_cmos_sensor(0x0341, frame_length & 0xFF);
		spin_lock_irqsave(&t4k04mipiraw_drv_lock,flags);
		t4k04.maxExposureLines = frame_length;
		spin_unlock_irqrestore(&t4k04mipiraw_drv_lock,flags);

		//Set shutter (Coarse integration time, uint: lines.)
		T4K04_write_cmos_sensor(0x0202, (shutter>>8) & 0xFF);
		T4K04_write_cmos_sensor(0x0203,  shutter& 0xFF);
		
		//T4K04DB("framerate(10 base) = %d\n",(t4k04.pvPclk*10000)*10 /line_length/frame_length);
		
		T4K04DB("AutoFlickerMode_ON:shutter=%d, extra_lines=%d, line_length=%d, frame_length=%d\n", shutter, extra_lines, line_length, frame_length);
	
	}
	else
	{
		if ( SENSOR_MODE_PREVIEW == t4k04.sensorMode )  
		{
			max_shutter = T4K04_PV_PERIOD_LINE_NUMS + t4k04.DummyLines ; 
		}
		else if(SENSOR_MODE_VIDEO == t4k04.sensorMode)
		{
			max_shutter = T4K04_VIDEO_PERIOD_LINE_NUMS + t4k04.DummyLines ; 
		}
		else	
		{
			max_shutter = T4K04_FULL_PERIOD_LINE_NUMS + t4k04.DummyLines ; 
		}
		
		if (shutter < 1)
			shutter = 1;

		if (shutter > max_shutter)
			extra_lines = shutter - max_shutter;
		else
			extra_lines = 0;

		if ( SENSOR_MODE_PREVIEW == t4k04.sensorMode )
		{
			line_length = T4K04_PV_PERIOD_PIXEL_NUMS + t4k04.DummyPixels; 
			frame_length = T4K04_PV_PERIOD_LINE_NUMS+ t4k04.DummyLines + extra_lines ; 
		}
		else if(SENSOR_MODE_VIDEO == t4k04.sensorMode)
		{
			line_length = T4K04_VIDEO_PERIOD_PIXEL_NUMS + t4k04.DummyPixels; 
			frame_length = T4K04_VIDEO_PERIOD_LINE_NUMS+ t4k04.DummyLines + extra_lines ; 
		}
		else				//QSXGA size output
		{
			line_length = T4K04_FULL_PERIOD_PIXEL_NUMS + t4k04.DummyPixels; 
			frame_length = T4K04_FULL_PERIOD_LINE_NUMS + t4k04.DummyLines + extra_lines ; 
		}


		//Set total frame length
		T4K04_write_cmos_sensor(0x0340, (frame_length >> 8) & 0xFF);
		T4K04_write_cmos_sensor(0x0341, frame_length & 0xFF);
		spin_lock_irqsave(&t4k04mipiraw_drv_lock,flags);
		t4k04.maxExposureLines = frame_length;
		spin_unlock_irqrestore(&t4k04mipiraw_drv_lock,flags);

		//Set shutter (Coarse integration time, uint: lines.)
		T4K04_write_cmos_sensor(0x0202, (shutter>>8) & 0xFF);
		T4K04_write_cmos_sensor(0x0203,  shutter& 0xFF);
		
		T4K04DB("AutoFlickerMode_OFF:shutter=%d, extra_lines=%d, line_length=%d, frame_length=%d\n", shutter, extra_lines, line_length, frame_length);
	}
	
}   /* write_T4K04_shutter */

/*******************************************************************************
* 
********************************************************************************/
static kal_uint16 T4K04Reg2Gain(const kal_uint16 iReg)
{
	kal_uint16 sensorGain=0x0000;	

    sensorGain = iReg/T4K04_ANALOG_GAIN_1X;  //get sensor gain multiple
    return sensorGain*BASEGAIN; 
}

/*******************************************************************************
* 
********************************************************************************/
static kal_uint16 T4K04Gain2Reg(const kal_uint16 Gain)
{
    kal_uint16 iReg = 0x0000;
	kal_uint16 sensorGain=0x0000;	

	iReg= Gain*T4K04_ANALOG_GAIN_1X/BASEGAIN;
	
	T4K04DB("T4K04Gain2Reg iReg =%d",iReg);
    return iReg;
}


void write_T4K04_gain(kal_uint16 gain)
{

	T4K04_write_cmos_sensor(0x0204,(gain>>8));
	T4K04_write_cmos_sensor(0x0205,(gain&0xff));

	return;
}

/*************************************************************************
* FUNCTION
*    T4K04_SetGain
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
void T4K04_SetGain(UINT16 iGain)
{
	UINT16 gain_reg=0;
	unsigned long flags;

	
	T4K04DB("4K04:featurecontrol--iGain=%d\n",iGain);

	spin_lock_irqsave(&t4k04mipiraw_drv_lock,flags);
	t4k04.realGain = iGain;//64 Base
	t4k04.sensorGlobalGain = T4K04Gain2Reg(iGain);
	spin_unlock_irqrestore(&t4k04mipiraw_drv_lock,flags);
	
	write_T4K04_gain(t4k04.sensorGlobalGain);	
	T4K04DB("[T4K04_SetGain]t4k04.sensorGlobalGain=0x%x,t4k04.realGain=%d\n",t4k04.sensorGlobalGain,t4k04.realGain);
	
}   /*  T4K04_SetGain_SetGain  */


/*************************************************************************
* FUNCTION
*    read_T4K04_gain
*
* DESCRIPTION
*    This function is to set global gain to sensor.
*
* PARAMETERS
*    None
*
* RETURNS
*    gain : sensor global gain
*
* GLOBALS AFFECTED
*
*************************************************************************/
kal_uint16 read_T4K04_gain(void)
{
    kal_uint8  temp_reg;
	kal_uint16 sensor_gain =0, read_gain=0;

	read_gain=((T4K04_read_cmos_sensor(0x0204) << 8) | T4K04_read_cmos_sensor(0x0205));
	
	spin_lock(&t4k04mipiraw_drv_lock);
	t4k04.sensorGlobalGain = read_gain; 
	t4k04.realGain = T4K04Reg2Gain(t4k04.sensorGlobalGain);
	spin_unlock(&t4k04mipiraw_drv_lock);
	
	T4K04DB("t4k04.sensorGlobalGain=0x%x,t4k04.realGain=%d\n",t4k04.sensorGlobalGain,t4k04.realGain);
	
	return t4k04.sensorGlobalGain;
} 


void T4K04_camera_para_to_sensor(void)
{
    kal_uint32    i;
    for(i=0; 0xFFFFFFFF!=T4K04SensorReg[i].Addr; i++)
    {
        T4K04_write_cmos_sensor(T4K04SensorReg[i].Addr, T4K04SensorReg[i].Para);
    }
    for(i=ENGINEER_START_ADDR; 0xFFFFFFFF!=T4K04SensorReg[i].Addr; i++)
    {
        T4K04_write_cmos_sensor(T4K04SensorReg[i].Addr, T4K04SensorReg[i].Para);
    }
    for(i=FACTORY_START_ADDR; i<FACTORY_END_ADDR; i++)
    {
        T4K04_write_cmos_sensor(T4K04SensorCCT[i].Addr, T4K04SensorCCT[i].Para);
    }
}


/*************************************************************************
* FUNCTION
*    T4K04_sensor_to_camera_para
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
void T4K04_sensor_to_camera_para(void)
{
    kal_uint32    i, temp_data;
    for(i=0; 0xFFFFFFFF!=T4K04SensorReg[i].Addr; i++)
    {
         temp_data = T4K04_read_cmos_sensor(T4K04SensorReg[i].Addr);
		 spin_lock(&t4k04mipiraw_drv_lock);
		 T4K04SensorReg[i].Para =temp_data;
		 spin_unlock(&t4k04mipiraw_drv_lock);
    }
    for(i=ENGINEER_START_ADDR; 0xFFFFFFFF!=T4K04SensorReg[i].Addr; i++)
    {
        temp_data = T4K04_read_cmos_sensor(T4K04SensorReg[i].Addr);
		spin_lock(&t4k04mipiraw_drv_lock);
		T4K04SensorReg[i].Para = temp_data;
		spin_unlock(&t4k04mipiraw_drv_lock);
    }
}

/*************************************************************************
* FUNCTION
*    T4K04_get_sensor_group_count
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
kal_int32  T4K04_get_sensor_group_count(void)
{
    return GROUP_TOTAL_NUMS;
}

void T4K04_get_sensor_group_info(kal_uint16 group_idx, kal_int8* group_name_ptr, kal_int32* item_count_ptr)
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
            ASSERT(0);
}
}

void T4K04_get_sensor_item_info(kal_uint16 group_idx,kal_uint16 item_idx, MSDK_SENSOR_ITEM_INFO_STRUCT* info_ptr)
{
    kal_int16 temp_reg=0;
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

            temp_para= T4K04SensorCCT[temp_addr].Para;
			//temp_gain= (temp_para/t4k04.sensorBaseGain) * 1000;

            info_ptr->ItemValue=temp_gain;
            info_ptr->IsTrueFalse=KAL_FALSE;
            info_ptr->IsReadOnly=KAL_FALSE;
            info_ptr->IsNeedRestart=KAL_FALSE;
            info_ptr->Min= T4K04_MIN_ANALOG_GAIN * 1000;
            info_ptr->Max= T4K04_MAX_ANALOG_GAIN * 1000;
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
    }
}



kal_bool T4K04_set_sensor_item_info(kal_uint16 group_idx, kal_uint16 item_idx, kal_int32 ItemValue)
{
//   kal_int16 temp_reg;
   kal_uint16  temp_gain=0,temp_addr=0, temp_para=0;

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
                 ASSERT(0);
          }

		 temp_gain=((ItemValue*BASEGAIN+500)/1000);			//+500:get closed integer value

		  if(temp_gain>=1*BASEGAIN && temp_gain<=16*BASEGAIN)
          {
//             temp_para=(temp_gain * t4k04.sensorBaseGain + BASEGAIN/2)/BASEGAIN;
          }          
          else
			  ASSERT(0);

		  spin_lock(&t4k04mipiraw_drv_lock);
          T4K04SensorCCT[temp_addr].Para = temp_para;
		  spin_unlock(&t4k04mipiraw_drv_lock);
          T4K04_write_cmos_sensor(T4K04SensorCCT[temp_addr].Addr,temp_para);

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
					spin_lock(&t4k04mipiraw_drv_lock);
                    T4K04_FAC_SENSOR_REG=ItemValue;
					spin_unlock(&t4k04mipiraw_drv_lock);
                    break;
                case 1:
                    T4K04_write_cmos_sensor(T4K04_FAC_SENSOR_REG,ItemValue);
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

static void T4K04_SetDummy( const kal_uint32 iPixels, const kal_uint32 iLines )//checkd
{
	kal_uint32 line_length = 0;
	kal_uint32 frame_length = 0;

	if ( SENSOR_MODE_PREVIEW == t4k04.sensorMode )
	{
		line_length = T4K04_PV_PERIOD_PIXEL_NUMS + iPixels;
		frame_length = T4K04_PV_PERIOD_LINE_NUMS + iLines;
	}
	else if(SENSOR_MODE_VIDEO == t4k04.sensorMode)
	{
		line_length = T4K04_VIDEO_PERIOD_PIXEL_NUMS + iPixels;
		frame_length = T4K04_VIDEO_PERIOD_LINE_NUMS + iLines;
	}
	else				
	{
		line_length = T4K04_FULL_PERIOD_PIXEL_NUMS + iPixels;
		frame_length = T4K04_FULL_PERIOD_LINE_NUMS + iLines;
	}
	
	
	//Set total frame length
	T4K04_write_cmos_sensor(0x0340, (frame_length >> 8) & 0xFF);
	T4K04_write_cmos_sensor(0x0341, frame_length & 0xFF);
	spin_lock(&t4k04mipiraw_drv_lock);
	t4k04.maxExposureLines = frame_length;
	t4k04.DummyPixels =iPixels;
	t4k04.DummyLines =iLines;
	spin_unlock(&t4k04mipiraw_drv_lock);

	//Set total line length
	T4K04_write_cmos_sensor(0x0342, (line_length >> 8) & 0xFF);
	T4K04_write_cmos_sensor(0x0343, line_length & 0xFF);
	
}

static void T4K04_Sensor_Init(void)
{
	T4K04DB("T4K04_Sensor_Init enter_4lane :\n ");	

	T4K04_write_cmos_sensor(0x0000,0x14);//,[RO] RO_MODEL_ID[15:8];
	T4K04_write_cmos_sensor(0x0001,0x50);//,[RO] RO_MODEL_ID[7:0];
	T4K04_write_cmos_sensor(0x0005,0xFF);//,[RO] R_FRAME_COUNT[7:0];
	T4K04_write_cmos_sensor(0x0101,0x03);//,-/-/-/-/-/-/IMAGE_ORIENT[1:0];
	T4K04_write_cmos_sensor(0x0103,0x00);//,-/-/-/-/-/-/-/SOFTWARE_RESET;
	T4K04_write_cmos_sensor(0x0104,0x00);//,-/-/-/-/-/-/-/GROUP_PARA_HOLD;
	T4K04_write_cmos_sensor(0x0105,0x00);//,-/-/-/-/-/-/-/MSK_CORRUPT_FR;
	T4K04_write_cmos_sensor(0x0110,0x00);//,-/-/-/-/-/CSI_CHAN_IDNTF[2:0];
	T4K04_write_cmos_sensor(0x0111,0x02);//,-/-/-/-/-/-/CSI_SIG_MODE[1:0];
	T4K04_write_cmos_sensor(0x0112,0x0A);//,CSI_DATA_FORMAT[15:8];
	T4K04_write_cmos_sensor(0x0113,0x0A);//,CSI_DATA_FORMAT[7:0];
	
	//T4K04_write_cmos_sensor(0x0114,0x01);//-/-/-/-/-/CSI_LANE_MODE[2:0];//2lane
	T4K04_write_cmos_sensor(0x0114,0x03);//-/-/-/-/-/CSI_LANE_MODE[2:0];//4lane
	
	T4K04_write_cmos_sensor(0x0115,0x30);//,-/-/CSI_10TO8_DT[5:0];
	T4K04_write_cmos_sensor(0x0117,0x32);//,-/-/CSI_10TO6_DT[5:0];
	T4K04_write_cmos_sensor(0x0118,0x33);//,-/-/CSI_12TO8_DT[5:0];
	T4K04_write_cmos_sensor(0x0200,0x0B);//,[RO] -/RO_FINE_INTEGR_TIM[14:8];
	T4K04_write_cmos_sensor(0x0201,0x02);//,[RO] RO_FINE_INTEGR_TIM[7:0];
#if 0
	T4K04_write_cmos_sensor(0x0202,0x09);//,COAR_INTEGR_TIM[15:8];
	T4K04_write_cmos_sensor(0x0203,0xC0);//,COAR_INTEGR_TIM[7:0];
	T4K04_write_cmos_sensor(0x0204,0x00);//,ANA_GA_CODE_GL[15:8];
	T4K04_write_cmos_sensor(0x0205,0x41);//,ANA_GA_CODE_GL[7:0];
#endif
	T4K04_write_cmos_sensor(0x020E,0x01);//,-/-/-/-/-/-/DG_GA_GREENR[9:8];
	T4K04_write_cmos_sensor(0x020F,0x00);//,DG_GA_GREENR[7:0];
	T4K04_write_cmos_sensor(0x0210,0x01);//,-/-/-/-/-/-/DG_GA_RED[9:8];
	T4K04_write_cmos_sensor(0x0211,0x00);//,DG_GA_RED[7:0];
	T4K04_write_cmos_sensor(0x0212,0x01);//,-/-/-/-/-/-/DG_GA_BLUE[9:8];
	T4K04_write_cmos_sensor(0x0213,0x00);//,DG_GA_BLUE[7:0];
	T4K04_write_cmos_sensor(0x0214,0x01);//,-/-/-/-/-/-/DG_GA_GREENB[9:8];
	T4K04_write_cmos_sensor(0x0215,0x00);//,DG_GA_GREENB[7:0];
	
	T4K04_write_cmos_sensor(0x0301,0x06);//-/-/-/-/VT_PIX_CLK_DIV[3:0];
	T4K04_write_cmos_sensor(0x0303,0x01);//-/-/-/-/VT_SYS_CLK_DIV[3:0];
	T4K04_write_cmos_sensor(0x0305,0x05);//-/-/-/-/-/PRE_PLL_CLK_DIV[2:0];
	T4K04_write_cmos_sensor(0x0306,0x00);//-/-/-/-/-/-/-/PLL_MULTIPLIER[8];
	T4K04_write_cmos_sensor(0x0307,0xF2);//PLL_MULTIPLIER[7:0];
	T4K04_write_cmos_sensor(0x0309,0x08);//,- / - / - / - / OP_PIX_CLK_DIV[3:0] ;
	T4K04_write_cmos_sensor(0x030B,0x01);//,- / - / - / - / OP_SYS_CLK_DIV[3:0] ;
	T4K04_write_cmos_sensor(0x0340,0x09);//,FR_LENGTH_LINES[15:8] ;
	T4K04_write_cmos_sensor(0x0341,0x32);//,FR_LENGTH_LINES[7:0] ;
	T4K04_write_cmos_sensor(0x0342,0x0D);//,LINE_LENGTH_PCK[15:8] ;
	T4K04_write_cmos_sensor(0x0343,0x54);//,LINE_LENGTH_PCK[7:0] ;
	T4K04_write_cmos_sensor(0x0344,0x00);//,X_ADDR_START[15:8] ;
	T4K04_write_cmos_sensor(0x0345,0x00);//,X_ADDR_START[7:0] ;
	T4K04_write_cmos_sensor(0x0346,0x00);//,Y_ADDR_START[15:8] ;
	T4K04_write_cmos_sensor(0x0347,0x00);//,Y_ADDR_START[7:0] ;
	T4K04_write_cmos_sensor(0x0348,0x0C);//,X_ADDR_END[15:8] ;
	T4K04_write_cmos_sensor(0x0349,0xCF);//,X_ADDR_END[7:0] ;
	T4K04_write_cmos_sensor(0x034A,0x09);//,Y_ADDR_END[15:8] ;
	T4K04_write_cmos_sensor(0x034B,0x9F);//,Y_ADDR_END[7:0] ;
	T4K04_write_cmos_sensor(0x034C,0x06);//,X_OUTPUT_SIZE[15:8] ;
	T4K04_write_cmos_sensor(0x034D,0x68);//,X_OUTPUT_SIZE[7:0] ;
	T4K04_write_cmos_sensor(0x034E,0x04);//,Y_OUTPUT_SIZE[15:8] ;
	T4K04_write_cmos_sensor(0x034F,0xD0);//,Y_OUTPUT_SIZE[7:0] ;
	T4K04_write_cmos_sensor(0x0381,0x01);//,-/-/-/-/X_EVEN_INC[3:0];
	T4K04_write_cmos_sensor(0x0383,0x01);//,-/-/-/-/X_ODD_INC[3:0];
	T4K04_write_cmos_sensor(0x0385,0x01);//,[RO] -/-/-/-/-/-/-/RO_Y_EVEN_INC;
	T4K04_write_cmos_sensor(0x0387,0x01);//,[RO] -/-/-/-/-/-/-/RO_Y_ODD_INC;
	T4K04_write_cmos_sensor(0x0401,0x02);//,- / - / - / - / - / - / SCALING_MODE[1:0] ;
	T4K04_write_cmos_sensor(0x0403,0x00);//,- / - / - / - / - / - / - / SPATIAL_SAMPLING ;
	T4K04_write_cmos_sensor(0x0405,0x10);//,SCALE_M[7:0] ;
	T4K04_write_cmos_sensor(0x0408,0x00);//,- / - / - / DCROP_XOFS[12:8] ;
	T4K04_write_cmos_sensor(0x0409,0x00);//,DCROP_XOFS[7:0] ;
	T4K04_write_cmos_sensor(0x040A,0x00);//,- / - / - / - / DCROP_YOFS[11:8] ;
	T4K04_write_cmos_sensor(0x040B,0x00);//,DCROP_YOFS[7:0] ;
	T4K04_write_cmos_sensor(0x040C,0x0C);//,- / - / - / DCROP_WIDTH[12:8] ;
	T4K04_write_cmos_sensor(0x040D,0xD0);//,DCROP_WIDTH[7:0] ;
	T4K04_write_cmos_sensor(0x040E,0x09);//,- / - / - / - / DCROP_HIGT[11:8] ;
	T4K04_write_cmos_sensor(0x040F,0xA0);//,DCROP_HIGT[7:0] ;
	T4K04_write_cmos_sensor(0x0500,0x00);//,[RO] RO_COMPRESSION_MODE[15:8];
	T4K04_write_cmos_sensor(0x0501,0x01);//,[RO] RO_COMPRESSION_MODE[7:0];
	T4K04_write_cmos_sensor(0x0601,0x00);//,TEST_PATT_MODE[7:0];
	T4K04_write_cmos_sensor(0x0602,0x02);//,-/-/-/-/-/-/TEST_DATA_RED[9:8];
	T4K04_write_cmos_sensor(0x0603,0xC0);//,TEST_DATA_RED[7:0];
	T4K04_write_cmos_sensor(0x0604,0x02);//,-/-/-/-/-/-/TEST_DATA_GREENR[9:8];
	T4K04_write_cmos_sensor(0x0605,0xC0);//,TEST_DATA_GREENR[7:0];
	T4K04_write_cmos_sensor(0x0606,0x02);//,-/-/-/-/-/-/TEST_DATA_BLUE[9:8];
	T4K04_write_cmos_sensor(0x0607,0xC0);//,TEST_DATA_BLUE[7:0];
	T4K04_write_cmos_sensor(0x0608,0x02);//,-/-/-/-/-/-/TEST_DATA_GREENB[9:8];
	T4K04_write_cmos_sensor(0x0609,0xC0);//,TEST_DATA_GREENB[7:0];
	T4K04_write_cmos_sensor(0x060A,0x00);//,HO_CURS_WIDTH[15:8];
	T4K04_write_cmos_sensor(0x060B,0x00);//,HO_CURS_WIDTH[7:0];
	T4K04_write_cmos_sensor(0x060C,0x00);//,HO_CURS_POSITION[15:8];
	T4K04_write_cmos_sensor(0x060D,0x00);//,HO_CURS_POSITION[7:0];
	T4K04_write_cmos_sensor(0x060E,0x00);//,VE_CURS_WIDTH[15:8];
	T4K04_write_cmos_sensor(0x060F,0x00);//,VE_CURS_WIDTH[7:0];
	T4K04_write_cmos_sensor(0x0610,0x00);//,VE_CURS_POSITION[15:8];
	T4K04_write_cmos_sensor(0x0611,0x00);//,VE_CURS_POSITION[7:0];
	T4K04_write_cmos_sensor(0x0800,0x80);//,TCLK_POST[7:3]/-/-/-;
	T4K04_write_cmos_sensor(0x0801,0x28);//,THS_PREPARE[7:3]/-/-/-;
	T4K04_write_cmos_sensor(0x0802,0x68);//,THS_ZERO[7:3]/-/-/-;
	T4K04_write_cmos_sensor(0x0803,0x48);//,THS_TRAIL[7:3]/-/-/-;
	T4K04_write_cmos_sensor(0x0804,0x40);//,TCLK_TRAIL[7:3]/-/-/-;
	T4K04_write_cmos_sensor(0x0805,0x28);//,TCLK_PREPARE[7:3]/-/-/-;
	T4K04_write_cmos_sensor(0x0806,0xF8);//,TCLK_ZERO[7:3]/-/-/-;
	T4K04_write_cmos_sensor(0x0807,0x38);//,TLPX[7:3]/-/-/-;
	T4K04_write_cmos_sensor(0x0808,0x01);//,-/-/-/-/-/-/DPHY_CTRL[1:0];
	T4K04_write_cmos_sensor(0x0820,0x6D);//,MSB_LBRATE[31:24];
	T4K04_write_cmos_sensor(0x0821,0x0E);//,MSB_LBRATE[23:16];
	T4K04_write_cmos_sensor(0x0900,0x01);//,-/-/-/-/-/-/-/BINNING_MODE;
	T4K04_write_cmos_sensor(0x0901,0x22);//,BINNING_TYPE[7:0];
	T4K04_write_cmos_sensor(0x0902,0x00);//,-/-/-/-/-/-/BINNING_WEIGHTING[1:0];
	T4K04_write_cmos_sensor(0x0B05,0x00);//,-/-/-/-/-/-/-/MAP_DEF_EN;
	T4K04_write_cmos_sensor(0x0B06,0x01);//,-/-/-/-/-/-/-/SGL_DEF_EN;
	T4K04_write_cmos_sensor(0x0B07,0x40);//,-/SGL_DEF_W[6:0];
	T4K04_write_cmos_sensor(0x0B0A,0x00);//,-/-/-/-/-/-/-/CONB_CPLT_SGL_DEF_EN;
	T4K04_write_cmos_sensor(0x0B0B,0x40);//,-/CONB_CPLT_SGL_DEF_W[6:0];
	T4K04_write_cmos_sensor(0x3000,0x00);//,-/-/LB_LANE_SELA[1:0]/LBTEST_CLR/LB_TEST_EN/LBTEST_SELB/LB_MODE;
	T4K04_write_cmos_sensor(0x3001,0x00);//,[RO] -/RO_LBTEST_ERR_K/RO_LBTEST_ERR_M/RO_LBTEST_CNT[4:0];
	T4K04_write_cmos_sensor(0x3002,0x00);//,[RO] RO_CRC[15:8];
	T4K04_write_cmos_sensor(0x3003,0x00);//,[RO] RO_CRC[7:0];
	T4K04_write_cmos_sensor(0x3004,0x00);//,LANE_OFF_MN/DPCM6EMB/EMB_OUT_SW/ESC_AUTO_OFF/-/-/CLKULPS/ESCREQ;
	T4K04_write_cmos_sensor(0x3005,0x78);//,ESCDATA[7:0];
	//no back to LP from HS
	//T4K04_write_cmos_sensor(0x3006,0x42);//PISO_STP_X/MIPI_CLK_MODE/-/-/-/-/-/- ;
	//back to LP from HS
	T4K04_write_cmos_sensor(0x3006,0x02);//PISO_STP_X/MIPI_CLK_MODE/-/-/-/-/-/- ;
	T4K04_write_cmos_sensor(0x3007,0x30);//,HS_SR_CNT[1:0]/LP_SR_CNT[1:0]/REG18_CNT[3:0];
	T4K04_write_cmos_sensor(0x3008,0xFF);//,LANE_OFF[7:0];
	T4K04_write_cmos_sensor(0x3009,0x44);//,-/PHASE_ADJ_A[2:0]/-/-/-/-;
	T4K04_write_cmos_sensor(0x300A,0x00);//,-/LVDS_CKA_DELAY[2:0]/-/-/-/-;
	T4K04_write_cmos_sensor(0x300B,0x00);//,-/LVDS_D1_DELAY[2:0]/-/LVDS_D2_DELAY[2:0];
	T4K04_write_cmos_sensor(0x300C,0x00);//,-/LVDS_D3_DELAY[2:0]/-/LVDS_D4_DELAY[2:0];
	T4K04_write_cmos_sensor(0x300F,0x00);//,-/-/-/-/-/-/EN_PHASE_SEL[1:0];
	T4K04_write_cmos_sensor(0x3010,0x00);//,-/-/-/-/-/-/FIFODLY[9:8];
	T4K04_write_cmos_sensor(0x3011,0x00);//,FIFODLY[7:0];
	T4K04_write_cmos_sensor(0x3012,0x01);//,-/-/SM_PARA_SW[1:0]/-/-/-/LNKBTWK_ON;
	T4K04_write_cmos_sensor(0x3013,0xA7);//,NUMWAKE[7:0];
	T4K04_write_cmos_sensor(0x3014,0x40);//,-/ZERO_CONV_MR/ZERO_CONV_MD/-/SM8SM[3:0];
	T4K04_write_cmos_sensor(0x3015,0x00);//,T_VALUE1[7:0];
	T4K04_write_cmos_sensor(0x3016,0x00);//,T_VALUE2[7:0];
	T4K04_write_cmos_sensor(0x3017,0x00);//,T_VALUE3[7:0];
	T4K04_write_cmos_sensor(0x3018,0x00);//,T_VALUE4[7:0];
	T4K04_write_cmos_sensor(0x301D,0x01);//,MIPI_FS_CD[3:0]/MIPI_FE_CD[3:0];
	T4K04_write_cmos_sensor(0x3050,0x25);//,-/AD_CNTL[2:0]/-/ST_CNTL[2:0];
	T4K04_write_cmos_sensor(0x3051,0x05);//,-/-/-/-/-/BST_CNTL[2:0];
	T4K04_write_cmos_sensor(0x3060,0x00);//,-/-/-/-/-/H_CROP[2:0];
	T4K04_write_cmos_sensor(0x3061,0x40);//,-/HCRP_AUTO/-/HGAIN2/-/-/HFCORROFF/EQ_MONI;
	T4K04_write_cmos_sensor(0x3062,0x01);//,-/-/-/-/-/-/-/HLNRBLADJ_HOLD;
	T4K04_write_cmos_sensor(0x3070,0x00);//,-/-/-/-/-/MONI_MODE[2:0];
	T4K04_write_cmos_sensor(0x30A0,0x11);//,[RO] PISO[15:8];
	T4K04_write_cmos_sensor(0x30A1,0x10);//,[RO] PISO[7:0];
	T4K04_write_cmos_sensor(0x30A2,0xF8);//,DCLK_DRVUP/DOUT1_7_DRVUP/SDA_DRVUP/FLASH_DRVUP/DOUT0_DRVUP/-/VLAT_;
	T4K04_write_cmos_sensor(0x30A3,0xE8);//,PARA_HZ/DCLK_POL/GLB_HZ/-/AF_HZ/-/-/-;
	T4K04_write_cmos_sensor(0x30A4,0x80);//,WAIT_TIME_SEL[1:0]/-/-/VTCK_SEL[1:0]/OPCK_SEL[1:0];
	T4K04_write_cmos_sensor(0x30A5,0x00);//,SLEEP_SW/VCO_STP_SW/PHY_PWRON_A/-/SLEEP_MN/VCO_STP_MN/PHY_PWRON_MN;
	T4K04_write_cmos_sensor(0x30A6,0x03);//,PLL_SNR_CNTL[1:0]/PLL_SYS_CNTL[1:0]/-/-/VCO_EN/DIVRSTX;
	T4K04_write_cmos_sensor(0x30A7,0x00);//,AUTO_IR_SEL/-/ICP_SEL[1:0]/LPFR_SEL[1:0]/-/-;
	T4K04_write_cmos_sensor(0x30A8,0x21);//,PCMODE/-/ICP_PCH/ICP_NCH/-/-/-/VCO_TESTSEL;
	T4K04_write_cmos_sensor(0x30A9,0x00);//,-/-/AMON0_SEL[1:0]/-/REGVD_SEL/PLLEV_EN/PLLEV_SEL;
	T4K04_write_cmos_sensor(0x30AA,0xCC);//,VOUT_SWSB[3:0]/VOUT_SEL[3:0];
	T4K04_write_cmos_sensor(0x30AB,0x13);//,-/-/CL_SEL[1:0]/-/-/BIAS_SEL/CAMP_EN;
	T4K04_write_cmos_sensor(0x30AC,0x18);//,MHZ_EXTCLK_TB[15:8];
	T4K04_write_cmos_sensor(0x30AD,0x00);//,MHZ_EXTCLK_TB[7:0];
	T4K04_write_cmos_sensor(0x30B1,0x00);//,-/-/-/PARA_SW[4:0];
	T4K04_write_cmos_sensor(0x30B2,0x01);//,-/-/-/-/-/-/-/LSTOP;
	T4K04_write_cmos_sensor(0x3100,0x00);//,OTP_STA/-/-/-/-/OTP_CLRE/OTP_WREC/OTP_ENBL;
	T4K04_write_cmos_sensor(0x3101,0x00);//,[RO] -/-/-/-/OTP_IIU/OTP_DCOR/OTP_WIR/OTP_RIR;
	T4K04_write_cmos_sensor(0x3102,0x00);//,OTP_PSEL[7:0];
	T4K04_write_cmos_sensor(0x3104,0x00);//,OTP_DATA0[7:0];
	T4K04_write_cmos_sensor(0x3105,0x00);//,OTP_DATA1[7:0];
	T4K04_write_cmos_sensor(0x3106,0x00);//,OTP_DATA2[7:0];
	T4K04_write_cmos_sensor(0x3107,0x00);//,OTP_DATA3[7:0];
	T4K04_write_cmos_sensor(0x3108,0x00);//,OTP_DATA4[7:0];
	T4K04_write_cmos_sensor(0x3109,0x00);//,OTP_DATA5[7:0];
	T4K04_write_cmos_sensor(0x310A,0x00);//,OTP_DATA6[7:0];
	T4K04_write_cmos_sensor(0x310B,0x00);//,OTP_DATA7[7:0];
	T4K04_write_cmos_sensor(0x310C,0x00);//,OTP_DATA8[7:0];
	T4K04_write_cmos_sensor(0x310D,0x00);//,OTP_DATA9[7:0];
	T4K04_write_cmos_sensor(0x310E,0x00);//,OTP_DATA10[7:0];
	T4K04_write_cmos_sensor(0x310F,0x00);//,OTP_DATA11[7:0];
	T4K04_write_cmos_sensor(0x3110,0x00);//,OTP_DATA12[7:0];
	T4K04_write_cmos_sensor(0x3111,0x00);//,OTP_DATA13[7:0];
	T4K04_write_cmos_sensor(0x3112,0x00);//,OTP_DATA14[7:0];
	T4K04_write_cmos_sensor(0x3113,0x00);//,OTP_DATA15[7:0];
	T4K04_write_cmos_sensor(0x3114,0x00);//,OTP_DATA16[7:0];
	T4K04_write_cmos_sensor(0x3115,0x00);//,OTP_DATA17[7:0];
	T4K04_write_cmos_sensor(0x3116,0x00);//,OTP_DATA18[7:0];
	T4K04_write_cmos_sensor(0x3117,0x00);//,OTP_DATA19[7:0];
	T4K04_write_cmos_sensor(0x3118,0x00);//,OTP_DATA20[7:0];
	T4K04_write_cmos_sensor(0x3119,0x00);//,OTP_DATA21[7:0];
	T4K04_write_cmos_sensor(0x311A,0x00);//,OTP_DATA22[7:0];
	T4K04_write_cmos_sensor(0x311B,0x00);//,OTP_DATA23[7:0];
	T4K04_write_cmos_sensor(0x311C,0x00);//,OTP_DATA24[7:0];
	T4K04_write_cmos_sensor(0x311D,0x00);//,OTP_DATA25[7:0];
	T4K04_write_cmos_sensor(0x311E,0x00);//,OTP_DATA26[7:0];
	T4K04_write_cmos_sensor(0x311F,0x00);//,OTP_DATA27[7:0];
	T4K04_write_cmos_sensor(0x3120,0x00);//,OTP_DATA28[7:0];
	T4K04_write_cmos_sensor(0x3121,0x00);//,OTP_DATA29[7:0];
	T4K04_write_cmos_sensor(0x3122,0x00);//,OTP_DATA30[7:0];
	T4K04_write_cmos_sensor(0x3123,0x00);//,OTP_DATA31[7:0];
	T4K04_write_cmos_sensor(0x3124,0x00);//,OTP_DATA32[7:0];
	T4K04_write_cmos_sensor(0x3125,0x00);//,OTP_DATA33[7:0];
	T4K04_write_cmos_sensor(0x3126,0x00);//,OTP_DATA34[7:0];
	T4K04_write_cmos_sensor(0x3127,0x00);//,OTP_DATA35[7:0];
	T4K04_write_cmos_sensor(0x3128,0x00);//,OTP_DATA36[7:0];
	T4K04_write_cmos_sensor(0x3129,0x00);//,OTP_DATA37[7:0];
	T4K04_write_cmos_sensor(0x312A,0x00);//,OTP_DATA38[7:0];
	T4K04_write_cmos_sensor(0x312B,0x00);//,OTP_DATA39[7:0];
	T4K04_write_cmos_sensor(0x312C,0x00);//,OTP_DATA40[7:0];
	T4K04_write_cmos_sensor(0x312D,0x00);//,OTP_DATA41[7:0];
	T4K04_write_cmos_sensor(0x312E,0x00);//,OTP_DATA42[7:0];
	T4K04_write_cmos_sensor(0x312F,0x00);//,OTP_DATA43[7:0];
	T4K04_write_cmos_sensor(0x3130,0x00);//,OTP_DATA44[7:0];
	T4K04_write_cmos_sensor(0x3131,0x00);//,OTP_DATA45[7:0];
	T4K04_write_cmos_sensor(0x3132,0x00);//,OTP_DATA46[7:0];
	T4K04_write_cmos_sensor(0x3133,0x00);//,OTP_DATA47[7:0];
	T4K04_write_cmos_sensor(0x3134,0x00);//,OTP_DATA48[7:0];
	T4K04_write_cmos_sensor(0x3135,0x00);//,OTP_DATA49[7:0];
	T4K04_write_cmos_sensor(0x3136,0x00);//,OTP_DATA50[7:0];
	T4K04_write_cmos_sensor(0x3137,0x00);//,OTP_DATA51[7:0];
	T4K04_write_cmos_sensor(0x3138,0x00);//,OTP_DATA52[7:0];
	T4K04_write_cmos_sensor(0x3139,0x00);//,OTP_DATA53[7:0];
	T4K04_write_cmos_sensor(0x313A,0x00);//,OTP_DATA54[7:0];
	T4K04_write_cmos_sensor(0x313B,0x00);//,OTP_DATA55[7:0];
	T4K04_write_cmos_sensor(0x313C,0x00);//,OTP_DATA56[7:0];
	T4K04_write_cmos_sensor(0x313D,0x00);//,OTP_DATA57[7:0];
	T4K04_write_cmos_sensor(0x313E,0x00);//,OTP_DATA58[7:0];
	T4K04_write_cmos_sensor(0x313F,0x00);//,OTP_DATA59[7:0];
	T4K04_write_cmos_sensor(0x3140,0x00);//,OTP_DATA60[7:0];
	T4K04_write_cmos_sensor(0x3141,0x00);//,OTP_DATA61[7:0];
	T4K04_write_cmos_sensor(0x3142,0x00);//,OTP_DATA62[7:0];
	T4K04_write_cmos_sensor(0x3143,0x00);//,OTP_DATA63[7:0];
	T4K04_write_cmos_sensor(0x3145,0x07);//,OTP_RWT/OTP_RNUM[1:0]/OTP_VERIFY/OTP_VMOD/OTP_PCLK[2:0];
	T4K04_write_cmos_sensor(0x3146,0x00);//,[RO] OTP_ISTS[2:0]/OTP_VE/OTP_TOE/OTP_VIR/-/-;
	T4K04_write_cmos_sensor(0x3147,0x00);//,OTP_TEST[3:0]/OTP_SPBE/OTP_TOEC/OTP_VEEC/OTP_STRC;
	T4K04_write_cmos_sensor(0x3148,0x00);//,[RO] OTP_VEE[7:0];
	T4K04_write_cmos_sensor(0x3150,0x06);//,OTP_LD_FEND/OTP_LD_RELD/-/-/-/OTP_LD_STS[1:0]/OTP_LD_ING;
	T4K04_write_cmos_sensor(0x3160,0x00);//,DFCT_TYP0[1:0]/-/DFCT_XADR0[12:8];
	T4K04_write_cmos_sensor(0x3161,0x00);//,DFCT_XADR0[7:0];
	T4K04_write_cmos_sensor(0x3162,0x00);//,-/-/-/-/DFCT_YADR0[11:8];
	T4K04_write_cmos_sensor(0x3163,0x00);//,DFCT_YADR0[7:0];
	T4K04_write_cmos_sensor(0x3164,0x00);//,DFCT_TYP1[1:0]/-/DFCT_XADR1[12:8];
	T4K04_write_cmos_sensor(0x3165,0x00);//,DFCT_XADR1[7:0];
	T4K04_write_cmos_sensor(0x3166,0x00);//,-/-/-/-/DFCT_YADR1[11:8];
	T4K04_write_cmos_sensor(0x3167,0x00);//,DFCT_YADR1[7:0];
	T4K04_write_cmos_sensor(0x3168,0x00);//,DFCT_TYP2[1:0]/-/DFCT_XADR2[12:8];
	T4K04_write_cmos_sensor(0x3169,0x00);//,DFCT_XADR2[7:0];
	T4K04_write_cmos_sensor(0x316A,0x00);//,-/-/-/-/DFCT_YADR2[11:8];
	T4K04_write_cmos_sensor(0x316B,0x00);//,DFCT_YADR2[7:0];
	T4K04_write_cmos_sensor(0x316C,0x00);//,DFCT_TYP3[1:0]/-/DFCT_XADR3[12:8];
	T4K04_write_cmos_sensor(0x316D,0x00);//,DFCT_XADR3[7:0];
	T4K04_write_cmos_sensor(0x316E,0x00);//,-/-/-/-/DFCT_YADR3[11:8];
	T4K04_write_cmos_sensor(0x316F,0x00);//,DFCT_YADR3[7:0];
	T4K04_write_cmos_sensor(0x3170,0x00);//,DFCT_TYP4[1:0]/-/DFCT_XADR4[12:8];
	T4K04_write_cmos_sensor(0x3171,0x00);//,DFCT_XADR4[7:0];
	T4K04_write_cmos_sensor(0x3172,0x00);//,-/-/-/-/DFCT_YADR4[11:8];
	T4K04_write_cmos_sensor(0x3173,0x00);//,DFCT_YADR4[7:0];
	T4K04_write_cmos_sensor(0x3174,0x00);//,DFCT_TYP5[1:0]/-/DFCT_XADR5[12:8];
	T4K04_write_cmos_sensor(0x3175,0x00);//,DFCT_XADR5[7:0];
	T4K04_write_cmos_sensor(0x3176,0x00);//,-/-/-/-/DFCT_YADR5[11:8];
	T4K04_write_cmos_sensor(0x3177,0x00);//,DFCT_YADR5[7:0];
	T4K04_write_cmos_sensor(0x3178,0x00);//,DFCT_TYP6[1:0]/-/DFCT_XADR6[12:8];
	T4K04_write_cmos_sensor(0x3179,0x00);//,DFCT_XADR6[7:0];
	T4K04_write_cmos_sensor(0x317A,0x00);//,-/-/-/-/DFCT_YADR6[11:8];
	T4K04_write_cmos_sensor(0x317B,0x00);//,DFCT_YADR6[7:0];
	T4K04_write_cmos_sensor(0x317C,0x00);//,DFCT_TYP7[1:0]/-/DFCT_XADR7[12:8];
	T4K04_write_cmos_sensor(0x317D,0x00);//,DFCT_XADR7[7:0];
	T4K04_write_cmos_sensor(0x317E,0x00);//,-/-/-/-/DFCT_YADR7[11:8];
	T4K04_write_cmos_sensor(0x317F,0x00);//,DFCT_YADR7[7:0];
	T4K04_write_cmos_sensor(0x3180,0x00);//,DFCT_TYP8[1:0]/-/DFCT_XADR8[12:8];
	T4K04_write_cmos_sensor(0x3181,0x00);//,DFCT_XADR8[7:0];
	T4K04_write_cmos_sensor(0x3182,0x00);//,-/-/-/-/DFCT_YADR8[11:8];
	T4K04_write_cmos_sensor(0x3183,0x00);//,DFCT_YADR8[7:0];
	T4K04_write_cmos_sensor(0x3184,0x00);//,DFCT_TYP9[1:0]/-/DFCT_XADR9[12:8];
	T4K04_write_cmos_sensor(0x3185,0x00);//,DFCT_XADR9[7:0];
	T4K04_write_cmos_sensor(0x3186,0x00);//,-/-/-/-/DFCT_YADR9[11:8];
	T4K04_write_cmos_sensor(0x3187,0x00);//,DFCT_YADR9[7:0];
	T4K04_write_cmos_sensor(0x3188,0x00);//,DFCT_TYP10[1:0]/-/DFCT_XADR10[12:8];
	T4K04_write_cmos_sensor(0x3189,0x00);//,DFCT_XADR10[7:0];
	T4K04_write_cmos_sensor(0x318A,0x00);//,-/-/-/-/DFCT_YADR10[11:8];
	T4K04_write_cmos_sensor(0x318B,0x00);//,DFCT_YADR10[7:0];
	T4K04_write_cmos_sensor(0x318C,0x00);//,DFCT_TYP11[1:0]/-/DFCT_XADR11[12:8];
	T4K04_write_cmos_sensor(0x318D,0x00);//,DFCT_XADR11[7:0];
	T4K04_write_cmos_sensor(0x318E,0x00);//,-/-/-/-/DFCT_YADR11[11:8];
	T4K04_write_cmos_sensor(0x318F,0x00);//,DFCT_YADR11[7:0];
	T4K04_write_cmos_sensor(0x3190,0x00);//,DFCT_TYP12[1:0]/-/DFCT_XADR12[12:8];
	T4K04_write_cmos_sensor(0x3191,0x00);//,DFCT_XADR12[7:0];
	T4K04_write_cmos_sensor(0x3192,0x00);//,-/-/-/-/DFCT_YADR12[11:8];
	T4K04_write_cmos_sensor(0x3193,0x00);//,DFCT_YADR12[7:0];
	T4K04_write_cmos_sensor(0x3194,0x00);//,DFCT_TYP13[1:0]/-/DFCT_XADR13[12:8];
	T4K04_write_cmos_sensor(0x3195,0x00);//,DFCT_XADR13[7:0];
	T4K04_write_cmos_sensor(0x3196,0x00);//,-/-/-/-/DFCT_YADR13[11:8];
	T4K04_write_cmos_sensor(0x3197,0x00);//,DFCT_YADR13[7:0];
	T4K04_write_cmos_sensor(0x3198,0x00);//,DFCT_TYP14[1:0]/-/DFCT_XADR14[12:8];
	T4K04_write_cmos_sensor(0x3199,0x00);//,DFCT_XADR14[7:0];
	T4K04_write_cmos_sensor(0x319A,0x00);//,-/-/-/-/DFCT_YADR14[11:8];
	T4K04_write_cmos_sensor(0x319B,0x00);//,DFCT_YADR14[7:0];
	T4K04_write_cmos_sensor(0x319C,0x00);//,DFCT_TYP15[1:0]/-/DFCT_XADR15[12:8];
	T4K04_write_cmos_sensor(0x319D,0x00);//,DFCT_XADR15[7:0];
	T4K04_write_cmos_sensor(0x319E,0x00);//,-/-/-/-/DFCT_YADR15[11:8];
	T4K04_write_cmos_sensor(0x319F,0x00);//,DFCT_YADR15[7:0];
	T4K04_write_cmos_sensor(0x3200,0x10);//,MKCP_MSK[1:0]/-/MSK_OFF/VCO_CONV[2:0]/-;
	T4K04_write_cmos_sensor(0x3202,0x04);//,ES_MARGIN[7:0];
	T4K04_write_cmos_sensor(0x3205,0x00);//,FLS_ESMODE/-/-/-/TEST_MTC_VGSW/TEST_MTC_RG_VG/-/-;
	T4K04_write_cmos_sensor(0x3206,0x05);//,-/KUMA_SW/KURI_SET/KURI_FCSET/ESYNC_SW/VSYNC_PH/HSYNC_PH/GLBRST_SE;
	T4K04_write_cmos_sensor(0x3207,0x00);//,SP_STEP1_DLYSEL[1:0]/SP_STEP1_DLYADJ[5:0];
	T4K04_write_cmos_sensor(0x3208,0x00);//,-/-/-/-/-/-/AF_REQ/GLBRST_REQ;
	T4K04_write_cmos_sensor(0x3209,0x00);//,-/-/-/-/-/-/-/LONG_EXP;
	T4K04_write_cmos_sensor(0x320A,0x00);//,-/-/-/AF_MODE/GRST_QUICK/LINE_START_SEL/GLBRST_MODE[1:0];
	T4K04_write_cmos_sensor(0x320B,0x60);//,GLB_SENTG_W[5:0]/-/GRST_RDY_W[8];
	T4K04_write_cmos_sensor(0x320C,0x33);//,GRST_RDY_W[7:0];
	T4K04_write_cmos_sensor(0x320D,0x20);//,DRVR_STB_S[15:8];
	T4K04_write_cmos_sensor(0x320E,0x00);//,DRVR_STB_S[7:0];
	T4K04_write_cmos_sensor(0x320F,0x08);//,FLSH_STB_S[15:8];
	T4K04_write_cmos_sensor(0x3210,0x00);//,FLSH_STB_S[7:0];
	T4K04_write_cmos_sensor(0x3211,0x20);//,EXPO_TIM_W[15:8];
	T4K04_write_cmos_sensor(0x3212,0x00);//,EXPO_TIM_W[7:0];
	T4K04_write_cmos_sensor(0x3213,0x00);//,-/-/-/-/DRVR_STB_W[11:8];
	T4K04_write_cmos_sensor(0x3214,0x32);//,DRVR_STB_W[7:0];
	T4K04_write_cmos_sensor(0x3215,0x30);//,FLSH_STB_W[15:8];
	T4K04_write_cmos_sensor(0x3216,0x00);//,FLSH_STB_W[7:0];
	T4K04_write_cmos_sensor(0x3217,0x00);//,AF_STB_W[15:8];
	T4K04_write_cmos_sensor(0x3218,0x05);//,AF_STB_W[7:0];
	T4K04_write_cmos_sensor(0x3219,0x00);//,-/-/AF_STB_N[5:0];
	T4K04_write_cmos_sensor(0x321A,0x00);//,-/-/-/-/FLSH_LINE[11:8];
	T4K04_write_cmos_sensor(0x321B,0x00);//,FLSH_LINE[7:0];
	T4K04_write_cmos_sensor(0x321C,0x00);//,MANU_MHZ_SPCK[7:0];
	T4K04_write_cmos_sensor(0x321D,0x00);//,MANU_MHZ/-/SPCKSW_VEN/SPCKSW_HEN/-/-/-/-;
	T4K04_write_cmos_sensor(0x321E,0x00);//,-/HC_PRESET[14:8];
	T4K04_write_cmos_sensor(0x321F,0x00);//,HC_PRESET[7:0];
	T4K04_write_cmos_sensor(0x3220,0x00);//,VC_PRESET[15:8];
	T4K04_write_cmos_sensor(0x3221,0x00);//,VC_PRESET[7:0];
	T4K04_write_cmos_sensor(0x3222,0x02);//,TPG_HRST_POS[7:0];
	T4K04_write_cmos_sensor(0x3223,0x04);//,TPG_VRST_POS[7:0];
	T4K04_write_cmos_sensor(0x3225,0x12);//,PP_VCNT_ADJ[7:0];
	T4K04_write_cmos_sensor(0x3226,0x54);//,PP_HCNT_ADJ[7:0];
	T4K04_write_cmos_sensor(0x3227,0x04);//,V_YUKO_START[7:0];
	T4K04_write_cmos_sensor(0x3228,0x07);//,H_YUKO_START[7:0];
	T4K04_write_cmos_sensor(0x3229,0x06);//,H_OB_START[7:0];
	T4K04_write_cmos_sensor(0x322A,0x02);//,PP_VBLK_START[7:0];
	T4K04_write_cmos_sensor(0x322B,0x00);//,PP_VBLK_WIDTH[7:0];
	T4K04_write_cmos_sensor(0x322C,0x1E);//,PP_HBLK_START[7:0];
	T4K04_write_cmos_sensor(0x322D,0x00);//,PP_HBLK_WIDTH[7:0];
	T4K04_write_cmos_sensor(0x322E,0x00);//,-/-/-/TEST_TG_VGSW/VOB_DISP/HOB_DISP/-/-;
	T4K04_write_cmos_sensor(0x322F,0x04);//,SBNRY_V_START[7:0];
	T4K04_write_cmos_sensor(0x3230,0x05);//,SBNRY_H_START[7:0];
	T4K04_write_cmos_sensor(0x3231,0x04);//,SBNRY_VBLK_START[7:0];
	T4K04_write_cmos_sensor(0x3232,0x00);//,SBNRY_VBLK_WIDTH[7:0];
	T4K04_write_cmos_sensor(0x3233,0x05);//,SBNRY_HBLK_START[7:0];
	T4K04_write_cmos_sensor(0x3234,0x00);//,SBNRY_HBLK_WIDTH[7:0];
	T4K04_write_cmos_sensor(0x3235,0x11);//,HLNR_HBLK_START[7:0];
	T4K04_write_cmos_sensor(0x3236,0x00);//,HLNR_HBLK_WIDTH[7:0];
	T4K04_write_cmos_sensor(0x3237,0x01);//,-/-/-/-/-/SP_COUNT[10:8];
	T4K04_write_cmos_sensor(0x3238,0x9E);//,SP_COUNT[7:0];
	T4K04_write_cmos_sensor(0x3239,0x00);//,AG_LIMSW/-/TEST_AGC_VGSW/TEST_SPG_VGSW/TEST_MAG/TEST_AGCONT/-/-;
	T4K04_write_cmos_sensor(0x323A,0x00);//,AG_MIN[7:0];
	T4K04_write_cmos_sensor(0x323B,0xFF);//,AG_MAX[7:0];
	T4K04_write_cmos_sensor(0x323C,0x00);//,-/-/-/-/-/HREG_HRST_POS[10:8];
	T4K04_write_cmos_sensor(0x323D,0x1C);//,HREG_HRST_POS[7:0];
	T4K04_write_cmos_sensor(0x323E,0x08);//,DIN_SW[3:0]/DOUT_ASW/-/-/-;
	T4K04_write_cmos_sensor(0x323F,0x08);//,TPG_TEST[3:0]/DOUT_BSW/-/-/-;
	T4K04_write_cmos_sensor(0x3240,0x00);//,TPG_NOISE_MP[7:0];
	T4K04_write_cmos_sensor(0x3241,0x80);//,TPG_OB_LV[7:0];
	T4K04_write_cmos_sensor(0x3242,0x30);//,RAMP_MODE/-/TPG_BLK_LVSEL[1:0]/-/-/TPBP_PIX[1:0];
	T4K04_write_cmos_sensor(0x3243,0x00);//,D_DANSA_SW/D_NOISE_SW/TPG_NOISE_SEL[1:0]/-/-/TPG_LIPOL/TPG_RLPOL;
	T4K04_write_cmos_sensor(0x3244,0x80);//,R_DANSA[7:0];
	T4K04_write_cmos_sensor(0x3245,0x80);//,GR_DANSA[7:0];
	T4K04_write_cmos_sensor(0x3246,0x80);//,GB_DANSA[7:0];
	T4K04_write_cmos_sensor(0x3247,0x80);//,B_DANSA[7:0];
	T4K04_write_cmos_sensor(0x3248,0x00);//,RDIN[7:0];
	T4K04_write_cmos_sensor(0x324A,0x00);//,CSR_TEST[3:0]/CSR_ABCHG/-/CSR_LIPOL/CSR_RLPOL;
	T4K04_write_cmos_sensor(0x324B,0x01);//,-/-/-/-/-/-/-/LNOBHLNR_SW;
	T4K04_write_cmos_sensor(0x3250,0x00);//,HLNR1INT_MPYSW/HLNR1INT_MPSEL[2:0]/-/-/HLNR1INT_MP[9:8];
	T4K04_write_cmos_sensor(0x3251,0x00);//,HLNR1INT_MP[7:0];
	T4K04_write_cmos_sensor(0x3254,0x00);//,HLNR3INT_MPYSW/HLNR3INT_MPSEL[2:0]/-/-/HLNR3INT_MP[9:8];
	T4K04_write_cmos_sensor(0x3255,0x00);//,HLNR3INT_MP[7:0];
	T4K04_write_cmos_sensor(0x3257,0x81);//,HLNR_SW/DANSA_SW/-/TEST_HLNR/HLNR_DISP/-/-/DANSA_RLPOL;
	T4K04_write_cmos_sensor(0x3258,0x40);//,AGMIN_LOB_REFLV[7:0];
	T4K04_write_cmos_sensor(0x3259,0x40);//,AGMAX_LOB_REFLV[7:0];
	T4K04_write_cmos_sensor(0x325A,0x20);//,AGMIN_LOB_WIDTH[7:0];
	T4K04_write_cmos_sensor(0x325B,0x20);//,AGMAX_LOB_WIDTH[7:0];
	T4K04_write_cmos_sensor(0x325C,0x40);//,MANU_BLACK[7:0];
	T4K04_write_cmos_sensor(0x325D,0x00);//,AGMIN_LOB_REFLV[8]/AGMAX_LOB_REFLV[8]/-/-/-/-/-/MANU_BLACK[8];
	T4K04_write_cmos_sensor(0x325E,0x90);//,AGMIN_DLBLACK_ADJ[7:0];
	T4K04_write_cmos_sensor(0x325F,0x16);//,AGMAX_DLBLACK_ADJ[7:0];
	T4K04_write_cmos_sensor(0x3260,0x20);//,HLNR_MANU_BLK[7:0];
	T4K04_write_cmos_sensor(0x3261,0x44);//,HLNR_OBCHG/OBDC_SW/-/-/-/OBDC_SEL/-/LOBINV_POL;
	T4K04_write_cmos_sensor(0x3262,0x06);//,AGMIN_OBDC_REFLV[8]/AGMAX_OBDC_REFLV[8]/-/-/-/OBDC_MIX[2:0];
	T4K04_write_cmos_sensor(0x3263,0x40);//,AGMIN_OBDC_REFLV[7:0];
	T4K04_write_cmos_sensor(0x3264,0x40);//,AGMAX_OBDC_REFLV[7:0];
	T4K04_write_cmos_sensor(0x3265,0x20);//,AGMIN_OBDC_WIDTH[7:0];
	T4K04_write_cmos_sensor(0x3266,0x20);//,AGMAX_OBDC_WIDTH[7:0];
	T4K04_write_cmos_sensor(0x3267,0x08);//,HLNRBLADJ_SW/HLNRBLADJ_CHG/HLNR_OBNC_SW/HLNRBLADJ_BLKSW/HLNR1INT_S;
	T4K04_write_cmos_sensor(0x3268,0xFF);//,AGMIN_HLNRBLADJ_R[8]/AGMAX_HLNRBLADJ_R[8]/AGMIN_HLNRBLADJ_GR[8]/AG;
	T4K04_write_cmos_sensor(0x3269,0x00);//,AGMIN_HLNRBLADJ_R[7:0];
	T4K04_write_cmos_sensor(0x326A,0x00);//,AGMAX_HLNRBLADJ_R[7:0];
	T4K04_write_cmos_sensor(0x326B,0x00);//,AGMIN_HLNRBLADJ_GR[7:0];
	T4K04_write_cmos_sensor(0x326C,0x00);//,AGMAX_HLNRBLADJ_GR[7:0];
	T4K04_write_cmos_sensor(0x326D,0x00);//,AGMIN_HLNRBLADJ_GB[7:0];
	T4K04_write_cmos_sensor(0x326E,0x00);//,AGMAX_HLNRBLADJ_GB[7:0];
	T4K04_write_cmos_sensor(0x326F,0x00);//,AGMIN_HLNRBLADJ_B[7:0];
	T4K04_write_cmos_sensor(0x3270,0x00);//,AGMAX_HLNRBLADJ_B[7:0];
	T4K04_write_cmos_sensor(0x3271,0x01);//,-/-/-/-/-/-/-/BLADJ_HOLD_WAIT;
	T4K04_write_cmos_sensor(0x3276,0x00);//,-/-/-/TEST_PWB/TEST_DGM/-/MPY_LIPOL/MPY_RLPOL;

	
	T4K04_write_cmos_sensor(0x3277,0x80);//,PWB_WLRG[7:0];//mtk revise ,donot need sensor LSC
	T4K04_write_cmos_sensor(0x3278,0x80);//,PWB_WLGRG[7:0];
	T4K04_write_cmos_sensor(0x3279,0x80);//,PWB_WLGBG[7:0];
	T4K04_write_cmos_sensor(0x327A,0x80);//,PWB_WLBG[7:0];

	
	T4K04_write_cmos_sensor(0x327B,0x83);//,MPX4_SEL/-/-/-/-/-/DATABIT_MP[9:8];
	T4K04_write_cmos_sensor(0x327C,0xCD);//,DATABIT_MP[7:0];
	T4K04_write_cmos_sensor(0x3280,0x00);//,LSSC_SW/-/-/TEST_LSSC/LSSC_DISP/-/LSSC_LIPOL/LSSC_CSPOL;
	T4K04_write_cmos_sensor(0x3281,0x44);//,LSSC_HCNT_ADJ[7:0];
	T4K04_write_cmos_sensor(0x3282,0x80);//,LSSC_HCNT_MPY[7:0];
	T4K04_write_cmos_sensor(0x3283,0x80);//,LSSC_HCEN_ADJ[7:0];
	T4K04_write_cmos_sensor(0x3284,0x10);//,LSSC_VCNT_ADJ[7:0];
	T4K04_write_cmos_sensor(0x3285,0x00);//,LSSC_VCNT_MPY[7:0];
	T4K04_write_cmos_sensor(0x3286,0x81);//,LSSC_VCNT_MPYSW/-/-/-/LSSC_VCNT_MPY[11:8];
	T4K04_write_cmos_sensor(0x3287,0x00);//,LSSC_VCEN_ADJ[7:0];
	T4K04_write_cmos_sensor(0x3288,0x02);//,LSSC_VCEN_WIDTH/-/-/-/-/-/LSSC_VCEN_ADJ[9:8];
	T4K04_write_cmos_sensor(0x3289,0x1C);//,LSSC_TOPL_PM1RG[7:0];
	T4K04_write_cmos_sensor(0x328A,0x0A);//,LSSC_TOPL_PM1GRG[7:0];
	T4K04_write_cmos_sensor(0x328B,0x0A);//,LSSC_TOPL_PM1GBG[7:0];
	T4K04_write_cmos_sensor(0x328C,0x07);//,LSSC_TOPL_PM1BG[7:0];
	T4K04_write_cmos_sensor(0x328D,0x16);//,LSSC_TOPR_PM1RG[7:0];
	T4K04_write_cmos_sensor(0x328E,0x11);//,LSSC_TOPR_PM1GRG[7:0];
	T4K04_write_cmos_sensor(0x328F,0x11);//,LSSC_TOPR_PM1GBG[7:0];
	T4K04_write_cmos_sensor(0x3290,0x14);//,LSSC_TOPR_PM1BG[7:0];
	T4K04_write_cmos_sensor(0x3291,0x28);//,LSSC_BOTL_PM1RG[7:0];
	T4K04_write_cmos_sensor(0x3292,0x10);//,LSSC_BOTL_PM1GRG[7:0];
	T4K04_write_cmos_sensor(0x3293,0x10);//,LSSC_BOTL_PM1GBG[7:0];
	T4K04_write_cmos_sensor(0x3294,0x16);//,LSSC_BOTL_PM1BG[7:0];
	T4K04_write_cmos_sensor(0x3295,0x16);//,LSSC_BOTR_PM1RG[7:0];
	T4K04_write_cmos_sensor(0x3296,0x14);//,LSSC_BOTR_PM1GRG[7:0];
	T4K04_write_cmos_sensor(0x3297,0x14);//,LSSC_BOTR_PM1GBG[7:0];
	T4K04_write_cmos_sensor(0x3298,0x10);//,LSSC_BOTR_PM1BG[7:0];
	T4K04_write_cmos_sensor(0x3299,0x0F);//,-/-/-/-/LSSC1BG_PMSW/LSSC1GBG_PMSW/LSSC1GRG_PMSW/LSSC1RG_PMSW;
	T4K04_write_cmos_sensor(0x329A,0xC8);//,LSSC_LEFT_P2RG[7:0];
	T4K04_write_cmos_sensor(0x329B,0x90);//,LSSC_LEFT_P2GRG[7:0];
	T4K04_write_cmos_sensor(0x329C,0x90);//,LSSC_LEFT_P2GBG[7:0];
	T4K04_write_cmos_sensor(0x329D,0x88);//,LSSC_LEFT_P2BG[7:0];
	T4K04_write_cmos_sensor(0x329E,0xCC);//,LSSC_RIGHT_P2RG[7:0];
	T4K04_write_cmos_sensor(0x329F,0x86);//,LSSC_RIGHT_P2GRG[7:0];
	T4K04_write_cmos_sensor(0x32A0,0x86);//,LSSC_RIGHT_P2GBG[7:0];
	T4K04_write_cmos_sensor(0x32A1,0x7E);//,LSSC_RIGHT_P2BG[7:0];
	T4K04_write_cmos_sensor(0x32A2,0xD6);//,LSSC_TOP_P2RG[7:0];
	T4K04_write_cmos_sensor(0x32A3,0x9A);//,LSSC_TOP_P2GRG[7:0];
	T4K04_write_cmos_sensor(0x32A4,0x9A);//,LSSC_TOP_P2GBG[7:0];
	T4K04_write_cmos_sensor(0x32A5,0x90);//,LSSC_TOP_P2BG[7:0];
	T4K04_write_cmos_sensor(0x32A6,0xBD);//,LSSC_BOTTOM_P2RG[7:0];
	T4K04_write_cmos_sensor(0x32A7,0x80);//,LSSC_BOTTOM_P2GRG[7:0];
	T4K04_write_cmos_sensor(0x32A8,0x80);//,LSSC_BOTTOM_P2GBG[7:0];
	T4K04_write_cmos_sensor(0x32A9,0x84);//,LSSC_BOTTOM_P2BG[7:0];
	T4K04_write_cmos_sensor(0x32AA,0x00);//,LSSC_LEFT_PM4RG[7:0];
	T4K04_write_cmos_sensor(0x32AB,0x00);//,LSSC_LEFT_PM4GRG[7:0];
	T4K04_write_cmos_sensor(0x32AC,0x00);//,LSSC_LEFT_PM4GBG[7:0];
	T4K04_write_cmos_sensor(0x32AD,0x00);//,LSSC_LEFT_PM4BG[7:0];
	T4K04_write_cmos_sensor(0x32AE,0x00);//,LSSC_RIGHT_PM4RG[7:0];
	T4K04_write_cmos_sensor(0x32AF,0x00);//,LSSC_RIGHT_PM4GRG[7:0];
	T4K04_write_cmos_sensor(0x32B0,0x00);//,LSSC_RIGHT_PM4GBG[7:0];
	T4K04_write_cmos_sensor(0x32B1,0x00);//,LSSC_RIGHT_PM4BG[7:0];
	T4K04_write_cmos_sensor(0x32B2,0x00);//,LSSC_TOP_PM4RG[7:0];
	T4K04_write_cmos_sensor(0x32B3,0x00);//,LSSC_TOP_PM4GRG[7:0];
	T4K04_write_cmos_sensor(0x32B4,0x00);//,LSSC_TOP_PM4GBG[7:0];
	T4K04_write_cmos_sensor(0x32B5,0x00);//,LSSC_TOP_PM4BG[7:0];
	T4K04_write_cmos_sensor(0x32B6,0x00);//,LSSC_BOTTOM_PM4RG[7:0];
	T4K04_write_cmos_sensor(0x32B7,0x00);//,LSSC_BOTTOM_PM4GRG[7:0];
	T4K04_write_cmos_sensor(0x32B8,0x00);//,LSSC_BOTTOM_PM4GBG[7:0];
	T4K04_write_cmos_sensor(0x32B9,0x00);//,LSSC_BOTTOM_PM4BG[7:0];
	T4K04_write_cmos_sensor(0x32BA,0x00);//,LSSC_MGSEL[1:0]/-/-/LSSC4BG_PMSW/LSSC4GBG_PMSW/LSSC4GRG_PMSW/LSSC4;
	T4K04_write_cmos_sensor(0x32BB,0x00);//,LSSC_BLACK[7:0];
	T4K04_write_cmos_sensor(0x32BC,0x01);//,-/-/-/-/-/-/-/LSSC_BLACK[8];
	T4K04_write_cmos_sensor(0x32BF,0x10);//,AGMAX_BBPC_SLV[7:0];
	T4K04_write_cmos_sensor(0x32C0,0x10);//,AGMAX_WBPC_SLV[7:0];
	T4K04_write_cmos_sensor(0x32C1,0x08);//,-/-/-/-/ABPC_SW/ABPC_MAP_EN/MAP_RLPOL/ABPC_THUR;
	T4K04_write_cmos_sensor(0x32C2,0x48);//,DRC_RAMADR_ADJ[7:0];
	T4K04_write_cmos_sensor(0x32C4,0x6C);//,-/WBPC_SW/BBPC_SW/TEST_ABPC/WBPC_MODE/BBPC_MODE/-/ABPC_DISP;
	T4K04_write_cmos_sensor(0x32C5,0x10);//,BBPC_SLV[7:0];
	T4K04_write_cmos_sensor(0x32C6,0x10);//,WBPC_SLV[7:0];
	T4K04_write_cmos_sensor(0x32C7,0x00);//,DFCT_VBIN/-/-/-/-/-/MAP_DSEL/ABPC_EDGE_SW;
	T4K04_write_cmos_sensor(0x32C8,0x08);//,AGMAX_BPCEDGE_MP[7:0];
	T4K04_write_cmos_sensor(0x32C9,0x08);//,AGMIN_BPCEDGE_MP[7:0];
	T4K04_write_cmos_sensor(0x32CA,0x94);//,DFCT_XADJ[3:0]/DFCT_YADJ[3:0];
	T4K04_write_cmos_sensor(0x32D3,0x20);//,ANR_SW/LPF_SEL/ANR_LIM/TEST_ANR/-/-/ANR_LIPOL/ANR_RLPOL;
	T4K04_write_cmos_sensor(0x32D4,0x20);//,AGMIN_ANRW_R[7:0];
	T4K04_write_cmos_sensor(0x32D5,0x20);//,AGMIN_ANRW_G[7:0];
	T4K04_write_cmos_sensor(0x32D6,0x30);//,AGMIN_ANRW_B[7:0];
	T4K04_write_cmos_sensor(0x32D7,0x40);//,AGMAX_ANRW_R[7:0];
	T4K04_write_cmos_sensor(0x32D8,0x40);//,AGMAX_ANRW_G[7:0];
	T4K04_write_cmos_sensor(0x32D9,0x4F);//,AGMAX_ANRW_B[7:0];
	T4K04_write_cmos_sensor(0x32DA,0x80);//,AGMIN_ANRMP_R[7:0];
	T4K04_write_cmos_sensor(0x32DB,0x80);//,AGMIN_ANRMP_G[7:0];
	T4K04_write_cmos_sensor(0x32DC,0xC0);//,AGMIN_ANRMP_B[7:0];
	T4K04_write_cmos_sensor(0x32DD,0xF0);//,AGMAX_ANRMP_R[7:0];
	T4K04_write_cmos_sensor(0x32DE,0xF0);//,AGMAX_ANRMP_G[7:0];
	T4K04_write_cmos_sensor(0x32DF,0xFF);//,AGMAX_ANRMP_B[7:0];
	T4K04_write_cmos_sensor(0x32E2,0x2C);//,-/LOB1INT_PIX[6:0];
	T4K04_write_cmos_sensor(0x32E4,0x2C);//,-/LOB3INT_PIX[6:0];
	T4K04_write_cmos_sensor(0x32E5,0x30);//,-/H_LOB_WIDTH[6:0];
	T4K04_write_cmos_sensor(0x32E6,0x02);//,-/HLNR_LOB1CP_S[6:0];
	T4K04_write_cmos_sensor(0x32E7,0x02);//,-/HLNR_LOB2CP_S[6:0];
	T4K04_write_cmos_sensor(0x32E8,0x02);//,-/HLNR_LOB3CP_S[6:0];
	T4K04_write_cmos_sensor(0x32E9,0x07);//,PP_LOB_START[7:0];
	T4K04_write_cmos_sensor(0x32EC,0x25);//,NZ_REG[7:0];
	T4K04_write_cmos_sensor(0x3300,0x1C);//,-/-/-/BOOSTEN/POSLFIX/NEGLFIX/-/NEGLEAKCUT;
	T4K04_write_cmos_sensor(0x3301,0x06);//,BSTREADEV/-/-/-/NEGBSTCNT[3:0];
	T4K04_write_cmos_sensor(0x3302,0x04);//,POSBSTSEL/-/-/-/-/POSBSTCNT[2:0];
	T4K04_write_cmos_sensor(0x3303,0x35);//,-/POSBSTHG[2:0]/-/POSBSTGA[2:0];
	T4K04_write_cmos_sensor(0x3304,0x00);//,VDSEL[1:0]/LNOBMODE[1:0]/-/READVDSEL/-/GDMOSBGREN;
	T4K04_write_cmos_sensor(0x3305,0x80);//,KBIASSEL/-/-/-/-/-/-/-;
	T4K04_write_cmos_sensor(0x3306,0x00);//,-/-/-/BSVBPSEL[4:0];
	T4K04_write_cmos_sensor(0x3307,0x24);//,-/-/RSTVDSEL_AL0/RSTVDSEL_NML/DRADRVI_AL0[1:0]/DRADRVI_NML[1:0];
	T4K04_write_cmos_sensor(0x3308,0x88);//,DRADRVPU[1:0]/-/VREFV[4:0];
	T4K04_write_cmos_sensor(0x3309,0xCC);//,ADSW2WEAK/ADSW1WEAK/-/-/VREFAI[3:0];
	T4K04_write_cmos_sensor(0x330A,0x24);//,ADCKSEL/-/ADCKDIV[1:0]/-/SENSEMODE[2:0];
	T4K04_write_cmos_sensor(0x330B,0x00);//,-/-/SPARE[1:0]/ANAMON1_SEL[3:0];
	T4K04_write_cmos_sensor(0x330C,0x07);//,HREG_TEST/-/TESTCROP/-/-/BINVSIG/BINED/BINCMP;
	T4K04_write_cmos_sensor(0x330E,0x10);//,EXT_HCNT_MAX_ON/-/-/HCNT_MAX_MODE/-/-/-/MLT_SPL_MODE;
	T4K04_write_cmos_sensor(0x330F,0x0B);//,HCNT_MAX_FIXVAL[15:8];
	T4K04_write_cmos_sensor(0x3310,0x54);//,HCNT_MAX_FIXVAL[7:0];
	T4K04_write_cmos_sensor(0x3312,0x04);//,-/-/VREG_TEST[1:0]/ES_MODE/BIN_MODE/DIS_MODE/RODATA_U;
	T4K04_write_cmos_sensor(0x3313,0x00);//,-/-/-/-/-/-/-/DMRC_MODE;
	T4K04_write_cmos_sensor(0x3314,0x5C);//,BSC_OFF/LIMITTER_BSC/VSIG_BSC/DRESET_CONJ_U[4:0];
	T4K04_write_cmos_sensor(0x3315,0x05);//,-/-/-/DRESET_HIGH/DRESET_CONJ_D[3:0];
	T4K04_write_cmos_sensor(0x3316,0x04);//,FTLSNS_HIGH/-/FTLSNS_LBSC_U[5:0];
	T4K04_write_cmos_sensor(0x3317,0x00);//,-/-/-/-/-/FTLSNS_LBSC_D[2:0];
	T4K04_write_cmos_sensor(0x3318,0x18);//,-/FTLSNS_CONJ_W[6:0];
	T4K04_write_cmos_sensor(0x3319,0x05);//,-/-/-/-/FTLSNS_CONJ_D[3:0];
	T4K04_write_cmos_sensor(0x331A,0x08);//,SADR_HIGH/-/SADR_LBSC_U[5:0];
	T4K04_write_cmos_sensor(0x331B,0x00);//,-/-/-/-/-/SADR_LBSC_D[2:0];
	T4K04_write_cmos_sensor(0x331C,0x4E);//,SADR_CONJ_U[7:0];
	T4K04_write_cmos_sensor(0x331D,0x00);//,-/-/-/-/SADR_CONJ_D[3:0];
	T4K04_write_cmos_sensor(0x331F,0x83);//,ESREAD_ALT_OFF/-/-/ELEC_INJ_MODE/-/-/AUTO_READ_W/AUTO_ESREAD_2D;
	T4K04_write_cmos_sensor(0x3320,0x1A);//,-/-/-/DRESET_VSIG_U[4:0];
	T4K04_write_cmos_sensor(0x3321,0x04);//,-/-/-/-/DRESET_VSIG_D[3:0];
	T4K04_write_cmos_sensor(0x3322,0x56);//,ESREAD_1D[7:0];
	T4K04_write_cmos_sensor(0x3323,0x7B);//,ESREAD_1W[7:0];
	T4K04_write_cmos_sensor(0x3324,0x01);//,-/-/-/-/-/ESREAD_2D[10:8];
	T4K04_write_cmos_sensor(0x3325,0x4D);//,ESREAD_2D[7:0];
	T4K04_write_cmos_sensor(0x3326,0xA6);//,ESREAD_2W[7:0];
	T4K04_write_cmos_sensor(0x3327,0x14);//,ESTGRESET_LOW/ESTGRESET_D[6:0];
	T4K04_write_cmos_sensor(0x3328,0x40);//,ALLZEROSET_ON/ZEROSET_1ST/ALLZEROSET_CHG_ON/EXTD_ROTGRESET/-/-/ROT;
	T4K04_write_cmos_sensor(0x3329,0xC6);//,ROTGRESET_U[7:0];
	T4K04_write_cmos_sensor(0x332A,0x1E);//,-/-/ROTGRESET_W[5:0];
	T4K04_write_cmos_sensor(0x332B,0x00);//,ROREAD_U[7:0];
	T4K04_write_cmos_sensor(0x332C,0xA6);//,ROREAD_W[7:0];
	T4K04_write_cmos_sensor(0x332D,0x16);//,ZEROSET_U[7:0];
	T4K04_write_cmos_sensor(0x332E,0x58);//,ZEROSET_W[7:0];
	T4K04_write_cmos_sensor(0x332F,0x00);//,-/-/FIX_RSTDRAIN[1:0]/FIX_RSTDRAIN2[1:0]/FIX_RSTDRAIN3[1:0];
	T4K04_write_cmos_sensor(0x3330,0x00);//,-/RSTDRAIN_D[6:0];
	T4K04_write_cmos_sensor(0x3331,0x0C);//,-/-/RSTDRAIN_U[5:0];
	T4K04_write_cmos_sensor(0x3332,0x05);//,-/-/-/-/RSTDRAIN2_D[3:0];
	T4K04_write_cmos_sensor(0x3333,0x05);//,-/-/-/-/RSTDRAIN2_U[3:0];
	T4K04_write_cmos_sensor(0x3334,0x00);//,-/-/-/-/RSTDRAIN3_D[3:0];
	T4K04_write_cmos_sensor(0x3335,0x14);//,-/-/RSTDRAIN3_U[5:0];
	T4K04_write_cmos_sensor(0x3336,0x03);//,-/-/DRCUT_SIGIN/DRCUT_HIGH/-/-/VSIGDR_MODE[1:0];
	T4K04_write_cmos_sensor(0x3337,0x01);//,-/-/DRCUT_NML_U[5:0];
	T4K04_write_cmos_sensor(0x3338,0x20);//,-/-/DRCUT_CGR_U[5:0];
	T4K04_write_cmos_sensor(0x3339,0x20);//,-/-/DRCUT_CGR_D[5:0];
	T4K04_write_cmos_sensor(0x333A,0x30);//,-/-/DRCUT_VDER_1U[5:0];
	T4K04_write_cmos_sensor(0x333B,0x04);//,-/-/DRCUT_VDER_1D[5:0];
	T4K04_write_cmos_sensor(0x333C,0x30);//,-/-/DRCUT_VDER_2U[5:0];
	T4K04_write_cmos_sensor(0x333D,0x04);//,-/-/DRCUT_VDER_2D[5:0];
	T4K04_write_cmos_sensor(0x333E,0x00);//,-/-/DRCUT_1ITV_MIN[1:0]/-/-/DRCUT_2ITV_MIN[1:0];
	T4K04_write_cmos_sensor(0x333F,0x3A);//,GDMOSCNT_NML[3:0]/GDMOSCNT_VDER[3:0];
	T4K04_write_cmos_sensor(0x3340,0x11);//,GDMOSCNT_CGR[3:0]/-/-/GDMOS_VDER_1U[1:0];
	T4K04_write_cmos_sensor(0x3341,0x04);//,-/-/GDMOS_VDER_1D[5:0];
	T4K04_write_cmos_sensor(0x3342,0x11);//,-/-/GDMOS2CNT[1:0]/-/-/GDMOS_VDER_2U[1:0];
	T4K04_write_cmos_sensor(0x3343,0x04);//,-/-/GDMOS_VDER_2D[5:0];
	T4K04_write_cmos_sensor(0x3344,0x10);//,-/-/-/RO_DRCUT_OFF/-/-/-/SIGIN_ON;
	T4K04_write_cmos_sensor(0x3345,0x05);//,GDMOS_CGR_D[5:0]/GDMOSLT_VDER_1W[1:0];
	T4K04_write_cmos_sensor(0x3346,0x61);//,-/GDMOSLT_VDER_1D[6:0];
	T4K04_write_cmos_sensor(0x3347,0x01);//,-/-/-/-/-/-/GDMOSLT_VDER_2W[1:0];
	T4K04_write_cmos_sensor(0x3348,0x60);//,-/GDMOSLT_VDER_2D[6:0];
	T4K04_write_cmos_sensor(0x334C,0x18);//,ADSW1_D[7:0];
	T4K04_write_cmos_sensor(0x334D,0x00);//,ADSW1_HIGH/-/ADSW_U[5:0];
	T4K04_write_cmos_sensor(0x334E,0x0C);//,ADSW1DMX_LOW/-/-/ADSW1DMX_U[4:0];
	T4K04_write_cmos_sensor(0x334F,0x30);//,ADSW1LK_HIGH/ADSW1LK_D[6:0];
	T4K04_write_cmos_sensor(0x3350,0x30);//,ADSW1LKX_LOW/ADSW1LKX_U[6:0];
	T4K04_write_cmos_sensor(0x3351,0x00);//,ADCMP1SRT_LOW/-/-/-/-/-/-/-;
	T4K04_write_cmos_sensor(0x3353,0x18);//,ADSW2_HIGH/-/ADSW2_D[5:0];
	T4K04_write_cmos_sensor(0x3354,0x0C);//,ADSW2DMX_LOW/-/-/ADSW2DMX_U[4:0];
	T4K04_write_cmos_sensor(0x3355,0x80);//,FIX_ADENX[1:0]/ADENX_U[5:0];
	T4K04_write_cmos_sensor(0x3356,0x01);//,-/-/ADENX_D[5:0];
	T4K04_write_cmos_sensor(0x3357,0x00);//,-/-/CMPI_CGR_U[5:0];
	T4K04_write_cmos_sensor(0x3358,0x01);//,-/-/CMPI_CGR_D[5:0];
	T4K04_write_cmos_sensor(0x3359,0x83);//,CMPI1_NML[3:0]/CMPI2_NML[3:0];
	T4K04_write_cmos_sensor(0x335A,0x33);//,CMPI1_CGR[3:0]/CMPI2_CGR[3:0];
	T4K04_write_cmos_sensor(0x335B,0x03);//,-/-/-/-/-/-/CGR_MODE/CDS_STOPBST;
	T4K04_write_cmos_sensor(0x335C,0x10);//,BSTCKLFIX_HIGH/BSTCKLFIX_CMP_U[6:0];
	T4K04_write_cmos_sensor(0x335D,0x14);//,-/BSTCKLFIX_CMP_D[6:0];
	T4K04_write_cmos_sensor(0x335E,0x89);//,CDS_ADC_BSTOFF/-/-/BSTCKLFIX_ADC_U[4:0];
	T4K04_write_cmos_sensor(0x335F,0x01);//,-/-/-/BSTCKLFIX_ADC_D[4:0];
	T4K04_write_cmos_sensor(0x3360,0x11);//,VSIGLMTCNT_RNG3[3:0]/VSIGLMTCNT_RNG2[3:0];
	T4K04_write_cmos_sensor(0x3361,0x17);//,VSIGLMTCNT_RNG1[3:0]/VSIGLMTCNT_RNG0[3:0];
	T4K04_write_cmos_sensor(0x3362,0x00);//,VSIGLMTEN_U0[7:0];
	T4K04_write_cmos_sensor(0x3363,0x00);//,-/FIX_VSIGLMTEN[2:0]/VSIGLMTEN_D[3:0];
	T4K04_write_cmos_sensor(0x3364,0x00);//,VSIGLMTEN_U1[7:0];
	T4K04_write_cmos_sensor(0x3366,0x03);//,-/-/-/-/SINT_ZS_U[3:0];
	T4K04_write_cmos_sensor(0x3367,0x4F);//,SINT_ZS_W[7:0];
	T4K04_write_cmos_sensor(0x3368,0x3B);//,-/SINT_RS_U[6:0];
	T4K04_write_cmos_sensor(0x3369,0x77);//,SINT_RS_W[7:0];
	T4K04_write_cmos_sensor(0x336A,0x1D);//,SINT_FB_U[7:0];
	T4K04_write_cmos_sensor(0x336B,0x4F);//,-/SINT_FB_W[6:0];
	T4K04_write_cmos_sensor(0x336C,0x01);//,-/-/-/-/-/-/SINT_AD_U[9:8];
	T4K04_write_cmos_sensor(0x336D,0x17);//,SINT_AD_U[7:0];
	T4K04_write_cmos_sensor(0x336E,0x01);//,-/-/-/-/-/-/-/SINT_AD_W[8];
	T4K04_write_cmos_sensor(0x336F,0x77);//,SINT_AD_W[7:0];
	T4K04_write_cmos_sensor(0x3370,0x61);//,-/SINTLSEL2/SINTLSEL/SINTSELPH2/SINTSELPH1/SINTSELFB[2:0];
	T4K04_write_cmos_sensor(0x3371,0x20);//,SINTSELOUT2[3:0]/SINTSELOUT1[3:0];
	T4K04_write_cmos_sensor(0x3372,0x33);//,DRADRVLV_AL0_RNG3[3:0]/DRADRVLV_AL0_RNG2[3:0];
	T4K04_write_cmos_sensor(0x3373,0x33);//,DRADRVLV_AL0_RNG1[3:0]/DRADRVLV_AL0_RNG0[3:0];
	T4K04_write_cmos_sensor(0x3374,0x11);//,DRADRVLV_NML_RNG3[3:0]/DRADRVLV_NML_RNG2[3:0];
	T4K04_write_cmos_sensor(0x3375,0x11);//,DRADRVLV_NML_RNG1[3:0]/DRADRVLV_NML_RNG0[3:0];
	T4K04_write_cmos_sensor(0x3376,0x00);//,-/-/-/-/GDMOS2ENX_MODE[3:0];
	T4K04_write_cmos_sensor(0x3380,0x11);//,-/-/SINTX_DSHIFT[1:0]/-/-/SINTX_USHIFT[1:0];
	T4K04_write_cmos_sensor(0x3381,0x10);//,-/-/VCD_RNG_TYPE_SEL[1:0]/-/-/VREFIC_MODE[1:0];
	T4K04_write_cmos_sensor(0x3382,0x00);//,EXT_VREFIC_ON/-/-/-/-/VREFIC_FIXVAL[2:0];
	T4K04_write_cmos_sensor(0x3383,0x00);//,-/FIX_VREFICAID[2:0]/-/-/VREFICAID_OFF[1:0];
	T4K04_write_cmos_sensor(0x3384,0x00);//,-/-/-/-/-/-/VREFICAID_W[9:8];
	T4K04_write_cmos_sensor(0x3385,0x64);//,VREFICAID_W[7:0];
	T4K04_write_cmos_sensor(0x3386,0x00);//,-/-/-/VREFALN/-/-/-/VREFIMBC;
	T4K04_write_cmos_sensor(0x3387,0x00);//,-/PS_VFB_GBL_VAL[6:0];
	T4K04_write_cmos_sensor(0x3388,0x00);//,-/PS_VFB_10B_RNG1[6:0];
	T4K04_write_cmos_sensor(0x3389,0x00);//,-/PS_VFB_10B_RNG2[6:0];
	T4K04_write_cmos_sensor(0x338A,0x00);//,-/PS_VFB_10B_RNG3[6:0];
	T4K04_write_cmos_sensor(0x338B,0x00);//,-/PS_VFB_10B_RNG4[6:0];
	T4K04_write_cmos_sensor(0x338C,0x01);//,-/-/-/-/VFB_STEP_GBL[3:0];
	T4K04_write_cmos_sensor(0x338D,0x01);//,-/-/-/-/VFB_STEP_RNG1[3:0];
	T4K04_write_cmos_sensor(0x338E,0x01);//,-/-/-/-/VFB_STEP_RNG2[3:0];
	T4K04_write_cmos_sensor(0x338F,0x01);//,-/-/-/-/VFB_STEP_RNG3[3:0];
	T4K04_write_cmos_sensor(0x3390,0x01);//,-/-/-/-/VFB_STEP_RNG4[3:0];
	T4K04_write_cmos_sensor(0x3391,0xC6);//,SRST_RS_U[7:0];
	T4K04_write_cmos_sensor(0x3392,0x0D);//,SRST_RS_U[8]/-/SRST_RS_W[5:0];
	T4K04_write_cmos_sensor(0x3393,0xB5);//,SRST_ZS_U[7:0];
	T4K04_write_cmos_sensor(0x3394,0x0D);//,-/-/SRST_ZS_W[5:0];
	T4K04_write_cmos_sensor(0x3395,0x03);//,-/-/-/-/SRST_AD_U[3:0];
	T4K04_write_cmos_sensor(0x3396,0xA5);//,SRST_AD_D[7:0];
	T4K04_write_cmos_sensor(0x339F,0x00);//,VREF12ADIP/-/-/-/-/-/-/-;
	T4K04_write_cmos_sensor(0x33A0,0x03);//,VREFSHBGR_LOW/-/-/-/VREFSHBGR_D[3:0];
	T4K04_write_cmos_sensor(0x33A1,0x38);//,-/-/VREFSHBGR_U[5:0];
	T4K04_write_cmos_sensor(0x33A2,0xBC);//,EN_VREFC_ZERO/-/VREF_H_START_U[5:0];
	T4K04_write_cmos_sensor(0x33A3,0x00);//,ADCKEN_MASK[1:0]/-/-/-/-/-/-;
	T4K04_write_cmos_sensor(0x33A4,0x0B);//,-/ADCKEN_1U[6:0];
	T4K04_write_cmos_sensor(0x33A5,0x0F);//,-/-/-/ADCKEN_1D[4:0];
	T4K04_write_cmos_sensor(0x33A6,0x0B);//,-/ADCKEN_2U[6:0];
	T4K04_write_cmos_sensor(0x33A7,0x0F);//,-/-/-/ADCKEN_2D[4:0];
	T4K04_write_cmos_sensor(0x33A8,0x09);//,-/-/-/-/CNTRSTX_U[3:0];
	T4K04_write_cmos_sensor(0x33A9,0x0F);//,-/-/-/CNT0RSTX_1D[4:0];
	T4K04_write_cmos_sensor(0x33AA,0x09);//,-/-/-/-/CNT0RSTX_2U[3:0];
	T4K04_write_cmos_sensor(0x33AB,0x0F);//,-/-/-/CNT0RSTX_2D[4:0];
	T4K04_write_cmos_sensor(0x33AC,0x08);//,-/-/-/CNTINVX_START[4:0];
	T4K04_write_cmos_sensor(0x33AD,0x14);//,EDCONX_1D[7:0];
	T4K04_write_cmos_sensor(0x33AE,0x00);//,EDCONX_RS_HIGH/EDCONX_AD_HIGH/-/-/-/-/-/EDCONX_2D[8];
	T4K04_write_cmos_sensor(0x33AF,0x28);//,EDCONX_2D[7:0];
	T4K04_write_cmos_sensor(0x33B0,0x00);//,ADTESTCK_INTVL[3:0]/-/-/ADTESTCK_ON/COUNTER_TEST;
	T4K04_write_cmos_sensor(0x33B2,0x00);//,-/-/-/-/-/-/-/ROUND_VREF_CODE;
	T4K04_write_cmos_sensor(0x33B3,0x01);//,EXT_VCD_ADJ_ON/-/-/AG_SEN_SHIFT/-/-/VCD_COEF_FIXVAL[9:8];
	T4K04_write_cmos_sensor(0x33B4,0x00);//,VCD_COEF_FIXVAL[7:0];
	T4K04_write_cmos_sensor(0x33B5,0x00);//,-/-/VCD_INTC_FIXVAL[5:0];
	T4K04_write_cmos_sensor(0x33B6,0x1B);//,VREFAD_RNG1_SEL[1:0]/VREFAD_RNG2_SEL[1:0]/VREFAD_RNG3_SEL[1:0]/VRE;
	T4K04_write_cmos_sensor(0x33B7,0x00);//,-/-/-/-/-/-/AGADJ1_VREFI_ZS[9:8];
	T4K04_write_cmos_sensor(0x33B8,0x3C);//,AGADJ1_VREFI_ZS[7:0];
	T4K04_write_cmos_sensor(0x33B9,0x00);//,-/-/-/-/-/-/AGADJ2_VREFI_ZS[9:8];
	T4K04_write_cmos_sensor(0x33BA,0x1E);//,AGADJ2_VREFI_ZS[7:0];
	T4K04_write_cmos_sensor(0x33BB,0x00);//,-/-/-/-/-/-/-/AGADJ1_VREFI_AD[8];
	T4K04_write_cmos_sensor(0x33BC,0x3C);//,AGADJ1_VREFI_AD[7:0];
	T4K04_write_cmos_sensor(0x33BD,0x00);//,-/-/-/-/-/-/-/AGADJ2_VREFI_AD[8];
	T4K04_write_cmos_sensor(0x33BE,0x1E);//,AGADJ2_VREFI_AD[7:0];
	T4K04_write_cmos_sensor(0x33BF,0x70);//,-/AGADJ_VREFIC[2:0]/-/AGADJ_VREFC[2:0];
	T4K04_write_cmos_sensor(0x33C0,0x00);//,EXT_VREFI_ZS_ON/-/-/-/-/-/VREFI_ZS_FIXVAL[9:8];
	T4K04_write_cmos_sensor(0x33C1,0x00);//,VREFI_ZS_FIXVAL[7:0];
	T4K04_write_cmos_sensor(0x33C2,0x00);//,EXT_VREFI_FB_ON/-/-/-/-/-/-/-;
	T4K04_write_cmos_sensor(0x33C3,0x00);//,-/-/VREFI_FB_FIXVAL[5:0];
	T4K04_write_cmos_sensor(0x33C6,0x00);//,EXT_VREFC_ON/-/-/-/-/VREFC_FIXVAL[2:0];
	T4K04_write_cmos_sensor(0x33C7,0x00);//,EXT_PLLFREQ_ON/-/-/-/PLLFREQ_FIXVAL[3:0];
	T4K04_write_cmos_sensor(0x33C9,0x01);//,-/-/-/-/-/-/-/BCDCNT_ST6;
	T4K04_write_cmos_sensor(0x33CA,0x00);//,-/-/-/ACT_TESTDAC/-/-/-/TESTDACEN;
	T4K04_write_cmos_sensor(0x33CB,0x80);//,TDAC_INT[7:0];
	T4K04_write_cmos_sensor(0x33CC,0x00);//,TDAC_MIN[7:0];
	T4K04_write_cmos_sensor(0x33CD,0x10);//,TDAC_STEP[3:0]/-/-/TDAC_SWD[1:0];
	T4K04_write_cmos_sensor(0x33CE,0x00);//,-/-/-/-/-/-/-/AG_TEST;
	T4K04_write_cmos_sensor(0x33CF,0x00);//,DACS_INT[7:0];
	T4K04_write_cmos_sensor(0x33D0,0xFF);//,DACS_MAX[7:0];
	T4K04_write_cmos_sensor(0x33D1,0x10);//,DACS_STEP[3:0]/-/-/DACS_SWD[1:0];
	T4K04_write_cmos_sensor(0x33D2,0x80);//,TESTDAC_RSVOL[7:0];
	T4K04_write_cmos_sensor(0x33D3,0x60);//,TESTDAC_ADVOL[7:0];
	T4K04_write_cmos_sensor(0x33D4,0x62);//,ZSV_EXEC_MODE[3:0]/-/AGADJ_EXEC_MODE[2:0];
	T4K04_write_cmos_sensor(0x33D5,0x02);//,-/AGADJ_CALC_MODE/-/-/AGADJ_FIX_COEF[11:8];
	T4K04_write_cmos_sensor(0x33D6,0x06);//,AGADJ_FIX_COEF[7:0];
	T4K04_write_cmos_sensor(0x33D7,0xF1);//,ZSV_FORCE_END[3:0]/-/-/ZSV_SUSP_RANGE[1:0];
	T4K04_write_cmos_sensor(0x33D8,0x86);//,ZSV_SUSP_CND/-/-/-/EN_PS_VREFI_ZS[3:0];
	T4K04_write_cmos_sensor(0x33DA,0xA0);//,ZSV_LEVEL[7:0];
	T4K04_write_cmos_sensor(0x33DB,0x10);//,-/-/ZSV_IN_RANGE[5:0];
	T4K04_write_cmos_sensor(0x33DC,0xC7);//,PS_VZS_NML_COEF[7:0];
	T4K04_write_cmos_sensor(0x33DD,0x00);//,-/PS_VZS_NML_INTC[6:0];
	T4K04_write_cmos_sensor(0x33DE,0x10);//,VZS_NML_STEP[7:0];
	T4K04_write_cmos_sensor(0x33DF,0x42);//,ZSV_STOP_CND[1:0]/-/-/ZSV_IN_LINES[3:0];
	T4K04_write_cmos_sensor(0x33E2,0x10);//,-/FBC_IN_RANGE[6:0];
	T4K04_write_cmos_sensor(0x33E3,0xC4);//,FBC_SUSP_RANGE[1:0]/-/FBC_IN_LINES[4:0];
	T4K04_write_cmos_sensor(0x33E4,0x5F);//,FBC_OUT_RANGE[1:0]/-/FBC_OUT_LINES[4:0];
	T4K04_write_cmos_sensor(0x33E5,0x21);//,FBC_STOP_CND[2:0]/-/PS_VREFI_FB[3:0];
	T4K04_write_cmos_sensor(0x33E6,0x21);//,FBC_START_CND[3:0]/-/-/-/-;
	T4K04_write_cmos_sensor(0x33E7,0x86);//,FBC_SUSP_CND/-/-/EN_PS_VREFI_FB[4:0];
	T4K04_write_cmos_sensor(0x33E8,0x00);//,-/-/-/-/-/-/-/ST_CLIP_DATA;
	T4K04_write_cmos_sensor(0x33E9,0x00);//,ST_BLACK_LEVEL[9:8]/ST_CKI[5:0];
	T4K04_write_cmos_sensor(0x33EA,0x40);//,ST_BLACK_LEVEL[7:0];
	T4K04_write_cmos_sensor(0x33EB,0xF0);//,ST_RSVD_REG[7:0];
	T4K04_write_cmos_sensor(0x33EC,0x00);//,[RO] -/-/-/-/ob_ave[11:8];
	T4K04_write_cmos_sensor(0x33ED,0x00);//,[RO] ob_ave[7:0];
	T4K04_write_cmos_sensor(0x33EE,0x11);//,[RO] -/-/-/gdmos2enx/-/-/agadj_coef[9:8];
	T4K04_write_cmos_sensor(0x33EF,0x00);//,[RO] agadj_coef[7:0];
	T4K04_write_cmos_sensor(0x33F0,0x00);//,[RO] -/-/-/-/-/-/vrefi_zs[9:8];
	T4K04_write_cmos_sensor(0x33F1,0x00);//,[RO] vrefi_zs[7:0];
	T4K04_write_cmos_sensor(0x33F2,0x00);//,[RO] -/-/-/-/-/-/vrefi_fb[9:8];
	T4K04_write_cmos_sensor(0x33F3,0x00);//,[RO] vrefi_fb[7:0];
	T4K04_write_cmos_sensor(0x33F4,0x00);//,[RO] -/-/-/zsv_operation/-/-/-/fbc_operation;
	T4K04_write_cmos_sensor(0x33F5,0x00);//,[RO] -/-/-/-/-/-/vref_code[9:8];
	T4K04_write_cmos_sensor(0x33F6,0x00);//,[RO] vref_code[7:0];
	T4K04_write_cmos_sensor(0x33F7,0x00);//,[RO] -/-/-/-/tdac_c1val[11:8];
	T4K04_write_cmos_sensor(0x33F8,0x00);//,[RO] tdac_c1val[7:0];
	T4K04_write_cmos_sensor(0x33FB,0x00);//,[RO] -/-/-/-/hcnt_max[19:16];
	T4K04_write_cmos_sensor(0x33FC,0x00);//,[RO] hcnt_max[15:8];
	T4K04_write_cmos_sensor(0x33FD,0x00);//,[RO] hcnt_max[7:0];
	T4K04_write_cmos_sensor(0x33FE,0x00);//,[RO] tstr_vreg_exp[7:0];
	T4K04_write_cmos_sensor(0x3400,0x98);//,GLB0SET_MODE/-/GLBTGRESET_U[5:0];
	T4K04_write_cmos_sensor(0x3401,0x01);//,-/-/-/-/-/-/GLBTGRESET_W[9:8];
	T4K04_write_cmos_sensor(0x3402,0x14);//,GLBTGRESET_W[7:0];
	T4K04_write_cmos_sensor(0x3403,0x00);//,-/-/-/-/-/-/-/GLBREAD_1W[8];
	T4K04_write_cmos_sensor(0x3404,0x9C);//,GLBREAD_1W[7:0];
	T4K04_write_cmos_sensor(0x3405,0x00);//,-/-/-/-/-/-/-/GLBREAD_1D[8];
	T4K04_write_cmos_sensor(0x3406,0x90);//,GLBREAD_1D[7:0];
	T4K04_write_cmos_sensor(0x3407,0x00);//,-/-/-/-/-/-/-/GLBREAD_2W[8];
	T4K04_write_cmos_sensor(0x3408,0xCC);//,GLBREAD_2W[7:0];
	T4K04_write_cmos_sensor(0x3409,0x00);//,-/-/-/-/GLBREAD_2D[3:0];
	T4K04_write_cmos_sensor(0x340A,0x42);//,GLBZEROSET_U[3:0]/GLBZEROSET_W[3:0];
	T4K04_write_cmos_sensor(0x3410,0x16);//,-/GLBRSTDRAIN3_U[6:0];
	T4K04_write_cmos_sensor(0x3411,0x28);//,-/GBLRSTDRAIN_D[6:0];
	T4K04_write_cmos_sensor(0x3412,0x0C);//,-/-/GBLRSTDRAIN_U[5:0];
	T4K04_write_cmos_sensor(0x3414,0x00);//,SENDUM_1U[7:0];
	T4K04_write_cmos_sensor(0x3415,0x00);//,SENDUM_1W[7:0];
	T4K04_write_cmos_sensor(0x3416,0x00);//,SENDUM_2U[7:0];
	T4K04_write_cmos_sensor(0x3417,0x00);//,SENDUM_2W[7:0];
	T4K04_write_cmos_sensor(0x3418,0x00);//,SENDUM_3U[7:0];
	T4K04_write_cmos_sensor(0x3419,0x00);//,SENDUM_3W[7:0];
	T4K04_write_cmos_sensor(0x341A,0x30);//,BSC_ULMT_AGRNG2[7:0];
	T4K04_write_cmos_sensor(0x341B,0x14);//,BSC_ULMT_AGRNG1[7:0];
	T4K04_write_cmos_sensor(0x341C,0x0C);//,BSC_ULMT_AGRNG0[7:0];
	T4K04_write_cmos_sensor(0x341D,0xA9);//,KBIASCNT_RNG3[3:0]/KBIASCNT_RNG2[3:0];
	T4K04_write_cmos_sensor(0x341E,0x87);//,KBIASCNT_RNG1[3:0]/KBIASCNT_RNG0[3:0];
	T4K04_write_cmos_sensor(0x341F,0x0F);//,-/-/-/-/LIMIT_BSC_MODE[3:0];
	T4K04_write_cmos_sensor(0x3420,0x00);//,PSRR[15:8];
	T4K04_write_cmos_sensor(0x3421,0x00);//,PSRR[7:0];
	T4K04_write_cmos_sensor(0x3423,0x00);//,-/-/POSBSTCUT/-/-/-/-/GDMOSCNTX4;
	T4K04_write_cmos_sensor(0x3424,0x10);//,-/-/-/DRADRVSTPEN/-/-/-/VREFMSADIP;
	T4K04_write_cmos_sensor(0x3425,0x31);//,-/-/ADCMP1SRTSEL/CMPAMPCAPEN/-/-/VREFTEST[1:0];
	T4K04_write_cmos_sensor(0x3426,0x00);//,ST_PSREV/-/-/-/-/-/-/-;
	T4K04_write_cmos_sensor(0x3427,0x01);//,AGADJ_REV_INT/-/-/-/-/-/VREFIMX4_SEL/REV_INT_MODE;
	T4K04_write_cmos_sensor(0x3428,0x80);//,RI_VREFAD_COEF[7:0];
	T4K04_write_cmos_sensor(0x3429,0x11);//,SINT_RF_U[7:0];
	T4K04_write_cmos_sensor(0x342A,0x77);//,-/SINT_RF_W[6:0];
	T4K04_write_cmos_sensor(0x3434,0x00);//,ADCMP1SRT_NML_RS_U[7:0];
	T4K04_write_cmos_sensor(0x3435,0x23);//,-/ADCMP1SRT_D[6:0];
	T4K04_write_cmos_sensor(0x3436,0x00);//,-/-/-/-/-/-/ADCMP1SRT_NML_AD_U[9:8];
	T4K04_write_cmos_sensor(0x3437,0x00);//,ADCMP1SRT_NML_AD_U[7:0];
	T4K04_write_cmos_sensor(0x344E,0x0F);//,-/-/-/-/BSDIGITAL_MODE[3:0];
	T4K04_write_cmos_sensor(0x3450,0x07);//,-/-/-/-/FRAC_EXP_TIME_10NML[11:8];
	T4K04_write_cmos_sensor(0x3451,0xD8);//,FRAC_EXP_TIME_10NML[7:0];
	T4K04_write_cmos_sensor(0x3458,0x01);//,-/-/-/-/-/-/-/BGRDVSTPEN;
	T4K04_write_cmos_sensor(0x3459,0x27);//,BGRDVSTP_D[7:0];
	T4K04_write_cmos_sensor(0x345A,0x16);//,-/-/BGRDVSTP_U[5:0];
	T4K04_write_cmos_sensor(0x0100,0x01);//,-/-/-/-/-/-/-/MODE_SELECT;
	T4K04DB("T4K04_Sensor_Init exit_4lane :\n ");	
}



void T4K04PreviewSetting(void)
{
	// ==========Summary========
	// Extclk= 24Mhz  PCK = 242Mhz	 DCLK = 90.75MHz  MIPI Lane CLK = 363MHz  ES 1 line = 14.0992us
	// FPS = 30.13fps	Image output size = 1640x1232
	T4K04DB("T4K04PreviewSetting enter_4lane :\n ");	

	T4K04_write_cmos_sensor(0x0104,0x01);//,-/-/-/-/-/-/-/GROUP_PARA_HOLD
	T4K04_write_cmos_sensor(0x30AC,0x18);//,MHZ_EXTCLK_TB[15:8] ;		 
	T4K04_write_cmos_sensor(0x30AD,0x00);//,MHZ_EXTCLK_TB[7:0] ;		 
	T4K04_write_cmos_sensor(0x0301,0x06);//,- / - / - / - / VT_PIX_CLK_DI
	T4K04_write_cmos_sensor(0x0303,0x01);//,- / - / - / - / VT_SYS_CLK_DI
	T4K04_write_cmos_sensor(0x0305,0x05);//,- / - / - / - / - / PRE_PLL_C
	T4K04_write_cmos_sensor(0x0306,0x00);//,- / - / - / - / - / - / - / P
	T4K04_write_cmos_sensor(0x0307,0xF2);//,PLL_MULTIPLIER[7:0] ;		 
	T4K04_write_cmos_sensor(0x0309,0x08);//,- / - / - / - / OP_PIX_CLK_DI
	T4K04_write_cmos_sensor(0x030B,0x01);//,- / - / - / - / OP_SYS_CLK_DI
	T4K04_write_cmos_sensor(0x0340,0x09);//,FR_LENGTH_LINES[15:8] ; 	 
	T4K04_write_cmos_sensor(0x0341,0x32);//,FR_LENGTH_LINES[7:0] ;		 
	T4K04_write_cmos_sensor(0x0342,0x0D);//,LINE_LENGTH_PCK[15:8] ; 	 
	T4K04_write_cmos_sensor(0x0343,0x54);//,LINE_LENGTH_PCK[7:0] ;		 
	T4K04_write_cmos_sensor(0x0344,0x00);//,X_ADDR_START[15:8] ;		 
	T4K04_write_cmos_sensor(0x0345,0x00);//,X_ADDR_START[7:0] ; 		 
	T4K04_write_cmos_sensor(0x0346,0x00);//,Y_ADDR_START[15:8] ;		 
	T4K04_write_cmos_sensor(0x0347,0x00);//,Y_ADDR_START[7:0] ; 		 
	T4K04_write_cmos_sensor(0x0348,0x0C);//,X_ADDR_END[15:8] ;			 
	T4K04_write_cmos_sensor(0x0349,0xCF);//,X_ADDR_END[7:0] ;			 
	T4K04_write_cmos_sensor(0x034A,0x09);//,Y_ADDR_END[15:8] ;			 
	T4K04_write_cmos_sensor(0x034B,0x9F);//,Y_ADDR_END[7:0] ;			 
	T4K04_write_cmos_sensor(0x034C,0x06);//,X_OUTPUT_SIZE[15:8] ;		 
	T4K04_write_cmos_sensor(0x034D,0x68);//,X_OUTPUT_SIZE[7:0] ;		 
	T4K04_write_cmos_sensor(0x034E,0x04);//,Y_OUTPUT_SIZE[15:8] ;		 
	T4K04_write_cmos_sensor(0x034F,0xD0);//,Y_OUTPUT_SIZE[7:0] ;		 
	T4K04_write_cmos_sensor(0x0401,0x02);//,- / - / - / - / - / - / SCALI
	T4K04_write_cmos_sensor(0x0403,0x00);//,- / - / - / - / - / - / - / S
	T4K04_write_cmos_sensor(0x0405,0x10);//,SCALE_M[7:0] ;				 
	T4K04_write_cmos_sensor(0x0408,0x00);//,- / - / - / DCROP_XOFS[12:8] 
	T4K04_write_cmos_sensor(0x0409,0x00);//,DCROP_XOFS[7:0] ;			 
	T4K04_write_cmos_sensor(0x040A,0x00);//,- / - / - / - / DCROP_YOFS[11
	T4K04_write_cmos_sensor(0x040B,0x00);//,DCROP_YOFS[7:0] ;			 
	T4K04_write_cmos_sensor(0x040C,0x0C);//,- / - / - / DCROP_WIDTH[12:8]
	T4K04_write_cmos_sensor(0x040D,0xD0);//,DCROP_WIDTH[7:0] ;			 
	T4K04_write_cmos_sensor(0x040E,0x09);//,- / - / - / - / DCROP_HIGT[11
	T4K04_write_cmos_sensor(0x040F,0xA0);//,DCROP_HIGT[7:0] ;			 
	T4K04_write_cmos_sensor(0x0820,0x6D);//,MSB_LBRATE[31:24];			 
	T4K04_write_cmos_sensor(0x0821,0x0E);//,MSB_LBRATE[23:16];			 
	T4K04_write_cmos_sensor(0x0900,0x01);//,- / - / - / - / - / - / - / B
	T4K04_write_cmos_sensor(0x0901,0x22);//,BINNING_TYPE[7:0] ; 		 

	T4K04_write_cmos_sensor(0x330F,0x06);//HCNT_MAX_FIXVAL[15:8];	 
	T4K04_write_cmos_sensor(0x3310,0xAA);//HCNT_MAX_FIXVAL[7:0];		 
	T4K04_write_cmos_sensor(0x3237,0x02);//-/-/-/-/-/SP_COUNT[10:8];
	T4K04_write_cmos_sensor(0x3238,0x40);//SP_COUNT[7:0]; 
	
	T4K04_write_cmos_sensor(0x0104,0x00);//,-/-/-/-/-/-/-/GROUP_PARA_HOLD
	
	T4K04DB("T4K04PreviewSetting out_4lane :\n ");	
}


void T4K04VideoSetting(void)
{
	// ==========Summary========
	// Extclk= 24Mhz  PCK = 261Mhz	 DCLK = 97.875MHz  MIPI Lane CLK = 391.5MHz  ES 1 line = 13.3333us
	// FPS = 30.05fps	Image output size = 3280x1846
	//sensor max support pclk is 261M
	T4K04DB("T4K04VideoSetting enter_4lane:\n ");	

	T4K04_write_cmos_sensor(0x0104,0x01);//,-/-/-/-/-/-/-/GROUP_PARA_HOLD
	T4K04_write_cmos_sensor(0x30AC,0x18);//,MHZ_EXTCLK_TB[15:8] ;
	T4K04_write_cmos_sensor(0x30AD,0x00);//,MHZ_EXTCLK_TB[7:0] ;
	T4K04_write_cmos_sensor(0x0301,0x06);//,- / - / - / - / VT_PIX_CLK_DIV[3:0] ;
	T4K04_write_cmos_sensor(0x0303,0x01);//,- / - / - / - / VT_SYS_CLK_DIV[3:0] ;
	T4K04_write_cmos_sensor(0x0305,0x05);//,- / - / - / - / - / PRE_PLL_CLK_DIV[2:0] ;
	T4K04_write_cmos_sensor(0x0306,0x01);//,- / - / - / - / - / - / - / PLL_MULTIPLIER[8] ;
	T4K04_write_cmos_sensor(0x0307,0x05);//,PLL_MULTIPLIER[7:0] ;
	T4K04_write_cmos_sensor(0x0309,0x08);//,- / - / - / - / OP_PIX_CLK_DIV[3:0] ;
	T4K04_write_cmos_sensor(0x030B,0x01);//,- / - / - / - / OP_SYS_CLK_DIV[3:0] ;
	
	T4K04_write_cmos_sensor(0x0340,0x09);//,FR_LENGTH_LINES[15:8] ;
	T4K04_write_cmos_sensor(0x0341,0xC0);//,FR_LENGTH_LINES[7:0] ;
	//T4K04_write_cmos_sensor(0x0341,0x73);//,FR_LENGTH_LINES[7:0] ;

	
	T4K04_write_cmos_sensor(0x0342,0x0D);//,LINE_LENGTH_PCK[15:8] ;
	T4K04_write_cmos_sensor(0x0343,0x98);//,LINE_LENGTH_PCK[7:0] ;
	
	T4K04_write_cmos_sensor(0x0344,0x00);//,X_ADDR_START[15:8] ;
	T4K04_write_cmos_sensor(0x0345,0x00);//,X_ADDR_START[7:0] ;
	T4K04_write_cmos_sensor(0x0346,0x01);//,Y_ADDR_START[15:8] ;
	T4K04_write_cmos_sensor(0x0347,0x35);//,Y_ADDR_START[7:0] ;
	T4K04_write_cmos_sensor(0x0348,0x0C);//,X_ADDR_END[15:8] ;
	T4K04_write_cmos_sensor(0x0349,0xCF);//,X_ADDR_END[7:0] ;
	T4K04_write_cmos_sensor(0x034A,0x08);//,Y_ADDR_END[15:8] ;
	T4K04_write_cmos_sensor(0x034B,0x6A);//,Y_ADDR_END[7:0] ;
	T4K04_write_cmos_sensor(0x034C,0x0C);//,X_OUTPUT_SIZE[15:8] ;
	T4K04_write_cmos_sensor(0x034D,0xD0);//,X_OUTPUT_SIZE[7:0] ;
	T4K04_write_cmos_sensor(0x034E,0x07);//,Y_OUTPUT_SIZE[15:8] ;
	T4K04_write_cmos_sensor(0x034F,0x36);//,Y_OUTPUT_SIZE[7:0] ;
	T4K04_write_cmos_sensor(0x0401,0x02);//,- / - / - / - / - / - / SCALING_MODE[1:0] ;
	T4K04_write_cmos_sensor(0x0403,0x00);//,- / - / - / - / - / - / - / SPATIAL_SAMPLING ;
	T4K04_write_cmos_sensor(0x0405,0x10);//,SCALE_M[7:0] ;
	T4K04_write_cmos_sensor(0x0408,0x00);//,- / - / - / DCROP_XOFS[12:8] ;
	T4K04_write_cmos_sensor(0x0409,0x00);//,DCROP_XOFS[7:0] ;
	T4K04_write_cmos_sensor(0x040A,0x00);//,- / - / - / - / DCROP_YOFS[11:8] ;
	T4K04_write_cmos_sensor(0x040B,0x00);//,DCROP_YOFS[7:0] ;
	T4K04_write_cmos_sensor(0x040C,0x0C);//,- / - / - / DCROP_WIDTH[12:8] ;
	T4K04_write_cmos_sensor(0x040D,0xD0);//,DCROP_WIDTH[7:0] ;
	T4K04_write_cmos_sensor(0x040E,0x07);//,- / - / - / - / DCROP_HIGT[11:8] ;
	T4K04_write_cmos_sensor(0x040F,0x36);//,DCROP_HIGT[7:0] ;
	T4K04_write_cmos_sensor(0x0820,0x8F);//,MSB_LBRATE[31:24];
	T4K04_write_cmos_sensor(0x0821,0x0F);//,MSB_LBRATE[23:16];
	T4K04_write_cmos_sensor(0x0900,0x01);//,- / - / - / - / - / - / - / BINNING_MODE ;
	T4K04_write_cmos_sensor(0x0901,0x11);//,BINNING_TYPE[7:0] ;

	T4K04_write_cmos_sensor(0x330F,0x06);//HCNT_MAX_FIXVAL[15:8];	 
	T4K04_write_cmos_sensor(0x3310,0xCC);//HCNT_MAX_FIXVAL[7:0];		 
	T4K04_write_cmos_sensor(0x3237,0x02);//-/-/-/-/-/SP_COUNT[10:8];
	T4K04_write_cmos_sensor(0x3238,0x40);//SP_COUNT[7:0]; 

	T4K04_write_cmos_sensor(0x0104,0x00);//,-/-/-/-/-/-/-/GROUP_PARA_HOLD

	T4K04DB("T4K04VideoSetting out_4lane:\n ");	
}
	


void T4K04CaptureSetting(void)
{

	// ==========Summary========
	// Extclk= 24Mhz  PCK = 242Mhz	 DCLK = 90.75MHz  MIPI Lane CLK = 363MHz  ES 1 line = 14.3802us
	// FPS = 27.86fps	Image output size = 3280x2464

	T4K04DB("T4K04CaptureSetting enter_4lane :\n ");	
	T4K04_write_cmos_sensor(0x0104,0x01);//,-/-/-/-/-/-/-/GROUP_PARA_HOLD
	T4K04_write_cmos_sensor(0x30AC,0x18);//,MHZ_EXTCLK_TB[15:8] ;		 
	T4K04_write_cmos_sensor(0x30AD,0x00);//,MHZ_EXTCLK_TB[7:0] ;		 
	T4K04_write_cmos_sensor(0x0301,0x06);//,- / - / - / - / VT_PIX_CLK_DI
	T4K04_write_cmos_sensor(0x0303,0x01);//,- / - / - / - / VT_SYS_CLK_DI
	T4K04_write_cmos_sensor(0x0305,0x05);//,- / - / - / - / - / PRE_PLL_C
	T4K04_write_cmos_sensor(0x0306,0x00);//,- / - / - / - / - / - / - / P
	T4K04_write_cmos_sensor(0x0307,0xF2);//,PLL_MULTIPLIER[7:0] ;		 
	T4K04_write_cmos_sensor(0x0309,0x08);//,- / - / - / - / OP_PIX_CLK_DI
	T4K04_write_cmos_sensor(0x030B,0x01);//,- / - / - / - / OP_SYS_CLK_DI
	T4K04_write_cmos_sensor(0x0340,0x09);//,FR_LENGTH_LINES[15:8] ; 	 
	T4K04_write_cmos_sensor(0x0341,0xC0);//,FR_LENGTH_LINES[7:0] ;		 
	T4K04_write_cmos_sensor(0x0342,0x0D);//,LINE_LENGTH_PCK[15:8] ; 	 
	T4K04_write_cmos_sensor(0x0343,0x98);//,LINE_LENGTH_PCK[7:0] ;		 
	T4K04_write_cmos_sensor(0x0344,0x00);//,X_ADDR_START[15:8] ;		 
	T4K04_write_cmos_sensor(0x0345,0x00);//,X_ADDR_START[7:0] ; 		 
	T4K04_write_cmos_sensor(0x0346,0x00);//,Y_ADDR_START[15:8] ;		 
	T4K04_write_cmos_sensor(0x0347,0x00);//,Y_ADDR_START[7:0] ; 		 
	T4K04_write_cmos_sensor(0x0348,0x0C);//,X_ADDR_END[15:8] ;			 
	T4K04_write_cmos_sensor(0x0349,0xCF);//,X_ADDR_END[7:0] ;			 
	T4K04_write_cmos_sensor(0x034A,0x09);//,Y_ADDR_END[15:8] ;			 
	T4K04_write_cmos_sensor(0x034B,0x9F);//,Y_ADDR_END[7:0] ;			 
	T4K04_write_cmos_sensor(0x034C,0x0C);//,X_OUTPUT_SIZE[15:8] ;		 
	T4K04_write_cmos_sensor(0x034D,0xD0);//,X_OUTPUT_SIZE[7:0] ;		 
	T4K04_write_cmos_sensor(0x034E,0x09);//,Y_OUTPUT_SIZE[15:8] ;		 
	T4K04_write_cmos_sensor(0x034F,0xA0);//,Y_OUTPUT_SIZE[7:0] ;		 
	T4K04_write_cmos_sensor(0x0401,0x02);//,- / - / - / - / - / - / SCALI
	T4K04_write_cmos_sensor(0x0403,0x00);//,- / - / - / - / - / - / - / S
	T4K04_write_cmos_sensor(0x0405,0x10);//,SCALE_M[7:0] ;				 
	T4K04_write_cmos_sensor(0x0408,0x00);//,- / - / - / DCROP_XOFS[12:8] 
	T4K04_write_cmos_sensor(0x0409,0x00);//,DCROP_XOFS[7:0] ;			 
	T4K04_write_cmos_sensor(0x040A,0x00);//,- / - / - / - / DCROP_YOFS[11
	T4K04_write_cmos_sensor(0x040B,0x00);//,DCROP_YOFS[7:0] ;			 
	T4K04_write_cmos_sensor(0x040C,0x0C);//,- / - / - / DCROP_WIDTH[12:8]
	T4K04_write_cmos_sensor(0x040D,0xD0);//,DCROP_WIDTH[7:0] ;			 
	T4K04_write_cmos_sensor(0x040E,0x09);//,- / - / - / - / DCROP_HIGT[11
	T4K04_write_cmos_sensor(0x040F,0xA0);//,DCROP_HIGT[7:0] ;			 
	T4K04_write_cmos_sensor(0x0820,0x6D);//,MSB_LBRATE[31:24];			 
	T4K04_write_cmos_sensor(0x0821,0x0E);//,MSB_LBRATE[23:16];			 
	T4K04_write_cmos_sensor(0x0900,0x01);//,- / - / - / - / - / - / - / B
	T4K04_write_cmos_sensor(0x0901,0x11);//,BINNING_TYPE[7:0] ; 	

	T4K04_write_cmos_sensor(0x330F,0x06);//HCNT_MAX_FIXVAL[15:8];	 
	T4K04_write_cmos_sensor(0x3310,0xCC);//HCNT_MAX_FIXVAL[7:0];		 
	T4K04_write_cmos_sensor(0x3237,0x02);//-/-/-/-/-/SP_COUNT[10:8];
	T4K04_write_cmos_sensor(0x3238,0x40);//SP_COUNT[7:0]; 
	
	T4K04_write_cmos_sensor(0x0104,0x00);//,-/-/-/-/-/-/-/GROUP_PARA_HOLD
	T4K04DB("T4K04CaptureSetting out_4lane:\n ");	
}



UINT32 T4K04Open(void)
{
	kal_uint16 sensor_id = 0;
	
	T4K04DB("T4K04Open enter :\n ");
	
	sensor_id = T4K04_read_cmos_sensor(0x0000);
	T4K04DB("T4K04 READ ID :%x",sensor_id);
	
	if(sensor_id != T4K04_SENSOR_ID)
	{
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	
	spin_lock(&t4k04mipiraw_drv_lock);
	t4k04.sensorMode = SENSOR_MODE_INIT;
	t4k04.T4K04AutoFlickerMode = KAL_FALSE;
	t4k04.T4K04VideoMode = KAL_FALSE;
	spin_unlock(&t4k04mipiraw_drv_lock);
	
	T4K04_Sensor_Init();
	
	spin_lock(&t4k04mipiraw_drv_lock);
	t4k04.DummyLines= 0;
	t4k04.DummyPixels= 0;
	
	t4k04.pvPclk =  (24200);
	t4k04.capPclk = (24200);
	t4k04.videoPclk = (26100);
	
	t4k04.shutter = 0x09C0;
	t4k04.maxExposureLines =T4K04_PV_PERIOD_LINE_NUMS;
	
	t4k04.ispBaseGain = BASEGAIN;//0x40
	t4k04.sensorGlobalGain = 0x41;//1X
	t4k04.pvGain = 0x41;
	t4k04.realGain = BASEGAIN;//1x
	spin_unlock(&t4k04mipiraw_drv_lock);
	
	T4K04DB("T4K04Open exit :\n ");
	
    return ERROR_NONE;
}

/*************************************************************************
* FUNCTION
*   T4K04GetSensorID
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
UINT32 T4K04GetSensorID(UINT32 *sensorID)
{
    int  retry = 3; 

	T4K04DB("T4K04GetSensorID enter :\n ");
    mDELAY(5);
	
    do {
        *sensorID = T4K04_read_cmos_sensor(0x0000);

        if (*sensorID == T4K04_SENSOR_ID)
        	{
        		T4K04DB("Sensor ID = 0x%04x\n", *sensorID);
            	break; 
        	}
        T4K04DB("Read Sensor ID Fail = 0x%04x\n", *sensorID); 
        retry--; 
    } while (retry > 0);

    if (*sensorID != T4K04_SENSOR_ID) {
        *sensorID = 0xFFFFFFFF; 
        return ERROR_SENSOR_CONNECT_FAIL;
    }
    return ERROR_NONE;
}


/*************************************************************************
* FUNCTION
*   T4K04_SetShutter
*
* DESCRIPTION
*   This function set e-shutter of T4K04 to change exposure time.
*
* PARAMETERS
*   shutter : exposured lines
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void T4K04_SetShutter(kal_uint32 iShutter)
{

	T4K04DB("4K04:featurecontrol:iShutter=%d\n",iShutter);
	
	//if(t4k04.shutter == iShutter)
	//	return;
	
   spin_lock(&t4k04mipiraw_drv_lock);
   t4k04.shutter= iShutter;
   spin_unlock(&t4k04mipiraw_drv_lock);
   
   T4K04_write_shutter(iShutter);
   return;
}   /*  T4K04_SetShutter   */



/*************************************************************************
* FUNCTION
*   T4K04_read_shutter
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
UINT32 T4K04_read_shutter(void)
{

	kal_uint16 temp_reg1, temp_reg2;
	UINT32 shutter =0;

	temp_reg1 = T4K04_read_cmos_sensor(0x0202);
	temp_reg2 = T4K04_read_cmos_sensor(0x0203);
	
	shutter  = ((temp_reg1 <<8)|temp_reg2);

	return shutter;
}

/*************************************************************************
* FUNCTION
*   T4K04_night_mode
*
* DESCRIPTION
*   This function night mode of T4K04.
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
void T4K04_NightMode(kal_bool bEnable)
{
}



/*************************************************************************
* FUNCTION
*   T4K04Close
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
UINT32 T4K04Close(void)
{
    //work-around for Power-on sequence[SW-SleepIn]
	//T4K04_write_cmos_sensor(0x0100,0x00);

    return ERROR_NONE;
}

void T4K04SetFlipMirror(kal_int32 imgMirror)
{

	T4K04DB("T4K04SetFlipMirror  imgMirror =%d:\n ",imgMirror);	

    switch (imgMirror)
    {
        case IMAGE_NORMAL:
            T4K04_write_cmos_sensor(0x0101,0x00);
            break;
        case IMAGE_H_MIRROR:
            T4K04_write_cmos_sensor(0x0101,0x01);
            break;
        case IMAGE_V_MIRROR:
            T4K04_write_cmos_sensor(0x0101,0x02);
            break;
        case IMAGE_HV_MIRROR:
            T4K04_write_cmos_sensor(0x0101,0x03);
            break;
    }
}


/*************************************************************************
* FUNCTION
*   T4K04Preview
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
UINT32 T4K04Preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                                                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	T4K04DB("T4K04Preview enter:");
	
	T4K04PreviewSetting();
 
	spin_lock(&t4k04mipiraw_drv_lock);
	t4k04.sensorMode = SENSOR_MODE_PREVIEW;
	t4k04.DummyPixels =0;
    t4k04.DummyLines  =0;
	spin_unlock(&t4k04mipiraw_drv_lock);

	//T4K04DB("[T4K04Preview] mirror&flip: %d \n",sensor_config_data->SensorImageMirror);
	spin_lock(&t4k04mipiraw_drv_lock);
	sensor_config_data->SensorImageMirror = IMAGE_HV_MIRROR;//by layout
	
	t4k04.imgMirror = sensor_config_data->SensorImageMirror;
	spin_unlock(&t4k04mipiraw_drv_lock);
	T4K04SetFlipMirror(sensor_config_data->SensorImageMirror);
	
	T4K04DB("T4K04Preview exit:\n");
    return ERROR_NONE;
}

UINT32 T4K04Video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                                                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	T4K04DB("T4K04Video enter:");
	
	T4K04VideoSetting();
	
	spin_lock(&t4k04mipiraw_drv_lock);
	t4k04.sensorMode = SENSOR_MODE_VIDEO;
	t4k04.DummyPixels =0;
    t4k04.DummyLines  =0;
	sensor_config_data->SensorImageMirror = IMAGE_HV_MIRROR;//by layout
	t4k04.imgMirror = sensor_config_data->SensorImageMirror;
	spin_unlock(&t4k04mipiraw_drv_lock);

	T4K04SetFlipMirror(sensor_config_data->SensorImageMirror);
	
	T4K04DB("T4K04Video exit:\n");
    return ERROR_NONE;
}



UINT32 T4K04Capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                                                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

 	kal_uint32 shutter = t4k04.shutter;
	kal_uint32 pv_line_length , cap_line_length,temp_data;
	
	T4K04DB("T4K04Capture enter:\n");
	
	if( SENSOR_MODE_CAPTURE != t4k04.sensorMode)
	{
		T4K04CaptureSetting();
		mdelay(100);
	}
	
	spin_lock(&t4k04mipiraw_drv_lock);		
	t4k04.sensorMode = SENSOR_MODE_CAPTURE;
	sensor_config_data->SensorImageMirror = IMAGE_HV_MIRROR;//by layout
	t4k04.imgMirror = sensor_config_data->SensorImageMirror;
	t4k04.DummyPixels =0;
	t4k04.DummyLines  =0;
	spin_unlock(&t4k04mipiraw_drv_lock);
	
	T4K04SetFlipMirror(sensor_config_data->SensorImageMirror);

	//T4K04_SetDummy( t4k04.DummyPixels, t4k04.DummyLines);

	T4K04DB("T4K04Capture exit:\n");
    return ERROR_NONE;
}	/* T4K04Capture() */


UINT32 T4K04GetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution)
{
    T4K04DB("T4K04GetResolution!!\n");
	pSensorResolution->SensorPreviewWidth	= T4K04_IMAGE_SENSOR_PV_WIDTH;
    pSensorResolution->SensorPreviewHeight	= T4K04_IMAGE_SENSOR_PV_HEIGHT;
	
	pSensorResolution->SensorVideoWidth		= T4K04_IMAGE_SENSOR_VDO_WIDTH;
    pSensorResolution->SensorVideoHeight	= T4K04_IMAGE_SENSOR_VDO_HEIGHT;
	
    pSensorResolution->SensorFullWidth		= T4K04_IMAGE_SENSOR_FULL_WIDTH;
    pSensorResolution->SensorFullHeight		= T4K04_IMAGE_SENSOR_FULL_HEIGHT; 
	
    return ERROR_NONE;
} 

UINT32 T4K04GetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId,
                                                MSDK_SENSOR_INFO_STRUCT *pSensorInfo,
                                                MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{	
	pSensorConfigData->SensorImageMirror = IMAGE_HV_MIRROR;//by layout
	
	switch(pSensorConfigData->SensorImageMirror)
	{
		case IMAGE_NORMAL:
			 pSensorInfo->SensorOutputDataFormat   = SENSOR_OUTPUT_FORMAT_RAW_Gr;
             break;
		case IMAGE_H_MIRROR:
			pSensorInfo->SensorOutputDataFormat	 = SENSOR_OUTPUT_FORMAT_RAW_R;
			break;
		case IMAGE_V_MIRROR:
			pSensorInfo->SensorOutputDataFormat	  = SENSOR_OUTPUT_FORMAT_RAW_B;
			break;
		case IMAGE_HV_MIRROR:
			pSensorInfo->SensorOutputDataFormat    = SENSOR_OUTPUT_FORMAT_RAW_Gb;
			break;
	}

	spin_lock(&t4k04mipiraw_drv_lock);
	t4k04.imgMirror = pSensorConfigData->SensorImageMirror ;
	spin_unlock(&t4k04mipiraw_drv_lock);
	T4K04DB("[T4K04GetInfo]SensorImageMirror:%d\n", t4k04.imgMirror );

	pSensorInfo->SensorClockPolarity =SENSOR_CLOCK_POLARITY_LOW; 
    pSensorInfo->SensorClockFallingPolarity=SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;

    pSensorInfo->SensroInterfaceType=SENSOR_INTERFACE_TYPE_MIPI;

    pSensorInfo->CaptureDelayFrame = 3; 
    pSensorInfo->PreviewDelayFrame = 2; 
    pSensorInfo->VideoDelayFrame = 2; 

    pSensorInfo->SensorDrivingCurrent = ISP_DRIVING_6MA;      
    pSensorInfo->AEShutDelayFrame = 0;//0;		    /* The frame of setting shutter default 0 for TG int */
    pSensorInfo->AESensorGainDelayFrame = 1 ;//0;     /* The frame of setting sensor gain */
    pSensorInfo->AEISPGainDelayFrame = 2;	
	   
    switch (ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            pSensorInfo->SensorClockFreq=24;
            pSensorInfo->SensorClockRisingCount= 0;

            pSensorInfo->SensorGrabStartX = T4K04_PV_X_START; 
            pSensorInfo->SensorGrabStartY = T4K04_PV_Y_START;  
			
            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;			
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0; 
	     	pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 4; 
	    	pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorPacketECCOrder = 1;
            break;

        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            pSensorInfo->SensorClockFreq=24;
            pSensorInfo->SensorClockRisingCount= 0;

            pSensorInfo->SensorGrabStartX = T4K04_VIDEO_X_START; 
            pSensorInfo->SensorGrabStartY = T4K04_VIDEO_Y_START;  
			
            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;			
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0; 
	     	pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 4; 
	    	pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorPacketECCOrder = 1;
            break;

        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
            pSensorInfo->SensorClockFreq=24;
            pSensorInfo->SensorClockRisingCount= 0;
			
            pSensorInfo->SensorGrabStartX = T4K04_FULL_X_START;	
            pSensorInfo->SensorGrabStartY = T4K04_FULL_Y_START;	
            
            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;			
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0; 
            pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 4; 
            pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0; 
            pSensorInfo->SensorPacketECCOrder = 1;
            break;
        default:
			pSensorInfo->SensorClockFreq=24;
            pSensorInfo->SensorClockRisingCount= 0;
			
            pSensorInfo->SensorGrabStartX = T4K04_PV_X_START; 
            pSensorInfo->SensorGrabStartY = T4K04_PV_Y_START;  
			
            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;			
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0; 
	     	pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 4; 
	    	pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorPacketECCOrder = 1;            
            break;
    }

    memcpy(pSensorConfigData, &T4K04SensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));

    return ERROR_NONE;
}


UINT32 T4K04Control(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow,
                                                MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
		spin_lock(&t4k04mipiraw_drv_lock);
		T4K04CurrentScenarioId = ScenarioId;
		spin_unlock(&t4k04mipiraw_drv_lock);
		T4K04DB("T4K04CurrentScenarioId=%d\n",T4K04CurrentScenarioId);
    switch (ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            T4K04Preview(pImageWindow, pSensorConfigData);
            break;

        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            T4K04Video(pImageWindow, pSensorConfigData);
			break;
			
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
            T4K04Capture(pImageWindow, pSensorConfigData);
            break;

        default:
            return ERROR_INVALID_SCENARIO_ID;

    }
    return ERROR_NONE;
}


UINT32 T4K04SetVideoMode(UINT16 u2FrameRate)
{

    kal_uint32 MAX_Frame_length =0,frameRate=0,extralines=0;
    T4K04DB("[T4K04SetVideoMode] frame rate = %d\n", u2FrameRate);

	if(u2FrameRate==0)
	{
		T4K04DB("T4K04SetVideoMode FPS:0\n");

		//for dynamic fps
		t4k04.DummyPixels = 0;
		t4k04.DummyLines = 0;
		T4K04_SetDummy(t4k04.DummyPixels,t4k04.DummyLines);
		
		return KAL_TRUE;
	}
	if(u2FrameRate >30 || u2FrameRate <5)
	    T4K04DB("error frame rate seting,u2FrameRate =%d\n",u2FrameRate);

    if(t4k04.sensorMode == SENSOR_MODE_VIDEO)
    {
    	if(t4k04.T4K04AutoFlickerMode == KAL_TRUE)
    	{
    		if (u2FrameRate==30)
				frameRate= 296;
			else if(u2FrameRate==15)
				frameRate= 148;
			else
				frameRate=u2FrameRate*10;
			
			MAX_Frame_length = (t4k04.videoPclk*10000)/(T4K04_VIDEO_PERIOD_PIXEL_NUMS + t4k04.DummyPixels)/frameRate*10;
    	}
		else
			MAX_Frame_length = (t4k04.videoPclk*10000) /(T4K04_VIDEO_PERIOD_PIXEL_NUMS + t4k04.DummyPixels)/u2FrameRate;
				
		if((MAX_Frame_length <=T4K04_VIDEO_PERIOD_LINE_NUMS))
		{
			MAX_Frame_length = T4K04_VIDEO_PERIOD_LINE_NUMS;
			T4K04DB("[T4K04SetVideoMode]current fps = %d\n", (t4k04.videoPclk*10000)/(T4K04_VIDEO_PERIOD_PIXEL_NUMS + t4k04.DummyPixels)/T4K04_PV_PERIOD_LINE_NUMS);	
		}
		T4K04DB("[T4K04SetVideoMode]current fps (10 base)= %d\n", (t4k04.videoPclk*10000)*10/(T4K04_VIDEO_PERIOD_PIXEL_NUMS + t4k04.DummyPixels)/MAX_Frame_length);
		extralines = MAX_Frame_length - T4K04_VIDEO_PERIOD_LINE_NUMS;
	
		T4K04_SetDummy(t4k04.DummyPixels,extralines);
    }

	T4K04DB("[T4K04SetVideoMode]MAX_Frame_length=%d,t4k04.DummyLines=%d\n",MAX_Frame_length,t4k04.DummyLines);
	
    return KAL_TRUE;
}

UINT32 T4K04SetAutoFlickerMode(kal_bool bEnable, UINT16 u2FrameRate)
{

    T4K04DB("[T4K04SetAutoFlickerMode] enable =%d, frame rate(10base) =  %d\n", bEnable, u2FrameRate);
	if(bEnable) {
		spin_lock(&t4k04mipiraw_drv_lock);
		t4k04.T4K04AutoFlickerMode = KAL_TRUE; 
		spin_unlock(&t4k04mipiraw_drv_lock);
    } else {
    	spin_lock(&t4k04mipiraw_drv_lock);
        t4k04.T4K04AutoFlickerMode = KAL_FALSE; 
		spin_unlock(&t4k04mipiraw_drv_lock);
        T4K04DB("Disable Auto flicker\n");    
    }

    return ERROR_NONE;
}

UINT32 T4K04SetTestPatternMode(kal_bool bEnable)
{
    T4K04DB("[T4K04SetTestPatternMode] Test pattern enable:%d\n", bEnable);
	
    return ERROR_NONE;
}


UINT32 T4K04MIPISetMaxFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 frameRate) {
	kal_uint32 pclk;
	kal_int16 dummyLine;
	kal_uint16 lineLength,frameHeight;
		
	T4K04DB("T4K04MIPISetMaxFramerateByScenario: scenarioId = %d, frame rate = %d\n",scenarioId,frameRate);
	switch (scenarioId) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			pclk = T4K04_PREVIEW_PCLK;
			lineLength = T4K04_PV_PERIOD_PIXEL_NUMS;
			frameHeight = (10 * pclk)/frameRate/lineLength;
			dummyLine = frameHeight - T4K04_PV_PERIOD_LINE_NUMS;
			if(dummyLine<0)
				dummyLine = 0;
			spin_lock(&t4k04mipiraw_drv_lock);
			t4k04.DummyLines = dummyLine;
			t4k04.sensorMode = SENSOR_MODE_PREVIEW;
			spin_unlock(&t4k04mipiraw_drv_lock);
			T4K04_SetDummy(t4k04.DummyPixels,t4k04.DummyLines);
			break;			
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			pclk = T4K04_VIDEO_PCLK;
			lineLength = T4K04_VIDEO_PERIOD_PIXEL_NUMS;
			frameHeight = (10 * pclk)/frameRate/lineLength;
			dummyLine = frameHeight - T4K04_VIDEO_PERIOD_LINE_NUMS;
			if(dummyLine<0)
				dummyLine = 0;
			spin_lock(&t4k04mipiraw_drv_lock);
			t4k04.DummyLines = dummyLine;
			t4k04.sensorMode = SENSOR_MODE_VIDEO;
			spin_unlock(&t4k04mipiraw_drv_lock);
			T4K04_SetDummy(t4k04.DummyPixels,t4k04.DummyLines);
			break;			
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:			
			pclk = T4K04_CAPTURE_PCLK;
			lineLength = T4K04_FULL_PERIOD_PIXEL_NUMS;
			frameHeight = (10 * pclk)/frameRate/lineLength;
			dummyLine = frameHeight - T4K04_FULL_PERIOD_LINE_NUMS;
			if(dummyLine<0)
				dummyLine = 0;
			spin_lock(&t4k04mipiraw_drv_lock);
			t4k04.DummyLines = dummyLine;
			t4k04.sensorMode = SENSOR_MODE_CAPTURE;
			spin_unlock(&t4k04mipiraw_drv_lock);
			T4K04_SetDummy(t4k04.DummyPixels,t4k04.DummyLines);
			break;		
        case MSDK_SCENARIO_ID_CAMERA_3D_PREVIEW: 
            break;
        case MSDK_SCENARIO_ID_CAMERA_3D_VIDEO:
			break;
        case MSDK_SCENARIO_ID_CAMERA_3D_CAPTURE:    
			break;		
		default:
			break;
	}	
	return ERROR_NONE;
}


UINT32 T4K04MIPIGetDefaultFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 *pframeRate) 
{

	switch (scenarioId) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			 *pframeRate = 300;
			 break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
			 *pframeRate = 278;
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

UINT32 T4K04FeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId,
                                                                UINT8 *pFeaturePara,UINT32 *pFeatureParaLen)
{
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
 
    switch (FeatureId)
    {
        case SENSOR_FEATURE_GET_RESOLUTION:
            *pFeatureReturnPara16++= T4K04_IMAGE_SENSOR_FULL_WIDTH;
            *pFeatureReturnPara16= T4K04_IMAGE_SENSOR_FULL_HEIGHT;
            *pFeatureParaLen=4;
            break;
        case SENSOR_FEATURE_GET_PERIOD:
			switch(T4K04CurrentScenarioId)
			{
				//T4K04DB("T4K04FeatureControl:T4K04CurrentScenarioId:%d\n",T4K04CurrentScenarioId);
				case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				case MSDK_SCENARIO_ID_CAMERA_ZSD:
            		*pFeatureReturnPara16++= T4K04_FULL_PERIOD_PIXEL_NUMS + t4k04.DummyPixels;  
            		*pFeatureReturnPara16= T4K04_FULL_PERIOD_LINE_NUMS + t4k04.DummyLines;	
            		T4K04DB("capture_Sensor period:%d ,%d\n", T4K04_FULL_PERIOD_PIXEL_NUMS + t4k04.DummyPixels, T4K04_FULL_PERIOD_LINE_NUMS + t4k04.DummyLines); 
            		*pFeatureParaLen=4;        				
					break;
				case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            		*pFeatureReturnPara16++= T4K04_VIDEO_PERIOD_PIXEL_NUMS + t4k04.DummyPixels;  
            		*pFeatureReturnPara16= T4K04_VIDEO_PERIOD_LINE_NUMS + t4k04.DummyLines;	
            		T4K04DB("video_Sensor period:%d ,%d\n", T4K04_VIDEO_PERIOD_PIXEL_NUMS + t4k04.DummyPixels, T4K04_VIDEO_PERIOD_LINE_NUMS + t4k04.DummyLines); 
            		*pFeatureParaLen=4;  
        			break;
				case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            		*pFeatureReturnPara16++= T4K04_PV_PERIOD_PIXEL_NUMS + t4k04.DummyPixels;  
            		*pFeatureReturnPara16= T4K04_PV_PERIOD_LINE_NUMS + t4k04.DummyLines;
            		T4K04DB("preview_Sensor period:%d ,%d\n", T4K04_PV_PERIOD_PIXEL_NUMS  + t4k04.DummyPixels, T4K04_PV_PERIOD_LINE_NUMS + t4k04.DummyLines); 
            		*pFeatureParaLen=4;
        			break;
				default:	
            		*pFeatureReturnPara16++= T4K04_PV_PERIOD_PIXEL_NUMS + t4k04.DummyPixels;  
            		*pFeatureReturnPara16= T4K04_PV_PERIOD_LINE_NUMS + t4k04.DummyLines;
            		T4K04DB("Sensor period:%d ,%d\n", T4K04_PV_PERIOD_PIXEL_NUMS  + t4k04.DummyPixels, T4K04_PV_PERIOD_LINE_NUMS + t4k04.DummyLines); 
            		*pFeatureParaLen=4;
        			break;
  				}
          	break;
        case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
			switch(T4K04CurrentScenarioId)
			{
				//T4K04DB("T4K04FeatureControl:T4K04CurrentScenarioId:%d\n",T4K04CurrentScenarioId);
				case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
					*pFeatureReturnPara32 = T4K04_PREVIEW_PCLK;
					*pFeatureParaLen=4;
				break;
				case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
					*pFeatureReturnPara32 = T4K04_VIDEO_PCLK;
					*pFeatureParaLen=4;
				break;
				case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				case MSDK_SCENARIO_ID_CAMERA_ZSD:
					*pFeatureReturnPara32 = T4K04_CAPTURE_PCLK;
					*pFeatureParaLen=4;
				break;
				default:
					*pFeatureReturnPara32 = T4K04_PREVIEW_PCLK;
					*pFeatureParaLen=4;
				break;
			}
		    break;
        case SENSOR_FEATURE_SET_ESHUTTER:
            T4K04_SetShutter(*pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_NIGHTMODE:
            T4K04_NightMode((BOOL) *pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_GAIN:
            T4K04_SetGain((UINT16) *pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_FLASHLIGHT:
            break;
        case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
            //T4K04_isp_master_clock=*pFeatureData32;
            break;
        case SENSOR_FEATURE_SET_REGISTER:
            T4K04_write_cmos_sensor(pSensorRegData->RegAddr, pSensorRegData->RegData);
            break;
        case SENSOR_FEATURE_GET_REGISTER:
            pSensorRegData->RegData = T4K04_read_cmos_sensor(pSensorRegData->RegAddr);
            break;
        case SENSOR_FEATURE_SET_CCT_REGISTER:
            SensorRegNumber=FACTORY_END_ADDR;
            for (i=0;i<SensorRegNumber;i++)
            {
            	spin_lock(&t4k04mipiraw_drv_lock);
                T4K04SensorCCT[i].Addr=*pFeatureData32++;
                T4K04SensorCCT[i].Para=*pFeatureData32++;
				spin_unlock(&t4k04mipiraw_drv_lock);
            }
            break;
        case SENSOR_FEATURE_GET_CCT_REGISTER:
            SensorRegNumber=FACTORY_END_ADDR;
            if (*pFeatureParaLen<(SensorRegNumber*sizeof(SENSOR_REG_STRUCT)+4))
                return FALSE;
            *pFeatureData32++=SensorRegNumber;
            for (i=0;i<SensorRegNumber;i++)
            {
                *pFeatureData32++=T4K04SensorCCT[i].Addr;
                *pFeatureData32++=T4K04SensorCCT[i].Para;
            }
            break;
        case SENSOR_FEATURE_SET_ENG_REGISTER:
            SensorRegNumber=ENGINEER_END;
            for (i=0;i<SensorRegNumber;i++)
            {
            	spin_lock(&t4k04mipiraw_drv_lock);
                T4K04SensorReg[i].Addr=*pFeatureData32++;
                T4K04SensorReg[i].Para=*pFeatureData32++;
				spin_unlock(&t4k04mipiraw_drv_lock);
            }
            break;
        case SENSOR_FEATURE_GET_ENG_REGISTER:
            SensorRegNumber=ENGINEER_END;
            if (*pFeatureParaLen<(SensorRegNumber*sizeof(SENSOR_REG_STRUCT)+4))
                return FALSE;
            *pFeatureData32++=SensorRegNumber;
            for (i=0;i<SensorRegNumber;i++)
            {
                *pFeatureData32++=T4K04SensorReg[i].Addr;
                *pFeatureData32++=T4K04SensorReg[i].Para;
            }
            break;
        case SENSOR_FEATURE_GET_REGISTER_DEFAULT:
            if (*pFeatureParaLen>=sizeof(NVRAM_SENSOR_DATA_STRUCT))
            {
                pSensorDefaultData->Version=NVRAM_CAMERA_SENSOR_FILE_VERSION;
                pSensorDefaultData->SensorId=T4K04_SENSOR_ID;
                memcpy(pSensorDefaultData->SensorEngReg, T4K04SensorReg, sizeof(SENSOR_REG_STRUCT)*ENGINEER_END);
                memcpy(pSensorDefaultData->SensorCCTReg, T4K04SensorCCT, sizeof(SENSOR_REG_STRUCT)*FACTORY_END_ADDR);
            }
            else
                return FALSE;
            *pFeatureParaLen=sizeof(NVRAM_SENSOR_DATA_STRUCT);
            break;
        case SENSOR_FEATURE_GET_CONFIG_PARA:
            memcpy(pSensorConfigData, &T4K04SensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));
            *pFeatureParaLen=sizeof(MSDK_SENSOR_CONFIG_STRUCT);
            break;
        case SENSOR_FEATURE_CAMERA_PARA_TO_SENSOR:
            T4K04_camera_para_to_sensor();
            break;

        case SENSOR_FEATURE_SENSOR_TO_CAMERA_PARA:
            T4K04_sensor_to_camera_para();
            break;
        case SENSOR_FEATURE_GET_GROUP_COUNT:
            *pFeatureReturnPara32++=T4K04_get_sensor_group_count();
            *pFeatureParaLen=4;
            break;
        case SENSOR_FEATURE_GET_GROUP_INFO:
            T4K04_get_sensor_group_info(pSensorGroupInfo->GroupIdx, pSensorGroupInfo->GroupNamePtr, &pSensorGroupInfo->ItemCount);
            *pFeatureParaLen=sizeof(MSDK_SENSOR_GROUP_INFO_STRUCT);
            break;
        case SENSOR_FEATURE_GET_ITEM_INFO:
            T4K04_get_sensor_item_info(pSensorItemInfo->GroupIdx,pSensorItemInfo->ItemIdx, pSensorItemInfo);
            *pFeatureParaLen=sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
            break;

        case SENSOR_FEATURE_SET_ITEM_INFO:
            T4K04_set_sensor_item_info(pSensorItemInfo->GroupIdx, pSensorItemInfo->ItemIdx, pSensorItemInfo->ItemValue);
            *pFeatureParaLen=sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
            break;

        case SENSOR_FEATURE_GET_ENG_INFO:
            pSensorEngInfo->SensorId = 129;
            pSensorEngInfo->SensorType = CMOS_SENSOR;
			pSensorEngInfo->SensorOutputDataFormat	  = SENSOR_OUTPUT_FORMAT_RAW_Gb;
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
            T4K04SetVideoMode(*pFeatureData16);
            break;
        case SENSOR_FEATURE_CHECK_SENSOR_ID:
            T4K04GetSensorID(pFeatureReturnPara32); 
            break;             
        case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
            T4K04SetAutoFlickerMode((BOOL)*pFeatureData16, *(pFeatureData16+1));            
	        break;
        case SENSOR_FEATURE_SET_TEST_PATTERN:
            T4K04SetTestPatternMode((BOOL)*pFeatureData16);        	
            break;
			
		case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
			T4K04MIPISetMaxFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32, *(pFeatureData32+1));
			break;
		case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
			T4K04MIPIGetDefaultFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32, (MUINT32 *)(*(pFeatureData32+1)));
			break;
			
        default:
            break;
    }
    return ERROR_NONE;
}	/* T4K04FeatureControl() */


SENSOR_FUNCTION_STRUCT	SensorFuncT4K04=
{
    T4K04Open,  
    T4K04GetInfo,
    T4K04GetResolution,
    T4K04FeatureControl,
    T4K04Control,
    T4K04Close
};

UINT32 T4K04_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
    /* To Do : Check Sensor status here */
    if (pfFunc!=NULL)
        *pfFunc=&SensorFuncT4K04;

    return ERROR_NONE;
}   /* SensorInit() */

