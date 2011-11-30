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
#include <stdlib.h>
#include <string.h>

#include <at_command.h>
#include <at_thread.h>
#include <at_log.h>
#include <dbus/dbus.h>
#include <assert.h>

#include "ofono.h"
#include "core.h"


static DBusMessage *get_calls (plugin_t *p, at_error_t *ret,
                               DBusMessageIter *calls)
{
	DBusMessage *msg = modem_req_new (p, "VoiceCallManager", "GetCalls");
	if (msg == NULL)
	{
		*ret = AT_CME_ENOMEM;
		return NULL;
	}

	msg = ofono_query (msg, ret);
	if (msg == NULL)
		return NULL;

	DBusMessageIter args;

	if (!dbus_message_iter_init (msg, &args)
	 || dbus_message_iter_get_arg_type (&args) != DBUS_TYPE_ARRAY
	 || dbus_message_iter_get_element_type (&args) != DBUS_TYPE_STRUCT)
	{
		dbus_message_unref (msg);
		*ret = AT_CME_ERROR_0;
		return NULL;
	}

	dbus_message_iter_recurse (&args, calls);
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

static int find_call_by_state (plugin_t *p, const char *state, at_error_t *err)
{
	int id = -1;
	int canc = at_cancel_disable ();
	DBusMessageIter calls;

	DBusMessage *msg = get_calls (p, err, &calls);
	if (msg == NULL)
		goto out;

	for (;
		dbus_message_iter_get_arg_type (&calls) != DBUS_TYPE_INVALID;
		dbus_message_iter_next (&calls))
	{
		const char *callpath, *callstate;
		DBusMessageIter call;

		dbus_message_iter_recurse (&calls, &call);
		if (dbus_message_iter_get_arg_type (&call) != DBUS_TYPE_OBJECT_PATH)
			continue;
		dbus_message_iter_get_basic (&call, &callpath);

		dbus_message_iter_next (&call);
		callstate = ofono_dict_find_string (&call, "State");
		if (callstate != NULL && !strcmp (state, callstate))
		{
			id = get_call_id (callpath);
			break;
		}
	}
	dbus_message_unref (msg);

	assert (*err == AT_OK);
out:
	at_cancel_enable (canc);
	return id;
}


/*** ATA ***/

/* NOTE: It is assumed incoming data calls are not supported */
static at_error_t handle_answer (at_modem_t *modem, unsigned val, void *data)
{
	plugin_t *p = data;

	int id = find_call_by_state (p, "incoming", &(at_error_t){ 0 });
	if (id == -1)
		return AT_NO_CARRIER;

	(void) modem;
	(void) val;
	return voicecall_request (p, id, "Answer", DBUS_TYPE_INVALID);
}


/*** ATD ***/

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

/*** AT+CSTA ***/
static at_error_t set_csta (at_modem_t *modem, const char *req, void *data)
{
	unsigned mode;

	if (sscanf (req, " %u", &mode) != 1)
		mode = 145;
	switch (mode)
	{
		case 129:
		case 145:
			break;
		default:
			return AT_CME_ENOTSUP;
	}
	(void) modem; (void) data;
	return AT_OK;
}

static at_error_t get_csta (at_modem_t *modem, void *data)
{
	(void) data;
	return at_intermediate (modem, "\r\n+CSTA: 145");
}

static at_error_t list_csta (at_modem_t *modem, void *data)
{
	(void) data;
	return at_intermediate (modem, "\r\n+CSTA: (129,145)");
}


/*** RING ***/
static void incoming_call (plugin_t *p, DBusMessageIter *call, at_modem_t *m)
{
	if (p->cring)
		at_unsolicited (m, "\r\n+CRING: VOICE\r\n");
	else
		at_ring (m);

	if (p->clip)
	{
		const char *str = ofono_dict_find_string (call, "LineIdentification");
		if (str == NULL || !strcmp (str, "withheld"))
			at_unsolicited (m, "\r\n+CLIP: \"\",128\r\n");
		else
			at_unsolicited (m, "\r\n+CLIP: \"%s\",%u\r\n", str,
			                (str[0] == '+') ? 145 : 129);
	}

	if (p->cnap)
	{
		const char *str = ofono_dict_find_string (call, "Name");
		if (str == NULL)
			at_unsolicited (m, "\r\n+CNAP: \"\",2\r\n");
		else if (!strcmp (str, "withheld"))
			at_unsolicited (m, "\r\n+CNAP: \"\",1\r\n");
		else
			at_unsolicited (m, "\r\n+CNAP: \"%.80s\"\r\n", str);
	}

	if (p->cdip)
	{
		const char *str = ofono_dict_find_string (call, "IncomingLine");
		if (str != NULL)
			at_unsolicited (m, "\r\n+CDIP: \"%s\",%u\r\n", str,
			                (str[0] == '+') ? 145 : 129);
	}
}

static void waiting_call (plugin_t *p, DBusMessageIter *call, at_modem_t *m)
{
	if (p->ccwa)
	{
		const char *str = ofono_dict_find_string (call, "LineIdentification");
		if (str == NULL || !strcmp (str, "withheld"))
			at_unsolicited (m, "\r\n+CCWA: \"\",128\r\n");
		else
			at_unsolicited (m, "\r\n+CCWA: \"%s\",%u\r\n", str,
			                (str[0] == '+') ? 145 : 129);
	}
}

static void ring_callback (plugin_t *p, DBusMessage *msg, void *data)
{
	at_modem_t *m = data;
	DBusMessageIter call;

	/* Skip call object path */
	if (!dbus_message_iter_init (msg, &call)
	 || dbus_message_iter_get_arg_type (&call) != DBUS_TYPE_OBJECT_PATH)
		return;
	dbus_message_iter_next (&call);

	/* Only care about incoming or waiting calls */
	const char *str = ofono_dict_find_string (&call, "State");
	if (str == NULL)
		return;
	if (!strcmp (str, "incoming"))
		incoming_call (p, &call, m);
	if (!strcmp (str, "waiting"))
		waiting_call (p, &call, m);
}


/** AT+CRC (+CRING) */
static at_error_t set_ring (at_modem_t *modem, const char *req, void *data)
{
	plugin_t *p = data;
	unsigned mode;

	if (sscanf (req, " %u", &mode) != 1)
		return AT_CME_EINVAL;
	if (mode > 1)
		return AT_CME_ENOTSUP;

	p->cring = mode;
	(void) modem;
	return AT_OK;
}

static at_error_t get_ring (at_modem_t *modem, void *data)
{
	plugin_t *p = data;

	at_intermediate (modem, "\r\n+CRC: %u", p->cring);
	return AT_OK;
}

static at_error_t list_ring (at_modem_t *modem, void *data)
{
	at_intermediate (modem, "\r\n+CRC: (0-1)");
	(void) data;
	return AT_OK;
}


/*** AT+CLCC ***/

static at_error_t handle_clcc (at_modem_t *modem, const char *req, void *data)
{
	if (*req)
		return AT_CME_EINVAL;

	plugin_t *p = data;
	at_error_t ret;
	int canc = at_cancel_disable ();
	DBusMessageIter array;

	DBusMessage *msg = get_calls (p, &ret, &array);
	if (msg == NULL)
		goto out;

	static const char states[][9] = {
		"active", "held", "dialing", "alerting", "incoming", "waiting",
	};

	for (;
		dbus_message_iter_get_arg_type (&array) != DBUS_TYPE_INVALID;
		dbus_message_iter_next (&array))
	{
		const char *path, *dir, *number, *state;
		unsigned id;
		int stat = -1, mpty;

		DBusMessageIter call;
		dbus_message_iter_recurse (&array, &call);
		if (dbus_message_iter_get_arg_type (&call) != DBUS_TYPE_OBJECT_PATH)
			continue;
		dbus_message_iter_get_basic (&call, &path);
		id = get_call_id (path);

		dbus_message_iter_next (&call);
		number = ofono_dict_find_string (&call, "LineIdentification");
		dir = ofono_dict_find_string (&call, "Direction");
		mpty = ofono_dict_find_bool (&call, "Multiparty");
		state = ofono_dict_find_string (&call, "State");
		if (dir == NULL || mpty == -1 || state == NULL)
		{
			ret = AT_CME_UNKNOWN;
			goto out;
		}

		for (size_t i = 0; i < sizeof (states) / sizeof (*states); i++)
			if (!strcmp (state, states[i]))
			{
				stat = 0;
				break;
			}

		if (stat == -1)
		{
			error ("Unknown call state \"%s\"", state);
			ret = AT_CME_UNKNOWN;
			goto out;
		}

		if (number != NULL && strcmp (number, "withheld"))
			at_intermediate (modem, "\r\n+CLCC: %u,%u,%d,0,%u,\"%s\",%u", id,
			                 !strcmp (dir, "mt"), stat, mpty, number,
			                 (number[0] == '+') ? 145 : 129);
		else
			at_intermediate (modem, "\r\n+CLCC: %u,%u,%u,0,%u", id,
			                 !strcmp (dir, "mt"), stat, mpty);
	}

	ret = AT_OK;
out:
	at_cancel_enable (canc);
	return ret;
}


/*** AT+CHUP  ***/

static at_error_t set_chup (at_modem_t *modem, const char *req, void *data)
{
	if (*req)
		return AT_CME_EINVAL;

	plugin_t *p = data;
	at_error_t ret;
	int id;

	while ((id = find_call_by_state (p, "active", &ret)) != -1)
		voicecall_request (p, id, "Hangup", DBUS_TYPE_INVALID);
	(void) modem;
	return AT_OK;
}


/*** ATH ***/

static at_error_t handle_hangup (at_modem_t *modem, unsigned val, void *data)
{
	plugin_t *p = data;

	at_error_t ret = modem_request (p, "VoiceCallManager", "HangupAll",
	                     DBUS_TYPE_INVALID);
	if (ret == AT_CME_ERROR_0)
		ret = AT_OK; /* there was no ongoing call */
	(void) modem; /* maybe NULL with AT+CVHU=2 */
	(void) val;
	return ret;
}


/*** AT+CHLD ***/

static at_error_t set_chld (at_modem_t *modem, const char *value, void *data)
{
	plugin_t *p = data;

	(void)modem;

	if (!*value)
		return AT_CME_EINVAL;

	char op = *(value++);
	char *end;
	unsigned id = strtoul (value, &end, 10);

	if (end == value)
	{
		const char *method;

		switch (op)
		{
			case '0':
			{
				/* XXX: race-prone. Use a single call list */
				at_error_t ret;
				int id = find_call_by_state (p, "waiting", &ret);
				if (id != -1)
					return voicecall_request (p, id, "Hangup",
					                         DBUS_TYPE_INVALID);

				while ((id = find_call_by_state (p, "held", &ret)) != -1)
					voicecall_request (p, id, "Hangup", DBUS_TYPE_INVALID);
				return AT_OK;
			}

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
		return modem_request (p, "VoiceCallManager", method,
		                      DBUS_TYPE_INVALID);
	}
	else
	{
		switch (op)
		{
			case '1':
				return voicecall_request (p, id, "Hangup", DBUS_TYPE_INVALID);

			case '2':
			{
				char *call;
				at_error_t ret;

				if (asprintf (&call, "%s/voicecall%u", p->modemv[p->modem],
				              id) == -1)
					return AT_CME_ENOMEM;

				ret = modem_request (p, "VoiceCallManager", "PrivateChat",
				                     DBUS_TYPE_OBJECT_PATH, &call,
				                     DBUS_TYPE_INVALID);
				free (call);
				return ret;
			}
		}
	}
	return AT_CME_ENOTSUP;
}

static at_error_t list_chld (at_modem_t *modem, void *data)
{
	at_intermediate (modem, "\r\n+CHLD: (0,1,1x,2,2x,3,4)");

	(void)data;
	return AT_OK;
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

	at_intermediate (modem, "\r\n+CVHU: (0-2)");
	return AT_OK;
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
		else if (sscanf (req, " [ %*u , %*u , %u ] %n, %n",
		                 &dur, &len, &len_wc) >= 1)
			return AT_CME_ENOTSUP;
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

static at_error_t list_vts (at_modem_t *modem, void *data)
{
	(void)data;

	at_intermediate (modem, "\r\n+VTS: (0-9,#,*,A-D),(),(0)");
	return AT_OK;
}


/*** AT+VTD ***/

static at_error_t set_vtd (at_modem_t *m, const char *req, void *data)
{
	(void)data;
	(void)m;

	unsigned dura;
	if (sscanf (req, " %u", &dura) < 1)
		return AT_CME_EINVAL;
	if (dura != 0)
		return AT_CME_ENOTSUP;

	return AT_OK;
}

static at_error_t get_vtd (at_modem_t *m, void *data)
{
	(void)data;

	at_intermediate (m, "\r\n+VTD: 0");

	return AT_OK;
}

static at_error_t list_vtd (at_modem_t *modem, void *data)
{
	(void)data;

	at_intermediate (modem, "\r\n+VTD: (0)");
	return AT_OK;
}


/*** AT+CTFR ***/

static at_error_t do_ctfr (at_modem_t *modem, const char *req, void *data)
{
	plugin_t *p = data;
	char number[21], subaddr[24];
	unsigned type, satype;

	switch (sscanf (req, " \"%20[^\"]\" , %u , \"%23[^\"]\" , %u",
	                number, &type, subaddr, &satype))
	{
		case 4:
		case 3:
			return AT_CME_ENOTSUP;
		case 2:
			if (type != (number[0] == '+') ? 145 : 129)
				return AT_CME_ENOTSUP;
		case 1:
			break;
		default:
			return AT_CME_EINVAL;
	}

	int id = find_call_by_state (p, "incoming", &(at_error_t){ 0 });
	if (id == -1)
		return AT_CME_ENOENT;

	(void) modem;
	return voicecall_request (p, id, "Deflect",
	                          DBUS_TYPE_STRING, &(const char *){ number },
	                          DBUS_TYPE_INVALID);
}


/*** AT+CPAS ***/

static at_error_t show_cpas (at_modem_t *modem, const char *req, void *data)
{
	plugin_t *p = data;
	unsigned pas = 2; /* unknown */
	at_error_t dummy;

	if (*req)
		return AT_CME_ENOTSUP;

	int canc = at_cancel_disable ();

	DBusMessageIter calls;
	DBusMessage *msg = get_calls (p, &dummy, &calls);
	if (msg == NULL)
	{
		if (modem_prop_get_bool (p, "Modem", "Powered") == 0)
			pas = 5; /* asleep */
	}
	else
	{
		pas = 0; /* ready */

		for (;
			dbus_message_iter_get_arg_type (&calls) != DBUS_TYPE_INVALID;
			dbus_message_iter_next (&calls))
		{
			const char *state;
			DBusMessageIter call;

			dbus_message_iter_recurse (&calls, &call);
			dbus_message_iter_next (&call);
			state = ofono_dict_find_string (&call, "State");
			if (state == NULL)
				continue;
			if (!strcmp (state, "ringing"))
			{
				pas = 3; /* ringing */
				break;
			}
			if (!strcmp (state, "active") || !strcmp (state, "alerting"))
			{
				pas = 4; /* call in progress */
				break;
			}
		}
		dbus_message_unref (msg);
	}
	at_cancel_enable (canc);

	at_intermediate (modem, "\r\n+CPAS: %u", pas);
	return AT_OK;
}

static at_error_t list_cpas (at_modem_t *modem, void *data)
{
	(void) data;
	at_intermediate (modem, "\r\n+CPAS: (0-5)");
	return AT_OK;
}


/*** Emergency Numbers */
static at_error_t read_en (at_modem_t *modem, unsigned start, unsigned end,
                           void *data)
{
	plugin_t *p = data;
	at_error_t ret = AT_OK;

	int canc = at_cancel_disable ();
	DBusMessage *props = modem_props_get (p, "VoiceCallManager");
	if (props == NULL)
	{
		ret = AT_CME_ERROR_0;
		goto out;
	}

	DBusMessageIter it;
	unsigned idx = 0;
	if (ofono_prop_find_array (props, "EmergencyNumbers", &it))
	{
		dbus_message_unref (props);
		ret = AT_CME_UNKNOWN;
		goto out;
	}

	while (dbus_message_iter_get_arg_type (&it) == DBUS_TYPE_STRING)
	{
		const char *en;

		dbus_message_iter_get_basic (&it, &en);
		if (idx >= start && idx <= end)
			at_intermediate (modem, "\r\n+CPBR: %u,\"%s\",129,\"\"", idx, en);
		idx++;
		dbus_message_iter_next (&it);
	}
	dbus_message_unref (props);
out:
	at_cancel_enable (canc);
	return ret;
}


static at_error_t count_en (unsigned *a, unsigned *b, void *data)
{
	plugin_t *p = data;
	at_error_t ret = AT_OK;

	int canc = at_cancel_disable ();
	DBusMessage *props = modem_props_get (p, "VoiceCallManager");
	if (props == NULL)
	{
		ret = AT_CME_ERROR_0;
		goto out;
	}

	DBusMessageIter it;
	unsigned idx = 0;
	if (ofono_prop_find_array (props, "EmergencyNumbers", &it))
	{
		dbus_message_unref (props);
		ret = AT_CME_UNKNOWN;
		goto out;
	}

	while (dbus_message_iter_get_arg_type (&it) == DBUS_TYPE_STRING)
	{
		idx++;
		dbus_message_iter_next (&it);
	}
	dbus_message_unref (props);

	*a = 0;
	if (idx == 0)
		ret = AT_CME_ENOENT;
	else
		*b = idx - 1;
out:
	at_cancel_enable (canc);
	return ret;
}

/*** Registration ***/

void voicecallmanager_register (at_commands_t *set, plugin_t *p)
{
	at_register_alpha (set, 'A', handle_answer, p);
	at_register_dial (set, true, handle_dial, p);
	at_register_ext (set, "+CSTA", set_csta, get_csta, list_csta, p);
	at_register_ext (set, "+CLCC", handle_clcc, NULL, NULL, p);
	at_register_ext (set, "+CHUP", set_chup, NULL, NULL, p);
	at_register_alpha (set, 'H', handle_hangup, p);
	at_register_ext (set, "+CHLD", set_chld, NULL, list_chld, p);
	p->cring = false;
	at_register_ext (set, "+CRC", set_ring, get_ring, list_ring, p);
	p->vhu = 0;
	at_register_ext (set, "+CVHU", set_cvhu, get_cvhu, list_cvhu, &p->vhu);
	at_register_ext (set, "+VTS", set_vts, NULL, list_vts, p);
	at_register_ext (set, "+VTD", set_vtd, get_vtd, list_vtd, p);
	at_register_ext (set, "+CTFR", do_ctfr, NULL, NULL, p);
	at_register_ext (set, "+CPAS", show_cpas, NULL, list_cpas, p);
	at_register_pb (set, "EN", NULL, read_en, NULL, NULL, count_en, p);

	p->ring_filter = ofono_signal_watch (p, NULL, "VoiceCallManager",
	                                     "CallAdded", NULL, ring_callback,
	                                     AT_COMMANDS_MODEM(set));
}

void voicecallmanager_unregister (plugin_t *p)
{
	ofono_signal_unwatch (p->ring_filter);
	if (p->vhu == 2)
		handle_hangup (NULL, 'H', p);
}
