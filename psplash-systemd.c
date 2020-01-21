/*
 * pslash-systemd - systemd integration for psplash
 *
 * Copyright (c) 2020 Toradex
 *
 * Author: Stefan Agner <stefan.agner@toradex.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <systemd/sd-bus.h>
#include <systemd/sd-event.h>
#include "psplash.h"

#define PSPLASH_UPDATE_USEC 1000000

typedef uint64_t usec_t;

static int pipe_fd;

int get_progress(void)
{
	sd_bus_error error = SD_BUS_ERROR_NULL;
	static double current_progress = 0;
	double progress = 0;
	sd_bus *bus = NULL;
	int r;
	char buffer[20];
	int len;

        /* Connect to the system bus */
	r = sd_bus_new(&bus);
	if (r < 0)
		goto finish;

	r = sd_bus_set_address(bus, "unix:path=/run/systemd/private");
	if (r < 0)
		goto finish;

	r = sd_bus_start(bus);
	if (r < 0) {
		fprintf(stderr, "Failed to connect to systemd private bus: %s\n", strerror(-r));
		goto finish;
        }

        /* Issue the method call and store the respons message in m */
	r = sd_bus_get_property_trivial(bus,
		"org.freedesktop.systemd1",           /* service to contact */
		"/org/freedesktop/systemd1",          /* object path */
		"org.freedesktop.systemd1.Manager",   /* interface name */
		"Progress",                           /* method name */
		&error,                               /* object to return error in */
		'd',                                  /* return message on success */
		&progress);                           /* value */
	if (r < 0) {
		fprintf(stderr, "Failed to get progress: %s\n", error.message);
		goto finish;
	}

	/*
	 * Systemd's progress seems go backwards at times. Prevent that
	 * progress bar on psplash goes backward by just communicating the
	 * highest observed progress so far.
	 */
	if (current_progress < progress)
		current_progress = progress;

	len = snprintf(buffer, 20, "PROGRESS %d", (int)(current_progress * 100));
	write(pipe_fd, buffer, len + 1);

	if (progress == 1.0) {
		printf("Systemd reported progress of 1.0, quit psplash.\n");
		write(pipe_fd, "QUIT", 5);
		r = -1;
	}

finish:
	sd_bus_error_free(&error);
	sd_bus_unref(bus);

	return r;
}

int psplash_handler(sd_event_source *s,
			uint64_t usec,
			void *userdata)
{
	sd_event *event = userdata;
	int r;

	r = get_progress();
	if (r < 0)
		goto err;

	r = sd_event_source_set_time(s, usec + PSPLASH_UPDATE_USEC);
	if (r < 0)
		goto err;

	return 0;
err:
	sd_event_exit(event, EXIT_FAILURE);

	return r;
}

int main()
{
	sd_event *event;
	sd_event_source *event_source = NULL;
	int r;
	sigset_t ss;
	usec_t time_now;
	char *rundir;

	/* Open pipe for psplash */
	rundir = getenv("PSPLASH_FIFO_DIR");

	if (!rundir)
		rundir = "/run";

	chdir(rundir);

	if ((pipe_fd = open (PSPLASH_FIFO,O_WRONLY|O_NONBLOCK)) == -1) {
		fprintf(stderr, "Error unable to open fifo");
		exit(EXIT_FAILURE);
	}

	r = sd_event_default(&event);
	if (r < 0)
		goto finish;

	if (sigemptyset(&ss) < 0 ||
	    sigaddset(&ss, SIGTERM) < 0 ||
	    sigaddset(&ss, SIGINT) < 0) {
		r = -errno;
		goto finish;
	}

	/* Block SIGTERM first, so that the event loop can handle it */
	if (sigprocmask(SIG_BLOCK, &ss, NULL) < 0) {
		r = -errno;
		goto finish;
	}

	/* Let's make use of the default handler and "floating" reference
	 * features of sd_event_add_signal() */
	r = sd_event_add_signal(event, NULL, SIGTERM, NULL, NULL);
	if (r < 0)
		goto finish;

	r = sd_event_add_signal(event, NULL, SIGINT, NULL, NULL);
	if (r < 0)
		goto finish;

	r = sd_event_now(event, CLOCK_MONOTONIC, &time_now);
	if (r < 0)
		goto finish;

	r = sd_event_add_time(event, &event_source, CLOCK_MONOTONIC,
			      time_now, 0, psplash_handler, event);
	if (r < 0)
		goto finish;

	r = sd_event_source_set_enabled(event_source, SD_EVENT_ON);
	if (r < 0)
		goto finish;

	r = sd_event_loop(event);
finish:
	event = sd_event_unref(event);
	close(pipe_fd);

	return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
