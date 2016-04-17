/*
 * ALSA SoC TWL6040 codec driver
 *
 * Author:	 Misael Lopez Cruz <x0052729@ti.com>
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
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/i2c/twl.h>
#include <linux/switch.h>
#include <linux/mfd/twl6040-codec.h>
#include <linux/regulator/consumer.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include "twl6040.h"

extern int twl6040_read_reg_volatile(struct snd_soc_codec *codec, unsigned int reg);

struct snd_soc_codec *g_twl6040_codec = NULL;

static ssize_t twl6040_headset_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int status = 0;
	int reg;

	#define TWL6040_HEADSET_PLUGIN	0x1

	printk("twl6040_headset_show()\n");

	if (g_twl6040_codec == NULL) {	
		pr_err("[%s] chip data is not ready\n", __func__);
		return 0;	
	}

	reg = twl6040_read_reg_volatile(g_twl6040_codec, TWL6040_REG_STATUS);

	if (reg & TWL6040_PLUGCOMP) {
		status |= TWL6040_HEADSET_PLUGIN;
	}

	return sprintf(buf, "%x\n", status);
}

static DEVICE_ATTR(headset, 0644, twl6040_headset_show, NULL);

int fih_twl6040_probe(struct snd_soc_codec *codec)
{
	int ret = 0;

	g_twl6040_codec = codec;

	//Add sys device
	ret = device_create_file(codec->dev, &dev_attr_headset);
	if (ret != 0) {
		pr_err("[%s] Failed to register devive %s:(%d)\n", __func__, dev_attr_headset.attr.name, ret);
	}

	return ret;
}

int fih_twl6040_remove(struct snd_soc_codec *codec)
{
	device_remove_file(codec->dev, &dev_attr_headset);
	g_twl6040_codec = NULL;

	return 0;
}
