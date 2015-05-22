#ifndef _XHCI_MTK_POWER_H
#define _XHCI_MTK_POWER_H

#include <linux/usb.h>
#include <xhci.h>

void enableXhciAllPortPower(struct xhci_hcd *xhci);
void disableXhciAllPortPower(struct xhci_hcd *xhci);
void enableAllClockPower(bool is_reset);
void disableAllClockPower(void);
void disablePortClockPower(int port_index, int port_rev);
void enablePortClockPower(int port_index, int port_rev);

#ifdef CONFIG_USB_MTK_DUALMODE
void mtk_switch2host(void);
void mtk_switch2device(bool skip);
#endif

#endif
