/** 
* Battery/Charger Driver
*****************************************************/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include "fih_hwid.h"

#ifdef CONFIG_FIH_HW_INFO
static struct platform_device fih_hwid_device = {
	.name		= "fih_hwid",
	.id		= -1,
};

int __init hwid_init(void)
{
	printk(KERN_ALERT "%s, begin\n",__func__);
	platform_device_register(&fih_hwid_device);
	printk(KERN_ALERT "%s, end\n",__func__);
	
	return 0;
}
#else	
int __init hwid_init(void)
{
	return 0;
}
#endif
