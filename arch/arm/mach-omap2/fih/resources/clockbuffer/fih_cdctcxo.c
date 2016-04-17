/** 
* TCXO/CDC3S04 Driver
*****************************************************/

#include <linux/init.h>
#include <linux/module.h>
#include "fih_cdctcxo.h"

#if defined(CONFIG_FIH_CDC3S04)
#include <linux/cdc_tcxo.h>
/*
 * The Clock Driver Chip (TCXO) on OMAP4 based SDP needs to
 * be programmed to output CLK1 based on REQ1 from OMAP.
 * By default CLK1 is driven based on an internal REQ1INT signal
 * which is always set to 1.
 * Doing this helps gate sysclk (from CLK1) to OMAP while OMAP
 * is in sleep states.
 */

static struct cdc_tcxo_platform_data sdp4430_cdc_data = {
	.buf = {
		CDC_TCXO_REQ4INT | CDC_TCXO_REQ1INT |
		CDC_TCXO_REQ4POL | CDC_TCXO_REQ3POL |
		CDC_TCXO_REQ2POL | CDC_TCXO_REQ1POL,
		CDC_TCXO_MREQ4 | CDC_TCXO_MREQ3 |
		CDC_TCXO_MREQ2 | CDC_TCXO_MREQ1,
		0x20, 0 },
};

static struct i2c_board_info __initdata tablet_i2c_1_boardinfo[] = {
	{
		.type	= "cdc_tcxo_driver",
		.addr	= 0x6c,
		.platform_data = &sdp4430_cdc_data,
	},
};

int __init cdctcxo_init(void)
{
	printk(KERN_ALERT "%s, begin\n",__func__);
	i2c_register_board_info(1, tablet_i2c_1_boardinfo, ARRAY_SIZE(tablet_i2c_1_boardinfo));
	printk(KERN_ALERT "%s, end\n",__func__);
	
	return 0;
}
#else	
int __init cdctcxo_init(void)
{
	return 0;
}
#endif
