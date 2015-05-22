//********************************************************************************
//
//		LC89821x Interface module
//
//	    Program Name	: AfInter.c
//		Design			: Rex.Tang
//		History			: First edition						2013.07.13 Rex.Tang
//
//		Description		: User needs to complete the interface functions
//********************************************************************************

#include	<linux/i2c.h>
#include 	<linux/delay.h>
#define		DeviceAddr		0xE4  	// Device address of driver IC


extern int LC898212XD_WriteRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u16 i2cId);
extern int LC898212XD_ReadRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId);

/*--------------------------------------------------------
  	IIC wrtie 2 bytes function
  	Parameters:	addr, data
--------------------------------------------------------*/
void RamWriteA(unsigned short addr, unsigned short data)
{
       // To call your IIC function here
	u8 puSendCmd[3] = {(u8)(addr & 0xFF) ,(u8)(data >> 8),(u8)(data & 0xFF)};        
	LC898212XD_WriteRegI2C(puSendCmd , sizeof(puSendCmd), DeviceAddr);
        
}


/*------------------------------------------------------
  	IIC read 2 bytes function
  	Parameters:	addr, *data
-------------------------------------------------------*/                   		
void RamReadA(unsigned short addr, unsigned short *data)
{
      // To call your IIC function here
	u8 buf[2];
	u8 puSendCmd[1] = {(u8)(addr & 0xFF) };
	LC898212XD_ReadRegI2C(puSendCmd , sizeof(puSendCmd), buf, 2, DeviceAddr);
	*data = (buf[0] << 8) | (buf[1] & 0x00FF);
}


/*--------------------------------------------------------
  	IIC wrtie 1 byte function
  	Parameters:	addr, data
--------------------------------------------------------*/
void RegWriteA(unsigned short addr, unsigned char data)
{
      // To call your IIC function here
	u8 puSendCmd[2] = {(u8)(addr & 0xFF) ,(u8)(data & 0xFF)};        
	LC898212XD_WriteRegI2C(puSendCmd , sizeof(puSendCmd), DeviceAddr);
}


/*--------------------------------------------------------
  	IIC read 1 byte function
  	Parameters:	addr, *data
--------------------------------------------------------*/
void RegReadA(unsigned short addr, unsigned char *data)
{
     // To call your IIC function here
	u8 puSendCmd[1] = {(u8)(addr & 0xFF) };
	LC898212XD_ReadRegI2C(puSendCmd , sizeof(puSendCmd), data, 1, DeviceAddr);     
}


/*--------------------------------------------------------
  	Wait function
  	Parameters:	msec
--------------------------------------------------------*/
void WaitTime(unsigned short msec)
{
     // To call your Wait function here
     usleep_range(msec * 1000, (msec + 1) * 1000);     
}

