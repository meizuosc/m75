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

#include "ov2722mipiraw_Sensor.h"
#include "ov2722mipiraw_Camera_Sensor_para.h"
#include "ov2722mipiraw_CameraCustomized.h"

#if 1
#define SENSORDB printk
#else
#define SENSORDB(x,...)
#endif
//#define ACDK
extern int iReadRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u8 * a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u16 i2cId);


static OV2722MIPI_sensor_struct OV2722MIPI_sensor =
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
    .SensorOutputDataFormat = OV2722MIPI_COLOR_FORMAT,
  },
  .shutter = 0x20,  
  .gain = 0x20,
  .pclk = OV2722MIPI_PREVIEW_CLK,
  .frame_height = OV2722MIPI_PV_PERIOD_LINE_NUMS+OV2722MIPI_DEFAULT_DUMMY_PIXELS,
  .line_length = OV2722MIPI_PV_PERIOD_PIXEL_NUMS+OV2722MIPI_DEFAULT_DUMMY_LINES,
};

kal_uint16 OV2722MIPI_sensor_id=0;

kal_uint16 OV2722MIPI_state=1;

static DEFINE_SPINLOCK(ov2722mipiraw_drv_lock);

kal_bool OV2722MIPI_AutoFlicker_Mode=KAL_FALSE;


static MSDK_SCENARIO_ID_ENUM CurrentScenarioId=MSDK_SCENARIO_ID_CAMERA_PREVIEW;

kal_uint16 OV2722MIPI_write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
    char puSendCmd[3] = {(char)(addr >> 8) , (char)(addr & 0xFF) ,(char)(para & 0xFF)};
	
	iWriteRegI2C(puSendCmd , 3,OV2722MIPI_WRITE_ID);

}
kal_uint16 OV2722MIPI_read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte=0;
    char puSendCmd[2] = {(char)(addr >> 8) , (char)(addr & 0xFF) };
	iReadRegI2C(puSendCmd , 2, (u8*)&get_byte,1,OV2722MIPI_WRITE_ID);
		
    return get_byte;
}
int OV2722MIPI_Set_VTS_Shutter(kal_uint16 VTS)
{
   OV2722MIPI_write_cmos_sensor(0x380e, VTS >> 8);
   OV2722MIPI_write_cmos_sensor(0x380f, VTS & 0xFF); 
   return 0;
}

static void OV2722MIPI_Write_Shutter(kal_uint16 iShutter)
{
	kal_uint16 shutter_line = 0,current_fps=0;
	unsigned long flags;
	SENSORDB("OV2722MIPI_Write_Shutter:%x \n",iShutter);	    
	if (!iShutter) iShutter = 1; /* avoid 0 */	
	if(iShutter > OV2722MIPI_sensor.default_height-4) 
	{
	    shutter_line=iShutter+4;
	}
	else
	{
	   shutter_line=OV2722MIPI_sensor.default_height;
	}
	current_fps=OV2722MIPI_sensor.pclk/OV2722MIPI_sensor.line_length/shutter_line;
	SENSORDB("CURRENT FPS:%d,OV2722MIPI_sensor.default_height=%d",
		current_fps,OV2722MIPI_sensor.default_height);		 
	if(current_fps==30 || current_fps==15)
	{
	if(OV2722MIPI_AutoFlicker_Mode==TRUE)
		  shutter_line=shutter_line+(shutter_line>>7);
	}
	//OV2722MIPI_Set_VTS_Shutter(shutter_line);
	SENSORDB("shutter_line=%d,shutter_line");	
	OV2722MIPI_write_cmos_sensor(0x380e, shutter_line >> 8);   
    OV2722MIPI_write_cmos_sensor(0x380f, shutter_line & 0xFF); 
	spin_lock_irqsave(&ov2722mipiraw_drv_lock,flags);
	OV2722MIPI_sensor.frame_height=shutter_line;
	spin_unlock_irqrestore(&ov2722mipiraw_drv_lock,flags);
	OV2722MIPI_write_cmos_sensor(0x3502, (iShutter & 0x0f)<<4);
	OV2722MIPI_write_cmos_sensor(0x3501, (iShutter>>4) & 0xFF);
	OV2722MIPI_write_cmos_sensor(0x3500, (iShutter>>12) & 0x0F);		 
	SENSORDB("OV2722MIPI_frame_length:%x \n",OV2722MIPI_sensor.frame_height);		
}   /*  OV2722MIPI_Write_Shutter    */

static void OV2722MIPI_Set_Dummy(const kal_uint16 iPixels, const kal_uint16 iLines)
{
	kal_uint16 hactive, vactive, line_length, frame_height;

	SENSORDB("OV2722MIPI_Set_Dummy:iPixels:%x; iLines:%x \n",iPixels,iLines);
    line_length = OV2722MIPI_FULL_PERIOD_PIXEL_NUMS + iPixels+OV2722MIPI_DEFAULT_DUMMY_PIXELS;
	frame_height = OV2722MIPI_FULL_PERIOD_LINE_NUMS + iLines+OV2722MIPI_DEFAULT_DUMMY_LINES;	
	SENSORDB("line_length:%x; frame_height:%x \n",line_length,frame_height);
	if ((line_length >= 0x1FFF)||(frame_height >= 0xFFF))
	{
		SENSORDB("Warnning: line length or frame height is overflow!!!!!!!!  \n");
		return ;
	}
#if 1  //add by chenqiang 
    spin_lock(&ov2722mipiraw_drv_lock);
	OV2722MIPI_sensor.line_length = line_length;
	OV2722MIPI_sensor.frame_height = frame_height;
	OV2722MIPI_sensor.default_height=frame_height;
	spin_unlock(&ov2722mipiraw_drv_lock);
	SENSORDB("line_length:%x; frame_height:%x \n",line_length,frame_height);
    OV2722MIPI_write_cmos_sensor(0x380c, line_length >> 8);
    OV2722MIPI_write_cmos_sensor(0x380d, line_length & 0xFF);
    OV2722MIPI_write_cmos_sensor(0x380e, frame_height >> 8);
    OV2722MIPI_write_cmos_sensor(0x380f, frame_height & 0xFF);
#endif
	
}   /*  OV2722MIPI_Set_Dummy    */

/*************************************************************************
* FUNCTION
*	OV2722MIPI_SetShutter
*
* DESCRIPTION
*	This function set e-shutter of OV2722MIPI to change exposure time.
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
void set_OV2722MIPI_shutter(kal_uint16 iShutter)
{
#if 1
	unsigned long flags;

	SENSORDB("set_OV2722MIPI_shutter:%x \n",iShutter);

    spin_lock_irqsave(&ov2722mipiraw_drv_lock,flags);
	OV2722MIPI_sensor.shutter = iShutter;
	spin_unlock_irqrestore(&ov2722mipiraw_drv_lock,flags);
    OV2722MIPI_Write_Shutter(iShutter);
#endif
}   /*  Set_OV2722MIPI_Shutter */



static kal_uint16 OV2722MIPIReg2Gain(const kal_uint8 iReg)
{
#if 1
    kal_uint8 iI;
    kal_uint16 iGain = BASEGAIN;    // 1x-gain base

    // Range: 1x to 32x
    // Gain = (GAIN[7] + 1) * (GAIN[6] + 1) * (GAIN[5] + 1) * (GAIN[4] + 1) * (1 + GAIN[3:0] / 16)
    for (iI = 7; iI >= 4; iI--) {
        iGain *= (((iReg >> iI) & 0x01) + 1);
    }

    return iGain +  iGain * (iReg & 0x0F) / 16;
#endif
}

static kal_uint8 OV2722MIPIGain2Reg(const kal_uint16 iGain)
{
	#if 1
	kal_uint16 iReg = 0x00;
	iReg = ((iGain / BASEGAIN) << 4) + ((iGain % BASEGAIN) * 16 / BASEGAIN);
	iReg = iReg & 0xFF;
	return (kal_uint8)iReg;
	#endif
}





/*************************************************************************
* FUNCTION
*	OV2722MIPI_SetGain
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
void OV2722MIPI_SetGain(UINT16 iGain)
{
	#if 1

	kal_uint8 iReg;
	//V5647_sensor.gain = iGain;
	/* 0x350a[0:1], 0x350b AGC real gain */
	/* [0:3] = N meams N /16 X	*/
	/* [4:9] = M meams M X	*/
	/* Total gain = M + N /16 X */
	SENSORDB("OV2722MIPI_SetGain::%x \n",iGain);
	iReg = OV2722MIPIGain2Reg(iGain);
	SENSORDB("OVOV2722MIPI_SetGain,iReg:%x",iReg);
	if (iReg < 0x10) iReg = 0x10;
	//OV5647MIPI_write_cmos_sensor(0x3508, iReg);
	 OV2722MIPI_write_cmos_sensor(0x3509,iReg);
	return iGain;		
	#endif
}	/*	OV2722MIPI_SetGain	*/



/*************************************************************************
* FUNCTION
*	OV2722MIPI_NightMode
*
* DESCRIPTION
*	This function night mode of OV2722MIPI.
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
void OV2722MIPI_night_mode(kal_bool enable)
{
	const kal_uint16 dummy_pixel = OV2722MIPI_sensor.line_length - OV2722MIPI_PV_PERIOD_PIXEL_NUMS;
	const kal_uint16 pv_min_fps =  enable ? OV2722MIPI_sensor.night_fps : OV2722MIPI_sensor.normal_fps;
	kal_uint16 dummy_line = OV2722MIPI_sensor.frame_height - OV2722MIPI_PV_PERIOD_LINE_NUMS;
	kal_uint16 max_exposure_lines;
	

   SENSORDB("OV2722MIPI_night_mode:enable:%x;\n",enable);


	if (!OV2722MIPI_sensor.video_mode) return;
	max_exposure_lines = OV2722MIPI_sensor.pclk * OV2722MIPI_FPS(1) / (pv_min_fps * OV2722MIPI_sensor.line_length);
	if (max_exposure_lines > OV2722MIPI_sensor.frame_height) /* fix max frame rate, AE table will fix min frame rate */
	{
	  dummy_line = max_exposure_lines - OV2722MIPI_PV_PERIOD_LINE_NUMS;

	 SENSORDB("dummy_line:%x;\n",dummy_line);

	  OV2722MIPI_Set_Dummy(dummy_pixel, dummy_line);
	}

}   /*  OV2722MIPI_NightMode    */


/* write camera_para to sensor register */
static void OV2722MIPI_camera_para_to_sensor(void)
{
  kal_uint32 i;

	 SENSORDB("OV2722MIPI_camera_para_to_sensor\n");

  for (i = 0; 0xFFFFFFFF != OV2722MIPI_sensor.eng.reg[i].Addr; i++)
  {
    OV2722MIPI_write_cmos_sensor(OV2722MIPI_sensor.eng.reg[i].Addr, OV2722MIPI_sensor.eng.reg[i].Para);
  }
  for (i = OV2722MIPI_FACTORY_START_ADDR; 0xFFFFFFFF != OV2722MIPI_sensor.eng.reg[i].Addr; i++)
  {
    OV2722MIPI_write_cmos_sensor(OV2722MIPI_sensor.eng.reg[i].Addr, OV2722MIPI_sensor.eng.reg[i].Para);
  }
  OV2722MIPI_SetGain(OV2722MIPI_sensor.gain); /* update gain */
}

/* update camera_para from sensor register */
static void OV2722MIPI_sensor_to_camera_para(void)
{
  kal_uint32 i,temp_data;

   SENSORDB("OV2722MIPI_sensor_to_camera_para\n");

  for (i = 0; 0xFFFFFFFF != OV2722MIPI_sensor.eng.reg[i].Addr; i++)
  {
    
		temp_data= OV2722MIPI_read_cmos_sensor(OV2722MIPI_sensor.eng.reg[i].Addr);
	    spin_lock(&ov2722mipiraw_drv_lock);
		OV2722MIPI_sensor.eng.reg[i].Para =temp_data;
		spin_lock(&ov2722mipiraw_drv_lock);
  }
  for (i = OV2722MIPI_FACTORY_START_ADDR; 0xFFFFFFFF != OV2722MIPI_sensor.eng.reg[i].Addr; i++)
  {
		temp_data= OV2722MIPI_read_cmos_sensor(OV2722MIPI_sensor.eng.reg[i].Addr);
		spin_unlock(&ov2722mipiraw_drv_lock);
		OV2722MIPI_sensor.eng.reg[i].Para=temp_data;
		spin_unlock(&ov2722mipiraw_drv_lock);
  }
}

/* ------------------------ Engineer mode ------------------------ */
inline static void OV2722MIPI_get_sensor_group_count(kal_int32 *sensor_count_ptr)
{

   SENSORDB("OV2722MIPI_get_sensor_group_count\n");

  *sensor_count_ptr = OV2722MIPI_GROUP_TOTAL_NUMS;
}

inline static void OV2722MIPI_get_sensor_group_info(MSDK_SENSOR_GROUP_INFO_STRUCT *para)
{

   SENSORDB("OV2722MIPI_get_sensor_group_info\n");

  switch (para->GroupIdx)
  {
  case OV2722MIPI_PRE_GAIN:
    sprintf(para->GroupNamePtr, "CCT");
    para->ItemCount = 5;
    break;
  case OV2722MIPI_CMMCLK_CURRENT:
    sprintf(para->GroupNamePtr, "CMMCLK Current");
    para->ItemCount = 1;
    break;
  case OV2722MIPI_FRAME_RATE_LIMITATION:
    sprintf(para->GroupNamePtr, "Frame Rate Limitation");
    para->ItemCount = 2;
    break;
  case OV2722MIPI_REGISTER_EDITOR:
    sprintf(para->GroupNamePtr, "Register Editor");
    para->ItemCount = 2;
    break;
  default:
    ASSERT(0);
  }
}

inline static void OV2722MIPI_get_sensor_item_info(MSDK_SENSOR_ITEM_INFO_STRUCT *para)
{

  const static kal_char *cct_item_name[] = {"SENSOR_BASEGAIN", "Pregain-R", "Pregain-Gr", "Pregain-Gb", "Pregain-B"};
  const static kal_char *editer_item_name[] = {"REG addr", "REG value"};
  

	 SENSORDB("OV2722MIPI_get_sensor_item_info\n");

  switch (para->GroupIdx)
  {
  case OV2722MIPI_PRE_GAIN:
    switch (para->ItemIdx)
    {
    case OV2722MIPI_SENSOR_BASEGAIN:
    case OV2722MIPI_PRE_GAIN_R_INDEX:
    case OV2722MIPI_PRE_GAIN_Gr_INDEX:
    case OV2722MIPI_PRE_GAIN_Gb_INDEX:
    case OV2722MIPI_PRE_GAIN_B_INDEX:
      break;
    default:
      ASSERT(0);
    }
    sprintf(para->ItemNamePtr, cct_item_name[para->ItemIdx - OV2722MIPI_SENSOR_BASEGAIN]);
    para->ItemValue = OV2722MIPI_sensor.eng.cct[para->ItemIdx].Para * 1000 / BASEGAIN;
    para->IsTrueFalse = para->IsReadOnly = para->IsNeedRestart = KAL_FALSE;
    para->Min = OV2722MIPI_MIN_ANALOG_GAIN * 1000;
    para->Max = OV2722MIPI_MAX_ANALOG_GAIN * 1000;
    break;
  case OV2722MIPI_CMMCLK_CURRENT:
    switch (para->ItemIdx)
    {
    case 0:
      sprintf(para->ItemNamePtr, "Drv Cur[2,4,6,8]mA");
      switch (OV2722MIPI_sensor.eng.reg[OV2722MIPI_CMMCLK_CURRENT_INDEX].Para)
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
  case OV2722MIPI_FRAME_RATE_LIMITATION:
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
  case OV2722MIPI_REGISTER_EDITOR:
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

inline static kal_bool OV2722MIPI_set_sensor_item_info(MSDK_SENSOR_ITEM_INFO_STRUCT *para)
{
  kal_uint16 temp_para;

   SENSORDB("OV2722MIPI_set_sensor_item_info\n");

  switch (para->GroupIdx)
  {
  case OV2722MIPI_PRE_GAIN:
    switch (para->ItemIdx)
    {
    case OV2722MIPI_SENSOR_BASEGAIN:
    case OV2722MIPI_PRE_GAIN_R_INDEX:
    case OV2722MIPI_PRE_GAIN_Gr_INDEX:
    case OV2722MIPI_PRE_GAIN_Gb_INDEX:
    case OV2722MIPI_PRE_GAIN_B_INDEX:
      spin_lock(&ov2722mipiraw_drv_lock);
      OV2722MIPI_sensor.eng.cct[para->ItemIdx].Para = para->ItemValue * BASEGAIN / 1000;
	  spin_unlock(&ov2722mipiraw_drv_lock);
      OV2722MIPI_SetGain(OV2722MIPI_sensor.gain); /* update gain */
      break;
    default:
      ASSERT(0);
    }
    break;
  case OV2722MIPI_CMMCLK_CURRENT:
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
      //OV2722MIPI_set_isp_driving_current(temp_para);
	  spin_lock(&ov2722mipiraw_drv_lock);
      OV2722MIPI_sensor.eng.reg[OV2722MIPI_CMMCLK_CURRENT_INDEX].Para = temp_para;
	  spin_unlock(&ov2722mipiraw_drv_lock);
      break;
    default:
      ASSERT(0);
    }
    break;
  case OV2722MIPI_FRAME_RATE_LIMITATION:
    ASSERT(0);
    break;
  case OV2722MIPI_REGISTER_EDITOR:
    switch (para->ItemIdx)
    {
      static kal_uint32 fac_sensor_reg;
    case 0:
      if (para->ItemValue < 0 || para->ItemValue > 0xFFFF) return KAL_FALSE;
      fac_sensor_reg = para->ItemValue;
      break;
    case 1:
      if (para->ItemValue < 0 || para->ItemValue > 0xFF) return KAL_FALSE;
      OV2722MIPI_write_cmos_sensor(fac_sensor_reg, para->ItemValue);
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
void OV2722MIPI_init()
{
	SENSORDB("OV2722MIPI init\n");
	OV2722MIPI_write_cmos_sensor(0x0103,0x01); // software reset
	mdelay(200);//delay 200ms
	OV2722MIPI_write_cmos_sensor(0x3718,0x10);
	OV2722MIPI_write_cmos_sensor(0x3702,0x24);
	OV2722MIPI_write_cmos_sensor(0x373a,0x60);
	OV2722MIPI_write_cmos_sensor(0x3715,0x01);
	OV2722MIPI_write_cmos_sensor(0x3703,0x2e);
	OV2722MIPI_write_cmos_sensor(0x3705,0x2b);// ;v05
	OV2722MIPI_write_cmos_sensor(0x3730,0x30);
	OV2722MIPI_write_cmos_sensor(0x3704,0x62);
	OV2722MIPI_write_cmos_sensor(0x3f06,0x3a);
	OV2722MIPI_write_cmos_sensor(0x371c,0x00);
	OV2722MIPI_write_cmos_sensor(0x371d,0xc4);
	OV2722MIPI_write_cmos_sensor(0x371e,0x01);
	OV2722MIPI_write_cmos_sensor(0x371f,0x28);
	OV2722MIPI_write_cmos_sensor(0x3708,0x61);
	OV2722MIPI_write_cmos_sensor(0x3709,0x12);
	OV2722MIPI_write_cmos_sensor(0x3800,0x00);
	OV2722MIPI_write_cmos_sensor(0x3801,0x06);
	OV2722MIPI_write_cmos_sensor(0x3802,0x00); 
	OV2722MIPI_write_cmos_sensor(0x3803,0x00);
	OV2722MIPI_write_cmos_sensor(0x3804,0x07);  
	OV2722MIPI_write_cmos_sensor(0x3805,0x9d);
	OV2722MIPI_write_cmos_sensor(0x3806,0x04);  
	OV2722MIPI_write_cmos_sensor(0x3807,0x47);
	OV2722MIPI_write_cmos_sensor(0x3808,0x07);  
	OV2722MIPI_write_cmos_sensor(0x3809,0x84);
	OV2722MIPI_write_cmos_sensor(0x380a,0x04);  
	OV2722MIPI_write_cmos_sensor(0x380b,0x3C);
	OV2722MIPI_write_cmos_sensor(0x380c,0x08);  
	OV2722MIPI_write_cmos_sensor(0x380d,0x5c); //0x85c
	OV2722MIPI_write_cmos_sensor(0x380e,0x04);  
	OV2722MIPI_write_cmos_sensor(0x380f,0x60); //0x460
	OV2722MIPI_write_cmos_sensor(0x3810,0x00);  
	OV2722MIPI_write_cmos_sensor(0x3811,0x09);
	OV2722MIPI_write_cmos_sensor(0x3812,0x00);    
	OV2722MIPI_write_cmos_sensor(0x3813,0x06);
	
	OV2722MIPI_write_cmos_sensor(0x3820, (0x80|(0x06))); //add by chenqiang
	OV2722MIPI_write_cmos_sensor(0x3821, (0x06&(0xF9)));
	//OV2722MIPI_write_cmos_sensor(0x3820, (0x80); 
	//OV2722MIPI_write_cmos_sensor(0x3821, (0x06);
	OV2722MIPI_write_cmos_sensor(0x3814,0x11);
	OV2722MIPI_write_cmos_sensor(0x3815,0x11);
	OV2722MIPI_write_cmos_sensor(0x3612,0x4b);// ;BA_V03
	OV2722MIPI_write_cmos_sensor(0x3618,0x04);
	
	OV2722MIPI_write_cmos_sensor(0x3a08,0x01);
	OV2722MIPI_write_cmos_sensor(0x3a09,0x50);
	OV2722MIPI_write_cmos_sensor(0x3a0a,0x01);
	OV2722MIPI_write_cmos_sensor(0x3a0b,0x18);
	OV2722MIPI_write_cmos_sensor(0x3a0d,0x03);
	OV2722MIPI_write_cmos_sensor(0x3a0e,0x03);
	OV2722MIPI_write_cmos_sensor(0x4520,0x00);
	OV2722MIPI_write_cmos_sensor(0x4837,0x1b);
	OV2722MIPI_write_cmos_sensor(0x3000,0xff);
	OV2722MIPI_write_cmos_sensor(0x3001,0xff);
	OV2722MIPI_write_cmos_sensor(0x3002,0xf0);
	OV2722MIPI_write_cmos_sensor(0x3600,0x08);
	OV2722MIPI_write_cmos_sensor(0x3621,0xc0);
	OV2722MIPI_write_cmos_sensor(0x3632,0x53);// ;v06
	OV2722MIPI_write_cmos_sensor(0x3633,0x63);// ;v06
	OV2722MIPI_write_cmos_sensor(0x3634,0x24);// ;R1A_AM02
	OV2722MIPI_write_cmos_sensor(0x3f01,0x0c);
	OV2722MIPI_write_cmos_sensor(0x5001,0xc1);
	OV2722MIPI_write_cmos_sensor(0x3614,0xf0);
	OV2722MIPI_write_cmos_sensor(0x3630,0x2d);
	OV2722MIPI_write_cmos_sensor(0x370b,0x62);
	OV2722MIPI_write_cmos_sensor(0x3706,0x61);
	OV2722MIPI_write_cmos_sensor(0x4000,0x02);
	OV2722MIPI_write_cmos_sensor(0x4002,0xc5);
	OV2722MIPI_write_cmos_sensor(0x4005,0x08);
	OV2722MIPI_write_cmos_sensor(0x404f,0x84);
	OV2722MIPI_write_cmos_sensor(0x4051,0x00);
	OV2722MIPI_write_cmos_sensor(0x5000,0xff);
	OV2722MIPI_write_cmos_sensor(0x3a18,0x00);
	OV2722MIPI_write_cmos_sensor(0x3a19,0x80);
	OV2722MIPI_write_cmos_sensor(0x3503,0x07);
	OV2722MIPI_write_cmos_sensor(0x4521,0x00);
	OV2722MIPI_write_cmos_sensor(0x5183,0xb0);
	OV2722MIPI_write_cmos_sensor(0x5184,0xb0);
	OV2722MIPI_write_cmos_sensor(0x5185,0xb0);
	OV2722MIPI_write_cmos_sensor(0x5180,0x03);
	OV2722MIPI_write_cmos_sensor(0x370c,0x0c);
	
	OV2722MIPI_write_cmos_sensor(0x3035,0x10);
	OV2722MIPI_write_cmos_sensor(0x3036,0x1e);
	OV2722MIPI_write_cmos_sensor(0x3037,0x21);
	OV2722MIPI_write_cmos_sensor(0x303e,0x19);
	OV2722MIPI_write_cmos_sensor(0x3038,0x06);
	OV2722MIPI_write_cmos_sensor(0x3018,0x04);
	OV2722MIPI_write_cmos_sensor(0x3000,0x00);
	OV2722MIPI_write_cmos_sensor(0x3001,0x00);
	OV2722MIPI_write_cmos_sensor(0x3002,0x00);
	OV2722MIPI_write_cmos_sensor(0x3a0f,0x40);
	OV2722MIPI_write_cmos_sensor(0x3a10,0x38);
	OV2722MIPI_write_cmos_sensor(0x3a1b,0x48);
	OV2722MIPI_write_cmos_sensor(0x3a1e,0x30);
	OV2722MIPI_write_cmos_sensor(0x3a11,0x90);
	OV2722MIPI_write_cmos_sensor(0x3a1f,0x10);

	//OV2722MIPI_write_cmos_sensor(0x4800,0x04);  //add by chenqiang for r4800 bit[3]  Line sync enable.
	OV2722MIPI_write_cmos_sensor(0x4800,0x14);//len start/end
	OV2722MIPI_write_cmos_sensor(0x3011,0x22);
	OV2722MIPI_write_cmos_sensor(0x0100,0x01);
	OV2722MIPI_AutoFlicker_Mode=KAL_FALSE;	
}
static void OV2722MIPI_HVMirror(kal_uint8 image_mirror)
{
#if 0
	kal_int16 mirror=0,flip=0;
	mirror= OV2722MIPI_read_cmos_sensor(0x3820);
	flip  = OV2722MIPI_read_cmos_sensor(0x3821);
    switch (imgMirror)
    {
        case IMAGE_H_MIRROR://IMAGE_NORMAL:
            OV2722MIPI_write_cmos_sensor(0x3820, (mirror & (0xF9)));//Set normal
            OV2722MIPI_write_cmos_sensor(0x3821, (flip & (0xF9)));	//Set normal
            break;
        case IMAGE_NORMAL://IMAGE_V_MIRROR:
            OV2722MIPI_write_cmos_sensor(0x3820, (mirror & (0xF9)));//Set flip
            OV2722MIPI_write_cmos_sensorr(0x3821, (flip | (0x06)));	//Set flip
            break;
        case IMAGE_HV_MIRROR://IMAGE_H_MIRROR:
            OV2722MIPI_write_cmos_sensor(0x3820, (mirror |(0x06)));	//Set mirror
            OV2722MIPI_write_cmos_sensor(0x3821, (flip & (0xF9)));	//Set mirror
            break;
        case IMAGE_V_MIRROR://IMAGE_HV_MIRROR:
            OV2722MIPI_write_cmos_sensor(0x3820, (mirror |(0x06)));	//Set mirror & flip
            OV2722MIPI_write_cmos_sensor(0x3821, (flip |(0x06)));	//Set mirror & flip
            break;
    }

#endif
}



/*****************************************************************************/
/* Windows Mobile Sensor Interface */
/*****************************************************************************/
/*************************************************************************
* FUNCTION
*	OV2722MIPIOpen
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

UINT32 OV2722MIPIOpen(void)
{
 kal_uint32 id=0;
 if(OV2722MIPI_SENSOR_ID!=OV2722MIPI_sensor_id)
 	{    	 
		id= ((OV2722MIPI_read_cmos_sensor(0x300A) << 8) | OV2722MIPI_read_cmos_sensor(0x300B));
		spin_lock(&ov2722mipiraw_drv_lock);
        OV2722MIPI_sensor_id=id;
		spin_unlock(&ov2722mipiraw_drv_lock);
    	if (OV2722MIPI_sensor_id != OV2722MIPI_SENSOR_ID)
		{
    		return ERROR_SENSOR_CONNECT_FAIL;
        }
 	}
    SENSORDB("OV2722MIPI Sensor Read ID:0x%04x OK\n",OV2722MIPI_sensor_id);
    OV2722MIPI_init();
    spin_lock(&ov2722mipiraw_drv_lock);
	OV2722MIPI_state=1;
	OV2722MIPI_sensor.pv_mode=KAL_TRUE;
	OV2722MIPI_sensor.shutter=0x100;
	spin_unlock(&ov2722mipiraw_drv_lock);
    return ERROR_NONE;
}


UINT32 OV2722MIPIGetSensorID(UINT32 *sensorID)
{
	int  retry = 3; 
	SENSORDB("OV2722 Get sensor id start!\n");

    // check if sensor ID correct
    do {
        *sensorID = ((OV2722MIPI_read_cmos_sensor(0x300A) << 8) | OV2722MIPI_read_cmos_sensor(0x300B));
		spin_lock(&ov2722mipiraw_drv_lock);
		OV2722MIPI_sensor_id=*sensorID;
		spin_unlock(&ov2722mipiraw_drv_lock);
        if (*sensorID == OV2722MIPI_SENSOR_ID)
            break; 
        SENSORDB(" 0v2722 Read Sensor ID Fail = 0x%04x\n", *sensorID); 
        retry--; 
    } while (retry > 0);

    if (*sensorID != OV2722MIPI_SENSOR_ID) {
        *sensorID = 0xFFFFFFFF; 
        return ERROR_SENSOR_CONNECT_FAIL;
    }
	SENSORDB("Get sensor id sccuss!\n");
    return ERROR_NONE;
}


/*************************************************************************
* FUNCTION
*	OV2722MIPIClose
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
UINT32 OV2722MIPIClose(void)
{

   SENSORDB("OV2722MIPIClose\n");

  //CISModulePowerOn(FALSE);
//	DRV_I2CClose(OV2722MIPIhDrvI2C);
	return ERROR_NONE;
}   /* OV2722MIPIClose */

/*************************************************************************
* FUNCTION
* OV2722MIPIPreview
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
UINT32 OV2722MIPIPreview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint16 startx=0,starty=0;
	kal_uint16 dummy_line;

	SENSORDB("OV2722MIPIPreview \n");
	if(OV2722MIPI_state==1)
		{
		  spin_lock(&ov2722mipiraw_drv_lock);
		   OV2722MIPI_state=0;
		   spin_unlock(&ov2722mipiraw_drv_lock);
		}
	spin_lock(&ov2722mipiraw_drv_lock);
	OV2722MIPI_sensor.pv_mode = KAL_TRUE;
	spin_unlock(&ov2722mipiraw_drv_lock);
	switch (sensor_config_data->SensorOperationMode)
	{
	  case MSDK_SENSOR_OPERATION_MODE_VIDEO: 
	    spin_lock(&ov2722mipiraw_drv_lock);
		OV2722MIPI_sensor.video_mode = KAL_TRUE;
		spin_unlock(&ov2722mipiraw_drv_lock);		

		SENSORDB("Video mode \n");
		break;

	  default: /* ISP_PREVIEW_MODE */
		  spin_lock(&ov2722mipiraw_drv_lock);
		OV2722MIPI_sensor.video_mode = KAL_FALSE;
		spin_unlock(&ov2722mipiraw_drv_lock);

		SENSORDB("Camera preview mode \n");
		break;

	}
	OV2722MIPI_HVMirror(sensor_config_data->SensorImageMirror);
	dummy_line = 0;
	OV2722MIPI_Set_Dummy(0, dummy_line); /* modify dummy_pixel must gen AE table again */
//	OV2722MIPI_Write_Shutter(OV2722MIPI_sensor.shutter);	 
/*
	startx+=OV2722MIPI_PV_X_START;
	starty+=OV2722MIPI_PV_Y_START;
	image_window->GrabStartX= startx;
    image_window->GrabStartY= starty;
    image_window->ExposureWindowWidth= OV2722MIPI_IMAGE_SENSOR_PV_WIDTH - 2*startx;
    image_window->ExposureWindowHeight= OV2722MIPI_IMAGE_SENSOR_PV_HEIGHT - 2*starty;
   */
	return ERROR_NONE;
	
}   /*  OV2722MIPIPreview   */

/*************************************************************************
* FUNCTION
*	OV2722MIPICapture
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
UINT32 OV2722MIPICapture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
						  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	const kal_uint16 pv_line_length = OV2722MIPI_sensor.line_length;
	//const kal_uint32 pv_pclk = OV2722MIPI_sensor.pclk;
	kal_uint16 shutter = OV2722MIPI_sensor.shutter;
	kal_uint16 dummy_pixel=0,dummy_line=0, cap_fps;
	kal_uint16 startx=0,starty=0;

	SENSORDB("OV2722MIPICapture \n");
	spin_lock(&ov2722mipiraw_drv_lock);
	OV2722MIPI_state=1;
	spin_unlock(&ov2722mipiraw_drv_lock);

	spin_lock(&ov2722mipiraw_drv_lock);
    OV2722MIPI_sensor.video_mode=KAL_FALSE;
	OV2722MIPI_AutoFlicker_Mode=KAL_FALSE;
	spin_unlock(&ov2722mipiraw_drv_lock);

	
	OV2722MIPI_HVMirror(sensor_config_data->SensorImageMirror);

	
		spin_lock(&ov2722mipiraw_drv_lock);
		OV2722MIPI_sensor.pv_mode = KAL_FALSE;
		spin_unlock(&ov2722mipiraw_drv_lock);
		#if 0
		cap_fps = OV2722MIPI_FPS(13);
		
		dummy_pixel= OV2722MIPI_sensor.pclk * OV2722MIPI_FPS(1) / ((OV2722MIPI_FULL_PERIOD_LINE_NUMS+OV2722MIPI_DEFAULT_DUMMY_LINES) * cap_fps);
		dummy_pixel = dummy_pixel< (OV2722MIPI_FULL_PERIOD_PIXEL_NUMS+OV2722MIPI_DEFAULT_DUMMY_PIXELS) ? 0 : dummy_pixel- (OV2722MIPI_FULL_PERIOD_PIXEL_NUMS+OV2722MIPI_DEFAULT_DUMMY_PIXELS);
		#endif
	startx+=OV2722MIPI_FULL_X_START;
	starty+=OV2722MIPI_FULL_Y_START;
	image_window->GrabStartX= startx;
    image_window->GrabStartY= starty;
    image_window->ExposureWindowWidth= OV2722MIPI_IMAGE_SENSOR_PV_WIDTH - 2*startx;
    image_window->ExposureWindowHeight= OV2722MIPI_IMAGE_SENSOR_PV_HEIGHT - 2*starty;
	

	return ERROR_NONE;
}   /* OV2722MIPI_Capture() */

UINT32 OV2722MIPIGetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution)
{

		
	    pSensorResolution->SensorPreviewWidth=OV2722MIPI_IMAGE_SENSOR_PV_WIDTH;
	    pSensorResolution->SensorPreviewHeight=OV2722MIPI_IMAGE_SENSOR_PV_HEIGHT;
		pSensorResolution->SensorFullWidth=OV2722MIPI_IMAGE_SENSOR_FULL_WIDTH;
		pSensorResolution->SensorFullHeight=OV2722MIPI_IMAGE_SENSOR_FULL_HEIGHT;
//Gionee yanggy 2012-12-10 add for rear camera begin
        pSensorResolution->SensorVideoWidth= OV2722MIPI_IMAGE_SENSOR_PV_WIDTH;
        pSensorResolution->SensorVideoHeight=OV2722MIPI_IMAGE_SENSOR_PV_HEIGHT;
        pSensorResolution->Sensor3DPreviewWidth=OV2722MIPI_IMAGE_SENSOR_PV_WIDTH;
        pSensorResolution->Sensor3DPreviewHeight=OV2722MIPI_IMAGE_SENSOR_PV_HEIGHT;
        pSensorResolution->Sensor3DFullWidth=OV2722MIPI_IMAGE_SENSOR_PV_WIDTH;
        pSensorResolution->Sensor3DFullHeight=OV2722MIPI_IMAGE_SENSOR_PV_HEIGHT;
        pSensorResolution->Sensor3DVideoWidth=OV2722MIPI_IMAGE_SENSOR_PV_WIDTH;
        pSensorResolution->Sensor3DVideoHeight=OV2722MIPI_IMAGE_SENSOR_PV_HEIGHT;
//Gionee yanggy 2012-12-10 add for rear camera end
		SENSORDB("OV2722MIPIGetResolution:%d,%d \n",pSensorResolution->SensorPreviewWidth,pSensorResolution->SensorFullWidth);
	return ERROR_NONE;
}	/* OV2722MIPIGetResolution() */

UINT32 OV2722MIPIGetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId,
					  MSDK_SENSOR_INFO_STRUCT *pSensorInfo,
					  MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{

	SENSORDB("OV2722MIPIGetInfo£¬FeatureId:%d\n",ScenarioId);


    switch(ScenarioId)
    {
    	case MSDK_SCENARIO_ID_CAMERA_ZSD:
			pSensorInfo->SensorPreviewResolutionX=OV2722MIPI_IMAGE_SENSOR_PV_WIDTH;
			pSensorInfo->SensorPreviewResolutionY=OV2722MIPI_IMAGE_SENSOR_PV_HEIGHT;
			pSensorInfo->SensorCameraPreviewFrameRate=15;
			break;
		default:			
			pSensorInfo->SensorPreviewResolutionX=OV2722MIPI_IMAGE_SENSOR_PV_WIDTH;
			pSensorInfo->SensorPreviewResolutionY=OV2722MIPI_IMAGE_SENSOR_PV_HEIGHT;
			pSensorInfo->SensorCameraPreviewFrameRate=30;
		    break;
			
    }

	pSensorInfo->SensorFullResolutionX=OV2722MIPI_IMAGE_SENSOR_FULL_WIDTH;
	pSensorInfo->SensorFullResolutionY=OV2722MIPI_IMAGE_SENSOR_FULL_HEIGHT;


	pSensorInfo->SensorVideoFrameRate=30;
	pSensorInfo->SensorStillCaptureFrameRate=10;
	pSensorInfo->SensorWebCamCaptureFrameRate=15;
	pSensorInfo->SensorResetActiveHigh=TRUE; //low active
	pSensorInfo->SensorResetDelayCount=5; 

	pSensorInfo->SensorOutputDataFormat=OV2722MIPI_COLOR_FORMAT;
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
    pSensorInfo->SensorDrivingCurrent = ISP_DRIVING_8MA;
    pSensorInfo->AEShutDelayFrame = 0;		   /* The frame of setting shutter default 0 for TG int */
	pSensorInfo->AESensorGainDelayFrame = 0;	   /* The frame of setting sensor gain */
	pSensorInfo->AEISPGainDelayFrame = 2; 
	//pSensorInfo->AEISPGainDelayFrame = 1;    
	switch (ScenarioId)
	{
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			pSensorInfo->SensorClockFreq=24;//26  hehe@0327 for flicker
			pSensorInfo->SensorClockDividCount= 3;
			pSensorInfo->SensorClockRisingCount= 0;
			pSensorInfo->SensorClockFallingCount= 2;
			pSensorInfo->SensorPixelClockCount= 3;
			pSensorInfo->SensorDataLatchCount= 2;
			pSensorInfo->SensorGrabStartX = OV2722MIPI_FULL_X_START; 
			pSensorInfo->SensorGrabStartY = OV2722MIPI_FULL_Y_START; 
			pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_2_LANE;			
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0; 
	        pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 0; 
	        pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorWidthSampling = 0;  // 0 is default 1x
            pSensorInfo->SensorHightSampling = 0;   // 0 is default 1x 
            pSensorInfo->SensorPacketECCOrder = 1;

		break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		 case MSDK_SCENARIO_ID_CAMERA_ZSD:

			pSensorInfo->SensorClockFreq=24;//26  hehe@0327 for flicker
			pSensorInfo->SensorClockDividCount= 3;
			pSensorInfo->SensorClockRisingCount=0;
			pSensorInfo->SensorClockFallingCount=2;
			pSensorInfo->SensorPixelClockCount=3;
			pSensorInfo->SensorDataLatchCount=2;
			pSensorInfo->SensorGrabStartX = OV2722MIPI_FULL_X_START; 
			pSensorInfo->SensorGrabStartY = OV2722MIPI_FULL_Y_START; 
			pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_2_LANE;			
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0; 
	        pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 0; 
	        pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorWidthSampling = 0;  // 0 is default 1x
            pSensorInfo->SensorHightSampling = 0;   // 0 is default 1x 
            pSensorInfo->SensorPacketECCOrder = 1;
		break;
		default:
			pSensorInfo->SensorClockFreq=24;//26 hehe@0327 for flicker
			pSensorInfo->SensorClockDividCount=3;
			pSensorInfo->SensorClockRisingCount=0;
			pSensorInfo->SensorClockFallingCount=2;		
			pSensorInfo->SensorPixelClockCount=3;
			pSensorInfo->SensorDataLatchCount=2;
			pSensorInfo->SensorGrabStartX = OV2722MIPI_FULL_Y_START; 
			pSensorInfo->SensorGrabStartY = OV2722MIPI_FULL_Y_START; 
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
	OV2722MIPIPixelClockDivider=pSensorInfo->SensorPixelClockCount;
	memcpy(pSensorConfigData, &OV2722MIPISensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));
#endif		
  return ERROR_NONE;
}	/* OV2722MIPIGetInfo() */


UINT32 OV2722MIPIControl(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow,
					  MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{

	SENSORDB("OV2722MIPIControl£¬FeatureId:%d\n",ScenarioId);
	CurrentScenarioId=ScenarioId;
		
	switch (ScenarioId)
	{
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			OV2722MIPIPreview(pImageWindow, pSensorConfigData);
		break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		 case MSDK_SCENARIO_ID_CAMERA_ZSD:
		 	SENSORDB("MSDK_SCENARIO_ID_CAMERA_ZSD\n");


			OV2722MIPICapture(pImageWindow, pSensorConfigData);

		break;		
        default:
            return ERROR_INVALID_SCENARIO_ID;
	}
	return TRUE;
}	/* OV2722MIPIControl() */

UINT32 OV2722MIPISetVideoMode(UINT16 u2FrameRate)
{
	kal_int16 dummy_line;
    /* to fix VSYNC, to fix frame rate */

	SENSORDB("OV2722MIPISetVideoMode£¬u2FrameRate:%d\n",u2FrameRate);
	if(u2FrameRate==0)
		return TRUE;
	dummy_line = OV2722MIPI_sensor.pclk / u2FrameRate / (OV2722MIPI_PV_PERIOD_PIXEL_NUMS+OV2722MIPI_DEFAULT_DUMMY_PIXELS) - (OV2722MIPI_PV_PERIOD_LINE_NUMS+OV2722MIPI_DEFAULT_DUMMY_LINES);
	if (dummy_line < 0) dummy_line = 0;

	SENSORDB("dummy_line %d\n", dummy_line);

	OV2722MIPI_Set_Dummy(0, dummy_line); /* modify dummy_pixel must gen AE table again */
	spin_lock(&ov2722mipiraw_drv_lock);
	OV2722MIPI_sensor.video_mode = KAL_TRUE;
	spin_unlock(&ov2722mipiraw_drv_lock);
    return TRUE;
}

UINT32 OV2722MIPISetAutoFlickerMode(kal_bool bEnable,UINT16 u2FrameRate)
{
	SENSORDB("OV2722MIPISetAutoFlickerMode bEnable=%d,u2FrameRate=%d\n",bEnable,u2FrameRate);
	if(bEnable)
		{
			spin_lock(&ov2722mipiraw_drv_lock);
		   OV2722MIPI_AutoFlicker_Mode=KAL_TRUE;
		   spin_unlock(&ov2722mipiraw_drv_lock);

		}
	else
		{
			spin_lock(&ov2722mipiraw_drv_lock);
		   OV2722MIPI_AutoFlicker_Mode=KAL_FALSE;
		   spin_unlock(&ov2722mipiraw_drv_lock);

		}

}



UINT32 OV2722MIPIFeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId,
							 UINT8 *pFeaturePara,UINT32 *pFeatureParaLen)
{
	UINT16 *pFeatureReturnPara16=(UINT16 *) pFeaturePara;
	UINT16 *pFeatureData16=(UINT16 *) pFeaturePara;
	UINT32 *pFeatureReturnPara32=(UINT32 *) pFeaturePara;
	UINT32 *pFeatureData32=(UINT32 *) pFeaturePara;
	UINT32 OV2722MIPISensorRegNumber;
	UINT32 i;
	PNVRAM_SENSOR_DATA_STRUCT pSensorDefaultData=(PNVRAM_SENSOR_DATA_STRUCT) pFeaturePara;
	MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData=(MSDK_SENSOR_CONFIG_STRUCT *) pFeaturePara;
	MSDK_SENSOR_REG_INFO_STRUCT *pSensorRegData=(MSDK_SENSOR_REG_INFO_STRUCT *) pFeaturePara;
	MSDK_SENSOR_GROUP_INFO_STRUCT *pSensorGroupInfo=(MSDK_SENSOR_GROUP_INFO_STRUCT *) pFeaturePara;
	MSDK_SENSOR_ITEM_INFO_STRUCT *pSensorItemInfo=(MSDK_SENSOR_ITEM_INFO_STRUCT *) pFeaturePara;
	MSDK_SENSOR_ENG_INFO_STRUCT	*pSensorEngInfo=(MSDK_SENSOR_ENG_INFO_STRUCT *) pFeaturePara;


	SENSORDB("OV2722MIPIFeatureControl£¬FeatureId:%d\n",FeatureId);
		
	switch (FeatureId)
	{
		case SENSOR_FEATURE_GET_RESOLUTION:
			*pFeatureReturnPara16++=OV2722MIPI_IMAGE_SENSOR_FULL_WIDTH;
			*pFeatureReturnPara16=OV2722MIPI_IMAGE_SENSOR_FULL_HEIGHT;
			*pFeatureParaLen=4;
		break;
		case SENSOR_FEATURE_GET_PERIOD:	/* 3 */

			 switch(CurrentScenarioId)
			 {
			 	case MSDK_SCENARIO_ID_CAMERA_ZSD:
					*pFeatureReturnPara16++=OV2722MIPI_sensor.line_length;
					*pFeatureReturnPara16=OV2722MIPI_sensor.frame_height;
					*pFeatureParaLen=4;
					break;
				default:
					*pFeatureReturnPara16++=OV2722MIPI_sensor.line_length;
					*pFeatureReturnPara16=OV2722MIPI_sensor.frame_height;
					*pFeatureParaLen=4;
					break;
					
			 }

		break;
		case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:  /* 3 */

			switch(CurrentScenarioId)
			 {
			 	case MSDK_SCENARIO_ID_CAMERA_ZSD:
					*pFeatureReturnPara32 = OV2722MIPI_sensor.pclk;
					*pFeatureParaLen=4;
					break;
				default:
					*pFeatureReturnPara32 = OV2722MIPI_sensor.pclk;
					*pFeatureParaLen=4;
					break;					
			 }

			break;
		case SENSOR_FEATURE_SET_ESHUTTER:	/* 4 */
			set_OV2722MIPI_shutter(*pFeatureData16);
		break;
		case SENSOR_FEATURE_SET_NIGHTMODE:
			OV2722MIPI_night_mode((BOOL) *pFeatureData16);
		break;
		case SENSOR_FEATURE_SET_GAIN:	/* 6 */
			OV2722MIPI_SetGain((UINT16) *pFeatureData16);
		break;
		case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
		case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
#if 1
		case SENSOR_FEATURE_SET_REGISTER:
		OV2722MIPI_write_cmos_sensor(pSensorRegData->RegAddr, pSensorRegData->RegData);
		break;
		case SENSOR_FEATURE_GET_REGISTER:
			pSensorRegData->RegData = OV2722MIPI_read_cmos_sensor(pSensorRegData->RegAddr);
		break;
		case SENSOR_FEATURE_SET_CCT_REGISTER:
			memcpy(&OV2722MIPI_sensor.eng.cct, pFeaturePara, sizeof(OV2722MIPI_sensor.eng.cct));
			break;
		break;
		case SENSOR_FEATURE_GET_CCT_REGISTER:	/* 12 */
			if (*pFeatureParaLen >= sizeof(OV2722MIPI_sensor.eng.cct) + sizeof(kal_uint32))
			{
			  *((kal_uint32 *)pFeaturePara++) = sizeof(OV2722MIPI_sensor.eng.cct);
			  memcpy(pFeaturePara, &OV2722MIPI_sensor.eng.cct, sizeof(OV2722MIPI_sensor.eng.cct));
			}
			break;
		case SENSOR_FEATURE_SET_ENG_REGISTER:
			memcpy(&OV2722MIPI_sensor.eng.reg, pFeaturePara, sizeof(OV2722MIPI_sensor.eng.reg));
			break;
		case SENSOR_FEATURE_GET_ENG_REGISTER:	/* 14 */
			if (*pFeatureParaLen >= sizeof(OV2722MIPI_sensor.eng.reg) + sizeof(kal_uint32))
			{
			  *((kal_uint32 *)pFeaturePara++) = sizeof(OV2722MIPI_sensor.eng.reg);
			  memcpy(pFeaturePara, &OV2722MIPI_sensor.eng.reg, sizeof(OV2722MIPI_sensor.eng.reg));
			}
		case SENSOR_FEATURE_GET_REGISTER_DEFAULT:
			((PNVRAM_SENSOR_DATA_STRUCT)pFeaturePara)->Version = NVRAM_CAMERA_SENSOR_FILE_VERSION;
			((PNVRAM_SENSOR_DATA_STRUCT)pFeaturePara)->SensorId = OV2722MIPI_SENSOR_ID;
			memcpy(((PNVRAM_SENSOR_DATA_STRUCT)pFeaturePara)->SensorEngReg, &OV2722MIPI_sensor.eng.reg, sizeof(OV2722MIPI_sensor.eng.reg));
			memcpy(((PNVRAM_SENSOR_DATA_STRUCT)pFeaturePara)->SensorCCTReg, &OV2722MIPI_sensor.eng.cct, sizeof(OV2722MIPI_sensor.eng.cct));
			*pFeatureParaLen = sizeof(NVRAM_SENSOR_DATA_STRUCT);
			break;
		case SENSOR_FEATURE_GET_CONFIG_PARA:
			memcpy(pFeaturePara, &OV2722MIPI_sensor.cfg_data, sizeof(OV2722MIPI_sensor.cfg_data));
			*pFeatureParaLen = sizeof(OV2722MIPI_sensor.cfg_data);
			break;
		case SENSOR_FEATURE_CAMERA_PARA_TO_SENSOR:
		     OV2722MIPI_camera_para_to_sensor();
		break;
		case SENSOR_FEATURE_SENSOR_TO_CAMERA_PARA:
			OV2722MIPI_sensor_to_camera_para();
		break;							
		case SENSOR_FEATURE_GET_GROUP_COUNT:
			OV2722MIPI_get_sensor_group_count((kal_uint32 *)pFeaturePara);
			*pFeatureParaLen = 4;
		break;										
		  OV2722MIPI_get_sensor_group_info((MSDK_SENSOR_GROUP_INFO_STRUCT *)pFeaturePara);
		  *pFeatureParaLen = sizeof(MSDK_SENSOR_GROUP_INFO_STRUCT);
		  break;
		case SENSOR_FEATURE_GET_ITEM_INFO:
		  OV2722MIPI_get_sensor_item_info((MSDK_SENSOR_ITEM_INFO_STRUCT *)pFeaturePara);
		  *pFeatureParaLen = sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
		  break;
		case SENSOR_FEATURE_SET_ITEM_INFO:
		  OV2722MIPI_set_sensor_item_info((MSDK_SENSOR_ITEM_INFO_STRUCT *)pFeaturePara);
		  *pFeatureParaLen = sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
		  break;
		case SENSOR_FEATURE_GET_ENG_INFO:
     		memcpy(pFeaturePara, &OV2722MIPI_sensor.eng_info, sizeof(OV2722MIPI_sensor.eng_info));
     		*pFeatureParaLen = sizeof(OV2722MIPI_sensor.eng_info);
     		break;
		case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
			// get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE
			// if EEPROM does not exist in camera module.
			*pFeatureReturnPara32=LENS_DRIVER_ID_DO_NOT_CARE;
			*pFeatureParaLen=4;
		break;
#endif
		case SENSOR_FEATURE_SET_VIDEO_MODE:
		       OV2722MIPISetVideoMode(*pFeatureData16);
        break; 

		case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
			OV2722MIPISetAutoFlickerMode((BOOL)*pFeatureData16,*(pFeatureData16+1));
			break;

        case SENSOR_FEATURE_CHECK_SENSOR_ID:
            OV2722MIPIGetSensorID(pFeatureReturnPara32); 
            break; 
		default:
			break;
	}
	return ERROR_NONE;
}	/* OV2722MIPIFeatureControl() */
SENSOR_FUNCTION_STRUCT    SensorFuncOV2722MIPI=
{
  OV2722MIPIOpen,
  OV2722MIPIGetInfo,
  OV2722MIPIGetResolution,
  OV2722MIPIFeatureControl,
  OV2722MIPIControl,
  OV2722MIPIClose
};

UINT32 OV2722MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{

  /* To Do : Check Sensor status here */
  if (pfFunc!=NULL)
      *pfFunc=&SensorFuncOV2722MIPI;

  return ERROR_NONE;
} /* SensorInit() */



