#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/printk.h>
#include "mach/gpio_omap44xx_fih.h"

int first_enable=0; 
int gox_lcm_gpio_init(void)
{
	int error;
	error = gpio_request(LCD_3V3_EN, "LCD_touch_power");
	if (error < 0) {
                pr_err("%s:failed to request GPIO %d, error %d\n",
                        __func__, LCD_3V3_EN, error);
                return error;
        }
	
	error = gpio_request(EN_VREG_LCD_V3P3, "LCD_power");
	if (error < 0) {
                pr_err("%s:failed to request GPIO %d, error %d\n",
                        __func__, EN_VREG_LCD_V3P3, error);
                return error;
        }	
	return error;
}

void gox_lcm_gpio_free(void)
{
	gpio_free(EN_VREG_LCD_V3P3);
	gpio_free(LCD_3V3_EN);
}

void gox_lcm_power_on(void)
{
	gpio_direction_output(LCD_3V3_EN, 1);		
	gpio_direction_output(EN_VREG_LCD_V3P3, 1);
}

void gox_lcm_power_off(void)
{
	gpio_direction_output(BRIDGE_RESX, 0);
	mdelay(3); 
	gpio_direction_output(EN_VREG_LCD_V3P3, 0);
	gpio_direction_output(LCD_3V3_EN, 0);			
}

int gox_lcm_power_on_check(void)
{
	if(first_enable==0)
	{
		first_enable=1;
		return 1;
	}
	return 0;	
}
