/**
* Touch Screen Driver
******************************************/

#include "fih_touchscreen.h"


#include <linux/input/ft5x0x_i2c_ts.h>
#include <plat/i2c.h>
#include <plat/hwspinlock.h>
#include <linux/delay.h>
#include <linux/gpio.h>
//#include "mux.h"

static struct i2c_board_info __initdata tablet_i2c_2_boardinfo[] = {
#ifdef CONFIG_TOUCHSCREEN_FOCALTECH_FT5606
	{
		I2C_BOARD_INFO("ft5x0x_ts", 0x38),
		//I2C_BOARD_INFO("ft5x0x_ts", 0x70),
		.platform_data = NULL,
		.irq = OMAP_GPIO_IRQ(TOUCH_INT_N),
	}
#endif
};

int __init touchscreen_init(void)
{
	int ret = 0, error = 0;

	ret = i2c_register_board_info(2, tablet_i2c_2_boardinfo, ARRAY_SIZE(tablet_i2c_2_boardinfo));

	//omap_mux_init_gpio(TOUCH_RST_N, OMAP_PIN_OUTPUT);
	error = gpio_request(TOUCH_RST_N, "ft5606_reset");
	if (error < 0) {
		pr_err("%s:failed to request GPIO %d, error %d\n",
			__func__, TOUCH_RST_N, error);
		return error;
	}
	gpio_direction_output(TOUCH_RST_N, 1);

	return ret;
}

