/**
 * @file sms.c
 * @brief Message (manager) commands with oFono
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

#include <at_command.h>
#include <at_thread.h>

#include "ofono.h"

/*** AT+CGSMS ***/

static const char bearers[][16] =
{
	"ps-only",
	"cs-only",
	"ps-preferred",
	"cs-preferred",
};

static const size_t max_bearer = sizeof (bearers) / sizeof (bearers[0]);

static at_error_t set_cgsms (at_modem_t *modem, const char *req, void *data)
{
	plugin_t *p = data;
	unsigned service;

	(void) modem;

	if (sscanf (req, " %u", &service) != 1 || service > max_bearer)
		return AT_ERROR;

	return modem_prop_set_string (p, "MessageManager", "Bearer",
	                              bearers[service]);
}

static at_error_t get_cgsms (at_modem_t *modem, void *data)
{
	plugin_t *p = data;
	char *bearer = modem_prop_get_string (p, "MessageManager", "Bearer");
	at_error_t ret = AT_ERROR;

	if (bearer != NULL)
	{
		for (unsigned service = 0; service < max_bearer; service++)
			if (!strcmp (bearer, bearers[service]))
			{
				at_intermediate (modem, "\r\n+CGSMS: %u", service);
				ret = AT_OK;
				break;
			}
		free (bearer);
	}
	return ret;
}

static at_error_t list_cgsms (at_modem_t *modem, void *data)
{
	at_intermediate (modem, "\r\n+CGSMS: (0-3)");
	(void) data;
	return AT_OK;
}


/*** AT+CSMS ***/

static at_error_t set_csms (at_modem_t *modem, const char *req, void *data)
{
	unsigned service;

	if (sscanf (req, " %u", &service) != 1)
		return AT_ERROR;
	if (service > 0)
		return AT_CMS_ENOTSUP;
	(void) modem;
	(void) data;
	return at_intermediate (modem, "\r\n+CSMS: 1,1,1");
}

static at_error_t get_csms (at_modem_t *modem, void *data)
{
	(void) data;
	return at_intermediate (modem, "\r\n+CSMS: 0,1,1,1");
}

static at_error_t list_csms (at_modem_t *modem, void *data)
{
	(void) data;
	return at_intermediate (modem, "\r\n+CSMS: (0)");
}


/*** AT+CSCA ***/

static at_error_t set_csca (at_modem_t *modem, const char *req, void *data)
{
	plugin_t *p = data;
	char number[256];
	unsigned type;

	(void) modem;

	switch (sscanf (req, " \"%255[^\"]\" , %u", number, &type))
	{
		case 2:
			if (type != (*number == '+') ? 145 : 129)
				return AT_CMS_ENOTSUP;
			/* fallthrough */
		case 1:
			break;
		default:
			return AT_ERROR;
	}

	return modem_prop_set_string (p, "MessageManager", "ServiceCenterAddress",
	                              number);
}

static at_error_t get_csca (at_modem_t *modem, void *data)
{
	plugin_t *p = data;
	char *number = modem_prop_get_string (p, "MessageManager",
	                                      "ServiceCenterAddress");

	if (number == NULL)
		return AT_CMS_UNKNOWN;

	at_intermediate (modem, "\r\n+CSCA: \"%s\",%u", number,
	                 (*number == '+') ? 145 : 129);
	free (number);
	return AT_OK;
}


/*** AT+CMGF ***/

/* TODO: PDU mode */

static at_error_t set_cmgf (at_modem_t *m, const char *req, void *data)
{
	unsigned mode;

	if (sscanf (req, " %u", &mode) != 1)
		return AT_ERROR;
	if (mode != 1)
		return AT_CMS_ENOTSUP;

	(void)m;
	(void)data;
	return AT_OK;
}

static at_error_t get_cmgf (at_modem_t *m, void *data)
{
	(void)data;
	return at_intermediate (m, "\r\n+CMGF: 1");
}

static at_error_t list_cmgf (at_modem_t *m, void *data)
{
	(void)data;
	return at_intermediate (m, "\r\n+CMGF: (1)");
}


/*** AT+CMGS ***/

static at_error_t set_cmgs (at_modem_t *m, const char *req, void *data)
{
	char number[21];
	unsigned type;

	switch (sscanf (req, " \"%20[^\"]\" , %u", number, &type))
	{
		case 2:
			if (type != (*number == '+') ? 145 : 129)
				return AT_CMS_ENOTSUP;
			/* fallthrough */
		case 1:
			break;
		default:
			return AT_CMS_TXT_EINVAL;
	}

	plugin_t *p = data;
	char *text = at_read_text (m, "\r\n> ");
	if (text == NULL)
		return AT_OK;

	at_error_t ret = AT_OK;
	int canc = at_cancel_disable ();

	DBusMessage *msg = modem_req_new (p, "MessageManager", "SendMessage");
	if (msg == NULL
	 || !dbus_message_append_args (msg,
	                               DBUS_TYPE_STRING, &(const char *){ number },
	                               DBUS_TYPE_STRING, &text,
	                               DBUS_TYPE_INVALID))
	{
		ret = AT_CMS_ENOMEM;
		goto out;
	}

	msg = ofono_query (msg, &ret);
	if (ret != AT_OK)
		goto out;

	const char *path;
	uint8_t mr;
	if (!dbus_message_get_args (msg, NULL,
	                            DBUS_TYPE_OBJECT_PATH, &path,
	                            DBUS_TYPE_INVALID))
	{
		ret = AT_CMS_UNKNOWN;
		goto out;
	}
	/* This message reference is totally fake. FIXME? */
	if (sscanf (path, "%*[^_]_%2"SCNx8, &mr) != 1)
		mr = 0;
	at_intermediate (m, "\r\n+CMGS: %"PRIu8, mr);
out:
	if (msg != NULL)
		dbus_message_unref (msg);
	at_cancel_enable (canc);
	return ret;
}


/*** Registration ***/

void sms_register (at_commands_t *set, plugin_t *p)
{
	at_register_ext (set, "+CGSMS", set_cgsms, get_cgsms, list_cgsms, p);
	at_register_ext (set, "+CSMS", set_csms, get_csms, list_csms, p);
	at_register_ext (set, "+CSCA", set_csca, get_csca, NULL, p);
	at_register_ext (set, "+CMGF", set_cmgf, get_cmgf, list_cmgf, p);
	at_register_ext (set, "+CMGS", set_cmgs, NULL, NULL, p);
}
