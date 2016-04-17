#ifndef FIH_AUDIO_H
#define FIH_AUDIO_H

#include "../../fih_resources.h"


#ifdef CONFIG_FIH_AUDIO_AMPLIFIER
#include "./fih_amplifier/fih_amplifier.h"
#endif

int __init audio_init(void);

#endif /* FIH_AUDIO_H */