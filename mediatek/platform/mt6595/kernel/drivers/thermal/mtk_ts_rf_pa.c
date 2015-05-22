#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/dmi.h>
#include <linux/acpi.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <linux/aee.h>
#include <linux/xlog.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include <mach/system.h>
#include "mach/mtk_thermal_monitor.h"
#include "mach/mt_typedefs.h"
#include "mach/mt_thermal.h"
#include <mach/upmu_common_sw.h>
#include <mach/upmu_hw.h>

typedef struct{
    INT32 TeryTemp;
    INT32 TemperatureR;
}RFPA_TEMPERATURE;

extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int* rawdata);
extern int IMM_IsAdcInitReady(void);

#define AUX_IN1_NTC (12)
static int g_RAP_pull_up_R = 39000;
static int g_ntc_r_max = 188500; // -40 degree Celsius
static int g_ntc_r_min = 534;   // 125 degree Celsius
static int g_RAP_pull_up_voltage = 1800;
static int g_RAP_ntc_table = 7;  //default is NTCG103JF103F
static int g_RAP_ADC_channel = AUX_IN1_NTC;  //default is 2

static unsigned int interval = 1; /* seconds, 0 : no auto polling */
static unsigned int trip_temp[10] = {85000,80000,70000,60000,50000,40000,30000,20000,10000,5000};

static unsigned int cl_dev_sysrst_state = 0;
static struct thermal_zone_device *thz_dev;

static struct thermal_cooling_device *cl_dev_sysrst;
static int kernelmode = 0;

static int g_THERMAL_TRIP[10] = {0,0,0,0,0,0,0,0,0,0};
static int num_trip=1;
static char g_bind0[20]="rfpa-sysrst";
static char g_bind1[20]={0};
static char g_bind2[20]={0};
static char g_bind3[20]={0};
static char g_bind4[20]={0};
static char g_bind5[20]={0};
static char g_bind6[20]={0};
static char g_bind7[20]={0};
static char g_bind8[20]={0};
static char g_bind9[20]={0};

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
#define RFPA_TEMP_CRIT 85000 /* 85.000 degree Celsius */


#if 1
#define dprintk
#else
static int debug_log = 1;
#define dprintk(fmt, args...)   \
	do {									\
		if (debug_log) {				\
		xlog_printk(ANDROID_LOG_INFO, "Rfpa", fmt, ##args); \
	}								   \
} while(0)
#endif

static int g_TemperatureR = 0;
//NTCG103JF103F(10K)
RFPA_TEMPERATURE RFPA_Temperature_Table[] = {
	    {-40,188500},
	    {-35,144290},
	    {-30,111330},
	    {-25,86560},
	    {-20,67790},
	    {-15,53460},
	    {-10,42450},
	    { -5,33930},
	    {  0,27280},
	    {  5,22070},
	    { 10,17960},
	    { 15,14700},
	    { 20,12090},
	    { 25,10000},
	    { 30,8310},
	    { 35,6940},
	    { 40,5830},
	    { 45,4910},
	    { 50,4160},
	    { 55,3540},
	    { 60,3020},
	    { 65,2590},
	    { 70,2230},
	    { 75,1920},
	    { 80,1670},
	    { 85,1450},
	    { 90,1270},
	    { 95,1110},
	    { 100,975},
	    { 105,860},
	    { 110,760},
	    { 115,674},
	    { 120,599},
	    { 125,534},
};

/* convert register to temperature  */
static INT16 RfPatThermistorConverTemp(INT32 Res)
{
    int i=0;
    INT32 RES1=0,RES2=0;
    INT32 temperature=-200,TMP1=0,TMP2=0;

    if(Res>RFPA_Temperature_Table[0].TemperatureR)
    {
    	temperature = RFPA_Temperature_Table[0].TeryTemp;
    }
    else if(Res<RFPA_Temperature_Table[ARRAY_SIZE(RFPA_Temperature_Table)-1].TemperatureR)
    {
    	temperature = RFPA_Temperature_Table[ARRAY_SIZE(RFPA_Temperature_Table)-1].TeryTemp;
    }
    else
    {
        RES1=RFPA_Temperature_Table[0].TemperatureR;
        TMP1=RFPA_Temperature_Table[0].TeryTemp;

        for(i=0;i<ARRAY_SIZE(RFPA_Temperature_Table);i++)
        {
            if(Res>=RFPA_Temperature_Table[i].TemperatureR)
            {
                RES2=RFPA_Temperature_Table[i].TemperatureR;
                TMP2=RFPA_Temperature_Table[i].TeryTemp;
                break;
            }
            else
            {
                RES1=RFPA_Temperature_Table[i].TemperatureR;
                TMP1=RFPA_Temperature_Table[i].TeryTemp;
            }
        }

        temperature = (((Res-RES2)*TMP1)+((RES1-Res)*TMP2))/(RES1-RES2);
    }
    dprintk("@@@@@@@@@@@@@@@@@ [rfpa]RES1:%d,TMP1:%d,RES2:%d,TMP2:%d\n",RES1,TMP1,RES2,TMP2);
    dprintk("@@@@@@@@@@@@@@@@@ [rfpa]Res:%ld, TAP_Value:%d\n",Res,temperature);
    dprintk("@@@@@@@@@@@@@@@@@ [rfpa]Res:%ld, TAP_Value:%d\n",Res,temperature);
    dprintk("@@@@@@@@@@@@@@@@@ [rfpa]Res:%ld, TAP_Value:%d\n",Res,temperature);
    return temperature;
}

/* convert ADC_AP_temp_volt to register */
/*Volt to Temp formula same with 6589*/
static INT16 RFPaVoltToTemp(UINT32 dwVolt)
{
    INT32 TRes;
    INT32 vol_max = 0,vol_min = 0;
    INT32 sTemperature = -100;

    /*
     * when the temperature is -20 degree Celsius,
     * the voltage across the NTC will reach the maximum.
     *
     * +125 degree Celsius, min
     */
    vol_max = (g_ntc_r_max * g_RAP_pull_up_voltage) / (g_ntc_r_max + g_RAP_pull_up_R);
    vol_min = (g_ntc_r_min * g_RAP_pull_up_voltage) / (g_ntc_r_min + g_RAP_pull_up_R);

    if(dwVolt > vol_max) {

        TRes = g_ntc_r_max;
        printk("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
        printk("%s [Warnning] the temperature measured is not narmal,< -40 oC!\n ",__func__);
        printk("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
    } else if( dwVolt<vol_min ) {

        TRes = g_ntc_r_min;
        printk("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
        printk("%s [Warnning] the temperature measured is not narmal,> +125 oC!\n ",__func__);
        printk("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
    } else {
        TRes = (g_RAP_pull_up_R*dwVolt) / (g_RAP_pull_up_voltage-dwVolt);
    }
    //------------------------------------------------------------------

    g_TemperatureR = TRes;

    /* convert register to temperature */
    sTemperature = RfPatThermistorConverTemp(TRes);

    return sTemperature;
}

static int get_hw_rfpa_temp(void)
{

	int ret = 0, data[4], i, ret_value = 0, ret_temp = 0, output;
	int times=1, Channel=g_RAP_ADC_channel;//6592=1(AUX_IN1_NTC)

	if( IMM_IsAdcInitReady() == 0 )
	{
       	        printk("[rfpa][thermal_auxadc_get_data]: AUXADC is not ready\n");
		return 0;
	}

	i = times;
	while (i--)
	{
		ret_value = IMM_GetOneChannelValue(Channel, data, &ret_temp);
		ret += ret_temp;
		dprintk("[rfpa][thermal_auxadc_get_data(AUX_IN-%d_NTC)]: ret_temp=%d\n",Channel,ret_temp);
	}

	//ret = ret*1500/4096	;
	ret = ret*1800/4096;//82's ADC power
	dprintk("[rfpa] AUXIN-%d output mV = %d\n",g_RAP_ADC_channel,ret);
	output = RFPaVoltToTemp(ret);
	dprintk("[rfpa] AUXIN-%d output temperature = %d\n",g_RAP_ADC_channel,output);

	return output;
}


static DEFINE_MUTEX(RFPA_LOCK);
static int rfpa_get_hw_temp(void)
{
	int t_ret=0;

	mutex_lock(&RFPA_LOCK);

    //get HW AP temp (TSAP)
	t_ret = get_hw_rfpa_temp();
	t_ret = t_ret * 1000;

	mutex_unlock(&RFPA_LOCK);

    if (t_ret > 60000) // abnormal high temp
        printk("[[rfpa]] temperature=%d\n", t_ret);

	dprintk("[[rfpa]] temperature, %d\n", t_ret);
	return t_ret;
}

static int rfpa_get_temp(struct thermal_zone_device *thermal,
						   unsigned long *t)
{
	*t = rfpa_get_hw_temp();
	return 0;
}

static int rfpa_bind(struct thermal_zone_device *thermal,
					   struct thermal_cooling_device *cdev)
{
	int table_val=0;

	if(!strcmp(cdev->type, g_bind0))
	{
		table_val = 0;
		dprintk("[%s] %s\n", __func__, cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind1))
	{
		table_val = 1;
		dprintk("[%s] %s\n", __func__, cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind2))
	{
		table_val = 2;
		dprintk("[%s] %s\n", __func__, cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind3))
	{
		table_val = 3;
		dprintk("[%s] %s\n", __func__, cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind4))
	{
		table_val = 4;
		dprintk("[%s] %s\n", __func__, cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind5))
	{
		table_val = 5;
		dprintk("[%s] %s\n", __func__, cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind6))
	{
		table_val = 6;
		dprintk("[%s] %s\n", __func__, cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind7))
	{
		table_val = 7;
		dprintk("[%s] %s\n", __func__, cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind8))
	{
		table_val = 8;
		dprintk("[%s] %s\n", __func__, cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind9))
	{
		table_val = 9;
		dprintk("[%s] %s\n", __func__, cdev->type);
	}
	else
	{
		return 0;
	}

	if (mtk_thermal_zone_bind_cooling_device(thermal, table_val, cdev)) {
		dprintk("[%s] error binding cooling dev\n",__func__);
		return -EINVAL;
	} else {
		dprintk("[%s] binding OK, %d\n",__func__, table_val);
	}

	return 0;
}

static int rfpa_unbind(struct thermal_zone_device *thermal,
						 struct thermal_cooling_device *cdev)
{
	int table_val=0;

	if(!strcmp(cdev->type, g_bind0))
	{
		table_val = 0;
		dprintk("[%s] %s\n",__func__, cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind1))
	{
		table_val = 1;
		dprintk("[%s] %s\n",__func__, cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind2))
	{
		table_val = 2;
		dprintk("[%s] %s\n",__func__, cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind3))
	{
		table_val = 3;
		dprintk("[%s] %s\n",__func__, cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind4))
	{
		table_val = 4;
		dprintk("[%s] %s\n",__func__, cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind5))
	{
		table_val = 5;
		dprintk("[%s] %s\n",__func__, cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind6))
	{
		table_val = 6;
		dprintk("[%s] %s\n",__func__, cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind7))
	{
		table_val = 7;
		dprintk("[%s] %s\n",__func__, cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind8))
	{
		table_val = 8;
		dprintk("[%s] %s\n",__func__, cdev->type);
	}
	else if(!strcmp(cdev->type, g_bind9))
	{
		table_val = 9;
		dprintk("[%s] %s\n",__func__, cdev->type);
	}
	else
		return 0;

	if (thermal_zone_unbind_cooling_device(thermal, table_val, cdev)) {
		dprintk("[%s] error unbinding cooling dev\n",__func__);
		return -EINVAL;
	} else {
		dprintk("[%s] unbinding OK\n",__func__);
	}

	return 0;
}

static int rfpa_get_mode(struct thermal_zone_device *thermal,
						   enum thermal_device_mode *mode)
{
	*mode = (kernelmode) ? THERMAL_DEVICE_ENABLED
			: THERMAL_DEVICE_DISABLED;
	return 0;
}

static int rfpa_set_mode(struct thermal_zone_device *thermal,
						   enum thermal_device_mode mode)
{
	kernelmode = mode;
	return 0;
}

static int rfpa_get_trip_type(struct thermal_zone_device *thermal, int trip,
								enum thermal_trip_type *type)
{
	*type = g_THERMAL_TRIP[trip];
	return 0;
}

static int rfpa_get_trip_temp(struct thermal_zone_device *thermal, int trip,
								unsigned long *temp)
{
	*temp = trip_temp[trip];
	return 0;
}

static int rfpa_get_crit_temp(struct thermal_zone_device *thermal,
								unsigned long *temperature)
{
	*temperature = RFPA_TEMP_CRIT;
	return 0;
}

/* bind callback functions to thermalzone */
static struct thermal_zone_device_ops rfpa_dev_ops = {
	.bind = rfpa_bind,
	.unbind = rfpa_unbind,
	.get_temp = rfpa_get_temp,
	.get_mode = rfpa_get_mode,
	.set_mode = rfpa_set_mode,
	.get_trip_type = rfpa_get_trip_type,
	.get_trip_temp = rfpa_get_trip_temp,
	.get_crit_temp = rfpa_get_crit_temp,
};

static int rfpa_get_max_state(struct thermal_cooling_device *cdev,
									   unsigned long *state)
{
	*state = 1;
	return 0;
}
static int rfpa_get_cur_state(struct thermal_cooling_device *cdev,
									   unsigned long *state)
{

	*state = cl_dev_sysrst_state;

	return 0;
}
static int rfpa_set_cur_state(struct thermal_cooling_device *cdev,
									   unsigned long state)
{

	cl_dev_sysrst_state = state;
	if(cl_dev_sysrst_state == 1)
	{
		printk("%s do something here to lower the temperature!!!\n",__func__);
		printk("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
		printk("*****************************************\n");
		printk("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");

		BUG();

		//arch_reset(0,NULL);
	}

	return 0;
}

static struct thermal_cooling_device_ops rfpa_cooling_ops = {
	.get_max_state = rfpa_get_max_state,
	.get_cur_state = rfpa_get_cur_state,
	.set_cur_state = rfpa_set_cur_state,
};


static int rfpa_read(struct seq_file *m, void *v)
{
	seq_printf(m, "[ rfpa_read] trip_0_temp=%d,trip_1_temp=%d,trip_2_temp=%d,trip_3_temp=%d,trip_4_temp=%d,\n\
			   trip_5_temp=%d,trip_6_temp=%d,trip_7_temp=%d,trip_8_temp=%d,trip_9_temp=%d,\n\
			   g_THERMAL_TRIP_0=%d,g_THERMAL_TRIP_1=%d,g_THERMAL_TRIP_2=%d,g_THERMAL_TRIP_3=%d,g_THERMAL_TRIP_4=%d,\n\
			   g_THERMAL_TRIP_5=%d,g_THERMAL_TRIP_6=%d,g_THERMAL_TRIP_7=%d,g_THERMAL_TRIP_8=%d,g_THERMAL_TRIP_9=%d,\n\
			   cooldev0=%s,cooldev1=%s,cooldev2=%s,cooldev3=%s,cooldev4=%s,\n\
			   cooldev5=%s,cooldev6=%s,cooldev7=%s,cooldev8=%s,cooldev9=%s,time_ms=%d\n",
			   trip_temp[0],trip_temp[1],trip_temp[2],trip_temp[3],trip_temp[4],
			   trip_temp[5],trip_temp[6],trip_temp[7],trip_temp[8],trip_temp[9],
			   g_THERMAL_TRIP[0],g_THERMAL_TRIP[1],g_THERMAL_TRIP[2],g_THERMAL_TRIP[3],g_THERMAL_TRIP[4],
			   g_THERMAL_TRIP[5],g_THERMAL_TRIP[6],g_THERMAL_TRIP[7],g_THERMAL_TRIP[8],g_THERMAL_TRIP[9],
			   g_bind0,g_bind1,g_bind2,g_bind3,g_bind4,g_bind5,g_bind6,g_bind7,g_bind8,g_bind9,
			   interval*1000);

	return 0;
}

int rfpa_register_thermal(void);
void rfpa_unregister_thermal(void);

static ssize_t rfpa_write(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	/*
	 * TODO:
	 * just sampling temperature right now,do something here later
	 */
	printk("[rfpa] %s\n",__func__);
#if 1
	int len=0,time_msec=0;
	int trip[10]={0};
	int t_type[10]={0};
	int i;
	char bind0[20],bind1[20],bind2[20],bind3[20],bind4[20];
	char bind5[20],bind6[20],bind7[20],bind8[20],bind9[20];
	char desc[512];


	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
	{
		return 0;
	}
	desc[len] = '\0';

	if (sscanf(desc, "%d %d %d %s %d %d %s %d %d %s %d %d %s %d %d %s %d %d %s %d %d %s %d %d %s %d %d %s %d %d %s %d",
			   &num_trip, &trip[0],&t_type[0],bind0, &trip[1],&t_type[1],bind1,
			   &trip[2],&t_type[2],bind2, &trip[3],&t_type[3],bind3,
			   &trip[4],&t_type[4],bind4, &trip[5],&t_type[5],bind5,
			   &trip[6],&t_type[6],bind6, &trip[7],&t_type[7],bind7,
			   &trip[8],&t_type[8],bind8, &trip[9],&t_type[9],bind9,
			   &time_msec) == 32)
	{
		dprintk("[rfpa_write] rfpa_unregister_thermal\n");
		rfpa_unregister_thermal();

		for(i=0; i<num_trip; i++)
			g_THERMAL_TRIP[i] = t_type[i];

		g_bind0[0]=g_bind1[0]=g_bind2[0]=g_bind3[0]=g_bind4[0]=g_bind5[0]=g_bind6[0]=g_bind7[0]=g_bind8[0]=g_bind9[0]='\0';

		for(i=0; i<20; i++)
		{
			g_bind0[i]=bind0[i];
			g_bind1[i]=bind1[i];
			g_bind2[i]=bind2[i];
			g_bind3[i]=bind3[i];
			g_bind4[i]=bind4[i];
			g_bind5[i]=bind5[i];
			g_bind6[i]=bind6[i];
			g_bind7[i]=bind7[i];
			g_bind8[i]=bind8[i];
			g_bind9[i]=bind9[i];
		}

		dprintk("[rfpa_write] g_THERMAL_TRIP_0=%d,g_THERMAL_TRIP_1=%d,g_THERMAL_TRIP_2=%d,g_THERMAL_TRIP_3=%d,g_THERMAL_TRIP_4=%d,\
					   g_THERMAL_TRIP_5=%d,g_THERMAL_TRIP_6=%d,g_THERMAL_TRIP_7=%d,g_THERMAL_TRIP_8=%d,g_THERMAL_TRIP_9=%d,\n",
					   g_THERMAL_TRIP[0],g_THERMAL_TRIP[1],g_THERMAL_TRIP[2],g_THERMAL_TRIP[3],g_THERMAL_TRIP[4],
					   g_THERMAL_TRIP[5],g_THERMAL_TRIP[6],g_THERMAL_TRIP[7],g_THERMAL_TRIP[8],g_THERMAL_TRIP[9]);
		dprintk("[rfpa_write] cooldev0=%s,cooldev1=%s,cooldev2=%s,cooldev3=%s,cooldev4=%s,\
					   cooldev5=%s,cooldev6=%s,cooldev7=%s,cooldev8=%s,cooldev9=%s\n",
					   g_bind0,g_bind1,g_bind2,g_bind3,g_bind4,g_bind5,g_bind6,g_bind7,g_bind8,g_bind9);

		for(i=0; i<num_trip; i++)
		{
			trip_temp[i]=trip[i];
		}

		interval=time_msec / 1000;

		dprintk("[rfpa_write] trip_0_temp=%d,trip_1_temp=%d,trip_2_temp=%d,trip_3_temp=%d,trip_4_temp=%d,\
					   trip_5_temp=%d,trip_6_temp=%d,trip_7_temp=%d,trip_8_temp=%d,trip_9_temp=%d,time_ms=%d\n",
					   trip_temp[0],trip_temp[1],trip_temp[2],trip_temp[3],trip_temp[4],
					   trip_temp[5],trip_temp[6],trip_temp[7],trip_temp[8],trip_temp[9],interval*1000);

		dprintk("[rfpa_write] tsbuck_register_thermal\n");
		rfpa_register_thermal();

		return count;
	}
	else
	{
		dprintk("[rfpa_write] bad argument\n");
	}

	return -EINVAL;
#endif
}

int rfpa_register_cooler(void)
{
	cl_dev_sysrst = mtk_thermal_cooling_device_register("rfpa-sysrst", NULL,
					&rfpa_cooling_ops);
   	return 0;
}

int rfpa_register_thermal(void)
{
	dprintk("[rfpa_register_thermal] \n");

	/* trips : trip 0~2 */
	thz_dev = mtk_thermal_zone_device_register("rfpa", num_trip, NULL,
			  &rfpa_dev_ops, 0, 0, 0, interval*1000);

	return 0;
}

void rfpa_unregister_cooler(void)
{
	if (cl_dev_sysrst) {
		mtk_thermal_cooling_device_unregister(cl_dev_sysrst);
		cl_dev_sysrst = NULL;
	}
}

void rfpa_unregister_thermal(void)
{
	dprintk("[rfpa_unregister_thermal] \n");

	if (thz_dev) {
		mtk_thermal_zone_device_unregister(thz_dev);
		thz_dev = NULL;
	}
}

static int rfpa_open(struct inode *inode, struct file *file)
{
	return single_open(file, rfpa_read, NULL);
}

static const struct file_operations rfpa_fops = {
	.owner = THIS_MODULE,
	.open = rfpa_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = rfpa_write,
	.release = single_release,
};

static int __init rfpa_init(void)
{
	int err = 0;
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *tsbuck_dir = NULL;

	dprintk("[rfpa_init] \n");
	
	err = rfpa_register_cooler();
	if(err)
		return err;
	err = rfpa_register_thermal();
	if (err)
		goto err_unreg;

	tsbuck_dir = proc_mkdir("rfpa", NULL);
	if (!tsbuck_dir)
	{
		dprintk("[rfpa_init]: mkdir /proc/rfpa failed\n");
	}
	else
	{
		entry = proc_create("rfpa",  S_IRUGO | S_IWUSR | S_IWGRP, tsbuck_dir, &rfpa_fops);
		if (entry) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
            proc_set_user(entry, 0, 1000);
#else
            entry->gid = 1000;
#endif
		}
	}

	return 0;

err_unreg:
	 rfpa_unregister_cooler();
	return err;
}

static void __exit rfpa_exit(void)
{
	dprintk("[rfpa_exit] \n");
	rfpa_unregister_thermal();
	rfpa_unregister_cooler();
}

/*
 * the only useful policy for cooling this thermal zone is to reset the machine.
 * and this module must be loaded after cooling device "mtktspa-sysrst" registered in mtk_ts_pa.c.
 */
late_initcall(rfpa_init);
module_exit(rfpa_exit);

MODULE_AUTHOR("bsp meizu");
MODULE_DESCRIPTION("Rf Pa NTC Temperature");
MODULE_LICENSE("GPL");
