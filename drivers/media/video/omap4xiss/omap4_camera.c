/*
 * V4L2 Driver for OMAP4 camera host
 *
 * Copyright (C) 2011, Texas Instruments
 *
 * Author: Sergio Aguirre <saaguirre@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "iss_common.h"
#include "isp_ipipeif.h"
#include "isp_isif.h"
#include "isp_rsz.h"
#include <mach/tiler.h>
#include "../tiler/_tiler.h"	//SW4-L1-HL-Camera-FTM-ICS-00+_20120109
#include <plat/control.h>				//HL+CSI_20111123
#include <linux/ion.h>					//SW4-L1-HL-Camera-FTM-ICS-01+_20120104
#include "../../../gpu/ion/ion_priv.h"	//SW4-L1-HL-Camera-FTM-ICS-01+_20120104

//SW4-L1-HL-Camera-FTM-ICS-00+{_20120111
/* Begin : Tiler memory malloc by John.Lin */
extern struct ion_heap *tiler_heap;
/* End : Tiler memory malloc by John.Lin */
//SW4-L1-HL-Camera-FTM-ICS-00+}_20120111

//SW4-L1-HL-Camera-ICS-00+{_20111213
struct omap_tiler_info {
	tiler_blk_handle tiler_handle;	/* handle of the allocation intiler */
	bool lump;			/* true for a single lump allocation */
	u32 n_phys_pages;		/* number of physical pages */
	u32 *phys_addrs;		/* array addrs of pages */
	u32 n_tiler_pages;		/* number of tiler pages */
	u32 *tiler_addrs;		/* array of addrs of tiler pages */
	u32 tiler_start;		/* start addr in tiler -- if not page
					   aligned this may not equal the
					   first entry onf tiler_addrs */
};
//SW4-L1-HL-Camera-ICS-00+}_20111213

//SW4-L1-HL-Camera-FTM-ICS-00+{_20120111
struct omap_tiler_info *info;
//SW4-L1-HL-Camera-FTM-ICS-00+}_20120111

//SW4-L1-HL-Camera-FTM-ReleaseMem-00+{_20120203
struct omap_tiler_info *info_release[6];
//SW4-L1-HL-Camera-FTM-ReleaseMem-00+}_20120203


#define OMAP4_CAM_VERSION_CODE KERNEL_VERSION(0, 0, 1)
#define OMAP4_CAM_DRV_NAME "omap4-camera"

//SW4-L1-HL-Camera-FTM-ICS-00*{_20111205
//HL-{CSI
#if defined(CONFIG_VIDEO_OMAP4_USE_PREALLOC) && CONFIG_VIDEO_OMAP4_USE_PREALLOC
#define USE_BIGBLOCK
#endif
//HL-}CSI
//SW4-L1-HL-Camera-FTM-ICS-00*}_20111205

static const char *omap4_camera_driver_description = "OMAP4_Camera";

static const struct soc_mbus_pixelfmt omap4_camera_formats[] = {
	{
		.fourcc			= V4L2_PIX_FMT_NV12,
		.name			= "2 Planes Y/CbCr 4:2:0",
		.bits_per_sample	= 8,
		.packing		= SOC_MBUS_PACKING_NONE,
		.order			= SOC_MBUS_ORDER_LE,
	},
};

static int omap4_camera_check_frame(u32 width, u32 height)
{
	//printk("[HL]%s: +++---\n", __func__);

	/* limit to omap4 hardware capabilities */
	return height > 8192 || width > 8192;
}

static void omap4_camera_top_reset(struct omap4_camera_dev *dev)
{
	//printk("[HL]%s: +++\n", __func__);

	writel(readl(dev->regs.top + ISS_HL_SYSCONFIG) |
		ISS_HL_SYSCONFIG_SOFTRESET,
		dev->regs.top + ISS_HL_SYSCONFIG);

	/* Endless loop to wait for ISS HL reset */
	for (;;) {
		mdelay(1);
		/* If ISS_HL_SYSCONFIG.SOFTRESET == 0, reset is done */
		if (!(readl(dev->regs.top + ISS_HL_SYSCONFIG) &
				ISS_HL_SYSCONFIG_SOFTRESET))
			break;
	}

	//printk("[HL]%s: ---\n", __func__);	
}

static void omap4_camera_isp_reset(struct omap4_camera_dev *dev)
{
	//printk("[HL]%s: +++\n", __func__);

	/* Fist, ensure that the ISP is IDLE (no transactions happening) */
	writel((readl(dev->regs.isp5_sys1 + ISP5_SYSCONFIG) &
		~ISP5_SYSCONFIG_STANDBYMODE_MASK) |
		ISP5_SYSCONFIG_STANDBYMODE_SMART,
		dev->regs.isp5_sys1 + ISP5_SYSCONFIG);

	writel(readl(dev->regs.isp5_sys1 + ISP5_CTRL) |
		ISP5_CTRL_MSTANDBY,
		dev->regs.isp5_sys1 + ISP5_CTRL);

	for (;;) {
		mdelay(1);
		if (readl(dev->regs.isp5_sys1 + ISP5_CTRL) &
				ISP5_CTRL_MSTANDBY_WAIT)
			break;
	}

	/* Now finally, do the reset */
	writel(readl(dev->regs.isp5_sys1 + ISP5_SYSCONFIG) |
		ISP5_SYSCONFIG_SOFTRESET,
		dev->regs.isp5_sys1 + ISP5_SYSCONFIG);

	//printk("[HL]%s: ---\n", __func__);	
}

static int omap4_camera_csi2_reset(struct omap4_camera_dev *dev)
{
	int ret = 0;
	int timeout;

	//printk("[HL]%s: +++\n", __func__);

	/* Do a CSI2(A or B) RX soft reset */
	writel(readl(dev->regs.csi2a.regs1 + CSI2_SYSCONFIG) |
		CSI2_SYSCONFIG_SOFT_RESET,
		dev->regs.csi2a.regs1 + CSI2_SYSCONFIG);

	/* Wait for completion */
	timeout = 5; /* Retry 5 times as per TRM suggestion */
	do {
		mdelay(1);
		if (readl(dev->regs.csi2a.regs1 + CSI2_SYSSTATUS) &
				CSI2_SYSSTATUS_RESET_DONE)
			break;
	} while (--timeout > 0);

	if (timeout == 0) {
		dev_err(dev->dev, "CSI2 reset timeout!");
		ret = -EBUSY;
		goto out;
	}

	/* Default configs */
	/*
	 * MSTANDBY_MODE = 2 - Smart standby
	 * AUTO_IDLE = 0 - OCP clock is free-running
	 */
	writel((readl(dev->regs.csi2a.regs1 + CSI2_SYSCONFIG) &
		~(CSI2_SYSCONFIG_MSTANDBY_MODE_MASK |
		  CSI2_SYSCONFIG_AUTO_IDLE)) |
		CSI2_SYSCONFIG_MSTANDBY_MODE_SMART,
		dev->regs.csi2a.regs1 + CSI2_SYSCONFIG);


	//HL+{CSI_20111123
	/* Enable OCP and ComplexIO error IRQs */
	writel(readl(dev->regs.csi2a.regs1 + CSI2_IRQENABLE) |
		CSI2_IRQ_OCP_ERR |
		CSI2_IRQ_COMPLEXIO_ERR,
		dev->regs.csi2a.regs1 + CSI2_IRQENABLE);

	/* Enable all ComplexIO IRQs */
	writel(readl(dev->regs.csi2a.regs1 + CSI2_COMPLEXIO_IRQENABLE) |
		0xFFFFFFFF,
		dev->regs.csi2a.regs1 + CSI2_COMPLEXIO_IRQENABLE);
	//printk("[HL]%s: Enable OCP and ComplexIO error IRQs AND Enable all ComplexIO IRQs\n", __func__);
	//HL+}CSI_20111123

	//printk("[HL]%s: ---\n", __func__);

out:
	//printk("[HL]%s: out---\n", __func__);
	return ret;
}

static int omap4_camera_csiphy_init(struct omap4_camera_dev *dev)
{
	int ret = 0;
	int timeout;
	unsigned int tmpreg, ddr_freq;
	struct omap4_camera_csi2_config *cfg = &dev->pdata->csi2cfg;	//HL+CSI_20111124
	
	//printk("[HL]%s: +++\n", __func__);

	//HL+{CSI_20111123
	/*
	 * CSI2 2(B):
	 *	 LANEENABLE[1:0] = 11(0x3) - Lanes 0 & 1 enabled
	 *	 CTRLCLKEN = 1 - Active high enable for CTRLCLK
	 *	 CAMMODE = 0 - DPHY mode
	 */
	omap4_ctrl_pad_writel((omap4_ctrl_pad_readl(
				OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_CAMERA_RX) &
			  ~(OMAP4_CAMERARX_CSI22_LANEENABLE_MASK |
				OMAP4_CAMERARX_CSI22_CAMMODE_MASK)) |
			 (0x3 << OMAP4_CAMERARX_CSI22_LANEENABLE_SHIFT) |
			 OMAP4_CAMERARX_CSI22_CTRLCLKEN_MASK,
			 OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_CAMERA_RX);	
	//HL+}CSI_20111123

	if (dev->if_parms.parms.serial.phy_rate == 0)
		return -EINVAL;

	/* De-assert the CSIPHY reset */
	writel(readl(dev->regs.csi2a.regs1 + CSI2_COMPLEXIO_CFG) |
		CSI2_COMPLEXIO_CFG_RESET_CTRL,
		dev->regs.csi2a.regs1 + CSI2_COMPLEXIO_CFG);

	//printk("[HL]%s: dev->if_parms.parms.serial.phy_rate = %d\n", __func__, dev->if_parms.parms.serial.phy_rate);

	ddr_freq = dev->if_parms.parms.serial.phy_rate / 2 / 1000000;

	//printk("[HL]%s: ddr_freq = %d\n", __func__, ddr_freq);

	/* CSI-PHY config */
	writel((readl(dev->regs.csi2phy + REGISTER0) &
		~(REGISTER0_THS_TERM_MASK | REGISTER0_THS_SETTLE_MASK)) |
		(((((12500 * ddr_freq + 1000000) / 1000000) - 1) & 0xFF) <<
		 REGISTER0_THS_TERM_SHIFT) |
		(((((90000 * ddr_freq + 1000000) / 1000000) + 3) & 0xFF)),
		dev->regs.csi2phy + REGISTER0);

	//HL*{CSI_20111123
	/* Assert the FORCERXMODE signal */
	writel(readl(dev->regs.csi2a.regs1 + CSI2_TIMING) |
		 CSI2_TIMING_FORCE_RX_MODE_IO1,
		dev->regs.csi2a.regs1 + CSI2_TIMING);
	//printk("[HL]%s: Assert the FORCERXMODE signal\n", __func__);
	//HL*}CSI_20111123

	//HL+{CSI_20111123
	/* Pulldown on signals through padconf registers */
	//HL*{CSI_20111124
	//csi22_dx0 & csi22_dy0
	writel((readl(dev->regs.csi2a.ctrl + CONTROL_CORE_PAD0_CSI22_DX0_PAD1_CSI22_DY0) & 
		~(CSI22_DX0_MUXMODE | CSI22_DX0_PULLTYPESELECT | CSI22_DY0_MUXMODE | CSI22_DY0_PULLTYPESELECT)) | 
		CSI22_DX0_INPUTENABLE | 
		CSI22_DX0_PULLUDENABLE | 
		CSI22_DY0_INPUTENABLE | 
		CSI22_DY0_PULLUDENABLE,
		dev->regs.csi2a.ctrl + CONTROL_CORE_PAD0_CSI22_DX0_PAD1_CSI22_DY0);

	//csi22_dx1 & csi22_dy1
	writel((readl(dev->regs.csi2a.ctrl + CONTROL_CORE_PAD0_CSI22_DX1_PAD1_CSI22_DY1) & 
		~(CSI22_DX1_MUXMODE | CSI22_DX1_PULLTYPESELECT | CSI22_DY1_MUXMODE | CSI22_DY1_PULLTYPESELECT)) | 
		CSI22_DX1_INPUTENABLE | 
		CSI22_DX1_PULLUDENABLE | 
		CSI22_DY1_INPUTENABLE | 
		CSI22_DY1_PULLUDENABLE,
		dev->regs.csi2a.ctrl + CONTROL_CORE_PAD0_CSI22_DX1_PAD1_CSI22_DY1);	
	//printk("[HL]%s: Pulldown on signals through padconf registers!\n", __func__);

	/* Wait for STOPSTATE = 1 (for all enabled lane modules) */
	writel((readl(dev->regs.csi2a.regs1 + CSI2_TIMING) &
		~(CSI2_TIMING_STOP_STATE_COUNTER_IO1_MASK |
		  CSI2_TIMING_STOP_STATE_X4_IO1)) |
		 CSI2_TIMING_STOP_STATE_X16_IO1 |
		 (0x1d6),
		dev->regs.csi2a.regs1 + CSI2_TIMING);
	
	for (;;) {
		mdelay(1);
		/* If FORCE_RX_MODE_IO1 == 0, all enabled lanes are in STOPSTATE and the timer is finished */
		if (!(readl(dev->regs.csi2a.regs1 + CSI2_TIMING) &
				CSI2_TIMING_FORCE_RX_MODE_IO1))
			break;
	}
	//printk("[HL]%s: Wait for STOPSTATE = 1 (for all enabled lane modules)\n", __func__);
	//HL+}CSI_20111123

	/* Power up the CSIPHY */
	writel((readl(dev->regs.csi2a.regs1 + CSI2_COMPLEXIO_CFG) &
		~CSI2_COMPLEXIO_CFG_PWD_CMD_MASK) |
		CSI2_COMPLEXIO_CFG_PWD_CMD_ON,
		dev->regs.csi2a.regs1 + CSI2_COMPLEXIO_CFG);

	/* Wait for CSIPHY power transition */
	timeout = 1000;
	do {
		mdelay(1);
		tmpreg = readl(dev->regs.csi2a.regs1 + CSI2_COMPLEXIO_CFG) &
				CSI2_COMPLEXIO_CFG_PWD_STATUS_MASK;
		if (tmpreg == CSI2_COMPLEXIO_CFG_PWD_STATUS_ON)
			break;

	} while (--timeout > 0);

	if (timeout == 0) {
		dev_err(dev->dev, "CSIPHY power on transition timeout!\n");
		ret = -EBUSY;
		goto out;
	}

	//HL*{CIS_20111124
	/* Release PIPD* (=1), Pullup on signals through padconf registers */
	//csi22_dx0 & csi22_dy0
	writel(readl(dev->regs.csi2a.ctrl + CONTROL_CORE_PAD0_CSI22_DX0_PAD1_CSI22_DY0) |
		CSI22_DX0_PULLTYPESELECT | 
		CSI22_DY0_PULLTYPESELECT,
		dev->regs.csi2a.ctrl + CONTROL_CORE_PAD0_CSI22_DX0_PAD1_CSI22_DY0);

	//csi22_dx1 & csi22_dy1
	writel(readl(dev->regs.csi2a.ctrl + CONTROL_CORE_PAD0_CSI22_DX1_PAD1_CSI22_DY1) |
		CSI22_DX1_PULLTYPESELECT | 
		CSI22_DY1_PULLTYPESELECT,
		dev->regs.csi2a.ctrl + CONTROL_CORE_PAD0_CSI22_DX1_PAD1_CSI22_DY1);
	//printk("[HL]%s: Pullup on signals through padconf registers!\n", __func__);	
	//HL*}CIS_20111124


	BUG_ON(dev->if_parms.parms.serial.lanes < 1);
	//printk("[HL]%s: dev->if_parms.parms.serial.lanes = %d\n", __func__, dev->if_parms.parms.serial.lanes);

	//HL*{CSI_20111124
	#if 1
	/* Lane config (pos/pol) */
	/*
	 * Clock: Position 1 (d[x/y]0)
	 * Data lane 1: Position 2 (d[x/y]1)
	 */
	writel((readl(dev->regs.csi2a.regs1 + CSI2_COMPLEXIO_CFG) &
			 ~(CSI2_COMPLEXIO_CFG_DATA4_POSITION_MASK |
			   CSI2_COMPLEXIO_CFG_DATA3_POSITION_MASK |
			   CSI2_COMPLEXIO_CFG_DATA2_POSITION_MASK |
			   CSI2_COMPLEXIO_CFG_DATA1_POSITION_MASK |
			   CSI2_COMPLEXIO_CFG_CLOCK_POSITION_MASK |
			   CSI2_COMPLEXIO_CFG_DATA4_POL |
			   CSI2_COMPLEXIO_CFG_DATA3_POL |
			   CSI2_COMPLEXIO_CFG_DATA2_POL |
			   CSI2_COMPLEXIO_CFG_DATA1_POL |
			   CSI2_COMPLEXIO_CFG_CLOCK_POL)) |
			((dev->if_parms.parms.serial.lanes > 3 ?
				cfg->lanes.data[3].pos : 0) <<
				CSI2_COMPLEXIO_CFG_DATA4_POSITION_SHIFT) |
			((dev->if_parms.parms.serial.lanes > 2 ?
				cfg->lanes.data[2].pos : 0) <<
				CSI2_COMPLEXIO_CFG_DATA3_POSITION_SHIFT) |
			((dev->if_parms.parms.serial.lanes > 1 ?
				cfg->lanes.data[1].pos : 0) <<
				CSI2_COMPLEXIO_CFG_DATA2_POSITION_SHIFT) |
			(cfg->lanes.data[0].pos <<
				CSI2_COMPLEXIO_CFG_DATA1_POSITION_SHIFT) |
			(cfg->lanes.clock.pos <<
				CSI2_COMPLEXIO_CFG_CLOCK_POSITION_SHIFT) |
			(cfg->lanes.data[3].pol *
				CSI2_COMPLEXIO_CFG_DATA4_POL) |
			(cfg->lanes.data[2].pol *
				CSI2_COMPLEXIO_CFG_DATA3_POL) |
			(cfg->lanes.data[1].pol *
				CSI2_COMPLEXIO_CFG_DATA2_POL) |
			(cfg->lanes.data[0].pol *
				CSI2_COMPLEXIO_CFG_DATA1_POL) |
			(cfg->lanes.clock.pol *
				CSI2_COMPLEXIO_CFG_CLOCK_POL),
		dev->regs.csi2a.regs1 + CSI2_COMPLEXIO_CFG);
	
	//printk("[HL]%s: cfg->lanes.clock.pos = %d, cfg->lanes.clock.pol = %d\n", __func__, cfg->lanes.clock.pos, cfg->lanes.clock.pol);
	//printk("[HL]%s: cfg->lanes.data[0].pos = %d, cfg->lanes.data[0].pol = %d\n", __func__, cfg->lanes.data[0].pos, cfg->lanes.data[0].pol);
	//printk("[HL]%s: cfg->lanes.data[1].pos = %d, cfg->lanes.data[1].pol = %d\n", __func__, cfg->lanes.data[1].pos, cfg->lanes.data[1].pol);	
	//printk("[HL]%s: cfg->lanes.data[2].pos = %d, cfg->lanes.data[2].pol = %d\n", __func__, cfg->lanes.data[2].pos, cfg->lanes.data[2].pol);
	//printk("[HL]%s: cfg->lanes.data[3].pos = %d, cfg->lanes.data[3].pol = %d\n", __func__, cfg->lanes.data[3].pos, cfg->lanes.data[3].pol);		
	#else
	/* Lane config (pos/pol) */
	/*
	 * Clock: Position 1 (d[x/y]0)
	 * Data lane 1: Position 2 (d[x/y]1)
	 */
	writel((readl(dev->regs.csi2a.regs1 + CSI2_COMPLEXIO_CFG) &
			 ~(CSI2_COMPLEXIO_CFG_DATA1_POSITION_MASK |
			   CSI2_COMPLEXIO_CFG_CLOCK_POSITION_MASK |
			   CSI2_COMPLEXIO_CFG_DATA2_POL |
			   CSI2_COMPLEXIO_CFG_DATA1_POL |
			   CSI2_COMPLEXIO_CFG_CLOCK_POL)) |
			(cfg->lanes.data[0].pos <<
				CSI2_COMPLEXIO_CFG_DATA1_POSITION_SHIFT) |
			(cfg->lanes.clock.pos <<
				CSI2_COMPLEXIO_CFG_CLOCK_POSITION_SHIFT) |
			(cfg->lanes.data[0].pol *
				CSI2_COMPLEXIO_CFG_DATA1_POL) |
			(cfg->lanes.clock.pol *
				CSI2_COMPLEXIO_CFG_CLOCK_POL),
		dev->regs.csi2a.regs1 + CSI2_COMPLEXIO_CFG);	

	//printk("[HL]%s: cfg->lanes.clock.pos = %d, cfg->lanes.clock.pol = %d\n", __func__, cfg->lanes.clock.pos, cfg->lanes.clock.pol);
	//printk("[HL]%s: cfg->lanes.data[0].pos = %d, cfg->lanes.data[0].pol = %d\n", __func__, cfg->lanes.data[0].pos, cfg->lanes.data[0].pol);
	#endif
	//HL*}CSI_20111124

	//printk("[HL]%s: ---\n", __func__);

out:
	//printk("[HL]%s: out---\n", __func__);	
	return ret;
}

static void omap4_camera_csi2_init_ctx(struct omap4_camera_dev *dev)
{
	//printk("[HL]%s: +++\n", __func__);

	writel((readl(dev->regs.csi2a.regs1 + CSI2_CTX_CTRL2(0)) &
		~(CSI2_CTX_CTRL2_VIRTUAL_ID_MASK)) |
	       (dev->if_parms.parms.serial.channel <<
		CSI2_CTX_CTRL2_VIRTUAL_ID_SHIFT),
	       dev->regs.csi2a.regs1 + CSI2_CTX_CTRL2(0));

	/* Activate respective IRQs */
	/* Enable Context #0 IRQ */
	writel(readl(dev->regs.csi2a.regs1 + CSI2_IRQENABLE) |
			CSI2_IRQ_CONTEXT0,
		dev->regs.csi2a.regs1 + CSI2_IRQENABLE);

	/* Enable Context #0 Frame End IRQ */
	writel(readl(dev->regs.csi2a.regs1 + CSI2_CTX_IRQENABLE(0)) |
			CSI2_CTX_IRQ_FE,
		dev->regs.csi2a.regs1 + CSI2_CTX_IRQENABLE(0));

	/* Enable some CSI2 DMA settings */
	writel((readl(dev->regs.csi2a.regs1 + CSI2_CTRL) &
		~(CSI2_CTRL_MFLAG_LEVH_MASK |
		  CSI2_CTRL_MFLAG_LEVL_MASK |
		  CSI2_CTRL_VP_OUT_CTRL_MASK)) |
	       CSI2_CTRL_BURST_SIZE_EXPAND |
	       CSI2_CTRL_NON_POSTED_WRITE |
	       CSI2_CTRL_FRAME |
	       CSI2_CTRL_BURST_SIZE_MASK |
	       CSI2_CTRL_ENDIANNESS |
	       CSI2_CTRL_VP_CLK_EN |
	       CSI2_CTRL_VP_ONLY_EN |
	       (1 << CSI2_CTRL_VP_OUT_CTRL_SHIFT) |
	       (6 << CSI2_CTRL_MFLAG_LEVH_SHIFT) |
	       (7 << CSI2_CTRL_MFLAG_LEVL_SHIFT),
	       dev->regs.csi2a.regs1 + CSI2_CTRL);

	//printk("[HL]%s: ---\n", __func__);	
}

static int __omap4_camera_streamon(struct soc_camera_device *icd);
static void __omap4_camera_streamoff(struct soc_camera_device *icd);
void omap4_camera_streamoff(struct soc_camera_device *icd);
static int omap4_camera_capture(struct omap4_camera_dev *dev);

static irqreturn_t omap4_camera_isr(int irq, void *arg)
{
	struct omap4_camera_dev *dev = (struct omap4_camera_dev *)arg;
	unsigned int issirq = readl(dev->regs.top + ISS_HL_IRQSTATUS_5);

	//printk("[HL]%s: +++\n", __func__);

	writel(issirq, dev->regs.top + ISS_HL_IRQSTATUS_5);
	if (issirq & ISS_HL_IRQ_CSIB) {	//ISS_HL_IRQ_CSIA		//HL*CSI
		unsigned int csi2irq = readl(dev->regs.csi2a.regs1 +
						CSI2_IRQSTATUS);

		writel(csi2irq, dev->regs.csi2a.regs1 + CSI2_IRQSTATUS);
		if (csi2irq & CSI2_IRQ_CONTEXT0) {
			unsigned int csi2ctxirq = readl(dev->regs.csi2a.regs1 +
					  CSI2_CTX_IRQSTATUS(0));

			writel(csi2ctxirq,
			       dev->regs.csi2a.regs1 + CSI2_CTX_IRQSTATUS(0));
			if ((csi2ctxirq & CSI2_CTX_IRQ_FE) &&
			    !(readl(dev->regs.csi2a.regs1 + CSI2_CTRL) &
			      CSI2_CTRL_VP_ONLY_EN)) {
				struct videobuf_buffer *vb = dev->active;
				unsigned long flags;

				dev_dbg(dev->dev, "Frame received!\n");

				if (dev->skip_frames > 0) {
					dev->skip_frames--;
					goto out;
				}

				if (!vb)
					goto out;
				spin_lock_irqsave(&dev->lock, flags);
				list_del_init(&vb->queue);

				if (!list_empty(&dev->capture))
					dev->active = list_entry(
							dev->capture.next,
							struct videobuf_buffer,
							queue);
				else
					dev->active = NULL;

				writel(readl(dev->regs.csi2a.regs1 +
					     CSI2_CTRL) &
					~CSI2_CTRL_IF_EN,
					dev->regs.csi2a.regs1 +
					CSI2_CTRL);
				if (omap4_camera_update_buf(dev) == 0) {
					writel(readl(dev->regs.csi2a.regs1 +
						     CSI2_CTRL) |
						CSI2_CTRL_IF_EN,
						dev->regs.csi2a.regs1 +
						CSI2_CTRL);
				}
				vb->state = VIDEOBUF_DONE;
				do_gettimeofday(&vb->ts);
				wake_up(&vb->done);

				spin_unlock_irqrestore(&dev->lock, flags);
			} else if (csi2ctxirq & CSI2_CTX_IRQ_FE) {
				/*
				 * SW Workaround for 1 extra HSYNC needed by
				 * ISP Resizer
				 */
				writel(readl(dev->regs.ipipeif + IPIPEIF_CFG2) ^
					IPIPEIF_CFG2_HDPOL,
					dev->regs.ipipeif + IPIPEIF_CFG2);
				writel(readl(dev->regs.ipipeif + IPIPEIF_CFG2) ^
					IPIPEIF_CFG2_HDPOL,
					dev->regs.ipipeif + IPIPEIF_CFG2);
			}
		}

		if (csi2irq & CSI2_IRQ_COMPLEXIO_ERR) {
			unsigned int csi2ctxirq = readl(dev->regs.csi2a.regs1 +
					  CSI2_COMPLEXIO_IRQSTATUS);

			writel(csi2ctxirq, dev->regs.csi2a.regs1 +
			       CSI2_COMPLEXIO_IRQSTATUS);
			dev_err(dev->dev, "COMPLEXIO ERR(0x%x)\n",
				csi2ctxirq);
		}
	}

	if (issirq & ISS_HL_IRQ_ISP0) {
		unsigned int isp0irq = readl(dev->regs.isp5_sys1 +
					     ISP5_IRQSTATUS(0));
		unsigned int isp0irq2 = readl(dev->regs.isp5_sys2 +
					      ISP5_IRQSTATUS2(0));

		writel(isp0irq, dev->regs.isp5_sys1 + ISP5_IRQSTATUS(0));
		writel(isp0irq2, dev->regs.isp5_sys2 + ISP5_IRQSTATUS2(0));

		if (isp0irq & ISP5_IRQ_RSZ_INT_DMA) {
			isp0irq &= ~ISP5_IRQ_RSZ_INT_DMA;
			omap4_camera_resizer_isr(dev);
		}

		if (isp0irq & ISP5_IRQ_RSZ_FIFO_IN_BLK) {
			isp0irq &= ~ISP5_IRQ_RSZ_FIFO_IN_BLK;
			dev_info(dev->dev, "ISP RSZ FIFO_IN_BLK Error\n");
		}

		if (isp0irq & ISP5_IRQ_RSZ_FIFO_OVF) {
			struct videobuf_buffer *vb = dev->active;
			unsigned long flags;

			isp0irq &= ~ISP5_IRQ_RSZ_FIFO_OVF;
			dev_info(dev->dev, "RSZ FIFO_OVF Error\n");

			/* Recovery mechanism */
			__omap4_camera_streamoff(dev->icd);
			__omap4_camera_streamon(dev->icd);

			if (!vb)
				goto out;
			spin_lock_irqsave(&dev->lock, flags);
			list_del_init(&vb->queue);

			if (!list_empty(&dev->capture))
				dev->active = list_entry(
						dev->capture.next,
						struct videobuf_buffer,
						queue);
			else
				dev->active = NULL;

			if (omap4_camera_update_buf(dev) == 0)
				omap4_camera_capture(dev);

			vb->state = VIDEOBUF_ERROR;
			do_gettimeofday(&vb->ts);
			wake_up(&vb->done);

			spin_unlock_irqrestore(&dev->lock, flags);
		}

		if (isp0irq & ISP5_IRQ_OCP_ERR) {
			isp0irq &= ~ISP5_IRQ_OCP_ERR;
			dev_info(dev->dev, "ISP OCP Error\n");
		}

		if (isp0irq2)
			dev_info(dev->dev, "ISP IRQ2 Error (%x)\n",
				 isp0irq2);
	}

	//printk("[HL]%s: ---\n", __func__);	
out:
	//printk("[HL]%s: out---\n", __func__);	
	return IRQ_HANDLED;
}

static int omap4_camera_add_device(struct soc_camera_device *icd)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	struct omap4_camera_dev *dev = ici->priv;

	//printk("[HL]%s: +++\n", __func__);

	if (dev->icd)
		return -EBUSY;

	dev->icd = icd;

	dev_dbg(icd->dev.parent, "OMAP4 Camera driver attached to camera %d\n",
		 icd->devnum);

	//printk("[HL]%s: ---\n", __func__);

	return 0;
}

/* Called with .video_lock held */
static void omap4_camera_remove_device(struct soc_camera_device *icd)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	struct omap4_camera_dev *dev = ici->priv;

	//printk("[HL]%s: +++\n", __func__);

	BUG_ON(icd != dev->icd);

	dev_dbg(icd->dev.parent,
		 "OMAP4 Camera driver detached from camera %d\n",
		 icd->devnum);

	if (dev->streaming)
		omap4_camera_streamoff(icd);	//New -- __omap4_camera_streamoff(icd);	//Original -- omap4_camera_streamoff(icd);	//SW4-L1-HL-Camera-FTM-ICS-00*_20120111

	dev->icd = NULL;

	//printk("[HL]%s: ---\n", __func__);	
}

static int omap4_camera_get_formats(struct soc_camera_device *icd,
				    unsigned int idx,
				    struct soc_camera_format_xlate *xlate)
{
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	struct device *dev = icd->dev.parent;
	enum v4l2_mbus_pixelcode code;
	const struct soc_mbus_pixelfmt *fmt;
	int formats = 0;

	//printk("[HL]%s: +++\n", __func__);

	if (v4l2_subdev_call(sd, video, enum_mbus_fmt, idx, &code) < 0)
		/* No more formats */
		return 0;

	fmt = soc_mbus_get_fmtdesc(code);
	if (!fmt) {
		dev_err(dev, "Invalid format code #%u: %d\n", idx, code);
		return 0;
	}

	//printk("[HL]%s: code = %d\n", __func__, code);

	switch (code) {
	case V4L2_MBUS_FMT_UYVY8_2X8:
		formats++;
		if (xlate) {
			xlate->host_fmt	= &omap4_camera_formats[0];
			xlate->code	= code;
			xlate++;
			dev_dbg(dev, "Providing format %s using code %d\n",
				omap4_camera_formats[0].name, code);
		}
		break;
	case V4L2_MBUS_FMT_YUYV8_2X8:
	default:
		if (xlate)
			dev_dbg(dev,
				"Providing format %s in pass-through mode\n",
				fmt->name);
	}

	/* Generic pass-through */
	formats++;
	if (xlate) {
		xlate->host_fmt	= fmt;
		xlate->code	= code;
		xlate++;
	}

	//printk("[HL]%s: formats = %d\n", __func__, formats);

	//printk("[HL]%s: ---\n", __func__);

	return formats;
}

static int omap4_camera_set_fmt(struct soc_camera_device *icd,
			      struct v4l2_format *f)
{
	struct device *dev = icd->dev.parent;
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	struct omap4_camera_dev *omap4cam_dev = ici->priv;
	const struct soc_camera_format_xlate *xlate = NULL;
	struct v4l2_pix_format *pix = &f->fmt.pix;
	struct v4l2_mbus_framefmt mf;
	int ret;
	u32 skip_frames = 0;

	//printk("[HL]%s: +++\n", __func__);

	xlate = soc_camera_xlate_by_fourcc(icd, pix->pixelformat);
	if (!xlate) {
		dev_warn(dev, "Format %x not found\n", pix->pixelformat);
		return -EINVAL;
	}

	mf.width	= pix->width;
	mf.height	= pix->height;
	mf.field	= pix->field;
	mf.colorspace	= pix->colorspace;
	mf.code		= xlate->code;

	ret = v4l2_subdev_call(sd, video, s_mbus_fmt, &mf);

	if (mf.code != xlate->code)
		return -EINVAL;

	if (ret < 0) {
		dev_warn(dev, "Failed to configure for format %x\n",
			 pix->pixelformat);
	} else if (omap4_camera_check_frame(mf.width, mf.height)) {
		dev_warn(dev,
			 "Camera driver produced an unsupported frame %dx%d\n",
			 mf.width, mf.height);
		ret = -EINVAL;
	}

	if (ret < 0)
		return ret;

	ret = v4l2_subdev_call(sd, sensor, g_skip_frames, &skip_frames);
	if (ret < 0 && ret != -ENOIOCTLCMD)
		return ret;

	omap4cam_dev->skip_frames = skip_frames;
	omap4cam_dev->pixcode = mf.code;

	/* Reset crop rectangle */
	omap4cam_dev->rect.left = 0;
	omap4cam_dev->rect.top = 0;
	omap4cam_dev->rect.height = mf.height;
	omap4cam_dev->rect.width = mf.width;

	pix->width		= mf.width;
	pix->height		= mf.height;
	pix->field		= mf.field;
	pix->colorspace		= mf.colorspace;
	icd->current_fmt	= xlate;

	//printk("[HL]%s: ---\n", __func__);

	return ret;
}

static int omap4_camera_try_fmt(struct soc_camera_device *icd,
			      struct v4l2_format *f)
{
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	const struct soc_camera_format_xlate *xlate;
	struct v4l2_pix_format *pix = &f->fmt.pix;
	struct v4l2_mbus_framefmt mf;
	__u32 pixfmt = pix->pixelformat;
	int ret;

	//printk("[HL]%s: +++\n", __func__);

	xlate = soc_camera_xlate_by_fourcc(icd, pixfmt);
	if (!xlate) {
		dev_warn(icd->dev.parent, "Format %x not found\n", pixfmt);
		return -EINVAL;
	}

#ifdef USE_BIGBLOCK
	//printk("[HL]%s: USE_BIGBLOCK is defined\n", __func__);

	pix->bytesperline = soc_mbus_bytes_per_line(pix->width,
						    xlate->host_fmt);
	if (pix->bytesperline < 0)
		return pix->bytesperline;
#else
	//printk("[HL]%s: USE_BIGBLOCK is not defined\n", __func__);

	if (pixfmt == V4L2_PIX_FMT_NV12) {
		pix->bytesperline = (pix->width +
			TILER_PAGE - 1) & ~(TILER_PAGE - 1);
	} else {
		pix->bytesperline = (pix->width * 2 +
			TILER_PAGE - 1) & ~(TILER_PAGE - 1);
	}
#endif
	pix->sizeimage = pix->height * pix->bytesperline;

	//printk("[HL]%s: pix->height = %d, pix->bytesperline = %d\n", __func__, pix->height, pix->bytesperline);

	//printk("[HL]%s: pixfmt = %d\n", __func__, pixfmt);

	if (pixfmt == V4L2_PIX_FMT_NV12)
	{	//HL+
		//printk("[HL]%s: pixfmt == V4L2_PIX_FMT_NV12\n", __func__);
		pix->sizeimage += (pix->height / 2) * pix->bytesperline;
	}	//HL+{CSI
	else if (pixfmt == V4L2_PIX_FMT_YUYV)
	{
		//printk("[HL]%s: pixfmt == V4L2_PIX_FMT_YUYV\n", __func__);
	}
	else if (pixfmt == V4L2_PIX_FMT_UYVY)
	{
		//printk("[HL]%s: pixfmt == V4L2_PIX_FMT_UYVY\n", __func__);
	}
	else if (pixfmt == V4L2_PIX_FMT_JPEG)
	{
		//printk("[HL]%s: pixfmt == V4L2_PIX_FMT_JPEG\n", __func__);
	}
	else
	{
		//printk("[HL]%s: pixfmt == else\n", __func__);
	}
	//HL+}CSI

	/* limit to sensor capabilities */
	mf.width	= pix->width;
	mf.height	= pix->height;
	mf.field	= pix->field;
	mf.colorspace	= pix->colorspace;
	mf.code		= xlate->code;

	ret = v4l2_subdev_call(sd, video, try_mbus_fmt, &mf);
	if (ret < 0)
		return ret;

	pix->width	= mf.width;
	pix->height	= mf.height;
	pix->colorspace	= mf.colorspace;

	//printk("[HL]%s: mf.field = %d, mf.code = %d, mf.colorspace = %d\n", __func__, mf.field, mf.code, mf.colorspace);

	switch (mf.field) {
	case V4L2_FIELD_ANY:
	case V4L2_FIELD_NONE:
		pix->field	= V4L2_FIELD_NONE;
		break;
	default:
		dev_err(icd->dev.parent, "Field type %d unsupported.\n",
			mf.field);
		return -EINVAL;
	}

	switch (mf.code) {
	case V4L2_MBUS_FMT_YUYV8_2X8:
	case V4L2_MBUS_FMT_UYVY8_2X8:
	case V4L2_MBUS_FMT_SBGGR10_1X10:
	case V4L2_MBUS_FMT_SBGGR10_2X8_PADLO_LE:
	case V4L2_MBUS_FMT_SBGGR8_1X8:
		/* Above formats are supported */
		break;
	default:
		dev_err(icd->dev.parent, "Sensor format code %d unsupported.\n",
			mf.code);
		return -EINVAL;
	}

	//printk("[HL]%s: ---\n", __func__);

	return ret;
}

static int omap4_camera_cropcap(struct soc_camera_device *icd,
				struct v4l2_cropcap *a)
{
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	struct v4l2_mbus_framefmt mf;
	int ret;

	//printk("[HL]%s: +++\n", __func__);

	ret = v4l2_subdev_call(sd, video, g_mbus_fmt, &mf);
	if (ret < 0)
		return ret;

	a->bounds.left = 0;
	a->bounds.top = 0;
	a->bounds.height = mf.height;
	a->bounds.width = mf.width;

	a->defrect = a->bounds;

	a->pixelaspect.numerator = 1;
	a->pixelaspect.denominator = 1;

	//printk("[HL]%s: ---\n", __func__);	
	
	return ret;
}

static int omap4_camera_get_crop(struct soc_camera_device *icd,
				 struct v4l2_crop *a)
{
	struct v4l2_rect *rect = &a->c;
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	struct omap4_camera_dev *dev = ici->priv;

	*rect = dev->rect;

	//printk("[HL]%s: +++---\n", __func__);

	return 0;
}

static int omap4_camera_set_crop(struct soc_camera_device *icd,
				 struct v4l2_crop *a)
{
	struct v4l2_rect *rect = &a->c;
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	struct omap4_camera_dev *dev = ici->priv;
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	struct v4l2_mbus_framefmt mf;
	int ret;
	u32 tmp_ratio;

	//printk("[HL]%s: +++\n", __func__);

	ret = v4l2_subdev_call(sd, video, g_mbus_fmt, &mf);
	if (ret < 0)
		return ret;

	/* As rectangle vars are s32, protect from negatives */
	if (rect->left < 0)
		rect->left = 0;
	if (rect->top < 0)
		rect->top = 0;
	if (rect->width < 0)
		rect->width = 0;
	if (rect->height < 0)
		rect->height = 0;

	/* Adjust ratios to make sure output is preserved */
	tmp_ratio = (256 * rect->width) / mf.width;
	rect->width = (tmp_ratio * mf.width) / 256;
	tmp_ratio = (256 * rect->height) / mf.height;
	rect->height = (tmp_ratio * mf.height) / 256;

	/* Avoid out-of-range windows */
	if ((rect->width + rect->left) > mf.width)
		rect->width = mf.width - rect->left;
	if ((rect->height + rect->top) > mf.height)
		rect->height = mf.height - rect->top;

	/* Make sure HPS & HSZ are going to be even */
	rect->left -= (rect->left % 2);
	rect->width -= (rect->width % 2);

	dev->rect = *rect;

	if (dev->streaming)
		omap4_camera_resizer_set_src_rect(dev, dev->rect);

	//printk("[HL]%s: ---\n", __func__);

	return ret;
}

static int omap4_camera_get_parm(struct soc_camera_device *icd,
				 struct v4l2_streamparm *a)
{
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);

	//printk("[HL]%s: +++---\n", __func__);

	return v4l2_subdev_call(sd, video, g_parm, a);
}

static int omap4_camera_set_parm(struct soc_camera_device *icd,
				 struct v4l2_streamparm *a)
{
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);

	//printk("[HL]%s: +++---\n", __func__);

	return v4l2_subdev_call(sd, video, s_parm, a);
}

/*
 *  Videobuf operations
 */
static int omap4_videobuf_setup(struct videobuf_queue *vq,
				unsigned int *count,
				unsigned int *size)
{
	struct soc_camera_device *icd = vq->priv_data;
	int bytes_per_line;
#ifdef USE_BIGBLOCK
	unsigned int max_size = omap_cam_get_mempool_size();
#endif

	//printk("[HL]%s: +++\n", __func__);

	//HL+{CSI
	if (icd->current_fmt->host_fmt->fourcc == V4L2_PIX_FMT_NV12)
	{
		//printk("[HL]%s: icd->current_fmt->host_fmt->fourcc == V4L2_PIX_FMT_NV12\n", __func__);
	}
	else if (icd->current_fmt->host_fmt->fourcc == V4L2_PIX_FMT_YUYV)
	{
		//printk("[HL]%s: icd->current_fmt->host_fmt->fourcc == V4L2_PIX_FMT_YUYV\n", __func__);
	}
	else if (icd->current_fmt->host_fmt->fourcc == V4L2_PIX_FMT_UYVY)
	{
		//printk("[HL]%s: icd->current_fmt->host_fmt->fourcc == V4L2_PIX_FMT_UYVY\n", __func__);
	}
	else if (icd->current_fmt->host_fmt->fourcc == V4L2_PIX_FMT_JPEG)
	{
		//printk("[HL]%s: icd->current_fmt->host_fmt->fourcc == V4L2_PIX_FMT_JPEG\n", __func__);
	}
	else
	{
		//printk("[HL]%s: icd->current_fmt->host_fmt->fourcc == else\n", __func__);
	}
	//HL+}CSI

	//printk("[HL]%s: icd->user_width=%d\n", __func__, icd->user_width);

#ifdef USE_BIGBLOCK
	bytes_per_line = soc_mbus_bytes_per_line(icd->user_width,
						icd->current_fmt->host_fmt);
	if (bytes_per_line < 0)
		return bytes_per_line;
#else
	if (icd->current_fmt->host_fmt->fourcc == V4L2_PIX_FMT_NV12) {
		bytes_per_line = (icd->user_width +
			TILER_PAGE - 1) & ~(TILER_PAGE - 1);
	} else {
		bytes_per_line = (icd->user_width * 2 +
			TILER_PAGE - 1) & ~(TILER_PAGE - 1);
	}
#endif

	//printk("[HL]%s: bytes_per_line = %d\n", __func__, bytes_per_line);

	*size = bytes_per_line * icd->user_height;	//icd->user_width * icd->user_height * 2;	//HL*CSI_20111128

	//printk("[HL]%s: size = %d\n", __func__, *size);

	if (icd->current_fmt->host_fmt->fourcc == V4L2_PIX_FMT_NV12)
		*size = *size * 3 / 2;

	//printk("[HL]%s: count=%d, size=%d\n", __func__, *count, *size);

#ifdef USE_BIGBLOCK
	if (*count * *size > max_size)
		*count = max_size / *size;
#else
	/*
	 * TODO: Limit buffer count to currently available TILER memory.
	 * Is there an API?
	 */
#endif

	dev_dbg(icd->dev.parent, "count=%d, size=%d\n", *count, *size);
	//printk("[HL]%s: count=%d, size=%d\n", __func__, *count, *size);


	//printk("[HL]%s: ---\n", __func__);

	return 0;
}

static void free_buffer(struct videobuf_queue *vq,
			struct omap4_camera_buffer *buf)
{
	struct soc_camera_device *icd = vq->priv_data;
	struct device *dev = icd->dev.parent;

	dev_dbg(dev, "%s (vb=0x%p) 0x%08lx %zd\n", __func__,
		&buf->vb, buf->vb.baddr, buf->vb.bsize);

	if (in_interrupt())
		BUG();

	videobuf_waiton(vq, &buf->vb, 0, 0);	//SW4-L1-HL-Camera-FTM-ICS-00*
	videobuf_dma_contig_free(vq, &buf->vb);
	dev_dbg(dev, "%s freed\n", __func__);
	buf->vb.state = VIDEOBUF_NEEDS_INIT;
}

int omap4_camera_update_buf(struct omap4_camera_dev *dev)
{
	dma_addr_t phys_addr;
	dma_addr_t phys_ofst;
	int i = 0;

	//printk("[HL]%s: +++\n", __func__);

	if (!dev->active)
		return -ENOMEM;

	i = dev->active->i;

	phys_addr = dev->buf_phy_addr[i];
	phys_ofst = dev->buf_phy_dat_ofst[i];

	if (readl(dev->regs.csi2a.regs1 + CSI2_CTRL) & CSI2_CTRL_VP_ONLY_EN) {
		dma_addr_t phys_addr_c = dev->buf_phy_uv_addr[i];
		dma_addr_t phys_ofst_c = dev->buf_phy_uv_dat_ofst[i];

		/* Save address splitted in Base Address H & L */
		writel((phys_addr >> 16) & 0xffff,
			dev->regs.resizer + RZA_SDR_Y_BAD_H);
		writel(phys_addr & 0xffff,
			dev->regs.resizer + RZA_SDR_Y_BAD_L);

		/* SAD = BAD */
		writel((phys_addr >> 16) & 0xffff,
			dev->regs.resizer + RZA_SDR_Y_SAD_H);
		writel(phys_addr & 0xffff,
			dev->regs.resizer + RZA_SDR_Y_SAD_L);

		writel(0, dev->regs.resizer + RZA_SDR_Y_PTR_S);
		writel(dev->icd->user_height,
			dev->regs.resizer + RZA_SDR_Y_PTR_E);

		writel(phys_ofst, dev->regs.resizer + RZA_SDR_Y_OFT);

		/* Ensure Y_BAD_L[6:0] = C_BAD_L[6:0]*/
		if ((phys_addr_c ^ phys_addr) & 0x7f) {
			phys_addr_c &= ~0x7f;
			phys_addr_c += 0x80;
			phys_addr_c |= phys_addr & 0x7f;
		}

		writel((phys_addr_c >> 16) & 0xffff,
			dev->regs.resizer + RZA_SDR_C_BAD_H);
		writel(phys_addr_c & 0xffff,
			dev->regs.resizer + RZA_SDR_C_BAD_L);

		/* SAD = BAD */
		writel((phys_addr_c >> 16) & 0xffff,
			dev->regs.resizer + RZA_SDR_C_SAD_H);
		writel(phys_addr_c & 0xffff,
			dev->regs.resizer + RZA_SDR_C_SAD_L);

		writel(0, dev->regs.resizer + RZA_SDR_C_PTR_S);
		writel(dev->icd->user_height,
			dev->regs.resizer + RZA_SDR_C_PTR_E);

		writel(phys_ofst_c, dev->regs.resizer + RZA_SDR_C_OFT);
	} else {
		writel(phys_addr & CSI2_CTX_PING_ADDR_MASK,
			dev->regs.csi2a.regs1 + CSI2_CTX_PING_ADDR(0));
		writel(phys_addr & CSI2_CTX_PONG_ADDR_MASK,
			dev->regs.csi2a.regs1 + CSI2_CTX_PONG_ADDR(0));

		writel(phys_ofst & CSI2_CTX_DAT_OFST_MASK,
			dev->regs.csi2a.regs1 + CSI2_CTX_DAT_OFST(0));
	}

	//printk("[HL]%s: ---\n", __func__);

	return 0;
}
/*
 * return value doesn't reflex the success/failure to queue the new buffer,
 * but rather the status of the previous buffer.
 */
static int omap4_camera_capture(struct omap4_camera_dev *dev)
{
	int ret = 0;
	u32 fmt_reg = 0;

	//printk("[HL]%s: +++\n", __func__);

	if (!dev->active)
		return ret;

	/* Configure format */
	switch (dev->pixcode) {
	case V4L2_MBUS_FMT_YUYV8_2X8:
	case V4L2_MBUS_FMT_UYVY8_2X8:
		fmt_reg = ISS_CSI2_CTXFMT_YUV422_8_VP16;	//Original -- ISS_CSI2_CTXFMT_YUV422_8_VP16	//Modify--ISS_CSI2_CTXFMT_YUV422_8		//HL*CSI_20111125
		break;
	case V4L2_MBUS_FMT_SBGGR10_1X10:
		fmt_reg = ISS_CSI2_CTXFMT_RAW_10;
		break;
	case V4L2_MBUS_FMT_SBGGR10_2X8_PADLO_LE:
		fmt_reg = ISS_CSI2_CTXFMT_RAW_10_EXP16;
		break;
	case V4L2_MBUS_FMT_SBGGR8_1X8:
		fmt_reg = ISS_CSI2_CTXFMT_RAW_8;
		break;
	default:
		/* This shouldn't happen if s_fmt is correctly implemented */
		BUG_ON(1);
		break;
	}

	writel((readl(dev->regs.csi2a.regs1 + CSI2_CTX_CTRL2(0)) &
				~(CSI2_CTX_CTRL2_FORMAT_MASK)) |
			(fmt_reg << CSI2_CTX_CTRL2_FORMAT_SHIFT),
		dev->regs.csi2a.regs1 + CSI2_CTX_CTRL2(0));

	dev->active->state = VIDEOBUF_ACTIVE;

	omap4_camera_resizer_enable(dev, 1);
	omap4_camera_isif_enable(dev, 1);

	/* Enable Context #0 */
	writel(readl(dev->regs.csi2a.regs1 + CSI2_CTX_CTRL1(0)) |
			CSI2_CTX_CTRL1_CTX_EN,
		dev->regs.csi2a.regs1 + CSI2_CTX_CTRL1(0));

	/* Enable CSI2 Interface */
	writel(readl(dev->regs.csi2a.regs1 + CSI2_CTRL) |
			CSI2_CTRL_IF_EN,
		dev->regs.csi2a.regs1 + CSI2_CTRL);

	//printk("[HL]%s: ---\n", __func__);

	return ret;
}

static dma_addr_t omap4_get_phy_from_user(unsigned long usr_addr,
					  size_t usr_size,
					  unsigned int usr_pgofst,
					  unsigned long *psize,
					  unsigned long *poffset)
{
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma;
	unsigned long this_pfn, prev_pfn;
	unsigned long pages_done, curr_usr_addr, prev_ofst = 0;
	unsigned int offset;
	dma_addr_t dma_handle = 0;
	int ret = 0, const_ofst = 0;

	//printk("[HL]%s: +++\n", __func__);

	/* Don't allow a zero page offset */
	if (!usr_pgofst)
		return -EINVAL;

	offset = usr_addr & ~PAGE_MASK;
	*psize = PAGE_ALIGN(usr_size + offset);

	down_read(&mm->mmap_sem);

	vma = find_vma(mm, usr_addr);
	if (!vma)
		goto out_up;

	if ((usr_addr + *psize) > vma->vm_end)
		goto out_up;

	pages_done = 0;
	prev_pfn = 0; /* kill warning */
	curr_usr_addr = usr_addr;

	while (pages_done < (*psize >> PAGE_SHIFT)) {
		ret = follow_pfn(vma, curr_usr_addr, &this_pfn);
		if (ret)
			break;

		if (pages_done == 0) {
			dma_handle = (this_pfn << PAGE_SHIFT) + offset;
			const_ofst = 0;
		} else {
			*poffset = (this_pfn - prev_pfn) << PAGE_SHIFT;

			/* Make sure the offset between pages is constant */
			if (prev_ofst && (prev_ofst != *poffset)) {
				*poffset = 0;
				dma_handle = 0;
				break;
			}

			prev_ofst = *poffset;
		}

		prev_pfn = this_pfn;
		curr_usr_addr += PAGE_SIZE * usr_pgofst;
		pages_done++;
	}

	//printk("[HL]%s: ---\n", __func__);

out_up:
	//printk("[HL]%s: out_up---\n", __func__);
	up_read(&mm->mmap_sem);
	return dma_handle;
}

static int omap4_videobuf_prepare(struct videobuf_queue *vq,
				  struct videobuf_buffer *vb,
				  enum v4l2_field field)
{
	struct soc_camera_device *icd = vq->priv_data;
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	struct omap4_camera_dev *dev = ici->priv;
	struct omap4_camera_buffer *buf;
	int bytes_per_line;
	int ret;

	//printk("[HL]%s: +++\n", __func__);

	buf = container_of(vb, struct omap4_camera_buffer, vb);

	//printk("[HL]%s: (vb=0x%p) 0x%08lx %zd\n", __func__,
	//	vb, vb->baddr, vb->bsize);

	dev_dbg(icd->dev.parent, "%s (vb=0x%p) 0x%08lx %zd\n", __func__,
		vb, vb->baddr, vb->bsize);

	/* Added list head initialization on alloc */
	WARN_ON(!list_empty(&vb->queue));

	BUG_ON(NULL == icd->current_fmt);

	if (buf->code	!= icd->current_fmt->code ||
	    vb->width	!= icd->user_width ||
	    vb->height	!= icd->user_height ||
	    vb->field	!= field) {
		buf->code	= icd->current_fmt->code;
		vb->width	= icd->user_width;
		vb->height	= icd->user_height;
		vb->field	= field;
		vb->state	= VIDEOBUF_NEEDS_INIT;
	}

#ifdef USE_BIGBLOCK
	bytes_per_line = soc_mbus_bytes_per_line(icd->user_width,
						icd->current_fmt->host_fmt);
	if (bytes_per_line < 0)
		return bytes_per_line;
#else
	if (icd->current_fmt->host_fmt->fourcc == V4L2_PIX_FMT_NV12) {
		bytes_per_line = (vb->width +
			TILER_PAGE - 1) & ~(TILER_PAGE - 1);
	} else {
		bytes_per_line = (vb->width * 2 +
			TILER_PAGE - 1) & ~(TILER_PAGE - 1);
	}
#endif

	vb->size = vb->height * bytes_per_line;
	if (icd->current_fmt->host_fmt->fourcc == V4L2_PIX_FMT_NV12)
		vb->size += (vb->height / 2) * bytes_per_line;

	if (0 != vb->baddr && vb->bsize < vb->size) {
		ret = -EINVAL;
		goto out;
	}

	if (vb->state == VIDEOBUF_NEEDS_INIT) {
		if (vb->memory == V4L2_MEMORY_USERPTR) {
			unsigned long psize = 0;
			unsigned long poffset = 0;
			dma_addr_t paddr;

			paddr = omap4_get_phy_from_user(vb->baddr,
						vb->height *
						    PAGE_ALIGN(bytes_per_line),
						PAGE_ALIGN(bytes_per_line) >>
							PAGE_SHIFT,
						&psize, &poffset);
			if (!paddr) {
				ret = -EFAULT;
				goto fail;
			}

			/*
			 * Get UVC buffer physical address, which might not
			 * be physically contiguous to the Y buffer above.
			 */
			if (icd->current_fmt->host_fmt->fourcc ==
			    V4L2_PIX_FMT_NV12) {
				unsigned long psize_uv = 0;
				unsigned long poffset_uv = 0;
				dma_addr_t paddr_uv;

				paddr_uv = omap4_get_phy_from_user(vb->baddr +
					(PAGE_ALIGN(bytes_per_line) * vb->height),
					(vb->height / 2) * bytes_per_line,
					PAGE_ALIGN(bytes_per_line) >>
						PAGE_SHIFT,
					&psize_uv, &poffset_uv);
				if (!paddr_uv) {
					ret = -EFAULT;
					goto fail;
				}

				dev->buf_phy_uv_addr[vb->i] = paddr_uv;
				dev->buf_phy_uv_dat_ofst[vb->i] = poffset_uv;
				dev->buf_phy_uv_bsize[vb->i] = psize_uv;
			}
			dev->buf_phy_addr[vb->i] = paddr;
			dev->buf_phy_dat_ofst[vb->i] = poffset;
			dev->buf_phy_bsize[vb->i] = psize;
		}

		/* Do nothing since everything is already pre-alocated */
		vb->state = VIDEOBUF_PREPARED;
	}

	//printk("[HL]%s: ---\n", __func__);

	return 0;
fail:
	//printk("[HL]%s: fail---\n", __func__);
	free_buffer(vq, buf);
out:
	//printk("[HL]%s: out---\n", __func__);	
	return ret;
}

/* Called under spinlock_irqsave(&dev->lock, ...) */
static void omap4_videobuf_queue(struct videobuf_queue *vq,
					 struct videobuf_buffer *vb)
{
	struct soc_camera_device *icd = vq->priv_data;
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	struct omap4_camera_dev *dev = ici->priv;

	//printk("[HL]%s: +++\n", __func__);

	dev_dbg(icd->dev.parent, "%s (vb=0x%p) 0x%08lx %zd\n", __func__,
		vb, vb->baddr, vb->bsize);

	vb->state = VIDEOBUF_QUEUED;
	list_add_tail(&vb->queue, &dev->capture);

	if (!dev->active) {
		/*
		 * Because there were no active buffer at this moment,
		 * we are not interested in the return value of
		 * omap4_capture here.
		 */
		dev->active = vb;

		/*
		 * Failure in below functions is impossible after above
		 * conditions
		 */
		omap4_camera_update_buf(dev);
		omap4_camera_capture(dev);
	}

	//printk("[HL]%s: ---\n", __func__);	
}

static void omap4_videobuf_release(struct videobuf_queue *vq,
					   struct videobuf_buffer *vb)
{
	struct soc_camera_device *icd = vq->priv_data;
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	struct omap4_camera_dev *dev = ici->priv;
	unsigned long flags;
	int i = vb->i;

	//printk("[HL]%s: +++, i = %d\n", __func__, i);

	spin_lock_irqsave(&dev->lock, flags);

	if ((vb->state == VIDEOBUF_ACTIVE || vb->state == VIDEOBUF_QUEUED) &&
	    !list_empty(&vb->queue)) {
	    
	    //printk("[HL]%s: (vb->state == VIDEOBUF_ACTIVE || vb->state == VIDEOBUF_QUEUED) && !list_empty(&vb->queue)\n", __func__, i);
		
		vb->state = VIDEOBUF_ERROR;
		list_del_init(&vb->queue);
	}

	spin_unlock_irqrestore(&dev->lock, flags);

	free_buffer(vq, container_of(vb, struct omap4_camera_buffer, vb));

	if (vb->memory == V4L2_MEMORY_MMAP) 
	{
		if (info_release[i]->tiler_handle)
		{
			//printk("[HL]%s: info_release[%d]->tiler_handle = %u\n", __func__, i, info_release[i]->tiler_handle);
			//printk("[HL]%s: info_release[%d]->lump = %u\n", __func__, i, info_release[i]->lump);
			//printk("[HL]%s: info_release[%d]->n_phys_pages = %u\n", __func__, i, info_release[i]->n_phys_pages);
			//int j;
			//for (j = 0; j < info_release[i]->n_phys_pages; j++)
			//	printk("[HL]%s: info_release[%d]->phys_addrs[%d] = %u\n", __func__, i, j, info_release[i]->phys_addrs[j]);
		
			tiler_unpin_block(info_release[i]->tiler_handle);
		}
		else
		{
			printk("[HL]%s: ERR! info_release[%d]->tiler_handle is NULL\n", __func__, i);
			return;
		}
		//printk("[HL]%s: tiler_unpin_block(info_release[%d]->tiler_handle)\n", __func__, i);

		if (info_release[i]->tiler_handle)
		{
			//printk("[HL]%s: info_release[%d]->tiler_handle = %u\n", __func__, i, info_release[i]->tiler_handle);
			tiler_free_block_area(info_release[i]->tiler_handle);
		}
		else
		{
			printk("[HL]%s: ERR! info_release[%d]->tiler_handle is NULL\n", __func__, i);
			return;
		}
		//printk("[HL]%s: tiler_free_block_area(info_release[%d]->tiler_handle)\n", __func__, i);

		//SW4-L1-HL-Camera-FTM-ReleaseMem-00*{_20120209
		if (tiler_heap)
		{
			//SW4-L1-HL-Camera-FTM-ReleaseMem-00+{_20120209
			//printk("[HL]%s: tiler_heap = %u\n", __func__, tiler_heap);
			//SW4-L1-HL-Camera-FTM-ReleaseMem-00+}_20120209

			if (info_release[i]->lump) 
			{
				ion_carveout_free(tiler_heap, info_release[i]->phys_addrs[0],
						  info_release[i]->n_phys_pages*PAGE_SIZE);	
			} 
			else 
			{
				int j;
				for (j = 0; j < info_release[i]->n_phys_pages; j++)
					ion_carveout_free(tiler_heap,
							  info_release[i]->phys_addrs[j], PAGE_SIZE);
			}
		}
		else
		{
			printk("[HL]%s: ERR! tiler_heap is NULL\n", __func__);
			return;		
		}
		//printk("[HL]%s: ion_carveout_free(tiler_heap)\n", __func__);
		//SW4-L1-HL-Camera-FTM-ReleaseMem-00*}_20120209

		kfree(info_release[i]);
		//printk("[HL]%s: kfree(info_release[%d])\n", __func__, i);
		//SW4-L1-HL-Camera-FTM-ReleaseMem-00*}_20120207

		//SW4-L1-HL-Camera-FTM-ICS-00-{_20120111
		#if 0
#ifndef USE_BIGBLOCK
		if (dev->buf_phy_addr[i])
			tiler_free(dev->buf_phy_addr[i]);
		if (dev->buf_phy_uv_addr[i])
			tiler_free(dev->buf_phy_uv_addr[i]);
#endif
		#endif
		//SW4-L1-HL-Camera-FTM-ICS-00-}_20120111

		dev->buf_phy_addr[i] = 0;
		dev->buf_phy_uv_addr[i] = 0;
	}

	//printk("[HL]%s: ---\n", __func__);	
}

static struct videobuf_queue_ops omap4_videobuf_ops = {
	.buf_setup      = omap4_videobuf_setup,
	.buf_prepare    = omap4_videobuf_prepare,
	.buf_queue      = omap4_videobuf_queue,
	.buf_release    = omap4_videobuf_release,
};

static void omap4_camera_init_videobuf(struct videobuf_queue *q,
				       struct soc_camera_device *icd)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	struct omap4_camera_dev *dev = ici->priv;

	//printk("[HL]%s: +++\n", __func__);

	videobuf_queue_dma_contig_init(q,
				       &omap4_videobuf_ops,
				       icd->dev.parent, 
				       &dev->lock,
				       V4L2_BUF_TYPE_VIDEO_CAPTURE,
				       V4L2_FIELD_NONE,
				       sizeof(struct omap4_camera_buffer),
				       icd,
				       q->ext_lock);	//SW4-L1-HL-Camera-FTM-ICS-00*_20111208

	//printk("[HL]%s: ---\n", __func__);	
}

static int omap4_camera_reqbufs(struct soc_camera_device *icd,	//SW4-L1-HL-Camera-FTM-ICS-00*_20111206
			      struct v4l2_requestbuffers *p)
{
	int i;
	int j;	//SW4-L1-HL-Camera-FTM-ICS-00+_20120111

	//printk("[HL]%s: +++\n", __func__);

	if (p->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;
	
	/*
	 * This is for locking debugging only. I removed spinlocks and now I
	 * check whether .prepare is ever called on a linked buffer, or whether
	 * a dma IRQ can occur for an in-work or unlinked buffer. Until now
	 * it hadn't triggered
	 */
	for (i = 0; i < p->count; i++) {
		struct omap4_camera_buffer *buf;

		buf = container_of(icd->vb_vidq.bufs[i],
				   struct omap4_camera_buffer, vb);
		INIT_LIST_HEAD(&buf->vb.queue);
	}

	if (p->memory == V4L2_MEMORY_MMAP) 
	{
		struct soc_camera_host *ici =
					to_soc_camera_host(icd->dev.parent);
		struct omap4_camera_dev *dev = ici->priv;
		int bytes_per_line;
		
#ifdef USE_BIGBLOCK
		phys_addr_t prealloc_base = omap_cam_get_mempool_base();
		unsigned int size;

		bytes_per_line = soc_mbus_bytes_per_line(icd->user_width,
						icd->current_fmt->host_fmt);
		if (bytes_per_line < 0)
			return bytes_per_line;

		size = bytes_per_line * icd->user_height;

		if (icd->current_fmt->host_fmt->fourcc == V4L2_PIX_FMT_NV12)
			size = size * 3 / 2;

		for (i = 0; i < p->count; i++) {
			dev->buf_phy_addr[i] = prealloc_base +
						i * PAGE_ALIGN(size);
			dev->buf_phy_dat_ofst[i] = bytes_per_line;
			dev->buf_phy_bsize[i] = bytes_per_line *
						icd->user_height;
			if (icd->current_fmt->host_fmt->fourcc ==
							V4L2_PIX_FMT_NV12) {
				dev->buf_phy_uv_addr[i] = dev->buf_phy_addr[i] +
							dev->buf_phy_bsize[i];
				dev->buf_phy_uv_dat_ofst[i] = bytes_per_line;
				dev->buf_phy_uv_bsize[i] = bytes_per_line *
							icd->user_height / 2;
			}
		}
#else
		if (icd->current_fmt->host_fmt->fourcc == V4L2_PIX_FMT_NV12) 
		{
			//printk("[HL]%s: icd->current_fmt->host_fmt->fourcc == V4L2_PIX_FMT_NV12, DO NOTHING!!!\n", __func__);

			//SW4-L1-HL-Camera-FTM-ICS-00-{_20120111
			#if 0
			bytes_per_line = (icd->user_width +
				TILER_PAGE - 1) & ~(TILER_PAGE - 1);

			for (i = 0; i < p->count; i++) {
				u32 y_addr = 0, uv_addr = 0;
				//HL+{ICS_20111202
				struct tiler_block_t *omap4_blk;

				omap4_blk->phys		= y_addr;
				omap4_blk->height 	= icd->user_height;
				omap4_blk->width	= icd->user_width;
				omap4_blk->key		= 0;	//SW4-L1-HL-Camera-FTM-ICS-00+_20111205
				omap4_blk->id		= 0;	//SW4-L1-HL-Camera-FTM-ICS-00+_20111205				
				//HL+}ICS_20111202

				if (tiler_alloc(omap4_blk, TILFMT_8BIT)) {	//HL*ICS_20111202
					break;
				}

				//HL+{ICS_20111202
				omap4_blk->phys		= y_addr;
				omap4_blk->height 	= icd->user_height / 2;
				omap4_blk->width	= icd->user_width;
				omap4_blk->key		= 0;	//SW4-L1-HL-Camera-FTM-ICS-00+_20111205
				omap4_blk->id		= 0;	//SW4-L1-HL-Camera-FTM-ICS-00+_20111205				
				tiler_free(omap4_blk);
				//HL+}ICS_20111202

				if (tiler_alloc(omap4_blk, TILFMT_16BIT)) {	//HL*ICS_20111202
					break;
				}				
				dev->buf_phy_addr[i] = y_addr;
				dev->buf_phy_dat_ofst[i] =
					tiler_stride(dev->buf_phy_addr[i]);
				dev->buf_phy_uv_addr[i] = uv_addr;
				dev->buf_phy_uv_dat_ofst[i] =
					tiler_stride(dev->buf_phy_uv_addr[i]);
			}		
			#endif
			//SW4-L1-HL-Camera-FTM-ICS-00-}_20120111			
		} 
		else 
		{
			//printk("[HL]%s: icd->current_fmt->host_fmt->fourcc != V4L2_PIX_FMT_NV12\n", __func__);
			//printk("[HL]%s: p->count = %d\n", __func__, p->count);
			//printk("[HL]%s: w %u h %u\n", __func__, icd->user_width, icd->user_height);

			//SW4-L1-HL-Camera-FTM-ICS-01*{_20120104
			#if 0
			/* Assume YUV422 here */
			bytes_per_line = (icd->user_width * 2 +
				TILER_PAGE - 1) & ~(TILER_PAGE - 1);

			for (i = 0; i < p->count; i++) {
				u32 y_addr = 0;
				//HL+{ICS_20111202
				struct tiler_block_t *omap4_blk;

				omap4_blk->phys		= y_addr;
				omap4_blk->height 	= icd->user_height;
				omap4_blk->width	= icd->user_width;
				omap4_blk->key		= 0;	//SW4-L1-HL-Camera-FTM-ICS-00+_20111205
				omap4_blk->id		= 0;	//SW4-L1-HL-Camera-FTM-ICS-00+_20111205
				//HL+}ICS_20111202

				if (tiler_alloc(omap4_blk, TILFMT_16BIT)) {	//HL*ICS_20111202
					break;
				}

				dev->buf_phy_addr[i] = y_addr;
				dev->buf_phy_dat_ofst[i] =
					tiler_stride(dev->buf_phy_addr[i]);
			}
			#else

				#if 1
				int ret;
				u32 n_phys_pages,n_tiler_pages;
				ion_phys_addr_t addr;

				u32  y_address;
				u32  uv_address;	

				for (i = 0; i < p->count; i++) 
				{
					/*Allocate Y Buffer*/
					ret = tiler_memsize(TILFMT_16BIT,	//New -- TILFMT_16BIT	//Original -- icd->current_fmt->host_fmt->fourcc	//SW4-L1-HL-Camera-FTM-ICS-00*_20120109
									icd->user_width, 
									icd->user_height,
									&n_phys_pages,
									&n_tiler_pages);
					if (ret) 
					{
						pr_err("%s: invalid tiler request w %u h %u fmt %u\n", __func__,
							   icd->user_width, icd->user_height, icd->current_fmt->host_fmt->fourcc);
						return ret;
					}
					
					BUG_ON(!n_phys_pages || !n_tiler_pages);
					
					info = kzalloc(sizeof(struct omap_tiler_info) +
								   sizeof(u32) * n_phys_pages +
								   sizeof(u32) * n_tiler_pages, GFP_KERNEL);

					if (!info)
					{
						printk("[HL]%s: info == NULL, return -ENOMEM\n", __func__);
						return -ENOMEM;
					}
					
					info->n_phys_pages = n_phys_pages;
					info->n_tiler_pages = n_tiler_pages;
					info->phys_addrs = (u32 *)(info + 1);
					info->tiler_addrs = info->phys_addrs + n_phys_pages;
					//printk("[HL]%s: info->n_phys_pages = %u\n", __func__, info->n_phys_pages);
					//printk("[HL]%s: info->n_tiler_pages = %u\n", __func__, info->n_tiler_pages);
					//printk("[HL]%s: info->phys_addrs = %u\n", __func__, info->phys_addrs);
					//printk("[HL]%s: info->tiler_addrs = %u\n", __func__, info->tiler_addrs);					

					info->tiler_handle = tiler_alloc_block_area(TILFMT_16BIT, 	//New -- TILFMT_16BIT	//SW4-L1-HL-Camera-FTM-ICS-00*_20120109
										icd->user_width, 
										icd->user_height,
										&info->tiler_start,
										info->tiler_addrs); /*You can ignore the value returned in info->tiler_addrs*/

					if (IS_ERR_OR_NULL(info->tiler_handle)) {
						ret = PTR_ERR(info->tiler_handle);
						pr_err("%s: failure to allocate address space from tiler\n",
							   __func__);
						//goto err_nomem;
						kfree(info);
						return ret;
					}

					//SW4-L1-HL-Camera-FTM-ICS-00+_20120109
					//printk("[HL]%s: width = %u, height = %u, key = %u, id = %u\n", __func__, info->tiler_handle->blk.width, info->tiler_handle->blk.height, info->tiler_handle->blk.key, info->tiler_handle->blk.id);


					//SW4-L1-HL-Camera-FTM-ICS-00+{_20120111
					/* Begin : Tiler memory malloc by John.Lin */
					addr = ion_carveout_allocate(tiler_heap, n_phys_pages*PAGE_SIZE, 0);
					if (addr == ION_CARVEOUT_ALLOCATE_FAIL) 
					{
						//printk("[HL]%s: addr == ION_CARVEOUT_ALLOCATE_FAIL\n", __func__);
						//printk("[HL]%s: n_phys_pages = %d\n", __func__, n_phys_pages);
						for (j = 0; j < n_phys_pages; j++) 
						{
							addr = ion_carveout_allocate(tiler_heap, PAGE_SIZE, 0);
					
							if (addr == ION_CARVEOUT_ALLOCATE_FAIL) 
							{
								printk("[HL]%s: addr == ION_CARVEOUT_ALLOCATE_FAIL, j = %d\n", __func__, j);
								ret = -ENOMEM;
								pr_err("%s: failed to allocate pages to back "
									"tiler address space\n", __func__);
								tiler_free_block_area(info->tiler_handle);
								if (info->lump)
									ion_carveout_free(tiler_heap, addr, n_phys_pages * PAGE_SIZE);
								else
									for (j -= 1; j >= 0; j--)
										ion_carveout_free(tiler_heap, info->phys_addrs[j], PAGE_SIZE);

							}
							info->phys_addrs[j] = addr;
						}
					} 
					else 
					{
						//printk("[HL]%s: addr != ION_CARVEOUT_ALLOCATE_FAIL\n", __func__);
						info->lump = true;
						for (j = 0; j < n_phys_pages; j++)
						{
							info->phys_addrs[j] = addr + j*PAGE_SIZE;
							//printk("[HL]%s: addr = %u\n", __func__, addr);
							//printk("[HL]%s: info->phys_addrs[%d] = %u\n", __func__, j, info->phys_addrs[j]);
						}
					}
					
					ret = tiler_pin_block(info->tiler_handle, info->phys_addrs, info->n_phys_pages);
					//printk("[HL]%s: tiler_pin_block(info->tiler_handle, info->phys_addrs, info->n_phys_pages), ret = %d\n", __func__, ret);
					if (ret) {
						pr_err("%s: failure to pin pages to tiler\n", __func__);
						tiler_free_block_area(info->tiler_handle);
						if (info->lump)
							ion_carveout_free(tiler_heap, addr, n_phys_pages * PAGE_SIZE);
						else {
							for (j -= 1; j >= 0; j--)
								ion_carveout_free(tiler_heap, info->phys_addrs[j], PAGE_SIZE);
						}

						return ret;
					}
					/* End : Tiler memory malloc by John.Lin */
					//SW4-L1-HL-Camera-FTM-ICS-00+}_20120111

					dev->buf_phy_addr[i] = info->tiler_start;
					dev->buf_phy_dat_ofst[i] = tiler_pstride(&info->tiler_handle->blk);	//New -- tiler_pstride(&info->tiler_handle->blk);	//Original -- tiler_stride(dev->buf_phy_addr[i]);		//SW4-L1-HL-Camera-FTM-ICS-00*_20120109

					//SW4-L1-HL-Camera-FTM-ReleaseMem-00+{_20120203
					//printk("[HL]%s: info = %u\n", __func__, info);
					//printk("[HL]%s: info->tiler_start = %u\n", __func__, info->tiler_start);
					//printk("[HL]%s: info->tiler_handle = %u\n", __func__, &(info->tiler_handle));
					info_release[i] = info;
					//printk("[HL]%s: info_release[%d] = %u\n", __func__, i, info_release[i]);
					//printk("[HL]%s: info_release[%d]->tiler_start = %u\n", __func__, i, info_release[i]->tiler_start);
					//printk("[HL]%s: info_release[%d]->tiler_handle = %u\n", __func__, i, info_release[i]->tiler_handle);
					//SW4-L1-HL-Camera-FTM-ReleaseMem-00+}_20120203

					//SW4-L1-HL-Camera-FTM-ICS-00+_20120109
					//printk("[HL]%s: info->tiler_start = %u, dev->buf_phy_dat_ofst[%d] = %u\n", __func__, info->tiler_start, i, dev->buf_phy_dat_ofst[i]);
				}
				#else
				//printk("[HL]%s: Do Nothing!!\n", __func__);
				#endif
			
			#endif
			//SW4-L1-HL-Camera-FTM-ICS-01*}_20120104

		}
		p->count = i;

		if (p->count <= 0) {
			dev_err(icd->dev.parent, "Couldn't allocate the"
				" necessary TILER memory\n");
			return -ENOMEM;
		}

		for (i = 0; i < p->count; i++) {
			dev_dbg(icd->dev.parent,
					"y=%08lx uv=%08lx\n",
					dev->buf_phy_addr[i],
					dev->buf_phy_uv_addr[i]);
		}
#endif
	}

	//printk("[HL]%s: ---\n", __func__);

	return 0;
}

static void omap4_camera_vm_open(struct vm_area_struct *vma)
{
	struct omap4_camera_dev *dev = vma->vm_private_data;

	//printk("[HL]%s: +++---\n", __func__);

	dev_dbg(dev->dev,
		"vm_open [vma=%08lx-%08lx]\n", vma->vm_start, vma->vm_end);
	dev->mmap_count++;
}

static void omap4_camera_vm_close(struct vm_area_struct *vma)
{
	struct omap4_camera_dev *dev = vma->vm_private_data;

	//printk("[HL]%s: +++---\n", __func__);

	dev_dbg(dev->dev,
		"vm_close [vma=%08lx-%08lx]\n", vma->vm_start, vma->vm_end);
	dev->mmap_count--;
}

static struct vm_operations_struct omap4_camera_vm_ops = {
	.open	= omap4_camera_vm_open,
	.close	= omap4_camera_vm_close,
};

//SW4-L1-HL-Camera-FTM-ICS-00*{_20111208
static int omap4_camera_mmap(struct videobuf_queue *vbq,
			     struct soc_camera_device *icd,
			     struct vm_area_struct *vma)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	struct omap4_camera_dev *dev = ici->priv;
	int i;
	void *pos;
#ifndef USE_BIGBLOCK
	int j = 0, k = 0, m = 0, p = 0, m_increment = 0;
#endif

	//printk("[HL]%s: +++\n", __func__);

	/* look for the buffer to map */
	for (i = 0; i < VIDEO_MAX_FRAME; i++) {
		if (NULL == vbq->bufs[i])
			continue;
		if (V4L2_MEMORY_MMAP != vbq->bufs[i]->memory)
			continue;
		if (vbq->bufs[i]->boff == (vma->vm_pgoff << PAGE_SHIFT))
			break;
	}

	if (VIDEO_MAX_FRAME == i) {
		dev_dbg(dev->dev,
			"offset invalid [offset=0x%lx]\n",
			(vma->vm_pgoff << PAGE_SHIFT));
		return -EINVAL;
	}
	vbq->bufs[i]->baddr = vma->vm_start;

	vma->vm_flags |= VM_RESERVED;
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	vma->vm_ops = &omap4_camera_vm_ops;
	vma->vm_private_data = (void *)dev;

	pos = (void *)dev->buf_phy_addr[i];

#ifdef USE_BIGBLOCK
	if (icd->current_fmt->host_fmt->fourcc == V4L2_PIX_FMT_NV12) 
	{
		//printk("[HL]%s: icd->current_fmt->host_fmt->fourcc == V4L2_PIX_FMT_NV12\n", __func__);	
		/* This assumes that the Y and UV buffers are next to each
		   other */
		if (remap_pfn_range(vma, vma->vm_start,
			dev->buf_phy_addr[i] >> PAGE_SHIFT,
			dev->buf_phy_bsize[i] + dev->buf_phy_uv_bsize[i],
			vma->vm_page_prot))
				return -EAGAIN;
	}
	else 
	{
		//printk("[HL]%s: icd->current_fmt->host_fmt->fourcc != V4L2_PIX_FMT_NV12\n", __func__);
		if (remap_pfn_range(vma, vma->vm_start,
			dev->buf_phy_addr[i] >> PAGE_SHIFT,
			dev->buf_phy_bsize[i], vma->vm_page_prot))
				return -EAGAIN;
	}
#else
	if (icd->current_fmt->host_fmt->fourcc == V4L2_PIX_FMT_NV12) {	
		//printk("[HL]%s: icd->current_fmt->host_fmt->fourcc == V4L2_PIX_FMT_NV12\n", __func__);
		
		p = (icd->user_width +
			TILER_PAGE - 1) & ~(TILER_PAGE - 1);
		m_increment = 64 * TILER_WIDTH;
	} 
	else 
	{
		//printk("[HL]%s: icd->current_fmt->host_fmt->fourcc != V4L2_PIX_FMT_NV12\n", __func__);
	
		p = (icd->user_width * 2 +
			TILER_PAGE - 1) & ~(TILER_PAGE - 1);

		m_increment = 2 * 64 * TILER_WIDTH;
	}

	for (j = 0; j < icd->user_height; j++) {
		vma->vm_pgoff =
			((unsigned long)pos + m) >> PAGE_SHIFT;

		if (remap_pfn_range(vma, vma->vm_start + k,
			((unsigned long)pos + m) >> PAGE_SHIFT,
			p, vma->vm_page_prot))
				return -EAGAIN;
		k += p;
		m += m_increment;
	}
	m = 0;

	/* UV Buffer in case of NV12 format */
	if (icd->current_fmt->host_fmt->fourcc == V4L2_PIX_FMT_NV12) {
		pos = (void *) dev->buf_phy_uv_addr[i];
		/* UV buffer is 2 bpp, but half size, so p remains */
		m_increment = 2 * 64 * TILER_WIDTH;

		/* UV buffer is height / 2*/
		for (j = 0; j < icd->user_height / 2; j++) {
			/* map each page of the line */
			vma->vm_pgoff =
				((unsigned long)pos + m) >> PAGE_SHIFT;

			if (remap_pfn_range(vma, vma->vm_start + k,
				((unsigned long)pos + m) >> PAGE_SHIFT,
				p, vma->vm_page_prot))
				return -EAGAIN;
			k += p;
			m += m_increment;
		}
	}
#endif

	vma->vm_flags &= ~VM_IO; /* using shared anonymous pages */
	dev->mmap_count++;
	dev_dbg(dev->dev, "Exiting %s\n", __func__);

	//printk("[HL]%s: ---\n", __func__);

	return 0;
}
//SW4-L1-HL-Camera-FTM-ICS-00*}_20111208

static unsigned int omap4_camera_poll(struct file *file, poll_table *pt)
{
	struct soc_camera_device *icd = file->private_data;	//SW4-L1-HL-Camera-FTM-ICS-00*_20111206
	struct omap4_camera_buffer *buf;

	//printk("[HL]%s: +++\n", __func__);

	buf = list_entry(icd->vb_vidq.stream.next,
			 struct omap4_camera_buffer, vb.stream);

	poll_wait(file, &buf->vb.done, pt);

	if (buf->vb.state == VIDEOBUF_DONE ||
	    buf->vb.state == VIDEOBUF_ERROR)
		return POLLIN|POLLRDNORM;

	//printk("[HL]%s: ---\n", __func__);

	return 0;
}

static int omap4_camera_querycap(struct soc_camera_host *ici,
				struct v4l2_capability *cap)
{
	strlcpy(cap->card, omap4_camera_driver_description, sizeof(cap->card));
	cap->version = OMAP4_CAM_VERSION_CODE;
	cap->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;

	//printk("[HL]%s: +++---\n", __func__);

	return 0;
}

static int omap4_camera_set_bus_param(struct soc_camera_device *icd,
					__u32 pixfmt)
{
	/* TODO: This basically queries the sensor bus parameters,
		and finds the compatible configuration with the
		one set from the platform_data. Add this here. */

	//printk("[HL]%s: +++---\n", __func__);

	return 0;
}

static int __omap4_camera_streamon(struct soc_camera_device *icd)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	struct omap4_camera_dev *dev = ici->priv;
	struct v4l2_subdev_sensor_interface_parms if_parms;
	int ret = 0;

	//printk("[HL]%s: +++\n", __func__);

	clk_enable(dev->iss_ctrlclk);
	clk_enable(dev->iss_fck);

	ret = v4l2_subdev_call(sd, sensor, g_interface_parms, &if_parms);
	if (ret < 0) {
		dev_err(icd->dev.parent, "Error on g_interface_params (%d)\n",
			 ret);
		goto error;
	}

	dev->if_parms = if_parms;

	omap4_camera_top_reset(dev);

	/* Configure BTE BW_LIMITER field to max recommended value (1 GB) */
	writel((readl(dev->regs.bte + BTE_CTRL) & ~BTE_CTRL_BW_LIMITER_MASK) |
		(18 << BTE_CTRL_BW_LIMITER_SHIFT),
		dev->regs.bte + BTE_CTRL);

	/* Set ISP Input & Trigger HS_VS interrupt on VS Rising */
	writel((readl(dev->regs.top + ISS_CTRL) &
		~ISS_CTRL_INPUT_SEL_MASK) |
		ISS_CTRL_SYNC_DETECT_VS_RAISING | ISS_CTRL_INPUT_SEL_CSI2B,	//HL+CSI
		dev->regs.top + ISS_CTRL);

	//printk("[HL]%s: ISS_CTRL_SYNC_DETECT_VS_RAISING | ISS_CTRL_INPUT_SEL_CSI2B\n", __func__);

	writel(readl(dev->regs.top + ISS_CLKCTRL) |
		ISS_CLKCTRL_CSI2_B | ISS_CLKCTRL_ISP,	//ISS_CLKCTRL_CSI2_A	//HL*CSI
		dev->regs.top + ISS_CLKCTRL);

	/* Wait for HW assertion */
	for (;;) {
		mdelay(1);
		if ((readl(dev->regs.top + ISS_CLKSTAT) &
		     (ISS_CLKCTRL_CSI2_B | ISS_CLKCTRL_ISP)) ==		//ISS_CLKCTRL_CSI2_A	//HL*CSI
		    (ISS_CLKCTRL_CSI2_B | ISS_CLKCTRL_ISP))			//ISS_CLKCTRL_CSI2_A	//HL*CSI
			break;
	}

	omap4_camera_isp_reset(dev);

	/* Enable clocks for ISP submodules */
	writel(readl(dev->regs.isp5_sys1 + ISP5_CTRL) |
		ISP5_CTRL_BL_CLK_ENABLE |
		ISP5_CTRL_ISIF_CLK_ENABLE |
		ISP5_CTRL_RSZ_CLK_ENABLE |
		ISP5_CTRL_IPIPEIF_CLK_ENABLE |
		ISP5_CTRL_SYNC_ENABLE
		| ISP5_CTRL_VD_PULSE_EXT /* only when CSI2 sends data to VP */
		| ISP5_CTRL_PSYNC_CLK_SEL,
		dev->regs.isp5_sys1 + ISP5_CTRL);

	ret = omap4_camera_csi2_reset(dev);
	if (ret)
		goto error;

	ret = omap4_camera_csiphy_init(dev);
	if (ret)
		goto error;

	omap4_camera_csi2_init_ctx(dev);

	/* ISP Components initialization */
	omap4_camera_isif_init(dev);

	omap4_camera_ipipeif_init(dev);

	omap4_camera_resizer_init(dev);

	/* Propagate to ISP Resizer */
	omap4_camera_isif_set_size(dev, icd->user_width, icd->user_height);
	omap4_camera_resizer_set_out_size(dev, icd->user_width, icd->user_height);
	omap4_camera_resizer_set_src_rect(dev, dev->rect);
	omap4_camera_resizer_enable_colorconv(dev,
			icd->current_fmt->host_fmt->fourcc == V4L2_PIX_FMT_NV12 ? 1 : 0);

	/* Cleanup any probable remaining interrupts */
	writel(readl(dev->regs.top + ISS_HL_IRQSTATUS_5),
		dev->regs.top + ISS_HL_IRQSTATUS_5);

	writel(readl(dev->regs.isp5_sys1 + ISP5_IRQSTATUS(0)),
		dev->regs.isp5_sys1 + ISP5_IRQSTATUS(0));

	writel(readl(dev->regs.isp5_sys2 + ISP5_IRQSTATUS2(0)),
		dev->regs.isp5_sys2 + ISP5_IRQSTATUS2(0));

	/* Enable HL interrupts */
	//HL*{CSI_20111124
	#if 0
	writel(ISS_HL_IRQ_HS_VS	|	//HL+CSI_20111123
		ISS_HL_IRQ_CSIB |		//ISS_HL_IRQ_CSIA		//HL*CSI
		ISS_HL_IRQ_BTE |
		ISS_HL_IRQ_CBUFF |
		ISS_HL_IRQ_ISP0,
		dev->regs.top + ISS_HL_IRQENABLE_5_SET);
	//printk("[HL]%s: Enable ISS_HL_IRQ_HS_VS\n", __func__);
	#else
	writel(ISS_HL_IRQ_CSIB |		//ISS_HL_IRQ_CSIA		//HL*CSI
		ISS_HL_IRQ_BTE |
		ISS_HL_IRQ_CBUFF |
		ISS_HL_IRQ_ISP0,
		dev->regs.top + ISS_HL_IRQENABLE_5_SET);
	//printk("[HL]%s: NO Enable ISS_HL_IRQ_HS_VS\n", __func__);	
	#endif
	//HL*}CSI_20111124

	/* Enable ISP interrupts */
	writel(ISP5_IRQ_OCP_ERR |
		ISP5_IRQ_RSZ_FIFO_IN_BLK |
		ISP5_IRQ_RSZ_FIFO_OVF |
		ISP5_IRQ_RSZ_INT_DMA,
		dev->regs.isp5_sys1 + ISP5_IRQENABLE_SET(0));

	writel(ISP5_IRQ2_IPIPE_HST_ERR |
		ISP5_IRQ2_ISIF_OVF |
		ISP5_IRQ2_IPIPE_BOXCAR_OVF |
		ISP5_IRQ2_IPIPEIF_UDF |
		ISP5_IRQ2_H3A_OVF,
		dev->regs.isp5_sys2 + ISP5_IRQENABLE_SET2(0));

	//HL+{CSI_20111123
	#if 1
	/* Enable Context #0 */
	writel(readl(dev->regs.csi2a.regs1 + CSI2_CTX_CTRL1(0)) |
			CSI2_CTX_CTRL1_CTX_EN,
		dev->regs.csi2a.regs1 + CSI2_CTX_CTRL1(0));

	/* Enable CSI2 Interface */
	writel(readl(dev->regs.csi2a.regs1 + CSI2_CTRL) |
			CSI2_CTRL_IF_EN,
		dev->regs.csi2a.regs1 + CSI2_CTRL);
	//printk("[HL]%s: Enable Context #0 & Enable CSI2 Interface\n", __func__);
	#else
	//printk("[HL]%s: NO Enable Context #0 & Enable CSI2 Interface\n", __func__);	
	#endif
	//HL+}CSI_20111123

	dev->streaming = 1;

	//printk("[HL]%s: ---\n", __func__);
	
	return 0;

//SW4-L1-HL-Camera-FTM-ICS-00+{
error:
	//printk("[HL]%s: error---\n", __func__);
	return ret;
//SW4-L1-HL-Camera-FTM-ICS-00+}
}

//SW4-L1-HL-Camera-FTM-ICS-00*{_20111208
void omap4_camera_streamon(struct soc_camera_device *icd)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	struct omap4_camera_dev *dev = ici->priv;
	int ret;

	//printk("[HL]%s: +++\n", __func__);

	/* Register IRQ */
	ret = request_irq(dev->irq, omap4_camera_isr, 0,
			OMAP4_CAM_DRV_NAME, dev);
	if (ret) {
		dev_err(icd->dev.parent,
			"could not install interrupt service routine\n");
		return;
	}

	if (__omap4_camera_streamon(icd))
		free_irq(dev->irq, dev);

	//printk("[HL]%s: ---\n", __func__);	
}
//SW4-L1-HL-Camera-FTM-ICS-00*}_20111208

static void __omap4_camera_streamoff(struct soc_camera_device *icd)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	struct omap4_camera_dev *dev = ici->priv;
	int timeout;
	unsigned int tmpreg;

	//printk("[HL]%s: +++\n", __func__);

	/* Disable CSI2 Interface */
	writel(readl(dev->regs.csi2a.regs1 + CSI2_CTRL) &
			~CSI2_CTRL_IF_EN,
		dev->regs.csi2a.regs1 + CSI2_CTRL);

	omap4_camera_isif_enable(dev, 0);
	omap4_camera_resizer_enable(dev, 0);
	dev->active = NULL;

	/* Disable HL interrupts */
	writel(ISS_HL_IRQ_CSIB | ISS_HL_IRQ_BTE | ISS_HL_IRQ_CBUFF | ISS_HL_IRQ_ISP0,		//ISS_HL_IRQ_CSIA		//HL*CSI_20111123
		dev->regs.top + ISS_HL_IRQENABLE_5_CLR);

	/* Disable clocks */
	writel(readl(dev->regs.top + ISS_CLKCTRL) &
		~(ISS_CLKCTRL_CSI2_B | ISS_CLKCTRL_ISP),	//ISS_CLKCTRL_CSI2_A	//HL*CSI
		dev->regs.top + ISS_CLKCTRL);

	dev->streaming = 0;

	writel((readl(dev->regs.csi2a.regs1 + CSI2_SYSCONFIG) &
		~CSI2_SYSCONFIG_MSTANDBY_MODE_MASK) |
		CSI2_SYSCONFIG_MSTANDBY_MODE_FORCE |
		CSI2_SYSCONFIG_AUTO_IDLE,
		dev->regs.csi2a.regs1 + CSI2_SYSCONFIG);

	/* Ensure that the ISP is IDLE (no transactions happening) */
	writel((readl(dev->regs.isp5_sys1 + ISP5_SYSCONFIG) &
		~ISP5_SYSCONFIG_STANDBYMODE_MASK) |
		ISP5_SYSCONFIG_STANDBYMODE_FORCE,
		dev->regs.isp5_sys1 + ISP5_SYSCONFIG);

	/* Now, wait for all modules to go to standby until timeout */
	timeout = 1000;
	do {
		mdelay(1);
		tmpreg = readl(dev->regs.top + ISS_PM_STATUS);
		if (tmpreg == 0)
			break;
	} while (--timeout > 0);

	if (timeout == 0)
		dev_warn(dev->dev, "ISS standby transition timeout."
				   " (ISS_PM_STATUS: %08x)\n", tmpreg);

	clk_disable(dev->iss_ctrlclk);
	clk_disable(dev->iss_fck);

	//printk("[HL]%s: ---\n", __func__);
}

//SW4-L1-HL-Camera-FTM-ICS-00*{_20111208
void omap4_camera_streamoff(struct soc_camera_device *icd)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	struct omap4_camera_dev *dev = ici->priv;

	//printk("[HL]%s: +++\n", __func__);

	__omap4_camera_streamoff(icd);

	free_irq(dev->irq, dev);

	//printk("[HL]%s: ---\n", __func__);

}
//SW4-L1-HL-Camera-FTM-ICS-00*}_20111208

static struct soc_camera_host_ops omap4_soc_camera_host_ops = {
	.owner		= THIS_MODULE,
	.add		= omap4_camera_add_device,
	.remove		= omap4_camera_remove_device,
	.get_formats	= omap4_camera_get_formats,
	.set_fmt	= omap4_camera_set_fmt,
	.try_fmt	= omap4_camera_try_fmt,
	.cropcap	= omap4_camera_cropcap,
	.get_crop	= omap4_camera_get_crop,
	.set_crop	= omap4_camera_set_crop,
	/*
	 * Add same callback, just to let soc camera that we allow
	 * on-stream (live) cropping.
	 */
	.set_livecrop	= omap4_camera_set_crop,
	.get_parm	= omap4_camera_get_parm,
	.set_parm	= omap4_camera_set_parm,
	.init_videobuf	= omap4_camera_init_videobuf,
	.reqbufs	= omap4_camera_reqbufs,
	.mmap		= omap4_camera_mmap,				//SW4-L1-HL-Camera-FTM-ICS-00*_20111208
	.poll		= omap4_camera_poll,
	.querycap	= omap4_camera_querycap,
	.set_bus_param	= omap4_camera_set_bus_param,
	.streamon	= omap4_camera_streamon,			//SW4-L1-HL-Camera-FTM-ICS-00*_20111208
	.streamoff	= omap4_camera_streamoff,			//SW4-L1-HL-Camera-FTM-ICS-00*_20111208
};

static int __devinit omap4_camera_probe(struct platform_device *pdev)
{
	struct omap4_camera_dev *omap4cam_dev;
	int irq;
	int err = 0;
	struct resource *res[OMAP4_CAM_MEM_LAST];
	int i;

	//printk("[HL]%s: +++\n", __func__);

	if (!pdev->dev.platform_data) {
		dev_err(&pdev->dev,
			"Platform data not available. Please declare it.\n");
		err = -ENODEV;
		goto exit;
	}

	//printk("[HL]%s: pdev->dev.platform_data is NOT NULL\n", __func__);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		err = -ENODEV;
		goto exit;
	}

	//printk("[HL]%s: platform_get_irq(pdev, 0)\n", __func__);

	for (i = 0; i < OMAP4_CAM_MEM_LAST; i++) {		
		res[i] = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res[i]) {
			err = -ENODEV;
			goto exit;
		}
		
		if (!request_mem_region(res[i]->start, resource_size(res[i]),
					OMAP4_CAM_DRV_NAME)) {
			err = -EBUSY;
			goto exit;
		}		
	}

	//printk("[HL]%s: Resource request successfully!\n", __func__);

	omap4cam_dev = kzalloc(sizeof(*omap4cam_dev), GFP_KERNEL);
	if (!omap4cam_dev) {
		dev_err(&pdev->dev, "Could not allocate omap4cam_dev\n");
		err = -ENOMEM;
		goto exit;
	}

	//printk("[HL]%s: allocate omap4cam_dev\n", __func__);

	INIT_LIST_HEAD(&omap4cam_dev->capture);
	spin_lock_init(&omap4cam_dev->lock);

	for (i = 0; i < OMAP4_CAM_MEM_LAST; i++)
		omap4cam_dev->res[i] = res[i];
	omap4cam_dev->irq = irq;
	omap4cam_dev->pdata = pdev->dev.platform_data;

	omap4cam_dev->regs.top = ioremap(res[OMAP4_CAM_MEM_TOP]->start,
					resource_size(res[OMAP4_CAM_MEM_TOP]));
	if (!omap4cam_dev->regs.top) {
		dev_err(&pdev->dev,
			"Unable to mmap TOP register region\n");
		goto exit_kfree;
	}

	//printk("[HL]%s: mmap TOP register region\n", __func__);

	omap4cam_dev->regs.csi2a.regs1 = ioremap(
				res[OMAP4_CAM_MEM_CSI2_B_REGS1]->start,				//OMAP4_CAM_MEM_CSI2_A_REGS1	//HL*CSI
				resource_size(res[OMAP4_CAM_MEM_CSI2_B_REGS1]));	//OMAP4_CAM_MEM_CSI2_A_REGS1	//HL*CSI
	if (!omap4cam_dev->regs.csi2a.regs1) {
		dev_err(&pdev->dev,
			"Unable to mmap CSI2_B_REGS1 register region\n");		//CSI2_A_REGS1	//HL*CSI
		goto exit_mmap1;
	}

	//printk("[HL]%s: mmap CSI2_B_REGS1 register region\n", __func__);

	//HL+{CSI_20111124
	omap4cam_dev->regs.csi2a.ctrl= ioremap(
				res[OMAP4_CAM_MEM_CTRL]->start,	
				resource_size(res[OMAP4_CAM_MEM_CTRL]));
	if (!omap4cam_dev->regs.csi2a.regs1) {
		dev_err(&pdev->dev,
			"Unable to mmap OMAP4_CAM_MEM_CTRL register region\n");
		goto exit_mmap1;
	}
	//HL+}CSI_20111124

	omap4cam_dev->regs.csi2phy = ioremap(
			res[OMAP4_CAM_MEM_CAMERARX_CORE1]->start,
			resource_size(res[OMAP4_CAM_MEM_CAMERARX_CORE1]));
	if (!omap4cam_dev->regs.csi2phy) {
		dev_err(&pdev->dev,
			"Unable to mmap CAMERARX_CORE1 register region\n");
		goto exit_mmap2;
	}

	omap4cam_dev->regs.bte = ioremap(
			res[OMAP4_CAM_MEM_BTE]->start,
			resource_size(res[OMAP4_CAM_MEM_BTE]));
	if (!omap4cam_dev->regs.bte) {
		dev_err(&pdev->dev,
			"Unable to mmap BTE register region\n");
		goto exit_mmap3;
	}

	/* ioremap for isp5_sys1, resizer and ipipeif */
	omap4cam_dev->regs.isp5_sys1 = ioremap(
			res[OMAP4_CAM_MEM_ISP5_SYS1]->start,
			resource_size(res[OMAP4_CAM_MEM_ISP5_SYS1]));
	if (!omap4cam_dev->regs.isp5_sys1) {
		dev_err(&pdev->dev,
			"Unable to mmap ISP5_SYS1 register region\n");
		goto exit_mmap4;
	}

	omap4cam_dev->regs.isp5_sys2 = ioremap(
			res[OMAP4_CAM_MEM_ISP5_SYS2]->start,
			resource_size(res[OMAP4_CAM_MEM_ISP5_SYS2]));
	if (!omap4cam_dev->regs.isp5_sys2) {
		dev_err(&pdev->dev,
			"Unable to mmap ISP5_SYS2 register region\n");
		goto exit_isp5_sys1;
	}

	omap4cam_dev->regs.resizer = ioremap(
			res[OMAP4_CAM_MEM_RESIZER]->start,
			resource_size(res[OMAP4_CAM_MEM_RESIZER]));
	if (!omap4cam_dev->regs.resizer) {
		dev_err(&pdev->dev,
			"Unable to mmap RESIZER register region\n");
		goto exit_isp5_sys2;
	}

	omap4cam_dev->regs.isif = ioremap(
			res[OMAP4_CAM_MEM_ISIF]->start,
			resource_size(res[OMAP4_CAM_MEM_ISIF]));
	if (!omap4cam_dev->regs.isif) {
		dev_err(&pdev->dev,
			"Unable to mmap ISIF register region\n");
		goto exit_resizer;
	}

	omap4cam_dev->regs.ipipeif = ioremap(
			res[OMAP4_CAM_MEM_IPIPEIF]->start,
			resource_size(res[OMAP4_CAM_MEM_IPIPEIF]));
	if (!omap4cam_dev->regs.ipipeif) {
		dev_err(&pdev->dev,
			"Unable to mmap IPIPEIF register region\n");
		goto exit_isif;
	}

	omap4cam_dev->iss_fck = clk_get(&pdev->dev, "iss_fck");
	if (IS_ERR(omap4cam_dev->iss_fck)) {
		dev_err(&pdev->dev, "Unable to get iss_fck clock info\n");
		err = -ENODEV;
		goto exit_ipipeif;
	}

	omap4cam_dev->iss_ctrlclk = clk_get(&pdev->dev, "iss_ctrlclk");
	if (IS_ERR(omap4cam_dev->iss_ctrlclk)) {
		dev_err(&pdev->dev, "Unable to get iss_ctrlclk clock info\n");
		err = -ENODEV;
		goto exit_iss_fck;
	}

	omap4cam_dev->ducati_clk_mux_ck = clk_get(&pdev->dev,
						  "ducati_clk_mux_ck");
	if (IS_ERR(omap4cam_dev->ducati_clk_mux_ck)) {
		dev_err(&pdev->dev,
			"Unable to get ducati_clk_mux_ck clock info\n");
		err = -ENODEV;
		goto exit_iss_ctrlclk;
	}

	omap4cam_dev->dev = &pdev->dev;

	omap4cam_dev->soc_host.drv_name	= OMAP4_CAM_DRV_NAME;
	omap4cam_dev->soc_host.ops		= &omap4_soc_camera_host_ops;
	omap4cam_dev->soc_host.priv		= omap4cam_dev;
	omap4cam_dev->soc_host.v4l2_dev.dev	= &pdev->dev;
	omap4cam_dev->soc_host.nr		= pdev->id;

	err = soc_camera_host_register(&omap4cam_dev->soc_host);
	if (err) {
		dev_err(&pdev->dev, "SoC camera registration error\n");
		goto exit_ducati_clk_mux_ck;
	}

	dev_info(&pdev->dev, "OMAP4 Camera driver loaded\n");

	//printk("[HL]%s: ---20111208_11\n", __func__);
	
	return 0;

exit_ducati_clk_mux_ck:
	clk_put(omap4cam_dev->ducati_clk_mux_ck);
exit_iss_ctrlclk:
	clk_put(omap4cam_dev->iss_ctrlclk);
exit_iss_fck:
	clk_put(omap4cam_dev->iss_fck);
exit_ipipeif:
	iounmap(omap4cam_dev->regs.ipipeif);
exit_isif:
	iounmap(omap4cam_dev->regs.isif);
exit_resizer:
	iounmap(omap4cam_dev->regs.resizer);
exit_isp5_sys2:
	iounmap(omap4cam_dev->regs.isp5_sys2);
exit_isp5_sys1:
	iounmap(omap4cam_dev->regs.isp5_sys1);
exit_mmap4:
	iounmap(omap4cam_dev->regs.bte);
exit_mmap3:
	iounmap(omap4cam_dev->regs.csi2phy);
exit_mmap2:
	iounmap(omap4cam_dev->regs.csi2a.regs1);
exit_mmap1:
	iounmap(omap4cam_dev->regs.top);
exit_kfree:
	for (i = 0; i < OMAP4_CAM_MEM_LAST; i++) {
		if (omap4cam_dev->res[i]) {
			release_mem_region(omap4cam_dev->res[i]->start,
					resource_size(omap4cam_dev->res[i]));
		}
	}

	kfree(omap4cam_dev);
exit:
	//printk("[HL]%s: exit---\n", __func__);
	return err;
}

static int __devexit omap4_camera_remove(struct platform_device *pdev)
{
	struct soc_camera_host *soc_host = to_soc_camera_host(&pdev->dev);
	struct omap4_camera_dev *omap4cam_dev = container_of(soc_host,
					struct omap4_camera_dev, soc_host);
	int i;

	//printk("[HL]%s: +++\n", __func__);

	soc_camera_host_unregister(soc_host);

	clk_put(omap4cam_dev->ducati_clk_mux_ck);
	clk_put(omap4cam_dev->iss_ctrlclk);
	clk_put(omap4cam_dev->iss_fck);

	iounmap(omap4cam_dev->regs.ipipeif);
	iounmap(omap4cam_dev->regs.isif);
	iounmap(omap4cam_dev->regs.resizer);
	iounmap(omap4cam_dev->regs.isp5_sys2);
	iounmap(omap4cam_dev->regs.isp5_sys1);
	iounmap(omap4cam_dev->regs.bte);
	iounmap(omap4cam_dev->regs.csi2phy);
	iounmap(omap4cam_dev->regs.csi2a.regs1);
	iounmap(omap4cam_dev->regs.top);

	for (i = 0; i < OMAP4_CAM_MEM_LAST; i++) {
		if (omap4cam_dev->res[i]) {
			release_mem_region(omap4cam_dev->res[i]->start,
					resource_size(omap4cam_dev->res[i]));
		}
	}

	kfree(omap4cam_dev);

	dev_info(&pdev->dev, "OMAP4 Camera driver unloaded\n");

	//printk("[HL]%s: ---\n", __func__);

	return 0;
}

static struct platform_driver omap4_camera_driver = {
	.driver = {
		.name	= OMAP4_CAM_DRV_NAME,
	},
	.probe		= omap4_camera_probe,
	.remove		= __devexit_p(omap4_camera_remove),
};

static int __init omap4_camera_init(void)
{
	//printk("[HL]%s: +++---, return platform_driver_register(&omap4_camera_driver)\n", __func__);

	return platform_driver_register(&omap4_camera_driver);
}

static void __exit omap4_camera_exit(void)
{
	platform_driver_unregister(&omap4_camera_driver);
}

/*
 * FIXME: Had to make it late_initcall. Strangely while being module_init,
 * The I2C communication was failing in the sensor, because no XCLK was
 * provided.
 */
late_initcall(omap4_camera_init);
module_exit(omap4_camera_exit);

MODULE_DESCRIPTION("OMAP4 SoC Camera Host driver");
//MODULE_AUTHOR("Sergio Aguirre <saaguirre@ti.com>");
MODULE_AUTHOR("Helli Liu <helliilu@fih-foxconn.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" OMAP4_CAM_DRV_NAME);
