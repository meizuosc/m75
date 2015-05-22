#include <asm/types.h>
#include <linux/atomic.h>
#include <linux/mm.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/kthread.h>
#include <linux/audit.h>
#include <net/sock.h>
#include <net/netlink.h>
#include <linux/skbuff.h>
#include <linux/netlink.h>
#include <linux/freezer.h>
#include <linux/tty.h>
#include <linux/platform_device.h>
#include <linux/module.h>

#ifdef CONFIG_MTK_AEE_FEATURE
#include <linux/aee.h>
#endif

#define SELINUX_WARNING_C
#include <mtk_selinux_warning_list.h>  //locate at custom/kernel/seplolicy
#undef SELINUX_WARNING_C

#define AEE_FILTER_LEN 50

#ifdef CONFIG_SECURITY_SELINUX_DEVELOP
extern int selinux_enforcing;
#else
#define selinux_enforcing 1
#endif



static int   mtk_check_filter(char * scontext);
static int   mtk_get_scontext(char *data, char * buf);
static char* mtk_get_process(char * in);
void  mtk_audit_hook(char *data);




static int mtk_check_filter(char * scontext)
{
    int i = 0 ;
      
    /*check whether scontext in filter list */ 	    
    for( i= 0 ;  i < AEE_FILTER_NUM && aee_filter_list[i]!=NULL   ; i++){
       if( strcmp(scontext,aee_filter_list[i]) == 0 ){
            return i;
       }
    }

    return -1;
}

static int mtk_get_scontext(char *data, char * buf)
{
    char * t1;
    char * t2;
    int diff = 0;

    t1 = strstr(data,"scontext=");

    if(t1 == NULL)
        return 0;

    t1+=9;
    t2 = strchr(t1,' ');

    if(t2 == NULL)
        return 0;

    diff = t2-t1;
    strncpy(buf,t1,diff); 

    return 1;
        
}

static char* mtk_get_process(char * in)
{
    char *out = in;
	char *tmp;
	int i;

	/*Omit two ':'*/
	for(i = 0; i < 2; i++)
		out = strchr(out, ':') + 1;
	
	if( out == NULL )
		return 0;
	
	tmp = strchr(out, ':');
	
	if( tmp == NULL)
		return 0;
		
	*tmp = '\0';	
    return out;
}

 void mtk_audit_hook(char *data)
{
    char scontext[AEE_FILTER_LEN] = {'\0'};
    char *pname = scontext;
    char printBuf[AEE_FILTER_LEN]  = {'\0'};

    	
    int ret = 0;
    
	/*get scontext from avc warning*/
    ret = mtk_get_scontext(data,scontext);
	if(!ret)
	    return ;
    /*check scontext is in warning list*/
	ret = mtk_check_filter(scontext);
    if(ret >= 0)
    {                          
		pname = mtk_get_process(scontext);
		if(pname !=0)
		{
			printk("[selinux]Enforce: %d, In AEE Warning List scontext: %s\n",selinux_enforcing ,pname); 
			sprintf(printBuf,"Selinux Enforce violation: %s ",pname);
			#ifdef CONFIG_MTK_AEE_FEATURE
			if(selinux_enforcing)
				aee_kernel_warning(printBuf, "\nCR_DISPATCH_PROCESSNAME:%s\n%s",pname,data);
			#endif
		}
    }
}
EXPORT_SYMBOL(mtk_audit_hook);


