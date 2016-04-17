/*
 * V4L2 Driver for OMAP4 camera host: Header file
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
#ifndef _ISS_COMMON_H_
#define _ISS_COMMON_H_

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/version.h>

#include <media/v4l2-common.h>
#include <media/v4l2-dev.h>
#include <media/videobuf-dma-contig.h>
#include <media/soc_camera.h>
#include <media/soc_mediabus.h>
#include <media/omap4_camera.h>

#include <linux/videodev2.h>

#include "omap4_camera_regs.h"
#include "ispregs.h"

enum omap4_camera_memresource {
	OMAP4_CAM_MEM_TOP,
	OMAP4_CAM_MEM_CSI2_A_REGS1,
	OMAP4_CAM_MEM_CSI2_B_REGS1,		//HL+CSI
	OMAP4_CAM_MEM_CTRL,				//HL+CSI	
	OMAP4_CAM_MEM_CAMERARX_CORE1,
	OMAP4_CAM_MEM_BTE,
	OMAP4_CAM_MEM_ISP5_SYS1,
	OMAP4_CAM_MEM_ISP5_SYS2,
	OMAP4_CAM_MEM_RESIZER,
	OMAP4_CAM_MEM_IPIPE,
	OMAP4_CAM_MEM_ISIF,
	OMAP4_CAM_MEM_IPIPEIF,
	OMAP4_CAM_MEM_H3A,
	OMAP4_CAM_MEM_LAST,
};

enum omap4_camera_csi2_ctxfmt {
	ISS_CSI2_CTXFMT_YUV422_8 = 0x1E,
	ISS_CSI2_CTXFMT_RAW_8 = 0x2A,
	ISS_CSI2_CTXFMT_RAW_10 = 0x2B,
	ISS_CSI2_CTXFMT_YUV422_8_VP = 0x9E,
	ISS_CSI2_CTXFMT_RAW_10_EXP16 = 0xAB,
	ISS_CSI2_CTXFMT_YUV422_8_VP16 = 0xDE,
};

struct omap4_camera_iss_csi2_regbases {
	void __iomem				*regs1;
	void __iomem				*ctrl;			//HL+CSI_20111124	
};

struct omap4_camera_iss_regbases {
	void __iomem				*top;
	void __iomem				*csi2phy;
	void __iomem				*bte;
	void __iomem				*isp5_sys1;
	void __iomem				*isp5_sys2;
	void __iomem				*resizer;
	void __iomem				*isif;
	void __iomem				*ipipeif;
	struct omap4_camera_iss_csi2_regbases	csi2a;
};

/* per video frame buffer */
struct omap4_camera_buffer {
	struct videobuf_buffer vb; /* v4l buffer must be first */
	enum v4l2_mbus_pixelcode code;
};

struct omap4_camera_dev {
	struct soc_camera_host		soc_host;
	/*
	 * OMAP4 is only supposed to handle one camera on its CSI2A
	 * interface. If anyone ever builds hardware to enable more than
	 * one camera, they will have to modify this driver too
	 */
	struct soc_camera_device	*icd;

	struct device			*dev;

	struct resource			*res[OMAP4_CAM_MEM_LAST];
	unsigned int			irq;
	struct omap4_camera_pdata	*pdata;
	struct v4l2_subdev_sensor_interface_parms if_parms;

	/* lock used to protect videobuf */
	spinlock_t			lock;
	struct list_head		capture;
	struct videobuf_buffer		*active;

	struct clk			*iss_fck;
	struct clk			*iss_ctrlclk;
	struct clk			*ducati_clk_mux_ck;

	/* keep buffer info across opens */
	unsigned long			buf_phy_addr[VIDEO_MAX_FRAME];
	unsigned long			buf_phy_dat_ofst[VIDEO_MAX_FRAME];
	unsigned long			buf_phy_bsize[VIDEO_MAX_FRAME];

	/* UV buffer for NV12 */
	unsigned long			buf_phy_uv_addr[VIDEO_MAX_FRAME];
	unsigned long			buf_phy_uv_dat_ofst[VIDEO_MAX_FRAME];
	unsigned long			buf_phy_uv_bsize[VIDEO_MAX_FRAME];

	int				mmap_count;

	struct omap4_camera_iss_regbases regs;

	u32				skip_frames;
	u32				pixcode;
	struct v4l2_rect		rect;

	u8				streaming;
};

#define to_iss_device(ptr_module)                               \
	container_of(ptr_module, struct omap4_camera_dev, isp_##ptr_module)

static inline
u32 iss_reg_readl(struct omap4_camera_dev *iss, u32 reg_offset)
{
	return readl(iss->regs.resizer + reg_offset);
}

static inline
void iss_reg_writel(struct omap4_camera_dev *iss, u32 reg_value, u32 reg_offset)
{
	writel(reg_value, iss->regs.resizer + reg_offset);
}

static inline
void iss_reg_clr(struct omap4_camera_dev *iss,  u32 reg, u32 clr_bits)
{
	u32 v = iss_reg_readl(iss, reg);

	iss_reg_writel(iss, v & ~clr_bits, reg);
}

static inline
void iss_reg_set(struct omap4_camera_dev *iss, u32 reg, u32 set_bits)
{
	u32 v = iss_reg_readl(iss, reg);

	iss_reg_writel(iss, v | set_bits, reg);
}

int omap4_camera_update_buf(struct omap4_camera_dev *dev);
#endif

