/**
 * @file ss.c
 * @brief USSD commands with oFono
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
 * Copyright (C) 2010-2011 Nokia Corporation and/or its subsidiary(-ies).
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
#include <inttypes.h>
#include <string.h>

#include <at_command.h>
#include <at_log.h>
#include "ofono.h"
#include "core.h"

/** Gets the number of "bits" from Data Coding Scheme. */
static int cb_dcs (uint_fast8_t dcs)
{
	unsigned hi = (dcs >> 4) & 0xF, lo = dcs & 0xF;

	switch (hi)
	{
		case 0:
		case 2:
		case 3:
			return 7;
		case 1:
			if (lo > 1)
				break;
			return lo ? 16 : 7;
		case 4:
		case 5:
		case 6:
		case 7:
		case 9:
			switch (lo >> 2)
			{
				case 0:
					return 7;
				case 1:
					return 8;
				case 2:
					return 16;
			}
			break;
		case 15:
			switch (lo >> 2)
			{
				case 0:
					return 7;
				case 1:
					return 8;
			}
			break;
	}
	return -1;
}

static int hexdigit (unsigned char c)
{
	if ((c - '0') < 10)
		return c - '0';
	if ((c - 'A') < 6)
		return c - ('A' - 10);
	if ((c - 'a') < 6)
		return c - ('a' - 10);
	return -1;
}

/** Decodes hexadecimal-encoded UCS-2 to UTF-8. */
static char *sms16_decode (const char *str)
{
	size_t len = strlen (str) / 4;
	char *out = malloc (3 * len + 1), *p = out;
	if (out == NULL)
		return NULL;

	for (size_t i = 0; str[i] != '\0'; i += 4)
	{
		int v;
		uint_fast16_t cp = 0;

		/* Convert 4 nibbles to a code point */
		v = hexdigit (str[i]);
		if (v < 0)
			goto err;
		cp |= v << 12;
		v = hexdigit (str[i + 1]);
		if (v < 0)
			goto err;
		cp |= v << 8;
		v = hexdigit (str[i + 2]);
		if (v < 0)
			goto err;
		cp |= v << 4;
		v = hexdigit (str[i + 2]);
		if (v < 0)
			goto err;
		cp |= v;

		/* Convert the code point to 1-3 UTF-8 bytes */
		if (cp < 0x80)
		{
			if (cp == 0)
				goto err;
			*(p++) = cp; /* ASCII */
		}
		else if (cp < 0x800)
		{
			*(p++) = 0xC0 | (cp >> 6);
			*(p++) = 0x80 | (cp & 0x3F);
		}
		else
		{
			if (0xD800 <= cp && cp < 0xE000)
				return NULL; /* surrogate!? */
			*(p++) = 0xE0 | (cp >> 12);
			*(p++) = 0x80 | ((cp >> 6) & 0x3F);
			*(p++) = 0x80 | (cp & 0x3F);
		}
	}
	*p = '\0';
	return out;
err:
	free (out);
	return NULL;
}

/** Decodes to UTF-8. */
static char *cb_decode (at_modem_t *modem, uint_fast8_t dcs, const char *str)
{
	switch (cb_dcs (dcs))
	{
		case 7:
			return at_to_utf8 (modem, str);
		case 8:
			return NULL; /* FIXME */
		case 16:
			return sms16_decode (str);
	}
	return NULL;
}

/*** AT+CUSD ***/

static void ussd_cb (plugin_t *p, DBusMessage *msg, void *data)
{
	at_modem_t *modem = data;
	const char *event = dbus_message_get_member (msg);
	unsigned m;

	if (!strcmp (event, "NotificationReceived"))
		m = 0;
	else
	if (!strcmp (event, "RequestReceived"))
		m = 1;
	else
	if (!strcmp (event, "ResponseReceived"))
		m = 2;
	else
		return;

	const char *message;
	if (!dbus_message_get_args (msg, NULL, DBUS_TYPE_STRING, &message,
	                            DBUS_TYPE_INVALID))
		return;

	/* FIXME: race prone (AT+CSCS could be in processing) */
	char *str = at_from_utf8 (modem, message);
	if (str != NULL)
	{
		at_unsolicited (modem, "\r\n+CUSD: %u,\"%s\",0\r\n", m, str);
		free (str);
	}
	(void) p;
}

static at_error_t set_ussd (at_modem_t *modem, const char *req, void *data)
{
	plugin_t *p = data;
	char buf[160], *const str = buf;
	unsigned n;
	uint8_t dcs;

	switch (sscanf (req, " %u , \"%159[^\"]\" , %"SCNd8, &n, str, &dcs))
	{
		case 0:
			n = 0;
		case 1:
			*str = '\0';
		case 2:
			dcs = 0;
		case 3:
			break;
		default:
			return AT_CME_EINVAL;
	}

	switch (n)
	{
		case 0:
			if (p->ussd_filter == NULL)
				break;
			ofono_signal_unwatch (p->ussd_filter);
			p->ussd_filter = NULL;
			break;
		case 1:
			if (p->ussd_filter != NULL)
				break;
			p->ussd_filter = ofono_signal_watch (p, OFONO_MODEM,
			                                     "SupplementaryServices",
				                                 NULL, NULL, ussd_cb, modem);
			if (p->ussd_filter == NULL)
				return AT_CME_ENOMEM;
			break;
		case 2:
			if (*str)
				return AT_CME_ENOTSUP;
			return modem_request (p, "SupplementaryServices", "Cancel",
			                      DBUS_TYPE_INVALID);
		default:
			return AT_CME_ENOTSUP;
	}

	if (!*str)
		return AT_OK;

	char *u8 = cb_decode (modem, dcs, str);
	if (u8 == NULL)
		return AT_CME_ENOTSUP;

	char *state = modem_prop_get_string (p, "SupplementaryServices", "State");
	at_error_t ret;

	/* FIXME: send request, do not wait for answer */
	/* FIXME: do print result */
	if (state == NULL)
		ret = AT_CME_UNKNOWN;
	else if (!strcmp (state, "idle"))
		ret = modem_request (p, "SupplementaryServices", "Command",
		                     DBUS_TYPE_STRING, &u8, DBUS_TYPE_INVALID);
	else if (!strcmp (state, "active"))
		ret = AT_CME_EBUSY;
	else if (!strcmp (state, "user-response"))
		ret = modem_request (p, "SupplementaryServices", "Respond",
		                     DBUS_TYPE_STRING, &u8, DBUS_TYPE_INVALID);
	else
	{
		error ("Unknown supplementary services state \"%s\"", state);
		ret = AT_CME_UNKNOWN;
	}
	free (u8);
	free (state);
	return ret;
}

static at_error_t get_ussd (at_modem_t *m, void *data)
{
	plugin_t *p = data;

	return at_intermediate (m, "\r\n+CUSD: %u", p->ussd_filter != NULL);
}

static at_error_t list_ussd (at_modem_t *m, void *data)
{
	(void) m; (void) data;
	return at_intermediate (m, "\r\n+CUSD: (0-2)");
}


/*** Registration ***/

void ss_register (at_commands_t *set, plugin_t *p)
{
	p->ussd_filter = NULL;
	at_register_ext (set, "+CUSD", set_ussd, get_ussd, list_ussd, p);
}

void ss_unregister (plugin_t *p)
{
	if (p->ussd_filter != NULL)
		ofono_signal_unwatch (p->ussd_filter);
}
