#!/bin/sh

# Simple script to turn pump on/off by controlling its GPIO pin
# Thomas Ames, ECEA 5307, August 2024

GPIO_PIN=17
GPIO_DIR=/sys/class/gpio

# Export and set direction only needs to be done once.
# Technically, we should also unexport when done by
# writing GPIO number to /sys/class/gpio/unexport
case "$1" in
    on)
	if [ ! -e $GPIO_DIR/gpio$GPIO_PIN ]; then
	    echo -n $GPIO_PIN > $GPIO_DIR/export
	    echo -n "out" > $GPIO_DIR/gpio$GPIO_PIN/direction
	fi
	echo -n "1" > $GPIO_DIR/gpio$GPIO_PIN/value
	;;
    off)
        if [ ! -e $GPIO_DIR/gpio$GPIO_PIN ]; then
            echo -n $GPIO_PIN > $GPIO_DIR/export
            echo -n "out" > $GPIO_DIR/gpio$GPIO_PIN/direction
        fi
	echo -n "0" > $GPIO_DIR/gpio$GPIO_PIN/value
	;;
    *)
	echo "Usage: $0 {on|off}"
	exit 1
esac

exit 0
