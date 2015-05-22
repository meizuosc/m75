// [START]{ SMARTBOOK CUSTOM CODE. TODO: move to standalone file
// Original MHL driver use tabstop=4, this policy is followed here.
#include <linux/delay.h>
#include <mach/mt_gpio.h>
#include <cust_gpio_usage.h>
#include <cust_eint.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/time.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/power_supply.h>
#include <linux/kthread.h>
#include <linux/wakelock.h>
#include <linux/timex.h>
#include <linux/proc_fs.h>
#include <linux/mutex.h>
#include <linux/sched.h>

#include "si_cra.h"
#include "si_cra_cfg.h"
#include "si_mhl_defs.h"
#include "si_mhl_tx_api.h"
#include "si_mhl_tx_base_drv_api.h"
#include "si_8338_regs.h"
#include "si_drv_mhl_tx.h"
#include "si_platform.h"
#include "linux/aee.h"
#include "hdmitx_drv.h"
#include "smartbook.h"

#include <linux/mmprofile.h>
struct SMB_MMP_Events_t
{
    MMP_Event SmartBook;
    MMP_Event Keyboard_Ctrl;
	MMP_Event Keyboard_Normal;
	MMP_Event Mouse;
}SMB_MMP_Events;

#ifdef MTK_SMARTBOOK_SUPPORT

extern int SiiHandshakeCommand(HandshakeType ComType);
extern bool_t PutPriorityCBusTransactionWrapper(cbus_req_t *preq);
extern bool_t PutPriorityCBusTransactionImpl(cbus_req_t *preq);

#ifdef ENABLE_TX_DEBUG_PRINT
#define PutPriorityCBusTransaction(req) PutPriorityCBusTransactionWrapper(req)
#else 
#define PutPriorityCBusTransaction(req) PutPriorityCBusTransactionImpl(req)
#endif 

static struct input_dev *smartbook_dev = NULL;

// HID Buffer
static uint8_t hidbi = 0;                    // HID buffer index
static uint8_t hidbt = 0;                    // HID buffer tail
static uint8_t hidbuf[HID_BUF * HID_SIZE];    // HID data ring buffer

// Mouse
static __u32 mouse_btns[] = { BTN_LEFT, BTN_MIDDLE, BTN_RIGHT }; // BTN_RIGHT
int mouse_btn = 0;
int HID_RES_X, HID_RES_Y = 0;

/*#ifdef SMARTBOOK_CRYPTO
// Crypto, deprecated
unsigned char hid_mid[3] = {0xff,0xff,0xff};
unsigned char hid_cpc[3] = {0, 0, 0};
unsigned char mtkid[]   = { 0x66, 0x19, 0x5a, 0x22, 0xba, 0x51 };
unsigned char methods[] = { 0xcc, 0xb2, 0xaa, 0x95, 0x8b };
#endif*/

/****************************Debug**********************************/
const char print_mouse_btns[3][32] = {{"BTN_LEFT"}, {"BTN_MIDDLE"}, {"BTN_RIGHT"}};
const char print_kb_modmap[8][32] = {{"KEY_LEFTCTRL"}, {"KEY_LEFTSHIFT"}, {"KEY_LEFTALT"}, {"KEY_HOMEPAGE"},
	{"KEY_RIGHTCTRL"}, {"KEY_RIGHTSHIFT"}, {"KEY_RIGHTALT"}, {"KEY_RIGHTMETA"}};
const char print_kb_map[KB_LEN][32] = {  // 10 keys per line. refer USB_HID_Usage_Table.pdf for this mapping
  {0},{0},{0},{0}, {"KEY_A"}, {"KEY_B"}, {"KEY_C"}, {"KEY_D"}, {"KEY_E"}, {"KEY_F"}, 
  {"KEY_G"}, {"KEY_H"}, {"KEY_I"}, {"KEY_J"}, {"KEY_K"}, {"KEY_L"}, {"KEY_M"}, {"KEY_N"}, {"KEY_O"}, {"KEY_P"},
  {"KEY_Q"}, {"KEY_R"}, {"KEY_S"}, {"KEY_T"}, {"KEY_U"}, {"KEY_V"}, {"KEY_W"}, {"KEY_X"}, {"KEY_Y"}, {"KEY_Z"}, 
  {"KEY_1"}, {"KEY_2"}, {"KEY_3"}, {"KEY_4"}, {"KEY_5"}, {"KEY_6"}, {"KEY_7"}, {"KEY_8"}, {"KEY_9"}, {"KEY_0"},
  {"KEY_ENTER"}, 	 {"KEY_ESC"},       {"KEY_BACKSPACE"}, {"KEY_TAB"},     {"KEY_SPACE"}, {"EY_MINUS"}, {"KEY_EQUAL"}, {"KEY_LEFTBRACE"}, {"KEY_RIGHTBRACE"}, {"KEY_BACKSLASH"}, 
  {"KEY_GRAVE"}, 	 {"KEY_SEMICOLON"}, {"KEY_APOSTROPHE"},{"KEY_GRAVE"},   {"KEY_COMMA"}, {"KEY_DOT"}, {"KEY_SLASH"}, {"KEY_CAPSLOCK"}, {"KEY_F1, KEY_F2"},
  {"KEY_F3"},   	 {"KEY_F4"},        {"KEY_F5"}, 	   {"KEY_F6"},      {"KEY_F7"},    {"KEY_F8"}, {"KEY_F9"}, {"KEY_F10"}, {"KEY_F11"}, {"KEY_F12"},
  {"KEY_SYSRQ"},     {"KEY_SCROLLLOCK"},{"KEY_PAUSE"}, 	   {"KEY_INSERT"},  {"KEY_HOME"}, {"KEY_PAGEUP"}, {"KEY_DELETE"}, {"KEY_END"}, {"KEY_PAGEDOWN"}, {"KEY_RIGHT"},
  {"KEY_LEFT"},      {"KEY_DOWN"},      {"KEY_UP"}, 	   {"KEY_NUMLOCK"} ,{"KEY_KPSLASH"}, {"KEY_KPASTERISK"}, {"KEY_KPMINUS"}, {"KEY_KPPLUS"}, {"KEY_KPENTER"}, {"KEY_KP1"},
  {"KEY_KP2"},       {"KEY_KP3"},       {"KEY_KP4"},       {"KEY_KP5"},     {"KEY_KP6"}, {"KEY_KP7"}, {"KEY_KP8"}, {"KEY_KP9"}, {"KEY_KP0"}, {"KEY_KPDOT"}, 
  {"KEY_BACKSLASH"}, {"KEY_MENU"},      {"KEY_POWER"},     {"KEY_KPEQUAL"}, {"KEY_F13"}, {"KEY_F14"}, {"KEY_F15"}, {"KEY_F16"}, {"KEY_F17"}, {"KEY_F18"},
  {"KEY_F19"}, 		 {"KEY_F20"},       {"KEY_F21"},       {"KEY_F22"}, 	{"KEY_F23"}, {"KEY_F24"}, {"KEY_PLAYPAUSE"}, {"KEY_HELP"}, {"KEY_MENU"}, {"KEY_SELECT"},
  {"KEY_STOP"}, 	 {"KEY_AGAIN"},     {"KEY_UNDO"},      {"KEY_CUT"}, 	{"KEY_COPY"}, {"KEY_PASTE"}, {"KEY_FIND"}, {"KEY_MUTE"}, {"KEY_VOLUMEUP"}, {"KEY_VOLUMEDOWN"},
  {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0},
  {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0},
  {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0},
  {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0},
  {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0},
  {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0},
  {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, /*19x*/
  {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, /*20x*/
  {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, /*21x*/
  {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, /*22x*/
  {0}, {0}, {"KEY_BRIGHTNESSUP"}, {"KEY_BRIGHTNESSDOWN"}, {"KEY_FN_F1"}, {"KEY_FN_F2"}, {"KEY_FN_F3"}, {"KEY_FN_F4"}, {"KEY_FN_F5"}, {"KEY_FN_F6"},
  {"KEY_FN_F7"}, {"KEY_FN_F8"}, {"KEY_FN_F9"}, {"KEY_FN_F10"}, {"KEY_FN_F11"}, {"KEY_FN_F12"}, {0}, {0}, {0}, {0},
};


// Keyboard
int kb_modifier = 0;
int kb_codes[] = {0,0,0,0};
const unsigned int kb_modmap[] = {
  KEY_LEFTCTRL,  KEY_LEFTSHIFT,  KEY_LEFTALT,  KEY_HOMEPAGE,
  KEY_RIGHTCTRL, KEY_RIGHTSHIFT, KEY_RIGHTALT, KEY_RIGHTMETA
};

const unsigned int kb_map[KB_LEN] = {  // 10 keys per line. refer USB_HID_Usage_Table.pdf for this mapping
  0,0,0,0, KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, 
  KEY_G, KEY_H, KEY_I, KEY_J, KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P,
  KEY_Q, KEY_R, KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z, 
  KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9, KEY_0,
  KEY_ENTER, KEY_ESC, KEY_BACKSPACE, KEY_TAB, KEY_SPACE, KEY_MINUS, KEY_EQUAL, KEY_LEFTBRACE, KEY_RIGHTBRACE, KEY_BACKSLASH, 
  KEY_GRAVE, KEY_SEMICOLON, KEY_APOSTROPHE, KEY_GRAVE, KEY_COMMA, KEY_DOT, KEY_SLASH, KEY_CAPSLOCK, KEY_F1, KEY_F2,
  KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12,
  KEY_SYSRQ, KEY_SCROLLLOCK, KEY_PAUSE, KEY_INSERT, KEY_HOME, KEY_PAGEUP, KEY_DELETE, KEY_END, KEY_PAGEDOWN, KEY_RIGHT,
  KEY_LEFT, KEY_DOWN, KEY_UP, KEY_NUMLOCK, KEY_KPSLASH, KEY_KPASTERISK, KEY_KPMINUS, KEY_KPPLUS, KEY_KPENTER, KEY_KP1,
  KEY_KP2, KEY_KP3, KEY_KP4, KEY_KP5, KEY_KP6, KEY_KP7, KEY_KP8, KEY_KP9, KEY_KP0, KEY_KPDOT, 
  KEY_BACKSLASH, KEY_MENU, KEY_POWER, KEY_KPEQUAL, KEY_F13, KEY_F14, KEY_F15, KEY_F16, KEY_F17, KEY_F18,
  KEY_F19, KEY_F20, KEY_F21, KEY_F22, KEY_F23, KEY_F24, KEY_PLAYPAUSE/*can't find EXECUTE*/, KEY_HELP, KEY_MENU, KEY_SELECT,
  KEY_STOP, KEY_AGAIN, KEY_UNDO, KEY_CUT, KEY_COPY, KEY_PASTE, KEY_FIND, KEY_MUTE, KEY_VOLUMEUP, KEY_VOLUMEDOWN,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /*19x*/
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /*20x*/
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /*21x*/
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /*22x*/
  0, 0, KEY_BRIGHTNESSUP, KEY_BRIGHTNESSDOWN, KEY_FN_F1, KEY_FN_F2, KEY_FN_F3, KEY_FN_F4, KEY_FN_F5, KEY_FN_F6,
  KEY_FN_F7, KEY_FN_F8, KEY_FN_F9, KEY_FN_F10, KEY_FN_F11, KEY_FN_F12, 0, 0, 0, 0,
};

const unsigned char SourceID[ID_LEN] = 
    {SMB_SOURCE_ID_0, SMB_SOURCE_ID_1, SMB_SOURCE_ID_2, SMB_SOURCE_ID_3, SMB_SOURCE_ID_4, SMB_SOURCE_ID_5};
const unsigned char SinkID[ID_LEN] = 
    {SMB_SINK_ID_0, SMB_SINK_ID_1, SMB_SINK_ID_2, SMB_SINK_ID_3, SMB_SINK_ID_4, SMB_SINK_ID_5};

// Misc.
wait_queue_head_t smartbook_wq;
struct wake_lock smartbook_suspend_lock;
int sbk_isSuspend = 0;
//int deinit = 0;
//#ifdef SBK_FAKE_BATTERY
//int sbk_fake_battery = 0;
//#endif

static DEFINE_MUTEX(smb_mutex);
SinkType SinkStatus = NotConnect;

#define PERF_MONITOR

#ifdef PERF_MONITOR

#define NUM_RECORD 5
// Performance monitor facility
int64_t GetTimeStamp(){
    struct timeval tv;
    do_gettimeofday(&tv);
    return (tv.tv_sec & 0xff) * 1000000 + tv.tv_usec; /*convert to microsecond*/
}
void RecordStamp(bool dump, char tag){
    static int records = 0;
    int i;
    static int stamp[NUM_RECORD];
    static char name[NUM_RECORD];

    stamp[records] = (int)GetTimeStamp();
    name[records] = tag;
    records++;

    if(dump == true || records >= NUM_RECORD){
        for(i = 0; i < records; i++){
            smb_print("TimeStamp[%c]: %d\n", name[i], stamp[i]);
            stamp[i] = 0;
            name[i] = 0;
        }
        records = 0;
    }
}

#else
void RecordStamp(bool dump, char tag){

}
#endif


void Init_SMB_mmp_Events(void)
{
    if (SMB_MMP_Events.SmartBook == 0)
    {
    	smb_print("Init_SMB_mmp_Events\n");
        SMB_MMP_Events.SmartBook = MMProfileRegisterEvent(MMP_RootEvent, "SmartBook");
        SMB_MMP_Events.Keyboard_Ctrl = MMProfileRegisterEvent(SMB_MMP_Events.SmartBook, "Keyboard_Ctrl");
        SMB_MMP_Events.Keyboard_Normal = MMProfileRegisterEvent(SMB_MMP_Events.SmartBook, "Keyboard_Normal");
		SMB_MMP_Events.Mouse = MMProfileRegisterEvent(SMB_MMP_Events.SmartBook, "Mouse");
		
        MMProfileEnableEventRecursive(SMB_MMP_Events.SmartBook, 1);
    }
}

static int smartbook_init(int flag) {
    int err, i;
    if(smartbook_dev==NULL) {
        HID_RES_X = simple_strtoul(LCM_WIDTH, NULL, 0);
        HID_RES_Y = simple_strtoul(LCM_HEIGHT, NULL, 0);
        smartbook_dev = input_allocate_device();
        if (!smartbook_dev) {
            smb_print("smartbook_dev: Not enough memory\n");
            return -ENOMEM;
        }
        smartbook_dev->name = "sbk-kpd";
        smartbook_dev->id.bustype = BUS_HOST;
        smartbook_dev->id.vendor = 0x2454;
        smartbook_dev->id.product = 0x6589;
        smartbook_dev->id.version = 0x0001;
        set_bit(EV_KEY, smartbook_dev->evbit); // for kpd and mouse
        //set_bit(EV_ABS, smartbook_dev->evbit); 
        set_bit(EV_REL, smartbook_dev->evbit); // for mouse
        set_bit(REL_X, smartbook_dev->relbit);
        set_bit(REL_Y, smartbook_dev->relbit);
        set_bit(REL_WHEEL, smartbook_dev->relbit);
        set_bit(BTN_LEFT, smartbook_dev->keybit);
        set_bit(BTN_MIDDLE, smartbook_dev->keybit);
        set_bit(BTN_RIGHT, smartbook_dev->keybit);
        set_bit(KEY_BACK, smartbook_dev->keybit);
        set_bit(BTN_MOUSE, smartbook_dev->keybit);

        for(i=0;i<KB_LEN;i++) set_bit(kb_map[i], smartbook_dev->keybit);
        for(i=0;i<KB_MODLEN;i++) set_bit(kb_modmap[i], smartbook_dev->keybit);

    }
	
    if(flag!=0) {
        err = input_register_device(smartbook_dev);
        smb_print("plug-in into smartbook, register input_dev\n");
        SinkStatus = MonitorTV;
        if(err) {
            smb_print("smartbook_dev: Failed to register device\n");
            input_free_device(smartbook_dev);
            return err;
        }
    } else {
        //deinit = 1;
        hidbi  = 0;
        hidbt  = 0;
        input_unregister_device(smartbook_dev);
        smb_print("plug-out from smartbook, unregister input_dev\n");
        smartbook_dev = NULL;
        if(SinkStatus == SmartBook){
            smb_print("call smartbook disconnection\n");
        }
        SinkStatus = NotConnect;
        //deinit = 0;
        // remove battery status when MHL connection is break
        update_battery_2nd_info(POWER_SUPPLY_STATUS_NOT_CHARGING, 0, 0);
        smb_print("MHL connection break\n");
    }

	Init_SMB_mmp_Events();
    return 0;
}
SinkType SMBGetSinkStatus(){
    return SinkStatus;
}

/*#ifdef SMARTBOOK_CRYPTO
static void smartbook_crypt(unsigned char mid, unsigned char cpc) {
  unsigned char cps[6], enc;
  unsigned int i;
  for(i=0;i<6;i++) {
    enc = cpc;
    enc = enc & methods[mid];
    enc = enc ^ (enc >> 4);
    enc = enc ^ (enc >> 2);
    enc = enc ^ (enc >> 1);
    cpc = ( cpc << 1 ) | (enc & 1);
    cps[i] = cpc;
  }
  for(i=hidbi+2;i<hidbi+8;i++) {
    hidbuf[i] = hidbuf[i] ^ cps[i-2];
  }
  return 0;
}
#endif*/

/*void SiiSendCbusWriteBurst(unsigned char *bufp, int length){
    int i;
    cbus_req_t req;
    req.command = MHL_WRITE_BURST;
    req.length = length;
    req.offsetData = SCRATCHPAD_OFFSET;  // scratchpad offset (Don't modify it)
    req.payload_u.pdatabytes = bufp;
    //mutex_lock(&smb_mutex);
    //SiiMhlTxDrvSendCbusCommand(&req);
    PutNextCBusTransaction(&req);
    //mutex_unlock(&smb_mutex);
}*/

int SiiHandshakeCommand(HandshakeType ComType){
    int i;
    bool error = false;
    cbus_req_t req;
    HIDCommand *comm = (HIDCommand *)(req.payload_u.msgData);

    req.command = MHL_WRITE_BURST;
    req.length = WRITEBURST_MAX_LEN;
    req.offsetData = SCRATCHPAD_OFFSET;  // scratchpad offset (Don't modify it)

    comm->category = CA_MISC;
    comm->command = MISC_HANDSHAKE;

    switch(ComType){
        case Init:
            for(i = 0; i < ID_LEN; i++){
                comm->payload[i] = SourceID[i];
            }
            smb_print("handshake Init\n");
            break;
        case Ack:
            for(i = 0; i < ID_LEN; i++){
                comm->payload[i] = 0x0;
            }
            break;
        default:
            error = true;
            break;
    }
    if(error == false){
        //PutPriorityCBusTransaction(&req);
        SiiMhlTxDrvSendCbusCommand(&req);
    }
}

int SiiSendScreenCommand(ScreenOffType ComType, unsigned int downCountSec){
    
    bool error = false;
    cbus_req_t req;
    HIDCommand *comm = (HIDCommand *)(req.payload_u.msgData);

    req.command = MHL_WRITE_BURST;
    req.length = 4;
    req.offsetData = SCRATCHPAD_OFFSET;  // scratchpad offset (Don't modify it)

    comm->category = CA_PMU;
    comm->command = PMU_SCREEN;

    switch(ComType){
        case ImmediateOff:
            comm->payload[0] = 0xC0;
            comm->payload[1] = 0x0;
            break;
        case DownCount:
            comm->payload[0] = 0x80 + ((downCountSec & 0x3f00) >> 8);
            comm->payload[1] = (downCountSec & 0xff);
            break;
        case CancelDownCount:
            comm->payload[0] = 0x0;
            comm->payload[1] = 0x0;
            break;
        default:
            error = true;
            break;
    }
    if(error == false){
        //PutPriorityCBusTransaction(&req);
        SiiMhlTxDrvSendCbusCommand(&req);
    }
}

int SiiLatencyCommand(){
    bool error = false;
    struct timeval tv;
    int64_t count;

    cbus_req_t req;
    HIDCommand *comm = (HIDCommand *)(req.payload_u.msgData);

    req.command = MHL_WRITE_BURST;
    req.length = 5;
    req.offsetData = SCRATCHPAD_OFFSET;  // scratchpad offset (Don't modify it)

    comm->category = CA_MISC;
    comm->command = MISC_LATENCY;

    do_gettimeofday(&tv);
    count = tv.tv_sec*1000 + tv.tv_usec/1000; /*convert to millisecond*/

    comm->payload[0] = 0x1; // plug-in code ID, don't care on the sink side
    comm->payload[1] = (unsigned char)((count & 0xff00) >> 8);
    comm->payload[2] = (unsigned char)(count & 0xff);

    if(error == false){
        //PutPriorityCBusTransaction(&req);
        SiiMhlTxDrvSendCbusCommand(&req);
    }
}

static void smartbook_kb(void) {
    int i, j, k, kcode, kb_update[4] = {0,0,0,0};
    uint8_t *chidbuf = &(hidbuf[hidbi]);
    int mod_update = kb_modifier ^ chidbuf[2];
    if(chidbuf[3]==0x1 && chidbuf[4]==0x1 && chidbuf[5]==0x1) return;
	smb_print("chidbuf[2]:%d, chidbuf[3]:%d, chidbuf[4]:%d, chidbuf[5]:%d, chidbuf[6]:%d, mod_update:0x%x\n", chidbuf[2], chidbuf[3], chidbuf[4], chidbuf[5], chidbuf[6], mod_update);
    for(i=0;i<8;i++) {
        if(mod_update&(1<<i)) { 
            input_report_key(smartbook_dev, kb_modmap[i], ((chidbuf[2]&(1<<i))?1:0));
			smb_mmp_print(SMB_MMP_Events.Keyboard_Ctrl, MMProfileFlagPulse, ((chidbuf[2]&(1<<i))?1:0), kb_modmap[i], print_kb_modmap[i]);
        }
    }
    kb_modifier = chidbuf[2];
    for(i=3;i<7;i++) {
        if(chidbuf[i]==0) break;
        for(j=0;j<4;j++) if(kb_codes[j]==chidbuf[i]) break;
        if(j==4) {
            //smb_print("Press HID KeyCode: %d\n", (int)chidbuf[i]);
            kcode = kb_map[chidbuf[i]<KB_LEN?chidbuf[i]:0];
            input_report_key(smartbook_dev, kcode, 1);
            smb_print("Press ScanCode: %d\n", kcode);
			smb_mmp_print(SMB_MMP_Events.Keyboard_Normal, MMProfileFlagPulse, 1, kcode, print_kb_map[chidbuf[i]]);
            //for aee dump temp solution
            if(kcode == KEY_4 && ((chidbuf[2] & 0x5) == 0x5 || (chidbuf[2] & 0x50) == 0x50)) {
                aee_kernel_reminding("manual dump", "CTRL + ALT + 4 to trigger dump");
            }
        }
        else kb_codes[j] = 0;
    }
    for(i=0;i<4;i++) if(kb_codes[i]) {
        kcode = kb_map[kb_codes[i]];
        input_report_key(smartbook_dev, kcode, 0);
		smb_mmp_print(SMB_MMP_Events.Keyboard_Normal, MMProfileFlagPulse, 0, kcode, print_kb_map[kb_codes[i]]);
    }
    for(i=0;i<4;i++) kb_codes[i] = chidbuf[i+3];
    input_sync(smartbook_dev);
}

static void smartbook_mouse(void) {
    int x,y,z,i,tmp_btn;
    uint8_t *chidbuf = &(hidbuf[hidbi]);
	smb_print("chidbuf[2]:%d, chidbuf[3]:%d, chidbuf[4]:%d, chidbuf[5]:%d, chidbuf[6]:%d\n", chidbuf[2], chidbuf[3], chidbuf[4], chidbuf[5], chidbuf[6]);
    x = ((chidbuf[3]<<8)&0xff00) + (chidbuf[4]&0xff);
    y = ((chidbuf[5]<<8)&0xff00) + (chidbuf[6]&0xff);
    z = chidbuf[7];
    tmp_btn = mouse_btn ^ chidbuf[2];
    if(x>>15) x=-(((~x)&0xfff)+1);
    if(y>>15) y=-(((~y)&0xfff)+1);
    if(z>>7)  z=-(((~z)&0x0ff)+1);
    if(x<-100 || x>100 || y<-100 || y>100) return;
    mouse_btn = chidbuf[2];
    x = x * 2; y = y * 2;

    for(i=0;i<3;i++) 
		if(tmp_btn&(1<<i)) 
	{
		input_report_key(smartbook_dev, mouse_btns[i], chidbuf[2]&(1<<i));
		smb_mmp_print(SMB_MMP_Events.Mouse, MMProfileFlagPulse, chidbuf[2]&(1<<i), mouse_btns[i], print_mouse_btns[i]);
	}
	
    input_report_rel(smartbook_dev, REL_X, x);
    input_report_rel(smartbook_dev, REL_Y, y);
    input_report_rel(smartbook_dev, REL_WHEEL, z);

    smb_print("Update Mouse: %d %d %d %d\n", x, y, z, mouse_btn);

    input_sync(smartbook_dev);
}

static void smartbook_battery(void) {
    uint8_t *chidbuf = &(hidbuf[hidbi]);
    int charge_status = ((chidbuf[2] & 0x80)?1:0);
    int sbk_power_level = chidbuf[2] & 0x7f;
    
    smb_print("sbk_power_level: %d \n", sbk_power_level);
    
    update_battery_2nd_info(
        (sbk_power_level==100?
            POWER_SUPPLY_STATUS_FULL:
            (charge_status?
                POWER_SUPPLY_STATUS_CHARGING:
                POWER_SUPPLY_STATUS_NOT_CHARGING)),
        sbk_power_level,
        1
    );
    //SendCommand = true; 
}

static void smartbook_handshake(void) {
    uint8_t *chidbuf = &(hidbuf[hidbi]);
    int i;
    // skip category & command , total 2 bytes
    chidbuf += 2;
    /*for(i = 2; i < ID_LEN; i++){
        if(chidbuf[i] != SinkID[i]){
            smb_print("Compare ID Fail, i: %d, content: %d\n", i, chidbuf[i]);
            //SinkStatus = Unknown;
            return ;
        }
    }*/
    if(SinkStatus != SmartBook) {
        smb_print("Identify Sink is Smartbook!\n");
        SinkStatus = SmartBook;
        SiiHandshakeCommand(Ack);
    }
}

static void smartbook_latency(void) {
    uint8_t *chidbuf = &(hidbuf[hidbi]);
    int timestamp = (chidbuf[3] << 8) + chidbuf[4];

    smb_print("Latency: %d\n", timestamp);

    SiiLatencyCommand(); 
}

// Called in component/mhl_tx/si_mhl_tx.c, when plug / unplug MHL
void SiiHidSuspend(int flag) {
    if(sbk_isSuspend == flag) return;
    mutex_lock(&smb_mutex);

    sbk_isSuspend = flag;
    smartbook_init(flag);

    mutex_unlock(&smb_mutex);
    wake_up_interruptible(&smartbook_wq);
}

// Read data from Registers, write them to HID Buffer
int SiiHidWrite(int key) {
    int cbusInt, i, startReg = 0;
    unsigned int hid_kid = 0;
    uint8_t *chidbuf = &(hidbuf[hidbt]);

    if(!smartbook_dev) return 0;
    if(unlikely(HID_RES_Y==0)) smartbook_init(1);
    if(key==0) SiiRegReadBlock(REG_CBUS_SCRATCHPAD_0 + 8, chidbuf, HID_SIZE); //8);
    hidbt = (hidbt + HID_SIZE)%(HID_SIZE * HID_BUF);
    //hidbt = (hidbt + HID_SIZE) & 0x3ff;
    if(unlikely(hidbi==hidbt)) {
        smb_print("MHL Hid Ring buf Overflow. ");
        hidbi = (hidbt + HID_SIZE)%(HID_SIZE * HID_BUF);
        //hidbi = (hidbt + HID_SIZE) & 0x3ff;
    }

    wake_up_interruptible(&smartbook_wq);
}

// Read data from HID Buffer
int SiiHidRead() {
    int cbusInt, i, startReg = 0;
    unsigned int hid_kid = 0;
    uint8_t *chidbuf = &(hidbuf[hidbi]);
/*#ifdef SMARTBOOK_CRYPTO
    if(chidbuf[0]==2) {
        if(chidbuf[1]==2) hid_kid = 0;
        if(chidbuf[1]==3) hid_kid = 1;
        hid_mid[hid_kid] = chidbuf[2]>>4;
        hid_cpc[hid_kid] = chidbuf[3];
    }
    if(chidbuf[0]==1 && chidbuf[1]==2) hid_kid = 0;
    if(chidbuf[0]==1 && chidbuf[1]==3) hid_kid = 1;
#endif*/

    if(chidbuf[0]==1 && chidbuf[1]==2) smartbook_mouse();
    else if (chidbuf[0]==1 && chidbuf[1]==3) smartbook_kb();
    else if (chidbuf[0]==0x80 && chidbuf[1]==3) smartbook_battery();
    else if (chidbuf[0]==0x2 && chidbuf[1]==0xaa) smartbook_latency();
    else if (chidbuf[0]==0x2) smartbook_handshake();
    hidbi = (hidbi + HID_SIZE)%(HID_SIZE * HID_BUF);

}

// HID kthread offload ISR's tasks. ISR --> Kthread by HID Buffer
int smartbook_kthread(void *data) {
    struct sched_param param;
    int i, retval;

    //adjust priority
    param.sched_priority = RTPM_PRIO_SCRN_UPDATE;
    sched_setscheduler(current, SCHED_RR, &param);

    while(1) {
        if(hidbt==hidbi) { // ring buffer empty
          set_current_state(TASK_INTERRUPTIBLE);
          retval = wait_event_interruptible_timeout(smartbook_wq, hidbt!=hidbi , 60*HZ);
          set_current_state(TASK_RUNNING);
        }
        mutex_lock(&smb_mutex);
        if(smartbook_dev && retval!=0 && hidbt!=hidbi) SiiHidRead(); // not timeout
        mutex_unlock(&smb_mutex);
        /*if(deinit) {
            input_unregister_device(smartbook_dev);
            smartbook_dev = NULL;
            if(SinkStatus == SmartBook){
                smb_print("call smartbook disconnection\n");
                smartbook_state_callback(SMART_BOOK_DISCONNECTED);	
            }
            SinkStatus = NotConnect;
            deinit = 0;
            // remove battery status when MHL connection is break
            update_battery_2nd_info(POWER_SUPPLY_STATUS_NOT_CHARGING, 0, 0);
            smb_print("MHL connection break\n");
            continue;
       	 }*/

        if (kthread_should_stop()) break;
    }
}
static int SMBSinkTypeRead(char *buf, char **start, off_t off, int count, int *eof, void *data)
{
    int len = 0;
    char *p = buf;

    switch(SinkStatus){
        case NotConnect:
            p += sprintf(p, "N\n");
            break;
        case SmartBook:
            p += sprintf(p, "S\n");
            break;
        case MonitorTV:
            p += sprintf(p, "M\n");
            break;
        case Unknown:
            p += sprintf(p, "U\n");
            break;
        default:
            p += sprintf(p, "error\n");
            break;
    }

    len = p - buf;
    return len;
}


static int SMBScreenCommandRead(char *buf, char **start, off_t off, int count, int *eof, void *data)
{
    /*int len = 0;
    char *p = buf;

    if (mt_gpufreq_debug)
        p += sprintf(p, "gpufreq debug enabled\n");
    else
        p += sprintf(p, "gpufreq debug disabled\n");

    len = p - buf;
    return len;*/
}


static ssize_t SMBScreenCommandWrite(struct file *file, const char *buffer, unsigned long count, void *data)
{

    int comType = 0, countTime = 0;
    
    if (sscanf(buffer, "%d %d", &comType, &countTime) == 2 && smartbook_dev != NULL)
    {
        if(comType >= ImmediateOff && comType <= CancelDownCount){
            SiiSendScreenCommand(comType, countTime);
            smb_print("SendScreenCommand: %d %d\n", comType, countTime);
            return count;
        }
    }
    else if (smartbook_dev == NULL){
        smb_print("Smartbook is not connected! \n");
    }
    else {
        smb_print("SendScreenCommand fail!\n");
    }

    return -EINVAL;
}

static int __init smb_init(void){
    struct proc_dir_entry *entry = NULL;
    struct proc_dir_entry *smb_dir = NULL;
        
    smb_dir = proc_mkdir("smb", NULL);
    if (!smb_dir)
    {
        pr_err("[%s]: mkdir /proc/smb failed\n", __FUNCTION__);
    }
    else
    {
        entry = create_proc_entry("ScreenComm", S_IRUGO | S_IWUSR | S_IWGRP, smb_dir);
        if (entry)
        {
            entry->read_proc = NULL;
            entry->write_proc = SMBScreenCommandWrite;
        }
        entry = create_proc_entry("SinkType", S_IRUGO | S_IWUSR | S_IWGRP, smb_dir);
        if (entry)
        {
            entry->read_proc = SMBSinkTypeRead;
            entry->write_proc = NULL;
        }
    }
}

late_initcall(smb_init);

#endif
// [END]{ SMARTBOOK CUSTOM CODE. TODO: move to standalone file
