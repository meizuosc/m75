#ifndef _TRACE_H_
#define _TRACE_H_

#define FMT1	",%x\n"
#define FMT2	",%x,%x\n"
#define FMT3	",%x,%x,%x\n"
#define FMT4	",%x,%x,%x,%x\n"
#define FMT5	",%x,%x,%x,%x,%x\n"
#define FMT6	",%x,%x,%x,%x,%x,%x\n"
#define FMT7	",%x,%x,%x,%x,%x,%x,%x\n"
#define FMT8	",%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT9	",%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT10	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT11	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT12	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT13	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT14	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT15	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT16	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT17	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT18	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT19	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT20	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT21	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT22	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT23	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT24	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT25	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT26	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT27	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT28	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT29	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT30	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT31	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT32	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT33	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT34	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT35	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT36	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT37	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT38	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT39	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT40	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT41	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT42	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT43	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT44	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT45	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT46	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT47	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT48	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT49	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT50	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT51	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT52	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT53	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT54	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT55	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT56	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT57	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT58	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT59	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT60	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT66	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define FMT82	",%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"

#define VAL1	,value[0]
#define VAL2	,value[0],value[1]
#define VAL3	,value[0],value[1],value[2]
#define VAL4	,value[0],value[1],value[2],value[3]
#define VAL5	,value[0],value[1],value[2],value[3],value[4]
#define VAL6	,value[0],value[1],value[2],value[3],value[4] \
		,value[5]
#define VAL7	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6]
#define VAL8	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7]
#define VAL9	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8]
#define VAL10	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9]
#define VAL11	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10]
#define VAL12	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11]
#define VAL13	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12]
#define VAL14	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13]
#define VAL15	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14]
#define VAL16	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15]
#define VAL17	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16]
#define VAL18	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16],value[17]
#define VAL19	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16],value[17],value[18]
#define VAL20	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16],value[17],value[18],value[19]
#define VAL21	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16],value[17],value[18],value[19] \
		,value[20]
#define VAL22	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16],value[17],value[18],value[19] \
		,value[20],value[21]
#define VAL23	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16],value[17],value[18],value[19] \
		,value[20],value[21],value[22]
#define VAL24	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16],value[17],value[18],value[19] \
		,value[20],value[21],value[22],value[23]
#define VAL25	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16],value[17],value[18],value[19] \
		,value[20],value[21],value[22],value[23],value[24]
#define VAL26	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16],value[17],value[18],value[19] \
		,value[20],value[21],value[22],value[23],value[24] \
		,value[25]
#define VAL27	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16],value[17],value[18],value[19] \
		,value[20],value[21],value[22],value[23],value[24] \
		,value[25],value[26]
#define VAL28	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16],value[17],value[18],value[19] \
		,value[20],value[21],value[22],value[23],value[24] \
		,value[25],value[26],value[27]
#define VAL29	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16],value[17],value[18],value[19] \
		,value[20],value[21],value[22],value[23],value[24] \
		,value[25],value[26],value[27],value[28]
#define VAL30	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16],value[17],value[18],value[19] \
		,value[20],value[21],value[22],value[23],value[24] \
		,value[25],value[26],value[27],value[28],value[29]
#define VAL31	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16],value[17],value[18],value[19] \
		,value[20],value[21],value[22],value[23],value[24] \
		,value[25],value[26],value[27],value[28],value[29] \
		,value[30]
#define VAL32	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16],value[17],value[18],value[19] \
		,value[20],value[21],value[22],value[23],value[24] \
		,value[25],value[26],value[27],value[28],value[29] \
		,value[30],value[31]
#define VAL33	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16],value[17],value[18],value[19] \
		,value[20],value[21],value[22],value[23],value[24] \
		,value[25],value[26],value[27],value[28],value[29] \
		,value[30],value[31],value[32]
#define VAL34	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16],value[17],value[18],value[19] \
		,value[20],value[21],value[22],value[23],value[24] \
		,value[25],value[26],value[27],value[28],value[29] \
		,value[30],value[31],value[32],value[33]
#define VAL35	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16],value[17],value[18],value[19] \
		,value[20],value[21],value[22],value[23],value[24] \
		,value[25],value[26],value[27],value[28],value[29] \
		,value[30],value[31],value[32],value[33],value[34]
#define VAL36	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16],value[17],value[18],value[19] \
		,value[20],value[21],value[22],value[23],value[24] \
		,value[25],value[26],value[27],value[28],value[29] \
		,value[30],value[31],value[32],value[33],value[34] \
		,value[35]
#define VAL37	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16],value[17],value[18],value[19] \
		,value[20],value[21],value[22],value[23],value[24] \
		,value[25],value[26],value[27],value[28],value[29] \
		,value[30],value[31],value[32],value[33],value[34] \
		,value[35],value[36]
#define VAL38	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16],value[17],value[18],value[19] \
		,value[20],value[21],value[22],value[23],value[24] \
		,value[25],value[26],value[27],value[28],value[29] \
		,value[30],value[31],value[32],value[33],value[34] \
		,value[35],value[36],value[37]
#define VAL39	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16],value[17],value[18],value[19] \
		,value[20],value[21],value[22],value[23],value[24] \
		,value[25],value[26],value[27],value[28],value[29] \
		,value[30],value[31],value[32],value[33],value[34] \
		,value[35],value[36],value[37],value[38]
#define VAL40	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16],value[17],value[18],value[19] \
		,value[20],value[21],value[22],value[23],value[24] \
		,value[25],value[26],value[27],value[28],value[29] \
		,value[30],value[31],value[32],value[33],value[34] \
		,value[35],value[36],value[37],value[38],value[39]
#define VAL41	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16],value[17],value[18],value[19] \
		,value[20],value[21],value[22],value[23],value[24] \
		,value[25],value[26],value[27],value[28],value[29] \
		,value[30],value[31],value[32],value[33],value[34] \
		,value[35],value[36],value[37],value[38],value[39] \
		,value[40]
#define VAL42	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16],value[17],value[18],value[19] \
		,value[20],value[21],value[22],value[23],value[24] \
		,value[25],value[26],value[27],value[28],value[29] \
		,value[30],value[31],value[32],value[33],value[34] \
		,value[35],value[36],value[37],value[38],value[39] \
		,value[40],value[41]
#define VAL43	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16],value[17],value[18],value[19] \
		,value[20],value[21],value[22],value[23],value[24] \
		,value[25],value[26],value[27],value[28],value[29] \
		,value[30],value[31],value[32],value[33],value[34] \
		,value[35],value[36],value[37],value[38],value[39] \
		,value[40],value[41],value[42]
#define VAL44	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16],value[17],value[18],value[19] \
		,value[20],value[21],value[22],value[23],value[24] \
		,value[25],value[26],value[27],value[28],value[29] \
		,value[30],value[31],value[32],value[33],value[34] \
		,value[35],value[36],value[37],value[38],value[39] \
		,value[40],value[41],value[42],value[43]
#define VAL45	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16],value[17],value[18],value[19] \
		,value[20],value[21],value[22],value[23],value[24] \
		,value[25],value[26],value[27],value[28],value[29] \
		,value[30],value[31],value[32],value[33],value[34] \
		,value[35],value[36],value[37],value[38],value[39] \
		,value[40],value[41],value[42],value[43],value[44]
#define VAL46	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16],value[17],value[18],value[19] \
		,value[20],value[21],value[22],value[23],value[24] \
		,value[25],value[26],value[27],value[28],value[29] \
		,value[30],value[31],value[32],value[33],value[34] \
		,value[35],value[36],value[37],value[38],value[39] \
		,value[40],value[41],value[42],value[43],value[44] \
		,value[45]
#define VAL47	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16],value[17],value[18],value[19] \
		,value[20],value[21],value[22],value[23],value[24] \
		,value[25],value[26],value[27],value[28],value[29] \
		,value[30],value[31],value[32],value[33],value[34] \
		,value[35],value[36],value[37],value[38],value[39] \
		,value[40],value[41],value[42],value[43],value[44] \
		,value[45],value[46]
#define VAL48	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16],value[17],value[18],value[19] \
		,value[20],value[21],value[22],value[23],value[24] \
		,value[25],value[26],value[27],value[28],value[29] \
		,value[30],value[31],value[32],value[33],value[34] \
		,value[35],value[36],value[37],value[38],value[39] \
		,value[40],value[41],value[42],value[43],value[44] \
		,value[45],value[46],value[47]
#define VAL49	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16],value[17],value[18],value[19] \
		,value[20],value[21],value[22],value[23],value[24] \
		,value[25],value[26],value[27],value[28],value[29] \
		,value[30],value[31],value[32],value[33],value[34] \
		,value[35],value[36],value[37],value[38],value[39] \
		,value[40],value[41],value[42],value[43],value[44] \
		,value[45],value[46],value[47],value[48]
#define VAL50	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16],value[17],value[18],value[19] \
		,value[20],value[21],value[22],value[23],value[24] \
		,value[25],value[26],value[27],value[28],value[29] \
		,value[30],value[31],value[32],value[33],value[34] \
		,value[35],value[36],value[37],value[38],value[39] \
		,value[40],value[41],value[42],value[43],value[44] \
		,value[45],value[46],value[47],value[48],value[49]
#define VAL51	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16],value[17],value[18],value[19] \
		,value[20],value[21],value[22],value[23],value[24] \
		,value[25],value[26],value[27],value[28],value[29] \
		,value[30],value[31],value[32],value[33],value[34] \
		,value[35],value[36],value[37],value[38],value[39] \
		,value[40],value[41],value[42],value[43],value[44] \
		,value[45],value[46],value[47],value[48],value[49] \
		,value[50]
#define VAL52	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16],value[17],value[18],value[19] \
		,value[20],value[21],value[22],value[23],value[24] \
		,value[25],value[26],value[27],value[28],value[29] \
		,value[30],value[31],value[32],value[33],value[34] \
		,value[35],value[36],value[37],value[38],value[39] \
		,value[40],value[41],value[42],value[43],value[44] \
		,value[45],value[46],value[47],value[48],value[49] \
		,value[50],value[51]
#define VAL53	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16],value[17],value[18],value[19] \
		,value[20],value[21],value[22],value[23],value[24] \
		,value[25],value[26],value[27],value[28],value[29] \
		,value[30],value[31],value[32],value[33],value[34] \
		,value[35],value[36],value[37],value[38],value[39] \
		,value[40],value[41],value[42],value[43],value[44] \
		,value[45],value[46],value[47],value[48],value[49] \
		,value[50],value[51],value[52]
#define VAL54	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16],value[17],value[18],value[19] \
		,value[20],value[21],value[22],value[23],value[24] \
		,value[25],value[26],value[27],value[28],value[29] \
		,value[30],value[31],value[32],value[33],value[34] \
		,value[35],value[36],value[37],value[38],value[39] \
		,value[40],value[41],value[42],value[43],value[44] \
		,value[45],value[46],value[47],value[48],value[49] \
		,value[50],value[51],value[52],value[53]
#define VAL55	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16],value[17],value[18],value[19] \
		,value[20],value[21],value[22],value[23],value[24] \
		,value[25],value[26],value[27],value[28],value[29] \
		,value[30],value[31],value[32],value[33],value[34] \
		,value[35],value[36],value[37],value[38],value[39] \
		,value[40],value[41],value[42],value[43],value[44] \
		,value[45],value[46],value[47],value[48],value[49] \
		,value[50],value[51],value[52],value[53],value[54]
#define VAL56	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16],value[17],value[18],value[19] \
		,value[20],value[21],value[22],value[23],value[24] \
		,value[25],value[26],value[27],value[28],value[29] \
		,value[30],value[31],value[32],value[33],value[34] \
		,value[35],value[36],value[37],value[38],value[39] \
		,value[40],value[41],value[42],value[43],value[44] \
		,value[45],value[46],value[47],value[48],value[49] \
		,value[50],value[51],value[52],value[53],value[54] \
		,value[55]
#define VAL57	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16],value[17],value[18],value[19] \
		,value[20],value[21],value[22],value[23],value[24] \
		,value[25],value[26],value[27],value[28],value[29] \
		,value[30],value[31],value[32],value[33],value[34] \
		,value[35],value[36],value[37],value[38],value[39] \
		,value[40],value[41],value[42],value[43],value[44] \
		,value[45],value[46],value[47],value[48],value[49] \
		,value[50],value[51],value[52],value[53],value[54] \
		,value[55],value[56]
#define VAL58	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16],value[17],value[18],value[19] \
		,value[20],value[21],value[22],value[23],value[24] \
		,value[25],value[26],value[27],value[28],value[29] \
		,value[30],value[31],value[32],value[33],value[34] \
		,value[35],value[36],value[37],value[38],value[39] \
		,value[40],value[41],value[42],value[43],value[44] \
		,value[45],value[46],value[47],value[48],value[49] \
		,value[50],value[51],value[52],value[53],value[54] \
		,value[55],value[56],value[57]
#define VAL59	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16],value[17],value[18],value[19] \
		,value[20],value[21],value[22],value[23],value[24] \
		,value[25],value[26],value[27],value[28],value[29] \
		,value[30],value[31],value[32],value[33],value[34] \
		,value[35],value[36],value[37],value[38],value[39] \
		,value[40],value[41],value[42],value[43],value[44] \
		,value[45],value[46],value[47],value[48],value[49] \
		,value[50],value[51],value[52],value[53],value[54] \
		,value[55],value[56],value[57],value[58]
#define VAL60	,value[0],value[1],value[2],value[3],value[4] \
		,value[5],value[6],value[7],value[8],value[9] \
		,value[10],value[11],value[12],value[13],value[14] \
		,value[15],value[16],value[17],value[18],value[19] \
		,value[20],value[21],value[22],value[23],value[24] \
		,value[25],value[26],value[27],value[28],value[29] \
		,value[30],value[31],value[32],value[33],value[34] \
		,value[35],value[36],value[37],value[38],value[39] \
		,value[40],value[41],value[42],value[43],value[44] \
		,value[45],value[46],value[47],value[48],value[49] \
		,value[50],value[51],value[52],value[53],value[54] \
		,value[55],value[56],value[57],value[58],value[59]
#define VAL66	,value[0],value[1],value[2],value[3],value[4],value[5],value[6],value[7],value[8],value[9]\
                ,value[10],value[11],value[12],value[13],value[14],value[15],value[16],value[17],value[18],value[19]\
                ,value[20],value[21],value[22],value[23],value[24],value[25],value[26],value[27],value[28],value[29]\
		,value[30],value[31],value[32],value[33],value[34],value[35],value[36],value[37],value[38],value[39]\
		,value[40],value[41],value[42],value[43],value[44],value[45],value[46],value[47],value[48],value[49]\
		,value[50],value[51],value[52],value[53],value[54],value[55],value[56],value[57],value[58],value[59]\
		,value[60],value[61],value[62],value[63],value[64],value[65]
#define VAL82	,value[0],value[1],value[2],value[3],value[4],value[5],value[6],value[7],value[8],value[9]\
                ,value[10],value[11],value[12],value[13],value[14],value[15],value[16],value[17],value[18],value[19]\
                ,value[20],value[21],value[22],value[23],value[24],value[25],value[26],value[27],value[28],value[29]\
		,value[30],value[31],value[32],value[33],value[34],value[35],value[36],value[37],value[38],value[39]\
		,value[40],value[41],value[42],value[43],value[44],value[45],value[46],value[47],value[48],value[49]\
		,value[50],value[51],value[52],value[53],value[54],value[55],value[56],value[57],value[58],value[59]\
		,value[60],value[61],value[62],value[63],value[64],value[65],value[66],value[67],value[68],value[69]\
		,value[70],value[71],value[72],value[73],value[74],value[75],value[76],value[77],value[78],value[79]\
		,value[80],value[81]

extern void (*mp_cp_ptr)(unsigned long long timestamp,
	       struct task_struct *task,
	       unsigned long program_counter,
	       unsigned long dcookie,
	       unsigned long offset,
	       unsigned char cnt, unsigned int *value);

#define MP_FMT1	"%x\n"
#define MP_FMT2	"%x,%x\n"
#define MP_FMT3	"%x,%x,%x\n"
#define MP_FMT4	"%x,%x,%x,%x\n"
#define MP_FMT5	"%x,%x,%x,%x,%x\n"
#define MP_FMT6	"%x,%x,%x,%x,%x,%x\n"
#define MP_FMT7	"%x,%x,%x,%x,%x,%x,%x\n"
#define MP_FMT8	"%x,%x,%x,%x,%x,%x,%x,%x\n"
#define MP_FMT9	"%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define MP_FMT10 "%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define MP_FMT11 "%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define MP_FMT12 "%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n"
#define MP_VAL1	value[0]
#define MP_VAL2	value[0],value[1]
#define MP_VAL3	value[0],value[1],value[2]
#define MP_VAL4	value[0],value[1],value[2],value[3]
#define MP_VAL5	value[0],value[1],value[2],value[3],value[4]
#define MP_VAL6	value[0],value[1],value[2],value[3],value[4],value[5]
#define MP_VAL7	value[0],value[1],value[2],value[3],value[4],value[5],value[6]
#define MP_VAL8	value[0],value[1],value[2],value[3],value[4],value[5],value[6],value[7]
#define MP_VAL9	value[0],value[1],value[2],value[3],value[4],value[5],value[6],value[7],value[8]
#define MP_VAL10 value[0],value[1],value[2],value[3],value[4],value[5],value[6],value[7],value[8],value[9]
#define MP_VAL11 value[0],value[1],value[2],value[3],value[4],value[5],value[6],value[7],value[8],value[9],value[10]
#define MP_VAL12 value[0],value[1],value[2],value[3],value[4],value[5],value[6],value[7],value[8],value[9],value[10],value[11]


#endif // _TRACE_H_
