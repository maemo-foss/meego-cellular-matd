/**
 * @file ofono.h
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

#include <stdbool.h>
#include <stdint.h>
#include <dbus/dbus.h>

typedef struct plugin plugin_t;

/* D-Bus oFono modem helpers */
DBusMessage *modem_req_new (const plugin_t *, const char *, const char *);
at_error_t modem_request (const plugin_t *, const char *, const char *,
                          int, ...);

/* Get all properties of one modem atom (use with ofono_prop_find()) */
DBusMessage *modem_props_get (const plugin_t *, const char *iface);

/* Gets one modem property */
char *modem_prop_get_string (const plugin_t *, const char *, const char *);
int modem_prop_get_bool (const plugin_t *, const char *, const char *);
int modem_prop_get_byte (const plugin_t *, const char *, const char *);
int modem_prop_get_u16 (const plugin_t *, const char *, const char *);
int64_t modem_prop_get_u32 (const plugin_t *, const char *, const char *);

/* Set one modem property */
at_error_t modem_prop_set (const plugin_t *, const char *iface,
                           const char *name, int type, void *value,
                           const char *password);

static inline
at_error_t modem_prop_set_string (const plugin_t *p, const char *iface,
                                  const char *name, const char *value)
{
	return modem_prop_set (p, iface, name, DBUS_TYPE_STRING, &value, NULL);
}

static inline
at_error_t modem_prop_set_string_pw (const plugin_t *p, const char *iface,
                                     const char *name, const char *value,
                                     const char *password)
{
	return modem_prop_set (p, iface, name, DBUS_TYPE_STRING, &value, password);
}

static inline
at_error_t modem_prop_set_bool (const plugin_t *p, const char *iface,
                                const char *name, bool value)
{
	dbus_bool_t b = value;
	return modem_prop_set (p, iface, name, DBUS_TYPE_BOOLEAN, &b, NULL);
}

static inline
at_error_t modem_prop_set_u16 (const plugin_t *p, const char *iface,
                               const char *name, unsigned value)
{
	dbus_uint16_t u = value;
	return modem_prop_set (p, iface, name, DBUS_TYPE_UINT16, &u, NULL);
}

static inline
at_error_t modem_prop_set_u32_pw (const plugin_t *p, const char *iface,
                                  const char *name, unsigned value,
                                  const char *password)
{
	dbus_uint32_t u = value;
	return modem_prop_set (p, iface, name, DBUS_TYPE_UINT32, &u, password);
}

static inline
at_error_t modem_prop_set_double_pw (const plugin_t *p, const char *iface,
                                     const char *name, double value,
                                     const char *password)
{
	return modem_prop_set (p, iface, name, DBUS_TYPE_DOUBLE, &value, password);
}

/* D-Bus oFono voicecall helpers */
at_error_t voicecall_request (const plugin_t *, unsigned, const char *,
                              int, ...);

/* D-Bus oFono generic helpers */
DBusMessage *ofono_query (DBusMessage *, at_error_t *);
DBusMessage *ofono_req_new (const plugin_t *, const char *,
				const char *, const char *);
at_error_t ofono_request (const plugin_t *, const char *,
			      const char *, const char *,
			      int, ...);

/* Finds one entry in a string-indexed dictionary */
int ofono_dict_find (DBusMessageIter *, const char *, int, DBusMessageIter *);
int ofono_dict_find_basic (DBusMessageIter *, const char *, int, void *);

static inline
const char *ofono_dict_find_string (DBusMessageIter *dict, const char *name)
{
	const char *value;

	if (ofono_dict_find_basic (dict, name, DBUS_TYPE_STRING, &value))
		value = NULL;
	return value;
}

static inline
int ofono_dict_find_byte (DBusMessageIter *dict, const char *name)
{
	unsigned char b;

	if (ofono_dict_find_basic (dict, name, DBUS_TYPE_BYTE, &b))
		return -1;
	return b;
}

static inline
int ofono_dict_find_bool (DBusMessageIter *dict, const char *name)
{
	dbus_bool_t b;

	if (ofono_dict_find_basic (dict, name, DBUS_TYPE_BOOLEAN, &b))
		return -1;
	return b;
}

/* Finds one oFono property in a D-Bus message */
static inline
int ofono_prop_find (DBusMessage *msg, const char *name, int type,
                     DBusMessageIter *value)
{
	DBusMessageIter dict;

	if (!dbus_message_iter_init (msg, &dict))
		return -1;
	return ofono_dict_find (&dict, name, type, value);
}

static inline
const char *ofono_prop_find_string (DBusMessage *msg, const char *name)
{
	DBusMessageIter dict;

	if (!dbus_message_iter_init (msg, &dict))
		return NULL;
	return ofono_dict_find_string (&dict, name);
}

static inline
int ofono_prop_find_bool (DBusMessage *msg, const char *name)
{
	DBusMessageIter dict;

	if (!dbus_message_iter_init (msg, &dict))
		return -1;
	return ofono_dict_find_bool (&dict, name);
}

static inline
int ofono_prop_find_byte (DBusMessage *msg, const char *name)
{
	DBusMessageIter dict;

	if (!dbus_message_iter_init (msg, &dict))
		return -1;
	return ofono_dict_find_byte (&dict, name);
}

static inline
int ofono_prop_find_u16 (DBusMessage *msg, const char *name)
{
	DBusMessageIter dict;
	dbus_uint16_t val;

	if (!dbus_message_iter_init (msg, &dict)
	 || ofono_dict_find_basic (&dict, name, DBUS_TYPE_UINT16, &val))
		return -1;
	return val;
}

static inline
int64_t ofono_prop_find_u32 (DBusMessage *msg, const char *name)
{
	DBusMessageIter dict;
	dbus_uint32_t val;

	if (!dbus_message_iter_init (msg, &dict)
	 || ofono_dict_find_basic (&dict, name, DBUS_TYPE_UINT32, &val))
		return -1;
	return val;
}

static inline
double ofono_prop_find_double (DBusMessage *msg, const char *name)
{
	DBusMessageIter dict;
	double val;

	if (!dbus_message_iter_init (msg, &dict)
	 || ofono_dict_find_basic (&dict, name, DBUS_TYPE_DOUBLE, &val))
		val = -1.;
	return val;
}

/* D-Bus oFono signal handling */
typedef void (*ofono_signal_t) (plugin_t *, DBusMessage *, void *);
typedef struct ofono_watch ofono_watch_t;
ofono_watch_t *ofono_signal_watch (plugin_t *, const char *, const char *,
				   const char *, const char *, ofono_signal_t,
				   void *);
void ofono_signal_unwatch (ofono_watch_t *);

/* Helper to print registration status */
at_error_t ofono_netreg_print (at_modem_t *modem, plugin_t *p,
                               const char *prefix, int n);


/* Misc */
bool utf8_validate_string (const char *str);

/* Command handlers */
void modem_register (at_commands_t *, plugin_t *);
void agps_register (at_commands_t *, plugin_t *);
void call_forwarding_register (at_commands_t *, plugin_t *);
void call_meter_register (at_commands_t *, plugin_t *);
void call_meter_unregister (plugin_t *);
void call_settings_register (at_commands_t *, plugin_t *);
void gprs_register (at_commands_t *, plugin_t *);
void gprs_unregister (plugin_t *);
void network_register (at_commands_t *, plugin_t *);
void network_unregister (plugin_t *);
void sim_register (at_commands_t *, plugin_t *);
void sms_register (at_commands_t *, plugin_t *);
void voicecallmanager_register (at_commands_t *, plugin_t *);
void voicecallmanager_unregister (plugin_t *);
at_error_t set_cnti (at_modem_t *, const char *, void *);
at_error_t list_cnti (at_modem_t *, void *);
