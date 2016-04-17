/*
 * Functions for FIH HWID informations.
 *
 * Copyright (C) 2011 Foxconn.
 *
 * Author:  PinyCHWu <pinychwu@fih-foxconn.com>
 *	    September 2011
 */

#include <linux/module.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/skbuff.h>
#include <linux/miscdevice.h>
#include <linux/device.h>
#include <linux/sysdev.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/consumer.h>
#include <asm/setup.h>
#include <linux/fih_hw_info.h>

#ifdef CONFIG_TWL6030_GPADC
#include <linux/i2c/twl6030-gpadc.h>
#endif

/* ------------------- OMAP HWID Table ------------------- */
/* ----------------------------------------------------------------------------
    member 1 | member 2 | member 3 | member 4 | member 5 | member 6 | member 7
   ----------------------------------------------------------------------------
     HWID1   |   HWID2  |   HWID3  |  PRO_ID  | Phase_ID |  Band_ID |   Name
   ---------------------------------------------------------------------------- */
static fih_hwid_entry fih_hwid_table[] =
{
// GOX series
	{ 1, 1, 1, Product_GOX, Product_PR1, FIH_NO_BAND, "HWID-GOX_PR1_NO_BAND\r\n" },
	{ 1, 2, 1, Product_GOX, Product_PR1p1, FIH_NO_BAND, "HWID-GOX_PR1.1_NO_BAND\r\n" },
	{ 1, 3, 1, Product_GOX, Product_PR2, FIH_NO_BAND, "HWID-GOX_PR2_NO_BAND\r\n" },
	{ 1, 4, 1, Product_GOX, Product_PR3, FIH_NO_BAND, "HWID-GOX_PR3_NO_BAND\r\n" },

// To be continued....
};

static fih_id_to_str productid_str[] = {
	{Product_Unknow,"Unknow"},
	{Product_GOX,	"GOX"},
};

static fih_id_to_str phaseid_str[] = {
	{Product_PR_Unknow,	"Unknow"},
	{Product_EVB,		"EVB"},
	{Product_PR1,		"PR1"},
	{Product_PR1p1,		"PR1p1"},
	{Product_PR2,		"PR2"},
	{Product_PR3,		"PR3"},
	{Product_PR4,		"PR4"},
	{Product_PR5,		"PR5"},
	{Product_PCR,		"PCR"},
};

static fih_id_to_str bandid_str[] = {
	{FIH_NO_BAND,				"NO BAND"},
// GSM Quad-band + WCDMA
	{FIH_BAND_W1245,			"WCDMA_BAND_1245"},
	{FIH_BAND_W1248,			"WCDMA_BAND_1248"},
	{FIH_BAND_W125,				"WCDMA_BAND_125"},
	{FIH_BAND_W128,				"WCDMA_BAND_128"},
	{FIH_BAND_W25,				"WCDMA_BAND_25"},
	{FIH_BAND_W18,				"WCDMA_BAND_18"},
	{FIH_BAND_W15,				"WCDMA_BAND_15"},
// CDMA (and EVDO) only
	{FIH_BAND_C01,				"CDMA_BAND_0_1"},
	{FIH_BAND_C0,				"CDMA_BAND_0"},
	{FIH_BAND_C1,				"CDMA_BAND_1"},
	{FIH_BAND_C01_AWS,			"CDMA_BAND_0_1_AWS"},
// Multimode + Quad-band
	{FIH_BAND_W1245_C01,			"WCDMA_BAND_1245,CDMA_BAND_01"},
// GSM Tri-band + WCDMA
	{FIH_BAND_W1245_G_850_1800_1900,	"WCDMA_BAND_1245,GSM_BAND_850_1800_1900"},
	{FIH_BAND_W1248_G_900_1800_1900,	"WCDMA_BAND_1248,GSM_BAND_900_1800_1900"},
	{FIH_BAND_W125_G_850_1800_1900,		"WCDMA_BAND_125,GSM_BAND_850_1800_1900"},
	{FIH_BAND_W128_G_900_1800_1900,		"WCDMA_BAND_125,GSM_BAND_900_1800_1900"},
	{FIH_BAND_W25_G_850_1800_1900,		"WCDMA_BAND_25,GSM_BAND_850_1800_1900"},
	{FIH_BAND_W18_G_900_1800_1900,		"WCDMA_BAND_18,GSM_BAND_900_1800_1900"},
	{FIH_BAND_W15_G_850_1800_1900,		"WCDMA_BAND_15,GSM_BAND_850_1800_1900"},
	{FIH_BAND_W1_XI_G_900_1800_1900,	"WCDMA_BAND_1_XI,GSM_BAND_900_1800_1900"},
};

/* ------------------- general use ------------------- */
static unsigned int fih_product_id = Product_Unknow;
static unsigned int fih_product_phase = Product_PR_Unknow;
static unsigned int fih_band_id = FIH_NO_BAND;

unsigned int fih_get_product_id(void)
{
	return fih_product_id;
}
EXPORT_SYMBOL(fih_get_product_id);

unsigned int fih_get_product_phase(void)
{
	return fih_product_phase;
}
EXPORT_SYMBOL(fih_get_product_phase);

unsigned int fih_get_band_id(void)
{
	return fih_band_id;
}
EXPORT_SYMBOL(fih_get_band_id);

//only for setup.c to set id value from u-boot tags.
void fih_set_product_id(unsigned int product_id)
{
	fih_product_id = product_id;
}
EXPORT_SYMBOL(fih_set_product_id);

void fih_set_product_phase(unsigned int phase_id)
{
	fih_product_phase = phase_id;
}
EXPORT_SYMBOL(fih_set_product_phase);

void fih_set_band_id(unsigned int band_id)
{
	fih_band_id = band_id;
}
EXPORT_SYMBOL(fih_set_band_id);

int __init parse_tag_fih_hwid(const struct tag *tag)
{
	fih_set_product_id(tag->u.fih_hwid.product_id);
	fih_set_product_phase(tag->u.fih_hwid.phase_id);
	fih_set_band_id(tag->u.fih_hwid.band_id);
	printk("FIH ProductID:%d, PhaseID:%d, BandID:%d\n", fih_get_product_id(), fih_get_product_phase(),
	    						fih_get_band_id());
	return 0;
}

/* ------------------- ADC tranfer function ------------------- */
#if 0 //this all would get from u-boot, thus disable in kernel.
static unsigned int adc_max_vol = 0;
static unsigned int adc_max_resol = 0;
static unsigned int hwid_index[] = {0, 0, 0};
static unsigned int hwid_value[] = {0, 0, 0};
static int hwid_adc_channel[] = {
	1,	//for product id
	4,	//for phase id
	6,	//for band id
};

static void find_hwid_table(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(fih_hwid_table); i++) {
		if ((hwid_index[0] == fih_hwid_table[i].hwid1_index) &&
		    (hwid_index[1] == fih_hwid_table[i].hwid2_index) &&
		    (hwid_index[2] == fih_hwid_table[i].hwid3_index)) {
			fih_product_id = fih_hwid_table[i].virtual_project_id_value;
			fih_product_phase = fih_hwid_table[i].virtual_phase_id_value;
			fih_band_id = fih_hwid_table[i].virtual_band_id_value;
			printk("[%s] HWID: %s\n", __func__, fih_hwid_table[i].name_string);
			break;
		}
	}

	if (i >= ARRAY_SIZE(fih_hwid_table)) {
		printk("[%s] HWID: Not match any item in table\n", __func__);
	}
}

static int transfer_adc_index(unsigned int adc_value)
{
	unsigned int adc_vol = (adc_value * adc_max_vol / (1 << adc_max_resol));
	unsigned int temp_index = 0;

	if (adc_vol >= 100 && adc_vol < 300)
		temp_index = 1;
	else if (adc_vol >= 300 && adc_vol < 500)
		temp_index = 2;
	else if ( adc_vol >= 500 && adc_vol < 650)
		temp_index = 3;
	else if ( adc_vol >= 650 && adc_vol < 800) //Add for PR3
		temp_index = 4;
	/* others TBD */
	else
		temp_index = 999;

	return temp_index;
}

static int get_hwid_adc(unsigned int index)
{
	int val = 0;
	struct twl6030_gpadc_request req;

	if (index >= ARRAY_SIZE(hwid_adc_channel)) {
		printk("[%s] HWID INDEX (%u) out range\n", __func__, index);
		return 0;
	}

	req.channels	= (1 << hwid_adc_channel[index]);
	req.method	= TWL6030_GPADC_SW2;
	req.func_cb	= NULL;

	val = twl6030_gpadc_conversion(&req);
	if (likely(val > 0)) {
		printk("[%s] HWID%d = 0x%04X\n", __func__, index+1, (u16)req.rbuf[hwid_adc_channel[index]]);
		return ((u16)req.rbuf[hwid_adc_channel[index]]);
	} else {
		printk("[%s] HWID%d get failed\n", __func__, index+1);
		return 0;
	}
}

void get_hwid(void)
{
	unsigned int i;
	int ret;
	struct regulator *vcc_hwid_reg;

	adc_max_vol = 1250; //1.25 V
	adc_max_resol = 10; //10 bits

	vcc_hwid_reg = regulator_get(NULL, "vcc_hwid");
	if (IS_ERR(vcc_hwid_reg)) {
		pr_err("[%s] Unable to get vcc_hwid regulator: %d\n", __func__, (int)vcc_hwid_reg);
		return;
	}

	ret = regulator_set_voltage(vcc_hwid_reg, 2200000, 2200000);
	if (ret < 0) {
		pr_err("[%s] Set vcc_hwid regulator voltage failed: %d\n", __func__, ret);
		goto failed1;
	}

	ret = regulator_enable(vcc_hwid_reg);
	if (ret < 0) {
		pr_err("[%s] enable vcc_hwid regulator failed: %d\n", __func__, ret);
		goto failed1;
	}
	mdelay(100);

	for (i = 0; i < ARRAY_SIZE(hwid_adc_channel); i++) {
		hwid_value[i] = get_hwid_adc(i);
		hwid_index[i] = transfer_adc_index(hwid_value[i]);
	}

	find_hwid_table();

	regulator_disable(vcc_hwid_reg);
failed1:
	regulator_put(vcc_hwid_reg);
}
#endif

/* Driver part to create sys-dev for uplayer used */
static ssize_t fih_hwid_product_id_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", fih_product_id);
}

static ssize_t fih_hwid_phase_id_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", fih_product_phase);
}

static ssize_t fih_hwid_band_id_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", fih_band_id);
}

static ssize_t fih_hwid_product_id_str_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(productid_str); i++) {
		if (productid_str[i].id == fih_product_id)
			break;
	}
	if (i < ARRAY_SIZE(productid_str))
		return sprintf(buf, "%s\n", productid_str[i].str);
	else
		return sprintf(buf, "Not Defined!!\n");
}

static ssize_t fih_hwid_phase_id_str_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(phaseid_str); i++) {
		if (phaseid_str[i].id == fih_product_phase)
			break;
	}
	if (i < ARRAY_SIZE(phaseid_str))
		return sprintf(buf, "%s\n", phaseid_str[i].str);
	else
		return sprintf(buf, "Not Defined!!\n");
}

static ssize_t fih_hwid_band_id_str_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(bandid_str); i++) {
		if (bandid_str[i].id == fih_band_id)
			break;
	}
	if (i < ARRAY_SIZE(bandid_str))
		return sprintf(buf, "%s\n", bandid_str[i].str);
	else
		return sprintf(buf, "Not Defined!!\n");
}

static DEVICE_ATTR(product_id, 0644, fih_hwid_product_id_show, NULL);
static DEVICE_ATTR(phase_id, 0644, fih_hwid_phase_id_show, NULL);
static DEVICE_ATTR(band_id, 0644, fih_hwid_band_id_show, NULL);
static DEVICE_ATTR(product_id_str, 0644, fih_hwid_product_id_str_show, NULL);
static DEVICE_ATTR(phase_id_str, 0644, fih_hwid_phase_id_str_show, NULL);
static DEVICE_ATTR(band_id_str, 0644, fih_hwid_band_id_str_show, NULL);

static struct attribute *fih_hwid_attrs[] = {
	&dev_attr_product_id.attr,
	&dev_attr_phase_id.attr,
	&dev_attr_band_id.attr,
	&dev_attr_product_id_str.attr,
	&dev_attr_phase_id_str.attr,
	&dev_attr_band_id_str.attr,
	NULL,
};

static struct attribute_group fih_hwid_attrs_group = {
	.attrs = fih_hwid_attrs,
};

static int __devinit fih_hwid_probe(struct platform_device *pdev)
{
	int ret = 0;

	if ((ret = sysfs_create_group(&pdev->dev.kobj, &fih_hwid_attrs_group))) {
		pr_err("[%s] Unable to create sysfs group: %d\n", __func__, ret);
		return -1;
	}

	return 0;
}

static int __devexit fih_hwid_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj, &fih_hwid_attrs_group);
	return 0;
}

static struct platform_driver fih_hwid_driver = {
	.probe		= fih_hwid_probe,
	.remove		= __devexit_p(fih_hwid_remove),
	.driver		= {
		.name	= "fih_hwid",
		.owner	= THIS_MODULE,
	},
};

static int __init fih_hwid_init(void)
{
	return platform_driver_register(&fih_hwid_driver);
}
module_init(fih_hwid_init);

static void __exit fih_hwid_exit(void)
{
	platform_driver_unregister(&fih_hwid_driver);
}
module_exit(fih_hwid_exit);

MODULE_ALIAS("platform:fih-hwid");
MODULE_AUTHOR("PinyCHWu <pinychwu@fih-foxconn.com>");
MODULE_DESCRIPTION("FIH HWID driver");

