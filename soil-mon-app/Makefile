######################################################################
##
## Thomas Ames
## ECEA 5305, assignment #5, Makefile for aesdsocket.c
## July 2023
##

#CC ?= $(CROSS_COMPILE)gcc
#CFLAGS ?= -Wall -Werror -g
#LDFLAGS ?= -pthread

#all: soil-monitor gpio-test
all: gpio-test

clean:
	rm -f soil-monitor *.o

soil-monitor: gpio.o
	$(CC) -o $@ $^ $(LDFLAGS)

gpio-test: gpio.o gpio-test.o
	$(CC) -o $@ $^ $(LDFLAGS)
