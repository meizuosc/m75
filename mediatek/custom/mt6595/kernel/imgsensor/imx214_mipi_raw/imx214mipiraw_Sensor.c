/*******************************************************************************************/
   

/******************************************************************************************/

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include <linux/xlog.h>
#include <asm/system.h>

#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "imx214mipiraw_Sensor.h"
#include "imx214mipiraw_Camera_Sensor_para.h"
#include "imx214mipiraw_CameraCustomized.h"
//#include "AfInit.h"
//#include "AfSTMV.h"
//#include "LC898212AF.h"

static DEFINE_SPINLOCK(imx214mipiraw_drv_lock);

#define IMX214_DEBUG
//#define IMX214_DEBUG_SOFIA

#ifdef IMX214_DEBUG
	#define IMX214DB(fmt, arg...) xlog_printk(ANDROID_LOG_DEBUG, "[IMX214Raw] ",  fmt, ##arg)
#else
	#define IMX214DB(fmt, arg...)
#endif

#ifdef IMX214_DEBUG_SOFIA
	#define IMX214DBSOFIA(fmt, arg...) xlog_printk(ANDROID_LOG_DEBUG, "[IMX214Raw] ",  fmt, ##arg)
#else
	#define IMX214DBSOFIA(fmt, arg...)
#endif

#define mDELAY(ms)  mdelay(ms)

kal_uint32 IMX214_FeatureControl_PERIOD_PixelNum=IMX214_PV_PERIOD_PIXEL_NUMS;
kal_uint32 IMX214_FeatureControl_PERIOD_LineNum=IMX214_PV_PERIOD_LINE_NUMS;

UINT16 VIDEO_MODE_TARGET_FPS = 30;
static BOOL ReEnteyCamera = KAL_FALSE;
kal_bool IMX214DuringTestPattern = KAL_FALSE;
#define IMX214_TEST_PATTERN_CHECKSUM (0x90e95611)


MSDK_SENSOR_CONFIG_STRUCT IMX214SensorConfigData;

kal_uint32 IMX214_FAC_SENSOR_REG;

MSDK_SCENARIO_ID_ENUM IMX214CurrentScenarioId = MSDK_SCENARIO_ID_CAMERA_PREVIEW;

/* FIXME: old factors and DIDNOT use now. s*/
SENSOR_REG_STRUCT IMX214SensorCCT[]=CAMERA_SENSOR_CCT_DEFAULT_VALUE;
SENSOR_REG_STRUCT IMX214SensorReg[ENGINEER_END]=CAMERA_SENSOR_REG_DEFAULT_VALUE;
/* FIXME: old factors and DIDNOT use now. e*/
struct IMX214_SENSOR_STRUCT IMX214_sensor=
{
    .i2c_write_id = 0x20,
    .i2c_read_id  = 0x21,

};

static IMX214_PARA_STRUCT IMX214;
#define IMX214MIPI_MaxGainIndex 159

kal_uint16 IMX214_sensorGainMapping[IMX214MIPI_MaxGainIndex][2] = {
	
	{64 , 1  },
	{65 , 8  },
	{66 , 13 },
	{67 , 23 },
	{68 , 27 },
	{69 , 36 },
	{70 , 41 },
	{71 , 49 },
	{72 , 53 },
	{73 , 61 },
	{74 , 69 },
	{75 , 73 },
	{76 , 80 },
	{77 , 88 },
	{78 , 91 },
	{79 , 98 },
	{80 , 101},
	{81 , 108},
	{82 , 111},
	{83 , 117},
	{84 , 120},
	{85 , 126},
	{86 , 132},
	{87 , 135},
	{88 , 140},
	{89 , 143},
	{90 , 148},
	{91 , 151},
	{92 , 156},
	{93 , 161},
	{94 , 163},
	{95 , 168},
	{96 , 170},
	{97 , 175},
	{98 , 177},
	{99 , 181},
	{100, 185},
	{101, 187},
	{102, 191},
	{103, 193},
	{104, 197},
	{105, 199},
	{106, 203},
	{107, 205},
	{108, 207},
	{109, 212},
	{110, 214},
	{111, 217},
	{112, 219},
	{113, 222},
	{114, 224},
	{115, 227},
	{116, 230},
	{117, 232},
	{118, 234},
	{119, 236},
	{120, 239},
	{122, 244},
	{123, 245},
	{124, 248},
	{125, 249},
	{126, 252},
	{127, 253},
	{128, 256},
	{129, 258},
	{130, 260},
	{131, 262},
	{132, 263},
	{133, 266},
	{134, 268},
	{136, 272},
	{138, 274},
	{139, 276},
	{140, 278},
	{141, 280},
	{143, 282},
	{144, 284},
	{145, 286},
	{147, 288},
	{148, 290},
	{149, 292},
	{150, 294},
	{152, 296},
	{153, 298},
	{155, 300},
	{156, 302},
	{157, 304},
	{159, 306},
	{161, 308},
	{162, 310},
	{164, 312},
	{166, 314},
	{167, 316},
	{169, 318},
	{171, 320},
	{172, 322},
	{174, 324},
	{176, 326},
	{178, 328},
	{180, 330},
	{182, 332},
	{184, 334},
	{186, 336},
	{188, 338},
	{191, 340},
	{193, 342},
	{195, 344},
	{197, 346},
	{200, 348},
	{202, 350},
	{205, 352},
	{207, 354},
	{210, 356},
	{212, 358},
	{216, 360},
	{218, 362},
	{221, 364},
	{225, 366},
	{228, 368},
	{231, 370},
	{234, 372},
	{237, 374},
	{241, 376},
	{244, 378},
	{248, 380},
	{252, 382},
	{256, 384},
	{260, 386},
	{264, 388},
	{269, 390},
	{273, 392},
	{278, 394},
	{282, 396},
	{287, 398},
	{292, 400},
	{298, 402},
	{303, 404},
	{309, 406},
	{315, 408},
	{321, 410},
	{328, 412},
	{334, 414},
	{341, 416},
	{349, 418},
	{356, 420},
	{364, 422},
	{372, 424},
	{381, 426},
	{390, 428},
	{399, 430},
	{410, 432},
	{420, 434},
	{431, 436},
	{443, 438},
	{455, 440},
	{468, 442},
	{482, 444},
	{497, 446},
	{512, 448}	
};
extern int iReadReg(u16 a_u2Addr , u8 * a_puBuff , u16 i2cId);
extern int iWriteReg(u16 a_u2Addr , u32 a_u4Data , u32 a_u4Bytes , u16 i2cId);
#define IMX214_write_cmos_sensor(addr, para) iWriteReg((u16) addr , (u32) para , 1, IMX214MIPI_WRITE_ID)

kal_uint16 IMX214_read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte=0;
    iReadReg((u16) addr ,(u8*)&get_byte,IMX214MIPI_WRITE_ID);
    return get_byte;
}

#define Sleep(ms) mdelay(ms)


/*******************************************************************************
*
********************************************************************************/
kal_uint16 read_IMX214MIPI_gain(void)
{
}
static kal_uint16 IMX214MIPIReg2Gain(const kal_uint8 iReg)
{
	kal_uint8 iI;

    // Range: 1x to 8x
    for (iI = 0; iI < IMX214MIPI_MaxGainIndex; iI++) {
	  if(iReg<= IMX214_sensorGainMapping[iI][1]){
		break;
	   }
     }
    return IMX214_sensorGainMapping[iI][0];    
}
	
static kal_uint16 IMX214MIPIGain2Reg(kal_uint16 iGain)
{
	IMX214DB("[IMX214MIPIGain2Reg1] iGain is :%d \n", iGain);
	kal_uint8 iI;

	for (iI = 0; iI < (IMX214MIPI_MaxGainIndex-1); iI++) {
			if(iGain <= IMX214_sensorGainMapping[iI][0]){	
				break;
			}
		}
       if(iGain!= IMX214_sensorGainMapping[iI][0])
		{
			 printk("[IMX214MIPIGain2Reg] Gain mapping don't correctly:%d %d \n", iGain, IMX214_sensorGainMapping[iI][0]);
		}
    IMX214DB("[IMX214MIPIGain2Reg1] iI :%d \n", iI);
	return IMX214_sensorGainMapping[iI][1];

}

/*************************************************************************
* FUNCTION
*    IMX214MIPI_SetGain
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

void IMX214MIPI_SetGain(UINT16 iGain)
{
	IMX214DB("[IMX214MIPI_SetGain] iGain is :%d \n ",iGain);
	kal_uint16 iReg;
	
    iReg=IMX214MIPIGain2Reg(iGain);
	printk("Gain2Reg:%d\n",  iReg);
	
    IMX214_write_cmos_sensor(0x0104, 1);
    IMX214_write_cmos_sensor(0x0204, (iReg>>8)& 0xFF);
	IMX214_write_cmos_sensor(0x0205, iReg & 0xFF);
    IMX214_write_cmos_sensor(0x0104, 0);
    iReg=IMX214_read_cmos_sensor(0x205);
	printk("checkReg:%d\n",  iReg);

}   /*  IMX214MIPI_SetGain  */

static void IMX214_SetDummy( const kal_uint32 iPixels, const kal_uint32 iLines )
{
    //return;
	kal_uint32 line_length = 0;
	kal_uint32 frame_length = 0;

	if ( SENSOR_MODE_PREVIEW == IMX214.sensorMode )	//SXGA size output
	{
		line_length = IMX214_PV_PERIOD_PIXEL_NUMS + iPixels;
		frame_length = IMX214_PV_PERIOD_LINE_NUMS + iLines;
	}
	else if( SENSOR_MODE_VIDEO== IMX214.sensorMode )
	{
		line_length = IMX214_VIDEO_PERIOD_PIXEL_NUMS + iPixels;
		frame_length = IMX214_VIDEO_PERIOD_LINE_NUMS + iLines;
	}
	else if( SENSOR_MODE_CAPTURE== IMX214.sensorMode )
	{
		line_length = IMX214_FULL_PERIOD_PIXEL_NUMS + iPixels;
		frame_length = IMX214_FULL_PERIOD_LINE_NUMS + iLines;
	}

	spin_lock(&imx214mipiraw_drv_lock);
	IMX214_FeatureControl_PERIOD_PixelNum = line_length;
	IMX214_FeatureControl_PERIOD_LineNum = frame_length;
	spin_unlock(&imx214mipiraw_drv_lock);

	//Set total line length
	IMX214_write_cmos_sensor(0x0104, 1); 
	IMX214_write_cmos_sensor(0x0340, (frame_length >>8) & 0xFF);
	IMX214_write_cmos_sensor(0x0341, frame_length& 0xFF);
	IMX214_write_cmos_sensor(0x0342, (line_length >>8) & 0xFF);
	IMX214_write_cmos_sensor(0x0343, line_length& 0xFF);
	IMX214_write_cmos_sensor(0x0104, 0); 

	IMX214DB("[IMX214MIPI_SetDummy] frame_length is :%d \n ",frame_length);
	IMX214DB("[IMX214MIPI_SetDummy] line_lengthis :%d \n ",line_length);

}   /*  IMX214_SetDummy */

static void IMX214_Sensor_Init(void)
{

	  IMX214DB("IMX214_Sensor_Init 4lane:\n ");			
	  //Global	setting
	  
	  IMX214_write_cmos_sensor(0x4601,0x00);
	  IMX214_write_cmos_sensor(0x4642,0x05);
	  IMX214_write_cmos_sensor(0x6276,0x00);
	  IMX214_write_cmos_sensor(0x900E,0x05);
	  IMX214_write_cmos_sensor(0xA802,0x90);
	  IMX214_write_cmos_sensor(0xA803,0x11);
	  IMX214_write_cmos_sensor(0xA804,0x62);
	  IMX214_write_cmos_sensor(0xA805,0x77);
	  IMX214_write_cmos_sensor(0xA806,0xAE);
	  IMX214_write_cmos_sensor(0xA807,0x34);
	  IMX214_write_cmos_sensor(0xA808,0xAE);
	  IMX214_write_cmos_sensor(0xA809,0x35);
	  IMX214_write_cmos_sensor(0xAE33,0x00);
	  
	  IMX214_write_cmos_sensor(0x4612,0x29);
	  IMX214_write_cmos_sensor(0x461B,0x12);
	  IMX214_write_cmos_sensor(0x4637,0x30);
	  IMX214_write_cmos_sensor(0x463F,0x18);
	  IMX214_write_cmos_sensor(0x4641,0x0D);
	  IMX214_write_cmos_sensor(0x465B,0x12);
	  IMX214_write_cmos_sensor(0x465F,0x11);
	  IMX214_write_cmos_sensor(0x4663,0x11);
	  IMX214_write_cmos_sensor(0x4667,0x0F);
	  IMX214_write_cmos_sensor(0x466F,0x0F);
	  IMX214_write_cmos_sensor(0x470E,0x09);
	  IMX214_write_cmos_sensor(0x4915,0x5D);
	  IMX214_write_cmos_sensor(0x4A5F,0xFF);
	  IMX214_write_cmos_sensor(0x4A61,0xFF);
	  IMX214_write_cmos_sensor(0x4A73,0x62);
	  IMX214_write_cmos_sensor(0x4A85,0x00);
	  IMX214_write_cmos_sensor(0x4A87,0xFF);
	  IMX214_write_cmos_sensor(0x620E,0x04);
	  IMX214_write_cmos_sensor(0x6EB2,0x01);
	  IMX214_write_cmos_sensor(0x6EB3,0x00);
	  
	  //HDR Setting
	  IMX214_write_cmos_sensor(0x6D12,0x3F);
	  IMX214_write_cmos_sensor(0x6D13,0xFF);
	  IMX214_write_cmos_sensor(0x9344,0x03);
	  IMX214_write_cmos_sensor(0x3001,0x07);
	  
	  //CNR parameter setting
	  IMX214_write_cmos_sensor(0x69DB,0x01);
	  IMX214_write_cmos_sensor(0x974C,0x00);
	  IMX214_write_cmos_sensor(0x974D,0x40);
	  IMX214_write_cmos_sensor(0x974E,0x00);
	  IMX214_write_cmos_sensor(0x974F,0x40);
	  IMX214_write_cmos_sensor(0x9750,0x00);
	  IMX214_write_cmos_sensor(0x9751,0x40);
	  
	  //Moir¨¦ reduction Parameter Setting
	  IMX214_write_cmos_sensor(0x6957,0x01);
	  
	  //Image enhancement Setting
	  IMX214_write_cmos_sensor(0x6987,0x17);
	  IMX214_write_cmos_sensor(0x698A,0x03);
	  IMX214_write_cmos_sensor(0x698B,0x03);

      IMX214DB("IMX214_Sensor_Init exit :\n ");
	
}   /*  IMX214_Sensor_Init  */

void IMX214PreviewSetting(void)
{
    IMX214DB("IMX214Preview setting:");
    IMX214DB("Func_RSE1616x1212_30FPS_MCLK24_4LANE_RAW10");
    	
    //1/2 Binning setting
    IMX214_write_cmos_sensor(0x0100,0x00);
	
    IMX214_write_cmos_sensor(0x0114,0x03);
    IMX214_write_cmos_sensor(0x0220,0x00);
    IMX214_write_cmos_sensor(0x0221,0x11);
    IMX214_write_cmos_sensor(0x0222,0x01);
    IMX214_write_cmos_sensor(0x0340,0x09);
    IMX214_write_cmos_sensor(0x0341,0xE6);
    IMX214_write_cmos_sensor(0x0342,0x13);
    IMX214_write_cmos_sensor(0x0343,0x90);
    IMX214_write_cmos_sensor(0x0344,0x00);
    IMX214_write_cmos_sensor(0x0345,0x00);
    IMX214_write_cmos_sensor(0x0346,0x00);
    IMX214_write_cmos_sensor(0x0347,0x00);
    IMX214_write_cmos_sensor(0x0348,0x10);
    IMX214_write_cmos_sensor(0x0349,0x6F);
    IMX214_write_cmos_sensor(0x034A,0x0C);
    IMX214_write_cmos_sensor(0x034B,0x2F);
    IMX214_write_cmos_sensor(0x0381,0x01);
    IMX214_write_cmos_sensor(0x0383,0x01);
    IMX214_write_cmos_sensor(0x0385,0x01);
    IMX214_write_cmos_sensor(0x0387,0x01);
    IMX214_write_cmos_sensor(0x0900,0x01);
    IMX214_write_cmos_sensor(0x0901,0x22);
    IMX214_write_cmos_sensor(0x0902,0x00);
    IMX214_write_cmos_sensor(0x3000,0x3D);
    IMX214_write_cmos_sensor(0x3054,0x01);
    IMX214_write_cmos_sensor(0x305C,0x11);

    IMX214_write_cmos_sensor(0x0112,0x0A);
    IMX214_write_cmos_sensor(0x0113,0x0A);
    IMX214_write_cmos_sensor(0x034C,0x08);
    IMX214_write_cmos_sensor(0x034D,0x38);
    IMX214_write_cmos_sensor(0x034E,0x06);
    IMX214_write_cmos_sensor(0x034F,0x18);
    IMX214_write_cmos_sensor(0x0401,0x00);
    IMX214_write_cmos_sensor(0x0404,0x00);
    IMX214_write_cmos_sensor(0x0405,0x10);
    IMX214_write_cmos_sensor(0x0408,0x00);
    IMX214_write_cmos_sensor(0x0409,0x00);
    IMX214_write_cmos_sensor(0x040A,0x00);
    IMX214_write_cmos_sensor(0x040B,0x00);
    IMX214_write_cmos_sensor(0x040C,0x08);
    IMX214_write_cmos_sensor(0x040D,0x38);
    IMX214_write_cmos_sensor(0x040E,0x06);
    IMX214_write_cmos_sensor(0x040F,0x18);

    IMX214_write_cmos_sensor(0x0301,0x05);
    IMX214_write_cmos_sensor(0x0303,0x02);
    IMX214_write_cmos_sensor(0x0305,0x03);
    IMX214_write_cmos_sensor(0x0306,0x00);
    IMX214_write_cmos_sensor(0x0307,0x77);
    IMX214_write_cmos_sensor(0x0309,0x0A);
    IMX214_write_cmos_sensor(0x030B,0x01);
	
    IMX214_write_cmos_sensor(0x0310,0x00);

    IMX214_write_cmos_sensor(0x0820,0x0E);
    IMX214_write_cmos_sensor(0x0821,0xE0);
    IMX214_write_cmos_sensor(0x0822,0x00);
    IMX214_write_cmos_sensor(0x0823,0x00);

    IMX214_write_cmos_sensor(0x3A03,0x08);
    IMX214_write_cmos_sensor(0x3A04,0x20);

    IMX214_write_cmos_sensor(0x0B06,0x01);

    IMX214_write_cmos_sensor(0x30A2,0x00);
    IMX214_write_cmos_sensor(0x30B4,0x00);

    IMX214_write_cmos_sensor(0x3A02,0xFF);

    IMX214_write_cmos_sensor(0x3013,0x00);
    IMX214_write_cmos_sensor(0x5062,0x08);
    IMX214_write_cmos_sensor(0x5063,0x38);
    IMX214_write_cmos_sensor(0x5064,0x00);

    //IMX214_write_cmos_sensor(0x0202,0x09);
    //IMX214_write_cmos_sensor(0x0203,0xDC);
    IMX214_write_cmos_sensor(0x0224,0x01);
    IMX214_write_cmos_sensor(0x0225,0xF4);

    //IMX214_write_cmos_sensor(0x0204,0x00);
    //IMX214_write_cmos_sensor(0x0205,0x00);
    IMX214_write_cmos_sensor(0x020E,0x01);
    IMX214_write_cmos_sensor(0x020F,0x00);
    IMX214_write_cmos_sensor(0x0210,0x01);
    IMX214_write_cmos_sensor(0x0211,0x00);
    IMX214_write_cmos_sensor(0x0212,0x01);
    IMX214_write_cmos_sensor(0x0213,0x00);
    IMX214_write_cmos_sensor(0x0214,0x01);
    IMX214_write_cmos_sensor(0x0215,0x00);
    IMX214_write_cmos_sensor(0x0216,0x00);
    IMX214_write_cmos_sensor(0x0217,0x00);
	
    IMX214_write_cmos_sensor(0x0136,0x18);
	IMX214_write_cmos_sensor(0x0137,0x00);
	
    IMX214_write_cmos_sensor(0x0100,0x01);

}


void IMX214VideoSetting(void)
{
	IMX214DB("IMX214VideoSetting_16:9 exit :\n ");
	    IMX214_write_cmos_sensor(0x0100,0x00);
		
		IMX214_write_cmos_sensor(0x0114,0x03);
		IMX214_write_cmos_sensor(0x0220,0x00);
		IMX214_write_cmos_sensor(0x0221,0x11);
		IMX214_write_cmos_sensor(0x0222,0x01);
		IMX214_write_cmos_sensor(0x0340,0x09);
		IMX214_write_cmos_sensor(0x0341,0xE6);
		IMX214_write_cmos_sensor(0x0342,0x13);
		IMX214_write_cmos_sensor(0x0343,0x90);
		IMX214_write_cmos_sensor(0x0344,0x00);
		IMX214_write_cmos_sensor(0x0345,0x00);
		IMX214_write_cmos_sensor(0x0346,0x01);
		IMX214_write_cmos_sensor(0x0347,0x78);
		IMX214_write_cmos_sensor(0x0348,0x10);
		IMX214_write_cmos_sensor(0x0349,0x6F);
		IMX214_write_cmos_sensor(0x034A,0x0A);
		IMX214_write_cmos_sensor(0x034B,0xB7);
		IMX214_write_cmos_sensor(0x0381,0x01);
		IMX214_write_cmos_sensor(0x0383,0x01);
		IMX214_write_cmos_sensor(0x0385,0x01);
		IMX214_write_cmos_sensor(0x0387,0x01);
		IMX214_write_cmos_sensor(0x0900,0x01);
		IMX214_write_cmos_sensor(0x0901,0x22);
		IMX214_write_cmos_sensor(0x0902,0x00);
		IMX214_write_cmos_sensor(0x3000,0x3D);
		IMX214_write_cmos_sensor(0x3054,0x01);
		IMX214_write_cmos_sensor(0x305C,0x11);
	
		IMX214_write_cmos_sensor(0x0112,0x0A);
		IMX214_write_cmos_sensor(0x0113,0x0A);
		IMX214_write_cmos_sensor(0x034C,0x08);
		IMX214_write_cmos_sensor(0x034D,0x38);
		IMX214_write_cmos_sensor(0x034E,0x04);
		IMX214_write_cmos_sensor(0x034F,0xA0);
		IMX214_write_cmos_sensor(0x0401,0x00);
		IMX214_write_cmos_sensor(0x0404,0x00);
		IMX214_write_cmos_sensor(0x0405,0x10);
		IMX214_write_cmos_sensor(0x0408,0x00);
		IMX214_write_cmos_sensor(0x0409,0x00);
		IMX214_write_cmos_sensor(0x040A,0x00);
		IMX214_write_cmos_sensor(0x040B,0x00);
		IMX214_write_cmos_sensor(0x040C,0x08);
		IMX214_write_cmos_sensor(0x040D,0x38);
		IMX214_write_cmos_sensor(0x040E,0x04);
		IMX214_write_cmos_sensor(0x040F,0xA0);
	
		IMX214_write_cmos_sensor(0x0301,0x05);
		IMX214_write_cmos_sensor(0x0303,0x02);
		IMX214_write_cmos_sensor(0x0305,0x03);
		IMX214_write_cmos_sensor(0x0306,0x00);
		IMX214_write_cmos_sensor(0x0307,0x77);
		IMX214_write_cmos_sensor(0x0309,0x0A);
		IMX214_write_cmos_sensor(0x030B,0x01);
		
		IMX214_write_cmos_sensor(0x0310,0x00);
	
		IMX214_write_cmos_sensor(0x0820,0x0E);
		IMX214_write_cmos_sensor(0x0821,0xE0);
		IMX214_write_cmos_sensor(0x0822,0x00);
		IMX214_write_cmos_sensor(0x0823,0x00);
	
		IMX214_write_cmos_sensor(0x3A03,0x06);
		IMX214_write_cmos_sensor(0x3A04,0x68);
	
		IMX214_write_cmos_sensor(0x0B06,0x01);
	
		IMX214_write_cmos_sensor(0x30A2,0x00);
		IMX214_write_cmos_sensor(0x30B4,0x00);
	
		IMX214_write_cmos_sensor(0x3A02,0xFF);
	
		IMX214_write_cmos_sensor(0x3013,0x00);
		IMX214_write_cmos_sensor(0x5062,0x08);
		IMX214_write_cmos_sensor(0x5063,0x38);
		IMX214_write_cmos_sensor(0x5064,0x00);
	
		//IMX214_write_cmos_sensor(0x0202,0x09);
		//IMX214_write_cmos_sensor(0x0203,0xDC);
		IMX214_write_cmos_sensor(0x0224,0x01);
		IMX214_write_cmos_sensor(0x0225,0xF4);
	
		//IMX214_write_cmos_sensor(0x0204,0x00);
		//IMX214_write_cmos_sensor(0x0205,0x00);
		IMX214_write_cmos_sensor(0x020E,0x01);
		IMX214_write_cmos_sensor(0x020F,0x00);
		IMX214_write_cmos_sensor(0x0210,0x01);
		IMX214_write_cmos_sensor(0x0211,0x00);
		IMX214_write_cmos_sensor(0x0212,0x01);
		IMX214_write_cmos_sensor(0x0213,0x00);
		IMX214_write_cmos_sensor(0x0214,0x01);
		IMX214_write_cmos_sensor(0x0215,0x00);
		IMX214_write_cmos_sensor(0x0216,0x00);
		IMX214_write_cmos_sensor(0x0217,0x00);
		
		IMX214_write_cmos_sensor(0x0136,0x18);
		IMX214_write_cmos_sensor(0x0137,0x00);
		
		IMX214_write_cmos_sensor(0x0100,0x01);
		

}


void IMX214CaptureSetting(void)
{
      IMX214DB("IMX214capture setting:");
      IMX214DB("Func_RSE3264x2448_30FPS_MCLK24_4LANE_RAW10");
	  //Full size capture
	  IMX214_write_cmos_sensor(0x0100,0x00);
	  IMX214_write_cmos_sensor(0x0114,0x03);
	  IMX214_write_cmos_sensor(0x0220,0x00);
	  IMX214_write_cmos_sensor(0x0221,0x11);
	  IMX214_write_cmos_sensor(0x0222,0x01);
	  IMX214_write_cmos_sensor(0x0340,0x0C);
	  IMX214_write_cmos_sensor(0x0341,0x60);
	  IMX214_write_cmos_sensor(0x0342,0x13);
	  IMX214_write_cmos_sensor(0x0343,0x90);
	  IMX214_write_cmos_sensor(0x0344,0x00);
	  IMX214_write_cmos_sensor(0x0345,0x00);
	  IMX214_write_cmos_sensor(0x0346,0x00);
	  IMX214_write_cmos_sensor(0x0347,0x00);
	  IMX214_write_cmos_sensor(0x0348,0x10);
	  IMX214_write_cmos_sensor(0x0349,0x6F);
	  IMX214_write_cmos_sensor(0x034A,0x0C);
	  IMX214_write_cmos_sensor(0x034B,0x2F);
	  IMX214_write_cmos_sensor(0x0381,0x01);
	  IMX214_write_cmos_sensor(0x0383,0x01);
	  IMX214_write_cmos_sensor(0x0385,0x01);
	  IMX214_write_cmos_sensor(0x0387,0x01);
	  IMX214_write_cmos_sensor(0x0900,0x00);
	  IMX214_write_cmos_sensor(0x0901,0x00);
	  IMX214_write_cmos_sensor(0x0902,0x00);
	  IMX214_write_cmos_sensor(0x3000,0x3D);
	  IMX214_write_cmos_sensor(0x3054,0x01);
	  IMX214_write_cmos_sensor(0x305C,0x11);
	  
	  IMX214_write_cmos_sensor(0x0112,0x0A);
	  IMX214_write_cmos_sensor(0x0113,0x0A);
	  IMX214_write_cmos_sensor(0x034C,0x10);
	  IMX214_write_cmos_sensor(0x034D,0x70);
	  IMX214_write_cmos_sensor(0x034E,0x0C);
	  IMX214_write_cmos_sensor(0x034F,0x30);
	  IMX214_write_cmos_sensor(0x0401,0x00);
	  IMX214_write_cmos_sensor(0x0404,0x00);
	  IMX214_write_cmos_sensor(0x0405,0x10);
	  IMX214_write_cmos_sensor(0x0408,0x00);
	  IMX214_write_cmos_sensor(0x0409,0x00);
	  IMX214_write_cmos_sensor(0x040A,0x00);
	  IMX214_write_cmos_sensor(0x040B,0x00);
	  IMX214_write_cmos_sensor(0x040C,0x10);
	  IMX214_write_cmos_sensor(0x040D,0x70);
	  IMX214_write_cmos_sensor(0x040E,0x0C);
	  IMX214_write_cmos_sensor(0x040F,0x30);
	  
	  IMX214_write_cmos_sensor(0x0301,0x05);
	  IMX214_write_cmos_sensor(0x0303,0x02);
	  IMX214_write_cmos_sensor(0x0305,0x03);
	  IMX214_write_cmos_sensor(0x0306,0x00);
	  IMX214_write_cmos_sensor(0x0307,0x77);
	  IMX214_write_cmos_sensor(0x0309,0x0A);
	  IMX214_write_cmos_sensor(0x030B,0x01);
	  IMX214_write_cmos_sensor(0x0310,0x00);
	  
	  IMX214_write_cmos_sensor(0x0820,0x0E);
	  IMX214_write_cmos_sensor(0x0821,0xE0);
	  IMX214_write_cmos_sensor(0x0822,0x00);
	  IMX214_write_cmos_sensor(0x0823,0x00);
	  
	  IMX214_write_cmos_sensor(0x3A03,0x08);
	  IMX214_write_cmos_sensor(0x3A04,0x20);
	  
	  IMX214_write_cmos_sensor(0x0B06,0x01);
	  
	  IMX214_write_cmos_sensor(0x30A2,0x00);
	  IMX214_write_cmos_sensor(0x30B4,0x00);
	  
	  IMX214_write_cmos_sensor(0x3A02,0xFF);
	  
	  IMX214_write_cmos_sensor(0x3013,0x00);
	  IMX214_write_cmos_sensor(0x5062,0x10);
	  IMX214_write_cmos_sensor(0x5063,0x70);
	  IMX214_write_cmos_sensor(0x5064,0x00);
	  
	  //IMX214_write_cmos_sensor(0x0202,0x0C);
	  //IMX214_write_cmos_sensor(0x0203,0x56);
	  IMX214_write_cmos_sensor(0x0224,0x01);
	  IMX214_write_cmos_sensor(0x0225,0xF4);
	  
	  //IMX214_write_cmos_sensor(0x0204,0x00);
	  //IMX214_write_cmos_sensor(0x0205,0x00);
	  IMX214_write_cmos_sensor(0x020E,0x01);
	  IMX214_write_cmos_sensor(0x020F,0x00);
	  IMX214_write_cmos_sensor(0x0210,0x01);
	  IMX214_write_cmos_sensor(0x0211,0x00);
	  IMX214_write_cmos_sensor(0x0212,0x01);
	  IMX214_write_cmos_sensor(0x0213,0x00);
	  IMX214_write_cmos_sensor(0x0214,0x01);
	  IMX214_write_cmos_sensor(0x0215,0x00);
	  IMX214_write_cmos_sensor(0x0216,0x00);
	  IMX214_write_cmos_sensor(0x0217,0x00);
	  
	  IMX214_write_cmos_sensor(0x0136,0x18);
	  IMX214_write_cmos_sensor(0x0137,0x00);
	  
	  IMX214_write_cmos_sensor(0x0100,0x01);


}

/*************************************************************************
* FUNCTION
*   IMX214Open
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
/*======================LC898212AF=============================

extern	void StmvSet( stSmvPar ) ;
extern  void AfInit( unsigned char hall_bias, unsigned char hall_off );
extern  void ServoOn(void);


#define		REG_ADDR_START	  	0x80		// REG Start address

void LC898212_Init()
{
    stSmvPar StSmvPar;

    int HallOff = 0x75;	 	// Please Read Offset from EEPROM or OTP
    int HallBiase = 0x2E;   // Please Read Bias from EEPROM or OTP
    
	AfInit( HallOff, HallBiase );	// Initialize driver IC

    // Step move parameter set
    StSmvPar.UsSmvSiz	= STMV_SIZE ;
	StSmvPar.UcSmvItv	= STMV_INTERVAL ;
	StSmvPar.UcSmvEnb	= STMCHTG_SET | STMSV_SET | STMLFF_SET ;
	StmvSet( StSmvPar ) ;
	
	ServoOn();	// Close loop ON
	
}
=========================LC898212AF========================*/

UINT32 IMX214Open(void)
{

	volatile signed int i;
	kal_uint16 sensor_id = 0;

	IMX214DB("IMX214Open enter :\n ");

	//  Read sensor ID to adjust I2C is OK?
	for(i=0;i<3;i++)
	{
		sensor_id =(IMX214_read_cmos_sensor(0x0016)<<8)|IMX214_read_cmos_sensor(0x0017);
		IMX214DB("OIMX214 READ ID :%x",sensor_id);
		if(sensor_id != IMX214_SENSOR_ID)
		{
			return ERROR_NONE;
		}else
			break;
	}

	spin_lock(&imx214mipiraw_drv_lock);
	IMX214.sensorMode = SENSOR_MODE_INIT;
	IMX214.IMX214AutoFlickerMode = KAL_FALSE;
	IMX214.IMX214VideoMode = KAL_FALSE;
	spin_unlock(&imx214mipiraw_drv_lock);
	IMX214_Sensor_Init();
    //LC898212_Init();
	spin_lock(&imx214mipiraw_drv_lock);
	IMX214.DummyLines= 0;
	IMX214.DummyPixels= 0;
	IMX214.pvPclk =  (3808); 
	IMX214.videoPclk = (3808);
	IMX214.capPclk = (3808);
	
	IMX214.shutter = 0x4EA;
	IMX214.pvShutter = 0x4EA;
	IMX214.maxExposureLines =IMX214_PV_PERIOD_LINE_NUMS;

	IMX214.ispBaseGain = BASEGAIN;//0x40
	IMX214.sensorGlobalGain = 0x1000;//sensor gain 1x
	IMX214.pvGain = 0x1000;
	IMX214DuringTestPattern = KAL_FALSE;
	
	spin_unlock(&imx214mipiraw_drv_lock);
	IMX214DB("IMX214Open exit :\n ");

    return ERROR_NONE;
}

/*************************************************************************
* FUNCTION
*   IMX214GetSensorID
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
UINT32 IMX214GetSensorID(UINT32 *sensorID)
{
    int  retry = 1;

	IMX214DB("IMX214GetSensorID enter :\n ");

    // check if sensor ID correct
    do {
        *sensorID =(IMX214_read_cmos_sensor(0x0016)<<8)|IMX214_read_cmos_sensor(0x0017);
		IMX214DB("REG0016= 0x%04x\n", IMX214_read_cmos_sensor(0x0016));
		IMX214DB("REG0017 = 0x%04x\n", IMX214_read_cmos_sensor(0x0017));
        if (*sensorID == IMX214_SENSOR_ID)
        	{
        		IMX214DB("Sensor ID = 0x%04x\n", *sensorID);
            	break;
        	}
        IMX214DB("Read Sensor ID Fail = 0x%04x\n", *sensorID);
        retry--;
    } while (retry > 0);

    if (*sensorID != IMX214_SENSOR_ID) {
        *sensorID = 0x0214;
        return ERROR_NONE;
    }
    return ERROR_NONE;
}


/*************************************************************************
* FUNCTION
*   IMX214_SetShutter
*
* DESCRIPTION
*   This function set e-shutter of IMX214 to change exposure time.
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
void IMX214_write_shutter(kal_uint16 shutter)
{
	IMX214DB("IMX214MIPI_Write_Shutter =%d \n ",shutter);

	IMX214_write_cmos_sensor(0x0104, 1);      	
   
    IMX214_write_cmos_sensor(0x0202, (shutter >> 8) & 0xFF);
    IMX214_write_cmos_sensor(0x0203, shutter  & 0xFF);
	
    IMX214_write_cmos_sensor(0x0104, 0);   

}	/* write_IMX214_shutter */

void IMX214_SetShutter(kal_uint16 iShutter)
{

   IMX214DB("IMX214MIPI_SetShutter =%d \n ",iShutter);
   //iShutter=0x013c;
 
   kal_uint16 frame_length=0;
   if (iShutter < 1)
	   iShutter = 1; 
   
   IMX214DB("IMX214.sensorMode=%d \n ",IMX214.sensorMode);
  
   if ( SENSOR_MODE_PREVIEW== IMX214.sensorMode ) 
   {
	   frame_length = IMX214_PV_PERIOD_LINE_NUMS+IMX214.DummyLines;
   }
   else if( SENSOR_MODE_VIDEO== IMX214.sensorMode )
   {
	   frame_length = IMX214_VIDEO_PERIOD_LINE_NUMS+IMX214.DummyLines;
   }
   else if( SENSOR_MODE_CAPTURE== IMX214.sensorMode)
   {
	   frame_length = IMX214_FULL_PERIOD_LINE_NUMS + IMX214.DummyLines;
   }
   
   IMX214DB("IMX214_frame_length=%d \n ",frame_length);
   
   if(iShutter>frame_length)
   	{   
   	    frame_length=iShutter+10;
	   	IMX214_write_cmos_sensor(0x0104, 1); 
	    IMX214_write_cmos_sensor(0x0340, (frame_length >>8) & 0xFF);
        IMX214_write_cmos_sensor(0x0341, frame_length& 0xFF);	        
        IMX214_write_cmos_sensor(0x0104, 0);   
   	}
   else
   	{
   		IMX214_write_cmos_sensor(0x0104, 1); 
	    IMX214_write_cmos_sensor(0x0340, (frame_length >>8) & 0xFF);
        IMX214_write_cmos_sensor(0x0341, frame_length& 0xFF);	        
        IMX214_write_cmos_sensor(0x0104, 0);   
   	}
   spin_lock(&imx214mipiraw_drv_lock);
   IMX214.shutter= iShutter;
   if(iShutter>frame_length)
     IMX214_FeatureControl_PERIOD_LineNum=iShutter;
   spin_unlock(&imx214mipiraw_drv_lock);
   
   IMX214_write_shutter(iShutter);

	  
   return;
}   /*  IMX214_SetShutter   */


/*************************************************************************
* FUNCTION
*   IMX214_read_shutter
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
UINT32 IMX214_read_shutter(void)
{

    kal_uint16 ishutter;

    ishutter = IMX214_read_cmos_sensor(0x0202); /* course_integration_time */

    IMX214DB("IMX214_read_shutter (0x%x)\n",ishutter);

    return ishutter;

}

/*************************************************************************
* FUNCTION
*   IMX214_night_mode
*
* DESCRIPTION
*   This function night mode of IMX214.
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
void IMX214_NightMode(kal_bool bEnable)
{
}/*	IMX214_NightMode */



/*************************************************************************
* FUNCTION
*   IMX214Close
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
UINT32 IMX214Close(void)
{
    ReEnteyCamera = KAL_FALSE;
    return ERROR_NONE;
}	/* IMX214Close() */

void IMX214SetFlipMirror(kal_int32 imgMirror)
{

}


/*************************************************************************
* FUNCTION
*   IMX214Preview
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
UINT32 IMX214Preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                                                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	IMX214DB("IMX214Preview enter:");

	// preview size
	if(IMX214.sensorMode == SENSOR_MODE_PREVIEW)
	{
		// do nothing
		// FOR CCT PREVIEW
	}
	else
	{
		IMX214DB("IMX214Preview setting!!\n");
		IMX214PreviewSetting();
	}
	
	spin_lock(&imx214mipiraw_drv_lock);
	IMX214.sensorMode = SENSOR_MODE_PREVIEW; // Need set preview setting after capture mode
	IMX214.DummyPixels = 0;//define dummy pixels and lines
	IMX214.DummyLines = 0 ;
	IMX214_FeatureControl_PERIOD_PixelNum=IMX214_PV_PERIOD_PIXEL_NUMS+ IMX214.DummyPixels;
	IMX214_FeatureControl_PERIOD_LineNum=IMX214_PV_PERIOD_LINE_NUMS+IMX214.DummyLines;
	spin_unlock(&imx214mipiraw_drv_lock);


	//set mirror & flip
	IMX214DB("[IMX214Preview] mirror&flip: %d \n",sensor_config_data->SensorImageMirror);
	spin_lock(&imx214mipiraw_drv_lock);
	IMX214.imgMirror = sensor_config_data->SensorImageMirror;
	spin_unlock(&imx214mipiraw_drv_lock);
	//IMX214SetFlipMirror(sensor_config_data->SensorImageMirror);

	IMX214DBSOFIA("[IMX214Preview]frame_len=%x\n", (IMX214_read_cmos_sensor(0x0340)<<8)|IMX214_read_cmos_sensor(0x0341));

    mDELAY(40);
	IMX214DB("IMX214Preview exit:\n");
    return ERROR_NONE;
}	/* IMX214Preview() */



UINT32 IMX214Video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                                                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	IMX214DB("IMX214Video enter:");

	if(IMX214.sensorMode == SENSOR_MODE_VIDEO)
	{
		// do nothing
	}
	else
	{
		IMX214VideoSetting();

	}
	spin_lock(&imx214mipiraw_drv_lock);
	IMX214.sensorMode = SENSOR_MODE_VIDEO;
	IMX214.DummyPixels = 0;//define dummy pixels and lines
	IMX214.DummyLines = 0 ;
	IMX214_FeatureControl_PERIOD_PixelNum=IMX214_VIDEO_PERIOD_PIXEL_NUMS+ IMX214.DummyPixels;
	IMX214_FeatureControl_PERIOD_LineNum=IMX214_VIDEO_PERIOD_LINE_NUMS+IMX214.DummyLines;
	spin_unlock(&imx214mipiraw_drv_lock);


	spin_lock(&imx214mipiraw_drv_lock);
	IMX214.imgMirror = sensor_config_data->SensorImageMirror;
	spin_unlock(&imx214mipiraw_drv_lock);
	IMX214SetFlipMirror(sensor_config_data->SensorImageMirror);

	IMX214DBSOFIA("[IMX214Video]frame_len=%x\n", IMX214_read_cmos_sensor(0x0340));

    mDELAY(40);
	IMX214DB("IMX214Video exit:\n");
    return ERROR_NONE;
}


UINT32 IMX214Capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                                                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

 	kal_uint32 shutter = IMX214.shutter;
	kal_uint32 temp_data;
	//kal_uint32 pv_line_length , cap_line_length,

	if( SENSOR_MODE_CAPTURE== IMX214.sensorMode)
	{
		IMX214DB("IMX214Capture BusrtShot!!!\n");
	}
	else{
	IMX214DB("IMX214Capture enter:\n");

	//Record Preview shutter & gain
	shutter=IMX214_read_shutter();
	temp_data =  read_IMX214MIPI_gain();
	spin_lock(&imx214mipiraw_drv_lock);
	IMX214.pvShutter =shutter;
	IMX214.sensorGlobalGain = temp_data;
	IMX214.pvGain =IMX214.sensorGlobalGain;
	spin_unlock(&imx214mipiraw_drv_lock);

	IMX214DB("[IMX214Capture]IMX214.shutter=%d, read_pv_shutter=%d, read_pv_gain = 0x%x\n",IMX214.shutter, shutter,IMX214.sensorGlobalGain);

	// Full size setting
	IMX214CaptureSetting();

	spin_lock(&imx214mipiraw_drv_lock);
	IMX214.sensorMode = SENSOR_MODE_CAPTURE;
	IMX214.imgMirror = sensor_config_data->SensorImageMirror;
	IMX214.DummyPixels = 0;//define dummy pixels and lines
	IMX214.DummyLines = 0 ;
	IMX214_FeatureControl_PERIOD_PixelNum = IMX214_FULL_PERIOD_PIXEL_NUMS + IMX214.DummyPixels;
	IMX214_FeatureControl_PERIOD_LineNum = IMX214_FULL_PERIOD_LINE_NUMS + IMX214.DummyLines;
	spin_unlock(&imx214mipiraw_drv_lock);

	IMX214DB("[IMX214Capture] mirror&flip: %d\n",sensor_config_data->SensorImageMirror);
	//IMX214SetFlipMirror(sensor_config_data->SensorImageMirror);

	IMX214DB("IMX214Capture exit:\n");
	}

    return ERROR_NONE;
}	/* IMX214Capture() */

UINT32 IMX214GetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution)
{

    IMX214DB("IMX214GetResolution!!\n");

	pSensorResolution->SensorPreviewWidth	= IMX214_IMAGE_SENSOR_PV_WIDTH;
    pSensorResolution->SensorPreviewHeight	= IMX214_IMAGE_SENSOR_PV_HEIGHT;
    pSensorResolution->SensorFullWidth		= IMX214_IMAGE_SENSOR_FULL_WIDTH;
    pSensorResolution->SensorFullHeight		= IMX214_IMAGE_SENSOR_FULL_HEIGHT;
    pSensorResolution->SensorVideoWidth		= IMX214_IMAGE_SENSOR_VIDEO_WIDTH;
    pSensorResolution->SensorVideoHeight    = IMX214_IMAGE_SENSOR_VIDEO_HEIGHT;
	
    return ERROR_NONE;
}   /* IMX214GetResolution() */

UINT32 IMX214GetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId,
                                                MSDK_SENSOR_INFO_STRUCT *pSensorInfo,
                                                MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{      switch(ScenarioId)
	   {
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
			pSensorInfo->SensorPreviewResolutionX=IMX214_IMAGE_SENSOR_FULL_WIDTH;
			pSensorInfo->SensorPreviewResolutionY=IMX214_IMAGE_SENSOR_FULL_HEIGHT;
            pSensorInfo->SensorFullResolutionX    =  IMX214_IMAGE_SENSOR_FULL_WIDTH;
            pSensorInfo->SensorFullResolutionY    =  IMX214_IMAGE_SENSOR_FULL_HEIGHT; 			
			pSensorInfo->SensorCameraPreviewFrameRate=24;
		break;

        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        	pSensorInfo->SensorPreviewResolutionX=IMX214_IMAGE_SENSOR_PV_WIDTH;
       		pSensorInfo->SensorPreviewResolutionY=IMX214_IMAGE_SENSOR_PV_HEIGHT;
            pSensorInfo->SensorFullResolutionX    =  IMX214_IMAGE_SENSOR_FULL_WIDTH;
            pSensorInfo->SensorFullResolutionY    =  IMX214_IMAGE_SENSOR_FULL_HEIGHT;       		
			pSensorInfo->SensorCameraPreviewFrameRate=24;            
            break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
        	pSensorInfo->SensorPreviewResolutionX=IMX214_IMAGE_SENSOR_VIDEO_WIDTH;
       		pSensorInfo->SensorPreviewResolutionY=IMX214_IMAGE_SENSOR_VIDEO_HEIGHT;
            pSensorInfo->SensorFullResolutionX    =  IMX214_IMAGE_SENSOR_FULL_WIDTH;
            pSensorInfo->SensorFullResolutionY    =  IMX214_IMAGE_SENSOR_FULL_HEIGHT;       		
			pSensorInfo->SensorCameraPreviewFrameRate=30;  
			break;
		default:
        	pSensorInfo->SensorPreviewResolutionX=IMX214_IMAGE_SENSOR_PV_WIDTH;
       		pSensorInfo->SensorPreviewResolutionY=IMX214_IMAGE_SENSOR_PV_HEIGHT;
            pSensorInfo->SensorFullResolutionX    =  IMX214_IMAGE_SENSOR_FULL_WIDTH;
            pSensorInfo->SensorFullResolutionY    =  IMX214_IMAGE_SENSOR_FULL_HEIGHT;       		
			pSensorInfo->SensorCameraPreviewFrameRate=30;
		break;
	}

	spin_lock(&imx214mipiraw_drv_lock);
	IMX214.imgMirror = pSensorConfigData->SensorImageMirror ;
	spin_unlock(&imx214mipiraw_drv_lock);

   	pSensorInfo->SensorOutputDataFormat= SENSOR_OUTPUT_FORMAT_RAW_B;
    pSensorInfo->SensorClockPolarity =SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorClockFallingPolarity=SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;

    pSensorInfo->SensroInterfaceType=SENSOR_INTERFACE_TYPE_MIPI;
	pSensorInfo->MIPIsensorType=MIPI_OPHY_NCSI2;
    pSensorInfo->SettleDelayMode = MIPI_SETTLEDELAY_AUTO;

    pSensorInfo->CaptureDelayFrame = 3;
    pSensorInfo->PreviewDelayFrame = 3;
    pSensorInfo->VideoDelayFrame = 3;

    pSensorInfo->SensorDrivingCurrent = ISP_DRIVING_8MA;
    pSensorInfo->AEShutDelayFrame = 0;//0;		    /* The frame of setting shutter default 0 for TG int */
    pSensorInfo->AESensorGainDelayFrame = 1 ;//0;     /* The frame of setting sensor gain */
    pSensorInfo->AEISPGainDelayFrame = 2;
	  

    switch (ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            pSensorInfo->SensorClockFreq=24;
            pSensorInfo->SensorClockRisingCount= 0;

            pSensorInfo->SensorGrabStartX = IMX214_PV_X_START;
            pSensorInfo->SensorGrabStartY = IMX214_PV_Y_START;

            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	     	pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 85;
	    	pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
			pSensorInfo->SensorWidthSampling = 0;	// 0 is default 1x
			pSensorInfo->SensorHightSampling = 0;	 // 0 is default 1x
			pSensorInfo->SensorPacketECCOrder = 1;
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            pSensorInfo->SensorClockFreq=24;
            pSensorInfo->SensorClockRisingCount= 0;

            pSensorInfo->SensorGrabStartX = IMX214_VIDEO_X_START;
            pSensorInfo->SensorGrabStartY = IMX214_VIDEO_Y_START;

            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	     	pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 85;
	    	pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
			pSensorInfo->SensorWidthSampling = 0;	// 0 is default 1x
			pSensorInfo->SensorHightSampling = 0;	 // 0 is default 1x
			pSensorInfo->SensorPacketECCOrder = 1;
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
            pSensorInfo->SensorClockFreq=24;
            pSensorInfo->SensorClockRisingCount= 0;

            pSensorInfo->SensorGrabStartX = IMX214_FULL_X_START;	//2*IMX214_IMAGE_SENSOR_PV_STARTX;
            pSensorInfo->SensorGrabStartY = IMX214_FULL_Y_START;	//2*IMX214_IMAGE_SENSOR_PV_STARTY;

            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 85;
            pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
			pSensorInfo->SensorWidthSampling = 0;	// 0 is default 1x
			pSensorInfo->SensorHightSampling = 0;	 // 0 is default 1x
			pSensorInfo->SensorPacketECCOrder = 1;

            break;
        default:
			pSensorInfo->SensorClockFreq=24;
            pSensorInfo->SensorClockRisingCount= 0;

            pSensorInfo->SensorGrabStartX = IMX214_PV_X_START;
            pSensorInfo->SensorGrabStartY = IMX214_PV_Y_START;

            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	     	pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 85;
	    	pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
			pSensorInfo->SensorWidthSampling = 0;	// 0 is default 1x
			pSensorInfo->SensorHightSampling = 0;	 // 0 is default 1x
			pSensorInfo->SensorPacketECCOrder = 1;

            break;
    }

    memcpy(pSensorConfigData, &IMX214SensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));

    return ERROR_NONE;
}   /* IMX214GetInfo() */


UINT32 IMX214Control(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow,
                                                MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
		spin_lock(&imx214mipiraw_drv_lock);
		IMX214CurrentScenarioId = ScenarioId;
		spin_unlock(&imx214mipiraw_drv_lock);
		IMX214DB("IMX214CurrentScenarioId=%d\n",IMX214CurrentScenarioId);

	switch (ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            IMX214Preview(pImageWindow, pSensorConfigData);
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			IMX214Video(pImageWindow, pSensorConfigData);
			break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
            IMX214Capture(pImageWindow, pSensorConfigData);
            break;
        default:
            return ERROR_INVALID_SCENARIO_ID;

    }
    return ERROR_NONE;
} /* IMX214Control() */


UINT32 IMX214SetVideoMode(UINT16 u2FrameRate)
{
    //return;
    kal_uint32 MIN_Frame_length =0,frameRate=0,extralines=0;
    IMX214DB("[IMX214SetVideoMode] frame rate = %d\n", u2FrameRate);

	spin_lock(&imx214mipiraw_drv_lock);
	VIDEO_MODE_TARGET_FPS=u2FrameRate;
	spin_unlock(&imx214mipiraw_drv_lock);

	if(u2FrameRate==0)
	{
		IMX214DB("Disable Video Mode or dynimac fps\n");
		return KAL_TRUE;
	}
	if(u2FrameRate >30 || u2FrameRate <5)
	    IMX214DB("error frame rate seting\n");

    if(IMX214.sensorMode == SENSOR_MODE_VIDEO)//video ScenarioId recording
    {
    	if(IMX214.IMX214AutoFlickerMode == KAL_TRUE)
    	{
    		if (u2FrameRate==30)
				frameRate= 306;
			else if(u2FrameRate==15)
				frameRate= 148;//148;
			else
				frameRate=u2FrameRate*10;

			MIN_Frame_length = (IMX214.videoPclk*100000)/(IMX214_VIDEO_PERIOD_PIXEL_NUMS + IMX214.DummyPixels)/frameRate*10;
    	}
		else
			MIN_Frame_length = (IMX214.videoPclk*100000) /(IMX214_VIDEO_PERIOD_PIXEL_NUMS + IMX214.DummyPixels)/u2FrameRate;

		if((MIN_Frame_length <=IMX214_VIDEO_PERIOD_LINE_NUMS))
		{
			MIN_Frame_length = IMX214_VIDEO_PERIOD_LINE_NUMS;
			IMX214DB("[IMX214SetVideoMode]current fps = %d\n", (IMX214.pvPclk*100000)  /(IMX214_VIDEO_PERIOD_PIXEL_NUMS)/IMX214_VIDEO_PERIOD_LINE_NUMS);
		}
		IMX214DB("[IMX214SetVideoMode]current fps (10 base)= %d\n", (IMX214.pvPclk*100000)*10/(IMX214_VIDEO_PERIOD_PIXEL_NUMS + IMX214.DummyPixels)/MIN_Frame_length);
		extralines = MIN_Frame_length - IMX214_VIDEO_PERIOD_LINE_NUMS;
		IMX214DB("[IMX214SetVideoMode]extralines= %d\n", extralines);
		spin_lock(&imx214mipiraw_drv_lock);
		IMX214.DummyPixels = 0;//define dummy pixels and lines
		IMX214.DummyLines = extralines ;
		spin_unlock(&imx214mipiraw_drv_lock);
		
		//IMX214_SetDummy(IMX214.DummyPixels,extralines);
        IMX214_write_cmos_sensor(0x0104, 1); 
	    IMX214_write_cmos_sensor(0x0340, (MIN_Frame_length >>8) & 0xFF);
	    IMX214_write_cmos_sensor(0x0341, MIN_Frame_length& 0xFF);

		if(IMX214.shutter>MIN_Frame_length-10)
			{
			  IMX214.shutter=MIN_Frame_length-10;
			  IMX214_write_cmos_sensor(0x0202, (IMX214.shutter >> 8) & 0xFF);
              IMX214_write_cmos_sensor(0x0203, IMX214.shutter  & 0xFF);
			}
	    IMX214_write_cmos_sensor(0x0104, 0); 
    }
	IMX214DB("[IMX214SetVideoMode]MIN_Frame_length=%d,IMX214.DummyLines=%d\n",MIN_Frame_length,IMX214.DummyLines);

    return KAL_TRUE;
}

UINT32 IMX214SetAutoFlickerMode(kal_bool bEnable, UINT16 u2FrameRate)
{
    //return;
    IMX214DB("[IMX214SetAutoFlickerMode] frame rate(10base) = %d %d\n", bEnable, u2FrameRate);
    if (KAL_TRUE == IMX214DuringTestPattern) return ERROR_NONE;
	if(bEnable) {   // enable auto flicker
		spin_lock(&imx214mipiraw_drv_lock);
		IMX214.IMX214AutoFlickerMode = KAL_TRUE;
		spin_unlock(&imx214mipiraw_drv_lock);
    } else {
    	spin_lock(&imx214mipiraw_drv_lock);
        IMX214.IMX214AutoFlickerMode = KAL_FALSE;
		spin_unlock(&imx214mipiraw_drv_lock);
        IMX214DB("Disable Auto flicker\n");
    }

    return ERROR_NONE;
}

UINT32 IMX214SetTestPatternMode(kal_bool bEnable)
{
    IMX214DB("[IMX214SetTestPatternMode] Test pattern enable:%d\n", bEnable);
	kal_uint16 temp;

	if(bEnable) 
	{
        spin_lock(&imx214mipiraw_drv_lock);
	    IMX214DuringTestPattern = KAL_TRUE;
	    spin_unlock(&imx214mipiraw_drv_lock);
		IMX214_write_cmos_sensor(0x0601,0x0002);
	}
   else		
	{
		IMX214_write_cmos_sensor(0x0601,0x0000);	
	}

    return ERROR_NONE;
}

UINT32 IMX214MIPISetMaxFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 frameRate) {
	kal_uint32 pclk;
	kal_int16 dummyLine;
	kal_uint16 lineLength,frameHeight;
		
	IMX214DB("IMX214MIPISetMaxFramerateByScenario: scenarioId = %d, frame rate = %d\n",scenarioId,frameRate);
	switch (scenarioId) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			pclk = 380800000;
			lineLength = IMX214_PV_PERIOD_PIXEL_NUMS;
			frameHeight = (10 * pclk)/frameRate/lineLength;
			dummyLine = frameHeight - IMX214_PV_PERIOD_LINE_NUMS;
			if(dummyLine<0)
				dummyLine = 0;
			spin_lock(&imx214mipiraw_drv_lock);
			IMX214.sensorMode = SENSOR_MODE_PREVIEW;
			spin_unlock(&imx214mipiraw_drv_lock);
			IMX214_SetDummy(0, dummyLine);			
			break;			
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			pclk = 380800000;
			lineLength = IMX214_VIDEO_PERIOD_PIXEL_NUMS;
			frameHeight = (10 * pclk)/frameRate/lineLength;
			dummyLine = frameHeight - IMX214_VIDEO_PERIOD_LINE_NUMS;
			if(dummyLine<0)
				dummyLine = 0;
			spin_lock(&imx214mipiraw_drv_lock);
			IMX214.sensorMode = SENSOR_MODE_VIDEO;
			spin_unlock(&imx214mipiraw_drv_lock);
			IMX214_SetDummy(0, dummyLine);			
			break;			
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:			
			pclk = 380800000;
			lineLength = IMX214_FULL_PERIOD_PIXEL_NUMS;
			frameHeight = (10 * pclk)/frameRate/lineLength;
			dummyLine = frameHeight - IMX214_FULL_PERIOD_LINE_NUMS;
			if(dummyLine<0)
				dummyLine = 0;
			spin_lock(&imx214mipiraw_drv_lock);
			IMX214.sensorMode = SENSOR_MODE_CAPTURE;
			spin_unlock(&imx214mipiraw_drv_lock);
			IMX214_SetDummy(0, dummyLine);			
			break;			
		default:
			break;
	}	
	return ERROR_NONE;
}


UINT32 IMX214MIPIGetDefaultFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 *pframeRate) 
{

	switch (scenarioId) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			 *pframeRate = 300;
			 break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
			 *pframeRate = 240;
			break;			
		default:
			break;
	}

	return ERROR_NONE;
}



UINT32 IMX214FeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId,
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
	   MSDK_SENSOR_ENG_INFO_STRUCT	  *pSensorEngInfo=(MSDK_SENSOR_ENG_INFO_STRUCT *) pFeaturePara;
	
	   IMX214DB("IMX214_FeatureControl FeatureId(%d)\n",FeatureId);
	
	   switch (FeatureId)
	   {
		   case SENSOR_FEATURE_GET_RESOLUTION:
			   *pFeatureReturnPara16++=IMX214_IMAGE_SENSOR_FULL_WIDTH;
			   *pFeatureReturnPara16=IMX214_IMAGE_SENSOR_FULL_HEIGHT;
			   *pFeatureParaLen=4;
			   break;
		   case SENSOR_FEATURE_GET_PERIOD:
			   *pFeatureReturnPara16++= IMX214_FeatureControl_PERIOD_PixelNum;
			   *pFeatureReturnPara16= IMX214_FeatureControl_PERIOD_LineNum;
			   *pFeatureParaLen=4;
			   break;
		   case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
			   switch(IMX214CurrentScenarioId)
			   {   
			       case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
					   *pFeatureReturnPara32 = IMX214.videoPclk * 100000;
					      *pFeatureParaLen=4;
					   break;
				   case MSDK_SCENARIO_ID_CAMERA_ZSD:
				   case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
					   *pFeatureReturnPara32 = IMX214.capPclk * 100000; //19500000;
						  *pFeatureParaLen=4;
					   break;
				   default:
					   *pFeatureReturnPara32 = IMX214.pvPclk * 100000; //19500000;
						  *pFeatureParaLen=4;
					   break;
			   }
			   break;
		   case SENSOR_FEATURE_SET_ESHUTTER:
			   IMX214_SetShutter(*pFeatureData16);
			   break;
		   case SENSOR_FEATURE_SET_NIGHTMODE:
			   IMX214_NightMode((BOOL) *pFeatureData16);
			   break;
		   case SENSOR_FEATURE_SET_GAIN:
			   IMX214MIPI_SetGain((UINT16) *pFeatureData16);
			   break;
		   case SENSOR_FEATURE_SET_FLASHLIGHT:
			   break;
		   case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
			  // IMX214_isp_master_clock=*pFeatureData32;
			   break;
		   case SENSOR_FEATURE_SET_REGISTER:
			   IMX214_write_cmos_sensor(pSensorRegData->RegAddr, pSensorRegData->RegData);
			   break;
		   case SENSOR_FEATURE_GET_REGISTER:
			   pSensorRegData->RegData = IMX214_read_cmos_sensor(pSensorRegData->RegAddr);
			   break;
		   case SENSOR_FEATURE_SET_CCT_REGISTER:
			   SensorRegNumber=FACTORY_END_ADDR;
			   for (i=0;i<SensorRegNumber;i++)
			   {
				   IMX214SensorCCT[i].Addr=*pFeatureData32++;
				   IMX214SensorCCT[i].Para=*pFeatureData32++;
			   }
			   break;
		   case SENSOR_FEATURE_GET_CCT_REGISTER:
			   SensorRegNumber=FACTORY_END_ADDR;
			   if (*pFeatureParaLen<(SensorRegNumber*sizeof(SENSOR_REG_STRUCT)+4))
				   return FALSE;
			   *pFeatureData32++=SensorRegNumber;
			   for (i=0;i<SensorRegNumber;i++)
			   {
				   *pFeatureData32++=IMX214SensorCCT[i].Addr;
				   *pFeatureData32++=IMX214SensorCCT[i].Para;
			   }
			   break;
		   case SENSOR_FEATURE_SET_ENG_REGISTER:
			   SensorRegNumber=ENGINEER_END;
			   for (i=0;i<SensorRegNumber;i++)
			   {
				   IMX214SensorReg[i].Addr=*pFeatureData32++;
				   IMX214SensorReg[i].Para=*pFeatureData32++;
			   }
			   break;
		   case SENSOR_FEATURE_GET_ENG_REGISTER:
			   SensorRegNumber=ENGINEER_END;
			   if (*pFeatureParaLen<(SensorRegNumber*sizeof(SENSOR_REG_STRUCT)+4))
				   return FALSE;
			   *pFeatureData32++=SensorRegNumber;
			   for (i=0;i<SensorRegNumber;i++)
			   {
				   *pFeatureData32++=IMX214SensorReg[i].Addr;
				   *pFeatureData32++=IMX214SensorReg[i].Para;
			   }
			   break;
		   case SENSOR_FEATURE_GET_REGISTER_DEFAULT:
			   if (*pFeatureParaLen>=sizeof(NVRAM_SENSOR_DATA_STRUCT))
			   {
				   pSensorDefaultData->Version=NVRAM_CAMERA_SENSOR_FILE_VERSION;
				   pSensorDefaultData->SensorId=IMX214_SENSOR_ID;
				   memcpy(pSensorDefaultData->SensorEngReg, IMX214SensorReg, sizeof(SENSOR_REG_STRUCT)*ENGINEER_END);
				   memcpy(pSensorDefaultData->SensorCCTReg, IMX214SensorCCT, sizeof(SENSOR_REG_STRUCT)*FACTORY_END_ADDR);
			   }
			   else
				   return FALSE;
			   *pFeatureParaLen=sizeof(NVRAM_SENSOR_DATA_STRUCT);
			   break;
		   case SENSOR_FEATURE_GET_CONFIG_PARA:
			   break;
		   case SENSOR_FEATURE_CAMERA_PARA_TO_SENSOR:
			   //IMX214_camera_para_to_sensor();
			   break;
	
		   case SENSOR_FEATURE_SENSOR_TO_CAMERA_PARA:
			   //IMX214_sensor_to_camera_para();
			   break;
		   case SENSOR_FEATURE_GET_GROUP_COUNT:

			   break;
		   case SENSOR_FEATURE_GET_GROUP_INFO:

			   break;
		   case SENSOR_FEATURE_GET_ITEM_INFO:

			   break;
	
		   case SENSOR_FEATURE_SET_ITEM_INFO:

			   break;
	
		   case SENSOR_FEATURE_GET_ENG_INFO:
			   pSensorEngInfo->SensorId = 221;
			   pSensorEngInfo->SensorType = CMOS_SENSOR;
	
			   pSensorEngInfo->SensorOutputDataFormat = SENSOR_OUTPUT_FORMAT_RAW_B;
	
			   *pFeatureParaLen=sizeof(MSDK_SENSOR_ENG_INFO_STRUCT);
			   break;
		   case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
			   // get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE
			   // if EEPROM does not exist in camera module.
			   *pFeatureReturnPara32=LENS_DRIVER_ID_DO_NOT_CARE;
			   *pFeatureParaLen=4;
			   break;
	
		   case SENSOR_FEATURE_SET_VIDEO_MODE:
			   IMX214SetVideoMode(*pFeatureData16);
			   break;
		   case SENSOR_FEATURE_CHECK_SENSOR_ID:
			   IMX214GetSensorID(pFeatureReturnPara32);
			   break;
	       case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
               IMX214SetAutoFlickerMode((BOOL)*pFeatureData16, *(pFeatureData16+1));
	           break;
           case SENSOR_FEATURE_SET_TEST_PATTERN:
               IMX214SetTestPatternMode((BOOL)*pFeatureData16);
               break;
		   case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE://for factory mode auto testing 			
               *pFeatureReturnPara32= IMX214_TEST_PATTERN_CHECKSUM;
			   *pFeatureParaLen=4; 							
			break;	
		   case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
			   IMX214MIPISetMaxFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32, *(pFeatureData32+1));
			   break;
		   case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
			   IMX214MIPIGetDefaultFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32, (MUINT32 *)(*(pFeatureData32+1)));
			  break;

		   default:
			   break;

    }
    return ERROR_NONE;
}	/* IMX214FeatureControl() */


SENSOR_FUNCTION_STRUCT	SensorFuncIMX214=
{
    IMX214Open,
    IMX214GetInfo,
    IMX214GetResolution,
    IMX214FeatureControl,
    IMX214Control,
    IMX214Close
};

UINT32 IMX214_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
    /* To Do : Check Sensor status here */
    if (pfFunc!=NULL)
        *pfFunc=&SensorFuncIMX214;

    return ERROR_NONE;
}   /* SensorInit() */

