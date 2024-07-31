/**************************************************************************
 *
 * main.c
 *
 * i2c soil moisture driver
 *
 * Thomas Ames, July 25, 2024
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/i2c.h>
#include <linux/delay.h>

#include "i2c-soil-drv.h"

MODULE_AUTHOR("Thomas Ames");
MODULE_LICENSE("Dual BSD/GPL");

/*
 * Scull assigns major to a constant, allowing static or dynamic major
 * number assignment. We just use dynamic
 */
dev_t i2c_soil_dev_major = 0;
dev_t i2c_soil_dev_minor = 0;

#define NUM_MINORS 1

struct i2c_soil_dev i2c_soil_device;

int i2c_soil_drv_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");

    /*
     * Use container_of macro to get pointer to the i2c_soil_dev and
     * store in filp->private_data. If i2c_soil_dev has no fields
     * other than the cdev, then this macro isn't explicitly
     * necessary, as p_cdev == p_i2c_soil_dev.
     */
    filp->private_data = container_of(inode->i_cdev, struct i2c_soil_dev, cdev);
    PDEBUG("filp->private_data = %p, inode->i_cdev = %p, &i2c_soil_device = %p",
	   filp->private_data, inode->i_cdev, &i2c_soil_device);
    return 0;
}

int i2c_soil_drv_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");

    /*
     * Nothing to do - release is the opposite of open, but we don't
     * do anything in open that needs to be undone.
     */
    return 0;
}

/*
 * Does a single read of the moisture sensor at I2C address 0x36.
 * Returns a 2-byte sensor, if >=0, or -ERRNO if <0.
 *
 * See register definitions TOUCH_BASE=0x0f (line 60) and
 * TOUCH_CHANNEL_OFFSET=0x10 (line 105) and functions moisture_read()
 * (line 298), read() (line 499), and write() (line 510) here:
 *
 * https://github.com/adafruit/Adafruit_CircuitPython_seesaw/blob/main/adafruit_seesaw/seesaw.py
 *
 * General algorithm is write base/offset address pair (0x0f/0x10),
 * then read 2 byte register value, with a short delay (5 mSec) after
 * writes and before reads.
 *
 * Reads can be emulated with i2ctransfer via:
 *
 *   i2ctransfer -y 1 w2@0x36 0x0f 0x10 r2@0x36
 *   i2ctransfer -y 1 w2@0x36 0x0f 0x10 ; i2ctransfer -y 1 r2@0x36
 *   i2ctransfer -y 1 w2@0x36 0x0f 0x10 ; sleep 1 ; i2ctransfer -y 1 r2@0x36
 *   i2ctransfer -y 1 w2@0x36 0x0f 0x10 ; sleep 5 ; i2ctransfer -y 1 r2@0x36
 *
 * Writes and reads must be done as a single, 2 byte write or read. 2
 * 1-byte writes/reads will not work.
 *
 * Example return values (from empirical testing):
 *   0x141 - Suspended in free air
 *   0x24c - in water
 *   0x280 - in dry soil
 *   0x3c0 - (max) in saturated soil
 *   0x3f8 - held between fingers
 */
ssize_t i2c_soil_drv_single_read_sensor(struct i2c_client *p_i2c_client)
{
    ssize_t retval = 0;
    char i2c_buf[2];		/* 2 byte buffer for reg addr and read data */

    /* Load address info for reg */
    i2c_buf[0] = I2C_TOUCH_BASE_ADDR;
    i2c_buf[1] = I2C_TOUCH_OFFSET;

    /* Write 2 byte register address pair */
    retval = i2c_master_send(p_i2c_client, i2c_buf, sizeof(i2c_buf));
    PDEBUG("In i2c_soil_drv_read_sensor, i2c_master_send returned %ld", retval);
    if (retval < 0) {
	printk(KERN_WARNING "i2c-soil-drv: i2c_master_send FAILED, retval=%ld\n", retval);
	return retval;
    } else if (sizeof(i2c_buf) != retval) {
	printk(KERN_WARNING "i2c-soil-drv: i2c_master_send partial send, retval=%ld\n", retval);
	return -EIO;		/* What to return? -EIO, -EAGAIN, -EBUSY? */
    }

    /*
     * After sending the register address info, need a short delay for the
     * part to respond with data. Adafruit code uses a 5ms delay.
     */
    msleep(I2C_MSEC_DELAY);

    /* Read 2 byte register pair */
    retval = i2c_master_recv(p_i2c_client, i2c_buf, sizeof(i2c_buf));
    PDEBUG("In i2c_soil_drv_read_sensor, i2c_master_recv returned %ld", retval);
    if (retval < 0) {
	printk(KERN_WARNING "i2c-soil-drv: i2c_master_recv FAILED, retval=%ld\n", retval);
	return retval;
    } else if (sizeof(i2c_buf) != retval) {
	printk(KERN_WARNING "i2c-soil-drv: i2c_master_recv partial send, retval=%ld\n", retval);
	return -EIO;		/* What to return? -EIO, -EAGAIN, -EBUSY? */
    }

    /* Merge bytes into a single 16-bit value and return */
    retval = ((i2c_buf[0] << 8) | i2c_buf[1]);
    PDEBUG("Raw sensor data: 0x%04lx", retval);
    return retval;
}

/*
 * Read the moisture sensor, but throw away bogus values and try
 * again if necessary. See moisture_read in:
 * https://github.com/adafruit/Adafruit_CircuitPython_seesaw/blob/main/adafruit_seesaw/seesaw.py, which throws out values > 4095 and tries at
 * most 3 re-reads.
 *
 * After a successful, in-range read, return the reading normalized to
 * a one-byte value, 0 = dry, 0xff = wet
 *
 * Returns normalized sensor reading or -ERRNO on error.
 */
ssize_t i2c_soil_drv_read_sensor(struct i2c_client *p_i2c_client)
{
    ssize_t reading;

    /*
     * Including initial assignment in for init clause caused
     * (erroneous) uninitialized variable warnings?
     */
    reading = i2c_soil_drv_single_read_sensor(p_i2c_client);

    for (int i=0;
	 (I2C_READING_OUT_OF_BOUNDS(reading) && (i < I2C_MAX_REREADS));
	 i++) {
	/* Sample code has a short delay before re-read */
	msleep(I2C_MSEC_DELAY);
	reading = i2c_soil_drv_single_read_sensor(p_i2c_client);
    }

    /* What to return? -EIO, -EAGAIN, -EBUSY? */
    if (I2C_READING_OUT_OF_BOUNDS(reading))	return -EIO;
    else if (reading < I2C_MIN_RAW_DRY_READING)	return I2C_MIN_DRY_READING;
    else if (reading > I2C_MAX_RAW_WET_READING)	return I2C_MAX_WET_READING;
    else return (reading - I2C_MIN_RAW_DRY_READING);
}

/* Returns negative on error, >=0 indicated # of bytes read. */
ssize_t i2c_soil_drv_read(struct file *filp, char __user *buf, size_t count,
			  loff_t *f_pos)
{
    /* Probably safe to assume the kernel doesn't pass a null filp */
    struct i2c_soil_dev *p_i2c_soil_dev = (struct i2c_soil_dev *) filp->private_data;
    char moisture = 0;
    ssize_t retval = 0;

    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);

    /*
     * Soil moisture level is 0-255 (1 unsigned byte). Only read 1
     * byte. If user tries to read multiple bytes, that will result in
     * multiple calls to read. But reading >1 is really a user
     * mistake, so there is no need to try to optimize for it.
     */
    count = 1;

    /* If simulation is on, return saved byte in dev struct */
    if (p_i2c_soil_dev->use_simulation) {
	/* Return previously write simulated data */
	moisture = p_i2c_soil_dev->sim_data;
    } else {
	/* Do I2C read here */
	retval = i2c_soil_drv_read_sensor(p_i2c_soil_dev->p_i2c_client);
	if (retval < 0) {
	    printk(KERN_WARNING "i2c-soil-drv: i2c_soil_drv_read_sensor FAILED, retval=%ld\n", retval);
	    return retval;	/* Sensor read failed, bail out  */
	} else {
	    moisture = retval;	/* retval has valid read if >= 0 */
	}
    }

    /* moisture holds the value to return (simulated or real) */
    /* copy_to_user returns number NOT copied, 0 on success. */
    if (copy_to_user(buf, &moisture, count)) {
	retval = -EFAULT;
    } else {
	retval = count;
    }

    PDEBUG("1 byte read=0x%02x, sim mode %s", moisture,
	   (p_i2c_soil_dev->use_simulation ? "on" : "off"));
    PDEBUG("read: user buf = %p, retval = %ld", buf, retval);
    return retval;
}

/* Returns negative on error, >=0 indicated # of bytes read. */
ssize_t i2c_soil_drv_write(struct file *filp, const char __user *buf,
			   size_t count, loff_t *f_pos)
{
    /* Probably safe to assume the kernel doesn't pass a null filp */
    struct i2c_soil_dev *p_i2c_soil_dev = (struct i2c_soil_dev *) filp->private_data;
    ssize_t retval = count;
    char cmd_buf[MAX_CMD_BUF_SIZE];

    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);

    /*
     * 4 possible cases, user has written:
     *  1. Single byte of simulated data
     *  2. SIM_ON_CMD (ie, "sim-on" without quotes)
     *  3. SIM_OFF_CMD (ie, "sim-off" without quotes)
     *  4. Multi-byte write of other data (ignored)
     */
    if (1 == count) {		/* Case 1 */
	if (p_i2c_soil_dev->use_simulation) {
	    /*
	     * Soil moisture level is 0-255 (1 unsigned byte). Only
	     * read 1 byte. If user tries to read multiple bytes,
	     * copy_from_user returns number NOT copied, 0 on success.
	     */
	    if (copy_from_user(&(p_i2c_soil_dev->sim_data), buf, count)) {
		retval = -EFAULT;
	    }
	    PDEBUG("1 byte write=0x%02x, sim mode on", p_i2c_soil_dev->sim_data);
	} else {
	    /* Do nothing - ignore single byte writes if simulation is off */
	    PDEBUG("1 byte write ignored, sim mode off");
	}
    } else {		 /* Case 2, 3, or 4 */
	/* copy_from_user returns number NOT copied, 0 on success. */
	/* min() to avoid buffer overrun on stack */
	if (copy_from_user(cmd_buf, buf,
			   min((size_t)MAX_CMD_BUF_SIZE, count))) {
	    retval = -EFAULT;
	} else {
	    /* Case 2 */
	    if (!strncmp(cmd_buf,SIM_ON_CMD,strlen(SIM_ON_CMD))) {
		p_i2c_soil_dev->use_simulation = 1;
		PDEBUG("sim mode enabled");
	    } else if (!strncmp(cmd_buf,SIM_OFF_CMD,strlen(SIM_OFF_CMD))) {
		/* Case 3 */
		p_i2c_soil_dev->use_simulation = 0;
		PDEBUG("sim mode disabled");
	    } else {
		/* Case 4 - write data is unknown, ignore */
		cmd_buf[MAX_CMD_BUF_SIZE-1] = 0; /* Force null term */
		PDEBUG("Unexpected multi-byte write, data=%s",cmd_buf);
	    }
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

    /* Devnum is output-only, per LDD chpt 3 */
    /* Don't put call in if; want to save major num before test for cleanup */
    retval = alloc_chrdev_region(&devnum, i2c_soil_dev_minor, NUM_MINORS,
				 "i2c-soil-drv");
    i2c_soil_dev_major = MAJOR(devnum);
    if (retval < 0 ) {
	printk(KERN_WARNING "i2c-soil-drv: can't get major %d\n", i2c_soil_dev_major);
	goto alloc_chrdev_region_failed;
    }

    /* Zero out soil dev - this will default simulation mode to off. */
    memset(&i2c_soil_device, 0, sizeof(struct i2c_soil_dev));

    cdev_init(&i2c_soil_device.cdev, &i2c_soil_drv_fops);
    i2c_soil_device.cdev.owner = THIS_MODULE;
    /* Why doesn't cdev_init set cedv.ops? */
    i2c_soil_device.cdev.ops   = &i2c_soil_drv_fops;

    i2c_soil_device.p_i2c_adapter = i2c_get_adapter(I2C_BUS_NUM);
    /* Looking at i2c-core-base.c, returns NULL on error */
    if (!(i2c_soil_device.p_i2c_adapter)) {
	printk(KERN_WARNING "i2c-soil-drv: i2c_get_adapter failed\n");
	retval = -ENOMEM;	/* Guess... */
	goto i2c_get_adapter_failed;
    }

    i2c_soil_device.p_i2c_client =
	i2c_new_dummy_device(i2c_soil_device.p_i2c_adapter, I2C_BUS_ADDR);
    /* see LDD3, pg 295 - ERR_PTR/IS_ERR/PTR_ERR */
    if (IS_ERR(i2c_soil_device.p_i2c_client)) {
	printk(KERN_WARNING "i2c-soil-drv: i2c_new_dummy_device failed\n");
	retval = PTR_ERR(i2c_soil_device.p_i2c_client);
	goto i2c_new_dummy_failed;
    }

    if ((retval = cdev_add(&i2c_soil_device.cdev, devnum, NUM_MINORS)) < 0 ) {
	printk(KERN_WARNING "i2c-soil-drv: cdev_add failed\n");
	goto cdev_add_failed;
    }

    /* Driver is "live" after successful cdev_add call */

    PDEBUG("i2c_soil_drv_init, major=%d, minor=%d, &i2c_soil_device=%p, p_i2c_adapter=%p\n",
	   MAJOR(devnum), MINOR(devnum), &i2c_soil_device,
	   i2c_soil_device.p_i2c_adapter);
    return 0;

cdev_add_failed:
    i2c_unregister_device(i2c_soil_device.p_i2c_client);
i2c_new_dummy_failed:
    /* Is there an adapter release (opposite of i2c_get_adapter)? */
i2c_get_adapter_failed:
    unregister_chrdev_region(devnum, NUM_MINORS);
alloc_chrdev_region_failed:
    return retval;
}

static void i2c_soil_drv_cleanup(void)
{
    dev_t devnum = MKDEV(i2c_soil_dev_major, i2c_soil_dev_minor);

    PDEBUG("i2c_soil_drv_cleanup\n");

    /* Order is reverse of i2c_soil_drv_init */
    cdev_del(&i2c_soil_device.cdev);
    i2c_unregister_device(i2c_soil_device.p_i2c_client);
    /* Is there an adapter release (opposite of i2c_get_adapter)? */
    unregister_chrdev_region(devnum, NUM_MINORS);
}

module_init(i2c_soil_drv_init)
module_exit(i2c_soil_drv_cleanup)
