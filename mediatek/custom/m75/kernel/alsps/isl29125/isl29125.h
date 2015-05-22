/******************************************************************************
	File		: isl29125.h

        Description     : Header file for the driver isl29125.c that services the ISL29125
                  	  RGB light sensor

	License		: GPLv2

	Copyright	: Intersil Corporation (c) 2013
*******************************************************************************/


#ifndef _ISL29125_H_
#define _ISL29125_H_

#define ISL29125_I2C_ADDR			0x44

/* Default device id */
#define ISL29125_DEV_ID				0x7D

/* ISL29125 REGISTER SET */

/**********************  DEVICE ID REGISTER ***********************************/
#define DEVICE_ID_REG				0x00

/**********************  CONFIGURATION 1 REGISTER *****************************/
#define CONFIG1_REG				0x01

/* Operating mode register field */
#define RGB_OP_MODE_POS				0
#define RGB_OP_MODE_CLEAR			~(0x7 << RGB_OP_MODE_POS)
#define RGB_OP_PWDN_MODE_SET			(0x0 << RGB_OP_MODE_POS)
#define RGB_OP_GREEN_MODE_SET			(0x1 << RGB_OP_MODE_POS)
#define RGB_OP_RED_MODE_SET			(0x2 << RGB_OP_MODE_POS)
#define RGB_OP_BLUE_MODE_SET			(0x3 << RGB_OP_MODE_POS)
#define RGB_OP_STANDBY_MODE_SET			(0x4 << RGB_OP_MODE_POS)
#define RGB_OP_GRB_MODE_SET			(0x5 << RGB_OP_MODE_POS)
#define RGB_OP_GR_MODE_SET			(0x6 << RGB_OP_MODE_POS)
#define RGB_OP_GB_MODE_SET			(0x7 << RGB_OP_MODE_POS)

/* Optical data sensing range field */
#define RGB_DATA_SENSE_RANGE_POS		3

#define RGB_SENSE_RANGE_4000_SET	(1 << RGB_DATA_SENSE_RANGE_POS)
#define RGB_SENSE_RANGE_330_SET		~RGB_SENSE_RANGE_4000_SET

/* ADC resolution field */
#define ADC_BIT_RESOLUTION_POS			4

#define ADC_RESOLUTION_12BIT_SET		(1 << ADC_BIT_RESOLUTION_POS)
#define ADC_RESOLUTION_16BIT_SET		~ADC_RESOLUTION_12BIT_SET 

/* RGB start sync field */
#define RGB_START_SYNC_AT_INTB_POS		5

#define ADC_START_AT_RISING_INTB		(1 << RGB_START_SYNC_AT_INTB_POS)
#define ADC_START_AT_I2C_WRITE			~ADC_START_AT_RISING_INTB 

/*************************  CONFIGURATION 2 REGISTER ***************************/
#define CONFIG2_REG				0x02

/* Active IR compensation field */
#define ACTIVE_IR_COMPENSATION_POS		0

#define ACTIVE_IR_COMPENSATION_CLEAR 		~(0x3F << ACTIVE_IR_COMPENSATION_POS)

/* IR compensation control field */
#define IR_COMPENSATION_CTRL_POS		7

#define IR_COMPENSATION_SET			(1 << IR_COMPENSATION_CTRL_POS)
#define IR_COMPENSATION_CLEAR			~IR_COMPENSATION_SET

/************************* CONFIGURATION 3 REGISTER ****************************/
#define CONFIG3_REG				0x03

/* Interrupt threshold assign field */
#define INTR_THRESHOLD_ASSIGN_POS		0

#define INTR_THRESHOLD_ASSIGN_CLEAR		~(0x3 << INTR_THRESHOLD_ASSIGN_POS)
#define INTR_THRESHOLD_ASSIGN_GREEN  		(0x1 << INTR_THRESHOLD_ASSIGN_POS) 
#define INTR_THRESHOLD_ASSIGN_RED    		(0x2 << INTR_THRESHOLD_ASSIGN_POS) 
#define INTR_THRESHOLD_ASSIGN_BLUE   		(0x3 << INTR_THRESHOLD_ASSIGN_POS) 

/* Interrupt persistency field */
#define INTR_PERSIST_CTRL_POS			2

#define INTR_PERSIST_CTRL_CLEAR			~(0x3 << INTR_PERSIST_CTRL_POS)
#define INTR_PERSIST_SET_1			 INTR_PERSIST_CTRL_CLEAR
#define INTR_PERSIST_SET_2			 (0x1 << INTR_PERSIST_CTRL_POS)
#define INTR_PERSIST_SET_4			 (0x2 << INTR_PERSIST_CTRL_POS)
#define INTR_PERSIST_SET_8			 (0x3 << INTR_PERSIST_CTRL_POS)

/* RGB conversion to interrupt field */
#define RGB_CONV_TO_INTB_CTRL_POS		4

#define RGB_CONV_TO_INTB_SET			(1 << RGB_CONV_TO_INTB_CTRL_POS)
#define RGB_CONV_TO_INTB_CLEAR			~RGB_CONV_TO_INTB_SET
		
/*************** LOW AND HIGH INTERRUPT THRESHOLD REGISTERS ********************/

#define LOW_THRESHOLD_LBYTE_REG			0x04
#define LOW_THRESHOLD_HBYTE_REG			0x05
#define HIGH_THRESHOLD_LBYTE_REG		0x06
#define HIGH_THRESHOLD_HBYTE_REG		0x07

/***************************** STATUS REGISTER *********************************/
#define STATUS_FLAGS_REG			0x08

#define RGBTHF_FLAG_POS				0
#define CONVF_FLAG_POS				1
#define BOUTF_FLAG_POS				2
#define GRBCF_FLAG_POS				4

/*************************** RGB Data Register *********************************/
#define GREEN_DATA_LBYTE_REG			0x09
#define GREEN_DATA_HBYTE_REG			0x0A
#define RED_DATA_LBYTE_REG			0x0B
#define RED_DATA_HBYTE_REG			0x0C
#define BLUE_DATA_LBYTE_REG			0x0D
#define BLUE_DATA_HBYTE_REG			0x0E

#define ABS_R	ABS_MISC
#define ABS_G	ABS_MISC+1
#define ABS_B	ABS_MISC+2

#define ISL29125_SYSFS_PERMISSIONS		00666

/************************** PLATFORM DATA *************************************/

 
#endif
