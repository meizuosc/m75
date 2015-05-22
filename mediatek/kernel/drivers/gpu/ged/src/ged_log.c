#include <linux/version.h>
#include <asm/io.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/genalloc.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/xlog.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "ged_base.h"
#include "ged_log.h"
#include "ged_debugFS.h"
#include "ged_profile_dvfs.h"
#include "ged_hashtable.h"

enum
{
    /* 0~7 reserved for internal buffer type */

    /* rewrite the oldest log when buffer is full */
    GED_LOG_ATTR_RINGBUFFER     = 0x1,
    /* stop logging when buffer is full */
    GED_LOG_ATTR_QUEUEBUFFER    = 0x2,
    /* increase buffersize when buffer is full */
    GED_LOG_ATTR_AUTO_INCREASE  = 0x4,
};

typedef struct GED_LOG_BUF_TAG
{
    GED_LOG_BUF_TYPE    eType;
    int                 attrs;

    void                *pMemory;
    int                 i32MemorySize;

    int                 *pi32Line;
    char                *pcBuffer;
    int                 i32LineCount;
    int                 i32BufferSize;
    int                 i32LineCurrent;
    int                 i32BufferCurrent;

    spinlock_t          sSpinLock;
    unsigned long       ui32IRQFlags;

    char                acName[GED_LOG_BUF_NAME_LENGTH];
    char                acNodeName[GED_LOG_BUF_NODE_NAME_LENGTH];

    struct dentry*      psEntry;

    struct list_head    sList;

    unsigned int        ui32HashNodeID;

} GED_LOG_BUF;

typedef struct GED_LOG_BUF_LIST_TAG
{
    rwlock_t sLock;
    struct list_head sList;
} GED_LOG_BUF_LIST;

static GED_LOG_BUF_LIST gsGEDLogBufList;

static struct dentry* gpsGEDLogEntry = NULL;
static struct dentry* gpsGEDLogBufsDir = NULL;

static GED_HASHTABLE_HANDLE ghHashTable = NULL;

//-----------------------------------------------------------------------------
//
//  GED Log Buf
//
//-----------------------------------------------------------------------------
static GED_LOG_BUF* ged_log_buf_from_handle(GED_LOG_BUF_HANDLE hLogBuf)
{
    return ged_hashtable_find(ghHashTable, (unsigned int)hLogBuf);
}

static GED_ERROR __ged_log_buf_vprint(GED_LOG_BUF *psGEDLogBuf, const char *fmt, va_list args, int attrs)
{
    int buf_n_r;
    int buf_n;
    char *buf;

    if (!psGEDLogBuf)
        return GED_OK;

    spin_lock_irqsave(&psGEDLogBuf->sSpinLock, psGEDLogBuf->ui32IRQFlags);

    /* if OOM */
    if (psGEDLogBuf->i32LineCurrent >= psGEDLogBuf->i32LineCount ||
        psGEDLogBuf->i32BufferCurrent + 256 > psGEDLogBuf->i32BufferSize)
    {
        if (attrs & GED_LOG_ATTR_RINGBUFFER)
        {
            /* for ring buffer, we start over. */
            psGEDLogBuf->i32LineCurrent = 0;
            psGEDLogBuf->i32BufferCurrent = 0;
        }
        else if (attrs & GED_LOG_ATTR_QUEUEBUFFER)
        {
            if (attrs & GED_LOG_ATTR_AUTO_INCREASE)
            {
                /* calc the avg. and incease the space by 2048 line */
                // TODO
                //ged_log_buf_resize(psGEDLogBuf->ui32HashNodeID, psGEDLogBuf->i32LineCount + psGEDLogBuf->i32LineCountOrg);
            }
            else
            {
                /* for queuebuffer only, we skip the log. */
                spin_unlock_irqrestore(&psGEDLogBuf->sSpinLock, psGEDLogBuf->ui32IRQFlags);
                return GED_ERROR_OOM;
            }
        }
    }

    buf_n = psGEDLogBuf->i32BufferSize - psGEDLogBuf->i32BufferCurrent;
    buf = psGEDLogBuf->pcBuffer + psGEDLogBuf->i32BufferCurrent;

    psGEDLogBuf->pi32Line[psGEDLogBuf->i32LineCurrent] = psGEDLogBuf->i32BufferCurrent;

    /* print the kernel time */
    if (attrs & GED_LOG_ATTR_TIME)
    {
        unsigned long long t;
        unsigned long nanosec_rem;

        t = cpu_clock(smp_processor_id());
        nanosec_rem = do_div(t, 1000000000);

        buf_n_r = snprintf(buf, buf_n, "[%5lu.%06lu] ", (unsigned long) t, nanosec_rem / 1000);

        if (buf_n_r > buf_n) buf_n_r = buf_n;

        buf_n -= buf_n_r;
        buf += buf_n_r;
    }

    buf_n_r = vsnprintf(buf, buf_n, fmt, args);

    if (buf_n_r > buf_n) buf_n_r = buf_n;

    buf_n -= buf_n_r;
    buf += buf_n_r;

    if (attrs & GED_LOG_ATTR_RINGBUFFER)
    {
        int i;
        int check = 10 + 1; /* we check the following 10 items. */ 
        int a = psGEDLogBuf->i32BufferCurrent;
        int b = psGEDLogBuf->i32BufferSize - buf_n + 1;

        for (i = psGEDLogBuf->i32LineCurrent+1; --check && i < psGEDLogBuf->i32LineCount; ++i)
        {
            int pos = psGEDLogBuf->pi32Line[i];
            if (pos >= a && pos < b)
                psGEDLogBuf->pi32Line[i] = -1;
        }

        if (check && i == psGEDLogBuf->i32LineCount)
        {
            for (i = 0; --check && i < psGEDLogBuf->i32LineCurrent; ++i)
            {
                int pos = psGEDLogBuf->pi32Line[i];

                if (pos >= a && pos < b)
                    psGEDLogBuf->pi32Line[i] = -1;
            }
        }
    }

    /* update current */
    psGEDLogBuf->i32BufferCurrent = psGEDLogBuf->i32BufferSize - buf_n + 1;
    psGEDLogBuf->i32LineCurrent += 1;

    spin_unlock_irqrestore(&psGEDLogBuf->sSpinLock, psGEDLogBuf->ui32IRQFlags);

    return GED_OK;
}

static GED_ERROR __ged_log_buf_print(GED_LOG_BUF *psGEDLogBuf, const char *fmt, ...)
{
    va_list args;
    GED_ERROR err;

    va_start(args, fmt);
    err = __ged_log_buf_vprint(psGEDLogBuf, fmt, args, psGEDLogBuf->attrs);
    va_end(args);

    return err;
}

static int __ged_log_buf_write(GED_LOG_BUF *psGEDLogBuf, const char __user *pszBuffer, int i32Count)
{
    int cnt;
    char buf[256];

    if (!psGEDLogBuf)
    {
        return 0;
    }

    cnt = (i32Count >= 256) ? 255 : i32Count;

    ged_copy_from_user(buf, pszBuffer, cnt);

    __ged_log_buf_print(psGEDLogBuf, buf);

    return cnt;
}

static ssize_t ged_log_buf_write_entry(const char __user *pszBuffer, size_t uiCount, loff_t uiPosition, void *pvData)
{
    return (ssize_t)__ged_log_buf_write((GED_LOG_BUF *)pvData, pszBuffer, (int)uiCount);
}
//-----------------------------------------------------------------------------
static void* ged_log_buf_seq_start(struct seq_file *psSeqFile, loff_t *puiPosition)
{
    GED_LOG_BUF *psGEDLogBuf = (GED_LOG_BUF *)psSeqFile->private;

    if (0 == *puiPosition)
    {
        return psGEDLogBuf;
    }
    return NULL;
}
//-----------------------------------------------------------------------------
static void ged_log_buf_seq_stop(struct seq_file *psSeqFile, void *pvData)
{

}
//-----------------------------------------------------------------------------
static void* ged_log_buf_seq_next(struct seq_file *psSeqFile, void *pvData, loff_t *puiPosition)
{
    (*puiPosition)++;

    return NULL;
}
//-----------------------------------------------------------------------------
static int ged_log_buf_seq_show(struct seq_file *psSeqFile, void *pvData)
{
    GED_LOG_BUF *psGEDLogBuf = (GED_LOG_BUF *)pvData;

    if (psGEDLogBuf != NULL)
    {
        int i;
        int attrs;

        spin_lock_irqsave(&psGEDLogBuf->sSpinLock, psGEDLogBuf->ui32IRQFlags);

        if (psGEDLogBuf->acName[0] != '\0')
        {
            seq_printf(psSeqFile, "---------- %s (%d/%d) ----------\n",
                    psGEDLogBuf->acName, psGEDLogBuf->i32BufferCurrent, psGEDLogBuf->i32BufferSize);
        }

        if (psGEDLogBuf->attrs & GED_LOG_ATTR_RINGBUFFER)
        {
            for (i = psGEDLogBuf->i32LineCurrent; i < psGEDLogBuf->i32LineCount; ++i)
            {
                int pos = psGEDLogBuf->pi32Line[i];

                if (pos >= 0)
                {
                    if (0 != seq_printf(psSeqFile, "%s\n", psGEDLogBuf->pcBuffer + pos))
                        break;
                }
            }

            //seq_printf(psSeqFile, " > ---------- start over ----------\n");

            for (i = 0; i < psGEDLogBuf->i32LineCurrent; ++i)
            {
                int pos = psGEDLogBuf->pi32Line[i];

                if (pos >= 0)
                {
                    if (0 != seq_printf(psSeqFile, "%s\n", psGEDLogBuf->pcBuffer + pos))
                        break;
                }
            }
        }
        else if (psGEDLogBuf->attrs & GED_LOG_ATTR_QUEUEBUFFER)
        {
            for (i = 0; i < psGEDLogBuf->i32LineCount; ++i)
            {
                int pos = psGEDLogBuf->pi32Line[i];

                if (pos < 0)
                    break;
                if (0 != seq_printf(psSeqFile, "%s\n", psGEDLogBuf->pcBuffer + pos))
                    break;
            }
        }

        spin_unlock_irqrestore(&psGEDLogBuf->sSpinLock, psGEDLogBuf->ui32IRQFlags);
    }

    return 0;
}
//-----------------------------------------------------------------------------
static struct seq_operations gsGEDLogBufReadOps = 
{
    .start = ged_log_buf_seq_start,
    .stop = ged_log_buf_seq_stop,
    .next = ged_log_buf_seq_next,
    .show = ged_log_buf_seq_show,
};
//-----------------------------------------------------------------------------
GED_LOG_BUF_HANDLE ged_log_buf_alloc(
        int i32MaxLineCount,
        int i32MaxBufferSizeByte,
        GED_LOG_BUF_TYPE eType, 
        const char* pszName,
        const char* pszNodeName)
{
    GED_LOG_BUF *psGEDLogBuf;
    GED_ERROR error;

    if (((!pszName) && (!pszNodeName)) || (i32MaxLineCount <= 0) || (i32MaxBufferSizeByte <= 0))
    {
        return (GED_LOG_BUF_HANDLE)0;
    }

    psGEDLogBuf = (GED_LOG_BUF*)ged_alloc(sizeof(GED_LOG_BUF));
    if (NULL == psGEDLogBuf)
    {
        GED_LOGE("ged: failed to allocate log buf!\n");
        return (GED_LOG_BUF_HANDLE)0;
    }

    psGEDLogBuf->eType = eType;

    switch (eType)
    {
        case GED_LOG_BUF_TYPE_RINGBUFFER:
            psGEDLogBuf->attrs = GED_LOG_ATTR_RINGBUFFER;
            break;
        case GED_LOG_BUF_TYPE_QUEUEBUFFER:
            psGEDLogBuf->attrs = GED_LOG_ATTR_QUEUEBUFFER;
            break;
        case GED_LOG_BUF_TYPE_QUEUEBUFFER_AUTO_INCREASE:
            psGEDLogBuf->attrs = GED_LOG_ATTR_QUEUEBUFFER | GED_LOG_ATTR_AUTO_INCREASE;
            break;
    }

    psGEDLogBuf->i32MemorySize = i32MaxBufferSizeByte + sizeof(int) * i32MaxLineCount;
    psGEDLogBuf->pMemory = ged_alloc(psGEDLogBuf->i32MemorySize);
    if (NULL == psGEDLogBuf->pMemory)
    {
        ged_free(psGEDLogBuf, sizeof(GED_LOG_BUF));
        GED_LOGE("ged: failed to allocate log buf!\n");
        return (GED_LOG_BUF_HANDLE)0;
    }

    psGEDLogBuf->pi32Line = (int *)psGEDLogBuf->pMemory;
    psGEDLogBuf->pcBuffer = ((char *)psGEDLogBuf->pMemory) + sizeof(int) * i32MaxLineCount;
    psGEDLogBuf->i32LineCount = i32MaxLineCount;
    psGEDLogBuf->i32BufferSize = i32MaxBufferSizeByte;
    psGEDLogBuf->i32LineCurrent = 0;
    psGEDLogBuf->i32BufferCurrent = 0;

    psGEDLogBuf->psEntry = NULL;
    spin_lock_init(&psGEDLogBuf->sSpinLock);
    psGEDLogBuf->acName[0] = '\0';
    psGEDLogBuf->acNodeName[0] = '\0';

    /* Init Line */
    {
        int i = 0;
        for (i = 0; i < psGEDLogBuf->i32LineCount; ++i)
            psGEDLogBuf->pi32Line[i] = -1;
    }

    if (pszName)
    {
        snprintf(psGEDLogBuf->acName, GED_LOG_BUF_NAME_LENGTH, "%s", pszName);
    }

    // Add into the global list
    INIT_LIST_HEAD(&psGEDLogBuf->sList);
    write_lock_bh(&gsGEDLogBufList.sLock);
    list_add(&psGEDLogBuf->sList, &gsGEDLogBufList.sList);
    write_unlock_bh(&gsGEDLogBufList.sLock);

    if (pszNodeName)
    {
        int err;
        snprintf(psGEDLogBuf->acNodeName, GED_LOG_BUF_NODE_NAME_LENGTH, "%s", pszNodeName);
        err = ged_debugFS_create_entry(
                psGEDLogBuf->acNodeName,
                gpsGEDLogBufsDir,
                &gsGEDLogBufReadOps,
                ged_log_buf_write_entry,
                psGEDLogBuf,
                &psGEDLogBuf->psEntry);

        if (unlikely(err)) 
        {
            GED_LOGE("ged: failed to create %s entry, err(%d)!\n", pszNodeName, err);
            ged_log_buf_free(psGEDLogBuf->ui32HashNodeID);
            return (GED_LOG_BUF_HANDLE)0;
        }
    }

    error = ged_hashtable_insert(ghHashTable, psGEDLogBuf, &psGEDLogBuf->ui32HashNodeID);
    if (GED_OK != error)
    {
        GED_LOGE("ged: failed to insert into a hash table, err(%d)!\n", error);
        ged_log_buf_free(psGEDLogBuf->ui32HashNodeID);
        return (GED_LOG_BUF_HANDLE)0;
    }

    return (GED_LOG_BUF_HANDLE)psGEDLogBuf->ui32HashNodeID;
}

GED_ERROR ged_log_buf_resize(
        GED_LOG_BUF_HANDLE hLogBuf,
        int i32NewMaxLineCount,
        int i32NewMaxBufferSizeByte)
{
    GED_LOG_BUF *psGEDLogBuf = ged_log_buf_from_handle(hLogBuf);
    int i32NewMemorySize, i32OldMemorySize;
    void *pNewMemory, *pOldMemory;

    if ((NULL == psGEDLogBuf) || (i32NewMaxLineCount <= 0) || (i32NewMaxBufferSizeByte <= 0))
    {
        return GED_ERROR_INVALID_PARAMS;
    }

    i32NewMemorySize = i32NewMaxBufferSizeByte + sizeof(int) * i32NewMaxLineCount;
    pNewMemory = ged_alloc(i32NewMemorySize);
    if (NULL == pNewMemory)
    {
        return GED_ERROR_OOM;
    }

    spin_lock_irqsave(&psGEDLogBuf->sSpinLock, psGEDLogBuf->ui32IRQFlags);
    {
        int *pi32NewLine = (int *)pNewMemory;
        char *pcNewBuffer = ((char *)pNewMemory) + sizeof(int) * i32NewMaxLineCount;

        memcpy(pi32NewLine, psGEDLogBuf->pi32Line, sizeof(int) * min(i32NewMaxLineCount, psGEDLogBuf->i32LineCount));
        {
            int i;
            for (i = psGEDLogBuf->i32LineCount + 1; i < i32NewMaxLineCount; ++i)
            {
                pi32NewLine[i] = -1;
            }
        }
        memcpy(pcNewBuffer, psGEDLogBuf->pcBuffer, min(i32NewMaxBufferSizeByte, psGEDLogBuf->i32BufferSize));

        i32OldMemorySize = psGEDLogBuf->i32MemorySize;
        pOldMemory = psGEDLogBuf->pMemory;

        psGEDLogBuf->i32MemorySize = i32NewMemorySize;
        psGEDLogBuf->pMemory = pNewMemory;
        psGEDLogBuf->pi32Line = pi32NewLine;
        psGEDLogBuf->pcBuffer = pcNewBuffer;
        psGEDLogBuf->i32LineCount = i32NewMaxLineCount;
        psGEDLogBuf->i32BufferSize = i32NewMaxBufferSizeByte;
    }
    spin_unlock_irqrestore(&psGEDLogBuf->sSpinLock, psGEDLogBuf->ui32IRQFlags);
    ged_free(pOldMemory, i32OldMemorySize);

    return GED_OK;
}

GED_ERROR ged_log_buf_ignore_lines(GED_LOG_BUF_HANDLE hLogBuf, int n)
{
    GED_LOG_BUF *psGEDLogBuf = ged_log_buf_from_handle(hLogBuf);

    if (psGEDLogBuf)
    {
        /* Only Queuebuffer support this function now */
        if (psGEDLogBuf->attrs & GED_LOG_ATTR_QUEUEBUFFER)
        {
            if (n > psGEDLogBuf->i32LineCurrent)
            {
                ged_log_buf_reset(hLogBuf);
            }
            else
            {
                int i;
                int buf_start;
                int buf_n;

                spin_lock_irqsave(&psGEDLogBuf->sSpinLock, psGEDLogBuf->ui32IRQFlags);

                /* Move lines */
                for (i = 0; n + i < psGEDLogBuf->i32LineCount; ++i)
                    psGEDLogBuf->pi32Line[i] = psGEDLogBuf->pi32Line[n + i];
                psGEDLogBuf->i32LineCurrent -= n;

                /* Move buffers */
                buf_start = psGEDLogBuf->pi32Line[n];
                buf_n = psGEDLogBuf->i32BufferCurrent - buf_start;
                for (i = 0; i < buf_n; ++i)
                    psGEDLogBuf->pcBuffer[i] = psGEDLogBuf->pcBuffer[buf_start + i];
                psGEDLogBuf->i32BufferCurrent = buf_n;

                spin_unlock_irqrestore(&psGEDLogBuf->sSpinLock, psGEDLogBuf->ui32IRQFlags);
            }
        }
    }

    return GED_OK;
}

GED_LOG_BUF_HANDLE ged_log_buf_get(const char* pszName)
{
    struct list_head *psListEntry, *psListEntryTemp, *psList;
    GED_LOG_BUF* psFound = NULL, *psLogBuf;

    if (!pszName)
    {
        return (GED_LOG_BUF_HANDLE)0;
    }

    read_lock_bh(&gsGEDLogBufList.sLock);

    psList = &gsGEDLogBufList.sList;
    list_for_each_safe(psListEntry, psListEntryTemp, psList)
    {
        psLogBuf = list_entry(psListEntry, GED_LOG_BUF, sList);
        if (0 == strcmp(psLogBuf->acName, pszName))
        {
            psFound = psLogBuf;
            break;
        }
    }

    read_unlock_bh(&gsGEDLogBufList.sLock);

    if (!psFound)
    {
        return (GED_LOG_BUF_HANDLE)0;
    }

    return (GED_LOG_BUF_HANDLE)psFound->ui32HashNodeID;
}
//-----------------------------------------------------------------------------
void ged_log_buf_free(GED_LOG_BUF_HANDLE hLogBuf)
{
    GED_LOG_BUF *psGEDLogBuf = ged_log_buf_from_handle(hLogBuf);
    if (psGEDLogBuf)
    {
        ged_hashtable_remove(ghHashTable, psGEDLogBuf->ui32HashNodeID);

        write_lock_bh(&gsGEDLogBufList.sLock);
        list_del(&psGEDLogBuf->sList);
        write_unlock_bh(&gsGEDLogBufList.sLock);

        if (psGEDLogBuf->psEntry)
        {
            ged_debugFS_remove_entry(psGEDLogBuf->psEntry);
        }

        ged_free(psGEDLogBuf->pMemory, psGEDLogBuf->i32MemorySize);
        ged_free(psGEDLogBuf, sizeof(GED_LOG_BUF));
    }
}
//-----------------------------------------------------------------------------
GED_ERROR ged_log_buf_print(GED_LOG_BUF_HANDLE hLogBuf, const char *fmt, ...)
{
    va_list args;
    GED_ERROR err;
    GED_LOG_BUF *psGEDLogBuf = ged_log_buf_from_handle(hLogBuf);

    va_start(args, fmt);
    err = __ged_log_buf_vprint(psGEDLogBuf, fmt, args, psGEDLogBuf->attrs);
    va_end(args);

    return GED_OK;
}
GED_ERROR ged_log_buf_print2(GED_LOG_BUF_HANDLE hLogBuf, int i32LogAttrs, const char *fmt, ...)
{
    va_list args;
    GED_ERROR err;
    GED_LOG_BUF *psGEDLogBuf = ged_log_buf_from_handle(hLogBuf);

    /* 0~7 reserved for internal buffer type */
    i32LogAttrs &= ~(0xff);

    va_start(args, fmt);
    err = __ged_log_buf_vprint(psGEDLogBuf, fmt, args, psGEDLogBuf->attrs | i32LogAttrs);
    va_end(args);

    return GED_OK;
}
//-----------------------------------------------------------------------------
GED_ERROR ged_log_buf_reset(GED_LOG_BUF_HANDLE hLogBuf)
{
    GED_LOG_BUF *psGEDLogBuf = ged_log_buf_from_handle(hLogBuf);
    if (psGEDLogBuf)
    {
        int i;
        spin_lock_irqsave(&psGEDLogBuf->sSpinLock, psGEDLogBuf->ui32IRQFlags);

        psGEDLogBuf->i32LineCurrent = 0;
        psGEDLogBuf->i32BufferCurrent = 0;
        for (i = 0; i < psGEDLogBuf->i32LineCount; ++i)
        {
            psGEDLogBuf->pi32Line[i] = -1;
        }

        spin_unlock_irqrestore(&psGEDLogBuf->sSpinLock, psGEDLogBuf->ui32IRQFlags);
    }

    return GED_OK;
}

//-----------------------------------------------------------------------------
//
//  GED Log System
//
//-----------------------------------------------------------------------------
static ssize_t ged_log_write_entry(const char __user *pszBuffer, size_t uiCount, loff_t uiPosition, void *pvData)
{
#define GED_LOG_CMD_SIZE 64
    char acBuffer[GED_LOG_CMD_SIZE];

    int i32Value;

    if ((0 < uiCount) && (uiCount < GED_LOG_CMD_SIZE))
    {
        if (0 == ged_copy_from_user(acBuffer, pszBuffer, uiCount))
        {
            acBuffer[uiCount] = '\0';
            if (strcmp(acBuffer, "reset") == 0)
            {
                struct list_head *psListEntry, *psListEntryTemp, *psList;
                write_lock_bh(&gsGEDLogBufList.sLock);
                psList = &gsGEDLogBufList.sList;
                list_for_each_safe(psListEntry, psListEntryTemp, psList)
                {
                    GED_LOG_BUF* psGEDLogBuf = (GED_LOG_BUF*)list_entry(psListEntry, GED_LOG_BUF, sList);
                    ged_log_buf_reset(psGEDLogBuf->ui32HashNodeID);
                }
                write_unlock_bh(&gsGEDLogBufList.sLock);
            }
            else if (strcmp(acBuffer, "profile_dvfs_enable") == 0)
            {
                ged_profile_dvfs_enable();
            }
            else if (strcmp(acBuffer, "profile_dvfs_disable") == 0)
            {
                ged_profile_dvfs_disable();
            }
            else if (strcmp(acBuffer, "profile_dvfs_start") == 0)
            {
                ged_profile_dvfs_start();
            }
            else if (strcmp(acBuffer, "profile_dvfs_stop") == 0)
            {
                ged_profile_dvfs_stop();
            }
            else if (sscanf(acBuffer, "profile_dvfs_ignore_lines %d", &i32Value) == 1)
            {
                ged_profile_dvfs_ignore_lines(i32Value);
            }
            //else if (...) //for other commands
            //{
            //}
        }
    }

    return uiCount;
}
//-----------------------------------------------------------------------------
static void* ged_log_seq_start(struct seq_file *psSeqFile, loff_t *puiPosition)
{
    struct list_head *psListEntry, *psListEntryTemp, *psList;
    loff_t uiCurrentPosition = 0;

    read_lock_bh(&gsGEDLogBufList.sLock);

    psList = &gsGEDLogBufList.sList;
    list_for_each_safe(psListEntry, psListEntryTemp, psList)
    {
        GED_LOG_BUF* psGEDLogBuf = (GED_LOG_BUF*)list_entry(psListEntry, GED_LOG_BUF, sList);
        if (psGEDLogBuf->acName[0] != '\0')
        {
            if (uiCurrentPosition == *puiPosition)
            {
                return psGEDLogBuf;
            }
            uiCurrentPosition ++;
        }
    }

    return NULL;
}
//-----------------------------------------------------------------------------
static void ged_log_seq_stop(struct seq_file *psSeqFile, void *pvData)
{
    read_unlock_bh(&gsGEDLogBufList.sLock);
}
//-----------------------------------------------------------------------------
static void* ged_log_seq_next(struct seq_file *psSeqFile, void *pvData, loff_t *puiPosition)
{
    struct list_head *psListEntry, *psListEntryTemp, *psList;
    loff_t uiCurrentPosition = 0;

    (*puiPosition)++;

    psList = &gsGEDLogBufList.sList;
    list_for_each_safe(psListEntry, psListEntryTemp, psList)
    {
        GED_LOG_BUF* psGEDLogBuf = (GED_LOG_BUF*)list_entry(psListEntry, GED_LOG_BUF, sList);
        if (psGEDLogBuf->acName[0] != '\0')
        {
            if (uiCurrentPosition == *puiPosition)
            {
                return psGEDLogBuf;
            }
            uiCurrentPosition ++;
        }
    }

    return NULL;
}
//-----------------------------------------------------------------------------
static struct seq_operations gsGEDLogReadOps = 
{
    .start = ged_log_seq_start,
    .stop = ged_log_seq_stop,
    .next = ged_log_seq_next,
    .show = ged_log_buf_seq_show,
};
//-----------------------------------------------------------------------------
GED_ERROR ged_log_system_init(void)
{
    GED_ERROR err = GED_OK;

    INIT_LIST_HEAD(&gsGEDLogBufList.sList);
    rwlock_init(&gsGEDLogBufList.sLock);

    err = ged_debugFS_create_entry(
            "gedlog",
            NULL,
            &gsGEDLogReadOps,
            ged_log_write_entry,
            NULL,
            &gpsGEDLogEntry);

    if (unlikely(err != GED_OK))
    {
        GED_LOGE("ged: failed to create gedlog entry!\n");
        goto ERROR;
    }

    err = ged_debugFS_create_entry_dir(
            "logbufs",
            NULL,
            &gpsGEDLogBufsDir);

    if (unlikely(err != GED_OK))
    {
        err = GED_ERROR_FAIL;
        GED_LOGE("ged: failed to create logbufs dir!\n");
        goto ERROR;
    }

    ghHashTable = ged_hashtable_create(5);
    if (!ghHashTable) 
    {
        err = GED_ERROR_OOM;
        GED_LOGE("ged: failed to create a hash table!\n");
        goto ERROR;
    }

    return err;

ERROR:

    ged_log_system_exit();

    return err;
}
//-----------------------------------------------------------------------------
void ged_log_system_exit(void)
{
    ged_hashtable_destroy(ghHashTable);

    ged_debugFS_remove_entry(gpsGEDLogEntry);
}
//-----------------------------------------------------------------------------
int ged_log_buf_write(GED_LOG_BUF_HANDLE hLogBuf, const char __user *pszBuffer, int i32Count)
{
    GED_LOG_BUF *psGEDLogBuf = ged_log_buf_from_handle(hLogBuf);
    return __ged_log_buf_write(psGEDLogBuf, pszBuffer, i32Count);
}

EXPORT_SYMBOL(ged_log_buf_alloc);
EXPORT_SYMBOL(ged_log_buf_get);
EXPORT_SYMBOL(ged_log_buf_free);
EXPORT_SYMBOL(ged_log_buf_print);
EXPORT_SYMBOL(ged_log_buf_reset);
