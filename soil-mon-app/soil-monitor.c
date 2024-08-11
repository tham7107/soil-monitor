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

#include "MQTTClient.h"

/* To get sim on/off commands */
#include "i2c-soil-drv-api.h"

/* GPIO access for pump control */
#include "gpio.h"

/*Client ID string passed to broker */
#define MQTT_CLIENT_ID		"soil-monitor"
#define MQTT_TOPIC		"soil-monitor"
#define MQTT_QOS		0 /* 0=fire and forget */
#define MQTT_MSG_BUFSIZE	100

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
    fprintf(stderr,"   -p <pump_run_time> : Set pump run time in seconds (default is %d).\n", PUMP_TIME);
    fprintf(stderr,"   -m <broker_URI> : Publish MQTT messages to broker <broker_URI>\n");
    fprintf(stderr,"        (default is off).\n");
}

/*
 * Call getopts to parse the command line args in argc/argv and fill
 * in the various parameters passed as call-by-reference.
 */
void parse_options(int argc, char *argv[], int *daemonize, const char **sim_cmd,
		   unsigned char *target, int *sleep_time, int *pump_time,
		   char **mqtt_broker_uri)
{
    int opt;

    /* Parse options -s, -t xx, and -? */
    while ((opt = getopt(argc, argv, "fst:w:p:m:?")) != -1) {
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
	case 'm':
	    if (*mqtt_broker_uri = malloc(strlen(optarg)+1)) { /* +1=space for \0 */
		strcpy(*mqtt_broker_uri, optarg);
	    }
	    break;
	case '?':
	default:
	    print_usage(argv[0]);
	    exit(EXIT_FAILURE);
	}
    }
}

/*
 * Catch SIGINT (ctrl-c) and SIGTERM, cleanup and exit
 * Only cleanup needed is to un-export the GPIO pin and exit.
 */
void signal_handler(int signal)
{
    if (SIGUSR1 == signal) {
	/* Do nothing, sleep will return early */
    } else {
	/* Exit on all others */
	syslog(LOG_USER|LOG_INFO, "Caught signal %s, exiting.\n",
	       ((SIGINT == signal) ? "SIGINT" :
		((SIGTERM == signal) ? "SIGTERM" : "UNKNOWN")));

	/* Disable GPIO control - ignore errors, since we are exiting anyway */
	(void) gpio_disable();
	exit(EXIT_SUCCESS);
    }
}

/*
 * Install signal handler to catch SIGINT (ctrl-c) and SIGTERM.
 * Argument is argv[0] for perror. Returns nothing.
 */
void init_signal_handlers(const char *argv0)
{
    if ((signal(SIGINT, signal_handler) == SIG_ERR) ||
	(signal(SIGTERM, signal_handler) == SIG_ERR) ||
	(signal(SIGUSR1, signal_handler) == SIG_ERR)) {
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

/*
 * Connect to the broker. Called from mqtt_client_init and
 * mqtt_connection_lost. Returns result from final call to
 * MQTTClient_connect (successful or not).
 *
 * At bootup, it can take a little while for the RPi to get an
 * address through DHCP, and the client connect call will fail
 * in this case. If the first connection attempt fails, try
 * again, up to 5 times, with a 5 sec sleep in between. Usually,
 * the first attempt at boot fails, but the second one succeeds.
 */
int mqtt_client_connect(MQTTClient mqtt_client)
{
    MQTTClient_connectOptions opts = MQTTClient_connectOptions_initializer;
    int conn_attempt_result=-1;
    int conn_attempt_count=0;

    conn_attempt_result = MQTTClient_connect(mqtt_client, &opts);
    /* If it failed, sleep and try again, up to 5 times. */
    while ((conn_attempt_result != MQTTCLIENT_SUCCESS) &&
	   (conn_attempt_count < 5)) {
	sleep(5);
	conn_attempt_result = MQTTClient_connect(mqtt_client, &opts);
	conn_attempt_count++;
    }

    if (conn_attempt_result == MQTTCLIENT_SUCCESS) {
	syslog(LOG_USER|LOG_INFO, "MQTTClient_connect success.\n");
    } else {
	syslog(LOG_USER|LOG_INFO, "MQTTClient_connect failed, returned %d.\n",
	       conn_attempt_result);
    }

    return conn_attempt_result;
}

/*
 * MQTT Callback to handle connection losses. Logs a message, attempts
 * reconnection, and returns. Context is set to the client handle in
 * MQTTClient_setCallbacks in mqtt_client_init
 */
void mqtt_connection_lost(void *context, char *cause)
{
    syslog(LOG_USER|LOG_INFO,
	   "MQTT connection lost, attempting reconnection. Cause: %s\n", cause);

    /*
     * Once running, consider failed reconnects a non-fatal error
     * Already syslog'ed error in mqtt_client_connect, so ignore return
     * and allow soil-monitor to continue without MQTT.
     */
    (void) mqtt_client_connect(context);
}

/*
 * Message arrived; only callback that cannot be NULL - returns 1 to
 * indicate we handled it. We don't expect to get any messages (not
 * subscribing to any), but we need to provide an RX callback to
 * MQTTClient_setCallbacks (can't pass NULL).
 *
 * syslogs the message, frees the buffers, and returns 1 to indicate handled.
 */
int mqtt_message_arrived(void *context, char *topicName, int topicLen,
			 MQTTClient_message *message)
{
    syslog(LOG_USER|LOG_INFO, "MQTT message arrived, topic: %s, msg: %s\n",
	   topicName, message->payload);
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

/*
 * Basic client init. Create a client, setup callbacks, connect to the
 * broker, and return.
 *
 * argv0 is used for perror, mqtt_client is call by ref and filled in,
 * mqtt_broker_uri is the full URI for the broker (eg,
 * "tcp://broker.mqtt.cool:1883")
 */
void mqtt_client_init(const char *argv0, MQTTClient *mqtt_client,
		      const char *mqtt_broker_uri)
{
    if (MQTTClient_create(mqtt_client, mqtt_broker_uri, MQTT_CLIENT_ID,
			  MQTTCLIENT_PERSISTENCE_NONE, NULL) !=
	MQTTCLIENT_SUCCESS) {
	perror(argv0);
	exit(EXIT_FAILURE);
    }

    /*
     * Pass in newly-created client for context pointer (both are
     * (void *) so that mqtt_connection_lost can attempt reconnect.
     */
    if (MQTTClient_setCallbacks(*mqtt_client, *mqtt_client,
				mqtt_connection_lost,
				mqtt_message_arrived, NULL) !=
	MQTTCLIENT_SUCCESS) {
	perror(argv0);
	exit(EXIT_FAILURE);
    }

    /* Connect will fail if not connected to the internet */
    if (mqtt_client_connect(*mqtt_client) != MQTTCLIENT_SUCCESS) {
	perror(argv0);
	exit(EXIT_FAILURE);
    }
}

/*
 * Publish a message via a client alread set up with a call to
 * mqtt_client_init. argv0 is used for error reporting,
 * client is passed (by value) in mqtt_client, null terminated
 * string in string.
 */
void mqtt_publish_msg(const char *argv0, MQTTClient mqtt_client,
		      char *string)
{
    MQTTClient_message mqtt_msg = MQTTClient_message_initializer;
    int retval;

    mqtt_msg.payload = string;
    mqtt_msg.payloadlen = strlen(string);
    mqtt_msg.qos = MQTT_QOS;
    mqtt_msg.retained = 0;

    if ((retval = MQTTClient_publishMessage(mqtt_client, MQTT_TOPIC, &mqtt_msg,
					    NULL)) != MQTTCLIENT_SUCCESS) {
	syslog(LOG_USER|LOG_INFO,
	       "MQTTClient_publishMessage failed, retval=%d\n", retval);
	perror(argv0);
    }
}

int main(int argc, char *argv[])
{
    /* Defaults for options */
    const char *sim_cmd = SIM_OFF_CMD;
    int daemonize = 1; /* default is to run as deamon w/out -f */
    unsigned char target = DEFAULT_MOISTURE_TARGET;
    int sleep_time = SLEEP_TIME;
    int pump_time = PUMP_TIME;
    char *mqtt_broker_uri = NULL;
    int soil_drv_fd;
    MQTTClient mqtt_client = NULL;
    char *msgbuf = NULL;
    unsigned char current;

    parse_options(argc, argv, &daemonize, &sim_cmd, &target,
		  &sleep_time, &pump_time, &mqtt_broker_uri);

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

    syslog(LOG_USER|LOG_INFO, "Options parsed. simulation=%s target=%d,\n",
	   sim_cmd, target);
    syslog(LOG_USER|LOG_INFO, "sleep_time=%d, pump_time=%d, foreground=%s,\n",
	   sleep_time, pump_time, ((!daemonize) ? "yes" : "no"));
    if (mqtt_broker_uri) {
	syslog(LOG_USER|LOG_INFO, "MQTT enabled, broker=%s.\n",
	       mqtt_broker_uri);
	mqtt_client_init(argv[0], &mqtt_client, mqtt_broker_uri);
    } else {
	syslog(LOG_USER|LOG_INFO, "MQTT disabled.\n");
    }

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

    if (!(msgbuf = malloc(MQTT_MSG_BUFSIZE))) {
	perror(argv[0]);
	exit(EXIT_FAILURE);
    }

    snprintf(msgbuf, MQTT_MSG_BUFSIZE, "Init done, entering main loop\n");
    syslog(LOG_USER|LOG_INFO, msgbuf);
    if (mqtt_broker_uri) {
	mqtt_publish_msg(argv[0], mqtt_client, msgbuf);
    }
    while (1) {
	/* Read current moisture level */
	if (read(soil_drv_fd, &current, sizeof(current)) != sizeof(current)) {
	    perror(argv[0]);
	    /* Disable GPIO control - ignore errors; exiting anyway */
	    (void) gpio_disable();
	    exit(EXIT_FAILURE);
	}
	snprintf(msgbuf, MQTT_MSG_BUFSIZE, "Current moisture=%d\n", current);
	syslog(LOG_USER|LOG_INFO, msgbuf);
	if (mqtt_broker_uri) {
	    mqtt_publish_msg(argv[0], mqtt_client, msgbuf);
	}

	if (current < target) {
	    if (gpio_on() == GPIO_ERROR) {
		perror(argv[0]);
		(void) gpio_disable();
		exit(EXIT_FAILURE);
	    }
	    snprintf(msgbuf, MQTT_MSG_BUFSIZE, "Pump on, runtime=%d sec\n",
		     pump_time);
	    syslog(LOG_USER|LOG_INFO, msgbuf);
	    if (mqtt_broker_uri) {
		mqtt_publish_msg(argv[0], mqtt_client, msgbuf);
	    }
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
	    snprintf(msgbuf, MQTT_MSG_BUFSIZE, "Pump off\n");
	    syslog(LOG_USER|LOG_INFO, msgbuf);
	    if (mqtt_broker_uri) {
		mqtt_publish_msg(argv[0], mqtt_client, msgbuf);
	    }

	}
	snprintf(msgbuf, MQTT_MSG_BUFSIZE, "Sleeping for %d sec\n",
		 sleep_time);
	syslog(LOG_USER|LOG_INFO, msgbuf);
	if (mqtt_broker_uri) {
	    mqtt_publish_msg(argv[0], mqtt_client, msgbuf);
	}
	sleep(sleep_time);
    }
}
