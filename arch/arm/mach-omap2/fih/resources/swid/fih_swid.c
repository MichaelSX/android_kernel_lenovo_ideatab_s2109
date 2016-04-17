/** 
* SW MODEL Driver
*****************************************************/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include "fih_swid.h"

#ifdef CONFIG_FIH_SW_INFO
static struct platform_device fih_swid_device = {
	.name		= "fih_swid",
	.id		= -1,
};

int __init swid_init(void)
{
	printk(KERN_ALERT "%s, begin\n",__func__);
	platform_device_register(&fih_swid_device);
	printk(KERN_ALERT "%s, end\n",__func__);
	
	return 0;
}
#else	
int __init swid_init(void)
{
	return 0;
}
#endif
