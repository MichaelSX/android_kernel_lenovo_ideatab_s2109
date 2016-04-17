/*
 * Driver for charger with SUMMIT SMB347 chips inside.
 *
 * Copyright (C) 2011 Foxconn, INC.
 *
 * Derived from kernel/drivers/power/smb347_charger.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Author:  PinyCHWu <pinychwu@fih-foxconn.com>
 *	    September 2011
 */

#include <linux/module.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/pm.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/interrupt.h>
#include <linux/wakelock.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/skbuff.h>
#include <linux/miscdevice.h>
#include <linux/device.h>
#include <linux/sysdev.h>
#include <mach/gpio.h>
#include <fih/resources/charger/smb347_charger.h>
#include <linux/fih_hw_info.h>

#ifdef CONFIG_BATTERY_BQ27520
#include <fih/resources/battery/bq27520_battery.h>
#endif

static struct smb347_chip *g_smb_chip = NULL;
static int product_id = 0;
static int phase_id = 0;
static int ftm_mode = 0;

static char reg_permission_table[] = {
  	//command index		{read-only:0/nonvol-write:1/vol-write:2}
	[SMB347_CHG_CUR]	= 1,
	[SMB347_INPUT_CUR]	= 1,
	[SMB347_VAR_FUNC]	= 1,
	[SMB347_FLOAT_VOL]	= 1,
	[SMB347_CHG_CNTRL]	= 1,
	[SMB347_STAT_CNTRL]	= 1,
	[SMB347_PIN_CNTRL]	= 1,
	[SMB347_THERM_CNTRL]	= 1,
	[SMB347_SYSOK_SEL]	= 1,
	[SMB347_OTHER_CNTRL]	= 1,
	[SMB347_OTG_CNTRL]	= 1,
	[SMB347_CELL_TEMP]	= 1,
	[SMB347_FAULT_INT]	= 1,
	[SMB347_STATUS_INT]	= 1,
	[SMB347_I2C_ADDR]	= 0,
	[SMB347_CMD_A]		= 2,
	[SMB347_CMD_B]		= 2,
	[SMB347_RSRV1]		= 0,
	[SMB347_CMD_C]		= 0,
	[SMB347_RSRV2]		= 0,
	[SMB347_INTS_A]		= 0,
	[SMB347_INTS_B]		= 0,
	[SMB347_INTS_C]		= 0,
	[SMB347_INTS_D]		= 0,
	[SMB347_INTS_E]		= 0,
	[SMB347_INTS_F]		= 0,
	[SMB347_STS_A]		= 0,
	[SMB347_STS_B]		= 0,
	[SMB347_STS_C]		= 0,
	[SMB347_STS_D]		= 0,
	[SMB347_STS_E]		= 0,
};
/* ---------------------- Charger access functions ------------------------- */
static int smb347_i2c_read(struct smb347_chip *chip, unsigned char reg, unsigned char *data)
{
	int ret = 0;

	mutex_lock(&chip->read_lock);
	ret = i2c_smbus_read_byte_data(chip->client, reg);
	if (ret < 0) {
		pr_err("[%s] read i2c failed (%d)\n", __func__, ret);
		mutex_unlock(&chip->read_lock);
		return ret;
	}

	*data = (unsigned char) (ret & 0xFF);
	mutex_unlock(&chip->read_lock);
	return 0;
}

static int smb347_i2c_write(struct smb347_chip *chip, unsigned char reg, unsigned char data)
{
	int ret = 0;

	mutex_lock(&chip->read_lock);
	ret = i2c_smbus_write_byte_data(chip->client, reg, data);
	if (ret < 0) {
		pr_err("[%s] write i2c failed (%d)\n", __func__, ret);
	}

	mutex_unlock(&chip->read_lock);
	return ret;
}

static int smb347_i2c_read_block(struct smb347_chip *chip, unsigned char reg, unsigned char len, unsigned char *data)
{
	int ret = 0;

	mutex_lock(&chip->read_lock);
	ret = i2c_smbus_read_i2c_block_data(chip->client, reg, len, data);
	if (ret < 0) {
		pr_err("[%s] read i2c failed (%d)\n", __func__, ret);
		mutex_unlock(&chip->read_lock);
		return ret;
	}
	if (ret < len)
		pr_info("[%s] only read %d/%d bytes\n", __func__, ret, len);
	mutex_unlock(&chip->read_lock);
	return ret;
}

static int check_reg_permission(unsigned char reg)
{
	return (int)reg_permission_table[reg];
}

static int enable_nonvolatile_write(struct smb347_chip *chip, int enable)
{
	int ret = 0;
	unsigned char data = 0;
	
	ret = smb347_i2c_read(chip, SMB347_CMD_A, &data);
	if (ret) {
		pr_err("[%s] read failed\n", __func__);
		return -1;
	}

	if (enable)
		data |= (1 << 7);
	else
		data &= ~(1 << 7);

	ret = smb347_i2c_write(chip, SMB347_CMD_A, data);
	if (ret) {
		pr_err("[%s] write failed\n", __func__);
		return -1;
	}

	if (enable)
		chip->nonvol_write = 1;
	else
		chip->nonvol_write = 0;

	return 0;
}

static int get_nonvol_write_status(struct smb347_chip *chip)
{
	int ret = 0;
	unsigned char data = 0;
	
	ret = smb347_i2c_read(chip, SMB347_CMD_A, &data);
	if (ret) {
		pr_err("[%s] read failed\n", __func__);
		return 0;
	}

	if (data & (1 << 7))
		return 1;
	else
		return 0;
}

static int smb347_alive(struct smb347_chip *chip)
{
	int ret = 0;
	unsigned char data = 0;
	unsigned short addr = chip->client->addr;
	
	ret = smb347_i2c_read(chip, SMB347_I2C_ADDR, &data);
	if (ret < 0)
		return -1;

	data >>= 1;
	if (data != addr)
		return -1;

	return 0;
}

static void smb347_reg_setting(struct smb347_chip *chip)
{
	struct smb347_platform_data *pdata = chip->dev->platform_data;
	struct smb347_reg *reg_array = pdata->init_array;
	int i, ret = 0;

	if (pdata->init_array_size > 0)
		enable_nonvolatile_write(chip, 1);

	for (i = 0; i < pdata->init_array_size; i++)
	{
		if (!check_reg_permission(reg_array[i].reg)) {
			pr_info("[%s] reg(0x%02X) is read-only\n", __func__, reg_array[i].reg);
			continue;
		}

		if (ftm_mode && (reg_array[i].reg == SMB347_PIN_CNTRL)) { //in ftm mode, skip enable function 
			reg_array[i].val &= ~(7 << 4);
		}

		ret = smb347_i2c_write(chip, reg_array[i].reg, reg_array[i].val);
		if (ret < 0) {
			pr_err("[%s] init reg(0x%02X) failed\n", __func__, reg_array[i].reg);
		}
		pr_info("[%s] init reg(0x%02X)=0x%02X\n", __func__, reg_array[i].reg, reg_array[i].val);
	}

	if (pdata->init_array_size > 0)
		enable_nonvolatile_write(chip, 0);
}

static void smb347_reg_check(struct smb347_chip *chip)
{
	int i = 0, ret = 0;
	unsigned char value;

	for (i = SMB347_CHG_CUR; i <= SMB347_I2C_ADDR; i++) {
		ret = smb347_i2c_read(chip, i, &value);
		if (ret < 0) {
			pr_info("[%s] get reg (0x%02X) failed\n", __func__, i);
		} else {
			pr_info("[%s] reg (0x%02X) = 0x%02X\n", __func__, i, value);
		}
	}

	for (i = SMB347_CMD_A; i <= SMB347_RSRV2; i++) {
		ret = smb347_i2c_read(chip, i, &value);
		if (ret < 0) {
			pr_info("[%s] get reg (0x%02X) failed\n", __func__, i);
		} else {
			pr_info("[%s] reg (0x%02X) = 0x%02X\n", __func__, i, value);
		}
	}
}

static int smb347_get_stat_status(struct smb347_chip *chip)
{
	int value = 0;

	//get gpio value
	value = gpio_get_value(chip->stat_gpio);
	printk(KERN_DEBUG "[%s] stat_gpio is %s\n", __func__, (value?"high":"low"));

	//decide charger status
	if (chip->stat_mode == SMB347_IND_CHG_MODE) {
		if (chip->stat_pol != value) { //active, omap is reversed to summit
			chip->charging_status = 1;
		} else {
			chip->charging_status = 0;
		}
	} else if (chip->stat_mode == SMB347_USB_FAIL_MODE) {
		/* TBD */
	  	;
	} else {
		pr_info("[%s] Unknow mode(%d)\n", __func__, chip->stat_mode);
	}

	return value;
}

static int smb347_get_inok_status(struct smb347_chip *chip)
{
	int value = 0;

	//get gpio value
	value = gpio_get_value(chip->inok_gpio);
	printk(KERN_DEBUG "[%s] inok_gpio is %s\n", __func__, (value?"high":"low"));

	//decide charger status
	if (chip->inok_mode == SMB347_INOK_MODE) {
		if (chip->inok_pol == value) { //active
			wake_lock(&chip->charge_wakelock);
			chip->charger_status = 1;
		} else {
			chip->charger_status = 0;
			wake_unlock(&chip->charge_wakelock);
		}
	} else if (chip->inok_mode == SMB347_SYSOK_MODE_A) {
		if (value == 1) {
			//Battery lower than Vlowbatt, Input and battery present
			/* TBD */
		} else {
			//others
			/* TBD */
		}
	} else if (chip->inok_mode == SMB347_SYSOK_MODE_B) {
		/* TBD */
	} else if (chip->inok_mode == SMB347_CHG_DET_MODE) {
		if (value == 0) { //charger detect
			//check APSD results
			/* TBD */
		} else {
			//unknow charger or not
			/* TBD */
		}
	}

	return value;
}

/* ---------------------- Global functions ------------------------- */
int smb347_set_susp_mode(int value)
{
	int ret = 0;
	int susp_mode = 0;
	unsigned char data = 0, old_data = 0;

	if (g_smb_chip == NULL) {
		pr_err("[%s] chip data is not ready\n", __func__);
		return -1;
	}

	ret = smb347_i2c_read(g_smb_chip, SMB347_VAR_FUNC, &data);
	if (ret < 0) {
		pr_err("[%s] Get susp mode failed: %d\n", __func__, ret);
		return -1;
	}
	
	if (data & (1 << 7))
		susp_mode = SMB347_SUSP_REG_MODE;
	else
		susp_mode = SMB347_SUSP_PIN_MODE;

	if (susp_mode == SMB347_SUSP_PIN_MODE) {
		if (g_smb_chip->irq_gpio_map & SMB347_SUSP_GPIO_MASK) {
			gpio_set_value(g_smb_chip->susp_gpio, !!value);
		} else {
			pr_err("[%s] no susp pin define\n", __func__);
			return -1;
		}
	} else if (susp_mode == SMB347_SUSP_REG_MODE) {
		data = 0;
		ret = smb347_i2c_read(g_smb_chip, SMB347_CMD_A, &data);
		if (ret < 0) {
			pr_err("[%s] get current susp mode failed\n", __func__);
			return -1;
		}
		old_data = data;
		if (value) {
			data |= (1 << 2);
		} else {
			data &= ~(1 << 2);
		}
		if (data != old_data) {
			ret = smb347_i2c_write(g_smb_chip, SMB347_CMD_A, data);
			if (ret < 0) {
				pr_err("[%s] set current susp mode %d failed\n", __func__, value);
				return -1;
			} 
		}
	}

	return 0;
}
EXPORT_SYMBOL(smb347_set_susp_mode);

int smb347_set_usb_mode(int value)
{
	int ret = 0;
	int usb_mode = 0;
	unsigned char data = 0, old_data = 0;

	if (g_smb_chip == NULL) {
		pr_err("[%s] chip data is not ready\n", __func__);
		return -1;
	}

	ret = smb347_i2c_read(g_smb_chip, SMB347_PIN_CNTRL, &data);
	if (ret < 0) {
		pr_err("[%s] Get usb mode failed: %d\n", __func__, ret);
		return -1;
	}
	
	if (data & (1 << 4))
		usb_mode = SMB347_USB_PIN_MODE;
	else
		usb_mode = SMB347_USB_REG_MODE;

	if (usb_mode == SMB347_SUSP_PIN_MODE) {
		if (g_smb_chip->irq_gpio_map & SMB347_USB_GPIO_MASK) {
			gpio_set_value(g_smb_chip->usb_gpio, !!value);
		} else {
			pr_err("[%s] no usb pin define\n", __func__);
			return -1;
		}
	} else if (usb_mode == SMB347_USB_REG_MODE) {
		data = 0;
		ret = smb347_i2c_read(g_smb_chip, SMB347_CMD_B, &data);
		if (ret < 0) {
			pr_err("[%s] get current usb mode failed\n", __func__);
			return -1;
		}
		old_data = data;
		if (value) {
			data |= (1 << 1);
		} else {
			data &= ~(1 << 1);
		}
		if (data != old_data) {
			ret = smb347_i2c_write(g_smb_chip, SMB347_CMD_B, data);
			if (ret < 0) {
				pr_err("[%s] set current usb mode %d failed\n", __func__, value);
				return -1;
			} 
		}
	}

	return 0;
}
EXPORT_SYMBOL(smb347_set_usb_mode);

int smb347_set_booster_gpio(int value)
{
	if (g_smb_chip == NULL) {
		pr_err("[%s] chip data is not ready\n", __func__);
		return -1;
	}

	if (g_smb_chip->irq_gpio_map & SMB347_BSTR_GPIO_MASK) {
		gpio_set_value(g_smb_chip->bstr_gpio, !!value);
	} else {
		pr_err("[%s] no booster pin define\n", __func__);
		return -1;
	}

	return 0;
}
EXPORT_SYMBOL(smb347_set_booster_gpio);

/* ---------------------- Power supply functions ------------------------- */
static int smb347_ac_get_prop(struct power_supply *psy,
			       enum power_supply_property psp,
			       union power_supply_propval *val)
{
	struct smb347_chip *chip = container_of(psy, struct smb347_chip, ac);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = chip->ac_prop.status;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = chip->ac_prop.online;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static enum power_supply_property smb347_ac_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_ONLINE,
};

static int smb347_usb_get_prop(struct power_supply *psy,
			       enum power_supply_property psp,
			       union power_supply_propval *val)
{
	struct smb347_chip *chip = container_of(psy, struct smb347_chip, usb);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = chip->usb_prop.status;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = chip->usb_prop.online;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static enum power_supply_property smb347_usb_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_ONLINE,
};

/* ----------------------------- Sys/Misc device functions ----------------------------- */
static int smb347_init_ftm_chg(struct smb347_chip *chip, int enable)
{
	static int first_run = 0;
	static unsigned char org_en = 0;	//enable method
	static unsigned char org_aicl = 0;	//aicl method
	static unsigned char org_inlim = 0;	//input limit
	static unsigned char org_chglim = 0;	//charge limit
	static unsigned char org_nc = 0;	//NC charge setting
	unsigned char en = 0;
	unsigned char aicl = 0;
	unsigned char nc = 0;

	int ret = 0;

	if (!chip->nonvol_write) {
		pr_info("[%s] not enable non-volatile write\n", __func__);
		return -1;
	}
	
	ret = smb347_i2c_read(chip, SMB347_PIN_CNTRL, &en);
	if (ret < 0)
		return -1;
	ret = smb347_i2c_read(chip, SMB347_VAR_FUNC, &aicl);
	if (ret < 0)
		return -1;
	ret = smb347_i2c_read(chip, SMB347_STAT_CNTRL, &nc);
	if (ret < 0)
		return -1;

	if (!first_run) {
		org_en = en;
		org_aicl = aicl;
		org_nc = nc;
		ret = smb347_i2c_read(chip, SMB347_INPUT_CUR, &org_inlim);
		if (ret < 0)
			return -1;
		ret = smb347_i2c_read(chip, SMB347_CHG_CUR, &org_chglim);
		if (ret < 0)
			return -1;
		first_run = 1;
	}

	if (enable) {
		en &= ~(7 << 4); //command control, 0 is disable
		aicl &= ~(1 << 4); //disable aicl for ftm input limit
		nc |= (1 << 4); //set NC charging setting as HC limit
	} else {
		en = org_en;
		aicl = org_aicl;
		nc = org_nc;
		ret = smb347_i2c_write(chip, SMB347_INPUT_CUR, org_inlim);
		if (ret < 0)
			return -1;
		ret = smb347_i2c_write(chip, SMB347_CHG_CUR, org_chglim);
		if (ret < 0)
			return -1;
	}

	ret = smb347_i2c_write(chip, SMB347_PIN_CNTRL, en);
	if (ret < 0)
		return -1;
	ret = smb347_i2c_write(chip, SMB347_VAR_FUNC, aicl);
	if (ret < 0)
		return -1;
	ret = smb347_i2c_write(chip, SMB347_STAT_CNTRL, nc);
	if (ret < 0)
		return -1;

	return 0;
}

static int smb347_enable_charging(struct smb347_chip *chip, int enable)
{
	int ret = 0;
	unsigned char data = 0;

	ret = smb347_i2c_read(chip, SMB347_CMD_A, &data);
	if (ret < 0)
		return -1;

	if (enable)
		data |= (1 << 1);
	else
		data &= ~(1 << 1);

	ret = smb347_i2c_write(chip, SMB347_CMD_A, data);
	if (ret < 0)
		return -1;

	ret = smb347_i2c_read(chip, SMB347_CMD_B, &data);
	if (ret < 0)
		return -1;

	if (enable)
		data |= 3;
	else
		data &= ~3;

	ret = smb347_i2c_write(chip, SMB347_CMD_B, data);
	if (ret < 0)
		return -1;

	return 0;
}

static int input_current_map[10] = {
	300, 500, 700, 900, 1200,
	1500, 1800, 2000, 2200, 2500
};
static int smb347_set_input_current(struct smb347_chip *chip, int cur)
{
	int ret = 0;
	int current_index = 0;
	unsigned char data = 0;

	if (!chip->nonvol_write) {
		pr_info("[%s] not enable non-volatile write\n", __func__);
		return -1;
	}

	for (; current_index < ARRAY_SIZE(input_current_map); current_index++) {
		if (cur <= input_current_map[current_index])
			break;
	}
	
	if (current_index >= ARRAY_SIZE(input_current_map))
		current_index = ARRAY_SIZE(input_current_map) - 1; //last one item

	data = (current_index << 4) | current_index;

	ret = smb347_i2c_write(chip, SMB347_INPUT_CUR, data);
	if (ret < 0)
		return -1;

	return current_index;
}

static int fast_current_map[8] = {
	700, 900, 1200, 1500,
	1800, 2000, 2200, 2500
};

static int smb347_set_fast_current(struct smb347_chip *chip, int cur)
{
	int ret = 0;
	int current_index = 0;
	unsigned char data = 0;

	if (!chip->nonvol_write) {
		pr_info("[%s] not enable non-volatile write\n", __func__);
		return -1;
	}

	for (; current_index < ARRAY_SIZE(fast_current_map); current_index++) {
		if (cur <= fast_current_map[current_index])
			break;
	}
	
	if (current_index >= ARRAY_SIZE(fast_current_map))
		current_index = ARRAY_SIZE(fast_current_map) - 1; //last one item

	ret = smb347_i2c_read(chip, SMB347_CHG_CUR, &data);
	if (ret < 0)
		return -1;

	data &= ~(7 << 5);
	data |= ((unsigned char)current_index << 5);

	ret = smb347_i2c_write(chip, SMB347_CHG_CUR, data);
	if (ret < 0)
		return -1;

	return current_index;
}

static ssize_t charger_write(struct file *file, const char __user * buf,
			     size_t count, loff_t * ppos)
{
  	char temp[256];
	int value = 0, ret = 0;

	if (g_smb_chip == NULL) {
		pr_err("[%s] chip data is not ready\n", __func__);
		return 0;
	}

	memset(temp, 0, sizeof(temp));
	if (copy_from_user(temp, buf, sizeof(temp))) {
	  	pr_err("[%s] copy_from_user failed\n", __func__);
	  	return -EFAULT;
	}

	mutex_lock(&g_smb_chip->charger_lock);
	g_smb_chip->nonvol_write = get_nonvol_write_status(g_smb_chip);
	if ('%' == temp[0]) { //init ftm charge command
		if ('1' == temp[1] && !g_smb_chip->nonvol_write) {
			/* Disable original charging mechanism */
			ret = enable_nonvolatile_write(g_smb_chip, 1);
			if (ret < 0) {
				mutex_unlock(&g_smb_chip->charger_lock);
				return -1;
			}

			ret = smb347_init_ftm_chg(g_smb_chip, 1);
			if (ret < 0) {
				pr_err("[%s] enable comand chg failed\n", __func__);
				count = -1;
			} else {
				pr_info("[%s] Disable charger fsm mechanism\n", __func__);
			}
			ftm_mode = 1;
		} else if ('0' == temp[1] && g_smb_chip->nonvol_write) {
			/* Enable original charging mechanism */
			ret = smb347_init_ftm_chg(g_smb_chip, 0);
			if (ret < 0) {
				pr_err("[%s] disable comand chg failed\n", __func__);
				count = -1;
			} else {
				pr_info("[%s] Enable charger fsm mechanism\n", __func__);
			}
			ret = enable_nonvolatile_write(g_smb_chip, 0);
			if (ret < 0) {
				mutex_unlock(&g_smb_chip->charger_lock);
				return -1;
			}
			ftm_mode = 0;
		}
	} else if ('@' == temp[0]) { //set current command
		value = simple_strtoul((temp+1), NULL, 10);
		ret = smb347_set_fast_current(g_smb_chip, value);
		if (ret < 0) {
			pr_err("[%s] set fast current failed\n", __func__);
		} else {
			pr_info("[%s] set fast current %d, real current %d\n", __func__, value, fast_current_map[ret]);
		}
		ret = smb347_set_input_current(g_smb_chip, value);
		if (ret < 0) {
			pr_err("[%s] set input current failed\n", __func__);
		} else {
			pr_info("[%s] set input current %d, real current %d\n", __func__, value, input_current_map[ret]);
		}
	} else if ('+' == temp[0]) { //turn on/off charging command
	  	if ('1' == temp[1]) {
  			/* Turn on fast charge mode for set some parameter */
			ret = smb347_enable_charging(g_smb_chip, 1);
			if (ret < 0) {
				count = -1;
				pr_err("[%s] Enable charging failed\n", __func__);
			} else {
				pr_info("[%s] Turn on the fast charging...\n", __func__);
			}
		} else if ('0' == temp[1]) {
			/* Turn off charge mode */
			ret = smb347_enable_charging(g_smb_chip, 0);
			if (ret < 0) {
				count = -1;
				pr_err("[%s] Disable charging failed\n", __func__);
			} else {
				pr_info("[%s] Turn off the fast charging...\n", __func__);
			}
		}
	}
	mutex_unlock(&g_smb_chip->charger_lock);
	return count;
}

static struct file_operations charger_fops = {
	.owner = THIS_MODULE,
	.write = charger_write,
};

static struct miscdevice charger_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ftm_charger",
	.fops = &charger_fops,
};

static unsigned char g_reg = 0;
static ssize_t smb347_reg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	int ret = 0;

	if (g_smb_chip == NULL) {
		pr_err("[%s] chip data is not ready\n", __func__);
		return 0;
	}

	ret = smb347_i2c_read(g_smb_chip, g_reg, &data);
	if (ret < 0) {
		pr_err("[%s] read reg(0x%02X) failed\n", __func__, g_reg);
		return -EINVAL;
	}

	return sprintf(buf, "reg(0x%02X)=0x%02X\n", g_reg, data);
}

static ssize_t smb347_reg_store(struct device *dev, 
	struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int reg = 0, value = 0;
	char cmd = 0;

	if (g_smb_chip == NULL) {
		pr_err("[%s] chip data is not ready\n", __func__);
		return 0;
	}

	if (sscanf(buf, "%c %x %x", &cmd, &reg, &value) <= 0) {
		pr_err("[%s] get user-space data failed\n", __func__);
		return -EINVAL;
	}

	if ((reg < SMB347_CHG_CUR) || ((reg > SMB347_I2C_ADDR) && (reg < SMB347_CMD_A)) || (reg > SMB347_STS_E)) {
		pr_err("[%s] reg(0x%02X) is out of range\n", __func__, reg);
		return -EINVAL;
	}

	if (cmd == 'r' || cmd == 'R') { //read
		g_reg = (unsigned char)reg;
	} else if (cmd == 'w' || cmd == 'W') { //write
		int enable = 0, ret = 0;
		
		enable = check_reg_permission((unsigned char)reg);
		if (enable == 0) {
			pr_err("[%s] This reg(0x%02X) is read-only\n", __func__, (unsigned char)reg);
			return -EINVAL;
		} else if (enable == 1) {
			ret = enable_nonvolatile_write(g_smb_chip, 1);
			if (ret)
				return -EINVAL;
		}

		ret = smb347_i2c_write(g_smb_chip, (unsigned char)reg, (unsigned char)value);

		if (enable == 1) {
			ret = enable_nonvolatile_write(g_smb_chip, 0);
			if (ret)
				return -EINVAL;
		}

		if (ret < 0) {
			pr_err("[%s] write reg(0x%02X) failed\n", __func__, (unsigned char)reg);
			return -EINVAL;
		}
		pr_info("[%s] write reg(0x%02X)=0x%02X done\n", __func__, (unsigned char)reg, value);
	} else if (cmd == 's' || cmd == 'S') { //schedule work
		cancel_delayed_work(&g_smb_chip->charger_monitor_work);
		schedule_delayed_work(&g_smb_chip->charger_monitor_work, HZ * 1);
	} else { //default
		pr_info("[%s] Unknow command (%c)\n", __func__, cmd);
	}

	return count;
}

static DEVICE_ATTR(reg, 0644, smb347_reg_show,  smb347_reg_store);

/* ----------------------------- monitor & irq handler ------------------------ */
static void smb347_interrupt_signal_check(struct smb347_chip *chip)
{
	unsigned int status = chip->intsts_bits;

	if (status & SMB347_IRQ_HTEMP_H) {
		printk(KERN_DEBUG "[%s] Hot Temprature by hard limit\n", __func__);
	}
	if (status & SMB347_IRQ_LTEMP_H) {
		printk(KERN_DEBUG "[%s] Cold Temprature by hard limit\n", __func__);
	}
	if (status & SMB347_IRQ_HTEMP_S) {
		printk(KERN_DEBUG "[%s] Hot Temprature by soft limit\n", __func__);
	}
	if (status & SMB347_IRQ_LTEMP_S) {
		printk(KERN_DEBUG "[%s] Cold Temprature by soft limit\n", __func__);
	}
	if (status & SMB347_IRQ_BAT_OV) {
		printk(KERN_DEBUG "[%s] Battery over-voltage\n", __func__);
	}
	if (status & SMB347_IRQ_MISS_BAT) {
		printk(KERN_DEBUG "[%s] Missing battery\n", __func__);
	}
	if (status & SMB347_IRQ_LOW_BAT) {
		printk(KERN_DEBUG "[%s] Low battery voltage\n", __func__);
	}
	if (status & SMB347_IRQ_PTOF_CHG) {
		printk(KERN_DEBUG "[%s] Pre-to-Fast charge battery voltage\n", __func__);
	}
	if (status & SMB347_IRQ_INT_TEMP) {
		printk(KERN_DEBUG "[%s] Internal temperature limit\n", __func__);
		/* TBD to re-init chip for charging */
	}
	if (status & SMB347_IRQ_RECHG) {
		printk(KERN_DEBUG "[%s] Re-charge battery threshold\n", __func__);
	}
	if (status & SMB347_IRQ_TAPER_CHG) {
		printk(KERN_DEBUG "[%s] Taper charging mode\n", __func__);
	}
	if (status & SMB347_IRQ_TERM_CHG) {
		printk(KERN_DEBUG "[%s] Termination charging current hit\n", __func__);
	}
	if (status & SMB347_IRQ_APSD_COMP) {
		printk(KERN_DEBUG "[%s] APSD complete\n", __func__);
	}
	if (status & SMB347_IRQ_AICL_COMP) {
		printk(KERN_DEBUG "[%s] AICL complete\n", __func__);
	}
	if (status & SMB347_IRQ_CHG_TIMEOUT) {
		printk(KERN_DEBUG "[%s] Complete charge timeout\n", __func__);
	}
	if (status & SMB347_IRQ_PCHG_TIMEOUT) {
		printk(KERN_DEBUG "[%s] Pre-charge timeout\n", __func__);
	}
	if (status & SMB347_IRQ_DCIN_OV) {
		printk(KERN_DEBUG "[%s] DCIN over-voltage\n", __func__);
	}
	if (status & SMB347_IRQ_DCIN_UV) {
		printk(KERN_DEBUG "[%s] DCIN under-voltage\n", __func__);
	}
	if (status & SMB347_IRQ_USBIN_OV) {
		printk(KERN_DEBUG "[%s] USBIN over-voltage\n", __func__);
	}
	if (status & SMB347_IRQ_USBIN_UV) {
		printk(KERN_DEBUG "[%s] USBIN under-voltage\n", __func__);
	}
	if (status & SMB347_IRQ_OTG_OC) {
		printk(KERN_DEBUG "[%s] OTG over-current\n", __func__);
	}
	if (status & SMB347_IRQ_OTG_UV) {
		printk(KERN_DEBUG "[%s] OTG battery under-voltage\n", __func__);
	}
	if (status & SMB347_IRQ_OTG_DET) {
		printk(KERN_DEBUG "[%s] OTG detected\n", __func__);
	}
	if (status & SMB347_IRQ_POWER_OK) {
		printk(KERN_DEBUG "[%s] power ok\n", __func__);
	}
}

static char smb347_apsd_string[8][16] = {
[SMB347_APSD_NOT_RUN]	= {"NOT RUN"},
[SMB347_APSD_CDP]	= {"CDP type"},
[SMB347_APSD_DCP]	= {"DCP type"},
[SMB347_APSD_OTHER_CHG]	= {"Other CHG"},
[SMB347_APSD_SDP]	= {"SDP type"},
[SMB347_APSD_ACA]	= {"ACA type"},
[SMB347_APSD_TBD1]	= {"TBD1"},
[SMB347_APSD_TBD2]	= {"TBD2"}
};

static int smb347_apsd_cable_type(struct smb347_chip *chip, u8 apsd_result)
{
	int ret = 0;

	pr_info("[%s] Input Cable: %s (%d)\n", __func__, smb347_apsd_string[apsd_result], apsd_result);
	switch(apsd_result) {
	case SMB347_APSD_NOT_RUN:
		ret = -1;
		break;
	case SMB347_APSD_SDP:
		chip->ac_prop.online = 0;
		chip->usb_prop.online = 1;
		break;
	case SMB347_APSD_CDP:
	case SMB347_APSD_DCP:
	case SMB347_APSD_OTHER_CHG:
	case SMB347_APSD_ACA:
		chip->ac_prop.online = 1;
		chip->usb_prop.online = 0;
		break;
	case SMB347_APSD_TBD1:
	case SMB347_APSD_TBD2:
	default:
		chip->ac_prop.online = 0;
		chip->usb_prop.online = 0;
		break;
	}

	return ret;
}

static int smb347_get_apsd_result(struct smb347_chip *chip)
{
	unsigned long timeout;
	int ret = 0;
	u8 reg = 0;

	timeout = jiffies + msecs_to_jiffies(1000); //1s timeout
	if (!(chip->status.sts_bits.apsd_sts)) {
		do {
			reg = 0;
			ret = smb347_i2c_read(chip, SMB347_STS_D, &reg);
			if (reg & (1 << 3)) { //APSD complete
				chip->status.sts_bytes[3] = reg;
				break;
			}
			mdelay(50);
		} while (!time_after(jiffies, timeout));
	}

	if (chip->status.sts_bits.apsd_sts) {
		ret = smb347_apsd_cable_type(chip, chip->status.sts_bits.apsd_result);
	} else {
		ret = -1;
	}
	return ret;
}

static char usb_mode_str[4][16] = {
	{"HC Mode"},
	{"USB1/1.5 Mode"},
	{"USB5/9 Mode"},
	{"N/A"},
};

static void smb347_get_cable_type(struct smb347_chip *chip)
{
	static int dc_input = 0;
	static int first_run = 0;
	int ret = 0;
	int usb_suspend = chip->status.sts_bits.usb_suspend;
	int usb_used = chip->status.sts_bits.usbin_input;
	int usb_mode = chip->status.sts_bits.usb_mode;
	int apsd_comp = (chip->intsts_bits & SMB347_IRQ_APSD_COMP)?1:0;
	int usb_uv = (chip->intsts_bits & SMB347_IRQ_USBIN_UV)?1:0;

	if (!first_run) { //first kernel run, it would lost first interrupt
		first_run = 1;
		apsd_comp = 1;
	}

	if (!chip->charger_status) {
		dc_input = 0;
		chip->ac_prop.online = 0;
		chip->usb_prop.online = 0;
		pr_info("[%s] No valid input\n", __func__);
	} else {
		//check usb first
		if (apsd_comp) {
			ret = smb347_get_apsd_result(chip);
			if (ret < 0) {
				pr_err("[%s] Get APSD result failed\n", __func__);
			}
			pr_info("[%s] usb suspend: %s, usb mode: %s\n", __func__, (usb_suspend?"Active":"Not Active"), usb_mode_str[usb_mode]);
		} else if (usb_uv) {
			chip->usb_prop.online = 0;
		}

		//check dc second
		if (usb_used && dc_input) {
			dc_input = 0;
			chip->ac_prop.online = 0;
			ret = smb347_get_apsd_result(chip);
			if (ret < 0) {
				pr_err("[%s] Get APSD result failed\n", __func__);
			}
			pr_info("[%s] usb suspend: %s, usb mode: %s\n", __func__, (usb_suspend?"Active":"Not Active"), usb_mode_str[usb_mode]);
		} else if (!usb_used) {
			dc_input = 1;
			chip->ac_prop.online = 1;
			pr_info("[%s] DC input\n", __func__);
		}
	}

	printk(KERN_DEBUG "[%s] dc_input:%d, usb_in:%d\n", __func__, dc_input, usb_used);
}

static void smb347_get_charging_status(struct smb347_chip *chip)
{
	int charging_enable = chip->status.sts_bits.chg_enable;
	int charging_mode = chip->status.sts_bits.chging_sts;

	//get status
	if (charging_enable) {
		chip->charging_mode = charging_mode;
	} else {
		chip->charging_mode = SMB347_NO_CHARGING;
	}

	if (chip->charging_mode == SMB347_NO_CHARGING) {
		chip->ac_prop.status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		chip->usb_prop.status = POWER_SUPPLY_STATUS_NOT_CHARGING;
	} else {
		if (chip->ac_prop.online == 1)
			chip->ac_prop.status = POWER_SUPPLY_STATUS_CHARGING;
		else
			chip->ac_prop.status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		
		if ((chip->usb_prop.online == 1) && (chip->ac_prop.online == 0))
			chip->usb_prop.status = POWER_SUPPLY_STATUS_CHARGING;
		else
			chip->usb_prop.status = POWER_SUPPLY_STATUS_NOT_CHARGING;
	}
	pr_info("[%s] charging status:%d (mode:%d)\n", __func__, chip->charging_status, chip->charging_mode);
}

static int aicl_result_value[16] = {
	300, 500, 700, 900,
	1200, 1500, 1800, 2000,
	2200, 2500, 2500, 2500,
	2500, 2500, 2500, 2500
};

static int actual_current_value[12] = {
	100, 150, 200, 250,
	700, 900, 1200, 1500,
	1800, 2000, 2200, 2500
};

static void smb347_get_current_info(struct smb347_chip *chip)
{
	int actual_current_index = 0;
	int acutal_current_result = chip->status.sts_bits.act_chg_cur;
	int aicl_result =  chip->status.sts_bits.aicl_result;
	int aicl_status =  chip->status.sts_bits.aicl_sts;

	if (!(acutal_current_result & (1 << 5))) {
		actual_current_index = ((acutal_current_result >> 3) & 0x3);
	} else {
		actual_current_index = ((acutal_current_result & 0x7) + 4);
	}

	printk(KERN_DEBUG "[%s] actual current: %d, aicl setting: %d\n", __func__, actual_current_value[actual_current_index], (aicl_status?aicl_result_value[aicl_result]:0));
}

static void smb347_status_check(struct smb347_chip *chip)
{
	struct smb347_property ac_old = chip->ac_prop;
	struct smb347_property usb_old = chip->usb_prop;

	//check cable type
	smb347_get_cable_type(chip);

	//otg part, TBD

	//charging status
	smb347_get_charging_status(chip);

	//current information
	smb347_get_current_info(chip);

	//power supply
	if (ac_old.online != chip->ac_prop.online || ac_old.status != chip->ac_prop.status) {
		pr_info("[%s] AC power_supply changed\n", __func__);
		power_supply_changed(&chip->ac);
	}
	if (usb_old.online != chip->usb_prop.online || usb_old.status != chip->usb_prop.status) {
		pr_info("[%s] USB power_supply changed\n", __func__);
		power_supply_changed(&chip->usb);
	}
}

static void smb347_read_status(struct smb347_chip *chip)
{
	int ret = 0, i, j, shift_index;
	unsigned char reg_data[SMB347_ALL_STATUS_NUM];
	
	chip->intsts_bits = 0;
	memset(reg_data, 0, SMB347_ALL_STATUS_NUM);
	ret = smb347_i2c_read_block(chip, SMB347_INTS_A, SMB347_ALL_STATUS_NUM, reg_data);
	if (ret < SMB347_ALL_STATUS_NUM) {
		pr_err("[%s] Lost data or failed(%d)\n", __func__, ret);
		return;
	}

	for (i = 0; i < SMB347_ALL_STATUS_NUM; i++) { //analysis status data
		printk(KERN_DEBUG "[%s] reg[%d] = 0x%02X\n", __func__, i, reg_data[i]);
	  	if (i < SMB347_INT_STATUS_NUM) { //interrupt status reg
			for (j = 0, shift_index = (i * 4); j < 4; j++, shift_index++) {
				if (reg_data[i] & (1 << (7 - (2 * j))))	{ //bit 7, 5, 3, 1, irq bit
					if (reg_data[i] & (1 << (6 - (2 * j))))	{ //bit 6, 4, 2, 0, status bit
						chip->intsts_bits |= (1 << shift_index);
					}
				} else {
					chip->intsts_bits &= ~(1 << shift_index);
				}
			}
		} else { //status reg
			chip->status.sts_bytes[i - SMB347_INT_STATUS_NUM] = reg_data[i];
		}
	}
}

static void charger_update_status(struct smb347_chip *chip)
{
	//get chip status
	smb347_read_status(chip);

	//get stat and inok status
	smb347_get_stat_status(chip);
	smb347_get_inok_status(chip);

	//check status and process
	smb347_interrupt_signal_check(chip);
	smb347_status_check(chip);

	smb347_reg_setting(chip);

	//notify battery
#ifdef CONFIG_BATTERY_BQ27520
	bq27520_notify_from_changer(chip->charging_status, chip->charger_status);
#endif
}

static void smb347_charger_monitor_work(struct work_struct *work)
{
	struct smb347_chip *chip = container_of(work, struct smb347_chip, charger_monitor_work.work);

	wake_lock(&chip->work_wakelock);
	charger_update_status(chip);
	wake_unlock(&chip->work_wakelock);
}

static irqreturn_t smb347_stat_irq_handler(int irq, void *data)
{
	struct smb347_chip *chip = g_smb_chip;

	printk(KERN_DEBUG "[%s] STAT interrupt %d\n", __func__, irq);
	cancel_delayed_work(&chip->charger_monitor_work);
	schedule_delayed_work(&chip->charger_monitor_work, HZ/10);
	return IRQ_HANDLED;
}

static void smb347_get_stat_mode(struct smb347_chip *chip)
{
	int ret = 0;
	unsigned char value = 0;
	ret = smb347_i2c_read(chip, SMB347_STAT_CNTRL, &value);
	if (ret < 0) {
		pr_err("[%s] Get stat mode failed: %d\n", __func__, ret);
		chip->inok_mode = SMB347_NONE_MODE;
		return;
	}
	
	if (!(value & (1 << 5))) { //stat output enable
		if (value & (1 << 7)) {
			chip->stat_pol = 1;
		} else {
			chip->stat_pol = 0;
		}

		if (value & (1 << 6)) {
			chip->stat_mode = SMB347_USB_FAIL_MODE;
		} else {
			chip->stat_mode = SMB347_IND_CHG_MODE;
		}
	} else {
		chip->stat_mode = SMB347_NONE_MODE;
	}
	pr_info("[%s] stat mode:%d, polarity:%s\n", __func__, chip->stat_mode, (chip->stat_pol?"high":"low"));
}

static void smb347_get_inok_mode(struct smb347_chip *chip)
{
	int ret = 0;
	unsigned char value = 0;
	ret = smb347_i2c_read(chip, SMB347_SYSOK_SEL, &value);
	if (ret < 0) {
		pr_err("[%s] Get inok mode failed: %d\n", __func__, ret);
		chip->inok_mode = SMB347_NONE_MODE;
		return;
	}
	
	if (value & 0x1) {
		chip->inok_pol = 1;
	} else {
		chip->inok_pol = 0;
	}

	value = (value >> 6); //for bits[7:6]
	switch (value) {
	case 0:
		chip->inok_mode = SMB347_INOK_MODE;
		break;
	case 1:
		chip->inok_mode = SMB347_SYSOK_MODE_A;
		break;
	case 2:
		chip->inok_mode = SMB347_SYSOK_MODE_B;
		break;
	case 3:
		chip->inok_mode = SMB347_CHG_DET_MODE;
		break;
	default:
		pr_info("[%s] Unknow mode (value=%d)\n", __func__, value);
		chip->inok_mode = SMB347_NONE_MODE;
		break;
	}
	pr_info("[%s] inok mode:%d, polarity:%s\n", __func__, chip->inok_mode, (chip->inok_pol?"high":"low"));
}

static int smb347_set_gpio_irq(struct smb347_platform_data *pdata, struct smb347_chip *chip)
{
	int ret = 0;

	//stat irq and gpio
  	if ((pdata->gpio_map & SMB347_STAT_GPIO_MASK) && (pdata->stat_gpio >= 0)) {
		smb347_get_stat_mode(chip);
		chip->stat_gpio = pdata->stat_gpio;
		ret = gpio_request(chip->stat_gpio, "smb347_stat");
		if (ret) {
			pr_err("[%s] Failed to request GPIO #%d: %d\n", __func__, chip->stat_gpio, ret);
			goto failed1;
		}
		gpio_direction_input(chip->stat_gpio);
	  	chip->stat_irq = gpio_to_irq(pdata->stat_gpio);
		if (chip->stat_irq < 0) {
			pr_err("[%s] Get GPIO-%d to irq failed\n", __func__, pdata->stat_gpio);
			goto failed1;
		}
		ret = request_threaded_irq(chip->stat_irq, NULL, smb347_stat_irq_handler, IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, 
			"smb347_stat", chip);
		if (ret < 0) {
			pr_err("[%s] Failed to request IRQ: #%d: %d\n", __func__, chip->stat_irq, ret);
			goto failed1;
		}
		chip->irq_gpio_map |= SMB347_STAT_GPIO_MASK;
	}
	//inok irq and gpio
  	if ((pdata->gpio_map & SMB347_INOK_GPIO_MASK) && (pdata->inok_gpio >= 0)) {
		smb347_get_inok_mode(chip);
		chip->inok_gpio = pdata->inok_gpio;
		ret = gpio_request(chip->inok_gpio, "smb347_inok");
		if (ret) {
			pr_err("[%s] Failed to request GPIO #%d: %d\n", __func__, chip->inok_gpio, ret);
			goto failed2;
		}
		gpio_direction_input(chip->inok_gpio);
		chip->irq_gpio_map |= SMB347_INOK_GPIO_MASK;
	}
	//usb suspend gpio
  	if ((pdata->gpio_map & SMB347_SUSP_GPIO_MASK) && (pdata->susp_gpio >= 0)) {
		chip->susp_gpio = pdata->susp_gpio;
		ret = gpio_request(chip->susp_gpio, "smb347_susp");
		if (ret) {
			pr_err("[%s] Failed to request GPIO #%d: %d\n", __func__, chip->susp_gpio, ret);
			goto failed3;
		}
		gpio_direction_output(chip->susp_gpio, 1);
		chip->irq_gpio_map |= SMB347_SUSP_GPIO_MASK;
	}
	//usb mode gpio
	if ((pdata->gpio_map & SMB347_USB_GPIO_MASK) && (pdata->usb_gpio >= 0)) {
		chip->usb_gpio = pdata->usb_gpio;
		gpio_request(chip->usb_gpio, "smb347_usb");
		if (ret) {
			pr_err("[%s] Failed to request GPIO #%d: %d\n", __func__, chip->usb_gpio, ret);
			goto failed4;
		}
		gpio_direction_output(chip->usb_gpio, 1);
		chip->irq_gpio_map |= SMB347_USB_GPIO_MASK;
	}
	//booster gpio
	if ((pdata->gpio_map & SMB347_BSTR_GPIO_MASK) && (pdata->bstr_gpio >= 0)) {
		chip->bstr_gpio = pdata->bstr_gpio;
		gpio_request(chip->bstr_gpio, "smb347_booster");
		if (ret) {
			pr_err("[%s] Failed to request GPIO #%d: %d\n", __func__, chip->bstr_gpio, ret);
			goto failed5;
		}
		gpio_direction_output(chip->bstr_gpio, 0);
		chip->irq_gpio_map |= SMB347_BSTR_GPIO_MASK;
	}

	return 0;

failed5:
	if (chip->irq_gpio_map & SMB347_USB_GPIO_MASK)
		gpio_free(chip->usb_gpio);

failed4:
	if (chip->irq_gpio_map & SMB347_SUSP_GPIO_MASK)
		gpio_free(chip->susp_gpio);

failed3:
	if (chip->irq_gpio_map & SMB347_INOK_GPIO_MASK)
		gpio_free(chip->inok_gpio);

failed2:
	if (chip->irq_gpio_map & SMB347_STAT_GPIO_MASK) {
		free_irq(chip->stat_irq, chip);
		gpio_free(chip->stat_gpio);
	}

failed1:
	return -1;
}

static void smb347_free_gpio_irq(struct smb347_chip *chip)
{
	if (chip->irq_gpio_map & SMB347_STAT_GPIO_MASK) {
		free_irq(chip->stat_irq, chip);
		gpio_free(chip->stat_gpio);
	}
	if (chip->irq_gpio_map & SMB347_INOK_GPIO_MASK)
		gpio_free(chip->inok_gpio);
	if (chip->irq_gpio_map & SMB347_SUSP_GPIO_MASK)
		gpio_free(chip->susp_gpio);
	if (chip->irq_gpio_map & SMB347_USB_GPIO_MASK)
		gpio_free(chip->usb_gpio);
	if (chip->irq_gpio_map & SMB347_BSTR_GPIO_MASK)
		gpio_free(chip->bstr_gpio);
}

#if 0
static int smb347_usb_notifier_cb(struct notifier_block *nb,
			unsigned long val, void *data)
{
	struct smb347_chip *chip = container_of(nb, struct smb347_chip,
					usb_notifier);

	switch (val) {
	case USB_EVENT_NONE:
		/* USB disconnected */
		chip->ac_prop.online = 0;
		chip->usb_prop.online = 0;
		
		power_supply_changed(&chip->ac);
		power_supply_changed(&chip->usb);

		pr_info("[%s] USB disconnected\n", __func__);
		break;
	}

	return NOTIFY_OK;
}
#endif

/* ----------------------------- Driver functions ----------------------------- */
static int smb347_charger_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct smb347_platform_data *pdata = client->dev.platform_data;
  	struct smb347_chip *chip = NULL;
  	int retval = 0;

  	pr_info("[%s] start charger smb347 probe\n", __func__);

	product_id = fih_get_product_id();
	phase_id = fih_get_product_phase();

	if (!pdata) {
		pr_info("[%s] No platform data for used\n", __func__);
		return -ENOMEM;
	}

	//original chip data
	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		pr_err("[%s] Failed to allocate chip data\n", __func__);
		return-ENOMEM;
	}

	//chip data init
	chip->dev = &client->dev;
	chip->client = client;
	mutex_init(&chip->read_lock);
	mutex_init(&chip->charger_lock);
	wake_lock_init(&chip->work_wakelock, WAKE_LOCK_SUSPEND, "smb347_work");
	wake_lock_init(&chip->charge_wakelock, WAKE_LOCK_SUSPEND, "smb347_charge");
	i2c_set_clientdata(client, chip);
	g_smb_chip = chip;
	chip->charger_status = 0;
	chip->charging_status = 0;

	//check smb347 exist
	retval = smb347_alive(chip);
	if (retval) {
		pr_err("[%s] No SMB347 exist!!\n", __func__);
		goto i2c_free;
	}

	smb347_reg_check(chip);
	smb347_reg_setting(chip);

	//set irq
	retval = smb347_set_gpio_irq(pdata, chip);
	if (retval < 0) {
		pr_err("[%s] Set smb347 gpio/irq mode failed(%d)\n", __func__, retval);
		goto i2c_free;
	}

	//init power supply property
	//ac part
	chip->ac.name			= "ac";
	chip->ac.type			= POWER_SUPPLY_TYPE_MAINS;
	chip->ac.properties		= smb347_ac_props;
	chip->ac.num_properties		= ARRAY_SIZE(smb347_ac_props);
	chip->ac.get_property		= smb347_ac_get_prop;
	chip->ac_prop.status		= POWER_SUPPLY_STATUS_UNKNOWN;
	retval = power_supply_register(chip->dev, &chip->ac); //create battery sysfs
	if (retval) {
		pr_err("[%s] Failed to register %s:(%d)\n", __func__, chip->ac.name, retval);
		goto err_irq;
	}

	//usb part
	chip->usb.name			= "usb";
	chip->usb.type			= POWER_SUPPLY_TYPE_USB;
	chip->usb.properties		= smb347_usb_props;
	chip->usb.num_properties	= ARRAY_SIZE(smb347_usb_props);
	chip->usb.get_property		= smb347_usb_get_prop;
	chip->usb_prop.status		= POWER_SUPPLY_STATUS_UNKNOWN;
	retval = power_supply_register(chip->dev, &chip->usb); //create battery sysfs
	if (retval) {
		pr_err("[%s] Failed to register %s:(%d)\n", __func__, chip->usb.name, retval);
		goto err_psy_ac;
	}

	//sys device
	retval = device_create_file(&client->dev, &dev_attr_reg);
	if (retval) {
		pr_err("[%s] Failed to register devive %s:(%d)\n", __func__, dev_attr_reg.attr.name, retval);
		goto err_psy_usb;
	}

	retval = misc_register(&charger_miscdev);
 	if (retval < 0) {
		pr_err("[%s] Failed to register miscdev %s:(%d)\n", __func__, charger_miscdev.name, retval);
 		goto err_sysdev;
	}

	INIT_DELAYED_WORK(&chip->charger_monitor_work, smb347_charger_monitor_work);
	schedule_delayed_work(&g_smb_chip->charger_monitor_work, HZ * 4);

	return 0;

err_sysdev:
	device_remove_file(&client->dev, &dev_attr_reg);
err_psy_usb:
	power_supply_unregister(&chip->usb);
err_psy_ac:
	power_supply_unregister(&chip->ac);
err_irq:
	smb347_free_gpio_irq(chip);
i2c_free:
	g_smb_chip = NULL;
	i2c_set_clientdata(client, NULL);
	wake_lock_destroy(&chip->work_wakelock);
	kfree(chip);

	return retval;
}

static int smb347_charger_remove(struct i2c_client *client)
{
  	struct smb347_chip *chip = i2c_get_clientdata(client);
  	pr_info("[%s] start charger smb347 remove\n", __func__);

	misc_deregister(&charger_miscdev);
	device_remove_file(&client->dev, &dev_attr_reg);
	power_supply_unregister(&chip->usb);
	power_supply_unregister(&chip->ac);
	smb347_free_gpio_irq(chip);
	g_smb_chip = NULL;
	i2c_set_clientdata(client, NULL);
	wake_lock_destroy(&chip->work_wakelock);
	kfree(chip);

	return 0;
}

#ifdef CONFIG_PM
static int smb347_charger_suspend(struct i2c_client *client, pm_message_t state)
{
  	struct smb347_chip *chip = i2c_get_clientdata(client);
  	pr_info("[%s] start charger smb347 suspend\n", __func__);
	return 0;
}

static int smb347_charger_resume(struct i2c_client *client)
{
  	struct smb347_chip *chip = i2c_get_clientdata(client);
  	pr_info("[%s] start charger smb347 resume\n", __func__);
	return 0;
}
#else
#define smb347_charger_suspend	NULL
#define smb347_charger_resume	NULL
#endif

static const struct i2c_device_id smb347_id[] = {
	{ "smb347_charger", 0 },
	{},
};

static struct i2c_driver smb347_charger_driver = {
	.driver = {
		.name = "smb347_charger",
	},
	.id_table	= smb347_id,
	.probe		= smb347_charger_probe,
	.remove		= smb347_charger_remove,
	.suspend	= smb347_charger_suspend,
	.resume		= smb347_charger_resume,
};

static int __init smb347_charger_init(void)
{
	return i2c_add_driver(&smb347_charger_driver);
}

static void __exit smb347_charger_exit(void)
{
	i2c_del_driver(&smb347_charger_driver);
}

module_init(smb347_charger_init);
module_exit(smb347_charger_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("PinyCHWu <pinychwu@fih-foxconn.com>");
MODULE_DESCRIPTION("smb347 charger driver");

