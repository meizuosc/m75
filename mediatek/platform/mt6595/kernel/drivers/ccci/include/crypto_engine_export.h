#ifndef _CRYPTO_ENGINE_EXPORT_H_
#define _CRYPTO_ENGINE_EXPORT_H_

// ========================================================
// CHIP SELECTION
// ========================================================
#ifdef CONFIG_ARCH_MT6516
#include <mach/mt6516_typedefs.h>
#elif defined CONFIG_ARCH_MT6573
#include <mach/mt6573_typedefs.h>
#elif defined CONFIG_ARCH_MT6575
#include <mach/mt6575_typedefs.h>
#else
#error crypto enfine is not supported in this chip !
#endif

// ========================================================
// CRYPTO ENGINE EXPORTED API
// ========================================================

/* perform crypto operation
   @ Direction   : TRUE  (1) means encrypt
                   FALSE (0) means decrypt
   @ ContentAddr : input source address
   @ ContentLen  : input source length
   @ CustomSeed  : customization seed for crypto engine
   @ ResText     : output destination address */
extern void SST_Secure_Algo(kal_uint8 Direction, kal_uint32 ContentAddr, kal_uint32 ContentLen, kal_uint8 *CustomSeed, kal_uint8 *ResText);

/* return the result of hwEnableClock ( )
   - TRUE  (1) means crypto engine init success
   - FALSE (0) means crypto engine init fail    */
extern BOOL SST_Secure_Init(void);

/* return the result of hwDisableClock ( )
   - TRUE  (1) means crypto engine de-init success
   - FALSE (0) means crypto engine de-init fail    */
extern BOOL SST_Secure_DeInit(void);

#endif 

