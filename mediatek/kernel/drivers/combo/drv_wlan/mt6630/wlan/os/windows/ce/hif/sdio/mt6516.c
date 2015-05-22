/*
** $Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/os/windows/ce/hif/sdio/mt6516.c#1 $
*/

/*! \file   "mt6516.c"
    \brief  Define MT6516-platform related functions

*/



/*
** $Log: mt6516.c $
** 
** 09 17 2012 cm.chang
** [BORA00002149] [MT6630 Wi-Fi] Initial software development
** Duplicate source from MT6620 v2.3 driver branch
** (Davinci label: MT6620_WIFI_Driver_V2_3_120913_1942_As_MT6630_Base)
 *
 * 03 14 2011 terry.wu
 * [WCXRP00000521] [MT6620 Wi-Fi][Driver] Remove non-standard debug message
 * Revert windows debug message.
 *
 * 07 08 2010 cp.wu
 * 
 * [WPD00003833] [MT6620 and MT5931] Driver migration - move to new repository.
 *
 * 06 06 2010 kevin.huang
 * [WPD00003832][MT6620 5931] Create driver base 
 * [MT6620 5931] Create driver base
**  \main\maintrunk.MT6620WiFiDriver_Prj\2 2009-03-10 20:29:24 GMT mtk01426
**  Init for develop
**
*/

/******************************************************************************
*                         C O M P I L E R   F L A G S
*******************************************************************************
*/

/******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
*******************************************************************************
*/
#include "gl_os.h"
LINT_EXT_HEADER_BEGIN
#include <ceddk.h>
LINT_EXT_HEADER_END
#include "hif.h"
#include "mt6516.h"

/******************************************************************************
*                              C O N S T A N T S
*******************************************************************************
*/

/******************************************************************************
*                             D A T A   T Y P E S
*******************************************************************************
*/

/******************************************************************************
*                            P U B L I C   D A T A
*******************************************************************************
*/
HANDLE hSdio = INVALID_HANDLE_VALUE;

/******************************************************************************
*                           P R I V A T E   D A T A
*******************************************************************************
*/

/******************************************************************************
*                                 M A C R O S
*******************************************************************************
*/

/******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
*******************************************************************************
*/

/******************************************************************************
*                              F U N C T I O N S
*******************************************************************************
*/

BOOLEAN
platformBusInit (
    IN P_GLUE_INFO_T  prGlueInfo
    )
{
    ASSERT(prGlueInfo);

    if (INVALID_HANDLE_VALUE == hSdio) {
        hSdio = CreateFile(SDIO_DEVICE_NAME,
                           (GENERIC_READ | GENERIC_WRITE),
                           0,    //  devices must be opened w/exclusive-access
                           NULL, // no security attrs
                           OPEN_EXISTING, //  devices must use OPEN_EXISTING
                           FILE_ATTRIBUTE_NORMAL,    // normal
                           NULL  // hTemplate must be NULL for devices
                           );
    }

    if (INVALID_HANDLE_VALUE == hSdio) {
        ERRORLOG(("Fail to open SDIO device:%s\r\n", SDIO_DEVICE_NAME));
        return FALSE;
    }

    return TRUE;
}

VOID
platformBusDeinit (
    IN P_GLUE_INFO_T  prGlueInfo
    )
{
    ASSERT(prGlueInfo);

    if (INVALID_HANDLE_VALUE != hSdio) {
        CloseHandle(hSdio);
        hSdio = INVALID_HANDLE_VALUE;
    }
}

VOID
platformSetPowerState (
    IN P_GLUE_INFO_T  prGlueInfo,
    IN UINT_32 ePowerMode
    )
{
    CEDEVICE_POWER_STATE cPowerState;

    ASSERT(prGlueInfo);

    if (ParamDeviceStateD0 == ePowerMode) {
        cPowerState = D0;
    }
    else if (ParamDeviceStateD3 == ePowerMode) {
        cPowerState = D4;
    }
    else {
        ERRORLOG(("platformSetPowerState fail, unsupported ePowerMode:%x\n", ePowerMode));
        return;
    }

    DeviceIoControl(hSdio,
                    IOCTL_POWER_SET,
                    NULL,
                    0,
                    &cPowerState,
                    sizeof(cPowerState),
                    NULL,
                    NULL);
    return;
}


