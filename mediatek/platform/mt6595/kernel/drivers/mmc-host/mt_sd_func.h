#ifndef  MT_SD_FUNC_H
#define  MT_SD_FUNC_H

int sdio_stop_transfer(void);
int sdio_start_ot_transfer(void);
int autok_abort_action(void);
bool autok_is_vol_done(unsigned int voltage, int id);

#endif /* end of  MT_SD_FUNC_H */

