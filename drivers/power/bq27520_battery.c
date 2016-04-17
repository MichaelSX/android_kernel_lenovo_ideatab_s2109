/*
 * Driver for batteries with BQ27520 gas gauge chips inside.
 *
 * Copyright (C) 2010 Chi Mei Communication Systems, INC.
 *
 * Derived from kernel/drivers/power/bq27520_battery.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Author:  PinyCHWu <pinychwu@fihtdc.com>
 *	    March 2010
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
#include <linux/idr.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/skbuff.h>
#include <linux/miscdevice.h>
#include <linux/device.h>
#include <linux/sysdev.h>

#include <mach/gpio.h>
#include <fih/resources/battery/bq27520_battery.h>

//related charger part, now disable due to charger driver not done, piny.
#define RELATED_CHARGER_PART		1
//monitor interval
#define BQ27520_MONITOR_INTERVAL	(HZ * 10)
//normal temperature 25 degree C
#define BQ27520_NORMAL_TEMP		250

/* If the system has several batteries we need a different name for each
 * of them...
 */
static DEFINE_IDR(battery_id);
static DEFINE_MUTEX(battery_mutex);
static struct bq27520_chip *g_bq_chip = NULL;
static unsigned int cache_time = 1000;
static int monitor_init_flag = 0;
static int fakecap_enable = 0; //1: enable, 0: disable (default)

static void bq27520_update_status(struct bq27520_chip *chip, int interrupt_type);

static const char check_control_cmd_table[CNTL_LAST][4] = {
  	//command index		{unsealed/sealed/size/avaliable}
	[CNTL_CONTROL_STATUS]	= {0, 0, 2, 1}, //[0,1]:0->R, 1:W [2]:size [3]:1->avaliable
	[CNTL_DEVICE_TYPE]	= {0, 0, 2, 1},
	[CNTL_FW_VERSION]	= {0, 0, 2, 1},
	[CNTL_HW_VERSION]	= {0, 0, 2, 1},
	[CNTL_DF_CHECKSUM]	= {0, 0, 2, 1},
	[CNTL_PREV_MACWRITE]	= {0, 0, 2, 1},
	[CNTL_CHEM_ID]		= {0, 0, 2, 1},
	[CNTL_BOARD_OFFSET]	= {0, 0, 2, 1},
	[CNTL_CC_INT_OFFSET]	= {0, 0, 2, 1},
	[CNTL_WRITE_CC_OFFSET]	= {1, -1, 2, 1},
	[CNTL_OCV_CMD]		= {1, 1, 2, 1},
	[CNTL_BAT_INSERT]	= {1, 1, 2, 1},
	[CNTL_BAT_REMOVE]	= {1, 1, 2, 1},
	[CNTL_SET_HIBERNATE]	= {1, 1, 2, 1},
	[CNTL_CLEAR_HIBERNATE]	= {1, 1, 2, 1},
	[CNTL_SET_SLEEP]	= {1, 1, 2, 1},
	[CNTL_CLEAR_SLEEP]	= {1, 1, 2, 1},
	[CNTL_FACTORY_RESTORE]	= {1, -1, 2, 1},
	[CNTL_ENABLE_DLOG]	= {1, 1, 2, 1},
	[CNTL_DISABLE_DLOG]	= {1, 1, 2, 1},
	[CNTL_SEALED]		= {1, -1, 2, 1},
	[CNTL_IT_ENABLE]	= {1, -1, 2, 1},
	[CNTL_IT_DISABLE]	= {1, -1, 2, 1},
	[CNTL_CAL_MODE]		= {1, -1, 2, 1},
	[CNTL_RESET]		= {1, -1, 2, 1},
};

static const char check_cmd_table[BQ27520_CMD_LAST][4] = {
	//command index		{unsealed/sealed/size/avaliable}
  	//standard command	//[0,1]:0->R, 1->R/W [2]:size [3]:1->avaliable
  	[BQ27520_CMD_CNTL]	= {1, 1, 2, 1},
  	[BQ27520_CMD_AR]	= {1, 1, 2, 1},
  	[BQ27520_CMD_ARTTE]	= {1, 1, 2, 1},
  	[BQ27520_CMD_TEMP]	= {1, 0, 2, 1},
  	[BQ27520_CMD_VOLT]	= {1, 1, 2, 1},
  	[BQ27520_CMD_FLAGS]	= {1, 0, 2, 1},
  	[BQ27520_CMD_NAC]	= {1, 0, 2, 1},
  	[BQ27520_CMD_FAC]	= {1, 0, 2, 1},
  	[BQ27520_CMD_RM]	= {1, 0, 2, 1},
  	[BQ27520_CMD_FCC]	= {1, 0, 2, 1},
  	[BQ27520_CMD_AI]	= {1, 0, 2, 1},
  	[BQ27520_CMD_TTE]	= {1, 0, 2, 1},
  	[BQ27520_CMD_TTF]	= {1, 0, 2, 1},
  	[BQ27520_CMD_SI]	= {1, 0, 2, 1},
  	[BQ27520_CMD_STTE]	= {1, 0, 2, 1},
  	[BQ27520_CMD_MLI]	= {1, 0, 2, 1},
  	[BQ27520_CMD_MLTTE]	= {1, 0, 2, 1},
  	[BQ27520_CMD_AE]	= {1, 0, 2, 1},
  	[BQ27520_CMD_AP]	= {1, 0, 2, 1},
  	[BQ27520_CMD_TTECP]	= {1, 0, 2, 1},
  	[BQ27520_CMD_SOH]	= {1, 0, 2, 1},
  	[BQ27520_CMD_SOC]	= {1, 0, 2, 1},
  	[BQ27520_CMD_NIC]	= {1, 0, 2, 1},
  	[BQ27520_CMD_ICR]	= {1, 0, 2, 1},
  	[BQ27520_CMD_DLI]	= {0, 0, 2, 1},
  	[BQ27520_CMD_DLB]	= {0, 0, 2, 1},

	//external command
	[BQ27520_EXT_DCAP]	= {0, 0, 2, 1},
	[BQ27520_EXT_DFCLS]	= {1, -1, 1, 1},
	[BQ27520_EXT_DFBLK]	= {1, 1, 1, 1},
	[BQ27520_EXT_DFD]	= {1, 0, 32, 1},
	[BQ27520_EXT_DFDCKS]	= {1, 1, 1, 1},
	[BQ27520_EXT_DFDCNTL]	= {1, -1, 1, 1},
	[BQ27520_EXT_DNAMELEN]	= {0, 0, 1, 1},
	[BQ27520_EXT_DNAME]	= {0, 0, 7, 1},
	[BQ27520_EXT_APPSTAT]	= {0, 0, 1, 1},
};

/* ---------------------- Gas Gauge access functions ------------------------- */
static int bq27520_io(struct i2c_client *client, u8 reg, u8 *data, unsigned int size, u8 mode)
{
  	struct bq27520_chip *chip = i2c_get_clientdata(client);
  	int ret = 0;
	u8 cmd[256];
	int send_size = 0;

	mutex_lock(&chip->io_lock);
	//check mode
	memset(cmd, 0, 256);
	cmd[0] = (char)reg;
	if (mode) {//write 
		memcpy(&cmd[1], data, size);
		send_size = size + 1;
	} else { //read
		send_size = 1;
	}

	//first send command: reg+data(write) /reg(read)
	ret = i2c_master_send(client, cmd, send_size);
	if (ret < 0) {
	  	pr_err("[%s] send error: %d\n", __func__, ret);
	  	goto out_io;
	}

	//read if needed
	if (!mode) {
		ret = i2c_master_recv(client, data, size);
		if (ret < 0) {
			memset(data, 0, size);
	  		pr_err("[%s] read error: %d\n", __func__, ret);
		}
	}

out_io:
	if (mode)
		mdelay(16);
	else
	  	mdelay(1);

	mutex_unlock(&chip->io_lock);
	return ret;
}

static int bq27520_control_cmd(struct bq27520_chip *chip, unsigned int sub_cmd, char *data)
{
	int ret = 0;
	int mode = 0, size = 0;

	if (!check_control_cmd_table[sub_cmd][3]) {
		pr_err("[%s] Unknown Control sub-command (0x%02X)\n", __func__, sub_cmd);
		return -EINVAL;
	}

	mode =  check_control_cmd_table[sub_cmd][chip->sealed_status];
	if (mode == -1) {
	  	pr_err("[%s] sub-command (0x%02X) not support in Sealed mode(%d)\n", __func__, sub_cmd, chip->sealed_status);
		return -EINVAL;
	}

	size = check_control_cmd_table[sub_cmd][2];
	
	ret = bq27520_io(chip->client, BQ27520_CMD_CNTL, (u8*)&sub_cmd, size, 1);
	if (ret < 0) {
		pr_err("[%s] Write control sub-command (0x%02X) failed\n", __func__, sub_cmd);
		return ret;
	}

	if (!mode) { //read data
		ret = bq27520_io(chip->client, BQ27520_CMD_CNTL, data, size, 0);
		if (ret < 0) {
		  	pr_err("[%s] Read control sub-command (0x%02X) failed\n", __func__, sub_cmd);
			return ret;
		}
	}

	return 0;
}

static int bq27520_cmd(struct bq27520_chip *chip, unsigned int cmd, char *data, unsigned int mode)
{
	int ret = 0, size = 0;

	if (!check_cmd_table[cmd][3]) {
		pr_err("[%s] Unknown standard command (0x%02X)\n", __func__, cmd);
		return -EINVAL;
	}

	if (mode > check_cmd_table[cmd][chip->sealed_status]) {
	  	pr_err("[%s] command (0x%02X) not support in Sealed mode(%d)\n", __func__, cmd, chip->sealed_status);
		return -EINVAL;
	}

	size = check_cmd_table[cmd][2];
	ret = bq27520_io(chip->client, cmd, data, size, mode);
	if (ret < 0) {
		pr_err("[%s] RW standard command (0x%02X) failed\n", __func__, cmd);
		return ret;
	}

	return size;
}

static u8 bq27520_dataflash_checksum(char *buf, unsigned int size)
{
	int i = 0;
	u8 check_sum = 0;

	for(; i < size; i++)
		check_sum += *(buf + i);
	check_sum = 255 - check_sum;

	return check_sum;
}

static int bq27520_dataflash_cmd(struct bq27520_chip *chip, unsigned int sub_class, unsigned int offset, 
    							char *data, unsigned int len, unsigned int mode)
{
	int ret = 0, size = len;
	u8 check_sum, block_no, block_off, block_size, control_mode = 0;
	int index = 0;
	
	//check sealed / unsealed mode
	if (chip->sealed_status == 1) { //sealed mode
		if (sub_class != SUBCLASS_MANUFACTURE) {
			pr_err("[%s] Wrong class in sealed mode\n", __func__);
			return -EINVAL;
		}
	} else { //unsealed mode
		//set data flash control mode
		ret = bq27520_cmd(chip, BQ27520_EXT_DFDCNTL, (char *)&control_mode, 1);
		if (ret < 0) {
			pr_err("[%s] set data flash control failed\n", __func__);
			return ret;
		}
		//set data flash which class
		ret = bq27520_cmd(chip, BQ27520_EXT_DFCLS, (char *)&sub_class, 1);
		if (ret < 0) {
			pr_err("[%s] set data flash class failed\n", __func__);
			return ret;
		}
	}

	while(size > 0) {
	  	if (chip->sealed_status) { //sealed mode
	  		if (offset < 32) {
				block_no = 1; //Manufacturer info block A
			} else if (offset < 64){
			  	block_no = 2; //Manufacturer info block B
			} else {
				pr_err("[%s] offset out of range in sealed mode\n", __func__);
				return -EINVAL;
			}
		} else { //unsealed mode
			block_no = offset / 32;
		}
		block_off = BQ27520_EXT_DFD + (offset % 32);
		if (size > (32 - (offset % 32))) {
			block_size = 32 - (offset % 32);
			offset += block_size;
			size -= block_size;
		} else {
			block_size = size;
			offset += size;
			size = 0;
		}
		
		//set data flash which number of block
		ret = bq27520_cmd(chip, BQ27520_EXT_DFBLK, (char *)&block_no, 1);
		if (ret < 0) {
			pr_err("[%s] set data flash block failed\n", __func__);
			return ret;
		}
		if (mode) { //write
		  	check_sum = bq27520_dataflash_checksum((data + index), block_size);
			ret = bq27520_io(chip->client, block_off, (data + index), block_size, 1);
			if (ret < 0) {
				pr_err("[%s] write data to flash failed\n", __func__);
				return ret;
			}
			ret = bq27520_cmd(chip, BQ27520_EXT_DFDCKS, (char *)&check_sum, 1);
			if (ret < 0) {
				pr_err("[%s] set data flash checksum failed\n", __func__);
				return ret;
			}
		} else { //read
			ret = bq27520_io(chip->client, block_off, (data + index), block_size, 0);
			if (ret < 0) {
				pr_err("[%s] read data to flash failed\n", __func__);
				return ret;
			}
		}
		index += block_size;
	}

	return len;
}

/* ---------------------- refresh DFI functions ------------------------- */
#define DFI_SIZE	(0x400)
#define ROW_SIZE	(96)
static u8 *row_if = NULL;
static u8 *dfi_array = NULL;
static u16 dfi_checksum = 0;

//step 1-2
static int bq27520_enter_rom_mode(struct bq27520_chip *chip)
{
  	int ret;
	u16 rom_mode_command = 0x0F00;

	ret = bq27520_io(chip->client, 0x00, (u8*)&rom_mode_command, 2, 1);
	if (ret < 0) {
		pr_err("[%s] set rom mode failed: %d\n", __func__, ret);
		return ret;
	}
	mdelay(2);
	return 0;
}

//step 1-3
static int bq27520_read_if_row(struct bq27520_chip *chip)
{
  	int ret, row_index;
  	u8 data[3];

	if (row_if == NULL) {
		row_if = kzalloc(2 * ROW_SIZE, GFP_KERNEL);
		if (row_if == 0) {
		  	ret = 1;
			goto out;
		}
	}

	//3. read instruction flash first tow row
	data[0] = data[2] = 0x00;
	for (row_index = 0; row_index < 2; row_index++) {
		data[1] = row_index;
		ret = bq27520_io(chip->rom_client, 0x00, data, 3, 1);
		if (ret < 0) goto out;
		ret = bq27520_io(chip->rom_client, 0x64, &data[1], 2, 1);
		if (ret < 0) goto out;
		ret = bq27520_io(chip->rom_client, 0x04, (row_if + row_index * ROW_SIZE), ROW_SIZE, 0);
		if (ret < 0) goto out;
		mdelay(20);
	}
	return 0;
out:
	pr_err("[%s] read row data failed: %d\n", __func__, ret);
	return -1;
}

//step 1-4
static int bq27520_erase_if_row(struct bq27520_chip *chip)
{
  	int ret;
  	u8 data[2];

	if (dfi_array == NULL) {
		ret = 1;
		goto out;
	}

	data[0] = 0x03;
	data[1] = 0x00;
	ret = bq27520_io(chip->rom_client, 0x00, data, 1, 1);
	if (ret < 0) goto out;
	ret = bq27520_io(chip->rom_client, 0x64, data, 2, 1);
	if (ret < 0) goto out;
	mdelay(20);
	ret = bq27520_io(chip->rom_client, 0x66, data, 1, 0);
	if (ret < 0) goto out;
	if (data[0] != 0) goto out;
	return 0;
out:
	pr_err("[%s] erase row data failed: %d\n", __func__, ret);
	return -1;
}

//step 2-1
static int bq27520_mass_erase(struct bq27520_chip *chip)
{
  	int ret;
	u8 data[2];

	if (dfi_array == NULL) {
		ret = 1;
		goto out;
	}

	data[0] = 0x0C;
	ret = bq27520_io(chip->rom_client, 0x00, data, 1, 1);
	if (ret < 0) goto out;
	data[0] = 0x83;
	data[1] = 0xDE;
	ret = bq27520_io(chip->rom_client, 0x04, data, 2, 1);
	if (ret < 0) goto out;
	data[0] = 0x6D;
	data[1] = 0x01;
	ret = bq27520_io(chip->rom_client, 0x64, data, 2, 1);
	if (ret < 0) goto out;
	mdelay(20);
	ret = bq27520_io(chip->rom_client, 0x66, data, 1, 0);
	if (ret < 0) goto out;
	if (data[0] != 0) goto out;
	return 0;
out:
	pr_err("[%s] erase gasgauge flash failed: %d\n", __func__, ret);
	return -1;
}

//step 2-2
static int bq27520_write_flash(struct bq27520_chip *chip)
{
	int ret, row_index;
	u8 data[2], rowdata[32];
	u16 checksum, sum, i;

	if (dfi_array == NULL) {
		ret = 1;
		goto out;
	}

	dfi_checksum = 0;
	for (row_index = 0; row_index < (DFI_SIZE / 32); row_index++) {
		memcpy(rowdata, (dfi_array + (row_index * 32)), 32);
		
		data[0] = 0x0A;
		data[1] = row_index;
		ret = bq27520_io(chip->rom_client, 0x00, data, 2, 1);
		if (ret < 0) goto out;
		ret = bq27520_io(chip->rom_client, 0x04, rowdata, 32, 1);
		if (ret < 0) goto out;
		
		for(i = 0, sum = 0; i < 32; i++)
			sum += rowdata[i];
		checksum = (0x0A + row_index + sum) % 0x10000;
		dfi_checksum = (dfi_checksum + sum) % 0x10000;

		data[0] = (checksum & 0xFF);
		data[1] = ((checksum >> 8) & 0xFF);
		ret = bq27520_io(chip->rom_client, 0x64, data, 2, 1);
		if (ret < 0) goto out;
		mdelay(2);
		ret = bq27520_io(chip->rom_client, 0x66, data, 1, 0);
		if (ret < 0) goto out;
		if (data[0] != 0)
			goto out;
	}
	return 0;
out:
	pr_err("[%s] Write gasgauge flash failed: %d\n", __func__, ret);
	return -1;
}

//step 2-3
static int bq27520_check_dficks(struct bq27520_chip *chip)
{
  	int ret;
	u8 data[2];

	data[0] = 0x08;
	data[1] = 0x00;
	ret = bq27520_io(chip->rom_client, 0x00, data, 1, 1);
	if (ret < 0) goto out;
	ret = bq27520_io(chip->rom_client, 0x64, data, 2, 1);
	if (ret < 0) goto out;
	mdelay(20);
	ret = bq27520_io(chip->rom_client, 0x04, data, 2, 0);
	if (ret < 0) goto out;
	if (dfi_checksum != *(u16*)data) goto out;
	return 0;
out:
	pr_err("[%s] check flash checksum failed: %d\n", __func__, ret);
	return -1;
}

//step 3-1
static int bq27520_write_back_row(struct bq27520_chip *chip)
{
  	int ret, row_index;
	u16 checksum, sum, i;
	u8 data[3];

	if (row_if == NULL) {
		ret = 1;
		goto out;
	}

	for (row_index = 1; row_index >= 0; row_index--) {
		data[0] = 0x02;
		data[1] = row_index;
		data[2] = 0x00;
		ret = bq27520_io(chip->rom_client, 0x00, data, 3, 1);
		if (ret < 0) goto out;
		ret = bq27520_io(chip->rom_client, 0x04, (row_if + row_index * ROW_SIZE), ROW_SIZE, 1);
		if (ret < 0) goto out;
		for(i = 0, sum = 0; i < ROW_SIZE; i++)
			sum += *(row_if + row_index * ROW_SIZE + i);
		checksum = (0x02 + row_index + sum) % 0x10000;

		ret = bq27520_io(chip->rom_client, 0x64, (u8*)&checksum, 2, 1);
		if (ret < 0) goto out;
		mdelay(20);
		ret = bq27520_io(chip->rom_client, 0x66, data, 1, 0);
		if (ret < 0) goto out;
		if (data[0] != 0){ ret = 5; goto out;}
	}
	return 0;
out:
	pr_err("[%s] write back row data failed: %d\n", __func__, ret);
	return -1;
}

//step 3-2
static int bq27520_exit_rom_mode(struct bq27520_chip *chip)
{
  	int ret;
	u8 data[2];

	data[0] = 0x0F;
	data[1] = 0x00;
	ret = bq27520_io(chip->rom_client, 0x00, data, 1, 1);
	if (ret < 0) goto out;
	ret = bq27520_io(chip->rom_client, 0x64, data, 2, 1);
	if (ret < 0) goto out;
	return 0;
out:
	pr_err("[%s] exit rom mode failed: %d\n", __func__, ret);
	return -1;
}

//step 4
static int bq27520_reset_chip(struct bq27520_chip *chip)
{
	int ret = 0;

	ret = bq27520_control_cmd(chip, CNTL_IT_ENABLE, NULL);
	if (ret < 0) goto out;
	mdelay(1000);
	ret = bq27520_control_cmd(chip, CNTL_RESET, NULL);
	if (ret < 0) goto out;

//	ret = bq27520_control_cmd(chip, CNTL_SEALED, NULL);
//	if (ret < 0) goto out;
	return 0;
out:
	pr_err("[%s] reset gasgauge chip failed: %d\n", __func__, ret);
	return -1;
}

/* ---------------------- export battery functions ------------------------- */
unsigned int bq27520_battery_control_status(void)
{
	struct bq27520_chip *chip = g_bq_chip;
	int ret = 0;

	if (!chip) {
		pr_err("[%s] bq27520 chip not ready\n", __func__);
		return 0;
	}

	ret = bq27520_control_cmd(chip, CNTL_CONTROL_STATUS, (char*)&(chip->prop.cntrl_status));
	if (ret < 0)
	  	return 0;

	return chip->prop.cntrl_status;
}
EXPORT_SYMBOL(bq27520_battery_control_status);

unsigned int bq27520_battery_flags(void)
{
	struct bq27520_chip *chip = g_bq_chip;
	int ret = 0;
	u16 data = 0;

	if (!chip) {
		pr_err("[%s] bq27520 chip not ready\n", __func__);
		return 0;
	}

	ret = bq27520_cmd(chip, BQ27520_CMD_FLAGS, (char*)&data, 0);
	if (ret < 0)
	  	return 0;

	chip->prop.flags = data;
	return chip->prop.flags;
}
EXPORT_SYMBOL(bq27520_battery_flags);

unsigned int bq27520_battery_capacity(void)
{
	struct bq27520_chip *chip = g_bq_chip;
	int ret = 0;
	u16 data = 0;

	if (!chip) {
		pr_err("[%s] bq27520 chip not ready\n", __func__);
		return 0;
	}

	ret = bq27520_cmd(chip, BQ27520_CMD_SOC, (char*)&data, 0);
	if (ret >= 0)
		chip->prop.capacity = data;

	return chip->prop.capacity;
}
EXPORT_SYMBOL(bq27520_battery_capacity);

unsigned int bq27520_battery_voltage(void)
{
	struct bq27520_chip *chip = g_bq_chip;
	int ret = 0;
	u16 data = 0;

	if (!chip) {
		pr_err("[%s] bq27520 chip not ready\n", __func__);
		return 0;
	}

	ret = bq27520_cmd(chip, BQ27520_CMD_VOLT, (char*)&data, 0);
	if (ret >= 0)
		chip->prop.voltage_mV = data;

	return chip->prop.voltage_mV;
}
EXPORT_SYMBOL(bq27520_battery_voltage);

int bq27520_battery_temperature(void)
{
	struct bq27520_chip *chip = g_bq_chip;
	int ret = 0;
	s16 data = 0;

	if (!chip) {
		pr_err("[%s] bq27520 chip not ready\n", __func__);
		return 0;
	}

	ret = bq27520_cmd(chip, BQ27520_CMD_TEMP, (char*)&data, 0);
	if (ret >= 0)
		chip->prop.temp_C = (int)data - 2730; //K to C

	return chip->prop.temp_C;
}
EXPORT_SYMBOL(bq27520_battery_temperature);

int bq27520_battery_current(void)
{
	struct bq27520_chip *chip = g_bq_chip;
	int ret = 0;
	s16 data = 0;

	if (!chip) {
		pr_err("[%s] bq27520 chip not ready\n", __func__);
		return 0;
	}

	ret = bq27520_cmd(chip, BQ27520_CMD_ICR, (char*)&data, 0);
	if (ret >= 0)
		chip->prop.current_mA = (int)data;

	return chip->prop.current_mA;
}
EXPORT_SYMBOL(bq27520_battery_current);

unsigned int bq27520_battery_rm(void)
{
	struct bq27520_chip *chip = g_bq_chip;
	int ret = 0;
	u16 data = 0;

	if (!chip) {
		pr_err("[%s] bq27520 chip not ready\n", __func__);
		return 0;
	}

	ret = bq27520_cmd(chip, BQ27520_CMD_RM, (char*)&data, 0);
	if (ret >= 0)
		chip->prop.rm_mAh = data;

	return chip->prop.rm_mAh;
}
EXPORT_SYMBOL(bq27520_battery_rm);

unsigned int bq27520_battery_resistor(void)
{
	struct bq27520_chip *chip = g_bq_chip;
	int ret = 0;
	s16 data = 0;

	if (!chip) {
		pr_err("[%s] bq27520 chip not ready\n", __func__);
		return 0;
	}

	ret = bq27520_cmd(chip, BQ27520_CMD_NIC, (char*)&data, 0);
	if (ret >= 0)
		chip->prop.im_mohm = data;

	return chip->prop.im_mohm;
}
EXPORT_SYMBOL(bq27520_battery_resistor);

void bq27520_notify_from_changer(int charge_fsm_state, int charger_state)
{
	struct bq27520_chip *chip = g_bq_chip;

	if (!chip) {
		pr_err("[%s] bq27520 chip not ready\n", __func__);
		return;
	}

	if ((charge_fsm_state == chip->charge_fsm_state) && (charger_state == chip->charger_state)) 
		return;

	pr_info("[%s] charger notify battery to update status~ charge fsm status: %d charger status: %d\n", __func__, charge_fsm_state, charger_state);
	chip->charge_fsm_state = charge_fsm_state;
	chip->charger_state = charger_state;
	chip->interrupt_type = NOTIFY_FLAG;
	cancel_delayed_work(&chip->monitor_work);
	schedule_delayed_work(&chip->monitor_work, 1);
}
EXPORT_SYMBOL(bq27520_notify_from_changer);

/* ---------------------- Power supply functions ------------------------- */
static int bq27520_battery_get_prop(struct power_supply *psy,
			       enum power_supply_property psp,
			       union power_supply_propval *val)
{
	struct bq27520_chip *chip = container_of(psy, struct bq27520_chip, bat);

	//bq27520_update_status(chip, 0);
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = chip->prop.status;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = chip->prop.health;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = chip->prop.present;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = chip->prop.capacity;
		if (fakecap_enable != 0) //use for dummy battery
			val->intval = 50;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = (chip->prop.voltage_mV * 1000); //uV
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = (chip->prop.current_mA * 1000); //uA
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = chip->prop.temp_C;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = chip->prop.online;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static enum power_supply_property bq27520_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_TEMP,
};

/* ----------------------------- Misc device functions ----------------------------- */
static ssize_t dfi_data_write(struct file *file, const char __user * buf, size_t count, loff_t * ppos)
{
	int ret = 0;
	size_t size = count;

	if (dfi_array == NULL) {
		dfi_array = kzalloc(DFI_SIZE, GFP_KERNEL);
		if (!dfi_array)
			return -1;
	}
	if (size > DFI_SIZE)
		size = DFI_SIZE;

	ret = copy_from_user((dfi_array+*ppos), buf, size);
	if (ret) {
		pr_err("[%s] write to array failed(%d) start at offset 0x%08x\n", __func__, ret, *(unsigned int*)ppos);
		return -2;
	}

	return count;

}

static struct file_operations dfi_data_fops = {
	.owner = THIS_MODULE,
	.write = dfi_data_write,
};

static struct miscdevice dfi_data_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ftm_dfi_data",
	.fops = &dfi_data_fops,
};

static ssize_t row_data_write(struct file *file, const char __user * buf, size_t count, loff_t * ppos)
{
	int ret = 0;
	size_t size = count;

	if (row_if == NULL) {
		row_if = kzalloc(2 * ROW_SIZE, GFP_KERNEL);
		if (row_if == 0)
			return -1;
	} else {
		memset(row_if, 0, 2 * ROW_SIZE);
	}
	if (size > 2 * ROW_SIZE)
		size = 2 * ROW_SIZE;

	ret = copy_from_user((row_if+*ppos), buf, size);
	if (ret) {
		pr_err("[%s] write to row failed(%d) start at offset 0x%08x\n", __func__, ret, *(unsigned int*)ppos);
		return -2;
	}

	return count;
}

static ssize_t row_data_read(struct file *file, char __user *buf, size_t count, loff_t * ppos)
{
	int ret = 0;
	size_t size = count;

	if (row_if == NULL) {
		row_if = kzalloc(2 * ROW_SIZE, GFP_KERNEL);
		if (!row_if)
			return -1;
	}
	if (size > 2 * ROW_SIZE)
		size = 2 * ROW_SIZE;

	ret = copy_to_user(buf, (row_if+*ppos), size);
	if (ret) {
		pr_err("[%s] read from row failed(%d) start at offset 0x%08x\n", __func__, ret, *(unsigned int*)ppos);
		return -2;
	}
	return count;
}

static struct file_operations row_data_fops = {
	.owner = THIS_MODULE,
	.write = row_data_write,
	.read = row_data_read,
};

static struct miscdevice row_data_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ftm_row_data",
	.fops = &row_data_fops,
};

static ssize_t dfi_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	if (g_bq_chip == NULL)
		return 0;

	ret = bq27520_dataflash_cmd(g_bq_chip, SUBCLASS_MANUFACTURE, 0, (char*)&g_bq_chip->m_info, 32, 0);
	if (ret < 0) {
		pr_err("get manufacture information failed\n");
		return -1;
	}

	return sprintf(buf, "%d\n", g_bq_chip->m_info.dfi_version);
}

static int programming_result = 0;
static int run_flag = 0;
static ssize_t programming_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", programming_result);
}

static ssize_t programming_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int ret = -1;
	int cmd = 0;

	if (buf[0] == 'r' || buf[0] =='R') { //reset run_flag
		run_flag = 0;
		return 0;
	}

	if (buf[0] >= 'a' && buf[0] <= 'f')
		cmd = buf[0] - 'a' + 10;
	else if (buf[0] >= 'A' && buf[0] <= 'F')
	  	cmd = buf[0] - 'A' + 10;
	else if (buf[0] >= '0' && buf[0] <= '9')
		cmd = buf[0] - '0';
	else
		return 0;
	
	if ((run_flag & 1<<cmd)>>cmd == 0)
		run_flag |= 1<<cmd;
	else
		return 0;

	switch (cmd) {
	case 0:
		pr_info("Close battery monitor...\n");
		monitor_init_flag = 0;
		run_flag &= ~(1<<10);
		ret = 0;
		break;
	case 1:
	  	pr_info("Enter rom mode...\n");
		ret = bq27520_enter_rom_mode(g_bq_chip);
		if (ret == 0)
			run_flag &= ~(1<<8);
		break;
	case 2:
		pr_info("Read internal row data...\n");
		ret = bq27520_read_if_row(g_bq_chip);
		break;
	case 3:
		pr_info("Erase internal rom data...\n");
		ret = bq27520_erase_if_row(g_bq_chip);
		break;
	case 4:
		pr_info("Erase gasgauge flash...\n");
		ret = bq27520_mass_erase(g_bq_chip);
		break;
	case 5:
		pr_info("Write new dfi to flash...\n");
		ret = bq27520_write_flash(g_bq_chip);
		break;
	case 6:
		pr_info("Check data flash checksum...\n");
		ret = bq27520_check_dficks(g_bq_chip);
		break;
	case 7:
		pr_info("Write back row data...\n");
		ret = bq27520_write_back_row(g_bq_chip);
		break;
	case 8:
		pr_info("Exit rom mode...\n");
		ret = bq27520_exit_rom_mode(g_bq_chip);
		if (ret == 0)
			run_flag &= ~(1<<1);
		break;
	case 9:
		pr_info("Reset gasgauge chip...\n");
		mdelay(3000);
		ret = bq27520_reset_chip(g_bq_chip);
		pr_info("Reset gasgauge chip... ret=%d\n", ret);
		break;
	case 10:
		pr_info("Restart battery monitor...\n");
		monitor_init_flag = 1;
		run_flag &= ~(1<<0);
		ret = 0;
		break;
	default:
		pr_info("Unknow programming command:%s, nothing to do\n", buf);
		break;
	}

	if (ret < 0) {
		programming_result = 0;
		run_flag &= ~(1<<cmd);
	} else {
		programming_result = 1;
	}

	return 0;
}

static DEVICE_ATTR(dfi_version, 0644, dfi_version_show, NULL);
static DEVICE_ATTR(programming, 0644, programming_show, programming_store);

static ssize_t bq27520_fakecap_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "fake=%d\n", fakecap_enable);
}
static ssize_t bq27520_fakecap_store(struct device *dev, 
        struct device_attribute *attr, const char *buf, size_t count)
{
	int value;

	sscanf(buf, "%d", &value);
	fakecap_enable = value;
	return count;
}
static DEVICE_ATTR(fakecap, 0644, bq27520_fakecap_show,  bq27520_fakecap_store);

/* ----------------------------- monitor & irq handler ------------------------ */
static int bq27520_read_status(struct bq27520_chip *chip, int interrupt_type)
{
  	int ret = 0, capacity_org = 0;
	u16 capacity_tran;

	mutex_lock(&chip->read_lock);
	ret = bq27520_battery_flags();
	if (ret == 0) {
		mutex_unlock(&chip->read_lock);
		return -1;
	}

	bq27520_battery_temperature();
	bq27520_battery_current();
	bq27520_battery_voltage();
	bq27520_battery_capacity();
	bq27520_battery_resistor();
	bq27520_battery_control_status();
	//BobIHLee@20120112, add 5% battery 
	#if 0 
	if(chip->prop.capacity<5)
	{
		capacity_org = chip->prop.capacity;
		capacity_tran=0;
		chip->prop.capacity = capacity_tran;
	}
	else if(chip->prop.capacity>=5)
	{
		capacity_org = chip->prop.capacity;
		capacity_tran=(100*(capacity_org-5))/95;
		chip->prop.capacity = capacity_tran;
	}
	else
	{
		capacity_org = chip->prop.capacity;
		capacity_tran=0;
		chip->prop.capacity = capacity_tran;
		pr_info("[%s] capacity Error ORG_capacity:%d capacity:%d\n", __func__,capacity_org, chip->prop.capacity);
	}
	#endif
	#if 1 
	if(chip->prop.capacity<3)
	{
		capacity_org = chip->prop.capacity;
		capacity_tran=0;
		chip->prop.capacity = capacity_tran;
	}
	else if((chip->prop.capacity <= 100)&&(chip->prop.capacity >= 3))
	{
		capacity_org = chip->prop.capacity;
		capacity_tran=(100*(capacity_org-3))/97;
		chip->prop.capacity = capacity_tran;
	}
	else
	{
		capacity_org = chip->prop.capacity;
		capacity_tran=0;
		chip->prop.capacity = capacity_tran;
		pr_info("[%s] capacity Error ORG_capacity:%d capacity:%d\n", __func__,capacity_org, chip->prop.capacity);
	}
	#endif	
	#if 0
	if((chip->prop.capacity <= 100)&&(chip->prop.capacity >= 0))
	{
		capacity_org = chip->prop.capacity;
	}
	else
	{
		capacity_org = chip->prop.capacity;
		capacity_tran=0;
		chip->prop.capacity = capacity_tran;
		pr_info("[%s] capacity Error ORG_capacity:%d capacity:%d\n", __func__,capacity_org, chip->prop.capacity);
	}
	#endif

	if (interrupt_type) {
		pr_info("[%s] flags:0x%04X ORG_capacity:%d capacity:%d tempC:%d(0.1C) Current:%d(mA) voltage:%d(mV) resistor:%d(mohm)\n", __func__, 
		    chip->prop.flags, capacity_org, chip->prop.capacity, chip->prop.temp_C, chip->prop.current_mA, chip->prop.voltage_mV, chip->prop.im_mohm);
		pr_info("[%s] control status: 0x%04X\n", __func__, chip->prop.cntrl_status);
	} else {
		printk(KERN_DEBUG "[%s] flags:0x%04X ORG_capacity:%d capacity:%d tempC:%d(0.1C) Current:%d(mA) voltage:%d(mV) resistor:%d(mohm)\n", __func__, 
		    chip->prop.flags, capacity_org, chip->prop.capacity, chip->prop.temp_C, chip->prop.current_mA, chip->prop.voltage_mV, chip->prop.im_mohm);
		printk(KERN_DEBUG "[%s] control status: 0x%04X\n", __func__, chip->prop.cntrl_status);
	}
	//BobIHLee@20120112, add 5% battery 

	mutex_unlock(&chip->read_lock);
	return 0;
}

static int bq27520_check_temperature_status(struct bq27520_chip *chip, int otp_flag)
{
	static int last_otp_flag = 0;
	static int last_ph_flag = 0;
	static int last_pl_flag = 0;
	int ph_flag = 0;
	int pl_flag = 0;
	int notify_state = NONE_STATE;

	if ((chip->use_partial & PARTIAL_HIGH) && (chip->prop.temp_C >= chip->partial_temp_h))
		ph_flag = 1;

	if ((chip->use_partial & PARTIAL_LOW) && (chip->prop.temp_C <= chip->partial_temp_l))
		pl_flag = 1;

	//battery temp falg is changed or not
	if (otp_flag == 1) { //otp part
		if (last_otp_flag == 0) {
	  		pr_info("[%s] Bq27520 detect overtemperature (%d) of battery\n", __func__, chip->prop.temp_C);
			last_otp_flag = 1;
			last_ph_flag = 0;
			last_pl_flag = 0;
			notify_state = OTP_STATE;
		}
	} else if (ph_flag == 1) { //partial high part
		if (last_ph_flag == 0) {
	  		pr_info("[%s] Bq27520 detect partial high overtemp (%d) of battery\n", __func__, chip->prop.temp_C);
			last_otp_flag = 0;
			last_ph_flag = 1;
			last_pl_flag = 0;
			notify_state = PH_OTP_STATE;
		}
	} else if (pl_flag == 1) { //partial low part
		if (last_pl_flag == 0) {
	  		pr_info("[%s] Bq27520 detect partial low overtemp (%d) of battery\n", __func__, chip->prop.temp_C);
			last_otp_flag = 0;
			last_ph_flag = 0;
			last_pl_flag = 1;
			notify_state = PL_OTP_STATE;
		}
	} else { //normal temperature part
		if (last_otp_flag || last_ph_flag || last_pl_flag) {
			pr_info("[%s] Bq27520 detect normal temperature (%d) of battery\n", __func__, chip->prop.temp_C);
			last_otp_flag = 0;
			last_ph_flag = 0;
			last_pl_flag = 0;
			notify_state = NORMAL_STATE;
		}
	}

	if (notify_state != NONE_STATE) {
#if (RELATED_CHARGER_PART)
		//notify to charger, piny
#endif
	}

	return 0;
}

static void check_bgd_blo_status(struct bq27520_chip *chip)
{
	int gpio_n, gpio_v;

	if (chip->irq_reg_map & BGD_INT_MASK) {
		gpio_n = gpio_v = 0;
	  	gpio_n = chip->bgd_gpio;
		gpio_v = gpio_get_value(gpio_n);
		printk(KERN_DEBUG "[%s] BATT_GD gpio(%d) is %s\n", __func__, gpio_n, (gpio_v?"high":"low"));
		if (chip->bgd_status != gpio_v) {
			//notify charger enable/disable
			chip->bgd_status = gpio_v;
#if (RELATED_CHARGER_PART)
			if (chip->bgd_status == chip->bgd_pol) {
				//has error, disable charger, piny
			  	pr_info("[%s] Has disable charger\n", __func__);

			} else {
				//normally, enable charger, piny
			  	pr_info("[%s] Has enable charger\n", __func__);
			}
#endif
		}
	}

	if (chip->irq_reg_map & BLO_INT_MASK) {
		gpio_n = gpio_v = 0;
	  	gpio_n = chip->blo_gpio;
		gpio_v = gpio_get_value(gpio_n);
		gpio_free(gpio_n);
		printk(KERN_DEBUG "[%s] BATT_LOW gpio(%d) is %s\n", __func__, gpio_n, (gpio_v?"high":"low"));
		if (chip->blo_status != gpio_v) {
			//notify uplayer capacity status
			chip->blo_status = gpio_v;
			if (chip->blo_status == chip->blo_pol) {
				//notify too low cap, piny
				pr_info("[%s] Notify Too Low Capacity\n", __func__);
			} else {
			  	//normal cap, piny
				pr_info("[%s] Back to Normal Capacity\n", __func__);
			}
		}
	}
}

static void bq27520_update_status(struct bq27520_chip *chip, int interrupt_type)
{
    static int last_online = 0;
    static int full_count = 0;
    static unsigned int old_status = 0;
    static unsigned int old_health = 0;
    static unsigned int old_capacity = 0;
    static unsigned int old_tempc = 0;

	int ret = 0, otp_flag = 0;
	char f_bytes[2] = {0, 0};

#if (RELATED_CHARGER_PART)
	/* charger part */
	int charge_fsm_state = chip->charge_fsm_state, charger_state = chip->charger_state;
#endif

	if (monitor_init_flag == 0)
		return;

	if ((!interrupt_type) && chip->update_time && time_before(jiffies, chip->update_time +  msecs_to_jiffies(cache_time)))
		return;

	chip->update_time = jiffies;
	
	//get new infomation
	ret = bq27520_read_status(chip, interrupt_type);
	if (ret < 0) {
		pr_err("[%s] Get flags status error\n", __func__);
		goto end;
	}

	f_bytes[0] = (chip->prop.flags & 0xff);
	f_bytes[1] = ((chip->prop.flags >> 8) & 0xff);

	//get gpio status about battery good and battery low
	check_bgd_blo_status(chip);

	//online and present property
	if (f_bytes[0] & FLAGS_BAT_DET) {
		chip->prop.online = 1;
		chip->prop.present = 1;
		if (!last_online) {
			pr_info("[%s] Bq27520 detect battery is detected\n", __func__);
			last_online = 1;
		}
	} else {
	  	chip->prop.online = 0;
		chip->prop.present = 0;
		if (last_online) {
			pr_info("[%s] Bq27520 detect battery is removed\n", __func__);
			last_online = 0;
		} else {
			pr_info("[%s] No Battery be checked by Bq27520\n", __func__);
		}
	}
	
	//health property
	if (f_bytes[1] & (FLAGS_OTC | FLAGS_OTD | FLAGS_CHG_INH | FLAGS_XCHG)) {
		otp_flag = 1;
		if (chip->prop.temp_C > BQ27520_NORMAL_TEMP)
			chip->prop.health = POWER_SUPPLY_HEALTH_OVERHEAT;
		else
			chip->prop.health = POWER_SUPPLY_HEALTH_COLD;
	} else {
		otp_flag = 0;
	  	chip->prop.health = POWER_SUPPLY_HEALTH_GOOD;
	}
	//overtemperature check
	ret = bq27520_check_temperature_status(chip, otp_flag);

	//status property
	if (!(f_bytes[0] & FLAGS_BAT_DET)) {
		chip->prop.status = POWER_SUPPLY_STATUS_UNKNOWN;
	} else if ((f_bytes[1] & FLAGS_FC) && (full_count >= 5) && (chip->prop.capacity == 100)) {
		chip->prop.status = POWER_SUPPLY_STATUS_FULL;
		full_count = 5;
	} else {
		if ((f_bytes[1] & FLAGS_FC) && (chip->prop.capacity == 100))
			full_count++;
		else
			full_count = 0;

		if ((f_bytes[0] & FLAGS_DSG) || (otp_flag == 1)) {
			chip->prop.status = POWER_SUPPLY_STATUS_DISCHARGING;
		} else {
			chip->prop.status = POWER_SUPPLY_STATUS_CHARGING;
		}
#if (RELATED_CHARGER_PART)
		if (charger_state == 0) {
			chip->prop.status = POWER_SUPPLY_STATUS_DISCHARGING;
		} else {
			if (charge_fsm_state == 0 || otp_flag == 1) {
				chip->prop.status = POWER_SUPPLY_STATUS_NOT_CHARGING;
			} else {
				chip->prop.status = POWER_SUPPLY_STATUS_CHARGING;
			}
		}
#endif
	}

end:
	//power supply change
	if ((old_health != chip->prop.health) || (old_capacity != chip->prop.capacity) || (old_status != chip->prop.status) || (old_tempc != chip->prop.temp_C)) {
		pr_info("[%s] Battery power supply changed\n", __func__);
		power_supply_changed(&chip->bat);
	}

	old_status = chip->prop.status;
	old_health = chip->prop.health;
	old_capacity = chip->prop.capacity;
	old_tempc = chip->prop.temp_C;
	return;
}

static char interrupt_type_name[FLAG_NUM][16] = {
	[NONE_FLAG] = "None",
	[SOC_INT_FLAG] = "SOC",
	[BLO_INT_FLAG] = "BATT_LOW",
	[BGD_INT_FLAG] = "BATT_GD",
	[NOTIFY_FLAG] = "Notify",
};

static void battery_monitor_work(struct work_struct *work)
{
	struct bq27520_chip *chip = container_of(work, struct bq27520_chip, monitor_work.work);
	int interrupt_type = chip->interrupt_type;

	wake_lock(&chip->work_wakelock);
	printk(KERN_DEBUG "[%s] interrupt_type = %s\n", __func__, interrupt_type_name[interrupt_type]);
	chip->interrupt_type = NONE_FLAG;
	bq27520_update_status(chip, interrupt_type);
	wake_unlock(&chip->work_wakelock);

	schedule_delayed_work(&chip->monitor_work, BQ27520_MONITOR_INTERVAL);
}

static irqreturn_t bq27520_irq_handler(int irq, void *data)
{
	struct bq27520_chip *chip = (struct bq27520_chip*)data;

	if (chip->soc_irq == irq) {
		printk(KERN_DEBUG "[%s] SOC interrupt %d\n", __func__, irq);
		chip->interrupt_type = SOC_INT_FLAG;
	} else if (chip->blo_irq == irq) {
		printk(KERN_DEBUG "[%s] BATT LOW interrupt %d\n", __func__, irq);
		chip->interrupt_type = BLO_INT_FLAG;
	} else if (chip->bgd_irq == irq) {
		printk(KERN_DEBUG "[%s] BATT GOOD interrupt %d\n", __func__, irq);
		chip->interrupt_type = BGD_INT_FLAG;
	} else {
		printk(KERN_DEBUG "[%s] Unknow interrupt %d\n", __func__, irq);
		return IRQ_HANDLED;
	}

	cancel_delayed_work(&chip->monitor_work);
	schedule_delayed_work(&chip->monitor_work, 1);
	return IRQ_HANDLED;
}

static int bq27520_set_irq(struct bq27520_platform_data *pdata, struct bq27520_chip *chip)
{
  	int ret = 0;

	//soc_int irq
  	if ((pdata->gpio_map & SOC_INT_MASK) && pdata->soc_gpio) {
	  	chip->soc_irq = gpio_to_irq(pdata->soc_gpio);
		if (chip->soc_irq < 0) {
			pr_err("[%s] Get GPIO-%d to irq failed\n", __func__, pdata->soc_gpio);
			goto failed1;
		}
		ret = request_threaded_irq(chip->soc_irq, NULL, bq27520_irq_handler, IRQF_TRIGGER_FALLING, 
			"bq27520_soc_int", chip);
		if (ret < 0) {
			pr_err("[%s] Failed to request IRQ: #%d: %d\n", __func__, chip->soc_irq, ret);
			goto failed1;
		}
		chip->irq_reg_map |= SOC_INT_MASK;
	}
	//bat_low irq
  	if ((pdata->gpio_map & BLO_INT_MASK) && pdata->blo_gpio) {
		chip->blo_gpio = pdata->blo_gpio;
		ret = gpio_request(chip->blo_gpio, "BAT_LOW");
		if (ret) {
			pr_err("[%s] Failed to request GPIO #%d: %d\n", __func__, chip->blo_gpio, ret);
			goto failed2;
		}
		gpio_direction_input(chip->blo_gpio);
		chip->blo_irq = gpio_to_irq(pdata->blo_gpio);
		if (chip->blo_irq < 0) {
			pr_err("[%s] Get GPIO-%d to irq failed\n", __func__, pdata->blo_gpio);
			goto failed2;
		}
		ret = request_threaded_irq(chip->blo_irq, NULL, bq27520_irq_handler, IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, 
			"bq27520_blo_int", chip);
		if (ret < 0) {
			pr_err("[%s] Failed to request IRQ: #%d: %d\n", __func__, chip->blo_irq, ret);
			goto failed2;
		}
		chip->irq_reg_map |= BLO_INT_MASK;
	}
	//bat_good irq
  	if ((pdata->gpio_map & BGD_INT_MASK) && pdata->bgd_gpio) {
		chip->bgd_gpio = pdata->bgd_gpio;
		ret = gpio_request(chip->bgd_gpio, "BAT_GD");
		if (ret) {
			pr_err("[%s] Failed to request GPIO #%d: %d\n", __func__, chip->bgd_gpio, ret);
			goto failed3;
		}
		gpio_direction_input(chip->bgd_gpio);
		chip->bgd_irq = gpio_to_irq(pdata->bgd_gpio);
		if (chip->bgd_irq < 0) {
			pr_err("[%s] Get GPIO-%d to irq failed\n", __func__, pdata->bgd_gpio);
			goto failed3;
		}
		ret = request_threaded_irq(chip->bgd_irq, NULL, bq27520_irq_handler, IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, 
			"bq27520_bgd_int", chip);
		if (ret < 0) {
			pr_err("[%s] Failed to request IRQ: #%d: %d\n", __func__, chip->bgd_irq, ret);
			goto failed3;
		}
		chip->irq_reg_map |= BGD_INT_MASK;
	}

	return 0;

failed3:
	if (chip->irq_reg_map & BLO_INT_MASK) {
	  	free_irq(chip->blo_irq, chip);
		gpio_free(chip->blo_gpio);
	}

failed2:
	if (chip->irq_reg_map & SOC_INT_MASK)
		free_irq(chip->soc_irq, chip);

failed1:
	return -1;
}

static void bq27520_free_irq(struct bq27520_chip *chip)
{
  	if (chip->irq_reg_map & SOC_INT_MASK) {
		free_irq(chip->soc_irq, chip);
	}
	if (chip->irq_reg_map & BLO_INT_MASK) {
	  	free_irq(chip->blo_irq, chip);
		gpio_free(chip->blo_gpio);
	}
	if (chip->irq_reg_map & BGD_INT_MASK) {
		free_irq(chip->bgd_irq, chip);
		gpio_free(chip->bgd_gpio);
	}
	chip->irq_reg_map = 0;
}

static void bq27520_battery_start(struct bq27520_chip *chip)
{
	int ret = 0;

	//set sleep+ for i2c echo early
	ret = bq27520_control_cmd(chip, CNTL_SET_SLEEP, NULL);
	if (ret < 0)
		pr_err("[%s] set sleep+ failed\n", __func__);
	mdelay(1);

	/* get bq27520 control information */
	ret = bq27520_control_cmd(chip, CNTL_CONTROL_STATUS, (char*)&(chip->prop.cntrl_status));
	if (ret < 0) pr_err("get control status failed\n");
/* Remove this to speed up */
#if 1
	ret = bq27520_control_cmd(chip, CNTL_DEVICE_TYPE, (char*)&(chip->prop.device_type));
	if (ret < 0) pr_err("get device type failed\n");
	ret = bq27520_control_cmd(chip, CNTL_FW_VERSION, (char*)&(chip->prop.fw_version));
	if (ret < 0) pr_err("get  firmware version failed\n");
	ret = bq27520_control_cmd(chip, CNTL_HW_VERSION, (char*)&(chip->prop.hw_version));
	if (ret < 0) pr_err("get hardware version failed\n");
	ret = bq27520_control_cmd(chip, CNTL_DF_CHECKSUM, (char*)&(chip->prop.df_checksum));
	if (ret < 0) pr_err("get data flash checksum failed\n");
	ret = bq27520_control_cmd(chip, CNTL_CHEM_ID, (char*)&(chip->prop.chem_id));
	if (ret < 0) pr_err("get chemical id failed\n");
#endif
	/* set sealed status */
	chip->sealed_status = 0;
	if ((chip->prop.cntrl_status >> 8) & STATUS_SS) {
		pr_info("[%s] Now gas gauge is in Sealed mode\n", __func__);
		chip->sealed_status = 1;
	} else {
		pr_info("[%s] Now gas gauge is in Un-sealed mode\n", __func__);
	}
/* Remove this to speed up */
#if 1 
	/* get bq27520 external information */
	ret = bq27520_cmd(chip, BQ27520_EXT_DCAP, (char*)&(chip->prop.design_capacity), 0);
	if (ret < 0) pr_err("get design capacity failed\n");
	ret = bq27520_cmd(chip, BQ27520_EXT_DNAMELEN, (char*)&(chip->prop.dev_name_len), 0);
	if (ret < 0) pr_err("get device name length failed\n");
	ret = bq27520_cmd(chip, BQ27520_EXT_DNAME, (char*)&(chip->prop.dev_name), 0);
	if (ret < 0) pr_err("get device name failed\n");
	ret = bq27520_cmd(chip, BQ27520_EXT_APPSTAT, (char*)&(chip->prop.app_status), 0);
	if (ret < 0) pr_err("get application status failed\n");
	chip->prop.dev_name[chip->prop.dev_name_len] = '\0';
	/* get bq27520 dataflash some information */
	ret = bq27520_dataflash_cmd(chip, SUBCLASS_MANUFACTURE, 0, (char*)&chip->m_info, 32, 0);
	if (ret < 0) pr_err("get manufacture information failed\n");
	ret = bq27520_dataflash_cmd(chip, SUBCLASS_REGISTER, 0, (char*)&chip->op_cfg, 32, 0);
	if (ret < 0) pr_err("get operation configuration failed\n");
	
	pr_info("[%s] Device:%s\n", __func__, chip->prop.dev_name);
	pr_info("[%s] Design capacity:%dmAh, App status:0x%04X\n", __func__, chip->prop.design_capacity, chip->prop.app_status);
	pr_info("[%s] Control status:0x%04X, Device type:0x%04X\n", __func__, chip->prop.cntrl_status, chip->prop.device_type);
	pr_info("[%s] Firmware version:0x%04X, Hardware version:0x%04X\n", __func__, chip->prop.fw_version, chip->prop.hw_version);
	pr_info("[%s] Dataflash checksum:0x%04X, Chemical id:0x%04X\n", __func__, chip->prop.df_checksum, chip->prop.chem_id);
	pr_info("[%s] DFI version: 0x%02X Project Name: %s\n", __func__, chip->m_info.dfi_version, chip->m_info.prj_name);
	pr_info("[%s] Date: 0x%08X\n", __func__, *(u32*)chip->m_info.date);
	pr_info("[%s] OP A: 0x%04X OP B: 0x%02X\n", __func__, *(u16*)chip->op_cfg.op_a, chip->op_cfg.op_b);
	pr_info("[%s] OP C: 0x%02X SOC Delta: %d\n", __func__, chip->op_cfg.op_c, chip->op_cfg.soc_delta);
#endif
	
	//check battery good and battery low polarity
	chip->bgd_pol = (*(u16*)chip->op_cfg.op_a & 0x0400)? 0 : 1;
	chip->blo_pol = (*(u16*)chip->op_cfg.op_a & 0x0200)? 1 : 0;
	pr_info("[%s] BATT_GD active %s, BATT_LOW active %s\n", __func__, (chip->bgd_pol?"high":"low"), (chip->blo_pol?"high":"low"));

}

/* ----------------------------- Driver functions ----------------------------- */
static int bq27520_battery_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct bq27520_platform_data *pdata = client->dev.platform_data;
  	struct bq27520_chip *chip;
  	int retval, num;

  	pr_info("[%s] start battery bq27520 probe\n", __func__);

	if (!pdata) {
		pr_err("[%s] No platform data for used\n", __func__);
		return -ENOMEM;
	}

	/* Get new ID for the new battery device */
	retval = idr_pre_get(&battery_id, GFP_KERNEL);
	if (retval == 0)
		return -ENOMEM;
	mutex_lock(&battery_mutex);
	retval = idr_get_new(&battery_id, client, &num);
	mutex_unlock(&battery_mutex);
	if (retval < 0) {
	  	pr_err("[%s] Get battery id failed(%d)\n", __func__, retval);
		return retval;
	}

	//original chip data
	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		pr_err("[%s] Failed to allocate chip data\n", __func__);
		retval = -ENOMEM;
		goto idrfree;
	}

	//chip data init
	chip->id = num;
	chip->dev = &client->dev;
	chip->client = client;
	chip->rom_addr = 0;
	chip->irq_reg_map = 0;
	mutex_init(&chip->io_lock);
	mutex_init(&chip->read_lock);
	wake_lock_init(&chip->work_wakelock, WAKE_LOCK_SUSPEND, "bq27520");
	i2c_set_clientdata(client, chip);

	//set partial temperature
	if (pdata->use_partial) {
		chip->use_partial = pdata->use_partial;
		chip->partial_temp_h = pdata->partial_temp_h;
		chip->partial_temp_l = pdata->partial_temp_l;
	}

	//set rom addr
	if (pdata->rom_addr && (pdata->rom_addr != client->addr)) {
		chip->rom_addr = pdata->rom_addr;
		chip->rom_client = i2c_new_dummy(chip->client->adapter, chip->rom_addr);
		if (!chip->rom_client) {
			pr_err("[%s] set rom addr i2c failed\n", __func__);
			retval = -ENODEV;
			goto i2c_free;
		}
		i2c_set_clientdata(chip->rom_client, chip);
	}
	g_bq_chip = chip;

	//start work
	bq27520_battery_start(chip);

	//set irq
	retval = bq27520_set_irq(pdata, chip);
	if (retval < 0) {
		pr_err("[%s] Set bq27520 irq mode failed(%d)\n", __func__, retval);
		goto free_romi2c; 
	}

	retval = misc_register(&dfi_data_miscdev);
 	if (retval < 0) {
		pr_err("[%s] register dfi data misc device failed(%d)\n", __func__, retval);
 		goto free_irq;
	}
	retval = misc_register(&row_data_miscdev);
 	if (retval < 0) {
		pr_err("[%s] register row data misc device failed(%d)\n", __func__, retval);
 		goto out_dfidata;
	}
	retval = device_create_file(&client->dev, &dev_attr_dfi_version);
	if (retval) {
		pr_err("[%s] register dev fs dfi_version failed(%d)\n", __func__, retval);
		goto err_misc;
	}
	retval = device_create_file(&client->dev, &dev_attr_programming);
	if (retval) {
		pr_err("[%s] register dev fs program failed(%d)\n", __func__, retval);
		goto err_dfiv;
	}
	retval = device_create_file(&client->dev, &dev_attr_fakecap);
	if (retval) {
		pr_err("[%s] register dev fs fakecap failed(%d)\n", __func__, retval);
		goto err_prog;
	}

	//init power supply property
	chip->bat.name			= "battery";
	chip->bat.type			= POWER_SUPPLY_TYPE_BATTERY;
	chip->bat.properties		= bq27520_battery_props;
	chip->bat.num_properties	= ARRAY_SIZE(bq27520_battery_props);
	chip->bat.get_property		= bq27520_battery_get_prop;
	chip->prop.status 		= POWER_SUPPLY_STATUS_UNKNOWN;
	chip->prop.health 		= POWER_SUPPLY_HEALTH_GOOD;
	chip->prop.temp_C 		= BQ27520_NORMAL_TEMP;
	INIT_DELAYED_WORK(&chip->monitor_work, battery_monitor_work);
	monitor_init_flag = 1;
	chip->interrupt_type = NONE_FLAG;
	
	retval = power_supply_register(chip->dev, &chip->bat); //create battery sysfs
	if (retval) {
		pr_err("[%s] Failed to register battery:(%d)\n", __func__, retval);
		goto err_fake;
	}

	schedule_delayed_work(&chip->monitor_work, HZ * 1);

	return 0;

err_fake:
	device_remove_file(&client->dev, &dev_attr_fakecap);
err_prog:
	device_remove_file(&client->dev, &dev_attr_programming);
err_dfiv:
	device_remove_file(&client->dev, &dev_attr_dfi_version);
err_misc:
	misc_deregister(&row_data_miscdev);
out_dfidata:
	misc_deregister(&dfi_data_miscdev);
free_irq:
	bq27520_free_irq(chip);
free_romi2c:
	g_bq_chip = NULL;
	if (chip->rom_addr) {
	  	chip->rom_addr = 0;
		i2c_unregister_device(chip->rom_client);
		i2c_set_clientdata(chip->rom_client, NULL);
	}
i2c_free:
	i2c_set_clientdata(client, NULL);
	wake_lock_destroy(&chip->work_wakelock);
	kfree(chip);
idrfree:
	mutex_lock(&battery_mutex);
	idr_remove(&battery_id, num);
	mutex_unlock(&battery_mutex);

	return retval;
}

static int bq27520_battery_remove(struct i2c_client *client)
{
  	struct bq27520_chip *chip = i2c_get_clientdata(client);
	int num = chip->id, ret = 0;
  	pr_info("[%s] start battery bq27520 remove\n", __func__);

	//clear sleep+ to reduce consumption
	ret = bq27520_control_cmd(chip, CNTL_CLEAR_SLEEP, NULL);
	if (ret < 0)
		pr_err("[%s] clear sleep+ failed\n", __func__);
	mdelay(1);

	bq27520_free_irq(chip);
	power_supply_unregister(&chip->bat);
	device_remove_file(&client->dev, &dev_attr_fakecap);
	device_remove_file(&client->dev, &dev_attr_programming);
	device_remove_file(&client->dev, &dev_attr_dfi_version);
	misc_deregister(&row_data_miscdev);
	misc_deregister(&dfi_data_miscdev);
	cancel_delayed_work(&chip->monitor_work);
	g_bq_chip = NULL;
	if (chip->rom_addr) {
	  	chip->rom_addr = 0;
		i2c_unregister_device(chip->rom_client);
		i2c_set_clientdata(chip->rom_client, NULL);
	}
	i2c_set_clientdata(client, NULL);
	wake_lock_destroy(&chip->work_wakelock);
	kfree(chip);
	mutex_lock(&battery_mutex);
	idr_remove(&battery_id, num);
	mutex_unlock(&battery_mutex);

	return 0;
}

#ifdef CONFIG_PM
static int bq27520_battery_suspend(struct i2c_client *client, pm_message_t state)
{
  	struct bq27520_chip *chip = i2c_get_clientdata(client);
  	pr_info("[%s] start battery bq27520 suspend\n", __func__);
	disable_irq(chip->soc_irq);
	cancel_delayed_work(&chip->monitor_work);
	flush_delayed_work(&chip->monitor_work);
	return 0;
}

static int bq27520_battery_resume(struct i2c_client *client)
{
  	struct bq27520_chip *chip = i2c_get_clientdata(client);
  	pr_info("[%s] start battery bq27520 resume\n", __func__);
	schedule_delayed_work(&chip->monitor_work, 1);
	enable_irq(chip->soc_irq);
	return 0;
}
#else
#define bq27520_battery_suspend	NULL
#define bq27520_battery_resume	NULL
#endif

static const struct i2c_device_id bq27520_id[] = {
	{ "bq27520_battery", 0 },
	{},
};


static struct i2c_driver bq27520_battery_driver = {
	.driver = {
		.name = "bq27520_battery",
	},
	.id_table	= bq27520_id,
	.probe		= bq27520_battery_probe,
	.remove		= bq27520_battery_remove,
	.suspend	= bq27520_battery_suspend,
	.resume		= bq27520_battery_resume,
};

static int __init bq27520_battery_init(void)
{
	return i2c_add_driver(&bq27520_battery_driver);
}

static void __exit bq27520_battery_exit(void)
{
	i2c_del_driver(&bq27520_battery_driver);
}

module_init(bq27520_battery_init);
module_exit(bq27520_battery_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("PinyCHWu <pinychwu@fihtdc.com>");
MODULE_DESCRIPTION("bq27520 battery driver");
