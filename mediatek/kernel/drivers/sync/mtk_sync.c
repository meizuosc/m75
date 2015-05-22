#define DEBUG_LOG_TAG "mtk_sync"

#include <linux/debugfs.h>
#include <linux/export.h>
#include <linux/seq_file.h>
#include <linux/file.h>
#include <linux/kthread.h>
#include <linux/xlog.h>
#include <linux/delay.h>

#include "mtk_sync.h"

#define SYNC_LOGV(...) xlog_printk(ANDROID_LOG_VERBOSE,             \
                                   DEBUG_LOG_TAG, __VA_ARGS__)
#define SYNC_LOGD(...) xlog_printk(ANDROID_LOG_DEBUG,               \
                                   DEBUG_LOG_TAG, __VA_ARGS__)

#define SYNC_LOGI(...) xlog_printk(ANDROID_LOG_INFO,                \
                                   DEBUG_LOG_TAG, __VA_ARGS__)
#define SYNC_LOGW(...) xlog_printk(ANDROID_LOG_WARN,                \
                                   DEBUG_LOG_TAG, __VA_ARGS__)
#define SYNC_LOGE(...) xlog_printk(ANDROID_LOG_ERROR,               \
                                   DEBUG_LOG_TAG, __VA_ARGS__)

// ---------------------------------------------------------------------------

struct sw_sync_timeline *timeline_create(const char *name)
{
    return sw_sync_timeline_create(name);
}
EXPORT_SYMBOL(timeline_create);

void timeline_destroy(struct sw_sync_timeline *obj)
{
    sync_timeline_destroy(&obj->obj);
}
EXPORT_SYMBOL(timeline_destroy);

void timeline_inc(struct sw_sync_timeline *obj, u32 value)
{
    sw_sync_timeline_inc(obj, value);
}
EXPORT_SYMBOL(timeline_inc);

int fence_create(struct sw_sync_timeline *obj, struct fence_data *data)
{
    int fd = get_unused_fd();
    int err;
    struct sync_pt *pt;
    struct sync_fence *fence;

    if (fd < 0)
        return fd;

    pt = sw_sync_pt_create(obj, data->value);
    if (pt == NULL)
    {
        err = -ENOMEM;
        goto err;
    }

    data->name[sizeof(data->name) - 1] = '\0';
    fence = sync_fence_create(data->name, pt);
    if (fence == NULL)
    {
        sync_pt_free(pt);
        err = -ENOMEM;
        goto err;
    }

    data->fence = fd;

    sync_fence_install(fence, fd);

    return 0;

err:
    put_unused_fd(fd);
    return err;
}
EXPORT_SYMBOL(fence_create);

int fence_merge(char * const name, int fd1, int fd2)
{
    int fd = get_unused_fd();
    int err;
    struct sync_fence *fence1, *fence2, *fence3;

    if (fd < 0)
        return fd;

    fence1 = sync_fence_fdget(fd1);
    if (NULL == fence1)
    {
        err = -ENOENT;
        goto err_put_fd;
    }

    fence2 = sync_fence_fdget(fd2);
    if (NULL == fence2)
    {
        err = -ENOENT;
        goto err_put_fence1;
    }

    name[sizeof(name) - 1] = '\0';
    fence3 = sync_fence_merge(name, fence1, fence2);
    if (fence3 == NULL)
    {
        err = -ENOMEM;
        goto err_put_fence2;
    }

    sync_fence_install(fence3, fd);
    sync_fence_put(fence2);
    sync_fence_put(fence1);

    return fd;

err_put_fence2:
    sync_fence_put(fence2);

err_put_fence1:
    sync_fence_put(fence1);

err_put_fd:
    put_unused_fd(fd);
    return err;
}
EXPORT_SYMBOL(fence_merge);

inline int fence_wait(struct sync_fence *fence, int timeout)
{
    return sync_fence_wait(fence, timeout);
}
EXPORT_SYMBOL(fence_wait);

// ---------------------------------------------------------------------------

#ifdef CONFIG_DEBUG_FS

/* for merged fence, the minimum of KTHREAD_NUM should be at least 3 */
#define KTHREAD_NUM     3
#define TIMEOUT         10000

static int sync_test_kthread(void *data)
{
    int i, err;
    struct sync_thread_data *sync_data = data;

    for (i = 0; i < 2; i++)
    {
        SYNC_LOGD("thread %d wait fd %d for %d ms",
            sync_data->thread_no, sync_data->fd[i], TIMEOUT);
        err = fence_wait(sync_data->fence[i], TIMEOUT);

        if (err < 0)
        {
            SYNC_LOGE("worker %d wait %d failed(%d)", sync_data->thread_no, i, err);
        }
        else
        {
            SYNC_LOGD("worker %d wait %d done", sync_data->thread_no, i);
        }
    }

    return 0;
}

static int sync_debugfs_show(struct seq_file *s, void *unused)
{

    int i, j;
    int err;
    int created_kthread_num;

    u32 curr_marker, last_marker;

    struct sw_sync_timeline *sync_timeline;
    struct sync_thread_data sync_data[KTHREAD_NUM];
    struct task_struct *sync_test_task[KTHREAD_NUM];

    SYNC_LOGD("====== start sync test ======");

    curr_marker = 0;
    last_marker = 0;

    /* create sync timeline */
    sync_timeline = timeline_create("sync_timeline");
    if (NULL == sync_timeline)
    {
        SYNC_LOGD("can't create sw_sync_timeline");
        return -ENOENT;
    }
    else
    {
        SYNC_LOGD("sw_sync_timline created");
    }

    /* create sync fence */
    for (i = 0; i < KTHREAD_NUM - 1; i++)
    {
        sync_data[i].thread_no = i;

        for (j = 0; j < 2; j++)
        {
            struct fence_data data;

            sprintf(data.name, "fence_%d-%d", i, j);
            last_marker = last_marker + 1;
            data.value = last_marker;
            err = fence_create(sync_timeline, &data);
            if (err < 0)
            {
                SYNC_LOGE("create sync fence failed(%d)", err);
                goto err_stop_timeline;
            }

            sync_data[i].fd[j] = data.fence;
            /* get fence from fd */
            sync_data[i].fence[j] = sync_fence_fdget(sync_data[i].fd[j]);

            SYNC_LOGD("curr(%u) last(%u), %s fd %d created",
                curr_marker, last_marker, data.name, data.fence);
        }
    }

    /* create merged fence */
    sync_data[i].thread_no = i;
    for (j = 0; j < 2; j++)
    {
        struct fence_data data;

        sprintf(data.name, "merged_fence_%d-%d", i, j);
        data.fence = fence_merge(
                        data.name, sync_data[0].fd[j], sync_data[1].fd[j]);
        if (data.fence < 0)
        {
            SYNC_LOGE("create merged sync fence failed(%d)", data.fence);
            goto err_stop_timeline;
        }

        sync_data[i].fd[j] = data.fence;
        /* get fence from fd */
        sync_data[i].fence[j] = sync_fence_fdget(sync_data[i].fd[j]);

        SYNC_LOGD("curr(%u) last(%u), %s fd %d created",
            curr_marker, last_marker, data.name, data.fence);
    }

    /* create worker thread */
    created_kthread_num = 0;
    for (i = 0; i < KTHREAD_NUM; i++)
    {
        struct task_struct *task;
        char name[32];

        sprintf(name, "sync_worker_%d", i);

        task = kthread_create(sync_test_kthread, &sync_data[i], name);
        if (IS_ERR(task))
        {
            SYNC_LOGE("create sync test task failed");
            err = PTR_ERR(task);
            task = NULL;
        }

        sync_test_task[i] = task;
        wake_up_process(sync_test_task[i]);
        SYNC_LOGD("%s created", name);

        created_kthread_num = created_kthread_num + 1;
    }

    /* signal fence */
    for (i = 0; i < KTHREAD_NUM; i++)
    {
        for (j = 0; j < 2; j++)
        {
            /* sleep for 1 second before signal fence */
            const int DELAY_MS = 1000;
            SYNC_LOGD("sleep for %d ms before singal fence", DELAY_MS);
            msleep(DELAY_MS);

            curr_marker = curr_marker + 1;
            timeline_inc(sync_timeline, 1);

            SYNC_LOGD("curr(%u) last(%u), inc timeline", curr_marker, last_marker);
        }
    }

err_stop_timeline:
    /* release timeline */
    timeline_destroy(sync_timeline);
    SYNC_LOGD("sw_sync_timline released");
    SYNC_LOGD("====== end sync test ======");

	return err;
}

static int sync_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, sync_debugfs_show, inode->i_private);
}

static const struct file_operations sync_debugfs_fops = {
    .open           = sync_debugfs_open,
    .read           = seq_read,
    .llseek         = seq_lseek,
    .release        = single_release,
};

static __init int sync_debugfs_init(void)
{
	debugfs_create_file("mtk_sync", S_IRUGO, NULL, NULL, &sync_debugfs_fops);
	return 0;
}
late_initcall(sync_debugfs_init);


#endif
