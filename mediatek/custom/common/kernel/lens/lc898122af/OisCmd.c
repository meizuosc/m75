//********************************************************************************
//
//		<< LC898122 Evaluation Soft >>
//	    Program Name	: OisCmd.c
//		Design			: Y.Yamada
//		History			: First edition						2009.07.31 Y.Tashita
//********************************************************************************
//**************************
//	Include Header File		
//**************************
#define		OISCMD

//#include	"Main.h"
//#include	"Cmd.h"
#include	"Ois.h"
#include	"OisDef.h"

//**************************
//	Local Function Prottype	
//**************************
void			MesFil( unsigned char ) ;					// Measure Filter Setting
#ifdef	MODULE_CALIBRATION
 #ifndef	HALLADJ_HW
 void			LopIni( unsigned char ) ;					// Loop Gain Initialize
 #endif
 void			LopPar( unsigned char ) ;					// Loop Gain Parameter initialize
 #ifndef	HALLADJ_HW
  void			LopSin( unsigned char, unsigned char ) ;	// Loop Gain Sin Wave Output
  unsigned char	LopAdj( unsigned char ) ;					// Loop Gain Adjust
  void			LopMes( void ) ;							// Loop Gain Measure
 #endif
#endif
#ifndef	HALLADJ_HW
unsigned long	GinMes( unsigned char ) ;					// Measure Result Getting
#endif
void			GyrCon( unsigned char ) ;					// Gyro Filter Control
short			GenMes( unsigned short, unsigned char ) ;	// General Measure
#ifndef	HALLADJ_HW
// unsigned long	TnePtp( unsigned char, unsigned char ) ;	// Get Hall Peak to Peak Values
// unsigned char	TneCen( unsigned char, UnDwdVal ) ;			// Tuning Hall Center
 unsigned long	TneOff( UnDwdVal, unsigned char ) ;			// Hall Offset Tuning
 unsigned long	TneBia( UnDwdVal, unsigned char ) ;			// Hall Bias Tuning
#endif

void 			StbOnn( void ) ;							// Servo ON Slope mode

void			SetSineWave(   unsigned char , unsigned char );
void			StartSineWave( void );
void			StopSineWave(  void );

void			SetMeasFil(  unsigned char );
void			ClrMeasFil( void );



//**************************
//	define					
//**************************
#define		MES_XG1			0								// LXG1 Measure Mode
#define		MES_XG2			1								// LXG2 Measure Mode

#define		HALL_ADJ		0
#define		LOOPGAIN		1
#define		THROUGH			2
#define		NOISE			3

// Measure Mode

 #define		TNE 			80								// Waiting Time For Movement
#ifdef	HALLADJ_HW

 #define     __MEASURE_LOOPGAIN      0x00
 #define     __MEASURE_BIASOFFSET    0x01

#else

 /******* Hall calibration Type 1 *******/
 #define		MARJIN			0x0300							// Marjin
 #define		BIAS_ADJ_BORDER	0x1998							// HALL_MAX_GAP < BIAS_ADJ_BORDER < HALL_MIN_GAP(80%)

 #define		HALL_MAX_GAP	BIAS_ADJ_BORDER - MARJIN
 #define		HALL_MIN_GAP	BIAS_ADJ_BORDER + MARJIN
 /***************************************/
 
 #define		BIAS_LIMIT		0xFFFF							// HALL BIAS LIMIT
 #define		OFFSET_DIV		2								// Divide Difference For Offset Step
 #define		TIME_OUT		40								// Time Out Count

 /******* Hall calibration Type 2 *******/
 #define		MARGIN			0x0300							// Margin

 #define		BIAS_ADJ_OVER	0xD998							// 85%
 #define		BIAS_ADJ_RANGE	0xCCCC							// 80%
 #define		BIAS_ADJ_SKIP	0xBFFF							// 75%
 #define		HALL_MAX_RANGE	BIAS_ADJ_RANGE + MARGIN
 #define		HALL_MIN_RANGE	BIAS_ADJ_RANGE - MARGIN

 #define		DECRE_CAL		0x0100							// decrease value
 /***************************************/
#endif

#ifdef H1COEF_CHANGER
 #ifdef	CORRECT_1DEG
  #define		MAXLMT		0x40400000				// 3.0
  #define		MINLMT		0x3FE66666				// 1.8
  #define		CHGCOEF		0xBA195555				// 
 #else
  #define		MAXLMT		0x40000000				// 2.0
  #define		MINLMT		0x3F8CCCCD				// 1.1
  #define		CHGCOEF		0xBA4C71C7				// 
 #endif
  #define		MINLMT_MOV	0x00000000				// 0.0
  #define		CHGCOEF_MOV	0xB9700000
#endif

//**************************
//	Global Variable			
//**************************
#ifdef	HALLADJ_HW
 unsigned char UcAdjBsy;

#else
 unsigned short	UsStpSiz	= 0 ;							// Bias Step Size
 unsigned short	UsErrBia, UsErrOfs ;
#endif



//**************************
//	Const					
//**************************
// gxzoom Setting Value
#define		ZOOMTBL	16
const unsigned long	ClGyxZom[ ZOOMTBL ]	= {
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000
	} ;

// gyzoom Setting Value
const unsigned long	ClGyyZom[ ZOOMTBL ]	= {
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000,
		0x3F800000
	} ;

// DI Coefficient Setting Value
#define		COEFTBL	7
const unsigned long	ClDiCof[ COEFTBL ]	= {
		DIFIL_S2,		/* 0 */
		DIFIL_S2,		/* 1 */
		DIFIL_S2,		/* 2 */
		DIFIL_S2,		/* 3 */
		DIFIL_S2,		/* 4 */
		DIFIL_S2,		/* 5 */
		DIFIL_S2		/* 6 */
	} ;

//********************************************************************************
// Function Name 	: TneRun
// Retun Value		: Hall Tuning SUCCESS or FAILURE
// Argment Value	: NON
// Explanation		: Hall System Auto Adjustment Function
// History			: First edition 						2009.12.1 YS.Kim
//********************************************************************************
unsigned short	TneRun( void )
{
	unsigned char	UcHlySts, UcHlxSts, UcAtxSts, UcAtySts ;
	unsigned short	UsFinSts , UsOscSts ; 								// Final Adjustment state
	unsigned char	UcDrvMod ;
#ifndef	HALLADJ_HW
	UnDwdVal		StTneVal ;
#endif

#ifdef	USE_EXTCLK_ALL	// 24MHz
	UsOscSts	= EXE_END ;
#else
 #ifdef	MODULE_CALIBRATION
	/* OSC adjustment */
	UsOscSts	= OscAdj() ;
 #else
	UsOscSts	= EXE_END ;
 #endif
#endif
	
	UcDrvMod = UcPwmMod ;
	if( UcDrvMod == PWMMOD_CVL )
	{
		DrvPwmSw( Mpwm ) ;		/* PWM mode */
	}
	
#ifdef	HALLADJ_HW
	UcHlySts = BiasOffsetAdj( Y_DIR , 0 ) ;
	WitTim( TNE ) ;
	UcHlxSts = BiasOffsetAdj( X_DIR , 0 ) ;
	WitTim( TNE ) ;
	UcHlySts = BiasOffsetAdj( Y_DIR , 1 ) ;
	WitTim( TNE ) ;
	UcHlxSts = BiasOffsetAdj( X_DIR , 1 ) ;

	SrvCon( Y_DIR, OFF ) ;
	SrvCon( X_DIR, OFF ) ;
	
	if( UcDrvMod == PWMMOD_CVL )
	{
		DrvPwmSw( Mlnp ) ;		/* PWM mode */
	}
	
  #ifdef	NEUTRAL_CENTER
	TneHvc();
  #endif	//NEUTRAL_CENTER	
#else
//	StbOnnN( OFF , ON ) ;				/* Y OFF, X ON */
	WitTim( TNE ) ;

	StTneVal.UlDwdVal	= TnePtp( Y_DIR , PTP_BEFORE ) ;
//	UcHlySts	= TneCen( Y_DIR, StTneVal ) ;
	UcHlySts	= TneCen( Y2_DIR, StTneVal ) ;
	
	StbOnnN( ON , OFF ) ;				/* Y ON, X OFF */
	WitTim( TNE ) ;

	StTneVal.UlDwdVal	= TnePtp( X_DIR , PTP_BEFORE ) ;
//	UcHlxSts	= TneCen( X_DIR, StTneVal ) ;
	UcHlxSts	= TneCen( X2_DIR, StTneVal ) ;

	StbOnnN( OFF , ON ) ;				/* Y OFF, X ON */
	WitTim( TNE ) ;

	StTneVal.UlDwdVal	= TnePtp( Y_DIR , PTP_AFTER ) ;
//	UcHlySts	= TneCen( Y_DIR, StTneVal ) ;
	UcHlySts	= TneCen( Y2_DIR, StTneVal ) ;

	StbOnnN( ON , OFF ) ;				/* Y ON, X OFF */
	WitTim( TNE ) ;

	StTneVal.UlDwdVal	= TnePtp( X_DIR , PTP_AFTER ) ;
//	UcHlxSts	= TneCen( X_DIR, StTneVal ) ;
	UcHlxSts	= TneCen( X2_DIR, StTneVal ) ;

	SrvCon( Y_DIR, OFF ) ;
	SrvCon( X_DIR, OFF ) ;
	
	if( UcDrvMod == PWMMOD_CVL )
	{
		DrvPwmSw( Mlnp ) ;		/* PWM mode */
	}
	
  #ifdef	NEUTRAL_CENTER
	TneHvc();
  #endif	//NEUTRAL_CENTER
#endif
	

	WitTim( TNE ) ;

	RamAccFixMod( ON ) ;							// Fix mode
	
	StAdjPar.StHalAdj.UsAdxOff = (unsigned short)((unsigned long)0x00010000 - (unsigned long)StAdjPar.StHalAdj.UsHlxCna ) ;
	StAdjPar.StHalAdj.UsAdyOff = (unsigned short)((unsigned long)0x00010000 - (unsigned long)StAdjPar.StHalAdj.UsHlyCna ) ;
	
	RamWriteA( OFF0Z,  StAdjPar.StHalAdj.UsAdxOff ) ;	// 0x1450
	RamWriteA( OFF1Z,  StAdjPar.StHalAdj.UsAdyOff ) ;	// 0x14D0

	RamReadA( DAXHLO, &StAdjPar.StHalAdj.UsHlxOff ) ;		// 0x1479
	RamReadA( DAXHLB, &StAdjPar.StHalAdj.UsHlxGan ) ;		// 0x147A
	RamReadA( DAYHLO, &StAdjPar.StHalAdj.UsHlyOff ) ;		// 0x14F9
	RamReadA( DAYHLB, &StAdjPar.StHalAdj.UsHlyGan ) ;		// 0x14FA
	RamReadA( OFF0Z, &StAdjPar.StHalAdj.UsAdxOff ) ;		// 0x1450
	RamReadA( OFF1Z, &StAdjPar.StHalAdj.UsAdyOff ) ;		// 0x14D0
	
	RamAccFixMod( OFF ) ;							// Float mode
	
	StbOnn() ;											// Slope Mode

	
	WitTim( TNE ) ;

#ifdef	MODULE_CALIBRATION
	// X Loop Gain Adjust
	UcAtxSts	= LopGan( X_DIR ) ;


	// Y Loop Gain Adjust
	UcAtySts	= LopGan( Y_DIR ) ;
#else		//  default value
	RamAccFixMod( ON ) ;								// Fix mode
	RamReadA( sxg, &StAdjPar.StLopGan.UsLxgVal ) ;		// 0x10D3
	RamReadA( syg, &StAdjPar.StLopGan.UsLygVal ) ;		// 0x11D3
	RamAccFixMod( OFF ) ;								// Float mode
	UcAtxSts	= EXE_END ;
	UcAtySts	= EXE_END ;
#endif
	

	TneGvc() ;


	UsFinSts	= (unsigned short)( UcHlxSts - EXE_END ) + (unsigned short)( UcHlySts - EXE_END ) + (unsigned short)( UcAtxSts - EXE_END ) + (unsigned short)( UcAtySts - EXE_END ) + ( UsOscSts - (unsigned short)EXE_END ) + (unsigned short)EXE_END ;


	return( UsFinSts ) ;
}


#ifndef	HALLADJ_HW

//********************************************************************************
// Function Name 	: TnePtp
// Retun Value		: Hall Top & Bottom Gaps
// Argment Value	: X,Y Direction, Adjust Before After Parameter
// Explanation		: Measuring Hall Paek To Peak
// History			: First edition 						2009.12.1 YS.Kim
//********************************************************************************
 
unsigned long	TnePtp ( unsigned char	UcDirSel, unsigned char	UcBfrAft )
{
	UnDwdVal		StTneVal ;

	MesFil( THROUGH ) ;					// 測定用フィルターを設定する。


	if ( !UcDirSel ) {
		RamWrite32A( sxsin , HALL_H_VAL );		// 0x10D5
		SetSinWavePara( 0x0A , XHALWAVE ); 
	}else{
	    RamWrite32A( sysin , HALL_H_VAL ); 		// 0x11D5
		SetSinWavePara( 0x0A , YHALWAVE ); 
	}

	if ( !UcDirSel ) {					// AXIS X
		RegWriteA( WC_MES1ADD0,  ( unsigned char )AD0Z ) ;							/* 0x0194	*/
		RegWriteA( WC_MES1ADD1,  ( unsigned char )(( AD0Z >> 8 ) & 0x0001 ) ) ;		/* 0x0195	*/
	} else {							// AXIS Y
		RegWriteA( WC_MES1ADD0,  ( unsigned char )AD1Z ) ;							/* 0x0194	*/
		RegWriteA( WC_MES1ADD1,  ( unsigned char )(( AD1Z >> 8 ) & 0x0001 ) ) ;		/* 0x0195	*/
	}

	RegWriteA( WC_MESLOOP1	, 0x00 );			// 0x0193	CmMesLoop[15:8]
	RegWriteA( WC_MESLOOP0	, 0x01);			// 0x0192	CmMesLoop[7:0]
	
	RamWrite32A( msmean	, 0x3F800000 );			// 0x1230	1/CmMesLoop[15:0]
	
	RamWrite32A( MSMAX1, 	0x00000000 ) ;		// 0x1050
	RamWrite32A( MSMAX1AV, 	0x00000000 ) ;		// 0x1051
	RamWrite32A( MSMIN1, 	0x00000000 ) ;		// 0x1060
	RamWrite32A( MSMIN1AV, 	0x00000000 ) ;		// 0x1061
	
	RegWriteA( WC_MESABS, 0x00 ) ;				// 0x0198	none ABS
	BsyWit( WC_MESMODE, 0x02 ) ;				// 0x0190		Sine wave Measure

	RamAccFixMod( ON ) ;							// Fix mode
	
	RamReadA( MSMAX1AV, &StTneVal.StDwdVal.UsHigVal ) ;		// 0x1051
	RamReadA( MSMIN1AV, &StTneVal.StDwdVal.UsLowVal ) ;		// 0x1061

	RamAccFixMod( OFF ) ;							// Float mode

	if ( !UcDirSel ) {					// AXIS X
		SetSinWavePara( 0x00 , XHALWAVE ); 	/* STOP */
	}else{
		SetSinWavePara( 0x00 , YHALWAVE ); 	/* STOP */
	}

	if( UcBfrAft == 0 ) {
		if( UcDirSel == X_DIR ) {
			StAdjPar.StHalAdj.UsHlxCen	= ( ( signed short )StTneVal.StDwdVal.UsHigVal + ( signed short )StTneVal.StDwdVal.UsLowVal ) / 2 ;
			StAdjPar.StHalAdj.UsHlxMax	= StTneVal.StDwdVal.UsHigVal ;
			StAdjPar.StHalAdj.UsHlxMin	= StTneVal.StDwdVal.UsLowVal ;
		} else {
			StAdjPar.StHalAdj.UsHlyCen	= ( ( signed short )StTneVal.StDwdVal.UsHigVal + ( signed short )StTneVal.StDwdVal.UsLowVal ) / 2 ;
			StAdjPar.StHalAdj.UsHlyMax	= StTneVal.StDwdVal.UsHigVal ;
			StAdjPar.StHalAdj.UsHlyMin	= StTneVal.StDwdVal.UsLowVal ;
		}
	} else {
		if( UcDirSel == X_DIR ){
			StAdjPar.StHalAdj.UsHlxCna	= ( ( signed short )StTneVal.StDwdVal.UsHigVal + ( signed short )StTneVal.StDwdVal.UsLowVal ) / 2 ;
			StAdjPar.StHalAdj.UsHlxMxa	= StTneVal.StDwdVal.UsHigVal ;
			StAdjPar.StHalAdj.UsHlxMna	= StTneVal.StDwdVal.UsLowVal ;
		} else {
			StAdjPar.StHalAdj.UsHlyCna	= ( ( signed short )StTneVal.StDwdVal.UsHigVal + ( signed short )StTneVal.StDwdVal.UsLowVal ) / 2 ;
			StAdjPar.StHalAdj.UsHlyMxa	= StTneVal.StDwdVal.UsHigVal ;
			StAdjPar.StHalAdj.UsHlyMna	= StTneVal.StDwdVal.UsLowVal ;
		}
	}

	StTneVal.StDwdVal.UsHigVal	= 0x7fff - StTneVal.StDwdVal.UsHigVal ;		// Maximum Gap = Maximum - Hall Peak Top
	StTneVal.StDwdVal.UsLowVal	= StTneVal.StDwdVal.UsLowVal - 0x7fff ; 	// Minimum Gap = Hall Peak Bottom - Minimum

	
	return( StTneVal.UlDwdVal ) ;
}

//********************************************************************************
// Function Name 	: TneCen
// Retun Value		: Hall Center Tuning Result
// Argment Value	: X,Y Direction, Hall Top & Bottom Gaps
// Explanation		: Hall Center Tuning Function
// History			: First edition 						2009.12.1 YS.Kim
//********************************************************************************
unsigned short	UsValBef,UsValNow ;
unsigned char	TneCen( unsigned char	UcTneAxs, UnDwdVal	StTneVal )
{
	unsigned char 	UcTneRst, UcTmeOut, UcTofRst ;
	unsigned short	UsOffDif ;
	unsigned short	UsBiasVal ;

	UsErrBia	= 0 ;
	UsErrOfs	= 0 ;
	UcTmeOut	= 1 ;
	UsStpSiz	= 1 ;
	UcTneRst	= FAILURE ;
	UcTofRst	= FAILURE ;

	while ( UcTneRst && UcTmeOut )
	{
		if( UcTofRst == FAILURE ) {
			StTneVal.UlDwdVal	= TneOff( StTneVal, UcTneAxs ) ;
		} else {
			StTneVal.UlDwdVal	= TneBia( StTneVal, UcTneAxs ) ;
			UcTofRst	= FAILURE ;
		}

		if( !( UcTneAxs & 0xF0 ) )
		{
			if ( StTneVal.StDwdVal.UsHigVal > StTneVal.StDwdVal.UsLowVal ) {									// Check Offset Tuning Result
				UsOffDif	= ( StTneVal.StDwdVal.UsHigVal - StTneVal.StDwdVal.UsLowVal ) / 2 ;
			} else {
				UsOffDif	= ( StTneVal.StDwdVal.UsLowVal - StTneVal.StDwdVal.UsHigVal ) / 2 ;
			}

			if( UsOffDif < MARJIN ) {
				UcTofRst	= SUCCESS ;
			} else {
				UcTofRst	= FAILURE ;
			}

			if ( ( StTneVal.StDwdVal.UsHigVal < HALL_MIN_GAP && StTneVal.StDwdVal.UsLowVal < HALL_MIN_GAP )		// Check Tuning Result 
			&& ( StTneVal.StDwdVal.UsHigVal > HALL_MAX_GAP && StTneVal.StDwdVal.UsLowVal > HALL_MAX_GAP ) ) {
				UcTneRst	= SUCCESS ;
				break ;
			} else if ( UsStpSiz == 0 ) {
				UcTneRst	= SUCCESS ;
				break ;
			} else {
				UcTneRst	= FAILURE ;
				UcTmeOut++ ;
			}
		}else{
			if( (StTneVal.StDwdVal.UsHigVal > MARGIN ) && (StTneVal.StDwdVal.UsLowVal > MARGIN ) )	/* position check */
			{
				UcTofRst	= SUCCESS ;
				UsValBef = UsValNow = 0x0000 ;
			}else if( (StTneVal.StDwdVal.UsHigVal <= MARGIN ) && (StTneVal.StDwdVal.UsLowVal <= MARGIN ) ){
				UcTofRst	= SUCCESS ;
				UcTneRst	= FAILURE ;
			}else if( ((unsigned short)0xFFFF - ( StTneVal.StDwdVal.UsHigVal + StTneVal.StDwdVal.UsLowVal )) > BIAS_ADJ_OVER ) {
				UcTofRst	= SUCCESS ;
				UcTneRst	= FAILURE ;
			}else{
				UcTofRst	= FAILURE ;

				
				UsValBef = UsValNow ;

				RamAccFixMod( ON ) ;							// Fix mode
				
				if( !( UcTneAxs & 0x0F ) ) {
					RamReadA( DAXHLO, &UsValNow ) ;				// 0x1479	Hall X Offset Read
				}else{
					RamReadA( DAYHLO, &UsValNow ) ;				// 0x14F9	Hall Y Offset Read
				}
				if( ((( UsValBef & 0xFF00 ) == 0x8000 ) && ( UsValNow & 0xFF00 ) == 0x8000 )
				 || ((( UsValBef & 0xFF00 ) == 0x7F00 ) && ( UsValNow & 0xFF00 ) == 0x7F00 ) )
				{
					if( !( UcTneAxs & 0x0F ) ) {
						RamReadA( DAXHLB, &UsBiasVal ) ;		// 0x147A	Hall X Bias Read
					}else{
						RamReadA( DAYHLB, &UsBiasVal ) ;		// 0x14FA	Hall Y Bias Read
					}
					if( UsBiasVal > 0x8000 )
					{
						UsBiasVal -= 0x8000 ;
					}
					else
					{
						UsBiasVal += 0x8000 ;
					}
					if( UsBiasVal > DECRE_CAL )
					{
						UsBiasVal -= DECRE_CAL ;
					}
					UsBiasVal += 0x8000 ;
					
					if( !( UcTneAxs & 0x0F ) ) {
						RamWriteA( DAXHLB, UsBiasVal ) ;		// 0x147A	Hall X Bias
					}else{
						RamWriteA( DAYHLB, UsBiasVal ) ;		// 0x14FA	Hall Y Bias
					}
				}

				RamAccFixMod( OFF ) ;							// Float mode
				
			}
			
			if((( (unsigned short)0xFFFF - ( StTneVal.StDwdVal.UsHigVal + StTneVal.StDwdVal.UsLowVal )) < HALL_MAX_RANGE )
			&& (( (unsigned short)0xFFFF - ( StTneVal.StDwdVal.UsHigVal + StTneVal.StDwdVal.UsLowVal )) > HALL_MIN_RANGE ) ) {
				if(UcTofRst	== SUCCESS)
				{
					UcTneRst	= SUCCESS ;
					break ;
				}
			}
			UcTneRst	= FAILURE ;
			UcTmeOut++ ;
		}

		if( UcTneAxs & 0xF0 )
		{
			if ( ( UcTmeOut / 2 ) == TIME_OUT ) {
				UcTmeOut	= 0 ;
			}		 																							// Set Time Out Count
		}else{
			if ( UcTmeOut == TIME_OUT ) {
				UcTmeOut	= 0 ;
			}		 																							// Set Time Out Count
		}
	}

	if( UcTneRst == FAILURE ) {
		if( !( UcTneAxs & 0x0F ) ) {
			UcTneRst					= EXE_HXADJ ;
			StAdjPar.StHalAdj.UsHlxGan	= 0xFFFF ;
			StAdjPar.StHalAdj.UsHlxOff	= 0xFFFF ;
		} else {
			UcTneRst					= EXE_HYADJ ;
			StAdjPar.StHalAdj.UsHlyGan	= 0xFFFF ;
			StAdjPar.StHalAdj.UsHlyOff	= 0xFFFF ;
		}
	} else {
		UcTneRst	= EXE_END ;
	}

	return( UcTneRst ) ;
}



//********************************************************************************
// Function Name 	: TneBia
// Retun Value		: Hall Top & Bottom Gaps
// Argment Value	: Hall Top & Bottom Gaps , X,Y Direction
// Explanation		: Hall Bias Tuning Function
// History			: First edition 						2009.12.1 YS.Kim
//********************************************************************************
unsigned long	TneBia( UnDwdVal	StTneVal, unsigned char	UcTneAxs )
{
	long					SlSetBia ;
	unsigned short			UsSetBia ;
	unsigned char			UcChkFst ;
	static unsigned short	UsTneVax ;							// Variable For 1/2 Searching
	unsigned short			UsDecCal ;

	UcChkFst	= 1 ;

	if ( UsStpSiz == 1) {
		UsTneVax	= 2 ;

		if( UcTneAxs & 0xF0 ){
			if ( ((unsigned short)0xFFFF - ( StTneVal.StDwdVal.UsHigVal + StTneVal.StDwdVal.UsLowVal )) > BIAS_ADJ_OVER ) {
				UcChkFst	= 0 ;

				RamAccFixMod( ON ) ;							// Fix mode
				
				if ( !( UcTneAxs & 0x0F ) ) {							// Initializing Hall Offset & Bias, Step Size
					RamReadA( DAXHLB, &UsSetBia ) ;		// 0x147A	Hall X Bias Read
				} else {
					RamReadA( DAYHLB, &UsSetBia ) ;		// 0x14FA	Hall Y Bias Read
				}
				if( UsSetBia > 0x8000 )
				{
					UsSetBia -= 0x8000 ;
				}
				else
				{
					UsSetBia += 0x8000 ;
				}
				if( !UcChkFst )	{
					UsDecCal = ( DECRE_CAL << 3 ) ;
				}else{
					UsDecCal = DECRE_CAL ;
				}
				if( UsSetBia > UsDecCal )
				{
					UsSetBia -= UsDecCal ;
				}
				UsSetBia += 0x8000 ;
				if ( !( UcTneAxs & 0x0F ) ) {							// Initializing Hall Offset & Bias, Step Size
					RamWriteA( DAXHLB, UsSetBia ) ;				// 0x147A	Hall X Bias
					RamWriteA( DAXHLO, 0x0000 ) ;				// 0x1479	Hall X Offset 0x0000
				} else {
					RamWriteA( DAYHLB, UsSetBia ) ;				// 0x14FA	Hall Y Bias
					RamWriteA( DAYHLO, 0x0000 ) ;				// 0x14F9	Hall Y Offset 0x0000
				}
				UsStpSiz	= BIAS_LIMIT / UsTneVax ;

				RamAccFixMod( OFF ) ;							// Float mode
				
			}
		}else{
			if ( ( StTneVal.StDwdVal.UsHigVal + StTneVal.StDwdVal.UsLowVal ) / 2 < BIAS_ADJ_BORDER ) {
				UcChkFst	= 0 ;
			}

			if ( !UcTneAxs ) {										// Initializing Hall Offset & Bias, Step Size
				
				RamWrite32A( DAXHLB, 0xBF800000 ) ; 	// 0x147A	Hall X Bias 0x8001
				RamWrite32A( DAXHLO, 0x00000000 ) ;		// 0x1479	Hall X Offset 0x0000

				UsStpSiz	= BIAS_LIMIT / UsTneVax ;
			} else {
				RamWrite32A( DAYHLB, 0xBF800000 ) ; 	// 0x14FA	Hall Y Bias 0x8001
				RamWrite32A( DAYHLO, 0x00000000 ) ;		// 0x14F9	 Y Offset 0x0000
				UsStpSiz	= BIAS_LIMIT / UsTneVax ;
			}
		}
	}

	RamAccFixMod( ON ) ;							// Fix mode
	
	if ( !( UcTneAxs & 0x0F ) ) {
		RamReadA( DAXHLB, &UsSetBia ) ;					// 0x147A	Hall X Bias Read
		SlSetBia	= ( long )UsSetBia ;
	} else {
		RamReadA( DAYHLB, &UsSetBia ) ;					// 0x14FA	Hall Y Bias Read
		SlSetBia	= ( long )UsSetBia ;
	}

	if( SlSetBia >= 0x00008000 ) {
		SlSetBia	|= 0xFFFF0000 ;
	}

	if( UcChkFst ) {
		if( UcTneAxs & 0xF0 )
		{
			if ( ((unsigned short)0xFFFF - ( StTneVal.StDwdVal.UsHigVal + StTneVal.StDwdVal.UsLowVal )) < BIAS_ADJ_RANGE ) {	// Calculatiton For Hall BIAS 1/2 Searching
				if( ((unsigned short)0xFFFF - ( StTneVal.StDwdVal.UsHigVal + StTneVal.StDwdVal.UsLowVal )) < BIAS_ADJ_SKIP ) {
					SlSetBia	+= 0x0400 ;
				}else{
					SlSetBia	+= 0x0100 ;
				}
			} else {
				if( ((unsigned short)0xFFFF - ( StTneVal.StDwdVal.UsHigVal + StTneVal.StDwdVal.UsLowVal )) > BIAS_ADJ_OVER ) {
					SlSetBia	-= 0x0400 ;
				}else{
					SlSetBia	-= 0x0100 ;
				}
			}
			UsStpSiz	= 0x0200 ;
			
		}else{
			if ( ( StTneVal.StDwdVal.UsHigVal + StTneVal.StDwdVal.UsLowVal ) / 2 > BIAS_ADJ_BORDER ) {	// Calculatiton For Hall BIAS 1/2 Searching
				SlSetBia	+= UsStpSiz ;
			} else {
				SlSetBia	-= UsStpSiz ;
			}

			UsTneVax	= UsTneVax * 2 ;
			UsStpSiz	= BIAS_LIMIT / UsTneVax ;
		}
	}

	if( SlSetBia > ( long )0x00007FFF ) {
		SlSetBia	= 0x00007FFF ;
	} else if( SlSetBia < ( long )0xFFFF8001 ) {
		SlSetBia	= 0xFFFF8001 ;
	}

	if ( !( UcTneAxs & 0x0F ) ) {
		RamWriteA( DAXHLB, SlSetBia ) ;		// 0x147A	Hall X Bias Ram Write
	} else {
		RamWriteA( DAYHLB, SlSetBia ) ;		// 0x14FA	Hall Y Bias Ram Write
	}
	
	RamAccFixMod( OFF ) ;							// Float mode

	StTneVal.UlDwdVal	= TnePtp( UcTneAxs & 0x0F , PTP_AFTER ) ;

	return( StTneVal.UlDwdVal ) ;
}



//********************************************************************************
// Function Name 	: TneOff
// Retun Value		: Hall Top & Bottom Gaps
// Argment Value	: Hall Top & Bottom Gaps , X,Y Direction
// Explanation		: Hall Offset Tuning Function
// History			: First edition 						2009.12.1 YS.Kim
//********************************************************************************
unsigned long	TneOff( UnDwdVal	StTneVal, unsigned char	UcTneAxs )
{
	long			SlSetOff ;
	unsigned short	UsSetOff ;

	UcTneAxs &= 0x0F ;
	
	RamAccFixMod( ON ) ;							// Fix mode
		
	if ( !UcTneAxs ) {																			// Initializing Hall Offset & Bias
		RamReadA( DAXHLO, &UsSetOff ) ;				// 0x1479	Hall X Offset Read
		SlSetOff	= ( long )UsSetOff ;
	} else {
		RamReadA( DAYHLO, &UsSetOff ) ;				// 0x14F9	Hall Y Offset Read
		SlSetOff	= ( long )UsSetOff ;
	}

	if( SlSetOff > 0x00008000 ) {
		SlSetOff	|= 0xFFFF0000 ;
	}

	if ( StTneVal.StDwdVal.UsHigVal > StTneVal.StDwdVal.UsLowVal ) {
		SlSetOff	+= ( StTneVal.StDwdVal.UsHigVal - StTneVal.StDwdVal.UsLowVal ) / OFFSET_DIV ;	// Calculating Value For Increase Step
	} else {
		SlSetOff	-= ( StTneVal.StDwdVal.UsLowVal - StTneVal.StDwdVal.UsHigVal ) / OFFSET_DIV ;	// Calculating Value For Decrease Step
	}

	if( SlSetOff > ( long )0x00007FFF ) {
		SlSetOff	= 0x00007FFF ;
	} else if( SlSetOff < ( long )0xFFFF8001 ) {
		SlSetOff	= 0xFFFF8001 ;
	}

	if ( !UcTneAxs ) {
		RamWriteA( DAXHLO, SlSetOff ) ;		// 0x1479	Hall X Offset Ram Write
	} else {
		RamWriteA( DAYHLO, SlSetOff ) ;		// 0x14F9	Hall Y Offset Ram Write
	}

	RamAccFixMod( OFF ) ;							// Float mode
	
	StTneVal.UlDwdVal	= TnePtp( UcTneAxs, PTP_AFTER ) ;

	return( StTneVal.UlDwdVal ) ;
}

#endif

//********************************************************************************
// Function Name 	: MesFil
// Retun Value		: NON
// Argment Value	: Measure Filter Mode
// Explanation		: Measure Filter Setting Function
// History			: First edition 						2009.07.31  Y.Tashita
//********************************************************************************
void	MesFil( unsigned char	UcMesMod )
{
#ifdef	USE_EXTCLK_ALL	// 24MHz
	if( !UcMesMod ) {								// Hall Bias&Offset Adjust
		// Measure Filter1 Setting
		RamWrite32A( mes1aa, 0x3D1E5A40 ) ;		// 0x10F0	LPF150Hz
		RamWrite32A( mes1ab, 0x3D1E5A40 ) ;		// 0x10F1
		RamWrite32A( mes1ac, 0x3F6C34C0 ) ;		// 0x10F2
		RamWrite32A( mes1ad, 0x00000000 ) ;		// 0x10F3
		RamWrite32A( mes1ae, 0x00000000 ) ;		// 0x10F4
		RamWrite32A( mes1ba, 0x3F800000 ) ;		// 0x10F5	Through
		RamWrite32A( mes1bb, 0x00000000 ) ;		// 0x10F6
		RamWrite32A( mes1bc, 0x00000000 ) ;		// 0x10F7
		RamWrite32A( mes1bd, 0x00000000 ) ;		// 0x10F8
		RamWrite32A( mes1be, 0x00000000 ) ;		// 0x10F9
		
		// Measure Filter2 Setting
		RamWrite32A( mes2aa, 0x3D1E5A40 ) ;		// 0x11F0	LPF150Hz
		RamWrite32A( mes2ab, 0x3D1E5A40 ) ;		// 0x11F1
		RamWrite32A( mes2ac, 0x3F6C34C0 ) ;		// 0x11F2
		RamWrite32A( mes2ad, 0x00000000 ) ;		// 0x11F3
		RamWrite32A( mes2ae, 0x00000000 ) ;		// 0x11F4
		RamWrite32A( mes2ba, 0x3F800000 ) ;		// 0x11F5	Through
		RamWrite32A( mes2bb, 0x00000000 ) ;		// 0x11F6
		RamWrite32A( mes2bc, 0x00000000 ) ;		// 0x11F7
		RamWrite32A( mes2bd, 0x00000000 ) ;		// 0x11F8
		RamWrite32A( mes2be, 0x00000000 ) ;		// 0x11F9
		
	} else if( UcMesMod == LOOPGAIN ) {				// Loop Gain Adjust
		// Measure Filter1 Setting
		RamWrite32A( mes1aa, 0x3E587E00 ) ;		// 0x10F0	LPF1000Hz
		RamWrite32A( mes1ab, 0x3E587E00 ) ;		// 0x10F1
		RamWrite32A( mes1ac, 0x3F13C100 ) ;		// 0x10F2
		RamWrite32A( mes1ad, 0x00000000 ) ;		// 0x10F3
		RamWrite32A( mes1ae, 0x00000000 ) ;		// 0x10F4
		RamWrite32A( mes1ba, 0x3F7DF500 ) ;		// 0x10F5	HPF30Hz
		RamWrite32A( mes1bb, 0xBF7DF500 ) ;		// 0x10F6
		RamWrite32A( mes1bc, 0x3F7BEA40 ) ;		// 0x10F7
		RamWrite32A( mes1bd, 0x00000000 ) ;		// 0x10F8
		RamWrite32A( mes1be, 0x00000000 ) ;		// 0x10F9
		
		// Measure Filter2 Setting
		RamWrite32A( mes2aa, 0x3E587E00 ) ;		// 0x11F0	LPF1000Hz
		RamWrite32A( mes2ab, 0x3E587E00 ) ;		// 0x11F1
		RamWrite32A( mes2ac, 0x3F13C100 ) ;		// 0x11F2
		RamWrite32A( mes2ad, 0x00000000 ) ;		// 0x11F3
		RamWrite32A( mes2ae, 0x00000000 ) ;		// 0x11F4
		RamWrite32A( mes2ba, 0x3F7DF500 ) ;		// 0x11F5	HPF30Hz
		RamWrite32A( mes2bb, 0xBF7DF500 ) ;		// 0x11F6
		RamWrite32A( mes2bc, 0x3F7BEA40 ) ;		// 0x11F7
		RamWrite32A( mes2bd, 0x00000000 ) ;		// 0x11F8
		RamWrite32A( mes2be, 0x00000000 ) ;		// 0x11F9
		
	} else if( UcMesMod == THROUGH ) {				// for Through
		// Measure Filter1 Setting
		RamWrite32A( mes1aa, 0x3F800000 ) ;		// 0x10F0	Through
		RamWrite32A( mes1ab, 0x00000000 ) ;		// 0x10F1
		RamWrite32A( mes1ac, 0x00000000 ) ;		// 0x10F2
		RamWrite32A( mes1ad, 0x00000000 ) ;		// 0x10F3
		RamWrite32A( mes1ae, 0x00000000 ) ;		// 0x10F4
		RamWrite32A( mes1ba, 0x3F800000 ) ;		// 0x10F5	Through
		RamWrite32A( mes1bb, 0x00000000 ) ;		// 0x10F6
		RamWrite32A( mes1bc, 0x00000000 ) ;		// 0x10F7
		RamWrite32A( mes1bd, 0x00000000 ) ;		// 0x10F8
		RamWrite32A( mes1be, 0x00000000 ) ;		// 0x10F9
		
		// Measure Filter2 Setting
		RamWrite32A( mes2aa, 0x3F800000 ) ;		// 0x11F0	Through
		RamWrite32A( mes2ab, 0x00000000 ) ;		// 0x11F1
		RamWrite32A( mes2ac, 0x00000000 ) ;		// 0x11F2
		RamWrite32A( mes2ad, 0x00000000 ) ;		// 0x11F3
		RamWrite32A( mes2ae, 0x00000000 ) ;		// 0x11F4
		RamWrite32A( mes2ba, 0x3F800000 ) ;		// 0x11F5	Through
		RamWrite32A( mes2bb, 0x00000000 ) ;		// 0x11F6
		RamWrite32A( mes2bc, 0x00000000 ) ;		// 0x11F7
		RamWrite32A( mes2bd, 0x00000000 ) ;		// 0x11F8
		RamWrite32A( mes2be, 0x00000000 ) ;		// 0x11F9
		
	} else if( UcMesMod == NOISE ) {				// SINE WAVE TEST for NOISE
		// Measure Filter1 Setting
		RamWrite32A( mes1aa, 0x3D1E5A40 ) ;		// 0x10F0	LPF150Hz
		RamWrite32A( mes1ab, 0x3D1E5A40 ) ;		// 0x10F1
		RamWrite32A( mes1ac, 0x3F6C34C0 ) ;		// 0x10F2
		RamWrite32A( mes1ad, 0x00000000 ) ;		// 0x10F3
		RamWrite32A( mes1ae, 0x00000000 ) ;		// 0x10F4
		RamWrite32A( mes1ba, 0x3D1E5A40 ) ;		// 0x10F5	LPF150Hz
		RamWrite32A( mes1bb, 0x3D1E5A40 ) ;		// 0x10F6
		RamWrite32A( mes1bc, 0x3F6C34C0 ) ;		// 0x10F7
		RamWrite32A( mes1bd, 0x00000000 ) ;		// 0x10F8
		RamWrite32A( mes1be, 0x00000000 ) ;		// 0x10F9
		
		// Measure Filter2 Setting
		RamWrite32A( mes2aa, 0x3D1E5A40 ) ;		// 0x11F0	LPF150Hz
		RamWrite32A( mes2ab, 0x3D1E5A40 ) ;		// 0x11F1
		RamWrite32A( mes2ac, 0x3F6C34C0 ) ;		// 0x11F2
		RamWrite32A( mes2ad, 0x00000000 ) ;		// 0x11F3
		RamWrite32A( mes2ae, 0x00000000 ) ;		// 0x11F4
		RamWrite32A( mes2ba, 0x3D1E5A40 ) ;		// 0x11F5	LPF150Hz
		RamWrite32A( mes2bb, 0x3D1E5A40 ) ;		// 0x11F6
		RamWrite32A( mes2bc, 0x3F6C34C0 ) ;		// 0x11F7
		RamWrite32A( mes2bd, 0x00000000 ) ;		// 0x11F8
		RamWrite32A( mes2be, 0x00000000 ) ;		// 0x11F9
	}
#else
	if( !UcMesMod ) {								// Hall Bias&Offset Adjust
		// Measure Filter1 Setting
		RamWrite32A( mes1aa, 0x3CA175C0 ) ;		// 0x10F0	LPF150Hz
		RamWrite32A( mes1ab, 0x3CA175C0 ) ;		// 0x10F1
		RamWrite32A( mes1ac, 0x3F75E8C0 ) ;		// 0x10F2
		RamWrite32A( mes1ad, 0x00000000 ) ;		// 0x10F3
		RamWrite32A( mes1ae, 0x00000000 ) ;		// 0x10F4
		RamWrite32A( mes1ba, 0x3F800000 ) ;		// 0x10F5	Through
		RamWrite32A( mes1bb, 0x00000000 ) ;		// 0x10F6
		RamWrite32A( mes1bc, 0x00000000 ) ;		// 0x10F7
		RamWrite32A( mes1bd, 0x00000000 ) ;		// 0x10F8
		RamWrite32A( mes1be, 0x00000000 ) ;		// 0x10F9
		
		// Measure Filter2 Setting
		RamWrite32A( mes2aa, 0x3CA175C0 ) ;		// 0x11F0	LPF150Hz
		RamWrite32A( mes2ab, 0x3CA175C0 ) ;		// 0x11F1
		RamWrite32A( mes2ac, 0x3F75E8C0 ) ;		// 0x11F2
		RamWrite32A( mes2ad, 0x00000000 ) ;		// 0x11F3
		RamWrite32A( mes2ae, 0x00000000 ) ;		// 0x11F4
		RamWrite32A( mes2ba, 0x3F800000 ) ;		// 0x11F5	Through
		RamWrite32A( mes2bb, 0x00000000 ) ;		// 0x11F6
		RamWrite32A( mes2bc, 0x00000000 ) ;		// 0x11F7
		RamWrite32A( mes2bd, 0x00000000 ) ;		// 0x11F8
		RamWrite32A( mes2be, 0x00000000 ) ;		// 0x11F9
		
	} else if( UcMesMod == LOOPGAIN ) {				// Loop Gain Adjust
		// Measure Filter1 Setting
		RamWrite32A( mes1aa, 0x3DF21080 ) ;		// 0x10F0	LPF1000Hz
		RamWrite32A( mes1ab, 0x3DF21080 ) ;		// 0x10F1
		RamWrite32A( mes1ac, 0x3F437BC0 ) ;		// 0x10F2
		RamWrite32A( mes1ad, 0x00000000 ) ;		// 0x10F3
		RamWrite32A( mes1ae, 0x00000000 ) ;		// 0x10F4
		RamWrite32A( mes1ba, 0x3F7EF980 ) ;		// 0x10F5	HPF30Hz
		RamWrite32A( mes1bb, 0xBF7EF980 ) ;		// 0x10F6
		RamWrite32A( mes1bc, 0x3F7DF300 ) ;		// 0x10F7
		RamWrite32A( mes1bd, 0x00000000 ) ;		// 0x10F8
		RamWrite32A( mes1be, 0x00000000 ) ;		// 0x10F9
		
		// Measure Filter2 Setting
		RamWrite32A( mes2aa, 0x3DF21080 ) ;		// 0x11F0	LPF1000Hz
		RamWrite32A( mes2ab, 0x3DF21080 ) ;		// 0x11F1
		RamWrite32A( mes2ac, 0x3F437BC0 ) ;		// 0x11F2
		RamWrite32A( mes2ad, 0x00000000 ) ;		// 0x11F3
		RamWrite32A( mes2ae, 0x00000000 ) ;		// 0x11F4
		RamWrite32A( mes2ba, 0x3F7EF980 ) ;		// 0x11F5	HPF30Hz
		RamWrite32A( mes2bb, 0xBF7EF980 ) ;		// 0x11F6
		RamWrite32A( mes2bc, 0x3F7DF300 ) ;		// 0x11F7
		RamWrite32A( mes2bd, 0x00000000 ) ;		// 0x11F8
		RamWrite32A( mes2be, 0x00000000 ) ;		// 0x11F9
		
	} else if( UcMesMod == THROUGH ) {				// for Through
		// Measure Filter1 Setting
		RamWrite32A( mes1aa, 0x3F800000 ) ;		// 0x10F0	Through
		RamWrite32A( mes1ab, 0x00000000 ) ;		// 0x10F1
		RamWrite32A( mes1ac, 0x00000000 ) ;		// 0x10F2
		RamWrite32A( mes1ad, 0x00000000 ) ;		// 0x10F3
		RamWrite32A( mes1ae, 0x00000000 ) ;		// 0x10F4
		RamWrite32A( mes1ba, 0x3F800000 ) ;		// 0x10F5	Through
		RamWrite32A( mes1bb, 0x00000000 ) ;		// 0x10F6
		RamWrite32A( mes1bc, 0x00000000 ) ;		// 0x10F7
		RamWrite32A( mes1bd, 0x00000000 ) ;		// 0x10F8
		RamWrite32A( mes1be, 0x00000000 ) ;		// 0x10F9
		
		// Measure Filter2 Setting
		RamWrite32A( mes2aa, 0x3F800000 ) ;		// 0x11F0	Through
		RamWrite32A( mes2ab, 0x00000000 ) ;		// 0x11F1
		RamWrite32A( mes2ac, 0x00000000 ) ;		// 0x11F2
		RamWrite32A( mes2ad, 0x00000000 ) ;		// 0x11F3
		RamWrite32A( mes2ae, 0x00000000 ) ;		// 0x11F4
		RamWrite32A( mes2ba, 0x3F800000 ) ;		// 0x11F5	Through
		RamWrite32A( mes2bb, 0x00000000 ) ;		// 0x11F6
		RamWrite32A( mes2bc, 0x00000000 ) ;		// 0x11F7
		RamWrite32A( mes2bd, 0x00000000 ) ;		// 0x11F8
		RamWrite32A( mes2be, 0x00000000 ) ;		// 0x11F9
		
	} else if( UcMesMod == NOISE ) {				// SINE WAVE TEST for NOISE
		// Measure Filter1 Setting
		RamWrite32A( mes1aa, 0x3CA175C0 ) ;		// 0x10F0	LPF150Hz
		RamWrite32A( mes1ab, 0x3CA175C0 ) ;		// 0x10F1
		RamWrite32A( mes1ac, 0x3F75E8C0 ) ;		// 0x10F2
		RamWrite32A( mes1ad, 0x00000000 ) ;		// 0x10F3
		RamWrite32A( mes1ae, 0x00000000 ) ;		// 0x10F4
		RamWrite32A( mes1ba, 0x3CA175C0 ) ;		// 0x10F5	LPF150Hz
		RamWrite32A( mes1bb, 0x3CA175C0 ) ;		// 0x10F6
		RamWrite32A( mes1bc, 0x3F75E8C0 ) ;		// 0x10F7
		RamWrite32A( mes1bd, 0x00000000 ) ;		// 0x10F8
		RamWrite32A( mes1be, 0x00000000 ) ;		// 0x10F9
		
		// Measure Filter2 Setting
		RamWrite32A( mes2aa, 0x3CA175C0 ) ;		// 0x11F0	LPF150Hz
		RamWrite32A( mes2ab, 0x3CA175C0 ) ;		// 0x11F1
		RamWrite32A( mes2ac, 0x3F75E8C0 ) ;		// 0x11F2
		RamWrite32A( mes2ad, 0x00000000 ) ;		// 0x11F3
		RamWrite32A( mes2ae, 0x00000000 ) ;		// 0x11F4
		RamWrite32A( mes2ba, 0x3CA175C0 ) ;		// 0x11F5	LPF150Hz
		RamWrite32A( mes2bb, 0x3CA175C0 ) ;		// 0x11F6
		RamWrite32A( mes2bc, 0x3F75E8C0 ) ;		// 0x11F7
		RamWrite32A( mes2bd, 0x00000000 ) ;		// 0x11F8
		RamWrite32A( mes2be, 0x00000000 ) ;		// 0x11F9
	}
#endif
}



//********************************************************************************
// Function Name 	: SrvCon
// Retun Value		: NON
// Argment Value	: X or Y Select, Servo ON/OFF
// Explanation		: Servo ON,OFF Function
// History			: First edition 						2013.01.09 Y.Shigeoka
//********************************************************************************
void	SrvCon( unsigned char	UcDirSel, unsigned char	UcSwcCon )
{
	if( UcSwcCon ) {
		if( !UcDirSel ) {						// X Direction
			RegWriteA( WH_EQSWX , 0x03 ) ;			// 0x0170
			RamWrite32A( sxggf, 0x00000000 ) ;		// 0x10B5
		} else {								// Y Direction
			RegWriteA( WH_EQSWY , 0x03 ) ;			// 0x0171
			RamWrite32A( syggf, 0x00000000 ) ;		// 0x11B5
		}
	} else {
		if( !UcDirSel ) {						// X Direction
			RegWriteA( WH_EQSWX , 0x02 ) ;			// 0x0170
			RamWrite32A( SXLMT, 0x00000000 ) ;		// 0x1477
		} else {								// Y Direction
			RegWriteA( WH_EQSWY , 0x02 ) ;			// 0x0171
			RamWrite32A( SYLMT, 0x00000000 ) ;		// 0x14F7
		}
	}
}



#ifdef	MODULE_CALIBRATION
//********************************************************************************
// Function Name 	: LopGan
// Retun Value		: Execute Result
// Argment Value	: X,Y Direction
// Explanation		: Loop Gain Adjust Function
// History			: First edition 						2009.07.31 Y.Tashita
//********************************************************************************
unsigned char	LopGan( unsigned char	UcDirSel )
{
	unsigned char	UcLpAdjSts ;
	
 #ifdef	HALLADJ_HW
	UcLpAdjSts	= LoopGainAdj( UcDirSel ) ;
 #else
	MesFil( LOOPGAIN ) ;

	// Servo ON
	SrvCon( X_DIR, ON ) ;
	SrvCon( Y_DIR, ON ) ;

	// Wait 300ms
	WitTim( 300 ) ;

	// Loop Gain Adjust Initialize
	LopIni( UcDirSel ) ;

	// Loop Gain Adjust
	UcLpAdjSts	= LopAdj( UcDirSel ) ;
 #endif
	// Servo OFF
	SrvCon( X_DIR, OFF ) ;
	SrvCon( Y_DIR, OFF ) ;

	if( !UcLpAdjSts ) {
		return( EXE_END ) ;
	} else {
		if( !UcDirSel ) {
			return( EXE_LXADJ ) ;
		} else {
			return( EXE_LYADJ ) ;
		}
	}
}



 #ifndef	HALLADJ_HW
//********************************************************************************
// Function Name 	: LopIni
// Retun Value		: NON
// Argment Value	: X,Y Direction
// Explanation		: Loop Gain Adjust Initialize Function
// History			: First edition 						2009.07.31 Y.Tashita
//********************************************************************************
void	LopIni( unsigned char	UcDirSel )
{
	// Loop Gain Value Initialize
	LopPar( UcDirSel ) ;

	// Sign Wave Output Setting
	LopSin( UcDirSel, ON ) ;

}
 #endif


//********************************************************************************
// Function Name 	: LopPar
// Retun Value		: NON
// Argment Value	: X,Y Direction
// Explanation		: Loop Gain Adjust Parameter Initialize Function
// History			: First edition 						2013.01.15 Y.Shigeoka
//********************************************************************************
void	LopPar( unsigned char	UcDirSel )
{
	unsigned short	UsLopGan ;

	RamAccFixMod( ON ) ;							// Fix mode
	
	if( !UcDirSel ) {
		UsLopGan	= SXGAIN_LOP ;
		RamWriteA( sxg, UsLopGan ) ;			/* 0x10D3 */
	} else {
		UsLopGan	= SYGAIN_LOP ;
		RamWriteA( syg, UsLopGan ) ;			/* 0x11D3 */
	}

	RamAccFixMod( OFF ) ;							// Float mode
}



 #ifndef	HALLADJ_HW
//********************************************************************************
// Function Name 	: LopSin
// Retun Value		: NON
// Argment Value	: X,Y Direction, ON/OFF Switch
// Explanation		: Loop Gain Adjust Sign Wave Initialize Function
// History			: First edition 						2009.07.31 Y.Tashita
//********************************************************************************
void	LopSin( unsigned char	UcDirSel, unsigned char	UcSonOff )
{
	unsigned short		UsFreqVal ;
	unsigned char		UcEqSwX , UcEqSwY ;
	
	RegReadA( WH_EQSWX, &UcEqSwX ) ;				/* 0x0170	*/
	RegReadA( WH_EQSWY, &UcEqSwY ) ;				/* 0x0171	*/
		
	if( UcSonOff ) {
		
  #ifdef	USE_EXTCLK_ALL	// 24MHz
		/* Freq = CmSinFrq * 11.718kHz / 65536 / 16 */
   #ifdef	ACTREG_6P5OHM
//		UsFreqVal	=	0x30EE ;				/* 139.9Hz */
		UsFreqVal	=	0x29F1 ;				/* 119.9Hz */
   #endif
   #ifdef	ACTREG_10P2OHM
		UsFreqVal	=	0x29F1 ;				/* 119.9Hz */
   #endif
   #ifdef	ACTREG_15OHM
		UsFreqVal	=	0x3B6B ;				/* 169.9Hz */
   #endif
  #else
		/* Freq = CmSinFrq * 23.4375kHz / 65536 / 16 */
   #ifdef	ACTREG_6P5OHM
//		UsFreqVal	=	0x1877 ;				/* 139.9Hz */
		UsFreqVal	=	0x14F8 ;				/* 119.9Hz */
   #endif
   #ifdef	ACTREG_10P2OHM
		UsFreqVal	=	0x14F8 ;				/* 119.9Hz */
   #endif
   #ifdef	ACTREG_15OHM
		UsFreqVal	=	0x1DB5 ;				/* 169.9Hz */
   #endif
  #endif
		
		RegWriteA( WC_SINFRQ0,	(unsigned char)UsFreqVal ) ;				// 0x0181		Freq L
		RegWriteA( WC_SINFRQ1,	(unsigned char)(UsFreqVal >> 8) ) ;			// 0x0182		Freq H
		
		if( !UcDirSel ) {

			UcEqSwX |= 0x10 ;
			UcEqSwY &= ~EQSINSW ;
			
			RamWrite32A( sxsin, 0x3CA3D70A ) ;				// 0x10D5		-34dB
		} else {

			UcEqSwX &= ~EQSINSW ;
			UcEqSwY |= 0x10 ;
			
			RamWrite32A( sysin, 0x3CA3D70A ) ;				// 0x11D5		-34dB
		}
		RegWriteA( WC_SINPHSX, 0x00 ) ;					/* 0x0183	X Sine phase */
		RegWriteA( WC_SINPHSY, 0x00 ) ;					/* 0x0184	Y Sine phase */
		RegWriteA( WH_EQSWX, UcEqSwX ) ;				/* 0x0170	Switch control */
		RegWriteA( WH_EQSWY, UcEqSwY ) ;				/* 0x0171	Switch control */
		RegWriteA( WC_SINON,     0x01 ) ;				/* 0x0180	Sine wave  */
	} else {
		UcEqSwX &= ~EQSINSW ;
		UcEqSwY &= ~EQSINSW ;
		RegWriteA( WC_SINON,     0x00 ) ;				/* 0x0180	Sine wave  */
		if( !UcDirSel ) {
			RamWrite32A( sxsin, 0x00000000 ) ;			// 0x10D5
		} else {
			RamWrite32A( sysin, 0x00000000 ) ;			// 0x11D5
		}
		RegWriteA( WH_EQSWX, UcEqSwX ) ;				/* 0x0170	Switch control */
		RegWriteA( WH_EQSWY, UcEqSwY ) ;				/* 0x0171	Switch control */
	}
}



//********************************************************************************
// Function Name 	: LopAdj
// Retun Value		: Command Status
// Argment Value	: X,Y Direction
// Explanation		: Loop Gain Adjust Function
// History			: First edition 						2013.01.15 Y.Shigeoka
//********************************************************************************
unsigned char	LopAdj( unsigned char	UcDirSel )
{
	unsigned char	UcAdjSts	= FAILURE ;
	unsigned short	UsRtnVal ;
	float			SfCmpVal ;
	unsigned char	UcIdxCnt ;
	unsigned char	UcIdxCn1 ;
	unsigned char	UcIdxCn2 ;
	UnFltVal		UnAdcXg1, UnAdcXg2 , UnRtnVa ;

	float			DfGanVal[ 5 ] ;
	float			DfTemVal ;

	if( !UcDirSel ) {
		RegWriteA( WC_MES1ADD0, (unsigned char)SXGZ ) ;							// 0x0194
		RegWriteA( WC_MES1ADD1, (unsigned char)(( SXGZ >> 8 ) & 0x0001 ) ) ;	// 0x0195
		RegWriteA( WC_MES2ADD0, (unsigned char)SXG3Z ) ;						// 0x0196
		RegWriteA( WC_MES2ADD1, (unsigned char)(( SXG3Z >> 8 ) & 0x0001 ) ) ;	// 0x0197
	} else {
		RegWriteA( WC_MES1ADD0, (unsigned char)SYGZ ) ;							// 0x0194
		RegWriteA( WC_MES1ADD1, (unsigned char)(( SYGZ >> 8 ) & 0x0001 ) ) ;	// 0x0195
		RegWriteA( WC_MES2ADD0, (unsigned char)SYG3Z ) ;						// 0x0196
		RegWriteA( WC_MES2ADD1, (unsigned char)(( SYG3Z >> 8 ) & 0x0001 ) ) ;	// 0x0197
	}
	
	// 5 Times Average Value Calculation
	for( UcIdxCnt = 0 ; UcIdxCnt < 5 ; UcIdxCnt++ )
	{
		LopMes( ) ;																// Loop Gain Mesurement Start

		UnAdcXg1.UlLngVal	= GinMes( MES_XG1 ) ;										// LXG1 Measure
		UnAdcXg2.UlLngVal	= GinMes( MES_XG2 ) ;										// LXG2 Measure

		SfCmpVal	= UnAdcXg2.SfFltVal / UnAdcXg1.SfFltVal ;					// Compare Coefficient Value

		if( !UcDirSel ) {
			RamRead32A( sxg, &UnRtnVa.UlLngVal ) ;									// 0x10D3
		} else {
			RamRead32A( syg, &UnRtnVa.UlLngVal ) ;									// 0x11D3
		}
		UnRtnVa.SfFltVal	=  UnRtnVa.SfFltVal * SfCmpVal ;

		DfGanVal[ UcIdxCnt ]	= UnRtnVa.SfFltVal ;
	}

	for( UcIdxCn1 = 0 ; UcIdxCn1 < 4 ; UcIdxCn1++ )
	{
		for( UcIdxCn2 = UcIdxCn1+1 ; UcIdxCn2 < 5 ; UcIdxCn2++ )
		{
			if( DfGanVal[ UcIdxCn1 ] > DfGanVal[ UcIdxCn2 ] ) {
				DfTemVal				= DfGanVal[ UcIdxCn1 ] ;
				DfGanVal[ UcIdxCn1 ]	= DfGanVal[ UcIdxCn2 ] ;
				DfGanVal[ UcIdxCn2 ]	= DfTemVal ;
			}
		}
	}

	UnRtnVa.SfFltVal	=  ( DfGanVal[ 1 ] + DfGanVal[ 2 ] + DfGanVal[ 3 ] ) / 3  ;

	LopSin( UcDirSel, OFF ) ;

	if( UnRtnVa.UlLngVal < 0x3F800000 ) {									// Adjust Error
		UcAdjSts	= SUCCESS ;												// Status OK
	}

	if( UcAdjSts ) {
		if( !UcDirSel ) {
			RamWrite32A( sxg, 0x3F800000 ) ;						// 0x10D3
			StAdjPar.StLopGan.UsLxgVal	= 0x7FFF ;
			StAdjPar.StLopGan.UsLxgSts	= 0x0000 ;
		} else {
			RamWrite32A( syg, 0x3F800000 ) ;						// 0x11D3
			StAdjPar.StLopGan.UsLygVal	= 0x7FFF ;
			StAdjPar.StLopGan.UsLygSts	= 0x0000 ;
		}
	} else {
		if( !UcDirSel ) {
			RamWrite32A( sxg, UnRtnVa.UlLngVal ) ;						// 0x10D3
			RamAccFixMod( ON ) ;								// Fix mode
			RamReadA( sxg, &UsRtnVal ) ;						// 0x10D3
			StAdjPar.StLopGan.UsLxgVal	= UsRtnVal ;
			StAdjPar.StLopGan.UsLxgSts	= 0xFFFF ;
		} else {
			RamWrite32A( syg, UnRtnVa.UlLngVal ) ;						// 0x11D3
			RamAccFixMod( ON ) ;								// Fix mode
			RamReadA( syg, &UsRtnVal ) ;						// 0x11D3
			StAdjPar.StLopGan.UsLygVal	= UsRtnVal ;
			StAdjPar.StLopGan.UsLygSts	= 0xFFFF ;
		}
		RamAccFixMod( OFF ) ;							// Float mode
	}
	return( UcAdjSts ) ;
}


//********************************************************************************
// Function Name 	: LopMes
// Retun Value		: void
// Argment Value	: void
// Explanation		: Loop Gain Adjust Measure Setting
// History			: First edition 						2013.01.15 Y.Shigeoka
//********************************************************************************
void	LopMes( void )
{
	ClrGyr( 0x1000 , CLR_FRAM1 );					// Measure Filter RAM Clear
	RamWrite32A( MSABS1AV, 0x00000000 ) ;			// 0x1041	Clear
	RamWrite32A( MSABS2AV, 0x00000000 ) ;			// 0x1141	Clear
	RegWriteA( WC_MESLOOP1, 0x04 ) ;				// 0x0193
	RegWriteA( WC_MESLOOP0, 0x00 ) ;				// 0x0192	1024 Times Measure
	RamWrite32A( msmean	, 0x3A800000 );				// 0x1230	1/CmMesLoop[15:0]
	RegWriteA( WC_MESABS, 0x01 ) ;					// 0x0198	ABS
	RegWriteA( WC_MESWAIT,     0x00 ) ;				/* 0x0199	0 cross wait */
	BsyWit( WC_MESMODE, 0x01 ) ;					// 0x0190	Sin Wave Measure
}


//********************************************************************************
// Function Name 	: GinMes
// Retun Value		: Measure Result
// Argment Value	: MES1/MES2 Select
// Explanation		: Measure Result Read
// History			: First edition 						2013.01.15 Y.Shigeoka
//********************************************************************************
unsigned long	GinMes( unsigned char	UcXg1Xg2 )
{
	unsigned long	UlMesVal ;

	if( !UcXg1Xg2 ) {
		RamRead32A( MSABS1AV, &UlMesVal ) ;			// 0x1041
	} else {
		RamRead32A( MSABS2AV, &UlMesVal ) ;			// 0x1141
	}
	
	return( UlMesVal ) ;
}

 #endif
#endif

//********************************************************************************
// Function Name 	: TneGvc
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Tunes the Gyro VC offset
// History			: First edition 						2013.01.15  Y.Shigeoka
//********************************************************************************
#define	LIMITH		0x0FA0
#define	LIMITL		0xF060
#define	INITVAL		0x0000
unsigned char	TneGvc( void )
{
	unsigned char  UcRsltSts;
	
	
	// A/D Offset Clear
	RegWriteA( IZAH,	(unsigned char)(INITVAL >> 8) ) ;	// 0x02A0		Set Offset High byte
	RegWriteA( IZAL,	(unsigned char)INITVAL ) ;			// 0x02A1		Set Offset Low byte
	RegWriteA( IZBH,	(unsigned char)(INITVAL >> 8) ) ;	// 0x02A2		Set Offset High byte
	RegWriteA( IZBL,	(unsigned char)INITVAL ) ;			// 0x02A3		Set Offset Low byte
	
	MesFil( THROUGH ) ;				// 測定用フィルターを設定する。
	//////////
	// X
	//////////
	RegWriteA( WC_MES1ADD0, 0x00 ) ;		// 0x0194
	RegWriteA( WC_MES1ADD1, 0x00 ) ;		// 0x0195
	ClrGyr( 0x1000 , CLR_FRAM1 );					// Measure Filter RAM Clear
	StAdjPar.StGvcOff.UsGxoVal = (unsigned short)GenMes( AD2Z, 0 );		// 64回の平均値測定	GYRMON1(0x1110) <- GXADZ(0x144A)
	RegWriteA( IZAH, (unsigned char)(StAdjPar.StGvcOff.UsGxoVal >> 8) ) ;	// 0x02A0		Set Offset High byte
	RegWriteA( IZAL, (unsigned char)(StAdjPar.StGvcOff.UsGxoVal) ) ;		// 0x02A1		Set Offset Low byte
	//////////
	// Y
	//////////
	RegWriteA( WC_MES1ADD0, 0x00 ) ;		// 0x0194
	RegWriteA( WC_MES1ADD1, 0x00 ) ;		// 0x0195
	ClrGyr( 0x1000 , CLR_FRAM1 );					// Measure Filter RAM Clear
	StAdjPar.StGvcOff.UsGyoVal = (unsigned short)GenMes( AD3Z, 0 );		// 64回の平均値測定	GYRMON2(0x1111) <- GYADZ(0x14CA)
	RegWriteA( IZBH, (unsigned char)(StAdjPar.StGvcOff.UsGyoVal >> 8) ) ;	// 0x02A2		Set Offset High byte
	RegWriteA( IZBL, (unsigned char)(StAdjPar.StGvcOff.UsGyoVal) ) ;		// 0x02A3		Set Offset Low byte
	
	UcRsltSts = EXE_END ;						/* Clear Status */

	StAdjPar.StGvcOff.UsGxoSts	= 0xFFFF ;
	if(( (short)StAdjPar.StGvcOff.UsGxoVal < (short)LIMITL ) || ( (short)StAdjPar.StGvcOff.UsGxoVal > (short)LIMITH ))
	{
		UcRsltSts |= EXE_GXADJ ;
		StAdjPar.StGvcOff.UsGxoSts	= 0x0000 ;
	}
	
	StAdjPar.StGvcOff.UsGyoSts	= 0xFFFF ;
	if(( (short)StAdjPar.StGvcOff.UsGyoVal < (short)LIMITL ) || ( (short)StAdjPar.StGvcOff.UsGyoVal > (short)LIMITH ))
	{
		UcRsltSts |= EXE_GYADJ ;
		StAdjPar.StGvcOff.UsGyoSts	= 0x0000 ;
	}
	return( UcRsltSts );
		
}



//********************************************************************************
// Function Name 	: RtnCen
// Retun Value		: Command Status
// Argment Value	: Command Parameter
// Explanation		: Return to center Command Function
// History			: First edition 						2013.01.15 Y.Shigeoka
//********************************************************************************
unsigned char	RtnCen( unsigned char	UcCmdPar )
{
	unsigned char	UcCmdSts ;

	UcCmdSts	= EXE_END ;

	GyrCon( OFF ) ;											// Gyro OFF

	if( !UcCmdPar ) {										// X,Y Centering

		StbOnn() ;											// Slope Mode
		
	} else if( UcCmdPar == 0x01 ) {							// X Centering Only

		SrvCon( X_DIR, ON ) ;								// X only Servo ON
		SrvCon( Y_DIR, OFF ) ;
	} else if( UcCmdPar == 0x02 ) {							// Y Centering Only

		SrvCon( X_DIR, OFF ) ;								// Y only Servo ON
		SrvCon( Y_DIR, ON ) ;
	}

	return( UcCmdSts ) ;
}



//********************************************************************************
// Function Name 	: GyrCon
// Retun Value		: NON
// Argment Value	: Gyro Filter ON or OFF
// Explanation		: Gyro Filter Control Function
// History			: First edition 						2013.01.15 Y.Shigeoka
//********************************************************************************
void	GyrCon( unsigned char	UcGyrCon )
{
	// Return HPF Setting
	RegWriteA( WG_SHTON, 0x00 ) ;									// 0x0107
	
	if( UcGyrCon == ON ) {												// Gyro ON

		
#ifdef	GAIN_CONT
		/* Gain3 Register */
//		AutoGainControlSw( ON ) ;											/* Auto Gain Control Mode ON */
#endif
		ClrGyr( 0x000E , CLR_FRAM1 );		// Gyro Delay RAM Clear

		RamWrite32A( sxggf, 0x3F800000 ) ;	// 0x10B5
		RamWrite32A( syggf, 0x3F800000 ) ;	// 0x11B5
		
	} else if( UcGyrCon == SPC ) {										// Gyro ON for LINE

		
#ifdef	GAIN_CONT
		/* Gain3 Register */
//		AutoGainControlSw( ON ) ;											/* Auto Gain Control Mode ON */
#endif

		RamWrite32A( sxggf, 0x3F800000 ) ;	// 0x10B5
		RamWrite32A( syggf, 0x3F800000 ) ;	// 0x11B5
		

	} else {															// Gyro OFF
		
		RamWrite32A( sxggf, 0x00000000 ) ;	// 0x10B5
		RamWrite32A( syggf, 0x00000000 ) ;	// 0x11B5
		

#ifdef	GAIN_CONT
		/* Gain3 Register */
//		AutoGainControlSw( OFF ) ;											/* Auto Gain Control Mode OFF */
#endif
	}
}



//********************************************************************************
// Function Name 	: OisEna
// Retun Value		: NON
// Argment Value	: Command Parameter
// Explanation		: OIS Enable Control Function
// History			: First edition 						2013.01.15 Y.Shigeoka
//********************************************************************************
void	OisEna( void )
{
	// Servo ON
	SrvCon( X_DIR, ON ) ;
	SrvCon( Y_DIR, ON ) ;

	GyrCon( ON ) ;
}

//********************************************************************************
// Function Name 	: OisEnaLin
// Retun Value		: NON
// Argment Value	: Command Parameter
// Explanation		: OIS Enable Control Function for Line adjustment
// History			: First edition 						2013.09.05 Y.Shigeoka
//********************************************************************************
void	OisEnaLin( void )
{
	// Servo ON
	SrvCon( X_DIR, ON ) ;
	SrvCon( Y_DIR, ON ) ;

	GyrCon( SPC ) ;
}



//********************************************************************************
// Function Name 	: TimPro
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Timer Interrupt Process Function
// History			: First edition 						2013.01.15 Y.Shigeoka
//********************************************************************************
void	TimPro( void )
{
#ifdef	MODULE_CALIBRATION
	if( UcOscAdjFlg )
	{
		if( UcOscAdjFlg == MEASSTR )
		{
			RegWriteA( OSCCNTEN, 0x01 ) ;		// 0x0258	OSC Cnt enable
			UcOscAdjFlg = MEASCNT ;
		}
		else if( UcOscAdjFlg == MEASCNT )
		{
			RegWriteA( OSCCNTEN, 0x00 ) ;		// 0x0258	OSC Cnt disable
			UcOscAdjFlg = MEASFIX ;
		}
	}
#endif
}



//********************************************************************************
// Function Name 	: S2cPro
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: S2 Command Function
// History			: First edition 						2013.01.15 Y.Shigeoka
//********************************************************************************
void	S2cPro( unsigned char uc_mode )
{
	if( uc_mode == 1 )
	{
#ifdef H1COEF_CHANGER
		SetH1cMod( S2MODE ) ;							/* cancel Lvl change */
#endif
		// HPF→Through Setting
		RegWriteA( WG_SHTON, 0x11 ) ;							// 0x0107
		RamWrite32A( gxh1c, DIFIL_S2 );							// 0x1012
		RamWrite32A( gyh1c, DIFIL_S2 );							// 0x1112
	}
	else
	{
		RamWrite32A( gxh1c, UlH1Coefval );							// 0x1012
		RamWrite32A( gyh1c, UlH1Coefval );							// 0x1112
		// HPF→Through Setting
		RegWriteA( WG_SHTON, 0x00 ) ;							// 0x0107

#ifdef H1COEF_CHANGER
		SetH1cMod( UcH1LvlMod ) ;							/* Re-setting */
#endif
	}
	
}


//********************************************************************************
// Function Name 	: GenMes
// Retun Value		: A/D Convert Result
// Argment Value	: Measure Filter Input Signal Ram Address
// Explanation		: General Measure Function
// History			: First edition 						2013.01.10 Y.Shigeoka
//********************************************************************************
short	GenMes( unsigned short	UsRamAdd, unsigned char	UcMesMod )
{
	short	SsMesRlt ;

	RegWriteA( WC_MES1ADD0, (unsigned char)UsRamAdd ) ;							// 0x0194
	RegWriteA( WC_MES1ADD1, (unsigned char)(( UsRamAdd >> 8 ) & 0x0001 ) ) ;	// 0x0195
	RamWrite32A( MSABS1AV, 0x00000000 ) ;				// 0x1041	Clear
	
	if( !UcMesMod ) {
		RegWriteA( WC_MESLOOP1, 0x04 ) ;				// 0x0193
		RegWriteA( WC_MESLOOP0, 0x00 ) ;				// 0x0192	1024 Times Measure
		RamWrite32A( msmean	, 0x3A7FFFF7 );				// 0x1230	1/CmMesLoop[15:0]
	} else {
		RegWriteA( WC_MESLOOP1, 0x01 ) ;				// 0x0193
		RegWriteA( WC_MESLOOP0, 0x00 ) ;				// 0x0192	1 Times Measure
		RamWrite32A( msmean	, 0x3F800000 );				// 0x1230	1/CmMesLoop[15:0]
	}

	RegWriteA( WC_MESABS, 0x00 ) ;						// 0x0198	none ABS
	BsyWit( WC_MESMODE, 0x01 ) ;						// 0x0190	normal Measure

	RamAccFixMod( ON ) ;							// Fix mode
	
	RamReadA( MSABS1AV, ( unsigned short * )&SsMesRlt ) ;	// 0x1041

	RamAccFixMod( OFF ) ;							// Float mode
	
	return( SsMesRlt ) ;
}


//********************************************************************************
// Function Name 	: SetSinWavePara
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Sine wave Test Function
// History			: First edition 						2013.01.15 Y.Shigeoka
//********************************************************************************
#ifdef	USE_EXTCLK_ALL	// 24MHz
	/********* Parameter Setting *********/
	/* Servo Sampling Clock		=	11.71875kHz						*/
	/* Freq						=	CmSinFreq*Fs/65536/16			*/
	/* 05 00 XX MM 				XX:Freq MM:Sin or Circle */
const unsigned short	CucFreqVal[ 17 ]	= {
		0xFFFF,				//  0:  Stop
		0x0059,				//  1: 0.994653Hz
		0x00B2,				//  2: 1.989305Hz
		0x010C,				//  3: 2.995133Hz	
		0x0165,				//  4: 3.989786Hz
		0x01BF,				//  5: 4.995614Hz
		0x0218,				//  6: 5.990267Hz
		0x0272,				//  7: 6.996095Hz
		0x02CB,				//  8: 7.990748Hz
		0x0325,				//  9: 8.996576Hz
		0x037E,				//  A: 9.991229Hz
		0x03D8,				//  B: 10.99706Hz
		0x0431,				//  C: 11.99171Hz
		0x048B,				//  D: 12.99754Hz
		0x04E4,				//  E: 13.99219Hz
		0x053E,				//  F: 14.99802Hz
		0x0597				// 10: 15.99267Hz
	} ;
#else
	/********* Parameter Setting *********/
	/* Servo Sampling Clock		=	23.4375kHz						*/
	/* Freq						=	CmSinFreq*Fs/65536/16			*/
	/* 05 00 XX MM 				XX:Freq MM:Sin or Circle */
const unsigned short	CucFreqVal[ 17 ]	= {
		0xFFFF,				//  0:  Stop
		0x002C,				//  1: 0.983477Hz
		0x0059,				//  2: 1.989305Hz
		0x0086,				//  3: 2.995133Hz	
		0x00B2,				//  4: 3.97861Hz
		0x00DF,				//  5: 4.984438Hz
		0x010C,				//  6: 5.990267Hz
		0x0139,				//  7: 6.996095Hz
		0x0165,				//  8: 7.979572Hz
		0x0192,				//  9: 8.9854Hz
		0x01BF,				//  A: 9.991229Hz
		0x01EC,				//  B: 10.99706Hz
		0x0218,				//  C: 11.98053Hz
		0x0245,				//  D: 12.98636Hz
		0x0272,				//  E: 13.99219Hz
		0x029F,				//  F: 14.99802Hz
		0x02CB				// 10: 15.9815Hz
	} ;
#endif
	
#define		USE_SINLPF			/* if sin or circle movement is used LPF , this define has to enable */
	
/* 振幅はsxsin(0x10D5),sysin(0x11D5)で調整 */
void	SetSinWavePara( unsigned char UcTableVal ,  unsigned char UcMethodVal )
{
	unsigned short	UsFreqDat ;
	unsigned char	UcEqSwX , UcEqSwY ;

	
	if(UcTableVal > 0x10 )
		UcTableVal = 0x10 ;			/* Limit */
	UsFreqDat = CucFreqVal[ UcTableVal ] ;	
	
	if( UcMethodVal == SINEWAVE) {
		RegWriteA( WC_SINPHSX, 0x00 ) ;					/* 0x0183	*/
		RegWriteA( WC_SINPHSY, 0x00 ) ;					/* 0x0184	*/
	}else if( UcMethodVal == CIRCWAVE ){
		RegWriteA( WC_SINPHSX,	0x00 ) ;				/* 0x0183	*/
		RegWriteA( WC_SINPHSY,	0x20 ) ;				/* 0x0184	*/
	}else{
		RegWriteA( WC_SINPHSX, 0x00 ) ;					/* 0x0183	*/
		RegWriteA( WC_SINPHSY, 0x00 ) ;					/* 0x0184	*/
	}

#ifdef	USE_SINLPF
	if(( UcMethodVal != XHALWAVE ) && ( UcMethodVal != YHALWAVE )) {
		MesFil( NOISE ) ;			/* LPF */
	}
#endif

	if( UsFreqDat == 0xFFFF )			/* Sine波中止 */
	{

		RegReadA( WH_EQSWX, &UcEqSwX ) ;				/* 0x0170	*/
		RegReadA( WH_EQSWY, &UcEqSwY ) ;				/* 0x0171	*/
		UcEqSwX &= ~EQSINSW ;
		UcEqSwY &= ~EQSINSW ;
		RegWriteA( WH_EQSWX, UcEqSwX ) ;				/* 0x0170	*/
		RegWriteA( WH_EQSWY, UcEqSwY ) ;				/* 0x0171	*/
		
#ifdef	USE_SINLPF
		if(( UcMethodVal != XHALWAVE ) && ( UcMethodVal != YHALWAVE )) {
			RegWriteA( WC_DPON,     0x00 ) ;			/* 0x0105	Data pass off */
			RegWriteA( WC_DPO1ADD0, 0x00 ) ;			/* 0x01B8	output initial */
			RegWriteA( WC_DPO1ADD1, 0x00 ) ;			/* 0x01B9	output initial */
			RegWriteA( WC_DPO2ADD0, 0x00 ) ;			/* 0x01BA	output initial */
			RegWriteA( WC_DPO2ADD1, 0x00 ) ;			/* 0x01BB	output initial */
			RegWriteA( WC_DPI1ADD0, 0x00 ) ;			/* 0x01B0	input initial */
			RegWriteA( WC_DPI1ADD1, 0x00 ) ;			/* 0x01B1	input initial */
			RegWriteA( WC_DPI2ADD0, 0x00 ) ;			/* 0x01B2	input initial */
			RegWriteA( WC_DPI2ADD1, 0x00 ) ;			/* 0x01B3	input initial */
			
			/* Ram Access */
			RamAccFixMod( ON ) ;							// Fix mode
			
			RamWriteA( SXOFFZ1, UsCntXof ) ;			/* 0x1461	set optical value */
			RamWriteA( SYOFFZ1, UsCntYof ) ;			/* 0x14E1	set optical value */
			
			/* Ram Access */
			RamAccFixMod( OFF ) ;							// Float mode
	
			RegWriteA( WC_MES1ADD0,  0x00 ) ;			/* 0x0194	*/
			RegWriteA( WC_MES1ADD1,  0x00 ) ;			/* 0x0195	*/
			RegWriteA( WC_MES2ADD0,  0x00 ) ;			/* 0x0196	*/
			RegWriteA( WC_MES2ADD1,  0x00 ) ;			/* 0x0197	*/
			
		}
#endif
		RegWriteA( WC_SINON,     0x00 ) ;			/* 0x0180	Sine wave  */
		
	}
	else
	{
		
		RegReadA( WH_EQSWX, &UcEqSwX ) ;				/* 0x0170	*/
		RegReadA( WH_EQSWY, &UcEqSwY ) ;				/* 0x0171	*/
		
		if(( UcMethodVal != XHALWAVE ) && ( UcMethodVal != YHALWAVE )) {
#ifdef	USE_SINLPF
			RegWriteA( WC_DPI1ADD0,  ( unsigned char )MES1BZ2 ) ;						/* 0x01B0	input Meas-Fil */
			RegWriteA( WC_DPI1ADD1,  ( unsigned char )(( MES1BZ2 >> 8 ) & 0x0001 ) ) ;	/* 0x01B1	input Meas-Fil */
			RegWriteA( WC_DPI2ADD0,  ( unsigned char )MES2BZ2 ) ;						/* 0x01B2	input Meas-Fil */
			RegWriteA( WC_DPI2ADD1,  ( unsigned char )(( MES2BZ2 >> 8 ) & 0x0001 ) ) ;	/* 0x01B3	input Meas-Fil */
			RegWriteA( WC_DPO1ADD0, ( unsigned char )SXOFFZ1 ) ;						/* 0x01B8	output SXOFFZ1 */
			RegWriteA( WC_DPO1ADD1, ( unsigned char )(( SXOFFZ1 >> 8 ) & 0x0001 ) ) ;	/* 0x01B9	output SXOFFZ1 */
			RegWriteA( WC_DPO2ADD0, ( unsigned char )SYOFFZ1 ) ;						/* 0x01BA	output SYOFFZ1 */
			RegWriteA( WC_DPO2ADD1, ( unsigned char )(( SYOFFZ1 >> 8 ) & 0x0001 ) ) ;	/* 0x01BA	output SYOFFZ1 */
			
			RegWriteA( WC_MES1ADD0,  ( unsigned char )SINXZ ) ;							/* 0x0194	*/
			RegWriteA( WC_MES1ADD1,  ( unsigned char )(( SINXZ >> 8 ) & 0x0001 ) ) ;	/* 0x0195	*/
			RegWriteA( WC_MES2ADD0,  ( unsigned char )SINYZ ) ;							/* 0x0196	*/
			RegWriteA( WC_MES2ADD1,  ( unsigned char )(( SINYZ >> 8 ) & 0x0001 ) ) ;	/* 0x0197	*/
			
			RegWriteA( WC_DPON,     0x03 ) ;			/* 0x0105	Data pass[1:0] on */
			
			UcEqSwX &= ~EQSINSW ;
			UcEqSwY &= ~EQSINSW ;
#else
			UcEqSwX |= 0x08 ;
			UcEqSwY |= 0x08 ;
#endif

		}else{
			if( UcMethodVal == XHALWAVE ){
		    	UcEqSwX = 0x22 ;				/* SW[5] */
//		    	UcEqSwY = 0x03 ;
			}else{
//				UcEqSwX = 0x03 ;
				UcEqSwY = 0x22 ;				/* SW[5] */
			}
		}
		
		RegWriteA( WC_SINFRQ0,	(unsigned char)UsFreqDat ) ;				// 0x0181		Freq L
		RegWriteA( WC_SINFRQ1,	(unsigned char)(UsFreqDat >> 8) ) ;			// 0x0182		Freq H
		RegWriteA( WC_MESSINMODE,     0x00 ) ;			/* 0x0191	Sine 0 cross  */

		RegWriteA( WH_EQSWX, UcEqSwX ) ;				/* 0x0170	*/
		RegWriteA( WH_EQSWY, UcEqSwY ) ;				/* 0x0171	*/

		RegWriteA( WC_SINON,     0x01 ) ;			/* 0x0180	Sine wave  */
		
	}
	
	
}




#ifdef STANDBY_MODE
//********************************************************************************
// Function Name 	: SetStandby
// Retun Value		: NON
// Argment Value	: 0:Standby ON 1:Standby OFF 2:Standby2 ON 3:Standby2 OFF 
//					: 4:Standby3 ON 5:Standby3 OFF
// Explanation		: Set Standby
// History			: First edition 						2013.01.15 Y.Shigeoka
//********************************************************************************
void	SetStandby( unsigned char UcContMode )
{
	unsigned char	UcStbb0 , UcClkon ;
	
	switch(UcContMode)
	{
	case STB1_ON:

#ifdef	AF_PWMMODE
#else
		RegWriteA( DRVFCAF	, 0x00 );				// 0x0081	Drv.MODEAF=0,Drv.ENAAF=0,MODE-0
#endif
		RegWriteA( STBB0 	, 0x00 );		// 0x0250 	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
		RegWriteA( STBB1 	, 0x00 );		// 0x0264 	[ - | - | - | - ][ - | STBAFOP1 | - | STBAFDAC ]
		RegWriteA( PWMA 	, 0x00 );		// 0x0010		PWM Standby
		RegWriteA( PWMAAF 	, 0x00 );		// 0x0090		AF PWM Standby
		RegWriteA( CVA,  0x00 ) ;			/* 0x0020	LINEAR PWM mode standby	*/
		DrvSw( OFF ) ;						/* Driver OFF */
		AfDrvSw( OFF ) ;					/* AF Driver OFF */
#ifdef	MONITOR_OFF
#else
		RegWriteA( PWMMONA, 0x00 ) ;		// 0x0030	Monitor Standby
#endif
//		RegWriteA( DACMONFC, 0x01 ) ;		// 0x0032	DAC Monitor Standby
		SelectGySleep( ON ) ;				/* Gyro Sleep */
		break ;
	case STB1_OFF:
		SelectGySleep( OFF ) ;				/* Gyro Wake Up */
//		RegWriteA( DACMONFC, 0x81 ) ;		// 0x0032	DAC Monitor Active
		RegWriteA( PWMMONA, 0x80 ) ;		/* 0x0030	Monitor Active	*/
		DrvSw( ON ) ;						/* Driver Mode setting */
		AfDrvSw( ON ) ;						/* AF Driver Mode setting */
		RegWriteA( CVA		, 0xC0 );		// 0x0020	Linear PWM mode enable
		RegWriteA( PWMAAF 	, 0x00 );		// 0x0090		AF PWM Standby
		RegWriteA( PWMA		, 0xC0 );		// 0x0010	PWM enable
		RegWriteA( STBB1	, 0x05 ) ;		// 0x0264	[ - | - | - | - ][ - | STBAFOP1 | - | STBAFDAC ]
		RegWriteA( STBB0	, 0xDF );		// 0x0250 	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
		break ;
	case STB2_ON:
#ifdef	AF_PWMMODE
#else
		RegWriteA( DRVFCAF	, 0x00 );				// 0x0081	Drv.MODEAF=0,Drv.ENAAF=0,MODE-0
#endif
		RegWriteA( STBB0 	, 0x00 );		// 0x0250 	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
		RegWriteA( STBB1 	, 0x00 );		// 0x0264 	[ - | - | - | - ][ - | STBAFOP1 | - | STBAFDAC ]
		RegWriteA( PWMA 	, 0x00 );		// 0x0010		PWM Standby
		RegWriteA( PWMAAF 	, 0x00 );		// 0x0090		AF PWM Standby
		RegWriteA( CVA,  0x00 ) ;			/* 0x0020	LINEAR PWM mode standby	*/
		DrvSw( OFF ) ;						/* Drvier Block Ena=0 */
		AfDrvSw( OFF ) ;					/* AF Drvier Block Ena=0 */
#ifdef	MONITOR_OFF
#else
		RegWriteA( PWMMONA, 0x00 ) ;		// 0x0030	Monitor Standby
#endif
//		RegWriteA( DACMONFC, 0x01 ) ;		// 0x0032	DAC Monitor Standby
		SelectGySleep( ON ) ;				/* Gyro Sleep */
		RegWriteA( CLKON, 0x00 ) ;			/* 0x020B	Servo & PWM Clock OFF + D-Gyro I/F OFF	*/
		break ;
	case STB2_OFF:
		RegWriteA( CLKON,	0x1F ) ;		// 0x020B	[ - | - | CmOpafClkOn | CmAfpwmClkOn | CMGifClkOn  | CmScmClkOn  | CmSrvClkOn  | CmPwmClkOn  ]
		SelectGySleep( OFF ) ;				/* Gyro Wake Up */
//		RegWriteA( DACMONFC, 0x81 ) ;		// 0x0032	DAC Monitor Active
		RegWriteA( PWMMONA, 0x80 ) ;		/* 0x0030	Monitor Active	*/
		DrvSw( ON ) ;						/* Driver Mode setting */
		AfDrvSw( ON ) ;						/* AF Driver Mode setting */
		RegWriteA( CVA, 	0xC0 );			// 0x0020	Linear PWM mode enable
		RegWriteA( PWMAAF 	, 0x00 );		// 0x0090		AF PWM Standby
		RegWriteA( PWMA	, 	0xC0 );			// 0x0010	PWM enable
		RegWriteA( STBB1	, 0x05 ) ;		// 0x0264	[ - | - | - | - ][ - | STBAFOP1 | - | STBAFDAC ]
		RegWriteA( STBB0	, 0xDF );		// 0x0250 	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
		break ;
	case STB3_ON:
#ifdef	AF_PWMMODE
#else
		RegWriteA( DRVFCAF	, 0x00 );				// 0x0081	Drv.MODEAF=0,Drv.ENAAF=0,MODE-0
#endif
		RegWriteA( STBB0 	, 0x00 );		// 0x0250 	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
		RegWriteA( STBB1 	, 0x00 );		// 0x0264 	[ - | - | - | - ][ - | STBAFOP1 | - | STBAFDAC ]
		RegWriteA( PWMA 	, 0x00 );			// 0x0010		PWM Standby
		RegWriteA( PWMAAF 	, 0x00 );		// 0x0090		AF PWM Standby
		RegWriteA( CVA,  0x00 ) ;			/* 0x0020	LINEAR PWM mode standby	*/
		DrvSw( OFF ) ;						/* Drvier Block Ena=0 */
		AfDrvSw( OFF ) ;					/* AF Drvier Block Ena=0 */
#ifdef	MONITOR_OFF
#else
		RegWriteA( PWMMONA, 0x00 ) ;		// 0x0030	Monitor Standby
#endif
//		RegWriteA( DACMONFC, 0x01 ) ;		// 0x0032	DAC Monitor Standby
		SelectGySleep( ON ) ;				/* Gyro Sleep */
		RegWriteA( CLKON, 0x00 ) ;			/* 0x020B	Servo & PWM Clock OFF + D-Gyro I/F OFF	*/
		RegWriteA( I2CSEL, 0x01 ) ;			/* 0x0248	I2C Noise Cancel circuit OFF	*/
		RegWriteA( OSCSTOP, 0x02 ) ;		// 0x0256	Source Clock Input OFF
		break ;
	case STB3_OFF:
		RegWriteA( OSCSTOP, 0x00 ) ;		// 0x0256	Source Clock Input ON
		RegWriteA( I2CSEL, 0x00 ) ;			/* 0x0248	I2C Noise Cancel circuit ON	*/
		RegWriteA( CLKON,	0x1F ) ;		// 0x020B	[ - | - | - | - | CMGifClkOn  | CmScmClkOn  | CmSrvClkOn  | CmPwmClkOn  ]
		SelectGySleep( OFF ) ;				/* Gyro Wake Up */
//		RegWriteA( DACMONFC, 0x81 ) ;		// 0x0032	DAC Monitor Active
		RegWriteA( PWMMONA, 0x80 ) ;		/* 0x0030	Monitor Active	*/
		DrvSw( ON ) ;						/* Driver Mode setting */
		AfDrvSw( ON ) ;						/* AF Driver Mode setting */
		RegWriteA( CVA, 	0xC0 );			// 0x0020	Linear PWM mode enable
		RegWriteA( PWMAAF,	0x00 );			// 0x0090		AF PWM Standby
		RegWriteA( PWMA	, 	0xC0 );			// 0x0010	PWM enable
		RegWriteA( STBB1	, 0x05 ) ;		// 0x0264	[ - | - | - | - ][ - | STBAFOP1 | - | STBAFDAC ]
		RegWriteA( STBB0	, 0xDF );		// 0x0250 	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
		break ;
		
	case STB4_ON:
#ifdef	AF_PWMMODE
#else
		RegWriteA( DRVFCAF	, 0x00 );				// 0x0081	Drv.MODEAF=0,Drv.ENAAF=0,MODE-0
#endif
		RegWriteA( STBB0 	, 0x00 );		// 0x0250 	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
		RegWriteA( STBB1 	, 0x00 );		// 0x0264 	[ - | - | - | - ][ - | STBAFOP1 | - | STBAFDAC ]
		RegWriteA( PWMA 	, 0x00 );		// 0x0010		PWM Standby
		RegWriteA( PWMAAF 	, 0x00 );		// 0x0090		AF PWM Standby
		RegWriteA( CVA,  	0x00 ) ;		/* 0x0020	LINEAR PWM mode standby	*/
		DrvSw( OFF ) ;						/* Drvier Block Ena=0 */
		AfDrvSw( OFF ) ;					/* AF Drvier Block Ena=0 */
#ifdef	MONITOR_OFF
#else
		RegWriteA( PWMMONA, 0x00 ) ;		// 0x0030	Monitor Standby
#endif
//		RegWriteA( DACMONFC, 0x01 ) ;		// 0x0032	DAC Monitor Standby
		GyOutSignalCont( ) ;				/* Gyro Continuos mode */
		RegWriteA( CLKON, 0x04 ) ;			/* 0x020B	Servo & PWM Clock OFF + D-Gyro I/F ON	*/
		break ;
	case STB4_OFF:
		RegWriteA( CLKON,	0x1F ) ;		// 0x020B	[ - | - | - | - | CMGifClkOn  | CmScmClkOn  | CmSrvClkOn  | CmPwmClkOn  ]
		SelectGySleep( OFF ) ;				/* Gyro OIS mode */
//		RegWriteA( DACMONFC, 0x81 ) ;		// 0x0032	DAC Monitor Active
		RegWriteA( PWMMONA, 0x80 ) ;		/* 0x0030	Monitor Active	*/
		DrvSw( ON ) ;						/* Driver Mode setting */
		AfDrvSw( ON ) ;						/* AF Driver Mode setting */
		RegWriteA( CVA, 	0xC0 );			// 0x0020	Linear PWM mode enable
		RegWriteA( PWMAAF, 	0x00 );			// 0x0090		AF PWM Standby
		RegWriteA( PWMA	, 	0xC0 );			// 0x0010	PWM enable
		RegWriteA( STBB1	, 0x05 ) ;		// 0x0264	[ - | - | - | - ][ - | STBAFOP1 | - | STBAFDAC ]
		RegWriteA( STBB0	, 0xDF );		// 0x0250 	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
		break ;
		
		/************** special mode ************/
	case STB2_OISON:
		RegReadA( STBB0 	, &UcStbb0 );		// 0x0250 	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
		UcStbb0 &= 0x80 ;
		RegWriteA( STBB0 	, UcStbb0 );		// 0x0250 	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
		RegWriteA( PWMA 	, 0x00 );		// 0x0010		PWM Standby
		RegWriteA( CVA,  0x00 ) ;			/* 0x0020	LINEAR PWM mode standby	*/
		DrvSw( OFF ) ;						/* Drvier Block Ena=0 */
#ifdef	MONITOR_OFF
#else
		RegWriteA( PWMMONA, 0x00 ) ;		// 0x0030	Monitor Standby
#endif
//		RegWriteA( DACMONFC, 0x01 ) ;		// 0x0032	DAC Monitor Standby
		SelectGySleep( ON ) ;				/* Gyro Sleep */
		RegReadA( CLKON, &UcClkon ) ;		/* 0x020B	PWM Clock OFF + D-Gyro I/F OFF	SRVCLK can't OFF */
		UcClkon &= 0x1A ;
		RegWriteA( CLKON, UcClkon ) ;		/* 0x020B	PWM Clock OFF + D-Gyro I/F OFF	SRVCLK can't OFF */
		break ;
	case STB2_OISOFF:
		RegReadA( CLKON, &UcClkon ) ;		/* 0x020B	PWM Clock OFF + D-Gyro I/F ON  */
		UcClkon |= 0x05 ;
		RegWriteA( CLKON,	UcClkon ) ;		// 0x020B	[ - | - | CmOpafClkOn | CmAfpwmClkOn | CMGifClkOn  | CmScmClkOn  | CmSrvClkOn  | CmPwmClkOn  ]
		SelectGySleep( OFF ) ;				/* Gyro Wake Up */
//		RegWriteA( DACMONFC, 0x81 ) ;		// 0x0032	DAC Monitor Active
		RegWriteA( PWMMONA, 0x80 ) ;		/* 0x0030	Monitor Active	*/
		DrvSw( ON ) ;						/* Driver Mode setting */
		RegWriteA( CVA, 	0xC0 );			// 0x0020	Linear PWM mode enable
		RegWriteA( PWMA	, 	0xC0 );			// 0x0010	PWM enable
		RegReadA( STBB0	, &UcStbb0 );		// 0x0250 	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
		UcStbb0 |= 0x5F ;
		RegWriteA( STBB0	, UcStbb0 );	// 0x0250 	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
		break ;
		
	case STB2_AFON:
#ifdef	AF_PWMMODE
#else
		RegWriteA( DRVFCAF	, 0x00 );				// 0x0081	Drv.MODEAF=0,Drv.ENAAF=0,MODE-0
#endif
		RegReadA( STBB0 	, &UcStbb0 );		// 0x0250 	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
		UcStbb0 &= 0x7F ;
		RegWriteA( STBB0 	, UcStbb0 );		// 0x0250 	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
		RegWriteA( STBB1 	, 0x00 );		// 0x0264 	[ - | - | - | - ][ - | STBAFOP1 | - | STBAFDAC ]
		RegWriteA( PWMAAF 	, 0x00 );		// 0x0090		AF PWM Standby
		AfDrvSw( OFF ) ;					/* AF Drvier Block Ena=0 */
#ifdef	MONITOR_OFF
#else
		RegWriteA( PWMMONA, 0x00 ) ;		// 0x0030	Monitor Standby
#endif
		RegReadA( CLKON, &UcClkon ) ;		/* 0x020B	OPAF Clock OFF + AFPWM OFF	SRVCLK can't OFF	*/
		UcClkon &= 0x07 ;
		RegWriteA( CLKON, UcClkon ) ;		/* 0x020B	OPAF Clock OFF + AFPWM OFF	SRVCLK can't OFF	*/
		break ;
	case STB2_AFOFF:
		RegReadA( CLKON, &UcClkon ) ;		/* 0x020B	OPAF Clock ON + AFPWM ON  */
		UcClkon |= 0x18 ;
		RegWriteA( CLKON,	UcClkon ) ;		// 0x020B	[ - | - | CmOpafClkOn | CmAfpwmClkOn | CMGifClkOn  | CmScmClkOn  | CmSrvClkOn  | CmPwmClkOn  ]
		AfDrvSw( ON ) ;						/* AF Driver Mode setting */
		RegWriteA( PWMAAF 	, 0x00 );		// 0x0090		AF PWM Standby
		RegWriteA( STBB1	, 0x05 ) ;		// 0x0264	[ - | - | - | - ][ - | STBAFOP1 | - | STBAFDAC ]
		RegReadA( STBB0	, &UcStbb0 );		// 0x0250 	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
		UcStbb0 |= 0x80 ;
		RegWriteA( STBB0	, UcStbb0 );	// 0x0250 	[ STBAFDRV | STBOISDRV | STBOPAAF | STBOPAY ][ STBOPAX | STBDACI | STBDACV | STBADC ]
		break ;
		/************** special mode ************/
	}
}
#endif

//********************************************************************************
// Function Name 	: SetZsp
// Retun Value		: NON
// Argment Value	: Command Parameter
// Explanation		: Set Zoom Step parameter Function
// History			: First edition 						2013.01.15 Y.Shigeoka
//********************************************************************************
void	SetZsp( unsigned char	UcZoomStepDat )
{
	unsigned long	UlGyrZmx, UlGyrZmy, UlGyrZrx, UlGyrZry ;

	
	/* Zoom Step */
	if(UcZoomStepDat > (ZOOMTBL - 1))
		UcZoomStepDat = (ZOOMTBL -1) ;										/* 上限をZOOMTBL-1に設定する */

	if( UcZoomStepDat == 0 )				/* initial setting	*/
	{
		UlGyrZmx	= ClGyxZom[ 0 ] ;		// Same Wide Coefficient
		UlGyrZmy	= ClGyyZom[ 0 ] ;		// Same Wide Coefficient
		/* Initial Rate value = 1 */
	}
	else
	{
		UlGyrZmx	= ClGyxZom[ UcZoomStepDat ] ;
		UlGyrZmy	= ClGyyZom[ UcZoomStepDat ] ;
		
		
	}
	
	// Zoom Value Setting
	RamWrite32A( gxlens, UlGyrZmx ) ;		/* 0x1022 */
	RamWrite32A( gylens, UlGyrZmy ) ;		/* 0x1122 */

	RamRead32A( gxlens, &UlGyrZrx ) ;		/* 0x1022 */
	RamRead32A( gylens, &UlGyrZry ) ;		/* 0x1122 */

	// Zoom Value Setting Error Check
	if( UlGyrZmx != UlGyrZrx ) {
		RamWrite32A( gxlens, UlGyrZmx ) ;		/* 0x1022 */
	}

	if( UlGyrZmy != UlGyrZry ) {
		RamWrite32A( gylens, UlGyrZmy ) ;		/* 0x1122 */
	}

}

//********************************************************************************
// Function Name 	: StbOnn
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Stabilizer For Servo On Function
// History			: First edition 						2013.01.09 Y.Shigeoka
//********************************************************************************
 
void StbOnn( void )
{
	unsigned char	UcRegValx,UcRegValy;					// Registor value 
	unsigned char	UcRegIni ;
	
	RegReadA( WH_EQSWX , &UcRegValx ) ;			// 0x0170
	RegReadA( WH_EQSWY , &UcRegValy ) ;			// 0x0171
	
	if( (( UcRegValx & 0x01 ) != 0x01 ) && (( UcRegValy & 0x01 ) != 0x01 ))
	{
		
		RegWriteA( WH_SMTSRVON,	0x01 ) ;				// 0x017C		Smooth Servo ON
		
		SrvCon( X_DIR, ON ) ;
		SrvCon( Y_DIR, ON ) ;
		
		UcRegIni = 0x11;
		while( (UcRegIni & 0x77) != 0x66 )
		{
			RegReadA( RH_SMTSRVSTT,	&UcRegIni ) ;		// 0x01F8		Smooth Servo phase read
		}
		RegWriteA( WH_SMTSRVON,	0x00 ) ;				// 0x017C		Smooth Servo OFF
		
	}
	else
	{
		SrvCon( X_DIR, ON ) ;
		SrvCon( Y_DIR, ON ) ;
	}
}

//********************************************************************************
// Function Name 	: StbOnnN
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Stabilizer For Servo On Function
// History			: First edition 						2013.10.09 Y.Shigeoka
//********************************************************************************
 
void StbOnnN( unsigned char UcStbY , unsigned char UcStbX )
{
	unsigned char	UcRegIni ;
	unsigned char	UcSttMsk = 0 ;
	
	
	RegWriteA( WH_SMTSRVON,	0x01 ) ;				// 0x017C		Smooth Servo ON
	if( UcStbX == ON )	UcSttMsk |= 0x07 ;
	if( UcStbY == ON )	UcSttMsk |= 0x70 ;
	
	SrvCon( X_DIR, UcStbX ) ;
	SrvCon( Y_DIR, UcStbY ) ;
	
	UcRegIni = 0x11;
	while( (UcRegIni & UcSttMsk) != ( 0x66 & UcSttMsk ) )
	{
		RegReadA( RH_SMTSRVSTT,	&UcRegIni ) ;		// 0x01F8		Smooth Servo phase read
	}
	RegWriteA( WH_SMTSRVON,	0x00 ) ;				// 0x017C		Smooth Servo OFF
		
}

//********************************************************************************
// Function Name 	: OptCen
// Retun Value		: NON
// Argment Value	: UcOptMode 0:Set 1:Save&Set
//					: UsOptXval Xaxis offset
//					: UsOptYval Yaxis offset
// Explanation		: Send Optical Center
// History			: First edition 						2013.01.15 Y.Shigeoka
//********************************************************************************
void	OptCen( unsigned char UcOptmode , unsigned short UsOptXval , unsigned short UsOptYval )
{
	RamAccFixMod( ON ) ;							// Fix mode
	
	switch ( UcOptmode ) {
		case VAL_SET :
			RamWriteA( SXOFFZ1   , UsOptXval ) ;		/* 0x1461	Check Hall X optical center */
			RamWriteA( SYOFFZ1   , UsOptYval ) ;		/* 0x14E1	Check Hall Y optical center */
			break ;
		case VAL_FIX :
			UsCntXof = UsOptXval ;
			UsCntYof = UsOptYval ;
			RamWriteA( SXOFFZ1   , UsCntXof ) ;		/* 0x1461	Check Hall X optical center */
			RamWriteA( SYOFFZ1   , UsCntYof ) ;		/* 0x14E1	Check Hall Y optical center */

			break ;
		case VAL_SPC :
			RamReadA( SXOFFZ1   , &UsOptXval ) ;		/* 0x1461	Check Hall X optical center */
			RamReadA( SYOFFZ1   , &UsOptYval ) ;		/* 0x14E1	Check Hall Y optical center */
			UsCntXof = UsOptXval ;
			UsCntYof = UsOptYval ;


			break ;
	}

	RamAccFixMod( OFF ) ;							// Float mode
	
}

#ifdef	MODULE_CALIBRATION
//********************************************************************************
// Function Name 	: OscAdj
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: OSC Clock adjustment
// History			: First edition 						2013.01.15 Y.Shigeoka
//********************************************************************************
#define	RRATETABLE	8
#define	CRATETABLE	16
const signed char	ScRselRate[ RRATETABLE ]	= {
		-12,			/* -12% */
		 -9,			/*  -9% */
		 -6,			/*  -6% */
		 -3,			/*  -3% */
		  0,			/*   0% */
		  3,			/*   3% */
		  7,			/*   7% */
		 11				/*  11% */
	} ;
const signed char	ScCselRate[ CRATETABLE ]	= {
		-14,			/* -14% */
		-12,			/* -12% */
		-10,			/* -10% */
		 -8,			/*  -8% */
		 -6,			/*  -6% */
		 -4,			/*  -4% */
		 -2,			/*  -2% */
		  0,			/*   0% */
		  0,			/*   0% */
		  2,			/*   2% */
		  4,			/*   4% */
		  6,			/*   6% */
		  8,			/*   8% */
		 10,			/*  10% */
		 12,			/*  12% */
		 14				/*  14% */
	} ;
	

#define	TARGET_FREQ		48000.0F/* 48MHz */
//#define	TARGET_FREQ		24000.0F/* 24MHz */
#define	START_RSEL		0x04	/* Typ */
#define	START_CSEL		0x08	/* Typ bit4:OSCPMSEL */
#define	MEAS_MAX		32		/* 上限32回 */
/* Measure Status (UcClkJdg) */
#define	UNDR_MEAS		0x00
#define	FIX_MEAS		0x01
#define	JST_FIX			0x03
#define	OVR_MEAS		0x80
/* Measure Check Flag (UcMeasFlg) */
#define	RSELFX			0x08
#define	RSEL1ST			0x01
#define	RSEL2ND			0x02
#define	CSELFX			0x80
#define	CSELPLS			0x10
#define	CSELMNS			0x20

unsigned short	OscAdj( void )
{
	unsigned char	UcMeasFlg ;									/* Measure check flag */
	UnWrdVal		StClkVal ;									/* Measure value */
	unsigned char	UcMeasCnt ;									/* Measure counter */
	unsigned char	UcOscrsel , UcOsccsel ;						/* Reg set value */
	unsigned char	UcSrvDivBk ;								/* back up value */
	unsigned char	UcClkJdg ;									/* State flag */
	float			FcalA,FcalB ;								/* calcurate value */
	signed char		ScTblRate_Val, ScTblRate_Now, ScTblRate_Tgt ;	/* rate */
	float			FlRatePbk,FlRateMbk ;							/* Rate bk  */
	unsigned char	UcOsccselP , UcOsccselM ;					/* Reg set value */
	unsigned short	UsResult ;

//	unsigned char	UcOscsetBk ;								/* Reg set value */
	
	UcMeasFlg = 0 ;						/* Clear Measure check flag */
	UcMeasCnt = 0;						/* Clear Measure counter */
	UcClkJdg = UNDR_MEAS;				/* under Measure */
	UcOscrsel = START_RSEL ;
	UcOsccsel = START_CSEL ;
	/* check */
//	RegReadA( OSCSET, &UcOscsetBk ) ;	// 0x0264	
//	UcOscrsel = ( UcOscsetBk & 0xE0 ) >> 5 ;
//	UcOsccsel = ( UcOscsetBk & 0x1E ) >> 1 ;
	/**/
	
	RegReadA( SRVDIV, &UcSrvDivBk ) ;	/* 0x0211 */
	RegWriteA( SRVDIV,	0x00 ) ;		// 0x0211	 SRV Clock = Xtalck
	RegWriteA( OSCSET, ( UcOscrsel << 5 ) | (UcOsccsel << 1 ) ) ;	// 0x0257	 
	
	while( UcClkJdg == UNDR_MEAS )
	{
		UcMeasCnt++ ;						/* Measure count up */
		UcOscAdjFlg = MEASSTR ;				// Start trigger ON
		
		while( UcOscAdjFlg != MEASFIX )
		{
			;
		}
		
		UcOscAdjFlg = 0x00 ;				// Clear Flag
		RegReadA( OSCCK_CNTR0, &StClkVal.StWrdVal.UcLowVal ) ;		/* 0x025E */
		RegReadA( OSCCK_CNTR1, &StClkVal.StWrdVal.UcHigVal ) ;		/* 0x025F */
		
		FcalA = (float)StClkVal.UsWrdVal ;
		FcalB = TARGET_FREQ / FcalA ;
		FcalB =  FcalB - 1.0F ;
		FcalB *= 100.0F ;
		
		if( FcalB == 0.0F )
		{
			UcClkJdg = JST_FIX ;					/* Just 36MHz */
			UcMeasFlg |= ( CSELFX | RSELFX ) ;		/* Fix Flag */
			break ;
		}

		/* Rsel check */
		if( !(UcMeasFlg & RSELFX) )
		{
			if(UcMeasFlg & RSEL1ST)
			{
				UcMeasFlg |= ( RSELFX | RSEL2ND ) ;
			}
			else
			{
				UcMeasFlg |= RSEL1ST ;
			}
			ScTblRate_Now = ScRselRate[ UcOscrsel ] ;					/* 今のRate */
			ScTblRate_Tgt = ScTblRate_Now + (short)FcalB ;
			if( ScTblRate_Now > ScTblRate_Tgt )
			{
				while(1)
				{
					if( UcOscrsel == 0 )
					{
						break;
					}
					UcOscrsel -= 1 ;
					ScTblRate_Val = ScRselRate[ UcOscrsel ] ;	
					if( ScTblRate_Tgt >= ScTblRate_Val )
					{
						break;
					}
				}
			}
			else if( ScTblRate_Now < ScTblRate_Tgt )
			{
				while(1)
				{
					if(UcOscrsel == (RRATETABLE - 1))
					{
						break;
					}
					UcOscrsel += 1 ;
					ScTblRate_Val = ScRselRate[ UcOscrsel ] ;	
					if( ScTblRate_Tgt <= ScTblRate_Val )
					{
						break;
					}
				}
			}
			else
			{
				;
			}
		}
		else
		{		
		/* Csel check */
			if( FcalB > 0 )			/* Plus */
			{
				UcMeasFlg |= CSELPLS ;
				FlRatePbk = FcalB ;
				UcOsccselP = UcOsccsel ;
				if( UcMeasFlg & CSELMNS)
				{
					UcMeasFlg |= CSELFX ;
					UcClkJdg = FIX_MEAS ;			/* OK */
				}
				else if(UcOsccsel == (CRATETABLE - 1))
				{
					if(UcOscrsel < ( RRATETABLE - 1 ))
					{
						UcOscrsel += 1 ;
						UcOsccsel = START_CSEL ;
						UcMeasFlg = 0 ;			/* Clear */
					}
					else
					{
						UcClkJdg = OVR_MEAS ;			/* Over */
					}
				}
				else
				{
					UcOsccsel += 1 ;
				}
			}
			else					/* Minus */
			{
				UcMeasFlg |= CSELMNS ;
				FlRateMbk = (-1)*FcalB ;
				UcOsccselM = UcOsccsel ;
				if( UcMeasFlg & CSELPLS)
				{
					UcMeasFlg |= CSELFX ;
					UcClkJdg = FIX_MEAS ;			/* OK */
				}
				else if(UcOsccsel == 0x00)
				{
					if(UcOscrsel > 0)
					{
						UcOscrsel -= 1 ;
						UcOsccsel = START_CSEL ;
						UcMeasFlg = 0 ;			/* Clear */
					}
					else
					{
					UcClkJdg = OVR_MEAS ;			/* Over */
					}
				}
				else
				{
					UcOsccsel -= 1 ;
				}
			}
			if(UcMeasCnt >= MEAS_MAX)
			{
				UcClkJdg = OVR_MEAS ;			/* Over */
			}
		}	
		RegWriteA( OSCSET, ( UcOscrsel << 5 ) | (UcOsccsel << 1 ) ) ;	// 0x0257	 
	}
	
	UsResult = EXE_END ;
	
	if(UcClkJdg == FIX_MEAS)
	{
		if( FlRatePbk < FlRateMbk )
		{
			UcOsccsel = UcOsccselP ; 
		}
		else
		{
			UcOsccsel = UcOsccselM ; 
		}
	
		RegWriteA( OSCSET, ( UcOscrsel << 5 ) | (UcOsccsel << 1 ) ) ;	// 0x0264	 

		/* check */
//		RegReadA( OSCSET, &UcOscsetBk ) ;	// 0x0257	
		
	}
	StAdjPar.UcOscVal = ( ( UcOscrsel << 5 ) | (UcOsccsel << 1 ) );
	
	if(UcClkJdg == OVR_MEAS)
	{
		UsResult = EXE_OCADJ ;
		StAdjPar.UcOscVal = 0x00 ;
	}
	RegWriteA( SRVDIV,	UcSrvDivBk ) ;		// 0x0211	 SRV Clock set
	return( UsResult );
}
#endif


#ifdef HALLADJ_HW
//==============================================================================
//  Function    :   SetSineWave()
//  inputs      :   UcJikuSel   0: X-Axis
//                              1: Y-Axis
//                  UcMeasMode  0: Loop Gain frequency setting
//                              1: Bias/Offset frequency setting
//  outputs     :   void
//  explanation :   Initializes sine wave settings:
//                      Sine Table, Amplitue, Offset, Frequency
//  revisions   :   First Edition                          2013.01.15 Y.Shigeoka
//==============================================================================
void SetSineWave( unsigned char UcJikuSel , unsigned char UcMeasMode )
{
 #ifdef	USE_EXTCLK_ALL	// 24MHz
    unsigned short  UsFRQ[]   = { 0x30EE/*139.9Hz*/ , 0x037E/*10Hz*/ } ;          // { Loop Gain setting , Bias/Offset setting}
 #else
    unsigned short  UsFRQ[]   = { 0x1877/*139.9Hz*/ , 0x01BF/*10Hz*/ } ;          // { Loop Gain setting , Bias/Offset setting}
 #endif
	unsigned long   UlAMP[2][2]   = {{ 0x3CA3D70A , 0x3CA3D70A } ,		// Loop Gain   { X amp , Y amp }
									 { 0x3F800000 , 0x3F800000 } };		// Bias/offset { X amp , Y amp }
	unsigned char	UcEqSwX , UcEqSwY ;

    UcMeasMode &= 0x01;
    UcJikuSel  &= 0x01;

	/* Phase parameter 0deg */
	RegWriteA( WC_SINPHSX, 0x00 ) ;					/* 0x0183	*/
	RegWriteA( WC_SINPHSY, 0x00 ) ;					/* 0x0184	*/
	
	/* wait 0 cross */
	RegWriteA( WC_MESSINMODE,     0x00 ) ;			/* 0x0191	Sine 0 cross  */
	RegWriteA( WC_MESWAIT,     0x00 ) ;				/* 0x0199	0 cross wait */
	
    /* Manually Set Amplitude */
	RamWrite32A( sxsin, UlAMP[UcMeasMode][X_DIR] ) ;				// 0x10D5
	RamWrite32A( sysin, UlAMP[UcMeasMode][Y_DIR] ) ;				// 0x11D5

	/* Freq */
	RegWriteA( WC_SINFRQ0,	(unsigned char)UsFRQ[UcMeasMode] ) ;				// 0x0181		Freq L
	RegWriteA( WC_SINFRQ1,	(unsigned char)(UsFRQ[UcMeasMode] >> 8) ) ;			// 0x0182		Freq H

    /* Clear Optional Sine wave input address */
	RegReadA( WH_EQSWX, &UcEqSwX ) ;				/* 0x0170	*/
	RegReadA( WH_EQSWY, &UcEqSwY ) ;				/* 0x0171	*/
    if( !UcMeasMode && !UcJikuSel )       // Loop Gain mode  X-axis
    {
		UcEqSwX |= 0x10 ;				/* SW[4] */
		UcEqSwY &= ~EQSINSW ;
    }
    else if( !UcMeasMode && UcJikuSel )   // Loop Gain mode Y-Axis
    {
		UcEqSwX &= ~EQSINSW ;
		UcEqSwY |= 0x10 ;				/* SW[4] */
    }
    else if( UcMeasMode && !UcJikuSel )   // Bias/Offset mode X-Axis
    {
    	UcEqSwX = 0x22 ;				/* SW[5] */
    	UcEqSwY = 0x03 ;
    }
    else                    // Bias/Offset mode Y-Axis
    {
		UcEqSwX = 0x03 ;
		UcEqSwY = 0x22 ;				/* SW[5] */

    }
	RegWriteA( WH_EQSWX, UcEqSwX ) ;				/* 0x0170	*/
	RegWriteA( WH_EQSWY, UcEqSwY ) ;				/* 0x0171	*/
}

//==============================================================================
//  Function    :   StartSineWave()
//  inputs      :   none
//  outputs     :   void
//  explanation :   Starts sine wave
//  revisions   :   First Edition                          2011.04.13 d.yamagata
//==============================================================================
void StartSineWave( void )
{
    /* Start Sine Wave */
	RegWriteA( WC_SINON,     0x01 ) ;				/* 0x0180	Sine wave  */

}

//==============================================================================
//  Function    :   StopSineWave()
//  inputs      :   void
//  outputs     :   void
//  explanation :   Stops sine wave
//  revisions   :   First Edition                          2013.01.15 Y.Shigeoka
//==============================================================================
void StopSineWave( void )
{
	unsigned char		UcEqSwX , UcEqSwY ;
	
	RegWriteA( WC_SINON,     0x00 ) ;				/* 0x0180	Sine wave Stop */
	RegReadA( WH_EQSWX, &UcEqSwX ) ;				/* 0x0170	*/
	RegReadA( WH_EQSWY, &UcEqSwY ) ;				/* 0x0171	*/
	UcEqSwX &= ~EQSINSW ;
	UcEqSwY &= ~EQSINSW ;
	RegWriteA( WH_EQSWX, UcEqSwX ) ;				/* 0x0170	Switch control */
	RegWriteA( WH_EQSWY, UcEqSwY ) ;				/* 0x0171	Switch control */

}

//==============================================================================
//  Function    :   SetMeaseFil_LoopGain()
//  inputs      :   UcJikuSel   0: X-Axis
//                              1: Y-Axis
//                  UcMeasMode  0: Loop Gain frequency setting
//                              1: Bias/Offset frequency setting
//                  UcFilSel
//  outputs     :   void
//  explanation :
//  revisions   :   First Edition                          2013.01.15 Y.Shigeoka
//==============================================================================
void SetMeasFil( unsigned char UcFilSel )
{
	
	MesFil( UcFilSel ) ;					/* Set Measure filter */

}

//==============================================================================
//  Function    :   ClrMeasFil()
//  inputs      :   void
//  outputs     :   void
//  explanation :
//  revisions   :   First Edition                          2013.01.15 Y.Shigeoka
//==============================================================================
void ClrMeasFil( void )
{
    /* Measure Filters clear */
	ClrGyr( 0x1000 , CLR_FRAM1 );		// MEAS-FIL Delay RAM Clear
 
}

 #ifdef	MODULE_CALIBRATION
//==============================================================================
//  Function    :   LoopGainAdj()
//  inputs      :   UcJikuSel   0: X-Axis, 1: Y-Axis
//  outputs     :   void
//  explanation :
//  revisions   :   First Edition                          2011.04.13 d.yamagata
//==============================================================================
unsigned char	 LoopGainAdj( unsigned char UcJikuSel)
{

	unsigned short	UsRltVal ;
	unsigned char	UcAdjSts	= FAILURE ;
	
    UcJikuSel &= 0x01;

	StbOnn() ;											// Slope Mode
	
	// Wait 200ms
	WitTim( 200 ) ;
	
	/* set start gain */
	LopPar( UcJikuSel ) ;
	
	/* set sine wave */
    SetSineWave( UcJikuSel , __MEASURE_LOOPGAIN );

	/* Measure count */
	RegWriteA( WC_MESLOOP1, 0x00 ) ;				// 0x0193
	RegWriteA( WC_MESLOOP0, 0x40 ) ;				// 0x0192	64 Times Measure
	RamWrite32A( msmean	, 0x3C800000 );				// 0x1230	1/CmMesLoop[15:0]
	RegWriteA( WC_MESABS, 0x01 ) ;					// 0x0198	ABS
	
    /* Set Adjustment Limits */
    RamWrite32A( LGMax  , 0x3F800000 );		// 0x1092	Loop gain adjustment max limit
    RamWrite32A( LGMin  , 0x3E000100 );		// 0x1091	Loop gain adjustment min limit
    RegWriteA( WC_AMJLOOP1, 0x00 );			// 0x01A3	Time-Out time
    RegWriteA( WC_AMJLOOP0, 0x41 );			// 0x01A2	Time-Out time
    RegWriteA( WC_AMJIDL1, 0x00 );			// 0x01A5	wait
    RegWriteA( WC_AMJIDL0, 0x00 );			// 0x01A4	wait

    /* set Measure Filter */
    SetMeasFil( LOOPGAIN );

    /* Clear Measure Filters */
    ClrMeasFil();

    /* Start Sine Wave */
    StartSineWave();

    /* Enable Loop Gain Adjustment */
    /* Check Busy Flag */
	BsyWit( WC_AMJMODE, (0x0E | ( UcJikuSel << 4 )) ) ;				// 0x01A0	Loop Gain adjustment

	RegReadA( RC_AMJERROR, &UcAdjBsy ) ;							// 0x01AD

	/* Ram Access */
	RamAccFixMod( ON ) ;							// Fix mode
	
	if( UcAdjBsy )
	{
		if( UcJikuSel == X_DIR )
		{
			RamReadA( sxg, &UsRltVal ) ;						// 0x10D3
			StAdjPar.StLopGan.UsLxgVal	= UsRltVal ;
			StAdjPar.StLopGan.UsLxgSts	= 0x0000 ;
		} else {
			RamReadA( syg, &UsRltVal ) ;						// 0x11D3
			StAdjPar.StLopGan.UsLygVal	= UsRltVal ;
			StAdjPar.StLopGan.UsLygSts	= 0x0000 ;
		}

	}
	else
	{
		if( UcJikuSel == X_DIR )
		{
			RamReadA( sxg, &UsRltVal ) ;						// 0x10D3
			StAdjPar.StLopGan.UsLxgVal	= UsRltVal ;
			StAdjPar.StLopGan.UsLxgSts	= 0xFFFF ;
		} else {
			RamReadA( syg, &UsRltVal ) ;						// 0x11D3
			StAdjPar.StLopGan.UsLygVal	= UsRltVal ;
			StAdjPar.StLopGan.UsLygSts	= 0xFFFF ;
		}
		UcAdjSts	= SUCCESS ;												// Status OK
	}

	/* Ram Access */
	RamAccFixMod( OFF ) ;							// Float mode
	
    /* Stop Sine Wave */
    StopSineWave();

	return( UcAdjSts ) ;
}
 #endif

//==============================================================================
//  Function    :   BiasOffsetAdj()
//  inputs      :   UcJikuSel   0: X-Axis, 1: Y-Axis	UcMeasCnt  :Measure Count
//  outputs     :   Result status
//  explanation :
//  revisions   :   First Edition                          2013.01.16 Y.Shigeoka
//==============================================================================
unsigned char  BiasOffsetAdj( unsigned char UcJikuSel , unsigned char UcMeasCnt )
{
	unsigned char 	UcHadjRst ;
									/*	   STEP         OFSTH        OFSTL        AMPH         AMPL                 (80%)*/
	unsigned long  UlTgtVal[2][5]    =  {{ 0x3F800000 , 0x3D200140 , 0xBD200140 , 0x3F547AE1 , 0x3F451EB8 },	/* ROUGH */
										 { 0x3F000000 , 0x3D200140 , 0xBD200140 , 0x3F50A3D7 , 0x3F48F5C3 }} ;	/* FINE */

	if(UcMeasCnt > 1)		UcMeasCnt = 1 ;
	
    UcJikuSel &= 0x01;

    /* Set Sine Wave */
    SetSineWave( UcJikuSel , __MEASURE_BIASOFFSET );

	/* Measure count */
	RegWriteA( WC_MESLOOP1, 0x00 ) ;				// 0x0193
	RegWriteA( WC_MESLOOP0, 0x04 ) ;				// 0x0192	4 Times Measure
	RamWrite32A( msmean	, 0x3E000000 );				// 0x10AE	1/CmMesLoop[15:0]/2
	RegWriteA( WC_MESABS, 0x00 ) ;					// 0x0198	ABS

    /* Set Adjustment Limits */
    RamWrite32A( HOStp  , UlTgtVal[UcMeasCnt][0] );		// 0x1070	Hall Offset Stp
    RamWrite32A( HOMax  , UlTgtVal[UcMeasCnt][1] );		// 0x1072	Hall Offset max limit
    RamWrite32A( HOMin  , UlTgtVal[UcMeasCnt][2] );		// 0x1071	Hall Offset min limit
    RamWrite32A( HBStp  , UlTgtVal[UcMeasCnt][0] );		// 0x1080	Hall Bias Stp
    RamWrite32A( HBMax  , UlTgtVal[UcMeasCnt][3] );		// 0x1082	Hall Bias max limit
    RamWrite32A( HBMin  , UlTgtVal[UcMeasCnt][4] );		// 0x1081	Hall Bias min limit

	RegWriteA( WC_AMJLOOP1, 0x00 );			// 0x01A3	Time-Out time
    RegWriteA( WC_AMJLOOP0, 0x40 );			// 0x01A2	Time-Out time
    RegWriteA( WC_AMJIDL1, 0x00 );			// 0x01A5	wait
    RegWriteA( WC_AMJIDL0, 0x00 );			// 0x01A4	wait
	
    /* Set Measure Filter */
    SetMeasFil( HALL_ADJ );

    /* Clear Measure Filters */
    ClrMeasFil();

    /* Start Sine Wave */
    StartSineWave();

    /* Check Busy Flag */
	BsyWit( WC_AMJMODE, (0x0C | ( UcJikuSel << 4 )) ) ;				// 0x01A0	Hall bais/offset ppt adjustment
	
	RegReadA( RC_AMJERROR, &UcAdjBsy ) ;							// 0x01AD
	
	if( UcAdjBsy )
	{
		if( UcJikuSel == X_DIR )
		{
			UcHadjRst = EXE_HXADJ ;
		}
		else
		{
			UcHadjRst = EXE_HYADJ ;
		}

	}
	else
	{
		UcHadjRst = EXE_END ;
	}

    /* Stop Sine Wave */
    StopSineWave();

    /* Set Servo Filter */
	
	/* Ram Access */
	RamAccFixMod( ON ) ;							// Fix mode
	
	if( UcJikuSel == X_DIR )
	{
		RamReadA( MSPP1AV, &StAdjPar.StHalAdj.UsHlxMxa  ) ;	 	// 0x1042 Max width value
		RamReadA( MSCT1AV, &StAdjPar.StHalAdj.UsHlxCna  ) ;	 	// 0x1052 offset value
	}
	else
	{
//		RamReadA( MSPP2AV, &StAdjPar.StHalAdj.UsHlyMxa  ) ;	 	// 0x1142 Max width value
//		RamReadA( MSCT2AV, &StAdjPar.StHalAdj.UsHlyCna  ) ;	 	// 0x1152 offset value
		RamReadA( MSPP1AV, &StAdjPar.StHalAdj.UsHlyMxa  ) ;	 	// 0x1042 Max width value
		RamReadA( MSCT1AV, &StAdjPar.StHalAdj.UsHlyCna  ) ;	 	// 0x1052 offset value
	}

	StAdjPar.StHalAdj.UsHlxCna = (unsigned short)((signed short)StAdjPar.StHalAdj.UsHlxCna << 1 ) ;
	StAdjPar.StHalAdj.UsHlyCna = (unsigned short)((signed short)StAdjPar.StHalAdj.UsHlyCna << 1 ) ;
	/* Ram Access */
	RamAccFixMod( OFF ) ;							// Float mode
	
	return( UcHadjRst ) ;
}

#endif


//********************************************************************************
// Function Name 	: GyrGan
// Retun Value		: NON
// Argment Value	: UcGygmode 0:Set 1:Save&Set
//					: UlGygXval Xaxis Gain
//					: UlGygYval Yaxis Gain
// Explanation		: Send Gyro Gain
// History			: First edition 						2011.02.09 Y.Shigeoka
//********************************************************************************
void	GyrGan( unsigned char UcGygmode , unsigned long UlGygXval , unsigned long UlGygYval )
{
	switch ( UcGygmode ) {
		case VAL_SET :
			RamWrite32A( gxzoom, UlGygXval ) ;		// 0x1020
			RamWrite32A( gyzoom, UlGygYval ) ;		// 0x1120
			break ;
		case VAL_FIX :
			RamWrite32A( gxzoom, UlGygXval ) ;		// 0x1020
			RamWrite32A( gyzoom, UlGygYval ) ;		// 0x1120

			break ;
		case VAL_SPC :
			RamRead32A( gxzoom, &UlGygXval ) ;		// 0x1020
			RamRead32A( gyzoom, &UlGygYval ) ;		// 0x1120
		
			break ;
	}

}

//********************************************************************************
// Function Name 	: SetPanTiltMode
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Pan-Tilt Enable/Disable
// History			: First edition 						2013.01.09 Y.Shigeoka
//********************************************************************************
void	SetPanTiltMode( unsigned char UcPnTmod )
{
	switch ( UcPnTmod ) {
		case OFF :
			RegWriteA( WG_PANON, 0x00 ) ;			// 0x0109	X,Y Pan/Tilt Function OFF
			break ;
		case ON :
			RegWriteA( WG_PANON, 0x01 ) ;			// 0x0109	X,Y Pan/Tilt Function ON
//			RegWriteA( WG_PANON, 0x10 ) ;			// 0x0109	X,Y New Pan/Tilt Function ON
			break ;
	}

}


#ifdef GAIN_CONT
//********************************************************************************
// Function Name 	: TriSts
// Retun Value		: Tripod Status
//					: bit0( 1:Y Tripod ON / 0:OFF)
//					: bit4( 1:X Tripod ON / 0:OFF)
//					: bit7( 1:Tripod ENABLE  / 0:DISABLE)
// Argment Value	: NON
// Explanation		: Read Status of Tripod mode Function
// History			: First edition 						2013.02.18 Y.Shigeoka
//********************************************************************************
unsigned char	TriSts( void )
{
	unsigned char UcRsltSts = 0;
	unsigned char UcVal ;

	RegReadA( WG_ADJGANGXATO, &UcVal ) ;	// 0x0129
	if( UcVal & 0x03 ){						// Gain control enable?
		RegReadA( RG_LEVJUGE, &UcVal ) ;	// 0x01F4
		UcRsltSts = UcVal & 0x11 ;		// bit0, bit4 set
		UcRsltSts |= 0x80 ;				// bit7 ON
	}
	return( UcRsltSts ) ;
}
#endif

//********************************************************************************
// Function Name 	: DrvPwmSw
// Retun Value		: Mode Status
//					: bit4( 1:PWM / 0:LinearPwm)
// Argment Value	: NON
// Explanation		: Select Driver mode Function
// History			: First edition 						2013.02.18 Y.Shigeoka
//********************************************************************************
unsigned char	DrvPwmSw( unsigned char UcSelPwmMod )
{

	switch ( UcSelPwmMod ) {
		case Mlnp :
			RegWriteA( DRVFC	, 0xF0 );			// 0x0001	Drv.MODE=1,Drv.BLK=1,MODE2,LCEN
			UcPwmMod = PWMMOD_CVL ;
			break ;
		
		case Mpwm :
#ifdef	PWM_BREAK
			RegWriteA( DRVFC	, 0x00 );			// 0x0001	Drv.MODE=0,Drv.BLK=0,MODE0B
#else
			RegWriteA( DRVFC	, 0xC0 );			// 0x0001	Drv.MODE=1,Drv.BLK=1,MODE1
#endif
			UcPwmMod = PWMMOD_PWM ;
 			break ;
	}
	
	return( UcSelPwmMod << 4 ) ;
}

 #ifdef	NEUTRAL_CENTER
//********************************************************************************
// Function Name 	: TneHvc
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: Tunes the Hall VC offset
// History			: First edition 						2013.03.13	T.Tokoro
//********************************************************************************
unsigned char	TneHvc( void )
{
	unsigned char	UcRsltSts;
	unsigned short	UsMesRlt1 ;
	unsigned short	UsMesRlt2 ;
	
	SrvCon( X_DIR, OFF ) ;				// X Servo OFF
	SrvCon( Y_DIR, OFF ) ;				// Y Servo OFF
	
	WitTim( 500 ) ;
	
	//平均値測定
	
	MesFil( THROUGH ) ;					// Set Measure Filter
	
	RegWriteA( WC_MESLOOP1	, 0x00 );			// 0x0193	CmMesLoop[15:8]
	RegWriteA( WC_MESLOOP0	, 0x40);			// 0x0192	CmMesLoop[7:0]

	RamWrite32A( msmean	, 0x3C800000 );			// 0x1230	1/CmMesLoop[15:0]
	
	RegWriteA( WC_MES1ADD0,  ( unsigned char )AD0Z ) ;							/* 0x0194	*/
	RegWriteA( WC_MES1ADD1,  ( unsigned char )(( AD0Z >> 8 ) & 0x0001 ) ) ;		/* 0x0195	*/
	RegWriteA( WC_MES2ADD0,  ( unsigned char )AD1Z ) ;							/* 0x0196	*/
	RegWriteA( WC_MES2ADD1,  ( unsigned char )(( AD1Z >> 8 ) & 0x0001 ) ) ;		/* 0x0197	*/
	
	RamWrite32A( MSABS1AV, 	0x00000000 ) ;		// 0x1041
	RamWrite32A( MSABS2AV, 	0x00000000 ) ;		// 0x1141
	
	RegWriteA( WC_MESABS, 0x00 ) ;				// 0x0198	none ABS
	
	BsyWit( WC_MESMODE, 0x01 ) ;				// 0x0190		Normal Measure
	
	RamAccFixMod( ON ) ;							// Fix mode
	
	RamReadA( MSABS1AV, &UsMesRlt1 ) ;			// 0x1041	Measure Result
	RamReadA( MSABS2AV, &UsMesRlt2 ) ;			// 0x1141	Measure Result

	RamAccFixMod( OFF ) ;							// Float mode
	
	StAdjPar.StHalAdj.UsHlxCna = UsMesRlt1;			//Measure Result Store
	StAdjPar.StHalAdj.UsHlxCen = UsMesRlt1;			//Measure Result Store
	
	StAdjPar.StHalAdj.UsHlyCna = UsMesRlt2;			//Measure Result Store
	StAdjPar.StHalAdj.UsHlyCen = UsMesRlt2;			//Measure Result Store
	
	UcRsltSts = EXE_END ;				// Clear Status
	
	return( UcRsltSts );
}
 #endif	//NEUTRAL_CENTER

//********************************************************************************
// Function Name 	: SetGcf
// Retun Value		: NON
// Argment Value	: Command Parameter
// Explanation		: Set DI filter coefficient Function
// History			: First edition 						2013.03.22 Y.Shigeoka
//********************************************************************************
void	SetGcf( unsigned char	UcSetNum )
{
	
	/* Zoom Step */
	if(UcSetNum > (COEFTBL - 1))
		UcSetNum = (COEFTBL -1) ;			/* 上限をCOEFTBL-1に設定する */

	UlH1Coefval	= ClDiCof[ UcSetNum ] ;
		
	// Zoom Value Setting
	RamWrite32A( gxh1c, UlH1Coefval ) ;		/* 0x1012 */
	RamWrite32A( gyh1c, UlH1Coefval ) ;		/* 0x1112 */

#ifdef H1COEF_CHANGER
		SetH1cMod( UcSetNum ) ;							/* Re-setting */
#endif

}

#ifdef H1COEF_CHANGER
//********************************************************************************
// Function Name 	: SetH1cMod
// Retun Value		: NON
// Argment Value	: Command Parameter
// Explanation		: Set H1C coefficient Level chang Function
// History			: First edition 						2013.04.18 Y.Shigeoka
//********************************************************************************
void	SetH1cMod( unsigned char	UcSetNum )
{
	
	switch( UcSetNum ){
	case ( ACTMODE ):				// initial 
		IniPtMovMod( OFF ) ;							// Pan/Tilt setting (Still)
		
		/* enable setting */
		/* Zoom Step */
		UlH1Coefval	= ClDiCof[ 0 ] ;
			
		UcH1LvlMod = 0 ;
		
		// Limit value Value Setting
		RamWrite32A( gxlmt6L, MINLMT ) ;		/* 0x102D L-Limit */
		RamWrite32A( gxlmt6H, MAXLMT ) ;		/* 0x102E H-Limit */

		RamWrite32A( gylmt6L, MINLMT ) ;		/* 0x112D L-Limit */
		RamWrite32A( gylmt6H, MAXLMT ) ;		/* 0x112E H-Limit */

		RamWrite32A( gxhc_tmp, 	UlH1Coefval ) ;	/* 0x100E Base Coef */
		RamWrite32A( gxmg, 		CHGCOEF ) ;		/* 0x10AA Change coefficient gain */

		RamWrite32A( gyhc_tmp, 	UlH1Coefval ) ;	/* 0x110E Base Coef */
		RamWrite32A( gymg, 		CHGCOEF ) ;		/* 0x11AA Change coefficient gain */
		
		RegWriteA( WG_HCHR, 0x12 ) ;			// 0x011B	GmHChrOn[1]=1 Sw ON
		break ;
		
	case( S2MODE ):				// cancel lvl change mode 
		RegWriteA( WG_HCHR, 0x10 ) ;			// 0x011B	GmHChrOn[1]=0 Sw OFF
		break ;
		
	case( MOVMODE ):			// Movie mode 
		IniPtMovMod( ON ) ;							// Pan/Tilt setting (Movie)
		
		RamWrite32A( gxlmt6L, MINLMT_MOV ) ;	/* 0x102D L-Limit */
		RamWrite32A( gylmt6L, MINLMT_MOV ) ;	/* 0x112D L-Limit */

		RamWrite32A( gxmg, CHGCOEF_MOV ) ;		/* 0x10AA Change coefficient gain */
		RamWrite32A( gymg, CHGCOEF_MOV ) ;		/* 0x11AA Change coefficient gain */
			
		RamWrite32A( gxhc_tmp, UlH1Coefval ) ;		/* 0x100E Base Coef */
		RamWrite32A( gyhc_tmp, UlH1Coefval ) ;		/* 0x110E Base Coef */
		
		RegWriteA( WG_HCHR, 0x12 ) ;			// 0x011B	GmHChrOn[1]=1 Sw ON
		break ;
		
	default :
		IniPtMovMod( OFF ) ;							// Pan/Tilt setting (Still)
		
		UcH1LvlMod = UcSetNum ;
			
		RamWrite32A( gxlmt6L, MINLMT ) ;		/* 0x102D L-Limit */
		RamWrite32A( gylmt6L, MINLMT ) ;		/* 0x112D L-Limit */
		
		RamWrite32A( gxmg, 	CHGCOEF ) ;			/* 0x10AA Change coefficient gain */
		RamWrite32A( gymg, 	CHGCOEF ) ;			/* 0x11AA Change coefficient gain */
			
		RamWrite32A( gxhc_tmp, UlH1Coefval ) ;		/* 0x100E Base Coef */
		RamWrite32A( gyhc_tmp, UlH1Coefval ) ;		/* 0x110E Base Coef */
		
		RegWriteA( WG_HCHR, 0x12 ) ;			// 0x011B	GmHChrOn[1]=1 Sw ON
		break ;
	}
}
#endif

//********************************************************************************
// Function Name 	: RdFwVr
// Retun Value		: Firmware version
// Argment Value	: NON
// Explanation		: Read Fw Version Function
// History			: First edition 						2013.05.07 Y.Shigeoka
//********************************************************************************
unsigned short	RdFwVr( void )
{
	unsigned short	UsVerVal ;
	
	UsVerVal = (unsigned short)((MDL_VER << 8) | FW_VER ) ;
	return( UsVerVal ) ;
}
