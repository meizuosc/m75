
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/notifier.h>
#include <linux/gpio_keys.h>
//#include <linux/input/synaptics_dsx.h>
#include "atmel_mxt_ts.h"
#include "tpd.h"
#ifdef KERNEL_ABOVE_2_6_38
#include <linux/input/mt.h>
#endif

#define GESTURE_ERROR      0x00 

/* Meizu double tap */
#define DOUBLE_TAP          0xA0 

/*Meizu swipe value */
#define SWIPE_X_LEFT        0xB0 
#define SWIPE_X_RIGHT       0xB1 
#define SWIPE_Y_UP          0xB2
#define SWIPE_Y_DOWN        0xB3 

/*Meizu Unicode value */
#define UNICODE_E           0xC0
#define UNICODE_C           0xC1
#define UNICODE_W           0xC2
#define UNICODE_M           0xC3
#define UNICODE_O           0xC4
#define UNICODE_S           0xC5
#define UNICODE_V_UP        0xC6
#define UNICODE_V_DOWN      0xC7
#define UNICODE_V_L         0xC8
#define UNICODE_V_R         0xC9
#define UNICODE_Z           0xCA

/*atmel  Unicode value*/
#define MXT_UNICODE_E   0x65
#define MXT_UNICODE_C   0x63
#define MXT_UNICODE_W   0x77
#define MXT_UNICODE_M   0x6D
#define MXT_UNICODE_S   0x73
#define MXT_UNICODE_O   0x6F
#define MXT_UNICODE_Z   0x7A
#define MXT_UNICODE_V   0x76

/*atmel swipe value */
#define MXT_SWIPE_X_LEFT        0x83
#define MXT_SWIPE_X_RIGHT       0x84 
#define MXT_SWIPE_Y_UP          0x85
#define MXT_SWIPE_Y_DOWN        0x86

/*atmel double tap */
#define MXT_DOUBLE_TAP 0x00

/*disable gesture value */
#define ALL_CTR     0x01
#define TAP_CTR     0x02
#define UNICODE_CTR 0x03
#define SWIPE_CTR   0x04


#define SWIPE_INDEX 0 
#define TAP_INDEX 1
#define UNICODE_INDEX 2 
#define ALL_INDEX 3

#define DOUBLE_TAP_BIT    (0x03)
#define SWIPE_BIT         (0x07)
#define UNICODE_O_BIT     (0x08)
#define UNICODE_V_BIT     (0x0A)
#define UNICODE_BIT       (0x0B)
 
#define SWIPE_UP_BIT          (0x08)
#define SWIPE_DOWN_BIT        (0x04)
#define SWIPE_L_BIT           (0x02)
#define SWIPE_R_BIT           (0x01)

#define OBJECT_SIZE (8)


#define V_BIT (1<<0)
#define C_BIT (1<<1)
#define E_BIT (1<<2)
#define W_BIT (1<<3)
#define M_BIT (1<<4)
#define S_BIT (1<<5)
#define Z_BIT (1<<6)
#define O_BIT (1<<7)

#define V_INDX (0)
#define C_INDX (1)
#define E_INDX (2)
#define W_INDX (3)
#define M_INDX (4)
#define S_INDX (5)
#define Z_INDX (6)
#define O_INDX (7)
#define UNICODE_MAX_INDX (8) 
#define MXT_CONST_SIZE (4)

#define MXT_EN_BIT (1<<7)

#define MXT_T221_SW_OFFSET 16 
#define MXT_T221_SW_L (1<<4)
#define MXT_T221_SW_R (1<<5)
#define MXT_T221_SW_U (1<<6)
#define MXT_T221_SW_D (1<<7)

#define MZ_SW_R  (1<<0)
#define MZ_SW_L  (1<<1)
#define MZ_SW_D  (1<<2)
#define MZ_SW_U  (1<<3)

#define __GESTURE_WITH_MOTOR__

#define  HALL_COVER  2 
#define  HALL_UNCOVER 3


#ifdef __GESTURE_WITH_MOTOR__
extern void motor_enable(void );
extern void motor_disable(void);
extern int motor_set_vibration(int value);
static int work_init = 0 ;
static struct delayed_work motor_work;
static struct workqueue_struct *workqueue;

static void mxt_motor_fn_work(struct work_struct *work)
{
	motor_disable();
}
static void  mxt_enable_motor(void)
{
	if(!work_init){
        workqueue = create_singlethread_workqueue("motor_workqueue");
	INIT_DELAYED_WORK(&motor_work, mxt_motor_fn_work);
	work_init = 1 ;
	}
	//motor_set_vibration(0x20);
	motor_enable();
	
	queue_delayed_work(workqueue,
			&motor_work,HZ/50);
}

#endif

static bool gesture_inited_mutex = false;
static struct mutex gesture_mutex;

struct gesture_object_info {
 u8 object_type ;
 u8 offset ;
 u8 size ;
 u8 ges_data[OBJECT_SIZE];
 u8 pri_data[OBJECT_SIZE];
};

 static int mxt_control_gesture(struct mxt_data*mxt_data);

 
 static void handler_unicode_gesture(struct mxt_data *mxt_data,char *data)
 {
	short unicode ;

	if(!mxt_data || !data)
		return ;

	unicode = *data ;
	switch(unicode){
	 case MXT_UNICODE_C :
	 	mxt_data->gesture_value = UNICODE_C ;
		break ;
	 case MXT_UNICODE_E :
	 	mxt_data->gesture_value = UNICODE_E ;
		break ;
	 case MXT_UNICODE_W  :
	 	mxt_data->gesture_value = UNICODE_W ;
		break ;
	case MXT_UNICODE_M  :
	 	mxt_data->gesture_value = UNICODE_M ;
		break ;
	case MXT_UNICODE_S   :
		mxt_data->gesture_value = UNICODE_S ;
		break ;
	case MXT_UNICODE_Z  :
		mxt_data->gesture_value = UNICODE_Z ;
		break ;
	case MXT_UNICODE_O  :
		mxt_data->gesture_value = UNICODE_O ;
		break ;
	case MXT_UNICODE_V  :
		mxt_data->gesture_value = UNICODE_V_UP ;
		break ;
	default :
		//mxt_data->gesture_value = GESTURE_ERROR ;
		info_printk("ERROR :detect unicode gesture[%x] \n",unicode);
	}

	return ;
 }


static int handler_swipe_gesture(struct mxt_data * mxt_data,unsigned char code)
{
	int retval = -1 ;
	char swipe[1] ;

	if(!mxt_data)
		return 0;
	info_printk("swipe code[%x]\n",code);
	switch(code){
	 case MXT_SWIPE_Y_UP:
	 	 mxt_data->gesture_value = SWIPE_Y_UP ;
		 break ;
	 case MXT_SWIPE_Y_DOWN:
	 	mxt_data->gesture_value = SWIPE_Y_DOWN ;
		break ;
	case MXT_SWIPE_X_LEFT:
		mxt_data->gesture_value = SWIPE_X_LEFT ;
		break ;
	case MXT_SWIPE_X_RIGHT:
		mxt_data->gesture_value = SWIPE_X_RIGHT;
		break ;
	default :
		info_printk("ERROR :detect swipe gesture \n");
		//mxt_data->gesture_value = GESTURE_ERROR ;
	}
	return 0 ;
}

static void mxt_report_wakeup_key(struct mxt_data *mxt_data)
{
	if(mxt_data->gesture_value!=GESTURE_ERROR){
	#ifdef __GESTURE_WITH_MOTOR__
	mxt_enable_motor();
	#endif
	input_report_key(mxt_data->input_dev,KEY_HOME,1);
	input_report_key(mxt_data->input_dev,KEY_HOME,0);
	input_sync(mxt_data->input_dev);
	}
}

int mxt_handler_tap_gesture(struct mxt_data *mxt_data,char *data)
{
	info_printk("handler tap\n");
	mxt_data->gesture_value  = DOUBLE_TAP ;
	mxt_report_wakeup_key(mxt_data);
    return 0;
}

 int mxt_handler_uni_sw_gesture(struct mxt_data *mxt_data,char *data)
{
	
	int retval = -1 ;
	info_printk(" handler uni_sw gesture[%x][%x][%x][%X][%x]\n",
		*data,*(data+1),*(data+2),*(data+3),*(data+4));
	
	
	if(!mxt_data||!data)
		return 0 ;
	if((*(data+1))&0x80){/*handler swipe */
		handler_swipe_gesture(mxt_data,(*(data+1)));

	}else {/*handler unicode */
		handler_unicode_gesture(mxt_data,data+1);		
	}
	
	mxt_report_wakeup_key(mxt_data);
	
	return 0 ;
}

static struct gesture_object_info gesture_info [] = {
{
.object_type = MXT_GEN_POWER_T7,
.offset      = 0,
.size        = 2,
.ges_data      ={60,10},
},{
.object_type = MXT_SPT_GPIOPWM_T19,
.offset     = 0,
.size    = 1,
.ges_data   = {3},
},{
.object_type = MXT_TOUCH_MULTITOUCHSCREEN_T100,
.offset  = 0,
.size = 1,
.ges_data = {0x8d},
},{
.object_type = MXT_NOISE_SUPPRESS_T72,
.offset = 0,
.size = 1,
.ges_data = {0},
},{
.object_type= MXT_SPT_CTECONFIG_T46,
.offset = 2,
.size = 2,
.ges_data = {8,20},
},{
.object_type = MXT_GEN_ACQUIRE_T8,
.offset = 14,
.size = 1,
.ges_data = {0x07},
},
};

#undef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

static int mxt_ir_mode(struct mxt_data *mxt_data,bool enable)
{
	int i = 0 ;
	struct mxt_object *object = NULL ;
	unsigned short addr ;
	unsigned char value ;
	int retval = -1 ;
	object = mxt_get_object(mxt_data,MXT_IR_OBJECT_T70) ;
	if(!object){
		info_printk("get ir object error\n");
		return -EIO ;
	}

	if(enable)
		value = 0x01 ;
	else 
		value = 0x00 ;
	
	for(i=0;(i<MXT_IR_INST_NUM)&&(i<object->instances_minus_one+1);i++){
		addr = object->start_address + (object->size_minus_one+1)*i ;
		retval = __mxt_write_reg(mxt_data->client,addr,sizeof(value),&value);
		if(retval!=0)
			info_printk("write IR object error \n");
	}

	return 0;
}

static int mxt_gesture_mode(struct mxt_data *mxt_data,bool enable)
{
	char data[3] ;
	int retval = -1 ;
	static int inited = 0 ;
	struct gesture_object_info *info ;
	struct mxt_object *object =NULL ;
	char * object_data = NULL ;
	int i = 0 ;

	
	for(i=0;i< ARRAY_SIZE(gesture_info);i++){
	info = &gesture_info[i] ;
	dbg_printk("info objec[%d][%d][%d]\n",info->object_type,i,ARRAY_SIZE(gesture_info));

	object = mxt_get_object(mxt_data,info->object_type);
	if(!object|| !info)
	 continue;

	if(!inited)
	  retval = __mxt_read_reg(mxt_data->client,object->start_address+info->offset,
	     info->size,info->pri_data);

	if(enable)
		object_data = info->ges_data ;/*load gesture data  */
    else 
		object_data = info->pri_data ;/*reload normal data */
	
	retval = __mxt_write_reg(mxt_data->client,object->start_address+info->offset,
		  info->size,object_data);
	if(retval!=0){
		info_printk("write objec(%d)error \n",object->type);
		return -1 ;
	}		
}
    mxt_ir_mode(mxt_data,enable);

	inited =1 ;
	
	return 0 ;

}

 static int mxt_swipe_unicode_control(struct mxt_data *mxt_data,bool enable)
 {
	 unsigned char data ;
	 struct mxt_object *object =NULL ;
	 int retval = -1 ;
	 
	 if(!mxt_data)
		 return -EIO ;
	 
	 object = mxt_get_object(mxt_data,MXT_GESTURE_UNICODE_CTR_T221);
	 if(!object)
		 return -EIO ;
 
	 
	 retval = __mxt_read_reg(mxt_data->client,object->start_address,sizeof(data),&data);
	 
	 if(enable)
		  data |= MXT_ENABLE_UNI_SW ;
	 else 
		  data &=~(MXT_ENABLE_UNI_SW);
	 
	 retval = __mxt_write_reg(mxt_data->client,object->start_address,sizeof(data),&data);
	 if(retval!=0)
		 return -EIO ;
   dbg_printk("Done\n");
 }


 void mxt_gesture_disable(struct mxt_data *mxt_data)
{

	info_printk("enter\n");

	if(!gesture_inited_mutex)
	{		
		mutex_init(&gesture_mutex);		
		gesture_inited_mutex = true;
	}	
	mutex_lock(&gesture_mutex);
	 

	// Check the touch sleep state,if sleep then wakeup it
	
	mxt_gesture_mode(mxt_data,false);
	mxt_swipe_unicode_control(mxt_data,false);
	mxt_data->gesture_enable = false ;
	

	mutex_unlock(&gesture_mutex);
}

 void mxt_gesture_enable(struct mxt_data *mxt_data)
{

	info_printk("enter\n");
	if(!gesture_inited_mutex)
	{		
		mutex_init(&gesture_mutex);		
		gesture_inited_mutex = true;
	}	
	mutex_lock(&gesture_mutex);
	
	mxt_data->gesture_value  = GESTURE_ERROR ;
	
	mxt_gesture_mode(mxt_data,true);
	mxt_control_gesture(mxt_data);
	mxt_data->gesture_enable = true ;
	mutex_unlock(&gesture_mutex);

}


static int mxt_init_code_value(struct mxt_data *data)
{
	memset(&data->unicode_info[0],0,sizeof(data->unicode_info));
	
	data->unicode_info[O_INDX].code = MXT_UNICODE_O ;
	data->unicode_info[Z_INDX].code = MXT_UNICODE_Z ;
	data->unicode_info[S_INDX].code = MXT_UNICODE_S ;
	data->unicode_info[M_INDX].code = MXT_UNICODE_M ;
	data->unicode_info[W_INDX].code = MXT_UNICODE_W ;
	data->unicode_info[E_INDX].code = MXT_UNICODE_E ;
	data->unicode_info[C_INDX].code = MXT_UNICODE_C ;
	data->unicode_info[V_INDX].code = MXT_UNICODE_V ;
	
	return 0 ;
}

static int mxt_save_unicode_block(struct mxt_data*data,int code,int value,int offset)
{
	int i = 0 ;
	int block_size = 0 ;
	int number = 0 ;
	if(!data || !code)
		return -EIO ;
	
	for(i=0;i<UNICODE_MAX_INDX;i++){
		if(data->unicode_info[i].code==code){
			data->unicode_info[i].number++ ;
			number = data->unicode_info[i].number ;
			block_size = number*sizeof(struct unicode_entity) ;
			data->unicode_info[i].entity = krealloc(data->unicode_info[i].entity,
				block_size,GFP_KERNEL);

			if(data->unicode_info[i].entity){
				dbg_printk("set code [%x],number[%d],offset[%d]\n",code,number,offset);
			  data->unicode_info[i].entity[number-1].offset = offset ;
			  data->unicode_info[i].entity[number-1].value = value ;
		    }
		}
	}
	
	return 0 ;
}



static int mxt_query_t220_block_info(struct mxt_data *data)
{
	struct mxt_object *object = NULL ;
	int i = 0 ;
	char * buf = NULL ;
	char *tmp = NULL ;
	int retval = -1 ;
	int block_size ;
	int code ;
	int offset ;
	int rfu ;

	if(data->t220_query){
	 info_printk("t220 already query \n");
	 return 0;
	}
	object = mxt_get_object(data,MXT_GESTURE_UNICODE_T220);
	if(!object)
		return -EIO ;
	
	buf = kzalloc(object->size_minus_one+64,GFP_KERNEL);
	if(!buf)
		return -EIO ;

	retval = __mxt_read_reg(data->client,object->start_address,object->size_minus_one+1,buf);
	if(retval!=0){
		kfree(buf);
		return -EIO ;
	}

	
    mxt_init_code_value(data);
	
	/*parse unicode enable/disable byte offset*/
	tmp = buf + 1 ;
	block_size =(*tmp)&0x0F ;
	offset = 1 ;
	
	while(block_size){
		code = (*(tmp+3))&0x7F ;		
		offset += 3 ;/*first block offset */		
		mxt_save_unicode_block(data,code,*(tmp+3),offset) ;		
		tmp += (MXT_CONST_SIZE+(block_size+1)/2) ;/*next block start address */
		offset += (block_size+1)/2 + 1 ;
		block_size = (*tmp)&0x0F ;
	}
	
	data->t220_query = true ;
#if 0
	for(i=0;i<UNICODE_MAX_INDX;i++){
		int number = 0 ;
		int j= 0 ;
	    number = data->unicode_info[i].number ;
		for(j=0;j<number;j++){
		info_printk(" code [%x],offset[%d],value[%x]\n",data->unicode_info[i].code,
		data->unicode_info[i].entity[j].offset ,
		data->unicode_info[i].entity[j].value);
	}
		
	}
#endif	
	info_printk("Done\n");
	kfree(buf);
	return 0 ;
}

static int mxt_set_enable_bit(struct mxt_data *data,int index,bool disable)
{
	struct unicode_control_info *info = NULL ;
	int retval = -1 ;
	int i = 0 ;
	
	if(!data || index<0 ||index>UNICODE_MAX_INDX)
		return -EIO ;
	
	info = &(data->unicode_info[index]) ;
	dbg_printk("enter index[%d] number[%d],code[%x]\n",index,info->number,info->code);
		
	for(i=0;i<info->number;i++){
		if(disable)
			info->entity[i].value |= MXT_EN_BIT ;/*disable this gesture */
		else 
			info->entity[i].value &= ~(MXT_EN_BIT);/*enable */
	}
	
	return 0 ;
}

static int mxt_wirte_data_to_t220(struct mxt_data *data)
{
	struct mxt_object *object = NULL ;
	struct unicode_control_info *info ;
	int retval = -1 ;
	int i,j;
	int offset ;
	
	object = mxt_get_object(data,MXT_GESTURE_UNICODE_T220);
	if(!object)
		return -EIO ;
	for(i=0;i<UNICODE_MAX_INDX;i++){
	 info = &(data->unicode_info[i]) ;
	 for(j=0;j<info->number;j++){
		offset = info->entity[j].offset ;
		retval = __mxt_write_reg(data->client,object->start_address+offset,
			sizeof(info->entity[j].value),&(info->entity[j].value));
		if(retval!=0)
			info_printk("!!!!!error wirte code[%x]\n",info->code);
		else {
		dbg_printk("code[%c:%x],offset[%d],value[%x]\n",info->code,info->code,
			 info->entity[j].offset,info->entity[j].value);
	}
	 }
	}
	return 0;
}
static int mxt_unicode_gesture_control(struct mxt_data *mxt_data,char *data)
{
	
	unsigned char unicode = MXT_ENABLE_UNI_SW ;
	int retval = -1 ;
	
	if(!mxt_data || !data)
		return -EIO ;
	
	if(!mxt_data->t220_query)
		mxt_query_t220_block_info(mxt_data);

	retval = mxt_swipe_unicode_control(mxt_data,true);
	/*O,Z,S,M,W,E,C,V */

	/*handler V */
	mxt_set_enable_bit(mxt_data,V_INDX,!(data[0]&V_BIT)) ;

	mxt_set_enable_bit(mxt_data,C_INDX,!(data[0]&C_BIT));
	mxt_set_enable_bit(mxt_data,E_INDX,!(data[0]&E_BIT));
	mxt_set_enable_bit(mxt_data,W_INDX,!(data[0]&W_BIT));
	
	mxt_set_enable_bit(mxt_data,M_INDX,!(data[0]&M_BIT));
	mxt_set_enable_bit(mxt_data,S_INDX,!(data[0]&S_BIT));
	mxt_set_enable_bit(mxt_data,Z_INDX,!(data[0]&Z_BIT));
	/*handler O */
	mxt_set_enable_bit(mxt_data,O_INDX,!(data[0]&O_BIT));
	
	mxt_wirte_data_to_t220(mxt_data);
	
   return 0;
}

static int mxt_tap_gesture_control(struct mxt_data *mxt_data,char *data)
{
	struct mxt_object * object = NULL ;
	unsigned char tap ;

	if(!mxt_data || !data)
		return -EIO ;
	info_printk("+++\n");
	object = mxt_get_object(mxt_data,MXT_GESTURE_TAP_T93);
	if(!object)
		return -1 ;
	
	if(data[0])
		 /*enable double tap gesture */
		 tap = 0x0F ;
	else 
		/*disable double tap gesture */
		tap = 0x00 ;
	info_printk("---\n");
	return __mxt_write_reg(mxt_data->client,object->start_address,sizeof(tap),&tap);
     
}


static int mxt_swipe_gesture_control(struct mxt_data *mxt_data,char *data)
{
	struct mxt_object *object =NULL ;
	unsigned char reg_val ;
	int retval = -1 ;
	
	if(!mxt_data || !data)
		return -EIO ;
	
	object = mxt_get_object(mxt_data,MXT_GESTURE_UNICODE_CTR_T221);
	if(!object)
		return -EIO ;
	
	retval = mxt_swipe_unicode_control(mxt_data,true);
	
	retval |= __mxt_read_reg(mxt_data->client,object->start_address+MXT_T221_SW_OFFSET,
		     sizeof(reg_val),&reg_val);

	if(data[0]&MZ_SW_R)
		 reg_val &=~MXT_T221_SW_R ;
	else
		reg_val |=(MXT_T221_SW_R) ;

	if(data[0]&MZ_SW_L)
		 reg_val &= ~MXT_T221_SW_L ;
	else
		reg_val |= (MXT_T221_SW_L) ;

	if(data[0]&MZ_SW_D)
		 reg_val &=~MXT_T221_SW_D ;
	else
		reg_val |=(MXT_T221_SW_D) ;

	if(data[0]&MZ_SW_U)
		 reg_val &= ~MXT_T221_SW_U ;
	else
		reg_val |=(MXT_T221_SW_U) ;
	
   retval |= __mxt_write_reg(mxt_data->client,object->start_address+MXT_T221_SW_OFFSET,
   	sizeof(reg_val),&reg_val);
   if(retval!=0)
   	  info_printk("write swipe control data error\n");
   dbg_printk("Done\n");
   return retval ;
}


 static int mxt_control_gesture(struct mxt_data*mxt_data)
{
	
	int retval = -1 ;
	int *p = (int *)mxt_data->gesture_mask ;
	retval  = mxt_swipe_gesture_control(mxt_data,&mxt_data->gesture_mask[SWIPE_INDEX]);
	retval |= mxt_unicode_gesture_control(mxt_data,&mxt_data->gesture_mask[UNICODE_INDEX]);
	retval |= mxt_tap_gesture_control(mxt_data,&mxt_data->gesture_mask[TAP_INDEX]);
	
	dbg_printk("write gesture mask(%x) \n",*p);

	return retval ;
}


/***********************************************
byte0 : 0=disable ,1=enable 
byte1 : enable/disalbe gesture (unicode e/w,swipe x/y)
byte2 : enable/disable gesture type(unicode,tap,swipe)
byte3 : PFU
***********************************************/
  ssize_t mxt_gesture_control_write(struct device *dev,
		 struct device_attribute *attr, const char *buf, size_t count)
 {
	 
	 const char * data = buf ;
	 int *p = NULL ;
	 struct mxt_data *mxt_data = dev_get_drvdata(dev);
	 int tmp = 0 ;
	 dev_err(&mxt_data->client->dev, "turn on/off [%x][%x][%x][%x] gesture control\n",
		  *(data+3),*(data+2),*(data+1),*(data));
	 	 
	if(data[0])
		mxt_data->disable_all = false ;

	if(data[2]==ALL_CTR){
	 	mxt_data->gesture_mask[ALL_INDEX] = (0x01&data[0])<<6 ;
		mxt_data->disable_all = !data[0] ;		
		mxt_data->gesture_mask[SWIPE_INDEX] = (data[0]?0xFF:0);
		mxt_data->gesture_mask[UNICODE_INDEX] = (data[0]?0xFF:0);
		mxt_data->gesture_mask[TAP_INDEX] = (data[0]?0xFF:0);
	}else if(data[2]==SWIPE_CTR){
		mxt_data->gesture_mask[SWIPE_INDEX] = 0x0F&data[0] ;
		//mxt_data->gesture_mask[ALL_INDEX]   = ((0x0F&data[0])==0x0F?1:2)<<6 ;
	}else if(data[2]==UNICODE_CTR){
		mxt_data->gesture_mask[UNICODE_INDEX] = 0xFF&data[0] ;
		//mxt_data->gesture_mask[ALL_INDEX]     = ((0xFF&data[0])==0xFF?1:2)<<6 ;
	}else if(data[2]==TAP_CTR){
		mxt_data->gesture_mask[TAP_INDEX] = 0x01&data[0] ;		
		//mxt_data->gesture_mask[ALL_INDEX] = ((0x01&data[0])==0x01?1:2)<<6 ;
	}else {
		info_printk("parse gesture type error\n");		
		//mxt_data->gesture_mask[ALL_INDEX] = 1
		return -EIO ;
	}

	tmp = ((mxt_data->gesture_mask[SWIPE_INDEX]==0x0F)&&
		   (mxt_data->gesture_mask[UNICODE_INDEX]==0xFF)&&
		   (mxt_data->gesture_mask[TAP_INDEX]==0x01));
	mxt_data->gesture_mask[ALL_INDEX] = (tmp?1:2)<<6 ;

	 return count;
}

ssize_t mxt_gesture_control_read(struct device *dev,
		 struct device_attribute *attr, char *buf)
 {
 
	 struct mxt_data *mxt_data = dev_get_drvdata(dev);
	 int *p = (int *)mxt_data->gesture_mask ;
	 memcpy(buf,p,4);
	 info_printk("gesture mask %x %p \n",*p,buf);
	 return 4 ;
 }

ssize_t mxt_gesture_hex_write(struct device *dev,
		 struct device_attribute *attr, const char *buf, size_t count)
 {
	 unsigned int value;
	 unsigned char * data = (char *)&value ;
	 int tmp = 0 ;
	 struct mxt_data *mxt_data = dev_get_drvdata(dev);
	 
	 if (sscanf(buf, "%x", &value) != 1){
		printk(KERN_ERR"%s:write gesture control data error\n",__func__);
		return -EINVAL;
	 }

	 dev_err(&mxt_data->client->dev, "turn on/off [%x][%x][%x][%x] gesture control\n",
		  *(data+3),*(data+2),*(data+1),*(data));
		 
	if(data[0])
		mxt_data->disable_all = false ;

	if(data[2]==ALL_CTR){
	 	mxt_data->gesture_mask[ALL_INDEX] = 0x01&data[0] ;
		mxt_data->disable_all = !data[0] ;
	}else if(data[2]==SWIPE_CTR){
		mxt_data->gesture_mask[SWIPE_INDEX] = 0x0F&data[0] ;
		//mxt_data->gesture_mask[ALL_INDEX]   = ((0x0F&data[0])==0x0F?1:2)<<6 ;
	}else if(data[2]==UNICODE_CTR){
		mxt_data->gesture_mask[UNICODE_INDEX] = 0xFF&data[0] ;
		//mxt_data->gesture_mask[ALL_INDEX]     = ((0xFF&data[0])==0xFF?1:2)<<6 ;
	}else if(data[2]==TAP_CTR){
		mxt_data->gesture_mask[TAP_INDEX] = 0x01&data[0] ;		
		//mxt_data->gesture_mask[ALL_INDEX] = ((0x01&data[0])==0x01?1:2)<<6 ;
	}else {
		info_printk("parse gesture type error\n");		
		//mxt_data->gesture_mask[ALL_INDEX] = 1
		return -EIO ;
	}
	tmp = ((mxt_data->gesture_mask[SWIPE_INDEX]==0x0F)&&
		   (mxt_data->gesture_mask[UNICODE_INDEX]==0xFF)&&
		   (mxt_data->gesture_mask[TAP_INDEX]==0x01));
	mxt_data->gesture_mask[ALL_INDEX] = (tmp?1:2)<<6 ;
	
	 return count;
}

ssize_t mxt_gesture_hex_read(struct device *dev,
		 struct device_attribute *attr, char *buf)
 {
	 struct mxt_data *mxt_data = dev_get_drvdata(dev);
	 int *p = (int *)mxt_data->gesture_mask ;
	 int count = snprintf(buf, PAGE_SIZE, "%x\n",
			 *p);
	 info_printk("gesture %x detect \n",mxt_data->gesture_value);
	
	 return count ;
 }

ssize_t mxt_gesture_value_read(struct device *dev,
		 struct device_attribute *attr, char *buf)
 {
	 struct mxt_data *mxt_data = dev_get_drvdata(dev);
 	 int count = snprintf(buf, PAGE_SIZE, "%u\n",
			 mxt_data->gesture_value);
	 info_printk("gesture %x detect \n",mxt_data->gesture_value);
	 mxt_data->gesture_value = GESTURE_ERROR ;
	 return count ;
 }

 ssize_t mxt_gesture_test(struct device *dev,
		  struct device_attribute *attr, char *buf)
  {
	  struct mxt_data *mxt_data = dev_get_drvdata(dev);
	  char value[8] ;
      int *p = (int *)mxt_data->gesture_mask ;
	  int count = snprintf(buf, PAGE_SIZE, "gesture_value:%u ",
			  mxt_data->gesture_value);

	  count += snprintf(buf+count, PAGE_SIZE, "gesture_mask:%x ",
			  *p);
	  
	  count += snprintf(buf+count, PAGE_SIZE, "disable all:%x ",
			  mxt_data->disable_all);
	  
     
		
	  return count ;
  }


