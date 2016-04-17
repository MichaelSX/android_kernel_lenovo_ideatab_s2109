#include "go9_proj.h"

int __init GO9_init(void)
{
	printk(KERN_ALERT "%s, begin\n",__func__);
	
#ifdef CONFIG_FIH_DEBUG
	ram_console_init();
	last_alog_init();
#endif	

#ifdef CONFIG_FIH_LCM
	fih_fb_device_init();
#endif

#ifdef CONFIG_FIH_TOUCHSCREEN
	touchscreen_init();
#endif

#ifdef CONFIG_FIH_USB
	usb_init();
#endif

#ifdef CONFIG_FIH_LED
	led_init();
#endif

#ifdef CONFIG_FIH_KEYPAD
	keypad_init();
#endif

#ifdef CONFIG_FIH_BATTERY
	battery_init();
#endif
	
#ifdef CONFIG_FIH_BACKLIGHT
	backlight_init();
#endif

#ifdef CONFIG_FIH_SENSORS
	sensors_init();
#endif

#ifdef CONFIG_FIH_BT
	bt_init();
#endif

#ifdef CONFIG_FIH_WIFI
	wifi_init();
#endif

#ifdef CONFIG_FIH_VIBRATOR
	vibrator_init();
#endif

#ifdef CONFIG_FIH_CAM
	camera_init();
#endif

	printk(KERN_ALERT "%s, end\n",__func__);

	return 0;
}
