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

static int qnap_cmd_powerled(int argc, const char **argv)
{
	unsigned char code = 0;

	if (argc != 1)
		return -1;

	if (strcmp(argv[0], "on") == 0)
		code = QNAP_PICCMD_POWER_LED_ON;
	else if (strcmp(argv[0], "1hz") == 0)
		code = QNAP_PICCMD_POWER_LED_1HZ;
	else if (strcmp(argv[0], "2hz") == 0)
		code = QNAP_PICCMD_POWER_LED_2HZ;
	else if (strcmp(argv[0], "off") == 0)
		code = QNAP_PICCMD_POWER_LED_OFF;
	else
		return -1;

	return qnap_serial_write(&code, 1);
}

static int qnap_cmd_statusled(int argc, const char **argv)
{
	unsigned char code = 0;

	if (argc != 1)
		return -1;

	if (strcmp(argv[0], "red2hz") == 0)
		code = QNAP_PICCMD_STATUS_RED_2HZ;
	else if (strcmp(argv[0], "green2hz") == 0)
		code = QNAP_PICCMD_STATUS_GREEN_2HZ;
	else if (strcmp(argv[0], "greenon") == 0)
		code = QNAP_PICCMD_STATUS_GREEN_ON;
	else if (strcmp(argv[0], "redon") == 0)
		code = QNAP_PICCMD_STATUS_RED_ON;
	else if (strcmp(argv[0], "greenred2hz") == 0)
		code = QNAP_PICCMD_STATUS_BOTH_2HZ;
	else if (strcmp(argv[0], "off") == 0)
		code = QNAP_PICCMD_STATUS_OFF;
	else if (strcmp(argv[0], "green1hz") == 0)
		code = QNAP_PICCMD_STATUS_GREEN_1HZ;
	else if (strcmp(argv[0], "red1hz") == 0)
		code = QNAP_PICCMD_STATUS_RED_1HZ;
	else if (strcmp(argv[0], "greenred1hz") == 0)
		code = QNAP_PICCMD_STATUS_BOTH_1HZ;
	else
		return -1;

	return qnap_serial_write(&code, 1);
}

static int qnap_cmd_usbled(int argc, const char **argv)
{
	unsigned char code = 0;

	if (argc != 1)
		return -1;

	if (strcmp(argv[0], "on") == 0)
		code = QNAP_PICCMD_USB_LED_ON;
	else if (strcmp(argv[0], "8hz") == 0)
		code = QNAP_PICCMD_USB_LED_8HZ;
	else if (strcmp(argv[0], "off") == 0)
		code = QNAP_PICCMD_USB_LED_OFF;
	else
		return -1;

	return qnap_serial_write(&code, 1);
}

static int qnap_cmd_autopower(int argc, const char **argv)
{
	unsigned char code = 0;

	if (argc != 1)
		return -1;

	if (strcmp(argv[0], "on") == 0)
		code = QNAP_PICCMD_AUTOPOWER_ON;
	else if (strcmp(argv[0], "off") == 0)
		code = QNAP_PICCMD_AUTOPOWER_OFF;
	else
		return -1;

	return qnap_serial_write(&code, 1);
}

static int qnap_cmd_buzzer(int argc, const char **argv)
{
	unsigned char code = 0;

	if (argc != 1)
		return -1;

	if (strcmp(argv[0], "short") == 0)
		code = QNAP_PICCMD_BUZZER_SHORT;
	else if (strcmp(argv[0], "long") == 0)
		code = QNAP_PICCMD_BUZZER_LONG;
	else
		return -1;

	return qnap_serial_write(&code, 1);
}

static int qnap_cmd_fanspeed(int argc, const char **argv)
{
	unsigned char code = 0;

	if (argc != 1)
		return -1;

	if (strcmp(argv[0], "stop") == 0)
		code = QNAP_PICCMD_FAN_STOP;
	else if (strcmp(argv[0], "silence") == 0)
		code = QNAP_PICCMD_FAN_SILENCE;
	else if (strcmp(argv[0], "low") == 0)
		code = QNAP_PICCMD_FAN_LOW;
	else if (strcmp(argv[0], "medium") == 0)
		code = QNAP_PICCMD_FAN_MEDIUM;
	else if (strcmp(argv[0], "high") == 0)
		code = QNAP_PICCMD_FAN_HIGH;
	else if (strcmp(argv[0], "full") == 0)
		code = QNAP_PICCMD_FAN_FULL;
	else
		return -1;

	return qnap_serial_write(&code, 1);
}

static int qnap_cmd_watchdog(int argc, const char **argv)
{
	unsigned char code = 0;

	if (argc != 1)
		return -1;
	if (strcmp(argv[0], "off") == 0)
		code = QNAP_PICCMD_WDT_OFF;
	else
		return -1;

	return qnap_serial_write(&code, 1);
}

static int qnap_cmd_eup(int argc, const char **argv)
{
	unsigned char code = 0;

	if (argc != 1)
		return -1;

	if (strcmp(argv[0], "on") == 0)
		code = QNAP_PICCMD_EUP_ENABLE;
	else if (strcmp(argv[0], "off") == 0)
		code = QNAP_PICCMD_EUP_DISABLE;
	else
		return -1;

	return qnap_serial_write(&code, 1);
}

static int qnap_cmd_wol(int argc, const char **argv)
{
	unsigned char code[2];
	int len;

	if (argc != 1)
		return -1;

	if (strcmp(argv[0], "on") == 0) {
		/* EUP turns the device in such a deep power-saving
		 * mode that WOL does not work. Therefore, in order
		 * to have WOL turned on, we also disable EUP. */
		code[0] = QNAP_PICCMD_WOL_ENABLE;
		code[1] = QNAP_PICCMD_EUP_DISABLE;
		len = 2;
	} else if (strcmp(argv[0], "off") == 0) {
		code[0] = QNAP_PICCMD_WOL_DISABLE;
		len = 1;
	} else
		return -1;

	return qnap_serial_write(code, len);
}


int qnap_register_commands(unsigned long features)
{
#define REGISTER1(feat,name,shorthelp,longhelp) do { \
	if (features & QNAP_PIC_FEATURE_##feat) { \
		int err = register_command(#name, shorthelp, longhelp, qnap_cmd_##name); \
		if (err < 0) return err; \
	} \
} while(0)

	REGISTER1(POWERLED, powerled, "Change the power LED",
		  "Change the power LED, options are:\n"
		  "\ton\n\toff\n\t1hz\n\t2hz\n");
	REGISTER1(STATUSLED, statusled, "Change the status LED",
		  "Change the status LED, options are:\n"
		  "\tred2hz\n\tgreen2hz\n\tgreenon\n\tredon\n"
		  "\tgreenred2hz\n\toff\n\tgreen1hz\n\tred1hz\n");
	REGISTER1(USBLED, usbled, "Set the usbled",
		  "Set the usbled, options are:\n"
		  "\ton\n\t8hz\n\toff\n");
	REGISTER1(AUTOPOWER, autopower, "Control the automatic power mechanism",
		  "Control the automatic power mechanism, options are:\n"
		  "\ton\n\toff\n");
	REGISTER1(BUZZER, buzzer, "Buzz",
		  "Buzz, options are:\n"
		  "\tshort\n\tlong\n");
	REGISTER1(FANSPEED, fanspeed, "Set the fanspeed",
		  "Set the fanspeed, options are:\n"
		  "\tstop\n\tsilence\n\tlow\n\tmedium\n"
		  "\thigh\n\tfull\n");
	REGISTER1(WATCHDOG, watchdog, "Disable the PIC watchdog",
		  "Watchdog options are:\n"
		  "\toff");
	REGISTER1(WOL, wol, "Control Wake on LAN",
		 "Control Wake on LAN, options are:\n"
		  "\ton\n\toff");
	REGISTER1(EUP, eup, "Control EUP (Energy-using Products) power saving",
		  "Control EUP, options are:\n"
		  "\ton\n\toff");

#undef REGISTER1
	return 0;
}

