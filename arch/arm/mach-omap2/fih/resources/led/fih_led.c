/**
* LED Driver
********************************************/
#include "fih_led.h"

#if 1
int __init led_init(void)
{
	printk(KERN_ALERT "%s()\n",__func__);
	return 0;	
}
#else
int __init led_init(void)
{
	return 0;	
}
#endif
