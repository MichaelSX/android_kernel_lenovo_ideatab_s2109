#ifndef GPIO_OMAP44XX_GOX_B0_FIH_H
#define GPIO_OMAP44XX_GOX_B0_FIH_H



/* 000 */ #define CHRG_SYS_OK		GPIO_PIN_0
/* 001 */ 
/* 002 */ #define CHRG_STAT		GPIO_PIN_2
/* 003 */ 
/* 004 */ #define AIRQ1_INT    GPIO_PIN_4   
/* 005 */ 
/* 006 */ 
/* 007 */ 
/* 008 */ 
/* 009 */ 
/* 010 */ 
/* 011 */ 
/* 012 */ 
/* 013 */ 
/* 014 */ 
/* 015 */ #define GOX_3G_RESERVED_1		GPIO_PIN_15
/* 016 */ #define GOX_3G_RESERVED_2		GPIO_PIN_16
/* 017 */ #define GOX_3G_RADIO_ON_OFF	GPIO_PIN_17
/* 018 */ #define GOX_3G_WAN_DISABLE_N	GPIO_PIN_18
/* 019 */ 
/* 020 */ 
/* 021 */
/* 022 */ #define E_COMP_INT		GPIO_PIN_22
/* 023 */ #define TOUCH_RST_N		GPIO_PIN_23
/* 024 */ #define TOUCH_INT_N		GPIO_PIN_24
/* 025 */ #define LCDBKL_EN		GPIO_PIN_25
/* 026 */ #define E_COMP_RST_N		GPIO_PIN_26
/* 027 */
/* 028 */
/* 029 */ 
/* 030 */ 
/* 031 */ 
/* 032 */ #define EMMC_RESET_B_1V8      GPIO_PIN_32
/* 033 */ #define SOC_INT_N		GPIO_PIN_33
/* 034 */ #define BATT_GOOD_N		GPIO_PIN_34
/* 035 */ #define CABC_EN          	GPIO_PIN_35
/* 036 */ #define ALS_INT_N             GPIO_PIN_36
/* 037 */ #define CAP_INT1		GPIO_PIN_37
/* 038 */ #define CAP_INT2		GPIO_PIN_38
/* 039 */ #define BAT_LOW		GPIO_PIN_39
/* 040 */ #define BRIDGE_RESX		GPIO_PIN_40
/* 041 */ #define HDMI_LS_OE            GPIO_PIN_41
/* 042 */ 
/* 043 */ #define WLAN_IRQ		GPIO_PIN_43
/* 044 */ #define PB_POWER_ON_OMAP	GPIO_PIN_44
/* 045 */ #define CHRG_SUSP		GPIO_PIN_45
/* 046 */ #define WLANENABLE		GPIO_PIN_46
/* 047 */ #define CHRG_USB_HCS		GPIO_PIN_47
/* 048 */ 
/* 049 */ 
/* 050 */ #define VOLUME_DOWN		GPIO_PIN_50
/* 051 */ 
/* 052 */ #define USB_OTG_JACK_ID	GPIO_PIN_52	
/* 053 */ #define BT_RESETX		GPIO_PIN_53
/* 054 */ #define GPADC_START           GPIO_PIN_54
/* 055 */ #define EN_VREG_LCD_V3P3	GPIO_PIN_55
/* 056 */ #define EN_OTG_BOOST		GPIO_PIN_56
/* 057 */ 
/* 058 */
/* 059 */
/* 060 */ #define HDMI_CT_CP_HPD	GPIO_PIN_60
/* 061 */ 	
/* 062 */ #define USB_NRESET		GPIO_PIN_62
/* 063 */ 
/* 064 */         
/* 065 */ 
/* 066 */ 
/* 067 */
/* 068 */
/* 069 */
/* 070 */
/* 071 */
/* 072 */
/* 073 */
/* 074 */
/* 075 */
/* 076 */
/* 077 */
/* 078 */
/* 079 */ 
/* 079 */ 
/* 080 */ 
/* 081 */ 
/* 082 */ 
/* 083 */ 
/* 084 */ 
/* 085 */ 
/* 086 */
/* 087 */
/* 088 */
/* 089 */
/* 090 */
/* 091 */
/* 092 */
/* 093 */
/* 094 */
/* 095 */
/* 096 */
/* 097 */
/* 098 */ #define FLASH_DRV_EN		GPIO_PIN_98
/* 099 */ #define SPK_AMP_SHDN_N	GPIO_PIN_99
/* 100 */
/* 101 */
/* 102 */
/* 103 */ 
/* 104 */ #define SENSOR_LDO_EN         GPIO_PIN_104
/* 105 */ 
/* 106 */ 
/* 107 */ 
/* 108 */ 
/* 109 */ 
/* 110 */
/* 111 */
/* 112 */
/* 113 */ 
/* 114 */
/* 115 */
/* 116 */
/* 117 */
/* 118 */
/* 119 */ 
/* 120 */
/* 121 */
/* 122 */#define LCD_3V3_EN             GPIO_PIN_122
/* 123 */
/* 124 */
/* 125 */
/* 126 */
/* 127 */ #define H_AUD_PWRON		GPIO_PIN_127
/* 128 */
/* 129 */
/* 130 */
/* 131 */
/* 132 */
/* 133 */
/* 134 */
/* 135 */
/* 136 */
/* 117 */
/* 118 */
/* 119 */
/* 120 */
/* 121 */
/* 122 */
/* 123 */
/* 124 */
/* 125 */
/* 126 */
/* 127 */
/* 128 */
/* 129 */
/* 130 */
/* 131 */
/* 132 */
/* 133 */
/* 134 */
/* 135 */
/* 136 */
/* 137 */
/* 138 */
/* 139 */ 
/* 140 */
/* 141 */
/* 142 */
/* 143 */
/* 144 */
/* 145 */
/* 146 */
/* 147 */
/* 148 */
/* 149 */
/* 150 */
/* 151 */
/* 152 */
/* 153 */
/* 154 */
/* 155 */
/* 156 */
/* 157 */ 
/* 158 */ 
/* 159 */ 
/* 160 */ 
/* 161 */ 
/* 162 */ 
/* 163 */
/* 164 */
/* 165 */
/* 166 */
/* 167 */
/* 168 */
/* 169 */ 
/* 170 */ 
/* 171 */
/* 172 */ #define PCIE_CLK_REQ		GPIO_PIN_172
/* 173 */ 
/* 174 */ #define H_SIM_CD		GPIO_PIN_174
/* 175 */ 
/* 176 */ 
/* 177 */ 
/* 178 */ 
/* 179 */
/* 180 */
/* 181 */ 
/* 182 */ 
/* 183 */
/* 184 */
/* 185 */
/* 186 */
/* 187 */
/* 188 */
/* 189 */
/* 190 */
/* 191 */
/* 192 */
/* the below GPIOs just to avoid compile, these shoule be removed after owner update their code */
 

#endif/*GPIO_OMAP44XX_GOX_B0_FIH_H*/

