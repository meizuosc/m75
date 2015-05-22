#ifndef __MTK_SELINUX_WARNING_LIST_H__
#define __MTK_SELINUX_WARNING_LIST_H__

#ifdef SELINUX_WARNING_C
    #define AUTOEXT
#else //SELINUX_WARNING_C
    #define AUTOEXT  extern
#endif //SELINUX_WARNING_C

#define AEE_FILTER_NUM 10
AUTOEXT const char *aee_filter_list[AEE_FILTER_NUM] = 
{ 
"u:r:zygote:s0",
"u:r:netd:s0",
"u:r:installd:s0",
"u:r:vold:s0",
};
	
#endif
	
