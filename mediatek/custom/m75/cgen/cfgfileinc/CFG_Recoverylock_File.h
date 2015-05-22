#ifndef _CFG_RECOVERYLOCK_FILE_H
#define _CFG_RECOVERYLOCK_FILE_H

typedef struct {
	unsigned int Array[128];
} File_Recoverylock_Struct;

#define CFG_FILE_RECOVERYLOCK_REC_SIZE	sizeof(File_Recoverylock_Struct)
#define CFG_FILE_RECOVERYLOCK_REC_TOTAL	1

#endif
