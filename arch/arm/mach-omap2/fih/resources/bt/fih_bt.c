/**
* Broadcom BlueTooth Driver
************************************************************/
#include "fih_bt.h"

#if 1
inline int bt_init(void)
{
	printk(KERN_ALERT "%s()\n",__func__);
	return 0;	
}
#else
inline int bt_init(void)
{
	return 0;	
}
#endif