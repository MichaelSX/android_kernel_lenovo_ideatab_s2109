/*
 * ISP IPIPEIF: V4L2 OMAP4 camera host driver
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

void omap4_camera_ipipeif_init(struct omap4_camera_dev *iss)
{
	/* IPIPEIF with YUV422 input from ISIF */
	writel(readl(iss->regs.ipipeif + IPIPEIF_CFG1) &
		~(IPIPEIF_CFG1_INPSRC1_MASK | IPIPEIF_CFG1_INPSRC2_MASK),
		iss->regs.ipipeif + IPIPEIF_CFG1);

	writel((readl(iss->regs.ipipeif + IPIPEIF_CFG2) &
		~IPIPEIF_CFG2_YUV8) |
		IPIPEIF_CFG2_YUV16,
		iss->regs.ipipeif + IPIPEIF_CFG2);
}

