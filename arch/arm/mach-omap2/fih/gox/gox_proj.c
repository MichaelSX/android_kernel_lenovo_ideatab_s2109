#include "gox_proj.h"

int __init GOX_init(void)
{
	printk(KERN_ALERT "%s, begin\n",__func__);

#ifdef CONFIG_FIH_LAST_ALOG
	printk(KERN_ALERT "************************************** ram_console_init \n");
	ram_console_init();
	printk(KERN_ALERT "************************************** ram_console_init \n");
	printk(KERN_ALERT "************************************** last_alog_init \n");
	last_alog_init();
	printk(KERN_ALERT "************************************** last_alog_init \n");
#endif	

#ifdef CONFIG_FIH_LCM
	printk(KERN_ALERT "************************************** remove fih_fb_device_init \n");
//	fih_fb_device_init();
	printk(KERN_ALERT "************************************** remove fih_fb_device_init \n");
#endif

#ifdef CONFIG_FIH_TOUCHSCREEN
	printk(KERN_ALERT "************************************** touchscreen_init \n");
	touchscreen_init();
	printk(KERN_ALERT "************************************** touchscreen_init \n");
#endif

#ifdef CONFIG_FIH_USB
	printk(KERN_ALERT "************************************** usb_init \n");
	usb_init();
	printk(KERN_ALERT "************************************** usb_init \n");
#endif

#ifdef CONFIG_FIH_LED
	printk(KERN_ALERT "************************************** led_init \n");
	led_init();
	printk(KERN_ALERT "************************************** led_init \n");
#endif

#ifdef CONFIG_FIH_KEYPAD
	printk(KERN_ALERT "************************************** keypad_init \n");
	keypad_init();
	printk(KERN_ALERT "************************************** keypad_init \n");
#endif

#ifdef CONFIG_FIH_BATTERY
	printk(KERN_ALERT "************************************** battery_init \n");
	battery_init();
	printk(KERN_ALERT "************************************** battery_init \n");
#endif
	
#ifdef CONFIG_FIH_BACKLIGHT
	printk(KERN_ALERT "************************************** backlight_init \n");
	backlight_init();
	printk(KERN_ALERT "************************************** backlight_init \n");
#endif

#ifdef CONFIG_FIH_CHARGER
	printk(KERN_ALERT "************************************** charger_init \n");
	charger_init();
	printk(KERN_ALERT "************************************** charger_init \n");
#endif

#ifdef CONFIG_FIH_SENSORS
	printk(KERN_ALERT "************************************** sensors_init \n");
	sensors_init();
	printk(KERN_ALERT "************************************** sensors_init \n");
#endif

#ifdef CONFIG_FIH_BT
	printk(KERN_ALERT "************************************** bt_init \n");
	bt_init();
	printk(KERN_ALERT "************************************** bt_init \n");
#endif

#ifdef CONFIG_FIH_WIFI
	printk(KERN_ALERT "************************************** wifi_init \n");
	wifi_init();
	printk(KERN_ALERT "************************************** wifi_init \n");
#endif

#ifdef CONFIG_FIH_VIBRATOR
	printk(KERN_ALERT "************************************** vibrator_init \n");
	vibrator_init();
	printk(KERN_ALERT "************************************** vibrator_init \n");
#endif

#ifdef CONFIG_FIH_CAM
	printk(KERN_ALERT "************************************** camera_init \n");
	camera_init();
	printk(KERN_ALERT "************************************** camera_init \n");
#endif

#ifdef CONFIG_FIH_HW_INFO
	printk(KERN_ALERT "************************************** hwid_init \n");
	hwid_init();
	printk(KERN_ALERT "************************************** hwid_init \n");
#endif

#ifdef CONFIG_FIH_AUDIO
	printk(KERN_ALERT "************************************** audio_init \n");
	audio_init();
	printk(KERN_ALERT "************************************** audio_init \n");
#endif

#ifdef CONFIG_FIH_SW_INFO
	printk(KERN_ALERT "************************************** swid_init \n");
	swid_init();
	printk(KERN_ALERT "************************************** swid_init \n");
#endif

#ifdef CONFIG_FIH_CDC3S04
	printk(KERN_ALERT "************************************** cdctcxo_init \n");
	cdctcxo_init();
	printk(KERN_ALERT "************************************** cdctcxo_init \n");
#endif

	printk(KERN_ALERT "%s, end\n",__func__);

	return 0;
}
