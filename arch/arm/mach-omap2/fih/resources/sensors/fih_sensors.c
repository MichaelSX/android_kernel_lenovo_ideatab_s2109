/** 
* Sensors Driver
****************************************************/
#include "fih_sensors.h"


#include <linux/mpu.h>
#include <plat/i2c.h>
#include <plat/hwspinlock.h>
#include <linux/delay.h>


#ifdef CONFIG_MPU_SENSORS_MPU3050
static struct mpu_platform_data mpu3050_data = {
	.int_config = 0x10,
	.orientation =   
		{ 0, 1, 0,
		  -1, 0, 0,
		  0, 0, 1},
	.level_shifter = 0,
	.accel = {
//		.irq = OMAP_GPIO_IRQ(ACCEL_INT),
		.get_slave_descr = bma250_get_slave_descr, 
		.adapt_num = 4,
		.bus = EXT_SLAVE_BUS_SECONDARY,
		.address = 0x18,  
		.orientation =    
			{ 1, 0, 0,
			  0, 1, 0,
			  0, 0, 1},
		},
	.compass = {
		.get_slave_descr = yas530_get_slave_descr, 
//		.irq = OMAP_GPIO_IRQ(E_COMP_INT),
		.adapt_num = 4,
		.bus = EXT_SLAVE_BUS_PRIMARY,
		.address = 0x2E,  //YAS530
		.orientation =    
			{ 1, 0, 0,
			  0, 1, 0,
			  0, 0, 1},
		},		
};
#endif

static struct i2c_board_info __initdata tablet_i2c_4_boardinfo[] = {
#ifdef CONFIG_MPU_SENSORS_MPU3050	
	{
		I2C_BOARD_INFO("mpu3050", 0x68),
		.irq = OMAP_GPIO_IRQ(AIRQ1_INT), 
		.platform_data = &mpu3050_data,
	},
#endif	
};

#ifdef CONFIG_MPU_SENSORS_YAS530
static void omap_yas530_init(void) 
{
	if (gpio_request(E_COMP_RST_N, "yas530_rst_n") < 0) {
		pr_err("%s: YAS530 GPIO request failed\n", __func__);
		return;
	}

	gpio_direction_output(E_COMP_RST_N, 1);
	gpio_set_value(E_COMP_RST_N, 0);
	mdelay(1);
	gpio_set_value(E_COMP_RST_N, 1);
	gpio_free(E_COMP_RST_N);
}
#endif
int __init sensors_init(void)
{
	int ret = 0;
	ret = i2c_register_board_info(4, tablet_i2c_4_boardinfo, ARRAY_SIZE(tablet_i2c_4_boardinfo));		

#ifdef CONFIG_MPU_SENSORS_YAS530		
	omap_yas530_init();
#endif	

#ifdef CONFIG_FIH_LIGHT_SENSORS	
	light_sensors_init();
#endif		

#ifdef CONFIG_SENSORS_LM75
	temp_sensors_init();
#endif	
	return ret;
}


