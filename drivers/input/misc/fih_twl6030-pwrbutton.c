/**
 * twl6030-pwrbutton.c - TWL6030 Power Button Input Driver
 *
 * Copyright (C) 2011
 *
 * Written by Dan Murphy <DMurphy@ti.com>
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License. See the file "COPYING" in the main directory of this
 * archive for more details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Derived Work from: twl4030-pwrbutton.c from
 * Peter De Schrijver <peter.de-schrijver@nokia.com>
 * Felipe Balbi <felipe.balbi@nokia.com>
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/i2c/twl.h>
#include <linux/gpio.h>
#include <linux/pm.h>
#include "mach/gpio_omap44xx_fih.h"

#include "../../../kernel/power/power.h"



#include<linux/gpio_keys.h>


#define PWR_PWRON_IRQ (1 << 0)
#define STS_HW_CONDITIONS 0x21

int pwrbutton_hw_state = 0xFF; 

struct twl6030_pwr_button {
	struct input_dev *input_dev;
	struct device		*dev;
	int report_key;
	int ext_gpio;
	int ext_irq;
	int valid_flag;

};

struct twl6030_ext_pwrbutton	twl6030_ext_pwr = {
	.ext_gpio	= PB_POWER_ON_OMAP,
};

void fih_parse_twldata(void *twldata)
{
	struct twl4030_platform_data *temp = (struct twl4030_platform_data*)twldata;
	temp->ext_pwrbutton = &twl6030_ext_pwr;
}

int fih_twl6030_pwrbutton_state(){
	  if(pwrbutton_hw_state == 0xFF) return 0;
	  printk(KERN_DEBUG "[dw][%s] gpio value %d\n", __func__, !(pwrbutton_hw_state & PWR_PWRON_IRQ));
    return !(pwrbutton_hw_state & PWR_PWRON_IRQ);
}

static inline int twl6030_readb(struct twl6030_pwr_button *twl,
		u8 module, u8 address)
{
	u8 data = 0;
	int ret = 0;

	ret = twl_i2c_read_u8(module, &data, address);
	if (ret >= 0)
		ret = data;
	else
		dev_dbg(twl->dev,
			"TWL6030:readb[0x%x,0x%x] Error %d\n",
					module, address, ret);

	return ret;
}

static inline int twl6030_writeb(struct twl6030_pwr_button *twl, u8 module,
						u8 data, u8 address)
{
	int ret = 0;

	ret = twl_i2c_write_u8(module, data, address);
	if (ret < 0)
		dev_dbg(twl->dev,
			"TWL6030:Write[0x%x] Error %d\n", address, ret);
	return ret;
}

static irqreturn_t fih_powerbutton_irq(int irq, void *_pwr)
{
	struct twl6030_pwr_button *pwr = _pwr;
	int hw_state;
	int ret=request_suspend_state_for_gpio();
	if (ret==1){
		printk(KERN_DEBUG "[dw][%s] it's deep sleep mode,send key event, 1\n", __func__);
		input_report_key(pwr->input_dev, pwr->report_key,1);
		input_report_key(pwr->input_dev, pwr->report_key,0);
		input_sync(pwr->input_dev);
		return IRQ_HANDLED;
	}else if(ret==2){
		if (irq == pwr->ext_irq) {
			hw_state = twl6030_readb(pwr, TWL6030_MODULE_ID0, STS_HW_CONDITIONS);
			if (gpio_get_value(pwr->ext_gpio)) { //no pressed
				hw_state |= PWR_PWRON_IRQ; //high level
				pwrbutton_hw_state = 0xFF;
				reset_key_combination();
				
			} else {	//pressed
				hw_state &= ~PWR_PWRON_IRQ; //low level
				pwrbutton_hw_state = hw_state; 
            }
			printk(KERN_DEBUG "[dw][%s] normal case send gpio value %d\n", __func__, !(hw_state & PWR_PWRON_IRQ));
			input_report_key(pwr->input_dev, pwr->report_key,!(hw_state & PWR_PWRON_IRQ));
			input_sync(pwr->input_dev);
			return IRQ_HANDLED;
		}
	}else{
		printk(KERN_DEBUG "[dw][%s] it's deep sleep mode adn ignore two times", __func__);
	}
}

static int fih_ext_set_gpio_irq(struct twl6030_pwr_button *chip, int gpio)
{
	int ret = 0;

	if (gpio_is_valid(gpio)) {
		chip->ext_gpio = gpio;
		ret = gpio_request(chip->ext_gpio, "ext_pwr_gpio");
		if (ret) {
			pr_err("[%s] Failed to request GPIO #%d: %d\n", __func__, chip->ext_gpio, ret);
			return -1;
		}
		gpio_direction_input(chip->ext_gpio);
  		chip->ext_irq = gpio_to_irq(chip->ext_gpio);
		if (chip->ext_irq < 0) {
			pr_err("[%s] Get GPIO-%d to irq failed\n", __func__, chip->ext_gpio);
			goto failed1;
		}
		ret = request_threaded_irq(chip->ext_irq, NULL, fih_powerbutton_irq, 
					IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
					"ext_pwr_gpio", chip);
		if (ret < 0) {
			pr_err("[%s] Failed to request IRQ: #%d: %d\n", __func__, chip->ext_irq, ret);
			goto failed1;
		}
		chip->valid_flag = 1;
	}

	if (chip->valid_flag)
		return 0;
	else
		return -1;

failed1:
	gpio_free(chip->ext_gpio);
	return -1;
}

static void fih_ext_free_gpio_irq(struct twl6030_pwr_button *chip)
{
	if (chip->valid_flag) {
		free_irq(chip->ext_irq, chip);
		gpio_free(chip->ext_gpio);
	}
}

static int __devinit fih_twl6030_pwrbutton_probe(struct platform_device *pdev)
{
	struct twl6030_pwr_button *pwr_button;

	//int irq = platform_get_irq(pdev, 0);
	int err = -ENODEV;
	struct twl6030_ext_pwrbutton *ext_pwr_pdata = pdev->dev.platform_data;


	pr_info("%s: Enter\n", __func__);
	pwr_button = kzalloc(sizeof(struct twl6030_pwr_button), GFP_KERNEL);
	if (!pwr_button)
		return -ENOMEM;

	fih_ext_set_gpio_irq(pwr_button, ext_pwr_pdata->ext_gpio);


	pwr_button->input_dev = input_allocate_device();
	if (!pwr_button->input_dev) {
		dev_dbg(&pdev->dev, "Can't allocate power button\n");
		goto input_error;
	}

	__set_bit(EV_KEY, pwr_button->input_dev->evbit);

	pwr_button->report_key = KEY_POWER; //KEY_END; 
	pwr_button->dev = &pdev->dev;
	pwr_button->input_dev->evbit[0] = BIT_MASK(EV_KEY);
	pwr_button->input_dev->keybit[BIT_WORD(pwr_button->report_key)] =
			BIT_MASK(pwr_button->report_key);
	pwr_button->input_dev->name = "twl6030_pwrbutton";
	pwr_button->input_dev->phys = "twl6030_pwrbutton/input0";
	pwr_button->input_dev->dev.parent = &pdev->dev;

	//err = request_threaded_irq(irq, NULL, fih_powerbutton_irq,
	//		IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
	//		"fih_twl6030_pwrbutton", pwr_button);
	//if (err < 0) {
	//	dev_dbg(&pdev->dev, "Can't get IRQ for pwrbutton: %d\n", err);
	//	goto free_input_dev;
	//}

	err = input_register_device(pwr_button->input_dev);
	if (err) {
		dev_dbg(&pdev->dev, "Can't register power button: %d\n", err);
		goto free_irq;
	}

	twl6030_interrupt_unmask(0x01, REG_INT_MSK_LINE_A);
	twl6030_interrupt_unmask(0x01, REG_INT_MSK_STS_A);

	platform_set_drvdata(pdev, pwr_button);

	return 0;

free_irq:

	//free_irq(irq, NULL);
free_input_dev:
	input_free_device(pwr_button->input_dev);
input_error:
	kfree(pwr_button);
	return err;
}

static int __devexit fih_twl6030_pwrbutton_remove(struct platform_device *pdev)
{
	struct twl6030_pwr_button *pwr_button = platform_get_drvdata(pdev);
	struct input_dev *pwr = pwr_button->input_dev;
	int irq = platform_get_irq(pdev, 0);

	//free_irq(irq, pwr);
	//input_unregister_device(pwr);

	fih_ext_free_gpio_irq(pwr_button);
	kfree(pwr_button);


	return 0;
}


#ifdef CONFIG_PM
static int fih_twl6030_pwrbutton_suspend(struct platform_device *pdev, pm_message_t state)
{
  	pr_info("[%s]\n", __func__);
	return 0;
}

static int fih_twl6030_pwrbutton_resume(struct platform_device *pdev)
{
	struct twl6030_pwr_button *pwr = platform_get_drvdata(pdev);
	int hw_state = 0;

  	pr_info("[%s]\n", __func__);

	hw_state = twl6030_readb(pwr, TWL6030_MODULE_ID0, STS_HW_CONDITIONS);

	if (gpio_get_value(pwr->ext_gpio)) //no pressed
		hw_state |= PWR_PWRON_IRQ; //high level
	else	//pressed
		hw_state &= ~PWR_PWRON_IRQ; //low level

	pr_info("[%s] gpio value %d\n", __func__, (hw_state & PWR_PWRON_IRQ));

	input_report_key(pwr->input_dev, pwr->report_key,
			!(hw_state & PWR_PWRON_IRQ));
	input_sync(pwr->input_dev);

	return 0;
}
#else
#define fih_twl6030_pwrbutton_suspend	NULL
#define fih_twl6030_pwrbutton_resume	NULL
#endif


struct platform_driver fih_twl6030_pwrbutton_driver = {
	.probe		= fih_twl6030_pwrbutton_probe,
	.remove		= __devexit_p(fih_twl6030_pwrbutton_remove),

	.suspend	= fih_twl6030_pwrbutton_suspend,
	.resume		= fih_twl6030_pwrbutton_resume,

	.driver		= {
		.name	= "fih_twl6030_pwrbutton",
		.owner	= THIS_MODULE,
	},
};

static int __init fih_twl6030_pwrbutton_init(void)
{
	return platform_driver_register(&fih_twl6030_pwrbutton_driver);
}
module_init(fih_twl6030_pwrbutton_init);

static void __exit fih_twl6030_pwrbutton_exit(void)
{
	platform_driver_unregister(&fih_twl6030_pwrbutton_driver);
}
module_exit(fih_twl6030_pwrbutton_exit);

MODULE_ALIAS("platform:twl6030_pwrbutton");
MODULE_DESCRIPTION("Triton2 Power Button");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dan Murphy <DMurphy@ti.com>");
