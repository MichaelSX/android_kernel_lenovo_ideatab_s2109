#ifndef TPS61181_H
#define TPS61181_H

#include <plat/dmtimer.h>

struct tps61181_platform_data {
	int 		bl_en_gpio;
	int 		lcd_en_gpio;
	int 		pwm_timer;
	unsigned int	source_sel;
	unsigned int	source_freq;
	unsigned int	pwm_freq;
	int		bl_max_level;
};

#endif
