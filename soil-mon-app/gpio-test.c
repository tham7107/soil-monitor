/**************************************************************************
 *
 * gpio-test.c
 *
 * Test routines for gpio control
 *
 * Thomas Ames, August 2, 2024
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "gpio.h"

/*
 * Test for GPIO control. Test requires manual verification; either
 * connect the pump to the GPIO with a MOSFET or motor driver, or use
 * a voltmeter to monitor pin output (on=3.3V, off=0V)
 */
int main()
{
    printf("GPIO_PIN: %s\n", GPIO_PIN);
    printf("GPIO_DIRECTORY: %s\n", GPIO_DIRECTORY);
    printf("GPIO_EXPORT: %s\n", GPIO_EXPORT);
    printf("GPIO_UNEXPORT: %s\n", GPIO_UNEXPORT);
    printf("GPIO_DIRECTION: %s\n", GPIO_DIRECTION);
    printf("GPIO_OUTPUT: %s\n", GPIO_OUTPUT);
    printf("GPIO_INPUT: %s\n", GPIO_INPUT);
    printf("GPIO_VALUE: %s\n", GPIO_VALUE);
    printf("GPIO_ON: %s\n", GPIO_ON);
    printf("GPIO_OFF: %s\n", GPIO_OFF);

    printf("\ngpio_enable(): ");
    if (gpio_enable()) {
	perror("");
	exit(1);
    }
    printf("Success\n");

    printf("\nTest 5 turn on/off with 2 second delay.\n");
    for (int i=0; i<5; i++) {
	printf("gpio_on(): ");
	if(gpio_on()) {
	    perror("");
	    exit(1);
	}
	printf("Success\n");
	sleep(2);
	printf("gpio_off(): ");
	if(gpio_off()) {
	    perror("");
	    exit(1);
	}
	printf("Success\n");
	sleep(2);
    }
    /* Turn on before disable - disable should shut off */
    printf("\nFinal turn on before disable.\n");
    printf("gpio_on(): ");
    if(gpio_on()) {
	perror("");
	exit(1);
    }
    printf("Success\n");
    sleep(2);
    printf("gpio_disable(), should shut off output: ");
    if(gpio_disable()) {
	perror("");
	exit(1);
    }
    printf("Success\n");
}
