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

#include "ov16825mipiraw_Sensor.h"
#include "ov16825mipiraw_Camera_Sensor_para.h"
#include "ov16825mipiraw_CameraCustomized.h"

#define OV16825_DEBUG
#define OV16825_DRIVER_TRACE
#define LOG_TAG "[OV16825MIPIRaw]"
#ifdef OV16825_DEBUG
#define SENSORDB(fmt,arg...) printk(LOG_TAG "%s: " fmt "\n", __FUNCTION__ ,##arg)
#else
#define SENSORDB printk
#endif
//#define ACDK
extern int iReadRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u8 * a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u16 i2cId);



MSDK_SCENARIO_ID_ENUM ov16825CurrentScenarioId = MSDK_SCENARIO_ID_CAMERA_PREVIEW;
static OV16825_sensor_struct OV16825_sensor =
{
  //.eng =
  //{
    //.reg = OV16825_CAMERA_SENSOR_REG_DEFAULT_VALUE,
    //.cct = OV16825_CAMERA_SENSOR_CCT_DEFAULT_VALUE,
  //},
  .eng_info =
  {
    .SensorId = OV16825MIPI_SENSOR_ID,
    .SensorType = CMOS_SENSOR,
    .SensorOutputDataFormat = OV16825_COLOR_FORMAT,
  },
  .shutter = 0x20,  
  .gain = 0x20,
  .pv_pclk = OV16825_PREVIEW_CLK,
  .cap_pclk = OV16825_CAPTURE_CLK,
  .pclk = OV16825_PREVIEW_CLK,
  .frame_height = OV16825_PV_PERIOD_LINE_NUMS,
  .line_length = OV16825_PV_PERIOD_PIXEL_NUMS,
  .is_zsd = KAL_FALSE, //for zsd
  .dummy_pixels = 0,
  .dummy_lines = 0,  //for zsd
  .is_autofliker = KAL_FALSE,
};

static DEFINE_SPINLOCK(OV16825_drv_lock);

kal_uint16 OV16825_read_cmos_sensor(kal_uint32 addr)
{
    kal_uint16 get_byte=0;
    char puSendCmd[2] = {(char)(addr >> 8) , (char)(addr & 0xFF) };
    iReadRegI2C(puSendCmd , 2, (u8*)&get_byte,1,OV16825_sensor.write_id);
#ifdef OV16825_DRIVER_TRACE
	//SENSORDB("OV16825_read_cmos_sensor, addr:%x;get_byte:%x \n",addr,get_byte);
#endif
    return get_byte;
}
kal_uint16 OV16825_read_cmos_sensor_otp(kal_uint16 addr, kal_uint8 write_id)
{
    kal_uint16 get_byte=0;
    char puSendCmd[1] = {(char)addr&0xFF};
    iReadRegI2C(puSendCmd , 1, (u8*)&get_byte,1,write_id);
#ifdef OV16825_DRIVER_TRACE
	//SENSORDB("OV16825_read_cmos_sensor OTP, addr:%x;get_byte:%x \n",addr,get_byte);
#endif
    return get_byte;
}


kal_uint16 OV16825_write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
    //kal_uint16 reg_tmp;

    char puSendCmd[3] = {(char)(addr >> 8) , (char)(addr & 0xFF) ,(char)(para & 0xFF)};

    iWriteRegI2C(puSendCmd , 3,OV16825_sensor.write_id);
    return ERROR_NONE;
}

/*****************************  OTP Feature  **********************************/
//#define OV16825_USE_OTP
#if 0
static void clear_otp_buffer(void)
{
	int i;
	// clear otp buffer
	for (i=0;i<16;i++) {
		OV16825_write_cmos_sensor(0x3d00 + i, 0x00);
	}
}

static void OV16825_Read_OTP(void)
{
	kal_uint8 value = 0;
	kal_uint16 addr = 0;
	kal_uint8 write_id = 0;
	addr = 0x00;
	write_id = 0xA0;
	for(addr = 0; addr <= 0xFF; addr++){
		value = OV16825_read_cmos_sensor_otp(addr,write_id);
		SENSORDB("test OV16825_read_cmos_sensor OTP, write_id = %x, addr:%x, value:%x \n",write_id,addr,value);
	}
	write_id = 0xA2;
	for(addr = 0; addr <= 0xFF; addr++){
		value = OV16825_read_cmos_sensor_otp(addr,write_id);
		SENSORDB("test OV16825_read_cmos_sensor OTP, write_id = %x, addr:%x, value:%x \n",write_id,addr,value);
	}
	
	write_id = 0xA4;
	for(addr = 0; addr <= 0xFF; addr++){
		value = OV16825_read_cmos_sensor_otp(addr,write_id);
		SENSORDB("test OV16825_read_cmos_sensor OTP, write_id = %x, addr:%x, value:%x \n",write_id,addr,value);
	}
	

	

}
#endif

#if defined(OV16825_USE_WB_OTP)

//For HW define
struct otp_struct {
	int product_year;
	int product_month;
	int product_day;
	int module_integrator_id; 
	int rg_ratio;
	int bg_ratio;
	int br_ratio;
	int VCM_start;
	int VCM_end;
	int lenc[62];
	int light_rg;
	int light_bg;

};

// R/G and B/G of typical camera module is defined here

int RG_Ratio_Typical = 584;
int BG_Ratio_Typical = 602;



//For HW
// index: index of otp group. (0, 1, 2)
// return: 	index 0, 1, 2, if empty, return 4;
static int check_otp_wb(void)
{
	int flag_Pro,flag_AWB;
	int index;
	int bank, address;
	kal_uint8 write_id;

	write_id = 0xA0;
	
	//for(addr = 0x09; addr <= 0x0C; addr++){
	//	value = OV16825_read_cmos_sensor_otp(addr,write_id);
	//	SENSORDB("test OV16825_read_cmos_sensor OTP, write_id = %x, addr:%x, value:%x \n",write_id,addr,value);
	//}
	
	flag_Pro= OV16825_read_cmos_sensor_otp(0x00,write_id);
	flag_AWB= OV16825_read_cmos_sensor_otp(0x08,write_id);
	
	SENSORDB("check_otp_wb, flag_Pro = %x, flag_AWB = %x\n", flag_Pro, flag_AWB);
	if((flag_Pro&0x01)&&(flag_AWB&0x01))
		return 1;
	else
		return 0;
/*
	//for(index = 0;index<3;index++)
	{
		// select bank index
		bank = 0xc0 | (index*5+1);
		OV16825_write_cmos_sensor;

		// read otp into buffer
		OV16825_write_cmos_sensor(0x3d81, 0x01);
		mdelay(5);
		// disable otp read
		//OV16825_write_cmos_sensor(0x3d81, 0x00);

		// read WB
		address = 0x3d08;
		int temp1 = OV16825_read_cmos_sensor(address);
		int temp2 =OV16825_read_cmos_sensor(address+1);
		flag = (OV16825_read_cmos_sensor(address) << 8)+ OV16825_read_cmos_sensor(address+1);
	
    	SENSORDB("check_otp_wb, temp1 = %x, temp2 = %x\n", temp1, temp2);
    	SENSORDB("check_otp_wb, temp1 = %x, temp2 = %x\n", temp1, temp2);

		OV16825_write_cmos_sensor(0x3d81, 0x00);

		clear_otp_buffer();

		if (flag==0) {
			return index-1;
		}
	
	}
	*/
	//return 2;
}




// For HW
// index: index of otp group. (0, 1, 2)
// otp_ptr: pointer of otp_struct
// return: 	0, 
static int read_otp_wb( struct otp_struct * otp_ptr)
{
	//int bank;
	//int address;
	/*AWB write_ID:A0, 0x07~0x10*/
	/*0x400 = 1.0*/
	kal_uint8 value = 0;
	kal_uint16 addr = 0;
	kal_uint8 write_id = 0;
	kal_uint16 WB_R, WB_Gr, WB_Gb,WB_B,WB_G;
	
	write_id = 0xA0;
	
	//for(addr = 0x09; addr <= 0x0C; addr++){
	//	value = OV16825_read_cmos_sensor_otp(addr,write_id);
	//	SENSORDB("test OV16825_read_cmos_sensor OTP, write_id = %x, addr:%x, value:%x \n",write_id,addr,value);
	//}
	
	WB_R = OV16825_read_cmos_sensor_otp(0x09,write_id);
	WB_Gr = OV16825_read_cmos_sensor_otp(0x0A,write_id);
	WB_Gb = OV16825_read_cmos_sensor_otp(0x0B,write_id);
	WB_B = OV16825_read_cmos_sensor_otp(0x0C,write_id);

	WB_G = (WB_Gr + WB_Gb)/2;


	//(*otp_ptr).product_year =  OV16825_read_cmos_sensor(address);
	//(*otp_ptr).product_month = OV16825_read_cmos_sensor(address + 1);
	//(*otp_ptr).product_day = OV16825_read_cmos_sensor(address + 2);
	//(*otp_ptr).module_integrator_id = OV16825_read_cmos_sensor(address + 7);
	(*otp_ptr).rg_ratio = (WB_R*1024)/WB_G;
	(*otp_ptr).bg_ratio = (WB_B*1024)/WB_G;
	(*otp_ptr).br_ratio = (WB_B*1024)/WB_R;
	
	//(*otp_ptr).bg_ratio = ((OV16825_read_cmos_sensor(address + 10))<<8)+(OV16825_read_cmos_sensor(address + 11));
	//(*otp_ptr).br_ratio = ((OV16825_read_cmos_sensor(address + 12))<<8)+(OV16825_read_cmos_sensor(address + 13));
	//(*otp_ptr).VCM_start = OV16825_read_cmos_sensor(address + 14);
	//(*otp_ptr).VCM_end = OV16825_read_cmos_sensor(address + 15);
	
	// disable otp read
	//OV16825_write_cmos_sensor(0x3d81, 0x00);

	//clear_otp_buffer();

//no write light sourch
	(*otp_ptr).light_rg =0;
	(*otp_ptr).light_bg =0;

	//bank = 0xc0 | (5*index+5);
	//OV16825_write_cmos_sensor(0x3d84, bank);

	// read otp into buffer
	//OV16825_write_cmos_sensor(0x3d81, 0x01);
	//address = 0x3d0e;
	//(*otp_ptr).light_rg =OV16825_read_cmos_sensor(address);
	//(*otp_ptr).light_bg =OV16825_read_cmos_sensor(address+1);

	// disable otp read
	//OV16825_write_cmos_sensor(0x3d81, 0x00);

	//clear_otp_buffer();
	return 0;	
}


// R_gain, sensor red gain of AWB, 0x400 =1
// G_gain, sensor green gain of AWB, 0x400 =1
// B_gain, sensor blue gain of AWB, 0x400 =1
// return 0;
static int update_awb_gain(int R_gain, int G_gain, int B_gain)
{
	if (R_gain>=0x400) {
		OV16825_write_cmos_sensor(0x500C, (R_gain>>8)&0x0F);
		OV16825_write_cmos_sensor(0x500D, R_gain & 0x00ff);
	}

	if (G_gain>=0x400) {
		OV16825_write_cmos_sensor(0x500E, (G_gain>>8)&0x0F);
		OV16825_write_cmos_sensor(0x500F, G_gain & 0x00ff);
	}

	if (B_gain>=0x400) {
		OV16825_write_cmos_sensor(0x5010, (B_gain>>8)&0x0F);
		OV16825_write_cmos_sensor(0x5011, B_gain & 0x00ff);
	}

    SENSORDB("update_awb_gain, 0x500C = %x\n", OV16825_read_cmos_sensor(0x500C));
    SENSORDB("update_awb_gain, 0x500D = %x\n", OV16825_read_cmos_sensor(0x500D));
    SENSORDB("update_awb_gain, 0x500E = %x\n", OV16825_read_cmos_sensor(0x500E));
    SENSORDB("update_awb_gain, 0x500F = %x\n", OV16825_read_cmos_sensor(0x500F));
    SENSORDB("update_awb_gain, 0x5010 = %x\n", OV16825_read_cmos_sensor(0x5010));
    SENSORDB("update_awb_gain, 0x5011 = %x\n", OV16825_read_cmos_sensor(0x5011));
	
	return 0;
}

// call this function after OV16825 initialization
// return value: 0 update success
//		1, no OTP
static int update_otp_wb(void)
{
	struct otp_struct current_otp;
	int otp_valid;
	int R_gain, G_gain, B_gain, G_gain_R, G_gain_B;
	int rg,bg;
	SENSORDB("update_otp_wb\n");


	// R/G and B/G of current camera module is read out from sensor OTP
	// check first OTP with valid data
	SENSORDB("OV16825_Upate_Otp_WB,\n");
	otp_valid = check_otp_wb();
	if(otp_valid==0)
	{	
		// no valid wb OTP data
		
		SENSORDB("OV16825_Upate_Otp_WB,no valid wb OTP data\n");
		return 1;
	}	

	read_otp_wb(&current_otp);



	if(current_otp.light_rg==0) {
		// no light source information in OTP, light factor = 1
		rg = current_otp.rg_ratio;
	}
	else {
		rg = current_otp.rg_ratio * (current_otp.light_rg +512) /1024;
	}
	
	if(current_otp.light_bg==0) {
		// not light source information in OTP, light factor = 1
		bg = current_otp.bg_ratio;
	}
	else {
		bg = current_otp.bg_ratio * (current_otp.light_bg +512) /1024;
	}

    SENSORDB("OV16825_Upate_Otp_WB, r/g:0x%x, b/g:0x%x\n", rg, bg);

	//calculate G gain
	//0x400 = 1x gain
	if(bg < BG_Ratio_Typical) {
		if (rg< RG_Ratio_Typical) {
			// current_otp.bg_ratio < BG_Ratio_typical &&  
			// current_otp.rg_ratio < RG_Ratio_typical
   			G_gain = 0x400;
			B_gain = 0x400 * BG_Ratio_Typical / bg;
    		R_gain = 0x400 * RG_Ratio_Typical / rg; 
		}
		else {
			// current_otp.bg_ratio < BG_Ratio_typical &&  
			// current_otp.rg_ratio >= RG_Ratio_typical
    		R_gain = 0x400;
   	 		G_gain = 0x400 * rg / RG_Ratio_Typical;
    		B_gain = G_gain * BG_Ratio_Typical /bg;
		}
	}
	else {
		if (rg < RG_Ratio_Typical) {
			// current_otp.bg_ratio >= BG_Ratio_typical &&  
			// current_otp.rg_ratio < RG_Ratio_typical
    		B_gain = 0x400;
    		G_gain = 0x400 * bg / BG_Ratio_Typical;
    		R_gain = G_gain * RG_Ratio_Typical / rg;
		}
		else {
			// current_otp.bg_ratio >= BG_Ratio_typical &&  
			// current_otp.rg_ratio >= RG_Ratio_typical
    		G_gain_B = 0x400 * bg / BG_Ratio_Typical;
   	 		G_gain_R = 0x400 * rg / RG_Ratio_Typical;

    		if(G_gain_B > G_gain_R ) {
        				B_gain = 0x400;
        				G_gain = G_gain_B;
 	     			R_gain = G_gain * RG_Ratio_Typical /rg;
  			}
    		else {
        			R_gain = 0x400;
       				G_gain = G_gain_R;
        			B_gain = G_gain * BG_Ratio_Typical / bg;
			}
    	}    
	}
    SENSORDB("OV16825_Upate_Otp_WB, R_gain:0x%x, G_gain:0x%x, B_gain:0x%x\n", R_gain, G_gain, B_gain);

	update_awb_gain(R_gain, G_gain, B_gain);

	return 0;

}


#endif

#if 0
// For HW
// index: index of otp group. (0, 1, 2)
// otp_ptr: pointer of otp_struct
// return: 	0, 
static int read_otp_lenc(int index, struct otp_struct * otp_ptr)
{
	int bank, i;
	int address;

	// select bank:2,7,12,
	bank = 0xc0 + (index*5+2);
	OV16825_write_cmos_sensor(0x3d84, bank);

	// read otp into buffer
	OV16825_write_cmos_sensor(0x3d81, 0x01);
	mdelay(10);

	address = 0x3d00;
	for(i=0;i<16;i++) {
		(* otp_ptr).lenc[i]=OV16825_read_cmos_sensor(address);
		address++;
	}
	
	// disable otp read
	OV16825_write_cmos_sensor(0x3d81, 0x00);

	clear_otp_buffer();

	// select 2nd bank
	bank++;
	OV16825_write_cmos_sensor(0x3d84, bank);

	// read otp
	OV16825_write_cmos_sensor(0x3d81, 0x01);
	mdelay(10);

	address = 0x3d00;
	for(i=16;i<32;i++) {
		(* otp_ptr).lenc[i]=OV16825_read_cmos_sensor(address);
		address++;
	}

	// disable otp read
	OV16825_write_cmos_sensor(0x3d81, 0x00);

	clear_otp_buffer();

	// select 3rd bank
	bank++;
	OV16825_write_cmos_sensor(0x3d84, bank);

	// read otp
	OV16825_write_cmos_sensor(0x3d81, 0x01);
	mdelay(10);

	address = 0x3d00;
	for(i=32;i<48;i++) {
		(* otp_ptr).lenc[i]=OV16825_read_cmos_sensor(address);
		address++;
	}

	// disable otp read
	OV16825_write_cmos_sensor(0x3d81, 0x00);

	clear_otp_buffer();

	// select 4th bank
	bank++;
	OV16825_write_cmos_sensor(0x3d84, bank);

	// read otp
	OV16825_write_cmos_sensor(0x3d81, 0x01);
	mdelay(10);

	address = 0x3d00;
	for(i=48;i<62;i++) {
		(* otp_ptr).lenc[i]=OV16825_read_cmos_sensor(address);
		address++;
	}

	// disable otp read
	OV16825_write_cmos_sensor(0x3d81, 0x00);

	clear_otp_buffer();

	return 0;	
}



// call this function after OV16825 initialization
// otp_ptr: pointer of otp_struct
static int update_lenc(struct otp_struct * otp_ptr)
{
	int i, temp;
	temp = 0x80|OV16825_read_cmos_sensor(0x5000);
	OV16825_write_cmos_sensor(0x5000, temp);

	for(i=0;i<62;i++) {
		OV16825_write_cmos_sensor(0x5800 + i, (*otp_ptr).lenc[i]);
	}

	for(i=0;i<62;i++){
		SENSORDB("update_lenc, 0x5800 + %d = %x\n", i,OV16825_read_cmos_sensor((0x5800)+i));
	}

	return 0;
}



static int update_otp_lenc()
{
	struct otp_struct current_otp;
	int otp_index;

	// R/G and B/G of current camera module is read out from sensor OTP
	// check first OTP with valid data
	
	otp_index = check_otp_wb();
	if(otp_index==-1)
	{	
		// no valid wb OTP data
		return 1;
	}	
	read_otp_lenc(otp_index, &current_otp);

	update_lenc(&current_otp);
	return 0;
}


#endif

#if 0
// Always Do check_dcblc
// return: 	0 – use module DCBLC, 
//			1 – use sensor DCBL 
//			2 – use defualt DCBLC
static int check_dcblc()
{
	int bank, dcblc;
	int address;
	int temp, flag;

	// select bank 31
	bank = 0xc0 | 31;
	OV16825_write_cmos_sensor(0x3d84, bank);

	// read otp into buffer
	OV16825_write_cmos_sensor(0x3d81, 0x01);
	mdelay(10);

	temp = OV16825_read_cmos_sensor(0x4000);
	address = 0x3d0b;
	dcblc = OV16825_read_cmos_sensor(address);

	if ((dcblc>=0x15) && (dcblc<=0x40)){
		// module DCBLC value is valid
		if((temp && 0x08)==0) {
			// DCBLC auto load
			flag = 0;
			clear_otp_buffer();
			return flag;
		}
	}

	address--;
	dcblc = OV16825_read_cmos_sensor(address);
	if ((dcblc>=0x10) && (dcblc<=0x40)){
		// sensor DCBLC value is valid
		temp = temp | 0x08;		// DCBLC manual load enable
		OV16825_write_cmos_sensor(0x4000, temp);
		OV16825_write_cmos_sensor(0x4006, dcblc);	// manual load sensor level DCBLC

		flag = 1;				// sensor level DCBLC is used
	}
	else{
		OV16825_write_cmos_sensor(0x4006, 0x20);
		flag = 2;				// default DCBLC is used
	}

    SENSORDB("check_dcblc, 0x4000 = %x\n", OV16825_read_cmos_sensor(0x4000));
    SENSORDB("check_dcblc, 0x4006 = %x\n", OV16825_read_cmos_sensor(0x4006));
	// disable otp read
	OV16825_write_cmos_sensor(0x3d81, 0x00);

	clear_otp_buffer();

	return flag;	
}
#endif

/*****************************  OTP Feature  End**********************************/

void OV16825_Write_Shutter(kal_uint16 ishutter)
{

    kal_uint16 extra_shutter = 0;
    kal_uint16 realtime_fp = 0;
    kal_uint16 frame_height = 0;
    //kal_uint16 line_length = 0;

    unsigned long flags;
#ifdef OV16825_DRIVER_TRACE
    SENSORDB("OV16825_write_shutter:%x \n",ishutter);
#endif
   if (!ishutter) ishutter = 1; /* avoid 0 */

    if (OV16825_sensor.pv_mode){
        //line_length = OV16825_PV_PERIOD_PIXEL_NUMS;
        frame_height = OV16825_PV_PERIOD_LINE_NUMS + OV16825_sensor.dummy_lines;
    }
    else if (OV16825_sensor.video_mode) {
        //line_length = OV16825_VIDEO_PERIOD_PIXEL_NUMS;
		  frame_height = OV16825_VIDEO_PERIOD_LINE_NUMS + OV16825_sensor.dummy_lines;
    }
    else{
        //line_length = OV16825_FULL_PERIOD_PIXEL_NUMS;
        frame_height = OV16825_FULL_PERIOD_LINE_NUMS + OV16825_sensor.dummy_lines;
    }

    if(ishutter > (frame_height -4))
    {
		extra_shutter = ishutter - frame_height + 4;
        SENSORDB("[shutter > frame_height] frame_height:%x extra_shutter:%x \n",frame_height,extra_shutter);
    }
    else  
    {
        extra_shutter = 0;
    }
    frame_height += extra_shutter;
    OV16825_sensor.frame_height = frame_height;
    SENSORDB("OV16825_sensor.is_autofliker:%x, OV16825_sensor.frame_height: %x \n",OV16825_sensor.is_autofliker,OV16825_sensor.frame_height);
	#if 1
    if(OV16825_sensor.is_autofliker == KAL_TRUE)
    {
        realtime_fp = OV16825_sensor.pclk *10 / ((OV16825_sensor.line_length/4) * OV16825_sensor.frame_height);
        SENSORDB("[OV16825_Write_Shutter]pv_clk:%d\n",OV16825_sensor.pclk);
        SENSORDB("[OV16825_Write_Shutter]line_length/4:%d\n",(OV16825_sensor.line_length/4));
        SENSORDB("[OV16825_Write_Shutter]frame_height:%d\n",OV16825_sensor.frame_height);
        SENSORDB("[OV16825_Write_Shutter]framerate(10base):%d\n",realtime_fp);

        if((realtime_fp >= 297)&&(realtime_fp <= 303))
        {
            realtime_fp = 296;
            spin_lock_irqsave(&OV16825_drv_lock,flags);
            OV16825_sensor.frame_height = OV16825_sensor.pclk *10 / ((OV16825_sensor.line_length/4) * realtime_fp);
            spin_unlock_irqrestore(&OV16825_drv_lock,flags);

            SENSORDB("[autofliker realtime_fp=30,extern heights slowdown to 29.6fps][height:%d]",OV16825_sensor.frame_height);
        }
      else if((realtime_fp >= 147)&&(realtime_fp <= 153))
        {
            realtime_fp = 146;
            spin_lock_irqsave(&OV16825_drv_lock,flags);
            OV16825_sensor.frame_height = OV16825_sensor.pclk *10 / ((OV16825_sensor.line_length/4) * realtime_fp);
            spin_unlock_irqrestore(&OV16825_drv_lock,flags);
            SENSORDB("[autofliker realtime_fp=15,extern heights slowdown to 14.6fps][height:%d]",OV16825_sensor.frame_height);
        }
    //OV16825_sensor.frame_height = OV16825_sensor.frame_height +(OV16825_sensor.frame_height>>7);

    }
	#endif
    OV16825_write_cmos_sensor(0x380e, (OV16825_sensor.frame_height>>8)&0xFF);
    OV16825_write_cmos_sensor(0x380f, (OV16825_sensor.frame_height)&0xFF);

    OV16825_write_cmos_sensor(0x3500, (ishutter >> 12) & 0xF);
    OV16825_write_cmos_sensor(0x3501, (ishutter >> 4) & 0xFF);
    OV16825_write_cmos_sensor(0x3502, (ishutter << 4) & 0xFF);

}


/*************************************************************************
* FUNCTION
*   OV16825_Set_Dummy
*
* DESCRIPTION
*   This function set dummy pixel or dummy line of OV16825
*
* PARAMETERS
*   iPixels : dummy pixel
*   iLines :  dummy linel
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/

static void OV16825_Set_Dummy(const kal_uint16 iPixels, const kal_uint16 iLines)
{
    kal_uint16 line_length = 0;
	kal_uint16 frame_height = 0;
#ifdef OV16825_DRIVER_TRACE
    SENSORDB("OV16825_Set_Dummy:iPixels:%x; iLines:%x \n",iPixels,iLines);
#endif
	

    OV16825_sensor.dummy_lines = iLines;
    OV16825_sensor.dummy_pixels = iPixels;

    switch (ov16825CurrentScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            //case MSDK_SCENARIO_ID_VIDEO_CAPTURE_MPEG4:
            line_length = OV16825_PV_PERIOD_PIXEL_NUMS + iPixels;
            frame_height = OV16825_PV_PERIOD_LINE_NUMS + iLines;
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            line_length = OV16825_VIDEO_PERIOD_PIXEL_NUMS + iPixels;
            frame_height = OV16825_VIDEO_PERIOD_LINE_NUMS + iLines;
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            //case MSDK_SCENARIO_ID_CAMERA_CAPTURE_MEM:
            line_length = OV16825_FULL_PERIOD_PIXEL_NUMS + iPixels;
            frame_height = OV16825_FULL_PERIOD_LINE_NUMS + iLines;
            break;
        case MSDK_SCENARIO_ID_CAMERA_ZSD:
            line_length = OV16825_FULL_PERIOD_PIXEL_NUMS + iPixels;
            frame_height = OV16825_FULL_PERIOD_LINE_NUMS + iLines;
            break;
        case MSDK_SCENARIO_ID_CAMERA_3D_PREVIEW: //added
        case MSDK_SCENARIO_ID_CAMERA_3D_VIDEO:
        case MSDK_SCENARIO_ID_CAMERA_3D_CAPTURE: //added   
            line_length = OV16825_PV_PERIOD_PIXEL_NUMS + iPixels;
            frame_height = OV16825_PV_PERIOD_LINE_NUMS + iLines;
            break;
		default:
            line_length = OV16825_PV_PERIOD_PIXEL_NUMS + iPixels;
            frame_height = OV16825_PV_PERIOD_LINE_NUMS + iLines;
            break;
    }

#ifdef OV16825_DRIVER_TRACE
    SENSORDB("line_length:%x; frame_height:%x \n",line_length,frame_height);
#endif

    if ((line_length >= 0xFFFF)||(frame_height >= 0xFFFF))
    {
        #ifdef OV16825_DRIVER_TRACE
        SENSORDB("ERROR: line length or frame height is overflow!!!!!!!!  \n");
        #endif
		return;
        //return ERROR_NONE;
    }
//	if((line_length == OV16825_sensor.line_length)&&(frame_height == OV16825_sensor.frame_height))
//		return ;
    spin_lock(&OV16825_drv_lock);
    OV16825_sensor.line_length = line_length;
    OV16825_sensor.frame_height = frame_height;
    spin_unlock(&OV16825_drv_lock);

    SENSORDB("line_length:%x; frame_height:%x \n",line_length,frame_height);
	SENSORDB("write to register line_length/4:%x; frame_height:%x \n",(line_length/4),frame_height);

    /*  Add dummy pixels: */
    /* 0x380c [0:4], 0x380d defines the PCLKs in one line of OV16825  */  
    /* Add dummy lines:*/
    /* 0x380e [0:1], 0x380f defines total lines in one frame of OV16825 */
    OV16825_write_cmos_sensor(0x380c, (line_length/4) >> 8);
    OV16825_write_cmos_sensor(0x380d, (line_length/4) & 0xFF);
    OV16825_write_cmos_sensor(0x380e, frame_height >> 8);
    OV16825_write_cmos_sensor(0x380f, frame_height & 0xFF);
    //return ERROR_NONE;
	return;
}   /*  OV16825_Set_Dummy    */


/*************************************************************************
* FUNCTION
*	OV16825_SetShutter
*
* DESCRIPTION
*	This function set e-shutter of OV16825 to change exposure time.
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


void set_OV16825_shutter(kal_uint16 iShutter)
{

    unsigned long flags;
#ifdef OV16825_DRIVER_TRACE
    SENSORDB("set_OV16825_shutter:%x \n",iShutter);
#endif


    #if 0
    if((OV16825_sensor.pv_mode == KAL_FALSE)&&(OV16825_sensor.is_zsd == KAL_FALSE))
    {
        SENSORDB("[set_OV16825_shutter]now is in 1/4size cap mode\n");
        //return;
    }
    else if((OV16825_sensor.is_zsd == KAL_TRUE)&&(OV16825_sensor.is_zsd_cap == KAL_TRUE))
    {
        SENSORDB("[set_OV16825_shutter]now is in zsd cap mode\n");

        //SENSORDB("[set_OV16825_shutter]0x3500:%x\n",OV16825_read_cmos_sensor(0x3500));
        //SENSORDB("[set_OV16825_shutter]0x3500:%x\n",OV16825_read_cmos_sensor(0x3501));
        //SENSORDB("[set_OV16825_shutter]0x3500:%x\n",OV16825_read_cmos_sensor(0x3502));
        //return;
    }
    #endif
    #if 0
    if(OV16825_sensor.shutter == iShutter)
    {
        SENSORDB("[set_OV16825_shutter]shutter is the same with previous, skip\n");
        return;
    }
    #endif

    spin_lock_irqsave(&OV16825_drv_lock,flags);
    OV16825_sensor.shutter = iShutter;
    spin_unlock_irqrestore(&OV16825_drv_lock,flags);

    OV16825_Write_Shutter(iShutter);

}   /*  Set_OV16825_Shutter */

 kal_uint16 OV16825Gain2Reg(const kal_uint16 iGain)
{
    kal_uint16 iReg = 0x00;

    //iReg = ((iGain / BASEGAIN) << 4) + ((iGain % BASEGAIN) * 16 / BASEGAIN);
    iReg = iGain *16 / BASEGAIN;

    iReg = iReg & 0xFF;
#ifdef OV16825_DRIVER_TRACE
    SENSORDB("OV16825Gain2Reg:iGain:%x; iReg:%x \n",iGain,iReg);
#endif
    return iReg;
}


kal_uint16 OV16825_SetGain(kal_uint16 iGain)
{
   kal_uint16 i;
   kal_uint16 gain_reg = 0;
#ifdef OV16825_DRIVER_TRACE
   SENSORDB("OV16825_SetGain:iGain = %x;\n",iGain);
	SENSORDB("OV16825_SetGain:gain_reg 0 = %x;\n",gain_reg);

#endif
	/*
		sensor gain 1x = 128
		max gain = 0x7ff = 15.992x <16x
		here we just use 0x3508 analog gain 1 bit[3:2].
		16x~32x should use 0x3508 analog gain 0 bit[1:0]
	*/
	iGain *= 2;
	iGain = (iGain & 0x7ff);
	
	for(i = 1; i <= 3; i++){
		if(iGain >= 0x100){
			gain_reg = gain_reg + 1;
			iGain = iGain/2;			
		}
	}
	SENSORDB("OV16825_SetGain:gain_reg 1 = %x;\n",gain_reg);
	gain_reg = (gain_reg << 2);
	SENSORDB("OV16825_SetGain:gain_reg 2 = %x;\n",gain_reg);
	/*下面这个if 其实不会跑进来，是因为iGain = (iGain & 0x7ff);  这里限制了最大不超过16x*/
	//if(iGain > 0x100){  
	//	gain_reg = gain_reg + 1;
	//	iGain = iGain/2;
	//	}
	
	SENSORDB("OV16825_SetGain:iGain = %x, gain_reg = %x;\n",iGain, gain_reg);
	
	OV16825_write_cmos_sensor(0x3509, iGain);
	OV16825_write_cmos_sensor(0x3508, gain_reg);
    
    return ERROR_NONE;
}




/*************************************************************************
* FUNCTION
*	OV16825_SetGain
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

#if 0
void OV16825_set_isp_driving_current(kal_uint16 current)
{
#ifdef OV16825_DRIVER_TRACE
   SENSORDB("OV16825_set_isp_driving_current:current:%x;\n",current);
#endif
  //iowrite32((0x2 << 12)|(0<<28)|(0x8880888), 0xF0001500);
}
#endif

/*************************************************************************
* FUNCTION
*	OV16825_NightMode
*
* DESCRIPTION
*	This function night mode of OV16825.
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
void OV16825_night_mode(kal_bool enable)
{
}   /*  OV16825_NightMode    */


/* write camera_para to sensor register */
static void OV16825_camera_para_to_sensor(void)
{
    kal_uint32 i;
#ifdef OV16825_DRIVER_TRACE
    SENSORDB("OV16825_camera_para_to_sensor\n");
#endif
  for (i = 0; 0xFFFFFFFF != OV16825_sensor.eng.reg[i].Addr; i++)
  {
    OV16825_write_cmos_sensor(OV16825_sensor.eng.reg[i].Addr, OV16825_sensor.eng.reg[i].Para);
  }
  for (i = OV16825_FACTORY_START_ADDR; 0xFFFFFFFF != OV16825_sensor.eng.reg[i].Addr; i++)
  {
    OV16825_write_cmos_sensor(OV16825_sensor.eng.reg[i].Addr, OV16825_sensor.eng.reg[i].Para);
  }
  OV16825_SetGain(OV16825_sensor.gain); /* update gain */
}

/* update camera_para from sensor register */
static void OV16825_sensor_to_camera_para(void)
{
  kal_uint32 i;
  kal_uint32 temp_data;
  
#ifdef OV16825_DRIVER_TRACE
   SENSORDB("OV16825_sensor_to_camera_para\n");
#endif
  for (i = 0; 0xFFFFFFFF != OV16825_sensor.eng.reg[i].Addr; i++)
  {
    temp_data = OV16825_read_cmos_sensor(OV16825_sensor.eng.reg[i].Addr);

    spin_lock(&OV16825_drv_lock);
    OV16825_sensor.eng.reg[i].Para = temp_data;
    spin_unlock(&OV16825_drv_lock);

    }
  for (i = OV16825_FACTORY_START_ADDR; 0xFFFFFFFF != OV16825_sensor.eng.reg[i].Addr; i++)
  {
    temp_data = OV16825_read_cmos_sensor(OV16825_sensor.eng.reg[i].Addr);

    spin_lock(&OV16825_drv_lock);
    OV16825_sensor.eng.reg[i].Para = temp_data;
    spin_unlock(&OV16825_drv_lock);
  }
}

/* ------------------------ Engineer mode ------------------------ */
inline static void OV16825_get_sensor_group_count(kal_int32 *sensor_count_ptr)
{
#ifdef OV16825_DRIVER_TRACE
   SENSORDB("OV16825_get_sensor_group_count\n");
#endif
  *sensor_count_ptr = OV16825_GROUP_TOTAL_NUMS;
}

inline static void OV16825_get_sensor_group_info(MSDK_SENSOR_GROUP_INFO_STRUCT *para)
{
#ifdef OV16825_DRIVER_TRACE
   SENSORDB("OV16825_get_sensor_group_info\n");
#endif
  switch (para->GroupIdx)
  {
  case OV16825_PRE_GAIN:
    sprintf(para->GroupNamePtr, "CCT");
    para->ItemCount = 5;
    break;
  case OV16825_CMMCLK_CURRENT:
    sprintf(para->GroupNamePtr, "CMMCLK Current");
    para->ItemCount = 1;
    break;
  case OV16825_FRAME_RATE_LIMITATION:
    sprintf(para->GroupNamePtr, "Frame Rate Limitation");
    para->ItemCount = 2;
    break;
  case OV16825_REGISTER_EDITOR:
    sprintf(para->GroupNamePtr, "Register Editor");
    para->ItemCount = 2;
    break;
  default:
    ASSERT(0);
  }
}

inline static void OV16825_get_sensor_item_info(MSDK_SENSOR_ITEM_INFO_STRUCT *para)
{

  const static kal_char *cct_item_name[] = {"SENSOR_BASEGAIN", "Pregain-R", "Pregain-Gr", "Pregain-Gb", "Pregain-B"};
  const static kal_char *editer_item_name[] = {"REG addr", "REG value"};
  
#ifdef OV16825_DRIVER_TRACE
	 SENSORDB("OV16825_get_sensor_item_info\n");
#endif
  switch (para->GroupIdx)
  {
  case OV16825_PRE_GAIN:
    switch (para->ItemIdx)
    {
    case OV16825_SENSOR_BASEGAIN:
    case OV16825_PRE_GAIN_R_INDEX:
    case OV16825_PRE_GAIN_Gr_INDEX:
    case OV16825_PRE_GAIN_Gb_INDEX:
    case OV16825_PRE_GAIN_B_INDEX:
      break;
    default:
      ASSERT(0);
    }
    sprintf(para->ItemNamePtr, cct_item_name[para->ItemIdx - OV16825_SENSOR_BASEGAIN]);
    para->ItemValue = OV16825_sensor.eng.cct[para->ItemIdx].Para * 1000 / BASEGAIN;
    para->IsTrueFalse = para->IsReadOnly = para->IsNeedRestart = KAL_FALSE;
    para->Min = OV16825_MIN_ANALOG_GAIN * 1000;
    para->Max = OV16825_MAX_ANALOG_GAIN * 1000;
    break;
  case OV16825_CMMCLK_CURRENT:
    switch (para->ItemIdx)
    {
    case 0:
      sprintf(para->ItemNamePtr, "Drv Cur[2,4,6,8]mA");
      switch (OV16825_sensor.eng.reg[OV16825_CMMCLK_CURRENT_INDEX].Para)
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
  case OV16825_FRAME_RATE_LIMITATION:
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
  case OV16825_REGISTER_EDITOR:
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

inline static kal_bool OV16825_set_sensor_item_info(MSDK_SENSOR_ITEM_INFO_STRUCT *para)
{
  kal_uint16 temp_para;
#ifdef OV16825_DRIVER_TRACE
   SENSORDB("OV16825_set_sensor_item_info\n");
#endif
  switch (para->GroupIdx)
  {
  case OV16825_PRE_GAIN:
    switch (para->ItemIdx)
    {
    case OV16825_SENSOR_BASEGAIN:
    case OV16825_PRE_GAIN_R_INDEX:
    case OV16825_PRE_GAIN_Gr_INDEX:
    case OV16825_PRE_GAIN_Gb_INDEX:
    case OV16825_PRE_GAIN_B_INDEX:
        spin_lock(&OV16825_drv_lock);
        OV16825_sensor.eng.cct[para->ItemIdx].Para = para->ItemValue * BASEGAIN / 1000;
        spin_unlock(&OV16825_drv_lock);

        OV16825_SetGain(OV16825_sensor.gain); /* update gain */
        break;
    default:
        ASSERT(0);
    }
    break;
  case OV16825_CMMCLK_CURRENT:
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
        //OV16825_set_isp_driving_current((kal_uint16)temp_para);
        spin_lock(&OV16825_drv_lock);
        OV16825_sensor.eng.reg[OV16825_CMMCLK_CURRENT_INDEX].Para = temp_para;
        spin_unlock(&OV16825_drv_lock);
      break;
    default:
      ASSERT(0);
    }
    break;
  case OV16825_FRAME_RATE_LIMITATION:
    ASSERT(0);
    break;
  case OV16825_REGISTER_EDITOR:
    switch (para->ItemIdx)
    {
      static kal_uint32 fac_sensor_reg;
    case 0:
      if (para->ItemValue < 0 || para->ItemValue > 0xFFFF) return KAL_FALSE;
      fac_sensor_reg = para->ItemValue;
      break;
    case 1:
      if (para->ItemValue < 0 || para->ItemValue > 0xFF) return KAL_FALSE;
      OV16825_write_cmos_sensor(fac_sensor_reg, para->ItemValue);
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

//static int Flag = 0;



static void OV16825MIPI_Sensor_Init(void)
{
	/*
	@@Initial - MIPI 4-Lane 4608x3456 10-bit 15fps_640Mbps_lane
	100 99 4608 3456
	100 98 1 1
	102 81 0 ffff
	102 84 1 ffff
	102 3601 5dc
	102 3f00 da2
	102 910 31
	*/
	
	//;Reset
	OV16825_write_cmos_sensor(0x0103, 0x01);
	
	//;delay 20ms
	mdelay(20);
	
	//;PLL
	OV16825_write_cmos_sensor(0x0300, 0x02);
	OV16825_write_cmos_sensor(0x0302, 0x50);//;64
	OV16825_write_cmos_sensor(0x0305, 0x01);
	OV16825_write_cmos_sensor(0x0306, 0x00);
	OV16825_write_cmos_sensor(0x030b, 0x02);
	OV16825_write_cmos_sensor(0x030c, 0x14);
	OV16825_write_cmos_sensor(0x030e, 0x00);
	OV16825_write_cmos_sensor(0x0313, 0x02);
	OV16825_write_cmos_sensor(0x0314, 0x14);
	OV16825_write_cmos_sensor(0x031f, 0x00);
	
	OV16825_write_cmos_sensor(0x3022, 0x01);
	OV16825_write_cmos_sensor(0x3032, 0x80);
	OV16825_write_cmos_sensor(0x3601, 0xf8);
	OV16825_write_cmos_sensor(0x3602, 0x00);
	OV16825_write_cmos_sensor(0x3605, 0x50);
	OV16825_write_cmos_sensor(0x3606, 0x00);
	OV16825_write_cmos_sensor(0x3607, 0x2b);
	OV16825_write_cmos_sensor(0x3608, 0x16);
	OV16825_write_cmos_sensor(0x3609, 0x00);
	OV16825_write_cmos_sensor(0x360e, 0x99);
	OV16825_write_cmos_sensor(0x360f, 0x75);
	OV16825_write_cmos_sensor(0x3610, 0x69);
	OV16825_write_cmos_sensor(0x3611, 0x59);
	OV16825_write_cmos_sensor(0x3612, 0x40);
	OV16825_write_cmos_sensor(0x3613, 0x89);
	OV16825_write_cmos_sensor(0x3615, 0x44);
	OV16825_write_cmos_sensor(0x3617, 0x00);
	OV16825_write_cmos_sensor(0x3618, 0x20);
	OV16825_write_cmos_sensor(0x3619, 0x00);
	OV16825_write_cmos_sensor(0x361a, 0x10);
	OV16825_write_cmos_sensor(0x361c, 0x10);
	OV16825_write_cmos_sensor(0x361d, 0x00);
	OV16825_write_cmos_sensor(0x361e, 0x00);
	OV16825_write_cmos_sensor(0x3640, 0x15);
	OV16825_write_cmos_sensor(0x3641, 0x54);
	OV16825_write_cmos_sensor(0x3642, 0x63);
	OV16825_write_cmos_sensor(0x3643, 0x32);
	OV16825_write_cmos_sensor(0x3644, 0x03);
	OV16825_write_cmos_sensor(0x3645, 0x04);
	OV16825_write_cmos_sensor(0x3646, 0x85);
	OV16825_write_cmos_sensor(0x364a, 0x07);
	OV16825_write_cmos_sensor(0x3707, 0x08);
	OV16825_write_cmos_sensor(0x3718, 0x75);
	OV16825_write_cmos_sensor(0x371a, 0x55);
	OV16825_write_cmos_sensor(0x371c, 0x55);
	OV16825_write_cmos_sensor(0x3733, 0x80);
	OV16825_write_cmos_sensor(0x3760, 0x00);
	OV16825_write_cmos_sensor(0x3761, 0x30);
	OV16825_write_cmos_sensor(0x3762, 0x00);
	OV16825_write_cmos_sensor(0x3763, 0xc0);
	OV16825_write_cmos_sensor(0x3764, 0x03);
	OV16825_write_cmos_sensor(0x3765, 0x00);
	
	OV16825_write_cmos_sensor(0x3823, 0x08);
	OV16825_write_cmos_sensor(0x3827, 0x02);
	OV16825_write_cmos_sensor(0x3828, 0x00);
	OV16825_write_cmos_sensor(0x3832, 0x00);
	OV16825_write_cmos_sensor(0x3833, 0x00);
	OV16825_write_cmos_sensor(0x3834, 0x00);
	OV16825_write_cmos_sensor(0x3d85, 0x17);
	OV16825_write_cmos_sensor(0x3d8c, 0x70);
	OV16825_write_cmos_sensor(0x3d8d, 0xa0);
	OV16825_write_cmos_sensor(0x3f00, 0x02);
	
	OV16825_write_cmos_sensor(0x4001, 0x83);
	OV16825_write_cmos_sensor(0x400e, 0x00);
	OV16825_write_cmos_sensor(0x4011, 0x00);
	OV16825_write_cmos_sensor(0x4012, 0x00);
	OV16825_write_cmos_sensor(0x4200, 0x08);
	OV16825_write_cmos_sensor(0x4302, 0x7f);
	OV16825_write_cmos_sensor(0x4303, 0xff);
	OV16825_write_cmos_sensor(0x4304, 0x00);
	OV16825_write_cmos_sensor(0x4305, 0x00);
	OV16825_write_cmos_sensor(0x4501, 0x30);
	OV16825_write_cmos_sensor(0x4603, 0x20);
	OV16825_write_cmos_sensor(0x4b00, 0x22);
	OV16825_write_cmos_sensor(0x4903, 0x00);
	OV16825_write_cmos_sensor(0x5000, 0x7f);
	OV16825_write_cmos_sensor(0x5001, 0x01);
	OV16825_write_cmos_sensor(0x5004, 0x00);
	OV16825_write_cmos_sensor(0x5013, 0x20);
	OV16825_write_cmos_sensor(0x5051, 0x00);
	OV16825_write_cmos_sensor(0x5500, 0x01);
	OV16825_write_cmos_sensor(0x5501, 0x00);
	OV16825_write_cmos_sensor(0x5502, 0x07);
	OV16825_write_cmos_sensor(0x5503, 0xff);
	OV16825_write_cmos_sensor(0x5505, 0x6c);
	OV16825_write_cmos_sensor(0x5509, 0x02);
	OV16825_write_cmos_sensor(0x5780, 0xfc);
	OV16825_write_cmos_sensor(0x5781, 0xff);
	OV16825_write_cmos_sensor(0x5787, 0x40);
	OV16825_write_cmos_sensor(0x5788, 0x08);
	OV16825_write_cmos_sensor(0x578a, 0x02);
	OV16825_write_cmos_sensor(0x578b, 0x01);
	OV16825_write_cmos_sensor(0x578c, 0x01);
	OV16825_write_cmos_sensor(0x578e, 0x02);
	OV16825_write_cmos_sensor(0x578f, 0x01);
	OV16825_write_cmos_sensor(0x5790, 0x01);
	OV16825_write_cmos_sensor(0x5792, 0x00);
	OV16825_write_cmos_sensor(0x5980, 0x00);
	OV16825_write_cmos_sensor(0x5981, 0x21);
	OV16825_write_cmos_sensor(0x5982, 0x00);
	OV16825_write_cmos_sensor(0x5983, 0x00);
	OV16825_write_cmos_sensor(0x5984, 0x00);
	OV16825_write_cmos_sensor(0x5985, 0x00);
	OV16825_write_cmos_sensor(0x5986, 0x00);
	OV16825_write_cmos_sensor(0x5987, 0x00);
	OV16825_write_cmos_sensor(0x5988, 0x00);
	
	//;Because current regsiter number in group hold is more than 85 (default), change group1 and group2 start address. Use group 0, 1, 2.
	OV16825_write_cmos_sensor(0x3201, 0x15);
	OV16825_write_cmos_sensor(0x3202, 0x2a);

	#if 1
	//;MIPI 4-Lane 4608x3456 10-bit 15fps setting
	
	//;don't change any PLL VCO in group hold
	OV16825_write_cmos_sensor(0x0305, 0x01);
	OV16825_write_cmos_sensor(0x030e, 0x01);
	
	OV16825_write_cmos_sensor(0x3018, 0x7a);
	OV16825_write_cmos_sensor(0x3031, 0x0a);
	OV16825_write_cmos_sensor(0x3603, 0x00);
	OV16825_write_cmos_sensor(0x3604, 0x00);
	OV16825_write_cmos_sensor(0x360a, 0x00);
	OV16825_write_cmos_sensor(0x360b, 0x02);
	OV16825_write_cmos_sensor(0x360c, 0x12);
	OV16825_write_cmos_sensor(0x360d, 0x00);
	OV16825_write_cmos_sensor(0x3614, 0x77);
	OV16825_write_cmos_sensor(0x3616, 0x30);
	OV16825_write_cmos_sensor(0x3631, 0x60);
	OV16825_write_cmos_sensor(0x3700, 0x30);
	OV16825_write_cmos_sensor(0x3701, 0x08);
	OV16825_write_cmos_sensor(0x3702, 0x11);
	OV16825_write_cmos_sensor(0x3703, 0x20);
	OV16825_write_cmos_sensor(0x3704, 0x08);
	OV16825_write_cmos_sensor(0x3705, 0x00);
	OV16825_write_cmos_sensor(0x3706, 0x84);
	OV16825_write_cmos_sensor(0x3708, 0x20);
	OV16825_write_cmos_sensor(0x3709, 0x3c);
	OV16825_write_cmos_sensor(0x370a, 0x01);
	OV16825_write_cmos_sensor(0x370b, 0x5d);
	OV16825_write_cmos_sensor(0x370c, 0x03);
	OV16825_write_cmos_sensor(0x370e, 0x20);
	OV16825_write_cmos_sensor(0x370f, 0x05);
	OV16825_write_cmos_sensor(0x3710, 0x20);
	OV16825_write_cmos_sensor(0x3711, 0x20);
	OV16825_write_cmos_sensor(0x3714, 0x31);
	OV16825_write_cmos_sensor(0x3719, 0x13);
	OV16825_write_cmos_sensor(0x371b, 0x03);
	OV16825_write_cmos_sensor(0x371d, 0x03);
	OV16825_write_cmos_sensor(0x371e, 0x09);
	OV16825_write_cmos_sensor(0x371f, 0x17);
	OV16825_write_cmos_sensor(0x3720, 0x0b);
	OV16825_write_cmos_sensor(0x3721, 0x18);
	OV16825_write_cmos_sensor(0x3722, 0x0b);
	OV16825_write_cmos_sensor(0x3723, 0x18);
	OV16825_write_cmos_sensor(0x3724, 0x04);
	OV16825_write_cmos_sensor(0x3725, 0x04);
	OV16825_write_cmos_sensor(0x3726, 0x02);
	OV16825_write_cmos_sensor(0x3727, 0x02);
	OV16825_write_cmos_sensor(0x3728, 0x02);
	OV16825_write_cmos_sensor(0x3729, 0x02);
	OV16825_write_cmos_sensor(0x372a, 0x25);
	OV16825_write_cmos_sensor(0x372b, 0x65);
	OV16825_write_cmos_sensor(0x372c, 0x55);
	OV16825_write_cmos_sensor(0x372d, 0x65);
	OV16825_write_cmos_sensor(0x372e, 0x53);
	OV16825_write_cmos_sensor(0x372f, 0x33);
	OV16825_write_cmos_sensor(0x3730, 0x33);
	OV16825_write_cmos_sensor(0x3731, 0x33);
	OV16825_write_cmos_sensor(0x3732, 0x03);
	OV16825_write_cmos_sensor(0x3734, 0x10);
	OV16825_write_cmos_sensor(0x3739, 0x03);
	OV16825_write_cmos_sensor(0x373a, 0x20);
	OV16825_write_cmos_sensor(0x373b, 0x0c);
	OV16825_write_cmos_sensor(0x373c, 0x1c);
	OV16825_write_cmos_sensor(0x373e, 0x0b);
	OV16825_write_cmos_sensor(0x373f, 0x80);
	
	OV16825_write_cmos_sensor(0x3800, 0x00);
	OV16825_write_cmos_sensor(0x3801, 0x20);
	OV16825_write_cmos_sensor(0x3802, 0x00);
	OV16825_write_cmos_sensor(0x3803, 0x0e);
	OV16825_write_cmos_sensor(0x3804, 0x12);
	OV16825_write_cmos_sensor(0x3805, 0x3f);
	OV16825_write_cmos_sensor(0x3806, 0x0d);
	OV16825_write_cmos_sensor(0x3807, 0x93);
	OV16825_write_cmos_sensor(0x3808, 0x12);
	OV16825_write_cmos_sensor(0x3809, 0x00);
	OV16825_write_cmos_sensor(0x380a, 0x0d);
	OV16825_write_cmos_sensor(0x380b, 0x80);
	OV16825_write_cmos_sensor(0x380c, 0x05);
	OV16825_write_cmos_sensor(0x380d, 0xf8);
	OV16825_write_cmos_sensor(0x380e, 0x0d);
	OV16825_write_cmos_sensor(0x380f, 0xa2);
	OV16825_write_cmos_sensor(0x3811, 0x0f);
	OV16825_write_cmos_sensor(0x3813, 0x02);
	OV16825_write_cmos_sensor(0x3814, 0x01);
	OV16825_write_cmos_sensor(0x3815, 0x01);
	OV16825_write_cmos_sensor(0x3820, 0x00);
	OV16825_write_cmos_sensor(0x3821, 0x06);
	OV16825_write_cmos_sensor(0x3829, 0x00);
	OV16825_write_cmos_sensor(0x382a, 0x01);
	OV16825_write_cmos_sensor(0x382b, 0x01);
	OV16825_write_cmos_sensor(0x3830, 0x08);
	
	OV16825_write_cmos_sensor(0x3f08, 0x20);
	OV16825_write_cmos_sensor(0x4000, 0xf1);   // add for BCL trigger setting
	OV16825_write_cmos_sensor(0x4002, 0x04);
	OV16825_write_cmos_sensor(0x4003, 0x08);
	OV16825_write_cmos_sensor(0x4837, 0x14);
	
	OV16825_write_cmos_sensor(0x3501, 0xd9);
	OV16825_write_cmos_sensor(0x3502, 0xe0);
	OV16825_write_cmos_sensor(0x3508, 0x04);
	OV16825_write_cmos_sensor(0x3509, 0xff);
	
	OV16825_write_cmos_sensor(0x3638, 0x00); //;activate 36xx

	#endif
	
	//OV16825_write_cmos_sensor(0x3503, 0x00); //bit2,1:sensor gain, 0: real gain

	OV16825_write_cmos_sensor(0x0100, 0x01);

	mdelay(40);


}   /*  OV16825MIPI_Sensor_Init  */   /*  OV16825MIPI_Sensor_Init  */
static void OV16825MIPI_Sensor_1080P(void) 
{
	SENSORDB("OV16825MIPI 1080P Setting \n");

	/*
	@@MIPI 4-Lane 1920x1080 10-bit VHbinning2 30fps 640Mbps/lane
	;;PCLK=HTS*VTS*fps=0x4b6*0x8a4*30=1206*2212*30=80M
	100 99 1920 1080
	100 98 1 1
	102 81 0 ffff
	102 84 1 ffff
	102 3601 1770
	102 3f00 452
	*/
	
	//;Sensor Setting
	//;group 0
	OV16825_write_cmos_sensor(0x3208, 0x00);
	
	OV16825_write_cmos_sensor(0x301a, 0xfb);
	
	//;don't change any PLL VCO in group hold
	OV16825_write_cmos_sensor(0x0305, 0x01);
	OV16825_write_cmos_sensor(0x030e, 0x01);
				   
	OV16825_write_cmos_sensor(0x3018, 0x7a);
	OV16825_write_cmos_sensor(0x3031, 0x0a);
	OV16825_write_cmos_sensor(0x3603, 0x05);
	OV16825_write_cmos_sensor(0x3604, 0x02);
	OV16825_write_cmos_sensor(0x360a, 0x00);
	OV16825_write_cmos_sensor(0x360b, 0x02);
	OV16825_write_cmos_sensor(0x360c, 0x12);
	OV16825_write_cmos_sensor(0x360d, 0x04);
	OV16825_write_cmos_sensor(0x3614, 0x77);
	OV16825_write_cmos_sensor(0x3616, 0x30);
	OV16825_write_cmos_sensor(0x3631, 0x40);
	OV16825_write_cmos_sensor(0x3700, 0x30);
	OV16825_write_cmos_sensor(0x3701, 0x08);
	OV16825_write_cmos_sensor(0x3702, 0x11);
	OV16825_write_cmos_sensor(0x3703, 0x20);
	OV16825_write_cmos_sensor(0x3704, 0x08);
	OV16825_write_cmos_sensor(0x3705, 0x00);
	OV16825_write_cmos_sensor(0x3706, 0x84);
	OV16825_write_cmos_sensor(0x3708, 0x20);
	OV16825_write_cmos_sensor(0x3709, 0x3c);
	OV16825_write_cmos_sensor(0x370a, 0x01);
	OV16825_write_cmos_sensor(0x370b, 0x5d);
	OV16825_write_cmos_sensor(0x370c, 0x03);
	OV16825_write_cmos_sensor(0x370e, 0x20);
	OV16825_write_cmos_sensor(0x370f, 0x05);
	OV16825_write_cmos_sensor(0x3710, 0x20);
	OV16825_write_cmos_sensor(0x3711, 0x20);
	OV16825_write_cmos_sensor(0x3714, 0x31);
	OV16825_write_cmos_sensor(0x3719, 0x13);
	OV16825_write_cmos_sensor(0x371b, 0x03);
	OV16825_write_cmos_sensor(0x371d, 0x03);
	OV16825_write_cmos_sensor(0x371e, 0x09);
	OV16825_write_cmos_sensor(0x371f, 0x17);
	OV16825_write_cmos_sensor(0x3720, 0x0b);
	OV16825_write_cmos_sensor(0x3721, 0x18);
	OV16825_write_cmos_sensor(0x3722, 0x0b);
	OV16825_write_cmos_sensor(0x3723, 0x18);
	OV16825_write_cmos_sensor(0x3724, 0x04);
	OV16825_write_cmos_sensor(0x3725, 0x04);
	OV16825_write_cmos_sensor(0x3726, 0x02);
	OV16825_write_cmos_sensor(0x3727, 0x02);
	OV16825_write_cmos_sensor(0x3728, 0x02);
	OV16825_write_cmos_sensor(0x3729, 0x02);
	OV16825_write_cmos_sensor(0x372a, 0x25);
	OV16825_write_cmos_sensor(0x372b, 0x65);
	OV16825_write_cmos_sensor(0x372c, 0x55);
	OV16825_write_cmos_sensor(0x372d, 0x65);
	OV16825_write_cmos_sensor(0x372e, 0x53);
	OV16825_write_cmos_sensor(0x372f, 0x33);
	OV16825_write_cmos_sensor(0x3730, 0x33);
	OV16825_write_cmos_sensor(0x3731, 0x33);
	OV16825_write_cmos_sensor(0x3732, 0x03);
	OV16825_write_cmos_sensor(0x3734, 0x10);
	OV16825_write_cmos_sensor(0x3739, 0x03);
	OV16825_write_cmos_sensor(0x373a, 0x20);
	OV16825_write_cmos_sensor(0x373b, 0x0c);
	OV16825_write_cmos_sensor(0x373c, 0x1c);
	OV16825_write_cmos_sensor(0x373e, 0x0b);
	OV16825_write_cmos_sensor(0x373f, 0x80);
	
	OV16825_write_cmos_sensor(0x3800, 0x01);
	OV16825_write_cmos_sensor(0x3801, 0x80);
	OV16825_write_cmos_sensor(0x3802, 0x02);
	OV16825_write_cmos_sensor(0x3803, 0x94);
	OV16825_write_cmos_sensor(0x3804, 0x10);
	OV16825_write_cmos_sensor(0x3805, 0xbf);
	OV16825_write_cmos_sensor(0x3806, 0x0b);
	OV16825_write_cmos_sensor(0x3807, 0x0f);
	OV16825_write_cmos_sensor(0x3808, 0x07);
	OV16825_write_cmos_sensor(0x3809, 0x80);
	OV16825_write_cmos_sensor(0x380a, 0x04);
	OV16825_write_cmos_sensor(0x380b, 0x38);
	OV16825_write_cmos_sensor(0x380c, 0x04);
	OV16825_write_cmos_sensor(0x380d, 0xb6);
	OV16825_write_cmos_sensor(0x380e, 0x08);//;04
	OV16825_write_cmos_sensor(0x380f, 0xa4);//;52
	OV16825_write_cmos_sensor(0x3811, 0x17);
	OV16825_write_cmos_sensor(0x3813, 0x02);
	OV16825_write_cmos_sensor(0x3814, 0x03);
	OV16825_write_cmos_sensor(0x3815, 0x01);
	OV16825_write_cmos_sensor(0x3820, 0x01);
	OV16825_write_cmos_sensor(0x3821, 0x07);
	OV16825_write_cmos_sensor(0x3829, 0x02);
	OV16825_write_cmos_sensor(0x382a, 0x03);
	OV16825_write_cmos_sensor(0x382b, 0x01);
	OV16825_write_cmos_sensor(0x3830, 0x08);
	
	OV16825_write_cmos_sensor(0x3f08, 0x20);
	OV16825_write_cmos_sensor(0x4002, 0x02);
	OV16825_write_cmos_sensor(0x4003, 0x04);
	OV16825_write_cmos_sensor(0x4837, 0x14);
	
	OV16825_write_cmos_sensor(0x3501, 0x44);
	OV16825_write_cmos_sensor(0x3502, 0xe0);
	OV16825_write_cmos_sensor(0x3508, 0x08);
	OV16825_write_cmos_sensor(0x3509, 0xff);
	
	OV16825_write_cmos_sensor(0x3638, 0x00); //;activate 36xx
	
	OV16825_write_cmos_sensor(0x301a, 0xf0);
	
	OV16825_write_cmos_sensor(0x3208, 0x10);
	OV16825_write_cmos_sensor(0x3208, 0xa0);
	mdelay(30);


}

static void OV16825MIPI_Sensor_4M(void)
{
	//-------------------------------------------------------------------------------
	// PLL MY_OUTPUT clock(fclk)
	// fclk = (0x40 - 0x300E[5:0]) x N x Bit8Div x MCLK / M, where
	//		N = 1, 1.5, 2, 3 for 0x300F[7:6] = 0~3, respectively
	//		M = 1, 1.5, 2, 3 for 0x300F[1:0] = 0~3, respectively
	//		Bit8Div = 1, 1, 4, 5 for 0x300F[5:4] = 0~3, respectively
	// Sys Clk = fclk / Bit8Div / SenDiv
	// Sensor MY_OUTPUT clock(DVP PCLK)
	// DVP PCLK = ISP CLK / DVPDiv, where
	//		ISP CLK =  fclk / Bit8Div / SenDiv / CLKDiv / 2, where
	//			Bit8Div = 1, 1, 4, 5 for 0x300F[5:4] = 0~3, respectively
	//			SenDiv = 1, 2 for 0x3010[4] = 0 or 1 repectively
	//			CLKDiv = (0x3011[5:0] + 1)
	//		DVPDiv = 0x304C[3:0] * (2 ^ 0x304C[4]), if 0x304C[3:0] = 0, use 16 instead
	//
	// Base shutter calculation
	//		60Hz: (1/120) * ISP Clk / QXGA_MODE_WITHOUT_DUMMY_PIXELS
	//		50Hz: (1/100) * ISP Clk / QXGA_MODE_WITHOUT_DUMMY_PIXELS
	//-------------------------------------------------------------------------------
	
	
	SENSORDB("OV16825MIPIPreview Setting \n");
	/*
	@@MIPI 4-Lane 2304x1728 10-bit VHbinning2 30fps 640Mbps/lane
	;;PCLK=HTS*VTS*fps=0x5f0*0x6da*30=1520*1754*30=80M
	100 99 2304 1728
	100 98 1 1
	102 81 0 ffff
	102 84 1 ffff
	102 3601 bb8
	102 3f00 6da
	
	; 2304x1728 4 lane	setting
	; Mipi: 4 lane
	; width		:2304 (0x900)
	; height		:1728 (0x6c0) 
	HTS = 0x5f0* 4 = 0x17c0 = 6080
	VTS = 0x6da = 1754
	
	*/
	//;Sensor Setting
	//;group 0
	
    OV16825_write_cmos_sensor(0x0100, 0x00);
	
	OV16825_write_cmos_sensor(0x3208, 0x00);
	
	OV16825_write_cmos_sensor(0x301a, 0xfb);
	
	//;don't change any PLL VCO in group hold
	OV16825_write_cmos_sensor(0x0305, 0x01);
	OV16825_write_cmos_sensor(0x030e, 0x01);
	
	OV16825_write_cmos_sensor(0x3018, 0x7a);
	OV16825_write_cmos_sensor(0x3031, 0x0a);
	OV16825_write_cmos_sensor(0x3603, 0x05);
	OV16825_write_cmos_sensor(0x3604, 0x02);
	OV16825_write_cmos_sensor(0x360a, 0x00);
	OV16825_write_cmos_sensor(0x360b, 0x02);
	OV16825_write_cmos_sensor(0x360c, 0x12);
	OV16825_write_cmos_sensor(0x360d, 0x04);
	OV16825_write_cmos_sensor(0x3614, 0x77);
	OV16825_write_cmos_sensor(0x3616, 0x30);
	OV16825_write_cmos_sensor(0x3631, 0x40);
	OV16825_write_cmos_sensor(0x3700, 0x30);
	OV16825_write_cmos_sensor(0x3701, 0x08);
	OV16825_write_cmos_sensor(0x3702, 0x11);
	OV16825_write_cmos_sensor(0x3703, 0x20);
	OV16825_write_cmos_sensor(0x3704, 0x08);
	OV16825_write_cmos_sensor(0x3705, 0x00);
	OV16825_write_cmos_sensor(0x3706, 0x84);
	OV16825_write_cmos_sensor(0x3708, 0x20);
	OV16825_write_cmos_sensor(0x3709, 0x3c);
	OV16825_write_cmos_sensor(0x370a, 0x01);
	OV16825_write_cmos_sensor(0x370b, 0x5d);
	OV16825_write_cmos_sensor(0x370c, 0x03);
	OV16825_write_cmos_sensor(0x370e, 0x20);
	OV16825_write_cmos_sensor(0x370f, 0x05);
	OV16825_write_cmos_sensor(0x3710, 0x20);
	OV16825_write_cmos_sensor(0x3711, 0x20);
	OV16825_write_cmos_sensor(0x3714, 0x31);
	OV16825_write_cmos_sensor(0x3719, 0x13);
	OV16825_write_cmos_sensor(0x371b, 0x03);
	OV16825_write_cmos_sensor(0x371d, 0x03);
	OV16825_write_cmos_sensor(0x371e, 0x09);
	OV16825_write_cmos_sensor(0x371f, 0x17);
	OV16825_write_cmos_sensor(0x3720, 0x0b);
	OV16825_write_cmos_sensor(0x3721, 0x18);
	OV16825_write_cmos_sensor(0x3722, 0x0b);
	OV16825_write_cmos_sensor(0x3723, 0x18);
	OV16825_write_cmos_sensor(0x3724, 0x04);
	OV16825_write_cmos_sensor(0x3725, 0x04);
	OV16825_write_cmos_sensor(0x3726, 0x02);
	OV16825_write_cmos_sensor(0x3727, 0x02);
	OV16825_write_cmos_sensor(0x3728, 0x02);
	OV16825_write_cmos_sensor(0x3729, 0x02);
	OV16825_write_cmos_sensor(0x372a, 0x25);
	OV16825_write_cmos_sensor(0x372b, 0x65);
	OV16825_write_cmos_sensor(0x372c, 0x55);
	OV16825_write_cmos_sensor(0x372d, 0x65);
	OV16825_write_cmos_sensor(0x372e, 0x53);
	OV16825_write_cmos_sensor(0x372f, 0x33);
	OV16825_write_cmos_sensor(0x3730, 0x33);
	OV16825_write_cmos_sensor(0x3731, 0x33);
	OV16825_write_cmos_sensor(0x3732, 0x03);
	OV16825_write_cmos_sensor(0x3734, 0x10);
	OV16825_write_cmos_sensor(0x3739, 0x03);
	OV16825_write_cmos_sensor(0x373a, 0x20);
	OV16825_write_cmos_sensor(0x373b, 0x0c);
	OV16825_write_cmos_sensor(0x373c, 0x1c);
	OV16825_write_cmos_sensor(0x373e, 0x0b);
	OV16825_write_cmos_sensor(0x373f, 0x80);
	
	OV16825_write_cmos_sensor(0x3800, 0x00);
	OV16825_write_cmos_sensor(0x3801, 0x00);
	OV16825_write_cmos_sensor(0x3802, 0x00);
	OV16825_write_cmos_sensor(0x3803, 0x0c);
	OV16825_write_cmos_sensor(0x3804, 0x12);
	OV16825_write_cmos_sensor(0x3805, 0x3f);
	OV16825_write_cmos_sensor(0x3806, 0x0d);
	OV16825_write_cmos_sensor(0x3807, 0x97);
	OV16825_write_cmos_sensor(0x3808, 0x09);
	OV16825_write_cmos_sensor(0x3809, 0x00);
	OV16825_write_cmos_sensor(0x380a, 0x06);
	OV16825_write_cmos_sensor(0x380b, 0xc0);
	OV16825_write_cmos_sensor(0x380c, 0x05);
	OV16825_write_cmos_sensor(0x380d, 0xf0);
	OV16825_write_cmos_sensor(0x380e, 0x06);
	OV16825_write_cmos_sensor(0x380f, 0xda);
	OV16825_write_cmos_sensor(0x3811, 0x17);
	OV16825_write_cmos_sensor(0x3813, 0x02);
	OV16825_write_cmos_sensor(0x3814, 0x03);
	OV16825_write_cmos_sensor(0x3815, 0x01);
	OV16825_write_cmos_sensor(0x3820, 0x01);
	OV16825_write_cmos_sensor(0x3821, 0x07);
	OV16825_write_cmos_sensor(0x3829, 0x02);
	OV16825_write_cmos_sensor(0x382a, 0x03);
	OV16825_write_cmos_sensor(0x382b, 0x01);
	OV16825_write_cmos_sensor(0x3830, 0x08);
	
	OV16825_write_cmos_sensor(0x3f08, 0x20);
	OV16825_write_cmos_sensor(0x4002, 0x02);
	OV16825_write_cmos_sensor(0x4003, 0x04);
	OV16825_write_cmos_sensor(0x4837, 0x14);
	
	OV16825_write_cmos_sensor(0x3501, 0x6d);
	OV16825_write_cmos_sensor(0x3502, 0x60);
	OV16825_write_cmos_sensor(0x3508, 0x08);
	OV16825_write_cmos_sensor(0x3509, 0xff);
	
	OV16825_write_cmos_sensor(0x3638, 0x00); //;activate 36xx
	
	OV16825_write_cmos_sensor(0x301a, 0xf0);
	
	OV16825_write_cmos_sensor(0x3208, 0x10);
	OV16825_write_cmos_sensor(0x3208, 0xa0);

	
    OV16825_write_cmos_sensor(0x0100, 0x01);
	
    //OV16825_write_cmos_sensor(0x4800, 0x14);
	//OV16825_write_cmos_sensor(0x5040, 0x80);

mdelay(60);

}


static void OV16825MIPI_Sensor_16M(void)
{
    SENSORDB("OV16825MIPICapture Setting\n");

  
/*
;----------------------------------------------
; 2592x1944_2lane setting
; Mipi: 2 lane
: Mipi data rate: 280Mbps/lane 
; SystemCLK     :56Mhz
; FPS	        :7.26
; HTS		:3468 (R380c:R380d) 
; VTS		:2224 (R380e:R380f)
; Tline 	:61.93us
;---------------------------------------------
*/
		
/*
@@MIPI 4-Lane 4608x3456 10-bit 15fps 640Mbps/lane
;;PCLK=HTS*VTS*fps=0x5f8*0xda2*15=1528*3490*15=80M
100 99 4608 3456
100 98 1 1
102 81 0 ffff
102 84 1 ffff
102 3601 5dc
102 3f00 da2

HTS = 0x5f8 *4 = 0x17E0 = 6112
VTS = 0xda2 = 3490

*/
	//;Sensor Setting
	//;group 0
	OV16825_write_cmos_sensor(0x0100, 0x00);
	OV16825_write_cmos_sensor(0x3208, 0x00);

	OV16825_write_cmos_sensor(0x301a, 0xfb);

	//;don't change any PLL VCO in group hold
	OV16825_write_cmos_sensor(0x0305, 0x01);
	OV16825_write_cmos_sensor(0x030e, 0x01);

	OV16825_write_cmos_sensor(0x3018, 0x7a);
	OV16825_write_cmos_sensor(0x3031, 0x0a);
	OV16825_write_cmos_sensor(0x3603, 0x00);
	OV16825_write_cmos_sensor(0x3604, 0x00);
	OV16825_write_cmos_sensor(0x360a, 0x00);
	OV16825_write_cmos_sensor(0x360b, 0x02);
	OV16825_write_cmos_sensor(0x360c, 0x12);
	OV16825_write_cmos_sensor(0x360d, 0x00);
	OV16825_write_cmos_sensor(0x3614, 0x77);
	OV16825_write_cmos_sensor(0x3616, 0x30);
	OV16825_write_cmos_sensor(0x3631, 0x60);
	OV16825_write_cmos_sensor(0x3700, 0x30);
	OV16825_write_cmos_sensor(0x3701, 0x08);
	OV16825_write_cmos_sensor(0x3702, 0x11);
	OV16825_write_cmos_sensor(0x3703, 0x20);
	OV16825_write_cmos_sensor(0x3704, 0x08);
	OV16825_write_cmos_sensor(0x3705, 0x00);
	OV16825_write_cmos_sensor(0x3706, 0x84);
	OV16825_write_cmos_sensor(0x3708, 0x20);
	OV16825_write_cmos_sensor(0x3709, 0x3c);
	OV16825_write_cmos_sensor(0x370a, 0x01);
	OV16825_write_cmos_sensor(0x370b, 0x5d);
	OV16825_write_cmos_sensor(0x370c, 0x03);
	OV16825_write_cmos_sensor(0x370e, 0x20);
	OV16825_write_cmos_sensor(0x370f, 0x05);
	OV16825_write_cmos_sensor(0x3710, 0x20);
	OV16825_write_cmos_sensor(0x3711, 0x20);
	OV16825_write_cmos_sensor(0x3714, 0x31);
	OV16825_write_cmos_sensor(0x3719, 0x13);
	OV16825_write_cmos_sensor(0x371b, 0x03);
	OV16825_write_cmos_sensor(0x371d, 0x03);
	OV16825_write_cmos_sensor(0x371e, 0x09);
	OV16825_write_cmos_sensor(0x371f, 0x17);
	OV16825_write_cmos_sensor(0x3720, 0x0b);
	OV16825_write_cmos_sensor(0x3721, 0x18);
	OV16825_write_cmos_sensor(0x3722, 0x0b);
	OV16825_write_cmos_sensor(0x3723, 0x18);
	OV16825_write_cmos_sensor(0x3724, 0x04);
	OV16825_write_cmos_sensor(0x3725, 0x04);
	OV16825_write_cmos_sensor(0x3726, 0x02);
	OV16825_write_cmos_sensor(0x3727, 0x02);
	OV16825_write_cmos_sensor(0x3728, 0x02);
	OV16825_write_cmos_sensor(0x3729, 0x02);
	OV16825_write_cmos_sensor(0x372a, 0x25);
	OV16825_write_cmos_sensor(0x372b, 0x65);
	OV16825_write_cmos_sensor(0x372c, 0x55);
	OV16825_write_cmos_sensor(0x372d, 0x65);
	OV16825_write_cmos_sensor(0x372e, 0x53);
	OV16825_write_cmos_sensor(0x372f, 0x33);
	OV16825_write_cmos_sensor(0x3730, 0x33);
	OV16825_write_cmos_sensor(0x3731, 0x33);
	OV16825_write_cmos_sensor(0x3732, 0x03);
	OV16825_write_cmos_sensor(0x3734, 0x10);
	OV16825_write_cmos_sensor(0x3739, 0x03);
	OV16825_write_cmos_sensor(0x373a, 0x20);
	OV16825_write_cmos_sensor(0x373b, 0x0c);
	OV16825_write_cmos_sensor(0x373c, 0x1c);
	OV16825_write_cmos_sensor(0x373e, 0x0b);
	OV16825_write_cmos_sensor(0x373f, 0x80);

	OV16825_write_cmos_sensor(0x3800, 0x00);
	OV16825_write_cmos_sensor(0x3801, 0x20);
	OV16825_write_cmos_sensor(0x3802, 0x00);
	OV16825_write_cmos_sensor(0x3803, 0x0e);
	OV16825_write_cmos_sensor(0x3804, 0x12);
	OV16825_write_cmos_sensor(0x3805, 0x3f);
	OV16825_write_cmos_sensor(0x3806, 0x0d);
	OV16825_write_cmos_sensor(0x3807, 0x93);
	OV16825_write_cmos_sensor(0x3808, 0x12);
	OV16825_write_cmos_sensor(0x3809, 0x00);
	OV16825_write_cmos_sensor(0x380a, 0x0d);
	OV16825_write_cmos_sensor(0x380b, 0x80);
	OV16825_write_cmos_sensor(0x380c, 0x05);
	OV16825_write_cmos_sensor(0x380d, 0xf8);
	OV16825_write_cmos_sensor(0x380e, 0x0d);
	OV16825_write_cmos_sensor(0x380f, 0xa2);
	OV16825_write_cmos_sensor(0x3811, 0x0f);
	OV16825_write_cmos_sensor(0x3813, 0x02);
	OV16825_write_cmos_sensor(0x3814, 0x01);
	OV16825_write_cmos_sensor(0x3815, 0x01);
	OV16825_write_cmos_sensor(0x3820, 0x00);
	OV16825_write_cmos_sensor(0x3821, 0x06);
	OV16825_write_cmos_sensor(0x3829, 0x00);
	OV16825_write_cmos_sensor(0x382a, 0x01);
	OV16825_write_cmos_sensor(0x382b, 0x01);
	OV16825_write_cmos_sensor(0x3830, 0x08);

	OV16825_write_cmos_sensor(0x3f08, 0x20);
	OV16825_write_cmos_sensor(0x4002, 0x04);
	OV16825_write_cmos_sensor(0x4003, 0x08);
	OV16825_write_cmos_sensor(0x4837, 0x14);

	OV16825_write_cmos_sensor(0x3501, 0xd9);
	OV16825_write_cmos_sensor(0x3502, 0xe0);
	OV16825_write_cmos_sensor(0x3508, 0x04);
	OV16825_write_cmos_sensor(0x3509, 0xff);

	OV16825_write_cmos_sensor(0x3638, 0x00); //;activate 36xx

	OV16825_write_cmos_sensor(0x301a, 0xf0);

	OV16825_write_cmos_sensor(0x3208, 0x10);
	OV16825_write_cmos_sensor(0x3208, 0xa0);	
	
	OV16825_write_cmos_sensor(0x0100, 0x01);
	mdelay(50);
}

UINT32 OV16825Open(void)
{
    kal_uint32 sensor_id=0; 
    int i;
    const kal_uint16 sccb_writeid[] = {OV16825_SLAVE_WRITE_ID_1,OV16825_SLAVE_WRITE_ID_2};
	SENSORDB("OV16825Open\n");

   spin_lock(&OV16825_drv_lock);
   OV16825_sensor.is_zsd = KAL_FALSE;  //for zsd full size preview
   OV16825_sensor.is_zsd_cap = KAL_FALSE;
   OV16825_sensor.is_autofliker = KAL_FALSE; //for autofliker.
   OV16825_sensor.pv_mode = KAL_TRUE;
   OV16825_sensor.pclk = OV16825_PREVIEW_CLK;
   spin_unlock(&OV16825_drv_lock);
   
  for(i = 0; i <(sizeof(sccb_writeid)/sizeof(sccb_writeid[0])); i++)
    {
        spin_lock(&OV16825_drv_lock);
        OV16825_sensor.write_id = sccb_writeid[i];
        OV16825_sensor.read_id = (sccb_writeid[i]|0x01);
        spin_unlock(&OV16825_drv_lock);

        sensor_id=((OV16825_read_cmos_sensor(0x300A) << 8) | OV16825_read_cmos_sensor(0x300B));

#ifdef OV16825_DRIVER_TRACE
        SENSORDB("OV16825Open, sensor_id:%x \n",sensor_id);
#endif
        if(OV16825MIPI_SENSOR_ID == sensor_id)
        {
            SENSORDB("OV16825 slave write id:%x \n",OV16825_sensor.write_id);
            break;
        }
    }
  
    // check if sensor ID correct
    if (sensor_id != OV16825MIPI_SENSOR_ID) 
    {
        SENSORDB("OV16825 Check ID fails! \n");
		
		SENSORDB("[Warning]OV16825GetSensorID, sensor_id:%x \n",sensor_id);
		//sensor_id = OV16825_SENSOR_ID;
        sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
    }

	//OV16825_global_setting();
	OV16825MIPI_Sensor_Init();
	//OV16825_Read_OTP();
#if defined(OV16825_USE_WB_OTP)
    update_otp_wb();
#endif

    SENSORDB("test for bootimage \n");

   return ERROR_NONE;
}   /* OV16825Open  */

/*************************************************************************
* FUNCTION
*   OV5642GetSensorID
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
UINT32 OV16825GetSensorID(UINT32 *sensorID) 
{
  //added by mandrave
   int i;
   const kal_uint16 sccb_writeid[] = {OV16825_SLAVE_WRITE_ID_1, OV16825_SLAVE_WRITE_ID_2};
 
 SENSORDB("OV16825GetSensorID enter,\n");
    for(i = 0; i <(sizeof(sccb_writeid)/sizeof(sccb_writeid[0])); i++)
    {
        spin_lock(&OV16825_drv_lock);
        OV16825_sensor.write_id = sccb_writeid[i];
        OV16825_sensor.read_id = (sccb_writeid[i]|0x01);
        spin_unlock(&OV16825_drv_lock);

        *sensorID=((OV16825_read_cmos_sensor(0x300A) << 8) | OV16825_read_cmos_sensor(0x300B));	

#ifdef OV16825_DRIVER_TRACE
        SENSORDB("OV16825GetSensorID, sensor_id:%x \n",*sensorID);
#endif
        if(OV16825MIPI_SENSOR_ID == *sensorID)
        {
            SENSORDB("OV16825 slave write id:%x \n",OV16825_sensor.write_id);
            break;
        }
    }

    // check if sensor ID correct		
    if (*sensorID != OV16825MIPI_SENSOR_ID) 
    {
        	SENSORDB("[Warning]OV16825GetSensorID, sensor_id:%x \n",*sensorID);
			*sensorID = 0xFFFFFFFF;
			return ERROR_SENSOR_CONNECT_FAIL;
    }
	SENSORDB("OV16825GetSensorID exit,\n");
   return ERROR_NONE;
}

/*************************************************************************
* FUNCTION
*	OV16825Close
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
UINT32 OV16825Close(void)
{
#ifdef OV16825_DRIVER_TRACE
   SENSORDB("OV16825Close\n");
#endif

    return ERROR_NONE;
}   /* OV16825Close */

/*************************************************************************
* FUNCTION
* OV16825Preview
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
UINT32 OV16825Preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
        MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint16 dummy_line;
#ifdef OV16825_DRIVER_TRACE
    SENSORDB("OV16825Preview \n");
#endif
	//OV16825_1632_1224_30fps_Mclk24M_setting();
	OV16825MIPI_Sensor_4M();


    //msleep(10);
    spin_lock(&OV16825_drv_lock);
    OV16825_sensor.pv_mode = KAL_TRUE;
    spin_unlock(&OV16825_drv_lock);

    spin_lock(&OV16825_drv_lock);
    OV16825_sensor.video_mode = KAL_FALSE;
    spin_unlock(&OV16825_drv_lock);
    dummy_line = 0;

    spin_lock(&OV16825_drv_lock);
    OV16825_sensor.dummy_pixels = 0;
    OV16825_sensor.dummy_lines = 0;
    OV16825_sensor.line_length = OV16825_PV_PERIOD_PIXEL_NUMS;
    OV16825_sensor.frame_height = OV16825_PV_PERIOD_LINE_NUMS + dummy_line;
    OV16825_sensor.pclk = OV16825_PREVIEW_CLK;
    spin_unlock(&OV16825_drv_lock);

    OV16825_Set_Dummy(0, dummy_line); /* modify dummy_pixel must gen AE table again */
    //OV16825_Write_Shutter(OV16825_sensor.shutter);

    //mdelay(100);

    return ERROR_NONE;

}   /*  OV16825Preview   */


/*************************************************************************
* FUNCTION
* OV16825VIDEO
*
* DESCRIPTION
*	This function start the sensor Video preview.
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

UINT32 OV16825VIDEO(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
        MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    kal_uint16 dummy_line;
    //kal_uint16 ret;
#ifdef OV16825_DRIVER_TRACE
    SENSORDB("OV16825VIDEO \n");
#endif
	 //return ERROR_NONE;

	OV16825MIPI_Sensor_4M();

    spin_lock(&OV16825_drv_lock);
    OV16825_sensor.pv_mode = KAL_FALSE;
    spin_unlock(&OV16825_drv_lock);

    spin_lock(&OV16825_drv_lock);
    OV16825_sensor.video_mode = KAL_TRUE;
    spin_unlock(&OV16825_drv_lock);
    dummy_line = 0;

    spin_lock(&OV16825_drv_lock);
    OV16825_sensor.dummy_pixels = 0;
    OV16825_sensor.dummy_lines = 0;
    OV16825_sensor.line_length = OV16825_VIDEO_PERIOD_PIXEL_NUMS;
    OV16825_sensor.frame_height = OV16825_VIDEO_PERIOD_LINE_NUMS+ dummy_line;
    OV16825_sensor.pclk = OV16825_VIDEO_CLK;
    spin_unlock(&OV16825_drv_lock);

    OV16825_Set_Dummy(0, dummy_line); /* modify dummy_pixel must gen AE table again */
    //mdelay(40);

    return ERROR_NONE;
	
}   /*  OV16825VIDEO   */


/*************************************************************************
* FUNCTION
*    OV16825ZsdPreview
*
* DESCRIPTION
*    This function setup the CMOS sensor in Full Size output  mode
*
* PARAMETERS
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 OV16825ZsdPreview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
    MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)

{
    //kal_uint16 dummy_pixel = 0;
    kal_uint16 dummy_line = 0;
    kal_uint16 ret;
#ifdef OV16825_DRIVER_TRACE
    SENSORDB("OV16825ZsdPreview \n");
#endif


	OV16825MIPI_Sensor_16M();

    spin_lock(&OV16825_drv_lock);
    OV16825_sensor.pv_mode = KAL_FALSE;
    spin_unlock(&OV16825_drv_lock);


    spin_lock(&OV16825_drv_lock);
    OV16825_sensor.video_mode = KAL_FALSE;
    spin_unlock(&OV16825_drv_lock);
    dummy_line = 0;

    spin_lock(&OV16825_drv_lock);
    OV16825_sensor.dummy_pixels = 0;
    OV16825_sensor.dummy_lines = 0;
    OV16825_sensor.line_length = OV16825_FULL_PERIOD_PIXEL_NUMS;
    OV16825_sensor.frame_height = OV16825_FULL_PERIOD_LINE_NUMS+ dummy_line;
    OV16825_sensor.pclk = OV16825_ZSD_PRE_CLK;
    spin_unlock(&OV16825_drv_lock);

    OV16825_Set_Dummy(0, dummy_line); /* modify dummy_pixel must gen AE table again */
    mdelay(10);

    return ERROR_NONE;
}



/*************************************************************************
* FUNCTION
*OV16825Capture
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
UINT32 OV16825Capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
        MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    kal_uint16 dummy_pixel = 0;
    kal_uint16 dummy_line = 0;
    kal_uint16 ret;
#ifdef OV16825_DRIVER_TRACE
    SENSORDB("OV16825Capture start \n");
#endif


    spin_lock(&OV16825_drv_lock);
    OV16825_sensor.video_mode = KAL_FALSE;
    OV16825_sensor.is_autofliker = KAL_FALSE;
    OV16825_sensor.pv_mode = KAL_FALSE;
    spin_unlock(&OV16825_drv_lock);

	OV16825MIPI_Sensor_16M();


    spin_lock(&OV16825_drv_lock);
    OV16825_sensor.dummy_pixels = 0;
    OV16825_sensor.dummy_lines = 0;
    spin_unlock(&OV16825_drv_lock);

    dummy_pixel = 0;
    dummy_line = 0;

    spin_lock(&OV16825_drv_lock);
    OV16825_sensor.pclk = OV16825_CAPTURE_CLK;
    OV16825_sensor.line_length = OV16825_FULL_PERIOD_PIXEL_NUMS + dummy_pixel;
    OV16825_sensor.frame_height = OV16825_FULL_PERIOD_LINE_NUMS + dummy_line;
    spin_unlock(&OV16825_drv_lock);

    OV16825_Set_Dummy(0, dummy_line);


#ifdef OV16825_DRIVER_TRACE
    SENSORDB("OV16825Capture end\n");
#endif
    //mdelay(50);

    return ERROR_NONE;
}   /* OV16825_Capture() */


UINT32 OV168253DPreview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	kal_uint16 dummy_line;
	kal_uint16 ret;
#ifdef OV16825_DRIVER_TRACE
	SENSORDB("OV168253DPreview \n");
#endif

	//OV16825_Sensor_1M();

	OV16825MIPI_Sensor_4M();

    //msleep(30);
    spin_lock(&OV16825_drv_lock);
	OV16825_sensor.pv_mode = KAL_TRUE;
	spin_unlock(&OV16825_drv_lock);
	
	//OV16825_set_mirror(sensor_config_data->SensorImageMirror);
	switch (sensor_config_data->SensorOperationMode)
	{
	  case MSDK_SENSOR_OPERATION_MODE_VIDEO: 
	  	spin_lock(&OV16825_drv_lock);
		OV16825_sensor.video_mode = KAL_TRUE;		
		spin_unlock(&OV16825_drv_lock);
		dummy_line = 0;
#ifdef OV16825_DRIVER_TRACE
		SENSORDB("Video mode \n");
#endif
	   break;
	  default: /* ISP_PREVIEW_MODE */
	  	spin_lock(&OV16825_drv_lock);
		OV16825_sensor.video_mode = KAL_FALSE;
		spin_unlock(&OV16825_drv_lock);
		dummy_line = 0;
#ifdef OV16825_DRIVER_TRACE
		SENSORDB("Camera preview mode \n");
#endif
	  break;
	}

	spin_lock(&OV16825_drv_lock);
	OV16825_sensor.dummy_pixels = 0;
	OV16825_sensor.dummy_lines = 0;
	OV16825_sensor.line_length = OV16825_PV_PERIOD_PIXEL_NUMS;
	OV16825_sensor.frame_height = OV16825_PV_PERIOD_LINE_NUMS + dummy_line;
	OV16825_sensor.pclk = OV16825_PREVIEW_CLK;
	spin_unlock(&OV16825_drv_lock);
	
	OV16825_Set_Dummy(0, dummy_line); /* modify dummy_pixel must gen AE table again */
	//OV16825_Write_Shutter(OV16825_sensor.shutter);
		
	return ERROR_NONE;
	
}


UINT32 OV16825GetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution)
{
#ifdef OV16825_DRIVER_TRACE
    SENSORDB("OV16825GetResolution \n");
#endif
    pSensorResolution->SensorFullWidth=OV16825_IMAGE_SENSOR_FULL_WIDTH;
    pSensorResolution->SensorFullHeight=OV16825_IMAGE_SENSOR_FULL_HEIGHT;
    pSensorResolution->SensorPreviewWidth=OV16825_IMAGE_SENSOR_PV_WIDTH;
    pSensorResolution->SensorPreviewHeight=OV16825_IMAGE_SENSOR_PV_HEIGHT;
    pSensorResolution->SensorVideoWidth=OV16825_IMAGE_SENSOR_VIDEO_WIDTH;
    pSensorResolution->SensorVideoHeight=OV16825_IMAGE_SENSOR_VIDEO_HEIGHT;
	pSensorResolution->Sensor3DFullWidth=OV16825_IMAGE_SENSOR_3D_FULL_WIDTH;
	pSensorResolution->Sensor3DFullHeight=OV16825_IMAGE_SENSOR_3D_FULL_HEIGHT;
	pSensorResolution->Sensor3DPreviewWidth=OV16825_IMAGE_SENSOR_3D_PV_WIDTH;
	pSensorResolution->Sensor3DPreviewHeight=OV16825_IMAGE_SENSOR_3D_PV_HEIGHT;	
	pSensorResolution->Sensor3DVideoWidth=OV16825_IMAGE_SENSOR_3D_VIDEO_WIDTH;
	pSensorResolution->Sensor3DVideoHeight=OV16825_IMAGE_SENSOR_3D_VIDEO_HEIGHT;
    return ERROR_NONE;
}/* OV16825GetResolution() */

UINT32 OV16825GetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId,
        MSDK_SENSOR_INFO_STRUCT *pSensorInfo,
        MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
#ifdef OV16825_DRIVER_TRACE
    SENSORDB("OV16825GetInfo，FeatureId:%d\n",ScenarioId);
#endif

    switch(ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_ZSD:
            pSensorInfo->SensorPreviewResolutionX=OV16825_IMAGE_SENSOR_FULL_WIDTH;
            pSensorInfo->SensorPreviewResolutionY=OV16825_IMAGE_SENSOR_FULL_HEIGHT;
            pSensorInfo->SensorFullResolutionX=OV16825_IMAGE_SENSOR_FULL_WIDTH;
            pSensorInfo->SensorFullResolutionY=OV16825_IMAGE_SENSOR_FULL_HEIGHT;
            pSensorInfo->SensorCameraPreviewFrameRate = 15;
            break;
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            pSensorInfo->SensorPreviewResolutionX=OV16825_IMAGE_SENSOR_PV_WIDTH;
            pSensorInfo->SensorPreviewResolutionY=OV16825_IMAGE_SENSOR_PV_HEIGHT;
            pSensorInfo->SensorFullResolutionX=OV16825_IMAGE_SENSOR_FULL_WIDTH;
            pSensorInfo->SensorFullResolutionY=OV16825_IMAGE_SENSOR_FULL_HEIGHT;
            pSensorInfo->SensorCameraPreviewFrameRate=30;
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            pSensorInfo->SensorPreviewResolutionX=OV16825_IMAGE_SENSOR_VIDEO_WIDTH;
            pSensorInfo->SensorPreviewResolutionY=OV16825_IMAGE_SENSOR_VIDEO_HEIGHT;
            pSensorInfo->SensorFullResolutionX=OV16825_IMAGE_SENSOR_FULL_WIDTH;
            pSensorInfo->SensorFullResolutionY=OV16825_IMAGE_SENSOR_FULL_HEIGHT;
            pSensorInfo->SensorCameraPreviewFrameRate=30;
            break;
	  case MSDK_SCENARIO_ID_CAMERA_3D_PREVIEW: //added
	  case MSDK_SCENARIO_ID_CAMERA_3D_VIDEO:
	  case MSDK_SCENARIO_ID_CAMERA_3D_CAPTURE: //added	 
		   pSensorInfo->SensorPreviewResolutionX=OV16825_IMAGE_SENSOR_3D_VIDEO_WIDTH;
		   pSensorInfo->SensorPreviewResolutionY=OV16825_IMAGE_SENSOR_3D_VIDEO_HEIGHT;
		   pSensorInfo->SensorFullResolutionX=OV16825_IMAGE_SENSOR_3D_FULL_WIDTH;
		   pSensorInfo->SensorFullResolutionY=OV16825_IMAGE_SENSOR_3D_FULL_HEIGHT;			   
		   pSensorInfo->SensorCameraPreviewFrameRate=30;		  
		  break;
        default:
            pSensorInfo->SensorPreviewResolutionX=OV16825_IMAGE_SENSOR_PV_WIDTH;
            pSensorInfo->SensorPreviewResolutionY=OV16825_IMAGE_SENSOR_PV_HEIGHT;
            pSensorInfo->SensorFullResolutionX=OV16825_IMAGE_SENSOR_FULL_WIDTH;
            pSensorInfo->SensorFullResolutionY=OV16825_IMAGE_SENSOR_FULL_HEIGHT;
            pSensorInfo->SensorCameraPreviewFrameRate = 30;
            break;
    }

    //pSensorInfo->SensorCameraPreviewFrameRate=30;
    pSensorInfo->SensorVideoFrameRate=30;
    pSensorInfo->SensorStillCaptureFrameRate=10;
    pSensorInfo->SensorWebCamCaptureFrameRate=15;
    pSensorInfo->SensorResetActiveHigh=FALSE; //low active
    pSensorInfo->SensorResetDelayCount=5; 

    pSensorInfo->SensorOutputDataFormat=OV16825_COLOR_FORMAT;
    pSensorInfo->SensorClockPolarity=SENSOR_CLOCK_POLARITY_LOW;	
    pSensorInfo->SensorClockFallingPolarity=SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorInterruptDelayLines = 4;
    pSensorInfo->SensroInterfaceType        = SENSOR_INTERFACE_TYPE_MIPI;
    pSensorInfo->CaptureDelayFrame = 2; 
    pSensorInfo->PreviewDelayFrame = 2; 
    pSensorInfo->VideoDelayFrame = 1;

    pSensorInfo->SensorMasterClockSwitch = 0; 
    pSensorInfo->SensorDrivingCurrent = ISP_DRIVING_4MA;
    pSensorInfo->AEShutDelayFrame = 0;   /* The frame of setting shutter default 0 for TG int */
    pSensorInfo->AESensorGainDelayFrame = 0;/* The frame of setting sensor gain */
    pSensorInfo->AEISPGainDelayFrame = 2;    
    switch (ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			pSensorInfo->SensorClockFreq=24;
            pSensorInfo->SensorClockDividCount=3;
            pSensorInfo->SensorClockRisingCount= 0;
            pSensorInfo->SensorClockFallingCount= 2;
            pSensorInfo->SensorPixelClockCount= 3;
            pSensorInfo->SensorDataLatchCount= 2;
            pSensorInfo->SensorGrabStartX = OV16825_PV_X_START; 
            pSensorInfo->SensorGrabStartY = OV16825_PV_Y_START; 

            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0; 
            pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 26; 
            pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorWidthSampling = 0;  // 0 is default 1x
            pSensorInfo->SensorHightSampling = 0;   // 0 is default 1x 
            pSensorInfo->SensorPacketECCOrder = 1;
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			pSensorInfo->SensorClockFreq=24;
            pSensorInfo->SensorClockDividCount=	3;
            pSensorInfo->SensorClockRisingCount= 0;
            pSensorInfo->SensorClockFallingCount= 2;
            pSensorInfo->SensorPixelClockCount= 3;
            pSensorInfo->SensorDataLatchCount= 2;
            pSensorInfo->SensorGrabStartX = OV16825_PV_X_START; 
            pSensorInfo->SensorGrabStartY = OV16825_PV_Y_START; 

            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0; 
            pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 26; 
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
            pSensorInfo->SensorGrabStartX = OV16825_FULL_X_START; 
            pSensorInfo->SensorGrabStartY = OV16825_FULL_Y_START; 
            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;	//Hesong Modify 10/25  SENSOR_MIPI_2_LANE	
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0; 
	        pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 26; 
	        pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorWidthSampling = 0;  // 0 is default 1x
            pSensorInfo->SensorHightSampling = 0;   // 0 is default 1x 
            pSensorInfo->SensorPacketECCOrder = 1;
		break;

        case MSDK_SCENARIO_ID_CAMERA_3D_PREVIEW: //added
        case MSDK_SCENARIO_ID_CAMERA_3D_VIDEO:
        case MSDK_SCENARIO_ID_CAMERA_3D_CAPTURE: //added   
			pSensorInfo->SensorClockFreq=24;
			pSensorInfo->SensorClockDividCount=	3;
			pSensorInfo->SensorClockRisingCount= 0;
			pSensorInfo->SensorClockFallingCount= 2;
			pSensorInfo->SensorPixelClockCount= 3;
			pSensorInfo->SensorDataLatchCount= 2;
			pSensorInfo->SensorGrabStartX=  OV16825_3D_PV_X_START;
			pSensorInfo->SensorGrabStartY = OV16825_3D_PV_Y_START; 
            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;	//Hesong Modify 10/25  SENSOR_MIPI_2_LANE	
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0; 
            pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 26; 
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
            pSensorInfo->SensorGrabStartX = OV16825_PV_X_START; 
            pSensorInfo->SensorGrabStartY = OV16825_PV_Y_START; 

            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0; 
            pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 26; 
            pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorWidthSampling = 0;  // 0 is default 1x
            pSensorInfo->SensorHightSampling = 0;   // 0 is default 1x 
            pSensorInfo->SensorPacketECCOrder = 1;
            break;
    }
  return ERROR_NONE;
}	/* OV16825GetInfo() */


UINT32 OV16825Control(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow,
        MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
#ifdef OV16825_DRIVER_TRACE
    SENSORDB("OV16825Control，ScenarioId:%d\n",ScenarioId);
#endif	

    spin_lock(&OV16825_drv_lock);
    ov16825CurrentScenarioId = ScenarioId;
    spin_unlock(&OV16825_drv_lock);

    switch (ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            OV16825Preview(pImageWindow, pSensorConfigData);
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            OV16825VIDEO(pImageWindow, pSensorConfigData);
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            OV16825Capture(pImageWindow, pSensorConfigData);
        break;
        case MSDK_SCENARIO_ID_CAMERA_ZSD:
            OV16825ZsdPreview(pImageWindow, pSensorConfigData);
		break;
        case MSDK_SCENARIO_ID_CAMERA_3D_PREVIEW: //added
        case MSDK_SCENARIO_ID_CAMERA_3D_VIDEO:
        case MSDK_SCENARIO_ID_CAMERA_3D_CAPTURE: //added   
        	OV168253DPreview(pImageWindow, pSensorConfigData);
            break;
        default:
            return ERROR_INVALID_SCENARIO_ID;
    }
    return ERROR_NONE;
}	/* OV16825Control() */

UINT32 OV16825SetAutoFlickerMode(kal_bool bEnable, UINT16 u2FrameRate)
{

    //kal_uint32 pv_max_frame_rate_lines = OV16825_sensor.dummy_lines;

	SENSORDB("[OV16825SetAutoFlickerMode] bEnable = d%, frame rate(10base) = %d\n", bEnable, u2FrameRate);

    if(bEnable)
    {
        spin_lock(&OV16825_drv_lock);
        OV16825_sensor.is_autofliker = KAL_TRUE;
        spin_unlock(&OV16825_drv_lock);
    }
    else
    {
        spin_lock(&OV16825_drv_lock);
        OV16825_sensor.is_autofliker = KAL_FALSE;
        spin_unlock(&OV16825_drv_lock);
    }
    SENSORDB("[OV16825SetAutoFlickerMode]bEnable:%x \n",bEnable);
	return ERROR_NONE;
}


UINT32 OV16825SetCalData(PSET_SENSOR_CALIBRATION_DATA_STRUCT pSetSensorCalData)
{
    UINT32 i;
    SENSORDB("OV16825 Sensor write calibration data num = %d \r\n", pSetSensorCalData->DataSize);
    SENSORDB("OV16825 Sensor write calibration data format = %x \r\n", pSetSensorCalData->DataFormat);
    if(pSetSensorCalData->DataSize <= MAX_SHADING_DATA_TBL){
        for (i = 0; i < pSetSensorCalData->DataSize; i++){
            if (((pSetSensorCalData->DataFormat & 0xFFFF) == 1) && ((pSetSensorCalData->DataFormat >> 16) == 1)){
                SENSORDB("OV16825 Sensor write calibration data: address = %x, value = %x \r\n",(pSetSensorCalData->ShadingData[i])>>16,(pSetSensorCalData->ShadingData[i])&0xFFFF);
                OV16825_write_cmos_sensor((pSetSensorCalData->ShadingData[i])>>16, (pSetSensorCalData->ShadingData[i])&0xFFFF);
            }
        }
    }
    return ERROR_NONE;
}

UINT32 OV16825SetMaxFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 frameRate) 
{
    kal_uint32 pclk;
    kal_int16 dummyLine;
    kal_uint16 lineLength,frameHeight;

    SENSORDB("OV16825SetMaxFramerateByScenario: scenarioId = %d, frame rate = %d\n",scenarioId,frameRate);
    switch (scenarioId) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            pclk = OV16825_PREVIEW_CLK;
            lineLength = OV16825_PV_PERIOD_PIXEL_NUMS;
            frameHeight = (10 * pclk)/frameRate/(lineLength/4);
            dummyLine = frameHeight - OV16825_PV_PERIOD_LINE_NUMS;
            if (dummyLine < 0){
            dummyLine = 0;
            }
            OV16825_Set_Dummy(0, dummyLine);
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            pclk = OV16825_VIDEO_CLK;
            lineLength = OV16825_VIDEO_PERIOD_PIXEL_NUMS;
            frameHeight = (10 * pclk)/frameRate/(lineLength/4);
            dummyLine = frameHeight - OV16825_VIDEO_PERIOD_LINE_NUMS;
            if (dummyLine < 0){
            dummyLine = 0;
            }
            OV16825_Set_Dummy(0, dummyLine);
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        case MSDK_SCENARIO_ID_CAMERA_ZSD:
            pclk = OV16825_CAPTURE_CLK;
            lineLength = OV16825_FULL_PERIOD_PIXEL_NUMS;
            frameHeight = (10 * pclk)/frameRate/(lineLength/4);
            if(frameHeight < OV16825_FULL_PERIOD_LINE_NUMS)
            frameHeight = OV16825_FULL_PERIOD_LINE_NUMS;
            dummyLine = frameHeight - OV16825_FULL_PERIOD_LINE_NUMS;
            SENSORDB("OV16825SetMaxFramerateByScenario: scenarioId = %d, frame rate calculate = %d\n",((10 * pclk)/frameHeight/lineLength));
            OV16825_Set_Dummy(0, dummyLine);
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


UINT32 OV16825GetDefaultFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 *pframeRate) 
{
    switch (scenarioId) {
    case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
        *pframeRate = 300;
        break;
    case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
    case MSDK_SCENARIO_ID_CAMERA_ZSD:
		//#ifdef OV16825_2_LANE
        *pframeRate = 150;
		//#else
		//*pframeRate = 250;
		//#endif
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

UINT32 OV16825SetVideoMode(UINT16 u2FrameRate)
{
	kal_int16 dummy_line;
    /* to fix VSYNC, to fix frame rate */
#ifdef OV16825_DRIVER_TRACE
    SENSORDB("OV16825SetVideoMode，u2FrameRate:%d\n",u2FrameRate);
#endif	


    if((30 == u2FrameRate)||(15 == u2FrameRate)||(24 == u2FrameRate))
    {
        dummy_line = OV16825_sensor.pclk / u2FrameRate / (OV16825_sensor.line_length/4) - OV16825_sensor.frame_height;
        if (dummy_line < 0) 
            dummy_line = 0;
#ifdef OV16825_DRIVER_TRACE
        SENSORDB("dummy_line %d\n", dummy_line);
#endif
        OV16825_Set_Dummy(0, dummy_line); /* modify dummy_pixel must gen AE table again */
        spin_lock(&OV16825_drv_lock);
        OV16825_sensor.video_mode = KAL_TRUE;
        spin_unlock(&OV16825_drv_lock);
    }
    else if(0 == u2FrameRate)
    {
        spin_lock(&OV16825_drv_lock);
        OV16825_sensor.video_mode = KAL_FALSE;
        spin_unlock(&OV16825_drv_lock);

        SENSORDB("disable video mode\n");
    }
    else{
        SENSORDB("[OV16825SetVideoMode],Error Framerate, u2FrameRate=%d",u2FrameRate);
    }
    return ERROR_NONE;
}


UINT32 OV16825FeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId,
        UINT8 *pFeaturePara,UINT32 *pFeatureParaLen)
{
    UINT16 *pFeatureReturnPara16=(UINT16 *) pFeaturePara;
    UINT16 *pFeatureData16=(UINT16 *) pFeaturePara;
    UINT32 *pFeatureReturnPara32=(UINT32 *) pFeaturePara;
    UINT32 *pFeatureData32=(UINT32 *) pFeaturePara;
    UINT32 OV16825SensorRegNumber;
    UINT32 i;
    //PNVRAM_SENSOR_DATA_STRUCT pSensorDefaultData=(PNVRAM_SENSOR_DATA_STRUCT) pFeaturePara;
    //MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData=(MSDK_SENSOR_CONFIG_STRUCT *) pFeaturePara;
    MSDK_SENSOR_REG_INFO_STRUCT *pSensorRegData=(MSDK_SENSOR_REG_INFO_STRUCT *) pFeaturePara;
    //MSDK_SENSOR_GROUP_INFO_STRUCT *pSensorGroupInfo=(MSDK_SENSOR_GROUP_INFO_STRUCT *) pFeaturePara;
    //MSDK_SENSOR_ITEM_INFO_STRUCT *pSensorItemInfo=(MSDK_SENSOR_ITEM_INFO_STRUCT *) pFeaturePara;
    //MSDK_SENSOR_ENG_INFO_STRUCT	*pSensorEngInfo=(MSDK_SENSOR_ENG_INFO_STRUCT *) pFeaturePara;
    PSET_SENSOR_CALIBRATION_DATA_STRUCT pSetSensorCalData=(PSET_SENSOR_CALIBRATION_DATA_STRUCT)pFeaturePara;

#ifdef OV16825_DRIVER_TRACE
    //SENSORDB("OV16825FeatureControl，FeatureId:%d\n",FeatureId); 
#endif		
    switch (FeatureId)
    {
        case SENSOR_FEATURE_GET_RESOLUTION:
            *pFeatureReturnPara16++=OV16825_IMAGE_SENSOR_FULL_WIDTH;
            *pFeatureReturnPara16=OV16825_IMAGE_SENSOR_FULL_HEIGHT;
            *pFeatureParaLen=4;
            break;
        case SENSOR_FEATURE_GET_PERIOD:/* 3 */
            *pFeatureReturnPara16++= (OV16825_sensor.line_length/4);
            *pFeatureReturnPara16= OV16825_sensor.frame_height;
            *pFeatureParaLen=4;
            break;
        case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:  /* 3 */
            *pFeatureReturnPara32 = OV16825_sensor.pclk;
            break;
        case SENSOR_FEATURE_SET_ESHUTTER:	/* 4 */
            set_OV16825_shutter(*pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_NIGHTMODE:
            //OV16825_night_mode((BOOL) *pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_GAIN:	/* 6 */
            OV16825_SetGain((UINT16) *pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_FLASHLIGHT:
            break;
        case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
            break;
        case SENSOR_FEATURE_SET_REGISTER:
            OV16825_write_cmos_sensor(pSensorRegData->RegAddr, pSensorRegData->RegData);
            break;
        case SENSOR_FEATURE_GET_REGISTER:
            pSensorRegData->RegData = OV16825_read_cmos_sensor(pSensorRegData->RegAddr);
            break;
        case SENSOR_FEATURE_SET_CCT_REGISTER:
            //memcpy(&OV16825_sensor.eng.cct, pFeaturePara, sizeof(OV16825_sensor.eng.cct));
            OV16825SensorRegNumber = OV16825_FACTORY_END_ADDR;
            for (i=0;i<OV16825SensorRegNumber;i++)
            {
                spin_lock(&OV16825_drv_lock);
                OV16825_sensor.eng.cct[i].Addr=*pFeatureData32++;
                OV16825_sensor.eng.cct[i].Para=*pFeatureData32++;
                spin_unlock(&OV16825_drv_lock);
            }

            break;
        case SENSOR_FEATURE_GET_CCT_REGISTER:/* 12 */
            if (*pFeatureParaLen >= sizeof(OV16825_sensor.eng.cct) + sizeof(kal_uint32))
            {
                *((kal_uint32 *)pFeaturePara++) = sizeof(OV16825_sensor.eng.cct);
                memcpy(pFeaturePara, &OV16825_sensor.eng.cct, sizeof(OV16825_sensor.eng.cct));
            }
            break;
        case SENSOR_FEATURE_SET_ENG_REGISTER:
            //memcpy(&OV16825_sensor.eng.reg, pFeaturePara, sizeof(OV16825_sensor.eng.reg));
            OV16825SensorRegNumber = OV16825_ENGINEER_END;
            for (i=0;i<OV16825SensorRegNumber;i++)
            {
                spin_lock(&OV16825_drv_lock);
                OV16825_sensor.eng.reg[i].Addr=*pFeatureData32++;
                OV16825_sensor.eng.reg[i].Para=*pFeatureData32++;
                spin_unlock(&OV16825_drv_lock);
            }
            break;
        case SENSOR_FEATURE_GET_ENG_REGISTER:	/* 14 */
            if (*pFeatureParaLen >= sizeof(OV16825_sensor.eng.reg) + sizeof(kal_uint32))
            {
                *((kal_uint32 *)pFeaturePara++) = sizeof(OV16825_sensor.eng.reg);
                memcpy(pFeaturePara, &OV16825_sensor.eng.reg, sizeof(OV16825_sensor.eng.reg));
            }
            break;
        case SENSOR_FEATURE_GET_REGISTER_DEFAULT:
            ((PNVRAM_SENSOR_DATA_STRUCT)pFeaturePara)->Version = NVRAM_CAMERA_SENSOR_FILE_VERSION;
            ((PNVRAM_SENSOR_DATA_STRUCT)pFeaturePara)->SensorId = OV16825MIPI_SENSOR_ID;
            memcpy(((PNVRAM_SENSOR_DATA_STRUCT)pFeaturePara)->SensorEngReg, &OV16825_sensor.eng.reg, sizeof(OV16825_sensor.eng.reg));
            memcpy(((PNVRAM_SENSOR_DATA_STRUCT)pFeaturePara)->SensorCCTReg, &OV16825_sensor.eng.cct, sizeof(OV16825_sensor.eng.cct));
            *pFeatureParaLen = sizeof(NVRAM_SENSOR_DATA_STRUCT);
            break;
        case SENSOR_FEATURE_GET_CONFIG_PARA:
            memcpy(pFeaturePara, &OV16825_sensor.cfg_data, sizeof(OV16825_sensor.cfg_data));
            *pFeatureParaLen = sizeof(OV16825_sensor.cfg_data);
            break;
        case SENSOR_FEATURE_CAMERA_PARA_TO_SENSOR:
            OV16825_camera_para_to_sensor();
            break;
        case SENSOR_FEATURE_SENSOR_TO_CAMERA_PARA:
            OV16825_sensor_to_camera_para();
            break;
        case SENSOR_FEATURE_GET_GROUP_COUNT:
            OV16825_get_sensor_group_count((kal_uint32 *)pFeaturePara);
            *pFeatureParaLen = 4;
            break;
        case SENSOR_FEATURE_GET_GROUP_INFO:
            OV16825_get_sensor_group_info((MSDK_SENSOR_GROUP_INFO_STRUCT *)pFeaturePara);
            *pFeatureParaLen = sizeof(MSDK_SENSOR_GROUP_INFO_STRUCT);
            break;
        case SENSOR_FEATURE_GET_ITEM_INFO:
            OV16825_get_sensor_item_info((MSDK_SENSOR_ITEM_INFO_STRUCT *)pFeaturePara);
            *pFeatureParaLen = sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
            break;
        case SENSOR_FEATURE_SET_ITEM_INFO:
            OV16825_set_sensor_item_info((MSDK_SENSOR_ITEM_INFO_STRUCT *)pFeaturePara);
            *pFeatureParaLen = sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
            break;
        case SENSOR_FEATURE_GET_ENG_INFO:
            memcpy(pFeaturePara, &OV16825_sensor.eng_info, sizeof(OV16825_sensor.eng_info));
            *pFeatureParaLen = sizeof(OV16825_sensor.eng_info);
            break;
        case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
            // get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE
            // if EEPROM does not exist in camera module.
            *pFeatureReturnPara32=LENS_DRIVER_ID_DO_NOT_CARE;
            *pFeatureParaLen=4;
            break;
        case SENSOR_FEATURE_SET_VIDEO_MODE:
            OV16825SetVideoMode(*pFeatureData16);
            break; 
        case SENSOR_FEATURE_CHECK_SENSOR_ID:
            OV16825GetSensorID(pFeatureReturnPara32); 
            break; 
        case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
            OV16825SetAutoFlickerMode((BOOL)*pFeatureData16,*(pFeatureData16+1));
            break;
        case SENSOR_FEATURE_SET_CALIBRATION_DATA:
            OV16825SetCalData(pSetSensorCalData);
            break;
        case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
            OV16825SetMaxFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32, *(pFeatureData32+1));
            break;
        case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
            OV16825GetDefaultFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32, (MUINT32 *)(*(pFeatureData32+1)));
            break;
        default:
            break;
    }
    return ERROR_NONE;
}/* OV16825FeatureControl() */
SENSOR_FUNCTION_STRUCT SensorFuncOV16825=
{
    OV16825Open,
    OV16825GetInfo,
    OV16825GetResolution,
    OV16825FeatureControl,
    OV16825Control,
    OV16825Close
};

UINT32 OV16825MIPISensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
//UINT32 OV16825SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
/* To Do : Check Sensor status here */
    if (pfFunc!=NULL)
        *pfFunc=&SensorFuncOV16825;

    return ERROR_NONE;
}/* SensorInit() */



