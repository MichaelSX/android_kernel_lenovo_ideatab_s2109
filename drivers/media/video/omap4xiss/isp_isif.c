/*
 * ISP ISIF: V4L2 OMAP4 camera host driver
 *
 * Copyright (C) 2011, Texas Instruments
 * Author: Sergio Aguirre <saaguirre@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/kernel.h>
#include "iss_common.h"
#include "ispregs.h"

void omap4_camera_isif_enable(struct omap4_camera_dev *iss, bool enable)
{
	/* NOTE: Shown as RESERVED in TRM register summary! */
	writel((readl(iss->regs.isif + ISIF_SYNCEN) &
		~ISIF_SYNCEN_SYEN) |
		enable ? ISIF_SYNCEN_SYEN : 0,
		iss->regs.isif + ISIF_SYNCEN);

	//printk("[HL]%s +++---\n", __func__);
}

int omap4_camera_isif_set_size(struct omap4_camera_dev *iss,
			       u32 width, u32 height)
{
	writel((0) & ISIF_SPH_MASK, iss->regs.isif + ISIF_SPH);
	writel((width - 1) & ISIF_LNH_MASK, iss->regs.isif + ISIF_LNH);
	writel((height - 1) & ISIF_LNV_MASK, iss->regs.isif + ISIF_LNV);

	//printk("[HL]%s +++---\n", __func__);

	return 0;
}

void omap4_camera_isif_init(struct omap4_camera_dev *iss)
{
	/* FIXME: This is thight to YUV422 sensor usecase */
	/* ISIF input selection */
	writel((readl(iss->regs.isif + ISIF_MODESET) &
		~(ISIF_MODESET_CCDMD | ISIF_MODESET_INPMOD_MASK)) |
		ISIF_MODESET_INPMOD_YCBCR16,	//HL?
		iss->regs.isif + ISIF_MODESET);

	//printk("[HL]%s +++---\n", __func__);	
}

