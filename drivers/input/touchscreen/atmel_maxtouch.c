/*
 *  Atmel maXTouch Touchscreen Controller
 *
 *
 *  Copyright (C) 2010 Atmel Corporation
 *  Copyright (C) 2009 Raphael Derosso Pereira <raphaelpereira@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/debugfs.h>
#include <linux/cdev.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#define ATMEL_MAXTOUCH_DRV_DEF
#include <linux/i2c/atmel_maxtouch.h>
#include <mach/gpio.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

/*
 * This is a driver for the Atmel maXTouch Object Protocol
 *
 * When the driver is loaded, mxt_init is called.
 * mxt_driver registers the "mxt_driver" structure in the i2c subsystem
 * The mxt_idtable.name string allows the board support to associate
 * the driver with its own data.
 *
 * The i2c subsystem will call the mxt_driver.probe == mxt_probe
 * to detect the device.
 * mxt_probe will reset the maXTouch device, and then
 * determine the capabilities of the I2C peripheral in the
 * host processor (needs to support BYTE transfers)
 *
 * If OK; mxt_probe will try to identify which maXTouch device it is
 * by calling mxt_identify.
 *
 * If a known device is found, a linux input device is initialized
 * the "mxt" device data structure is allocated,
 * as well as an input device structure "mxt->input"
 * "mxt->client" is provided as a parameter to mxt_probe.
 *
 * mxt_read_object_table is called to determine which objects
 * are present in the device, and to determine their adresses.
 *
 *
 * Addressing an object:
 *
 * The object is located at a 16-bit address in the object address space.
 *
 * The address is provided through an object descriptor table in the beginning
 * of the object address space. This address can change between firmware
 * revisions, so it's important that the driver will make no assumptions
 * about addresses but instead reads the object table and gets the correct
 * addresses there.
 *
 * Each object type can have several instances, and the number of
 * instances is available in the object table as well.
 *
 * The base address of the first instance of an object is stored in
 * "mxt->object_table[object_type].chip_addr",
 * This is indexed by the object type and allows direct access to the
 * first instance of an object.
 *
 * Each instance of an object is assigned a "Report Id" uniquely identifying
 * this instance. Information about this instance is available in the
 * "mxt->report_id" variable, which is a table indexed by the "Report Id".
 *
 * The maXTouch object protocol supports adding a checksum to messages.
 * By setting the most significant bit of the maXTouch address,
 * an 8 bit checksum is added to all writes.
 *
 *
 * How to use driver.
 * -----------------
 * Example:
 * In arch/avr32/boards/atstk1000/atstk1002.c
 * an "i2c_board_info" descriptor is declared.
 * This contains info about which driver ("mXT224"),
 * which i2c address and which pin for CHG interrupts are used.
 *
 * In the "atstk1002_init" routine, "i2c_register_board_info" is invoked
 * with this information. Also, the I/O pins are configured, and the I2C
 * controller registered is on the application processor.
 *
 *
 */

//#define MXT224_SYSFS

static int debug = DEBUG_TRACE;
static int comms;
module_param(debug, int, 0644);
module_param(comms, int, 0644);

MODULE_PARM_DESC(debug, "Activate debugging output");
MODULE_PARM_DESC(comms, "Select communications mode");

#define I2C_RETRY_COUNT 5
#define I2C_PAYLOAD_SIZE 254

static int reconfig = 0;
static int early_lock = 0;

/* Returns the start address of object in mXT memory. */
#define MXT_BASE_ADDR(object_type, mxt)                 \
  get_object_address(object_type, 0, mxt->object_table, mxt->device_info.num_objs)

/* Maps a report ID to an object type (object type number). */
#define REPORT_ID_TO_OBJECT(rid, mxt)                   \
  (((rid) == 0xff) ? 0 : mxt->rid_map[rid].object)

/* Maps a report ID to an object type (string). */
#define REPORT_ID_TO_OBJECT_NAME(rid, mxt)              \
  object_type_name[REPORT_ID_TO_OBJECT(rid, mxt)]

/* Returns non-zero if given object is a touch object */
#define IS_TOUCH_OBJECT(object)                   \
  ((object == MXT_TOUCH_MULTITOUCHSCREEN_T9) ||   \
   (object == MXT_TOUCH_KEYARRAY_T15) ||          \
   (object == MXT_TOUCH_PROXIMITY_T23) ||         \
   (object == MXT_TOUCH_SINGLETOUCHSCREEN_T10) || \
   (object == MXT_TOUCH_XSLIDER_T11) ||           \
   (object == MXT_TOUCH_YSLIDER_T12) ||           \
   (object == MXT_TOUCH_XWHEEL_T13) ||            \
   (object == MXT_TOUCH_YWHEEL_T14) ||            \
   (object == MXT_TOUCH_KEYSET_T31) ||            \
   (object == MXT_TOUCH_XSLIDERSET_T32) ? 1 : 0)

#define mxt_debug(level, ...) \
  do                          \
  {                           \
    if (debug >= (level))     \
      printk(__VA_ARGS__);    \
  } while (0)

static const u8 *object_type_name[] = 
{
  [0] = "Reserved",
  [5] = "GEN_MESSAGEPROCESSOR_T5",
  [6] = "GEN_COMMANDPROCESSOR_T6",
  [7] = "GEN_POWERCONFIG_T7",
  [8] = "GEN_ACQUIRECONFIG_T8",
  [9] = "TOUCH_MULTITOUCHSCREEN_T9",
  [15] = "TOUCH_KEYARRAY_T15",
  [18] = "SPT_COMMSCONFIG_T18",
  [19] = "SPT_GPIOPWM_T19",
  [20] = "PROCI_GRIPFACESUPPRESSION_T20",
  [22] = "PROCG_NOISESUPPRESSION_T22",
  [23] = "TOUCH_PROXIMITY_T23",
  [24] = "PROCI_ONETOUCHGESTUREPROCESSOR_T24",
  [25] = "SPT_SELFTEST_T25",
  [27] = "PROCI_TWOTOUCHGESTUREPROCESSOR_T27",
  [28] = "SPT_CTECONFIG_T28",
  [37] = "DEBUG_DIAGNOSTICS_T37",
  [38] = "SPT_USER_DATA_T38",
  [40] = "PROCI_GRIPSUPPRESSION_T40",
  [41] = "PROCI_PALMSUPPRESSION_T41",
  [42] = "PROCI_FACESUPPRESSION_T42",
  [43] = "SPT_DIGITIZER_T43",
  [44] = "SPT_MESSAGECOUNT_T44",
};

static u8 suspend_config[2];
static u8 config[32];
static u16 get_object_address(uint8_t object_type, uint8_t instance, struct mxt_object *object_table, int max_objs);

/*
 * Reads a block of bytes from given address from mXT chip. If we are
 * reading from message window, and previous read was from message window,
 * there's no need to write the address pointer: the mXT chip will
 * automatically set the address pointer back to message window start.
 */
static int mxt_read_block(struct i2c_client *client, u16 addr, u16 length, u8 *value)
{
  struct i2c_adapter *adapter = client->adapter;
  struct i2c_msg msg[2];
  __le16 le_addr;
  struct mxt_data *mxt;

  mxt = i2c_get_clientdata(client);

  if(mxt != NULL)
  {
    if((mxt->last_read_addr == addr) && (addr == mxt->msg_proc_addr))
    {
      if (i2c_master_recv(client, value, length) == length)
        return length;
      else
        return -EIO;
    }
    else
    {
      mxt->last_read_addr = addr;
    }
  }

  //mxt_debug(DEBUG_TRACE, "Writing address pointer & reading %d bytes in on i2c transaction...\n", length);

  le_addr = cpu_to_le16(addr);
  msg[0].addr = client->addr;
  msg[0].flags = 0x00;
  msg[0].len = 2;
  msg[0].buf = (u8 *) &le_addr;

  msg[1].addr = client->addr;
  msg[1].flags = I2C_M_RD;
  msg[1].len = length;
  msg[1].buf = (u8 *) value;
  if (i2c_transfer(adapter, msg, 2) == 2)
    return length;
  else
    return -EIO;
}

/* Writes one byte to given address in mXT chip. */
static int mxt_write_byte(struct i2c_client *client, u16 addr, u8 value)
{
  struct
  {
    __le16 le_addr;
    u8 data;
  }i2c_byte_transfer;

  struct mxt_data *mxt;

  mxt = i2c_get_clientdata(client);
  if(mxt != NULL)
    mxt->last_read_addr = -1;
  i2c_byte_transfer.le_addr = cpu_to_le16(addr);
  i2c_byte_transfer.data = value;
  if(i2c_master_send(client, (u8 *) &i2c_byte_transfer, 3) == 3)
    return 0;
  else
    return -EIO;
}

/* Writes a block of bytes (max 256) to given address in mXT chip. */
static int mxt_write_block(struct i2c_client *client, u16 addr, u16 length, u8 *value)
{
  int i;
  struct
  {
    __le16 le_addr;
    u8 data[256];
  }i2c_block_transfer;

  struct mxt_data *mxt;

  mxt_debug(DEBUG_TRACE, "Writing %d bytes to %d...", length, addr);
  if(length > 256)
    return -EINVAL;
  mxt = i2c_get_clientdata(client);
  if(mxt != NULL)
    mxt->last_read_addr = -1;
  for(i = 0; i < length; i++)
    i2c_block_transfer.data[i] = *value++;
  i2c_block_transfer.le_addr = cpu_to_le16(addr);
  i = i2c_master_send(client, (u8 *) &i2c_block_transfer, length + 2);
  if(i == (length + 2))
    return length;
  else
    return -EIO;
}


#ifdef MXT224_SYSFS
ssize_t debug_data_read(struct mxt_data *mxt, char *buf, size_t count, loff_t *ppos, u8 debug_command)
{
  int i;
  u16 *data;
  u16 diagnostics_reg;
  int offset = 0;
  int size;
  int read_size;
  int error;
  char *buf_start;
  u16 debug_data_addr;
  u16 page_address;
  u8 page;
  u8 debug_command_reg;
  
  data = mxt->debug_data;
  if(data == NULL)
    return -EIO;
  
  /* If first read after open, read all data to buffer. */
  if(mxt->current_debug_datap == 0)
  {
    diagnostics_reg = MXT_BASE_ADDR(MXT_GEN_COMMANDPROCESSOR_T6, mxt) + MXT_ADR_T6_DIAGNOSTIC;
    if(count > (mxt->device_info.num_nodes * 2))
      count = mxt->device_info.num_nodes;
  
    debug_data_addr = MXT_BASE_ADDR(MXT_DEBUG_DIAGNOSTIC_T37, mxt) +
          MXT_ADR_T37_DATA;
      page_address = MXT_BASE_ADDR(MXT_DEBUG_DIAGNOSTIC_T37, mxt) +
          MXT_ADR_T37_PAGE;
      error = mxt_read_block(mxt->client, page_address, 1, &page);
      if (error < 0)
          return error;
      mxt_debug(DEBUG_TRACE, "debug data page = %d\n", page);
      while (page != 0) {
          error = mxt_write_byte(mxt->client,
                         diagnostics_reg,
                         MXT_CMD_T6_PAGE_DOWN);
          if (error < 0)
              return error;
          /* Wait for command to be handled; when it has, the
             register will be cleared. */
          debug_command_reg = 1;
          while (debug_command_reg != 0) {
              error = mxt_read_block(mxt->client,
                             diagnostics_reg, 1,
                             &debug_command_reg);
              if (error < 0)
                  return error;
              mxt_debug(DEBUG_TRACE,
                    "Waiting for debug diag command "
                    "to propagate...\n");
  
          }
          error = mxt_read_block(mxt->client, page_address, 1,
                         &page);
          if (error < 0)
              return error;
          mxt_debug(DEBUG_TRACE, "debug data page = %d\n", page);
      }
  
      /*
       * Lock mutex to prevent writing some unwanted data to debug
       * command register. User can still write through the char
       * device interface though. TODO: fix?
       */
  
      mutex_lock(&mxt->debug_mutex);
      /* Configure Debug Diagnostics object to show deltas/refs */
      error = mxt_write_byte(mxt->client, diagnostics_reg,
                     debug_command);
  
      /* Wait for command to be handled; when it has, the
       * register will be cleared. */
      debug_command_reg = 1;
      while (debug_command_reg != 0) {
          error = mxt_read_block(mxt->client,
                         diagnostics_reg, 1,
                         &debug_command_reg);
          if (error < 0)
              return error;
          mxt_debug(DEBUG_TRACE, "Waiting for debug diag command "
                "to propagate...\n");
  
      }
  
      if (error < 0) {
          printk(KERN_WARNING
                 "Error writing to maXTouch device!\n");
          return error;
      }
  
      size = mxt->device_info.num_nodes * sizeof(u16);
  
      while (size > 0) {
          read_size = size > 128 ? 128 : size;
          mxt_debug(DEBUG_TRACE,
                "Debug data read loop, reading %d bytes...\n",
                read_size);
          error = mxt_read_block(mxt->client,
                         debug_data_addr,
                         read_size,
                         (u8 *) &data[offset]);
          if (error < 0) {
              printk(KERN_WARNING
                     "Error reading debug data\n");
              goto error;
          }
          offset += read_size / 2;
          size -= read_size;
  
          /* Select next page */
          error = mxt_write_byte(mxt->client, diagnostics_reg,
                         MXT_CMD_T6_PAGE_UP);
          if (error < 0) {
              printk(KERN_WARNING
                     "Error writing to maXTouch device!\n");
              goto error;
          }
      }
      mutex_unlock(&mxt->debug_mutex);
  }
  
  buf_start = buf;
  i = mxt->current_debug_datap;
  
  while (((buf - buf_start) < (count - 6)) &&
         (i < mxt->device_info.num_nodes)) {
  
      mxt->current_debug_datap++;
      if (debug_command == MXT_CMD_T6_REFERENCES_MODE)
          buf += sprintf(buf, "%d: %5d\n", i,
                     (u16) le16_to_cpu(data[i]));
      else if (debug_command == MXT_CMD_T6_DELTAS_MODE)
          buf += sprintf(buf, "%d: %5d\n", i,
                     (s16) le16_to_cpu(data[i]));
      i++;
  }
  
  return buf - buf_start;
 error:
    mutex_unlock(&mxt->debug_mutex);
    return error;
}

ssize_t deltas_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
    return debug_data_read(file->private_data, buf, count, ppos,
                   MXT_CMD_T6_DELTAS_MODE);
}

ssize_t refs_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
    return debug_data_read(file->private_data, buf, count, ppos,
                   MXT_CMD_T6_REFERENCES_MODE);
}

int debug_data_open(struct inode *inode, struct file *file)
{
    struct mxt_data *mxt;
    int i;
    mxt = inode->i_private;
    if (mxt == NULL)
        return -EIO;
    mxt->current_debug_datap = 0;
    mxt->debug_data = kmalloc(mxt->device_info.num_nodes * sizeof(u16),
                  GFP_KERNEL);
    if (mxt->debug_data == NULL)
        return -ENOMEM;

    for (i = 0; i < mxt->device_info.num_nodes; i++)
        mxt->debug_data[i] = 7777;

    file->private_data = mxt;
    return 0;
}

int debug_data_release(struct inode *inode, struct file *file)
{
    struct mxt_data *mxt;
    mxt = file->private_data;
    kfree(mxt->debug_data);
    return 0;
}

const struct file_operations delta_fops = {
    .owner = THIS_MODULE,
    .open = debug_data_open,
    .release = debug_data_release,
    .read = deltas_read,
};

const struct file_operations refs_fops = {
    .owner = THIS_MODULE,
    .open = debug_data_open,
    .release = debug_data_release,
    .read = refs_read,
};

int mxt_memory_open(struct inode *inode, struct file *file)
{
    struct mxt_data *mxt;
    mxt = container_of(inode->i_cdev, struct mxt_data, cdev);
    if (mxt == NULL)
        return -EIO;
    file->private_data = mxt;
    return 0;
}

int mxt_message_open(struct inode *inode, struct file *file)
{
    struct mxt_data *mxt;
    mxt = container_of(inode->i_cdev, struct mxt_data, cdev_messages);
    if (mxt == NULL)
        return -EIO;
    file->private_data = mxt;
    return 0;
}


ssize_t mxt_memory_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
    int i;
    struct mxt_data *mxt;

    mxt = file->private_data;
    if (mxt->valid_ap) {
        mxt_debug(DEBUG_TRACE, "Reading %d bytes from current ap\n",
              (int)count);
        i = mxt_read_block_wo_addr(mxt->client, count, (u8 *) buf);
    } else {
        mxt_debug(DEBUG_TRACE, "Address pointer changed since set;"
              "writing AP (%d) before reading %d bytes",
              mxt->address_pointer, (int)count);
        i = mxt_read_block(mxt->client, mxt->address_pointer, count,
                   buf);
    }

    return i;
}

ssize_t mxt_memory_write(struct file *file, const char *buf, size_t count,
             loff_t *ppos)
{
    int i;
    int whole_blocks;
    int last_block_size;
    struct mxt_data *mxt;
    u16 address;

    mxt = file->private_data;
    address = mxt->address_pointer;

    mxt_debug(DEBUG_TRACE, "mxt_memory_write entered\n");
    whole_blocks = count / I2C_PAYLOAD_SIZE;
    last_block_size = count % I2C_PAYLOAD_SIZE;

    for (i = 0; i < whole_blocks; i++) {
        mxt_debug(DEBUG_TRACE, "About to write to %d...", address);
        mxt_write_block(mxt->client, address, I2C_PAYLOAD_SIZE,
                (u8 *) buf);
        address += I2C_PAYLOAD_SIZE;
        buf += I2C_PAYLOAD_SIZE;
    }

    mxt_write_block(mxt->client, address, last_block_size, (u8 *) buf);

    return count;
}

/* Writes the address pointer (to set up following reads). */
int mxt_write_ap(struct mxt_data *mxt, u16 ap)
{
    struct i2c_client *client;
    __le16 le_ap = cpu_to_le16(ap);
    client = mxt->client;
    if (mxt != NULL)
        mxt->last_read_addr = -1;
    if (i2c_master_send(client, (u8 *) &le_ap, 2) == 2) {
        mxt_debug(DEBUG_TRACE, "Address pointer set to %d\n", ap);
        return 0;
    } else {
        mxt_debug(DEBUG_INFO, "Error writing address pointer!\n");
        return -EIO;
    }
}

/* Reads a block of bytes from current address from mXT chip. */
static int mxt_read_block_wo_addr(struct i2c_client *client, u16 length, u8 *value)
{
  if(i2c_master_recv(client, value, length) == length)
  {
    mxt_debug(DEBUG_TRACE, "I2C block read ok\n");
    return length;
  }
  else
  {
    mxt_debug(DEBUG_INFO, "I2C block read failed\n");
    return -EIO;
  }
}

static int mxt_ioctl(struct inode *inode, struct file *file,
             unsigned int cmd, unsigned long arg)
{
    int retval;
    struct mxt_data *mxt;

    retval = 0;
    mxt = file->private_data;

    switch (cmd) {
    case MXT_SET_ADDRESS:
        retval = mxt_write_ap(mxt, (u16) arg);
        if (retval >= 0) {
            mxt->address_pointer = (u16) arg;
            mxt->valid_ap = 1;
        }
        break;
    case MXT_RESET:
        retval = mxt_write_byte(mxt->client,
                    MXT_BASE_ADDR
                    (MXT_GEN_COMMANDPROCESSOR_T6,
                     mxt) + MXT_ADR_T6_RESET, 1);
        break;
    case MXT_CALIBRATE:
        retval = mxt_write_byte(mxt->client,
                    MXT_BASE_ADDR
                    (MXT_GEN_COMMANDPROCESSOR_T6,
                     mxt) + MXT_ADR_T6_CALIBRATE, 1);

        break;
    case MXT_BACKUP:
        retval = mxt_write_byte(mxt->client,
                    MXT_BASE_ADDR
                    (MXT_GEN_COMMANDPROCESSOR_T6,
                     mxt) + MXT_ADR_T6_BACKUPNV,
                    MXT_CMD_T6_BACKUP);
        break;
    case MXT_NONTOUCH_MSG:
        mxt->nontouch_msg_only = 1;
        break;
    case MXT_ALL_MSG:
        mxt->nontouch_msg_only = 0;
        break;
    default:
        return -EIO;
    }

    return retval;
}

/*
 * Copies messages from buffer to user space.
 *
 * NOTE: if less than (mxt->message_size * 5 + 1) bytes requested,
 * this will return 0!
 *
 */
ssize_t mxt_message_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
    int i;
    struct mxt_data *mxt;
    char *buf_start;

    mxt = file->private_data;
    if (mxt == NULL)
        return -EIO;
    buf_start = buf;

    mutex_lock(&mxt->msg_mutex);
    /* Copy messages until buffer empty, or 'count' bytes written */
    while ((mxt->msg_buffer_startp != mxt->msg_buffer_endp) &&
           ((buf - buf_start) < (count - 5 * mxt->message_size - 1))) {

        for (i = 0; i < mxt->message_size; i++) {
            buf += sprintf(buf, "[%2X] ",
                       *(mxt->messages + mxt->msg_buffer_endp *
                     mxt->message_size + i));
        }
        buf += sprintf(buf, "\n");
        if (mxt->msg_buffer_endp < MXT_MESSAGE_BUFFER_SIZE)
            mxt->msg_buffer_endp++;
        else
            mxt->msg_buffer_endp = 0;
    }
    mutex_unlock(&mxt->msg_mutex);
    return buf - buf_start;
}

const struct file_operations mxt_message_fops = 
{
    .owner = THIS_MODULE,
    .open = mxt_message_open,
    .read = mxt_message_read,
};

const struct file_operations mxt_memory_fops = 
{
    .owner = THIS_MODULE,
    .open = mxt_memory_open,
    .read = mxt_memory_read,
    .write = mxt_memory_write,
    .ioctl = mxt_ioctl,
};
#endif /*MXT224_SYSFS*/


/* Calculates the 24-bit CRC sum. */
static u32 CRC_24(u32 crc, u8 byte1, u8 byte2)
{
  static const u32 crcpoly = 0x80001B;
  u32 result;
  u32 data_word;

  data_word = ((((u16) byte2) << 8u) | byte1);
  result = ((crc << 1u) ^ data_word);
  if(result & 0x1000000)
    result ^= crcpoly;
  return result;
}

/* Returns object address in mXT chip, or zero if object is not found */
static u16 get_object_address(uint8_t object_type, uint8_t instance, struct mxt_object *object_table, int max_objs)
{
  uint8_t object_table_index = 0;
  uint8_t address_found = 0;
  uint16_t address = 0;
  struct mxt_object *obj;

  while((object_table_index < max_objs) && !address_found)
  {
    obj = &object_table[object_table_index];
    if(obj->type == object_type)
    {
      address_found = 1;
      /* Are there enough instances defined in the FW? */
      if(obj->instances >= instance)
      {
        address = obj->chip_addr + (obj->size + 1) * instance;
      }
      else
      {
        return 0;
      }
    }
    object_table_index++;
  }
  return address;
}


/* Calculates the CRC value for mXT infoblock. */
int calculate_infoblock_crc(u32 *crc_result, u8 *data, int crc_area_size)
{
  u32 crc = 0;
  int i;

  for(i = 0; i < (crc_area_size - 1); i = i + 2)
    crc = CRC_24(crc, *(data + i), *(data + i + 1));
  /* If uneven size, pad with zero */
  if(crc_area_size & 0x0001)
    crc = CRC_24(crc, *(data + i), 0);
  /* Return only 24 bits of CRC. */
  *crc_result = (crc & 0x00FFFFFF);

  return 0;
}

void process_T9_message(u8 *message, struct mxt_data *mxt, int last_touch)
{
  struct input_dev *input;
  u8 status;
  u16 xpos = 0xFFFF;
  u16 ypos = 0xFFFF;
  u8 touch_size = 255;
  u8 touch_number;
  u8 amplitude;
  u8 report_id;

  static int stored_size[10];
  static int stored_x[10];
  static int stored_y[10];
  static int stored_amp[10];
	static int stored_press[10]; //JossCheng, add
  int i;
  int active_touches = 0;
  /*
   * If the 'last_touch' flag is set, we have received
     all the touch messages
   * there are available in this cycle, so send the
     events for touches that are
   * active.
   */
  //printk("Joss: process_T9_message last_touch=[%d]\n",last_touch);

  if(last_touch)
  {
    for(i = 0; i < 10; i++)
    {
      if(stored_size[i])
      {
#if 0 //JossCheng      
        if(stored_amp[touch_number] > 10)
#else
        if(stored_press[i])
#endif
        {
          active_touches++;
#if 1 //JossCheng
          input_report_abs(mxt->input, ABS_MT_TRACKING_ID, i);
          input_report_abs(mxt->input, ABS_MT_TOUCH_MAJOR, 255 /*stored_size[i]*/);
#else
          //input_report_abs(mxt->input, ABS_MT_TRACKING_ID, i);
          input_report_abs(mxt->input, ABS_MT_TOUCH_MAJOR, stored_size[i]);
#endif					
          input_report_abs(mxt->input, ABS_MT_POSITION_X, (1024-stored_y[i])/*stored_x[i]*/);
          input_report_abs(mxt->input, ABS_MT_POSITION_Y, (600-((stored_x[i]/7)+18))/*stored_y[i]*/);

 //         printk("Joss: org stored_size[%d]=[%d] x=[%d] y=[%d], input sent x=[%d] y=[%d]\n",i,stored_size[i],stored_x[i],stored_y[i],(1024-stored_y[i]),(600-((stored_x[i]/7)+18)));
          //input_report_abs(mxt->input, ABS_MT_POSITION_X, stored_y[i]/*stored_x[i]*/);
          //input_report_abs(mxt->input, ABS_MT_POSITION_Y, (670-(stored_x[i]/6))/*stored_y[i]*/);
          //printk("input sent x=[%d] y=[%d]\n",stored_y[i],(670-(stored_x[i]/6)));
          input_mt_sync(mxt->input);
        }
      }
    }
    if(active_touches)
    {
      input_sync(mxt->input);
	    //printk("active_touches input_count=[%d]\n",input_count);
	  }
    else
    {
      input_mt_sync(mxt->input);
      input_sync(mxt->input);
      //printk("not active_touches\n");
    }
  }
  else
  {
    if(message == NULL)
      return;
      
    input = mxt->input;
    status = message[MXT_MSG_T9_STATUS];
    report_id = message[0];

    if(status & MXT_MSGB_T9_SUPPRESS)
    {
      /* Touch has been suppressed by grip/face detection */
      mxt_debug(DEBUG_TRACE, "SUPRESS");
    }
    else
    {
#if 0
      if(status & MXT_MSGB_T9_MOVE)
      {
        input_count++;
        if(input_count < mxt->irq_samping_interval )
        {
          return;
        }
      }
      input_count=0;
#endif
      
      xpos = message[MXT_MSG_T9_XPOSMSB] * 16 + ((message[MXT_MSG_T9_XYPOSLSB] >> 4) & 0xF);
      ypos = message[MXT_MSG_T9_YPOSMSB] * 16 + ((message[MXT_MSG_T9_XYPOSLSB] >> 0) & 0xF);
      if(mxt->max_x_val < 1024)
        xpos >>= 2;
      if(mxt->max_y_val < 1024)
        ypos >>= 2;

      touch_number = message[MXT_MSG_REPORTID] - mxt->rid_map[report_id].first_rid;
      stored_x[touch_number] = xpos;
      stored_y[touch_number] = ypos;
      stored_amp[touch_number]=message[6];
      
      if(status & MXT_MSGB_T9_DETECT)
      {
        /*
         * TODO: more precise touch size calculation?
         * mXT224 reports the number of touched nodes,
         * so the exact value for touch ellipse major
         * axis length would be 2*sqrt(touch_size/pi)
         * (assuming round touch shape).
        */
        touch_size = message[MXT_MSG_T9_TCHAREA];
        touch_size = touch_size >> 2;
        if (!touch_size)
          touch_size = 1;

        stored_size[touch_number] = touch_size;

        /* Amplitude of touch has changed */
        if (status & MXT_MSGB_T9_AMP)
          amplitude = message[MXT_MSG_T9_TCHAMPLITUDE];
      }

      /* The previously reported touch has been removed. */
      if(status & MXT_MSGB_T9_RELEASE)
      {
        stored_size[touch_number] = 0;
      }
#if 1 //JossCheng, add
      if(status & MXT_MSGB_T9_PRESS)
      {
        stored_press[touch_number] = 1;
      }
      
      if(status & MXT_MSGB_T9_RELEASE)
      {
        /* The previously reported touch has been removed. */
        stored_size[touch_number] = 0;
        stored_press[touch_number] = 0;
      }
#endif			
    }
#if 0
    if(status & MXT_MSGB_T9_SUPPRESS)
    {
      mxt_debug(DEBUG_TRACE, "SUPRESS");
    }
    else
    {
      if(status & MXT_MSGB_T9_DETECT)
      {
        mxt_debug(DEBUG_TRACE, "DETECT:%s%s%s%s",
                      ((status & MXT_MSGB_T9_PRESS) ? " PRESS" : ""),
                      ((status & MXT_MSGB_T9_MOVE) ? " MOVE" : ""),
                      ((status & MXT_MSGB_T9_AMP) ? " AMP" : ""),
                      ((status & MXT_MSGB_T9_VECTOR) ? " VECT" : ""));
      }
      else if(status & MXT_MSGB_T9_RELEASE)
      {
        mxt_debug(DEBUG_TRACE, "RELEASE ");
      }
    }
    mxt_debug(DEBUG_TRACE, " X=%d, Y=%d, TOUCHSIZE=%d AMP=[%d]\n", xpos, ypos, touch_size,message[6]);
#endif
  }
  return;
}

int process_message(u8 *message, u8 object, struct mxt_data *mxt)
{
  struct i2c_client *client;
  u8 status;
  u16 xpos = 0xFFFF;
  u16 ypos = 0xFFFF;
  u8 event;
  u8 length;
  u8 report_id;
  static int lastkey=0;

  client = mxt->client;
  length = mxt->message_size;
  report_id = message[0];
  
  if((mxt->nontouch_msg_only == 0) || (!IS_TOUCH_OBJECT(object)))
  {
    mutex_lock(&mxt->msg_mutex);
    /* Copy the message to buffer */
    if(mxt->msg_buffer_startp < MXT_MESSAGE_BUFFER_SIZE)
      mxt->msg_buffer_startp++;
    else
      mxt->msg_buffer_startp = 0;

    if(mxt->msg_buffer_startp == mxt->msg_buffer_endp)
    {
      ///mxt_debug(DEBUG_TRACE, "Message buf full, discarding last entry.\n");
      if(mxt->msg_buffer_endp < MXT_MESSAGE_BUFFER_SIZE)
        mxt->msg_buffer_endp++;
      else
        mxt->msg_buffer_endp = 0;
    }
    memcpy((mxt->messages + mxt->msg_buffer_startp * length), message, length);
    mutex_unlock(&mxt->msg_mutex);
  }
  
  switch (object)
  {
    case MXT_GEN_COMMANDPROCESSOR_T6:
      //printk("MXT_GEN_COMMANDPROCESSOR_T6\n");
      status = message[1];
      if(status & MXT_MSGB_T6_COMSERR)
        dev_err(&client->dev, "maXTouch checksum error\n");
        
      if(status & MXT_MSGB_T6_CFGERR)
      {
        /* Configuration error. A proper configuration needs to be written to chip and backed up. 
           Refer to protocol document for further info. */
        dev_err(&client->dev, "maXTouch configuration error\n");
      }
      if(status & MXT_MSGB_T6_CAL)
      {
        /* Calibration in action, no need to react */
        dev_info(&client->dev, "maXTouch calibration in progress\n");
      }
      if (status & MXT_MSGB_T6_SIGERR)
      {
        /* Signal acquisition error, something is seriously wrong, 
           not much we can in the driver to correct this */
        dev_err(&client->dev, "maXTouch acquisition error\n");
      }
      if(status & MXT_MSGB_T6_OFL)
      {
        /* Cycle overflow, the acquisition is too short. Can happen temporarily when there's a complex
           touch shape on the screen requiring lots of processing. */
        dev_err(&client->dev, "maXTouch cycle overflow\n");
      }
      if(status & MXT_MSGB_T6_RESET)
      {
        /*Chip has reseted, no need to react. */
        dev_info(&client->dev, "maXTouch chip reset\n");
      }
      if(status == 0) 
      {
        /* Chip status back to normal. */
        dev_info(&client->dev, "maXTouch status normal\n");
      }
      break;

    case MXT_TOUCH_MULTITOUCHSCREEN_T9:
      //printk("MXT_TOUCH_MULTITOUCHSCREEN_T9\n");
      process_T9_message(message, mxt, 0);
      break;

    case MXT_TOUCH_KEYARRAY_T15:
      //printk("MXT_TOUCH_KEYARRAY_T15\n");
      if (early_lock)
      {
        printk("screen lock do nothing");
      }
      else
      {
	      switch(message[2])
	      {
		    case 0x1: /*back key*/
		    	lastkey = KEY_BACK;
		    	input_report_key(mxt->input, KEY_BACK, 1);
		    	printk("send KEY_BACK msg=[0x%X][0x%X][0x%X]\n",message[0],message[1],message[2]);
		    	break;
		    case 0x2: /*menu key*/
		    	lastkey = KEY_MENU;
		    	input_report_key(mxt->input, KEY_MENU, 1);
		    	input_sync(mxt->input);
		    	printk("send KEY_MENU msg=[0x%X][0x%X][0x%X]\n",message[0],message[1],message[2]);
		    	break;
		    case 0x4: /*home key*/
		    	lastkey = KEY_HOME;
		    	input_report_key(mxt->input, KEY_HOME, 1);
		    	input_sync(mxt->input);
		    	printk("send KEY_HOME msg=[0x%X][0x%X][0x%X]\n",message[0],message[1],message[2]);
		    	break;
		    case 0x8: /*search key*/
		    	lastkey = KEY_SEARCH;
		    	input_report_key(mxt->input, KEY_SEARCH, 1);
		    	input_sync(mxt->input);
		    	printk("send KEY_SEARCH msg=[0x%X][0x%X][0x%X]\n",message[0],message[1],message[2]);
		    	break;
		    default:
		    	input_report_key(mxt->input, lastkey, 0);
		    	input_sync(mxt->input);
		    	printk("send [%d] up msg=[0x%X][0x%X][0x%X]\n",lastkey,message[0],message[1],message[2]);
		    	break;
	      }
      }
	  
      break;

    case MXT_SPT_GPIOPWM_T19:
      //printk("MXT_SPT_GPIOPWM_T19\n");
      if(debug >= DEBUG_TRACE)
        dev_info(&client->dev, "Receiving GPIO message\n");
      break;

    case MXT_PROCI_GRIPFACESUPPRESSION_T20:
      //printk("MXT_PROCI_GRIPFACESUPPRESSION_T20\n");
      if(debug >= DEBUG_TRACE)
        dev_info(&client->dev, "Receiving face suppression msg\n");
      break;

    case MXT_PROCG_NOISESUPPRESSION_T22:
      //printk("MXT_PROCG_NOISESUPPRESSION_T22\n");
      if(debug >= DEBUG_TRACE)
        dev_info(&client->dev, "Receiving noise suppression msg\n");
      status = message[MXT_MSG_T22_STATUS];
      if(status & MXT_MSGB_T22_FHCHG)
      {
        if(debug >= DEBUG_TRACE)
          dev_info(&client->dev, "maXTouch: Freq changed\n");
      }
      if(status & MXT_MSGB_T22_GCAFERR)
      {
        if(debug >= DEBUG_TRACE)
          dev_info(&client->dev, "maXTouch: High noise " "level\n");
      }
      if(status & MXT_MSGB_T22_FHERR)
      {
        if(debug >= DEBUG_TRACE)
          dev_info(&client->dev, "maXTouch: Freq changed - Noise level too high\n");
      }
      break;

    case MXT_PROCI_ONETOUCHGESTUREPROCESSOR_T24:
      //printk("MXT_PROCI_ONETOUCHGESTUREPROCESSOR_T24\n");
      if(debug >= DEBUG_TRACE)
        dev_info(&client->dev, "Receiving one-touch gesture msg\n");

      event = message[MXT_MSG_T24_STATUS] & 0x0F;
      xpos = message[MXT_MSG_T24_XPOSMSB] * 16 +
          ((message[MXT_MSG_T24_XYPOSLSB] >> 4) & 0x0F);
      ypos = message[MXT_MSG_T24_YPOSMSB] * 16 +
          ((message[MXT_MSG_T24_XYPOSLSB] >> 0) & 0x0F);
      xpos >>= 2;
      ypos >>= 2;

      switch(event)
      {
        case MT_GESTURE_RESERVED:
          break;
        case MT_GESTURE_PRESS:
          break;
        case MT_GESTURE_RELEASE:
          break;
        case MT_GESTURE_TAP:
          break;
        case MT_GESTURE_DOUBLE_TAP:
          break;
        case MT_GESTURE_FLICK:
          break;
        case MT_GESTURE_DRAG:
          break;
        case MT_GESTURE_SHORT_PRESS:
          break;
        case MT_GESTURE_LONG_PRESS:
          break;
        case MT_GESTURE_REPEAT_PRESS:
          break;
        case MT_GESTURE_TAP_AND_PRESS:
          break;
        case MT_GESTURE_THROW:
          break;
        default:
          break;
      }
      break;

    case MXT_SPT_SELFTEST_T25:
      //printk("MXT_SPT_SELFTEST_T25\n");
      if(debug >= DEBUG_TRACE)
        dev_info(&client->dev, "Receiving Self-Test msg\n");

      if(message[MXT_MSG_T25_STATUS] == MXT_MSGR_T25_OK)
      {
        if(debug >= DEBUG_TRACE)
          dev_info(&client->dev, "maXTouch: Self-Test OK\n");
      }
      else
      {
        dev_err(&client->dev, "maXTouch: Self-Test Failed [%02x]: {%02x,%02x,%02x,%02x,%02x}\n",
              message[MXT_MSG_T25_STATUS],
              message[MXT_MSG_T25_STATUS + 0],
              message[MXT_MSG_T25_STATUS + 1],
              message[MXT_MSG_T25_STATUS + 2],
              message[MXT_MSG_T25_STATUS + 3],
              message[MXT_MSG_T25_STATUS + 4]);
      }
      break;

    case MXT_PROCI_TWOTOUCHGESTUREPROCESSOR_T27:
      //printk("MXT_PROCI_TWOTOUCHGESTUREPROCESSOR_T27\n");
      if(debug >= DEBUG_TRACE)
        dev_info(&client->dev, "Receiving 2-touch gesture message\n");
      break;

    case MXT_SPT_CTECONFIG_T28:
      //printk("MXT_SPT_CTECONFIG_T28\n");
      if(debug >= DEBUG_TRACE)
        dev_info(&client->dev, "Receiving CTE message...\n");
      status = message[MXT_MSG_T28_STATUS];
      if(status & MXT_MSGB_T28_CHKERR)
        dev_err(&client->dev, "maXTouch: Power-Up CRC failure\n");
      break;
 
    default:
      if(debug >= DEBUG_TRACE)
        dev_info(&client->dev, "maXTouch: Unknown message!\n");
      break;
  }

    return 0;
}

static int mxt_schedule_delayed_work(struct delayed_work *work, unsigned long delay)
{
	struct mxt_data *mxt = container_of(work, struct mxt_data, dwork);
	return queue_delayed_work(mxt->mxt_single_workqueue, work, delay);
}

/*
 * Processes messages when the interrupt line (CHG) is asserted. Keeps
 * reading messages until a message with report ID 0xFF is received,
 * which indicates that there is no more new messages.
 *
 */
static void mxt_worker(struct work_struct *work)
{
  struct mxt_data *mxt;
  struct i2c_client *client;

  u8 *message;
  u16 message_length;
  u16 message_addr;
  u8 report_id;
  u8 object;
  int error;
  int i;

  message = NULL;
  mxt = container_of(work, struct mxt_data, dwork.work);
  disable_irq(mxt->irq);
  //printk("disable_irq\n");
  client = mxt->client;
  message_addr = mxt->msg_proc_addr;
  message_length = mxt->message_size;

  if (message_length < 256)
  {
    message = kmalloc(message_length, GFP_KERNEL);
    if(message == NULL)
    {
      dev_err(&client->dev, "Error allocating memory\n");
      return;
    }
  }
  else
  {
    dev_err(&client->dev, "Message length larger than 256 bytes not supported\n");
    return;
  }
  //mxt_debug(DEBUG_TRACE, "\nmaXTouch worker active: \n");
  

  //do
  //{
    /* Read next message, reread on failure. */
    /* -1 TO WORK AROUND A BUG ON 0.9 FW MESSAGING, needs */
    /* to be changed back if checksum is read */
    for (i = 1; i < I2C_RETRY_COUNT; i++)
    {
      error = mxt_read_block(client, message_addr, message_length - 1, message);
      if(error >= 0)
        break;
      mxt->read_fail_counter++;
      dev_err(&client->dev, "Failure reading maxTouch device\n");
    }
    if(error < 0)
    {
      kfree(message);
      return;
    }

    /*read message object successful*/
    mxt->message_counter++;
    
    if(mxt->address_pointer != message_addr)
      mxt->valid_ap = 0;
      report_id = message[0];

#if 0
    if(debug >= DEBUG_RAW)
    {
      mxt_debug(DEBUG_RAW, "[%s] message [msg count: %08x]:", REPORT_ID_TO_OBJECT_NAME(report_id, mxt), mxt->message_counter);
      /* 5 characters per one byte */
      message_string = kmalloc(message_length * 5, GFP_KERNEL);
      if(message_string == NULL)
      {
        dev_err(&client->dev, "Error allocating memory\n");
        kfree(message);
        return;
      }
      message_start = message_string;
      for (i = 0; i < message_length; i++)
      {
        message_string +=
        sprintf(message_string, "0x%02X ", message[i]);
      }
      mxt_debug(DEBUG_RAW, "%s\n", message_start);
      kfree(message_start);
    }
#endif
    if((report_id != MXT_END_OF_MESSAGES) && (report_id != 0))
    {
      memcpy(mxt->last_message, message, message_length);
      mxt->new_msgs = 1;
      smp_wmb();
      /* Get type of object and process the message */
      object = mxt->rid_map[report_id].object;
      process_message(message, object, mxt);
      
    }
    //} while (comms ? (mxt->read_chg() == 0) : ((report_id != MXT_END_OF_MESSAGES) && (report_id != 0)));

    /* All messages processed, send the events) */
    process_T9_message(NULL, mxt, 1);

    kfree(message);
    enable_irq(mxt->irq);
    ///* Make sure we don't miss any interrupts and read changeline. */
    if (mxt->read_chg() == 0)
    {
      //schedule_delayed_work(&mxt->dwork, 0);
      mxt_schedule_delayed_work(&mxt->dwork, mxt->wq_samping_interval);
    }
}

/*
 * The maXTouch device will signal the host about a new message by asserting
 * the CHG line. This ISR schedules a worker routine to read the message when
 * that happens.
 */

static irqreturn_t mxt_irq_handler(int irq, void *_mxt)
{
  struct mxt_data *mxt = _mxt;
  //printk("mxt_irq_handler\n");
  //mxt->irq_counter++;
  //if (mxt->valid_interrupt())
  //{
    /* Send the signal only if falling edge generated the irq. */
    //cancel_delayed_work(&mxt->dwork);
    //schedule_delayed_work(&mxt->dwork, 0);
    mxt_schedule_delayed_work(&mxt->dwork, 0);
    //mxt->valid_irq_counter++;
  //}
  //else
  //{
    //mxt->invalid_irq_counter++;
    //return IRQ_NONE;
  //}

  return IRQ_HANDLED;
}

/******************************************************************************/
/* Initialization of driver                                                   */
/******************************************************************************/
static int __devinit mxt_identify(struct i2c_client *client, struct mxt_data *mxt, u8 *id_block_data)
{
  u8 buf[7];
  int error;
  int identified;

  identified = 0;

  /* Read Device info to check if chip is valid */
  error = mxt_read_block(client, MXT_ADDR_INFO_BLOCK, MXT_ID_BLOCK_SIZE, (u8 *) buf);
  if (error < 0)
  {
    mxt->read_fail_counter++;
    dev_err(&client->dev, "Failure accessing maXTouch device\n");
    return -EIO;
  }

  memcpy(id_block_data, buf, MXT_ID_BLOCK_SIZE);

  mxt->device_info.family_id = buf[0];
  mxt->device_info.variant_id = buf[1];
  mxt->device_info.major = ((buf[2] >> 4) & 0x0F);
  mxt->device_info.minor = (buf[2] & 0x0F);
  mxt->device_info.build = buf[3];
  mxt->device_info.x_size = buf[4];
  mxt->device_info.y_size = buf[5];
  mxt->device_info.num_objs = buf[6];
  mxt->device_info.num_nodes = mxt->device_info.x_size * mxt->device_info.y_size;

  /*
   * Check Family & Variant Info; warn if not recognized but
   * still continue.
   */

  /* MXT224 */
  if(mxt->device_info.family_id == MXT224_FAMILYID)
  {
    strcpy(mxt->device_info.family_name, "mXT224");

    if(mxt->device_info.variant_id == MXT224_CAL_VARIANTID)
    {
      strcpy(mxt->device_info.variant_name, "Calibrated");
    }
    else if(mxt->device_info.variant_id == MXT224_UNCAL_VARIANTID)
    {
      strcpy(mxt->device_info.variant_name, "Uncalibrated");
    }
    else
    {
      dev_err(&client->dev, "Warning: maXTouch Variant ID [%d] not supported\n", mxt->device_info.variant_id);
      strcpy(mxt->device_info.variant_name, "UNKNOWN");
      identified = -ENXIO;
    }
  }
  else
  {
    dev_err(&client->dev, "Warning: maXTouch Family ID [%d] not supported\n", mxt->device_info.family_id);
    strcpy(mxt->device_info.family_name, "UNKNOWN");
    strcpy(mxt->device_info.variant_name, "UNKNOWN");
    identified = -ENXIO;
  }

  dev_info(&client->dev, "Atmel maXTouch (Family %s (%X), Variant %s (%X)) Firmware version [%d.%d] Build %d\n",
         mxt->device_info.family_name,
         mxt->device_info.family_id,
         mxt->device_info.variant_name,
         mxt->device_info.variant_id,
         mxt->device_info.major,
         mxt->device_info.minor, mxt->device_info.build);
  dev_info(&client->dev, "Atmel maXTouch Configuration [X: %d] x [Y: %d] num_objs=[%d]\n", mxt->device_info.x_size, mxt->device_info.y_size,mxt->device_info.num_objs);
  return identified;
}

/*
 * Reads the object table from maXTouch chip to get object data like
 * address, size, report id. For Info Block CRC calculation, already read
 * id data is passed to this function too (Info Block consists of the ID
 * block and object table).
 *
 */
static int __devinit mxt_read_object_table(struct i2c_client *client, struct mxt_data *mxt, u8 *raw_id_data)
{
  u16 report_id_count;
  u8 buf[MXT_OBJECT_TABLE_ELEMENT_SIZE];
  u8 *raw_ib_data;
  u8 object_type;
  u16 object_address;
  u16 object_size;
  u8 object_instances;
  u8 object_report_ids;
  u16 object_info_address;
  u32 crc;
  u32 calculated_crc;
  int i;
  int error;

  u8 object_instance;
  u8 object_report_id;
  u8 report_id;
  int first_report_id;
  int ib_pointer;
  struct mxt_object *object_table;

  mxt_debug(DEBUG_TRACE, "maXTouch driver reading configuration\n");

  object_table = kzalloc(sizeof(struct mxt_object) * mxt->device_info.num_objs, GFP_KERNEL);
  if(object_table == NULL)
  {
    printk(KERN_WARNING "maXTouch: Memory allocation failed!\n");
    error = -ENOMEM;
    goto err_object_table_alloc;
  }

  raw_ib_data = kmalloc(MXT_OBJECT_TABLE_ELEMENT_SIZE * mxt->device_info.num_objs + MXT_ID_BLOCK_SIZE, GFP_KERNEL);
  if (raw_ib_data == NULL)
  {
    printk(KERN_WARNING "maXTouch: Memory allocation failed!\n");
    error = -ENOMEM;
    goto err_ib_alloc;
  }

  /* Copy the ID data for CRC calculation. */
  memcpy(raw_ib_data, raw_id_data, MXT_ID_BLOCK_SIZE);
  ib_pointer = MXT_ID_BLOCK_SIZE;

  mxt->object_table = object_table;

  mxt_debug(DEBUG_TRACE, "maXTouch driver Memory allocated\n");

  object_info_address = MXT_ADDR_OBJECT_TABLE;

  report_id_count = 0;
  for(i = 0; i < mxt->device_info.num_objs; i++)
  {
    mxt_debug(DEBUG_TRACE, "Reading maXTouch at [0x%04x]: ", object_info_address);

    error = mxt_read_block(client, object_info_address, MXT_OBJECT_TABLE_ELEMENT_SIZE, buf);

    if(error < 0)
    {
      mxt->read_fail_counter++;
      dev_err(&client->dev, "maXTouch Object %d could not be read\n", i);
      error = -EIO;
      goto err_object_read;
    }

    memcpy(raw_ib_data + ib_pointer, buf, MXT_OBJECT_TABLE_ELEMENT_SIZE);
    ib_pointer += MXT_OBJECT_TABLE_ELEMENT_SIZE;

    object_type = buf[0];
    object_address = (buf[2] << 8) + buf[1];
    object_size = buf[3] + 1;
    object_instances = buf[4] + 1;
    object_report_ids = buf[5];
    mxt_debug(DEBUG_TRACE, "Type=[%03d], Address=[0x%04x], Size=[0x%02x], [%d] instances, [%d] report id's\n",
              object_type,
              object_address,
              object_size, object_instances, object_report_ids);

    if(object_type > MXT_MAX_OBJECT_TYPES)
    {
      /* Unknown object type */
      printk("maXTouch object type [%d] not recognized\n",object_type);
      return -ENXIO;
    }
    /* Save frequently needed info. */
    if(object_type == MXT_GEN_MESSAGEPROCESSOR_T5)
    {
      mxt->msg_proc_addr = object_address;
      mxt->message_size = object_size;
      printk(KERN_ALERT "message length: %d\n", object_size);
    }

    object_table[i].type = object_type;
    object_table[i].chip_addr = object_address;
    object_table[i].size = object_size;
    object_table[i].instances = object_instances;
    object_table[i].num_report_ids = object_report_ids;
    report_id_count += object_instances * object_report_ids;

    object_info_address += MXT_OBJECT_TABLE_ELEMENT_SIZE;
  }
  /* allocate for report_id 0, even if not used */
  mxt->rid_map = kzalloc(sizeof(struct report_id_map) * (report_id_count + 1), GFP_KERNEL);
  if (mxt->rid_map == NULL)
  {
    printk(KERN_WARNING "maXTouch: Can't allocate memory!\n");
    error = -ENOMEM;
    goto err_rid_map_alloc;
  }

  mxt->messages = kzalloc(mxt->message_size * MXT_MESSAGE_BUFFER_SIZE, GFP_KERNEL);
  if (mxt->messages == NULL)
  {
    printk(KERN_WARNING "maXTouch: Can't allocate memory!\n");
    error = -ENOMEM;
    goto err_msg_alloc;
  }

  mxt->last_message = kzalloc(mxt->message_size, GFP_KERNEL);
  if(mxt->last_message == NULL)
  {
    printk(KERN_WARNING "maXTouch: Can't allocate memory!\n");
    error = -ENOMEM;
    goto err_msg_alloc;
  }

  mxt->report_id_count = report_id_count;
  if(report_id_count > 254) /* 0 & 255 are reserved */
  {
    dev_err(&client->dev, "Too many maXTouch report id's [%d]\n", report_id_count);
    error = -ENXIO;
    goto err_max_rid;
  }

  /* Create a mapping from report id to object type */
  report_id = 1;        /* Start from 1, 0 is reserved. */

  /* Create table associating report id's with objects & instances */
  for(i = 0; i < mxt->device_info.num_objs; i++)
  {
    for (object_instance = 0; object_instance < object_table[i].instances; object_instance++)
    {
      first_report_id = report_id;
      for (object_report_id = 0; object_report_id < object_table[i].num_report_ids; object_report_id++)
      {
        mxt->rid_map[report_id].object = object_table[i].type;
        mxt->rid_map[report_id].instance = object_instance;
        mxt->rid_map[report_id].first_rid = first_report_id;
        report_id++;
      }
    }
  }

  /* Read 3 byte CRC */
  error = mxt_read_block(client, object_info_address, 3, buf);
  if (error < 0)
  {
    mxt->read_fail_counter++;
    dev_err(&client->dev, "Error reading CRC\n");
  }

  crc = (buf[2] << 16) | (buf[1] << 8) | buf[0];

  if (calculate_infoblock_crc(&calculated_crc, raw_ib_data, ib_pointer))
  {
    printk(KERN_WARNING "Error while calculating CRC!\n");
    calculated_crc = 0;
  }
  kfree(raw_ib_data);

  mxt_debug(DEBUG_TRACE, "\nReported info block CRC = 0x%6X\n", crc);
  mxt_debug(DEBUG_TRACE, "Calculated info block CRC = 0x%6X\n\n", calculated_crc);

  if (crc == calculated_crc)
  {
    mxt->info_block_crc = crc;
    if(crc != 0x5D9A69)
      reconfig = 1;
  }
  else
  {
    mxt->info_block_crc = 0;
    reconfig = 1;
    printk(KERN_ALERT "maXTouch: Info block CRC invalid!\n");
  }

  if(debug >= DEBUG_VERBOSE)
  {
    dev_info(&client->dev, "maXTouch: %d Objects\n", mxt->device_info.num_objs);

    for(i = 0; i < mxt->device_info.num_objs; i++)
    {
      dev_info(&client->dev, "Type:\t\t\t[%d]: %s\n", object_table[i].type, object_type_name[object_table[i].type]);
      dev_info(&client->dev, "\tAddress:\t0x%04X\n",  object_table[i].chip_addr);
      dev_info(&client->dev, "\tSize:\t\t%d Bytes\n", object_table[i].size);
      dev_info(&client->dev, "\tInstances:\t%d\n", object_table[i].instances);
      dev_info(&client->dev, "\tReport Id's:\t%d\n", object_table[i].num_report_ids);
      memset(config,0x00,32);
      if(object_table[i].type != 37)
      {
        mxt_read_block(mxt->client,object_table[i].chip_addr, object_table[i].size, (u8 *)config);
        dev_info(&client->dev, "\tconfig: [%x][%x][%x][%x][%x][%x][%x][%x][%x][%x][%x][%x][%x][%x][%x][%x]\n"
                               , config[0], config[1], config[2], config[3], config[4], config[5], config[6], config[7]
                               , config[8], config[9], config[10], config[11], config[12], config[13], config[14], config[15]);
        dev_info(&client->dev, "\t        [%x][%x][%x][%x][%x][%x][%x][%x][%x][%x][%x][%x][%x][%x][%x][%x]\n"
                              , config[16], config[17], config[18], config[19], config[20], config[21], config[22], config[23]
                              , config[24], config[25], config[26], config[27], config[28], config[29], config[30], config[31]);
      }
    }
  }

  return 0;

err_max_rid:
  kfree(mxt->last_message);
err_msg_alloc:
  kfree(mxt->rid_map);
err_rid_map_alloc:
err_object_read:
  kfree(raw_ib_data);
err_ib_alloc:
  kfree(object_table);
err_object_table_alloc:
  return error;
}

/***********************************************************************
 * DEVFS Management
 ***********************************************************************/

static ssize_t show_wq_sampling_interval(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mxt_data *mxt = i2c_get_clientdata(to_i2c_client(dev));

	return sprintf(buf, "wq_samping_interval: %d\n", mxt->wq_samping_interval);
}

static ssize_t set_wq_sampling_interval(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int interval = simple_strtoul(buf, NULL, 0);
	struct mxt_data *mxt = i2c_get_clientdata(to_i2c_client(dev));

	mxt->wq_samping_interval = interval;

  return count;
}

static ssize_t show_irq_sampling_interval(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mxt_data *mxt = i2c_get_clientdata(to_i2c_client(dev));

	return sprintf(buf, "irq_samping_interval: %d\n", mxt->irq_samping_interval);
}

static ssize_t set_irq_sampling_interval(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int interval = simple_strtoul(buf, NULL, 0);
	struct mxt_data *mxt = i2c_get_clientdata(to_i2c_client(dev));

	mxt->irq_samping_interval = interval;

  return count;
}


static DEVICE_ATTR(wq_sampling_rate, S_IWUSR | S_IRUGO, show_wq_sampling_interval, set_wq_sampling_interval);
static DEVICE_ATTR(irq_sampling_rate, S_IWUSR | S_IRUGO, show_irq_sampling_interval, set_irq_sampling_interval);

static struct attribute *mxt_sysfs_entries[] = {
	&dev_attr_wq_sampling_rate.attr,
	&dev_attr_irq_sampling_rate.attr,
	NULL
};

static const struct attribute_group mxt_attr_group = {
	.name	= "maXTouch",			/* put in device directory */
	.attrs = mxt_sysfs_entries,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void mxt_earlysuspend(struct early_suspend *pes)
{
  printk("mxt_earlysuspend\n");
  early_lock = 1;
}

static void mxt_earlyresume(struct early_suspend *pes)
{
  printk("mxt_earlyresume\n");
  early_lock = 0;
}

static struct early_suspend mxt_earlys = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB,
	.suspend = mxt_earlysuspend,
	.resume = mxt_earlyresume,
};
#endif

static int __devinit mxt_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
  struct mxt_data *mxt;
  struct mxt_platform_data *pdata;
  struct input_dev *input;
  u8 *id_data;
  int error;

  mxt_debug(DEBUG_INFO, "mXT224: mxt_probe comms=[%d]\n",comms);
  
  if (client == NULL)
  {
    printk("maXTouch: client == NULL\n");
    return -EINVAL;
  }
  else if(client->adapter == NULL)
  {
    printk("maXTouch: client->adapter == NULL\n");
    return -EINVAL;
  }
  else if(&client->dev == NULL)
  {
    printk("maXTouch: client->dev == NULL\n");
    return -EINVAL;
  }
  else if(&client->adapter->dev == NULL)
  {
    printk("maXTouch: client->adapter->dev == NULL\n");
    return -EINVAL;
  }
  else if(id == NULL)
  {
    printk("maXTouch: id == NULL\n");
    return -EINVAL;
  }

  mxt_debug(DEBUG_INFO, "maXTouch driver\n");
  mxt_debug(DEBUG_INFO, "\t \"%s\"\n", client->name);
  mxt_debug(DEBUG_INFO, "\taddr:\t0x%04x\n", client->addr);
  mxt_debug(DEBUG_INFO, "\tirq:\t%d\n", client->irq);
  mxt_debug(DEBUG_INFO, "\tflags:\t0x%04x\n", client->flags);
  mxt_debug(DEBUG_INFO, "\tadapter:\"%s\"\n", client->adapter->name);
  mxt_debug(DEBUG_INFO, "\tdevice:\t\"%s\"\n", client->dev.init_name);

  /* Check if the I2C bus supports BYTE transfer */
  //error = i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE);
  //printk("RRC:  i2c_check_functionality = %i\n", error);
  //error = 0xff;
/*
  if (!error)
  {
    dev_err(&client->dev, "maXTouch driver\n");
    dev_err(&client->dev, "\t \"%s\"\n", client->name);
    dev_err(&client->dev, "\taddr:\t0x%04x\n", client->addr);
    dev_err(&client->dev, "\tirq:\t%d\n", client->irq);
    dev_err(&client->dev, "\tflags:\t0x%04x\n", client->flags);
    dev_err(&client->dev, "\tadapter:\"%s\"\n", client->adapter->name);
    dev_err(&client->dev, "\tdevice:\t\"%s\"\n", client->dev.init_name);
    dev_err(&client->dev, "%s adapter not supported\n", dev_driver_string(&client->adapter->dev));
    return -ENODEV;
  }
*/
  mxt_debug(DEBUG_TRACE, "maXTouch driver functionality OK\n");

  /* Allocate structure - we need it to identify device */
  mxt = kzalloc(sizeof(struct mxt_data), GFP_KERNEL);
  if(mxt == NULL)
  {
    dev_err(&client->dev, "insufficient memory\n");
    error = -ENOMEM;
    goto err_mxt_alloc;
  }

  id_data = kmalloc(MXT_ID_BLOCK_SIZE, GFP_KERNEL);
  if(id_data == NULL)
  {
    dev_err(&client->dev, "insufficient memory\n");
    error = -ENOMEM;
    goto err_id_alloc;
  }

  /* Initialize Platform data */
  pdata = client->dev.platform_data;
  if(pdata == NULL)
  {
    dev_err(&client->dev, "platform data is required!\n");
    error = -EINVAL;
    goto err_pdata;
  }
  if(debug >= DEBUG_TRACE)
    printk(KERN_INFO "Platform OK: pdata = 0x%08x\n", (unsigned int)pdata);

  mxt->read_fail_counter = 0;
  mxt->message_counter = 0;
  
  mxt->min_x_val = pdata->min_x;
  mxt->min_y_val = pdata->min_y;
  mxt->max_x_val = pdata->max_x;
  mxt->max_y_val = pdata->max_y;

  /* Get data that is defined in board specific code. */
  mxt->init_hw = pdata->init_platform_hw;
  mxt->exit_hw = pdata->exit_platform_hw;
  mxt->read_chg = pdata->read_chg;

  if(pdata->valid_interrupt != NULL)
    mxt->valid_interrupt = pdata->valid_interrupt;
  else
    mxt->valid_interrupt = mxt_valid_interrupt_dummy;

  if(mxt->init_hw != NULL)
    mxt->init_hw();

  if(debug >= DEBUG_TRACE)
    printk(KERN_INFO "maXTouch driver identifying chip\n");
  if(mxt_identify(client, mxt, id_data) < 0)
  {
    dev_err(&client->dev, "Chip could not be identified\n");
    error = -ENODEV;
    goto err_identify;
  }
  
  /* Chip is valid and active. */
  if(debug >= DEBUG_TRACE)
    printk(KERN_INFO "maXTouch driver allocating input device\n");

  input = input_allocate_device();
  if(!input)
  {
    dev_err(&client->dev, "error allocating input device\n");
    error = -ENOMEM;
    goto err_input_dev_alloc;
  }

  mxt->client = client;
  mxt->input = input;

  INIT_DELAYED_WORK(&mxt->dwork, mxt_worker);
	mxt->mxt_single_workqueue = create_singlethread_workqueue("mxt_singlethread_wq");
	if (!(mxt->mxt_single_workqueue))
	{
		printk("create single work queue fail");
		error = -ENOMEM;
		goto err_input_dev_alloc;
	}

  mutex_init(&mxt->debug_mutex);
  mutex_init(&mxt->msg_mutex);
  mxt_debug(DEBUG_TRACE, "maXTouch driver creating device name\n");

  snprintf(mxt->phys_name, sizeof(mxt->phys_name), "%s/input0", dev_name(&client->dev));
  input->name = "Atmel maXTouch Touchscreen controller";
  input->phys = mxt->phys_name;
  input->id.bustype = BUS_I2C;
  input->dev.parent = &client->dev;

  mxt_debug(DEBUG_INFO, "maXTouch name: \"%s\"\n", input->name);
  mxt_debug(DEBUG_INFO, "maXTouch phys: \"%s\"\n", input->phys);
  mxt_debug(DEBUG_INFO, "maXTouch driver setting abs parameters\n");

  set_bit(BTN_TOUCH, input->keybit);
  
  /* Single touch */
  input_set_abs_params(input, ABS_X, mxt->min_x_val, mxt->max_x_val, 0, 0);
  input_set_abs_params(input, ABS_Y, mxt->min_y_val, mxt->max_y_val, 0, 0);
  input_set_abs_params(input, ABS_PRESSURE, 0, MXT_MAX_REPORTED_PRESSURE, 0, 0);
  input_set_abs_params(input, ABS_TOOL_WIDTH, 0, MXT_MAX_REPORTED_WIDTH,  0, 0);

  /* Multitouch */
  input_set_abs_params(input, ABS_MT_POSITION_X, mxt->min_x_val, mxt->max_x_val, 0, 0);
  input_set_abs_params(input, ABS_MT_POSITION_Y, mxt->min_y_val, mxt->max_y_val, 0, 0);
#if 1 // Joss modified	
  input_set_abs_params(input, ABS_MT_TOUCH_MAJOR, 0, MXT_MAX_TOUCH_SIZE, 0, 0);
  input_set_abs_params(input, ABS_MT_TRACKING_ID, 0, 5/*MXT_MAX_NUM_TOUCHES*/, 0, 0);
#else
  input_set_abs_params(input, ABS_MT_TOUCH_MAJOR, 0, MXT_MAX_TOUCH_SIZE, 0, 0);
  //input_set_abs_params(input, ABS_MT_TRACKING_ID, 0, MXT_MAX_NUM_TOUCHES, 0, 0);	
#endif 


  /*virtual key*/
  set_bit(KEY_HOME, input->keybit);
  set_bit(KEY_MENU, input->keybit);
  set_bit(KEY_BACK, input->keybit);
  set_bit(KEY_SEARCH, input->keybit);

  __set_bit(EV_ABS, input->evbit);
  __set_bit(EV_SYN, input->evbit);
  __set_bit(EV_KEY, input->evbit);

  mxt_debug(DEBUG_TRACE, "maXTouch driver setting client data\n");
  i2c_set_clientdata(client, mxt);
  mxt_debug(DEBUG_TRACE, "maXTouch driver setting drv data\n");
  input_set_drvdata(input, mxt);
  mxt_debug(DEBUG_TRACE, "maXTouch driver input register device\n");
  error = input_register_device(mxt->input);
  if(error < 0)
  {
    dev_err(&client->dev, "Failed to register input device\n");
    goto err_register_device;
  }

  error = mxt_read_object_table(client, mxt, id_data);
  if(error < 0)
    goto err_read_ot;

#ifdef MXT224_SYSFS
  /* Create debugfs entries. */
  mxt->debug_dir = debugfs_create_dir("maXTouch", NULL);
  /* debugfs is not enabled. */
  if(mxt->debug_dir == -ENODEV)
  {
    printk(KERN_WARNING "debugfs not enabled in kernel\n");
  }
  else if (mxt->debug_dir == NULL)
  {
    printk(KERN_WARNING "error creating debugfs dir\n");
  }
  else
  {
    mxt_debug(DEBUG_TRACE, "created \"maXTouch\" debugfs dir\n");
    debugfs_create_file("deltas", S_IRUSR, mxt->debug_dir, mxt, &delta_fops);
    debugfs_create_file("refs", S_IRUSR, mxt->debug_dir, mxt, &refs_fops);
  }
  /* Create character device nodes for reading & writing registers */
  mxt->mxt_class = class_create(THIS_MODULE, "maXTouch_memory");
  /* 2 numbers; one for memory and one for messages */

  error = alloc_chrdev_region(&mxt->dev_num, 0, 2, "maXTouch_memory");
  mxt_debug(DEBUG_VERBOSE, "device number %d allocated!\n", MAJOR(mxt->dev_num));
  if(error)
    printk(KERN_WARNING "Error registering device\n");
  cdev_init(&mxt->cdev, &mxt_memory_fops);
  cdev_init(&mxt->cdev_messages, &mxt_message_fops);

  mxt_debug(DEBUG_VERBOSE, "cdev initialized\n");
  mxt->cdev.owner = THIS_MODULE;
  mxt->cdev_messages.owner = THIS_MODULE;

  error = cdev_add(&mxt->cdev, mxt->dev_num, 1);
  if(error)
    printk(KERN_WARNING "Bad cdev\n");

  error = cdev_add(&mxt->cdev_messages, mxt->dev_num + 1, 1);
  if(error)
    printk(KERN_WARNING "Bad cdev\n");

  mxt_debug(DEBUG_VERBOSE, "cdev added\n");

  device_create(mxt->mxt_class, NULL, MKDEV(MAJOR(mxt->dev_num), 0), NULL, "maXTouch");
  device_create(mxt->mxt_class, NULL, MKDEV(MAJOR(mxt->dev_num), 1), NULL, "maXTouch_messages");
#endif /*MXT224_SYSFS*/

  mxt->msg_buffer_startp = 0;
  mxt->msg_buffer_endp = 0;

  /* Allocate the interrupt */
  mxt_debug(DEBUG_TRACE, "maXTouch driver allocating interrupt...\n");
  mxt->irq = client->irq;
  mxt->valid_irq_counter = 0;
  mxt->invalid_irq_counter = 0;
  mxt->irq_counter = 0;
  if (mxt->irq)
  {
    error = request_irq(mxt->irq, mxt_irq_handler, IRQF_TRIGGER_FALLING, client->dev.driver->name, mxt);
    if(error < 0)
    {
      dev_err(&client->dev, "failed to allocate irq %d\n", mxt->irq);
      goto err_irq;
    }
  }

  if (debug > DEBUG_INFO)
    dev_info(&client->dev, "touchscreen, irq %d\n", mxt->irq);

	mxt->wq_samping_interval = 0;
	mxt->irq_samping_interval = 0;

	error = sysfs_create_group(&client->dev.kobj, &mxt_attr_group);
	if (error)
	{
		printk("error creating mxt sysfs group!\n");
		goto err_irq;
	}

	 /* Early suspende registration */
#ifdef CONFIG_HAS_EARLYSUSPEND
  early_lock = 0;
	register_early_suspend(&mxt_earlys);
#endif

  //if(reconfig)
  {
    printk("ATMEL-MXT224 need reconfig...\n");
    /*free*/  
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_GEN_POWERCONFIG_T7, mxt),255); /*IDLEACQINT*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_GEN_POWERCONFIG_T7, mxt)+1,255); /*ACTVACQINT*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_GEN_POWERCONFIG_T7, mxt)+2,50); /*ACTV2IDLETO*/
  
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_GEN_ACQUIRECONFIG_T8, mxt),9); /*CHRGTIME*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_GEN_ACQUIRECONFIG_T8, mxt)+2,5); /*TCHDRIFT*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_GEN_ACQUIRECONFIG_T8, mxt)+3,5);/*DRIFTST*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_GEN_ACQUIRECONFIG_T8, mxt)+4,0);/*TCHAUTOCAL*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_GEN_ACQUIRECONFIG_T8, mxt)+5,0);/*SYNC*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_GEN_ACQUIRECONFIG_T8, mxt)+6,15);/*ATCHCALST*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_GEN_ACQUIRECONFIG_T8, mxt)+7,30);/*ATCHCALSTHR*/
  
    //mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9, mxt),143);/*CTRL*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9, mxt),11);/*CTRL*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9, mxt)+1,0); /*XORIGIN*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9, mxt)+2,0);/*YORIGIN*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9, mxt)+3,18);/*XSIZE*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9, mxt)+4,11);/*YSIZE*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9, mxt)+5,1);/*AKSCFG*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9, mxt)+6,0);/*BLEN*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9, mxt)+7,30);/*TCHTHR*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9, mxt)+8,2);/*TCHDI*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9, mxt)+9,1);/*ORIENT*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9, mxt)+10,0);/*MRGTIMEOUT*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9, mxt)+11,5);/*MOVHYSTI*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9, mxt)+12,10);/*MOVHYSTN*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9, mxt)+13,32);/*MOVFILTER*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9, mxt)+14,5);/*NUMTOUCH*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9, mxt)+15,5);/*MRGHYST*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9, mxt)+16,20);/*MRGTHR*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9, mxt)+17,0);/*AMPHYST*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9, mxt)+22,0);/*XLOCLIP*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9, mxt)+23,0);/*XHICLIP*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9, mxt)+24,0);/*YLOCLIP*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9, mxt)+25,0);/*YHICLIP*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9, mxt)+26,0);/*XEDGECTRL*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9, mxt)+27,0);/*XEDGEDIST*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9, mxt)+28,0);/*YEDGECTRL*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9, mxt)+29,0);/*YEDGEDIST*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9, mxt)+30,10);/*JUMPLIMIT*/
  
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_KEYARRAY_T15, mxt),131); /*CTRL*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_KEYARRAY_T15, mxt)+1,0); /*XORIGIN*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_KEYARRAY_T15, mxt)+2,11);/*YORIGIN*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_KEYARRAY_T15, mxt)+3,4);/*XSIZE*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_KEYARRAY_T15, mxt)+4,1);/*YSIZE*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_KEYARRAY_T15, mxt)+5,1);/*AKSCFG*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_KEYARRAY_T15, mxt)+6,16);/*BLEN*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_KEYARRAY_T15, mxt)+7,30);/*TCHTHR*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_KEYARRAY_T15, mxt)+8,3);/*TCHDI*/
  
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_SPT_COMMSCONFIG_T18, mxt),0); /*CTRL*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_SPT_COMMSCONFIG_T18, mxt)+1,0); /*COMMAND*/
  
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_SPT_GPIOPWM_T19, mxt),0); /*CTRL*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_SPT_GPIOPWM_T19, mxt)+1,0); /*REPORTMASK*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_SPT_GPIOPWM_T19, mxt)+2,0);/*DIR*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_SPT_GPIOPWM_T19, mxt)+3,0);/*INTPULLUP*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_SPT_GPIOPWM_T19, mxt)+4,0);/*OUT*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_SPT_GPIOPWM_T19, mxt)+5,0);/*WAKE*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_SPT_GPIOPWM_T19, mxt)+7,0);/*PWM*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_SPT_GPIOPWM_T19, mxt)+8,0);/*PERIOD*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_SPT_GPIOPWM_T19, mxt)+9,0);/*SHPTHR1*/
  
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_PROCI_GRIPFACESUPPRESSION_T20, mxt),15); /*CTRL*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_PROCI_GRIPFACESUPPRESSION_T20, mxt)+1,0); /*XLOGRIP*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_PROCI_GRIPFACESUPPRESSION_T20, mxt)+2,0);/*XHIGRIP*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_PROCI_GRIPFACESUPPRESSION_T20, mxt)+3,0);/*YLOGRIP*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_PROCI_GRIPFACESUPPRESSION_T20, mxt)+4,0);/*YHIGRIP*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_PROCI_GRIPFACESUPPRESSION_T20, mxt)+5,0);/*MAXTCHS*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_PROCI_GRIPFACESUPPRESSION_T20, mxt)+7,0);/*SZTHR1*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_PROCI_GRIPFACESUPPRESSION_T20, mxt)+8,0);/*SZTHR2*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_PROCI_GRIPFACESUPPRESSION_T20, mxt)+9,0);/*SHPTHR1*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_PROCI_GRIPFACESUPPRESSION_T20, mxt)+10,0);/*SHPTHR2*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_PROCI_GRIPFACESUPPRESSION_T20, mxt)+11,0);/*SUPEXTTO*/
    
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_PROCG_NOISESUPPRESSION_T22, mxt),13);/*CTRL*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_PROCG_NOISESUPPRESSION_T22, mxt)+3,0);/*GCAFUL*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_PROCG_NOISESUPPRESSION_T22, mxt)+4,0);
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_PROCG_NOISESUPPRESSION_T22, mxt)+5,0);/*GCAFLL*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_PROCG_NOISESUPPRESSION_T22, mxt)+6,0);
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_PROCG_NOISESUPPRESSION_T22, mxt)+7,0);/*ACTVGCAFVALID*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_PROCG_NOISESUPPRESSION_T22, mxt)+8,30);/*NOISETHR*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_PROCG_NOISESUPPRESSION_T22, mxt)+10,0);/*FREQHOPSCALE*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_PROCG_NOISESUPPRESSION_T22, mxt)+11,0);/*FREQ 0*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_PROCG_NOISESUPPRESSION_T22, mxt)+12,10);/*FREQ 1*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_PROCG_NOISESUPPRESSION_T22, mxt)+13,15);/*FREQ 2*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_PROCG_NOISESUPPRESSION_T22, mxt)+14,20);/*FREQ 3*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_PROCG_NOISESUPPRESSION_T22, mxt)+15,40);/*FREQ 4*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_PROCG_NOISESUPPRESSION_T22, mxt)+16,0);/*IDLEGCAFVALID*/
  
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_PROXIMITY_T23, mxt),0); /*CTRL*/
    
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_PROCI_ONETOUCHGESTUREPROCESSOR_T24, mxt),0); /*CTRL*/
  
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_SPT_CTECONFIG_T28, mxt),0); /*CTRL*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_SPT_CTECONFIG_T28, mxt)+1,0); /*CMD*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_SPT_CTECONFIG_T28, mxt)+2,2);/*MODE*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_SPT_CTECONFIG_T28, mxt)+3,16);/*IDLEGCAFDEPTH*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_SPT_CTECONFIG_T28, mxt)+4,16);/*ACTVGCAFDEPTH*/
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_SPT_CTECONFIG_T28, mxt)+5,30);/*VOLTAGE*/
  
    
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_GEN_COMMANDPROCESSOR_T6, mxt) + MXT_ADR_T6_BACKUPNV, MXT_CMD_T6_BACKUP);
    msleep(100);
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_GEN_COMMANDPROCESSOR_T6, mxt) + MXT_ADR_T6_RESET, 1);
    msleep(80);
    error = mxt_read_object_table(client, mxt, id_data);
  }

  /* Schedule a worker routine to read any messages that might have
   * been sent before interrupts were enabled. */
  //cancel_delayed_work(&mxt->dwork);
  //schedule_delayed_work(&mxt->dwork, 0);
  mxt_schedule_delayed_work(&mxt->dwork, 0);
  kfree(id_data);

  return 0;

err_irq:
  kfree(mxt->rid_map);
  kfree(mxt->object_table);
  kfree(mxt->last_message);
err_read_ot:
err_register_device:
  input_free_device(input);
err_identify:
err_input_dev_alloc:
  kfree(id_data);
err_pdata:
err_id_alloc:
  if (mxt->exit_hw != NULL)
    mxt->exit_hw();
  kfree(mxt);
err_mxt_alloc:
  return error;
}

static int __devexit mxt_remove(struct i2c_client *client)
{
  struct mxt_data *mxt;

  mxt = i2c_get_clientdata(client);

#ifdef MXT224_SYSFS
  /* Remove debug dir entries */
  debugfs_remove_recursive(mxt->debug_dir);

  unregister_chrdev_region(mxt->dev_num, 2);
  device_destroy(mxt->mxt_class, MKDEV(MAJOR(mxt->dev_num), 0));
  device_destroy(mxt->mxt_class, MKDEV(MAJOR(mxt->dev_num), 1));
  cdev_del(&mxt->cdev);
  cdev_del(&mxt->cdev_messages);
#endif

  if(mxt != NULL)
  {
    if(mxt->exit_hw != NULL)
      mxt->exit_hw();

    if(mxt->irq)
      free_irq(mxt->irq, mxt);

    cancel_delayed_work_sync(&mxt->dwork);
    input_unregister_device(mxt->input);
    class_destroy(mxt->mxt_class);
    debugfs_remove(mxt->debug_dir);

    kfree(mxt->rid_map);
    kfree(mxt->object_table);
    kfree(mxt->last_message);
  }
  kfree(mxt);

  i2c_set_clientdata(client, NULL);
  if(debug >= DEBUG_TRACE)
    dev_info(&client->dev, "Touchscreen unregistered\n");

  return 0;
}

#if defined(CONFIG_PM)
static int mxt_suspend(struct i2c_client *client, pm_message_t mesg)
{
  int error;
  struct mxt_data *mxt = i2c_get_clientdata(client);
printk("mxt_suspend\n");
  error = mxt_read_block(mxt->client,MXT_BASE_ADDR(MXT_GEN_POWERCONFIG_T7, mxt), 2, (u8 *) suspend_config);
  if(error != 0)
  {
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_GEN_POWERCONFIG_T7, mxt), 0);
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_GEN_POWERCONFIG_T7, mxt) + 1, 0);
  }
  disable_irq(mxt->irq);

  return 0;
}

static int mxt_resume(struct i2c_client *client)
{
  struct mxt_data *mxt = i2c_get_clientdata(client);
  printk("mxt_resume\n");
  
  if((suspend_config[0] != 0) || (suspend_config[1] != 0))
  {
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_SPT_USER_INFO_T38, mxt), 0);
    msleep(25);
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_GEN_POWERCONFIG_T7, mxt), suspend_config[0]);
    mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_GEN_POWERCONFIG_T7, mxt) + 1, suspend_config[1]);
  }

  enable_irq(mxt->irq);
  mxt_schedule_delayed_work(&mxt->dwork, 0);
  return 0;
}
#else
#define mxt_suspend NULL
#define mxt_resume NULL
#endif

static const struct i2c_device_id mxt_idtable[] =
{
  {"maXTouch", 0,},
  {}
};

MODULE_DEVICE_TABLE(i2c, mxt_idtable);

static struct i2c_driver mxt_driver =
{
  .driver =
  {
    .name = "maXTouch",
    .owner = THIS_MODULE,
  },
  .id_table = mxt_idtable,
  .probe = mxt_probe,
  .remove = __devexit_p(mxt_remove),
  .suspend = mxt_suspend,
  .resume = mxt_resume,
};

static int __init mxt_init(void)
{
  int err;
  err = i2c_add_driver(&mxt_driver);
  if (err)
    printk("Adding maXTouch driver failed (errno = %d)\n", err);
  else
    mxt_debug(DEBUG_TRACE, "Successfully added driver %s\n", mxt_driver.driver.name);
  return err;
}

static void __exit mxt_cleanup(void)
{
  i2c_del_driver(&mxt_driver);
}

module_init(mxt_init);
module_exit(mxt_cleanup);

MODULE_AUTHOR("Iiro Valkonen");
MODULE_DESCRIPTION("Driver for Atmel maXTouch Touchscreen Controller");
MODULE_LICENSE("GPL");
