#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <ccci.h>
extern  int __init ccmni_init(void);
extern  void __exit ccmni_exit(void);
static  struct init_mod  __refdata init_mod_array[]={
	{
		.init=ccif_module_init,
		.exit=ccif_module_exit,
	},
	{
		.init=ccci_md_init_mod_init,
		.exit=ccci_md_init_mod_exit,
	},
	{
		.init=ccci_tty_init,
		.exit=ccci_tty_exit,
	},	
	{
		.init=ccci_fs_init,
		.exit=ccci_fs_exit,
	},
	{
		.init=ccci_ipc_init,
		.exit=ccci_ipc_exit,
	},
	{
		.init=ccmni_init,
		.exit=ccmni_exit,
	},	
	/*{
		.init=ccci_pmic_init,
		.exit=ccci_pmic_exit,
	},*/
	{
		.init=NULL,
		.exit=NULL,
	},
};

static int __init ccif_init(void)
{
	struct init_mod *mod_ptr=init_mod_array;
	int ret=0;
#ifdef ENABLE_CCCI_DRV_BUILDIN
	CCCI_MSG("ccci_init: device_initcall_sync\n");
#else  // MODULE
	CCCI_MSG("ccci_init: module_init\n");
#endif
	while (mod_ptr->init)
	{
		ret=mod_ptr->init();
		if (ret)  
		{
			mod_ptr--;
			break;
		}
		mod_ptr++;
	}
	if (ret)
	{
		while (1)
		{
			mod_ptr->exit();
			if (mod_ptr==init_mod_array) break;
			mod_ptr--;
		}
	}
	return ret;
}

static void __init ccif_exit(void)
{	
	struct init_mod *mod_ptr=init_mod_array;
	while (mod_ptr->exit)
	{
		mod_ptr->exit();
		mod_ptr++;
	}

}

//  Build-in Modified - S
#ifdef ENABLE_CCCI_DRV_BUILDIN
device_initcall_sync(ccif_init);
#else  // MODULE
module_init(ccif_init);
#endif
//  Build-in Modified - E
module_exit(ccif_exit);

MODULE_DESCRIPTION("CCCI Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("MTK");