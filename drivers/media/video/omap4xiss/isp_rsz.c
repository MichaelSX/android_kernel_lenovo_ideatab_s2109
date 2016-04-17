/*
 * ISP Resizer: V4L2 OMAP4 camera host driver
 *
 * Copyright (C) 2011, Texas Instruments
 * Author: RaniSuneela <r-m@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/kernel.h>
#include "iss_common.h"
#include "ispregs.h"

static int is_crop_pending;

static int __omap4_camera_resizer_set_src_rect(struct omap4_camera_dev *iss,
						struct v4l2_rect rect);

int omap4_camera_resizer_isr(struct omap4_camera_dev *iss)
{
	struct videobuf_buffer *vb = iss->active;
	unsigned long flags;

	dev_dbg(iss->dev, "Frame received!\n");

	if (!(readl(iss->regs.resizer + RSZ_SRC_EN) & RSZ_SRC_EN_SRC_EN) ||
		!(readl(iss->regs.resizer + RZA_EN) & RSZ_EN_EN)) {
		dev_err(iss->dev, "Spurious RSZ IRQ!\n");
		goto out;
	}

	if (iss->skip_frames > 0) {
		iss->skip_frames--;
		goto out;
	}

	if (!vb)
		goto out;

	spin_lock_irqsave(&iss->lock, flags);
	list_del_init(&vb->queue);

	if (!list_empty(&iss->capture))
		iss->active = list_entry(
				iss->capture.next,
				struct videobuf_buffer,
				queue);
	else
		iss->active = NULL;

	/* REVISIT: Disabling input I/F when there's no buffer? */
	writel(readl(iss->regs.csi2a.regs1 + CSI2_CTRL) &
		~CSI2_CTRL_IF_EN,
		iss->regs.csi2a.regs1 + CSI2_CTRL);
	iss_reg_clr(iss, RZA_EN, RSZ_EN_EN);
	if (omap4_camera_update_buf(iss) == 0) {
		/* Apply crop before reenabling */
		if (is_crop_pending) {
			__omap4_camera_resizer_set_src_rect(iss, iss->rect);
			is_crop_pending = 0;
		}
		iss_reg_set(iss, RZA_EN, RSZ_EN_EN);
		writel(readl(iss->regs.csi2a.regs1 + CSI2_CTRL) |
			CSI2_CTRL_IF_EN,
			iss->regs.csi2a.regs1 + CSI2_CTRL);
	}

	vb->state = VIDEOBUF_DONE;
	do_gettimeofday(&vb->ts);
	wake_up(&vb->done);

	spin_unlock_irqrestore(&iss->lock, flags);

out:
	return 0;
}

void omap4_camera_resizer_enable(struct omap4_camera_dev *iss, bool enable)
{
	if (enable) {
		iss_reg_set(iss, RSZ_SRC_EN, RSZ_SRC_EN_SRC_EN);
		iss_reg_set(iss, RZA_EN, RSZ_EN_EN);
	} else {
		iss_reg_clr(iss, RSZ_SRC_EN, RSZ_SRC_EN_SRC_EN);
		iss_reg_clr(iss, RZA_EN, RSZ_EN_EN);
	}
}

static int __omap4_camera_resizer_set_src_rect(struct omap4_camera_dev *iss,
						struct v4l2_rect rect)
{
	u32 outv, outh;

	outv = readl(iss->regs.resizer + RZA_O_VSZ);
	outh = readl(iss->regs.resizer + RZA_O_HSZ);

	if ((outh == 0) || (outv == 0)) {
		dev_err(iss->dev, "Can't set source rectangle without output"
				  " size locked in\n");
		return -EINVAL;
	}

	writel(rect.top, iss->regs.resizer + RSZ_SRC_VPS);
	writel(rect.left, iss->regs.resizer + RSZ_SRC_HPS);
	writel(rect.height - 1, iss->regs.resizer + RSZ_SRC_VSZ);
	writel(rect.width - 1, iss->regs.resizer + RSZ_SRC_HSZ);

	/* Ensure no additional cropping is done after SRC_[HV]PS */
	writel(0, iss->regs.resizer + RZA_I_VPS);
	writel(0, iss->regs.resizer + RZA_I_HPS);

	/* Recalculate ratios */
	writel((256 * rect.height) / outv, iss->regs.resizer + RZA_V_DIF);
	writel((256 * rect.width) / outh, iss->regs.resizer + RZA_H_DIF);

	return 0;
}

int omap4_camera_resizer_set_src_rect(struct omap4_camera_dev *iss,
				      struct v4l2_rect rect)
{
	/* If omap4_camera is streaming, we shall wait for the next frame */
	if (iss->streaming) {
		is_crop_pending = 1;
		return 0;
	}

	return __omap4_camera_resizer_set_src_rect(iss, rect);
}

int omap4_camera_resizer_set_out_size(struct omap4_camera_dev *iss,
				      u32 width, u32 height)
{
	writel(height - 1, iss->regs.resizer + RZA_O_VSZ);
	writel(width - 1, iss->regs.resizer + RZA_O_HSZ);

	return 0;
}

void omap4_camera_resizer_enable_colorconv(struct omap4_camera_dev *iss,
					   bool enable)
{
	if (enable)
		iss_reg_set(iss, RZA_420, RSZ_420_CEN | RSZ_420_YEN);
	else
		iss_reg_clr(iss, RZA_420, RSZ_420_CEN | RSZ_420_YEN);
}

void omap4_camera_resizer_init(struct omap4_camera_dev *iss)
{
	iss_reg_set(iss, RSZ_GCK_MMR, RSZ_GCK_MMR_MMR);
	iss_reg_set(iss, RSZ_GCK_SDR, RSZ_GCK_SDR_CORE);

	iss_reg_clr(iss, RSZ_SRC_FMT0, RSZ_SRC_FMT0_BYPASS);

	/* Select IPIPEIF as RSZ input */
	iss_reg_set(iss, RSZ_SRC_FMT0, RSZ_SRC_FMT0_SEL);

	/* RSZ ignores WEN signal from IPIPE/IPIPEIF */
	iss_reg_clr(iss, RSZ_SRC_MODE, RSZ_SRC_MODE_WRT);

	/* Set Resizer in free-running mode */
	iss_reg_clr(iss, RSZ_SRC_MODE, RSZ_SRC_MODE_OST);

	/* Init Resizer A */
	iss_reg_set(iss, RSZ_SYSCONFIG, RSZ_SYSCONFIG_RSZA_CLK_EN);
	iss_reg_clr(iss, RZA_MODE, RZA_MODE_ONE_SHOT);
}

