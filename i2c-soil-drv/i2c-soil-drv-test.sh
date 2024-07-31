#!/bin/sh
# No /bin/bash in buildroot; must use /bin/sh
#
# Test script for i2c soil driver
# Thomas Ames, ECEA 5307, July 28, 2024
#
# Write values 0-255 to driver and verify reads back correctly.
# Write values 255-0 to driver and verify reads back correctly.
# Write 50 random values to driver and verify reads back correctly.
# echo $((1 + $RANDOM % 10))
# $RANDOM returns 0-32767

I2C_SOIL_DEV=/dev/i2c-soil-drv
SIM_ON_CMD=sim-on
SIM_OFF_CMD=sim-off

# Set sim mode active in driver
sim_on() {
    echo -n $SIM_ON_CMD > $I2C_SOIL_DEV
}

# Set sim mode inactive in driver
sim_off() {
    echo -n $SIM_OFF_CMD > $I2C_SOIL_DEV
}

# Takes a single decimal number as a string, convert it to byte data,
# write it the to the driver and read back to verify.  Will call exit
# 1 to terminate script immediately on a mis-match.
write_and_verify() {
    # Convert dec string to hex. Will have leading \x for echo -e
    HEX_IN=`printf "\x%02x" $1`
	  
    # Write then read w/ dd - status=none to avoid record output
    echo -ne "$HEX_IN" > $I2C_SOIL_DEV

    # Add leading "\x" so HEX_OUT matches HEX_IN
    HEX_OUT="\x"`dd if=$I2C_SOIL_DEV count=1 bs=1 status=none|od -t x1|awk '{ print $2}'`

    if [ $HEX_IN != $HEX_OUT ]; then
	echo "FAILED"
	echo "\$HEX_IN="$HEX_IN
	echo "\$HEX_OUT="$HEX_OUT
	exit 1
    fi
}

sim_on

echo -n "Testing write/read 0..255... "
for i in $(seq 0 1 255); do
    write_and_verify $i;
done

# If we get here, we pass. write_and_verify will exit if one iteration failed.
echo "PASS"

echo -n "Testing write/read 255..0... "
for i in $(seq 255 -1 0); do
    write_and_verify $i;
done

echo "PASS"

echo -n "Testing 50 random write/read values... "
for i in $(seq 0 1 50); do
    write_and_verify $(($RANDOM % 256));
done

echo "PASS"

sim_off
