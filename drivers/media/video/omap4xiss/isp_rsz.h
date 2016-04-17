/*
 * isp_rsz.h
 * OMAP4 ISP Resizer module API
 *
 * Copyright (C) 2011 Texas Instruments.
 * Author: Sergio Aguirre <saaguirre@ti.com>
 */

#ifndef _ISP_RSZ_H_
#define _ISP_RSZ_H_

#include "iss_common.h"

int omap4_camera_resizer_isr(struct omap4_camera_dev *iss);
void omap4_camera_resizer_enable(struct omap4_camera_dev *iss, bool enable);
int omap4_camera_resizer_set_src_rect(struct omap4_camera_dev *iss,
				      struct v4l2_rect rect);
int omap4_camera_resizer_set_out_size(struct omap4_camera_dev *iss,
				      u32 width, u32 height);
void omap4_camera_resizer_enable_colorconv(struct omap4_camera_dev *iss,
					   bool enable);
void omap4_camera_resizer_init(struct omap4_camera_dev *iss);

#endif /* _ISP_RSZ_H_ */
