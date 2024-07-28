///////////////////////////////////////////////////////////////////////////
//
// i2c-soil-drv.h
//
// Include file for i2c soil moisture driver.
//
// Thomas Ames, July 25, 2024
//

#ifndef I2C_SOIL_DRV_H
#define I2C_SOIL_DRV_H

#define I2C_SOIL_DRV_DEBUG 1

// From scull driver
#undef PDEBUG             /* undef it, just in case */
#ifdef I2C_SOIL_DRV_DEBUG
#  ifdef __KERNEL__
     /* This one if debugging is on, and kernel space */
#    define PDEBUG(fmt, args...) printk( KERN_DEBUG "i2c-soil-drv: " fmt, ## args)
#  else
     /* This one for user space */
#    define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#  endif
#else
#  define PDEBUG(fmt, args...) /* not debugging: nothing */
#endif

// Writing these stings to the driver turn simulation mode on or off.
// Using in-band control instead of ioctl's to simplify testing via
// shell scripting w/ echo/dd/cat
#define SIM_ON_CMD "sim-on"
#define SIM_OFF_CMD "sim-off"
#define MAX_CMD_BUF_SIZE 8

struct i2c_soil_dev
{
    // cdev @ start - single inheritance, p_cdev = p_aesd_dev
    // Don't really need to use container_of
    struct cdev cdev;     // Char device structure
    int use_simulation;	  // 1=simulation (no i2c), 0=i2c mode
    int sim_data;         // When sim on, write updates this, read returns this
};

#endif // I2C_SOIL_DRV_H
