/*
 * Copyright (C) 2007-2008  Byron Bradley (byron.bbradley@gmail.com)
 * Copyright (C) 2008, 2009  Martin Michlmayr (tbm@cyrius.com)
 * Copyright (C) 2013  Michael Stapelberg (michael+qnap@stapelberg.de)
 * Copyright (C) 2013  Ian Campbell (ijc@hellion.org.uk)
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
#include <pthread.h>
#include <termios.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <sys/select.h>

#include "picmodule.h"
#include "qnap-pic.h"

static int serial;
static struct termios oldtio, newtio;
static pthread_t qnap_thread;

int qnap_serial_read(unsigned char *buf, int len)
{
	int err;
	static int error_count = 0;

	err = read(serial, buf, len);
	if (err != -1) {
		buf[err] = 0;
		error_count = 0;
	} else if (errno == EAGAIN) {
		error_count++;
		if (error_count == 5)
			print_log(LOG_WARNING,
"Contradicting information about data available to be read from /dev/ttyS1.\n"
"Please make sure nothing else is reading things there.");
	}

	return err;
}

int qnap_serial_write(unsigned char *buf, int len)
{
	int err;

	err = write(serial, buf, len);

	return err;
}

static void *serial_poll(void *data)
{
	qnap_serial_cb cb = data;
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
		cb();
		FD_SET(serial, &rset);
	}

	return NULL;
}

int qnap_serial_poll(qnap_serial_cb cb)
{
	return pthread_create(&qnap_thread, NULL, serial_poll, cb);
}

static int set_nonblock(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0)
		flags = 0;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int qnap_serial_open(char *device)
{
	int err;

	if ((serial = open(device , O_RDWR)) < 0) {
		print_log(LOG_ERR, "Failed to open %s: %s", device,
			  strerror(errno));
		return -1;
	}
	err = set_nonblock(serial);
	if (err < 0) {
		print_log(LOG_ERR, "Error setting nonblock: %s",
		          strerror(errno));
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
		print_log(LOG_ERR, "Failed to set attributes for %s: %s",
		          device, strerror(errno));
		return -1;
	}

	return 0;
}

void qnap_serial_close(void)
{
	tcsetattr(serial, TCSANOW, &oldtio);
	close(serial);
}

