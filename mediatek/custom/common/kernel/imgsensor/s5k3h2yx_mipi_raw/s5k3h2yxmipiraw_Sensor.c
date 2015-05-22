/*****************************************************************************
 *
 * Filename:
 * ---------
 *   sensor.c
 *
 * Project:
 * --------
 *   RAW
 *
 * Description:
 * ------------
 *   Source code of Sensor driver
 *
 *
 * Author:
 * -------
 *   Jun Pei (MTK70837)
 *
 *============================================================================
 *             HISTORY
 * Below this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *------------------------------------------------------------------------------
 * $Revision:$
 * $Modtime:$
 * $Log:$
 *
 * 04 12 2013 guoqing.liu
 * [ALPS00564761] sensor driver check in
 * sensor driver check in.
 *
 * 09 24 2012 chengxue.shen
 * NULL
 * .
 *
 * 09 21 2012 chengxue.shen
 * NULL
 * .
 *
 * 09 21 2012 chengxue.shen
 * NULL
 * .
 *
 * 09 17 2012 chengxue.shen
 * NULL
 * .
 *
 * 06 13 2011 koli.lin
 * [ALPS00053429] [Need Patch] [Volunteer Patch]
 * [Camera] Modify the sensor color order for the CCT tool.
 *
 * 06 07 2011 koli.lin
 * [ALPS00050047] [Camera]AE flash when set EV as -2
 * [Camera] Rollback the preview resolution to 800x600.
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
#include <asm/atomic.h>
#include <asm/system.h>

#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "s5k3h2yxmipiraw_Sensor.h"
#include "s5k3h2yxmipiraw_Camera_Sensor_para.h"
#include "s5k3h2yxmipiraw_CameraCustomized.h"

static DEFINE_SPINLOCK(s5k3h2yxmipiraw_drv_lock);

//#define PRE_PCLK 135200000

#define S5K3H2YXMIPI_DEBUG
#ifdef S5K3H2YXMIPI_DEBUG
#define SENSORDB printk
#else
#define SENSORDB(x,...)
#endif

static struct S5K3H2YXMIPI_sensor_STRUCT S5K3H2YXMIPI_sensor;//={S5K3H2YXMIPI_WRITE_ID,S5K3H2YXMIPI_READ_ID,KAL_TRUE,KAL_FALSE,KAL_TRUE,KAL_FALSE,
//KAL_FALSE,135200000,135200000/*114400000*/,800,0,64,64,3536,1270,3536,2534,0,0,0,0,30};

MSDK_SCENARIO_ID_ENUM CurrentScenarioId = MSDK_SCENARIO_ID_CAMERA_PREVIEW;

//kal_uint8 SCCB_Slave_addr[3][2] = {{S5K3H2YXMIPI_WRITE_ID_1,S5K3H2YXMIPI_READ_ID_1},{S5K3H2YXMIPI_WRITE_ID_2,S5K3H2YXMIPI_READ_ID_2},{S5K3H2YXMIPI_WRITE_ID_3,S5K3H2YXMIPI_READ_ID_3}};


/* FIXME: old factors and DIDNOT use now. s*/
SENSOR_REG_STRUCT S5K3H2YXMIPISensorCCT[]=CAMERA_SENSOR_CCT_DEFAULT_VALUE;
SENSOR_REG_STRUCT S5K3H2YXMIPISensorReg[ENGINEER_END]=CAMERA_SENSOR_REG_DEFAULT_VALUE;
/* FIXME: old factors and DIDNOT use now. e*/

MSDK_SENSOR_CONFIG_STRUCT S5K3H2YXMIPISensorConfigData;
/* Parameter For Engineer mode function */
kal_uint32 S5K3H2YXMIPI_FAC_SENSOR_REG;

kal_uint16 S5K3H2YXMIPI_MAX_EXPOSURE_LINES = S5K3H2YXMIPI_PV_FRAME_LENGTH_LINES -5;
kal_uint8  S5K3H2YXMIPI_MIN_EXPOSURE_LINES = 1;

//kal_uint32 S5K3H2YXMIPI_isp_master_clock;
//UINT8 S5K3H2YXMIPIPixelClockDivider=0;

#define S5K3H2YXMIPI_MaxGainIndex 50																				 // Gain Index
kal_uint16 S5K3H2YXMIPI_sensorGainMapping[S5K3H2YXMIPI_MaxGainIndex][2] = {
    { 64,  0}, { 68, 15}, { 71, 28}, { 75, 40}, { 79, 51}, {84, 61}, { 88, 70}, { 92, 78}, { 95, 85}, { 99, 92},
    {103, 98}, { 107, 104}, {112,110}, {116,115}, {119,119}, {124,124}, {128,128}, {136,136}, {143,142}, {151,148},
    {160,154}, {167,158}, {176,163}, {184,167}, {192,171}, {199,174}, {207,177}, {215,180}, {224,183}, {230,185},
    {240,188}, {248,190}, {256,192}, {273,196}, {287,199}, {303,202}, {321,205}, {334,207}, {348,209}, {364,211}, 
    {381,213}, {399,215}, {420,217}, {431,218}, {442,219}, {455,220}, {468,221}, {481,222}, {496,223}, {512,224}    
};



extern int iReadRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u8 * a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u16 i2cId);
kal_uint16 S5K3H2YXMIPI_Read_Gain(void);
static UINT32 S5K3H2YXMIPISetMaxFrameRate(UINT16 u2FrameRate);


kal_uint8 S5K3H2YXMIPI_read_cmos_sensor(kal_uint16 addr)
{
	kal_uint8 get_byte=0;
    char puSendCmd[2] = {(char)((addr&0xFF00) >> 8) , (char)(addr & 0xFF) };
	iReadRegI2C(puSendCmd , 2, (u8*)&get_byte,1,S5K3H2YXMIPI_WRITE_ID);
    return get_byte;
}

void S5K3H2YXMIPI_write_cmos_sensor(kal_uint16 addr, kal_uint8 para)
{

	char puSendCmd[3] = {(char)((addr&0xFF00) >> 8) , (char)(addr & 0xFF) ,(char)(para & 0xFF)};
	
	iWriteRegI2C(puSendCmd , 3,S5K3H2YXMIPI_WRITE_ID);
}


void S5K3H2YXMIPI_write_shutter(kal_uint16 shutter)
{

	kal_uint32 frame_length = 0,line_length=0;
    kal_uint32 realtime_fp = 0;
	kal_uint32 max_exp_shutter = 0;
	unsigned long flags;
	
	if (shutter < 3)
	  shutter = 3;

	if(KAL_TRUE==S5K3H2YXMIPI_sensor.bAutoFlickerMode){
		if(KAL_FALSE==S5K3H2YXMIPI_sensor.VideoMode){
		   if(SENSOR_MODE_ZSD_PREVIEW==S5K3H2YXMIPI_sensor.sensor_mode)
		   {
		      realtime_fp = S5K3H2YXMIPI_sensor.cp_pclk*10 / (S5K3H2YXMIPI_sensor.cp_line_length *S5K3H2YXMIPI_sensor.cp_frame_length);
			  if((realtime_fp >= 149)&&(realtime_fp <=153))
				  {
				  	 //Change frame 14.9fps ~ 15.3fps to do auto flick
					  realtime_fp = 148;
					  S5K3H2YXMIPI_sensor.cp_frame_length = S5K3H2YXMIPI_sensor.cp_pclk*10 /(S5K3H2YXMIPI_sensor.cp_line_length *realtime_fp);
					  SENSORDB("[S5K3H2YXMIPI]Write_shutter: autofliker realtime_fp=30,extern heights slowdown to 29.6fps][cp_frame_length:%d]",S5K3H2YXMIPI_sensor.cp_frame_length);
				  }

			  SENSORDB("[S5K3H2YXMIPI]Write_shutter:realtime_fp = %d\n",realtime_fp);
		   }
		   else
		   {
			   realtime_fp = S5K3H2YXMIPI_sensor.pv_pclk*10 / (S5K3H2YXMIPI_sensor.pv_line_length *S5K3H2YXMIPI_sensor.pv_frame_length);
			   if((realtime_fp >= 297)&&(realtime_fp <=303))
				   {
					  //Change frame 29.7fps ~ 30.3fps to do auto flick
					   realtime_fp = 296;
					   S5K3H2YXMIPI_sensor.pv_frame_length = S5K3H2YXMIPI_sensor.pv_pclk*10 /(S5K3H2YXMIPI_sensor.pv_line_length *realtime_fp);
					   SENSORDB("[S5K3H2YXMIPI]Write_shutter: autofliker realtime_fp=30,extern heights slowdown to 29.6fps][pv_frame_length:%d]",S5K3H2YXMIPI_sensor.pv_frame_length);
				   }
			   
			   SENSORDB("[S5K3H2YXMIPI]Write_shutter:realtime_fp = %d\n",realtime_fp);
		   }
		}
	}
	

  if (S5K3H2YXMIPI_sensor.sensor_mode == SENSOR_MODE_PREVIEW) 
  {
	  if(shutter > (S5K3H2YXMIPI_sensor.pv_frame_length - 8))
		  frame_length = shutter +8;
	  else 
		  frame_length = S5K3H2YXMIPI_sensor.pv_frame_length;

  }
  else
  {
	  if(shutter > (S5K3H2YXMIPI_sensor.cp_frame_length - 8))
		  frame_length = shutter +8;
	  else 
		  frame_length = S5K3H2YXMIPI_sensor.cp_frame_length;
  }

SENSORDB("[S5K3H2YXMIPI]Write_shutter:sensor_mode =%d,shutter=%d,frame_length=%d\n",S5K3H2YXMIPI_sensor.sensor_mode,shutter,frame_length);

 S5K3H2YXMIPI_write_cmos_sensor(0x0104, 1);    //Grouped parameter hold    

 S5K3H2YXMIPI_write_cmos_sensor(0x0340, (frame_length >>8) & 0xFF);
 S5K3H2YXMIPI_write_cmos_sensor(0x0341, frame_length & 0xFF);	 

 S5K3H2YXMIPI_write_cmos_sensor(0x0202, (shutter >> 8) & 0xFF);
 S5K3H2YXMIPI_write_cmos_sensor(0x0203, shutter  & 0xFF);
 
 S5K3H2YXMIPI_write_cmos_sensor(0x0104, 0);    //Grouped parameter release
   
}   /* write_S5K3H2YXMIPI_shutter */

kal_uint16 S5K3H2YXMIPI_Read_Gain(void)
{
    kal_uint8  temp_reg;
	kal_uint16 sensor_gain;

	sensor_gain = (((S5K3H2YXMIPI_read_cmos_sensor(0x0204)<<8)&0xFF00) | (S5K3H2YXMIPI_read_cmos_sensor(0x0205)&0xFF)); // ANALOG_GAIN_CTRLR  

	SENSORDB("read sensor gain value is %d",sensor_gain);
	
	return sensor_gain;
}  /* S5K4E5YA_Read_Gain */

/*************************************************************************
* FUNCTION
*    S5K3H2YXMIPI_SetGain
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
void S5K3H2YXMIPI_SetGain(UINT16 iGain)
{
	UINT16 gain = iGain;
	unsigned long flags;

	if(gain < (S5K3H2YX_MIN_ANALOG_GAIN * S5K3H2YXMIPI_sensor.ispBaseGain))
	{
		gain = S5K3H2YX_MIN_ANALOG_GAIN * S5K3H2YXMIPI_sensor.ispBaseGain;
		SENSORDB("[error] gain value is below 1*IspBaseGain!");
	}
	if(gain > (S5K3H2YX_MAX_ANALOG_GAIN * S5K3H2YXMIPI_sensor.ispBaseGain))
	{
		gain = S5K3H2YX_MAX_ANALOG_GAIN * S5K3H2YXMIPI_sensor.ispBaseGain;//Max up to 16X
		SENSORDB("[error] gain value exceeds 16*IspBaseGain!");
	}

	// Analog gain = Analog_gain_code[15:0] / 32
	spin_lock_irqsave(&s5k3h2yxmipiraw_drv_lock,flags);
	S5K3H2YXMIPI_sensor.sensorGlobalGain= (gain * S5K3H2YXMIPI_sensor.sensorBaseGain) / S5K3H2YXMIPI_sensor.ispBaseGain;
	spin_unlock_irqrestore(&s5k3h2yxmipiraw_drv_lock,flags);
	S5K3H2YXMIPI_write_cmos_sensor(0x0104, 0x01);	//Grouped parameter hold

	S5K3H2YXMIPI_write_cmos_sensor(0x0204, (S5K3H2YXMIPI_sensor.sensorGlobalGain & 0xFF00) >> 8); // ANALOG_GAIN_CTRLR
	S5K3H2YXMIPI_write_cmos_sensor(0x0205, S5K3H2YXMIPI_sensor.sensorGlobalGain & 0xFF);

	S5K3H2YXMIPI_write_cmos_sensor(0x0104, 0x00);	//Grouped parameter release

	SENSORDB("ISP gain=%d, S5K3H2YXMIPI_sensor.sensorGlobalGain=%d\n", gain, S5K3H2YXMIPI_sensor.sensorGlobalGain);

}


void S5K3H2YXMIPI_camera_para_to_sensor(void)
{
    kal_uint32    i;
    for(i=0; 0xFFFFFFFF!=S5K3H2YXMIPISensorReg[i].Addr; i++)
    {
        S5K3H2YXMIPI_write_cmos_sensor(S5K3H2YXMIPISensorReg[i].Addr, S5K3H2YXMIPISensorReg[i].Para);
    }
    for(i=ENGINEER_START_ADDR; 0xFFFFFFFF!=S5K3H2YXMIPISensorReg[i].Addr; i++)
    {
        S5K3H2YXMIPI_write_cmos_sensor(S5K3H2YXMIPISensorReg[i].Addr, S5K3H2YXMIPISensorReg[i].Para);
    }
    for(i=FACTORY_START_ADDR+1; i<5; i++)
    {
        S5K3H2YXMIPI_write_cmos_sensor(S5K3H2YXMIPISensorCCT[i].Addr, S5K3H2YXMIPISensorCCT[i].Para);
    }
}



/*************************************************************************
* FUNCTION
*    S5K3H2YXMIPI_sensor_to_camera_para
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
void S5K3H2YXMIPI_sensor_to_camera_para(void)
{
    kal_uint32    i,temp_data;
    for(i=0; 0xFFFFFFFF!=S5K3H2YXMIPISensorReg[i].Addr; i++)
    {
        temp_data = S5K3H2YXMIPI_read_cmos_sensor(S5K3H2YXMIPISensorReg[i].Addr);
        spin_lock(&s5k3h2yxmipiraw_drv_lock);
        S5K3H2YXMIPISensorReg[i].Para = temp_data;
		spin_unlock(&s5k3h2yxmipiraw_drv_lock);
    }
    for(i=ENGINEER_START_ADDR; 0xFFFFFFFF!=S5K3H2YXMIPISensorReg[i].Addr; i++)
    {
        temp_data = S5K3H2YXMIPI_read_cmos_sensor(S5K3H2YXMIPISensorReg[i].Addr);
        spin_lock(&s5k3h2yxmipiraw_drv_lock);
        S5K3H2YXMIPISensorReg[i].Para = temp_data;
		spin_unlock(&s5k3h2yxmipiraw_drv_lock);
    }
}


/*************************************************************************
* FUNCTION
*    S5K3H2YXMIPI_get_sensor_group_count
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
kal_int32  S5K3H2YXMIPI_get_sensor_group_count(void)
{
    return GROUP_TOTAL_NUMS;
}

void S5K3H2YXMIPI_get_sensor_group_info(kal_uint16 group_idx, kal_int8* group_name_ptr, kal_int32* item_count_ptr)
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


void S5K3H2YXMIPI_get_sensor_item_info(kal_uint16 group_idx,kal_uint16 item_idx, MSDK_SENSOR_ITEM_INFO_STRUCT* info_ptr)
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

			temp_para= S5K3H2YXMIPISensorCCT[temp_addr].Para;
			temp_gain= (temp_para/S5K3H2YXMIPI_sensor.sensorBaseGain) * 1000;

			info_ptr->ItemValue=temp_gain;
			info_ptr->IsTrueFalse=KAL_FALSE;
			info_ptr->IsReadOnly=KAL_FALSE;
			info_ptr->IsNeedRestart=KAL_FALSE;
			info_ptr->Min= S5K3H2YX_MIN_ANALOG_GAIN * 1000;
			info_ptr->Max= S5K3H2YX_MAX_ANALOG_GAIN * 1000;
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
					info_ptr->ItemValue=	111;  
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


kal_bool S5K3H2YXMIPI_set_sensor_item_info(kal_uint16 group_idx, kal_uint16 item_idx, kal_int32 ItemValue)
{
//	 kal_int16 temp_reg;
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

		 temp_gain=((ItemValue*BASEGAIN+500)/1000); 		//+500:get closed integer value

		  if(temp_gain>=1*BASEGAIN && temp_gain<=16*BASEGAIN)
		  {
			 temp_para=(temp_gain * S5K3H2YXMIPI_sensor.sensorBaseGain + BASEGAIN/2)/BASEGAIN;
		  } 		 
		  else
			  ASSERT(0);
		  spin_lock(&s5k3h2yxmipiraw_drv_lock);
		  S5K3H2YXMIPISensorCCT[temp_addr].Para = temp_para;
		  spin_unlock(&s5k3h2yxmipiraw_drv_lock);
		  S5K3H2YXMIPI_write_cmos_sensor(S5K3H2YXMIPISensorCCT[temp_addr].Addr,temp_para);
		  spin_lock(&s5k3h2yxmipiraw_drv_lock);
		  S5K3H2YXMIPI_sensor.sensorGlobalGain= S5K3H2YXMIPI_Read_Gain();
		  spin_unlock(&s5k3h2yxmipiraw_drv_lock);

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
					spin_lock(&s5k3h2yxmipiraw_drv_lock);
					S5K3H2YXMIPI_FAC_SENSOR_REG=ItemValue;
					spin_unlock(&s5k3h2yxmipiraw_drv_lock);
					break;
				case 1:
					S5K3H2YXMIPI_write_cmos_sensor(S5K3H2YXMIPI_FAC_SENSOR_REG,ItemValue);
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

static void S5K3H2YXMIPI_SetDummy(const kal_uint16 iPixels, const kal_uint16 iLines)
{
  kal_uint32 frame_length = 0, line_length = 0;


   if(S5K3H2YXMIPI_sensor.sensor_mode == SENSOR_MODE_PREVIEW)
   	{
   	 spin_lock(&s5k3h2yxmipiraw_drv_lock);
   	 S5K3H2YXMIPI_sensor.pv_dummy_pixels = iPixels;
	 S5K3H2YXMIPI_sensor.pv_dummy_lines = iLines;
   	 S5K3H2YXMIPI_sensor.pv_line_length = S5K3H2YXMIPI_PV_LINE_LENGTH_PIXELS + iPixels;
	 S5K3H2YXMIPI_sensor.pv_frame_length = S5K3H2YXMIPI_PV_FRAME_LENGTH_LINES + iLines;
	 spin_unlock(&s5k3h2yxmipiraw_drv_lock);
	 line_length = S5K3H2YXMIPI_sensor.pv_line_length;
	 frame_length = S5K3H2YXMIPI_sensor.pv_frame_length;
	 	
   	}
   else
   	{
   	  spin_lock(&s5k3h2yxmipiraw_drv_lock);
   	  S5K3H2YXMIPI_sensor.cp_dummy_pixels = iPixels;
	  S5K3H2YXMIPI_sensor.cp_dummy_lines = iLines;
	  S5K3H2YXMIPI_sensor.cp_line_length = S5K3H2YXMIPI_FULL_LINE_LENGTH_PIXELS + iPixels;
	  S5K3H2YXMIPI_sensor.cp_frame_length = S5K3H2YXMIPI_FULL_FRAME_LENGTH_LINES + iLines;
	  spin_unlock(&s5k3h2yxmipiraw_drv_lock);
	  line_length = S5K3H2YXMIPI_sensor.cp_line_length;
	  frame_length = S5K3H2YXMIPI_sensor.cp_frame_length;
    }
 
      S5K3H2YXMIPI_write_cmos_sensor(0x0104, 1);        
	  
      S5K3H2YXMIPI_write_cmos_sensor(0x0340, (frame_length >>8) & 0xFF);
      S5K3H2YXMIPI_write_cmos_sensor(0x0341, frame_length & 0xFF);	
      S5K3H2YXMIPI_write_cmos_sensor(0x0342, (line_length >>8) & 0xFF);
      S5K3H2YXMIPI_write_cmos_sensor(0x0343, line_length & 0xFF);

      S5K3H2YXMIPI_write_cmos_sensor(0x0104, 0);

	  SENSORDB("[S5K3H2YXMIPI]%s(),dumy_pixel=%d,dumy_line=%d,\n",__FUNCTION__,iPixels,iLines);
	  SENSORDB("[S5K3H2YXMIPI]sensor_mode=%d,line_length=%d,frame_length=%d,\n",S5K3H2YXMIPI_sensor.sensor_mode,line_length,frame_length);
	  SENSORDB("[S5K3H2YXMIPI]0x340=%x,0x341=%x\n",S5K3H2YXMIPI_read_cmos_sensor(0x0340),S5K3H2YXMIPI_read_cmos_sensor(0x0341));
	  SENSORDB("[S5K3H2YXMIPI]0x342=%x,0x343=%x\n",S5K3H2YXMIPI_read_cmos_sensor(0x0342),S5K3H2YXMIPI_read_cmos_sensor(0x0343));
  
}   /*  S5K3H2YXMIPI_SetDummy */

void S5K3H2YXMIPI_Set_2M_PVsize(void)
{
#if 1
		int i=0;
		SENSORDB("[S5K3H2YXMIPI]%s()\n",__FUNCTION__);

		//○Mode Setting		Readout H:1/2 SubSampling binning, V:1/2 SubSampling binning		30	fps
																						   
		S5K3H2YXMIPI_write_cmos_sensor(0x0100,0x00); // stream off
		S5K3H2YXMIPI_write_cmos_sensor(0x0103,0x01); // software reset	
	    mdelay(20);
		#if 1
		//Flip/Mirror Setting						 
		//Address	Data	Comment 						 
		S5K3H2YXMIPI_write_cmos_sensor(0x0101, 0x00);	//Flip/Mirror ON 0x03	   OFF 0x00
					
		//MIPI Setting								 
		//Address	Data	Comment 	
		#ifdef Using_linestart 
		
		S5K3H2YXMIPI_write_cmos_sensor(0x30a0, 0x0f);
		#endif
		
		S5K3H2YXMIPI_write_cmos_sensor(0x3065, 0x35);		
		S5K3H2YXMIPI_write_cmos_sensor(0x310E, 0x00);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3098, 0xAB);		
		S5K3H2YXMIPI_write_cmos_sensor(0x30C7, 0x0A);		
		S5K3H2YXMIPI_write_cmos_sensor(0x309A, 0x01);		
		S5K3H2YXMIPI_write_cmos_sensor(0x310D, 0xC6);		
		S5K3H2YXMIPI_write_cmos_sensor(0x30c3, 0x40);		
		S5K3H2YXMIPI_write_cmos_sensor(0x30BB, 0x02);//two lane		
		S5K3H2YXMIPI_write_cmos_sensor(0x30BC, 0x38);	//According to MCLK, these registers should be changed.
		S5K3H2YXMIPI_write_cmos_sensor(0x30BD, 0x40);	
		S5K3H2YXMIPI_write_cmos_sensor(0x3110, 0x70);	
		S5K3H2YXMIPI_write_cmos_sensor(0x3111, 0x80);	
		S5K3H2YXMIPI_write_cmos_sensor(0x3112, 0x7B);	
		S5K3H2YXMIPI_write_cmos_sensor(0x3113, 0xC0);	
		S5K3H2YXMIPI_write_cmos_sensor(0x30C7, 0x1A);	
														 
					
		//Manufacture Setting								 
		//Address	Data	Comment 								 
		S5K3H2YXMIPI_write_cmos_sensor(0x3000, 0x08);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3001, 0x05);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3002, 0x0D);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3003, 0x21);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3004, 0x62);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3005, 0x0B);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3006, 0x6D);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3007, 0x02);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3008, 0x62);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3009, 0x62);		
		S5K3H2YXMIPI_write_cmos_sensor(0x300A, 0x41);		
		S5K3H2YXMIPI_write_cmos_sensor(0x300B, 0x10);		
		S5K3H2YXMIPI_write_cmos_sensor(0x300C, 0x21);		
		S5K3H2YXMIPI_write_cmos_sensor(0x300D, 0x04);		
		S5K3H2YXMIPI_write_cmos_sensor(0x307E, 0x03);		
		S5K3H2YXMIPI_write_cmos_sensor(0x307F, 0xA5);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3080, 0x04);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3081, 0x29);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3082, 0x03);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3083, 0x21);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3011, 0x5F);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3156, 0xE2);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3027, 0xBE);		//DBR_CLK enable for EMI	
		S5K3H2YXMIPI_write_cmos_sensor(0x300f, 0x02);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3010, 0x10);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3017, 0x74);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3018, 0x00);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3020, 0x02);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3021, 0x24);		//EMI		
		S5K3H2YXMIPI_write_cmos_sensor(0x3023, 0x80);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3024, 0x08);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3025, 0x08);		
		S5K3H2YXMIPI_write_cmos_sensor(0x301C, 0xD4);		
		S5K3H2YXMIPI_write_cmos_sensor(0x315D, 0x00);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3053, 0xCF);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3054, 0x00);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3055, 0x35);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3062, 0x04);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3063, 0x38);		
		S5K3H2YXMIPI_write_cmos_sensor(0x31A4, 0x04);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3016, 0x54);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3157, 0x02);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3158, 0x00);		
		S5K3H2YXMIPI_write_cmos_sensor(0x315B, 0x02);		
		S5K3H2YXMIPI_write_cmos_sensor(0x315C, 0x00);		
		S5K3H2YXMIPI_write_cmos_sensor(0x301B, 0x05);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3028, 0x41);		
		S5K3H2YXMIPI_write_cmos_sensor(0x302A, 0x00);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3060, 0x00);		
		S5K3H2YXMIPI_write_cmos_sensor(0x302D, 0x19);		
		S5K3H2YXMIPI_write_cmos_sensor(0x302B, 0x05);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3072, 0x13);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3073, 0x21);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3074, 0x82);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3075, 0x20);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3076, 0xA2);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3077, 0x02);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3078, 0x91);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3079, 0x91);		
		S5K3H2YXMIPI_write_cmos_sensor(0x307A, 0x61);		
		S5K3H2YXMIPI_write_cmos_sensor(0x307B, 0x28);		
		S5K3H2YXMIPI_write_cmos_sensor(0x307C, 0x31);		
		
		//black level =64 @ 10bit 
		S5K3H2YXMIPI_write_cmos_sensor(0x304E, 0x40);		//Pedestal
		S5K3H2YXMIPI_write_cmos_sensor(0x304F, 0x01);		//Pedestal
		S5K3H2YXMIPI_write_cmos_sensor(0x3050, 0x00);		//Pedestal
		S5K3H2YXMIPI_write_cmos_sensor(0x3088, 0x01);		//Pedestal
		S5K3H2YXMIPI_write_cmos_sensor(0x3089, 0x00);		//Pedestal
		S5K3H2YXMIPI_write_cmos_sensor(0x3210, 0x01);		//Pedestal
		S5K3H2YXMIPI_write_cmos_sensor(0x3211, 0x00);		//Pedestal
		S5K3H2YXMIPI_write_cmos_sensor(0x30bE, 0x01);		 
		S5K3H2YXMIPI_write_cmos_sensor(0x308F, 0x8F);	
		#endif

		//S5K3H2YXMIPI_write_cmos_sensor(0x0105, 0x01);
		
			//PLL設定		EXCLK 24MHz, vt_pix_clk_freq_mhz=129.6,op_sys_clk_freq_mhz=648			   
			//Address	Data	Comment 																   
		S5K3H2YXMIPI_write_cmos_sensor(0x0305, 0x04);	//pre_pll_clk_div = 4													   
		S5K3H2YXMIPI_write_cmos_sensor(0x0306, 0x00);	//pll_multiplier															   
		S5K3H2YXMIPI_write_cmos_sensor(0x0307, 0x6C);	//pll_multiplier  = 108 												   
		S5K3H2YXMIPI_write_cmos_sensor(0x0303, 0x01);	//vt_sys_clk_div = 1													   
		S5K3H2YXMIPI_write_cmos_sensor(0x0301, 0x05);	//vt_pix_clk_div = 5													   
		S5K3H2YXMIPI_write_cmos_sensor(0x030B, 0x01);	//op_sys_clk_div = 1													   
		S5K3H2YXMIPI_write_cmos_sensor(0x0309, 0x05);	//op_pix_clk_div = 5													   
		S5K3H2YXMIPI_write_cmos_sensor(0x30CC, 0xB0);	//DPHY_band_ctrl 640～690MHz											   
		S5K3H2YXMIPI_write_cmos_sensor(0x31A1, 0x5A);	//EMI control																	   
																					   
		//Readout	H:1/2 SubSampling binning, V:1/2 SubSampling binning						   
		//Address	Data	Comment 																   
		S5K3H2YXMIPI_write_cmos_sensor(0x0344, 0x00);	//X addr start 0d															   
		S5K3H2YXMIPI_write_cmos_sensor(0x0345, 0x00);																		   
		S5K3H2YXMIPI_write_cmos_sensor(0x0346, 0x00);	//Y addr start 0d															   
		S5K3H2YXMIPI_write_cmos_sensor(0x0347, 0x00);																		   
		S5K3H2YXMIPI_write_cmos_sensor(0x0348, 0x0C);	//X addr end 3277d													   
		S5K3H2YXMIPI_write_cmos_sensor(0x0349, 0xCD);																		   
		S5K3H2YXMIPI_write_cmos_sensor(0x034A, 0x09);	//Y addr end 2463d													   
		S5K3H2YXMIPI_write_cmos_sensor(0x034B, 0x9F);																		   
																					   
		S5K3H2YXMIPI_write_cmos_sensor(0x0381, 0x01);	//x_even_inc = 1															   
		S5K3H2YXMIPI_write_cmos_sensor(0x0383, 0x03);	//x_odd_inc = 3 														   
		S5K3H2YXMIPI_write_cmos_sensor(0x0385, 0x01);	//y_even_inc = 1															   
		S5K3H2YXMIPI_write_cmos_sensor(0x0387, 0x03);	//y_odd_inc = 3 														   
																					   
		S5K3H2YXMIPI_write_cmos_sensor(0x0401, 0x00);	//Derating_en  = 0 (disable)											   
		S5K3H2YXMIPI_write_cmos_sensor(0x0405, 0x10);																		   
		S5K3H2YXMIPI_write_cmos_sensor(0x0700, 0x05);	//fifo_water_mark_pixels = 1328 										   
		S5K3H2YXMIPI_write_cmos_sensor(0x0701, 0x30);																		   
																					   
		S5K3H2YXMIPI_write_cmos_sensor(0x034C, 0x06);	//x_output_size = 1640													   
		S5K3H2YXMIPI_write_cmos_sensor(0x034D, 0x68);																		   
		S5K3H2YXMIPI_write_cmos_sensor(0x034E, 0x04);	//y_output_size = 1232													   
		S5K3H2YXMIPI_write_cmos_sensor(0x034F, 0xD0);																		   
																					   
		S5K3H2YXMIPI_write_cmos_sensor(0x0200, 0x02);	//fine integration time 												   
		S5K3H2YXMIPI_write_cmos_sensor(0x0201, 0x50);																		   
		//S5K3H2YXMIPI_write_cmos_sensor(0x0202, 0x04);	//Coarse integration time													   
		//S5K3H2YXMIPI_write_cmos_sensor(0x0203, 0xC0);																		   
		//S5K3H2YXMIPI_write_cmos_sensor(0x0204, 0x00);	//Analog gain															   
		//S5K3H2YXMIPI_write_cmos_sensor(0x0205, 0x20);		
#ifdef Using_linestart
		S5K3H2YXMIPI_write_cmos_sensor(0x0342, 0x0E);	//Line_length_pck 3470d 												   
		S5K3H2YXMIPI_write_cmos_sensor(0x0343, 0x20);
#else
		S5K3H2YXMIPI_write_cmos_sensor(0x0342, 0x0D);	//Line_length_pck 3470d 												   
		S5K3H2YXMIPI_write_cmos_sensor(0x0343, 0x8E);	

#endif
		S5K3H2YXMIPI_write_cmos_sensor(0x0340, 0x04);	//Frame_length_lines 1248d											   
		S5K3H2YXMIPI_write_cmos_sensor(0x0341, 0xEC);		//E0															   
		//S5K3H2YXMIPI_write_cmos_sensor(0x0340, 0x04);	//Frame_length_lines 1244d											   
		//S5K3H2YXMIPI_write_cmos_sensor(0x0341, 0xDC);																					   
		//Manufacture Setting															   
		//Address	Data	Comment 																   
		S5K3H2YXMIPI_write_cmos_sensor(0x300E, 0x2D);																		   
		S5K3H2YXMIPI_write_cmos_sensor(0x31A3, 0x40);																		   
		S5K3H2YXMIPI_write_cmos_sensor(0x301A, 0x77);		
												   
		S5K3H2YXMIPI_write_cmos_sensor(0x0100, 0x01);// stream on									   
#else
     int i=0;
		SENSORDB("[S5K3H2YXMIPI]%s()\n",__FUNCTION__);

		//○Mode Setting		Readout H:1/2 SubSampling binning, V:1/2 SubSampling binning		30	fps
																						   
		S5K3H2YXMIPI_write_cmos_sensor(0x0100,0x00); // stream off
		S5K3H2YXMIPI_write_cmos_sensor(0x0103,0x01); // software reset	
	    mdelay(20);
		#if 1
		//Flip/Mirror Setting						 
		//Address	Data	Comment 						 
		S5K3H2YXMIPI_write_cmos_sensor(0x0101, 0x00);	//Flip/Mirror ON 0x03	   OFF 0x00
					
		//MIPI Setting								 
		//Address	Data	Comment 	
		#ifdef Using_linestart 
		
		S5K3H2YXMIPI_write_cmos_sensor(0x30a0, 0x0f);
		#endif
		
		S5K3H2YXMIPI_write_cmos_sensor(0x3065, 0x35);		
		S5K3H2YXMIPI_write_cmos_sensor(0x310E, 0x00);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3098, 0xAB);		
		S5K3H2YXMIPI_write_cmos_sensor(0x30C7, 0x0A);		
		S5K3H2YXMIPI_write_cmos_sensor(0x309A, 0x01);		
		S5K3H2YXMIPI_write_cmos_sensor(0x310D, 0xC6);		
		S5K3H2YXMIPI_write_cmos_sensor(0x30c3, 0x40);		
		S5K3H2YXMIPI_write_cmos_sensor(0x30BB, 0x02);//two lane		
		S5K3H2YXMIPI_write_cmos_sensor(0x30BC, 0x38);	//According to MCLK, these registers should be changed.
		S5K3H2YXMIPI_write_cmos_sensor(0x30BD, 0x40);	
		S5K3H2YXMIPI_write_cmos_sensor(0x3110, 0x70);	
		S5K3H2YXMIPI_write_cmos_sensor(0x3111, 0x80);	
		S5K3H2YXMIPI_write_cmos_sensor(0x3112, 0x7B);	
		S5K3H2YXMIPI_write_cmos_sensor(0x3113, 0xC0);	
		S5K3H2YXMIPI_write_cmos_sensor(0x30C7, 0x1A);	
														 
					
		//Manufacture Setting								 
		//Address	Data	Comment 
		S5K3H2YXMIPI_write_cmos_sensor(0x308E, 0x01);		
		S5K3H2YXMIPI_write_cmos_sensor(0x308F, 0x8F);	
				//S5K3H2YXMIPI_write_cmos_sensor(0x0105, 0x01);
		
			//PLL設定		EXCLK 24MHz, vt_pix_clk_freq_mhz=129.6,op_sys_clk_freq_mhz=648			   
			//Address	Data	Comment 																   
		S5K3H2YXMIPI_write_cmos_sensor(0x0305, 0x04);	//pre_pll_clk_div = 4													   
		S5K3H2YXMIPI_write_cmos_sensor(0x0306, 0x00);	//pll_multiplier															   
		S5K3H2YXMIPI_write_cmos_sensor(0x0307, 0x6C);	//pll_multiplier  = 108 												   
		S5K3H2YXMIPI_write_cmos_sensor(0x0303, 0x01);	//vt_sys_clk_div = 1													   
		S5K3H2YXMIPI_write_cmos_sensor(0x0301, 0x05);	//vt_pix_clk_div = 5													   
		S5K3H2YXMIPI_write_cmos_sensor(0x030B, 0x01);	//op_sys_clk_div = 1													   
		S5K3H2YXMIPI_write_cmos_sensor(0x0309, 0x05);	//op_pix_clk_div = 5													   
		S5K3H2YXMIPI_write_cmos_sensor(0x31A1, 0x5A);	//EMI control																	   
		S5K3H2YXMIPI_write_cmos_sensor(0x30CC, 0xB0);	//DPHY_band_ctrl 640～690MHz											   
	//	S5K3H2YXMIPI_write_cmos_sensor(0x0344, 0x00);	//X addr start 0d															   
	//	S5K3H2YXMIPI_write_cmos_sensor(0x0345, 0x00);																		   
	//	S5K3H2YXMIPI_write_cmos_sensor(0x0346, 0x00);	//Y addr start 0d															   
	//	S5K3H2YXMIPI_write_cmos_sensor(0x0347, 0x00);																		   
	//	S5K3H2YXMIPI_write_cmos_sensor(0x0348, 0x0C);	//X addr end 3277d													   
		S5K3H2YXMIPI_write_cmos_sensor(0x0349, 0xCF);																		   
	//	S5K3H2YXMIPI_write_cmos_sensor(0x034A, 0x09);	//Y addr end 2463d													   
	//	S5K3H2YXMIPI_write_cmos_sensor(0x034B, 0x9F);																		   
																					   
		S5K3H2YXMIPI_write_cmos_sensor(0x0381, 0x01);	//x_even_inc = 1															   
		S5K3H2YXMIPI_write_cmos_sensor(0x0383, 0x03);	//x_odd_inc = 3 														   
		S5K3H2YXMIPI_write_cmos_sensor(0x0385, 0x01);	//y_even_inc = 1															   
		S5K3H2YXMIPI_write_cmos_sensor(0x0387, 0x03);	//y_odd_inc = 3 														   
																					   
		S5K3H2YXMIPI_write_cmos_sensor(0x0401, 0x00);	//Derating_en  = 0 (disable)											   
		S5K3H2YXMIPI_write_cmos_sensor(0x0405, 0x1B);																		   
		S5K3H2YXMIPI_write_cmos_sensor(0x0700, 0x05);	//fifo_water_mark_pixels = 1328 										   
		S5K3H2YXMIPI_write_cmos_sensor(0x0701, 0xCE);																		   
		S5K3H2YXMIPI_write_cmos_sensor(0x034C, 0x06);	//x_output_size = 1640													   
		S5K3H2YXMIPI_write_cmos_sensor(0x034D, 0x60);																		   
		S5K3H2YXMIPI_write_cmos_sensor(0x034E, 0x04);	//y_output_size = 1232													   
		S5K3H2YXMIPI_write_cmos_sensor(0x034F, 0xC8);
    	S5K3H2YXMIPI_write_cmos_sensor(0x3000, 0x08);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3001, 0x05);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3002, 0x0D);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3003, 0x21);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3004, 0x62);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3005, 0x0B);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3006, 0x6D);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3007, 0x02);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3008, 0x62);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3009, 0x62);		
		S5K3H2YXMIPI_write_cmos_sensor(0x300A, 0x41);		
		S5K3H2YXMIPI_write_cmos_sensor(0x300B, 0x10);		
		S5K3H2YXMIPI_write_cmos_sensor(0x300C, 0x21);		
		S5K3H2YXMIPI_write_cmos_sensor(0x300D, 0x04);		
		S5K3H2YXMIPI_write_cmos_sensor(0x307E, 0x03);		
		S5K3H2YXMIPI_write_cmos_sensor(0x307F, 0xA5);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3080, 0x04);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3081, 0x29);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3082, 0x03);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3083, 0x21);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3011, 0x5F);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3156, 0xE2);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3027, 0xBE);		//DBR_CLK enable for EMI	
		S5K3H2YXMIPI_write_cmos_sensor(0x300f, 0x02);	
		S5K3H2YXMIPI_write_cmos_sensor(0x3065, 0x35);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3072, 0x13);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3073, 0x21);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3074, 0x82);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3075, 0x20);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3076, 0xA2);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3077, 0x02);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3078, 0x91);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3079, 0x91);		
		S5K3H2YXMIPI_write_cmos_sensor(0x307A, 0x61);		
		S5K3H2YXMIPI_write_cmos_sensor(0x307B, 0x28);		
		S5K3H2YXMIPI_write_cmos_sensor(0x307C, 0x31);
		S5K3H2YXMIPI_write_cmos_sensor(0x3010, 0x10);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3017, 0x74);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3018, 0x00);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3020, 0x02);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3021, 0x24);		//EMI		
		S5K3H2YXMIPI_write_cmos_sensor(0x3023, 0x80);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3024, 0x08);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3025, 0x08);		
		S5K3H2YXMIPI_write_cmos_sensor(0x301C, 0xD4);		
		S5K3H2YXMIPI_write_cmos_sensor(0x315D, 0x00);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3053, 0xCF);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3054, 0x00);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3055, 0x35);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3062, 0x04);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3063, 0x38);		
		S5K3H2YXMIPI_write_cmos_sensor(0x31A4, 0x04);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3016, 0x54);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3157, 0x02);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3158, 0x00);		
		S5K3H2YXMIPI_write_cmos_sensor(0x315B, 0x02);		
		S5K3H2YXMIPI_write_cmos_sensor(0x315C, 0x00);		
		S5K3H2YXMIPI_write_cmos_sensor(0x301B, 0x05);
		S5K3H2YXMIPI_write_cmos_sensor(0x301A, 0x77);
		S5K3H2YXMIPI_write_cmos_sensor(0x3028, 0x41);		
		S5K3H2YXMIPI_write_cmos_sensor(0x302A, 0x00);		
		S5K3H2YXMIPI_write_cmos_sensor(0x3060, 0x00);
		S5K3H2YXMIPI_write_cmos_sensor(0x300E, 0x2D);		
		S5K3H2YXMIPI_write_cmos_sensor(0x31A3, 0x40);
		S5K3H2YXMIPI_write_cmos_sensor(0x302D, 0x19);
		S5K3H2YXMIPI_write_cmos_sensor(0x3164, 0x03);

		S5K3H2YXMIPI_write_cmos_sensor(0x31A7, 0x0F);
		//black level =64 @ 10bit 
		#endif

																				   
		//Readout	H:1/2 SubSampling binning, V:1/2 SubSampling binning						   
		//Address	Data	Comment 																   
																		   
																					   
		S5K3H2YXMIPI_write_cmos_sensor(0x0200, 0x02);	//fine integration time 												   
		S5K3H2YXMIPI_write_cmos_sensor(0x0201, 0x50);																		   
		//S5K3H2YXMIPI_write_cmos_sensor(0x0202, 0x04);	//Coarse integration time													   
		//S5K3H2YXMIPI_write_cmos_sensor(0x0203, 0xC0);																		   
		//S5K3H2YXMIPI_write_cmos_sensor(0x0204, 0x00);	//Analog gain															   
		//S5K3H2YXMIPI_write_cmos_sensor(0x0205, 0x20);		
#ifdef Using_linestart
		S5K3H2YXMIPI_write_cmos_sensor(0x0342, 0x0E);	//Line_length_pck 3470d 												   
		S5K3H2YXMIPI_write_cmos_sensor(0x0343, 0x20);
#else
		S5K3H2YXMIPI_write_cmos_sensor(0x0342, 0x0D);	//Line_length_pck 3470d 												   
		S5K3H2YXMIPI_write_cmos_sensor(0x0343, 0x8E);	

#endif
		S5K3H2YXMIPI_write_cmos_sensor(0x0340, 0x04);	//Frame_length_lines 1248d											   
		S5K3H2YXMIPI_write_cmos_sensor(0x0341, 0xE0);																		   
		
												   
		S5K3H2YXMIPI_write_cmos_sensor(0x0100, 0x01);// stream on  
#endif
	
}


void S5K3H2YXMIPI_set_8M_FullSize(void)
{	
       SENSORDB("[S5K3H2YXMIPI]%s()\n",__FUNCTION__);
	   //○Mode Setting 	   Readout Full 	   15  fps 
	   S5K3H2YXMIPI_write_cmos_sensor(0x0100,0x00); // stream off
	   S5K3H2YXMIPI_write_cmos_sensor(0x0103,0x01); // software reset	
	    mdelay(20);
	   #if 1
	   //Flip/Mirror Setting						
	   //Address   Data    Comment							
	   S5K3H2YXMIPI_write_cmos_sensor(0x0101, 0x00);   //Flip/Mirror ON 0x03	  OFF 0x00
				   
	   //MIPI Setting								
	   //Address   Data    Comment		
	   #ifdef Using_linestart
	   S5K3H2YXMIPI_write_cmos_sensor(0x30a0, 0x0f);
	   #endif
	   S5K3H2YXMIPI_write_cmos_sensor(0x3065, 0x35);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x310E, 0x00);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3098, 0xAB);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x30C7, 0x0A);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x309A, 0x01);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x310D, 0xC6);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x30c3, 0x40);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x30BB, 0x02);//two lane	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x30BC, 0x38);   //According to MCLK, these registers should be changed.
	   S5K3H2YXMIPI_write_cmos_sensor(0x30BD, 0x40);   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3110, 0x70);   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3111, 0x80);   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3112, 0x7B);   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3113, 0xC0);   
	   S5K3H2YXMIPI_write_cmos_sensor(0x30C7, 0x1A);   
														
				   
	   //Manufacture Setting								
	   //Address   Data    Comment									
	   S5K3H2YXMIPI_write_cmos_sensor(0x3000, 0x08);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3001, 0x05);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3002, 0x0D);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3003, 0x21);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3004, 0x62);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3005, 0x0B);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3006, 0x6D);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3007, 0x02);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3008, 0x62);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3009, 0x62);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x300A, 0x41);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x300B, 0x10);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x300C, 0x21);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x300D, 0x04);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x307E, 0x03);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x307F, 0xA5);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3080, 0x04);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3081, 0x29);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3082, 0x03);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3083, 0x21);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3011, 0x5F);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3156, 0xE2);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3027, 0xBE);	   //DBR_CLK enable for EMI    
	   S5K3H2YXMIPI_write_cmos_sensor(0x300f, 0x02);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3010, 0x10);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3017, 0x74);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3018, 0x00);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3020, 0x02);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3021, 0x00);	   //EMI	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3023, 0x80);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3024, 0x08);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3025, 0x08);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x301C, 0xD4);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x315D, 0x00);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3053, 0xCF);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3054, 0x00);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3055, 0x35);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3062, 0x04);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3063, 0x38);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x31A4, 0x04);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3016, 0x54);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3157, 0x02);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3158, 0x00);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x315B, 0x02);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x315C, 0x00);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x301B, 0x05);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3028, 0x41);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x302A, 0x00);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3060, 0x00);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x302D, 0x19);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x302B, 0x05);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3072, 0x13);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3073, 0x21);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3074, 0x82);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3075, 0x20);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3076, 0xA2);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3077, 0x02);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3078, 0x91);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x3079, 0x91);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x307A, 0x61);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x307B, 0x28);	   
	   S5K3H2YXMIPI_write_cmos_sensor(0x307C, 0x31);	   
	   
	   //black level =64 @ 10bit 
	   S5K3H2YXMIPI_write_cmos_sensor(0x304E, 0x40);	   //Pedestal
	   S5K3H2YXMIPI_write_cmos_sensor(0x304F, 0x01);	   //Pedestal
	   S5K3H2YXMIPI_write_cmos_sensor(0x3050, 0x00);	   //Pedestal
	   S5K3H2YXMIPI_write_cmos_sensor(0x3088, 0x01);	   //Pedestal
	   S5K3H2YXMIPI_write_cmos_sensor(0x3089, 0x00);	   //Pedestal
	   S5K3H2YXMIPI_write_cmos_sensor(0x3210, 0x01);	   //Pedestal
	   S5K3H2YXMIPI_write_cmos_sensor(0x3211, 0x00);	   //Pedestal
	   S5K3H2YXMIPI_write_cmos_sensor(0x30bE, 0x01);		
	   S5K3H2YXMIPI_write_cmos_sensor(0x308F, 0x8F);   
	   

	   #endif
							   
	   //PLL設定	   EXCLK 24MHz, vt_pix_clk_freq_mhz=129.6,op_sys_clk_freq_mhz=648			   
	   //Address   Data    Comment			   
	   S5K3H2YXMIPI_write_cmos_sensor(0x0305, 0x04);   //pre_pll_clk_div = 4			   
	   S5K3H2YXMIPI_write_cmos_sensor(0x0306, 0x00);   //pll_multiplier 			   
	   S5K3H2YXMIPI_write_cmos_sensor(0x0307, 0x6C);   //pll_multiplier  = 108			   
	   S5K3H2YXMIPI_write_cmos_sensor(0x0303, 0x01);   //vt_sys_clk_div = 1 			   
	   S5K3H2YXMIPI_write_cmos_sensor(0x0301, 0x05);   //vt_pix_clk_div = 5 			   
	   S5K3H2YXMIPI_write_cmos_sensor(0x030B, 0x01);   //op_sys_clk_div = 1 			   
	   S5K3H2YXMIPI_write_cmos_sensor(0x0309, 0x05);   //op_pix_clk_div = 5 			   
	   S5K3H2YXMIPI_write_cmos_sensor(0x30CC, 0xB0);   //DPHY_band_ctrl 640～690MHz 			   
	   S5K3H2YXMIPI_write_cmos_sensor(0x31A1, 0x58);   //EMI control				   
							   
	   //Readout   Full 				   
	   //Address   Data    Comment			   
	   S5K3H2YXMIPI_write_cmos_sensor(0x0344, 0x00);	   //X addr start 0d			   
	   S5K3H2YXMIPI_write_cmos_sensor(0x0345, 0x00);				   
	   S5K3H2YXMIPI_write_cmos_sensor(0x0346, 0x00);   //Y addr start 0d			   
	   S5K3H2YXMIPI_write_cmos_sensor(0x0347, 0x00);				   
	   S5K3H2YXMIPI_write_cmos_sensor(0x0348, 0x0C);	   //X addr end 3279d			   
	   S5K3H2YXMIPI_write_cmos_sensor(0x0349, 0xCF);				   
	   S5K3H2YXMIPI_write_cmos_sensor(0x034A, 0x09);   //Y addr end 2463d			   
	   S5K3H2YXMIPI_write_cmos_sensor(0x034B, 0x9F);				   
							   
	   S5K3H2YXMIPI_write_cmos_sensor(0x0381, 0x01);	//x_even_inc = 1			   
	   S5K3H2YXMIPI_write_cmos_sensor(0x0383, 0x01);  //x_odd_inc = 1			   
	   S5K3H2YXMIPI_write_cmos_sensor(0x0385, 0x01);  //y_even_inc = 1			   
	   S5K3H2YXMIPI_write_cmos_sensor(0x0387, 0x01);  //y_odd_inc = 1			   
							   
	   S5K3H2YXMIPI_write_cmos_sensor(0x0401, 0x00);	   //Derating_en  = 0 (disable) 			   
	   S5K3H2YXMIPI_write_cmos_sensor(0x0405, 0x10);				   
	   S5K3H2YXMIPI_write_cmos_sensor(0x0700, 0x05);   //fifo_water_mark_pixels = 1328			   
	   S5K3H2YXMIPI_write_cmos_sensor(0x0701, 0x30);				   
							   
	   S5K3H2YXMIPI_write_cmos_sensor(0x034C, 0x0C);	   //x_output_size = 3280			   
	   S5K3H2YXMIPI_write_cmos_sensor(0x034D, 0xD0);				   
	   S5K3H2YXMIPI_write_cmos_sensor(0x034E, 0x09);   //y_output_size = 2464			   
	   S5K3H2YXMIPI_write_cmos_sensor(0x034F, 0xA0);				   
							   
	   S5K3H2YXMIPI_write_cmos_sensor(0x0200, 0x02);	   //fine integration time			   
	   S5K3H2YXMIPI_write_cmos_sensor(0x0201, 0x50);				   
	   S5K3H2YXMIPI_write_cmos_sensor(0x0202, 0x04);   //Coarse integration time			   
	   S5K3H2YXMIPI_write_cmos_sensor(0x0203, 0xE7);				   
	   S5K3H2YXMIPI_write_cmos_sensor(0x0204, 0x00);	   //Analog gain			   
	   S5K3H2YXMIPI_write_cmos_sensor(0x0205, 0x20);				   
#ifdef Using_linestart
		S5K3H2YXMIPI_write_cmos_sensor(0x0342, 0x0E);	//Line_length_pck 3470d 												   
		S5K3H2YXMIPI_write_cmos_sensor(0x0343, 0x20);
#else
		S5K3H2YXMIPI_write_cmos_sensor(0x0342, 0x0D);	//Line_length_pck 3470d 												   
		S5K3H2YXMIPI_write_cmos_sensor(0x0343, 0x8E);	

#endif				   
	   S5K3H2YXMIPI_write_cmos_sensor(0x0340, 0x09); //Frame_length_lines 2480d			   
	   S5K3H2YXMIPI_write_cmos_sensor(0x0341, 0xB0);				   
							   
	   //Manufacture Setting					   
	   //Address   Data    Comment			   
	   S5K3H2YXMIPI_write_cmos_sensor(0x300E, 0x29);					   
	   S5K3H2YXMIPI_write_cmos_sensor(0x31A3, 0x00);				   
	   S5K3H2YXMIPI_write_cmos_sensor(0x301A, 0x77);				   
												   
	   S5K3H2YXMIPI_write_cmos_sensor(0x0100, 0x01);// stream on								

   
}



static kal_uint16 S5K3H2YXMIPI_power_on(void)
{
    kal_uint8 i;
	kal_uint16 S5K3H2YXMIPI_sensor_id = 0;
	
    SENSORDB("[S5K3H2YXMIPI]%s()\n",__FUNCTION__);
	
	S5K3H2YXMIPI_sensor_id = ((S5K3H2YXMIPI_read_cmos_sensor(0x0000) << 8) | S5K3H2YXMIPI_read_cmos_sensor(0x0001));
    SENSORDB("[S5K3H2YXMIPI]read Sensor_ID = %x\n",S5K3H2YXMIPI_sensor_id);
	
	return S5K3H2YXMIPI_sensor_id;
}
static void S5K3H2YXMIPI_Init_Parameter(void)
{
	spin_lock(&s5k3h2yxmipiraw_drv_lock);

    S5K3H2YXMIPI_sensor.sensor_mode = SENSOR_MODE_INIT;
	S5K3H2YXMIPI_sensor.first_init = KAL_TRUE;
    S5K3H2YXMIPI_sensor.night_mode = KAL_FALSE;
    S5K3H2YXMIPI_sensor.cp_pclk = 129600000;//135200000;//114400000;  
    S5K3H2YXMIPI_sensor.pv_pclk = 129600000;//135200000;  
    S5K3H2YXMIPI_sensor.pv_dummy_pixels = 0;
    S5K3H2YXMIPI_sensor.pv_dummy_lines = 0;
	S5K3H2YXMIPI_sensor.cp_dummy_pixels = 0;
	S5K3H2YXMIPI_sensor.cp_dummy_lines = 0;
	S5K3H2YXMIPI_sensor.pv_line_length = S5K3H2YXMIPI_PV_LINE_LENGTH_PIXELS + S5K3H2YXMIPI_sensor.pv_dummy_pixels;
	S5K3H2YXMIPI_sensor.pv_frame_length = S5K3H2YXMIPI_PV_FRAME_LENGTH_LINES + S5K3H2YXMIPI_sensor.pv_dummy_lines;
	S5K3H2YXMIPI_sensor.cp_line_length = S5K3H2YXMIPI_FULL_LINE_LENGTH_PIXELS + S5K3H2YXMIPI_sensor.cp_dummy_pixels;
	S5K3H2YXMIPI_sensor.cp_frame_length = S5K3H2YXMIPI_FULL_FRAME_LENGTH_LINES+ S5K3H2YXMIPI_sensor.cp_dummy_lines;
    S5K3H2YXMIPI_sensor.mirror_flip = 0;  //normal:0, mirror:1, flip: 2, mirror_flip:3
    S5K3H2YXMIPI_sensor.ispBaseGain = 64;
	S5K3H2YXMIPI_sensor.sensorBaseGain = 32;
	S5K3H2YXMIPI_sensor.sensorGlobalGain = (4 * S5K3H2YXMIPI_sensor.sensorBaseGain);//32;
	S5K3H2YXMIPI_sensor.bAutoFlickerMode=KAL_FALSE;
	S5K3H2YXMIPI_sensor.IsVideoNightMode=KAL_FALSE;
	S5K3H2YXMIPI_sensor.VideoMode = KAL_FALSE;
	spin_unlock(&s5k3h2yxmipiraw_drv_lock);
   // S5K3H2YXMIPI_sensor.pv_shutter = 0x200;
    
}


UINT32 S5K3H2YXMIPIGetDefaultFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId,MUINT32 *pframeRate)
{
	switch(scenarioId)
	{
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*pframeRate=300;
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
			*pframeRate=150;
			break;
		case MSDK_SCENARIO_ID_CAMERA_3D_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_3D_VIDEO:
		case MSDK_SCENARIO_ID_CAMERA_3D_CAPTURE:
			*pframeRate=300;
			break;
		default:
			break;
		
	}
	return ERROR_NONE;
}


UINT32 S5K3H2YXMIPISetMaxFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 frameRate)
{
	kal_int16 dummyLine=0;
	SENSORDB("S5K3H2YXMIPISetMaxFramerateByScenario scenarioId=%d,frameRate=%d",scenarioId,frameRate);
	switch(scenarioId)
	{
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			dummyLine=10*S5K3H2YXMIPI_sensor.pv_pclk/frameRate/S5K3H2YXMIPI_sensor.pv_line_length-S5K3H2YXMIPI_PV_FRAME_LENGTH_LINES;
			if(dummyLine<=0)
				dummyLine=0;
			S5K3H2YXMIPI_SetDummy(0,dummyLine);
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
			dummyLine=10*S5K3H2YXMIPI_sensor.cp_pclk/frameRate/S5K3H2YXMIPI_sensor.cp_line_length-S5K3H2YXMIPI_FULL_FRAME_LENGTH_LINES;
			if(dummyLine<=0)
				dummyLine=0;
			S5K3H2YXMIPI_SetDummy(0,dummyLine);
			
			break;			
		case MSDK_SCENARIO_ID_CAMERA_3D_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_3D_VIDEO:
		case MSDK_SCENARIO_ID_CAMERA_3D_CAPTURE:
			break;
	}
	return ERROR_NONE;
}








/*****************************************************************************/
/* Windows Mobile Sensor Interface */
/*****************************************************************************/
/*************************************************************************
* FUNCTION
*   S5K3H2YXMIPIOpen
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

UINT32 S5K3H2YXMIPIOpen(void)
{
    SENSORDB("[S5K3H2YXMIPI]%s()\n",__FUNCTION__);
	
	S5K3H2YXMIPI_write_cmos_sensor(0x0103,0x01);// Reset sensor
    mDELAY(10);
  
   if (S5K3H2YXMIPI_power_on() != S5K3H2YXMIPI_SENSOR_ID) 
   {
	 SENSORDB("[S5K3H2YXMIPI]Error:read sensor ID fail\n");
	 return ERROR_SENSOR_CONNECT_FAIL;
   }
 
   S5K3H2YXMIPI_Init_Parameter();

  return ERROR_NONE;

}

/*************************************************************************
* FUNCTION
*   S5K3H2YXMIPIGetSensorID
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
UINT32 S5K3H2YXMIPIGetSensorID(UINT32 *sensorID) 
{
    //int  retry = 3; 
	SENSORDB("[S5K3H2YXMIPI]%s()\n",__FUNCTION__);

	S5K3H2YXMIPI_write_cmos_sensor(0x0103,0x01);// Reset sensor
    mDELAY(10);

    // check if sensor ID correct
    #if 0
    do {
        *sensorID = S5K3H2YXMIPI_power_on();         
        if (*sensorID == S5K3H2YXMIPI_SENSOR_ID)
            break; 
        SENSORDB("Read Sensor ID Fail = 0x%04x\n", *sensorID); 
        retry--; 
    } while (retry > 0);
	#else
	*sensorID = S5K3H2YXMIPI_power_on();  
	SENSORDB("S5K3H2YXMIPIGetSensorID = 0x%04x\n", *sensorID); 
    #endif
    if (*sensorID != S5K3H2YXMIPI_SENSOR_ID) {
        *sensorID = 0xFFFFFFFF; 
        return ERROR_SENSOR_CONNECT_FAIL;
    }
    return ERROR_NONE;

}


/*************************************************************************
* FUNCTION
*   S5K3H2YXMIPI_SetShutter
*
* DESCRIPTION
*   This function set e-shutter of S5K3H2YXMIPI to change exposure time.
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
void S5K3H2YXMIPI_SetShutter(kal_uint16 iShutter)
{
	unsigned long flags;
   
    if (iShutter < 1)
        iShutter = 1; 
	else if(iShutter > 0xffff)
		iShutter = 0xffff;
	spin_lock_irqsave(&s5k3h2yxmipiraw_drv_lock,flags);
    S5K3H2YXMIPI_sensor.pv_shutter = iShutter;
	spin_unlock_irqrestore(&s5k3h2yxmipiraw_drv_lock,flags);

    S5K3H2YXMIPI_write_shutter(iShutter);
}   /*  S5K3H2YXMIPI_SetShutter   */



/*************************************************************************
* FUNCTION
*   S5K3H2YXMIPI_read_shutter
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
UINT16 S5K3H2YXMIPI_read_shutter(void)
{
	//SENSORDB("[S5K3H2YXMIPI]%s()\n",__FUNCTION__);
	//return;
    UINT16 shutter = 0;

	shutter = ((S5K3H2YXMIPI_read_cmos_sensor(0x0202)<<8) & 0xFF00) | (S5K3H2YXMIPI_read_cmos_sensor(0x0203)&0xFF);
	SENSORDB(" read shutter value is %d",shutter);
	return shutter;

    //return (UINT16)( (S5K3H2YXMIPI_read_cmos_sensor(0x0202)<<8) | S5K3H2YXMIPI_read_cmos_sensor(0x0203) );
}

/*************************************************************************
* FUNCTION
*   S5K3H2YXMIPI_night_mode
*
* DESCRIPTION
*   This function night mode of S5K3H2YXMIPI.
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
void S5K3H2YXMIPI_NightMode(kal_bool bEnable)
{
	SENSORDB("[S5K3H2YXMIPI]%s():enable=%d\n",__FUNCTION__,bEnable);

}/*	S5K3H2YXMIPI_NightMode */



/*************************************************************************
* FUNCTION
*   S5K3H2YXMIPIClose
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
/*3H2 power down flow:sw standby(stream off)->delay 1 frame->hw stanby(pwdn/reset pin to low)*/
UINT32 S5K3H2YXMIPIClose(void)
{
	kal_uint8 temp;
	SENSORDB("[S5K3H2YXMIPI]%s()\n",__FUNCTION__);

    S5K3H2YXMIPI_write_cmos_sensor(0x0100,0x00); // stream off
    mDELAY(30);//should delay about 1 frame for power down
		 	
	S5K3H2YXMIPI_write_cmos_sensor(0x0103,0x01); // software reset
	mDELAY(10);

	temp = S5K3H2YXMIPI_read_cmos_sensor(0x0005);//if temp= 255,it means power down successful,or you need  increase delay time after stream off
	SENSORDB("[S5K3H2YXMIPI]%s():temp=%d\n",__FUNCTION__,temp);
    return ERROR_NONE;
}	/* S5K3H2YXMIPIClose() */

void S5K3H2YXMIPISetFlipMirror(kal_int32 imgMirror)
{
    kal_uint8  iTemp; 

     SENSORDB("[S5K3H2YXMIPI]%s():mirror_flip=%d\n",__FUNCTION__,imgMirror);
	spin_lock(&s5k3h2yxmipiraw_drv_lock); 
	S5K3H2YXMIPI_sensor.mirror_flip = imgMirror;
	spin_unlock(&s5k3h2yxmipiraw_drv_lock);   
    iTemp = S5K3H2YXMIPI_read_cmos_sensor(0x0101) & 0x03;	//Clear the mirror and flip bits.
    switch (imgMirror)
    {
        case IMAGE_NORMAL:
            S5K3H2YXMIPI_write_cmos_sensor(0x0101, 0x00);	//Set normal
            break;
        case IMAGE_V_MIRROR:
            S5K3H2YXMIPI_write_cmos_sensor(0x0101, iTemp | 0x02);	//Set flip
            break;
        case IMAGE_H_MIRROR:
            S5K3H2YXMIPI_write_cmos_sensor(0x0101, iTemp | 0x01);	//Set mirror
            break;
        case IMAGE_HV_MIRROR:
            S5K3H2YXMIPI_write_cmos_sensor(0x0101, 0x03);	//Set mirror and flip
            break;
    }
}


/*************************************************************************
* FUNCTION
*   S5K3H2YXMIPIPreview
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
UINT32 S5K3H2YXMIPIPreview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                                                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	//kal_uint8 temp;

   
   SENSORDB("[S5K3H2YXMIPI]%s()..................................................\n",__FUNCTION__);
   	//set mirror & flip
   	spin_lock(&s5k3h2yxmipiraw_drv_lock); 
	S5K3H2YXMIPI_sensor.mirror_flip = sensor_config_data->SensorImageMirror;
	spin_unlock(&s5k3h2yxmipiraw_drv_lock); 

	 
     if(S5K3H2YXMIPI_sensor.sensor_mode != SENSOR_MODE_PREVIEW)
     {
	  S5K3H2YXMIPI_Set_2M_PVsize();   
	  spin_lock(&s5k3h2yxmipiraw_drv_lock);
	  S5K3H2YXMIPI_sensor.pv_dummy_pixels = 0;
	  S5K3H2YXMIPI_sensor.pv_dummy_lines = 0;
	  spin_unlock(&s5k3h2yxmipiraw_drv_lock); 
	  S5K3H2YXMIPI_SetDummy(S5K3H2YXMIPI_sensor.pv_dummy_pixels,S5K3H2YXMIPI_sensor.pv_dummy_lines);
	 }
	 S5K3H2YXMIPISetFlipMirror(sensor_config_data->SensorImageMirror);
	 spin_lock(&s5k3h2yxmipiraw_drv_lock);
	 S5K3H2YXMIPI_sensor.sensor_mode = SENSOR_MODE_PREVIEW;
	 S5K3H2YXMIPI_sensor.IsVideoNightMode=KAL_FALSE;
	 S5K3H2YXMIPI_sensor.VideoMode = KAL_FALSE;
	 S5K3H2YXMIPI_sensor.pv_line_length = S5K3H2YXMIPI_PV_LINE_LENGTH_PIXELS+S5K3H2YXMIPI_sensor.pv_dummy_pixels; 
	 S5K3H2YXMIPI_sensor.pv_frame_length = S5K3H2YXMIPI_PV_FRAME_LENGTH_LINES+S5K3H2YXMIPI_sensor.pv_dummy_lines;
	 spin_unlock(&s5k3h2yxmipiraw_drv_lock); 

	 //S5K3H2YXMIPI_SetShutter(S5K3H2YXMIPI_sensor.pv_shutter);    
	  
     memcpy(&S5K3H2YXMIPISensorConfigData, sensor_config_data, sizeof(MSDK_SENSOR_CONFIG_STRUCT));
   
    return ERROR_NONE;
}	/* S5K3H2YXMIPIPreview() */

UINT32 S5K3H2YXMIPICapture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                                                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
       kal_uint32 shutter = S5K3H2YXMIPI_sensor.pv_shutter;
	   //set mirror & flip
	    spin_lock(&s5k3h2yxmipiraw_drv_lock); 
	    S5K3H2YXMIPI_sensor.bAutoFlickerMode=KAL_FALSE;			  
	  	S5K3H2YXMIPI_sensor.IsVideoNightMode=KAL_FALSE;
	  	S5K3H2YXMIPI_sensor.VideoMode = KAL_FALSE;	  
		S5K3H2YXMIPI_sensor.mirror_flip = sensor_config_data->SensorImageMirror;
		spin_unlock(&s5k3h2yxmipiraw_drv_lock); 
	   
        SENSORDB("[S5K3H2YXMIPI]%s(),pv_shtter=%d,pv_line_length=%d\n",__FUNCTION__,shutter,S5K3H2YXMIPI_sensor.pv_line_length);
	   
        S5K3H2YXMIPI_set_8M_FullSize();
		spin_lock(&s5k3h2yxmipiraw_drv_lock); 
		S5K3H2YXMIPI_sensor.cp_dummy_pixels =200;
		S5K3H2YXMIPI_sensor.cp_dummy_lines = 0;
		S5K3H2YXMIPI_sensor.cp_line_length = S5K3H2YXMIPI_FULL_LINE_LENGTH_PIXELS + S5K3H2YXMIPI_sensor.cp_dummy_pixels;
	    S5K3H2YXMIPI_sensor.cp_frame_length = S5K3H2YXMIPI_FULL_FRAME_LENGTH_LINES + S5K3H2YXMIPI_sensor.cp_dummy_lines;
		spin_unlock(&s5k3h2yxmipiraw_drv_lock); 
		S5K3H2YXMIPI_SetDummy(S5K3H2YXMIPI_sensor.cp_dummy_pixels,S5K3H2YXMIPI_sensor.cp_dummy_lines);
                  
        memcpy(&S5K3H2YXMIPISensorConfigData, sensor_config_data, sizeof(MSDK_SENSOR_CONFIG_STRUCT));

    return ERROR_NONE;
}	/* S5K3H2YXMIPICapture() */

UINT32 S5K3H2YXMIPIGetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution)
{
   #if 1
    pSensorResolution->SensorPreviewWidth = S5K3H2YXMIPI_PV_ACTIVE_PIXEL_NUMS;
    pSensorResolution->SensorPreviewHeight = S5K3H2YXMIPI_PV_ACTIVE_LINE_NUMS;
    pSensorResolution->SensorFullWidth = S5K3H2YXMIPI_FULL_ACTIVE_PIXEL_NUMS;
    pSensorResolution->SensorFullHeight= S5K3H2YXMIPI_FULL_ACTIVE_LINE_NUMS;
	pSensorResolution->SensorVideoWidth=S5K3H2YXMIPI_PV_ACTIVE_PIXEL_NUMS;
	pSensorResolution->SensorVideoHeight=S5K3H2YXMIPI_PV_ACTIVE_LINE_NUMS;
	pSensorResolution->Sensor3DFullWidth=S5K3H2YXMIPI_FULL_ACTIVE_PIXEL_NUMS;
	pSensorResolution->Sensor3DFullHeight=S5K3H2YXMIPI_FULL_ACTIVE_LINE_NUMS;	
	pSensorResolution->Sensor3DPreviewWidth=S5K3H2YXMIPI_PV_ACTIVE_PIXEL_NUMS;
	pSensorResolution->Sensor3DPreviewHeight=S5K3H2YXMIPI_PV_ACTIVE_LINE_NUMS;		
	pSensorResolution->Sensor3DVideoWidth=S5K3H2YXMIPI_PV_ACTIVE_PIXEL_NUMS;
	pSensorResolution->Sensor3DVideoHeight=S5K3H2YXMIPI_PV_ACTIVE_LINE_NUMS;

	
   #else
    pSensorResolution->SensorPreviewWidth = S5K3H2YXMIPI_PV_ACTIVE_PIXEL_NUMS - 12;//12;//12;//16;
    pSensorResolution->SensorPreviewHeight = S5K3H2YXMIPI_PV_ACTIVE_LINE_NUMS - 20;//20;//8;//12;
    pSensorResolution->SensorFullWidth = S5K3H2YXMIPI_FULL_ACTIVE_PIXEL_NUMS - 32;//40;//24;//32;
    pSensorResolution->SensorFullHeight= S5K3H2YXMIPI_FULL_ACTIVE_LINE_NUMS - 6-12-38;//16;//16;//24;
   #endif

    return ERROR_NONE;
}   /* S5K3H2YXMIPIGetResolution() */

UINT32 S5K3H2YXMIPIGetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId,
                                                MSDK_SENSOR_INFO_STRUCT *pSensorInfo,
                                                MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
   #if 1
   // pSensorInfo->SensorPreviewResolutionX=S5K3H2YXMIPI_PV_ACTIVE_PIXEL_NUMS;
   // pSensorInfo->SensorPreviewResolutionY=S5K3H2YXMIPI_PV_ACTIVE_LINE_NUMS;
    pSensorInfo->SensorFullResolutionX=S5K3H2YXMIPI_FULL_ACTIVE_PIXEL_NUMS;
    pSensorInfo->SensorFullResolutionY=S5K3H2YXMIPI_FULL_ACTIVE_LINE_NUMS;
   #else
    pSensorInfo->SensorPreviewResolutionX=S5K3H2YXMIPI_PV_ACTIVE_PIXEL_NUMS - 12;//12;//40;//12;//16;
    pSensorInfo->SensorPreviewResolutionY=S5K3H2YXMIPI_PV_ACTIVE_LINE_NUMS - 20;//20;//8;//12;
    pSensorInfo->SensorFullResolutionX=S5K3H2YXMIPI_FULL_ACTIVE_PIXEL_NUMS - 32;//40//80;//24;//32;
    pSensorInfo->SensorFullResolutionY=S5K3H2YXMIPI_FULL_ACTIVE_LINE_NUMS - 6-12-38;//32;//64;//16;//
   #endif

  //  pSensorInfo->SensorCameraPreviewFrameRate=30;
    spin_lock(&s5k3h2yxmipiraw_drv_lock); 
	S5K3H2YXMIPI_sensor.mirror_flip = pSensorConfigData->SensorImageMirror ;
	spin_unlock(&s5k3h2yxmipiraw_drv_lock); 
	//SENSORDB("S5K3H2YXMIPI_sensor.mirror_flip=%d", S5K3H2YXMIPI_sensor.mirror_flip );
/*
	switch(S5K3H2YXMIPI_sensor.mirror_flip)
	{
		case IMAGE_NORMAL: 
			 pSensorInfo->SensorOutputDataFormat = SENSOR_OUTPUT_FORMAT_RAW_Gr;
			 break;
		case IMAGE_H_MIRROR: 
			 pSensorInfo->SensorOutputDataFormat = SENSOR_OUTPUT_FORMAT_RAW_Gb;
			 break;
	    case IMAGE_V_MIRROR: 
			 pSensorInfo->SensorOutputDataFormat = SENSOR_OUTPUT_FORMAT_RAW_Gr;
			 break;
	    case IMAGE_HV_MIRROR: 
			 pSensorInfo->SensorOutputDataFormat = SENSOR_OUTPUT_FORMAT_RAW_R;
			 break;
		default:
			break;
	}
*/
    pSensorInfo->SensorVideoFrameRate=30;
    pSensorInfo->SensorStillCaptureFrameRate=15;
    pSensorInfo->SensorWebCamCaptureFrameRate=15;
    pSensorInfo->SensorResetActiveHigh=FALSE;
    pSensorInfo->SensorResetDelayCount=5;
   	pSensorInfo->SensorOutputDataFormat=SENSOR_OUTPUT_FORMAT_RAW_B;
    pSensorInfo->SensorClockPolarity=SENSOR_CLOCK_POLARITY_LOW; /*??? */
    pSensorInfo->SensorClockFallingPolarity=SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;//only for parallel
    pSensorInfo->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;//only for parallel
    pSensorInfo->SensorInterruptDelayLines = 1;
    pSensorInfo->SensroInterfaceType=SENSOR_INTERFACE_TYPE_MIPI;

    pSensorInfo->CaptureDelayFrame = 2;//5;//2; 
    pSensorInfo->PreviewDelayFrame = 2;//3;//5;//2; //20120703 for resolve CCT (Raw image analysis error)
    pSensorInfo->VideoDelayFrame = 5; 	
	
    pSensorInfo->SensorMasterClockSwitch = 0; 
    pSensorInfo->SensorDrivingCurrent = ISP_DRIVING_4MA;      
    pSensorInfo->AEShutDelayFrame = 0;		    /* The frame of setting shutter default 0 for TG int */
    pSensorInfo->AESensorGainDelayFrame = 0;     /* The frame of setting sensor gain */
    pSensorInfo->AEISPGainDelayFrame = 2;// 1	

	switch (ScenarioId)
	{
        case MSDK_SCENARIO_ID_CAMERA_ZSD:
			 pSensorInfo->SensorPreviewResolutionX=S5K3H2YXMIPI_FULL_ACTIVE_PIXEL_NUMS;
             pSensorInfo->SensorPreviewResolutionY=S5K3H2YXMIPI_FULL_ACTIVE_LINE_NUMS;
			 pSensorInfo->SensorCameraPreviewFrameRate=15;
			 break;
			 
		default:
			 pSensorInfo->SensorPreviewResolutionX=S5K3H2YXMIPI_PV_ACTIVE_PIXEL_NUMS;
             pSensorInfo->SensorPreviewResolutionY=S5K3H2YXMIPI_PV_ACTIVE_LINE_NUMS;
			 pSensorInfo->SensorCameraPreviewFrameRate=30;
			 break;	
	}

	   
    switch (ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            pSensorInfo->SensorClockFreq=24;//26;
            pSensorInfo->SensorClockDividCount=	5;
            pSensorInfo->SensorClockRisingCount= 0;
            pSensorInfo->SensorClockFallingCount= 2;
            pSensorInfo->SensorPixelClockCount= 3;
            pSensorInfo->SensorDataLatchCount= 2;
            pSensorInfo->SensorGrabStartX = S5K3H2YXMIPI_PV_GRAB_X; 
            pSensorInfo->SensorGrabStartY = S5K3H2YXMIPI_PV_GRAB_Y;           		
            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_2_LANE;			
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0; 
	     pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 14; 
	     pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorWidthSampling = 0;  // 0 is default 1x
            pSensorInfo->SensorHightSampling = 0;   // 0 is default 1x 
            pSensorInfo->SensorPacketECCOrder = 1;
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
            pSensorInfo->SensorClockFreq=24;//26;
            pSensorInfo->SensorClockDividCount=	5;
            pSensorInfo->SensorClockRisingCount= 0;
            pSensorInfo->SensorClockFallingCount= 2;
            pSensorInfo->SensorPixelClockCount= 3;
            pSensorInfo->SensorDataLatchCount= 2;
            pSensorInfo->SensorGrabStartX = S5K3H2YXMIPI_FULL_GRAB_X; 
            pSensorInfo->SensorGrabStartY = S5K3H2YXMIPI_FULL_GRAB_Y;          			
            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_2_LANE;			
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0; 
            pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 14; 
            pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0; 
            pSensorInfo->SensorWidthSampling = 0;  // 0 is default 1x
            pSensorInfo->SensorHightSampling = 0;   // 0 is default 1x
            pSensorInfo->SensorPacketECCOrder = 1;
            break;
        default:
            pSensorInfo->SensorClockFreq=24;//26;
            pSensorInfo->SensorClockDividCount=	3;
            pSensorInfo->SensorClockRisingCount= 0;
            pSensorInfo->SensorClockFallingCount= 2;
            pSensorInfo->SensorPixelClockCount= 3;
            pSensorInfo->SensorDataLatchCount= 2;
            pSensorInfo->SensorGrabStartX = S5K3H2YXMIPI_PV_GRAB_X; 
            pSensorInfo->SensorGrabStartY = S5K3H2YXMIPI_PV_GRAB_Y;             
            break;
    }

    //S5K3H2YXMIPIPixelClockDivider=pSensorInfo->SensorPixelClockCount;
    memcpy(pSensorConfigData, &S5K3H2YXMIPISensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));

    return ERROR_NONE;
}   /* S5K3H2YXMIPIGetInfo() */


UINT32 S5K3H2YXMIPIControl(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow,
                                                MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
    CurrentScenarioId = ScenarioId;
    switch (ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            S5K3H2YXMIPIPreview(pImageWindow, pSensorConfigData);
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
			if(ScenarioId == MSDK_SCENARIO_ID_CAMERA_ZSD)
				{
				   spin_lock(&s5k3h2yxmipiraw_drv_lock);
				   S5K3H2YXMIPI_sensor.sensor_mode = SENSOR_MODE_ZSD_PREVIEW;
				   spin_unlock(&s5k3h2yxmipiraw_drv_lock);
				}
			else
				{
				   spin_lock(&s5k3h2yxmipiraw_drv_lock);
				   S5K3H2YXMIPI_sensor.sensor_mode = SENSOR_MODE_CAPTURE;
				   spin_unlock(&s5k3h2yxmipiraw_drv_lock);
				}
            S5K3H2YXMIPICapture(pImageWindow, pSensorConfigData);
            break;
        default:
            return ERROR_INVALID_SCENARIO_ID;
    }
    return  ERROR_NONE;;
} /* S5K3H2YXMIPIControl() */

//TO DO :need to write fix frame rate func
static void S5K3H2YXMIPI_Fix_Video_Frame_Rate(kal_uint16 framerate)
{    
	kal_uint16 S5K3H2YXMIPI_Video_Max_Line_Length = 0;
	kal_uint16 S5K3H2YXMIPI_Video_Max_Expourse_Time = 0;
  
	
    SENSORDB("[S5K3H2YXMIPI]%s():fix_frame_rate=%d\n",__FUNCTION__,framerate);
	
	S5K3H2YXMIPI_Video_Max_Expourse_Time = (kal_uint16)((S5K3H2YXMIPI_sensor.pv_pclk*10/framerate)/S5K3H2YXMIPI_sensor.pv_line_length);
	
    if (S5K3H2YXMIPI_Video_Max_Expourse_Time > S5K3H2YXMIPI_PV_FRAME_LENGTH_LINES/*S5K3H2YXMIPI_sensor.pv_frame_length*/)	
    	{
    	    spin_lock(&s5k3h2yxmipiraw_drv_lock); 
	    	S5K3H2YXMIPI_sensor.pv_frame_length = S5K3H2YXMIPI_Video_Max_Expourse_Time;
			S5K3H2YXMIPI_sensor.pv_dummy_lines = S5K3H2YXMIPI_sensor.pv_frame_length-S5K3H2YXMIPI_PV_FRAME_LENGTH_LINES;
			spin_unlock(&s5k3h2yxmipiraw_drv_lock); 
			SENSORDB("[S5K3H2YXMIPI]%s():frame_length=%d,dummy_lines=%d\n",__FUNCTION__,S5K3H2YXMIPI_sensor.pv_frame_length,S5K3H2YXMIPI_sensor.pv_dummy_lines);
			S5K3H2YXMIPI_SetDummy(S5K3H2YXMIPI_sensor.pv_dummy_pixels,S5K3H2YXMIPI_sensor.pv_dummy_lines);
    	}
	else
		{
		    spin_lock(&s5k3h2yxmipiraw_drv_lock); 
			S5K3H2YXMIPI_sensor.pv_frame_length = S5K3H2YXMIPI_PV_FRAME_LENGTH_LINES;
			S5K3H2YXMIPI_sensor.pv_dummy_lines = S5K3H2YXMIPI_sensor.pv_frame_length-S5K3H2YXMIPI_PV_FRAME_LENGTH_LINES;
			spin_unlock(&s5k3h2yxmipiraw_drv_lock); 
			SENSORDB("[S5K3H2YXMIPI]%s():frame_length=%d,dummy_lines=%d\n",__FUNCTION__,S5K3H2YXMIPI_sensor.pv_frame_length,S5K3H2YXMIPI_sensor.pv_dummy_lines);
			S5K3H2YXMIPI_SetDummy(S5K3H2YXMIPI_sensor.pv_dummy_pixels,S5K3H2YXMIPI_sensor.pv_dummy_lines);

		}
}

UINT32 S5K3H2YXMIPISetVideoMode(UINT16 u2FrameRate)
{
	SENSORDB("[S5K3H2YXMIPI]%s():fix_frame_rate=%d\n",__FUNCTION__,u2FrameRate);
    spin_lock(&s5k3h2yxmipiraw_drv_lock); 
	S5K3H2YXMIPI_sensor.video_current_frame_rate = u2FrameRate;
	S5K3H2YXMIPI_sensor.VideoMode = KAL_TRUE;
	spin_unlock(&s5k3h2yxmipiraw_drv_lock); 
    if((u2FrameRate==30)||(u2FrameRate==24))
    {
        if(KAL_TRUE==S5K3H2YXMIPI_sensor.bAutoFlickerMode)
			u2FrameRate = u2FrameRate*10 - 4;
		else
			u2FrameRate = u2FrameRate*10;
        spin_lock(&s5k3h2yxmipiraw_drv_lock);
		S5K3H2YXMIPI_sensor.IsVideoNightMode=KAL_FALSE;
		spin_unlock(&s5k3h2yxmipiraw_drv_lock);
		S5K3H2YXMIPI_Fix_Video_Frame_Rate(u2FrameRate);
	  
    }
    else if(u2FrameRate==15)
    {
        if(KAL_TRUE==S5K3H2YXMIPI_sensor.bAutoFlickerMode)
			u2FrameRate = u2FrameRate*10 - 2;
		else
			u2FrameRate = u2FrameRate*10;
        spin_lock(&s5k3h2yxmipiraw_drv_lock);
    	S5K3H2YXMIPI_sensor.IsVideoNightMode=KAL_TRUE;
		spin_unlock(&s5k3h2yxmipiraw_drv_lock);
		S5K3H2YXMIPI_Fix_Video_Frame_Rate(u2FrameRate);

    }
	else if(u2FrameRate == 0)
	{
		spin_lock(&s5k3h2yxmipiraw_drv_lock); 
	   	S5K3H2YXMIPI_sensor.VideoMode = KAL_FALSE;
		spin_unlock(&s5k3h2yxmipiraw_drv_lock);
	   	 return ERROR_NONE;;

	}
	else
	{
      SENSORDB("[S5K3H2YXMIPI][Error]Wrong frame rate setting %d\n", u2FrameRate);
	  //return;
	}

     return ERROR_NONE;;
}
static UINT32 S5K3H2YXMIPISetMaxFrameRate(UINT16 u2FrameRate)
{
#if 1
	kal_int16 dummy_line;
	kal_uint16 FrameHeight;
		
	SENSORDB("u2FrameRate=%d\n",u2FrameRate);

	if(SENSOR_MODE_PREVIEW == S5K3H2YXMIPI_sensor.sensor_mode)
	{
		FrameHeight= (10 * S5K3H2YXMIPI_sensor.pv_pclk) / u2FrameRate / S5K3H2YXMIPI_PV_LINE_LENGTH_PIXELS;
		dummy_line = FrameHeight - S5K3H2YXMIPI_PV_FRAME_LENGTH_LINES;

	}
	else if(SENSOR_MODE_ZSD_PREVIEW == S5K3H2YXMIPI_sensor.sensor_mode)
	{
		FrameHeight= (10 * S5K3H2YXMIPI_sensor.cp_pclk) / u2FrameRate / S5K3H2YXMIPI_FULL_LINE_LENGTH_PIXELS;
		dummy_line = FrameHeight - S5K3H2YXMIPI_FULL_FRAME_LENGTH_LINES;
	}
	
	SENSORDB("dummy_line=%d",dummy_line);
	dummy_line = (dummy_line>0?dummy_line:0);
	S5K3H2YXMIPI_SetDummy(0, dummy_line); /* modify dummy_pixel must gen AE table again */	
#endif
}

UINT32 S5K3H2YXMIPISetAutoFlickerMode(kal_bool bEnable, UINT16 u2FrameRate)
{
	if(bEnable)
	{
	    spin_lock(&s5k3h2yxmipiraw_drv_lock); 
		S5K3H2YXMIPI_sensor.bAutoFlickerMode=KAL_TRUE;
		spin_unlock(&s5k3h2yxmipiraw_drv_lock);
		if(KAL_TRUE==S5K3H2YXMIPI_sensor.VideoMode) 
		{
			if(KAL_TRUE==S5K3H2YXMIPI_sensor.IsVideoNightMode) 
			{
				S5K3H2YXMIPISetMaxFrameRate(148);
			}
			else
			{
				S5K3H2YXMIPISetMaxFrameRate(296);
			}	
		}
	}
	else
	{
	    spin_lock(&s5k3h2yxmipiraw_drv_lock); 
		S5K3H2YXMIPI_sensor.bAutoFlickerMode=KAL_FALSE;
		spin_unlock(&s5k3h2yxmipiraw_drv_lock); 
		if(KAL_TRUE==S5K3H2YXMIPI_sensor.IsVideoNightMode) 
		{
			S5K3H2YXMIPISetMaxFrameRate(150);
		}
		else
		{
			S5K3H2YXMIPISetMaxFrameRate(300);
		}	
		
	}
    SENSORDB("[S5K3H2YXSetAutoFlickerMode] frame rate(10base) = %d %d\n", bEnable, u2FrameRate);
     return ERROR_NONE;;
}


UINT32 S5K3H2YXMIPIFeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId,
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
            *pFeatureReturnPara16++=S5K3H2YXMIPI_FULL_ACTIVE_PIXEL_NUMS ;//- S5K3H2YXMIPI_FULL_GRAB_X;
            *pFeatureReturnPara16=S5K3H2YXMIPI_FULL_ACTIVE_LINE_NUMS ;//- S5K3H2YXMIPI_FULL_GRAB_Y;
            *pFeatureParaLen=4;
            break;
        case SENSOR_FEATURE_GET_PERIOD:
			switch(CurrentScenarioId)
			{
              case MSDK_SCENARIO_ID_CAMERA_ZSD:
			  case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			  	*pFeatureReturnPara16++=S5K3H2YXMIPI_sensor.cp_line_length;
                *pFeatureReturnPara16=S5K3H2YXMIPI_sensor.cp_frame_length;
                SENSORDB("Sensor period:%d %d\n", S5K3H2YXMIPI_sensor.cp_line_length, S5K3H2YXMIPI_sensor.cp_frame_length); 
                *pFeatureParaLen=4;
				break;
				
			  default:
			  	*pFeatureReturnPara16++=S5K3H2YXMIPI_sensor.pv_line_length;
                *pFeatureReturnPara16=S5K3H2YXMIPI_sensor.pv_frame_length;
                SENSORDB("Sensor period:%d %d\n", S5K3H2YXMIPI_sensor.pv_line_length, S5K3H2YXMIPI_sensor.pv_frame_length); 
                *pFeatureParaLen=4;
				break;
			}
            break;
        case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
			switch(CurrentScenarioId)
			{
              case MSDK_SCENARIO_ID_CAMERA_ZSD:
			  case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			  	 *pFeatureReturnPara32 = S5K3H2YXMIPI_sensor.cp_pclk;
                 *pFeatureParaLen=4;
			  	 break;
				
			  default:
			  	 *pFeatureReturnPara32 = S5K3H2YXMIPI_sensor.pv_pclk;
                 *pFeatureParaLen=4;
				 break;
			}
            break;
        case SENSOR_FEATURE_SET_ESHUTTER:
            S5K3H2YXMIPI_SetShutter(*pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_NIGHTMODE:
            //S5K3H2YXMIPI_NightMode((BOOL) *pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_GAIN:
            S5K3H2YXMIPI_SetGain((UINT16) *pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_FLASHLIGHT:
            break;
        case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
            //S5K3H2YXMIPI_isp_master_clock=*pFeatureData32;
            break;
		//For debug and tuning
        case SENSOR_FEATURE_SET_REGISTER:
            S5K3H2YXMIPI_write_cmos_sensor(pSensorRegData->RegAddr, pSensorRegData->RegData);
            break;
        case SENSOR_FEATURE_GET_REGISTER:
            pSensorRegData->RegData = S5K3H2YXMIPI_read_cmos_sensor(pSensorRegData->RegAddr);
            break;
		//for CCT(only for raw sensor)
        case SENSOR_FEATURE_SET_CCT_REGISTER:
            SensorRegNumber=FACTORY_END_ADDR;
            for (i=0;i<SensorRegNumber;i++)
            {
                spin_lock(&s5k3h2yxmipiraw_drv_lock); 
                S5K3H2YXMIPISensorCCT[i].Addr=*pFeatureData32++;
                S5K3H2YXMIPISensorCCT[i].Para=*pFeatureData32++;
				spin_unlock(&s5k3h2yxmipiraw_drv_lock); 
            }
            break;
        case SENSOR_FEATURE_GET_CCT_REGISTER:
            SensorRegNumber=FACTORY_END_ADDR;
            if (*pFeatureParaLen<(SensorRegNumber*sizeof(SENSOR_REG_STRUCT)+4))
                return FALSE;
            *pFeatureData32++=SensorRegNumber;
            for (i=0;i<SensorRegNumber;i++)
            {
                *pFeatureData32++=S5K3H2YXMIPISensorCCT[i].Addr;
                *pFeatureData32++=S5K3H2YXMIPISensorCCT[i].Para;
            }
            break;
        case SENSOR_FEATURE_SET_ENG_REGISTER:
            SensorRegNumber=ENGINEER_END;
            for (i=0;i<SensorRegNumber;i++)
            {
                spin_lock(&s5k3h2yxmipiraw_drv_lock); 
                S5K3H2YXMIPISensorReg[i].Addr=*pFeatureData32++;
                S5K3H2YXMIPISensorReg[i].Para=*pFeatureData32++;
				spin_unlock(&s5k3h2yxmipiraw_drv_lock); 
            }
            break;
        case SENSOR_FEATURE_GET_ENG_REGISTER:
            SensorRegNumber=ENGINEER_END;
            if (*pFeatureParaLen<(SensorRegNumber*sizeof(SENSOR_REG_STRUCT)+4))
                return FALSE;
            *pFeatureData32++=SensorRegNumber;
            for (i=0;i<SensorRegNumber;i++)
            {
                *pFeatureData32++=S5K3H2YXMIPISensorReg[i].Addr;
                *pFeatureData32++=S5K3H2YXMIPISensorReg[i].Para;
            }
            break;
        case SENSOR_FEATURE_GET_REGISTER_DEFAULT:
            if (*pFeatureParaLen>=sizeof(NVRAM_SENSOR_DATA_STRUCT))
            {
                pSensorDefaultData->Version=NVRAM_CAMERA_SENSOR_FILE_VERSION;
                pSensorDefaultData->SensorId=S5K3H2YXMIPI_SENSOR_ID;
                memcpy(pSensorDefaultData->SensorEngReg, S5K3H2YXMIPISensorReg, sizeof(SENSOR_REG_STRUCT)*ENGINEER_END);
                memcpy(pSensorDefaultData->SensorCCTReg, S5K3H2YXMIPISensorCCT, sizeof(SENSOR_REG_STRUCT)*FACTORY_END_ADDR);
            }
            else
                return FALSE;
            *pFeatureParaLen=sizeof(NVRAM_SENSOR_DATA_STRUCT);
            break;
        case SENSOR_FEATURE_GET_CONFIG_PARA:
            memcpy(pSensorConfigData, &S5K3H2YXMIPISensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));
            *pFeatureParaLen=sizeof(MSDK_SENSOR_CONFIG_STRUCT);
            break;
        case SENSOR_FEATURE_CAMERA_PARA_TO_SENSOR:
            S5K3H2YXMIPI_camera_para_to_sensor();
            break;

        case SENSOR_FEATURE_SENSOR_TO_CAMERA_PARA:
            S5K3H2YXMIPI_sensor_to_camera_para();
            break;
        case SENSOR_FEATURE_GET_GROUP_COUNT:
            *pFeatureReturnPara32++=S5K3H2YXMIPI_get_sensor_group_count();
            *pFeatureParaLen=4;
            break;
        case SENSOR_FEATURE_GET_GROUP_INFO:
            S5K3H2YXMIPI_get_sensor_group_info(pSensorGroupInfo->GroupIdx, pSensorGroupInfo->GroupNamePtr, &pSensorGroupInfo->ItemCount);
            *pFeatureParaLen=sizeof(MSDK_SENSOR_GROUP_INFO_STRUCT);
            break;
        case SENSOR_FEATURE_GET_ITEM_INFO:
            S5K3H2YXMIPI_get_sensor_item_info(pSensorItemInfo->GroupIdx,pSensorItemInfo->ItemIdx, pSensorItemInfo);
            *pFeatureParaLen=sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
            break;

        case SENSOR_FEATURE_SET_ITEM_INFO:
            S5K3H2YXMIPI_set_sensor_item_info(pSensorItemInfo->GroupIdx, pSensorItemInfo->ItemIdx, pSensorItemInfo->ItemValue);
            *pFeatureParaLen=sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
            break;

        case SENSOR_FEATURE_GET_ENG_INFO:
            pSensorEngInfo->SensorId = 283;
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
            S5K3H2YXMIPISetVideoMode(*pFeatureData16);
            break;
        case SENSOR_FEATURE_CHECK_SENSOR_ID:
            S5K3H2YXMIPIGetSensorID(pFeatureReturnPara32); 
            break;  
		case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
            S5K3H2YXMIPISetAutoFlickerMode((BOOL)*pFeatureData16, *(pFeatureData16+1));            
	        break;
		case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
			S5K3H2YXMIPISetMaxFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32,*(pFeatureData32+1));
			break;
		case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
			S5K3H2YXMIPIGetDefaultFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32,(MUINT32 *)(*(pFeatureData32+1)));
			break;
        default:
            break;
    }
    return ERROR_NONE;
}	/* S5K3H2YXMIPIFeatureControl() */


SENSOR_FUNCTION_STRUCT	SensorFuncS5K3H2YXMIPI=
{
    S5K3H2YXMIPIOpen,
    S5K3H2YXMIPIGetInfo,
    S5K3H2YXMIPIGetResolution,
    S5K3H2YXMIPIFeatureControl,
    S5K3H2YXMIPIControl,
    S5K3H2YXMIPIClose
};

UINT32 S5K3H2YX_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
    /* To Do : Check Sensor status here */
    if (pfFunc!=NULL)
        *pfFunc=&SensorFuncS5K3H2YXMIPI;

    return ERROR_NONE;
}   /* SensorInit() */

