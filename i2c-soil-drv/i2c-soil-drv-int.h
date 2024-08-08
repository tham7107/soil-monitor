/**************************************************************************
 *
 * i2c-soil-drv-int.h
 *
 * Internal include file for i2c soil moisture driver.
 *
 * Thomas Ames, July 25, 2024
 */

#ifndef I2C_SOIL_DRV_INT_H
#define I2C_SOIL_DRV_INT_H

/*#define I2C_SOIL_DRV_DEBUG 1*/

#include "i2c-soil-drv-api.h"

/* From scull driver */
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

/*
 * Adafruit soil moisture sensor parameters.  See:
 *
 * https://github.com/adafruit/Adafruit_CircuitPython_seesaw/blob/main/adafruit_seesaw/seesaw.py
 */
#define I2C_BUS_ADDR		0x36 /* Hardcoded i2c addr */
#define I2C_TOUCH_BASE_ADDR	0x0f
#define I2C_TOUCH_OFFSET	0x10
#define I2C_MSEC_DELAY		10
#define I2C_HIGH_OUT_OF_RANGE	4095
#define I2C_MAX_REREADS		4
#define I2C_READING_OUT_OF_BOUNDS(X) ((X < 0) || (X > I2C_HIGH_OUT_OF_RANGE))

/* reading < I2C_MIN_DRY_READING returns 0, > I2C_MAX_WET_READING returns 255 */
#define I2C_MIN_RAW_DRY_READING	0x2a0
#define I2C_MAX_RAW_WET_READING	0x39f
#define I2C_MIN_DRY_READING	0
#define I2C_MAX_WET_READING	255

struct i2c_soil_dev
{
    /* cdev @ start - single inheritance, p_cdev = p_aesd_dev */
    /* Don't really need to use container_of */
    struct cdev cdev;		/* Char device structure */
    struct i2c_adapter *p_i2c_adapter;
    struct i2c_client *p_i2c_client; /* dummy client */
    int use_simulation;	       /* 1=simulation (no i2c), 0=i2c mode */
    unsigned char sim_data; /* When sim on, write updates this, read returns this */
};

#endif /* I2C_SOIL_DRV_INT_H */
