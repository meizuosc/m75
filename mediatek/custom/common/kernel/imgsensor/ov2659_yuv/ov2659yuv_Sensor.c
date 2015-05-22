#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include <asm/io.h>
#include <asm/system.h>	 
#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "kd_camera_feature.h"
#include "ov2659yuv_Sensor.h"
#include "ov2659yuv_Camera_Sensor_para.h"
#include "ov2659yuv_CameraCustomized.h" 

#define OV2659YUV_DEBUG
#ifdef OV2659YUV_DEBUG
#define OV2659SENSORDB printk
#else
#define OV2659SENSORDB(x,...)
#endif

static DEFINE_SPINLOCK(ov2659_drv_lock);

extern int iReadReg(u16 a_u2Addr , u8 * a_puBuff , u16 i2cId);
extern int iWriteReg(u16 a_u2Addr , u32 a_u4Data , u32 a_u4Bytes , u16 i2cId);

#define OV2659_write_cmos_sensor(addr, para) iWriteReg((u16) addr , (u32) para ,1,OV2659_WRITE_ID)
#define mDELAY(ms)  mdelay(ms)
#define OV2659_TEST_PATTERN_CHECKSUM (0xc6bca6c1)

static struct
{
	kal_uint8   IsPVmode;
	kal_uint32  PreviewDummyPixels;
	kal_uint32  PreviewDummyLines;
	kal_uint32  CaptureDummyPixels;
	kal_uint32  CaptureDummyLines;
	kal_uint32  PreviewPclk;
	kal_uint32  CapturePclk;
	kal_uint32  PreviewShutter;
	kal_uint32  SensorGain;
	kal_uint32  sceneMode;
	kal_uint32  SensorShutter;
	unsigned char 	isoSpeed;
	OV2659_SENSOR_MODE SensorMode;
} OV2659Sensor;

UINT16 WBcount = 0;

MSDK_SENSOR_CONFIG_STRUCT OV2659SensorConfigData;



kal_uint16 OV2659_read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte=0;
    iReadReg((u16) addr ,(u8*)&get_byte,OV2659_WRITE_ID);
    
    return get_byte;
}



kal_uint32 OV2659_gain_check(kal_uint32 gain)
{
	gain = (gain > OV2659_MAX_GAIN) ? OV2659_MAX_GAIN : gain;
	gain = (gain < OV2659_MIN_GAIN) ? OV2659_MIN_GAIN : gain;

	return gain;
}



kal_uint32 OV2659_shutter_check(kal_uint32 shutter)
{
	if(OV2659Sensor.IsPVmode)
		shutter = (shutter > OV2659_MAX_SHUTTER_PREVIEW) ? OV2659_MAX_SHUTTER_PREVIEW : shutter;
	else
		shutter = (shutter > OV2659_MAX_SHUTTER_CAPTIRE) ? OV2659_MAX_SHUTTER_CAPTIRE: shutter;
	
	shutter = (shutter < OV2659_MIN_SHUTTER) ? OV2659_MIN_SHUTTER : shutter;

	return shutter;
}



static void OV2659_set_AE_mode(kal_bool AE_enable)
{
    kal_uint8 AeTemp;

	AeTemp = OV2659_read_cmos_sensor(0x3503); 

    if (AE_enable == KAL_TRUE)
    {
    	OV2659SENSORDB("[OV2659_set_AE_mode] enable\n");
        OV2659_write_cmos_sensor(0x3503, (AeTemp&(~0x07)));
    }
    else
    {
    	OV2659SENSORDB("[OV2659_set_AE_mode] disable\n");
      	OV2659_write_cmos_sensor(0x3503, (AeTemp| 0x07));
    }
}



static void OV2659WriteShutter(kal_uint32 shutter)
{
	kal_uint32 extra_exposure_lines = 0;

	OV2659SENSORDB("[OV2659WriteShutter] shutter=%d\n", shutter);
	
	if (OV2659Sensor.IsPVmode) 
	{
		if (shutter <= OV2659_PV_EXPOSURE_LIMITATION) 
			extra_exposure_lines = 0;
		else 
			extra_exposure_lines=shutter - OV2659_PV_EXPOSURE_LIMITATION;		
	}
	else 
	{
		if (shutter <= OV2659_FULL_EXPOSURE_LIMITATION) 
			extra_exposure_lines = 0;
		else 
			extra_exposure_lines = shutter - OV2659_FULL_EXPOSURE_LIMITATION;		
	}
	
	shutter*=16;
	
	OV2659_write_cmos_sensor(0x3502, shutter & 0x00FF);          
	OV2659_write_cmos_sensor(0x3501, ((shutter & 0x0FF00) >>8));  
	OV2659_write_cmos_sensor(0x3500, ((shutter & 0xFF0000) >> 16));	
	
	if(extra_exposure_lines>0)
	{
		OV2659_write_cmos_sensor(0x3507, extra_exposure_lines & 0xFF);         
		OV2659_write_cmos_sensor(0x3506, (extra_exposure_lines & 0xFF00) >> 8); 
	}
	else
	{
		OV2659_write_cmos_sensor(0x3507, 0x00);        
		OV2659_write_cmos_sensor(0x3506, 0x00); 
	}
}   



static void OV2659WriteSensorGain(kal_uint32 gain)
{
	kal_uint16 temp_reg = 0;

	OV2659SENSORDB("[OV2659WriteSensorGain] gain=%d\n", gain);
		
	gain = OV2659_gain_check(gain);
	temp_reg = 0;
	temp_reg=gain&0xFF;
	
	OV2659_write_cmos_sensor(0x350B,temp_reg);
} 



void OV2659_night_mode(kal_bool enable)
{
	kal_uint16 night = OV2659_read_cmos_sensor(0x3A00); 
	
	if (enable)
	{
		OV2659SENSORDB("[OV2659_night_mode] enable\n");
		
       	OV2659_write_cmos_sensor(0x3A00,night|0x04); // 25fps-5fps
       	OV2659_write_cmos_sensor(0x3a02,0x0e); 
      	OV2659_write_cmos_sensor(0x3a03,0x70);                         
      	OV2659_write_cmos_sensor(0x3a14,0x0e); 
      	OV2659_write_cmos_sensor(0x3a15,0x70);                    
    }
	else
	{   
		OV2659SENSORDB("[OV2659_night_mode] disable\n");
		
       	OV2659_write_cmos_sensor(0x3A00,night|0x04); //25fps-12.5fps               
      	OV2659_write_cmos_sensor(0x3a02,0x07);
       	OV2659_write_cmos_sensor(0x3a03,0x38);
     	OV2659_write_cmos_sensor(0x3a14,0x07); 
      	OV2659_write_cmos_sensor(0x3a15,0x38);
    }
}	




void OV2659_set_contrast(UINT16 para)
{   
    OV2659SENSORDB("[OV2659_set_contrast]para=%d\n", para);
	
    switch (para)
    {
        case ISP_CONTRAST_HIGH:
			OV2659_write_cmos_sensor(0x3208,0x00); 
           	OV2659_write_cmos_sensor(0x5080,0x28);             
           	OV2659_write_cmos_sensor(0x5081,0x28);                         
           	OV2659_write_cmos_sensor(0x3208,0x10); 
           	OV2659_write_cmos_sensor(0x3208,0xa0); 
           	break;
        case ISP_CONTRAST_MIDDLE:
			OV2659_write_cmos_sensor(0x3208,0x00);          
           	OV2659_write_cmos_sensor(0x5080,0x20);             
           	OV2659_write_cmos_sensor(0x5081,0x20);                        
           	OV2659_write_cmos_sensor(0x3208,0x10); 
           	OV2659_write_cmos_sensor(0x3208,0xa0); 
			break;
		case ISP_CONTRAST_LOW:
			OV2659_write_cmos_sensor(0x3208,0x00);          
         	OV2659_write_cmos_sensor(0x5080,0x18);             
          	OV2659_write_cmos_sensor(0x5081,0x18);                        
          	OV2659_write_cmos_sensor(0x3208,0x10); 
          	OV2659_write_cmos_sensor(0x3208,0xa0); 
           	break;
        default:
             break;
    }
	
    return;
}



void OV2659_set_brightness(UINT16 para)
{
    OV2659SENSORDB("[OV5645MIPI_set_brightness]para=%d\n", para);
	
    switch (para)
    {
        case ISP_BRIGHT_HIGH:
        	OV2659_write_cmos_sensor(0x3208,0x00); 
      		OV2659_write_cmos_sensor(0x3a0f,0x50); 
           	OV2659_write_cmos_sensor(0x3a10,0x48); 
           	OV2659_write_cmos_sensor(0x3a11,0xa0);
          	OV2659_write_cmos_sensor(0x3a1b,0x50); 
          	OV2659_write_cmos_sensor(0x3a1e,0x48); 	   
           	OV2659_write_cmos_sensor(0x3a1f,0x24);            
         	OV2659_write_cmos_sensor(0x3208,0x10); 
          	OV2659_write_cmos_sensor(0x3208,0xa0); 
            break;
        case ISP_BRIGHT_MIDDLE:
    		OV2659_write_cmos_sensor(0x3208,0x00); 
    	  	OV2659_write_cmos_sensor(0x3a0f,0x38); 
          	OV2659_write_cmos_sensor(0x3a10,0x30); 
           	OV2659_write_cmos_sensor(0x3a11,0x70);
          	OV2659_write_cmos_sensor(0x3a1b,0x38); 
          	OV2659_write_cmos_sensor(0x3a1e,0x30); 	   
           	OV2659_write_cmos_sensor(0x3a1f,0x18);            
           	OV2659_write_cmos_sensor(0x3208,0x10); 
        	OV2659_write_cmos_sensor(0x3208,0xa0); 
			break;
		case ISP_BRIGHT_LOW:
			OV2659_write_cmos_sensor(0x3208,0x00); 
			OV2659_write_cmos_sensor(0x3a0f,0x20); 
          	OV2659_write_cmos_sensor(0x3a10,0x18); 
         	OV2659_write_cmos_sensor(0x3a11,0x40);
           	OV2659_write_cmos_sensor(0x3a1b,0x20); 
         	OV2659_write_cmos_sensor(0x3a1e,0x18); 	   
         	OV2659_write_cmos_sensor(0x3a1f,0x0c);             
           	OV2659_write_cmos_sensor(0x3208,0x10); 
       		OV2659_write_cmos_sensor(0x3208,0xa0); 		
            break;
        default:
            break;
    }
	
    return;
}



void OV2659_set_saturation(UINT16 para)
{
	OV2659SENSORDB("[OV5645MIPI_set_saturation]para=%d\n", para);
	
    switch (para)
    {
        case ISP_SAT_HIGH:
			OV2659_write_cmos_sensor(0x3208,0x00); 
			OV2659_write_cmos_sensor(0x5073,0x1B);
            OV2659_write_cmos_sensor(0x5074,0xd6); 
            OV2659_write_cmos_sensor(0x5075,0xf2); 
            OV2659_write_cmos_sensor(0x5076,0xf2); 
            OV2659_write_cmos_sensor(0x5077,0xe7); 
            OV2659_write_cmos_sensor(0x5078,0x0a); 
			OV2659_write_cmos_sensor(0x3208,0x10); 
          	OV2659_write_cmos_sensor(0x3208,0xa0); 
            break;
        case ISP_SAT_MIDDLE:
			OV2659_write_cmos_sensor(0x3208,0x00); 
    	    OV2659_write_cmos_sensor(0x5073,0x17);
            OV2659_write_cmos_sensor(0x5074,0xb3); 
            OV2659_write_cmos_sensor(0x5075,0xca); 
            OV2659_write_cmos_sensor(0x5076,0xca); 
            OV2659_write_cmos_sensor(0x5077,0xc1); 
            OV2659_write_cmos_sensor(0x5078,0x09); 	
			OV2659_write_cmos_sensor(0x3208,0x10); 
         	OV2659_write_cmos_sensor(0x3208,0xa0); 
			break;
		case ISP_SAT_LOW:
			OV2659_write_cmos_sensor(0x3208,0x00); 
			OV2659_write_cmos_sensor(0x5073,0x12);
            OV2659_write_cmos_sensor(0x5074,0x8f); 
            OV2659_write_cmos_sensor(0x5075,0xa1); 
            OV2659_write_cmos_sensor(0x5076,0xa1); 
            OV2659_write_cmos_sensor(0x5077,0x9a); 
            OV2659_write_cmos_sensor(0x5078,0x07); 
			OV2659_write_cmos_sensor(0x3208,0x10); 
          	OV2659_write_cmos_sensor(0x3208,0xa0); 
            break;
        default:
			break;
    }
	
     return;
}



void OV2659_set_iso(UINT16 para)
{
	OV2659SENSORDB("[OV5645MIPI_set_iso]para=%d\n", para);
	
    spin_lock(&ov2659_drv_lock);
    OV2659Sensor.isoSpeed = para;
    spin_unlock(&ov2659_drv_lock);   
	
    switch (para)
    {
    	case AE_ISO_AUTO:
			OV2659_write_cmos_sensor(0x3a19,0x38);
            break;
        case AE_ISO_100:
           	OV2659_write_cmos_sensor(0x3a19,0x18);
            break;
        case AE_ISO_200:
			OV2659_write_cmos_sensor(0x3a19,0x38);
            break;
        case AE_ISO_400:
      		OV2659_write_cmos_sensor(0x3a19,0x58);
            break;
        default:
            break;
    }
    return;
}



BOOL OV2659_set_param_exposure_for_HDR(UINT16 para)
{
    kal_uint32 gain = 0, shutter = 0;
	
	OV2659SENSORDB("[OV2659_set_param_exposure_for_HDR]para=%d\n", para);
	
    OV2659_set_AE_mode(KAL_FALSE);
    
	gain = OV2659Sensor.SensorGain;
    shutter = OV2659Sensor.SensorShutter;
	
	switch (para)
	{
	   case AE_EV_COMP_20:	
       case AE_EV_COMP_10:	
		   gain =gain<<1;
           shutter = shutter<<1;   		
		 break;
	   case AE_EV_COMP_00:
		 break;
	   case AE_EV_COMP_n10: 
	   case AE_EV_COMP_n20:
		   gain = gain >> 1;
           shutter = shutter >> 1;
		 break;
	   default:
		 break;
	}

    OV2659WriteSensorGain(gain);	
	OV2659WriteShutter(shutter);	
	
	return TRUE;
}



void OV2659_set_scene_mode(UINT16 para)
{
	OV2659SENSORDB("[OV2659_set_scene_mode]para=%d\n", para);
	
	spin_lock(&ov2659_drv_lock);
	OV2659Sensor.sceneMode=para;
	spin_unlock(&ov2659_drv_lock);
	
    switch (para)
    { 
		case SCENE_MODE_NIGHTSCENE:
          	 OV2659_night_mode(KAL_TRUE); 
			 break;
        case SCENE_MODE_PORTRAIT:
        case SCENE_MODE_LANDSCAPE:
        case SCENE_MODE_SUNSET:
        case SCENE_MODE_SPORTS:	 
        case SCENE_MODE_HDR:
            break;
        case SCENE_MODE_OFF:
			OV2659_night_mode(KAL_FALSE);
			break;
        default:
			return KAL_FALSE;
            break;
    }
	
	return;
}



static void OV2659SetDummy(kal_uint32 dummy_pixels, kal_uint32 dummy_lines)
{
	kal_uint32 temp_reg, temp_reg1, temp_reg2;

	OV2659SENSORDB("[OV2659SetDummy] dummy_pixels=%d, dummy_lines=%d\n", dummy_pixels, dummy_lines);
	
	if (dummy_pixels > 0)
	{
		temp_reg1 = OV2659_read_cmos_sensor(0x380D);  
		temp_reg2 = OV2659_read_cmos_sensor(0x380C);  
		
		temp_reg = (temp_reg1 & 0xFF) | (temp_reg2 << 8);
		temp_reg += dummy_pixels;
	
		OV2659_write_cmos_sensor(0x380D,(temp_reg&0xFF));        
		OV2659_write_cmos_sensor(0x380C,((temp_reg&0xFF00)>>8));
	}

	if (dummy_lines > 0)
	{
		temp_reg1 = OV2659_read_cmos_sensor(0x380F);    
		temp_reg2 = OV2659_read_cmos_sensor(0x380E);  
		
		temp_reg = (temp_reg1 & 0xFF) | (temp_reg2 << 8);
		temp_reg += dummy_lines;
	
		OV2659_write_cmos_sensor(0x380F,(temp_reg&0xFF));        
		OV2659_write_cmos_sensor(0x380E,((temp_reg&0xFF00)>>8)); 
	}
}



static kal_uint32 OV2659ReadShutter(void)
{
	kal_uint16 temp_reg1, temp_reg2 ,temp_reg3;
	
	temp_reg1 = OV2659_read_cmos_sensor(0x3500);   
	temp_reg2 = OV2659_read_cmos_sensor(0x3501);  
	temp_reg3 = OV2659_read_cmos_sensor(0x3502); 

	spin_lock(&ov2659_drv_lock);
	OV2659Sensor.PreviewShutter  = (temp_reg1 <<12)| (temp_reg2<<4)|(temp_reg3>>4);
	spin_unlock(&ov2659_drv_lock);

	OV2659SENSORDB("[OV2659ReadShutter] shutter=%d\n", OV2659Sensor.PreviewShutter);
	
	return OV2659Sensor.PreviewShutter;
}



static kal_uint32 OV2659ReadSensorGain(void)
{
	kal_uint32 sensor_gain = 0;
	
	sensor_gain=(OV2659_read_cmos_sensor(0x350B)&0xFF); 		

	OV2659SENSORDB("[OV2659ReadSensorGain] gain_350B=%d\n", sensor_gain);

	return sensor_gain;
}  




static void OV2659_set_AWB_mode(kal_bool AWB_enable)
{
    kal_uint8 AwbTemp;
	
	AwbTemp = OV2659_read_cmos_sensor(0x3406);

    if (AWB_enable == KAL_TRUE)
    {
    	OV2659SENSORDB("[OV2659_set_AWB_mode] enable\n");
		OV2659_write_cmos_sensor(0x3406, AwbTemp&0xFE); 
    }
    else
    {
    	OV2659SENSORDB("[OV2659_set_AWB_mode] disable\n");
		OV2659_write_cmos_sensor(0x3406, AwbTemp|0x01); 
    }

}



BOOL OV2659_set_param_wb(UINT16 para)
{
	OV2659SENSORDB("[OV2659_set_param_wb]para=%d\n", para);

    switch (para)
    {
        case AWB_MODE_OFF:
            OV2659_set_AWB_mode(KAL_FALSE);
            break;                    
        case AWB_MODE_AUTO:
            OV2659_set_AWB_mode(KAL_TRUE);
			break;
        case AWB_MODE_CLOUDY_DAYLIGHT:
        	 OV2659_set_AWB_mode(KAL_FALSE); 
        	 OV2659_write_cmos_sensor(0x3208,0x00); 
			 OV2659_write_cmos_sensor(0x3400, 0x06);
			 OV2659_write_cmos_sensor(0x3401, 0x30);
			 OV2659_write_cmos_sensor(0x3402, 0x04);
			 OV2659_write_cmos_sensor(0x3403, 0x00);
			 OV2659_write_cmos_sensor(0x3404, 0x04);
			 OV2659_write_cmos_sensor(0x3405, 0x30);
             OV2659_write_cmos_sensor(0x3208,0x10); 
             OV2659_write_cmos_sensor(0x3208,0xa0); 
             break;	
        case AWB_MODE_DAYLIGHT:
        	 OV2659_set_AWB_mode(KAL_FALSE); 
             OV2659_write_cmos_sensor(0x3208,0x00);                           
			 OV2659_write_cmos_sensor(0x3400, 0x06);
			 OV2659_write_cmos_sensor(0x3401, 0x10);
			 OV2659_write_cmos_sensor(0x3402, 0x04);
			 OV2659_write_cmos_sensor(0x3403, 0x00);
			 OV2659_write_cmos_sensor(0x3404, 0x04);
			 OV2659_write_cmos_sensor(0x3405, 0x48);
             OV2659_write_cmos_sensor(0x3208,0x10); 
             OV2659_write_cmos_sensor(0x3208,0xa0); 
			 break;
        case AWB_MODE_INCANDESCENT:
        	 OV2659_set_AWB_mode(KAL_FALSE); 
             OV2659_write_cmos_sensor(0x3208,0x00); 
			 OV2659_write_cmos_sensor(0x3400, 0x04);
			 OV2659_write_cmos_sensor(0x3401, 0xe0);
			 OV2659_write_cmos_sensor(0x3402, 0x04);
			 OV2659_write_cmos_sensor(0x3403, 0x00);
			 OV2659_write_cmos_sensor(0x3404, 0x05);
			 OV2659_write_cmos_sensor(0x3405, 0xa0);
             OV2659_write_cmos_sensor(0x3208,0x10); 
             OV2659_write_cmos_sensor(0x3208,0xa0); 
			 break;
        case AWB_MODE_TUNGSTEN:
			 OV2659_set_AWB_mode(KAL_FALSE); 
             OV2659_write_cmos_sensor(0x3208,0x00);                           
             OV2659_write_cmos_sensor(0x3400,0x5); 
             OV2659_write_cmos_sensor(0x3401,0x48); 
			 OV2659_write_cmos_sensor(0x3402,0x4 );
			 OV2659_write_cmos_sensor(0x3403,0x0 );
             OV2659_write_cmos_sensor(0x3404,0x5); 
             OV2659_write_cmos_sensor(0x3405,0xe0);              
             OV2659_write_cmos_sensor(0x3208,0x10); 
             OV2659_write_cmos_sensor(0x3208,0xa0); 
			 break;
        case AWB_MODE_FLUORESCENT:
			 OV2659_set_AWB_mode(KAL_FALSE); 
             OV2659_write_cmos_sensor(0x3208,0x00);                                        
             OV2659_write_cmos_sensor(0x3400,0x4); 
             OV2659_write_cmos_sensor(0x3401,0x0); 
			 OV2659_write_cmos_sensor(0x3402,0x4 );
			 OV2659_write_cmos_sensor(0x3403,0x0 );
             OV2659_write_cmos_sensor(0x3404,0x6); 
             OV2659_write_cmos_sensor(0x3405,0x50);              
             OV2659_write_cmos_sensor(0x3208,0x10); 
             OV2659_write_cmos_sensor(0x3208,0xa0); 
			 break;
        default:
            return FALSE;
    }

	spin_lock(&ov2659_drv_lock);
	WBcount= para;
	spin_unlock(&ov2659_drv_lock);
	
    return TRUE;
}



static kal_uint32 OV2659_GetSensorID(kal_uint32 *sensorID)
{
	volatile signed char i;
	kal_uint32 sensor_id=0;
	
	OV2659_write_cmos_sensor(0x0103,0x01);
	  mDELAY(10);
	
	for(i=0;i<3;i++)
	{
		sensor_id = (OV2659_read_cmos_sensor(0x300A) << 8) | OV2659_read_cmos_sensor(0x300B);
		
		OV2659SENSORDB("[OV2659_GetSensorID] sensorID=%x\n", sensor_id);

		if(sensor_id != OV2659_SENSOR_ID)
		{	
			*sensorID =0xffffffff;
			return ERROR_SENSOR_CONNECT_FAIL;
		}
	}
	
	*sensorID = sensor_id;
	
    return ERROR_NONE;    
}   



static void OV2659InitialSetting(void)
{
	OV2659SENSORDB("[OV2659InitialSetting]\n");
	
	OV2659_write_cmos_sensor(0x3000,0x0f); 
	OV2659_write_cmos_sensor(0x3001,0xff); 
	OV2659_write_cmos_sensor(0x3002,0xff); 
	OV2659_write_cmos_sensor(0x0100,0x01); 
	OV2659_write_cmos_sensor(0x3633,0x3d); 
	OV2659_write_cmos_sensor(0x3620,0x02); 
	OV2659_write_cmos_sensor(0x3631,0x11); 
	OV2659_write_cmos_sensor(0x3612,0x04); 
	OV2659_write_cmos_sensor(0x3630,0x20); 
	OV2659_write_cmos_sensor(0x4702,0x02); 
	OV2659_write_cmos_sensor(0x370c,0x34); 
	OV2659_write_cmos_sensor(0x4003,0x88); 						   
	OV2659_write_cmos_sensor(0x3800,0x00); 
	OV2659_write_cmos_sensor(0x3801,0x00); 
	OV2659_write_cmos_sensor(0x3802,0x00); 
	OV2659_write_cmos_sensor(0x3803,0x00); 
	OV2659_write_cmos_sensor(0x3804,0x06); 
	OV2659_write_cmos_sensor(0x3805,0x5f); 
	OV2659_write_cmos_sensor(0x3806,0x04); 
	OV2659_write_cmos_sensor(0x3807,0xb7); 
	OV2659_write_cmos_sensor(0x3808,0x03); 
	OV2659_write_cmos_sensor(0x3809,0x20); 
	OV2659_write_cmos_sensor(0x380a,0x02); 
	OV2659_write_cmos_sensor(0x380b,0x58); 
	OV2659_write_cmos_sensor(0x3811,0x08); 
	OV2659_write_cmos_sensor(0x3813,0x02); 
	OV2659_write_cmos_sensor(0x3814,0x31); 
	OV2659_write_cmos_sensor(0x3815,0x31);		  
	OV2659_write_cmos_sensor(0x3820,0x81); 
	OV2659_write_cmos_sensor(0x3821,0x01); 
	OV2659_write_cmos_sensor(0x5002,0x10);
	OV2659_write_cmos_sensor(0x4608,0x00); 
	OV2659_write_cmos_sensor(0x4609,0xa0);	
	OV2659_write_cmos_sensor(0x3623,0x00); 
	OV2659_write_cmos_sensor(0x3634,0x76); 
	OV2659_write_cmos_sensor(0x3701,0x44); 
	OV2659_write_cmos_sensor(0x3208,0x01);
	OV2659_write_cmos_sensor(0x3702,0x18); 
	OV2659_write_cmos_sensor(0x3703,0x24); 
	OV2659_write_cmos_sensor(0x3704,0x24); 
	OV2659_write_cmos_sensor(0x3208,0x11);
	OV2659_write_cmos_sensor(0x3208,0x02);
	OV2659_write_cmos_sensor(0x3702,0x30); 
	OV2659_write_cmos_sensor(0x3703,0x48); 
	OV2659_write_cmos_sensor(0x3704,0x48); 
	OV2659_write_cmos_sensor(0x3208,0x12);
	OV2659_write_cmos_sensor(0x3705,0x0c); 
	OV2659_write_cmos_sensor(0x370a,0x52); 
	OV2659_write_cmos_sensor(0x3003,0x80);//3
	OV2659_write_cmos_sensor(0x3004,0x10); 
	OV2659_write_cmos_sensor(0x3005,0x16); 
	OV2659_write_cmos_sensor(0x3006,0x0d); 
	OV2659_write_cmos_sensor(0x380c,0x05); 
	OV2659_write_cmos_sensor(0x380d,0x14); 
	OV2659_write_cmos_sensor(0x380e,0x02); 
	OV2659_write_cmos_sensor(0x380f,0xe3); 						
	OV2659_write_cmos_sensor(0x3a05,0x30);				
	OV2659_write_cmos_sensor(0x3a08,0x00); 
	OV2659_write_cmos_sensor(0x3a09,0xb9); 
	OV2659_write_cmos_sensor(0x3a0e,0x04); 
	OV2659_write_cmos_sensor(0x3a0a,0x00); 
	OV2659_write_cmos_sensor(0x3a0b,0x9a); 
	OV2659_write_cmos_sensor(0x3a0d,0x04);								  
	OV2659_write_cmos_sensor(0x3a00,0x3c); 
	OV2659_write_cmos_sensor(0x3a02,0x05); 
	OV2659_write_cmos_sensor(0x3a03,0xc6); 
	OV2659_write_cmos_sensor(0x3a14,0x05); 
	OV2659_write_cmos_sensor(0x3a15,0xc6);	  
	OV2659_write_cmos_sensor(0x350c,0x00); 
	OV2659_write_cmos_sensor(0x350d,0x00); 
	OV2659_write_cmos_sensor(0x4300,0x31); 
	OV2659_write_cmos_sensor(0x5086,0x02); 
	OV2659_write_cmos_sensor(0x5000,0xff); 
	OV2659_write_cmos_sensor(0x5001,0x1f); 
	OV2659_write_cmos_sensor(0x507e,0x3a);
	OV2659_write_cmos_sensor(0x507f,0x10); 
	OV2659_write_cmos_sensor(0x507c,0x80); 
	OV2659_write_cmos_sensor(0x507d,0x00);		   
	OV2659_write_cmos_sensor(0x507b,0x06);								
	OV2659_write_cmos_sensor(0x5025,0x06); 
	OV2659_write_cmos_sensor(0x5026,0x0c); 
	OV2659_write_cmos_sensor(0x5027,0x1c); 
	OV2659_write_cmos_sensor(0x5028,0x36); 
	OV2659_write_cmos_sensor(0x5029,0x4e); 
	OV2659_write_cmos_sensor(0x502a,0x5f);		   
	OV2659_write_cmos_sensor(0x502b,0x6d); 
	OV2659_write_cmos_sensor(0x502c,0x78); 
	OV2659_write_cmos_sensor(0x502d,0x84); 
	OV2659_write_cmos_sensor(0x502e,0x95); 
	OV2659_write_cmos_sensor(0x502f,0xa5); 
	OV2659_write_cmos_sensor(0x5030,0xb4); 
	OV2659_write_cmos_sensor(0x5031,0xc8); 
	OV2659_write_cmos_sensor(0x5032,0xde); 
	OV2659_write_cmos_sensor(0x5033,0xf0); 
	OV2659_write_cmos_sensor(0x5034,0x15); 				   
	OV2659_write_cmos_sensor(0x5070,0x28); 
	OV2659_write_cmos_sensor(0x5071,0x48); 
	OV2659_write_cmos_sensor(0x5072,0x10); 
	OV2659_write_cmos_sensor(0x5073,0x17); 
	OV2659_write_cmos_sensor(0x5074,0xb3); 
	OV2659_write_cmos_sensor(0x5075,0xca); 
	OV2659_write_cmos_sensor(0x5076,0xca); 
	OV2659_write_cmos_sensor(0x5077,0xc1); 
	OV2659_write_cmos_sensor(0x5078,0x09); 
	OV2659_write_cmos_sensor(0x5079,0x98); 
	OV2659_write_cmos_sensor(0x507a,0x01); 
	OV2659_write_cmos_sensor(0x5035,0x6a); 
	OV2659_write_cmos_sensor(0x5036,0x11); 
	OV2659_write_cmos_sensor(0x5037,0x92); 
	OV2659_write_cmos_sensor(0x5038,0x21); 
	OV2659_write_cmos_sensor(0x5039,0xe1); 
	OV2659_write_cmos_sensor(0x503a,0x01); 
	OV2659_write_cmos_sensor(0x503c,0x10); 
	OV2659_write_cmos_sensor(0x503d,0x10); 
	OV2659_write_cmos_sensor(0x503e,0x10); 
	OV2659_write_cmos_sensor(0x503f,0x65);
	OV2659_write_cmos_sensor(0x5040,0x62);
	OV2659_write_cmos_sensor(0x5041,0x0e); 
	OV2659_write_cmos_sensor(0x5042,0x9c); 
	OV2659_write_cmos_sensor(0x5043,0x20); 
	OV2659_write_cmos_sensor(0x5044,0x28);
	OV2659_write_cmos_sensor(0x5045,0x22); 
	OV2659_write_cmos_sensor(0x5046,0x5c);
	OV2659_write_cmos_sensor(0x5047,0xf8); 
	OV2659_write_cmos_sensor(0x5048,0x08); 
	OV2659_write_cmos_sensor(0x5049,0x70); 
	OV2659_write_cmos_sensor(0x504a,0xf0); 
	OV2659_write_cmos_sensor(0x504b,0xf0);	
	OV2659_write_cmos_sensor(0x500c,0x03); 
	OV2659_write_cmos_sensor(0x500d,0x26); 
	OV2659_write_cmos_sensor(0x500e,0x02); 
	OV2659_write_cmos_sensor(0x500f,0x64);		   
	OV2659_write_cmos_sensor(0x5010,0x6a); 
	OV2659_write_cmos_sensor(0x5011,0x00); 
	OV2659_write_cmos_sensor(0x5012,0x66); 
	OV2659_write_cmos_sensor(0x5013,0x03); 
	OV2659_write_cmos_sensor(0x5014,0x24); 
	OV2659_write_cmos_sensor(0x5015,0x02); 
	OV2659_write_cmos_sensor(0x5016,0x74); 
	OV2659_write_cmos_sensor(0x5017,0x62); 
	OV2659_write_cmos_sensor(0x5018,0x00); 
	OV2659_write_cmos_sensor(0x5019,0x66); 
	OV2659_write_cmos_sensor(0x501a,0x03); 
	OV2659_write_cmos_sensor(0x501b,0x16); 
	OV2659_write_cmos_sensor(0x501c,0x02); 
	OV2659_write_cmos_sensor(0x501d,0x76); 
	OV2659_write_cmos_sensor(0x501e,0x5d); 
	OV2659_write_cmos_sensor(0x501f,0x00); 
	OV2659_write_cmos_sensor(0x5020,0x66);				
	OV2659_write_cmos_sensor(0x506e,0x44);		   
	OV2659_write_cmos_sensor(0x5064,0x08);		   
	OV2659_write_cmos_sensor(0x5065,0x10); 
	OV2659_write_cmos_sensor(0x5066,0x16);		   
	OV2659_write_cmos_sensor(0x5067,0x10); 
	OV2659_write_cmos_sensor(0x506c,0x08); 
	OV2659_write_cmos_sensor(0x506d,0x10);		   
	OV2659_write_cmos_sensor(0x506f,0xa6);		   
	OV2659_write_cmos_sensor(0x5068,0x08); 
	OV2659_write_cmos_sensor(0x5069,0x10);		   
	OV2659_write_cmos_sensor(0x506a,0x18); 
	OV2659_write_cmos_sensor(0x506b,0x28); 					 
	OV2659_write_cmos_sensor(0x5084,0x0c);
	OV2659_write_cmos_sensor(0x5085,0x3c);//3	   
	OV2659_write_cmos_sensor(0x5005,0x80);								   	   
	OV2659_write_cmos_sensor(0x5051,0x40); 
	OV2659_write_cmos_sensor(0x5052,0x40); 
	OV2659_write_cmos_sensor(0x5053,0x40);	
	OV2659_write_cmos_sensor(0x3a0f,0x38); //4
	OV2659_write_cmos_sensor(0x3a10,0x30); //3
	OV2659_write_cmos_sensor(0x3a11,0x70);
	OV2659_write_cmos_sensor(0x3a1b,0x38); //4
	OV2659_write_cmos_sensor(0x3a1e,0x30); //3   
	OV2659_write_cmos_sensor(0x3a1f,0x20); 
	OV2659_write_cmos_sensor(0x5060,0x69); 
	OV2659_write_cmos_sensor(0x5061,0xbe); 
	OV2659_write_cmos_sensor(0x5062,0xbe); 
	OV2659_write_cmos_sensor(0x5063,0x69);		   
	OV2659_write_cmos_sensor(0x3a18,0x00); 
	OV2659_write_cmos_sensor(0x3a19,0x38);
	OV2659_write_cmos_sensor(0x4009,0x02);	
	OV2659_write_cmos_sensor(0x3503,0x00);				   
	OV2659_write_cmos_sensor(0x3011,0x82);		   

	spin_lock(&ov2659_drv_lock);
	OV2659Sensor.IsPVmode= 1;
	OV2659Sensor.PreviewDummyPixels= 0;
	OV2659Sensor.PreviewDummyLines= 0;
	OV2659Sensor.PreviewPclk= 480;
	OV2659Sensor.CapturePclk= 480;
	OV2659Sensor.SensorGain=0x10;
	WBcount = AWB_MODE_AUTO;
	spin_unlock(&ov2659_drv_lock);
}                                  



static void OV2659PreviewSetting(void)
{
	OV2659SENSORDB("[OV2659PreviewSetting]\n");
	
    OV2659_write_cmos_sensor(0X0100, 0X00);	
	OV2659_write_cmos_sensor(0x3503,OV2659_read_cmos_sensor(0x3503)|0x07);
    OV2659_write_cmos_sensor(0x3500,((OV2659Sensor.PreviewShutter*16)>>16)&0xff); 
    OV2659_write_cmos_sensor(0x3501,((OV2659Sensor.PreviewShutter*16)>>8)&0xff); 
   	OV2659_write_cmos_sensor(0x3502,(OV2659Sensor.PreviewShutter*16)&0xff); 
	OV2659_write_cmos_sensor(0x350B, OV2659Sensor.SensorGain);           
   	OV2659_write_cmos_sensor(0x3a00,OV2659_read_cmos_sensor(0x3a00)|0x04); 
  	OV2659_write_cmos_sensor(0x3503,OV2659_read_cmos_sensor(0x3503)&0xf8); 
    OV2659_write_cmos_sensor(0x5066,0x28); 
    OV2659_write_cmos_sensor(0x5067,0x10); 
    OV2659_write_cmos_sensor(0x506a,0x0c); 
    OV2659_write_cmos_sensor(0x506b,0x1c); 
    OV2659_write_cmos_sensor(0x3800,0x00); 
    OV2659_write_cmos_sensor(0x3801,0x00); 
    OV2659_write_cmos_sensor(0x3802,0x00); 
    OV2659_write_cmos_sensor(0x3803,0x00); 
  	OV2659_write_cmos_sensor(0x3804,0x06); 
    OV2659_write_cmos_sensor(0x3805,0x5f); 
    OV2659_write_cmos_sensor(0x3806,0x04); 
  	OV2659_write_cmos_sensor(0x3807,0xb7); 
  	OV2659_write_cmos_sensor(0x3808,0x03); 
    OV2659_write_cmos_sensor(0x3809,0x20); 
   	OV2659_write_cmos_sensor(0x380a,0x02); 
    OV2659_write_cmos_sensor(0x380b,0x58); 
    OV2659_write_cmos_sensor(0x3811,0x08); 
    OV2659_write_cmos_sensor(0x3813,0x02); 
    OV2659_write_cmos_sensor(0x3814,0x31); 
   	OV2659_write_cmos_sensor(0x3815,0x31); 
    if(1) 
    { 
    	OV2659_write_cmos_sensor(0x3820,0x81);           
     	OV2659_write_cmos_sensor(0x3821,0x01);         
    } 
    else 
    {         
    	OV2659_write_cmos_sensor(0x3820,0x87);           
       	OV2659_write_cmos_sensor(0x3821,0x07); 
   	}         
    OV2659_write_cmos_sensor(0x3623,0x00); 
    OV2659_write_cmos_sensor(0x3634,0x76); 
    OV2659_write_cmos_sensor(0x3701,0x44); 
    OV2659_write_cmos_sensor(0x3208,0xa1); 
    OV2659_write_cmos_sensor(0x3705,0x0c);                           
   	OV2659_write_cmos_sensor(0x370a,0x52); 
    OV2659_write_cmos_sensor(0x4608,0x00); 
   	OV2659_write_cmos_sensor(0x4609,0x80); 
    OV2659_write_cmos_sensor(0x5002,0x10);           
   	OV2659_write_cmos_sensor(0x3003,0x80);//30fps 26mclk 
  	OV2659_write_cmos_sensor(0x3004,0x10); 
   	OV2659_write_cmos_sensor(0x3005,0x16); 
   	OV2659_write_cmos_sensor(0x3006,0x0d); 
    OV2659_write_cmos_sensor(0x380c,0x05); 
   	OV2659_write_cmos_sensor(0x380d,0x14); 
  	OV2659_write_cmos_sensor(0x380e,0x02); 
 	OV2659_write_cmos_sensor(0x380f,0x68);     
   	OV2659_write_cmos_sensor(0x3a08,0x00); 
  	OV2659_write_cmos_sensor(0x3a09,0xb9); 
   	OV2659_write_cmos_sensor(0x3a0e,0x03); 
   	OV2659_write_cmos_sensor(0x3a0a,0x00); 
    OV2659_write_cmos_sensor(0x3a0b,0x9a); 
    OV2659_write_cmos_sensor(0x3a0d,0x04);             
    OV2659_write_cmos_sensor(0X0100,0X01);
   	OV2659_write_cmos_sensor(0X301d,0X08);
	mDELAY(10); 
   	OV2659_write_cmos_sensor(0X301d,0X00);
	
	spin_lock(&ov2659_drv_lock);
	OV2659Sensor.IsPVmode = KAL_TRUE;
	OV2659Sensor.PreviewPclk= 480;
	OV2659Sensor.SensorMode= SENSOR_MODE_PREVIEW;
	spin_unlock(&ov2659_drv_lock);
}



static void OV2659FullSizeCaptureSetting(void)
{
	OV2659SENSORDB("[OV2659FullSizeCaptureSetting]\n");

	OV2659_write_cmos_sensor(0X0100, 0X00);	
   	OV2659_write_cmos_sensor(0x3a00,OV2659_read_cmos_sensor(0x3a00)&0xfb); 
  	OV2659_write_cmos_sensor(0x3503,OV2659_read_cmos_sensor(0x3503)|0x07);               
 	OV2659_write_cmos_sensor(0x5066, 0x28);         
 	OV2659_write_cmos_sensor(0x5067, 0x18); 
  	OV2659_write_cmos_sensor(0x506a, 0x06);    
  	OV2659_write_cmos_sensor(0x506b, 0x16);    
   	OV2659_write_cmos_sensor(0x3800, 0x00); 
   	OV2659_write_cmos_sensor(0x3801, 0x00); 
 	OV2659_write_cmos_sensor(0x3802, 0x00); 
   	OV2659_write_cmos_sensor(0x3803, 0x00); 
   	OV2659_write_cmos_sensor(0x3804, 0x06); 
   	OV2659_write_cmos_sensor(0x3805, 0x5f); 
 	OV2659_write_cmos_sensor(0x3806, 0x04); 
   	OV2659_write_cmos_sensor(0x3807, 0xbb); 
  	OV2659_write_cmos_sensor(0x3808, 0x06); 
   	OV2659_write_cmos_sensor(0x3809, 0x40); 
  	OV2659_write_cmos_sensor(0x380a, 0x04); 
  	OV2659_write_cmos_sensor(0x380b, 0xb0); 
   	OV2659_write_cmos_sensor(0x3811, 0x10); 
 	OV2659_write_cmos_sensor(0x3813, 0x06); 
   	OV2659_write_cmos_sensor(0x3814, 0x11); 
 	OV2659_write_cmos_sensor(0x3815, 0x11); 
 	OV2659_write_cmos_sensor(0x3623, 0x00); 
   	OV2659_write_cmos_sensor(0x3634, 0x44); 
  	OV2659_write_cmos_sensor(0x3701, 0x44); 
   	OV2659_write_cmos_sensor(0x3208, 0xa2); 
  	OV2659_write_cmos_sensor(0x3705, 0x18);      
 	OV2659_write_cmos_sensor(0x3820, OV2659_read_cmos_sensor(0x3820)&0xfe); 
  	OV2659_write_cmos_sensor(0x3821, OV2659_read_cmos_sensor(0x3821)&0xfe); 
  	OV2659_write_cmos_sensor(0x370a, 0x12); 
 	OV2659_write_cmos_sensor(0x4608, 0x00); 
 	OV2659_write_cmos_sensor(0x4609, 0x80); 
   	OV2659_write_cmos_sensor(0x5002, 0x00);
  	OV2659_write_cmos_sensor(0x3003, 0x80);//15fps 
  	OV2659_write_cmos_sensor(0x3004, 0x10);        
   	OV2659_write_cmos_sensor(0x3005, 0x21); 
  	OV2659_write_cmos_sensor(0x3006, 0x0d); 
	OV2659_write_cmos_sensor(0x380c, 0x07); 
   	OV2659_write_cmos_sensor(0x380d, 0x9f); 
   	OV2659_write_cmos_sensor(0x380e, 0x04); 
 	OV2659_write_cmos_sensor(0x380f, 0xd0);                        
  	OV2659_write_cmos_sensor(0x3a08, 0x00); 
   	OV2659_write_cmos_sensor(0x3a09, 0xb9);
   	OV2659_write_cmos_sensor(0x3a0e, 0x06);             
   	OV2659_write_cmos_sensor(0x3a0a, 0x00); 
  	OV2659_write_cmos_sensor(0x3a0b, 0x9a);                 
   	OV2659_write_cmos_sensor(0x3a0d, 0x08);         
 	OV2659_write_cmos_sensor(0x4003, 0x88);            	
	OV2659_write_cmos_sensor(0X0100, 0X01);	
	
	spin_lock(&ov2659_drv_lock);
	OV2659Sensor.IsPVmode = KAL_FALSE;
	OV2659Sensor.CapturePclk= 585;
	spin_unlock(&ov2659_drv_lock);
}



static void OV2659SetHVMirror(kal_uint8 Mirror)
{
 	kal_uint8 mirror= 0, flip=0;

	OV2659SENSORDB("[OV2659SetHVMirror]mirror=%d\n", Mirror);
    
  	flip = OV2659_read_cmos_sensor(0x3820);
	mirror=OV2659_read_cmos_sensor(0x3821);

	switch (Mirror)
	{
	case IMAGE_NORMAL:
		OV2659_write_cmos_sensor(0x3820, flip&0xf9);     
		OV2659_write_cmos_sensor(0x3821, mirror&0xf9);
		break;
	case IMAGE_H_MIRROR:
		OV2659_write_cmos_sensor(0x3820, flip&0xf9);     
		OV2659_write_cmos_sensor(0x3821, mirror|0x06);
		break;
	case IMAGE_V_MIRROR: 
		OV2659_write_cmos_sensor(0x3820, flip|0x06);     
		OV2659_write_cmos_sensor(0x3821, mirror&0xf9);
		break;
	case IMAGE_HV_MIRROR:
		OV2659_write_cmos_sensor(0x3820, flip|0x06);     
		OV2659_write_cmos_sensor(0x3821, mirror|0x06);
		break;
	default:
		break;
	}
}



UINT32 OV2659Open(void)
{
	volatile signed int i;
	kal_uint16 sensor_id = 0;
	
	OV2659SENSORDB("[OV2659Open]\n");
	
	OV2659_write_cmos_sensor(0x0103,0x01);
    mDELAY(10);

	for(i=0;i<3;i++)
	{
		sensor_id = (OV2659_read_cmos_sensor(0x300A) << 8) | OV2659_read_cmos_sensor(0x300B);
		
		OV2659SENSORDB("[OV2659Open]SensorId=%x\n", sensor_id);

		if(sensor_id != OV2659_SENSOR_ID)
		{
			return ERROR_SENSOR_CONNECT_FAIL;
		}
	}
	
	spin_lock(&ov2659_drv_lock);
	OV2659Sensor.CaptureDummyPixels = 0;
  	OV2659Sensor.CaptureDummyLines = 0;
	OV2659Sensor.PreviewDummyPixels = 0;
  	OV2659Sensor.PreviewDummyLines = 0;
	OV2659Sensor.SensorMode= SENSOR_MODE_INIT;
	OV2659Sensor.isoSpeed = 100;
	spin_unlock(&ov2659_drv_lock);

	OV2659InitialSetting();

	return ERROR_NONE;
}



UINT32 OV2659Close(void)
{
	OV2659SENSORDB("[OV2659Close]\n");
	
	return ERROR_NONE;
}



UINT32 OV2659Preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{	
	OV2659SENSORDB("[OV2659Preview]enter\n");
	
	OV2659PreviewSetting();
	mDELAY(300);
	OV2659_set_AE_mode(KAL_TRUE);
	OV2659_set_AWB_mode(KAL_TRUE);
	OV2659SetHVMirror(sensor_config_data->SensorImageMirror);
	
	OV2659SENSORDB("[OV2659Preview]exit\n");
	
  	return TRUE ;
}	



UINT32 OV2659Capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint32 shutter = 0, prev_line_len = 0, cap_line_len = 0, temp = 0;

	OV2659SENSORDB("[OV2659Capture]enter\n");
	
	if(SENSOR_MODE_PREVIEW == OV2659Sensor.SensorMode )
	{
		OV2659SENSORDB("[OV2659Capture]Normal Capture\n ");

		OV2659_set_AWB_mode(KAL_FALSE);

        OV2659_write_cmos_sensor(0x3a00,OV2659_read_cmos_sensor(0x3a00)&0xfb); 
        OV2659_write_cmos_sensor(0x3503,OV2659_read_cmos_sensor(0x3503)|0x07); 		
		
		shutter=OV2659ReadShutter();
		temp =OV2659ReadSensorGain();	
		
		mDELAY(30);
		OV2659FullSizeCaptureSetting();
		
		spin_lock(&ov2659_drv_lock);
		OV2659Sensor.SensorMode= SENSOR_MODE_CAPTURE;
		OV2659Sensor.CaptureDummyPixels = 0;
  		OV2659Sensor.CaptureDummyLines = 0;
		spin_unlock(&ov2659_drv_lock);
		
		shutter=shutter * 2;
	

  		OV2659WriteShutter(shutter);
  		mDELAY(300);

		spin_lock(&ov2659_drv_lock);
  		OV2659Sensor.SensorGain= temp;
		OV2659Sensor.SensorShutter = shutter;
		spin_unlock(&ov2659_drv_lock);
	}
	else if(SENSOR_MODE_ZSD == OV2659Sensor.SensorMode)
	{
		//for zsd hdr use
		shutter=OV2659ReadShutter();
		temp =OV2659ReadSensorGain();
		
		spin_lock(&ov2659_drv_lock);
  		OV2659Sensor.SensorGain= temp;
		OV2659Sensor.SensorShutter = shutter;
		spin_unlock(&ov2659_drv_lock);
	}
	
	OV2659SENSORDB("[OV2659Capture]exit\n");
	
	return ERROR_NONE; 
}



UINT32 OV2659ZSDPreview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	OV2659SENSORDB("[OV2659ZSDPreview]enter\n");
	
	if(SENSOR_MODE_PREVIEW == OV2659Sensor.SensorMode || OV2659Sensor.SensorMode == SENSOR_MODE_INIT)
	{
		OV2659FullSizeCaptureSetting();
	}

	spin_lock(&ov2659_drv_lock);
	OV2659Sensor.SensorMode= SENSOR_MODE_ZSD;
	spin_unlock(&ov2659_drv_lock);
	
	OV2659_set_AE_mode(KAL_TRUE);
	OV2659_set_AWB_mode(KAL_TRUE);
	
	OV2659SENSORDB("[OV2659ZSDPreview]exit\n");
	
	return ERROR_NONE; 
}



UINT32 OV2659GetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution)
{
	OV2659SENSORDB("[OV2659GetResolution]\n");

	pSensorResolution->SensorPreviewWidth= OV2659_IMAGE_SENSOR_SVGA_WIDTH - 1 * 8;
	pSensorResolution->SensorPreviewHeight= OV2659_IMAGE_SENSOR_SVGA_HEIGHT - 1 * 8;
	pSensorResolution->SensorFullWidth= OV2659_IMAGE_SENSOR_UVGA_WITDH - 2 * 8;  
	pSensorResolution->SensorFullHeight= OV2659_IMAGE_SENSOR_UVGA_HEIGHT - 2 * 8;
	pSensorResolution->SensorVideoWidth=OV2659_IMAGE_SENSOR_SVGA_WIDTH - 1 * 8;
	pSensorResolution->SensorVideoHeight=OV2659_IMAGE_SENSOR_SVGA_HEIGHT - 1 * 8;
	return ERROR_NONE;
}



UINT32 OV2659GetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId,
					  MSDK_SENSOR_INFO_STRUCT *pSensorInfo,
					  MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
	OV2659SENSORDB("[OV2659GetInfo]\n");

	pSensorInfo->SensorPreviewResolutionX= OV2659_IMAGE_SENSOR_SVGA_WIDTH - 1 * 8;
	pSensorInfo->SensorPreviewResolutionY= OV2659_IMAGE_SENSOR_SVGA_HEIGHT - 1 * 8;
	pSensorInfo->SensorFullResolutionX= OV2659_IMAGE_SENSOR_UVGA_WITDH - 2 * 8;
	pSensorInfo->SensorFullResolutionY= OV2659_IMAGE_SENSOR_UVGA_HEIGHT - 2 * 8;
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
	pSensorInfo->SensroInterfaceType=SENSOR_INTERFACE_TYPE_PARALLEL;
	pSensorInfo->CaptureDelayFrame = 2;
	pSensorInfo->PreviewDelayFrame = 4; 
	pSensorInfo->VideoDelayFrame = 4; 		
	pSensorInfo->SensorMasterClockSwitch = 0; 
    pSensorInfo->SensorDrivingCurrent = ISP_DRIVING_8MA;   		

	switch (ScenarioId)
	{
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			pSensorInfo->SensorClockFreq=26;
			pSensorInfo->SensorClockDividCount=	3;
			pSensorInfo->SensorClockRisingCount= 0;
			pSensorInfo->SensorClockFallingCount= 2;
			pSensorInfo->SensorPixelClockCount= 3;
			pSensorInfo->SensorDataLatchCount= 2;
            pSensorInfo->SensorGrabStartX = 2; 
            pSensorInfo->SensorGrabStartY = 2;             			
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			pSensorInfo->SensorClockFreq=26;
			pSensorInfo->SensorClockDividCount=	3;
			pSensorInfo->SensorClockRisingCount= 0;
			pSensorInfo->SensorClockFallingCount= 2;
			pSensorInfo->SensorPixelClockCount= 3;
			pSensorInfo->SensorDataLatchCount= 2;
            pSensorInfo->SensorGrabStartX = 2; 
            pSensorInfo->SensorGrabStartY = 2;             
			break;
		default:
			pSensorInfo->SensorClockFreq=26;
			pSensorInfo->SensorClockDividCount=3;
			pSensorInfo->SensorClockRisingCount=0;
			pSensorInfo->SensorClockFallingCount=2;
			pSensorInfo->SensorPixelClockCount=3;
			pSensorInfo->SensorDataLatchCount=2;
            pSensorInfo->SensorGrabStartX = 2; 
            pSensorInfo->SensorGrabStartY = 2;             
			break;
	}

	memcpy(pSensorConfigData, &OV2659SensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));
	
	return ERROR_NONE;
}



UINT32 OV2659Control(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow,
					  MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
	OV2659SENSORDB("[OV2659Control]\n");
		
	switch (ScenarioId)
	{
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			OV2659Preview(pImageWindow, pSensorConfigData);
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:			
			OV2659Capture(pImageWindow, pSensorConfigData);
			break;
		case MSDK_SCENARIO_ID_CAMERA_ZSD:			
			OV2659ZSDPreview(pImageWindow, pSensorConfigData);
			break;
		default:
		    break; 
	}
	
	return TRUE;
}



BOOL OV2659_set_param_effect(UINT16 para)
{
	OV2659SENSORDB("[OV2659_set_param_effect]para=%d\n", para);
    switch (para)
    {
        case MEFFECT_OFF:
          	OV2659_write_cmos_sensor(0x3208,0x00);           
            OV2659_write_cmos_sensor(0x5001,0x1f); 
            OV2659_write_cmos_sensor(0x507B,0x06); 
           	OV2659_write_cmos_sensor(0x507e,0x3a); 
            OV2659_write_cmos_sensor(0x507f,0x10);           
           	OV2659_write_cmos_sensor(0x3208,0x10); 
            OV2659_write_cmos_sensor(0x3208,0xa0); 
			break;
        case MEFFECT_SEPIA:
            OV2659_write_cmos_sensor(0x3208,0x00);           
           	OV2659_write_cmos_sensor(0x5001,0x1f); 
           	OV2659_write_cmos_sensor(0x507B,0x1e); 
          	OV2659_write_cmos_sensor(0x507e,0x40); 
           	OV2659_write_cmos_sensor(0x507f,0xa0);           
         	OV2659_write_cmos_sensor(0x3208,0x10); 
           	OV2659_write_cmos_sensor(0x3208,0xa0); 
			break;
        case MEFFECT_NEGATIVE:
          	OV2659_write_cmos_sensor(0x3208,0x00);           
            OV2659_write_cmos_sensor(0x5001,0x1f); 
           	OV2659_write_cmos_sensor(0x507B,0x46);           
            OV2659_write_cmos_sensor(0x3208,0x10); 
          	OV2659_write_cmos_sensor(0x3208,0xa0); 
			break;
        case MEFFECT_SEPIAGREEN:
           	OV2659_write_cmos_sensor(0x3208,0x00);          
           	OV2659_write_cmos_sensor(0x5001,0x1f); 
          	OV2659_write_cmos_sensor(0x507B,0x1e); 
           	OV2659_write_cmos_sensor(0x507e,0x60); 
           	OV2659_write_cmos_sensor(0x507f,0x60);           
           	OV2659_write_cmos_sensor(0x3208,0x10); 
           	OV2659_write_cmos_sensor(0x3208,0xa0); 
            break;
        case MEFFECT_SEPIABLUE:
        	OV2659_write_cmos_sensor(0x3208,0x00);           
           	OV2659_write_cmos_sensor(0x5001,0x1f); 
           	OV2659_write_cmos_sensor(0x507B,0x1e); 
           	OV2659_write_cmos_sensor(0x507e,0xa0); 
         	OV2659_write_cmos_sensor(0x507f,0x40);           
         	OV2659_write_cmos_sensor(0x3208,0x10); 
           	OV2659_write_cmos_sensor(0x3208,0xa0); 
            break;
		case MEFFECT_MONO:
          	OV2659_write_cmos_sensor(0x3208,0x00); 
            OV2659_write_cmos_sensor(0x5001,0x1f); 
           	OV2659_write_cmos_sensor(0x507B,0x26); 
          	OV2659_write_cmos_sensor(0x3208,0x10); 
         	OV2659_write_cmos_sensor(0x3208,0xa0); 
			break;
        default:
            return KAL_FALSE;
    }

    return KAL_TRUE;
} 



BOOL OV2659_set_param_banding(UINT16 para)
{
    kal_uint8 banding;
	kal_uint16 temp_reg = 0;
  	kal_uint32 base_shutter = 0, max_shutter_step = 0, exposure_limitation = 0;
  	kal_uint32 line_length = 0, sensor_pixel_clock = 0;

	OV2659SENSORDB("[OV2659_set_param_banding]para=%d\n", para);	
  
	if (OV2659Sensor.IsPVmode == KAL_TRUE)
	{
		line_length = OV2659_PV_PERIOD_PIXEL_NUMS + OV2659Sensor.PreviewDummyPixels;
		exposure_limitation = OV2659_PV_PERIOD_LINE_NUMS + OV2659Sensor.PreviewDummyLines;
		sensor_pixel_clock = OV2659Sensor.PreviewPclk * 100 * 1000;
	}
	else
	{
		line_length = OV2659_FULL_PERIOD_PIXEL_NUMS + OV2659Sensor.CaptureDummyPixels;
		exposure_limitation = OV2659_FULL_PERIOD_LINE_NUMS + OV2659Sensor.CaptureDummyLines;
		sensor_pixel_clock = OV2659Sensor.CapturePclk * 100 * 1000;
	}

	line_length = line_length * 2;
    banding = OV2659_read_cmos_sensor(0x3A05);
	
    switch (para)
    {
        case AE_FLICKER_MODE_50HZ:
			OV2659_write_cmos_sensor(0x3a05, banding&0x7f);
			break;
        case AE_FLICKER_MODE_60HZ:			
			OV2659_write_cmos_sensor(0x3a05, banding|0x80);
			break;
        default:
			return FALSE;
    }

    return TRUE;
}



BOOL OV2659_set_param_exposure(UINT16 para)
{
    kal_uint8 EvTemp0 = 0x00, EvTemp1 = 0x00, temp_reg= 0x00;

	OV2659SENSORDB("[OV2659_set_param_exposure]para=%d\n", para);

	if (SCENE_MODE_HDR == OV2659Sensor.sceneMode)
   {
       OV2659_set_param_exposure_for_HDR(para);
       return TRUE;
   }
	
	temp_reg=OV2659_read_cmos_sensor(0x5083);
	OV2659_write_cmos_sensor(0x507b,OV2659_read_cmos_sensor(0x507b)|0x04);

    switch (para)
    {	
    	case AE_EV_COMP_20:
			                   EvTemp0= 0x20;
			EvTemp1= temp_reg&0xf7;
			break;
		case AE_EV_COMP_10:
			                   EvTemp0= 0x10;
			EvTemp1= temp_reg&0xf7;
			break;
		case AE_EV_COMP_00:
			EvTemp0= 0x00;
			EvTemp1= temp_reg&0xf7;
			break;
		case AE_EV_COMP_n10:
			                   EvTemp0= 0x10;
			EvTemp1= temp_reg|0x08;	
			break;
        case AE_EV_COMP_n20:
			                   EvTemp0= 0x20;
			EvTemp1= temp_reg|0x08;	
			break;		
        default:
            return FALSE;
    }
    OV2659_write_cmos_sensor(0x3208, 0x00); 
	OV2659_write_cmos_sensor(0x5082, EvTemp0);
	OV2659_write_cmos_sensor(0x5083, EvTemp1);	
    OV2659_write_cmos_sensor(0x3208, 0x10); 
    OV2659_write_cmos_sensor(0x3208, 0xa0); 
    return TRUE;
}



UINT32 OV2659YUVSensorSetting(FEATURE_ID iCmd, UINT32 iPara)
{
	OV2659SENSORDB("[OV2659YUVSensorSetting]icmd=%d, ipara=%d\n", iCmd, iPara);

	switch (iCmd) {
		case FID_SCENE_MODE:
				OV2659_set_scene_mode(iPara); 
	    	break; 	    
		case FID_AWB_MODE: 	    
        	OV2659_set_param_wb(iPara);
			break;
		case FID_COLOR_EFFECT:	    	    
         	OV2659_set_param_effect(iPara);
		 	break;
		case FID_AE_EV:    	    
         	OV2659_set_param_exposure(iPara);
		 	break;
		case FID_AE_FLICKER:    	    	    
         	OV2659_set_param_banding(iPara);
		 	break;
    	case FID_AE_SCENE_MODE: 
         	if (iPara == AE_MODE_OFF) 
				OV2659_set_AE_mode(KAL_FALSE);
         	else 
	         	OV2659_set_AE_mode(KAL_TRUE);
         	break; 
   	 	case FID_ZOOM_FACTOR:	    
         	break; 
		case FID_ISP_CONTRAST:
            OV2659_set_contrast(iPara);
            break;
        case FID_ISP_BRIGHT:
            OV2659_set_brightness(iPara);
            break;
        case FID_ISP_SAT:
            OV2659_set_saturation(iPara);
            break;
		case FID_AE_ISO:
            OV2659_set_iso(iPara);
            break;
		default:
		 	break;
	}
	
	return TRUE;
}



UINT32 OV2659YUVSetVideoMode(UINT16 u2FrameRate)
{
	if (u2FrameRate == 30)
	{
    	OV2659_write_cmos_sensor(0x3003,0x80);//30fps 26mclk 
        OV2659_write_cmos_sensor(0x3004,0x10); 
        OV2659_write_cmos_sensor(0x3005,0x16); 
        OV2659_write_cmos_sensor(0x3006,0x0d); 
        OV2659_write_cmos_sensor(0x380c,0x05); 
        OV2659_write_cmos_sensor(0x380d,0x14); 
        OV2659_write_cmos_sensor(0x380e,0x02); 
        OV2659_write_cmos_sensor(0x380f,0x68);
        OV2659_write_cmos_sensor(0x3a08,0x00); 
        OV2659_write_cmos_sensor(0x3a09,0xb9); 
        OV2659_write_cmos_sensor(0x3a0e,0x03); 
        OV2659_write_cmos_sensor(0x3a0a,0x00); 
        OV2659_write_cmos_sensor(0x3a0b,0x9a); 
        OV2659_write_cmos_sensor(0x3a0d,0x04);  
        OV2659_write_cmos_sensor(0x3a00,0x38); 
        OV2659_write_cmos_sensor(0x3a02,0x02); 
        OV2659_write_cmos_sensor(0x3a03,0x68); 
        OV2659_write_cmos_sensor(0x3a14,0x02); 
        OV2659_write_cmos_sensor(0x3a15,0x68);   
	}
    else if (u2FrameRate == 15)   
	{
    	OV2659_write_cmos_sensor(0x3003,0x80);//15fps 26mclk 
        OV2659_write_cmos_sensor(0x3004,0x20);
        OV2659_write_cmos_sensor(0x3005,0x16); 
        OV2659_write_cmos_sensor(0x3006,0x0d); 
        OV2659_write_cmos_sensor(0x380c,0x05); 
        OV2659_write_cmos_sensor(0x380d,0x14); 
        OV2659_write_cmos_sensor(0x380e,0x02); 
        OV2659_write_cmos_sensor(0x380f,0x68); 
        OV2659_write_cmos_sensor(0x3a08,0x00); 
        OV2659_write_cmos_sensor(0x3a09,0x5c); 
        OV2659_write_cmos_sensor(0x3a0e,0x06); 
        OV2659_write_cmos_sensor(0x3a0a,0x00); 
        OV2659_write_cmos_sensor(0x3a0b,0x4d); 
        OV2659_write_cmos_sensor(0x3a0d,0x08); 
        OV2659_write_cmos_sensor(0x3a00,0x38); 
        OV2659_write_cmos_sensor(0x3a02,0x02); 
        OV2659_write_cmos_sensor(0x3a03,0x68); 
        OV2659_write_cmos_sensor(0x3a14,0x02); 
        OV2659_write_cmos_sensor(0x3a15,0x68);   
	}
    else 
    {
        printk("Wrong frame rate setting \n");
    }   
	mDELAY(30);

    return TRUE;
}



UINT32 OV2659SetMaxFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 frameRate) {
	kal_uint32 pclk;
	kal_int16 dummyLine;
	kal_uint16 lineLength,frameHeight;
		
	switch (scenarioId) 
	{
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			pclk = 480/10;
			lineLength = OV2659_PV_PERIOD_PIXEL_NUMS;
			frameHeight = (10 * pclk)/frameRate/lineLength;
			dummyLine = frameHeight - OV2659_PV_PERIOD_LINE_NUMS;
			OV2659SENSORDB("[OV2659SetMaxFramerateByScenario][preview]framerate=%d, dummy_line=%d\n",frameRate, dummyLine);
			OV2659SetDummy(0, dummyLine);			
			break;			
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			pclk = 480/10;
			lineLength = OV2659_PV_PERIOD_PIXEL_NUMS;
			frameHeight = (10 * pclk)/frameRate/lineLength;
			dummyLine = frameHeight - OV2659_PV_PERIOD_LINE_NUMS;
			OV2659SENSORDB("[OV2659SetMaxFramerateByScenario][video]framerate=%d, dummy_line=%d\n",frameRate, dummyLine);
			OV2659SetDummy(0, dummyLine);			
			break;			
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:			
			pclk = 480/10;
			lineLength = OV2659_FULL_PERIOD_PIXEL_NUMS;
			frameHeight = (10 * pclk)/frameRate/lineLength;
			dummyLine = frameHeight - OV2659_FULL_PERIOD_LINE_NUMS;
			OV2659SENSORDB("[OV2659SetMaxFramerateByScenario][capture/zsd]framerate=%d, dummy_line=%d\n",frameRate, dummyLine);
			OV2659SetDummy(0, dummyLine);			
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



UINT32 OV2659GetDefaultFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 *pframeRate) 
{
	OV2659SENSORDB("[OV2659GetDefaultFramerateByScenario]\n");
	
	switch (scenarioId) 
	{
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



UINT32 OV2659SetTestPatternMode(kal_bool bEnable)
{	
	OV2659SENSORDB("[OV2659SetTestPatternMode]bEnable=%d\n",bEnable);	

	if(bEnable)	
		OV2659_write_cmos_sensor(0x50a0,0x80);		
	else	
		OV2659_write_cmos_sensor(0x50a0,0x00);		

	return ERROR_NONE;
}



void OV2659Set3ACtrl(ACDK_SENSOR_3A_LOCK_ENUM action)
{
	OV2659SENSORDB("[OV2659Set3ACtrl]action=%d\n",action);	

   switch (action)
   {
      case SENSOR_3A_AE_LOCK:
          OV2659_set_AE_mode(KAL_FALSE);
      break;
      case SENSOR_3A_AE_UNLOCK:
          OV2659_set_AE_mode(KAL_TRUE);
      break;
      case SENSOR_3A_AWB_LOCK:
          OV2659_set_AWB_mode(KAL_FALSE);
      break;
      case SENSOR_3A_AWB_UNLOCK:
          OV2659_set_AWB_mode(KAL_TRUE);
      break;
      default:
      	break;
   }

   //fix wb mode for capture -> preview,root casue is ap follow .
   OV2659_set_param_wb(WBcount);
   return;
}



static void OV2659GetCurAeAwbInfo(UINT32 pSensorAEAWBCurStruct)
{
	OV2659SENSORDB("[OV2659GetCurAeAwbInfo]\n");	
	
	PSENSOR_AE_AWB_CUR_STRUCT Info = (PSENSOR_AE_AWB_CUR_STRUCT)pSensorAEAWBCurStruct;
	Info->SensorAECur.AeCurShutter=OV2659ReadShutter();
	Info->SensorAECur.AeCurGain=OV2659ReadSensorGain() * 2;
	Info->SensorAwbGainCur.AwbCurRgain=OV2659_read_cmos_sensor(0x504c);
	Info->SensorAwbGainCur.AwbCurBgain=OV2659_read_cmos_sensor(0x504e);
}



void OV2659_get_AEAWB_lock(UINT32 *pAElockRet32, UINT32 *pAWBlockRet32)
{
	OV2659SENSORDB("[OV2659_get_AEAWB_lock]\n");	
	
	*pAElockRet32 =1;
	*pAWBlockRet32=1;
}



void OV2659_GetDelayInfo(UINT32 delayAddr)
{
	OV2659SENSORDB("[OV2659_GetDelayInfo]\n");	
	
	SENSOR_DELAY_INFO_STRUCT *pDelayInfo=(SENSOR_DELAY_INFO_STRUCT*)delayAddr;
	pDelayInfo->InitDelay=0;
	pDelayInfo->EffectDelay=0;
	pDelayInfo->AwbDelay=0;
	pDelayInfo->AFSwitchDelayFrame=50;
}



void OV2659_AutoTestCmd(UINT32 *cmd,UINT32 *para)
{
	OV2659SENSORDB("[OV2659_AutoTestCmd]\n");
	
	switch(*cmd)
	{
		case YUV_AUTOTEST_SET_SHADDING:
		case YUV_AUTOTEST_SET_GAMMA:
		case YUV_AUTOTEST_SET_AE:
		case YUV_AUTOTEST_SET_SHUTTER:
		case YUV_AUTOTEST_SET_GAIN:
		case YUV_AUTOTEST_GET_SHUTTER_RANGE:
			break;
		default:
			break;	
	}
}



void OV2659GetExifInfo(UINT32 exifAddr)
{
	OV2659SENSORDB("[OV2659_AutoTestCmd]\n");
	
	SENSOR_EXIF_INFO_STRUCT* pExifInfo = (SENSOR_EXIF_INFO_STRUCT*)exifAddr;
    pExifInfo->FNumber = 28;
    pExifInfo->AEISOSpeed = OV2659Sensor.isoSpeed;
    pExifInfo->FlashLightTimeus = 0;
    pExifInfo->RealISOValue = OV2659Sensor.isoSpeed;
	pExifInfo->AWBMode = WBcount;
}



UINT32 OV2659FeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId,
							 UINT8 *pFeaturePara,UINT32 *pFeatureParaLen)
{
	UINT16 *pFeatureReturnPara16=(UINT16 *) pFeaturePara;
	UINT16 *pFeatureData16=(UINT16 *) pFeaturePara;
	UINT32 *pFeatureReturnPara32=(UINT32 *) pFeaturePara;
	UINT32 *pFeatureData32=(UINT32 *) pFeaturePara;
	MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData=(MSDK_SENSOR_CONFIG_STRUCT *) pFeaturePara;
	MSDK_SENSOR_REG_INFO_STRUCT *pSensorRegData=(MSDK_SENSOR_REG_INFO_STRUCT *) pFeaturePara;
	UINT32 Tony_Temp1 = 0;
	UINT32 Tony_Temp2 = 0;
	Tony_Temp1 = pFeaturePara[0];
	Tony_Temp2 = pFeaturePara[1];
	
	switch (FeatureId)
	{
		case SENSOR_FEATURE_GET_RESOLUTION:
			*pFeatureReturnPara16++=OV2659_IMAGE_SENSOR_UVGA_WITDH;
			*pFeatureReturnPara16=OV2659_IMAGE_SENSOR_UVGA_HEIGHT;
			*pFeatureParaLen=4;
			break;
		case SENSOR_FEATURE_GET_PERIOD:
			*pFeatureReturnPara16++=OV2659_PV_PERIOD_PIXEL_NUMS + OV2659Sensor.PreviewDummyPixels;
			*pFeatureReturnPara16=OV2659_PV_PERIOD_LINE_NUMS + OV2659Sensor.PreviewDummyLines;
			*pFeatureParaLen=4;
			break;
		case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
			*pFeatureReturnPara32 = OV2659Sensor.PreviewPclk * 1000 *100;
			*pFeatureParaLen=4;
			break;
		case SENSOR_FEATURE_SET_NIGHTMODE:
			OV2659_night_mode((BOOL) *pFeatureData16);
			break;
		case SENSOR_FEATURE_SET_REGISTER:
			OV2659_write_cmos_sensor(pSensorRegData->RegAddr, pSensorRegData->RegData);
			break;
		case SENSOR_FEATURE_GET_REGISTER:
			pSensorRegData->RegData = OV2659_read_cmos_sensor(pSensorRegData->RegAddr);
			break;
		case SENSOR_FEATURE_GET_CONFIG_PARA:
			memcpy(pSensorConfigData, &OV2659SensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));
			*pFeatureParaLen=sizeof(MSDK_SENSOR_CONFIG_STRUCT);
			break;
		case SENSOR_FEATURE_GET_GROUP_COUNT:
            *pFeatureReturnPara32++=0;
            *pFeatureParaLen=4;	   
		    break; 
		case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
			*pFeatureReturnPara32=LENS_DRIVER_ID_DO_NOT_CARE;
			*pFeatureParaLen=4;
			break;
		case SENSOR_FEATURE_CHECK_SENSOR_ID:
			OV2659_GetSensorID(pFeatureData32);
			break;
		case SENSOR_FEATURE_SET_YUV_CMD:
			OV2659YUVSensorSetting((FEATURE_ID)*pFeatureData32, *(pFeatureData32+1));
			break;
		case SENSOR_FEATURE_SET_VIDEO_MODE:
		    OV2659YUVSetVideoMode(*pFeatureData16);
		    break; 
		case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
			OV2659SetMaxFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32, *(pFeatureData32+1));
			break;
		case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
			OV2659GetDefaultFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32, (MUINT32 *)(*(pFeatureData32+1)));
			break;
		case SENSOR_FEATURE_SET_TEST_PATTERN:            			
			OV2659SetTestPatternMode((BOOL)*pFeatureData16);            			
			break;
		case SENSOR_FEATURE_SET_YUV_3A_CMD:
            OV2659Set3ACtrl((ACDK_SENSOR_3A_LOCK_ENUM)*pFeatureData32);
            break;
		case SENSOR_FEATURE_GET_SHUTTER_GAIN_AWB_GAIN:
			OV2659GetCurAeAwbInfo(*pFeatureData32);			
			break;
		case SENSOR_FEATURE_GET_AE_AWB_LOCK_INFO:
			OV2659_get_AEAWB_lock(*pFeatureData32, *(pFeatureData32+1));
			break;
		case SENSOR_FEATURE_GET_DELAY_INFO:
			OV2659_GetDelayInfo(*pFeatureData32);
			break;
		case SENSOR_FEATURE_AUTOTEST_CMD:
			OV2659_AutoTestCmd(*pFeatureData32,*(pFeatureData32+1));
			break;
		case SENSOR_FEATURE_GET_EXIF_INFO:
            OV2659GetExifInfo(*pFeatureData32);
            break;
		case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
             *pFeatureReturnPara32= OV2659_TEST_PATTERN_CHECKSUM;
             *pFeatureParaLen=4;
             break;
		case SENSOR_FEATURE_SET_GAIN:
		case SENSOR_FEATURE_SET_FLASHLIGHT:
		case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		case SENSOR_FEATURE_SET_ESHUTTER:
		case SENSOR_FEATURE_SET_CCT_REGISTER:
		case SENSOR_FEATURE_GET_CCT_REGISTER:
		case SENSOR_FEATURE_SET_ENG_REGISTER:
		case SENSOR_FEATURE_GET_ENG_REGISTER:
		case SENSOR_FEATURE_GET_REGISTER_DEFAULT:
		case SENSOR_FEATURE_CAMERA_PARA_TO_SENSOR:
		case SENSOR_FEATURE_SENSOR_TO_CAMERA_PARA:
		case SENSOR_FEATURE_GET_GROUP_INFO:
		case SENSOR_FEATURE_GET_ITEM_INFO:
		case SENSOR_FEATURE_SET_ITEM_INFO:
		case SENSOR_FEATURE_GET_ENG_INFO:
			break;
		default:
			break;			
	}
	return ERROR_NONE;
}



SENSOR_FUNCTION_STRUCT	SensorFuncOV2659=
{
	OV2659Open,
	OV2659GetInfo,
	OV2659GetResolution,
	OV2659FeatureControl,
	OV2659Control,
	OV2659Close
};



UINT32 OV2659_YUV_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
	if (pfFunc!=NULL)
		*pfFunc=&SensorFuncOV2659;

	return ERROR_NONE;
}


