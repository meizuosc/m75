/*
 * es-d300.h  --  eS D300 Soc Audio driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ES_D300_H
#define _ES_D300_H

/* set data path - path type values */
#define PRIMARY		1
#define SECONDARY	2
#define TERITARY	3
#define FEIN		5
#define AUDIN1		6
#define AUDIN2		7
#define AUDIN3		8
#define AUDIN4		9
#define UITONE1		10
#define UITONE2		11
#define AECREF1		12
#define CSOUT		16
#define FEOUT1		17
#define FEOUT2		18
#define AUDOUT1		19
#define AUDOUT2		20
#define AUDOUT3		21
#define AUDOUT4		22

/* set data path - port number values */
#define PCM0	0
#define PCM1	1
#define PCM2	2
#define SBUS	5
#define PDMI0	6
#define PDMI1	7
#define PDMI2	8
#define PDMI3	9
#define ADC0	0x18
#define ADC1	0x0C
#define ADC2	0x0D
#define ADC3	0x0F
#define DAC0	0x15
#define DAC1	0x17

#define DATA_PATH(xpath, xport, xchan) ((xpath << 10) | (xport << 5) | (xchan))

#define MAX_PATH_TYPE	DATA_PATH(AUDOUT4, 0, 0)
#define MAX_INPUT_PORT	DATA_PATH(0, ADC3, 0)

#define OUT	1
#define IN	0

#define VP_RXCHMGR_MAX 8
#define VP_TXCHMGR_MAX 6
#define MM_RXCHMGR_MAX 6
#define MM_TXCHMGR_MAX 6
#define VP_MM_RXCHMGR_MAX 10
#define VP_MM_TXCHMGR_MAX 5

enum {
	ES_VP_None,
	ES_VP_RX_INIT, /* VP RX Initialized */
	ES_VP_TX_INIT, /* VP TX Initialized */
	ES_AZ_None,
	ES_AZ_RX_INIT, /* AZ RX Initialized */
	ES_AZ_TX_INIT, /* AZ TX Initialized */
};

/* Algorithm */
enum {
	None,
	VP,
	MM,
	VP_MM,
	AudioZoom,
	Passthru,
	Passthru_VP,
	Passthru_MM,
	Passthru_VP_MM,
	Passthru_AZ,
	Voicesense,
	ALGO_MAX,
};

/* Filter IDs*/
enum {
	Filter_Reserved = 0x0,

	Filter_RxChanMgr0 = 0x3,
	Filter_RxChanMgr1,
	Filter_RxChanMgr2,
	Filter_RxChanMgr3,
	Filter_RxChanMgr4,
	Filter_RxChanMgr5,
	Filter_RxChanMgr6,
	Filter_RxChanMgr7,
	Filter_RxChanMgr8,
	Filter_RxChanMgr9,

	Filter_TxChanMgr0,
	Filter_TxChanMgr1,
	Filter_TxChanMgr2,
	Filter_TxChanMgr3,
	Filter_TxChanMgr4,
	Filter_TxChanMgr5,

	Filter_VP = 0x13,
	Filter_AZ,
	Filter_MM,
	Filter_Passthru,
	Filter_Beep,
	Filter_Chime,
	Filter_VS,
};

/* Channel Designators */

enum {
	RXCHMGR0 = 0x0,
	RXCHMGR1,
	RXCHMGR2,
	RXCHMGR3,
	RXCHMGR4,
	RXCHMGR5,
	RXCHMGR6,
	RXCHMGR7,
	RXCHMGR8,
	RXCHMGR9,
	TXCHMGR0,
	TXCHMGR1,
	TXCHMGR2,
	TXCHMGR3,
	TXCHMGR4,
	TXCHMGR5,
	MAX_CHMGR,
};

/* Path IDs */
enum {
	ES300_PRI = 0x0,
	ES300_SEC,
	ES300_TER,
	ES300_FEIN,
	ES300_AECREF,
	ES300_AUDIN1,
	ES300_AUDIN2,

	ES300_UITONE1 = 0x9,
	ES300_UITONE2,
	ES300_UITONE3,
	ES300_UITONE4,
	ES300_PASSIN1,
	ES300_PASSIN2,
	ES300_PASSIN3,
	ES300_PASSIN4,
	ES300_FEIN2,

	ES300_CSOUT1 = 0x20,
	ES300_CSOUT2,
	ES300_FEOUT1,
	ES300_FEOUT2,
	ES300_AUDOUT1,
	ES300_AUDOUT2,
	ES300_MONOUT1 = 0x28,
	ES300_MONOUT2,
	ES300_MONOUT3,
	ES300_MONOUT4,
	ES300_PASSOUT1,
	ES300_PASSOUT2,
	ES300_PASSOUT3,
	ES300_PASSOUT4,

	/* Extra Path ID for MM */
	ES300_MM_MONOUT1,
	ES300_MM_MONOUT2,
};

/* Endpoints */

enum {
	/* Type: Voice Processing */
	vp_i0 = 0x0,
	vp_i1,
	vp_i2,
	vp_i3,
	vp_i4,
	vp_o0 = 0x0,
	vp_o1,

	/* Type: Multimedia */
	mm_i0 = 0x0,
	mm_i1,
	mm_o0 = 0x0,
	mm_o1,

	az_i0 = 0x0,
	az_i1,
	az_i2,
	az_o0 = 0x0,

	pass_i0 = 0x0,
	pass_i1,
	pass_i2,
	pass_i3,
	pass_o0 = 0x0,
	pass_o1,
	pass_o2,
	pass_o3,

	mix_i0 = 0x0,
	mix_i1,
	mix_o0 = 0x0,

	copy_i0 = 0x0,
	copy_o0 = 0x0,
	copy_o1,

	TxChMgr_i0 = 0,
	RxChMgr_o0 = 0,

	rate_i0 = 0x0,
	rate_o0 = 0x0,

	om_i0 = 0x0,
	om_i1,
	om_o0 = 0x0,
	om_o1,

	chime_i0 = 0x0,
	chime_o0 = 0x0,

	beep_i0 = 0x0,
	beep_i1,
	beep_o0 = 0x0,
	beep_o1,

	vs_i0 = 0x0,
};

struct es_ch_mgr_max {
	u8 rx;
	u8 tx;
};


/* TODO: Create a 2D array to map endpoint values
 * with path IDs. */

#define ES300_DATA_PATH(xport, xchan, xchandesg) \
	((xport << 9) | (xchan << 4) | (xchandesg))

#define ES300_ENDPOINT(xfilter, xdir, xepnum) \
	((xfilter << 4) | (xdir << 3) | (xepnum))

#define ES300_PATH_ID(xchandesg, xpath)	((xchandesg << 8) | (xpath))
#define ES300_RATE(filter, rate)	((rate << 8) | filter)
#define ES300_GROUP(filter, groupid)	((filter << 8) | (groupid & 0xf))
#define ES300_MAX_BASE_ROUTE		ALGO_MAX
/* Extract chmgr from Datapath */
#define ES_CHMGR_DATAPATH(datapath)	(datapath & 0x0F)
/* Extract chmgr from PathID */
#define ES_CHMGR_PATHID(pathid)	((pathid >> 8) & 0x0F)


/* Set Data Path NULL api command */
#define ES_SET_MUX_NULL		0xB05A3FF0
#define ES_SET_MUX_NULL_MASK	0xFFFFFFF0
extern int es_d300_add_snd_soc_dapm_controls(struct snd_soc_codec *codec);
extern int es_d300_add_snd_soc_controls(struct snd_soc_codec *codec);
extern int es_d300_add_snd_soc_route_map(struct snd_soc_codec *codec);
int es_d300_fill_cmdcache(struct snd_soc_codec *codec);
void es_d300_reset_cmdcache(void);

#endif /* _ES_D300_H */
