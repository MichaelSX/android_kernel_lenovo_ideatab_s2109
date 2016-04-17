/*
 * fih_twl6030-irq.c
 *
 * Copyright (C) 2011, BokeeLi <BokeeLi@fih-foxconn.com>
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 */


#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kthread.h>
#include <linux/i2c/twl.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <linux/reboot.h>

#include "twl-core.h"

void fih_twl6030_mmc_card_detect_config(u8 *reg_val)
{
	*reg_val = ((*reg_val) | (MMC_PU));
	*reg_val = ((*reg_val) & (~MMC_PD));
}
