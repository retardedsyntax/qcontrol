/*
 * Copyright (C) 2007-2008  Byron Bradley (byron.bbradley@gmail.com)
 * Copyright (C) 2008, 2009  Martin Michlmayr (tbm@cyrius.com)
 * Copyright (C) 2013  Michael Stapelberg (michael+qnap@stapelberg.de)
 * Copyright (C) 2013  Ian Campbell (ijc@hellion.org.uk)
 * Copyright (C) 2014  Ben Peddell (klightspeed@killerwolves.net)
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
#include "synology.h"

static int serial;
static struct termios oldtio, newtio;
static pthread_t synology_thread;

static int synology_serial_read(unsigned char *buf, int len)
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

static int synology_serial_write(unsigned char *buf, int len)
{
	int err;

	err = write(serial, buf, len);

	return err;
}

static int synology_read_serial_events(void)
{
	unsigned char buf[100];
	int err = synology_serial_read(buf, 100);
	if (err < 0)
		return err;
	switch (buf[0]) {
	case SYNOLOGY_PICSTS_POWER_BUTTON:
		call_function("power_button", "%d", 3);
		break;
	case SYNOLOGY_PICSTS_MEDIA_BUTTON:
		call_function("media_button", "%d", 3);
		break;
	case SYNOLOGY_PICSTS_RESET_BUTTON:
		call_function("restart_button", "%d", 3);
		break;
	default:
		print_log(LOG_WARNING, "(PIC 0x%x) unknown command from PIC",
			  buf[0]);
	}

	return -1;
}

static void *serial_poll(void *data UNUSED)
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
		synology_read_serial_events();
		FD_SET(serial, &rset);
	}

	return NULL;
}

static int synology_serial_poll(void)
{
	return pthread_create(&synology_thread, NULL, serial_poll, NULL);
}

static int set_nonblock(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0)
		flags = 0;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int synology_serial_open(char *device)
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
	newtio.c_cflag = B9600 | CS8 | CLOCAL | CREAD;
	newtio.c_cc[VMIN] = 1;
	newtio.c_cc[VTIME] = 0;
	cfsetospeed(&newtio, B9600);
	cfsetispeed(&newtio, B9600);

	err = tcsetattr(serial, TCSANOW, &newtio);
	if (err < 0) {
		print_log(LOG_ERR, "Failed to set attributes for %s: %s",
		          device, strerror(errno));
		return -1;
	}

	return 0;
}

static void synology_serial_close(void)
{
	tcsetattr(serial, TCSANOW, &oldtio);
	close(serial);
}

static int synology_cmd_powerled(int argc, const char **argv)
{
	unsigned char code = 0;

	if (argc != 1)
		return -1;

	if (strcmp(argv[0], "on") == 0)
		code = SYNOLOGY_PICCMD_POWER_LED_ON;
	else if (strcmp(argv[0], "2hz") == 0)
		code = SYNOLOGY_PICCMD_POWER_LED_2HZ;
	else if (strcmp(argv[0], "off") == 0)
		code = SYNOLOGY_PICCMD_POWER_LED_OFF;
	else
		return -1;

	return synology_serial_write(&code, 1);
}

static int synology_cmd_statusled(int argc, const char **argv)
{
	unsigned char code = 0;

	if (argc != 1)
		return -1;

	if (strcmp(argv[0], "orange2hz") == 0)
		code = SYNOLOGY_PICCMD_STATUS_ORANGE_2HZ;
	else if (strcmp(argv[0], "green2hz") == 0)
		code = SYNOLOGY_PICCMD_STATUS_GREEN_2HZ;
	else if (strcmp(argv[0], "orangeon") == 0)
		code = SYNOLOGY_PICCMD_STATUS_ORANGE_ON;
	else if (strcmp(argv[0], "greenon") == 0)
		code = SYNOLOGY_PICCMD_STATUS_GREEN_ON;
	else if (strcmp(argv[0], "off") == 0)
		code = SYNOLOGY_PICCMD_STATUS_OFF;
	else
		return -1;

	return synology_serial_write(&code, 1);
}

static int synology_cmd_usbled(int argc, const char **argv)
{
	unsigned char code = 0;

	if (argc != 1)
		return -1;

	if (strcmp(argv[0], "on") == 0)
		code = SYNOLOGY_PICCMD_USB_LED_ON;
	else if (strcmp(argv[0], "2hz") == 0)
		code = SYNOLOGY_PICCMD_USB_LED_2HZ;
	else if (strcmp(argv[0], "off") == 0)
		code = SYNOLOGY_PICCMD_USB_LED_OFF;
	else
		return -1;

	return synology_serial_write(&code, 1);
}

static int synology_cmd_autopower(int argc, const char **argv)
{
	unsigned char code = 0;

	if (argc != 1)
		return -1;

	if (strcmp(argv[0], "on") == 0)
		code = SYNOLOGY_PICCMD_AUTOPOWER_ON;
	else if (strcmp(argv[0], "off") == 0)
		code = SYNOLOGY_PICCMD_AUTOPOWER_OFF;
	else
		return -1;

	return synology_serial_write(&code, 1);
}

static int synology_cmd_buzzer(int argc, const char **argv)
{
	unsigned char code = 0;

	if (argc != 1)
		return -1;

	if (strcmp(argv[0], "short") == 0)
		code = SYNOLOGY_PICCMD_BUZZER_SHORT;
	else if (strcmp(argv[0], "long") == 0)
		code = SYNOLOGY_PICCMD_BUZZER_LONG;
	else
		return -1;

	return synology_serial_write(&code, 1);
}

static int synology_cmd_rtc(int argc, const char **argv)
{
	unsigned char code = 0;

	if (argc != 1)
		return -1;

	if (strcmp(argv[0], "on") == 0)
		code = SYNOLOGY_PICCMD_RTC_ENABLE;
	else if (strcmp(argv[0], "off") == 0)
		code = SYNOLOGY_PICCMD_RTC_DISABLE;
	else
		return -1;

	return synology_serial_write(&code, 1);
}

static int synology_init(int argc, const char **argv UNUSED)
{
	int err;

	if (argc > 0) {
		print_log(LOG_ERR, "synology: module takes no arguments");
		return -1;
	}

	err = synology_serial_open("/dev/ttyS1");
	if (err < 0)
		return err;

#define REGISTER1(name,shorthelp,longhelp) do { \
	int err = register_command(#name, shorthelp, longhelp, synology_cmd_##name); \
	if (err < 0) return err; \
} while(0)

	REGISTER1(powerled, "Change the power LED",
		  "Change the power LED, options are:\n"
		  "\ton\n\toff\n\t2hz\n");
	REGISTER1(statusled, "Change the status LED",
		  "Change the status LED, options are:\n"
		  "\torange2hz\n\tgreen2hz\n"
		  "\torangeon\n\tgreenon\n\toff\n");
	REGISTER1(usbled, "Set the usbled",
		  "Set the usbled, options are:\n"
		  "\ton\n\t2hz\n\toff\n");
	REGISTER1(autopower, "Control the automatic power mechanism",
		  "Control the automatic power mechanism, options are:\n"
		  "\ton\n\toff\n");
	REGISTER1(buzzer, "Buzz",
		  "Buzz, options are:\n"
		  "\tshort\n\tlong\n");
	REGISTER1(rtc, "Control RTC (real time clock)",
		  "Control RTC, options are:\n"
		  "\ton\n\toff");

#undef REGISTER1
	return synology_serial_poll();
}

static void synology_exit(void)
{
	synology_serial_close();
}

struct picmodule synology_module = {
	.name		= "synology",
	.init		= synology_init,
	.exit		= synology_exit,
};
