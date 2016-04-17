#ifndef GPIO_OMAP44XX_FIH_H
#define GPIO_OMAP44XX_FIH_H

#include FIH_CUST_H

enum {
    GPIO_PIN_0 = 0,
    GPIO_PIN_1,
    GPIO_PIN_2,
    GPIO_PIN_3,
    GPIO_PIN_4,
    GPIO_PIN_5,
    GPIO_PIN_6,
    GPIO_PIN_7,
    GPIO_PIN_8,
    GPIO_PIN_9,
    GPIO_PIN_10,
    GPIO_PIN_11,
    GPIO_PIN_12,
    GPIO_PIN_13,
    GPIO_PIN_14,
    GPIO_PIN_15,
    GPIO_PIN_16,
    GPIO_PIN_17,
    GPIO_PIN_18,
    GPIO_PIN_19,
    GPIO_PIN_20,
    GPIO_PIN_21,
    GPIO_PIN_22,
    GPIO_PIN_23,
    GPIO_PIN_24,
    GPIO_PIN_25,
    GPIO_PIN_26,
    GPIO_PIN_27,
    GPIO_PIN_28,
    GPIO_PIN_29,
    GPIO_PIN_30,
    GPIO_PIN_31,
    GPIO_PIN_32,
    GPIO_PIN_33,
    GPIO_PIN_34,
    GPIO_PIN_35,
    GPIO_PIN_36,
    GPIO_PIN_37,
    GPIO_PIN_38,
    GPIO_PIN_39,
    GPIO_PIN_40,
    GPIO_PIN_41,
    GPIO_PIN_42,
    GPIO_PIN_43,
    GPIO_PIN_44,
    GPIO_PIN_45,
    GPIO_PIN_46,
    GPIO_PIN_47,
    GPIO_PIN_48,
    GPIO_PIN_49,
    GPIO_PIN_50,
    GPIO_PIN_51,
    GPIO_PIN_52,
    GPIO_PIN_53,
    GPIO_PIN_54,
    GPIO_PIN_55,
    GPIO_PIN_56,
    GPIO_PIN_57,
    GPIO_PIN_58,
    GPIO_PIN_59,
    GPIO_PIN_60,
    GPIO_PIN_61,
    GPIO_PIN_62,
    GPIO_PIN_63,
    GPIO_PIN_64,
    GPIO_PIN_65,
    GPIO_PIN_66,
    GPIO_PIN_67,
    GPIO_PIN_68,
    GPIO_PIN_69,
    GPIO_PIN_70,
    GPIO_PIN_71,
    GPIO_PIN_72,
    GPIO_PIN_73,
    GPIO_PIN_74,
    GPIO_PIN_75,
    GPIO_PIN_76,
    GPIO_PIN_77,
    GPIO_PIN_78,
    GPIO_PIN_79,
    GPIO_PIN_80,
    GPIO_PIN_81,
    GPIO_PIN_82,
    GPIO_PIN_83,
    GPIO_PIN_84,
    GPIO_PIN_85,
    GPIO_PIN_86,
    GPIO_PIN_87,
    GPIO_PIN_88,
    GPIO_PIN_89,
    GPIO_PIN_90,
    GPIO_PIN_91,
    GPIO_PIN_92,
    GPIO_PIN_93,
    GPIO_PIN_94,
    GPIO_PIN_95,
    GPIO_PIN_96,
    GPIO_PIN_97,
    GPIO_PIN_98,
    GPIO_PIN_99,
    GPIO_PIN_100,
    GPIO_PIN_101,
    GPIO_PIN_102,
    GPIO_PIN_103,
    GPIO_PIN_104,
    GPIO_PIN_105,
    GPIO_PIN_106,
    GPIO_PIN_107,
    GPIO_PIN_108,
    GPIO_PIN_109,
    GPIO_PIN_110,
    GPIO_PIN_111,
    GPIO_PIN_112,
    GPIO_PIN_113,
    GPIO_PIN_114,
    GPIO_PIN_115,
    GPIO_PIN_116,
    GPIO_PIN_117,
    GPIO_PIN_118,
    GPIO_PIN_119,
    GPIO_PIN_120,
    GPIO_PIN_121,
    GPIO_PIN_122,
    GPIO_PIN_123,
    GPIO_PIN_124,
    GPIO_PIN_125,
    GPIO_PIN_126,
    GPIO_PIN_127,
    GPIO_PIN_128,
    GPIO_PIN_129,
    GPIO_PIN_130,
    GPIO_PIN_131,
    GPIO_PIN_132,
    GPIO_PIN_133,
    GPIO_PIN_134,
    GPIO_PIN_135,
    GPIO_PIN_136,
    GPIO_PIN_137,
    GPIO_PIN_138,
    GPIO_PIN_139,
    GPIO_PIN_140,
    GPIO_PIN_141,
    GPIO_PIN_142,
    GPIO_PIN_143,
    GPIO_PIN_144,
    GPIO_PIN_145,
    GPIO_PIN_146,
    GPIO_PIN_147,
    GPIO_PIN_148,
    GPIO_PIN_149,
    GPIO_PIN_150,
    GPIO_PIN_151,
    GPIO_PIN_152,
    GPIO_PIN_153,
    GPIO_PIN_154,
    GPIO_PIN_155,
    GPIO_PIN_156,
    GPIO_PIN_157,
    GPIO_PIN_158,
    GPIO_PIN_159,
    GPIO_PIN_160,
    GPIO_PIN_161,
    GPIO_PIN_162,
    GPIO_PIN_163,
    GPIO_PIN_164,
    GPIO_PIN_165,
    GPIO_PIN_166,
    GPIO_PIN_167,
    GPIO_PIN_168,
    GPIO_PIN_169,
    GPIO_PIN_170,
    GPIO_PIN_171,
    GPIO_PIN_172,
    GPIO_PIN_173,
    GPIO_PIN_174,
    GPIO_PIN_175,
    GPIO_PIN_176,
    GPIO_PIN_177,
    GPIO_PIN_178,
    GPIO_PIN_179,
    GPIO_PIN_180,
    GPIO_PIN_181,
    GPIO_PIN_182,
    GPIO_PIN_183,
    GPIO_PIN_184,
    GPIO_PIN_185,
    GPIO_PIN_186,
    GPIO_PIN_187,
    GPIO_PIN_188,
    GPIO_PIN_189,
    GPIO_PIN_190,
    GPIO_PIN_191,
    GPIO_PIN_192,
    GPIO_PIN_193,
    GPIO_PIN_194,
    GPIO_PIN_195,
    GPIO_PIN_196,
    GPIO_PIN_197,
    GPIO_PIN_198,
    GPIO_PIN_199,
};

#if( FIH_MODEL == FIHV_GO7 )
	#include "gpio_omap44xx_go7_fih.h"
#elif( FIH_MODEL == FIHV_GOX )
	#include "gpio_omap44xx_gox_fih.h"
#elif( FIH_MODEL == FIHV_JEM )
	#include "gpio_omap44xx_jem_fih.h"
#else
    #error FIH_MODEL should be defined
#endif
 

#ifdef  CONFIG_FIH_EXTERNAL_POWER_GPIO
int fih_twl6030_pwrbutton_state();
#endif


 
#endif /*GPIO_OMAP44XX_FIH_H*/
