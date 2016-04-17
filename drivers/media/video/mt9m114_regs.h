/*
 * Aptina MT9M114 sensor driver - Register definitions
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

#ifndef _MT9M114_REGS_H_
#define _MT9M114_REGS_H_

/* Register offsets */
//SW4-L1-HL-Camera-FTM_BringUp-00+{
#define REG_MT9M114_CHIP_ID			0x0000

enum mt9m114_width {
	WORD_LEN,
	DOUBLE_WORD_LEN,
	BYTE_LEN,
	WORD_POLL,
	BYTE_POLL
};

#define HOST_COMMAND_0  (0x0001) // Apply Patch: The command tells the Patch Loader task to apply a patch.
#define HOST_COMMAND_1  (0x0002) // Set State : The command takes a parameter (SYSMGR_NEXT_STATE) to tell the System Mgr which state to switch to.
#define HOST_COMMAND_2  (0x0004) // Refresh : The command tells the Sequencer task to refresh the hardware configuration.
#define HOST_COMMAND_3  (0x0008) // Wait For Event : The Wait For Event command allows the Host to synchronize with the MT9M114 frame timing.
#define HOST_COMMAND_OK (0x8000) //

// SYSCTL Registers
#define MT9M114_chip_id                         0x0000
#define MT9M114_clocks_control                  0x0016
#define MT9M114_reset_and_misc_control          0x001A
#define MT9M114_pad_slew                        0x001E
#define MT9M114_user_defined_device_address_id  0x002E
#define MT9M114_pad_control                     0x0032
#define MT9M114_command_register                0x0080

// XDMA Registers
#define MT9M114_access_ctl_stat                 0x0982
#define MT9M114_physical_address_access         0x098A
#define MT9M114_logical_address_access          0x098E
#define MT9M114_mcu_variable_data0              0x0990
#define MT9M114_mcu_variable_data1              0x0992
#define MT9M114_mcu_variable_data2              0x0994
#define MT9M114_mcu_variable_data3              0x0996
#define MT9M114_mcu_variable_data4              0x0998
#define MT9M114_mcu_variable_data5              0x099A
#define MT9M114_mcu_variable_data6              0x099C
#define MT9M114_mcu_variable_data7              0x099E

// Core Registers
#define MT9M114_y_addr_start_                   0x3002
#define MT9M114_x_addr_start_                   0x3004
#define MT9M114_y_addr_end_                     0x3006
#define MT9M114_x_addr_end_                     0x3008
#define MT9M114_frame_length_lines_             0x300A
#define MT9M114_line_length_pck_                0x300C
#define MT9M114_coarse_integration_time_        0x3012
#define MT9M114_fine_integration_time_          0x3014
#define MT9M114_reset_register                  0x301A
#define MT9M114_flash                           0x3046
#define MT9M114_flash_count                     0x3048
#define MT9M114_green1_gain                     0x3056
#define MT9M114_blue_gain                       0x3058
#define MT9M114_red_gain                        0x305A
#define MT9M114_green2_gain                     0x305C
#define MT9M114_global_gain                     0x305E
#define MT9M114_fuse_id1                        0x31F4
#define MT9M114_fuse_id2                        0x31F6
#define MT9M114_fuse_id3                        0x31F8
#define MT9M114_fuse_id4                        0x31FA
#define MT9M114_chain_control                   0x31FC
#define MT9M114_customer_rev                    0x31FE

// SOC1 Registers
#define MT9M114_color_pipeline_control          0x3210

// SOC2 Registers
#define MT9M114_p_g1_p0q0                       0x3640
#define MT9M114_p_g1_p0q1                       0x3642
#define MT9M114_p_g1_p0q2                       0x3644
#define MT9M114_p_g1_p0q3                       0x3646
#define MT9M114_p_g1_p0q4                       0x3648
#define MT9M114_p_r_p0q0                        0x364A
#define MT9M114_p_r_p0q1                        0x364C
#define MT9M114_p_r_p0q2                        0x364E
#define MT9M114_p_r_p0q3                        0x3650
#define MT9M114_p_r_p0q4                        0x3652
#define MT9M114_p_b_p0q0                        0x3654
#define MT9M114_p_b_p0q1                        0x3656
#define MT9M114_p_b_p0q2                        0x3658
#define MT9M114_p_b_p0q3                        0x365A
#define MT9M114_p_b_p0q4                        0x365C
#define MT9M114_p_g2_p0q0                       0x365E
#define MT9M114_p_g2_p0q1                       0x3660
#define MT9M114_p_g2_p0q2                       0x3662
#define MT9M114_p_g2_p0q3                       0x3664
#define MT9M114_p_g2_p0q4                       0x3666
#define MT9M114_p_g1_p1q0                       0x3680
#define MT9M114_p_g1_p1q1                       0x3682
#define MT9M114_p_g1_p1q2                       0x3684
#define MT9M114_p_g1_p1q3                       0x3686
#define MT9M114_p_g1_p1q4                       0x3688
#define MT9M114_p_r_p1q0                        0x368A
#define MT9M114_p_r_p1q1                        0x368C
#define MT9M114_p_r_p1q2                        0x368E
#define MT9M114_p_r_p1q3                        0x3690
#define MT9M114_p_r_p1q4                        0x3692
#define MT9M114_p_b_p1q0                        0x3694
#define MT9M114_p_b_p1q1                        0x3696
#define MT9M114_p_b_p1q2                        0x3698
#define MT9M114_p_b_p1q3                        0x369A
#define MT9M114_p_b_p1q4                        0x369C
#define MT9M114_p_g2_p1q0                       0x369E
#define MT9M114_p_g2_p1q1                       0x36A0
#define MT9M114_p_g2_p1q2                       0x36A2
#define MT9M114_p_g2_p1q3                       0x36A4
#define MT9M114_p_g2_p1q4                       0x36A6
#define MT9M114_p_g1_p2q0                       0x36C0
#define MT9M114_p_g1_p2q1                       0x36C2
#define MT9M114_p_g1_p2q2                       0x36C4
#define MT9M114_p_g1_p2q3                       0x36C6
#define MT9M114_p_g1_p2q4                       0x36C8
#define MT9M114_p_r_p2q0                        0x36CA
#define MT9M114_p_r_p2q1                        0x36CC
#define MT9M114_p_r_p2q2                        0x36CE
#define MT9M114_p_r_p2q3                        0x36D0
#define MT9M114_p_r_p2q4                        0x36D2
#define MT9M114_p_b_p2q0                        0x36D4
#define MT9M114_p_b_p2q1                        0x36D6
#define MT9M114_p_b_p2q2                        0x36D8
#define MT9M114_p_b_p2q3                        0x36DA
#define MT9M114_p_b_p2q4                        0x36DC
#define MT9M114_p_g2_p2q0                       0x36DE
#define MT9M114_p_g2_p2q1                       0x36E0
#define MT9M114_p_g2_p2q2                       0x36E2
#define MT9M114_p_g2_p2q3                       0x36E4
#define MT9M114_p_g2_p2q4                       0x36E6
#define MT9M114_p_g1_p3q0                       0x3700
#define MT9M114_p_g1_p3q1                       0x3702
#define MT9M114_p_g1_p3q2                       0x3704
#define MT9M114_p_g1_p3q3                       0x3706
#define MT9M114_p_g1_p3q4                       0x3708
#define MT9M114_p_r_p3q0                        0x370A
#define MT9M114_p_r_p3q1                        0x370C
#define MT9M114_p_r_p3q2                        0x370E
#define MT9M114_p_r_p3q3                        0x3710
#define MT9M114_p_r_p3q4                        0x3712
#define MT9M114_p_b_p3q0                        0x3714
#define MT9M114_p_b_p3q1                        0x3716
#define MT9M114_p_b_p3q2                        0x3718
#define MT9M114_p_b_p3q3                        0x371A
#define MT9M114_p_b_p3q4                        0x371C
#define MT9M114_p_g2_p3q0                       0x371E
#define MT9M114_p_g2_p3q1                       0x3720
#define MT9M114_p_g2_p3q2                       0x3722
#define MT9M114_p_g2_p3q3                       0x3724
#define MT9M114_p_g2_p3q4                       0x3726
#define MT9M114_p_g1_p4q0                       0x3740
#define MT9M114_p_g1_p4q1                       0x3742
#define MT9M114_p_g1_p4q2                       0x3744
#define MT9M114_p_g1_p4q3                       0x3746
#define MT9M114_p_g1_p4q4                       0x3748
#define MT9M114_p_r_p4q0                        0x374A
#define MT9M114_p_r_p4q1                        0x374C
#define MT9M114_p_r_p4q2                        0x374E
#define MT9M114_p_r_p4q3                        0x3750
#define MT9M114_p_r_p4q4                        0x3752
#define MT9M114_p_b_p4q0                        0x3754
#define MT9M114_p_b_p4q1                        0x3756
#define MT9M114_p_b_p4q2                        0x3758
#define MT9M114_p_b_p4q3                        0x375A
#define MT9M114_p_b_p4q4                        0x375C
#define MT9M114_p_g2_p4q0                       0x375E
#define MT9M114_p_g2_p4q1                       0x3760
#define MT9M114_p_g2_p4q2                       0x3762
#define MT9M114_p_g2_p4q3                       0x3764
#define MT9M114_p_g2_p4q4                       0x3766
#define MT9M114_center_row                      0x3782
#define MT9M114_center_column                   0x3784

//SW4-L1-HL-Camera-FTM_BringUp-00+}


/* System and IO pad control [0x3000 ~ 0x3052] */
#define REG_MT9M114_SYSTEM_RESET00			0x3000
#define REG_MT9M114_SYSTEM_RESET01			0x3001
#define REG_MT9M114_SYSTEM_RESET02			0x3002
#define REG_MT9M114_SYSTEM_RESET03			0x3003
#define REG_MT9M114_CLK_ENABLE00				0x3004
#define REG_MT9M114_CLK_ENABLE01				0x3005
#define REG_MT9M114_CLK_ENABLE02				0x3006
#define REG_MT9M114_CLK_ENABLE03				0x3007
#define REG_MT9M114_SYSTEM_CONTROL0			0x3008
#define REG_MT9M114_MIPI_CONTROL				0x300E
#define REG_MT9M114_PAD_OUTPUT_ENABLE01			0x3017
#define REG_MT9M114_PAD_OUTPUT_ENABLE02			0x3018
#define REG_MT9M114_CHIP_REVISION			0x302A
#define REG_MT9M114_SC_PLL_CONTROL0			0x3034
#define REG_MT9M114_SC_PLL_CONTROL1			0x3035
#define REG_MT9M114_SC_PLL_CONTROL2			0x3036
#define REG_MT9M114_SC_PLL_CONTROL3			0x3037
#define REG_MT9M114_SC_PLL_CONTROL5			0x3039
#define REG_MT9M114_SC_PLLS_CTRL1			0x303B
#define REG_MT9M114_SC_PLLS_CTRL2			0x303C
#define REG_MT9M114_SC_PLLS_CTRL3			0x303D

/* SCCB control [0x3100 ~ 0x3108] */
#define REG_MT9M114_SCCB_SYSTEM_CTRL1			0x3103
#define REG_MT9M114_SCCB_SYSTEM_ROOT_DIVIDER		0x3108

/* AEC/AGC control [0x3500 ~ 0x350D] */
#define REG_MT9M114_AEC_PK_EXPOSURE00			0x3500
#define REG_MT9M114_AEC_PK_EXPOSURE01			0x3501
#define REG_MT9M114_AEC_PK_EXPOSURE02			0x3502
#define REG_MT9M114_AEC_PK_MANUAL			0x3503
#define REG_MT9M114_AEC_PK_REAL_GAIN_UPPER		0x350a
#define REG_MT9M114_AEC_PK_REAL_GAIN_LOWER		0x350b

/* Timing control [0x3800 ~ 0x3821] */
#define REG_MT9M114_TIMING_HS_X_ADDR_START_UPPER		0x3800
#define REG_MT9M114_TIMING_HS_X_ADDR_START_LOWER		0x3801
#define REG_MT9M114_TIMING_VS_Y_ADDR_START_UPPER		0x3802
#define REG_MT9M114_TIMING_VS_Y_ADDR_START_LOWER		0x3803
#define REG_MT9M114_TIMING_HW_X_ADDR_END_UPPER		0x3804
#define REG_MT9M114_TIMING_HW_X_ADDR_END_LOWER		0x3805
#define REG_MT9M114_TIMING_VH_Y_ADDR_END_UPPER		0x3806
#define REG_MT9M114_TIMING_VH_Y_ADDR_END_LOWER		0x3807
#define REG_MT9M114_TIMING_DVPHO_HORZ_WIDTH_UPPER	0x3808
#define REG_MT9M114_TIMING_DVPHO_HORZ_WIDTH_LOWER	0x3809
#define REG_MT9M114_TIMING_DVPO_UPPER			0x380A
#define REG_MT9M114_TIMING_DVPO_LOWER			0x380B
#define REG_MT9M114_TIMING_HTS_UPPER			0x380C
#define REG_MT9M114_TIMING_HTS_LOWER			0x380D
#define REG_MT9M114_TIMING_VTS_UPPER			0x380E
#define REG_MT9M114_TIMING_VTS_LOWER			0x380F
#define REG_MT9M114_TIMING_HOFFSET_UPPER			0x3810
#define REG_MT9M114_TIMING_HOFFSET_LOWER			0x3811
#define REG_MT9M114_TIMING_VOFFSET_UPPER			0x3812
#define REG_MT9M114_TIMING_VOFFSET_LOWER			0x3813
#define REG_MT9M114_TIMING_X_INC				0x3814
#define REG_MT9M114_TIMING_Y_INC				0x3815
#define REG_MT9M114_TIMING_REG20				0x3820
#define REG_MT9M114_TIMING_REG21				0x3821

/* DVP clk divider ctrl */
#define REG_MT9M114_REG24				0x3824

/* AEC/AGC power down domain control [0x3A00 ~ 0x3A25] */
#define REG_MT9M114_AEC_MAX_EXPO_60HZ_UPPER		0x3a02
#define REG_MT9M114_AEC_MAX_EXPO_60HZ_LOWER		0x3a03
#define REG_MT9M114_AEC_B50_STEP_UPPER			0x3a08
#define REG_MT9M114_AEC_B50_STEP_LOWER			0x3a09
#define REG_MT9M114_AEC_B60_STEP_UPPER			0x3a0a
#define REG_MT9M114_AEC_B60_STEP_LOWER			0x3a0b
#define REG_MT9M114_AEC_CTRL0D				0x3a0d
#define REG_MT9M114_AEC_CTRL0E				0x3a0e
#define REG_MT9M114_AEC_CTRL0F				0x3a0f
#define REG_MT9M114_AEC_CTRL10				0x3a10
#define REG_MT9M114_AEC_CTRL11				0x3a11
#define REG_MT9M114_AEC_MAX_EXPO_50HZ_UPPER		0x3a14
#define REG_MT9M114_AEC_MAX_EXPO_50HZ_LOWER		0x3a15
#define REG_MT9M114_AEC_GAIN_CEILING_UPPER		0x3a18
#define REG_MT9M114_AEC_GAIN_CEILING_LOWER		0x3a19
#define REG_MT9M114_AEC_CTRL1B				0x3a1b
#define REG_MT9M114_AEC_CTRL1E				0x3a1e
#define REG_MT9M114_AEC_CTRL1F				0x3a1f

/* BLC control [0x4000 ~ 0x4033] */
#define REG_MT9M114_BLC_CTRL00				0x4000
#define REG_MT9M114_BLC_CTRL01				0x4001
#define REG_MT9M114_BLC_CTRL04				0x4004

/* Format control [0x4300 ~ 0x430D] */
#define REG_MT9M114_FORMAT_CONTROL00			0x4300
#define REG_MT9M114_FORMAT_CONTROL01			0x4301

/* VFIFO control [0x4600 ~ 0x460D] */
#define REG_MT9M114_VFIFO_CTRL0C				0x460C
#define REG_MT9M114_VFIFO_CTRL0D				0x460D

/* DVP control [0x4709 ~ 0x4745] */
#define REG_MT9M114_JPG_MODE_SELECT			0x4713

/* MIPI control [0x4800 ~ 0x4837] */
#define REG_MT9M114_MIPI_CTRL_00				0x4800

/* ISP top control [0x5000 ~ 0x5063] */
#define REG_MT9M114_ISP_CONTROL00			0x5000
#define REG_MT9M114_ISP_CONTROL01			0x5001
#define REG_MT9M114_ISP_CONTROL03			0x5003
#define REG_MT9M114_ISP_CONTROL05			0x5005
#define REG_MT9M114_ISP_MISC_00				0x501D
#define REG_MT9M114_FORMAT_MUX_CONTROL			0x501F

/* CIP control [0x5300 ~ 0x530F] */
#define REG_MT9M114_CIP_SHARPENMT_THRESHOLD_1		0x5300
#define REG_MT9M114_CIP_SHARPENMT_THRESHOLD_2		0x5301
#define REG_MT9M114_CIP_SHARPENMT_OFFSET1		0x5302
#define REG_MT9M114_CIP_SHARPENMT_OFFSET2		0x5303
#define REG_MT9M114_CIP_DNS_THRESHOLD_1			0x5304
#define REG_MT9M114_CIP_DNS_THRESHOLD_2			0x5305
#define REG_MT9M114_CIP_DNS_OFFSET1			0x5306
#define REG_MT9M114_CIP_DNS_OFFSET2			0x5307

/* CMX control [0x5380 ~ 0x538B] */
#define REG_MT9M114_CMX1					0x5381
#define REG_MT9M114_CMX2					0x5382
#define REG_MT9M114_CMX3					0x5383
#define REG_MT9M114_CMX4					0x5384
#define REG_MT9M114_CMX5					0x5385
#define REG_MT9M114_CMX6					0x5386
#define REG_MT9M114_CMX7					0x5387
#define REG_MT9M114_CMX8					0x5388
#define REG_MT9M114_CMX9					0x5389
#define REG_MT9M114_CMXSIGN_UPPER			0x538a
#define REG_MT9M114_CMXSIGN_LOWER			0x538b

/* Gamma control [0x5480 ~ 0x5490] */
#define REG_MT9M114_GAMMA_YST00				0x5481
#define REG_MT9M114_GAMMA_YST01				0x5482
#define REG_MT9M114_GAMMA_YST02				0x5483
#define REG_MT9M114_GAMMA_YST03				0x5484
#define REG_MT9M114_GAMMA_YST04				0x5485
#define REG_MT9M114_GAMMA_YST05				0x5486
#define REG_MT9M114_GAMMA_YST06				0x5487
#define REG_MT9M114_GAMMA_YST07				0x5488
#define REG_MT9M114_GAMMA_YST08				0x5489
#define REG_MT9M114_GAMMA_YST09				0x548a
#define REG_MT9M114_GAMMA_YST0A				0x548b
#define REG_MT9M114_GAMMA_YST0B				0x548c
#define REG_MT9M114_GAMMA_YST0C				0x548d
#define REG_MT9M114_GAMMA_YST0D				0x548e
#define REG_MT9M114_GAMMA_YST0E				0x548f
#define REG_MT9M114_GAMMA_YST0F				0x5490

/* SDE control [0x5580 ~ 0x558C] */
#define REG_MT9M114_SDE_CTRL0				0x5580
#define REG_MT9M114_SDE_CTRL3				0x5583
#define REG_MT9M114_SDE_CTRL4				0x5584
#define REG_MT9M114_SDE_CTRL5				0x5585
#define REG_MT9M114_SDE_CTRL6				0x5586
#define REG_MT9M114_SDE_CTRL7				0x5587
#define REG_MT9M114_SDE_CTRL8				0x5588
#define REG_MT9M114_SDE_CTRL9				0x5589
#define REG_MT9M114_SDE_CTRL10				0x558A
#define REG_MT9M114_SDE_CTRL11				0x558B

/* AVG control [0x5680 ~ 0x56A2] */
#define REG_MT9M114_X_WINDOW_UPPER			0x5684
#define REG_MT9M114_X_WINDOW_LOWER			0x5685
#define REG_MT9M114_Y_WINDOW_UPPER			0x5686
#define REG_MT9M114_Y_WINDOW_LOWER			0x5687

/* Bitmasks */
/* System and IO pad control [0x3000 ~ 0x3052] */
#define MT9M114_SYSTEM_RESET02_JFIFO			(1 << 4)
#define MT9M114_SYSTEM_RESET02_SFIFO			(1 << 3)
#define MT9M114_SYSTEM_RESET02_JPG			(1 << 2)

#define MT9M114_SYSTEM_RESET03_MIPI			(1 << 1)
#define MT9M114_SYSTEM_RESET03_DVP			1

#define MT9M114_CLK_ENABLE02_PSRAM			(1 << 7)
#define MT9M114_CLK_ENABLE02_FMT				(1 << 6)
#define MT9M114_CLK_ENABLE02_FMTMUX			(1 << 1)
#define MT9M114_CLK_ENABLE02_AVG				1

#define MT9M114_SYSTEM_CONTROL0_SW_RESET			(1 << 7)
#define MT9M114_SYSTEM_CONTROL0_SW_PWRDN			(1 << 6)

#define MT9M114_MIPI_CONTROL_2LANES			(1 << 5)
#define MT9M114_MIPI_CONTROL_MIPI_EN			(1 << 2)

#define MT9M114_CHIP_REVISION_MASK			0xF

#define MT9M114_SC_PLL_CONTROL0_CHRGPUMP_MASK		(0x7 << 4)
#define MT9M114_SC_PLL_CONTROL0_CHRGPUMP_SHIFT		4
#define MT9M114_SC_PLL_CONTROL0_MIPI8BIT			0x8
#define MT9M114_SC_PLL_CONTROL0_MIPI10BIT		0xA

#define MT9M114_SC_PLL_CONTROL1_SYSCLKDIV_MASK		(0xF << 4)
#define MT9M114_SC_PLL_CONTROL1_SYSCLKDIV_SHIFT		4
#define MT9M114_SC_PLL_CONTROL1_MIPIPCLKDIV_MASK		0xF

#define MT9M114_SC_PLL_CONTROL3_PLLROOTDIV		(1 << 4)
#define MT9M114_SC_PLL_CONTROL3_PLLPREDIV_MASK		0xF

/* SCCB control [0x3100 ~ 0x3108] */
#define MT9M114_SCCB_SYSTEM_CTRL1_SYSINPCLK_PLL		(1 << 1)

#define MT9M114_SCCB_SYSTEM_ROOT_DIVIDER_SCLKDIV4	0x2

/* AEC/AGC control [0x3500 ~ 0x350D] */
#define MT9M114_AEC_PK_EXPOSURE00_MASK			0xF

#define MT9M114_AEC_PK_MANUAL_AGC			(1 << 1)
#define MT9M114_AEC_PK_MANUAL_AEC			1

#define MT9M114_AEC_PK_REAL_GAIN_UPPER_MASK		0x3

/* Timing control [0x3800 ~ 0x3821] */
#define MT9M114_TIMING_REG21_ISPMIRROR			(1 << 2)
#define MT9M114_TIMING_REG21_SENSORMIRROR		(1 << 1)

/* AEC/AGC power down domain control [0x3A00 ~ 0x3A25] */

#define MT9M114_AEC_B50_STEP_UPPER_MASK			0x3

#define MT9M114_AEC_B60_STEP_UPPER_MASK			0x3

#define MT9M114_AEC_CTRL0D_B60MAX_MASK			0x1F

#define MT9M114_AEC_CTRL0E_B50MAX_MASK			0x1F

#define MT9M114_AEC_MAX_EXPO_50HZ_UPPER_MASK		0xF

#define MT9M114_AEC_GAIN_CEILING_UPPER_MASK		0x3

/* BLC control [0x4000 ~ 0x4033] */
#define MT9M114_BLC_CTRL01_STARTLINE_MASK		0x1F

/* Format control [0x4300 ~ 0x430D] */
#define MT9M114_FORMAT_CONTROL00_OUTFMT_YUV422		(3 << 4)
#define MT9M114_FORMAT_CONTROL00_SEQ_YUV422_YUYV		0
#define MT9M114_FORMAT_CONTROL00_SEQ_YUV422_UYVY		2

/* VFIFO control [0x4600 ~ 0x460D] */
#define MT9M114_VFIFO_CTRL0C_JPEGDUMMYSPD_MASK		(0xF << 4)
#define MT9M114_VFIFO_CTRL0C_JPEGDUMMYSPD_SHIFT		4

/* DVP control [0x4709 ~ 0x4745] */
#define MT9M114_JPG_MODE_SELECT_MODE2			0x2

/* MIPI control [0x4800 ~ 0x4837] */
#define MT9M114_MIPI_CTRL_00_CLKGATING			(1 << 5)
#define MT9M114_MIPI_CTRL_00_LP11ONIDLE			(1 << 2)

/* ISP top control [0x5000 ~ 0x5063] */
#define MT9M114_ISP_CONTROL00_RAWGMA_EN			(1 << 5)
#define MT9M114_ISP_CONTROL00_BLACKPIXCANCEL_EN		(1 << 2)
#define MT9M114_ISP_CONTROL00_WHITEPIXCANCEL_EN		(1 << 1)
#define MT9M114_ISP_CONTROL00_COLORINTRP_EN		1

#define MT9M114_ISP_CONTROL01_SPECIALDFX_EN		(1 << 7)
#define MT9M114_ISP_CONTROL01_SCALE_EN			(1 << 5)
#define MT9M114_ISP_CONTROL01_UV_AVERAGE_EN		(1 << 2)
#define MT9M114_ISP_CONTROL01_COLORMATRIX_EN		(1 << 1)
#define MT9M114_ISP_CONTROL01_AWB_EN			1

/* SDE control [0x5580 ~ 0x558C] */
#define MT9M114_REG_MT9M114_SDE_CTRL0_FIXED_Y_EN		(1 << 7)
#define MT9M114_REG_MT9M114_SDE_CTRL0_NEGATIVE_EN		(1 << 6)
#define MT9M114_REG_MT9M114_SDE_CTRL0_GRAY_EN		(1 << 5)
#define MT9M114_REG_MT9M114_SDE_CTRL0_FIXED_V_EN		(1 << 4)
#define MT9M114_REG_MT9M114_SDE_CTRL0_FIXED_U_EN		(1 << 3)
#define MT9M114_REG_MT9M114_SDE_CTRL0_CONTRAST_EN		(1 << 2)
#define MT9M114_REG_MT9M114_SDE_CTRL0_SATURATION_EN	(1 << 1)
#define MT9M114_REG_MT9M114_SDE_CTRL0_HUE_EN		(1 << 0)

/* AVG control [0x5680 ~ 0x56A2] */
#define MT9M114_X_WINDOW_UPPER_MASK			0xF

#define MT9M114_Y_WINDOW_UPPER_MASK			0x7

#define MT9M114_BRIGHTNESS_MIN				0
#define MT9M114_BRIGHTNESS_MAX				200
#define MT9M114_BRIGHTNESS_STEP				100
#define MT9M114_BRIGHTNESS_DEF				100

#define MT9M114_CONTRAST_MIN				0
#define MT9M114_CONTRAST_MAX				200
#define MT9M114_CONTRAST_STEP				100
#define MT9M114_CONTRAST_DEF				100

#endif /* _MT9M114_REGS_H_ */
