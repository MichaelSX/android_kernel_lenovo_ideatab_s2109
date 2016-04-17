#ifndef FIH_RESOURCES_H
#define FIH_RESOURCES_H


#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/gpio_event.h>
#include <asm/mach-types.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>

#include <linux/device.h>

#include <linux/i2c.h>
#include "mach/gpio_omap44xx_fih.h"

#if defined(CONFIG_FIH_WIFI)
#include "resources/wifi/fih_wifi.h"
#endif

#if defined(CONFIG_FIH_TOUCHSCREEN)
#include "resources/touchscreen/fih_touchscreen.h"
#endif

#if defined(CONFIG_FIH_BT)
#include "resources/bt/fih_bt.h"
#endif

#if defined(CONFIG_FIH_LCM)
#include "resources/lcm/fih_lcm.h"
#endif

#if defined(CONFIG_FIH_KEYPAD)
#include "resources/keypad/fih_keypad.h"
#endif

#if defined(CONFIG_FIH_LED)
#include "resources/led/fih_led.h"
#endif

#if defined(CONFIG_FIH_CAM)
#include "resources/cam/fih_cam.h"
#endif

#if defined(CONFIG_FIH_SENSORS)
#include "resources/sensors/fih_sensors.h"
#endif

#if defined(CONFIG_FIH_LAST_ALOG)
#include "resources/debug/fih_dbg.h"
#endif

#if defined(CONFIG_FIH_USB)
#include "resources/usb/fih_usb.h"
#endif

#if defined(CONFIG_FIH_BATTERY)
#include "resources/battery/fih_battery.h"
#endif

#if defined(CONFIG_FIH_BACKLIGHT)
#include "resources/backlight/fih_backlight.h"
#endif

#if defined(CONFIG_FIH_CHARGER)
#include "resources/charger/fih_charger.h"
#endif

#if defined(CONFIG_FIH_VIBRATOR)
#include "resources/vibrator/fih_vibrator.h"
#endif

#if defined(CONFIG_FIH_HW_INFO)
#include "resources/hwid/fih_hwid.h"
#endif

#if defined(CONFIG_FIH_AUDIO)
#include "resources/audio/fih_audio.h"
#endif

#if defined(CONFIG_FIH_SW_INFO)
#include "resources/swid/fih_swid.h"
#endif

#if defined(CONFIG_FIH_CDC3S04)
#include "resources/clockbuffer/fih_cdctcxo.h"
#endif

#endif /* FIH_RESOURCES_H */
