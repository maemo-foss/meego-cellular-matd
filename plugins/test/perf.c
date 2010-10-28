/**
 * @file pref.c
 * @brief AT commands for data performance stress test
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

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>

#include <at_command.h>
#include <at_thread.h>

#define BUFSIZE 4096

static void *chargen (void *data)
{
	int fd = (intptr_t)data;
	unsigned char buf[BUFSIZE];

	for (size_t i = 0; i < sizeof (buf); i++)
		buf[i] = i;

	while (send (fd, buf, sizeof (buf), MSG_NOSIGNAL) >= 0);

	return NULL;
}

static void *discard (void *data)
{
	int fd = (intptr_t)data;
	unsigned char buf[BUFSIZE];

	while (recv (fd, buf, sizeof (buf), 0) >= 0);

	return NULL;
}

static void *echo (void *data)
{
	int fd = (intptr_t)data;
	unsigned char buf[BUFSIZE];

	for (;;)
	{
		ssize_t len = recv (fd, buf, sizeof (buf), 0);
		if (len < 0)
			break;
		if (send (fd, buf, len, MSG_NOSIGNAL) < 0)
			break;
	}

	return NULL;
}

static void cleanup_fds (void *data)
{
	const int *fds = data;

	close (fds[1]);
	close (fds[0]);
}

static void cleanup_thread (void *data)
{
	pthread_t th = *(pthread_t *)data;

	pthread_cancel (th);
	pthread_join (th, NULL);
}


static at_error_t forward (at_modem_t *modem, const char *req, void *data)
{
	void *(*func) (void *) = data;
	int fds[2];

#ifdef SOCK_CLOEXEC
	if (socketpair (PF_LOCAL, SOCK_STREAM|SOCK_CLOEXEC, 0, fds)
	 && errno == EINVAL)
#endif
	{
		if (socketpair (PF_LOCAL, SOCK_STREAM, 0, fds))
			return AT_CME_ENOMEM;
		fcntl (fds[0], F_SETFD, FD_CLOEXEC);
		fcntl (fds[1], F_SETFD, FD_CLOEXEC);
	}

	pthread_t th;

	if (at_thread_create (&th, func, (void *)(intptr_t)fds[1]))
	{
		int canc = at_cancel_disable ();
		close (fds[1]);
		close (fds[0]);
		at_cancel_enable (canc);
		return AT_CME_ENOMEM;
	}

	pthread_cleanup_push (cleanup_fds, fds);
	pthread_cleanup_push (cleanup_thread, &th);
	at_connect (modem, fds[0]);
	pthread_cleanup_pop (1);
	pthread_cleanup_pop (1);

	(void) req;
	return AT_NO_CARRIER;
}

void *at_plugin_register (at_commands_t *set)
{
	at_register (set, "*MCHARGEN", forward, chargen);
	at_register (set, "*MDISCARD", forward, discard);
	at_register (set, "*MECHO", forward, echo);

	return NULL;
}
