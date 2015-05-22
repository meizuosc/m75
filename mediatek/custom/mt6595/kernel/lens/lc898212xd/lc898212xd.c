/*
 * TDK tvclb850lba voice coil motor driver IC LC898212XD.
 *
 *
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/meizu-sys.h>
#include <asm/atomic.h>
#include "lc898212xd.h"
#include "../camera/kd_camera_hw.h"
#include "AfInit.h"
#include "AfSTMV.h"

#define LENS_I2C_BUSNUM 0
#define E2PROM_WRITE_ID 0xA8
#define E2PROM_SHARP_WRITE_ID 0xA0

#define LITEON_MODULE	0x01
#define SHARP_MODULE	0x02

static struct i2c_board_info __initdata kd_lens_dev={ I2C_BOARD_INFO("LC898212XD", 0x72)};

#define	Min_Pos		0
#define Max_Pos		1023

#define MAX_INFI	0x6400
#define MAX_MACRO	0x9C00

signed short	Hall_Max = 0x0000; // Please read INF position from EEPROM or OTP
signed short	Hall_Min = 0x0000; // Please read MACRO position from EEPROM or OTP

signed short	test_pos_16 = 0x0000;	/* 16bit vcm moveto set, just for test */
int		test_pos_10 = 0;	/* 10bit vcm moveto set, just for test */

#define LC898212XD_DRVNAME "LC898212XD"

#define LC898212XD_DEBUG
#ifdef LC898212XD_DEBUG
#define LC898212XDDB printk
#else
#define LC898212XDDB(x,...)
#endif

static spinlock_t g_LC898212XD_SpinLock;

static struct i2c_client * g_pstLC898212XD_I2Cclient = NULL;

static dev_t g_LC898212XD_devno;
static struct cdev * g_pLC898212XD_CharDrv = NULL;
static struct class *actuator_class = NULL;
static struct device *vcm_device = NULL;

static int  g_s4LC898212XD_Opened = 0;
static long g_i4MotorStatus = 0;
static unsigned long g_u4LC898212XD_INF = 0;
static unsigned long g_u4LC898212XD_MACRO = 1023;
static unsigned long g_u4TargetPosition = 0;
static unsigned long g_u4CurrPosition   = 0;

extern int camera_af_poweron(char *mode_name, BOOL on);
extern void RamReadA(unsigned short addr, unsigned short *data);

/*******************************************************************************
* WriteRegI2C
********************************************************************************/
int LC898212XD_WriteRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u16 i2cId)
{
    int  i4RetValue = 0;
    int retry = 3;

	spin_lock(&g_LC898212XD_SpinLock);
	g_pstLC898212XD_I2Cclient->addr = (i2cId >> 1);
	g_pstLC898212XD_I2Cclient->ext_flag = (g_pstLC898212XD_I2Cclient->ext_flag)&(~I2C_DMA_FLAG);
	spin_unlock(&g_LC898212XD_SpinLock);

    do {
	i4RetValue = i2c_master_send(g_pstLC898212XD_I2Cclient, a_pSendData, a_sizeSendData);
	if (i4RetValue != a_sizeSendData) {
	    LC898212XDDB("[LC898212XD] I2C send failed!!, Addr = 0x%x, Data = 0x%x \n", a_pSendData[0], a_pSendData[1] );
	}
	else {
	    break;
	}
	udelay(50);
    } while ((retry--) > 0);

    return 0;
}

/*******************************************************************************
* ReadRegI2C
********************************************************************************/
int LC898212XD_ReadRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId)
{
	int  i4RetValue = 0;

	spin_lock(&g_LC898212XD_SpinLock);
	g_pstLC898212XD_I2Cclient->addr = (i2cId >> 1);
	g_pstLC898212XD_I2Cclient->ext_flag = (g_pstLC898212XD_I2Cclient->ext_flag)&(~I2C_DMA_FLAG);
	spin_unlock(&g_LC898212XD_SpinLock);

	i4RetValue = i2c_master_send(g_pstLC898212XD_I2Cclient, a_pSendData, a_sizeSendData);
	if (i4RetValue != a_sizeSendData) {
		LC898212XDDB("[LC898212XD] I2C send failed!!, Addr = 0x%x\n", a_pSendData[0]);
	        return -1;
	}

	i4RetValue = i2c_master_recv(g_pstLC898212XD_I2Cclient, (u8 *)a_pRecvData, a_sizeRecvData);
	if (i4RetValue != a_sizeRecvData) {
		LC898212XDDB("[LC898212XD] I2C read failed!! \n");
	        return -1;
	}

	return 0;
}

static int get_module_type()
{
	int ret = 0;
	u8 module_id = 0;
	u8 puSendCmd[2] = {0x00, 0x01};	/* module id reg addr = 0x0001 for imx220 liteon module */

	ret = LC898212XD_ReadRegI2C(puSendCmd , sizeof(puSendCmd), &module_id, 1, E2PROM_WRITE_ID);
	if ((ret < 0) || (module_id != 0x01))
		LC898212XDDB("[LC898212XD] I2C read liteon e2prom failed!! \n");
	else
		return LITEON_MODULE;

	/* module id reg addr = 0x0000 for imx220 sharp module */
	puSendCmd[0] = 0x00;
	puSendCmd[1] = 0x00;
	ret = LC898212XD_ReadRegI2C(puSendCmd , sizeof(puSendCmd), &module_id, 1, E2PROM_SHARP_WRITE_ID);
	if ((ret < 0) || (module_id != 0x02))
		LC898212XDDB("[LC898212XD] I2C read sharp e2prom failed!! \n");
	else
		return SHARP_MODULE;

	LC898212XDDB("[LC898212XD] get module type failed!! \n");
	return -ENODEV;
}

void E2PROMReadA_sharp(unsigned short addr, u8 *data)
{
	int ret = 0;

	u8 puSendCmd[2] = {(u8)(addr >> 8), (u8)(addr & 0xFF)};
	ret = LC898212XD_ReadRegI2C(puSendCmd , sizeof(puSendCmd), data, 1, E2PROM_SHARP_WRITE_ID);     
	if (ret < 0)
		LC898212XDDB("[LC898212XD] I2C read e2prom failed!! \n");

	return;
}

void E2PROMReadA(unsigned short addr, u8 *data)
{
	int ret = 0;

	u8 puSendCmd[2] = {(u8)(addr >> 8), (u8)(addr & 0xFF)};
	ret = LC898212XD_ReadRegI2C(puSendCmd , sizeof(puSendCmd), data, 1, E2PROM_WRITE_ID);     
	if (ret < 0)
		LC898212XDDB("[LC898212XD] I2C read e2prom failed!! \n");

	return;
}

inline static int getLC898212XDInfo(__user stLC898212XD_MotorInfo * pstMotorInfo)
{
    stLC898212XD_MotorInfo stMotorInfo;
    stMotorInfo.u4MacroPosition   = g_u4LC898212XD_MACRO;
    stMotorInfo.u4InfPosition     = g_u4LC898212XD_INF;
    stMotorInfo.u4CurrentPosition = g_u4CurrPosition;
    stMotorInfo.bIsSupportSR      = TRUE;

	if (g_i4MotorStatus == 1)	{stMotorInfo.bIsMotorMoving = 1;}
	else						{stMotorInfo.bIsMotorMoving = 0;}

	if (g_s4LC898212XD_Opened >= 1)	{stMotorInfo.bIsMotorOpen = 1;}
	else						{stMotorInfo.bIsMotorOpen = 0;}

    if(copy_to_user(pstMotorInfo , &stMotorInfo , sizeof(stLC898212XD_MotorInfo)))
    {
        LC898212XDDB("[LC898212XD] copy to user failed when getting motor information \n");
    }

    return 0;
}

#ifdef LensdrvCM3
inline static int getLC898212XDMETA(__user stFM50AF_MotorMETAInfo * pstMotorMETAInfo)
{
    stLC898212XD_MotorMETAInfo stMotorMETAInfo;
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
	
	if(copy_to_user(pstMotorMETAInfo , &stMotorMETAInfo , sizeof(stLC898212XD_MotorMETAInfo)))
	{
		LC898212XDDB("[LC898212XD] copy to user failed when getting motor information \n");
	}

    return 0;
}
#endif

int AF_reverse_convert(signed short position)
{
#if 0       // 1: INF -> Macro =  0x8001 -> 0x7FFF
	return (((unsigned short)(position - Hall_Min) * (Max_Pos - Min_Pos) / (unsigned short)(Hall_Max - Hall_Min)) + Min_Pos);
#else  // 0: INF -> Macro =  0x7FFF -> 0x8001
	return (Max_Pos - ((unsigned short)(position - Hall_Min) * (Max_Pos - Min_Pos) / (unsigned short)(Hall_Max - Hall_Min)));
#endif
}

unsigned short AF_convert(int position)
{
#if 0       // 1: INF -> Macro =  0x8001 -> 0x7FFF
    return (((position - Min_Pos) * (unsigned short)(Hall_Max - Hall_Min) / (Max_Pos - Min_Pos)) + Hall_Min) & 0xFFFF;
#else  // 0: INF -> Macro =  0x7FFF -> 0x8001
	return (((Max_Pos - position) * (unsigned short)(Hall_Max - Hall_Min) / (Max_Pos - Min_Pos)) + Hall_Min) & 0xFFFF;
#endif
}

inline static int moveLC898212XD(unsigned long a_u4Position)
{
	StmvTo( AF_convert(a_u4Position) ) ;	// Move to Target Position

	spin_lock(&g_LC898212XD_SpinLock);
	g_u4CurrPosition = (unsigned long)a_u4Position;
	spin_unlock(&g_LC898212XD_SpinLock);
	return 0;
}

inline static int setLC898212XDInf(unsigned long a_u4Position)
{
    spin_lock(&g_LC898212XD_SpinLock);
    g_u4LC898212XD_INF = a_u4Position;
    spin_unlock(&g_LC898212XD_SpinLock);	
    return 0;
}

inline static int setLC898212XDMacro(unsigned long a_u4Position)
{
    spin_lock(&g_LC898212XD_SpinLock);
    g_u4LC898212XD_MACRO = a_u4Position;
    spin_unlock(&g_LC898212XD_SpinLock);	
    return 0;	
}

////////////////////////////////////////////////////////////////
static long LC898212XD_Ioctl(
struct file * a_pstFile,
unsigned int a_u4Command,
unsigned long a_u4Param)
{
    long i4RetValue = 0;

    switch(a_u4Command)
    {
        case LC898212XDIOC_G_MOTORINFO :
            i4RetValue = getLC898212XDInfo((__user stLC898212XD_MotorInfo *)(a_u4Param));
        break;
		#ifdef LensdrvCM3
        case LC898212XDIOC_G_MOTORMETAINFO :
            i4RetValue = getLC898212XDMETA((__user stLC898212XD_MotorMETAInfo *)(a_u4Param));
        break;
		#endif
        case LC898212XDIOC_T_MOVETO :
		i4RetValue = moveLC898212XD(a_u4Param);
        break;
 
        case LC898212XDIOC_T_SETINFPOS :
            i4RetValue = setLC898212XDInf(a_u4Param);
        break;

        case LC898212XDIOC_T_SETMACROPOS :
            i4RetValue = setLC898212XDMacro(a_u4Param);
        break;
		
        default :
      	    LC898212XDDB("[LC898212XD] No CMD \n");
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
static int LC898212XD_Open(struct inode * a_pstInode, struct file * a_pstFile)
{
	stSmvPar StSmvPar;
	u8 val1 =0, val2 = 0;
	u8 module_id = 0;

	int HallOff = 0x00;	 	// Please Read Offset from EEPROM or OTP
	int HallBias = 0x00;   // Please Read Bias from EEPROM or OTP
    
	if(g_s4LC898212XD_Opened)
	{
		LC898212XDDB("[LC898212XD] the device is opened \n");
		return -EBUSY;
	}

	LC898212XDDB("[LC898212XD] LC898212XD_Open - Start\n");

	module_id = get_module_type();

	if (module_id == LITEON_MODULE) {
		E2PROMReadA(0x002A, &val1);
		E2PROMReadA(0x002B, &val2);
		Hall_Max = ((val1 << 8) | (val2 & 0x00FF)) & 0xFFFF;
		E2PROMReadA(0x0030, &val1);
		E2PROMReadA(0x0031, &val2);
		Hall_Min = ((val1 << 8) | (val2 & 0x00FF)) & 0xFFFF;
		E2PROMReadA(0x0032, &val1);
		E2PROMReadA(0x0033, &val2);
		HallOff = val2;
		HallBias = val1;
	} else if (module_id == SHARP_MODULE) {
		E2PROMReadA_sharp(0x0020, &val1);
		E2PROMReadA_sharp(0x002B, &val2);
		/* read from vcm1 */
		if (((val1 >> 6) & 0x03) == 0x01) {
			E2PROMReadA_sharp(0x0026, &val1);
			E2PROMReadA_sharp(0x0025, &val2);
			Hall_Max = ((val1 << 8) | (val2 & 0x00FF)) & 0xFFFF;
			E2PROMReadA_sharp(0x0028, &val1);
			E2PROMReadA_sharp(0x0027, &val2);
			Hall_Min = ((val1 << 8) | (val2 & 0x00FF)) & 0xFFFF;
			E2PROMReadA_sharp(0x0029, &val1);
			E2PROMReadA_sharp(0x002A, &val2);
			HallOff = val2;
			HallBias = val1;
		} else if (((val1 >> 6) & 0x03) == 0x02 &&
			   ((val2 >> 6) & 0x03) == 0x01) {
			E2PROMReadA_sharp(0x0031, &val1);
			E2PROMReadA_sharp(0x0030, &val2);
			Hall_Max = ((val1 << 8) | (val2 & 0x00FF)) & 0xFFFF;
			E2PROMReadA_sharp(0x0033, &val1);
			E2PROMReadA_sharp(0x0032, &val2);
			Hall_Min = ((val1 << 8) | (val2 & 0x00FF)) & 0xFFFF;
			E2PROMReadA_sharp(0x0034, &val1);
			E2PROMReadA_sharp(0x0035, &val2);
			HallOff = val2;
			HallBias = val1;
		}
	}
	AfInit( HallOff,  HallBias);	// Initialize driver IC

	// Step move parameter set
	StSmvPar.UsSmvSiz	= STMV_SIZE ;
	StSmvPar.UcSmvItv	= STMV_INTERVAL ;
	StSmvPar.UcSmvEnb	= STMCHTG_SET | STMSV_SET | STMLFF_SET ;
	StmvSet( StSmvPar ) ;
	
	ServoOn();	// Close loop ON

	spin_lock(&g_LC898212XD_SpinLock);
	g_s4LC898212XD_Opened = 1;
	spin_unlock(&g_LC898212XD_SpinLock);

	LC898212XDDB("[LC898212XD] LC898212XD_Open - End\n");

	return 0;
}

//Main jobs:
// 1.Deallocate anything that "open" allocated in private_data.
// 2.Shut down the device on last close.
// 3.Only called once on last time.
// Q1 : Try release multiple times.
static int LC898212XD_Release(struct inode * a_pstInode, struct file * a_pstFile)
{
    LC898212XDDB("[LC898212XD] LC898212XD_Release - Start\n");

    if (g_s4LC898212XD_Opened)
    {
        spin_lock(&g_LC898212XD_SpinLock);
        g_s4LC898212XD_Opened = 0;
        spin_unlock(&g_LC898212XD_SpinLock);

    }

    LC898212XDDB("[LC898212XD] LC898212XD_Release - End\n");

    return 0;
}

static const struct file_operations g_stLC898212XD_fops = 
{
    .owner = THIS_MODULE,
    .open = LC898212XD_Open,
    .release = LC898212XD_Release,
    .unlocked_ioctl = LC898212XD_Ioctl
};

static int lc898212xd_init(void)
{
	stSmvPar StSmvPar;
	u8 val1 =0, val2 = 0;
	u8 module_id = 0;

	int HallOff = 0x00;	 	// Please Read Offset from EEPROM or OTP
	int HallBias = 0x00;   // Please Read Bias from EEPROM or OTP
    
	LC898212XDDB("[LC898212XD] LC898212XD_init - Start\n");

	module_id = get_module_type();

	if (module_id == LITEON_MODULE) {
		E2PROMReadA(0x002A, &val1);
		E2PROMReadA(0x002B, &val2);
		Hall_Max = ((val1 << 8) | (val2 & 0x00FF)) & 0xFFFF;
		E2PROMReadA(0x0030, &val1);
		E2PROMReadA(0x0031, &val2);
		Hall_Min = ((val1 << 8) | (val2 & 0x00FF)) & 0xFFFF;
		E2PROMReadA(0x0032, &val1);
		E2PROMReadA(0x0033, &val2);
		HallOff = val2;
		HallBias = val1;
	} else if (module_id == SHARP_MODULE) {
		E2PROMReadA_sharp(0x0020, &val1);
		E2PROMReadA_sharp(0x002B, &val2);
		/* read from vcm1 */
		if (((val1 >> 6) & 0x03) == 0x01) {
			E2PROMReadA_sharp(0x0026, &val1);
			E2PROMReadA_sharp(0x0025, &val2);
			Hall_Max = ((val1 << 8) | (val2 & 0x00FF)) & 0xFFFF;
			E2PROMReadA_sharp(0x0028, &val1);
			E2PROMReadA_sharp(0x0027, &val2);
			Hall_Min = ((val1 << 8) | (val2 & 0x00FF)) & 0xFFFF;
			E2PROMReadA_sharp(0x0029, &val1);
			E2PROMReadA_sharp(0x002A, &val2);
			HallOff = val2;
			HallBias = val1;
		} else if (((val1 >> 6) & 0x03) == 0x02 &&
			   ((val2 >> 6) & 0x03) == 0x01) {
			E2PROMReadA_sharp(0x0031, &val1);
			E2PROMReadA_sharp(0x0030, &val2);
			Hall_Max = ((val1 << 8) | (val2 & 0x00FF)) & 0xFFFF;
			E2PROMReadA_sharp(0x0033, &val1);
			E2PROMReadA_sharp(0x0032, &val2);
			Hall_Min = ((val1 << 8) | (val2 & 0x00FF)) & 0xFFFF;
			E2PROMReadA_sharp(0x0034, &val1);
			E2PROMReadA_sharp(0x0035, &val2);
			HallOff = val2;
			HallBias = val1;
		}
	}
	AfInit( HallOff,  HallBias);	// Initialize driver IC

	// Step move parameter set
	StSmvPar.UsSmvSiz	= STMV_SIZE ;
	StSmvPar.UcSmvItv	= STMV_INTERVAL ;
	StSmvPar.UcSmvEnb	= STMCHTG_SET | STMSV_SET | STMLFF_SET ;
	StmvSet( StSmvPar ) ;
	
	ServoOn();	// Close loop ON

	LC898212XDDB("[LC898212XD] LC898212XD_init - End\n");

	return 0;
}

static ssize_t af_range_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	char *p = buf;
	int min_pos, max_pos;

	if(g_s4LC898212XD_Opened)
	{
		LC898212XDDB("[LC898212XD] the device is opened. called by %s \n", __func__);
	} else {
		/* Camera AF power on */
		camera_af_poweron(LC898212XD_DRVNAME, 1);

		/* initialize camera af */
		lc898212xd_init();
	}

	min_pos = AF_reverse_convert(MAX_INFI);
	max_pos = AF_reverse_convert(MAX_MACRO);
	p += sprintf(p, "Min AF position: %d\n", min_pos);
	p += sprintf(p, "Max AF position: %d\n", max_pos);

	if(g_s4LC898212XD_Opened == 0)
		camera_af_poweron(LC898212XD_DRVNAME, 0);

	return (p - buf);
}

static ssize_t af_range_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	return count;
}

static ssize_t af_active_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	char *p = buf;
	unsigned short pos;
	int real_pos_10;

	if ((test_pos_10 < 0) || (test_pos_10 > 1023)) {
		p += sprintf(p, "Invalid AF move position!\n");
	} else {
		RamReadA(0x3C,	&pos);	/* Get Position */
		real_pos_10 = AF_reverse_convert(pos);

		if(g_s4LC898212XD_Opened == 0)
			camera_af_poweron(LC898212XD_DRVNAME, 0);

		p += sprintf(p, "current AF position: %d\n", real_pos_10);

		/* check whether VCM moveto is blocked */
		if (((signed short)pos >= (test_pos_16 - 0x200))
		    && ((signed short)pos <= (test_pos_16 + 0x200)))
			p += sprintf(p, "PASS:AF move is OK.\n");
		else
			p += sprintf(p, "NO PASS:AF move is not OK, maybe blocked!\n");
	}

	return (p - buf);
}

static ssize_t af_active_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int val;

	sscanf(buf, "%d", &val);
	test_pos_10 = val;
	if (val < 0 || val > 1023) {
		LC898212XDDB("[LC898212XD] invalid test moveto position\n");
		return -EINVAL;
	}
	test_pos_16 = (signed short)AF_convert(val);	/* 10bit ADC exchange to 16bit ADC value */

	if(g_s4LC898212XD_Opened)
	{
		LC898212XDDB("[LC898212XD] the device is opened. called by %s \n", __func__);
	} else {
		/* Camera AF power on */
		camera_af_poweron(LC898212XD_DRVNAME, 1);

		/* initialize camera af */
		lc898212xd_init();
	}

	moveLC898212XD(val);
	/* enough settle time for vcm moveto */
	msleep(50);

	return count;
}

static struct device_attribute dev_attr_af_active = {
	.attr = {.name = "camera_af_active", .mode = 0644},
	.show = af_active_show,
	.store = af_active_store,
};

static struct device_attribute dev_attr_af_range = {
	.attr = {.name = "camera_af_range", .mode = 0644},
	.show = af_range_show,
	.store = af_range_store,
};

inline static int Register_LC898212XD_CharDrv(void)
{
    //Allocate char driver no.
    if( alloc_chrdev_region(&g_LC898212XD_devno, 0, 1,LC898212XD_DRVNAME) )
    {
        LC898212XDDB("[LC898212XD] Allocate device no failed\n");

        return -EAGAIN;
    }

    //Allocate driver
    g_pLC898212XD_CharDrv = cdev_alloc();

    if(NULL == g_pLC898212XD_CharDrv)
    {
        unregister_chrdev_region(g_LC898212XD_devno, 1);

        LC898212XDDB("[LC898212XD] Allocate mem for kobject failed\n");

        return -ENOMEM;
    }

    //Attatch file operation.
    cdev_init(g_pLC898212XD_CharDrv, &g_stLC898212XD_fops);

    g_pLC898212XD_CharDrv->owner = THIS_MODULE;

    //Add to system
    if(cdev_add(g_pLC898212XD_CharDrv, g_LC898212XD_devno, 1))
    {
        LC898212XDDB("[LC898212XD] Attatch file operation failed\n");

        unregister_chrdev_region(g_LC898212XD_devno, 1);

        return -EAGAIN;
    }

    actuator_class = class_create(THIS_MODULE, "actuatordrv");
    if (IS_ERR(actuator_class)) {
        int ret = PTR_ERR(actuator_class);
        LC898212XDDB("Unable to create class, err = %d\n", ret);
        return ret;            
    }

    vcm_device = device_create(actuator_class, NULL, g_LC898212XD_devno, NULL, LC898212XD_DRVNAME);

    if(NULL == vcm_device)
    {
        return -EIO;
    }
    
	device_create_file(vcm_device, &dev_attr_af_active);
	device_create_file(vcm_device, &dev_attr_af_range);
	meizu_sysfslink_register(vcm_device);

    return 0;
}

inline static void Unregister_LC898212XD_CharDrv(void)
{
    LC898212XDDB("[LC898212XD] Unregister_LC898212XD_CharDrv - Start\n");

	meizu_sysfslink_unregister(vcm_device);
	device_remove_file(vcm_device, &dev_attr_af_range);
	device_remove_file(vcm_device, &dev_attr_af_active);

    //Release char driver
    cdev_del(g_pLC898212XD_CharDrv);

    unregister_chrdev_region(g_LC898212XD_devno, 1);
    
    device_destroy(actuator_class, g_LC898212XD_devno);

    class_destroy(actuator_class);

    LC898212XDDB("[LC898212XD] Unregister_LC898212XD_CharDrv - End\n");    
}

//////////////////////////////////////////////////////////////////////

static int LC898212XD_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int LC898212XD_i2c_remove(struct i2c_client *client);
static const struct i2c_device_id LC898212XD_i2c_id[] = {{LC898212XD_DRVNAME,0},{}};   
struct i2c_driver LC898212XD_i2c_driver = {                       
    .probe = LC898212XD_i2c_probe,                                   
    .remove = LC898212XD_i2c_remove,                           
    .driver.name = LC898212XD_DRVNAME,                 
    .id_table = LC898212XD_i2c_id,                             
};  

#if 0 
static int LC898212XD_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info) {         
    strcpy(info->type, LC898212XD_DRVNAME);                                                         
    return 0;                                                                                       
}      
#endif 
static int LC898212XD_i2c_remove(struct i2c_client *client) {
	Unregister_LC898212XD_CharDrv();
    return 0;
}

/* Kirby: add new-style driver {*/
static int LC898212XD_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int i4RetValue = 0;

    /* Kirby: add new-style driver { */
    g_pstLC898212XD_I2Cclient = client;
    
    g_pstLC898212XD_I2Cclient->addr = g_pstLC898212XD_I2Cclient->addr >> 1;
    
    //Register char driver
    i4RetValue = Register_LC898212XD_CharDrv();

    if(i4RetValue){

        LC898212XDDB("[LC898212XD] register char device failed!\n");

        return i4RetValue;
    }

    spin_lock_init(&g_LC898212XD_SpinLock);

    return 0;
}

static int LC898212XD_probe(struct platform_device *pdev)
{
    return i2c_add_driver(&LC898212XD_i2c_driver);
}

static int LC898212XD_remove(struct platform_device *pdev)
{
    i2c_del_driver(&LC898212XD_i2c_driver);
    return 0;
}

static int LC898212XD_suspend(struct platform_device *pdev, pm_message_t mesg)
{
    return 0;
}

static int LC898212XD_resume(struct platform_device *pdev)
{
    return 0;
}

// platform structure
static struct platform_driver g_stLC898212XD_Driver = {
    .probe		= LC898212XD_probe,
    .remove	= LC898212XD_remove,
    .suspend	= LC898212XD_suspend,
    .resume	= LC898212XD_resume,
    .driver		= {
        .name	= "lens_actuator",
        .owner	= THIS_MODULE,
    }
};

static int __init LC898212XD_i2C_init(void)
{
    i2c_register_board_info(LENS_I2C_BUSNUM, &kd_lens_dev, 1);
	
    if(platform_driver_register(&g_stLC898212XD_Driver)){
        LC898212XDDB("failed to register LC898212XD driver\n");
        return -ENODEV;
    }

    return 0;
}

static void __exit LC898212XD_i2C_exit(void)
{
	platform_driver_unregister(&g_stLC898212XD_Driver);
}

module_init(LC898212XD_i2C_init);
module_exit(LC898212XD_i2C_exit);

MODULE_DESCRIPTION("LC898212XD lens module driver");
MODULE_AUTHOR("KY Chen <ky.chen@Mediatek.com>");
MODULE_LICENSE("GPL");


