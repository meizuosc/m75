#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <mach/mt_clkmgr.h>
#include <mach/memory.h>

#include "cmdq_record.h"
#include "cmdq_reg.h"
#include "cmdq_core.h"
#include "cmdq_device.h"



#define CMDQ_TEST

#ifdef CMDQ_TEST
// test register
#ifdef CMDQ_OF_SUPPORT
#define MMSYS_CONFIG_BASE cmdq_dev_get_module_base_VA_MMSYS_CONFIG()
#else
#include <mach/mt_reg_base.h>
#endif
#define CMDQ_TEST_MMSYS_DUMMY_PA     (0x14000000 + CMDQ_TEST_MMSYS_DUMMY_OFFSET)
#define CMDQ_TEST_MMSYS_DUMMY_VA     (MMSYS_CONFIG_BASE + CMDQ_TEST_MMSYS_DUMMY_OFFSET)

// test configuration
static DEFINE_MUTEX(gCmdqTestProcLock);
static int32_t gCmdqTestConfig[2] = {0, 0};  // {normal, secure}

extern unsigned long msleep_interruptible(unsigned int msecs);

static struct proc_dir_entry   *gCmdqTestProcEntry;

extern int32_t cmdq_core_suspend_HW_thread(int32_t thread);

extern int32_t cmdq_append_command(cmdqRecHandle  handle,
                                   CMDQ_CODE_ENUM code,
                                   uint32_t       argA,
                                   uint32_t       argB);
extern int32_t cmdq_rec_finalize_command(cmdqRecHandle handle, bool loop);

static int32_t _test_submit_async(cmdqRecHandle handle, TaskStruct **ppTask)
{
    cmdqCommandStruct desc = {
        .scenario = handle->scenario,
        .priority = handle->priority,
        .engineFlag = handle->engineFlag,
        .pVABase = handle->pBuffer,
        .blockSize = handle->blockSize,
    };

    return cmdqCoreSubmitTaskAsync(&desc,
                                   NULL,
                                   0,
                                   ppTask);
}


static void testcase_scenario(void)
{
	cmdqRecHandle hRec;
    int32_t ret;
    int i = 0;

    // make sure each scenario runs properly with empty commands
    for(i = 0; i < CMDQ_MAX_SCENARIO_COUNT; ++i)
    {
        if (i == CMDQ_SCENARIO_USER_SPACE)
        {
            continue;
        }

        CMDQ_MSG("testcase_scenario id:%d\n", i);
        cmdqRecCreate((CMDQ_SCENARIO_ENUM)i,&hRec);
    	cmdqRecReset(hRec);
        ret = cmdqRecFlush(hRec);
    	cmdqRecDestroy(hRec);
    }
    return;
}

static struct timer_list timer;

static void _testcase_sync_token_timer_func(unsigned long data)
{
    CMDQ_MSG("%s\n", __FUNCTION__);

    // trigger sync event
    CMDQ_MSG("trigger event=0x%08lx\n", (1L << 16) | data);
    CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | data);
}

static void testcase_sync_token(void)
{
    cmdqRecHandle hRec;
    int32_t ret = 0;;

    CMDQ_MSG("%s\n", __FUNCTION__);

    do {
        cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_MEMOUT, &hRec);
        cmdqRecReset(hRec);

        // setup timer to trigger sync token
        setup_timer(&timer, &_testcase_sync_token_timer_func, CMDQ_SYNC_TOKEN_USER_0);
        mod_timer(&timer, jiffies + msecs_to_jiffies(1000));

        // wait for sync token
        cmdqRecWait(hRec, CMDQ_SYNC_TOKEN_USER_0);

        CMDQ_MSG("start waiting\n");
        ret = cmdqRecFlush(hRec);
    	cmdqRecDestroy(hRec);
        CMDQ_MSG("waiting done\n");

        // clear token
        CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);
        del_timer(&timer);
    } while(0);

    CMDQ_MSG("%s, timeout case\n", __FUNCTION__);
    //
    // test for timeout
    //
    do {
        cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_MEMOUT, &hRec);
        cmdqRecReset(hRec);

        // wait for sync token
        cmdqRecWait(hRec, CMDQ_SYNC_TOKEN_USER_0);

        CMDQ_MSG("start waiting\n");
        ret = cmdqRecFlush(hRec);
    	cmdqRecDestroy(hRec);
        CMDQ_MSG("waiting done\n");

        // clear token
        CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

        BUG_ON(ret >= 0);
    } while(0);

    CMDQ_MSG("%s END\n", __FUNCTION__);
}

static struct timer_list timer_reqA;
static struct timer_list timer_reqB;

static void testcase_async_suspend_resume(void)
{
    cmdqRecHandle hReqA;
    TaskStruct *pTaskA;
    int32_t ret = 0;

    CMDQ_MSG("%s\n", __FUNCTION__);

    // setup timer to trigger sync token
    //setup_timer(&timer_reqA, &_testcase_sync_token_timer_func, CMDQ_SYNC_TOKEN_USER_0);
    //mod_timer(&timer_reqA, jiffies + msecs_to_jiffies(300));
    CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

    do {
        // let this thread wait for user token, then finish
        cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_MEMOUT,&hReqA);
        cmdqRecReset(hReqA);
        cmdqRecWait(hReqA, CMDQ_SYNC_TOKEN_USER_0);
        cmdq_append_command(hReqA, CMDQ_CODE_EOC, 0, 1);

        ret = _test_submit_async(hReqA, &pTaskA);

        CMDQ_MSG("%s start suspend+resume thread 0========\n", __FUNCTION__);
        cmdq_core_suspend_HW_thread(0);
        CMDQ_REG_SET32(CMDQ_THR_SUSPEND_TASK(0), 0x00);
        CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | CMDQ_SYNC_TOKEN_USER_0);

        msleep_interruptible(500);
        CMDQ_MSG("%s start wait A========\n", __FUNCTION__);
        ret = cmdqCoreWaitAndReleaseTask(pTaskA, 500);
    } while(0);

    // clear token
    CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);
    CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_1);
    cmdqRecDestroy(hReqA);
    //del_timer(&timer_reqA);

    CMDQ_MSG("%s END\n", __FUNCTION__);
}

static void testcase_errors(void)
{
    cmdqRecHandle hReq;
    cmdqRecHandle hLoop;
    TaskStruct *pTask;
    int32_t ret;
    const unsigned long MMSYS_DUMMY_REG = CMDQ_TEST_MMSYS_DUMMY_VA;
    const uint32_t UNKNOWN_OP = 0x50;
    uint32_t *pCommand;

    ret = 0 ;
    do {
        // SW timeout
        CMDQ_MSG("%s line:%d\n", __FUNCTION__, __LINE__);

        cmdqRecCreate(CMDQ_SCENARIO_TRIGGER_LOOP, &hLoop);
        cmdqRecReset(hLoop);
        cmdqRecPoll(hLoop, CMDQ_TEST_MMSYS_DUMMY_PA, 1, 0xFFFFFFFF);
        cmdqRecStartLoop(hLoop);

        CMDQ_MSG("=============== INIFINITE Wait ===================\n");

        CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_ENG_MDP_TDSHP0);
        cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_DISP, &hReq);

        // turn on ALL engine flag to test dump
        for (ret = 0; ret < CMDQ_MAX_ENGINE_COUNT; ++ret)
        {
            hReq->engineFlag |= 1LL << ret;
        }
        cmdqRecReset(hReq);
        cmdqRecWait(hReq, CMDQ_ENG_MDP_TDSHP0);
        cmdqRecFlush(hReq);

        CMDQ_MSG("=============== INIFINITE JUMP ===================\n");

        // HW timeout
        CMDQ_MSG("%s line:%d\n", __FUNCTION__, __LINE__);
        CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_ENG_MDP_TDSHP0);
        cmdqRecReset(hReq);
        cmdqRecWait(hReq, CMDQ_ENG_MDP_TDSHP0);
        cmdq_append_command(hReq, CMDQ_CODE_JUMP, 0, 8);    // JUMP to connect tasks
        ret = _test_submit_async(hReq, &pTask);
        msleep_interruptible(500);
        ret = cmdqCoreWaitAndReleaseTask(pTask, 8000);

        CMDQ_MSG("================POLL INIFINITE====================\n");

        CMDQ_MSG("testReg: %lx\n", MMSYS_DUMMY_REG);

        CMDQ_REG_SET32(MMSYS_DUMMY_REG, 0x0);
	    cmdqRecReset(hReq);
        cmdqRecPoll(hReq, CMDQ_TEST_MMSYS_DUMMY_PA, 1, 0xFFFFFFFF);
        cmdqRecFlush(hReq);

        CMDQ_MSG("=================INVALID INSTR=================\n");

        // invalid instruction
        CMDQ_MSG("%s line:%d\n", __FUNCTION__, __LINE__);
        cmdqRecReset(hReq);
        cmdq_append_command(hReq, CMDQ_CODE_JUMP, -1, 0);
        cmdqRecFlush(hReq);

        CMDQ_MSG("=================INVALID INSTR: UNKNOWN OP(0x%x)=================\n", UNKNOWN_OP);
        CMDQ_MSG("%s line:%d\n", __FUNCTION__, __LINE__);

        // invalid instruction is asserted when unkown OP
        cmdqRecReset(hReq);
        {
            pCommand = (uint32_t*)((uint8_t*)hReq->pBuffer + hReq->blockSize);
            *pCommand++ = 0x0;
            *pCommand++ = (UNKNOWN_OP << 24);
            hReq->blockSize += 8;
        }
        cmdqRecFlush(hReq);

    }while(0);

    cmdqRecDestroy(hReq);
    cmdqRecDestroy(hLoop);
    CMDQ_MSG("testcase_errors done\n");
    return;
}

static int32_t finishCallback(unsigned long data)
{
    CMDQ_ERR("callback() with data=0x%08lx\n", data);
    return 0;
}


static void testcase_fire_and_forget(void)
{
    cmdqRecHandle hReqA, hReqB;

    CMDQ_MSG("%s BEGIN\n", __FUNCTION__);
    do {
        cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &hReqA);
        cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &hReqB);
        cmdqRecReset(hReqA);
        cmdqRecReset(hReqB);

        CMDQ_MSG("%s %d\n", __FUNCTION__, __LINE__);
        cmdqRecFlushAsync(hReqA);
        CMDQ_MSG("%s %d\n", __FUNCTION__, __LINE__);
        cmdqRecFlushAsyncCallback(hReqB, finishCallback, 443);
        CMDQ_MSG("%s %d\n", __FUNCTION__, __LINE__);
    } while(0);

    cmdqRecDestroy(hReqA);
    cmdqRecDestroy(hReqB);

    CMDQ_MSG("%s END\n", __FUNCTION__);
}

static void testcase_async_request(void)
{
    cmdqRecHandle hReqA, hReqB;
    TaskStruct *pTaskA, *pTaskB;
    int32_t ret = 0;

    CMDQ_MSG("%s\n", __FUNCTION__);

    // setup timer to trigger sync token
    setup_timer(&timer_reqA, &_testcase_sync_token_timer_func, CMDQ_SYNC_TOKEN_USER_0);
    mod_timer(&timer_reqA, jiffies + msecs_to_jiffies(1000));

    setup_timer(&timer_reqB, &_testcase_sync_token_timer_func, CMDQ_SYNC_TOKEN_USER_1);
    // mod_timer(&timer_reqB, jiffies + msecs_to_jiffies(1300));

    CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);
    CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_1);

    do {
        cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_MEMOUT,&hReqA);
        cmdqRecReset(hReqA);
        cmdqRecWait(hReqA, CMDQ_SYNC_TOKEN_USER_0);
        cmdq_append_command(hReqA, CMDQ_CODE_EOC, 0, 1);
        cmdq_append_command(hReqA, CMDQ_CODE_JUMP,0, 8);    // JUMP to connect tasks

        cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_MEMOUT,&hReqB);
        cmdqRecReset(hReqB);
        cmdqRecWait(hReqB, CMDQ_SYNC_TOKEN_USER_1);
        cmdq_append_command(hReqB, CMDQ_CODE_EOC, 0, 1);
        cmdq_append_command(hReqB, CMDQ_CODE_JUMP,0, 8);    // JUMP to connect tasks

        ret = _test_submit_async(hReqA, &pTaskA);
        ret = _test_submit_async(hReqB, &pTaskB);

        CMDQ_MSG("%s start wait sleep========\n", __FUNCTION__);
        msleep_interruptible(500);

        CMDQ_MSG("%s start wait A========\n", __FUNCTION__);
        ret = cmdqCoreWaitAndReleaseTask(pTaskA, 500);

        CMDQ_MSG("%s start wait B, this should timeout========\n", __FUNCTION__);
        ret = cmdqCoreWaitAndReleaseTask(pTaskB, 600);
        CMDQ_MSG("%s wait B get %d ========\n", __FUNCTION__, ret);

    } while(0);

    // clear token
    CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);
    CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_1);
    cmdqRecDestroy(hReqA);
    cmdqRecDestroy(hReqB);
    del_timer(&timer_reqA);
    del_timer(&timer_reqB);

    CMDQ_MSG("%s END\n", __FUNCTION__);
}

static void testcase_multiple_async_request(void)
{
    #define TEST_REQ_COUNT 30
    cmdqRecHandle hReq[TEST_REQ_COUNT];
    TaskStruct *pTask[TEST_REQ_COUNT] = {0};
    int32_t ret = 0;
    int i;

    CMDQ_MSG("%s\n", __FUNCTION__);

    // setup timer to trigger sync token
    setup_timer(&timer_reqA, &_testcase_sync_token_timer_func, CMDQ_SYNC_TOKEN_USER_0);
    mod_timer(&timer_reqA, jiffies + msecs_to_jiffies(10));

    // Queue multiple async request
    // to test dynamic task allocation
    CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);
    for (i = 0; i < TEST_REQ_COUNT; ++i)
    {
        cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &hReq[i]);
        cmdqRecReset(hReq[i]);
        cmdqRecWait(hReq[i], CMDQ_SYNC_TOKEN_USER_0);
        cmdq_rec_finalize_command(hReq[i], false);

        // higher priority for later tasks
        hReq[i]->priority = i;

        ret = _test_submit_async(hReq[i], &pTask[i]);
    }

    // release token and wait them
    for (i = 0; i < TEST_REQ_COUNT; ++i)
    {
        #if 0
        mod_timer(&timer_reqA, jiffies + msecs_to_jiffies(10));
        msleep_interruptible(100);
        #else
        CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | CMDQ_SYNC_TOKEN_USER_0);
        #endif

        CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | CMDQ_SYNC_TOKEN_USER_0);

        CMDQ_MSG("wait 0x%p========\n", pTask[i]);
        ret = cmdqCoreWaitAndReleaseTask(pTask[i], 3000);
        cmdqRecDestroy(hReq[i]);
    }

    // clear token
    CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

    del_timer(&timer_reqA);

    CMDQ_MSG("%s END\n", __FUNCTION__);
}


static void testcase_async_request_partial_engine(void)
{
    int32_t ret = 0;
    int i;
    CMDQ_SCENARIO_ENUM scn[] = { CMDQ_SCENARIO_PRIMARY_DISP,
                                     CMDQ_SCENARIO_JPEG_DEC,
                                     CMDQ_SCENARIO_PRIMARY_MEMOUT,
                                     CMDQ_SCENARIO_SUB_DISP,
                                     CMDQ_SCENARIO_DEBUG,
                                   };

    struct timer_list timers[sizeof(scn)/sizeof(scn[0])];

    cmdqRecHandle hReq[(sizeof(scn) / sizeof(scn[0]))] = {0};
    TaskStruct *pTasks[(sizeof(scn) / sizeof(scn[0]))] = {0};

    CMDQ_MSG("%s\n", __FUNCTION__);

    // setup timer to trigger sync token
    for (i = 0; i < (sizeof(scn) / sizeof(scn[0])); ++i)
    {
        setup_timer(&timers[i], &_testcase_sync_token_timer_func, CMDQ_SYNC_TOKEN_USER_0 + i);
        mod_timer(&timers[i], jiffies + msecs_to_jiffies(400*(1+i)));
        CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0 + i);

        cmdqRecCreate(scn[i],&hReq[i]);
        cmdqRecReset(hReq[i]);
        cmdqRecWait(hReq[i], CMDQ_SYNC_TOKEN_USER_0 + i);
        cmdq_rec_finalize_command(hReq[i], false);

        CMDQ_MSG("TEST: SUBMIT scneario %d\n", scn[i]);
        ret = _test_submit_async(hReq[i], &pTasks[i]);
    }


    // wait for task completion
    for (i = 0; i < (sizeof(scn) / sizeof(scn[0])); ++i)
    {
        ret = cmdqCoreWaitAndReleaseTask(pTasks[i], msecs_to_jiffies(3000));
    }

    // clear token
    for(i = 0; i < (sizeof(scn) / sizeof(scn[0])); ++i)
    {
        CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0 + i);
        cmdqRecDestroy(hReq[i]);
        del_timer(&timers[i]);
    }

    CMDQ_MSG("%s END\n", __FUNCTION__);

}

static void _testcase_unlock_all_event_timer_func(unsigned long data)
{
    uint32_t token = 0;

    CMDQ_MSG("%s\n", __FUNCTION__);

    // trigger sync event
    CMDQ_MSG("trigger events\n");
    for (token = 0; token < CMDQ_SYNC_TOKEN_MAX; ++token)
    {
        //  3 threads waiting, so update 3 times
        CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | token);
        CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | token);
        CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | token);
    }
}

static void testcase_sync_token_threaded(void)
{
    CMDQ_SCENARIO_ENUM scn[] = { CMDQ_SCENARIO_PRIMARY_DISP,    // high prio
                                 CMDQ_SCENARIO_JPEG_DEC,        // normal prio
                                 CMDQ_SCENARIO_TRIGGER_LOOP     // normal prio
                               };
    int32_t ret = 0;
    int i = 0;
    uint32_t token = 0;
    struct timer_list eventTimer;
    cmdqRecHandle hReq[(sizeof(scn) / sizeof(scn[0]))] = {0};
    TaskStruct *pTasks[(sizeof(scn) / sizeof(scn[0]))] = {0};

    CMDQ_MSG("%s\n", __FUNCTION__);

    // setup timer to trigger sync token
    for (i = 0; i < (sizeof(scn) / sizeof(scn[0])); ++i)
    {
        setup_timer(&eventTimer, &_testcase_unlock_all_event_timer_func, 0);
        mod_timer(&eventTimer, jiffies + msecs_to_jiffies(500));

        //
        // 3 threads, all wait & clear 511 events
        //
        cmdqRecCreate(scn[i],&hReq[i]);
        cmdqRecReset(hReq[i]);
        for(token = 0; token < CMDQ_SYNC_TOKEN_MAX; ++token)
        {
            cmdqRecWait(hReq[i], (CMDQ_EVENT_ENUM)token);
        }
        cmdq_rec_finalize_command(hReq[i], false);

        CMDQ_MSG("TEST: SUBMIT scneario %d\n", scn[i]);
        ret = _test_submit_async(hReq[i], &pTasks[i]);
    }


    // wait for task completion
    msleep_interruptible(1000);
    for (i = 0; i < (sizeof(scn) / sizeof(scn[0])); ++i)
    {
        ret = cmdqCoreWaitAndReleaseTask(pTasks[i], msecs_to_jiffies(5000));
    }

    // clear token
    for(i = 0; i < (sizeof(scn) / sizeof(scn[0])); ++i)
    {
        cmdqRecDestroy(hReq[i]);
    }

    del_timer(&eventTimer);
    CMDQ_MSG("%s END\n", __FUNCTION__);
}

static struct timer_list g_loopTimer;
static int g_loopIter = 0;
static cmdqRecHandle hLoopReq;

static void _testcase_loop_timer_func(unsigned long data)
{
    CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | data);
    mod_timer(&g_loopTimer, jiffies + msecs_to_jiffies(300));
    g_loopIter++;
}

static void testcase_loop(void)
{
    int status = 0;
    cmdqRecCreate(CMDQ_SCENARIO_TRIGGER_LOOP,&hLoopReq);
    cmdqRecReset(hLoopReq);
    cmdqRecWait(hLoopReq, CMDQ_SYNC_TOKEN_USER_0);

    setup_timer(&g_loopTimer, &_testcase_loop_timer_func, CMDQ_SYNC_TOKEN_USER_0);
    mod_timer(&g_loopTimer, jiffies + msecs_to_jiffies(300));
    CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_TOKEN_USER_0);

    g_loopIter = 0;

    // should success
    status = cmdqRecStartLoop(hLoopReq);
    BUG_ON(status != 0);

    // should fail because already started
    CMDQ_MSG("============testcase_loop start loop\n");
    status = cmdqRecStartLoop(hLoopReq);
    BUG_ON(status >= 0);

    cmdqRecDumpCommand(hLoopReq);

    // WAIT
    while(g_loopIter < 20)
    {
        msleep_interruptible(2000);
    }
    msleep_interruptible(2000);

    CMDQ_MSG("============testcase_loop stop timer\n");
    cmdqRecDestroy(hLoopReq);
    del_timer(&g_loopTimer);
}

static unsigned long gLoopCount = 0L;
static void _testcase_trigger_func(unsigned long data)
{
    // trigger sync event
    CMDQ_MSG("_testcase_trigger_func");
    CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | CMDQ_SYNC_TOKEN_USER_0);
    CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | CMDQ_SYNC_TOKEN_USER_1);

    // start again
    mod_timer(&timer, jiffies + msecs_to_jiffies(1000));
    gLoopCount++;
}

/*
static void leave_loop_func(struct work_struct *w)
{
    CMDQ_MSG("leave_loop_func: cancel loop");
    cmdqRecStopLoop(hLoopConfig);
    hLoopConfig = NULL;
    return;
}

DECLARE_WORK(leave_loop, leave_loop_func);

int32_t my_irq_callback(unsigned long data)
{
    CMDQ_MSG("%s data=%d\n", __FUNCTION__, data);

    ++gLoopCount;

    switch(data)
    {
    case 1:
        if(gLoopCount < 20)
        {
            return 0;
        }
        else
        {
            return -1;
        }
        break;
    case 2:
        if(gLoopCount > 40)
        {
            // insert stopping cal
            schedule_work(&leave_loop);
        }
        break;
    }
    return 0;
}
*/

static void testcase_trigger_thread(void)
{
    cmdqRecHandle hTrigger, hConfig;
    int32_t ret = 0;
    int index = 0;

    CMDQ_MSG("%s\n", __FUNCTION__);

    // setup timer to trigger sync token for every 1 sec
    setup_timer(&timer, &_testcase_trigger_func, 0);
    mod_timer(&timer, jiffies + msecs_to_jiffies(1000));

    do {
        // THREAD 1, trigger loop
        cmdqRecCreate(CMDQ_SCENARIO_TRIGGER_LOOP, &hTrigger);
        cmdqRecReset(hTrigger);
        // * WAIT and CLEAR config dirty
        //cmdqRecWait(hTrigger, CMDQ_SYNC_TOKEN_CONFIG_DIRTY);

        // * WAIT and CLEAR TE
        //cmdqRecWait(hTrigger, CMDQ_EVENT_MDP_DSI0_TE_SOF);

        // * WAIT and CLEAR stream done
        //cmdqRecWait(hTrigger, CMDQ_EVENT_MUTEX0_STREAM_EOF);

        // * WRITE mutex enable
        //cmdqRecWait(hTrigger, MM_MUTEX_BASE + 0x20);

        cmdqRecWait(hTrigger, CMDQ_SYNC_TOKEN_USER_0);

        // * RUN forever but each IRQ trigger is bypass to my_irq_callback
    	ret = cmdqRecStartLoop(hTrigger);

        // THREAD 2, config thread
        cmdqRecCreate(CMDQ_SCENARIO_JPEG_DEC, &hConfig);


        hConfig->priority = CMDQ_THR_PRIO_NORMAL;
        cmdqRecReset(hConfig);
        // insert tons of instructions
        for (index = 0; index < 10; ++index)
        {
            cmdq_append_command(hConfig, CMDQ_CODE_MOVE, 0, 0x1);
        }
        ret = cmdqRecFlush(hConfig);
        CMDQ_MSG("flush 0\n");

        hConfig->priority = CMDQ_THR_PRIO_DISPLAY_CONFIG;
        cmdqRecReset(hConfig);
        // insert tons of instructions
        for (index = 0; index < 10; ++index)
        {
            cmdq_append_command(hConfig, CMDQ_CODE_MOVE, 0, 0x1);
        }
        ret = cmdqRecFlush(hConfig);
        CMDQ_MSG("flush 1\n");

        cmdqRecReset(hConfig);
        // insert tons of instructions
        for (index = 0; index < 500; ++index)
        {
            cmdq_append_command(hConfig, CMDQ_CODE_MOVE, 0, 0x1);
        }
        ret = cmdqRecFlush(hConfig);
        CMDQ_MSG("flush 2\n");

        // WAIT
        while(gLoopCount < 20)
        {
            msleep_interruptible(2000);
        }
    } while(0);

    del_timer(&timer);
    cmdqRecDestroy(hTrigger);
    cmdqRecDestroy(hConfig);

    CMDQ_MSG("%s END\n", __FUNCTION__);
}

static void testcase_prefetch_scenarios(void)
{
    // make sure both prefetch and non-prefetch cases
    // handle 248+ instructions properly
    cmdqRecHandle hConfig;
    int32_t ret = 0;
    int index = 0, scn = 0;
    const int INSTRUCTION_COUNT = 500;
    CMDQ_MSG("%s\n", __FUNCTION__);

    // make sure each scenario runs properly with 248+ commands
    for(scn = 0; scn < CMDQ_MAX_SCENARIO_COUNT; ++scn)
    {
        if (scn == CMDQ_SCENARIO_USER_SPACE)
        {
            continue;
        }

        CMDQ_MSG("testcase_prefetch_scenarios scenario:%d\n", scn);
        cmdqRecCreate((CMDQ_SCENARIO_ENUM)scn,&hConfig);
    	cmdqRecReset(hConfig);
        // insert tons of instructions
        for (index = 0; index < INSTRUCTION_COUNT; ++index)
        {
            cmdq_append_command(hConfig, CMDQ_CODE_MOVE, 0, 0x1);
        }

        ret = cmdqRecFlush(hConfig);
        BUG_ON(ret < 0);
    	cmdqRecDestroy(hConfig);
    }
    CMDQ_MSG("%s END\n", __FUNCTION__);
}

extern void cmdq_core_reset_hw_events(void);

static void testcase_clkmgr(void)
{
    uint32_t value = 0;

    CMDQ_MSG("testcase_clkmgr()\n");
    // turn on CLK, function should work

    CMDQ_MSG("testcase_clkmgr() enable_clock\n");
    enable_clock(MT_CG_INFRA_GCE, "CMDQ_TEST");
    cmdq_core_reset_hw_events();

    CMDQ_REG_SET32(CMDQ_GPR_R32(CMDQ_DATA_REG_DEBUG), 0xFFFFDEAD);
    value = CMDQ_REG_GET32(CMDQ_GPR_R32(CMDQ_DATA_REG_DEBUG));
    if (value != 0xFFFFDEAD)
    {
        CMDQ_ERR("when enable clock CMDQ_DATA_REG_DEBUG = 0x%08x\n", value);
        BUG();
    }

    // turn off CLK, function should not work and access register should not cause hang
    CMDQ_MSG("testcase_clkmgr() disable_clock\n");
    disable_clock(MT_CG_INFRA_GCE, "CMDQ_TEST");

    CMDQ_REG_SET32(CMDQ_GPR_R32(CMDQ_DATA_REG_DEBUG), 0xDEADDEAD);
    value = CMDQ_REG_GET32(CMDQ_GPR_R32(CMDQ_DATA_REG_DEBUG));
    if (value != 0)
    {
        CMDQ_ERR("when disable clock CMDQ_DATA_REG_DEBUG = 0x%08x\n", value);
        BUG();
    }

}

static void testcase_dram_access(void)
{
    cmdqRecHandle handle;
    uint32_t    *regResults;
    dma_addr_t  regResultsMVA;
    dma_addr_t dstMVA;
    uint32_t argA;
    uint32_t subsysCode;
    uint32_t *pCmdEnd = NULL;
    unsigned long long data64;

    CMDQ_MSG("testcase_dram_access\n");

    regResults = dma_alloc_coherent(cmdq_dev_get(),
                                    sizeof(uint32_t) * 2,
                                    &regResultsMVA,
                                    GFP_KERNEL);


    // set up intput
    regResults[0] = 0xdeaddead;     // this is read-from
    regResults[1] = 0xffffffff;     // this is write-to

    cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
    cmdqRecReset(handle);

    //
    // READ from DRAME: register to read from
    //
    // note that we force convert to physical reg address.
    // if it is already physical address, it won't be affected (at least on this platform)
    argA = CMDQ_TEST_MMSYS_DUMMY_PA;
    subsysCode = cmdq_subsys_from_phys_addr(argA);

    pCmdEnd = (uint32_t*)(((char*)handle->pBuffer) + handle->blockSize);

    CMDQ_MSG("pCmdEnd initial=0x%p, reg MVA=%pa, size=%d\n",
        pCmdEnd,
        &regResultsMVA,
        handle->blockSize);

    // Move &(regResults[0]) to CMDQ_DATA_REG_DEBUG_DST
    *pCmdEnd = (uint32_t)CMDQ_PHYS_TO_AREG(regResultsMVA);
    pCmdEnd += 1;
    *pCmdEnd = (CMDQ_CODE_MOVE << 24) |
                #ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
                ((regResultsMVA >> 32) & 0xffff) |
                #endif
                ((CMDQ_DATA_REG_DEBUG_DST & 0x1f) << 16) |
                (4 << 21);
    pCmdEnd += 1;

    //
    // WRITE to DRAME:
    // from src_addr(CMDQ_DATA_REG_DEBUG_DST) to external RAM (regResults[1])
    //

    // Read data from *CMDQ_DATA_REG_DEBUG_DST to CMDQ_DATA_REG_DEBUG
    *pCmdEnd = CMDQ_DATA_REG_DEBUG;
    pCmdEnd += 1;
    *pCmdEnd = (CMDQ_CODE_READ << 24) | (0 & 0xffff) | ((CMDQ_DATA_REG_DEBUG_DST & 0x1f)<< 16) | (6 << 21); //1 1 0
    pCmdEnd += 1;

    // Load ddst_addr to GPR: Move &(regResults[1]) to CMDQ_DATA_REG_DEBUG_DST
    dstMVA = regResultsMVA + 4; // note regResults is a uint32_t array
    *pCmdEnd = ((uint32_t)dstMVA);
    pCmdEnd += 1;
    *pCmdEnd = (CMDQ_CODE_MOVE << 24) |
                #ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
                ((dstMVA >> 32) & 0xffff) |
                #endif
                ((CMDQ_DATA_REG_DEBUG_DST & 0x1f) << 16) |
                (4 << 21);
    pCmdEnd += 1;

    // Write from CMDQ_DATA_REG_DEBUG to *CMDQ_DATA_REG_DEBUG_DST
    *pCmdEnd = CMDQ_DATA_REG_DEBUG;
    pCmdEnd += 1;
    *pCmdEnd = (CMDQ_CODE_WRITE << 24) | (0 & 0xffff) | ((CMDQ_DATA_REG_DEBUG_DST & 0x1f)<< 16) | (6 << 21); //1 1 0
    pCmdEnd += 1;

    handle->blockSize += 4 * 8;     // 4 * 64-bit instructions

    cmdqRecDumpCommand(handle);

    cmdqRecFlush(handle);

    cmdqRecDumpCommand(handle);

    cmdqRecDestroy(handle);

    data64 = 0LL;
    data64 = CMDQ_REG_GET64_GPR_PX(CMDQ_DATA_REG_DEBUG_DST);

    CMDQ_MSG("regResults=[0x%08x, 0x%08x]\n", regResults[0], regResults[1]);
    CMDQ_MSG("CMDQ_DATA_REG_DEBUG=0x%08x, CMDQ_DATA_REG_DEBUG_DST=0x%llx\n",
             CMDQ_REG_GET32(CMDQ_GPR_R32(CMDQ_DATA_REG_DEBUG)),
             data64);

    if (regResults[1] != regResults[0])
    {
        CMDQ_ERR("ERROR!!!!!!\n");
    }
    else
    {
        CMDQ_MSG("OK!!!!!!\n");
    }

    dma_free_coherent(cmdq_dev_get(),
                      2 * sizeof(uint32_t),
                      regResults,
                      regResultsMVA);

}

static void testcase_long_command(void)
{
    int i;
    cmdqRecHandle handle;
    uint32_t data;
    const unsigned long MMSYS_DUMMY_REG = CMDQ_TEST_MMSYS_DUMMY_VA;

    CMDQ_MSG("testcase_long_command\n");

    CMDQ_REG_SET32(MMSYS_DUMMY_REG, 0xdeaddead);

    cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
	cmdqRecReset(handle);
    // build a 64KB instruction buffer
    for (i = 0; i < 64 * 1024 / 8; ++i)
    {
        cmdqRecReadToDataRegister(handle, CMDQ_TEST_MMSYS_DUMMY_PA, CMDQ_DATA_REG_PQ_COLOR);
    }
    cmdqRecFlush(handle);
	cmdqRecDestroy(handle);

    // verify data
    data = CMDQ_REG_GET32(CMDQ_GPR_R32(CMDQ_DATA_REG_PQ_COLOR));
    if (data != 0xdeaddead)
    {
        CMDQ_ERR("TEST FAIL: reg value is 0x%08x\n", data);
    }
    return;
}

static void testcase_perisys_apb(void)
{
    // write value to PERISYS register
    // we use MSDC debug to test:
    // write SEL, read OUT.

    const uint32_t MSDC_SW_DBG_SEL_PA = 0x11230000 + 0xA0;
    const uint32_t MSDC_SW_DBG_OUT_PA = 0x11230000 + 0xA4;
    const uint32_t AUDIO_TOP_CONF0_PA = 0x11220000;

#ifdef CMDQ_OF_SUPPORT
    const unsigned long MSDC_VA_BASE= cmdq_dev_alloc_module_base_VA_by_name("mediatek,MSDC0");
    const unsigned long AUDIO_VA_BASE = cmdq_dev_alloc_module_base_VA_by_name("mediatek,AUDIO");
    const unsigned long MSDC_SW_DBG_OUT = MSDC_VA_BASE + 0xA4;
    const unsigned long AUDIO_TOP_CONF0 = AUDIO_VA_BASE;

    CMDQ_LOG("MSDC_VA_BASE:  VA:%lx, PA: 0x%08x\n", MSDC_VA_BASE, 0x11230000);
    CMDQ_LOG("AUDIO_VA_BASE: VA:%lx, PA: 0x%08x\n", AUDIO_TOP_CONF0_PA,0x11220000);
#else
    const uint32_t MSDC_SW_DBG_OUT = 0xF1230000 + 0xA4;
    const uint32_t AUDIO_TOP_CONF0 = 0xF1220000;
#endif

    const uint32_t AUDIO_TOP_MASK  = ~0 & ~(1 << 28 |
                                    1 << 21 |
                                    1 << 17 |
                                    1 << 16 |
                                    1 << 15 |
                                    1 << 11 |
                                    1 << 10 |
                                    1 << 7  |
                                    1 << 5  |
                                    1 << 4  |
                                    1 << 3  |
                                    1 << 1  |
                                    1 << 0);
    cmdqRecHandle handle = NULL;
    uint32_t data = 0;
    uint32_t dataRead = 0;

    CMDQ_MSG("testcase_perisys_apb\n");
    cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);

	cmdqRecReset(handle);
    cmdqRecWrite(handle,
                 MSDC_SW_DBG_SEL_PA,
                 1,
                 ~0);
    cmdqRecFlush(handle);
    // verify data
    data = CMDQ_REG_GET32(MSDC_SW_DBG_OUT);
    CMDQ_MSG("MSDC_SW_DBG_OUT = 0x%08x=====\n", data);

    // test read from AP_DMA_GLOBAL_SLOW_DOWN to CMDQ GPR
    cmdqRecReset(handle);
    cmdqRecReadToDataRegister(handle,
                              MSDC_SW_DBG_OUT_PA,
                              CMDQ_DATA_REG_PQ_COLOR);
    cmdqRecFlush(handle);
    // verify data
    dataRead = CMDQ_REG_GET32(CMDQ_GPR_R32(CMDQ_DATA_REG_PQ_COLOR));
    if (data != dataRead)
    {
        CMDQ_ERR("TEST FAIL: CMDQ_DATA_REG_PQ_COLOR is 0x%08x, different=====\n", dataRead);
    }


    CMDQ_REG_SET32(AUDIO_TOP_CONF0, ~0);
    data = CMDQ_REG_GET32(AUDIO_TOP_CONF0);
    CMDQ_MSG("write 0xFFFFFFFF to AUDIO_TOP_CONF0 = 0x%08x=====\n", data);
    CMDQ_REG_SET32(AUDIO_TOP_CONF0, 0);
    data = CMDQ_REG_GET32(AUDIO_TOP_CONF0);
    CMDQ_MSG("Before AUDIO_TOP_CONF0 = 0x%08x=====\n", data);
    cmdqRecReset(handle);
    cmdqRecWrite(handle,
                 AUDIO_TOP_CONF0_PA,
                 ~0,
                 AUDIO_TOP_MASK);
    cmdqRecFlush(handle);
    // verify data
    data = CMDQ_REG_GET32(AUDIO_TOP_CONF0);
    CMDQ_MSG("after AUDIO_TOP_CONF0 = 0x%08x=====\n", data);
    if (data != AUDIO_TOP_MASK)
    {
        CMDQ_ERR("TEST FAIL: AUDIO_TOP_CONF0 is 0x%08x=====\n", data);
    }

    cmdqRecDestroy(handle);

#ifdef CMDQ_OF_SUPPORT
    // release registers map
    cmdq_dev_free_module_base_VA(MSDC_VA_BASE);
    cmdq_dev_free_module_base_VA(AUDIO_VA_BASE);
#endif

    return;
}

static void testcase_write_address(void)
{
    dma_addr_t pa = 0;
    uint32_t value = 0;

    cmdqCoreAllocWriteAddress(3, &pa);
    CMDQ_LOG("ALLOC: 0x%pa\n", &pa);
    value = cmdqCoreReadWriteAddress(pa);
    CMDQ_LOG("value 0: 0x%08x\n", value);
    value = cmdqCoreReadWriteAddress(pa+1);
    CMDQ_LOG("value 1: 0x%08x\n", value);
    value = cmdqCoreReadWriteAddress(pa+2);
    CMDQ_LOG("value 2: 0x%08x\n", value);
    value = cmdqCoreReadWriteAddress(pa+3);
    CMDQ_LOG("value 3: 0x%08x\n", value);
    value = cmdqCoreReadWriteAddress(pa+4);
    CMDQ_LOG("value 4: 0x%08x\n", value);

    value = cmdqCoreReadWriteAddress(pa+(4*20));
    CMDQ_LOG("value 80: 0x%08x\n", value);

    cmdqCoreFreeWriteAddress(pa);
    cmdqCoreFreeWriteAddress(0);
}

static void testcase_read_to_data_reg(void)
{
    cmdqRecHandle handle;
    uint32_t data;
    unsigned long long data64;
    unsigned long MMSYS_DUMMY_REG = CMDQ_TEST_MMSYS_DUMMY_VA;

    CMDQ_MSG("testcase_read_to_data_reg\n");

    cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
	cmdqRecReset(handle);

    CMDQ_REG_SET32(MMSYS_DUMMY_REG, 0xdeaddead);
    CMDQ_REG_SET32(CMDQ_GPR_R32(CMDQ_DATA_REG_PQ_COLOR), 0xbeefbeef);
    // move data from GPR to GPR_Px: COLOR to COLOR_DST (64 bit)
    cmdqRecReadToDataRegister(handle, CMDQ_GPR_R32_PA(CMDQ_DATA_REG_PQ_COLOR), CMDQ_DATA_REG_PQ_COLOR_DST);
    // move data from register value to GPR_Rx: MM_DUMMY_REG to COLOR(32 bit)
    cmdqRecReadToDataRegister(handle, CMDQ_TEST_MMSYS_DUMMY_PA, CMDQ_DATA_REG_PQ_COLOR);

    cmdqRecFlush(handle);

	cmdqRecDestroy(handle);

    // verify data
    data = CMDQ_REG_GET32(CMDQ_GPR_R32(CMDQ_DATA_REG_PQ_COLOR));
    if (data != 0xdeaddead)
    {
        CMDQ_ERR("[Read from GPR_Rx]TEST FAIL: PQ reg value is 0x%08x\n", data);
    }

    data64 = 0LL;
    data64 = CMDQ_REG_GET64_GPR_PX(CMDQ_DATA_REG_PQ_COLOR_DST);
    if(0xbeefbeef != data64)
    {
        CMDQ_ERR("[Read from GPR_Px]TEST FAIL: PQ_DST reg value is 0x%llx\n", data64);
    }
    return;
}

static void testcase_backup_reg_to_slot(void)
{
    cmdqRecHandle handle;
    unsigned long MMSYS_DUMMY_REG = CMDQ_TEST_MMSYS_DUMMY_VA;
    cmdqBackupSlotHandle hSlot = 0;
    int i;
    uint32_t value = 0;

    CMDQ_LOG("testcase_read_to_data_reg\n");

    CMDQ_REG_SET32(MMSYS_DUMMY_REG, 0xdeaddead);

    // Create cmdqRec
    cmdqRecCreate(CMDQ_SCENARIO_DISP_ESD_CHECK, &handle);
    // Create Slot
    cmdqBackupAllocateSlot(&hSlot, 5);

    for (i = 0; i < 5; ++i)
    {
        cmdqBackupWriteSlot(hSlot, i, i);
    }

    for (i = 0; i < 5; ++i)
    {
        cmdqBackupReadSlot(hSlot, i, &value);
        if (value != i)
        {
            CMDQ_ERR("testcase_cmdqBackupWriteSlot FAILED!!!!!\n");
        }
        CMDQ_LOG("testcase_cmdqBackupWriteSlot OK!!!!!\n");
    }

    // Reset command buffer
	cmdqRecReset(handle);

    // Insert commands to backup registers
    for (i = 0; i < 5; ++i)
    {
        cmdqRecBackupRegisterToSlot(handle, hSlot, i, CMDQ_TEST_MMSYS_DUMMY_PA);
    }

    // Execute commands
    cmdqRecFlush(handle);

    // debug dump command instructions
    cmdqRecDumpCommand(handle);

    // we can destroy cmdqRec handle after flush.
	cmdqRecDestroy(handle);

    // verify data by reading it back from slot
    for (i = 0; i < 5; ++i)
    {
        cmdqBackupReadSlot(hSlot, i, &value);
        CMDQ_LOG("backup slot %d = 0x%08x\n", i, value);

        if (value != 0xdeaddead)
        {
            CMDQ_ERR("content error!!!!!!!!!!!!!!!!!!!!\n");
        }
    }

    // release result free slot
    cmdqBackupFreeSlot(hSlot);

    return;
}

static void testcase_poll(void)
{
    cmdqRecHandle handle;
    const unsigned long MMSYS_DUMMY_REG = CMDQ_TEST_MMSYS_DUMMY_VA;

    uint32_t value = 0;
    uint32_t testReg = MMSYS_DUMMY_REG;
    uint32_t pollingVal = 0x00003001;

    CMDQ_MSG("testcase_poll\n");

    CMDQ_REG_SET32(MMSYS_DUMMY_REG, ~0);

    // it's too slow that set value after enable CMDQ
    // sw timeout will be hanppened before CPU schedule to set value..., so we set value here
    CMDQ_REG_SET32(MMSYS_DUMMY_REG, pollingVal);
    value = CMDQ_REG_GET32(testReg);
    CMDQ_MSG("target value is 0x%08x\n", value);

    cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
	cmdqRecReset(handle);

    cmdqRecPoll(handle, CMDQ_TEST_MMSYS_DUMMY_PA, pollingVal, ~0);

    cmdqRecFlush(handle);
    cmdqRecDestroy(handle);

    // value check
    value = CMDQ_REG_GET32(testReg);
    if(pollingVal != value)
    {
        CMDQ_ERR("polling target value is 0x%08x\n", value);
    }
}

static void testcase_write(void)
{
    cmdqRecHandle handle;
    const unsigned long MMSYS_DUMMY_REG = CMDQ_TEST_MMSYS_DUMMY_VA;
    const uint32_t PATTERN = (1<<0) | (1<<2) | (1<<16); //0xDEADDEAD;

    uint32_t value = 0;

    // set to 0xFFFFFFFF
    CMDQ_REG_SET32(MMSYS_DUMMY_REG, ~0);

    // use CMDQ to set to PATTERN
    cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
	cmdqRecReset(handle);
    cmdqRecWrite(handle, CMDQ_TEST_MMSYS_DUMMY_PA, PATTERN, ~0);
    cmdqRecFlush(handle);
    cmdqRecDestroy(handle);

    // value check
    value = CMDQ_REG_GET32(MMSYS_DUMMY_REG);
    if (value != PATTERN)
    {
        CMDQ_ERR("TEST FAIL: wrote value is 0x%08x, not 0x%08x\n", value, PATTERN);
    }
}

static void testcase_prefetch(void)
{
    cmdqRecHandle handle;
    const uint32_t MMSYS_DUMMY_REG = MMSYS_CONFIG_BASE + 0x0890;
    // const uint32_t DSI_REG = 0xF401b000;
    const uint32_t PATTERN = (1<<0) | (1<<2) | (1<<16); //0xDEADDEAD;
    int i;
    uint32_t value = 0;
    uint32_t testReg = MMSYS_DUMMY_REG;
    const uint32_t REP_COUNT = 500;

    // set to 0xFFFFFFFF
    CMDQ_REG_SET32(MMSYS_DUMMY_REG, ~0);

    // No prefetch.
    // use CMDQ to set to PATTERN
    cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
	cmdqRecReset(handle);
    for (i = 0; i < REP_COUNT; ++i)
    {
        cmdqRecWrite(handle, IO_VIRT_TO_PHYS(testReg), PATTERN, ~0);
    }
    cmdqRecFlushAsync(handle);
    cmdqRecFlushAsync(handle);
    cmdqRecFlushAsync(handle);
    msleep_interruptible(1000);
    cmdqRecDestroy(handle);

    // use prefetch
    cmdqRecCreate(CMDQ_SCENARIO_DEBUG_PREFETCH, &handle);
	cmdqRecReset(handle);
    for (i = 0; i < REP_COUNT; ++i)
    {
        cmdqRecWrite(handle, IO_VIRT_TO_PHYS(testReg), PATTERN, ~0);
    }
    cmdqRecFlushAsync(handle);
    cmdqRecFlushAsync(handle);
    cmdqRecFlushAsync(handle);
    msleep_interruptible(1000);
    cmdqRecDestroy(handle);

    // value check
    value = CMDQ_REG_GET32(testReg);
    if (value != PATTERN)
    {
        CMDQ_ERR("TEST FAIL: wrote value is 0x%08x, not 0x%08x\n", value, PATTERN);
    }
}

static void testcase_backup_register(void)
{
    const unsigned long MMSYS_DUMMY_REG = CMDQ_TEST_MMSYS_DUMMY_VA;
    cmdqRecHandle handle;
    int ret = 0;
    uint32_t regAddr[3] = {CMDQ_TEST_MMSYS_DUMMY_PA,
                           CMDQ_GPR_R32_PA(CMDQ_DATA_REG_PQ_COLOR),
                           CMDQ_GPR_R32_PA(CMDQ_DATA_REG_2D_SHARPNESS_0)
                          };
    uint32_t regValue[3] = {0};

    CMDQ_REG_SET32(MMSYS_DUMMY_REG, 0xAAAAAAAA);
    CMDQ_REG_SET32(CMDQ_GPR_R32(CMDQ_DATA_REG_PQ_COLOR), 0xBBBBBBBB);
    CMDQ_REG_SET32(CMDQ_GPR_R32(CMDQ_DATA_REG_2D_SHARPNESS_0), 0xCCCCCCCC);


    cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
    cmdqRecReset(handle);
    ret = cmdqRecFlushAndReadRegister(handle, 3, regAddr, regValue);
    cmdqRecDestroy(handle);

    if (regValue[0] != 0xAAAAAAAA)
    {
        CMDQ_ERR("regValue[0] is 0x%08x, wrong!\n", regValue[0]);
    }
    if (regValue[1] != 0xBBBBBBBB)
    {
        CMDQ_ERR("regValue[1] is 0x%08x, wrong!\n", regValue[1]);
    }
    if (regValue[2] != 0xCCCCCCCC)
    {
        CMDQ_ERR("regValue[2] is 0x%08x, wrong!\n", regValue[2]);
    }
}

static void testcase_get_result(void)
{
    const unsigned long MMSYS_DUMMY_REG = CMDQ_TEST_MMSYS_DUMMY_VA;
    int i;
    cmdqRecHandle handle;
    int ret = 0;
    cmdqCommandStruct desc;

    int registers[1] = {CMDQ_TEST_MMSYS_DUMMY_PA};
    int result[1] = {0};

    CMDQ_MSG("testcase_get_result\n");

    // make sure each scenario runs properly with empty commands
    // use CMDQ_SCENARIO_PRIMARY_ALL to test
    // because it has COLOR0 HW flag
    cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_ALL,&handle);
	cmdqRecReset(handle);

    // insert dummy commands
    cmdq_rec_finalize_command(handle, false);

    // init desc attributes after finalize command to ensure correct size and buffer addr
    desc.scenario = handle->scenario;
    desc.priority = handle->priority;
    desc.engineFlag = handle->engineFlag;
    desc.pVABase = handle->pBuffer;
    desc.blockSize = handle->blockSize;

    desc.regRequest.count = 1;
    desc.regRequest.regAddresses = registers;
    desc.regValue.count = 1;
    desc.regValue.regValues = result;
    CMDQ_REG_SET32(MMSYS_DUMMY_REG, 0xdeaddead);

    // manually raise the dirty flag
    CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | CMDQ_EVENT_MUTEX0_STREAM_EOF);
    CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | CMDQ_EVENT_MUTEX1_STREAM_EOF);
    CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | CMDQ_EVENT_MUTEX2_STREAM_EOF);
    CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | CMDQ_EVENT_MUTEX3_STREAM_EOF);

    for(i = 0; i < 1; ++i)
    {
        ret = cmdqCoreSubmitTask(&desc);
        if (desc.regValue.regValues[0] != 0xdeaddead)
        {
            CMDQ_ERR("TEST FAIL: reg value is 0x%08x\n", desc.regValue.regValues[0]);
        }
    }

	cmdqRecDestroy(handle);
    return;
}

static void _testcase_thread_dispatch(void* data)
{
    cmdqRecHandle handle;
    long long engineFlag;
    int32_t i; 

    engineFlag = *((long long*)data);
    
    cmdqRecCreate(CMDQ_SCENARIO_DEBUG, &handle);
    for(i == 0; i < 1000; i++)
    {
        CMDQ_LOG("pid: %d, flush:%4d, engineFlag:0x%llx\n", current->pid, i, engineFlag);
        cmdqRecReset(handle);
        handle->engineFlag = engineFlag;
        cmdqRecFlush(handle);
    }
    cmdqRecDestroy(handle);    
}

static void testcase_thread_dispatch(void)
{
    char threadName[20]; 
    const long long  engineFlag1 = (0x1 << CMDQ_ENG_ISP_IMGI) | (0x1 << CMDQ_ENG_ISP_IMGO);
    const long long  engineFlag2 = (0x1 << CMDQ_ENG_MDP_RDMA0) | (0x1 << CMDQ_ENG_MDP_WDMA);
        
    CMDQ_MSG("=============== 2 THREAD with different engines ===============\n");

    sprintf(threadName, "cmdqKTHR_%llx", engineFlag1);
    struct task_struct *pKThread1 = kthread_run(_testcase_thread_dispatch, &engineFlag1, threadName);
    if(IS_ERR(pKThread1))
    {
        CMDQ_ERR("create thread failed, thread:%s\n", threadName);
        return;
    }
  
    sprintf(threadName, "cmdqKTHR_%llx", engineFlag2);
    struct task_struct *pKThread2 = kthread_run(_testcase_thread_dispatch, &engineFlag2, threadName);
    if(IS_ERR(pKThread2))
    {
        CMDQ_ERR("create thread failed, thread:%s\n", threadName);
        return;
    }

    msleep_interruptible(5*1000);
    return;
}

#if 0
static void testcase_DSI_command_mode(void)
{

	int32_t status = 0;
	cmdqRecHandle hTrigger;
	cmdqRecHandle hSetting;

    TaskStruct *pTask = NULL;

	//
	// build the trigger thread (normal priority)
	//
	// WRITE to query TE signal
	// WFE wait for TE(CMDQ_EVENT_MDP_DSI0_TE_SOF)
	// WFE wait stream done (CMDQ_EVENT_MUTEX0_STREAM_EOF? TODO: which stream done??
	//     and clear  (CMDQ_EVENT_MUTEX0_STREAM_EOF)
	// WRITE enable mutex to start engine (DISP_MUTEX0_EN in disp_mutex_coda.xls)
	// EOC (to issue interrupt, may need callback??? why need this one???)
	// JUMP to loopback to start

	// TODO: use a special repeat mode in cmdqRecRepeatFlush()
	cmdqRecCreate(CMDQ_SCENARIO_JPEG_DEC, &hTrigger);	// use CMDQ_SCENARIO_JPEG_DEC just to use another thread
	// cmdqRecWrite(hTrigger,


	//
	// build the setting thread (high priority)
	//
	cmdqRecCreate(CMDQ_SCENARIO_PRIMARY_MEMOUT, &hSetting);
	cmdqRecWait(hSetting, CMDQ_EVENT_MUTEX0_STREAM_EOF);
	cmdqRecMark(hSetting);
	int i = 0;
	for (i = 0; i < 200; ++i)
	{
		// increase cmd count but do not raise irq
		cmdq_append_command(hSetting, CMDQ_CODE_EOC, 0, 0);
	}
	cmdqRecMark(hSetting);
	// Jump back to head
	cmdq_append_command(hSetting,
						CMDQ_CODE_JUMP,
						0, 			// bit 32: is_absolute
						-hSetting->blockSize);
	hSetting->priority = 1;


	// Both are inifinte loop
	// so we call async then sync

	ret = _test_submit_async(hTrigger, &pTask);
	status = cmdqCoreSubmitTask(hSetting->scenario,
                                hSetting->priority,
	                            hSetting->engineFlag,
		                        hSetting->pBuffer,
	    	                    hSetting->blockSize);
	return;
}
#endif

typedef enum CMDQ_TESTCASE_ENUM
{
    CMDQ_TESTCASE_ALL     = 0,
    CMDQ_TESTCASE_BASIC   = 1,
    CMDQ_TESTCASE_ERROR   = 2,
    CMDQ_TESTCASE_READ_REG_REQUEST, //user request get some registers' value when task execution
    CMDQ_TESTCASE_GPR,
    CMDQ_TESTCASE_SW_TIMEOUT_HANDLE,

    CMDQ_TESTCASE_END,    // always at the end
}CMDQ_TESTCASE_ENUM;

ssize_t cmdq_test_proc(struct file * fp, char __user * u, size_t s, loff_t * l)
{
    uint32_t testId = 0;

    mutex_lock(&gCmdqTestProcLock);
    smp_mb();

    CMDQ_LOG("[TESTCASE]CONFIG: normal: %d, secure: %d\n", gCmdqTestConfig[0], gCmdqTestConfig[1]);
    testId  = gCmdqTestConfig[0];
    mutex_unlock(&gCmdqTestProcLock);

	// trigger test case here
	CMDQ_MSG("//\n//\n//\ncmdq_test_proc\n");

    // unconditionally set CMDQ_SYNC_TOKEN_CONFIG_ALLOW and mutex STREAM_DONE
    // so that DISPSYS scenarios may pass check.
    cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_STREAM_EOF);
    cmdqCoreSetEvent(CMDQ_EVENT_MUTEX0_STREAM_EOF);
    cmdqCoreSetEvent(CMDQ_EVENT_MUTEX1_STREAM_EOF);
    cmdqCoreSetEvent(CMDQ_EVENT_MUTEX2_STREAM_EOF);
    cmdqCoreSetEvent(CMDQ_EVENT_MUTEX3_STREAM_EOF);

    switch(gCmdqTestConfig[0])
    {
    case 99:
        testcase_write();
        break;
    case 98:
        testcase_errors();
        break;
    case 97:
        testcase_scenario();
        break;
    case 96:
        testcase_sync_token();
        break;
    case 95:
        testcase_write_address();
        break;
    case 94:
        testcase_async_request();
        break;
    case 93:
        testcase_async_suspend_resume();
        break;
    case 92:
        testcase_async_request_partial_engine();
        break;
    case 91:
        testcase_prefetch_scenarios();
        break;
    case 90:
        testcase_loop();
        break;
    case 89:
        testcase_trigger_thread();
        break;
    case 88:
        testcase_multiple_async_request();
        break;
    case 87:
        testcase_get_result();
        break;
    case 86:
        testcase_read_to_data_reg();
        break;
    case 85:
        testcase_dram_access();
        break;
    case 84:
        testcase_backup_register();
        break;
    case 83:
        testcase_fire_and_forget();
        break;
    case 82:
        testcase_sync_token_threaded();
        break;
    case 81:
        testcase_long_command();
        break;
    case 80:
        testcase_clkmgr();
        break;
    case 79:
        testcase_perisys_apb();
        break;
    case 78:
        testcase_backup_reg_to_slot();
        break;
    case 77: 
        testcase_thread_dispatch(); 
        break; 
    case CMDQ_TESTCASE_ERROR:
        testcase_errors();
        break;
    case CMDQ_TESTCASE_BASIC:
        testcase_write();
        testcase_poll();
        testcase_scenario();
        break;
    case CMDQ_TESTCASE_READ_REG_REQUEST:
        testcase_get_result();
        break;
    case CMDQ_TESTCASE_GPR:
        testcase_read_to_data_reg();    // must verify!
        testcase_dram_access();
        break;
    case CMDQ_TESTCASE_ALL:
    default:
        testcase_multiple_async_request();
        testcase_read_to_data_reg();
        testcase_get_result();
        testcase_errors();

        testcase_scenario();
        testcase_sync_token();

        testcase_write();
        testcase_write_address();
        testcase_async_request();
        testcase_async_suspend_resume();
        testcase_errors();
        testcase_async_request_partial_engine();
        testcase_prefetch_scenarios();
        testcase_loop();
        testcase_trigger_thread();
        testcase_prefetch();

        testcase_multiple_async_request();
    //    testcase_sync_token_threaded();

        testcase_read_to_data_reg();
        testcase_get_result();
        testcase_long_command();

        testcase_loop();
     //   testcase_clkmgr();
        testcase_dram_access();
        testcase_write();
        testcase_perisys_apb();
        testcase_errors();
        testcase_backup_register();
        testcase_fire_and_forget();

        testcase_backup_reg_to_slot();

        testcase_thread_dispatch();

        break;
    }

    CMDQ_MSG("cmdq_test_proc ended\n");
	return 0;
}

static ssize_t cmdq_write_test_proc_config(struct file *file,
                                           const char __user *userBuf,
                                           size_t count,
                                           loff_t *data)
{
    char desc[10];
    int testType = -1;
    int newTestSuit = -1;
	int32_t len = 0;

    // copy user input
	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if(copy_from_user(desc, userBuf, count))
    {
        CMDQ_ERR("TEST_CONFIG: data fail\n");
        return 0;
    }
    desc[len] = '\0';

    // process and update config
    if(0 >= sscanf(desc, "%d %d", &testType, &newTestSuit))
    {
        // sscanf returns the number of items in argument list successfully filled.
        CMDQ_ERR("TEST_CONFIG: sscanf failed\n");
        return 0;
    }

    if((0 > testType) || (2 <= testType) || (-1 == newTestSuit))
    {
        CMDQ_ERR("TEST_CONFIG: testType:%d, newTestSuit:%d\n", testType, newTestSuit);
        return 0;
    }

    mutex_lock(&gCmdqTestProcLock);
    smp_mb();

    gCmdqTestConfig[testType] = newTestSuit;

    mutex_unlock(&gCmdqTestProcLock);

    return count;
}

static int cmdq_test_open(struct inode *pInode, struct file  *pFile)
{
    return 0;
}

static struct file_operations cmdq_fops = {
    .owner = THIS_MODULE,
    .open = cmdq_test_open,
    .read = cmdq_test_proc,
    .write = cmdq_write_test_proc_config,
};

static int __init cmdq_test_init(void)
{
    CMDQ_MSG("cmdq_test_init\n");

    // Mout proc entry for debug
    gCmdqTestProcEntry = proc_mkdir("cmdq_test", NULL);
    if (NULL != gCmdqTestProcEntry)
    {
        if (NULL == proc_create("test", 0660, gCmdqTestProcEntry, &cmdq_fops))
        {
            CMDQ_MSG("cmdq_test_init failed\n");
        }
    }

    return 0;
}

static void __exit cmdq_test_exit(void)
{
    CMDQ_MSG("cmdq_test_exit\n");
	if (NULL != gCmdqTestProcEntry)
    {
        proc_remove(gCmdqTestProcEntry);
        gCmdqTestProcEntry = NULL;
    }
}

module_init(cmdq_test_init);
module_exit(cmdq_test_exit);

MODULE_LICENSE("GPL");


#endif  // CMDQ_TEST
