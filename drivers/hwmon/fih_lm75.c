/*20111221, Robert Hu, implement TempSensor as input device { */
#include <linux/workqueue.h>
#include <linux/input.h>

#define DEFAULT_POLLING_INTERVAL        1000

static int lm75_read_value(struct i2c_client *client, u8 reg);


void lm75_report_func(struct work_struct *work)
{
	struct lm75_data *tmp105 = container_of((struct delayed_work *)work,
								struct lm75_data,
								d_work);
	int temp=0;
	int rawdata=0;
	rawdata=lm75_read_value(tmp105->client, LM75_REG_TEMP[0]);
	temp=LM75_TEMP_FROM_REG(rawdata);
	input_event(tmp105->lm75_input_dev, EV_MSC, MSC_RAW, temp);
	input_sync(tmp105->lm75_input_dev);
	schedule_delayed_work(&tmp105->d_work, msecs_to_jiffies(tmp105->req_poll_rate));
}


int FIH_lm75_input_dev_init(struct lm75_data *data, struct i2c_client *client)
{
	int ret=0;

	data->req_poll_rate = DEFAULT_POLLING_INTERVAL;
	INIT_DELAYED_WORK(&data->d_work,lm75_report_func);
	data->lm75_input_dev = input_allocate_device();

	if (data->lm75_input_dev == NULL) {
		ret = -ENOMEM;
		pr_err("---LM75: %s:Failed to allocate lm75 input device\n", __func__);
		goto lm75_input_error;
	}

	data->lm75_input_dev->name = "tmp105";
	data->lm75_input_dev->id.bustype = BUS_I2C;

	input_set_capability(data->lm75_input_dev, EV_MSC, MSC_RAW);

	ret = input_register_device(data->lm75_input_dev);
	if (ret) {
		pr_err("---LM75: %s:Unable to register lm75 device\n", __func__);
		goto lm75_register_fail;
	}

	data->client = client;
	schedule_delayed_work(&data->d_work, msecs_to_jiffies(data->req_poll_rate));

	return 0;

lm75_register_fail:
lm75_input_error:
	return -1;
}

int FIH_lm75_remove(struct lm75_data *data)
{
	input_unregister_device(data->lm75_input_dev);
	input_free_device(data->lm75_input_dev);
	cancel_delayed_work_sync(&data->d_work);
	return 0;
}
/*20111221, Robert Hu, implement TempSensor as input device } */	
