/*
 * Copyright (C) 2007-2008  Byron Bradley (byron.bbradley@gmail.com)
 * Copyright (C) 2008, 2009  Martin Michlmayr (tbm@cyrius.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
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

#include <fcntl.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <linux/input.h>

#include "picmodule.h"

static int serial;
static struct termios oldtio, newtio;
static pthread_t ts219_thread;

static int serial_read(char *buf, int len)
{
	int err;

	err = read(serial, buf, len);
	buf[err] = 0;

	return err;
}

static int serial_write(char *buf, int len)
{
	int err;

	err = write(serial, buf, len);

	return err;
}

int ts219_read_serial_events()
{
	char buf[100];
	int err = serial_read(buf, 100);
	if (err < 0)
		return err;
	switch (buf[0]) {
	case 0x40:
		call_function("power_button", "%d", 3);
		break;
	case 0x73:
	case 0x75:
	case 0x77:
	case 0x79:
		call_function("fan_error", "");
		break;
	case 0x74:
	case 0x76:
	case 0x78:
	case 0x7a:
		call_function("fan_normal", "");
		break;
	case 0x80 ... 0x8f: /*  0 - 15 */
	case 0x90 ... 0x9f: /* 16 - 31 */
	case 0xa0 ... 0xaf: /* 32 - 47 */
	case 0xb0 ... 0xbf: /* 48 - 63 */
	case 0xc0 ... 0xc6: /* 64 - 70 */
		call_function("temp", "%d", buf[0] - 128);
		break;
	case 0x38: /* 71 - 79 */
		call_function("temp", "%d", 75);
		break;
	case 0x39: /* 80 or higher */
		call_function("temp", "%d", 80);
		break;
	default:
		fprintf(stderr, "(PIC 0x%x) unknown command from PIC\n", buf[0]);
	}

	return -1;
}

static void *serial_poll(void *tmp)
{
	int err;
	fd_set rset;

	FD_ZERO(&rset);
	FD_SET(serial, &rset);

	for (;;) {
		err = select(serial + 1, &rset, NULL, NULL, NULL);
		if (err <= 0) {
			FD_SET(serial, &rset);
			continue;
		}
		ts219_read_serial_events();
		FD_SET(serial, &rset);
	}

	return NULL;
}

static int set_nonblock(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0)
		flags = 0;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int serial_open(char *device)
{
	char buf[100];
	int err;

	if ((serial = open(device , O_RDWR)) < 0) {
		sprintf(buf, "Failed to open %s", device);
		perror(buf);
		return -1;
	}
	err = set_nonblock(serial);
	if (err < 0) {
		perror("Error setting nonblock");
		return -1;
	}

	tcgetattr(serial, &oldtio);
	memset(&newtio, 0, sizeof(newtio));

	newtio.c_iflag |= IGNBRK;
	newtio.c_lflag &= ~(ISIG | ICANON | ECHO);
	newtio.c_cflag = B19200 | CS8 | CLOCAL | CREAD;
	newtio.c_cc[VMIN] = 1;
	newtio.c_cc[VTIME] = 0;
	cfsetospeed(&newtio, B19200);
	cfsetispeed(&newtio, B19200);

	err = tcsetattr(serial, TCSANOW, &newtio);
	if (err < 0) {
		sprintf(buf, "Failed to set attributes for %s", device);
		perror(buf);
		return -1;
	}

	return 0;
}

static void serial_close()
{
	tcsetattr(serial, TCSANOW, &oldtio);
	close(serial);
}

static int ts219_powerled(int argc, const char **argv)
{
	char code = 0;

	if (argc != 1)
		return -1;

	if (strcmp(argv[0], "on") == 0)
		code = 0x4d;
	else if (strcmp(argv[0], "1hz") == 0)
		code = 0x4e;
	else if (strcmp(argv[0], "2hz") == 0)
		code = 0x4c;
	else if (strcmp(argv[0], "off") == 0)
		code = 0x4b;
	else
		return -1;

	return serial_write(&code, 1);
	return 0;
}

static int ts219_statusled(int argc, const char **argv)
{
	char code = 0;

	if (argc != 1)
		return -1;

	if (strcmp(argv[0], "red2hz") == 0)
		code = 0x54;
	else if (strcmp(argv[0], "green2hz") == 0)
		code = 0x55;
	else if (strcmp(argv[0], "greenon") == 0)
		code = 0x56;
	else if (strcmp(argv[0], "redon") == 0)
		code = 0x57;
	else if (strcmp(argv[0], "greenred2hz") == 0)
		code = 0x58;
	else if (strcmp(argv[0], "off") == 0)
		code = 0x59;
	else if (strcmp(argv[0], "green1hz") == 0)
		code = 0x5a;
	else if (strcmp(argv[0], "red1hz") == 0)
		code = 0x5b;
	else if (strcmp(argv[0], "greenred1hz") == 0)
		code = 0x5c;
	else
		return -1;

	return serial_write(&code, 1);
	return 0;
}

static int ts219_buzz(int argc, const char **argv)
{
	char code = 0;

	if (argc != 1)
		return -1;

	if (strcmp(argv[0], "short") == 0)
		code = 0x50;
	else if (strcmp(argv[0], "long") == 0)
		code = 0x51;
	else
		return -1;

	return serial_write(&code, 1);
	return 0;
}

static int ts219_fanspeed(int argc, const char **argv)
{
	char code = 0;

	if (argc != 1)
		return -1;

	if (strcmp(argv[0], "stop") == 0)
		code = 0x30;
	else if (strcmp(argv[0], "silence") == 0)
		code = 0x31;
	else if (strcmp(argv[0], "low") == 0)
		code = 0x32;
	else if (strcmp(argv[0], "medium") == 0)
		code = 0x33;
	else if (strcmp(argv[0], "high") == 0)
		code = 0x34;
	else if (strcmp(argv[0], "full") == 0)
		code = 0x35;
	else
		return -1;

	return serial_write(&code, 1);
	return 0;
}

static int ts219_usbled(int argc, const char **argv)
{
	char code = 0;

	if (argc != 1)
		return -1;

	if (strcmp(argv[0], "on") == 0)
		code = 0x60;
	else if (strcmp(argv[0], "8hz") == 0)
		code = 0x61;
	else if (strcmp(argv[0], "off") == 0)
		code = 0x62;
	else
		return -1;

	return serial_write(&code, 1);
	return 0;
}

static int ts219_autopower(int argc, const char **argv)
{
	char code = 0;

	if (argc != 1)
		return -1;

	if (strcmp(argv[0], "on") == 0)
		code = 0x48;
	else if (strcmp(argv[0], "off") == 0)
		code = 0x49;
	else
		return -1;

	return serial_write(&code, 1);
	return 0;
}

int ts219_init(int argc, const char **argv)
{
	int err;

	if (argc > 0) {
		printf("%s: module does not take any arguments\n", __func__);
		return -1;
	}

	err = serial_open("/dev/ttyS1");
	if (err < 0)
		return err;

	err = register_command("statusled",
	                       "Change the status LED",
	                       "Change the status LED, options are:\n"
	                       "\tred2hz\n\tgreen2hz\n\tgreenon\n\tredon\n"
	                       "\tgreenred2hz\n\toff\n\tgreen1hz\n\tred1hz\n",
	                       ts219_statusled);
	err = register_command("powerled",
	                       "Change the power LED",
	                       "Change the power LED, options are:\n"
	                       "\ton\n\toff\n\t1hz\n\t2hz\n",
	                       ts219_powerled);
	err = register_command("buzzer",
	                       "Buzz",
	                       "Buzz, options are:\n"
	                       "\tshort\n\tlong\n",
	                       ts219_buzz);
	err = register_command("fanspeed",
	                       "Set the fanspeed",
	                       "Set the fanspeed, options are:\n"
	                       "\tstop\n\tsilence\n\tlow\n\tmedium\n"
	                       "\thigh\n\tfull\n",
	                       ts219_fanspeed);
	err = register_command("usbled",
	                       "Set the usbled",
	                       "Set the usbled, options are:\n"
	                       "\ton\n\t8hz\n\toff\n",
	                       ts219_usbled);
	err = register_command("autopower",
	                       "Control the automatic power mechanism",
	                       "Control the automatic power mechanism, options are:\n"
	                       "\ton\n\toff\n",
	                       ts219_autopower);

	return pthread_create(&ts219_thread, NULL, serial_poll, NULL);
}

void ts219_exit(void)
{
	serial_close();
}

struct picmodule ts219_module = {
	.name           = "ts219",
	.init           = ts219_init,
	.exit           = ts219_exit,
};
