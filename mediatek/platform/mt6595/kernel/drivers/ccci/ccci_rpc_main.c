/*****************************************************************************
 *
 * Filename:
 * ---------
 *   ccci_fs.c
 *
 * Project:
 * --------
 *   YuSu
 *
 * Description:
 * ------------
 *   MT6516 CCCI RPC
 *
 * Author:
 * -------
 *   
 *
 ****************************************************************************/

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/kfifo.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/semaphore.h>
#include <linux/version.h>
#include <ccci.h>


#if 0
//#define ENCRYPT_DEBUG
//#define CCCI_RPC_DEBUG_ON
#ifdef CCCI_RPC_DEBUG_ON
#define RPC_DBG(format, args...)  	printk("ccci_rpc: " format, ##args)
#else
#define RPC_DBG(format, ...)
#endif
#endif

//#define RPC_TST
#ifndef RPC_TST
#if 0
extern void SST_Secure_Algo(kal_uint8 Direction, kal_uint32 ContentAddr, 
						kal_uint32 ContentLen, kal_uint8 *CustomSeed, kal_uint8 *ResText);
extern bool SST_Secure_Init(void);
extern bool SST_Secure_DeInit(void);
#endif 
#else
void SST_Secure_Algo(kal_uint8 Direction, kal_uint32 ContentAddr, 
						kal_uint32 ContentLen, kal_uint8 *CustomSeed, kal_uint8 *ResText){}
bool SST_Secure_Init(void){}
bool SST_Secure_DeInit(void){}
#endif
static RPC_BUF			*rpc_buf_vir;
static unsigned int     rpc_buf_phy;
static unsigned int     rpc_buf_len;
//static spinlock_t       rpc_fifo_lock = SPIN_LOCK_UNLOCKED;
static DEFINE_SPINLOCK(rpc_fifo_lock);
static struct kfifo     rpc_fifo;
struct work_struct      rpc_work;

//#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36))
//DECLARE_MUTEX(sej_sem);
//#else
//DEFINE_SEMAPHORE(sej_sem);
//#endif

unsigned char tst_tmp[512];
unsigned char tst_tmp1[512];
unsigned int tst_len=512;

static void security_api_tst(unsigned long data);

struct timer_list tst_timer = TIMER_INITIALIZER(security_api_tst, 0, 0);

static void security_api_tst(unsigned long data)
{
	unsigned int i = 0, curr=0;
	sed_t CustomSeed = SED_INITIALIZER;
	unsigned char buf[128];
	
	CCCI_RPC_MSG("security_api_tst: DeCrypt");			
	SST_Secure_Algo(false, (kal_uint32)tst_tmp, tst_len, CustomSeed.sed, tst_tmp1);

	CCCI_RPC_MSG("\n");
	for(i = 0; i < tst_len; i++)
	{
		curr += snprintf(&buf[curr], sizeof(buf)-curr, "%02X ", tst_tmp1[i]);
		if( (i!=0) && (i % 16 == 0) ){
			curr += snprintf(&buf[curr], sizeof(buf)-curr, "\n");
			curr = 0;
			CCCI_RPC_MSG("%s", buf);
		}
		//CCCI_RPC_MSG("%02X ", tst_tmp1[i]);
	}
	if(curr!=0){
		curr += snprintf(&buf[curr], sizeof(buf)-curr, "\n");
		curr = 0;
		CCCI_RPC_MSG("%s", buf);
	}
	mod_timer(&tst_timer, jiffies + msecs_to_jiffies(2000));
}


static bool get_pkt_info(unsigned int* pktnum, RPC_PKT* pkt_info, char* pdata)
{
	unsigned int pkt_num = *((unsigned int*)pdata);
	unsigned int idx = 0;
	unsigned int i = 0;

	CCCI_RPC_MSG("package number = 0x%08X\n", pkt_num);
	
	if(pkt_num > IPC_RPC_MAX_ARG_NUM)
		return false;
	
	idx = sizeof(unsigned int);
	for(i = 0; i < pkt_num; i++)
	{
		pkt_info[i].len = *((unsigned int*)(pdata + idx));
		idx += sizeof(unsigned int);
		pkt_info[i].buf = (pdata + idx);

		CCCI_RPC_MSG("pak[%d]: vir = 0x%08X, len = 0x%08X\n", i, (unsigned int)pkt_info[i].buf, pkt_info[i].len);

		// 4 byte alignment
		idx += ((pkt_info[i].len+3)>>2)<<2;
	}
	
	if(idx > IPC_RPC_MAX_BUF_SIZE)
	{
		CCCI_MSG_INF("rpc", "over flow, pdata = %p, idx = 0x%08X, max = %p\n", 
								pdata, idx, pdata + IPC_RPC_MAX_BUF_SIZE);
		return false;
	}
	*pktnum = pkt_num;
	
	return true;
}

static bool rpc_write(int buf_idx, RPC_PKT* pkt_src, unsigned int pkt_num)
{
	int ret = CCCI_FAIL;
	CCCI_BUFF_T     stream = { {0}, };
	RPC_BUF	*rpc_buf_tmp = NULL;
	unsigned char	*pdata = NULL;
	unsigned int 	data_len = 0;
	unsigned int 	i = 0;
	unsigned int	AlignLength = 0;

	rpc_buf_tmp = rpc_buf_vir + buf_idx;
	rpc_buf_tmp->op_id = IPC_RPC_API_RESP_ID | rpc_buf_tmp->op_id;
	pdata = rpc_buf_tmp->buf;
	*((unsigned int*)pdata) = pkt_num;

	pdata += sizeof(unsigned int);
	data_len += sizeof(unsigned int);

	for(i = 0; i < pkt_num; i++)
	{
		if((data_len + 2*sizeof(unsigned int) + pkt_src[i].len) > IPC_RPC_MAX_BUF_SIZE)
		{
			CCCI_MSG_INF("rpc", "Stream buffer full!!\n");
			goto _Exit;
		}
		
		*((unsigned int*)pdata) = pkt_src[i].len;
		pdata += sizeof(unsigned int);
		data_len += sizeof(unsigned int);
			
		// 4  byte aligned
		AlignLength = ((pkt_src[i].len + 3) >> 2) << 2;
		data_len += AlignLength;
			
		if(pdata != pkt_src[i].buf)
			memcpy(pdata, pkt_src[i].buf, pkt_src[i].len);
		else
			CCCI_RPC_MSG("same addr, no copy\n");
					
		pdata += AlignLength;
	}

	stream.data[0]  = rpc_buf_phy + (sizeof(RPC_BUF) * buf_idx);
	stream.data[1]  = data_len + 4;
	stream.reserved = buf_idx;

	CCCI_RPC_MSG("Write, %08X, %08X, %08X, %08X\n", 
			stream.data[0], stream.data[1], stream.channel, stream.reserved);

	ret = ccci_write(CCCI_RPC_TX, &stream);
	if(ret != CCCI_SUCCESS)
	{
		CCCI_MSG_INF("rpc", "fail send msg <%d>!!!\n", ret);
		return ret;
	}
			
_Exit:
	return ret;
}
static void ccci_rpc_work(struct work_struct *work  __always_unused)
{
	int pkt_num = 0;		
	int ret_val = 0;
	unsigned int buf_idx = 0;
	RPC_PKT pkt[IPC_RPC_MAX_ARG_NUM] = { {0}, };
	RPC_BUF *rpc_buf_tmp = NULL;
	unsigned char log_buf[128];
	
	CCCI_RPC_MSG("ccci_rpc_work++\n");
				
	if(rpc_buf_vir == NULL)
	{
		CCCI_MSG_INF("rpc", "invalid rpc_buf_vir!!\n");
		return ;			
	}

	//RPC_DBG("rpc_buf: phy = 0x%08X, vir = 0x%08X, len = 0x%08X\n",
	//					rpc_buf_phy, rpc_buf_vir, rpc_buf_len);
	
	while(kfifo_out(&rpc_fifo, &buf_idx, sizeof(unsigned int)))
	{
//		int m = 0;
		if(buf_idx < 0 || buf_idx > IPC_RPC_REQ_BUFFER_NUM)
		{
			CCCI_MSG_INF("rpc", "invalid idx %d\n", buf_idx);
			ret_val = FS_PARAM_ERROR;
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void*) &ret_val;
			goto _Next;
		}	
		//RPC_DBG("rpc_buf: index = 0x%08X\n", buf_idx);

		pkt_num = 0;
		memset(pkt, 0x00, sizeof(RPC_PKT)*IPC_RPC_MAX_ARG_NUM);

		rpc_buf_tmp = rpc_buf_vir + buf_idx;
#if 0
		for(m = 0; m < IPC_RPC_MAX_BUF_SIZE + sizeof(unsigned int); m++)
		{
			if(m % 16 == 0)
				printk("\nSHM: ");
			printk("%02X ", *((unsigned char*)rpc_buf_tmp+m));					
			msleep(1);
		}				
		printk("\n\n");
#endif			
		if(!get_pkt_info(&pkt_num, pkt, rpc_buf_tmp->buf))
		{
			CCCI_MSG_INF("rpc", "Fail to get packet info\n");
			ret_val = FS_PARAM_ERROR;
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void*) &ret_val;
			goto _Next;
		}
				
		switch(rpc_buf_tmp->op_id)
		{
		case IPC_RPC_CPSVC_SECURE_ALGO_OP:
		{
			unsigned char Direction = 0;
			unsigned int  ContentAddr = 0;
			unsigned int  ContentLen = 0;
			sed_t CustomSeed = SED_INITIALIZER;
			unsigned char *ResText __always_unused= NULL;
			unsigned char *RawText __always_unused= NULL;
			unsigned int i __always_unused= 0;

			if(pkt_num < 4)
			{
				CCCI_MSG_INF("rpc", "invalid pkt_num %d for RPC_SECURE_ALGO_OP!\n", pkt_num);
				ret_val = FS_PARAM_ERROR;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void*) &ret_val;
				goto _Next;
			}

			Direction = *(unsigned char*)pkt[0].buf;
			ContentAddr = (unsigned int)pkt[1].buf;				
			CCCI_RPC_MSG("RPC_SECURE_ALGO_OP: Content_Addr = 0x%08X, RPC_Base = 0x%08X, RPC_Len = 0x%08X\n", 
				ContentAddr, (unsigned int)rpc_buf_vir, CCCI_RPC_SMEM_SIZE);
			if(ContentAddr < (unsigned int)rpc_buf_vir || 
							ContentAddr > ((unsigned int)rpc_buf_vir + CCCI_RPC_SMEM_SIZE))
			{
				CCCI_MSG_INF("rpc", "invalid ContentAdddr[0x%08X] for RPC_SECURE_ALGO_OP!\n", ContentAddr);
				ret_val = FS_PARAM_ERROR;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void*) &ret_val;
				goto _Next;
			}
			ContentLen = *(unsigned int*)pkt[2].buf;	
			//	CustomSeed = *(sed_t*)pkt[3].buf;
			WARN_ON(sizeof(CustomSeed.sed)<pkt[3].len);
			memcpy(CustomSeed.sed,pkt[3].buf,pkt[3].len);

#ifdef ENCRYPT_DEBUG
			if(Direction == TRUE)
				CCCI_MSG_INF("rpc", "SEJ_S: EnCrypt_src:\n");
			else
				CCCI_MSG_INF("rpc", "SEJ_S: DeCrypt_src:\n");
			for(i = 0; i < ContentLen; i++)
			{
				if(i % 16 == 0){
					if(i!=0){
						CCCI_RPC_MSG("%s\n", log_buf);
					}
					curr = 0;
					curr += snprintf(log_buf, sizeof(log_buf)-curr, "SEJ_S: ");
					//CCCI_MSG("\nSEJ_S: ");
				}
				//CCCI_MSG("0x%02X ", *(unsigned char*)(ContentAddr+i));
				curr += snprintf(&log_buf[curr], sizeof(log_buf)-curr, "0x%02X ", *(unsigned char*)(ContentAddr+i));					
				//sleep(1);
			}
			CCCI_RPC_MSG("%s\n", log_buf);
				
			RawText = kmalloc(ContentLen, GFP_KERNEL);
			if(RawText == NULL)
			{
				CCCI_MSG_INF("rpc", "Fail alloc Mem for RPC_SECURE_ALGO_OP!\n");
			}
			else
				memcpy(RawText, (unsigned char*)ContentAddr, ContentLen);
#endif
			ResText = kmalloc(ContentLen, GFP_KERNEL);
			if(ResText == NULL)
			{
				CCCI_RPC_MSG("rpc", "Fail alloc Mem for RPC_SECURE_ALGO_OP!\n");
				ret_val = FS_PARAM_ERROR;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void*) &ret_val;
				goto _Next;
			}
			//down(&sej_sem);
			if(!SST_Secure_Init())
			{
				CCCI_MSG_INF("rpc", "SST_Secure_Init fail!\n");
				ASSERT(0);
			}

			//tomSeed = 0x52515049;
			CCCI_RPC_MSG("RPC_SECURE_ALGO_OP: Dir=0x%08X, Addr=0x%08X, Len=0x%08X, Seed=0x%016llX\n", 
				Direction, ContentAddr, ContentLen, *(long long *)CustomSeed.sed);
			SST_Secure_Algo(Direction, ContentAddr, ContentLen, CustomSeed.sed, ResText);

			if(!SST_Secure_DeInit())
				CCCI_MSG_INF("rpc", "SST_Secure_DeInit fail!\n");
			//up(&sej_sem);
			pkt_num = 0;
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void*) &ret_val;
			pkt[pkt_num].len = ContentLen;	
			//pkt[pkt_num++].buf = ResText;
			memcpy(pkt[pkt_num++].buf, ResText, ContentLen);
#ifdef ENCRYPT_DEBUG
		#if 0		//tst
			if(tst_len == 0 && tst_tmp != NULL && Direction == TRUE)
			{
				memcpy(tst_tmp, ResText, ContentLen);
				tst_len = ContentLen;
				mod_timer(&tst_timer, jiffies + msecs_to_jiffies(2000));
			}
		#endif
			if(Direction == TRUE)
				CCCI_RPC_MSG("SEJ_D: EnCrypt_dst:\n");
			else
				CCCI_RPC_MSG("SEJ_D: DeCrypt_dst:\n");
			for(i = 0; i < ContentLen; i++)
			{
				if(i % 16 == 0){
					if(i!=0){
						CCCI_RPC_MSG("%s\n", log_buf);
					}
					curr = 0;
					curr += snprintf(&log_buf[curr], sizeof(log_buf)-curr, "SEJ_D: ");
				}
				//CCCI_MSG("%02X ", *(ResText+i));
				curr += snprintf(&log_buf[curr], sizeof(log_buf)-curr, "0x%02X ", *(ResText+i));
				//sleep(1);
			}
			
			CCCI_RPC_MSG("%s\n", log_buf);
#if 0
			if(Direction == true)
			{
				RPC_DBG("DeCrypt_back:");			
				SST_Secure_Algo(false, (unsigned int)pkt[pkt_num-1].buf, ContentLen, &CustomSeed, ResText);
			}
			else
			{
				RPC_DBG("EnCrypt_back:");
				SST_Secure_Algo(true, (unsigned int)pkt[pkt_num-1].buf, ContentLen, &CustomSeed, ResText);
			}
			for(i = 0; i < ContentLen; i++)
			{
				if(i % 16 == 0)
					printk("\nSEJ_B: ");
				printk("%02X ", *(ResText+i));					
				msleep(10);
			}				
			printk("\n\n");
			msleep(100);

			if(memcmp(ResText, RawText, ContentLen))
				RPC_DBG("Compare Fail!\n");
#endif

			if(RawText)
				kfree(RawText);


#endif
			kfree(ResText);
			break;
		}		

		default:
			CCCI_MSG_INF("rpc", "Unknow Operation ID (%d)\n", rpc_buf_tmp->op_id);			
			ret_val = FS_PARAM_ERROR;
			pkt_num = 0;
			pkt[pkt_num].len = sizeof(int);
			pkt[pkt_num++].buf = (void*) &ret_val;	
		}
_Next:
		if(rpc_write(buf_idx, pkt, pkt_num) != CCCI_SUCCESS)
		{
			CCCI_MSG_INF("rpc", "fail to write packet!!\r\n");
			return ;
		}		
	} 

	CCCI_RPC_MSG("ccci_rpc_work--\n");    
}

static void ccci_rpc_callback(CCCI_BUFF_T *buff, void *private_data)
{
	if (buff->channel==CCCI_RPC_TX) 
		CCCI_MSG_INF("rpc", "Wrong CH for recv.\n");
	CCCI_RPC_MSG("callback, %08X, %08X, %08X, %08X\n", 
		buff->data[0], buff->data[1], buff->channel, buff->reserved);
	spin_lock_bh(&rpc_fifo_lock);
	kfifo_in(&rpc_fifo, &buff->reserved, sizeof(unsigned int));
	spin_unlock_bh(&rpc_fifo_lock);
   	schedule_work(&rpc_work);
	CCCI_RPC_MSG("callback --\n");    
}

int __init ccci_rpc_init(void)
{ 
    int ret;	
    ret=kfifo_alloc(&rpc_fifo,sizeof(unsigned) * IPC_RPC_REQ_BUFFER_NUM, GFP_KERNEL);
    if (ret)
    {
        CCCI_MSG_INF("rpc", "Unable to create fifo\n");
        return ret;
    }
  

#if 0
	rpc_work_queue = create_workqueue(WORK_QUE_NAME);
	if(rpc_work_queue == NULL)
	{
        printk(KERN_ERR "Fail create rpc_work_queue!!\n");
        return -EFAULT;
	}
#endif
    INIT_WORK(&rpc_work, ccci_rpc_work);

    // modem related channel registration.
    ASSERT(ccci_rpc_setup((int*)&rpc_buf_vir, &rpc_buf_phy, &rpc_buf_len) == CCCI_SUCCESS);
    CCCI_RPC_MSG("rpc_buf_vir=0x%p, rpc_buf_phy=0x%08X, rpc_buf_len=0x%08X\n", rpc_buf_vir, rpc_buf_phy, rpc_buf_len);
	ASSERT(rpc_buf_vir != NULL);
	ASSERT(rpc_buf_len != 0);
    ASSERT(ccci_register(CCCI_RPC_TX, ccci_rpc_callback, NULL) == CCCI_SUCCESS);
    ASSERT(ccci_register(CCCI_RPC_RX, ccci_rpc_callback, NULL) == CCCI_SUCCESS);
    //ASSERT(ccci_register(CCCI_RPC_TX, NULL, NULL) == CCCI_SUCCESS);
    return 0;
}

void __exit ccci_rpc_exit(void)
{
	CCCI_RPC_MSG("deinit\n");
#if 0
    if(cancel_work_sync(&rpc_work)< 0)
    {
		printk("ccci_rpc: Cancel rpc work fail!\n");
    }
	//destroy_workqueue(rpc_work_queue);
#endif
	
	kfifo_free(&rpc_fifo);
	ccci_unregister(CCCI_RPC_RX);
	ccci_unregister(CCCI_RPC_TX);

}

//EXPORT_SYMBOL(sej_sem);
#if 0
module_init(ccci_rpc_init);
module_exit(ccci_rpc_exit);

EXPORT_SYMBOL(SST_Secure_Init);
EXPORT_SYMBOL(SST_Secure_DeInit);
MODULE_DESCRIPTION("MEDIATEK CCCI RPC");
MODULE_AUTHOR("MEDIATEK");
MODULE_LICENSE("Proprietary");
#endif 
