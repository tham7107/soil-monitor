/**************************************************************************
 *
 * gpio.c
 *
 * Routines to control pump via gpio
 *
 * Thomas Ames, August 2, 2024
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include "gpio.h"

/*
 * Write the GPIO export file in sysfs to enable our pin and set
 * it as an output.
 *
 * Returns GPIO_OK on success, GPIO_ERROR on faliure.
 *
 * Note, repeat calls will fail - write will fail on second and
 * subsequent calls (ie, can't export a currently-exported pin).
 */
int gpio_enable(void)
{
    int fd;
    int num_bytes;

    /* Export enables control, creates additional gpio pin entries in sysfs */
    num_bytes = strlen(GPIO_PIN);
    if (((fd = open(GPIO_EXPORT, O_WRONLY)) == -1) ||
	(write(fd, GPIO_PIN, num_bytes) != num_bytes) ||
	close(fd)) {
	return GPIO_ERROR;
    }

    /* Set pin as output */
    num_bytes = strlen(GPIO_OUTPUT);
    if (((fd = open(GPIO_DIRECTION, O_WRONLY)) == -1) ||
	(write(fd, GPIO_OUTPUT, num_bytes) != num_bytes) ||
	close(fd)) {
	return GPIO_ERROR;
    }

    return GPIO_OK;
}

/*
 * Set the pin as an input (to shut off drive) and Write the GPIO
 * unexport file in sysfs to disable our pin.
 *
 * Returns GPIO_OK on success, GPIO_ERROR on faliure.
 *
 * Note, repeat calls will fail - write will fail on second and
 * subsequent calls (ie, can't unexport a not-exported pin).
 */
int gpio_disable(void)
{
    int fd;
    int num_bytes;

    /* Setting pin as an input disables drive, regardless of current state */
    num_bytes = strlen(GPIO_INPUT);
    if (((fd = open(GPIO_DIRECTION, O_WRONLY)) == -1) ||
	(write(fd, GPIO_INPUT, num_bytes) != num_bytes) ||
	close(fd)) {
	return GPIO_ERROR;
    }

    /* Unexport disables control, removes gpio pin entries from sysfs */
    num_bytes = strlen(GPIO_PIN);
    if (((fd = open(GPIO_UNEXPORT, O_WRONLY)) == -1) ||
	(write(fd, GPIO_PIN, num_bytes) != num_bytes) ||
	close(fd)) {
	return GPIO_ERROR;
    }

    return GPIO_OK;
}

/*
 * Turn on the already-exported pin.
 *
 * Returns GPIO_OK on success, GPIO_ERROR on faliure.
 *
 * Will fail if pin is not exported already.
 */
int gpio_on(void)
{
    int fd;
    int num_bytes;

    /* Write 1, set pin high */
    num_bytes = strlen(GPIO_ON);
    if (((fd = open(GPIO_VALUE, O_WRONLY)) == -1) ||
	(write(fd, GPIO_ON, num_bytes) != num_bytes) ||
	close(fd)) {
	return GPIO_ERROR;
    }

    return GPIO_OK;
}

/*
 * Turn off the already-exported pin.
 *
 * Returns GPIO_OK on success, GPIO_ERROR on faliure.
 *
 * Will fail if pin is not exported already.
 */
int gpio_off(void)
{
    int fd;
    int num_bytes;

    /* Write 0, set pin low */
    num_bytes = strlen(GPIO_OFF);
    if (((fd = open(GPIO_VALUE, O_WRONLY)) == -1) ||
	(write(fd, GPIO_OFF, num_bytes) != num_bytes) ||
	close(fd)) {
	return GPIO_ERROR;
    }

    return GPIO_OK;
}
