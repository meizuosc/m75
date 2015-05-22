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
#include "BU64241.h"
#include "../camera/kd_camera_hw.h"

#define LENS_I2C_BUSNUM 0
static struct i2c_board_info __initdata kd_lens_dev={ I2C_BOARD_INFO("BU64241", 0x19)};//0x18


#define BU64241_DRVNAME "BU64241"
#define BU64241_VCM_WRITE_ID           0x19//0x18

#define BU64241_DEBUG
#ifdef BU64241_DEBUG
#define BU64241DB printk
#else
#define BU64241DB(x,...)
#endif

static spinlock_t g_BU64241_SpinLock;

static struct i2c_client * g_pstBU64241_I2Cclient = NULL;

static dev_t g_BU64241_devno;
static struct cdev * g_pBU64241_CharDrv = NULL;
static struct class *actuator_class = NULL;

static int  g_s4BU64241_Opened = 0;
static long g_i4MotorStatus = 0;
static long g_i4Dir = 0;
static unsigned long g_u4BU64241_INF = 0;
static unsigned long g_u4BU64241_MACRO = 1023;
static unsigned long g_u4TargetPosition = 0;
static unsigned long g_u4CurrPosition   = 0;

static int g_sr = 3;

#if 0
extern s32 mt_set_gpio_mode(u32 u4Pin, u32 u4Mode);
extern s32 mt_set_gpio_out(u32 u4Pin, u32 u4PinOut);
extern s32 mt_set_gpio_dir(u32 u4Pin, u32 u4Dir);
#endif

static int s4BU64241_ReadReg(unsigned short * a_pu2Result)
{
    int  i4RetValue = 0;
    char pBuff[2];

    i4RetValue = i2c_master_recv(g_pstBU64241_I2Cclient, pBuff , 2);

    if (i4RetValue < 0) 
    {
        BU64241DB("[BU64241] I2C read failed!! \n");
        return -1;
    }

    *a_pu2Result = (((u16)pBuff[0]) << 4) + (pBuff[1] >> 4);

    return 0;
}

static int s4BU64241_WriteReg(u16 a_u2Data)
{
    int  i4RetValue = 0;

    char puSendCmd[2] = {(char)(a_u2Data >> 8)&0x03|0xc4 , (char)(a_u2Data & 0xff )};

    //BU64241DB("[BU64241] g_sr %d, write %d \n", g_sr, a_u2Data);
    g_pstBU64241_I2Cclient->ext_flag |= I2C_A_FILTER_MSG;
    i4RetValue = i2c_master_send(g_pstBU64241_I2Cclient, puSendCmd, 2);
	
    if (i4RetValue < 0) 
    {
        BU64241DB("[BU64241] I2C send failed!! \n");
        return -1;
    }

    return 0;
}

inline static int getBU64241Info(__user stBU64241_MotorInfo * pstMotorInfo)
{
    stBU64241_MotorInfo stMotorInfo;
    stMotorInfo.u4MacroPosition   = g_u4BU64241_MACRO;
    stMotorInfo.u4InfPosition     = g_u4BU64241_INF;
    stMotorInfo.u4CurrentPosition = g_u4CurrPosition;
    stMotorInfo.bIsSupportSR      = TRUE;

	if (g_i4MotorStatus == 1)	{stMotorInfo.bIsMotorMoving = 1;}
	else						{stMotorInfo.bIsMotorMoving = 0;}

	if (g_s4BU64241_Opened >= 1)	{stMotorInfo.bIsMotorOpen = 1;}
	else						{stMotorInfo.bIsMotorOpen = 0;}

    if(copy_to_user(pstMotorInfo , &stMotorInfo , sizeof(stBU64241_MotorInfo)))
    {
        BU64241DB("[BU64241] copy to user failed when getting motor information \n");
    }

    return 0;
}

inline static int moveBU64241(unsigned long a_u4Position)
{
    int ret = 0;
    
    if((a_u4Position > g_u4BU64241_MACRO) || (a_u4Position < g_u4BU64241_INF))
    {
        BU64241DB("[BU64241] out of range \n");
        return -EINVAL;
    }

    if (g_s4BU64241_Opened == 1)
    {
        unsigned short InitPos;
        ret = s4BU64241_ReadReg(&InitPos);
	    
        spin_lock(&g_BU64241_SpinLock);
        if(ret == 0)
        {
            BU64241DB("[BU64241] Init Pos %6d \n", InitPos);
            g_u4CurrPosition = (unsigned long)InitPos;
        }
        else
        {		
            g_u4CurrPosition = 0;
        }
        g_s4BU64241_Opened = 2;
        spin_unlock(&g_BU64241_SpinLock);
    }

    if (g_u4CurrPosition < a_u4Position)
    {
        spin_lock(&g_BU64241_SpinLock);	
        g_i4Dir = 1;
        spin_unlock(&g_BU64241_SpinLock);	
    }
    else if (g_u4CurrPosition > a_u4Position)
    {
        spin_lock(&g_BU64241_SpinLock);	
        g_i4Dir = -1;
        spin_unlock(&g_BU64241_SpinLock);			
    }
    else										{return 0;}

    spin_lock(&g_BU64241_SpinLock);    
    g_u4TargetPosition = a_u4Position;
    spin_unlock(&g_BU64241_SpinLock);	

    //BU64241DB("[BU64241] move [curr] %d [target] %d\n", g_u4CurrPosition, g_u4TargetPosition);

            spin_lock(&g_BU64241_SpinLock);
            g_sr = 3;
            g_i4MotorStatus = 0;
            spin_unlock(&g_BU64241_SpinLock);	
		
            if(s4BU64241_WriteReg((unsigned short)g_u4TargetPosition) == 0)
            {
                spin_lock(&g_BU64241_SpinLock);		
                g_u4CurrPosition = (unsigned long)g_u4TargetPosition;
                spin_unlock(&g_BU64241_SpinLock);				
            }
            else
            {
                BU64241DB("[BU64241] set I2C failed when moving the motor \n");			
                spin_lock(&g_BU64241_SpinLock);
                g_i4MotorStatus = -1;
                spin_unlock(&g_BU64241_SpinLock);				
            }

    return 0;
}

inline static int setBU64241Inf(unsigned long a_u4Position)
{
    spin_lock(&g_BU64241_SpinLock);
    g_u4BU64241_INF = a_u4Position;
    spin_unlock(&g_BU64241_SpinLock);	
    return 0;
}

inline static int setBU64241Macro(unsigned long a_u4Position)
{
    spin_lock(&g_BU64241_SpinLock);
    g_u4BU64241_MACRO = a_u4Position;
    spin_unlock(&g_BU64241_SpinLock);	
    return 0;	
}

////////////////////////////////////////////////////////////////
static long BU64241_Ioctl(
struct file * a_pstFile,
unsigned int a_u4Command,
unsigned long a_u4Param)
{
    long i4RetValue = 0;

    switch(a_u4Command)
    {
        case BU64241IOC_G_MOTORINFO :
            i4RetValue = getBU64241Info((__user stBU64241_MotorInfo *)(a_u4Param));
        break;

        case BU64241IOC_T_MOVETO :
            i4RetValue = moveBU64241(a_u4Param);
        break;
 
        case BU64241IOC_T_SETINFPOS :
            i4RetValue = setBU64241Inf(a_u4Param);
        break;

        case BU64241IOC_T_SETMACROPOS :
            i4RetValue = setBU64241Macro(a_u4Param);
        break;
		
        default :
      	    BU64241DB("[BU64241] No CMD \n");
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
static int BU64241_Open(struct inode * a_pstInode, struct file * a_pstFile)
{
    int  i4RetValue = 0;
    BU64241DB("[BU64241] BU64241_Open - Start\n");

    spin_lock(&g_BU64241_SpinLock);

    if(g_s4BU64241_Opened)
    {
        spin_unlock(&g_BU64241_SpinLock);
        BU64241DB("[BU64241] the device is opened \n");
        return -EBUSY;
    }

    g_s4BU64241_Opened = 1;
		
    spin_unlock(&g_BU64241_SpinLock);
	#if 0
char puSendCmd[2]={(char)(0xA8),(char)(0x4D)};//≤‚ ‘¥˙¬Î
    if (i4RetValue < 0) 
    {
        BU64241DB("[BU64241] I2C send failed!! \n");
        return -1;
    }
#else
char puSendCmd1[2]={(char)(0x94),(char)(0x64)};//A point=100d 
char puSendCmd2[2]={(char)(0x9C),(char)(0x8B)};//B point=139d 
char puSendCmd3[2]={(char)(0xA4),(char)(0xE4)};//A-B point step mode:200us/7Lsd 
char puSendCmd4[2]={(char)(0x8C),(char)(0x63)};//105Hz,Fastest mode

 i4RetValue = i2c_master_send(g_pstBU64241_I2Cclient, puSendCmd1, 2);	
    if (i4RetValue < 0) 
    {
        BU64241DB("[BU64241] I2C send failed!! \n");
        return -1;
    }
 i4RetValue = i2c_master_send(g_pstBU64241_I2Cclient, puSendCmd2, 2);
    if (i4RetValue < 0) 
    {
        BU64241DB("[BU64241] I2C send failed!! \n");
        return -1;
    }
 i4RetValue = i2c_master_send(g_pstBU64241_I2Cclient, puSendCmd3, 2);
    if (i4RetValue < 0) 
    {
        BU64241DB("[BU64241] I2C send failed!! \n");
        return -1;
    }
 i4RetValue = i2c_master_send(g_pstBU64241_I2Cclient, puSendCmd4, 2);
    if (i4RetValue < 0) 
    {
        BU64241DB("[BU64241] I2C send failed!! \n");
        return -1;
    }
#endif

   // BU64241DB("[BU64241] BU64241_Open - End\n");

    return 0;
}

//Main jobs:
// 1.Deallocate anything that "open" allocated in private_data.
// 2.Shut down the device on last close.
// 3.Only called once on last time.
// Q1 : Try release multiple times.
static int BU64241_Release(struct inode * a_pstInode, struct file * a_pstFile)
{
    BU64241DB("[BU64241] BU64241_Release - Start\n");

    if (g_s4BU64241_Opened)
    {
        BU64241DB("[BU64241] feee \n");
        g_sr = 5;
	    s4BU64241_WriteReg(200);
        msleep(10);
	    s4BU64241_WriteReg(100);
        msleep(10);
            	            	    	    
        spin_lock(&g_BU64241_SpinLock);
        g_s4BU64241_Opened = 0;
        spin_unlock(&g_BU64241_SpinLock);

    }

    BU64241DB("[BU64241] BU64241_Release - End\n");

    return 0;
}

static const struct file_operations g_stBU64241_fops = 
{
    .owner = THIS_MODULE,
    .open = BU64241_Open,
    .release = BU64241_Release,
    .unlocked_ioctl = BU64241_Ioctl
};

inline static int Register_BU64241_CharDrv(void)
{
    struct device* vcm_device = NULL;

    BU64241DB("[BU64241] Register_BU64241_CharDrv - Start\n");

    //Allocate char driver no.
    if( alloc_chrdev_region(&g_BU64241_devno, 0, 1,BU64241_DRVNAME) )
    {
        BU64241DB("[BU64241] Allocate device no failed\n");

        return -EAGAIN;
    }

    //Allocate driver
    g_pBU64241_CharDrv = cdev_alloc();

    if(NULL == g_pBU64241_CharDrv)
    {
        unregister_chrdev_region(g_BU64241_devno, 1);

        BU64241DB("[BU64241] Allocate mem for kobject failed\n");

        return -ENOMEM;
    }

    //Attatch file operation.
    cdev_init(g_pBU64241_CharDrv, &g_stBU64241_fops);

    g_pBU64241_CharDrv->owner = THIS_MODULE;

    //Add to system
    if(cdev_add(g_pBU64241_CharDrv, g_BU64241_devno, 1))
    {
        BU64241DB("[BU64241] Attatch file operation failed\n");

        unregister_chrdev_region(g_BU64241_devno, 1);

        return -EAGAIN;
    }

    actuator_class = class_create(THIS_MODULE, "actuatordrv1");
    if (IS_ERR(actuator_class)) {
        int ret = PTR_ERR(actuator_class);
        BU64241DB("Unable to create class, err = %d\n", ret);
        return ret;            
    }

    vcm_device = device_create(actuator_class, NULL, g_BU64241_devno, NULL, BU64241_DRVNAME);

    if(NULL == vcm_device)
    {
        return -EIO;
    }
    
    BU64241DB("[BU64241] Register_BU64241_CharDrv - End\n");    
    return 0;
}

inline static void Unregister_BU64241_CharDrv(void)
{
    BU64241DB("[BU64241] Unregister_BU64241_CharDrv - Start\n");

    //Release char driver
    cdev_del(g_pBU64241_CharDrv);

    unregister_chrdev_region(g_BU64241_devno, 1);
    
    device_destroy(actuator_class, g_BU64241_devno);

    class_destroy(actuator_class);

    BU64241DB("[BU64241] Unregister_BU64241_CharDrv - End\n");    
}

//////////////////////////////////////////////////////////////////////

static int BU64241_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int BU64241_i2c_remove(struct i2c_client *client);
static const struct i2c_device_id BU64241_i2c_id[] = {{BU64241_DRVNAME,0},{}};   
struct i2c_driver BU64241_i2c_driver = {                       
    .probe = BU64241_i2c_probe,                                   
    .remove = BU64241_i2c_remove,                           
    .driver.name = BU64241_DRVNAME,                 
    .id_table = BU64241_i2c_id,                             
};  

#if 0 
static int BU64241_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info) {         
    strcpy(info->type, BU64241_DRVNAME);                                                         
    return 0;                                                                                       
}      
#endif 
static int BU64241_i2c_remove(struct i2c_client *client) {
    return 0;
}


/* Kirby: add new-style driver {*/
static int BU64241_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int i4RetValue = 0;

    BU64241DB("[BU64241] BU64241_i2c_probe\n");

    /* Kirby: add new-style driver { */
    g_pstBU64241_I2Cclient = client;
    g_pstBU64241_I2Cclient->addr = BU64241_VCM_WRITE_ID;
    g_pstBU64241_I2Cclient->addr = g_pstBU64241_I2Cclient->addr >> 1;
    
    //Register char driver
    i4RetValue = Register_BU64241_CharDrv();

    if(i4RetValue){

        BU64241DB("[BU64241] register char device failed!\n");

        return i4RetValue;
    }

    spin_lock_init(&g_BU64241_SpinLock);

    BU64241DB("[BU64241] Attached!! \n");

    return 0;
}

static int BU64241_probe(struct platform_device *pdev)
{
    return i2c_add_driver(&BU64241_i2c_driver);
}

static int BU64241_remove(struct platform_device *pdev)
{
    i2c_del_driver(&BU64241_i2c_driver);
    return 0;
}

static int BU64241_suspend(struct platform_device *pdev, pm_message_t mesg)
{
    return 0;
}

static int BU64241_resume(struct platform_device *pdev)
{
    return 0;
}

// platform structure
static struct platform_driver g_stBU64241_Driver = {
    .probe		= BU64241_probe,
    .remove	= BU64241_remove,
    .suspend	= BU64241_suspend,
    .resume	= BU64241_resume,
    .driver		= {
        .name	= "lens_actuator1",
        .owner	= THIS_MODULE,
    }
};

static int __init BU64241_i2C_init(void)
{
    i2c_register_board_info(LENS_I2C_BUSNUM, &kd_lens_dev, 1);
	
    if(platform_driver_register(&g_stBU64241_Driver)){
        BU64241DB("failed to register BU64241 driver\n");
        return -ENODEV;
    }

    return 0;
}

static void __exit BU64241_i2C_exit(void)
{
	platform_driver_unregister(&g_stBU64241_Driver);
}

module_init(BU64241_i2C_init);
module_exit(BU64241_i2C_exit);

MODULE_DESCRIPTION("BU64241 lens module driver");
MODULE_AUTHOR("KY Chen <ky.chen@Mediatek.com>");
MODULE_LICENSE("GPL");


