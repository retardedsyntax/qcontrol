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

#include <errno.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <glob.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>

#ifdef HAVE_SYSTEMD
#include <systemd/sd-daemon.h>
#endif

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

bool g_use_syslog = false;
char *version = "qcontrol " QCONTROL_VERSION "\n";
char *usage = "Usage: qcontrol [OPTION...] [command] [args...]\n"
              "PIC Controller\n\n"
              "  -d, --daemon               Run the server as a daemon\n"
              "  -f, --foreground           Run the server in the foreground\n"
              "  -?, --help                 Give this help list\n"
              "  -V, --version              Print program version\n\n"
              "Mandatory or optional arguments to long options are also "
              "mandatory or optional\nfor any corresponding short options.\n";
static const char *configfilename = "/etc/qcontrol.conf";
static lua_State *lua = NULL;
unsigned int commandcount;
struct piccommand **commands;

extern struct picmodule system_module;
extern struct picmodule ts209_module;
extern struct picmodule ts219_module;
extern struct picmodule ts409_module;
extern struct picmodule ts41x_module;
extern struct picmodule a125_module;
extern struct picmodule evdev_module;

struct picmodule *modules[] = {
	&system_module,
	&ts209_module,
	&ts219_module,
	&ts409_module,
	&ts41x_module,
	&a125_module,
	&evdev_module,
	NULL
};

/**
 * Print an error message, either to the console or syslog depending on the
 * value of g_use_syslog
 */
int print_log(int priority, const char *format, ...)
{
	int err = 0;
	va_list ap;

	va_start(ap, format);
	if (g_use_syslog == true) {
		vsyslog(priority, format, ap);
	} else {
		FILE *f = (priority == LOG_ERR) ? stderr : stdout;
		err = vfprintf(f, format, ap);
		fprintf(f, "\n");
		fflush(f);
	}

	va_end(ap);
	return err;
}

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
			print_log(LOG_WARNING, "Unrecognised format");
			return -1;
		}
		++i;
	}
	va_end(s);

	if (lua_pcall(lua, i / 2, 0, 0) == LUA_ERRRUN) {
		print_log(LOG_ERR, "Error calling lua function %s: %s",
		          fname, lua_tostring(lua, -1));
		return -1;
	}

	return 0;
}

/**
 * Return an error to lua
 */
static void return_error(const char *error)
{
	lua_pushstring(lua, error);
	lua_error(lua);
}

/**
 * Load files from a configuration dir.
 */
static int confdir(lua_State *L UNUSED)
{
	static const char gl_match[] = "/*.conf";
	const char *path = lua_tostring(lua, 1);
	glob_t gl;
	char *gl_path;
	size_t p;
	int rc;

	if (!path) {
		print_log(LOG_ERR, "confdir: no path given");
		return -1;
	}

	print_log(LOG_ERR, "confdir: loading from %s...", path);

	gl_path = lua_newuserdata(lua, strlen(path) + sizeof(gl_match));
	if (!gl_path) {
		print_log(LOG_ERR, "confdir: out of memory allocating path");
		return -1;
	}
	strcpy(gl_path, path);
	strcat(gl_path, gl_match);

	rc = glob(gl_path, 0, NULL, &gl);

	switch (rc) {
	case 0: /* Success */ break;
	case GLOB_NOMATCH: /* No matches, this is ok */
		return 0;

	case GLOB_NOSPACE:
		print_log(LOG_ERR, "confdir: out of memory during glob");
		return -1;
	case GLOB_ABORTED:
		print_log(LOG_ERR, "confdir: read error during glob");
		return -1;
	default:
		print_log(LOG_ERR, "confdir: unknown error %d during glob", rc);
		return -1;
	}

	for (p=0; p<gl.gl_pathc; p++) {
		struct stat st_buf;

		path = gl.gl_pathv[p];

		rc = stat(path, &st_buf);
		if (rc < 0) {
			print_log(LOG_ERR, "confdir: stat(%s) failed: %s",
				  path, strerror(errno));
			continue;
		}
		if (!S_ISREG(st_buf.st_mode) && !S_ISLNK(st_buf.st_mode)) {
			print_log(LOG_DEBUG,
				  "confdir: %s not a file or symlink",
				  path);
			continue;
		}
		print_log(LOG_ERR, "confdir: including %s", path);

		rc = luaL_dofile(lua, path);
		if (rc != 0) {
			print_log(LOG_ERR, "%s", lua_tostring(lua, -1));
			lua_pop(lua, 1);
		}
	}

	globfree(&gl);
	return 0;
}

/**
 * Get the arguments of a function call from lua
 */
int get_args(int *argc, const char ***argv)
{
	int i;

	*argc = lua_gettop(lua);
	*argv = (const char **) lua_newuserdata(lua, *argc * sizeof(char*));
	for (i = 1; i <= *argc; ++i) {
		const char *arg = (const char*) lua_tostring(lua, i);
		if (!arg)
			return -1;
		*(*argv + i - 1) = arg;
	}
	return 0;
}

static int register_module(lua_State *L UNUSED)
{
	int i, argc, err;
	const char **argv;

	err = get_args(&argc, &argv);
	if (err < 0) {
		print_log(LOG_ERR, "register() - Error getting arguments");
		return err;
	}

	for (i = 0; modules[i]; ++i) {
		if (strcmp(argv[0], modules[i]->name) != 0)
			continue;
		err = modules[i]->init(argc - 1, argv + 1);
		break;
	}
	if (err < 0)
		return_error("register() - Error loading module");
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

static int shorthelp_fmt(char *buf, size_t size, struct piccommand *c)
{
	return snprintf(buf, size, "%-16s %s\n", c->name, c->shorthelp);
}

static void shorthelp_commands(char **buf, int *len)
{
	unsigned int i, off=0;

	*len = sizeof(int) + 1; /* Length field + NUL terminator */
	for (i = 0; i < commandcount; ++i)
		*len += shorthelp_fmt(NULL, 0, commands[i]);

	*buf = malloc(*len);
	memset(*buf, 0, sizeof(int));
	off += sizeof(int); /* Skip length */
	for (i = 0; i < commandcount; ++i)
		off += shorthelp_fmt(*buf+off, *len-off, commands[i]);
}

static int run_command(const char *cmd, int argc, const char **argv)
{
	unsigned int i;

	for (i = 0; i < commandcount; ++i)
		if (strcmp(cmd, commands[i]->name) == 0)
			return commands[i]->call(argc, argv);

	return -1;
}

static int run_command_lua(lua_State *L UNUSED)
{
	int argc, err;
	const char **argv;

	err = get_args(&argc, &argv);
	if (err < 0) {
		return_error("piccmd() - Error getting arguments");
		return -1;
	}

	return run_command(argv[0], argc-1, argv+1);
}

static int run_command_direct(const char *cmd, int argc, const char **argv)
{
	int err = run_command(cmd, argc, argv);

	if (err < 0)
		return -1;

	return 0;
}

static int script_print(lua_State *L UNUSED)
{
	int argc, err;
	const char **argv;

	err = get_args(&argc, &argv);
	if (err < 0 || argc != 1) {
		return_error("logprint() - Error getting arguments");
		return -1;
	}

	print_log(LOG_NOTICE, "%s", argv[0]);

	return 0;
}

static const char *help_command(const char *cmd)
{
	unsigned int i;

	for (i = 0; i < commandcount; ++i)
		if (strcmp(cmd, commands[i]->name) == 0)
			return commands[i]->help;

	return "Command not found\n";
}

static void pic_lua_close(void)
{
	lua_close(lua);
}

static int pic_lua_setup(lua_State **L UNUSED)
{
	int err;

	lua = lua_open();
	atexit(pic_lua_close);
	luaL_openlibs(lua);

	lua_register(lua, "register", register_module);
	lua_register(lua, "piccmd", run_command_lua);
	lua_register(lua, "logprint", script_print);
	lua_register(lua, "confdir", confdir);

	err = luaL_dofile(lua, configfilename);
	if (err != 0) {
		print_log(LOG_ERR, "%s", lua_tostring(lua, -1));
		lua_pop(lua, 1);
	}

	return err;
}

static int write_uint32(uint32_t i, char **buf, int off,  unsigned int *maxlen)
{
	unsigned int ilen = sizeof(uint32_t);

	if ((unsigned int)off + ilen > *maxlen) {
		*maxlen += ilen + MALLOC_SIZE;
		*buf = realloc(*buf, *maxlen);
		if (!buf)
			return -1;
	}
	memcpy((*buf)+off, &i, ilen);
	return off + ilen;
}

static int read_uint32(uint32_t *i, char *buf, int off)
{
	memcpy(i, buf+off, sizeof(uint32_t));
	return off + sizeof(uint32_t);
}

static int write_string(const char *str, char **buf, int off, unsigned int *maxlen)
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

static int read_string(char **str, char *buf, int off)
{
	uint32_t len;

	off = read_uint32(&len, buf, off);
	*str = malloc(len);
	memcpy(*str, buf+off, len);
	return off+len;
}

static int network_send(int argc, const char **argv)
{
	int sock, err, i, off=0, rlen;
	struct sockaddr_un remote;
	unsigned int maxlen = MALLOC_SIZE;
	char *buf, retbuf[MAX_NET_BUF], *errstr;

	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock < 0) {
		print_log(LOG_ERR, "Error opening socket: %s", strerror(errno));
		return -1;
	}

	remote.sun_family = AF_UNIX;
	strcpy(remote.sun_path, PIC_SOCKET);
	err = connect(sock, (struct sockaddr*)&remote,
					sizeof(struct sockaddr_un));
	if (err < 0) {
		print_log(LOG_ERR, "Error connecting to socket: %s",
		          strerror(errno));
		return -1;
	}

	buf = malloc(maxlen);
	off = write_uint32(argc, &buf, off, &maxlen);
	for (i = 0; i < argc; ++i) {
		off = write_string(argv[i], &buf, off, &maxlen);
	}

	send(sock, buf, off, 0);
	free(buf);
	rlen = read(sock, retbuf, MAX_NET_BUF);
	if (rlen < 0) {
		print_log(LOG_ERR, "Error during read: %s", strerror(errno));
		return -1;
	}

	off = 0;
	off = read_uint32((uint32_t*)&err, retbuf, off);
	if (err < 0) {
		off = read_string(&errstr, retbuf, off);
		print_log(LOG_ERR, "%s", errstr);
	} else if (err == 0 && rlen > (int)sizeof(int)) {
		print_log(LOG_ERR, "\nAvailable commands are:\n%s",
		          retbuf + sizeof(int));
		err = 1;
	} else {
		err = 0;
	}

	close(sock);

	return err;
}

static int open_socket(void)
{
	int sock, err;
	struct sockaddr_un name;

#ifdef HAVE_SYSTEMD
	int fds;
	/* When qcontrol gets socket-activated by systemd, fds is > 0 to indicate
	 * that a socket was inherited which we can just use, i.e. we don’t need to
	 * create and listen() on a new socket on our own. */
	fds = sd_listen_fds(0);
	if (fds < 0) {
		print_log(LOG_ERR, "Error in sd_listen_fds()");
		return -1;
	}
	return SD_LISTEN_FDS_START;
#endif

	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock < 0) {
		print_log(LOG_ERR, "Error creating socket: %s",
		          strerror(errno));
		return -1;
	}

	name.sun_family = AF_UNIX;
	strcpy(name.sun_path, PIC_SOCKET);
	err = bind(sock, (struct sockaddr*) &name, sizeof(struct sockaddr_un));
	if (err != 0) {
		err = connect(sock, (struct sockaddr*)&name,
		              sizeof(struct sockaddr_un));
		if (err < 0) {
			err = unlink(PIC_SOCKET);
			if (err != 0)
				print_log(LOG_ERR, "Error deleting socket: %s",
				          strerror(errno));
			err = bind(sock, (struct sockaddr*) &name,
			           sizeof(struct sockaddr_un));
			if (err != 0)
				print_log(LOG_ERR, "Error creating socket: %s",
				          strerror(errno));
		} else {
			print_log(LOG_ERR, "server already running");
			close(sock);
			return -1;
		}
	}

	err = listen(sock, 10);
	if (err < 0) {
		print_log(LOG_ERR, "Error listening on socket: %s",
		          strerror(errno));
		return -1;
	}

	return sock;
}

static int network_listen(void)
{
	int err, con, argc, off=0, i;
	char **argv;
	socklen_t remotelen;
	char buf[MAX_NET_BUF], *rbuf;
	int maxlen = MALLOC_SIZE;
	struct sockaddr_un remote;

	int sock = open_socket();

	remotelen = sizeof(remote);
	for (;;) {
		con = accept(sock, (struct sockaddr*)&remote, &remotelen);
		if (con < 0) {
			print_log(LOG_ERR, "Error accepting connection: %s",
			          strerror(errno));
			break;
		}
		err = read(con, buf, MAX_NET_BUF);
		if (err < 0) {
			print_log(LOG_ERR, "Error during read: %s",
			          strerror(errno));
			break;
		} else if (err == 0) {
			/* read nothing, probably just somebody checking we're
			   alive */
			close(con);
			continue;
		}

		/* Process the arguments */
		off = read_uint32((uint32_t*)&argc, buf, off);
		argv = malloc(argc * sizeof(char*));
		if (!argv) {
			print_log(LOG_ERR, "read failed: %s", strerror(errno));
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

static int start_daemon(bool daemon_mode)
{
	int err;
	pid_t pid, sid;

	if (daemon_mode == true) {
		g_use_syslog = true;

		pid = fork();
		if (pid < 0)
			return -1;
		else if (pid > 0)
			return 0;

		umask(0);
		sid = setsid();
		if (sid < 0)
			return -1;
		if ((chdir("/")) < 0)
			return -1;
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
		openlog("qcontrol", LOG_PID, LOG_DAEMON);
	}

	print_log(LOG_INFO, "qcontrol " QCONTROL_VERSION " daemon starting.");
	err = pic_lua_setup(&lua);
	if (err != 0)
		return -1;
	err = network_listen();

	if (daemon_mode == true)
		closelog();
	return err;
}

int main(int argc, const char *argv[])
{
	const char *help = "--help";
	commandcount = 0;

	if (argc > 1 && (strcmp(argv[1], "-c") == 0)) {
		configfilename = argv[2];
		argc -= 2;
		argv += 2;
	}

	if (argc > 1 && (strcmp(argv[1], "--help") == 0
	              || strcmp(argv[1], "-?") == 0)) {
		printf("%s", usage);
		return network_send(argc - 1, argv + 1);
	} else if (argc > 1 && (strcmp(argv[1], "-V") == 0
	                     || strcmp(argv[1], "--version") == 0)) {
		printf("%s", version);
		return 0;
	} else if (argc > 1 && (strcmp(argv[1], "-d") == 0
                                || strcmp(argv[1], "--daemon") == 0)) {
		return start_daemon(true);
	} else if (argc > 1 && (strcmp(argv[1], "-f") == 0
                                || strcmp(argv[1], "--foreground") == 0)) {
		return start_daemon(false);
	} else if (argc > 2 && strcmp(argv[1], "--direct") == 0) {
		/* Execute a single command and terminate */
		pic_lua_setup(&lua);
		return run_command_direct(argv[2], argc - 3, argv + 3);
	} else if (argc > 1) {
		/* Send the command to the server */
		return network_send(argc - 1, argv + 1);
	} else {
		printf("%s", usage);
		return network_send(1, &help);
	}

	return -1;
}
