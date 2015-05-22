/*
 * NET4:	Implementation of BSD Unix domain sockets.
 *
 * Authors:	Alan Cox, <alan@lxorguk.ukuu.org.uk>
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Fixes:
 *		Linus Torvalds	:	Assorted bug cures.
 *		Niibe Yutaka	:	async I/O support.
 *		Carsten Paeth	:	PF_UNIX check, address fixes.
 *		Alan Cox	:	Limit size of allocated blocks.
 *		Alan Cox	:	Fixed the stupid socketpair bug.
 *		Alan Cox	:	BSD compatibility fine tuning.
 *		Alan Cox	:	Fixed a bug in connect when interrupted.
 *		Alan Cox	:	Sorted out a proper draft version of
 *					file descriptor passing hacked up from
 *					Mike Shaver's work.
 *		Marty Leisner	:	Fixes to fd passing
 *		Nick Nevin	:	recvmsg bugfix.
 *		Alan Cox	:	Started proper garbage collector
 *		Heiko EiBfeldt	:	Missing verify_area check
 *		Alan Cox	:	Started POSIXisms
 *		Andreas Schwab	:	Replace inode by dentry for proper
 *					reference counting
 *		Kirk Petersen	:	Made this a module
 *	    Christoph Rohland	:	Elegant non-blocking accept/connect algorithm.
 *					Lots of bug fixes.
 *	     Alexey Kuznetosv	:	Repaired (I hope) bugs introduces
 *					by above two patches.
 *	     Andrea Arcangeli	:	If possible we block in connect(2)
 *					if the max backlog of the listen socket
 *					is been reached. This won't break
 *					old apps and it will avoid huge amount
 *					of socks hashed (this for unix_gc()
 *					performances reasons).
 *					Security fix that limits the max
 *					number of socks to 2*max_files and
 *					the number of skb queueable in the
 *					dgram receiver.
 *		Artur Skawina   :	Hash function optimizations
 *	     Alexey Kuznetsov   :	Full scale SMP. Lot of bugs are introduced 8)
 *	      Malcolm Beattie   :	Set peercred for socketpair
 *	     Michal Ostrowski   :       Module initialization cleanup.
 *	     Arnaldo C. Melo	:	Remove MOD_{INC,DEC}_USE_COUNT,
 *	     				the core infrastructure is doing that
 *	     				for all net proto families now (2.5.69+)
 *
 *
 * Known differences from reference BSD that was tested:
 *
 *	[TO FIX]
 *	ECONNREFUSED is not returned from one end of a connected() socket to the
 *		other the moment one end closes.
 *	fstat() doesn't return st_dev=0, and give the blksize as high water mark
 *		and a fake inode identifier (nor the BSD first socket fstat twice bug).
 *	[NOT TO FIX]
 *	accept() returns a path name even if the connecting socket has closed
 *		in the meantime (BSD loses the path and gives up).
 *	accept() returns 0 length path for an unbound connector. BSD returns 16
 *		and a null first byte in the path (but not for gethost/peername - BSD bug ??)
 *	socketpair(...SOCK_RAW..) doesn't panic the kernel.
 *	BSD af_unix apparently has connect forgetting to block properly.
 *		(need to check this with the POSIX spec in detail)
 *
 * Differences from 2.0.0-11-... (ANK)
 *	Bug fixes and improvements.
 *		- client shutdown killed server socket.
 *		- removed all useless cli/sti pairs.
 *
 *	Semantic changes/extensions.
 *		- generic control message passing.
 *		- SCM_CREDENTIALS control message.
 *		- "Abstract" (not FS based) socket bindings.
 *		  Abstract names are sequences of bytes (not zero terminated)
 *		  started by 0, so that this name space does not intersect
 *		  with BSD names.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/stat.h>
#include <linux/dcache.h>
#include <linux/namei.h>
#include <linux/socket.h>
#include <linux/un.h>
#include <linux/fcntl.h>
#include <linux/termios.h>
#include <linux/sockios.h>
#include <linux/net.h>
#include <linux/in.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <net/net_namespace.h>
#include <net/sock.h>
#include <net/tcp_states.h>
#include <net/af_unix.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <net/scm.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/rtnetlink.h>
#include <linux/mount.h>
#include <net/checksum.h>
#include <linux/security.h>
#include <linux/freezer.h>

//#ifndef CONFIG_UNIX_SOCKET_TRACK_TOOL
//#define CONFIG_UNIX_SOCKET_TRACK_TOOL
//#endif

#ifdef CONFIG_UNIX_SOCKET_TRACK_TOOL
#include <linux/file.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/jiffies.h>
#endif/* CONFIG_UNIX_SOCKET_TRACK_TOOL */
#include <linux/uio.h>
#include <linux/blkdev.h>
#include <linux/compat.h>
#include <linux/rtc.h>
#include <asm/kmap_types.h>
#include <linux/device.h>


struct hlist_head unix_socket_table[2 * UNIX_HASH_SIZE];
EXPORT_SYMBOL_GPL(unix_socket_table);
DEFINE_SPINLOCK(unix_table_lock);
EXPORT_SYMBOL_GPL(unix_table_lock);
static atomic_long_t unix_nr_socks;


static struct hlist_head *unix_sockets_unbound(void *addr)
{
	unsigned long hash = (unsigned long)addr;

	hash ^= hash >> 16;
	hash ^= hash >> 8;
	hash %= UNIX_HASH_SIZE;
	return &unix_socket_table[UNIX_HASH_SIZE + hash];
}

#define UNIX_ABSTRACT(sk)	(unix_sk(sk)->addr->hash < UNIX_HASH_SIZE)

#ifdef CONFIG_UNIX_SOCKET_TRACK_TOOL
//unix_sock_track_blc结构 description:

typedef struct unix_sock_track_blc_st
{
    struct unix_sock_track_blc_st     *next;    /* point to next unix_sock_track_blc_st */
    //char            record_time_s[20];        /* string like '04-26 15:32:17.850' */
                                                /* record 0 position time.  recode_wrap_around == 1, 
                                                great current position time: 
                                                current position time - __UNIX_SOCKET_INFO_SIZE__*__UNIX_SOCKET_INFO_SIZE__ */
    //unsigned int    time_yy;                  /* year */
    //unsigned int    time_mon;                 /* mon */
    //unsigned int    time_day;                 /* day */
    //unsigned int    time_hour;                /* hour */
    //unsigned int    time_min;                 /* min */
    unsigned int    time_sc;                  /* socket create sec */
    unsigned int    time_ms;                  /* socket create msec */   
    //unsigned long record_time_ms;           /* ms value. first recod infor used time */
    unsigned int    recode_wrap_around;       /* record bytes used out */
    unsigned int    record_current_p;         /* current recod infor position */
    unsigned long   record_current_sc;        /* tick value. first recod infor used time */   
    unsigned long   record_current_ms;        /* tick value. first recod infor used time */
    //unsigned int    socket_type;              /* socket type */
    //unsigned int    block;                    /* 0: block, 1: non-block*/
    //unsigned int    wait_timer;               /* block socket wait time, default value MAX_SCHEDULE_TIMEOUT */
    unsigned int    nod_ino;                  /* self kernel socket identify */       
    unsigned int    listen;                   /* socket listen or not. 0: not listen, value>0: backlog number */
    unsigned long   listen_time_sc;           /* do listen action time(se) */
    unsigned long   listen_time_ms;           /* do listen action time(ms) */
    unsigned int    peer_ino;                 /* peer kernel socket identify */
    unsigned int    create_fd;                /* fd value when socket create at create thread */
    unsigned int    used_fd;                  /* fd value when socket used at use thread */
    //char            create_thread_name[16];   /* create socket tread name */
    //char            use_thread_name[16];      /* use socket thread name */
    unsigned int    create_pid;               /* create task pid */
    unsigned int    create_tid;               /* create task tid */
    unsigned int    used_pid;                 /* used task pid */
    unsigned int    used_tid;                 /* used task tid */
    char            unix_address[16];         /* unix socket address, not auto bind.
                                               unix_address[0] == 0, not bind
                                               unix_address[0] != 0, app call bind
                                              */
    unsigned int    send_buffer_size_max;     /* max send buffer size */
    unsigned int    send_buffer_size_min;     /* min send buffer size */
    unsigned int    recv_buffer_size_max;     /* max recv buffer size */
    unsigned int    recv_buffer_size_min;     /* min recv buffer size */
    unsigned int    recv_queue_len;           /* max recv queue packet number */
    unsigned int    recv_count;               /* recv count */
    unsigned int    send_count;               /* send count */
    unsigned int    send_data_total;          /* total send data number */
    unsigned int    recv_data_total;          /* total recv data number */
    unsigned int    shutdown_state;           /* shutdown state.  */
    int             error_flag;               /* action error. this error could not recover */
    unsigned long   close_time_sc;            /* socket close time(se  value) */
    unsigned long   close_time_ms;            /* socket close time(ms value) */
    unsigned char   *record_info_blc;         /* point to record infor address */
}unix_sock_track_blc;


//////////update 'record infor description' begin///////////////// 
typedef struct unix_sock_track_header_st
{
    unsigned char		*list_head;
    spinlock_t 			list_lock;
    unsigned long		rm_close_sock_time;
}unix_sock_track_header;
//record infor的记录单元是 byte.

#define SK_RW   0x01  //socket do read action 
#define SK_RD   0x02  //socket read finish 
#define SK_RO   0x03  //socket read 0 size 

#define SK_READ_MASK  0x03

#define SK_WW   0x04  //socket do write action 
#define SK_WD   0x08  //socket write finish 
#define SK_WE   0x0C  //socket write error ----- not include retry 

#define SK_WRITE_MASK 0x0c

#define SK_PW   0x10  //socket do poll action 
#define SK_PF   0x20  //socket poll finish 
#define SK_PT   0x30  //socket poll there is event to handle 

#define SK_POLL_MASK  0x30

#define SK_AW   0x40  //socket do accept or connect action 
#define SK_AF   0x80  //socket accept or connect fail
#define SK_AT   0xc0  //socket accept or connect ok 

#define SK_ACCEPT_MASK  0xc0

//////////update 'record infor description' end/////////////////  


//#define __UNIX_SOCKET_RM_SOKCET_TIME__    1000*10
#define __UNIX_SOCKET_RM_SOKCET_TIME__    60  // seconds

#define __UNIX_SOCKET_INFO_SIZE__     3*1024
#define __UNIX_SOCKET_TIME_UNIT__     200

static spinlock_t unix_dbg_info_lock;
static unix_sock_track_blc *unix_sock_blc_head = NULL;
static unix_sock_track_blc *unix_sock_blc_out_head = NULL;
static int unix_sock_blc_out_index = -1;
#define USTK_HASH_LEN		6
#define USTK_HASH_SIZE	((1 << USTK_HASH_LEN) + 1)//64

static int unix_socket_info_lock_init = 0;
static unsigned long  unix_socket_rm_close_sock_blc_time = 0; 
//record last clear time value

static unix_sock_track_header	unix_sock_track_head[USTK_HASH_SIZE] = {0};

static unix_sock_track_blc* unix_sock_track_find_blc_with_action(unsigned int action, unix_sock_track_blc* tmp, unsigned long ino);

#endif /* CONFIG_UNIX_SOCKET_TRACK_TOOL */

//for aee interface start
#define __UNIX_SOCKET_OUTPUT_BUF_SIZE__   3500
static struct proc_dir_entry *gunix_socket_track_aee_entry = NULL;
#define UNIX_SOCK_TRACK_AEE_PROCNAME "driver/usktrk_aee"
#define UNIX_SOCK_TRACK_PROC_AEE_SIZE 3072

static volatile unsigned int unix_sock_track_stop_flag = 0;
#define unix_peer(sk) (unix_sk(sk)->peer)

#ifdef CONFIG_UNIX_SOCKET_TRACK_TOOL
static unsigned int g_buf_len = 0;
static unsigned char *pBuf = NULL;
static unsigned int passCnt = 1;
static unsigned int unix_socket_test_stop = 0;
static unsigned char unix_sock_track_out_buf[__UNIX_SOCKET_OUTPUT_BUF_SIZE__] = {1};

static void unix_sock_track_dump_socket_info(void)
{
    unsigned char *tmp = 0;
    unsigned long flags;
    #ifdef CONFIG_MTK_NET_LOGGING 
    printk("[mtk_net][af_unix] stop socket record, start output infor \n");   
    #endif    
    g_buf_len = 0;
  
    if (unix_sock_track_stop_flag == 0)
    {
        //unix_sock_track_stop_flag = 1; 
    }
    #ifdef CONFIG_MTK_NET_LOGGING 
    printk("[mtk_net][unix] dump_socket_info unix_sock_track_stop_flag=%d\n", unix_sock_track_stop_flag);  
    #endif
    memset(unix_sock_track_out_buf, 0, __UNIX_SOCKET_OUTPUT_BUF_SIZE__);

unix_sock_track_dump_next_list:    
	
    if (unix_sock_blc_out_head == NULL)
    {
        unix_sock_blc_out_index++;
            
        if (unix_sock_blc_out_index >= USTK_HASH_SIZE)
        {
            return;
        }
    }

    spin_lock_irqsave(&(unix_sock_track_head[unix_sock_blc_out_index].list_lock), flags);  
    
    if (unix_sock_blc_out_head == NULL)
    {
        if ((unix_sock_track_blc*)(unix_sock_track_head[unix_sock_blc_out_index].list_head != NULL))
        {
            unix_sock_blc_out_head = (unix_sock_track_blc*)(unix_sock_track_head[unix_sock_blc_out_index].list_head);
        }
        else
        {
            spin_unlock_irqrestore(&(unix_sock_track_head[unix_sock_blc_out_index].list_lock), flags); 
            goto unix_sock_track_dump_next_list;
        }
    }

    //unix_sock_track_stop_flag = 2;
     
    while(unix_sock_blc_out_head != NULL)
    {
        if (unix_sock_blc_out_head->record_info_blc != NULL)
        {
            tmp = unix_sock_blc_out_head->record_info_blc;
            if (g_buf_len == 0)
            {
                memcpy(unix_sock_track_out_buf, unix_sock_blc_out_head, sizeof(unix_sock_track_blc));
                g_buf_len = g_buf_len + sizeof(unix_sock_track_blc);
                memcpy(unix_sock_track_out_buf + g_buf_len, tmp, __UNIX_SOCKET_INFO_SIZE__);
                g_buf_len = g_buf_len + __UNIX_SOCKET_INFO_SIZE__;
                unix_sock_blc_out_head = unix_sock_blc_out_head->next;
            }
            //printk("[usktrk] unix_sock_track_dump_socket_info g_buf_len=%d\n", g_buf_len);
            break;
        }
        else
        {
            //printk("[usktrk] unix_sock_track_dump_socket_info1 g_buf_len=%d\n", g_buf_len);
            if ((g_buf_len + sizeof(unix_sock_track_blc)) >= __UNIX_SOCKET_INFO_SIZE__)
            {
                break;
            }
      
            memcpy(unix_sock_track_out_buf + g_buf_len, unix_sock_blc_out_head, sizeof(unix_sock_track_blc));
            g_buf_len = g_buf_len + sizeof(unix_sock_track_blc);    
            unix_sock_blc_out_head = unix_sock_blc_out_head->next;        
        }
        
        if (unix_sock_blc_out_head == NULL)
        {
            spin_unlock_irqrestore(&(unix_sock_track_head[unix_sock_blc_out_index].list_lock), flags); 
            goto unix_sock_track_dump_next_list;
        }    

    }
    
    spin_unlock_irqrestore(&(unix_sock_track_head[unix_sock_blc_out_index].list_lock), flags); 
    #ifdef CONFIG_MTK_NET_LOGGING 
    printk("[mtk_net][unix] unix_sock_track_dump_socket_info1 g_buf_len=%d\n", g_buf_len);
    #endif
}

static int unix_sock_track_dev_proc_for_aee_read(char *page, char **start, off_t off, int count, int *eof, void *data)
{
    unix_sock_track_blc* outcheck_tmp = NULL;
    unsigned int len = 0;
    #ifdef CONFIG_MTK_NET_LOGGING 
    printk("[mtk_net][unix] for aee page(%p)off(%d)count(%d)\n", page, off, count);
    #endif
    
    unix_sock_track_dump_socket_info();
    #ifdef CONFIG_MTK_NET_LOGGING 
    printk("[mtk_net][unix] unix_sock_track_dev_proc_for_aee_read g_buf_len=%d\n", g_buf_len);
    #endif
    if (g_buf_len > 0)
    {
        memcpy(page, unix_sock_track_out_buf, g_buf_len);
        *start += g_buf_len;
        *eof = 1;
        passCnt--;
        return g_buf_len;
    }
    else
    {
        unix_sock_blc_out_index = -1;
        unix_sock_blc_out_head = NULL;
        return 0;
    }
    
#if 0    // 0626 test ok
    if (passCnt > 0)
    {
        g_buf_len = __UNIX_SOCKET_OUTPUT_BUF_SIZE__;
        memcpy(page, unix_sock_track_out_buf, g_buf_len);
        *start += g_buf_len;
        *eof = 1;
        passCnt--;
        return g_buf_len;
    }
    else
        return 0;
#endif
}

#else

static int unix_sock_track_dev_proc_for_aee_read(char *page, char **start, off_t off, int count, int *eof, void *data)
{
    return 0;
}

#endif /*CONFIG_UNIX_SOCKET_TRACK_TOOL*/
#if 0
static int unix_sock_track_dev_proc_for_aee_write(struct file *file, const char *buffer, unsigned long count, void *data)
{
    return 0;
}

int unix_sock_track_dev_proc_for_aee_setup(void)
{
    gunix_socket_track_aee_entry= create_proc_entry(UNIX_SOCK_TRACK_AEE_PROCNAME, 0664, NULL);
    if(gunix_socket_track_aee_entry == NULL)
    {
       #ifdef CONFIG_MTK_NET_LOGGING 
        printk("[mtk_net][unix] Unable to create / usktrk_aee proc entry\n\r");
        #endif
        return -1;
    }
    gunix_socket_track_aee_entry->read_proc = unix_sock_track_dev_proc_for_aee_read;
    gunix_socket_track_aee_entry->write_proc = unix_sock_track_dev_proc_for_aee_write;
    return 0;
}

int unix_sock_track_dev_proc_for_aee_remove(void)
{
    if (NULL != gunix_socket_track_aee_entry)
    {
        remove_proc_entry(UNIX_SOCK_TRACK_AEE_PROCNAME, NULL);
    }
    return 0;
}

static int unix_sock_track_open(struct inode *inode, struct file *file)
{
	#ifdef CONFIG_MTK_NET_LOGGING 
    printk("[mtk_net][unix] unix_sock_track_open\n");
    #endif
    return 0;
}

static int unix_sock_track_close(struct inode *inode, struct file *file)
{
	#ifdef CONFIG_MTK_NET_LOGGING 
    printk("[mtk_net][unix] unix_sock_track\n");
    #endif
    return 0;
}

#define USKTRK_DRIVER_NAME "mtk_stp_usktrk"
#define MTK_USKTRK_VERSION  "SOC Consys unix socket Driver - v1.0"
#define MTK_USKTRK_DATE     "2013/01/20"
//#define USKTRK_DEV_MAJOR 191 
#define USKTRK_DEV_MAJOR 141 
#define USKTRK_DEV_NUM 1

/* Linux UCHAR device */
static int gusktrkMajor = USKTRK_DEV_MAJOR;
static struct cdev gusktrkCdev;

ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long);

struct file_operations gusktrkfops = {
    .open = unix_sock_track_open,
    .release = unix_sock_track_close,
    .read = NULL,
    .write = NULL,
//    .ioctl = WMT_ioctl,
    .unlocked_ioctl = NULL,
    .poll = NULL,
};

static int unix_sock_track_init(void)
{
    dev_t devID = MKDEV(gusktrkMajor, 0);
    int cdevErr = -1;
    int ret = -1;
    #ifdef CONFIG_MTK_NET_LOGGING 
    printk("[mtk_net][unix] Version= %s DATE=%s\n", MTK_USKTRK_VERSION, MTK_USKTRK_DATE);
    #endif
    /* Prepare a UCHAR device */
    /*static allocate chrdev*/

    ret = register_chrdev_region(devID, USKTRK_DEV_NUM, USKTRK_DRIVER_NAME);
    if (ret) 
    {
    	#ifdef CONFIG_MTK_NET_LOGGING 
        printk("[mtk_net][unix] fail to register chrdev\n");
        #endif
        return ret;
    }

    cdev_init(&gusktrkCdev, &gusktrkfops);
    gusktrkCdev.owner = THIS_MODULE;

    cdevErr = cdev_add(&gusktrkCdev, devID, USKTRK_DEV_NUM);
    if (cdevErr) 
    {
    	#ifdef CONFIG_MTK_NET_LOGGING 
        printk("[mtk_net][unix] cdev_add() fails (%d)\n", cdevErr);
        #endif
        goto error;
    }
    #ifdef CONFIG_MTK_NET_LOGGING 
    printk("[mtk_net][unix] driver(major %d) installed \n", gusktrkMajor);
    #endif
    unix_sock_track_dev_proc_for_aee_setup();
    #ifdef CONFIG_MTK_NET_LOGGING 
    printk("[mtk_net][unix] dev register success \n");
    #endif
    return 0;

error:
    #ifdef CONFIG_MTK_NET_LOGGING 
    printk("[mtk_net][unix] dev register fail \n");
    #endif
    return -1;
}

static void unix_sock_track_exit (void)
{
    dev_t dev = MKDEV(gusktrkMajor, 0);
  
    unix_sock_track_dev_proc_for_aee_remove();

    cdev_del(&gusktrkCdev);
    unregister_chrdev_region(dev, USKTRK_DEV_NUM);
    gusktrkMajor = -1;
    #ifdef CONFIG_MTK_NET_LOGGING 
    printk("[mtk_net][unix] exit done\n");
    #endif
}

module_init(unix_sock_track_init);
module_exit(unix_sock_track_exit);
//MODULE_LICENSE("Proprietary");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("MediaTek Inc unix socket tool");
MODULE_DESCRIPTION("MTK unix socket tool function");

module_param(gusktrkMajor, uint, 0);
#endif
#ifdef CONFIG_UNIX_SOCKET_TRACK_TOOL
//doing    1: read
//doing    2: write
//doing    3: poll
//doing    4: accept

void unix_test_time_log(struct socket *sock, int doing)
{
#if 0	
    struct timeval tv = {0}; //tv_0 = {0}
    char  name[200] = {0};
    struct rtc_time tm;
    unsigned int inod = 0;

    //if ((strstr(current->comm, "zygote") != NULL)/* ||
    //(strstr(current->comm, "UI") != NULL)*/)
    //if ((strstr(current->comm, "adbd") != NULL))
    //if ((strstr(current->comm, "UI") != NULL))

    if ((strstr(current->comm, "init") != NULL))
    {
        do_gettimeofday(&tv);
        tv.tv_sec -= sys_tz.tz_minuteswest*60;
        rtc_time_to_tm(tv.tv_sec, &tm);

        if (SOCK_INODE(sock))
        {
             inod = SOCK_INODE(sock)->i_ino;
        }
      
        memset(name, 0, sizeof(name));
        sprintf(name, "[mtk_net][usktrk], nod= %u time:%02d-%02d-%02d:%lu, tv_sec=%u",
        inod, tm.tm_hour, tm.tm_min, tm.tm_sec, tv.tv_usec, tv.tv_sec);    

        if (doing == 1)
        {
        	#ifdef CONFIG_MTK_NET_LOGGING 
            printk("[mtk_net][unix] %s:  do read\n", name);
            #endif
        }
    
        if (doing == 2)
        {
        	#ifdef CONFIG_MTK_NET_LOGGING 
            printk("[mtk_net][unix] %s:  do write\n", name);
            #endif
        }

        if (doing == 3)
        {
        	#ifdef CONFIG_MTK_NET_LOGGING 
            printk("[mtk_net][unix] %s:  do poll\n", name);
            #endif
        } 

        if (doing == 4)
        {
        	#ifdef CONFIG_MTK_NET_LOGGING 
            printk("[mtk_net][unix] %s:  do accept\n", name);
            #endif
        }
    }
#endif
    return;
}


static void unix_sock_track_rm_close_sock(unsigned long ino)
{
    if (unix_sock_track_stop_flag == 0)
    {
        unix_sock_track_find_blc_with_action(3, NULL, ino);
    }
}

static unix_sock_track_blc* unix_sock_track_find_blc_with_action(unsigned int action, unix_sock_track_blc* tmp, unsigned long ino)
{
    unix_sock_track_blc *head_t = NULL;
    unsigned long flags;
    unsigned int	hash_value = 0;
    //action==1, add node 
    //action==2, find nod_ino==ino node
    //action==3, free time over node
    //action==4, output sokcet infor 

    if (unix_sock_track_stop_flag == 0)
    {
  
        if (action == 1)
        {
            unsigned char *list_head_t = 0;
            
            hash_value = ino - ((ino >> USTK_HASH_LEN) << USTK_HASH_LEN);
            //printk("[usktrk] with_action 1 hash_value =%u\n", hash_value);
            spin_lock_irqsave(&(unix_sock_track_head[hash_value].list_lock), flags);
            if (unix_sock_track_head[hash_value].list_head == NULL)
            {
                unix_sock_track_head[hash_value].list_head = (unsigned char*)tmp;
                #ifdef CONFIG_MTK_NET_LOGGING 
                printk("[mtk_net][unix]action unix_sock_blc_head=NULL \n");    
                #endif 
            }
            else
            {
                head_t = (unix_sock_track_blc*)unix_sock_track_head[hash_value].list_head;
                
                while(head_t->next != NULL)
                {
                    head_t = head_t->next;
                    if (unix_sock_track_stop_flag != 0)
                    {
                        break;
                    }
                }
        
                head_t->next = tmp;           
            }
            
            spin_unlock_irqrestore(&(unix_sock_track_head[hash_value].list_lock), flags); 
        }
        else if (action == 2)
        {
            hash_value = ino - ((ino >> USTK_HASH_LEN) << USTK_HASH_LEN);
            //printk("[usktrk] with_action 2 hash_value =%u\n", hash_value);
            spin_lock_irqsave(&(unix_sock_track_head[hash_value].list_lock), flags);
            head_t = (unix_sock_track_blc*)unix_sock_track_head[hash_value].list_head;
            while(head_t != NULL)
            {
                if ((head_t->nod_ino == ino) && (head_t->record_info_blc != NULL))
                {
                    break;
                }
                
                if (unix_sock_track_stop_flag != 0)
                {
                    break;
                }
                                    
                head_t = head_t->next;
            }
            spin_unlock_irqrestore(&(unix_sock_track_head[hash_value].list_lock), flags); 
        }
        else if (action == 3)
        {
            unix_sock_track_blc *tmp1 = NULL;
            unix_sock_track_blc *tmp_prev = NULL;
            unsigned long current_time_ms_t = 0;
            unsigned long rm_close_base_ms_t = 0;
            unsigned long long t;
            unsigned long nanosec_rem;    
            struct timeval tv = {0};

            do_gettimeofday(&tv);

            tv.tv_sec -= sys_tz.tz_minuteswest*60;
            current_time_ms_t = tv.tv_sec;
               
            hash_value = ino - ((ino >> USTK_HASH_LEN) << USTK_HASH_LEN);
            
            spin_lock_irqsave(&(unix_sock_track_head[hash_value].list_lock), flags);
            rm_close_base_ms_t = unix_sock_track_head[hash_value].rm_close_sock_time;
            
            if (unix_sock_track_head[hash_value].list_head == NULL)
            {
                goto rm_node_end;
            }
            
            if (unix_sock_track_head[hash_value].rm_close_sock_time == 0)
            {
                unix_sock_track_head[hash_value].rm_close_sock_time = current_time_ms_t;
                goto rm_node_end;
            }
            else
            {
                if (current_time_ms_t - rm_close_base_ms_t <  __UNIX_SOCKET_RM_SOKCET_TIME__)
                {
                    goto rm_node_end;
                }
      
  rm_node_update_header:      
                if (unix_sock_track_head[hash_value].list_head == NULL)
                {
                    head_t = NULL;
                    goto rm_node_end;
                }
           
                tmp_prev = (unix_sock_track_blc*)unix_sock_track_head[hash_value].list_head;
                head_t = tmp_prev;          
          
  rm_node_check:        
                if (head_t != NULL) 
                {
                    tmp1 = NULL;
                                  
                    if ((head_t->close_time_ms != 0) && (head_t->record_info_blc == NULL))
                    {
                        //printk("[usktrk] action close_time_ms:%lu\n", head_t->close_time_ms); 
                        if (((current_time_ms_t > head_t->close_time_ms) && 
                           ((current_time_ms_t - head_t->close_time_ms) >= __UNIX_SOCKET_RM_SOKCET_TIME__)) ||
                           ((current_time_ms_t < head_t->close_time_ms) && 
                           (current_time_ms_t >= __UNIX_SOCKET_RM_SOKCET_TIME__)))
                        {
                            tmp1 = head_t;
                            //printk("[usktrk] action tmp1 = 0x%x\n", tmp1);  
                        }
                    }         
            
                    if (unix_sock_track_stop_flag != 0)
                    {
                        goto rm_node_end;
                    }
                
                    if (tmp1 != NULL)
                    {
                        //printk("[usktrk] 1 socket close[%lu] \n", tmp1->nod_ino);      
                        //printk("[usktrk] 1 recv coun:%u, send count:%u \n", tmp1->recv_count, tmp1->send_count);   
                        //printk("[usktrk] 1 send data total:%u, recv data tatal:%u \n", tmp1->send_data_total, tmp1->recv_data_total);                 
              
                        if (((unsigned char*)tmp1) == unix_sock_track_head[hash_value].list_head)
                        {                 
                            unix_sock_track_head[hash_value].list_head = (unsigned char*)(tmp1->next);
                            //printk("[usktrk] 3 free tmp1 \n");
                            kfree(tmp1);                    
                            goto rm_node_update_header;
                        } 
                
                    }       
        
                    if (tmp_prev != head_t)
                    {
                        if (tmp1 != NULL)
                        {
                            tmp_prev->next = tmp1->next;
                            //printk("[usktrk] 4 free tmp1 \n");
                            kfree(tmp1);                  
                        } 
                        else
                        {             
                            tmp_prev = tmp_prev->next;
                        }
                
                        if (tmp_prev == NULL)
                        {
                            //printk("[usktrk] 5 last nod \n");
                            head_t = NULL;
                            goto rm_node_end;
                        }
                    }
            
                    head_t = tmp_prev->next;
                
                    if (unix_sock_track_stop_flag != 0)
                    {
                        goto rm_node_end;
                    }                
            
                    goto rm_node_check;
                        
                }

                head_t = NULL;
                goto rm_node_end;
            
rm_node_end:      
                spin_unlock_irqrestore(&(unix_sock_track_head[hash_value].list_lock), flags);        
            }

        }
    }
        
    return head_t;
}

unix_sock_track_blc* unix_sock_track_socket_create1(unsigned long ino, unsigned int socket_type)
{

    unix_sock_track_blc *unix_sock_blc_t = NULL;
    unix_sock_track_blc *head_t = NULL;
    char  *sock_infor = NULL;
    unsigned long flags;
    unsigned long current_time_se = 0;
    unsigned long time_se = 0;
    unsigned long time_ms = 0;
    int temp_len = 0;
    unsigned long long t;
    unsigned long nanosec_rem;  
    struct timeval tv = {0};

    //printk("[usktrk] create1 unix_sock_track_stop_flag=%d, unix_socket_info_lock_init=%d\n", unix_sock_track_stop_flag, unix_socket_info_lock_init);   
    if (unix_sock_track_stop_flag == 0)
    {
        if (unix_socket_info_lock_init == 0)
        {
            int i = 0;
            
            for(i = 0; i < USTK_HASH_SIZE; i++)
            {
                unix_sock_track_head[i].list_head = NULL;
                spin_lock_init(&unix_sock_track_head[i].list_lock);
                unix_sock_track_head[i].rm_close_sock_time = 0;
            }
            //spin_lock_init(&unix_dbg_info_lock);
            unix_socket_info_lock_init = 1; 
        }
      
        do_gettimeofday(&tv);

        tv.tv_sec -= sys_tz.tz_minuteswest*60;
        time_se = tv.tv_sec;
        time_ms = (unsigned long)((tv.tv_usec) / 1000);

        //printk("[usktrk] create1 time_ms =%u\n", time_ms);

        current_time_se = time_se;

        sock_infor = kzalloc(__UNIX_SOCKET_INFO_SIZE__, GFP_ATOMIC);
    
        //printk("[usktrk] create1 ion=%d\n", ino);   
        if (sock_infor != NULL)
        {
            temp_len = sizeof(unix_sock_track_blc);
            //unix_sock_blc_t = kzalloc(temp_len, GFP_KERNEL);
            unix_sock_blc_t = kzalloc(temp_len, GFP_ATOMIC);
             
            if (unix_sock_blc_t != NULL)
            {
                unix_sock_blc_t->record_info_blc = sock_infor;          
                unix_sock_track_find_blc_with_action(1, unix_sock_blc_t, ino);        
                unix_sock_blc_t->create_fd = socket_type;
                unix_sock_blc_t->nod_ino = ino;

                unix_sock_blc_t->create_pid = current->tgid;
                unix_sock_blc_t->create_tid = current->pid;
                unix_sock_blc_t->record_current_p = 0;    
                unix_sock_blc_t->record_current_sc = time_se;   
                unix_sock_blc_t->record_current_ms = (time_ms / __UNIX_SOCKET_TIME_UNIT__) * __UNIX_SOCKET_TIME_UNIT__;;    
                unix_sock_blc_t->time_sc = time_se;
                unix_sock_blc_t->time_ms = time_ms;
            }
            else
            {
            	#ifdef CONFIG_MTK_NET_LOGGING 
                printk("[mtk_net][unix] alloc unix_sock_blc fail\n");
                #endif
                kfree(sock_infor);
            }              
        }
        else
        {
        	#ifdef CONFIG_MTK_NET_LOGGING 
            printk("[mtk_net][unix]alloc sock_infor fail\n");
            #endif
        }
    }
  
    return unix_sock_blc_t;
    
}

   
void unix_sock_track_socket_create(unsigned long ino, unsigned int socket_type)
{

    unix_sock_track_blc *tmp = NULL;

    tmp = unix_sock_track_socket_create1(ino, socket_type);
    if (tmp == NULL)
    {
    	#ifdef CONFIG_MTK_NET_LOGGING 
        printk("[mtk_net][unix] unix_sock_track_socket_create1 error\n");
        #endif
    }
    
}

void unix_sock_track_socket_accept_create(unsigned long ino, unsigned int socket_type, struct socket *sock)
{

    unix_sock_track_blc *tmp = NULL;
    struct sock *sk = NULL;
    struct sock *sk_p = NULL;

    tmp = unix_sock_track_socket_create1(ino, socket_type);
    if (tmp == NULL)
    {
    	#ifdef CONFIG_MTK_NET_LOGGING 
        printk("[mtk_net][unix] create1 error\n");
        #endif
    }    
}

static void unix_socket_track_socket_pino(unix_sock_track_blc *blc, struct sock *sk)
{
    struct sock *peer_sk = NULL;
    unsigned long peer_ino = 0;
    
    if (blc != NULL)
    {
        peer_sk = unix_peer(sk);
        if (peer_sk != NULL)
        {
            if(peer_sk != NULL && peer_sk->sk_socket && SOCK_INODE(peer_sk->sk_socket))
            {
                peer_ino = SOCK_INODE(peer_sk->sk_socket)->i_ino;
            
                if (blc->peer_ino != peer_ino)
                {
                    if (blc->peer_ino != 0)
                    {
                    	#ifdef CONFIG_MTK_NET_LOGGING 
                        printk("[mtk_net][unix] prev=%ul, now=%ul\n", blc->peer_ino, peer_ino);
                        #endif
                    }
                    blc->peer_ino = peer_ino;
                }
            }
        }
    }
}

void unix_sock_track_socket_pair_create(unsigned long ino, unsigned int socket_type, unsigned long ino1, unsigned int socket_type1)
{

    unix_sock_track_blc *tmp = NULL;
    unix_sock_track_blc *tmp1 = NULL;
 
    tmp = unix_sock_track_socket_create1(ino, socket_type);
    if (tmp == NULL)
    {
    	#ifdef CONFIG_MTK_NET_LOGGING 
        printk("[mtk_net][unix] create error\n");
        #endif
    }
    else
        tmp->peer_ino = ino1;

    tmp1 = unix_sock_track_socket_create1(ino1, socket_type1);
    if (tmp1 == NULL)
    {
    	#ifdef CONFIG_MTK_NET_LOGGING 
        printk("[mtk_net][unix] create1 error\n");
        #endif
    }
    else
          tmp1->peer_ino = ino;   
 
}


static unix_sock_track_blc *blc_check = NULL;
static void unix_sock_track_fill_action(unix_sock_track_blc *blc, unsigned int action, unsigned int action_mask)
{
    unsigned long time_ms = 0;
    unsigned long time_se = 0;
    unsigned long time_offset = 0;
    unsigned long time_offset_se = 0;
    unsigned long time_offset_ms = 0;
    unsigned int  offset_byte = 0;
    char  *record_info_t = NULL;
    unsigned int  record_current_p_t = 0;
    unsigned int  recode_start_p_t = 0;
    unsigned int  recode_start_p_diff = 0;
    unsigned int  i = 0;
    unsigned int  time_wrap_around = 0;
    unsigned int  post_wrap_around = 0;
    unsigned int  update_time_info = 0;
    unsigned long flags;
    unsigned long long t;
    unsigned long nanosec_rem;     
    struct timeval tv = {0};
    
    if (unix_sock_track_stop_flag == 0)
    {
        do_gettimeofday(&tv);

        tv.tv_sec -= sys_tz.tz_minuteswest*60;
        time_se = tv.tv_sec;
        time_ms = (unsigned long)((tv.tv_usec) / 1000);

        record_info_t = blc->record_info_blc;

        //if (blc_check == NULL)
        {
            blc_check = blc;
        }

        if (record_info_t != NULL)
        {
        
            if ((blc->record_current_ms == 0) && (blc->record_current_sc == 0))
            {
            	#ifdef CONFIG_MTK_NET_LOGGING 
                printk("[mtk_net][unix], nod=%u, error happen\n", blc->nod_ino);
                #endif

            }
            else
            {
        
                // get time offset
                if (time_se < blc->record_current_sc)
                {
                    time_offset_se = ((unsigned long)(-1)) - blc->record_current_sc + time_se;
                }
                else
                {
                    time_offset_se = time_se - blc->record_current_sc;
                }

                if (time_offset_se > 0)
                {
                    if (time_ms < blc->record_current_ms)
                    {
                        time_offset_se = time_offset_se - 1;

                        time_offset_ms = time_ms + 1000 - blc->record_current_ms;
                    }
                    else
                    {
                        time_offset_ms = time_ms - blc->record_current_ms;
                    }
                }
                else
                {
                    if (time_ms < blc->record_current_ms)
                    {
                    	#ifdef CONFIG_MTK_NET_LOGGING 
                        printk("[mtk_net][unix] time error happen \n");
                        #endif
                        return;
                    }
                    else
                    {
                        time_offset_ms = time_ms - blc->record_current_ms;
                    }
                }

                time_offset = time_offset_se * 1000 + time_offset_ms;
        
                if (time_offset >= __UNIX_SOCKET_TIME_UNIT__ * __UNIX_SOCKET_INFO_SIZE__)
                {   
                    if((blc->record_info_blc == NULL) || (blc->close_time_sc != 0))
                    {
                        return;
                    }
                    //printk("[usktrk] nod=%u time> max save \n", blc->nod_ino);
                    memset(record_info_t, 0, sizeof(__UNIX_SOCKET_INFO_SIZE__));    
                    //blc->recode_wrap_around = 1;   
                    update_time_info = 1;    
                    blc->record_current_p = 0;     
                    blc->record_current_sc = time_se;  
                    blc->record_current_ms = (time_ms / __UNIX_SOCKET_TIME_UNIT__) * __UNIX_SOCKET_TIME_UNIT__;  

                }
                else if (time_offset >= __UNIX_SOCKET_TIME_UNIT__)
                {
                    //get bytes offset
                    offset_byte = time_offset / __UNIX_SOCKET_TIME_UNIT__;

                    {  
                        unsigned int tmp = blc->record_current_p;
                
                        for(i = 1; i <= offset_byte; i++)
                        {
                            tmp++;
                            //record_info_t[tmp] = 0;
                            blc->record_current_ms = blc->record_current_ms + __UNIX_SOCKET_TIME_UNIT__;
                            if ( blc->record_current_ms >= 1000)
                            {
                                blc->record_current_sc = blc->record_current_sc + 1;
                                blc->record_current_ms = blc->record_current_ms - 1000;
                            }
                            if (tmp >= __UNIX_SOCKET_INFO_SIZE__)
                            {
                                //printk("[usktrk] nod=%u new pos> max save \n", blc->nod_ino);
                                tmp = 0;
                                blc->recode_wrap_around = 1;
                                update_time_info = 1;                       
                            }
                            
				                    if((blc->record_info_blc == NULL) || (blc->close_time_sc != 0))
				                    {
				                        return;
				                    }                               
                            record_info_t[tmp] = 0;
                        }
                
                        blc->record_current_p = tmp;
                    }       
                }
            }

            if (current->tgid != blc->create_pid)
            {
                if (blc->used_pid != current->tgid)
                {
                    if((blc->record_info_blc != NULL) && (blc->close_time_sc == 0))
                    {
                        blc->used_pid = current->tgid;
                    }
                }
                
            }   

            if (current->pid != blc->create_tid)
            {
                if (blc->used_tid != current->pid)
                {
                    if((blc->record_info_blc != NULL) && (blc->close_time_sc == 0))
                    {
                        blc->used_tid = current->pid;
                    }
                } 
            }
            
            if((blc->record_info_blc == NULL) || (blc->close_time_sc != 0))
            {
                return;
            }        
            record_info_t[blc->record_current_p] = record_info_t[blc->record_current_p] & (~action_mask);
            record_info_t[blc->record_current_p] = record_info_t[blc->record_current_p] | action;   
            //printk("[usktrk]new record_current_p=%u record_info_t=0x%x\n", blc->record_current_p, record_info_t[blc->record_current_p]);    
        }    
    }
}
#endif/* CONFIG_UNIX_SOCKET_TRACK_TOOL */


#ifdef CONFIG_SECURITY_NETWORK
static void unix_get_secdata(struct scm_cookie *scm, struct sk_buff *skb)
{
	memcpy(UNIXSID(skb), &scm->secid, sizeof(u32));
}

static inline void unix_set_secdata(struct scm_cookie *scm, struct sk_buff *skb)
{
	scm->secid = *UNIXSID(skb);
}
#else
static inline void unix_get_secdata(struct scm_cookie *scm, struct sk_buff *skb)
{ }

static inline void unix_set_secdata(struct scm_cookie *scm, struct sk_buff *skb)
{ }
#endif /* CONFIG_SECURITY_NETWORK */

/*
 *  SMP locking strategy:
 *    hash table is protected with spinlock unix_table_lock
 *    each socket state is protected by separate spin lock.
 */

static inline unsigned int unix_hash_fold(__wsum n)
{
	unsigned int hash = (__force unsigned int)csum_fold(n);

	hash ^= hash>>8;
	return hash&(UNIX_HASH_SIZE-1);
}



static inline int unix_our_peer(struct sock *sk, struct sock *osk)
{
	return unix_peer(osk) == sk;
}

static inline int unix_may_send(struct sock *sk, struct sock *osk)
{
	return unix_peer(osk) == NULL || unix_our_peer(sk, osk);
}

static inline int unix_recvq_full(struct sock const *sk)
{
	return skb_queue_len(&sk->sk_receive_queue) > sk->sk_max_ack_backlog;
}

struct sock *unix_peer_get(struct sock *s)
{
	struct sock *peer;

	unix_state_lock(s);
	peer = unix_peer(s);
	if (peer)
		sock_hold(peer);
	unix_state_unlock(s);
	return peer;
}
EXPORT_SYMBOL_GPL(unix_peer_get);

static inline void unix_release_addr(struct unix_address *addr)
{
	if (atomic_dec_and_test(&addr->refcnt))
		kfree(addr);
}

/*
 *	Check unix socket name:
 *		- should be not zero length.
 *	        - if started by not zero, should be NULL terminated (FS object)
 *		- if started by zero, it is abstract name.
 */

static int unix_mkname(struct sockaddr_un *sunaddr, int len, unsigned int *hashp)
{
	if (len <= sizeof(short) || len > sizeof(*sunaddr))
		return -EINVAL;
	if (!sunaddr || sunaddr->sun_family != AF_UNIX)
		return -EINVAL;
	if (sunaddr->sun_path[0]) {
		/*
		 * This may look like an off by one error but it is a bit more
		 * subtle. 108 is the longest valid AF_UNIX path for a binding.
		 * sun_path[108] doesn't as such exist.  However in kernel space
		 * we are guaranteed that it is a valid memory location in our
		 * kernel address buffer.
		 */
		((char *)sunaddr)[len] = 0;
		len = strlen(sunaddr->sun_path)+1+sizeof(short);
		return len;
	}

	*hashp = unix_hash_fold(csum_partial(sunaddr, len, 0));
	return len;
}

static void __unix_remove_socket(struct sock *sk)
{
	sk_del_node_init(sk);
}

static void __unix_insert_socket(struct hlist_head *list, struct sock *sk)
{
	WARN_ON(!sk_unhashed(sk));
	sk_add_node(sk, list);
}

static inline void unix_remove_socket(struct sock *sk)
{
	spin_lock(&unix_table_lock);
	__unix_remove_socket(sk);
	spin_unlock(&unix_table_lock);
}

static inline void unix_insert_socket(struct hlist_head *list, struct sock *sk)
{
	spin_lock(&unix_table_lock);
	__unix_insert_socket(list, sk);
	spin_unlock(&unix_table_lock);
}

static struct sock *__unix_find_socket_byname(struct net *net,
					      struct sockaddr_un *sunname,
					      int len, int type, unsigned int hash)
{
	struct sock *s;

	sk_for_each(s, &unix_socket_table[hash ^ type]) {
		struct unix_sock *u = unix_sk(s);

		if (!net_eq(sock_net(s), net))
			continue;

		if (u->addr->len == len &&
		    !memcmp(u->addr->name, sunname, len))
			goto found;
	}
	s = NULL;
found:
	return s;
}

static inline struct sock *unix_find_socket_byname(struct net *net,
						   struct sockaddr_un *sunname,
						   int len, int type,
						   unsigned int hash)
{
	struct sock *s;

	spin_lock(&unix_table_lock);
	s = __unix_find_socket_byname(net, sunname, len, type, hash);
	if (s)
		sock_hold(s);
	spin_unlock(&unix_table_lock);
	return s;
}

static struct sock *unix_find_socket_byinode(struct inode *i)
{
	struct sock *s;

	spin_lock(&unix_table_lock);
	sk_for_each(s,
		    &unix_socket_table[i->i_ino & (UNIX_HASH_SIZE - 1)]) {
		struct dentry *dentry = unix_sk(s)->path.dentry;

		if (dentry && dentry->d_inode == i) {
			sock_hold(s);
			goto found;
		}
	}
	s = NULL;
found:
	spin_unlock(&unix_table_lock);
	return s;
}

static inline int unix_writable(struct sock *sk)
{
	return (atomic_read(&sk->sk_wmem_alloc) << 2) <= sk->sk_sndbuf;
}

static void unix_write_space(struct sock *sk)
{
	struct socket_wq *wq;

	rcu_read_lock();
	if (unix_writable(sk)) {
		wq = rcu_dereference(sk->sk_wq);
		if (wq_has_sleeper(wq))
			wake_up_interruptible_sync_poll(&wq->wait,
				POLLOUT | POLLWRNORM | POLLWRBAND);
		sk_wake_async(sk, SOCK_WAKE_SPACE, POLL_OUT);
	}
	rcu_read_unlock();
}

/* When dgram socket disconnects (or changes its peer), we clear its receive
 * queue of packets arrived from previous peer. First, it allows to do
 * flow control based only on wmem_alloc; second, sk connected to peer
 * may receive messages only from that peer. */
static void unix_dgram_disconnected(struct sock *sk, struct sock *other)
{
	if (!skb_queue_empty(&sk->sk_receive_queue)) {
		skb_queue_purge(&sk->sk_receive_queue);
		wake_up_interruptible_all(&unix_sk(sk)->peer_wait);

		/* If one link of bidirectional dgram pipe is disconnected,
		 * we signal error. Messages are lost. Do not make this,
		 * when peer was not connected to us.
		 */
		if (!sock_flag(other, SOCK_DEAD) && unix_peer(other) == sk) {
			other->sk_err = ECONNRESET;
			other->sk_error_report(other);
		}
	}
}

static void unix_sock_destructor(struct sock *sk)
{
	struct unix_sock *u = unix_sk(sk);

	skb_queue_purge(&sk->sk_receive_queue);

	WARN_ON(atomic_read(&sk->sk_wmem_alloc));
	WARN_ON(!sk_unhashed(sk));
	WARN_ON(sk->sk_socket);
	if (!sock_flag(sk, SOCK_DEAD)) {
		#ifdef CONFIG_MTK_NET_LOGGING 
		printk(KERN_INFO "[mtk_net][unix]Attempt to release alive unix socket: %p\n", sk);
		#endif
		return;
	}

	if (u->addr)
		unix_release_addr(u->addr);

	atomic_long_dec(&unix_nr_socks);
	local_bh_disable();
	sock_prot_inuse_add(sock_net(sk), sk->sk_prot, -1);
	local_bh_enable();
    #ifdef UNIX_REFCNT_DEBUG
	printk(KERN_DEBUG "[mtk_net][unix]UNIX %p is destroyed, %ld are still alive.\n", sk,
		atomic_long_read(&unix_nr_socks));
    #endif
}

static void unix_release_sock(struct sock *sk, int embrion)
{
	struct unix_sock *u = unix_sk(sk);
	struct path path;
	struct sock *skpair;
	struct sk_buff *skb;
	int state;

	unix_remove_socket(sk);

	/* Clear state */
	unix_state_lock(sk);
	sock_orphan(sk);
	sk->sk_shutdown = SHUTDOWN_MASK;
	path	     = u->path;
	u->path.dentry = NULL;
	u->path.mnt = NULL;
	state = sk->sk_state;
	sk->sk_state = TCP_CLOSE;
	unix_state_unlock(sk);

	wake_up_interruptible_all(&u->peer_wait);

	skpair = unix_peer(sk);

	if (skpair != NULL) {
		if (sk->sk_type == SOCK_STREAM || sk->sk_type == SOCK_SEQPACKET) {
			unix_state_lock(skpair);
			/* No more writes */
			skpair->sk_shutdown = SHUTDOWN_MASK;
			if (!skb_queue_empty(&sk->sk_receive_queue) || embrion)
				skpair->sk_err = ECONNRESET;
			unix_state_unlock(skpair);
			skpair->sk_state_change(skpair);
			sk_wake_async(skpair, SOCK_WAKE_WAITD, POLL_HUP);
		}
		sock_put(skpair); /* It may now die */
		unix_peer(sk) = NULL;
	}

	/* Try to flush out this socket. Throw out buffers at least */

	while ((skb = skb_dequeue(&sk->sk_receive_queue)) != NULL) {
		if (state == TCP_LISTEN)
			unix_release_sock(skb->sk, 1);
		/* passed fds are erased in the kfree_skb hook	      */
		kfree_skb(skb);
	}

	if (path.dentry)
		path_put(&path);

	sock_put(sk);

	/* ---- Socket is dead now and most probably destroyed ---- */

	/*
	 * Fixme: BSD difference: In BSD all sockets connected to us get
	 *	  ECONNRESET and we die on the spot. In Linux we behave
	 *	  like files and pipes do and wait for the last
	 *	  dereference.
	 *
	 * Can't we simply set sock->err?
	 *
	 *	  What the above comment does talk about? --ANK(980817)
	 */

	if (unix_tot_inflight)
		unix_gc();		/* Garbage collect fds */
}

static void init_peercred(struct sock *sk)
{
	put_pid(sk->sk_peer_pid);
	if (sk->sk_peer_cred)
		put_cred(sk->sk_peer_cred);
	sk->sk_peer_pid  = get_pid(task_tgid(current));
	sk->sk_peer_cred = get_current_cred();
}

static void copy_peercred(struct sock *sk, struct sock *peersk)
{
	put_pid(sk->sk_peer_pid);
	if (sk->sk_peer_cred)
		put_cred(sk->sk_peer_cred);
	sk->sk_peer_pid  = get_pid(peersk->sk_peer_pid);
	sk->sk_peer_cred = get_cred(peersk->sk_peer_cred);
}

static int unix_listen(struct socket *sock, int backlog)
{
	int err;
	struct sock *sk = sock->sk;
	struct unix_sock *u = unix_sk(sk);
	struct pid *old_pid = NULL;
#ifdef CONFIG_UNIX_SOCKET_TRACK_TOOL	
    //////////////add code begin///////////////////
    unix_sock_track_blc* unix_sock_track_tmp = NULL;
    struct timeval tv = {0};
    //////////////add code end/////////////////// 
#endif/* CONFIG_UNIX_SOCKET_TRACK_TOOL */

	err = -EOPNOTSUPP;
	if (sock->type != SOCK_STREAM && sock->type != SOCK_SEQPACKET)
		goto out;	/* Only stream/seqpacket sockets accept */
	err = -EINVAL;
	if (!u->addr)
		goto out;	/* No listens on an unbound socket */
	unix_state_lock(sk);
	if (sk->sk_state != TCP_CLOSE && sk->sk_state != TCP_LISTEN)
		goto out_unlock;
	if (backlog > sk->sk_max_ack_backlog)
		wake_up_interruptible_all(&u->peer_wait);
	sk->sk_max_ack_backlog	= backlog;
	sk->sk_state		= TCP_LISTEN;
	/* set credentials so connect can copy them */
	init_peercred(sk);
	err = 0;

out_unlock:
	unix_state_unlock(sk);
	put_pid(old_pid);
out:
#ifdef CONFIG_UNIX_SOCKET_TRACK_TOOL	
    //////////////add code begin///////////////////  
    //unix_sock_track_tmp = unix_sock_track_get_blc_by_ino(SOCK_INODE(sock)->i_ino);
    if (SOCK_INODE(sock))
    {
        unix_sock_track_tmp = unix_sock_track_find_blc_with_action(2, NULL, SOCK_INODE(sock)->i_ino);
    }
    //unix_sock_track_tmp = unix_sock_track_find_blc_with_action(2, NULL, SOCK_INODE(sock)->i_ino);

    if (unix_sock_track_tmp != NULL)
    {
        if (err == 0)
        {
            unsigned long long t;
            unsigned long nanosec_rem;        

            //get current time ms
            do_gettimeofday(&tv);

            tv.tv_sec -= sys_tz.tz_minuteswest*60;  
      
            if((unix_sock_track_tmp->record_info_blc != NULL) && (unix_sock_track_tmp->close_time_sc == 0))
            {
                unix_sock_track_tmp->listen = backlog;
                unix_sock_track_tmp->listen_time_sc = tv.tv_sec;
                unix_sock_track_tmp->listen_time_ms = (unsigned long)((tv.tv_usec) / 1000);
            }
        }
        else
        {
        	  if((unix_sock_track_tmp->record_info_blc != NULL) && (unix_sock_track_tmp->close_time_sc == 0))
        	  {
                unix_sock_track_tmp->error_flag = err;
            }
        }
    }
    //////////////add code end///////////////////    
#endif/* CONFIG_UNIX_SOCKET_TRACK_TOOL */     
	return err;
}

static int unix_release(struct socket *);
static int unix_bind(struct socket *, struct sockaddr *, int);
static int unix_stream_connect(struct socket *, struct sockaddr *,
			       int addr_len, int flags);
static int unix_socketpair(struct socket *, struct socket *);
static int unix_accept(struct socket *, struct socket *, int);
static int unix_getname(struct socket *, struct sockaddr *, int *, int);
static unsigned int unix_poll(struct file *, struct socket *, poll_table *);
static unsigned int unix_dgram_poll(struct file *, struct socket *,
				    poll_table *);
static int unix_ioctl(struct socket *, unsigned int, unsigned long);
static int unix_shutdown(struct socket *, int);
static int unix_stream_sendmsg(struct kiocb *, struct socket *,
			       struct msghdr *, size_t);
static int unix_stream_recvmsg(struct kiocb *, struct socket *,
			       struct msghdr *, size_t, int);
static int unix_dgram_sendmsg(struct kiocb *, struct socket *,
			      struct msghdr *, size_t);
static int unix_dgram_recvmsg(struct kiocb *, struct socket *,
			      struct msghdr *, size_t, int);
static int unix_dgram_connect(struct socket *, struct sockaddr *,
			      int, int);
static int unix_seqpacket_sendmsg(struct kiocb *, struct socket *,
				  struct msghdr *, size_t);
static int unix_seqpacket_recvmsg(struct kiocb *, struct socket *,
				  struct msghdr *, size_t, int);

static int unix_set_peek_off(struct sock *sk, int val)
{
	struct unix_sock *u = unix_sk(sk);

	if (mutex_lock_interruptible(&u->readlock))
		return -EINTR;

	sk->sk_peek_off = val;
	mutex_unlock(&u->readlock);

	return 0;
}


static const struct proto_ops unix_stream_ops = {
	.family =	PF_UNIX,
	.owner =	THIS_MODULE,
	.release =	unix_release,
	.bind =		unix_bind,
	.connect =	unix_stream_connect,
	.socketpair =	unix_socketpair,
	.accept =	unix_accept,
	.getname =	unix_getname,
	.poll =		unix_poll,
	.ioctl =	unix_ioctl,
	.listen =	unix_listen,
	.shutdown =	unix_shutdown,
	.setsockopt =	sock_no_setsockopt,
	.getsockopt =	sock_no_getsockopt,
	.sendmsg =	unix_stream_sendmsg,
	.recvmsg =	unix_stream_recvmsg,
	.mmap =		sock_no_mmap,
	.sendpage =	sock_no_sendpage,
	.set_peek_off =	unix_set_peek_off,
};

static const struct proto_ops unix_dgram_ops = {
	.family =	PF_UNIX,
	.owner =	THIS_MODULE,
	.release =	unix_release,
	.bind =		unix_bind,
	.connect =	unix_dgram_connect,
	.socketpair =	unix_socketpair,
	.accept =	sock_no_accept,
	.getname =	unix_getname,
	.poll =		unix_dgram_poll,
	.ioctl =	unix_ioctl,
	.listen =	sock_no_listen,
	.shutdown =	unix_shutdown,
	.setsockopt =	sock_no_setsockopt,
	.getsockopt =	sock_no_getsockopt,
	.sendmsg =	unix_dgram_sendmsg,
	.recvmsg =	unix_dgram_recvmsg,
	.mmap =		sock_no_mmap,
	.sendpage =	sock_no_sendpage,
	.set_peek_off =	unix_set_peek_off,
};

static const struct proto_ops unix_seqpacket_ops = {
	.family =	PF_UNIX,
	.owner =	THIS_MODULE,
	.release =	unix_release,
	.bind =		unix_bind,
	.connect =	unix_stream_connect,
	.socketpair =	unix_socketpair,
	.accept =	unix_accept,
	.getname =	unix_getname,
	.poll =		unix_dgram_poll,
	.ioctl =	unix_ioctl,
	.listen =	unix_listen,
	.shutdown =	unix_shutdown,
	.setsockopt =	sock_no_setsockopt,
	.getsockopt =	sock_no_getsockopt,
	.sendmsg =	unix_seqpacket_sendmsg,
	.recvmsg =	unix_seqpacket_recvmsg,
	.mmap =		sock_no_mmap,
	.sendpage =	sock_no_sendpage,
	.set_peek_off =	unix_set_peek_off,
};

static struct proto unix_proto = {
	.name			= "UNIX",
	.owner			= THIS_MODULE,
	.obj_size		= sizeof(struct unix_sock),
};

/*
 * AF_UNIX sockets do not interact with hardware, hence they
 * dont trigger interrupts - so it's safe for them to have
 * bh-unsafe locking for their sk_receive_queue.lock. Split off
 * this special lock-class by reinitializing the spinlock key:
 */
static struct lock_class_key af_unix_sk_receive_queue_lock_key;

static struct sock *unix_create1(struct net *net, struct socket *sock)
{
	struct sock *sk = NULL;
	struct unix_sock *u;

	atomic_long_inc(&unix_nr_socks);
	if (atomic_long_read(&unix_nr_socks) > 2 * get_max_files())
		goto out;

	sk = sk_alloc(net, PF_UNIX, GFP_KERNEL, &unix_proto);
	if (!sk)
		goto out;

	sock_init_data(sock, sk);
	lockdep_set_class(&sk->sk_receive_queue.lock,
				&af_unix_sk_receive_queue_lock_key);

	sk->sk_write_space	= unix_write_space;
	sk->sk_max_ack_backlog	= net->unx.sysctl_max_dgram_qlen;
	sk->sk_destruct		= unix_sock_destructor;
	u	  = unix_sk(sk);
	u->path.dentry = NULL;
	u->path.mnt = NULL;
	spin_lock_init(&u->lock);
	atomic_long_set(&u->inflight, 0);
	INIT_LIST_HEAD(&u->link);
	mutex_init(&u->readlock); /* single task reading lock */
	init_waitqueue_head(&u->peer_wait);
	unix_insert_socket(unix_sockets_unbound(sk), sk);
out:
	if (sk == NULL)
		atomic_long_dec(&unix_nr_socks);
	else {
		local_bh_disable();
		sock_prot_inuse_add(sock_net(sk), sk->sk_prot, 1);
		local_bh_enable();
	}
	return sk;
}

static int unix_create(struct net *net, struct socket *sock, int protocol,
		       int kern)
{
	if (protocol && protocol != PF_UNIX)
		return -EPROTONOSUPPORT;

	sock->state = SS_UNCONNECTED;

	switch (sock->type) {
	case SOCK_STREAM:
		sock->ops = &unix_stream_ops;
		break;
		/*
		 *	Believe it or not BSD has AF_UNIX, SOCK_RAW though
		 *	nothing uses it.
		 */
	case SOCK_RAW:
		sock->type = SOCK_DGRAM;
	case SOCK_DGRAM:
		sock->ops = &unix_dgram_ops;
		break;
	case SOCK_SEQPACKET:
		sock->ops = &unix_seqpacket_ops;
		break;
	default:
		return -ESOCKTNOSUPPORT;
	}

	return unix_create1(net, sock) ? 0 : -ENOMEM;
}

static int unix_release(struct socket *sock)
{
	struct sock *sk = sock->sk;
#ifdef CONFIG_UNIX_SOCKET_TRACK_TOOL	
    //////////////add code begin///////////////////
    unix_sock_track_blc* unix_sock_track_tmp = NULL;
    //////////////add code end///////////////////   
#endif/* CONFIG_UNIX_SOCKET_TRACK_TOOL */

	if (!sk)
		return 0;
#ifdef CONFIG_UNIX_SOCKET_TRACK_TOOL
    //////////////add code begin///////////////////  
    if (SOCK_INODE(sock))
    {
        unix_sock_track_tmp = unix_sock_track_find_blc_with_action(2, NULL, SOCK_INODE(sock)->i_ino);
    }

    if (unix_sock_track_tmp != NULL)
    {
        unsigned long long t;
        unsigned long nanosec_rem;  
        struct timeval tv = {0};

        do_gettimeofday(&tv);

        tv.tv_sec -= sys_tz.tz_minuteswest*60;
        unix_sock_track_tmp->close_time_sc = tv.tv_sec;
        unix_sock_track_tmp->close_time_ms = (unsigned long)((tv.tv_usec) / 1000);

        unix_sock_track_tmp->shutdown_state = SHUTDOWN_MASK;
        if (unix_sock_track_tmp->record_info_blc != NULL)
        {
        	  unsigned char   *tmp1 = NULL;
        	  tmp1 = unix_sock_track_tmp->record_info_blc;
        	  unix_sock_track_tmp->record_info_blc = NULL;
            kfree(tmp1);
        }
        //printk("[usktrk] 1 socket close[%lu] \n",unix_sock_track_tmp->nod_ino); 
        
        unix_sock_track_rm_close_sock(SOCK_INODE(sock)->i_ino);
    }

    //////////////add code end/////////////////// 
#endif/* CONFIG_UNIX_SOCKET_TRACK_TOOL */
	unix_release_sock(sk, 0);
	sock->sk = NULL;

	return 0;
}

static int unix_autobind(struct socket *sock)
{
	struct sock *sk = sock->sk;
	struct net *net = sock_net(sk);
	struct unix_sock *u = unix_sk(sk);
	static u32 ordernum = 1;
	struct unix_address *addr;
	int err;
	unsigned int retries = 0;

	err = mutex_lock_interruptible(&u->readlock);
	if (err)
		return err;

	err = 0;
	if (u->addr)
		goto out;

	err = -ENOMEM;
	addr = kzalloc(sizeof(*addr) + sizeof(short) + 16, GFP_KERNEL);
	if (!addr)
		goto out;

	addr->name->sun_family = AF_UNIX;
	atomic_set(&addr->refcnt, 1);

retry:
	addr->len = sprintf(addr->name->sun_path+1, "%05x", ordernum) + 1 + sizeof(short);
	addr->hash = unix_hash_fold(csum_partial(addr->name, addr->len, 0));

	spin_lock(&unix_table_lock);
	ordernum = (ordernum+1)&0xFFFFF;

	if (__unix_find_socket_byname(net, addr->name, addr->len, sock->type,
				      addr->hash)) {
		spin_unlock(&unix_table_lock);
		/*
		 * __unix_find_socket_byname() may take long time if many names
		 * are already in use.
		 */
		cond_resched();
		/* Give up if all names seems to be in use. */
		if (retries++ == 0xFFFFF) {
			err = -ENOSPC;
			kfree(addr);
			goto out;
		}
		goto retry;
	}
	addr->hash ^= sk->sk_type;

	__unix_remove_socket(sk);
	u->addr = addr;
	__unix_insert_socket(&unix_socket_table[addr->hash], sk);
	spin_unlock(&unix_table_lock);
	err = 0;

out:	mutex_unlock(&u->readlock);
	return err;
}

static struct sock *unix_find_other(struct net *net,
				    struct sockaddr_un *sunname, int len,
				    int type, unsigned int hash, int *error)
{
	struct sock *u;
	struct path path;
	int err = 0;

	if (sunname->sun_path[0]) {
		struct inode *inode;
		err = kern_path(sunname->sun_path, LOOKUP_FOLLOW, &path);
		if (err)
			goto fail;
		inode = path.dentry->d_inode;
		err = inode_permission(inode, MAY_WRITE);
		if (err)
			goto put_fail;

		err = -ECONNREFUSED;
		if (!S_ISSOCK(inode->i_mode))
			goto put_fail;
		u = unix_find_socket_byinode(inode);
		if (!u)
			goto put_fail;

		if (u->sk_type == type)
			touch_atime(&path);

		path_put(&path);

		err = -EPROTOTYPE;
		if (u->sk_type != type) {
			sock_put(u);
			goto fail;
		}
	} else {
		err = -ECONNREFUSED;
		u = unix_find_socket_byname(net, sunname, len, type, hash);
		if (u) {
			struct dentry *dentry;
			dentry = unix_sk(u)->path.dentry;
			if (dentry)
				touch_atime(&unix_sk(u)->path);
		} else
			goto fail;
	}
	return u;

put_fail:
	path_put(&path);
fail:
	*error = err;
	return NULL;
}

static int unix_mknod(const char *sun_path, umode_t mode, struct path *res)
{
	struct dentry *dentry;
	struct path path;
	int err = 0;
	/*
	 * Get the parent directory, calculate the hash for last
	 * component.
	 */
	dentry = kern_path_create(AT_FDCWD, sun_path, &path, 0);
	err = PTR_ERR(dentry);
	if (IS_ERR(dentry))
		return err;

	/*
	 * All right, let's create it.
	 */
	err = security_path_mknod(&path, dentry, mode, 0);
	if (!err) {
		err = vfs_mknod(path.dentry->d_inode, dentry, mode, 0);
		if (!err) {
			res->mnt = mntget(path.mnt);
			res->dentry = dget(dentry);
		}
	}
	done_path_create(&path, dentry);
	return err;
}

static int unix_bind(struct socket *sock, struct sockaddr *uaddr, int addr_len)
{
	struct sock *sk = sock->sk;
	struct net *net = sock_net(sk);
	struct unix_sock *u = unix_sk(sk);
	struct sockaddr_un *sunaddr = (struct sockaddr_un *)uaddr;
	char *sun_path = sunaddr->sun_path;
	int err;
	unsigned int hash;
	struct unix_address *addr;
	struct hlist_head *list;
#ifdef CONFIG_UNIX_SOCKET_TRACK_TOOL	
    //////////////add code begin///////////////////
    unix_sock_track_blc* unix_sock_track_tmp = NULL;
    //////////////add code end///////////////////
#endif/* CONFIG_UNIX_SOCKET_TRACK_TOOL */
	err = -EINVAL;
	if (sunaddr->sun_family != AF_UNIX)
		goto out;

	if (addr_len == sizeof(short)) {
		err = unix_autobind(sock);
		goto out;
	}

	err = unix_mkname(sunaddr, addr_len, &hash);
	if (err < 0)
		goto out;
	addr_len = err;

	err = mutex_lock_interruptible(&u->readlock);
	if (err)
		goto out;

	err = -EINVAL;
	if (u->addr)
		goto out_up;

	err = -ENOMEM;
	addr = kmalloc(sizeof(*addr)+addr_len, GFP_KERNEL);
	if (!addr)
		goto out_up;

	memcpy(addr->name, sunaddr, addr_len);
	addr->len = addr_len;
	addr->hash = hash ^ sk->sk_type;
	atomic_set(&addr->refcnt, 1);

	if (sun_path[0]) {
		struct path path;
#ifdef CONFIG_UNIX_SOCKET_TRACK_TOOL
        //////////////add code begin///////////////////  

        if (SOCK_INODE(sock))
        {
            unix_sock_track_tmp = unix_sock_track_find_blc_with_action(2, NULL, SOCK_INODE(sock)->i_ino);
        }        

        if (unix_sock_track_tmp != NULL)
        {
            if (addr->len > 15)
            {
                char *tt;

                tt = sun_path + (addr->len - 15);
                memcpy(unix_sock_track_tmp->unix_address, tt, 15);
                unix_sock_track_tmp->unix_address[15] = 0;
            }
            else
            {
                memcpy(unix_sock_track_tmp->unix_address, sun_path, addr->len);
                unix_sock_track_tmp->unix_address[addr->len] = 0;
            }  
        }  
        //////////////add code end///////////////////     
#endif/* CONFIG_UNIX_SOCKET_TRACK_TOOL */        

		umode_t mode = S_IFSOCK |
		       (SOCK_INODE(sock)->i_mode & ~current_umask());
		err = unix_mknod(sun_path, mode, &path);
		if (err) {
			if (err == -EEXIST)
				err = -EADDRINUSE;
			unix_release_addr(addr);
			goto out_up;
		}
		addr->hash = UNIX_HASH_SIZE;
		hash = path.dentry->d_inode->i_ino & (UNIX_HASH_SIZE-1);
		spin_lock(&unix_table_lock);
		u->path = path;
		list = &unix_socket_table[hash];
	} else {
		spin_lock(&unix_table_lock);
		err = -EADDRINUSE;
		if (__unix_find_socket_byname(net, sunaddr, addr_len,
					      sk->sk_type, hash)) {
			unix_release_addr(addr);
			goto out_unlock;
		}

		list = &unix_socket_table[addr->hash];
	}

	err = 0;
	__unix_remove_socket(sk);
	u->addr = addr;
	__unix_insert_socket(list, sk);

out_unlock:
	spin_unlock(&unix_table_lock);
out_up:
	mutex_unlock(&u->readlock);
out:
#ifdef CONFIG_UNIX_SOCKET_TRACK_TOOL
    //////////////add code begin///////////////////  
    if (err < 0)
    {
        if (err != -EAGAIN)
        {
            if (unix_sock_track_tmp != NULL)
            {
                // record this error. this error could not recover.  
                if((unix_sock_track_tmp->record_info_blc != NULL) && (unix_sock_track_tmp->close_time_sc == 0))
                {
                    unix_sock_track_tmp->error_flag = err;
                }
            }
        }
    }

    //////////////add code end/////////////////// 
#endif/* CONFIG_UNIX_SOCKET_TRACK_TOOL */   
	return err;
}

static void unix_state_double_lock(struct sock *sk1, struct sock *sk2)
{
	if (unlikely(sk1 == sk2) || !sk2) {
		unix_state_lock(sk1);
		return;
	}
	if (sk1 < sk2) {
		unix_state_lock(sk1);
		unix_state_lock_nested(sk2);
	} else {
		unix_state_lock(sk2);
		unix_state_lock_nested(sk1);
	}
}

static void unix_state_double_unlock(struct sock *sk1, struct sock *sk2)
{
	if (unlikely(sk1 == sk2) || !sk2) {
		unix_state_unlock(sk1);
		return;
	}
	unix_state_unlock(sk1);
	unix_state_unlock(sk2);
}

static int unix_dgram_connect(struct socket *sock, struct sockaddr *addr,
			      int alen, int flags)
{
	struct sock *sk = sock->sk;
	struct net *net = sock_net(sk);
	struct sockaddr_un *sunaddr = (struct sockaddr_un *)addr;
	struct sock *other;
	unsigned int hash;
	int err;
#ifdef CONFIG_UNIX_SOCKET_TRACK_TOOL	
    //////////////add code begin///////////////////
    unix_sock_track_blc* unix_sock_track_tmp = NULL;
    //////////////add code end///////////////////
#endif/* CONFIG_UNIX_SOCKET_TRACK_TOOL */

	if (addr->sa_family != AF_UNSPEC) {
#ifdef CONFIG_UNIX_SOCKET_TRACK_TOOL		
    //////////////add code begin///////////////////  
    if (SOCK_INODE(sock))
    {
        unix_sock_track_tmp = unix_sock_track_find_blc_with_action(2, NULL, SOCK_INODE(sock)->i_ino);
    }
         
    if (unix_sock_track_tmp != NULL)
    {
        unix_sock_track_fill_action(unix_sock_track_tmp, SK_AW, SK_ACCEPT_MASK);
    }
    //////////////add code end///////////////////  
#endif/* CONFIG_UNIX_SOCKET_TRACK_TOOL */
     
		err = unix_mkname(sunaddr, alen, &hash);
		if (err < 0)
			goto out;
		alen = err;

		if (test_bit(SOCK_PASSCRED, &sock->flags) &&
		    !unix_sk(sk)->addr && (err = unix_autobind(sock)) != 0)
			goto out;

restart:
		other = unix_find_other(net, sunaddr, alen, sock->type, hash, &err);
		if (!other)
			goto out;

		unix_state_double_lock(sk, other);

		/* Apparently VFS overslept socket death. Retry. */
		if (sock_flag(other, SOCK_DEAD)) {
			unix_state_double_unlock(sk, other);
			sock_put(other);
			goto restart;
		}

		err = -EPERM;
		if (!unix_may_send(sk, other))
			goto out_unlock;

		err = security_unix_may_send(sk->sk_socket, other->sk_socket);
		if (err)
			goto out_unlock;

	} else {
		/*
		 *	1003.1g breaking connected state with AF_UNSPEC
		 */
		other = NULL;
		unix_state_double_lock(sk, other);
	}

	/*
	 * If it was connected, reconnect.
	 */
	if (unix_peer(sk)) {
		struct sock *old_peer = unix_peer(sk);
		unix_peer(sk) = other;
		unix_state_double_unlock(sk, other);

		if (other != old_peer)
			unix_dgram_disconnected(sk, old_peer);
		sock_put(old_peer);
	} else {
		unix_peer(sk) = other;
		unix_state_double_unlock(sk, other);
	}
#ifdef CONFIG_UNIX_SOCKET_TRACK_TOOL	
    //////////////add code begin///////////////////  
    if (unix_sock_track_tmp != NULL)
    {
        unix_sock_track_fill_action(unix_sock_track_tmp, SK_AT, SK_ACCEPT_MASK);
    }
    //////////////add code end/////////////////// 
#endif/* CONFIG_UNIX_SOCKET_TRACK_TOOL */
            
	return 0;

out_unlock:
	unix_state_double_unlock(sk, other);
	sock_put(other);
out:
#ifdef CONFIG_UNIX_SOCKET_TRACK_TOOL	
    //////////////add code begin///////////////////  
    if (unix_sock_track_tmp != NULL)
    {
        unix_sock_track_fill_action(unix_sock_track_tmp, SK_AT, SK_ACCEPT_MASK);

        if (err != EAGAIN)
        {
            // record this error. this error could not recover.
            if((unix_sock_track_tmp->record_info_blc != NULL) && (unix_sock_track_tmp->close_time_sc == 0))
            {  
                unix_sock_track_tmp->error_flag = err;
            }
        }      
    }  

    //////////////add code end///////////////////    
#endif/* CONFIG_UNIX_SOCKET_TRACK_TOOL */
     
	return err;
}

static long unix_wait_for_peer(struct sock *other, long timeo)
{
	struct unix_sock *u = unix_sk(other);
	int sched;
	DEFINE_WAIT(wait);

	prepare_to_wait_exclusive(&u->peer_wait, &wait, TASK_INTERRUPTIBLE);

	sched = !sock_flag(other, SOCK_DEAD) &&
		!(other->sk_shutdown & RCV_SHUTDOWN) &&
		unix_recvq_full(other);

	unix_state_unlock(other);

	if (sched)
		timeo = schedule_timeout(timeo);

	finish_wait(&u->peer_wait, &wait);
	return timeo;
}

static int unix_stream_connect(struct socket *sock, struct sockaddr *uaddr,
			       int addr_len, int flags)
{
	struct sockaddr_un *sunaddr = (struct sockaddr_un *)uaddr;
	struct sock *sk = sock->sk;
	struct net *net = sock_net(sk);
	struct unix_sock *u = unix_sk(sk), *newu, *otheru;
	struct sock *newsk = NULL;
	struct sock *other = NULL;
	struct sk_buff *skb = NULL;
	unsigned int hash;
	int st;
	int err;
	long timeo;
#ifdef CONFIG_UNIX_SOCKET_TRACK_TOOL	
    //////////////add code begin///////////////////
    unix_sock_track_blc* unix_sock_track_tmp = NULL;
    //////////////add code end///////////////////

    //////////////add code begin///////////////////  

    if (SOCK_INODE(sock))
    {
        unix_sock_track_tmp = unix_sock_track_find_blc_with_action(2, NULL, SOCK_INODE(sock)->i_ino);
    }    

    if (unix_sock_track_tmp != NULL)
    {
        unix_sock_track_fill_action(unix_sock_track_tmp, SK_AW, SK_ACCEPT_MASK);
    }
    //////////////add code end///////////////////  
#endif/* CONFIG_UNIX_SOCKET_TRACK_TOOL */

	err = unix_mkname(sunaddr, addr_len, &hash);
	if (err < 0)
		goto out;
	addr_len = err;

	if (test_bit(SOCK_PASSCRED, &sock->flags) && !u->addr &&
	    (err = unix_autobind(sock)) != 0)
		goto out;

	timeo = sock_sndtimeo(sk, flags & O_NONBLOCK);

	/* First of all allocate resources.
	   If we will make it after state is locked,
	   we will have to recheck all again in any case.
	 */

	err = -ENOMEM;

	/* create new sock for complete connection */
	newsk = unix_create1(sock_net(sk), NULL);
	if (newsk == NULL)
		goto out;

	/* Allocate skb for sending to listening sock */
	skb = sock_wmalloc(newsk, 1, 0, GFP_KERNEL);
	if (skb == NULL)
		goto out;

restart:
	/*  Find listening sock. */
	other = unix_find_other(net, sunaddr, addr_len, sk->sk_type, hash, &err);
	if (!other)
		goto out;

	/* Latch state of peer */
	unix_state_lock(other);

	/* Apparently VFS overslept socket death. Retry. */
	if (sock_flag(other, SOCK_DEAD)) {
		unix_state_unlock(other);
		sock_put(other);
		goto restart;
	}

	err = -ECONNREFUSED;
	if (other->sk_state != TCP_LISTEN)
		goto out_unlock;
	if (other->sk_shutdown & RCV_SHUTDOWN)
		goto out_unlock;

	if (unix_recvq_full(other)) {
		err = -EAGAIN;
		if (!timeo)
			goto out_unlock;

		timeo = unix_wait_for_peer(other, timeo);

		err = sock_intr_errno(timeo);
		if (signal_pending(current))
			goto out;
		sock_put(other);
		goto restart;
	}

	/* Latch our state.

	   It is tricky place. We need to grab our state lock and cannot
	   drop lock on peer. It is dangerous because deadlock is
	   possible. Connect to self case and simultaneous
	   attempt to connect are eliminated by checking socket
	   state. other is TCP_LISTEN, if sk is TCP_LISTEN we
	   check this before attempt to grab lock.

	   Well, and we have to recheck the state after socket locked.
	 */
	st = sk->sk_state;

	switch (st) {
	case TCP_CLOSE:
		/* This is ok... continue with connect */
		break;
	case TCP_ESTABLISHED:
		/* Socket is already connected */
		err = -EISCONN;
		goto out_unlock;
	default:
		err = -EINVAL;
		goto out_unlock;
	}

	unix_state_lock_nested(sk);

	if (sk->sk_state != st) {
		unix_state_unlock(sk);
		unix_state_unlock(other);
		sock_put(other);
		goto restart;
	}

	err = security_unix_stream_connect(sk, other, newsk);
	if (err) {
		unix_state_unlock(sk);
		goto out_unlock;
	}

	/* The way is open! Fastly set all the necessary fields... */

	sock_hold(sk);
	unix_peer(newsk)	= sk;
	newsk->sk_state		= TCP_ESTABLISHED;
	newsk->sk_type		= sk->sk_type;
	init_peercred(newsk);
	newu = unix_sk(newsk);
	RCU_INIT_POINTER(newsk->sk_wq, &newu->peer_wq);
	otheru = unix_sk(other);

	/* copy address information from listening to new sock*/
	if (otheru->addr) {
		atomic_inc(&otheru->addr->refcnt);
		newu->addr = otheru->addr;
	}
	if (otheru->path.dentry) {
		path_get(&otheru->path);
		newu->path = otheru->path;
	}

	/* Set credentials */
	copy_peercred(sk, other);

	sock->state	= SS_CONNECTED;
	sk->sk_state	= TCP_ESTABLISHED;
	sock_hold(newsk);

	smp_mb__after_atomic_inc();	/* sock_hold() does an atomic_inc() */
	unix_peer(sk)	= newsk;

	unix_state_unlock(sk);

	/* take ten and and send info to listening sock */
	spin_lock(&other->sk_receive_queue.lock);
	__skb_queue_tail(&other->sk_receive_queue, skb);
	spin_unlock(&other->sk_receive_queue.lock);
	unix_state_unlock(other);
	other->sk_data_ready(other, 0);
	sock_put(other);
#ifdef CONFIG_UNIX_SOCKET_TRACK_TOOL  
    //////////////add code begin///////////////////  
    if (unix_sock_track_tmp != NULL)
    {
        unix_sock_track_fill_action(unix_sock_track_tmp, SK_AT, SK_ACCEPT_MASK);
    }
    //////////////add code end///////////////////  
#endif/* CONFIG_UNIX_SOCKET_TRACK_TOOL */
       
	return 0;

out_unlock:
	if (other)
		unix_state_unlock(other);

out:
	kfree_skb(skb);
	if (newsk)
		unix_release_sock(newsk, 0);
	if (other)
		sock_put(other);
#ifdef CONFIG_UNIX_SOCKET_TRACK_TOOL		
    //////////////add code begin///////////////////  

    if (unix_sock_track_tmp != NULL)
    {
        unix_sock_track_fill_action(unix_sock_track_tmp, SK_AF, SK_ACCEPT_MASK);

        if (err != EAGAIN)
        {
            // record this error. this error could not recover.  
            if((unix_sock_track_tmp->record_info_blc != NULL) && (unix_sock_track_tmp->close_time_sc == 0))
            {
                unix_sock_track_tmp->error_flag = err;
            }
        }
        //////////////add code end///////////////////         
    }  
#endif/* CONFIG_UNIX_SOCKET_TRACK_TOOL */
    
	return err;
}

static int unix_socketpair(struct socket *socka, struct socket *sockb)
{
	struct sock *ska = socka->sk, *skb = sockb->sk;

	/* Join our sockets back to back */
	sock_hold(ska);
	sock_hold(skb);
	unix_peer(ska) = skb;
	unix_peer(skb) = ska;
	init_peercred(ska);
	init_peercred(skb);

	if (ska->sk_type != SOCK_DGRAM) {
		ska->sk_state = TCP_ESTABLISHED;
		skb->sk_state = TCP_ESTABLISHED;
		socka->state  = SS_CONNECTED;
		sockb->state  = SS_CONNECTED;
	}
	return 0;
}

static void unix_sock_inherit_flags(const struct socket *old,
				    struct socket *new)
{
	if (test_bit(SOCK_PASSCRED, &old->flags))
		set_bit(SOCK_PASSCRED, &new->flags);
	if (test_bit(SOCK_PASSSEC, &old->flags))
		set_bit(SOCK_PASSSEC, &new->flags);
}

static int unix_accept(struct socket *sock, struct socket *newsock, int flags)
{
	struct sock *sk = sock->sk;
	struct sock *tsk;
	struct sk_buff *skb;
	int err;
#ifdef CONFIG_UNIX_SOCKET_TRACK_TOOL	
    //////////////add code begin///////////////////
    unix_sock_track_blc* unix_sock_track_tmp = NULL;
    //////////////add code end///////////////////
#endif/* CONFIG_UNIX_SOCKET_TRACK_TOOL */

	err = -EOPNOTSUPP;
	if (sock->type != SOCK_STREAM && sock->type != SOCK_SEQPACKET)
		goto out;

	err = -EINVAL;
	if (sk->sk_state != TCP_LISTEN)
		goto out;

	/* If socket state is TCP_LISTEN it cannot change (for now...),
	 * so that no locks are necessary.
	 */

#ifdef CONFIG_UNIX_SOCKET_TRACK_TOOL  
    //////////////add code begin///////////////////  
    if (SOCK_INODE(sock))
    {
        unix_sock_track_tmp = unix_sock_track_find_blc_with_action(2, NULL, SOCK_INODE(sock)->i_ino);
    }    

    if (unix_sock_track_tmp != NULL)
    {
        unix_sock_track_fill_action(unix_sock_track_tmp, SK_AW, SK_ACCEPT_MASK);
    }
    //////////////add code end///////////////////
#endif/* CONFIG_UNIX_SOCKET_TRACK_TOOL */
    
	skb = skb_recv_datagram(sk, 0, flags&O_NONBLOCK, &err);
	if (!skb) {
		/* This means receive shutdown. */
		if (err == 0)
			err = -EINVAL;
		goto out;
	}

	tsk = skb->sk;
	skb_free_datagram(sk, skb);
	wake_up_interruptible(&unix_sk(sk)->peer_wait);

	/* attach accepted sock to socket */
	unix_state_lock(tsk);
	newsock->state = SS_CONNECTED;
	unix_sock_inherit_flags(sock, newsock);
	sock_graft(tsk, newsock);
	unix_state_unlock(tsk);
#ifdef CONFIG_UNIX_SOCKET_TRACK_TOOL	
    //////////////add code begin///////////////////    
    if (unix_sock_track_tmp != NULL)
    {
        unix_sock_track_fill_action(unix_sock_track_tmp, SK_AT, SK_ACCEPT_MASK);
    }
    //////////////add code end/////////////////// 
#endif/* CONFIG_UNIX_SOCKET_TRACK_TOOL */
    
	return 0;

out:
#ifdef CONFIG_UNIX_SOCKET_TRACK_TOOL	
    //////////////add code begin///////////////////    
    if (unix_sock_track_tmp != NULL)
    {
        unix_sock_track_fill_action(unix_sock_track_tmp, SK_AF, SK_ACCEPT_MASK);
    }
    //////////////add code end/////////////////// 
#endif/* CONFIG_UNIX_SOCKET_TRACK_TOOL */
    
	return err;
}


static int unix_getname(struct socket *sock, struct sockaddr *uaddr, int *uaddr_len, int peer)
{
	struct sock *sk = sock->sk;
	struct unix_sock *u;
	DECLARE_SOCKADDR(struct sockaddr_un *, sunaddr, uaddr);
	int err = 0;

	if (peer) {
		sk = unix_peer_get(sk);

		err = -ENOTCONN;
		if (!sk)
			goto out;
		err = 0;
	} else {
		sock_hold(sk);
	}

	u = unix_sk(sk);
	unix_state_lock(sk);
	if (!u->addr) {
		sunaddr->sun_family = AF_UNIX;
		sunaddr->sun_path[0] = 0;
		*uaddr_len = sizeof(short);
	} else {
		struct unix_address *addr = u->addr;

		*uaddr_len = addr->len;
		memcpy(sunaddr, addr->name, *uaddr_len);
	}
	unix_state_unlock(sk);
	sock_put(sk);
out:
	return err;
}

static void unix_detach_fds(struct scm_cookie *scm, struct sk_buff *skb)
{
	int i;

	scm->fp = UNIXCB(skb).fp;
	UNIXCB(skb).fp = NULL;

	for (i = scm->fp->count-1; i >= 0; i--)
		unix_notinflight(scm->fp->fp[i]);
}

static void unix_destruct_scm(struct sk_buff *skb)
{
	struct scm_cookie scm;
	memset(&scm, 0, sizeof(scm));
	scm.pid  = UNIXCB(skb).pid;
	if (UNIXCB(skb).fp)
		unix_detach_fds(&scm, skb);

	/* Alas, it calls VFS */
	/* So fscking what? fput() had been SMP-safe since the last Summer */
	scm_destroy(&scm);
	sock_wfree(skb);
}

#define MAX_RECURSION_LEVEL 4

static int unix_attach_fds(struct scm_cookie *scm, struct sk_buff *skb)
{
	int i;
	unsigned char max_level = 0;
	int unix_sock_count = 0;

	for (i = scm->fp->count - 1; i >= 0; i--) {
		struct sock *sk = unix_get_socket(scm->fp->fp[i]);

		if (sk) {
			unix_sock_count++;
			max_level = max(max_level,
					unix_sk(sk)->recursion_level);
		}
	}
	if (unlikely(max_level > MAX_RECURSION_LEVEL))
		return -ETOOMANYREFS;

	/*
	 * Need to duplicate file references for the sake of garbage
	 * collection.  Otherwise a socket in the fps might become a
	 * candidate for GC while the skb is not yet queued.
	 */
	UNIXCB(skb).fp = scm_fp_dup(scm->fp);
	if (!UNIXCB(skb).fp)
		return -ENOMEM;

	if (unix_sock_count) {
		for (i = scm->fp->count - 1; i >= 0; i--)
			unix_inflight(scm->fp->fp[i]);
	}
	return max_level;
}

static int unix_scm_to_skb(struct scm_cookie *scm, struct sk_buff *skb, bool send_fds)
{
	int err = 0;

	UNIXCB(skb).pid  = get_pid(scm->pid);
	UNIXCB(skb).uid = scm->creds.uid;
	UNIXCB(skb).gid = scm->creds.gid;
	UNIXCB(skb).fp = NULL;
	if (scm->fp && send_fds)
		err = unix_attach_fds(scm, skb);

	skb->destructor = unix_destruct_scm;
	return err;
}

/*
 * Some apps rely on write() giving SCM_CREDENTIALS
 * We include credentials if source or destination socket
 * asserted SOCK_PASSCRED.
 */
static void maybe_add_creds(struct sk_buff *skb, const struct socket *sock,
			    const struct sock *other)
{
	if (UNIXCB(skb).pid)
		return;
	if (test_bit(SOCK_PASSCRED, &sock->flags) ||
	    !other->sk_socket ||
	    test_bit(SOCK_PASSCRED, &other->sk_socket->flags)) {
		UNIXCB(skb).pid  = get_pid(task_tgid(current));
		current_uid_gid(&UNIXCB(skb).uid, &UNIXCB(skb).gid);
	}
}

/*
 *	Send AF_UNIX data.
 */

static int unix_dgram_sendmsg(struct kiocb *kiocb, struct socket *sock,
			      struct msghdr *msg, size_t len)
{
	struct sock_iocb *siocb = kiocb_to_siocb(kiocb);
	struct sock *sk = sock->sk;
	struct net *net = sock_net(sk);
	struct unix_sock *u = unix_sk(sk);
	struct sockaddr_un *sunaddr = msg->msg_name;
	struct sock *other = NULL;
	int namelen = 0; /* fake GCC */
	int err;
	unsigned int hash;
	struct sk_buff *skb;
	long timeo;
	struct scm_cookie tmp_scm;
	int max_level;
	int data_len = 0;
#ifdef CONFIG_UNIX_SOCKET_TRACK_TOOL	
    //////////////add code begin///////////////////
    unix_sock_track_blc* unix_sock_track_tmp = NULL;
    //////////////add code end/////////////////// 

//    unix_test_time_log(sock, 2);
    //////////////add code begin///////////////////  
    if (SOCK_INODE(sock))
    {
         unix_sock_track_tmp = unix_sock_track_find_blc_with_action(2, NULL, SOCK_INODE(sock)->i_ino);
    }    

    if (unix_sock_track_tmp != NULL)
    {
        unix_socket_track_socket_pino(unix_sock_track_tmp, sk);
        unix_sock_track_fill_action(unix_sock_track_tmp, SK_WW, SK_WRITE_MASK);
    }
    //////////////add code end///////////////////
#endif/* CONFIG_UNIX_SOCKET_TRACK_TOOL */  
	if (NULL == siocb->scm)
		siocb->scm = &tmp_scm;
	wait_for_unix_gc();
	err = scm_send(sock, msg, siocb->scm, false);
	if (err < 0)
		return err;

	err = -EOPNOTSUPP;
	if (msg->msg_flags&MSG_OOB)
		goto out;

	if (msg->msg_namelen) {
		err = unix_mkname(sunaddr, msg->msg_namelen, &hash);
		if (err < 0)
			goto out;
		namelen = err;
	} else {
		sunaddr = NULL;
		err = -ENOTCONN;
		other = unix_peer_get(sk);
		if (!other)
			goto out;
	}

	if (test_bit(SOCK_PASSCRED, &sock->flags) && !u->addr
	    && (err = unix_autobind(sock)) != 0)
		goto out;

	err = -EMSGSIZE;
	if (len > sk->sk_sndbuf - 32)
		goto out;

	if (len > SKB_MAX_ALLOC)
		data_len = min_t(size_t,
				 len - SKB_MAX_ALLOC,
				 MAX_SKB_FRAGS * PAGE_SIZE);

	skb = sock_alloc_send_pskb(sk, len - data_len, data_len,
				   msg->msg_flags & MSG_DONTWAIT, &err);
	if (skb == NULL)
		goto out;

	err = unix_scm_to_skb(siocb->scm, skb, true);
	if (err < 0)
		goto out_free;
	max_level = err + 1;
	unix_get_secdata(siocb->scm, skb);

	skb_put(skb, len - data_len);
	skb->data_len = data_len;
	skb->len = len;
	err = skb_copy_datagram_from_iovec(skb, 0, msg->msg_iov, 0, len);
	if (err)
		goto out_free;

	timeo = sock_sndtimeo(sk, msg->msg_flags & MSG_DONTWAIT);

restart:
	if (!other) {
		err = -ECONNRESET;
		if (sunaddr == NULL)
			goto out_free;

		other = unix_find_other(net, sunaddr, namelen, sk->sk_type,
					hash, &err);
		if (other == NULL)
			goto out_free;
	}

	if (sk_filter(other, skb) < 0) {
		/* Toss the packet but do not return any error to the sender */
		err = len;
		goto out_free;
	}

	unix_state_lock(other);
	err = -EPERM;
	if (!unix_may_send(sk, other))
		goto out_unlock;

	if (sock_flag(other, SOCK_DEAD)) {
		/*
		 *	Check with 1003.1g - what should
		 *	datagram error
		 */
		unix_state_unlock(other);
		sock_put(other);

		err = 0;
		unix_state_lock(sk);
		if (unix_peer(sk) == other) {
			unix_peer(sk) = NULL;
			unix_state_unlock(sk);

			unix_dgram_disconnected(sk, other);
			sock_put(other);
			err = -ECONNREFUSED;
		} else {
			unix_state_unlock(sk);
		}

		other = NULL;
		if (err)
			goto out_free;
		goto restart;
	}

	err = -EPIPE;
	if (other->sk_shutdown & RCV_SHUTDOWN)
		goto out_unlock;

	if (sk->sk_type != SOCK_SEQPACKET) {
		err = security_unix_may_send(sk->sk_socket, other->sk_socket);
		if (err)
			goto out_unlock;
	}

	if (unix_peer(other) != sk && unix_recvq_full(other)) {
		if (!timeo) {
			err = -EAGAIN;
			goto out_unlock;
		}

		timeo = unix_wait_for_peer(other, timeo);

		err = sock_intr_errno(timeo);
		if (signal_pending(current))
			goto out_free;

		goto restart;
	}

	if (sock_flag(other, SOCK_RCVTSTAMP))
		__net_timestamp(skb);
	maybe_add_creds(skb, sock, other);
	skb_queue_tail(&other->sk_receive_queue, skb);
	if (max_level > unix_sk(other)->recursion_level)
		unix_sk(other)->recursion_level = max_level;
	unix_state_unlock(other);
	other->sk_data_ready(other, len);
	sock_put(other);
	scm_destroy(siocb->scm);
#ifdef CONFIG_UNIX_SOCKET_TRACK_TOOL	
    //////////////add code begin///////////////////  

    if (unix_sock_track_tmp != NULL)
    {
        unix_sock_track_fill_action(unix_sock_track_tmp, SK_WD, SK_WRITE_MASK);

        // record this send data len.  
        if((unix_sock_track_tmp->record_info_blc != NULL) && (unix_sock_track_tmp->close_time_sc == 0))
        {
            unix_sock_track_tmp->send_data_total += len;
            unix_sock_track_tmp->send_count ++;    
        }           
    }  

    //////////////add code end///////////////////     
#endif/* CONFIG_UNIX_SOCKET_TRACK_TOOL */
    
	return len;

out_unlock:
	unix_state_unlock(other);
out_free:
	kfree_skb(skb);
out:
	if (other)
		sock_put(other);
	scm_destroy(siocb->scm);
#ifdef CONFIG_UNIX_SOCKET_TRACK_TOOL	
    //////////////add code begin///////////////////  

    if (unix_sock_track_tmp != NULL)
    {
        unix_sock_track_fill_action(unix_sock_track_tmp, SK_WE, SK_WRITE_MASK);

        if (err != EAGAIN)
        {
            // record this error. this error could not recover.  
            if((unix_sock_track_tmp->record_info_blc != NULL) && (unix_sock_track_tmp->close_time_sc == 0))
            {
                unix_sock_track_tmp->error_flag = err;
            }
        }
    }  

    //////////////add code end///////////////////   
#endif/* CONFIG_UNIX_SOCKET_TRACK_TOOL */
      
	return err;
}


static int unix_stream_sendmsg(struct kiocb *kiocb, struct socket *sock,
			       struct msghdr *msg, size_t len)
{
	struct sock_iocb *siocb = kiocb_to_siocb(kiocb);
	struct sock *sk = sock->sk;
	struct sock *other = NULL;
	int err, size;
	struct sk_buff *skb;
	int sent = 0;
	struct scm_cookie tmp_scm;
	bool fds_sent = false;
	int max_level;
#ifdef CONFIG_UNIX_SOCKET_TRACK_TOOL	
    //////////////add code begin///////////////////
    unix_sock_track_blc* unix_sock_track_tmp = NULL;
    //////////////add code end/////////////////// 

//    unix_test_time_log(sock, 2);
    //////////////add code begin///////////////////  
    if (SOCK_INODE(sock))
    {
        unix_sock_track_tmp = unix_sock_track_find_blc_with_action(2, NULL, SOCK_INODE(sock)->i_ino);
    }
  
    if (unix_sock_track_tmp != NULL)
    {
        unix_socket_track_socket_pino(unix_sock_track_tmp, sk);
        unix_sock_track_fill_action(unix_sock_track_tmp, SK_WW, SK_WRITE_MASK);
    }
    //////////////add code end///////////////////
#endif/* CONFIG_UNIX_SOCKET_TRACK_TOOL */

	if (NULL == siocb->scm)
		siocb->scm = &tmp_scm;
	wait_for_unix_gc();
	err = scm_send(sock, msg, siocb->scm, false);
	if (err < 0)
		return err;

	err = -EOPNOTSUPP;
	if (msg->msg_flags&MSG_OOB)
		goto out_err;

	if (msg->msg_namelen) {
		err = sk->sk_state == TCP_ESTABLISHED ? -EISCONN : -EOPNOTSUPP;
		goto out_err;
	} else {
		err = -ENOTCONN;
		other = unix_peer(sk);
		if (!other)
			goto out_err;
	}

	if (sk->sk_shutdown & SEND_SHUTDOWN)
		goto pipe_err;

	while (sent < len) {
		/*
		 *	Optimisation for the fact that under 0.01% of X
		 *	messages typically need breaking up.
		 */

		size = len-sent;

		/* Keep two messages in the pipe so it schedules better */
		if (size > ((sk->sk_sndbuf >> 1) - 64))
			size = (sk->sk_sndbuf >> 1) - 64;

		if (size > SKB_MAX_ALLOC)
			size = SKB_MAX_ALLOC;

		/*
		 *	Grab a buffer
		 */

		skb = sock_alloc_send_skb(sk, size, msg->msg_flags&MSG_DONTWAIT,
					  &err);

		if (skb == NULL)
			goto out_err;

		/*
		 *	If you pass two values to the sock_alloc_send_skb
		 *	it tries to grab the large buffer with GFP_NOFS
		 *	(which can fail easily), and if it fails grab the
		 *	fallback size buffer which is under a page and will
		 *	succeed. [Alan]
		 */
		size = min_t(int, size, skb_tailroom(skb));


		/* Only send the fds in the first buffer */
		err = unix_scm_to_skb(siocb->scm, skb, !fds_sent);
		if (err < 0) {
			kfree_skb(skb);
			goto out_err;
		}
		max_level = err + 1;
		fds_sent = true;

		err = memcpy_fromiovec(skb_put(skb, size), msg->msg_iov, size);
		if (err) {
			kfree_skb(skb);
			goto out_err;
		}

		unix_state_lock(other);

		if (sock_flag(other, SOCK_DEAD) ||
		    (other->sk_shutdown & RCV_SHUTDOWN))
		{
                    if( other->sk_socket )
                    {
                        if(sk->sk_socket)
                        {
                
                         #ifdef CONFIG_MTK_NET_LOGGING 
                         printk(KERN_INFO " [mtk_net][unix]: sendmsg[%lu:%lu]:peer close\n" ,SOCK_INODE(sk->sk_socket)->i_ino,SOCK_INODE(other->sk_socket)->i_ino);
				         #endif
		         }
		         else{
				   	    #ifdef CONFIG_MTK_NET_LOGGING 
				        printk(KERN_INFO " [mtk_net][unix]: sendmsg[null:%lu]:peer close\n" ,SOCK_INODE(other->sk_socket)->i_ino);
				        #endif
		         }        

		    }
		    else	
					{
						#ifdef CONFIG_MTK_NET_LOGGING 	
				        printk(KERN_INFO " [mtk_net][unix]: sendmsg:peer close \n" );
				        #endif
				}
		   		
          
			goto pipe_err_free;
		}

		maybe_add_creds(skb, sock, other);
		skb_queue_tail(&other->sk_receive_queue, skb);
		if (max_level > unix_sk(other)->recursion_level)
			unix_sk(other)->recursion_level = max_level;
		unix_state_unlock(other);
		other->sk_data_ready(other, size);
		sent += size;
	}

	scm_destroy(siocb->scm);
	siocb->scm = NULL;
#ifdef CONFIG_UNIX_SOCKET_TRACK_TOOL	
    //////////////add code begin///////////////////  

    if (unix_sock_track_tmp != NULL)
    {
        unix_sock_track_fill_action(unix_sock_track_tmp, SK_WD, SK_WRITE_MASK);

        // record this send data len.  
        if((unix_sock_track_tmp->record_info_blc != NULL) && (unix_sock_track_tmp->close_time_sc == 0))
        {        
            unix_sock_track_tmp->send_data_total += len;
            unix_sock_track_tmp->send_count ++;   
        }
    }  

    //////////////add code end///////////////////
#endif/* CONFIG_UNIX_SOCKET_TRACK_TOOL */

	return sent;

pipe_err_free:
	unix_state_unlock(other);
	kfree_skb(skb);
pipe_err:
	if (sent == 0 && !(msg->msg_flags&MSG_NOSIGNAL))
		send_sig(SIGPIPE, current, 0);
	err = -EPIPE;
out_err:
	scm_destroy(siocb->scm);
	siocb->scm = NULL;
#ifdef CONFIG_UNIX_SOCKET_TRACK_TOOL	
    //////////////add code begin///////////////////  

    if (unix_sock_track_tmp != NULL)
    {
        unix_sock_track_fill_action(unix_sock_track_tmp, SK_WE, SK_WRITE_MASK);

        if (err != EAGAIN)
        {
            // record this error. this error could not recover.  
            if((unix_sock_track_tmp->record_info_blc != NULL) && (unix_sock_track_tmp->close_time_sc == 0))
            {
                unix_sock_track_tmp->error_flag = err;
            }
        }   
    }  

    //////////////add code end/////////////////// 
#endif/* CONFIG_UNIX_SOCKET_TRACK_TOOL */
        
	return sent ? : err;
}

static int unix_seqpacket_sendmsg(struct kiocb *kiocb, struct socket *sock,
				  struct msghdr *msg, size_t len)
{
	int err;
	struct sock *sk = sock->sk;

	err = sock_error(sk);
	if (err)
		return err;

	if (sk->sk_state != TCP_ESTABLISHED)
		return -ENOTCONN;

	if (msg->msg_namelen)
		msg->msg_namelen = 0;

	return unix_dgram_sendmsg(kiocb, sock, msg, len);
}

static int unix_seqpacket_recvmsg(struct kiocb *iocb, struct socket *sock,
			      struct msghdr *msg, size_t size,
			      int flags)
{
	struct sock *sk = sock->sk;

	if (sk->sk_state != TCP_ESTABLISHED)
		return -ENOTCONN;

	return unix_dgram_recvmsg(iocb, sock, msg, size, flags);
}

static void unix_copy_addr(struct msghdr *msg, struct sock *sk)
{
	struct unix_sock *u = unix_sk(sk);

	if (u->addr) {
		msg->msg_namelen = u->addr->len;
		memcpy(msg->msg_name, u->addr->name, u->addr->len);
	}
}

static int unix_dgram_recvmsg(struct kiocb *iocb, struct socket *sock,
			      struct msghdr *msg, size_t size,
			      int flags)
{
	struct sock_iocb *siocb = kiocb_to_siocb(iocb);
	struct scm_cookie tmp_scm;
	struct sock *sk = sock->sk;
	struct unix_sock *u = unix_sk(sk);
	int noblock = flags & MSG_DONTWAIT;
	struct sk_buff *skb;
	int err;
	int peeked, skip;
#ifdef CONFIG_UNIX_SOCKET_TRACK_TOOL	
    //////////////add code begin///////////////////
    unix_sock_track_blc* unix_sock_track_tmp = NULL;
    //////////////add code end/////////////////// 

//    unix_test_time_log(sock, 1);
    //////////////add code begin///////////////////  
    if (SOCK_INODE(sock))
    {
        unix_sock_track_tmp = unix_sock_track_find_blc_with_action(2, NULL, SOCK_INODE(sock)->i_ino);
    }    

    if (unix_sock_track_tmp != NULL)
    {
        unix_socket_track_socket_pino(unix_sock_track_tmp, sk);
        unix_sock_track_fill_action(unix_sock_track_tmp, SK_RW, SK_READ_MASK);
        if (sk->sk_receive_queue.qlen > unix_sock_track_tmp->recv_queue_len)
        {
            unix_sock_track_tmp->recv_queue_len = sk->sk_receive_queue.qlen;
        }

    }
    //////////////add code end///////////////////
#endif/* CONFIG_UNIX_SOCKET_TRACK_TOOL */

	err = -EOPNOTSUPP;
	if (flags&MSG_OOB)
		goto out;

	err = mutex_lock_interruptible(&u->readlock);
	if (err) {
		err = sock_intr_errno(sock_rcvtimeo(sk, noblock));
		goto out;
	}

	skip = sk_peek_offset(sk, flags);

	skb = __skb_recv_datagram(sk, flags, &peeked, &skip, &err);
	if (!skb) {
		unix_state_lock(sk);
		/* Signal EOF on disconnected non-blocking SEQPACKET socket. */
		if (sk->sk_type == SOCK_SEQPACKET && err == -EAGAIN &&
		    (sk->sk_shutdown & RCV_SHUTDOWN))
			err = 0;
		unix_state_unlock(sk);
		goto out_unlock;
	}

	wake_up_interruptible_sync_poll(&u->peer_wait,
					POLLOUT | POLLWRNORM | POLLWRBAND);

	if (msg->msg_name)
		unix_copy_addr(msg, skb->sk);

	if (size > skb->len - skip)
		size = skb->len - skip;
	else if (size < skb->len - skip)
		msg->msg_flags |= MSG_TRUNC;

	err = skb_copy_datagram_iovec(skb, skip, msg->msg_iov, size);
	if (err)
		goto out_free;

	if (sock_flag(sk, SOCK_RCVTSTAMP))
		__sock_recv_timestamp(msg, sk, skb);

	if (!siocb->scm) {
		siocb->scm = &tmp_scm;
		memset(&tmp_scm, 0, sizeof(tmp_scm));
	}
	scm_set_cred(siocb->scm, UNIXCB(skb).pid, UNIXCB(skb).uid, UNIXCB(skb).gid);
	unix_set_secdata(siocb->scm, skb);

	if (!(flags & MSG_PEEK)) {
		if (UNIXCB(skb).fp)
			unix_detach_fds(siocb->scm, skb);

		sk_peek_offset_bwd(sk, skb->len);
	} else {
		/* It is questionable: on PEEK we could:
		   - do not return fds - good, but too simple 8)
		   - return fds, and do not return them on read (old strategy,
		     apparently wrong)
		   - clone fds (I chose it for now, it is the most universal
		     solution)

		   POSIX 1003.1g does not actually define this clearly
		   at all. POSIX 1003.1g doesn't define a lot of things
		   clearly however!

		*/

		sk_peek_offset_fwd(sk, size);

		if (UNIXCB(skb).fp)
			siocb->scm->fp = scm_fp_dup(UNIXCB(skb).fp);
	}
	err = (flags & MSG_TRUNC) ? skb->len - skip : size;

	scm_recv(sock, msg, siocb->scm, flags);

out_free:
	skb_free_datagram(sk, skb);
out_unlock:
	mutex_unlock(&u->readlock);
out:
#ifdef CONFIG_UNIX_SOCKET_TRACK_TOOL
    //////////////add code begin///////////////////  

    //printk("[usktrk] unix_dgram_recvmsg err=%d \n", err);
    if (unix_sock_track_tmp != NULL)
    {
        unix_sock_track_fill_action(unix_sock_track_tmp, SK_RD, SK_READ_MASK);

        if (err < 0)
        {
            if (err != -EAGAIN)
            {
                // record this error. this error could not recover.  
                if((unix_sock_track_tmp->record_info_blc != NULL) && (unix_sock_track_tmp->close_time_sc == 0))
                {
                    unix_sock_track_tmp->error_flag = err;
                }
            }
        }
        else
        {
            // record this send data len.  
            if (!(flags & MSG_PEEK))
            {
            		if((unix_sock_track_tmp->record_info_blc != NULL) && (unix_sock_track_tmp->close_time_sc == 0))
            		{
                    unix_sock_track_tmp->recv_data_total += err;  
                    unix_sock_track_tmp->recv_count ++;
                }
            }       
      
            if (err == 0)
            {
                unix_sock_track_fill_action(unix_sock_track_tmp, SK_RO, SK_READ_MASK);    
            }
        }
    }
    //////////////add code end/////////////////// 
#endif/* CONFIG_UNIX_SOCKET_TRACK_TOOL */
      
	return err;
}

/*
 *	Sleep until more data has arrived. But check for races..
 */
static long unix_stream_data_wait(struct sock *sk, long timeo,
				  struct sk_buff *last)
{
	DEFINE_WAIT(wait);

	unix_state_lock(sk);

	for (;;) {
		prepare_to_wait(sk_sleep(sk), &wait, TASK_INTERRUPTIBLE);

		if (skb_peek_tail(&sk->sk_receive_queue) != last ||
		    sk->sk_err ||
		    (sk->sk_shutdown & RCV_SHUTDOWN) ||
		    signal_pending(current) ||
		    !timeo)
			break;

		set_bit(SOCK_ASYNC_WAITDATA, &sk->sk_socket->flags);
		unix_state_unlock(sk);
		timeo = freezable_schedule_timeout(timeo);
		unix_state_lock(sk);
		clear_bit(SOCK_ASYNC_WAITDATA, &sk->sk_socket->flags);
	}

	finish_wait(sk_sleep(sk), &wait);
	unix_state_unlock(sk);
	return timeo;
}

static int unix_stream_recvmsg(struct kiocb *iocb, struct socket *sock,
			       struct msghdr *msg, size_t size,
			       int flags)
{
	struct sock_iocb *siocb = kiocb_to_siocb(iocb);
	struct scm_cookie tmp_scm;
	struct sock *sk = sock->sk;
	struct unix_sock *u = unix_sk(sk);
	struct sockaddr_un *sunaddr = msg->msg_name;
	int copied = 0;
	int check_creds = 0;
	int target;
	int err = 0;
	long timeo;
	int skip;
	struct sock * other = unix_peer(sk);
#ifdef CONFIG_UNIX_SOCKET_TRACK_TOOL	
    //////////////add code begin///////////////////
    unix_sock_track_blc* unix_sock_track_tmp = NULL;
    //////////////add code end///////////////////   

//    unix_test_time_log(sock, 1);

    //////////////add code begin///////////////////  
    if (SOCK_INODE(sock))
    {
        unix_sock_track_tmp = unix_sock_track_find_blc_with_action(2, NULL, SOCK_INODE(sock)->i_ino);
    }    

    if (unix_sock_track_tmp != NULL)
    {
        unix_socket_track_socket_pino(unix_sock_track_tmp, sk);
        unix_sock_track_fill_action(unix_sock_track_tmp, SK_RW, SK_READ_MASK);
    
        if (sk->sk_receive_queue.qlen > unix_sock_track_tmp->recv_queue_len)
        {
            unix_sock_track_tmp->recv_queue_len = sk->sk_receive_queue.qlen;
        }   
    }
    //////////////add code end/////////////////// 
#endif/* CONFIG_UNIX_SOCKET_TRACK_TOOL */

	err = -EINVAL;
	if (sk->sk_state != TCP_ESTABLISHED)
		goto out;

	err = -EOPNOTSUPP;
	if (flags&MSG_OOB)
		goto out;

	target = sock_rcvlowat(sk, flags&MSG_WAITALL, size);
	timeo = sock_rcvtimeo(sk, flags&MSG_DONTWAIT);

	/* Lock the socket to prevent queue disordering
	 * while sleeps in memcpy_tomsg
	 */

	if (!siocb->scm) {
		siocb->scm = &tmp_scm;
		memset(&tmp_scm, 0, sizeof(tmp_scm));
	}

	err = mutex_lock_interruptible(&u->readlock);
	if (err) {
		err = sock_intr_errno(timeo);
		goto out;
	}

	do {
		int chunk;
		struct sk_buff *skb, *last;

		unix_state_lock(sk);
		last = skb = skb_peek(&sk->sk_receive_queue);
again:
		if (skb == NULL) {
			unix_sk(sk)->recursion_level = 0;
			if (copied >= target)
				goto unlock;

			/*
			 *	POSIX 1003.1g mandates this order.
			 */

			err = sock_error(sk);
			if (err)
				goto unlock;
			if (sk->sk_shutdown & RCV_SHUTDOWN)
			{
                            if(sk && sk->sk_socket )
                            {
				   if(other && other->sk_socket ){
				   	#ifdef CONFIG_MTK_NET_LOGGING 
				   	
                     printk(KERN_INFO " [mtk_net][unix]: recvmsg[%lu:%lu]:exit read due to peer shutdown  \n" ,SOCK_INODE(sk->sk_socket)->i_ino,SOCK_INODE(other->sk_socket)->i_ino);
				   #endif
				   }else{				   
				   	#ifdef CONFIG_MTK_NET_LOGGING 				   
                     printk(KERN_INFO "[mtk_net][unix]: recvmsg[%lu:null]:exit read due to peer shutdown  \n" ,SOCK_INODE(sk->sk_socket)->i_ino);
                     #endif
				   }
				 }
			    else{	
					#ifdef CONFIG_MTK_NET_LOGGING 
				   printk(KERN_INFO " [mtk_net][unix]: recvmsg: exit read due to peer shutdown \n" );
				   #endif
			    }
				goto unlock;
			}
			unix_state_unlock(sk);
			err = -EAGAIN;
			if (!timeo)
				break;
			mutex_unlock(&u->readlock);

			timeo = unix_stream_data_wait(sk, timeo, last);
                        if (!timeo)
                        {
                            if(sk && sk->sk_socket )
                            {
                                if(other && other->sk_socket ){
				   	#ifdef CONFIG_MTK_NET_LOGGING 
                     printk(KERN_INFO " [mtk_net][unix]: recvmsg[%lu:%lu]:exit read due to timeout  \n" ,SOCK_INODE(sk->sk_socket)->i_ino,SOCK_INODE(other->sk_socket)->i_ino);
				   #endif
				   }else{				   
				   	#ifdef CONFIG_MTK_NET_LOGGING 				   
                     printk(KERN_INFO " [mtk_net][unix]: recvmsg[%lu:null]:exit read due to timeout  \n" ,SOCK_INODE(sk->sk_socket)->i_ino);
                     #endif
				    }			  
		           }
			   else	
					{
						#ifdef CONFIG_MTK_NET_LOGGING 	
				  printk(KERN_INFO " [mtk_net][unix]: recvmsg:exit read due to timeout \n" );
				  #endif
				}
		   		  
			 }

			if (signal_pending(current)
			    ||  mutex_lock_interruptible(&u->readlock)) {
				err = sock_intr_errno(timeo);
				goto out;
			}

			continue;
 unlock:
			unix_state_unlock(sk);
			break;
		}

		skip = sk_peek_offset(sk, flags);
		while (skip >= skb->len) {
			skip -= skb->len;
			last = skb;
			skb = skb_peek_next(skb, &sk->sk_receive_queue);
			if (!skb)
				goto again;
		}

		unix_state_unlock(sk);

		if (check_creds) {
			/* Never glue messages from different writers */
			if ((UNIXCB(skb).pid  != siocb->scm->pid) ||
			    !uid_eq(UNIXCB(skb).uid, siocb->scm->creds.uid) ||
			    !gid_eq(UNIXCB(skb).gid, siocb->scm->creds.gid))
				break;
		} else if (test_bit(SOCK_PASSCRED, &sock->flags)) {
			/* Copy credentials */
			scm_set_cred(siocb->scm, UNIXCB(skb).pid, UNIXCB(skb).uid, UNIXCB(skb).gid);
			check_creds = 1;
		}

		/* Copy address just once */
		if (sunaddr) {
			unix_copy_addr(msg, skb->sk);
			sunaddr = NULL;
		}

		chunk = min_t(unsigned int, skb->len - skip, size);
		if (memcpy_toiovec(msg->msg_iov, skb->data + skip, chunk)) {
			if (copied == 0)
				copied = -EFAULT;
			break;
		}
		copied += chunk;
		size -= chunk;

		/* Mark read part of skb as used */
		if (!(flags & MSG_PEEK)) {
			skb_pull(skb, chunk);

			sk_peek_offset_bwd(sk, chunk);

			if (UNIXCB(skb).fp)
				unix_detach_fds(siocb->scm, skb);

			if (skb->len)
				break;

			skb_unlink(skb, &sk->sk_receive_queue);
			consume_skb(skb);

			if (siocb->scm->fp)
				break;
		} else {
			/* It is questionable, see note in unix_dgram_recvmsg.
			 */
			if (UNIXCB(skb).fp)
				siocb->scm->fp = scm_fp_dup(UNIXCB(skb).fp);

			sk_peek_offset_fwd(sk, chunk);

			break;
		}
	} while (size);

	mutex_unlock(&u->readlock);
	scm_recv(sock, msg, siocb->scm, flags);
out:
#ifdef CONFIG_UNIX_SOCKET_TRACK_TOOL  
    //////////////add code begin///////////////////  

    if (unix_sock_track_tmp != NULL)
    {
        unix_sock_track_fill_action(unix_sock_track_tmp, SK_RD, SK_READ_MASK);

        if (err < 0)
        {
            if (err != -EAGAIN)
            {
                // record this error. this error could not recover.  
                if((unix_sock_track_tmp->record_info_blc != NULL) && (unix_sock_track_tmp->close_time_sc == 0))
                {
                    unix_sock_track_tmp->error_flag = err;
                }
            }
        }
        else
        {
            // record this send data len.  
            if (!(flags & MSG_PEEK))
            {
                if (copied > 0)
                {
                    if((unix_sock_track_tmp->record_info_blc != NULL) && (unix_sock_track_tmp->close_time_sc == 0))
                    {
                        unix_sock_track_tmp->recv_data_total += copied;  
                        unix_sock_track_tmp->recv_count ++;
                    }
                }
            }

            //printk("[usktrk] unix_stream_recvmsg recv_data_total =%u\n", unix_sock_track_tmp->recv_data_total); 

            if ((copied == 0) && (err == 0))
            {
                unix_sock_track_fill_action(unix_sock_track_tmp, SK_RO, SK_READ_MASK);
            }

        }
    }
    //////////////add code end///////////////////     
#endif/* CONFIG_UNIX_SOCKET_TRACK_TOOL */
  
	return copied ? : err;
}

static int unix_shutdown(struct socket *sock, int mode)
{
	struct sock *sk = sock->sk;
	struct sock *other;

	if (mode < SHUT_RD || mode > SHUT_RDWR)
		return -EINVAL;
	/* This maps:
	 * SHUT_RD   (0) -> RCV_SHUTDOWN  (1)
	 * SHUT_WR   (1) -> SEND_SHUTDOWN (2)
	 * SHUT_RDWR (2) -> SHUTDOWN_MASK (3)
	 */
	++mode;

	unix_state_lock(sk);
	sk->sk_shutdown |= mode;
	other = unix_peer(sk);
	if (other)
		sock_hold(other);
	unix_state_unlock(sk);
	sk->sk_state_change(sk);

	if (other &&
		(sk->sk_type == SOCK_STREAM || sk->sk_type == SOCK_SEQPACKET)) {

		int peer_mode = 0;

		if (mode&RCV_SHUTDOWN)
			peer_mode |= SEND_SHUTDOWN;
		if (mode&SEND_SHUTDOWN)
			peer_mode |= RCV_SHUTDOWN;
		unix_state_lock(other);
		other->sk_shutdown |= peer_mode;
		unix_state_unlock(other);
		other->sk_state_change(other);
		if (peer_mode == SHUTDOWN_MASK)
			sk_wake_async(other, SOCK_WAKE_WAITD, POLL_HUP);
		else if (peer_mode & RCV_SHUTDOWN)
			sk_wake_async(other, SOCK_WAKE_WAITD, POLL_IN);
	}
	if (other)
		sock_put(other);

	return 0;
}

long unix_inq_len(struct sock *sk)
{
	struct sk_buff *skb;
	long amount = 0;

	if (sk->sk_state == TCP_LISTEN)
		return -EINVAL;

	spin_lock(&sk->sk_receive_queue.lock);
	if (sk->sk_type == SOCK_STREAM ||
	    sk->sk_type == SOCK_SEQPACKET) {
		skb_queue_walk(&sk->sk_receive_queue, skb)
			amount += skb->len;
	} else {
		skb = skb_peek(&sk->sk_receive_queue);
		if (skb)
			amount = skb->len;
	}
	spin_unlock(&sk->sk_receive_queue.lock);

	return amount;
}
EXPORT_SYMBOL_GPL(unix_inq_len);

long unix_outq_len(struct sock *sk)
{
	return sk_wmem_alloc_get(sk);
}
EXPORT_SYMBOL_GPL(unix_outq_len);

static int unix_ioctl(struct socket *sock, unsigned int cmd, unsigned long arg)
{
	struct sock *sk = sock->sk;
	long amount = 0;
	int err;

	switch (cmd) {
	case SIOCOUTQ:
		amount = unix_outq_len(sk);
		err = put_user(amount, (int __user *)arg);
		break;
	case SIOCINQ:
		amount = unix_inq_len(sk);
		if (amount < 0)
			err = amount;
		else
			err = put_user(amount, (int __user *)arg);
		break;
	default:
		err = -ENOIOCTLCMD;
		break;
	}
	return err;
}

static unsigned int unix_poll(struct file *file, struct socket *sock, poll_table *wait)
{
	struct sock *sk = sock->sk;
	unsigned int mask;
#ifdef CONFIG_UNIX_SOCKET_TRACK_TOOL
    //////////////add code begin///////////////////
    unix_sock_track_blc* unix_sock_track_tmp = NULL;
    //////////////add code end///////////////////
  
    //unix_test_time_log(sock, 3);
    //////////////add code begin///////////////////  
    if (SOCK_INODE(sock))
    {
        unix_sock_track_tmp = unix_sock_track_find_blc_with_action(2, NULL, SOCK_INODE(sock)->i_ino);
    }
  
    if (unix_sock_track_tmp != NULL)
    {
        unix_sock_track_fill_action(unix_sock_track_tmp, SK_PW, SK_POLL_MASK);
    }
    //////////////add code end///////////////////
#endif/* CONFIG_UNIX_SOCKET_TRACK_TOOL */

	sock_poll_wait(file, sk_sleep(sk), wait);
	mask = 0;

	/* exceptional events? */
	if (sk->sk_err)
		mask |= POLLERR;
	if (sk->sk_shutdown == SHUTDOWN_MASK)
		mask |= POLLHUP;
	if (sk->sk_shutdown & RCV_SHUTDOWN)
		mask |= POLLRDHUP | POLLIN | POLLRDNORM;

	/* readable? */
	if (!skb_queue_empty(&sk->sk_receive_queue))
		mask |= POLLIN | POLLRDNORM;

	/* Connection-based need to check for termination and startup */
	if ((sk->sk_type == SOCK_STREAM || sk->sk_type == SOCK_SEQPACKET) &&
	    sk->sk_state == TCP_CLOSE)
		mask |= POLLHUP;

	/*
	 * we set writable also when the other side has shut down the
	 * connection. This prevents stuck sockets.
	 */
	if (unix_writable(sk))
		mask |= POLLOUT | POLLWRNORM | POLLWRBAND;
#ifdef CONFIG_UNIX_SOCKET_TRACK_TOOL
    //////////////add code begin///////////////////    
    if (unix_sock_track_tmp != NULL)
    {
        if (mask & (POLLIN | POLLERR | POLLHUP))
        {
            unix_sock_track_fill_action(unix_sock_track_tmp, SK_PT, SK_POLL_MASK);      
        }
        else
        {
            unix_sock_track_fill_action(unix_sock_track_tmp, SK_PF, SK_POLL_MASK);
        }
    }
   
    //////////////add code end///////////////////
#endif/* CONFIG_UNIX_SOCKET_TRACK_TOOL */  

	return mask;
}

static unsigned int unix_dgram_poll(struct file *file, struct socket *sock,
				    poll_table *wait)
{
	struct sock *sk = sock->sk, *other;
	unsigned int mask, writable;
#ifdef CONFIG_UNIX_SOCKET_TRACK_TOOL	
    //////////////add code begin///////////////////
    unix_sock_track_blc* unix_sock_track_tmp = NULL;
    //////////////add code end///////////////////

    //unix_test_time_log(sock, 3);
    //////////////add code begin///////////////////  

    if (SOCK_INODE(sock))
    {
        unix_sock_track_tmp = unix_sock_track_find_blc_with_action(2, NULL, SOCK_INODE(sock)->i_ino);
    }
  
    if (unix_sock_track_tmp != NULL)
    {
        unix_sock_track_fill_action(unix_sock_track_tmp, SK_PW, SK_POLL_MASK);
    }
    //////////////add code end///////////////////
#endif/* CONFIG_UNIX_SOCKET_TRACK_TOOL */

	sock_poll_wait(file, sk_sleep(sk), wait);
	mask = 0;

	/* exceptional events? */
	if (sk->sk_err || !skb_queue_empty(&sk->sk_error_queue))
		mask |= POLLERR |
			(sock_flag(sk, SOCK_SELECT_ERR_QUEUE) ? POLLPRI : 0);

	if (sk->sk_shutdown & RCV_SHUTDOWN)
		mask |= POLLRDHUP | POLLIN | POLLRDNORM;
	if (sk->sk_shutdown == SHUTDOWN_MASK)
		mask |= POLLHUP;

	/* readable? */
	if (!skb_queue_empty(&sk->sk_receive_queue))
		mask |= POLLIN | POLLRDNORM;

	/* Connection-based need to check for termination and startup */
	if (sk->sk_type == SOCK_SEQPACKET) {
		if (sk->sk_state == TCP_CLOSE)
			mask |= POLLHUP;
		/* connection hasn't started yet? */
		if (sk->sk_state == TCP_SYN_SENT)
    {
#ifdef CONFIG_UNIX_SOCKET_TRACK_TOOL    	
        //////////////add code begin///////////////////    
        if (unix_sock_track_tmp != NULL)
        {
            if (mask != 0)
            {
                unix_sock_track_fill_action(unix_sock_track_tmp, SK_PT, SK_POLL_MASK);      
            }
            else
            {
                unix_sock_track_fill_action(unix_sock_track_tmp, SK_PF, SK_POLL_MASK);
            }
        }
       
        //////////////add code end///////////////////   
#endif/* CONFIG_UNIX_SOCKET_TRACK_TOOL */
        
			return mask;
	}
  }

	/* No write status requested, avoid expensive OUT tests. */
	if (!(poll_requested_events(wait) & (POLLWRBAND|POLLWRNORM|POLLOUT)))
  {
#ifdef CONFIG_UNIX_SOCKET_TRACK_TOOL  	
      //////////////add code begin///////////////////    
      if (unix_sock_track_tmp != NULL)
      {
          if (mask != 0)
          {
              unix_sock_track_fill_action(unix_sock_track_tmp, SK_PT, SK_POLL_MASK);      
          }
          else
          {
              unix_sock_track_fill_action(unix_sock_track_tmp, SK_PF, SK_POLL_MASK);
          }
      }
     
      //////////////add code end/////////////////// 
#endif/* CONFIG_UNIX_SOCKET_TRACK_TOOL */
        
		return mask;
  }

	writable = unix_writable(sk);
	other = unix_peer_get(sk);
	if (other) {
		if (unix_peer(other) != sk) {
			sock_poll_wait(file, &unix_sk(other)->peer_wait, wait);
			if (unix_recvq_full(other))
				writable = 0;
		}
		sock_put(other);
	}

	if (writable)
		mask |= POLLOUT | POLLWRNORM | POLLWRBAND;
	else
		set_bit(SOCK_ASYNC_NOSPACE, &sk->sk_socket->flags);
#ifdef CONFIG_UNIX_SOCKET_TRACK_TOOL		
    //////////////add code begin///////////////////    
    if (unix_sock_track_tmp != NULL)
    {
        if (mask & (POLLIN | POLLERR | POLLHUP))
        {
            unix_sock_track_fill_action(unix_sock_track_tmp, SK_PT, SK_POLL_MASK);      
        }
        else
        {
            unix_sock_track_fill_action(unix_sock_track_tmp, SK_PF, SK_POLL_MASK);
        }
   }

   //////////////add code end///////////////////
#endif/* CONFIG_UNIX_SOCKET_TRACK_TOOL */

	return mask;
}

#ifdef CONFIG_PROC_FS

#define BUCKET_SPACE (BITS_PER_LONG - (UNIX_HASH_BITS + 1) - 1)

#define get_bucket(x) ((x) >> BUCKET_SPACE)
#define get_offset(x) ((x) & ((1L << BUCKET_SPACE) - 1))
#define set_bucket_offset(b, o) ((b) << BUCKET_SPACE | (o))

static struct sock *unix_from_bucket(struct seq_file *seq, loff_t *pos)
{
	unsigned long offset = get_offset(*pos);
	unsigned long bucket = get_bucket(*pos);
	struct sock *sk;
	unsigned long count = 0;

	for (sk = sk_head(&unix_socket_table[bucket]); sk; sk = sk_next(sk)) {
		if (sock_net(sk) != seq_file_net(seq))
			continue;
		if (++count == offset)
			break;
	}

	return sk;
}

static struct sock *unix_next_socket(struct seq_file *seq,
				     struct sock *sk,
				     loff_t *pos)
{
	unsigned long bucket;

	while (sk > (struct sock *)SEQ_START_TOKEN) {
		sk = sk_next(sk);
		if (!sk)
			goto next_bucket;
		if (sock_net(sk) == seq_file_net(seq))
			return sk;
	}

	do {
		sk = unix_from_bucket(seq, pos);
		if (sk)
			return sk;

next_bucket:
		bucket = get_bucket(*pos) + 1;
		*pos = set_bucket_offset(bucket, 1);
	} while (bucket < ARRAY_SIZE(unix_socket_table));

	return NULL;
}

static void *unix_seq_start(struct seq_file *seq, loff_t *pos)
	__acquires(unix_table_lock)
{
	spin_lock(&unix_table_lock);

	if (!*pos)
		return SEQ_START_TOKEN;

	if (get_bucket(*pos) >= ARRAY_SIZE(unix_socket_table))
		return NULL;

	return unix_next_socket(seq, NULL, pos);
}

static void *unix_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	++*pos;
	return unix_next_socket(seq, v, pos);
}

static void unix_seq_stop(struct seq_file *seq, void *v)
	__releases(unix_table_lock)
{
	spin_unlock(&unix_table_lock);
}

static int unix_seq_show(struct seq_file *seq, void *v)
{

	if (v == SEQ_START_TOKEN)
		seq_puts(seq, "Num       RefCount Protocol Flags    Type St "
			 "Inode Path\n");
	else {
		struct sock *s = v;
		struct unix_sock *u = unix_sk(s);
		unix_state_lock(s);

		seq_printf(seq, "%pK: %08X %08X %08X %04X %02X %5lu",
			s,
			atomic_read(&s->sk_refcnt),
			0,
			s->sk_state == TCP_LISTEN ? __SO_ACCEPTCON : 0,
			s->sk_type,
			s->sk_socket ?
			(s->sk_state == TCP_ESTABLISHED ? SS_CONNECTED : SS_UNCONNECTED) :
			(s->sk_state == TCP_ESTABLISHED ? SS_CONNECTING : SS_DISCONNECTING),
			sock_i_ino(s));

		if (u->addr) {
			int i, len;
			seq_putc(seq, ' ');

			i = 0;
			len = u->addr->len - sizeof(short);
			if (!UNIX_ABSTRACT(s))
				len--;
			else {
				seq_putc(seq, '@');
				i++;
			}
			for ( ; i < len; i++)
				seq_putc(seq, u->addr->name->sun_path[i]);
		}
		unix_state_unlock(s);
		seq_putc(seq, '\n');
	}

	return 0;
}

static const struct seq_operations unix_seq_ops = {
	.start  = unix_seq_start,
	.next   = unix_seq_next,
	.stop   = unix_seq_stop,
	.show   = unix_seq_show,
};

static int unix_seq_open(struct inode *inode, struct file *file)
{
	return seq_open_net(inode, file, &unix_seq_ops,
			    sizeof(struct seq_net_private));
}

static const struct file_operations unix_seq_fops = {
	.owner		= THIS_MODULE,
	.open		= unix_seq_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release_net,
};

#endif

static const struct net_proto_family unix_family_ops = {
	.family = PF_UNIX,
	.create = unix_create,
	.owner	= THIS_MODULE,
};


static int __net_init unix_net_init(struct net *net)
{
	int error = -ENOMEM;

	net->unx.sysctl_max_dgram_qlen = 10;
	if (unix_sysctl_register(net))
		goto out;

#ifdef CONFIG_PROC_FS
	if (!proc_create("unix", 0, net->proc_net, &unix_seq_fops)) {
		unix_sysctl_unregister(net);
		goto out;
	}
#endif
	error = 0;
out:
	return error;
}

static void __net_exit unix_net_exit(struct net *net)
{
	unix_sysctl_unregister(net);
	remove_proc_entry("unix", net->proc_net);
}

static struct pernet_operations unix_net_ops = {
	.init = unix_net_init,
	.exit = unix_net_exit,
};

static int __init af_unix_init(void)
{
	int rc = -1;

	BUILD_BUG_ON(sizeof(struct unix_skb_parms) > FIELD_SIZEOF(struct sk_buff, cb));

	rc = proto_register(&unix_proto, 1);
	if (rc != 0) {
		printk(KERN_CRIT "%s: Cannot create unix_sock SLAB cache!\n",
		       __func__);
		goto out;
	}

	sock_register(&unix_family_ops);
	register_pernet_subsys(&unix_net_ops);
out:
	return rc;
}

static void __exit af_unix_exit(void)
{
	sock_unregister(PF_UNIX);
	proto_unregister(&unix_proto);
	unregister_pernet_subsys(&unix_net_ops);
}

/* Earlier than device_initcall() so that other drivers invoking
   request_module() don't end up in a loop when modprobe tries
   to use a UNIX socket. But later than subsys_initcall() because
   we depend on stuff initialised there */
fs_initcall(af_unix_init);
module_exit(af_unix_exit);

MODULE_LICENSE("GPL");
MODULE_ALIAS_NETPROTO(PF_UNIX);
