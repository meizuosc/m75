#include "cmdq_core.h"
#include "cmdq_reg.h"
#include "cmdq_struct.h"
#include "cmdq_mmp.h"
#include "cmdq_mutex.h"
#include "cmdq_device.h"
#include "cmdq_platform.h"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/vmalloc.h>
#include <linux/atomic.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <mach/mt_clkmgr.h>
#include <mach/mt_irq.h>
#include <mach/memory.h>
#include <linux/ftrace.h>
#include <linux/met_drv.h>
#include <linux/seq_file.h>
#include "smi_debug.h"
#include "ddp_hal.h"
#include "ddp_reg.h"

#ifdef CMDQ_OF_SUPPORT
#include "cmdq_device.h"
#define MMSYS_CONFIG_BASE cmdq_dev_get_module_base_VA_MMSYS_CONFIG()
#else
#include <mach/mt_reg_base.h>
#endif

#define CMDQ_GET_COOKIE_CNT(thread) (CMDQ_REG_GET32(CMDQ_THR_EXEC_CNT(thread)) & CMDQ_MAX_COOKIE_VALUE)

extern void mt_irq_dump_status(int irq);

#define DEBUG_STATIC static

// use mutex because we don't access task list in IRQ
// and we may allocate memory when create list items
static DEFINE_MUTEX(gCmdqTaskMutex);
static DEFINE_MUTEX(gCmdqWriteAddrMutex);

// These may access in IRQ so use spin lock.
static DEFINE_SPINLOCK(gCmdqThreadLock);
static atomic_t gCmdqThreadUsage;
static bool gCmdqSuspended;
static DEFINE_SPINLOCK(gCmdqExecLock);
static DEFINE_SPINLOCK(gCmdqRecordLock);

// Emergency buffer when fail to allocate memory.
static DEFINE_SPINLOCK(gCmdqAllocLock);
static EmergencyBufferStruct gCmdqEmergencyBuffer[CMDQ_EMERGENCY_BLOCK_COUNT];

// The main context structure
static wait_queue_head_t       gCmdWaitQueue[CMDQ_MAX_THREAD_COUNT];    // task done notification
static wait_queue_head_t       gCmdqThreadDispatchQueue;                // thread acquire notification

static ContextStruct           gCmdqContext;
static CmdqCBkStruct           gCmdqGroupCallback[CMDQ_MAX_GROUP_COUNT];
static CmdqDebugCBkStruct      gCmdqDebugCallback;

const static uint64_t          gCmdqEngineGroupBits[CMDQ_MAX_GROUP_COUNT] = {CMDQ_ENG_ISP_GROUP_BITS,
                                                                             CMDQ_ENG_MDP_GROUP_BITS,
                                                                             CMDQ_ENG_DISP_GROUP_BITS,
                                                                             CMDQ_ENG_JPEG_GROUP_BITS,
                                                                             CMDQ_ENG_VENC_GROUP_BITS};

DEBUG_STATIC void cmdqCoreHandleError(int32_t thread, int32_t value, CMDQ_TIME *pGotIRQ);
DEBUG_STATIC void cmdqCoreHandleDone(int32_t thread, int32_t value, CMDQ_TIME *pGotIRQ);
DEBUG_STATIC void cmdq_core_consume_waiting_list(struct work_struct *);
DEBUG_STATIC uint32_t* cmdq_core_get_pc(const TaskStruct *pTask, uint32_t thread, uint32_t insts[4]);

static bool cmdq_core_init_emergency_buffer(void)
{
    int i;

    memset(&gCmdqEmergencyBuffer[0], 0, sizeof(gCmdqEmergencyBuffer));
    
    for (i = 0; i < CMDQ_EMERGENCY_BLOCK_COUNT; ++i)
    {
        EmergencyBufferStruct *buf = &gCmdqEmergencyBuffer[i];
        
        buf->va = dma_alloc_coherent(cmdq_dev_get(), 
                                     CMDQ_EMERGENCY_BLOCK_SIZE, 
                                     &buf->pa, GFP_KERNEL);
        buf->size = CMDQ_EMERGENCY_BLOCK_SIZE;
        buf->used = false;
    }

    return true;
}

static bool cmdq_core_uninit_emergency_buffer(void)
{
    int i;
    for (i = 0; i < CMDQ_EMERGENCY_BLOCK_COUNT; ++i)
    {
        EmergencyBufferStruct *buf = &gCmdqEmergencyBuffer[i];
        dma_free_coherent(cmdq_dev_get(),
                          CMDQ_EMERGENCY_BLOCK_SIZE,
                          buf->va,
                          buf->pa);
        if (buf->used)
        {
            CMDQ_ERR("Emergency buffer %d, 0x%p, 0x%pa still using\n", 
                     i,
                     buf->va,
                     &buf->pa);
        }
    }

    memset(&gCmdqEmergencyBuffer[0], 0, sizeof(gCmdqEmergencyBuffer));

    return true;
}


static bool cmdq_core_alloc_emergency_buffer(void **va, dma_addr_t *pa)
{
    int i;
    bool ret = false;
    
    spin_lock(&gCmdqAllocLock);
    for (i = 0; i < CMDQ_EMERGENCY_BLOCK_COUNT; ++i)
    {
        if (!gCmdqEmergencyBuffer[i].used &&
            gCmdqEmergencyBuffer[i].va)
        {
            gCmdqEmergencyBuffer[i].used = true;
            *va = gCmdqEmergencyBuffer[i].va;
            *pa = gCmdqEmergencyBuffer[i].pa;
            ret = true;
            break;
        }
    }
    spin_unlock(&gCmdqAllocLock);
    return ret;
}

static void cmdq_core_free_emergency_buffer(void *va, dma_addr_t pa)
{
    int i;
    
    spin_lock(&gCmdqAllocLock);
    for (i = 0; i < CMDQ_EMERGENCY_BLOCK_COUNT; ++i)
    {
        if (gCmdqEmergencyBuffer[i].used &&
            va == gCmdqEmergencyBuffer[i].va)
        {
            gCmdqEmergencyBuffer[i].used = false;
            break;
        }
    }
    spin_unlock(&gCmdqAllocLock);
}

bool cmdq_core_is_emergency_buffer(void *va)
{
    int i;
    bool ret = false;
    
    spin_lock(&gCmdqAllocLock);
    for (i = 0; i < CMDQ_EMERGENCY_BLOCK_COUNT; ++i)
    {
        if (gCmdqEmergencyBuffer[i].used &&
            va == gCmdqEmergencyBuffer[i].va)
        {
            ret = true;
            break;
        }
    }
    spin_unlock(&gCmdqAllocLock);
    return ret;
}

const char* cmdq_core_get_event_name(CMDQ_EVENT_ENUM event)
{
#undef DECLARE_CMDQ_EVENT
#define DECLARE_CMDQ_EVENT(name, val) case val:return #name;

    switch(event)
    {
        #include "cmdq_event.h"
    }

    return "CMDQ_EVENT_UNKNOWN";
#undef DECLARE_CMDQ_EVENT
}

ssize_t cmdqCorePrintLogLevel(struct device *dev, struct device_attribute *attr, char *buf)
{
    int len = 0;
    if (buf)
    {
        len = sprintf(buf, "%d\n", gCmdqContext.logLevel);
    }
    return len;
}

ssize_t cmdqCoreWriteLogLevel(struct device *dev,
                              struct device_attribute *attr,
                              const char *buf,
                              size_t size)
{
    int len = 0;
    int32_t value = 0;

    char textBuf[10] = {0};

    if (size >= 10)
    {
        return -EFAULT;
    }

    len = size;
    memcpy(textBuf, buf, len);

    textBuf[len] = '\0';
    sscanf(textBuf, "%d", &value);

    if (value < 0 || value > 3)
    {
        value = 0;
    }
    gCmdqContext.logLevel = value;

    return len;
}

ssize_t cmdqCorePrintProfileEnable(struct device *dev, struct device_attribute *attr, char *buf)
{
    int len = 0;
    if (buf)
    {
        len = sprintf(buf, "%d\n", gCmdqContext.enableProfile);
    }
    return len;

}

ssize_t cmdqCoreWriteProfileEnable(struct device *dev,
                              struct device_attribute *attr,
                              const char *buf,
                              size_t size)
{
    int len = 0;
    int32_t value = 0;

    char textBuf[10] = {0};

    if (size >= 10)
    {
        return -EFAULT;
    }

    len = size;
    memcpy(textBuf, buf, len);

    textBuf[len] = '\0';
    sscanf(textBuf, "%d", &value);

    if (value < 0 || value > 3)
    {
        value = 0;
    }
    gCmdqContext.enableProfile = value;

    return len;
}

extern int primary_display_check_path(char* stringbuf, int buf_len);

ssize_t cmdqCorePrintStatus(struct device *dev,
                            struct device_attribute *attr,
                            char *buf)
{
    unsigned long flags = 0L;
    EngineStruct *pEngine = NULL;
    TaskStruct   *pTask = NULL;
    struct list_head *p = NULL;
    ThreadStruct *pThread = NULL;
    int32_t      index = 0;
    int32_t      inner = 0;
    int32_t      length = 0;
    int          listIdx = 0;
    char         *pBuffer = buf;
    const struct list_head *lists[] = {&gCmdqContext.taskFreeList, &gCmdqContext.taskActiveList, &gCmdqContext.taskWaitList};
    const static char         *listNames[] = {"Free", "Active", "Wait"};

    const CMDQ_ENG_ENUM  engines[]      = { CMDQ_FOREACH_STATUS_MODULE_PRINT(GENERATE_ENUM) };
    const static char    *engineNames[] = { CMDQ_FOREACH_STATUS_MODULE_PRINT(GENERATE_STRING) };

#ifdef CMDQ_PWR_AWARE
    pBuffer += sprintf(pBuffer, "====== Clock Status =======\n");
    pBuffer += sprintf(pBuffer, "MT_CG_INFRA_GCE: %d, MT_CG_DISP0_MUTEX_32K: %d\n",
        clock_is_on(MT_CG_INFRA_GCE), clock_is_on(MT_CG_DISP0_MUTEX_32K));
#endif

    pBuffer += sprintf(pBuffer, "====== Events =======\n");
    pBuffer += cmdq_core_print_event(pBuffer);

    pBuffer += sprintf(pBuffer, "====== Mutex Usage =======\n");
    for (listIdx = 0; listIdx < DISP_MUTEX_MDP_COUNT; ++listIdx)
    {
        if (cmdqMDPMutexInUse(listIdx))
        {
            pBuffer += sprintf(pBuffer, "Mutex %d used by pid:%d\n",
                               listIdx,
                               cmdqMDPMutexOwnerPid(listIdx));
        }
    }

    pBuffer += sprintf(pBuffer, "====== Engine Usage =======\n");

    for (listIdx = 0; listIdx < (sizeof(engines) / sizeof(engines[0])); ++listIdx)
    {
        pEngine = &gCmdqContext.engine[engines[listIdx]];
        pBuffer += sprintf(pBuffer, "%s: count %d, owner %d, fail: %d, reset: %d\n",
                       engineNames[listIdx],
                       pEngine->userCount,
                       pEngine->currOwner,
                       pEngine->failCount,
                       pEngine->resetCount);
    }


    mutex_lock(&gCmdqTaskMutex);

    // print all tasks in both list
    for (listIdx = 0; listIdx < (sizeof(lists) / sizeof(lists[0])); listIdx++)
    {
        // skip FreeTasks by default
        if (gCmdqContext.logLevel < 2 &&
            listIdx == 0)
        {
            continue;
        }

        index = 0;
        list_for_each(p, lists[listIdx])
        {
            pTask = list_entry(p, struct TaskStruct, listEntry);
            pBuffer += sprintf(pBuffer, "====== %s Task(%d) 0x%p Usage =======\n", listNames[listIdx], index, pTask);

            pBuffer += sprintf(pBuffer, "State %d, VABase: 0x%p, MVABase: %pa, Size: %d\n",
                pTask->taskState, pTask->pVABase, &pTask->MVABase, pTask->commandSize);
            pBuffer += sprintf(pBuffer, "Scenario %d, Priority: %d, Flag: 0x%08llx, VAEnd: 0x%p\n",
                pTask->scenario, pTask->priority, pTask->engineFlag, pTask->pCMDEnd);
            pBuffer += sprintf(pBuffer, "Trigger %lld, IRQ: %lld, Wait: %lld, Wake Up: %lld\n",
                               pTask->trigger,
                               pTask->gotIRQ,
                               pTask->beginWait,
                               pTask->wakedUp);
            ++index;
        }
        pBuffer += sprintf(pBuffer, "====== Total %d %s Task =======\n", index, listNames[listIdx]);
    }

    for (index = 0; index < CMDQ_MAX_THREAD_COUNT; index++)
    {
        pThread = &(gCmdqContext.thread[index]);

        if (pThread->taskCount > 0)
        {
            pBuffer += sprintf(pBuffer, "====== Thread %d Usage =======\n", index);
            pBuffer += sprintf(pBuffer, "Wait Cookie %d, Next Cookie %d\n", pThread->waitCookie, pThread->nextCookie);

            spin_lock_irqsave(&gCmdqThreadLock, flags);

            for (inner = 0; inner < CMDQ_MAX_TASK_IN_THREAD; inner++)
            {
                pTask = pThread->pCurTask[inner];
                if (NULL != pTask)
                {
                    // dump task basic info
                    pBuffer += sprintf(pBuffer, "Slot: %d, Task: 0x%p, Pid: %d, Name: %s, Scn: %d, VABase: 0x%p, MVABase: %pa, Size: %d, Last Command: 0x%08x:0x%08x\n",
                        index,
                        pTask,
                        pTask->callerPid,
                        pTask->callerName,
                        pTask->scenario,
                        pTask->pVABase,
                        &pTask->MVABase,
                        pTask->commandSize,
                        pTask->pCMDEnd[-1],
                        pTask->pCMDEnd[0]);

                    // dump PC info
                    do
                    {
                        uint32_t *pcVA = NULL;
                        uint32_t  insts[4] = {0};
                        char      parsedInstruction[128] = {0};

                        pcVA = cmdq_core_get_pc(pTask, index, insts);
                        if (pcVA)
                        {
                            cmdq_core_parse_instruction(pcVA, parsedInstruction, sizeof(parsedInstruction));
                            pBuffer += sprintf(pBuffer, "PC(VA): 0x%p, 0x%08x:0x%08x => %s",
                                               pcVA,
                                               insts[2],
                                               insts[3],
                                               parsedInstruction);
                        }
                        else
                        {
                            pBuffer += sprintf(pBuffer, "PC(VA): Not available\n");
                        }

                    }while(0);


                }
            }

            spin_unlock_irqrestore(&gCmdqThreadLock, flags);
        }
    }

    do{
        uint32_t value[10] = {0};
        uint32_t thread = CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT;
        
        pBuffer += sprintf(pBuffer, ("trigger thread:\n"));
    	value[0] = CMDQ_REG_GET32(CMDQ_THR_CURR_ADDR(thread));
    	value[1] = CMDQ_REG_GET32(CMDQ_THR_END_ADDR(thread));
    	value[2] = CMDQ_REG_GET32(CMDQ_THR_WAIT_TOKEN(thread));
    	value[3] = CMDQ_GET_COOKIE_CNT(thread);
    	value[4] = CMDQ_REG_GET32(CMDQ_THR_IRQ_STATUS(thread));
    	value[5] = CMDQ_REG_GET32(CMDQ_THR_INST_CYCLES(thread));
    	value[6] = CMDQ_REG_GET32(CMDQ_THR_CURR_STATUS(thread));
    	value[7] = CMDQ_REG_GET32(CMDQ_THR_IRQ_ENABLE(thread));
    	value[8] = CMDQ_REG_GET32(CMDQ_THR_ENABLE_TASK(thread));

    	pBuffer += sprintf(pBuffer, "Index: %d, Enabled: %d, IRQ: 0x%08x, Thread PC: 0x%08x, End: 0x%08x, Wait Token: 0x%08x\n",
    	                            thread, value[8], value[4], value[0], value[1], value[2]);
    	pBuffer += sprintf(pBuffer, "Curr Cookie: %d, Wait Cookie: %d, Next Cookie: %d, Task Count %d,\n", value[3],
    		                        pThread->waitCookie, pThread->nextCookie, pThread->taskCount);
    	pBuffer += sprintf(pBuffer, "Timeout Cycle:%d, Status:0x%08x, IRQ_EN: 0x%08x\n", value[5], value[6], value[7]);
    }while(0);

    // dump display registers...
    length   = pBuffer - buf;
    if (length < PAGE_SIZE)
    {
        pBuffer += primary_display_check_path(pBuffer, (PAGE_SIZE - length));
    }
    
    mutex_unlock(&gCmdqTaskMutex);

    length   = pBuffer - buf;

    BUG_ON(length > PAGE_SIZE);

    return length;

}

static int cmdq_core_print_record(const RecordStruct *pRecord, int index, char *buf, int bufLen)
{
    int length = 0;
    char *unit[5] = {"ms", "ms", "ms", "ms", "ms"};
    int32_t      IRQTime;
    int32_t      execTime;
    int32_t      beginWaitTime;
    int32_t      totalTime;
    int32_t      acquireThreadTime;
    unsigned long rem_nsec = 0;
    CMDQ_TIME submitTimeSec = pRecord->submit;
    rem_nsec = do_div(submitTimeSec, 1000000000);

    unit[0] = "ms";
    unit[1] = "ms";
    unit[2] = "ms";
    unit[3] = "ms";
    unit[4] = "ms";
    CMDQ_GET_TIME_IN_MS(pRecord->submit, pRecord->done, totalTime);
    CMDQ_GET_TIME_IN_MS(pRecord->submit, pRecord->trigger, acquireThreadTime);
    CMDQ_GET_TIME_IN_MS(pRecord->submit, pRecord->beginWait, beginWaitTime)
    CMDQ_GET_TIME_IN_MS(pRecord->trigger, pRecord->gotIRQ,  IRQTime)
    CMDQ_GET_TIME_IN_MS(pRecord->trigger, pRecord->wakedUp, execTime);

    // detect us interval
    if (0 == acquireThreadTime)
    {
        CMDQ_GET_TIME_IN_US_PART(pRecord->submit, pRecord->trigger, acquireThreadTime);
        unit[0] = "us";
    }
    if (0 == IRQTime)
    {
        CMDQ_GET_TIME_IN_US_PART(pRecord->trigger, pRecord->gotIRQ, IRQTime);
        unit[1] = "us";
    }
    if (0 == beginWaitTime)
    {
        CMDQ_GET_TIME_IN_US_PART(pRecord->submit, pRecord->beginWait, beginWaitTime);
        unit[2] = "us";
    }
    if (0 == execTime)
    {
        CMDQ_GET_TIME_IN_US_PART(pRecord->trigger, pRecord->wakedUp, execTime);
        unit[3] = "us";
    }
    if (0 == totalTime)
    {
        CMDQ_GET_TIME_IN_US_PART(pRecord->submit, pRecord->done, totalTime);
        unit[4] = "us";
    }

    length = snprintf(buf,
              bufLen,
              "%4d:(%5d, %2d, 0x%012llx)(%02d, %02d)(%5dns)(%5llu.%06lu, %4d%s, %4d%s, %4d%s, %4d%s)%4d%s\n",
              index,
              pRecord->user,
              pRecord->scenario,
              pRecord->engineFlag,
              pRecord->thread,
              cmdq_core_priority_from_scenario(pRecord->scenario),
              pRecord->writeTimeNS,
              submitTimeSec, rem_nsec / 1000,
              acquireThreadTime, unit[0],
              IRQTime, unit[1],
              beginWaitTime, unit[2],
              execTime, unit[3],
              totalTime, unit[4]);

    return length;
}


ssize_t cmdqCorePrintError(struct device *dev,
                                  struct device_attribute *attr,
                                  char *buf)
{
    int i;
    int length = 0;

    for (i = 0; i < gCmdqContext.errNum && i < CMDQ_MAX_ERROR_COUNT; ++i)
    {
        ErrorStruct *pError = &gCmdqContext.error[i];
        u64 ts = pError->ts_nsec;
        unsigned long rem_nsec = do_div(ts, 1000000000);
        length += snprintf(buf + length, PAGE_SIZE - length, "[%5lu.%06lu] ", (unsigned long)ts, rem_nsec / 1000);
        length += cmdq_core_print_record(&pError->errorRec, i, buf + length, PAGE_SIZE - length);
        if (length >= PAGE_SIZE)
        {
            break;
        }
    }

    return length;
}

int cmdqCorePrintRecordSeq(struct seq_file *m, void *v)
{
    unsigned long flags;
    int32_t      index;
    int32_t      numRec;
    RecordStruct record;
    char         msg[512] = {0};


    // we try to minimize time spent in spin lock
    // since record is an array so it is okay to
    // allow displaying an out-of-date entry.
    spin_lock_irqsave(&gCmdqRecordLock, flags);
    numRec  = gCmdqContext.recNum;
    index = gCmdqContext.lastID - 1;
    spin_unlock_irqrestore(&gCmdqRecordLock, flags);

    // we print record in reverse order.
    for (; numRec > 0; --numRec, --index)
    {
        if (index >= CMDQ_MAX_RECORD_COUNT)
        {
            index = 0;
        }
        else if (index < 0)
        {
            index = CMDQ_MAX_RECORD_COUNT - 1;
        }

        // Make sure we don't print a record that is during updating.
        // However, this record may already be different
        // from the time of entering cmdqCorePrintRecordSeq().
        spin_lock_irqsave(&gCmdqRecordLock, flags);
        record = gCmdqContext.record[index];
        spin_unlock_irqrestore(&gCmdqRecordLock, flags);

        cmdq_core_print_record(&record, index, msg, sizeof(msg));

        seq_printf(m, "%s", msg);
    }

    return 0;
}

int cmdqCorePrintErrorSeq(struct seq_file *m, void *v)
{
    // error is not used by now
    return 0;
}

void cmdq_core_print_thread_seq(int thread, struct seq_file *m)
{
    uint32_t value[10] = {0};
    seq_printf(m, "trigger thread:\n");
    value[0] = CMDQ_REG_GET32(CMDQ_THR_CURR_ADDR(thread));
    value[1] = CMDQ_REG_GET32(CMDQ_THR_END_ADDR(thread));
    value[2] = CMDQ_REG_GET32(CMDQ_THR_WAIT_TOKEN(thread));
    value[3] = CMDQ_GET_COOKIE_CNT(thread);
    value[4] = CMDQ_REG_GET32(CMDQ_THR_IRQ_STATUS(thread));
    value[5] = CMDQ_REG_GET32(CMDQ_THR_INST_CYCLES(thread));
    value[6] = CMDQ_REG_GET32(CMDQ_THR_CURR_STATUS(thread));
    value[7] = CMDQ_REG_GET32(CMDQ_THR_IRQ_ENABLE(thread));
    value[8] = CMDQ_REG_GET32(CMDQ_THR_ENABLE_TASK(thread));

    seq_printf(m, "Index: %d, Enabled: %d, IRQ: 0x%08x, Thread PC: 0x%08x, End: 0x%08x, Wait Token: 0x%08x\n",
                                thread, value[8], value[4], value[0], value[1], value[2]);
    seq_printf(m, "Timeout Cycle:%d, Status:0x%08x, IRQ_EN: 0x%08x\n", value[5], value[6], value[7]);
}

int cmdqCorePrintStatusSeq(struct seq_file *m, void *v)
{
    unsigned long flags = 0;
    EngineStruct *pEngine = NULL;
    TaskStruct   *pTask = NULL;
    struct list_head *p = NULL;
    ThreadStruct *pThread = NULL;
    int32_t      index = 0;
    int32_t      inner = 0;
    int          listIdx = 0;
    const struct list_head *lists[] = {&gCmdqContext.taskFreeList, &gCmdqContext.taskActiveList, &gCmdqContext.taskWaitList};
    const static char *listNames[] = {"Free", "Active", "Wait"};

    const CMDQ_ENG_ENUM  engines[]      = { CMDQ_FOREACH_STATUS_MODULE_PRINT(GENERATE_ENUM) };
    const static char    *engineNames[] = { CMDQ_FOREACH_STATUS_MODULE_PRINT(GENERATE_STRING) };

    #ifdef CMDQ_PWR_AWARE
    seq_printf(m, "====== Clock Status =======\n");
    seq_printf(m, "MT_CG_INFRA_GCE: %d, MT_CG_DISP0_MUTEX_32K: %d\n",
        clock_is_on(MT_CG_INFRA_GCE), clock_is_on(MT_CG_DISP0_MUTEX_32K));
    #endif

    seq_printf(m, "====== Mutex Usage =======\n");
    for (listIdx = 0; listIdx < DISP_MUTEX_MDP_COUNT; ++listIdx)
    {
        if (cmdqMDPMutexInUse(listIdx))
        {
            seq_printf(m, "Mutex %d used by pid:%d\n",
                       listIdx,
                       cmdqMDPMutexOwnerPid(listIdx));
        }
    }

    do
    {
        int32_t coreExecThread = CMDQ_INVALID_THREAD;
        uint32_t value[6] = { 0 };
    
        value[0] = CMDQ_REG_GET32(CMDQ_CURR_LOADED_THR);
        value[1] = CMDQ_REG_GET32(CMDQ_THR_EXEC_CYCLES);
        value[2] = CMDQ_REG_GET32(CMDQ_THR_TIMEOUT_TIMER);
        value[3] = CMDQ_REG_GET32(CMDQ_BUS_CONTROL_TYPE);
    
        coreExecThread = __builtin_ffs(value[0]) - 1;   /* this returns (1 + index of least bit set) or 0 if input is 0. */
        seq_printf(m, "IRQ flag:0x%08x, Execing:%d, Exec Thread:%d, CMDQ_CURR_LOADED_THR: 0x%08x\n",
             CMDQ_REG_GET32(CMDQ_CURR_IRQ_STATUS),
             (0x80000000 & value[0]) ? 1 : 0, coreExecThread, value[0]);
        if (CMDQ_INVALID_THREAD != coreExecThread)
        {   
            seq_printf(m, "exec thread %d dump:\n", coreExecThread);
            cmdq_core_print_thread_seq(coreExecThread, m);
        }
        seq_printf(m, "CMDQ_THR_EXEC_CYCLES:0x%08x, CMDQ_THR_TIMER:0x%08x, CMDQ_BUS_CTRL:0x%08x\n",
                    value[1], value[2], value[3]);
        seq_printf(m, "CMDQ_DEBUG_1: 0x%08x\n", CMDQ_REG_GET32((GCE_BASE_VA + 0xF0)));
        seq_printf(m, "CMDQ_DEBUG_2: 0x%08x\n", CMDQ_REG_GET32((GCE_BASE_VA + 0xF4)));
        seq_printf(m, "CMDQ_DEBUG_3: 0x%08x\n", CMDQ_REG_GET32((GCE_BASE_VA + 0xF8)));
        seq_printf(m, "CMDQ_DEBUG_4: 0x%08x\n", CMDQ_REG_GET32((GCE_BASE_VA + 0xFC)));

    }while(0);
    

    seq_puts(m, "====== Events =======\n");
    cmdq_core_print_event_seq(m);

    seq_printf(m, "====== Engine Usage =======\n");

    for (listIdx = 0; listIdx < (sizeof(engines) / sizeof(engines[0])); ++listIdx)
    {
        pEngine = &gCmdqContext.engine[engines[listIdx]];
        seq_printf(m, "%s: count %d, owner %d, fail: %d, reset: %d\n",
                           engineNames[listIdx],
                           pEngine->userCount,
                           pEngine->currOwner,
                           pEngine->failCount,
                           pEngine->resetCount);
    }


    mutex_lock(&gCmdqTaskMutex);

    // print all tasks in both list
    for (listIdx = 0; listIdx < (sizeof(lists) / sizeof(lists[0])); listIdx++)
    {
        // skip FreeTasks by default
        if (gCmdqContext.logLevel < 2 &&
            listIdx == 0)
        {
            continue;
        }

        index = 0;
        list_for_each(p, lists[listIdx])
        {
            pTask = list_entry(p, struct TaskStruct, listEntry);
            seq_printf(m, "====== %s Task(%d) 0x%p Usage =======\n", listNames[listIdx], index, pTask);

            seq_printf(m, "State %d, VABase: 0x%p, MVABase: %pa, Size: %d\n",
                pTask->taskState, pTask->pVABase, &pTask->MVABase, pTask->commandSize);
            seq_printf(m, "Scenario %d, Priority: %d, Flag: 0x%08llx, VAEnd: 0x%p\n",
                pTask->scenario, pTask->priority, pTask->engineFlag, pTask->pCMDEnd);
            seq_printf(m, "Trigger %lld, IRQ: %lld, Wait: %lld, Wake Up: %lld\n",
                   pTask->trigger,
                   pTask->gotIRQ,
                   pTask->beginWait,
                   pTask->wakedUp);
            ++index;
        }
        seq_printf(m, "====== Total %d %s Task =======\n", index, listNames[listIdx]);
    }

    for (index = 0; index < CMDQ_MAX_THREAD_COUNT; index++)
    {
        pThread = &(gCmdqContext.thread[index]);

        if (pThread->taskCount > 0)
        {
            seq_printf(m, "====== Thread %d Usage =======\n", index);
            seq_printf(m, "Wait Cookie %d, Next Cookie %d\n", pThread->waitCookie, pThread->nextCookie);

            spin_lock_irqsave(&gCmdqThreadLock, flags);

            for (inner = 0; inner < CMDQ_MAX_TASK_IN_THREAD; inner++)
            {
                pTask = pThread->pCurTask[inner];
                if (NULL != pTask)
                {
                    // dump task basic info
                    seq_printf(m, "Slot: %d, Task: 0x%p, Pid: %d, Name: %s, Scn: %d, VABase: 0x%p, MVABase: %pa, Size: %d, Last Command: 0x%08x:0x%08x\n",
                        index,
                        pTask,
                        pTask->callerPid,
                        pTask->callerName,
                        pTask->scenario,
                        pTask->pVABase,
                        &pTask->MVABase,
                        pTask->commandSize,
                        pTask->pCMDEnd[-1],
                        pTask->pCMDEnd[0]);

                    // dump PC info
                    do
                    {
                        uint32_t *pcVA = NULL;
                        uint32_t  insts[4] = {0};
                        char      parsedInstruction[128] = {0};

                        pcVA = cmdq_core_get_pc(pTask, index, insts);
                        if (pcVA)
                        {
                            cmdq_core_parse_instruction(pcVA, parsedInstruction, sizeof(parsedInstruction));
                            seq_printf(m, "PC(VA): 0x%p, 0x%08x:0x%08x => %s",
                                               pcVA,
                                               insts[2],
                                               insts[3],
                                               parsedInstruction);
                        }
                        else
                        {
                            seq_printf(m, "PC(VA): Not available\n");
                        }

                    }while(0);


                }
            }

            spin_unlock_irqrestore(&gCmdqThreadLock, flags);
        }
    }

    cmdq_core_print_thread_seq(CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT, m);

    // dump display registers...
    do
    {
        char dispStatBuf[1024] = {0};
        primary_display_check_path(dispStatBuf, sizeof(dispStatBuf));
        seq_printf(m, dispStatBuf);
    }while(0);
    
    mutex_unlock(&gCmdqTaskMutex);

    return 0;
}

int32_t cmdq_subsys_from_phys_addr(uint32_t physAddr)
{
    const int32_t  msb = (physAddr & 0x0FFFF0000) >> 16;

#undef DECLARE_CMDQ_SUBSYS
#define DECLARE_CMDQ_SUBSYS(addr, id, grp, base) case addr: return id;
    switch(msb)
    {
    #include "cmdq_subsys.h"
    }

    CMDQ_ERR("unrecognized subsys, msb=0x%04x, physAddr:0x%08x\n", msb, physAddr);
    return -1;
#undef DECLARE_CMDQ_SUBSYS
}

ssize_t cmdqCorePrintRecord(struct device *dev,
                            struct device_attribute *attr,
                            char *buf)
{
    unsigned long flags;
    int32_t      begin;
    int32_t      curPos;
    ssize_t     bufLen = PAGE_SIZE;
    ssize_t     length;
    int32_t      index;
    int32_t      numRec;
    RecordStruct record;

    begin = 0;
    curPos = 0;
    length = 0;
    bufLen = PAGE_SIZE;

    // we try to minimize time spent in spin lock
    // since record is an array so it is okay to
    // allow displaying an out-of-date entry.
    spin_lock_irqsave(&gCmdqRecordLock, flags);
    numRec  = gCmdqContext.recNum;
    index = gCmdqContext.lastID - 1;
    spin_unlock_irqrestore(&gCmdqRecordLock, flags);

    // we print record in reverse order.
    for (; numRec > 0; --numRec, --index)
    {
        // CMDQ_ERR("[rec] index=%d numRec =%d\n", index, numRec);

        if (index >= CMDQ_MAX_RECORD_COUNT)
        {
            index = 0;
        }
        else if (index < 0)
        {
            index = CMDQ_MAX_RECORD_COUNT - 1;
        }

        // Make sure we don't print a record that is during updating.
        // However, this record may already be different
        // from the time of entering cmdqCorePrintRecordSeq().
        spin_lock_irqsave(&gCmdqRecordLock, flags);
        record = (gCmdqContext.record[index]);
        spin_unlock_irqrestore(&gCmdqRecordLock, flags);

        length = cmdq_core_print_record(&record, index, &buf[curPos], bufLen);

        bufLen -= length;
        curPos += length;

        if (bufLen <= 0 || curPos >= PAGE_SIZE)
        {
            break;
        }
    }

    if (curPos >= PAGE_SIZE)
    {
        curPos = PAGE_SIZE;
    }

    return curPos;
}

//
// For kmemcache, initialize variables of TaskStruct (but not buffers)
DEBUG_STATIC void cmdq_core_task_ctor(void *param)
{
    struct TaskStruct *pTask = (TaskStruct*)param;

    CMDQ_VERBOSE("cmdq_core_task_ctor: 0x%p\n", param);
    memset(pTask, 0, sizeof(TaskStruct));
    INIT_LIST_HEAD(&(pTask->listEntry));
    pTask->taskState = TASK_STATE_IDLE;
    pTask->thread = CMDQ_INVALID_THREAD;
    return;
}

void cmdq_task_free_task_command_buffer(TaskStruct *pTask)
{
    if (pTask->pVABase)
    {
        if (pTask->useEmergencyBuf)
        {
            cmdq_core_free_emergency_buffer(pTask->pVABase, pTask->MVABase);
        }
        else
        {
            dma_free_coherent(cmdq_dev_get(), pTask->bufferSize, pTask->pVABase, pTask->MVABase);
        }

        pTask->pVABase = NULL;
        pTask->MVABase = 0;
        pTask->bufferSize = 0;
        pTask->commandSize = 0;
        pTask->pCMDEnd = NULL;
    }
}

//
//  Ensures size of command buffer of the given task.
//  Existing buffer will be copied to new buffer.
//
//  This buffer is guranteed to be physically continous.
//
//  returns -ENOMEM if cannot allocate new buffer
DEBUG_STATIC int32_t cmdq_core_task_realloc_buffer_size(TaskStruct *pTask, uint32_t size)
{
    void *pNewBuffer = NULL;
    dma_addr_t newMVABase = 0;
    int32_t commandSize = 0;
    uint32_t *pCMDEnd = NULL;

    if (pTask->pVABase && pTask->bufferSize >= size)
    {
        // buffer size is already good, do nothing.
        return 0;
    }

    do
    {
        // allocate new buffer, try if we can alloc without reclaim
        pNewBuffer = dma_alloc_coherent(cmdq_dev_get(), size, &newMVABase, GFP_KERNEL | __GFP_NO_KSWAPD);

        if (pNewBuffer)
        {
            pTask->useEmergencyBuf = false;
            break;
        }

        // failed. Try emergency buffer
        if (size <= CMDQ_EMERGENCY_BLOCK_SIZE)
        {
            cmdq_core_alloc_emergency_buffer(&pNewBuffer, &newMVABase);
        }
        if (pNewBuffer)
        {
            CMDQ_LOG("emergency buffer %p allocated\n", pNewBuffer);
            pTask->useEmergencyBuf = true;
            break;
        }

        // finally try reclaim
        pNewBuffer = dma_alloc_coherent(cmdq_dev_get(), size, &newMVABase, GFP_KERNEL);
        if (pNewBuffer)
        {
            CMDQ_LOG("buffer %p allocated after reclaim\n", pNewBuffer);
            pTask->useEmergencyBuf = false;
            break;
        }
    }while(0);
    
    if (NULL == pNewBuffer)
    {
        CMDQ_ERR("realloc cmd buffer of size %d failed\n", size);
        return -ENOMEM;
    }

    memset(pNewBuffer, 0, size);

    // copy and release old buffer
    if (pTask->pVABase)
    {
        memcpy(pNewBuffer, pTask->pVABase, pTask->bufferSize);
    }
    /* we should keep track of pCMDEnd and cmdSize since they are cleared in free command buffer */
    pCMDEnd = pTask->pCMDEnd;
    commandSize = pTask->commandSize;
    cmdq_task_free_task_command_buffer(pTask);

    /* attach the new buffer */
    pTask->pVABase = (uint32_t *) pNewBuffer;
    pTask->MVABase = newMVABase;
    pTask->bufferSize = size;
    pTask->pCMDEnd = pCMDEnd;
    pTask->commandSize = commandSize;

    CMDQ_MSG("Task Buffer:0x%p, VA:%p PA:%pa\n", pTask, pTask->pVABase, &pTask->MVABase);
    return 0;
}

//
//  Allocate and initialize TaskStruct and its command buffer
DEBUG_STATIC TaskStruct* cmdq_core_task_create(void)
{
    struct TaskStruct *pTask = NULL;
    int32_t status = 0;

    pTask = (TaskStruct*)kmem_cache_alloc(gCmdqContext.taskCache, GFP_KERNEL);
    status = cmdq_core_task_realloc_buffer_size(pTask, CMDQ_INITIAL_CMD_BLOCK_SIZE);
    if(status < 0)
    {
        CMDQ_AEE("CMDQ", "Allocate command buffer failed\n");
        kmem_cache_free(gCmdqContext.taskCache, pTask);
        pTask = NULL;
        return NULL;
    }
    return pTask;
}

void cmdq_core_reset_hw_events(void)
{
    // set all defined events to 0
    CMDQ_MSG("cmdq_core_reset_hw_events\n");

#undef DECLARE_CMDQ_EVENT
#define DECLARE_CMDQ_EVENT(name, val) CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (CMDQ_SYNC_TOKEN_MAX & name));
    #include "cmdq_event.h"
#undef DECLARE_CMDQ_EVENT

    // However, GRP_SET are resource flags,
    // by default they should be 1.
    cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_GPR_SET_0);
    cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_GPR_SET_1);
    cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_GPR_SET_2);
    cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_GPR_SET_3);
    cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_GPR_SET_4);

    #if 0
    do
    {
        uint32_t value = 0;
        CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_ID, 0x1FF & CMDQ_EVENT_MDP_WDMA_EOF);
        value = CMDQ_REG_GET32(CMDQ_SYNC_TOKEN_VAL);
        CMDQ_ERR("[DEBUG] CMDQ_EVENT_MDP_WDMA_EOF after reset is %d\n", value);
    }while(0);
    #endif

    return;
}

#if 0
uint32_t *addressToDump[3] = {IO_VIRT_TO_PHYS(MMSYS_CONFIG_BASE + 0x0890),
                         IO_VIRT_TO_PHYS(MMSYS_CONFIG_BASE + 0x0890),
                         IO_VIRT_TO_PHYS(MMSYS_CONFIG_BASE + 0x0890)};

static int32_t testcase_regdump_begin(uint32_t taskID, uint32_t *regCount, uint32_t **regAddress)
{
    CMDQ_MSG("@@@@@@@@@@@@@@@@@@ testcase_regdump_begin, tid = %d\n", taskID);
    *regCount = 3;
    *regAddress = addressToDump;
    return 0;
}

static int32_t testcase_regdump_end(uint32_t taskID, uint32_t regCount, uint32_t *regValues)
{
    int i;
    CMDQ_MSG("@@@@@@@@@@@@@@@@@@ testcase_regdump_end, tid = %d\n", taskID);
    CMDQ_MSG("@@@@@@@@@@@@@@@@@@ regCount = %d\n", regCount);

    for (i = 0; i < regCount; ++i)
    {
        CMDQ_MSG("@@@@@@@@@@@@@@@@@@ regValue[%d] = 0x%08x\n", i, regValues[i]);
    }

    return 0;
}
#endif

void cmdq_core_reset_engine_struct(void)
{
    struct EngineStruct   *pEngine;
    int index;
    
    // Reset engine status
    pEngine = gCmdqContext.engine;
    for (index = 0; index < CMDQ_MAX_ENGINE_COUNT; index++)
    {
        pEngine[index].currOwner = CMDQ_INVALID_THREAD;
    }
}

void cmdq_core_reset_thread_struct()
{
    struct ThreadStruct   *pThread;    
    int index;
    
    // Reset thread status
    pThread = &(gCmdqContext.thread[0]);
    for(index = 0; index < CMDQ_MAX_THREAD_COUNT; index++)
    {
        pThread[index].allowDispatching = 1; 
    }
}

int32_t cmdqCoreInitialize(void)
{
    struct TaskStruct     *pTask;
    int32_t               index;

    atomic_set(&gCmdqThreadUsage, 0);

    BUG_ON(0 != atomic_read(&gCmdqThreadUsage));

    for (index = 0; index < CMDQ_MAX_THREAD_COUNT; index++)
    {
        init_waitqueue_head(&gCmdWaitQueue[index]);
    }

    init_waitqueue_head(&gCmdqThreadDispatchQueue);

    // Reset overall context
    memset(&gCmdqContext, 0x0, sizeof(ContextStruct));
    // some fields has non-zero initial value
    cmdq_core_reset_engine_struct();
    cmdq_core_reset_thread_struct();

    // Create task pool
    gCmdqContext.taskCache = kmem_cache_create(CMDQ_DRIVER_DEVICE_NAME "_task",
                                               sizeof(struct TaskStruct),
                                               __alignof__(struct TaskStruct),
                                               SLAB_POISON | SLAB_HWCACHE_ALIGN | SLAB_RED_ZONE,
                                               &cmdq_core_task_ctor
                                               );
    // Initialize task lists
    INIT_LIST_HEAD(&gCmdqContext.taskFreeList);
    INIT_LIST_HEAD(&gCmdqContext.taskActiveList);
    INIT_LIST_HEAD(&gCmdqContext.taskWaitList);
    INIT_WORK(&gCmdqContext.taskConsumeWaitQueueItem, cmdq_core_consume_waiting_list);

    // Initialize writable address
    INIT_LIST_HEAD(&gCmdqContext.writeAddrList);

    // Initialize emergency buffer
    cmdq_core_init_emergency_buffer();

    gCmdqContext.taskAutoReleaseWQ = create_singlethread_workqueue("cmdq_auto_release");
    gCmdqContext.taskConsumeWQ = create_singlethread_workqueue("cmdq_task");

    // pre-allocate free tasks
    for (index = 0; index < CMDQ_INIT_FREE_TASK_COUNT; index++)
    {
        pTask = cmdq_core_task_create();
        if (pTask)
        {
            mutex_lock(&gCmdqTaskMutex);
            list_add_tail(&(pTask->listEntry), &gCmdqContext.taskFreeList);
            mutex_unlock(&gCmdqTaskMutex);
        }
    }

    #if 0
    //cmdqCoreRegisterDebugRegDumpCB(testcase_regdump_begin, testcase_regdump_end);
    #endif

    // Initialize MET for statistics
    // note that we don't need to uninit it.
    CMDQ_PROF_INIT();
    cmdq_mmp_init();

    return 0;
}


bool cmdq_core_is_valid_group(CMDQ_GROUP_ENUM engGroup)
{
    // check range
    if (engGroup < 0 || engGroup >= CMDQ_MAX_GROUP_COUNT)
    {
        return false;
    }
    return true;
}

int32_t cmdq_core_is_group_flag(CMDQ_GROUP_ENUM engGroup, uint64_t engineFlag)
{
    if (!cmdq_core_is_valid_group(engGroup))
    {
        return false;
    }

    if (gCmdqEngineGroupBits[engGroup] & engineFlag)
    {
        return true;
    }
    return false;
}

static inline uint32_t cmdq_core_get_task_timeout_cycle(struct ThreadStruct *pThread)
{
    // if there is loop callback, this thread is in loop mode,
    // and should not have a timeout.
    // So pass 0 as "no timeout"

    // return pThread->loopCallback ? 0 : CMDQ_MAX_INST_CYCLE;

    // HACK: disable HW timeout
    return 0;
}

bool cmdq_core_should_profile(CMDQ_SCENARIO_ENUM scenario)
{
    switch (scenario)
    {
    case CMDQ_SCENARIO_PRIMARY_DISP:
    case CMDQ_SCENARIO_PRIMARY_ALL:
    case CMDQ_SCENARIO_DEBUG_PREFETCH:
    case CMDQ_SCENARIO_DEBUG:
        return true;
    default:
        return false;
    }
    return false;
}

void cmdqCoreInitGroupCB(void)
{
    memset(&(gCmdqGroupCallback), 0x0, sizeof(gCmdqGroupCallback));
    memset(&(gCmdqDebugCallback), 0x0, sizeof(gCmdqDebugCallback));
}

void cmdqCoreDeinitGroupCB(void)
{
    memset(&(gCmdqGroupCallback), 0x0, sizeof(gCmdqGroupCallback));
    memset(&(gCmdqDebugCallback), 0x0, sizeof(gCmdqDebugCallback));
}

int32_t cmdqCoreRegisterCB(CMDQ_GROUP_ENUM   engGroup,
                           CmdqClockOnCB     clockOn,
                           CmdqDumpInfoCB    dumpInfo,
                           CmdqResetEngCB    resetEng,
                           CmdqClockOffCB    clockOff)
{
    CmdqCBkStruct *pCallback;

    if (!cmdq_core_is_valid_group(engGroup))
    {
        return -EFAULT;
    }

    CMDQ_MSG("Register %d group engines' callback\n", engGroup);
    CMDQ_MSG("clockOn:  0x%pf, dumpInfo: 0x%pf\n", clockOn, dumpInfo);
    CMDQ_MSG("resetEng: 0x%pf, clockOff: 0x%pf\n", resetEng, clockOff);

    pCallback = &(gCmdqGroupCallback[engGroup]);

    pCallback->clockOn  = clockOn;
    pCallback->dumpInfo = dumpInfo;
    pCallback->resetEng = resetEng;
    pCallback->clockOff = clockOff;

    return 0;
}

int32_t cmdqCoreRegisterDebugRegDumpCB(CmdqDebugRegDumpBeginCB beginCB,
                                       CmdqDebugRegDumpEndCB endCB)
{
    CMDQ_VERBOSE("Register reg dump: begin=%p, end=%p\n", beginCB, endCB);
    gCmdqDebugCallback.beginDebugRegDump = beginCB;
    gCmdqDebugCallback.endDebugRegDump = endCB;
    return 0;
}

DEBUG_STATIC void cmdq_core_release_task_unlocked(TaskStruct *pTask)
{
    pTask->taskState = TASK_STATE_IDLE;
    pTask->thread = CMDQ_INVALID_THREAD;

    if (pTask->profileData)
    {
        dma_free_coherent(NULL,
                         2 * sizeof(uint32_t),
                         pTask->profileData,
                         pTask->profileDataPA);
        pTask->profileData = NULL;
        pTask->profileDataPA = 0;
    }

    if (pTask->regResults)
    {
        CMDQ_MSG("COMMAND: Free result buf VA:0x%p, PA:%pa\n", pTask->regResults, &pTask->regResultsMVA);
        dma_free_coherent(NULL,
                          pTask->regCount * sizeof(pTask->regResults[0]),
                          pTask->regResults,
                          pTask->regResultsMVA);
    }
    pTask->regResults = NULL;
    pTask->regResultsMVA = 0;
    pTask->regCount = 0;

    cmdq_task_free_task_command_buffer(pTask);
    
    // remove from active/waiting list
    list_del_init(&(pTask->listEntry));
    // insert into free list. Currently we don't shrink free list.
    list_add_tail(&(pTask->listEntry), &gCmdqContext.taskFreeList);
}

DEBUG_STATIC void cmdq_core_release_task(TaskStruct *pTask)
{
    CMDQ_MSG("-->TASK: Release task structure 0x%p begin\n", pTask);

    mutex_lock(&gCmdqTaskMutex);

    cmdq_core_release_task_unlocked(pTask);

    mutex_unlock(&gCmdqTaskMutex);

    CMDQ_MSG("<--TASK: Release task structure end\n");
}

DEBUG_STATIC TaskStruct* cmdq_core_find_free_task(void)
{
    TaskStruct *pTask = NULL;

    mutex_lock(&gCmdqTaskMutex);

    // Pick from free list first;
    // create one if there is no free entry.
    if (list_empty(&gCmdqContext.taskFreeList))
    {
        pTask = cmdq_core_task_create();
    }
    else
    {
        pTask = list_first_entry(&(gCmdqContext.taskFreeList), TaskStruct, listEntry);
        // remove from free list
        list_del_init(&(pTask->listEntry));
    }

    mutex_unlock(&gCmdqTaskMutex);

    return pTask;
}

static void cmdq_core_append_command_into_task(TaskStruct *pTask, uint32_t argA, uint32_t argB)
{
    pTask->pCMDEnd[1] = argB;
    pTask->pCMDEnd[2] = argA;
    pTask->commandSize += 1 * CMDQ_INST_SIZE;
    pTask->pCMDEnd += 2;
}

static void cmdq_core_insert_backup_instruction(TaskStruct *pTask,
                                                const uint32_t regAddr,
                                                const dma_addr_t writeAddress,
                                                const CMDQ_DATA_REGISTER_ENUM valueRegId,
                                                const CMDQ_DATA_REGISTER_ENUM destRegId)
{
    uint32_t argA;
    int32_t subsysCode;

    // register to read from
    // note that we force convert to physical reg address.
    // if it is already physical address, it won't be affected (at least on this platform)
    argA = IO_VIRT_TO_PHYS(regAddr);
    subsysCode = cmdq_subsys_from_phys_addr(argA);

    // CMDQ_ERR("test %d\n", __LINE__);

    //
    // Load into 32-bit GPR (R0-R15)
    cmdq_core_append_command_into_task(pTask,
                    (CMDQ_CODE_READ << 24) | (argA & 0xffff) | ((subsysCode & 0x1f)<< 16) | (2 << 21), //  0 1 0
                    valueRegId);


    // CMDQ_ERR("test %d\n", __LINE__);

    // Note that <MOVE> argB is 48-bit
    // so writeAddress is split into 2 parts
    // and we store address in 64-bit GPR (P0-P7)
    cmdq_core_append_command_into_task(pTask,
                    (CMDQ_CODE_MOVE << 24) |
                    #ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
                    ((writeAddress >> 32) & 0xffff) |
                    #endif
                    ((destRegId & 0x1f) << 16) | (4 << 21),
                    (uint32_t)writeAddress);

    // CMDQ_ERR("test %d\n", __LINE__);

    // write to memory
    cmdq_core_append_command_into_task(pTask,
                    (CMDQ_CODE_WRITE << 24) | (0 & 0xffff) | ((destRegId & 0x1f)<< 16) | (6 << 21), // 1 0
                    valueRegId);

    CMDQ_VERBOSE("COMMAND: copy reg:0x%08x to phys:%pa, GPR(%d, %d)\n", argA, &writeAddress, valueRegId, destRegId);

    // CMDQ_ERR("test %d\n", __LINE__);
}

DEBUG_STATIC int32_t cmdq_core_insert_read_reg_command(TaskStruct *pTask, cmdqCommandStruct *pCommandDesc)
{
    int32_t status = 0;
    uint32_t extraBufferSize = 0;
    int i = 0;
    CMDQ_DATA_REGISTER_ENUM valueRegId;
    CMDQ_DATA_REGISTER_ENUM destRegId;
    CMDQ_EVENT_ENUM         regAccessToken;
    uint32_t prependBufferSize = 0;

    bool profileCommand = false;
    uint32_t copiedBeforeProfile = 0;

    #define CMDQ_PROFILE_COMMAND

    // calculate required buffer size
    // we need to consider {READ, MOVE, WRITE} for each register
    // and the SYNC in the begin and end
    if (pTask->regCount)
    {
        extraBufferSize = (3 * CMDQ_INST_SIZE * pTask->regCount) + (2 * CMDQ_INST_SIZE);
    }
    else
    {
        extraBufferSize = 0;
    }


    CMDQ_VERBOSE("test %d, original command size = %d\n", __LINE__, pTask->commandSize);

    #ifdef CMDQ_PROFILE_COMMAND
    // do not insert profile command for trigger loop
    #if 1
    profileCommand = pTask &&
                    cmdq_core_should_profile(pTask->scenario) &&
                    (pTask->loopCallback == NULL) &&
                    (pTask->profileData != NULL);

    #endif

    if (profileCommand)
    {
        // backup GPT at begin and end
        extraBufferSize += (CMDQ_INST_SIZE * 3);

        // insert after the first MARKER instruction
        // and first SYNC instruction (if any)
        prependBufferSize += 2 * (CMDQ_INST_SIZE * 3);
        if (pTask->commandSize < prependBufferSize)
        {
            prependBufferSize = 0;
        }
    }
    #endif

    status = cmdq_core_task_realloc_buffer_size(pTask, pTask->commandSize + prependBufferSize + extraBufferSize);
    if (status < 0)
    {
        CMDQ_ERR("finalize command buffer failed to realloc, pTask=0x%p, requireSize=%d\n", pTask, pTask->commandSize + prependBufferSize + extraBufferSize);
        return status;
    }

    #ifdef CMDQ_PROFILE_COMMAND

    // pCMDEnd start from beginning. Note it is out-of-sync with pTask->commandSize.
    pTask->pCMDEnd = pTask->pVABase - 1;


    if (profileCommand)
    {
        if (cmdq_core_should_enable_prefetch(pTask->scenario))
        {
            // HACK:
            // MARKER + WAIT_FOR_EOF
            copiedBeforeProfile = 2 * CMDQ_INST_SIZE;
        }
        else
        {
            // HACK: we copy the 1st "WAIT FOR EOF" instruction,
            // because this is the point where we start writing registers!
            copiedBeforeProfile = 1 * CMDQ_INST_SIZE;
        }


        memcpy(pTask->pCMDEnd + 1, pCommandDesc->pVABase, copiedBeforeProfile);
        pTask->pCMDEnd = pTask->pVABase + (copiedBeforeProfile/sizeof(pTask->pVABase[0])) - 1;

        // now we start insert backup instructions
        CMDQ_VERBOSE("test %d\n", __LINE__);
        do{
            CMDQ_VERBOSE("[BACKUP]va=%p, pa=%pa, task=%p\n", pTask->profileData, &pTask->profileDataPA, pTask);

            cmdq_core_insert_backup_instruction(pTask,
                                            CMDQ_APXGPT2_COUNT,
                                            pTask->profileDataPA,
                                            CMDQ_DATA_REG_JPEG,
                                            CMDQ_DATA_REG_JPEG_DST);
        }while(0);
        // this increases pTask->commandSize
    }
    #endif

    //  Copy the commands to our DMA buffer
    if (CMDQ_SCENARIO_USER_SPACE != pTask->scenario)
    {
        CMDQ_VERBOSE("COMMAND: Copy kernel to 0x%p\n", pTask->pVABase);
        memcpy(pTask->pCMDEnd + 1, pCommandDesc->pVABase + (copiedBeforeProfile/sizeof(pCommandDesc->pVABase[0])), pCommandDesc->blockSize - copiedBeforeProfile);
    }
    else
    {
        CMDQ_VERBOSE("COMMAND: Copy user to 0x%p\n", pTask->pVABase);
        if (copy_from_user(pTask->pCMDEnd + 1, pCommandDesc->pVABase + (copiedBeforeProfile/sizeof(pCommandDesc->pVABase[0])), pCommandDesc->blockSize - copiedBeforeProfile))
        {
            CMDQ_AEE("CMDQ", "CRDISPATCH_KEY:CMDQ Fail to copy from user 0x%p, size:%d\n", pCommandDesc->pVABase, pCommandDesc->blockSize);
            return -ENOMEM;
        }
    }

    // re-adjust pCMDEnd according to commandSize
    pTask->pCMDEnd = pTask->pVABase + (pTask->commandSize / sizeof(pTask->pVABase[0])) - 1;

    CMDQ_VERBOSE("test %d, CMDEnd=%p, base=%p, cmdSize=%d\n", __LINE__, pTask->pCMDEnd, pTask->pVABase, pTask->commandSize);

    // if no read request, no post-process needed.
    if (0 == pTask->regCount && extraBufferSize == 0)
    {
        return 0;
    }
    else
    {
        // move EOC+JUMP to the new end
        memcpy(pTask->pCMDEnd + 1 - 4 + (extraBufferSize / sizeof(pTask->pCMDEnd[0])),
               &pTask->pCMDEnd[-3],
               2 * CMDQ_INST_SIZE);

        // start from old EOC (replace it)
        pTask->pCMDEnd -= 4;

        if (pTask->regCount)
        {
            CMDQ_VERBOSE("COMMAND:allocate register output section\n");
            // allocate register output section
            BUG_ON(pTask->regResults);
            pTask->regResults = dma_alloc_coherent(NULL,
                                                   pTask->regCount * sizeof(pTask->regResults[0]),
                                                   &pTask->regResultsMVA,
                                                   GFP_KERNEL);
            CMDQ_MSG("COMMAND: result buf VA:0x%p, PA:%pa\n", pTask->regResults, &pTask->regResultsMVA);



            // allocate GPR resource
            cmdq_core_get_reg_id_from_hwflag(pTask->engineFlag, &valueRegId, &destRegId, &regAccessToken);

            // use SYNC TOKEN to make sure only 1 thread access at a time
            // bit 0-11: wait_value
            // bit 15: to_wait, true
            // bit 31: to_update, true
            // bit 16-27: update_value
            cmdq_core_append_command_into_task(pTask,
                                (CMDQ_CODE_WFE << 24) | regAccessToken,
                                ((1 << 31) | (1 << 15) | 1) // wait and clear
                                );

            for (i = 0; i < pTask->regCount; ++i)
            {
                cmdq_core_insert_backup_instruction(pTask,
                                                    pCommandDesc->regRequest.regAddresses[i],
                                                    pTask->regResultsMVA + (i * sizeof(pTask->regResults[0])),
                                                    valueRegId,
                                                    destRegId);
            }

            cmdq_core_append_command_into_task(pTask,
                                (CMDQ_CODE_WFE << 24) | regAccessToken,
                                ((1 << 31) | (1 << 16)) // set directly
                                );
        }

        #ifdef CMDQ_PROFILE_COMMAND
        if (profileCommand)
        {
            cmdq_core_insert_backup_instruction(pTask,
                                                CMDQ_APXGPT2_COUNT,
                                                pTask->profileDataPA + sizeof(uint32_t),
                                                CMDQ_DATA_REG_JPEG,
                                                CMDQ_DATA_REG_JPEG_DST);
        }
        // this increases pTask->commandSize
        #endif

        // move END to copied EOC+JUMP
        pTask->pCMDEnd += 4;
    }

    // make sure instructions are really in DRAM
    smp_mb();

    #if 0 // HACK
    if (cmdq_core_should_profile(pTask->scenario))
    {
        cmdqCoreDebugDumpCommand(pTask);
    }
    #endif

    CMDQ_MSG("COMMAND: size = %d, end = 0x%p\n", pTask->commandSize, pTask->pCMDEnd);

    return status;
}


DEBUG_STATIC TaskStruct* cmdq_core_acquire_task(cmdqCommandStruct *pCommandDesc,
                                                CmdqInterruptCB loopCB,
                                                unsigned long loopData)
{
    TaskStruct *pTask = NULL;
    int32_t status;

    CMDQ_MSG("-->TASK: acquire task begin CMD: 0x%p, size: %d, Eng: 0x%016llx\n", pCommandDesc->pVABase, pCommandDesc->blockSize, pCommandDesc->engineFlag);
    CMDQ_PROF_START(current->pid, __FUNCTION__);

    pTask = cmdq_core_find_free_task();
    do
    {
        if (NULL == pTask)
        {
            CMDQ_AEE("CMDQ", "Can't acquire task info\n");
            break;
        }

        // initialize field values
        pTask->scenario   = pCommandDesc->scenario;
        pTask->priority   = pCommandDesc->priority;
        pTask->engineFlag = pCommandDesc->engineFlag;
        pTask->privateData = pCommandDesc->privateData;
        pTask->loopCallback = loopCB;
        pTask->loopData = loopData;
        pTask->taskState = TASK_STATE_WAITING;

        // profile data
        if (cmdq_core_should_profile(pTask->scenario))
        {
            pTask->profileData = dma_alloc_coherent(NULL,
                                2 * sizeof(uint32_t),
                                &pTask->profileDataPA,
                                GFP_KERNEL);
        }
        else
        {
            pTask->profileData = NULL;
        }


        // profile timers
        memset(&(pTask->trigger), 0x0, sizeof(pTask->trigger));
        memset(&(pTask->gotIRQ), 0x0, sizeof(pTask->gotIRQ));
        memset(&(pTask->beginWait), 0x0, sizeof(pTask->beginWait));
        memset(&(pTask->wakedUp), 0x0, sizeof(pTask->wakedUp));

        pTask->commandSize  = pCommandDesc->blockSize;
        pTask->regCount = pCommandDesc->regRequest.count;

        // store caller info for debug
        if (current)
        {
            pTask->callerPid = current->pid;
            memcpy(pTask->callerName, current->comm, sizeof(current->comm));
        }

        status = cmdq_core_insert_read_reg_command(pTask, pCommandDesc);
        if (0 > status)
        {
            // raise AEE first
            CMDQ_AEE("CMDQ", "Can't alloc command buffer\n");

            // then release task
            cmdq_core_release_task(pTask);
            pTask = NULL;
        }
    }while(0);

    //
    // insert into waiting list to process
    //
    mutex_lock(&gCmdqTaskMutex);
    if (pTask)
    {
        struct list_head *insertAfter = &gCmdqContext.taskWaitList;

        struct TaskStruct *taskEntry = NULL;
        struct list_head *p = NULL;

        pTask->submit = sched_clock();

        // add to waiting list, keep it sorted by priority
        // so that we add high-priority tasks first.
        list_for_each(p, &gCmdqContext.taskWaitList)
        {
            taskEntry = list_entry(p, struct TaskStruct, listEntry);
            // keep the list sorted.
            // higher priority tasks are inserted in front of the queue
            if (taskEntry->priority < pTask->priority)
            {
                break;
            }
            insertAfter = p;
        }

        list_add(&(pTask->listEntry), insertAfter);
    }
    mutex_unlock(&gCmdqTaskMutex);

    CMDQ_MSG("<--TASK: acquire task 0x%p end\n", pTask);
    CMDQ_PROF_END(current->pid, __FUNCTION__);
    return pTask;
}

bool cmdqIsValidTaskPtr(void *pTask)
{
    struct TaskStruct *ptr = NULL;
    struct list_head *p = NULL;
    bool ret = false;

    mutex_lock(&gCmdqTaskMutex);

    list_for_each(p, &gCmdqContext.taskActiveList)
    {
        ptr = list_entry(p, struct TaskStruct, listEntry);
        if (ptr == pTask && TASK_STATE_IDLE != ptr->taskState)
        {
            ret = true;
            break;
        }
    }

    list_for_each(p, &gCmdqContext.taskWaitList)
    {
        ptr = list_entry(p, struct TaskStruct, listEntry);
        if (ptr == pTask && TASK_STATE_WAITING == ptr->taskState)
        {
            ret = true;
            break;
        }
    }

    mutex_unlock(&gCmdqTaskMutex);
    return ret;
}

static void cmdq_core_enable_common_clock_locked(bool enable, const uint64_t engineFlag)
{
    // CMDQ(GCE) clock
    if (enable)
    {
        CMDQ_VERBOSE("[CLOCK] Enable CMDQ(GCE) Clock test=%d\n", atomic_read(&gCmdqThreadUsage));

        if (0 == atomic_read(&gCmdqThreadUsage))
        {
            // CMDQ init flow:
            // 1. clock-on
            // 2. reset all events
            #ifdef CMDQ_PWR_AWARE
            CMDQ_LOG("[CLOCK] Enable CMDQ(GCE) Clock\n");
            enable_clock(MT_CG_INFRA_GCE, CMDQ_DRIVER_DEVICE_NAME);


            CMDQ_LOG("[CLOCK] Enable SMI & LARB0 Clock\n");
            enable_clock(MT_CG_DISP0_SMI_COMMON, "CMDQ_MDP");
            enable_clock(MT_CG_DISP0_SMI_LARB0, "CMDQ_MDP");

            CMDQ_LOG("[CLOCK] enable MT_CG_DISP0_MUTEX_32K\n");
            enable_clock(MT_CG_DISP0_MUTEX_32K, "CMDQ_MDP");
            #endif
            cmdq_core_reset_hw_events();
        }
        atomic_inc(&gCmdqThreadUsage);
    }
    else
    {
        atomic_dec(&gCmdqThreadUsage);
        if (0 >= atomic_read(&gCmdqThreadUsage))
        {
            #ifdef CMDQ_PWR_AWARE
            CMDQ_LOG("[CLOCK] Disable CMDQ(GCE) Clock\n");
            disable_clock(MT_CG_INFRA_GCE, CMDQ_DRIVER_DEVICE_NAME);


            CMDQ_LOG("[CLOCK] Disable SMI & LARB0 Clock\n");
            // disable, reverse the sequence
            disable_clock(MT_CG_DISP0_SMI_LARB0, "CMDQ_MDP");
            disable_clock(MT_CG_DISP0_SMI_COMMON, "CMDQ_MDP");


            CMDQ_LOG("[CLOCK] disable MT_CG_DISP0_MUTEX_32K\n");
            disable_clock(MT_CG_DISP0_MUTEX_32K, "CMDQ_MDP");
            #endif
        }
    }
}

static int32_t gCmdqISPClockCounter = 0;

static void cmdq_core_enable_clock(uint64_t engineFlag,
                                   int32_t  thread)
{
    EngineStruct  *pEngine;
    uint64_t      engines;
    int32_t       index;
    CmdqCBkStruct *pCallback;
    int32_t status;

    CMDQ_VERBOSE("-->CLOCK: Enable flag 0x%llx thread %d begin\n", engineFlag, thread);

    // enable fundamental clocks if needed
    cmdq_core_enable_common_clock_locked(true, engineFlag);

    pEngine = gCmdqContext.engine;
    engines = 0;
    for (index = 0; index < CMDQ_MAX_ENGINE_COUNT; index++)
    {
        if (engineFlag & (1LL << index))
        {
            if (pEngine[index].userCount <= 0)
            {
                pEngine[index].currOwner = thread;
                engines |= (1LL << index);
            }

            pEngine[index].userCount++;
        }
    }

    pCallback = gCmdqGroupCallback;

    // ISP special check: Always call ISP on/off if this task
    // involves ISP. Ignore the ISP HW flags.
    if (cmdq_core_is_group_flag(CMDQ_GROUP_ISP, engineFlag))
    {
        CMDQ_VERBOSE("CLOCK: enable group %d clockOn\n", CMDQ_GROUP_ISP);
        
        if(NULL == pCallback[CMDQ_GROUP_ISP].clockOn)
        {
            CMDQ_ERR("CLOCK: enable group %d clockOn func NULL\n", CMDQ_GROUP_ISP);
        }
        else
        {
            status = pCallback[CMDQ_GROUP_ISP].clockOn(gCmdqEngineGroupBits[CMDQ_GROUP_ISP] & engineFlag);

            #if 1
            ++gCmdqISPClockCounter;
            #endif

            if (status < 0)
            {
                CMDQ_ERR("CLOCK: enable group %d clockOn failed\n", CMDQ_GROUP_ISP);
            }
        }
    }

    for (index = CMDQ_MAX_GROUP_COUNT - 1; index >= 0; --index)
    {
        // note that DISPSYS controls their own clock on/off
        if (CMDQ_GROUP_DISP == index)
        {
            continue;
        }

        // note that ISP is per-task on/off, not per HW flag
        if (CMDQ_GROUP_ISP == index)
        {
            continue;
        }

        if (cmdq_core_is_group_flag((CMDQ_GROUP_ENUM)index, engines))
        {
            CMDQ_MSG("CLOCK: enable group %d clockOn\n", index);
            if(NULL == pCallback[index].clockOn)
            {
                CMDQ_ERR("CLOCK: enable group %d clockOn func NULL\n", index);
                continue;
            }
            status = pCallback[index].clockOn(gCmdqEngineGroupBits[index] & engines);
            if (status < 0)
            {
                CMDQ_ERR("CLOCK: enable group %d clockOn failed\n", index);
            }
        }
    }

    CMDQ_MSG("<--CLOCK: Enable hardware clock end\n");
}


DEBUG_STATIC int32_t cmdq_core_acquire_thread(uint64_t engineFlag,
                                              CMDQ_HW_THREAD_PRIORITY_ENUM thread_prio,
                                              CMDQ_SCENARIO_ENUM scenario,
                                              bool forceLog)
{
    EngineStruct *pEngine;
    ThreadStruct *pThread;
    unsigned long flags;
    unsigned long flagsExecLock;    
    uint64_t     engine;
    uint32_t     free;
    int32_t      index;
    int32_t      thread;
    bool         shouldWait = false;

    pEngine = gCmdqContext.engine;
    pThread = gCmdqContext.thread;

    CMDQ_PROF_START(current->pid, __FUNCTION__);

    do
    {
        spin_lock_irqsave(&gCmdqThreadLock, flags);

        // Default values
        engine = engineFlag;
        free   = 0xFFFFFFFF;
        thread = CMDQ_INVALID_THREAD;

        for (index = 0; ((index < CMDQ_MAX_ENGINE_COUNT) && (engine != 0)); index++)
        {
            if (engine & (0x1LL << index))
            {
                if (CMDQ_INVALID_THREAD == pEngine[index].currOwner)
                {
                    continue;
                }
                else if (CMDQ_INVALID_THREAD == thread)
                {
                    thread = pEngine[index].currOwner;
                    free   &= ~(0x1 << thread);
                }
                else if (thread != pEngine[index].currOwner)
                {
                    // Partial HW occupied by different threads,
                    // we need to wait.
                    if (forceLog)
                    {
                        CMDQ_LOG("THREAD: try locate on thread %d but engine %d also occupied by thread %d\n", thread, index, pEngine[index].currOwner);
                    }
                    else
                    {
                        CMDQ_VERBOSE("THREAD: try locate on thread %d but engine %d also occupied by thread %d\n", thread, index, pEngine[index].currOwner);
                    }
                    
                    thread = CMDQ_INVALID_THREAD;
                    break;
                }
                engine &= ~(0x1LL << index);
            }
        }

        if ((0xFFFFFFFF == free) &&
            (CMDQ_INVALID_THREAD == thread))
        {
            // use scenario to dispatch default thread
            thread = cmdq_core_disp_thread_index_from_scenario(scenario);

            // otherwise find a free slot
            if (CMDQ_INVALID_THREAD == thread)
            {
                // thread 0 - CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT are preserved for DISPSYS for high-prio threads
                const bool isDisplayThread = CMDQ_THR_PRIO_DISPLAY_TRIGGER < thread_prio;
                const bool isTriggerThread = (CMDQ_SCENARIO_TRIGGER_LOOP == scenario);

                // There are 3 sections of thread id
                // 1. Display config threads which is thread 0~3
                // 2. Trigger threads which is thread 4~5
                // 3. Free threads (e.g. MDP) which is thread 6~15
                int startIndex = CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT + CMDQ_MAX_TRIGGER_LOOP_COUNT;
                int endIndex = CMDQ_MAX_THREAD_COUNT;
                if (isDisplayThread)
                {
                    startIndex = 0;
                    endIndex = CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT;
                }
                else if (isTriggerThread)
                {
                    startIndex = CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT;
                    endIndex = CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT + CMDQ_MAX_TRIGGER_LOOP_COUNT;
                }
                else
                {
                    startIndex = CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT + CMDQ_MAX_TRIGGER_LOOP_COUNT;
                    endIndex = CMDQ_MAX_THREAD_COUNT;
                }
                
                for (index = startIndex; index < endIndex; ++index)
                {
                    spin_lock_irqsave(&gCmdqExecLock, flagsExecLock); 
                    if ((0 == pThread[index].taskCount) && (1 == pThread[index].allowDispatching))
                    {
                        CMDQ_VERBOSE("THREAD: dispatch to thread %d, taskCount:%d, allowDispatching:%d\n", index, pThread[index].taskCount, pThread[index].allowDispatching);

                        thread = index;
                        pThread[index].allowDispatching = 0;                        
                        spin_unlock_irqrestore(&gCmdqExecLock, flagsExecLock);
                        break;
                    }
                    spin_unlock_irqrestore(&gCmdqExecLock, flagsExecLock); 
                }
            }
        }

        // Make sure the found thread has enough space for the task;
        // ThreadStruct->pCurTask has size limitation.
        if (CMDQ_INVALID_THREAD != thread &&
            CMDQ_MAX_TASK_IN_THREAD <= pThread[thread].taskCount)
        {
            if (forceLog)
            {
                CMDQ_LOG("THREAD: thread %d task count = %d full\n", thread, pThread[thread].taskCount);
            }
            else
            {
                CMDQ_VERBOSE("THREAD: thread %d task count = %d full\n", thread, pThread[thread].taskCount);
            }
            
            thread = CMDQ_INVALID_THREAD;
        }

        // no thread available now, wait for it
        if (CMDQ_INVALID_THREAD == thread)
        {
            if (forceLog)
            {
                CMDQ_LOG("THREAD: need to wait\n");
            }
            else
            {
                CMDQ_VERBOSE("THREAD: need to wait\n");
            }
            shouldWait = true;
            break;
        }
    } while(0);

    if (!shouldWait)
    {
        // we can start now
        cmdq_core_enable_clock(engineFlag,
                               thread);
    }

    spin_unlock_irqrestore(&gCmdqThreadLock, flags);

	CMDQ_PROF_END(current->pid, __FUNCTION__);

    return thread;
}


static void cmdq_core_disable_clock(uint64_t engineFlag)
{
    EngineStruct  *pEngine;
    uint64_t       enginesNotUsed;
    int32_t       index;
    int32_t       status;
    CmdqCBkStruct *pCallback;

    CMDQ_VERBOSE("-->CLOCK: Disable hardware clock 0x%llx begin\n", engineFlag);

    pEngine = gCmdqContext.engine;
    pCallback = gCmdqGroupCallback;

    // decrement engine usage count
    // and find engine that become unused
    enginesNotUsed = 0;
    for (index = 0; index < CMDQ_MAX_ENGINE_COUNT; index++)
    {
        if (engineFlag & (1LL << index))
        {
            pEngine[index].userCount--;
            if (pEngine[index].userCount <= 0)
            {
                enginesNotUsed |= (1LL << index);
                pEngine[index].currOwner = CMDQ_INVALID_THREAD;
            }
        }
    }

    // ISP special check: Always call ISP on/off if this task
    // involves ISP. Ignore the ISP HW flags ref count.
    if (cmdq_core_is_group_flag(CMDQ_GROUP_ISP, engineFlag))
    {
        CMDQ_VERBOSE("CLOCK: disable group %d clockOff\n", CMDQ_GROUP_ISP);
        if(NULL == pCallback[CMDQ_GROUP_ISP].clockOff)
        {
            CMDQ_ERR("CLOCK: disable group %d clockOff func NULL\n", CMDQ_GROUP_ISP);
        }
        else
        {
            status = pCallback[CMDQ_GROUP_ISP].clockOff(gCmdqEngineGroupBits[CMDQ_GROUP_ISP] & engineFlag);

            #if 1
            --gCmdqISPClockCounter;
            if (gCmdqISPClockCounter != 0)
            {
                CMDQ_VERBOSE("CLOCK: ISP clockOff cnt=%d\n", gCmdqISPClockCounter);
            }
            #endif

            if (status < 0)
            {
                CMDQ_ERR("CLOCK: disable group %d clockOff failed\n", CMDQ_GROUP_ISP);
            }
        }
    }

    // Turn off unused engines
    for (index = 0; index < CMDQ_MAX_GROUP_COUNT; ++index)
    {
        // note that DISPSYS controls their own clock on/off
        if (CMDQ_GROUP_DISP == index)
        {
            continue;
        }

        // note that ISP is per-task on/off, not per HW flag
        if (CMDQ_GROUP_ISP == index)
        {
            continue;
        }

        if (cmdq_core_is_group_flag((CMDQ_GROUP_ENUM)index, enginesNotUsed))
        {
            CMDQ_MSG("CLOCK: Disable engine group %d flag=0x%llx clockOff\n", index, enginesNotUsed);
            if(NULL == pCallback[index].clockOff)
            {
                CMDQ_ERR("CLOCK: Disable engine group %d clockOff func NULL\n", index);
                continue;
            }
            status = pCallback[index].clockOff(gCmdqEngineGroupBits[index] & enginesNotUsed);
            if (status < 0)
            {
                CMDQ_ERR("CLOCK: Disable engine group %d clock failed\n", index);
            }
        }
    }

    // disable fundamental clocks if needed
    cmdq_core_enable_common_clock_locked(false, engineFlag);

    CMDQ_MSG("<--CLOCK: Disable hardware clock 0x%llx end\n", engineFlag);
}

void cmdq_core_add_consume_task(void)
{
    if (!work_pending(&gCmdqContext.taskConsumeWaitQueueItem))
    {
        CMDQ_PROF_MMP(cmdq_mmp_get_event()->consume_add,
                   MMProfileFlagPulse,
                   0,
                   0);
        queue_work(gCmdqContext.taskConsumeWQ, &gCmdqContext.taskConsumeWaitQueueItem);
    }
}

static void cmdq_core_release_thread(TaskStruct *pTask)
{
    unsigned long flags;
    const int32_t thread = pTask->thread;
    const uint64_t engineFlag = pTask->engineFlag;

    if (thread == CMDQ_INVALID_THREAD)
    {
        return;
    }

    spin_lock_irqsave(&gCmdqThreadLock, flags);

    cmdq_core_disable_clock(engineFlag);
    pTask->thread = CMDQ_INVALID_THREAD;

    spin_unlock_irqrestore(&gCmdqThreadLock, flags);
}

static void cmdq_core_reset_hw_engine(int32_t engineFlag)
{
    EngineStruct  *pEngine;
    uint32_t      engines;
    int32_t       index;
    int32_t       status;
    CmdqCBkStruct *pCallback;

    CMDQ_MSG("Reset hardware engine begin\n");

    pEngine = gCmdqContext.engine;

    engines = 0;
    for (index = 0; index < CMDQ_MAX_ENGINE_COUNT; index++)
    {
        if (engineFlag & (1LL << index))
        {
            engines |= (1LL << index);
        }
    }

    pCallback = gCmdqGroupCallback;

    for (index = 0; index < CMDQ_MAX_GROUP_COUNT; ++index)
    {
        if (cmdq_core_is_group_flag((CMDQ_GROUP_ENUM)index, engines))
        {
            CMDQ_MSG("Reset engine group %d clock\n", index);
            if(NULL == pCallback[index].resetEng)
            {
                CMDQ_ERR("Reset engine group %d clock func NULL\n", index);
                continue;
            }
            status = pCallback[index].resetEng(gCmdqEngineGroupBits[index] & engineFlag);
            if (status < 0)
            {
                CMDQ_ERR("Reset engine group %d clock failed\n", index);
            }
        }
    }

    CMDQ_MSG("Reset hardware engine end\n");
}

int32_t cmdq_core_suspend_HW_thread(int32_t thread)
{
    int32_t loop = 0;
    uint32_t enabled = 0;

    if (CMDQ_INVALID_THREAD == thread)
    {
        CMDQ_ERR("suspend invalid thread\n"); 
        return -EFAULT;
    }

    // write suspend bit
    CMDQ_REG_SET32(CMDQ_THR_SUSPEND_TASK(thread), 0x01);

    // check if the thread is already disabled.
    // if already disabled, treat as suspend successful but print error log
    enabled = CMDQ_REG_GET32(CMDQ_THR_ENABLE_TASK(thread));
    if (0 == (0x01 & enabled))
    {
        CMDQ_LOG("[WARNING] thread %d suspend not effective, enable=%d\n", thread, enabled);
        return 0;
    }
    
    loop = 0;
    while(0x0 == (CMDQ_REG_GET32(CMDQ_THR_CURR_STATUS(thread)) & 0x2))
    {
        if(loop > CMDQ_MAX_LOOP_COUNT)
        {
            CMDQ_AEE("CMDQ", "Suspend HW thread %d failed\n", thread);
            return -EFAULT;
        }
        loop++;
    }

    #ifdef CONFIG_MTK_FPGA
    CMDQ_MSG("EXEC: Suspend HW thread(%d)\n", thread);
    #endif

    return 0;
}

DEBUG_STATIC inline void cmdq_core_resume_HW_thread(int32_t thread)
{
    #ifdef CONFIG_MTK_FPGA
    CMDQ_MSG("EXEC: Resume HW thread(%d)\n", thread);
    #endif
    smp_mb();
    CMDQ_REG_SET32(CMDQ_THR_SUSPEND_TASK(thread), 0x00);
}

DEBUG_STATIC inline int32_t cmdq_core_reset_HW_thread(int32_t thread)
{
    int32_t loop = 0;

    CMDQ_MSG("Reset HW thread(%d)\n", thread);

    CMDQ_REG_SET32(CMDQ_THR_WARM_RESET(thread), 0x01);
    while(0x1 == (CMDQ_REG_GET32(CMDQ_THR_WARM_RESET(thread))))
    {
        if(loop > CMDQ_MAX_LOOP_COUNT)
        {
            CMDQ_AEE("CMDQ", "Reset HW thread %d failed\n", thread);
            return -EFAULT;
        }
        loop++;
    }

    CMDQ_REG_SET32(CMDQ_THR_SLOT_CYCLES, 0x3200);
    return 0;
}

DEBUG_STATIC inline int32_t cmdq_core_disable_HW_thread(int32_t thread)
{
    cmdq_core_reset_HW_thread(thread);

    // Disable thread
    CMDQ_MSG("Disable HW thread(%d)\n", thread);
    CMDQ_REG_SET32(CMDQ_THR_ENABLE_TASK(thread), 0x00);
    return 0;
}

DEBUG_STATIC uint32_t* cmdq_core_get_pc(const TaskStruct *pTask, uint32_t thread, uint32_t insts[4])
{
    long      currPC = 0L;
    uint8_t   *pInst = NULL;

    insts[0] = 0;
    insts[1] = 0;
    insts[2] = 0;
    insts[3] = 0;

    currPC = CMDQ_AREG_TO_PHYS(CMDQ_REG_GET32(CMDQ_THR_CURR_ADDR(thread)));
    pInst  = (uint8_t*)pTask->pVABase + (currPC - pTask->MVABase);

    // CMDQ_ERR("[PC]thr:%d, paPC:0x%08x, pVABase:0x%08x, PABase:0x%08x\n", thread, currPC, pTask->pVABase, pTask->MVABase);

    if(((uint8_t*)pTask->pVABase <= pInst) && (pInst <= (uint8_t*)pTask->pCMDEnd))
    {
        if (pInst != (uint8_t*)pTask->pCMDEnd)
        {
            // If PC points to start of pCMD,
            // - 8 causes access violation
            // insts[0] = CMDQ_REG_GET32(pInst - 8);
            // insts[1] = CMDQ_REG_GET32(pInst - 4);
            insts[2] = CMDQ_REG_GET32(pInst + 0);
            insts[3] = CMDQ_REG_GET32(pInst + 4);
        }
        else
        {
            //insts[0] = CMDQ_REG_GET32(pInst - 16);
            //insts[1] = CMDQ_REG_GET32(pInst - 12);
            insts[2] = CMDQ_REG_GET32(pInst - 8);
            insts[3] = CMDQ_REG_GET32(pInst - 4);
        }
    }
    else
    {
        // invalid PC address
        return NULL;
    }

    return (uint32_t*)pInst;
}

uint32_t cmdq_core_subsys_to_reg_addr(uint32_t argA)
{
    const int32_t subsys_id = (argA & 0x001F0000) >> 16;
    const uint32_t offset = (argA & 0xFFFF);
    uint32_t base_addr = 0;

#undef DECLARE_CMDQ_SUBSYS
#define DECLARE_CMDQ_SUBSYS(addr, id, grp, base) case id: base_addr = addr; break;
    switch(subsys_id)
    {
    #include "cmdq_subsys.h"
    }
#undef DECLARE_CMDQ_SUBSYS

    return (base_addr << 16) | offset;
}

static const char* cmdq_core_parse_op(uint32_t opCode)
{
    switch(opCode)
    {
    case CMDQ_CODE_POLL:
        return "POLL";
    case CMDQ_CODE_WRITE:
        return "WRIT";
    case CMDQ_CODE_WFE:
        return "SYNC";
    case CMDQ_CODE_READ:
        return "READ";
    case CMDQ_CODE_MOVE:
        return "MASK";
    case CMDQ_CODE_JUMP:
        return "JUMP";
    case CMDQ_CODE_EOC:
        return "MARK";
    }
    return NULL;
}

static uint32_t* cmdq_core_dump_pc(const TaskStruct *pTask, int thread, const char *tag)
{
    uint32_t *pcVA = NULL;
    uint32_t  insts[4] = {0};
    char      parsedInstruction[128] = {0};

    pcVA = cmdq_core_get_pc(pTask, thread, insts);
    if (pcVA)
    {
        const uint32_t op = (insts[3] & 0xFF000000) >> 24;

        cmdq_core_parse_instruction(pcVA, parsedInstruction, sizeof(parsedInstruction));
        CMDQ_LOG("[%s]Thread %d PC(VA): 0x%p, 0x%08x:0x%08x => %s",
                   tag,
                   thread,
                   pcVA,
                   insts[2],
                   insts[3],
                   parsedInstruction);

        // for WFE, we specifically dump the event value
        if (op == CMDQ_CODE_WFE)
        {
            uint32_t regValue = 0;
            const uint32_t eventID = 0x3FF & insts[3];

            CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_ID, eventID);
            regValue = CMDQ_REG_GET32(CMDQ_SYNC_TOKEN_VAL);
            CMDQ_LOG("[%s]CMDQ_SYNC_TOKEN_VAL of %s is %d\n", tag, cmdq_core_get_event_name(eventID), regValue);
        }
    }
    else
    {
        CMDQ_LOG("[%s]Thread %d PC(VA): Not available\n", tag, thread);
    }

    return pcVA;
}

static void cmdq_core_dump_status(const char *tag)
{
    int32_t coreExecThread = CMDQ_INVALID_THREAD;
    uint32_t value[6] = {0};

    value[0] = CMDQ_REG_GET32(CMDQ_CURR_LOADED_THR);
    value[1] = CMDQ_REG_GET32(CMDQ_THR_EXEC_CYCLES);
    value[2] = CMDQ_REG_GET32(CMDQ_THR_TIMEOUT_TIMER);
    value[3] = CMDQ_REG_GET32(CMDQ_BUS_CONTROL_TYPE);

    coreExecThread = __builtin_ffs(value[0]) - 1; // this returns (1 + index of least bit set) or 0 if input is 0.
    CMDQ_LOG("[%s]IRQ flag:0x%08x, Execing:%d, Exec Thread:%d, CMDQ_CURR_LOADED_THR: 0x%08x\n",
             tag,
             CMDQ_REG_GET32(CMDQ_CURR_IRQ_STATUS),
             (0x80000000 & value[0]) ? 1 : 0,
             coreExecThread,
             value[0]);
    CMDQ_LOG("[%s]CMDQ_THR_EXEC_CYCLES:0x%08x, CMDQ_THR_TIMER:0x%08x, CMDQ_BUS_CTRL:0x%08x\n", tag, value[1], value[2], value[3]);
    CMDQ_LOG("[%s]CMDQ_DEBUG_1: 0x%08x\n", tag, CMDQ_REG_GET32((GCE_BASE_VA + 0xF0)));
    CMDQ_LOG("[%s]CMDQ_DEBUG_2: 0x%08x\n", tag, CMDQ_REG_GET32((GCE_BASE_VA + 0xF4)));
    CMDQ_LOG("[%s]CMDQ_DEBUG_3: 0x%08x\n", tag, CMDQ_REG_GET32((GCE_BASE_VA + 0xF8)));
    CMDQ_LOG("[%s]CMDQ_DEBUG_4: 0x%08x\n", tag, CMDQ_REG_GET32((GCE_BASE_VA + 0xFC)));
    return;
}

static void cmdq_core_dump_task(const TaskStruct *pTask)
{
	CMDQ_ERR
	    ("Task: 0x%p, Scenario: %d, State: %d, Priority: %d, Flag: 0x%016llx, VABase: 0x%08x\n",
	     pTask, pTask->scenario, pTask->taskState, pTask->priority, pTask->engineFlag,
	     (uint32_t) pTask->pVABase);

	/* dump last Inst only when VALID command buffer */
	/* otherwise data abort is happened */
	if (pTask->pVABase) {
		CMDQ_ERR("CMDEnd: 0x%p, MVABase: %pa, Size: %d, Last Inst: 0x%08x:0x%08x, 0x%08x:0x%08x\n",
			pTask->pCMDEnd, &pTask->MVABase, pTask->commandSize, pTask->pCMDEnd[-3],
			pTask->pCMDEnd[-2], pTask->pCMDEnd[-1], pTask->pCMDEnd[0]);
	} else {
	    CMDQ_ERR("CMDEnd: 0x%p, MVABase: %pa, Size: %d\n", 
			pTask->pCMDEnd, &pTask->MVABase, pTask->commandSize);
	}

	CMDQ_ERR("Trigger: %lld, Got IRQ: %lld, Wait: %lld, Finish: %lld\n",
		 pTask->trigger,
		 pTask->gotIRQ,
		 pTask->beginWait,
		 pTask->wakedUp);
	CMDQ_ERR("Caller pid:%d name:%s\n", pTask->callerPid, pTask->callerName);
	return;
}

static void cmdq_core_dump_task_with_engine_flag(uint64_t engineFlag)
{
    struct TaskStruct *pDumpTask = NULL;
    struct list_head *p = NULL;

    CMDQ_ERR("=============== [CMDQ] All active tasks sharing same engine flag 0x%08llx===============\n", engineFlag);
    
    list_for_each(p, &gCmdqContext.taskActiveList)
    {
        pDumpTask = list_entry(p, struct TaskStruct, listEntry);
        if (NULL != pDumpTask &&
            (engineFlag & pDumpTask->engineFlag))
        {
            CMDQ_ERR("Thr %d, Task: 0x%p, VABase: 0x%p, MVABase 0x%pa, Size: %d, Flag: 0x%08llx, Last Inst 0x%08x:0x%08x, 0x%08x:0x%08x\n",
                (pDumpTask->thread),
                (pDumpTask),
                (pDumpTask->pVABase),
                &(pDumpTask->MVABase),
                pDumpTask->commandSize,
                pDumpTask->engineFlag,
                pDumpTask->pCMDEnd[-3],
                pDumpTask->pCMDEnd[-2],
                pDumpTask->pCMDEnd[-1],
                pDumpTask->pCMDEnd[0]);
        }
    }

}

int32_t cmdq_core_parse_instruction(const uint32_t *pCmd, char *textBuf, int bufLen)
{
    int reqLen = 0;
    const uint32_t op = (pCmd[1] & 0xFF000000) >> 24;
    const uint32_t argA = pCmd[1] & (~0xFF000000);
    const uint32_t argB = pCmd[0];

    switch(op)
    {
    case CMDQ_CODE_MOVE:
        if (1 & (argA >> 23))
        {
            reqLen = snprintf(textBuf, bufLen, "MOVE: 0x%08x to R%d\n", argB, (argA >> 16) & 0x1f);
        }
        else
        {
            reqLen = snprintf(textBuf, bufLen, "MASK: 0x%08x\n", argB);
        }
        break;
    case CMDQ_CODE_READ:
    case CMDQ_CODE_WRITE:
    case CMDQ_CODE_POLL:
        reqLen = snprintf(textBuf, bufLen, "%s: ", cmdq_core_parse_op(op));
        bufLen -= reqLen;
        textBuf += reqLen;

        // data (value)
        if (argA & (1 << 22))
        {
            reqLen = snprintf(textBuf, bufLen, "R%d, ", argB);
            bufLen -= reqLen;
            textBuf += reqLen;
        }
        else
        {
            reqLen = snprintf(textBuf, bufLen, "0x%08x, ", argB);
            bufLen -= reqLen;
            textBuf += reqLen;
        }

        // address
        if (argA & (1 << 23))
        {
            reqLen = snprintf(textBuf, bufLen, "R%d\n", (argA >> 16) & 0x1F);
            bufLen -= reqLen;
            textBuf += reqLen;
        }
        else
        {
            const uint32_t addr = cmdq_core_subsys_to_reg_addr(argA);
            reqLen = snprintf(textBuf, bufLen, "addr=0x%08x [%s],use_mask=%d\n",
                               (addr & 0xFFFFFFFE),
                               cmdq_core_parse_module_from_reg_addr(addr),
                               (addr & 0x1)
                               );
            bufLen -= reqLen;
            textBuf += reqLen;
        }
        break;
    case CMDQ_CODE_JUMP:
        if (argA)
        {
            if (argA & (1 << 22))
            {
                // jump by register
                reqLen = snprintf(textBuf, bufLen, "JUMP(REG): R%d\n", argB);
            }
            else
            {
                // absolute
                reqLen = snprintf(textBuf, bufLen, "JUMP(ABS): 0x%08x\n", argB);
            }
        }
        else
        {
            // relative
            if ((int32_t)argB >= 0)
            {
                reqLen = snprintf(textBuf, bufLen, "JUMP(REL): +%d\n", (int32_t)argB);
            }
            else
            {
                reqLen = snprintf(textBuf, bufLen, "JUMP(REL): %d\n", (int32_t)argB);
            }
        }
        break;
    case CMDQ_CODE_WFE:
        if (0x80008001 == argB)
        {
            reqLen = snprintf(textBuf, bufLen, "WFE: %s\n", cmdq_core_get_event_name(argA));
        }
        else
        {
            reqLen = snprintf(textBuf, bufLen, "SYNC: %s, upd=%d, op=%d, val=%d, wait=%d, wop=%d, val=%d\n",
                                cmdq_core_get_event_name(argA),
                                (argB >> 31) & 0x1,
                                (argB >> 28) & 0x7,
                                (argB >> 16) & 0xFFF,
                                (argB >> 15) & 0x1,
                                (argB >> 12) & 0x7,
                                (argB >> 0 ) & 0xFFF
                                );
        }
        break;
    case CMDQ_CODE_EOC:
        if (argA == 0 && argB == 0x00000001)
        {
            reqLen = snprintf(textBuf, bufLen, "EOC\n");
        }
        else
        {

            if(cmdq_core_support_sync_non_suspendable())
            {
                reqLen = snprintf(textBuf, bufLen, "MARKER: sync_no_suspnd=%d", (argA & (1 << 20)) > 0);
            }
            else
            {
                reqLen = snprintf(textBuf, bufLen, "MARKER:");
            }

            reqLen = snprintf(textBuf, bufLen, "no_suspnd=%d, no_inc=%d, m=%d, m_en=%d, prefetch=%d, irq=%d\n",
                             (argA & (1 << 21)) > 0,
                             (argA & (1 << 16)) > 0,
                             (argB & (1 << 20)) > 0,
                             (argB & (1 << 17)) > 0,
                             (argB & (1 << 16)) > 0,
                             (argB & (1 <<  0)) > 0
                             );
        }
        break;
    default:
        reqLen = snprintf(textBuf, bufLen, "UNDEFINED\n");
        break;
    }

    return reqLen;
}

int32_t cmdqCoreDebugDumpCommand(TaskStruct *pTask)
{
    const uint32_t *pCmd = NULL;
    static char textBuf[128] = {0};
    int i = 0;
    if (NULL == pTask)
    {
        return -EFAULT;
    }

    mutex_lock(&gCmdqTaskMutex);

    CMDQ_LOG("======TASK 0x%p command buffer:\n", pTask);
    print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 16, 4,
                   pTask->pVABase, pTask->commandSize, false);
    CMDQ_LOG("======TASK 0x%p command buffer END\n", pTask);
    CMDQ_LOG("TASK 0x%p command buffer TRANSLATED:\n", pTask);
    for (i = 0, pCmd = pTask->pVABase; i < pTask->commandSize; i += 8, pCmd += 2)
    {
        cmdq_core_parse_instruction(pCmd, textBuf, 128);
        CMDQ_LOG("%s", textBuf);
    }
    CMDQ_LOG("======TASK 0x%p command END\n", pTask);

    mutex_unlock(&gCmdqTaskMutex);

    return 0;
}

static void cmdq_core_fill_task_record(RecordStruct *pRecord, const TaskStruct *pTask, uint32_t thread)
{
    uint32_t begin, end;

    if (pRecord && pTask)
    {
        // Record scenario
        pRecord->user      = pTask->callerPid;
        pRecord->scenario  = pTask->scenario;
        pRecord->priority  = pTask->priority;
        pRecord->thread    = thread;
        pRecord->engineFlag = pTask->engineFlag;

        if (NULL == pTask->profileData)
        {
            pRecord->writeTimeNS = 0;

        }
        else
        {
            // Command exec time, each count is 76ns
            begin = *((volatile uint32_t*)pTask->profileData);
            end = *((volatile uint32_t*)(pTask->profileData+1));
            pRecord->writeTimeNS = (end - begin) * 76;
        }

        // Record time
        pRecord->submit    = pTask->submit;
        pRecord->trigger   = pTask->trigger;
        pRecord->gotIRQ    = pTask->gotIRQ;
        pRecord->beginWait = pTask->beginWait;
        pRecord->wakedUp   = pTask->wakedUp;
    }
}

static void cmdq_core_track_task_record(TaskStruct *pTask, uint32_t thread)
{
    RecordStruct   *pRecord;
    unsigned long  flags;
    CMDQ_TIME done;


    #if 0
    if (cmdq_core_should_profile(pTask->scenario))
    {
        return;
    }
    #endif

    done = sched_clock();

    spin_lock_irqsave(&gCmdqRecordLock, flags);

    pRecord = &(gCmdqContext.record[gCmdqContext.lastID]);

    cmdq_core_fill_task_record(pRecord, pTask, thread);

    pRecord->done = done;

    gCmdqContext.lastID++;
    if (gCmdqContext.lastID >= CMDQ_MAX_RECORD_COUNT)
    {
        gCmdqContext.lastID = 0;
    }

    gCmdqContext.recNum++;
    if (gCmdqContext.recNum >= CMDQ_MAX_RECORD_COUNT)
    {
        gCmdqContext.recNum = CMDQ_MAX_RECORD_COUNT;
    }

    spin_unlock_irqrestore(&gCmdqRecordLock, flags);
}


static void cmdq_core_attach_error_task(const TaskStruct *pTask, int32_t thread)
{
    EngineStruct  *pEngine = NULL;
    ThreadStruct  *pThread = NULL;
    CmdqCBkStruct *pCallback = NULL;
    uint64_t      engFlag = 0;
    int32_t       index = 0;
    uint32_t      *hwPC = NULL;
    uint32_t      value[10] = {0};
    int           isSMIHang = 0;

    static const char *engineGroupName[] = { CMDQ_FOREACH_GROUP(GENERATE_STRING) };

    pThread = &(gCmdqContext.thread[thread]);
    pEngine = gCmdqContext.engine;

    CMDQ_PROF_MMP(cmdq_mmp_get_event()->warning,
                  MMProfileFlagPulse,
                  (uint32_t)pTask,
                  thread);

    //
    //  Update engine fail count
    //
    if (pTask)
    {
        engFlag = pTask->engineFlag;
        for (index = 0; index < CMDQ_MAX_ENGINE_COUNT; index++)
        {
            if (engFlag & (1LL << index))
            {
                pEngine[index].failCount++;
            }
        }
    }

    //
    //  register error record
    //
    if (gCmdqContext.errNum < CMDQ_MAX_ERROR_COUNT)
    {
        ErrorStruct *pError = &gCmdqContext.error[gCmdqContext.errNum];
        cmdq_core_fill_task_record(&pError->errorRec, pTask, thread);
        pError->ts_nsec = local_clock();
    }

    //
    //  Then we just print out info
    //
    CMDQ_ERR("================= [CMDQ] Begin of Error %d================\n", gCmdqContext.errNum);

    #ifndef CONFIG_MTK_FPGA
    CMDQ_ERR("=============== [CMDQ] SMI Status ===============\n");
    isSMIHang = smi_debug_bus_hanging_detect(SMI_DBG_DISPSYS | SMI_DBG_VDEC | SMI_DBG_IMGSYS |SMI_DBG_VENC | SMI_DBG_MJC, 1);
    CMDQ_ERR("SMI Hang? = %d\n", isSMIHang);
    #endif

    CMDQ_ERR("=============== [CMDQ] Error Thread Status ===============\n");
    value[0] = CMDQ_REG_GET32(CMDQ_THR_CURR_ADDR(thread));
    value[1] = CMDQ_REG_GET32(CMDQ_THR_END_ADDR(thread));
    value[2] = CMDQ_REG_GET32(CMDQ_THR_WAIT_TOKEN(thread));
    value[3] = CMDQ_GET_COOKIE_CNT(thread);
    value[4] = CMDQ_REG_GET32(CMDQ_THR_IRQ_STATUS(thread));
    value[5] = CMDQ_REG_GET32(CMDQ_THR_INST_CYCLES(thread));
    value[6] = CMDQ_REG_GET32(CMDQ_THR_CURR_STATUS(thread));
    value[7] = CMDQ_REG_GET32(CMDQ_THR_IRQ_ENABLE(thread));
    value[8] = CMDQ_REG_GET32(CMDQ_THR_ENABLE_TASK(thread));

    CMDQ_ERR("Index: %d, Enabled: %d, IRQ: 0x%08x, Thread PC: 0x%08x, End: 0x%08x, Wait Token: 0x%08x\n",
        thread, value[8], value[4], value[0], value[1], value[2]);
    CMDQ_ERR("Curr Cookie: %d, Wait Cookie: %d, Next Cookie: %d, Task Count %d, \n",
        value[3], pThread->waitCookie, pThread->nextCookie, pThread->taskCount);
    CMDQ_ERR("Timeout Cycle:%d, Status:0x%08x, IRQ_EN: 0x%08x\n",
        value[5], value[6], value[7]);

    CMDQ_ERR("=============== [CMDQ] CMDQ Status ===============\n");
    cmdq_core_dump_status("ERR");

    if (NULL != pTask)
    {
        CMDQ_ERR("=============== [CMDQ] Error Thread PC ===============\n");
        hwPC = cmdq_core_dump_pc(pTask, thread, "ERR");

        CMDQ_ERR("=============== [CMDQ] Error Task Status ===============\n");
        cmdq_core_dump_task(pTask);
    }

    CMDQ_ERR("=============== [CMDQ] Clock Gating Status ===============\n");
    cmdq_core_dump_clock_gating();

    //
    //  Dump MMSYS configuration
    //
    CMDQ_ERR("=============== [CMDQ] MMSYS_CONFIG ===============\n");
    cmdq_core_dump_mmsys_config();

    //
    //  ask each module to print their status
    //
    CMDQ_ERR("=============== [CMDQ] Engine Status ===============\n");
    pCallback = gCmdqGroupCallback;
    for (index = 0; index < CMDQ_MAX_GROUP_COUNT; ++index)
    {
        if (!cmdq_core_is_group_flag((CMDQ_GROUP_ENUM)index, pTask->engineFlag))
        {
            continue;
        }
        CMDQ_ERR("====== engine group %s status =======\n", engineGroupName[index]);

        if (NULL == pCallback[index].dumpInfo){
            CMDQ_ERR("(no dump function)\n");
            continue;
        }

        pCallback[index].dumpInfo((gCmdqEngineGroupBits[index] & pTask->engineFlag),
                                   gCmdqContext.logLevel);
    }

    CMDQ_ERR("=============== [CMDQ] GIC dump ===============\n");
    mt_irq_dump_status(cmdq_dev_get_irq_id());

    CMDQ_ERR("=============== [CMDQ] All Task in Error Thread Info ===============\n");
    do
    {
        struct TaskStruct *pDumpTask = NULL;
        for (index = 0; index < CMDQ_MAX_TASK_IN_THREAD; index++)
        {
            pDumpTask = pThread->pCurTask[index];
            if (NULL != pDumpTask)
            {
                CMDQ_ERR("Slot %d, Task: 0x%p, VABase: 0x%p, MVABase 0x%pa, Size: %d, Last Inst 0x%08x:0x%08x, 0x%08x:0x%08x\n",
                    index, (pDumpTask), (pDumpTask->pVABase), &(pDumpTask->MVABase), pDumpTask->commandSize,
                    pDumpTask->pCMDEnd[-3], pDumpTask->pCMDEnd[-2], pDumpTask->pCMDEnd[-1], pDumpTask->pCMDEnd[0]);
            }
        }
    }while(0);

    cmdq_core_dump_task_with_engine_flag(pTask->engineFlag);

    CMDQ_ERR("=============== [CMDQ] Error Command Buffer ===============\n");
    if (hwPC && pTask && hwPC >= pTask->pVABase)
    {
        // because hwPC points to "start" of the instruction
        // add offset 1
        print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 16, 4,
                       pTask->pVABase, (2 + hwPC - pTask->pVABase) * sizeof(uint32_t), true);
    }
    else
    {
        CMDQ_ERR("hwPC is not in region, dump all\n");
        print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 16, 4,
                       pTask->pVABase, (pTask->commandSize), true);
    }

    CMDQ_ERR("================= [CMDQ] End of Error %d ================\n", gCmdqContext.errNum);
    gCmdqContext.errNum++;
}

DEBUG_STATIC void cmdq_core_parse_error(struct TaskStruct *pTask, uint32_t thread,
                                        const char **moduleName, int32_t *flag, uint32_t *instA, uint32_t *instB)
{
    uint32_t     op, argA, argB;
    uint32_t     insts[4] = {0};
    uint32_t     addr = 0;
    const char   *module = NULL;
    int32_t      irqFlag = pTask->irqFlag;
    int          isSMIHang = 0;

    do
    {
	#ifndef CONFIG_MTK_FPGA
        isSMIHang = smi_debug_bus_hanging_detect(SMI_DBG_DISPSYS | SMI_DBG_VDEC | SMI_DBG_IMGSYS |SMI_DBG_VENC | SMI_DBG_MJC, 0);
        if (isSMIHang)
        {
            module = "SMI";
            break;
        }
        #endif

        // other cases, use instruction to judge
        // because scenario / HW flag are not sufficient
        // e.g. ISP pass 2 involves both MDP and ISP
        // so we need to check which instruction timeout-ed.
        if(cmdq_core_get_pc(pTask, thread, insts))
        {
            op = (insts[3] & 0xFF000000) >> 24;
            argA = insts[3] & (~0xFF000000);
            argB = insts[2];

            // for JPEG scenario, use HW flag is sufficient
            if (pTask->engineFlag & (1LL << CMDQ_ENG_JPEG_ENC))
            {
                module = "JPGENC";
                break;
            }
            else if (pTask->engineFlag & (1LL << CMDQ_ENG_JPEG_DEC))
            {
                module = "JPGDEC";
                break;
            }

            switch(op)
            {
            case CMDQ_CODE_POLL:
            case CMDQ_CODE_WRITE:
                addr = cmdq_core_subsys_to_reg_addr(argA);
                module = cmdq_core_parse_module_from_reg_addr(addr);
                break;
            case CMDQ_CODE_WFE:
                // argA is the event ID
                module = cmdq_core_module_from_event_id((CMDQ_EVENT_ENUM)argA, argA, argB);
                break;
            case CMDQ_CODE_READ:
            case CMDQ_CODE_MOVE:
            case CMDQ_CODE_JUMP:
            case CMDQ_CODE_EOC:
            default:
                module = "CMDQ";
                break;
            }
            break;
        }

        module = "CMDQ";
        break;
    }while(0);


    // fill output parameter
    *moduleName = module;
    *flag = irqFlag;
    *instA = insts[3];
    *instB = insts[2];

}

DEBUG_STATIC int32_t cmdq_core_remove_task_from_thread_array_by_cookie(ThreadStruct *pThread,
                                                                       int32_t index,
                                                                       TASK_STATE_ENUM newTaskState)
{
    TaskStruct *pTask = NULL;

    if((NULL == pThread) ||
       (CMDQ_MAX_TASK_IN_THREAD < index) ||
       (0 > index))
    {
        CMDQ_ERR("remove task from thread array, invalid param. THR[0x%p], task_slot[%d], newTaskState[%d]\n",
                  pThread,
                  index,
                  newTaskState);
        return -EINVAL;
    }

    pTask = pThread->pCurTask[index];
    if (NULL == pTask)
    {
        CMDQ_ERR("remove fail, task_slot[%d] on thread[%p] is NULL\n",
                  index, pThread);
        return -EINVAL;
    }

    // note timing to switch a task to done_status(_ERROR, _KILLED, _DONE) is aligned with thread's taskcount change
    // check task status to prevent double clean-up thread's taskcount
    if (TASK_STATE_BUSY != pTask->taskState)
    {
        CMDQ_ERR("remove task, taskStatus err[%d]. THR[0x%p], task_slot[%d], targetTaskStaus[%d]\n",
                 pTask->taskState,
                 pThread,
                 index,
                 newTaskState);
        return -EINVAL;
    }

    CMDQ_VERBOSE("remove task, slot[%d], targetStatus: %d\n", index, newTaskState);
    pTask->taskState = newTaskState;
    pTask = NULL;
    pThread->pCurTask[index] = NULL;
    pThread->taskCount--;

    if (0 > pThread->taskCount)
    {
        CMDQ_ERR("taskCount < 0 after cmdq_core_remove_task_from_thread_array_by_cookie\n");
    }

    return 0;
}

DEBUG_STATIC int32_t cmdq_core_force_remove_task_from_thread(TaskStruct *pTask, uint32_t thread)
{
    int32_t status = 0;
    int32_t cookie = 0;
    int index = 0;
    int loop = 0;
    struct TaskStruct *pExecTask = NULL;
    struct ThreadStruct *pThread = &(gCmdqContext.thread[thread]);

    status = cmdq_core_suspend_HW_thread(thread);
    
    CMDQ_REG_SET32(CMDQ_THR_INST_CYCLES(thread), cmdq_core_get_task_timeout_cycle(pThread));

    // The cookie of the task currently being processed
    cookie = CMDQ_GET_COOKIE_CNT(thread) + 1;

    pExecTask = pThread->pCurTask[cookie % CMDQ_MAX_TASK_IN_THREAD];
    if (NULL != pExecTask && (pExecTask == pTask))
    {
        // The task is executed now, set the PC to EOC for bypass
        CMDQ_REG_SET32(CMDQ_THR_CURR_ADDR(thread), CMDQ_PHYS_TO_AREG(pTask->MVABase + pTask->commandSize - 16));

        cmdq_core_reset_hw_engine(pTask->engineFlag);

        pThread->pCurTask[cookie % CMDQ_MAX_TASK_IN_THREAD] = NULL;
        pTask->taskState = TASK_STATE_KILLED;
    }
    else
    {
        loop = pThread->taskCount;
        for (index = (cookie % CMDQ_MAX_TASK_IN_THREAD); loop > 0; loop--, index++)
        {
            if (index >= CMDQ_MAX_TASK_IN_THREAD)
            {
                index = 0;
            }

            pExecTask = pThread->pCurTask[index];
            if (NULL == pExecTask)
            {
                continue;
            }
            
            if ((0x10000000 == pExecTask->pCMDEnd[ 0]) &&
                (0x00000008 == pExecTask->pCMDEnd[-1]))
            {
                // We reached the last task
                break;
            }
            else if (pExecTask->pCMDEnd[-1] == pTask->MVABase)
            {
                // Fake EOC command
                pExecTask->pCMDEnd[-1]  = 0x00000001;
                pExecTask->pCMDEnd[ 0]  = 0x40000000;

                // Bypass the task
                pExecTask->pCMDEnd[ 1]  = pTask->pCMDEnd[-1];
                pExecTask->pCMDEnd[ 2]  = pTask->pCMDEnd[ 0];

                index += 1;
                if (index >= CMDQ_MAX_TASK_IN_THREAD)
                {
                    index = 0;
                }

                pThread->pCurTask[index] = NULL;
                pTask->taskState = TASK_STATE_KILLED;
                status = 0;
                break;
            }
        }
    }

    return status;
}

static TaskStruct* cmdq_core_search_task_by_pc(uint32_t threadPC, const ThreadStruct *pThread)
{
    TaskStruct *pTask = NULL;
    int i = 0;
    for (i = 0; i < CMDQ_MAX_TASK_IN_THREAD; ++i)
    {
        pTask = pThread->pCurTask[i];
        if (pTask &&
            threadPC >= pTask->MVABase &&
            threadPC <= (pTask->MVABase+pTask->commandSize))
        {
            break;
        }
    }

    return pTask;
}

DEBUG_STATIC int32_t cmdq_core_wait_task_done(TaskStruct *pTask, long timeout_jiffies)
{
    int32_t      waitQ;
    int32_t      status;
    int32_t      index;
    unsigned long flags;
    uint32_t     thread;
    ThreadStruct *pThread = NULL;
    bool         markAsErrorTask = false;
    // error report
    bool         throwAEE = false;
    const char   *module = NULL;
    uint32_t     instA = 0, instB = 0;
    int32_t      retryCount = 0;
    int32_t      irqFlag = 0;

    status = 0;  // Default status

    thread = pTask->thread;
    if (CMDQ_INVALID_THREAD == thread)
    {
        CMDQ_PROF_MMP(cmdq_mmp_get_event()->wait_thread,
                  MMProfileFlagPulse,
                  (uint32_t)pTask,
                  -1);

        CMDQ_PROF_START(current->pid, "wait_for_thread");

        CMDQ_LOG("pid:%d task:0x%p wait for valid thread first\n", current->pid, pTask);

        // wait for acquire thread (this is done by cmdq_core_consume_waiting_list);
        waitQ = wait_event_timeout(gCmdqThreadDispatchQueue,
                                   (CMDQ_INVALID_THREAD != pTask->thread),
                                   msecs_to_jiffies(CMDQ_ACQUIRE_THREAD_TIMEOUT_MS));

        CMDQ_PROF_END(current->pid, "wait_for_thread");
        if (0 == waitQ || CMDQ_INVALID_THREAD == pTask->thread)
        {
            mutex_lock(&gCmdqTaskMutex);
            // it's possible that the task was just consumed now.
            // so check again.
            if (CMDQ_INVALID_THREAD == pTask->thread)
            {
                // task may already released, or starved to death
                CMDQ_ERR("task 0x%p timeout with invalid thread\n", pTask);
                cmdq_core_dump_task(pTask);
                cmdq_core_dump_task_with_engine_flag(pTask->engineFlag);
                // remove from waiting list, 
                // so that it won't be consumed in the future
                list_del_init(&(pTask->listEntry));
                
                mutex_unlock(&gCmdqTaskMutex);
                return -EINVAL;
            }

            // valid thread, so we keep going
            mutex_unlock(&gCmdqTaskMutex);
        }
    }

    thread = pTask->thread;
    pThread = &(gCmdqContext.thread[thread]);

    CMDQ_PROF_MMP(cmdq_mmp_get_event()->wait_task,
                  MMProfileFlagPulse,
                  (uint32_t)pTask,
                  thread);

    CMDQ_PROF_START(current->pid, "wait_for_task_done");

    pTask->beginWait = sched_clock();

    CMDQ_MSG("-->WAIT: task 0x%p on thread %d timeout: %d(ms) begin\n", pTask, thread, jiffies_to_msecs(timeout_jiffies));

    // timeout wait & make sure this task is finished.
    // pTask->taskState flag is updated in IRQ handlers like cmdqCoreHandleDone.
    retryCount = 0;
    waitQ = wait_event_timeout(gCmdWaitQueue[thread],
                               (TASK_STATE_BUSY != pTask->taskState && TASK_STATE_WAITING != pTask->taskState),
                                // timeout_jiffies);
                                msecs_to_jiffies(CMDQ_PREDUMP_TIMEOUT_MS));

    // if SW-timeout, pre-dump hang instructions
    while(0 == waitQ && retryCount < CMDQ_PREDUMP_RETRY_COUNT)
    {
        CMDQ_LOG("=============== [CMDQ] SW timeout Pre-dump(%d)===============\n", retryCount);
        ++retryCount;

        spin_lock_irqsave(&gCmdqExecLock, flags);
        cmdq_core_dump_status("INFO");
        cmdq_core_dump_pc(pTask, thread, "INFO");
        // HACK: check trigger thread status
        // we assume the first non-high-priority thread is trigger loop thread.
        // since it will start very early
        if (gCmdqContext.thread[CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT].taskCount
            && gCmdqContext.thread[CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT].pCurTask[1]
            && gCmdqContext.thread[CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT].loopCallback)
        {
            uint32_t regValue = 0;
            cmdq_core_dump_pc(gCmdqContext.thread[CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT].pCurTask[1], CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT, "INFO");
            CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_ID, CMDQ_EVENT_DISP_RDMA0_EOF);
            regValue = CMDQ_REG_GET32(CMDQ_SYNC_TOKEN_VAL);
            CMDQ_LOG("CMDQ_SYNC_TOKEN_VAL of %s is %d\n", cmdq_core_get_event_name(CMDQ_EVENT_DISP_RDMA0_EOF), regValue);
        }
        // end of HACK

        spin_unlock_irqrestore(&gCmdqExecLock, flags);

        // then we wait again
        waitQ = wait_event_timeout(gCmdWaitQueue[thread],
                                   (TASK_STATE_BUSY != pTask->taskState && TASK_STATE_WAITING != pTask->taskState),
                                   msecs_to_jiffies(CMDQ_PREDUMP_TIMEOUT_MS));
    }

    // so the maximum total waiting time would be
    // CMDQ_PREDUMP_TIMEOUT_MS * CMDQ_PREDUMP_RETRY_COUNT
    pTask->wakedUp = sched_clock();
    CMDQ_MSG("WAIT: task 0x%p waitq=%d state=%d\n", pTask, waitQ, pTask->taskState);
    CMDQ_PROF_END(current->pid, "wait_for_task_done");

    // Note that although we disable IRQ, HW continues to execute
    // so it's possible to have pending IRQ
    spin_lock_irqsave(&gCmdqExecLock, flags);

    do
    {
        TaskStruct *pNextTask = NULL;
        TaskStruct *pPrevTask = NULL;
        TaskStruct *pRunningTask = NULL;
        int32_t     cookie = 0;
        long        threadPC = 0L;

        status = 0;
        throwAEE = false;
        markAsErrorTask = false;

        if (TASK_STATE_DONE == pTask->taskState)
        {
            break;
        }

        CMDQ_ERR("Task state of %p is not TASK_STATE_DONE, %d\n", pTask, pTask->taskState);

        // Oops, tha tasks is not done.
        // We have several possible error scenario:
        // 1. task still running (hang / timeout)
        // 2. IRQ pending (done or error/timeout IRQ)
        // 3. task's SW thread has been signaled (e.g. SIGKILL)

        // suspend HW thread first, so that we work in a consistent state
        status = cmdq_core_suspend_HW_thread(thread);
        if (0 > status)
        {
            throwAEE = true;
        }

        // The cookie of the task currently being processed
        cookie = CMDQ_GET_COOKIE_CNT(thread) + 1;
        pRunningTask = pThread->pCurTask[cookie % CMDQ_MAX_TASK_IN_THREAD];
        threadPC = CMDQ_AREG_TO_PHYS(CMDQ_REG_GET32(CMDQ_THR_CURR_ADDR(thread)));

        // process any pending IRQ
        // TODO: provide no spin lock version because we already locked.
        irqFlag = CMDQ_REG_GET32(CMDQ_THR_IRQ_STATUS(thread));
        if(irqFlag & 0x12)
        {
            cmdqCoreHandleError(thread, irqFlag, &pTask->wakedUp);
        }
        else if (irqFlag & 0x01)
        {
            cmdqCoreHandleDone(thread, irqFlag, &pTask->wakedUp);
        }
        CMDQ_REG_SET32(CMDQ_THR_IRQ_STATUS(thread), ~irqFlag);

        // check if this task has finished after handling pending IRQ
        if (TASK_STATE_DONE == pTask->taskState)
        {
            break;
        }

        // Then decide we are SW timeout or SIGNALed (not an error)
        if (0 == waitQ)
        {
            // SW timeout and no IRQ received
            markAsErrorTask = true;

            // if we reach here, we're in errornous state.
            // print error log immediately.
            cmdq_core_attach_error_task(pTask, thread);

            CMDQ_ERR("SW timeout of task 0x%p on thread %d\n", pTask, thread);
            throwAEE = true;
            cmdq_core_parse_error(pTask, thread,
                                  &module,
                                  &irqFlag,
                                  &instA,
                                  &instB);
            status = -ETIMEDOUT;

        }
        else if (0 > waitQ)  // Task be killed. Not an error, but still need removal.
        {
            markAsErrorTask = false;

            if(-ERESTARTSYS == waitQ)
            {
                CMDQ_ERR("Task %p KILLED by waitQ = -ERESTARTSYS\n", pTask);
            }
            else if(-EINTR == waitQ)
            {
                CMDQ_ERR("Task %p KILLED by waitQ = -EINTR\n", pTask);
            }
            else
            {
                CMDQ_ERR("Task %p KILLED by waitQ = %d\n", pTask, waitQ);
            }

            status = waitQ;
        }

        // reset HW engine immediately if we already got error IRQ.
        if (TASK_STATE_ERROR == pTask->taskState)
        {
            cmdq_core_reset_hw_engine(pTask->engineFlag);
        }
        else if (TASK_STATE_BUSY == pTask->taskState)
        {
            //
            // if taskState is BUSY, this means we did not reach EOC, did not have error IRQ.
            //      - remove the task from thread.pCurTask[]
            //      - and decrease thread.taskCount
            // NOTE: after this, the pCurTask will not contain link to pTask anymore.
            //       and pTask should become TASK_STATE_ERROR

            // we find our place in pThread->pCurTask[].
            for (index = 0; index < CMDQ_MAX_TASK_IN_THREAD; ++index)
            {
                if (pThread->pCurTask[index] == pTask)
                {
                    // update taskCount and pCurTask[]
                    cmdq_core_remove_task_from_thread_array_by_cookie(pThread,
                                                                      index,
                                                                      markAsErrorTask ? TASK_STATE_ERROR : TASK_STATE_KILLED);
                    break;
                }
            }
        }

        // Then, we try remove pTask from the chain of pThread->pCurTask.
        // . if HW PC falls in pTask range
        //      . HW EXEC_CNT += 1
        //      . thread.waitCookie += 1
        //      . set HW PC to next task head
        // . if not, find previous task (whose jump address is pTask->MVABase)
        //      . check if HW PC points is not at the EOC/JUMP end
        //          . change jump to fake EOC(no IRQ)
        //          . insert jump to next task head and increase cmd buffer size
        //              . if there is no next task, set HW End Address
        if (pTask->pCMDEnd && threadPC >= pTask->MVABase && threadPC <= (pTask->MVABase+pTask->commandSize))
        {
            pNextTask = NULL;

            // find pTask's jump destination
            if (0x10000001 == pTask->pCMDEnd[0])
            {
                pNextTask = cmdq_core_search_task_by_pc(pTask->pCMDEnd[-1], pThread);
            }

            if (pNextTask)
            {
                // cookie already +1
                CMDQ_REG_SET32(CMDQ_THR_EXEC_CNT(thread), cookie);
                pThread->waitCookie = cookie + 1;
                CMDQ_REG_SET32(CMDQ_THR_CURR_ADDR(thread), CMDQ_PHYS_TO_AREG(pNextTask->MVABase));
                CMDQ_MSG("WAIT: resume task 0x%p from err\n", pNextTask);
            }
        }
        else
        {
            pPrevTask = NULL;
            for (index = 0; index < CMDQ_MAX_TASK_IN_THREAD; ++index)
            {
                pPrevTask = pThread->pCurTask[index];

                // find which task JUMP into pTask
                if (pPrevTask &&
                    pPrevTask->pCMDEnd[-1] == pTask->MVABase &&     // pTask command
                    pPrevTask->pCMDEnd[0]  == 0x10000001)           // JUMP
                {
                    // Fake EOC command
                    // only increment EXEC_CNT, but no IRQ throw
                    pPrevTask->pCMDEnd[-1]  = 0x00000000;
                    pPrevTask->pCMDEnd[ 0]  = 0x40000000;

                    // Bypass the task
                    pPrevTask->pCMDEnd[ 1]  = pTask->pCMDEnd[-1];
                    pPrevTask->pCMDEnd[ 2]  = pTask->pCMDEnd[ 0];
                    break;
                }
            }
        }
    }while(0);

    if (pThread->taskCount <= 0)
    {
        #if 0
        CMDQ_LOG("bypass disable_HW_thread, thread:%d\n", thread);
        #else
        cmdq_core_disable_HW_thread(thread);
        #endif
    }
    else
    {
        cmdq_core_resume_HW_thread(thread);
    }

    spin_unlock_irqrestore(&gCmdqExecLock, flags);

    if (throwAEE)
    {
        const uint32_t op = (instA & 0xFF000000) >> 24;
        switch(op)
        {
        case CMDQ_CODE_WFE:
            CMDQ_AEE(module, "%s in CMDQ IRQ:0x%02x, INST:(0x%08x, 0x%08x), OP:WAIT EVENT:%s\n",
                     module,
                     irqFlag,
                     instA, instB,
                     cmdq_core_get_event_name(instA & (~0xFF000000)));
            break;
        default:
            CMDQ_AEE(module, "%s in CMDQ IRQ:0x%02x, INST:(0x%08x, 0x%08x), OP:%s\n", module, irqFlag, instA, instB, cmdq_core_parse_op(op));
            break;
        }
    }

    CMDQ_MSG("<--WAIT: task 0x%p on thread %d end\n", pTask, thread);

    return status;
}

DEBUG_STATIC int32_t cmdq_core_exec_task_async_impl(TaskStruct *pTask,
                                                    int32_t    thread)
{
    int32_t      status;
    ThreadStruct *pThread;
    unsigned long flags;
    int32_t      loop;
    int32_t      count;
    int32_t      minimum;
    int32_t      cookie;
    int32_t      prev;
    TaskStruct   *pPrev;
    TaskStruct   *pLast;
    int32_t      index;
    int          threadPrio = 0;

    CMDQ_MSG("-->EXEC: task 0x%p on thread %d begin\n", pTask, thread);

    if (pTask && pTask->loopCallback)
    {
        CMDQ_LOG("-->EXEC: task 0x%p on thread %d begin\n", pTask, thread);
    }
    
    status = 0;

    pThread = &(gCmdqContext.thread[thread]);

    pTask->trigger = sched_clock();

    spin_lock_irqsave(&gCmdqExecLock, flags);

    // update task's thread info
    pTask->thread = thread;
    pTask->irqFlag = 0;
    pTask->taskState = TASK_STATE_BUSY;

    if (pThread->taskCount <= 0)
    {
        CMDQ_MSG("EXEC: new HW thread(%d)\n", thread);

        if (cmdq_core_reset_HW_thread(thread) < 0)
        {
            spin_unlock_irqrestore(&gCmdqExecLock, flags);
            return -EFAULT;
        }

        CMDQ_REG_SET32(CMDQ_THR_INST_CYCLES(thread), cmdq_core_get_task_timeout_cycle(pThread));
        threadPrio = cmdq_core_priority_from_scenario(pTask->scenario);
        CMDQ_MSG("EXEC: set HW thread(%d) pc:%pa, qos:%d\n", thread, &pTask->MVABase, threadPrio);
        CMDQ_REG_SET32(CMDQ_THR_CURR_ADDR(thread), CMDQ_PHYS_TO_AREG(pTask->MVABase));
        CMDQ_REG_SET32(CMDQ_THR_END_ADDR(thread), CMDQ_PHYS_TO_AREG(pTask->MVABase + pTask->commandSize));
        CMDQ_REG_SET32(CMDQ_THR_CFG(thread), threadPrio & 0x7); //bit 0-2 for priority level;

        // For loop thread, do not enable timeout
        CMDQ_REG_SET32(CMDQ_THR_IRQ_ENABLE(thread),
                       pThread->loopCallback ? 0x011 : 0x013);

        if (pThread->loopCallback)
        {
            CMDQ_MSG("EXEC: HW thread(%d) in loop func 0x%p\n", thread, pThread->loopCallback);
        }

        minimum = CMDQ_GET_COOKIE_CNT(thread);

        pThread->nextCookie = minimum + 1;
        if (pThread->nextCookie > CMDQ_MAX_COOKIE_VALUE)  // Reach the maximum cookie
        {
            pThread->nextCookie = 0;
        }

        cookie = pThread->nextCookie;
        pThread->waitCookie = cookie;

        pThread->pCurTask[cookie % CMDQ_MAX_TASK_IN_THREAD] = pTask;
        // taskCount must start from 0.
        // and we are the first task, so set to 1.
        pThread->taskCount = 1;
        pThread->allowDispatching = 1;

        pThread->nextCookie += 1;
        if (pThread->nextCookie > CMDQ_MAX_COOKIE_VALUE)  // Reach the maximum cookie
        {
            pThread->nextCookie = 0;
        }

        CMDQ_MSG("enable HW thread(%d)\n", thread);

        CMDQ_PROF_MMP(cmdq_mmp_get_event()->thread_en,
                   MMProfileFlagPulse,
                   thread,
                   pThread->nextCookie - 1);

        CMDQ_REG_SET32(CMDQ_THR_ENABLE_TASK(thread), 0x01);
    }
    else
    {
        CMDQ_MSG("EXEC: reuse HW thread(%d)\n", thread);
        
        if (pTask->loopCallback)
        {
            CMDQ_ERR("[WARNING]trigger loop task 0x%p on thread %d has running task count=%d\n", pTask, thread, pThread->taskCount);
        }

        status = cmdq_core_suspend_HW_thread(thread);
        if (status < 0)
        {
            spin_unlock_irqrestore(&gCmdqExecLock, flags);
            return status;
        }

        CMDQ_REG_SET32(CMDQ_THR_INST_CYCLES(thread), cmdq_core_get_task_timeout_cycle(pThread));

        cookie  = pThread->nextCookie;

        // Boundary case tested: EOC have been executed, but JUMP is not executed
        // Thread PC: 0x9edc0dd8, End: 0x9edc0de0, Curr Cookie: 1, Next Cookie: 2
        if((CMDQ_AREG_TO_PHYS(CMDQ_REG_GET32(CMDQ_THR_CURR_ADDR(thread))) == (CMDQ_AREG_TO_PHYS(CMDQ_REG_GET32(CMDQ_THR_END_ADDR(thread))) - 8)) ||  // PC = END - 8, EOC is executed
           (CMDQ_AREG_TO_PHYS(CMDQ_REG_GET32(CMDQ_THR_CURR_ADDR(thread))) == (CMDQ_AREG_TO_PHYS(CMDQ_REG_GET32(CMDQ_THR_END_ADDR(thread))) - 0)))    // PC = END - 0, All CMDs are executed
        {
            CMDQ_MSG("EXEC: Set HW thread(%d) pc:%pa\n", thread, &pTask->MVABase);
            CMDQ_REG_SET32(CMDQ_THR_CURR_ADDR(thread), CMDQ_PHYS_TO_AREG(pTask->MVABase));
            CMDQ_REG_SET32(CMDQ_THR_END_ADDR(thread), CMDQ_PHYS_TO_AREG(pTask->MVABase + pTask->commandSize));

            pThread->pCurTask[cookie % CMDQ_MAX_TASK_IN_THREAD] = pTask;
            pThread->taskCount++;
        }
        else
        {
            CMDQ_MSG("Connect new task's MVA to previous one\n");

            // Current task that shuld be processed
            minimum = CMDQ_GET_COOKIE_CNT(thread) + 1;
            if (minimum > CMDQ_MAX_COOKIE_VALUE)
            {
                minimum = 0;
            }

            // Calculate loop count to adjust the tasks' order
            if (minimum <= cookie)
            {
                loop = cookie - minimum;
            }
            else
            {
                // Counter wrapped
                loop = (CMDQ_MAX_COOKIE_VALUE - minimum + 1) + cookie;
            }

            CMDQ_MSG("Reorder task in range [%d, %d] with count %d\n", minimum, cookie, loop);

            if (loop < 1)
            {
                CMDQ_AEE("CMDQ", "Invalid task count(%d) for reorder\n", loop);
                spin_unlock_irqrestore(&gCmdqExecLock, flags);
                return -EFAULT;
            }
            else
            {
                CMDQ_MSG("Reorder %d tasks for performance begin\n", loop);

                pLast = pTask;  // Default last task

                // Adjust tasks' order according to their priorities
                for (index = (cookie % CMDQ_MAX_TASK_IN_THREAD); loop > 0; loop--, index--)
                {
                    if (index < 0)
                    {
                        index = 15;
                    }

                    prev = index - 1;
                    if (prev < 0)
                    {
                        prev = 15;
                    }

                    pPrev = pThread->pCurTask[prev];

                    // Maybe the job is killed, search a new one
                    count = CMDQ_MAX_TASK_IN_THREAD;
                    while((NULL == pPrev) && (count > 0))
                    {
                        prev = prev - 1;
                        if (prev < 0)
                        {
                            prev = CMDQ_MAX_TASK_IN_THREAD - 1;
                        }

                        pPrev = pThread->pCurTask[prev];
                        loop--;
                        index--;
                        count--;
                    }

                    if (NULL != pPrev)
                    {
                        if (loop > 1)
                        {
                            if (pPrev->priority < pTask->priority)
                            {
                                CMDQ_MSG("Switch prev(%d) and current(%d) order\n", prev, index);

                                pThread->pCurTask[index] = pPrev;
                                pPrev->pCMDEnd[ 0] = pTask->pCMDEnd[ 0];
                                pPrev->pCMDEnd[-1] = pTask->pCMDEnd[-1];

                                // Boot priority for the task
                                pPrev->priority += CMDQ_MIN_AGE_VALUE;

                                pThread->pCurTask[prev]  = pTask;
                                pTask->pCMDEnd[ 0] = 0x10000001;     //Jump: Absolute
                                pTask->pCMDEnd[-1] = pPrev->MVABase; //Jump to here

                                if (pLast == pTask)
                                {
                                    pLast = pPrev;
                                }
                            }
                            else
                            {
                                CMDQ_MSG("Set current(%d) order for the new task\n", index);

                                CMDQ_MSG("Original PC %pa, end %pa\n", &pPrev->MVABase, &pPrev->MVABase + pPrev->commandSize);
                                CMDQ_MSG("Original instruction 0x%08x, 0x%08x\n", pPrev->pCMDEnd[0], pPrev->pCMDEnd[-1]);

                                pThread->pCurTask[index] = pTask;
                                pPrev->pCMDEnd[ 0] = 0x10000001;     //Jump: Absolute
                                pPrev->pCMDEnd[-1] = pTask->MVABase; //Jump to here
                                break;
                            }
                        }
                        else
                        {
                            CMDQ_MSG("Set current(%d) order for the new task\n", index);

                            CMDQ_MSG("Original PC %pa, end %pa\n", &pPrev->MVABase, &pPrev->MVABase + pPrev->commandSize);
                            CMDQ_MSG("Original instruction 0x%08x, 0x%08x\n", pPrev->pCMDEnd[0], pPrev->pCMDEnd[-1]);

                            pThread->pCurTask[index] = pTask;
                            pPrev->pCMDEnd[ 0] = 0x10000001;     //Jump: Absolute
                            pPrev->pCMDEnd[-1] = pTask->MVABase; //Jump to here
                            break;
                        }
                    }
                    else
                    {
                        cmdq_core_attach_error_task(pTask, thread);
                        spin_unlock_irqrestore(&gCmdqExecLock, flags);
                        CMDQ_AEE("CMDQ", "Invalid task state for reorder %d %d\n", index, loop);
                        return -EFAULT;
                    }
                }
            }

            CMDQ_MSG("Reorder %d tasks for performance end\n", loop);

            CMDQ_REG_SET32(CMDQ_THR_END_ADDR(thread), CMDQ_PHYS_TO_AREG(pLast->MVABase + pLast->commandSize));
            pThread->taskCount++;
            pThread->allowDispatching = 1;
        }

        pThread->nextCookie += 1;
        if (pThread->nextCookie > CMDQ_MAX_COOKIE_VALUE)  // Reach the maximum cookie
        {
            pThread->nextCookie = 0;
        }

        CMDQ_PROF_MMP(cmdq_mmp_get_event()->thread_en,
                   MMProfileFlagPulse,
                   thread,
                   pThread->nextCookie - 1);

        cmdq_core_resume_HW_thread(thread);
    }

    spin_unlock_irqrestore(&gCmdqExecLock, flags);

    CMDQ_MSG("<--EXEC: status: %d\n", status);

    return status;
}

static const char *gCmdqThreadLabel[CMDQ_MAX_THREAD_COUNT] =
{
    "CMDQ_IRQ_THR_0",
    "CMDQ_IRQ_THR_1",
    "CMDQ_IRQ_THR_2",
    "CMDQ_IRQ_THR_3",
    "CMDQ_IRQ_THR_4",
    "CMDQ_IRQ_THR_5",
    "CMDQ_IRQ_THR_6",
    "CMDQ_IRQ_THR_7",
    "CMDQ_IRQ_THR_8",
    "CMDQ_IRQ_THR_9",
    "CMDQ_IRQ_THR_10",
    "CMDQ_IRQ_THR_11",
    "CMDQ_IRQ_THR_12",
    "CMDQ_IRQ_THR_13",
    "CMDQ_IRQ_THR_14",
    "CMDQ_IRQ_THR_15",
};

DEBUG_STATIC void cmdqCoreHandleError(int32_t thread, int32_t value, CMDQ_TIME *pGotIRQ)
{
    ThreadStruct *pThread = NULL;
    TaskStruct *pTask = NULL;
    int32_t      cookie;
    int32_t      count;
    int32_t      inner;

    CMDQ_ERR("IRQ: error thread=%d, irq_flag=0x%x\n", thread, value);

    pThread = &(gCmdqContext.thread[thread]);

    cookie = CMDQ_GET_COOKIE_CNT(thread);
    // we assume error happens BEFORE EOC
    // because it wouldn't be error if this interrupt is issue by EOC.
    // So we should inc by 1 to locate "current" task
    cookie += 1;

    // Set the issued task to error state
    if (NULL != pThread->pCurTask[cookie % CMDQ_MAX_TASK_IN_THREAD])
    {
        pTask = pThread->pCurTask[cookie % CMDQ_MAX_TASK_IN_THREAD];
        pTask->gotIRQ = *pGotIRQ;
        pTask->irqFlag = value;
        cmdq_core_attach_error_task(pTask, thread);
        cmdq_core_remove_task_from_thread_array_by_cookie(pThread,
                                                          cookie % CMDQ_MAX_TASK_IN_THREAD,
                                                          TASK_STATE_ERROR);
    }

    // Set the remain tasks to done state
    if (pThread->waitCookie <= cookie)
    {
        count = cookie - pThread->waitCookie + 1;
    }
    else
    {
        // Counter wrapped
        count = (CMDQ_MAX_COOKIE_VALUE - pThread->waitCookie + 1) + (cookie + 1);
    }

    for (inner = (pThread->waitCookie % CMDQ_MAX_TASK_IN_THREAD); count > 0; count--, inner++)
    {
        if (inner >= CMDQ_MAX_TASK_IN_THREAD)
        {
            inner = 0;
        }

        if (NULL != pThread->pCurTask[inner])
        {
            pTask = pThread->pCurTask[inner];
            pTask->gotIRQ = sched_clock();
            pTask->irqFlag = 0; // we don't know the exact irq flag.
            cmdq_core_remove_task_from_thread_array_by_cookie(pThread,
                                                              inner,
                                                              TASK_STATE_DONE);
        }
    }

    pThread->waitCookie = cookie + 1;
    if (pThread->waitCookie > CMDQ_MAX_COOKIE_VALUE)
    {
        pThread->waitCookie -= (CMDQ_MAX_COOKIE_VALUE + 1); // min cookie value is 0
    }

    wake_up(&gCmdWaitQueue[thread]);
}

DEBUG_STATIC void cmdqCoreHandleDone(int32_t thread, int32_t value, CMDQ_TIME *pGotIRQ)
{
    ThreadStruct *pThread;
    int32_t      cookie;
    int32_t      count;
    int32_t      inner;
    int32_t      loopResult = 0;

    pThread = &(gCmdqContext.thread[thread]);

    //
    // Loop execution never gets done; unless
    // user loop function returns error
    //
    if (NULL != pThread->loopCallback)
    {
        loopResult = pThread->loopCallback(pThread->loopData);

        CMDQ_PROF_MMP(cmdq_mmp_get_event()->loopBeat,
                           MMProfileFlagPulse,
                           thread,
                           loopResult);

        if (loopResult >= 0)
        {
            // Success, contiue execution as if nothing happens
            CMDQ_REG_SET32(CMDQ_THR_IRQ_STATUS(thread), ~value);
            return;
        }
    }

    // do not print excessive message for looping thread
    if (NULL == pThread->loopCallback)
    {
        #ifdef CONFIG_MTK_FPGA
        // ASYNC: debug log, use printk_sched to prevent block IRQ handler
        CMDQ_MSG("IRQ: Done, thread: %d\n", thread);
        #endif
    }

    if (loopResult < 0)
    {
        // The loop CB failed, so stop HW thread now.
        cmdq_core_disable_HW_thread(thread);

        // loop CB failed. the EXECUTION count should not be used as cookie,
        // since it will increase by each loop iteration.
        cookie = pThread->waitCookie;
    }
    else
    {
        // Normal task cookie
        cookie = CMDQ_GET_COOKIE_CNT(thread);
    }

    if (pThread->waitCookie <= cookie)
    {
        count = cookie - pThread->waitCookie + 1;
    }
    else
    {
        // Counter wrapped
        count = (CMDQ_MAX_COOKIE_VALUE - pThread->waitCookie + 1) + (cookie + 1);
    }

    for (inner = (pThread->waitCookie % CMDQ_MAX_TASK_IN_THREAD); count > 0; count--, inner++)
    {
        if (inner >= CMDQ_MAX_TASK_IN_THREAD)
        {
            inner = 0;
        }

        if (NULL != pThread->pCurTask[inner])
        {
            struct TaskStruct *pTask = pThread->pCurTask[inner];
            pTask->gotIRQ = *pGotIRQ;
            pTask->irqFlag = value;
            cmdq_core_remove_task_from_thread_array_by_cookie(pThread,
                                                              inner,
                                                              TASK_STATE_DONE);
        }
    }

    CMDQ_PROF_MMP(cmdq_mmp_get_event()->CMDQ_IRQ,
                   MMProfileFlagPulse,
                   thread,
                   cookie);

    pThread->waitCookie = cookie + 1;
    if (pThread->waitCookie > CMDQ_MAX_COOKIE_VALUE)
    {
        pThread->waitCookie -= (CMDQ_MAX_COOKIE_VALUE + 1); // min cookie value is 0
    }

    wake_up(&gCmdWaitQueue[thread]);
}

void cmdqCoreHandleIRQ(int32_t thread)
{
    unsigned long flags = 0L;
    int32_t      isInvalidInstruction = false;
    bool         isAlreadySuspended = false;
    CMDQ_TIME gotIRQ;
    int value;
    int enabled;

    gotIRQ = sched_clock();

    //
    //  Normal execution, marks tasks done and remove from thread
    //  Also, handle "loop CB fail" case
    //
    spin_lock_irqsave(&gCmdqExecLock, flags);

    // it is possible for another CPU core
    // to run "releaseTask" right before we acquire the spin lock
    // and thus reset / disable this HW thread
    // so we check both the IRQ flag and the enable bit of this thread
    value = CMDQ_REG_GET32(CMDQ_THR_IRQ_STATUS(thread));

    if (0 == (value & 0x13))
    {        
        CMDQ_ERR("IRQ: thread %d got interrupt but IRQ flag is 0x%08x\n", thread, value);
        spin_unlock_irqrestore(&gCmdqExecLock, flags);
        return;
    }

    enabled = CMDQ_REG_GET32(CMDQ_THR_ENABLE_TASK(thread));

    if (0 == (enabled & 0x01))
    {
        CMDQ_ERR("IRQ: thread %d got interrupt already disabled 0x%08x\n", thread, enabled);
        spin_unlock_irqrestore(&gCmdqExecLock, flags);
        return;
    }

    CMDQ_PROF_START(0, gCmdqThreadLabel[thread]);

    // we must suspend thread before query cookie
    isAlreadySuspended = ((CMDQ_REG_GET32(CMDQ_THR_SUSPEND_TASK(thread)) & 0x1) > 0) ? true : false;
    isInvalidInstruction = value & 0x10;

    // suspend HW thread first to prevent race condition between HW and SW 
    // for example, HW executes a task done and asserts interrupt when driver's process ISR
    // in such case HW performs two task done, but SW mistakes just one task done...[ALPS01544172]
    if (false == isAlreadySuspended)
    {
        CMDQ_VERBOSE("IRQ: suspend thread %d\n", thread);
        cmdq_core_suspend_HW_thread(thread);
    }

    if(value & 0x12)
    {
        cmdqCoreHandleError(thread, value, &gotIRQ);
    }
    else if (value & 0x01)
    {
        cmdqCoreHandleDone(thread, value, &gotIRQ);
    }

    CMDQ_REG_SET32(CMDQ_THR_IRQ_STATUS(thread), ~value);

    // do not resume if it is invalide instruction.
    // let user space handle this situation.
    if (false == isAlreadySuspended && false == isInvalidInstruction)
    {
        cmdq_core_resume_HW_thread(thread);
    }

    CMDQ_PROF_END(0, gCmdqThreadLabel[thread]);

    spin_unlock_irqrestore(&gCmdqExecLock, flags);

}


int32_t cmdqCoreSuspend()
{
    unsigned long flags = 0L;
    EngineStruct *pEngine = NULL;
    uint32_t execThreads = 0x0;
    int refCount = 0;
    bool killTasks = false;
    struct TaskStruct *pTask = NULL;
    struct list_head *p = NULL;
    int i = 0;

    pEngine = gCmdqContext.engine;

    execThreads = CMDQ_REG_GET32(CMDQ_CURR_LOADED_THR);
    refCount = atomic_read(&gCmdqThreadUsage);
    
    if(0 > cmdq_core_can_module_entry_suspend(pEngine))
    {
        CMDQ_ERR("[SUSPEND] MDP running, kill tasks. threads:0x%08x, ref:%d\n", execThreads, refCount);
        killTasks = true;
    }
    else if ((refCount > 0) || (0x80000000 & execThreads))
    {
        CMDQ_ERR("[SUSPEND] other running, kill tasks. threads:0x%08x, ref:%d\n", execThreads, refCount);
        killTasks = true;
    }

    //
    // We need to ensure the system is ready to suspend,
    // so kill all running CMDQ tasks
    // and release HW engines.
    //
    if (killTasks)
    {
        // print active tasks
        CMDQ_ERR("[SUSPEND] active tasks during suspend:\n");
        list_for_each(p, &gCmdqContext.taskActiveList)
        {
            pTask = list_entry(p, struct TaskStruct, listEntry);
            cmdq_core_dump_task(pTask);
        }

        // remove all active task from thread
        CMDQ_ERR("[SUSPEND] remove all active tasks\n");
        list_for_each(p, &gCmdqContext.taskActiveList)
        {
            pTask = list_entry(p, struct TaskStruct, listEntry);
            if (pTask->thread != CMDQ_INVALID_THREAD)
            {
                spin_lock_irqsave(&gCmdqExecLock, flags);
                cmdq_core_force_remove_task_from_thread(pTask, pTask->thread);
                pTask->taskState = TASK_STATE_KILLED;
                spin_unlock_irqrestore(&gCmdqExecLock, flags);
                
                // release all thread and mark all active tasks as "KILLED"
                // (so that thread won't release again)
                CMDQ_ERR("[SUSPEND] release all threads and HW clocks\n");
                cmdq_core_release_thread(pTask);
            }
        }

        // disable all HW thread
        CMDQ_ERR("[SUSPEND] disable all HW threads\n");
        for(i = 0; i < CMDQ_MAX_THREAD_COUNT; ++i)
        {
            cmdq_core_disable_HW_thread(i);
        }
        
        // reset all threadStruct
        memset(&gCmdqContext.thread[0], 0, sizeof(gCmdqContext.thread));
        cmdq_core_reset_thread_struct();
        
        // reset all engineStruct
        memset(&gCmdqContext.engine[0], 0, sizeof(gCmdqContext.engine));
        cmdq_core_reset_engine_struct();
    }

    spin_lock_irqsave(&gCmdqThreadLock, flags);
    gCmdqSuspended = true;
    spin_unlock_irqrestore(&gCmdqThreadLock, flags);

    // ALWAYS allow suspend
    return 0;
}


int32_t cmdqCoreResume()
{
    unsigned long flags = 0L;
    int refCount = 0;

    spin_lock_irqsave(&gCmdqThreadLock, flags);

    refCount = atomic_read(&gCmdqThreadUsage);
    CMDQ_MSG("[RESUME] resume, refCount:%d\n", refCount);

    gCmdqSuspended = false;

    // during suspending, there may be queued tasks.
    // we should process them if any.
    if (!work_pending(&gCmdqContext.taskConsumeWaitQueueItem))
    {
        CMDQ_MSG("[RESUME] there are undone task, process them\n");
        // we use system global work queue (kernel thread kworker/n)
        CMDQ_PROF_MMP(cmdq_mmp_get_event()->consume_add,
                   MMProfileFlagPulse,
                   0,
                   0);
        queue_work(gCmdqContext.taskConsumeWQ, &gCmdqContext.taskConsumeWaitQueueItem);
    }

    spin_unlock_irqrestore(&gCmdqThreadLock, flags);
    return 0;
}


DEBUG_STATIC int32_t cmdq_core_exec_task_async_with_retry(TaskStruct *pTask,
                                                   int32_t    thread)
{
    int32_t        retry;
    int32_t        status;

    retry = 0;
    status = -EFAULT;
    do
    {
        status = cmdq_core_exec_task_async_impl(pTask,
                                                thread);
        if (status >= 0)
        {
            break;
        }

        if((TASK_STATE_KILLED == pTask->taskState) ||
           (TASK_STATE_ERROR  == pTask->taskState))
        {
            CMDQ_ERR("cmdq_core_exec_task_async_impl fail\n");
            status = -EFAULT;
            break;
        }

        ++retry;
    } while(retry < CMDQ_MAX_RETRY_COUNT);

    return status;
}

DEBUG_STATIC void cmdq_core_consume_waiting_list(struct work_struct * _ignore)
{
    struct list_head *p, *n = NULL;
    struct TaskStruct *pTask = NULL;
    struct ThreadStruct *pThread = NULL;
    int32_t thread = CMDQ_INVALID_THREAD;
    int32_t status = 0;
    bool threadAcquired = false;
    CMDQ_HW_THREAD_PRIORITY_ENUM thread_prio = CMDQ_THR_PRIO_NORMAL;
    CMDQ_TIME consumeTime;
    int32_t waitingTimeMS;
    bool needLog = false;

    // when we're suspending, do not execute any tasks. delay & hold them.
    if ( gCmdqSuspended )
    {
        return;
    }

    CMDQ_PROF_START(current->pid, __FUNCTION__);
    CMDQ_PROF_MMP(cmdq_mmp_get_event()->consume_done,
                   MMProfileFlagStart,
                   current->pid,
                   0);
    consumeTime = sched_clock();

    mutex_lock(&gCmdqTaskMutex);

    threadAcquired = false;

    // scan and remove (if executed) waiting tasks
    list_for_each_safe(p, n, &gCmdqContext.taskWaitList)
    {
        pTask = list_entry(p, struct TaskStruct, listEntry);

        thread_prio = cmdq_core_priority_from_scenario(pTask->scenario);

        CMDQ_MSG("-->THREAD: try acquire thread for task: 0x%p, thread_prio: %d, task_prio: %d, flag: 0x%llx  begin\n", pTask, thread_prio, pTask->priority, pTask->engineFlag);
        CMDQ_GET_TIME_IN_MS(pTask->submit, consumeTime, waitingTimeMS);
        needLog = waitingTimeMS >= CMDQ_PREDUMP_TIMEOUT_MS;

        // Allocate hw thread
        thread = cmdq_core_acquire_thread(pTask->engineFlag,
                                          thread_prio,
                                          pTask->scenario,
                                          needLog);
        pThread = &gCmdqContext.thread[thread];

        if (CMDQ_INVALID_THREAD == thread || NULL == pThread)
        {
            // have to wait, remain in wait list
            CMDQ_MSG("<--THREAD: acquire thread fail, need to wait\n");
            if (needLog)
            {
                // task wait too long
                CMDQ_ERR("acquire thread pre-dump for task: 0x%p, thread_prio: %d, task_prio: %d, flag: 0x%llx\n", pTask, thread_prio, pTask->priority, pTask->engineFlag);
            }
            continue;
        }

        // some task is ready to run
        threadAcquired = true;

        // Assign loop function if the thread should be a loop thread
        pThread->loopCallback = pTask->loopCallback;
        pThread->loopData = pTask->loopData;

        if (pTask->loopCallback)
        {
            CMDQ_LOG("[TRIGGER]loop 0x%p on thread %d\n", pTask, thread);
        }

        // Start execution,
        // remove from wait list and put into active list
        list_del_init(&(pTask->listEntry));
        list_add_tail(&(pTask->listEntry), &gCmdqContext.taskActiveList);

        CMDQ_MSG("<--THREAD: acquire thread w/flag: 0x%llx on thread(%d): 0x%p end\n", pTask->engineFlag, thread, pThread);

        // Run task on thread
        status = cmdq_core_exec_task_async_with_retry(pTask, thread);
        if (status < 0)
        {
            CMDQ_ERR("<--THREAD: cmdq_core_exec_task_async_with_retry fail, release task 0x%p\n", pTask);
            cmdq_core_track_task_record(pTask, thread);
            cmdq_core_release_thread(pTask);
            cmdq_core_release_task_unlocked(pTask);
            pTask = NULL;
        }
    }

    if (threadAcquired)
    {
        // notify some task's SW thread to change their waiting state.
        // (if they already called cmdqCoreWaitResultAndReleaseTask())
        wake_up_all(&gCmdqThreadDispatchQueue);
    }

    mutex_unlock(&gCmdqTaskMutex);

    CMDQ_PROF_END(current->pid, __FUNCTION__);

    CMDQ_PROF_MMP(cmdq_mmp_get_event()->consume_done,
                   MMProfileFlagEnd,
                   current->pid,
                   0);

    return;
}

int32_t cmdqCoreSubmitTaskAsync(cmdqCommandStruct *pCommandDesc,
                                CmdqInterruptCB loopCB,
                                unsigned long loopData,
                                TaskStruct **ppTaskOut
                                )
{
    struct TaskStruct *pTask = NULL;
    int32_t status = 0;

    CMDQ_MSG("-->SUBMIT_ASYNC: cmd 0x%p begin\n", pCommandDesc->pVABase);
    CMDQ_PROF_START(current->pid, __FUNCTION__);

    CMDQ_PROF_MMP(cmdq_mmp_get_event()->alloc_task,
                   MMProfileFlagStart,
                   current->pid,
                   0);

    // Allocate Task. This creates a new task
    // and put into tail of waiting list
    pTask = cmdq_core_acquire_task(pCommandDesc,
                                   loopCB,
                                   loopData);

    CMDQ_PROF_MMP(cmdq_mmp_get_event()->alloc_task,
                   MMProfileFlagEnd,
                   current->pid,
                   0);

    if (NULL == pTask)
    {
        CMDQ_PROF_END(current->pid, __FUNCTION__);
        return -EFAULT;
    }

    if(NULL != ppTaskOut)
    {
        *ppTaskOut = pTask;
    }

    // consume the waiting list.
    // this may or may not execute the task,
    // depending on available threads.
    cmdq_core_consume_waiting_list(NULL);

    CMDQ_MSG("<--SUBMIT_ASYNC: task: 0x%p end\n", pCommandDesc->pVABase);
    CMDQ_PROF_END(current->pid, __FUNCTION__);
    return status;
}

int32_t cmdqCoreReleaseTask(TaskStruct *pTask)
{
    unsigned long flags; 
    int32_t status = 0;
    int32_t thread = pTask->thread;
    struct ThreadStruct *pThread = &(gCmdqContext.thread[thread]);

    CMDQ_MSG("<--TASK: cmdqCoreReleaseTask 0x%p\n", pTask);

    if (thread != CMDQ_INVALID_THREAD && NULL != pThread)
    {
        // this task is being executed (or queueed) on a HW thread

        // get SW lock first to ensure atomic access HW
        spin_lock_irqsave(&gCmdqExecLock, flags);        
        smp_mb();
        
        if (pThread->loopCallback)
        {
            // a loop thread has only 1 task involved
            // so we can release thread directly
            // otherwise we need to connect remaining tasks
            BUG_ON(pThread->taskCount > 1);

            // suspend and reset the thread
            status = cmdq_core_suspend_HW_thread(thread);
            BUG_ON(status < 0);
            pThread->taskCount = 0;
            cmdq_core_disable_HW_thread(thread);
        }
        else
        {
            // TODO: we should check thread enabled or not before resume it.
            status = cmdq_core_force_remove_task_from_thread(pTask, thread);
            if (pThread->taskCount > 0)
            {
                cmdq_core_resume_HW_thread(thread);
            }
        }

        spin_unlock_irqrestore(&gCmdqExecLock, flags);
        wake_up(&gCmdWaitQueue[thread]);
    }

    cmdq_core_track_task_record(pTask, thread);
    cmdq_core_release_thread(pTask);
    cmdq_core_release_task(pTask);
    CMDQ_MSG("-->TASK: cmdqCoreReleaseTask 0x%p end\n", pTask);
    return 0;
}

int32_t cmdqCoreWaitAndReleaseTask(TaskStruct *pTask, long timeout_jiffies)
{
    return cmdqCoreWaitResultAndReleaseTask(pTask, NULL, timeout_jiffies);
}

int32_t cmdqCoreWaitResultAndReleaseTask(TaskStruct *pTask, cmdqRegValueStruct *pResult, long timeout_jiffies)
{
    int32_t status;
    int32_t thread;
    int i;

    if (NULL == pTask)
    {
        CMDQ_ERR("cmdqCoreWaitAndReleaseTask err ptr=0x%p\n", pTask);
        return -EFAULT;
    }

    if (pTask->taskState == TASK_STATE_IDLE){
        CMDQ_ERR("cmdqCoreWaitAndReleaseTask task=0x%p is IDLE\n", pTask);
        return -EFAULT;
    }
    CMDQ_PROF_START(current->pid, __FUNCTION__);


    //
    // wait for task finish
    thread = pTask->thread;
    status = cmdq_core_wait_task_done(pTask, timeout_jiffies);

    //
    // retrieve result
    if (pResult && pResult->count)
    {
        // clear results
        memset(pResult->regValues, 0, pResult->count * sizeof(pResult->regValues[0]));

        // fill results
        mutex_lock(&gCmdqTaskMutex);
        for (i = 0; i < pResult->count && i < pTask->regCount; ++i)
        {
            pResult->regValues[i] = pTask->regResults[i];
        }
        mutex_unlock(&gCmdqTaskMutex);
    }

    cmdq_core_track_task_record(pTask, thread);
    cmdq_core_release_thread(pTask);
    cmdq_core_release_task(pTask);

    CMDQ_PROF_END(current->pid, __FUNCTION__);

    return status;
}

DEBUG_STATIC void cmdq_core_auto_release_work(struct work_struct *workItem)
{
    int32_t status = 0;
    TaskStruct *pTask = NULL;
    CmdqAsyncFlushCB finishCallback = NULL;
    uint32_t userData = 0;

    pTask = container_of(workItem, struct TaskStruct, autoReleaseWork);

    if (pTask)
    {
        finishCallback = pTask->flushCallback;
        userData = pTask->flushData;

        status = cmdqCoreWaitResultAndReleaseTask(pTask,
                    NULL,
                    msecs_to_jiffies(CMDQ_DEFAULT_TIMEOUT_MS));

        CMDQ_VERBOSE("[Auto Release] released pTask=%p, statu=%d\n", pTask, status);
        CMDQ_PROF_MMP(cmdq_mmp_get_event()->autoRelease_done,
                   MMProfileFlagPulse,
                   (uint32_t)pTask,
                   current->pid);

        pTask = NULL;

        // Notify user
        if (finishCallback)
        {
            CMDQ_VERBOSE("[Auto Release] call user callback %p with data 0x%08x\n", finishCallback, userData);
            finishCallback(userData);
        }
    }
}

int32_t cmdqCoreAutoReleaseTask(TaskStruct *pTask)
{
    // the work item is embeded in pTask already
    // but we need to initialized it
    INIT_WORK(&pTask->autoReleaseWork, cmdq_core_auto_release_work);

    CMDQ_PROF_MMP(cmdq_mmp_get_event()->autoRelease_add,
                  MMProfileFlagPulse,
                  (uint32_t)pTask,
                  0);

    queue_work(gCmdqContext.taskAutoReleaseWQ, &pTask->autoReleaseWork);
    return 0;
}

void cmdq_core_release_task_by_file_node(void* file_node)
{
    struct TaskStruct *pTask = NULL;
    struct list_head *p = NULL;

    // Since the file node is closed, there is no way
    // user space can issue further "wait_and_close" request,
    // so we must auto-release running/waiting tasks
    // to prevent resource leakage

    // walk through active and waiting lists and release them
    mutex_lock(&gCmdqTaskMutex);

    list_for_each(p, &gCmdqContext.taskActiveList)
    {
        pTask = list_entry(p, struct TaskStruct, listEntry);
        if (TASK_STATE_IDLE != pTask->taskState && 
            pTask->privateData == file_node && 
            CMDQ_SCENARIO_USER_SPACE == pTask->scenario)
        {
            CMDQ_LOG("[WARNING] ACTIVE task 0x%p release because file node 0x%p closed\n", pTask, file_node);
            cmdq_core_dump_task(pTask);

            // since we already inside mutex,
            // do not cmdqReleaseTask directly,
            // instead we change state to "KILLED"
            // and arrange a auto-release.
            // Note that these tasks may already issued to HW
            // so there is a chance that following MPU/M4U violation
            // may occur, if the user space process has destroyed.
            // The ideal solution is to stop / cancel HW operation
            // immediately, but we cannot do so due to SMI hang risk.
            cmdqCoreAutoReleaseTask(pTask);
        }
    }

    list_for_each(p, &gCmdqContext.taskWaitList)
    {
        pTask = list_entry(p, struct TaskStruct, listEntry);
        if (TASK_STATE_WAITING == pTask->taskState && 
            pTask->privateData == file_node && 
            CMDQ_SCENARIO_USER_SPACE == pTask->scenario)
        {
            CMDQ_LOG("[WARNING] WAITING task 0x%p release because file node 0x%p closed\n", pTask, file_node);
            cmdq_core_dump_task(pTask);
            
            // since we already inside mutex,
            // and these WATING tasks will not be consumed (acquire thread / exec)
            // we can release them directly.
            // note that we use unlocked version since we already hold gCmdqTaskMutex.
            cmdq_core_release_task_unlocked(pTask);
        }
    }

    mutex_unlock(&gCmdqTaskMutex);
}

int32_t cmdqCoreSubmitTask(cmdqCommandStruct *pCommandDesc)
{
    int32_t status;
    TaskStruct *pTask = NULL;

    CMDQ_MSG("-->SUBMIT: SYNC cmd 0x%p begin\n", pCommandDesc->pVABase);
    status = cmdqCoreSubmitTaskAsync(pCommandDesc,
                                     NULL,
                                     0,
                                     &pTask);

    if (status >= 0)
    {
        status = cmdqCoreWaitResultAndReleaseTask(pTask,
                                &pCommandDesc->regValue,
                                msecs_to_jiffies(CMDQ_DEFAULT_TIMEOUT_MS));
        if(status < 0)
        {
            CMDQ_ERR("Task 0x%p wait fails\n", pTask);
        }
    }
    else
    {
        CMDQ_ERR("cmdqCoreSubmitTaskAsync failed=%d", status);
    }

    CMDQ_MSG("<--SUBMIT: SYNC cmd 0x%p end\n", pCommandDesc->pVABase);
    return status;
}


int32_t cmdqCoreQueryUsage(int32_t *pCount)
{
    unsigned long flags;
    EngineStruct *pEngine;
    int32_t      index;

    pEngine = gCmdqContext.engine;

    spin_lock_irqsave(&gCmdqThreadLock, flags);

    for (index = 0; index < CMDQ_MAX_ENGINE_COUNT; index++)
    {
        pCount[index] = pEngine[index].userCount;
    }

    spin_unlock_irqrestore(&gCmdqThreadLock, flags);

    return 0;
}

unsigned long long cmdq_core_get_GPR64(const CMDQ_DATA_REGISTER_ENUM regID)
{
    unsigned long long value;
    unsigned long long value1;
    unsigned long long value2;
    const uint32_t x = regID & 0x0F;

    if(0 < (regID & 0x10))
    {
        // query address GPR(64bit), Px
        value1 = 0LL | CMDQ_REG_GET32(CMDQ_GPR_R32((2 * x)));
        value2 = 0LL | CMDQ_REG_GET32(CMDQ_GPR_R32((2 * x + 1)));
    }
    else
    {
        // query data GPR(32bit), Rx
        value1 = 0LL | CMDQ_REG_GET32(CMDQ_GPR_R32(x));
        value2 = 0LL;
    }

    value = (0LL) | (value2 << 32) | (value1);
    CMDQ_VERBOSE("get_GPR64(%x): 0x%llx(0x%llx, 0x%llx)\n", regID, value, value2, value1);

    return value;
}

uint32_t cmdqCoreReadDataRegister(CMDQ_DATA_REGISTER_ENUM regID)
{
    return CMDQ_REG_GET32(CMDQ_GPR_R32(regID));
}

void cmdqCoreDeInitialize()
{
    struct TaskStruct *pTask = NULL;
    struct list_head *p;
    int index;
    struct list_head *lists[] = {&gCmdqContext.taskFreeList, &gCmdqContext.taskActiveList, &gCmdqContext.taskWaitList};

    // directly destory the auto release WQ since we're going to release tasks anyway.
    destroy_workqueue(gCmdqContext.taskAutoReleaseWQ);
    gCmdqContext.taskAutoReleaseWQ = NULL;

    destroy_workqueue(gCmdqContext.taskConsumeWQ);
    gCmdqContext.taskConsumeWQ = NULL;

    // release all tasks in both list
    for (index = 0; index < (sizeof(lists) / sizeof(lists[0])); ++index)
    {
        list_for_each(p, lists[index])
        {
            mutex_lock(&gCmdqTaskMutex);

            pTask = list_entry(p, struct TaskStruct, listEntry);

            // free allocated DMA buffer
            cmdq_task_free_task_command_buffer(pTask);
            kmem_cache_free(gCmdqContext.taskCache, pTask);
            list_del(p);

            mutex_unlock(&gCmdqTaskMutex);
        }
    }

    // check if there are dangling write addresses.
    if (!list_empty(&gCmdqContext.writeAddrList))
    {
        CMDQ_AEE("CMDQ", "there are unreleased write buffer");
    }

    kmem_cache_destroy(gCmdqContext.taskCache);
    gCmdqContext.taskCache = NULL;

    // release emergency buffer
    cmdq_core_uninit_emergency_buffer();
}

void cmdqCoreSetEvent(CMDQ_EVENT_ENUM event)
{
    CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | event);
}

int cmdqCoreAllocWriteAddress(uint32_t count, dma_addr_t *paStart)
{
    WriteAddrStruct *pWriteAddr = NULL;
    int status = 0;

    CMDQ_VERBOSE("ALLOC: line %d\n", __LINE__);

    do{
        if (NULL == paStart)
        {
            CMDQ_ERR("invalid output argument\n");
            status = -EINVAL;
            break;
        }
        *paStart = 0;

        CMDQ_VERBOSE("ALLOC: line %d\n", __LINE__);

        pWriteAddr = kzalloc(sizeof(WriteAddrStruct), GFP_KERNEL);
        if (NULL == pWriteAddr)
        {
            CMDQ_ERR("failed to alloc WriteAddrStruct\n");
            status = -ENOMEM;
            break;
        }
        memset(pWriteAddr, 0, sizeof(WriteAddrStruct));

        CMDQ_VERBOSE("ALLOC: line %d\n", __LINE__);

        pWriteAddr->count = count;
        pWriteAddr->va = dma_alloc_coherent(cmdq_dev_get(), count * sizeof(uint32_t), &(pWriteAddr->pa), GFP_KERNEL);
        if (current)
        {
            pWriteAddr->user = current->pid;
        }

        CMDQ_VERBOSE("ALLOC: line %d\n", __LINE__);

        if (NULL == pWriteAddr->va)
        {
            CMDQ_ERR("failed to alloc write buffer\n");
            status = -ENOMEM;
            break;
        }

        CMDQ_VERBOSE("ALLOC: line %d\n", __LINE__);

        // clear buffer content
        do
        {
            volatile uint32_t *pInt = (uint32_t*)pWriteAddr->va;
            int i = 0;
            for (i = 0; i < count; ++i)
            {
                *(pInt + i) = 0xcdcdabab;
                dsb();
                smp_mb();
            }
        }while(0);

        // assign output pa
        *paStart = pWriteAddr->pa;

        mutex_lock(&gCmdqWriteAddrMutex);
        list_add_tail(&(pWriteAddr->list_node), &gCmdqContext.writeAddrList);
        mutex_unlock(&gCmdqWriteAddrMutex);

        status = 0;

    }while(0);

    if (0 != status)
    {
        // release resources
        if (pWriteAddr->va)
        {
            dma_free_coherent(NULL,
                              sizeof(uint32_t) * pWriteAddr->count,
                              pWriteAddr->va,
                              pWriteAddr->pa);
            memset(pWriteAddr, 0, sizeof(WriteAddrStruct));
        }

        if (pWriteAddr)
        {
            kfree(pWriteAddr);
            pWriteAddr = NULL;
        }
    }

    CMDQ_VERBOSE("ALLOC: line %d\n", __LINE__);

    return status;
}

uint32_t cmdqCoreReadWriteAddress(dma_addr_t pa)
{
    struct list_head *p = NULL;
    WriteAddrStruct *pWriteAddr = NULL;
    int32_t offset = 0;
    uint32_t value = 0;

    // seach for the entry
    mutex_lock(&gCmdqWriteAddrMutex);
    list_for_each(p, &gCmdqContext.writeAddrList)
    {
        pWriteAddr = list_entry(p, struct WriteAddrStruct, list_node);
        if (NULL == pWriteAddr)
        {
            continue;
        }

        offset = pa - pWriteAddr->pa;

        if (offset >= 0 &&
            (offset / sizeof(uint32_t)) < pWriteAddr->count)
        {
            CMDQ_VERBOSE("cmdqCoreReadWriteAddress() input:%pa, got offset=%d va=%p pa_start=%pa\n",
                      &pa,
                      offset,
                      (pWriteAddr->va + offset),
                      &(pWriteAddr->pa));
            value = *((volatile uint32_t*)(pWriteAddr->va + offset));
            CMDQ_VERBOSE("cmdqCoreReadWriteAddress() found offset=%d va=%p value=0x%08x\n", offset, (pWriteAddr->va + offset), value);
            break;
        }
    }
    mutex_unlock(&gCmdqWriteAddrMutex);

    return value;
}

uint32_t cmdqCoreWriteWriteAddress(dma_addr_t pa, uint32_t value)
{
    struct list_head *p = NULL;
    WriteAddrStruct *pWriteAddr = NULL;
    int32_t offset = 0;

    // seach for the entry
    mutex_lock(&gCmdqWriteAddrMutex);
    list_for_each(p, &gCmdqContext.writeAddrList)
    {
        pWriteAddr = list_entry(p, struct WriteAddrStruct, list_node);
        if (NULL == pWriteAddr)
        {
            continue;
        }

        offset = pa - pWriteAddr->pa;

        // note it is 64 bit length for uint32_t variable in 64 bit kernel
        // use sizeof(u_log) to check valid offset range
        if (offset >= 0 &&
            (offset / sizeof(unsigned long)) < pWriteAddr->count)
        {
            CMDQ_VERBOSE("cmdqCoreWriteWriteAddress() input:0x%pa, got offset=%d va=%p pa_start=0x%pa, value=0x%08x\n",
                      &pa,
                      offset,
                      (pWriteAddr->va + offset),
                      &pWriteAddr->pa,
                      value);
            *((volatile uint32_t*)(pWriteAddr->va + offset)) = value;
            break;
        }
    }
    mutex_unlock(&gCmdqWriteAddrMutex);

    return value;
}


int cmdqCoreFreeWriteAddress(dma_addr_t paStart)
{
    struct list_head *p, *n = NULL;
    WriteAddrStruct *pWriteAddr = NULL;

    // seach for the entry
    mutex_lock(&gCmdqWriteAddrMutex);
    list_for_each_safe(p, n, &gCmdqContext.writeAddrList)
    {
        pWriteAddr = list_entry(p, struct WriteAddrStruct, list_node);
        if (pWriteAddr && pWriteAddr->pa == paStart)
        {
            list_del(&(pWriteAddr->list_node));
            break;
        }
    }
    mutex_unlock(&gCmdqWriteAddrMutex);

    if (NULL == pWriteAddr)
    {
        CMDQ_ERR("cmdqCoreFreeWriteAddress() no matching entry\n");
        return -EINVAL;
    }

    // release resources
    if (pWriteAddr->va)
    {
        dma_free_coherent(cmdq_dev_get(),
                          sizeof(uint32_t) * pWriteAddr->count,
                          pWriteAddr->va,
                          pWriteAddr->pa);
        memset(pWriteAddr, 0xdeaddead, sizeof(WriteAddrStruct));
    }

    if (pWriteAddr)
    {
        kfree(pWriteAddr);
        pWriteAddr = NULL;
    }

    return 0;
}

int32_t cmdqCoreDebugRegDumpBegin(uint32_t taskID, uint32_t *regCount, uint32_t **regAddress)
{
    if (NULL == gCmdqDebugCallback.beginDebugRegDump)
    {
        CMDQ_ERR("beginDebugRegDump not registered\n");
        return -EFAULT;
    }

    return gCmdqDebugCallback.beginDebugRegDump(taskID,
                                          regCount,
                                          regAddress);
}

int32_t cmdqCoreDebugRegDumpEnd(uint32_t taskID, uint32_t regCount, uint32_t *regValues)
{
    if (NULL == gCmdqDebugCallback.endDebugRegDump)
    {
        CMDQ_ERR("endDebugRegDump not registered\n");
        return -EFAULT;
    }

    return gCmdqDebugCallback.endDebugRegDump(taskID,
                                        regCount,
                                        regValues);
}

bool cmdq_core_should_print_msg(void)
{
    return (gCmdqContext.logLevel > 0);
}

bool cmdq_core_profile_enabled(void)
{
    return (gCmdqContext.enableProfile > 0);
}
