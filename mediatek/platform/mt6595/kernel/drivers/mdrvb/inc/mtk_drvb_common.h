#ifndef __MTK_DRVB_COMMON_H
#define __MTK_DRVB_COMMON_H    

/**************************************************************************
 *  DEBUG CONTROL 
 **************************************************************************/
#ifdef DEBUG_
#define MSG(format, arg...) printk(KERN_INFO "[MTK Driver Base] " format "\n", ## arg)
#define hexdump(buf, len) print_hex_dump(KERN_CONT, "", DUMP_PREFIX_OFFSET, 16, 1, buf, len, false)

#else
#define MSG(format, arg...)
#define hexdump(format, arg...)
#endif

#endif
