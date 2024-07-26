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


struct i2c_soil_dev
{
    struct cdev cdev;     /* Char device structure      */
};

#endif // I2C_SOIL_DRV_H
