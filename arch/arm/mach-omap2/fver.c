/**+===========================================================================
  File: fver.c
============================================================================+*/
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>

#define FVER_PROC "fver"
#define fver_BLOCK "/dev/block/platform/omap/omap_hsmmc.1/by-name/systeminfo"

static char *fver_preload;
static int fver_len=12000;
static int fver_open_times=0;

static int fver_show(struct seq_file *s, void *unused)
{
	struct file *fver_filp = NULL;
	mm_segment_t oldfs;
	if(!fver_open_times){
		oldfs=get_fs();
		set_fs(KERNEL_DS);
		fver_filp = filp_open(fver_BLOCK, O_RDONLY, 0);
		if(!IS_ERR(fver_filp)){
			fver_filp->f_op->read(fver_filp,fver_preload,sizeof(char)*fver_len,&fver_filp->f_pos);
			filp_close(fver_filp, NULL);
			fver_open_times++;
		}else {
			printk("[dw]open %s fail\n", fver_BLOCK);
		}
		set_fs(oldfs);
	}else{
		seq_printf(s,"%s\n",fver_preload);   
	}
    	return 0;
}

static int fver_open(struct inode *inode, struct file *file)
{
        return single_open(file, fver_show, &inode->i_private);
}

static const struct file_operations fver_fops = {
        .open        = fver_open,
        .read        = seq_read,
        .llseek      = seq_lseek,
        .release     = single_release,
};
static int __init fver_module_init(void)
{
	struct proc_dir_entry *entry;
    	entry = create_proc_entry(FVER_PROC, S_IFREG | S_IRUGO, NULL);
    	entry->proc_fops = &fver_fops;
    	entry->size = 0xff;    
    	fver_preload=kmalloc(sizeof(char)*fver_len, GFP_KERNEL);
    	return 0;
}

static void __exit fver_module_exit(void)
{
	//return 0;
}

module_exit(fver_module_exit);
module_init(fver_module_init);


