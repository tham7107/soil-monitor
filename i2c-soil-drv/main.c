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
    PDEBUG("open");

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

    // Nothing to do - release is the opposite of open, but we don't
    // do anything in open that needs to be undone.
    return 0;
}

// Returns negative on error, >=0 indicated # of bytes read.
ssize_t i2c_soil_drv_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    // Probably safe to assume the kernel doesn't pass a null filp
    struct i2c_soil_dev *p_i2c_soil_dev = (struct i2c_soil_dev *) filp->private_data;
    ssize_t retval = 0;

    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);

    // Soil moisture level is 0-100 (1 byte). Only read 1 byte. If
    // user tries to read multiple bytes, that will result in multiple
    // calls to read. But reading >1 is really a user mistake, so
    // there is no need to try to optimize for it.
    count = 1;

    if (p_i2c_soil_dev->use_simulation)
    {
	// If simulation is on, return saved byte in dev struct
	// copy_to_user returns number NOT copied, 0 on success.
	if (copy_to_user(buf, &(p_i2c_soil_dev->sim_data), count))
	{
	    retval = -EFAULT;
	}
	else
	{
	    PDEBUG("1 byte read=0x%02x, sim mode on", p_i2c_soil_dev->sim_data);
	    retval = count;
	}
    }
    else
    {
	// Do I2C read here
	//PDEBUG("1 byte read=0x%02x, sim mode off", p_i2c_soil_dev->sim_data);
	retval = count;
    }

    PDEBUG("read: user buf = %p, retval = %ld", buf, retval);
    return retval;
}

// Returns negative on error, >=0 indicated # of bytes read.
ssize_t i2c_soil_drv_write(struct file *filp, const char __user *buf,
			   size_t count, loff_t *f_pos)
{
    // Probably safe to assume the kernel doesn't pass a null filp
    struct i2c_soil_dev *p_i2c_soil_dev = (struct i2c_soil_dev *) filp->private_data;
    ssize_t retval = 0;
    char cmd_buf[MAX_CMD_BUF_SIZE];

    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);

    // User has written:
    //  - Single byte of simulated data
    //  - SIM_ON_CMD (ie, "simon" without quotes)
    //  - SIM_OFF_CMD (ie, "simoff" without quotes)
    if (1 == count)
    {
	if (p_i2c_soil_dev->use_simulation)
	{
	    // Soil moisture level is 0-100 (1 byte). Only read 1 byte. If
	    // user tries to read multiple bytes, 
	    // copy_from_user returns number NOT copied, 0 on success.
	    if (copy_from_user(&(p_i2c_soil_dev->sim_data), buf, count))
	    {
		retval = -EFAULT;
	    }
	    else
	    {
		retval = count;
	    }
	    PDEBUG("1 byte write=0x%02x, sim mode on", p_i2c_soil_dev->sim_data);
	}
	else
	{
	    // Do nothing - ignore single byte writes if simulation is off
	    PDEBUG("1 byte write ignored, sim mode off");
	    // Return count to prevent subsequent calls (all data consumed)
	    retval = count;
	}
    }
    else // For multi-byte write, check for command
    {
	// copy_from_user returns number NOT copied, 0 on success.
	// min() to avoid buffer overrun on stack
	if (copy_from_user(cmd_buf, buf, min((size_t)MAX_CMD_BUF_SIZE, count)))
	{
	    retval = -EFAULT;
	}
	else
	{
	    if (!strncmp(cmd_buf,SIM_ON_CMD,strlen(SIM_ON_CMD)))
	    {
		p_i2c_soil_dev->use_simulation = 1;
		PDEBUG("sim mode enabled");
	    }
	    else if (!strncmp(cmd_buf,SIM_OFF_CMD,strlen(SIM_OFF_CMD)))
	    {
		p_i2c_soil_dev->use_simulation = 0;
		PDEBUG("sim mode disabled");
	    }
	    else
	    {
		// Write data is unknown, ignore
		cmd_buf[MAX_CMD_BUF_SIZE-1] = 0; // Force null term
		PDEBUG("Unexpected multi-byte write, data=%s",cmd_buf);
	    }
	    // Return count to prevent subsequent calls (all data consumed)
	    retval = count;
	}
    }

    PDEBUG("write: user buf = %p, retval = %ld", buf, retval);
    return retval;
}

struct file_operations i2c_soil_drv_fops = {
    .owner          = THIS_MODULE,
    .read           = i2c_soil_drv_read,
    .write          = i2c_soil_drv_write,
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

    // Zero out soil dev - this will default simulation mode to off.
    memset(&i2c_soil_device, 0, sizeof(struct i2c_soil_dev));

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
