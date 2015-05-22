/*
** $Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/include/nic_init_cmd_event.h#1 $
*/

/*! \file   "nic_init_cmd_event.h"
    \brief This file contains the declairation file of the WLAN initialization routines
           for MediaTek Inc. 802.11 Wireless LAN Adapters.
*/



/*
** $Log: nic_init_cmd_event.h $
**
** 05 03 2013 cp.wu
** [BORA00002227] [MT6630 Wi-Fi][Driver] Update for Makefile and HIFSYS modifications
** change to use FT=1 for firmware download path
**
** 02 06 2013 cp.wu
** [BORA00002227] [MT6630 Wi-Fi][Driver] Update for Makefile and HIFSYS modifications
** add reset option for firmware download configuration
**
** 02 05 2013 cp.wu
** [BORA00002227] [MT6630 Wi-Fi][Driver] Update for Makefile and HIFSYS modifications
** 1. change to use long format (FT=1) for initial command
** 2. fix a typo
**
** 02 01 2013 cp.wu
** [BORA00002227] [MT6630 Wi-Fi][Driver] Update for Makefile and HIFSYS modifications
** 1. eliminate MT5931/MT6620/MT6628 logic
** 2. add firmware download control sequence
**
** 11 29 2012 cp.wu
** [BORA00002227] [MT6630 Wi-Fi][Driver] Update for Makefile and HIFSYS modifications
** add interface to indicate that Wi-Fi to start with calibration delayed for reconnection within 1.1sec
**
** 11 01 2012 cp.wu
** [BORA00002227] [MT6630 Wi-Fi][Driver] Update for Makefile and HIFSYS modifications
** update to MT6630 CMD/EVENT definitions.
** 
** 09 17 2012 cm.chang
** [BORA00002149] [MT6630 Wi-Fi] Initial software development
** Duplicate source from MT6620 v2.3 driver branch
** (Davinci label: MT6620_WIFI_Driver_V2_3_120913_1942_As_MT6630_Base)
 *
 * 09 26 2011 cp.wu
 * [WCXRP00001011] [MT6628 Wi-Fi] Firmware Download Agent: make CRC validation as an optional feature
 * add definition for disabling CRC32 validation (for MT6628 only)
 *
 * 07 08 2010 cp.wu
 * 
 * [WPD00003833] [MT6620 and MT5931] Driver migration - move to new repository.
 *
 * 06 06 2010 kevin.huang
 * [WPD00003832][MT6620 5931] Create driver base 
 * [MT6620 5931] Create driver base
 *
 * 03 12 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP 
 * add two option for ACK and ENCRYPTION for firmware download
 *
 * 03 01 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP 
 * add command/event definitions for initial states
 *
 * 02 10 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP 
 * implement host-side firmware download logic
 *
 * 02 08 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP 
 * prepare for implementing fw download logic
 *
*/
#ifndef _NIC_INIT_CMD_EVENT_H
#define _NIC_INIT_CMD_EVENT_H

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

#include "gl_typedef.h"

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#define INIT_CMD_STATUS_SUCCESS                 0
#define INIT_CMD_STATUS_REJECTED_INVALID_PARAMS 1
#define INIT_CMD_STATUS_REJECTED_CRC_ERROR      2
#define INIT_CMD_STATUS_REJECTED_DECRYPT_FAIL   3
#define INIT_CMD_STATUS_UNKNOWN                 4

#define EVENT_HDR_SIZE          OFFSET_OF(WIFI_EVENT_T, aucBuffer[0])


#define INIT_CMD_PQ_ID              (0x8000)
#define INIT_CMD_PACKET_TYPE_ID     (0xA0)

#define INIT_CMD_PDA_PQ_ID          (0xC000)
#define INIT_CMD_PDA_PACKET_TYPE_ID (0xA0)


typedef enum _ENUM_INIT_CMD_ID {
    INIT_CMD_ID_DOWNLOAD_CONFIG = 1,
    INIT_CMD_ID_WIFI_START,
    INIT_CMD_ID_ACCESS_REG,
    INIT_CMD_ID_QUERY_PENDING_ERROR,
    INIT_CMD_ID_PATCH_START,
    INIT_CMD_ID_PATCH_WRITE,
    INIT_CMD_ID_PATCH_FINISH,
    INIT_CMD_ID_NUM
} ENUM_INIT_CMD_ID, *P_ENUM_INIT_CMD_ID;

typedef enum _ENUM_INIT_EVENT_ID {
    INIT_EVENT_ID_CMD_RESULT = 1,
    INIT_EVENT_ID_ACCESS_REG,
    INIT_EVENT_ID_PENDING_ERROR
} ENUM_INIT_EVENT_ID, *P_ENUM_INIT_EVENT_ID;

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
typedef UINT_8 CMD_STATUS;

// commands
typedef struct _INIT_WIFI_CMD_T {
    UINT_8      ucCID;
    UINT_8      ucPktTypeID;    /* Must be 0x20 (CMD Packet) */
    UINT_8      ucReserved;
    UINT_8      ucSeqNum;

    UINT_8      aucBuffer[0];
} INIT_WIFI_CMD_T, *P_INIT_WIFI_CMD_T;

typedef struct _INIT_HIF_TX_HEADER_T {
    UINT_16     u2TxByteCount;  /* Max value is over 2048 */
    UINT_16     u2PQ_ID;        /* Must be 0x8000 (Port1, Queue 0) */

    INIT_WIFI_CMD_T rInitWifiCmd;
} INIT_HIF_TX_HEADER_T, *P_INIT_HIF_TX_HEADER_T;

#define DOWNLOAD_CONFIG_ENCRYPTION_MODE     BIT(0)
#define DOWNLOAD_CONFIG_RESET_OPTION        BIT(3)
#define DOWNLOAD_CONFIG_ACK_OPTION          BIT(31)
typedef struct _INIT_CMD_DOWNLOAD_CONFIG {
    UINT_32     u4Address;
    UINT_32     u4Length;
    UINT_32     u4DataMode;
} INIT_CMD_DOWNLOAD_CONFIG, *P_INIT_CMD_DOWNLOAD_CONFIG;

#define START_OVERRIDE_START_ADDRESS    BIT(0)
#define START_DELAY_CALIBRATION         BIT(1)
typedef struct _INIT_CMD_WIFI_START {
    UINT_32     u4Override;
    UINT_32     u4Address;
} INIT_CMD_WIFI_START, *P_INIT_CMD_WIFI_START;

typedef struct _INIT_CMD_ACCESS_REG {
    UINT_8      ucSetQuery;
    UINT_8      aucReserved[3];
    UINT_32     u4Address;
    UINT_32     u4Data;
} INIT_CMD_ACCESS_REG, *P_INIT_CMD_ACCESS_REG;

// Events
typedef struct _INIT_WIFI_EVENT_T {
    UINT_16     u2RxByteCount;
    UINT_16     u2PacketType;    /* Must be filled with 0xE000 (EVENT Packet) */
    UINT_8      ucEID;
    UINT_8      ucSeqNum;
    UINT_8      aucReserved[2];

    UINT_8      aucBuffer[0];
} INIT_WIFI_EVENT_T, *P_INIT_WIFI_EVENT_T;

typedef struct _INIT_HIF_RX_HEADER_T {
    INIT_WIFI_EVENT_T rInitWifiEvent;
} INIT_HIF_RX_HEADER_T, *P_INIT_HIF_RX_HEADER_T;

typedef struct _INIT_EVENT_CMD_RESULT {
    UINT_8      ucStatus;   // 0: success 
                            // 1: rejected by invalid param 
                            // 2: rejected by incorrect CRC
                            // 3: rejected by decryption failure
                            // 4: unknown CMD
                            // 5: timeout
    UINT_8      aucReserved[3];
} INIT_EVENT_CMD_RESULT, *P_INIT_EVENT_CMD_RESULT, INIT_EVENT_PENDING_ERROR, *P_INIT_EVENT_PENDING_ERROR;

typedef struct _INIT_EVENT_ACCESS_REG {
    UINT_32     u4Address;
    UINT_32     u4Data;
} INIT_EVENT_ACCESS_REG, *P_INIT_EVENT_ACCESS_REG;

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

#endif /* _NIC_INIT_CMD_EVENT_H */

