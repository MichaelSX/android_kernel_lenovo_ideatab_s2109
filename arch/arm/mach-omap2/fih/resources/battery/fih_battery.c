/** 
* Battery/Charger Driver
*****************************************************/

#include <linux/init.h>
#include <linux/module.h>
#include "fih_battery.h"

#if defined(CONFIG_BATTERY_BQ27520)
#include <fih/resources/battery/bq27520_battery.h>

static struct bq27520_platform_data bq27520_pdata = {
	.rom_addr = 0xB,
	.gpio_map = SOC_INT_MASK | BLO_INT_MASK | BGD_INT_MASK,
	.soc_gpio = SOC_INT_N, //gpio 33
	.blo_gpio = BAT_LOW, //gpio 39
	.bgd_gpio = BATT_GOOD_N, //gpio 34
	.use_partial = 0,
	.partial_temp_h = 0,
	.partial_temp_l = 0,
};

static struct i2c_board_info __initdata tablet_i2c_2_boardinfo[] = {
	{
		.type	= "bq27520_battery",
		.addr	= 0x55,
		.platform_data = &bq27520_pdata,
	},
};

int __init battery_init(void)
{
	printk(KERN_ALERT "%s, begin\n",__func__);
	i2c_register_board_info(2, tablet_i2c_2_boardinfo, ARRAY_SIZE(tablet_i2c_2_boardinfo));
	printk(KERN_ALERT "%s, end\n",__func__);
	
	return 0;
}
#else	
int __init battery_init(void)
{
	return 0;
}
#endif