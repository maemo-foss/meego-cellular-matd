/**
 * @file modem.c
 * @brief oFono modem commands
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

#include <at_command.h>
#include <at_thread.h>
#include <dbus/dbus.h>
#include "ofono.h"


/*** AT+CFUN ***/

static at_error_t set_cfun (at_modem_t *modem, const char *req, void *data)
{
	plugin_t *p = data;
	unsigned fun, rst;

	switch (sscanf (req, " %u , %u", &fun, &rst))
	{
		case 1:
			rst = 0;
		case 2:
			break;
		default:
			return AT_CME_EINVAL;
	}
	if (fun > 127 || rst > 1)
		return AT_CME_EINVAL;
	if (fun > 4 || fun == 2 || fun == 3)
		return AT_CME_ENOTSUP;

	if (fun == 0)
		return modem_prop_set_bool (p, "Modem", "Powered", false);

	at_error_t ret;

	/* Ideally, the state would be changed while the modem is not powered,
	 * to ensure atomicity. But oFono does not support that. */
	if (rst)
	{	/* Reset requested: power off first */
		ret = modem_prop_set_bool (p, "Modem", "Powered", false);
		if (ret != AT_OK)
			return ret;
	}

	ret = modem_prop_set_bool (p, "Modem", "Powered", true);
	if (ret != AT_OK)
		return ret;

	(void) modem;
	return modem_prop_set_bool (p, "Modem", "Online", fun == 1);
}

static at_error_t get_cfun (at_modem_t *modem, void *data)
{
	plugin_t *p = data;
	DBusMessage *msg;
	at_error_t ret = AT_CME_UNKNOWN;
	int canc = at_cancel_disable ();

	msg = modem_props_get (p, "Modem");
	if (msg == NULL)
		goto out;

	int fun = -1;
	switch (ofono_prop_find_bool (msg, "Powered"))
	{
		case 0:
			fun = 0;
			break;
		case 1:
			switch (ofono_prop_find_bool (msg, "Online"))
			{
				case 0:
					fun = 4;
					break;
				case 1:
					fun = 1;
					break;
			}
	}

	if (fun >= 0)
	{
		at_intermediate (modem, "\r\n+CFUN: %d", fun);
		ret = AT_OK;
	}
	dbus_message_unref (msg);
out:
	at_cancel_enable (canc);
	return ret;
}

static at_error_t list_cfun (at_modem_t *modem, void *data)
{
	at_intermediate (modem, "\r\n+CFUN: (0,1,4),(0,1)");
	(void) data;
	return AT_OK;
}

static at_error_t handle_cfun (at_modem_t *modem, const char *req, void *data)
{
	return at_setting (modem, req, data, set_cfun, get_cfun, list_cfun);
}


/*** AT+CGSN ***/

static at_error_t show_gsn (at_modem_t *modem, const char *req, void *data)
{
	if (*req)
		return AT_CME_EINVAL;

	char *imei = modem_prop_get_string (data, "Modem", "Serial");
	if (imei == NULL)
		return AT_CME_UNKNOWN;

	at_intermediate (modem, "\r\n%s\r\n", imei);
	free (imei);
	(void) req;
	return AT_OK;
}

static at_error_t handle_gsn (at_modem_t *modem, const char *req, void *data)
{
	return at_setting (modem, req, data, show_gsn, NULL, NULL);
}


/*** Modem revision ***/

static at_error_t handle_gmr (at_modem_t *modem, const char *req, void *data)
{
	char *revision = modem_prop_get_string (data, "Modem", "Revision");
	if (revision == NULL)
		return AT_ERROR;

	for (char *p = strchr (revision, '\n'); p != NULL; p = strchr (p, '\n'))
		*(p++) = ' ';

	at_intermediate (modem, "\r\nModem %s", revision);
	free (revision);
	(void) req;
	return AT_OK;
}


/*** Modem atom registration ***/

void modem_register (at_commands_t *set, plugin_t *p)
{
	at_register (set, "+CFUN", handle_cfun, p);
	at_register (set, "+CGSN", handle_gsn, p);
	at_register (set, "+GSN", handle_gsn, p);
	at_register (set, "*OFGMR", handle_gmr, p);
}
