#ifndef _AUDIO_BTCVSD_DEF_H_
#define _AUDIO_BTCVSD_DEF_H_

// BT HW Register address
#if 0 // use ioremap in kernel
//#define BTSRAM_BASE                (0xA3000000)
#define BTPKT_BASE                 (0x18000000)//(0xA3340000)
#define BT_SCO_HW_REG_PACKET_R     ((volatile kal_uint32*)(BTPKT_BASE+0x0FD0))
#define BT_SCO_HW_REG_PACKET_W     ((volatile kal_uint32*)(BTPKT_BASE+0x0FD4))
#define BT_SCO_HW_REG_CONTROL      ((volatile kal_uint32*)(BTPKT_BASE+0x0FD8))

#define BT_LOOPBACK_CTL            ((volatile kal_uint16*)(0x18000000))

//#define BT_POWER_OFF               ((volatile kal_uint16*)(0xA0700170))
//#define BT_POWER_OFF_MASK          (0x2)
#endif

#define BT_SCO_PACKET_120 120
#define BT_SCO_PACKET_180 180

#define BT_CVSD_TX_NREADY    (1<<21)
#define BT_CVSD_RX_READY     (1<<22)
#define BT_CVSD_TX_UNDERFLOW (1<<23)
#define BT_CVSD_RX_OVERFLOW  (1<<24)
#define BT_CVSD_INTERRUPT    (1<<31)

#define BT_CVSD_CLEAR        (BT_CVSD_TX_NREADY | BT_CVSD_RX_READY | BT_CVSD_TX_UNDERFLOW | BT_CVSD_RX_OVERFLOW | BT_CVSD_INTERRUPT)

//TX
#define SCO_TX_ENCODE_SIZE           (60                             ) // 60 byte (60*8 samples)
#define SCO_TX_PACKER_BUF_NUM        (8                              ) // 8
#define SCO_TX_PACKET_MASK           (0x7                            ) //0x7
#define SCO_TX_PCM64K_BUF_SIZE       (SCO_TX_ENCODE_SIZE*2*8         ) // 60 * 2 * 8 byte

//RX
#define SCO_RX_PLC_SIZE              (30                    )
#define SCO_RX_PACKER_BUF_NUM        (16                    )   //16
#define SCO_RX_PACKET_MASK           (0xF                   )   //0xF
#define SCO_RX_PCM64K_BUF_SIZE       (SCO_RX_PLC_SIZE*2*8   )
#define SCO_RX_PCM8K_BUF_SIZE        (SCO_RX_PLC_SIZE*2     )

#define BTSCO_CVSD_RX_FRAME SCO_RX_PACKER_BUF_NUM
#define BTSCO_CVSD_RX_INBUF_SIZE (BTSCO_CVSD_RX_FRAME*SCO_RX_PLC_SIZE)
#define BTSCO_CVSD_PACKET_VALID_SIZE 2
#define BTSCO_CVSD_RX_TEMPINPUTBUF_SIZE (BTSCO_CVSD_RX_FRAME*(SCO_RX_PLC_SIZE+BTSCO_CVSD_PACKET_VALID_SIZE))

#define BTSCO_CVSD_TX_FRAME SCO_TX_PACKER_BUF_NUM
#define BTSCO_CVSD_TX_OUTBUF_SIZE (BTSCO_CVSD_TX_FRAME*SCO_TX_ENCODE_SIZE)

#endif
