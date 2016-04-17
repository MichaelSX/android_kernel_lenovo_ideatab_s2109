/** 
* Audio Driver
****************************************************/
#include "fih_audio.h"

int __init audio_init(void)
{

#ifdef CONFIG_FIH_AUDIO_AMPLIFIER	
	amplifier_init();
#endif		

	return 0;
}