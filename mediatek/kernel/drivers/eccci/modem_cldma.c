/*
 * this is a CLDMA modem driver.
 *
 * V0.1: Xiao Wang <xiao.wang@mediatek.com>
 */

#include <linux/list.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/skbuff.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/netdevice.h>
#include <linux/random.h>
#include <linux/platform_device.h>
#include <mach/mt_boot.h>
#include "ccci_core.h"
#include "ccci_bm.h"
#include "ccci_dfo.h"
#include "ccci_sysfs.h"
#include "ccci_platform.h"
#include "modem_cldma.h"
#include "cldma_platform.h"
#include "cldma_reg.h"
#ifdef CCCI_STATISTIC
#define CREATE_TRACE_POINTS
#include "modem_cldma_events.h"
#endif

// always keep this in mind: what if there are more than 1 modems using CLDMA...

extern unsigned int ccci_get_md_debug_mode(struct ccci_modem *md);
static int md_cd_ccif_send(struct ccci_modem *md, int channel_id);

// Port mapping
extern struct ccci_port_ops char_port_ops;
extern struct ccci_port_ops net_port_ops;
extern struct ccci_port_ops kernel_port_ops;
extern struct ccci_port_ops ipc_port_ack_ops;
extern struct ccci_port_ops ipc_kern_port_ops;
static struct ccci_port md_cd_ports_normal[] = {
{CCCI_MONITOR_CH,	CCCI_MONITOR_CH,	0xFF,	0xFF,	0xFF,	0xFF,	4,	&char_port_ops, 	0,	"ccci_monitor",	},
{CCCI_PCM_TX,		CCCI_PCM_RX,		0,		0,		0xFF,	0xFF,	4,	&char_port_ops, 	1,	"ccci_aud",	},
{CCCI_CONTROL_TX,	CCCI_CONTROL_RX,	0,		0,		0,		0,		0,	&kernel_port_ops,	0,	"ccci_ctrl",	},
{CCCI_SYSTEM_TX,	CCCI_SYSTEM_RX, 	0,		0,		0xFF,	0xFF,	0,	&kernel_port_ops,	0,	"ccci_sys",	},
{CCCI_STATUS_TX,	CCCI_STATUS_RX, 	0,		0,		0,		0,		0,	&kernel_port_ops,	0,	"ccci_poll",	},

{CCCI_UART1_TX, 	CCCI_UART1_RX,		1,		1,		3,		3,		0,	&char_port_ops, 	2,	"ccci_md_log_ctrl",	},
{CCCI_UART2_TX, 	CCCI_UART2_RX,		1,		1,		0xFF,	0xFF,	0,	&char_port_ops, 	3,	"ttyC0",	},
{CCCI_FS_TX,		CCCI_FS_RX, 		1,		1,		1,		1,		4,	&char_port_ops, 	4,	"ccci_fs",	},
{CCCI_IPC_UART_TX,	CCCI_IPC_UART_RX,	1,		1,		0xFF,	0xFF,	0,	&char_port_ops, 	5,	"ttyC2",	},
{CCCI_ICUSB_TX, 	CCCI_ICUSB_RX,		1,		1,		0xFF,	0xFF,	0,	&char_port_ops, 	6,	"ttyC3",	},
{CCCI_RPC_TX,		CCCI_RPC_RX,		1,		1,		0xFF,	0xFF,	0,	&kernel_port_ops,	0,	"ccci_rpc",	},

{CCCI_IPC_TX,		CCCI_IPC_RX,		1,		1,		0xFF,	0xFF,	0,	&char_port_ops, 	0,	"ccci_ipc_1220_0",	},
{CCCI_IPC_TX,		CCCI_IPC_RX,		1,		1,		0xFF,	0xFF,	0,	&char_port_ops, 	2,	"ccci_ipc_2",	},
{CCCI_IPC_TX,		CCCI_IPC_RX,		1,		1,		0xFF,	0xFF,	0,	&ipc_kern_port_ops,	3,	"ccci_ipc_3",	},

{CCCI_MD_LOG_TX,	CCCI_MD_LOG_RX, 	2,		2,		2,		2,		8,	&char_port_ops, 	7,	"ttyC1",	},

{CCCI_CCMNI1_TX,	CCCI_CCMNI1_RX, 	3,		3,		0xF4,	0xFF,	8,	&net_port_ops,		0,	"ccmni0",	},
{CCCI_CCMNI2_TX,	CCCI_CCMNI2_RX, 	3,		4,		0xF4,	0xFF,	8,	&net_port_ops,		0,	"ccmni1",	},
{CCCI_CCMNI3_TX,	CCCI_CCMNI3_RX, 	5,		5,		0xFF,	0xFF,	8,	&net_port_ops,		0,	"ccmni2",	},

{CCCI_IMSV_UL,		CCCI_IMSV_DL,		6,		6,		0xFF,	0xFF,	0,	&char_port_ops, 	8,	"ccci_imsv",	},
{CCCI_IMSC_UL,		CCCI_IMSC_DL,		6,		6,		0xFF,	0xFF,	0,	&char_port_ops, 	9,	"ccci_imsc",	},
{CCCI_IMSA_UL,		CCCI_IMSA_DL,		6,		6,		0xFF,	0xFF,	0,	&char_port_ops, 	10, "ccci_imsa",	},
{CCCI_IMSDC_UL, 	CCCI_IMSDC_DL,		6,		6,		0xFF,	0xFF,	0,	&char_port_ops, 	11, "ccci_imsdc",	},

{CCCI_DUMMY_CH, 	CCCI_DUMMY_CH,		0xFF,	0xFF,	0xFF,	0xFF,	0,	&char_port_ops, 	12, "ccci_ioctl0",	},
{CCCI_DUMMY_CH, 	CCCI_DUMMY_CH,		0xFF,	0xFF,	0xFF,	0xFF,	0,	&char_port_ops, 	13, "ccci_ioctl1",	},
{CCCI_DUMMY_CH, 	CCCI_DUMMY_CH,		0xFF,	0xFF,	0xFF,	0xFF,	0,	&char_port_ops, 	14, "ccci_ioctl2",	},
{CCCI_DUMMY_CH, 	CCCI_DUMMY_CH,		0xFF,	0xFF,	0xFF,	0xFF,	0,	&char_port_ops, 	15, "ccci_ioctl3",	},
{CCCI_DUMMY_CH, 	CCCI_DUMMY_CH,		0xFF,	0xFF,	0xFF,	0xFF,	0,	&char_port_ops, 	16, "ccci_ioctl4",	},

{CCCI_IT_TX,		CCCI_IT_RX, 		0,		0,		0xFF,	0xFF,	4,	&char_port_ops, 	17, "eemcs_it",	},
{CCCI_LB_IT_TX, 	CCCI_LB_IT_RX,		0,		0,		0xFF,	0xFF,	0,	&char_port_ops, 	18, "eemcs_lb_it",	},
};

// CLDMA setting
/* 
 * we use this as rgpd->data_allow_len, so skb length must be >= this size, check ccci_bm.c's skb pool design.
 * channel 3 is for network in normal mode, but for mdlogger_ctrl in exception mode, so choose the max packet size.
 */
static int rx_queue_buffer_size[8] = {SKB_4K, SKB_4K, SKB_4K, SKB_1_5K, SKB_1_5K, SKB_1_5K, SKB_4K, SKB_16};
static int rx_queue_buffer_number[8] = {16, 16, 16, 32, 32, 32, 16, 16};
static int tx_queue_buffer_number[8] = {16, 16, 16, 16, 16, 16, 16, 16};
static const unsigned char high_priority_queue_mask =  0x00;

#define NAPI_QUEUE_MASK 0x38 // Rx, only Rx-exclusive port can enable NAPI, set it to 0 if disabled NAPI
#define NONSTOP_QUEUE_MASK 0xF0 // Rx, for convenience, queue 0,1,2,3 are non-stop
#define NONSTOP_QUEUE_MASK_32 0xF0F0F0F0

#define CLDMA_CG_POLL 6
#define CLDMA_ACTIVE_T 20
#define BOOT_TIMER_ON 10
#define BOOT_TIMER_HS1 10

#define TAG "mcd"

/*
 * do NOT add any static data, data should be in modem's instance
 */

#if 0 // for debugging not finished EMI transaction
static void dump_latency(void)
{
	unsigned int i=0,addr=0;
	printk(KERN_CRIT "[EMI]1,0xF02034e8:0x%08x,0xF0203400:0x%08x\n",readl(IOMEM(0xF02034e8)),readl(IOMEM(0xF0203400)));
	mt65xx_reg_sync_writel(0x200000,0xF02034e8);
	mt65xx_reg_sync_writel(0xff0001,0xF0203400);
	printk(KERN_CRIT "[EMI]2,0xF02034e8:0x%08x,0xF0203400:0x%08x\n",readl(IOMEM(0xF02034e8)),readl(IOMEM(0xF0203400)));
	for(i=0;i<5;i++) {
		for(addr=0;addr<0x78;addr+=4) {
			printk(KERN_CRIT "[EMI]0x%x:0x%08x\n",0xF0203500+addr,readl(IOMEM(0xF0203500+addr)));
		}
	}
}
#endif

static void cldma_dump_gpd_ring(void *start, int size)
{
	// assume TGPD and RGPD's "next" pointers use the same offset
	struct cldma_tgpd *curr = (struct cldma_tgpd *)start;
	int i, *tmp;
	printk("[CLDMA] gpd starts from %p\n", start);
	/*
	 * virtual address get from dma_pool_alloc is not equal to phys_to_virt. 
	 * e.g. dma_pool_alloca returns 0xFFDFF00, and phys_to_virt will return 0xDF364000 for the same
	 * DMA address. therefore we can't compare gpd address with @start to exit loop.
	 */
	for(i=0; i<size; i++) {
		tmp = (int *) curr;
		printk("[CLDMA] %p: %X %X %X %X\n", curr, *tmp, *(tmp+1), *(tmp+2), *(tmp+3));
		curr = (struct cldma_tgpd *)phys_to_virt(curr->next_gpd_ptr);
	}
	printk("[CLDMA] end of ring %p\n", start);
}

static void cldma_dump_all_gpd(struct md_cd_ctrl *md_ctrl)
{
	int i;
	struct ccci_request *req = NULL;
	for(i=0; i<QUEUE_LEN(md_ctrl->txq); i++) {
		// use GPD's pointer to traverse
		printk("[CLDMA] dump txq %d GPD\n", i);
		req = list_entry(md_ctrl->txq[i].tr_ring, struct ccci_request, entry);
		cldma_dump_gpd_ring(req->gpd, tx_queue_buffer_number[i]);
#if 0 // UT code
		// use request's link head to traverse
		printk("[CLDMA] dump txq %d request\n", i);
		list_for_each_entry(req, md_ctrl->txq[i].tr_ring, entry) { // due to we do NOT have an extra head, this will miss the first request		
			printk("[CLDMA] %p (%x->%x)\n", req->gpd, 
				req->gpd_addr, ((struct cldma_tgpd *)req->gpd)->next_gpd_ptr);
		}
#endif
	}
	for(i=0; i<QUEUE_LEN(md_ctrl->rxq); i++) {
		// use GPD's pointer to traverse
		printk("[CLDMA] dump rxq %d GPD\n", i);
		req = list_entry(md_ctrl->rxq[i].tr_ring, struct ccci_request, entry);
		cldma_dump_gpd_ring(req->gpd, rx_queue_buffer_number[i]);
#if 0 // UT code
		// use request's link head to traverse
		printk("[CLDMA] dump rxq %d request\n", i);
		list_for_each_entry(req, md_ctrl->rxq[i].tr_ring, entry) {
			printk("[CLDMA] %p/%p (%x->%x)\n", req->gpd, req->skb,
				req->gpd_addr, ((struct cldma_rgpd *)req->gpd)->next_gpd_ptr);
		}
#endif
	}
}

static void cldma_dump_register(struct md_cd_ctrl *md_ctrl)
{
	md_cd_lock_cldma_clock_src(1);
	printk("[CCCI-DUMP]dump AP CLDMA Tx register, active=%x\n", md_ctrl->txq_active);
	ccci_mem_dump(md_ctrl->cldma_ap_base+CLDMA_AP_UL_START_ADDR_0, CLDMA_AP_UL_CHECKSUM_CHANNEL_ENABLE-CLDMA_AP_UL_START_ADDR_0+4);
	printk("[CCCI-DUMP]dump AP CLDMA Rx register, active=%x\n", md_ctrl->rxq_active);
	ccci_mem_dump(md_ctrl->cldma_ap_base+CLDMA_AP_SO_ERROR, CLDMA_AP_DEBUG_ID_EN-CLDMA_AP_SO_ERROR+4);
	printk("[CCCI-DUMP]dump AP CLDMA MISC register\n");
	ccci_mem_dump(md_ctrl->cldma_ap_base+CLDMA_AP_L2TISAR0, CLDMA_AP_CHNL_IDLE-CLDMA_AP_L2TISAR0+4);
	printk("[CCCI-DUMP]dump MD CLDMA Tx register\n");
	ccci_mem_dump(md_ctrl->cldma_md_base+CLDMA_AP_UL_START_ADDR_0, CLDMA_AP_UL_CHECKSUM_CHANNEL_ENABLE-CLDMA_AP_UL_START_ADDR_0+4);
	printk("[CCCI-DUMP]dump MD CLDMA Rx register\n");
	ccci_mem_dump(md_ctrl->cldma_md_base+CLDMA_AP_SO_ERROR, CLDMA_AP_DEBUG_ID_EN-CLDMA_AP_SO_ERROR+4);
	printk("[CCCI-DUMP]dump MD CLDMA MISC register\n");
	ccci_mem_dump(md_ctrl->cldma_md_base+CLDMA_AP_L2TISAR0, CLDMA_AP_CHNL_IDLE-CLDMA_AP_L2TISAR0+4);
	md_cd_lock_cldma_clock_src(0);
}

static void cldma_dump_packet_history(struct md_cd_ctrl *md_ctrl)
{
#if PACKET_HISTORY_DEPTH
	int i;
	for(i=0; i<QUEUE_LEN(md_ctrl->txq); i++) {
		printk("[CCCI-DUMP]dump txq%d packet history, ptr=%d, tr_done=%p, tx_xmit=%p\n", i, 
			md_ctrl->tx_history_ptr[i], md_ctrl->txq[i].tr_done->gpd, md_ctrl->txq[i].tx_xmit->gpd);
		ccci_mem_dump(md_ctrl->tx_history[i], sizeof(md_ctrl->tx_history[i]));
	}
	for(i=0; i<QUEUE_LEN(md_ctrl->rxq); i++) {
		printk("[CCCI-DUMP]dump rxq%d packet history, ptr=%d, tr_done=%p\n", i, 
			md_ctrl->rx_history_ptr[i], md_ctrl->rxq[i].tr_done->gpd);
		ccci_mem_dump(md_ctrl->rx_history[i], sizeof(md_ctrl->rx_history[i]));
	}
#endif
}

#if CHECKSUM_SIZE
static inline void caculate_checksum(char *address, char first_byte)
{
	int i;
	char sum = first_byte;
	for (i = 2 ; i < CHECKSUM_SIZE; i++)
		sum += *(address + i);
	*(address + 1) = 0xFF - sum; 
}
#else
#define caculate_checksum(address, first_byte)	 
#endif

static int cldma_queue_broadcast_state(struct ccci_modem *md, MD_STATE state, DIRECTION dir, int index)
{
	int i, match=0;;
	struct ccci_port *port;

	for(i=0;i<md->port_number;i++) {
		port = md->ports + i;
		// consider network data/ack queue design
		if(md->md_state==EXCEPTION)
			match = dir==OUT?index==port->txq_exp_index:index==port->rxq_exp_index;
		else
			match = dir==OUT?index==port->txq_index||index==(port->txq_exp_index&0x0F):index==port->rxq_index;
		if(match && port->ops->md_state_notice) {
			port->ops->md_state_notice(port, state);
		}
	}
}

// this function may be called from both workqueue and softirq (NAPI)
static int cldma_rx_collect(struct md_cd_queue *queue, int budget, int blocking, int *result)
{
	struct ccci_modem *md = queue->modem;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
#if 0
	static int hole = 0;
#endif
	
	struct ccci_request *req;
	struct cldma_rgpd *rgpd;
	struct ccci_request *new_req;
	struct ccci_header *ccci_h;
	int ret=0, count=0;
	*result = UNDER_BUDGET;

	// find all done RGPD, we didn't use any lock here, be careful with md_cd_clear_queue()
	while(1) { // not hardware own
		req = queue->tr_done;
		rgpd = (struct cldma_rgpd *)req->gpd;
		if(unlikely(!req->skb)) {
			// check this first, as tr_done should remain where we failed to allocate skb
			CCCI_ERR_MSG(md->index, TAG, "found a hole on q%d, try refill and move forward\n", queue->index);
			goto fill_and_move;
		}
		if((rgpd->gpd_flags&0x1) != 0) {
			break;
		}
		if(unlikely(req->skb->len!=0)) {
			// should never happen
			CCCI_ERR_MSG(md->index, TAG, "reuse skb %p with len %d\n", req->skb, req->skb->len);
			break;
		}
		// allocate a new wrapper, do nothing if this failed, just wait someone to collect this queue again, if lucky enough
		new_req = ccci_alloc_req(IN, -1, blocking, 0);
#if 0 // UT code
		if(hole < 50) {
			if(!blocking) hole++;
		} else {
			new_req->policy = NOOP;
			ccci_free_req(new_req);
			new_req = NULL;
			hole = 0;
		}
#endif
		if(unlikely(!new_req)) {
			CCCI_ERR_MSG(md->index, TAG, "alloc req fail on q%d\n", queue->index);
			*result = NO_REQ;
			break;
		}
		// update skb
		dma_unmap_single(NULL, rgpd->data_buff_bd_ptr, skb_size(req->skb), DMA_FROM_DEVICE);
		skb_put(req->skb, rgpd->data_buff_len);
		new_req->skb = req->skb;
		INIT_LIST_HEAD(&new_req->entry); // as port will run list_del
		ccci_h = (struct ccci_header *)new_req->skb->data;
		if(atomic_cmpxchg(&md->wakeup_src, 1, 0) == 1)
			CCCI_INF_MSG(md->index, TAG, "CLDMA_MD wakeup source:(%d/%d)\n", queue->index, ccci_h->channel);
		CCCI_DBG_MSG(md->index, TAG, "recv Rx msg (%x %x %x %x) rxq=%d len=%d\n", 
			ccci_h->data[0], ccci_h->data[1], *(((u32 *)ccci_h)+2), ccci_h->reserved, queue->index, rgpd->data_buff_len);
#if PACKET_HISTORY_DEPTH
		memcpy(&md_ctrl->rx_history[queue->index][md_ctrl->rx_history_ptr[queue->index]], ccci_h, sizeof(struct ccci_header));
		md_ctrl->rx_history_ptr[queue->index]++;
		md_ctrl->rx_history_ptr[queue->index] &= (PACKET_HISTORY_DEPTH-1);
#endif
		ret = ccci_port_recv_request(md, new_req);
		CCCI_DBG_MSG(md->index, TAG, "Rx port recv req ret=%d\n", ret);
		if(ret>=0 || ret==-CCCI_ERR_DROP_PACKET) {
fill_and_move:
			// allocate a new skb and change skb pointer
			req->skb = ccci_alloc_skb(rx_queue_buffer_size[queue->index], blocking);
#if 0 // UT code
			if(hole < 50) {
				hole++;
			} else {
				ccci_free_skb(req->skb, RECYCLE);
				req->skb = NULL;
				hole = 0;
			}
#endif
			if(likely(req->skb)) {
				rgpd->data_buff_bd_ptr = dma_map_single(NULL, req->skb->data, skb_size(req->skb), DMA_FROM_DEVICE);
				// checksum of GPD
				caculate_checksum((char *)rgpd, 0x81);
				// update GPD
				cldma_write8(&rgpd->gpd_flags, 0, 0x81);
				// step forward
				req = list_entry(req->entry.next, struct ccci_request, entry);
				rgpd = (struct cldma_rgpd *)req->gpd;
				queue->tr_done = req;
#if TRAFFIC_MONITOR_INTERVAL
				md_ctrl->rx_traffic_monitor[queue->index]++;
#endif
			} else {
				/*
				 * low memory, just stop and if lucky enough, some one collect it again (most likely
				 * NAPI), the fill_and_move should work.
				 */
				CCCI_ERR_MSG(md->index, TAG, "alloc skb fail on q%d\n", queue->index);
				*result = NO_SKB;
				break;
			}
		} else {
			// undo skb, as it remains in buffer and will be handled later
			new_req->skb->len = 0;
			skb_reset_tail_pointer(new_req->skb);
			// free the wrapper
			list_del(&new_req->entry);
			new_req->policy = NOOP;
			ccci_free_req(new_req);
#if PACKET_HISTORY_DEPTH
			md_ctrl->rx_history_ptr[queue->index]--;
			md_ctrl->rx_history_ptr[queue->index] &= (PACKET_HISTORY_DEPTH-1);
#endif
			*result = PORT_REFUSE;
			break;
		}
		// check budget
		if(count++ >= budget) {
			*result = REACH_BUDGET;
			break;
		}
	}
	/*
	 * do not use if(count == RING_BUFFER_SIZE) to resume Rx queue.
	 * resume Rx queue every time. we may not handle all RX ring buffer at one time due to
	 * user can refuse to receive patckets. so when a queue is stopped after it consumes all
	 * GPD, there is a chance that "count" never reaches ring buffer size and the queue is stopped 
	 * permanentely.
	 *
	 * resume after all RGPD handled also makes budget useless when it is less than ring buffer length.
	 */
	// if result == 0, that means all skb have been handled
	CCCI_DBG_MSG(md->index, TAG, "CLDMA Rxq%d collected, result=%d, count=%d\n", queue->index, *result, count);
#ifdef CCCI_STATISTIC
	// during NAPI polling, only the last function call with count==0 can be saved, so we only update if count!=0
	md_ctrl->stat_rx_used[queue->index] = count==0?md_ctrl->stat_rx_used[queue->index]:count;
	trace_md_rx_used(md_ctrl->stat_rx_used);
#endif
	return count;
}

static void cldma_rx_done(struct work_struct *work)
{
	struct md_cd_queue *queue = container_of(work, struct md_cd_queue, cldma_work);
	struct ccci_modem *md = queue->modem;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	int result;
	unsigned long flags;
	
	cldma_rx_collect(queue, queue->budget, 1, &result);
	md_cd_lock_cldma_clock_src(1);
	spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
	if(md_ctrl->rxq_active & (1<<queue->index)) {
		// resume Rx queue
		cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_SO_RESUME_CMD, CLDMA_BM_ALL_QUEUE&(1<<queue->index));
		cldma_read32(md_ctrl->cldma_ap_base, CLDMA_AP_SO_RESUME_CMD); // dummy read
		// enable RX_DONE interrupt
		cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L2RIMCR0, CLDMA_BM_ALL_QUEUE&(1<<queue->index));
	}
	spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
	md_cd_lock_cldma_clock_src(0);
}

static void cldma_tx_done(struct work_struct *work)
{
	struct md_cd_queue *queue = container_of(work, struct md_cd_queue, cldma_work);
	struct ccci_modem *md = queue->modem;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	unsigned long flags;
	
	struct ccci_request *req;
	struct cldma_tgpd *tgpd;
	struct ccci_header *ccci_h;
	int count = 0;
	struct sk_buff *skb_free;
	DATA_POLICY skb_free_p;
	
	while(1) {
		spin_lock_irqsave(&queue->ring_lock, flags);
		req = queue->tr_done;
		tgpd = (struct cldma_tgpd *)req->gpd;
		if(!((tgpd->gpd_flags&0x1) == 0 && req->skb)) {
			spin_unlock_irqrestore(&queue->ring_lock, flags);
			break;
		}
		// update counter
		queue->free_slot++;
		dma_unmap_single(NULL, tgpd->data_buff_bd_ptr, req->skb->len, DMA_TO_DEVICE);
		ccci_h = (struct ccci_header *)req->skb->data;
		CCCI_DBG_MSG(md->index, TAG, "harvest Tx msg (%x %x %x %x) txq=%d len=%d\n", 
			ccci_h->data[0], ccci_h->data[1], *(((u32 *)ccci_h)+2), ccci_h->reserved, queue->index, tgpd->data_buff_len);
		// free skb
		skb_free = req->skb;
		skb_free_p = req->policy;
		req->skb = NULL;
		count++;
		// step forward
		req = list_entry(req->entry.next, struct ccci_request, entry);
		tgpd = (struct cldma_tgpd *)req->gpd;
		queue->tr_done = req;
		if(likely(md->capability & MODEM_CAP_TXBUSY_STOP)) 
			cldma_queue_broadcast_state(md, TX_IRQ, OUT, queue->index);
		spin_unlock_irqrestore(&queue->ring_lock, flags);
		/* 
		 * After enabled NAPI, when free skb, cosume_skb() will eventually called nf_nat_cleanup_conntrack(),
		 * which will call spin_unlock_bh() to let softirq to run. so there is a chance a Rx softirq is triggered (cldma_rx_collect)	  
		 * and if it's a TCP packet, it will send ACK -- another Tx is scheduled which will require queue->ring_lock,
		 * cause a deadlock!
		 *
		 * This should not be an issue any more, after we start using dev_kfree_skb_any() instead of dev_kfree_skb().
		 */
		ccci_free_skb(skb_free, skb_free_p);
#if TRAFFIC_MONITOR_INTERVAL
		md_ctrl->tx_traffic_monitor[queue->index]++;
#endif
	}
	if(count)
		wake_up_nr(&queue->req_wq, count);
	// enable TX_DONE interrupt
	md_cd_lock_cldma_clock_src(1);
	spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
	if(md_ctrl->txq_active & (1<<queue->index))
		cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L2TIMCR0, CLDMA_BM_ALL_QUEUE&(1<<queue->index));
	spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
	md_cd_lock_cldma_clock_src(0);
}

static void cldma_rx_queue_init(struct md_cd_queue *queue)
{
	int i;
	struct ccci_request *req;
	struct cldma_rgpd *gpd=NULL, *prev_gpd=NULL;
	struct ccci_modem *md = queue->modem;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	
	for(i=0; i<rx_queue_buffer_number[queue->index]; i++) {
		req = ccci_alloc_req(IN, rx_queue_buffer_size[queue->index], 1, 0);
		req->gpd = dma_pool_alloc(md_ctrl->rgpd_dmapool, GFP_KERNEL, &req->gpd_addr);
		gpd = (struct cldma_rgpd *)req->gpd;
		memset(gpd, 0, sizeof(struct cldma_rgpd));
		gpd->data_buff_bd_ptr = dma_map_single(NULL, req->skb->data, skb_size(req->skb), DMA_FROM_DEVICE);
		gpd->data_allow_len = rx_queue_buffer_size[queue->index];
		gpd->gpd_flags = 0x81; // IOC|HWO
		if(i==0) {
			queue->tr_done = req;
			queue->tr_ring = &req->entry;
			INIT_LIST_HEAD(queue->tr_ring); // check ccci_request_struct_init for why we init here
		} else {
			prev_gpd->next_gpd_ptr = req->gpd_addr;
			caculate_checksum((char *)prev_gpd, 0x81);
			list_add_tail(&req->entry, queue->tr_ring);
		}
		prev_gpd = gpd;
	}
	gpd->next_gpd_ptr = queue->tr_done->gpd_addr;
	caculate_checksum((char *)gpd, 0x81);

	/*
	 * we hope work item of different CLDMA queue can work concurrently, but work items of the same
	 * CLDMA queue must be work sequentially as wo didn't implement any lock in rx_done or tx_done.
	 */
	queue->worker = alloc_workqueue("rx%d_worker", WQ_UNBOUND | WQ_MEM_RECLAIM | WQ_HIGHPRI, 1, queue->index);
	INIT_WORK(&queue->cldma_work, cldma_rx_done);
	CCCI_DBG_MSG(md->index, TAG, "rxq%d work=%p\n", queue->index, &queue->cldma_work);
}

static void cldma_tx_queue_init(struct md_cd_queue *queue)
{
	int i;
	struct ccci_request *req;
	struct cldma_tgpd *gpd=NULL, *prev_gpd=NULL;
	struct ccci_modem *md = queue->modem;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	
	for(i=0; i<tx_queue_buffer_number[queue->index]; i++) {
		req = ccci_alloc_req(OUT, -1, 1, 0);
		req->gpd = dma_pool_alloc(md_ctrl->tgpd_dmapool, GFP_KERNEL, &req->gpd_addr);
		gpd = (struct cldma_tgpd *)req->gpd;
		memset(gpd, 0, sizeof(struct cldma_tgpd));
		// network needs we free skb as soon as possible as they are tracking skb completion
		gpd->gpd_flags = 0x80; // IOC
		if(i==0) {
			queue->tr_done = req;
			queue->tx_xmit = req;
			queue->tr_ring = &req->entry;
			INIT_LIST_HEAD(queue->tr_ring);
		} else {
			prev_gpd->next_gpd_ptr = req->gpd_addr;
			list_add_tail(&req->entry, queue->tr_ring);
		}
		prev_gpd = gpd;
	}
	gpd->next_gpd_ptr = queue->tr_done->gpd_addr;

	queue->worker = alloc_workqueue("tx%d_worker", WQ_UNBOUND | WQ_MEM_RECLAIM | WQ_HIGHPRI, 1, queue->index);
	INIT_WORK(&queue->cldma_work, cldma_tx_done);
	CCCI_DBG_MSG(md->index, TAG, "txq%d work=%p\n", queue->index, &queue->cldma_work);
}

static void cldma_timeout_timer_func(unsigned long data)
{
	struct ccci_modem *md = (struct ccci_modem *)data;
	struct ccci_port *port;
	unsigned long long port_full=0, i; // hardcode, port number should not be larger than 64
	
	for(i=0; i<md->port_number; i++) {
		port = md->ports + i;
		if(port->flags & PORT_F_RX_FULLED)
			port_full |= (1<<i);
	}
	CCCI_ERR_MSG(md->index, TAG, "CLDMA no response for %d seconds, ports=%llx\n", CLDMA_ACTIVE_T, port_full);
	md->ops->dump_info(md, DUMP_FLAG_CLDMA|DUMP_FLAG_REG, NULL, 0);
	ccci_md_exception_notify(md, MD_NO_RESPONSE);
}

static irqreturn_t cldma_isr(int irq, void *data)
{
	struct ccci_modem *md = (struct ccci_modem *)data;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;

	CCCI_DBG_MSG(md->index, TAG, "CLDMA IRQ!\n");
	disable_irq_nosync(CLDMA_AP_IRQ);
	tasklet_hi_schedule(&md_ctrl->cldma_irq_task);
	return IRQ_HANDLED;
}

void cldma_irq_task(unsigned long data)
{
	struct ccci_modem *md = (struct ccci_modem *)data;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	int i, ret;
	unsigned int L2TIMR0, L2RIMR0, L2TISAR0, L2RISAR0;
	unsigned int L3TIMR0, L3RIMR0, L3TISAR0, L3RISAR0;

	md_cd_lock_cldma_clock_src(1);
	// get L2 interrupt status
	L2TISAR0 = cldma_read32(md_ctrl->cldma_ap_base, CLDMA_AP_L2TISAR0);
	L2RISAR0 = cldma_read32(md_ctrl->cldma_ap_base, CLDMA_AP_L2RISAR0);
	L2TIMR0 = cldma_read32(md_ctrl->cldma_ap_base, CLDMA_AP_L2TIMR0);
	L2RIMR0 = cldma_read32(md_ctrl->cldma_ap_base, CLDMA_AP_L2RIMR0);
	// get L3 interrupt status
	L3TISAR0 = cldma_read32(md_ctrl->cldma_ap_base, CLDMA_AP_L3TISAR0);
	L3RISAR0 = cldma_read32(md_ctrl->cldma_ap_base, CLDMA_AP_L3RISAR0);
	L3TIMR0 = cldma_read32(md_ctrl->cldma_ap_base, CLDMA_AP_L3TIMR0);
	L3RIMR0 = cldma_read32(md_ctrl->cldma_ap_base, CLDMA_AP_L3RIMR0);
	
	if(atomic_read(&md->wakeup_src) == 1)
		CCCI_INF_MSG(md->index, TAG, "wake up by CLDMA_MD L2(%x/%x) L3(%x/%x)!\n", L2TISAR0, L2RISAR0, L3TISAR0, L3RISAR0);
	else
		CCCI_DBG_MSG(md->index, TAG, "CLDMA IRQ L2(%x/%x) L3(%x/%x)!\n", L2TISAR0, L2RISAR0, L3TISAR0, L3RISAR0);

	L2TISAR0 &= (~L2TIMR0);
	L2RISAR0 &= (~L2RIMR0);

	L3TISAR0 &= (~L3TIMR0);
	L3RISAR0 &= (~L3RIMR0);

	if(L2TISAR0 & CLDMA_BM_INT_ERROR) {
		// TODO:
	}
	if(L2RISAR0 & CLDMA_BM_INT_ERROR) {
		// TODO:
	}
	if(unlikely(!(L2RISAR0&CLDMA_BM_INT_DONE) && !(L2TISAR0&CLDMA_BM_INT_DONE))) {
		CCCI_ERR_MSG(md->index, TAG, "no Tx or Rx, L2TISAR0=%X, L3TISAR0=%X, L2RISAR0=%X, L3RISAR0=%X\n", 
			cldma_read32(md_ctrl->cldma_ap_base, CLDMA_AP_L2TISAR0),
			cldma_read32(md_ctrl->cldma_ap_base, CLDMA_AP_L3TISAR0),
			cldma_read32(md_ctrl->cldma_ap_base, CLDMA_AP_L2RISAR0),
			cldma_read32(md_ctrl->cldma_ap_base, CLDMA_AP_L3RISAR0));
	} else {
#ifdef ENABLE_CLDMA_TIMER
		del_timer(&md_ctrl->cldma_timeout_timer);
#endif
	}
	// ack Tx interrupt
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L2TISAR0, L2TISAR0);
	for(i=0; i<QUEUE_LEN(md_ctrl->txq); i++) {
		if(L2TISAR0 & CLDMA_BM_INT_DONE & (1<<i)) {
			// disable TX_DONE interrupt
			cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L2TIMSR0, CLDMA_BM_ALL_QUEUE&(1<<i));
			ret = queue_work(md_ctrl->txq[i].worker, &md_ctrl->txq[i].cldma_work);
		}
	}
	// ack Rx interrupt
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L2RISAR0, L2RISAR0);
	// clear MD2AP_PEER_WAKEUP when get RX_DONE
#ifdef MD_PEER_WAKEUP
	if(L2RISAR0 & CLDMA_BM_INT_DONE)
		cldma_write32(md_ctrl->md_peer_wakeup, 0, cldma_read32(md_ctrl->md_peer_wakeup, 0) & ~0x01);
#endif
	for(i=0; i<QUEUE_LEN(md_ctrl->rxq); i++) {
		if(L2RISAR0 & CLDMA_BM_INT_DONE & (1<<i)) {
			// disable RX_DONE interrupt
			cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L2RIMSR0, CLDMA_BM_ALL_QUEUE&(1<<i));
			if(md->md_state!=EXCEPTION && ((1<<i)&NAPI_QUEUE_MASK)) {
				md_ctrl->rxq[i].napi_port->ops->md_state_notice(md_ctrl->rxq[i].napi_port, RX_IRQ);
			} else {
				ret = queue_work(md_ctrl->rxq[i].worker, &md_ctrl->rxq[i].cldma_work);
			}
		}
	}
	md_cd_lock_cldma_clock_src(0);
	enable_irq(CLDMA_AP_IRQ);
}

static inline void cldma_stop(struct ccci_modem *md)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	int ret, count;
	unsigned long flags;
	
	CCCI_INF_MSG(md->index, TAG, "%s from %ps\n", __FUNCTION__, __builtin_return_address(0));
	spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
	// stop all Tx and Rx queues
	count = 0;
	md_ctrl->txq_active &= (~CLDMA_BM_ALL_QUEUE);
	do {
		cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_UL_STOP_CMD, CLDMA_BM_ALL_QUEUE);
		cldma_read32(md_ctrl->cldma_ap_base, CLDMA_AP_UL_STOP_CMD); // dummy read
		ret = cldma_read32(md_ctrl->cldma_ap_base, CLDMA_AP_UL_STATUS);
		if((++count)%100000 == 0) {
			CCCI_INF_MSG(md->index, TAG, "stop Tx CLDMA, status=%x, count=%d\n", ret, count);	
			md->ops->dump_info(md, DUMP_FLAG_CLDMA, NULL, 0);
			BUG_ON(1);
		}
	} while(ret != 0);
	count = 0;
	md_ctrl->rxq_active &= (~CLDMA_BM_ALL_QUEUE);
	do {
		cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_SO_STOP_CMD, CLDMA_BM_ALL_QUEUE);
		cldma_read32(md_ctrl->cldma_ap_base, CLDMA_AP_SO_STOP_CMD); // dummy read
		ret = cldma_read32(md_ctrl->cldma_ap_base, CLDMA_AP_SO_STATUS);
		if((++count)%100000 == 0) {
			CCCI_INF_MSG(md->index, TAG, "stop Rx CLDMA, status=%x, count=%d\n", ret, count);
			md->ops->dump_info(md, DUMP_FLAG_CLDMA, NULL, 0);
			BUG_ON(1);
		}
	} while(ret != 0);
	// clear all L2 and L3 interrupts
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L2TISAR0, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L2TISAR1, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L2RISAR0, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L2RISAR1, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L3TISAR0, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L3TISAR1, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L3RISAR0, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L3RISAR1, CLDMA_BM_INT_ALL);
	// disable all L2 and L3 interrupts
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L2TIMSR0, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L2TIMSR1, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L2RIMSR0, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L2RIMSR1, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L3TIMSR0, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L3TIMSR1, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L3RIMSR0, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L3RIMSR1, CLDMA_BM_INT_ALL);
	// stop timer
#ifdef ENABLE_CLDMA_TIMER
	del_timer(&md_ctrl->cldma_timeout_timer);
#endif
	spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
}

static inline void cldma_stop_for_ee(struct ccci_modem *md)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	int ret,count;
	unsigned long flags;
	
	CCCI_INF_MSG(md->index, TAG, "%s from %ps\n", __FUNCTION__, __builtin_return_address(0));
	spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
	// stop all Tx and Rx queues, but non-stop Rx ones
	count = 0;
	md_ctrl->txq_active &= (~CLDMA_BM_ALL_QUEUE);
	do {
		cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_UL_STOP_CMD, CLDMA_BM_ALL_QUEUE);
		cldma_read32(md_ctrl->cldma_ap_base, CLDMA_AP_UL_STOP_CMD); // dummy read
		ret = cldma_read32(md_ctrl->cldma_ap_base, CLDMA_AP_UL_STATUS);
		if((++count)%100000 == 0) {
			CCCI_INF_MSG(md->index, TAG, "stop Tx CLDMA E, status=%x, count=%d\n", ret, count);
			md->ops->dump_info(md, DUMP_FLAG_CLDMA|DUMP_FLAG_REG|DUMP_FLAG_SMEM, NULL, 0);
			BUG_ON(1);
		}
	} while(ret != 0);
	count = 0;
	md_ctrl->rxq_active &= (~(CLDMA_BM_ALL_QUEUE&NONSTOP_QUEUE_MASK));
	do {
		cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_SO_STOP_CMD, CLDMA_BM_ALL_QUEUE&NONSTOP_QUEUE_MASK);
		cldma_read32(md_ctrl->cldma_ap_base, CLDMA_AP_SO_STOP_CMD); // dummy read
		ret = cldma_read32(md_ctrl->cldma_ap_base, CLDMA_AP_SO_STATUS) & NONSTOP_QUEUE_MASK;
		if((++count)%100000 == 0) {
			CCCI_INF_MSG(md->index, TAG, "stop Rx CLDMA E, status=%x, count=%d\n", ret, count);
			md->ops->dump_info(md, DUMP_FLAG_CLDMA|DUMP_FLAG_REG|DUMP_FLAG_SMEM, NULL, 0);
			BUG_ON(1);
		}
	} while(ret != 0);
	// clear all L2 and L3 interrupts, but non-stop Rx ones
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L2TISAR0, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L2TISAR1, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L2RISAR0, CLDMA_BM_INT_ALL&NONSTOP_QUEUE_MASK_32);
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L2RISAR1, CLDMA_BM_INT_ALL&NONSTOP_QUEUE_MASK_32);
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L3TISAR0, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L3TISAR1, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L3RISAR0, CLDMA_BM_INT_ALL&NONSTOP_QUEUE_MASK_32);
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L3RISAR1, CLDMA_BM_INT_ALL&NONSTOP_QUEUE_MASK_32);
	// disable all L2 and L3 interrupts, but non-stop Rx ones
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L2TIMSR0, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L2TIMSR1, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L2RIMSR0, (CLDMA_BM_INT_DONE|CLDMA_BM_INT_ERROR)&NONSTOP_QUEUE_MASK_32);
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L2RIMSR1, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L3TIMSR0, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L3TIMSR1, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L3RIMSR0, CLDMA_BM_INT_ALL&NONSTOP_QUEUE_MASK_32);
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L3RIMSR1, CLDMA_BM_INT_ALL&NONSTOP_QUEUE_MASK_32);
	spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
}

static inline void cldma_reset(struct ccci_modem *md)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	
	CCCI_INF_MSG(md->index, TAG, "%s from %ps\n", __FUNCTION__, __builtin_return_address(0));
	cldma_stop(md);
	// enable OUT DMA
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_SO_CFG, cldma_read32(md_ctrl->cldma_ap_base, CLDMA_AP_SO_CFG)|0x01);
	// wait RGPD write transaction repsonse
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_SO_CFG, cldma_read32(md_ctrl->cldma_ap_base, CLDMA_AP_SO_CFG) | 0x4);
	// enable SPLIT_EN
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_BUS_CFG, cldma_read32(md_ctrl->cldma_ap_base, CLDMA_AP_BUS_CFG)|0x02);
	// set high priority queue
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_HPQR, high_priority_queue_mask);
	// TODO: traffic control value
	// set checksum
	switch (CHECKSUM_SIZE) {
	case 0:
		cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_UL_CHECKSUM_CHANNEL_ENABLE, 0);
		cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_SO_CHECKSUM_CHANNEL_ENABLE, 0);
		break;
	case 12:
		cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_UL_CHECKSUM_CHANNEL_ENABLE, CLDMA_BM_ALL_QUEUE);
		cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_SO_CHECKSUM_CHANNEL_ENABLE, CLDMA_BM_ALL_QUEUE);		
		cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_UL_CFG, cldma_read32(md_ctrl->cldma_ap_base, CLDMA_AP_UL_CFG)&~0x10);
		cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_SO_CFG, cldma_read32(md_ctrl->cldma_ap_base, CLDMA_AP_SO_CFG)&~0x10);
		break;
	case 16:
		cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_UL_CHECKSUM_CHANNEL_ENABLE, CLDMA_BM_ALL_QUEUE);
		cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_SO_CHECKSUM_CHANNEL_ENABLE, CLDMA_BM_ALL_QUEUE);	
		cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_UL_CFG, cldma_read32(md_ctrl->cldma_ap_base, CLDMA_AP_UL_CFG)|0x10);
		cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_SO_CFG, cldma_read32(md_ctrl->cldma_ap_base, CLDMA_AP_SO_CFG)|0x10);
		break;
	}
	// TODO: need to select CLDMA mode in CFG?
	// TODO: enable debug ID?
}

static inline void cldma_start(struct ccci_modem *md)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	int i;
	unsigned long flags;

	CCCI_INF_MSG(md->index, TAG, "%s from %ps\n", __FUNCTION__, __builtin_return_address(0));
	spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
	// set start address
	for(i=0; i<QUEUE_LEN(md_ctrl->txq); i++) {
		cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_TQSAR(md_ctrl->txq[i].index), md_ctrl->txq[i].tr_done->gpd_addr);
	}
	for(i=0; i<QUEUE_LEN(md_ctrl->rxq); i++) {
		cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_RQSAR(md_ctrl->rxq[i].index), md_ctrl->rxq[i].tr_done->gpd_addr);
	}
	wmb();
	// start all Tx and Rx queues
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_UL_START_CMD, CLDMA_BM_ALL_QUEUE);
	cldma_read32(md_ctrl->cldma_ap_base, CLDMA_AP_UL_START_CMD); // dummy read
	md_ctrl->txq_active |= CLDMA_BM_ALL_QUEUE;
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_SO_START_CMD, CLDMA_BM_ALL_QUEUE);
	cldma_read32(md_ctrl->cldma_ap_base, CLDMA_AP_SO_START_CMD); // dummy read
	md_ctrl->rxq_active |= CLDMA_BM_ALL_QUEUE;
	// enable L2 DONE and ERROR interrupts
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L2TIMCR0, CLDMA_BM_INT_DONE|CLDMA_BM_INT_ERROR);
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L2RIMCR0, CLDMA_BM_INT_DONE|CLDMA_BM_INT_ERROR);
	// enable all L3 interrupts
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L3TIMCR0, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L3TIMCR1, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L3RIMCR0, CLDMA_BM_INT_ALL);
	cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L3RIMCR1, CLDMA_BM_INT_ALL);
	spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
}

// only allowed when cldma is stopped
static void md_cd_clear_queue(struct ccci_modem *md, DIRECTION dir)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	int i;
	struct ccci_request *req = NULL;
	struct cldma_tgpd *tgpd;
	unsigned long flags;

	if(dir == OUT) {	
		for(i=0; i<QUEUE_LEN(md_ctrl->txq); i++) {
			spin_lock_irqsave(&md_ctrl->txq[i].ring_lock, flags);
			req = list_entry(md_ctrl->txq[i].tr_ring, struct ccci_request, entry);
			md_ctrl->txq[i].tr_done = req;
			md_ctrl->txq[i].tx_xmit = req;
			md_ctrl->txq[i].free_slot = tx_queue_buffer_number[i];
			md_ctrl->txq[i].debug_id = 0;
#if PACKET_HISTORY_DEPTH
			md_ctrl->tx_history_ptr[i] = 0;
#endif
			do {
				tgpd = (struct cldma_tgpd *)req->gpd;
				cldma_write8(&tgpd->gpd_flags, 0, cldma_read8(&tgpd->gpd_flags, 0) & ~0x1);
				cldma_write32(&tgpd->data_buff_bd_ptr, 0, 0);
				cldma_write16(&tgpd->data_buff_len, 0, 0);
				if(req->skb) {
					ccci_free_skb(req->skb, req->policy);
					req->skb = NULL;
				}

				req = list_entry(req->entry.next, struct ccci_request, entry);
			} while(&req->entry != md_ctrl->txq[i].tr_ring);
			spin_unlock_irqrestore(&md_ctrl->txq[i].ring_lock, flags);
		}
	} else if(dir == IN) {
		struct cldma_rgpd *rgpd;
		for(i=0; i<QUEUE_LEN(md_ctrl->rxq); i++) {
			req = list_entry(md_ctrl->rxq[i].tr_ring, struct ccci_request, entry);
			md_ctrl->rxq[i].tr_done = req;
#if PACKET_HISTORY_DEPTH
			md_ctrl->rx_history_ptr[i] = 0;
#endif
			do {
				rgpd = (struct cldma_rgpd *)req->gpd;
				cldma_write8(&rgpd->gpd_flags, 0, 0x81);
				cldma_write16(&rgpd->data_buff_len, 0, 0);
				req->skb->len = 0;
				skb_reset_tail_pointer(req->skb);

				req = list_entry(req->entry.next, struct ccci_request, entry);
			} while(&req->entry != md_ctrl->rxq[i].tr_ring);
		}
	}
}

static void md_cd_wdt_work(struct work_struct *work)
{
	struct md_cd_ctrl *md_ctrl = container_of(work, struct md_cd_ctrl, wdt_work);
	struct ccci_modem *md = md_ctrl->txq[0].modem;
	int ret = 0;

	// 1. dump RGU reg
	CCCI_INF_MSG(md->index, TAG, "Dump MD RGU registers\n");
	ccci_mem_dump(md_ctrl->md_rgu_base, 0x30);
	// 2. wakelock
	wake_lock_timeout(&md_ctrl->trm_wake_lock, 10*HZ);

	if(*((int *)(md->mem_layout.smem_region_vir+CCCI_SMEM_OFFSET_EPON)) == 0xBAEBAE10) { //hardcode
		// 3. reset
		ret = md->ops->reset(md);
		CCCI_INF_MSG(md->index, TAG, "reset MD after WDT %d\n", ret);
		// 4. send message, only reset MD on non-eng load
		ccci_send_virtual_md_msg(md, CCCI_MONITOR_CH, CCCI_MD_MSG_RESET, 0);
	} else {
		md_cd_lock_cldma_clock_src(1);
		CCCI_INF_MSG(md->index, TAG, "Dump MD PC monitor %x\n", MD_PC_MONITOR_BASE);
		cldma_write32(md_ctrl->md_pc_monitor, 0, 0x80000000); // stop MD PCMon
		ccci_mem_dump(md_ctrl->md_pc_monitor, MD_PC_MONITOR_LENGTH);
		md_cd_lock_cldma_clock_src(0);
		ccci_md_exception_notify(md, MD_WDT);
	}
}

static irqreturn_t md_cd_wdt_isr(int irq, void *data)
{
	struct ccci_modem *md = (struct ccci_modem *)data;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	
	CCCI_INF_MSG(md->index, TAG, "MD WDT IRQ\n");
	// 1. disable MD WDT
	del_timer(&md_ctrl->bus_timeout_timer);
#ifdef ENABLE_MD_WDT_DBG
	unsigned int state;
	state = cldma_read32(md_ctrl->md_rgu_base, WDT_MD_STA);
	cldma_write32(md_ctrl->md_rgu_base, WDT_MD_MODE, WDT_MD_MODE_KEY);
	CCCI_INF_MSG(md->index, TAG, "WDT IRQ disabled for debug, state=%X\n", state);
#endif
	// 2. start a work queue to do the reset, because we used flush_work which is not designed for ISR
	schedule_work(&md_ctrl->wdt_work);
	return IRQ_HANDLED;
}

void md_cd_ap2md_bus_timeout_timer_func(unsigned long data)
{
	struct ccci_modem *md = (struct ccci_modem *)data;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	
	CCCI_INF_MSG(md->index, TAG, "MD bus timeout but no WDT IRQ\n");
	// same as WDT ISR
	schedule_work(&md_ctrl->wdt_work);
}

static irqreturn_t md_cd_ap2md_bus_timeout_isr(int irq, void *data)
{
	struct ccci_modem *md = (struct ccci_modem *)data;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;

	CCCI_INF_MSG(md->index, TAG, "MD bus timeout IRQ\n");
	mod_timer(&md_ctrl->bus_timeout_timer, jiffies+5*HZ);
	return IRQ_HANDLED;
}

static int md_cd_ccif_send(struct ccci_modem *md, int channel_id)
{
	int busy = 0;
	busy = cldma_read32(AP_CCIF0_BASE, APCCIF_BUSY);
	if(busy & (1<<channel_id)) {
		return -1;
	}
	cldma_write32(AP_CCIF0_BASE, APCCIF_BUSY, 1<<channel_id);
	cldma_write32(AP_CCIF0_BASE, APCCIF_TCHNUM, channel_id);
	return 0;
}

static void md_cd_exception(struct ccci_modem *md, HIF_EX_STAGE stage)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	
	CCCI_INF_MSG(md->index, TAG, "MD exception HIF %d\n", stage);
	// in exception mode, MD won't sleep, so we do not need to request MD resource first
	switch(stage) {
	case HIF_EX_INIT:
		wake_lock_timeout(&md_ctrl->trm_wake_lock, 10*HZ);
		ccci_md_exception_notify(md, EX_INIT);
		// disable CLDMA except un-stop queues
		cldma_stop_for_ee(md);
		// purge Tx queue	
		md_cd_clear_queue(md, OUT);
		// Rx dispatch does NOT depend on queue index in port structure, so it still can find right port.
		md_cd_ccif_send(md, H2D_EXCEPTION_ACK);
		break;
	case HIF_EX_INIT_DONE:
		ccci_md_exception_notify(md, EX_DHL_DL_RDY);
		break;
	case HIF_EX_CLEARQ_DONE:
		// give DHL some time to flush data
		schedule_delayed_work(&md_ctrl->ccif_delayed_work, 2*HZ);
		break;
	case HIF_EX_ALLQ_RESET:
		// re-start CLDMA
		cldma_reset(md);
		md_cd_clear_queue(md, IN); // purge Rx queue
		ccci_md_exception_notify(md, EX_INIT_DONE);
		cldma_start(md);
		break;
	default:
		break;
	};
}

static void md_cd_ccif_delayed_work(struct work_struct *work)
{
	struct md_cd_ctrl *md_ctrl = container_of(to_delayed_work(work), struct md_cd_ctrl, ccif_delayed_work);
	struct ccci_modem *md = md_ctrl->txq[0].modem;
	int i;

	// stop CLDMA, we don't want to get CLDMA IRQ when MD is reseting CLDMA after it got cleaq_ack
	cldma_stop(md);
	// flush work
	for(i=0; i<QUEUE_LEN(md_ctrl->txq); i++) {
		flush_work(&md_ctrl->txq[i].cldma_work);
	}
	for(i=0; i<QUEUE_LEN(md_ctrl->rxq); i++) {
		flush_work(&md_ctrl->rxq[i].cldma_work);
	}
	// tell MD to reset CLDMA
	md_cd_ccif_send(md, H2D_EXCEPTION_CLEARQ_ACK);
	CCCI_INF_MSG(md->index, TAG, "send clearq_ack to MD\n");
}

static void md_cd_ccif_work(struct work_struct *work)
{
	struct md_cd_ctrl *md_ctrl = container_of(work, struct md_cd_ctrl, ccif_work);
	struct ccci_modem *md = md_ctrl->txq[0].modem;

	// seems sometime MD send D2H_EXCEPTION_INIT_DONE and D2H_EXCEPTION_CLEARQ_DONE together
	if(md_ctrl->channel_id & (1<<D2H_EXCEPTION_INIT))
		md_cd_exception(md, HIF_EX_INIT);
	if(md_ctrl->channel_id & (1<<D2H_EXCEPTION_INIT_DONE))
		md_cd_exception(md, HIF_EX_INIT_DONE);
	if(md_ctrl->channel_id & (1<<D2H_EXCEPTION_CLEARQ_DONE))
		md_cd_exception(md, HIF_EX_CLEARQ_DONE);
	if(md_ctrl->channel_id & (1<<D2H_EXCEPTION_ALLQ_RESET))
		md_cd_exception(md, HIF_EX_ALLQ_RESET);
	if(md_ctrl->channel_id & (1<<AP_MD_PEER_WAKEUP))
		wake_lock_timeout(&md_ctrl->peer_wake_lock, HZ);
	if(md_ctrl->channel_id & (1<<AP_MD_SEQ_ERROR)) {
		CCCI_ERR_MSG(md->index, TAG, "MD check seq fail\n");
		md->ops->dump_info(md, DUMP_FLAG_CCIF, NULL, 0);
	}
}

static irqreturn_t md_cd_ccif_isr(int irq, void *data)
{
	struct ccci_modem *md = (struct ccci_modem *)data;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	
	// must ack first, otherwise IRQ will rush in
	md_ctrl->channel_id = cldma_read32(AP_CCIF0_BASE, APCCIF_RCHNUM);
	CCCI_DBG_MSG(md->index, TAG, "MD CCIF IRQ 0x%X\n", md_ctrl->channel_id);
	cldma_write32(AP_CCIF0_BASE, APCCIF_ACK, md_ctrl->channel_id);

#if 0 // workqueue is too slow
	schedule_work(&md_ctrl->ccif_work);
#else
	md_cd_ccif_work(&md_ctrl->ccif_work);
#endif
	return IRQ_HANDLED;
}

static inline int cldma_sw_init(struct ccci_modem *md)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	int ret;
	// do NOT touch CLDMA HW after power on MD
	// ioremap CLDMA register region
	md_ctrl->cldma_ap_base = ioremap_nocache(CLDMA_AP_BASE, CLDMA_AP_LENGTH);
	md_ctrl->cldma_md_base = ioremap_nocache(CLDMA_MD_BASE, CLDMA_MD_LENGTH);
	md_ctrl->md_boot_slave_Vector = ioremap_nocache(MD_BOOT_VECTOR, 0x4);
	md_ctrl->md_boot_slave_Key = ioremap_nocache(MD_BOOT_VECTOR_KEY, 0x4);
	md_ctrl->md_boot_slave_En = ioremap_nocache(MD_BOOT_VECTOR_EN, 0x4);
	md_ctrl->md_rgu_base = ioremap_nocache(MD_RGU_BASE, 0x40);
	md_ctrl->md_global_con0 = ioremap_nocache(MD_GLOBAL_CON0, 0x4);
	md_ctrl->md_bus_status = ioremap_nocache(MD_BUS_STATUS_BASE, MD_BUS_STATUS_LENGTH);
	md_ctrl->md_pc_monitor = ioremap_nocache(MD_PC_MONITOR_BASE, MD_PC_MONITOR_LENGTH);
	md_ctrl->md_topsm_status = ioremap_nocache(MD_TOPSM_STATUS_BASE, MD_TOPSM_STATUS_LENGTH);
	md_ctrl->md_ost_status = ioremap_nocache(MD_OST_STATUS_BASE, MD_OST_STATUS_LENGTH);
#ifdef MD_PEER_WAKEUP
	md_ctrl->md_peer_wakeup = ioremap_nocache(MD_PEER_WAKEUP, 0x4);
#endif
	// request IRQ
	ret = request_irq(CLDMA_AP_IRQ, cldma_isr, IRQF_TRIGGER_HIGH, "CLDMA_AP", md);
	if(ret) {
		CCCI_ERR_MSG(md->index, TAG, "request CLDMA_AP IRQ(%d) error %d\n", CLDMA_AP_IRQ, ret);
		return ret;
	}
	ret = request_irq(MD_WDT_IRQ, md_cd_wdt_isr, IRQF_TRIGGER_FALLING, "MD_WDT", md);
	if(ret) {
		CCCI_ERR_MSG(md->index, TAG, "request MD_WDT IRQ(%d) error %d\n", MD_WDT_IRQ, ret);
		return ret;
	}
	atomic_inc(&md_ctrl->wdt_enabled); // IRQ is enabled after requested, so call enable_irq after request_irq will get a unbalance warning
	ret = request_irq(AP2MD_BUS_TIMEOUT_IRQ, md_cd_ap2md_bus_timeout_isr, IRQF_TRIGGER_FALLING, "AP2MD_BUS_TIMEOUT", md);
	if(ret) {
		CCCI_ERR_MSG(md->index, TAG, "request AP2MD_BUS_TIMEOUT IRQ(%d) error %d\n", AP2MD_BUS_TIMEOUT_IRQ, ret);
		return ret;
	}
	ret = request_irq(CCIF0_AP_IRQ, md_cd_ccif_isr, IRQF_TRIGGER_LOW, "CCIF0_AP", md);
	if(ret) {
		CCCI_ERR_MSG(md->index, TAG, "request CCIF0_AP IRQ(%d) error %d\n", CCIF0_AP_IRQ, ret);
		return ret;
	}
	return 0;
}

static int md_cd_broadcast_state(struct ccci_modem *md, MD_STATE state)
{
	int i;
	struct ccci_port *port;

	// only for thoes states which are updated by port_kernel.c
	switch(state) {
	case READY:
		md_cd_bootup_cleanup(md, 1);
		break;
	case BOOT_FAIL:
		if(md->md_state != BOOT_FAIL) // bootup timeout may comes before MD EE
			md_cd_bootup_cleanup(md, 0);
		return 0;
	case RX_IRQ:
		CCCI_ERR_MSG(md->index, TAG, "%ps broadcast RX_IRQ to ports!\n", __builtin_return_address(0));
		return 0;
	default:
		break;
	};

	if(md->md_state == state) // must have, due to we broadcast EXCEPTION both in MD_EX and EX_INIT
		return 1;
	
	md->md_state = state;
	for(i=0;i<md->port_number;i++) {
		port = md->ports + i;
		if(port->ops->md_state_notice)
			port->ops->md_state_notice(port, state);
	}
	return 0;
}

static int md_cd_init(struct ccci_modem *md)
{
	int i;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	struct ccci_port *port = NULL;

	CCCI_INF_MSG(md->index, TAG, "CLDMA modem is initializing\n");
	// init CLMDA, must before queue init as we set start address there
	cldma_sw_init(md);
	// init queue
	for(i=0; i<QUEUE_LEN(md_ctrl->txq); i++) {
		md_cd_queue_struct_init(&md_ctrl->txq[i], md, OUT, i, tx_queue_buffer_number[i]);
		md_ctrl->txq[i].free_slot = tx_queue_buffer_number[i];
		cldma_tx_queue_init(&md_ctrl->txq[i]);
#if PACKET_HISTORY_DEPTH
		md_ctrl->tx_history_ptr[i] = 0;
#endif
	}
	for(i=0; i<QUEUE_LEN(md_ctrl->rxq); i++) {
		md_cd_queue_struct_init(&md_ctrl->rxq[i], md, IN, i, rx_queue_buffer_number[i]);
		cldma_rx_queue_init(&md_ctrl->rxq[i]);
#if PACKET_HISTORY_DEPTH
		md_ctrl->rx_history_ptr[i] = 0;
#endif
	}
	// init port
	for(i=0; i<md->port_number; i++) {
		port = md->ports + i;
		ccci_port_struct_init(port, md);
		port->ops->init(port);
		if((port->flags&PORT_F_RX_EXCLUSIVE) && ((1<<port->rxq_index)&NAPI_QUEUE_MASK)) {
			md_ctrl->rxq[port->rxq_index].napi_port = port;
			CCCI_DBG_MSG(md->index, TAG, "queue%d add NAPI port %s\n", port->rxq_index, port->name);
		}
	}
	ccci_setup_channel_mapping(md);
	// update state
	md->md_state = GATED;
	return 0;
}

void wdt_enable_irq(struct md_cd_ctrl *md_ctrl)
{
	if(atomic_read(&md_ctrl->wdt_enabled) == 0) {
		enable_irq(MD_WDT_IRQ);
		atomic_inc(&md_ctrl->wdt_enabled);
	}
}

void wdt_disable_irq(struct md_cd_ctrl *md_ctrl)
{
	if(atomic_read(&md_ctrl->wdt_enabled) == 1) {
		/*may be called in isr, so use disable_irq_nosync.
		if use disable_irq in isr, system will hang*/
		disable_irq_nosync(MD_WDT_IRQ);
		atomic_dec(&md_ctrl->wdt_enabled);
	}
}
//used for throttling feature - start
extern unsigned long ccci_modem_boot_count[];
//used for throttling feature - end
static int md_cd_start(struct ccci_modem *md)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	char img_err_str[IMG_ERR_STR_LEN];
	int ret=0, retry, cldma_on=0;

	// 0. init security, as security depends on dummy_char, which is ready very late.
	ccci_init_security(md);
	
	CCCI_INF_MSG(md->index, TAG, "CLDMA modem is starting\n");
	// 1. load modem image
	if(md->config.setting&MD_SETTING_FIRST_BOOT || md->config.setting&MD_SETTING_RELOAD) {
		ret = ccci_load_firmware(md, IMG_MD, img_err_str);
		if(ret<0) {
			CCCI_ERR_MSG(md->index, TAG, "load firmware fail, %s\n", img_err_str);
			goto out;
		}
		ret = 0; // load_std_firmware returns MD image size
		md->config.setting &= ~MD_SETTING_RELOAD;
	}
	// 2. clear share memory and ring buffer
#if 0 // no need now, MD will clear share memory itself
	memset(md->mem_layout.smem_region_vir, 0, md->mem_layout.smem_region_size);
#endif
#if 1 // just in case
	md_cd_clear_queue(md, OUT);
	md_cd_clear_queue(md, IN);
#endif
	// 3. enable MPU
	ccci_set_mem_access_protection(md);
	// 4. power on modem, do NOT touch MD register before this
	if(md->config.setting & MD_SETTING_FIRST_BOOT) {
		ret = md_cd_power_off(md, 0);
		CCCI_INF_MSG(md->index, TAG, "power off MD first %d\n", ret);
		md->config.setting &= ~MD_SETTING_FIRST_BOOT;
	}
	ret = md_cd_power_on(md);
	if(ret) {
		CCCI_ERR_MSG(md->index, TAG, "power on MD fail %d\n", ret);
		goto out;
	}
	// 5. update mutex
	atomic_set(&md_ctrl->reset_on_going, 0);
	// 6. start timer
	mod_timer(&md->bootup_timer, jiffies+BOOT_TIMER_ON*HZ);
	// 7. let modem go
	md_cd_let_md_go(md);
	wdt_enable_irq(md_ctrl);
	// 8. start CLDMA
	retry = CLDMA_CG_POLL;
	while(retry-->0) {
		if(!(cldma_read32(md_ctrl->md_global_con0, 0) & (1<<MD_GLOBAL_CON0_CLDMA_BIT))) {
			CCCI_INF_MSG(md->index, TAG, "CLDMA clock is on, retry=%d\n", retry);
			cldma_on = 1;
			break;
		} else {
			CCCI_INF_MSG(md->index, TAG, "CLDMA clock is still off, retry=%d\n", retry);
			mdelay(1000);
		}
	}
	if(!cldma_on) {
		ret = -CCCI_ERR_HIF_NOT_POWER_ON;
		CCCI_ERR_MSG(md->index, TAG, "CLDMA clock is off, retry=%d\n", retry);
		goto out;
	}
	cldma_reset(md);
	md->ops->broadcast_state(md, BOOTING);
	cldma_start(md);
#ifdef CCCI_STATISTIC
	mod_timer(&md_ctrl->stat_timer, jiffies+HZ/2);
#endif
out:
	CCCI_INF_MSG(md->index, TAG, "CLDMA modem started %d\n", ret);
	//used for throttling feature - start
	ccci_modem_boot_count[md->index]++;
	//used for throttling feature - end
	return ret;
}

static int md_cd_stop(struct ccci_modem *md, unsigned int timeout)
{
	int ret=0, count=0;
	
	CCCI_INF_MSG(md->index, TAG, "CLDMA modem is power off, timeout=%d\n", timeout);
	// check EMI
	while((readl(IOMEM(EMI_IDLE_STATE_REG)) & EMI_MD_IDLE_MASK) != EMI_MD_IDLE_MASK) {
		CCCI_ERR_MSG(md->index, TAG, "wrong EMI idle state 0x%X, count=%d\n", readl(IOMEM(EMI_IDLE_STATE_REG)), count);
		msleep(1000);
		count++;
	}
	CCCI_INF_MSG(md->index, TAG, "before MD off, EMI idle state 0x%X\n", readl(IOMEM(EMI_IDLE_STATE_REG)));
	// power off MD
	ret = md_cd_power_off(md, timeout);
	CCCI_INF_MSG(md->index, TAG, "CLDMA modem is power off done, %d\n", ret);
	md->ops->broadcast_state(md, GATED);
	// ACK CCIF for MD. while entering flight mode, we may send something after MD slept
	cldma_write32(_MD_CCIF0_BASE, APCCIF_ACK, cldma_read32(_MD_CCIF0_BASE, APCCIF_RCHNUM));
	// check EMI
	CCCI_INF_MSG(md->index, TAG, "after MD off, EMI idle state 0x%X\n", readl(IOMEM(EMI_IDLE_STATE_REG)));
	if((readl(IOMEM(EMI_IDLE_STATE_REG)) & EMI_MD_IDLE_MASK) != EMI_MD_IDLE_MASK)
		CCCI_ERR_MSG(md->index, TAG, "wrong EMI idle state 0x%X\n", readl(IOMEM(EMI_IDLE_STATE_REG)));
	return 0;
}

// only run this in thread context, as we use flush_work in it
static int md_cd_reset(struct ccci_modem *md)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	int i;
	unsigned long flags;
	
	// 1. mutex check
	if(atomic_add_return(1, &md_ctrl->reset_on_going) > 1){
		CCCI_INF_MSG(md->index, TAG, "One reset flow is on-going\n");
		return -CCCI_ERR_MD_IN_RESET;
	}
	CCCI_INF_MSG(md->index, TAG, "CLDMA modem is resetting\n");
	// 2. disable WDT IRQ
	wdt_disable_irq(md_ctrl);
	// use spin_lock_irqsave here, because md_cd_send_request may be called in net sotfirq.
	md->ops->broadcast_state(md, RESET); // to block port's write operation
	md_cd_lock_cldma_clock_src(1);
	cldma_stop(md);
	md_cd_lock_cldma_clock_src(0);
	// 3. reset EE flag
	spin_lock_irqsave(&md->ctrl_lock, flags);
	md->ee_info_flag = 0; // must be after broadcast_state(RESET), check port_kernel.c
	spin_unlock_irqrestore(&md->ctrl_lock, flags);
	// 4. update state
	del_timer(&md->bootup_timer);
	// 5. flush CLDMA work and reset ring buffer
	for(i=0; i<QUEUE_LEN(md_ctrl->txq); i++) {
		flush_work(&md_ctrl->txq[i].cldma_work);
	}
	for(i=0; i<QUEUE_LEN(md_ctrl->rxq); i++) {
		flush_work(&md_ctrl->rxq[i].cldma_work);
	}
	md_cd_clear_queue(md, OUT);
	/*
	 * there is a race condition between md_power_off and CLDMA IRQ. after we get a CLDMA IRQ,
	 * if we power off MD before CLDMA tasklet is scheduled, the tasklet will get 0 when reading CLDMA
	 * register, and not schedule workqueue to check RGPD. this will leave an HWO=0 RGPD in ring
	 * buffer and cause a queue being stopped. so we flush RGPD here to kill this missing RX_DONE interrupt.
	 */
	md_cd_clear_queue(md, IN);
	md->boot_stage = MD_BOOT_STAGE_0;
#ifdef CCCI_STATISTIC
	del_timer(&md_ctrl->stat_timer);
#endif
	return 0;
}

static int md_cd_write_room(struct ccci_modem *md, unsigned char qno)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	if(qno >= QUEUE_LEN(md_ctrl->txq))
		return -CCCI_ERR_INVALID_QUEUE_INDEX;
	return md_ctrl->txq[qno].free_slot;
}

static int md_cd_send_request(struct ccci_modem *md, unsigned char qno, struct ccci_request* req)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	struct md_cd_queue *queue;
	struct ccci_request *tx_req;
	struct cldma_tgpd *tgpd;
	int ret;
	unsigned long flags;
	
#if TRAFFIC_MONITOR_INTERVAL
	if((jiffies-md_ctrl->traffic_stamp)/HZ >= TRAFFIC_MONITOR_INTERVAL) {
		md_ctrl->traffic_stamp = jiffies;
		mod_timer(&md_ctrl->traffic_monitor, jiffies);
	}
#endif
	
	if(qno >= QUEUE_LEN(md_ctrl->txq))
		return -CCCI_ERR_INVALID_QUEUE_INDEX;
	spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
	if(!(md_ctrl->txq_active & (1<<qno))) {
		spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
		return -CCCI_ERR_HIF_NOT_POWER_ON;
	}
	spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
	queue = &md_ctrl->txq[qno];
	
retry:
	md_cd_lock_cldma_clock_src(1); // put it outside of spin_lock_irqsave to avoid disabling IRQ too long
	spin_lock_irqsave(&queue->ring_lock, flags); // we use irqsave as network require a lock in softirq, cause a potential deadlock
	CCCI_DBG_MSG(md->index, TAG, "get a Tx req on q%d free=%d\n", qno, queue->free_slot);
	if(queue->free_slot > 0) {
		ccci_inc_tx_seq_num(md, req);
		wmb();
#if PACKET_HISTORY_DEPTH
		memcpy(&md_ctrl->tx_history[queue->index][md_ctrl->tx_history_ptr[queue->index]], req->skb->data, sizeof(struct ccci_header));
		md_ctrl->tx_history_ptr[queue->index]++;
		md_ctrl->tx_history_ptr[queue->index] &= (PACKET_HISTORY_DEPTH-1);
#endif
		queue->free_slot--;
		// step forward
		tx_req = queue->tx_xmit;
		tgpd = tx_req->gpd;
		queue->tx_xmit = list_entry(tx_req->entry.next, struct ccci_request, entry);
		// copy skb pointer
		tx_req->skb = req->skb;
		tx_req->policy = req->policy;
		// free old request as wrapper, do NOT reference this request after this, use tx_req instead
		req->policy = NOOP;
		ccci_free_req(req);
		// update GPD
		tgpd->data_buff_bd_ptr = dma_map_single(NULL, tx_req->skb->data, tx_req->skb->len, DMA_TO_DEVICE);
		tgpd->data_buff_len = tx_req->skb->len;
		tgpd->debug_id = queue->debug_id++;
		// checksum of GPD
		caculate_checksum((char *)tgpd, tgpd->gpd_flags | 0x1);
		// resume Tx queue
		cldma_write8(&tgpd->gpd_flags, 0, cldma_read8(&tgpd->gpd_flags, 0) | 0x1);
		/*
		 * resume queue inside spinlock, otherwise there is race conditon between ports over the same queue.
		 * one port is just setting TGPD, another port may have resumed the queue.
		 *
		 * start timer before resume CLDMA and inside spinlock_irqsave protection, avoid race condition with disabling timer in tasklet.
		 * use an extra spin lock to avoid race conditon with disabling timer in cldma_stop@md_cd_reset. 
		 */
		spin_lock(&md_ctrl->cldma_timeout_lock);
		if(md_ctrl->txq_active & (1<<qno)) {
#ifdef ENABLE_CLDMA_TIMER
			mod_timer(&md_ctrl->cldma_timeout_timer, jiffies+CLDMA_ACTIVE_T*HZ);	
#endif
			cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_UL_RESUME_CMD, CLDMA_BM_ALL_QUEUE&(1<<qno));
			cldma_read32(md_ctrl->cldma_ap_base, CLDMA_AP_UL_RESUME_CMD); // dummy read to create a non-buffable write
		}
		spin_unlock(&md_ctrl->cldma_timeout_lock);
		md_cd_ccif_send(md, AP_MD_PEER_WAKEUP);
		spin_unlock_irqrestore(&queue->ring_lock, flags); // TX_DONE will check this flag to recycle TGPD
		md_cd_lock_cldma_clock_src(0);
#ifdef CCCI_STATISTIC
		{
			int i;
			for(i=0; i<8; i++) { // hardcode
				md_ctrl->stat_tx_free[i] = md_ctrl->txq[i].free_slot;
			}
			trace_md_tx_free(md_ctrl->stat_tx_free);
		}
#endif
	} else {
		if(likely(md->capability & MODEM_CAP_TXBUSY_STOP)) 
			cldma_queue_broadcast_state(md, TX_FULL, OUT, queue->index);
		spin_unlock_irqrestore(&queue->ring_lock, flags);
		md_cd_lock_cldma_clock_src(0);
		if(req->blocking) {
			ret = wait_event_interruptible_exclusive(queue->req_wq, (queue->free_slot>0));
			if(ret == -ERESTARTSYS) {
				return -EINTR;
			}
			goto retry;
		} else {
			return -EBUSY;
		}
	}
	return 0;
}

static int md_cd_give_more(struct ccci_modem *md, unsigned char qno)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	int ret;
	
	if(qno >= QUEUE_LEN(md_ctrl->rxq))
		return -CCCI_ERR_INVALID_QUEUE_INDEX;
	CCCI_DBG_MSG(md->index, TAG, "give more on queue %d work %p\n", qno, &md_ctrl->rxq[qno].cldma_work);
	ret = queue_work(md_ctrl->rxq[qno].worker, &md_ctrl->rxq[qno].cldma_work);
	return 0;
}

static int md_cd_napi_poll(struct ccci_modem *md, unsigned char qno, struct napi_struct *napi ,int weight)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	int ret, result, all_clr=0;
	unsigned long flags;

	if(qno >= QUEUE_LEN(md_ctrl->rxq))
		return -CCCI_ERR_INVALID_QUEUE_INDEX;

	ret = cldma_rx_collect(&md_ctrl->rxq[qno], weight, 0, &result);
	if(likely(weight >= md_ctrl->rxq[qno].budget))
		all_clr = ret<md_ctrl->rxq[qno].budget?1:0;
	else
		all_clr = ret==0?1:0;
	if(likely(all_clr && result!=NO_REQ && result!=NO_SKB))
		all_clr = 1;
	else
		all_clr = 0;
	
	md_cd_lock_cldma_clock_src(1);
	// resume Rx queue
	spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
	if(md_ctrl->rxq_active & (1<<qno)) {
		cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_SO_RESUME_CMD, CLDMA_BM_ALL_QUEUE&(1<<md_ctrl->rxq[qno].index));
		cldma_read32(md_ctrl->cldma_ap_base, CLDMA_AP_SO_RESUME_CMD); // dummy read
		// enable RX_DONE interrupt
		if(all_clr)
			cldma_write32(md_ctrl->cldma_ap_base, CLDMA_AP_L2RIMCR0, CLDMA_BM_ALL_QUEUE&(1<<qno));
	}
	spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
	md_cd_lock_cldma_clock_src(0);
	if(all_clr) {
		napi_complete(napi);
	}
	return ret;
}

static struct ccci_port* md_cd_get_port_by_minor(struct ccci_modem *md, int minor)
{
	int i;
	struct ccci_port *port;
	
	for(i=0; i<md->port_number; i++) {
		port = md->ports + i;
		if(port->minor == minor)
			return port;
	}
	return NULL;
}

static struct ccci_port* md_cd_get_port_by_channel(struct ccci_modem *md, CCCI_CH ch)
{
	int i;
	struct ccci_port *port;
	
	for(i=0; i<md->port_number; i++) {
		port = md->ports + i;
		if(port->rx_ch == ch || port->tx_ch == ch)
			return port;
	}
	return NULL;
}

static void dump_runtime_data(struct ccci_modem *md, struct modem_runtime *runtime)
{
	char	ctmp[12];
	int		*p;

	p = (int*)ctmp;
	*p = runtime->Prefix;
	p++;
	*p = runtime->Platform_L;
	p++;
	*p = runtime->Platform_H;

	CCCI_INF_MSG(md->index, TAG, "**********************************************\n");
	CCCI_INF_MSG(md->index, TAG, "Prefix                      %c%c%c%c\n", ctmp[0], ctmp[1], ctmp[2], ctmp[3]);
	CCCI_INF_MSG(md->index, TAG, "Platform_L                  %c%c%c%c\n", ctmp[4], ctmp[5], ctmp[6], ctmp[7]);
	CCCI_INF_MSG(md->index, TAG, "Platform_H                  %c%c%c%c\n", ctmp[8], ctmp[9], ctmp[10], ctmp[11]);
	CCCI_INF_MSG(md->index, TAG, "DriverVersion               0x%x\n", runtime->DriverVersion);
	CCCI_INF_MSG(md->index, TAG, "BootChannel                 %d\n", runtime->BootChannel);
	CCCI_INF_MSG(md->index, TAG, "BootingStartID(Mode)        0x%x\n", runtime->BootingStartID);
	CCCI_INF_MSG(md->index, TAG, "BootAttributes              %d\n", runtime->BootAttributes);
	CCCI_INF_MSG(md->index, TAG, "BootReadyID                 %d\n", runtime->BootReadyID);
	
	CCCI_INF_MSG(md->index, TAG, "ExceShareMemBase            0x%x\n", runtime->ExceShareMemBase);
	CCCI_INF_MSG(md->index, TAG, "ExceShareMemSize            0x%x\n", runtime->ExceShareMemSize);
	CCCI_INF_MSG(md->index, TAG, "TotalShareMemBase           0x%x\n", runtime->TotalShareMemBase);
	CCCI_INF_MSG(md->index, TAG, "TotalShareMemSize           0x%x\n", runtime->TotalShareMemSize);

	CCCI_INF_MSG(md->index, TAG, "CheckSum                    %d\n", runtime->CheckSum);

	p = (int*)ctmp;
	*p = runtime->Postfix;
	CCCI_INF_MSG(md->index, TAG, "Postfix                     %c%c%c%c\n", ctmp[0], ctmp[1], ctmp[2], ctmp[3]);
	CCCI_INF_MSG(md->index, TAG, "**********************************************\n");
	
	p = (int*)ctmp;
	*p = runtime->misc_prefix;
	CCCI_INF_MSG(md->index, TAG, "Prefix                      %c%c%c%c\n", ctmp[0], ctmp[1], ctmp[2], ctmp[3]);
	CCCI_INF_MSG(md->index, TAG, "SupportMask                 0x%x\n", runtime->support_mask);
	CCCI_INF_MSG(md->index, TAG, "Index                       0x%x\n", runtime->index);
	CCCI_INF_MSG(md->index, TAG, "Next                        0x%x\n", runtime->next);
	CCCI_INF_MSG(md->index, TAG, "Feature2                    0x%x\n", runtime->feature_2_val[0]);
	CCCI_INF_MSG(md->index, TAG, "Feature4                    0x%x\n", runtime->feature_4_val[0]);
	
	p = (int*)ctmp;
	*p = runtime->misc_postfix;
	CCCI_INF_MSG(md->index, TAG, "Postfix                     %c%c%c%c\n", ctmp[0], ctmp[1], ctmp[2], ctmp[3]);

	CCCI_INF_MSG(md->index, TAG, "----------------------------------------------\n");
}

static int md_cd_send_runtime_data(struct ccci_modem *md, unsigned int sbp_code)
{
	int packet_size = sizeof(struct modem_runtime)+sizeof(struct ccci_header);
	struct ccci_request *req = NULL;
	struct ccci_header *ccci_h;
	struct modem_runtime *runtime;
    struct file *filp = NULL;
	LOGGING_MODE mdlog_flag = MODE_IDLE;
	int ret;
    char str[16];
	unsigned int random_seed = 0;
	snprintf(str, sizeof(str), "%s", CCCI_PLATFORM);

	req = ccci_alloc_req(OUT, packet_size, 1, 1);
	if(!req) {
		return -CCCI_ERR_ALLOCATE_MEMORY_FAIL;
	}
	ccci_h = (struct ccci_header *)req->skb->data;
	runtime = (struct modem_runtime *)(req->skb->data + sizeof(struct ccci_header));

	ccci_set_ap_region_protection(md);
	// header
	ccci_h->data[0]=0x00;
	ccci_h->data[1]= packet_size;
	ccci_h->reserved = MD_INIT_CHK_ID;
	ccci_h->channel = CCCI_CONTROL_TX;
	memset(runtime, 0, sizeof(struct modem_runtime));
	// runtime data, little endian for string
    runtime->Prefix = 0x46494343; // "CCIF"
    runtime->Postfix = 0x46494343; // "CCIF"
	runtime->Platform_L = *((int*)str);
    runtime->Platform_H = *((int*)&str[4]);
    runtime->BootChannel = CCCI_CONTROL_RX;
    runtime->DriverVersion = CCCI_DRIVER_VER;
    filp = filp_open(MDLOGGER_FILE_PATH, O_RDONLY, 0777);
    if (!IS_ERR(filp)) {
        ret = kernel_read(filp, 0, (char*)&mdlog_flag, sizeof(int));	
        if (ret != sizeof(int)) 
            mdlog_flag = MODE_IDLE;
    } else {
        CCCI_ERR_MSG(md->index, TAG, "open %s fail", MDLOGGER_FILE_PATH);
        filp = NULL;
    }
    if (filp != NULL) {
        filp_close(filp, NULL);
    }

    if (is_meta_mode() || is_advanced_meta_mode())
        runtime->BootingStartID = ((char)mdlog_flag << 8 | META_BOOT_ID);
    else
        runtime->BootingStartID = ((char)mdlog_flag << 8 | NORMAL_BOOT_ID);

	// share memory layout
	runtime->ExceShareMemBase = md->mem_layout.smem_region_phy - md->mem_layout.smem_offset_AP_to_MD;
	runtime->ExceShareMemSize = md->mem_layout.smem_region_size;
	runtime->TotalShareMemBase = md->mem_layout.smem_region_phy - md->mem_layout.smem_offset_AP_to_MD;
	runtime->TotalShareMemSize = md->mem_layout.smem_region_size;
	// misc region, little endian for string
	runtime->misc_prefix = 0x4353494D; // "MISC"
	runtime->misc_postfix = 0x4353494D; // "MISC"
	runtime->index = 0;
	runtime->next = 0;
	// random seed
	get_random_bytes(&random_seed, sizeof(int));
	runtime->feature_2_val[0] = random_seed;
	CCCI_INF_MSG(md->index, TAG, "random_seed=%x,misc_info.feature_2_val[0]=%x\n", random_seed, runtime->feature_2_val[0]);
	runtime->support_mask |= (FEATURE_SUPPORT<<(MISC_RAND_SEED*2));
	// SBP
#ifdef MTK_MD_SBP_CUSTOM_VALUE
	if (sbp_code > 0) {
		runtime->support_mask |= (FEATURE_SUPPORT<<(MISC_MD_SBP_SETTING * 2));
		runtime->feature_4_val[0] = sbp_code;
	}
#endif
	// CCCI debug
#if defined(FEATURE_SEQ_CHECK_EN) || defined(FEATURE_POLL_MD_EN)
	runtime->support_mask |= (FEATURE_SUPPORT<<(MISC_MD_SEQ_CHECK * 2));
	runtime->feature_5_val[0] = 0;
#ifdef FEATURE_SEQ_CHECK_EN
	runtime->feature_5_val[0] |= (1<<0);
#endif
#ifdef FEATURE_POLL_MD_EN
	runtime->feature_5_val[0] |= (1<<1);
#endif
#endif

	dump_runtime_data(md, runtime);
	skb_put(req->skb, packet_size);
	ret =  md->ops->send_request(md, 0, req); // hardcode to queue 0
	if(ret==0 && (ccci_get_md_debug_mode(md)&(DBG_FLAG_JTAG|DBG_FLAG_DEBUG))==0) {
		mod_timer(&md->bootup_timer, jiffies+BOOT_TIMER_HS1*HZ);
	}
	return ret;
}

static int md_cd_force_assert(struct ccci_modem *md, MD_COMM_TYPE type)
{
	struct ccci_request *req = NULL;
	struct ccci_header *ccci_h;

	CCCI_INF_MSG(md->index, TAG, "force assert MD using %d\n", type);
	switch(type) {
	case CCCI_MESSAGE:
		req = ccci_alloc_req(OUT, sizeof(struct ccci_header), 1, 1);
		if(req) {
			req->policy = RECYCLE;
			ccci_h = (struct ccci_header *)skb_put(req->skb, sizeof(struct ccci_header));
			ccci_h->data[0] = 0xFFFFFFFF;
			ccci_h->data[1] = 0x5A5A5A5A;
			//ccci_h->channel = CCCI_FORCE_ASSERT_CH;
			*(((u32 *)ccci_h)+2) = CCCI_FORCE_ASSERT_CH;
			ccci_h->reserved = 0xA5A5A5A5;
			return md->ops->send_request(md, 0, req); // hardcode to queue 0
		}
		return -CCCI_ERR_ALLOCATE_MEMORY_FAIL;
	case CCIF_INTERRUPT:
		md_cd_ccif_send(md, H2D_FORCE_MD_ASSERT);
		break;
	case CCIF_INTR_SEQ:
		md_cd_ccif_send(md, AP_MD_SEQ_ERROR);
		break;
	};
	return 0;
}

static int md_cd_dump_info(struct ccci_modem *md, MODEM_DUMP_FLAG flag, void *buff, int length)
{
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;

	if(flag & DUMP_FLAG_CCIF) {
		int i;
		unsigned int *dest_buff = NULL;
		char ccif_sram[CCIF_SRAM_SIZE];
		if(buff)
			dest_buff = (unsigned int*)buff;
		else
			dest_buff = (unsigned int*)ccif_sram;
		if(length<CCIF_SRAM_SIZE && length>0)
			CCCI_ERR_MSG(md->index, TAG, "dump buffer length %d/%d\n", length, CCIF_SRAM_SIZE);
		else
			length = CCIF_SRAM_SIZE;

		for(i=0; i<length/sizeof(unsigned int); i++) {
			*(dest_buff+i) = cldma_read32(AP_CCIF0_BASE, APCCIF_CHDATA+i*sizeof(unsigned int));
		}
		
		CCCI_INF_MSG(md->index, TAG, "Dump CCIF SRAM\n");
		ccci_mem_dump(dest_buff, CCIF_SRAM_SIZE);
	}
	if(flag & DUMP_FLAG_CLDMA) {
		cldma_dump_all_gpd(md_ctrl);
		cldma_dump_register(md_ctrl);
		cldma_dump_packet_history(md_ctrl);
	}
	if(flag & DUMP_FLAG_REG) {
		md_cd_lock_cldma_clock_src(1);
		CCCI_INF_MSG(md->index, TAG, "Dump MD Bus status %x\n", MD_BUS_STATUS_BASE);
		ccci_mem_dump(md_ctrl->md_bus_status, MD_BUS_STATUS_LENGTH);
		CCCI_INF_MSG(md->index, TAG, "Dump MD PC monitor %x\n", MD_PC_MONITOR_BASE);
		cldma_write32(md_ctrl->md_pc_monitor, 0, 0x80000000); // stop MD PCMon
		ccci_mem_dump(md_ctrl->md_pc_monitor, MD_PC_MONITOR_LENGTH);
		CCCI_INF_MSG(md->index, TAG, "Dump MD TOPSM status %x\n", MD_TOPSM_STATUS_BASE);
		ccci_mem_dump(md_ctrl->md_topsm_status, MD_TOPSM_STATUS_LENGTH);
		CCCI_INF_MSG(md->index, TAG, "Dump MD OST status %x\n", MD_OST_STATUS_BASE);
		ccci_mem_dump(md_ctrl->md_ost_status, MD_OST_STATUS_LENGTH);
		CCCI_INF_MSG(md->index, TAG, "Dump global con0 status %x\n", MD_GLOBAL_CON0);
		ccci_mem_dump(md_ctrl->md_global_con0, 0x4);
		md_cd_lock_cldma_clock_src(0);

		CCCI_INF_MSG(md->index, TAG, "Dump INFRACFG_AO status %x\n", (INFRACFG_AO_BASE+0x224));
		ccci_mem_dump((INFRACFG_AO_BASE+0x224), 0x4);
	}
	if(flag & DUMP_FLAG_SMEM) {
		CCCI_INF_MSG(md->index, TAG, "Dump share memory\n");
		ccci_mem_dump(md->smem_layout.ccci_exp_smem_base_vir, md->smem_layout.ccci_exp_smem_size);
	}
	if(flag & DUMP_FLAG_IMAGE) {
		CCCI_INF_MSG(md->index, KERN, "Dump MD image memory\n");
		ccci_mem_dump((void*)md->mem_layout.md_region_vir, MD_IMG_DUMP_SIZE);
	}
	if(flag & DUMP_FLAG_LAYOUT) {
		CCCI_INF_MSG(md->index, KERN, "Dump MD layout struct\n");
		ccci_mem_dump(&md->mem_layout, sizeof(struct ccci_mem_layout));
	}
	return length;
}

static struct ccci_modem_ops md_cd_ops = {
	.init = &md_cd_init,
	.start = &md_cd_start,
	.stop = &md_cd_stop,
	.reset = &md_cd_reset,
	.send_request = &md_cd_send_request,
	.give_more = &md_cd_give_more,
	.napi_poll = &md_cd_napi_poll,
	.send_runtime_data = &md_cd_send_runtime_data,
	.broadcast_state = &md_cd_broadcast_state,
	.force_assert = &md_cd_force_assert,
	.dump_info = &md_cd_dump_info,
	.write_room = &md_cd_write_room,
	.get_port_by_minor = &md_cd_get_port_by_minor,
	.get_port_by_channel = &md_cd_get_port_by_channel,
	.low_power_notify = &md_cd_low_power_notify,
};

static ssize_t md_cd_dump_show(struct ccci_modem *md, char *buf)
{	
	int count = 0;
	count = snprintf(buf, 256, "support: ccif cldma register smem image layout\n");
	return count;
}

static ssize_t md_cd_dump_store(struct ccci_modem *md, const char *buf, size_t count)
{
	// echo will bring "xxx\n" here, so we eliminate the "\n" during comparing
	if(strncmp(buf, "ccif", count-1) == 0) {
		CCCI_INF_MSG(md->index, TAG, "AP_CON(%x)=%x\n", AP_CCIF0_BASE+APCCIF_CON, cldma_read32(AP_CCIF0_BASE, APCCIF_CON));
		CCCI_INF_MSG(md->index, TAG, "AP_BUSY(%x)=%x\n", AP_CCIF0_BASE+APCCIF_BUSY, cldma_read32(AP_CCIF0_BASE, APCCIF_BUSY));
		CCCI_INF_MSG(md->index, TAG, "AP_START(%x)=%x\n", AP_CCIF0_BASE+APCCIF_START, cldma_read32(AP_CCIF0_BASE, APCCIF_START));
		CCCI_INF_MSG(md->index, TAG, "AP_TCHNUM(%x)=%x\n", AP_CCIF0_BASE+APCCIF_TCHNUM, cldma_read32(AP_CCIF0_BASE, APCCIF_TCHNUM));
		CCCI_INF_MSG(md->index, TAG, "AP_RCHNUM(%x)=%x\n", AP_CCIF0_BASE+APCCIF_RCHNUM, cldma_read32(AP_CCIF0_BASE, APCCIF_RCHNUM));
		CCCI_INF_MSG(md->index, TAG, "AP_ACK(%x)=%x\n", AP_CCIF0_BASE+APCCIF_ACK, cldma_read32(AP_CCIF0_BASE, APCCIF_ACK));

		CCCI_INF_MSG(md->index, TAG, "MD_CON(%x)=%x\n", _MD_CCIF0_BASE+APCCIF_CON, cldma_read32(_MD_CCIF0_BASE, APCCIF_CON));
		CCCI_INF_MSG(md->index, TAG, "MD_BUSY(%x)=%x\n", _MD_CCIF0_BASE+APCCIF_BUSY, cldma_read32(_MD_CCIF0_BASE, APCCIF_BUSY));
		CCCI_INF_MSG(md->index, TAG, "MD_START(%x)=%x\n", _MD_CCIF0_BASE+APCCIF_START, cldma_read32(_MD_CCIF0_BASE, APCCIF_START));
		CCCI_INF_MSG(md->index, TAG, "MD_TCHNUM(%x)=%x\n", _MD_CCIF0_BASE+APCCIF_TCHNUM, cldma_read32(_MD_CCIF0_BASE, APCCIF_TCHNUM));
		CCCI_INF_MSG(md->index, TAG, "MD_RCHNUM(%x)=%x\n", _MD_CCIF0_BASE+APCCIF_RCHNUM, cldma_read32(_MD_CCIF0_BASE, APCCIF_RCHNUM));
		CCCI_INF_MSG(md->index, TAG, "MD_ACK(%x)=%x\n", _MD_CCIF0_BASE+APCCIF_ACK, cldma_read32(_MD_CCIF0_BASE, APCCIF_ACK));
		
		md->ops->dump_info(md, DUMP_FLAG_CCIF, NULL, 0);
	}
	if(strncmp(buf, "cldma", count-1) == 0) {
		md->ops->dump_info(md, DUMP_FLAG_CLDMA, NULL, 0);
	}
	if(strncmp(buf, "register", count-1) == 0) {
		md->ops->dump_info(md, DUMP_FLAG_REG, NULL, 0);
	}
	if(strncmp(buf, "smem", count-1) == 0) {
		md->ops->dump_info(md, DUMP_FLAG_SMEM, NULL, 0);
	}
	if(strncmp(buf, "image", count-1) == 0) {
		md->ops->dump_info(md, DUMP_FLAG_IMAGE, NULL, 0);
	}
	if(strncmp(buf, "layout", count-1) == 0) {
		md->ops->dump_info(md, DUMP_FLAG_LAYOUT, NULL, 0);
	}
	return count;
}

static ssize_t md_cd_control_show(struct ccci_modem *md, char *buf)
{	
	int count = 0;
	count = snprintf(buf, 256, "support: cldma_reset cldma_stop ccif_assert\n");
	return count;
}

static ssize_t md_cd_control_store(struct ccci_modem *md, const char *buf, size_t count)
{
	if(strncmp(buf, "cldma_reset", count-1) == 0) {
		CCCI_INF_MSG(md->index, TAG, "reset CLDMA\n");
		md_cd_lock_cldma_clock_src(1);
		cldma_stop(md);
		md_cd_clear_queue(md, OUT);
		md_cd_clear_queue(md, IN);
		cldma_reset(md);
		cldma_start(md);
		md_cd_lock_cldma_clock_src(0);
	}
	if(strncmp(buf, "cldma_stop", count-1) == 0) {
		CCCI_INF_MSG(md->index, TAG, "stop CLDMA\n");
		md_cd_lock_cldma_clock_src(1);
		cldma_stop(md);
		md_cd_lock_cldma_clock_src(0);
	}
	if(strncmp(buf, "ccif_assert", count-1) == 0) {
		CCCI_INF_MSG(md->index, TAG, "use CCIF to force MD assert\n");
		md->ops->force_assert(md, CCIF_INTERRUPT);
	}
	return count;
}

static ssize_t md_cd_parameter_show(struct ccci_modem *md, char *buf)
{	
	int count = 0;
	
	count += snprintf(buf+count, 128, "CHECKSUM_SIZE=%d\n", CHECKSUM_SIZE);
	count += snprintf(buf+count, 128, "PACKET_HISTORY_DEPTH=%d\n", PACKET_HISTORY_DEPTH);
	return count;
}

static ssize_t md_cd_parameter_store(struct ccci_modem *md, const char *buf, size_t count)
{
	return count;
}

CCCI_MD_ATTR(NULL, dump, 0660, md_cd_dump_show, md_cd_dump_store);
CCCI_MD_ATTR(NULL, control, 0660, md_cd_control_show, md_cd_control_store);
CCCI_MD_ATTR(NULL, parameter, 0660, md_cd_parameter_show, md_cd_parameter_store);

static void md_cd_sysfs_init(struct ccci_modem *md)
{
	int ret;
	ccci_md_attr_dump.modem = md;
	ret = sysfs_create_file(&md->kobj, &ccci_md_attr_dump.attr);
	if(ret)
		CCCI_ERR_MSG(md->index, TAG, "fail to add sysfs node %s %d\n", 
		ccci_md_attr_dump.attr.name, ret);

	ccci_md_attr_control.modem = md;
	ret = sysfs_create_file(&md->kobj, &ccci_md_attr_control.attr);
	if(ret)
		CCCI_ERR_MSG(md->index, TAG, "fail to add sysfs node %s %d\n", 
		ccci_md_attr_control.attr.name, ret);

	ccci_md_attr_parameter.modem = md;
	ret = sysfs_create_file(&md->kobj, &ccci_md_attr_parameter.attr);
	if(ret)
		CCCI_ERR_MSG(md->index, TAG, "fail to add sysfs node %s %d\n", 
		ccci_md_attr_parameter.attr.name, ret);
}

#ifdef CCCI_STATISTIC
void md_cd_stat_timer_func(unsigned long data)
{
	struct ccci_modem *md = (struct ccci_modem *)data;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	int i;

	for(i=0; i<8; i++) { // hardcode
		md_ctrl->stat_tx_free[i] = md_ctrl->txq[i].free_slot;
	}
	for(i=0; i<8; i++) { // hardcode
		md_ctrl->stat_rx_used[i] = 0;
	}
	
	trace_md_tx_free(md_ctrl->stat_tx_free);
	trace_md_rx_used(md_ctrl->stat_rx_used);
	// mod_timer(&md_ctrl->stat_timer, jiffies+HZ/2);
}
#endif

#if TRAFFIC_MONITOR_INTERVAL
void md_cd_traffic_monitor_func(unsigned long data)
{
	struct ccci_modem *md = (struct ccci_modem *)data;
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	struct ccci_port *port;
	unsigned long long port_full=0, i; // hardcode, port number should not be larger than 64
	
	for(i=0; i<md->port_number; i++) {
		port = md->ports + i;
		if(port->flags & PORT_F_RX_FULLED)
			port_full |= (1<<i);
		if(port->tx_busy_count!=0 || port->rx_busy_count!=0) {
			CCCI_INF_MSG(md->index, TAG, "port %s busy count %d/%d\n", port->name, 
				port->tx_busy_count, port->rx_busy_count);
			port->tx_busy_count = 0;
			port->rx_busy_count = 0;
		}
	}

	CCCI_DBG_MSG(md->index, TAG, "traffic: (%d/%llx)(Tx(%x): %d,%d,%d,%d,%d,%d,%d,%d)(Rx(%x): %d,%d,%d,%d,%d,%d,%d,%d)\n", 
		md->md_state, port_full,
		md_ctrl->txq_active,
		md_ctrl->tx_traffic_monitor[0], md_ctrl->tx_traffic_monitor[1],
		md_ctrl->tx_traffic_monitor[2], md_ctrl->tx_traffic_monitor[3],
		md_ctrl->tx_traffic_monitor[4], md_ctrl->tx_traffic_monitor[5],
		md_ctrl->tx_traffic_monitor[6], md_ctrl->tx_traffic_monitor[7],
		md_ctrl->rxq_active,
		md_ctrl->rx_traffic_monitor[0], md_ctrl->rx_traffic_monitor[1],
		md_ctrl->rx_traffic_monitor[2], md_ctrl->rx_traffic_monitor[3],
		md_ctrl->rx_traffic_monitor[4], md_ctrl->rx_traffic_monitor[5],
		md_ctrl->rx_traffic_monitor[6], md_ctrl->rx_traffic_monitor[7]);
	memset(md_ctrl->tx_traffic_monitor, 0, sizeof(md_ctrl->tx_traffic_monitor));
	memset(md_ctrl->rx_traffic_monitor, 0, sizeof(md_ctrl->rx_traffic_monitor));
	//mod_timer(&md_ctrl->traffic_monitor, jiffies+TRAFFIC_MONITOR_INTERVAL*HZ);
}
#endif

static void md_cd_modem_setup(struct ccci_modem *md)
{
	struct md_cd_ctrl *md_ctrl;
	static char trm_wakelock_name[16];
	static char peer_wakelock_name[16];
	// init modem structure
	md->ops = &md_cd_ops;
	md->ports = md_cd_ports_normal;
	md->port_number = ARRAY_SIZE(md_cd_ports_normal);
	// init modem private data
	md_ctrl = (struct md_cd_ctrl *)md->private_data;
	md_ctrl->txq_active = 0;
	md_ctrl->rxq_active = 0;
	snprintf(trm_wakelock_name, sizeof(trm_wakelock_name), "ccci%d_trm", md->index);
	wake_lock_init(&md_ctrl->trm_wake_lock, WAKE_LOCK_SUSPEND, trm_wakelock_name);
	snprintf(peer_wakelock_name, sizeof(peer_wakelock_name), "ccci%d_peer", md->index);
	wake_lock_init(&md_ctrl->peer_wake_lock, WAKE_LOCK_SUSPEND, peer_wakelock_name);
	md_ctrl->tgpd_dmapool = dma_pool_create("CLDMA_TGPD_DMA",
						NULL, 
						sizeof(struct cldma_tgpd), 
						16, 
						0);
	md_ctrl->rgpd_dmapool = dma_pool_create("CLDMA_RGPD_DMA", 
						NULL, 
						sizeof(struct cldma_rgpd), 
						16, 
						0);
	INIT_WORK(&md_ctrl->ccif_work, md_cd_ccif_work);
	INIT_DELAYED_WORK(&md_ctrl->ccif_delayed_work, md_cd_ccif_delayed_work);
	init_timer(&md_ctrl->bus_timeout_timer);
	md_ctrl->bus_timeout_timer.function = md_cd_ap2md_bus_timeout_timer_func;
	md_ctrl->bus_timeout_timer.data = (unsigned long)md;
	init_timer(&md_ctrl->cldma_timeout_timer);
	md_ctrl->cldma_timeout_timer.function = cldma_timeout_timer_func;
	md_ctrl->cldma_timeout_timer.data = (unsigned long)md;
	spin_lock_init(&md_ctrl->cldma_timeout_lock);
#ifdef CCCI_STATISTIC
	init_timer(&md_ctrl->stat_timer);
	md_ctrl->stat_timer.function = md_cd_stat_timer_func;
	md_ctrl->stat_timer.data = (unsigned long)md;
#endif
	tasklet_init(&md_ctrl->cldma_irq_task, cldma_irq_task, (unsigned long)md);
	md_ctrl->channel_id = 0;
	atomic_set(&md_ctrl->reset_on_going, 0);
	atomic_set(&md_ctrl->wdt_enabled, 0);
	INIT_WORK(&md_ctrl->wdt_work, md_cd_wdt_work);
#if TRAFFIC_MONITOR_INTERVAL
	init_timer(&md_ctrl->traffic_monitor);
	md_ctrl->traffic_monitor.function = md_cd_traffic_monitor_func;
	md_ctrl->traffic_monitor.data = (unsigned long)md;
	//mod_timer(&md_ctrl->traffic_monitor, jiffies+TRAFFIC_MONITOR_INTERVAL*HZ);
#endif
}

static int ccci_modem_probe(struct platform_device *dev)
{
	struct ccci_dev_cfg* dev_cfg = (struct ccci_dev_cfg*)dev->dev.platform_data;
	struct ccci_modem *md_cd;
	int i;

	CCCI_INF_MSG(-1, TAG, "modem CLDMA module init\n");
	md_cd = ccci_allocate_modem(sizeof(struct md_cd_ctrl), md_cd_modem_setup);
	md_cd->index = dev_cfg->index;
	md_cd->major = dev_cfg->major;
	md_cd->minor_base = dev_cfg->minor_base;
	md_cd->capability = dev_cfg->capability;
	// register modem
	ccci_register_modem(md_cd);
	// add sysfs entries
	md_cd_sysfs_init(md_cd);
	// hook up to device
	dev->dev.platform_data = md_cd;
	// init CCIF
	cldma_write32(AP_CCIF0_BASE, APCCIF_CON, 0x01); // arbitration
	cldma_write32(AP_CCIF0_BASE, APCCIF_ACK, 0xFFFF);
	for(i=0; i<APCCIF_SRAM_SIZE/sizeof(u32); i++) {
		cldma_write32(AP_CCIF0_BASE, APCCIF_CHDATA+i*sizeof(u32), 0);
	}

	return 0;
}

static struct dev_pm_ops ccci_modem_pm_ops = {
    .suspend = ccci_modem_pm_suspend,
    .resume = ccci_modem_pm_resume,
    .freeze = ccci_modem_pm_suspend,
    .thaw = ccci_modem_pm_resume,
    .poweroff = ccci_modem_pm_suspend,
    .restore = ccci_modem_pm_resume,
    .restore_noirq = ccci_modem_pm_restore_noirq,
};

static struct platform_driver ccci_modem_driver =
{
	.driver = {
		.name = "cldma_modem",
#ifdef CONFIG_PM
		.pm = &ccci_modem_pm_ops,
#endif
	},
	.probe = ccci_modem_probe,
	.remove = ccci_modem_remove,
	.shutdown = ccci_modem_shutdown,
	.suspend = ccci_modem_suspend,
	.resume = ccci_modem_resume,
};

static int __init modem_cd_init(void)
{
	int ret;
	ret = platform_driver_register(&ccci_modem_driver);
	if (ret) {
		CCCI_ERR_MSG(-1, TAG, "clmda modem platform driver register fail(%d)\n", ret);
		return ret;
	}
	return 0;
}

module_init(modem_cd_init);

MODULE_AUTHOR("Xiao Wang <xiao.wang@mediatek.com>");
MODULE_DESCRIPTION("CLDMA modem driver v0.1");
MODULE_LICENSE("GPL");
