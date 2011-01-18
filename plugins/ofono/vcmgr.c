/**
 * @file voicecall.c
 * @brief voice call commands with oFono
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

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <dbus/dbus.h>

#include <at_command.h>
#include <at_thread.h>
#include <at_log.h>
#include "ofono.h"
#include "core.h"

static at_error_t handle_dial (at_modem_t *modem, const char *str, void *data)
{
	plugin_t *p = data;
	char buf[256], *num;
	const char *callerid = "";

	for (num = buf; *str; str++)
	{
		char c = *str;

		if ((c >= '0' && c <= '9') || c == '*' || c == '#' || c == '+'
		 || (c >= 'A' && c <= 'C'))
		{
			*(num++) = c;
			if (num >= (buf + sizeof (buf)))
				return AT_ERROR;
		}
		else if (c == 'I')
			callerid = "enabled";
		else if (c == 'i')
			callerid = "disabled";
		else if (c == 'G' || c == 'g')
			return AT_CME_ENOTSUP; /* XXX? */
		else if (c == '>')
			return AT_CME_ENOENT; /* phonebook -> reject */
	}
	*num = '\0';
	num = buf;

	(void) modem;

	/* FIXME TODO: signal call progress asynchronously */
	return modem_request (p, "VoiceCallManager", "Dial",
	                      DBUS_TYPE_STRING, &num, DBUS_TYPE_STRING, &callerid,
	                      DBUS_TYPE_INVALID);
}


static DBusMessage *get_calls (plugin_t *p, at_error_t *ret)
{
	DBusMessage *msg = modem_req_new (p, "VoiceCallManager", "GetCalls");
	if (msg == NULL)
		*ret = AT_CME_ENOMEM;
	else
		msg = ofono_query (msg, ret);
	return msg;
}

static int get_call_id (const char *call)
{
	unsigned id;

	if (sscanf (call, "/%*[a-z0-9]/%*[a-z]%u", &id) != 1)
	{
		error ("Cannot assign number for call %s", call);
		return -1;
	}
	return id;
}


/*** AT+CLCC ***/

static at_error_t handle_clcc (at_modem_t *modem, const char *req, void *data)
{
	plugin_t *p = data;
	at_error_t ret;
	int canc = at_cancel_disable ();

	DBusMessage *msg = get_calls (p, &ret);
	if (msg == NULL)
		goto out;

	DBusMessageIter args, array;

	if (!dbus_message_iter_init (msg, &args)
	 || dbus_message_iter_get_arg_type (&args) != DBUS_TYPE_ARRAY
	 || dbus_message_iter_get_element_type (&args) != DBUS_TYPE_STRUCT)
	{
		dbus_message_unref (msg);
		ret = AT_CME_ERROR_0;
		goto out;
	}

	dbus_message_iter_recurse (&args, &array);
	while (dbus_message_iter_get_arg_type (&array) != DBUS_TYPE_INVALID)
	{
		const char *path, *number = NULL;
		unsigned id;
		int stat = -1;
		dbus_bool_t orig = 0, mpty = 0;

		DBusMessageIter call;
		dbus_message_iter_recurse (&array, &call);
		if (dbus_message_iter_get_arg_type (&call) != DBUS_TYPE_OBJECT_PATH)
			continue;
		dbus_message_iter_get_basic (&call, &path);
		id = get_call_id (path);

		dbus_message_iter_next (&call);
		if (dbus_message_iter_get_arg_type (&call) != DBUS_TYPE_ARRAY
		 || dbus_message_iter_get_element_type (&call) != DBUS_TYPE_DICT_ENTRY)
                continue;

		DBusMessageIter props;
        dbus_message_iter_recurse (&call, &props);

        while (dbus_message_iter_get_arg_type (&props) != DBUS_TYPE_INVALID)
        {
			DBusMessageIter prop, value;
			const char *key;

			dbus_message_iter_recurse (&props, &prop);
			if (dbus_message_iter_get_arg_type (&prop) != DBUS_TYPE_STRING)
				break; /* wrong dictionary key type */
			dbus_message_iter_get_basic (&prop, &key);
			dbus_message_iter_next (&prop);
			if (dbus_message_iter_get_arg_type (&prop) != DBUS_TYPE_VARIANT)
				break;
			dbus_message_iter_recurse (&prop, &value);

			if (!strcmp (key, "LineIdentification")
			 && dbus_message_iter_get_arg_type (&value) == DBUS_TYPE_STRING)
				dbus_message_iter_get_basic (&value, &number);
			else
			if (!strcmp (key, "Originated")
			 && dbus_message_iter_get_arg_type (&value) == DBUS_TYPE_BOOLEAN)
				dbus_message_iter_get_basic (&value, &orig);
			else
			if (!strcmp (key, "Multiparty")
			 && dbus_message_iter_get_arg_type (&value) == DBUS_TYPE_BOOLEAN)
				dbus_message_iter_get_basic (&value, &mpty);
			else
			if (!strcmp (key, "State")
			 && dbus_message_iter_get_arg_type (&value) == DBUS_TYPE_STRING)
			{
				const char *state;

				dbus_message_iter_get_basic (&value, &state);
				if (!strcmp (state, "active"))
					stat = 0;
				else if (!strcmp (state, "held"))
					stat = 1;
				else if (!strcmp (state, "dialing"))
					stat = 2;
				else if (!strcmp (state, "alerting"))
					stat = 3;
				else if (!strcmp (state, "incoming"))
					stat = 4;
				else if (!strcmp (state, "waiting"))
					stat = 5;
			}
			dbus_message_iter_next (&props);
		}

		if (stat == -1)
			continue;

		if (number != NULL && strcmp (number, "withheld"))
			at_intermediate (modem, "\r\n+CLCC: %u,%u,%d,0,%u,\"%s\",%u", id,
			                 !orig, stat, mpty, number,
			                 (number[0] == '+') ? 145 : 129);
		else
			at_intermediate (modem, "\r\n+CLCC: %u,%u,%u,0,%u", id,
			                 !orig, stat, mpty);
		dbus_message_iter_next (&array);
	}

	ret = AT_OK;
out:
	at_cancel_enable (canc);
	(void) req;
	return ret;
}


/*** AT+CHUP  ***/

/* XXX: review AT+CHUP and ATH when data mode is implemented */
static at_error_t handle_chup (at_modem_t *modem, const char *req, void *data)
{
	plugin_t *p = data;
	at_error_t ret;

	ret = modem_request (p, "VoiceCallManager", "ReleaseAndAnswer",
	                     DBUS_TYPE_INVALID);
	if (ret == AT_CME_ERROR_0)
		ret = AT_OK; /* there was no ongoing call */

	(void) modem; /* maybe NULL with AT+CVHU=2 */
	(void) req;
	return ret;
}


/*** ATH ***/

static at_error_t handle_hangup (at_modem_t *modem, unsigned val, void *data)
{
	plugin_t *p = data;

	if (val != 0)
		return AT_CME_ENOTSUP;

	if (p->vhu == 1)
		return AT_OK; /* ignore ATH */

	/* We don't do alternating calls, so ATH is the same as AT+CHUP */
	return handle_chup (modem, "+CHUP", data) ? AT_ERROR : AT_OK;
}


/*** AT+CMOD (dummy) ***/

static at_error_t set_zero (at_modem_t *modem, const char *req, void *data)
{
	unsigned mode;

	if (sscanf (req, " %u", &mode) != 1)
		return AT_CME_EINVAL;
	if (mode != 0)
		return AT_CME_ENOTSUP;
	(void) modem;
	(void) data;
	return AT_OK;
}

static at_error_t get_cmod (at_modem_t *modem, void *data)
{
	at_intermediate (modem, "\r\n+CMOD: 0");
	(void) data;
	return AT_OK;
}

static at_error_t list_cmod (at_modem_t *modem, void *data)
{
	at_intermediate (modem, "\r\n+CMOD: (0)");
	(void) data;
	return AT_OK;
}

static at_error_t handle_cmod (at_modem_t *modem, const char *req, void *data)
{
	return at_setting (modem, req, data, set_zero, get_cmod, list_cmod);
}


/*** AT+CVMOD (dummy) ***/

static at_error_t get_cvmod (at_modem_t *modem, void *data)
{
	at_intermediate (modem, "\r\n+CVMOD: 0");
	(void) data;
	return AT_OK;
}

static at_error_t list_cvmod (at_modem_t *modem, void *data)
{
	at_intermediate (modem, "\r\n+CVMOD: (0)");
	(void) data;
	return AT_OK;
}

static at_error_t handle_cvmod (at_modem_t *modem, const char *req, void *data)
{
	return at_setting (modem, req, data, set_zero, get_cvmod, list_cvmod);
}

/*** AT+CHLD ***/

static at_error_t set_chld (at_modem_t *modem, const char *value, void *data)
{
	plugin_t *p = data;
	const char *method;

	value += strspn (value, " ");

	if (!*value)
		return AT_CME_ENOTSUP;

	/* 1x/2x not supported yet. */
	if (value[1])
		return AT_CME_ENOTSUP;

	switch (*value) {
		case '1':
			method = "ReleaseAndAnswer";
			break;
		case '2':
			method = "SwapCalls";
			break;
		case '3':
			method = "CreateMultiparty";
			break;
		case '4':
			method = "Transfer";
			break;
		default:
			return AT_CME_ENOTSUP;
	}

	(void)modem;

	return modem_request (p, "VoiceCallManager", method, DBUS_TYPE_INVALID);
}

static at_error_t get_chld (at_modem_t *modem, void *data)
{
	(void)modem;
	(void)data;

	return AT_CME_EINVAL;
}

static at_error_t list_chld (at_modem_t *modem, void *data)
{
	at_intermediate (modem, "\r\n+CHLD: (1,2,3,4)");

	(void)data;
	return AT_OK;
}

static at_error_t handle_chld (at_modem_t *modem, const char *req, void *data)
{
	return at_setting (modem, req, data, set_chld, get_chld, list_chld);
}


/*** AT+CVHU ***/

/* Contrary to 3GPP TS 27.007, OK is not returned on DTR drop.
 * We can typically not write "OK" to the serial line when DTR is low,
 * and the DTE would probably ignore any data we send. */

static at_error_t set_cvhu (at_modem_t *modem, const char *req, void *data)
{
	unsigned char *pvhu = data;
	unsigned char mode;

	if (sscanf (req, " %"SCNu8, &mode) != 1)
		return AT_CME_EINVAL;
	if (mode > 2)
		return AT_CME_ENOTSUP;

	*pvhu = mode;

	(void) modem;
	return AT_OK;
}

static at_error_t get_cvhu (at_modem_t *modem, void *data)
{
	unsigned char *pvhu = data;

	at_intermediate (modem, "\r\n+CVHU: %"PRIu8, *pvhu);
	return AT_OK;
}

static at_error_t list_cvhu (at_modem_t *modem, void *data)
{
	(void) data;

	at_intermediate (modem, "\r\n+CMOD: (0-2)");
	return AT_OK;
}

static at_error_t handle_cvhu (at_modem_t *modem, const char *req, void *data)
{
	return at_setting (modem, req, data, set_cvhu, get_cvhu, list_cvhu);
}

/*** AT+VTS ***/

static at_error_t set_vts (at_modem_t *m, const char *req, void *data)
{
	plugin_t *p = data;

	char buf[256];
	char *tones = buf;

	while (*req)
	{
		int len = -1;
		int len_wc = -1;
		unsigned dur;
		char ch;

		if (sscanf (req, " { %c , %u } %n, %n",
		            &ch, &dur, &len, &len_wc) >= 2)
		{
			if (dur != 0)
				return AT_CME_ENOTSUP;
		}
		else if (sscanf (req, " %c %n, %n", &ch, &len, &len_wc) < 1)
			return AT_CME_EINVAL;

		if (len == -1)
			return AT_CME_EINVAL;

		if ((ch >= '0' && ch <= '9') ||
		    (ch >= 'A' && ch <= 'D') ||
		    ch == '#' || ch == '*')
		{
			*(tones++) = ch;
			if (tones >= (buf + sizeof (buf)))
				return AT_ERROR;
		} else
			return AT_CME_EINVAL;

		if (len_wc != -1)
		{
			req += len_wc;
			if (!*req)
				return AT_CME_EINVAL;
		}
		else
		{
			req += len;
			if (*req)
				return AT_CME_EINVAL;
		}
	}
	*tones = '\0';
	tones = buf;

	(void)m;
	return modem_request (p, "VoiceCallManager", "SendTones",
	                      DBUS_TYPE_STRING, &tones,
	                      DBUS_TYPE_INVALID);
}

static at_error_t get_vts (at_modem_t *m, void *data)
{
	(void)m;
	(void)data;

	return AT_CME_EINVAL;
}

static at_error_t list_vts (at_modem_t *modem, void *data)
{
	(void)data;

	at_intermediate (modem, "\r\n+VTS: (0-9,#,*,A-D),(),(0)");
	return AT_OK;
}

static at_error_t handle_vts (at_modem_t *modem, const char *req, void *data)
{
	return at_setting (modem, req, data, set_vts, get_vts, list_vts);
}

/*** Registration ***/

void voicecallmanager_register (at_commands_t *set, plugin_t *p)
{
	at_register (set, "+CLCC", handle_clcc, p);
	at_register_dial (set, true, handle_dial, p);
	at_register (set, "+CHUP", handle_chup, p);
	at_register_alpha (set, 'H', handle_hangup, p);
	at_register (set, "+CMOD", handle_cmod, p);
	at_register (set, "+CVMOD", handle_cvmod, p);
	at_register (set, "+CHLD", handle_chld, p);
	p->vhu = 0;
	at_register (set, "+CVHU", handle_cvhu, &p->vhu);
	at_register (set, "+VTS", handle_vts, p);
}

void voicecallmanager_unregister (plugin_t *p)
{
	if (p->vhu == 2)
		handle_chup (NULL, "+CHUP", p);
}
