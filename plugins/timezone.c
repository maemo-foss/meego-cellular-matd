/**
 * @file timezone.c
 * @brief AT+CTZU time zone reporting
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
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
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
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/inotify.h>

#include <at_command.h>
#include <at_log.h>
#include <at_thread.h>

typedef struct
{
	at_modem_t *modem;
	pthread_t task;
	int fd;
	bool enabled;
} ctzr_t;

static void cleanup_fd (void *data)
{
	close ((intptr_t)data);
}

static void *ctzr_thread (void *opaque)
{
	at_modem_t *modem = opaque;
	const int flags = IN_DELETE_SELF|IN_MODIFY|IN_DONT_FOLLOW;
	static const char path[] = "/etc/localtime";

	int fd = inotify_init1 (IN_CLOEXEC);
	if (fd == -1)
		return NULL;

	pthread_cleanup_push (cleanup_fd, (void *)(intptr_t)fd);

	//fcntl (fd, F_SETFD, FD_CLOEXEC);
	inotify_add_watch (fd, path, flags);

	for (long current_tz = 0;;)
	{
		tzset ();

		long new_tz = timezone / (-15 * 60);
		if (current_tz != new_tz)
		{
			current_tz = new_tz;
			at_unsolicited (modem, "\r\n+CTZV: %ld\r\n", current_tz);
		}

		/* wait for change */
		struct
		{
			struct inotify_event ev;
			char buf[32];
		} b;

		ssize_t val = read (fd, &b, sizeof (b));
		if (val < (ssize_t)sizeof (b.ev))
			break;

		/* wait a little as the file may be removed and recreated */
		struct timespec tv = { 0, 100000000 };
		while (nanosleep (&tv, &tv));

		/* re-adds watch if the file was deleted */
		if (b.ev.mask & IN_IGNORED)
			inotify_add_watch (fd, path, flags);
	}

	pthread_cleanup_pop (1);
	return NULL;
}

static at_error_t set_ctzr (at_modem_t *m, const char *req, void *opaque)
{
	ctzr_t *ctzr = opaque;
	unsigned onoff;

	switch (sscanf (req, "%u", &onoff))
	{
		case 1:
			break;
		default:
			return AT_CME_EINVAL;
	}

	if (onoff > 1)
		return AT_CME_ENOTSUP;

	if (onoff == ctzr->enabled)
		return AT_OK; /* nothing to do */

	if (onoff)
	{
		if (at_thread_create (&ctzr->task, ctzr_thread, m))
			return AT_CME_ENOMEM;
		else
			ctzr->enabled = true;
	}
	else
	{
		pthread_cancel (ctzr->task);
		pthread_join (ctzr->task, NULL);
		ctzr->enabled = false;
	}

	return AT_OK;
}

static at_error_t get_ctzr (at_modem_t *m, void *opaque)
{
	ctzr_t *ctzr = opaque;

	at_intermediate (m, "\r\n+CTZR: %u", ctzr->enabled);
	return AT_OK;
}

static at_error_t list_ctzr (at_modem_t *m, void *opaque)
{
	(void) opaque;
	at_intermediate (m, "\r\n+CTZR: (0-1)");
	return AT_OK;
}

static at_error_t handle_ctzr (at_modem_t *m, const char *req, void *opaque)
{
	return at_setting (m, req, opaque, set_ctzr, get_ctzr, list_ctzr);
}

void *at_plugin_register (at_commands_t *set)
{
	ctzr_t *ctzr = malloc (sizeof (*ctzr));
	if (ctzr == NULL)
		return NULL;

	ctzr->enabled = false;

	at_register (set, "+CTZR", handle_ctzr, ctzr);
	return ctzr;
}

void at_plugin_unregister (void *opaque)
{
	ctzr_t *ctzr = opaque;
	if (ctzr == NULL)
		return;

	set_ctzr (NULL, "0", ctzr);
	free (ctzr);
}
