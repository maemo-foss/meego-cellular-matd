/**
 * @file input.c
 * @brief AT+CMER event reporting
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
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <linux/input.h>

#include <at_command.h>
#include <at_log.h>
#include <at_thread.h>

typedef struct
{
	at_modem_t *modem;
	pthread_t task;
	int keyp_fd;
	int tscrn_fd;
	bool enabled;
	bool depressed;
	unsigned x;
	unsigned y;
} cmer_t;

#include "keymap.h"

static void report_keyp (at_modem_t *m, int fd)
{
	struct input_event ev;

	if (read (fd, &ev, sizeof (ev)) != sizeof (ev))
		return;

	switch (ev.type)
	{
		case EV_KEY:
			if (ev.value > 1)
				break; // unknown value
			if (ev.code == KEY_SEMICOLON)
			{
				at_unsolicited (m, "\r\n+CKEV: \";;\",%"PRIu32"\r\n",
				                ev.value);
				break;
			}
			for (unsigned i = 0; i < 96; i++)
			{
				if (keymap[i].key == ev.code)
				{
					at_unsolicited (m, "\r\n+CKEV: \"%c\",%"PRIu32"\r\n",
					                32 + i, ev.value);
					break;
				}
				if (keymap[i].alpha == ev.code)
				{
					at_unsolicited (m, "\r\n+CKEV: ;%c;,%"PRIu32"\r\n",
					                32 + i, ev.value);
					break;
				}
			}
			break;
	}
}

static void report_tscrn (cmer_t *cmer)
{
	struct input_event ev;

	if (read (cmer->tscrn_fd, &ev, sizeof (ev)) != sizeof (ev))
		return;

	switch (ev.type)
	{
		case EV_SYN:
			at_unsolicited (cmer->modem, "\r\n+CTEV: %u,%u,%u\r\n",
			                cmer->depressed, cmer->x, cmer->y);
			break;

		case EV_KEY:
			if (ev.code != BTN_TOUCH || ev.value > 1)
				break; // unknown value
			cmer->depressed = ev.value;
			break;

		case EV_ABS:
			switch (ev.code)
			{
				case ABS_X:
					cmer->x = ev.value;
					break;
				case ABS_Y:
					cmer->y = ev.value;
					break;
			}
			break;
	}
}

static void *cmer_thread (void *opaque)
{
	cmer_t *cmer = opaque;

	for (;;)
	{
		struct pollfd ufd[2];
		unsigned n = 0;

		if (cmer->keyp_fd != -1)
		{
			ufd[n].fd = cmer->keyp_fd;
			ufd[n].events = POLLIN;
			n++;
		}

		if (cmer->tscrn_fd != -1)
		{
			ufd[n].fd = cmer->tscrn_fd;
			ufd[n].events = POLLIN;
			n++;
		}

		if (poll (ufd, n, -1) == -1)
			continue;

		n = 0;

		if (cmer->keyp_fd != -1)
		{
			if (ufd[n].revents & POLLHUP)
			{
				warning ("Keypad device is gone");
				close (cmer->keyp_fd);
				cmer->keyp_fd = -1;
			}
			else
			if (ufd[n].revents)
				report_keyp (cmer->modem, ufd[n].fd);
			n++;
		}

		if (cmer->tscrn_fd != -1)
		{
			if (ufd[n].revents & POLLHUP)
			{
				warning ("Touchscreen device is gone");
				close (cmer->tscrn_fd);
				cmer->tscrn_fd = -1;
			}
			else
			if (ufd[n].revents)
				report_tscrn (cmer);
			n++;
		}
			
	}
	return NULL;
}

static at_error_t set_cmer (at_modem_t *m, const char *req, void *opaque)
{
	cmer_t *cmer = opaque;
	unsigned mode, keyp, disp, ind, bfr, tscrn;

	switch (sscanf (req, "%u , %u , %u , %u , %u , %u", &mode, &keyp, &disp,
	                &ind, &bfr, &tscrn))
	{
		case 1:
			keyp = 0;
		case 2:
			disp = 0;
		case 3:
			ind = 0;
		case 4:
			bfr = 0;
		case 5:
			tscrn = 0;
		case 6:
			break;
		default:
			return AT_CME_EINVAL;
	}

	if (mode > 1 || keyp > 1 || disp > 0 || ind > 0 || bfr > 0
	 || tscrn > 3 || ((tscrn + 1) & 2))
		return AT_ERROR;

	at_error_t ret = AT_ERROR;
	int canc = at_cancel_disable ();

	/* Kill old thread */
	if (cmer->enabled)
	{
		pthread_cancel (cmer->task);
		pthread_join (cmer->task, NULL);
		cmer->enabled = false;
	}
	if (cmer->keyp_fd != -1)
	{
		close (cmer->keyp_fd);
		cmer->keyp_fd = -1;
	}

	/* Create new thread */
	if (keyp > 0)
	{
#ifdef KEYPAD_NODE
		int fd = open (KEYPAD_NODE, O_RDONLY|O_NDELAY|O_CLOEXEC);
		if (fd == -1)
		{
			error ("Keypad input device error (%m)");
			goto out;
		}
		fcntl (fd, F_SETFD, FD_CLOEXEC);
		cmer->keyp_fd = fd;
#else
		goto out;
#endif
	}
	if (tscrn > 0)
	{
#ifdef TOUCHSCREEN_NODE
		int fd = open (TOUCHSCREEN_NODE, O_RDONLY|O_NDELAY|O_CLOEXEC);
		if (fd == -1)
		{
			error ("Touchscreen input device error (%m)");
			goto out;
		}
		fcntl (fd, F_SETFD, FD_CLOEXEC);
		cmer->tscrn_fd = fd;
#else
		goto out;
#endif
	}
	if (mode > 0)
	{
		cmer->modem = m;
		if (at_thread_create (&cmer->task, cmer_thread, cmer))
		{
			ret = AT_CME_ENOMEM;
			goto out;
		}
		cmer->enabled = true;
	}
	ret = AT_OK;
out:
	at_cancel_enable (canc);
	(void)m;
	return ret;
}

static at_error_t get_cmer (at_modem_t *m, void *opaque)
{
	cmer_t *cmer = opaque;

	/* FIXME: There is a small theoretical violation of the POSIX memory model
	 * if cmer_thread() gets POLLHUP, overwrites a file descriptor to -1. */
	at_intermediate (m, "\r\n+CMER: %u,%u,0,0,0,%u", cmer->enabled,
	                 cmer->keyp_fd != -1,
	                 (cmer->tscrn_fd != -1) ? 3 : 0);
	return AT_OK;
}

static at_error_t list_cmer (at_modem_t *m, void *opaque)
{
	const char *can_keyp = "0", *can_tscrn = "0";

#ifdef KEYPAD_NODE
	if (access (KEYPAD_NODE, R_OK) == 0)
		can_keyp = "0-1";
#endif
#ifdef TOUCHSCREEN_NODE
	if (access (TOUCHSCREEN_NODE, R_OK) == 0)
		can_tscrn = "0,3";
#endif

	at_intermediate (m, "\r\n+CMER: (0-1),(%s),(0),(0),(0),(%s)",
	                 can_keyp, can_tscrn);
	(void)opaque;
	return AT_OK;
}

static at_error_t handle_cmer (at_modem_t *m, const char *req, void *opaque)
{
	return at_setting (m, req, opaque, set_cmer, get_cmer, list_cmer);
}

void *at_plugin_register (at_commands_t *set)
{
	cmer_t *cmer = malloc (sizeof (*cmer));
	if (cmer == NULL)
		return NULL;

	cmer->keyp_fd = -1;
	cmer->tscrn_fd = -1;
	cmer->enabled = false;
	cmer->depressed = false;
	cmer->x = cmer->y = 0;

	at_register (set, "+CMER", handle_cmer, cmer);
	return cmer;
}

void at_plugin_unregister (void *opaque)
{
	cmer_t *cmer = opaque;
	if (cmer == NULL)
		return;

	/* Release all CMER stuff */
	set_cmer (NULL, "0,0,0,0,0,0", cmer);
	free (cmer);
}
