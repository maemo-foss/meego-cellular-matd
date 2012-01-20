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
#include <at_log.h>

#include "ofono.h"
#include "core.h"

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

static at_error_t set_cmgf (at_modem_t *m, const char *req, void *data)
{
	plugin_t *p = data;
	unsigned mode;

	if (sscanf (req, " %u", &mode) != 1)
		mode = 0;
	if (mode > 1)
		return AT_CMS_ENOTSUP;

	p->text_mode = mode;
	(void)m;
	return AT_OK;
}

static at_error_t get_cmgf (at_modem_t *m, void *data)
{
	plugin_t *p = data;

	return at_intermediate (m, "\r\n+CMGF: %u", p->text_mode);
}

static at_error_t list_cmgf (at_modem_t *m, void *data)
{
	(void)data;
	return at_intermediate (m, "\r\n+CMGF: (0-1)");
}


/*** AT+CMGS ***/

static at_error_t send_text (at_modem_t *m, const char *req, void *data)
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

	char *utf8 = at_to_utf8 (m, text);
	free (text);
	if (utf8 == NULL)
		return AT_CMS_TXT_EINVAL;

	at_error_t ret = AT_OK;
	int canc = at_cancel_disable ();

	DBusMessage *msg = modem_req_new (p, "MessageManager", "SendMessage");
	if (msg != NULL
	 && !dbus_message_append_args (msg,
	                               DBUS_TYPE_STRING, &(const char *){ number },
	                               DBUS_TYPE_STRING, &utf8,
	                               DBUS_TYPE_INVALID))
	{
		dbus_message_unref (msg);
		msg = NULL;
	}
	free (utf8);
	if (msg == NULL)
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
		dbus_message_unref (msg);
		ret = AT_CMS_UNKNOWN;
		goto out;
	}
	/* This message reference is totally fake. FIXME? */
	if (sscanf (path, "%*[^_]_%2"SCNx8, &mr) != 1)
		mr = 0;
	at_intermediate (m, "\r\n+CMGS: %"PRIu8, mr);
out:
	at_cancel_enable (canc);
	return ret;
}

static int hexdigit (char c)
{
	if ((unsigned)(c - '0') < 10u)
		return c - '0';
	if ((unsigned)(c - 'A') < 6u)
		return c + 10 - 'A';
	if ((unsigned)(c - 'a') < 6u)
		return c + 10 - 'a';
	return -1;
}

static at_error_t send_pdu (at_modem_t *m, const char *req, void *data)
{
	unsigned len;

	if (sscanf (req, " %u", &len) != 1)
		return AT_CMS_PDU_EINVAL;

	plugin_t *p = data;
	/* Read hexadecimal PDU */
	char *pdu = at_read_text (m, "\r\n> ");
	if (pdu == NULL)
		return AT_OK;

	/* Convert to binary */
	dbus_uint32_t bytes = 0;
	for (char *in = pdu, *out = pdu; bytes < len; bytes++)
	{
		int hinib = hexdigit (*(in++));
		int lonib = hexdigit (*(in++));

		if (hinib < 0 || lonib < 0)
		{
			free (pdu);
			return AT_CMS_PDU_EINVAL;
		}

		*(out++) = (hinib << 4) | lonib;
	}

	debug ("sending SMS PDU (%u bytes)", (unsigned)bytes);

	at_error_t ret = AT_CMS_ENOMEM;
	int canc = at_cancel_disable ();

	DBusMessage *msg = modem_req_new (p, "MessageManager", "SendMessagePDU");
	if (msg == NULL)
		goto out;

	DBusMessageIter args, pdus, payload;
	dbus_message_iter_init_append (msg, &args);
	if (!dbus_message_iter_open_container (&args, DBUS_TYPE_ARRAY, "ay", &pdus)
	 || !dbus_message_iter_open_container (&pdus, DBUS_TYPE_ARRAY, "y",
	                                       &payload)
	 || !dbus_message_iter_append_fixed_array (&payload, DBUS_TYPE_BYTE,
	                                           &pdu, bytes)
	 || !dbus_message_iter_close_container (&pdus, &payload)
	 || !dbus_message_iter_close_container (&args, &pdus)
	 || !dbus_message_iter_open_container (&args, DBUS_TYPE_ARRAY, "{sv}",
	                                       &pdus)
	 || !dbus_message_iter_close_container (&args, &pdus))
	{
		dbus_message_unref (msg);
		goto out;
	}

	msg = ofono_query (msg, &ret);
	if (msg == NULL)
		goto out;

	const char *path;
	uint8_t mr;
	if (!dbus_message_get_args (msg, NULL,
	                            DBUS_TYPE_OBJECT_PATH, &path,
	                            DBUS_TYPE_INVALID))
	{
		dbus_message_unref (msg);
		ret = AT_CMS_UNKNOWN;
		goto out;
	}
	/* This message reference is totally fake. FIXME? */
	if (sscanf (path, "%*[^_]_%2"SCNx8, &mr) != 1)
		mr = 0;
	dbus_message_unref (msg);
	ret = at_intermediate (m, "\r\n+CMGS: %"PRIu8, mr);
out:
	at_cancel_enable (canc);
	return ret;
}


static at_error_t set_cmgs (at_modem_t *m, const char *req, void *data)
{
	plugin_t *p = data;
	at_set_t send_cb = p->text_mode ? send_text : send_pdu;

	return send_cb (m, req, data);
}


/*** AT+CMMS (stub) ***/

static at_error_t set_mms (at_modem_t *m, const char *req, void *data)
{
	unsigned n;

	if (sscanf (req, " %u", &n) != 1)
		return AT_CME_EINVAL;
	if (n != 2)
		return AT_CME_ENOTSUP;
	(void) m;
	(void) data;
	return AT_OK;
}

static at_error_t get_mms (at_modem_t *m, void *data)
{
	(void) data;
	return at_intermediate (m, "\r\n+CMMS: 2");
}

static at_error_t list_mms (at_modem_t *m, void *data)
{
	(void) data;
	return at_intermediate (m, "\r\n+CMMS: (2)");
}


/*** Registration ***/

void sms_register (at_commands_t *set, plugin_t *p)
{
	at_register_ext (set, "+CGSMS", set_cgsms, get_cgsms, list_cgsms, p);
	at_register_ext (set, "+CSMS", set_csms, get_csms, list_csms, p);
	at_register_ext (set, "+CSCA", set_csca, get_csca, NULL, p);
	p->text_mode = false;
	at_register_ext (set, "+CMGF", set_cmgf, get_cmgf, list_cmgf, p);
	at_register_ext (set, "+CMGS", set_cmgs, NULL, NULL, p);
	at_register_ext (set, "+CMMS", set_mms, get_mms, list_mms, p);
}
