#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/ipv6.h>
#include <linux/skbuff.h>
#include <linux/module.h>
#include "ccci_core.h"
#include "ccci_bm.h"

struct netdev_entity {
	struct napi_struct napi;
	struct net_device *ndev;
#ifndef FEATURE_SEQ_CHECK_EN
	unsigned int rx_seq_num;
	unsigned int tx_seq_num;
#endif
};

static int ccmni_open(struct net_device *dev)
{
	struct ccci_port *port = *((struct ccci_port **)netdev_priv(dev));
	struct netdev_entity *nent = (struct netdev_entity *)port->private_data;

	atomic_inc(&port->usage_cnt);
	netif_start_queue(dev);
	if(likely(port->modem->capability & MODEM_CAP_NAPI)) {
		napi_enable(&nent->napi);
		napi_schedule(&nent->napi);
	}
	return 0;
}

static int ccmni_close(struct net_device *dev)
{
	struct ccci_port *port = *((struct ccci_port **)netdev_priv(dev));

	atomic_dec(&port->usage_cnt);
	netif_stop_queue(dev);
	if(likely(port->modem->capability & MODEM_CAP_NAPI))
		napi_disable(&((struct netdev_entity *)port->private_data)->napi);
	return 0;
}

static inline int skb_is_ack(struct sk_buff *skb)
{
	int is_ack = 0;

	unsigned int data_len, protoff;
	struct tcphdr _tcph;
	const struct tcphdr *th;

	if((skb->len<=100) && (skb->protocol==htons(ETH_P_IP))) {
		if((ip_hdr(skb)->protocol) == IPPROTO_TCP) {
			protoff = (ip_hdr(skb)->ihl*4);
			th = skb_header_pointer(skb, protoff, sizeof(_tcph), &_tcph);
			data_len = skb->len - protoff - th->doff*4;
			if(data_len == 0)
				is_ack = 1;
		}
	}

	return is_ack;
}

#define NET_DAT_TXQ_INDEX(p) ((p)->modem->md_state==EXCEPTION?(p)->txq_exp_index:(p)->txq_index)
#define NET_ACK_TXQ_INDEX(p) ((p)->modem->md_state==EXCEPTION?(p)->txq_exp_index:((p)->txq_exp_index&0x0F))

static int ccmni_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct ccci_port *port = *((struct ccci_port **)netdev_priv(dev));
	struct ccci_request *req = NULL;
	struct ccci_header *ccci_h;
	int ret;
	int skb_len = skb->len;
	static int tx_busy_retry_cnt = 0;
	int tx_queue, tx_channel;

#ifndef FEATURE_SEQ_CHECK_EN
	struct netdev_entity *nent = (struct netdev_entity *)port->private_data;
	CCCI_DBG_MSG(port->modem->index, NET, "write on %s, len=%d/%d, curr_seq=%d\n", 
		port->name, skb_headroom(skb), skb->len, nent->tx_seq_num);
#else
	CCCI_DBG_MSG(port->modem->index, NET, "write on %s, len=%d/%d\n", port->name, skb_headroom(skb), skb->len);
#endif

	if(unlikely(skb->len > CCMNI_MTU)) {
		CCCI_ERR_MSG(port->modem->index, NET, "exceeds MTU(%d) with %d/%d\n", CCMNI_MTU, dev->mtu, skb->len);
		dev_kfree_skb(skb);
		dev->stats.tx_dropped++;
		return NETDEV_TX_OK;
    }
    if(unlikely(skb_headroom(skb) < sizeof(struct ccci_header))) {
		CCCI_ERR_MSG(port->modem->index, NET, "not enough header room on %s, len=%d header=%d hard_header=%d\n",
			port->name, skb->len, skb_headroom(skb), dev->hard_header_len);
		dev_kfree_skb(skb);
		dev->stats.tx_dropped++;
        return NETDEV_TX_OK;
    }
    if(unlikely(port->modem->md_state != READY)) {
		dev_kfree_skb(skb);
		dev->stats.tx_dropped++;
        return NETDEV_TX_OK;
    }

	req = ccci_alloc_req(OUT, -1, 1, 0);
	if(req) {
		if(likely(port->rx_ch != CCCI_CCMNI3_RX)) {
			if(unlikely(skb_is_ack(skb))) {
				tx_channel = port->tx_ch==CCCI_CCMNI1_TX?CCCI_CCMNI1_DL_ACK:CCCI_CCMNI2_DL_ACK;
				tx_queue = NET_ACK_TXQ_INDEX(port);
			} else {
				tx_channel = port->tx_ch;
				tx_queue = NET_DAT_TXQ_INDEX(port);
			}
		} else {
			tx_channel = port->tx_ch;
			tx_queue = NET_DAT_TXQ_INDEX(port);
		}

		req->skb = skb;
		req->policy = FREE;
		ccci_h = (struct ccci_header*)skb_push(skb, sizeof(struct ccci_header));
		ccci_h->channel = tx_channel;
		ccci_h->data[0] = 0;
		ccci_h->data[1] = skb->len; // as skb->len already included ccci_header after skb_push
#ifndef FEATURE_SEQ_CHECK_EN
		ccci_h->reserved = nent->tx_seq_num++;
#else
		ccci_h->reserved = 0;
#endif
		ret = port->modem->ops->send_request(port->modem, tx_queue, req);
		if(ret) {
			skb_pull(skb, sizeof(struct ccci_header)); // undo header, in next retry, we'll reserve header again
			req->policy = NOOP; // if you return busy, do NOT free skb as network may still use it
			ccci_free_req(req);
			goto tx_busy;
		}
		dev->stats.tx_packets++;
		dev->stats.tx_bytes += skb_len;
		tx_busy_retry_cnt = 0;
	} else {
		CCCI_ERR_MSG(port->modem->index, NET, "fail to alloc request\n");
		goto tx_busy;
	}
	return NETDEV_TX_OK;

tx_busy:
	if((++tx_busy_retry_cnt)%20000 == 0)
		CCCI_INF_MSG(port->modem->index, NET, "%s TX busy: retry_times=%d\n", port->name, tx_busy_retry_cnt);
	return NETDEV_TX_BUSY;
}

static void ccmni_tx_timeout(struct net_device *dev)
{
	dev->stats.tx_errors++;
	netif_wake_queue(dev);
}

static const struct net_device_ops ccmni_netdev_ops = 
{
	.ndo_open		= ccmni_open,
	.ndo_stop		= ccmni_close,
	.ndo_start_xmit	= ccmni_start_xmit,
	.ndo_tx_timeout	= ccmni_tx_timeout,
};

int port_net_poll(struct napi_struct *napi ,int budget)
{
	struct ccci_port *port = *((struct ccci_port **)netdev_priv(napi->dev));

	return port->modem->ops->napi_poll(port->modem, PORT_RXQ_INDEX(port), napi, budget);
}

static int port_net_init(struct ccci_port *port)
{
	struct ccci_port **temp;
	struct net_device *dev = NULL;
	struct netdev_entity *nent = NULL;
	
	CCCI_DBG_MSG(port->modem->index, NET, "network port is initializing\n");
	dev = alloc_etherdev(sizeof(struct ccci_port *));
	dev->header_ops = NULL;
	dev->mtu = CCMNI_MTU;
	dev->tx_queue_len = 1000;
	dev->watchdog_timeo = 1*HZ;
	dev->flags = IFF_NOARP & /* ccmni is a pure IP device */ 
				(~IFF_BROADCAST & ~IFF_MULTICAST);	/* ccmni is P2P */
	dev->features = NETIF_F_VLAN_CHALLENGED; /* not support VLAN */
	dev->addr_len = ETH_ALEN; /* ethernet header size */
	dev->destructor = free_netdev;
	dev->hard_header_len += sizeof(struct ccci_header); /* reserve Tx CCCI header room */
	dev->netdev_ops = &ccmni_netdev_ops;
	
	temp = netdev_priv(dev);
	*temp = port;
	sprintf(dev->name, "%s", port->name);
	
	random_ether_addr((u8 *) dev->dev_addr);

	nent = kzalloc(sizeof(struct netdev_entity), GFP_KERNEL);
	nent->ndev = dev;
	if(likely(port->modem->capability & MODEM_CAP_NAPI))
		netif_napi_add(dev, &nent->napi, port_net_poll, NAPI_POLL_WEIGHT); // hardcode
	port->private_data = nent;
	register_netdev(dev);
	CCCI_DBG_MSG(port->modem->index, NET, "network device %s hard_header_len=%d\n", dev->name, dev->hard_header_len);
	return 0;
}

static void ccmni_make_etherframe(void *_eth_hdr, unsigned char *mac_addr, unsigned int packet_type)
{
	struct ethhdr *eth_hdr = _eth_hdr;

	memcpy(eth_hdr->h_dest,   mac_addr, sizeof(eth_hdr->h_dest));
	memset(eth_hdr->h_source, 0, sizeof(eth_hdr->h_source));
	if(packet_type == 0x60) {
		eth_hdr->h_proto = __constant_cpu_to_be16(ETH_P_IPV6);
	} else {
		eth_hdr->h_proto = __constant_cpu_to_be16(ETH_P_IP);
	}
}

static int port_net_recv_req(struct ccci_port *port, struct ccci_request* req)
{
	struct sk_buff *skb = req->skb;
	struct netdev_entity *nent = (struct netdev_entity *)port->private_data;
	struct net_device *dev = nent->ndev;
	unsigned int packet_type;
	int skb_len = req->skb->len;

#ifndef FEATURE_SEQ_CHECK_EN
	struct ccci_header *ccci_h = (struct ccci_header*)req->skb->data;
	CCCI_DBG_MSG(port->modem->index, NET, "recv on %s, curr_seq=%d\n", port->name, ccci_h->reserved);
	if(unlikely(nent->rx_seq_num!=0 && (ccci_h->reserved-nent->rx_seq_num)!=1)) {
		CCCI_DBG_MSG(port->modem->index, NET, "possible packet lost on %s %d->%d\n", 
			port->name, nent->rx_seq_num, ccci_h->reserved);
	}
	nent->rx_seq_num = ccci_h->reserved;
#else
	CCCI_DBG_MSG(port->modem->index, NET, "recv on %s\n", port->name);
#endif

	list_del(&req->entry); // dequeue from queue's list
	skb_pull(skb, sizeof(struct ccci_header));
	packet_type = skb->data[0] & 0xF0;
	ccmni_make_etherframe(skb->data-ETH_HLEN, dev->dev_addr, packet_type);
    skb_set_mac_header(skb, -ETH_HLEN);
	skb->dev = dev;
	if(packet_type == 0x60) {            
		skb->protocol  = htons(ETH_P_IPV6);
	} else {
		skb->protocol  = htons(ETH_P_IP);
	}
	skb->ip_summed = CHECKSUM_NONE;
	if(likely(port->modem->capability & MODEM_CAP_NAPI))
		netif_receive_skb(skb);
	else
		netif_rx(skb);
	dev->stats.rx_packets++;
	dev->stats.rx_bytes += skb_len;
	req->policy = NOOP;
	ccci_free_req(req);
	wake_lock_timeout(&port->rx_wakelock, HZ);
	return 0;
}

static void port_net_md_state_notice(struct ccci_port *port, MD_STATE state)
{
	struct netdev_entity *nent = (struct netdev_entity *)port->private_data;
	struct net_device *dev = nent->ndev;

	switch(state) {
	case RX_IRQ:
		napi_schedule(&nent->napi);
		wake_lock_timeout(&port->rx_wakelock, HZ);
		break;
	case READY:
		netif_carrier_on(dev);
		break;
	case EXCEPTION:
	case RESET:
		netif_carrier_off(dev);
#ifndef FEATURE_SEQ_CHECK_EN
		nent->tx_seq_num = 0;
		nent->rx_seq_num = 0;
#endif
		break;
	default:
		break;
	};
}

struct ccci_port_ops net_port_ops = {
	.init = &port_net_init,
	.recv_request = &port_net_recv_req,
	.md_state_notice = &port_net_md_state_notice,
};
