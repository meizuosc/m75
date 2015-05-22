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
#include "RUMBAAF.h"
#include "../camera/kd_camera_hw.h"

#define LENS_I2C_BUSNUM 0
static struct i2c_board_info __initdata kd_lens_dev={ I2C_BOARD_INFO("RUMBAAF", 0x18)};
 
#define RUMBAAF_DRVNAME "RUMBAAF"
#define RUMBAAF_VCM_WRITE_ID           0x18

#define RUMBAAF_DEBUG
#ifdef RUMBAAF_DEBUG
#define RUMBAAFDB printk
#else
#define RUMBAAFDB(x,...)
#endif
 
static spinlock_t g_RUMBAAF_SpinLock;

static struct i2c_client * g_pstRUMBAAF_I2Cclient = NULL;

static dev_t g_RUMBAAF_devno;
static struct cdev * g_pRUMBAAF_CharDrv = NULL;
static struct class *actuator_class = NULL;

static int  g_s4RUMBAAF_Opened = 0;
static long g_i4MotorStatus = 0;
static long g_i4Dir = 0;
static unsigned long g_u4RUMBAAF_INF = 0;
static unsigned long g_u4RUMBAAF_MACRO = 1023;
static unsigned long g_u4TargetPosition = 0;
static unsigned long g_u4CurrPosition   = 0;

static int g_sr = 3;

static int s4RUMBAAF_ReadReg( unsigned short RamAddr, unsigned char *RegData )
{
    int  i4RetValue = 0;
    char pBuff[2] = {(char)(RamAddr >> 8) , (char)(RamAddr & 0xFF)};

    g_pstRUMBAAF_I2Cclient->addr = (0x48 >> 1);
    g_pstRUMBAAF_I2Cclient->ext_flag |= I2C_A_FILTER_MSG;

    i4RetValue = i2c_master_send(g_pstRUMBAAF_I2Cclient, pBuff, 2);
    if (i4RetValue < 0 ) 
    {
        RUMBAAFDB("[CAMERA SENSOR] read I2C send failed!!\n");
        return -1;
    }

    i4RetValue = i2c_master_recv(g_pstRUMBAAF_I2Cclient, (u8*)RegData, 1);
    RUMBAAFDB("[RUMBAAFDB]I2C r (%x %x) \n",RamAddr,*RegData);
    if (i4RetValue != 1) 
    {
        RUMBAAFDB("[CAMERA SENSOR] I2C read failed!! \n");
        return  -1;
    }
    return 0;
}

static int s4RUMBAAF_WriteReg(u16 a_u2Add, u16 a_u2Data)
{
    int  i4RetValue = 0;
    char puSendCmd[3] = {(char)(a_u2Add>>8), (char)(a_u2Add&0xFF), (char)(a_u2Data&0xFF)};
    g_pstRUMBAAF_I2Cclient->addr = (0x48 >> 1);
    g_pstRUMBAAF_I2Cclient->ext_flag |= I2C_A_FILTER_MSG;
    i4RetValue = i2c_master_send(g_pstRUMBAAF_I2Cclient, puSendCmd, 3);
    if (i4RetValue < 0) 
    {
        RUMBAAFDB("[RUMBAAF]1 I2C send failed!! %d\n",a_u2Add);
        return -1;
    }

    return 0;
}

static int s4RUMBAAF_WriteReg2(u16 a_u2Add, u16 a_u2Data)
{
    int  i4RetValue = 0;
    char puSendCmd[4] = {(char)(a_u2Add>>8), (char)(a_u2Add&0xFF), (char)(a_u2Data>>8), (char)(a_u2Data&0xFF)};
    g_pstRUMBAAF_I2Cclient->addr = (0x48 >> 1);
        g_pstRUMBAAF_I2Cclient->ext_flag |= I2C_A_FILTER_MSG;
    i4RetValue = i2c_master_send(g_pstRUMBAAF_I2Cclient, puSendCmd, 4);
    if (i4RetValue < 0) 
    {
        RUMBAAFDB("[RUMBAAF]1 I2C send failed!! %d\n",a_u2Add);
        return -1;
    }
    return 0;
}
static int s4RUMBAAF_SrWriteReg(u16 a_u2Add, u16 a_u2Data)
{
    int  i4RetValue = 0;
    char puSendCmd[3] = { (char)(a_u2Add>>8),  (char)(a_u2Add&0xFF),(char)(a_u2Data&0xFF) };
    g_pstRUMBAAF_I2Cclient->addr = (0x20 >> 1);
    i4RetValue = i2c_master_send(g_pstRUMBAAF_I2Cclient, puSendCmd, 3);
    if (i4RetValue < 0) 
    {
        RUMBAAFDB("[RUMBAAF]1 I2C sr send failed!! 1\n");
        return -1;
    }
    return 0;
}



inline static int getRUMBAAFInfo(__user stRUMBAAF_MotorInfo * pstMotorInfo)
{
    stRUMBAAF_MotorInfo stMotorInfo;
    stMotorInfo.u4MacroPosition   = g_u4RUMBAAF_MACRO;
    stMotorInfo.u4InfPosition     = g_u4RUMBAAF_INF;
    stMotorInfo.u4CurrentPosition = g_u4CurrPosition;
    stMotorInfo.bIsSupportSR      = TRUE;

    if (g_i4MotorStatus == 1)    {stMotorInfo.bIsMotorMoving = 1;}
    else                        {stMotorInfo.bIsMotorMoving = 0;}

    if (g_s4RUMBAAF_Opened >= 1)    {stMotorInfo.bIsMotorOpen = 1;}
    else                        {stMotorInfo.bIsMotorOpen = 0;}

    if(copy_to_user(pstMotorInfo , &stMotorInfo , sizeof(stRUMBAAF_MotorInfo)))
    {
        RUMBAAFDB("[RUMBAAF] copy to user failed when getting motor information \n");
    }

    return 0;
}

#ifdef LensdrvCM3
inline static int getRUMBAAFMETA(__user stRUMBAAF_MotorMETAInfo * pstMotorMETAInfo)
{
    stRUMBAAF_MotorMETAInfo stMotorMETAInfo;
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
    
    if(copy_to_user(pstMotorMETAInfo , &stMotorMETAInfo , sizeof(stRUMBAAF_MotorMETAInfo)))
    {
        RUMBAAFDB("[RUMBAAF] copy to user failed when getting motor information \n");
    }

    return 0;
}
#endif

inline static int moveRUMBAAF(unsigned long a_u4Position)
{
    int ret = 0;
    unsigned char posL,posH;
    unsigned char InitPos;
    if((a_u4Position > g_u4RUMBAAF_MACRO) || (a_u4Position < g_u4RUMBAAF_INF))
    {
        RUMBAAFDB("[RUMBAAF] out of range \n");
        return -EINVAL;
    }

    if (g_s4RUMBAAF_Opened == 1)
    {
        s4RUMBAAF_ReadReg(0x0062, &posL);
        ret = s4RUMBAAF_ReadReg(0x0063, &posH);
        InitPos = (posH<<8) + posL;
        if(ret == 0)
        {
            RUMBAAFDB("[RUMBAAF] Init Pos %6d \n", InitPos);
            
            spin_lock(&g_RUMBAAF_SpinLock);
            g_u4CurrPosition = (unsigned long)InitPos;
            spin_unlock(&g_RUMBAAF_SpinLock);
        }
        else
        {    
            spin_lock(&g_RUMBAAF_SpinLock);
            g_u4CurrPosition = 0;
            spin_unlock(&g_RUMBAAF_SpinLock);
        }

        spin_lock(&g_RUMBAAF_SpinLock);
        g_s4RUMBAAF_Opened = 2;
        spin_unlock(&g_RUMBAAF_SpinLock);
    }

    if (g_u4CurrPosition < a_u4Position)
    {
        spin_lock(&g_RUMBAAF_SpinLock);    
        g_i4Dir = 1;
        spin_unlock(&g_RUMBAAF_SpinLock);    
    }
    else if (g_u4CurrPosition > a_u4Position)
    {
        spin_lock(&g_RUMBAAF_SpinLock);    
        g_i4Dir = -1;
        spin_unlock(&g_RUMBAAF_SpinLock);            
    }
    else                                        {return 0;}

    spin_lock(&g_RUMBAAF_SpinLock);    
    g_u4TargetPosition = a_u4Position;
    spin_unlock(&g_RUMBAAF_SpinLock);    

    //RUMBAAFDB("[RUMBAAF] move [curr] %d [target] %d\n", g_u4CurrPosition, g_u4TargetPosition);

    spin_lock(&g_RUMBAAF_SpinLock);
    g_sr = 3;
    g_i4MotorStatus = 0;
    spin_unlock(&g_RUMBAAF_SpinLock);    

    posL =(unsigned short)(g_u4TargetPosition&0xFF);
    posH =(unsigned short)((g_u4TargetPosition>>8)&0xFF);
    //RUMBAAFDB("[RUMBAAF] moving the motor %d\n", g_u4TargetPosition);
    if(s4RUMBAAF_WriteReg2(0x0062, (u16)((posL<<8) + posH) )==0) //0x48    0x0063     0xMM
    {
        spin_lock(&g_RUMBAAF_SpinLock);        
        g_u4CurrPosition = (unsigned long)g_u4TargetPosition;
        spin_unlock(&g_RUMBAAF_SpinLock);                
    }
    else
    {
        RUMBAAFDB("[RUMBAAF] set I2C failed when moving the motor \n");            
        spin_lock(&g_RUMBAAF_SpinLock);
        g_i4MotorStatus = -1;
        spin_unlock(&g_RUMBAAF_SpinLock);                
    }

    return 0;
}

inline static int setRUMBAAFInf(unsigned long a_u4Position)
{
    spin_lock(&g_RUMBAAF_SpinLock);
    g_u4RUMBAAF_INF = a_u4Position;
    spin_unlock(&g_RUMBAAF_SpinLock);    
    return 0;
}

inline static int setRUMBAAFMacro(unsigned long a_u4Position)
{
    spin_lock(&g_RUMBAAF_SpinLock);
    g_u4RUMBAAF_MACRO = a_u4Position;
    spin_unlock(&g_RUMBAAF_SpinLock);    
    return 0;    
}

////////////////////////////////////////////////////////////////
static long RUMBAAF_Ioctl(
struct file * a_pstFile,
unsigned int a_u4Command,
unsigned long a_u4Param)
{
    long i4RetValue = 0;

    switch(a_u4Command)
    {
        case RUMBAAFIOC_G_MOTORINFO :
            i4RetValue = getRUMBAAFInfo((__user stRUMBAAF_MotorInfo *)(a_u4Param));
        break;
        #ifdef LensdrvCM3
        case RUMBAAFIOC_G_MOTORMETAINFO :
            i4RetValue = getRUMBAAFMETA((__user stRUMBAAF_MotorMETAInfo *)(a_u4Param));
        break;
        #endif
        case RUMBAAFIOC_T_MOVETO :
            i4RetValue = moveRUMBAAF(a_u4Param);
        break;
 
        case RUMBAAFIOC_T_SETINFPOS :
            i4RetValue = setRUMBAAFInf(a_u4Param);
        break;

        case RUMBAAFIOC_T_SETMACROPOS :
            i4RetValue = setRUMBAAFMacro(a_u4Param);
        break;
        
        default :
              RUMBAAFDB("[RUMBAAF] No CMD \n");
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
static int RUMBAAF_Open(struct inode * a_pstInode, struct file * a_pstFile)
{
    RUMBAAFDB("[RUMBAAF] RUMBAAF_Open - Start\n");
    if(g_s4RUMBAAF_Opened)
    {
        RUMBAAFDB("[RUMBAAF] the device is opened \n");
        return -EBUSY;
    }
    
    spin_lock(&g_RUMBAAF_SpinLock);
    g_s4RUMBAAF_Opened = 1;
    spin_unlock(&g_RUMBAAF_SpinLock);
    //power on AF

        //power on OIS
    s4RUMBAAF_SrWriteReg(0x3104, 0x00);//0x3104          0x00      ( Flash Pin enable )
    s4RUMBAAF_SrWriteReg(0x304F, 0x0D);//0x304F          0x0D      (Set Reset Low / Disable OIS)
    msleep(10);
    s4RUMBAAF_SrWriteReg(0x304F, 0x0E);//0x304F          0x0E      (Set Reset High /  Enable OIS ) 
    msleep(10);

    s4RUMBAAF_WriteReg(0x0061,0x02);//0x48      0x0061     0x02 ring control enable/ linear current mode
    s4RUMBAAF_WriteReg(0x0060,0x01);//0x48    0x0060     0x01 enable
   
    RUMBAAFDB("[RUMBAAF] RUMBAAF_Open - End\n");

    return 0;
}

//Main jobs:
// 1.Deallocate anything that "open" allocated in private_data.
// 2.Shut down the device on last close.
// 3.Only called once on last time.
// Q1 : Try release multiple times.
static int RUMBAAF_Release(struct inode * a_pstInode, struct file * a_pstFile)
{
    RUMBAAFDB("[RUMBAAF] RUMBAAF_Release - Start\n");

    if (g_s4RUMBAAF_Opened)
    {
        RUMBAAFDB("[RUMBAAF] feee \n");
        g_sr = 5;
        s4RUMBAAF_WriteReg2(0x0062, 200 );
        msleep(10);
        s4RUMBAAF_WriteReg2(0x0062, 100 );
        msleep(10);

        //power off AF
        s4RUMBAAF_WriteReg(0x0060,0x00);//0x48    0x0060     0x01 enable
        s4RUMBAAF_WriteReg(0x0061,0x00);//0x48    0x0061     0x02 ring control enable/ linear current mode

        //power off OIS
        s4RUMBAAF_SrWriteReg(0x3104, 0x00);//0x3104          0x00      ( Flash Pin enable )
        s4RUMBAAF_SrWriteReg(0x304F, 0x0D);//0x304F          0x0D      (Set Reset Low / Disable OIS)        
        
        spin_lock(&g_RUMBAAF_SpinLock);
        g_s4RUMBAAF_Opened = 0;
        spin_unlock(&g_RUMBAAF_SpinLock);

    }

    RUMBAAFDB("[RUMBAAF] RUMBAAF_Release - End\n");

    return 0;
}

static const struct file_operations g_stRUMBAAF_fops = 
{
    .owner = THIS_MODULE,
    .open = RUMBAAF_Open,
    .release = RUMBAAF_Release,
    .unlocked_ioctl = RUMBAAF_Ioctl
};

inline static int Register_RUMBAAF_CharDrv(void)
{
    struct device* vcm_device = NULL;

    RUMBAAFDB("[RUMBAAF] Register_RUMBAAF_CharDrv - Start\n");

    //Allocate char driver no.
    if( alloc_chrdev_region(&g_RUMBAAF_devno, 0, 1,RUMBAAF_DRVNAME) )
    {
        RUMBAAFDB("[RUMBAAF] Allocate device no failed\n");

        return -EAGAIN;
    }

    //Allocate driver
    g_pRUMBAAF_CharDrv = cdev_alloc();

    if(NULL == g_pRUMBAAF_CharDrv)
    {
        unregister_chrdev_region(g_RUMBAAF_devno, 1);

        RUMBAAFDB("[RUMBAAF] Allocate mem for kobject failed\n");

        return -ENOMEM;
    }

    //Attatch file operation.
    cdev_init(g_pRUMBAAF_CharDrv, &g_stRUMBAAF_fops);

    g_pRUMBAAF_CharDrv->owner = THIS_MODULE;

    //Add to system
    if(cdev_add(g_pRUMBAAF_CharDrv, g_RUMBAAF_devno, 1))
    {
        RUMBAAFDB("[RUMBAAF] Attatch file operation failed\n");

        unregister_chrdev_region(g_RUMBAAF_devno, 1);

        return -EAGAIN;
    }

    actuator_class = class_create(THIS_MODULE, "actuatordrv");
    if (IS_ERR(actuator_class)) {
        int ret = PTR_ERR(actuator_class);
        RUMBAAFDB("Unable to create class, err = %d\n", ret);
        return ret;            
    }

    vcm_device = device_create(actuator_class, NULL, g_RUMBAAF_devno, NULL, RUMBAAF_DRVNAME);

    if(NULL == vcm_device)
    {
        return -EIO;
    }
    
    RUMBAAFDB("[RUMBAAF] Register_RUMBAAF_CharDrv - End\n");    
    return 0;
}

inline static void Unregister_RUMBAAF_CharDrv(void)
{
    RUMBAAFDB("[RUMBAAF] Unregister_RUMBAAF_CharDrv - Start\n");

    //Release char driver
    cdev_del(g_pRUMBAAF_CharDrv);

    unregister_chrdev_region(g_RUMBAAF_devno, 1);
    
    device_destroy(actuator_class, g_RUMBAAF_devno);

    class_destroy(actuator_class);

    RUMBAAFDB("[RUMBAAF] Unregister_RUMBAAF_CharDrv - End\n");    
}

//////////////////////////////////////////////////////////////////////

static int RUMBAAF_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int RUMBAAF_i2c_remove(struct i2c_client *client);
static const struct i2c_device_id RUMBAAF_i2c_id[] = {{RUMBAAF_DRVNAME,0},{}};   
struct i2c_driver RUMBAAF_i2c_driver = {                       
    .probe = RUMBAAF_i2c_probe,                                   
    .remove = RUMBAAF_i2c_remove,                           
    .driver.name = RUMBAAF_DRVNAME,                 
    .id_table = RUMBAAF_i2c_id,                             
};  

#if 0 
static int RUMBAAF_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info) {         
    strcpy(info->type, RUMBAAF_DRVNAME);                                                         
    return 0;                                                                                       
}      
#endif 
static int RUMBAAF_i2c_remove(struct i2c_client *client) {
    return 0;
}

/* Kirby: add new-style driver {*/
static int RUMBAAF_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int i4RetValue = 0;

    RUMBAAFDB("[RUMBAAF] RUMBAAF_i2c_probe\n");

    /* Kirby: add new-style driver { */
    g_pstRUMBAAF_I2Cclient = client;
    
    g_pstRUMBAAF_I2Cclient->addr = (0x18 >> 1);
    
    //Register char driver
    i4RetValue = Register_RUMBAAF_CharDrv();

    if(i4RetValue){

        RUMBAAFDB("[RUMBAAF] register char device failed!\n");

        return i4RetValue;
    }

    spin_lock_init(&g_RUMBAAF_SpinLock);

    RUMBAAFDB("[RUMBAAF] Attached!! \n");

    return 0;
}

static int RUMBAAF_probe(struct platform_device *pdev)
{
    return i2c_add_driver(&RUMBAAF_i2c_driver);
}

static int RUMBAAF_remove(struct platform_device *pdev)
{
    i2c_del_driver(&RUMBAAF_i2c_driver);
    return 0;
}

static int RUMBAAF_suspend(struct platform_device *pdev, pm_message_t mesg)
{
    return 0;
}

static int RUMBAAF_resume(struct platform_device *pdev)
{
    return 0;
}

// platform structure
static struct platform_driver g_stRUMBAAF_Driver = {
    .probe        = RUMBAAF_probe,
    .remove    = RUMBAAF_remove,
    .suspend    = RUMBAAF_suspend,
    .resume    = RUMBAAF_resume,
    .driver        = {
        .name    = "lens_actuator",
        .owner    = THIS_MODULE,
    }
};

static int __init RUMBAAF_i2C_init(void)
{
    i2c_register_board_info(LENS_I2C_BUSNUM, &kd_lens_dev, 1);
    
    if(platform_driver_register(&g_stRUMBAAF_Driver)){
        RUMBAAFDB("failed to register RUMBAAF driver\n");
        return -ENODEV;
    }

    return 0;
}

static void __exit RUMBAAF_i2C_exit(void)
{
    platform_driver_unregister(&g_stRUMBAAF_Driver);
}

module_init(RUMBAAF_i2C_init);
module_exit(RUMBAAF_i2C_exit);

MODULE_DESCRIPTION("RUMBAAF lens module driver");
MODULE_AUTHOR("KY Chen <ky.chen@Mediatek.com>");
MODULE_LICENSE("GPL");


