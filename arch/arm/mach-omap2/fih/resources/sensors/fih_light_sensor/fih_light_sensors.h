#ifndef FIH_LIGHT_SENSORS_H
#define FIH_LIGHT_SENSORS_H

#include "../../../fih_resources.h"

#ifdef CONFIG_INPUT_CM3202_ALS
#include <fih/resources/sensors/fih_light_sensor/cm3202.h>
#endif

int __init light_sensors_init(void);

#endif 
