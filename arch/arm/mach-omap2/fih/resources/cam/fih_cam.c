/** 
* Camera Driver
*********************************************************/
#include "fih_cam.h"

//SW4-L1-HL-Camera-FTM_BringUp-00*{
#if defined(CONFIG_MT9M114)	//defined(CONFIG_VIDEO_OMAP4) || defined(CONFIG_VIDEO_OMAP4_MODULE)
#include <mach/omap4-cam.h>
#include <media/omap4_camera.h>
#include <media/soc_camera.h>
#include <linux/clk.h>
#include <plat/omap-pm.h>
#include <linux/i2c/twl.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_qos_params.h>		//SW4-L1-HL-Camera-FTM-ICS-00+_20111205

#define OMAP44XXGOX_CAM_RESET	83

static struct omap4_camera_pdata sdp4430_camera_pdata = {
	.csi2cfg = {
		.lanes.clock = {
			.pol = OMAP4_ISS_CSIPHY_LANEPOL_DXPOS_DYNEG,
			.pos = OMAP4_ISS_CSIPHY_LANEPOS_DXY0,
		},
		.lanes.data = {
			{
				.pol = OMAP4_ISS_CSIPHY_LANEPOL_DXPOS_DYNEG,
				.pos = OMAP4_ISS_CSIPHY_LANEPOS_DXY1,
			},
			//HL-{CSI
			#if 0
			{
				.pol = OMAP4_ISS_CSIPHY_LANEPOL_DXPOS_DYNEG,
				.pos = OMAP4_ISS_CSIPHY_LANEPOS_DXY2,
			},
			#endif
			//HL-}CSI
		},
	},
};

static struct platform_device sdp4430_camera = {
	.name			= "omap4-camera",
	.id				= 0,
	.num_resources	= ARRAY_SIZE(omap4_camera_resources),
	.resource		= omap4_camera_resources,
	.dev    		= {
						.platform_data = &sdp4430_camera_pdata,
						},
};

#define MT9M114_I2C_ADDRESS   (0x90 >> 1)

static struct i2c_board_info sdp4430_i2c_camera[] = {
	{
		I2C_BOARD_INFO("mt9m114", MT9M114_I2C_ADDRESS),
	},
};

static struct clk *dpll_per_m3x2_ck;
static struct clk *aux_clk;
static struct clk *aux_src_clk;

static int omap4_fref_clk_init(struct device *dev,
				unsigned int clknum,
				unsigned int freq)
{
	char clk_name[20];
	char src_clk_name[20];
	int ret = 0;

	//HL
	printk(KERN_ALERT "[HL]%s +++\n",__func__);

	if (clknum > 5) {
		dev_err(dev, "ERROR: Wrong fref_clk index. Should be between"
			"0 and 5.\n");
		return -EINVAL;
	}

	if (!dpll_per_m3x2_ck) {
		dpll_per_m3x2_ck = clk_get(dev, "dpll_per_m3x2_ck");
		if (IS_ERR(dpll_per_m3x2_ck)) {
			dev_err(dev,
				"Unable to get dpll_per_m3x2_ck clock info\n");
			return -ENODEV;
		}
	}

	/* building the name for aux_clk */
	sprintf(clk_name, "auxclk%d_ck", clknum);
	aux_clk = clk_get(dev, clk_name);
	if (IS_ERR(aux_clk)) {
		clk_put(dpll_per_m3x2_ck);
		dev_err(dev,
			"Unable to get %s clock info\n", clk_name);
		return -ENODEV;
	}

	/* building the name for aux_src_clk */
	sprintf(src_clk_name, "auxclk%d_src_ck", clknum);
	aux_src_clk = clk_get(dev, src_clk_name);
	if (IS_ERR(aux_src_clk)) {
		clk_put(aux_clk);
		clk_put(dpll_per_m3x2_ck);
		dev_err(dev,
			"Unable to get %s clock info\n", src_clk_name);
		return -ENODEV;
	}

	ret = clk_set_parent(aux_src_clk, dpll_per_m3x2_ck);
	if (ret) {
		clk_put(aux_src_clk);
		clk_put(aux_clk);
		clk_put(dpll_per_m3x2_ck);
		dev_err(dev, "Unable to set clock: dpll_per_m3x2_ck"
			" as parent of %s\n",
			src_clk_name);
		return -ENODEV;
	}

	ret = clk_set_rate(aux_clk, clk_round_rate(aux_clk, freq));
	if (ret) {
		clk_disable(aux_src_clk);
		clk_disable(dpll_per_m3x2_ck);
		clk_put(aux_src_clk);
		clk_put(aux_clk);
		clk_put(dpll_per_m3x2_ck);
		dev_err(dev, "Rate not supported by %s\n", clk_name);
		return -EINVAL;
	}

	//HL
	printk(KERN_ALERT "[HL]%s ---\n",__func__);

	return 0;
}

/* FIXME: Make this nicer */
static void omap4_fref_clk_enable(unsigned int clknum, int enable)
{
	//HL
	printk(KERN_ALERT "[HL]%s +++\n",__func__);

	if (enable)
		clk_enable(aux_clk);
	else
		clk_disable(aux_clk);

	//HL
	printk(KERN_ALERT "[HL]%s ---\n",__func__);	
}

static int sdp4430_mt9m114_power(struct device *dev, int power)
{
	static struct pm_qos_request_list *qos_request;
	int ret = 0;
	//u8 gpoctl = 0;		
	struct regulator *p_regulator = NULL;
	int retval = 0;

	//HL
	printk(KERN_ERR "[HL]%s +++, power = %d\n",__func__, power);

	if (power) 
	{
		if (gpio_request_one(OMAP44XXGOX_CAM_RESET, GPIOF_OUT_INIT_HIGH,
				     "CAM_RESET")) {
			printk(KERN_ERR "Cannot request GPIO %d\n",
				OMAP44XXGOX_CAM_RESET);
			return -ENODEV;
		}
		else
		{
			printk(KERN_ERR "[HL]Request GPIO %d\n", OMAP44XXGOX_CAM_RESET);
		}
		
		/*
		 * This should match the Throughtput limit set in the ISS
		 * BTE_CTRL register. Currently it's set to 1 GB.
		 */
		omap_pm_set_min_bus_tput(dev, OCP_INITIATOR_AGENT, 1000000);

		printk(KERN_ERR "[HL]%s: omap_pm_set_min_bus_tput(dev, OCP_INITIATOR_AGENT, 1000000)\n", __func__);

		/*
		 * Hold a constraint to keep MPU in C1.
		 * Value obtained from cpuidle44xx.c: cpuidle_params_table
		 */
		pm_qos_update_request(qos_request, (int)2);		//omap_pm_set_max_mpu_wakeup_lat(&qos_request, 2);	//SW4-L1-HL-Camera-FTM-ICS-00*_20111205

		printk(KERN_ERR "[HL]%s: pm_qos_update_request(qos_request, (int)2)\n", __func__);

		//Turn on 2V8 power		
		/* Search the name of regulator based on the id and request it */
		p_regulator = regulator_get(NULL, "cam2pwr");	
		if (p_regulator == NULL) {
			pr_err("%s %d Error providing regulator %s\n", __func__
									 , __LINE__
									 , "cam2pwr");
			return -EAGAIN;
		}
		
		/* Set the regulator voltage min = data[0]; max = data[1]*/
		retval = regulator_set_voltage(p_regulator, 2800000, 2800000);
		if (retval) {
			pr_err("%s %d Error setting %s voltage\n", __func__
								 , __LINE__
								 , "cam2pwr");
			return -EAGAIN;
		}

		/* enable regulator */
		retval = regulator_enable(p_regulator);
		if (retval) {
			pr_err("%s %d Error enabling %s ldo\n", __func__
								 , __LINE__
								 , "cam2pwr");
			return -EAGAIN;
		}

		printk(KERN_ERR "[HL]Turn on 2V8 power\n");
		//printk(KERN_ERR "[HL]Start waiting for 1 second after enabling 2V8 POWER...\n");
		//mdelay(1000);		
		//printk(KERN_ERR "[HL]Finish waiting for 1 second after enabling 2V8 POWER!\n");

		mdelay(10);

		dev_dbg(dev, "Initializing and enabling FREQ_CLK2...\n");
		if (omap4_fref_clk_init(dev, 2, 24000000))
		{
			printk(KERN_ERR "[HL]%s: Error init FREQ_CLK2\n", __func__);
			return -EINVAL;
		}

		gpio_set_value(OMAP44XXGOX_CAM_RESET, 1);
		mdelay(10);
		omap4_fref_clk_enable(2, 1); /* Enable XCLK */
		//printk(KERN_ERR "[HL]Start waiting for 1 second after enabling XCLK...\n");
		//mdelay(1000);
		printk(KERN_ERR "[HL]Finish enabling XCLK!\n");		
	}
	else 
	{
		omap4_fref_clk_enable(2, 0);
		mdelay(1);
		gpio_set_value(OMAP44XXGOX_CAM_RESET, 0);
		mdelay(1);
		omap_pm_set_min_bus_tput(dev, OCP_INITIATOR_AGENT, -1);
		pm_qos_update_request(qos_request, (int)-1);	////omap_pm_set_max_mpu_wakeup_lat(&qos_request, -1);	//SW4-L1-HL-Camera-FTM-ICS-00*_20111205

		gpio_free(OMAP44XXGOX_CAM_RESET);

		//Turn off 2V8 power		
		/* Search the name of regulator based on the id and request it */
		p_regulator = regulator_get(NULL, "cam2pwr");
		if (p_regulator == NULL) {
			pr_err("%s %d Error providing regulator %s\n", __func__
									 , __LINE__
									 , "cam2pwr");
			return -EAGAIN;
		}

		/* Disable regulator */
		retval = regulator_disable(p_regulator);
		if (retval) {
			pr_err("%s %d Error enabling %s ldo\n", __func__
								 , __LINE__
								 , "cam2pwr");
			return -EAGAIN;
		}
		printk(KERN_ERR "[HL]Turn off 2V8 power\n");		
	}

	//HL
	printk(KERN_ALERT "[HL]%s ---\n",__func__);

	return ret;
}

static struct v4l2_subdev_sensor_interface_parms mt9m114_if_params = {
	.if_type	= V4L2_SUBDEV_SENSOR_SERIAL,
	.if_mode	= V4L2_SUBDEV_SENSOR_MODE_SERIAL_CSI2,
	/* Below used to know the physical limitations */
	.parms.serial = {
		.lanes = 1,	// 2		//HL*CSI
		.channel = 0,
		.phy_rate = 0, /* If zero, there's no board limitation */
		.pix_clk = 0, /* if zero, there's no board limitation */
	},
};

static struct soc_camera_link iclink_mt9m114 = {
	.bus_id		= 0,		/* Must match with the camera ID */
	.board_info	= &sdp4430_i2c_camera[0],
	.i2c_adapter_id	= 3,
	.module_name	= "mt9m114",
	.power		= &sdp4430_mt9m114_power,
	.priv		= &mt9m114_if_params,
};

static struct platform_device sdp4430_mt9m114 = {
	.name	= "soc-camera-pdrv",
	.id	= 1,
	.dev	= {
		.platform_data = &iclink_mt9m114,
	},
};

static struct platform_device *blazetablet_devices[] __initdata = {
	&sdp4430_mt9m114,
	&sdp4430_camera,
};

int __init camera_init(void)
{
	printk(KERN_ALERT "%s, begin\n",__func__);
	//HL
	printk(KERN_ALERT "[HL]%s, ARRAY_SIZE(blazetablet_devices) = %d\n",__func__, ARRAY_SIZE(blazetablet_devices));
	platform_add_devices(blazetablet_devices, ARRAY_SIZE(blazetablet_devices));
	printk(KERN_ALERT "%s, end\n",__func__);
	
	return 0;
}
//SW4-L1-HL-Camera-FTM_BringUp-00*}


#else
int __init camera_init(void)
{
	return 0;	
}
#endif
