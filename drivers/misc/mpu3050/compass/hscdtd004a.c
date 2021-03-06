/*
 $License:
    Copyright (C) 2011 InvenSense Corporation, All Rights Reserved.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
  $
 */

/**
 *  @addtogroup COMPASSDL
 *
 *  @{
 *      @file   hscdtd004a.c
 *      @brief  Magnetometer setup and handling methods for Alps HSCDTD004A
 *              compass.
 */

/* ------------------ */
/* - Include Files. - */
/* ------------------ */

#ifdef __KERNEL__
#include <linux/module.h>
#endif

#include "mpu.h"
#include "mlsl.h"
#include "mlos.h"

#include <log.h>
#undef MPL_LOG_TAG
#define MPL_LOG_TAG "MPL-compass"

/*----- ALPS HSCDTD004A Registers ------*/
#define COMPASS_HSCDTD004A_STAT          (0x18)
#define COMPASS_HSCDTD004A_CTRL1         (0x1B)
#define COMPASS_HSCDTD004A_CTRL2         (0x1C)
#define COMPASS_HSCDTD004A_CTRL3         (0x1D)
#define COMPASS_HSCDTD004A_DATAX         (0x10)

/* --------------------- */
/* -    Variables.     - */
/* --------------------- */

/*****************************************
    Compass Initialization Functions
*****************************************/

int hscdtd004a_suspend(void *mlsl_handle,
		       struct ext_slave_descr *slave,
		       struct ext_slave_platform_data *pdata)
{
	int result = INV_SUCCESS;

	/* Power mode: stand-by */
	result =
	    inv_serial_single_write(mlsl_handle, pdata->address,
				    COMPASS_HSCDTD004A_CTRL1, 0x00);
	ERROR_CHECK(result);
	inv_sleep(1);		/* turn-off time */

	return result;
}

int hscdtd004a_resume(void *mlsl_handle,
		      struct ext_slave_descr *slave,
		      struct ext_slave_platform_data *pdata)
{
	int result = INV_SUCCESS;

	/* Soft reset */
	result =
	    inv_serial_single_write(mlsl_handle, pdata->address,
				    COMPASS_HSCDTD004A_CTRL3, 0x80);
	ERROR_CHECK(result);
	/* Normal state; Power mode: active */
	result =
	    inv_serial_single_write(mlsl_handle, pdata->address,
				    COMPASS_HSCDTD004A_CTRL1, 0x82);
	ERROR_CHECK(result);
	/* Data ready enable */
	result =
	    inv_serial_single_write(mlsl_handle, pdata->address,
				    COMPASS_HSCDTD004A_CTRL2, 0x7C);
	ERROR_CHECK(result);
	inv_sleep(1);		/* turn-on time */
	return result;
}

int hscdtd004a_read(void *mlsl_handle,
		    struct ext_slave_descr *slave,
		    struct ext_slave_platform_data *pdata,
		    unsigned char *data)
{
	unsigned char stat;
	inv_error_t result = INV_SUCCESS;
	int status = INV_SUCCESS;

	/* Read status reg. to check if data is ready */
	result =
	    inv_serial_read(mlsl_handle, pdata->address,
			    COMPASS_HSCDTD004A_STAT, 1, &stat);
	ERROR_CHECK(result);
	if (stat & 0x48) {
		result =
		    inv_serial_read(mlsl_handle, pdata->address,
				    COMPASS_HSCDTD004A_DATAX, 6,
				    (unsigned char *)data);
		ERROR_CHECK(result);
		status = INV_SUCCESS;
	} else if (stat & 0x68) {
		status = INV_ERROR_COMPASS_DATA_OVERFLOW;
	} else {
		status = INV_ERROR_COMPASS_DATA_NOT_READY;
	}
	/* trigger next measurement read */
	result =
	    inv_serial_single_write(mlsl_handle, pdata->address,
				    COMPASS_HSCDTD004A_CTRL3, 0x40);
	ERROR_CHECK(result);
	return status;

}

struct ext_slave_descr hscdtd004a_descr = {
	/*.init             = */ NULL,
	/*.exit             = */ NULL,
	/*.suspend          = */ hscdtd004a_suspend,
	/*.resume           = */ hscdtd004a_resume,
	/*.read             = */ hscdtd004a_read,
	/*.config           = */ NULL,
	/*.get_config       = */ NULL,
	/*.name             = */ "hscdtd004a",
	/*.type             = */ EXT_SLAVE_TYPE_COMPASS,
	/*.id               = */ COMPASS_ID_HSCDTD004A,
	/*.reg              = */ 0x10,
	/*.len              = */ 6,
	/*.endian           = */ EXT_SLAVE_LITTLE_ENDIAN,
	/*.range            = */ {9830, 4000},
	/*.trigger          = */ NULL,
};

struct ext_slave_descr *hscdtd004a_get_slave_descr(void)
{
	return &hscdtd004a_descr;
}
EXPORT_SYMBOL(hscdtd004a_get_slave_descr);

/**
 *  @}
 */
