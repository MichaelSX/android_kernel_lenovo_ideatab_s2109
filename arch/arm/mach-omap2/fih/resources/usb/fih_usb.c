/**
* USB Driver
******************************************/

#include "fih_usb.h"

#if 1
int __init usb_init(void)
{
	printk(KERN_ALERT "%s()\n",__func__);
	return 0;	
}
#else
int __init usb_init(void)
{
	return 0;	
}
#endif
