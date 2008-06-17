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

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "picmodule.h"

#define PIC_SOCKET	"/var/run/qcontrol.sock"
#define MAX_NET_BUF	1000
#define MALLOC_SIZE	100
#define MAX_CMD_NAME	16

struct piccommand {
	const char *name;
	const char *shorthelp;
	const char *help;
	int (*call)(int argc, const char **argv);
};

char *version = "qcontrol 0.4.1\n";
char *usage = "Usage: qcontrol [OPTION...] [command] [args...]\n"
              "PIC Controller\n\n"
              "  -d, --daemon                Run as a daemon\n"
              "  -?, --help                 Give this help list\n"
              "  -V, --version              Print program version\n\n"
              "Mandatory or optional arguments to long options are also "
              "mandatory or optional\nfor any corresponding short options.\n\n"
              "Report bugs to <byron.bbradley@gmail.com>.\n";
static lua_State *lua;
unsigned int commandcount;
struct piccommand **commands;

extern struct picmodule ts209_module;
extern struct picmodule evdev_module;

struct picmodule *modules[] = {
	&ts209_module,
	&evdev_module,
	NULL
};

/**
 * Calls a function in the lua config file
 */
int call_function(const char *fname, const char *fmt, ...)
{
	int i;
	va_list s;

	lua_getglobal(lua, fname);

	va_start(s, fmt);
	for (i = 0; fmt[i]; ) {
		if (fmt[i++] != '%')
			return -1;
		switch (fmt[i]) {
		case 'd':
			lua_pushinteger(lua, va_arg(s, int));
			break;
		case 's':
			lua_pushstring(lua, va_arg(s, char*));
			break;
		default:
			fprintf(stderr, "Unrecognised format\n");
			return -1;
		}
		++i;
	}
	va_end(s);

	lua_call(lua, i / 2, 0);

	return 0;
}

/**
 * Return an error to lua
 */
void return_error(const char *error)
{
	lua_pushstring(lua, error);
	lua_error(lua);
}

/**
 * Get the arguments of a function call from lua
 */
int get_args(int *argc, const char ***argv)
{
	int i;

	*argc = lua_gettop(lua);
	*argv = (const char **) calloc(*argc, sizeof(char*));
	for (i = 1; i <= *argc; ++i) {
		*(*argv + i - 1) = (const char*) lua_tostring(lua, i);
	}
	return 0;
}

int register_module()
{
	int i, argc, err;
	const char **argv;

	err = get_args(&argc, &argv);
	if (err < 0)
		return_error("Error getting arguments");

	for (i = 0; modules[i]; ++i) {
		if (strcmp(argv[0], modules[i]->name) != 0)
			continue;
		err = modules[i]->init(argc - 1, argv + 1);
		break;
	}
	if (err < 0)
		return_error("Error loading module");
	return err;
}

int register_command(const char *cmd, const char *shorthelp, const char *help,
                     int (*call)(int argc, const char **argv))
{
	struct piccommand *c = malloc(sizeof(struct piccommand));

	if (!c || strlen(cmd) > MAX_CMD_NAME) {
		free(c);
		return -1;
	}
	c->name = cmd;
	c->shorthelp = shorthelp;
	c->help = help;
	c->call = call;

	if (commandcount == 0)
		commands = malloc(++commandcount * sizeof(struct piccommand*));
	else
		commands = realloc(commands,
		                   ++commandcount * sizeof(struct piccommand*));

	commands[commandcount - 1] = c;

	return 0;
}

void shorthelp_commands(char **buf, int *len)
{
	int i, off=0;

	*len = 0;
	for (i = 0; i < commandcount; ++i)
		*len += strlen(commands[i]->shorthelp) + MAX_CMD_NAME
		        + sizeof("\n") + sizeof(int);

	*buf = malloc(*len);
	memset(*buf, 0, sizeof(int));
	off += sizeof(int);
	for (i = 0; i < commandcount; ++i) {
		strncpy(*buf+off, commands[i]->name, MAX_CMD_NAME);
		off += strlen(commands[i]->name);
		memset(*buf+off, ' ', MAX_CMD_NAME - strlen(commands[i]->name));
		off += MAX_CMD_NAME - strlen(commands[i]->name);
		strncpy(*buf+off, commands[i]->shorthelp, *len-off);
		off += strlen(commands[i]->shorthelp);
		strncpy(*buf+off, "\n", *len-off);
		off += strlen("\n");
	}
}

int run_command(const char *cmd, int argc, const char **argv)
{
	int i;

	for (i = 0; i < commandcount; ++i)
		if (strcmp(cmd, commands[i]->name) == 0)
			return commands[i]->call(argc, argv);

	return -1;
}

int run_command_lua()
{
	int argc, err;
	const char **argv;

	err = get_args(&argc, &argv);
	if (err < 0)
		return_error("Error getting arguments");

	return run_command(argv[0], argc-1, argv+1);
}

const char *help_command(const char *cmd)
{
	int i;

	for (i = 0; i < commandcount; ++i)
		if (strcmp(cmd, commands[i]->name) == 0)
			return commands[i]->help;

	return "Command not found\n";
}

static int pic_lua_setup()
{
	lua = lua_open();
	luaL_openlibs(lua);

	lua_register(lua, "register", register_module);
	lua_register(lua, "piccmd", run_command_lua);

	luaL_dofile(lua, "/etc/qcontrol.conf");

	return 0;
}

int write_uint32(uint32_t i, char **buf, int off,  unsigned int *maxlen)
{
	int ilen = sizeof(uint32_t);

	if (off + ilen > *maxlen) {
		*maxlen += ilen + MALLOC_SIZE;
		*buf = realloc(*buf, *maxlen);
		if (!buf)
			return -1;
	}
	memcpy((*buf)+off, &i, ilen);
	return off + ilen;
}

int read_uint32(uint32_t *i, char *buf, int off)
{
	memcpy(i, buf+off, sizeof(uint32_t));
	return off + sizeof(uint32_t);
}

int write_string(const char *str, char **buf, int off, unsigned int *maxlen)
{
	int slen = strlen(str) + 1;

	if (off + sizeof(int) + slen > *maxlen) {
		*maxlen += slen + MALLOC_SIZE;
		*buf = realloc(*buf, *maxlen);
		if (!buf)
			return -1;
	}
	off = write_uint32(slen, buf, off, maxlen);
	memcpy((*buf)+off, str, slen);
	return off+slen;
}

int read_string(char **str, char *buf, int off)
{
	uint32_t len;

	off = read_uint32(&len, buf, off);
	*str = malloc(len);
	memcpy(*str, buf+off, len);
	return off+len;
}

int network_send(int argc, char **argv)
{
	int sock, err, i, off=0, rlen;
	struct sockaddr_un remote;
	unsigned int maxlen = MALLOC_SIZE;
	char *buf, retbuf[MAX_NET_BUF], *errstr;

	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("Error opening socket");
		return -1;
	}

	remote.sun_family = AF_UNIX;
	strcpy(remote.sun_path, PIC_SOCKET);
	err = connect(sock, (struct sockaddr*)&remote,
					sizeof(struct sockaddr_un));
	if (err < 0) {
		perror("Error connecting to socket");
		return -1;
	}

	buf = malloc(maxlen);
	off = write_uint32(argc, &buf, off, &maxlen);
	for (i = 0; i < argc; ++i) {
		off = write_string(argv[i], &buf, off, &maxlen);
	}

	send(sock, buf, off, 0);
	rlen = read(sock, retbuf, MAX_NET_BUF);
	if (rlen < 0) {
		perror("Error during read");
		return -1;
	}

	off = 0;
	off = read_uint32((uint32_t*)&err, retbuf, off);
	if (err < 0) {
		off = read_string(&errstr, retbuf, off);
		printf("%s\n", errstr);
	} else if (err == 0 && rlen > sizeof(int)) {
		printf("\nAvailable commands are:\n%s\n", retbuf + sizeof(int));
		err = 0;
	}

	close(sock);

	return err;
}

static int network_listen()
{
	int sock, err, con, argc, off=0, i;
	char **argv;
	socklen_t remotelen;
	char buf[MAX_NET_BUF], *rbuf;
	int maxlen = MALLOC_SIZE;
	struct sockaddr_un name, remote;

	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("Error creating socket:");
		return -1;
	}

	name.sun_family = AF_UNIX;
	strcpy(name.sun_path, PIC_SOCKET);
	err = bind(sock, (struct sockaddr*) &name, sizeof(struct sockaddr_un));
	if (err != 0) {
		perror("Error binding to socket");
		return -1;
	}

	err = listen(sock, 10);
	if (err < 0) {
		perror("Error listening on socket");
		return -1;
	}

	remotelen = sizeof(remote);
	for (;;) {
		con = accept(sock, (struct sockaddr*)&remote, &remotelen);
		if (con < 0) {
			perror("Error accepting connection");
			break;
		}
		err = read(con, buf, MAX_NET_BUF);
		if (err < 0) {
			perror("Error during read");
			break;
		}

		/* Process the arguments */
		off = read_uint32((uint32_t*)&argc, buf, off);
		argv = malloc(argc * sizeof(char*));
		if (!argv) {
			perror("read failed");
			return -1;
		}
		for (i = 0; i < argc; ++i)
			off = read_string(&argv[i], buf, off);
		off = 0;

		if (argc > 0 && (strcmp(argv[0], "--help") == 0
		               ||strcmp(argv[0], "-h") == 0)) {
			/* Return the short helps */
			shorthelp_commands(&rbuf, &off);
		} else {
			/* Run the command */
			err = run_command(argv[0], argc-1, (const char**)argv+1);
			rbuf = malloc(maxlen);
			off = write_uint32(err, &rbuf, off, (uint32_t*)&maxlen);
			if (err < 0)
				off = write_string(help_command(argv[0]), &rbuf,
				                   off, (uint32_t*)&maxlen);
		}
		send(con, rbuf, off, 0);
		off = 0;


		for (i = 0; i < argc; ++i) {
			free(argv[i]);
			argv[i] = NULL;
		}
		free(argv);
		argv = NULL;
		free(rbuf);
		rbuf = NULL;
		close(con);
	}

	close(sock);
	unlink(PIC_SOCKET);

	return 0;
}

int main(int argc, char *argv[])
{
	commandcount = 0;

	if (argc > 1 && (strcmp(argv[1], "--help") == 0
	              || strcmp(argv[1], "-?") == 0)) {
		printf("%s", usage);
		return network_send(argc - 1, argv + 1);
	} else if (argc > 1 && (strcmp(argv[1], "-V") == 0
	                     || strcmp(argv[1], "--version") == 0)) {
		printf("%s", version);
		return 0;
	} else if (argc > 1 && strcmp(argv[1], "-d") == 0) {
		/* Startup in daemon mode */
		pic_lua_setup(&lua);
		return network_listen();
	} else if (argc > 1) {
		/* Send the command to the server */
		return network_send(argc - 1, argv + 1);
	} else {
		printf("Usage\n");
	}

	return -1;
}