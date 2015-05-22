#ifndef _CFG_ROOTSIGN_FILE_H
#define _CFG_ROOTSIGN_FILE_H

typedef struct {
	unsigned int Array[128];
} File_Rootsign_Struct;

#define CFG_FILE_ROOTSIGN_REC_SIZE	sizeof(File_Rootsign_Struct)
#define CFG_FILE_ROOTSIGN_REC_TOTAL	1

#endif
