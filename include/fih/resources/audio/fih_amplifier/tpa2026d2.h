/*
 * ALSA SoC Texas Instruments TPA2026D2 Speaker amplifier driver
 *
 * Copyright (C) 2011 Foxconn, INC.
 *
 * Derived from kernel/sound/soc/codecs/tpa2026d2.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 * Author:  AlvinLi <alvinli@fih-foxconn.com>
 *	    October 2011
 */

#ifndef __TPA2026D2_H__
#define __TPA2026D2_H__

/* Register addresses */
#define TPA2026D2_REG_IC_FUNCTION_CONTROL		0x01
#define TPA2026D2_REG_AGC_ATTACK_CONTROL		0x02
#define TPA2026D2_REG_AGC_RELEASE_CONTROL		0x03
#define TPA2026D2_REG_AGC_HOLD_TIME_CONTROL		0x04
#define TPA2026D2_REG_AGC_FIXED_GAIN_CONTROL	0x05
#define TPA2026D2_REG_AGC_CONTROL_1				0x06
#define TPA2026D2_REG_AGC_CONTROL_2				0x07

#define TPA2026D2_CACHEREGNUM					(TPA2026D2_REG_AGC_CONTROL_2 + 1)

/* Register bits */
/* TPA2026D2_REG_IC_FUNCTION_CONTROL (0x01) */
#define TPA2026D2_NG_EN							(0x01 << 0)
#define TPA2026D2_THERMAL						(0x01 << 2)
#define TPA2026D2_FAULT_L						(0x01 << 3)
#define TPA2026D2_FAULT_R						(0x01 << 4)
#define TPA2026D2_SWS							(0x01 << 5)
#define TPA2026D2_SPK_EN_L						(0x01 << 6)
#define TPA2026D2_SPK_EN_R						(0x01 << 7)

/* TPA2026D2_REG_AGC_ATTACK_CONTROL (0x02) */
#define TPA2026D2_ATK_TIME(x)					((x & 0x3f) << 0)

/* TPA2026D2_REG_AGC_RELEASE_CONTROL (0x03) */
#define TPA2026D2_REL_TIME(x)					((x & 0x3f) << 0)

/* TPA2026D2_REG_AGC_HOLD_TIME_CONTROL (0x04) */
#define TPA2026D2_HOLD_TIME(x)					((x & 0x3f) << 0)

/* TPA2026D2_REG_AGC_FIXED_GAIN_CONTROL (0x05) */
#define TPA2026D2_FIXED_GAIN(x)					((x & 0x3f) << 0)

/* TPA2026D2_REG_AGC_CONTROL_1 (0x06) */
#define TPA2026D2_OUTPUT_LIMITER_LEVEL(x)		((x & 0x1f) << 0)
#define TPA2026D2_NOISE_GATE_THRESHOLD(x)		((x & 0x03) << 5)
#define TPA2026D2_OUTPUT_LIMITER_DISABLE		(0x01 << 7)

/* TPA2026D2_REG_AGC_CONTROL_2 (0x07) */
#define TPA2026D2_COMPRESSION_RATIO(x)			((x & 0x03) << 0)
#define TPA2026D2_MAX_GAIN(x)					((x & 0x0f) << 4)

extern int tpa2026d2_stereo_enable(struct snd_soc_codec *codec, int enable);

#endif /* __TPA2026D2_H__ */
