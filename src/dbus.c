/**
 * @file dbus.c
 * @brief D-Bus helper
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

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include <time.h>
#include <sys/poll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <at_dbus.h>
#include <at_log.h>
#include <at_thread.h>

typedef struct
{
	DBusWatch *watch;
	int fd;
	unsigned events;
} at_dbus_watch_t;

typedef struct
{
	DBusTimeout *timeout;
	unsigned interval;
	uint64_t deadline;
} at_dbus_timeout_t;

typedef struct
{
	DBusConnection *conn;

	size_t nfds;
	at_dbus_watch_t **fds;
	at_dbus_watch_t wakeup;

	size_t ntos;
	at_dbus_timeout_t **tos;

	pthread_mutex_t lock;
	pthread_t thread;
} at_dbus_t;

static void at_dbus_wakeup (void *opaque)
{
	at_dbus_t *ad = opaque;
	uint64_t count = 1;

	write (ad->wakeup.fd, &count, sizeof (count));
}

static void at_dbus_toggle_watch (DBusWatch *watch, void *opaque)
{
	at_dbus_t *ad = opaque;
	at_dbus_watch_t *adw = dbus_watch_get_data (watch);
	unsigned events = 0;

	if (dbus_watch_get_enabled (watch))
	{
		unsigned flags = dbus_watch_get_flags (watch);

		if (flags & DBUS_WATCH_READABLE)
			events |= POLLIN;
		if (flags & DBUS_WATCH_WRITABLE)
			events |= POLLOUT;
	}

	pthread_mutex_lock (&ad->lock);
	assert (adw->fd == dbus_watch_get_unix_fd (watch));
	adw->events = events;
	pthread_mutex_unlock (&ad->lock);
}

static dbus_bool_t at_dbus_add_watch (DBusWatch *watch, void *opaque)
{
	at_dbus_t *ad = opaque;
	at_dbus_watch_t *adw = malloc (sizeof (*adw));
	if (adw == NULL)
		return FALSE;

	adw->watch = watch;
	adw->fd = dbus_watch_get_unix_fd (watch);
	adw->events = 0;
	dbus_watch_set_data (watch, adw, NULL);

	pthread_mutex_lock (&ad->lock);

	size_t n = ad->nfds;
	at_dbus_watch_t **fds = realloc (ad->fds, (n + 1) * sizeof (*fds));
	if (fds != NULL)
	{
		ad->fds = fds;
		fds[n++] = adw;
		ad->nfds = n;
	}

	pthread_mutex_unlock (&ad->lock);
	return (fds != NULL);
}

static void at_dbus_remove_watch (DBusWatch *watch, void *opaque)
{
	at_dbus_t *ad = opaque;
	at_dbus_watch_t *adw = dbus_watch_get_data (watch);

	pthread_mutex_lock (&ad->lock);
	at_dbus_watch_t **fds = ad->fds;

	size_t i, n = ad->nfds;
	for (i = 0; fds[i] != adw; i++)
		assert (i < n);

	ad->nfds = --n;
	memmove (fds + i, fds + i + 1, sizeof (*fds) * (n - i));
	fds = realloc (fds, sizeof (*fds) * n);
	if (fds != NULL)
		ad->fds = fds;
	pthread_mutex_unlock (&ad->lock);

	free (adw);
}

static uint64_t getclock (void)
{
	struct timespec ts;

	clock_gettime (CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * UINT64_C(1000000000) + ts.tv_nsec;
}

static void at_dbus_toggle_timeout (DBusTimeout *timeout, void *opaque)
{
	at_dbus_t *ad = opaque;
	at_dbus_timeout_t *adt = dbus_timeout_get_data (timeout);
	uint64_t deadline;

	if (dbus_timeout_get_enabled (timeout))
		deadline = getclock () + dbus_timeout_get_interval (timeout);
	else
		deadline = 0;

	pthread_mutex_lock (&ad->lock);
	assert (adt->interval == (unsigned)dbus_timeout_get_interval (timeout));
	adt->deadline = deadline;
	pthread_mutex_unlock (&ad->lock);
}

static dbus_bool_t at_dbus_add_timeout (DBusTimeout *timeout, void *opaque)
{
	at_dbus_t *ad = opaque;
	at_dbus_timeout_t *adt = malloc (sizeof (*adt));
	if (adt == NULL)
		return FALSE;

	adt->timeout = timeout;
	adt->interval = dbus_timeout_get_interval (timeout);
	adt->deadline = 0;
	dbus_timeout_set_data (timeout, adt, NULL);

	pthread_mutex_lock (&ad->lock);

	size_t n = ad->ntos;
	at_dbus_timeout_t **tos = realloc (ad->tos, (n + 1) * sizeof (*tos));
	if (tos != NULL)
	{
		ad->tos = tos;
		tos[n++] = adt;
		ad->ntos = n;
	}

	pthread_mutex_unlock (&ad->lock);
	return (tos != NULL);
}

static void at_dbus_remove_timeout (DBusTimeout *timeout, void *opaque)
{
	at_dbus_t *ad = opaque;
	at_dbus_timeout_t *adt = dbus_timeout_get_data (timeout);

	pthread_mutex_lock (&ad->lock);
	at_dbus_timeout_t **tos = ad->tos;

	size_t i, n = ad->ntos;
	for (i = 0; tos[i] != adt; i++)
		assert (i < n);

	ad->ntos = --n;
	if (n > 0)
	{
		memmove (tos + i, tos + i + 1, sizeof (*tos) * (n - i));
		tos = realloc (tos, sizeof (*tos) * n);
		if (tos != NULL)
			ad->tos = tos;
	}
	else
	{
		free (tos);
		ad->tos = NULL;
	}
	pthread_mutex_unlock (&ad->lock);

	free (adt);
}

static void *at_dbus_thread (void *opaque)
{
	at_dbus_t *ad = opaque;
	DBusConnection *conn = ad->conn;

loop:
	for (;;)
	{
		int canc = at_cancel_disable ();
		while (dbus_connection_get_dispatch_status (conn)
				== DBUS_DISPATCH_DATA_REMAINS)
			dbus_connection_dispatch (conn);
		at_cancel_enable (canc);

		uint64_t now = getclock ();

		pthread_mutex_lock (&ad->lock);
		size_t nfds = ad->nfds;
		size_t ntos = ad->ntos;
		struct pollfd ufd[nfds];
		int timeout = -1;

		for (size_t i = 0; i < nfds; i++)
		{
			at_dbus_watch_t *adw = ad->fds[i];

			ufd[i].fd = adw->fd;
			ufd[i].events = adw->events;
		}

		for (size_t i = 0; i < ntos; i++)
		{
			at_dbus_timeout_t *adt = ad->tos[i];

			if (adt->deadline == 0)
				continue;

			int64_t delta = now - adt->deadline;
			if (delta <= 0)
			{
				adt->deadline += adt->interval;
				pthread_mutex_unlock (&ad->lock);
				dbus_timeout_handle (adt->timeout);
				goto loop;
			}
			if (timeout < 0 || delta < timeout)
				timeout = delta;
		}

		pthread_mutex_unlock (&ad->lock);

		int n = poll (ufd, nfds, timeout);
		if (n == -1)
			continue;

		pthread_mutex_lock (&ad->lock);
		nfds = ad->nfds;
		for (size_t i = 0; n > 0; i++)
		{
			if (ufd[i].revents == 0)
				continue;

			int fd = ufd[i].fd;
			unsigned flags = 0;
			if (ufd[i].revents & POLLIN)
				flags |= DBUS_WATCH_READABLE;
			if (ufd[i].revents & POLLOUT)
				flags |= DBUS_WATCH_WRITABLE;
			if (ufd[i].revents & POLLERR)
				flags |= DBUS_WATCH_ERROR;
			if (ufd[i].revents & POLLHUP)
				flags |= DBUS_WATCH_HANGUP;

			/* Unfortunately, the order of watches could have changed */
			for (size_t j = 0; j < nfds; j++)
			{
				at_dbus_watch_t *adw = ad->fds[j];

				if (adw->fd == fd)
				{
					pthread_mutex_unlock (&ad->lock);
					if (adw == &ad->wakeup)
						read (fd, &(uint64_t){ 0 }, 8);
					else
						dbus_watch_handle (adw->watch, flags);
					goto loop;
				}
			}	
			n--;
		}
		pthread_mutex_unlock (&ad->lock);
	}
	assert (0);
}

/* Note: Cleanup is not implemented currently. This would become a problem if
 * some process would dlclose() libmatd. Currently, this is not the case.
 */

static DBusConnection *at_dbus_get (DBusBusType type)
{
	static at_dbus_t system_bus = { .conn = NULL };
	static at_dbus_t session_bus = { .conn = NULL };
	static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
	at_dbus_t *restrict bus;
	DBusConnection *conn;

	switch (type)
	{
		case DBUS_BUS_SYSTEM:
			bus = &system_bus;
			break;
		case DBUS_BUS_SESSION:
			bus = &session_bus;
			break;
		default:
			return NULL;
	}

	pthread_mutex_lock (&lock);
	conn = bus->conn;
	if (conn != NULL)
		goto out;

	/* Connect to DBus first time */
	if (!dbus_threads_init_default ())
		goto out;

	DBusError err;
	dbus_error_init (&err);
	conn = dbus_bus_get_private (type, &err);
	if (conn == NULL)
	{
		error ("Cannot connect to system D-Bus (%s)",
		       err.message ? err.message : "");
		dbus_error_free (&err);
		goto out;
	}

	bus->conn = conn;
	dbus_connection_set_exit_on_disconnect (conn, FALSE);

	/* Initialize DBus signal handling thread */
	bus->nfds = 1;
	bus->fds = malloc (sizeof (bus->fds[0]));
	if (bus->fds == NULL)
		goto drop;

	bus->wakeup.watch = NULL;
	bus->wakeup.fd = eventfd (0, EFD_CLOEXEC);
	if (bus->wakeup.fd == -1)
		goto drop;
	bus->fds[0] = &bus->wakeup;

	bus->ntos = 0;
	bus->tos = NULL;

	pthread_mutex_init (&bus->lock, NULL);
	dbus_connection_set_wakeup_main_function (conn, at_dbus_wakeup, bus, NULL);
	if (!dbus_connection_set_watch_functions (conn, at_dbus_add_watch,
	                     at_dbus_remove_watch, at_dbus_toggle_watch, bus, NULL)
	 || !dbus_connection_set_timeout_functions (conn, at_dbus_add_timeout,
	                 at_dbus_remove_timeout, at_dbus_toggle_timeout, bus, NULL)
	 || at_thread_create (&bus->thread, at_dbus_thread, bus))
	{
		close (bus->wakeup.fd);
		goto drop;
	}

out:
	pthread_mutex_unlock (&lock);
	return conn;

drop:
	dbus_connection_unref (conn);
	free (bus->fds);
	conn = NULL;
	goto out;
}


int at_dbus_add_filter (DBusBusType bus, DBusHandleMessageFunction filter_cb,
                        void *opaque, DBusFreeFunction free_cb)
{
	int canc = at_cancel_disable ();
	int ret = -1;

	DBusConnection *conn = at_dbus_get (bus);
	if (conn != NULL
	 && dbus_connection_add_filter (conn, filter_cb, opaque, free_cb))
		ret = 0;

	at_cancel_enable (canc);
	return 0;
}


void at_dbus_remove_filter (DBusBusType bus,
                            DBusHandleMessageFunction filter_cb, void *opaque)
{
	int canc = at_cancel_disable ();
	DBusConnection *conn = at_dbus_get (bus);
	assert (conn != NULL); /* cannot fail as add_filter succeeded */

	dbus_connection_remove_filter (conn, filter_cb, opaque);
	at_cancel_enable (canc);
}


void at_dbus_add_match (DBusBusType bus, const char *rule)
{
	DBusConnection *conn = at_dbus_get (bus);
	if (conn == NULL)
		return;

	dbus_bus_add_match (conn, rule, NULL);
}


void at_dbus_remove_match (DBusBusType bus, const char *rule)
{
	DBusConnection *conn = at_dbus_get (bus);
	if (conn == NULL)
		return;

	dbus_bus_remove_match (conn, rule, NULL);
}


/*** DBus request without reply ***/

int at_dbus_request (DBusBusType bus, DBusMessage *msg)
{
	at_cancel_assert (false);

	DBusConnection *conn = at_dbus_get (bus);
	if (conn == NULL)
		return -1;

	bool ok = dbus_connection_send (conn, msg, NULL);
	dbus_message_unref (msg);
	if (!ok)
	{
		error ("Cannot send D-Bus request");
		return -1;
	}
	dbus_connection_flush (conn);
	return 0;
}



/*** DBus request with reply ***/

DBusMessage *at_dbus_query (DBusBusType bus, DBusMessage *req, int delay,
                            DBusError *err)
{
	DBusMessage *reply;

	at_cancel_assert (false);

	if (err == NULL)
	{
		DBusError errbuf;

		reply = at_dbus_query (bus, req, delay, &errbuf);
		if (reply == NULL)
			dbus_error_free (&errbuf);
		return reply;
	}

	dbus_error_init (err);

	DBusConnection *conn = at_dbus_get (bus);
	if (conn == NULL)
		return NULL;

	reply = dbus_connection_send_with_reply_and_block (conn, req, delay, err);
	dbus_message_unref (req);

	if (reply == NULL)
		error ("Cannot send D-Bus request: %s (%s)",
		       err->message ? err->message : "unspecified error",
		       err->name ? err->name : "unnamed D-Bus error");
	return reply;
}

DBusMessage *at_dbus_request_reply (DBusBusType bus, DBusMessage *req)
{
	return at_dbus_query (bus, req, -1, NULL);
}


/*** Dictionary ***/

int at_dbus_dict_lookup_string (DBusMessageIter *it, const char *name,
                                DBusMessageIter *value)
{
	at_cancel_assert (false);

	DBusMessageIter array;

	if (dbus_message_iter_get_arg_type (it) != DBUS_TYPE_ARRAY
	 || dbus_message_iter_get_element_type (it) != DBUS_TYPE_DICT_ENTRY)
		return -1;

	dbus_message_iter_recurse (it, &array);

	while (dbus_message_iter_get_arg_type (&array) != DBUS_TYPE_INVALID)
	{
		const char *key;

		dbus_message_iter_recurse (&array, value);
		if (dbus_message_iter_get_arg_type (value) != DBUS_TYPE_STRING)
			break; /* wrong dictionary key type */
		dbus_message_iter_get_basic (value, &key);
		if (!strcmp (name, key))
		{	/* found! */
			dbus_message_iter_next (value);
			return 0;
		}
		dbus_message_iter_next (&array);
	}
	return -1;
}
