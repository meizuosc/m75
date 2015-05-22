/*******************************************************************************
 *
 * Filename:
 * ---------
 *   CFG_GPS_Default.h
 *
 * Project:
 * --------
 *   YuSu
 *
 * Description:
 * ------------
 *    give the default GPS config data.
 *
 * Author:
 * -------
 *  Mike Chang(MTK02063) 
 *
 *------------------------------------------------------------------------------
 * $Revision:$
 * $Modtime:$
 * $Log:$
 *
 * 06 24 2010 yunchang.chang
 * [ALPS00002677][Need Patch] [Volunteer Patch] ALPS.10X.W10.26 Volunteer patch for GPS customization use NVRam 
 * .
 *
 *******************************************************************************/
#ifndef _CFG_GPS_D_H
#define _CFG_GPS_D_H
ap_nvram_gps_config_struct stGPSConfigDefault =
{
    /* if chip detector say it's not 3332 use /dev/stpgps,else use /ttyMT1 */
    {'/','d','e','v','/','s','t','p','g','p','s',0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0},
    /* 0:s/w, 1:none, 2:h/w */
    1,
    
    /* 26MHz */
    26000000,
    /* default is 0ppm, by chip definetion. 6620 is 500, else 2000 */
	0,
    /* 0:16.368MHz TCXO */
    0xFF,
    /* 0:mixer-in, 1:internal-LNA, 6572/6582 dsp hardcode set this item to 1, only for 3332 one binary */
    0,
    /* sbas:0:none */
    0,
    0,
    0,
    0,
    0
};
#endif /* _CFG_GPS_D_H */
