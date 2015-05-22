/*****************************************************************************
 *
 * Filename:
 * ---------
 *   imx111mipiraw_sensor.c
 *
 * Project:
 * --------
 *   RAW
 *
 * Description:
 * ------------
 *   Source code of Sensor driver
 *

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

#include "imx135mipiraw_Sensor.h"
#include "imx135mipiraw_Camera_Sensor_para.h"
#include "imx135mipiraw_CameraCustomized.h"

kal_bool IMX135MIPI_MPEG4_encode_mode = KAL_FALSE;
kal_bool IMX135MIPI_Auto_Flicker_mode = KAL_FALSE;


kal_uint8 IMX135MIPI_sensor_write_I2C_address = IMX135MIPI_WRITE_ID;
kal_uint8 IMX135MIPI_sensor_read_I2C_address = IMX135MIPI_READ_ID;

#define IMX135_TEST_PATTERN_CHECKSUM (0x51905ccf)

#if 0//ndef IMX135_CAPTURE_24FPS
static struct IMX135MIPI_sensor_STRUCT IMX135MIPI_sensor={IMX135MIPI_WRITE_ID,IMX135MIPI_READ_ID,KAL_TRUE,KAL_FALSE,KAL_TRUE,KAL_FALSE,
KAL_FALSE,KAL_FALSE,KAL_FALSE,231270000,231270000,259200000,0,0,0,64,64,64,IMX135MIPI_PV_LINE_LENGTH_PIXELS,
IMX135MIPI_PV_FRAME_LENGTH_LINES,IMX135MIPI_VIDEO_LINE_LENGTH_PIXELS,IMX135MIPI_VIDEO_FRAME_LENGTH_LINES,IMX135MIPI_FULL_LINE_LENGTH_PIXELS,IMX135MIPI_FULL_FRAME_LENGTH_LINES,0,0,0,0,0,0,30};
#else
static struct IMX135MIPI_sensor_STRUCT IMX135MIPI_sensor=
{
	IMX135MIPI_WRITE_ID,											//kal_uint16 i2c_write_id;                                                 
	IMX135MIPI_READ_ID,                       //kal_uint16 i2c_read_id;                                                  
	KAL_TRUE,                                 //kal_bool first_init;                                                     
	KAL_FALSE,                                //kal_bool fix_video_fps;                                                  
	KAL_TRUE,                                 //kal_bool pv_mode;                                                        
	KAL_FALSE,                                //kal_bool video_mode; 				                                             
	KAL_FALSE,                                //kal_bool capture_mode; 				//True: Preview Mode; False: Capture Mode  
	KAL_FALSE,                                //kal_bool night_mode;				//True: Night Mode; False: Auto Mode         
	KAL_FALSE,                                //kal_uint8 mirror_flip;                                                   
	231270000,								  //kal_uint32 pv_pclk; 			//Preview Pclk								   
	231270000,								  //kal_uint32 video_pclk;				//video Pclk							   
#if defined(IMX135_CAPTURE_24FPS)
	348000000,								  //kal_uint32 cp_pclk; 			//Capture Pclk								   
#else
	259200000,                                //kal_uint32 cp_pclk;				//Capture Pclk                                 
#endif
	0,                                        //kal_uint32 pv_shutter;		                                               
	0,                                        //kal_uint32 video_shutter;		                                             
	0,                                        //kal_uint32 cp_shutter;                                                   
	64,                                       //kal_uint32 pv_gain;                                                      
	64,                                       //kal_uint32 video_gain;                                                   
	64,                                       //kal_uint32 cp_gain;                                                      
	IMX135MIPI_PV_LINE_LENGTH_PIXELS,         //kal_uint32 pv_line_length;                                               
	IMX135MIPI_PV_FRAME_LENGTH_LINES,         //kal_uint32 pv_frame_length;                                              
	IMX135MIPI_VIDEO_LINE_LENGTH_PIXELS,      //kal_uint32 video_line_length;                                            
	IMX135MIPI_VIDEO_FRAME_LENGTH_LINES,      //kal_uint32 video_frame_length;                                           
	IMX135MIPI_FULL_LINE_LENGTH_PIXELS,       //kal_uint32 cp_line_length;                                               
	IMX135MIPI_FULL_FRAME_LENGTH_LINES,       //kal_uint32 cp_frame_length;                                              
	0,                                        //kal_uint16 pv_dummy_pixels;		   //Dummy Pixels:must be 12s              
	0,                                        //kal_uint16 pv_dummy_lines;		   //Dummy Lines                           
	0,                                        //kal_uint16 video_dummy_pixels;		   //Dummy Pixels:must be 12s          
	0,                                        //kal_uint16 video_dummy_lines;		   //Dummy Lines                         
	0,                                        //kal_uint16 cp_dummy_pixels;		   //Dummy Pixels:must be 12s              
	0,                                        //kal_uint16 cp_dummy_lines;		   //Dummy Lines			                     
	30                                        //kal_uint16 video_current_frame_rate;                                     
};

#endif
MSDK_SCENARIO_ID_ENUM CurrentScenarioId = MSDK_SCENARIO_ID_CAMERA_PREVIEW;


/* MAX/MIN Explosure Lines Used By AE Algorithm */
kal_uint16 IMX135MIPI_MAX_EXPOSURE_LINES = IMX135MIPI_PV_FRAME_LENGTH_LINES-5;
kal_uint8  IMX135MIPI_MIN_EXPOSURE_LINES = 2;
kal_uint32 IMX135MIPI_isp_master_clock;
static DEFINE_SPINLOCK(imx135_drv_lock);

#define SENSORDB(fmt, arg...) printk( "[IMX135MIPIRaw] "  fmt, ##arg)
#define RETAILMSG(x,...)
#define TEXT
UINT8 IMX135MIPIPixelClockDivider=0;
kal_uint16 IMX135MIPI_sensor_id=0;
MSDK_SENSOR_CONFIG_STRUCT IMX135MIPISensorConfigData;
kal_uint32 IMX135MIPI_FAC_SENSOR_REG;
kal_uint16 IMX135MIPI_sensor_flip_value; 
																				 // Gain Index

#define IMX135MIPI_MaxGainIndex (71)
kal_uint16 IMX135MIPI_sensorGainMapping[IMX135MIPI_MaxGainIndex][2] ={
	{71  ,25 },
	{76  ,42 },
	{83  ,59 },
	{89  ,73 },
	{96  ,85 },
	{102 ,96 },
	{108 ,105},
	{115 ,114},
	{121 ,121},
	{128 ,128},
	{134 ,134},
	{140 ,140},
	{147 ,145},
	{153 ,149},
	{160 ,154},
	{166 ,158},
	{172 ,161},
	{179 ,164},
	{185 ,168},
	{192 ,171},
	{200 ,174},
	{208 ,177},
	{216 ,180},
	{224 ,183},
	{232 ,185},
	{240 ,188},
	{248 ,190},
	{256 ,192},
	{264 ,194},
	{272 ,196},
	{280 ,197},
	{288 ,199},
	{296 ,201},
	{304 ,202},
	{312 ,203},
	{320 ,205},
	{328 ,206},
	{336 ,207},
	{344 ,208},
	{352 ,209},
	{360 ,210},
	{368 ,211},
	{376 ,212},
	{384 ,213},
	{390 ,214},
	{399 ,215},
	{409 ,216},
	{419 ,217},
	{431 ,218},
	{442 ,219},
	{455 ,220},
	{467 ,221},
	{481 ,222},
	{496 ,223},
	{512 ,224},
	{528 ,225},
	{545 ,226},
	{565 ,227},
	{584 ,228},
	{606 ,229},
	{630 ,230},
	{655 ,231},
	{682 ,232},
	{712 ,233},
	{744 ,234},
	{780 ,235},
	{819 ,236},
	{862 ,237},
	{910 ,238},
	{963 ,239},
	{1024,240} 
};



/* FIXME: old factors and DIDNOT use now. s*/
SENSOR_REG_STRUCT IMX135MIPISensorCCT[]=CAMERA_SENSOR_CCT_DEFAULT_VALUE;
SENSOR_REG_STRUCT IMX135MIPISensorReg[ENGINEER_END]=CAMERA_SENSOR_REG_DEFAULT_VALUE;
/* FIXME: old factors and DIDNOT use now. e*/
extern int iReadReg(u16 a_u2Addr , u8 * a_puBuff , u16 i2cId);
extern int iWriteReg(u16 a_u2Addr , u32 a_u4Data , u32 a_u4Bytes , u16 i2cId);
#define IMX135MIPI_write_cmos_sensor(addr, para) iWriteReg((u16) addr , (u32) para , 1, IMX135MIPI_WRITE_ID)

kal_uint16 IMX135MIPI_read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte=0;
    iReadReg((u16) addr ,(u8*)&get_byte,IMX135MIPI_WRITE_ID);
    return get_byte;
}

void IMX135MIPI_write_shutter(kal_uint16 shutter)
{
	kal_uint32 frame_length = 0,line_length=0;
    kal_uint32 extra_lines = 0;
	kal_uint32 max_exp_shutter = 0;
	unsigned long flags;
//	SENSORDB("IMX135MIPI_write_shutter shutter = %d\n",shutter);
    if (IMX135MIPI_sensor.pv_mode == KAL_TRUE) 
	 {
	   max_exp_shutter = IMX135MIPI_PV_FRAME_LENGTH_LINES + IMX135MIPI_sensor.pv_dummy_lines-4;
     }
     else if (IMX135MIPI_sensor.video_mode== KAL_TRUE) 
     {
       max_exp_shutter = IMX135MIPI_VIDEO_FRAME_LENGTH_LINES + IMX135MIPI_sensor.video_dummy_lines-4;
	 }	 
	 else 
     {
       max_exp_shutter = IMX135MIPI_FULL_FRAME_LENGTH_LINES + IMX135MIPI_sensor.cp_dummy_lines-4;
	 }	 
	 
	 if(shutter > max_exp_shutter)
	   extra_lines = shutter - max_exp_shutter;
	 else 
	   extra_lines = 0;
	 if (IMX135MIPI_sensor.pv_mode == KAL_TRUE) 
	 {
       frame_length =IMX135MIPI_PV_FRAME_LENGTH_LINES+ IMX135MIPI_sensor.pv_dummy_lines + extra_lines;
	   line_length = IMX135MIPI_PV_LINE_LENGTH_PIXELS+ IMX135MIPI_sensor.pv_dummy_pixels;
	   spin_lock_irqsave(&imx135_drv_lock,flags);
	   IMX135MIPI_sensor.pv_line_length = line_length;
	   IMX135MIPI_sensor.pv_frame_length = frame_length;
	   spin_unlock_irqrestore(&imx135_drv_lock,flags);
	 }
	 else if (IMX135MIPI_sensor.video_mode== KAL_TRUE) 
     {
	    frame_length = IMX135MIPI_VIDEO_FRAME_LENGTH_LINES+ IMX135MIPI_sensor.video_dummy_lines + extra_lines;
		line_length =IMX135MIPI_VIDEO_LINE_LENGTH_PIXELS + IMX135MIPI_sensor.video_dummy_pixels;
		spin_lock_irqsave(&imx135_drv_lock,flags);
		IMX135MIPI_sensor.video_line_length = line_length;
	    IMX135MIPI_sensor.video_frame_length = frame_length;
		spin_unlock_irqrestore(&imx135_drv_lock,flags);
	 } 
	 else
	 	{
	    frame_length = IMX135MIPI_FULL_FRAME_LENGTH_LINES+ IMX135MIPI_sensor.cp_dummy_lines + extra_lines;
		line_length =IMX135MIPI_FULL_LINE_LENGTH_PIXELS + IMX135MIPI_sensor.cp_dummy_pixels;
		spin_lock_irqsave(&imx135_drv_lock,flags);
		IMX135MIPI_sensor.cp_line_length = line_length;
	    IMX135MIPI_sensor.cp_frame_length = frame_length;
		spin_unlock_irqrestore(&imx135_drv_lock,flags);
	 }
    IMX135MIPI_write_cmos_sensor(0x0104, 1);        
	IMX135MIPI_write_cmos_sensor(0x0340, (frame_length >>8) & 0xFF);
    IMX135MIPI_write_cmos_sensor(0x0341, frame_length & 0xFF);	

    IMX135MIPI_write_cmos_sensor(0x0202, (shutter >> 8) & 0xFF);
    IMX135MIPI_write_cmos_sensor(0x0203, shutter  & 0xFF);
    IMX135MIPI_write_cmos_sensor(0x0104, 0);    
	SENSORDB("IMX135MIPI_write_shutter frame_length = %d;shutter = %d\n",frame_length,shutter);

}   /* write_IMX135MIPI_shutter */

static kal_uint16 IMX135MIPIReg2Gain(const kal_uint8 iReg)
{

    kal_uint8 iI;
    // Range: 1x to 16x
    for (iI = 0; iI < IMX135MIPI_MaxGainIndex; iI++) {
        if(iReg <= IMX135MIPI_sensorGainMapping[iI][1]){
            break;
        }
    }
    return IMX135MIPI_sensorGainMapping[iI][0];

}
static kal_uint8 IMX135MIPIGain2Reg(const kal_uint16 iGain)
{

	kal_uint8 iI;
    
    for (iI = 0; iI < (IMX135MIPI_MaxGainIndex-1); iI++) {
        if(iGain <= IMX135MIPI_sensorGainMapping[iI][0]){    
            break;
        }
    }
    if(iGain != IMX135MIPI_sensorGainMapping[iI][0])
    {
         SENSORDB("[IMX135MIPIGain2Reg] Gain mapping don't correctly:%d %d \n", iGain, IMX135MIPI_sensorGainMapping[iI][0]);
    }
    return IMX135MIPI_sensorGainMapping[iI][1];
}


/*************************************************************************
* FUNCTION
*    IMX135MIPI_SetGain
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
void IMX135MIPI_SetGain(UINT16 iGain)
{
    kal_uint8 iReg;
//	SENSORDB("[IMX135MIPI_SetGain] SetGain=%d\n",  iGain);
    iReg = IMX135MIPIGain2Reg(iGain);
//	SENSORDB("[IMX135MIPI_SetGain ] RegisterGain:%d\n", iReg);
	
	IMX135MIPI_write_cmos_sensor(0x0104, 1);
    IMX135MIPI_write_cmos_sensor(0x0205, (kal_uint8)iReg);
    IMX135MIPI_write_cmos_sensor(0x0104, 0);

}   /*  IMX135MIPI_SetGain_SetGain  */


/*************************************************************************
* FUNCTION
*    read_IMX135MIPI_gain
*
* DESCRIPTION
*    This function is to set global gain to sensor.
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
kal_uint16 read_IMX135MIPI_gain(void)
{
    return (kal_uint16)(IMX135MIPI_read_cmos_sensor(0x0205)) ;
}  /* read_IMX135MIPI_gain */

void write_IMX135MIPI_gain(kal_uint16 gain)
{
    IMX135MIPI_SetGain(gain);
}
void IMX135MIPI_camera_para_to_sensor(void)
{

	kal_uint32    i;
    for(i=0; 0xFFFFFFFF!=IMX135MIPISensorReg[i].Addr; i++)
    {
        IMX135MIPI_write_cmos_sensor(IMX135MIPISensorReg[i].Addr, IMX135MIPISensorReg[i].Para);
    }
    for(i=ENGINEER_START_ADDR; 0xFFFFFFFF!=IMX135MIPISensorReg[i].Addr; i++)
    {
        IMX135MIPI_write_cmos_sensor(IMX135MIPISensorReg[i].Addr, IMX135MIPISensorReg[i].Para);
    }
    for(i=FACTORY_START_ADDR; i<FACTORY_END_ADDR; i++)
    {
        IMX135MIPI_write_cmos_sensor(IMX135MIPISensorCCT[i].Addr, IMX135MIPISensorCCT[i].Para);
    }

}


/*************************************************************************
* FUNCTION
*    IMX135MIPI_sensor_to_camera_para
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
void IMX135MIPI_sensor_to_camera_para(void)
{

	kal_uint32    i,temp_data;
    for(i=0; 0xFFFFFFFF!=IMX135MIPISensorReg[i].Addr; i++)
    {
		temp_data=IMX135MIPI_read_cmos_sensor(IMX135MIPISensorReg[i].Addr);
		spin_lock(&imx135_drv_lock);
		IMX135MIPISensorReg[i].Para = temp_data;
		spin_unlock(&imx135_drv_lock);
    }
    for(i=ENGINEER_START_ADDR; 0xFFFFFFFF!=IMX135MIPISensorReg[i].Addr; i++)
    {
    	temp_data=IMX135MIPI_read_cmos_sensor(IMX135MIPISensorReg[i].Addr);
         spin_lock(&imx135_drv_lock);
        IMX135MIPISensorReg[i].Para = temp_data;
		spin_unlock(&imx135_drv_lock);
    }

}

/*************************************************************************
* FUNCTION
*    IMX135MIPI_get_sensor_group_count
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
kal_int32  IMX135MIPI_get_sensor_group_count(void)
{
    return GROUP_TOTAL_NUMS;
}

void IMX135MIPI_get_sensor_group_info(kal_uint16 group_idx, kal_int8* group_name_ptr, kal_int32* item_count_ptr)
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

void IMX135MIPI_get_sensor_item_info(kal_uint16 group_idx,kal_uint16 item_idx, MSDK_SENSOR_ITEM_INFO_STRUCT* info_ptr)
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
                 SENSORDB("[IMX105MIPI][Error]get_sensor_item_info error!!!\n");
          }
           	spin_lock(&imx135_drv_lock);    
            temp_para=IMX135MIPISensorCCT[temp_addr].Para;	
			spin_unlock(&imx135_drv_lock);
            temp_gain = IMX135MIPIReg2Gain(temp_para);
            temp_gain=(temp_gain*1000)/BASEGAIN;
            info_ptr->ItemValue=temp_gain;
            info_ptr->IsTrueFalse=KAL_FALSE;
            info_ptr->IsReadOnly=KAL_FALSE;
            info_ptr->IsNeedRestart=KAL_FALSE;
            info_ptr->Min=1000;
            info_ptr->Max=15875;//why
            break;
        case CMMCLK_CURRENT:
            switch (item_idx)
            {
                case 0:
                    sprintf((char *)info_ptr->ItemNamePtr,"Drv Cur[2,4,6,8]mA");
                
                    //temp_reg=IMX135MIPISensorReg[CMMCLK_CURRENT_INDEX].Para;
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
                    info_ptr->ItemValue=IMX135MIPI_MAX_EXPOSURE_LINES;
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

kal_bool IMX135MIPI_set_sensor_item_info(kal_uint16 group_idx, kal_uint16 item_idx, kal_int32 ItemValue)
{
//   kal_int16 temp_reg;
   kal_uint16 temp_addr=0, temp_para=0;

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
                 SENSORDB("[IMX135MIPI][Error]set_sensor_item_info error!!!\n");
          }
            temp_para = IMX135MIPIGain2Reg(ItemValue);
            spin_lock(&imx135_drv_lock);    
            IMX135MIPISensorCCT[temp_addr].Para = temp_para;
			spin_unlock(&imx135_drv_lock);
            IMX135MIPI_write_cmos_sensor(IMX135MIPISensorCCT[temp_addr].Addr,temp_para);
			temp_para=read_IMX135MIPI_gain();	

            break;
        case CMMCLK_CURRENT:
            switch (item_idx)
            {
                case 0:
                    if(ItemValue==2)
                    {			
                    spin_lock(&imx135_drv_lock);    
                        IMX135MIPISensorReg[CMMCLK_CURRENT_INDEX].Para = ISP_DRIVING_2MA;
					spin_unlock(&imx135_drv_lock);
                    }
                    else if(ItemValue==3 || ItemValue==4)
                    {
                    	spin_lock(&imx135_drv_lock);    
                        IMX135MIPISensorReg[CMMCLK_CURRENT_INDEX].Para = ISP_DRIVING_4MA;
						spin_unlock(&imx135_drv_lock);
                    }
                    else if(ItemValue==5 || ItemValue==6)
                    {
                    	spin_lock(&imx135_drv_lock);    
                        IMX135MIPISensorReg[CMMCLK_CURRENT_INDEX].Para = ISP_DRIVING_6MA;
						spin_unlock(&imx135_drv_lock);
                    }
                    else
                    {
                    	spin_lock(&imx135_drv_lock);    
                        IMX135MIPISensorReg[CMMCLK_CURRENT_INDEX].Para = ISP_DRIVING_8MA;
						spin_unlock(&imx135_drv_lock);
                    }
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
					spin_lock(&imx135_drv_lock);    
                    IMX135MIPI_FAC_SENSOR_REG=ItemValue;
					spin_unlock(&imx135_drv_lock);
                    break;
                case 1:
                    IMX135MIPI_write_cmos_sensor(IMX135MIPI_FAC_SENSOR_REG,ItemValue);
                    break;
                default:
                    ASSERT(0);
            }
            break;
        default:
            ASSERT(0);
    }
    return TRUE;
}

static void IMX135MIPI_SetDummy(const kal_uint16 iPixels, const kal_uint16 iLines)
{
  kal_uint32 frame_length = 0, line_length = 0;
   if(IMX135MIPI_sensor.pv_mode == KAL_TRUE)
   	{
   	 spin_lock(&imx135_drv_lock);    
   	 IMX135MIPI_sensor.pv_dummy_pixels = iPixels;
	 IMX135MIPI_sensor.pv_dummy_lines = iLines;
   	 IMX135MIPI_sensor.pv_line_length = IMX135MIPI_PV_LINE_LENGTH_PIXELS + iPixels;
	 IMX135MIPI_sensor.pv_frame_length = IMX135MIPI_PV_FRAME_LENGTH_LINES + iLines;
	 spin_unlock(&imx135_drv_lock);
	 line_length = IMX135MIPI_sensor.pv_line_length;
	 frame_length = IMX135MIPI_sensor.pv_frame_length;
	 	
   	}
   else if(IMX135MIPI_sensor.video_mode == KAL_TRUE)
   	{
   	 spin_lock(&imx135_drv_lock);    
   	 IMX135MIPI_sensor.video_dummy_pixels = iPixels;
	 IMX135MIPI_sensor.video_dummy_lines = iLines;
   	 IMX135MIPI_sensor.video_line_length = IMX135MIPI_VIDEO_LINE_LENGTH_PIXELS + iPixels;
	 IMX135MIPI_sensor.video_frame_length = IMX135MIPI_VIDEO_FRAME_LENGTH_LINES + iLines;
	 spin_unlock(&imx135_drv_lock);
	 line_length = IMX135MIPI_sensor.video_line_length;
	 frame_length = IMX135MIPI_sensor.video_frame_length;
   	}

	else
		{
	  spin_lock(&imx135_drv_lock);	
   	  IMX135MIPI_sensor.cp_dummy_pixels = iPixels;
	  IMX135MIPI_sensor.cp_dummy_lines = iLines;
	  IMX135MIPI_sensor.cp_line_length = IMX135MIPI_FULL_LINE_LENGTH_PIXELS + iPixels;
	  IMX135MIPI_sensor.cp_frame_length = IMX135MIPI_FULL_FRAME_LENGTH_LINES + iLines;
	   spin_unlock(&imx135_drv_lock);
	  line_length = IMX135MIPI_sensor.cp_line_length;
	  frame_length = IMX135MIPI_sensor.cp_frame_length;
    }

      IMX135MIPI_write_cmos_sensor(0x0104, 1);        
	  
      IMX135MIPI_write_cmos_sensor(0x0340, (frame_length >>8) & 0xFF);
      IMX135MIPI_write_cmos_sensor(0x0341, frame_length & 0xFF);	
      IMX135MIPI_write_cmos_sensor(0x0342, (line_length >>8) & 0xFF);
      IMX135MIPI_write_cmos_sensor(0x0343, line_length & 0xFF);

      IMX135MIPI_write_cmos_sensor(0x0104, 0);

	  SENSORDB("IMX135MIPI_SetDummy,dumy_pixel=%d,dumy_line=%d,\n",iPixels,iLines);
  
}   /*  IMX135MIPI_SetDummy */

static void IMX135MIPI_Sensor_Init(void)
{
    SENSORDB("IMX135MIPI_Sensor_Init enter:\n");
	IMX135MIPI_write_cmos_sensor(0x0101, 0x00);//
	IMX135MIPI_write_cmos_sensor(0x0105, 0x01);//
	IMX135MIPI_write_cmos_sensor(0x0110, 0x00);//
	IMX135MIPI_write_cmos_sensor(0x0220, 0x01);//
	IMX135MIPI_write_cmos_sensor(0x3302, 0x11);//
	IMX135MIPI_write_cmos_sensor(0x3833, 0x20);//
	IMX135MIPI_write_cmos_sensor(0x3893, 0x00);//
	IMX135MIPI_write_cmos_sensor(0x3906, 0x08);//
	IMX135MIPI_write_cmos_sensor(0x3907, 0x01);//
	IMX135MIPI_write_cmos_sensor(0x391B, 0x01);//
	IMX135MIPI_write_cmos_sensor(0x3C09, 0x01);//
	IMX135MIPI_write_cmos_sensor(0x600A, 0x00);//
	IMX135MIPI_write_cmos_sensor(0x3008, 0xB0);//
	IMX135MIPI_write_cmos_sensor(0x320A, 0x01);//
	IMX135MIPI_write_cmos_sensor(0x320D, 0x10);//
	IMX135MIPI_write_cmos_sensor(0x3216, 0x2E);//
	IMX135MIPI_write_cmos_sensor(0x322C, 0x02);//
	IMX135MIPI_write_cmos_sensor(0x3409, 0x0C);//
	IMX135MIPI_write_cmos_sensor(0x340C, 0x2D);//
	IMX135MIPI_write_cmos_sensor(0x3411, 0x39);//
	IMX135MIPI_write_cmos_sensor(0x3414, 0x1E);//
	IMX135MIPI_write_cmos_sensor(0x3427, 0x04);//
	IMX135MIPI_write_cmos_sensor(0x3480, 0x1E);//
	IMX135MIPI_write_cmos_sensor(0x3484, 0x1E);//
	IMX135MIPI_write_cmos_sensor(0x3488, 0x1E);//
	IMX135MIPI_write_cmos_sensor(0x348C, 0x1E);//
	IMX135MIPI_write_cmos_sensor(0x3490, 0x1E);//
	IMX135MIPI_write_cmos_sensor(0x3494, 0x1E);//
	IMX135MIPI_write_cmos_sensor(0x3511, 0x8F);//
	IMX135MIPI_write_cmos_sensor(0x364F, 0x2D);//

//quality

	//defect forrection recommended setting
	
	IMX135MIPI_write_cmos_sensor(0x380A, 0x00);//
	IMX135MIPI_write_cmos_sensor(0x380B, 0x00);//
	IMX135MIPI_write_cmos_sensor(0x4103, 0x00);//

	//color artifact recommended setting
	
	IMX135MIPI_write_cmos_sensor(0x4243, 0x9A);//
	IMX135MIPI_write_cmos_sensor(0x4330, 0x01);//
	IMX135MIPI_write_cmos_sensor(0x4331, 0x90);//
	IMX135MIPI_write_cmos_sensor(0x4332, 0x02);//
	IMX135MIPI_write_cmos_sensor(0x4333, 0x58);//
	IMX135MIPI_write_cmos_sensor(0x4334, 0x03);//
	IMX135MIPI_write_cmos_sensor(0x4335, 0x20);//
	IMX135MIPI_write_cmos_sensor(0x4336, 0x03);//
	IMX135MIPI_write_cmos_sensor(0x4337, 0x84);//
	IMX135MIPI_write_cmos_sensor(0x433C, 0x01);//
	IMX135MIPI_write_cmos_sensor(0x4340, 0x02);//
	IMX135MIPI_write_cmos_sensor(0x4341, 0x58);//
	IMX135MIPI_write_cmos_sensor(0x4342, 0x03);//
	IMX135MIPI_write_cmos_sensor(0x4343, 0x52);//

	
	/////Moire reduction parameter setting
	
	IMX135MIPI_write_cmos_sensor(0x4364, 0x0B);//
	IMX135MIPI_write_cmos_sensor(0x4368, 0x00);//
	IMX135MIPI_write_cmos_sensor(0x4369, 0x0F);//
	IMX135MIPI_write_cmos_sensor(0x436A, 0x03);//
	IMX135MIPI_write_cmos_sensor(0x436B, 0xA8);//
	IMX135MIPI_write_cmos_sensor(0x436C, 0x00);//
	IMX135MIPI_write_cmos_sensor(0x436D, 0x00);//
	IMX135MIPI_write_cmos_sensor(0x436E, 0x00);//
	IMX135MIPI_write_cmos_sensor(0x436F, 0x06);//

	//CNR parameter setting

	IMX135MIPI_write_cmos_sensor(0x4281, 0x21);//
	IMX135MIPI_write_cmos_sensor(0x4282, 0x18);//
	IMX135MIPI_write_cmos_sensor(0x4283, 0x04);//
	IMX135MIPI_write_cmos_sensor(0x4284, 0x08);//
	IMX135MIPI_write_cmos_sensor(0x4287, 0x7F);//
	IMX135MIPI_write_cmos_sensor(0x4288, 0x08);//
	IMX135MIPI_write_cmos_sensor(0x428B, 0x7F);//
	IMX135MIPI_write_cmos_sensor(0x428C, 0x08);//
	IMX135MIPI_write_cmos_sensor(0x428F, 0x7F);//
	IMX135MIPI_write_cmos_sensor(0x4297, 0x00);//
	IMX135MIPI_write_cmos_sensor(0x4298, 0x7E);//
	IMX135MIPI_write_cmos_sensor(0x4299, 0x7E);//
	IMX135MIPI_write_cmos_sensor(0x429A, 0x7E);//
	IMX135MIPI_write_cmos_sensor(0x42A4, 0xFB);//
	IMX135MIPI_write_cmos_sensor(0x42A5, 0x7E);//
	IMX135MIPI_write_cmos_sensor(0x42A6, 0xDF);//
	IMX135MIPI_write_cmos_sensor(0x42A7, 0xB7);//
	IMX135MIPI_write_cmos_sensor(0x42AF, 0x03);//
	
   // ARNR Parameter setting
	IMX135MIPI_write_cmos_sensor(0x4207, 0x03);//
	IMX135MIPI_write_cmos_sensor(0x4216, 0x08);//
	IMX135MIPI_write_cmos_sensor(0x4217, 0x08);//

	//DLC Parammeter setting
	IMX135MIPI_write_cmos_sensor(0x4218, 0x00);//
	IMX135MIPI_write_cmos_sensor(0x421B, 0x20);//
	IMX135MIPI_write_cmos_sensor(0x421F, 0x04);//
	IMX135MIPI_write_cmos_sensor(0x4222, 0x02);//
	IMX135MIPI_write_cmos_sensor(0x4223, 0x22);//
	IMX135MIPI_write_cmos_sensor(0x422E, 0x54);//
	IMX135MIPI_write_cmos_sensor(0x422F, 0xFB);//
	IMX135MIPI_write_cmos_sensor(0x4230, 0xFF);//
	IMX135MIPI_write_cmos_sensor(0x4231, 0xFE);//
	IMX135MIPI_write_cmos_sensor(0x4232, 0xFF);//
	IMX135MIPI_write_cmos_sensor(0x4235, 0x58);//
	IMX135MIPI_write_cmos_sensor(0x4236, 0xF7);//
	IMX135MIPI_write_cmos_sensor(0x4237, 0xFD);//
	IMX135MIPI_write_cmos_sensor(0x4239, 0x4E);//
	IMX135MIPI_write_cmos_sensor(0x423A, 0xFC);//
	IMX135MIPI_write_cmos_sensor(0x423B, 0xFD);//
	
	//HDR
	

	//LSC setting
	IMX135MIPI_write_cmos_sensor(0x452A, 0x02);//


	//white balance setting
	IMX135MIPI_write_cmos_sensor(0x0712, 0x01);//
	IMX135MIPI_write_cmos_sensor(0x0713, 0x00);//
	IMX135MIPI_write_cmos_sensor(0x0714, 0x01);//
	IMX135MIPI_write_cmos_sensor(0x0715, 0x00);//
	IMX135MIPI_write_cmos_sensor(0x0716, 0x01);//
	IMX135MIPI_write_cmos_sensor(0x0717, 0x00);//
	IMX135MIPI_write_cmos_sensor(0x0718, 0x01);//
	IMX135MIPI_write_cmos_sensor(0x0719, 0x00);//

	//shading setting
	IMX135MIPI_write_cmos_sensor(0x4500, 0x1F);//
	
	SENSORDB("IMX135MIPI_Sensor_Init exit\n");

    // The register only need to enable 1 time.    
    spin_lock(&imx135_drv_lock);  
    IMX135MIPI_Auto_Flicker_mode = KAL_FALSE;     // reset the flicker status    
	spin_unlock(&imx135_drv_lock);
}   /*  IMX135MIPI_Sensor_Init  */
void VideoFullSizeSetting(void)//16:9   6M

{	
	SENSORDB("VideoFullSizeSetting enter:\n");

 //PLL setting 
	IMX135MIPI_write_cmos_sensor(0x0100,0x00);// STREAM STop

	//PLL setting
    IMX135MIPI_write_cmos_sensor(0x011E,0x18);//   
	IMX135MIPI_write_cmos_sensor(0x011F,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x0301,0x05);//   
	IMX135MIPI_write_cmos_sensor(0x0303,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x0305,0x0B);//   
	IMX135MIPI_write_cmos_sensor(0x0309,0x05);//   
	IMX135MIPI_write_cmos_sensor(0x030B,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x030C,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x030D,0x09);//   
	IMX135MIPI_write_cmos_sensor(0x030E,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x3A06,0x11);//   

	//Mode setting
	IMX135MIPI_write_cmos_sensor(0x0108,0x03);//   
	IMX135MIPI_write_cmos_sensor(0x0112,0x0A);//   
	IMX135MIPI_write_cmos_sensor(0x0113,0x0A);//   
	IMX135MIPI_write_cmos_sensor(0x0381,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x0383,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x0385,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x0387,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x0390,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x0391,0x22);//   
	IMX135MIPI_write_cmos_sensor(0x0392,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x0401,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x0404,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x0405,0x10);//   
	IMX135MIPI_write_cmos_sensor(0x4082,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x4083,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x7006,0x04);//

	//Optionnal function setting
	IMX135MIPI_write_cmos_sensor(0x0700,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x3A63,0x00);//
	IMX135MIPI_write_cmos_sensor(0x4100,0xF8);//   
	IMX135MIPI_write_cmos_sensor(0x4203,0xFF);//   
	IMX135MIPI_write_cmos_sensor(0x4344,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x441C,0x01);//

	//Size setting
	IMX135MIPI_write_cmos_sensor(0x0340,(IMX135MIPI_VIDEO_FRAME_LENGTH_LINES>>8)&0xFF);//   
	IMX135MIPI_write_cmos_sensor(0x0341,(IMX135MIPI_VIDEO_FRAME_LENGTH_LINES>>0)&0xFF);//	
	IMX135MIPI_write_cmos_sensor(0x0342,(IMX135MIPI_VIDEO_LINE_LENGTH_PIXELS>>8)&0xFF);// 
	IMX135MIPI_write_cmos_sensor(0x0343,(IMX135MIPI_VIDEO_LINE_LENGTH_PIXELS>>0)&0xFF);//
	IMX135MIPI_write_cmos_sensor(0x0344,0x00);//
	IMX135MIPI_write_cmos_sensor(0x0345,0x00);//
	IMX135MIPI_write_cmos_sensor(0x0346,0x00);//
	IMX135MIPI_write_cmos_sensor(0x0347,0x00);//
	IMX135MIPI_write_cmos_sensor(0x0348,0x10);//
	IMX135MIPI_write_cmos_sensor(0x0349,0x6F);//
	IMX135MIPI_write_cmos_sensor(0x034A,0x0C);//
	IMX135MIPI_write_cmos_sensor(0x034B,0x2F);//
	IMX135MIPI_write_cmos_sensor(0x034C,0x08);//
	IMX135MIPI_write_cmos_sensor(0x034D,0x38);//
	IMX135MIPI_write_cmos_sensor(0x034E,0x06);//
	IMX135MIPI_write_cmos_sensor(0x034F,0x18);//
	IMX135MIPI_write_cmos_sensor(0x0350,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x0351,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x0352,0x00);// 
	IMX135MIPI_write_cmos_sensor(0x0353,0x00);//
	IMX135MIPI_write_cmos_sensor(0x0354,0x08);//
	IMX135MIPI_write_cmos_sensor(0x0355,0x38);//
	IMX135MIPI_write_cmos_sensor(0x0356,0x06);//
	IMX135MIPI_write_cmos_sensor(0x0357,0x18);//	
	IMX135MIPI_write_cmos_sensor(0x301D,0x30);//   
	IMX135MIPI_write_cmos_sensor(0x3310,0x08);//   
	IMX135MIPI_write_cmos_sensor(0x3311,0x38);//   
	IMX135MIPI_write_cmos_sensor(0x3312,0x06);//   
	IMX135MIPI_write_cmos_sensor(0x3313,0x18);//   
	IMX135MIPI_write_cmos_sensor(0x331C,0x04);//   
	IMX135MIPI_write_cmos_sensor(0x331D,0xAB);//   
	IMX135MIPI_write_cmos_sensor(0x4084,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x4085,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x4086,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x4087,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x4400,0x00);//

	//global timing setting
	IMX135MIPI_write_cmos_sensor(0x0830,0x6F);//   
	IMX135MIPI_write_cmos_sensor(0x0831,0x27);//  
	IMX135MIPI_write_cmos_sensor(0x0832,0x4F);//   
	IMX135MIPI_write_cmos_sensor(0x0833,0x2F);// 
	IMX135MIPI_write_cmos_sensor(0x0834,0x2F);//   
	IMX135MIPI_write_cmos_sensor(0x0835,0x2F);// 
	IMX135MIPI_write_cmos_sensor(0x0836,0x9F);//   
	IMX135MIPI_write_cmos_sensor(0x0837,0x37);//
	IMX135MIPI_write_cmos_sensor(0x0839,0x1F);//
	IMX135MIPI_write_cmos_sensor(0x083A,0x17);//   
	IMX135MIPI_write_cmos_sensor(0x083B,0x02);// 

	// integration time setting
	IMX135MIPI_write_cmos_sensor(0x0202,0x06);//
	IMX135MIPI_write_cmos_sensor(0x0203,0x64);//

	//gain setting
	IMX135MIPI_write_cmos_sensor(0x0205,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x020E,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x020F,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x0210,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x0211,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x0212,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x0213,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x0214,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x0215,0x00);//

#if 0
	//hdr setting
	IMX135MIPI_write_cmos_sensor(0x0230,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x0231,0x00);//     
	IMX135MIPI_write_cmos_sensor(0x0233,0x00);//
	IMX135MIPI_write_cmos_sensor(0x0234,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x0235,0x40);//
	IMX135MIPI_write_cmos_sensor(0x0236,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x0238,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x0239,0x04);//
	IMX135MIPI_write_cmos_sensor(0x023B,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x023C,0x00);//
	IMX135MIPI_write_cmos_sensor(0x33B0,0x04);//   
	IMX135MIPI_write_cmos_sensor(0x33B1,0x00);//
	IMX135MIPI_write_cmos_sensor(0x33B3,0X00);//   
	IMX135MIPI_write_cmos_sensor(0x33B4,0X01);//
	IMX135MIPI_write_cmos_sensor(0x3800,0X00);//

	#endif
	
	IMX135MIPI_write_cmos_sensor(0x0100,0x01);// STREAM START

    SENSORDB("VideoFullSizeSetting exit\n");
}


void PreviewSetting(void)
{	

   SENSORDB("PreviewSetting enter:\n");
  //PLL setting 
	IMX135MIPI_write_cmos_sensor(0x0100,0x00);// STREAM STop

	//PLL setting
    IMX135MIPI_write_cmos_sensor(0x011E,0x18);//   
	IMX135MIPI_write_cmos_sensor(0x011F,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x0301,0x05);//   
	IMX135MIPI_write_cmos_sensor(0x0303,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x0305,0x0B);//   
	IMX135MIPI_write_cmos_sensor(0x0309,0x05);//   
	IMX135MIPI_write_cmos_sensor(0x030B,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x030C,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x030D,0x09);//   
	IMX135MIPI_write_cmos_sensor(0x030E,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x3A06,0x11);//   

	//Mode setting
	IMX135MIPI_write_cmos_sensor(0x0108,0x03);//   
	IMX135MIPI_write_cmos_sensor(0x0112,0x0A);//   
	IMX135MIPI_write_cmos_sensor(0x0113,0x0A);//   
	IMX135MIPI_write_cmos_sensor(0x0381,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x0383,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x0385,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x0387,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x0390,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x0391,0x22);//   
	IMX135MIPI_write_cmos_sensor(0x0392,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x0401,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x0404,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x0405,0x10);//   
	IMX135MIPI_write_cmos_sensor(0x4082,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x4083,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x7006,0x04);//

	//Optionnal function setting
	IMX135MIPI_write_cmos_sensor(0x0700,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x3A63,0x00);//
	IMX135MIPI_write_cmos_sensor(0x4100,0xF8);//   
	IMX135MIPI_write_cmos_sensor(0x4203,0xFF);//   
	IMX135MIPI_write_cmos_sensor(0x4344,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x441C,0x01);//

	//Size setting
	IMX135MIPI_write_cmos_sensor(0x0340,(IMX135MIPI_PV_FRAME_LENGTH_LINES>>8)&0xFF);//   
	IMX135MIPI_write_cmos_sensor(0x0341,(IMX135MIPI_PV_FRAME_LENGTH_LINES>>0)&0xFF);//   
	IMX135MIPI_write_cmos_sensor(0x0342,(IMX135MIPI_PV_LINE_LENGTH_PIXELS>>8)&0xFF);//
	IMX135MIPI_write_cmos_sensor(0x0343,(IMX135MIPI_PV_LINE_LENGTH_PIXELS>>0)&0xFF);//
	IMX135MIPI_write_cmos_sensor(0x0344,0x00);//
	IMX135MIPI_write_cmos_sensor(0x0345,0x00);//
	IMX135MIPI_write_cmos_sensor(0x0346,0x00);//
	IMX135MIPI_write_cmos_sensor(0x0347,0x00);//
	IMX135MIPI_write_cmos_sensor(0x0348,0x10);//
	IMX135MIPI_write_cmos_sensor(0x0349,0x6F);//
	IMX135MIPI_write_cmos_sensor(0x034A,0x0C);//
	IMX135MIPI_write_cmos_sensor(0x034B,0x2F);//
	IMX135MIPI_write_cmos_sensor(0x034C,0x08);//
	IMX135MIPI_write_cmos_sensor(0x034D,0x38);//
	IMX135MIPI_write_cmos_sensor(0x034E,0x06);//
	IMX135MIPI_write_cmos_sensor(0x034F,0x18);//
	IMX135MIPI_write_cmos_sensor(0x0350,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x0351,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x0352,0x00);// 
	IMX135MIPI_write_cmos_sensor(0x0353,0x00);//
	IMX135MIPI_write_cmos_sensor(0x0354,0x08);//
	IMX135MIPI_write_cmos_sensor(0x0355,0x38);//
	IMX135MIPI_write_cmos_sensor(0x0356,0x06);//
	IMX135MIPI_write_cmos_sensor(0x0357,0x18);//	
	IMX135MIPI_write_cmos_sensor(0x301D,0x30);//   
	IMX135MIPI_write_cmos_sensor(0x3310,0x08);//   
	IMX135MIPI_write_cmos_sensor(0x3311,0x38);//   
	IMX135MIPI_write_cmos_sensor(0x3312,0x06);//   
	IMX135MIPI_write_cmos_sensor(0x3313,0x18);//   
	IMX135MIPI_write_cmos_sensor(0x331C,0x04);//   
	IMX135MIPI_write_cmos_sensor(0x331D,0xAB);//   
	IMX135MIPI_write_cmos_sensor(0x4084,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x4085,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x4086,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x4087,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x4400,0x00);//

	//global timing setting
	IMX135MIPI_write_cmos_sensor(0x0830,0x6F);//   
	IMX135MIPI_write_cmos_sensor(0x0831,0x27);//  
	IMX135MIPI_write_cmos_sensor(0x0832,0x4F);//   
	IMX135MIPI_write_cmos_sensor(0x0833,0x2F);// 
	IMX135MIPI_write_cmos_sensor(0x0834,0x2F);//   
	IMX135MIPI_write_cmos_sensor(0x0835,0x2F);// 
	IMX135MIPI_write_cmos_sensor(0x0836,0x9F);//   
	IMX135MIPI_write_cmos_sensor(0x0837,0x37);//
	IMX135MIPI_write_cmos_sensor(0x0839,0x1F);//
	IMX135MIPI_write_cmos_sensor(0x083A,0x17);//   
	IMX135MIPI_write_cmos_sensor(0x083B,0x02);// 

	// integration time setting
	IMX135MIPI_write_cmos_sensor(0x0202,0x06);//
	IMX135MIPI_write_cmos_sensor(0x0203,0x64);//

	//gain setting
	IMX135MIPI_write_cmos_sensor(0x0205,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x020E,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x020F,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x0210,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x0211,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x0212,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x0213,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x0214,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x0215,0x00);//

#if 0
	//hdr setting
	IMX135MIPI_write_cmos_sensor(0x0230,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x0231,0x00);//     
	IMX135MIPI_write_cmos_sensor(0x0233,0x00);//
	IMX135MIPI_write_cmos_sensor(0x0234,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x0235,0x40);//
	IMX135MIPI_write_cmos_sensor(0x0236,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x0238,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x0239,0x04);//
	IMX135MIPI_write_cmos_sensor(0x023B,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x023C,0x00);//
	IMX135MIPI_write_cmos_sensor(0x33B0,0x04);//   
	IMX135MIPI_write_cmos_sensor(0x33B1,0x00);//
	IMX135MIPI_write_cmos_sensor(0x33B3,0X00);//   
	IMX135MIPI_write_cmos_sensor(0x33B4,0X01);//
	IMX135MIPI_write_cmos_sensor(0x3800,0X00);//

	#endif
	
	IMX135MIPI_write_cmos_sensor(0x0100,0x01);// STREAM START


     SENSORDB("PreviewSetting exit\n");
}

void IMX135MIPI_set_13M(void)
{	

	SENSORDB("IMX135MIPI_set_13M Capture setting enter:\n");

	IMX135MIPI_write_cmos_sensor(0x0100,0x00);// STREAM STop
#if defined(IMX135_CAPTURE_24FPS)
	//ClockSetting
	IMX135MIPI_write_cmos_sensor(0x011E,0x18);
	IMX135MIPI_write_cmos_sensor(0x011F,0x00);
	IMX135MIPI_write_cmos_sensor(0x0301,0x05);
	IMX135MIPI_write_cmos_sensor(0x0303,0x01);
	IMX135MIPI_write_cmos_sensor(0x0305,0x0C);
	IMX135MIPI_write_cmos_sensor(0x0309,0x05);
	IMX135MIPI_write_cmos_sensor(0x030B,0x01);
	IMX135MIPI_write_cmos_sensor(0x030C,0x01);
	IMX135MIPI_write_cmos_sensor(0x030D,0xB3);
	IMX135MIPI_write_cmos_sensor(0x030E,0x01);
	IMX135MIPI_write_cmos_sensor(0x3A06,0x11);
	
	//Modesetting
	IMX135MIPI_write_cmos_sensor(0x0108,0x03);
	IMX135MIPI_write_cmos_sensor(0x0112,0x0A);
	IMX135MIPI_write_cmos_sensor(0x0113,0x0A);
	IMX135MIPI_write_cmos_sensor(0x0381,0x01);
	IMX135MIPI_write_cmos_sensor(0x0383,0x01);
	IMX135MIPI_write_cmos_sensor(0x0385,0x01);
	IMX135MIPI_write_cmos_sensor(0x0387,0x01);
	IMX135MIPI_write_cmos_sensor(0x0390,0x00);
	IMX135MIPI_write_cmos_sensor(0x0391,0x11);
	IMX135MIPI_write_cmos_sensor(0x0392,0x00);
	IMX135MIPI_write_cmos_sensor(0x0401,0x00);
	IMX135MIPI_write_cmos_sensor(0x0404,0x00);
	IMX135MIPI_write_cmos_sensor(0x0405,0x10);
	IMX135MIPI_write_cmos_sensor(0x4082,0x01);
	IMX135MIPI_write_cmos_sensor(0x4083,0x01);
	IMX135MIPI_write_cmos_sensor(0x7006,0x04);
	
	//OptionnalFunctionsetting
	IMX135MIPI_write_cmos_sensor(0x0700,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x3A63,0x00);//
	IMX135MIPI_write_cmos_sensor(0x4100,0xF8);
	IMX135MIPI_write_cmos_sensor(0x4203,0xFF);
	IMX135MIPI_write_cmos_sensor(0x4344,0x00);
	IMX135MIPI_write_cmos_sensor(0x441C,0x01);
	
	//Sizesetting
	IMX135MIPI_write_cmos_sensor(0x0340,(IMX135MIPI_FULL_FRAME_LENGTH_LINES>>8)&0xFF);//   
	IMX135MIPI_write_cmos_sensor(0x0341,(IMX135MIPI_FULL_FRAME_LENGTH_LINES>>0)&0xFF);//   
	IMX135MIPI_write_cmos_sensor(0x0342,(IMX135MIPI_FULL_LINE_LENGTH_PIXELS>>8)&0xFF);//
	IMX135MIPI_write_cmos_sensor(0x0343,(IMX135MIPI_FULL_LINE_LENGTH_PIXELS>>0)&0xFF);//
	IMX135MIPI_write_cmos_sensor(0x0344,0x00);
	IMX135MIPI_write_cmos_sensor(0x0345,0x00);
	IMX135MIPI_write_cmos_sensor(0x0346,0x00);
	IMX135MIPI_write_cmos_sensor(0x0347,0x00);
	IMX135MIPI_write_cmos_sensor(0x0348,0x10);
	IMX135MIPI_write_cmos_sensor(0x0349,0x6F);
	IMX135MIPI_write_cmos_sensor(0x034A,0x0C);
	IMX135MIPI_write_cmos_sensor(0x034B,0x2F);
	IMX135MIPI_write_cmos_sensor(0x034C,0x10);
	IMX135MIPI_write_cmos_sensor(0x034D,0x70);
	IMX135MIPI_write_cmos_sensor(0x034E,0x0C);
	IMX135MIPI_write_cmos_sensor(0x034F,0x30);
	IMX135MIPI_write_cmos_sensor(0x0350,0x00);
	IMX135MIPI_write_cmos_sensor(0x0351,0x00);
	IMX135MIPI_write_cmos_sensor(0x0352,0x00);
	IMX135MIPI_write_cmos_sensor(0x0353,0x00);
	IMX135MIPI_write_cmos_sensor(0x0354,0x10);
	IMX135MIPI_write_cmos_sensor(0x0355,0x70);
	IMX135MIPI_write_cmos_sensor(0x0356,0x0C);
	IMX135MIPI_write_cmos_sensor(0x0357,0x30);
	IMX135MIPI_write_cmos_sensor(0x301D,0x30);
	IMX135MIPI_write_cmos_sensor(0x3310,0x10);
	IMX135MIPI_write_cmos_sensor(0x3311,0x70);
	IMX135MIPI_write_cmos_sensor(0x3312,0x0C);
	IMX135MIPI_write_cmos_sensor(0x3313,0x30);
	IMX135MIPI_write_cmos_sensor(0x331C,0x01);
	IMX135MIPI_write_cmos_sensor(0x331D,0x68);
	IMX135MIPI_write_cmos_sensor(0x4084,0x00);
	IMX135MIPI_write_cmos_sensor(0x4085,0x00);
	IMX135MIPI_write_cmos_sensor(0x4086,0x00);
	IMX135MIPI_write_cmos_sensor(0x4087,0x00);
	IMX135MIPI_write_cmos_sensor(0x4400,0x00);
	
	//GlobalTimingSetting
	IMX135MIPI_write_cmos_sensor(0x0830,0x7F);
	IMX135MIPI_write_cmos_sensor(0x0831,0x37);
	IMX135MIPI_write_cmos_sensor(0x0832,0x67);
	IMX135MIPI_write_cmos_sensor(0x0833,0x3F);
	IMX135MIPI_write_cmos_sensor(0x0834,0x3F);
	IMX135MIPI_write_cmos_sensor(0x0835,0x47);
	IMX135MIPI_write_cmos_sensor(0x0836,0xDF);
	IMX135MIPI_write_cmos_sensor(0x0837,0x47);
	IMX135MIPI_write_cmos_sensor(0x0839,0x1F);
	IMX135MIPI_write_cmos_sensor(0x083A,0x17);
	IMX135MIPI_write_cmos_sensor(0x083B,0x02);
	
	//IntegrationTimeSetting
	IMX135MIPI_write_cmos_sensor(0x0202,0x0C);
	IMX135MIPI_write_cmos_sensor(0x0203,0x46);
	
	//GainSetting
	IMX135MIPI_write_cmos_sensor(0x0205,0x00);
	IMX135MIPI_write_cmos_sensor(0x020E,0x01);
	IMX135MIPI_write_cmos_sensor(0x020F,0x00);
	IMX135MIPI_write_cmos_sensor(0x0210,0x01);
	IMX135MIPI_write_cmos_sensor(0x0211,0x00);
	IMX135MIPI_write_cmos_sensor(0x0212,0x01);
	IMX135MIPI_write_cmos_sensor(0x0213,0x00);
	IMX135MIPI_write_cmos_sensor(0x0214,0x01);
	IMX135MIPI_write_cmos_sensor(0x0215,0x00);
	
	//HDRSetting
#if 0
	IMX135MIPI_write_cmos_sensor(0x0230,0x00);
	IMX135MIPI_write_cmos_sensor(0x0231,0x00);
	IMX135MIPI_write_cmos_sensor(0x0233,0x00);
	IMX135MIPI_write_cmos_sensor(0x0234,0x00);
	IMX135MIPI_write_cmos_sensor(0x0235,0x40);
	IMX135MIPI_write_cmos_sensor(0x0238,0x00);
	IMX135MIPI_write_cmos_sensor(0x0239,0x04);
	IMX135MIPI_write_cmos_sensor(0x023B,0x00);
	IMX135MIPI_write_cmos_sensor(0x023C,0x01);
	IMX135MIPI_write_cmos_sensor(0x33B0,0x04);
	IMX135MIPI_write_cmos_sensor(0x33B1,0x00);
	IMX135MIPI_write_cmos_sensor(0x33B3,0x00);
	IMX135MIPI_write_cmos_sensor(0x33B4,0x01);
	IMX135MIPI_write_cmos_sensor(0x3800,0x00);
#endif//HDRSetting
#else
	//PLL setting
	IMX135MIPI_write_cmos_sensor(0x011E,0x18);//   
	IMX135MIPI_write_cmos_sensor(0x011F,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x0301,0x05);//   
	IMX135MIPI_write_cmos_sensor(0x0303,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x0305,0x0B);//   
	IMX135MIPI_write_cmos_sensor(0x0309,0x05);//   
	IMX135MIPI_write_cmos_sensor(0x030B,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x030C,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x030D,0x29);//   
	IMX135MIPI_write_cmos_sensor(0x030E,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x3A06,0x11);//   

	//Mode setting
	IMX135MIPI_write_cmos_sensor(0x0108,0x03);//   
	IMX135MIPI_write_cmos_sensor(0x0112,0x0A);//   
	IMX135MIPI_write_cmos_sensor(0x0113,0x0A);//   
	IMX135MIPI_write_cmos_sensor(0x0381,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x0383,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x0385,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x0387,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x0390,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x0391,0x11);//   
	IMX135MIPI_write_cmos_sensor(0x0392,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x0401,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x0404,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x0405,0x10);//   
	IMX135MIPI_write_cmos_sensor(0x4082,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x4083,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x7006,0x04);//

	//Optionnal function setting
	IMX135MIPI_write_cmos_sensor(0x0700,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x3A63,0x00);//
	IMX135MIPI_write_cmos_sensor(0x4100,0xF8);//   
	IMX135MIPI_write_cmos_sensor(0x4203,0xFF);//   
	IMX135MIPI_write_cmos_sensor(0x4344,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x441C,0x01);//

	//Size setting
	IMX135MIPI_write_cmos_sensor(0x0340,(IMX135MIPI_FULL_FRAME_LENGTH_LINES>>8)&0xFF);//   
	IMX135MIPI_write_cmos_sensor(0x0341,(IMX135MIPI_FULL_FRAME_LENGTH_LINES>>0)&0xFF);//   
	IMX135MIPI_write_cmos_sensor(0x0342,(IMX135MIPI_FULL_LINE_LENGTH_PIXELS>>8)&0xFF);//
	IMX135MIPI_write_cmos_sensor(0x0343,(IMX135MIPI_FULL_LINE_LENGTH_PIXELS>>0)&0xFF);//
	IMX135MIPI_write_cmos_sensor(0x0344,0x00);//
	IMX135MIPI_write_cmos_sensor(0x0345,0x00);//
	IMX135MIPI_write_cmos_sensor(0x0346,0x00);//
	IMX135MIPI_write_cmos_sensor(0x0347,0x00);//
	IMX135MIPI_write_cmos_sensor(0x0348,0x10);//
	IMX135MIPI_write_cmos_sensor(0x0349,0x6F);//
	IMX135MIPI_write_cmos_sensor(0x034A,0x0C);//
	IMX135MIPI_write_cmos_sensor(0x034B,0x2F);//
	IMX135MIPI_write_cmos_sensor(0x034C,0x10);//
	IMX135MIPI_write_cmos_sensor(0x034D,0x70);//
	IMX135MIPI_write_cmos_sensor(0x034E,0x0C);//
	IMX135MIPI_write_cmos_sensor(0x034F,0x30);//
	IMX135MIPI_write_cmos_sensor(0x0350,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x0351,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x0352,0x00);// 
	IMX135MIPI_write_cmos_sensor(0x0353,0x00);//
	IMX135MIPI_write_cmos_sensor(0x0354,0x10);//
	IMX135MIPI_write_cmos_sensor(0x0355,0x70);//
	IMX135MIPI_write_cmos_sensor(0x0356,0x0C);//
	IMX135MIPI_write_cmos_sensor(0x0357,0x30);//	
	IMX135MIPI_write_cmos_sensor(0x301D,0x30);//   
	IMX135MIPI_write_cmos_sensor(0x3310,0x10);//   
	IMX135MIPI_write_cmos_sensor(0x3311,0x70);//   
	IMX135MIPI_write_cmos_sensor(0x3312,0x0C);//   
	IMX135MIPI_write_cmos_sensor(0x3313,0x30);//   
	IMX135MIPI_write_cmos_sensor(0x331C,0x09);//   
	IMX135MIPI_write_cmos_sensor(0x331D,0x77);//   
	IMX135MIPI_write_cmos_sensor(0x4084,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x4085,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x4086,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x4087,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x4400,0x00);//

	//global timing setting
	IMX135MIPI_write_cmos_sensor(0x0830,0x77);//   
	IMX135MIPI_write_cmos_sensor(0x0831,0x2F);//  
	IMX135MIPI_write_cmos_sensor(0x0832,0x4F);//   
	IMX135MIPI_write_cmos_sensor(0x0833,0x2F);// 
	IMX135MIPI_write_cmos_sensor(0x0834,0x2F);//   
	IMX135MIPI_write_cmos_sensor(0x0835,0x37);// 
	IMX135MIPI_write_cmos_sensor(0x0836,0xA7);//   
	IMX135MIPI_write_cmos_sensor(0x0837,0x37);// 
	IMX135MIPI_write_cmos_sensor(0x0839,0x1F);//
	IMX135MIPI_write_cmos_sensor(0x083A,0x17);//   
	IMX135MIPI_write_cmos_sensor(0x083B,0x02);// 

	// integration time setting
	IMX135MIPI_write_cmos_sensor(0x0202,0x0C);//
	IMX135MIPI_write_cmos_sensor(0x0203,0x46);//

	//gain setting
	IMX135MIPI_write_cmos_sensor(0x0205,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x020E,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x020F,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x0210,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x0211,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x0212,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x0213,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x0214,0x01);//   
	IMX135MIPI_write_cmos_sensor(0x0215,0x00);//
#if 0
	//hdr setting
	IMX135MIPI_write_cmos_sensor(0x0230,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x0231,0x00);//     
	IMX135MIPI_write_cmos_sensor(0x0233,0x00);//
	IMX135MIPI_write_cmos_sensor(0x0234,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x0235,0x40);//
	IMX135MIPI_write_cmos_sensor(0x0236,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x0238,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x0239,0x04);//
	IMX135MIPI_write_cmos_sensor(0x023B,0x00);//   
	IMX135MIPI_write_cmos_sensor(0x023C,0x00);//
	IMX135MIPI_write_cmos_sensor(0x33B0,0x04);//   
	IMX135MIPI_write_cmos_sensor(0x33B1,0x00);//
	IMX135MIPI_write_cmos_sensor(0x33B3,0X00);//   
	IMX135MIPI_write_cmos_sensor(0x33B4,0X01);//
	IMX135MIPI_write_cmos_sensor(0x3800,0X00);//

#endif	
#endif//IMX135_CAPTURE_24FPS
	IMX135MIPI_write_cmos_sensor(0x0100,0x01);//STREAM START
   SENSORDB("IMX135MIPI_set_13M Capture setting exit\n");
}



/*****************************************************************************/
/* Windows Mobile Sensor Interface */
/*****************************************************************************/
/*************************************************************************
* FUNCTION
*   IMX135MIPIOpen
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

UINT32 IMX135MIPIOpen(void)
{
    int  retry = 0; 
	kal_uint16 sensorid; 
    // check if sensor ID correct
    retry = 3; 
	SENSORDB("IMX135MIPIOpen enter:\n"); 
    do {
       SENSORDB("Read ID in the Open function\n"); 
	   sensorid=(kal_uint16)((IMX135MIPI_read_cmos_sensor(0x0016)<<8) | IMX135MIPI_read_cmos_sensor(0x0017));  
//Debug
//sensorid = IMX135MIPI_SENSOR_ID;

	   spin_lock(&imx135_drv_lock);    
	   IMX135MIPI_sensor_id =sensorid;  
	   spin_unlock(&imx135_drv_lock);
		if (IMX135MIPI_sensor_id == IMX135MIPI_SENSOR_ID)
		break; 
		SENSORDB("Read Sensor ID Fail = 0x%04x\n", IMX135MIPI_sensor_id); 
		retry--; 
	    }
	while (retry > 0);
    SENSORDB("Read Sensor ID = 0x%04x\n", IMX135MIPI_sensor_id); 
    if (IMX135MIPI_sensor_id != IMX135MIPI_SENSOR_ID)
        return ERROR_SENSOR_CONNECT_FAIL;
    IMX135MIPI_Sensor_Init();
	spin_lock(&imx135_drv_lock);    
	   IMX135MIPI_sensor.capture_mode=KAL_FALSE;
	   spin_unlock(&imx135_drv_lock);
	SENSORDB("IMX135MIPIOpen exit:\n"); 
    return ERROR_NONE;
}

/*************************************************************************
* FUNCTION
*   IMX135MIPIGetSensorID
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
UINT32 IMX135MIPIGetSensorID(UINT32 *sensorID) 
{
    int  retry = 3; 
    // check if sensor ID correct
    SENSORDB("IMX135MIPIGetSensorID\n"); 
    do {		
	  *sensorID =(kal_uint16)((IMX135MIPI_read_cmos_sensor(0x0016)<<8) | IMX135MIPI_read_cmos_sensor(0x0017));   
	  SENSORDB("GetSensorID = 0x%04x\n", *sensorID); 
        if (*sensorID == IMX135MIPI_SENSOR_ID)
            break;
        SENSORDB("Read Sensor ID Fail = 0x%04x\n", *sensorID); 
        retry--; 
		
    } while (retry > 0);

//Debug
//*sensorID = IMX135MIPI_SENSOR_ID;

    if (*sensorID != IMX135MIPI_SENSOR_ID) {
        *sensorID = 0xFFFFFFFF; 
        return ERROR_SENSOR_CONNECT_FAIL;
    }
    return ERROR_NONE;
}


/*************************************************************************
* FUNCTION
*   IMX135MIPI_SetShutter
*
* DESCRIPTION
*   This function set e-shutter of IMX135MIPI to change exposure time.
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
void IMX135MIPI_SetShutter(kal_uint16 iShutter)
{
//	 SENSORDB("IMX135MIPI_SetShutter:shutter=%d\n",iShutter);
   
    if (iShutter < 1)
        iShutter = 1; 
	else if(iShutter > 0xffff)
		iShutter = 0xffff;
    IMX135MIPI_write_shutter(iShutter);
}   /*  IMX135MIPI_SetShutter   */



/*************************************************************************
* FUNCTION
*   IMX135MIPI_read_shutter
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
UINT16 IMX135MIPI_read_shutter(void)
{
    return (UINT16)( (IMX135MIPI_read_cmos_sensor(0x0202)<<8) | IMX135MIPI_read_cmos_sensor(0x0203) );
}

/*************************************************************************
* FUNCTION
*   IMX135MIPI_night_mode
*
* DESCRIPTION
*   This function night mode of IMX135MIPI.
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

///is not use in raw sensor
void IMX135MIPI_NightMode(kal_bool bEnable)
{
#if 0
    /************************************************************************/
    /*                      Auto Mode: 30fps                                                                                          */
    /*                      Night Mode:15fps                                                                                          */
    /************************************************************************/
    if(bEnable)
    {
        if(OV5642_MPEG4_encode_mode==KAL_TRUE)
        {
            OV5642_MAX_EXPOSURE_LINES = (kal_uint16)((OV5642_sensor_pclk/15)/(OV5642_PV_PERIOD_PIXEL_NUMS+OV5642_PV_dummy_pixels));
            OV5642_write_cmos_sensor(0x350C, (OV5642_MAX_EXPOSURE_LINES >> 8) & 0xFF);
            OV5642_write_cmos_sensor(0x350D, OV5642_MAX_EXPOSURE_LINES & 0xFF);
            OV5642_CURRENT_FRAME_LINES = OV5642_MAX_EXPOSURE_LINES;
            OV5642_MAX_EXPOSURE_LINES = OV5642_CURRENT_FRAME_LINES - OV5642_SHUTTER_LINES_GAP;
        }
    }
    else// Fix video framerate 30 fps
    {
        if(OV5642_MPEG4_encode_mode==KAL_TRUE)
        {
            OV5642_MAX_EXPOSURE_LINES = (kal_uint16)((OV5642_sensor_pclk/30)/(OV5642_PV_PERIOD_PIXEL_NUMS+OV5642_PV_dummy_pixels));
            if(OV5642_pv_exposure_lines < (OV5642_MAX_EXPOSURE_LINES - OV5642_SHUTTER_LINES_GAP)) // for avoid the shutter > frame_lines,move the frame lines setting to shutter function
            {
                OV5642_write_cmos_sensor(0x350C, (OV5642_MAX_EXPOSURE_LINES >> 8) & 0xFF);
                OV5642_write_cmos_sensor(0x350D, OV5642_MAX_EXPOSURE_LINES & 0xFF);
                OV5642_CURRENT_FRAME_LINES = OV5642_MAX_EXPOSURE_LINES;
            }
            OV5642_MAX_EXPOSURE_LINES = OV5642_MAX_EXPOSURE_LINES - OV5642_SHUTTER_LINES_GAP;
        }
    }
#endif	
}/*	IMX135MIPI_NightMode */



/*************************************************************************
* FUNCTION
*   IMX135MIPIClose
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
UINT32 IMX135MIPIClose(void)
{
    return ERROR_NONE;
}	/* IMX135MIPIClose() */

void IMX135MIPISetFlipMirror(kal_int32 imgMirror)
{
    kal_uint8  iTemp; 
	
	iTemp = IMX135MIPI_read_cmos_sensor(0x0101);
	iTemp&= ~0x03; //Clear the mirror and flip bits.
	switch (imgMirror)
	{
		case IMAGE_NORMAL:
			IMX135MIPI_write_cmos_sensor(0x0101, iTemp);	//Set normal
			break;
		case IMAGE_V_MIRROR:
			IMX135MIPI_write_cmos_sensor(0x0101, iTemp | 0x02); //Set flip
			break;
		case IMAGE_H_MIRROR:
			IMX135MIPI_write_cmos_sensor(0x0101, iTemp | 0x01); //Set mirror
			break;
		case IMAGE_HV_MIRROR:
			IMX135MIPI_write_cmos_sensor(0x0101, iTemp | 0x03); //Set mirror and flip
			break;
	}
}


/*************************************************************************
* FUNCTION
*   IMX135MIPIPreview
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
UINT32 IMX135MIPIPreview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                                                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    kal_uint16 iStartX = 0, iStartY = 0;
    if(sensor_config_data->SensorOperationMode==MSDK_SENSOR_OPERATION_MODE_VIDEO)		// MPEG4 Encode Mode
    {   
        SENSORDB("IMX135MIPIVideo enter:\n");
    	spin_lock(&imx135_drv_lock);    
        IMX135MIPI_MPEG4_encode_mode = KAL_TRUE;  
		IMX135MIPI_sensor.video_mode=KAL_TRUE;
		IMX135MIPI_sensor.pv_mode=KAL_FALSE;
		IMX135MIPI_sensor.capture_mode=KAL_FALSE;
		spin_unlock(&imx135_drv_lock);
		VideoFullSizeSetting();
		
		iStartX += IMX135MIPI_IMAGE_SENSOR_VIDEO_STARTX;
		iStartY += IMX135MIPI_IMAGE_SENSOR_VIDEO_STARTY;
		spin_lock(&imx135_drv_lock);	
		IMX135MIPI_sensor.cp_dummy_pixels = 0;
		IMX135MIPI_sensor.cp_dummy_lines = 0;
		IMX135MIPI_sensor.pv_dummy_pixels = 0;
		IMX135MIPI_sensor.pv_dummy_lines = 0;
		IMX135MIPI_sensor.video_dummy_pixels = 0;
		IMX135MIPI_sensor.video_dummy_lines = 0;
		IMX135MIPI_sensor.video_line_length = IMX135MIPI_VIDEO_LINE_LENGTH_PIXELS+IMX135MIPI_sensor.video_dummy_pixels; 
		IMX135MIPI_sensor.video_frame_length = IMX135MIPI_VIDEO_FRAME_LENGTH_LINES+IMX135MIPI_sensor.video_dummy_lines;
		spin_unlock(&imx135_drv_lock);
		
		IMX135MIPI_SetDummy(IMX135MIPI_sensor.video_dummy_pixels,IMX135MIPI_sensor.video_dummy_lines);
		IMX135MIPI_SetShutter(IMX135MIPI_sensor.video_shutter);
		memcpy(&IMX135MIPISensorConfigData, sensor_config_data, sizeof(MSDK_SENSOR_CONFIG_STRUCT));
		image_window->GrabStartX= iStartX;
		image_window->GrabStartY= iStartY;
		SENSORDB("IMX135MIPIVideo exit:\n");
	//	image_window->ExposureWindowWidth= IMX135MIPI_IMAGE_SENSOR_PV_WIDTH - 2*iStartX;
	//	image_window->ExposureWindowHeight= IMX135MIPI_IMAGE_SENSOR_PV_HEIGHT - 2*iStartY;

    }
    else
    {
		SENSORDB("IMX135MIPIPreview enter:\n");
    	spin_lock(&imx135_drv_lock);    
        IMX135MIPI_MPEG4_encode_mode = KAL_FALSE;
		IMX135MIPI_sensor.video_mode=KAL_FALSE;
		IMX135MIPI_sensor.pv_mode=KAL_TRUE;
		IMX135MIPI_sensor.capture_mode=KAL_FALSE;
		spin_unlock(&imx135_drv_lock);
        PreviewSetting();
		//IMX135MIPISetFlipMirror(IMAGE_HV_MIRROR);
		iStartX += IMX135MIPI_IMAGE_SENSOR_PV_STARTX;
		iStartY += IMX135MIPI_IMAGE_SENSOR_PV_STARTY;
		spin_lock(&imx135_drv_lock);	
		IMX135MIPI_sensor.cp_dummy_pixels = 0;
		IMX135MIPI_sensor.cp_dummy_lines = 0;
		IMX135MIPI_sensor.pv_dummy_pixels = 0;
		IMX135MIPI_sensor.pv_dummy_lines = 0;
		IMX135MIPI_sensor.video_dummy_pixels = 0;
		IMX135MIPI_sensor.video_dummy_lines = 0;
		IMX135MIPI_sensor.pv_line_length = IMX135MIPI_PV_LINE_LENGTH_PIXELS+IMX135MIPI_sensor.pv_dummy_pixels; 
		IMX135MIPI_sensor.pv_frame_length = IMX135MIPI_PV_FRAME_LENGTH_LINES+IMX135MIPI_sensor.pv_dummy_lines;
		spin_unlock(&imx135_drv_lock);
		
		IMX135MIPI_SetDummy(IMX135MIPI_sensor.pv_dummy_pixels,IMX135MIPI_sensor.pv_dummy_lines);
		IMX135MIPI_SetShutter(IMX135MIPI_sensor.pv_shutter);
		memcpy(&IMX135MIPISensorConfigData, sensor_config_data, sizeof(MSDK_SENSOR_CONFIG_STRUCT));
		image_window->GrabStartX= iStartX;
		image_window->GrabStartY= iStartY;
		image_window->ExposureWindowWidth= IMX135MIPI_IMAGE_SENSOR_PV_WIDTH - 2*iStartX;
		image_window->ExposureWindowHeight= IMX135MIPI_IMAGE_SENSOR_PV_HEIGHT - 2*iStartY;
		SENSORDB("IMX135MIPIPreview exit:\n");

    }
	

	return ERROR_NONE;
}	/* IMX135MIPIPreview() */

UINT32 IMX135MIPICapture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                                                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
   
    //kal_uint16 iStartX = 0, iStartY = 0;
   if(IMX135MIPI_sensor.capture_mode==KAL_TRUE)
   	return ERROR_NONE;	
	SENSORDB("IMX135MIPICapture enter:\n"); 
	spin_lock(&imx135_drv_lock);	
	IMX135MIPI_sensor.video_mode=KAL_FALSE;
	IMX135MIPI_sensor.pv_mode=KAL_FALSE;
	IMX135MIPI_sensor.capture_mode=KAL_TRUE;
    IMX135MIPI_MPEG4_encode_mode = KAL_FALSE; 
    IMX135MIPI_Auto_Flicker_mode = KAL_FALSE;   
	spin_unlock(&imx135_drv_lock);
	spin_lock(&imx135_drv_lock);    
    IMX135MIPI_sensor.cp_dummy_pixels= 0;
    IMX135MIPI_sensor.cp_dummy_lines = 0;   
	spin_unlock(&imx135_drv_lock);
    IMX135MIPI_set_13M();
     
   // IMX135MIPISetFlipMirror(sensor_config_data->SensorImageMirror); 
   //IMX135MIPISetFlipMirror(IMAGE_HV_MIRROR);
	
	spin_lock(&imx135_drv_lock);    
    IMX135MIPI_sensor.cp_line_length=IMX135MIPI_FULL_LINE_LENGTH_PIXELS+IMX135MIPI_sensor.cp_dummy_pixels;
    IMX135MIPI_sensor.cp_frame_length=IMX135MIPI_FULL_FRAME_LENGTH_LINES+IMX135MIPI_sensor.cp_dummy_lines;
	spin_unlock(&imx135_drv_lock);
	//iStartX = IMX135MIPI_IMAGE_SENSOR_CAP_STARTX;
	//iStartY = IMX135MIPI_IMAGE_SENSOR_CAP_STARTY;
	//image_window->ExposureWindowWidth=IMX135MIPI_IMAGE_SENSOR_FULL_WIDTH -2*iStartX;
	//image_window->ExposureWindowHeight=IMX135MIPI_IMAGE_SENSOR_FULL_HEIGHT-2*iStartY;  
	spin_lock(&imx135_drv_lock);	
    memcpy(&IMX135MIPISensorConfigData, sensor_config_data, sizeof(MSDK_SENSOR_CONFIG_STRUCT));
	spin_unlock(&imx135_drv_lock);
	SENSORDB("IMX135MIPICapture exit:\n"); 


    return ERROR_NONE;
}	/* IMX135MIPICapture() */

UINT32 IMX135MIPIGetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution)
{

    pSensorResolution->SensorPreviewWidth	= IMX135MIPI_REAL_PV_WIDTH;
    pSensorResolution->SensorPreviewHeight	= IMX135MIPI_REAL_PV_HEIGHT;
    pSensorResolution->SensorFullWidth		= IMX135MIPI_REAL_CAP_WIDTH;
    pSensorResolution->SensorFullHeight		= IMX135MIPI_REAL_CAP_HEIGHT;
    pSensorResolution->SensorVideoWidth		= IMX135MIPI_REAL_VIDEO_WIDTH;
    pSensorResolution->SensorVideoHeight    = IMX135MIPI_REAL_VIDEO_HEIGHT;
    SENSORDB("IMX135MIPIGetResolution \n");    

    return ERROR_NONE;
}   /* IMX135MIPIGetResolution() */

UINT32 IMX135MIPIGetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId,
                                                MSDK_SENSOR_INFO_STRUCT *pSensorInfo,
                                                MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
	switch(ScenarioId){
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
				pSensorInfo->SensorPreviewResolutionX=IMX135MIPI_REAL_CAP_WIDTH;
				pSensorInfo->SensorPreviewResolutionY=IMX135MIPI_REAL_CAP_HEIGHT;
				pSensorInfo->SensorCameraPreviewFrameRate=30;
			break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				pSensorInfo->SensorPreviewResolutionX=IMX135MIPI_REAL_VIDEO_WIDTH;
				pSensorInfo->SensorPreviewResolutionY=IMX135MIPI_REAL_VIDEO_HEIGHT;
				pSensorInfo->SensorCameraPreviewFrameRate=30;
			break;
		default:
        pSensorInfo->SensorPreviewResolutionX=IMX135MIPI_REAL_PV_WIDTH;
        pSensorInfo->SensorPreviewResolutionY=IMX135MIPI_REAL_PV_HEIGHT;
				pSensorInfo->SensorCameraPreviewFrameRate=30;
			break;
	}

    pSensorInfo->SensorVideoFrameRate=30;
    pSensorInfo->SensorStillCaptureFrameRate=15;
    pSensorInfo->SensorWebCamCaptureFrameRate=15;
    pSensorInfo->SensorResetActiveHigh=FALSE;
    pSensorInfo->SensorResetDelayCount=5;
    pSensorInfo->SensorOutputDataFormat=SENSOR_OUTPUT_FORMAT_RAW_B;
    pSensorInfo->SensorClockPolarity=SENSOR_CLOCK_POLARITY_LOW; /*??? */
    pSensorInfo->SensorClockFallingPolarity=SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorInterruptDelayLines = 1;
    pSensorInfo->SensroInterfaceType=SENSOR_INTERFACE_TYPE_MIPI;
    pSensorInfo->CaptureDelayFrame = 2; 
    pSensorInfo->PreviewDelayFrame = 2; 
    pSensorInfo->VideoDelayFrame = 5; 
    pSensorInfo->SensorMasterClockSwitch = 0; 
    pSensorInfo->SensorDrivingCurrent = ISP_DRIVING_8MA;      
    pSensorInfo->AEShutDelayFrame = 0;		    /* The frame of setting shutter default 0 for TG int */
    pSensorInfo->AESensorGainDelayFrame = 0;     /* The frame of setting sensor gain */
    pSensorInfo->AEISPGainDelayFrame = 2;	
	   
    switch (ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
     //   case MSDK_SCENARIO_ID_VIDEO_CAPTURE_MPEG4:
            pSensorInfo->SensorClockFreq=24;
            pSensorInfo->SensorClockDividCount=	5;
            pSensorInfo->SensorClockRisingCount= 0;
            pSensorInfo->SensorClockFallingCount= 2;
            pSensorInfo->SensorPixelClockCount= 3;
            pSensorInfo->SensorDataLatchCount= 2;
            pSensorInfo->SensorGrabStartX = IMX135MIPI_IMAGE_SENSOR_PV_STARTX; 
            pSensorInfo->SensorGrabStartY = IMX135MIPI_IMAGE_SENSOR_PV_STARTY;           		
            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;			
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0; 
	     pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 14; 
	     pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorWidthSampling = 0;  // 0 is default 1x
            pSensorInfo->SensorHightSampling = 0;   // 0 is default 1x 
            pSensorInfo->SensorPacketECCOrder = 1;
            break;
		
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			   pSensorInfo->SensorClockFreq=24;
			   pSensorInfo->SensorClockDividCount= 5;
			   pSensorInfo->SensorClockRisingCount= 0;
			   pSensorInfo->SensorClockFallingCount= 2;
			   pSensorInfo->SensorPixelClockCount= 3;
			   pSensorInfo->SensorDataLatchCount= 2;
			   pSensorInfo->SensorGrabStartX = IMX135MIPI_IMAGE_SENSOR_VIDEO_STARTX; 
			   pSensorInfo->SensorGrabStartY = IMX135MIPI_IMAGE_SENSOR_VIDEO_STARTY;				   
			   pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;		   
			   pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0; 
			pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 14; 
			pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
			   pSensorInfo->SensorWidthSampling = 0;  // 0 is default 1x
			   pSensorInfo->SensorHightSampling = 0;   // 0 is default 1x 
			   pSensorInfo->SensorPacketECCOrder = 1;

			break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
       // case MSDK_SCENARIO_ID_CAMERA_CAPTURE_MEM:
				case MSDK_SCENARIO_ID_CAMERA_ZSD:
            pSensorInfo->SensorClockFreq=24;
            pSensorInfo->SensorClockDividCount=	5;
            pSensorInfo->SensorClockRisingCount= 0;
            pSensorInfo->SensorClockFallingCount= 2;
            pSensorInfo->SensorPixelClockCount= 3;
            pSensorInfo->SensorDataLatchCount= 2;
            pSensorInfo->SensorGrabStartX = IMX135MIPI_IMAGE_SENSOR_CAP_STARTX;	//2*IMX135MIPI_IMAGE_SENSOR_PV_STARTX; 
            pSensorInfo->SensorGrabStartY = IMX135MIPI_IMAGE_SENSOR_CAP_STARTY;	//2*IMX135MIPI_IMAGE_SENSOR_PV_STARTY;          			
            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;			
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0; 
            pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 14; 
            pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0; 
            pSensorInfo->SensorWidthSampling = 0;  // 0 is default 1x
            pSensorInfo->SensorHightSampling = 0;   // 0 is default 1x
            pSensorInfo->SensorPacketECCOrder = 1;
            break;
        default:
            pSensorInfo->SensorClockFreq=24;
            pSensorInfo->SensorClockDividCount=	5;
            pSensorInfo->SensorClockRisingCount= 0;
            pSensorInfo->SensorClockFallingCount= 2;
            pSensorInfo->SensorPixelClockCount= 3;
            pSensorInfo->SensorDataLatchCount= 2;
            pSensorInfo->SensorGrabStartX = 1; 
            pSensorInfo->SensorGrabStartY = 1;             
            break;
    }
	spin_lock(&imx135_drv_lock);	

    IMX135MIPIPixelClockDivider=pSensorInfo->SensorPixelClockCount;
	
	spin_unlock(&imx135_drv_lock);
    memcpy(pSensorConfigData, &IMX135MIPISensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));

    return ERROR_NONE;
}   /* IMX135MIPIGetInfo() */


UINT32 IMX135MIPIControl(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow,
                                                MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
		spin_lock(&imx135_drv_lock);	
		CurrentScenarioId = ScenarioId;
		spin_unlock(&imx135_drv_lock);
    switch (ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            IMX135MIPIPreview(pImageWindow, pSensorConfigData);
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	case MSDK_SCENARIO_ID_CAMERA_ZSD:
            IMX135MIPICapture(pImageWindow, pSensorConfigData);
            break;
        default:
            return ERROR_INVALID_SCENARIO_ID;
    }
    return ERROR_NONE;
} /* IMX135MIPIControl() */

UINT32 IMX135MIPISetVideoMode(UINT16 u2FrameRate)
{
      kal_uint16 IMX135MIPI_Video_Max_Expourse_Time = 0;
	  SENSORDB("IMX135MIPISetVideoMode  u2FrameRate = %d\n", u2FrameRate);
      if(u2FrameRate==0)
	{
		SENSORDB("Disable Video Mode or dynimac fps\n");
		return ERROR_NONE;
	}
		spin_lock(&imx135_drv_lock);
		IMX135MIPI_sensor.fix_video_fps = KAL_TRUE;
		spin_unlock(&imx135_drv_lock);
		u2FrameRate=u2FrameRate*10;//10*FPS
		SENSORDB("[IMX135MIPI][Enter Fix_fps func] IMX135MIPI_Fix_Video_Frame_Rate = %d\n", u2FrameRate/10);
	
		IMX135MIPI_Video_Max_Expourse_Time = (kal_uint16)((IMX135MIPI_sensor.video_pclk*10/u2FrameRate)/IMX135MIPI_sensor.video_line_length);
		
		if (IMX135MIPI_Video_Max_Expourse_Time > IMX135MIPI_VIDEO_FRAME_LENGTH_LINES/*IMX135MIPI_sensor.pv_frame_length*/) 
			{
				spin_lock(&imx135_drv_lock);    
				IMX135MIPI_sensor.video_frame_length = IMX135MIPI_Video_Max_Expourse_Time;
				IMX135MIPI_sensor.video_dummy_lines = IMX135MIPI_sensor.video_frame_length-IMX135MIPI_VIDEO_FRAME_LENGTH_LINES;
				spin_unlock(&imx135_drv_lock);
				SENSORDB("[IMX135MIPI]%s():frame_length=%d,dummy_lines=%d\n",__FUNCTION__,IMX135MIPI_sensor.video_frame_length,IMX135MIPI_sensor.video_dummy_lines);
				IMX135MIPI_SetDummy(IMX135MIPI_sensor.video_dummy_pixels,IMX135MIPI_sensor.video_dummy_lines);
			}
	spin_lock(&imx135_drv_lock);    
    IMX135MIPI_MPEG4_encode_mode = KAL_TRUE; 
	spin_unlock(&imx135_drv_lock);
	
    return ERROR_NONE;
}

UINT32 IMX135MIPISetAutoFlickerMode(kal_bool bEnable, UINT16 u2FrameRate)
{
#if 0
	kal_uint32 pv_max_frame_rate_lines=0;
if(IMX135MIPI_sensor.pv_mode==TRUE)
	pv_max_frame_rate_lines=IMX135MIPI_PV_FRAME_LENGTH_LINES;
else
    pv_max_frame_rate_lines=IMX135MIPI_VIDEO_FRAME_LENGTH_LINES	;

    SENSORDB("[IMX135MIPISetAutoFlickerMode] frame rate(10base) = %d %d\n", bEnable, u2FrameRate);
    if(bEnable) {   // enable auto flicker   
    	spin_lock(&imx135_drv_lock);    
        IMX135MIPI_Auto_Flicker_mode = KAL_TRUE; 
		spin_unlock(&imx135_drv_lock);
        if(IMX135MIPI_MPEG4_encode_mode == KAL_TRUE) {    // in the video mode, reset the frame rate
            pv_max_frame_rate_lines = IMX135MIPI_MAX_EXPOSURE_LINES + (IMX135MIPI_MAX_EXPOSURE_LINES>>7);            
            IMX135MIPI_write_cmos_sensor(0x0104, 1);        
            IMX135MIPI_write_cmos_sensor(0x0340, (pv_max_frame_rate_lines >>8) & 0xFF);
            IMX135MIPI_write_cmos_sensor(0x0341, pv_max_frame_rate_lines & 0xFF);	
            IMX135MIPI_write_cmos_sensor(0x0104, 0);        	
        }
    } else {
    	spin_lock(&imx135_drv_lock);    
        IMX135MIPI_Auto_Flicker_mode = KAL_FALSE; 
		spin_unlock(&imx135_drv_lock);
        if(IMX135MIPI_MPEG4_encode_mode == KAL_TRUE) {    // in the video mode, restore the frame rate
            IMX135MIPI_write_cmos_sensor(0x0104, 1);        
            IMX135MIPI_write_cmos_sensor(0x0340, (IMX135MIPI_MAX_EXPOSURE_LINES >>8) & 0xFF);
            IMX135MIPI_write_cmos_sensor(0x0341, IMX135MIPI_MAX_EXPOSURE_LINES & 0xFF);	
            IMX135MIPI_write_cmos_sensor(0x0104, 0);        	
        }
        SENSORDB("Disable Auto flicker\n");    
    }
	#endif
    return ERROR_NONE;
}




UINT32 IMX135MIPISetMaxFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 frameRate) {
	kal_uint32 pclk;
	kal_int16 dummyLine;
	kal_uint16 lineLength,frameHeight;
		
	SENSORDB("IMX135MIPISetMaxFramerateByScenario: scenarioId = %d, frame rate = %d\n",scenarioId,frameRate);
	switch (scenarioId) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			pclk =IMX135MIPI_sensor.pv_pclk;
			lineLength = IMX135MIPI_PV_LINE_LENGTH_PIXELS;
			frameHeight = (10 * pclk)/frameRate/lineLength;
			dummyLine = frameHeight - IMX135MIPI_PV_FRAME_LENGTH_LINES;
			if(dummyLine<0)
				dummyLine = 0;
			IMX135MIPI_SetDummy(0, dummyLine);			
			break;			
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			pclk = IMX135MIPI_sensor.video_pclk;
			lineLength = IMX135MIPI_VIDEO_LINE_LENGTH_PIXELS;
			frameHeight = (10 * pclk)/frameRate/lineLength;
			dummyLine = frameHeight - IMX135MIPI_VIDEO_FRAME_LENGTH_LINES;
			if(dummyLine<0)
				dummyLine = 0;
			IMX135MIPI_SetDummy(0, dummyLine);			
			break;			
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:			
			pclk = IMX135MIPI_sensor.cp_pclk;
			lineLength = IMX135MIPI_FULL_LINE_LENGTH_PIXELS;
			frameHeight = (10 * pclk)/frameRate/lineLength;
			dummyLine = frameHeight - IMX135MIPI_FULL_FRAME_LENGTH_LINES;
			if(dummyLine<0)
				dummyLine = 0;
			IMX135MIPI_SetDummy(0, dummyLine);			
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



UINT32 IMX135MIPIGetDefaultFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 *pframeRate) 
{

	switch (scenarioId) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			 *pframeRate = 300;
			 break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
#if defined(IMX135_CAPTURE_24FPS)
			 *pframeRate = 240;
#else
			 *pframeRate = 150;
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




UINT32 IMX135MIPISetTestPatternMode(kal_bool bEnable)  //daikan
{
    SENSORDB("IMX135MIPISetTestPatternMode Test pattern enable:%d\n", bEnable);
    
    if(bEnable) {   // enable color bar   
        IMX135MIPI_write_cmos_sensor(0x30D8, 0x10);  // color bar test pattern
        IMX135MIPI_write_cmos_sensor(0x0600, 0x00);  // color bar test pattern
        IMX135MIPI_write_cmos_sensor(0x0601, 0x02);  // color bar test pattern 
    } else {
        IMX135MIPI_write_cmos_sensor(0x30D8, 0x00);  // disable color bar test pattern
    }
    return ERROR_NONE;
}

UINT32 IMX135MIPIFeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId,
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
            *pFeatureReturnPara16++=IMAGE_SENSOR_FULL_WIDTH;
            *pFeatureReturnPara16=IMAGE_SENSOR_FULL_HEIGHT;
            *pFeatureParaLen=4;
            break;
        case SENSOR_FEATURE_GET_PERIOD:
        		switch(CurrentScenarioId)
        		{
        			case MSDK_SCENARIO_ID_CAMERA_ZSD:
        		    case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
 		            *pFeatureReturnPara16++=IMX135MIPI_sensor.cp_line_length;  
 		            *pFeatureReturnPara16=IMX135MIPI_sensor.cp_frame_length;
		            SENSORDB("Sensor period:%d %d\n",IMX135MIPI_sensor.cp_line_length, IMX135MIPI_sensor.cp_frame_length); 
		            *pFeatureParaLen=4;        				
        				break;
        			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
					*pFeatureReturnPara16++=IMX135MIPI_sensor.video_line_length;  
					*pFeatureReturnPara16=IMX135MIPI_sensor.video_frame_length;
					 SENSORDB("Sensor period:%d %d\n", IMX135MIPI_sensor.video_line_length, IMX135MIPI_sensor.video_frame_length); 
					 *pFeatureParaLen=4;
						break;
        			default:	
					*pFeatureReturnPara16++=IMX135MIPI_sensor.pv_line_length;  
					*pFeatureReturnPara16=IMX135MIPI_sensor.pv_frame_length;
		            SENSORDB("Sensor period:%d %d\n", IMX135MIPI_sensor.pv_line_length, IMX135MIPI_sensor.pv_frame_length); 
		            *pFeatureParaLen=4;
	            break;
          	}
          	break;
        case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
        		switch(CurrentScenarioId)
        		{
        			case MSDK_SCENARIO_ID_CAMERA_ZSD:
        			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		            *pFeatureReturnPara32 = IMX135MIPI_sensor.cp_pclk; 
		            *pFeatureParaLen=4;		         	
		         		break;
					case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
						*pFeatureReturnPara32 = IMX135MIPI_sensor.video_pclk;
						*pFeatureParaLen=4;
						break;
		         		default:
		            *pFeatureReturnPara32 = IMX135MIPI_sensor.pv_pclk;
		            *pFeatureParaLen=4;
		            break;
		         }
		         break;
        case SENSOR_FEATURE_SET_ESHUTTER:
            IMX135MIPI_SetShutter(*pFeatureData16);
//			SENSORDB("shutter&gain test by hhl:IMX135MIPI_SetShutter in feature ctrl\n"); 
            break;
		case SENSOR_FEATURE_SET_SENSOR_SYNC:
//			SENSORDB("hhl'test the function of the sync cased\n"); 
			break;
        case SENSOR_FEATURE_SET_NIGHTMODE:
            IMX135MIPI_NightMode((BOOL) *pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_GAIN:
           IMX135MIPI_SetGain((UINT16) *pFeatureData16);
            
//			SENSORDB("shutter&gain test by hhl:IMX135MIPI_SetGain in feature ctrl\n"); 
            break;
        case SENSOR_FEATURE_SET_FLASHLIGHT:
            break;
        case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
			spin_lock(&imx135_drv_lock);    
            IMX135MIPI_isp_master_clock=*pFeatureData32;
			spin_unlock(&imx135_drv_lock);
            break;
        case SENSOR_FEATURE_SET_REGISTER:
			IMX135MIPI_write_cmos_sensor(pSensorRegData->RegAddr, pSensorRegData->RegData);
            break;
        case SENSOR_FEATURE_GET_REGISTER:
            pSensorRegData->RegData = IMX135MIPI_read_cmos_sensor(pSensorRegData->RegAddr);
            break;
        case SENSOR_FEATURE_SET_CCT_REGISTER:
            SensorRegNumber=FACTORY_END_ADDR;
            for (i=0;i<SensorRegNumber;i++)
            {
            	spin_lock(&imx135_drv_lock);    
                IMX135MIPISensorCCT[i].Addr=*pFeatureData32++;
                IMX135MIPISensorCCT[i].Para=*pFeatureData32++; 
				spin_unlock(&imx135_drv_lock);
            }
            break;
        case SENSOR_FEATURE_GET_CCT_REGISTER:
            SensorRegNumber=FACTORY_END_ADDR;
            if (*pFeatureParaLen<(SensorRegNumber*sizeof(SENSOR_REG_STRUCT)+4))
                return FALSE;
            *pFeatureData32++=SensorRegNumber;
            for (i=0;i<SensorRegNumber;i++)
            {
                *pFeatureData32++=IMX135MIPISensorCCT[i].Addr;
                *pFeatureData32++=IMX135MIPISensorCCT[i].Para; 
            }
            break;
        case SENSOR_FEATURE_SET_ENG_REGISTER:
            SensorRegNumber=ENGINEER_END;
            for (i=0;i<SensorRegNumber;i++)
            {	spin_lock(&imx135_drv_lock);    
                IMX135MIPISensorReg[i].Addr=*pFeatureData32++;
                IMX135MIPISensorReg[i].Para=*pFeatureData32++;
				spin_unlock(&imx135_drv_lock);
            }
            break;
        case SENSOR_FEATURE_GET_ENG_REGISTER:
            SensorRegNumber=ENGINEER_END;
            if (*pFeatureParaLen<(SensorRegNumber*sizeof(SENSOR_REG_STRUCT)+4))
                return FALSE;
            *pFeatureData32++=SensorRegNumber;
            for (i=0;i<SensorRegNumber;i++)
            {
                *pFeatureData32++=IMX135MIPISensorReg[i].Addr;
                *pFeatureData32++=IMX135MIPISensorReg[i].Para;
            }
            break;
        case SENSOR_FEATURE_GET_REGISTER_DEFAULT:
            if (*pFeatureParaLen>=sizeof(NVRAM_SENSOR_DATA_STRUCT))
            {
                pSensorDefaultData->Version=NVRAM_CAMERA_SENSOR_FILE_VERSION;
                pSensorDefaultData->SensorId=IMX135MIPI_SENSOR_ID;
                memcpy(pSensorDefaultData->SensorEngReg, IMX135MIPISensorReg, sizeof(SENSOR_REG_STRUCT)*ENGINEER_END);
                memcpy(pSensorDefaultData->SensorCCTReg, IMX135MIPISensorCCT, sizeof(SENSOR_REG_STRUCT)*FACTORY_END_ADDR);
            }
            else
                return FALSE;
            *pFeatureParaLen=sizeof(NVRAM_SENSOR_DATA_STRUCT);
            break;
        case SENSOR_FEATURE_GET_CONFIG_PARA:
            memcpy(pSensorConfigData, &IMX135MIPISensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));
            *pFeatureParaLen=sizeof(MSDK_SENSOR_CONFIG_STRUCT);
            break;
        case SENSOR_FEATURE_CAMERA_PARA_TO_SENSOR:
            IMX135MIPI_camera_para_to_sensor();
            break;

        case SENSOR_FEATURE_SENSOR_TO_CAMERA_PARA:
            IMX135MIPI_sensor_to_camera_para();
            break;
        case SENSOR_FEATURE_GET_GROUP_COUNT:
            *pFeatureReturnPara32++=IMX135MIPI_get_sensor_group_count();
            *pFeatureParaLen=4;
            break;
        case SENSOR_FEATURE_GET_GROUP_INFO:
            IMX135MIPI_get_sensor_group_info(pSensorGroupInfo->GroupIdx, pSensorGroupInfo->GroupNamePtr, &pSensorGroupInfo->ItemCount);
            *pFeatureParaLen=sizeof(MSDK_SENSOR_GROUP_INFO_STRUCT);
            break;
        case SENSOR_FEATURE_GET_ITEM_INFO:
            IMX135MIPI_get_sensor_item_info(pSensorItemInfo->GroupIdx,pSensorItemInfo->ItemIdx, pSensorItemInfo);
            *pFeatureParaLen=sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
            break;

        case SENSOR_FEATURE_SET_ITEM_INFO:
            IMX135MIPI_set_sensor_item_info(pSensorItemInfo->GroupIdx, pSensorItemInfo->ItemIdx, pSensorItemInfo->ItemValue);
            *pFeatureParaLen=sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
            break;

        case SENSOR_FEATURE_GET_ENG_INFO:
            pSensorEngInfo->SensorId = 129;
            pSensorEngInfo->SensorType = CMOS_SENSOR;
            pSensorEngInfo->SensorOutputDataFormat=SENSOR_OUTPUT_FORMAT_RAW_Gb;
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
            IMX135MIPISetVideoMode(*pFeatureData16);
            break;
        case SENSOR_FEATURE_CHECK_SENSOR_ID:
            IMX135MIPIGetSensorID(pFeatureReturnPara32); 
            break;             
        case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
            IMX135MIPISetAutoFlickerMode((BOOL)*pFeatureData16, *(pFeatureData16+1));            
	        break;
        case SENSOR_FEATURE_SET_TEST_PATTERN:
            IMX135MIPISetTestPatternMode((BOOL)*pFeatureData16);        	
            break;
		case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
			IMX135MIPISetMaxFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32, *(pFeatureData32+1));
			break;
		case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
			IMX135MIPIGetDefaultFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32, (MUINT32 *)(*(pFeatureData32+1)));
			break;
		case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE://for factory mode auto testing             
            *pFeatureReturnPara32= IMX135_TEST_PATTERN_CHECKSUM;
            *pFeatureParaLen=4;                             
             break; 
        default:
            break;
    }
    return ERROR_NONE;
}	/* IMX135MIPIFeatureControl() */


SENSOR_FUNCTION_STRUCT	SensorFuncIMX135MIPI=
{
    IMX135MIPIOpen,
    IMX135MIPIGetInfo,
    IMX135MIPIGetResolution,
    IMX135MIPIFeatureControl,
    IMX135MIPIControl,
    IMX135MIPIClose
};

UINT32 IMX135_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
    /* To Do : Check Sensor status here */
    if (pfFunc!=NULL)
        *pfFunc=&SensorFuncIMX135MIPI;

    return ERROR_NONE;
}   /* SensorInit() */

