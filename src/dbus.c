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
#include <stdbool.h>
#include <assert.h>

#include <at_dbus.h>
#include <at_log.h>
#include <at_thread.h>

/* Note: Cleanup is not implemented currently. This would become a problem if
 * some process would dlclose() libmatd. Currently, this is not the case.
 */

static DBusConnection *at_dbus_connect_unlocked (DBusBusType type)
{
	if (!dbus_threads_init_default ())
		return NULL;

	DBusError err;
	dbus_error_init (&err);

	DBusConnection *conn = dbus_bus_get_private (type, &err);
	if (conn == NULL)
	{
		error ("Cannot connect to system D-Bus (%s)",
		       err.message ? err.message : "");
		dbus_error_free (&err);
		return NULL;
	}
	dbus_connection_set_exit_on_disconnect (conn, FALSE);
	return conn;
}

#ifdef NEED_SIGNALS
static void *at_dbus_thread (void *data)
{
	DBusConnection *conn = data;

	while (dbus_connection_read_write_dispatch (conn, -1));

	return NULL;
}

static DBusConnection *at_dbus_get (DBusBusType type, bool signals)
#else
static DBusConnection *at_dbus_get (DBusBusType type)
#endif
{
	static DBusConnection *system_bus = NULL;
#ifdef NEED_SIGNALS
	static DBusConnection *system_bus_async = NULL;
#endif
	static DBusConnection *session_bus = NULL;
	static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
	DBusConnection *restrict *pconn, *conn;

	switch (type)
	{
		case DBUS_BUS_SYSTEM:
#ifdef NEED_SIGNALS
			pconn = signals ? &system_bus_async : &system_bus;
#else
			pconn = &system_bus;
#endif
			break;
		case DBUS_BUS_SESSION:
#ifdef NEED_SIGNALS
			if (signals)
				return NULL;
#endif
			pconn = &session_bus;
			break;
		default:
			return NULL;
	}

	pthread_mutex_lock (&lock);
	conn = *pconn;
	if (conn == NULL)
	{
		/* Connect to DBus first time */
		conn = at_dbus_connect_unlocked (type);
#ifdef NEED_SIGNALS
		/* Initialize DBus signal handling thread */
		if (conn != NULL && signals)
		{
			pthread_t th;

			if (at_thread_create (&th, at_dbus_thread, conn))
			{
				dbus_connection_unref (conn);
				conn = NULL;
			}
			else
				pthread_detach (th);
		}
#endif
		*pconn = conn;
	}
	pthread_mutex_unlock (&lock);
	return conn;
}

/**
 * Connect to a given DBus bus (if not already done).
 * @return the DBus connection or NULL on error.
 */
static DBusConnection *at_dbus_connect (DBusBusType type)
{
#ifndef NEED_SIGNALS
	return at_dbus_get (type);
#else
	return at_dbus_get (type, false);
}

/**
 * Connect to a given bus (if not already done), and start the read, write and
 * dispatch procedures asynchronously.
 * @return the DBus connection or NULL on error.
 */
static DBusConnection *at_dbus_connect_dispatch (DBusBusType type)
{
	return at_dbus_get (type, true);
}


int at_dbus_add_filter (DBusBusType bus, DBusHandleMessageFunction filter_cb,
                        void *opaque, DBusFreeFunction free_cb)
{
	int canc = at_cancel_disable ();
	int ret = -1;

	DBusConnection *conn = at_dbus_connect_dispatch (bus);
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
	DBusConnection *conn = at_dbus_connect_dispatch (bus);
	assert (conn != NULL); /* cannot fail as add_filter succeeded */

	dbus_connection_remove_filter (conn, filter_cb, opaque);
	at_cancel_enable (canc);
}


void at_dbus_add_match (DBusBusType bus, const char *rule)
{
	DBusConnection *conn = at_dbus_connect_dispatch (bus);
	if (conn == NULL)
		return;

	dbus_bus_add_match (conn, rule, NULL);
}


void at_dbus_remove_match (DBusBusType bus, const char *rule)
{
	DBusConnection *conn = at_dbus_connect_dispatch (bus);
	if (conn == NULL)
		return;

	dbus_bus_remove_match (conn, rule, NULL);
#endif
}


/*** DBus request without reply ***/

int at_dbus_request (DBusBusType bus, DBusMessage *msg)
{
	at_cancel_assert (false);

	DBusConnection *conn = at_dbus_connect (bus);
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

	DBusConnection *conn = at_dbus_connect (bus);
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
