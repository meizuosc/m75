/*
 * es-d202.h  --  eS D202 Soc Audio driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ES_D202_H
#define _ES_D202_H

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
#define ADC1	11
#define ADC2	12
#define ADC3	13
#define DAC0	14
#define DAC1	15

#define DATA_PATH(xpath, xport, xchan) ((xpath << 10) | (xport << 5) | (xchan))

#define MAX_PATH_TYPE	DATA_PATH(AUDOUT4, 0, 0)
#define MAX_INPUT_PORT	DATA_PATH(0, ADC3, 0)

extern unsigned int escore_read(struct snd_soc_codec *codec, unsigned int reg);
extern int escore_write(struct snd_soc_codec *codec, unsigned int reg,
	unsigned int value);
extern int escore_read_reg_to_msg(struct escore_priv *escore, unsigned int reg,
	char *msg, int *msg_len);
extern int escore_write_reg_to_msg(struct escore_priv *escore, unsigned int reg,
	unsigned int value, char *msg, int *msg_len);
extern int escore_queue_msg_to_list(struct escore_priv *escore, char *msg,
	int msg_len);
extern int escore_write_msg_list(struct escore_priv *escore);
extern int escore_flush_msg_list(struct escore_priv *escore);
extern int es_d202_probe(struct snd_soc_codec *codec);
extern void es_d202_fill_cmdcache(struct snd_soc_codec *codec);
int es_d202_add_snd_soc_controls(struct snd_soc_codec *codec);
int es_d202_add_snd_soc_dapm_controls(struct snd_soc_codec *codec);
int es_d202_add_snd_soc_route_map(struct snd_soc_codec *codec);

#endif /* _ES_D202_H */
