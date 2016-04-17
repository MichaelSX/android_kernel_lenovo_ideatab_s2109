#ifndef FIH_SENSORS_H
#define FIH_SENSORS_H

#include "../../fih_resources.h"


#ifdef CONFIG_FIH_LIGHT_SENSORS
#include "./fih_light_sensor/fih_light_sensors.h"
#endif

#ifdef CONFIG_SENSORS_LM75
#include "./fih_temp_sensor/fih_temp_sensors.h"
#endif

int __init sensors_init(void);

#endif /* FIH_LED_H */