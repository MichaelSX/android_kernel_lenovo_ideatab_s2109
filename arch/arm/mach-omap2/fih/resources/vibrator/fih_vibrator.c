/**
* Vibrator driver
**************************************/
#include "fih_vibrator.h"

#if 1
int __init vibrator_init(void)
{
	printk(KERN_ALERT "%s()\n",__func__);
	return 0;	
}
#else
int __init vibrator_init(void)
{
	return 0;	
}
#endif