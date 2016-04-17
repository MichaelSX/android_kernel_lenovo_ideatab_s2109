#ifndef FIH_AMPLIFIER_H
#define FIH_AMPLIFIER_H

#include "../../../fih_resources.h"

#ifdef CONFIG_AMPLIFIER_TPA2026D2
	#include <fih/resources/audio/fih_amplifier/tpa2026d2.h>
	#include <fih/resources/audio/fih_amplifier/tpa2026d2-plat.h>
#endif

int __init amplifier_init(void);

#endif 
