#ifndef __CMDQ_DEF_H__
#define __CMDQ_DEF_H__

#undef  CMDQ_OF_SUPPORT

#include <linux/kernel.h>

#define CMDQ_INIT_FREE_TASK_COUNT       (8)
#define CMDQ_MAX_THREAD_COUNT           (16)
#define CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT (4)     // Thread that are high-priority (display threads)
#define CMDQ_MAX_TRIGGER_LOOP_COUNT     (2)
#define CMDQ_MAX_RECORD_COUNT           (1024)
#define CMDQ_MAX_ERROR_COUNT            (2)
#define CMDQ_MAX_RETRY_COUNT            (1)
#define CMDQ_MAX_TASK_IN_THREAD         (16)
#define CMDQ_MAX_READ_SLOT_COUNT        (4)

#define CMDQ_MAX_PREFETCH_INSTUCTION    (240)       // Maximum prefetch buffer size, in instructions.
#define CMDQ_INITIAL_CMD_BLOCK_SIZE     (PAGE_SIZE)
#define CMDQ_EMERGENCY_BLOCK_SIZE       (256 * 1024)    // 128KB command buffer
#define CMDQ_EMERGENCY_BLOCK_COUNT      (4)             
#define CMDQ_INST_SIZE                  (2 * sizeof(uint32_t)) // instruction is 64-bit

#define CMDQ_MAX_LOOP_COUNT             (1000000)
#define CMDQ_MAX_INST_CYCLE             (27)
#define CMDQ_MIN_AGE_VALUE              (5)
#define CMDQ_MAX_ERROR_SIZE             (8 * 1024)

#define CMDQ_MAX_COOKIE_VALUE           (0xFFFF) // max value of CMDQ_THR_EXEC_CMD_CNT (value starts from 0)

#ifdef CONFIG_MTK_FPGA
#define CMDQ_DEFAULT_TIMEOUT_MS         (10000)
#else
#define CMDQ_DEFAULT_TIMEOUT_MS         (1000)
#endif

#define CMDQ_ACQUIRE_THREAD_TIMEOUT_MS  (2000)
#define CMDQ_PREDUMP_TIMEOUT_MS         (200)
#define CMDQ_PREDUMP_RETRY_COUNT        (5)

#define CMDQ_INVALID_THREAD             (-1)

#define CMDQ_DRIVER_DEVICE_NAME         "mtk_cmdq"

#ifndef CONFIG_MTK_FPGA
    #define CMDQ_PWR_AWARE 1        // FPGA does not have ClkMgr
#else
    #undef CMDQ_PWR_AWARE
#endif

typedef enum CMDQ_SCENARIO_ENUM
{
   CMDQ_SCENARIO_JPEG_DEC =0,
   CMDQ_SCENARIO_PRIMARY_DISP =1,
   CMDQ_SCENARIO_PRIMARY_MEMOUT = 2,
   CMDQ_SCENARIO_PRIMARY_ALL = 3,
   CMDQ_SCENARIO_SUB_DISP = 4,
   CMDQ_SCENARIO_SUB_MEMOUT = 5,
   CMDQ_SCENARIO_SUB_ALL = 6,
   CMDQ_SCENARIO_MHL_DISP = 7,
   CMDQ_SCENARIO_RDMA0_DISP = 8,
   CMDQ_SCENARIO_RDMA2_DISP = 9,

   CMDQ_SCENARIO_TRIGGER_LOOP = 10,          // Trigger loop scenario does not enable HWs

   CMDQ_SCENARIO_DISP_CONFIG_AAL = 11,
   CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_GAMMA = 12,
   CMDQ_SCENARIO_DISP_CONFIG_SUB_GAMMA = 13,
   CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_DITHER = 14,
   CMDQ_SCENARIO_DISP_CONFIG_SUB_DITHER = 15,
   CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_PWM = 16,
   CMDQ_SCENARIO_DISP_CONFIG_SUB_PWM = 17,
   CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_PQ = 18,
   CMDQ_SCENARIO_DISP_CONFIG_SUB_PQ = 19,
   CMDQ_SCENARIO_DISP_CONFIG_OD = 20,

   CMDQ_SCENARIO_USER_SPACE = 21,            // client from user space, so the cmd buffer is in user space.

   CMDQ_SCENARIO_DEBUG = 22,
   CMDQ_SCENARIO_DEBUG_PREFETCH = 23,

   CMDQ_SCENARIO_DISP_ESD_CHECK = 24,       // ESD check
   CMDQ_SCENARIO_DISP_SCREEN_CAPTURE = 25,  // for screen capture to wait for RDMA-done without blocking config thread

   CMDQ_MAX_SCENARIO_COUNT              // ALWAYS keep at the end
} CMDQ_SCENARIO_ENUM;

typedef enum CMDQ_HW_THREAD_PRIORITY_ENUM
{
    CMDQ_THR_PRIO_NORMAL                = 0,    // nomral (lowest) priority
    CMDQ_THR_PRIO_DISPLAY_TRIGGER       = 1,    // trigger loop (enables display mutex)

    CMDQ_THR_PRIO_DISPLAY_ESD           = 3,    // display ESD check (every 2 secs)
    CMDQ_THR_PRIO_DISPLAY_CONFIG        = 3,    // display config (every frame)

    CMDQ_THR_PRIO_MAX                   = 7,    // maximum possible priority
} CMDQ_HW_THREAD_PRIORITY_ENUM;

typedef enum CMDQ_DATA_REGISTER_ENUM
{
    // Value Reg, we use 32-bit
    // Address Reg, we use 64-bit
    // Note that R0-R15 and P0-P7 actullay share same memory
    // and R1 cannot be used.

    CMDQ_DATA_REG_JPEG                  = 0x00,     // R0
    CMDQ_DATA_REG_JPEG_DST              = 0x11,     // P1

    CMDQ_DATA_REG_PQ_COLOR              = 0x04,     // R4
    CMDQ_DATA_REG_PQ_COLOR_DST          = 0x13,     // P3

    CMDQ_DATA_REG_2D_SHARPNESS_0        = 0x05,     // R5
    CMDQ_DATA_REG_2D_SHARPNESS_0_DST    = 0x14,     // P4

    CMDQ_DATA_REG_2D_SHARPNESS_1        = 0x0a,     // R10
    CMDQ_DATA_REG_2D_SHARPNESS_1_DST    = 0x16,     // P6

    CMDQ_DATA_REG_DEBUG                 = 0x0b,     // R11
    CMDQ_DATA_REG_DEBUG_DST             = 0x17,     // P7

    // sentinel value for invalid register ID
    CMDQ_DATA_REG_INVALID               = -1,
} CMDQ_DATA_REGISTER_ENUM;

// CMDQ Events
#undef DECLARE_CMDQ_EVENT
#define DECLARE_CMDQ_EVENT(name, val) name = val,
typedef enum CMDQ_EVENT_ENUM
{
    #include "cmdq_event.h"
} CMDQ_EVENT_ENUM;
#undef DECLARE_CMDQ_EVENT

typedef enum CMDQ_ENG_ENUM
{
    // ISP
    CMDQ_ENG_ISP_IMGI                   = 0,
    CMDQ_ENG_ISP_IMGO                   , // 1
    CMDQ_ENG_ISP_IMG2O                  , // 2

    // MDP
    CMDQ_ENG_MDP_CAMIN                  , // 3
    CMDQ_ENG_MDP_RDMA0                  , // 4
    CMDQ_ENG_MDP_RDMA1                  , // 5
    CMDQ_ENG_MDP_RSZ0                   , // 6
    CMDQ_ENG_MDP_RSZ1                   , // 7
    CMDQ_ENG_MDP_RSZ2                   , // 8
    CMDQ_ENG_MDP_TDSHP0                 , // 9
    CMDQ_ENG_MDP_TDSHP1                 , // 10
    CMDQ_ENG_MDP_MOUT0                  , // 11
    CMDQ_ENG_MDP_MOUT1                  , // 12
    CMDQ_ENG_MDP_WROT0                  , // 13
    CMDQ_ENG_MDP_WROT1                  , // 14
    CMDQ_ENG_MDP_WDMA                   , // 15

    // JPEG & VENC
    CMDQ_ENG_JPEG_ENC                   , // 16
    CMDQ_ENG_VIDEO_ENC                  , // 17
    CMDQ_ENG_JPEG_DEC                   , // 18
    CMDQ_ENG_JPEG_REMDC                 , // 19

    // DISP
    CMDQ_ENG_DISP_UFOE                  , // 20
    CMDQ_ENG_DISP_AAL                   , // 21
    CMDQ_ENG_DISP_COLOR0                , // 22
    CMDQ_ENG_DISP_COLOR1                , // 23
    CMDQ_ENG_DISP_RDMA0                 , // 24
    CMDQ_ENG_DISP_RDMA1                 , // 25
    CMDQ_ENG_DISP_RDMA2                 , // 26
    CMDQ_ENG_DISP_WDMA0                 , // 27
    CMDQ_ENG_DISP_WDMA1                 , // 28
    CMDQ_ENG_DISP_OVL0                  , // 29
    CMDQ_ENG_DISP_OVL1                  , // 30
    CMDQ_ENG_DISP_GAMMA                 , // 31
    CMDQ_ENG_DISP_MERGE                 , // 32
    CMDQ_ENG_DISP_SPLIT0                , // 33
    CMDQ_ENG_DISP_SPLIT1                , // 34
    CMDQ_ENG_DISP_DSI0_VDO              , // 35
    CMDQ_ENG_DISP_DSI1_VDO              , // 36
    CMDQ_ENG_DISP_DSI0_CMD              , // 37
    CMDQ_ENG_DISP_DSI1_CMD              , // 38
    CMDQ_ENG_DISP_DSI0                  , // 39
    CMDQ_ENG_DISP_DSI1                  , // 40
    CMDQ_ENG_DISP_DPI                   , // 41

    CMDQ_MAX_ENGINE_COUNT               // ALWAYS keep at the end
} CMDQ_ENG_ENUM;

typedef struct cmdqReadRegStruct
{
    uint32_t count;         // number of entries in regAddresses
    uint32_t *regAddresses; // an array of register addresses
} cmdqReadRegStruct;

typedef struct cmdqRegValueStruct
{
    uint32_t count;
        // number of entries in result
    uint32_t *regValues;    // array of register values.
                            // in the same order as cmdqReadRegStruct
} cmdqRegValueStruct;

typedef struct cmdqReadAddressStruct
{
    uint32_t count;            // [IN] number of entries in result.
    uint32_t *dmaAddresses;    // [IN] array of physical addresses to read.
                               //      these value must allocated by CMDQ_IOCTL_ALLOC_WRITE_ADDRESS ioctl
    uint32_t *values;          // [OUT] values that dmaAddresses point into
} cmdqReadAddressStruct;

typedef struct cmdqCommandStruct
{
    uint32_t scenario;              // [IN] deprecated. will remove in the future.
    uint32_t priority;              // [IN] task schedule priorty. this is NOT HW thread priority.
    uint64_t engineFlag;            // [IN] bit flag of engines used.
    uint32_t *pVABase;              // [IN] pointer to instruction buffer
    uint32_t blockSize;             // [IN] size of instruction buffer, in bytes.
    cmdqReadRegStruct regRequest;   // [IN] request to read register values at the end of command
    cmdqRegValueStruct regValue;    // [OUT] register values of regRequest
    cmdqReadAddressStruct readAddress;  // [IN/OUT] physical addresses to read value
    uint32_t debugRegDump;          // [IN] set to non-zero to enable register debug dump.
    void *privateData;              // [Reserved] This is for CMDQ driver usage itself. Not for client.
} cmdqCommandStruct;

typedef enum CMDQ_CAP_BITS
{
    CMDQ_CAP_WFE = 0,   // bit 0: TRUE if WFE instruction support is ready. FALSE if we need to POLL instead. 
} CMDQ_CAP_BITS;


#endif  // __CMDQ_DEF_H__
