///////////////////////////////////////////////////////////////////////////
//
// main.c
//
// i2c soil moisture driver
//
// Thomas Ames, July 25, 2024
//

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cdev.h>

#include "i2c-soil-drv.h"

MODULE_AUTHOR("Thomas Ames");
MODULE_LICENSE("Dual BSD/GPL");

// Scull assigns major to a constant, allowing static or dynamic major
// number assignment. We just use dynamic
dev_t i2c_soil_dev_major = 0;
dev_t i2c_soil_dev_minor = 0;

#define NUM_MINORS 1

struct i2c_soil_dev i2c_soil_device;

int i2c_soil_drv_open(struct inode *inode, struct file *filp)
{
    struct i2c_soil_dev *pdev;

    PDEBUG("open");
    /**
     * TODO: handle open
     */

    // Use container_of macro to get pointer to the i2c_soil_dev and
    // store in filp->private_data. If i2c_soil_dev has no fields
    // other than the cdev, then this macro isn't explicitly
    // necessary, as p_cdev == p_i2c_soil_dev.
    filp->private_data = container_of(inode->i_cdev, struct i2c_soil_dev, cdev);
    PDEBUG("filp->private_data = %p, inode->i_cdev = %p, &i2c_soil_device = %p",
	   filp->private_data, inode->i_cdev, &i2c_soil_device);
    return 0;
}

int i2c_soil_drv_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    // Nothing to do - release is the opposite of open, but we don't
    // do anything in open that needs to be undone.
    return 0;
}
ssize_t i2c_soil_drv_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    // Probably safe to assume the kernel doesn't pass a null filp
    struct i2c_soil_dev *p_i2c_soil_dev = (struct i2c_soil_dev *) filp->private_data;
    ssize_t retval = 0;

    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    PDEBUG("read: user buf = %p, retval = %ld", buf,
	   retval);

    return retval;
}

ssize_t i2c_soil_drv_write(struct file *filp, const char __user *buf,
			   size_t count, loff_t *f_pos)
{
    // Probably safe to assume the kernel doesn't pass a null filp
    struct i2c_soil_dev *p_i2c_soil_dev = (struct i2c_soil_dev *) filp->private_data;
    ssize_t retval = 0;

    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    PDEBUG("write: user buf = %p, retval = %ld", buf,
	   retval);

    return retval;
}

struct file_operations i2c_soil_drv_fops = {
    .owner          = THIS_MODULE,
//    .llseek         = i2c_soil_drv_llseek,
    .read           = i2c_soil_drv_read,
    .write          = i2c_soil_drv_write, // XXX - should be null for non-debug?
//    .unlocked_ioctl = i2c_soil_drv_unlocked_ioctl,
    .open           = i2c_soil_drv_open,
    .release        = i2c_soil_drv_release,
};

static int i2c_soil_drv_init(void)
{
    dev_t devnum = 0;
    int retval;

    PDEBUG("i2c_soil_drv_init\n");

    // Devnum is output-only, per LDD chpt 3
    // Don't put call in if; want to save major num before test for cleanup
    retval = alloc_chrdev_region(&devnum, i2c_soil_dev_minor, NUM_MINORS,
				 "i2c-soil-drv");
    i2c_soil_dev_major = MAJOR(devnum);
    if (retval < 0 )
    {
	printk(KERN_WARNING "i2c-soil-drv: can't get major %d\n", i2c_soil_dev_major);
	return retval;
    }

    cdev_init(&i2c_soil_device.cdev, &i2c_soil_drv_fops);
    i2c_soil_device.cdev.owner = THIS_MODULE;
     // Why doesn't cdev_init set cedv.ops?
    i2c_soil_device.cdev.ops   = &i2c_soil_drv_fops;

    if ((retval = cdev_add(&i2c_soil_device.cdev, devnum, NUM_MINORS)) < 0 )
    {
	printk(KERN_WARNING "i2c-soil-drv: cdev_add failed\n");
	unregister_chrdev_region(devnum, NUM_MINORS);
	return retval;
    }

    // Driver is "live" after successful cdev_add call

    PDEBUG("i2c_soil_drv_init, major=%d, minor=%d, &i2c_soil_device=%p\n",
	   MAJOR(devnum), MINOR(devnum), &i2c_soil_device);
    return 0;
}

static void i2c_soil_drv_cleanup(void)
{
    dev_t devnum = MKDEV(i2c_soil_dev_major, i2c_soil_dev_minor);

    PDEBUG("i2c_soil_drv_cleanup\n");

    // Order is reverse of i2c_soil_drv_init
    cdev_del(&i2c_soil_device.cdev);
    unregister_chrdev_region(devnum, NUM_MINORS);
}

module_init(i2c_soil_drv_init)
module_exit(i2c_soil_drv_cleanup)
