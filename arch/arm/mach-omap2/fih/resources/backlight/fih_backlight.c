/** 
* Backlight Driver
*****************************************************/

#include <linux/init.h>
#include <linux/module.h>
#include "fih_backlight.h"

#ifdef CONFIG_BACKLIGHT_TPS61181
#include <fih/resources/backlight/tps61181.h>

static struct tps61181_platform_data tps61181_pdata = {
	.bl_en_gpio = LCDBKL_EN, //gpio 25
	.pwm_timer = 10, //omap timer10
	.source_sel = OMAP_TIMER_SRC_SYS_CLK, //this must match source_freq
	.source_freq = 38400000,

	.pwm_freq = 1000,
	.bl_max_level = 255,

};

static struct platform_device tps61181_backlight_device = {
    .name = "backlight",
    .id = -1,
    .dev = {
    	.platform_data = &tps61181_pdata,
    },
};

static struct platform_device *blazetablet_devices[] __initdata = {
	&tps61181_backlight_device,
};

int __init backlight_init(void)
{
	printk(KERN_ALERT "%s, begin\n",__func__);
	platform_add_devices(blazetablet_devices, ARRAY_SIZE(blazetablet_devices));
	printk(KERN_ALERT "%s, end\n",__func__);
	
	return 0;
}
#else	
int __init backlight_init(void)
{
	return 0;
}
#endif
