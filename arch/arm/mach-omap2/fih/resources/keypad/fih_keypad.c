/**
* Keypad Driver
****************************************************/
#include "fih_keypad.h"

#if 1
int __init keypad_init(void)
{
	printk(KERN_ALERT "%s()\n",__func__);
	return 0;	
}
#else
int __init keypad_init(void)
{
	return 0;	
}
#endif