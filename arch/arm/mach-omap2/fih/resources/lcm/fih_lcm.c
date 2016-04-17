/**
* LCM Driver
******************************************/

#include "fih_lcm.h"
#include <linux/delay.h>
#include <plat/display.h>
#include <plat/toshiba-dsi-panel.h>

static struct toshiba_dsi_panel_data blazetablet_dsi_panel = {
	.name	= "d2l",
	.reset_gpio	= BRIDGE_RESX, 
	.use_ext_te	= false,
	.ext_te_gpio	= 101,
	.use_esd_check	= false,
	.set_backlight	= NULL,
};

static struct omap_dss_device blazetablet_lcd_device = {
	.name			= "lcd",
	.driver_name		= "d2l",
	.type			= OMAP_DISPLAY_TYPE_DSI,
	.data			= &blazetablet_dsi_panel,
	.phy.dsi		= {
		.clk_lane	= 3,//JossCheng, GOX LCD data line is different with Blaze.
		.clk_pol	= 0,
		.data1_lane	= 1,
		.data1_pol	= 0,
		.data2_lane	= 2,
		.data2_pol	= 0,
		.data3_lane	= 4,
		.data3_pol	= 0,
		.data4_lane	= 5,
		.data4_pol	= 0,
		.div		= {
			.lck_div	= 1,	/* LCD */
			.pck_div	= 2,	/* PCD */
			.regm		= 394,	/* DSI_PLL_REGM */
			.regn		= 38,	/* DSI_PLL_REGN */
			.regm_dispc	= 6,	/* PLL_CLK1 (M4) */
			.regm_dsi	= 9,	/* PLL_CLK2 (M5) */
			.lp_clk_div	= 5,	/* LPDIV */
		},
		.xfer_mode = OMAP_DSI_XFER_VIDEO_MODE,
	},
	.panel			= {
		.width_in_mm = 210,
		.height_in_mm = 158,
	},
	.channel		= OMAP_DSS_CHANNEL_LCD,
};
#ifdef CONFIG_OMAP2_DSS_HDMI
static int sdp4430_panel_enable_hdmi(struct omap_dss_device *dssdev)
{
	gpio_request(HDMI_GPIO_60 , "hdmi_gpio_60");
	gpio_request(HDMI_GPIO_41 , "hdmi_gpio_41");
	gpio_direction_output(HDMI_GPIO_60, 1);
	gpio_direction_output(HDMI_GPIO_41, 1);
	gpio_set_value(HDMI_GPIO_60, 0);
	gpio_set_value(HDMI_GPIO_41, 0);
	mdelay(5);
	gpio_set_value(HDMI_GPIO_60, 1);
	gpio_set_value(HDMI_GPIO_41, 1);
	return 0;
}

static void sdp4430_panel_disable_hdmi(struct omap_dss_device *dssdev)
{
}

static __attribute__ ((unused)) void __init sdp4430_hdmi_init(void)
{
}

static struct omap_dss_device sdp4430_hdmi_device = {
	.name = "hdmi",
	.driver_name = "hdmi_panel",
	.type = OMAP_DISPLAY_TYPE_HDMI,
	.phy.dpi.data_lines = 24,
	.platform_enable = sdp4430_panel_enable_hdmi,
	.platform_disable = sdp4430_panel_disable_hdmi,
	.channel = OMAP_DSS_CHANNEL_DIGIT,
};
#endif /* CONFIG_OMAP2_DSS_HDMI */

#ifdef CONFIG_PANEL_PICO_DLP
static int sdp4430_panel_enable_pico_DLP(struct omap_dss_device *dssdev)
{
	/* int i = 0; */

	gpio_request(DLP_4430_GPIO_45, "DLP PARK");
	gpio_direction_output(DLP_4430_GPIO_45, 0);
	gpio_request(DLP_4430_GPIO_40, "DLP PHY RESET");
	gpio_direction_output(DLP_4430_GPIO_40, 0);
	/* gpio_request(DLP_4430_GPIO_44, "DLP READY RESET");
	gpio_direction_input(DLP_4430_GPIO_44); */
	mdelay(500);

	gpio_set_value(DLP_4430_GPIO_45, 1);
	mdelay(1000);

	gpio_set_value(DLP_4430_GPIO_40, 1);
	mdelay(1000);

	/* FIXME with the MLO gpio changes,
		gpio read is not retuning correct value even though
		it is  set in hardware so the check is comment
		till the problem is fixed */
	/* while (i == 0)
		i = gpio_get_value(DLP_4430_GPIO_44); */

	mdelay(2000);
	return 0;
}

static void sdp4430_panel_disable_pico_DLP(struct omap_dss_device *dssdev)
{
	gpio_set_value(DLP_4430_GPIO_40, 0);
	gpio_set_value(DLP_4430_GPIO_45, 0);
}

static struct omap_dss_device sdp4430_picoDLP_device = {
	.name			= "pico_DLP",
	.driver_name		= "picoDLP_panel",
	.type			= OMAP_DISPLAY_TYPE_DPI,
	.phy.dpi.data_lines	= 24,
	.platform_enable	= sdp4430_panel_enable_pico_DLP,
	.platform_disable	= sdp4430_panel_disable_pico_DLP,
	.channel		= OMAP_DSS_CHANNEL_LCD2,
};
#endif /* CONFIG_PANEL_PICO_DLP */

/* PARADE DP501, DisplayPort chip */
static int sdp4430_panel_enable_displayport(struct omap_dss_device *dssdev)
{
	printk(KERN_DEBUG "sdp4430_panel_enable_displayport is called\n");
	gpio_request(DP_4430_GPIO_59, "DISPLAYPORT POWER DOWN");
	gpio_direction_output(DP_4430_GPIO_59, 0);
	mdelay(100);
	gpio_set_value(DP_4430_GPIO_59, 1);
	mdelay(100);
	return 0;
}

static void sdp4430_panel_disable_displayport(struct omap_dss_device *dssdev)
{
	printk(KERN_DEBUG "sdp4430_panel_disable_displayport is called\n");
	gpio_set_value(DP_4430_GPIO_59, 0);
}

static struct omap_dss_device sdp4430_displayport_device = {
	.name				= "DP501",
	.driver_name			= "displayport_panel",
	.type				= OMAP_DISPLAY_TYPE_DPI,
	.phy.dpi.data_lines		= 24,
	.platform_enable		= sdp4430_panel_enable_displayport,
	.platform_disable		= sdp4430_panel_disable_displayport,
	.channel			= OMAP_DSS_CHANNEL_LCD2,
};

static struct omap_dss_device *blazetablet_dss_devices[] = {
	&blazetablet_lcd_device,
#ifdef CONFIG_PANEL_DP501
	&sdp4430_displayport_device,
#endif
#ifdef CONFIG_OMAP2_DSS_HDMI
	&sdp4430_hdmi_device,
#endif
#ifdef CONFIG_PANEL_PICO_DLP
	&sdp4430_picoDLP_device,
#endif
};

static struct omap_dss_board_info blazetablet_dss_data = {
	.num_devices	=	ARRAY_SIZE(blazetablet_dss_devices),
	.devices	=	blazetablet_dss_devices,
	.default_device =	&blazetablet_lcd_device,
};

static void __init omap4_display_init(void)
{
	void __iomem *phymux_base = NULL;
	u32 val = 0xFFFFC000;

	phymux_base = ioremap(0x4A100000, 0x1000);

	/* Turning on DSI PHY Mux*/
	__raw_writel(val, phymux_base + 0x618);

	/* Set mux to choose GPIO 101 for Taal 1 ext te line*/
	val = __raw_readl(phymux_base + 0x90);
	val = (val & 0xFFFFFFE0) | 0x11B;
	__raw_writel(val, phymux_base + 0x90);

	/* Set mux to choose GPIO 103 for Taal 2 ext te line*/
	val = __raw_readl(phymux_base + 0x94);
	val = (val & 0xFFFFFFE0) | 0x11B;
	__raw_writel(val, phymux_base + 0x94);

	iounmap(phymux_base);

	/* Panel D2L reset and backlight GPIO init */
	gpio_request(blazetablet_dsi_panel.reset_gpio, "dsi1_en_gpio");
	gpio_direction_output(blazetablet_dsi_panel.reset_gpio, 0);

}

void __init fih_fb_device_init(void)
{
	omap4_display_init(); 
	omap_display_init(&blazetablet_dss_data);
}