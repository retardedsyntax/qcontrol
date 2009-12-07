/*
 * Copyright (C) 2007-2008  Byron Bradley (byron.bbradley@gmail.com)
 * Copyright (C) 2008, 2009  Martin Michlmayr (tbm@cyrius.com)
 * Copyright (C) 2009  Bernhard R. Link (brlink@debian.org)
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

#include <errno.h>
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
static pthread_t a125_thread;

static int serial_read(char *buf, int len)
{
	int err, got = 0;

	while (len > 0) {
		errno = 0;
		err = read(serial, buf, len);
		if (err >= 0) {
			if (err > len)
				err = len;
			got += err;
			buf[err] = 0;
			buf += err;
			len -= err;
		} else {
			perror("Error reading from A125:");
			return err;
		}
		if (err == 0)
			fprintf(stderr, "EOF from A125???\n");
	}
	return got;
}

static int serial_write(char *buf, int len)
{
	int err;

	err = write(serial, buf, len);

	return err;
}

static int a125_read_serial_events(void)
{
	char buf[101];
	int err = serial_read(buf, 1);
	if (err <= 0)
		return err;
	if (buf[0] != 0x53) {
		fprintf(stderr, "Unknown command 0x%x from A125! stream out of sync\n", buf[0]);
		return -1;
	}
	err = serial_read(buf, 1);
	if (err <= 0)
		return err;
	switch ((unsigned char)buf[0]) {
	case 0x01:
		err = serial_read(buf, 2);
		if (err < 0)
			return -1;
		fprintf(stderr, "A125 ID is %04x\n", buf[0]*256+buf[1]);
		break;
	case 0x05:
		err = serial_read(buf, 2);
		if (err < 0)
			return -1;
		/* TODO: make them available to LUA */
		fprintf(stderr, "Button State now %x %x\n", buf[0], buf[1]);
		break;
	case 0x08:
		err = serial_read(buf, 2);
		if (err < 0)
			return -1;
		fprintf(stderr, "A125 Protocol version is %04x\n", buf[0]*256+buf[1]);
		break;
	case 0xAA:
		fprintf(stderr, "A125 Reset OK\n");
		break;
	case 0xFB:
		err = serial_read(buf, 1);
		if (err < 0)
			return -1;
		fprintf(stderr, "A125 NACKs command %x\n", buf[0]);
		break;
	default:
		fprintf(stderr, "(0x%x) unknown command from A125\n", buf[0]);
	}

	return -1;
}

static void *serial_poll(void *tmp UNUSED)
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
		a125_read_serial_events();
		FD_SET(serial, &rset);
	}

	return NULL;
}

static int serial_open(const char *device)
{
	char buf[100];
	int err;

	if ((serial = open(device , O_RDWR)) < 0) {
		snprintf(buf, 100, "Failed to open %s", device);
		perror(buf);
		return -1;
	}
	tcgetattr(serial, &oldtio);
	memset(&newtio, 0, sizeof(newtio));

	newtio.c_iflag |= IGNBRK;
	newtio.c_lflag &= ~(ISIG | ICANON | ECHO);
	newtio.c_cflag = B1200 | CS8 | CLOCAL | CREAD;
	newtio.c_cc[VMIN] = 1;
	newtio.c_cc[VTIME] = 0;
	cfsetospeed(&newtio, B1200);
	cfsetispeed(&newtio, B1200);

	err = tcsetattr(serial, TCSANOW, &newtio);
	if (err < 0) {
		snprintf(buf, 100, "Failed to set attributes for %s", device);
		perror(buf);
		return -1;
	}

	return 0;
}

static void serial_close(void)
{
	tcsetattr(serial, TCSANOW, &oldtio);
	close(serial);
}

static int a125_backlight(int argc, const char **argv)
{
	char code[3] = { 0x4D, 0x5E, 0x00 };

	if (argc != 1)
		return -1;

	if (strcmp(argv[0], "on") == 0)
		code[2] = 0x01;
	else if (strcmp(argv[0], "off") == 0)
		code[2] = 0x00;
	else
		return -1;

	serial_write(code, 3);
	return 0;
}

static int a125_line(int id, const char *line)
{
	size_t l;
	char code[20] = { 0x4D, 0x0C, 0x00, 0x10,
			 ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
			 ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};

	code[2] = id;

	l = strlen(line);
	if (l > 16)
		l = 16;
	memcpy(code + 4, line, l);

	serial_write(code, 20);
	return 0;
}

static int a125_line0(int argc, const char **argv)
{
	if (argc < 0 || argc > 1)
		return -1;

	return a125_line(0, (argc > 0) ? argv[0] : "");
}

static int a125_line1(int argc, const char **argv)
{
	if (argc < 0 || argc > 1)
		return -1;

	return a125_line(1, (argc > 0) ? argv[0] : "");
}

static int a125_reset(int argc, const char **argv)
{
	char code[3] = { 0x4D, 0xFF };

	if (argc != 0)
		return -1;

	serial_write(code, 2);
	return 0;
}

static int a125_clear(int argc, const char **argv)
{
	char code[3] = { 0x4D, 0x0D };

	if (argc != 0)
		return -1;

	serial_write(code, 2);
	return 0;
}

static int a125_init(int argc, const char **argv)
{
	int err;
	const char *devicename;

	if (argc > 1) {
		printf("%s: module takes at most one argument\n", __func__);
		return -1;
	}
	if (argc > 0 )
		devicename = argv[0];
	else
		devicename = "/dev/ttyS0";
	err = serial_open(devicename);
	if (err < 0) {
		fprintf(stderr, "Error opening '%s'!\n", devicename);
		return err;
	}

	err = register_command("lcd-reset",
	                       "Reset the LCD",
	                       "Reset the LCD\n",
	                       a125_reset);
	err = register_command("lcd-backlight",
	                       "Set the LCD backlight",
	                       "Set the LCD backlight, options are:\n"
	                       "\ton\n\toff\n",
	                       a125_backlight);
	err = register_command("lcd-clear",
	                       "Clear the LCD",
	                       "Clean the LCD\n",
	                       a125_clear);
	err = register_command("lcd-line0",
	                       "Set LCD line 0",
	                       "Set LCD line 0",
	                       a125_line0);
	err = register_command("lcd-line1",
	                       "Set LCD line 1",
	                       "Set LCD line 1",
	                       a125_line1);
	return pthread_create(&a125_thread, NULL, serial_poll, NULL);
}

static void a125_exit(void)
{
	serial_close();
}

struct picmodule a125_module = {
	.name           = "a125",
	.init           = a125_init,
	.exit           = a125_exit,
};
