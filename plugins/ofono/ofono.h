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

typedef struct plugin plugin_t;

/* D-Bus */
DBusMessage *modem_req_new (const plugin_t *, const char *, const char *);
at_error_t modem_request (const plugin_t *, const char *, const char *,
                          int, ...);

DBusMessage *modem_props_get (const plugin_t *, const char *);
char *modem_prop_get_string (const plugin_t *, const char *, const char *);
int modem_prop_get_bool (const plugin_t *, const char *, const char *);
int modem_prop_get_u16 (const plugin_t *, const char *, const char *);
at_error_t modem_prop_set_string (const plugin_t *, const char *,
                                  const char *, const char *);
at_error_t modem_prop_set_bool (const plugin_t *, const char *,
                                const char *, bool);
at_error_t modem_prop_set_u16 (const plugin_t *, const char *,
                               const char *, unsigned);

at_error_t voicecall_request (const plugin_t *, unsigned, const char *,
                              int, ...);

DBusMessage *ofono_query (DBusMessage *, at_error_t *);
DBusMessage *ofono_req_new (const plugin_t *, const char *,
				const char *, const char *);
at_error_t ofono_request (const plugin_t *, const char *,
			      const char *, const char *,
			      int, ...);

int ofono_dict_find (DBusMessageIter *, const char *, int, DBusMessageIter *);
const char *ofono_dict_find_string (DBusMessageIter *dict, const char *name);
int ofono_dict_find_bool (DBusMessageIter *dict, const char *name);
int ofono_prop_find (DBusMessage *, const char *, int, DBusMessageIter *);
const char *ofono_prop_find_string (DBusMessage *msg, const char *name);
int ofono_prop_find_bool (DBusMessage *msg, const char *name);

typedef void (*ofono_signal_t) (plugin_t *, DBusMessage *, void *);
typedef struct ofono_watch ofono_watch_t;
ofono_watch_t *ofono_signal_watch (plugin_t *, const char *, const char *,
				   const char *, const char *, ofono_signal_t,
				   void *);
void ofono_signal_unwatch (ofono_watch_t *);

/* Command handlers */
void modem_register (at_commands_t *, plugin_t *);
void call_forwarding_register (at_commands_t *, plugin_t *);
void call_settings_register (at_commands_t *, plugin_t *);
void gprs_register (at_commands_t *, plugin_t *);
void network_register (at_commands_t *, plugin_t *);
void sim_register (at_commands_t *, plugin_t *);
void voicecallmanager_register (at_commands_t *, plugin_t *);
void voicecallmanager_unregister (plugin_t *);
at_error_t handle_cnti (at_modem_t *, const char *, void *);
