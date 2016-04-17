/*
 * Functions for FIH SWID informations.
 *
 * Copyright (C) 2010 Foxconn.
 *
 * Author:  AlanHZChang <alanhzchang@fih-foxconn.com>
 *	    Jan. 2012
 */

#include <linux/module.h>
#include <linux/param.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>

#ifdef CONFIG_FIH_PROJECT_GOX
#define FIH_DEV_MODEL_NAME "GOX"
#endif

static struct proc_dir_entry *proc_entry;

int devinfo_read( char *page, char **start, off_t off,
                   int count, int *eof, void *data )
{
    int len;
    if (off > 0) {
      *eof = 1;
      return 0;
    }
#ifdef FIH_DEV_MODEL_NAME
    len = sprintf(page, "%s\n", FIH_DEV_MODEL_NAME);
#else
    len = sprintf(page, "%s\n", "unknown");
#endif

    return len;
}

static int __devinit fih_swid_probe(struct platform_device *pdev)
{
	int ret = 0;

	proc_entry = create_proc_entry("devmodel", 0444, NULL );
    if (proc_entry == NULL) {
      ret = -ENOMEM;
      pr_err("devmodel: Couldn't create proc entry\n");
    } else {
      proc_entry->read_proc = devinfo_read;
      proc_entry->write_proc = 0;
    }

	return 0;
}

static int __devexit fih_swid_remove(struct platform_device *pdev)
{
	remove_proc_entry("devmodel", NULL);
	return 0;
}

static struct platform_driver fih_swid_driver = {
	.probe		= fih_swid_probe,
	.remove		= __devexit_p(fih_swid_remove),
	.driver		= {
		.name	= "fih_swid",
		.owner	= THIS_MODULE,
	},
};

static int __init fih_swid_init(void)
{
	return platform_driver_register(&fih_swid_driver);
}
module_init(fih_swid_init);

static void __exit fih_swid_exit(void)
{
	platform_driver_unregister(&fih_swid_driver);
}
module_exit(fih_swid_exit);

MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:fih-swid");
MODULE_AUTHOR("AlanHZChang <alanhzchang@fih-foxconn.com>");
MODULE_DESCRIPTION("FIH SWID driver");

