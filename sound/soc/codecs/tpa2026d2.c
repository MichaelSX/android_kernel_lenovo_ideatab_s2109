/*
 * Driver for amplifier with SUMMIT TPA2026D2 chips inside.
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
 *	    September 2011
 */

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <sound/soc.h>

#include <fih/resources/audio/fih_amplifier/tpa2026d2-plat.h>
#include <fih/resources/audio/fih_amplifier/tpa2026d2.h>

#define TPA2026D2_POWERED_BY_BOOST

static struct i2c_client *tpa2026d2_client;

/* This struct is used to save the context */
struct tpa2026d2_data {
	struct mutex mutex;
	unsigned char regs[TPA2026D2_CACHEREGNUM];
#ifdef TPA2026D2_POWERED_BY_BOOST
	int boost_gpio;
#endif
	int power_gpio;
	u8 power_state:1;
	int channel_R;
};

static int speaker_TI_default[8] = {
	0, 0x23, 0x05, 0x0b, 0x00, 0x06, 0x3a, 0xc2,	//TI TPA2026D2 default
};

static int speaker_default[8] = {
	0, 0x23, 0x03, 0x0a, 0x00, 0x1a, 0x3c, 0xc0,	//TPA2026D2 default
};

static int speaker_music_DRC[8] = {
	0, 0xc3, 0x03, 0x06, 0x00, 0x09, 0x3c, 0xc1,	//for Speaker Music in DRC function
};

//TI AUDIO AMP - TPA2026D2 (Compression Ratio = 1:1) : 
static int speaker_music[8] = {
	0, 0xc3, 0x03, 0x0a, 0x00, 0x1a, 0x3c, 0xc0,	//for Speaker Music in non-DRC function
};

static int speaker_voice[8] = {
	0, 0x43, 0x02, 0x0a, 0x00, 0x1a, 0x3c, 0xc0,	//for Speaker Voice
};

static int tpa2026d2_i2c_read(unsigned char reg)
{
	struct tpa2026d2_data *data;
	int val;

	BUG_ON(tpa2026d2_client == NULL);
	data = i2c_get_clientdata(tpa2026d2_client);

	/* If powered off, return the cached value */
	if (data->power_state) {
		val = i2c_smbus_read_byte_data(tpa2026d2_client, reg);
		if (val < 0)
			dev_err(&tpa2026d2_client->dev, "Read failed, reg=%d\n", reg);
		else
			data->regs[reg] = val;
	} else {
		val = data->regs[reg];
	}

	return val;
}

static int tpa2026d2_i2c_write(unsigned char reg, unsigned char value)
{
	struct tpa2026d2_data *data;
	int val = 0;

	BUG_ON(tpa2026d2_client == NULL);
	data = i2c_get_clientdata(tpa2026d2_client);

	if (data->power_state) {
		val = i2c_smbus_write_byte_data(tpa2026d2_client, reg, value);
		if (val < 0) {
			dev_err(&tpa2026d2_client->dev, "Write failed, reg=%d\n", reg);
			return val;
		}
	}

	/* Either powered on or off, we save the context */
	data->regs[reg] = value;

	return val;
}

static u8 tpa2026d2_read(int reg)
{
	struct tpa2026d2_data *data;

	BUG_ON(tpa2026d2_client == NULL);
	data = i2c_get_clientdata(tpa2026d2_client);

	return data->regs[reg];
}

static int tpa2026d2_initialize(void)
{
	struct tpa2026d2_data *data;
	int i, ret = 0;

	BUG_ON(tpa2026d2_client == NULL);
	data = i2c_get_clientdata(tpa2026d2_client);

	for (i = 1; i < TPA2026D2_CACHEREGNUM; i++) {
		ret = tpa2026d2_i2c_write(i, data->regs[i]);
		if (ret < 0)
			break;
	}

	return ret;
}

static int tpa2026d2_power(u8 power)
{
	struct	tpa2026d2_data *data;
	u8	val;
	int	ret = 0;

	BUG_ON(tpa2026d2_client == NULL);
	data = i2c_get_clientdata(tpa2026d2_client);

	mutex_lock(&data->mutex);
	if (power == data->power_state)
		goto exit;

	if (power) {
#ifdef TPA2026D2_POWERED_BY_BOOST
		if (data->boost_gpio >= 0)
			gpio_set_value(data->boost_gpio, 1);
#endif

		/* Power on */
		if (data->power_gpio >= 0)
			gpio_set_value(data->power_gpio, 1);

		schedule_timeout_interruptible(msecs_to_jiffies(1));

		data->power_state = 1;
		ret = tpa2026d2_initialize();
		if (ret < 0) {
			dev_err(&tpa2026d2_client->dev,
				"Failed to initialize chip\n");
			if (data->power_gpio >= 0)
				gpio_set_value(data->power_gpio, 0);
#ifdef TPA2026D2_POWERED_BY_BOOST
			if (data->boost_gpio >= 0)
				gpio_set_value(data->boost_gpio, 0);
#endif
			data->power_state = 0;
			goto exit;
		}
	} else {
		/* set SWS */
		val = tpa2026d2_read(TPA2026D2_REG_IC_FUNCTION_CONTROL);
		val |= TPA2026D2_SWS;
		tpa2026d2_i2c_write(TPA2026D2_REG_IC_FUNCTION_CONTROL, val);

		/* Power off */
		if (data->power_gpio >= 0)
			gpio_set_value(data->power_gpio, 0);

#ifdef TPA2026D2_POWERED_BY_BOOST
		if (data->boost_gpio >= 0)
			gpio_set_value(data->boost_gpio, 0);
#endif

		data->power_state = 0;
	}

exit:
	mutex_unlock(&data->mutex);
	return ret;
}

static ssize_t tpa2026d2_amplifier_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tpa2026d2_data *data;
	int ret = 0;

	BUG_ON(tpa2026d2_client == NULL);
	data = i2c_get_clientdata(tpa2026d2_client);

	mutex_lock(&data->mutex);
	ret += sprintf(buf + ret, "Register:\n");
	ret += sprintf(buf + ret, "  IC_FUNCTION_CONTROL    =0x%02X\n", data->regs[TPA2026D2_REG_IC_FUNCTION_CONTROL]);
	ret += sprintf(buf + ret, "  AGC_ATTACK_CONTROL     =0x%02X\n", data->regs[TPA2026D2_REG_AGC_ATTACK_CONTROL]);
	ret += sprintf(buf + ret, "  AGC_RELEASE_CONTROL    =0x%02X\n", data->regs[TPA2026D2_REG_AGC_RELEASE_CONTROL]);
	ret += sprintf(buf + ret, "  AGC_HOLD_TIME_CONTROL  =0x%02X\n", data->regs[TPA2026D2_REG_AGC_HOLD_TIME_CONTROL]);
	ret += sprintf(buf + ret, "  AGC_FIXED_GAIN_CONTROL =0x%02X\n", data->regs[TPA2026D2_REG_AGC_FIXED_GAIN_CONTROL]);
	ret += sprintf(buf + ret, "  AGC_CONTROL_1          =0x%02X\n", data->regs[TPA2026D2_REG_AGC_CONTROL_1]);
	ret += sprintf(buf + ret, "  AGC_CONTROL_2          =0x%02X\n", data->regs[TPA2026D2_REG_AGC_CONTROL_2]);
	ret += sprintf(buf + ret, "Driver:\n");
#ifdef TPA2026D2_POWERED_BY_BOOST
	ret += sprintf(buf + ret, "  boost_gpio = %d\n", data->boost_gpio);
#endif
	ret += sprintf(buf + ret, "  power_gpio = %d\n", data->power_gpio);
	ret += sprintf(buf + ret, "  power_state = 0x%02X\n", data->power_state);
	mutex_unlock(&data->mutex);

	return ret;
}

static ssize_t tpa2026d2_amplifier_store(struct device *dev, 
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct tpa2026d2_data *data;
	//unsigned int reg = 0, value = 0;
	unsigned int arg1 = 0, arg2 = 0;
	char cmd = 0;
	int iCount = 1;
	int ret = 0;
	int *reg_map;

	BUG_ON(tpa2026d2_client == NULL);
	data = i2c_get_clientdata(tpa2026d2_client);

	if (sscanf(buf, "%c,%x,%x", &cmd, &arg1, &arg2) <= 0) {
		pr_err("[%s] get user-space data failed\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&data->mutex);
	if (cmd == 'w' || cmd == 'W') { //write
	    unsigned int reg = arg1, value = arg2;
    	if ((reg < TPA2026D2_REG_IC_FUNCTION_CONTROL) || (reg > TPA2026D2_REG_AGC_CONTROL_2)) {
    		pr_err("[%s] reg(0x%02X) is out of range\n", __func__, reg);
    		mutex_unlock(&data->mutex);
    		return -EINVAL;
    	}
		ret = tpa2026d2_i2c_write((unsigned char)reg, (unsigned char)value);

		if (ret < 0) {
			pr_err("[%s] write reg(0x%02X)=0x%02X failed\n", __func__, (unsigned char)reg, value);
			mutex_unlock(&data->mutex);
			return -EINVAL;
		}
		pr_info("[%s] write reg(0x%02X)=0x%02X done\n", __func__, (unsigned char)reg, value);
	} else if (cmd == 's' || cmd == 'S') { //set
	    unsigned int value = arg1;
		if (value == 0x1) {
			reg_map = speaker_default;
			data->channel_R = 1; //Reset Channel R
			pr_info("[%s] Set to speaker default\n", __func__);
		} else if (value == 0x2) {
			reg_map = speaker_music;
			//reg_map = speaker_music_DRC; 
			data->channel_R = 1; //Reset Channel R
			pr_info("[%s] Set to speaker music\n", __func__);
		} else if (value == 0x3) {
			reg_map = speaker_voice;
			//data->channel_R = 0; //Set Channel R to disable
			pr_info("[%s] Set to speaker voice\n", __func__);
		} else {
			pr_err("[%s] set speaker failed\n", __func__);
			mutex_unlock(&data->mutex);
			return -EINVAL;
		}

		for (iCount = TPA2026D2_REG_AGC_ATTACK_CONTROL; iCount <= TPA2026D2_REG_AGC_CONTROL_2; iCount++) {
			ret = tpa2026d2_i2c_write((unsigned char)iCount, reg_map[iCount]);
		
			if (ret < 0) {
				pr_err("[%s] write reg(0x%02X)=0x%02X failed\n", __func__, (unsigned char)iCount, reg_map[iCount]);
				mutex_unlock(&data->mutex);
				return -EINVAL;
			}
			pr_info("[%s] write reg(0x%02X)=0x%02X done\n", __func__, (unsigned char)iCount, reg_map[iCount]);
		}
	} else if (cmd == 'c' || cmd == 'C') { //Channel
	    unsigned int value = arg1;
	    data->channel_R = !!value; //Set Channel R to disable
	    
	    if(data->power_state){
        	u8	val;
        	/* Enable channel */
        	/* Enable amplifier */
        	val = tpa2026d2_read(TPA2026D2_REG_IC_FUNCTION_CONTROL);
        	val &= ~(TPA2026D2_SPK_EN_R | TPA2026D2_SPK_EN_L);
        	val |= (data->channel_R)? TPA2026D2_SPK_EN_R | TPA2026D2_SPK_EN_L:TPA2026D2_SPK_EN_L;
        	tpa2026d2_i2c_write(TPA2026D2_REG_IC_FUNCTION_CONTROL, val);
		}
		pr_info("[%s] set Channel R is (%d) done\n", __func__, value);
	} else {
		pr_info("[%s] Unknow command (%c)\n", __func__, cmd);
	}
	mutex_unlock(&data->mutex);

	return count;
}

static DEVICE_ATTR(amplifier, 0644, tpa2026d2_amplifier_show,  tpa2026d2_amplifier_store);

/*
 * Enable or disable channel (left or right)
 * The bit number for mute and amplifier are the same per channel:
 * bit 6: Left channel
 * bit 7: Right channel
 * in both registers.
 */
static void tpa2026d2_channel_enable(u8 channel, int enable)
{
	u8	val;

	if (enable) {
		/* Enable channel */
		/* Enable amplifier */
		val = tpa2026d2_read(TPA2026D2_REG_IC_FUNCTION_CONTROL);
		val |= channel;
		val &= ~TPA2026D2_SWS;
		tpa2026d2_i2c_write(TPA2026D2_REG_IC_FUNCTION_CONTROL, val);
	} else {
		/* Disable channel */
		/* Disable amplifier */
		val = tpa2026d2_read(TPA2026D2_REG_IC_FUNCTION_CONTROL);
		val &= ~channel;
		tpa2026d2_i2c_write(TPA2026D2_REG_IC_FUNCTION_CONTROL, val);
	}
}

int tpa2026d2_stereo_enable(struct snd_soc_codec *codec, int enable)
{
	int ret = 0;
	struct	tpa2026d2_data *data;

	BUG_ON(tpa2026d2_client == NULL);
	data = i2c_get_clientdata(tpa2026d2_client);
	
	if (enable) {
		ret = tpa2026d2_power(1);
		if (ret < 0)
			return ret;
		tpa2026d2_channel_enable((data->channel_R)? TPA2026D2_SPK_EN_R | TPA2026D2_SPK_EN_L:TPA2026D2_SPK_EN_L,
					 1);
	} else {
		tpa2026d2_channel_enable(TPA2026D2_SPK_EN_R | TPA2026D2_SPK_EN_L,
					 0);
		ret = tpa2026d2_power(0);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(tpa2026d2_stereo_enable);

static int __devinit tpa2026d2_probe(struct i2c_client *client,
				     const struct i2c_device_id *id)
{
	struct device *dev;
	struct tpa2026d2_data *data;
	struct tpa2026d2_platform_data *pdata;
	int ret;

	dev = &client->dev;

	if (client->dev.platform_data == NULL) {
		dev_err(dev, "Platform data not set\n");
		dump_stack();
		return -ENODEV;
	}

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (data == NULL) {
		dev_err(dev, "Can not allocate memory\n");
		return -ENOMEM;
	}

	tpa2026d2_client = client;

	i2c_set_clientdata(tpa2026d2_client, data);

	pdata = client->dev.platform_data;
	data->power_gpio = pdata->power_gpio;
#ifdef TPA2026D2_POWERED_BY_BOOST
	data->boost_gpio = pdata->boost_gpio;
#endif

	mutex_init(&data->mutex);

    data->channel_R = 1; //Reset Channel R

	/* Set default register values */
	data->regs[TPA2026D2_REG_IC_FUNCTION_CONTROL] = speaker_default[1];
	data->regs[TPA2026D2_REG_AGC_ATTACK_CONTROL] = speaker_default[2];
	data->regs[TPA2026D2_REG_AGC_RELEASE_CONTROL] = speaker_default[3];
	data->regs[TPA2026D2_REG_AGC_HOLD_TIME_CONTROL] = speaker_default[4];
	data->regs[TPA2026D2_REG_AGC_FIXED_GAIN_CONTROL] = speaker_default[5];
	data->regs[TPA2026D2_REG_AGC_CONTROL_1] = speaker_default[6];
	data->regs[TPA2026D2_REG_AGC_CONTROL_2] = speaker_default[7];

	dev_info(dev, "Request power GPIO (%d)\n", data->power_gpio);
	if (data->power_gpio >= 0) {
		ret = gpio_request(data->power_gpio, "tpa2026d2 enable");
		if (ret < 0) {
			dev_err(dev, "Failed to request power GPIO (%d)\n",
				data->power_gpio);
			goto err_power_gpio;
		}
		gpio_direction_output(data->power_gpio, 0);
	}

#ifdef TPA2026D2_POWERED_BY_BOOST
	dev_info(dev, "Request boost GPIO (%d)\n", data->boost_gpio);
	if (data->boost_gpio >= 0) {
		ret = gpio_request(data->boost_gpio, "tpa2026d2 boost");
		if (ret < 0) {
			dev_err(dev, "Failed to request boost GPIO (%d)\n",
				data->boost_gpio);
			goto err_boost_gpio;
		}
		gpio_direction_output(data->boost_gpio, 0);
	}
#endif

	//sys device
	ret = device_create_file(&client->dev, &dev_attr_amplifier);
	if (ret) {
		dev_err(dev, "Failed to register devive %s:(%d)\n", dev_attr_amplifier.attr.name, ret);
		goto err_sysdev;
	}

	ret = tpa2026d2_power(0);
	if (ret != 0)
		goto err_power;

	return 0;

err_power:
	device_remove_file(&client->dev, &dev_attr_amplifier);
err_sysdev:
#ifdef TPA2026D2_POWERED_BY_BOOST
	if (data->boost_gpio >= 0)
		gpio_free(data->boost_gpio);
err_boost_gpio:
#endif
	if (data->power_gpio >= 0)
		gpio_free(data->power_gpio);
err_power_gpio:
	kfree(data);
	i2c_set_clientdata(tpa2026d2_client, NULL);
	tpa2026d2_client = NULL;

	return ret;
}

static int __devexit tpa2026d2_remove(struct i2c_client *client)
{
	struct tpa2026d2_data *data = i2c_get_clientdata(client);

	tpa2026d2_power(0);

	if (data->power_gpio >= 0)
		gpio_free(data->power_gpio);

#ifdef TPA2026D2_POWERED_BY_BOOST
	if (data->boost_gpio >= 0)
		gpio_free(data->boost_gpio);
#endif

	device_remove_file(&client->dev, &dev_attr_amplifier);

	kfree(data);
	tpa2026d2_client = NULL;

	return 0;
}

static const struct i2c_device_id tpa2026d2_id[] = {
	{ "tpa2026d2", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tpa2026d2_id);

static struct i2c_driver tpa2026d2_i2c_driver = {
	.driver = {
		.name = "tpa2026d2",
		.owner = THIS_MODULE,
	},
	.probe = tpa2026d2_probe,
	.remove = __devexit_p(tpa2026d2_remove),
	.id_table = tpa2026d2_id,
};

static int __init tpa2026d2_init(void)
{
	return i2c_add_driver(&tpa2026d2_i2c_driver);
}

static void __exit tpa2026d2_exit(void)
{
	i2c_del_driver(&tpa2026d2_i2c_driver);
}

MODULE_AUTHOR("AlvinLi <alvinli@fih-foxconn.com>");
MODULE_DESCRIPTION("TPA2026D2 amplifier driver");
MODULE_LICENSE("GPL");

module_init(tpa2026d2_init);
module_exit(tpa2026d2_exit);