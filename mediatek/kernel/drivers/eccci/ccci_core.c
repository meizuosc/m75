/*
 * CCCI common service and routine. Consider it as a "logical" layer.
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
#include "ccci_core.h"
#include "ccci_dfo.h"
#include "ccci_bm.h"

static LIST_HEAD(modem_list); // don't use array, due to MD index may not be continuous
static void *dev_class = NULL;

// common sub-system
extern int ccci_subsys_bm_init(void);
extern int ccci_subsys_sysfs_init(void);
extern int ccci_subsys_dfo_init(void);
// per-modem sub-system
extern int ccci_sysfs_add_modem(struct ccci_modem *md);
extern int ccci_subsys_char_init(struct ccci_modem *md);
extern int ccci_platform_init(struct ccci_modem *md);
extern void md_ex_monitor_func(unsigned long data);
extern void md_ex_monitor2_func(unsigned long data);
extern void md_bootup_timeout_func(unsigned long data);
extern void md_status_poller_func(unsigned long data);
extern void md_status_timeout_func(unsigned long data);
//used for throttling feature - start
unsigned long ccci_modem_boot_count[5];
unsigned long ccci_get_md_boot_count(int md_id)
{
	return ccci_modem_boot_count[md_id];
}
//used for throttling feature - end

ssize_t boot_md_show(char *buf)
{
	int curr = 0;
	struct ccci_modem *md;

	list_for_each_entry(md, &modem_list, entry) {
		if(md->config.setting & MD_SETTING_ENABLE)
			curr += snprintf(&buf[curr], 128, "md%d:%d/%d/%d\n", md->index+1, 
						md->md_state, md->boot_stage, md->ex_stage);
	}
    return curr;
}

ssize_t boot_md_store(const char *buf, size_t count)
{
	unsigned int md_id;
	struct ccci_modem *md;

	md_id = buf[0] - '0';
	CCCI_INF_MSG(-1, CORE, "ccci core boot md%d\n", md_id);
	list_for_each_entry(md, &modem_list, entry) {
		if(md->index == md_id && md->md_state == GATED) {
			md->ops->start(md);
			return count;
		}
	}
	return count;
}

static int __init ccci_init(void)
{
	CCCI_INF_MSG(-1, CORE, "ccci core init\n");
	dev_class = class_create(THIS_MODULE, "ccci_node");
	// init common sub-system
	ccci_subsys_dfo_init();
	ccci_subsys_sysfs_init();
	ccci_subsys_bm_init();
	return 0;
}

// setup function is only for data structure initialization
struct ccci_modem *ccci_allocate_modem(int private_size, void (*setup)(struct ccci_modem *md))
{
	struct ccci_modem* md = kzalloc(sizeof(struct ccci_modem), GFP_KERNEL);
	int i;
	
	md->private_data = kzalloc(private_size, GFP_KERNEL);
	md->sim_type = 0xEEEEEEEE; //sim_type(MCC/MNC) sent by MD wouldn't be 0xEEEEEEEE
	md->config.setting |= MD_SETTING_FIRST_BOOT;
	md->md_state = INVALID;
	md->boot_stage = MD_BOOT_STAGE_0;
	md->ex_stage = EX_NONE;
	atomic_set(&md->wakeup_src, 0);
	INIT_LIST_HEAD(&md->entry);
	ccci_reset_seq_num(md);
	
	init_timer(&md->bootup_timer);	
	md->bootup_timer.function = md_bootup_timeout_func;
	md->bootup_timer.data = (unsigned long)md;
	init_timer(&md->ex_monitor);
	md->ex_monitor.function = md_ex_monitor_func;
	md->ex_monitor.data = (unsigned long)md;
	init_timer(&md->ex_monitor2);
	md->ex_monitor2.function = md_ex_monitor2_func;
	md->ex_monitor2.data = (unsigned long)md;
	init_timer(&md->md_status_poller);
	md->md_status_poller.function = md_status_poller_func;
	md->md_status_poller.data = (unsigned long)md;
	init_timer(&md->md_status_timeout);
	md->md_status_timeout.function = md_status_timeout_func;
	md->md_status_timeout.data = (unsigned long)md;
	
	spin_lock_init(&md->ctrl_lock);
	for(i=0; i<ARRAY_SIZE(md->rx_ch_ports); i++) {
		INIT_LIST_HEAD(&md->rx_ch_ports[i]);
	}
	setup(md);
	return md;
}
EXPORT_SYMBOL(ccci_allocate_modem);

int ccci_register_modem(struct ccci_modem *modem)
{
	int ret;
	
	CCCI_INF_MSG(-1, CORE, "register modem %d\n", modem->major);
	// init modem
	// TODO: check modem->ops for all must-have functions
	ret = modem->ops->init(modem);
	if(ret<0)
		return ret;
	ccci_config_modem(modem);
	list_add_tail(&modem->entry, &modem_list);
	// init per-modem sub-system
	ccci_subsys_char_init(modem);
	ccci_sysfs_add_modem(modem);
	ccci_platform_init(modem);
	return 0;
}
EXPORT_SYMBOL(ccci_register_modem);

struct ccci_modem *ccci_get_modem_by_id(int md_id)
{
	struct ccci_modem *md = NULL;
	list_for_each_entry(md, &modem_list, entry) {
		if(md->index == md_id)
			return md;
	}
	return NULL;
}

int ccci_get_modem_state(int md_id)
{
	struct ccci_modem *md = NULL;
	list_for_each_entry(md, &modem_list, entry) {
		if(md->index == md_id)
			return md->md_state;
	}
	return -CCCI_ERR_MD_INDEX_NOT_FOUND;
}

int exec_ccci_kern_func_by_md_id(int md_id, unsigned int id, char *buf, unsigned int len)
{
	struct ccci_modem *md = NULL;
	int ret = 0;
	
	list_for_each_entry(md, &modem_list, entry) {
		if(md->index == md_id) {
			ret = 1;
			break;
		}
	}
	if(!ret)
		return -CCCI_ERR_MD_INDEX_NOT_FOUND;

	CCCI_DBG_MSG(md->index, CORE, "%ps execuste function %d\n", __builtin_return_address(0), id);
	switch(id) {
	case ID_GET_MD_WAKEUP_SRC:		
		atomic_set(&md->wakeup_src, 1);
		break;
	case ID_GET_TXPOWER:
		if(buf[0] == 0) {
			ret = ccci_send_msg_to_md(md, CCCI_SYSTEM_TX, MD_TX_POWER, 0, 0);
		} else if(buf[0] == 1) {
			ret = ccci_send_msg_to_md(md, CCCI_SYSTEM_TX, MD_RF_TEMPERATURE, 0, 0);
		} else if(buf[0] == 2) {
			ret = ccci_send_msg_to_md(md, CCCI_SYSTEM_TX, MD_RF_TEMPERATURE_3G, 0, 0);
		}
		break;
	case ID_PAUSE_LTE:
		/*
		 * MD booting/flight mode/exception mode: return >0 to DVFS.
		 * MD ready: return 0 if message delivered, return <0 if get error.
		 * DVFS will call this API with IRQ disabled.
		 */
		if(md->md_state != READY)
			ret = 1;
		else
			ret = ccci_send_msg_to_md(md, CCCI_SYSTEM_TX, MD_PAUSE_LTE, *((int *)buf), 1);
		break;
   	case ID_STORE_SIM_SWITCH_MODE:
        {
            int simmode = *((int*)buf);
            ccci_store_sim_switch_mode(md, simmode);
        }
        break;
    case ID_GET_SIM_SWITCH_MODE:
        {
            int simmode = ccci_get_sim_switch_mode();
            memcpy(buf, &simmode, sizeof(int));
        }
        break;
	case ID_GET_MD_STATE:
		ret = md->boot_stage;
		break;
	default:
		ret = -CCCI_ERR_FUNC_ID_ERROR;
		break;
	};
	return ret;
}

struct ccci_port *ccci_get_port_for_node(int major, int minor)
{
	struct ccci_modem *md = NULL;
	struct ccci_port *port = NULL;
	
	list_for_each_entry(md, &modem_list, entry) {
		if(md->major == major) {			
			port = md->ops->get_port_by_minor(md, minor);
			break;
		}
	}
	return port;
}

int ccci_register_dev_node(const char *name, int major_id, int minor)
{
	int ret = 0;
	dev_t dev_n;
	struct device *dev;

	dev_n = MKDEV(major_id, minor);
	dev = device_create(dev_class, NULL, dev_n, NULL, "%s", name);

	if(IS_ERR(dev)) {
		ret = PTR_ERR(dev);
	}
	
	return ret;
}

/*
 * kernel inject CCCI message to modem.
 */
int ccci_send_msg_to_md(struct ccci_modem *md, CCCI_CH ch, CCCI_MD_MSG msg, u32 resv, int blocking)
{
	struct ccci_port *port = NULL;
	struct ccci_request *req = NULL;
	struct ccci_header *ccci_h;
	int ret;

	if(md->md_state!=BOOTING && md->md_state!=READY && md->md_state!=EXCEPTION)
		return -CCCI_ERR_MD_NOT_READY;
	if(ch==CCCI_SYSTEM_TX && md->md_state!=READY)
		return -CCCI_ERR_MD_NOT_READY;
	
	port = md->ops->get_port_by_channel(md, ch);
	if(port) {
		if(!blocking)
			req = ccci_alloc_req(OUT, sizeof(struct ccci_header), 0, 0);
		else
			req = ccci_alloc_req(OUT, sizeof(struct ccci_header), 1, 1);
		if(req) {
			req->policy = RECYCLE;
			ccci_h = (struct ccci_header *)skb_put(req->skb, sizeof(struct ccci_header));
			ccci_h->data[0] = CCCI_MAGIC_NUM;
			ccci_h->data[1] = msg;
			ccci_h->channel = ch;
			ccci_h->reserved = resv;
			ret = ccci_port_send_request(port, req);
			if(ret)
				ccci_free_req(req);
			return ret;
		} else {
			return -CCCI_ERR_ALLOCATE_MEMORY_FAIL;
		}
	}
	return -CCCI_ERR_INVALID_LOGIC_CHANNEL_ID;
}

/*
 * kernel inject message to user space daemon, this function may sleep
 */
int ccci_send_virtual_md_msg(struct ccci_modem *md, CCCI_CH ch, CCCI_MD_MSG msg, u32 resv)
{
	struct ccci_request *req = NULL;
	struct ccci_header *ccci_h;
	int ret=0, count=0;

	if(unlikely(ch != CCCI_MONITOR_CH)) {
		CCCI_ERR_MSG(md->index, CORE, "invalid channel %x for sending virtual msg\n", ch);
		return -CCCI_ERR_INVALID_LOGIC_CHANNEL_ID;
	}
	if(unlikely(in_interrupt() || in_atomic())) {
		CCCI_ERR_MSG(md->index, CORE, "sending virtual msg from IRQ context %ps\n", __builtin_return_address(0));
		return -CCCI_ERR_ASSERT_ERR;
	}

	req = ccci_alloc_req(IN, sizeof(struct ccci_header), 1, 0);
	// request will be recycled in char device's read function
	ccci_h = (struct ccci_header *)skb_put(req->skb, sizeof(struct ccci_header));
	ccci_h->data[0] = CCCI_MAGIC_NUM;
	ccci_h->data[1] = msg;
	ccci_h->channel = ch;
#ifdef FEATURE_SEQ_CHECK_EN
	ccci_h->assert_bit = 0;
#endif
	ccci_h->reserved = resv;
	INIT_LIST_HEAD(&req->entry);  // as port will run list_del
retry:
	ret = ccci_port_recv_request(md, req);
	if(ret>=0 || ret==-CCCI_ERR_DROP_PACKET) {
		return ret;
	} else {
		msleep(100);
		if(count++<20) {
			goto retry;
		} else {
			CCCI_ERR_MSG(md->index, CORE, "fail to send virtual msg %x for %ps\n", msg, __builtin_return_address(0));
			list_del(&req->entry);
			req->policy = RECYCLE;
			ccci_free_req(req);
		}
	}
	return ret;
}

subsys_initcall(ccci_init);

MODULE_AUTHOR("Xiao Wang <xiao.wang@mediatek.com>");
MODULE_DESCRIPTION("Unified CCCI driver v0.1");
MODULE_LICENSE("GPL");
