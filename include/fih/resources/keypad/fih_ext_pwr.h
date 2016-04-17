#ifndef FIH_EXT_PWR_H
#define FIH_EXT_PWR_H

/* SW5, PinyCHWu, 20111125, Add for external gpio power key, { */
#ifdef CONFIG_FIH_EXTERNAL_POWER_GPIO
struct twl6030_ext_pwrbutton { 
	int ext_gpio;
};
void fih_parse_twldata(void *twldata);
#else
#define twl6030_ext_pwrbutton	NULL
#define fih_parse_twldata	NULL
#endif
/* }, SW5, PinyCHWu, 20111125 */

#endif /* FIH_EXT_PWR_H */
