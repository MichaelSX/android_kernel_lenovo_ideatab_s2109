#ifndef GPIO_OMAP44XX_GOX_FIH_H
#define GPIO_OMAP44XX_GOX_FIH_H

#include FIH_CUST_H

#if( FIH_MODEL_PCB_REV >= FIHV_GOX_B0 )
  #include "gpio_omap44xx_gox_b0_fih.h"
#else
  #error FIH_MODEL_PCB_REV should be defined!!
#endif


#endif /*GPIO_OMAP44XX_GOX_FIH_H*/
