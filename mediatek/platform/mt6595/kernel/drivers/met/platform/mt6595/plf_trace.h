#ifndef _PLF_TRACE_H_
#define _PLF_TRACE_H_

void ms_emi(unsigned long long timestamp, unsigned char cnt, unsigned int *value);
void ms_ttype(unsigned long long timestamp, unsigned char cnt, unsigned int *value);
void ms_smi(unsigned long long timestamp, unsigned char cnt, unsigned int *value);
void ms_smit(unsigned long long timestamp, unsigned char cnt, unsigned int *value);

void ms_th(unsigned long long timestamp, unsigned char cnt, unsigned int *value);
void ms_dramc(unsigned long long timestamp, unsigned char cnt, unsigned int *value);

#endif // _PLF_TRACE_H_
