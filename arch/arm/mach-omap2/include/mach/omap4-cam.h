#ifndef __OMAP4_CAM_H__
#define __OMAP4_CAM_H__

#include <linux/platform_device.h>
#include <plat/omap44xx.h>

#define OMAP44XX_ISS_TOP_SIZE				256
#define OMAP44XX_ISS_CSI2_A_REGS1_SIZE		368
#define OMAP44XX_ISS_CSI2_B_REGS1_SIZE		368		//SW4-L1-HL-Camera-FTM-ICS-00+
#define OMAP443X_CTRL_BASE_SIZE				4096	//SW4-L1-HL-Camera-FTM-ICS-00+
#define OMAP44XX_ISS_CAMERARX_CORE1_SIZE	32
#define OMAP44XX_ISS_BTE_SIZE				512
#define OMAP44XX_ISS_ISP5_SYS1_SIZE			160
#define OMAP44XX_ISS_ISP5_SYS2_SIZE			864
#define OMAP44XX_ISS_RESIZER_SIZE			1024
#define OMAP44XX_ISS_IPIPE_SIZE				2048
#define OMAP44XX_ISS_ISIF_SIZE				512
#define OMAP44XX_ISS_IPIPEIF_SIZE			128
#define OMAP44XX_ISS_H3A_SIZE				512

static struct resource omap4_camera_resources[] = {
	{
		.start		= OMAP44XX_ISS_TOP_BASE,
		.end		= OMAP44XX_ISS_TOP_BASE +
				  OMAP44XX_ISS_TOP_SIZE - 1,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP44XX_ISS_CSI2_A_REGS1_BASE,
		.end		= OMAP44XX_ISS_CSI2_A_REGS1_BASE +
				  OMAP44XX_ISS_CSI2_A_REGS1_SIZE - 1,
		.flags		= IORESOURCE_MEM,
	},
	//SW4-L1-HL-Camera-FTM-ICS-00+{
	{
		.start		= OMAP44XX_ISS_CSI2_B_REGS1_BASE,
		.end		= OMAP44XX_ISS_CSI2_B_REGS1_BASE +
				  OMAP44XX_ISS_CSI2_B_REGS1_SIZE - 1,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP443X_CTRL_BASE,
		.end		= OMAP443X_CTRL_BASE +
				  OMAP443X_CTRL_BASE_SIZE - 1,
		.flags		= IORESOURCE_MEM,
	},	
	//SW4-L1-HL-Camera-FTM-ICS-00+}
	{
		.start		= OMAP44XX_ISS_CAMERARX_CORE1_BASE,
		.end		= OMAP44XX_ISS_CAMERARX_CORE1_BASE +
				  OMAP44XX_ISS_CAMERARX_CORE1_SIZE - 1,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP44XX_ISS_BTE_BASE,
		.end		= OMAP44XX_ISS_BTE_BASE +
				  OMAP44XX_ISS_BTE_SIZE - 1,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP44XX_ISS_ISP5_SYS1_BASE,
		.end		= OMAP44XX_ISS_ISP5_SYS1_BASE +
				  OMAP44XX_ISS_ISP5_SYS1_SIZE - 1,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP44XX_ISS_ISP5_SYS2_BASE,
		.end		= OMAP44XX_ISS_ISP5_SYS2_BASE +
				  OMAP44XX_ISS_ISP5_SYS2_SIZE - 1,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP44XX_ISS_RESIZER_BASE,
		.end		= OMAP44XX_ISS_RESIZER_BASE +
				  OMAP44XX_ISS_RESIZER_SIZE - 1,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP44XX_ISS_IPIPE_BASE,
		.end		= OMAP44XX_ISS_IPIPE_BASE +
				  OMAP44XX_ISS_IPIPE_SIZE - 1,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP44XX_ISS_ISIF_BASE,
		.end		= OMAP44XX_ISS_ISIF_BASE +
				  OMAP44XX_ISS_ISIF_SIZE - 1,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP44XX_ISS_IPIPEIF_BASE,
		.end		= OMAP44XX_ISS_IPIPEIF_BASE +
				  OMAP44XX_ISS_IPIPEIF_SIZE - 1,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP44XX_ISS_H3A_BASE,
		.end		= OMAP44XX_ISS_H3A_BASE +
				  OMAP44XX_ISS_H3A_SIZE - 1,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP44XX_IRQ_ISS_5,
		.flags		= IORESOURCE_IRQ,
	},
};

#endif
