/**
 * @file usb.c
 * @brief USB CDC ACM front-end
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

#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <sys/types.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <locale.h>

#include <at_modem.h>

static void open_syslog (void)
{
	int options = LOG_PID;
	int fd = open ("/root/usb-cdc-acm.log", O_WRONLY|O_APPEND);
	if (fd != -1)
	{
		dup2 (fd, 2);
		close (fd);
		options |= LOG_PERROR;
	}
	else
		setlogmask (LOG_UPTO(LOG_INFO));
	openlog ("cellular: acm", options, LOG_DAEMON);
}

static void close_syslog (void)
{
	closelog ();
}


static void hangup_cb (struct at_modem *m, void *data)
{
	pthread_t *pth = data;

	syslog (LOG_NOTICE, "USB host hung up");
	pthread_kill (*pth, SIGHUP);
	(void)m;
}

static int open_tty (const char *path, struct termios *oldtp)
{
	/* Prepare our terminal (connection to DTE) */
	int fd = open (path, O_RDWR|O_NOCTTY|O_CLOEXEC|O_NONBLOCK);
	if (fd == -1)
	{
		syslog (LOG_CRIT, "%s: %m", path);
		return -1;
	}
	fcntl (fd, F_SETFD, FD_CLOEXEC);

	if (tcgetattr (fd, oldtp))
	{
		syslog (LOG_CRIT, "%s: %m", path);
		close (fd);
		return -1;
	}

	/* Enable raw TTY mode */
	struct termios tp = *oldtp;

	ioctl (fd, TIOCEXCL);
	tp.c_iflag &= ~(IGNBRK | BRKINT | PARMRK |
	                INLCR | IGNCR | ICRNL | ISTRIP | IXON);
	tp.c_oflag &= ~(OPOST | ONLCR | OCRNL | ONOCR | ONLRET | OFILL);
	tp.c_cflag &= ~(CSIZE | PARENB);
	tp.c_cflag |= CS8 | CLOCAL;
	tp.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	tcsetattr (fd, TCSADRAIN, &tp);

	/* Blocking mode for actual I/O */
	fcntl (fd, F_SETFL, fcntl (fd, F_GETFL) & ~O_NONBLOCK);
	return fd;
}

static void close_tty (int fd, const struct termios *oldtp)
{
	tcsetattr (fd, TCSAFLUSH, oldtp);
	close (fd);
}


int main (void)
{
	sigset_t set;

	setsid ();

	/* During initialization, use default signals handlers */
	signal (SIGHUP, SIG_DFL);
	signal (SIGINT, SIG_DFL);
	signal (SIGQUIT, SIG_DFL);
	signal (SIGTERM, SIG_DFL);
	signal (SIGCHLD, SIG_DFL);
	sigemptyset (&set);
	sigaddset (&set, SIGHUP);
	sigaddset (&set, SIGINT);
	sigaddset (&set, SIGQUIT);
	sigaddset (&set, SIGTERM);
	sigaddset (&set, SIGCHLD);
	pthread_sigmask (SIG_UNBLOCK, &set, NULL);
	sigdelset (&set, SIGCHLD);

	setlocale (LC_CTYPE, "");

	if (unsetenv ("AT_PLUGINS_PATH"))
		return 2;

	open_syslog ();

	if (at_load_plugins () == -1)
		return 1;

	syslog (LOG_INFO, "started");

	pthread_sigmask (SIG_BLOCK, &set, NULL);
	pthread_t self = pthread_self ();
	int signum, ret;

	do
	{
		struct termios oldtp;
		int fd = open_tty ("/dev/usbacm", &oldtp);
		if (fd == -1)
		{
			ret = -1;
			goto error;
		}

		struct at_modem *m = at_modem_start (fd, hangup_cb, &self);
		if (m == NULL)
		{
			syslog (LOG_CRIT, "Cannot start USB modem: %m");
			ret = -1;
			goto error;
		}

		while (sigwait (&set, &signum) == -1);

		at_modem_stop (m);
		close_tty (fd, &oldtp);
		ret = 0;
	}
	while (signum == SIGHUP);

	syslog (LOG_INFO, "stopped (caught signal %d - %s)", signum,
	        strsignal (signum));
error:
	pthread_sigmask (SIG_UNBLOCK, &set, NULL);
	close_syslog ();
	at_unload_plugins ();
	return -ret;
}
