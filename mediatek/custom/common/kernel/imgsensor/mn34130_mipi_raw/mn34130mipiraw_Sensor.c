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
#include <linux/xlog.h>
#include <asm/system.h>

#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "mn34130mipiraw_Sensor.h"
#include "mn34130mipiraw_Camera_Sensor_para.h"
#include "mn34130mipiraw_CameraCustomized.h"




static void MN34130_Sensor_Init(void);

static DEFINE_SPINLOCK(mn34130mipiraw_drv_lock);

#define MN34130_DEBUG
#define MN34130_DEBUG_SOFIA

#ifdef MN34130_DEBUG
	#define MN34130DB(fmt, arg...) xlog_printk(ANDROID_LOG_DEBUG, "[MN34130Raw] ",  fmt, ##arg)
#else
	#define MN34130DB(fmt, arg...)
#endif

#ifdef MN34130_DEBUG_SOFIA
	#define MN34130DBSOFIA(fmt, arg...) xlog_printk(ANDROID_LOG_DEBUG, "[MN34130Raw] ",  fmt, ##arg)
#else
	#define MN34130DBSOFIA(fmt, arg...)
#endif

#define mDELAY(ms)  mdelay(ms)

kal_uint32 MN34130_FeatureControl_PERIOD_PixelNum=MN34130_PV_PERIOD_PIXEL_NUMS;
kal_uint32 MN34130_FeatureControl_PERIOD_LineNum=MN34130_PV_PERIOD_LINE_NUMS;


UINT16 VIDEO_MODE_TARGET_FPS = 30;

//static BOOL g_NeedSetPLL = KAL_TRUE;


MSDK_SENSOR_CONFIG_STRUCT MN34130SensorConfigData;

kal_uint32 MN34130_FAC_SENSOR_REG;

MSDK_SCENARIO_ID_ENUM MN34130CurrentScenarioId = MSDK_SCENARIO_ID_CAMERA_PREVIEW;

/* FIXME: old factors and DIDNOT use now. s*/
SENSOR_REG_STRUCT MN34130SensorCCT[]=CAMERA_SENSOR_CCT_DEFAULT_VALUE;
SENSOR_REG_STRUCT MN34130SensorReg[ENGINEER_END]=CAMERA_SENSOR_REG_DEFAULT_VALUE;
/* FIXME: old factors and DIDNOT use now. e*/

static MN34130_PARA_STRUCT mn34130;

extern int iReadReg(u16 a_u2Addr , u8 * a_puBuff , u16 i2cId);
extern int iWriteReg(u16 a_u2Addr , u32 a_u4Data , u32 a_u4Bytes , u16 i2cId);

#define MN34130_write_cmos_sensor(addr, para) iWriteReg((u16) addr , (u32) para , 1, MN34130MIPI_WRITE_ID)
static kal_uint16 mn34130_gaintable[232][2] = {
	{64  ,256  },
	{66  ,259  },
	{67  ,260  },
	{69  ,263  },
	{71  ,266  },
	{72  ,267  },
	{73  ,268  },
	{76  ,271  },
	{80  ,277  },
	{81  ,278  },
	{82  ,279  },
	{83  ,280  },
	{86  ,283  },
	{90  ,287  },
	{93  ,291  },
	{94  ,292  },
	{95  ,293  },
	{99  ,296  },
	{102 ,299  },
	{103 ,300  },
	{104 ,301  },
	{105 ,302  },
	{106 ,303  },
	{108 ,304  },
	{109 ,305  },
	{110 ,306  },
	{111 ,307  },
	{112 ,308  },
	{113 ,309  },
	{115 ,310  },
	{116 ,311  },
	{117 ,312  },
	{118 ,313  },
	{120 ,314  },
	{121 ,315  },
	{122 ,316  },
	{124 ,317  },
	{125 ,318  },
	{126 ,319  },
	{128 ,320  },
	{129 ,321  },
	{131 ,322  },
	{132 ,323  },
	{133 ,324  },
	{135 ,325  },
	{136 ,326  },
	{138 ,327  },
	{140 ,328  },
	{141 ,329  },
	{142 ,330  },
	{144 ,331  },
	{145 ,332  },
	{147 ,333  },
	{148 ,334  },
	{150 ,335  },
	{152 ,336  },
	{154 ,337  },
	{155 ,338  },
	{157 ,339  },
	{159 ,340  },
	{160 ,341  },
	{162 ,342  },
	{164 ,343  },
	{166 ,344  },
	{167 ,345  },
	{169 ,346  },
	{171 ,347  },
	{173 ,348  },
	{175 ,349  },
	{177 ,350  },
	{179 ,351  },
	{180 ,352  },
	{182 ,353  },
	{184 ,354  },
	{186 ,355  },
	{188 ,356  },
	{190 ,357  },
	{193 ,358  },
	{195 ,359  },
	{196 ,360  },
	{199 ,361  },
	{201 ,362  },
	{203 ,363  },
	{205 ,364  },
	{207 ,365  },
	{210 ,366  },
	{212 ,367  },
	{214 ,368  },
	{217 ,369  },
	{219 ,370  },
	{221 ,371  },
	{224 ,372  },
	{227 ,373  },
	{228 ,374  },
	{231 ,375  },
	{234 ,376  },
	{236 ,377  },
	{239 ,378  },
	{241 ,379  },
	{244 ,380  },
	{246 ,381  },
	{250 ,382  },
	{252 ,383  },
	{255 ,384  },
	{257 ,385  },
	{260 ,386  },
	{263 ,387  },
	{266 ,388  },
	{269 ,389  },
	{272 ,390  },
	{275 ,391  },
	{278 ,392  },
	{281 ,393  },
	{284 ,394  },
	{287 ,395  },
	{290 ,396  },
	{293 ,397  },
	{296 ,398  },
	{300 ,399  },
	{303 ,400  },
	{306 ,401  },
	{309 ,402  },
	{313 ,403  },
	{316 ,404  },
	{319 ,405  },
	{323 ,406  },
	{326 ,407  },
	{330 ,408  },
	{333 ,409  },
	{337 ,410  },
	{341 ,411  },
	{345 ,412  },
	{348 ,413  },
	{352 ,414  },
	{356 ,415  },
	{360 ,416  },
	{364 ,417  },
	{368 ,418  },
	{372 ,419  },
	{376 ,420  },
	{380 ,421  },
	{384 ,422  },
	{388 ,423  },
	{392 ,424  },
	{397 ,425  },
	{401 ,426  },
	{405 ,427  },
	{410 ,428  },
	{414 ,429  },
	{419 ,430  },
	{423 ,431  },
	{428 ,432  },
	{433 ,433  },
	{437 ,434  },
	{442 ,435  },
	{447 ,436  },
	{451 ,437  },
	{456 ,438  },
	{461 ,439  },
	{467 ,440  },
	{472 ,441  },
	{477 ,442  },
	{482 ,443  },
	{487 ,444  },
	{492 ,445  },
	{497 ,446  },
	{503 ,447  },
	{508 ,448  },
	{514 ,449  },
	{520 ,450  },
	{525 ,451  },
	{531 ,452  },
	{536 ,453  },
	{542 ,454  },
	{548 ,455  },
	{554 ,456  },
	{560 ,457  },
	{566 ,458  },
	{572 ,459  },
	{579 ,460  },
	{585 ,461  },
	{591 ,462  },
	{598 ,463  },
	{604 ,464  },
	{611 ,465  },
	{618 ,466  },
	{624 ,467  },
	{631 ,468  },
	{637 ,469  },
	{644 ,470  },
	{652 ,471  },
	{659 ,472  },
	{666 ,473  },
	{673 ,474  },
	{680 ,475  },
	{688 ,476  },
	{695 ,477  },
	{703 ,478  },
	{710 ,479  },
	{718 ,480  },
	{726 ,481  },
	{734 ,482  },
	{742 ,483  },
	{750 ,484  },
	{758 ,485  },
	{766 ,486  },
	{774 ,487  },
	{783 ,488  },
	{791 ,489  },
	{800 ,490  },
	{808 ,491  },
	{817 ,492  },
	{826 ,493  },
	{835 ,494  },
	{844 ,495  },
	{854 ,496  },
	{863 ,497  },
	{872 ,498  },
	{881 ,499  },
	{891 ,500  },
	{900 ,501  },
	{911 ,502  },
	{920 ,503  },
	{931 ,504  },
	{941 ,505  },
	{951 ,506  },
	{961 ,507  },
	{972 ,508  },
	{982 ,509  },
	{993 ,510  },
	{1004,511  },
	{1014,512  }


};
kal_uint16 MN34130_read_cmos_sensor(kal_uint32 addr)
{
    kal_uint16 get_byte = 0;
    iReadReg((u16) addr ,(u8*)&get_byte, MN34130MIPI_WRITE_ID);
    return get_byte;
}

#define Sleep(ms) mdelay(ms)

void MN34130_write_shutter(kal_uint32 shutter)
{
	kal_uint32 min_framelength = MN34130_PV_PERIOD_LINE_NUMS, max_shutter=0;
	kal_uint32 extra_lines = 0;
	kal_uint32 line_length = 0;
	kal_uint32 frame_length = 0;
	unsigned long flags;

	MN34130DBSOFIA("MN34130 write shutter!!shutter=%d!!!!!\n", shutter);

	if(mn34130.MN34130AutoFlickerMode == KAL_TRUE)
	{
		if ( SENSOR_MODE_PREVIEW == mn34130.sensorMode )  //(g_iMN34130_Mode == MN34130_MODE_PREVIEW)	//SXGA size output
		{
			line_length = MN34130_PV_PERIOD_PIXEL_NUMS + mn34130.DummyPixels;
			max_shutter = MN34130_PV_PERIOD_LINE_NUMS + mn34130.DummyLines ;
		}
		else if( SENSOR_MODE_VIDEO == mn34130.sensorMode ) //add for video_6M setting
		{
			line_length = MN34130_VIDEO_PERIOD_PIXEL_NUMS + mn34130.DummyPixels;
			max_shutter = MN34130_VIDEO_PERIOD_LINE_NUMS + mn34130.DummyLines ;
		}
		else
		{
			line_length = MN34130_FULL_PERIOD_PIXEL_NUMS + mn34130.DummyPixels;
			max_shutter = MN34130_FULL_PERIOD_LINE_NUMS + mn34130.DummyLines ;
		}

		switch(MN34130CurrentScenarioId)
		{
        	case MSDK_SCENARIO_ID_CAMERA_ZSD:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			MN34130DBSOFIA("AutoFlickerMode!!! MSDK_SCENARIO_ID_CAMERA_ZSD  0!!\n");
			min_framelength = max_shutter;// capture max_fps 15, no need calculate
			break;
			
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			if(VIDEO_MODE_TARGET_FPS == 30)
			{
				min_framelength = (mn34130.videoPclk*100000) /(MN34130_VIDEO_PERIOD_PIXEL_NUMS + mn34130.DummyPixels)/304 ;
			}
			else if(VIDEO_MODE_TARGET_FPS == 15)
			{
				min_framelength = (mn34130.videoPclk*100000) /(MN34130_VIDEO_PERIOD_PIXEL_NUMS + mn34130.DummyPixels)/148 ;
			}
			else
			{
				min_framelength = max_shutter;
			}
			break;
			
		default:
			min_framelength = (mn34130.pvPclk*100000) /(MN34130_PV_PERIOD_PIXEL_NUMS + mn34130.DummyPixels)/296 ;
    			break;
		}

		MN34130DBSOFIA("AutoFlickerMode!!! min_framelength for AutoFlickerMode = %d (0x%x)\n",min_framelength,min_framelength);
		MN34130DBSOFIA("max framerate(10 base) autofilker = %d\n",(mn34130.pvPclk*10000)*10 /line_length/min_framelength);

		if (shutter < 3)
			shutter = 3;

		if (shutter > max_shutter-4)
			extra_lines = shutter - (max_shutter - 4);
		else
			extra_lines = 0;

		if ( SENSOR_MODE_PREVIEW == mn34130.sensorMode )	//SXGA size output
		{
			frame_length = MN34130_PV_PERIOD_LINE_NUMS+ mn34130.DummyLines + extra_lines ;
			//MN34130_write_cmos_sensor(0x306f, (frame_length >> 8) & 0xFF);
			//MN34130_write_cmos_sensor(0x3070, frame_length & 0xFF);
		}
		else if(SENSOR_MODE_VIDEO == mn34130.sensorMode)
		{
			frame_length = MN34130_VIDEO_PERIOD_LINE_NUMS+ mn34130.DummyLines + extra_lines ;
			//MN34130_write_cmos_sensor(0x306f, (frame_length >> 8) & 0xFF);
			//MN34130_write_cmos_sensor(0x3070, frame_length & 0xFF);
		}
		else	
		{
			frame_length = MN34130_FULL_PERIOD_LINE_NUMS + mn34130.DummyLines + extra_lines ;
			//MN34130_write_cmos_sensor(0x3089, (frame_length >> 8) & 0xFF);
			//MN34130_write_cmos_sensor(0x308A, frame_length & 0xFF);
		}
		MN34130DBSOFIA("frame_length 0= %d\n",frame_length);

		if (frame_length < min_framelength)
		{
			//shutter = min_framelength - 4;

			switch(MN34130CurrentScenarioId)
			{
        		case MSDK_SCENARIO_ID_CAMERA_ZSD:
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				extra_lines = min_framelength - (MN34130_FULL_PERIOD_LINE_NUMS+ mn34130.DummyLines);
				break;
				
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				extra_lines = min_framelength - (MN34130_VIDEO_PERIOD_LINE_NUMS+ mn34130.DummyLines);
			default:
				extra_lines = min_framelength - (MN34130_PV_PERIOD_LINE_NUMS+ mn34130.DummyLines);
	    			break;
			}
			frame_length = min_framelength;
		}
		
		MN34130DBSOFIA("frame_length 1= %d\n",frame_length);

		ASSERT(line_length < MN34130_MAX_LINE_LENGTH);		//0xCCCC
		ASSERT(frame_length < MN34130_MAX_FRAME_LENGTH); 	//0xFFFF

		//Set total frame length
		
		if ( SENSOR_MODE_PREVIEW == mn34130.sensorMode )	//SXGA size output
				{
					//frame_length = MN34130_PV_PERIOD_LINE_NUMS+ mn34130.DummyLines + extra_lines ;
					MN34130_write_cmos_sensor(0x306f, (frame_length >> 8) & 0xFF);
					MN34130_write_cmos_sensor(0x3070, frame_length & 0xFF);
				}
				else if(SENSOR_MODE_VIDEO == mn34130.sensorMode)
				{
					//frame_length = MN34130_VIDEO_PERIOD_LINE_NUMS+ mn34130.DummyLines + extra_lines ;
					MN34130_write_cmos_sensor(0x306f, (frame_length >> 8) & 0xFF);
					MN34130_write_cmos_sensor(0x3070, frame_length & 0xFF);
				}
				else	
				{
					//frame_length = MN34130_FULL_PERIOD_LINE_NUMS + mn34130.DummyLines + extra_lines ;
					MN34130_write_cmos_sensor(0x3089, (frame_length >> 8) & 0xFF);
					MN34130_write_cmos_sensor(0x308A, frame_length & 0xFF);
				}
		//MN34130_write_cmos_sensor(0x0340, (frame_length >> 8) & 0xFF);
		//MN34130_write_cmos_sensor(0x0341, frame_length & 0xFF);

		spin_lock_irqsave(&mn34130mipiraw_drv_lock,flags);
		mn34130.maxExposureLines = frame_length-4;
		MN34130_FeatureControl_PERIOD_PixelNum = line_length;
		MN34130_FeatureControl_PERIOD_LineNum = frame_length;
		spin_unlock_irqrestore(&mn34130mipiraw_drv_lock,flags);

		//Set shutter (Coarse integration time, uint: lines.)
		MN34130_write_cmos_sensor(0x0202, (shutter>>8) & 0xFF);
		MN34130_write_cmos_sensor(0x0203, shutter & 0xFF);

		MN34130DBSOFIA("frame_length 2= %d\n",frame_length);
		MN34130DB("framerate(10 base) = %d\n",(mn34130.pvPclk*10000)*10 /line_length/frame_length);

		MN34130DB("shutter=%d, extra_lines=%d, line_length=%d, frame_length=%d\n", shutter, extra_lines, line_length, frame_length);

	}
	else
	{
		if ( SENSOR_MODE_PREVIEW == mn34130.sensorMode )  //(g_iMN34130_Mode == MN34130_MODE_PREVIEW)	//SXGA size output
		{
			max_shutter = MN34130_PV_PERIOD_LINE_NUMS + mn34130.DummyLines ;
		}
		else if( SENSOR_MODE_VIDEO == mn34130.sensorMode ) //add for video_6M setting
		{
			max_shutter = MN34130_VIDEO_PERIOD_LINE_NUMS + mn34130.DummyLines ;
		}
		else
		{
			max_shutter = MN34130_FULL_PERIOD_LINE_NUMS + mn34130.DummyLines ;
		}

		if (shutter < 3)
			shutter = 3;

		if (shutter > max_shutter-4)
			extra_lines = shutter - (max_shutter - 4);
		else
			extra_lines = 0;

		if ( SENSOR_MODE_PREVIEW == mn34130.sensorMode )	//SXGA size output
		{
			line_length = MN34130_PV_PERIOD_PIXEL_NUMS + mn34130.DummyPixels;
			frame_length = MN34130_PV_PERIOD_LINE_NUMS+ mn34130.DummyLines + extra_lines ;
			//MN34130_write_cmos_sensor(0x306f, (frame_length >> 8) & 0xFF);
			//MN34130_write_cmos_sensor(0x3070, frame_length & 0xFF);
		}
		else if( SENSOR_MODE_VIDEO == mn34130.sensorMode )
		{
			line_length = MN34130_VIDEO_PERIOD_PIXEL_NUMS + mn34130.DummyPixels;
			frame_length = MN34130_VIDEO_PERIOD_LINE_NUMS + mn34130.DummyLines + extra_lines ;
			//MN34130_write_cmos_sensor(0x306f, (frame_length >> 8) & 0xFF);
			//MN34130_write_cmos_sensor(0x3070, frame_length & 0xFF);
		}
		else	
		{
			line_length = MN34130_FULL_PERIOD_PIXEL_NUMS + mn34130.DummyPixels;
			frame_length = MN34130_FULL_PERIOD_LINE_NUMS + mn34130.DummyLines + extra_lines ;
			//MN34130_write_cmos_sensor(0x3089, (frame_length >> 8) & 0xFF);
			//MN34130_write_cmos_sensor(0x308A, frame_length & 0xFF);
		}

		ASSERT(line_length < MN34130_MAX_LINE_LENGTH);		//0xCCCC
		ASSERT(frame_length < MN34130_MAX_FRAME_LENGTH); 	//0xFFFF

		//Set total frame length
		if ( SENSOR_MODE_PREVIEW == mn34130.sensorMode )	//SXGA size output
		{
			//line_length = MN34130_PV_PERIOD_PIXEL_NUMS + mn34130.DummyPixels;
			//frame_length = MN34130_PV_PERIOD_LINE_NUMS+ mn34130.DummyLines + extra_lines ;
			MN34130_write_cmos_sensor(0x306f, (frame_length >> 8) & 0xFF);
			MN34130_write_cmos_sensor(0x3070, frame_length & 0xFF);
		}
		else if( SENSOR_MODE_VIDEO == mn34130.sensorMode )
		{
			//line_length = MN34130_VIDEO_PERIOD_PIXEL_NUMS + mn34130.DummyPixels;
			//frame_length = MN34130_VIDEO_PERIOD_LINE_NUMS + mn34130.DummyLines + extra_lines ;
			MN34130_write_cmos_sensor(0x306f, (frame_length >> 8) & 0xFF);
			MN34130_write_cmos_sensor(0x3070, frame_length & 0xFF);
		}
		else	
		{
			//line_length = MN34130_FULL_PERIOD_PIXEL_NUMS + mn34130.DummyPixels;
			//frame_length = MN34130_FULL_PERIOD_LINE_NUMS + mn34130.DummyLines + extra_lines ;
			MN34130_write_cmos_sensor(0x3089, (frame_length >> 8) & 0xFF);
			MN34130_write_cmos_sensor(0x308A, frame_length & 0xFF);
		}

		spin_lock_irqsave(&mn34130mipiraw_drv_lock,flags);
		mn34130.maxExposureLines = frame_length -4;
		MN34130_FeatureControl_PERIOD_PixelNum = line_length;
		MN34130_FeatureControl_PERIOD_LineNum = frame_length;
		spin_unlock_irqrestore(&mn34130mipiraw_drv_lock,flags);

		//Set shutter (Coarse integration time, uint: lines.)
		MN34130_write_cmos_sensor(0x0202, (shutter>>8) & 0xFF);
		MN34130_write_cmos_sensor(0x0203, shutter & 0xFF);

		MN34130DB("framerate(10 base) = %d\n", (mn34130.pvPclk*10000)*10 /line_length/frame_length);

		MN34130DB("shutter=%d, extra_lines=%d, line_length=%d, frame_length=%d\n", shutter, extra_lines, line_length, frame_length);
	}

}   /* write_MN34130_shutter */

/*******************************************************************************
*
********************************************************************************/
static kal_uint16 MN34130Reg2Gain(const kal_uint16 iReg)
{
	kal_uint16 i,iGain,totalCnt;    // 1x-gain base
	totalCnt = 232;
	for(i=0;i<totalCnt;i++)
	{
		if(mn34130_gaintable[i][1]==iReg)
			{
			iGain=mn34130_gaintable[i][0];
			break;
			}
	}
	MN34130DBSOFIA("[MN34130Reg2Gain]real gain= %d\n", iGain);
	return iGain; //mn34130.realGain
}

/*******************************************************************************
*
********************************************************************************/
static kal_uint16 MN34130Gain2Reg(const kal_uint16 Gain)
{
    kal_uint16 totalCnt,i,iGain=256;
	totalCnt = 232;
	MN34130DB("[MN34130Gain2Reg]: totalCnt: 0x%d\n", totalCnt);
	for(i=0;i<totalCnt;i++)
	 {
	 if (mn34130_gaintable[i][0]==Gain)
	 	{
	 	iGain=mn34130_gaintable[i][1];
	    return iGain;
	 	}
	 }
	//MN34130DB("[MN34130Gain2Reg]: isp gain:%d\n", iGain);
	//iGain = 256;

	return iGain ;
}

/*******************************************************************************
*
********************************************************************************/
void write_MN34130_gain(kal_uint16 gain)
{
	MN34130DB("[write_MN34130_gain]: gain write: 0x%x\n", gain);
	MN34130_write_cmos_sensor(0x0204, (gain>>8));
	MN34130_write_cmos_sensor(0x0205, (gain&0xff));	
}

/*************************************************************************
* FUNCTION
*    MN34130_SetGain
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
void MN34130_SetGain(UINT16 iGain)
{
	unsigned long flags;
	spin_lock_irqsave(&mn34130mipiraw_drv_lock, flags);
	mn34130.realGain = iGain;
	mn34130.sensorGlobalGain = MN34130Gain2Reg(iGain);
	spin_unlock_irqrestore(&mn34130mipiraw_drv_lock,flags);
    //return;
	write_MN34130_gain(mn34130.sensorGlobalGain);
	MN34130DB("[MN34130_SetGain]mn34130.sensorGlobalGain=0x%x,mn34130.realGain=%d\n", mn34130.sensorGlobalGain, mn34130.realGain);

}   /*  MN34130_SetGain_SetGain  */


/*************************************************************************
* FUNCTION
*    read_MN34130_gain
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
kal_uint16 read_MN34130_gain(void)
{
	kal_uint16 read_gain = 0;

	read_gain = ((MN34130_read_cmos_sensor(0x0204) << 8) | MN34130_read_cmos_sensor(0x0205));

	spin_lock(&mn34130mipiraw_drv_lock);
	mn34130.sensorGlobalGain = read_gain;
	mn34130.realGain = MN34130Reg2Gain(mn34130.sensorGlobalGain);
	spin_unlock(&mn34130mipiraw_drv_lock);

	MN34130DB("mn34130.sensorGlobalGain=0x%x,mn34130.realGain=%d\n", mn34130.sensorGlobalGain, mn34130.realGain);

	return mn34130.sensorGlobalGain;
}  /* read_MN34130_gain */


/*************************************************************************
* FUNCTION
*    MN34130_camera_para_to_sensor
*************************************************************************/
void MN34130_camera_para_to_sensor(void)
{
    kal_uint32    i;
    for(i=0; 0xFFFFFFFF!=MN34130SensorReg[i].Addr; i++)
    {
        MN34130_write_cmos_sensor(MN34130SensorReg[i].Addr, MN34130SensorReg[i].Para);
    }
#if 0	
    for(i=ENGINEER_START_ADDR; 0xFFFFFFFF!=MN34130SensorReg[i].Addr; i++)
    {
        MN34130_write_cmos_sensor(MN34130SensorReg[i].Addr, MN34130SensorReg[i].Para);
    }
    for(i=FACTORY_START_ADDR; i<FACTORY_END_ADDR; i++)
    {
        MN34130_write_cmos_sensor(MN34130SensorCCT[i].Addr, MN34130SensorCCT[i].Para);
    }
#endif	
}


/*************************************************************************
* FUNCTION
*    MN34130_sensor_to_camera_para
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
void MN34130_sensor_to_camera_para(void)
{
    kal_uint32    i, temp_data;
    for(i=0; 0xFFFFFFFF!=MN34130SensorReg[i].Addr; i++)
    {
         temp_data = MN34130_read_cmos_sensor(MN34130SensorReg[i].Addr);
		 spin_lock(&mn34130mipiraw_drv_lock);
		 MN34130SensorReg[i].Para =temp_data;
		 spin_unlock(&mn34130mipiraw_drv_lock);
    }
    for(i=ENGINEER_START_ADDR; 0xFFFFFFFF!=MN34130SensorReg[i].Addr; i++)
    {
        temp_data = MN34130_read_cmos_sensor(MN34130SensorReg[i].Addr);
		spin_lock(&mn34130mipiraw_drv_lock);
		MN34130SensorReg[i].Para = temp_data;
		spin_unlock(&mn34130mipiraw_drv_lock);
    }
}

/*************************************************************************
* FUNCTION
*    MN34130_get_sensor_group_count
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
kal_int32  MN34130_get_sensor_group_count(void)
{
    return GROUP_TOTAL_NUMS;
}

void MN34130_get_sensor_group_info(kal_uint16 group_idx, kal_int8* group_name_ptr, kal_int32* item_count_ptr)
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

void MN34130_get_sensor_item_info(kal_uint16 group_idx,kal_uint16 item_idx, MSDK_SENSOR_ITEM_INFO_STRUCT* info_ptr)
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

            temp_para= MN34130SensorCCT[temp_addr].Para;
			//temp_gain= (temp_para/mn34130.sensorBaseGain) * 1000;

            info_ptr->ItemValue=temp_gain;
            info_ptr->IsTrueFalse=KAL_FALSE;
            info_ptr->IsReadOnly=KAL_FALSE;
            info_ptr->IsNeedRestart=KAL_FALSE;
            info_ptr->Min= MN34130_MIN_ANALOG_GAIN * 1000;
            info_ptr->Max= MN34130_MAX_ANALOG_GAIN * 1000;
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



kal_bool MN34130_set_sensor_item_info(kal_uint16 group_idx, kal_uint16 item_idx, kal_int32 ItemValue)
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
//             temp_para=(temp_gain * mn34130.sensorBaseGain + BASEGAIN/2)/BASEGAIN;
          }
          else
			  ASSERT(0);

			 MN34130DBSOFIA("MN34130????????????????????? :\n ");
		  spin_lock(&mn34130mipiraw_drv_lock);
          MN34130SensorCCT[temp_addr].Para = temp_para;
		  spin_unlock(&mn34130mipiraw_drv_lock);
          MN34130_write_cmos_sensor(MN34130SensorCCT[temp_addr].Addr,temp_para);

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
					spin_lock(&mn34130mipiraw_drv_lock);
                    MN34130_FAC_SENSOR_REG=ItemValue;
					spin_unlock(&mn34130mipiraw_drv_lock);
                    break;
                case 1:
                    MN34130_write_cmos_sensor(MN34130_FAC_SENSOR_REG,ItemValue);
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

static int dbg_flag = 0;

static void MN34130_SetDummy( const kal_uint32 iPixels, const kal_uint32 iLines )
{
	kal_uint32 line_length = 0;
	kal_uint32 frame_length = 0;


	if ( SENSOR_MODE_PREVIEW == mn34130.sensorMode )
	{
		line_length = MN34130_PV_PERIOD_PIXEL_NUMS + iPixels;
		frame_length = MN34130_PV_PERIOD_LINE_NUMS + iLines;
		MN34130_write_cmos_sensor(0x306f, (frame_length >> 8) & 0xFF);
	    MN34130_write_cmos_sensor(0x3070, frame_length & 0xFF);
	}
	else if( SENSOR_MODE_VIDEO== mn34130.sensorMode )
	{
		line_length = MN34130_VIDEO_PERIOD_PIXEL_NUMS + iPixels;
		frame_length = MN34130_VIDEO_PERIOD_LINE_NUMS + iLines;
	    MN34130_write_cmos_sensor(0x306f, (frame_length >> 8) & 0xFF);
	    MN34130_write_cmos_sensor(0x3070, frame_length & 0xFF);
	}
	else
	{
		line_length = MN34130_FULL_PERIOD_PIXEL_NUMS + iPixels;
		frame_length = MN34130_FULL_PERIOD_LINE_NUMS + iLines;
		MN34130_write_cmos_sensor(0x3089, (frame_length >> 8) & 0xFF);
	    MN34130_write_cmos_sensor(0x308A, frame_length & 0xFF);
	}

	//if (dbg_flag)
		//return;

	//dbg_flag = 1;

	//if(mn34130.maxExposureLines > frame_length -4 )
	//	return;

	//ASSERT(line_length < MN34130_MAX_LINE_LENGTH);		//0xCCCC
	//ASSERT(frame_length < MN34130_MAX_FRAME_LENGTH);	//0xFFFF

	//Set total frame length
	//MN34130_write_cmos_sensor(0x0340, (frame_length >> 8) & 0xFF);
	//MN34130_write_cmos_sensor(0x0341, frame_length & 0xFF);
	
	//MN34130_write_cmos_sensor(0x306f, (frame_length >> 8) & 0xFF);
	//MN34130_write_cmos_sensor(0x3070, frame_length & 0xFF);

	spin_lock(&mn34130mipiraw_drv_lock);
	mn34130.maxExposureLines = frame_length -4;
	MN34130_FeatureControl_PERIOD_PixelNum = line_length;
	MN34130_FeatureControl_PERIOD_LineNum = frame_length;
	spin_unlock(&mn34130mipiraw_drv_lock);

	//Set total line length
	//MN34130_write_cmos_sensor(0x0342, (line_length >> 8) & 0xFF);
	//MN34130_write_cmos_sensor(0x0343, line_length & 0xFF);

}   /*  MN34130_SetDummy */

void MN34130PreviewSetting(void)
{
	MN34130DB("MN34130PreviewSetting_4lane_30fps enter :\n ");
	
	MN34130_write_cmos_sensor(0x0104, 0x01);// software reset
	MN34130_write_cmos_sensor(0x3319, 0x15);// software reset release

	MN34130_write_cmos_sensor(0x30BB, 0x00);
	MN34130_write_cmos_sensor(0x104, 0x00); // Half scan mode

	MN34130DB("MN34130PreviewSetting exit :\n ");
}


void MN34130VideoSetting(void)
{
	MN34130DB("MN34130VideoSetting enter :\n ");
	
	MN34130_write_cmos_sensor(0x0104, 0x01);// 
	MN34130_write_cmos_sensor(0x3319, 0x15);//

	MN34130_write_cmos_sensor(0x30BB, 0x00);
	MN34130_write_cmos_sensor(0x104, 0x00); // 

	//Sensor Driver Setting (TG Setting)

	MN34130DB("MN34130VideoSetting exit :\n ");
}


void MN34130CaptureSetting(void)
{
	MN34130DB("MN34130CaptureSetting_4lane_15fps enter :\n ");

	MN34130_write_cmos_sensor(0x0104, 0x01);// software reset		
	MN34130_write_cmos_sensor(0x3319, 0x29);// software reset release
	MN34130DB("MN34130PreviewSetting_4lane_30fps reset :\n ");

	MN34130_write_cmos_sensor(0x30BB, 0x01);
	MN34130_write_cmos_sensor(0x104, 0x00); // Half scan mode
	


	//MN34130_write_cmos_sensor(0x305B, 0x13);
	
	MN34130DB("MN34130CaptureSetting_4lane_15fps exit :\n ");
}

static void MN34130_Sensor_Init(void)
{
	MN34130DB("MN34130_Sensor_Init enter :\n ");

	//Sleep(20);

	MN34130_write_cmos_sensor(0x0304,0x00);
	MN34130_write_cmos_sensor(0x0305,0x01);
	MN34130_write_cmos_sensor(0x0306,0x00);
	MN34130_write_cmos_sensor(0x0307,0x36);
	MN34130_write_cmos_sensor(0x3000,0x01);
	MN34130_write_cmos_sensor(0x3004,0x02);
	MN34130_write_cmos_sensor(0x3005,0xA3);
	MN34130_write_cmos_sensor(0x3000,0x03);
	MN34130_write_cmos_sensor(0x300A,0x01);
	MN34130_write_cmos_sensor(0x3006,0x80);
	MN34130_write_cmos_sensor(0x300A,0x00);
	MN34130_write_cmos_sensor(0x3000,0x43);
	MN34130_write_cmos_sensor(0x0202,0x06);
	MN34130_write_cmos_sensor(0x0203,0xC7);
	MN34130_write_cmos_sensor(0x3006,0x00);
	MN34130_write_cmos_sensor(0x3016,0x23);
	//MN34130_write_cmos_sensor(0x3018,0x04);//add for power noise reduce
	//MN34130_write_cmos_sensor(0x301C,0x57);//add
	//MN34130_write_cmos_sensor(0x301E,0x54);//add
	MN34130_write_cmos_sensor(0x3020,0x00);
	MN34130_write_cmos_sensor(0x3021,0x00);
	MN34130_write_cmos_sensor(0x3022,0x00);
	MN34130_write_cmos_sensor(0x3023,0x00);
	MN34130_write_cmos_sensor(0x3028,0xF1);
	MN34130_write_cmos_sensor(0x3031,0x20);
	MN34130_write_cmos_sensor(0x3034,0x0E);
	MN34130_write_cmos_sensor(0x303D,0x01);
	MN34130_write_cmos_sensor(0x303E,0x02);
	MN34130_write_cmos_sensor(0x304F,0xFF);
	MN34130_write_cmos_sensor(0x305B,0x01);
	MN34130_write_cmos_sensor(0x3070,0x60);//7A->60 mode1 v_blanking 0x306f,3070[0x660]
	MN34130_write_cmos_sensor(0x3071,0x14);
	MN34130_write_cmos_sensor(0x3072,0x58);
	MN34130_write_cmos_sensor(0x3089,0x0C);//mode2 v_blanking 0x3089,308A
	MN34130_write_cmos_sensor(0x308A,0xF4);
	MN34130_write_cmos_sensor(0x308B,0x14);
	MN34130_write_cmos_sensor(0x308C,0x58);
	MN34130_write_cmos_sensor(0x30A1,0x05);
	MN34130_write_cmos_sensor(0x30A3,0x06);
	MN34130_write_cmos_sensor(0x30A4,0x7A);
	MN34130_write_cmos_sensor(0x30A5,0x14);
	MN34130_write_cmos_sensor(0x30A6,0x58);
	MN34130_write_cmos_sensor(0x30A8,0x00);
	MN34130_write_cmos_sensor(0x30A9,0x01);
	MN34130_write_cmos_sensor(0x30AA,0x8C);
	MN34130_write_cmos_sensor(0x30AC,0x00);
	MN34130_write_cmos_sensor(0x30AD,0x0A);
	MN34130_write_cmos_sensor(0x30AE,0xD3);
	MN34130_write_cmos_sensor(0x30AF,0x08);
	MN34130_write_cmos_sensor(0x30B0,0x40);
	MN34130_write_cmos_sensor(0x30B1,0x04);
	MN34130_write_cmos_sensor(0x30B2,0xA4);
	MN34130_write_cmos_sensor(0x30B4,0x01);
	MN34130_write_cmos_sensor(0x30B8,0x01);
	MN34130_write_cmos_sensor(0x30BC,0x01);
	MN34130_write_cmos_sensor(0x30BE,0x04);
	MN34130_write_cmos_sensor(0x30C2,0x42);
	MN34130_write_cmos_sensor(0x30C3,0x9C);
	MN34130_write_cmos_sensor(0x30C5,0x01);
	MN34130_write_cmos_sensor(0x30CB,0x40);
	MN34130_write_cmos_sensor(0x30CC,0x00);
	MN34130_write_cmos_sensor(0x30CD,0x01);
	MN34130_write_cmos_sensor(0x30CE,0x01);
	MN34130_write_cmos_sensor(0x30D0,0x04);
	MN34130_write_cmos_sensor(0x30D1,0x01);
	MN34130_write_cmos_sensor(0x30D2,0x00);
	MN34130_write_cmos_sensor(0x30D4,0x42);
	MN34130_write_cmos_sensor(0x30D5,0x9C);
	MN34130_write_cmos_sensor(0x3272,0x1F);
	MN34130_write_cmos_sensor(0x3276,0x2C);//2C->22->2C
	MN34130_write_cmos_sensor(0x3278,0x27);
	MN34130_write_cmos_sensor(0x3280,0x0B);
	MN34130_write_cmos_sensor(0x329A,0x78);
	MN34130_write_cmos_sensor(0x3319,0x15);
	MN34130_write_cmos_sensor(0x3345,0x2F);
	MN34130_write_cmos_sensor(0x3348,0x0B);
	MN34130_write_cmos_sensor(0x334B,0x19);
	MN34130_write_cmos_sensor(0x334D,0x19);
	MN34130_write_cmos_sensor(0x334F,0x19);
	MN34130_write_cmos_sensor(0x33C2,0x23);
	MN34130_write_cmos_sensor(0x3408,0x00);
	MN34130_write_cmos_sensor(0x3409,0x00);
	MN34130_write_cmos_sensor(0x340A,0x37);
	MN34130_write_cmos_sensor(0x340B,0x38);
	MN34130_write_cmos_sensor(0x340C,0x88);
	MN34130_write_cmos_sensor(0x340D,0x01);
	MN34130_write_cmos_sensor(0x340E,0x00);
	MN34130_write_cmos_sensor(0x340F,0x01);
	MN34130_write_cmos_sensor(0x3410,0x97);
	MN34130_write_cmos_sensor(0x3411,0x00);
	MN34130_write_cmos_sensor(0x3412,0xBC);
	MN34130_write_cmos_sensor(0x3413,0xC5);
	MN34130_write_cmos_sensor(0x3414,0x10);
	MN34130_write_cmos_sensor(0x3415,0x7A);
	MN34130_write_cmos_sensor(0x3416,0x90);
	MN34130_write_cmos_sensor(0x3417,0x20);
	MN34130_write_cmos_sensor(0x3418,0xC0);
	MN34130_write_cmos_sensor(0x3419,0x18);
	MN34130_write_cmos_sensor(0x341A,0x0D);
	MN34130_write_cmos_sensor(0x341B,0xF0);
	MN34130_write_cmos_sensor(0x341C,0x00);
	MN34130_write_cmos_sensor(0x341D,0x24);
	MN34130_write_cmos_sensor(0x341E,0x70);
	MN34130_write_cmos_sensor(0x341F,0x40);
	MN34130_write_cmos_sensor(0x3420,0x34);
	MN34130_write_cmos_sensor(0x3421,0x04);
	MN34130_write_cmos_sensor(0x3422,0xC1);
	MN34130_write_cmos_sensor(0x3423,0x37);
	MN34130_write_cmos_sensor(0x3424,0x38);
	MN34130_write_cmos_sensor(0x3425,0x88);
	MN34130_write_cmos_sensor(0x3426,0x01);
	MN34130_write_cmos_sensor(0x3427,0x00);
	MN34130_write_cmos_sensor(0x3428,0x01);
	MN34130_write_cmos_sensor(0x3429,0x97);
	MN34130_write_cmos_sensor(0x342A,0x00);
	MN34130_write_cmos_sensor(0x342B,0xBC);
	MN34130_write_cmos_sensor(0x342C,0xC5);
	MN34130_write_cmos_sensor(0x342D,0x10);
	MN34130_write_cmos_sensor(0x342E,0x7A);
	MN34130_write_cmos_sensor(0x342F,0x90);
	MN34130_write_cmos_sensor(0x3430,0x20);
	MN34130_write_cmos_sensor(0x3431,0xC0);
	MN34130_write_cmos_sensor(0x3432,0x18);
	MN34130_write_cmos_sensor(0x3433,0x0D);
	MN34130_write_cmos_sensor(0x3434,0xF0);
	MN34130_write_cmos_sensor(0x3435,0x00);
	MN34130_write_cmos_sensor(0x3436,0x24);
	MN34130_write_cmos_sensor(0x3437,0x70);
	MN34130_write_cmos_sensor(0x3438,0x40);
	MN34130_write_cmos_sensor(0x3439,0x34);
	MN34130_write_cmos_sensor(0x343A,0x04);
	MN34130_write_cmos_sensor(0x343B,0xC1);
	MN34130_write_cmos_sensor(0x343C,0xC6);
	MN34130_write_cmos_sensor(0x343D,0x80);
	MN34130_write_cmos_sensor(0x343E,0x06);
	MN34130_write_cmos_sensor(0x343F,0x1F);
	MN34130_write_cmos_sensor(0x3440,0x26);
	MN34130_write_cmos_sensor(0x3441,0x30);
	MN34130_write_cmos_sensor(0x3442,0x0A);
	MN34130_write_cmos_sensor(0x3443,0x10);
	MN34130_write_cmos_sensor(0x3444,0xC4);
	MN34130_write_cmos_sensor(0x3445,0xE5);
	MN34130_write_cmos_sensor(0x3446,0xB7);
	MN34130_write_cmos_sensor(0x3447,0x89);
	MN34130_write_cmos_sensor(0x3448,0xDB);
	MN34130_write_cmos_sensor(0x3449,0x0F);
	MN34130_write_cmos_sensor(0x344A,0x46);
	MN34130_write_cmos_sensor(0x344B,0x8B);
	MN34130_write_cmos_sensor(0x344C,0x8F);
	MN34130_write_cmos_sensor(0x344D,0x83);
	MN34130_write_cmos_sensor(0x344E,0x00);
	MN34130_write_cmos_sensor(0x344F,0xD0);
	MN34130_write_cmos_sensor(0x3450,0x20);
	MN34130_write_cmos_sensor(0x3451,0x75);
	MN34130_write_cmos_sensor(0x3452,0x19);
	MN34130_write_cmos_sensor(0x3453,0x1A);
	MN34130_write_cmos_sensor(0x3454,0xC6);
	MN34130_write_cmos_sensor(0x3455,0x80);
	MN34130_write_cmos_sensor(0x3456,0x06);
	MN34130_write_cmos_sensor(0x3457,0x1F);
	MN34130_write_cmos_sensor(0x3458,0x26);
	MN34130_write_cmos_sensor(0x3459,0x30);
	MN34130_write_cmos_sensor(0x345A,0x0A);
	MN34130_write_cmos_sensor(0x345B,0x10);
	MN34130_write_cmos_sensor(0x345C,0xC4);
	MN34130_write_cmos_sensor(0x345D,0xE5);
	MN34130_write_cmos_sensor(0x345E,0xB7);
	MN34130_write_cmos_sensor(0x345F,0x89);
	MN34130_write_cmos_sensor(0x3460,0xDB);
	MN34130_write_cmos_sensor(0x3461,0x0F);
	MN34130_write_cmos_sensor(0x3462,0x46);
	MN34130_write_cmos_sensor(0x3463,0x8B);
	MN34130_write_cmos_sensor(0x3464,0x8F);
	MN34130_write_cmos_sensor(0x3465,0x83);
	MN34130_write_cmos_sensor(0x3466,0x00);
	MN34130_write_cmos_sensor(0x3467,0xD0);
	MN34130_write_cmos_sensor(0x3468,0x20);
	MN34130_write_cmos_sensor(0x3469,0x75);
	MN34130_write_cmos_sensor(0x346A,0x19);
	MN34130_write_cmos_sensor(0x346B,0x1A);
	MN34130_write_cmos_sensor(0x346C,0x13);
	MN34130_write_cmos_sensor(0x346D,0x03);
	MN34130_write_cmos_sensor(0x346E,0x42);
	MN34130_write_cmos_sensor(0x346F,0x42);
	MN34130_write_cmos_sensor(0x3470,0x00);
	MN34130_write_cmos_sensor(0x3471,0x00);
	MN34130_write_cmos_sensor(0x3472,0x1E);
	MN34130_write_cmos_sensor(0x3473,0x1F);
	MN34130_write_cmos_sensor(0x3474,0x00);
	MN34130_write_cmos_sensor(0x3475,0x1E);
	MN34130_write_cmos_sensor(0x3476,0x13);
	MN34130_write_cmos_sensor(0x3477,0x03);
	MN34130_write_cmos_sensor(0x3478,0x42);
	MN34130_write_cmos_sensor(0x3479,0x42);
	MN34130_write_cmos_sensor(0x347A,0x00);
	MN34130_write_cmos_sensor(0x347B,0x00);
	MN34130_write_cmos_sensor(0x347C,0x1E);
	MN34130_write_cmos_sensor(0x347D,0x1F);
	MN34130_write_cmos_sensor(0x347E,0x00);
	MN34130_write_cmos_sensor(0x347F,0x1E);
	MN34130_write_cmos_sensor(0x3480,0x1C);
	MN34130_write_cmos_sensor(0x3481,0x37);
	MN34130_write_cmos_sensor(0x3482,0x03);
	MN34130_write_cmos_sensor(0x3483,0x73);
	MN34130_write_cmos_sensor(0x3484,0x21);
	MN34130_write_cmos_sensor(0x3485,0x01);
	MN34130_write_cmos_sensor(0x3486,0x00);
	MN34130_write_cmos_sensor(0x3487,0x00);
	MN34130_write_cmos_sensor(0x3488,0x00);
	MN34130_write_cmos_sensor(0x3489,0x3C);
	MN34130_write_cmos_sensor(0x348A,0x07);
	MN34130_write_cmos_sensor(0x348B,0x44);
	MN34130_write_cmos_sensor(0x348C,0x5C);
	MN34130_write_cmos_sensor(0x348D,0x00);
	MN34130_write_cmos_sensor(0x348E,0x00);
	MN34130_write_cmos_sensor(0x348F,0x00);
	MN34130_write_cmos_sensor(0x3490,0x39);//39->38->39
	MN34130_write_cmos_sensor(0x3491,0x00);//00->DF->00
	MN34130_write_cmos_sensor(0x3492,0x55);
	MN34130_write_cmos_sensor(0x3493,0x00);
	MN34130_write_cmos_sensor(0x3494,0x00);
	MN34130_write_cmos_sensor(0x3495,0x00);
	MN34130_write_cmos_sensor(0x3496,0x00);
	MN34130_write_cmos_sensor(0x3497,0x71);//71->70->71
	MN34130_write_cmos_sensor(0x3498,0x00);//00->DF->00
	MN34130_write_cmos_sensor(0x3499,0x55);
	MN34130_write_cmos_sensor(0x349A,0x00);
	MN34130_write_cmos_sensor(0x349B,0x00);
	MN34130_write_cmos_sensor(0x349C,0x00);
	MN34130_write_cmos_sensor(0x349D,0x00);
	MN34130_write_cmos_sensor(0x349E,0xB5);//B5->B4->B5
	MN34130_write_cmos_sensor(0x349F,0x00);//00->DF->00
	MN34130_write_cmos_sensor(0x34A0,0x55);
	MN34130_write_cmos_sensor(0x34A1,0x00);
	MN34130_write_cmos_sensor(0x34A2,0x00);
	MN34130_write_cmos_sensor(0x34A3,0x00);
	MN34130_write_cmos_sensor(0x34A4,0x00);
	MN34130_write_cmos_sensor(0x34A5,0x88);
	MN34130_write_cmos_sensor(0x34A6,0x88);
	MN34130_write_cmos_sensor(0x34A7,0x88);
	MN34130_write_cmos_sensor(0x34A8,0x88);
	MN34130_write_cmos_sensor(0x34A9,0x28);
	MN34130_write_cmos_sensor(0x34AA,0x5B);
	MN34130_write_cmos_sensor(0x34AB,0xB7);
	MN34130_write_cmos_sensor(0x34AC,0x5B);
	MN34130_write_cmos_sensor(0x34AD,0xB7);
	MN34130_write_cmos_sensor(0x34AE,0x5B);
	MN34130_write_cmos_sensor(0x34AF,0xB7);
	MN34130_write_cmos_sensor(0x34B0,0x8B);
	MN34130_write_cmos_sensor(0x3627,0x37);
	MN34130_write_cmos_sensor(0x3708,0xC7);
	MN34130_write_cmos_sensor(0x370F,0x11);
	MN34130_write_cmos_sensor(0x3721,0x38);
	MN34130_write_cmos_sensor(0x3723,0x62);
	MN34130_write_cmos_sensor(0x372D,0x31);
	MN34130_write_cmos_sensor(0x372E,0x08);
	MN34130_write_cmos_sensor(0x374A,0x2F);
	MN34130_write_cmos_sensor(0x375E,0x0F);
	MN34130_write_cmos_sensor(0x375F,0x18);
	MN34130_write_cmos_sensor(0x376C,0x0D);
	MN34130_write_cmos_sensor(0x3045,0x01);//enable shading calibration
	MN34130_write_cmos_sensor(0x0100,0x01);
	MN34130_write_cmos_sensor(0x3000,0xF3);


//												
	
	MN34130DB("MN34130_Sensor_Init exit :\n ");
}   /*  MN34130_Sensor_Init  */

/*************************************************************************
* FUNCTION
*   MN34130Open
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

UINT32 MN34130Open(void)
{

	volatile signed int i;
	kal_uint16 sensor_id = 0;

	MN34130DB("MN34130Open enter :\n ");
    	//Sleep(0x10);
	//MN34130_write_cmos_sensor(0x0103, 0x01);// Reset sensor
    	//Sleep(0x10);
	//g_NeedSetPLL = KAL_TRUE;

	//  Read sensor ID to adjust I2C is OK?

	for(i=3; i>0; i--)
	{
		sensor_id = MN34130_read_cmos_sensor(0x0001);
		MN34130DB("MN34130 READ sensor id :0x%x", sensor_id);		
		if(sensor_id == MN34130_SENSOR_ID)
		{
			MN34130DB("OMN34130 READ ID successfully.");
            break;
		}
	}

	//msleep(5);

	if (i == 0)
	{
		MN34130DB("MN34130 READ ID failed.");
        return ERROR_SENSOR_CONNECT_FAIL;
    }
	spin_lock(&mn34130mipiraw_drv_lock);
	mn34130.sensorMode = SENSOR_MODE_INIT;
	mn34130.MN34130AutoFlickerMode = KAL_FALSE;
	mn34130.MN34130VideoMode = KAL_FALSE;
	spin_unlock(&mn34130mipiraw_drv_lock);
	MN34130_Sensor_Init();

	spin_lock(&mn34130mipiraw_drv_lock);
	mn34130.DummyLines= 0;
	mn34130.DummyPixels= 0;
	//mn34130.pvPclk =  25920;//12960;   //Clk all check with 4lane setting
	mn34130.pvPclk =  12960;   //Clk all check with 4lane setting
	mn34130.videoPclk = 12960;

	mn34130.capPclk = 25920;

	mn34130.shutter = 0x4EA;
	mn34130.pvShutter = 0x4EA;
	mn34130.maxExposureLines =MN34130_PV_PERIOD_LINE_NUMS -4;

	mn34130.ispBaseGain = BASEGAIN;//0x40
	mn34130.sensorGlobalGain = 0x0100;
	mn34130.pvGain = 0x0100;
	mn34130.realGain = MN34130Reg2Gain(0x0100);//ispBaseGain as 1x
	spin_unlock(&mn34130mipiraw_drv_lock);
	//MN34130DB("MN34130Reg2Gain(0x1f)=%x :\n ",MN34130Reg2Gain(0x1f));

	MN34130DB("MN34130Open exit :\n ");

    return ERROR_NONE;
}

/*************************************************************************
* FUNCTION
*   MN34130GetSensorID
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
UINT32 MN34130GetSensorID(UINT32 *sensorID)
{
	int  i;
	UINT32 sensor_id = 0;

	MN34130DB("MN34130GetSensorID enter :\n ");

	// check if sensor ID correct
	//  Read sensor ID to adjust I2C is OK?
	for(i=3; i>0; i--)
	{
		//MN34130_write_cmos_sensor(0x0205, 0x11);
		sensor_id = MN34130_read_cmos_sensor(0x0001);
		MN34130DB("OMN34130 READ sensor id :0x%x", sensor_id);		
		if(sensor_id == MN34130_SENSOR_ID)
		{
			MN34130DB("OMN34130 READ ID successfully.");
			*sensorID = MN34130_SENSOR_ID;
			return ERROR_NONE;
		}
	}

	//msleep(5);
#if 1
	if (i == 0)
	{
		MN34130DB("OMN34130 READ ID failed.");
       	*sensorID = 0xFFFFFFFF;
        	return ERROR_SENSOR_CONNECT_FAIL;
    	}


#endif	
	
    	return ERROR_NONE;
}


/*************************************************************************
* FUNCTION
*   MN34130_SetShutter
*
* DESCRIPTION
*   This function set e-shutter of MN34130 to change exposure time.
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
UINT32 MN34130_read_shutter(void);
void MN34130_SetShutter(kal_uint32 iShutter)
{
	if(MSDK_SCENARIO_ID_CAMERA_ZSD == MN34130CurrentScenarioId )
	{
		//MN34130DB("always UPDATE SHUTTER when mn34130.sensorMode == SENSOR_MODE_CAPTURE\n");
	}
	else
	{
		if(mn34130.sensorMode == SENSOR_MODE_CAPTURE)
		{
			//MN34130DB("capture!!DONT UPDATE SHUTTER!!\n");
			//return;
		}
	}
	MN34130DB("MN34130_SetShutter,%d\n",iShutter);
	MN34130DB("MN34130 read shutter 0x202,0x203 =%d\n",MN34130_read_shutter());
	
	if(mn34130.shutter == iShutter && mn34130.sensorMode!=SENSOR_MODE_CAPTURE)
		return;
	//return;
   	spin_lock(&mn34130mipiraw_drv_lock);
   	mn34130.shutter= iShutter;
	spin_unlock(&mn34130mipiraw_drv_lock);
	MN34130_write_shutter(iShutter);
	return;
}   /*  MN34130_SetShutter   */



/*************************************************************************
* FUNCTION
*   MN34130_read_shutter
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
UINT32 MN34130_read_shutter(void)
{

	kal_uint16 temp_reg1, temp_reg2;
	UINT32 shutter =0;
	temp_reg1 = MN34130_read_cmos_sensor(0x0202);
	temp_reg2 = MN34130_read_cmos_sensor(0x0203);
	//read out register value and divide 16;
	shutter  = (temp_reg1 <<8)| (temp_reg2);

	return shutter;
}

/*************************************************************************
* FUNCTION
*   MN34130_night_mode
*
* DESCRIPTION
*   This function night mode of MN34130.
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
void MN34130_NightMode(kal_bool bEnable)
{
}/*	MN34130_NightMode */



/*************************************************************************
* FUNCTION
*   MN34130Close
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
UINT32 MN34130Close(void)
{
    //  CISModulePowerOn(FALSE);
    //s_porting
    //  DRV_I2CClose(MN34130hDrvI2C);
    //e_porting
    return ERROR_NONE;
}	/* MN34130Close() */


/*************************************************************************
* FUNCTION
*   MN34130SetFlipMirror
*
*************************************************************************/
void MN34130SetFlipMirror(kal_int32 imgMirror)
{
	kal_uint8 flip_mirror = 0x00;
	
	flip_mirror = MN34130_read_cmos_sensor(0x0101);
	
	switch (imgMirror)
	{
	case IMAGE_H_MIRROR: //IMAGE_NORMAL:
		flip_mirror &= 0xFD;
		MN34130_write_cmos_sensor(0x0101, (flip_mirror | (0x01)));	//Set mirror
		break;
		
	case IMAGE_NORMAL://IMAGE_V_MIRROR:
		MN34130_write_cmos_sensor(0x0101, (flip_mirror & (0xFC)));//Set normal
		break;
		
	case IMAGE_HV_MIRROR://IMAGE_H_MIRROR:
		MN34130_write_cmos_sensor(0x0101, (flip_mirror | (0x03)));	//Set mirror & flip
		break;
		
	case IMAGE_V_MIRROR://IMAGE_HV_MIRROR:
		flip_mirror &= 0xFE;
		MN34130_write_cmos_sensor(0x0101, (flip_mirror | (0x02)));	//Set flip
		break;
	}
}


/*************************************************************************
* FUNCTION
*   MN34130Preview
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
UINT32 MN34130Preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                                                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	MN34130DB("MN34130Preview enter:");

	// preview size
	if(mn34130.sensorMode == SENSOR_MODE_PREVIEW)
	{
		// do nothing
		// FOR CCT PREVIEW
	}
	else
	{
		//MN34130DB("MN34130Preview setting!!\n");
		MN34130PreviewSetting();
	}
	
	spin_lock(&mn34130mipiraw_drv_lock);
	mn34130.sensorMode = SENSOR_MODE_PREVIEW; // Need set preview setting after capture mode
	mn34130.DummyPixels = 0;//define dummy pixels and lines
	mn34130.DummyLines = 0 ;
	MN34130_FeatureControl_PERIOD_PixelNum=MN34130_PV_PERIOD_PIXEL_NUMS+ mn34130.DummyPixels;
	MN34130_FeatureControl_PERIOD_LineNum=MN34130_PV_PERIOD_LINE_NUMS+mn34130.DummyLines;
	spin_unlock(&mn34130mipiraw_drv_lock);

	//MN34130_write_shutter(mn34130.shutter);
	//write_MN34130_gain(mn34130.pvGain);

	//set mirror & flip
	//MN34130DB("[MN34130Preview] mirror&flip: %d \n",sensor_config_data->SensorImageMirror);
	spin_lock(&mn34130mipiraw_drv_lock);
	mn34130.imgMirror = sensor_config_data->SensorImageMirror;
	spin_unlock(&mn34130mipiraw_drv_lock);
	MN34130SetFlipMirror(sensor_config_data->SensorImageMirror);

	//MN34130DBSOFIA("[MN34130Preview]frame_len=%x\n", ((MN34130_read_cmos_sensor(0x380e)<<8)+MN34130_read_cmos_sensor(0x380f)));

    //mDELAY(40);
	MN34130DB("MN34130Preview exit:\n");
    return ERROR_NONE;
}	/* MN34130Preview() */



UINT32 MN34130Video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                                                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	MN34130DB("MN34130Video enter:");

	if(mn34130.sensorMode == SENSOR_MODE_VIDEO)
	{
		// do nothing
	}
	else
	{
		MN34130VideoSetting();

	}
	spin_lock(&mn34130mipiraw_drv_lock);
	mn34130.sensorMode = SENSOR_MODE_VIDEO;
	mn34130.DummyPixels = 0;//define dummy pixels and lines
	mn34130.DummyLines = 0 ;
	MN34130_FeatureControl_PERIOD_PixelNum=MN34130_VIDEO_PERIOD_PIXEL_NUMS+ mn34130.DummyPixels;
	MN34130_FeatureControl_PERIOD_LineNum=MN34130_VIDEO_PERIOD_LINE_NUMS+mn34130.DummyLines;
	spin_unlock(&mn34130mipiraw_drv_lock);

	//MN34130_write_shutter(mn34130.shutter);
	//write_MN34130_gain(mn34130.pvGain);

	spin_lock(&mn34130mipiraw_drv_lock);
	mn34130.imgMirror = sensor_config_data->SensorImageMirror;
	spin_unlock(&mn34130mipiraw_drv_lock);
	MN34130SetFlipMirror(sensor_config_data->SensorImageMirror);

	//MN34130DBSOFIA("[MN34130Video]frame_len=%x\n", ((MN34130_read_cmos_sensor(0x380e)<<8)+MN34130_read_cmos_sensor(0x380f)));

    //mDELAY(40);
	MN34130DB("MN34130Video exit:\n");
    return ERROR_NONE;
}


UINT32 MN34130Capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                                                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

 	kal_uint32 shutter = mn34130.shutter;
	kal_uint32 temp_data;
	//kal_uint32 pv_line_length , cap_line_length,

	if( SENSOR_MODE_CAPTURE== mn34130.sensorMode)
	{
		MN34130DB("MN34130Capture BusrtShot!!!\n");
	}else{
		MN34130DB("MN34130Capture enter:\n");

		//Record Preview shutter & gain
		shutter=MN34130_read_shutter();
		temp_data =  read_MN34130_gain();
		spin_lock(&mn34130mipiraw_drv_lock);
		mn34130.pvShutter =shutter;
		mn34130.sensorGlobalGain = temp_data;
		mn34130.pvGain =mn34130.sensorGlobalGain;
		spin_unlock(&mn34130mipiraw_drv_lock);

		MN34130DB("[MN34130Capture]mn34130.shutter=%d, read_pv_shutter=%d, read_pv_gain = 0x%x\n",mn34130.shutter, shutter,mn34130.sensorGlobalGain);

		// Full size setting
		MN34130CaptureSetting();
	    //mDELAY(40);

		spin_lock(&mn34130mipiraw_drv_lock);
		mn34130.sensorMode = SENSOR_MODE_CAPTURE;
		mn34130.imgMirror = sensor_config_data->SensorImageMirror;
		mn34130.DummyPixels = 0;//define dummy pixels and lines
		mn34130.DummyLines = 0 ;
		MN34130_FeatureControl_PERIOD_PixelNum = MN34130_FULL_PERIOD_PIXEL_NUMS + mn34130.DummyPixels;
		MN34130_FeatureControl_PERIOD_LineNum = MN34130_FULL_PERIOD_LINE_NUMS + mn34130.DummyLines;
		spin_unlock(&mn34130mipiraw_drv_lock);

		//MN34130DB("[MN34130Capture] mirror&flip: %d\n",sensor_config_data->SensorImageMirror);
		MN34130SetFlipMirror(sensor_config_data->SensorImageMirror);

		//#if defined(MT6575)||defined(MT6577)
	    if(MN34130CurrentScenarioId==MSDK_SCENARIO_ID_CAMERA_ZSD)
	    {
			MN34130DB("MN34130Capture exit ZSD!!\n");
			return ERROR_NONE;
	    }
		//#endif

#if 0 //no need to calculate shutter from mt6589
		//calculate shutter
		pv_line_length = MN34130_PV_PERIOD_PIXEL_NUMS + mn34130.DummyPixels;
		cap_line_length = MN34130_FULL_PERIOD_PIXEL_NUMS + mn34130.DummyPixels;

		MN34130DB("[MN34130Capture]pv_line_length =%d,cap_line_length =%d\n",pv_line_length,cap_line_length);
		MN34130DB("[MN34130Capture]pv_shutter =%d\n",shutter );

		shutter =  shutter * pv_line_length / cap_line_length;
		shutter = shutter *mn34130.capPclk / mn34130.pvPclk;
		shutter *= 2; //preview bining///////////////////////////////////////

		if(shutter < 3)
		    shutter = 3;

		MN34130_write_shutter(shutter);

		//gain = read_MN34130_gain();

		MN34130DB("[MN34130Capture]cap_shutter =%d , cap_read gain = 0x%x\n",shutter,read_MN34130_gain());
		//write_MN34130_gain(mn34130.sensorGlobalGain);
#endif

		MN34130DB("MN34130Capture exit:\n");
	}

	return ERROR_NONE;
}	/* MN34130Capture() */

UINT32 MN34130GetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution)
{

	MN34130DB("MN34130GetResolution!!\n");

	pSensorResolution->SensorPreviewWidth	= MN34130_IMAGE_SENSOR_PV_WIDTH;
	pSensorResolution->SensorPreviewHeight	= MN34130_IMAGE_SENSOR_PV_HEIGHT;
	pSensorResolution->SensorFullWidth		= MN34130_IMAGE_SENSOR_FULL_WIDTH;
	pSensorResolution->SensorFullHeight		= MN34130_IMAGE_SENSOR_FULL_HEIGHT;
	pSensorResolution->SensorVideoWidth		= MN34130_IMAGE_SENSOR_VIDEO_WIDTH;
	pSensorResolution->SensorVideoHeight    = MN34130_IMAGE_SENSOR_VIDEO_HEIGHT;
	return ERROR_NONE;
}   /* MN34130GetResolution() */

UINT32 MN34130GetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId,
                                                MSDK_SENSOR_INFO_STRUCT *pSensorInfo,
                                                MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{

	pSensorInfo->SensorPreviewResolutionX= MN34130_IMAGE_SENSOR_PV_WIDTH;
	pSensorInfo->SensorPreviewResolutionY= MN34130_IMAGE_SENSOR_PV_HEIGHT;

	pSensorInfo->SensorFullResolutionX= MN34130_IMAGE_SENSOR_FULL_WIDTH;
	pSensorInfo->SensorFullResolutionY= MN34130_IMAGE_SENSOR_FULL_HEIGHT;

	spin_lock(&mn34130mipiraw_drv_lock);
	mn34130.imgMirror = pSensorConfigData->SensorImageMirror ;
	spin_unlock(&mn34130mipiraw_drv_lock);

   	pSensorInfo->SensorOutputDataFormat= SENSOR_OUTPUT_FORMAT_RAW_Gr;
	pSensorInfo->SensorClockPolarity =SENSOR_CLOCK_POLARITY_LOW;
	pSensorInfo->SensorClockFallingPolarity=SENSOR_CLOCK_POLARITY_LOW;
	pSensorInfo->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	pSensorInfo->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;

	pSensorInfo->SensroInterfaceType=SENSOR_INTERFACE_TYPE_MIPI;

	pSensorInfo->CaptureDelayFrame = 1;
	pSensorInfo->PreviewDelayFrame = 2;
	pSensorInfo->VideoDelayFrame = 2; // 1 ?

	pSensorInfo->SensorDrivingCurrent = ISP_DRIVING_2MA;
	pSensorInfo->AEShutDelayFrame = 0;//0;		    /* The frame of setting shutter default 0 for TG int */
	pSensorInfo->AESensorGainDelayFrame = 1 ;//0;     /* The frame of setting sensor gain */
	pSensorInfo->AEISPGainDelayFrame = 2;

	switch (ScenarioId)
	{
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		pSensorInfo->SensorClockFreq=24;
		pSensorInfo->SensorClockRisingCount= 0;

		pSensorInfo->SensorGrabStartX = MN34130_PV_X_START;
		pSensorInfo->SensorGrabStartY = MN34130_PV_Y_START;

		pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;
		pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
		pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 14;
	    pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
		pSensorInfo->SensorPacketECCOrder = 1;
		break;
		
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		pSensorInfo->SensorClockFreq=24;
		pSensorInfo->SensorClockRisingCount= 0;

		pSensorInfo->SensorGrabStartX = MN34130_VIDEO_X_START;
		pSensorInfo->SensorGrabStartY = MN34130_VIDEO_Y_START;

		pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;
		pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	    pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 14;
	    pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
		pSensorInfo->SensorPacketECCOrder = 1;
		break;
		
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	case MSDK_SCENARIO_ID_CAMERA_ZSD:
            pSensorInfo->SensorClockFreq=24;
            pSensorInfo->SensorClockRisingCount= 0;

            pSensorInfo->SensorGrabStartX = MN34130_FULL_X_START;	
            pSensorInfo->SensorGrabStartY = MN34130_FULL_Y_START;	

            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 14;
            pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorPacketECCOrder = 1;
            break;
			
	default:
		pSensorInfo->SensorClockFreq=24;
		pSensorInfo->SensorClockRisingCount= 0;

		pSensorInfo->SensorGrabStartX = MN34130_PV_X_START;
		pSensorInfo->SensorGrabStartY = MN34130_PV_Y_START;

		pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;
		pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	     	pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 14;
	    	pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
		pSensorInfo->SensorPacketECCOrder = 1;
		break;
	}

	memcpy(pSensorConfigData, &MN34130SensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));

	return ERROR_NONE;
}   /* MN34130GetInfo() */


UINT32 MN34130Control(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow,
                                                MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
		spin_lock(&mn34130mipiraw_drv_lock);
		MN34130CurrentScenarioId = ScenarioId;
		spin_unlock(&mn34130mipiraw_drv_lock);
		//MN34130DB("ScenarioId=%d\n",ScenarioId);
		MN34130DB("MN34130CurrentScenarioId=%d\n",MN34130CurrentScenarioId);

	switch (ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            MN34130Preview(pImageWindow, pSensorConfigData);
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			MN34130Video(pImageWindow, pSensorConfigData);
			break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
            MN34130Capture(pImageWindow, pSensorConfigData);
            break;

        default:
            return ERROR_INVALID_SCENARIO_ID;

    }
    return ERROR_NONE;
} /* MN34130Control() */


UINT32 MN34130SetVideoMode(UINT16 u2FrameRate)
{

    kal_uint32 MIN_Frame_length =0,frameRate=0,extralines=0;
    MN34130DB("[MN34130SetVideoMode] frame rate = %d\n", u2FrameRate);

	spin_lock(&mn34130mipiraw_drv_lock);
	VIDEO_MODE_TARGET_FPS=u2FrameRate;
	spin_unlock(&mn34130mipiraw_drv_lock);

	if(u2FrameRate==0)
	{
		MN34130DB("Disable Video Mode or dynimac fps\n");
		return KAL_TRUE;
	}
	if(u2FrameRate >30 || u2FrameRate <5)
	    MN34130DB("error frame rate seting\n");

    if(mn34130.sensorMode == SENSOR_MODE_VIDEO)//video ScenarioId recording
    {
    	if(mn34130.MN34130AutoFlickerMode == KAL_TRUE)
    	{
    		if (u2FrameRate==30)
				frameRate= 304;
			else if(u2FrameRate==15)
				frameRate= 148;//148;
			else
				frameRate=u2FrameRate*10;

			MIN_Frame_length = (mn34130.videoPclk*100000)/(MN34130_VIDEO_PERIOD_PIXEL_NUMS + mn34130.DummyPixels)/frameRate;
    	}
		else
			{
    		if (u2FrameRate==30)
				MIN_Frame_length = (mn34130.videoPclk*100000) /(MN34130_VIDEO_PERIOD_PIXEL_NUMS + mn34130.DummyPixels)/304;
            else
			    MIN_Frame_length = (mn34130.videoPclk*100000) /(MN34130_VIDEO_PERIOD_PIXEL_NUMS + mn34130.DummyPixels)/(u2FrameRate*10);
			}
		if((MIN_Frame_length <=MN34130_VIDEO_PERIOD_LINE_NUMS))
		{
			MIN_Frame_length = MN34130_VIDEO_PERIOD_LINE_NUMS;
			MN34130DB("[MN34130SetVideoMode]current fps = %d\n", (mn34130.videoPclk*10000)  /(MN34130_PV_PERIOD_PIXEL_NUMS)/MN34130_PV_PERIOD_LINE_NUMS);
		}
		MN34130DB("[MN34130SetVideoMode]current fps (10 base)= %d\n", (mn34130.videoPclk*10000)*10/(MN34130_PV_PERIOD_PIXEL_NUMS + mn34130.DummyPixels)/MIN_Frame_length);
		extralines = MIN_Frame_length - MN34130_VIDEO_PERIOD_LINE_NUMS;
		
		spin_lock(&mn34130mipiraw_drv_lock);
		mn34130.DummyPixels = 0;//define dummy pixels and lines
		mn34130.DummyLines = extralines ;
		spin_unlock(&mn34130mipiraw_drv_lock);
		
		MN34130_SetDummy(mn34130.DummyPixels,extralines);
    }
	else if(mn34130.sensorMode == SENSOR_MODE_CAPTURE)
	{
		MN34130DB("-------[MN34130SetVideoMode]ZSD???---------\n");
		if(mn34130.MN34130AutoFlickerMode == KAL_TRUE)
    	{
    		if (u2FrameRate==15)
			    frameRate= 148;
			else
				frameRate=u2FrameRate*10;

			MIN_Frame_length = (mn34130.capPclk*100000) /(MN34130_FULL_PERIOD_PIXEL_NUMS + mn34130.DummyPixels)/frameRate;
    	}
		else
			MIN_Frame_length = (mn34130.capPclk*100000) /(MN34130_FULL_PERIOD_PIXEL_NUMS + mn34130.DummyPixels)/(u2FrameRate*10);

		if((MIN_Frame_length <=MN34130_FULL_PERIOD_LINE_NUMS))
		{
			MIN_Frame_length = MN34130_FULL_PERIOD_LINE_NUMS;
			MN34130DB("[MN34130SetVideoMode]current fps = %d\n", (mn34130.capPclk*10000) /(MN34130_FULL_PERIOD_PIXEL_NUMS)/MN34130_FULL_PERIOD_LINE_NUMS);

		}
		MN34130DB("[MN34130SetVideoMode]current fps (10 base)= %d\n", (mn34130.capPclk*10000)*10/(MN34130_FULL_PERIOD_PIXEL_NUMS + mn34130.DummyPixels)/MIN_Frame_length);

		extralines = MIN_Frame_length - MN34130_FULL_PERIOD_LINE_NUMS;

		spin_lock(&mn34130mipiraw_drv_lock);
		mn34130.DummyPixels = 0;//define dummy pixels and lines
		mn34130.DummyLines = extralines ;
		spin_unlock(&mn34130mipiraw_drv_lock);

		MN34130_SetDummy(mn34130.DummyPixels,extralines);
	}
	MN34130DB("[MN34130SetVideoMode]MIN_Frame_length=%d,mn34130.DummyLines=%d\n",MIN_Frame_length,mn34130.DummyLines);

    return KAL_TRUE;
}

UINT32 MN34130SetAutoFlickerMode(kal_bool bEnable, UINT16 u2FrameRate)
{
	//return ERROR_NONE;

    MN34130DB("[MN34130SetAutoFlickerMode] benable, u2FrameRate = %d %d\n", bEnable, u2FrameRate);
	if(bEnable) {   // enable auto flicker
		spin_lock(&mn34130mipiraw_drv_lock);
		mn34130.MN34130AutoFlickerMode = KAL_TRUE;
		spin_unlock(&mn34130mipiraw_drv_lock);
    } else {
    	spin_lock(&mn34130mipiraw_drv_lock);
        mn34130.MN34130AutoFlickerMode = KAL_FALSE;
		spin_unlock(&mn34130mipiraw_drv_lock);
        MN34130DB("Disable Auto flicker\n");
    }

    return ERROR_NONE;
}

UINT32 MN34130SetTestPatternMode(kal_bool bEnable)
{
    MN34130DB("[MN34130SetTestPatternMode] Test pattern enable:%d\n", bEnable);
	if(bEnable)
		{
		MN34130_write_cmos_sensor(0x0602,0xff);//red
		MN34130_write_cmos_sensor(0x0603,0xff);
		MN34130_write_cmos_sensor(0x0604,0x00);//greenR
		MN34130_write_cmos_sensor(0x0605,0x00);
		MN34130_write_cmos_sensor(0x0606,0x00);//blue
		MN34130_write_cmos_sensor(0x0607,0x00);
		MN34130_write_cmos_sensor(0x0608,0x00);//greenB
		MN34130_write_cmos_sensor(0x0609,0x00);
		
    	MN34130_write_cmos_sensor(0x0600,0x00);
		MN34130_write_cmos_sensor(0x0601,0x01);
		}
	else
		MN34130_write_cmos_sensor(0x0600,0x00);	
    return ERROR_NONE;
}

UINT32 MN34130MIPISetMaxFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 frameRate) {
	kal_uint32 pclk;
	kal_int16 dummyLine;
	kal_uint16 lineLength,frameHeight;
		
	MN34130DB("MN34130MIPISetMaxFramerateByScenario: scenarioId = %d, frame rate = %d\n",scenarioId,frameRate);
	switch (scenarioId) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		pclk = 129600000;
		lineLength = MN34130_PV_PERIOD_PIXEL_NUMS;
		frameHeight = (10 * pclk)/frameRate/lineLength;
		dummyLine = frameHeight - MN34130_PV_PERIOD_LINE_NUMS;
		if(dummyLine<0)
			dummyLine = 0;
		spin_lock(&mn34130mipiraw_drv_lock);
		mn34130.sensorMode = SENSOR_MODE_PREVIEW;
		spin_unlock(&mn34130mipiraw_drv_lock);
		MN34130_SetDummy(0, dummyLine);			
		break;
		
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		pclk = 129600000;
		lineLength = MN34130_VIDEO_PERIOD_PIXEL_NUMS;
		frameHeight = (10 * pclk)/frameRate/lineLength;
		dummyLine = frameHeight - MN34130_VIDEO_PERIOD_LINE_NUMS;
		if(dummyLine<0)
			dummyLine = 0;
		spin_lock(&mn34130mipiraw_drv_lock);
		mn34130.sensorMode = SENSOR_MODE_VIDEO;
		spin_unlock(&mn34130mipiraw_drv_lock);
		MN34130_SetDummy(0, dummyLine);			
		break;			

	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	case MSDK_SCENARIO_ID_CAMERA_ZSD:			
		pclk = 259200000;
		lineLength = MN34130_FULL_PERIOD_PIXEL_NUMS;
		frameHeight = (10 * pclk)/frameRate/lineLength;
		dummyLine = frameHeight - MN34130_FULL_PERIOD_LINE_NUMS;
		if(dummyLine<0)
			dummyLine = 0;
		spin_lock(&mn34130mipiraw_drv_lock);
		mn34130.sensorMode = SENSOR_MODE_CAPTURE;
		spin_unlock(&mn34130mipiraw_drv_lock);
		MN34130_SetDummy(0, dummyLine);			
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


UINT32 MN34130MIPIGetDefaultFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 *pframeRate) 
{

	switch (scenarioId) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		*pframeRate = 304;
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
#ifdef FULL_SCAN_TO_VIDEO
		*pframeRate = 150;
#else
		*pframeRate = 304;
#endif
		break;			
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	case MSDK_SCENARIO_ID_CAMERA_ZSD:
		*pframeRate = 150;
		break;		
    case MSDK_SCENARIO_ID_CAMERA_3D_PREVIEW: //added
    case MSDK_SCENARIO_ID_CAMERA_3D_VIDEO:
    case MSDK_SCENARIO_ID_CAMERA_3D_CAPTURE: //added   
		*pframeRate = 304;
		break;		
	default:
		break;
	}

	return ERROR_NONE;
}



UINT32 MN34130FeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId,
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
		*pFeatureReturnPara16++= MN34130_IMAGE_SENSOR_FULL_WIDTH;
		*pFeatureReturnPara16= MN34130_IMAGE_SENSOR_FULL_HEIGHT;
		*pFeatureParaLen=4;
		break;
       case SENSOR_FEATURE_GET_PERIOD:
		*pFeatureReturnPara16++= MN34130_FeatureControl_PERIOD_PixelNum;
		*pFeatureReturnPara16= MN34130_FeatureControl_PERIOD_LineNum;
		*pFeatureParaLen=4;
		break;
       case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		switch(MN34130CurrentScenarioId)
		{
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			*pFeatureReturnPara32 = 129600000;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:	
			*pFeatureReturnPara32 = 129600000;//129600000;
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
			*pFeatureReturnPara32 = 259200000;
			break;
		default:
			*pFeatureReturnPara32 = 129600000;
			break;
		}
		*pFeatureParaLen=4;
		break;

        case SENSOR_FEATURE_SET_ESHUTTER:
            MN34130_SetShutter(*pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_NIGHTMODE:
            MN34130_NightMode((BOOL) *pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_GAIN:
            MN34130_SetGain((UINT16) *pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_FLASHLIGHT:
            break;
        case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
            //MN34130_isp_master_clock=*pFeatureData32;
            break;
        case SENSOR_FEATURE_SET_REGISTER:
            MN34130_write_cmos_sensor(pSensorRegData->RegAddr, pSensorRegData->RegData);
            break;
        case SENSOR_FEATURE_GET_REGISTER:
            pSensorRegData->RegData = MN34130_read_cmos_sensor(pSensorRegData->RegAddr);
            break;
        case SENSOR_FEATURE_SET_CCT_REGISTER:
            SensorRegNumber=FACTORY_END_ADDR;
            for (i=0;i<SensorRegNumber;i++)
            {
            	spin_lock(&mn34130mipiraw_drv_lock);
                MN34130SensorCCT[i].Addr=*pFeatureData32++;
                MN34130SensorCCT[i].Para=*pFeatureData32++;
				spin_unlock(&mn34130mipiraw_drv_lock);
            }
            break;
        case SENSOR_FEATURE_GET_CCT_REGISTER:
            SensorRegNumber=FACTORY_END_ADDR;
            if (*pFeatureParaLen<(SensorRegNumber*sizeof(SENSOR_REG_STRUCT)+4))
                return FALSE;
            *pFeatureData32++=SensorRegNumber;
            for (i=0;i<SensorRegNumber;i++)
            {
                *pFeatureData32++=MN34130SensorCCT[i].Addr;
                *pFeatureData32++=MN34130SensorCCT[i].Para;
            }
            break;
        case SENSOR_FEATURE_SET_ENG_REGISTER:
            SensorRegNumber=ENGINEER_END;
            for (i=0;i<SensorRegNumber;i++)
            {
            	spin_lock(&mn34130mipiraw_drv_lock);
                MN34130SensorReg[i].Addr=*pFeatureData32++;
                MN34130SensorReg[i].Para=*pFeatureData32++;
				spin_unlock(&mn34130mipiraw_drv_lock);
            }
            break;
        case SENSOR_FEATURE_GET_ENG_REGISTER:
            SensorRegNumber=ENGINEER_END;
            if (*pFeatureParaLen<(SensorRegNumber*sizeof(SENSOR_REG_STRUCT)+4))
                return FALSE;
            *pFeatureData32++=SensorRegNumber;
            for (i=0;i<SensorRegNumber;i++)
            {
                *pFeatureData32++=MN34130SensorReg[i].Addr;
                *pFeatureData32++=MN34130SensorReg[i].Para;
            }
            break;
        case SENSOR_FEATURE_GET_REGISTER_DEFAULT:
            if (*pFeatureParaLen>=sizeof(NVRAM_SENSOR_DATA_STRUCT))
            {
                pSensorDefaultData->Version=NVRAM_CAMERA_SENSOR_FILE_VERSION;
                pSensorDefaultData->SensorId=MN34130_SENSOR_ID;
                memcpy(pSensorDefaultData->SensorEngReg, MN34130SensorReg, sizeof(SENSOR_REG_STRUCT)*ENGINEER_END);
                memcpy(pSensorDefaultData->SensorCCTReg, MN34130SensorCCT, sizeof(SENSOR_REG_STRUCT)*FACTORY_END_ADDR);
            }
            else
                return FALSE;
            *pFeatureParaLen=sizeof(NVRAM_SENSOR_DATA_STRUCT);
            break;
        case SENSOR_FEATURE_GET_CONFIG_PARA:
            memcpy(pSensorConfigData, &MN34130SensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));
            *pFeatureParaLen=sizeof(MSDK_SENSOR_CONFIG_STRUCT);
            break;
        case SENSOR_FEATURE_CAMERA_PARA_TO_SENSOR:
            MN34130_camera_para_to_sensor();
            break;

        case SENSOR_FEATURE_SENSOR_TO_CAMERA_PARA:
            MN34130_sensor_to_camera_para();
            break;
        case SENSOR_FEATURE_GET_GROUP_COUNT:
            *pFeatureReturnPara32++=MN34130_get_sensor_group_count();
            *pFeatureParaLen=4;
            break;
        case SENSOR_FEATURE_GET_GROUP_INFO:
            MN34130_get_sensor_group_info(pSensorGroupInfo->GroupIdx, pSensorGroupInfo->GroupNamePtr, &pSensorGroupInfo->ItemCount);
            *pFeatureParaLen=sizeof(MSDK_SENSOR_GROUP_INFO_STRUCT);
            break;
        case SENSOR_FEATURE_GET_ITEM_INFO:
            MN34130_get_sensor_item_info(pSensorItemInfo->GroupIdx,pSensorItemInfo->ItemIdx, pSensorItemInfo);
            *pFeatureParaLen=sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
            break;

        case SENSOR_FEATURE_SET_ITEM_INFO:
            MN34130_set_sensor_item_info(pSensorItemInfo->GroupIdx, pSensorItemInfo->ItemIdx, pSensorItemInfo->ItemValue);
            *pFeatureParaLen=sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
            break;

        case SENSOR_FEATURE_GET_ENG_INFO:
            pSensorEngInfo->SensorId = 129;
            pSensorEngInfo->SensorType = CMOS_SENSOR;
            pSensorEngInfo->SensorOutputDataFormat=SENSOR_OUTPUT_FORMAT_RAW_Gr;
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
            MN34130SetVideoMode(*pFeatureData16);
            break;
        case SENSOR_FEATURE_CHECK_SENSOR_ID:
            MN34130GetSensorID(pFeatureReturnPara32);
            break;
        case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
            MN34130SetAutoFlickerMode((BOOL)*pFeatureData16, *(pFeatureData16+1));
	        break;
        case SENSOR_FEATURE_SET_TEST_PATTERN:
            MN34130SetTestPatternMode((BOOL)*pFeatureData16);
            break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		MN34130MIPISetMaxFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32, *(pFeatureData32+1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		MN34130MIPIGetDefaultFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32, (MUINT32 *)(*(pFeatureData32+1)));
		break;
        default:
            break;
    }
    return ERROR_NONE;
}	/* MN34130FeatureControl() */


SENSOR_FUNCTION_STRUCT SensorFuncMN34130 =
{
    MN34130Open,
    MN34130GetInfo,
    MN34130GetResolution,
    MN34130FeatureControl,
    MN34130Control,
    MN34130Close
};

UINT32 MN34130_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
    /* To Do : Check Sensor status here */
    if (pfFunc!=NULL)
        *pfFunc=&SensorFuncMN34130;

    return ERROR_NONE;
}   /* SensorInit() */

