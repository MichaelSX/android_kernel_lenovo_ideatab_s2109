/** 
* Backlight Driver
*****************************************************/

#include <linux/init.h>
#include <linux/module.h>
#include "fih_charger.h"
#include <linux/fih_hw_info.h>

#ifdef CONFIG_CHARGER_SMB347
#include <fih/resources/charger/smb347_charger.h>

static struct smb347_reg smb347_init_regs[] = {
	{0x01, 0x83},
	{0x06, 0x77},
	{0x0C, 0x0F},
	{0x0D, 0xBD},
};

static struct smb347_platform_data smb347_pdata = {
	.gpio_map = (SMB347_STAT_GPIO_MASK | SMB347_INOK_GPIO_MASK | SMB347_BSTR_GPIO_MASK),
	.stat_gpio = CHRG_STAT, //gpio 2
	.inok_gpio = CHRG_SYS_OK, //gpio 0
	.susp_gpio = -1,
	.usb_gpio = -1,
	.bstr_gpio = EN_OTG_BOOST, //gpio 56
	.init_array = smb347_init_regs,
	.init_array_size = ARRAY_SIZE(smb347_init_regs),
};

static struct i2c_board_info __initdata tablet_i2c_2_boardinfo[] = {
	{
		.type	= "smb347_charger",
		.addr	= 0x11,
		.platform_data = &smb347_pdata,
	},
};

int __init charger_init(void)
{
	int product_id = fih_get_product_id(), phase_id = fih_get_product_phase();

	printk(KERN_ALERT "%s, begin\n",__func__);
	if ((product_id == Product_GOX) && (phase_id == Product_PR1)) {
		printk("SMB347 PR1 setting\n");
		smb347_pdata.gpio_map |= (SMB347_SUSP_GPIO_MASK | SMB347_USB_GPIO_MASK);
		smb347_pdata.susp_gpio = CHRG_SUSP; //gpio 45
		smb347_pdata.usb_gpio = CHRG_USB_HCS; //gpio 47
	}
	i2c_register_board_info(2, tablet_i2c_2_boardinfo, ARRAY_SIZE(tablet_i2c_2_boardinfo));
	printk(KERN_ALERT "%s, end\n",__func__);
	
	return 0;
}
#else	
int __init charger_init(void)
{
	return 0;
}
#endif
