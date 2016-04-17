/*
 * Driver for backlight with TPS61181 chips.
 *
 * Copyright (C) 2011 FOXCONN, INC.
 *
 * Derived from kernel/drivers/video/backlight/tps61181.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Author:  PinyCHWu <pinychwu@fih-foxconn.com>
 *	    September 2011
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <plat/board.h>
#include <fih/resources/backlight/tps61181.h>

#define TIMERVALUE_OVERFLOW    0xFFFFFFFF

struct tps61181_backlight_data {
	struct work_struct	work;
	struct omap_dm_timer	*timer;
	unsigned int		source_sel;
	unsigned long		timer_range;
	unsigned long		timer_reload;
	int			bl_en_gpio;
	int			bl_max_level;	//max real backlight level
	int			bl_level_set;	//want to set backlight level
	int			bl_level;	//current backlight level
	int			enabled;
};


#if 0
static void tps61181_set_pwm(struct tps61181_backlight_data *bdata)
{
	unsigned long cmp_value = 0;

	/* Set the new brightness */
	if (bdata->bl_level_set == bdata->bl_max_level) {
		cmp_value = TIMERVALUE_OVERFLOW+1;
	}
	else {
		cmp_value = ((bdata->timer_range * bdata->bl_level_set) / bdata->bl_max_level) + bdata->timer_reload;
	}
	printk(KERN_DEBUG "[%s] backlight pwm cmp val=%lx\n", __func__, cmp_value);

	if (bdata->bl_level_set) {
		omap_dm_timer_enable(bdata->timer);
		if (bdata->bl_level)
			clk_disable(bdata->timer->fclk);
		omap_dm_timer_set_source(bdata->timer, bdata->source_sel);
		clk_enable(bdata->timer->fclk);
		omap_dm_timer_set_prescaler(bdata->timer, 0);
		omap_dm_timer_start(bdata->timer);
		omap_dm_timer_set_match(bdata->timer, 1, cmp_value);
		omap_dm_timer_set_load(bdata->timer, 1, bdata->timer_reload);
		omap_dm_timer_set_pwm(bdata->timer, 0, 1, OMAP_TIMER_TRIGGER_OVERFLOW_AND_COMPARE);
	}
	else {
		omap_dm_timer_stop(bdata->timer);
		omap_dm_timer_disable(bdata->timer);
		clk_disable(bdata->timer->fclk);
	}
	bdata->bl_level = bdata->bl_level_set;
}

static void tps61181_control_gpio(struct tps61181_backlight_data *bdata)
{
	if (bdata->bl_level_set && !bdata->enabled) {
		printk("[%s] Enable TPS61181\n", __func__);
		if (bdata->bl_en_gpio) {
			gpio_direction_output(bdata->bl_en_gpio, 1);
			mdelay(1);
			gpio_set_value(bdata->bl_en_gpio, 1);
		}
		bdata->enabled = 1;
	} else if (!bdata->bl_level_set && bdata->enabled){
		printk("[%s] Disable TPS61181\n", __func__);
		if (bdata->bl_en_gpio) {
			gpio_direction_output(bdata->bl_en_gpio, 1);
			mdelay(1);
			gpio_set_value(bdata->bl_en_gpio, 0);
		}
		bdata->enabled = 0;
	}
}

static void tps61181_backlight_work(struct work_struct *work)
{
	struct tps61181_backlight_data *bdata = container_of(work, struct tps61181_backlight_data, work);

	tps61181_control_gpio(bdata);
	tps61181_set_pwm(bdata);
}
#endif

static void tps61181_set_backlight(struct tps61181_backlight_data *bdata)
{
	unsigned long cmp_value = 0;

	/* Set the new brightness */
	if (bdata->bl_level_set == bdata->bl_max_level) {
		cmp_value = TIMERVALUE_OVERFLOW+1;
	}
	else {
		cmp_value = ((bdata->timer_range * bdata->bl_level_set) / bdata->bl_max_level) + bdata->timer_reload;
	}
	printk(KERN_DEBUG "[%s] backlight pwm cmp val=%lx\n", __func__, cmp_value);

	if (bdata->bl_level_set) {
		if(!bdata->enabled)
		{
			if (bdata->bl_en_gpio) 
			{
				gpio_direction_output(bdata->bl_en_gpio, 1);
				mdelay(1);
				gpio_set_value(bdata->bl_en_gpio, 1);
			}
			bdata->enabled = 1;

			omap_dm_timer_enable(bdata->timer);
			if (bdata->bl_level)
				clk_disable(bdata->timer->fclk);
			omap_dm_timer_set_source(bdata->timer, bdata->source_sel);
			clk_enable(bdata->timer->fclk);
			omap_dm_timer_set_prescaler(bdata->timer, 0);
			omap_dm_timer_start(bdata->timer);
		
			omap_dm_timer_set_load(bdata->timer, 1, bdata->timer_reload);
			omap_dm_timer_set_pwm(bdata->timer, 0, 1, OMAP_TIMER_TRIGGER_OVERFLOW_AND_COMPARE);
		}
		omap_dm_timer_set_match_fih(bdata->timer, 1, cmp_value);
	}
	else {
		omap_dm_timer_stop(bdata->timer);
		omap_dm_timer_disable(bdata->timer);
		clk_disable(bdata->timer->fclk);
		if(bdata->enabled)
		{
			if (bdata->bl_en_gpio) 
			{
				gpio_direction_output(bdata->bl_en_gpio, 1);
				mdelay(1);
				gpio_set_value(bdata->bl_en_gpio, 0);
			}
			bdata->enabled = 0;
		}
	}
	bdata->bl_level = bdata->bl_level_set;
}


static int tps61181_backlight_update_status(struct backlight_device *bd)
{
	struct tps61181_backlight_data *bdata = bl_get_data(bd);
	int brightness = bd->props.brightness;
	int bl_level = 0;

	//brightness part
	bl_level = (2 * brightness * bdata->bl_max_level + bd->props.max_brightness) 
			/ (2 *bd->props.max_brightness);

	if (brightness > 0 && bl_level == 0) {
		bl_level = 1;
	} else if (bl_level > bdata->bl_max_level) {
		bl_level = bdata->bl_max_level;
	}

	//state part
	if ((bd->props.state & (BL_CORE_SUSPENDED | BL_CORE_FBBLANK))  || bd->props.power) {
		printk(KERN_DEBUG "[%s] state:0x%X, power:%d\n", __func__, bd->props.state, bd->props.power);
		bl_level = 0;
	}

	//result part
	if (bl_level != bdata->bl_level) {
		printk(KERN_DEBUG "[%s] update bl_level: %d\n", __func__, bl_level);
		bdata->bl_level_set = bl_level;

		tps61181_set_backlight(bdata);
		//schedule_work(&bdata->work);
	
	}

	return 0;
}

static int tps61181_backlight_get_brightness(struct backlight_device *bd)
{
	struct tps61181_backlight_data *bdata = bl_get_data(bd);
	return bdata->bl_level;
}

static const struct backlight_ops tps61181_backlight_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.update_status	= tps61181_backlight_update_status,
	.get_brightness	= tps61181_backlight_get_brightness,
};

static int tps61181_backlight_probe(struct platform_device *pdev)
{
	struct tps61181_platform_data *pdata = pdev->dev.platform_data;
	struct tps61181_backlight_data *bdata = NULL;
	struct backlight_device *bd = NULL;
	struct backlight_properties props;
	int    ret = 0;

	printk("[%s] TPS61181 backlight probe\n", __func__);

	if (!pdata) {
		printk(KERN_ERR "[%s] No platform data for backlight device\n", __func__);
		return -EINVAL;
	}

	bdata = kzalloc(sizeof(struct tps61181_backlight_data), GFP_KERNEL);
	if (!bdata) {
		printk(KERN_ERR "[%s] No memory for backlight device\n", __func__);
		return -ENOMEM;
	}

	if (pdata->pwm_timer && pdata->source_freq && pdata->pwm_freq) {
		bdata->timer = omap_dm_timer_request_specific(pdata->pwm_timer);
		if (bdata->timer == NULL) {
			kfree(bdata);
			printk(KERN_ERR "[%s] failed to request pwm timer\n", __func__);
			return -ENODEV;
		}
		bdata->timer_range = (pdata->source_freq / (pdata->pwm_freq * 2));
		bdata->timer_reload = TIMERVALUE_OVERFLOW - bdata->timer_range;
		bdata->source_sel = pdata->source_sel;
	}

	if (pdata->bl_en_gpio) {
		bdata->bl_en_gpio = pdata->bl_en_gpio;
		ret = gpio_request(bdata->bl_en_gpio, "backlight_en_gpio");
		if (ret != 0) {
			printk(KERN_ERR "[%s] backlight enable gpio (%d) request failed\n", __func__, pdata->bl_en_gpio);
			bdata->bl_en_gpio = 0;
		}
	}

	if (pdata->bl_max_level) {
		bdata->bl_max_level = pdata->bl_max_level;
		props.max_brightness = 255;
		props.brightness = 0; //default
		props.power = 0;
		props.type = BACKLIGHT_PLATFORM;
		bd = backlight_device_register("tps61181", &pdev->dev, bdata, &tps61181_backlight_ops, &props);
		if (IS_ERR(bd)) {
			printk(KERN_ERR "[%s] failed to register backlight device\n", __func__);
			omap_dm_timer_free(bdata->timer);
			kfree(bdata);
			return PTR_ERR(bd);
		}
	}

//	INIT_WORK(&bdata->work, tps61181_backlight_work);


	platform_set_drvdata(pdev, bd);
	printk("[%s] First turn on backlight chip\n", __func__);
	bd->props.brightness = 255;
	tps61181_backlight_update_status(bd);
	
	return 0;
}

static int tps61181_backlight_remove(struct platform_device *pdev)
{
	struct backlight_device *bd = platform_get_drvdata(pdev);
	struct tps61181_backlight_data *bdata = bl_get_data(bd);

	printk("[%s] TPS61181 backlight remove\n", __func__);
	cancel_work_sync(&bdata->work);
	backlight_device_unregister(bd);
	omap_dm_timer_free(bdata->timer);
	kfree(bdata);
	return 0;
}

static struct platform_driver tps61181_backlight_driver = {
	.driver		= {
		.name	= "backlight",
		.owner	= THIS_MODULE,
	},
	.probe		= tps61181_backlight_probe,
	.remove		= tps61181_backlight_remove,
};

static int __init tps61181_backlight_init(void)
{
	return platform_driver_register(&tps61181_backlight_driver);
}

static void __exit tps61181_backlight_exit(void)
{
	platform_driver_unregister(&tps61181_backlight_driver);
}

module_init(tps61181_backlight_init);
module_exit(tps61181_backlight_exit);
MODULE_DESCRIPTION("Backlight driver for TPS61181 chip");
MODULE_AUTHOR("PinyCHWu <pinychwu@fih-foxconn.com>");
MODULE_LICENSE("GPL");
