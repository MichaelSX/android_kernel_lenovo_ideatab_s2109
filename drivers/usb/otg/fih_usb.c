

#include <linux/i2c/twl.h>

#include <fih/resources/charger/smb347_charger.h>



struct delayed_work     d_work;
struct twl6030_usb      *Ptwl;
int suspend_charger=0,suspend_enable=0;

int twl6030_USB_ID_GET(void)
{
        int ret=0;
        ret = twl6030_readb(Ptwl, TWL6030_MODULE_ID0, STS_HW_CONDITIONS);
        return ret;
}

static void enable_OTG_listener(void)
{
        suspend_charger=1;
        suspend_enable=1;
	schedule_delayed_work(&d_work, msecs_to_jiffies(1000));
}

static void twl6030_work_queue(struct work_struct *work)
{
        int ret=0;
        ret=twl6030_USB_ID_GET();
        if( (ret & 0x04) == 0 )
        {
                printk("********************************************* USB_ID is USB and suspend_charger is %d\n",suspend_charger);
                smb347_set_booster_gpio(0);
                smb347_set_susp_mode(0);
                cancel_delayed_work(&d_work);
        }else
        {
                printk("********************************************* USB_ID is OTG and suspend_charger is %d\n",suspend_charger);
                if(suspend_charger)
                {
                        ret=smb347_set_susp_mode(suspend_enable);
                        if(ret==0)
                        {
                                smb347_set_booster_gpio(1); // power supply to OTG
                                suspend_charger=0;
                        }else
                        {
                                printk(" USB charger ic set fail , suspend_charger still is %d \n",suspend_charger);
                        }
                }
                schedule_delayed_work(&d_work, msecs_to_jiffies(1000));
        }
}

void FIH_init_otg_listener(struct twl6030_usb *twl)
{
	INIT_DELAYED_WORK(&d_work,twl6030_work_queue);
	Ptwl=twl;
}


