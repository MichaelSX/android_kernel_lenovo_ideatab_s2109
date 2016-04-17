#ifndef __CM3202_H__
#define __CM3202_H__

#include <linux/types.h>

#define CM3202_NAME           "CM3202"

/* IOCTL */
#define ACTIVE_LS     10
#define READ_LS       5


struct cm3202_als {
    char                        *name;
    struct mutex                update_lock;
    struct delayed_work         d_work;
    int                         lux;
    int                         old_lux;//wiya 20120117 GOXI.B-1400 add
    int                         enable;
    int                         ambient_interval;
    struct input_dev            *als_input_dev;
};

struct cm3202_platform_data {
    struct cm3202_als  *als;
};

#endif
