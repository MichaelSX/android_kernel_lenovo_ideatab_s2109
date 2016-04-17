

#include <fih/resources/charger/smb347_charger.h>
#include <linux/i2c/twl.h>

void FIH_OTG_Power_off(void)
{
	int     ret=0;
        ret=twl6030_USB_ID_GET() & (0x04);
        if(ret ==0) // USB DEVUCES
        {
                printk("USB OTG remove -> cat off OTG power\n");
                smb347_set_booster_gpio(0);
                smb347_set_susp_mode(0);
        }else
        {
                printk("USB OTG still exist power still supply \n");
        }
}


