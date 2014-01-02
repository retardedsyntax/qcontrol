/*
 * Copyright (C) 2007-2008  Byron Bradley (byron.bbradley@gmail.com)
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

static int ts209_read_serial_events(void)
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
		call_function("fan_error", "");
		break;
	case QNAP_PICSTS_FAN1_NORMAL:
		call_function("fan_normal", "");
		break;
	case QNAP_PICSTS_TEMP_WARM_TO_HOT:
	case QNAP_PICSTS_TEMP_COLD_TO_WARM:
		call_function("temp_low", "");
		break;
	case QNAP_PICSTS_TEMP_HOT_TO_WARM:
	case QNAP_PICSTS_TEMP_WARM_TO_COLD:
		call_function("temp_high", "");
		break;
	default:
		print_log(LOG_WARNING, "(PIC 0x%x) unknown command from PIC",
			  buf[0]);
	}

	return -1;
}

static int ts209_init(int argc, const char **argv UNUSED)
{
	int err;

	if (argc > 0) {
		print_log(LOG_ERR, "ts209: module takes no arguments");
		return -1;
	}

	err = qnap_serial_open("/dev/ttyS1");
	if (err < 0)
		return err;

	err = qnap_register_commands(QNAP_PIC_FEATURE_POWERLED|
				     QNAP_PIC_FEATURE_STATUSLED|
				     QNAP_PIC_FEATURE_USBLED|
				     QNAP_PIC_FEATURE_AUTOPOWER|
				     QNAP_PIC_FEATURE_BUZZER|
				     QNAP_PIC_FEATURE_FANSPEED);
	if (err < 0)
		return err;

	return qnap_serial_poll(&ts209_read_serial_events);
}

static void ts209_exit(void)
{
	qnap_serial_close();
}

struct picmodule ts209_module = {
	.name           = "ts209",
	.init           = ts209_init,
	.exit           = ts209_exit,
};
