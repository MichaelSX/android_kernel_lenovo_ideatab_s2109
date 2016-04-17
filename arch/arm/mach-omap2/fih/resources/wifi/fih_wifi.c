/**
* Broadcom Wifi Driver
************************************************************/
#include "fih_wifi.h"

#if 1
inline int wifi_init(void)
{
	printk(KERN_ALERT "%s()\n",__func__);
	return 0;	
}
#else
inline int wifi_init(void)
{
	return 0;	
}
#endif