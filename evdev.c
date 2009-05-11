/*
 * Copyright (C) 2008  Byron Bradley (byron.bbradley@gmail.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <fcntl.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signal.h>
#include <syslog.h>
#include <termios.h>
#include <unistd.h>
#include <linux/input.h>

#include "picmodule.h"

struct event_listen {
	int code;
	char *command;
	time_t pressed;
};
static struct event_listen **events;
static pthread_t evdev_thread;
static int event;

int evdev_event()
{
	struct input_event ie;
	char buf[100];
	int err, i, down;

	err = read(event, buf, 100);
	if (err < 0)
		return err;

	memcpy(&ie, buf, sizeof(struct input_event));

	for (i = 0; events[i]; ++i) {
		if (ie.code == events[i]->code) {
			if (ie.value == 1) {
				events[i]->pressed = time(NULL);
			} else {
				down = time(NULL) - events[i]->pressed;
				call_function(events[i]->command, "%d", down);
			}
			break;
		}
	}

	if (!events[i])
		print_log(LOG_WARNING, "evdev: Unknown event %d", ie.code);

	return 0;
}

static void *evdev_poll(void *tmp)
{
	for (;;)
		evdev_event();

	return NULL;
}

int evdev_init(int argc, const char **argv)
{
	int i, evcount;
	struct event_listen *el;

	evcount = (argc - 1) / 2;
	if (evcount < 1 || (argc - 1) % 2 != 0) {
		print_log(LOG_ERR, "evdev: got an uneven number of arguments");
		return -1;
	}

	events = calloc(evcount + 1, sizeof(struct event_listen*));
	for (i = 1; i < argc; i += 2) {
		el = malloc(sizeof(struct event_listen));
		el->code = atoi(argv[i]);
		el->command = malloc(strlen(argv[i+1]) + 1);
		strncpy(el->command, argv[i+1], strlen(argv[i+1]) + 1);
		events[(i-1)/2] = el;
	}

	event = open(argv[0], 0);
	if (event < 0) {
		print_log(LOG_ERR, "evdev: Error opening %s: %s",
		       argv[0], strerror(errno));
		for (i = 0; events[i]; ++i)
			free(events[i]);
		free(events);
		return -1;
	}

	return pthread_create(&evdev_thread, NULL, evdev_poll, NULL);
}

void evdev_exit()
{
	int i;

	pthread_kill(evdev_thread, SIGINT);
	close(event);

	for (i = 0; events[i]; ++i)
		free(events[i]);
	free(events);
}

struct picmodule evdev_module = {
	.name           = "evdev",
	.init           = evdev_init,
	.exit           = evdev_exit,
};
