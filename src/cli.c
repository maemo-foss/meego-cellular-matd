/**
 * @file mat.c
 * @brief Command line interface for the AT command parser
 */

/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is matd.
 *
 * The Initial Developer of the Original Code is
 * remi.denis-courmont@nokia.com.
 * Portions created by the Initial Developer are
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>

#include <sys/types.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <getopt.h>
#include <pthread.h>

#include <at_modem.h>
#include <at_log.h>

static int usage (const char *cmd)
{
	const char fmt[] =
"Usage: %s [-d] [-p] [TTY node]\n"
"Provides AT commands emulation through a given terminal device\n"
"(by default, standard input and output are used).\n"
"\n"
"  -d, --debug   enable debug messages\n"
"  -h, --help    print this help and exit\n"
"  -p, --pts     create a pseudo-terminal and print the slave name\n"
"  -V, --version print version informations and exit\n";

	return (printf (fmt, cmd) >= 0) ? 0 : 1;
}

static int version (void)
{
	const char text[] =
"MeeGo AT modem emulation (version "VERSION")\n"
"Written by Remi Denis-Courmont.\n"
"Copyright (C) 2008-2010 Nokia Corporation. All rights reserved.";
	return (puts (text) >= 0) ? 0 : 1;
}

static void hangup_cb (struct at_modem *m, void *data)
{
	pthread_t *self = data;

	syslog (LOG_NOTICE, "DTE hung up: exiting...");
	pthread_kill (*self, SIGHUP);
	(void)m;
}

int main (int argc, char *argv[])
{
	int ret = -1;
	sigset_t set;

	sigemptyset (&set);
	sigaddset (&set, SIGHUP);
	sigaddset (&set, SIGINT);
	sigaddset (&set, SIGQUIT);
	sigaddset (&set, SIGTERM);
	sigaddset (&set, SIGCHLD);
	signal (SIGHUP, SIG_DFL);
	signal (SIGINT, SIG_DFL);
	signal (SIGQUIT, SIG_DFL);
	signal (SIGTERM, SIG_DFL);
	signal (SIGCHLD, SIG_DFL);
	pthread_sigmask (SIG_UNBLOCK, &set, NULL);
	sigdelset (&set, SIGCHLD);

#ifdef AT_PLUGINS_PATH
	if (setenv ("AT_PLUGINS_PATH", AT_PLUGINS_PATH, 0))
		return 2;
#endif

	int logopts = LOG_PID;
	int logmask = LOG_UPTO(LOG_NOTICE);
	bool pts = false;

	static const struct option opts[] =
	{
		{ "debug",   no_argument, NULL, 'd' },
		{ "help",    no_argument, NULL, 'h' },
		{ "pts",     no_argument, NULL, 'p' },
		{ "version", no_argument, NULL, 'V' },
		{ NULL,      no_argument, NULL, '\0'}
	};

	for (;;)
	{
		switch (getopt_long (argc, argv, "dhpV", opts, NULL))
		{
			case -1:
				goto done;
			case 'd':
				logopts |= LOG_PERROR;
				logmask = LOG_UPTO(LOG_DEBUG);
				break;
			case 'h':
				return usage (argv[0]);
			case 'p':
				pts = true;
				break;
			case 'V':
				return version ();
			case '?':
			default:
				usage (argv[0]);
				return 2;
		}
	}

done:
	setlogmask (logmask);
	openlog ("cellular: matd", logopts, LOG_DAEMON);

	int fd;

	if (pts)
	{
		fd = posix_openpt (O_RDWR|O_NOCTTY|O_CLOEXEC);
		if (fd == -1)
		{
			syslog (LOG_CRIT, "Cannot open %s: %m", "pseudo terminal");
			return 1;
		}

		char *name = ptsname (fd);
		if (name == NULL)
		{
			close (fd);
			return 1;
		}
		unlockpt (fd);
		puts (name);
		fflush (stdout);
	}
	else
	if (optind < argc)
	{
		char *path = argv[optind++];
		fd = open (path, O_RDWR|O_NOCTTY|O_CLOEXEC);
		if (fd == -1)
		{
			if (memchr (path, 0, 40) == NULL)
				strcpy (path + 37, "...");
			syslog (LOG_CRIT, "Cannot open %s: %m", path);
			return 1;
		}
	}
	else
	{
		fd = dup (0);
		if (fd == -1)
		{
			syslog (LOG_CRIT, "Cannot open %s: %m", "standard input");
			return 1;
		}
	}
	fcntl (fd, F_SETFD, FD_CLOEXEC);

	struct termios oldtp;
	bool term = !tcgetattr (fd, &oldtp);
	if (term)
	{
		struct termios tp = oldtp;

		/* XXX: Unfortunately, this does not ensure exclusive access.
		 * Another process could have opened the TTY before this: */
		ioctl (fd, TIOCEXCL);
		tp.c_iflag &= ~(IGNBRK | BRKINT | PARMRK |
		                INLCR | IGNCR | ICRNL | ISTRIP | IXON);
		tp.c_oflag &= ~(OPOST | ONLCR | OCRNL | ONOCR | ONLRET | OFILL);
		tp.c_cflag &= ~(CSIZE | PARENB);
		tp.c_cflag |= CS8 | CLOCAL;
		tp.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
		tcsetattr (fd, TCSADRAIN, &tp);
	}

	/* Prepare signal handling before allocating modem pipe */
	pthread_sigmask (SIG_BLOCK, &set, NULL);

	pthread_t self = pthread_self ();

	struct at_modem *m = at_modem_start (fd, hangup_cb, &self);
	if (m == NULL)
	{
		syslog (LOG_CRIT, "Cannot start AT modem: %m");
		goto out;
	}

	while (sigwait (&set, &(int){ 0 }) == -1);

	at_modem_stop (m);
	ret = 0;

out:
	pthread_sigmask (SIG_UNBLOCK, &set, NULL);
	if (term)
		tcsetattr (fd, TCSAFLUSH, &oldtp);
	close (fd);
	closelog ();
	return -ret;
}
