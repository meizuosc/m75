/*****************************************************************************
 *
 * Filename:
 * ---------
 *   sensor.c
 *
 * Project:
 * --------
 *   DUMA
 *
 * Description:
 * ------------
 *   Source code of Sensor driver
 *
 *
 * Author:
 * -------
 *   PC Huang (MTK02204)
 *
 *============================================================================
 *             HISTORY
 * Below this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *------------------------------------------------------------------------------
 * $Revision:$
 * $Modtime:$
 * $Log:$
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/
//#include <windows.h>
//#include <memory.h>
//#include <nkintr.h>
//#include <ceddk.h>
//#include <ceddk_exp.h>

//#include "kal_release.h"
//#include "i2c_exp.h"
//#include "gpio_exp.h"
//#include "msdk_exp.h"
//#include "msdk_sensor_exp.h"
//#include "msdk_isp_exp.h"
//#include "base_regs.h"
//#include "Sensor.h"
//#include "camera_sensor_para.h"
//#include "CameraCustomized.h"

//s_porting add
//s_porting add
//s_porting add
#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/xlog.h>
#include <asm/atomic.h>
#include <asm/io.h>

#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "kd_camera_feature.h"


#include "s5k8aayxyuv_Sensor.h"
#include "s5k8aayxyuv_Camera_Sensor_para.h"
#include "s5k8aayxyuv_CameraCustomized.h"

#define S5K8AAYX_DEBUG
#ifdef S5K8AAYX_DEBUG
#define SENSORDB  printk
//#define SENSORDB(fmt, arg...)  printk(KERN_ERR fmt, ##arg)
//#define SENSORDB(fmt, arg...) xlog_printk(ANDROID_LOG_DEBUG, "[S5K8AAYX]", fmt, ##arg)
#else
#define SENSORDB(x,...)
#endif

#define S5K8AAYXYUV_TEST_PATTERN_CHECKSUM (0xe801a0a3)


typedef struct
{
    UINT16  iSensorVersion;
    UINT16  iNightMode;
    UINT16  iWB;
    UINT16  isoSpeed;
    UINT16  iEffect;
    UINT16  iEV;
    UINT16  iBanding;
    UINT16  iMirror;
    UINT16  iFrameRate;
} S5K8AAYXStatus;
S5K8AAYXStatus S5K8AAYXCurrentStatus;

extern int iReadRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u8 * a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u16 i2cId);
extern int iBurstWriteReg(u8 *a_pSendData , u16 a_sizeSendData, u16 i2cId);

//static int sensor_id_fail = 0;
static kal_uint32 zoom_factor = 0;


kal_uint16 S5K8AAYX_read_cmos_sensor(kal_uint32 addr)
{
    kal_uint16 get_byte=0;
    char puSendCmd[2] = {(char)(addr >> 8) , (char)(addr & 0xFF) };
    iReadRegI2C(puSendCmd , 2, (u8*)&get_byte,2,S5K8AAYX_WRITE_ID);
    return ((get_byte<<8)&0xff00)|((get_byte>>8)&0x00ff);
}

inline void S5K8AAYX_write_cmos_sensor(u16 addr, u32 para)
{
    char puSendCmd[4] = {(char)(addr >> 8) , (char)(addr & 0xFF) ,(char)(para >> 8),(char)(para & 0xFF)};

    iWriteRegI2C(puSendCmd , 4,S5K8AAYX_WRITE_ID);
}

#define USE_I2C_BURST_WRITE
#ifdef USE_I2C_BURST_WRITE
#define I2C_BUFFER_LEN 254 //MAX data to send by MT6572 i2c dma mode is 255 bytes
#define BLOCK_I2C_DATA_WRITE iBurstWriteReg
#else
#define I2C_BUFFER_LEN 8   // MT6572 i2c bus master fifo length is 8 bytes
#define BLOCK_I2C_DATA_WRITE iWriteRegI2C
#endif

// {addr, data} pair in para
// len is the total length of addr+data
// Using I2C multiple/burst write if the addr doesn't change
static kal_uint16 S5K8AAYX_table_write_cmos_sensor(kal_uint16* para, kal_uint32 len)
{
   char puSendCmd[I2C_BUFFER_LEN]; //at most 2 bytes address and 6 bytes data for multiple write. MTK i2c master has only 8bytes fifo.
   kal_uint32 tosend, IDX;
   kal_uint16 addr, addr_last, data;

   tosend = 0;
   IDX = 0;
   while(IDX < len)
   {
       addr = para[IDX];

       if (tosend == 0) // new (addr, data) to send
       {
           puSendCmd[tosend++] = (char)(addr >> 8);
           puSendCmd[tosend++] = (char)(addr & 0xFF);
           data = para[IDX+1];
           puSendCmd[tosend++] = (char)(data >> 8);
           puSendCmd[tosend++] = (char)(data & 0xFF);
           IDX += 2;
           addr_last = addr;
       }
       else if (addr == addr_last) // to multiple write the data to the same address
       {
           data = para[IDX+1];
           puSendCmd[tosend++] = (char)(data >> 8);
           puSendCmd[tosend++] = (char)(data & 0xFF);
           IDX += 2;
       }
       // to send out the data if the sen buffer is full or last data or to program to the different address.
       if (tosend == I2C_BUFFER_LEN || IDX == len || addr != addr_last)
       {
           BLOCK_I2C_DATA_WRITE(puSendCmd , tosend, S5K8AAYX_WRITE_ID);
           tosend = 0;
       }
   }
   return 0;
}

/*******************************************************************************
* // Adapter for Winmo typedef
********************************************************************************/
#define WINMO_USE 0

//#define Sleep(ms) msleep(ms)
#define RETAILMSG(x,...)
#define TEXT


/*******************************************************************************
* // End Adapter for Winmo typedef
********************************************************************************/

#define  S5K8AAYX_LIMIT_EXPOSURE_LINES                (1253)
#define  S5K8AAYX_VIDEO_NORMALMODE_30FRAME_RATE       (30)
#define  S5K8AAYX_VIDEO_NORMALMODE_FRAME_RATE         (15)
#define  S5K8AAYX_VIDEO_NIGHTMODE_FRAME_RATE          (7.5)
#define  BANDING50_30HZ
/* Global Valuable */


kal_bool S5K8AAYX_MPEG4_encode_mode = KAL_FALSE, S5K8AAYX_MJPEG_encode_mode = KAL_FALSE;

kal_uint32 S5K8AAYX_pixel_clk_freq = 0, S5K8AAYX_sys_clk_freq = 0;          // 480 means 48MHz

kal_uint16 S5K8AAYX_CAP_dummy_pixels = 0;
kal_uint16 S5K8AAYX_CAP_dummy_lines = 0;

kal_uint16 S5K8AAYX_PV_cintr = 0;
kal_uint16 S5K8AAYX_PV_cintc = 0;
kal_uint16 S5K8AAYX_CAP_cintr = 0;
kal_uint16 S5K8AAYX_CAP_cintc = 0;

kal_bool S5K8AAYX_night_mode_enable = KAL_FALSE;


//===============old============================================
//static kal_uint8 S5K8AAYX_exposure_line_h = 0, S5K8AAYX_exposure_line_l = 0,S5K8AAYX_extra_exposure_line_h = 0, S5K8AAYX_extra_exposure_line_l = 0;

//static kal_bool S5K8AAYX_gPVmode = KAL_TRUE; //PV size or Full size
//static kal_bool S5K8AAYX_VEDIO_encode_mode = KAL_FALSE; //Picture(Jpeg) or Video(Mpeg4)
static kal_bool S5K8AAYX_sensor_cap_state = KAL_FALSE; //Preview or Capture

//static kal_uint16 S5K8AAYX_dummy_pixels=0, S5K8AAYX_dummy_lines=0;

//static kal_uint16 S5K8AAYX_exposure_lines=0, S5K8AAYX_extra_exposure_lines = 0;



//static kal_uint8 S5K8AAYX_Banding_setting = AE_FLICKER_MODE_50HZ;  //Jinghe modified

/****** OVT 6-18******/
//static kal_uint16  S5K8AAYX_Capture_Max_Gain16= 6*16;
//static kal_uint16  S5K8AAYX_Capture_Gain16=0 ;
//static kal_uint16  S5K8AAYX_Capture_Shutter=0;
//static kal_uint16  S5K8AAYX_Capture_Extra_Lines=0;

//static kal_uint16  S5K8AAYX_PV_Dummy_Pixels =0, S5K8AAYX_Capture_Dummy_Pixels =0, S5K8AAYX_Capture_Dummy_Lines =0;
//static kal_uint16  S5K8AAYX_PV_Gain16 = 0;
//static kal_uint16  S5K8AAYX_PV_Shutter = 0;
//static kal_uint16  S5K8AAYX_PV_Extra_Lines = 0;
kal_uint16 S5K8AAYX_sensor_gain_base=0,S5K8AAYX_FAC_SENSOR_REG=0,S5K8AAYX_iS5K8AAYX_Mode=0,S5K8AAYX_max_exposure_lines=0;
kal_uint32 S5K8AAYX_capture_pclk_in_M=520,S5K8AAYX_preview_pclk_in_M=390,S5K8AAYX_PV_dummy_pixels=0,S5K8AAYX_PV_dummy_lines=0,S5K8AAYX_isp_master_clock=0;
static kal_uint32  S5K8AAYX_sensor_pclk=720;
//static kal_bool S5K8AAYX_AWB_ENABLE = KAL_TRUE;
static kal_bool S5K8AAYX_AE_ENABLE = KAL_TRUE;

//===============old============================================

kal_uint8 S5K8AAYX_sensor_write_I2C_address = S5K8AAYX_WRITE_ID;
kal_uint8 S5K8AAYX_sensor_read_I2C_address = S5K8AAYX_READ_ID;
//kal_uint16 S5K8AAYX_Sensor_ID = 0;

//HANDLE S5K8AAYXhDrvI2C;
//I2C_TRANSACTION S5K8AAYXI2CConfig;

UINT8 S5K8AAYXPixelClockDivider=0;

static DEFINE_SPINLOCK(s5k8aayx_drv_lock);
MSDK_SENSOR_CONFIG_STRUCT S5K8AAYXSensorConfigData;


/*************************************************************************
* FUNCTION
*       S5K8AAYXInitialPara
*
* DESCRIPTION
*       This function initialize the global status of  MT9V114
*
* PARAMETERS
*       None
*
* RETURNS
*       None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static void S5K8AAYXInitialPara(void)
{
    S5K8AAYXCurrentStatus.iNightMode = 0xFFFF;
    S5K8AAYXCurrentStatus.iWB = AWB_MODE_AUTO;
    S5K8AAYXCurrentStatus.isoSpeed = AE_ISO_100;
    S5K8AAYXCurrentStatus.iEffect = MEFFECT_OFF;
    S5K8AAYXCurrentStatus.iBanding = AE_FLICKER_MODE_50HZ;
    S5K8AAYXCurrentStatus.iEV = AE_EV_COMP_n03;
    S5K8AAYXCurrentStatus.iMirror = IMAGE_NORMAL;
    S5K8AAYXCurrentStatus.iFrameRate = 25;
}


void S5K8AAYX_set_mirror(kal_uint8 image_mirror)
{
SENSORDB("Enter S5K8AAYX_set_mirror \n");

    if(S5K8AAYXCurrentStatus.iMirror == image_mirror)
      return;

    S5K8AAYX_write_cmos_sensor(0x0028, 0x7000);
    S5K8AAYX_write_cmos_sensor(0x002a, 0x01E8);

    switch (image_mirror)
    {
        case IMAGE_NORMAL:
             S5K8AAYX_write_cmos_sensor(0x0F12, 0x0000);           // REG_0TC_PCFG_uPrevMirror
             S5K8AAYX_write_cmos_sensor(0x0F12, 0x0000);           // REG_0TC_PCFG_uCaptureMirror
             break;
        case IMAGE_H_MIRROR:
             S5K8AAYX_write_cmos_sensor(0x0F12, 0x0001);           // REG_0TC_PCFG_uPrevMirror
             S5K8AAYX_write_cmos_sensor(0x0F12, 0x0001);           // REG_0TC_PCFG_uCaptureMirror
             break;
        case IMAGE_V_MIRROR:
             S5K8AAYX_write_cmos_sensor(0x0F12, 0x0002);           // REG_0TC_PCFG_uPrevMirror
             S5K8AAYX_write_cmos_sensor(0x0F12, 0x0002);           // REG_0TC_PCFG_uCaptureMirror
             break;
        case IMAGE_HV_MIRROR:
             S5K8AAYX_write_cmos_sensor(0x0F12, 0x0003);           // REG_0TC_PCFG_uPrevMirror
             S5K8AAYX_write_cmos_sensor(0x0F12, 0x0003);           // REG_0TC_PCFG_uCaptureMirror
             break;

        default:
             ASSERT(0);
             break;
    }

    S5K8AAYX_write_cmos_sensor(0x002A,0x01A8);
    S5K8AAYX_write_cmos_sensor(0x0F12,0x0000); // #REG_TC_GP_ActivePrevConfig // Select preview configuration_0
    S5K8AAYX_write_cmos_sensor(0x002A,0x01AC);
    S5K8AAYX_write_cmos_sensor(0x0F12,0x0001); // #REG_TC_GP_PrevOpenAfterChange
    S5K8AAYX_write_cmos_sensor(0x002A,0x01A6);
    S5K8AAYX_write_cmos_sensor(0x0F12,0x0001);  // #REG_TC_GP_NewConfigSync // Update preview configuration
    S5K8AAYX_write_cmos_sensor(0x002A,0x01AA);
    S5K8AAYX_write_cmos_sensor(0x0F12,0x0001);  // #REG_TC_GP_PrevConfigChanged
    S5K8AAYX_write_cmos_sensor(0x002A,0x019E);
    S5K8AAYX_write_cmos_sensor(0x0F12,0x0001);  // #REG_TC_GP_EnablePreview // Start preview
    S5K8AAYX_write_cmos_sensor(0x0F12,0x0001);  // #REG_TC_GP_EnablePreviewChanged

    S5K8AAYXCurrentStatus.iMirror = image_mirror;
}


#if 0
void S5K8AAYX_set_isp_driving_current(kal_uint8 current)
{
}
#endif
/*****************************************************************************
 * FUNCTION
 *  S5K8AAYX_set_dummy
 * DESCRIPTION
 *
 * PARAMETERS
 *  pixels      [IN]
 *  lines       [IN]
 * RETURNS
 *  void
 *****************************************************************************/
void S5K8AAYX_set_dummy(kal_uint16 dummy_pixels, kal_uint16 dummy_lines)
{
    /****************************************************
      * Adjust the extra H-Blanking & V-Blanking.
      *****************************************************/
    S5K8AAYX_write_cmos_sensor(0x0028, 0x7000);
    S5K8AAYX_write_cmos_sensor(0x002A, 0x044C);

    S5K8AAYX_write_cmos_sensor(0x0F12, dummy_pixels);
    //S5K8AAYX_write_cmos_sensor(0x0F1C, dummy_pixels);   // Extra H-Blanking
    S5K8AAYX_write_cmos_sensor(0x0F12, dummy_lines);         // Extra V-Blanking
}   /* S5K8AAYX_set_dummy */


int SEN_SET_CURRENT = 1;
unsigned int D02D4_Current = 0x0;//0x02aa; 555
unsigned int D52D9_Current = 0x0;//0x02aa; 555
unsigned int unknow_gpio_Current   = 0x0000; //555
unsigned int CLK_Current   = 0x000; //555
MSDK_SCENARIO_ID_ENUM S5K8AAYXCurrentScenarioId = MSDK_SCENARIO_ID_CAMERA_PREVIEW;

/*****************************************************************************
 * FUNCTION
 *  S5K8AAYX_Initialize_Setting
 * DESCRIPTION
 *
 * PARAMETERS
 *  void
 * RETURNS
 *  void
 *****************************************************************************/

#define S5K8AAYX_PREVIEW_MODE             0
#define S5K8AAYX_VIDEO_MODE               1

void S5K8AAYX_Initialize_Setting(void)
{
  SENSORDB("Enter S5K8AAYX_Initialize_Setting\n");
  S5K8AAYX_write_cmos_sensor(0xFCFC ,0xD000);
  S5K8AAYX_write_cmos_sensor(0x0010 ,0x0001); // Reset
  S5K8AAYX_write_cmos_sensor(0xFCFC ,0x0000);
  S5K8AAYX_write_cmos_sensor(0x0000 ,0x0000); // Simmian bug workaround
  S5K8AAYX_write_cmos_sensor(0xFCFC ,0xD000);
  S5K8AAYX_write_cmos_sensor(0x1030 ,0x0000); // Clear host interrupt so main will wait
  S5K8AAYX_write_cmos_sensor(0x0014 ,0x0001); // ARM go

  mdelay(5);

  {
    static kal_uint16 addr_data_pair[] =
    {
  //=====================================================================================
  // T&P part   // chris 20130322
  //=====================================================================================
      0x0028, 0x7000,
      0x002A, 0x2470,
      0x0F12, 0xB510,
      0x0F12, 0x4910,
      0x0F12, 0x4810,
      0x0F12, 0xF000,
      0x0F12, 0xFA41,
      0x0F12, 0x4910,
      0x0F12, 0x4810,
      0x0F12, 0xF000,
      0x0F12, 0xFA3D,
      0x0F12, 0x4910,
      0x0F12, 0x4810,
      0x0F12, 0x6341,
      0x0F12, 0x4910,
      0x0F12, 0x4811,
      0x0F12, 0xF000,
      0x0F12, 0xFA36,
      0x0F12, 0x4910,
      0x0F12, 0x4811,
      0x0F12, 0xF000,
      0x0F12, 0xFA32,
      0x0F12, 0x4910,
      0x0F12, 0x4811,
      0x0F12, 0xF000,
      0x0F12, 0xFA2E,
      0x0F12, 0x4810,
      0x0F12, 0x4911,
      0x0F12, 0x6448,
      0x0F12, 0x4911,
      0x0F12, 0x4811,
      0x0F12, 0xF000,
      0x0F12, 0xFA27,
      0x0F12, 0xBC10,
      0x0F12, 0xBC08,
      0x0F12, 0x4718,
      0x0F12, 0x2870,
      0x0F12, 0x7000,
      0x0F12, 0x8EDD,
      0x0F12, 0x0000,
      0x0F12, 0x27E8,
      0x0F12, 0x7000,
      0x0F12, 0x8725,
      0x0F12, 0x0000,
      0x0F12, 0x2788,
      0x0F12, 0x7000,
      0x0F12, 0x0080,
      0x0F12, 0x7000,
      0x0F12, 0x26DC,
      0x0F12, 0x7000,
      0x0F12, 0xA6EF,
      0x0F12, 0x0000,
      0x0F12, 0x26A8,
      0x0F12, 0x7000,
      0x0F12, 0xA0F1,
      0x0F12, 0x0000,
      0x0F12, 0x2674,
      0x0F12, 0x7000,
      0x0F12, 0x058F,
      0x0F12, 0x0000,
      0x0F12, 0x2568,
      0x0F12, 0x7000,
      0x0F12, 0x0000,
      0x0F12, 0x7000,
      0x0F12, 0x24F4,
      0x0F12, 0x7000,
      0x0F12, 0xAC79,
      0x0F12, 0x0000,
      0x0F12, 0x4070,
      0x0F12, 0xE92D,
      0x0F12, 0x5000,
      0x0F12, 0xE1A0,
      0x0F12, 0x23BC,
      0x0F12, 0xE59F,
      0x0F12, 0x00BE,
      0x0F12, 0xE1D2,
      0x0F12, 0x0100,
      0x0F12, 0xE1A0,
      0x0F12, 0x10BC,
      0x0F12, 0xE1D2,
      0x0F12, 0x0000,
      0x0F12, 0xE351,
      0x0F12, 0x000E,
      0x0F12, 0x0A00,
      0x0F12, 0x33A8,
      0x0F12, 0xE59F,
      0x0F12, 0x14BC,
      0x0F12, 0xE1D3,
      0x0F12, 0x0000,
      0x0F12, 0xE151,
      0x0F12, 0x000A,
      0x0F12, 0x9A00,
      0x0F12, 0x0000,
      0x0F12, 0xE041,
      0x0F12, 0x11B0,
      0x0F12, 0xE1D2,
      0x0F12, 0x0091,
      0x0F12, 0xE000,
      0x0F12, 0x1034,
      0x0F12, 0xE593,
      0x0F12, 0x0520,
      0x0F12, 0xE1A0,
      0x0F12, 0x0091,
      0x0F12, 0xE000,
      0x0F12, 0x1384,
      0x0F12, 0xE59F,
      0x0F12, 0x1008,
      0x0F12, 0xE591,
      0x0F12, 0x00F0,
      0x0F12, 0xEB00,
      0x0F12, 0x4000,
      0x0F12, 0xE1A0,
      0x0F12, 0x0000,
      0x0F12, 0xEA00,
      0x0F12, 0x4000,
      0x0F12, 0xE3A0,
      0x0F12, 0x00EE,
      0x0F12, 0xEB00,
      0x0F12, 0x0004,
      0x0F12, 0xE080,
      0x0F12, 0x0000,
      0x0F12, 0xE585,
      0x0F12, 0x4070,
      0x0F12, 0xE8BD,
      0x0F12, 0xFF1E,
      0x0F12, 0xE12F,
      0x0F12, 0x401F,
      0x0F12, 0xE92D,
      0x0F12, 0x00EB,
      0x0F12, 0xEB00,
      0x0F12, 0x2000,
      0x0F12, 0xE3A0,
      0x0F12, 0x1002,
      0x0F12, 0xE3A0,
      0x0F12, 0x0F86,
      0x0F12, 0xE3A0,
      0x0F12, 0x00EA,
      0x0F12, 0xEB00,
      0x0F12, 0x2010,
      0x0F12, 0xE3A0,
      0x0F12, 0x1000,
      0x0F12, 0xE28D,
      0x0F12, 0x0E3F,
      0x0F12, 0xE3A0,
      0x0F12, 0x00E9,
      0x0F12, 0xEB00,
      0x0F12, 0x2001,
      0x0F12, 0xE3A0,
      0x0F12, 0x1002,
      0x0F12, 0xE3A0,
      0x0F12, 0x0F86,
      0x0F12, 0xE3A0,
      0x0F12, 0x00E2,
      0x0F12, 0xEB00,
      0x0F12, 0x0000,
      0x0F12, 0xE5DD,
      0x0F12, 0x00C3,
      0x0F12, 0xE350,
      0x0F12, 0x002F,
      0x0F12, 0x1A00,
      0x0F12, 0x0001,
      0x0F12, 0xE5DD,
      0x0F12, 0x003C,
      0x0F12, 0xE350,
      0x0F12, 0x002C,
      0x0F12, 0x1A00,
      0x0F12, 0x000E,
      0x0F12, 0xE5DD,
      0x0F12, 0x0011,
      0x0F12, 0xE350,
      0x0F12, 0x0029,
      0x0F12, 0x1A00,
      0x0F12, 0x000F,
      0x0F12, 0xE5DD,
      0x0F12, 0x0011,
      0x0F12, 0xE350,
      0x0F12, 0x0026,
      0x0F12, 0x1A00,
      0x0F12, 0x02E8,
      0x0F12, 0xE59F,
      0x0F12, 0x10BA,
      0x0F12, 0xE1D0,
      0x0F12, 0x0000,
      0x0F12, 0xE351,
      0x0F12, 0x0022,
      0x0F12, 0x0A00,
      0x0F12, 0x12E4,
      0x0F12, 0xE59F,
      0x0F12, 0x2000,
      0x0F12, 0xE5D1,
      0x0F12, 0x0000,
      0x0F12, 0xE352,
      0x0F12, 0x0002,
      0x0F12, 0x1A00,
      0x0F12, 0x00B8,
      0x0F12, 0xE1D0,
      0x0F12, 0x0000,
      0x0F12, 0xE350,
      0x0F12, 0x001B,
      0x0F12, 0x0A00,
      0x0F12, 0x0000,
      0x0F12, 0xE3A0,
      0x0F12, 0x0000,
      0x0F12, 0xE5C1,
      0x0F12, 0x1002,
      0x0F12, 0xE28D,
      0x0F12, 0x0015,
      0x0F12, 0xEA00,
      0x0F12, 0x2000,
      0x0F12, 0xE5D1,
      0x0F12, 0x3001,
      0x0F12, 0xE5D1,
      0x0F12, 0x3403,
      0x0F12, 0xE182,
      0x0F12, 0xC2B0,
      0x0F12, 0xE59F,
      0x0F12, 0x2080,
      0x0F12, 0xE08C,
      0x0F12, 0xE7B4,
      0x0F12, 0xE1D2,
      0x0F12, 0x039E,
      0x0F12, 0xE004,
      0x0F12, 0xE80F,
      0x0F12, 0xE3E0,
      0x0F12, 0x4624,
      0x0F12, 0xE00E,
      0x0F12, 0x47B4,
      0x0F12, 0xE1C2,
      0x0F12, 0x4004,
      0x0F12, 0xE280,
      0x0F12, 0xC084,
      0x0F12, 0xE08C,
      0x0F12, 0x47B4,
      0x0F12, 0xE1DC,
      0x0F12, 0x0493,
      0x0F12, 0xE004,
      0x0F12, 0x4624,
      0x0F12, 0xE00E,
      0x0F12, 0x47B4,
      0x0F12, 0xE1CC,
      0x0F12, 0xC8B4,
      0x0F12, 0xE1D2,
      0x0F12, 0x039C,
      0x0F12, 0xE003,
      0x0F12, 0x3623,
      0x0F12, 0xE00E,
      0x0F12, 0x38B4,
      0x0F12, 0xE1C2,
      0x0F12, 0x0001,
      0x0F12, 0xE280,
      0x0F12, 0x1002,
      0x0F12, 0xE281,
      0x0F12, 0x0004,
      0x0F12, 0xE350,
      0x0F12, 0xFFE7,
      0x0F12, 0xBAFF,
      0x0F12, 0x401F,
      0x0F12, 0xE8BD,
      0x0F12, 0xFF1E,
      0x0F12, 0xE12F,
      0x0F12, 0x4010,
      0x0F12, 0xE92D,
      0x0F12, 0x00B1,
      0x0F12, 0xEB00,
      0x0F12, 0x0250,
      0x0F12, 0xE59F,
      0x0F12, 0x00B2,
      0x0F12, 0xE1D0,
      0x0F12, 0x0000,
      0x0F12, 0xE350,
      0x0F12, 0x0004,
      0x0F12, 0x0A00,
      0x0F12, 0x0080,
      0x0F12, 0xE310,
      0x0F12, 0x0002,
      0x0F12, 0x1A00,
      0x0F12, 0x123C,
      0x0F12, 0xE59F,
      0x0F12, 0x0001,
      0x0F12, 0xE3A0,
      0x0F12, 0x0DB2,
      0x0F12, 0xE1C1,
      0x0F12, 0x4010,
      0x0F12, 0xE8BD,
      0x0F12, 0xFF1E,
      0x0F12, 0xE12F,
      0x0F12, 0x4010,
      0x0F12, 0xE92D,
      0x0F12, 0x4000,
      0x0F12, 0xE590,
      0x0F12, 0x0004,
      0x0F12, 0xE1A0,
      0x0F12, 0x00A5,
      0x0F12, 0xEB00,
      0x0F12, 0x021C,
      0x0F12, 0xE59F,
      0x0F12, 0x0000,
      0x0F12, 0xE5D0,
      0x0F12, 0x0000,
      0x0F12, 0xE350,
      0x0F12, 0x0002,
      0x0F12, 0x0A00,
      0x0F12, 0x0004,
      0x0F12, 0xE594,
      0x0F12, 0x00A0,
      0x0F12, 0xE1A0,
      0x0F12, 0x0004,
      0x0F12, 0xE584,
      0x0F12, 0x4010,
      0x0F12, 0xE8BD,
      0x0F12, 0xFF1E,
      0x0F12, 0xE12F,
      0x0F12, 0x4070,
      0x0F12, 0xE92D,
      0x0F12, 0x0000,
      0x0F12, 0xE590,
      0x0F12, 0x0800,
      0x0F12, 0xE1A0,
      0x0F12, 0x0820,
      0x0F12, 0xE1A0,
      0x0F12, 0x4041,
      0x0F12, 0xE280,
      0x0F12, 0x01E8,
      0x0F12, 0xE59F,
      0x0F12, 0x11B8,
      0x0F12, 0xE1D0,
      0x0F12, 0x51B6,
      0x0F12, 0xE1D0,
      0x0F12, 0x0005,
      0x0F12, 0xE041,
      0x0F12, 0x0094,
      0x0F12, 0xE000,
      0x0F12, 0x1D11,
      0x0F12, 0xE3A0,
      0x0F12, 0x007F,
      0x0F12, 0xEB00,
      0x0F12, 0x11C8,
      0x0F12, 0xE59F,
      0x0F12, 0x1000,
      0x0F12, 0xE5D1,
      0x0F12, 0x0000,
      0x0F12, 0xE351,
      0x0F12, 0x0000,
      0x0F12, 0x0A00,
      0x0F12, 0x00A0,
      0x0F12, 0xE1A0,
      0x0F12, 0x21B0,
      0x0F12, 0xE59F,
      0x0F12, 0x3FB0,
      0x0F12, 0xE1D2,
      0x0F12, 0x0000,
      0x0F12, 0xE353,
      0x0F12, 0x0003,
      0x0F12, 0x0A00,
      0x0F12, 0x31AC,
      0x0F12, 0xE59F,
      0x0F12, 0x5BB2,
      0x0F12, 0xE1C3,
      0x0F12, 0xC000,
      0x0F12, 0xE085,
      0x0F12, 0xCBB4,
      0x0F12, 0xE1C3,
      0x0F12, 0x0000,
      0x0F12, 0xE351,
      0x0F12, 0x0000,
      0x0F12, 0x0A00,
      0x0F12, 0x0080,
      0x0F12, 0xE1A0,
      0x0F12, 0x1DBC,
      0x0F12, 0xE1D2,
      0x0F12, 0x3EB4,
      0x0F12, 0xE1D2,
      0x0F12, 0x2EB2,
      0x0F12, 0xE1D2,
      0x0F12, 0x0193,
      0x0F12, 0xE001,
      0x0F12, 0x0092,
      0x0F12, 0xE000,
      0x0F12, 0x2811,
      0x0F12, 0xE3A0,
      0x0F12, 0x0194,
      0x0F12, 0xE001,
      0x0F12, 0x0092,
      0x0F12, 0xE000,
      0x0F12, 0x11A1,
      0x0F12, 0xE1A0,
      0x0F12, 0x01A0,
      0x0F12, 0xE1A0,
      0x0F12, 0x0064,
      0x0F12, 0xEB00,
      0x0F12, 0x1168,
      0x0F12, 0xE59F,
      0x0F12, 0x02B4,
      0x0F12, 0xE1C1,
      0x0F12, 0x4070,
      0x0F12, 0xE8BD,
      0x0F12, 0xFF1E,
      0x0F12, 0xE12F,
      0x0F12, 0x4010,
      0x0F12, 0xE92D,
      0x0F12, 0x0072,
      0x0F12, 0xEB00,
      0x0F12, 0x2150,
      0x0F12, 0xE59F,
      0x0F12, 0x14B0,
      0x0F12, 0xE1D2,
      0x0F12, 0x0080,
      0x0F12, 0xE311,
      0x0F12, 0x0005,
      0x0F12, 0x0A00,
      0x0F12, 0x0144,
      0x0F12, 0xE59F,
      0x0F12, 0x00B0,
      0x0F12, 0xE1D0,
      0x0F12, 0x0001,
      0x0F12, 0xE350,
      0x0F12, 0x0001,
      0x0F12, 0x9A00,
      0x0F12, 0x0001,
      0x0F12, 0xE3A0,
      0x0F12, 0x0000,
      0x0F12, 0xEA00,
      0x0F12, 0x0000,
      0x0F12, 0xE3A0,
      0x0F12, 0x3118,
      0x0F12, 0xE59F,
      0x0F12, 0x0000,
      0x0F12, 0xE5C3,
      0x0F12, 0x0000,
      0x0F12, 0xE5D3,
      0x0F12, 0x0000,
      0x0F12, 0xE350,
      0x0F12, 0x0003,
      0x0F12, 0x0A00,
      0x0F12, 0x0080,
      0x0F12, 0xE3C1,
      0x0F12, 0x1114,
      0x0F12, 0xE59F,
      0x0F12, 0x04B0,
      0x0F12, 0xE1C2,
      0x0F12, 0x00B2,
      0x0F12, 0xE1C1,
      0x0F12, 0x4010,
      0x0F12, 0xE8BD,
      0x0F12, 0xFF1E,
      0x0F12, 0xE12F,
      0x0F12, 0x41F0,
      0x0F12, 0xE92D,
      0x0F12, 0x1000,
      0x0F12, 0xE590,
      0x0F12, 0xC801,
      0x0F12, 0xE1A0,
      0x0F12, 0xC82C,
      0x0F12, 0xE1A0,
      0x0F12, 0x1004,
      0x0F12, 0xE590,
      0x0F12, 0x1801,
      0x0F12, 0xE1A0,
      0x0F12, 0x1821,
      0x0F12, 0xE1A0,
      0x0F12, 0x4008,
      0x0F12, 0xE590,
      0x0F12, 0x500C,
      0x0F12, 0xE590,
      0x0F12, 0x2004,
      0x0F12, 0xE1A0,
      0x0F12, 0x3005,
      0x0F12, 0xE1A0,
      0x0F12, 0x000C,
      0x0F12, 0xE1A0,
      0x0F12, 0x0052,
      0x0F12, 0xEB00,
      0x0F12, 0x609C,
      0x0F12, 0xE59F,
      0x0F12, 0x00B2,
      0x0F12, 0xE1D6,
      0x0F12, 0x0000,
      0x0F12, 0xE350,
      0x0F12, 0x000E,
      0x0F12, 0x0A00,
      0x0F12, 0x00C0,
      0x0F12, 0xE59F,
      0x0F12, 0x05B4,
      0x0F12, 0xE1D0,
      0x0F12, 0x0002,
      0x0F12, 0xE350,
      0x0F12, 0x000A,
      0x0F12, 0x1A00,
      0x0F12, 0x7080,
      0x0F12, 0xE59F,
      0x0F12, 0x10F4,
      0x0F12, 0xE1D6,
      0x0F12, 0x26B0,
      0x0F12, 0xE1D7,
      0x0F12, 0x00F0,
      0x0F12, 0xE1D4,
      0x0F12, 0x0048,
      0x0F12, 0xEB00,
      0x0F12, 0x00B0,
      0x0F12, 0xE1C4,
      0x0F12, 0x26B0,
      0x0F12, 0xE1D7,
      0x0F12, 0x10F6,
      0x0F12, 0xE1D6,
      0x0F12, 0x00F0,
      0x0F12, 0xE1D5,
      0x0F12, 0x0043,
      0x0F12, 0xEB00,
      0x0F12, 0x00B0,
      0x0F12, 0xE1C5,
      0x0F12, 0x41F0,
      0x0F12, 0xE8BD,
      0x0F12, 0xFF1E,
      0x0F12, 0xE12F,
      0x0F12, 0x4010,
      0x0F12, 0xE92D,
      0x0F12, 0x4000,
      0x0F12, 0xE1A0,
      0x0F12, 0x1004,
      0x0F12, 0xE594,
      0x0F12, 0x003C,
      0x0F12, 0xE59F,
      0x0F12, 0x00B0,
      0x0F12, 0xE1D0,
      0x0F12, 0x0000,
      0x0F12, 0xE350,
      0x0F12, 0x0008,
      0x0F12, 0x0A00,
      0x0F12, 0x0030,
      0x0F12, 0xE59F,
      0x0F12, 0x3001,
      0x0F12, 0xE1A0,
      0x0F12, 0x2068,
      0x0F12, 0xE590,
      0x0F12, 0x0058,
      0x0F12, 0xE59F,
      0x0F12, 0x1005,
      0x0F12, 0xE3A0,
      0x0F12, 0x0036,
      0x0F12, 0xEB00,
      0x0F12, 0x0000,
      0x0F12, 0xE584,
      0x0F12, 0x4010,
      0x0F12, 0xE8BD,
      0x0F12, 0xFF1E,
      0x0F12, 0xE12F,
      0x0F12, 0x0000,
      0x0F12, 0xE594,
      0x0F12, 0x0034,
      0x0F12, 0xEB00,
      0x0F12, 0x0000,
      0x0F12, 0xE584,
      0x0F12, 0xFFF9,
      0x0F12, 0xEAFF,
      0x0F12, 0x3360,
      0x0F12, 0x7000,
      0x0F12, 0x20D4,
      0x0F12, 0x7000,
      0x0F12, 0x16C8,
      0x0F12, 0x7000,
      0x0F12, 0x299C,
      0x0F12, 0x7000,
      0x0F12, 0x1272,
      0x0F12, 0x7000,
      0x0F12, 0x1728,
      0x0F12, 0x7000,
      0x0F12, 0x112C,
      0x0F12, 0x7000,
      0x0F12, 0x29A0,
      0x0F12, 0x7000,
      0x0F12, 0x122C,
      0x0F12, 0x7000,
      0x0F12, 0xF200,
      0x0F12, 0xD000,
      0x0F12, 0x2340,
      0x0F12, 0x7000,
      0x0F12, 0x0E2C,
      0x0F12, 0x7000,
      0x0F12, 0xF400,
      0x0F12, 0xD000,
      0x0F12, 0x0CDC,
      0x0F12, 0x7000,
      0x0F12, 0x06D4,
      0x0F12, 0x7000,
      0x0F12, 0x4778,
      0x0F12, 0x46C0,
      0x0F12, 0xC000,
      0x0F12, 0xE59F,
      0x0F12, 0xFF1C,
      0x0F12, 0xE12F,
      0x0F12, 0xC091,
      0x0F12, 0x0000,
      0x0F12, 0xF004,
      0x0F12, 0xE51F,
      0x0F12, 0xD14C,
      0x0F12, 0x0000,
      0x0F12, 0xC000,
      0x0F12, 0xE59F,
      0x0F12, 0xFF1C,
      0x0F12, 0xE12F,
      0x0F12, 0xAC79,
      0x0F12, 0x0000,
      0x0F12, 0xC000,
      0x0F12, 0xE59F,
      0x0F12, 0xFF1C,
      0x0F12, 0xE12F,
      0x0F12, 0x0467,
      0x0F12, 0x0000,
      0x0F12, 0xC000,
      0x0F12, 0xE59F,
      0x0F12, 0xFF1C,
      0x0F12, 0xE12F,
      0x0F12, 0x2FA7,
      0x0F12, 0x0000,
      0x0F12, 0xC000,
      0x0F12, 0xE59F,
      0x0F12, 0xFF1C,
      0x0F12, 0xE12F,
      0x0F12, 0xCB1F,
      0x0F12, 0x0000,
      0x0F12, 0xC000,
      0x0F12, 0xE59F,
      0x0F12, 0xFF1C,
      0x0F12, 0xE12F,
      0x0F12, 0x058F,
      0x0F12, 0x0000,
      0x0F12, 0xC000,
      0x0F12, 0xE59F,
      0x0F12, 0xFF1C,
      0x0F12, 0xE12F,
      0x0F12, 0xA0F1,
      0x0F12, 0x0000,
      0x0F12, 0xC000,
      0x0F12, 0xE59F,
      0x0F12, 0xFF1C,
      0x0F12, 0xE12F,
      0x0F12, 0x2B43,
      0x0F12, 0x0000,
      0x0F12, 0xC000,
      0x0F12, 0xE59F,
      0x0F12, 0xFF1C,
      0x0F12, 0xE12F,
      0x0F12, 0x8725,
      0x0F12, 0x0000,
      0x0F12, 0xC000,
      0x0F12, 0xE59F,
      0x0F12, 0xFF1C,
      0x0F12, 0xE12F,
      0x0F12, 0x6777,
      0x0F12, 0x0000,
      0x0F12, 0xC000,
      0x0F12, 0xE59F,
      0x0F12, 0xFF1C,
      0x0F12, 0xE12F,
      0x0F12, 0x8E49,
      0x0F12, 0x0000,
      0x0F12, 0xC000,
      0x0F12, 0xE59F,
      0x0F12, 0xFF1C,
      0x0F12, 0xE12F,
      0x0F12, 0x8EDD,
      0x0F12, 0x0000,
      0x0F12, 0xA4B6,
      0x0F12, 0x0000,
      0x0F12, 0x0001,
      0x0F12, 0x0000,
    };
    S5K8AAYX_table_write_cmos_sensor(addr_data_pair, sizeof(addr_data_pair)/sizeof(kal_uint16));

  }
  // End T&P part


  //============================================================
  // Set IO driving current
  //============================================================
  //S5K8AAYX_write_cmos_sensor(0x002A ,0x04B4);
  //S5K8AAYX_write_cmos_sensor(0x0F12 ,0x0155); // d0~d4
  //S5K8AAYX_write_cmos_sensor(0x0F12 ,0x0155); // d5~d9
  //S5K8AAYX_write_cmos_sensor(0x0F12 ,0x1555); // gpio1~gpio3
  //S5K8AAYX_write_cmos_sensor(0x0F12 ,0x0555); // HSYNC,VSYNC,PCLK,SCL,SDA
  /*if (SEN_SET_CURRENT)
  {
      S5K8AAYX_write_cmos_sensor(0x002A, 0x04B4);
      S5K8AAYX_write_cmos_sensor(0x0F12, D02D4_Current); //B4:155
      S5K8AAYX_write_cmos_sensor(0x0F12, D52D9_Current); //B6:155
      S5K8AAYX_write_cmos_sensor(0x0F12, unknow_gpio_Current); //0xB8:
      S5K8AAYX_write_cmos_sensor(0x0F12, CLK_Current); //0xBA:
  }*/

  {
    static kal_uint16 addr_data_pair[] =
    {
      //============================================================
      // Analog Settings
      //============================================================
      0x0028, 0x7000,
      0x002A, 0x0E38,
      0x0F12, 0x0476, //senHal_RegCompBiasNormSf //CDS bias
      0x0F12, 0x0476, //senHal_RegCompBiasYAv //CDS bias
      0x002A, 0x0AA0,
      0x0F12, 0x0001, //setot_bUseDigitalHbin //1-Digital, 0-Analog
      0x002A, 0x0E2C,
      0x0F12, 0x0001, //senHal_bUseAnalogVerAv //2-Adding/averaging, 1-Y-Avg, 0-PLA
      0x002A, 0x0E66,
      0x0F12, 0x0001, //senHal_RegBlstEnNorm
      0x002A, 0x1250,
      0x0F12, 0xFFFF,   //senHal_Bls_nSpExpLines
      0x002A, 0x1202,
      0x0F12, 0x0010,   //senHal_Dblr_VcoFreqMHZ

      //ADLC Filter
      0x002A, 0x1288,
      0x0F12, 0x020F, //gisp_dadlc_ResetFilterValue
      0x0F12, 0x1C02, //gisp_dadlc_SteadyFilterValue
      0x0F12, 0x0006, //gisp_dadlc_NResetIIrFrames

      //============================================================
      // AE
      //============================================================
      0x002A, 0x0D40,
      0x0F12, 0x003E, // 3E TVAR_ae_BrAve
      // For LT Calibration
      0x002A, 0x0D46,
      0x0F12, 0x000F, //ae_StatMode

      0x002A, 0x0440,
      0x0F12, 0x3410, //lt_uMaxExp_0_
      0x002A, 0x0444,
      0x0F12, 0x6590, //lt_uMaxExp_1_
      0x002A, 0x0448,
      0x0F12, 0xBB80, //lt_uMaxExp_2_
      0x002A, 0x044C,
      0x0F12, 0x3880, //lt_uMaxExp_3_
      0x0F12, 0x0001,
      0x002A, 0x0450,
      0x0F12, 0x3410, //lt_uCapMaxExp_0_
      0x002A, 0x0454,
      0x0F12, 0x6590, //lt_uCapMaxExp_1_
      0x002A, 0x0458,
      0x0F12, 0xBB80, //lt_uCapMaxExp_2_
      0x002A, 0x045C,
      0x0F12, 0x3880, //lt_uCapMaxExp_3_
      0x0F12, 0x0001,
      0x002A, 0x0460,
      0x0F12, 0x01B0, //lt_uMaxAnGain_0_
      0x0F12, 0x01B0, //lt_uMaxAnGain_1_
      0x0F12, 0x0280, //lt_uMaxAnGain_2_
      0x0F12, 0x0800, //lt_uMaxAnGain_3_
      0x0F12, 0x0100, //lt_uMaxDigGain
      0x0F12, 0x3000, //lt_uMaxTotGain
      0x002A, 0x042E,
      0x0F12, 0x010E, //lt_uLimitHigh
      0x0F12, 0x00F5, //lt_uLimitLow
      0x002A, 0x0DE0,
      0x0F12, 0x0002, //ae_Fade2BlackEnable  F2B off, F2W on



      0x002A,0x0D4E,
      0x0F12,0x0000, //ae_WeightTbl_16_0_
      0x0F12,0x0101, //ae_WeightTbl_16_1_
      0x0F12,0x0101, //ae_WeightTbl_16_2_
      0x0F12,0x0000, //ae_WeightTbl_16_3_
      0x0F12,0x0201, //ae_WeightTbl_16_4_
      0x0F12,0x0202, //ae_WeightTbl_16_5_
      0x0F12,0x0202, //ae_WeightTbl_16_6_
      0x0F12,0x0102, //ae_WeightTbl_16_7_
      0x0F12,0x0201, //ae_WeightTbl_16_8_
      0x0F12,0x0303, //ae_WeightTbl_16_9_
      0x0F12,0x0303, //ae_WeightTbl_16_10_
      0x0F12,0x0102, //ae_WeightTbl_16_11_
      0x0F12,0x0201, //ae_WeightTbl_16_12_
      0x0F12,0x0403, //ae_WeightTbl_16_13_
      0x0F12,0x0304, //ae_WeightTbl_16_14_
      0x0F12,0x0102, //ae_WeightTbl_16_15_
      0x0F12,0x0201, //ae_WeightTbl_16_16_
      0x0F12,0x0403, //ae_WeightTbl_16_17_
      0x0F12,0x0304, //ae_WeightTbl_16_18_
      0x0F12,0x0102, //ae_WeightTbl_16_19_
      0x0F12,0x0201, //ae_WeightTbl_16_20_
      0x0F12,0x0303, //ae_WeightTbl_16_21_
      0x0F12,0x0303, //ae_WeightTbl_16_22_
      0x0F12,0x0102, //ae_WeightTbl_16_23_
      0x0F12,0x0201, //ae_WeightTbl_16_24_
      0x0F12,0x0202, //ae_WeightTbl_16_25_
      0x0F12,0x0202, //ae_WeightTbl_16_26_
      0x0F12,0x0102, //ae_WeightTbl_16_27_
      0x0F12,0x0000, //ae_WeightTbl_16_28_
      0x0F12,0x0101, //ae_WeightTbl_16_29_
      0x0F12,0x0101, //ae_WeightTbl_16_30_
      0x0F12,0x0000, //ae_WeightTbl_16_31_
      
      
      
      //============================================================
      //  Illum Type calibration
      //============================================================
      //WRITE #SARR_IllumType_0_            0078
      //WRITE #SARR_IllumType_1_            00C3
      //WRITE #SARR_IllumType_2_            00E9
      //WRITE #SARR_IllumType_3_            0128
      //WRITE #SARR_IllumType_4_            016F
      //WRITE #SARR_IllumType_5_            0195
      //WRITE #SARR_IllumType_6_            01A4
      //
      //WRITE #SARR_IllumTypeF_0_             0100
      //WRITE #SARR_IllumTypeF_1_             0100
      //WRITE #SARR_IllumTypeF_2_             0110
      //WRITE #SARR_IllumTypeF_3_             00E5
      //WRITE #SARR_IllumTypeF_4_             0100
      //WRITE #SARR_IllumTypeF_5_             00ED
      //WRITE #SARR_IllumTypeF_6_             00ED
      
      //*************************************/
      // 05.OTP Control                     */
      //*************************************/
      0x002A, 0x3368,
      0x0F12, 0x0000,  // Tune_TP_bReMultGainsByNvm */
      0x0F12, 0x0001, // Tune_TP_bUseNvmMultGain            2 7000336A SHORT
      0x0F12, 0x0000, // Tune_TP_bCorrectBlueChMismatch     2 7000336C SHORT
      0x0F12, 0x0000, // Tune_TP_BlueGainOfs88              2 7000336E SHORT
      0x0F12, 0x0000, // Tune_TP_BlueGainFactor88           2 70003370 SHORT

      //============================================================
      // Lens Shading
      //============================================================
      0x002A, 0x1326,
      0x0F12, 0x0000,  //gisp_gos_Enable

      0x002A, 0x063A,
      0x0F12, 0x0100,  // #TVAR_ash_GASAlpha[0][0]
      0x0F12, 0x00E6,  // #TVAR_ash_GASAlpha[0][1]
      0x0F12, 0x00E6,  // #TVAR_ash_GASAlpha[0][2]
      0x0F12, 0x00B4,  // #TVAR_ash_GASAlpha[0][3]
      0x0F12, 0x00B4,  // #TVAR_ash_GASAlpha[1][0]
      0x0F12, 0x00E6,  // #TVAR_ash_GASAlpha[1][1]
      0x0F12, 0x00E6,  // #TVAR_ash_GASAlpha[1][2]
      0x0F12, 0x00D2,  // #TVAR_ash_GASAlpha[1][3]
      0x0F12, 0x00B4,  // #TVAR_ash_GASAlpha[2][0]
      0x0F12, 0x00E6,  // #TVAR_ash_GASAlpha[2][1]
      0x0F12, 0x00E6,  // #TVAR_ash_GASAlpha[2][2]
      0x0F12, 0x00D2,  // #TVAR_ash_GASAlpha[2][3]
      0x0F12, 0x00B4,  // #TVAR_ash_GASAlpha[3][0]
      0x0F12, 0x00E6,  // #TVAR_ash_GASAlpha[3][1]
      0x0F12, 0x00E6,  // #TVAR_ash_GASAlpha[3][2]
      0x0F12, 0x00DC,  // #TVAR_ash_GASAlpha[3][3]
      0x0F12, 0x00F5,  // #TVAR_ash_GASAlpha[4][0]
      0x0F12, 0x00F8,  // #TVAR_ash_GASAlpha[4][1]
      0x0F12, 0x00F8,  // #TVAR_ash_GASAlpha[4][2]
      0x0F12, 0x0100,  // #TVAR_ash_GASAlpha[4][3]
      0x0F12, 0x0109,  // #TVAR_ash_GASAlpha[5][0]
      0x0F12, 0x0100,  // #TVAR_ash_GASAlpha[5][1]
      0x0F12, 0x0100,  // #TVAR_ash_GASAlpha[5][2]
      0x0F12, 0x0100,  // #TVAR_ash_GASAlpha[5][3]
      0x0F12, 0x00F0,  // #TVAR_ash_GASAlpha[6][0]
      0x0F12, 0x0100,  // #TVAR_ash_GASAlpha[6][1]
      0x0F12, 0x0100,  // #TVAR_ash_GASAlpha[6][2]
      0x0F12, 0x0100,  // #TVAR_ash_GASAlpha[6][3]

      0x002A, 0x067A,
      0x0F12, 0x0064,  // #TVAR_ash_GASBeta[0][0]
      0x0F12, 0x0014,  // #TVAR_ash_GASBeta[0][1]
      0x0F12, 0x0014,  // #TVAR_ash_GASBeta[0][2]
      0x0F12, 0x0000,  // #TVAR_ash_GASBeta[0][3]
      0x0F12, 0x001E,  // #TVAR_ash_GASBeta[1][0]
      0x0F12, 0x000A,  // #TVAR_ash_GASBeta[1][1]
      0x0F12, 0x000A,  // #TVAR_ash_GASBeta[1][2]
      0x0F12, 0x0000,  // #TVAR_ash_GASBeta[1][3]
      0x0F12, 0x001E,  // #TVAR_ash_GASBeta[2][0]
      0x0F12, 0x000A,  // #TVAR_ash_GASBeta[2][1]
      0x0F12, 0x000A,  // #TVAR_ash_GASBeta[2][2]
      0x0F12, 0x0000,  // #TVAR_ash_GASBeta[2][3]
      0x0F12, 0x001E,  // #TVAR_ash_GASBeta[3][0]
      0x0F12, 0x0000,  // #TVAR_ash_GASBeta[3][1]
      0x0F12, 0x0000,  // #TVAR_ash_GASBeta[3][2]
      0x0F12, 0x0000,  // #TVAR_ash_GASBeta[3][3]
      0x0F12, 0x0046,  // #TVAR_ash_GASBeta[4][0]
      0x0F12, 0x0000,  // #TVAR_ash_GASBeta[4][1]
      0x0F12, 0x0000,  // #TVAR_ash_GASBeta[4][2]
      0x0F12, 0x0000,  // #TVAR_ash_GASBeta[4][3]
      0x0F12, 0x0032,  // #TVAR_ash_GASBeta[5][0]
      0x0F12, 0x0000,  // #TVAR_ash_GASBeta[5][1]
      0x0F12, 0x0000,  // #TVAR_ash_GASBeta[5][2]
      0x0F12, 0x0000,  // #TVAR_ash_GASBeta[5][3]
      0x0F12, 0x0055,  // #TVAR_ash_GASBeta[6][0]
      0x0F12, 0x0000,  // #TVAR_ash_GASBeta[6][1]
      0x0F12, 0x0000,  // #TVAR_ash_GASBeta[6][2]
      0x0F12, 0x0000,  // #TVAR_ash_GASBeta[6][3]

      0x002A, 0x06BA,
      0x0F12, 0x0001,  //ash_bLumaMode

      0x002A, 0x0632,
      0x0F12, 0x00F5, //TVAR_ash_CGrasAlphas_0_
      0x0F12, 0x00F8, //TVAR_ash_CGrasAlphas_1_
      0x0F12, 0x00F8, //TVAR_ash_CGrasAlphas_2_
      0x0F12, 0x0100, //TVAR_ash_CGrasAlphas_3_

      0x002A, 0x0672,
      0x0F12, 0x0100, //TVAR_ash_GASOutdoorAlpha_0_
      0x0F12, 0x0100, //TVAR_ash_GASOutdoorAlpha_1_
      0x0F12, 0x0100, //TVAR_ash_GASOutdoorAlpha_2_
      0x0F12, 0x0100, //TVAR_ash_GASOutdoorAlpha_3_
      0x002A, 0x06B2,
      0x0F12, 0x0000, //ash_GASOutdoorBeta_0_
      0x0F12, 0x0000, //ash_GASOutdoorBeta_1_
      0x0F12, 0x0000, //ash_GASOutdoorBeta_2_
      0x0F12, 0x0000, //ash_GASOutdoorBeta_3_

      0x002A, 0x0624,
      0x0F12, 0x009a,  //TVAR_ash_AwbAshCord_0_
      0x0F12, 0x00d3,  //TVAR_ash_AwbAshCord_1_
      0x0F12, 0x00d4,  //TVAR_ash_AwbAshCord_2_
      0x0F12, 0x012c,  //TVAR_ash_AwbAshCord_3_
      0x0F12, 0x0162,  //TVAR_ash_AwbAshCord_4_
      0x0F12, 0x0190,  //TVAR_ash_AwbAshCord_5_
      0x0F12, 0x01a0,  //TVAR_ash_AwbAshCord_6_

      0x002A, 0x06CC,
      0x0F12, 0x0280,  //ash_uParabolicCenterX
      0x0F12, 0x01E0,  //ash_uParabolicCenterY
      0x002A, 0x06D0,
      0x0F12, 0x000D,  //ash_uParabolicScalingA
      0x0F12, 0x000F,  //ash_uParabolicScalingB

      0x002A, 0x06C6,
      0x0F12, 0x0001,  //ash_bParabolicEstimation

      0x002A, 0x347C,
      0x0F12, 0x0254, //Tune_wbt_GAS_0_
      0x0F12, 0x01B3, //Tune_wbt_GAS_1_
      0x0F12, 0x0163, //Tune_wbt_GAS_2_
      0x0F12, 0x0120, //Tune_wbt_GAS_3_
      0x0F12, 0x00EE, //Tune_wbt_GAS_4_
      0x0F12, 0x00CC, //Tune_wbt_GAS_5_
      0x0F12, 0x00BE, //Tune_wbt_GAS_6_
      0x0F12, 0x00C5, //Tune_wbt_GAS_7_
      0x0F12, 0x00E1, //Tune_wbt_GAS_8_
      0x0F12, 0x0113, //Tune_wbt_GAS_9_
      0x0F12, 0x0151, //Tune_wbt_GAS_10_
      0x0F12, 0x01A0, //Tune_wbt_GAS_11_
      0x0F12, 0x0203, //Tune_wbt_GAS_12_
      0x0F12, 0x01F6, //Tune_wbt_GAS_13_
      0x0F12, 0x018A, //Tune_wbt_GAS_14_
      0x0F12, 0x013C, //Tune_wbt_GAS_15_
      0x0F12, 0x00F2, //Tune_wbt_GAS_16_
      0x0F12, 0x00BD, //Tune_wbt_GAS_17_
      0x0F12, 0x009D, //Tune_wbt_GAS_18_
      0x0F12, 0x008D, //Tune_wbt_GAS_19_
      0x0F12, 0x0095, //Tune_wbt_GAS_20_
      0x0F12, 0x00B3, //Tune_wbt_GAS_21_
      0x0F12, 0x00E7, //Tune_wbt_GAS_22_
      0x0F12, 0x0136, //Tune_wbt_GAS_23_
      0x0F12, 0x018E, //Tune_wbt_GAS_24_
      0x0F12, 0x01EA, //Tune_wbt_GAS_25_
      0x0F12, 0x01B1, //Tune_wbt_GAS_26_
      0x0F12, 0x014C, //Tune_wbt_GAS_27_
      0x0F12, 0x00FA, //Tune_wbt_GAS_28_
      0x0F12, 0x00AC, //Tune_wbt_GAS_29_
      0x0F12, 0x0078, //Tune_wbt_GAS_30_
      0x0F12, 0x0056, //Tune_wbt_GAS_31_
      0x0F12, 0x004B, //Tune_wbt_GAS_32_
      0x0F12, 0x0053, //Tune_wbt_GAS_33_
      0x0F12, 0x006C, //Tune_wbt_GAS_34_
      0x0F12, 0x00A1, //Tune_wbt_GAS_35_
      0x0F12, 0x00E7, //Tune_wbt_GAS_36_
      0x0F12, 0x014E, //Tune_wbt_GAS_37_
      0x0F12, 0x01A9, //Tune_wbt_GAS_38_
      0x0F12, 0x017F, //Tune_wbt_GAS_39_
      0x0F12, 0x0121, //Tune_wbt_GAS_40_
      0x0F12, 0x00C7, //Tune_wbt_GAS_41_
      0x0F12, 0x007D, //Tune_wbt_GAS_42_
      0x0F12, 0x0049, //Tune_wbt_GAS_43_
      0x0F12, 0x002A, //Tune_wbt_GAS_44_
      0x0F12, 0x0021, //Tune_wbt_GAS_45_
      0x0F12, 0x0027, //Tune_wbt_GAS_46_
      0x0F12, 0x003E, //Tune_wbt_GAS_47_
      0x0F12, 0x006E, //Tune_wbt_GAS_48_
      0x0F12, 0x00B4, //Tune_wbt_GAS_49_
      0x0F12, 0x0116, //Tune_wbt_GAS_50_
      0x0F12, 0x0178, //Tune_wbt_GAS_51_
      0x0F12, 0x0168, //Tune_wbt_GAS_52_
      0x0F12, 0x0105, //Tune_wbt_GAS_53_
      0x0F12, 0x00A9, //Tune_wbt_GAS_54_
      0x0F12, 0x005F, //Tune_wbt_GAS_55_
      0x0F12, 0x002D, //Tune_wbt_GAS_56_
      0x0F12, 0x0012, //Tune_wbt_GAS_57_
      0x0F12, 0x000A, //Tune_wbt_GAS_58_
      0x0F12, 0x000E, //Tune_wbt_GAS_59_
      0x0F12, 0x0023, //Tune_wbt_GAS_60_
      0x0F12, 0x004F, //Tune_wbt_GAS_61_
      0x0F12, 0x0095, //Tune_wbt_GAS_62_
      0x0F12, 0x00F6, //Tune_wbt_GAS_63_
      0x0F12, 0x015A, //Tune_wbt_GAS_64_
      0x0F12, 0x015C, //Tune_wbt_GAS_65_
      0x0F12, 0x00F6, //Tune_wbt_GAS_66_
      0x0F12, 0x0099, //Tune_wbt_GAS_67_
      0x0F12, 0x0050, //Tune_wbt_GAS_68_
      0x0F12, 0x0022, //Tune_wbt_GAS_69_
      0x0F12, 0x0009, //Tune_wbt_GAS_70_
      0x0F12, 0x0000, //Tune_wbt_GAS_71_
      0x0F12, 0x0003, //Tune_wbt_GAS_72_
      0x0F12, 0x0017, //Tune_wbt_GAS_73_
      0x0F12, 0x0043, //Tune_wbt_GAS_74_
      0x0F12, 0x0084, //Tune_wbt_GAS_75_
      0x0F12, 0x00E6, //Tune_wbt_GAS_76_
      0x0F12, 0x014F, //Tune_wbt_GAS_77_
      0x0F12, 0x015A, //Tune_wbt_GAS_78_
      0x0F12, 0x00F7, //Tune_wbt_GAS_79_
      0x0F12, 0x009C, //Tune_wbt_GAS_80_
      0x0F12, 0x0054, //Tune_wbt_GAS_81_
      0x0F12, 0x0024, //Tune_wbt_GAS_82_
      0x0F12, 0x0009, //Tune_wbt_GAS_83_
      0x0F12, 0x0000, //Tune_wbt_GAS_84_
      0x0F12, 0x0004, //Tune_wbt_GAS_85_
      0x0F12, 0x0018, //Tune_wbt_GAS_86_
      0x0F12, 0x0044, //Tune_wbt_GAS_87_
      0x0F12, 0x0086, //Tune_wbt_GAS_88_
      0x0F12, 0x00E6, //Tune_wbt_GAS_89_
      0x0F12, 0x014E, //Tune_wbt_GAS_90_
      0x0F12, 0x0162, //Tune_wbt_GAS_91_
      0x0F12, 0x0106, //Tune_wbt_GAS_92_
      0x0F12, 0x00AA, //Tune_wbt_GAS_93_
      0x0F12, 0x0062, //Tune_wbt_GAS_94_
      0x0F12, 0x0030, //Tune_wbt_GAS_95_
      0x0F12, 0x0014, //Tune_wbt_GAS_96_
      0x0F12, 0x000B, //Tune_wbt_GAS_97_
      0x0F12, 0x0010, //Tune_wbt_GAS_98_
      0x0F12, 0x0025, //Tune_wbt_GAS_99_
      0x0F12, 0x0053, //Tune_wbt_GAS_100_
      0x0F12, 0x0095, //Tune_wbt_GAS_101_
      0x0F12, 0x00F7, //Tune_wbt_GAS_102_
      0x0F12, 0x015C, //Tune_wbt_GAS_103_
      0x0F12, 0x017C, //Tune_wbt_GAS_104_
      0x0F12, 0x0122, //Tune_wbt_GAS_105_
      0x0F12, 0x00CB, //Tune_wbt_GAS_106_
      0x0F12, 0x0080, //Tune_wbt_GAS_107_
      0x0F12, 0x004C, //Tune_wbt_GAS_108_
      0x0F12, 0x0030, //Tune_wbt_GAS_109_
      0x0F12, 0x0023, //Tune_wbt_GAS_110_
      0x0F12, 0x002C, //Tune_wbt_GAS_111_
      0x0F12, 0x0043, //Tune_wbt_GAS_112_
      0x0F12, 0x0074, //Tune_wbt_GAS_113_
      0x0F12, 0x00B7, //Tune_wbt_GAS_114_
      0x0F12, 0x011B, //Tune_wbt_GAS_115_
      0x0F12, 0x017A, //Tune_wbt_GAS_116_
      0x0F12, 0x01A8, //Tune_wbt_GAS_117_
      0x0F12, 0x013C, //Tune_wbt_GAS_118_
      0x0F12, 0x00F2, //Tune_wbt_GAS_119_
      0x0F12, 0x00AA, //Tune_wbt_GAS_120_
      0x0F12, 0x0077, //Tune_wbt_GAS_121_
      0x0F12, 0x0058, //Tune_wbt_GAS_122_
      0x0F12, 0x004E, //Tune_wbt_GAS_123_
      0x0F12, 0x0056, //Tune_wbt_GAS_124_
      0x0F12, 0x0070, //Tune_wbt_GAS_125_
      0x0F12, 0x00A2, //Tune_wbt_GAS_126_
      0x0F12, 0x00EB, //Tune_wbt_GAS_127_
      0x0F12, 0x0146, //Tune_wbt_GAS_128_
      0x0F12, 0x019D, //Tune_wbt_GAS_129_
      0x0F12, 0x01D9, //Tune_wbt_GAS_130_
      0x0F12, 0x016A, //Tune_wbt_GAS_131_
      0x0F12, 0x0121, //Tune_wbt_GAS_132_
      0x0F12, 0x00E5, //Tune_wbt_GAS_133_
      0x0F12, 0x00B5, //Tune_wbt_GAS_134_
      0x0F12, 0x0098, //Tune_wbt_GAS_135_
      0x0F12, 0x008C, //Tune_wbt_GAS_136_
      0x0F12, 0x0094, //Tune_wbt_GAS_137_
      0x0F12, 0x00B1, //Tune_wbt_GAS_138_
      0x0F12, 0x00E4, //Tune_wbt_GAS_139_
      0x0F12, 0x0126, //Tune_wbt_GAS_140_
      0x0F12, 0x017D, //Tune_wbt_GAS_141_
      0x0F12, 0x01DE, //Tune_wbt_GAS_142_
      0x0F12, 0x01A0, //Tune_wbt_GAS_143_
      0x0F12, 0x00FA, //Tune_wbt_GAS_144_
      0x0F12, 0x00BE, //Tune_wbt_GAS_145_
      0x0F12, 0x0095, //Tune_wbt_GAS_146_
      0x0F12, 0x007E, //Tune_wbt_GAS_147_
      0x0F12, 0x006F, //Tune_wbt_GAS_148_
      0x0F12, 0x006A, //Tune_wbt_GAS_149_
      0x0F12, 0x006C, //Tune_wbt_GAS_150_
      0x0F12, 0x007C, //Tune_wbt_GAS_151_
      0x0F12, 0x0094, //Tune_wbt_GAS_152_
      0x0F12, 0x00B4, //Tune_wbt_GAS_153_
      0x0F12, 0x00E7, //Tune_wbt_GAS_154_
      0x0F12, 0x014D, //Tune_wbt_GAS_155_
      0x0F12, 0x013A, //Tune_wbt_GAS_156_
      0x0F12, 0x00D4, //Tune_wbt_GAS_157_
      0x0F12, 0x00A7, //Tune_wbt_GAS_158_
      0x0F12, 0x0081, //Tune_wbt_GAS_159_
      0x0F12, 0x0068, //Tune_wbt_GAS_160_
      0x0F12, 0x005A, //Tune_wbt_GAS_161_
      0x0F12, 0x0053, //Tune_wbt_GAS_162_
      0x0F12, 0x0059, //Tune_wbt_GAS_163_
      0x0F12, 0x0068, //Tune_wbt_GAS_164_
      0x0F12, 0x0082, //Tune_wbt_GAS_165_
      0x0F12, 0x00A7, //Tune_wbt_GAS_166_
      0x0F12, 0x00D5, //Tune_wbt_GAS_167_
      0x0F12, 0x011F, //Tune_wbt_GAS_168_
      0x0F12, 0x0103, //Tune_wbt_GAS_169_
      0x0F12, 0x00AB, //Tune_wbt_GAS_170_
      0x0F12, 0x0084, //Tune_wbt_GAS_171_
      0x0F12, 0x005D, //Tune_wbt_GAS_172_
      0x0F12, 0x0044, //Tune_wbt_GAS_173_
      0x0F12, 0x0034, //Tune_wbt_GAS_174_
      0x0F12, 0x0030, //Tune_wbt_GAS_175_
      0x0F12, 0x0035, //Tune_wbt_GAS_176_
      0x0F12, 0x0044, //Tune_wbt_GAS_177_
      0x0F12, 0x005F, //Tune_wbt_GAS_178_
      0x0F12, 0x0081, //Tune_wbt_GAS_179_
      0x0F12, 0x00B3, //Tune_wbt_GAS_180_
      0x0F12, 0x00F3, //Tune_wbt_GAS_181_
      0x0F12, 0x00DC, //Tune_wbt_GAS_182_
      0x0F12, 0x0094, //Tune_wbt_GAS_183_
      0x0F12, 0x006B, //Tune_wbt_GAS_184_
      0x0F12, 0x0044, //Tune_wbt_GAS_185_
      0x0F12, 0x0029, //Tune_wbt_GAS_186_
      0x0F12, 0x001A, //Tune_wbt_GAS_187_
      0x0F12, 0x0015, //Tune_wbt_GAS_188_
      0x0F12, 0x001A, //Tune_wbt_GAS_189_
      0x0F12, 0x0028, //Tune_wbt_GAS_190_
      0x0F12, 0x0042, //Tune_wbt_GAS_191_
      0x0F12, 0x0064, //Tune_wbt_GAS_192_
      0x0F12, 0x0094, //Tune_wbt_GAS_193_
      0x0F12, 0x00D0, //Tune_wbt_GAS_194_
      0x0F12, 0x00CD, //Tune_wbt_GAS_195_
      0x0F12, 0x0087, //Tune_wbt_GAS_196_
      0x0F12, 0x005B, //Tune_wbt_GAS_197_
      0x0F12, 0x0034, //Tune_wbt_GAS_198_
      0x0F12, 0x001A, //Tune_wbt_GAS_199_
      0x0F12, 0x000C, //Tune_wbt_GAS_200_
      0x0F12, 0x0008, //Tune_wbt_GAS_201_
      0x0F12, 0x000B, //Tune_wbt_GAS_202_
      0x0F12, 0x0018, //Tune_wbt_GAS_203_
      0x0F12, 0x0031, //Tune_wbt_GAS_204_
      0x0F12, 0x0055, //Tune_wbt_GAS_205_
      0x0F12, 0x0085, //Tune_wbt_GAS_206_
      0x0F12, 0x00BD, //Tune_wbt_GAS_207_
      0x0F12, 0x00C6, //Tune_wbt_GAS_208_
      0x0F12, 0x007E, //Tune_wbt_GAS_209_
      0x0F12, 0x0053, //Tune_wbt_GAS_210_
      0x0F12, 0x002C, //Tune_wbt_GAS_211_
      0x0F12, 0x0013, //Tune_wbt_GAS_212_
      0x0F12, 0x0006, //Tune_wbt_GAS_213_
      0x0F12, 0x0002, //Tune_wbt_GAS_214_
      0x0F12, 0x0004, //Tune_wbt_GAS_215_
      0x0F12, 0x0011, //Tune_wbt_GAS_216_
      0x0F12, 0x002A, //Tune_wbt_GAS_217_
      0x0F12, 0x004D, //Tune_wbt_GAS_218_
      0x0F12, 0x007C, //Tune_wbt_GAS_219_
      0x0F12, 0x00B7, //Tune_wbt_GAS_220_
      0x0F12, 0x00C6, //Tune_wbt_GAS_221_
      0x0F12, 0x007D, //Tune_wbt_GAS_222_
      0x0F12, 0x0054, //Tune_wbt_GAS_223_
      0x0F12, 0x002D, //Tune_wbt_GAS_224_
      0x0F12, 0x0013, //Tune_wbt_GAS_225_
      0x0F12, 0x0005, //Tune_wbt_GAS_226_
      0x0F12, 0x0001, //Tune_wbt_GAS_227_
      0x0F12, 0x0004, //Tune_wbt_GAS_228_
      0x0F12, 0x0010, //Tune_wbt_GAS_229_
      0x0F12, 0x002A, //Tune_wbt_GAS_230_
      0x0F12, 0x004C, //Tune_wbt_GAS_231_
      0x0F12, 0x007C, //Tune_wbt_GAS_232_
      0x0F12, 0x00B7, //Tune_wbt_GAS_233_
      0x0F12, 0x00CB, //Tune_wbt_GAS_234_
      0x0F12, 0x0083, //Tune_wbt_GAS_235_
      0x0F12, 0x005A, //Tune_wbt_GAS_236_
      0x0F12, 0x0034, //Tune_wbt_GAS_237_
      0x0F12, 0x001A, //Tune_wbt_GAS_238_
      0x0F12, 0x000B, //Tune_wbt_GAS_239_
      0x0F12, 0x0007, //Tune_wbt_GAS_240_
      0x0F12, 0x000A, //Tune_wbt_GAS_241_
      0x0F12, 0x0017, //Tune_wbt_GAS_242_
      0x0F12, 0x0031, //Tune_wbt_GAS_243_
      0x0F12, 0x0052, //Tune_wbt_GAS_244_
      0x0F12, 0x0082, //Tune_wbt_GAS_245_
      0x0F12, 0x00BB, //Tune_wbt_GAS_246_
      0x0F12, 0x00DF, //Tune_wbt_GAS_247_
      0x0F12, 0x0092, //Tune_wbt_GAS_248_
      0x0F12, 0x0069, //Tune_wbt_GAS_249_
      0x0F12, 0x0043, //Tune_wbt_GAS_250_
      0x0F12, 0x002A, //Tune_wbt_GAS_251_
      0x0F12, 0x001B, //Tune_wbt_GAS_252_
      0x0F12, 0x0015, //Tune_wbt_GAS_253_
      0x0F12, 0x001A, //Tune_wbt_GAS_254_
      0x0F12, 0x0028, //Tune_wbt_GAS_255_
      0x0F12, 0x0042, //Tune_wbt_GAS_256_
      0x0F12, 0x0063, //Tune_wbt_GAS_257_
      0x0F12, 0x0092, //Tune_wbt_GAS_258_
      0x0F12, 0x00CE, //Tune_wbt_GAS_259_
      0x0F12, 0x0100, //Tune_wbt_GAS_260_
      0x0F12, 0x00A3, //Tune_wbt_GAS_261_
      0x0F12, 0x007B, //Tune_wbt_GAS_262_
      0x0F12, 0x0056, //Tune_wbt_GAS_263_
      0x0F12, 0x003D, //Tune_wbt_GAS_264_
      0x0F12, 0x0030, //Tune_wbt_GAS_265_
      0x0F12, 0x002C, //Tune_wbt_GAS_266_
      0x0F12, 0x0030, //Tune_wbt_GAS_267_
      0x0F12, 0x003F, //Tune_wbt_GAS_268_
      0x0F12, 0x0058, //Tune_wbt_GAS_269_
      0x0F12, 0x007A, //Tune_wbt_GAS_270_
      0x0F12, 0x00A8, //Tune_wbt_GAS_271_
      0x0F12, 0x00E5, //Tune_wbt_GAS_272_
      0x0F12, 0x0139, //Tune_wbt_GAS_273_
      0x0F12, 0x00C6, //Tune_wbt_GAS_274_
      0x0F12, 0x0097, //Tune_wbt_GAS_275_
      0x0F12, 0x0077, //Tune_wbt_GAS_276_
      0x0F12, 0x0060, //Tune_wbt_GAS_277_
      0x0F12, 0x0053, //Tune_wbt_GAS_278_
      0x0F12, 0x0050, //Tune_wbt_GAS_279_
      0x0F12, 0x0053, //Tune_wbt_GAS_280_
      0x0F12, 0x0064, //Tune_wbt_GAS_281_
      0x0F12, 0x007D, //Tune_wbt_GAS_282_
      0x0F12, 0x009D, //Tune_wbt_GAS_283_
      0x0F12, 0x00CD, //Tune_wbt_GAS_284_
      0x0F12, 0x011F, //Tune_wbt_GAS_285_
      0x0F12, 0x0192, //Tune_wbt_GAS_286_
      0x0F12, 0x00F2, //Tune_wbt_GAS_287_
      0x0F12, 0x00B6, //Tune_wbt_GAS_288_
      0x0F12, 0x008D, //Tune_wbt_GAS_289_
      0x0F12, 0x0075, //Tune_wbt_GAS_290_
      0x0F12, 0x0066, //Tune_wbt_GAS_291_
      0x0F12, 0x0060, //Tune_wbt_GAS_292_
      0x0F12, 0x0064, //Tune_wbt_GAS_293_
      0x0F12, 0x0072, //Tune_wbt_GAS_294_
      0x0F12, 0x008A, //Tune_wbt_GAS_295_
      0x0F12, 0x00AB, //Tune_wbt_GAS_296_
      0x0F12, 0x00DD, //Tune_wbt_GAS_297_
      0x0F12, 0x0145, //Tune_wbt_GAS_298_
      0x0F12, 0x0136, //Tune_wbt_GAS_299_
      0x0F12, 0x00D0, //Tune_wbt_GAS_300_
      0x0F12, 0x00A5, //Tune_wbt_GAS_301_
      0x0F12, 0x007E, //Tune_wbt_GAS_302_
      0x0F12, 0x0064, //Tune_wbt_GAS_303_
      0x0F12, 0x0055, //Tune_wbt_GAS_304_
      0x0F12, 0x004E, //Tune_wbt_GAS_305_
      0x0F12, 0x0053, //Tune_wbt_GAS_306_
      0x0F12, 0x0061, //Tune_wbt_GAS_307_
      0x0F12, 0x007C, //Tune_wbt_GAS_308_
      0x0F12, 0x00A1, //Tune_wbt_GAS_309_
      0x0F12, 0x00CE, //Tune_wbt_GAS_310_
      0x0F12, 0x011C, //Tune_wbt_GAS_311_
      0x0F12, 0x00FC, //Tune_wbt_GAS_312_
      0x0F12, 0x00AA, //Tune_wbt_GAS_313_
      0x0F12, 0x0082, //Tune_wbt_GAS_314_
      0x0F12, 0x005A, //Tune_wbt_GAS_315_
      0x0F12, 0x0041, //Tune_wbt_GAS_316_
      0x0F12, 0x0030, //Tune_wbt_GAS_317_
      0x0F12, 0x002A, //Tune_wbt_GAS_318_
      0x0F12, 0x0030, //Tune_wbt_GAS_319_
      0x0F12, 0x003E, //Tune_wbt_GAS_320_
      0x0F12, 0x0059, //Tune_wbt_GAS_321_
      0x0F12, 0x007A, //Tune_wbt_GAS_322_
      0x0F12, 0x00AC, //Tune_wbt_GAS_323_
      0x0F12, 0x00EE, //Tune_wbt_GAS_324_
      0x0F12, 0x00D8, //Tune_wbt_GAS_325_
      0x0F12, 0x0094, //Tune_wbt_GAS_326_
      0x0F12, 0x006A, //Tune_wbt_GAS_327_
      0x0F12, 0x0043, //Tune_wbt_GAS_328_
      0x0F12, 0x0028, //Tune_wbt_GAS_329_
      0x0F12, 0x0016, //Tune_wbt_GAS_330_
      0x0F12, 0x0012, //Tune_wbt_GAS_331_
      0x0F12, 0x0016, //Tune_wbt_GAS_332_
      0x0F12, 0x0023, //Tune_wbt_GAS_333_
      0x0F12, 0x003E, //Tune_wbt_GAS_334_
      0x0F12, 0x0061, //Tune_wbt_GAS_335_
      0x0F12, 0x0090, //Tune_wbt_GAS_336_
      0x0F12, 0x00CE, //Tune_wbt_GAS_337_
      0x0F12, 0x00CC, //Tune_wbt_GAS_338_
      0x0F12, 0x0087, //Tune_wbt_GAS_339_
      0x0F12, 0x005C, //Tune_wbt_GAS_340_
      0x0F12, 0x0034, //Tune_wbt_GAS_341_
      0x0F12, 0x0019, //Tune_wbt_GAS_342_
      0x0F12, 0x000A, //Tune_wbt_GAS_343_
      0x0F12, 0x0005, //Tune_wbt_GAS_344_
      0x0F12, 0x0009, //Tune_wbt_GAS_345_
      0x0F12, 0x0015, //Tune_wbt_GAS_346_
      0x0F12, 0x002E, //Tune_wbt_GAS_347_
      0x0F12, 0x0052, //Tune_wbt_GAS_348_
      0x0F12, 0x0082, //Tune_wbt_GAS_349_
      0x0F12, 0x00BE, //Tune_wbt_GAS_350_
      0x0F12, 0x00C5, //Tune_wbt_GAS_351_
      0x0F12, 0x007F, //Tune_wbt_GAS_352_
      0x0F12, 0x0054, //Tune_wbt_GAS_353_
      0x0F12, 0x002D, //Tune_wbt_GAS_354_
      0x0F12, 0x0013, //Tune_wbt_GAS_355_
      0x0F12, 0x0004, //Tune_wbt_GAS_356_
      0x0F12, 0x0000, //Tune_wbt_GAS_357_
      0x0F12, 0x0002, //Tune_wbt_GAS_358_
      0x0F12, 0x000E, //Tune_wbt_GAS_359_
      0x0F12, 0x0027, //Tune_wbt_GAS_360_
      0x0F12, 0x004A, //Tune_wbt_GAS_361_
      0x0F12, 0x007A, //Tune_wbt_GAS_362_
      0x0F12, 0x00B5, //Tune_wbt_GAS_363_
      0x0F12, 0x00C4, //Tune_wbt_GAS_364_
      0x0F12, 0x0080, //Tune_wbt_GAS_365_
      0x0F12, 0x0056, //Tune_wbt_GAS_366_
      0x0F12, 0x002E, //Tune_wbt_GAS_367_
      0x0F12, 0x0014, //Tune_wbt_GAS_368_
      0x0F12, 0x0004, //Tune_wbt_GAS_369_
      0x0F12, 0x0000, //Tune_wbt_GAS_370_
      0x0F12, 0x0002, //Tune_wbt_GAS_371_
      0x0F12, 0x000E, //Tune_wbt_GAS_372_
      0x0F12, 0x0027, //Tune_wbt_GAS_373_
      0x0F12, 0x004A, //Tune_wbt_GAS_374_
      0x0F12, 0x0079, //Tune_wbt_GAS_375_
      0x0F12, 0x00B6, //Tune_wbt_GAS_376_
      0x0F12, 0x00CA, //Tune_wbt_GAS_377_
      0x0F12, 0x0086, //Tune_wbt_GAS_378_
      0x0F12, 0x005C, //Tune_wbt_GAS_379_
      0x0F12, 0x0035, //Tune_wbt_GAS_380_
      0x0F12, 0x001B, //Tune_wbt_GAS_381_
      0x0F12, 0x000A, //Tune_wbt_GAS_382_
      0x0F12, 0x0005, //Tune_wbt_GAS_383_
      0x0F12, 0x0008, //Tune_wbt_GAS_384_
      0x0F12, 0x0015, //Tune_wbt_GAS_385_
      0x0F12, 0x002F, //Tune_wbt_GAS_386_
      0x0F12, 0x0050, //Tune_wbt_GAS_387_
      0x0F12, 0x007F, //Tune_wbt_GAS_388_
      0x0F12, 0x00BA, //Tune_wbt_GAS_389_
      0x0F12, 0x00DE, //Tune_wbt_GAS_390_
      0x0F12, 0x0094, //Tune_wbt_GAS_391_
      0x0F12, 0x006B, //Tune_wbt_GAS_392_
      0x0F12, 0x0045, //Tune_wbt_GAS_393_
      0x0F12, 0x002A, //Tune_wbt_GAS_394_
      0x0F12, 0x001A, //Tune_wbt_GAS_395_
      0x0F12, 0x0013, //Tune_wbt_GAS_396_
      0x0F12, 0x0018, //Tune_wbt_GAS_397_
      0x0F12, 0x0025, //Tune_wbt_GAS_398_
      0x0F12, 0x003F, //Tune_wbt_GAS_399_
      0x0F12, 0x005F, //Tune_wbt_GAS_400_
      0x0F12, 0x0090, //Tune_wbt_GAS_401_
      0x0F12, 0x00CD, //Tune_wbt_GAS_402_
      0x0F12, 0x0101, //Tune_wbt_GAS_403_
      0x0F12, 0x00A6, //Tune_wbt_GAS_404_
      0x0F12, 0x007D, //Tune_wbt_GAS_405_
      0x0F12, 0x0057, //Tune_wbt_GAS_406_
      0x0F12, 0x003E, //Tune_wbt_GAS_407_
      0x0F12, 0x002F, //Tune_wbt_GAS_408_
      0x0F12, 0x002A, //Tune_wbt_GAS_409_
      0x0F12, 0x002E, //Tune_wbt_GAS_410_
      0x0F12, 0x003B, //Tune_wbt_GAS_411_
      0x0F12, 0x0055, //Tune_wbt_GAS_412_
      0x0F12, 0x0077, //Tune_wbt_GAS_413_
      0x0F12, 0x00A5, //Tune_wbt_GAS_414_
      0x0F12, 0x00E3, //Tune_wbt_GAS_415_
      0x0F12, 0x0139, //Tune_wbt_GAS_416_
      0x0F12, 0x00C6, //Tune_wbt_GAS_417_
      0x0F12, 0x0099, //Tune_wbt_GAS_418_
      0x0F12, 0x0078, //Tune_wbt_GAS_419_
      0x0F12, 0x0060, //Tune_wbt_GAS_420_
      0x0F12, 0x0052, //Tune_wbt_GAS_421_
      0x0F12, 0x004D, //Tune_wbt_GAS_422_
      0x0F12, 0x0051, //Tune_wbt_GAS_423_
      0x0F12, 0x0060, //Tune_wbt_GAS_424_
      0x0F12, 0x0079, //Tune_wbt_GAS_425_
      0x0F12, 0x009A, //Tune_wbt_GAS_426_
      0x0F12, 0x00CA, //Tune_wbt_GAS_427_
      0x0F12, 0x0120, //Tune_wbt_GAS_428_
      0x0F12, 0x016F, //Tune_wbt_GAS_429_
      0x0F12, 0x00D5, //Tune_wbt_GAS_430_
      0x0F12, 0x009D, //Tune_wbt_GAS_431_
      0x0F12, 0x007A, //Tune_wbt_GAS_432_
      0x0F12, 0x0065, //Tune_wbt_GAS_433_
      0x0F12, 0x0058, //Tune_wbt_GAS_434_
      0x0F12, 0x0054, //Tune_wbt_GAS_435_
      0x0F12, 0x0056, //Tune_wbt_GAS_436_
      0x0F12, 0x0063, //Tune_wbt_GAS_437_
      0x0F12, 0x007A, //Tune_wbt_GAS_438_
      0x0F12, 0x0095, //Tune_wbt_GAS_439_
      0x0F12, 0x00C5, //Tune_wbt_GAS_440_
      0x0F12, 0x011E, //Tune_wbt_GAS_441_
      0x0F12, 0x010E, //Tune_wbt_GAS_442_
      0x0F12, 0x00B5, //Tune_wbt_GAS_443_
      0x0F12, 0x008A, //Tune_wbt_GAS_444_
      0x0F12, 0x006B, //Tune_wbt_GAS_445_
      0x0F12, 0x0057, //Tune_wbt_GAS_446_
      0x0F12, 0x004B, //Tune_wbt_GAS_447_
      0x0F12, 0x0046, //Tune_wbt_GAS_448_
      0x0F12, 0x004A, //Tune_wbt_GAS_449_
      0x0F12, 0x0058, //Tune_wbt_GAS_450_
      0x0F12, 0x006E, //Tune_wbt_GAS_451_
      0x0F12, 0x0090, //Tune_wbt_GAS_452_
      0x0F12, 0x00B7, //Tune_wbt_GAS_453_
      0x0F12, 0x00EE, //Tune_wbt_GAS_454_
      0x0F12, 0x00D7, //Tune_wbt_GAS_455_
      0x0F12, 0x0091, //Tune_wbt_GAS_456_
      0x0F12, 0x006C, //Tune_wbt_GAS_457_
      0x0F12, 0x004C, //Tune_wbt_GAS_458_
      0x0F12, 0x0038, //Tune_wbt_GAS_459_
      0x0F12, 0x002A, //Tune_wbt_GAS_460_
      0x0F12, 0x0026, //Tune_wbt_GAS_461_
      0x0F12, 0x002A, //Tune_wbt_GAS_462_
      0x0F12, 0x0038, //Tune_wbt_GAS_463_
      0x0F12, 0x0050, //Tune_wbt_GAS_464_
      0x0F12, 0x006D, //Tune_wbt_GAS_465_
      0x0F12, 0x009C, //Tune_wbt_GAS_466_
      0x0F12, 0x00C7, //Tune_wbt_GAS_467_
      0x0F12, 0x00B5, //Tune_wbt_GAS_468_
      0x0F12, 0x007E, //Tune_wbt_GAS_469_
      0x0F12, 0x0058, //Tune_wbt_GAS_470_
      0x0F12, 0x0038, //Tune_wbt_GAS_471_
      0x0F12, 0x0021, //Tune_wbt_GAS_472_
      0x0F12, 0x0015, //Tune_wbt_GAS_473_
      0x0F12, 0x0011, //Tune_wbt_GAS_474_
      0x0F12, 0x0014, //Tune_wbt_GAS_475_
      0x0F12, 0x0022, //Tune_wbt_GAS_476_
      0x0F12, 0x003A, //Tune_wbt_GAS_477_
      0x0F12, 0x0057, //Tune_wbt_GAS_478_
      0x0F12, 0x0083, //Tune_wbt_GAS_479_
      0x0F12, 0x00AF, //Tune_wbt_GAS_480_
      0x0F12, 0x00A8, //Tune_wbt_GAS_481_
      0x0F12, 0x0071, //Tune_wbt_GAS_482_
      0x0F12, 0x004B, //Tune_wbt_GAS_483_
      0x0F12, 0x002B, //Tune_wbt_GAS_484_
      0x0F12, 0x0014, //Tune_wbt_GAS_485_
      0x0F12, 0x0009, //Tune_wbt_GAS_486_
      0x0F12, 0x0005, //Tune_wbt_GAS_487_
      0x0F12, 0x0008, //Tune_wbt_GAS_488_
      0x0F12, 0x0014, //Tune_wbt_GAS_489_
      0x0F12, 0x002B, //Tune_wbt_GAS_490_
      0x0F12, 0x004A, //Tune_wbt_GAS_491_
      0x0F12, 0x0076, //Tune_wbt_GAS_492_
      0x0F12, 0x009F, //Tune_wbt_GAS_493_
      0x0F12, 0x00A1, //Tune_wbt_GAS_494_
      0x0F12, 0x006B, //Tune_wbt_GAS_495_
      0x0F12, 0x0045, //Tune_wbt_GAS_496_
      0x0F12, 0x0024, //Tune_wbt_GAS_497_
      0x0F12, 0x000F, //Tune_wbt_GAS_498_
      0x0F12, 0x0003, //Tune_wbt_GAS_499_
      0x0F12, 0x0000, //Tune_wbt_GAS_500_
      0x0F12, 0x0002, //Tune_wbt_GAS_501_
      0x0F12, 0x000E, //Tune_wbt_GAS_502_
      0x0F12, 0x0024, //Tune_wbt_GAS_503_
      0x0F12, 0x0042, //Tune_wbt_GAS_504_
      0x0F12, 0x006E, //Tune_wbt_GAS_505_
      0x0F12, 0x0098, //Tune_wbt_GAS_506_
      0x0F12, 0x00A1, //Tune_wbt_GAS_507_
      0x0F12, 0x006C, //Tune_wbt_GAS_508_
      0x0F12, 0x0046, //Tune_wbt_GAS_509_
      0x0F12, 0x0027, //Tune_wbt_GAS_510_
      0x0F12, 0x0010, //Tune_wbt_GAS_511_
      0x0F12, 0x0004, //Tune_wbt_GAS_512_
      0x0F12, 0x0000, //Tune_wbt_GAS_513_
      0x0F12, 0x0002, //Tune_wbt_GAS_514_
      0x0F12, 0x000E, //Tune_wbt_GAS_515_
      0x0F12, 0x0024, //Tune_wbt_GAS_516_
      0x0F12, 0x0043, //Tune_wbt_GAS_517_
      0x0F12, 0x006E, //Tune_wbt_GAS_518_
      0x0F12, 0x0099, //Tune_wbt_GAS_519_
      0x0F12, 0x00AA, //Tune_wbt_GAS_520_
      0x0F12, 0x0073, //Tune_wbt_GAS_521_
      0x0F12, 0x004D, //Tune_wbt_GAS_522_
      0x0F12, 0x002D, //Tune_wbt_GAS_523_
      0x0F12, 0x0016, //Tune_wbt_GAS_524_
      0x0F12, 0x0009, //Tune_wbt_GAS_525_
      0x0F12, 0x0005, //Tune_wbt_GAS_526_
      0x0F12, 0x0008, //Tune_wbt_GAS_527_
      0x0F12, 0x0014, //Tune_wbt_GAS_528_
      0x0F12, 0x002C, //Tune_wbt_GAS_529_
      0x0F12, 0x0049, //Tune_wbt_GAS_530_
      0x0F12, 0x0076, //Tune_wbt_GAS_531_
      0x0F12, 0x009C, //Tune_wbt_GAS_532_
      0x0F12, 0x00BD, //Tune_wbt_GAS_533_
      0x0F12, 0x007F, //Tune_wbt_GAS_534_
      0x0F12, 0x0058, //Tune_wbt_GAS_535_
      0x0F12, 0x003A, //Tune_wbt_GAS_536_
      0x0F12, 0x0024, //Tune_wbt_GAS_537_
      0x0F12, 0x0018, //Tune_wbt_GAS_538_
      0x0F12, 0x0012, //Tune_wbt_GAS_539_
      0x0F12, 0x0016, //Tune_wbt_GAS_540_
      0x0F12, 0x0023, //Tune_wbt_GAS_541_
      0x0F12, 0x003B, //Tune_wbt_GAS_542_
      0x0F12, 0x0058, //Tune_wbt_GAS_543_
      0x0F12, 0x0082, //Tune_wbt_GAS_544_
      0x0F12, 0x00AB, //Tune_wbt_GAS_545_
      0x0F12, 0x00DC, //Tune_wbt_GAS_546_
      0x0F12, 0x008F, //Tune_wbt_GAS_547_
      0x0F12, 0x006A, //Tune_wbt_GAS_548_
      0x0F12, 0x004C, //Tune_wbt_GAS_549_
      0x0F12, 0x0038, //Tune_wbt_GAS_550_
      0x0F12, 0x002C, //Tune_wbt_GAS_551_
      0x0F12, 0x0028, //Tune_wbt_GAS_552_
      0x0F12, 0x002C, //Tune_wbt_GAS_553_
      0x0F12, 0x0038, //Tune_wbt_GAS_554_
      0x0F12, 0x0050, //Tune_wbt_GAS_555_
      0x0F12, 0x006C, //Tune_wbt_GAS_556_
      0x0F12, 0x0096, //Tune_wbt_GAS_557_
      0x0F12, 0x00C2, //Tune_wbt_GAS_558_
      0x0F12, 0x0117, //Tune_wbt_GAS_559_
      0x0F12, 0x00AF, //Tune_wbt_GAS_560_
      0x0F12, 0x0083, //Tune_wbt_GAS_561_
      0x0F12, 0x0068, //Tune_wbt_GAS_562_
      0x0F12, 0x0054, //Tune_wbt_GAS_563_
      0x0F12, 0x004A, //Tune_wbt_GAS_564_
      0x0F12, 0x0046, //Tune_wbt_GAS_565_
      0x0F12, 0x004A, //Tune_wbt_GAS_566_
      0x0F12, 0x0058, //Tune_wbt_GAS_567_
      0x0F12, 0x006D, //Tune_wbt_GAS_568_
      0x0F12, 0x008A, //Tune_wbt_GAS_569_
      0x0F12, 0x00B4, //Tune_wbt_GAS_570_
      0x0F12, 0x00FB, //Tune_wbt_GAS_571_

      0x002A, 0x1348,
      0x0F12, 0x0001,

      //============================================================
      // AWB
      //============================================================

      0X002A,0X04C6,
      0X0F12,0X0030,  //awbb_MinNumOfChromaClassifyPatches
      0x002A,0x0B36,
      0x0F12,0x0005,
      0x002A,0x0B3A,
      0x0F12,0x00D7,
      0x0F12,0x02AC,
      0x002A,0x0B38,
      0x0F12,0x0010,
      0x002A,0x0AE6,
      0x0F12,0x03E2,
      0x0F12,0x041C,
      0x0F12,0x03AC,
      0x0F12,0x041F,
      0x0F12,0x0371,
      0x0F12,0x03DF,
      0x0F12,0x0340,
      0x0F12,0x03BF,
      0x0F12,0x0317,
      0x0F12,0x0398,
      0x0F12,0x02EE,
      0x0F12,0x037B,
      0x0F12,0x02D1,
      0x0F12,0x0356,
      0x0F12,0x02AF,
      0x0F12,0x0330,
      0x0F12,0x0283,
      0x0F12,0x0304,
      0x0F12,0x0263,
      0x0F12,0x02DF,
      0x0F12,0x0252,
      0x0F12,0x02C2,
      0x0F12,0x0241,
      0x0F12,0x02AB,
      0x0F12,0x0224,
      0x0F12,0x0292,
      0x0F12,0x0209,
      0x0F12,0x0275,
      0x0F12,0x0205,
      0x0F12,0x0258,
      0x0F12,0x0227,
      0x0F12,0x0235,
      0x0F12,0x0000,
      0x0F12,0x0000,
      0x0F12,0x0000,
      0x0F12,0x0000,
      0x0F12,0x0000,
      0x0F12,0x0000,
      0x0F12,0x0000,
      0x0F12,0x0000,
      0x002A,0x0BAA,
      0x0F12,0x0006,
      0x002A,0x0BAE,
      0x0F12,0x010E,
      0x0F12,0x02E9,
      0x002A,0x0BAC,
      0x0F12,0x0009,
      0x002A,0x0B7A,
      0x0F12,0x038C,
      0x0F12,0x03DA,
      0x0F12,0x0339,
      0x0F12,0x03E9,
      0x0F12,0x02FD,
      0x0F12,0x03A5,
      0x0F12,0x02A4,
      0x0F12,0x0362,
      0x0F12,0x0223,
      0x0F12,0x0315,
      0x0F12,0x01F4,
      0x0F12,0x02CF,
      0x0F12,0x01D7,
      0x0F12,0x028E,
      0x0F12,0x01CB,
      0x0F12,0x0258,
      0x0F12,0x022B,
      0x0F12,0x01CC,
      0x0F12,0x0000,
      0x0F12,0x0000,
      0x0F12,0x0000,
      0x0F12,0x0000,
      0x0F12,0x0000,
      0x0F12,0x0000,
      0x002A,0x0B70,
      0x0F12,0x0005,
      0x002A,0x0B74,
      0x0F12,0x01F8,
      0x0F12,0x02A8,
      0x002A,0x0B72,
      0x0F12,0x0007,
      0x002A,0x0B40,
      0x0F12,0x029E,
      0x0F12,0x02C8,
      0x0F12,0x0281,
      0x0F12,0x02C8,
      0x0F12,0x0266,
      0x0F12,0x02AC,
      0x0F12,0x0251,
      0x0F12,0x028E,
      0x0F12,0x023D,
      0x0F12,0x0275,
      0x0F12,0x0228,
      0x0F12,0x025D,
      0x0F12,0x0228,
      0x0F12,0x0243,
      0x0F12,0x0000,
      0x0F12,0x0000,
      0x0F12,0x0000,
      0x0F12,0x0000,
      0x0F12,0x0000,
      0x0F12,0x0000,
      0x0F12,0x0000,
      0x0F12,0x0000,
      0x0F12,0x0000,
      0x0F12,0x0000,
      0x002A,0x0BC8,
      0x0F12,0x0005,
      0x002A,0x0BCC,
      0x0F12,0x010F,
      0x0F12,0x018F,
      0x002A,0x0BCA,
      0x0F12,0x0005,
      0x002A,0x0BB4,
      0x0F12,0x03E7,
      0x0F12,0x03F8,
      0x0F12,0x03A7,
      0x0F12,0x03FC,
      0x0F12,0x0352,
      0x0F12,0x03D0,
      0x0F12,0x0322,
      0x0F12,0x039E,
      0x0F12,0x032B,
      0x0F12,0x034D,
      0x002A,0x0BE6,
      0x0F12,0x0006,
      0x002A,0x0BEA,
      0x0F12,0x019E,
      0x0F12,0x0257,
      0x002A,0x0BE8,
      0x0F12,0x0004,
      0x002A,0x0BD2,
      0x0F12,0x030B,
      0x0F12,0x0323,
      0x0F12,0x02C3,
      0x0F12,0x030F,
      0x0F12,0x0288,
      0x0F12,0x02E5,
      0x0F12,0x026A,
      0x0F12,0x02A2,
      0x0F12,0x0000,
      0x0F12,0x0000,
      0x002A,0x0C2C,
      0x0F12,0x0139,
      0x0F12,0x0122,
      0x002A,0x0BFC,
      0x0F12,0x03AD,
      0x0F12,0x013F,
      0x0F12,0x0341,
      0x0F12,0x017B,
      0x0F12,0x038D,
      0x0F12,0x014B,
      0x0F12,0x02C3,
      0x0F12,0x01CC,
      0x0F12,0x0241,
      0x0F12,0x027F,
      0x0F12,0x0241,
      0x0F12,0x027F,
      0x0F12,0x0214,
      0x0F12,0x02A8,

      0x0F12,0x0270,
      0x0F12,0x0210,

      0x002A,0x0C4C,
      0x0F12,0x0452,
      0x002A,0x0C58,
      0x0F12,0x059C,
      0x002A,0x0BF8,
      0x0F12,0x01AE,
      0x002A,0x0C28,
      0x0F12,0x0000,
      0x002A,0x0CAC,
      0x0F12,0x0050,
      0x002A,0x0C28,
      0x0F12,0x0000,
      0x002A,0x0D0E,
      0x0F12,0x00B8,
      0x0F12,0x00B2,
      0x002A,0x0CFE,
      0x0F12,0x0FAB,
      0x0F12,0x0FF5,
      0x0F12,0x10BB,
      0x0F12,0x1123,
      0x0F12,0x1165,
      0x0F12,0x122A,
      0x0F12,0x00A9,
      0x0F12,0x00C0,
      0x002A,0x0CF8,
      0x0F12,0x030E,
      0x0F12,0x034C,
      0x0F12,0x0388,

      0x002A,0x0CB0,
      0x0F12,0x0000,
      0x0F12,0x0000,
      0x0F12,0x0064,
      0x0F12,0x0064,
      0x0F12,0x0078,
      0x0F12,0x0078,

      0x0F12,0x0000,
      0x0F12,0x0000,
      0x0F12,0x0064,
      0x0F12,0x0064,
      0x0F12,0x0078,
      0x0F12,0x0078,

      0x0F12,0x0000,
      0x0F12,0x0000,
      0x0F12,0x0064,
      0x0F12,0x0064,
      0x0F12,0x0078,
      0x0F12,0x0078,

      0x0F12,0x0000,
      0x0F12,0x0000,
      0x0F12,0x0000,
      0x0F12,0xFFC0,
      0x0F12,0xFF60,
      0x0F12,0xFF40,

      0x0F12,0x0000,
      0x0F12,0x0000,
      0x0F12,0x0000,
      0x0F12,0xFFC0,
      0x0F12,0xFF60,
      0x0F12,0xFF40,

      0x0F12,0x0000,
      0x0F12,0x0000,
      0x0F12,0x0000,
      0x0F12,0xFFC0,
      0x0F12,0xFF60,
      0x0F12,0xFF40,

      0x002A,0x0D30,
      0x0F12,0x0002,

      0x002A,0x3362,
      0x0F12,0x0001,
      0x0F12,0x0000,
      0x0F12,0x0000,


      // For Outdoor,
      0x002A,0x0C86,
      0x0F12,0x0005,
      0x002A,0x0C70,
      0x0F12,0xFF7B,
      0x0F12,0x00CE,
      0x0F12,0xFF23,
      0x0F12,0x010D,
      0x0F12,0xFEF3,
      0x0F12,0x012C,
      0x0F12,0xFED7,
      0x0F12,0x014E,
      0x0F12,0xFEBB,
      0x0F12,0x0162,
      0x0F12,0x1388,
      0x002A,0x0C8A,
      0x0F12,0x4ACB,
      0x002A,0x0C88,
      0x0F12,0x0A7C,


      //from 89
      0x002A,0x33A4,
      0x0F12,0x01D0,
      0x0F12,0xFFA1,
      0x0F12,0xFFFA,
      0x0F12,0xFF6F,
      0x0F12,0x0140,
      0x0F12,0xFF49,
      0x0F12,0xFFC1,
      0x0F12,0x001F,
      0x0F12,0x01BD,
      0x0F12,0x013F,
      0x0F12,0x00E1,
      0x0F12,0xFF43,
      0x0F12,0x0191,
      0x0F12,0xFFC0,
      0x0F12,0x01B7,
      0x0F12,0xFF30,
      0x0F12,0x015F,
      0x0F12,0x0106,
      0x0F12,0x01D0,
      0x0F12,0xFFA1,
      0x0F12,0xFFFA,
      0x0F12,0xFF6F,
      0x0F12,0x0140,
      0x0F12,0xFF49,
      0x0F12,0xFFC1,
      0x0F12,0x001F,
      0x0F12,0x01BD,
      0x0F12,0x013F,
      0x0F12,0x00E1,
      0x0F12,0xFF43,
      0x0F12,0x0191,
      0x0F12,0xFFC0,
      0x0F12,0x01B7,
      0x0F12,0xFF30,
      0x0F12,0x015F,
      0x0F12,0x0106,
      0x0F12,0x01D0,
      0x0F12,0xFFA1,
      0x0F12,0xFFFA,
      0x0F12,0xFF6F,
      0x0F12,0x0140,
      0x0F12,0xFF49,
      0x0F12,0xFFE3,
      0x0F12,0xFFF9,
      0x0F12,0x01C1,
      0x0F12,0x013F,
      0x0F12,0x00E1,
      0x0F12,0xFF43,
      0x0F12,0x0191,
      0x0F12,0xFFC0,
      0x0F12,0x01B7,
      0x0F12,0xFF30,
      0x0F12,0x015F,
      0x0F12,0x0106,
      0x0F12,0x01D0,
      0x0F12,0xFFA1,
      0x0F12,0xFFFA,
      0x0F12,0xFF6F,
      0x0F12,0x0140,
      0x0F12,0xFF49,
      0x0F12,0xFFE3,
      0x0F12,0xFFF9,
      0x0F12,0x01C1,
      0x0F12,0x013F,
      0x0F12,0x00E1,
      0x0F12,0xFF43,
      0x0F12,0x0191,
      0x0F12,0xFFC0,
      0x0F12,0x01B7,
      0x0F12,0xFF30,
      0x0F12,0x015F,
      0x0F12,0x0106,
      0x0F12,0x01BF,
      0x0F12,0xFFBF,
      0x0F12,0xFFFE,
      0x0F12,0xFF6D,
      0x0F12,0x01B4,
      0x0F12,0xFF66,
      0x0F12,0xFFCA,
      0x0F12,0xFFCE,
      0x0F12,0x017B,
      0x0F12,0x0136,
      0x0F12,0x0132,
      0x0F12,0xFF85,
      0x0F12,0x018B,
      0x0F12,0xFF73,
      0x0F12,0x0191,
      0x0F12,0xFF3F,
      0x0F12,0x015B,
      0x0F12,0x00D0,
      0x0F12,0x01BF,
      0x0F12,0xFFBF,
      0x0F12,0xFFFE,
      0x0F12,0xFF6D,
      0x0F12,0x01B4,
      0x0F12,0xFF66,
      0x0F12,0xFFCA,
      0x0F12,0xFFCE,
      0x0F12,0x017B,
      0x0F12,0x0136,
      0x0F12,0x0132,
      0x0F12,0xFF85,
      0x0F12,0x018B,
      0x0F12,0xFF73,
      0x0F12,0x0191,
      0x0F12,0xFF3F,
      0x0F12,0x015B,
      0x0F12,0x00D0,
      0x002A,0x3380,
      0x0F12,0x01AC,
      0x0F12,0xFFD7,
      0x0F12,0x0019,
      0x0F12,0xFF49,
      0x0F12,0x01D9,
      0x0F12,0xFF63,
      0x0F12,0xFFCA,
      0x0F12,0xFFCE,
      0x0F12,0x017B,
      0x0F12,0x0132,
      0x0F12,0x012E,
      0x0F12,0xFF8D,
      0x0F12,0x018B,
      0x0F12,0xFF73,
      0x0F12,0x0191,
      0x0F12,0xFF3F,
      0x0F12,0x015B,
      0x0F12,0x00D0,
      0x002A,0x0612,
      0x0F12,0x009D,
      0x0F12,0x00D5,
      0x0F12,0x0103,
      0x0F12,0x0128,
      0x0F12,0x0166,
      0x0F12,0x0193,


      //============================================================
      // Pre-Gamma
      //============================================================
      0x002A, 0x0538, //LutPreDemNoBin
      0x0F12, 0x0000,
      0x0F12, 0x001F,
      0x0F12, 0x0035,
      0x0F12, 0x005A,
      0x0F12, 0x0095,
      0x0F12, 0x00E6,
      0x0F12, 0x0121,
      0x0F12, 0x0139,
      0x0F12, 0x0150,
      0x0F12, 0x0177,
      0x0F12, 0x019A,
      0x0F12, 0x01BB,
      0x0F12, 0x01DC,
      0x0F12, 0x0219,
      0x0F12, 0x0251,
      0x0F12, 0x02B3,
      0x0F12, 0x030A,
      0x0F12, 0x035F,
      0x0F12, 0x03B1,
      0x0F12, 0x03FF,

      0x0F12, 0x0000, //LutPostDemNoBin
      0x0F12, 0x0001,
      0x0F12, 0x0001,
      0x0F12, 0x0002,
      0x0F12, 0x0004,
      0x0F12, 0x000A,
      0x0F12, 0x0012,
      0x0F12, 0x0016,
      0x0F12, 0x001A,
      0x0F12, 0x0024,
      0x0F12, 0x0031,
      0x0F12, 0x003E,
      0x0F12, 0x004E,
      0x0F12, 0x0075,
      0x0F12, 0x00A8,
      0x0F12, 0x0126,
      0x0F12, 0x01BE,
      0x0F12, 0x0272,
      0x0F12, 0x0334,
      0x0F12, 0x03FF,

      //============================================================
      // Gamma
      //============================================================
      0x002A,0x0498,
      0x0F12,0x0000,
      0x0F12,0x0002,
      0x0F12,0x0007,
      0x0F12,0x001D,
      0x0F12,0x006E,
      0x0F12,0x00D3,
      0x0F12,0x0127,
      0x0F12,0x014C,
      0x0F12,0x016E,
      0x0F12,0x01A5,
      0x0F12,0x01D3,
      0x0F12,0x01FB,
      0x0F12,0x021F,
      0x0F12,0x0260,
      0x0F12,0x029A,
      0x0F12,0x02F7,
      0x0F12,0x034D,
      0x0F12,0x0395,
      0x0F12,0x03CE,
      0x0F12,0x03FF,
      0x0F12,0x0000,
      0x0F12,0x0004,
      0x0F12,0x000C,
      0x0F12,0x0024,
      0x0F12,0x006E,
      0x0F12,0x00D1,
      0x0F12,0x0119,
      0x0F12,0x0139,
      0x0F12,0x0157,
      0x0F12,0x018E,
      0x0F12,0x01C3,
      0x0F12,0x01F3,
      0x0F12,0x021F,
      0x0F12,0x0269,
      0x0F12,0x02A6,
      0x0F12,0x02FF,
      0x0F12,0x0351,
      0x0F12,0x0395,
      0x0F12,0x03CE,
      0x0F12,0x03FF,
      
      
      
      //============================================================
      // AFIT
      //============================================================

      0x002A,0x3370,
      0x0F12,0x0000,  // afit_bUseNormBrForAfit
      
      0x002A,0x06D4,
      0x0F12,0x0032,  // afit_uNoiseIndInDoor_0
      0x0F12,0x0078,  // afit_uNoiseIndInDoor_1
      0x0F12,0x00C8,  // afit_uNoiseIndInDoor_2
      0x0F12,0x0190,  // afit_uNoiseIndInDoor_3
      0x0F12,0x028C,  // afit_uNoiseIndInDoor_4
      
      0x002A,0x0734,
      0x0F12,0x0000, // AfitBaseVals_0__0_  Brightness[0]
      0x0F12,0x0000, // AfitBaseVals_0__1_  Contrast[0]
      0x0F12,0x0000, // AfitBaseVals_0__2_
      0x0F12,0x0000, // AfitBaseVals_0__3_
      0x0F12,0x0000, // AfitBaseVals_0__4_
      0x0F12,0x0078, // AfitBaseVals_0__5_
      0x0F12,0x012C, // AfitBaseVals_0__6_
      0x0F12,0x03FF, // AfitBaseVals_0__7_
      0x0F12,0x0014, // AfitBaseVals_0__8_
      0x0F12,0x0064, // AfitBaseVals_0__9_
      0x0F12,0x000C, // AfitBaseVals_0__10_
      0x0F12,0x0010, // AfitBaseVals_0__11_
      0x0F12,0x01E6, // AfitBaseVals_0__12_
      0x0F12,0x0000, // AfitBaseVals_0__13_
      0x0F12,0x0070, // AfitBaseVals_0__14_
      0x0F12,0x01FF, // AfitBaseVals_0__15_
      0x0F12,0x0144, // AfitBaseVals_0__16_
      0x0F12,0x000F, // AfitBaseVals_0__17_
      0x0F12,0x000A, // AfitBaseVals_0__18_
      0x0F12,0x0073, // AfitBaseVals_0__19_
      0x0F12,0x0087, // AfitBaseVals_0__20_
      0x0F12,0x0014, // AfitBaseVals_0__21_
      0x0F12,0x000A, // AfitBaseVals_0__22_
      0x0F12,0x0023, // AfitBaseVals_0__23_
      0x0F12,0x001E, // AfitBaseVals_0__24_
      0x0F12,0x0014, // AfitBaseVals_0__25_
      0x0F12,0x000A, // AfitBaseVals_0__26_
      0x0F12,0x0023, // AfitBaseVals_0__27_
      0x0F12,0x0046, // AfitBaseVals_0__28_
      0x0F12,0x2B32, // AfitBaseVals_0__29_
      0x0F12,0x0601, // AfitBaseVals_0__30_
      0x0F12,0x0000, // AfitBaseVals_0__31_
      0x0F12,0x0000, // AfitBaseVals_0__32_
      0x0F12,0x0000, // AfitBaseVals_0__33_
      0x0F12,0x00FF, // AfitBaseVals_0__34_
      0x0F12,0x07FF, // AfitBaseVals_0__35_
      0x0F12,0xFFFF, // AfitBaseVals_0__36_
      0x0F12,0x0000, // AfitBaseVals_0__37_
      0x0F12,0x050D, // AfitBaseVals_0__38_
      0x0F12,0x1E80, // AfitBaseVals_0__39_
      0x0F12,0x0000, // AfitBaseVals_0__40_
      0x0F12,0x1408, // AfitBaseVals_0__41_
      0x0F12,0x0214, // AfitBaseVals_0__42_
      0x0F12,0xFF01, // AfitBaseVals_0__43_
      0x0F12,0x180F, // AfitBaseVals_0__44_
      0x0F12,0x0001, // AfitBaseVals_0__45_
      0x0F12,0x0000, // AfitBaseVals_0__46_
      0x0F12,0x8003, // AfitBaseVals_0__47_
      0x0F12,0x0094, // AfitBaseVals_0__48_
      0x0F12,0x0580, // AfitBaseVals_0__49_
      0x0F12,0x0280, // AfitBaseVals_0__50_
      0x0F12,0x0308, // AfitBaseVals_0__51_
      0x0F12,0x3186, // AfitBaseVals_0__52_
      0x0F12,0x5260, // AfitBaseVals_0__53_
      0x0F12,0x0A02, // AfitBaseVals_0__54_
      0x0F12,0x080A, // AfitBaseVals_0__55_
      0x0F12,0x0500, // AfitBaseVals_0__56_
      0x0F12,0x032D, // AfitBaseVals_0__57_
      0x0F12,0x324E, // AfitBaseVals_0__58_
      0x0F12,0x001E, // AfitBaseVals_0__59_
      0x0F12,0x0200, // AfitBaseVals_0__60_
      0x0F12,0x0103, // AfitBaseVals_0__61_
      0x0F12,0x010C, // AfitBaseVals_0__62_
      0x0F12,0x9696, // AfitBaseVals_0__63_
      0x0F12,0x4646, // AfitBaseVals_0__64_
      0x0F12,0x0802, // AfitBaseVals_0__65_
      0x0F12,0x0802, // AfitBaseVals_0__66_
      0x0F12,0x0000, // AfitBaseVals_0__67_
      0x0F12,0x030F, // AfitBaseVals_0__68_
      0x0F12,0x3202, // AfitBaseVals_0__69_
      0x0F12,0x0F1E, // AfitBaseVals_0__70_
      0x0F12,0x020F, // AfitBaseVals_0__71_
      0x0F12,0x0103, // AfitBaseVals_0__72_
      0x0F12,0x010C, // AfitBaseVals_0__73_
      0x0F12,0x9696, // AfitBaseVals_0__74_
      0x0F12,0x4646, // AfitBaseVals_0__75_
      0x0F12,0x0802, // AfitBaseVals_0__76_
      0x0F12,0x0802, // AfitBaseVals_0__77_
      0x0F12,0x0000, // AfitBaseVals_0__78_
      0x0F12,0x030F, // AfitBaseVals_0__79_
      0x0F12,0x3202, // AfitBaseVals_0__80_
      0x0F12,0x0F1E, // AfitBaseVals_0__81_
      0x0F12,0x020F, // AfitBaseVals_0__82_
      0x0F12,0x0003, // AfitBaseVals_0__83_
      0x0F12,0x0000, // AfitBaseVals_1__0_  Brightness[1]
      0x0F12,0x0000, // AfitBaseVals_1__1_  Contrast[1]
      0x0F12,0x0000, // AfitBaseVals_1__2_
      0x0F12,0x0000, // AfitBaseVals_1__3_
      0x0F12,0x0000, // AfitBaseVals_1__4_
      0x0F12,0x006A, // AfitBaseVals_1__5_
      0x0F12,0x012C, // AfitBaseVals_1__6_
      0x0F12,0x03FF, // AfitBaseVals_1__7_
      0x0F12,0x0014, // AfitBaseVals_1__8_
      0x0F12,0x0064, // AfitBaseVals_1__9_
      0x0F12,0x000C, // AfitBaseVals_1__10_
      0x0F12,0x0010, // AfitBaseVals_1__11_
      0x0F12,0x01E6, // AfitBaseVals_1__12_
      0x0F12,0x03FF, // AfitBaseVals_1__13_
      0x0F12,0x0070, // AfitBaseVals_1__14_
      0x0F12,0x007D, // AfitBaseVals_1__15_
      0x0F12,0x0064, // AfitBaseVals_1__16_
      0x0F12,0x0014, // AfitBaseVals_1__17_
      0x0F12,0x000A, // AfitBaseVals_1__18_
      0x0F12,0x0073, // AfitBaseVals_1__19_
      0x0F12,0x0087, // AfitBaseVals_1__20_
      0x0F12,0x0014, // AfitBaseVals_1__21_
      0x0F12,0x000A, // AfitBaseVals_1__22_
      0x0F12,0x0023, // AfitBaseVals_1__23_
      0x0F12,0x001E, // AfitBaseVals_1__24_
      0x0F12,0x0014, // AfitBaseVals_1__25_
      0x0F12,0x000A, // AfitBaseVals_1__26_
      0x0F12,0x0023, // AfitBaseVals_1__27_
      0x0F12,0x001E, // AfitBaseVals_1__28_
      0x0F12,0x2B32, // AfitBaseVals_1__29_
      0x0F12,0x0601, // AfitBaseVals_1__30_
      0x0F12,0x0000, // AfitBaseVals_1__31_
      0x0F12,0x0000, // AfitBaseVals_1__32_
      0x0F12,0x0000, // AfitBaseVals_1__33_
      0x0F12,0x00FF, // AfitBaseVals_1__34_
      0x0F12,0x07FF, // AfitBaseVals_1__35_
      0x0F12,0xFFFF, // AfitBaseVals_1__36_
      0x0F12,0x0000, // AfitBaseVals_1__37_
      0x0F12,0x050D, // AfitBaseVals_1__38_
      0x0F12,0x1E80, // AfitBaseVals_1__39_
      0x0F12,0x0000, // AfitBaseVals_1__40_
      0x0F12,0x1408, // AfitBaseVals_1__41_
      0x0F12,0x0214, // AfitBaseVals_1__42_
      0x0F12,0xFF01, // AfitBaseVals_1__43_
      0x0F12,0x180F, // AfitBaseVals_1__44_
      0x0F12,0x0002, // AfitBaseVals_1__45_
      0x0F12,0x0000, // AfitBaseVals_1__46_
      0x0F12,0x8003, // AfitBaseVals_1__47_
      0x0F12,0x0080, // AfitBaseVals_1__48_
      0x0F12,0x0080, // AfitBaseVals_1__49_
      0x0F12,0x0280, // AfitBaseVals_1__50_
      0x0F12,0x0308, // AfitBaseVals_1__51_
      0x0F12,0x1E65, // AfitBaseVals_1__52_
      0x0F12,0x1A24, // AfitBaseVals_1__53_
      0x0F12,0x0A03, // AfitBaseVals_1__54_
      0x0F12,0x080A, // AfitBaseVals_1__55_
      0x0F12,0x0500, // AfitBaseVals_1__56_
      0x0F12,0x032D, // AfitBaseVals_1__57_
      0x0F12,0x324D, // AfitBaseVals_1__58_
      0x0F12,0x001E, // AfitBaseVals_1__59_
      0x0F12,0x0200, // AfitBaseVals_1__60_
      0x0F12,0x0103, // AfitBaseVals_1__61_
      0x0F12,0x010C, // AfitBaseVals_1__62_
      0x0F12,0x9696, // AfitBaseVals_1__63_
      0x0F12,0x2F34, // AfitBaseVals_1__64_
      0x0F12,0x0504, // AfitBaseVals_1__65_
      0x0F12,0x080F, // AfitBaseVals_1__66_
      0x0F12,0x0000, // AfitBaseVals_1__67_
      0x0F12,0x030F, // AfitBaseVals_1__68_
      0x0F12,0x3208, // AfitBaseVals_1__69_
      0x0F12,0x0F1E, // AfitBaseVals_1__70_
      0x0F12,0x020F, // AfitBaseVals_1__71_
      0x0F12,0x0103, // AfitBaseVals_1__72_
      0x0F12,0x010C, // AfitBaseVals_1__73_
      0x0F12,0x9696, // AfitBaseVals_1__74_
      0x0F12,0x1414, // AfitBaseVals_1__75_
      0x0F12,0x0504, // AfitBaseVals_1__76_
      0x0F12,0x080F, // AfitBaseVals_1__77_
      0x0F12,0x0000, // AfitBaseVals_1__78_
      0x0F12,0x030F, // AfitBaseVals_1__79_
      0x0F12,0x3208, // AfitBaseVals_1__80_
      0x0F12,0x0F1E, // AfitBaseVals_1__81_
      0x0F12,0x020F, // AfitBaseVals_1__82_
      0x0F12,0x0003, // AfitBaseVals_1__83_
      0x0F12,0x0000, // AfitBaseVals_2__0_  Brightness[2]
      0x0F12,0x0000, // AfitBaseVals_2__1_  Contrast[2]
      0x0F12,0x0000, // AfitBaseVals_2__2_
      0x0F12,0x0000, // AfitBaseVals_2__3_
      0x0F12,0x0000, // AfitBaseVals_2__4_
      0x0F12,0x0064, // AfitBaseVals_2__5_
      0x0F12,0x012C, // AfitBaseVals_2__6_
      0x0F12,0x03FF, // AfitBaseVals_2__7_
      0x0F12,0x0014, // AfitBaseVals_2__8_
      0x0F12,0x0064, // AfitBaseVals_2__9_
      0x0F12,0x000C, // AfitBaseVals_2__10_
      0x0F12,0x0010, // AfitBaseVals_2__11_
      0x0F12,0x01E6, // AfitBaseVals_2__12_
      0x0F12,0x03FF, // AfitBaseVals_2__13_
      0x0F12,0x0070, // AfitBaseVals_2__14_
      0x0F12,0x007D, // AfitBaseVals_2__15_
      0x0F12,0x0064, // AfitBaseVals_2__16_
      0x0F12,0x0096, // AfitBaseVals_2__17_
      0x0F12,0x0096, // AfitBaseVals_2__18_
      0x0F12,0x0073, // AfitBaseVals_2__19_
      0x0F12,0x0087, // AfitBaseVals_2__20_
      0x0F12,0x0014, // AfitBaseVals_2__21_
      0x0F12,0x0019, // AfitBaseVals_2__22_
      0x0F12,0x0023, // AfitBaseVals_2__23_
      0x0F12,0x001E, // AfitBaseVals_2__24_
      0x0F12,0x0014, // AfitBaseVals_2__25_
      0x0F12,0x0019, // AfitBaseVals_2__26_
      0x0F12,0x0023, // AfitBaseVals_2__27_
      0x0F12,0x001E, // AfitBaseVals_2__28_
      0x0F12,0x2B32, // AfitBaseVals_2__29_
      0x0F12,0x0601, // AfitBaseVals_2__30_
      0x0F12,0x0000, // AfitBaseVals_2__31_
      0x0F12,0x0000, // AfitBaseVals_2__32_
      0x0F12,0x0000, // AfitBaseVals_2__33_
      0x0F12,0x00FF, // AfitBaseVals_2__34_
      0x0F12,0x07FF, // AfitBaseVals_2__35_
      0x0F12,0xFFFF, // AfitBaseVals_2__36_
      0x0F12,0x0000, // AfitBaseVals_2__37_
      0x0F12,0x050D, // AfitBaseVals_2__38_
      0x0F12,0x1E80, // AfitBaseVals_2__39_
      0x0F12,0x0000, // AfitBaseVals_2__40_
      0x0F12,0x0A08, // AfitBaseVals_2__41_
      0x0F12,0x0200, // AfitBaseVals_2__42_
      0x0F12,0xFF01, // AfitBaseVals_2__43_
      0x0F12,0x180F, // AfitBaseVals_2__44_
      0x0F12,0x0002, // AfitBaseVals_2__45_
      0x0F12,0x0000, // AfitBaseVals_2__46_
      0x0F12,0x8003, // AfitBaseVals_2__47_
      0x0F12,0x0080, // AfitBaseVals_2__48_
      0x0F12,0x0080, // AfitBaseVals_2__49_
      0x0F12,0x0280, // AfitBaseVals_2__50_
      0x0F12,0x0208, // AfitBaseVals_2__51_
      0x0F12,0x1E4B, // AfitBaseVals_2__52_
      0x0F12,0x1A24, // AfitBaseVals_2__53_
      0x0F12,0x0A05, // AfitBaseVals_2__54_
      0x0F12,0x080A, // AfitBaseVals_2__55_
      0x0F12,0x0500, // AfitBaseVals_2__56_
      0x0F12,0x032D, // AfitBaseVals_2__57_
      0x0F12,0x324D, // AfitBaseVals_2__58_
      0x0F12,0x001E, // AfitBaseVals_2__59_
      0x0F12,0x0200, // AfitBaseVals_2__60_
      0x0F12,0x0103, // AfitBaseVals_2__61_
      0x0F12,0x010C, // AfitBaseVals_2__62_
      0x0F12,0x9696, // AfitBaseVals_2__63_
      0x0F12,0x1E23, // AfitBaseVals_2__64_
      0x0F12,0x0505, // AfitBaseVals_2__65_
      0x0F12,0x080F, // AfitBaseVals_2__66_
      0x0F12,0x0000, // AfitBaseVals_2__67_
      0x0F12,0x030F, // AfitBaseVals_2__68_
      0x0F12,0x3208, // AfitBaseVals_2__69_
      0x0F12,0x0F1E, // AfitBaseVals_2__70_
      0x0F12,0x020F, // AfitBaseVals_2__71_
      0x0F12,0x0103, // AfitBaseVals_2__72_
      0x0F12,0x010C, // AfitBaseVals_2__73_
      0x0F12,0x9696, // AfitBaseVals_2__74_
      0x0F12,0x1E23, // AfitBaseVals_2__75_
      0x0F12,0x0505, // AfitBaseVals_2__76_
      0x0F12,0x080F, // AfitBaseVals_2__77_
      0x0F12,0x0000, // AfitBaseVals_2__78_
      0x0F12,0x030F, // AfitBaseVals_2__79_
      0x0F12,0x3208, // AfitBaseVals_2__80_
      0x0F12,0x0F1E, // AfitBaseVals_2__81_
      0x0F12,0x020F, // AfitBaseVals_2__82_
      0x0F12,0x0003, // AfitBaseVals_2__83_
      0x0F12,0x0000, // AfitBaseVals_3__0_  Brightness[3]
      0x0F12,0x0018, // 0000 AfitBaseVals_3__1_  Contrast[3]
      0x0F12,0x0000, // AfitBaseVals_3__2_
      0x0F12,0x0000, // AfitBaseVals_3__3_
      0x0F12,0x0000, // AfitBaseVals_3__4_
      0x0F12,0x0064, // AfitBaseVals_3__5_
      0x0F12,0x012C, // AfitBaseVals_3__6_
      0x0F12,0x03FF, // AfitBaseVals_3__7_
      0x0F12,0x0014, // AfitBaseVals_3__8_
      0x0F12,0x0064, // AfitBaseVals_3__9_
      0x0F12,0x000C, // AfitBaseVals_3__10_
      0x0F12,0x0010, // AfitBaseVals_3__11_
      0x0F12,0x01E6, // AfitBaseVals_3__12_
      0x0F12,0x0000, // AfitBaseVals_3__13_
      0x0F12,0x0070, // AfitBaseVals_3__14_
      0x0F12,0x007D, // AfitBaseVals_3__15_
      0x0F12,0x0064, // AfitBaseVals_3__16_
      0x0F12,0x0096, // AfitBaseVals_3__17_
      0x0F12,0x0096, // AfitBaseVals_3__18_
      0x0F12,0x0073, // AfitBaseVals_3__19_
      0x0F12,0x009F, // AfitBaseVals_3__20_
      0x0F12,0x0028, // AfitBaseVals_3__21_
      0x0F12,0x0028, // AfitBaseVals_3__22_
      0x0F12,0x0023, // AfitBaseVals_3__23_
      0x0F12,0x0037, // AfitBaseVals_3__24_
      0x0F12,0x0028, // AfitBaseVals_3__25_
      0x0F12,0x0028, // AfitBaseVals_3__26_
      0x0F12,0x0023, // AfitBaseVals_3__27_
      0x0F12,0x0037, // AfitBaseVals_3__28_
      0x0F12,0x2B32, // AfitBaseVals_3__29_
      0x0F12,0x0601, // AfitBaseVals_3__30_
      0x0F12,0x0000, // AfitBaseVals_3__31_
      0x0F12,0x0000, // AfitBaseVals_3__32_
      0x0F12,0x0000, // AfitBaseVals_3__33_
      0x0F12,0x00FF, // AfitBaseVals_3__34_
      0x0F12,0x07A0, // AfitBaseVals_3__35_
      0x0F12,0xFFFF, // AfitBaseVals_3__36_
      0x0F12,0x0000, // AfitBaseVals_3__37_
      0x0F12,0x050D, // AfitBaseVals_3__38_
      0x0F12,0x1E80, // AfitBaseVals_3__39_
      0x0F12,0x0000, // AfitBaseVals_3__40_
      0x0F12,0x0A08, // AfitBaseVals_3__41_
      0x0F12,0x0200, // AfitBaseVals_3__42_
      0x0F12,0xFF01, // AfitBaseVals_3__43_
      0x0F12,0x180F, // AfitBaseVals_3__44_
      0x0F12,0x0001, // AfitBaseVals_3__45_
      0x0F12,0x0000, // AfitBaseVals_3__46_
      0x0F12,0x8003, // AfitBaseVals_3__47_
      0x0F12,0x0080, // AfitBaseVals_3__48_
      0x0F12,0x0080, // AfitBaseVals_3__49_
      0x0F12,0x0280, // AfitBaseVals_3__50_
      0x0F12,0x0108, // AfitBaseVals_3__51_
      0x0F12,0x1E32, // AfitBaseVals_3__52_
      0x0F12,0x1A24, // AfitBaseVals_3__53_
      0x0F12,0x0A05, // AfitBaseVals_3__54_
      0x0F12,0x080A, // AfitBaseVals_3__55_
      0x0F12,0x0000, // AfitBaseVals_3__56_
      0x0F12,0x0328, // AfitBaseVals_3__57_
      0x0F12,0x324C, // AfitBaseVals_3__58_
      0x0F12,0x001E, // AfitBaseVals_3__59_
      0x0F12,0x0200, // AfitBaseVals_3__60_
      0x0F12,0x0103, // AfitBaseVals_3__61_
      0x0F12,0x010C, // AfitBaseVals_3__62_
      0x0F12,0x9696, // AfitBaseVals_3__63_
      0x0F12,0x0F0F, // AfitBaseVals_3__64_
      0x0F12,0x0307, // AfitBaseVals_3__65_
      0x0F12,0x080F, // AfitBaseVals_3__66_
      0x0F12,0x0000, // AfitBaseVals_3__67_
      0x0F12,0x030F, // AfitBaseVals_3__68_
      0x0F12,0x3208, // AfitBaseVals_3__69_
      0x0F12,0x0F1E, // AfitBaseVals_3__70_
      0x0F12,0x020F, // AfitBaseVals_3__71_
      0x0F12,0x0103, // AfitBaseVals_3__72_
      0x0F12,0x010C, // AfitBaseVals_3__73_
      0x0F12,0x9696, // AfitBaseVals_3__74_
      0x0F12,0x0F0F, // AfitBaseVals_3__75_
      0x0F12,0x0307, // AfitBaseVals_3__76_
      0x0F12,0x080F, // AfitBaseVals_3__77_
      0x0F12,0x0000, // AfitBaseVals_3__78_
      0x0F12,0x030F, // AfitBaseVals_3__79_
      0x0F12,0x3208, // AfitBaseVals_3__80_
      0x0F12,0x0F1E, // AfitBaseVals_3__81_
      0x0F12,0x020F, // AfitBaseVals_3__82_
      0x0F12,0x0003, // AfitBaseVals_3__83_
      0x0F12,0x0000, // AfitBaseVals_4__0_  Brightness[4]
      0x0F12,0x0014, // 0000 AfitBaseVals_4__1_  Contrast[4]
      0x0F12,0x0000, // AfitBaseVals_4__2_
      0x0F12,0x0000, // AfitBaseVals_4__3_
      0x0F12,0x0000, // AfitBaseVals_4__4_
      0x0F12,0x0028, // AfitBaseVals_4__5_
      0x0F12,0x012C, // AfitBaseVals_4__6_
      0x0F12,0x03FF, // AfitBaseVals_4__7_
      0x0F12,0x0014, // AfitBaseVals_4__8_
      0x0F12,0x0064, // AfitBaseVals_4__9_
      0x0F12,0x000C, // AfitBaseVals_4__10_
      0x0F12,0x0010, // AfitBaseVals_4__11_
      0x0F12,0x01E6, // AfitBaseVals_4__12_
      0x0F12,0x0000, // AfitBaseVals_4__13_
      0x0F12,0x0070, // AfitBaseVals_4__14_
      0x0F12,0x0087, // AfitBaseVals_4__15_
      0x0F12,0x0073, // AfitBaseVals_4__16_
      0x0F12,0x0096, // AfitBaseVals_4__17_
      0x0F12,0x0096, // AfitBaseVals_4__18_
      0x0F12,0x0073, // AfitBaseVals_4__19_
      0x0F12,0x00B4, // AfitBaseVals_4__20_
      0x0F12,0x0028, // AfitBaseVals_4__21_
      0x0F12,0x0028, // AfitBaseVals_4__22_
      0x0F12,0x0023, // AfitBaseVals_4__23_
      0x0F12,0x0046, // AfitBaseVals_4__24_
      0x0F12,0x0028, // AfitBaseVals_4__25_
      0x0F12,0x0028, // AfitBaseVals_4__26_
      0x0F12,0x0023, // AfitBaseVals_4__27_
      0x0F12,0x0046, // AfitBaseVals_4__28_
      0x0F12,0x2B23, // AfitBaseVals_4__29_
      0x0F12,0x0601, // AfitBaseVals_4__30_
      0x0F12,0x0000, // AfitBaseVals_4__31_
      0x0F12,0x0000, // AfitBaseVals_4__32_
      0x0F12,0x0000, // AfitBaseVals_4__33_
      0x0F12,0x00FF, // AfitBaseVals_4__34_
      0x0F12,0x0B84, // AfitBaseVals_4__35_
      0x0F12,0xFFFF, // AfitBaseVals_4__36_
      0x0F12,0x0000, // AfitBaseVals_4__37_
      0x0F12,0x050D, // AfitBaseVals_4__38_
      0x0F12,0x1E80, // AfitBaseVals_4__39_
      0x0F12,0x0000, // AfitBaseVals_4__40_
      0x0F12,0x0A08, // AfitBaseVals_4__41_
      0x0F12,0x0200, // AfitBaseVals_4__42_
      0x0F12,0xFF01, // AfitBaseVals_4__43_
      0x0F12,0x180F, // AfitBaseVals_4__44_
      0x0F12,0x0001, // AfitBaseVals_4__45_
      0x0F12,0x0000, // AfitBaseVals_4__46_
      0x0F12,0x8003, // AfitBaseVals_4__47_
      0x0F12,0x0080, // AfitBaseVals_4__48_
      0x0F12,0x0080, // AfitBaseVals_4__49_
      0x0F12,0x0280, // AfitBaseVals_4__50_
      0x0F12,0x0108, // AfitBaseVals_4__51_
      0x0F12,0x1E1E, // AfitBaseVals_4__52_
      0x0F12,0x1419, // AfitBaseVals_4__53_
      0x0F12,0x0A0A, // AfitBaseVals_4__54_
      0x0F12,0x0800, // AfitBaseVals_4__55_
      0x0F12,0x0000, // AfitBaseVals_4__56_
      0x0F12,0x0328, // AfitBaseVals_4__57_
      0x0F12,0x324C, // AfitBaseVals_4__58_
      0x0F12,0x001E, // AfitBaseVals_4__59_
      0x0F12,0x0200, // AfitBaseVals_4__60_
      0x0F12,0x0103, // AfitBaseVals_4__61_
      0x0F12,0x010C, // AfitBaseVals_4__62_
      0x0F12,0x6464, // AfitBaseVals_4__63_
      0x0F12,0x0F0F, // AfitBaseVals_4__64_
      0x0F12,0x0307, // AfitBaseVals_4__65_
      0x0F12,0x080F, // AfitBaseVals_4__66_
      0x0F12,0x0000, // AfitBaseVals_4__67_
      0x0F12,0x030F, // AfitBaseVals_4__68_
      0x0F12,0x3208, // AfitBaseVals_4__69_
      0x0F12,0x0F1E, // AfitBaseVals_4__70_
      0x0F12,0x020F, // AfitBaseVals_4__71_
      0x0F12,0x0103, // AfitBaseVals_4__72_
      0x0F12,0x010C, // AfitBaseVals_4__73_
      0x0F12,0x6464, // AfitBaseVals_4__74_
      0x0F12,0x0F0F, // AfitBaseVals_4__75_
      0x0F12,0x0307, // AfitBaseVals_4__76_
      0x0F12,0x080F, // AfitBaseVals_4__77_
      0x0F12,0x0000, // AfitBaseVals_4__78_
      0x0F12,0x030F, // AfitBaseVals_4__79_
      0x0F12,0x3208, // AfitBaseVals_4__80_
      0x0F12,0x0F1E, // AfitBaseVals_4__81_
      0x0F12,0x020F, // AfitBaseVals_4__82_
      0x0F12,0x0003, // AfitBaseVals_4__83_
      0x0F12,0x7F5E,  // ConstAfitBaseVals_0_
      0x0F12,0xFEEE,  // ConstAfitBaseVals_1_
      0x0F12,0xD9B7,  // ConstAfitBaseVals_2_
      0x0F12,0x0472,  // ConstAfitBaseVals_3_
      0x0F12,0x0001,  // ConstAfitBaseVals_4_

      //(0x002A ,0x0408);        chris 20130322
      //(0x0F12 ,0x067F); //REG_TC_DBG_AutoAlgEnBits all AA are on   chris 20130322
#if 0
      //============================================================
      // User Control
      //============================================================
      0x002A, 0x018E,
      0x0F12, 0x0000, //Brightness
      0x0F12, 0x0000, //contrast
      0x0F12, 0x0010, //Saturation
      0x0F12, 0x0000, //sharpness
#endif

      //============================================================
      // Flicker
      //============================================================
      0x002A, 0x0408,
      0x0F12, 0x065F,
      0x002A, 0x03F4,
      0x0F12, 0x0001,
      0x0F12, 0x0001,

      //============================================================
      // PLL
      //============================================================
      0x002A, 0x012E,
      0x0F12, S5K8AAYX_MCLK*1000, //input clock
      0x0F12, 0x0000,
      0x002A, 0x0146,
      0x0F12, 0x0002, //REG_TC_IPRM_UseNPviClocks
      0x0F12, 0x0000, //REG_TC_IPRM_UseNMipiClocks

      0x002A, 0x014C,
      0x0F12, 0x2C4A, //REG_TC_IPRM_sysClocks_0 45.352MHz chris 20130322
      0x002A, 0x0152,
      0x0F12, 0x57E4, //REG_TC_IPRM_MinOutRate4KHz_0 90MHz chris 20130322
      0x002A, 0x014E,
      0x0F12, 0x57E4, //REG_TC_IPRM_MaxOutRate4KHz_0 90MHz chris 20130322
      0x002A, 0x0154,
      0x0F12, 0x2981, //29FE //REG_TC_IPRM_sysClocks_1
      0x002A, 0x015A,
      0x0F12, 0x5208, //5302  //REG_TC_IPRM_MinOutRate4KHz_1
      0x002A, 0x0156,
      0x0F12, 0x53FC, //54F6 //REG_TC_IPRM_MaxOutRate4KHz_1

      0x002A, 0x0164, //update PLL
      0x0F12, 0x0001,

      //============================================================
      // Preview config0 1280*960 15~30fps
      //============================================================
      0x002A, 0x01BE,
      0x0F12, 0x0500, //REG_0TC_PCFG_usWidth//1280
      0x0F12, 0x03C0, //REG_0TC_PCFG_usHeight//960
      0x0F12, 0x0005, //REG_0TC_PCFG_Format
      0x0F12, 0x0060, //0x40:OK//REG_0TC_PCFG_PVIMask
      0x0F12, 0x0000, //REG_0TC_PCFG_OIFMask
      0x0F12, 0x0000, //REG_0TC_PCFG_uClockInd
      0x002A, 0x01D2,
      0x0F12, 0x0000, //REG_0TC_PCFG_usFrTimeType
      0x0F12, 0x0002, //REG_0TC_PCFG_FrRateQualityType
      0x0F12, 0x014D, //REG_0TC_PCFG_usMinFrTimeMsecMult10
      0x0F12, 0x029A, //REG_0TC_PCFG_usMaxFrTimeMsecMult10
      0x002A, 0x01E8,
      0x0F12, 0x0000, //REG_0TC_PCFG_uPrevMirror
      0x0F12, 0x0000, //REG_0TC_PCFG_uPCaptureMirror

      //============================================================
      // Preview config1 1280*960 30fps
      //============================================================
      0x002A, 0x01EE,
      0x0F12, 0x0500, //REG_0TC_PCFG_usWidth//1280
      0x0F12, 0x03C0, //REG_0TC_PCFG_usHeight//960
      0x0F12, 0x0005, //REG_0TC_PCFG_Format
      0x0F12, 0x0060, //0x40:OK//REG_0TC_PCFG_PVIMask
      0x0F12, 0x0000, //REG_0TC_PCFG_OIFMask
      0x0F12, 0x0000, //REG_0TC_PCFG_uClockInd
      0x002A, 0x0202,
      0x0F12, 0x0002, //REG_0TC_PCFG_usFrTimeType
      0x0F12, 0x0001, //REG_0TC_PCFG_FrRateQualityType
      0x0F12, 0x014D, //REG_0TC_PCFG_usMinFrTimeMsecMult10
      0x0F12, 0x014D, //REG_0TC_PCFG_usMaxFrTimeMsecMult10
      0x002A, 0x0218,
      0x0F12, 0x0000, //REG_0TC_PCFG_uPrevMirror
      0x0F12, 0x0000, //REG_0TC_PCFG_uPCaptureMirror


      //============================================================
      // Capture configuration 0  15fps~30fps
      //============================================================
      0x002A, 0x02AE,
      0x0F12, 0x0000, //REG_0TC_CCFG_uCaptureMode
      0x0F12, 0x0500, //REG_0TC_CCFG_usWidth
      0x0F12, 0x03C0, //REG_0TC_CCFG_usHeight
      0x0F12, 0x0005, //REG_0TC_CCFG_Format
      0x0F12, 0x0060, //REG_0TC_CCFG_PVIMask
      0x0F12, 0x0000, //REG_0TC_CCFG_OIFMask
      0x0F12, 0x0000, //REG_0TC_CCFG_uClockInd
      0x002A, 0x02C4,
      0x0F12, 0x0000, //REG_0TC_CCFG_usFrTimeType
      0x0F12, 0x0002, //REG_0TC_CCFG_FrRateQualityType
      0x0F12, 0x014D, //REG_0TC_CCFG_usMinFrTimeMsecMult10
      0x0F12, 0x029A, //REG_0TC_CCFG_usMaxFrTimeMsecMult10

      //============================================================
      // active preview configure
      //============================================================
      0x002A, 0x01A8,
      0x0F12, 0x0000,  // #REG_TC_GP_ActivePrevConfig
      0x002A, 0x01AC,
      0x0F12, 0x0001,  // #REG_TC_GP_PrevOpenAfterChange
      0x002A, 0x01A6,
      0x0F12, 0x0001,  // #REG_TC_GP_NewConfigSync
      0x002A, 0x01AA,
      0x0F12, 0x0001,  // #REG_TC_GP_PrevConfigChanged
      0x002A, 0x019E,
      0x0F12, 0x0001,  // #REG_TC_GP_EnablePreview
      0x0F12, 0x0001,  // #REG_TC_GP_EnablePreviewChanged

      0x0028, 0xD000,
      0x002A, 0x1000,  // chris 20130322
      0x1000, 0x0001,
    };
    S5K8AAYX_table_write_cmos_sensor(addr_data_pair, sizeof(addr_data_pair)/sizeof(kal_uint16));
  }
  SENSORDB("S5K8AAYX_Initialize_Setting Done\n");

}


void S5K8AAYX_SetFrameRate(MSDK_SCENARIO_ID_ENUM scen, UINT16 u2FrameRate)
{

    //spin_lock(&s5k8aayx_drv_lock);
    //MSDK_SCENARIO_ID_ENUM scen = S5K8AAYXCurrentScenarioId;
    //spin_unlock(&s5k8aayx_drv_lock);
    UINT32 u4frameTime = (1000 * 10) / u2FrameRate;

    if (15 >= u2FrameRate)
    {
       switch (scen)
       {
           default:
           case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            //7.5fps ~30fps
            S5K8AAYX_write_cmos_sensor(0x0028, 0x7000);
            S5K8AAYX_write_cmos_sensor(0x002A, 0x01D6);
            S5K8AAYX_write_cmos_sensor(0x0F12, 0x014D); //REG_xTC_PCFG_usMinFrTimeMsecMult10 //014Ah:30fps
            S5K8AAYX_write_cmos_sensor(0x0F12, 0x0535); //REG_xTC_PCFG_usMaxFrTimeMsecMult10 //09C4h:4fps
            //To make Preview/Capture with the same exposure time, 
            //update Capture-Config here...also.
            S5K8AAYX_write_cmos_sensor(0x002A, 0x02C8);
            S5K8AAYX_write_cmos_sensor(0x0F12, 0x014D); //REG_0TC_CCFG_usMinFrTimeMsecMult10 //014Ah:30fps
            S5K8AAYX_write_cmos_sensor(0x0F12, 0x0535); //REG_0TC_CCFG_usMaxFrTimeMsecMult10 //09C4h:4fps

            break;
           case MSDK_SCENARIO_ID_VIDEO_PREVIEW: //15fps
            S5K8AAYX_write_cmos_sensor(0x0028, 0x7000);
            S5K8AAYX_write_cmos_sensor(0x002A ,0x0206);
            S5K8AAYX_write_cmos_sensor(0x0F12, u4frameTime); //REG_xTC_PCFG_usMaxFrTimeMsecMult10 //09C4h:4fps
            S5K8AAYX_write_cmos_sensor(0x0F12, u4frameTime); //REG_xTC_PCFG_usMinFrTimeMsecMult10 //014Ah:30fps
            break;
        }
    }
    else
    {
      switch (scen)
      {
         default:
         case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            //15~30fps
            S5K8AAYX_write_cmos_sensor(0x0028, 0x7000);
            S5K8AAYX_write_cmos_sensor(0x002A, 0x01D6);
            S5K8AAYX_write_cmos_sensor(0x0F12, 0x014D); //REG_xTC_PCFG_usMinFrTimeMsecMult10 //014Ah:30fps
            S5K8AAYX_write_cmos_sensor(0x0F12, 0x029A); //REG_xTC_PCFG_usMaxFrTimeMsecMult10 //09C4h:4fps

            //To make Preview/Capture with the same exposure time, 
            //update Capture-Config here...also.
            S5K8AAYX_write_cmos_sensor(0x002A, 0x02C8);
            S5K8AAYX_write_cmos_sensor(0x0F12, 0x014D); //REG_0TC_CCFG_usMinFrTimeMsecMult10 //014Ah:30fps
            S5K8AAYX_write_cmos_sensor(0x0F12, 0x029A); //REG_0TC_CCFG_usMaxFrTimeMsecMult10 //09C4h:4fps
            break;
         case MSDK_SCENARIO_ID_VIDEO_PREVIEW: //30fps
            S5K8AAYX_write_cmos_sensor(0x0028, 0x7000);
            S5K8AAYX_write_cmos_sensor(0x002A ,0x0206);
            S5K8AAYX_write_cmos_sensor(0x0F12, u4frameTime); //REG_xTC_PCFG_usMaxFrTimeMsecMult10 //09C4h:4fps
            S5K8AAYX_write_cmos_sensor(0x0F12, u4frameTime); //REG_xTC_PCFG_usMinFrTimeMsecMult10 //014Ah:30fps
            break;
      }
    }

    S5K8AAYX_write_cmos_sensor(0x002A,0x01AC);
    S5K8AAYX_write_cmos_sensor(0x0F12,0x0001); // #REG_TC_GP_PrevOpenAfterChange
    S5K8AAYX_write_cmos_sensor(0x002A,0x01A6);
    S5K8AAYX_write_cmos_sensor(0x0F12,0x0001); // #REG_TC_GP_NewConfigSync // Update preview configuration
    S5K8AAYX_write_cmos_sensor(0x002A,0x01AA);
    S5K8AAYX_write_cmos_sensor(0x0F12,0x0001); // #REG_TC_GP_PrevConfigChanged
    S5K8AAYX_write_cmos_sensor(0x002A,0x019E);
    S5K8AAYX_write_cmos_sensor(0x0F12,0x0001); // #REG_TC_GP_EnablePreview // Start preview
    S5K8AAYX_write_cmos_sensor(0x0F12,0x0001); // #REG_TC_GP_EnablePreviewChanged
    return;
}



/*****************************************************************************
 * FUNCTION
 *  S5K8AAYX_PV_Mode
 * DESCRIPTION
 *
 * PARAMETERS
 *  void
 * RETURNS
 *  void
 *****************************************************************************/
void S5K8AAYX_PV_Mode(unsigned int config_num)
{
    SENSORDB("Enter S5K8AAYX_PV_Mode\n");

    S5K8AAYX_write_cmos_sensor(0x0028, 0x7000);
    S5K8AAYX_write_cmos_sensor(0x002A ,0x01A8);
    S5K8AAYX_write_cmos_sensor(0x0F12 ,config_num);  // #REG_TC_GP_ActivePrevConfig
    S5K8AAYX_write_cmos_sensor(0x002A ,0x01AC);
    S5K8AAYX_write_cmos_sensor(0x0F12 ,0x0001);  // #REG_TC_GP_PrevOpenAfterChange
    S5K8AAYX_write_cmos_sensor(0x002A ,0x01A6);
    S5K8AAYX_write_cmos_sensor(0x0F12 ,0x0001);  // #REG_TC_GP_NewConfigSync
    S5K8AAYX_write_cmos_sensor(0x002A ,0x01AA);
    S5K8AAYX_write_cmos_sensor(0x0F12 ,0x0001);  // #REG_TC_GP_PrevConfigChanged
    S5K8AAYX_write_cmos_sensor(0x002A ,0x019E);
    S5K8AAYX_write_cmos_sensor(0x0F12 ,0x0001);  // #REG_TC_GP_EnablePreview
    S5K8AAYX_write_cmos_sensor(0x0F12 ,0x0001);  // #REG_TC_GP_EnablePreviewChanged

    S5K8AAYX_write_cmos_sensor(0x002A, 0x0164);
    S5K8AAYX_write_cmos_sensor(0x0F12, 0x0001); // REG_TC_IPRM_InitParamsUpdated
    return;
}



/*****************************************************************************
 * FUNCTION
 *  S5K8AAYX_CAP_Mode
 * DESCRIPTION
 *
 * PARAMETERS
 *  void
 * RETURNS
 *  void
 *****************************************************************************/
void S5K8AAYX_CAP_Mode2(void)
{
//
}


void
S5K8AAYX_GetAutoISOValue(void)
{
    // Cal. Method : ((A-Gain*D-Gain)/100h)/2
    // A-Gain , D-Gain Read value is hex value.
    //   ISO 50  : 100(HEX)
    //   ISO 100 : 100 ~ 1FF(HEX)
    //   ISO 200 : 200 ~ 37F(HEX)
    //   ISO 400 : over 380(HEX)
    unsigned int A_Gain, D_Gain, ISOValue;
    S5K8AAYX_write_cmos_sensor(0xFCFC, 0xD000);
    S5K8AAYX_write_cmos_sensor(0x002C, 0x7000);
    S5K8AAYX_write_cmos_sensor(0x002E, 0x20E0);
    A_Gain = S5K8AAYX_read_cmos_sensor(0x0F12);
    D_Gain = S5K8AAYX_read_cmos_sensor(0x0F12);

    ISOValue = (A_Gain >> 8);
    spin_lock(&s5k8aayx_drv_lock);
#if 0
    if (ISOValue == 256)
    {
        S5K8AAYXCurrentStatus.isoSpeed = AE_ISO_50;
    }
    else if ((ISOValue >= 257) && (ISOValue <= 511 ))
    {
        S5K8AAYXCurrentStatus.isoSpeed = AE_ISO_100;
    }
#endif
    if ((ISOValue >= 2) && (ISOValue < 4 ))
    {
        S5K8AAYXCurrentStatus.isoSpeed = AE_ISO_200;
    }
    else if (ISOValue >= 4)
    {
        S5K8AAYXCurrentStatus.isoSpeed = AE_ISO_400;
    }
    else
    {
        S5K8AAYXCurrentStatus.isoSpeed = AE_ISO_100;
    }
    spin_unlock(&s5k8aayx_drv_lock);

    SENSORDB("[8AA] Auto ISO Value = %d \n", ISOValue);
}



void S5K8AAYX_CAP_Mode(void)
{
    S5K8AAYX_write_cmos_sensor(0x0028, 0x7000);
    S5K8AAYX_write_cmos_sensor(0x002A, 0x01B0);
    S5K8AAYX_write_cmos_sensor(0x0F12, 0x0000);  //config select   0 :48, 1, 20,
    S5K8AAYX_write_cmos_sensor(0x002a, 0x01A6);
    S5K8AAYX_write_cmos_sensor(0x0F12, 0x0001);
    S5K8AAYX_write_cmos_sensor(0x002a, 0x01B2);
    S5K8AAYX_write_cmos_sensor(0x0F12, 0x0001);
    S5K8AAYX_write_cmos_sensor(0x002a, 0x01A2);
    S5K8AAYX_write_cmos_sensor(0x0F12, 0x0001);
    S5K8AAYX_write_cmos_sensor(0x002a, 0x01A4);
    S5K8AAYX_write_cmos_sensor(0x0F12, 0x0001);
    mdelay(50);
    //S5K5CAGX_gPVmode = KAL_FALSE;

    S5K8AAYX_GetAutoISOValue();
}


/*void S5K8AAYX_AE_AWB_Enable(kal_bool enable)
{
    S5K8AAYX_write_cmos_sensor(0x0028, 0x7000);
    S5K8AAYX_write_cmos_sensor(0x002A, 0x04D2);           // REG_TC_DBG_AutoAlgEnBits

    if (enable)
    {
        // Enable AE/AWB
        S5K8AAYX_write_cmos_sensor(0x0F12, 0x077F); // Enable aa_all, ae, awb.
    }
    else
    {
        // Disable AE/AWB
        S5K8AAYX_write_cmos_sensor(0x0F12, 0x0770); // Disable aa_all, ae, awb.
    }

}*/
static void S5K8AAYX_set_AE_mode(kal_bool AE_enable)
{
    if(AE_enable==KAL_TRUE)
    {
        S5K8AAYX_write_cmos_sensor(0xFCFC, 0xD000); // Set page
        S5K8AAYX_write_cmos_sensor(0x0028, 0x7000); // Set address

        S5K8AAYX_write_cmos_sensor(0x002A, 0x214A);
        S5K8AAYX_write_cmos_sensor(0x0F12, 0x0001);
    }
    else
    {
        S5K8AAYX_write_cmos_sensor(0xFCFC, 0xD000); // Set page
        S5K8AAYX_write_cmos_sensor(0x0028, 0x7000); // Set address

        S5K8AAYX_write_cmos_sensor(0x002A, 0x214A);
        S5K8AAYX_write_cmos_sensor(0x0F12, 0x0000);
    }

}


/*************************************************************************
* FUNCTION
*       S5K8AAYX_night_mode
*
* DESCRIPTION
*       This function night mode of S5K8AAYX.
*
* PARAMETERS
*       none
*
* RETURNS
*       None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void S5K8AAYX_night_mode(kal_bool enable)
{
    /*----------------------------------------------------------------*/
    /* Local Variables                                                                                                             */
    /*----------------------------------------------------------------*/

    /*----------------------------------------------------------------*/
    /* Code Body                                                                                                                     */
    /*----------------------------------------------------------------*/
    //kal_uint16 video_frame_len = 0;
    //kal_uint16 prev_line_len = 0;

    SENSORDB("S5K8AAYX_night_mode [in] enable=%d \r\n",enable);

    if(S5K8AAYXCurrentStatus.iNightMode == enable)
        return;

    if (S5K8AAYX_sensor_cap_state == KAL_TRUE)
              return ;    //Don't need rewrite the setting when capture.
    S5K8AAYX_write_cmos_sensor(0x0028, 0x7000); // Set address

    if (enable)
    {
        S5K8AAYX_write_cmos_sensor(0x002A,0x0468);
        S5K8AAYX_write_cmos_sensor(0x0F12,0x0140);  //lt_uMaxDigGain

        S5K8AAYX_night_mode_enable = KAL_TRUE;
    }
    else
    {
        S5K8AAYX_write_cmos_sensor(0x002A,0x0468);
        S5K8AAYX_write_cmos_sensor(0x0F12,0x0100);  //lt_uMaxDigGain

        S5K8AAYX_night_mode_enable = KAL_FALSE;
    }
    
#if 0
    //move to  S5K8AAYX_SetFrameRate(~)
    // active preview configure
    //============================================================
    S5K8AAYX_write_cmos_sensor(0x002A,0x01A8);
    S5K8AAYX_write_cmos_sensor(0x0F12,0x0000);  // #REG_TC_GP_ActivePrevConfig // Select preview configuration_0
    S5K8AAYX_write_cmos_sensor(0x002A,0x01AC);
    S5K8AAYX_write_cmos_sensor(0x0F12,0x0001);  // #REG_TC_GP_PrevOpenAfterChange
    S5K8AAYX_write_cmos_sensor(0x002A,0x01A6);
    S5K8AAYX_write_cmos_sensor(0x0F12,0x0001);  // #REG_TC_GP_NewConfigSync // Update preview configuration
    S5K8AAYX_write_cmos_sensor(0x002A,0x01AA);
    S5K8AAYX_write_cmos_sensor(0x0F12,0x0001);  // #REG_TC_GP_PrevConfigChanged
    S5K8AAYX_write_cmos_sensor(0x002A,0x019E);
    S5K8AAYX_write_cmos_sensor(0x0F12,0x0001);  // #REG_TC_GP_EnablePreview // Start preview
    S5K8AAYX_write_cmos_sensor(0x0F12,0x0001);  // #REG_TC_GP_EnablePreviewChanged
#endif

    S5K8AAYXCurrentStatus.iNightMode = enable;

} /* S5K8AAYX_night_mode */

/*************************************************************************
* FUNCTION
*       S5K8AAYX_GetSensorID
*
* DESCRIPTION
*       This function get the sensor ID
*
* PARAMETERS
*       None
*
* RETURNS
*       None
*
* GLOBALS AFFECTED
*
*************************************************************************/
  kal_uint32 S5K8AAYX_GetSensorID(kal_uint32 *sensorID)
{
    kal_uint16 sensor_id = 0;
    //unsigned short version = 0;
    signed int retry = 3;

    //check if sensor ID correct
    while(--retry)
    {
        S5K8AAYX_write_cmos_sensor(0xFCFC, 0x0000);
        sensor_id = S5K8AAYX_read_cmos_sensor(0x0040);
        SENSORDB("[8AA]Sensor Read S5K8AAYX ID: 0x%x \r\n", sensor_id);

        *sensorID = sensor_id;
        if (sensor_id == S5K8AAYX_SENSOR_ID)
        {
            SENSORDB("[8AA] Sensor ID: 0x%x, Read OK \r\n", sensor_id);
            //version=S5K8AAYX_read_cmos_sensor(0x0042);
            //SENSORDB("[8AA]~~~~~~~~~~~~~~~~ S5K8AAYX version: 0x%x \r\n",version);
            return ERROR_NONE;
        }
    }
    *sensorID = 0xFFFFFFFF;
    SENSORDB("[8AA] Sensor Read Fail \r\n");
    return ERROR_SENSOR_CONNECT_FAIL;
}


/*****************************************************************************/
/* Windows Mobile Sensor Interface */
/*****************************************************************************/
/*************************************************************************
* FUNCTION
*       S5K8AAYXOpen
*
* DESCRIPTION
*       This function initialize the registers of CMOS sensor
*
* PARAMETERS
*       None
*
* RETURNS
*       None
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 S5K8AAYXOpen(void)
{
    kal_uint16 sensor_id=0;


	 S5K8AAYX_write_cmos_sensor(0xFCFC, 0x0000);
	 sensor_id=S5K8AAYX_read_cmos_sensor(0x0040);
	 SENSORDB("Sensor Read S5K8AAYX ID %x \r\n",sensor_id);

	if (sensor_id != S5K8AAYX_SENSOR_ID)
	{
	    SENSORDB("Sensor Read ByeBye \r\n");
			return ERROR_SENSOR_CONNECT_FAIL;
	}

    S5K8AAYXInitialPara();
    S5K8AAYX_Initialize_Setting();

    S5K8AAYX_set_mirror(IMAGE_HV_MIRROR);
    return ERROR_NONE;
}

/*************************************************************************
* FUNCTION
*       S5K8AAYXClose
*
* DESCRIPTION
*       This function is to turn off sensor module power.
*
* PARAMETERS
*       None
*
* RETURNS
*       None
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 S5K8AAYXClose(void)
{
    return ERROR_NONE;
} /* S5K8AAYXClose() */

/*************************************************************************
* FUNCTION
*       S5K8AAYXPreview
*
* DESCRIPTION
*       This function start the sensor preview.
*
* PARAMETERS
*       *image_window : address pointer of pixel numbers in one period of HSYNC
*  *sensor_config_data : address pointer of line numbers in one period of VSYNC
*
* RETURNS
*       None
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 S5K8AAYXPreview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                       MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    SENSORDB("Enter S5K8AAYXPreview__\n");
    spin_lock(&s5k8aayx_drv_lock);
    S5K8AAYX_sensor_cap_state = KAL_FALSE;
    //S5K8AAYX_PV_dummy_pixels = 0x0400;
    S5K8AAYX_PV_dummy_lines = 0;

    if(sensor_config_data->SensorOperationMode==MSDK_SENSOR_OPERATION_MODE_VIDEO) // MPEG4 Encode Mode
    {
        SENSORDB("S5K8AAYXPreview MSDK_SENSOR_OPERATION_MODE_VIDEO \n");
        S5K8AAYX_MPEG4_encode_mode = KAL_TRUE;
        S5K8AAYX_MJPEG_encode_mode = KAL_FALSE;
    }
    else
    {
        SENSORDB("S5K8AAYXPreview Normal \n");
        S5K8AAYX_MPEG4_encode_mode = KAL_FALSE;
        S5K8AAYX_MJPEG_encode_mode = KAL_FALSE;
    }
    spin_unlock(&s5k8aayx_drv_lock);


    if (MSDK_SCENARIO_ID_CAMERA_PREVIEW == S5K8AAYXCurrentScenarioId)
    {
        S5K8AAYX_PV_Mode(S5K8AAYX_PREVIEW_MODE);
    }
    else
    {
        S5K8AAYX_PV_Mode(S5K8AAYX_VIDEO_MODE);
    }


    //S5K8AAYX_set_mirror(sensor_config_data->SensorImageMirror);


    image_window->GrabStartX = S5K8AAYX_IMAGE_SENSOR_PV_INSERTED_PIXELS;
    image_window->GrabStartY = S5K8AAYX_IMAGE_SENSOR_PV_INSERTED_LINES;
    image_window->ExposureWindowWidth = S5K8AAYX_IMAGE_SENSOR_PV_WIDTH;
    image_window->ExposureWindowHeight = S5K8AAYX_IMAGE_SENSOR_PV_HEIGHT;

    // copy sensor_config_data
    memcpy(&S5K8AAYXSensorConfigData, sensor_config_data, sizeof(MSDK_SENSOR_CONFIG_STRUCT));
	
	mdelay(60);
    return ERROR_NONE;
}   /* S5K8AAYXPreview() */

UINT32 S5K8AAYXCapture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                       MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
   // kal_uint32 pv_integration_time = 0;  // Uinit - us
   // kal_uint32 cap_integration_time = 0;
   // kal_uint16 PV_line_len = 0;
   // kal_uint16 CAP_line_len = 0;
    SENSORDB("Enter S5K8AAYXCapture__\n");

    S5K8AAYX_sensor_cap_state = KAL_TRUE;
    S5K8AAYX_CAP_Mode();

    image_window->GrabStartX = S5K8AAYX_IMAGE_SENSOR_FULL_INSERTED_PIXELS;
    image_window->GrabStartY = S5K8AAYX_IMAGE_SENSOR_FULL_INSERTED_LINES;
    image_window->ExposureWindowWidth = S5K8AAYX_IMAGE_SENSOR_FULL_WIDTH;
    image_window->ExposureWindowHeight = S5K8AAYX_IMAGE_SENSOR_FULL_HEIGHT;

    // copy sensor_config_data
    memcpy(&S5K8AAYXSensorConfigData, sensor_config_data, sizeof(MSDK_SENSOR_CONFIG_STRUCT));
	
	mdelay(60);
    return ERROR_NONE;
}   /* S5K8AAYXCapture() */

UINT32 S5K8AAYXGetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution)
{
    SENSORDB("Enter S5K8AAYXGetResolution\n");
    pSensorResolution->SensorFullWidth=S5K8AAYX_IMAGE_SENSOR_FULL_WIDTH;  //modify by yanxu
    pSensorResolution->SensorFullHeight=S5K8AAYX_IMAGE_SENSOR_FULL_HEIGHT;
    pSensorResolution->SensorPreviewWidth=S5K8AAYX_IMAGE_SENSOR_PV_WIDTH;
    pSensorResolution->SensorPreviewHeight=S5K8AAYX_IMAGE_SENSOR_PV_HEIGHT;
    pSensorResolution->SensorVideoWidth=S5K8AAYX_IMAGE_SENSOR_PV_WIDTH;
    pSensorResolution->SensorVideoHeight=S5K8AAYX_IMAGE_SENSOR_PV_HEIGHT;
    return ERROR_NONE;
}   /* S5K8AAYXGetResolution() */

UINT32 S5K8AAYXGetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId,
                       MSDK_SENSOR_INFO_STRUCT *pSensorInfo,
                       MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
    SENSORDB("Enter S5K8AAYXGetInfo\n");
    SENSORDB("S5K8AAYXGetInfo ScenarioId =%d \n",ScenarioId);
    switch(ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_ZSD:
             pSensorInfo->SensorPreviewResolutionX=S5K8AAYX_IMAGE_SENSOR_FULL_WIDTH;
             pSensorInfo->SensorPreviewResolutionY=S5K8AAYX_IMAGE_SENSOR_FULL_HEIGHT;
             pSensorInfo->SensorCameraPreviewFrameRate=15;
             break;
        default:
             pSensorInfo->SensorPreviewResolutionX=S5K8AAYX_IMAGE_SENSOR_PV_WIDTH;
             pSensorInfo->SensorPreviewResolutionY=S5K8AAYX_IMAGE_SENSOR_PV_HEIGHT;
             pSensorInfo->SensorCameraPreviewFrameRate=30;
             break;
    }
    pSensorInfo->SensorFullResolutionX=S5K8AAYX_IMAGE_SENSOR_FULL_WIDTH;
    pSensorInfo->SensorFullResolutionY=S5K8AAYX_IMAGE_SENSOR_FULL_HEIGHT;
    pSensorInfo->SensorCameraPreviewFrameRate=30;
    pSensorInfo->SensorVideoFrameRate=30;
    pSensorInfo->SensorStillCaptureFrameRate=10;
    pSensorInfo->SensorWebCamCaptureFrameRate=15;
    pSensorInfo->SensorResetActiveHigh=FALSE;
    pSensorInfo->SensorResetDelayCount=1;

    pSensorInfo->SensorOutputDataFormat = SENSOR_OUTPUT_FORMAT_YUYV;

    pSensorInfo->SensorClockPolarity = SENSOR_CLOCK_POLARITY_HIGH;
    pSensorInfo->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_HIGH;

    pSensorInfo->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_HIGH;
    pSensorInfo->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorInterruptDelayLines = 1;

    #ifdef MIPI_INTERFACE
        pSensorInfo->SensroInterfaceType = SENSOR_INTERFACE_TYPE_MIPI;
    #else
        pSensorInfo->SensroInterfaceType = SENSOR_INTERFACE_TYPE_PARALLEL;
    #endif

    pSensorInfo->SensorMasterClockSwitch = 0;
    pSensorInfo->SensorDrivingCurrent = ISP_DRIVING_2MA;//ISP_DRIVING_4MA;
    pSensorInfo->CaptureDelayFrame = 3;
    pSensorInfo->PreviewDelayFrame = 3;
    pSensorInfo->VideoDelayFrame = 4;

    pSensorInfo->YUVAwbDelayFrame = 3;
    pSensorInfo->YUVEffectDelayFrame = 2;

    switch (ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
             pSensorInfo->SensorClockFreq = S5K8AAYX_MCLK;
             pSensorInfo->SensorClockDividCount = 3;
             pSensorInfo->SensorClockRisingCount = 0;
             pSensorInfo->SensorClockFallingCount = 2;
             pSensorInfo->SensorPixelClockCount = 3;
             pSensorInfo->SensorDataLatchCount = 2;
             pSensorInfo->SensorGrabStartX = 2;
             pSensorInfo->SensorGrabStartY = 2;
             break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        case MSDK_SCENARIO_ID_CAMERA_ZSD:
             pSensorInfo->SensorClockFreq=S5K8AAYX_MCLK;
             pSensorInfo->SensorClockDividCount=   3;
             pSensorInfo->SensorClockRisingCount= 0;
             pSensorInfo->SensorClockFallingCount= 2;
             pSensorInfo->SensorPixelClockCount= 3;
             pSensorInfo->SensorDataLatchCount= 2;
             pSensorInfo->SensorGrabStartX = 2;
             pSensorInfo->SensorGrabStartY = 2;
             break;
        default:
             pSensorInfo->SensorClockFreq=S5K8AAYX_MCLK;
             pSensorInfo->SensorClockDividCount=3;
             pSensorInfo->SensorClockRisingCount=0;
             pSensorInfo->SensorClockFallingCount=2;
             pSensorInfo->SensorPixelClockCount=3;
             pSensorInfo->SensorDataLatchCount=2;
             pSensorInfo->SensorGrabStartX = 2;
             pSensorInfo->SensorGrabStartY = 2;
             break;
    }
    memcpy(pSensorConfigData, &S5K8AAYXSensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));
    return ERROR_NONE;
}/* S5K8AAYXGetInfo() */


UINT32 S5K8AAYXControl(MSDK_SCENARIO_ID_ENUM ScenarioId,
                       MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow,
                       MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
    SENSORDB("Enter S5K8AAYXControl\n");
    SENSORDB("S5K8AAYXControl ScenarioId = %d ,\n",ScenarioId);

    spin_lock(&s5k8aayx_drv_lock);
    S5K8AAYXCurrentScenarioId = ScenarioId;
    spin_unlock(&s5k8aayx_drv_lock);

    switch (ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
             SENSORDB("YSZ_S5K8AAYX_S5K8AAYXControl_preview");
             S5K8AAYXPreview(pImageWindow, pSensorConfigData);
             break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
             S5K8AAYXCapture(pImageWindow, pSensorConfigData);
             break;
        case MSDK_SCENARIO_ID_CAMERA_ZSD:
             S5K8AAYXCapture(pImageWindow, pSensorConfigData);
             break;
        default:
           return ERROR_INVALID_SCENARIO_ID;
    }
    return ERROR_NONE;
}   /* S5K8AAYXControl() */



/* [TC] YUV sensor */



BOOL S5K8AAYX_set_param_wb(UINT16 para)
{
    kal_uint16 AWB_uGainsOut0,AWB_uGainsOut1,AWB_uGainsOut2;
    SENSORDB("Enter S5K8AAYX_set_param_wb\n");
    if(S5K8AAYXCurrentStatus.iWB == para)
        return TRUE;
    SENSORDB("[Enter]S5K8AAYX set_param_wb func:para = %d\n",para);

    switch (para)
    {
        case AWB_MODE_AUTO:
             //Read Back
             S5K8AAYX_write_cmos_sensor(0xFCFC, 0xD000);
             S5K8AAYX_write_cmos_sensor(0x002C, 0x7000);
             S5K8AAYX_write_cmos_sensor(0x002E, 0x20EC);
             AWB_uGainsOut0=S5K8AAYX_read_cmos_sensor(0x0F12); //20EC
             AWB_uGainsOut1=S5K8AAYX_read_cmos_sensor(0x0F12); //20EE
             AWB_uGainsOut2=S5K8AAYX_read_cmos_sensor(0x0F12); //20F0

             S5K8AAYX_write_cmos_sensor(0xFCFC, 0xD000);
             S5K8AAYX_write_cmos_sensor(0x0028, 0x7000);
             S5K8AAYX_write_cmos_sensor(0x002A, 0x0D2A);
             S5K8AAYX_write_cmos_sensor(0x0F12, AWB_uGainsOut0); //0x0D2A
             S5K8AAYX_write_cmos_sensor(0x0F12, AWB_uGainsOut1); //0x0D2C
             S5K8AAYX_write_cmos_sensor(0x0F12, AWB_uGainsOut2); //0x0D2E

             S5K8AAYX_write_cmos_sensor(0x002A, 0x2162);
             S5K8AAYX_write_cmos_sensor(0x0F12, 0x0001);

             //S5K8AAYX_write_cmos_sensor(0x002A,0x0408);
             //S5K8AAYX_write_cmos_sensor(0x0F12,0x067F);//bit[3]:AWB Auto:1 menual:0
             break;
        case AWB_MODE_CLOUDY_DAYLIGHT:
             //======================================================================
             //      MWB : Cloudy_D65
             //======================================================================
             S5K8AAYX_write_cmos_sensor(0xFCFC, 0xD000);
             S5K8AAYX_write_cmos_sensor(0x0028, 0x7000);
             S5K8AAYX_write_cmos_sensor(0x002A, 0x2162);
             S5K8AAYX_write_cmos_sensor(0x0F12, 0x0000);
             S5K8AAYX_write_cmos_sensor(0x002A, 0x03DA);
             S5K8AAYX_write_cmos_sensor(0x0F12, 0x0850);
             S5K8AAYX_write_cmos_sensor(0x0F12, 0x0001);
             S5K8AAYX_write_cmos_sensor(0x0F12, 0x0400);
             S5K8AAYX_write_cmos_sensor(0x0F12, 0x0001);
             S5K8AAYX_write_cmos_sensor(0x0F12, 0x04E0);
             S5K8AAYX_write_cmos_sensor(0x0F12, 0x0001);
             break;
        case AWB_MODE_DAYLIGHT:
             //==============================================
             //      MWB : sun&daylight_D50
             //==============================================
             S5K8AAYX_write_cmos_sensor(0xFCFC, 0xD000);
             S5K8AAYX_write_cmos_sensor(0x0028, 0x7000);
             S5K8AAYX_write_cmos_sensor(0x002A, 0x2162);
             S5K8AAYX_write_cmos_sensor(0x0F12, 0x0000);
             S5K8AAYX_write_cmos_sensor(0x002A, 0x03DA);
             S5K8AAYX_write_cmos_sensor(0x0F12, 0x0620);
             S5K8AAYX_write_cmos_sensor(0x0F12, 0x0001);
             S5K8AAYX_write_cmos_sensor(0x0F12, 0x0400);
             S5K8AAYX_write_cmos_sensor(0x0F12, 0x0001);
             S5K8AAYX_write_cmos_sensor(0x0F12, 0x0530);
             S5K8AAYX_write_cmos_sensor(0x0F12, 0x0001);
             break;
        case AWB_MODE_FLUORESCENT:
              //==================================================================
              //      MWB : Florescent_TL84
              //==================================================================
              S5K8AAYX_write_cmos_sensor(0xFCFC, 0xD000);
              S5K8AAYX_write_cmos_sensor(0x0028, 0x7000);
              S5K8AAYX_write_cmos_sensor(0x002A, 0x2162);
              S5K8AAYX_write_cmos_sensor(0x0F12, 0x0000);
              S5K8AAYX_write_cmos_sensor(0x002A, 0x03DA);
              S5K8AAYX_write_cmos_sensor(0x0F12, 0x0560);
              S5K8AAYX_write_cmos_sensor(0x0F12, 0x0001);
              S5K8AAYX_write_cmos_sensor(0x0F12, 0x0400);
              S5K8AAYX_write_cmos_sensor(0x0F12, 0x0001);
              S5K8AAYX_write_cmos_sensor(0x0F12, 0x0880);
              S5K8AAYX_write_cmos_sensor(0x0F12, 0x0001);
              break;
        case AWB_MODE_INCANDESCENT:
        case AWB_MODE_TUNGSTEN:
              S5K8AAYX_write_cmos_sensor(0xFCFC, 0xD000);
              S5K8AAYX_write_cmos_sensor(0x0028, 0x7000);
              S5K8AAYX_write_cmos_sensor(0x002A, 0x2162);
              S5K8AAYX_write_cmos_sensor(0x0F12, 0x0000);
              S5K8AAYX_write_cmos_sensor(0x002A, 0x03DA);
              S5K8AAYX_write_cmos_sensor(0x0F12, 0x03C0);
              S5K8AAYX_write_cmos_sensor(0x0F12, 0x0001);
              S5K8AAYX_write_cmos_sensor(0x0F12, 0x0400);
              S5K8AAYX_write_cmos_sensor(0x0F12, 0x0001);
              S5K8AAYX_write_cmos_sensor(0x0F12, 0x0980);
              S5K8AAYX_write_cmos_sensor(0x0F12, 0x0001);
              break;
        default:
              return KAL_FALSE;
    }
    spin_lock(&s5k8aayx_drv_lock);
    S5K8AAYXCurrentStatus.iWB = para;
    spin_unlock(&s5k8aayx_drv_lock);
    return TRUE;
}




void S5K8AAYX_GetAEAWBLock(UINT32 *pAElockRet32,UINT32 *pAWBlockRet32)
{
    *pAElockRet32 = 0;
    *pAWBlockRet32 = 0;
    SENSORDB("S5K8AAYX_GetAEAWBLock,AE=%d ,AWB=%d\n,",*pAElockRet32,*pAWBlockRet32);
}


BOOL S5K8AAYX_set_param_effect(UINT16 para)
{
    /*----------------------------------------------------------------*/
    /* Local Variables                                                                                                        */
    /*----------------------------------------------------------------*/
    kal_bool ret=KAL_TRUE;

    /*----------------------------------------------------------------*/
    /* Code Body                                                                                                                          */
    /*----------------------------------------------------------------*/

    SENSORDB("Enter S5K8AAYX_set_param_effect\n");

    if(S5K8AAYXCurrentStatus.iEffect == para)
        return TRUE;
    SENSORDB("[Enter]s5k8aayx set_param_effect func:para = %d\n",para);

    S5K8AAYX_write_cmos_sensor(0xFCFC,0xD000);
    S5K8AAYX_write_cmos_sensor(0x0028,0x7000);
    S5K8AAYX_write_cmos_sensor(0x002A,0x019C);
    switch (para)
    {
        case MEFFECT_OFF:
             S5K8AAYX_write_cmos_sensor(0x0F12,0x0000); // Normal,
             break;
        case MEFFECT_MONO:
             S5K8AAYX_write_cmos_sensor(0x0F12,0x0001); // Monochrome (Black & White)
             break;
        case MEFFECT_SEPIA:
             S5K8AAYX_write_cmos_sensor(0x0F12,0x0004); // Sepia
             break;
        case MEFFECT_NEGATIVE:
             S5K8AAYX_write_cmos_sensor(0x0F12,0x0003); // Negative Mono
             break;
        default:
             ret = KAL_FALSE;
    }
    spin_lock(&s5k8aayx_drv_lock);
    S5K8AAYXCurrentStatus.iEffect = para;
    spin_unlock(&s5k8aayx_drv_lock);
    return ret;
}


BOOL S5K8AAYX_set_param_banding(UINT16 para)
{
    /*----------------------------------------------------------------*/
    /* Local Variables                                                */
    /*----------------------------------------------------------------*/
    /*----------------------------------------------------------------*/
    /* Code Body                                                      */
    /*----------------------------------------------------------------*/
    SENSORDB("Enter S5K8AAYX_set_param_banding\n");
//#if (defined(S5K8AAYX_MANUAL_ANTI_FLICKER))

    if(S5K8AAYXCurrentStatus.iBanding == para)
        return TRUE;

    switch (para)
    {
        case AE_FLICKER_MODE_50HZ:
             S5K8AAYX_write_cmos_sensor(0x0028,0x7000);
             S5K8AAYX_write_cmos_sensor(0x002a,0x0408);
             S5K8AAYX_write_cmos_sensor(0x0f12,0x065F);
             S5K8AAYX_write_cmos_sensor(0x002a,0x03F4);
             S5K8AAYX_write_cmos_sensor(0x0f12,0x0001); //REG_SF_USER_FlickerQuant 1:50hz  2:60hz
             S5K8AAYX_write_cmos_sensor(0x0f12,0x0001); //REG_SF_USER_FlickerQuantChanged active flicker
             break;
        case AE_FLICKER_MODE_60HZ:
             S5K8AAYX_write_cmos_sensor(0x0028,0x7000);
             S5K8AAYX_write_cmos_sensor(0x002a,0x0408);
             S5K8AAYX_write_cmos_sensor(0x0f12,0x065F);
             S5K8AAYX_write_cmos_sensor(0x002a,0x03F4);
             S5K8AAYX_write_cmos_sensor(0x0f12,0x0002); //REG_SF_USER_FlickerQuant 1:50hz  2:60hz
             S5K8AAYX_write_cmos_sensor(0x0f12,0x0001); //REG_SF_USER_FlickerQuantChanged active flicker
             break;
        default:
             return KAL_FALSE;
    }
    spin_lock(&s5k8aayx_drv_lock);
    S5K8AAYXCurrentStatus.iBanding = para;
    spin_unlock(&s5k8aayx_drv_lock);
    return TRUE;

//#else
    /* Auto anti-flicker method is enabled, then nothing need to do in this function.  */
//#endif /* #if (defined(S5K8AAYX_MANUAL_ANTI_FLICKER)) */
    return KAL_TRUE;
} /* S5K8AAYX_set_param_banding */



BOOL S5K8AAYX_set_param_exposure(UINT16 para)
{
    SENSORDB("Enter S5K8AAYX_set_param_exposure\n");
    if(S5K8AAYXCurrentStatus.iEV == para)
        return TRUE;

    SENSORDB("[Enter]s5k8aayx set_param_exposure func:para = %d\n",para);
    S5K8AAYX_write_cmos_sensor(0x0028, 0x7000);
    S5K8AAYX_write_cmos_sensor(0x002a, 0x019A);

    switch (para)
    {
        case AE_EV_COMP_n10:
             S5K8AAYX_write_cmos_sensor(0x0F12, 0x0080); //EV -1
             break;
        case AE_EV_COMP_00:
             S5K8AAYX_write_cmos_sensor(0x0F12, 0x0100); //EV 0
             break;
        case AE_EV_COMP_10:
             S5K8AAYX_write_cmos_sensor(0x0F12, 0x0200);  //EV +1
             break;
        case AE_EV_COMP_n13:
        case AE_EV_COMP_n07:
        case AE_EV_COMP_n03:
        case AE_EV_COMP_03:
        case AE_EV_COMP_07:
        case AE_EV_COMP_13:
             break;
        default:
             return FALSE;
    }
    spin_lock(&s5k8aayx_drv_lock);
    S5K8AAYXCurrentStatus.iEV = para;
    spin_unlock(&s5k8aayx_drv_lock);
    return TRUE;

}/* S5K8AAYX_set_param_exposure */


UINT32 S5K8AAYXYUVSensorSetting(FEATURE_ID iCmd, UINT16 iPara)
{

	MSDK_SCENARIO_ID_ENUM scen = MSDK_SCENARIO_ID_CAMERA_PREVIEW;

    spin_lock(&s5k8aayx_drv_lock);
    scen = S5K8AAYXCurrentScenarioId;
    spin_unlock(&s5k8aayx_drv_lock);

    SENSORDB("Enter S5K8AAYXYUVSensorSetting\n");
    switch (iCmd)
    {
        case FID_SCENE_MODE:
            if (iPara == SCENE_MODE_OFF)
            {
                S5K8AAYX_SetFrameRate(scen, 30);
                S5K8AAYX_night_mode(0);
            }
            else if (iPara == SCENE_MODE_NIGHTSCENE)
            {
                S5K8AAYX_SetFrameRate(scen, 15);
                S5K8AAYX_night_mode(1);
            }
            break;
        case FID_AWB_MODE:
            S5K8AAYX_set_param_wb(iPara);
            break;
        case FID_COLOR_EFFECT:
            S5K8AAYX_set_param_effect(iPara);
            break;
        case FID_AE_EV:
            S5K8AAYX_set_param_exposure(iPara);
            break;
        case FID_AE_FLICKER:
            S5K8AAYX_set_param_banding(iPara);
            break;
        case FID_AE_SCENE_MODE:
            spin_lock(&s5k8aayx_drv_lock);
            if (iPara == AE_MODE_OFF) {
                S5K8AAYX_AE_ENABLE = KAL_FALSE;
            }
            else {
                S5K8AAYX_AE_ENABLE = KAL_TRUE;
            }
            spin_unlock(&s5k8aayx_drv_lock);
            S5K8AAYX_set_AE_mode(S5K8AAYX_AE_ENABLE);
            break;
        case FID_ZOOM_FACTOR:
            zoom_factor = iPara;
            break;
        default:
            break;
    }
    return ERROR_NONE;
}   /* S5K8AAYXYUVSensorSetting */


UINT32 S5K8AAYXYUVSetVideoMode(UINT16 u2FrameRate)
{
    SENSORDB("Enter S5K8AAYXYUVSetVideoMode,u2FrameRate=%d\n",u2FrameRate);

    spin_lock(&s5k8aayx_drv_lock);
    S5K8AAYXCurrentStatus.iFrameRate = u2FrameRate;
    spin_unlock(&s5k8aayx_drv_lock);

    S5K8AAYX_SetFrameRate(MSDK_SCENARIO_ID_VIDEO_PREVIEW, u2FrameRate);
    mdelay(100);

    return TRUE;
}


kal_uint16 S5K8AAYXReadShutter(void)
{
   kal_uint16 temp;
   S5K8AAYX_write_cmos_sensor(0x002c,0x7000);
   S5K8AAYX_write_cmos_sensor(0x002e,0x16E2);

   temp=S5K8AAYX_read_cmos_sensor(0x0f12);

   return temp;
}


kal_uint16 S5K8AAYXReadGain(void)
{
   kal_uint16 temp;
   S5K8AAYX_write_cmos_sensor(0x002c,0x7000);
   S5K8AAYX_write_cmos_sensor(0x002e,0x20D0);
   temp=S5K8AAYX_read_cmos_sensor(0x0f12);
   return temp;
}


kal_uint16 S5K8AAYXReadAwbRGain(void)
{
   kal_uint16 temp;
   S5K8AAYX_write_cmos_sensor(0x002c,0x7000);
   S5K8AAYX_write_cmos_sensor(0x002e,0x20D2);
   temp=S5K8AAYX_read_cmos_sensor(0x0f12);
   return temp;
}


kal_uint16 S5K8AAYXReadAwbBGain(void)
{
   kal_uint16 temp;
   S5K8AAYX_write_cmos_sensor(0x002c,0x7000);
   S5K8AAYX_write_cmos_sensor(0x002e,0x20D2);
   temp=S5K8AAYX_read_cmos_sensor(0x0f12);
   return temp;
}


//#if defined(MT6575)
/*************************************************************************
* FUNCTION
*    S5K8AAYXGetEvAwbRef
*
* DESCRIPTION
* RETURNS
*    None
*
* LOCAL AFFECTED
*
*************************************************************************/
static void S5K8AAYXGetEvAwbRef(UINT32 pSensorAEAWBRefStruct/*PSENSOR_AE_AWB_REF_STRUCT Ref*/)
{
    PSENSOR_AE_AWB_REF_STRUCT Ref = (PSENSOR_AE_AWB_REF_STRUCT)pSensorAEAWBRefStruct;
    //SENSORDB("S5K8AAYXGetEvAwbRef ref = 0x%x \n", Ref);

    Ref->SensorAERef.AeRefLV05Shutter = 1503;
    Ref->SensorAERef.AeRefLV05Gain = 496 * 2; /* 7.75x, 128 base */
    Ref->SensorAERef.AeRefLV13Shutter = 49;
    Ref->SensorAERef.AeRefLV13Gain = 64 * 2; /* 1x, 128 base */
    Ref->SensorAwbGainRef.AwbRefD65Rgain = 188; /* 1.46875x, 128 base */
    Ref->SensorAwbGainRef.AwbRefD65Bgain = 128; /* 1x, 128 base */
    Ref->SensorAwbGainRef.AwbRefCWFRgain = 160; /* 1.25x, 128 base */
    Ref->SensorAwbGainRef.AwbRefCWFBgain = 164; /* 1.28125x, 128 base */
}
/*************************************************************************
* FUNCTION
*    S5K8AAYXGetCurAeAwbInfo
*
* DESCRIPTION
* RETURNS
*    None
*
* LOCAL AFFECTED
*
*************************************************************************/
static void S5K8AAYXGetCurAeAwbInfo(UINT32 pSensorAEAWBCurStruct/*PSENSOR_AE_AWB_CUR_STRUCT Info*/)
{
    PSENSOR_AE_AWB_CUR_STRUCT Info = (PSENSOR_AE_AWB_CUR_STRUCT)pSensorAEAWBCurStruct;
    //SENSORDB("S5K8AAYXGetCurAeAwbInfo Info = 0x%x \n", Info);

    Info->SensorAECur.AeCurShutter = S5K8AAYXReadShutter();
    Info->SensorAECur.AeCurGain = S5K8AAYXReadGain() * 2; /* 128 base */

    Info->SensorAwbGainCur.AwbCurRgain = S5K8AAYXReadAwbRGain()<< 1; /* 128 base */

    Info->SensorAwbGainCur.AwbCurBgain = S5K8AAYXReadAwbBGain()<< 1; /* 128 base */
}
//#endif //6575

void S5K8AAYXGetAFMaxNumFocusAreas(UINT32 *pFeatureReturnPara32)
{
    *pFeatureReturnPara32 = 0;
    SENSORDB("S5K8AAYXGetAFMaxNumFocusAreas, *pFeatureReturnPara32 = %d\n",*pFeatureReturnPara32);
}


void S5K8AAYXGetAFMaxNumMeteringAreas(UINT32 *pFeatureReturnPara32)
{
    *pFeatureReturnPara32 = 0;
    SENSORDB("S5K8AAYXGetAFMaxNumMeteringAreas,*pFeatureReturnPara32 = %d\n",*pFeatureReturnPara32);
}


void S5K8AAYXGetExifInfo(UINT32 exifAddr)
{
    SENSOR_EXIF_INFO_STRUCT* pExifInfo = (SENSOR_EXIF_INFO_STRUCT*)exifAddr;
    pExifInfo->FNumber = 28;
    pExifInfo->AEISOSpeed = S5K8AAYXCurrentStatus.isoSpeed;
    pExifInfo->AWBMode = S5K8AAYXCurrentStatus.iWB;
    pExifInfo->CapExposureTime = 0;
    pExifInfo->FlashLightTimeus = 0;
    pExifInfo->RealISOValue = S5K8AAYXCurrentStatus.isoSpeed;
}


UINT32 S5K8AAYXYUVSetTestPatternMode(kal_bool bEnable)
{
    SENSORDB("[S5K8AAYXYUVSetTestPatternMode] Test pattern enable:%d\n", bEnable);

  if(bEnable)
  {
      //Address: D0003600
      //0x0000 -- bypass
      //0x0002 - solid color
      //0x0004 - gradient
      //0x0006 - address dependent noise
      //0x0008 - random
      //0x000A - gradient + address dependent noise
      //0x000C - gradient + random
      //0x000E - out pixel attributes
      S5K8AAYX_write_cmos_sensor(0x3600,0x0004);
      //0x0002 - solid color
      S5K8AAYX_write_cmos_sensor(0x3602,0x1F40);
      S5K8AAYX_write_cmos_sensor(0x3604,0x1A40);
      S5K8AAYX_write_cmos_sensor(0x3606,0x1A40);
      S5K8AAYX_write_cmos_sensor(0x3608,0x1040);
      //0x0004 - gradient
      S5K8AAYX_write_cmos_sensor(0x360a,0x0383);

  }
  else
  {
    S5K8AAYX_write_cmos_sensor(0x3600,0x0000);
  }
    return ERROR_NONE;
}

UINT32 S5K8AAYXFeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId,
                              UINT8 *pFeaturePara,UINT32 *pFeatureParaLen)
{
    UINT16 *pFeatureReturnPara16=(UINT16 *) pFeaturePara;
    UINT16 *pFeatureData16=(UINT16 *) pFeaturePara;
    UINT32 *pFeatureReturnPara32=(UINT32 *) pFeaturePara;
    UINT32 *pFeatureData32=(UINT32 *) pFeaturePara;
    //PNVRAM_SENSOR_DATA_STRUCT pSensorDefaultData=(PNVRAM_SENSOR_DATA_STRUCT) pFeaturePara;
    MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData=(MSDK_SENSOR_CONFIG_STRUCT *) pFeaturePara;
    MSDK_SENSOR_REG_INFO_STRUCT *pSensorRegData=(MSDK_SENSOR_REG_INFO_STRUCT *) pFeaturePara;
    //MSDK_SENSOR_GROUP_INFO_STRUCT *pSensorGroupInfo=(MSDK_SENSOR_GROUP_INFO_STRUCT *) pFeaturePara;
    //MSDK_SENSOR_ITEM_INFO_STRUCT *pSensorItemInfo=(MSDK_SENSOR_ITEM_INFO_STRUCT *) pFeaturePara;
    //MSDK_SENSOR_ENG_INFO_STRUCT      *pSensorEngInfo=(MSDK_SENSOR_ENG_INFO_STRUCT *) pFeaturePara;
#if WINMO_USE
    PMSDK_FEATURE_INFO_STRUCT pSensorFeatureInfo=(PMSDK_FEATURE_INFO_STRUCT) pFeaturePara;
#endif

    SENSORDB("Enter S5K8AAYXFeatureControl. ID=%d\n", FeatureId);

    switch (FeatureId)
    {
        case SENSOR_FEATURE_GET_RESOLUTION:
             *pFeatureReturnPara16++ = S5K8AAYX_IMAGE_SENSOR_FULL_WIDTH;
             *pFeatureReturnPara16 = S5K8AAYX_IMAGE_SENSOR_FULL_HEIGHT;
             *pFeatureParaLen=4;
             break;
        case SENSOR_FEATURE_GET_PERIOD:
             //*pFeatureReturnPara16++ = S5K8AAYX_PV_PERIOD_PIXEL_NUMS + S5K8AAYX_PV_dummy_pixels;
             //*pFeatureReturnPara16 = S5K8AAYX_PV_PERIOD_LINE_NUMS + S5K8AAYX_PV_dummy_lines;
             //*pFeatureParaLen=4;
             break;
        case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
             *pFeatureReturnPara32 = S5K8AAYX_sensor_pclk/10;
             *pFeatureParaLen=4;
             break;
        case SENSOR_FEATURE_SET_ESHUTTER:
             break;
        case SENSOR_FEATURE_SET_NIGHTMODE:
             S5K8AAYX_night_mode((BOOL) *pFeatureData16);
             break;
        case SENSOR_FEATURE_SET_GAIN:
        case SENSOR_FEATURE_SET_FLASHLIGHT:
             break;
        case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
             S5K8AAYX_isp_master_clock=*pFeatureData32;
             break;
        case SENSOR_FEATURE_SET_REGISTER:
             S5K8AAYX_write_cmos_sensor(pSensorRegData->RegAddr, pSensorRegData->RegData);
             break;
        case SENSOR_FEATURE_GET_REGISTER:
             pSensorRegData->RegData = S5K8AAYX_read_cmos_sensor(pSensorRegData->RegAddr);
             break;
        case SENSOR_FEATURE_GET_CONFIG_PARA:
             memcpy(pSensorConfigData, &S5K8AAYXSensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));
             *pFeatureParaLen=sizeof(MSDK_SENSOR_CONFIG_STRUCT);
             break;
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
        case SENSOR_FEATURE_GET_GROUP_COUNT:
             *pFeatureReturnPara32++ = 0;
             *pFeatureParaLen = 4;
             break;
        case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
             // get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE
             // if EEPROM does not exist in camera module.
             *pFeatureReturnPara32=LENS_DRIVER_ID_DO_NOT_CARE;
             *pFeatureParaLen=4;
             break;
        case SENSOR_FEATURE_SET_YUV_CMD:
             //S5K8AAYXYUVSensorSetting((MSDK_ISP_FEATURE_ENUM)*pFeatureData16, *(pFeatureData16+1));
             S5K8AAYXYUVSensorSetting((FEATURE_ID)*pFeatureData32, *(pFeatureData32+1));
             break;
             //break;
        #if WINMO_USE
             case SENSOR_FEATURE_QUERY:
                  S5K8AAYXQuery(pSensorFeatureInfo);
                  *pFeatureParaLen = sizeof(MSDK_FEATURE_INFO_STRUCT);
                  break;
             case SENSOR_FEATURE_SET_YUV_CAPTURE_RAW_SUPPORT:
                  /* update yuv capture raw support flag by *pFeatureData16 */
                  break;
        #endif

        case SENSOR_FEATURE_SET_VIDEO_MODE:
             //SENSORDB("S5K8AAYX SENSOR_FEATURE_SET_VIDEO_MODE\r\n");
             S5K8AAYXYUVSetVideoMode(*pFeatureData16);
             break;
        case SENSOR_FEATURE_GET_EV_AWB_REF:
             S5K8AAYXGetEvAwbRef(*pFeatureData32);
             break;
        case SENSOR_FEATURE_GET_SHUTTER_GAIN_AWB_GAIN:
             S5K8AAYXGetCurAeAwbInfo(*pFeatureData32);
             break;
        case SENSOR_FEATURE_CHECK_SENSOR_ID:
             S5K8AAYX_GetSensorID(pFeatureData32);
             break;

        case SENSOR_FEATURE_GET_AF_MAX_NUM_FOCUS_AREAS:
             S5K8AAYXGetAFMaxNumFocusAreas(pFeatureReturnPara32);
             *pFeatureParaLen=4;
             break;
        case SENSOR_FEATURE_GET_AE_MAX_NUM_METERING_AREAS:
             S5K8AAYXGetAFMaxNumMeteringAreas(pFeatureReturnPara32);
             *pFeatureParaLen=4;
             break;
        case SENSOR_FEATURE_GET_EXIF_INFO:
             SENSORDB("SENSOR_FEATURE_GET_EXIF_INFO\n");
             SENSORDB("EXIF addr = 0x%x\n",*pFeatureData32);
             S5K8AAYXGetExifInfo(*pFeatureData32);
             break;
       case SENSOR_FEATURE_SET_TEST_PATTERN:
            S5K8AAYXYUVSetTestPatternMode((BOOL)*pFeatureData16);
            break;
        case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE://for factory mode auto testing
            *pFeatureReturnPara32= S5K8AAYXYUV_TEST_PATTERN_CHECKSUM;
            *pFeatureParaLen=4;
            break;

        default:
             SENSORDB("Enter S5K8AAYXFeatureControl. default. return\n");
             break;
    }
    return ERROR_NONE;
}

SENSOR_FUNCTION_STRUCT SensorFuncS5K8AAYX=
{
    S5K8AAYXOpen,
    S5K8AAYXGetInfo,
    S5K8AAYXGetResolution,
    S5K8AAYXFeatureControl,
    S5K8AAYXControl,
    S5K8AAYXClose
};

UINT32 S5K8AAYX_PVI_YUV_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
    SENSORDB("Enter S5K8AAYX_MIPI_YUV_SensorInit\n");
    /* To Do : Check Sensor status here */
    if (pfFunc!=NULL)
        *pfFunc=&SensorFuncS5K8AAYX;
    return ERROR_NONE;
}   /* SensorInit() */

