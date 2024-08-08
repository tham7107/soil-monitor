/**************************************************************************
 *
 * i2c-soil-drv.h
 *
 * Include file for i2c soil moisture driver.
 *
 * Thomas Ames, July 25, 2024
 */

#ifndef I2C_SOIL_DRV_API_H
#define I2C_SOIL_DRV_API_H

/*
 * Writing these stings to the driver turn simulation mode on or off.
 * Using in-band control instead of ioctl's to simplify testing via
 * shell scripting w/ echo/dd/cat
 */
#define SIM_ON_CMD	"sim-on"
#define SIM_OFF_CMD	"sim-off"
#define MAX_CMD_BUF_SIZE 8

/* On RPi, 1 is /dev/i2c-1, bus on gpio2/3 */
#define I2C_BUS_NUM	1

#define I2C_SOIL_DEV	"/dev/i2c-soil-drv"

#endif /* I2C_SOIL_DRV_API_H */
