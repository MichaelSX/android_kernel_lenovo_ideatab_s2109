/** 
* Amplifier Driver
****************************************************/
#include <linux/init.h>
#include <linux/module.h>
#include "fih_amplifier.h"

#ifdef CONFIG_AMPLIFIER_TPA2026D2
static struct tpa2026d2_platform_data tpa2026d2_pdata = {
	.power_gpio = 142,
	.boost_gpio = 98,
};

static struct i2c_board_info __initdata tablet_i2c_4_boardinfo[] = {
	{
		I2C_BOARD_INFO("tpa2026d2", 0x58),
		.platform_data = &tpa2026d2_pdata,
	},
};

int __init amplifier_init(void)
{
	printk(KERN_ALERT "%s, begin\n",__func__);
	i2c_register_board_info(4, tablet_i2c_4_boardinfo, ARRAY_SIZE(tablet_i2c_4_boardinfo));
	printk(KERN_ALERT "%s, end\n",__func__);    
	
	return 0;
}
#else	
int __init amplifier_init(void)
{
	return 0;
}
#endif