/*
 * MediaTek Inc. (C) 2010.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 * 
 *  This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _HCI_STP_H
#define _HCI_STP_H

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/poll.h>

#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/signal.h>
#include <linux/ioctl.h>
#include <linux/skbuff.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

/* debugging */
#include <linux/time.h>
#include <linux/delay.h>

/* constant of kernel version */
#include <linux/version.h>

/* kthread APIs */
#include <linux/kthread.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>


/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

#define HCI_STP_TX_TASKLET (0) /* do tx in a tasklet context */
#define HCI_STP_TX_THRD (1) /* do tx in an init thread context */
/* select tx context */
#define HCI_STP_TX (HCI_STP_TX_THRD)


#if (HCI_STP_TX == HCI_STP_TX_TASKLET)
#define HCI_STP_TX_TASKLET_RWLOCK (0) /* use rwlock_t */
#define HCI_STP_TX_TASKLET_SPINLOCK (1) /* use spinlock_t */
/* select txq protection method */
#define HCI_STP_TX_TASKLET_LOCK (HCI_STP_TX_TASKLET_SPINLOCK)
#endif

/* Flag to enable BD address auto-gen mechanism */
/* Auto-gen address is illegal, default disabled */
#define BD_ADDR_AUTOGEN (0)

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/* HCI-STP flag bits */
#define HCI_STP_PROTO_SET (0)
/* HCI-STP flag TX states bits */
#define HCI_STP_SENDING (1)
#define HCI_STP_TX_WAKEUP (2)

/* maximum delay required, shall also consider possible delay on a busy system. */
#define BT_CMD_DELAY_MS_COMM (100) /*(50)*/
#define BT_CMD_DELAY_MS_RESET (600) /*(500)*/

#define BT_CMD_DELAY_SAFE_GUARD (20) /*(2)*/

/* HCI-STP safer hci_reset handling */
#define HCI_STP_SAFE_RESET (1)

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
struct hci_stp_init_cmd {
    unsigned char *hci_cmd;
    unsigned int cmdSz;
    unsigned char *hci_evt;
    unsigned int evtSz;
    char *str;
};

struct hci_stp {
    struct hci_dev *hdev;
    unsigned long flags;

    struct sk_buff_head txq; /* used to queue TX packets */
    unsigned long tx_state;

    struct work_struct init_work;
    struct completion *p_init_comp;
    wait_queue_head_t *p_init_evt_wq;
    spinlock_t init_lock; /* protect init variables: comp and wq */
    unsigned int init_cmd_idx;
    int init_evt_rx_flag; /* init result of last sent cmd */

#if HCI_STP_SAFE_RESET
    wait_queue_head_t reset_wq;
    atomic_t reset_count; /* !0: reset in progress */
#endif
    //void *priv; /* unused? */
    //struct sk_buff *tx_skb; /* unused? */
    //spinlock_t rx_lock; /* unused? */
};

/* This structure can refer to custom/common/cgen/cfgfileinc/CFG_BT_File.h:ap_nvram_btradio_struct */
struct btradio_conf_data {
  unsigned char addr[6];
  unsigned char voice[2];
  unsigned char codec[4];
  unsigned char radio[6];
  unsigned char sleep[7];
  unsigned char feature[2];
  unsigned char tx_pwr_offset[3];
  unsigned char coex_adjust[6];      /* BT/WIFI coexistence performance adjustment */
};
/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/
#define hci_stp_init_entry(c) \
  {.hci_cmd=c, .cmdSz=sizeof(c), .hci_evt=c##_evt, .evtSz=sizeof(c##_evt), .str=#c}

#endif /* end of _HCI_STP_H */

