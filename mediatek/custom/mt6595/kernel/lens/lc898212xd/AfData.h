//********************************************************************************
//
//		LC89821x Initialize Data Table
//
//	    Program Name	: AfData.h
//		Design			: Rex.Tang
//		History			: First edition						2013.07.20 Rex.Tang
//
//		Description		: AF Initialize Table Defination
//********************************************************************************

#define   	WAIT				0xFF      	// Wait command

/*----------------------------------------------------------
		Initial data table
-----------------------------------------------------------*/
struct INIDATA {
	unsigned short	addr ;
	unsigned short	data ;
}IniData;	

const struct INIDATA Init_Table[] = {
	//Addr,	  Data

	//TDK_CL-ACT_ 212_SPtype_Ini_13011
	{ 0x0080, 0x34 }, 	// CLKSEL 1/1, CLKON
	{ 0x0081, 0x20 }, 	// AD 4Time

	{ 0x0084, 0xE0 }, 	// STBY   AD ON,DA ON,OP ON
	{ 0x0087, 0x05 }, 	// PIDSW OFF,AF ON,MS2 ON
	{ 0x00A4, 0x24 }, 	// Internal OSC Setup (No01=24.18MHz)
	{ 0x003A, 0x0000 }, // OFFSET Clear
	{ 0x0004, 0x0000 }, // RZ Clear(Target Value)
	{ 0x0002, 0x0000 }, // PIDZO Clear
	{ 0x0018, 0x0000 }, // MS1Z22 Clear(STMV Target Value)
//  { WAIT, 5 },  		// Wait 5 ms

	// Filter Setting: ST140325-1.h
	{ 0x0086, 0x0040 },
	{ 0x0040, 0x4030 }, 
	{ 0x0042, 0x7150 }, 
	{ 0x0044, 0x8F90 }, 
	{ 0x0046, 0x61B0 }, 
	{ 0x0048, 0x47F0 }, 
	{ 0x004A, 0x2030 },
	{ 0x004C, 0x4030 },
	{ 0x004E, 0x7FF0 },
	{ 0x0050, 0x04F0 },
	{ 0x0052, 0x7610 },
	{ 0x0054, 0x1210 },
	{ 0x0056, 0x0000 },
	{ 0x0058, 0x7FF0 },
	{ 0x005A, 0x0680 },
	{ 0x005C, 0x72F0 },
	{ 0x005E, 0x7F70 },
	{ 0x0060, 0x7ED0 },
	{ 0x0062, 0x7FF0 },
	{ 0x0064, 0x0000 },
	{ 0x0066, 0x0000 },
	{ 0x0068, 0x5130 },
	{ 0x006A, 0x72F0 },
	{ 0x006C, 0x8010 },
	{ 0x006E, 0x0000 },
	{ 0x0070, 0x0000 },
	{ 0x0072, 0x18E0 },
	{ 0x0074, 0x4E30 },
	{ 0x0030, 0x0000 },
	{ 0x0076, 0x0C50 },
	{ 0x0078, 0x4000 },
	{ WAIT, 5 },  		// Wait 5 ms

	{ 0x0086, 0x60 },	// DSSEL 1/16 INTON
	{ 0x0088, 0x70 },	// ANA1   Hall Bias:2mA Amp Gain: x100(Spring Type)
//	{ 0x0028, 0x8080 },	// Hall Offset/Bias	
//	{ 0x004C, 0x4000 },	// Loop Gain
	{ 0x0083, 0x2C },	// RZ OFF,STSW=ms2x2,MS1IN OFF,MS2IN=RZ,FFIN AFTER,DSW ag
	{ 0x0085, 0xC0 },	// AF filter,MS1 Clr
	{ WAIT, 1 },  		// Wait 1 ms
	
	{ 0x0084, 0xE3 },	// STBY   AD ON,DA ON,OP ON,DRMODE H,LNSTBB H
	{ 0x0097, 0x00 },	// DRVSEL
	{ 0x0098, 0x42 },	
	{ 0x0099, 0x00 },
	{ 0x009A, 0x00 },

//	{ WAIT, 5 },  		// Wait 5 ms
//	{ 0x0087, 0x85 }
};

