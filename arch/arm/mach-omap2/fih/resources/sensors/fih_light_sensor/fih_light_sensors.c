/** 
* Sensors Driver
****************************************************/
#include "fih_light_sensors.h"

#ifdef CONFIG_FIH_LIGHT_SENSORS 
//pubile include file 
#include <linux/mpu.h>
#include <plat/i2c.h>
#include <linux/delay.h>

//#define DEBUGD

#ifdef DEBUGD
  #define dprintk(x...) printk(x)
#else
  #define dprintk(x...)
#endif //DEBUGD

#endif //CONFIG_FIH_LIGHT_SENSORS

#ifdef CONFIG_INPUT_CM3202_ALS
/*---------------------------------
CM3202 sensor
   
  PIN 2 sensor Shutdown(SD) Pin
  PIN 5 Analog voltage outout 0~1.8V
  
SD PIN -->  GPIO_36
  
PIN 
  high  sensor shutdown
  low   sensor active  
------------------------------------*/

static struct cm3202_platform_data cm3202_data;

static struct platform_device cm3202_platform_device = {
  .name = CM3202_NAME,
  .id       = -1,
  .dev  = {
          .platform_data = &cm3202_data,
  },
};

static struct platform_device *blazetablet_devices[] __initdata = {
  &cm3202_platform_device,
};

static int cm3202_sensors_init(void) 
{
  int ret=0;
  
  dprintk("%s:**************CM3202_light_sensor_init\n",__func__);
  ret=gpio_request(ALS_INT_N, "ALS ENABLE PIN");
  if(ret<0)
  {
    dprintk("%s: CM3202 GPIO request failed\n", __func__);
    return ret;
  }
  
  gpio_direction_output(ALS_INT_N, 1);
  gpio_set_value(ALS_INT_N, 0); 
  gpio_free(ALS_INT_N);
  return ret;
}
#endif //CONFIG_INPUT_CM3202_ALS

int __init light_sensors_init(void)
{
  int ret=0;  
  dprintk("%s: light_sensor_init\n",__func__);

#ifdef CONFIG_INPUT_CM3202_ALS
  platform_add_devices(blazetablet_devices, ARRAY_SIZE(blazetablet_devices));
  ret=cm3202_sensors_init();
#endif  
    
  return ret;
}


