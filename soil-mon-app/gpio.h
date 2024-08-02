/**************************************************************************
 *
 * gpio.h
 *
 * Include file for GPIO code.
 *
 * Thomas Ames, August 2, 2024
 */

#ifndef GPIO_H
#define GPIO_H

#define GPIO_PIN	"17"
#define GPIO_DIRECTORY	"/sys/class/gpio"
#define GPIO_EXPORT	GPIO_DIRECTORY "/export"
#define GPIO_UNEXPORT	GPIO_DIRECTORY "/unexport"
#define GPIO_DIRECTION	GPIO_DIRECTORY "/gpio" GPIO_PIN "/direction"
#define GPIO_INPUT	"in"
#define GPIO_OUTPUT	"out"
#define GPIO_VALUE	GPIO_DIRECTORY "/gpio" GPIO_PIN "/value"
#define GPIO_ON		"1"
#define GPIO_OFF	"0"
#define GPIO_ERROR	-1
#define GPIO_OK		0

/*
 * Write the GPIO export file in sysfs to enable our pin and set
 * it as an output.
 *
 * Returns GPIO_OK on success, GPIO_ERROR on faliure.
 *
 * Note, repeat calls will fail - write will fail on second and
 * subsequent calls (ie, can't export a currently-exported pin).
 */
int gpio_enable(void);

/*
 * Set the pin as an input (to shut off drive) and Write the GPIO
 * unexport file in sysfs to disable our pin.
 *
 * Returns GPIO_OK on success, GPIO_ERROR on faliure.
 *
 * Note, repeat calls will fail - write will fail on second and
 * subsequent calls (ie, can't unexport a not-exported pin).
 */
int gpio_disable(void);

/*
 * Turn on the already-exported pin.
 *
 * Returns GPIO_OK on success, GPIO_ERROR on faliure.
 *
 * Will fail if pin is not exported already.
 */
int gpio_on(void);

/*
 * Turn off the already-exported pin.
 *
 * Returns GPIO_OK on success, GPIO_ERROR on faliure.
 *
 * Will fail if pin is not exported already.
 */
int gpio_off(void);

#endif /* GPIO_H */
