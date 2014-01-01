/*
 * Copyright (C) 2007-2008  Byron Bradley (byron.bbradley@gmail.com)
 * Copyright (C) 2008  Martin Michlmayr (tbm@cyrius.com)
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

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <string.h>
#include <syslog.h>
#include <linux/input.h>

#include "picmodule.h"
#include "qnap-pic.h"

static int ts409_read_serial_events(void)
{
	unsigned char buf[100];
	int err = qnap_serial_read(buf, 100);
	if (err < 0)
		return err;
	switch (buf[0]) {
	case QNAP_PICSTS_POWER_BUTTON:
		call_function("power_button", "%d", 3);
		break;
	case QNAP_PICSTS_POWER_LOSS_POWER_OFF:
		/* RTC Wake-Up (ignored) */
		break;
	case QNAP_PICSTS_FAN1_ERROR:
	case QNAP_PICSTS_FAN2_ERROR:
	case QNAP_PICSTS_FAN3_ERROR:
	case QNAP_PICSTS_FAN4_ERROR:
		call_function("fan_error", "");
		break;
	case QNAP_PICSTS_FAN1_NORMAL:
	case QNAP_PICSTS_FAN2_NORMAL:
	case QNAP_PICSTS_FAN3_NORMAL:
	case QNAP_PICSTS_FAN4_NORMAL:
		call_function("fan_normal", "");
		break;
	case QNAP_PICSTS_SYS_TEMP_0 ... QNAP_PICSTS_SYS_TEMP_70:
		call_function("temp", "%d", buf[0] - QNAP_PICSTS_SYS_TEMP_0);
		break;
	case QNAP_PICSTS_SYS_TEMP_71_79:
		call_function("temp", "%d", 75);
		break;
	case QNAP_PICSTS_SYS_TEMP_80:
		call_function("temp", "%d", 80);
		break;
	default:
		print_log(LOG_WARNING, "(PIC 0x%x) unknown command from PIC",
			  buf[0]);
	}

	return -1;
}

static int ts409_statusled(int argc, const char **argv)
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

static int ts409_buzz(int argc, const char **argv)
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

static int ts409_fanspeed(int argc, const char **argv)
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

static int ts409_usbled(int argc, const char **argv)
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

static int ts409_autopower(int argc, const char **argv)
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

static int ts409_init(int argc, const char **argv UNUSED)
{
	int err;

	if (argc > 0) {
		print_log(LOG_ERR, "ts409: module takes no arguments");
		return -1;
	}

	err = qnap_serial_open("/dev/ttyS1");
	if (err < 0)
		return err;

	err = register_command("statusled",
	                       "Change the status LED",
	                       "Change the status LED, options are:\n"
	                       "\tred2hz\n\tgreen2hz\n\tgreenon\n\tredon\n"
	                       "\tgreenred2hz\n\toff\n\tgreen1hz\n\tred1hz\n",
	                       ts409_statusled);
	err = register_command("buzzer",
	                       "Buzz",
	                       "Buzz, options are:\n"
	                       "\tshort\n\tlong\n",
	                       ts409_buzz);
	err = register_command("fanspeed",
	                       "Set the fanspeed",
	                       "Set the fanspeed, options are:\n"
	                       "\tstop\n\tsilence\n\tlow\n\tmedium\n"
	                       "\thigh\n\tfull\n",
	                       ts409_fanspeed);
	err = register_command("usbled",
	                       "Set the usbled",
	                       "Set the usbled, options are:\n"
	                       "\ton\n\t8hz\n\toff\n",
	                       ts409_usbled);
	err = register_command("autopower",
	                       "Control the automatic power mechanism",
	                       "Control the automatic power mechanism, options are:\n"
	                       "\ton\n\toff\n",
	                       ts409_autopower);

	return qnap_serial_poll(&ts409_read_serial_events);
}

static void ts409_exit(void)
{
	qnap_serial_close();
}

struct picmodule ts409_module = {
	.name           = "ts409",
	.init           = ts409_init,
	.exit           = ts409_exit,
};
