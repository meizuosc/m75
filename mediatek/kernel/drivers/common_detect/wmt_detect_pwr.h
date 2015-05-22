#ifndef __WMT_DETECT_PWR_H_
#define __WMT_DETECT_PWR_H_


#define MAX_RTC_STABLE_TIME 100
#define MAX_LDO_STABLE_TIME 100
#define MAX_RST_STABLE_TIME 30
#define MAX_OFF_STABLE_TIME 10
#define MAX_ON_STABLE_TIME 30



extern int board_sdio_ctrl (unsigned int sdio_port_num, unsigned int on);
extern int wmt_detect_chip_pwr_ctrl (int on);
extern int wmt_detect_sdio_pwr_ctrl (int on);
extern int wmt_detect_read_ext_cmb_status (void);


#endif