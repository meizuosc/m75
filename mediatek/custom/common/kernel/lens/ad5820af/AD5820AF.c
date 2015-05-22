/*
 * MD218A voice coil motor driver
 *
 *
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include "AD5820AF.h"
#include "../camera/kd_camera_hw.h"

#define LENS_I2C_BUSNUM 0
static struct i2c_board_info __initdata kd_lens_dev={ I2C_BOARD_INFO("AD5820AF", 0x6C)};


#define AD5820AF_DRVNAME "AD5820AF"
#define AD5820AF_VCM_WRITE_ID           0x18

#define AD5820AF_DEBUG
#ifdef AD5820AF_DEBUG
#define AD5820AFDB printk
#else
#define AD5820AFDB(x,...)
#endif

static spinlock_t g_AD5820AF_SpinLock;

static struct i2c_client * g_pstAD5820AF_I2Cclient = NULL;

static dev_t g_AD5820AF_devno;
static struct cdev * g_pAD5820AF_CharDrv = NULL;
static struct class *actuator_class = NULL;

static int  g_s4AD5820AF_Opened = 0;
static long g_i4MotorStatus = 0;
static long g_i4Dir = 0;
static unsigned long g_u4AD5820AF_INF = 0;
static unsigned long g_u4AD5820AF_MACRO = 1023;
static unsigned long g_u4TargetPosition = 0;
static unsigned long g_u4CurrPosition   = 0;

static int g_sr = 3;

static int s4AD5820AF_ReadReg(unsigned short * a_pu2Result)
{
    int  i4RetValue = 0;
    char pBuff[2];
	g_pstAD5820AF_I2Cclient->addr = (0x18 >> 1);

    i4RetValue = i2c_master_recv(g_pstAD5820AF_I2Cclient, pBuff , 2);

    if (i4RetValue < 0) 
    {
        AD5820AFDB("[AD5820AF] I2C read failed!! \n");
        return -1;
    }

    *a_pu2Result = (((u16)pBuff[0]) << 4) + (pBuff[1] >> 4);

    return 0;
}

static int s4AD5820AF_WriteReg(u16 a_u2Data)
{
    int  i4RetValue = 0;

    char puSendCmd[2] = {(char)(a_u2Data >> 4) , (char)((a_u2Data & 0xF) << 4)};
    g_pstAD5820AF_I2Cclient->addr = (0x18 >> 1);
    //g_pstAD5820AF_I2Cclient->ext_flag |= I2C_A_FILTER_MSG;
    i4RetValue = i2c_master_send(g_pstAD5820AF_I2Cclient, puSendCmd, 2);
    if (i4RetValue < 0) 
    {
        AD5820AFDB("[AD5820AF]1 I2C send failed!! 1\n");
        return -1;
    }

    return 0;
}

inline static int getAD5820AFInfo(__user stAD5820AF_MotorInfo * pstMotorInfo)
{
    stAD5820AF_MotorInfo stMotorInfo;
    stMotorInfo.u4MacroPosition   = g_u4AD5820AF_MACRO;
    stMotorInfo.u4InfPosition     = g_u4AD5820AF_INF;
    stMotorInfo.u4CurrentPosition = g_u4CurrPosition;
    stMotorInfo.bIsSupportSR      = TRUE;

	if (g_i4MotorStatus == 1)	{stMotorInfo.bIsMotorMoving = 1;}
	else						{stMotorInfo.bIsMotorMoving = 0;}

	if (g_s4AD5820AF_Opened >= 1)	{stMotorInfo.bIsMotorOpen = 1;}
	else						{stMotorInfo.bIsMotorOpen = 0;}

    if(copy_to_user(pstMotorInfo , &stMotorInfo , sizeof(stAD5820AF_MotorInfo)))
    {
        AD5820AFDB("[AD5820AF] copy to user failed when getting motor information \n");
    }

    return 0;
}

#ifdef LensdrvCM3
inline static int getAD5820AFMETA(__user stAD5820AF_MotorMETAInfo * pstMotorMETAInfo)
{
    stAD5820AF_MotorMETAInfo stMotorMETAInfo;
    stMotorMETAInfo.Aperture=2.8;      //fn
	stMotorMETAInfo.Facing=1;   
	stMotorMETAInfo.FilterDensity=1;   //X
	stMotorMETAInfo.FocalDistance=1.0;  //diopters
	stMotorMETAInfo.FocalLength=34.0;  //mm
	stMotorMETAInfo.FocusRange=1.0;    //diopters
	stMotorMETAInfo.InfoAvalibleApertures=2.8;
	stMotorMETAInfo.InfoAvalibleFilterDensity=1;
	stMotorMETAInfo.InfoAvalibleFocalLength=34.0;
	stMotorMETAInfo.InfoAvalibleHypeDistance=1.0;
	stMotorMETAInfo.InfoAvalibleMinFocusDistance=1.0;
	stMotorMETAInfo.InfoAvalibleOptStabilization=0;
	stMotorMETAInfo.OpticalAxisAng[0]=0.0;
	stMotorMETAInfo.OpticalAxisAng[1]=0.0;
	stMotorMETAInfo.Position[0]=0.0;
	stMotorMETAInfo.Position[1]=0.0;
	stMotorMETAInfo.Position[2]=0.0;
	stMotorMETAInfo.State=0;
	stMotorMETAInfo.u4OIS_Mode=0;
	
	if(copy_to_user(pstMotorMETAInfo , &stMotorMETAInfo , sizeof(stAD5820AF_MotorMETAInfo)))
	{
		AD5820AFDB("[AD5820AF] copy to user failed when getting motor information \n");
	}

    return 0;
}
#endif

inline static int moveAD5820AF(unsigned long a_u4Position)
{
    int ret = 0;
    
    if((a_u4Position > g_u4AD5820AF_MACRO) || (a_u4Position < g_u4AD5820AF_INF))
    {
        AD5820AFDB("[AD5820AF] out of range \n");
        return -EINVAL;
    }

    if (g_s4AD5820AF_Opened == 1)
    {
        unsigned short InitPos;
        ret = s4AD5820AF_ReadReg(&InitPos);
	    
        if(ret == 0)
        {
            AD5820AFDB("[AD5820AF] Init Pos %6d \n", InitPos);
			
			spin_lock(&g_AD5820AF_SpinLock);
            g_u4CurrPosition = (unsigned long)InitPos;
			spin_unlock(&g_AD5820AF_SpinLock);
			
        }
        else
        {	
			spin_lock(&g_AD5820AF_SpinLock);
            g_u4CurrPosition = 0;
			spin_unlock(&g_AD5820AF_SpinLock);
        }

		spin_lock(&g_AD5820AF_SpinLock);
        g_s4AD5820AF_Opened = 2;
        spin_unlock(&g_AD5820AF_SpinLock);
    }

    if (g_u4CurrPosition < a_u4Position)
    {
        spin_lock(&g_AD5820AF_SpinLock);	
        g_i4Dir = 1;
        spin_unlock(&g_AD5820AF_SpinLock);	
    }
    else if (g_u4CurrPosition > a_u4Position)
    {
        spin_lock(&g_AD5820AF_SpinLock);	
        g_i4Dir = -1;
        spin_unlock(&g_AD5820AF_SpinLock);			
    }
    else										{return 0;}

    spin_lock(&g_AD5820AF_SpinLock);    
    g_u4TargetPosition = a_u4Position;
    spin_unlock(&g_AD5820AF_SpinLock);	

    //AD5820AFDB("[AD5820AF] move [curr] %d [target] %d\n", g_u4CurrPosition, g_u4TargetPosition);

            spin_lock(&g_AD5820AF_SpinLock);
            g_sr = 3;
            g_i4MotorStatus = 0;
            spin_unlock(&g_AD5820AF_SpinLock);	
		
            if(s4AD5820AF_WriteReg((unsigned short)g_u4TargetPosition) == 0)
            {
                spin_lock(&g_AD5820AF_SpinLock);		
                g_u4CurrPosition = (unsigned long)g_u4TargetPosition;
                spin_unlock(&g_AD5820AF_SpinLock);				
            }
            else
            {
                AD5820AFDB("[AD5820AF] set I2C failed when moving the motor \n");			
                spin_lock(&g_AD5820AF_SpinLock);
                g_i4MotorStatus = -1;
                spin_unlock(&g_AD5820AF_SpinLock);				
            }

    return 0;
}

inline static int setAD5820AFInf(unsigned long a_u4Position)
{
    spin_lock(&g_AD5820AF_SpinLock);
    g_u4AD5820AF_INF = a_u4Position;
    spin_unlock(&g_AD5820AF_SpinLock);	
    return 0;
}

inline static int setAD5820AFMacro(unsigned long a_u4Position)
{
    spin_lock(&g_AD5820AF_SpinLock);
    g_u4AD5820AF_MACRO = a_u4Position;
    spin_unlock(&g_AD5820AF_SpinLock);	
    return 0;	
}

////////////////////////////////////////////////////////////////
static long AD5820AF_Ioctl(
struct file * a_pstFile,
unsigned int a_u4Command,
unsigned long a_u4Param)
{
    long i4RetValue = 0;

    switch(a_u4Command)
    {
        case AD5820AFIOC_G_MOTORINFO :
            i4RetValue = getAD5820AFInfo((__user stAD5820AF_MotorInfo *)(a_u4Param));
        break;
		#ifdef LensdrvCM3
        case AD5820AFIOC_G_MOTORMETAINFO :
            i4RetValue = getAD5820AFMETA((__user stAD5820AF_MotorMETAInfo *)(a_u4Param));
        break;
		#endif
        case AD5820AFIOC_T_MOVETO :
            i4RetValue = moveAD5820AF(a_u4Param);
        break;
 
        case AD5820AFIOC_T_SETINFPOS :
            i4RetValue = setAD5820AFInf(a_u4Param);
        break;

        case AD5820AFIOC_T_SETMACROPOS :
            i4RetValue = setAD5820AFMacro(a_u4Param);
        break;
		
        default :
      	    AD5820AFDB("[AD5820AF] No CMD \n");
            i4RetValue = -EPERM;
        break;
    }

    return i4RetValue;
}

//Main jobs:
// 1.check for device-specified errors, device not ready.
// 2.Initialize the device if it is opened for the first time.
// 3.Update f_op pointer.
// 4.Fill data structures into private_data
//CAM_RESET
static int AD5820AF_Open(struct inode * a_pstInode, struct file * a_pstFile)
{
    AD5820AFDB("[AD5820AF] AD5820AF_Open - Start\n");
    if(g_s4AD5820AF_Opened)
    {
        AD5820AFDB("[AD5820AF] the device is opened \n");
        return -EBUSY;
    }
	
    spin_lock(&g_AD5820AF_SpinLock);
    g_s4AD5820AF_Opened = 1;
    spin_unlock(&g_AD5820AF_SpinLock);
    AD5820AFDB("[AD5820AF] AD5820AF_Open - End\n");
	//s4AD5820AF_WriteReg(1023);msleep(10);
	//s4AD5820AF_WriteReg(0);

    return 0;
}

//Main jobs:
// 1.Deallocate anything that "open" allocated in private_data.
// 2.Shut down the device on last close.
// 3.Only called once on last time.
// Q1 : Try release multiple times.
static int AD5820AF_Release(struct inode * a_pstInode, struct file * a_pstFile)
{
    AD5820AFDB("[AD5820AF] AD5820AF_Release - Start\n");

    if (g_s4AD5820AF_Opened)
    {
        AD5820AFDB("[AD5820AF] feee \n");
        g_sr = 5;
	    s4AD5820AF_WriteReg(200);
        msleep(10);
	    s4AD5820AF_WriteReg(100);
        msleep(10);
            	            	    	    
        spin_lock(&g_AD5820AF_SpinLock);
        g_s4AD5820AF_Opened = 0;
        spin_unlock(&g_AD5820AF_SpinLock);

    }

    AD5820AFDB("[AD5820AF] AD5820AF_Release - End\n");

    return 0;
}

static const struct file_operations g_stAD5820AF_fops = 
{
    .owner = THIS_MODULE,
    .open = AD5820AF_Open,
    .release = AD5820AF_Release,
    .unlocked_ioctl = AD5820AF_Ioctl
};

inline static int Register_AD5820AF_CharDrv(void)
{
    struct device* vcm_device = NULL;

    AD5820AFDB("[AD5820AF] Register_AD5820AF_CharDrv - Start\n");

    //Allocate char driver no.
    if( alloc_chrdev_region(&g_AD5820AF_devno, 0, 1,AD5820AF_DRVNAME) )
    {
        AD5820AFDB("[AD5820AF] Allocate device no failed\n");

        return -EAGAIN;
    }

    //Allocate driver
    g_pAD5820AF_CharDrv = cdev_alloc();

    if(NULL == g_pAD5820AF_CharDrv)
    {
        unregister_chrdev_region(g_AD5820AF_devno, 1);

        AD5820AFDB("[AD5820AF] Allocate mem for kobject failed\n");

        return -ENOMEM;
    }

    //Attatch file operation.
    cdev_init(g_pAD5820AF_CharDrv, &g_stAD5820AF_fops);

    g_pAD5820AF_CharDrv->owner = THIS_MODULE;

    //Add to system
    if(cdev_add(g_pAD5820AF_CharDrv, g_AD5820AF_devno, 1))
    {
        AD5820AFDB("[AD5820AF] Attatch file operation failed\n");

        unregister_chrdev_region(g_AD5820AF_devno, 1);

        return -EAGAIN;
    }

    actuator_class = class_create(THIS_MODULE, "actuatordrv2");
    if (IS_ERR(actuator_class)) {
        int ret = PTR_ERR(actuator_class);
        AD5820AFDB("Unable to create class, err = %d\n", ret);
        return ret;            
    }

    vcm_device = device_create(actuator_class, NULL, g_AD5820AF_devno, NULL, AD5820AF_DRVNAME);

    if(NULL == vcm_device)
    {
        return -EIO;
    }
    
    AD5820AFDB("[AD5820AF] Register_AD5820AF_CharDrv - End\n");    
    return 0;
}

inline static void Unregister_AD5820AF_CharDrv(void)
{
    AD5820AFDB("[AD5820AF] Unregister_AD5820AF_CharDrv - Start\n");

    //Release char driver
    cdev_del(g_pAD5820AF_CharDrv);

    unregister_chrdev_region(g_AD5820AF_devno, 1);
    
    device_destroy(actuator_class, g_AD5820AF_devno);

    class_destroy(actuator_class);

    AD5820AFDB("[AD5820AF] Unregister_AD5820AF_CharDrv - End\n");    
}

//////////////////////////////////////////////////////////////////////

static int AD5820AF_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int AD5820AF_i2c_remove(struct i2c_client *client);
static const struct i2c_device_id AD5820AF_i2c_id[] = {{AD5820AF_DRVNAME,0},{}};   
struct i2c_driver AD5820AF_i2c_driver = {                       
    .probe = AD5820AF_i2c_probe,                                   
    .remove = AD5820AF_i2c_remove,                           
    .driver.name = AD5820AF_DRVNAME,                 
    .id_table = AD5820AF_i2c_id,                             
};  

#if 0 
static int AD5820AF_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info) {         
    strcpy(info->type, AD5820AF_DRVNAME);                                                         
    return 0;                                                                                       
}      
#endif 
static int AD5820AF_i2c_remove(struct i2c_client *client) {
    return 0;
}

/* Kirby: add new-style driver {*/
static int AD5820AF_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int i4RetValue = 0;

    AD5820AFDB("[AD5820AF] AD5820AF_i2c_probe\n");

    /* Kirby: add new-style driver { */
    g_pstAD5820AF_I2Cclient = client;
    
    g_pstAD5820AF_I2Cclient->addr = (0x18 >> 1);
    
    //Register char driver
    i4RetValue = Register_AD5820AF_CharDrv();

    if(i4RetValue){

        AD5820AFDB("[AD5820AF] register char device failed!\n");

        return i4RetValue;
    }

    spin_lock_init(&g_AD5820AF_SpinLock);

    AD5820AFDB("[AD5820AF] Attached!! \n");

    return 0;
}

static int AD5820AF_probe(struct platform_device *pdev)
{
    return i2c_add_driver(&AD5820AF_i2c_driver);
}

static int AD5820AF_remove(struct platform_device *pdev)
{
    i2c_del_driver(&AD5820AF_i2c_driver);
    return 0;
}

static int AD5820AF_suspend(struct platform_device *pdev, pm_message_t mesg)
{
    return 0;
}

static int AD5820AF_resume(struct platform_device *pdev)
{
    return 0;
}

// platform structure
static struct platform_driver g_stAD5820AF_Driver = {
    .probe		= AD5820AF_probe,
    .remove	= AD5820AF_remove,
    .suspend	= AD5820AF_suspend,
    .resume	= AD5820AF_resume,
    .driver		= {
        .name	= "lens_actuator2",
        .owner	= THIS_MODULE,
    }
};

static int __init AD5820AF_i2C_init(void)
{
    i2c_register_board_info(LENS_I2C_BUSNUM, &kd_lens_dev, 1);
	
    if(platform_driver_register(&g_stAD5820AF_Driver)){
        AD5820AFDB("failed to register AD5820AF driver\n");
        return -ENODEV;
    }

    return 0;
}

static void __exit AD5820AF_i2C_exit(void)
{
	platform_driver_unregister(&g_stAD5820AF_Driver);
}

module_init(AD5820AF_i2C_init);
module_exit(AD5820AF_i2C_exit);

MODULE_DESCRIPTION("AD5820AF lens module driver");
MODULE_AUTHOR("KY Chen <ky.chen@Mediatek.com>");
MODULE_LICENSE("GPL");


