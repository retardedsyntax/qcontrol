/*
 * Copyright (C) 2007-2008  Byron Bradley (byron.bbradley@gmail.com)
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

#ifndef _PICMODULE_H_
#define _PICMODULE_H_

struct picmodule {
	char *name;
	int (*init)(int, const char**);
	void (*exit)(void);
};

int get_args(int *argc, const char ***argv);
int register_command(const char *cmd, const char *shorthelp, const char *help,
                     int (*call)(int argc, const char **argv));
int call_function(const char *fname, const char *fmt, ...);

#endif /* Not _PICMODULE_H_ */
