#ifndef _BU64241_H
#define _BU64241_H

#include <linux/ioctl.h>
//#include "kd_imgsensor.h"

#define BU64241_MAGIC 'A'
//IOCTRL(inode * ,file * ,cmd ,arg )


//Structures
typedef struct {
//current position
unsigned long u4CurrentPosition;
//macro position
unsigned long u4MacroPosition;
//Infiniti position
unsigned long u4InfPosition;
//Motor Status
bool          bIsMotorMoving;
//Motor Open?
bool          bIsMotorOpen;
//Support SR?
bool          bIsSupportSR;
} stBU64241_MotorInfo;

//Control commnad
//S means "set through a ptr"
//T means "tell by a arg value"
//G means "get by a ptr"             
//Q means "get by return a value"
//X means "switch G and S atomically"
//H means "switch T and Q atomically"
#define BU64241IOC_G_MOTORINFO _IOR(BU64241_MAGIC,0,stBU64241_MotorInfo)

#define BU64241IOC_T_MOVETO _IOW(BU64241_MAGIC,1,unsigned long)

#define BU64241IOC_T_SETINFPOS _IOW(BU64241_MAGIC,2,unsigned long)

#define BU64241IOC_T_SETMACROPOS _IOW(BU64241_MAGIC,3,unsigned long)

#else
#endif
