/*****************************************************************************
 *
 * Filename:
 * ---------
 *   Sensor.c
 *
 * Project:
 * --------
 *   DUMA
 *
 * Description:
 * ------------
 *   Image sensor driver function
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by PVCS VM. DO NOT MODIFY!!
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

#include "ov9726mipiraw_Sensor.h"
#include "ov9726mipiraw_Camera_Sensor_para.h"
#include "ov9726mipiraw_CameraCustomized.h"

#if 1
#define SENSORDB printk
#else
#define SENSORDB(x,...)
#endif
//#define ACDK
extern int iReadRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u8 * a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u16 i2cId);


static OV9726MIPI_sensor_struct OV9726MIPI_sensor =
{
  .eng =
  {
    .reg = CAMERA_SENSOR_REG_DEFAULT_VALUE,
    .cct = CAMERA_SENSOR_CCT_DEFAULT_VALUE,
  },
  .eng_info =
  {
    .SensorId = 128,
    .SensorType = CMOS_SENSOR,
    .SensorOutputDataFormat = OV9726MIPI_COLOR_FORMAT,
  },
  .shutter = 0x20,  
  .gain = 0x20,
  .pclk = OV9726MIPI_PREVIEW_CLK,
  .frame_height = OV9726MIPI_PV_PERIOD_LINE_NUMS+OV9726MIPI_DEFAULT_DUMMY_LINES,
  .line_length = OV9726MIPI_PV_PERIOD_PIXEL_NUMS+OV9726MIPI_DEFAULT_DUMMY_PIXELS,
};

kal_uint16 OV9726MIPI_sensor_id=0;


static DEFINE_SPINLOCK(ov9726mipiraw_drv_lock);

kal_bool OV9726MIPI_AutoFlicker_Mode=KAL_FALSE;

static MSDK_SCENARIO_ID_ENUM CurrentScenarioId=MSDK_SCENARIO_ID_CAMERA_PREVIEW;

void OV9726MIPI_write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
    char puSendCmd[3] = {(char)(addr >> 8) , (char)(addr & 0xFF) ,(char)(para & 0xFF)};
	
	iWriteRegI2C(puSendCmd , 3,OV9726MIPI_WRITE_ID);

}
kal_uint16 OV9726MIPI_read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte=0;
    char puSendCmd[2] = {(char)(addr >> 8) , (char)(addr & 0xFF) };
	iReadRegI2C(puSendCmd , 2, (u8*)&get_byte,1,OV9726MIPI_WRITE_ID);
		
    return get_byte;
}


static void OV9726MIPI_Write_Shutter(kal_uint16 iShutter)
{
#if 0
	kal_uint16 shutter_line = 0,current_fps=0;
	unsigned long flags;

	

    
	if (iShutter<4) iShutter = 4; /* avoid 0 */

	



	if(iShutter > OV9726MIPI_sensor.default_height-6) 
		{

		    shutter_line=iShutter+6;

		}
	else
		{
		   shutter_line=OV9726MIPI_sensor.default_height;

		}
     current_fps=OV9726MIPI_sensor.pclk/OV9726MIPI_sensor.line_length/shutter_line;
	 SENSORDB("CURRENT FPS:%d,OV9726MIPI_sensor.default_height=%d\n",
	 	current_fps,OV9726MIPI_sensor.default_height);
	 
	 if(current_fps==30 || current_fps==15)
	 {
	   if(OV9726MIPI_AutoFlicker_Mode==TRUE)
	   	  shutter_line=shutter_line+(shutter_line>>7);
	 }
     SENSORDB("OV9726MIPI_Write_Shutter line-length:%x,shutter:%x \n",shutter_line,iShutter);
     OV9726MIPI_write_cmos_sensor(0x0340, (shutter_line >> 8) & 0xFF);
	 OV9726MIPI_write_cmos_sensor(0x0341, shutter_line & 0xFF);
	 spin_lock_irqsave(&ov9726mipiraw_drv_lock,flags);
     OV9726MIPI_sensor.frame_height=shutter_line;
	 spin_unlock_irqrestore(&ov9726mipiraw_drv_lock,flags);
	 OV9726MIPI_write_cmos_sensor(0x0202, (iShutter >> 8) & 0xFF);	
	 OV9726MIPI_write_cmos_sensor(0x0203, (iShutter) & 0xFF);
	SENSORDB("OV9726MIPI_frame_length:%x \n",OV9726MIPI_sensor.frame_height);
	
/*
	extra_line = iShutter - OV9726MIPI_sensor.frame_height+6;

	SENSORDB("OV9726MIPI_sensor.frame_height:%d \n",OV9726MIPI_sensor.frame_height);
	SENSORDB("extra_line:%x \n",extra_line);
	if(extra_line>=0)
		{
    	OV9726MIPI_write_cmos_sensor(0x350c, (extra_line >> 8) & 0xFF);	
   		OV9726MIPI_write_cmos_sensor(0x350d, (extra_line) & 0xFF);
		OV9726MIPI_write_cmos_sensor(0x3503,0x17);
		}
*/
#else
	kal_uint16 shutter_line = 0,current_fps=0,extra_line=0;
	unsigned long flags;

	SENSORDB("OV9726MIPI_Write_Shutter:%x \n",iShutter);

    
	if (iShutter<4) iShutter = 4; /* avoid 0 */

	



	if(iShutter > OV9726MIPI_sensor.default_height-6) 
		{

		    shutter_line=iShutter;
			extra_line = iShutter - OV9726MIPI_sensor.default_height+6;

		}
	else
		{
		   shutter_line=OV9726MIPI_sensor.default_height;
		   extra_line=0;

		}


     current_fps=OV9726MIPI_sensor.pclk/OV9726MIPI_sensor.line_length/shutter_line;
	 if(current_fps==30 || current_fps==15)
	 {
	   if(OV9726MIPI_AutoFlicker_Mode==TRUE)
	   	  shutter_line=shutter_line+(shutter_line>>7);
	 }

	 spin_lock_irqsave(&ov9726mipiraw_drv_lock,flags);
     OV9726MIPI_sensor.frame_height=shutter_line;
	 spin_unlock_irqrestore(&ov9726mipiraw_drv_lock,flags);
#if 0
     OV9726MIPI_write_cmos_sensor(0x0340, (shutter_line >> 8) & 0xFF);
	 OV9726MIPI_write_cmos_sensor(0x0341, shutter_line & 0xFF);
	 
	 

	SENSORDB("OV9726_frame_length:%x \n",OV9726MIPI_sensor.frame_height);
	
#else
	

	SENSORDB("OV9726MIPI_sensor.frame_height:%d,default_height=%d\n",OV9726MIPI_sensor.frame_height,OV9726MIPI_sensor.default_height);
	SENSORDB("extra_line:%x \n",extra_line);
	if(extra_line<0)
		extra_line=0;
	
		
    	OV9726MIPI_write_cmos_sensor(0x350c, (extra_line >> 8) & 0xFF);	
   		OV9726MIPI_write_cmos_sensor(0x350d, (extra_line) & 0xFF);
		OV9726MIPI_write_cmos_sensor(0x3503,0x17);
		
	
#endif
	 OV9726MIPI_write_cmos_sensor(0x0202, (iShutter >> 8) & 0xFF);	
	 OV9726MIPI_write_cmos_sensor(0x0203, (iShutter) & 0xFF);

#endif
	
}   /*  OV9726MIPI_Write_Shutter    */

static void OV9726MIPI_Set_Dummy(const kal_uint16 iPixels, const kal_uint16 iLines)
{

	kal_uint16  line_length, frame_height;

	SENSORDB("OV9726MIPI_Set_Dummy:iPixels:%x; iLines:%x \n",iPixels,iLines);


	if (OV9726MIPI_sensor.pv_mode)
	{
	  line_length = OV9726MIPI_PV_PERIOD_PIXEL_NUMS + iPixels+OV9726MIPI_DEFAULT_DUMMY_PIXELS;
	  frame_height = OV9726MIPI_PV_PERIOD_LINE_NUMS + iLines+OV9726MIPI_DEFAULT_DUMMY_LINES;
	}
	else
	{
	  line_length = OV9726MIPI_FULL_PERIOD_PIXEL_NUMS + iPixels+OV9726MIPI_DEFAULT_DUMMY_PIXELS;
	  frame_height = OV9726MIPI_FULL_PERIOD_LINE_NUMS + iLines+OV9726MIPI_DEFAULT_DUMMY_LINES;
	}
	

	SENSORDB("line_length:%x; frame_height:%x \n",line_length,frame_height);


	if ((line_length >= 0x1FFF)||(frame_height >= 0xFFF))
	{

		SENSORDB("Warnning: line length or frame height is overflow!!!!!!!!  \n");

		return ;
	}
	
//	if((line_length == OV9726MIPI_sensor.line_length)&&(frame_height == OV9726MIPI_sensor.frame_height))
//		return ;
    spin_lock(&ov9726mipiraw_drv_lock);
	OV9726MIPI_sensor.line_length = line_length;
	OV9726MIPI_sensor.frame_height = frame_height;
	OV9726MIPI_sensor.default_height=frame_height;
	spin_unlock(&ov9726mipiraw_drv_lock);
	
    /*  Add dummy pixels: */
    /* 0x380c [0:4], 0x380d defines the PCLKs in one line of OV9726MIPI  */  
    /* Add dummy lines:*/
    /* 0x380e [0:1], 0x380f defines total lines in one frame of OV9726MIPI */
    OV9726MIPI_write_cmos_sensor(0x0342, line_length >> 8);
    OV9726MIPI_write_cmos_sensor(0x0343, line_length & 0xFF);
    OV9726MIPI_write_cmos_sensor(0x0340, frame_height >> 8);
    OV9726MIPI_write_cmos_sensor(0x0341, frame_height & 0xFF);

	
}   /*  OV9726MIPI_Set_Dummy    */

/*************************************************************************
* FUNCTION
*	OV9726MIPI_SetShutter
*
* DESCRIPTION
*	This function set e-shutter of OV9726MIPI to change exposure time.
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
void set_OV9726MIPI_shutter(kal_uint16 iShutter)
{
	unsigned long flags;

	SENSORDB("set_OV9726MIPI_shutter:%x \n",iShutter);

    spin_lock_irqsave(&ov9726mipiraw_drv_lock,flags);
	OV9726MIPI_sensor.shutter = iShutter;
	spin_unlock_irqrestore(&ov9726mipiraw_drv_lock,flags);
    OV9726MIPI_Write_Shutter(iShutter);

}   /*  Set_OV9726MIPI_Shutter */


#if 0
static kal_uint16 OV9726MIPIReg2Gain(const kal_uint8 iReg)
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
static kal_uint8 OV9726MIPIGain2Reg(const kal_uint16 iGain)
{
    kal_uint8 iReg = 0x00;

    if (iGain < 2 * BASEGAIN) {
        // Gain = 1 + GAIN[3:0](0x00) / 16
        iReg = 16 * (iGain - BASEGAIN) / BASEGAIN;
    }else if (iGain < 4 * BASEGAIN) {
        // Gain = 2 * (1 + GAIN[3:0](0x00) / 16)
        iReg |= 0x10;
        iReg |= 8 * (iGain - 2 * BASEGAIN) / BASEGAIN;
    }else if (iGain < 8 * BASEGAIN) {
        // Gain = 4 * (1 + GAIN[3:0](0x00) / 16)
        iReg |= 0x30;
        iReg |= 4 * (iGain - 4 * BASEGAIN) / BASEGAIN;
    }else if (iGain < 16 * BASEGAIN) {
        // Gain = 8 * (1 + GAIN[3:0](0x00) / 16)
        iReg |= 0x70;
        iReg |= 2 * (iGain - 8 * BASEGAIN) / BASEGAIN;
    }else if (iGain < 32 * BASEGAIN) {
        // Gain = 16 * (1 + GAIN[3:0](0x00) / 16)
        iReg |= 0xF0;
        iReg |= (iGain - 16 * BASEGAIN) / BASEGAIN;
    }else {
        ASSERT(0);
    }

    return iReg;

}




/*************************************************************************
* FUNCTION
*	OV9726MIPI_SetGain
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
	void OV9726MIPI_SetGain(UINT16 iGain)
	{
	//	static kal_uint16 Extra_Shutter = 0;
	
		kal_uint8 iReg;
		
		 iReg = OV9726MIPIGain2Reg(iGain);
		 SENSORDB("OV9726MIPI Set iGain=%d,ireg=%d\n",iGain,iReg);
		 //=======================================================
		 /* For sensor Gain curve correction, all shift number is test result. mtk70508*/
#if 0
		 if((iReg >=0x10)&&(iReg <0x30))// 2*Gain -4*Gain ,shift  1/8* Gain
		  iReg +=1;
	
		 if((iReg > 0x1f)&&(iReg <0x30))// Above 4 *Gain  after shift 1/8* Gain, Set 4*Gain register value ,Then shift	again
		  {
		  iReg |= 0x30;
		  iReg += 0x2;
		  }
		 else if((iReg >=0x30)&&(iReg <0x70))//  4*Gain - 8*Gain, Shift  3/4* Gain
		  iReg +=3;
		 if(OV9726MIPI_sensor_id == OV9726MIPI_SENSOR_ID)
		 {
			  if((iReg > 0x3f)||(iReg >=0x70))//ov2655 max gain is 8X
				 iReg = 0x3f;
		 }
		 else
		 {
			 if((iReg > 0x3f)&&(iReg <0x70))// above 8*Gain after shift   1* Gain, Set 8*Gain register value
			  {
			  iReg |= 0x70;
			  //iReg +=1;
			  }
			  else if((iReg >=0x70)&&(iReg <0xf0))//  8*Gain - 16*Gain, Shift  3/2* Gain
			  iReg +=3;
	
			  if(iReg >0x7f)// above 16*Gain, Set  as 16*Gain
			  iReg =0xf0;
		 }
#endif		  
		  //========================================================
//		  OV9726MIPI_write_cmos_sensor(0x3503,0x17);
		  OV9726MIPI_write_cmos_sensor(0x0205, (kal_uint32)iReg);
	
	//===============================

	}	/*	OV9726MIPI_SetGain	*/



/*************************************************************************
* FUNCTION
*	OV9726MIPI_NightMode
*
* DESCRIPTION
*	This function night mode of OV9726MIPI.
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
void OV9726MIPI_night_mode(kal_bool enable)
{
	const kal_uint16 dummy_pixel = OV9726MIPI_sensor.line_length - OV9726MIPI_PV_PERIOD_PIXEL_NUMS;
	const kal_uint16 pv_min_fps =  enable ? OV9726MIPI_sensor.night_fps : OV9726MIPI_sensor.normal_fps;
	kal_uint16 dummy_line = OV9726MIPI_sensor.frame_height - OV9726MIPI_PV_PERIOD_LINE_NUMS;
	kal_uint16 max_exposure_lines;
	

   SENSORDB("OV9726MIPI_night_mode:enable:%x;\n",enable);


	if (!OV9726MIPI_sensor.video_mode) return;
	max_exposure_lines = OV9726MIPI_sensor.pclk * OV9726MIPI_FPS(1) / (pv_min_fps * OV9726MIPI_sensor.line_length);
	if (max_exposure_lines > OV9726MIPI_sensor.frame_height) /* fix max frame rate, AE table will fix min frame rate */
	{
	  dummy_line = max_exposure_lines - OV9726MIPI_PV_PERIOD_LINE_NUMS;

	 SENSORDB("dummy_line:%x;\n",dummy_line);

	  OV9726MIPI_Set_Dummy(dummy_pixel, dummy_line);
	}

}   /*  OV9726MIPI_NightMode    */


/* write camera_para to sensor register */
static void OV9726MIPI_camera_para_to_sensor(void)
{
  kal_uint32 i;

	 SENSORDB("OV9726MIPI_camera_para_to_sensor\n");

  for (i = 0; 0xFFFFFFFF != OV9726MIPI_sensor.eng.reg[i].Addr; i++)
  {
    OV9726MIPI_write_cmos_sensor(OV9726MIPI_sensor.eng.reg[i].Addr, OV9726MIPI_sensor.eng.reg[i].Para);
  }
  for (i = OV9726MIPI_FACTORY_START_ADDR; 0xFFFFFFFF != OV9726MIPI_sensor.eng.reg[i].Addr; i++)
  {
    OV9726MIPI_write_cmos_sensor(OV9726MIPI_sensor.eng.reg[i].Addr, OV9726MIPI_sensor.eng.reg[i].Para);
  }
  OV9726MIPI_SetGain(OV9726MIPI_sensor.gain); /* update gain */
}

/* update camera_para from sensor register */
static void OV9726MIPI_sensor_to_camera_para(void)
{
  kal_uint32 i,temp_data;

   SENSORDB("OV9726MIPI_sensor_to_camera_para\n");

  for (i = 0; 0xFFFFFFFF != OV9726MIPI_sensor.eng.reg[i].Addr; i++)
  {
    
		temp_data= OV9726MIPI_read_cmos_sensor(OV9726MIPI_sensor.eng.reg[i].Addr);
	    spin_lock(&ov9726mipiraw_drv_lock);
		OV9726MIPI_sensor.eng.reg[i].Para =temp_data;
		spin_lock(&ov9726mipiraw_drv_lock);
  }
  for (i = OV9726MIPI_FACTORY_START_ADDR; 0xFFFFFFFF != OV9726MIPI_sensor.eng.reg[i].Addr; i++)
  {
		temp_data= OV9726MIPI_read_cmos_sensor(OV9726MIPI_sensor.eng.reg[i].Addr);
		spin_unlock(&ov9726mipiraw_drv_lock);
		OV9726MIPI_sensor.eng.reg[i].Para=temp_data;
		spin_unlock(&ov9726mipiraw_drv_lock);
  }
}

/* ------------------------ Engineer mode ------------------------ */
inline static void OV9726MIPI_get_sensor_group_count(kal_int32 *sensor_count_ptr)
{

   SENSORDB("OV9726MIPI_get_sensor_group_count\n");

  *sensor_count_ptr = OV9726MIPI_GROUP_TOTAL_NUMS;
}

inline static void OV9726MIPI_get_sensor_group_info(MSDK_SENSOR_GROUP_INFO_STRUCT *para)
{

   SENSORDB("OV9726MIPI_get_sensor_group_info\n");

  switch (para->GroupIdx)
  {
  case OV9726MIPI_PRE_GAIN:
    sprintf(para->GroupNamePtr, "CCT");
    para->ItemCount = 5;
    break;
  case OV9726MIPI_CMMCLK_CURRENT:
    sprintf(para->GroupNamePtr, "CMMCLK Current");
    para->ItemCount = 1;
    break;
  case OV9726MIPI_FRAME_RATE_LIMITATION:
    sprintf(para->GroupNamePtr, "Frame Rate Limitation");
    para->ItemCount = 2;
    break;
  case OV9726MIPI_REGISTER_EDITOR:
    sprintf(para->GroupNamePtr, "Register Editor");
    para->ItemCount = 2;
    break;
  default:
    ASSERT(0);
  }
}

inline static void OV9726MIPI_get_sensor_item_info(MSDK_SENSOR_ITEM_INFO_STRUCT *para)
{

  const static kal_char *cct_item_name[] = {"SENSOR_BASEGAIN", "Pregain-R", "Pregain-Gr", "Pregain-Gb", "Pregain-B"};
  const static kal_char *editer_item_name[] = {"REG addr", "REG value"};
  

	 SENSORDB("OV9726MIPI_get_sensor_item_info\n");

  switch (para->GroupIdx)
  {
  case OV9726MIPI_PRE_GAIN:
    switch (para->ItemIdx)
    {
    case OV9726MIPI_SENSOR_BASEGAIN:
    case OV9726MIPI_PRE_GAIN_R_INDEX:
    case OV9726MIPI_PRE_GAIN_Gr_INDEX:
    case OV9726MIPI_PRE_GAIN_Gb_INDEX:
    case OV9726MIPI_PRE_GAIN_B_INDEX:
      break;
    default:
      ASSERT(0);
    }
    sprintf(para->ItemNamePtr, cct_item_name[para->ItemIdx - OV9726MIPI_SENSOR_BASEGAIN]);
    para->ItemValue = OV9726MIPI_sensor.eng.cct[para->ItemIdx].Para * 1000 / BASEGAIN;
    para->IsTrueFalse = para->IsReadOnly = para->IsNeedRestart = KAL_FALSE;
    para->Min = OV9726MIPI_MIN_ANALOG_GAIN * 1000;
    para->Max = OV9726MIPI_MAX_ANALOG_GAIN * 1000;
    break;
  case OV9726MIPI_CMMCLK_CURRENT:
    switch (para->ItemIdx)
    {
    case 0:
      sprintf(para->ItemNamePtr, "Drv Cur[2,4,6,8]mA");
      switch (OV9726MIPI_sensor.eng.reg[OV9726MIPI_CMMCLK_CURRENT_INDEX].Para)
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
  case OV9726MIPI_FRAME_RATE_LIMITATION:
    switch (para->ItemIdx)
    {
    case 0:
      sprintf(para->ItemNamePtr, "Max Exposure Lines");
      para->ItemValue = 5998;
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
  case OV9726MIPI_REGISTER_EDITOR:
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

inline static kal_bool OV9726MIPI_set_sensor_item_info(MSDK_SENSOR_ITEM_INFO_STRUCT *para)
{
  kal_uint16 temp_para;

   SENSORDB("OV9726MIPI_set_sensor_item_info\n");

  switch (para->GroupIdx)
  {
  case OV9726MIPI_PRE_GAIN:
    switch (para->ItemIdx)
    {
    case OV9726MIPI_SENSOR_BASEGAIN:
    case OV9726MIPI_PRE_GAIN_R_INDEX:
    case OV9726MIPI_PRE_GAIN_Gr_INDEX:
    case OV9726MIPI_PRE_GAIN_Gb_INDEX:
    case OV9726MIPI_PRE_GAIN_B_INDEX:
      spin_lock(&ov9726mipiraw_drv_lock);
      OV9726MIPI_sensor.eng.cct[para->ItemIdx].Para = para->ItemValue * BASEGAIN / 1000;
	  spin_unlock(&ov9726mipiraw_drv_lock);
      OV9726MIPI_SetGain(OV9726MIPI_sensor.gain); /* update gain */
      break;
    default:
      ASSERT(0);
    }
    break;
  case OV9726MIPI_CMMCLK_CURRENT:
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
      //OV9726MIPI_set_isp_driving_current(temp_para);
	  spin_lock(&ov9726mipiraw_drv_lock);
      OV9726MIPI_sensor.eng.reg[OV9726MIPI_CMMCLK_CURRENT_INDEX].Para = temp_para;
	  spin_unlock(&ov9726mipiraw_drv_lock);
      break;
    default:
      ASSERT(0);
    }
    break;
  case OV9726MIPI_FRAME_RATE_LIMITATION:
    ASSERT(0);
    break;
  case OV9726MIPI_REGISTER_EDITOR:
    switch (para->ItemIdx)
    {
      static kal_uint32 fac_sensor_reg;
    case 0:
      if (para->ItemValue < 0 || para->ItemValue > 0xFFFF) return KAL_FALSE;
      fac_sensor_reg = para->ItemValue;
      break;
    case 1:
      if (para->ItemValue < 0 || para->ItemValue > 0xFF) return KAL_FALSE;
      OV9726MIPI_write_cmos_sensor(fac_sensor_reg, para->ItemValue);
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


void OV9726MIPI_init(void)
{
	SENSORDB("OV9726MIPI init\n");
	OV9726MIPI_write_cmos_sensor(0x0103,0x01);// software reset
	mdelay(200);//delay 200ms
	OV9726MIPI_write_cmos_sensor(0x3026,0x00);// set D[9:8] to normal data path
	OV9726MIPI_write_cmos_sensor(0x3027,0x00);// set D[7:0] to normal data path
	OV9726MIPI_write_cmos_sensor(0x3002,0xe8);// set VSYNC, HREF and PCLK to output
	OV9726MIPI_write_cmos_sensor(0x3004,0x03);// set D[9:8] to output
	OV9726MIPI_write_cmos_sensor(0x3005,0xff); //set D[7:0] to output
	OV9726MIPI_write_cmos_sensor(0x3703,0x42);// timing, sensor HLDWIDTH
	OV9726MIPI_write_cmos_sensor(0x3704,0x10);// timing, sensor TXWIDTH
	OV9726MIPI_write_cmos_sensor(0x3705,0x45);// timing
	OV9726MIPI_write_cmos_sensor(0x3603,0xaa);// analog control
	OV9726MIPI_write_cmos_sensor(0x3632,0x2f);// analog control
	OV9726MIPI_write_cmos_sensor(0x3620,0x66);// analog control
	OV9726MIPI_write_cmos_sensor(0x3621,0xc0);//analog control
	OV9726MIPI_write_cmos_sensor(0x0340,0x03);// VTS = 840
	OV9726MIPI_write_cmos_sensor(0x0341,0x48);// VTS
	OV9726MIPI_write_cmos_sensor(0x0342,0x06);// HTS = 1664
	OV9726MIPI_write_cmos_sensor(0x0343,0x80);// HTS
	//OV9726MIPI_write_cmos_sensor(0x0202,0x03);// exposure high
	//OV9726MIPI_write_cmos_sensor(0x0203,0x43);// exposure low
	OV9726MIPI_write_cmos_sensor(0x3833,0x04);// ISP X start
	OV9726MIPI_write_cmos_sensor(0x3835,0x02);// ISP Y start
	OV9726MIPI_write_cmos_sensor(0x4702,0x04);// DVP timing
	OV9726MIPI_write_cmos_sensor(0x4704,0x00);// DVP timing
	OV9726MIPI_write_cmos_sensor(0x4706,0x08);// DVP timing
	OV9726MIPI_write_cmos_sensor(0x5052,0x01);// ISP control
	OV9726MIPI_write_cmos_sensor(0x3819,0x6e);// timing
	OV9726MIPI_write_cmos_sensor(0x3817,0x94);// timing
	OV9726MIPI_write_cmos_sensor(0x3a18,0x00);// AEC max gain[8]
	OV9726MIPI_write_cmos_sensor(0x3a19,0x7f);// AEC max gain[7:0]
	OV9726MIPI_write_cmos_sensor(0x404e,0x7e);
	OV9726MIPI_write_cmos_sensor(0x3631,0x52);// analog control
	OV9726MIPI_write_cmos_sensor(0x3633,0x50);// analog control
	OV9726MIPI_write_cmos_sensor(0x3630,0xd2);// analog control
	OV9726MIPI_write_cmos_sensor(0x3604,0x08);//analog control
	OV9726MIPI_write_cmos_sensor(0x3601,0x40); //analog control
	OV9726MIPI_write_cmos_sensor(0x3602,0x14); //analog control
	OV9726MIPI_write_cmos_sensor(0x3610,0xa0);// analog control
	OV9726MIPI_write_cmos_sensor(0x3612,0x20);// analog control
	OV9726MIPI_write_cmos_sensor(0x0303,0x02);// system clock divider
	OV9726MIPI_write_cmos_sensor(0x0101,0x01);// flip off, mirror on
	OV9726MIPI_write_cmos_sensor(0x3707,0x14);// timing
	OV9726MIPI_write_cmos_sensor(0x3622,0x9f);// analog control
	OV9726MIPI_write_cmos_sensor(0x5047,0x63);// Lenc on, BIST off, OTP disable, AWB gain on, BLC on
	OV9726MIPI_write_cmos_sensor(0x4002,0x45 );//BLC auto, bit[6]
	OV9726MIPI_write_cmos_sensor(0x4005,0x18); //output black line disable, bit[4]
	OV9726MIPI_write_cmos_sensor(0x3a0f,0x64);// AEC stable in high
	OV9726MIPI_write_cmos_sensor(0x3a10,0x54);// AEC stable in low
	OV9726MIPI_write_cmos_sensor(0x3a11,0xc2);// fast zone high
	OV9726MIPI_write_cmos_sensor(0x3a1b,0x64);// AEC stable out high
	OV9726MIPI_write_cmos_sensor(0x3a1e,0x54);// AEC stable out low
	OV9726MIPI_write_cmos_sensor(0x3a1a,0x05);// difference maximum
	OV9726MIPI_write_cmos_sensor(0x3002,0x00);// X start
	OV9726MIPI_write_cmos_sensor(0x3004,0x00); //WPC on, BPC on
	OV9726MIPI_write_cmos_sensor(0x3005,0x00); //AWB on
	OV9726MIPI_write_cmos_sensor(0x4800,0x14);//line start
	OV9726MIPI_write_cmos_sensor(0x4801,0x0f); //AWB gain manual
	OV9726MIPI_write_cmos_sensor(0x4803,0x05); //Len on, BIST off, OTP disable, AWB gain on, BLC on
	OV9726MIPI_write_cmos_sensor(0x4601,0x16);// R gain
	OV9726MIPI_write_cmos_sensor(0x3014,0x05);
	OV9726MIPI_write_cmos_sensor(0x3104,0x20);// G gain

	//OV9726MIPI_write_cmos_sensor(0x0301,0x08);
	OV9726MIPI_write_cmos_sensor(0x0305,0x04);
	OV9726MIPI_write_cmos_sensor(0x0307,0x46);// B gain
	OV9726MIPI_write_cmos_sensor(0x300c,0x02);
	OV9726MIPI_write_cmos_sensor(0x300d,0x20);// manual AEC, manual AGC
	OV9726MIPI_write_cmos_sensor(0x300e,0x01);// exposure high
	OV9726MIPI_write_cmos_sensor(0x3010,0x01);// exposure low
	OV9726MIPI_write_cmos_sensor(0x0345,0x00); //gain
	OV9726MIPI_write_cmos_sensor(0x5051,0x10);
	OV9726MIPI_write_cmos_sensor(0x5000,0x06);
	OV9726MIPI_write_cmos_sensor(0x5001,0x01);
	OV9726MIPI_write_cmos_sensor(0x3406,0x01);
	OV9726MIPI_write_cmos_sensor(0x5047,0x63);
	OV9726MIPI_write_cmos_sensor(0x3400,0x04);
	OV9726MIPI_write_cmos_sensor(0x3401,0x00);
	OV9726MIPI_write_cmos_sensor(0x3402,0x04);
	OV9726MIPI_write_cmos_sensor(0x3403,0x00);
	OV9726MIPI_write_cmos_sensor(0x3404,0x04);
	OV9726MIPI_write_cmos_sensor(0x3405,0x00);
	OV9726MIPI_write_cmos_sensor(0x3503,0x17);
	//OV9726MIPI_write_cmos_sensor(0x5001,0x01);
	//OV9726MIPI_write_cmos_sensor(0x0202,0x00);
	//OV9726MIPI_write_cmos_sensor(0x0203,0xd0);
	OV9726MIPI_write_cmos_sensor(0x0205,0x3f);

	OV9726MIPI_write_cmos_sensor(0x0100,0x01); //stream on

	OV9726MIPI_AutoFlicker_Mode=KAL_FALSE;

	
}

void OV9726MIPI_set_720P(void)
{

	SENSORDB("720P init\n");
	OV9726MIPI_write_cmos_sensor(0x0100,0x00);//stream off
	OV9726MIPI_write_cmos_sensor(0x0303,0x01);//pll
	OV9726MIPI_write_cmos_sensor(0x0340,0x03);//VTS=910//Tonyli
	OV9726MIPI_write_cmos_sensor(0x0341,0x48);//VTS
	OV9726MIPI_write_cmos_sensor(0x0342,0x06);//HTS=1664
	OV9726MIPI_write_cmos_sensor(0x0343,0x80);//HTS
	OV9726MIPI_write_cmos_sensor(0x0344,0x00);//HStart=1
	OV9726MIPI_write_cmos_sensor(0x0345,0x01);//HStart_l
	OV9726MIPI_write_cmos_sensor(0x0346,0x00);//VStart=40
	OV9726MIPI_write_cmos_sensor(0x0347,0x28);//VStart_l
	OV9726MIPI_write_cmos_sensor(0x0348,0x05);//HEnd=1282
	OV9726MIPI_write_cmos_sensor(0x0349,0x0f);//HEnd_l
	OV9726MIPI_write_cmos_sensor(0x034a,0x03);//VEnd=807
	OV9726MIPI_write_cmos_sensor(0x034b,0x27);//VEnd_l
	OV9726MIPI_write_cmos_sensor(0x034c,0x05);//Image_Width=1280
	OV9726MIPI_write_cmos_sensor(0x034d,0x00);//Image_Width_l     //0x00
	OV9726MIPI_write_cmos_sensor(0x034e,0x02);//Image_Height=720
	OV9726MIPI_write_cmos_sensor(0x034f,0xd0);//Image_Height_l   //0xd0
	//OV9726MIPI_write_cmos_sensor(0x0202,0x03);//coarse exposure time
	//OV9726MIPI_write_cmos_sensor(0x0203,0x43);//coarse exposure time
	OV9726MIPI_write_cmos_sensor(0x3a02,0x19);//AEC 60hz max exposure limit
	OV9726MIPI_write_cmos_sensor(0x3a03,0xc0);//AEC 60hz max exposure limit
	OV9726MIPI_write_cmos_sensor(0x3a0a,0x00);//AEC exposure step for avoid 60hz banding
	OV9726MIPI_write_cmos_sensor(0x3a0b,0xce);//AEC exposure step for avoid 60hz banding
	OV9726MIPI_write_cmos_sensor(0x3a0d,0x04);//AEC max step times for 60hz
	OV9726MIPI_write_cmos_sensor(0x3a14,0x17);//AEC 50hz max exposure limit
	OV9726MIPI_write_cmos_sensor(0x3a15,0x28);//AEC 50hz max exposure limit
	OV9726MIPI_write_cmos_sensor(0x3a08,0x00);//AEC exposure step for avoid 50hz banding
	OV9726MIPI_write_cmos_sensor(0x3a09,0xf7);//AEC exposure step for avoid 50hz banding
	OV9726MIPI_write_cmos_sensor(0x3a0e,0x03);//AEC max step times for 50hz
	OV9726MIPI_write_cmos_sensor(0x0387,0x01);
	OV9726MIPI_write_cmos_sensor(0x381a,0x00);//HBinning disable,Vbinning disable
	OV9726MIPI_write_cmos_sensor(0x5050,0x10);//debug mode
	OV9726MIPI_write_cmos_sensor(0x5002,0x00);//Subsample disable
	OV9726MIPI_write_cmos_sensor(0x5901,0x00);//No variopixel
	OV9726MIPI_write_cmos_sensor(0x46e0,0x81);
	OV9726MIPI_write_cmos_sensor(0x0100,0x01);//streaming on

}



static void OV9726MIPI_HVMirror(kal_uint8 image_mirror)
{
	image_mirror=IMAGE_H_MIRROR;
	SENSORDB("OV9726MIPI HVMirror:%d\n",image_mirror);
	
	switch (image_mirror)
    {
    case IMAGE_NORMAL:
        OV9726MIPI_write_cmos_sensor(0x0101,0x00);
		OV9726MIPI_write_cmos_sensor(0x0347,0x00);
		OV9726MIPI_write_cmos_sensor(0x034b,0x27);
     break;
    case IMAGE_H_MIRROR:
        OV9726MIPI_write_cmos_sensor(0x0101,0x01);
		OV9726MIPI_write_cmos_sensor(0x0347,0x00);
		OV9726MIPI_write_cmos_sensor(0x034b,0x27);
      break;
    case IMAGE_V_MIRROR:
        OV9726MIPI_write_cmos_sensor(0x0101,0x02);
		OV9726MIPI_write_cmos_sensor(0x0347,0x01);
		OV9726MIPI_write_cmos_sensor(0x034b,0x27);

         break;
    case IMAGE_HV_MIRROR:
        OV9726MIPI_write_cmos_sensor(0x0101,0x03);
		OV9726MIPI_write_cmos_sensor(0x0347,0x01);
		OV9726MIPI_write_cmos_sensor(0x034b,0x27);
      break;
    default:
    ASSERT(0);
    }

}


UINT32 OV9726SetTestPatternMode(kal_bool bEnable)
{
    SENSORDB("[OV9726SetTestPatternMode] Test pattern enable:%d\n", bEnable);
	if(bEnable)
		OV9726MIPI_write_cmos_sensor(0x0601,0x02);
	else
		OV9726MIPI_write_cmos_sensor(0x0601,0x00);

    return ERROR_NONE;
}



UINT32 OV9726MIPIGetDefaultFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId,MUINT32 *pframeRate)
{
	switch(scenarioId)
	{
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
			*pframeRate=300;
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


UINT32 OV9726MIPISetMaxFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 frameRate)
{
	kal_int16 dummyLine=0;
	SENSORDB("OV9726MIPISetMaxFramerateByScenario scenarioId=%d,frameRate=%d",scenarioId,frameRate);
	switch(scenarioId)
	{
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
			dummyLine=10*OV9726MIPI_sensor.pclk/frameRate/OV9726MIPI_sensor.line_length-OV9726MIPI_sensor.frame_height;
			if(dummyLine<=0)
				dummyLine=0;
			OV9726MIPI_Set_Dummy(0,dummyLine);
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




/*****************************************************************************/
/* Windows Mobile Sensor Interface */
/*****************************************************************************/
/*************************************************************************
* FUNCTION
*	OV9726MIPIOpen
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

UINT32 OV9726MIPIOpen(void)
{
 kal_uint32 id=0;
 if(OV9726MIPI_SENSOR_ID!=OV9726MIPI_sensor_id)
 	{    	 
		id= ((OV9726MIPI_read_cmos_sensor(0x0000) << 8) | OV9726MIPI_read_cmos_sensor(0x0001));
		spin_lock(&ov9726mipiraw_drv_lock);
        OV9726MIPI_sensor_id=id;
		spin_unlock(&ov9726mipiraw_drv_lock);
    	if (OV9726MIPI_sensor_id != OV9726MIPI_SENSOR_ID)
		{
    		return ERROR_SENSOR_CONNECT_FAIL;
        }
 	}
    SENSORDB("OV9726MIPI Sensor Read ID:0x%04x OK\n",OV9726MIPI_sensor_id);
    OV9726MIPI_init();
	OV9726MIPI_set_720P();
    spin_lock(&ov9726mipiraw_drv_lock);
	OV9726MIPI_sensor.pv_mode=KAL_TRUE;
	OV9726MIPI_sensor.shutter=0x100;
	spin_unlock(&ov9726mipiraw_drv_lock);
    return ERROR_NONE;
}


UINT32 OV9726MIPIGetSensorID(UINT32 *sensorID)
{
	int  retry = 3; 
	SENSORDB("Get sensor id start!\n");
	OV9726MIPI_write_cmos_sensor(0x0103,0x01);
    // check if sensor ID correct
    do {
        *sensorID = ((OV9726MIPI_read_cmos_sensor(0x0000) << 8) | OV9726MIPI_read_cmos_sensor(0x0001));
		spin_lock(&ov9726mipiraw_drv_lock);
		OV9726MIPI_sensor_id=*sensorID;
		spin_unlock(&ov9726mipiraw_drv_lock);
        if (*sensorID == OV9726MIPI_SENSOR_ID)
            break; 
        SENSORDB("Read Sensor ID Fail = 0x%04x\n", *sensorID); 
        retry--; 
    } while (retry > 0);

    if (*sensorID != OV9726MIPI_SENSOR_ID) {
        *sensorID = 0xFFFFFFFF; 
        return ERROR_SENSOR_CONNECT_FAIL;
    }
	SENSORDB("Get sensor id sccuss!\n");
    return ERROR_NONE;
}


/*************************************************************************
* FUNCTION
*	OV9726MIPIClose
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
UINT32 OV9726MIPIClose(void)
{

   SENSORDB("OV9726MIPIClose\n");

  //CISModulePowerOn(FALSE);
//	DRV_I2CClose(OV9726MIPIhDrvI2C);
	return ERROR_NONE;
}   /* OV9726MIPIClose */

/*************************************************************************
* FUNCTION
* OV9726MIPIPreview
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
UINT32 OV9726MIPIPreview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint16 dummy_line;

	SENSORDB("OV9726MIPIPreview \n");
	OV9726MIPI_set_720P();

	spin_lock(&ov9726mipiraw_drv_lock);
	OV9726MIPI_sensor.pv_mode = KAL_TRUE;
	spin_unlock(&ov9726mipiraw_drv_lock);
	switch (sensor_config_data->SensorOperationMode)
	{
	  case MSDK_SENSOR_OPERATION_MODE_VIDEO: 
	    spin_lock(&ov9726mipiraw_drv_lock);
		OV9726MIPI_sensor.video_mode = KAL_TRUE;
		spin_unlock(&ov9726mipiraw_drv_lock);		

		SENSORDB("Video mode \n");
		break;

	  default: /* ISP_PREVIEW_MODE */
		  spin_lock(&ov9726mipiraw_drv_lock);
		OV9726MIPI_sensor.video_mode = KAL_FALSE;
		spin_unlock(&ov9726mipiraw_drv_lock);

		SENSORDB("Camera preview mode \n");
		break;

	}
	OV9726MIPI_HVMirror(sensor_config_data->SensorImageMirror);
	dummy_line = 12;
/*	
	dummy_line= OV9726MIPI_sensor.pclk * OV9726MIPI_FPS(1) / 
		((OV9726MIPI_FULL_PERIOD_PIXEL_NUMS+OV9726MIPI_DEFAULT_DUMMY_PIXELS) * OV9726MIPI_FPS(20));
	
	dummy_line = dummy_line< (OV9726MIPI_FULL_PERIOD_LINE_NUMS+OV9726MIPI_DEFAULT_DUMMY_LINES) ? 
		0 : dummy_line- (OV9726MIPI_FULL_PERIOD_LINE_NUMS+OV9726MIPI_DEFAULT_DUMMY_LINES);
	*/
	OV9726MIPI_Set_Dummy(0, dummy_line); /* modify dummy_pixel must gen AE table again */
//	OV9726MIPI_Write_Shutter(OV9726MIPI_sensor.shutter);	 
/*
	startx+=OV9726MIPI_PV_X_START;
	starty+=OV9726MIPI_PV_Y_START;
	image_window->GrabStartX= startx;
    image_window->GrabStartY= starty;
    image_window->ExposureWindowWidth= OV9726MIPI_IMAGE_SENSOR_PV_WIDTH - 2*startx;
    image_window->ExposureWindowHeight= OV9726MIPI_IMAGE_SENSOR_PV_HEIGHT - 2*starty;
   */
	return ERROR_NONE;
	
}   /*  OV9726MIPIPreview   */

/*************************************************************************
* FUNCTION
*	OV9726MIPICapture
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
UINT32 OV9726MIPICapture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
						  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	
	//const kal_uint32 pv_pclk = OV9726MIPI_sensor.pclk;
	kal_uint16 dummy_pixel=0,dummy_line=0;
	kal_uint16 startx=0,starty=0;

	SENSORDB("OV9726MIPICapture \n");
	if(CurrentScenarioId==MSDK_SCENARIO_ID_CAMERA_ZSD)
	{
		spin_lock(&ov9726mipiraw_drv_lock);
   		OV9726MIPI_sensor.video_mode=KAL_FALSE;
		OV9726MIPI_AutoFlicker_Mode=KAL_TRUE;
		OV9726MIPI_sensor.pv_mode = KAL_FALSE;
		spin_unlock(&ov9726mipiraw_drv_lock);
	}
	else
	

	spin_lock(&ov9726mipiraw_drv_lock);
    OV9726MIPI_sensor.video_mode=KAL_FALSE;
	OV9726MIPI_AutoFlicker_Mode=KAL_FALSE;
	OV9726MIPI_sensor.pv_mode = KAL_FALSE;
	spin_unlock(&ov9726mipiraw_drv_lock);

	
	OV9726MIPI_HVMirror(sensor_config_data->SensorImageMirror);


		#if 0
		cap_fps = OV9726MIPI_FPS(13);
		
		dummy_pixel= OV9726MIPI_sensor.pclk * OV9726MIPI_FPS(1) / ((OV9726MIPI_FULL_PERIOD_LINE_NUMS+OV9726MIPI_DEFAULT_DUMMY_LINES) * cap_fps);
		dummy_pixel = dummy_pixel< (OV9726MIPI_FULL_PERIOD_PIXEL_NUMS+OV9726MIPI_DEFAULT_DUMMY_PIXELS) ? 0 : dummy_pixel- (OV9726MIPI_FULL_PERIOD_PIXEL_NUMS+OV9726MIPI_DEFAULT_DUMMY_PIXELS);
		#endif
	dummy_line = 12;
	OV9726MIPI_Set_Dummy(dummy_pixel, dummy_line);
	startx+=OV9726MIPI_FULL_X_START;
	starty+=OV9726MIPI_FULL_Y_START;
	image_window->GrabStartX= startx;
    image_window->GrabStartY= starty;
    image_window->ExposureWindowWidth= OV9726MIPI_IMAGE_SENSOR_PV_WIDTH - 2*startx;
    image_window->ExposureWindowHeight= OV9726MIPI_IMAGE_SENSOR_PV_HEIGHT - 2*starty;
	

	return ERROR_NONE;
}   /* OV9726MIPI_Capture() */

UINT32 OV9726MIPIGetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution)
{

		
	    pSensorResolution->SensorPreviewWidth=OV9726MIPI_IMAGE_SENSOR_PV_WIDTH;
	    pSensorResolution->SensorPreviewHeight=OV9726MIPI_IMAGE_SENSOR_PV_HEIGHT;
		pSensorResolution->SensorFullWidth=OV9726MIPI_IMAGE_SENSOR_FULL_WIDTH;
		pSensorResolution->SensorFullHeight=OV9726MIPI_IMAGE_SENSOR_FULL_HEIGHT;
		pSensorResolution->SensorVideoWidth=OV9726MIPI_IMAGE_SENSOR_PV_WIDTH;
		pSensorResolution->SensorVideoHeight=OV9726MIPI_IMAGE_SENSOR_PV_HEIGHT;
		pSensorResolution->Sensor3DFullWidth=OV9726MIPI_IMAGE_SENSOR_PV_WIDTH;
		pSensorResolution->Sensor3DFullHeight=OV9726MIPI_IMAGE_SENSOR_PV_HEIGHT;	
		pSensorResolution->Sensor3DPreviewWidth=OV9726MIPI_IMAGE_SENSOR_PV_WIDTH;
		pSensorResolution->Sensor3DPreviewHeight=OV9726MIPI_IMAGE_SENSOR_PV_HEIGHT;		
		pSensorResolution->Sensor3DVideoWidth=OV9726MIPI_IMAGE_SENSOR_PV_WIDTH;
		pSensorResolution->Sensor3DVideoHeight=OV9726MIPI_IMAGE_SENSOR_PV_HEIGHT;
		SENSORDB("OV9726MIPIGetResolution:%d,%d \n",pSensorResolution->SensorPreviewWidth,pSensorResolution->SensorFullWidth);
	return ERROR_NONE;
}	/* OV9726MIPIGetResolution() */

UINT32 OV9726MIPIGetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId,
					  MSDK_SENSOR_INFO_STRUCT *pSensorInfo,
					  MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{

	SENSORDB("OV9726MIPIGetInfo ScenarioId:%d\n",ScenarioId);


    switch(ScenarioId)
    {
    	case MSDK_SCENARIO_ID_CAMERA_ZSD:
			pSensorInfo->SensorPreviewResolutionX=OV9726MIPI_IMAGE_SENSOR_PV_WIDTH;
			pSensorInfo->SensorPreviewResolutionY=OV9726MIPI_IMAGE_SENSOR_PV_HEIGHT;
			pSensorInfo->SensorCameraPreviewFrameRate=15;
			break;
		default:			
			pSensorInfo->SensorPreviewResolutionX=OV9726MIPI_IMAGE_SENSOR_PV_WIDTH;
			pSensorInfo->SensorPreviewResolutionY=OV9726MIPI_IMAGE_SENSOR_PV_HEIGHT;
			pSensorInfo->SensorCameraPreviewFrameRate=30;
		    break;
			
    }

	pSensorInfo->SensorFullResolutionX=OV9726MIPI_IMAGE_SENSOR_FULL_WIDTH;
	pSensorInfo->SensorFullResolutionY=OV9726MIPI_IMAGE_SENSOR_FULL_HEIGHT;


	pSensorInfo->SensorVideoFrameRate=30;
	pSensorInfo->SensorStillCaptureFrameRate=10;
	pSensorInfo->SensorWebCamCaptureFrameRate=15;
	pSensorInfo->SensorResetActiveHigh=TRUE; //low active
	pSensorInfo->SensorResetDelayCount=5; 

	pSensorInfo->SensorOutputDataFormat=OV9726MIPI_COLOR_FORMAT;
	pSensorInfo->SensorClockPolarity=SENSOR_CLOCK_POLARITY_LOW;	
	pSensorInfo->SensorClockFallingPolarity=SENSOR_CLOCK_POLARITY_LOW;
	pSensorInfo->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	pSensorInfo->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;

	pSensorInfo->SensorInterruptDelayLines = 4;
	pSensorInfo->SensroInterfaceType=SENSOR_INTERFACE_TYPE_MIPI;
    
	pSensorInfo->CaptureDelayFrame = 2; 
	pSensorInfo->PreviewDelayFrame = 2; 
	pSensorInfo->VideoDelayFrame = 5; 	

	pSensorInfo->SensorMasterClockSwitch = 0; 
    pSensorInfo->SensorDrivingCurrent = ISP_DRIVING_4MA; 
    pSensorInfo->AEShutDelayFrame = 0;		   /* The frame of setting shutter default 0 for TG int */
	pSensorInfo->AESensorGainDelayFrame = 0;	   /* The frame of setting sensor gain */
	pSensorInfo->AEISPGainDelayFrame = 2; 
	//pSensorInfo->AEISPGainDelayFrame = 1;    
	switch (ScenarioId)
	{
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			pSensorInfo->SensorClockFreq=24;
			pSensorInfo->SensorClockDividCount=	3;
			pSensorInfo->SensorClockRisingCount= 0;
			pSensorInfo->SensorClockFallingCount= 2;
			pSensorInfo->SensorPixelClockCount= 3;
			pSensorInfo->SensorDataLatchCount= 2;
			pSensorInfo->SensorGrabStartX = 0; 
			pSensorInfo->SensorGrabStartY = 0; 
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
			pSensorInfo->SensorGrabStartX = 0; 
			pSensorInfo->SensorGrabStartY = 0; 
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
			pSensorInfo->SensorGrabStartX = 0; 
			pSensorInfo->SensorGrabStartY = 0; 
			pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_2_LANE;			
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0; 
	        pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 14; 
	        pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorWidthSampling = 0;  // 0 is default 1x
            pSensorInfo->SensorHightSampling = 0;   // 0 is default 1x 
            pSensorInfo->SensorPacketECCOrder = 1;
		break;
	}
#if 0
	OV9726MIPIPixelClockDivider=pSensorInfo->SensorPixelClockCount;
	memcpy(pSensorConfigData, &OV9726MIPISensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));
#endif		
  return ERROR_NONE;
}	/* OV9726MIPIGetInfo() */


UINT32 OV9726MIPIControl(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow,
					  MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{

	SENSORDB("OV9726MIPIControl ScenarioId:%d\n",ScenarioId);
	CurrentScenarioId=ScenarioId;
		
	switch (ScenarioId)
	{
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			OV9726MIPIPreview(pImageWindow, pSensorConfigData);
		break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		 case MSDK_SCENARIO_ID_CAMERA_ZSD:
		 	SENSORDB("MSDK_SCENARIO_ID_CAMERA_ZSD\n");


			OV9726MIPICapture(pImageWindow, pSensorConfigData);

		break;		
        default:
            return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}	/* OV9726MIPIControl() */

UINT32 OV9726MIPISetVideoMode(UINT16 u2FrameRate)
{
	kal_int16 dummy_line;
    /* to fix VSYNC, to fix frame rate */

	SENSORDB("OV9726MIPISetVideoMode u2FrameRate:%d\n",u2FrameRate);
	if(u2FrameRate==0)
		return TRUE;
	dummy_line = OV9726MIPI_sensor.pclk / u2FrameRate / (OV9726MIPI_PV_PERIOD_PIXEL_NUMS+OV9726MIPI_DEFAULT_DUMMY_PIXELS) - (OV9726MIPI_PV_PERIOD_LINE_NUMS+OV9726MIPI_DEFAULT_DUMMY_LINES);
	if (dummy_line < 12) dummy_line = 12;

	SENSORDB("dummy_line %d\n", dummy_line);

	OV9726MIPI_Set_Dummy(0, dummy_line); /* modify dummy_pixel must gen AE table again */
	spin_lock(&ov9726mipiraw_drv_lock);
	OV9726MIPI_sensor.video_mode = KAL_TRUE;
	spin_unlock(&ov9726mipiraw_drv_lock);
    return ERROR_NONE;
}

UINT32 OV9726MIPISetAutoFlickerMode(kal_bool bEnable,UINT16 u2FrameRate)
{
	kal_uint32 ov9726mipi_pv_max_lines=OV9726MIPI_sensor.default_height;
	SENSORDB("OV9726MIPISetAutoFlickerMode bEnable=%d,u2FrameRate=%d\n",bEnable,u2FrameRate);
	if(bEnable)
		{
			spin_lock(&ov9726mipiraw_drv_lock);
		   OV9726MIPI_AutoFlicker_Mode=KAL_TRUE;
		   spin_unlock(&ov9726mipiraw_drv_lock);
		   if(OV9726MIPI_sensor.video_mode==KAL_TRUE)
		   	{
		   	   ov9726mipi_pv_max_lines=ov9726mipi_pv_max_lines+(ov9726mipi_pv_max_lines>>7);
			   OV9726MIPI_write_cmos_sensor(0x0340, ov9726mipi_pv_max_lines >> 8);
               OV9726MIPI_write_cmos_sensor(0x0341, ov9726mipi_pv_max_lines & 0xFF);
		   	}
		}
	else
		{
			spin_lock(&ov9726mipiraw_drv_lock);
		   OV9726MIPI_AutoFlicker_Mode=KAL_FALSE;
		   spin_unlock(&ov9726mipiraw_drv_lock);
		   if(OV9726MIPI_sensor.video_mode==KAL_TRUE)
		   	{
		   	   OV9726MIPI_write_cmos_sensor(0x0340, ov9726mipi_pv_max_lines >> 8);
               OV9726MIPI_write_cmos_sensor(0x0341, ov9726mipi_pv_max_lines & 0xFF);
		   	}
		}
 return ERROR_NONE;
}



UINT32 OV9726MIPIFeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId,
							 UINT8 *pFeaturePara,UINT32 *pFeatureParaLen)
{
	UINT16 *pFeatureReturnPara16=(UINT16 *) pFeaturePara;
	UINT16 *pFeatureData16=(UINT16 *) pFeaturePara;
	UINT32 *pFeatureReturnPara32=(UINT32 *) pFeaturePara;
	UINT32 *pFeatureData32=(UINT32 *) pFeaturePara;
	MSDK_SENSOR_REG_INFO_STRUCT *pSensorRegData=(MSDK_SENSOR_REG_INFO_STRUCT *) pFeaturePara;
	

	SENSORDB("OV9726MIPIFeatureControl£¬FeatureId:%d\n",FeatureId);
		
	switch (FeatureId)
	{
		case SENSOR_FEATURE_GET_RESOLUTION:
			*pFeatureReturnPara16++=OV9726MIPI_IMAGE_SENSOR_FULL_WIDTH;
			*pFeatureReturnPara16=OV9726MIPI_IMAGE_SENSOR_FULL_HEIGHT;
			*pFeatureParaLen=4;
		break;
		case SENSOR_FEATURE_GET_PERIOD:	/* 3 */

			 switch(CurrentScenarioId)
			 {
			 	case MSDK_SCENARIO_ID_CAMERA_ZSD:
					*pFeatureReturnPara16++=OV9726MIPI_sensor.line_length;
					*pFeatureReturnPara16=OV9726MIPI_sensor.frame_height;
					*pFeatureParaLen=4;
					break;
				default:
					*pFeatureReturnPara16++=OV9726MIPI_sensor.line_length;
					*pFeatureReturnPara16=OV9726MIPI_sensor.frame_height;
					*pFeatureParaLen=4;
					break;
					
			 }

		break;
		case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:  /* 3 */

			switch(CurrentScenarioId)
			 {
			 	case MSDK_SCENARIO_ID_CAMERA_ZSD:
					*pFeatureReturnPara32 = OV9726MIPI_sensor.pclk;
					*pFeatureParaLen=4;
					break;
				default:
					*pFeatureReturnPara32 = OV9726MIPI_sensor.pclk;
					*pFeatureParaLen=4;
					break;					
			 }

			break;
		case SENSOR_FEATURE_SET_ESHUTTER:	/* 4 */
			set_OV9726MIPI_shutter(*pFeatureData16);
		break;
		case SENSOR_FEATURE_SET_NIGHTMODE:
			//OV9726MIPI_night_mode((BOOL) *pFeatureData16);
		
		break;
		case SENSOR_FEATURE_SET_GAIN:	/* 6 */
			OV9726MIPI_SetGain((UINT16) *pFeatureData16);
		break;
		case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
		case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
		case SENSOR_FEATURE_SET_REGISTER:
		OV9726MIPI_write_cmos_sensor(pSensorRegData->RegAddr, pSensorRegData->RegData);
		break;
		case SENSOR_FEATURE_GET_REGISTER:
			pSensorRegData->RegData = OV9726MIPI_read_cmos_sensor(pSensorRegData->RegAddr);
		break;
		case SENSOR_FEATURE_SET_CCT_REGISTER:
			memcpy(&OV9726MIPI_sensor.eng.cct, pFeaturePara, sizeof(OV9726MIPI_sensor.eng.cct));
			break;
		break;
		case SENSOR_FEATURE_GET_CCT_REGISTER:	/* 12 */
			if (*pFeatureParaLen >= sizeof(OV9726MIPI_sensor.eng.cct) + sizeof(kal_uint32))
			{
			  *((kal_uint32 *)pFeaturePara++) = sizeof(OV9726MIPI_sensor.eng.cct);
			  memcpy(pFeaturePara, &OV9726MIPI_sensor.eng.cct, sizeof(OV9726MIPI_sensor.eng.cct));
			}
			break;
		case SENSOR_FEATURE_SET_ENG_REGISTER:
			memcpy(&OV9726MIPI_sensor.eng.reg, pFeaturePara, sizeof(OV9726MIPI_sensor.eng.reg));
			break;
		case SENSOR_FEATURE_GET_ENG_REGISTER:	/* 14 */
			if (*pFeatureParaLen >= sizeof(OV9726MIPI_sensor.eng.reg) + sizeof(kal_uint32))
			{
			  *((kal_uint32 *)pFeaturePara++) = sizeof(OV9726MIPI_sensor.eng.reg);
			  memcpy(pFeaturePara, &OV9726MIPI_sensor.eng.reg, sizeof(OV9726MIPI_sensor.eng.reg));
			}
		case SENSOR_FEATURE_GET_REGISTER_DEFAULT:
			((PNVRAM_SENSOR_DATA_STRUCT)pFeaturePara)->Version = NVRAM_CAMERA_SENSOR_FILE_VERSION;
			((PNVRAM_SENSOR_DATA_STRUCT)pFeaturePara)->SensorId = OV9726MIPI_SENSOR_ID;
			memcpy(((PNVRAM_SENSOR_DATA_STRUCT)pFeaturePara)->SensorEngReg, &OV9726MIPI_sensor.eng.reg, sizeof(OV9726MIPI_sensor.eng.reg));
			memcpy(((PNVRAM_SENSOR_DATA_STRUCT)pFeaturePara)->SensorCCTReg, &OV9726MIPI_sensor.eng.cct, sizeof(OV9726MIPI_sensor.eng.cct));
			*pFeatureParaLen = sizeof(NVRAM_SENSOR_DATA_STRUCT);
			break;
		case SENSOR_FEATURE_GET_CONFIG_PARA:
			memcpy(pFeaturePara, &OV9726MIPI_sensor.cfg_data, sizeof(OV9726MIPI_sensor.cfg_data));
			*pFeatureParaLen = sizeof(OV9726MIPI_sensor.cfg_data);
			break;
		case SENSOR_FEATURE_CAMERA_PARA_TO_SENSOR:
		     OV9726MIPI_camera_para_to_sensor();
		break;
		case SENSOR_FEATURE_SENSOR_TO_CAMERA_PARA:
			OV9726MIPI_sensor_to_camera_para();
		break;							
		case SENSOR_FEATURE_GET_GROUP_COUNT:
			OV9726MIPI_get_sensor_group_count((kal_uint32 *)pFeaturePara);
			*pFeatureParaLen = 4;
		break;										
		  OV9726MIPI_get_sensor_group_info((MSDK_SENSOR_GROUP_INFO_STRUCT *)pFeaturePara);
		  *pFeatureParaLen = sizeof(MSDK_SENSOR_GROUP_INFO_STRUCT);
		  break;
		case SENSOR_FEATURE_GET_ITEM_INFO:
		  OV9726MIPI_get_sensor_item_info((MSDK_SENSOR_ITEM_INFO_STRUCT *)pFeaturePara);
		  *pFeatureParaLen = sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
		  break;
		case SENSOR_FEATURE_SET_ITEM_INFO:
		  OV9726MIPI_set_sensor_item_info((MSDK_SENSOR_ITEM_INFO_STRUCT *)pFeaturePara);
		  *pFeatureParaLen = sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
		  break;
		case SENSOR_FEATURE_GET_ENG_INFO:
     		memcpy(pFeaturePara, &OV9726MIPI_sensor.eng_info, sizeof(OV9726MIPI_sensor.eng_info));
     		*pFeatureParaLen = sizeof(OV9726MIPI_sensor.eng_info);
     		break;
		case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
			// get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE
			// if EEPROM does not exist in camera module.
			*pFeatureReturnPara32=LENS_DRIVER_ID_DO_NOT_CARE;
			*pFeatureParaLen=4;
		break;
		case SENSOR_FEATURE_SET_VIDEO_MODE:
		       OV9726MIPISetVideoMode(*pFeatureData16);
        break; 

		case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
			OV9726MIPISetAutoFlickerMode((BOOL)*pFeatureData16,*(pFeatureData16+1));
			break;

        case SENSOR_FEATURE_CHECK_SENSOR_ID:
            OV9726MIPIGetSensorID(pFeatureReturnPara32); 
            break;
		case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
			OV9726MIPISetMaxFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32,*(pFeatureData32+1));
			break;
		case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
			OV9726MIPIGetDefaultFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32,(MUINT32 *)(*(pFeatureData32+1)));
			break;
		case SENSOR_FEATURE_SET_TEST_PATTERN:
            OV9726SetTestPatternMode((BOOL)*pFeatureData16);
            break;
		case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:			
			*pFeatureReturnPara32=OV9726_TEST_PATTERN_CHECKSUM;			
			*pFeatureParaLen=4;			
			
			break;
		default:
			break;
	}
	return ERROR_NONE;
}	/* OV9726MIPIFeatureControl() */
SENSOR_FUNCTION_STRUCT    SensorFuncOV9726MIPI=
{
  OV9726MIPIOpen,
  OV9726MIPIGetInfo,
  OV9726MIPIGetResolution,
  OV9726MIPIFeatureControl,
  OV9726MIPIControl,
  OV9726MIPIClose
};

UINT32 OV9726MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{

  /* To Do : Check Sensor status here */
  if (pfFunc!=NULL)
      *pfFunc=&SensorFuncOV9726MIPI;

  return ERROR_NONE;
} /* SensorInit() */



