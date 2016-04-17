/*
 * drivers/input/touchscreen/ft5x0x_ts.c
 *
 * FocalTech ft5x0x TouchScreen driver.
 *
 * Copyright (c) 2010  Focal tech Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
 *    note: only support mulititouch    Wenfs 2010-10-01
 */

//#define CONFIG_FTS_CUSTOME_ENV
#include <asm/irq.h>
#include <asm/mach/irq.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/earlysuspend.h>
#include <linux/input.h>
//#include "ft5x0x_i2c_ts.h"
#include <linux/input/ft5x0x_i2c_ts.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#include <linux/sysfs.h>
#include <linux/sysdev.h>

#include <linux/gpio.h>
#include "mach/gpio_omap44xx_fih.h"

#define FT5606_NEW_DRIVER

/* -------------- global variable definition -----------*/
static struct i2c_client *this_client;
static REPORT_FINGER_INFO_T _st_finger_infos[CFG_MAX_POINT_NUM];
//static unsigned int _sui_irq_num= IRQ_EINT(6);
static unsigned int _sui_irq_num = 0;
static int _si_touch_num = 0;

int tsp_keycodes[CFG_NUMOFKEYS] ={

        KEY_MENU,
        KEY_HOME,
        KEY_BACK,
        KEY_SEARCH
};

char *tsp_keyname[CFG_NUMOFKEYS] ={

        "Menu",
        "Home",
        "Back",
        "Search"
};

static bool tsp_keystatus[CFG_NUMOFKEYS];




/***********************************************************************
    [function]:
                            callback:            read data from ctpm by i2c interface;
    [parameters]:
                            buffer[in]:          data buffer;
                            length[in]:          the length of the data buffer;
    [return]:
                            FTS_TRUE:            success;
                            FTS_FALSE:           fail;
************************************************************************/
static bool i2c_read_interface(u8* pbt_buf, int dw_lenth)
{
    int ret;

    ret = i2c_master_recv(this_client, pbt_buf, dw_lenth);

    if(ret <= 0)
    {
        printk("[TSP]i2c_read_interface error\n");
        return FTS_FALSE;
    }

    return FTS_TRUE;
}



/***********************************************************************
    [function]:
                       callback:             write data to ctpm by i2c interface;
    [parameters]:
                       buffer[in]:          data buffer;
                       length[in]:          the length of the data buffer;
    [return]:
                       FTS_TRUE:            success;
                       FTS_FALSE:           fail;
************************************************************************/
static bool i2c_write_interface(u8* pbt_buf, int dw_lenth)
{
    int ret;
    ret = i2c_master_send(this_client, pbt_buf, dw_lenth);
    if(ret <= 0)
    {
        printk("[TSP]i2c_write_interface error line = %d, ret = %d\n", __LINE__, ret);
        return FTS_FALSE;
    }

    return FTS_TRUE;
}



/***********************************************************************
    [function]:
                        callback:             read register value ftom ctpm by i2c interface;
    [parameters]:
                        reg_name[in]:         the register which you want to read;
                        rx_buf[in]:           data buffer which is used to store register value;
                        rx_length[in]:        the length of the data buffer;
    [return]:
                        FTS_TRUE:             success;
                        FTS_FALSE:            fail;
************************************************************************/
static bool fts_register_read(u8 reg_name, u8* rx_buf, int rx_length)
{
    u8 read_cmd[2]= {0};
    u8 cmd_len  = 0;

    read_cmd[0] = reg_name;
    cmd_len = 1;

    /*send register addr*/
    if(!i2c_write_interface(&read_cmd[0], cmd_len))
    {
        return FTS_FALSE;
    }

    /*call the read callback function to get the register value*/
    if(!i2c_read_interface(rx_buf, rx_length))
    {
        return FTS_FALSE;
    }
    return FTS_TRUE;
}




/***********************************************************************
    [function]:
                        callback:             read register value ftom ctpm by i2c interface;
    [parameters]:
                        reg_name[in]:         the register which you want to write;
                        tx_buf[in]:           buffer which is contained of the writing value;
    [return]:
                        FTS_TRUE:             success;
                        FTS_FALSE:            fail;
************************************************************************/
static bool fts_register_write(u8 reg_name, u8* tx_buf)
{
    u8 write_cmd[2] = {0};

    write_cmd[0] = reg_name;
    write_cmd[1] = *tx_buf;

    /*call the write callback function*/
    return i2c_write_interface(write_cmd, 2);
}


#ifdef CONFIG_HAS_EARLYSUSPEND
/***********************************************************************
    [function]:
                        callback:                early suspend function interface;
    [parameters]:
                        handler:                 early suspend callback pointer
    [return]:
                        NULL
************************************************************************/
static void fts_ts_suspend(struct early_suspend *handler)
{
    u8 cmd;
		printk(" suspend touch panel start \n");
    cmd = PMODE_HIBERNATE;
    printk("\n [TSP]:device will suspend! \n");
    fts_register_write(FT5X0X_REG_PMODE,&cmd);
    printk(" suspend touch panel end \n");
}


/***********************************************************************
    [function]:
                        callback:                power resume function interface;
    [parameters]:
                        handler:                 early suspend callback pointer
    [return]:
                        NULL
************************************************************************/
static void fts_ts_resume(struct early_suspend *handler)
{
    u8 cmd;
		printk(" resume touch panel start \n");
    cmd = PMODE_ACTIVE;

    // Force reset 
    #if 0
    gpio_set_value(TOUCH_RST_N, 0);
    msleep(10);
    gpio_set_value(TOUCH_RST_N, 1);
    msleep(40);
    #endif

    printk("\n [TSP]:device will resume from sleep! \n");
    fts_register_write(FT5X0X_REG_PMODE, &cmd);
    printk(" resume touch panel end \n");
}
#endif


/***********************************************************************
    [function]:
                   callback:        report to the input system that the finger is put up;
    [parameters]:
                         null;
    [return]:
                         null;
************************************************************************/
#if 0
static void fts_ts_release(void)
{
    struct FTS_TS_DATA_T *data = i2c_get_clientdata(this_client);
    int i;
    int i_need_sync = 0;
    for ( i= 0; i<CFG_MAX_POINT_NUM; ++i )
    {
        if ( _st_finger_infos[i].u2_pressure == -1 )
            continue;

        _st_finger_infos[i].u2_pressure = 0;

        input_report_abs(data->input_dev, ABS_MT_POSITION_X, _st_finger_infos[i].i2_x);
        input_report_abs(data->input_dev, ABS_MT_POSITION_Y, _st_finger_infos[i].i2_y);
        input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, _st_finger_infos[i].u2_pressure);
        input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, _st_finger_infos[i].ui2_id);
        input_mt_sync(data->input_dev);

        i_need_sync = 1;

        if ( _st_finger_infos[i].u2_pressure == 0 )
            _st_finger_infos[i].u2_pressure= -1;
    }

    if (i_need_sync)
    {
        input_sync(data->input_dev);
    }

    _si_touch_num = 0;
}
#endif

#if 1
static void fts_ts_release(REPORT_FINGER_INFO_T *finger)
{
    #if 0
    int i;
    struct FTS_TS_DATA_T *data = i2c_get_clientdata(this_client);

    for ( i= 0; i<CFG_MAX_POINT_NUM; i++ )
    {
        input_report_abs(data->input_dev, ABS_MT_POSITION_X, finger[i].i2_x);
        input_report_abs(data->input_dev, ABS_MT_POSITION_Y, finger[i].i2_y);
        input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, finger[i].u2_pressure);
        input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, finger[i].ui2_id);
        input_mt_sync(data->input_dev);

    #if 0
        if ( _st_finger_infos[i].u2_pressure == 0 )
            _st_finger_infos[i].u2_pressure= -1;
    #endif	
    }
     
    input_sync(data->input_dev); 
    #endif
    _si_touch_num = 0;    
}
#endif


/***********************************************************************
    [function]:
                            callback:              read touch  data ftom ctpm by i2c interface;
    [parameters]:
                rxdata[in]:            data buffer which is used to store touch data;
                length[in]:            the length of the data buffer;
    [return]:
                FTS_TRUE:              success;
                FTS_FALSE:             fail;
************************************************************************/
static int fts_i2c_rxdata(u8 *rxdata, int length)
{
    int ret;
    struct i2c_msg msg;


    msg.addr = this_client->addr;
    msg.flags = 0;
    msg.len = 1;
    msg.buf = rxdata;
    ret = i2c_transfer(this_client->adapter, &msg, 1);

    if (ret < 0)
    {
        pr_err("msg %s i2c write error: %d\n", __func__, ret);
    }
    msg.addr = this_client->addr;
    msg.flags = I2C_M_RD;
    msg.len = length;
    msg.buf = rxdata;
    ret = i2c_transfer(this_client->adapter, &msg, 1);

    if (ret < 0)
    {
        pr_err("msg %s i2c write error: %d\n", __func__, ret);
    }
    return ret;
}





/***********************************************************************
    [function]:
                            callback:             send data to ctpm by i2c interface;
    [parameters]:
                           txdata[in]:            data buffer which is used to send data;
                           length[in]:            the length of the data buffer;
    [return]:
                           FTS_TRUE:              success;
                           FTS_FALSE:             fail;
************************************************************************/
static int fts_i2c_txdata(u8 *txdata, int length)
{
    int ret;
    struct i2c_msg msg;

    msg.addr = this_client->addr;
    msg.flags = 0;
    msg.len = length;
    msg.buf = txdata;
    ret = i2c_transfer(this_client->adapter, &msg, 1);
    if (ret < 0)
    {
        pr_err("%s i2c write error: %d\n", __func__, ret);
    }
    return ret;
}



/***********************************************************************
    [function]:
                         callback:            gather the finger information and calculate the X,Y
                                      coordinate then report them to the input system;
    [parameters]:
                         null;
    [return]:
                         null;
************************************************************************/
#if 0
int fts_read_data(void)
{
    struct FTS_TS_DATA_T *data = i2c_get_clientdata(this_client);
    struct FTS_TS_EVENT_T *event = &data->event;
    u8 buf[32] = {0};
    static int key_id = 0x80;

    int i,id,temp,i_count,ret = -1;
    int touch_point_num = 0, touch_event, x, y, pressure, size;
    REPORT_FINGER_INFO_T touch_info[CFG_MAX_POINT_NUM];

    i_count = 0;

    do
    {
        buf[0] = 3;

        id = 0xe;

        ret=fts_i2c_rxdata(buf, 6);
        if (ret > 0)
        {

            id = buf[2]>>4;
            //printk("\n--the id number is %d---\n",id);
            touch_event = buf[0]>>6;
            if (id >= 0 && id< CFG_MAX_POINT_NUM)
            {

                temp = buf[0]& 0x0f;
                temp = temp<<8;
                temp = temp | buf[1];
                x = temp;

                temp = (buf[2])& 0x0f;
                temp = temp<<8;
                temp = temp | buf[3];
                y=temp;

                pressure = buf[4] & 0x3f;
                size = buf[5]&0xf0;
                size = (id<<8)|size;
                touch_event = buf[0]>>6;

                if (touch_event == 0)  //press down
                {
                    if(y>=0 && y<850)
                    {
                        _st_finger_infos[id].u2_pressure= pressure;
                        _st_finger_infos[id].i2_x= (SCREEN_MAX_X - (int16_t)x);
                        _st_finger_infos[id].i2_y= (SCREEN_MAX_Y - (int16_t)y);												
                        _st_finger_infos[id].ui2_id  = size;
                        _si_touch_num ++;
                        printk("\n--report x position  is  %d----\n",_st_finger_infos[id].i2_x);
                        printk("\n--report y position  is  %d----\n",_st_finger_infos[id].i2_y);
                        //printk("\n--report pressure  is  %d----\n",_st_finger_infos[id].u2_pressure);
                    }
                    else if(y>=850 && y<=860)
                    {
                        if (x>=75 && x<=90)
                        {
                            key_id = 0;
                            printk("\n---virtual key 1 press---");
                        }
                        else if ( x>=185 && x<=200)
                        {
                            key_id = 1;
                            printk("\n---virtual key 2 press---");
                        }
                        else if (x>=290 && x<=305)
                        {
                            key_id = 2;
                            printk("\n---virtual key 3 press---");
                        }
                        else if ( x>=405 && x<=420)
                        {
                            key_id = 3;
                            printk("\n---virtual key 4 press---");
                        }
                        input_report_key(data->input_dev, tsp_keycodes[key_id], 1);
                        tsp_keystatus[key_id] = KEY_PRESS;
                    }
                }
                else if (touch_event == 1) //up event
                {
                    _st_finger_infos[id].u2_pressure= 0;

                    if(key_id !=0x80)
                    {
                        i = key_id;
                        printk("\n");
                        printk("\n---virtual key %d release---\n",++i);
                        for(i=0;i<8;i++)
                        {
                            input_report_key(data->input_dev, tsp_keycodes[key_id], 0);
                        }
                        key_id=0x80;
                    }
                }
                else if (touch_event == 2) //move
                {
                    _st_finger_infos[id].u2_pressure = pressure;
                    _st_finger_infos[id].i2_x = (SCREEN_MAX_X - (int16_t)x);
                    _st_finger_infos[id].i2_y = (SCREEN_MAX_Y - (int16_t)y);
                    _st_finger_infos[id].ui2_id  = size;
                    _si_touch_num ++;
                    //printk("Move event: touch number: %d, X: %d, Y:%d\n", _si_touch_num, _st_finger_infos[id].i2_x, _st_finger_infos[id].i2_y);
                }
                else
                    /*bad event, ignore*/
                     continue;

                if ( (touch_event == 1) )
                {
                    printk("[TSP] id = %d up\n",  id);
                }

                //for( i= 0; i <CFG_MAX_POINT_NUM; ++i)
                for( i= 0; i<_si_touch_num; ++i )                
                {
                    //printk("Report to framework, ID: %d, pressure: %d, X: %d, Y: %d\n", _st_finger_infos[i].ui2_id, _st_finger_infos[i].u2_pressure, _st_finger_infos[i].i2_x, _st_finger_infos[i].i2_y);
                    input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, _st_finger_infos[i].ui2_id);
                    input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, 1/*_st_finger_infos[i].u2_pressure*/);
                    input_report_abs(data->input_dev, ABS_MT_POSITION_X,  _st_finger_infos[i].i2_x);
                    input_report_abs(data->input_dev, ABS_MT_POSITION_Y,  _st_finger_infos[i].i2_y);
                    input_mt_sync(data->input_dev);

                    if(_st_finger_infos[i].u2_pressure == 0 )
                    {
                        _st_finger_infos[i].u2_pressure = -1;
                    }
                }
#if 1
                input_sync(data->input_dev);

                if (_si_touch_num == 0 )
                {
                    fts_ts_release();
                }
#else
                if(_si_touch_num)
                {
                    input_sync(data->input_dev);
                }
                else
                {
                    input_mt_sync(data->input_dev);
                    input_sync(data->input_dev);
                }
#endif
                _si_touch_num = 0;
            }
        }
        else
        {
            printk("[TSP] ERROR: in %s, line %d, ret = %d\n",
            __FUNCTION__, __LINE__, ret);
        }
        i_count++;
    }while( id != 0xf && i_count < 12);

    event->touch_point = touch_point_num;
    if (event->touch_point == 0)
    {
        return 1;
    }
    switch (event->touch_point) {
        case 5:
            event->x5           = touch_info[4].i2_x;
            event->y5           = touch_info[4].i2_y;
            event->pressure5 = touch_info[4].u2_pressure;
        case 4:
            event->x4          = touch_info[3].i2_x;
            event->y4          = touch_info[3].i2_y;
            event->pressure4= touch_info[3].u2_pressure;
        case 3:
            event->x3          = touch_info[2].i2_x;
            event->y3          = touch_info[2].i2_y;
            event->pressure3= touch_info[2].u2_pressure;
        case 2:
            event->x2          = touch_info[1].i2_x;
            event->y2          = touch_info[1].i2_y;
            event->pressure2= touch_info[1].u2_pressure;
        case 1:
            event->x1          = touch_info[0].i2_x;
            event->y1          = touch_info[0].i2_y;
            event->pressure1= touch_info[0].u2_pressure;
            break;
        default:
            return -1;
    }

    return 0;
}
#endif

#if 1
int fts_read_data(void)
{
    struct FTS_TS_DATA_T *data = i2c_get_clientdata(this_client);
    REPORT_FINGER_INFO_T _st_finger_infos[CFG_MAX_POINT_NUM]={0};
    //struct FTS_TS_EVENT_T *event = &data->event;
    u8 buf[ 3 + CFG_MAX_POINT_NUM*6 + 1] = {0};

    int i, id, temp, i_count, ret = -1;
    int touch_num = 0, touch_event, pressure; //x, y, size;
    int num;
    int width = 0;
   
    i_count = 0;
    disable_irq(_sui_irq_num);

    _si_touch_num = 0;
    //ret = fts_i2c_rxdata(buf, FT5X0X_TOTAL_DATA_BYTES);
    ret=fts_i2c_rxdata(buf, 3 + CFG_MAX_POINT_NUM*6);
    buf[ 3 + CFG_MAX_POINT_NUM * 6] = '\0';
   
    if (ret > 0)
    { 
        touch_num = buf[FT5X0X_REG_TD_STATUS] & 0x0F;
        for(num = 0 ; num < touch_num ; num++)
        //for(num=0; num<CFG_MAX_POINT_NUM; num++)
        {
            id = buf[FT5X0X_FIRST_ID_ADDR + FT5X0X_POINTER_INTERVAL*num]>>4;

            if (id >= 0 && id < CFG_MAX_POINT_NUM)
            {
                //touch_num++;
                touch_event = buf[3 + FT5X0X_POINTER_INTERVAL * num]>>6;
                //printk("\n-------touch event is %d------\n",touch_event);
                temp = buf[3 + FT5X0X_POINTER_INTERVAL * num]& 0x0f;
                temp = temp<<8;
                temp = temp | buf[4 + FT5X0X_POINTER_INTERVAL*num];
                //_st_finger_infos[num].i2_x = temp;
                _st_finger_infos[num].i2_x = SCREEN_MAX_X - temp;

                temp = (buf[5+FT5X0X_POINTER_INTERVAL*num]) & 0x0f;
                temp = temp<<8;
                temp = temp | buf[6+FT5X0X_POINTER_INTERVAL*num];
                //_st_finger_infos[num].i2_y = temp;//SCREEN_MAX_Y - temp;
                _st_finger_infos[num].i2_y = SCREEN_MAX_Y - temp;

                if (touch_event == 0 ||touch_event == 2||touch_event == 3)
                {
                    pressure = 32;
                    //width = 2;
                    _st_finger_infos[num].u2_pressure= pressure;
                    //printk("Finger[%d] event: %d, x: %d, y: %d, pressure: %d\n", num, touch_event, _st_finger_infos[num].i2_x,_st_finger_infos[num].i2_y, _st_finger_infos[num].u2_pressure);
                    _st_finger_infos[num].ui2_id = id;
                    _si_touch_num++;
                }
                else if (touch_event == 1)
                {
                    _st_finger_infos[num].ui2_id = id;
                    _st_finger_infos[num].u2_pressure= 0;
                    //printk("Finger[%d] event: %d, x: %d, y: %d, pressure: %d\n", num, touch_event, _st_finger_infos[num].i2_x,_st_finger_infos[num].i2_y, _st_finger_infos[num].u2_pressure);
                    //width = 0;
                    _si_touch_num++;
                }
 
                input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, _st_finger_infos[num].ui2_id);
                input_report_abs(data->input_dev, ABS_MT_PRESSURE,    _st_finger_infos[num].u2_pressure);
                //input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, 1/*_st_finger_infos[i].u2_pressure*/);
                //input_report_abs(data->input_dev, ABS_MT_WIDTH_MAJOR, width);
                input_report_abs(data->input_dev, ABS_MT_POSITION_X,  _st_finger_infos[num].i2_x);
                input_report_abs(data->input_dev, ABS_MT_POSITION_Y,  _st_finger_infos[num].i2_y);

                input_mt_sync(data->input_dev);	
            }
            else
            {
                break;
            }
        }
        // Add For FTM
	input_report_key(data->input_dev, BTN_TOUCH, _si_touch_num > 0);
        
        input_sync(data->input_dev);
    }

ENABLE_IRQ:
    if(_si_touch_num == 0)
    {
        for(i=0;i<CFG_MAX_POINT_NUM;i++)
        {
            _st_finger_infos[i].u2_pressure= 0;
            //_st_finger_infos[i].i2_x= 0;
            //_st_finger_infos[i].i2_y= 0;
            _st_finger_infos[i].ui2_id  = 0;
        }
        fts_ts_release(_st_finger_infos);
    }

    enable_irq(_sui_irq_num);
    return 0;
}
#endif

static void fts_work_func(struct work_struct *work)
{
    //printk("\n---work func ---\n");
    fts_read_data();
}



static irqreturn_t fts_ts_irq(int irq, void *dev_id)
{
    struct FTS_TS_DATA_T *ft5x0x_ts = dev_id;
    if (!work_pending(&ft5x0x_ts->pen_event_work)) {
        queue_work(ft5x0x_ts->ts_workqueue, &ft5x0x_ts->pen_event_work);
    }

    return IRQ_HANDLED;
}



/***********************************************************************
[function]:
                      callback:         send a command to ctpm.
[parameters]:
                      btcmd[in]:       command code;
                      btPara1[in]:     parameter 1;
                      btPara2[in]:     parameter 2;
                      btPara3[in]:     parameter 3;
                      num[in]:         the valid input parameter numbers,
                                           if only command code needed and no
                                           parameters followed,then the num is 1;
[return]:
                      FTS_TRUE:      success;
                      FTS_FALSE:     io fail;
************************************************************************/
static bool cmd_write(u8 btcmd,u8 btPara1,u8 btPara2,u8 btPara3,u8 num)
{
    u8 write_cmd[4] = {0};

    write_cmd[0] = btcmd;
    write_cmd[1] = btPara1;
    write_cmd[2] = btPara2;
    write_cmd[3] = btPara3;
    return i2c_write_interface(write_cmd, num);
}




/***********************************************************************
[function]:
                      callback:      write a byte data  to ctpm;
[parameters]:
                      buffer[in]:    write buffer;
                      length[in]:    the size of write data;
[return]:
                      FTS_TRUE:      success;
                      FTS_FALSE:     io fail;
************************************************************************/
static bool byte_write(u8* buffer, int length)
{

    return i2c_write_interface(buffer, length);
}




/***********************************************************************
[function]:
                      callback:         read a byte data  from ctpm;
[parameters]:
                      buffer[in]:      read buffer;
                      length[in]:      the size of read data;
[return]:
                      FTS_TRUE:      success;
                      FTS_FALSE:     io fail;
************************************************************************/
static bool byte_read(u8* buffer, int length)
{
    return i2c_read_interface(buffer, length);
}

/***********************************************************************
              SYSDEV FS
************************************************************************/

static ssize_t touch_ftmping_show(struct sys_device *dev,
        struct sysdev_attribute *attr, char *buf)
{
    unsigned char chip_id = 0;
    if(fts_register_read(FT5X0X_REG_CIPHER, &chip_id, 1) == FTS_TRUE)
    {
        return sprintf(buf, "%d", chip_id);
    }
    else
    {
        buf = NULL;
        return -1;
    }
}

static SYSDEV_ATTR(ftmping, 0644, touch_ftmping_show, NULL);


static ssize_t touch_ftmgetversion_show(struct sys_device *dev,
	struct sysdev_attribute *attr, char *buf)
{
    unsigned char reg_version = 0;
    if(fts_register_read(FT5X0X_REG_FIRMID, &reg_version, 1) == FTS_TRUE)
    {
        return sprintf(buf, "%d", reg_version);
    }
    else
    {
        buf = NULL;
        return -1;
    }
}

static SYSDEV_ATTR(ftmgetversion, 0644, touch_ftmgetversion_show, NULL);

static ssize_t touch_reset_show(struct sys_device *dev,
	struct sysdev_attribute *attr, char *buf)
{
	return sprintf(buf, "%d", gpio_get_value(TOUCH_RST_N));
}

static ssize_t touch_reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int value;
	unsigned char reg_value = 0;
	unsigned char reg_version = 0;
	unsigned char check_firmware = 0;

	sscanf(buf, "%d", &value);
	value = !!value;
	
	if(!value){
	    printk("Usage: echo a a non-zero value to reset touch\n");
	    return size;
	}
	
	// Force reset
	printk("Reset Touch now...\n");
	gpio_set_value(TOUCH_RST_N, 0);
	msleep(10);
	gpio_set_value(TOUCH_RST_N, 1);
	msleep(40);

	/***wait CTP to bootup normally***/
	msleep(400);

	check_firmware = fts_register_read(FT5X0X_REG_FIRMID, &reg_version,1);
	printk("[TSP] firmware version = 0x%2x\n", reg_version);
	fts_register_read(FT5X0X_REG_REPORT_RATE, &reg_value,1);
	printk("[TSP]firmware report rate = %dHz\n", reg_value*10);
	fts_register_read(FT5X0X_REG_THRES, &reg_value,1);
	printk("[TSP]firmware threshold = %d\n", reg_value * 4);
	fts_register_read(FT5X0X_REG_NOISE_MODE, &reg_value,1);
	printk("[TSP]noise mode = 0x%2x\n", reg_value);
	
	printk("Reset Touch fihish...\n");
	
	return size;
}

static SYSDEV_ATTR(reset_n, S_IRUGO | S_IWUSR, touch_reset_show, touch_reset_store);


static struct sysdev_class touch_sysclass = {
	.name	= "touch",
};

static struct sys_device device_touch = {
	.id	= 0,
	.cls	= &touch_sysclass,
};


#define FTS_PACKET_LENGTH 200
//#define    FTS_PACKET_LENGTH        128

static unsigned char CTPM_FW[]=
{
//#include "ft_app.i"
//#include <linux/input/fih_gox_ft5606_firmware_640x480.h>
//#include <linux/input/fih_gox_ft5606_firmware_1024x768.h>
//#include <fih/resources/touchscreen/NUC102_FT5301_CMI2A1_FW_Version8.i>
//#include <fih/resources/touchscreen/NUC102_FT5301_CMI2A1_FW_Version11.i>
//#include <fih/resources/touchscreen/NUC102_FT5301_CMI2A1_FW_Version10-4-point.i>
//#include <fih/resources/touchscreen/NUC102_FT5301_CMI2A1_FW_Version14.i>
//#include <fih/resources/touchscreen/NUC102_FT5301_CMI2A1_FW_Version16.i>
//#include <fih/resources/touchscreen/NUC102_FT5301_CMI2A1_FW_Version17.i>
#include <fih/resources/touchscreen/NUC102_FT5301_CMI2A1_FW_Version27.i>
};




/***********************************************************************
[function]:
                        callback:     burn the FW to ctpm.
[parameters]:
                        pbt_buf[in]:  point to Head+FW ;
                        dw_lenth[in]: the length of the FW + 6(the Head length);
[return]:
                        ERR_OK:       no error;
                        ERR_MODE:     fail to switch to UPDATE mode;
                        ERR_READID:   read id fail;
                        ERR_ERASE:    erase chip fail;
                        ERR_STATUS:   status error;
                        ERR_ECC:      ecc error.
************************************************************************/
E_UPGRADE_ERR_TYPE  fts_ctpm_fw_upgrade(u8* pbt_buf, int dw_lenth)
{
    u8  cmd,reg_val[2] = {0};
    u8  packet_buf[FTS_PACKET_LENGTH + 6];
    u8  auc_i2c_write_buf[10];
    u8  bt_ecc;
	
    int  j,temp,lenght,i_ret,packet_number, i = 0;
    int  i_is_new_protocol = 0;
    u32  config_add;
	

    /******write 0xaa to register 0xfc******/
    cmd=0xaa;
    fts_register_write(0xfc,&cmd);
	
     /******write 0x55 to register 0xfc******/
    cmd=0x55;
    fts_register_write(0xfc,&cmd);
    printk("[TSP] Step 1: Reset CTPM test\n");
   
    mdelay(35);  


    /*******Step 2:Enter upgrade mode ****/
    printk("\n[TSP] Step 2:enter new update mode\n");
    auc_i2c_write_buf[0] = 0x55;
    auc_i2c_write_buf[1] = 0xaa;
	
    fts_i2c_txdata(&auc_i2c_write_buf[0], 1);
    udelay(10);
    fts_i2c_txdata(&auc_i2c_write_buf[1], 1);
    udelay(10);

    /********Step 3:check READ-ID********/        
    cmd_write(0x90,0x00,0x00,0x00,4);
    byte_read(reg_val,2);
    if (reg_val[0] == 0x79 && reg_val[1] == 0x4)
    {
        printk("[TSP] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",reg_val[0],reg_val[1]);
    }
    else
    {
        printk("[TSP] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",reg_val[0],reg_val[1]);
        i_is_new_protocol = 1;
        return ERR_READID;
    }

    /*********Step 4:erase app**********/
    printk("[TSP] Step 4: erase. \n");
    //if (i_is_new_protocol)
    //{
        cmd_write(0x61,0x00,0x00,0x00,1);
    //}
    //else
    //{
    //    cmd_write(0x60,0x00,0x00,0x00,1);
    //}
    mdelay(3000);


    /*Step 5:write firmware(FW) to ctpm flash*/
    bt_ecc = 0;
    printk("[TSP] Step 5: start upgrade. \n");
    //dw_lenth = dw_lenth - 8;
    packet_number = (dw_lenth) / FTS_PACKET_LENGTH;
    //packet_number=20;
    packet_buf[0] = 0xbf;
    packet_buf[1] = 0x00;
    printk("\n----packet number is %d-----\n",packet_number);
    for(j=0;j<packet_number;j++)
    {
        //printk("----packet number count is %d-----\n", j);
        temp = j * FTS_PACKET_LENGTH;
        packet_buf[2] = (u8)(temp>>8);
        packet_buf[3] = (u8)(temp & 0xff);
        lenght = FTS_PACKET_LENGTH;
        packet_buf[4] = (u8)(lenght>>8);
        packet_buf[5] = (u8)(lenght & 0xff);

        //printk("buf[2]: 0x%X, buf[3]: 0x%X, buf[4]: 0x%X, buf[5]: 0x%X\n", packet_buf[2], packet_buf[3], packet_buf[4], packet_buf[5]);

        for(i=0;i<FTS_PACKET_LENGTH;i++)
        {
            packet_buf[6+i] = pbt_buf[j*FTS_PACKET_LENGTH + i]; 
            bt_ecc ^= packet_buf[6+i];
        }
        
        byte_write(&packet_buf[0],FTS_PACKET_LENGTH + 6);
        //mdelay(FTS_PACKET_LENGTH/6 + 1);
        msleep(12); 
        if ((j * FTS_PACKET_LENGTH % 1024) == 0)
        {
            printk("[TSP] upgrade the 0x%x th byte.\n", ((unsigned int)j) * FTS_PACKET_LENGTH);
        }
    }

   
    if ((dw_lenth) % FTS_PACKET_LENGTH > 0)
    {
        temp = packet_number * FTS_PACKET_LENGTH;
        packet_buf[2] = (u8)(temp>>8);
        packet_buf[3] = (u8)(temp & 0xff);

        temp = (dw_lenth) % FTS_PACKET_LENGTH;
        printk("\n-----the remainer data is %d------\n",temp);
        packet_buf[4] = (u8)(temp>>8);
        packet_buf[5] = (u8)(temp & 0xff);

        printk("buf[2]: 0x%X, buf[3]: 0x%X, buf[4]: 0x%X, buf[5]: 0x%X\n", packet_buf[2], packet_buf[3], packet_buf[4], packet_buf[5]);

        for (i=0;i<temp;i++)
        {
            packet_buf[6+i] = pbt_buf[ packet_number*FTS_PACKET_LENGTH + i]; 
            bt_ecc ^= packet_buf[6+i];
        }
        printk("\n--transfer the rest data--\n");
        byte_write(&packet_buf[0],temp+6);    
        msleep(13);
    }
//#if 0
    /***********send the last eight bytes**********/
	
    config_add = 0x1ff00;
    packet_buf[0] = 0xbf;
    packet_buf[1] = ((config_add & 0x10000)>>16);
    packet_buf[2] = ((config_add & 0xffff)>>8);
    packet_buf[3] = (config_add & 0xff);
    temp =6;
  
    packet_buf[4] = 0;
    packet_buf[5] = temp;
    packet_buf[6] = pbt_buf[sizeof(CTPM_FW)-8]; 
    bt_ecc ^=packet_buf[6];
    packet_buf[7] = pbt_buf[sizeof(CTPM_FW)-7]; 
    bt_ecc ^=packet_buf[7];
    packet_buf[8] = pbt_buf[sizeof(CTPM_FW)-6]; 
    bt_ecc ^=packet_buf[8];
    packet_buf[9] = pbt_buf[sizeof(CTPM_FW)-5]; 
    bt_ecc ^=packet_buf[9];
    packet_buf[10] = pbt_buf[sizeof(CTPM_FW)-4]; 
    bt_ecc ^=packet_buf[10];
    packet_buf[11] = pbt_buf[sizeof(CTPM_FW)-3]; 
    bt_ecc ^=packet_buf[11];
    byte_write(&packet_buf[0],12); 
    msleep(20);

    /********send the checkout************/
    cmd_write(0xcc,0x00,0x00,0x00,1);
	
    //printk("\n  byte read error \n");

    byte_read(reg_val,1);
    printk("[TSP] Step 6:  ecc read 0x%x, new firmware 0x%x. \n", reg_val[0], bt_ecc);
    if(reg_val[0] != bt_ecc)
    {
        printk("ECC Error\n");
        return ERR_ECC;
    }

    /*******Step 7: reset the new FW**********/
    cmd_write(0x07,0x00,0x00,0x00,1);

    gpio_set_value(TOUCH_RST_N, 0);
    msleep(10);
    gpio_set_value(TOUCH_RST_N, 1);
    msleep(40);

    return ERR_OK;
}




int fts_ctpm_fw_upgrade_with_i_file(void)
{
    u8* pbt_buf = FTS_NULL;
    int i_ret;
    u16 update_length;

    pbt_buf = CTPM_FW;
#ifdef FT5606_NEW_DRIVER
    printk("CTPM_FW Size: %d\n", sizeof(CTPM_FW));
    printk("\n   the buf lenth high  is %d,low is %d \n",pbt_buf[sizeof(CTPM_FW)-8],pbt_buf[sizeof(CTPM_FW)-7]);
    update_length=pbt_buf[sizeof(CTPM_FW)-8]<<8|pbt_buf[sizeof(CTPM_FW)-7];
    printk("\n   the buf lenth is %d ",update_length);
    //i_ret =  fts_ctpm_fw_upgrade(pbt_buf,update_length);
    i_ret = fts_ctpm_fw_upgrade(pbt_buf, sizeof(CTPM_FW));
#else
    i_ret =  fts_ctpm_fw_upgrade(pbt_buf,sizeof(CTPM_FW));
#endif

   return i_ret;
}

unsigned char fts_ctpm_get_upg_ver(void)
{
    unsigned int ui_sz;

    ui_sz = sizeof(CTPM_FW);
    if (ui_sz > 2)
    {
        return CTPM_FW[ui_sz - 2];
    }
    else
        return 0xff;

}


static int fts_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct FTS_TS_DATA_T *ft5x0x_ts;
    struct input_dev *input_dev;
    int err = 0;
    unsigned char reg_value = 0;
    unsigned char reg_version = 0;
    unsigned char check_firmware = 0;
    int i;

    //_sui_irq_num = IRQ_EINT(6);
    _sui_irq_num = (unsigned int)client->irq;

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        err = -ENODEV;
        goto exit_check_functionality_failed;
    }

    ft5x0x_ts = kzalloc(sizeof(*ft5x0x_ts), GFP_KERNEL);
    if (!ft5x0x_ts)    {
        err = -ENOMEM;
        goto exit_alloc_data_failed;
    }

    this_client = client;
    i2c_set_clientdata(client, ft5x0x_ts);

    INIT_WORK(&ft5x0x_ts->pen_event_work, fts_work_func);

    ft5x0x_ts->ts_workqueue = create_singlethread_workqueue(dev_name(&client->dev));
    if (!ft5x0x_ts->ts_workqueue) {
        err = -ESRCH;
        goto exit_create_singlethread;
    }

    // Force reset
    gpio_set_value(TOUCH_RST_N, 0);
    msleep(10);
    gpio_set_value(TOUCH_RST_N, 1);
    msleep(40);

    /***wait CTP to bootup normally***/
    msleep(400);

    check_firmware = fts_register_read(FT5X0X_REG_FIRMID, &reg_version,1);
    printk("[TSP] firmware version = 0x%2x\n", reg_version);
    fts_register_read(FT5X0X_REG_REPORT_RATE, &reg_value,1);
    printk("[TSP]firmware report rate = %dHz\n", reg_value*10);
    fts_register_read(FT5X0X_REG_THRES, &reg_value,1);
    printk("[TSP]firmware threshold = %d\n", reg_value * 4);
    fts_register_read(FT5X0X_REG_NOISE_MODE, &reg_value,1);
    printk("[TSP]noise mode = 0x%2x\n", reg_value);

#ifdef CONFIG_HAS_EARLYSUSPEND
    printk("\n [TSP]:register the early suspend \n");
    ft5x0x_ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
    ft5x0x_ts->early_suspend.suspend = fts_ts_suspend;
    ft5x0x_ts->early_suspend.resume  = fts_ts_resume;
    register_early_suspend(&ft5x0x_ts->early_suspend);
#endif

    #if 1
    if ((fts_ctpm_get_upg_ver() > reg_version) && (fts_ctpm_get_upg_ver() != 0xff) && (check_firmware == FTS_TRUE))
    {
        printk("[TSP] start upgrade new verison 0x%2x\n", fts_ctpm_get_upg_ver());
        msleep(200);
        err =  fts_ctpm_fw_upgrade_with_i_file();
        if (err == 0)
        {
            printk("[TSP] ugrade successfuly.\n");
            msleep(400);
            fts_register_read(FT5X0X_REG_FIRMID, &reg_value,1);
            printk("FTS_DBG from old version 0x%2x to new version = 0x%2x\n", reg_version, reg_value);
        }
        else
        {
            printk("[TSP]  ugrade fail err=%d, line = %d.\n",
                err, __LINE__);
        }
        msleep(4000);
    }
    #endif
    err = request_irq(_sui_irq_num, fts_ts_irq, IRQF_TRIGGER_FALLING, "qt602240_ts", ft5x0x_ts);
    //err = request_irq(client->irq, fts_ts_irq, IRQF_TRIGGER_FALLING, "qt602240_ts", ft5x0x_ts);

    if (err < 0) {
        dev_err(&client->dev, "ft5x0x_probe: request irq failed\n");
        goto exit_irq_request_failed;
    }
    disable_irq(_sui_irq_num);
    //disable_irq(client->irq);

    input_dev = input_allocate_device();
    if (!input_dev) {
        err = -ENOMEM;
        dev_err(&client->dev, "failed to allocate input device\n");
        goto exit_input_dev_alloc_failed;
    }

    ft5x0x_ts->input_dev = input_dev;

    /***setup coordinate area******/
    set_bit(EV_ABS, input_dev->evbit);
    set_bit(ABS_MT_TOUCH_MAJOR, input_dev->absbit);
    set_bit(ABS_MT_POSITION_X, input_dev->absbit);
    set_bit(ABS_MT_POSITION_Y, input_dev->absbit);
    set_bit(ABS_MT_WIDTH_MAJOR, input_dev->absbit);
    set_bit(ABS_MT_PRESSURE, input_dev->absbit);

    /****** for multi-touch *******/
    for (i=0; i<CFG_MAX_POINT_NUM; i++)
        _st_finger_infos[i].u2_pressure = -1;

    input_set_abs_params(input_dev,
                 ABS_MT_POSITION_X, 0, SCREEN_MAX_X - 1, 0, 0);
    input_set_abs_params(input_dev,
                 ABS_MT_POSITION_Y, 0, SCREEN_MAX_Y - 1, 0, 0);
    input_set_abs_params(input_dev,
                 ABS_MT_TOUCH_MAJOR, 0, PRESS_MAX, 0, 0);
    input_set_abs_params(input_dev,
                 ABS_MT_TRACKING_ID, 0, 30, 0, 0);
    input_set_abs_params(input_dev,
                 ABS_MT_PRESSURE, 0, 32, 0, 0);

    input_set_abs_params(input_dev, 
                 ABS_X, 0, SCREEN_MAX_X - 1, 0, 0);
    input_set_abs_params(input_dev, 
                 ABS_Y, 0, SCREEN_MAX_Y - 1, 0, 0);

    /*****setup key code area******/
    set_bit(EV_SYN, input_dev->evbit);
    set_bit(EV_KEY, input_dev->evbit);
    set_bit(BTN_TOUCH, input_dev->keybit);

    input_dev->keycode = tsp_keycodes;
    for(i = 0; i < CFG_NUMOFKEYS; i++)
    {
        input_set_capability(input_dev, EV_KEY, ((int*)input_dev->keycode)[i]);
        tsp_keystatus[i] = KEY_RELEASE;
    }

    input_dev->name        = FT5X0X_NAME;
    err = input_register_device(input_dev);
    if (err) {
        dev_err(&client->dev,
        "fts_ts_probe: failed to register input device: %s\n",
        dev_name(&client->dev));
        goto exit_input_register_device_failed;
    }

    // Create SYSFS
    err = sysdev_class_register(&touch_sysclass);
    if (err) goto exit_sysdev_register_device_failed;
    err = sysdev_register(&device_touch);
    if (err) goto exit_sysdev_register_device_failed;
    err = sysdev_create_file(&device_touch, &attr_ftmping);
    if (err) goto exit_sysdev_register_device_failed;
    err = sysdev_create_file(&device_touch, &attr_ftmgetversion);
    if (err) goto exit_sysdev_register_device_failed;
    err = sysdev_create_file(&device_touch, &attr_reset_n);
    if (err) goto exit_sysdev_register_device_failed;

    enable_irq(_sui_irq_num);
    //enable_irq(client->irq);
    printk("[TSP] file(%s), function (%s), -- end\n", __FILE__, __FUNCTION__);
    return 0;

exit_sysdev_register_device_failed:
    printk("Touch Probe Sysdev Create File Error %d\n", err);
exit_input_register_device_failed:
    input_free_device(input_dev);
exit_input_dev_alloc_failed:
    free_irq(_sui_irq_num, ft5x0x_ts);
    //free_irq(client->irq, ft5x0x_ts);
exit_irq_request_failed:
    cancel_work_sync(&ft5x0x_ts->pen_event_work);
    destroy_workqueue(ft5x0x_ts->ts_workqueue);
exit_create_singlethread:
    printk("[TSP] ==singlethread error =\n");
    i2c_set_clientdata(client, NULL);
    kfree(ft5x0x_ts);
exit_alloc_data_failed:
exit_check_functionality_failed:
    return err;
}



static int __devexit fts_ts_remove(struct i2c_client *client)
{
    struct FTS_TS_DATA_T *ft5x0x_ts;

    ft5x0x_ts = (struct FTS_TS_DATA_T *)i2c_get_clientdata(client);
    free_irq(_sui_irq_num, ft5x0x_ts);
    //free_irq(client->irq, ft5x0x_ts);

    sysdev_remove_file(&device_touch, &attr_ftmping);
    sysdev_remove_file(&device_touch, &attr_ftmgetversion);
    sysdev_remove_file(&device_touch, &attr_reset_n);
    

    input_unregister_device(ft5x0x_ts->input_dev);
    kfree(ft5x0x_ts);
    cancel_work_sync(&ft5x0x_ts->pen_event_work);
    destroy_workqueue(ft5x0x_ts->ts_workqueue);
    i2c_set_clientdata(client, NULL);
    return 0;
}



static const struct i2c_device_id ft5x0x_ts_id[] = {
    { FT5X0X_NAME, 0 },{ }
};


MODULE_DEVICE_TABLE(i2c, ft5x0x_ts_id);

static struct i2c_driver fts_ts_driver = {
    .probe        = fts_ts_probe,
    .remove        = __devexit_p(fts_ts_remove),
    .id_table    = ft5x0x_ts_id,
    .driver    = {
        .name = FT5X0X_NAME,
    },
};

static int __init fts_ts_init(void)
{
    return i2c_add_driver(&fts_ts_driver);
}


static void __exit fts_ts_exit(void)
{
    i2c_del_driver(&fts_ts_driver);
}



module_init(fts_ts_init);
module_exit(fts_ts_exit);

MODULE_AUTHOR("<duxx@Focaltech-systems.com>");
MODULE_DESCRIPTION("FocalTech ft5x0x TouchScreen driver");
MODULE_LICENSE("GPL");
