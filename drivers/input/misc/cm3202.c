/**
 * Copyright (C) 2011 Anvoi<anvoi@mrmo.cc>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <linux/platform_device.h>      
#include <linux/init.h>                 
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/types.h>
#include <mach/gpio.h>
#include <fih/resources/sensors/fih_light_sensor/cm3202.h>
#include <mach/gpio_omap44xx_fih.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/i2c/twl6030-gpadc.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/pm.h>

#define DEBUGD

#ifdef DEBUGD
	#define dprintk(x...) printk(x)
#else
	#define dprintk(x...)
#endif

#define DEFAULT_AMBIENT_INTERVAL	1000

/*==============================================
		Global variable
================================================*/
struct cm3202_als *als;

/*======================================================
	array format 
	mapping table from optial RD DinoLee(512#62223)
	RAW data,	transfor rate,	//map optics Lux 	
========================================================*/
int data_compensation=1; //for windows loss

static int lux_map_180k[]= 
{
	0,	333,	//  0 lux
	3,	200,	//100 lux	
	8,	142,	//200 lux			
	15,	133,	//300 lux
	30,	166,	//500 lux
	42,	222,	//700 lux
	51,	142,	//900 lux
	58,	272,	//1000 lux
	79,	0,	//1300 lux 
};

/*=============================================
	For Analog Devices 
channel_no : GPADC channel        
================================================*/
static int twl6030_get_gpadc_conversion(int channel_no)
{
        struct twl6030_gpadc_request req;
        int lux = 0;
        int ret;

        req.channels = (1 << channel_no);
        req.method = TWL6030_GPADC_SW2;
        req.active = 0;
        req.func_cb = NULL;

        ret = twl6030_gpadc_conversion(&req);
        if (ret < 0)
                return ret;

        if (req.rbuf[channel_no] > 0)
                lux = req.rbuf[channel_no];

        return lux;
}

static void cm3202_enable(int on)
{    
	if(!!on)
	{
		dprintk("%s: Light sensor enable\n",__func__);  
		gpio_direction_output(ALS_INT_N, 1);
		gpio_set_value(ALS_INT_N, 0);   
	}else
	{
		dprintk("%s: Light sensor shutdown\n",__func__);    
		gpio_direction_output(ALS_INT_N, 1);
		gpio_set_value(ALS_INT_N, 1); 
	}
}

static void cm3202_report_als_input(struct cm3202_als *data)
{
	dprintk("%s  data->lux=%d  data->old_lux=%d\n",__func__, data->lux, data->old_lux);
	//wiya GOXI.B-1400 add for fix can not receive 0 suddenly START {
	if(data->lux == 0 && data->old_lux != 0){
		input_event(data->als_input_dev, EV_LED, LED_MISC, 1);
		input_sync(data->als_input_dev);
	}
	data->old_lux = data->lux ;
	//wiya GOXI.B-1400 add for fix can not receive 0 suddenly END }
	
	input_event(data->als_input_dev, EV_LED, LED_MISC, data->lux);
	input_sync(data->als_input_dev);
}

int ADCToLux(int adc_value)
{
	int convert_value=0;
	convert_value=adc_value*(1050-0)/(1000-0);// 10 bit ADC but detect range only 0~1000 Cm3202 support 0~1050 lux
	return convert_value;
}

static void cm3202_work_queue(struct work_struct *work)
{
	int value=0;
	if(als->enable)
	{
		value=twl6030_get_gpadc_conversion(5);
		als->lux=ADCToLux(value)*data_compensation;
		//BobihLee, 20120309 GOXI.B-2168 under 3 modify to 0, START {
		if((als->lux <= 3) && (als->lux > 0))
		{
			als->lux = 0;
		}
		//BobihLee, 20120309 GOXI.B-2168 under 3 modify to 0, END }
		cm3202_report_als_input(als);
		dprintk(KERN_ERR "[Line:%d] %s_ Light sensor s value=%d\n", __LINE__,__func__,als->lux );
	}
	schedule_delayed_work(&als->d_work, msecs_to_jiffies(als->ambient_interval));
}

/*==============================================
	FTM Function
================================================*/

static int cm3202_open(struct inode *inode, struct file *file)
{
	dprintk(KERN_INFO "\n");
	return 0;
}

static ssize_t cm3202_read(struct file *file, char __user *buffer, size_t size, loff_t *f_ops)
{
	dprintk(KERN_INFO "\n");
	return 0; 
}

static ssize_t cm3202_write(struct file *file, const char __user *buffer, size_t size, loff_t *f_ops)
{
	dprintk(KERN_INFO "\n");
	return 0;
}

static long cm3202_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{  
	int ret=0, value=0, i=0, data=0;	
	switch (cmd)
	{
		case ACTIVE_LS:
			dprintk("%s: ACTIVE_LS\n", __func__);
			als->enable=1;
			cm3202_enable(als->enable);           
			break;

		case READ_LS:
			value=twl6030_get_gpadc_conversion(5);
			als->lux=ADCToLux(value)*data_compensation;
			if(als->lux == 0) // retry
			{
				for(i=0;i<3;i++)
				{
					value=twl6030_get_gpadc_conversion(5);
					data=ADCToLux(value)*data_compensation;					
					printk("Light sensor retry read %d = %d \n",i,data);

					if(data>0) //data filter
					{
						als->lux=data;
						i=3; //stop retry
					}												
				}		
			}

			ret = copy_to_user((unsigned long __user *)arg, &als->lux, sizeof(als->lux));
			dprintk( "Light sensor : READ_LS value=%d\n", (int)(als->lux));
			break;

		default:
			dprintk(KERN_INFO "Light sensor : ioctl default : NO ACTION!!\n");
	}
	return ret;
}

static int cm3202_release(struct inode *inode, struct file *filp)
{
	dprintk(KERN_INFO "\n");
	return 0;
}

static const struct file_operations cm3202_dev_fops = {
	.owner = THIS_MODULE,
	.open = cm3202_open,
	.read = cm3202_read,
	.write = cm3202_write,
//FTHTDC, 20111201, Anvoi, Change ioctl function type to unlocked_ioctl for K30
	.unlocked_ioctl = cm3202_ioctl,
//FTHTDC, 20111201, Anvoi, Change ioctl function type to unlocked_ioctl for K30

	.release = cm3202_release,
};

static struct miscdevice cm3202_dev =
{
	.minor = MISC_DYNAMIC_MINOR,
	.name = "cm3202_als",
	.fops = &cm3202_dev_fops,
};

/*==============================================
        SYSFS
1)Sensor enable(R/W)
        /sys/devices/platform/CM3202/als_enable
2)Sensor data(R)
        /sys/devices/platform/CM3202/lux

================================================*/
static ssize_t cm3202_lux_show(struct device *dev,
                  struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", als->lux);
}


static ssize_t cm3202_store_attr_als_enable(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val;
	int error = 0;

	dprintk( "%s, function in \n",__func__);
	error = strict_strtoul(buf, 0, &val);
	if (error){
		printk("%s, input format error \n",__func__);
		return error;
	}
	als->enable=!!val;
	dprintk( "%s, als->enable set %d  \n",__func__, als->enable);
	cm3202_enable(als->enable);
	return count;
}

static ssize_t cm3202_show_attr_enable(struct device *dev, struct device_attribute *attr, char *buf)
{
	dprintk( "%s, function in kernel attr enable\n",__func__);                  
	return sprintf(buf, "%d\n", (als->enable));
}

static ssize_t cm3202_data_compensation_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
        unsigned long val;
        int error = 0;

        dprintk( "%s, function in \n",__func__);
        error = strict_strtoul(buf, 0, &val);
        if (error){
                printk("%s, input format error \n",__func__);
                return error;
        }
 	data_compensation=val;
	dprintk( "%s, data_compensation=%d \n",__func__,data_compensation);
        return count;
}

static ssize_t cm3202_data_compensation_show(struct device *dev, struct device_attribute *attr, char *buf)
{
        dprintk( "%s, data_compensation=%d \n",__func__,data_compensation);
        return sprintf(buf, "%d\n", (data_compensation));
}

static DEVICE_ATTR(als_enable, 0644, cm3202_show_attr_enable, cm3202_store_attr_als_enable);
static DEVICE_ATTR(lux, S_IWUSR | S_IRUGO, cm3202_lux_show, NULL);
static DEVICE_ATTR(data_compensation, 0644, cm3202_data_compensation_show, cm3202_data_compensation_store);

static struct attribute *cm3202_attrs[] = {
	&dev_attr_als_enable.attr,
	&dev_attr_lux.attr,
	&dev_attr_data_compensation.attr,
	NULL
};

static const struct attribute_group cm3202_attr_group = {
	.attrs = cm3202_attrs,
};

static int __devinit cm3202_probe(struct platform_device *pdev)
{
	struct cm3202_platform_data *cm3202_pdata = pdev->dev.platform_data;
	int ret=0;

	dprintk(KERN_ERR "[Line:%d] %s\n", __LINE__,__func__ );

	als = kzalloc(sizeof(struct cm3202_als), GFP_KERNEL);
	if (als == NULL)
		return -ENOMEM;

	cm3202_pdata->als = als;
	als->ambient_interval = DEFAULT_AMBIENT_INTERVAL;

	INIT_DELAYED_WORK(&als->d_work,cm3202_work_queue);
	dprintk("%s: sensor enable",__func__);
  
	if (gpio_request(ALS_INT_N, "ALS ENABLE PIN") < 0) 
	{
		dprintk("%s: CM3202 GPIO request failed\n", __func__);
		return -1;
	}

	if (0 != misc_register(&cm3202_dev))
	{
		printk(KERN_ERR "[cm3202]%s: ltr502als_dev register failed.\n", __func__);
		return 0;
	}
	else
	{
		dprintk("cm3202_dev register ok.\n");
	}

	ret = sysfs_create_group(&pdev->dev.kobj, &cm3202_attr_group);
	if (ret)
	{
		printk(KERN_ERR "%s:Cannot create sysfs group\n", __func__);
		goto sysfs_create_fail;
	}

	als->als_input_dev = input_allocate_device();

	if (als->als_input_dev == NULL)
	{
		ret = -ENOMEM;
		printk(KERN_ERR "%s:Failed to allocate als input device\n", __func__);
		goto als_input_error;
	}

	als->als_input_dev->name = "cm3202_als";

	input_set_capability(als->als_input_dev, EV_MSC, MSC_RAW);
	input_set_capability(als->als_input_dev, EV_LED, LED_MISC);
	input_set_drvdata(als->als_input_dev, cm3202_pdata);
	
	ret = input_register_device(als->als_input_dev);

	if (ret) 
	{
		printk(KERN_ERR "%s:Unable to register als device\n", __func__);
		goto als_register_fail;
	}

	schedule_delayed_work(&als->d_work, msecs_to_jiffies(als->ambient_interval));

	return 0;

sysfs_create_fail:
	input_free_device(als->als_input_dev);
als_register_fail:
als_input_error:
	return -1;
}

static int __devexit cm3202_remove(struct platform_device *pdev)
{
	printk(KERN_ERR "[Line:%d] %s\n", __LINE__,__func__ );

	cancel_delayed_work_sync(&als->d_work);
	misc_deregister(&cm3202_dev);
	gpio_free(ALS_INT_N);
	return 0;
}

static int cm3202_suspend(struct platform_device *pdev, pm_message_t state)
{
	printk("%s \n",__func__);	
	cancel_delayed_work_sync(&als->d_work);
	return 0;
}

static int cm3202_resume(struct platform_device *pdev)
{
	printk("%s \n",__func__);
	schedule_delayed_work(&als->d_work, msecs_to_jiffies(als->ambient_interval));
	return 0;
}

static struct platform_driver cm3202_driver = {
	.probe          = cm3202_probe,
	.remove         = cm3202_remove,
	.driver         = 
	{
		.name   = CM3202_NAME,
		.owner  = THIS_MODULE,
	},

	.suspend	= cm3202_suspend,
	.resume		= cm3202_resume,

};

static int __init cm3202_init(void)
{
	printk(KERN_ERR "Light Sensor : cm3202 sensors driver: init\n");  
	return platform_driver_register(&cm3202_driver);
}

static void __exit cm3202_exit(void)
{
	printk(KERN_ERR "Light Sensor : cm3202 sensors driver: exit\n"); 
	platform_driver_unregister(&cm3202_driver);
}

module_init(cm3202_init);
module_exit(cm3202_exit);
