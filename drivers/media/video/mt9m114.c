/*
 * Aptina MT9M114 sensor driver
 *
 * Copyright (C) 2011, Texas Instruments
 * Copyright (C) 2011, OmniVision
 *
 * Contributors:
 *   Sergio Aguirre <saaguirre@ti.com>
 *   Cristina Warren <cawarren@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/videodev2.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/log2.h>
#include <linux/delay.h>

#include <media/v4l2-subdev.h>
#include <media/v4l2-chip-ident.h>
#include <media/soc_camera.h>

#include "mt9m114_regs.h"

/* MT9M114 has only one fixed colorspace per pixelcode */
struct mt9m114_datafmt {
	enum v4l2_mbus_pixelcode	code;
	enum v4l2_colorspace		colorspace;
};

//HL-{CSI_20111124
#if 0
struct mt9m114_timing_cfg {
	u16 x_addr_start;
	u16 y_addr_start;
	u16 x_addr_end;
	u16 y_addr_end;
	u16 h_output_size;
	u16 v_output_size;
	u16 h_total_size;
	u16 v_total_size;
	u16 isp_h_offset;
	u16 isp_v_offset;
	u8 h_odd_ss_inc;
	u8 h_even_ss_inc;
	u8 v_odd_ss_inc;
	u8 v_even_ss_inc;
};
#endif
//HL-}CSI_20111124

static const struct mt9m114_datafmt mt9m114_fmts[] = {
	/*
	 * Order important: first natively supported,
	 * second supported with a GPIO extender
	 */
	{V4L2_MBUS_FMT_UYVY8_2X8, V4L2_COLORSPACE_JPEG},
	{V4L2_MBUS_FMT_VYUY8_2X8, V4L2_COLORSPACE_JPEG},
	{V4L2_MBUS_FMT_YUYV8_2X8, V4L2_COLORSPACE_JPEG},
	{V4L2_MBUS_FMT_YVYU8_2X8, V4L2_COLORSPACE_JPEG},
};

enum mt9m114_size {
	MT9M114_SIZE_QVGA,
	MT9M114_SIZE_VGA,
	MT9M114_SIZE_720P,
	//MT9M114_SIZE_130MP,	//SW4-L1-HL-Camera-FTM-FixCaptureImageQualityTooLow-00+_20120229
	MT9M114_SIZE_LAST,
};

static const struct v4l2_frmsize_discrete mt9m114_frmsizes[MT9M114_SIZE_LAST] = {
	{  320,  240 },
	{  640,  480 },
	{ 1280,  720 },
	//{ 1280,  960 },		//SW4-L1-HL-Camera-FTM-FixCaptureImageQualityTooLow-00+_20120229
};

/* Find a data format by a pixel code in an array */
static int mt9m114_find_datafmt(enum v4l2_mbus_pixelcode code)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mt9m114_fmts); i++)
		if (mt9m114_fmts[i].code == code)
			break;

	return i;
}

/* Find a frame size in an array */
static int mt9m114_find_framesize(u32 width, u32 height)
{
	int i;

	for (i = 0; i < MT9M114_SIZE_LAST; i++) {
		if ((mt9m114_frmsizes[i].width >= width) &&
		    (mt9m114_frmsizes[i].height >= height))
			break;
	}

	return i;
}

struct mt9m114 {
	struct v4l2_subdev subdev;
	struct v4l2_subdev_sensor_interface_parms *plat_parms;
	int i_size;
	int i_fmt;
	int brightness;
	int contrast;
	int colorlevel;
};

static struct mt9m114 *to_mt9m114(const struct i2c_client *client)
{
	printk("[HL]%s: +++---\n", __func__);

	return container_of(i2c_get_clientdata(client), struct mt9m114, subdev);
}

/**
 * struct mt9m114_reg - mt9m114 register format
 * @reg: 16-bit offset to register
 * @val: 8/16/32-bit register value
 * @length: length of the register
 *
 * Define a structure for MT9M114 register initialization values
 */
struct mt9m114_reg {
	u16	reg;
	u32	val;	//u8					//HL*
	enum mt9m114_width width;		//HL+
	unsigned short mdelay_time;		//HL+
};


static const struct mt9m114_reg init_tbl[] = {
//SW4-L1-HL-Camera-FTM-FixCaptureImageQualityTooLow-00*{_20120229
#if 0
    // A1040_1280x960_1.3Mpix_MIPI_768_fixed_30fps_EXTCLK_24
	{0x098E,        0x0, WORD_LEN, 0 },
	{0xC97E,       0x01, BYTE_LEN, 0 },  //cam_sysctl_pll_enable = 1
	{0xC980,     0x0120, WORD_LEN, 0 },  //cam_sysctl_pll_divider_m_n = 288
	{0xC982,     0x0700, WORD_LEN, 0 },  //cam_sysctl_pll_divider_p = 1792
	{0xC984,     0x8001, WORD_LEN, 0 },  //cam_port_output_control = 32769
	{0xC988,     0x0F00, WORD_LEN, 0 },  //cam_port_mipi_timing_t_hs_zero = 3840
	{0xC98A,     0x0B07, WORD_LEN, 0 },  //cam_port_mipi_timing_t_hs_exit_hs_trail = 2823
	{0xC98C,     0x0D01, WORD_LEN, 0 },  //cam_port_mipi_timing_t_clk_post_clk_pre = 3329
	{0xC98E,     0x071D, WORD_LEN, 0 },  //cam_port_mipi_timing_t_clk_trail_clk_zero = 1821
	{0xC990,     0x0006, WORD_LEN, 0 },  //cam_port_mipi_timing_t_lpx = 6
	{0xC992,     0x0A0C, WORD_LEN, 0 },  //cam_port_mipi_timing_init_timing = 2572
	{0xC800,     0x0004, WORD_LEN, 0 },  //cam_sensor_cfg_y_addr_start = 4
	{0xC802,     0x0004, WORD_LEN, 0 },  //cam_sensor_cfg_x_addr_start = 4
	{0xC804,     0x03CB, WORD_LEN, 0 },  //cam_sensor_cfg_y_addr_end = 971
	{0xC806,     0x050B, WORD_LEN, 0 },  //cam_sensor_cfg_x_addr_end = 1291
	{0xC8080004, 0x02DC6C00, DOUBLE_WORD_LEN, 0 },  //cam_sensor_cfg_pixclk = 48000000
	{0xC80C,     0x0001, WORD_LEN, 0 },  //cam_sensor_cfg_row_speed = 1
	{0xC80E,     0x00DB, WORD_LEN, 0 },  //cam_sensor_cfg_fine_integ_time_min = 219
	{0xC810,     0x05BD, WORD_LEN, 0 },  //cam_sensor_cfg_fine_integ_time_max = 1469
	{0xC812,     0x03E8, WORD_LEN, 0 },  //cam_sensor_cfg_frame_length_lines = 1000
	{0xC814,     0x0640, WORD_LEN, 0 },  //cam_sensor_cfg_line_length_pck = 1600
	{0xC816,     0x0060, WORD_LEN, 0 },  //cam_sensor_cfg_fine_correction = 96
	{0xC818,     0x03C3, WORD_LEN, 0 },  //cam_sensor_cfg_cpipe_last_row = 963
	{0xC826,     0x0020, WORD_LEN, 0 },  //cam_sensor_cfg_reg_0_data = 32
	{0xC834,     0x0000, WORD_LEN, 0 },  //cam_sensor_control_read_mode = 0
	{0xC854,     0x0000, WORD_LEN, 0 },  //cam_crop_window_xoffset = 0
	{0xC856,     0x0000, WORD_LEN, 0 },  //cam_crop_window_yoffset = 0
	{0xC858,     0x0500, WORD_LEN, 0 },  //cam_crop_window_width = 1280
	{0xC85A,     0x03C0, WORD_LEN, 0 },  //cam_crop_window_height = 960
	{0xC85C,       0x03, BYTE_LEN, 0 },  //cam_crop_cropmode = 3
	{0xC868,     0x0500, WORD_LEN, 0 },  //cam_output_width = 1280
	{0xC86A,     0x03C0, WORD_LEN, 0 },  //cam_output_height = 960
	{0xC878,       0x00, BYTE_LEN, 0 },  //cam_aet_aemode = 0
	{0xC88C,     0x1E00, WORD_LEN, 0 },  //cam_aet_max_frame_rate = 7680
	{0xC88E,     0x1E00, WORD_LEN, 0 },  //cam_aet_min_frame_rate = 7680
	{0xC914,     0x0000, WORD_LEN, 0 },  //cam_stat_awb_clip_window_xstart = 0
	{0xC916,     0x0000, WORD_LEN, 0 },  //cam_stat_awb_clip_window_ystart = 0
	{0xC918,     0x04FF, WORD_LEN, 0 },  //cam_stat_awb_clip_window_xend = 1279
	{0xC91A,     0x02CF, WORD_LEN, 0 },  //cam_stat_awb_clip_window_yend = 719
	{0xC91C,     0x0000, WORD_LEN, 0 },  //cam_stat_ae_initial_window_xstart = 0
	{0xC91E,     0x0000, WORD_LEN, 0 },  //cam_stat_ae_initial_window_ystart = 0
	{0xC920,     0x00FF, WORD_LEN, 0 },  //cam_stat_ae_initial_window_xend = 255
	{0xC922,     0x008F, WORD_LEN, 0 },  //cam_stat_ae_initial_window_yend = 143
	
	//SW4-L1-HL-Camera-FTM-FixAEConvergenceTooSlow-00+{_20120222
	// Sensor optimization
	{0x316A, 0x8270, WORD_LEN, 0 },
	{0x316C, 0x8270, WORD_LEN, 0 },
	{0x3ED0, 0x2305, WORD_LEN, 0 },
	{0x3ED2, 0x77CF, WORD_LEN, 0 },
	{0x316E, 0x8202, WORD_LEN, 0 },
	{0x3180, 0x87FF, WORD_LEN, 0 },
	{0x30D4, 0x6080, WORD_LEN, 0 },
	{0xA802, 0x0008, WORD_LEN, 0 },  // AE_TRACK_MODE
	
	// Errata item 1
	{0x3E14, 0xFF39, WORD_LEN, 0 },
	
	// Errata item 3
	// Patch 0202; Feature Recommended; Black level correction fix
	// Patch 02 - Address issue associated black level correction not working correctly in binning mode.
	// Feature Recommended patch for errata item 3
	{0x0982, 0x0000, WORD_LEN, 0 },  // ACCESS_CTL_STAT??????
	{0x098A, 0x5000, WORD_LEN, 0 },  // PHYSICAL_ADDRESS_ACCESS
	{0xD000, 0x70CF, WORD_LEN, 0 },
	{0xD002, 0xFFFF, WORD_LEN, 0 },
	{0xD004, 0xC5D4, WORD_LEN, 0 },
	{0xD006, 0x903A, WORD_LEN, 0 },
	{0xD008, 0x2144, WORD_LEN, 0 },
	{0xD00A, 0x0C00, WORD_LEN, 0 },
	{0xD00C, 0x2186, WORD_LEN, 0 },
	{0xD00E, 0x0FF3, WORD_LEN, 0 },
	{0xD010, 0xB844, WORD_LEN, 0 },
	{0xD012, 0xB948, WORD_LEN, 0 },
	{0xD014, 0xE082, WORD_LEN, 0 },
	{0xD016, 0x20CC, WORD_LEN, 0 },
	{0xD018, 0x80E2, WORD_LEN, 0 },
	{0xD01A, 0x21CC, WORD_LEN, 0 },
	{0xD01C, 0x80A2, WORD_LEN, 0 },
	{0xD01E, 0x21CC, WORD_LEN, 0 },
	{0xD020, 0x80E2, WORD_LEN, 0 },
	{0xD022, 0xF404, WORD_LEN, 0 },
	{0xD024, 0xD801, WORD_LEN, 0 },
	{0xD026, 0xF003, WORD_LEN, 0 },
	{0xD028, 0xD800, WORD_LEN, 0 },
	{0xD02A, 0x7EE0, WORD_LEN, 0 },
	{0xD02C, 0xC0F1, WORD_LEN, 0 },
	{0xD02E, 0x08BA, WORD_LEN, 0 },
	{0xD030, 0x0600, WORD_LEN, 0 },
	{0xD032, 0xC1A1, WORD_LEN, 0 },
	{0xD034, 0x76CF, WORD_LEN, 0 },
	{0xD036, 0xFFFF, WORD_LEN, 0 },
	{0xD038, 0xC130, WORD_LEN, 0 },
	{0xD03A, 0x6E04, WORD_LEN, 0 },
	{0xD03C, 0xC040, WORD_LEN, 0 },
	{0xD03E, 0x71CF, WORD_LEN, 0 },
	{0xD040, 0xFFFF, WORD_LEN, 0 },
	{0xD042, 0xC790, WORD_LEN, 0 },
	{0xD044, 0x8103, WORD_LEN, 0 },
	{0xD046, 0x77CF, WORD_LEN, 0 },
	{0xD048, 0xFFFF, WORD_LEN, 0 },
	{0xD04A, 0xC7C0, WORD_LEN, 0 },
	{0xD04C, 0xE001, WORD_LEN, 0 },
	{0xD04E, 0xA103, WORD_LEN, 0 },
	{0xD050, 0xD800, WORD_LEN, 0 },
	{0xD052, 0x0C6A, WORD_LEN, 0 },
	{0xD054, 0x04E0, WORD_LEN, 0 },
	{0xD056, 0xB89E, WORD_LEN, 0 },
	{0xD058, 0x7508, WORD_LEN, 0 },
	{0xD05A, 0x8E1C, WORD_LEN, 0 },
	{0xD05C, 0x0809, WORD_LEN, 0 },
	{0xD05E, 0x0191, WORD_LEN, 0 },
	{0xD060, 0xD801, WORD_LEN, 0 },
	{0xD062, 0xAE1D, WORD_LEN, 0 },
	{0xD064, 0xE580, WORD_LEN, 0 },
	{0xD066, 0x20CA, WORD_LEN, 0 },
	{0xD068, 0x0022, WORD_LEN, 0 },
	{0xD06A, 0x20CF, WORD_LEN, 0 },
	{0xD06C, 0x0522, WORD_LEN, 0 },
	{0xD06E, 0x0C5C, WORD_LEN, 0 },
	{0xD070, 0x04E2, WORD_LEN, 0 },
	{0xD072, 0x21CA, WORD_LEN, 0 },
	{0xD074, 0x0062, WORD_LEN, 0 },
	{0xD076, 0xE580, WORD_LEN, 0 },
	{0xD078, 0xD901, WORD_LEN, 0 },
	{0xD07A, 0x79C0, WORD_LEN, 0 },
	{0xD07C, 0xD800, WORD_LEN, 0 },
	{0xD07E, 0x0BE6, WORD_LEN, 0 },
	{0xD080, 0x04E0, WORD_LEN, 0 },
	{0xD082, 0xB89E, WORD_LEN, 0 },
	{0xD084, 0x70CF, WORD_LEN, 0 },
	{0xD086, 0xFFFF, WORD_LEN, 0 },
	{0xD088, 0xC8D4, WORD_LEN, 0 },
	{0xD08A, 0x9002, WORD_LEN, 0 },
	{0xD08C, 0x0857, WORD_LEN, 0 },
	{0xD08E, 0x025E, WORD_LEN, 0 },
	{0xD090, 0xFFDC, WORD_LEN, 0 },
	{0xD092, 0xE080, WORD_LEN, 0 },
	{0xD094, 0x25CC, WORD_LEN, 0 },
	{0xD096, 0x9022, WORD_LEN, 0 },
	{0xD098, 0xF225, WORD_LEN, 0 },
	{0xD09A, 0x1700, WORD_LEN, 0 },
	{0xD09C, 0x108A, WORD_LEN, 0 },
	{0xD09E, 0x73CF, WORD_LEN, 0 },
	{0xD0A0, 0xFF00, WORD_LEN, 0 },
	{0xD0A2, 0x3174, WORD_LEN, 0 },
	{0xD0A4, 0x9307, WORD_LEN, 0 },
	{0xD0A6, 0x2A04, WORD_LEN, 0 },
	{0xD0A8, 0x103E, WORD_LEN, 0 },
	{0xD0AA, 0x9328, WORD_LEN, 0 },
	{0xD0AC, 0x2942, WORD_LEN, 0 },
	{0xD0AE, 0x7140, WORD_LEN, 0 },
	{0xD0B0, 0x2A04, WORD_LEN, 0 },
	{0xD0B2, 0x107E, WORD_LEN, 0 },
	{0xD0B4, 0x9349, WORD_LEN, 0 },
	{0xD0B6, 0x2942, WORD_LEN, 0 },
	{0xD0B8, 0x7141, WORD_LEN, 0 },
	{0xD0BA, 0x2A04, WORD_LEN, 0 },
	{0xD0BC, 0x10BE, WORD_LEN, 0 },
	{0xD0BE, 0x934A, WORD_LEN, 0 },
	{0xD0C0, 0x2942, WORD_LEN, 0 },
	{0xD0C2, 0x714B, WORD_LEN, 0 },
	{0xD0C4, 0x2A04, WORD_LEN, 0 },
	{0xD0C6, 0x10BE, WORD_LEN, 0 },
	{0xD0C8, 0x130C, WORD_LEN, 0 },
	{0xD0CA, 0x010A, WORD_LEN, 0 },
	{0xD0CC, 0x2942, WORD_LEN, 0 },
	{0xD0CE, 0x7142, WORD_LEN, 0 },
	{0xD0D0, 0x2250, WORD_LEN, 0 },
	{0xD0D2, 0x13CA, WORD_LEN, 0 },
	{0xD0D4, 0x1B0C, WORD_LEN, 0 },
	{0xD0D6, 0x0284, WORD_LEN, 0 },
	{0xD0D8, 0xB307, WORD_LEN, 0 },
	{0xD0DA, 0xB328, WORD_LEN, 0 },
	{0xD0DC, 0x1B12, WORD_LEN, 0 },
	{0xD0DE, 0x02C4, WORD_LEN, 0 },
	{0xD0E0, 0xB34A, WORD_LEN, 0 },
	{0xD0E2, 0xED88, WORD_LEN, 0 },
	{0xD0E4, 0x71CF, WORD_LEN, 0 },
	{0xD0E6, 0xFF00, WORD_LEN, 0 },
	{0xD0E8, 0x3174, WORD_LEN, 0 },
	{0xD0EA, 0x9106, WORD_LEN, 0 },
	{0xD0EC, 0xB88F, WORD_LEN, 0 },
	{0xD0EE, 0xB106, WORD_LEN, 0 },
	{0xD0F0, 0x210A, WORD_LEN, 0 },
	{0xD0F2, 0x8340, WORD_LEN, 0 },
	{0xD0F4, 0xC000, WORD_LEN, 0 },
	{0xD0F6, 0x21CA, WORD_LEN, 0 },
	{0xD0F8, 0x0062, WORD_LEN, 0 },
	{0xD0FA, 0x20F0, WORD_LEN, 0 },
	{0xD0FC, 0x0040, WORD_LEN, 0 },
	{0xD0FE, 0x0B02, WORD_LEN, 0 },
	{0xD100, 0x0320, WORD_LEN, 0 },
	{0xD102, 0xD901, WORD_LEN, 0 },
	{0xD104, 0x07F1, WORD_LEN, 0 },
	{0xD106, 0x05E0, WORD_LEN, 0 },
	{0xD108, 0xC0A1, WORD_LEN, 0 },
	{0xD10A, 0x78E0, WORD_LEN, 0 },
	{0xD10C, 0xC0F1, WORD_LEN, 0 },
	{0xD10E, 0x71CF, WORD_LEN, 0 },
	{0xD110, 0xFFFF, WORD_LEN, 0 },
	{0xD112, 0xC7C0, WORD_LEN, 0 },
	{0xD114, 0xD840, WORD_LEN, 0 },
	{0xD116, 0xA900, WORD_LEN, 0 },
	{0xD118, 0x71CF, WORD_LEN, 0 },
	{0xD11A, 0xFFFF, WORD_LEN, 0 },
	{0xD11C, 0xD02C, WORD_LEN, 0 },
	{0xD11E, 0xD81E, WORD_LEN, 0 },
	{0xD120, 0x0A5A, WORD_LEN, 0 },
	{0xD122, 0x04E0, WORD_LEN, 0 },
	{0xD124, 0xDA00, WORD_LEN, 0 },
	{0xD126, 0xD800, WORD_LEN, 0 },
	{0xD128, 0xC0D1, WORD_LEN, 0 },
	{0xD12A, 0x7EE0, WORD_LEN, 0 },
	{0x098E, 0x0000, WORD_LEN, 0 },  // LOGICAL_ADDRESS_ACCESS
	
	{0xE000, 0x010C, WORD_LEN, 0 },  // PATCHLDR_LOADER_ADDRESS
	{0xE002, 0x0202, WORD_LEN, 0 },  // PATCHLDR_PATCH_ID
	{0xE004, 0x41030202, DOUBLE_WORD_LEN, 0 },  // PATCHLDR_FIRMWARE_ID

	//SW4-L1-HL-Camera-AWB-FixCloudy-00+{_20120213
	//[Step4-APGA_85]
	//Disable PGA and APGA for LSC
	//{0x098E, 	0x495E, WORD_LEN, 0 },  // LOGICAL_ADDRESS_ACCESS [CAM_PGA_PGA_CONTROL]
	//{0xC95E, 	0x0000, WORD_LEN, 0 },  // CAM_PGA_PGA_CONTROL
	
	//TIMM_OSAL_SleepTask(3);
	
	//LSC 85%_20120209
	{0x3784, 	0x026C, WORD_LEN, 0 },  // CENTER_COLUMN
	{0x3782, 	0x01B8, WORD_LEN, 0 },  // CENTER_ROW
	{0x37C0, 	0x0000, WORD_LEN, 0 },  // P_GR_Q5
	{0x37C2, 	0x0000, WORD_LEN, 0 },  // P_RD_Q5
	{0x37C4, 	0x0000, WORD_LEN, 0 },  // P_BL_Q5
	{0x37C6, 	0x0000, WORD_LEN, 0 },  // P_GB_Q5
	{0x3642, 	0xA7CC, WORD_LEN, 0 },  // P_G1_P0Q1
	{0x3644, 	0x6050, WORD_LEN, 0 },  // P_G1_P0Q2
	{0x3646, 	0xDE2D, WORD_LEN, 0 },  // P_G1_P0Q3
	{0x3648, 	0x08D1, WORD_LEN, 0 },  // P_G1_P0Q4
	{0x364A, 	0x00F0, WORD_LEN, 0 },  // P_R_P0Q0
	{0x364C, 	0xC06C, WORD_LEN, 0 },  // P_R_P0Q1
	{0x364E, 	0x1231, WORD_LEN, 0 },  // P_R_P0Q2
	{0x3650, 	0xA58D, WORD_LEN, 0 },  // P_R_P0Q3
	{0x3652, 	0x27F1, WORD_LEN, 0 },  // P_R_P0Q4
	{0x3654, 	0x0130, WORD_LEN, 0 },  // P_B_P0Q0
	{0x3656, 	0xA2AB, WORD_LEN, 0 },  // P_B_P0Q1
	{0x3658, 	0x35F0, WORD_LEN, 0 },  // P_B_P0Q2
	{0x365A, 	0xCF0D, WORD_LEN, 0 },  // P_B_P0Q3
	{0x365C, 	0x00F1, WORD_LEN, 0 },  // P_B_P0Q4
	{0x3660, 	0xB16C, WORD_LEN, 0 },  // P_G2_P0Q1
	{0x3662, 	0x6610, WORD_LEN, 0 },  // P_G2_P0Q2
	{0x3664, 	0xF64C, WORD_LEN, 0 },  // P_G2_P0Q3
	{0x3666, 	0x6490, WORD_LEN, 0 },  // P_G2_P0Q4
	{0x3680, 	0x930C, WORD_LEN, 0 },  // P_G1_P1Q0
	{0x3682, 	0x568B, WORD_LEN, 0 },  // P_G1_P1Q1
	{0x3684, 	0x0B4E, WORD_LEN, 0 },  // P_G1_P1Q2
	{0x3686, 	0xA02E, WORD_LEN, 0 },  // P_G1_P1Q3
	{0x3688, 	0x08AF, WORD_LEN, 0 },  // P_G1_P1Q4
	{0x368A, 	0xDA8C, WORD_LEN, 0 },  // P_R_P1Q0
	{0x368C, 	0x330D, WORD_LEN, 0 },  // P_R_P1Q1
	{0x368E, 	0xF46B, WORD_LEN, 0 },  // P_R_P1Q2
	{0x3690, 	0xF409, WORD_LEN, 0 },  // P_R_P1Q3
	{0x3692, 	0x516F, WORD_LEN, 0 },  // P_R_P1Q4
	{0x3694, 	0x3606, WORD_LEN, 0 },  // P_B_P1Q0
	{0x3696, 	0x418C, WORD_LEN, 0 },  // P_B_P1Q1
	{0x3698, 	0xC06E, WORD_LEN, 0 },  // P_B_P1Q2
	{0x369A, 	0xBB6E, WORD_LEN, 0 },  // P_B_P1Q3
	{0x369C, 	0xB80E, WORD_LEN, 0 },  // P_B_P1Q4
	{0x369E, 	0xED6A, WORD_LEN, 0 },  // P_G2_P1Q0
	{0x36A0, 	0x6129, WORD_LEN, 0 },  // P_G2_P1Q1
	{0x36A2, 	0xA7CE, WORD_LEN, 0 },  // P_G2_P1Q2
	{0x36A4, 	0xE16E, WORD_LEN, 0 },  // P_G2_P1Q3
	{0x36A6, 	0x9D6D, WORD_LEN, 0 },  // P_G2_P1Q4
	{0x36C0, 	0x70F0, WORD_LEN, 0 },  // P_G1_P2Q0
	{0x36C2, 	0x9E2F, WORD_LEN, 0 },  // P_G1_P2Q1
	{0x36C4, 	0x0993, WORD_LEN, 0 },  // P_G1_P2Q2
	{0x36C6, 	0x0851, WORD_LEN, 0 },  // P_G1_P2Q3
	{0x36C8, 	0x9773, WORD_LEN, 0 },  // P_G1_P2Q4
	{0x36CA, 	0x2BB1, WORD_LEN, 0 },  // P_R_P2Q0
	{0x36CC, 	0x92EF, WORD_LEN, 0 },  // P_R_P2Q1
	{0x36CE, 	0x3ED3, WORD_LEN, 0 },  // P_R_P2Q2
	{0x36D0, 	0x2DB1, WORD_LEN, 0 },  // P_R_P2Q3
	{0x36D2, 	0xFE73, WORD_LEN, 0 },  // P_R_P2Q4
	{0x36D4, 	0x60B0, WORD_LEN, 0 },  // P_B_P2Q0
	{0x36D6, 	0xDEEE, WORD_LEN, 0 },  // P_B_P2Q1
	{0x36D8, 	0x7A72, WORD_LEN, 0 },  // P_B_P2Q2
	{0x36DA, 	0x1091, WORD_LEN, 0 },  // P_B_P2Q3
	{0x36DC, 	0x96D3, WORD_LEN, 0 },  // P_B_P2Q4
	{0x36DE, 	0x73F0, WORD_LEN, 0 },  // P_G2_P2Q0
	{0x36E0, 	0x91AF, WORD_LEN, 0 },  // P_G2_P2Q1
	{0x36E2, 	0x7C52, WORD_LEN, 0 },  // P_G2_P2Q2
	{0x36E4, 	0x6A70, WORD_LEN, 0 },  // P_G2_P2Q3
	{0x36E6, 	0x8CF3, WORD_LEN, 0 },  // P_G2_P2Q4
	{0x3700, 	0x192D, WORD_LEN, 0 },  // P_G1_P3Q0
	{0x3702, 	0x7ACB, WORD_LEN, 0 },  // P_G1_P3Q1
	{0x3704, 	0x8FD1, WORD_LEN, 0 },  // P_G1_P3Q2
	{0x3706, 	0x8CEF, WORD_LEN, 0 },  // P_G1_P3Q3
	{0x3708, 	0x4793, WORD_LEN, 0 },  // P_G1_P3Q4
	{0x370A, 	0x822D, WORD_LEN, 0 },  // P_R_P3Q0
	{0x370C, 	0x9F8E, WORD_LEN, 0 },  // P_R_P3Q1
	{0x370E, 	0xB871, WORD_LEN, 0 },  // P_R_P3Q2
	{0x3710, 	0xDA51, WORD_LEN, 0 },  // P_R_P3Q3
	{0x3712, 	0x1EB4, WORD_LEN, 0 },  // P_R_P3Q4
	{0x3714, 	0x6EC6, WORD_LEN, 0 },  // P_B_P3Q0
	{0x3716, 	0x154E, WORD_LEN, 0 },  // P_B_P3Q1
	{0x3718, 	0x8952, WORD_LEN, 0 },  // P_B_P3Q2
	{0x371A, 	0x8CF1, WORD_LEN, 0 },  // P_B_P3Q3
	{0x371C, 	0x37F4, WORD_LEN, 0 },  // P_B_P3Q4
	{0x371E, 	0x27AE, WORD_LEN, 0 },  // P_G2_P3Q0
	{0x3720, 	0x6AAE, WORD_LEN, 0 },  // P_G2_P3Q1
	{0x3722, 	0x8D12, WORD_LEN, 0 },  // P_G2_P3Q2
	{0x3724, 	0x8670, WORD_LEN, 0 },  // P_G2_P3Q3
	{0x3726, 	0x2AF4, WORD_LEN, 0 },  // P_G2_P3Q4
	{0x3740, 	0x6010, WORD_LEN, 0 },  // P_G1_P4Q0
	{0x3742, 	0x6E2F, WORD_LEN, 0 },  // P_G1_P4Q1
	{0x3744, 	0x02B3, WORD_LEN, 0 },  // P_G1_P4Q2
	{0x3746, 	0x6D92, WORD_LEN, 0 },  // P_G1_P4Q3
	{0x3748, 	0xD436, WORD_LEN, 0 },  // P_G1_P4Q4
	{0x374A, 	0x42B0, WORD_LEN, 0 },  // P_R_P4Q0
	{0x374C, 	0xBE4B, WORD_LEN, 0 },  // P_R_P4Q1
	{0x374E, 	0x62B2, WORD_LEN, 0 },  // P_R_P4Q2
	{0x3750, 	0x3A73, WORD_LEN, 0 },  // P_R_P4Q3
	{0x3752, 	0x8137, WORD_LEN, 0 },  // P_R_P4Q4
	{0x3754, 	0x14EF, WORD_LEN, 0 },  // P_B_P4Q0
	{0x3756, 	0xBC4F, WORD_LEN, 0 },  // P_B_P4Q1
	{0x3758, 	0x3793, WORD_LEN, 0 },  // P_B_P4Q2
	{0x375A, 	0x0533, WORD_LEN, 0 },  // P_B_P4Q3
	{0x375C, 	0xCBF6, WORD_LEN, 0 },  // P_B_P4Q4
	{0x375E, 	0x5A10, WORD_LEN, 0 },  // P_G2_P4Q0
	{0x3760, 	0x0450, WORD_LEN, 0 },  // P_G2_P4Q1
	{0x3762, 	0x25D3, WORD_LEN, 0 },  // P_G2_P4Q2
	{0x3764, 	0x5992, WORD_LEN, 0 },  // P_G2_P4Q3
	{0x3766, 	0xD5B6, WORD_LEN, 0 },  // P_G2_P4Q4
	{0x37C0, 	0x4385, WORD_LEN, 0 },  // P_GR_Q5
	{0x37C2, 	0x810A, WORD_LEN, 0 },  // P_RD_Q5
	{0x37C4, 	0xDD29, WORD_LEN, 0 },  // P_BL_Q5
	{0x37C6, 	0x8A65, WORD_LEN, 10 },  // P_GB_Q5

	//Enable PGA for LSC
	{0x098E, 	0x495E, WORD_LEN, 0 },  // LOGICAL_ADDRESS_ACCESS [CAM_PGA_PGA_CONTROL]
	{0xC95E, 	0x0001, WORD_LEN, 0 },  // CAM_PGA_PGA_CONTROL
	//SW4-L1-HL-Camera-AWB-FixCloudy-00+}_20120213

	// CCM
	{0xC892, 0x0267, WORD_LEN, 0 },  // CAM_AWB_CCM_L_0
	{0xC894, 0xFF1A, WORD_LEN, 0 },  // CAM_AWB_CCM_L_1
	{0xC896, 0xFFB3, WORD_LEN, 0 },  // CAM_AWB_CCM_L_2
	{0xC898, 0xFF80, WORD_LEN, 0 },  // CAM_AWB_CCM_L_3
	{0xC89A, 0x0166, WORD_LEN, 0 },  // CAM_AWB_CCM_L_4
	{0xC89C, 0x0003, WORD_LEN, 0 },  // CAM_AWB_CCM_L_5
	{0xC89E, 0xFF9A, WORD_LEN, 0 },  // CAM_AWB_CCM_L_6
	{0xC8A0, 0xFEB4, WORD_LEN, 0 },  // CAM_AWB_CCM_L_7
	{0xC8A2, 0x024D, WORD_LEN, 0 },  // CAM_AWB_CCM_L_8
	{0xC8A4, 0x01BF, WORD_LEN, 0 },  // CAM_AWB_CCM_M_0
	{0xC8A6, 0xFF01, WORD_LEN, 0 },  // CAM_AWB_CCM_M_1
	{0xC8A8, 0xFFF3, WORD_LEN, 0 },  // CAM_AWB_CCM_M_2
	{0xC8AA, 0xFF75, WORD_LEN, 0 },  // CAM_AWB_CCM_M_3
	{0xC8AC, 0x0198, WORD_LEN, 0 },  // CAM_AWB_CCM_M_4
	{0xC8AE, 0xFFFD, WORD_LEN, 0 },  // CAM_AWB_CCM_M_5
	{0xC8B0, 0xFF9A, WORD_LEN, 0 },  // CAM_AWB_CCM_M_6
	{0xC8B2, 0xFEE7, WORD_LEN, 0 },  // CAM_AWB_CCM_M_7
	{0xC8B4, 0x02A8, WORD_LEN, 0 },  // CAM_AWB_CCM_M_8
	{0xC8B6, 0x01D9, WORD_LEN, 0 },  // CAM_AWB_CCM_R_0
	{0xC8B8, 0xFF26, WORD_LEN, 0 },  // CAM_AWB_CCM_R_1
	{0xC8BA, 0xFFF3, WORD_LEN, 0 },  // CAM_AWB_CCM_R_2
	{0xC8BC, 0xFFB3, WORD_LEN, 0 },  // CAM_AWB_CCM_R_3
	{0xC8BE, 0x0132, WORD_LEN, 0 },  // CAM_AWB_CCM_R_4
	{0xC8C0, 0xFFE8, WORD_LEN, 0 },  // CAM_AWB_CCM_R_5
	{0xC8C2, 0xFFDA, WORD_LEN, 0 },  // CAM_AWB_CCM_R_6
	{0xC8C4, 0xFECD, WORD_LEN, 0 },  // CAM_AWB_CCM_R_7
	{0xC8C6, 0x02C2, WORD_LEN, 0 },  // CAM_AWB_CCM_R_8
	{0xC8C8, 0x0075, WORD_LEN, 0 },  // CAM_AWB_CCM_L_RG_GAIN
	{0xC8CA, 0x011C, WORD_LEN, 0 },  // CAM_AWB_CCM_L_BG_GAIN
	{0xC8CC, 0x009A, WORD_LEN, 0 },  // CAM_AWB_CCM_M_RG_GAIN
	{0xC8CE, 0x0105, WORD_LEN, 0 },  // CAM_AWB_CCM_M_BG_GAIN
	{0xC8D0, 0x00A4, WORD_LEN, 0 },  // CAM_AWB_CCM_R_RG_GAIN
	{0xC8D2, 0x00AC, WORD_LEN, 0 },  // CAM_AWB_CCM_R_BG_GAIN
	{0xC8D4, 0x0A8C, WORD_LEN, 0 },  // CAM_AWB_CCM_L_CTEMP
	{0xC8D6, 0x0F0A, WORD_LEN, 0 },  // CAM_AWB_CCM_M_CTEMP
	{0xC8D8, 0x1964, WORD_LEN, 0 },  // CAM_AWB_CCM_R_CTEMP

	//SW4-L1-HL-Camera-AEAWBFineTune-00+{_20120221
    //[AE target]
	{0x098E, 0xC87A, WORD_LEN, 0 },  // LOGICAL_ADDRESS_ACCESS [CAM_AET_TARGET_AVERAGE_LUMA]
	{0xC87A,   0x43, BYTE_LEN, 0 },  // CAM_AET_TARGET_AVERAGE_LUMA

	//[AWB]
	{0xC914, 0x0000, WORD_LEN, 0 },  // CAM_STAT_AWB_CLIP_WINDOW_XSTART
	{0xC916, 0x0000, WORD_LEN, 0 },  // CAM_STAT_AWB_CLIP_WI NDOW_YSTART
	{0xC918, 0x04FF, WORD_LEN, 0 },  // CAM_STAT_AWB_CLIP_WINDOW_XEND
	{0xC91A, 0x02CF, WORD_LEN, 0 },  // CAM_STAT_AWB_CLIP_WINDOW_YEND
	{0xC904, 0x0033, WORD_LEN, 0 },  // CAM_AWB_AWB_XSHIFT_PRE_ADJ
	{0xC906, 0x0040, WORD_LEN, 0 },  // CAM_AWB_AWB_YSHIFT_PRE_ADJ
	{0xC8F2,   0x03, BYTE_LEN, 0 },  // CAM_AWB_AWB_XSCALE
	{0xC8F3,   0x02, BYTE_LEN, 0 },  // CAM_AWB_AWB_YSCALE
	{0xC906, 0x003C, WORD_LEN, 0 },  // CAM_AWB_AWB_YSHIFT_PRE_ADJ
	{0xC8F4, 0x0000, WORD_LEN, 0 },  // CAM_AWB_AWB_WEIGHTS_0
	{0xC8F6, 0x0000, WORD_LEN, 0 },  // CAM_AWB_AWB_WEIGHTS_1
	{0xC8F8, 0x0000, WORD_LEN, 0 },  // CAM_AWB_AWB_WEIGHTS_2
	{0xC8FA, 0xE724, WORD_LEN, 0 },  // CAM_AWB_AWB_WEIGHTS_3
	{0xC8FC, 0x1583, WORD_LEN, 0 },  // CAM_AWB_AWB_WEIGHTS_4
	{0xC8FE, 0x2045, WORD_LEN, 0 },  // CAM_AWB_AWB_WEIGHTS_5
	{0xC900, 0x05DC, WORD_LEN, 0 },  // CAM_AWB_AWB_WEIGHTS_6
	{0xC902, 0x007C, WORD_LEN, 0 },  // CAM_AWB_AWB_WEIGHTS_7
	{0xC90C,   0x80, BYTE_LEN, 0 },  // CAM_AWB_K_R_L
	{0xC90D,   0x80, BYTE_LEN, 0 },  // CAM_AWB_K_G_L
	{0xC90E,   0x80, BYTE_LEN, 0 },  // CAM_AWB_K_B_L
	{0xC90F,   0x88, BYTE_LEN, 0 },  // CAM_AWB_K_R_R
	{0xC910,   0x80, BYTE_LEN, 0 },  // CAM_AWB_K_G_R
	{0xC911,   0x80, BYTE_LEN, 0 },  // CAM_AWB_K_B_R
	//SW4-L1-HL-Camera-AEAWBFineTune-00+}_20120221
	
	//[Step7-CPIPE_Preference]
	{0xC926, 0x0020, WORD_LEN, 0 },  // CAM_LL_START_BRIGHTNESS
	{0xC928, 0x009A, WORD_LEN, 0 },  // CAM_LL_STOP_BRIGHTNESS
	{0xC946, 0x0070, WORD_LEN, 0 },  // CAM_LL_START_GAIN_METRIC
	{0xC948, 0x00F3, WORD_LEN, 0 },  // CAM_LL_STOP_GAIN_METRIC
	{0xC952, 0x0020, WORD_LEN, 0 },  // CAM_LL_START_TARGET_LUMA_BM
	{0xC954, 0x009A, WORD_LEN, 0 },  // CAM_LL_STOP_TARGET_LUMA_BM
	{0xC92A, 0x80, BYTE_LEN, 0 },  // CAM_LL_START_SATURATION
	{0xC92B, 0x4B, BYTE_LEN, 0 },  // CAM_LL_END_SATURATION
	{0xC92C, 0x00, BYTE_LEN, 0 },  // CAM_LL_START_DESATURATION
	{0xC92D, 0xFF, BYTE_LEN, 0 },  // CAM_LL_END_DESATURATION
	{0xC92E, 0x3C, BYTE_LEN, 0 },  // CAM_LL_START_DEMOSAIC
	{0xC92F, 0x02, BYTE_LEN, 0 },  // CAM_LL_START_AP_GAIN
	{0xC930, 0x06, BYTE_LEN, 0 },  // CAM_LL_START_AP_THRESH
	{0xC931, 0x64, BYTE_LEN, 0 },  // CAM_LL_STOP_DEMOSAIC
	{0xC932, 0x01, BYTE_LEN, 0 },  // CAM_LL_STOP_AP_GAIN
	{0xC933, 0x0C, BYTE_LEN, 0 },  // CAM_LL_STOP_AP_THRESH
	{0xC934, 0x3C, BYTE_LEN, 0 },  // CAM_LL_START_NR_RED
	{0xC935, 0x3C, BYTE_LEN, 0 },  // CAM_LL_START_NR_GREEN
	{0xC936, 0x3C, BYTE_LEN, 0 },  // CAM_LL_START_NR_BLUE
	{0xC937, 0x0F, BYTE_LEN, 0 },  // CAM_LL_START_NR_THRESH
	{0xC938, 0x64, BYTE_LEN, 0 },  // CAM_LL_STOP_NR_RED
	{0xC939, 0x64, BYTE_LEN, 0 },  // CAM_LL_STOP_NR_GREEN
	{0xC93A, 0x64, BYTE_LEN, 0 },  // CAM_LL_STOP_NR_BLUE
	{0xC93B, 0x32, BYTE_LEN, 0 },  // CAM_LL_STOP_NR_THRESH
	{0xC93C, 0x0020, WORD_LEN, 0 },  // CAM_LL_START_CONTRAST_BM
	{0xC93E, 0x009A, WORD_LEN, 0 },  // CAM_LL_STOP_CONTRAST_BM
	{0xC940, 0x00DC, WORD_LEN, 0 },  // CAM_LL_GAMMA
	{0xC942, 0x38, BYTE_LEN, 0 },  // CAM_LL_START_CONTRAST_GRADIENT
	{0xC943, 0x30, BYTE_LEN, 0 },  // CAM_LL_STOP_CONTRAST_GRADIENT
	{0xC944, 0x50, BYTE_LEN, 0 },  // CAM_LL_START_CONTRAST_LUMA_PERCENTAGE
	{0xC945, 0x19, BYTE_LEN, 0 },  // CAM_LL_STOP_CONTRAST_LUMA_PERCENTAGE
	{0xC94A, 0x0230, WORD_LEN, 0 },  // CAM_LL_START_FADE_TO_BLACK_LUMA
	{0xC94C, 0x0010, WORD_LEN, 0 },  // CAM_LL_STOP_FADE_TO_BLACK_LUMA
	{0xC94E, 0x01CD, WORD_LEN, 0 },  // CAM_LL_CLUSTER_DC_TH_BM
	{0xC950, 0x05, BYTE_LEN, 0 },  // CAM_LL_CLUSTER_DC_GATE_PERCENTAGE
	{0xC951, 0x40, BYTE_LEN, 0 },  // CAM_LL_SUMMING_SENSITIVITY_FACTOR
	{0xC87B, 0x1B, BYTE_LEN, 0 },  // CAM_AET_TARGET_AVERAGE_LUMA_DARK
	{0xC878, 0x0E, BYTE_LEN, 0 },  // CAM_AET_AEMODE
	{0xC890, 0x0080, WORD_LEN, 0 },  // CAM_AET_TARGET_GAIN
	{0xC886, 0x0100, WORD_LEN, 0 },  // CAM_AET_AE_MAX_VIRT_AGAIN
	{0xC87C, 0x005A, WORD_LEN, 0 },  // CAM_AET_BLACK_CLIPPING_TARGET
	{0xB42A, 0x05, BYTE_LEN, 0 },  // CCM_DELTA_GAIN
	{0xA80A, 0x20, BYTE_LEN, 0 },  // AE_TRACK_AE_TRACKING_DAMPENING_SPEED
	//SW4-L1-HL-Camera-FTM-FixAEConvergenceTooSlow-00+}_20120222

#else

	//HL*{CSI_20111125
	#if 0	//720p
	// MT9M114_128{0x720_720p_MIPI_768_fixed_30fps_EXTCLK_24
	{0x098E, 0x1000, WORD_LEN, 0 },
	{0xC97E, 0x01, BYTE_LEN, 0 },  //cam_sysctl_pll_enable = 1
	{0xC980, 0x0120, WORD_LEN, 0 },  //cam_sysctl_pll_divider_m_n = 288
	{0xC982, 0x0700, WORD_LEN, 0 },  //cam_sysctl_pll_divider_p = 1792
	{0xC984, 0x8001, WORD_LEN, 0 },  //cam_port_output_control = 32769
	{0xC988, 0x0F00, WORD_LEN, 0 },  //cam_port_mipi_timing_t_hs_zero = 3840
	{0xC98A, 0x0B07, WORD_LEN, 0 },  //cam_port_mipi_timing_t_hs_exit_hs_trail = 2823
	{0xC98C, 0x0D01, WORD_LEN, 0 },  //cam_port_mipi_timing_t_clk_post_clk_pre = 3329
	{0xC98E, 0x071D, WORD_LEN, 0 },  //cam_port_mipi_timing_t_clk_trail_clk_zero = 1821
	{0xC990, 0x0006, WORD_LEN, 0 },  //cam_port_mipi_timing_t_lpx = 6
	{0xC992, 0x0A0C, WORD_LEN, 0 },  //cam_port_mipi_timing_init_timing = 2572
	{0xC800, 0x007C, WORD_LEN, 0 },  //cam_sensor_cfg_y_addr_start = 124
	{0xC802, 0x0004, WORD_LEN, 0 },  //cam_sensor_cfg_x_addr_start = 4
	{0xC804, 0x0353, WORD_LEN, 0 },  //cam_sensor_cfg_y_addr_end = 851
	{0xC806, 0x050B, WORD_LEN, 0 },  //cam_sensor_cfg_x_addr_end = 1291
	{0xC808, 0x02DC6C00, DOUBLE_WORD_LEN, 0 },	//cam_sensor_cfg_pixclk = 48000000
	{0xC80C, 0x0001, WORD_LEN, 0 },  //cam_sensor_cfg_row_speed = 1
	{0xC80E, 0x00DB, WORD_LEN, 0 },  //cam_sensor_cfg_fine_integ_time_min = 219
	{0xC810, 0x05BD, WORD_LEN, 0 },  //cam_sensor_cfg_fine_integ_time_max = 1469
	{0xC812, 0x03E8, WORD_LEN, 0 },  //cam_sensor_cfg_frame_length_lines = 1000
	{0xC814, 0x0640, WORD_LEN, 0 },  //cam_sensor_cfg_line_length_pck = 1600
	{0xC816, 0x0060, WORD_LEN, 0 },  //cam_sensor_cfg_fine_correction = 96
	{0xC818, 0x02D3, WORD_LEN, 0 },  //cam_sensor_cfg_cpipe_last_row = 723
	{0xC826, 0x0020, WORD_LEN, 0 },  //cam_sensor_cfg_reg_0_data = 32
	{0xC834, 0x0000, WORD_LEN, 0 },  //cam_sensor_control_read_mode = 0
	{0xC854, 0x0000, WORD_LEN, 0 },  //cam_crop_window_xoffset = 0
	{0xC856, 0x0000, WORD_LEN, 0 },  //cam_crop_window_yoffset = 0
	{0xC858, 0x0500, WORD_LEN, 0 },  //cam_crop_window_width = 1280
	{0xC85A, 0x02D0, WORD_LEN, 0 },  //cam_crop_window_height = 720
	{0xC85C, 0x03, BYTE_LEN, 0 },  //cam_crop_cropmode = 3
	{0xC868, 0x0500, WORD_LEN, 0 },  //cam_output_width = 1280
	{0xC86A, 0x02D0, WORD_LEN, 0 },  //cam_output_height = 720
	{0xC878, 0x00, BYTE_LEN, 0 },  //cam_aet_aemode = 0
	{0xC88C, 0x1E00, WORD_LEN, 0 },  //cam_aet_max_frame_rate = 7680
	{0xC88E, 0x1E00, WORD_LEN, 0 },  //cam_aet_min_frame_rate = 7680
	{0xC914, 0x0000, WORD_LEN, 0 },  //cam_stat_awb_clip_window_xstart = 0
	{0xC916, 0x0000, WORD_LEN, 0 },  //cam_stat_awb_clip_window_ystart = 0
	{0xC918, 0x04FF, WORD_LEN, 0 },  //cam_stat_awb_clip_window_xend = 1279
	{0xC91A, 0x02CF, WORD_LEN, 0 },  //cam_stat_awb_clip_window_yend = 719
	{0xC91C, 0x0000, WORD_LEN, 0 },  //cam_stat_ae_initial_window_xstart = 0
	{0xC91E, 0x0000, WORD_LEN, 0 },  //cam_stat_ae_initial_window_ystart = 0
	{0xC920, 0x00FF, WORD_LEN, 0 },  //cam_stat_ae_initial_window_xend = 255
	{0xC922, 0x008F, WORD_LEN, 0 },  //cam_stat_ae_initial_window_yend = 143

	//[Change-Config]
	{0x098E, 0xDC00, WORD_LEN, 0 },	// LOGICAL_ADDRESS_ACCESS [SYSMGR_NEXT_STATE]
	{0xDC00, 0x28, BYTE_LEN, 0 },	// SYSMGR_NEXT_STATE
	{0x0080, 0x8002, WORD_LEN, 0 },	// COMMAND_REGISTER
	#else	//VGA
	{0x098E, 0x1000, WORD_LEN, 0 },	//
	{0xC97E, 0x01, BYTE_LEN, 0 },  //cam_sysctl_pll_enable = 1
	{0xC980, 0x0120, WORD_LEN, 0 },  //cam_sysctl_pll_divider_m_n = 288
	{0xC982, 0x0700, WORD_LEN, 0 },  //cam_sysctl_pll_divider_p = 1792
	{0xC984, 0x8001, WORD_LEN, 0 },  //cam_port_output_control = 32769
	{0xC988, 0x0F00, WORD_LEN, 0 },  //cam_port_mipi_timing_t_hs_zero = 3840
	{0xC98A, 0x0B07, WORD_LEN, 0 },  //cam_port_mipi_timing_t_hs_exit_hs_trail = 2823
	{0xC98C, 0x0D01, WORD_LEN, 0 },  //cam_port_mipi_timing_t_clk_post_clk_pre = 3329
	{0xC98E, 0x071D, WORD_LEN, 0 },  //cam_port_mipi_timing_t_clk_trail_clk_zero = 1821
	{0xC990, 0x0006, WORD_LEN, 0 },  //cam_port_mipi_timing_t_lpx = 6
	{0xC992, 0x0A0C, WORD_LEN, 0 },  //cam_port_mipi_timing_init_timing = 2572
	{0xC800, 0x0000, WORD_LEN, 0 },  //cam_sensor_cfg_y_addr_start = 0
	{0xC802, 0x0000, WORD_LEN, 0 },  //cam_sensor_cfg_x_addr_start = 0
	{0xC804, 0x03CD, WORD_LEN, 0 },  //cam_sensor_cfg_y_addr_end = 973
	{0xC806, 0x050D, WORD_LEN, 0 },  //cam_sensor_cfg_x_addr_end = 1293
	{0xC808, 0x2DC6C00	 , DOUBLE_WORD_LEN, 0 },  //cam_sensor_cfg_pixclk = 48000000
	{0xC80C, 0x0001, WORD_LEN, 0 },  //cam_sensor_cfg_row_speed = 1
	{0xC80E, 0x01C3, WORD_LEN, 0 },  //cam_sensor_cfg_fine_integ_time_min = 451
	{0xC810, 0x03F7, WORD_LEN, 0 },  //cam_sensor_cfg_fine_integ_time_max = 1015
	{0xC812, 0x0280, WORD_LEN, 0 },  //cam_sensor_cfg_frame_length_lines = 640
	{0xC814, 0x04E2, WORD_LEN, 0 },  //cam_sensor_cfg_line_length_pck = 1250
	{0xC816, 0x00E0, WORD_LEN, 0 },  //cam_sensor_cfg_fine_correction = 224
	{0xC818, 0x01E3, WORD_LEN, 0 },  //cam_sensor_cfg_cpipe_last_row = 483
	{0xC826, 0x0020, WORD_LEN, 0 },  //cam_sensor_cfg_reg_0_data = 32
	{0xC834, 0x0330, WORD_LEN, 0 },  //cam_sensor_control_read_mode = 816
	{0xC854, 0x0000, WORD_LEN, 0 },  //cam_crop_window_xoffset = 0
	{0xC856, 0x0000, WORD_LEN, 0 },  //cam_crop_window_yoffset = 0
	{0xC858, 0x0280, WORD_LEN, 0 },  //cam_crop_window_width = 640
	{0xC85A, 0x01E0, WORD_LEN, 0 },  //cam_crop_window_height = 480
	{0xC85C, 0x03, BYTE_LEN, 0 },  //cam_crop_cropmode = 3
	{0xC868, 0x0280, WORD_LEN, 0 },  //cam_output_width = 640
	{0xC86A, 0x01E0, WORD_LEN, 0 },  //cam_output_height = 480
	{0xC878, 0x00, BYTE_LEN, 0 },  //cam_aet_aemode = 0
	{0xC88C, 0x3C00, WORD_LEN, 0 },  //cam_aet_max_frame_rate = 15360
	{0xC88E, 0x3C00, WORD_LEN, 0 },  //cam_aet_min_frame_rate = 15360
	{0xC914, 0x0000, WORD_LEN, 0 },  //cam_stat_awb_clip_window_xstart = 0
	{0xC916, 0x0000, WORD_LEN, 0 },  //cam_stat_awb_clip_window_ystart = 0
	{0xC918, 0x027F, WORD_LEN, 0 },  //cam_stat_awb_clip_window_xend = 639
	{0xC91A, 0x01DF, WORD_LEN, 0 },  //cam_stat_awb_clip_window_yend = 479
	{0xC91C, 0x0000, WORD_LEN, 0 },  //cam_stat_ae_initial_window_xstart = 0
	{0xC91E, 0x0000, WORD_LEN, 0 },  //cam_stat_ae_initial_window_ystart = 0
	{0xC920, 0x007F, WORD_LEN, 0 },  //cam_stat_ae_initial_window_xend = 127
	{0xC922, 0x005F, WORD_LEN, 0 },  //cam_stat_ae_initial_window_yend = 95

	//SW4-L1-HL-Camera-FTM-FixAEConvergenceTooSlow-00+{_20120222
	// Sensor optimization
	{0x316A, 0x8270, WORD_LEN, 0 },
	{0x316C, 0x8270, WORD_LEN, 0 },
	{0x3ED0, 0x2305, WORD_LEN, 0 },
	{0x3ED2, 0x77CF, WORD_LEN, 0 },
	{0x316E, 0x8202, WORD_LEN, 0 },
	{0x3180, 0x87FF, WORD_LEN, 0 },
	{0x30D4, 0x6080, WORD_LEN, 0 },
	{0xA802, 0x0008, WORD_LEN, 0 },  // AE_TRACK_MODE
	
	// Errata item 1
	{0x3E14, 0xFF39, WORD_LEN, 0 },
	
	// Errata item 3
	// Patch 0202; Feature Recommended; Black level correction fix
	// Patch 02 - Address issue associated black level correction not working correctly in binning mode.
	// Feature Recommended patch for errata item 3
	{0x0982, 0x0000, WORD_LEN, 0 },  // ACCESS_CTL_STAT??????
	{0x098A, 0x5000, WORD_LEN, 0 },  // PHYSICAL_ADDRESS_ACCESS
	{0xD000, 0x70CF, WORD_LEN, 0 },
	{0xD002, 0xFFFF, WORD_LEN, 0 },
	{0xD004, 0xC5D4, WORD_LEN, 0 },
	{0xD006, 0x903A, WORD_LEN, 0 },
	{0xD008, 0x2144, WORD_LEN, 0 },
	{0xD00A, 0x0C00, WORD_LEN, 0 },
	{0xD00C, 0x2186, WORD_LEN, 0 },
	{0xD00E, 0x0FF3, WORD_LEN, 0 },
	{0xD010, 0xB844, WORD_LEN, 0 },
	{0xD012, 0xB948, WORD_LEN, 0 },
	{0xD014, 0xE082, WORD_LEN, 0 },
	{0xD016, 0x20CC, WORD_LEN, 0 },
	{0xD018, 0x80E2, WORD_LEN, 0 },
	{0xD01A, 0x21CC, WORD_LEN, 0 },
	{0xD01C, 0x80A2, WORD_LEN, 0 },
	{0xD01E, 0x21CC, WORD_LEN, 0 },
	{0xD020, 0x80E2, WORD_LEN, 0 },
	{0xD022, 0xF404, WORD_LEN, 0 },
	{0xD024, 0xD801, WORD_LEN, 0 },
	{0xD026, 0xF003, WORD_LEN, 0 },
	{0xD028, 0xD800, WORD_LEN, 0 },
	{0xD02A, 0x7EE0, WORD_LEN, 0 },
	{0xD02C, 0xC0F1, WORD_LEN, 0 },
	{0xD02E, 0x08BA, WORD_LEN, 0 },
	{0xD030, 0x0600, WORD_LEN, 0 },
	{0xD032, 0xC1A1, WORD_LEN, 0 },
	{0xD034, 0x76CF, WORD_LEN, 0 },
	{0xD036, 0xFFFF, WORD_LEN, 0 },
	{0xD038, 0xC130, WORD_LEN, 0 },
	{0xD03A, 0x6E04, WORD_LEN, 0 },
	{0xD03C, 0xC040, WORD_LEN, 0 },
	{0xD03E, 0x71CF, WORD_LEN, 0 },
	{0xD040, 0xFFFF, WORD_LEN, 0 },
	{0xD042, 0xC790, WORD_LEN, 0 },
	{0xD044, 0x8103, WORD_LEN, 0 },
	{0xD046, 0x77CF, WORD_LEN, 0 },
	{0xD048, 0xFFFF, WORD_LEN, 0 },
	{0xD04A, 0xC7C0, WORD_LEN, 0 },
	{0xD04C, 0xE001, WORD_LEN, 0 },
	{0xD04E, 0xA103, WORD_LEN, 0 },
	{0xD050, 0xD800, WORD_LEN, 0 },
	{0xD052, 0x0C6A, WORD_LEN, 0 },
	{0xD054, 0x04E0, WORD_LEN, 0 },
	{0xD056, 0xB89E, WORD_LEN, 0 },
	{0xD058, 0x7508, WORD_LEN, 0 },
	{0xD05A, 0x8E1C, WORD_LEN, 0 },
	{0xD05C, 0x0809, WORD_LEN, 0 },
	{0xD05E, 0x0191, WORD_LEN, 0 },
	{0xD060, 0xD801, WORD_LEN, 0 },
	{0xD062, 0xAE1D, WORD_LEN, 0 },
	{0xD064, 0xE580, WORD_LEN, 0 },
	{0xD066, 0x20CA, WORD_LEN, 0 },
	{0xD068, 0x0022, WORD_LEN, 0 },
	{0xD06A, 0x20CF, WORD_LEN, 0 },
	{0xD06C, 0x0522, WORD_LEN, 0 },
	{0xD06E, 0x0C5C, WORD_LEN, 0 },
	{0xD070, 0x04E2, WORD_LEN, 0 },
	{0xD072, 0x21CA, WORD_LEN, 0 },
	{0xD074, 0x0062, WORD_LEN, 0 },
	{0xD076, 0xE580, WORD_LEN, 0 },
	{0xD078, 0xD901, WORD_LEN, 0 },
	{0xD07A, 0x79C0, WORD_LEN, 0 },
	{0xD07C, 0xD800, WORD_LEN, 0 },
	{0xD07E, 0x0BE6, WORD_LEN, 0 },
	{0xD080, 0x04E0, WORD_LEN, 0 },
	{0xD082, 0xB89E, WORD_LEN, 0 },
	{0xD084, 0x70CF, WORD_LEN, 0 },
	{0xD086, 0xFFFF, WORD_LEN, 0 },
	{0xD088, 0xC8D4, WORD_LEN, 0 },
	{0xD08A, 0x9002, WORD_LEN, 0 },
	{0xD08C, 0x0857, WORD_LEN, 0 },
	{0xD08E, 0x025E, WORD_LEN, 0 },
	{0xD090, 0xFFDC, WORD_LEN, 0 },
	{0xD092, 0xE080, WORD_LEN, 0 },
	{0xD094, 0x25CC, WORD_LEN, 0 },
	{0xD096, 0x9022, WORD_LEN, 0 },
	{0xD098, 0xF225, WORD_LEN, 0 },
	{0xD09A, 0x1700, WORD_LEN, 0 },
	{0xD09C, 0x108A, WORD_LEN, 0 },
	{0xD09E, 0x73CF, WORD_LEN, 0 },
	{0xD0A0, 0xFF00, WORD_LEN, 0 },
	{0xD0A2, 0x3174, WORD_LEN, 0 },
	{0xD0A4, 0x9307, WORD_LEN, 0 },
	{0xD0A6, 0x2A04, WORD_LEN, 0 },
	{0xD0A8, 0x103E, WORD_LEN, 0 },
	{0xD0AA, 0x9328, WORD_LEN, 0 },
	{0xD0AC, 0x2942, WORD_LEN, 0 },
	{0xD0AE, 0x7140, WORD_LEN, 0 },
	{0xD0B0, 0x2A04, WORD_LEN, 0 },
	{0xD0B2, 0x107E, WORD_LEN, 0 },
	{0xD0B4, 0x9349, WORD_LEN, 0 },
	{0xD0B6, 0x2942, WORD_LEN, 0 },
	{0xD0B8, 0x7141, WORD_LEN, 0 },
	{0xD0BA, 0x2A04, WORD_LEN, 0 },
	{0xD0BC, 0x10BE, WORD_LEN, 0 },
	{0xD0BE, 0x934A, WORD_LEN, 0 },
	{0xD0C0, 0x2942, WORD_LEN, 0 },
	{0xD0C2, 0x714B, WORD_LEN, 0 },
	{0xD0C4, 0x2A04, WORD_LEN, 0 },
	{0xD0C6, 0x10BE, WORD_LEN, 0 },
	{0xD0C8, 0x130C, WORD_LEN, 0 },
	{0xD0CA, 0x010A, WORD_LEN, 0 },
	{0xD0CC, 0x2942, WORD_LEN, 0 },
	{0xD0CE, 0x7142, WORD_LEN, 0 },
	{0xD0D0, 0x2250, WORD_LEN, 0 },
	{0xD0D2, 0x13CA, WORD_LEN, 0 },
	{0xD0D4, 0x1B0C, WORD_LEN, 0 },
	{0xD0D6, 0x0284, WORD_LEN, 0 },
	{0xD0D8, 0xB307, WORD_LEN, 0 },
	{0xD0DA, 0xB328, WORD_LEN, 0 },
	{0xD0DC, 0x1B12, WORD_LEN, 0 },
	{0xD0DE, 0x02C4, WORD_LEN, 0 },
	{0xD0E0, 0xB34A, WORD_LEN, 0 },
	{0xD0E2, 0xED88, WORD_LEN, 0 },
	{0xD0E4, 0x71CF, WORD_LEN, 0 },
	{0xD0E6, 0xFF00, WORD_LEN, 0 },
	{0xD0E8, 0x3174, WORD_LEN, 0 },
	{0xD0EA, 0x9106, WORD_LEN, 0 },
	{0xD0EC, 0xB88F, WORD_LEN, 0 },
	{0xD0EE, 0xB106, WORD_LEN, 0 },
	{0xD0F0, 0x210A, WORD_LEN, 0 },
	{0xD0F2, 0x8340, WORD_LEN, 0 },
	{0xD0F4, 0xC000, WORD_LEN, 0 },
	{0xD0F6, 0x21CA, WORD_LEN, 0 },
	{0xD0F8, 0x0062, WORD_LEN, 0 },
	{0xD0FA, 0x20F0, WORD_LEN, 0 },
	{0xD0FC, 0x0040, WORD_LEN, 0 },
	{0xD0FE, 0x0B02, WORD_LEN, 0 },
	{0xD100, 0x0320, WORD_LEN, 0 },
	{0xD102, 0xD901, WORD_LEN, 0 },
	{0xD104, 0x07F1, WORD_LEN, 0 },
	{0xD106, 0x05E0, WORD_LEN, 0 },
	{0xD108, 0xC0A1, WORD_LEN, 0 },
	{0xD10A, 0x78E0, WORD_LEN, 0 },
	{0xD10C, 0xC0F1, WORD_LEN, 0 },
	{0xD10E, 0x71CF, WORD_LEN, 0 },
	{0xD110, 0xFFFF, WORD_LEN, 0 },
	{0xD112, 0xC7C0, WORD_LEN, 0 },
	{0xD114, 0xD840, WORD_LEN, 0 },
	{0xD116, 0xA900, WORD_LEN, 0 },
	{0xD118, 0x71CF, WORD_LEN, 0 },
	{0xD11A, 0xFFFF, WORD_LEN, 0 },
	{0xD11C, 0xD02C, WORD_LEN, 0 },
	{0xD11E, 0xD81E, WORD_LEN, 0 },
	{0xD120, 0x0A5A, WORD_LEN, 0 },
	{0xD122, 0x04E0, WORD_LEN, 0 },
	{0xD124, 0xDA00, WORD_LEN, 0 },
	{0xD126, 0xD800, WORD_LEN, 0 },
	{0xD128, 0xC0D1, WORD_LEN, 0 },
	{0xD12A, 0x7EE0, WORD_LEN, 0 },
	{0x098E, 0x0000, WORD_LEN, 0 },  // LOGICAL_ADDRESS_ACCESS
	
	{0xE000, 0x010C, WORD_LEN, 0 },  // PATCHLDR_LOADER_ADDRESS
	{0xE002, 0x0202, WORD_LEN, 0 },  // PATCHLDR_PATCH_ID
	{0xE004, 0x41030202, DOUBLE_WORD_LEN, 0 },  // PATCHLDR_FIRMWARE_ID

	//SW4-L1-HL-Camera-AWB-FixCloudy-00+{_20120213
	//[Step4-APGA_85]
	//Disable PGA and APGA for LSC
	//{0x098E, 	0x495E, WORD_LEN, 0 },  // LOGICAL_ADDRESS_ACCESS [CAM_PGA_PGA_CONTROL]
	//{0xC95E, 	0x0000, WORD_LEN, 0 },  // CAM_PGA_PGA_CONTROL
	
	//TIMM_OSAL_SleepTask(3);
	
	//LSC 85%_20120209
	{0x3784, 	0x026C, WORD_LEN, 0 },  // CENTER_COLUMN
	{0x3782, 	0x01B8, WORD_LEN, 0 },  // CENTER_ROW
	{0x37C0, 	0x0000, WORD_LEN, 0 },  // P_GR_Q5
	{0x37C2, 	0x0000, WORD_LEN, 0 },  // P_RD_Q5
	{0x37C4, 	0x0000, WORD_LEN, 0 },  // P_BL_Q5
	{0x37C6, 	0x0000, WORD_LEN, 0 },  // P_GB_Q5
	{0x3642, 	0xA7CC, WORD_LEN, 0 },  // P_G1_P0Q1
	{0x3644, 	0x6050, WORD_LEN, 0 },  // P_G1_P0Q2
	{0x3646, 	0xDE2D, WORD_LEN, 0 },  // P_G1_P0Q3
	{0x3648, 	0x08D1, WORD_LEN, 0 },  // P_G1_P0Q4
	{0x364A, 	0x00F0, WORD_LEN, 0 },  // P_R_P0Q0
	{0x364C, 	0xC06C, WORD_LEN, 0 },  // P_R_P0Q1
	{0x364E, 	0x1231, WORD_LEN, 0 },  // P_R_P0Q2
	{0x3650, 	0xA58D, WORD_LEN, 0 },  // P_R_P0Q3
	{0x3652, 	0x27F1, WORD_LEN, 0 },  // P_R_P0Q4
	{0x3654, 	0x0130, WORD_LEN, 0 },  // P_B_P0Q0
	{0x3656, 	0xA2AB, WORD_LEN, 0 },  // P_B_P0Q1
	{0x3658, 	0x35F0, WORD_LEN, 0 },  // P_B_P0Q2
	{0x365A, 	0xCF0D, WORD_LEN, 0 },  // P_B_P0Q3
	{0x365C, 	0x00F1, WORD_LEN, 0 },  // P_B_P0Q4
	{0x3660, 	0xB16C, WORD_LEN, 0 },  // P_G2_P0Q1
	{0x3662, 	0x6610, WORD_LEN, 0 },  // P_G2_P0Q2
	{0x3664, 	0xF64C, WORD_LEN, 0 },  // P_G2_P0Q3
	{0x3666, 	0x6490, WORD_LEN, 0 },  // P_G2_P0Q4
	{0x3680, 	0x930C, WORD_LEN, 0 },  // P_G1_P1Q0
	{0x3682, 	0x568B, WORD_LEN, 0 },  // P_G1_P1Q1
	{0x3684, 	0x0B4E, WORD_LEN, 0 },  // P_G1_P1Q2
	{0x3686, 	0xA02E, WORD_LEN, 0 },  // P_G1_P1Q3
	{0x3688, 	0x08AF, WORD_LEN, 0 },  // P_G1_P1Q4
	{0x368A, 	0xDA8C, WORD_LEN, 0 },  // P_R_P1Q0
	{0x368C, 	0x330D, WORD_LEN, 0 },  // P_R_P1Q1
	{0x368E, 	0xF46B, WORD_LEN, 0 },  // P_R_P1Q2
	{0x3690, 	0xF409, WORD_LEN, 0 },  // P_R_P1Q3
	{0x3692, 	0x516F, WORD_LEN, 0 },  // P_R_P1Q4
	{0x3694, 	0x3606, WORD_LEN, 0 },  // P_B_P1Q0
	{0x3696, 	0x418C, WORD_LEN, 0 },  // P_B_P1Q1
	{0x3698, 	0xC06E, WORD_LEN, 0 },  // P_B_P1Q2
	{0x369A, 	0xBB6E, WORD_LEN, 0 },  // P_B_P1Q3
	{0x369C, 	0xB80E, WORD_LEN, 0 },  // P_B_P1Q4
	{0x369E, 	0xED6A, WORD_LEN, 0 },  // P_G2_P1Q0
	{0x36A0, 	0x6129, WORD_LEN, 0 },  // P_G2_P1Q1
	{0x36A2, 	0xA7CE, WORD_LEN, 0 },  // P_G2_P1Q2
	{0x36A4, 	0xE16E, WORD_LEN, 0 },  // P_G2_P1Q3
	{0x36A6, 	0x9D6D, WORD_LEN, 0 },  // P_G2_P1Q4
	{0x36C0, 	0x70F0, WORD_LEN, 0 },  // P_G1_P2Q0
	{0x36C2, 	0x9E2F, WORD_LEN, 0 },  // P_G1_P2Q1
	{0x36C4, 	0x0993, WORD_LEN, 0 },  // P_G1_P2Q2
	{0x36C6, 	0x0851, WORD_LEN, 0 },  // P_G1_P2Q3
	{0x36C8, 	0x9773, WORD_LEN, 0 },  // P_G1_P2Q4
	{0x36CA, 	0x2BB1, WORD_LEN, 0 },  // P_R_P2Q0
	{0x36CC, 	0x92EF, WORD_LEN, 0 },  // P_R_P2Q1
	{0x36CE, 	0x3ED3, WORD_LEN, 0 },  // P_R_P2Q2
	{0x36D0, 	0x2DB1, WORD_LEN, 0 },  // P_R_P2Q3
	{0x36D2, 	0xFE73, WORD_LEN, 0 },  // P_R_P2Q4
	{0x36D4, 	0x60B0, WORD_LEN, 0 },  // P_B_P2Q0
	{0x36D6, 	0xDEEE, WORD_LEN, 0 },  // P_B_P2Q1
	{0x36D8, 	0x7A72, WORD_LEN, 0 },  // P_B_P2Q2
	{0x36DA, 	0x1091, WORD_LEN, 0 },  // P_B_P2Q3
	{0x36DC, 	0x96D3, WORD_LEN, 0 },  // P_B_P2Q4
	{0x36DE, 	0x73F0, WORD_LEN, 0 },  // P_G2_P2Q0
	{0x36E0, 	0x91AF, WORD_LEN, 0 },  // P_G2_P2Q1
	{0x36E2, 	0x7C52, WORD_LEN, 0 },  // P_G2_P2Q2
	{0x36E4, 	0x6A70, WORD_LEN, 0 },  // P_G2_P2Q3
	{0x36E6, 	0x8CF3, WORD_LEN, 0 },  // P_G2_P2Q4
	{0x3700, 	0x192D, WORD_LEN, 0 },  // P_G1_P3Q0
	{0x3702, 	0x7ACB, WORD_LEN, 0 },  // P_G1_P3Q1
	{0x3704, 	0x8FD1, WORD_LEN, 0 },  // P_G1_P3Q2
	{0x3706, 	0x8CEF, WORD_LEN, 0 },  // P_G1_P3Q3
	{0x3708, 	0x4793, WORD_LEN, 0 },  // P_G1_P3Q4
	{0x370A, 	0x822D, WORD_LEN, 0 },  // P_R_P3Q0
	{0x370C, 	0x9F8E, WORD_LEN, 0 },  // P_R_P3Q1
	{0x370E, 	0xB871, WORD_LEN, 0 },  // P_R_P3Q2
	{0x3710, 	0xDA51, WORD_LEN, 0 },  // P_R_P3Q3
	{0x3712, 	0x1EB4, WORD_LEN, 0 },  // P_R_P3Q4
	{0x3714, 	0x6EC6, WORD_LEN, 0 },  // P_B_P3Q0
	{0x3716, 	0x154E, WORD_LEN, 0 },  // P_B_P3Q1
	{0x3718, 	0x8952, WORD_LEN, 0 },  // P_B_P3Q2
	{0x371A, 	0x8CF1, WORD_LEN, 0 },  // P_B_P3Q3
	{0x371C, 	0x37F4, WORD_LEN, 0 },  // P_B_P3Q4
	{0x371E, 	0x27AE, WORD_LEN, 0 },  // P_G2_P3Q0
	{0x3720, 	0x6AAE, WORD_LEN, 0 },  // P_G2_P3Q1
	{0x3722, 	0x8D12, WORD_LEN, 0 },  // P_G2_P3Q2
	{0x3724, 	0x8670, WORD_LEN, 0 },  // P_G2_P3Q3
	{0x3726, 	0x2AF4, WORD_LEN, 0 },  // P_G2_P3Q4
	{0x3740, 	0x6010, WORD_LEN, 0 },  // P_G1_P4Q0
	{0x3742, 	0x6E2F, WORD_LEN, 0 },  // P_G1_P4Q1
	{0x3744, 	0x02B3, WORD_LEN, 0 },  // P_G1_P4Q2
	{0x3746, 	0x6D92, WORD_LEN, 0 },  // P_G1_P4Q3
	{0x3748, 	0xD436, WORD_LEN, 0 },  // P_G1_P4Q4
	{0x374A, 	0x42B0, WORD_LEN, 0 },  // P_R_P4Q0
	{0x374C, 	0xBE4B, WORD_LEN, 0 },  // P_R_P4Q1
	{0x374E, 	0x62B2, WORD_LEN, 0 },  // P_R_P4Q2
	{0x3750, 	0x3A73, WORD_LEN, 0 },  // P_R_P4Q3
	{0x3752, 	0x8137, WORD_LEN, 0 },  // P_R_P4Q4
	{0x3754, 	0x14EF, WORD_LEN, 0 },  // P_B_P4Q0
	{0x3756, 	0xBC4F, WORD_LEN, 0 },  // P_B_P4Q1
	{0x3758, 	0x3793, WORD_LEN, 0 },  // P_B_P4Q2
	{0x375A, 	0x0533, WORD_LEN, 0 },  // P_B_P4Q3
	{0x375C, 	0xCBF6, WORD_LEN, 0 },  // P_B_P4Q4
	{0x375E, 	0x5A10, WORD_LEN, 0 },  // P_G2_P4Q0
	{0x3760, 	0x0450, WORD_LEN, 0 },  // P_G2_P4Q1
	{0x3762, 	0x25D3, WORD_LEN, 0 },  // P_G2_P4Q2
	{0x3764, 	0x5992, WORD_LEN, 0 },  // P_G2_P4Q3
	{0x3766, 	0xD5B6, WORD_LEN, 0 },  // P_G2_P4Q4
	{0x37C0, 	0x4385, WORD_LEN, 0 },  // P_GR_Q5
	{0x37C2, 	0x810A, WORD_LEN, 0 },  // P_RD_Q5
	{0x37C4, 	0xDD29, WORD_LEN, 0 },  // P_BL_Q5
	{0x37C6, 	0x8A65, WORD_LEN, 10 },  // P_GB_Q5

	//Enable PGA for LSC
	{0x098E, 	0x495E, WORD_LEN, 0 },  // LOGICAL_ADDRESS_ACCESS [CAM_PGA_PGA_CONTROL]
	{0xC95E, 	0x0001, WORD_LEN, 0 },  // CAM_PGA_PGA_CONTROL
	//SW4-L1-HL-Camera-AWB-FixCloudy-00+}_20120213

	// CCM
	{0xC892, 0x0267, WORD_LEN, 0 },  // CAM_AWB_CCM_L_0
	{0xC894, 0xFF1A, WORD_LEN, 0 },  // CAM_AWB_CCM_L_1
	{0xC896, 0xFFB3, WORD_LEN, 0 },  // CAM_AWB_CCM_L_2
	{0xC898, 0xFF80, WORD_LEN, 0 },  // CAM_AWB_CCM_L_3
	{0xC89A, 0x0166, WORD_LEN, 0 },  // CAM_AWB_CCM_L_4
	{0xC89C, 0x0003, WORD_LEN, 0 },  // CAM_AWB_CCM_L_5
	{0xC89E, 0xFF9A, WORD_LEN, 0 },  // CAM_AWB_CCM_L_6
	{0xC8A0, 0xFEB4, WORD_LEN, 0 },  // CAM_AWB_CCM_L_7
	{0xC8A2, 0x024D, WORD_LEN, 0 },  // CAM_AWB_CCM_L_8
	{0xC8A4, 0x01BF, WORD_LEN, 0 },  // CAM_AWB_CCM_M_0
	{0xC8A6, 0xFF01, WORD_LEN, 0 },  // CAM_AWB_CCM_M_1
	{0xC8A8, 0xFFF3, WORD_LEN, 0 },  // CAM_AWB_CCM_M_2
	{0xC8AA, 0xFF75, WORD_LEN, 0 },  // CAM_AWB_CCM_M_3
	{0xC8AC, 0x0198, WORD_LEN, 0 },  // CAM_AWB_CCM_M_4
	{0xC8AE, 0xFFFD, WORD_LEN, 0 },  // CAM_AWB_CCM_M_5
	{0xC8B0, 0xFF9A, WORD_LEN, 0 },  // CAM_AWB_CCM_M_6
	{0xC8B2, 0xFEE7, WORD_LEN, 0 },  // CAM_AWB_CCM_M_7
	{0xC8B4, 0x02A8, WORD_LEN, 0 },  // CAM_AWB_CCM_M_8
	{0xC8B6, 0x01D9, WORD_LEN, 0 },  // CAM_AWB_CCM_R_0
	{0xC8B8, 0xFF26, WORD_LEN, 0 },  // CAM_AWB_CCM_R_1
	{0xC8BA, 0xFFF3, WORD_LEN, 0 },  // CAM_AWB_CCM_R_2
	{0xC8BC, 0xFFB3, WORD_LEN, 0 },  // CAM_AWB_CCM_R_3
	{0xC8BE, 0x0132, WORD_LEN, 0 },  // CAM_AWB_CCM_R_4
	{0xC8C0, 0xFFE8, WORD_LEN, 0 },  // CAM_AWB_CCM_R_5
	{0xC8C2, 0xFFDA, WORD_LEN, 0 },  // CAM_AWB_CCM_R_6
	{0xC8C4, 0xFECD, WORD_LEN, 0 },  // CAM_AWB_CCM_R_7
	{0xC8C6, 0x02C2, WORD_LEN, 0 },  // CAM_AWB_CCM_R_8
	{0xC8C8, 0x0075, WORD_LEN, 0 },  // CAM_AWB_CCM_L_RG_GAIN
	{0xC8CA, 0x011C, WORD_LEN, 0 },  // CAM_AWB_CCM_L_BG_GAIN
	{0xC8CC, 0x009A, WORD_LEN, 0 },  // CAM_AWB_CCM_M_RG_GAIN
	{0xC8CE, 0x0105, WORD_LEN, 0 },  // CAM_AWB_CCM_M_BG_GAIN
	{0xC8D0, 0x00A4, WORD_LEN, 0 },  // CAM_AWB_CCM_R_RG_GAIN
	{0xC8D2, 0x00AC, WORD_LEN, 0 },  // CAM_AWB_CCM_R_BG_GAIN
	{0xC8D4, 0x0A8C, WORD_LEN, 0 },  // CAM_AWB_CCM_L_CTEMP
	{0xC8D6, 0x0F0A, WORD_LEN, 0 },  // CAM_AWB_CCM_M_CTEMP
	{0xC8D8, 0x1964, WORD_LEN, 0 },  // CAM_AWB_CCM_R_CTEMP

	//SW4-L1-HL-Camera-AEAWBFineTune-00+{_20120221
    //[AE target]
	{0x098E, 0xC87A, WORD_LEN, 0 },  // LOGICAL_ADDRESS_ACCESS [CAM_AET_TARGET_AVERAGE_LUMA]
	{0xC87A,   0x43, BYTE_LEN, 0 },  // CAM_AET_TARGET_AVERAGE_LUMA

	//[AWB]
	{0xC914, 0x0000, WORD_LEN, 0 },  // CAM_STAT_AWB_CLIP_WINDOW_XSTART
	{0xC916, 0x0000, WORD_LEN, 0 },  // CAM_STAT_AWB_CLIP_WI NDOW_YSTART
	{0xC918, 0x04FF, WORD_LEN, 0 },  // CAM_STAT_AWB_CLIP_WINDOW_XEND
	{0xC91A, 0x02CF, WORD_LEN, 0 },  // CAM_STAT_AWB_CLIP_WINDOW_YEND
	{0xC904, 0x0033, WORD_LEN, 0 },  // CAM_AWB_AWB_XSHIFT_PRE_ADJ
	{0xC906, 0x0040, WORD_LEN, 0 },  // CAM_AWB_AWB_YSHIFT_PRE_ADJ
	{0xC8F2,   0x03, BYTE_LEN, 0 },  // CAM_AWB_AWB_XSCALE
	{0xC8F3,   0x02, BYTE_LEN, 0 },  // CAM_AWB_AWB_YSCALE
	{0xC906, 0x003C, WORD_LEN, 0 },  // CAM_AWB_AWB_YSHIFT_PRE_ADJ
	{0xC8F4, 0x0000, WORD_LEN, 0 },  // CAM_AWB_AWB_WEIGHTS_0
	{0xC8F6, 0x0000, WORD_LEN, 0 },  // CAM_AWB_AWB_WEIGHTS_1
	{0xC8F8, 0x0000, WORD_LEN, 0 },  // CAM_AWB_AWB_WEIGHTS_2
	{0xC8FA, 0xE724, WORD_LEN, 0 },  // CAM_AWB_AWB_WEIGHTS_3
	{0xC8FC, 0x1583, WORD_LEN, 0 },  // CAM_AWB_AWB_WEIGHTS_4
	{0xC8FE, 0x2045, WORD_LEN, 0 },  // CAM_AWB_AWB_WEIGHTS_5
	{0xC900, 0x05DC, WORD_LEN, 0 },  // CAM_AWB_AWB_WEIGHTS_6
	{0xC902, 0x007C, WORD_LEN, 0 },  // CAM_AWB_AWB_WEIGHTS_7
	{0xC90C,   0x80, BYTE_LEN, 0 },  // CAM_AWB_K_R_L
	{0xC90D,   0x80, BYTE_LEN, 0 },  // CAM_AWB_K_G_L
	{0xC90E,   0x80, BYTE_LEN, 0 },  // CAM_AWB_K_B_L
	{0xC90F,   0x88, BYTE_LEN, 0 },  // CAM_AWB_K_R_R
	{0xC910,   0x80, BYTE_LEN, 0 },  // CAM_AWB_K_G_R
	{0xC911,   0x80, BYTE_LEN, 0 },  // CAM_AWB_K_B_R
	//SW4-L1-HL-Camera-AEAWBFineTune-00+}_20120221
	
	//[Step7-CPIPE_Preference]
	{0xC926, 0x0020, WORD_LEN, 0 },  // CAM_LL_START_BRIGHTNESS
	{0xC928, 0x009A, WORD_LEN, 0 },  // CAM_LL_STOP_BRIGHTNESS
	{0xC946, 0x0070, WORD_LEN, 0 },  // CAM_LL_START_GAIN_METRIC
	{0xC948, 0x00F3, WORD_LEN, 0 },  // CAM_LL_STOP_GAIN_METRIC
	{0xC952, 0x0020, WORD_LEN, 0 },  // CAM_LL_START_TARGET_LUMA_BM
	{0xC954, 0x009A, WORD_LEN, 0 },  // CAM_LL_STOP_TARGET_LUMA_BM
	{0xC92A, 0x80, BYTE_LEN, 0 },  // CAM_LL_START_SATURATION
	{0xC92B, 0x4B, BYTE_LEN, 0 },  // CAM_LL_END_SATURATION
	{0xC92C, 0x00, BYTE_LEN, 0 },  // CAM_LL_START_DESATURATION
	{0xC92D, 0xFF, BYTE_LEN, 0 },  // CAM_LL_END_DESATURATION
	{0xC92E, 0x3C, BYTE_LEN, 0 },  // CAM_LL_START_DEMOSAIC
	{0xC92F, 0x02, BYTE_LEN, 0 },  // CAM_LL_START_AP_GAIN
	{0xC930, 0x06, BYTE_LEN, 0 },  // CAM_LL_START_AP_THRESH
	{0xC931, 0x64, BYTE_LEN, 0 },  // CAM_LL_STOP_DEMOSAIC
	{0xC932, 0x01, BYTE_LEN, 0 },  // CAM_LL_STOP_AP_GAIN
	{0xC933, 0x0C, BYTE_LEN, 0 },  // CAM_LL_STOP_AP_THRESH
	{0xC934, 0x3C, BYTE_LEN, 0 },  // CAM_LL_START_NR_RED
	{0xC935, 0x3C, BYTE_LEN, 0 },  // CAM_LL_START_NR_GREEN
	{0xC936, 0x3C, BYTE_LEN, 0 },  // CAM_LL_START_NR_BLUE
	{0xC937, 0x0F, BYTE_LEN, 0 },  // CAM_LL_START_NR_THRESH
	{0xC938, 0x64, BYTE_LEN, 0 },  // CAM_LL_STOP_NR_RED
	{0xC939, 0x64, BYTE_LEN, 0 },  // CAM_LL_STOP_NR_GREEN
	{0xC93A, 0x64, BYTE_LEN, 0 },  // CAM_LL_STOP_NR_BLUE
	{0xC93B, 0x32, BYTE_LEN, 0 },  // CAM_LL_STOP_NR_THRESH
	{0xC93C, 0x0020, WORD_LEN, 0 },  // CAM_LL_START_CONTRAST_BM
	{0xC93E, 0x009A, WORD_LEN, 0 },  // CAM_LL_STOP_CONTRAST_BM
	{0xC940, 0x00DC, WORD_LEN, 0 },  // CAM_LL_GAMMA
	{0xC942, 0x38, BYTE_LEN, 0 },  // CAM_LL_START_CONTRAST_GRADIENT
	{0xC943, 0x30, BYTE_LEN, 0 },  // CAM_LL_STOP_CONTRAST_GRADIENT
	{0xC944, 0x50, BYTE_LEN, 0 },  // CAM_LL_START_CONTRAST_LUMA_PERCENTAGE
	{0xC945, 0x19, BYTE_LEN, 0 },  // CAM_LL_STOP_CONTRAST_LUMA_PERCENTAGE
	{0xC94A, 0x0230, WORD_LEN, 0 },  // CAM_LL_START_FADE_TO_BLACK_LUMA
	{0xC94C, 0x0010, WORD_LEN, 0 },  // CAM_LL_STOP_FADE_TO_BLACK_LUMA
	{0xC94E, 0x01CD, WORD_LEN, 0 },  // CAM_LL_CLUSTER_DC_TH_BM
	{0xC950, 0x05, BYTE_LEN, 0 },  // CAM_LL_CLUSTER_DC_GATE_PERCENTAGE
	{0xC951, 0x40, BYTE_LEN, 0 },  // CAM_LL_SUMMING_SENSITIVITY_FACTOR
	{0xC87B, 0x1B, BYTE_LEN, 0 },  // CAM_AET_TARGET_AVERAGE_LUMA_DARK
	{0xC878, 0x0E, BYTE_LEN, 0 },  // CAM_AET_AEMODE
	{0xC890, 0x0080, WORD_LEN, 0 },  // CAM_AET_TARGET_GAIN
	{0xC886, 0x0100, WORD_LEN, 0 },  // CAM_AET_AE_MAX_VIRT_AGAIN
	{0xC87C, 0x005A, WORD_LEN, 0 },  // CAM_AET_BLACK_CLIPPING_TARGET
	{0xB42A, 0x05, BYTE_LEN, 0 },  // CCM_DELTA_GAIN
	{0xA80A, 0x20, BYTE_LEN, 0 },  // AE_TRACK_AE_TRACKING_DAMPENING_SPEED
	//SW4-L1-HL-Camera-FTM-FixAEConvergenceTooSlow-00+}_20120222

	//SW4-L1-HL-Camera-FixAEConvergenceTooSlow+{_20120223
	#if 0
	//[Change-Config]
	{0x098E, 0xDC00, WORD_LEN, 0 }, // LOGICAL_ADDRESS_ACCESS [SYSMGR_NEXT_STATE]
	{0xDC00, 0x28, BYTE_LEN, 0 },	// SYSMGR_NEXT_STATE
	{0x0080, 0x8002, WORD_LEN, 0 }, // COMMAND_REGISTER
	#endif
	//SW4-L1-HL-Camera-FixAEConvergenceTooSlow+}_20120223	
	#endif
	//HL*}CSI_20111125

#endif
//SW4-L1-HL-Camera-FTM-FixCaptureImageQualityTooLow-00*}_20120229
};

//HL-{CSI_20111124
#if 0
static const struct mt9m114_timing_cfg timing_cfg[MT9M114_SIZE_LAST] = {
	[MT9M114_SIZE_QVGA] = {
		.x_addr_start = 0,
		.y_addr_start = 4,
		.x_addr_end = 1296,	//2623,	//SW4-L1-HL-Camera-FTM_BringUP-00*
		.y_addr_end = 976,	//1947,	//SW4-L1-HL-Camera-FTM_BringUP-00*
		.h_output_size = 320,
		.v_output_size = 240,
		.h_total_size = 1896,
		.v_total_size = 984,
		.isp_h_offset = 16,
		.isp_v_offset = 6,
		.h_odd_ss_inc = 3,
		.h_even_ss_inc = 1,
		.v_odd_ss_inc = 3,
		.v_even_ss_inc = 1,
	},
	[MT9M114_SIZE_VGA] = {
		.x_addr_start = 0,
		.y_addr_start = 4,
		.x_addr_end = 1296,	//2623,	//SW4-L1-HL-Camera-FTM_BringUP-00*
		.y_addr_end = 976,	//1947,	//SW4-L1-HL-Camera-FTM_BringUP-00*
		.h_output_size = 640,
		.v_output_size = 480,
		.h_total_size = 1896,
		.v_total_size = 984,
		.isp_h_offset = 16,
		.isp_v_offset = 6,
		.h_odd_ss_inc = 3,
		.h_even_ss_inc = 1,
		.v_odd_ss_inc = 3,
		.v_even_ss_inc = 1,
	},
	[MT9M114_SIZE_720P] = {
		.x_addr_start = 0,
		.y_addr_start = 4,
		.x_addr_end = 1296,	//2623,	//SW4-L1-HL-Camera-FTM_BringUP-00*
		.y_addr_end = 976,	//1947,	//SW4-L1-HL-Camera-FTM_BringUP-00*
		.h_output_size = 1280,
		.v_output_size = 720,
		.h_total_size = 1896,
		.v_total_size = 984,
		.isp_h_offset = 16,
		.isp_v_offset = 6,
		.h_odd_ss_inc = 3,
		.h_even_ss_inc = 1,
		.v_odd_ss_inc = 3,
		.v_even_ss_inc = 1,
	},
};
#endif
//HL-}CSI_20111124

static struct v4l2_subdev_sensor_serial_parms mipi_cfgs[MT9M114_SIZE_LAST] = {
	[MT9M114_SIZE_QVGA] = {
		.lanes = 1,
		.channel = 0,
		.phy_rate = (768 * 2 * 1000000),	//336	//HL*CSI_20111123
		.pix_clk = 48, /* Revisit */			//21		//HL*CSI_20111128
	},
	[MT9M114_SIZE_VGA] = {
		.lanes = 1,
		.channel = 0,
		.phy_rate = (768 * 2 * 1000000),	//336	//HL*CSI_20111123
		.pix_clk = 48, /* Revisit */			//21		//HL*CSI_20111128
	},
	[MT9M114_SIZE_720P] = {
		.lanes = 1,
		.channel = 0,
		.phy_rate = (768 * 2 * 1000000),	//336	//HL*CSI_20111123
		.pix_clk = 48, /* Revisit */			//21		//HL*CSI_20111128
	},
	//SW4-L1-HL-Camera-FTM-FixCaptureImageQualityTooLow-00+{_20120229
	#if 0
	[MT9M114_SIZE_130MP] = {
		.lanes = 1,
		.channel = 0,
		.phy_rate = (768 * 2 * 1000000),	//336	//HL*CSI_20111123
		.pix_clk = 48, /* Revisit */			//21		//HL*CSI_20111128
	},
	#endif
	//SW4-L1-HL-Camera-FTM-FixCaptureImageQualityTooLow-00+}_20120229
};

/**
 * mt9m114_reg_read - Read a value from a register in an mt9m114 sensor device
 * @client: i2c driver client structure
 * @reg: register address / offset
 * @val: stores the value that gets read
 *
 * Read a value from a register in an mt9m114 sensor device.
 * The value is returned in 'val'.
 * Returns zero if successful, or non-zero otherwise.
 */
static int mt9m114_reg_read(struct i2c_client *client, u16 reg, u8 *val)
{
	int ret;
	u8 data[2] = {0};
	struct i2c_msg msg = {
		.addr	= client->addr,
		.flags	= 0,
		.len	= 2,
		.buf	= data,
	};

	data[0] = (u8)(reg >> 8);
	data[1] = (u8)(reg & 0xff);

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0)
		goto err;

	msg.flags = I2C_M_RD;
	msg.len = 1;
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0)
		goto err;

	*val = data[0];
	return 0;

err:
	dev_err(&client->dev, "Failed reading register 0x%02x!\n", reg);
	return ret;
}

//HL+{
static int32_t mt9m114_i2c_txdata(struct i2c_client *client, unsigned short saddr,
	unsigned char *txdata, int length)
{
    struct i2c_msg msg[] = {
        {
            .addr = saddr,
            .flags = 0,
            .len = length,
            .buf = txdata,
        },
    };

    if (i2c_transfer(client->adapter, msg, 1) < 0) 
	{
        printk(KERN_ERR "%s: failed!\n", __func__);
        return -EIO;
    }

    return 0;
}

static int mt9m114_i2c_rxdata(struct i2c_client *client, unsigned short saddr,
	unsigned char *rxdata, int length)
{
    struct i2c_msg msgs[] = {
        {
            .addr   = saddr,
            .flags = 0,
            .len   = 2,
            .buf   = rxdata,
        },
        {
            .addr   = saddr,
            .flags = I2C_M_RD,
            .len   = length,
            .buf   = rxdata,
        },
    };

    if (i2c_transfer(client->adapter, msgs, 2) < 0) 
	{
        printk(KERN_ERR "%s: failed!\n", __func__);
        return -EIO;
    }

    return 0;
}

static int32_t mt9m114_i2c_read(struct i2c_client *client, unsigned short   saddr,
	unsigned short raddr, unsigned short *rdata, enum mt9m114_width width)
{
    int32_t rc = 0;
    unsigned char buf[4];

    if (!rdata)
        return -EIO;

    memset(buf, 0, sizeof(buf));

    switch (width) {
        case WORD_LEN: {
            buf[0] = (raddr & 0xFF00)>>8;
            buf[1] = (raddr & 0x00FF);

            rc = mt9m114_i2c_rxdata(client, saddr, buf, 2);
            if (rc < 0)
                return rc;

            *rdata = buf[0] << 8 | buf[1];
        }
            break;

        case BYTE_LEN: {
            buf[0] = (raddr & 0xFF00)>>8;
            buf[1] = (raddr & 0x00FF);

            rc = mt9m114_i2c_rxdata(client, saddr, buf, 2);

            if (rc < 0)
                return rc;

            *rdata = buf[0];
        }
            break;

        default:
            break;
    }

    if (rc < 0)
        printk("%s: failed!\n", __func__);

    return rc;
}

static int mt9m114_i2c_write(struct i2c_client *client, u16 saddr, u16 reg, u32 val, enum mt9m114_width width)
{
    int32_t rc = -EIO;
    int count = 0;
    unsigned char buf[6];
    unsigned short bytePoll;

    memset(buf, 0, sizeof(buf));
    switch (width) 
	{
        case DOUBLE_WORD_LEN: 
		{
            buf[0] = (reg & 0xFF00)>>8;
            buf[1] = (reg & 0x00FF);
            buf[2] = (val & 0xFF000000)>>24;
            buf[3] = (val & 0x00FF0000)>>16;
            buf[4] = (val & 0x0000FF00)>>8;
            buf[5] = (val & 0x000000FF);			

            rc = mt9m114_i2c_txdata(client, saddr, buf, 6);
        }
        break;
		
        case WORD_LEN: 
		{
            buf[0] = (reg & 0xFF00)>>8;
            buf[1] = (reg & 0x00FF);
            buf[2] = (val & 0xFF00)>>8;
            buf[3] = (val & 0x00FF);

            rc = mt9m114_i2c_txdata(client, saddr, buf, 4);
        }
        break;

        case BYTE_LEN: 
		{
            buf[0] = (reg & 0xFF00)>>8;
            buf[1] = (reg & 0x00FF);
            buf[2] = (val & 0xFF);

            rc = mt9m114_i2c_txdata(client, saddr, buf, 3);
        }
        break;

        case WORD_POLL: 
		{
            do 
			{
                count++;
                msleep(40);
                rc = mt9m114_i2c_read(client, saddr, reg, &bytePoll, WORD_LEN);
                printk("[Debug Info] Read Back!!, raddr = 0x%x, rdata = 0x%x.\n", reg, bytePoll);
            } while( (bytePoll != val) && (count <20) );
        }
        break;

        case BYTE_POLL: 
		{
            do 
			{
                count++;
                msleep(40);
                rc = mt9m114_i2c_read(client, saddr, reg, &bytePoll, BYTE_LEN);
                printk("[Debug Info] Read Back!!, raddr = 0x%x, rdata = 0x%x.\n", reg, bytePoll);
            } while( (bytePoll != val) && (count <20) );
        }
        break;

        default:
            break;
    }

    if (rc < 0)
        printk("%s: failed, addr = 0x%x, val = 0x%x!\n", __func__, reg, val);

    return rc;	
}

//HL+}

/**
 * Write a value to a register in mt9m114 sensor device.
 * @client: i2c driver client structure.
 * @reg: Address of the register to read value from.
 * @val: Value to be written to a specific register.
 * Returns zero if successful, or non-zero otherwise.
 */
static int mt9m114_reg_write(struct i2c_client *client, u16 reg, u8 val)
{
	int ret;
	unsigned char data[3] = { (u8)(reg >> 8), (u8)(reg & 0xff), val };
	struct i2c_msg msg = {
		.addr	= client->addr,
		.flags	= 0,
		.len	= 3,
		.buf	= data,
	};

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0) {
		dev_err(&client->dev, "Failed writing register 0x%02x!\n", reg);
		return ret;
	}

	return 0;
}

static const struct v4l2_queryctrl mt9m114_controls[] = {
	{
		.id      	= V4L2_CID_BRIGHTNESS,
		.type    	= V4L2_CTRL_TYPE_INTEGER,
		.name    	= "Brightness",
		.minimum	= MT9M114_BRIGHTNESS_MIN,
		.maximum	= MT9M114_BRIGHTNESS_MAX,
		.step		= MT9M114_BRIGHTNESS_STEP,
		.default_value	= MT9M114_BRIGHTNESS_DEF,
	},
	{
		.id		= V4L2_CID_CONTRAST,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "Contrast",
		.minimum	= MT9M114_CONTRAST_MIN,
		.maximum	= MT9M114_CONTRAST_MAX,
		.step		= MT9M114_CONTRAST_STEP,
		.default_value	= MT9M114_CONTRAST_DEF,
	},
	{
		.id		= V4L2_CID_COLORFX,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "Color Effects",
		.minimum	= V4L2_COLORFX_NONE,
		.maximum	= V4L2_COLORFX_NEGATIVE,
		.step		= 1,
		.default_value	= V4L2_COLORFX_NONE,
	},
};

/**
 * Initialize a list of mt9m114 registers.
 * The list of registers is terminated by the pair of values
 * @client: i2c driver client structure.
 * @reglist[]: List of address of the registers to write data.
 * Returns zero if successful, or non-zero otherwise.
 */
static int mt9m114_i2c_write_table(struct i2c_client *client,
			     const struct mt9m114_reg reglist[],
			     int num_of_items_in_table)	//HL*
{
	int err = 0, i;

	for (i = 0; i < num_of_items_in_table; i++) {
		err = mt9m114_i2c_write(client, client->addr, reglist[i].reg,
				reglist[i].val,
                reglist[i].width);	//HL*
		if (err)
			return err;

		//HL+{
        if (reglist[i].mdelay_time != 0)
            msleep(reglist[i].mdelay_time);
		//HL+}
	}
	return 0;
}

//HL-{
#if 0
static int mt9m114_reg_set(struct i2c_client *client, u16 reg, u8 val)
{
	int ret;
	u8 tmpval = 0;

	printk("[HL]%s: +++\n", __func__);

	ret = mt9m114_reg_read(client, reg, &tmpval);
	if (ret)
		return ret;

	printk("[HL]%s: ---\n", __func__);

	return mt9m114_reg_write(client, reg, tmpval | val);
}


static int mt9m114_reg_clr(struct i2c_client *client, u16 reg, u8 val)
{
	int ret;
	u8 tmpval = 0;

	printk("[HL]%s: +++\n", __func__);

	ret = mt9m114_reg_read(client, reg, &tmpval);
	if (ret)
		return ret;

	printk("[HL]%s: ---\n", __func__);

	return mt9m114_reg_write(client, reg, tmpval & ~val);
}

static int mt9m114_config_timing(struct i2c_client *client)
{
	struct mt9m114 *mt9m114 = to_mt9m114(client);
	int ret, i = mt9m114->i_size;

	printk("[HL]%s: +++\n", __func__);

	ret = mt9m114_reg_write(client,
			REG_MT9M114_TIMING_HS_X_ADDR_START_UPPER,
			(timing_cfg[i].x_addr_start & 0xFF00) >> 8);
	if (ret)
		return ret;

	ret = mt9m114_reg_write(client,
			REG_MT9M114_TIMING_HS_X_ADDR_START_LOWER,
			timing_cfg[i].x_addr_start & 0xFF);
	if (ret)
		return ret;

	ret = mt9m114_reg_write(client,
			REG_MT9M114_TIMING_VS_Y_ADDR_START_UPPER,
			(timing_cfg[i].y_addr_start & 0xFF00) >> 8);
	if (ret)
		return ret;

	ret = mt9m114_reg_write(client,
			REG_MT9M114_TIMING_VS_Y_ADDR_START_LOWER,
			timing_cfg[i].y_addr_start & 0xFF);
	if (ret)
		return ret;

	ret = mt9m114_reg_write(client,
			REG_MT9M114_TIMING_HW_X_ADDR_END_UPPER,
			(timing_cfg[i].x_addr_end & 0xFF00) >> 8);
	if (ret)
		return ret;

	ret = mt9m114_reg_write(client,
			REG_MT9M114_TIMING_HW_X_ADDR_END_LOWER,
			timing_cfg[i].x_addr_end & 0xFF);
	if (ret)
		return ret;

	ret = mt9m114_reg_write(client,
			REG_MT9M114_TIMING_VH_Y_ADDR_END_UPPER,
			(timing_cfg[i].y_addr_end & 0xFF00) >> 8);
	if (ret)
		return ret;

	ret = mt9m114_reg_write(client,
			REG_MT9M114_TIMING_VH_Y_ADDR_END_LOWER,
			timing_cfg[i].y_addr_end & 0xFF);
	if (ret)
		return ret;

	ret = mt9m114_reg_write(client,
			REG_MT9M114_TIMING_DVPHO_HORZ_WIDTH_UPPER,
			(timing_cfg[i].h_output_size & 0xFF00) >> 8);
	if (ret)
		return ret;

	ret = mt9m114_reg_write(client,
			REG_MT9M114_TIMING_DVPHO_HORZ_WIDTH_LOWER,
			timing_cfg[i].h_output_size & 0xFF);
	if (ret)
		return ret;

	ret = mt9m114_reg_write(client,
			REG_MT9M114_TIMING_DVPO_UPPER,
			(timing_cfg[i].v_output_size & 0xFF00) >> 8);
	if (ret)
		return ret;

	ret = mt9m114_reg_write(client,
			REG_MT9M114_TIMING_DVPO_LOWER,
			timing_cfg[i].v_output_size & 0xFF);
	if (ret)
		return ret;

	ret = mt9m114_reg_write(client,
			REG_MT9M114_TIMING_HTS_UPPER,
			(timing_cfg[i].h_total_size & 0xFF00) >> 8);
	if (ret)
		return ret;

	ret = mt9m114_reg_write(client,
			REG_MT9M114_TIMING_HTS_LOWER,
			timing_cfg[i].h_total_size & 0xFF);
	if (ret)
		return ret;

	ret = mt9m114_reg_write(client,
			REG_MT9M114_TIMING_VTS_UPPER,
			(timing_cfg[i].v_total_size & 0xFF00) >> 8);
	if (ret)
		return ret;

	ret = mt9m114_reg_write(client,
			REG_MT9M114_TIMING_VTS_LOWER,
			timing_cfg[i].v_total_size & 0xFF);
	if (ret)
		return ret;

	ret = mt9m114_reg_write(client,
			REG_MT9M114_TIMING_HOFFSET_UPPER,
			(timing_cfg[i].isp_h_offset & 0xFF00) >> 8);
	if (ret)
		return ret;

	ret = mt9m114_reg_write(client,
			REG_MT9M114_TIMING_HOFFSET_LOWER,
			timing_cfg[i].isp_h_offset & 0xFF);
	if (ret)
		return ret;

	ret = mt9m114_reg_write(client,
			REG_MT9M114_TIMING_VOFFSET_UPPER,
			(timing_cfg[i].isp_v_offset & 0xFF00) >> 8);
	if (ret)
		return ret;

	ret = mt9m114_reg_write(client,
			REG_MT9M114_TIMING_VOFFSET_LOWER,
			timing_cfg[i].isp_v_offset & 0xFF);
	if (ret)
		return ret;

	ret = mt9m114_reg_write(client,
			REG_MT9M114_TIMING_X_INC,
			((timing_cfg[i].h_odd_ss_inc & 0xF) << 4) |
			(timing_cfg[i].h_even_ss_inc & 0xF));
	if (ret)
		return ret;

	ret = mt9m114_reg_write(client,
			REG_MT9M114_TIMING_Y_INC,
			((timing_cfg[i].v_odd_ss_inc & 0xF) << 4) |
			(timing_cfg[i].v_even_ss_inc & 0xF));

	printk("[HL]%s: ---\n", __func__);

	return ret;
}
#endif
//HL-}

//HL+{
static int mt9m114_reg_init(struct i2c_client *client)
{
#if 1
	int rc;
	unsigned short tmpRegValue;
	int tmp_cnt = 0;
	
	printk("[HL]%s: Original +++\n", __func__);

    do{
        if( tmp_cnt > 0 ) {
            mdelay(10);
        }
		rc = mt9m114_i2c_read(client, client->addr, MT9M114_command_register, &tmpRegValue, WORD_LEN);
	    if (rc != 0)
	        goto out;		
        tmpRegValue = tmpRegValue & HOST_COMMAND_1;
        tmp_cnt++;
    }while(tmpRegValue != 0 && tmp_cnt < 10);

    //Errata item 2
	rc = mt9m114_i2c_read(client, client->addr, MT9M114_reset_register, &tmpRegValue, WORD_LEN);
    if (rc < 0)
        goto out;
	printk("[HL]%s: MT9M114_reset_register ==> tmpRegValue = 0x%x\n", __func__, tmpRegValue);	
    tmpRegValue &= 0x200;
	printk("[HL]%s: MT9M114_reset_register ==> tmpRegValue = 0x%x\n", __func__, tmpRegValue);
    rc = mt9m114_i2c_write(client, client->addr, MT9M114_reset_register, tmpRegValue, WORD_LEN);
    if (rc != 0)
		goto out;

	printk("[HL]%s: Errata item 2\n", __func__);

	//Write init table
	rc = mt9m114_i2c_write_table(client, init_tbl,
			ARRAY_SIZE(init_tbl));
	if (rc)
		goto out;

	printk("[HL]%s: mt9m114_i2c_write_table(init_tbl) succeed! -- 2012\/02\/23\n", __func__);

    // Speed up AE/AWB
	rc = mt9m114_i2c_read(client, client->addr, 0xA802, &tmpRegValue, WORD_LEN);
    if (rc < 0)
        goto out;
    tmpRegValue |= 0x8;
    rc = mt9m114_i2c_write(client, client->addr, 0xA802, tmpRegValue, WORD_LEN);	// Set bit 3=1  ae_track_ae_mode_fast_adaptation=1
    if (rc != 0)
		goto out;

    rc = mt9m114_i2c_write(client, client->addr, 0xC908, 0x01, BYTE_LEN);
    if (rc < 0)
		goto out;
    rc = mt9m114_i2c_write(client, client->addr, 0xC879, 0x01, BYTE_LEN);
    if (rc < 0)
		goto out;	

	rc = mt9m114_i2c_read(client, client->addr, 0xC909, &tmpRegValue, BYTE_LEN);
    if (rc < 0)
        goto out;
    tmpRegValue &= ~0x01;
    rc = mt9m114_i2c_write(client, client->addr, 0xC909, tmpRegValue, BYTE_LEN);
    if (rc != 0)
		goto out;

    rc = mt9m114_i2c_write(client, client->addr, 0xC909, 0x02, BYTE_LEN);
    if (rc < 0)
		goto out;	
    rc = mt9m114_i2c_write(client, client->addr, 0xA80A, 0x18, BYTE_LEN);
    if (rc < 0)
		goto out;	
    rc = mt9m114_i2c_write(client, client->addr, 0xA80B, 0x18, BYTE_LEN);
    if (rc < 0)
		goto out;	
    rc = mt9m114_i2c_write(client, client->addr, 0xAC16, 0x18, BYTE_LEN);
    if (rc < 0)
		goto out;	

	
	rc = mt9m114_i2c_read(client, client->addr, 0xC878, &tmpRegValue, BYTE_LEN);
    if (rc < 0)
        goto out;
    tmpRegValue |= 0x08;
    rc = mt9m114_i2c_write(client, client->addr, 0xC878, tmpRegValue, BYTE_LEN);
    if (rc != 0)
		goto out;
	

    // disables fade-to-black
	rc = mt9m114_i2c_read(client, client->addr, 0xBC02, &tmpRegValue, WORD_LEN);
    if (rc < 0)
        goto out;
    tmpRegValue |= 0x08;
    rc = mt9m114_i2c_write(client, client->addr, 0xBC02, tmpRegValue, WORD_LEN);
    if (rc < 0)
		goto out;
	

    // MIPI non-continuous clock mode
	rc = mt9m114_i2c_read(client, client->addr, 0x3C40, &tmpRegValue, WORD_LEN);
    if (rc < 0)
        goto out;
    tmpRegValue &= ~0x04;
    rc = mt9m114_i2c_write(client, client->addr, 0x3C40, tmpRegValue, WORD_LEN);
    if (rc < 0)
		goto out;
	
    rc = mt9m114_i2c_write(client, client->addr, 0x3C52, 0x0D10, WORD_LEN);
    if (rc < 0)
		goto out;

    // Change-Config
    rc = mt9m114_i2c_write(client, client->addr, 0x098E, 0xDC00, WORD_LEN);
    if (rc < 0)
		goto out;
    rc = mt9m114_i2c_write(client, client->addr, 0xDC00, 0x28, BYTE_LEN);
    if (rc < 0)
		goto out;	


    //Set State Command
    // (Optional) First check that the FW is ready to accept a new command
	rc = mt9m114_i2c_read(client, client->addr, MT9M114_command_register, &tmpRegValue, WORD_LEN);
    if (rc < 0)
        goto out;
	tmpRegValue = tmpRegValue & HOST_COMMAND_1;
    if ( tmpRegValue != 0) {
		printk("%s: Set State cmd bit is already set, tmpRegValue = 0x%x\n", __func__, tmpRegValue);
    }

    // (Mandatory) Issue the Set State command
    // We set the 'OK' bit so we can detect if the command fails
    // Note 0x8002 is equivalent to (HOST_COMMAND_OK | HOST_COMMAND_1)
    rc = mt9m114_i2c_write(client, client->addr, MT9M114_command_register, 0x8002, WORD_LEN);
    if (rc < 0)
		goto out;

    // Wait for the FW to complete the command (clear the HOST_COMMAND_1 bit)
    tmp_cnt = 0;

    do{
        if( tmp_cnt > 0 ) {
            mdelay(10);
        }
		rc = mt9m114_i2c_read(client, client->addr, MT9M114_command_register, &tmpRegValue, WORD_LEN);
		if (rc < 0)
			goto out;
        tmpRegValue = tmpRegValue & HOST_COMMAND_1;
        tmp_cnt++;
    }while(tmpRegValue != 0 && tmp_cnt < 10);

    // Check the 'OK' bit to see if the command was successful
	rc = mt9m114_i2c_read(client, client->addr, MT9M114_command_register, &tmpRegValue, WORD_LEN);
	if (rc < 0)
		goto out;
    tmpRegValue = tmpRegValue & HOST_COMMAND_OK;
    if ( tmpRegValue != 0x8000) {
		printk("%s: Set State cmd failed, tmpRegValue = 0x%x\n", __func__, tmpRegValue);
    }

	rc = mt9m114_i2c_read(client, client->addr, 0xDC01, &tmpRegValue, BYTE_LEN);
	if (rc < 0)
		goto out;		
    if ( tmpRegValue != 0x31) {
		printk("%s: System state is not STREAMING, tmpRegValue = 0x%x\n", __func__, tmpRegValue);
    }
	else
	{
		printk("%s: System state is STREAMING, tmpRegValue = 0x%x\n", __func__, tmpRegValue);
	}

	rc = mt9m114_i2c_read(client, client->addr, MT9M114_command_register, &tmpRegValue, WORD_LEN);
    if (rc < 0)
        goto out;
    tmpRegValue = tmpRegValue | HOST_COMMAND_OK;
    rc = mt9m114_i2c_write(client, client->addr, MT9M114_command_register, tmpRegValue, WORD_LEN);
    if (rc < 0)
		goto out;

    tmp_cnt = 0;

    do{
        if( tmp_cnt > 0 ) {
            mdelay(10);
        }
		rc = mt9m114_i2c_read(client, client->addr, MT9M114_command_register, &tmpRegValue, WORD_LEN);
		if (rc < 0)
			goto out;		
        tmpRegValue = tmpRegValue & HOST_COMMAND_0;
        tmp_cnt++;
    }while(tmpRegValue != 0 && tmp_cnt < 10);

	rc = mt9m114_i2c_read(client, client->addr, MT9M114_command_register, &tmpRegValue, WORD_LEN);
    if (rc < 0)
        goto out;
    tmpRegValue = tmpRegValue | HOST_COMMAND_0;
    rc = mt9m114_i2c_write(client, client->addr, MT9M114_command_register, tmpRegValue, WORD_LEN);
    if (rc < 0)
		goto out;

    tmp_cnt = 0;

    do{
        if( tmp_cnt > 0 ) {
            mdelay(10);
        }
		rc = mt9m114_i2c_read(client, client->addr, MT9M114_command_register, &tmpRegValue, WORD_LEN);
		if (rc < 0)
			goto out;			
        tmpRegValue = tmpRegValue & HOST_COMMAND_0;
        tmp_cnt++;
    }while(tmpRegValue != 0 && tmp_cnt < 10);


	rc = mt9m114_i2c_read(client, client->addr, MT9M114_command_register, &tmpRegValue, WORD_LEN);
    if (rc < 0)
        goto out;	
    tmpRegValue = tmpRegValue & HOST_COMMAND_OK;
    if ( tmpRegValue != 0x8000) {
		printk("%s: Couldn't apply patch, tmpRegValue = 0x%x\n", __func__, tmpRegValue);
    }


	rc = mt9m114_i2c_read(client, client->addr, 0xE008, &tmpRegValue, BYTE_LEN);
    if (rc < 0)
        goto out;	
    if ( tmpRegValue != 0) {
		printk("%s: Apply status non-zero, tmpRegValue = 0x%x\n", __func__, tmpRegValue);
    }	
	
	printk("[HL]%s: Original ---\n", __func__);
	
out:
	printk("[HL]%s: Original reg read/write fail, out---\n", __func__);	
	return rc;
#else
	int rc;
	unsigned short tmpRegValue;
	int tmp_cnt = 0;
	
	printk("[HL]%s: NEW+++\n", __func__);

    do{
        if( tmp_cnt > 0 ) {
            mdelay(10);
        }
		rc = mt9m114_i2c_read(client, client->addr, MT9M114_command_register, &tmpRegValue, WORD_LEN);
	    if (rc != 0)
	        goto out;		
        tmpRegValue = tmpRegValue & HOST_COMMAND_1;
        tmp_cnt++;
    }while(tmpRegValue != 0 && tmp_cnt < 10);

    //Errata item 2
	rc = mt9m114_i2c_read(client, client->addr, MT9M114_reset_register, &tmpRegValue, WORD_LEN);
    if (rc < 0)
        goto out;
	printk("[HL]%s: MT9M114_reset_register ==> tmpRegValue = 0x%x\n", __func__, tmpRegValue);	
    tmpRegValue &= 0x200;
	printk("[HL]%s: MT9M114_reset_register ==> tmpRegValue = 0x%x\n", __func__, tmpRegValue);
    rc = mt9m114_i2c_write(client, client->addr, MT9M114_reset_register, tmpRegValue, WORD_LEN);
    if (rc != 0)
		goto out;

	printk("[HL]%s: Errata item 2\n", __func__);

	//Write init table
	rc = mt9m114_i2c_write_table(client, init_tbl,
			ARRAY_SIZE(init_tbl));
	if (rc)
		goto out;
	printk("[HL]%s: mt9m114_i2c_write_table(init_tbl) succeed!\n", __func__);

	// Wait for the FW to complete the command (clear the HOST_COMMAND_1 bit)
	tmp_cnt = 0;
	do
	{
		if( tmp_cnt > 0 ) 
		{
			mdelay(10);
		}
		rc = mt9m114_i2c_read(client, client->addr, MT9M114_command_register, &tmpRegValue, WORD_LEN);
		if (rc < 0)
			goto out;
		tmpRegValue = tmpRegValue & HOST_COMMAND_1;
		tmp_cnt++;
	}while(tmpRegValue != 0 && tmp_cnt < 10);

	// Check the 'OK' bit to see if the command was successful
	rc = mt9m114_i2c_read(client, client->addr, MT9M114_command_register, &tmpRegValue, WORD_LEN);
	if (rc < 0)
		goto out;
	tmpRegValue = tmpRegValue & HOST_COMMAND_OK;
	if ( tmpRegValue != 0x8000) 
	{
		printk("%s: Set State cmd failed, tmpRegValue = 0x%x\n", __func__, tmpRegValue);
	}

	//Check whether sensor is in streaming mode
	rc = mt9m114_i2c_read(client, client->addr, 0xDC01, &tmpRegValue, BYTE_LEN);
	if (rc < 0)
		goto out;		 
	if ( tmpRegValue != 0x31) 
	{
		printk("%s: System state is not STREAMING, tmpRegValue = 0x%x\n", __func__, tmpRegValue);
	}
	else
	{
		printk("%s: System state is STREAMING, tmpRegValue = 0x%x\n", __func__, tmpRegValue);
	}
	 
	printk("[HL]%s: NEW---\n", __func__);
	
out:
	printk("[HL]%s: reg read/write fail, out---\n", __func__);	
	return rc;

#endif
}
//HL+}

static int mt9m114_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = sd->dev_priv;	//HL*CSI_20111123
	int ret = 0;
	//SW4-L1-HL-Camera-FixCaptureOverExposure-00+{_20120229	
	int rc;
	unsigned short tmpRegValue;
	int tmp_cnt = 0;
	//SW4-L1-HL-Camera-FixCaptureOverExposure-00+}_20120229	

	printk("[HL]%s: +++\n", __func__);

	if (enable) {
		#if 0
		ret = mt9m114_reg_clr(client, REG_MT9M114_SYSTEM_CONTROL0,
					MT9M114_SYSTEM_CONTROL0_SW_PWRDN);
		if (ret)
			goto out;
		#else
		printk("[HL]%s: enable = 1\n", __func__);

		//SW4-L1-HL-Camera-FixCaptureOverExposure-00*{_20120229
		rc = mt9m114_i2c_read(client, client->addr, 0xDC01, &tmpRegValue, BYTE_LEN);
		if (rc < 0)
			goto out;		
	    if (tmpRegValue != 0x31) 
		{
			printk("%s: System state is not STREAMING, tmpRegValue = 0x%x\n", __func__, tmpRegValue);
			
			//if ((tmpRegValue == 0x41) || (tmpRegValue == 0x52))	//0x41 SYS_STATE_SUSPENDED, 0x52 SYS_STATE_STANDBY
			if (tmpRegValue == 0x41)	//0x41 SYS_STATE_SUSPENDED
			{
				//printk("%s: System state is SUSPENDED or STANDBY\n", __func__);
				printk("%s: System state is SUSPENDED\n", __func__);
				
				// Change-Config
				rc = mt9m114_i2c_write(client, client->addr, 0x098E, 0xDC00, WORD_LEN);
				if (rc < 0)
					goto out;
				rc = mt9m114_i2c_write(client, client->addr, 0xDC00, 0x34, BYTE_LEN);	//0x40 SYS_STATE_ENTER_SUSPEND 
				if (rc < 0)
					goto out;	
				
				
				//Set State Command
				// (Optional) First check that the FW is ready to accept a new command
				rc = mt9m114_i2c_read(client, client->addr, MT9M114_command_register, &tmpRegValue, WORD_LEN);
				if (rc < 0)
					goto out;
				tmpRegValue = tmpRegValue & HOST_COMMAND_1;
				if ( tmpRegValue != 0) {
					printk("%s: Set State cmd bit is already set, tmpRegValue = 0x%x\n", __func__, tmpRegValue);
				}
				
				// (Mandatory) Issue the Set State command
				// We set the 'OK' bit so we can detect if the command fails
				// Note 0x8002 is equivalent to (HOST_COMMAND_OK | HOST_COMMAND_1)
				rc = mt9m114_i2c_write(client, client->addr, MT9M114_command_register, 0x8002, WORD_LEN);
				if (rc < 0)
					goto out;
				
				// Wait for the FW to complete the command (clear the HOST_COMMAND_1 bit)
				tmp_cnt = 0;
				
				do{
					if( tmp_cnt > 0 ) {
						mdelay(10);
					}
					rc = mt9m114_i2c_read(client, client->addr, MT9M114_command_register, &tmpRegValue, WORD_LEN);
					if (rc < 0)
						goto out;
					tmpRegValue = tmpRegValue & HOST_COMMAND_1;
					tmp_cnt++;
				}while(tmpRegValue != 0 && tmp_cnt < 10);
				
				// Check the 'OK' bit to see if the command was successful
				rc = mt9m114_i2c_read(client, client->addr, MT9M114_command_register, &tmpRegValue, WORD_LEN);
				if (rc < 0)
					goto out;
				tmpRegValue = tmpRegValue & HOST_COMMAND_OK;
				if ( tmpRegValue != 0x8000) {
					printk("%s: Set State cmd failed, tmpRegValue = 0x%x\n", __func__, tmpRegValue);
				}
				
				rc = mt9m114_i2c_read(client, client->addr, 0xDC01, &tmpRegValue, BYTE_LEN);
				if (rc < 0)
					goto out;		
				if ( tmpRegValue != 0x31) {
					printk("%s: System state is not STREAMING, tmpRegValue = 0x%x\n", __func__, tmpRegValue);
				}
				else
				{
					printk("%s: System state is STREAMING, tmpRegValue = 0x%x\n", __func__, tmpRegValue);
				}

			}
			else
			{
				//HL+{CSI_20111123
				ret = mt9m114_reg_init(client);
				if (ret < 0)
					goto out;		
				//HL+}CSI_20111123
			}
		}
		else
		{
			printk("%s: System state is STREAMING, tmpRegValue = 0x%x\n", __func__, tmpRegValue);
		}
		#endif
		//SW4-L1-HL-Camera-FixCaptureOverExposure-00*}_20120229
	} 
	else 
	{
		#if 0
		u8 tmpreg = 0;

		ret = mt9m114_reg_read(client, REG_MT9M114_SYSTEM_CONTROL0,
				      &tmpreg);
		if (ret)
			goto out;

		ret = mt9m114_reg_write(client, REG_MT9M114_SYSTEM_CONTROL0,
				tmpreg | MT9M114_SYSTEM_CONTROL0_SW_PWRDN);
		if (ret)
			goto out;
		#else
		printk("[HL]%s: enable = 0\n", __func__);

		//SW4-L1-HL-Camera-FixCaptureOverExposure-00*{_20120229
		rc = mt9m114_i2c_read(client, client->addr, 0xDC01, &tmpRegValue, BYTE_LEN);
		if (rc < 0)
			goto out;		
	    if ( tmpRegValue != 0x31) 
		{
			printk("%s: System state is not STREAMING, tmpRegValue = 0x%x\n", __func__, tmpRegValue);
		}
		else
		{
			// Change-Config
			rc = mt9m114_i2c_write(client, client->addr, 0x098E, 0xDC00, WORD_LEN);
			if (rc < 0)
				goto out;
			rc = mt9m114_i2c_write(client, client->addr, 0xDC00, 0x40, BYTE_LEN);	//0x40 SYS_STATE_ENTER_SUSPEND 
			if (rc < 0)
				goto out;	
			
			
			//Set State Command
			// (Optional) First check that the FW is ready to accept a new command
			rc = mt9m114_i2c_read(client, client->addr, MT9M114_command_register, &tmpRegValue, WORD_LEN);
			if (rc < 0)
				goto out;
			tmpRegValue = tmpRegValue & HOST_COMMAND_1;
			if ( tmpRegValue != 0) {
				printk("%s: Set State cmd bit is already set, tmpRegValue = 0x%x\n", __func__, tmpRegValue);
			}
			
			// (Mandatory) Issue the Set State command
			// We set the 'OK' bit so we can detect if the command fails
			// Note 0x8002 is equivalent to (HOST_COMMAND_OK | HOST_COMMAND_1)
			rc = mt9m114_i2c_write(client, client->addr, MT9M114_command_register, 0x8002, WORD_LEN);
			if (rc < 0)
				goto out;
			
			// Wait for the FW to complete the command (clear the HOST_COMMAND_1 bit)
			tmp_cnt = 0;
			
			do{
				if( tmp_cnt > 0 ) {
					mdelay(10);
				}
				rc = mt9m114_i2c_read(client, client->addr, MT9M114_command_register, &tmpRegValue, WORD_LEN);
				if (rc < 0)
					goto out;
				tmpRegValue = tmpRegValue & HOST_COMMAND_1;
				tmp_cnt++;
			}while(tmpRegValue != 0 && tmp_cnt < 10);
			
			// Check the 'OK' bit to see if the command was successful
			rc = mt9m114_i2c_read(client, client->addr, MT9M114_command_register, &tmpRegValue, WORD_LEN);
			if (rc < 0)
				goto out;
			tmpRegValue = tmpRegValue & HOST_COMMAND_OK;
			if ( tmpRegValue != 0x8000) {
				printk("%s: Set State cmd failed, tmpRegValue = 0x%x\n", __func__, tmpRegValue);
			}
			
			rc = mt9m114_i2c_read(client, client->addr, 0xDC01, &tmpRegValue, BYTE_LEN);
			if (rc < 0)
				goto out;		
			if ( tmpRegValue != 0x31) {
				printk("%s: System state is not STREAMING, tmpRegValue = 0x%x\n", __func__, tmpRegValue);
			}
			else
			{
				printk("%s: System state is STREAMING, tmpRegValue = 0x%x\n", __func__, tmpRegValue);
			}
		}		
		//SW4-L1-HL-Camera-FixCaptureOverExposure-00*}_20120229		
		#endif
	}

	printk("[HL]%s: ---\n", __func__);	

out:	//HL*CSI_20111123
	printk("[HL]%s: out---\n", __func__);		//HL*CSI_20111123
	return ret;
}

static int mt9m114_set_bus_param(struct soc_camera_device *icd,
				 unsigned long flags)
{
	printk("[HL]%s: +++---\n", __func__);

	/* TODO: Do the right thing here, and validate bus params */
	return 0;
}

static unsigned long mt9m114_query_bus_param(struct soc_camera_device *icd)
{
	unsigned long flags = SOCAM_PCLK_SAMPLE_FALLING |
		SOCAM_HSYNC_ACTIVE_HIGH | SOCAM_VSYNC_ACTIVE_HIGH |
		SOCAM_DATA_ACTIVE_HIGH | SOCAM_MASTER;

	/* TODO: Do the right thing here, and validate bus params */

	flags |= SOCAM_DATAWIDTH_10;

	printk("[HL]%s: +++---\n", __func__);

	return flags;
}

static int mt9m114_g_fmt(struct v4l2_subdev *sd,
			 struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = sd->dev_priv;
	struct mt9m114 *mt9m114 = to_mt9m114(client);

	printk("[HL]%s: +++\n", __func__);

	mf->width	= mt9m114_frmsizes[mt9m114->i_size].width;
	mf->height	= mt9m114_frmsizes[mt9m114->i_size].height;
	mf->code	= mt9m114_fmts[mt9m114->i_fmt].code;
	mf->colorspace	= mt9m114_fmts[mt9m114->i_fmt].colorspace;
	mf->field	= V4L2_FIELD_NONE;

	printk("[HL]%s: mf->width = %d\n", __func__, mf->width);
	printk("[HL]%s: mf->height = %d\n", __func__, mf->height);
	printk("[HL]%s: mf->code = %d\n", __func__, mf->code);
	printk("[HL]%s: mf->colorspace = %d\n", __func__, mf->colorspace);	

	printk("[HL]%s: ---\n", __func__);

	return 0;
}

static int mt9m114_try_fmt(struct v4l2_subdev *sd,
			   struct v4l2_mbus_framefmt *mf)
{
	int i_fmt;
	int i_size;

	printk("[HL]%s: +++\n", __func__);

	printk("[HL]%s: mf->code = %d\n", __func__, mf->code);

	i_fmt = mt9m114_find_datafmt(mf->code);

	printk("[HL]%s: i_fmt = %d\n", __func__, i_fmt);

	mf->code = mt9m114_fmts[i_fmt].code;
	mf->colorspace	= mt9m114_fmts[i_fmt].colorspace;
	mf->field	= V4L2_FIELD_NONE;

	i_size = mt9m114_find_framesize(mf->width, mf->height);

	mf->width = mt9m114_frmsizes[i_size].width;
	mf->height = mt9m114_frmsizes[i_size].height;

	printk("[HL]%s: ---\n", __func__);

	return 0;
}

static int mt9m114_s_fmt(struct v4l2_subdev *sd,
			 struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = sd->dev_priv;
	struct mt9m114 *mt9m114 = to_mt9m114(client);
	int ret;
	u8 fmtreg = 0, fmtmuxreg = 0;

	printk("[HL]%s: +++\n", __func__);

	ret = mt9m114_try_fmt(sd, mf);
	if (ret < 0)
		return ret;

	mt9m114->i_size = mt9m114_find_framesize(mf->width, mf->height);
	mt9m114->i_fmt = mt9m114_find_datafmt(mf->code);

	switch ((u32)mt9m114_fmts[mt9m114->i_fmt].code) {
	case V4L2_MBUS_FMT_UYVY8_2X8:
		fmtreg = MT9M114_FORMAT_CONTROL00_OUTFMT_YUV422 |
		MT9M114_FORMAT_CONTROL00_SEQ_YUV422_UYVY;
		fmtmuxreg = 0;
		break;
	case V4L2_MBUS_FMT_YUYV8_2X8:
		fmtreg = MT9M114_FORMAT_CONTROL00_OUTFMT_YUV422 |
		MT9M114_FORMAT_CONTROL00_SEQ_YUV422_YUYV;
		fmtmuxreg = 0;
		break;
	default:
		/* This shouldn't happen */
		ret = -EINVAL;
		return ret;
	}

	printk("[HL]%s: mt9m114->i_fmt = %d, (u32)mt9m114_fmts[mt9m114->i_fmt].code = %d\n", __func__, (u32)mt9m114_fmts[mt9m114->i_fmt].code, (u32)mt9m114_fmts[mt9m114->i_fmt].code);
		
	//HL
	#if 0
	ret = mt9m114_reg_write(client, REG_MT9M114_FORMAT_CONTROL00,
							fmtreg);
	if (ret)
		return ret;

	ret = mt9m114_reg_write(client, REG_MT9M114_FORMAT_MUX_CONTROL,
							fmtmuxreg);
	if (ret)
		return ret;

	ret = mt9m114_config_timing(client);
	if (ret)
		return ret;

	if ((mt9m114->i_size == MT9M114_SIZE_QVGA) ||
	    (mt9m114->i_size == MT9M114_SIZE_VGA)) {
		ret = mt9m114_reg_set(client, REG_MT9M114_ISP_CONTROL01,
				MT9M114_ISP_CONTROL01_SCALE_EN);
	} else {
		ret = mt9m114_reg_clr(client, REG_MT9M114_ISP_CONTROL01,
				MT9M114_ISP_CONTROL01_SCALE_EN);
	}
	#else

	#if 0
	ret = mt9m114_reg_init(client);
	if (ret < 0)
		goto out;
	#else
	printk("[HL]%s: No execute mt9m114_reg_init()\n", __func__);
	#endif
	
	#endif

	printk("[HL]%s: ---\n", __func__);

out:
	printk("[HL]%s: out---\n", __func__);	
	return ret;	
}

static int mt9m114_g_chip_ident(struct v4l2_subdev *sd,
				struct v4l2_dbg_chip_ident *id)
{
	struct i2c_client *client = sd->dev_priv;

	printk("[HL]%s: +++\n", __func__);

	if (id->match.type != V4L2_CHIP_MATCH_I2C_ADDR)
		return -EINVAL;

	if (id->match.addr != client->addr)
		return -ENODEV;

	id->ident	= V4L2_IDENT_MT9M114;
	id->revision	= 0;

	printk("[HL]%s: ---\n", __func__);

	return 0;
}

static int mt9m114_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = sd->dev_priv;
	struct mt9m114 *mt9m114 = to_mt9m114(client);

	printk("[HL]%s: +++, ctrl->id = 0x%x\n", __func__, ctrl->id);

	dev_dbg(&client->dev, "mt9m114_g_ctrl\n");

	//HL-{CSI_20111125
	#if 0
	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		ctrl->value = mt9m114->brightness;
		break;
	case V4L2_CID_CONTRAST:
		ctrl->value = mt9m114->contrast;
		break;
	case V4L2_CID_COLORFX:
		ctrl->value = mt9m114->colorlevel;
		break;
	}
	#else
	printk("[HL]%s: NO set any parameters\n", __func__);
	#endif
	//HL-}CSI_20111125

	printk("[HL]%s: ---\n", __func__);

	return 0;
}

static int mt9m114_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = sd->dev_priv;
	struct mt9m114 *mt9m114 = to_mt9m114(client);
	int i_sde_ctrl0 = 0;
	int i_sde_ctrl3 = 0, i_sde_ctrl4 = 0;
	int i_sde_ctrl5 = 0, i_sde_ctrl6 = 0;
	int i_sde_ctrl7 = 0, i_sde_ctrl8 = 0;
	int ret = 0;

	printk("[HL]%s: +++, ctrl->id = 0x%x\n", __func__, ctrl->id);

	dev_dbg(&client->dev, "mt9m114_s_ctrl\n");

	//HL-{CSI_20111125
	#if 0
	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:

		if (ctrl->value > MT9M114_BRIGHTNESS_MAX)
			return -EINVAL;

		mt9m114->brightness = ctrl->value;

		ret = mt9m114_reg_write(client, REG_MT9M114_SDE_CTRL0,
				MT9M114_REG_MT9M114_SDE_CTRL0_CONTRAST_EN);
		if (ret)
			return ret;

		switch(mt9m114->brightness) {
		case MT9M114_BRIGHTNESS_MIN:
			i_sde_ctrl7 = 0x40;
			i_sde_ctrl8 = 0x08;
		break;
		case MT9M114_BRIGHTNESS_MAX:
			i_sde_ctrl7 = 0x40;
			i_sde_ctrl8 = 0x00;
		break;
		default:
			i_sde_ctrl7 = 0x00;
			i_sde_ctrl8 = 0x00;
		break;
		}

		ret = mt9m114_reg_write(client, REG_MT9M114_SDE_CTRL7,
							i_sde_ctrl7);
		if (ret)
			return ret;

		ret = mt9m114_reg_write(client, REG_MT9M114_SDE_CTRL8,
							i_sde_ctrl8);
		if (ret)
			return ret;

		break;
	case V4L2_CID_CONTRAST:

		if (ctrl->value > MT9M114_CONTRAST_MAX)
			return -EINVAL;

		mt9m114->contrast = ctrl->value;

		ret = mt9m114_reg_write(client, REG_MT9M114_SDE_CTRL0,
			MT9M114_REG_MT9M114_SDE_CTRL0_CONTRAST_EN);
		if (ret)
			return ret;

		switch(mt9m114->contrast) {
		case MT9M114_CONTRAST_MIN:
			i_sde_ctrl5 = 0x10;
			i_sde_ctrl6 = 0x10;
			i_sde_ctrl8 = 0x00;
		break;
		case MT9M114_CONTRAST_MAX:
			i_sde_ctrl5 = 0x20;
			i_sde_ctrl6 = 0x30;
			i_sde_ctrl8 = 0x08;
		break;
		default:
			i_sde_ctrl5 = 0x00;
			i_sde_ctrl6 = 0x20;
			i_sde_ctrl8 = 0x00;
		break;
		}

		ret = mt9m114_reg_write(client, REG_MT9M114_SDE_CTRL5,
							i_sde_ctrl5);
		if (ret)
			return ret;

		ret = mt9m114_reg_write(client, REG_MT9M114_SDE_CTRL6,
							i_sde_ctrl6);
		if (ret)
			return ret;

		ret = mt9m114_reg_write(client, REG_MT9M114_SDE_CTRL8,
							i_sde_ctrl8);
		if (ret)
			return ret;

		break;
	case V4L2_CID_COLORFX:
		if (ctrl->value > V4L2_COLORFX_NEGATIVE)
			return -EINVAL;

		mt9m114->colorlevel = ctrl->value;

		switch (mt9m114->colorlevel) {
		case V4L2_COLORFX_BW:
			i_sde_ctrl0 = MT9M114_REG_MT9M114_SDE_CTRL0_FIXED_V_EN |
					MT9M114_REG_MT9M114_SDE_CTRL0_FIXED_U_EN;
			i_sde_ctrl3 = 0x80;
			i_sde_ctrl4 = 0x80;
			break;
		case V4L2_COLORFX_SEPIA:
			i_sde_ctrl0 = MT9M114_REG_MT9M114_SDE_CTRL0_FIXED_V_EN |
					MT9M114_REG_MT9M114_SDE_CTRL0_FIXED_U_EN;
			i_sde_ctrl3 = 0x40;
			i_sde_ctrl4 = 0xA0;
			break;
		case V4L2_COLORFX_NEGATIVE:
			i_sde_ctrl0 = MT9M114_REG_MT9M114_SDE_CTRL0_NEGATIVE_EN;
			i_sde_ctrl3 = 0x00;
			i_sde_ctrl4 = 0x00;
			break;
		default:
			i_sde_ctrl0 = 0x00;
			i_sde_ctrl3 = 0x00;
			i_sde_ctrl4 = 0x00;
			break;
		}

		ret = mt9m114_reg_write(client, REG_MT9M114_SDE_CTRL0,
							i_sde_ctrl0);
		if (ret)
			return ret;

		ret = mt9m114_reg_write(client, REG_MT9M114_SDE_CTRL3,
							i_sde_ctrl3);
		if (ret)
			return ret;

		ret = mt9m114_reg_write(client, REG_MT9M114_SDE_CTRL4,
							i_sde_ctrl4);
		if (ret)
			return ret;

		break;
	}
	#else
	printk("[HL]%s: NO set any parameters\n", __func__);
	#endif
	//HL-}CSI_20111125

	printk("[HL]%s: ---\n", __func__);

	return ret;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int mt9m114_g_register(struct v4l2_subdev *sd,
			     struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = sd->dev_priv;

	printk("[HL]%s: +++\n", __func__);

	if (reg->match.type != V4L2_CHIP_MATCH_I2C_ADDR || reg->size > 2)
		return -EINVAL;

	if (reg->match.addr != client->addr)
		return -ENODEV;

	reg->size = 2;
	if (mt9m114_reg_read(client, reg->reg, &reg->val))
		return -EIO

	printk("[HL]%s: ---\n", __func__);

	return 0;
}

static int mt9m114_s_register(struct v4l2_subdev *sd,
			     struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = sd->dev_priv;

	printk("[HL]%s: +++\n", __func__);

	if (reg->match.type != V4L2_CHIP_MATCH_I2C_ADDR || reg->size > 2)
		return -EINVAL;

	if (reg->match.addr != client->addr)
		return -ENODEV;

	if (mt9m114_reg_write(client, reg->reg, reg->val))
		return -EIO;

	printk("[HL]%s: ---\n", __func__);

	return 0;
}
#endif

static struct soc_camera_ops mt9m114_ops = {
	.set_bus_param		= mt9m114_set_bus_param,
	.query_bus_param	= mt9m114_query_bus_param,
	.controls		= mt9m114_controls,
	.num_controls		= ARRAY_SIZE(mt9m114_controls),
};

static int mt9m114_init(struct i2c_client *client)
{
	struct mt9m114 *mt9m114 = to_mt9m114(client);
	int ret = 0;

	printk("[HL]%s: +++\n", __func__);

	//HL
	#if 0
	/* SW Reset */
	ret = mt9m114_reg_set(client, REG_MT9M114_SYSTEM_CONTROL0,
			MT9M114_SYSTEM_CONTROL0_SW_RESET);
	if (ret)
		goto out;

	msleep(2);

	ret = mt9m114_reg_clr(client, REG_MT9M114_SYSTEM_CONTROL0,
			MT9M114_SYSTEM_CONTROL0_SW_RESET);
	if (ret)
		goto out;

	/* SW Powerdown */
	ret = mt9m114_reg_set(client, REG_MT9M114_SYSTEM_CONTROL0,
			MT9M114_SYSTEM_CONTROL0_SW_PWRDN);
	if (ret)
		goto out;

	ret = mt9m114_i2c_write_table(client, init_tbl,
			ARRAY_SIZE(init_tbl));
	if (ret)
		goto out;

	ret = mt9m114_i2c_write_table(client, configscript_common2,
			ARRAY_SIZE(configscript_common2));
	if (ret)
		goto out;
	#else

	#if 0
	ret = mt9m114_i2c_write_table(client, init_tbl,
			ARRAY_SIZE(init_tbl));
	if (ret)
		goto out;

	printk("[HL]%s: mt9m114_i2c_write_table(init_tbl) succeed!\n", __func__);
	#else
	ret = mt9m114_reg_init(client);
	if (ret < 0)
		goto out;
	#endif
	
	#endif

	/* default brightness and contrast */
	mt9m114->brightness = MT9M114_BRIGHTNESS_DEF;
	mt9m114->contrast = MT9M114_CONTRAST_DEF;

	mt9m114->colorlevel = V4L2_COLORFX_NONE;

	dev_dbg(&client->dev, "Sensor initialized\n");

	printk("[HL]%s: ---\n", __func__);

out:
	printk("[HL]%s: out---\n", __func__);	
	return ret;
}

/*
 * Interface active, can use i2c. If it fails, it can indeed mean, that
 * this wasn't our capture interface, so, we wait for the right one
 */
static int mt9m114_video_probe(struct soc_camera_device *icd,
			      struct i2c_client *client)
{
	//unsigned long flags;
	int ret = 0;
	u16 id = 0;	//u8 revision = 0;	//SW4-L1-HL-Camera-FTM_BringUp-00*

	printk("[HL]%s: +++\n", __func__);

	/*
	 * We must have a parent by now. And it cannot be a wrong one.
	 * So this entire test is completely redundant.
	 */
	if (!icd->dev.parent ||
	    to_soc_camera_host(icd->dev.parent)->nr != icd->iface)
		return -ENODEV;

	ret = mt9m114_i2c_read(client, client->addr, REG_MT9M114_CHIP_ID, &id, WORD_LEN);	//SW4-L1-HL-Camera-FTM_BringUp-00*
	if (ret) {
		dev_err(&client->dev, "Failure to detect MT9M114 chip\n");
		goto out;
	}

	//revision &= MT9M114_CHIP_REVISION_MASK;

	//flags = SOCAM_DATAWIDTH_8;

	printk("Detected a MT9M114 chip, id 0x%x\n",
		 id);

	printk("[HL]%s: ---\n", __func__);

	/* TODO: Do something like mt9m114_init */

out:
	return ret;
}

static void mt9m114_video_remove(struct soc_camera_device *icd)
{
	dev_dbg(&icd->dev, "Video removed: %p, %p\n",
		icd->dev.parent, icd->vdev);
	printk("[HL]%s: +++---\n", __func__);	
}

static struct v4l2_subdev_core_ops mt9m114_subdev_core_ops = {
	.g_chip_ident	= mt9m114_g_chip_ident,
	.g_ctrl		= mt9m114_g_ctrl,
	.s_ctrl 	= mt9m114_s_ctrl,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register	= mt9m114_g_register,
	.s_register	= mt9m114_s_register,
#endif
};

static int mt9m114_enum_fmt(struct v4l2_subdev *sd, unsigned int index,
			    enum v4l2_mbus_pixelcode *code)
{
	printk("[HL]%s: +++\n", __func__);

	if (index >= ARRAY_SIZE(mt9m114_fmts))
		return -EINVAL;

	*code = mt9m114_fmts[index].code;

	printk("[HL]%s: ---\n", __func__);
	
	return 0;
}

static int mt9m114_enum_framesizes(struct v4l2_subdev *sd,
				  struct v4l2_frmsizeenum *fsize)
{
	printk("[HL]%s: +++\n", __func__);

	if (fsize->index > MT9M114_SIZE_LAST)
		return -EINVAL;

	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->pixel_format = V4L2_PIX_FMT_UYVY;

	fsize->discrete = mt9m114_frmsizes[fsize->index];

	printk("[HL]%s: ---\n", __func__);

	return 0;
}

static int mt9m114_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	struct i2c_client *client = sd->dev_priv;
	struct mt9m114 *mt9m114 = to_mt9m114(client);
	struct v4l2_captureparm *cparm;

	printk("[HL]%s: +++\n", __func__);

	if (param->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	cparm = &param->parm.capture;

	memset(param, 0, sizeof(*param));
	param->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	cparm->capability = V4L2_CAP_TIMEPERFRAME;

	switch (mt9m114->i_size) {
	//SW4-L1-HL-Camera-FTM-FixCaptureImageQualityTooLow-00+{_20120229
	#if 0
	case MT9M114_SIZE_130MP:
	{
		cparm->timeperframe.numerator = 1;		
		cparm->timeperframe.denominator = 30;
		break;
	}
	#endif
	//SW4-L1-HL-Camera-FTM-FixCaptureImageQualityTooLow-00+}_20120229
	case MT9M114_SIZE_720P:
	//HL+{
	{
		cparm->timeperframe.numerator = 1;		
		cparm->timeperframe.denominator = 30;
		break;
	}
	//HL+}
	case MT9M114_SIZE_VGA:
	//HL+{
	{
		cparm->timeperframe.numerator = 1;		
		cparm->timeperframe.denominator = 60;	//75;	//HL*CSI_20111125
		break;
	}
	//HL+}		
	case MT9M114_SIZE_QVGA:
	//HL+{
	{
		cparm->timeperframe.numerator = 1;		
		cparm->timeperframe.denominator = 120;
		break;
	}
	//HL+}		
	default:
		cparm->timeperframe.numerator = 1;
		cparm->timeperframe.denominator = 24;
		break;
	}

	printk("[HL]%s: ---\n", __func__);

	return 0;
}
static int mt9m114_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	/*
	 * FIXME: This just enforces the hardcoded framerates until this is
	 * flexible enough.
	 */
	printk("[HL]%s: +++---\n", __func__);	 
	return mt9m114_g_parm(sd, param);
}

static struct v4l2_subdev_video_ops mt9m114_subdev_video_ops = {
	.s_stream	= mt9m114_s_stream,
	.s_mbus_fmt	= mt9m114_s_fmt,
	.g_mbus_fmt	= mt9m114_g_fmt,
	.try_mbus_fmt	= mt9m114_try_fmt,
	.enum_mbus_fmt	= mt9m114_enum_fmt,
	.enum_framesizes = mt9m114_enum_framesizes,
	.g_parm = mt9m114_g_parm,
	.s_parm = mt9m114_s_parm,
};

static int mt9m114_g_skip_frames(struct v4l2_subdev *sd, u32 *frames)
{
	/* Quantity of initial bad frames to skip. Revisit. */
	*frames = 3;

	printk("[HL]%s: +++---, frames = 3\n", __func__);

	return 0;
}

static int mt9m114_g_interface_parms(struct v4l2_subdev *sd,
			struct v4l2_subdev_sensor_interface_parms *parms)
{
	struct i2c_client *client = sd->dev_priv;
	struct mt9m114 *mt9m114 = to_mt9m114(client);

	printk("[HL]%s: +++\n", __func__);

	if (!parms)
		return -EINVAL;

	parms->if_type = mt9m114->plat_parms->if_type;
	parms->if_mode = mt9m114->plat_parms->if_mode;
	parms->parms.serial = mipi_cfgs[mt9m114->i_size];

	printk("[HL]%s: ---\n", __func__);

	return 0;
}

static struct v4l2_subdev_sensor_ops mt9m114_subdev_sensor_ops = {
	.g_skip_frames	= mt9m114_g_skip_frames,
	.g_interface_parms = mt9m114_g_interface_parms,
};

static struct v4l2_subdev_ops mt9m114_subdev_ops = {
	.core	= &mt9m114_subdev_core_ops,
	.video	= &mt9m114_subdev_video_ops,
	.sensor	= &mt9m114_subdev_sensor_ops,
};

static int mt9m114_probe(struct i2c_client *client,
			 const struct i2c_device_id *did)
{
	struct mt9m114 *mt9m114;
	struct soc_camera_device *icd = client->dev.platform_data;
	struct soc_camera_link *icl;
	int ret;

	printk("[HL]%s: +++\n", __func__);

	if (!icd) {
		dev_err(&client->dev, "MT9M114: missing soc-camera data!\n");
		return -EINVAL;
	}

	icl = to_soc_camera_link(icd);
	if (!icl) {
		dev_err(&client->dev, "MT9M114 driver needs platform data\n");
		return -EINVAL;
	}

	if (!icl->priv) {
		dev_err(&client->dev,
			"MT9M114 driver needs i/f platform data\n");
		return -EINVAL;
	}

	mt9m114 = kzalloc(sizeof(struct mt9m114), GFP_KERNEL);
	if (!mt9m114)
		return -ENOMEM;

	v4l2_i2c_subdev_init(&mt9m114->subdev, client, &mt9m114_subdev_ops);

	/* Second stage probe - when a capture adapter is there */
	icd->ops		= &mt9m114_ops;

	mt9m114->i_size = MT9M114_SIZE_VGA;	//New -- MT9M114_SIZE_130MP;	//Original -- MT9M114_SIZE_VGA;	//original_old -- MT9M114_SIZE_720P;	//modify -- MT9M114_SIZE_VGA	//SW4-L1-HL-Camera-FTM-FixCaptureImageQualityTooLow-00*_20120229
	mt9m114->i_fmt = 0; /* First format in the list */
	mt9m114->plat_parms = icl->priv;

	printk("[HL]%s: mt9m114->i_size = %d\n", __func__, mt9m114->i_size);

	ret = mt9m114_video_probe(icd, client);
	if (ret) {
		icd->ops = NULL;
		kfree(mt9m114);
		return ret;
	}

	/* init the sensor here */
	ret = mt9m114_init(client);
	if (ret) {
		dev_err(&client->dev, "Failed to initialize sensor\n");
		ret = -EINVAL;
	}

	printk("[HL]%s: ---\n", __func__);

	return ret;
}

static int mt9m114_remove(struct i2c_client *client)
{
	struct mt9m114 *mt9m114 = to_mt9m114(client);
	struct soc_camera_device *icd = client->dev.platform_data;

	printk("[HL]%s: +++\n", __func__);

	icd->ops = NULL;
	mt9m114_video_remove(icd);
	client->driver = NULL;
	kfree(mt9m114);

	printk("[HL]%s: ---\n", __func__);

	return 0;
}

static const struct i2c_device_id mt9m114_id[] = {
	{ "mt9m114", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mt9m114_id);

static struct i2c_driver mt9m114_i2c_driver = {
	.driver = {
		.name = "mt9m114",
	},
	.probe		= mt9m114_probe,
	.remove		= mt9m114_remove,
	.id_table	= mt9m114_id,
};

static int __init mt9m114_mod_init(void)
{
	return i2c_add_driver(&mt9m114_i2c_driver);
}

static void __exit mt9m114_mod_exit(void)
{
	i2c_del_driver(&mt9m114_i2c_driver);
}

module_init(mt9m114_mod_init);
module_exit(mt9m114_mod_exit);

MODULE_DESCRIPTION("Aptina MT9M114 Camera driver");
MODULE_AUTHOR("Helli Liu <helliliu@fih-foxconn.com>");
MODULE_LICENSE("GPL");
