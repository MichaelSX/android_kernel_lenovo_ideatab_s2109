/*************************************************** 
* 20111017, Robert Hu, GOX TempSensor, TMP105
****************************************************/
#include "fih_temp_sensors.h"

#ifdef CONFIG_SENSORS_LM75 
//pubile include file 
#include <linux/mpu.h>
#include <plat/i2c.h>
#include <linux/delay.h>

#define DEBUGD
#ifdef DEBUGD
  #define dprintk(x...) printk(x)
#else
  #define dprintk(x...)
#endif //DEBUGD

static struct i2c_board_info __initdata tempsensor_i2c_4_boardinfo[] = 
{
  {
   I2C_BOARD_INFO("tmp105", 0x48),
  },
};


static int tempsensor_i2c_init(void)
{
  int ret=0;
  ret = i2c_register_board_info(4, tempsensor_i2c_4_boardinfo, ARRAY_SIZE(tempsensor_i2c_4_boardinfo));		
  return ret;
}


int __init temp_sensors_init(void)
{
  int ret=0;  
  dprintk("%s: begin\n",__func__);
  ret=tempsensor_i2c_init();  
  if (ret == 0)
    dprintk("%s: SUCCESS !\n",__func__);
  else
    dprintk("%s: FAIL ! error code = 0x%x\n", __func__, ret);  
  dprintk("%s: end\n",__func__);
  return ret;
}

#endif //CONFIG_SENSORS_LM75
