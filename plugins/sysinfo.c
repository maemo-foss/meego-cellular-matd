/**
 * @file model.c
 * @brief AT commands for model infos
 * AT+GMI; +GMM; +GMR; +GCMR; +CGMM; CGMR
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <at_command.h>
#include <at_thread.h>
#include <at_dbus.h>
#include <sys/utsname.h>

static char *get_sysinfo (const char *key)
{
	DBusMessage *msg = dbus_message_new_method_call ("com.nokia.SystemInfo",
	                                                 "/com/nokia/SystemInfo",
	                                                 "com.nokia.SystemInfo",
	                                                 "GetConfigValue");
	if (msg == NULL)
		return NULL;
	if (!dbus_message_append_args (msg, DBUS_TYPE_STRING, &key,
	                               DBUS_TYPE_INVALID))
	{
		dbus_message_unref (msg);
		return NULL;
	}

	msg = at_dbus_request_reply (DBUS_BUS_SYSTEM, msg);
	if (msg == NULL)
		return NULL;

	const char *array;
	int length;
	char *ret;

	if (!dbus_message_get_args (msg, NULL, DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
	                            &array, &length, DBUS_TYPE_INVALID))
		ret = NULL;
	else
		ret = strndup (array, length);
	dbus_message_unref (msg);
	return ret;
}

static at_error_t handle_gmi (at_modem_t *modem, const char *req, void *data)
{
	(void) data;

	at_intermediate (modem, "\r\n"VENDOR"\r\n");

	(void) req;
	return AT_OK;
}

static at_error_t handle_gmm (at_modem_t *modem, const char *req, void *data)
{
	at_error_t ret = AT_ERROR;
	int canc = at_cancel_disable ();

	char *model = get_sysinfo ("/component/product-name");
	if (model != NULL)
	{
		at_intermediate (modem, "\r\n"VENDOR" %s\r\n", model);
		free (model);
		ret = AT_OK;
	}

	at_cancel_enable (canc);
	(void) req;
	(void) data;
	return ret;
}

static at_error_t handle_gmr (at_modem_t *modem, const char *req, void *data)
{
	struct utsname uts;
	if (uname (&uts))
		return AT_ERROR;

	int canc = at_cancel_disable ();

	char *name = get_sysinfo ("/component/product-name");
	char *code = get_sysinfo ("/component/product");
	char *hwid = get_sysinfo ("/component/hw-build");
	char *sw = get_sysinfo ("/device/sw-release-ver");
	if (sw != NULL)
	{
		char *ptr = sw;
		while ((ptr = strchr (ptr, '_')) != NULL)
			*ptr = ' ';
	}	

	at_intermediate (modem,
	                 "\r\n"VENDOR" %s (%s rev %s)"
	                 "\r\n%s"
	                 "\r\n%s version %s %s (%s)"
	                 "\r\n"PACKAGE" version "VERSION,
	                 name ? name : "XXX", code ? code : "NoRM",
	                 hwid ? hwid : "????", sw ? sw : "",
	                 uts.sysname, uts.release, uts.version, uts.machine);
	at_cancel_enable (canc);

	free (name);
	free (code);
	free (hwid);
	free (sw);

	/* Hook for modem revision */
	at_execute (modem, "*MATDMODEMMR");
	at_intermediate (modem, "\r\n");

	(void) req;
	(void) data;
	return AT_OK;
}

void *at_plugin_register (at_commands_t *set)
{
	at_register (set, "+CGMI", handle_gmi, NULL);
	at_register (set, "+CGMM", handle_gmm, NULL);
	at_register (set, "+CGMR", handle_gmr, NULL);

	return NULL;
}
