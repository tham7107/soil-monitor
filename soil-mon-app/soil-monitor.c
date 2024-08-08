/**************************************************************************
 *
 * soil-monitor.c
 *
 * i2c soil moisture driver
 *
 * Thomas Ames, July 25, 2024
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <syslog.h>
#include <libgen.h>

/* To get sim on/off commands */
#include "i2c-soil-drv-api.h"

/* GPIO access for pump control */
#include "gpio.h"

/* Target moisture level if not overridden by -t option */
#define DEFAULT_MOISTURE_TARGET	0x80

/* Time to sleep (seconds) between readings. Overridden by -w */
#define SLEEP_TIME		3600

/* Pump run time (seconds). Overriddent by -p */
#define PUMP_TIME		5

/*
 * Print usage to stderr. Arg is program name (ie, argv[0]).
 *
 * Returns nothing
 */
void print_usage(const char* argv0)
{
    fprintf(stderr, "Usage: %s [-s -t <target_moisture>]\n", argv0);
    fprintf(stderr,"   -s : Turn on simulation mode in soil moisture sensor driver\n");
    fprintf(stderr,"        (default is off).\n");
    fprintf(stderr,"   -t <target_moisture> : Set target moisture level, 0-255.\n");
    fprintf(stderr,"        (default is %d).\n", DEFAULT_MOISTURE_TARGET);
    fprintf(stderr,"   -w <wait_time> : Set wait time in seconds between readings.\n");
    fprintf(stderr,"        (default is %d).\n", SLEEP_TIME);
    fprintf(stderr,"   -p <pump_run_time : Set pump run time in seconds (default is %d).\n", PUMP_TIME);
}

/*
 * Call getopts to parse the command line args in argc/argv and fill
 * in the various parameters passed as call-by-reference.
 */
void parse_options(int argc, char *argv[], int *daemonize, const char **sim_cmd,
		   unsigned char *target, int *sleep_time, int *pump_time)
{
    int opt;

    /* Parse options -s, -t xx, and -? */
    while ((opt = getopt(argc, argv, "fst:w:p:?")) != -1) {
	switch (opt) {
	case 'f':
	    *daemonize = 0; /* run in foreground */
	    break;
	case 's':
	    *sim_cmd = SIM_ON_CMD;
	    break;
	case 't':
	    *target = atoi(optarg);
	    break;
	case 'w':
	    *sleep_time = atoi(optarg);
	    break;
	case 'p':
	    *pump_time = atoi(optarg);
	    break;
	case '?':
	default:
	    print_usage(argv[0]);
	    exit(EXIT_FAILURE);
	}
    }
}

/* Catch SIGINT (ctrl-c) and SIGTERM, cleanup and exit */
void signal_handler(int signal)
{
    syslog(LOG_USER|LOG_INFO,"Caught signal %s, exiting.\n",
	   ((SIGINT == signal) ? "SIGINT" :
	    ((SIGTERM == signal) ? "SIGTERM" : "UNKNOWN")));

    /* Disable GPIO control - ignore errors, since we are exiting anyway */
    (void) gpio_disable();
    exit(EXIT_SUCCESS);
}

/*
 * Install signal handler to catch SIGINT (ctrl-c) and SIGTERM.
 * Argument is argv[0] for perror. Returns nothing.
 */
void init_signal_handlers(const char *argv0)
{
    if ((signal(SIGINT, signal_handler) == SIG_ERR) ||
	(signal(SIGTERM, signal_handler) == SIG_ERR)) {
	perror(argv0);
	exit(EXIT_FAILURE);
    }
}

/*
 * Call openlog to prepend program name (argv0) and "[pid]" to logs.
 * Arg daemonize is used to determine foregroung (=0) or background/
 * deamon mode (=1). Log to syslog only in background, syslog and
 * std error in foreground. Returns nothing.
 */
void init_logging(const char *argv0, int daemonize)
{
    char * ident;

    /*
     * Build the ident sting for openlog - basename(argv[0]) + '[' +
     * pid + ']' Basename will return input+offset, ie, indexed
     * pointer into input str Note, dirname will modify input string,
     * replacing final '/' with 0.
     *
     * Malloc strlen(argv[0])+16 for space for '['+ pid + ']'.
     * Technically, we should free this, but exit will handle it.
     */
    ident = malloc(strlen(argv0)+16);
    sprintf(ident, "%s[%d]",argv0,getpid());
    ident = basename(ident);

    if (daemonize) {
	/* Now running in daemon, log to syslog only (no stderr) */
	openlog(ident, 0, LOG_USER);
    } else {
	/* If running in the foreground, log to syslog and stderr */
	openlog(ident, LOG_PERROR, LOG_USER);
    }
}

int main(int argc, char *argv[])
{
    /* Defaults for options */
    const char * sim_cmd = SIM_OFF_CMD;
    int daemonize = 1; /* default is to run as deamon w/out -f */
    unsigned char target = DEFAULT_MOISTURE_TARGET;
    int sleep_time = SLEEP_TIME;
    int pump_time = PUMP_TIME;
    int soil_drv_fd;
    unsigned char current;

    parse_options(argc, argv, &daemonize, &sim_cmd, &target,
		  &sleep_time, &pump_time);

    init_signal_handlers(argv[0]);

    /*
     * Daemonize before init_logging so getpid returns the correct value
     * (either foreground process w/ -f or background daemon w/out -f)
     * On success, deamon return 0 in child, does not return in parent.
     * On failure, return -1 in parent, no child created.
     */
    if (daemonize && daemon(0,0)) {
	perror(argv[0]);
	exit(EXIT_FAILURE);
    }

    init_logging(argv[0], daemonize);

    syslog(LOG_USER|LOG_INFO,"Options parsed. simulation=%s target=%d,\n",
	   sim_cmd, target);
    syslog(LOG_USER|LOG_INFO,"sleep_time=%d, pump_time=%d, foreground=%s\n",
	   sleep_time, pump_time, ((!daemonize) ? "yes" : "no"));

    if ((soil_drv_fd = open(I2C_SOIL_DEV, O_RDWR)) == -1) {
	perror(argv[0]);
	exit(EXIT_FAILURE);
    }

    /* Set sim mode so we are in a known state */
    if (write(soil_drv_fd, sim_cmd, sizeof(sim_cmd)) != sizeof(sim_cmd)) {
	perror(argv[0]);
	exit(EXIT_FAILURE);
    }

    /* Enable GPIO control - Any subsequent exits should call gpio_disable() */
    if (gpio_enable() == GPIO_ERROR) {
	perror(argv[0]);
	exit(EXIT_FAILURE);
    }

    syslog(LOG_USER|LOG_INFO,"Init done, entering main loop\n");
    while (1) {
	/* Read current moisture level */
	if (read(soil_drv_fd, &current, sizeof(current)) != sizeof(current)) {
	    perror(argv[0]);
	    /* Disable GPIO control - ignore errors; exiting anyway */
	    (void) gpio_disable();
	    exit(EXIT_FAILURE);
	}
	syslog(LOG_USER|LOG_INFO,"Current moisture=%d\n", current);

	if (current < target) {
	    if (gpio_on() == GPIO_ERROR) {
		perror(argv[0]);
		(void) gpio_disable();
		exit(EXIT_FAILURE);
	    }
	    syslog(LOG_USER|LOG_INFO,"Pump on, runtime=%d sec\n",pump_time);
	    /*
	     * Technically, we should check the return value from
	     * sleep.  Non-zero means sleep was interupted by a
	     * signal, but we exit on any signals (either gracefully
	     * if caught, or non- gracefully if ignored), so partial
	     * sleeps can't happen here.
	     */
	    (void) sleep(pump_time);
	    if (gpio_off() == GPIO_ERROR) {
		perror(argv[0]);
		(void) gpio_disable();
		exit(EXIT_FAILURE);
	    }
	    syslog(LOG_USER|LOG_INFO,"Pump off\n");
	}
	syslog(LOG_USER|LOG_INFO,"Sleeping for %d sec\n",sleep_time);
	sleep(sleep_time);
    }
}
