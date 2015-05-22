#ifndef _CFG_CALIBRATION_FILE_H
#define _CFG_CALIBRATION_FILE_H

typedef struct {
	unsigned int Array[2048];
} File_Calibration_Struct;

#define CFG_FILE_CALIBRATION_REC_SIZE	sizeof(File_Calibration_Struct)
#define CFG_FILE_CALIBRATION_REC_TOTAL	1

typedef struct {
	unsigned int Array[128];
} File_Acceleration_Struct;

#define CFG_FILE_ACCELERATION_REC_SIZE	sizeof(File_Acceleration_Struct)
#define CFG_FILE_ACCELERATION_REC_TOTAL	1

typedef struct {
	unsigned int Array[128];
} File_Gyroscope_Struct;

#define CFG_FILE_GYROSCOPE_REC_SIZE	sizeof(File_Gyroscope_Struct)
#define CFG_FILE_GYROSCOPE_REC_TOTAL	1

typedef struct {
	unsigned int Array[128];
} File_Shopdemo_Struct;

#define CFG_FILE_SHOPDEMO_REC_SIZE	sizeof(File_Shopdemo_Struct)
#define CFG_FILE_SHOPDEMO_REC_TOTAL	1

#endif
