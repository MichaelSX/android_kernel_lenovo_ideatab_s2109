/*
 * isp_isif.h
 * OMAP4 ISP ISIF module API
 *
 * Copyright (C) 2011 Texas Instruments.
 * Author: Sergio Aguirre <saaguirre@ti.com>
 */

#ifndef _ISP_ISIF_H_
#define _ISP_ISIF_H_

#include "iss_common.h"

void omap4_camera_isif_enable(struct omap4_camera_dev *iss, bool enable);
int omap4_camera_isif_set_size(struct omap4_camera_dev *iss,
			       u32 width, u32 height);
void omap4_camera_isif_init(struct omap4_camera_dev *iss);

#endif /* _ISP_ISIF_H_ */
