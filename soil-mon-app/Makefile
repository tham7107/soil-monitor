######################################################################
##
## Thomas Ames
## ECEA 5305, assignment #5, Makefile for aesdsocket.c
## July 2023
##

# Must use override for variables passed in on the make command line.
# https://www.gnu.org/software/make/manual/html_node/Override-Directive.html
override CFLAGS += -I../i2c-soil-drv
# paho-mqtt3c is MQTTClient, paho-mqtt3a is MQTTAsync
override LDFLAGS += -l paho-mqtt3c

all: soil-monitor gpio-test

clean:
	rm -f soil-monitor *.o

soil-monitor: gpio.o soil-monitor.o
	$(CC) -o $@ $^ $(LDFLAGS)

gpio-test: gpio.o gpio-test.o
	$(CC) -o $@ $^ $(LDFLAGS)
