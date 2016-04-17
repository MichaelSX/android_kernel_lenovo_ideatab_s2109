/*
 * SMB347 I2C Charger IC
 *
 * Copyright (C) 2011, PinyCHWu <pinychwu@fih-foxconn.com>
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 */

#ifndef __smb347_charger_h__
#define __smb347_charger_h__

#include <linux/workqueue.h>
#include <linux/power_supply.h>
#include <linux/i2c.h>

/* SUMMIT SMB347 REGISTER */
//config register
#define SMB347_CHG_CUR		(0x00)
#define SMB347_INPUT_CUR	(0x01)
#define SMB347_VAR_FUNC		(0x02)
#define SMB347_FLOAT_VOL	(0x03)
#define SMB347_CHG_CNTRL	(0x04)
#define SMB347_STAT_CNTRL	(0x05)
#define SMB347_PIN_CNTRL	(0x06)
#define SMB347_THERM_CNTRL	(0x07)
#define SMB347_SYSOK_SEL	(0x08)
#define SMB347_OTHER_CNTRL	(0x09)
#define SMB347_OTG_CNTRL	(0x0A)
#define SMB347_CELL_TEMP	(0x0B)
#define SMB347_FAULT_INT	(0x0C)
#define SMB347_STATUS_INT	(0x0D)
#define SMB347_I2C_ADDR		(0x0E)
//command register
#define SMB347_CMD_A		(0x30)
#define SMB347_CMD_B		(0x31)
#define SMB347_RSRV1		(0x32)
#define SMB347_CMD_C		(0x33)
#define SMB347_RSRV2		(0x34)
//status & interrupt statust register
#define SMB347_INTS_A		(0x35)
#define SMB347_INTS_B		(0x36)
#define SMB347_INTS_C		(0x37)
#define SMB347_INTS_D		(0x38)
#define SMB347_INTS_E		(0x39)
#define SMB347_INTS_F		(0x3A)
#define SMB347_STS_A		(0x3B)
#define SMB347_STS_B		(0x3C)
#define SMB347_STS_C		(0x3D)
#define SMB347_STS_D		(0x3E)
#define SMB347_STS_E		(0x3F)

//local interrupt bits
#define SMB347_IRQ_HTEMP_H	(1 << 0)
#define SMB347_IRQ_LTEMP_H	(1 << 1)
#define SMB347_IRQ_HTEMP_S	(1 << 2)
#define SMB347_IRQ_LTEMP_S	(1 << 3)
#define SMB347_IRQ_BAT_OV	(1 << 4)
#define SMB347_IRQ_MISS_BAT	(1 << 5)
#define SMB347_IRQ_LOW_BAT	(1 << 6)
#define SMB347_IRQ_PTOF_CHG	(1 << 7)
#define SMB347_IRQ_INT_TEMP	(1 << 8)
#define SMB347_IRQ_RECHG	(1 << 9)
#define SMB347_IRQ_TAPER_CHG	(1 << 10)
#define SMB347_IRQ_TERM_CHG	(1 << 11)
#define SMB347_IRQ_APSD_COMP	(1 << 12)
#define SMB347_IRQ_AICL_COMP	(1 << 13)
#define SMB347_IRQ_CHG_TIMEOUT	(1 << 14)
#define SMB347_IRQ_PCHG_TIMEOUT	(1 << 15)
#define SMB347_IRQ_DCIN_OV	(1 << 16)
#define SMB347_IRQ_DCIN_UV	(1 << 17)
#define SMB347_IRQ_USBIN_OV	(1 << 18)
#define SMB347_IRQ_USBIN_UV	(1 << 19)
#define SMB347_IRQ_OTG_OC	(1 << 20)
#define SMB347_IRQ_OTG_UV	(1 << 21)
#define SMB347_IRQ_OTG_DET	(1 << 22)
#define SMB347_IRQ_POWER_OK	(1 << 23)

struct smb347_reg {
	unsigned char reg;
	unsigned char val;
};

#define SMB347_STAT_GPIO_MASK	(1 << 0)
#define SMB347_INOK_GPIO_MASK	(1 << 1)
#define SMB347_SUSP_GPIO_MASK	(1 << 2)
#define SMB347_USB_GPIO_MASK	(1 << 3)
struct smb347_platform_data {
	int			gpio_map;
	int			stat_gpio;
	int			inok_gpio;
	int			susp_gpio;
	int			usb_gpio;
	struct smb347_reg	*init_array;
	int			init_array_size;
};

struct smb347_sts_bits {
  	//byte 1
	unsigned act_float_vol	:6;
	unsigned therm_regu	:1;
	unsigned temp_regu	:1;
	//byte 2
	unsigned act_chg_cur	:6;
	unsigned reserved1	:1;
	unsigned usb_suspend	:1;
	//byte 3
	unsigned chg_enable	:1;
	unsigned chging_sts	:2;
	unsigned hold_off_sts	:1;
	unsigned batt_vol_level	:1;
	unsigned chg_sts	:1;
	unsigned chger_err	:1;
	unsigned chger_err_irq	:1;
	//byte 4
	unsigned apsd_result	:3;
	unsigned apsd_sts	:1;
	unsigned aca_sts	:3;
	unsigned rid_sts	:1;
	//byte 5
	unsigned aicl_result	:4;
	unsigned aicl_sts	:1;
	unsigned usb_mode	:2;
	unsigned usbin_input	:1;
};

struct smb347_property {
	int			status;
	int			online;
};

struct smb347_chip {
	//i2c
	struct device 		*dev;
	struct mutex		read_lock;
	struct mutex		charger_lock;
	struct i2c_client	*client;

	//power supply
	struct power_supply	ac;
	struct power_supply	usb;
	struct smb347_property	ac_prop;
	struct smb347_property	usb_prop;

	//workqueue
	struct delayed_work	monitor_work;
	struct wake_lock	work_wakelock;

	//status
	unsigned int		intsts_bits;
	union {
		struct smb347_sts_bits	sts_bits;
		unsigned char		sts_bytes[5];
	} status;

	//irq
	int			irq_gpio_map;
	int			stat_irq;
	int			inok_irq;
	int			susp_gpio;
	int			usb_gpio;

	//setting
	int			nonvol_write;
};

#endif /* !__smb347_charger_h__ */
