/*
 * ispregs.h
 * OMAP4 ISP register definitions file
 *
 * Copyright (C) 2011 Texas Instruments.
 * Author: RaniSuneela <r-m@ti.com>
 */

#ifndef _ISPREGS_H_
#define _ISPREGS_H_

/* ISS ISP_SYS1 Register offsets */
#define ISP5_SYSCONFIG				(0x0010)
#define ISP5_SYSCONFIG_STANDBYMODE_MASK		(3 << 4)
#define ISP5_SYSCONFIG_STANDBYMODE_FORCE	(0 << 4)
#define ISP5_SYSCONFIG_STANDBYMODE_NO		(1 << 4)
#define ISP5_SYSCONFIG_STANDBYMODE_SMART	(2 << 4)
#define ISP5_SYSCONFIG_SOFTRESET		(1 << 1)

#define ISP5_IRQSTATUS(i)			(0x0028 + (0x10 * (i)))
#define ISP5_IRQENABLE_SET(i)			(0x002C + (0x10 * (i)))
#define ISP5_IRQENABLE_CLR(i)			(0x0030 + (0x10 * (i)))

/* Bits shared for ISP5_IRQ* registers */
#define ISP5_IRQ_OCP_ERR			(1 << 31)
#define ISP5_IRQ_RSZ_INT_EOF0			(1 << 22)
#define ISP5_IRQ_RSZ_FIFO_IN_BLK		(1 << 19)
#define ISP5_IRQ_RSZ_FIFO_OVF			(1 << 18)
#define ISP5_IRQ_RSZ_INT_CYC_RSZA		(1 << 16)
#define ISP5_IRQ_RSZ_INT_DMA			(1 << 15)
#define ISP5_IRQ_IPIPEIF			(1 << 9)
#define ISP5_IRQ_ISIF3				(1 << 3)
#define ISP5_IRQ_ISIF2				(1 << 2)
#define ISP5_IRQ_ISIF1				(1 << 1)
#define ISP5_IRQ_ISIF0				(1 << 0)

#define ISP5_CTRL				(0x006C)
#define ISP5_CTRL_MSTANDBY			(1 << 24)
#define ISP5_CTRL_VD_PULSE_EXT			(1 << 23)
#define ISP5_CTRL_MSTANDBY_WAIT			(1 << 20)
#define ISP5_CTRL_BL_CLK_ENABLE			(1 << 15)
#define ISP5_CTRL_ISIF_CLK_ENABLE		(1 << 14)
#define ISP5_CTRL_RSZ_CLK_ENABLE		(1 << 12)
#define ISP5_CTRL_IPIPEIF_CLK_ENABLE		(1 << 10)
#define ISP5_CTRL_SYNC_ENABLE			(1 << 9)
#define ISP5_CTRL_PSYNC_CLK_SEL			(1 << 8)

/* ISS ISP_SYS2 Register offsets */
#define ISP5_IRQSTATUS2(i)			(0x001C + (0x10 * (i)))
#define ISP5_IRQENABLE_SET2(i)			(0x0020 + (0x10 * (i)))
#define ISP5_IRQENABLE_CLR2(i)			(0x0024 + (0x10 * (i)))

/* Bits shared for ISP5_IRQ*2 registers */
#define ISP5_IRQ2_IPIPE_HST_ERR			(1 << 4)
#define ISP5_IRQ2_ISIF_OVF			(1 << 3)
#define ISP5_IRQ2_IPIPE_BOXCAR_OVF		(1 << 2)
#define ISP5_IRQ2_IPIPEIF_UDF			(1 << 1)
#define ISP5_IRQ2_H3A_OVF			(1 << 0)

/* ISS ISP Resizer register offsets */
#define RSZ_REVISION				(0x0000)
#define RSZ_SYSCONFIG				(0x0004)
#define RSZ_SYSCONFIG_RSZB_CLK_EN		(1 << 9)
#define RSZ_SYSCONFIG_RSZA_CLK_EN		(1 << 8)

#define RSZ_IN_FIFO_CTRL			(0x000C)
#define RSZ_IN_FIFO_CTRL_THRLD_LOW_MASK		(0x1FF << 16)
#define RSZ_IN_FIFO_CTRL_THRLD_LOW_SHIFT	16
#define RSZ_IN_FIFO_CTRL_THRLD_HIGH_MASK	(0x1FF << 0)
#define RSZ_IN_FIFO_CTRL_THRLD_HIGH_SHIFT	0

#define RSZ_FRACDIV				(0x0008)
#define RSZ_FRACDIV_MASK			(0xFFFF)

#define RSZ_SRC_EN				(0x0020)
#define RSZ_SRC_EN_SRC_EN			(1 << 0)

#define RSZ_SRC_MODE				(0x0024)
#define RSZ_SRC_MODE_OST			(1 << 0)
#define RSZ_SRC_MODE_WRT			(1 << 1)

#define RSZ_SRC_FMT0				(0x0028)
#define RSZ_SRC_FMT0_BYPASS			(1 << 1)
#define RSZ_SRC_FMT0_SEL			(1 << 0)

#define RSZ_SRC_FMT1				(0x002C)
#define RSZ_SRC_FMT1_IN420			(1 << 1)

#define RSZ_SRC_VPS				(0x0030)
#define RSZ_SRC_VSZ				(0x0034)
#define RSZ_SRC_HPS				(0x0038)
#define RSZ_SRC_HSZ				(0x003C)
#define RSZ_DMA_RZA				(0x0040)
#define RSZ_DMA_RZB				(0x0044)
#define RSZ_DMA_STA				(0x0048)
#define RSZ_GCK_MMR				(0x004C)
#define RSZ_GCK_MMR_MMR				(1 << 0)

#define RSZ_GCK_SDR				(0x0054)
#define RSZ_GCK_SDR_CORE			(1 << 0)

#define RSZ_IRQ_RZA				(0x0058)
#define RSZ_IRQ_RZA_MASK			(0x1FFF)

#define RSZ_IRQ_RZB				(0x005C)
#define RSZ_IRQ_RZB_MASK			(0x1FFF)

#define RSZ_YUV_Y_MIN				(0x0060)
#define RSZ_YUV_Y_MAX				(0x0064)
#define RSZ_YUV_C_MIN				(0x0068)
#define RSZ_YUV_C_MAX				(0x006C)

#define RSZ_SEQ					(0x0074)
#define RSZ_SEQ_HRVB				(1 << 2)
#define RSZ_SEQ_HRVA				(1 << 0)

#define RZA_EN					(0x0078)
#define RZA_MODE				(0x007C)
#define RZA_MODE_ONE_SHOT			(1 << 0)

#define RZA_420					(0x0080)
#define RZA_I_VPS				(0x0084)
#define RZA_I_HPS				(0x0088)
#define RZA_O_VSZ				(0x008C)
#define RZA_O_HSZ				(0x0090)
#define RZA_V_PHS_Y				(0x0094)
#define RZA_V_PHS_C				(0x0098)
#define RZA_V_DIF				(0x009C)
#define RZA_V_TYP				(0x00A0)
#define RZA_V_LPF				(0x00A4)
#define RZA_H_PHS				(0x00A8)
#define RZA_H_DIF				(0x00B0)
#define RZA_H_TYP				(0x00B4)
#define RZA_H_LPF				(0x00B8)
#define RZA_DWN_EN				(0x00BC)
#define RZA_SDR_Y_BAD_H				(0x00D0)
#define RZA_SDR_Y_BAD_L				(0x00D4)
#define RZA_SDR_Y_SAD_H				(0x00D8)
#define RZA_SDR_Y_SAD_L				(0x00DC)
#define RZA_SDR_Y_OFT				(0x00E0)
#define RZA_SDR_Y_PTR_S				(0x00E4)
#define RZA_SDR_Y_PTR_E				(0x00E8)
#define RZA_SDR_C_BAD_H				(0x00EC)
#define RZA_SDR_C_BAD_L				(0x00F0)
#define RZA_SDR_C_SAD_H				(0x00F4)
#define RZA_SDR_C_SAD_L				(0x00F8)
#define RZA_SDR_C_OFT				(0x00FC)
#define RZA_SDR_C_PTR_S				(0x0100)
#define RZA_SDR_C_PTR_E				(0x0104)

#define RZB_EN					(0x0108)
#define RZB_MODE				(0x010C)
#define RZB_420					(0x0110)
#define RZB_I_VPS				(0x0114)
#define RZB_I_HPS				(0x0118)
#define RZB_O_VSZ				(0x011C)
#define RZB_O_HSZ				(0x0120)

#define RZB_V_DIF				(0x012C)
#define RZB_V_TYP				(0x0130)
#define RZB_V_LPF				(0x0134)

#define RZB_H_DIF				(0x0140)
#define RZB_H_TYP				(0x0144)
#define RZB_H_LPF				(0x0148)

#define RZB_SDR_Y_BAD_H				(0x0160)
#define RZB_SDR_Y_BAD_L				(0x0164)
#define RZB_SDR_Y_SAD_H				(0x0168)
#define RZB_SDR_Y_SAD_L				(0x016C)
#define RZB_SDR_Y_OFT				(0x0170)
#define RZB_SDR_Y_PTR_S				(0x0174)
#define RZB_SDR_Y_PTR_E				(0x0178)
#define RZB_SDR_C_BAD_H				(0x017C)
#define RZB_SDR_C_BAD_L				(0x0180)
#define RZB_SDR_C_SAD_H				(0x0184)
#define RZB_SDR_C_SAD_L				(0x0188)

#define RZB_SDR_C_PTR_S				(0x0190)
#define RZB_SDR_C_PTR_E				(0x0194)

/* Shared Bitmasks between RZA & RZB */
#define RSZ_EN_EN				(1 << 0)

#define RSZ_420_CEN				(1 << 1)
#define RSZ_420_YEN				(1 << 0)

#define RSZ_I_VPS_MASK				(0x1FFF)

#define RSZ_I_HPS_MASK				(0x1FFF)

#define RSZ_O_VSZ_MASK				(0x1FFF)

#define RSZ_O_HSZ_MASK				(0x1FFE)

#define RSZ_V_PHS_Y_MASK			(0x3FFF)

#define RSZ_V_PHS_C_MASK			(0x3FFF)

#define RSZ_V_DIF_MASK				(0x3FFF)

#define RSZ_V_TYP_C				(1 << 1)
#define RSZ_V_TYP_Y				(1 << 0)

#define RSZ_V_LPF_C_MASK			(0x3F << 6)
#define RSZ_V_LPF_C_SHIFT			6
#define RSZ_V_LPF_Y_MASK			(0x3F << 0)
#define RSZ_V_LPF_Y_SHIFT			0

#define RSZ_H_PHS_MASK				(0x3FFF)

#define RSZ_H_DIF_MASK				(0x3FFF)

#define RSZ_H_TYP_C				(1 << 1)
#define RSZ_H_TYP_Y				(1 << 0)

#define RSZ_H_LPF_C_MASK			(0x3F << 6)
#define RSZ_H_LPF_C_SHIFT			6
#define RSZ_H_LPF_Y_MASK			(0x3F << 0)
#define RSZ_H_LPF_Y_SHIFT			0

#define RSZ_DWN_EN_DWN_EN			(1 << 0)

/* ISS ISP ISIF register offsets */
#define ISIF_SYNCEN				(0x0000)
#define ISIF_SYNCEN_DWEN			(1 << 1)
#define ISIF_SYNCEN_SYEN			(1 << 0)

#define ISIF_MODESET				(0x0004)
#define ISIF_MODESET_INPMOD_MASK		(3 << 12)
#define ISIF_MODESET_INPMOD_RAW			(0 << 12)
#define ISIF_MODESET_INPMOD_YCBCR16		(1 << 12)
#define ISIF_MODESET_INPMOD_YCBCR8		(2 << 12)
#define ISIF_MODESET_CCDMD			(1 << 7)
#define ISIF_MODESET_SWEN			(1 << 5)
#define ISIF_MODESET_HDPOL			(1 << 3)
#define ISIF_MODESET_VDPOL			(1 << 2)

#define ISIF_SPH				(0x0018)
#define ISIF_SPH_MASK				(0x7FFF)

#define ISIF_LNH				(0x001C)
#define ISIF_LNH_MASK				(0x7FFF)

#define ISIF_LNV				(0x0028)
#define ISIF_LNV_MASK				(0x7FFF)

#define ISIF_HSIZE				(0x0034)
#define ISIF_HSIZE_ADCR				(1 << 12)
#define ISIF_HSIZE_HSIZE_MASK			(0xFFF)

#define ISIF_CADU				(0x003C)
#define ISIF_CADU_MASK				(0x3FF)

#define ISIF_CADL				(0x0040)
#define ISIF_CADL_MASK				(0xFFFF)

#define ISIF_VDINT0				(0x0070)
#define ISIF_VDINT0_MASK			(0x7FFF)

#define ISIF_CCDCFG				(0x0088)
#define ISIF_CCDCFG_Y8POS			(1 << 11)

/* ISS ISP IPIPEIF register offsets */
#define IPIPEIF_ENABLE				(0x0000)

#define IPIPEIF_CFG1				(0x0004)
#define IPIPEIF_CFG1_INPSRC1_MASK		(3 << 14)
#define IPIPEIF_CFG1_INPSRC1_VPORT_RAW		(0 << 14)
#define IPIPEIF_CFG1_INPSRC1_SDRAM_RAW		(1 << 14)
#define IPIPEIF_CFG1_INPSRC1_ISIF_DARKFM	(2 << 14)
#define IPIPEIF_CFG1_INPSRC1_SDRAM_YUV		(3 << 14)
#define IPIPEIF_CFG1_INPSRC2_MASK		(3 << 2)
#define IPIPEIF_CFG1_INPSRC2_ISIF		(0 << 2)
#define IPIPEIF_CFG1_INPSRC2_SDRAM_RAW		(1 << 2)
#define IPIPEIF_CFG1_INPSRC2_ISIF_DARKFM	(2 << 2)
#define IPIPEIF_CFG1_INPSRC2_SDRAM_YUV		(3 << 2)

#define IPIPEIF_CFG2				(0x0030)
#define IPIPEIF_CFG2_YUV8P			(1 << 7)
#define IPIPEIF_CFG2_YUV8			(1 << 6)
#define IPIPEIF_CFG2_YUV16			(1 << 3)
#define IPIPEIF_CFG2_VDPOL			(1 << 2)
#define IPIPEIF_CFG2_HDPOL			(1 << 1)
#define IPIPEIF_CFG2_INTSW			(1 << 0)

#define IPIPEIF_CLKDIV				(0x0040)

#endif /* _OMAP4x_ISP_REGS_H_ */
