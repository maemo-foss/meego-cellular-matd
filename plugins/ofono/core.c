/**
 * @file core.c
 * @brief oFono plugin
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
#include <assert.h>

#include <at_command.h>
#include <at_log.h>
#include <at_thread.h>
#include <at_dbus.h>

#include "ofono.h"
#include "core.h"


/*** DBus oFono helpers ***/
DBusMessage *ofono_query (DBusMessage *req, at_error_t *err)
{
	DBusError error;
	DBusMessage *reply = at_dbus_query (DBUS_BUS_SYSTEM, req, -1, &error);

	at_error_t ret = AT_CME_UNKNOWN;

	if (reply != NULL)
		ret = AT_OK;
	else if (error.name == NULL)
		;
	else if (!strncmp (error.name, "org.ofono.Error.", 16))
	{
		const char *oferr = error.name + 16;
		if (!strcmp (oferr, "InvalidArguments"))
			ret = AT_CME_EINVAL;
		else if (!strcmp (oferr, "InvalidFormat"))
			ret = AT_CME_ERROR (25);
		else if (!strcmp (oferr, "NotImplemented"))
			ret = AT_CME_ENOTSUP;
		else if (!strcmp (oferr, "Failed"))
			ret = AT_CME_ERROR (0);
		else if (!strcmp (oferr, "InProgress"))
			ret = AT_CME_EBUSY;
		else if (!strcmp (oferr, "NotFound"))
			ret = AT_CME_ENOENT;
		else if (!strcmp (oferr, "NotActive"))
			ret = AT_CME_EINVAL;
		else if (!strcmp (oferr, "NotSupported"))
			ret = AT_CME_ENOTSUP;
		else if (!strcmp (oferr, "Timedout"))
			ret = AT_CME_ETIMEDOUT;
		else if (!strcmp (oferr, "SimNotReady"))
			ret = AT_CME_ERROR (14);
		else if (!strcmp (oferr, "InUse"))
			ret = AT_CME_EBUSY;
		else if (!strcmp (oferr, "NotAttached"))
			ret = AT_CME_ERROR (30);
		else if (!strcmp (oferr, "AttachInProgress"))
			ret = AT_CME_ERROR (30);
		else if (!strcmp (oferr, "Canceled"))
			ret = AT_CME_UNKNOWN;
		else if (!strcmp (oferr, "AccessDenied"))
			ret = AT_CME_EPERM;
	}
	else if (!strncmp (error.name, "org.freedesktop.DBus.Error.", 27))
	{
		const char *dberr = error.name + 27;
		if (!strcmp (dberr, "AccessDenied"))
			ret = AT_CME_EPERM;
		else if (!strcmp (dberr, "NoMemory"))
			ret = AT_CME_ENOMEM;
		else if (!strcmp (dberr, "InvalidArgs"))
			ret = AT_CME_EINVAL;
		else
			ret = AT_CME_ERROR (0);
	}
	else
		warning ("Unknown D-Bus error");
	dbus_error_free (&error);
	*err = ret;
	return reply;
}

int ofono_dict_find (DBusMessageIter *dict, const char *name, int type,
                     DBusMessageIter *value)
{
	DBusMessageIter entry;

	at_cancel_assert (false);

	if (at_dbus_dict_lookup_string (dict, name, &entry)
	 || dbus_message_iter_get_arg_type (&entry) != DBUS_TYPE_VARIANT)
	{
		warning ("Property %s not found", name);
		return -1;
	}
	dbus_message_iter_recurse (&entry, value);
	if (dbus_message_iter_get_arg_type (value) != type)
	{
		warning ("Property %s type wrong", name);
		return -1;
	}
	return 0;
}

int ofono_dict_find_basic (DBusMessageIter *dict, const char *name,
                           int type, void *buf)
{
	DBusMessageIter value;

	if (ofono_dict_find (dict, name, type, &value))
		return -1;
	dbus_message_iter_get_basic (&value, buf);
	return 0;
}

DBusMessage *ofono_req_new (const plugin_t *p, const char *path,
				const char *subif, const char *method)
{
	size_t len = strlen (subif);
	char iface[11 + len];

	memcpy (iface, "org.ofono.", 10);
	strcpy (iface + 10, subif);
	return dbus_message_new_method_call (p->name, path, iface, method);
}

static at_error_t ofono_request_va (const plugin_t *p, const char *path,
					const char *subif, const char *method,
					int first, va_list ap)
{
	at_error_t ret;
	int canc = at_cancel_disable ();

	DBusMessage *msg = ofono_req_new (p, path, subif, method);
	if (msg != NULL)
	{
		if (dbus_message_append_args_valist (msg, first, ap))
			msg = ofono_query (msg, &ret);

		if (msg != NULL)
			dbus_message_unref (msg);
	}
	else
		ret = AT_CME_ENOMEM;

	at_cancel_enable (canc);

	return ret;
}

at_error_t ofono_request (const plugin_t *p, const char *path,
			      const char *subif, const char *method,
			      int first, ...)
{
	at_error_t ret;
	va_list ap;

	va_start (ap, first);
	ret = ofono_request_va (p, path, subif, method, first, ap);
	va_end (ap);

	return ret;
}

/*** Modem D-Bus helpers ***/

DBusMessage *modem_req_new (const plugin_t *p, const char *subif,
                            const char *method)
{
	return ofono_req_new (p, p->modemv[p->modem], subif, method);
}

at_error_t modem_request (const plugin_t *p, const char *subif,
                          const char *method, int first, ...)
{
	at_error_t ret;
	va_list ap;

	va_start (ap, first);
	ret = ofono_request_va (p, p->modemv[p->modem], subif, method, first, ap);
	va_end (ap);

	return ret;
}

DBusMessage *modem_props_get (const plugin_t *p, const char *iface)
{
	DBusMessage *msg = modem_req_new (p, iface, "GetProperties");
	if (msg == NULL)
		return NULL;

	at_error_t err;
	msg = ofono_query (msg, &err);
	if (msg == NULL)
		warning ("Cannot get oFono %s properties (error %u)", iface, err);
	return msg;
}

int ofono_prop_find (DBusMessage *msg, const char *name, int type,
                     DBusMessageIter *value)
{
	DBusMessageIter dict;

	if (!dbus_message_iter_init (msg, &dict))
		return -1;
	return ofono_dict_find (&dict, name, type, value);
}

int ofono_prop_find_basic (DBusMessage *msg, const char *name, int type,
                           void *buf)
{
	DBusMessageIter value;

	if (ofono_prop_find (msg, name, type, &value))
		return -1;
	dbus_message_iter_get_basic (&value, buf);
	return 0;
}

char *modem_prop_get_string (const plugin_t *p, const char *iface,
                             const char *name)
{
	char *ret = NULL;
	int canc = at_cancel_disable ();

	DBusMessage *props = modem_props_get (p, iface);
	if (props != NULL)
	{
		const char *str = ofono_prop_find_string (props, name);
		if (str != NULL)
			ret = strdup (str);
		dbus_message_unref (props);
	}
	at_cancel_enable (canc);
	return ret;
}

int modem_prop_get_bool (const plugin_t *p, const char *iface,
                         const char *name)
{
	int canc = at_cancel_disable ();
	int ret;

	DBusMessage *props = modem_props_get (p, iface);
	if (props != NULL)
	{
		ret = ofono_prop_find_bool (props, name);
		dbus_message_unref (props);
	}
	else
		ret = -1;

	at_cancel_enable (canc);
	return ret;
}

int modem_prop_get_byte (const plugin_t *p, const char *iface,
                         const char *name)
{
	int canc = at_cancel_disable ();
	int ret;

	DBusMessage *props = modem_props_get (p, iface);
	if (props != NULL)
	{
		ret = ofono_prop_find_byte (props, name);
		dbus_message_unref (props);
	}
	else
		ret = -1;

	at_cancel_enable (canc);
	return ret;
}

int modem_prop_get_u16 (const plugin_t *p, const char *iface, const char *name)
{
	int ret;
	int canc = at_cancel_disable ();

	DBusMessage *props = modem_props_get (p, iface);
	if (props != NULL)
		ret = ofono_prop_find_u16 (props, name);
	else
		ret = -1;
	at_cancel_enable (canc);
	return ret;
}

int64_t modem_prop_get_u32 (const plugin_t *p, const char *iface,
                            const char *name)
{
	int64_t ret;
	int canc = at_cancel_disable ();

	DBusMessage *props = modem_props_get (p, iface);
	if (props != NULL)
		ret = ofono_prop_find_u32 (props, name);
	else
		ret = -1;
	at_cancel_enable (canc);
	return ret;
}

at_error_t modem_prop_set (const plugin_t *p, const char *iface,
                           const char *name, int type, void *value,
                           const char *pass)
{
	int canc = at_cancel_disable ();
	at_error_t ret = AT_CME_ENOMEM;

	DBusMessage *msg = modem_req_new (p, iface, "SetProperty");
	if (msg == NULL)
		goto out;

	DBusMessageIter args, variant;
	char signature[2] = { type, 0 };

	dbus_message_iter_init_append (msg, &args);
	if (!dbus_message_iter_append_basic (&args, DBUS_TYPE_STRING, &name)
	 || !dbus_message_iter_open_container (&args, DBUS_TYPE_VARIANT,
	                                       signature, &variant)
	 || !dbus_message_iter_append_basic (&variant, type, value)
	 || !dbus_message_iter_close_container (&args, &variant)
	 || ((pass != NULL)
	  && !dbus_message_iter_append_basic (&args, DBUS_TYPE_STRING, &pass)))
	{
		dbus_message_unref (msg);
		goto out;
	}

	msg = ofono_query (msg, &ret);
	if (msg == NULL)
		warning ("Cannot set oFono %s %s property", iface, name);
	else
		dbus_message_unref (msg);
out:
	at_cancel_enable (canc);
	return ret;
}

/*** Voice call helper ***/
at_error_t voicecall_request (const plugin_t *p, unsigned callid,
                              const char *method, int first, ...)
{
	const char *modem = p->modemv[p->modem];
	size_t len = strlen (modem) + sizeof ("/voicecall99");
	char path[len];

	if (callid > 99)
		return AT_CME_ENOENT;
	snprintf (path, len, "%s/voicecall%02u", modem, callid);

	DBusMessage *msg;
	at_error_t ret;
	int canc = at_cancel_disable ();

	msg = dbus_message_new_method_call (p->name, path,
	                                    "org.ofono.VoiceCall", method);
	if (msg != NULL)
	{
		va_list ap;

		va_start (ap, first);
		if (dbus_message_append_args_valist (msg, first, ap))
			msg = ofono_query (msg, &ret);
		va_end (ap);

		if (msg != NULL)
			dbus_message_unref (msg);
	}
	else
		ret = AT_CME_ENOMEM;

	at_cancel_enable (canc);
	return ret;
}


/*** Modem manager ***/
static char *manager_find (char ***modemlist, unsigned *modemcount)
{
	DBusMessage *msg;

	/* list oFono modems */
	msg = dbus_message_new_method_call ("org.ofono", "/",
	                                    "org.ofono.Manager", "GetModems");
	if (msg == NULL)
		return NULL;

	at_error_t err;
	msg = ofono_query (msg, &err);
	if (msg == NULL)
	{
		error ("oFono manager not present (error %u)", err);
		return NULL;
	}

	/* Remember unique name of oFono service */
	char *name = strdup (dbus_message_get_sender (msg));

	DBusMessageIter args, array;

	if (name == NULL
	 || !dbus_message_iter_init (msg, &args)
	 || dbus_message_iter_get_arg_type (&args) != DBUS_TYPE_ARRAY
	 || dbus_message_iter_get_element_type (&args) != DBUS_TYPE_STRUCT)
	{
		dbus_message_unref (msg);
		return NULL;
	}
	dbus_message_iter_recurse (&args, &array);

	/* Enumerate modems */
	char **tab = NULL;
	unsigned i = 0;

	while (dbus_message_iter_get_arg_type (&array) != DBUS_TYPE_INVALID)
	{
		DBusMessageIter modem;
		const char *path;
		char **newtab = realloc (tab, sizeof (*tab) * (i + 1));
		if (newtab == NULL)
			break;
		tab = newtab;

		dbus_message_iter_recurse (&array, &modem);
		if (dbus_message_iter_get_arg_type (&modem) != DBUS_TYPE_OBJECT_PATH)
			break;
		dbus_message_iter_get_basic (&modem, &path);
		tab[i++] = strdup (path);

		dbus_message_iter_next (&array);
	}
	dbus_message_unref (msg);

	*modemlist = tab;
	*modemcount = i;
	return name;
}

/*** oFono signal handling ***/

struct ofono_watch
{
	char *rule;
	char *path;
	char *interface;
	char *signal;
	char *arg0;

	plugin_t *p;
	ofono_signal_t cb;
	void *cbdata;
};

static DBusHandlerResult ofono_signal_matcher (DBusConnection *conn,
					       DBusMessage *msg,
					       void *user_data)
{
	ofono_watch_t *s = user_data;
	plugin_t *p = s->p;
	const char *data;

	if (dbus_message_get_type (msg) != DBUS_MESSAGE_TYPE_SIGNAL
	 || strcmp (dbus_message_get_sender (msg), s->p->name))
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	(void)conn;

	if (!dbus_message_has_interface (msg, s->interface))
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (s->signal
	 && !dbus_message_has_member (msg, s->signal))
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (s->arg0
	 && (!dbus_message_get_args (msg, NULL, DBUS_TYPE_STRING, &data,
				     DBUS_TYPE_INVALID)
	  || strcmp (s->arg0, data)))
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	pthread_mutex_lock (&p->modem_lock);
	if (dbus_message_has_path (msg, s->path ? s->path : p->modemv[p->modem]))
		s->cb (p, msg, s->cbdata);
	pthread_mutex_unlock (&p->modem_lock);
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void ofono_free_sigdata (void *data)
{
	ofono_watch_t *s = data;

	free (s->rule);
	free (s->path);
	free (s->interface);
	free (s->signal);
	free (s->arg0);

	free (s);
}

ofono_watch_t *ofono_signal_watch (plugin_t *p,
				   const char *path,
				   const char *subif,
				   const char *signal,
				   const char *arg0,
				   ofono_signal_t cb,
				   void *data)
{
	if (!subif)
		return NULL;

	ofono_watch_t *s = calloc (1, sizeof(ofono_watch_t));
	if (!s)
		return NULL;

	int canc = at_cancel_disable ();
	size_t len;
	FILE *rule = open_memstream (&s->rule, &len);

	if (!rule)
	{
		at_cancel_enable (canc);
		free (s);
		return NULL;
	}

	s->cb = cb;
	s->cbdata = data;
	s->p = p;

	fprintf (rule, "type='signal',interface='org.ofono.%s'", subif);

	s->interface = malloc (strlen (subif) + 11);
	sprintf (s->interface, "org.ofono.%s", subif);

	if (path)
	{
		fprintf (rule, ",path='%s'", path);
		s->path = strdup (path);
	}
	if (signal)
	{
		fprintf (rule, ",member='%s'", signal);
		s->signal = strdup (signal);
	}
	if (arg0)
	{
		fprintf (rule, ",arg0='%s'", arg0);
		s->arg0 = strdup (arg0);
	}

	fclose (rule);

	at_dbus_add_match (DBUS_BUS_SYSTEM, s->rule);
	at_dbus_add_filter (DBUS_BUS_SYSTEM, ofono_signal_matcher,
			    s, ofono_free_sigdata);
	at_cancel_enable (canc);
	return s;
}

void ofono_signal_unwatch (ofono_watch_t *s)
{
	at_dbus_remove_match (DBUS_BUS_SYSTEM, s->rule);
	at_dbus_remove_filter (DBUS_BUS_SYSTEM,
			       ofono_signal_matcher, s);
}

/*** oFono property change handling */

struct ofono_prop_watch
{
	int type;
	ofono_prop_t cb;
	void *cbdata;
	ofono_watch_t *watch;
};

static void ofono_prop_matcher (plugin_t *p, DBusMessage *msg, void *data)
{
	ofono_prop_watch_t *w = data;
	DBusMessageIter it, value;

	dbus_message_iter_init (msg, &it);
	assert (dbus_message_iter_get_arg_type (&it) == DBUS_TYPE_STRING);

	dbus_message_iter_next (&it);
	if (dbus_message_iter_get_arg_type (&it) != DBUS_TYPE_VARIANT)
	{
		error ("Property change malformatted");
		return;
	}

	dbus_message_iter_recurse (&it, &value);
	if (dbus_message_iter_get_arg_type (&value) != w->type)
	{
		error ("oFono \"%s.%s\" property type mismatch: wanted %d, got %d",
		       w->watch->interface, w->watch->arg0, w->type,
		       dbus_message_iter_get_arg_type (&it));
		return;
	}

	w->cb (p, &value, w->cbdata);
}

ofono_prop_watch_t *ofono_prop_watch (plugin_t *p, const char *path,
                                      const char *subif, const char *prop,
                                      int type, ofono_prop_t cb, void *data)
{
	ofono_prop_watch_t *w = malloc (sizeof (*w));
	if (w == NULL)
		return NULL;

	w->type = type;
	w->cb = cb;
	w->cbdata = data;
	w->watch = ofono_signal_watch (p, path, subif, "PropertyChanged", prop,
	                               ofono_prop_matcher, w);
	if (w->watch == NULL)
	{
		free (w);
		w = NULL;
	}

	return w;
}

void ofono_prop_unwatch (ofono_prop_watch_t *w)
{
	ofono_signal_unwatch (w->watch);
	free (w);
}


/*** Plugin registration ***/

void *at_plugin_register (at_commands_t *set)
{
	plugin_t *p = malloc (sizeof (*p));
	if (p == NULL)
		return NULL;

	p->name = manager_find (&p->modemv, &p->modemc);
	if (p->name == NULL || p->modemc == 0)
	{
		free (p->name);
		free (p);
		return NULL;
	}

	debug ("Using oFono %s", p->name);
	for (unsigned i = 0; i < p->modemc; i++)
		debug (" modem %u: %s", i, p->modemv[i]);
	p->modem = 0;
	pthread_mutex_init (&p->modem_lock, NULL);

	modem_register (set, p);
	agps_register (set, p);
	call_forwarding_register (set, p);
	call_meter_register (set, p);
	call_settings_register (set, p);
	gprs_register (set, p);
	network_register (set, p);
	sim_register (set, p);
	sms_register (set, p);
	voicecallmanager_register (set, p);
	at_register_ext (set, "*CNTI", set_cnti, NULL, list_cnti, p);
	return p;
}

void at_plugin_unregister (void *opaque)
{
	plugin_t *p = opaque;
	if (p == NULL)
		return;

	call_meter_unregister (p);
	gprs_unregister (p);
	network_unregister (p);
	voicecallmanager_unregister (p);
	pthread_mutex_destroy (&p->modem_lock);
	for (unsigned i = 0; i < p->modemc; i++)
		free (p->modemv[i]);
	free (p->modemv);
	free (p->name);
	free (p);
}
