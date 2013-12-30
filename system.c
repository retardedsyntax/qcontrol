/*
 * Copyright (C) 2013 Ian Campbell <ijc@hellion.org.uk>
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

#include <syslog.h>
#include "picmodule.h"

static int system_status(int argc, const char **argv)
{
	if (argc != 0)
		return -1;
	
	call_function("system_status", "%s", argv[0]);
	return 0;
}

static int system_init(int argc, const char **argv UNUSED)
{
	if (argc > 0) {
		print_log(LOG_ERR, "%s: module does not take any arguments\n",
			  __func__);
		return -1;
	}

	register_command("system-status",
			 "Signal system status",
			 "",
			 system_status);
	return 0;
}

struct picmodule system_module = {
	.name           = "system-status",
	.init           = system_init,
};
