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

#include "picmodule.h"

static int system_status(int argc, const char **argv)
{
	call_function("system_status", "%s", argv[0]);
	return 0;
}

static int system_init(int argc, const char **argv UNUSED)
{
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
