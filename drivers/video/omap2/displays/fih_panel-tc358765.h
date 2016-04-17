#ifndef __FIH_PANEL_TC358765_H__
#define __FIH_PANEL_TC358765_H__

#define TC358765_WIDTH		1024
#define TC358765_HEIGHT		768
#define TC358765_PCLK		65183
#define TC358765_HFP		24
#define TC358765_HSW		136
#define TC358765_HBP		160
#define TC358765_VFP		3
#define TC358765_VSW		6
#define TC358765_VBP		29


int gox_lcm_gpio_init(void);
void gox_lcm_gpio_free(void);
void gox_lcm_power_on(void);
void gox_lcm_power_off(void);
int gox_lcm_power_on_check(void); 
#endif