/**
 * @file at_dbus.h
 * @brief D-Bus helper functions
 * @ingroup dbus
 *
 * @defgroup dbus DBus
 * @ingroup helpers
 * @{
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

#ifndef MATD_AT_DBUS_H
# define MATD_AT_DBUS_H 1

# include <dbus/dbus.h>

# ifdef __cplusplus
extern "C" {
# endif

/**
 * Sends a blocking DBus method call and waits for the reply.
 * The D-Bus error is initialized first (no need for the caller to do that).
 * @param type DBus bus to use
 * @param req DBus method call
 * @param timeout response time out (ms), INT_MAX for infinite, -1 for default
 * @param err DBus error buffer
 * @return NULL on error or a DBus reply
 */
DBusMessage *at_dbus_query (DBusBusType type, DBusMessage *req, int timeout,
                            DBusError *err);

/**
 * Sends a blocking DBus method call and waits for the reply.
 * @param type DBus bus to use
 * @param req DBus method call
 * @return NULL on error or a DBus reply
 */
DBusMessage *at_dbus_request_reply (DBusBusType type, DBusMessage *req);

/**
 * Sends a DBus message not soliciting a response.
 * @param type DBus bus to use
 * @param msg DBus message to be sent
 * @return 0 on success, -1 on error
 */
int at_dbus_request (DBusBusType type, DBusMessage *msg);

/**
 * Registers a DBus message filter. This is used to receive DBus signals.
 * @param bus DBus bus to filter message from
 * @param filter_cb message filter callback function
 * @param opaque data for the callback
 * @param free_cb callback to free the data
 * @return 0 on success, -1 on error.
 */
int at_dbus_add_filter (DBusBusType bus, DBusHandleMessageFunction filter_cb,
                        void *opaque, DBusFreeFunction free_cb);

/**
 * Unregisters a DBus message filter. The message filter must have been
 * registered succesfully earlier (this is a limitation of libdbus).
 * @param bus DBus bus that the filter was registered on
 * @param filter_cb filter callback to remove
 * @param opaque data for the to-be-removed callback
 * @return nothing. This function always succeeds.
 */
void at_dbus_remove_filter (DBusBusType bus,
                            DBusHandleMessageFunction filter_cb, void *opaque);

/**
 * Adds a message match filter to a bus.
 * @param bus DBus bus type
 * @param rule DBus rule (with the standard DBus match syntax)
 * @return nothing. This functions fails silently.
 */
void at_dbus_add_match (DBusBusType bus, const char *rule);

/**
 * Removes a message match filter to a bus.
 * @param bus DBus bus type
 * @param rule DBus rule (with the standard DBus match syntax)
 */
void at_dbus_remove_match (DBusBusType bus, const char *rule);

/**
 * Looks up a value in a string-indexed DBus dictionary.
 * @param it DBus iterator on the array containing the dictionary [IN]
 * @param name key to look for
 * @param value DBus iterator to the value, if found [OUT]
 * @return 0 on success, -1 on error.
 */
int at_dbus_dict_lookup_string (DBusMessageIter *it, const char *name,
                                DBusMessageIter *value);

/** @} */

# ifdef __cplusplus
}
# endif
#endif
